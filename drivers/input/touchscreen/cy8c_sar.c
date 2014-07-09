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
#include <linux/firmware.h>
#include <linux/ctype.h>
#define SCNx8 "hhx"

static LIST_HEAD(sar_list);
static DEFINE_SPINLOCK(sar_list_lock);
static void sar_event_handler(struct work_struct *work);
static DECLARE_WORK(notifier_work, sar_event_handler);
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
	int ret = blocking_notifier_chain_register(&sar_notifier_list, nb);
	schedule_work(&notifier_work);
	return ret;
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

	pr_debug("[SAR] i2c_read_block retry over %d\n", CY8C_I2C_RETRY_TIMES);
	return -EIO;
}

static int i2c_cy8c_write(struct i2c_client *client, uint8_t addr, uint8_t *data, uint8_t length)
{
	int retry, ret;

	for (retry = 0; retry < CY8C_I2C_RETRY_TIMES; retry++) {
		ret = i2c_smbus_write_i2c_block_data(client, addr, length, data);
		if (ret == 0)
			return ret;
		msleep(10);
	}

	pr_debug("[SAR] i2c_write_block retry over %d\n", CY8C_I2C_RETRY_TIMES);
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
	struct cy8c_i2c_sar_platform_data *pdata;

	pdata = sar->client->dev.platform_data;
	pr_debug("[SAR] reset\n");
	pdata->reset();
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
	unsigned long spinlock_flags;

	sscanf(buf, "%x", &data);
	pr_debug("[SAR] %s +++", __func__);

	if (data != KEEP_AWAKE && data != DEEP_SLEEP)
		return count;
	cancel_delayed_work_sync(&sar->sleep_work);

	pr_debug("[SAR] %s: current mode = %d, new mode = %d\n", __func__, sar->sleep_mode, data);
	spin_lock_irqsave(&sar->spin_lock, spinlock_flags);
	sar->radio_state = data;
	if (sar->radio_state == KEEP_AWAKE)
		queue_delayed_work(sar->cy8c_wq, &sar->sleep_work, WAKEUP_DELAY);
	else
		queue_delayed_work(sar->cy8c_wq, &sar->sleep_work, 0);

	spin_unlock_irqrestore(&sar->spin_lock, spinlock_flags);
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

static int check_fw_version(char *buf, int len,
		struct cy8c_sar_data *sar, int *tag_len)
{
	uint8_t ver = 0;
	uint8_t readfwver = 0;
	int taglen, ret;

	*tag_len = taglen = strcspn(buf, "\n");
	if (taglen >= 3)
		sscanf(buf + taglen - 3, "%2" SCNx8, &readfwver);
	ret = i2c_cy8c_read(sar->client, CS_FW_VERSION, &ver, 1);
	pr_info("[SAR] from FIRMWARE file = %x, Chip FW ver: %x\n"
			, readfwver, ver);
	if (ver != readfwver) {
		pr_info("[SAR] %s : chip fw version need to update\n"
				, __func__);
		return 1;
	} else {
		pr_debug("[SAR] %s : bypass the fw update process\n"
				, __func__);
		return 0;
	}
}

static int i2c_tx_bytes(struct cy8c_sar_data *sar, u16 len, u8 *buf)
{
	int ret;
	ret = i2c_master_send(sar->client, (char *)buf, (int)len);
	if (ret != len) {
		pr_err("[SAR][ERR] I2C TX fail (%d)", ret);
		ret = -EIO;
	}
	return ret;
}

static int i2c_rx_bytes(struct cy8c_sar_data *sar, u16 len, u8 *buf)
{
	int ret;
	ret = i2c_master_recv(sar->client, (char *)buf, (int)len);
	if (ret != len) {
		pr_err("[SAR][ERR] I2C RX fail (%d)", ret);
		ret = -EIO;
	}
	return ret;
}

static void sar_event_handler(struct work_struct *work)
{
	struct cy8c_sar_data *sar;
	int active = SAR_MISSING;
	unsigned long spinlock_flags;

	pr_debug("[SAR] %s: enter\n", __func__);
	spin_lock_irqsave(&sar_list_lock, spinlock_flags);
	list_for_each_entry(sar, &sar_list, list) {
		struct cy8c_i2c_sar_platform_data *pdata;

		pdata = sar->client->dev.platform_data;
		active &= ~SAR_MISSING;
		if (sar->is_activated) {
			active |= 1 << (pdata->position_id);
		}
		if (sar->dysfunctional)
			active |= SAR_DYSFUNCTIONAL << pdata->position_id;
	}
	spin_unlock_irqrestore(&sar_list_lock, spinlock_flags);
	pr_info("[SAR] active=%x\n", active);
	blocking_notifier_call_chain(&sar_notifier_list, active, NULL);
}

static int smart_blmode_check(struct cy8c_sar_data *sar)
{
	int ret;
	uint8_t i2cbuf[3] = {0};

	ret = i2c_tx_bytes(sar, 2, i2cbuf);
	if (ret != 2) {
		pr_err("[SAR][ERR] Shift i2c_write ERROR!!\n");
		return -1;
	}
	ret = i2c_rx_bytes(sar, 3, i2cbuf);/*Confirm in BL mode*/
	if (ret != 3)
		pr_err("[SAR][ERR] i2c_BL_read ERROR!!\n");

	switch (i2cbuf[BL_CODEADD]) {
	case BL_RETMODE:
	case BL_RETBL:
		return 0;
	case BL_BLIVE:
		return (i2cbuf[BL_STATUSADD] == BL_BLMODE) ? 1 : -1;
	default:
		pr_err("[SAR][ERR] code = %x %x %x\n"
				, i2cbuf[0], i2cbuf[1], i2cbuf[2]);
		return -1;
	}
	return -1;
}

static int update_firmware(char *buf, int len, struct cy8c_sar_data *sar)
{
	int ret, i, j, cnt_blk = 0;
	uint8_t *sarfw, wbuf[18] = {0};
	uint8_t i2cbuf[3] = {0};
	int taglen;
	struct i2c_client *client = sar->client;
	struct cy8c_i2c_sar_platform_data *pdata = client->dev.platform_data;

	pr_info("[SAR] %s, %d\n", __func__, len);
	sarfw = kzalloc(len/2, GFP_KERNEL);

	disable_irq(sar->intr_irq);
	ret = check_fw_version(buf, len, sar, &taglen);
	if (ret) {
		for (i = j = 0; i < len - 1; ++i) {
			char buffer[3];
			memcpy(buffer, buf + taglen + i, 2);
			if (!isxdigit(buffer[0]))
				continue;
			buffer[2] = '\0';
			if (sscanf(buffer, "%2" SCNx8, sarfw + j) == 1) {
				++j;
				++i;
			}
		}
		j = 0;
		msleep(100);/*wait chip ready and set BL command*/
		i2cbuf[0] = CS_FW_BLADD;
		i2cbuf[1] = 1;
		ret = i2c_tx_bytes(sar, 2, i2cbuf);
		if (ret != 2)
			pr_err("[SAR][ERR] i2c_write G2B ERROR!!\n");

		client->addr = pdata->bl_addr;
		pr_info("[SAR] @set TOP BL mode addr:0x%x\n", pdata->bl_addr);
		msleep(100);/*wait chip into BL mode.*/
		memcpy(wbuf+2, sarfw, 10);
		ret = i2c_tx_bytes(sar, 12, wbuf);/*1st Block*/
		if (ret != 12) {
			pr_err("[SAR][ERR] 1st i2c_write ERROR!!\n");
			goto error_fw_fail;
		}

		memset(wbuf, 0, sizeof(wbuf));
		j += 10;

		msleep(100);/*wait chip move buf data to RAM for 1ST FW block*/
		ret = smart_blmode_check(sar);
		if (ret < 0) {
			pr_err("[SAR][ERR] 1st Blk BL Check Err\n");
			goto error_fw_fail;
		}
		pr_info("[SAR] check 1st BL mode Done!!\n");
		while (1) {
			if (sarfw[j+1] == 0x39) {
				cnt_blk++;
				msleep(10);

				for (i = 0; i < 4; i++) {
					wbuf[0] = 0;
					wbuf[1] = 0x10*i;
					memcpy(wbuf+2, sarfw+j, 16);
					ret = i2c_tx_bytes(sar, 18, wbuf);/*Write Blocks*/
					if (ret != 18) {
						pr_err("[SAR][ERR] i2c_write ERROR!! Data Block = %d\n"
								, cnt_blk);
						goto error_fw_fail;
					}

					memset(wbuf, 0, sizeof(wbuf));
					msleep(10);
					j += 16;
				}
				msleep(10);

				wbuf[1] = 0x40;
				memcpy(wbuf+2, sarfw+j, 14);
				ret = i2c_tx_bytes(sar, 16, wbuf);

				if (ret != 16) {
					pr_err("[SAR][ERR] i2c_write ERROR!! Data Block = %d\n"
							, cnt_blk);
					goto error_fw_fail;
				}
				memset(wbuf, 0 , sizeof(wbuf));
				j += 14;

				msleep(150);/*wait chip move buf to RAM for each data block.*/
				ret = smart_blmode_check(sar);/*confirm Bl*/
				if (ret < 0) {
					pr_err("[SAR][ERR] Check BL Error Blk = %d\n", cnt_blk);
					goto error_fw_fail;
				}
			} else if (sarfw[j+1] == 0x3B) {
				msleep(10);
				memcpy(wbuf+2, sarfw+j, 10);
				ret = i2c_tx_bytes(sar, 12, wbuf);
				if (ret != 12) {
					pr_err("[SAR][ERR] i2c_write ERROR!! Last Block\n");
					goto error_fw_fail;
				}
				memset(wbuf, 0, sizeof(wbuf));
				j += 10;

				msleep(200);/*write all block done, wait chip internal reset time.*/
				pr_info("[SAR] Firmware Update OK!\n");
				break;
			} else {
				pr_err("[SAR][ERR] Smart sensor firmware update error!!\n");
				break;
			}
		}

		client->addr = pdata->ap_addr;
		pr_debug("[SAR] Firmware Update OK and set the slave addr to %x\n"
				, pdata->ap_addr);
	}
	kfree(sarfw);
	enable_irq(client->irq);
	return 0;

error_fw_fail:
	kfree(sarfw);
	client->addr = pdata->position_id ? 0xBA >> 1 : 0xB8 >> 1;
	enable_irq(client->irq);
	return -1;
}

static void cy8c_sar_fw_update_func(const struct firmware *fw, void *context)
{
	int error, i;
	struct cy8c_sar_data *sar = (struct cy8c_sar_data *)context;
	struct i2c_client *client = sar->client;
	struct cy8c_i2c_sar_platform_data *pdata = client->dev.platform_data;

	if (!fw) {
		pr_err("[SAR] sar%d_CY8C.img not available\n"
				, pdata->position_id);
		return;
	}
	for (i = 0; i < 3; i++) {
		pdata->reset();
		error = update_firmware((char *)fw->data, fw->size, sar);
		if (error < 0) {
			pr_err("[SAR%d] %s update firmware fails, error = %d\n"
					, pdata->position_id, __func__ , error);
			if (i == 2) {
				pr_err("[SAR%d] fail 3 times to set the SAR always active"
						, pdata->position_id);
				sar->dysfunctional = 1;
				schedule_work(&notifier_work);
			}
		} else {
			sar->dysfunctional = 0;
			break;
		}
	}
}

static int sar_fw_update(struct cy8c_sar_data *sar)
{
	int ret;
	struct i2c_client *client = sar->client;
	struct cy8c_i2c_sar_platform_data *pdata = client->dev.platform_data;

	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG
			, pdata->position_id ? "sar1_CY8C.img" : "sar0_CY8C.img"
			, &client->dev, GFP_KERNEL
			, sar, cy8c_sar_fw_update_func);
	return ret;
}

static int cy8c_init_sensor(struct cy8c_sar_data *sar)
{
	uint8_t ver = 0, chip = 0;
	int ret;
	struct i2c_client *client = sar->client;
	struct cy8c_i2c_sar_platform_data *pdata = client->dev.platform_data;

	pr_info("[SAR] %s\n", __func__);
	ret = i2c_cy8c_read(client, CS_FW_CHIPID, &chip, 1);
	if (ret < 0) {
		client->addr = pdata->bl_addr;
		ret = i2c_cy8c_read(client, CS_FW_CHIPID, &chip, 1);
		if (ret < 0) {
			pr_err("[SAR%d] Chip not found\n", pdata->position_id);
			goto err_chip_found;
		} else {
			pr_err("[SAR][ERR] Chip in the BL mode need to recover\n");
			goto err_fw_get_fail;
		}
	}

	ret = i2c_cy8c_read(client, CS_FW_VERSION, &ver, 1);
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
	pr_info("[SAR%d] Block in BL mode need re-flash FW.\n"
			, pdata->position_id);
	return -2;
err_chip_found:
	return -1;
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
	struct i2c_client *client = sar->client;
	struct cy8c_i2c_sar_platform_data *pdata = client->dev.platform_data;

	pr_debug("[SAR] %s\n", __func__);

	mode = sar_update_mode(sar->radio_state, sar->pm_state);
	if (mode == sar->sleep_mode) {
		pr_debug("[SAR] sleep mode no change.\n");
		return;
	}
	switch (mode) {
	case KEEP_AWAKE:
		pdata->reset();
		enable_irq(sar->intr_irq);
		break;
	case DEEP_SLEEP:
		disable_irq(sar->intr_irq);
		err = i2c_cy8c_write_byte_data(client, CS_MODE, CS_CMD_DSLEEP);
		if (err < 0) {
			pr_err("[SAR] %s: I2C write fail. reg %d, err %d\n"
					, __func__, CS_CMD_DSLEEP, err);
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
	struct cy8c_i2c_sar_platform_data *pdata;

	pdata = sar->client->dev.platform_data;
	pr_info("[SAR] %s, position=%d\n", __func__, pdata->position_id);
	if (pdata->position_id == 0) {
		sar->sar_class = class_create(THIS_MODULE, "cap_sense");
		if (IS_ERR(sar->sar_class)) {
			pr_info("[SAR] %s, position=%d, create class fail1\n"
					, __func__, pdata->position_id);
			ret = PTR_ERR(sar->sar_class);
			sar->sar_class = NULL;
			return -ENXIO;
		}
		sar->sar_dev = device_create(sar->sar_class, NULL, 0, sar, "sar");
		if (unlikely(IS_ERR(sar->sar_dev))) {
			pr_info("[SAR] %s, position=%d, create class fail2\n"
					, __func__, pdata->position_id);
			ret = PTR_ERR(sar->sar_dev);
			sar->sar_dev = NULL;
			return -ENOMEM;
		}
	} else if (pdata->position_id == 1) {
		sar->sar_class = class_create(THIS_MODULE, "cap_sense1");
		if (IS_ERR(sar->sar_class)) {
			pr_info("[SAR] %s, position=%d, create class fail3\n"
					, __func__, pdata->position_id);
			ret = PTR_ERR(sar->sar_class);
			sar->sar_class = NULL;
			return -ENXIO;
		}
		sar->sar_dev = device_create(sar->sar_class, NULL, 0, sar, "sar1");
		if (unlikely(IS_ERR(sar->sar_dev))) {
			pr_info("[SAR] %s, position=%d, create class fail4\n"
					, __func__, pdata->position_id);
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

static irqreturn_t cy8c_sar_irq_handler(int irq, void *dev_id)
{
	struct cy8c_sar_data *sar = dev_id;
	struct i2c_client *client = sar->client;
	struct cy8c_i2c_sar_platform_data *pdata = client->dev.platform_data;
	int state, ret; /* written, unused */
	uint8_t buf[1] = {0};

	pr_debug("[SAR%d] %s: enter\n",
			pdata->position_id, __func__);
	ret = i2c_cy8c_read(client, CS_STATUS, buf, 1);
	state = gpio_get_value(pdata->gpio_irq);
	sar->is_activated = !!buf[0];
	pr_debug("[SAR] CS_STATUS:0x%x, is_activated = %d\n",
			buf[0], sar->is_activated);
	schedule_work(&notifier_work);
	return IRQ_HANDLED;
}

static int cy8c_sar_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct cy8c_sar_data *sar;
	struct cy8c_i2c_sar_platform_data *pdata;
	int ret;
	unsigned long spinlock_flags;

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
	}

	sar->intr_irq = gpio_to_irq(pdata->gpio_irq);

	ret = cy8c_init_sensor(sar);
	if (ret < 0) {
		if (ret == -1) {
			pr_err("[SAR][ERR], chip not found.\n");
			goto err_init_sensor_failed;
		} else if (ret == -2)
			ret = sar_fw_update(sar);
			if (ret) {
				pr_err("[SAR][ERR], recover fail.\n");
				goto err_check_functionality_failed;
			} else
				cy8c_init_sensor(sar);
	}
	sar->sleep_mode = sar->radio_state = sar->pm_state = KEEP_AWAKE;
	sar->is_activated = 0;
	sar->polarity = 0; /* 0: low active */

	spin_lock_irqsave(&sar_list_lock, spinlock_flags);
	list_add(&sar->list, &sar_list);
	spin_unlock_irqrestore(&sar_list_lock, spinlock_flags);

	sar->cy8c_wq = create_singlethread_workqueue("cypress_sar");
	if (!sar->cy8c_wq) {
		ret = -ENOMEM;
		pr_err("[SAR][ERR] create_singlethread_workqueue cy8c_wq fail\n");
		goto err_create_wq_failed;
	}
	INIT_DELAYED_WORK(&sar->sleep_work, sar_sleep_func);

	ret = sysfs_create(sar);
	if (ret == -EEXIST)
		goto err_create_device_file;
	else if (ret == -ENOMEM)
		goto err_create_device;
	else if (ret == -ENXIO)
		goto err_create_class;

	sar->use_irq = 1;
	if (client->irq && sar->use_irq) {
		ret = request_threaded_irq(sar->intr_irq, NULL
				, cy8c_sar_irq_handler
				, IRQF_TRIGGER_FALLING | IRQF_ONESHOT
				,CYPRESS_SAR_NAME, sar);
		if (ret < 0) {
			dev_err(&client->dev, "[SAR][ERR] request_irq failed\n");
			pr_err("[SAR][ERR] request_irq failed for gpio %d, irq %d\n",
				pdata->gpio_irq, client->irq);
			goto err_request_irq;
		}
	}
	ret = sar_fw_update(sar);
	if (ret) {
		pr_err("[SAR%d][ERR] SAR sensor fail %d.\n"
				, pdata->position_id, ret);
		goto err_check_functionality_failed;
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
	schedule_work(&notifier_work);
	return ret;
}

static int cy8c_sar_remove(struct i2c_client *client)
{
	struct cy8c_sar_data *sar = i2c_get_clientdata(client);
	unsigned long spinlock_flags;

	disable_irq(client->irq);
	spin_lock_irqsave(&sar_list_lock, spinlock_flags);
	list_del(&sar->list);
	spin_unlock_irqrestore(&sar_list_lock, spinlock_flags);
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
