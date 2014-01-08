/*
 * Copyright(c) 2012, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_data/slimport_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include "slimport_tx_drv.h"
#include <linux/slimport.h>
#ifdef CONFIG_SLIMPORT_CEC
#include "cec_lib/slimport_tx_cec.h"
#endif

struct i2c_client *anx7808_client;
int hdcp_en;

struct anx7808_data {
	struct anx7808_platform_data *pdata;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct mutex lock;
	struct wake_lock slimport_lock;
	int cab_irq;
};

static bool hdcp_enable = 1;

/*sysfs read interface*/
static ssize_t hdcp_ctrl_show(
	struct device *dev, struct device_attribute *attr,
	 char *buf)
{
	return sprintf(buf, "%d\n", hdcp_en);
}

/*sysfs write interface*/
static ssize_t hdcp_ctrl_store(
	struct device *dev, struct device_attribute *attr,
	 const char *buf, size_t count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	hdcp_en = val;
	return count;
}

/* for hdcp control from user space */
static struct device_attribute slimport_device_attrs[] = {
	__ATTR(hdcp_switch, S_IRUGO | S_IWUSR, hdcp_ctrl_show, hdcp_ctrl_store),
};

int sp_read_reg(uint8_t slave_addr, uint8_t offset, uint8_t *buf)
{
	int ret = 0;

	anx7808_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(anx7808_client, offset);
	if (ret < 0) {
		DEV_ERR("%s: failed to read i2c addr=%x\n",
			__func__, slave_addr);
		return ret;
	}
	*buf = (uint8_t) ret;

	return 0;
}

int sp_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value)
{
	int ret = 0;

	anx7808_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_write_byte_data(anx7808_client, offset, value);
	if (ret < 0) {
		DEV_ERR("%s: failed to write i2c addr=%x\n",
			__func__, slave_addr);
	}
	return ret;
}

void sp_tx_hardware_poweron(void)
{
	struct anx7808_platform_data *pdata =
		anx7808_client->dev.platform_data;

	gpio_set_value(pdata->gpio_reset, 0);
	msleep(1);
	gpio_set_value(pdata->gpio_p_dwn, 0);
	msleep(2);
	pdata->dvdd_power(1);
	msleep(20);
	gpio_set_value(pdata->gpio_reset, 1);

	DEV_DBG("%s: anx7808 power on\n", __func__);
}

void sp_tx_hardware_powerdown(void)
{
	struct anx7808_platform_data *pdata =
		anx7808_client->dev.platform_data;

	gpio_set_value(pdata->gpio_reset, 0);
	msleep(1);
	pdata->dvdd_power(0);
	msleep(5);
	gpio_set_value(pdata->gpio_p_dwn, 1);
	msleep(1);

	DEV_DBG("%s: anx7808 power down\n", __func__);
}

static void slimport_cable_plug_proc(struct anx7808_data *anx7808)
{
	if (gpio_get_value_cansleep(anx7808->pdata->gpio_cbl_det)) {
		msleep(50);
		if (gpio_get_value_cansleep(anx7808->pdata->gpio_cbl_det)) {
			if (sp_tx_pd_mode) {
				sp_tx_pd_mode = 0;
				sp_tx_hardware_poweron();
				sp_tx_power_on(SP_TX_PWR_REG);
				sp_tx_power_on(SP_TX_PWR_TOTAL);
				hdmi_rx_initialization();
				sp_tx_initialization();
				sp_tx_vbus_poweron();
				/*msleep(200);*/
				if (!sp_tx_get_cable_type(1)) {
					DEV_ERR("%s:AUX ERR\n", __func__);
					sp_tx_vbus_powerdown();
					sp_tx_power_down(SP_TX_PWR_REG);
					sp_tx_power_down(SP_TX_PWR_TOTAL);
					sp_tx_hardware_powerdown();
					sp_tx_pd_mode = 1;
					sp_tx_link_config_done = 0;
					sp_tx_hw_lt_enable = 0;
					sp_tx_hw_lt_done = 0;
					sp_tx_rx_type = RX_NULL;
					sp_tx_rx_type_backup = RX_NULL;
					sp_tx_set_sys_state(STATE_CABLE_PLUG);
					return;
				}
				sp_tx_rx_type_backup = sp_tx_rx_type;
			}
			switch (sp_tx_rx_type) {
			case RX_HDMI:
				if (sp_tx_get_hdmi_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				break;
			case RX_DP:
				if (sp_tx_get_dp_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				break;
			case RX_VGA_GEN:
				if (sp_tx_get_vga_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				break;
			case RX_VGA_9832:
				if (sp_tx_get_vga_connection()) {
					sp_tx_send_message(MSG_CLEAR_IRQ);
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				}
				break;
			case RX_NULL:
			default:
				break;
			}
		}
	} else if (sp_tx_pd_mode == 0) {
		sp_tx_vbus_powerdown();
		sp_tx_power_down(SP_TX_PWR_REG);
		sp_tx_power_down(SP_TX_PWR_TOTAL);
		sp_tx_hardware_powerdown();
		sp_tx_pd_mode = 1;
		sp_tx_link_config_done = 0;
		sp_tx_hw_lt_enable = 0;
		sp_tx_hw_lt_done = 0;
		sp_tx_rx_type = RX_NULL;
		sp_tx_rx_type_backup = RX_NULL;
		sp_tx_set_sys_state(STATE_CABLE_PLUG);
	}
}

static void slimport_edid_proc(void)
{
	sp_tx_aux_polling_enable(0);

#ifdef CONFIG_SLIMPORT_CEC
	/*Make sure CEC operation only
	happened after cec path is setup.*/
	if ((sp_tx_rx_type == RX_HDMI)
		&& (sp_tx_get_hdmi_connection())) {
		cec_init();
	}
#endif
	sp_tx_edid_read();

	if (bedid_break)
		DEV_ERR("%s: EDID corruption!\n", __func__);

	sp_tx_aux_polling_enable(1);

	hdmi_rx_set_hpd(1);
	hdmi_rx_set_termination(1);
	sp_tx_set_sys_state(STATE_LINK_TRAINING);

}

int slimport_read_edid_block(int block, uint8_t *edid_buf)
{
	if (block == 0) {
		memcpy(edid_buf, bedid_firstblock, sizeof(bedid_firstblock));
	} else if (block == 1) {
		memcpy(edid_buf, bedid_extblock, sizeof(bedid_extblock));
	} else {
		pr_err("%s: block number %d is invalid\n", __func__, block);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(slimport_read_edid_block);

enum SP_LINK_BW slimport_get_link_bw(void)
{
	return slimport_link_bw;
}
EXPORT_SYMBOL(slimport_get_link_bw);

enum RX_CBL_TYPE sp_get_ds_cable_type(void)
{
	return sp_tx_rx_type;
}
EXPORT_SYMBOL(sp_get_ds_cable_type);

#ifdef CONFIG_SLIMPORT_FAST_CHARGE
enum CHARGING_STATUS sp_get_ds_charge_type(void)
{
	return downstream_charging_status;
}
EXPORT_SYMBOL(sp_get_ds_charge_type);
#endif

static void slimport_config_output(void)
{
	sp_tx_clean_hdcp();
	sp_tx_set_colorspace();
	sp_tx_avi_setup();
	sp_tx_config_packets(AVI_PACKETS);
	sp_tx_enable_video_input(1);
	sp_tx_set_sys_state(STATE_HDCP_AUTH);
}

static void slimport_playback_proc(void)
{
	if ((sp_tx_rx_type == RX_VGA_9832)
		|| (sp_tx_rx_type == RX_VGA_GEN)) {
		if ((sp_tx_hw_hdcp_en == 0) && (hdcp_en == 1)) {
			sp_tx_video_mute(1);
			sp_tx_set_sys_state(STATE_HDCP_AUTH);
		} else if ((sp_tx_hw_hdcp_en == 1) && (hdcp_en == 0))
			sp_tx_disable_slimport_hdcp();
	}
}

static void slimport_cable_monitor(struct anx7808_data *anx7808)
{
	if ((gpio_get_value_cansleep(anx7808->pdata->gpio_cbl_det))
		&& (!sp_tx_pd_mode)) {
		sp_tx_get_downstream_type();
		if (sp_tx_rx_type_backup != sp_tx_rx_type) {
			DEV_DBG("cable changed!\n");
			sp_tx_vbus_powerdown();
			sp_tx_power_down(SP_TX_PWR_REG);
			sp_tx_power_down(SP_TX_PWR_TOTAL);
			sp_tx_hardware_powerdown();
			sp_tx_pd_mode = 1;
			sp_tx_link_config_done = 0;
			sp_tx_hw_lt_enable = 0;
			sp_tx_hw_lt_done = 0;
			sp_tx_rx_type = RX_NULL;
			sp_tx_rx_type_backup = RX_NULL;
			sp_tx_set_sys_state(STATE_CABLE_PLUG);
		}
	}
}

static void slimport_main_proc(struct anx7808_data *anx7808)
{
	mutex_lock(&anx7808->lock);

	if (!sp_tx_pd_mode) {
		sp_tx_int_irq_handler();
		hdmi_rx_int_irq_handler();
	}

	if (sp_tx_system_state == STATE_CABLE_PLUG)
		slimport_cable_plug_proc(anx7808);

	if (sp_tx_system_state == STATE_PARSE_EDID)
		slimport_edid_proc();

	if (sp_tx_system_state == STATE_CONFIG_HDMI)
		sp_tx_config_hdmi_input();

	if (sp_tx_system_state == STATE_LINK_TRAINING) {
		if (!sp_tx_lt_pre_config())
			sp_tx_hw_link_training();
	}

	if (sp_tx_system_state == STATE_CONFIG_OUTPUT)
		slimport_config_output();

	if (sp_tx_system_state == STATE_HDCP_AUTH) {
		if (hdcp_enable) {
			sp_tx_hdcp_process();
		} else {
			sp_tx_power_down(SP_TX_PWR_HDCP);
			sp_tx_video_mute(0);
			hdmi_rx_show_video_info();
			sp_tx_show_infomation();
			sp_tx_set_sys_state(STATE_PLAY_BACK);
		}
	}

	if (sp_tx_system_state == STATE_PLAY_BACK)
		slimport_playback_proc();

	slimport_cable_monitor(anx7808);

	mutex_unlock(&anx7808->lock);
}

static uint8_t anx7808_chip_detect(void)
{
	return sp_tx_chip_located();
}

static void anx7808_chip_initial(void)
{
#ifdef EYE_TEST
	sp_tx_eye_diagram_test();
#else
	sp_tx_variable_init();
	sp_tx_vbus_powerdown();
	sp_tx_hardware_powerdown();
	sp_tx_set_sys_state(STATE_CABLE_PLUG);
#endif
}

static void anx7808_free_gpio(struct anx7808_data *anx7808)
{
	gpio_free(anx7808->pdata->gpio_cbl_det);
	gpio_free(anx7808->pdata->gpio_int);
	gpio_free(anx7808->pdata->gpio_reset);
	gpio_free(anx7808->pdata->gpio_p_dwn);
}
static int anx7808_init_gpio(struct anx7808_data *anx7808)
{
	int ret = 0;

	DEV_DBG("anx7808 init gpio\n");

	ret = gpio_request(anx7808->pdata->gpio_p_dwn, "anx_p_dwn_ctl");
	if (ret) {
		DEV_ERR("%s : failed to request gpio %d\n", __func__,
				anx7808->pdata->gpio_p_dwn);
		goto err0;
	}
	gpio_direction_output(anx7808->pdata->gpio_p_dwn, 1);

	ret = gpio_request(anx7808->pdata->gpio_reset, "anx7808_reset_n");
	if (ret) {
		DEV_ERR("%s : failed to request gpio %d\n", __func__,
				anx7808->pdata->gpio_reset);
		goto err1;
	}
	gpio_direction_output(anx7808->pdata->gpio_reset, 0);

	ret = gpio_request(anx7808->pdata->gpio_int, "anx7808_int_n");
	if (ret) {
		DEV_ERR("%s : failed to request gpio %d\n", __func__,
				anx7808->pdata->gpio_int);
		goto err2;
	}
	gpio_direction_input(anx7808->pdata->gpio_int);

	ret = gpio_request(anx7808->pdata->gpio_cbl_det, "anx7808_cbl_det");
	if (ret) {
		DEV_ERR("%s : failed to request gpio %d\n", __func__,
				anx7808->pdata->gpio_cbl_det);
		goto err3;
	}
	gpio_direction_input(anx7808->pdata->gpio_cbl_det);

	gpio_set_value(anx7808->pdata->gpio_reset, 0);
	gpio_set_value(anx7808->pdata->gpio_p_dwn, 1);
	goto out;

err3:
	gpio_free(anx7808->pdata->gpio_cbl_det);
err2:
	gpio_free(anx7808->pdata->gpio_int);
err1:
	gpio_free(anx7808->pdata->gpio_reset);
err0:
	gpio_free(anx7808->pdata->gpio_p_dwn);
out:
	return ret;
}

static int anx7808_system_init(void)
{
	int ret = 0;

	ret = anx7808_chip_detect();
	if (ret == 0) {
		DEV_ERR("%s : failed to detect anx7808\n", __func__);
		return -ENODEV;
	}

	anx7808_chip_initial();
	return 0;
}

static irqreturn_t anx7808_cbl_det_isr(int irq, void *data)
{
	struct anx7808_data *anx7808 = (struct anx7808_data *)data;
	int status;


	if (gpio_get_value(anx7808->pdata->gpio_cbl_det)) {
		wake_lock(&anx7808->slimport_lock);
		DEV_DBG("%s : detect cable insertion\n", __func__);
		queue_delayed_work(anx7808->workqueue, &anx7808->work, 0);
	} else {
		DEV_DBG("%s : detect cable removal\n", __func__);
		status = cancel_delayed_work_sync(&anx7808->work);
		if (status == 0)
			flush_workqueue(anx7808->workqueue);
		wake_unlock(&anx7808->slimport_lock);
	}

	return IRQ_HANDLED;
}

static void anx7808_work_func(struct work_struct *work)
{
#ifndef EYE_TEST
	struct anx7808_data *td = container_of(work, struct anx7808_data,
								work.work);

	slimport_main_proc(td);
	queue_delayed_work(td->workqueue, &td->work,
			msecs_to_jiffies(300));
#endif
}

static int anx7808_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct anx7808_data *anx7808;
	int ret = 0;
	int i;
	if (!i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_I2C_BLOCK)) {
		DEV_ERR("%s: i2c bus does not support the anx7808\n",
			__func__);
		ret = -ENODEV;
		goto exit;
	}

	anx7808 = kzalloc(sizeof(struct anx7808_data), GFP_KERNEL);
	if (!anx7808) {
		DEV_ERR("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	anx7808->pdata = client->dev.platform_data;
	i2c_set_clientdata(client, anx7808);
	memcpy(&anx7808_client, &client, sizeof(client));

	mutex_init(&anx7808->lock);

	if (!anx7808->pdata) {
		ret = -EINVAL;
		goto err0;
	}

	ret = anx7808_init_gpio(anx7808);
	if (ret) {
		DEV_ERR("%s: failed to initialize gpio\n", __func__);
		goto err1;
	}

	INIT_DELAYED_WORK(&anx7808->work, anx7808_work_func);

	anx7808->workqueue = create_singlethread_workqueue("anx7808_work");
	if (anx7808->workqueue == NULL) {
		DEV_ERR("%s: failed to create work queue\n", __func__);
		ret = -ENOMEM;
		goto err2;
	}

	anx7808->pdata->avdd_power(1);
	anx7808->pdata->dvdd_power(1);

	ret = anx7808_system_init();
	if (ret) {
		DEV_ERR("%s: failed to initialize anx7808\n", __func__);
		goto err2;
	}

	anx7808->cab_irq = gpio_to_irq(anx7808->pdata->gpio_cbl_det);
	if (anx7808->cab_irq < 0) {
		DEV_ERR("%s : failed to get gpio irq\n", __func__);
		goto err3;
	}

	ret = request_threaded_irq(anx7808->cab_irq, NULL, anx7808_cbl_det_isr,
					IRQF_TRIGGER_RISING
					| IRQF_TRIGGER_FALLING
					| IRQF_ONESHOT,
					"anx7808_cabel_det", anx7808);
	if (ret < 0) {
		DEV_ERR("%s : failed to request irq\n", __func__);
		goto err3;
	}

	ret = enable_irq_wake(anx7808->cab_irq);
	if (ret < 0) {
		DEV_ERR("%s : Enable irq for cable detect", __func__);
		DEV_ERR("interrupt wake enable fail\n");
		goto err3;
	}

	for (i = 0; i < ARRAY_SIZE(slimport_device_attrs); i++) {
		ret = device_create_file(
			&client->dev, &slimport_device_attrs[i]);
		if (ret) {
			DEV_ERR("%s :anx7808 sysfs register failed\n",
				__func__);
			goto err4;
		}
	}

	wake_lock_init(&anx7808->slimport_lock,
		WAKE_LOCK_SUSPEND, "slimport_wake_lock");
	goto exit;

err4:
	for (i = 0; i < ARRAY_SIZE(slimport_device_attrs); i++)
		device_remove_file(&client->dev, &slimport_device_attrs[i]);
err3:
	free_irq(anx7808->cab_irq, anx7808);
err2:
	destroy_workqueue(anx7808->workqueue);
err1:
	anx7808_free_gpio(anx7808);
err0:
	kfree(anx7808);
exit:
	return ret;
}

static int anx7808_i2c_remove(struct i2c_client *client)
{
	struct anx7808_data *anx7808 = i2c_get_clientdata(client);
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(slimport_device_attrs); i++)
		device_remove_file(&client->dev, &slimport_device_attrs[i]);
	free_irq(anx7808->cab_irq, anx7808);
	anx7808_free_gpio(anx7808);
	destroy_workqueue(anx7808->workqueue);
	wake_lock_destroy(&anx7808->slimport_lock);
	kfree(anx7808);
	return 0;
}

static const struct i2c_device_id anx7808_id[] = {
	{ "anx7808", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, anx7808_id);

static struct i2c_driver anx7808_driver = {
	.driver = {
		.name = "anx7808",
		.owner = THIS_MODULE,
	},
	.probe = anx7808_i2c_probe,
	.remove = anx7808_i2c_remove,
	.id_table = anx7808_id,
};

static int __init anx7808_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&anx7808_driver);
	if (ret < 0)
		DEV_ERR("%s: failed to register anx7808 i2c drivern",
			__func__);
	return ret;
}

static void __exit anx7808_exit(void)
{
	i2c_del_driver(&anx7808_driver);
}

module_init(anx7808_init);
module_exit(anx7808_exit);


MODULE_DESCRIPTION("Slimport  transmitter ANX7808 driver");
MODULE_AUTHOR("FeiWang <fwang@analogixsemi.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("V0.4");
