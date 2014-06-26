/* drivers/input/touchscreen/cy8c_sar.c
 *
 * Copyright (C) 2011 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/input/cy8c_sar.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/irq.h>
#include <linux/pm.h>

BLOCKING_NOTIFIER_HEAD(sar_notifier_list);

/**
 * register_notifier_by_sar - Register function for Wifi core(s) to decrease/recover the transmission power
 * @nb: Hook to be registered
 *
 * The register_notifier_by_sar is used to notify the Wifi core(s) about the SAR sensor state changes
 * Returns zero always as notifier_chain_register.
 */
int register_notifier_by_sar(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&sar_notifier_list, nb);
}

/**
 * unregister_notifier_by_sar - Unregister previously registered SAR notifier
 * @nb: Hook to be unregistered
 *
 * The unregister_notifier_by_sar unregisters previously registered SAR notifier
 * Returns zero always as notifier_chain_register.
 */
int unregister_notifier_by_sar(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&sar_notifier_list, nb);
}

static int i2c_cy8c_read(struct i2c_client *client, uint8_t addr, uint8_t *data, uint8_t length)
{
	int retry, ret;

	for (retry = 0; retry < CY8C_I2C_RETRY_TIMES; retry++) {
		ret = i2c_smbus_read_i2c_block_data(client, addr, length, data);
		if (ret > 0)
			return ret;
		mdelay(10);
	}

	pr_info("[SAR] i2c_read_block retry over %d\n", CY8C_I2C_RETRY_TIMES);
	return -EIO;
}

static int i2c_cy8c_write(struct i2c_client *client, uint8_t addr, uint8_t *data, uint8_t length)
{
	int retry, ret;

	for (retry = 0; retry < CY8C_I2C_RETRY_TIMES; retry++) {
		ret = i2c_smbus_write_i2c_block_data(client, addr, length, data);
		if (ret == 0)
			return ret;
		mdelay(10);
	}

	pr_info("[SAR] i2c_write_block retry over %d\n", CY8C_I2C_RETRY_TIMES);
	return -EIO;

}

static int i2c_cy8c_write_byte_data(struct i2c_client *client, uint8_t addr, uint8_t value)
{
	return i2c_cy8c_write(client, addr, &value, 1);
}

static ssize_t reset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cy8c_sar_data *sar = dev_get_drvdata(dev);

	pr_debug("[SAR] reset\n");
	sar->reset();
	sar->sleep_mode = KEEP_AWAKE;
	return scnprintf(buf, PAGE_SIZE, "Reset chip");
}
static DEVICE_ATTR(reset, S_IRUGO, reset, NULL);

static ssize_t sar_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char data[2] = {0};
	int ret;
	struct cy8c_sar_data *sar = dev_get_drvdata(dev);

	ret = i2c_cy8c_read(sar->client, CS_FW_VERSION, data, sizeof(data));
	if (ret < 0) {
		pr_err("[SAR] i2c Read version Err\n");
		return ret;
	}

	return scnprintf(buf, PAGE_SIZE, "%s_V%x", CYPRESS_SAR_NAME, data[0]);
}
static DEVICE_ATTR(vendor, S_IRUGO, sar_vendor_show, NULL);

static ssize_t cy8c_sar_gpio_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct cy8c_sar_data *sar = dev_get_drvdata(dev);
	struct cy8c_i2c_sar_platform_data *pdata;

	pdata = sar->client->dev.platform_data;

	ret = gpio_get_value(pdata->gpio_irq);
	pr_debug("[SAR] %d", pdata->gpio_irq);
	return scnprintf(buf, PAGE_SIZE, "%d", ret);
}
static DEVICE_ATTR(gpio, S_IRUGO, cy8c_sar_gpio_show, NULL);

static ssize_t sleep_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int data;
	struct cy8c_sar_data *sar = dev_get_drvdata(dev);

	sscanf(buf, "%x", &data);
	pr_debug("[SAR] %s +++", __func__);

	if (data != KEEP_AWAKE && data != DEEP_SLEEP)
		return count;
	cancel_delayed_work_sync(&sar->sleep_work);

	pr_debug("[SAR] %s: current mode = %d, new mode = %d\n", __func__, sar->sleep_mode, data);
	spin_lock_irqsave(&sar->spin_lock, sar->spinlock_flags);
	sar->radio_state = data;
	if (sar->radio_state == KEEP_AWAKE)
		queue_delayed_work(sar->cy8c_wq, &sar->sleep_work, WAKEUP_DELAY);
	else
		queue_delayed_work(sar->cy8c_wq, &sar->sleep_work, 0);

	spin_unlock_irqrestore(&sar->spin_lock, sar->spinlock_flags);
	pr_debug("[SAR] %s ---", __func__);
	return count;
}

static ssize_t sleep_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cy8c_sar_data *sar = dev_get_drvdata(dev);
	pr_debug("[SAR] %s: sleep_mode = %d\n", __func__, sar->sleep_mode);
	return scnprintf(buf, PAGE_SIZE, "%d", sar->sleep_mode);
}
static DEVICE_ATTR(sleep, (S_IWUSR|S_IRUGO), sleep_show, sleep_store);

static struct attribute *sar_attrs[] = {
	&dev_attr_reset.attr,
	&dev_attr_vendor.attr,
	&dev_attr_gpio.attr,
	&dev_attr_sleep.attr,
	NULL,
};

static struct attribute_group sar_attr_group = {
	.attrs = sar_attrs,
};

static int cy8c_init_sensor(struct cy8c_sar_data *sar, struct cy8c_i2c_sar_platform_data *pdata)
{
	uint8_t ver = 0, chip = 0;
	int ret;
	pr_info("[SAR] %s\n", __func__);

	ret = i2c_cy8c_read(sar->client, CS_FW_CHIPID, &chip, 1);
	if (ret < 0) {
		pr_err("[SAR][ERR] Chip Read Err\n");
		goto err_fw_get_fail;
	}

	ret = i2c_cy8c_read(sar->client, CS_FW_VERSION, &ver, 1);
	if (ret < 0) {
		pr_err("[SAR][ERR] Ver Read Err\n");
		goto err_fw_get_fail;
	} else
		sar->id.version = ver;

	if (chip == CS_CHIPID) {
		sar->id.chipid = chip;
		pr_info("[SAR] CY8C_SAR_V%x\n", sar->id.version);
	} else
		pr_info("[SAR] CY8C_Cap_V%x\n", sar->id.version);
	return 0;

err_fw_get_fail:
	return ret;
}

static int sar_update_mode(int radio, int pm)
{
	pr_debug("[SAR] %s: radio=%d, pm=%d\n", __func__, radio, pm);
	if (radio == KEEP_AWAKE && pm == KEEP_AWAKE)
		return KEEP_AWAKE;
	else
		return DEEP_SLEEP;
}

static void sar_sleep_func(struct work_struct *work)
{
	struct cy8c_sar_data *sar = container_of(work, struct cy8c_sar_data, sleep_work.work);
	int mode, err;
	pr_debug("[SAR] %s\n", __func__);

	mode = sar_update_mode(sar->radio_state, sar->pm_state);
	if (mode == sar->sleep_mode) {
		pr_debug("[SAR] sleep mode no change.\n");
		return;
	}
	switch (mode) {
	case KEEP_AWAKE:
		sar->reset();
		mdelay(50);
		queue_work(sar->cy8c_wq, &sar->work);
		break;
	case DEEP_SLEEP:
		disable_irq(sar->intr_irq);
		if (cancel_work_sync(&sar->work))
			enable_irq(sar->intr_irq);
		err = i2c_cy8c_write_byte_data(sar->client, CS_MODE, CS_CMD_DSLEEP);
		if (err < 0) {
			pr_err("[SAR] %s: I2C write fail. reg %d, err %d\n", __func__, CS_CMD_DSLEEP, err);
			return;
		}
		break;
	default:
		pr_info("[SAR] Unknown sleep mode\n");
		return;
	}
	sar->sleep_mode = mode;
	pr_debug("[SAR] Set SAR sleep mode = %d\n", sar->sleep_mode);
}

static int sysfs_create(struct cy8c_sar_data *sar)
{
	int ret;

	pr_info("[SAR] %s, position=%d\n", __func__, sar->position_id);
	if (sar->position_id == 0) {
		sar->sar_class = class_create(THIS_MODULE, "cap_sense");
		if (IS_ERR(sar->sar_class)) {
			pr_info("[SAR] %s, position=%d, create class fail1\n", __func__, sar->position_id);
			ret = PTR_ERR(sar->sar_class);
			sar->sar_class = NULL;
			return -ENXIO;
		}
		sar->sar_dev = device_create(sar->sar_class, NULL, 0, sar, "sar");
		if (unlikely(IS_ERR(sar->sar_dev))) {
			pr_info("[SAR] %s, position=%d, create class fail2\n", __func__, sar->position_id);
			ret = PTR_ERR(sar->sar_dev);
			sar->sar_dev = NULL;
			return -ENOMEM;
		}
	} else if (sar->position_id == 1) {
		sar->sar_class = class_create(THIS_MODULE, "cap_sense1");
		if (IS_ERR(sar->sar_class)) {
			pr_info("[SAR] %s, position=%d, create class fail3\n", __func__, sar->position_id);
			ret = PTR_ERR(sar->sar_class);
			sar->sar_class = NULL;
			return -ENXIO;
		}
		sar->sar_dev = device_create(sar->sar_class, NULL, 0, sar, "sar1");
		if (unlikely(IS_ERR(sar->sar_dev))) {
			pr_info("[SAR] %s, position=%d, create class fail4\n", __func__, sar->position_id);
			ret = PTR_ERR(sar->sar_dev);
			sar->sar_dev = NULL;
			return -ENOMEM;
		}
	}

	ret = sysfs_create_group(&sar->sar_dev->kobj, &sar_attr_group);
	if (ret) {
		dev_err(sar->sar_dev, "Unable to create sar attr, error: %d\n", ret);
		return -EEXIST;
	}

	return 0;
}

static void cy8c_sar_work_func(struct work_struct *work)
{
	struct cy8c_sar_data *sar;
	uint8_t buf[1] = {0};
	int state, active, ret;

	pr_debug("[SAR] %s: enter\n", __func__);
	sar = container_of(work, struct cy8c_sar_data, work);

	ret = i2c_cy8c_read(sar->client, CS_STATUS, buf, 1);
	state = gpio_get_value(sar->intr);
	active = (state ^ !sar->polarity) ? 1 : 0;
	pr_debug("[SAR] CS_STATUS:0x%x\n", buf[0]);
	pr_debug("[SAR] %s active=%d, is_actived=%d\n", __func__, active, sar->is_actived);
	/*irq_set_irq_type(sar->intr_irq,*/
	/*	state ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);*/

	if (active != sar->is_actived && buf[0] != 0) {
		pr_info("[SAR] reduce Wifi power\n");
		sar->is_actived = active;

		if (sar->position_id == 0)
			active = SAR_ACT;
		else
			active = SAR1_ACT;
	} else {
		pr_info("[SAR] restore Wifi power\n");
		sar->is_actived = 0;

		if (sar->position_id == 0)
			active = SAR_NON_ACT;
		else
			active = SAR1_NON_ACT;
	}
	blocking_notifier_call_chain(&sar_notifier_list, active, NULL);

	enable_irq(sar->client->irq);
}

static irqreturn_t cy8c_sar_irq_handler(int irq, void *dev_id)
{
	struct cy8c_sar_data *sar = dev_id;

	disable_irq_nosync(sar->client->irq);
	queue_work(sar->cy8c_wq, &sar->work);
	return IRQ_HANDLED;
}

static int cy8c_sar_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct cy8c_sar_data *sar;
	struct cy8c_i2c_sar_platform_data *pdata;
	int ret;

	pr_debug("[SAR] %s: enter\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SAR][ERR] need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	sar = kzalloc(sizeof(struct cy8c_sar_data), GFP_KERNEL);
	if (sar == NULL) {
		pr_err("[SAR][ERR] allocate cy8c_sar_data failed\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	sar->client = client;
	i2c_set_clientdata(client, sar);
	pdata = client->dev.platform_data;

	if (pdata) {
		pdata->reset();
		msleep(50);
		sar->intr = pdata->gpio_irq;
	}

	ret = cy8c_init_sensor(sar, pdata);
	if (ret < 0) {
		pr_err("[SAR][ERR] init failure, not probe up driver\n");
		goto err_init_sensor_failed;
	}
	sar->sleep_mode = sar->radio_state = sar->pm_state = KEEP_AWAKE;
	sar->is_actived = 0;
	sar->polarity = 0; /* 0: low active */

	if (pdata) {
		if (pdata->id.config != sar->id.config) {
			pr_info("[SAR] pdata ++\n");
			pdata++;
		}
	}

	sar->cy8c_wq = create_singlethread_workqueue("cypress_sar");
	if (!sar->cy8c_wq) {
		ret = -ENOMEM;
		pr_err("[SAR][ERR] create_singlethread_workqueue cy8c_wq fail\n");
		goto err_create_wq_failed;
	}
	INIT_WORK(&sar->work, cy8c_sar_work_func);
	INIT_DELAYED_WORK(&sar->sleep_work, sar_sleep_func);

	sar->reset                 = pdata->reset;
	sar->intr_irq              = gpio_to_irq(sar->intr);
	sar->position_id		   = pdata->position_id;

	ret = sysfs_create(sar);
	if (ret == -EEXIST)
		goto err_create_device_file;
	else if (ret == -ENOMEM)
		goto err_create_device;
	else if (ret == -ENXIO)
		goto err_create_class;

	sar->use_irq = 1;
	if (client->irq && sar->use_irq) {
		ret = request_irq(sar->intr_irq, cy8c_sar_irq_handler,
				  IRQF_TRIGGER_FALLING,
				  CYPRESS_SAR_NAME, sar);
		if (ret < 0) {
			dev_err(&client->dev, "[SAR][ERR] request_irq failed\n");
			pr_err("[SAR][ERR] request_irq failed for gpio %d, irq %d\n",
				sar->intr, client->irq);
			goto err_request_irq;
		}
	}
	return 0;

err_request_irq:
	sysfs_remove_group(&sar->sar_dev->kobj, &sar_attr_group);
err_create_device_file:
	device_unregister(sar->sar_dev);
err_create_device:
	class_destroy(sar->sar_class);
err_create_class:
	destroy_workqueue(sar->cy8c_wq);
err_init_sensor_failed:
err_create_wq_failed:
	kfree(sar);

err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int cy8c_sar_remove(struct i2c_client *client)
{
	struct cy8c_sar_data *sar = i2c_get_clientdata(client);

	disable_irq(client->irq);
	sysfs_remove_group(&sar->sar_dev->kobj, &sar_attr_group);
	device_unregister(sar->sar_dev);
	class_destroy(sar->sar_class);
	destroy_workqueue(sar->cy8c_wq);
	free_irq(client->irq, sar);

	kfree(sar);

	return 0;
}

static int cy8c_sar_suspend(struct device *dev)
{
	struct cy8c_sar_data *sar = dev_get_drvdata(dev);

	pr_debug("[SAR] %s\n", __func__);
	cancel_delayed_work_sync(&sar->sleep_work);
	sar->pm_state = DEEP_SLEEP;
	queue_delayed_work(sar->cy8c_wq, &sar->sleep_work, 0);
	flush_delayed_work(&sar->sleep_work);
	return 0;
}

static int cy8c_sar_resume(struct device *dev)
{
	struct cy8c_sar_data *sar = dev_get_drvdata(dev);

	pr_debug("[SAR] %s\n", __func__);

	cancel_delayed_work_sync(&sar->sleep_work);
	sar->pm_state = KEEP_AWAKE;
	queue_delayed_work(sar->cy8c_wq, &sar->sleep_work, WAKEUP_DELAY*2);
	return 0;
}

static const struct i2c_device_id cy8c_sar_id[] = {
	{ CYPRESS_SAR_NAME, 0 },
	{ CYPRESS_SAR1_NAME, 0 },
};

static const struct dev_pm_ops pm_ops = {
	.suspend = cy8c_sar_suspend,
	.resume = cy8c_sar_resume,
};

static struct i2c_driver cy8c_sar_driver = {
	.probe		= cy8c_sar_probe,
	.remove		= cy8c_sar_remove,
	.id_table	= cy8c_sar_id,
	.driver		= {
		.name	= CYPRESS_SAR_NAME,
		.owner	= THIS_MODULE,
		.pm		= &pm_ops,
	},
};

static int __init cy8c_sar_init(void)
{
	pr_debug("[SAR] %s: enter\n", __func__);
	return i2c_add_driver(&cy8c_sar_driver);
}

static void __exit cy8c_sar_exit(void)
{
	i2c_del_driver(&cy8c_sar_driver);
}

module_init(cy8c_sar_init);
module_exit(cy8c_sar_exit);

MODULE_DESCRIPTION("cy8c_sar driver");
MODULE_LICENSE("GPL");
