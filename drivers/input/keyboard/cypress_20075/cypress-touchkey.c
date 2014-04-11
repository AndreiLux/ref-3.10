/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/sec_sysfs.h>
#include <linux/sec_batt.h>

#include "issp_extern.h"
#include "cypress_touchkey.h"

#ifdef  TK_HAS_FIRMWARE_UPDATE
u8 *tk_fw_name = FW_PATH;
/*static u8 fw_ver_file = 0x01;*/
static u8 md_ver_file = 0x06;
u8 module_divider[] = {0, 0xff};
#endif

static int touchkey_keycode[] = { 0,
	KEY_RECENT, KEY_BACK,
};
static const int touchkey_count = ARRAY_SIZE(touchkey_keycode);

static void cypress_config_gpio_i2c(struct touchkey_i2c *tkey_i2c, int onoff);

static bool touchkey_probe;

static const struct i2c_device_id sec_touchkey_id[] = {
	{"sec_touchkey", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sec_touchkey_id);

static unsigned int lcdtype;
static int __init lcdtype_setup(char *str)
{
	get_option(&str, &lcdtype);
	return 1;
}
__setup("lcdtype=", lcdtype_setup);

extern int get_touchkey_firmware(char *version);
static int touchkey_led_status;
static int touchled_cmd_reversed;

#ifdef LED_LDO_WITH_REGULATOR
static void change_touch_key_led_voltage(int vol_mv)
{
	struct regulator *tled_regulator;

	tled_regulator = regulator_get(NULL, TK_LED_REGULATOR_NAME);
	if (IS_ERR(tled_regulator)) {
		pr_err("%s: failed to get resource %s\n", __func__,
		       "touchkey_led");
		return;
	}
	regulator_set_voltage(tled_regulator, vol_mv * 1000, vol_mv * 1000);
	regulator_put(tled_regulator);
}

static ssize_t brightness_control(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	int data;

	if (sscanf(buf, "%d\n", &data) == 1) {
		dev_err(dev, "%s: %d\n", __func__, data);
		change_touch_key_led_voltage(data);
	} else {
		dev_err(dev, "%s Error\n", __func__);
	}

	return size;
}
#endif

static int i2c_touchkey_read(struct i2c_client *client,
		u8 reg, u8 *val, unsigned int len)
{
	int err = 0;
	int retry = 3;

	struct touchkey_i2c *tkey_i2c = i2c_get_clientdata(client);

	if ((client == NULL) || !(tkey_i2c->enabled)) {
		dev_err(&client->dev, "Touchkey is not enabled. %d\n",
		       __LINE__);
		return -ENODEV;
	}

	while (retry--) {
		err = i2c_smbus_read_i2c_block_data(client,
				reg, len, val);

		if (err >= 0)
			return 0;
		dev_err(&client->dev, "%s %d i2c transfer error\n",
		       __func__, __LINE__);
		mdelay(10);
	}
	return err;

}

static int i2c_touchkey_write(struct i2c_client *client,
		u8 *val, unsigned int len)
{
	int err = 0;
	int retry = 3;

	struct touchkey_i2c *tkey_i2c = i2c_get_clientdata(client);

	if ((client == NULL) || !(tkey_i2c->enabled)) {
		dev_err(&client->dev, "Touchkey is not enabled. %d\n",
		       __LINE__);
		return -ENODEV;
	}

	while (retry--) {
		err = i2c_smbus_write_i2c_block_data(client,
				BASE_REG, len, val);

		if (err >= 0)
			return 0;

		dev_err(&client->dev, "%s %d i2c transfer error\n",
		       __func__, __LINE__);
		mdelay(10);
	}
	return err;
}

static int touchkey_i2c_check(struct touchkey_i2c *tkey_i2c)
{
	char data[4] = { 0, };
	int ret = 0;

	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 4);
	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev, "Failed to read Module version\n");
		tkey_i2c->fw_ver_ic = 0;
		tkey_i2c->md_ver_ic = 0;
		tkey_i2c->device_ver = 0;
		return ret;
	}

	tkey_i2c->fw_ver_ic = data[1];
	tkey_i2c->md_ver_ic = data[2];
	tkey_i2c->device_ver = data[3];

	dev_dbg(&tkey_i2c->client->dev, "%s: ic_fw_ver = 0x%02x, module_ver = 0x%02x, CY device = 0x%02x\n",
		__func__, tkey_i2c->fw_ver_ic, tkey_i2c->md_ver_ic, tkey_i2c->device_ver);

	return ret;
}

#if defined(TK_INFORM_CHARGER)
static int touchkey_ta_setting(struct touchkey_i2c *tkey_i2c)
{
	u8 data[6] = { 0, };
	int count = 0;
	int ret = 0;
	unsigned short retry = 0;

    if (tkey_i2c->charging_mode) {
        data[1] = TK_BIT_CMD_TA_ON;
		data[2] = TK_BIT_WRITE_CONFIRM;
    } else {
        data[1] = TK_BIT_CMD_REGULAR;
		data[2] = TK_BIT_WRITE_CONFIRM;
    }

    count = i2c_touchkey_write(tkey_i2c->client, data, 4);

	while (retry < 3) {
		msleep(30);

		ret = i2c_touchkey_read(tkey_i2c->client, TK_STATUS_FLAG, data, 1);

		if (tkey_i2c->charging_mode) {
			if (data[0] & TK_BIT_TA_ON) {
				dev_dbg(&tkey_i2c->client->dev, "%s: TA mode is Enabled\n", __func__);
				break;
			} else {
				dev_err(&tkey_i2c->client->dev, "%s: Error to enable TA mode, retry %d\n",
					__func__, retry);
			}
		} else {
			if (!(data[0] & TK_BIT_TA_ON)) {
				dev_dbg(&tkey_i2c->client->dev, "%s: TA mode is Disabled\n", __func__);
				break;
			} else {
				dev_err(&tkey_i2c->client->dev, "%s: Error to disable TA mode, retry %d\n",
					__func__, retry);
			}
		}
		retry = retry + 1;
	}

	if (retry == 3)
		dev_err(&tkey_i2c->client->dev, "%s: Failed to set the TA mode\n", __func__);

	return count;

}

static void touchkey_ta_cb(struct touchkey_callbacks *cb, bool ta_status)
{
	struct touchkey_i2c *tkey_i2c =
			container_of(cb, struct touchkey_i2c, callbacks);
	struct i2c_client *client = tkey_i2c->client;

	tkey_i2c->charging_mode = ta_status;

	if (tkey_i2c->enabled)
		touchkey_ta_setting(tkey_i2c);
}
#endif
#if defined(CONFIG_GLOVE_TOUCH)
static void touchkey_glove_change_work(struct work_struct *work)
{
	u8 data[6] = { 0, };
	int ret = 0;
	unsigned short retry = 0;
	bool value;
	u8 glove_bit;
	struct touchkey_i2c *tkey_i2c =
			container_of(work, struct touchkey_i2c,
			glove_change_work.work);

#ifdef TKEY_FLIP_MODE
	if (tkey_i2c->enabled_flip) {
		dev_info(&tkey_i2c->client->dev,"As flip cover mode enabled, skip glove mode set\n");
		return;
	}
#endif

	mutex_lock(&tkey_i2c->tsk_glove_lock);
	value = tkey_i2c->tsk_glove_mode_status;
	mutex_unlock(&tkey_i2c->tsk_glove_lock);

	if (!tkey_i2c->enabled)
		return;

    if (value) {
        /* Send glove Command */
        data[1] = TK_BIT_CMD_GLOVE;
		data[2] = TK_BIT_WRITE_CONFIRM;
    } else {
        data[1] = TK_BIT_CMD_REGULAR;
		data[2] = TK_BIT_WRITE_CONFIRM;
    }

    i2c_touchkey_write(tkey_i2c->client, data, 4);

	while (retry < 3) {
        msleep(30);

		ret = i2c_touchkey_read(tkey_i2c->client, TK_STATUS_FLAG, data, 1);

		glove_bit = !!(data[0] & TK_BIT_GLOVE);

		if (value == glove_bit) {
			dev_dbg(&tkey_i2c->client->dev, "%s:Glove mode is %s\n",
				__func__, value ? "enabled" : "disabled");
			break;
		} else
			dev_err(&tkey_i2c->client->dev, "%s:Error to set glove_mode val %d, bit %d, retry %d\n",
				__func__, value, glove_bit, retry);

		retry = retry + 1;
	}
	if (retry == 3)
		dev_err(&tkey_i2c->client->dev, "%s: Failed to set the glove mode\n", __func__);
}

static struct touchkey_i2c *tkey_i2c_global;

void touchkey_glovemode(int on)
{
	struct touchkey_i2c *tkey_i2c = tkey_i2c_global;

	if (!touchkey_probe) {
		dev_err(&tkey_i2c->client->dev, "%s: Touchkey is not probed\n", __func__);
		return;
	}
	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:wackelock active\n");
		return ;
	}

	mutex_lock(&tkey_i2c->tsk_glove_lock);

	/* protect duplicated execution */
	if (on == tkey_i2c->tsk_glove_mode_status) {
		dev_info(&tkey_i2c->client->dev, "pass. cmd %d, cur status %d\n",
			on, tkey_i2c->tsk_glove_mode_status);
		goto end_glovemode;
	}

	cancel_delayed_work(&tkey_i2c->glove_change_work);

	tkey_i2c->tsk_glove_mode_status = on;
	schedule_delayed_work(&tkey_i2c->glove_change_work,
		msecs_to_jiffies(TK_GLOVE_DWORK_TIME));

	dev_info(&tkey_i2c->client->dev, "Touchkey glove %s\n", on ? "On" : "Off");

 end_glovemode:
	mutex_unlock(&tkey_i2c->tsk_glove_lock);
}
#endif

#ifdef TKEY_FLIP_MODE
void touchkey_flip_cover(int value)
{
	struct touchkey_i2c *tkey_i2c = tkey_i2c_global;
	u8 data[6] = { 0, };
	int ret = 0;
	unsigned short retry = 0;
	u8 flip_status;

	tkey_i2c->enabled_flip = value;

	if (!touchkey_probe) {
		dev_err(&tkey_i2c->client->dev, "%s: Touchkey is not probed\n", __func__);
		return;
	}

	if (!tkey_i2c->enabled) {
		dev_err(&tkey_i2c->client->dev, "%s: Touchkey is not enabled\n", __func__);
		return;
	}
	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:wackelock active\n");
		return ;
	}

    if (value == 1) {
        /* Send filp mode Command */
        data[1] = TK_BIT_CMD_FLIP;
		data[2] = TK_BIT_WRITE_CONFIRM;
    } else {
		data[1] = TK_BIT_CMD_REGULAR;
		data[2] = TK_BIT_WRITE_CONFIRM;
    }

    i2c_touchkey_write(tkey_i2c->client, data, 4);

	while (retry < 3) {
		msleep(20);

		/* Check status */
        ret = i2c_touchkey_read(tkey_i2c->client, TK_STATUS_FLAG, data, 1);
		flip_status = !!(data[0] & TK_BIT_FLIP);

		dev_dbg(&tkey_i2c->client->dev,
				"data[0]=%x",data[0] & TK_BIT_FLIP);

		if (value == flip_status) {
			dev_dbg(&tkey_i2c->client->dev, "%s: Flip mode is %s\n", __func__, flip_status ? "enabled" : "disabled");
			break;
		} else
			dev_err(&tkey_i2c->client->dev, "%s: Error to set Flip mode, val %d, flip bit %d, retry %d\n",
				__func__, value, flip_status, retry);

		retry = retry + 1;
	}

	if (retry == 3)
		dev_err(&tkey_i2c->client->dev, "%s: Failed to set the Flip mode\n", __func__);

	return;
}
#endif

#ifdef TKEY_1MM_MODE
void touchkey_1mm_mode(struct touchkey_i2c *tkey_i2c, int value)
{
	u8 data[6] = { 0, };
	int ret = 0;
	u8 retry = 0;
	u8 stylus_status;

	if (!(tkey_i2c->enabled)) {
		dev_info(&tkey_i2c->client->dev, "%s : Touchkey is not enabled.\n",
				__func__);
		return ;
	}

	if (value == 1) {
	/* Send 1mm mode Command */
		data[1] = TK_BIT_CMD_1mmSTYLUS;
		data[2] = TK_BIT_WRITE_CONFIRM;
	} else {
		data[1] = TK_BIT_CMD_REGULAR;
		data[2] = TK_BIT_WRITE_CONFIRM;
	}

	ret = i2c_touchkey_write(tkey_i2c->client, data, 4);
	if (ret < 0) {
		dev_info(&tkey_i2c->client->dev, "%s: Failed to write 1mm mode command.\n",
				__func__);
		return;
	}

	while (retry < 3) {
		msleep(30);
		/* Check status flag mode */
		ret = i2c_touchkey_read(tkey_i2c->client, TK_STATUS_FLAG, data, 1);
		if (ret < 0) {
			dev_info(&tkey_i2c->client->dev, "%s: Failed to check status flag.\n",
					__func__);
			return;
		}
		stylus_status = !!(data[0] & TK_BIT_1mmSTYLUS);

		dev_err(&tkey_i2c->client->dev,
			"data[0]=%x, 1mm: %x, flip: %x, glove: %x, ta: %x\n\n",
			data[0], data[0] & 0x20, data[0] & 0x10, data[0] & 0x08, data[0] & 0x04);

		if (value == stylus_status) {
			dev_info(&tkey_i2c->client->dev,
				"%s: 1MM mode is %s\n", __func__, stylus_status ? "enabled" : "disabled");
				break;
			} else {
			dev_err(&tkey_i2c->client->dev,
				"%s: Error to set 1MM mode, val %d, 1mm bit %d, retry %d\n",
				__func__, value, stylus_status, retry);
		}
		retry = retry + 1;
	}

	if (retry == 3)
		dev_err(&tkey_i2c->client->dev, "[Touchkey] 1mm mode failed\n");

	return;
}
#endif

static int touchkey_enable_status_update(struct touchkey_i2c *tkey_i2c)
{
	u8 data[4] = { 0, };
	int ret = 0;

    ret = i2c_touchkey_read(tkey_i2c->client, BASE_REG, data, 4);
    if (ret < 0) {
        dev_err(&tkey_i2c->client->dev, "%s: Failed to read Keycode_reg\n",
            __func__);
        return ret;
    }

    dev_dbg(&tkey_i2c->client->dev,
            "data[0]=%x, data[1]=%x\n",
            data[0], data[1]);

	data[1] = TK_BIT_CMD_INSPECTION;
	data[2] = TK_BIT_WRITE_CONFIRM;

    ret = i2c_touchkey_write(tkey_i2c->client, data, 4);
	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev, "%s, err(%d)\n", __func__, ret);
		tkey_i2c->status_update = false;
		return ret;
	}

	tkey_i2c->status_update = true;
	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	msleep(20);

	return 0;
}

enum {
	TK_CMD_READ_THRESHOLD = 0,
	TK_CMD_READ_DIFF,
	TK_CMD_READ_RAW,
	TK_CMD_READ_IDAC,
	TK_CMD_COMP_IDAC,
	TK_CMD_BASELINE,
};

const u8 fac_reg_index[] = {
	TK_THRESHOLD,
	TK_DIFF_DATA,
	TK_RAW_DATA,
	TK_IDAC,
	TK_COMP_IDAC,
	TK_BASELINE_DATA,
};

struct FAC_CMD {
	u8 cmd;
	u8 opt1; // 0, 1, 2, 3
	u16 result;
};

static u8 touchkey_get_read_size(u8 cmd)
{
	switch (cmd) {
	case TK_CMD_READ_RAW:
	case TK_CMD_READ_DIFF:
	case TK_BASELINE_DATA:
		return 2;
	case TK_CMD_READ_IDAC:
	case TK_CMD_COMP_IDAC:
	case TK_CMD_READ_THRESHOLD:
		return 1;
		break;
	default:
		break;
	}
	return 0;
}

static int touchkey_fac_read_data(struct device *dev,
		char *buf, struct FAC_CMD *cmd)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int ret;
	u8 size;
	u8 base_index;
	u8 data[26] = { 0, };
	int i, j = 0;
	u16 max_val = 0;

	if (unlikely(!tkey_i2c->status_update)) {
		ret = touchkey_enable_status_update(tkey_i2c);
		if (ret < 0)
			goto out_fac_read_data;
	}

	size = touchkey_get_read_size(cmd->cmd);
	if (size == 0) {
		printk(KERN_ERR"touchkey:wrong size %d\n", size);
		goto out_fac_read_data;
	}

	if (cmd->opt1 > 4) {
		printk(KERN_ERR"touchkey:wrong opt1 %d\n", cmd->opt1);
		goto out_fac_read_data;
	}

	base_index = fac_reg_index[cmd->cmd] + size * cmd->opt1;
	if (base_index > 46) {
		printk(KERN_ERR"touchkey:wrong index %d, cmd %d, size %d, opt1 %d\n",
			base_index, cmd->cmd, size, cmd->opt1);
		goto out_fac_read_data;
	}

	ret = i2c_touchkey_read(tkey_i2c->client, base_index, data, size);
	if (ret <  0) {
		printk(KERN_ERR"touchkey:i2c read failed\n");
		goto out_fac_read_data;
	}

	/* make value */
	cmd->result = 0;
	for (i = size - 1; i >= 0; --i) {
		cmd->result = cmd->result | (data[j++] << (8 * i));
		max_val |= 0xff << (8 * i);
	}

	/* garbage check */
	if (unlikely(cmd->result == max_val)) {
		printk(KERN_ERR"touchkey:cmd %d opt1 %d, max value\n", cmd->cmd, cmd->opt1);
		cmd->result = 0;
	}

 out_fac_read_data:
	return sprintf(buf, "%d\n", cmd->result);
}

static ssize_t touchkey_raw_data0_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_RAW, 0, 0}; /* recent outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_raw_data1_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_RAW, 1, 0}; /* recent inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_raw_data2_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_RAW, 2, 0}; /* back outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_raw_data3_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_RAW, 3, 0}; /* back inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_diff_data0_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_DIFF, 0, 0}; /* recent outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_diff_data1_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_DIFF, 1, 0}; /* recent inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_diff_data2_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_DIFF, 2, 0}; /* back outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_diff_data3_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_DIFF, 3, 0}; /* back inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_baseline_data0_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_BASELINE, 0, 0}; /* recent outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_baseline_data1_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_BASELINE, 1, 0};  /* recent inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_baseline_data2_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_BASELINE, 2, 0}; /* back outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);

}

static ssize_t touchkey_baseline_data3_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_BASELINE, 3, 0}; /* back inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_idac0_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_IDAC, 0, 0}; /* recent outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_idac1_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_IDAC, 1, 0}; /* recent inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_idac2_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_IDAC, 2, 0}; /* back outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_idac3_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_IDAC, 3, 0}; /* back inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_comp_idac0_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_COMP_IDAC, 0, 0}; /* recent outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_comp_idac1_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_COMP_IDAC, 1, 0}; /* recent inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_comp_idac2_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_COMP_IDAC, 2, 0}; /* back outer*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_comp_idac3_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_COMP_IDAC, 3, 0}; /* back inner*/
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_threshold_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_THRESHOLD, 0, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_threshold2_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_THRESHOLD, 1, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_threshold3_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_THRESHOLD, 2, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_threshold4_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_THRESHOLD, 3, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}

#if defined(TK_HAS_FIRMWARE_UPDATE)
#if 0
static int get_module_ver(void)
{
	if (likely(system_rev >= TKEY_MODULE07_HWID))
		return 0x09;
	else
		return 0x04;
}

static void touchkey_init_fw_name(struct touchkey_i2c *tkey_i2c)
{
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	/* check module ver of ic */
/*	if (tkey_i2c->md_ver_ic == 0) {
		tkey_i2c->md_ver_ic = get_module_ver();
		printk(KERN_DEBUG"touchkey:failed to read module_ver. seperate by hwid(ver %x)\n", tkey_i2c->md_ver_ic);
	}
*/
	/*set fw by module_ver*/
	switch(tkey_i2c->md_ver_ic) {
	case 0x08:
	case 0x09:
	case 0x0A:
		break;
	case 0x07:
		tk_fw_name = "cypress/cypress_ha_m07.fw";
		fw_ver_file = 0xA;
		md_ver_file = 0x7;
		break;
	default:
		printk(KERN_DEBUG"touchkey:%s, unknown module ver %x\n", __func__, tkey_i2c->md_ver_ic);
		return ;
	}
#endif
}
#endif
#endif
/* To check firmware compatibility */
int get_module_class(u8 ver)
{
	static int size = ARRAY_SIZE(module_divider);
	int i;

	if (size == 2)
		return 0;

	for (i = size - 1; i > 0; --i) {
		if (ver < module_divider[i] &&
			ver >= module_divider[i-1])
			return i;
	}

	return 0;
}

bool is_same_module_class(struct touchkey_i2c *tkey_i2c)
{
	int class_ic, class_file;

	if (md_ver_file == tkey_i2c->md_ver_ic)
		return true;

	class_file = get_module_class(md_ver_file);
	class_ic = get_module_class(tkey_i2c->md_ver_ic);

	printk(KERN_DEBUG"touchkey:module class, IC %d, File %d\n", class_ic, class_file);

	if (class_file == class_ic)
		return true;

	return false;
}

int tkey_load_fw_built_in(struct touchkey_i2c *tkey_i2c)
{
	int retry = 3;
	int ret;

	while (retry--) {
		ret =
			request_firmware(&tkey_i2c->update_info.firm_data, tk_fw_name,
			&tkey_i2c->client->dev);
		if (ret < 0) {
			printk(KERN_ERR
				"touchkey:Unable to open firmware. ret %d retry %d\n",
				ret, retry);
			continue;
		}
		break;
	}
	tkey_i2c->update_info.fw_img = (struct fw_image *)tkey_i2c->update_info.firm_data->data;
	tkey_i2c->src_fw_ver = tkey_i2c->update_info.fw_img->first_fw_ver;

	return ret;
}

int tkey_load_fw_sdcard(struct touchkey_i2c *tkey_i2c)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;
/*	unsigned int nSize;*/

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(TKEY_FW_PATH, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) {
		printk(KERN_ERR "touchkey:failed to open %s.\n", TKEY_FW_PATH);
		ret = -ENOENT;
		set_fs(old_fs);
		return ret;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	printk(KERN_NOTICE
		"touchkey:start, file path %s, size %ld Bytes\n",
		TKEY_FW_PATH, fsize);

	tkey_i2c->update_info.fw_img = kmalloc(fsize, GFP_KERNEL);
	if (!tkey_i2c->update_info.fw_img) {
		printk(KERN_ERR
			"touchkey:%s, kmalloc failed\n", __func__);
		ret = -EFAULT;
		goto malloc_error;
	}

	nread = vfs_read(fp, (char __user *)tkey_i2c->update_info.fw_img,
		fsize, &fp->f_pos);
	printk(KERN_NOTICE "touchkey:nread %ld Bytes\n", nread);
	if (nread != fsize) {
		printk(KERN_ERR
			"touchkey:failed to read firmware file, nread %ld Bytes\n",
			nread);
		ret = -EIO;
		kfree(tkey_i2c->update_info.fw_img);
		goto read_err;
	}

	filp_close(fp, current->files);
	set_fs(old_fs);

	return 0;

read_err:
malloc_error:
//size_error:
	filp_close(fp, current->files);
	set_fs(old_fs);
	return ret;
}

int touchkey_load_fw(struct touchkey_i2c *tkey_i2c, u8 fw_path)
{
	int ret = 0;

	switch (fw_path) {
	case FW_BUILT_IN:
		ret = tkey_load_fw_built_in(tkey_i2c);
		break;
	case FW_IN_SDCARD:
		ret = tkey_load_fw_sdcard(tkey_i2c);
		break;
	default:
		printk(KERN_DEBUG"touchkey:unknown path(%d)\n", fw_path);
		break;
	}

	return ret;
}

void touchkey_unload_fw(struct touchkey_i2c *tkey_i2c)
{
	switch (tkey_i2c->update_info.fw_path) {
	case FW_BUILT_IN:
		release_firmware(tkey_i2c->update_info.firm_data);
		tkey_i2c->update_info.firm_data = NULL;
		break;
	case FW_IN_SDCARD:
		kfree(tkey_i2c->update_info.fw_img);
		tkey_i2c->update_info.fw_img = NULL;
		break;
	default:
		break;
	}
	tkey_i2c->update_info.fw_path = FW_NONE;
}

int touchkey_fw_update(struct touchkey_i2c *tkey_i2c, u8 fw_path, bool bforced)
{
	int ret;

	if (tkey_i2c->device_ver != 0x10 && bforced == false)
		return 0;

	ret = touchkey_load_fw(tkey_i2c, fw_path);
	if (ret < 0) {
		printk(KERN_DEBUG"touchkey:failed to load fw data\n");
		return ret;
	}
	tkey_i2c->update_info.fw_path = fw_path;

	/* f/w info */
	dev_info(&tkey_i2c->client->dev, "fw ver %#x, new fw ver %#x\n",
		tkey_i2c->fw_ver_ic, tkey_i2c->src_fw_ver);
	dev_info(&tkey_i2c->client->dev, "module ver %#x, new module ver %#x\n",
		tkey_i2c->md_ver_ic, md_ver_file);
	dev_info(&tkey_i2c->client->dev, "new checksum %#x\n",
		tkey_i2c->update_info.fw_img->checksum);

	if (unlikely(bforced))
		goto run_fw_update;

	if (tkey_i2c->md_ver_ic != md_ver_file)
		goto out_fw_update;

	/* check update condition */
	ret = is_same_module_class(tkey_i2c);
	if (ret == false)
		goto out_fw_update;

#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
	if (tkey_i2c->fw_ver_ic != tkey_i2c->src_fw_ver)
		goto run_fw_update;
#else
	if (tkey_i2c->fw_ver_ic < tkey_i2c->src_fw_ver)
		goto run_fw_update;
	if (tkey_i2c->fw_ver_ic > tkey_i2c->src_fw_ver)
		goto out_fw_update;
#endif

	/* do not need to update f/w */
	/* check checksum */
	tkey_i2c->do_checksum = true;
	goto run_fw_update;

out_fw_update:

	/* pass update */
	dev_info(&tkey_i2c->client->dev, "pass fw update\n");
	touchkey_unload_fw(tkey_i2c);

 run_fw_update:
	queue_work(tkey_i2c->fw_wq, &tkey_i2c->update_work);
	return 0;
}

static void touchkey_i2c_update_work(struct work_struct *work)
{
	struct touchkey_i2c *tkey_i2c =
		container_of(work, struct touchkey_i2c, update_work);
/*	const struct firmware *firm_data = NULL;*/
	int ret;
	int retry = 3;

	disable_irq(tkey_i2c->irq);
	wake_lock(&tkey_i2c->fw_wakelock);

	if (tkey_i2c->update_info.fw_path == FW_NONE)
		goto end_fw_update;

	printk(KERN_DEBUG"touchkey:%s\n", __func__);
	tkey_i2c->update_status = TK_UPDATE_DOWN;
	cypress_config_gpio_i2c(tkey_i2c, 0);

	while (retry--) {
		ret = ISSP_main(tkey_i2c);
		if (ret != 0) {
			msleep(50);
			dev_err(&tkey_i2c->client->dev, "failed to update f/w. retry\n");
			continue;
		}

		dev_info(&tkey_i2c->client->dev, "finish f/w update\n");
		tkey_i2c->update_status = TK_UPDATE_PASS;
		break;
	}
	cypress_config_gpio_i2c(tkey_i2c, 1);

	tkey_i2c->pdata->power_on(tkey_i2c, 0);
	msleep(10);
	tkey_i2c->pdata->power_on(tkey_i2c, 1);

	if (retry <= 0) {
		tkey_i2c->pdata->power_on(tkey_i2c, 0);
		tkey_i2c->update_status = TK_UPDATE_FAIL;
		dev_err(&tkey_i2c->client->dev, "failed to update f/w\n");
		goto err_fw_update;
	}

	ret = touchkey_i2c_check(tkey_i2c);
	if (ret < 0)
		goto err_fw_update;

	dev_info(&tkey_i2c->client->dev, "f/w ver = %#X, module ver = %#X\n",
		tkey_i2c->fw_ver_ic, tkey_i2c->md_ver_ic);

 end_fw_update:
	tkey_i2c->pdata->led_power_on(1);
	enable_irq(tkey_i2c->irq);
 err_fw_update:
	touchkey_unload_fw(tkey_i2c);
	wake_unlock(&tkey_i2c->fw_wakelock);
}


static irqreturn_t touchkey_interrupt(int irq, void *dev_id)
{
	struct touchkey_i2c *tkey_i2c = dev_id;
	u8 data[3];
	int ret;
	int keycode_type = 0;
	int pressed;

	if (unlikely(!touchkey_probe)) {
		dev_err(&tkey_i2c->client->dev, "%s: Touchkey is not probed\n", __func__);
		return IRQ_HANDLED;
	}

	ret = i2c_touchkey_read(tkey_i2c->client, KEYCODE_REG, data, 1);
	if (ret < 0)
		return IRQ_HANDLED;

	keycode_type = (data[0] & TK_BIT_KEYCODE);
	pressed = !(data[0] & TK_BIT_PRESS_EV);

	if (keycode_type <= 0 || keycode_type >= touchkey_count) {
		dev_dbg(&tkey_i2c->client->dev, "keycode_type err\n");
		return IRQ_HANDLED;
	}

	input_report_key(tkey_i2c->input_dev,
			 touchkey_keycode[keycode_type], pressed);
	input_sync(tkey_i2c->input_dev);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	dev_info(&tkey_i2c->client->dev, "keycode:%d pressed:%d %d\n",
	touchkey_keycode[keycode_type], pressed, tkey_i2c->tsk_glove_mode_status);
#else
	dev_info(&tkey_i2c->client->dev, "pressed:%d %d\n",
		pressed, tkey_i2c->tsk_glove_mode_status);
#endif
#ifdef TOUCHKEY_BOOSTER
	INPUT_BOOSTER_SEND_EVENT(touchkey_keycode[keycode_type],
		!!pressed);
#endif
	return IRQ_HANDLED;
}

static int touchkey_stop(struct touchkey_i2c *tkey_i2c)
{
	int i;

	mutex_lock(&tkey_i2c->lock);

	if (!tkey_i2c->enabled) {
		dev_err(&tkey_i2c->client->dev, "Touch key already disabled\n");
		goto err_stop_out;
	}
	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:wake_lock active\n");
		goto err_stop_out;
	}

	disable_irq(tkey_i2c->irq);

	/* release keys */
	for (i = 1; i < touchkey_count; ++i) {
		input_report_key(tkey_i2c->input_dev,
				 touchkey_keycode[i], 0);
#ifdef TOUCHKEY_BOOSTER
		INPUT_BOOSTER_SEND_EVENT(touchkey_keycode[i],
			BOOSTER_MODE_FORCE_OFF);
#endif
	}
	input_sync(tkey_i2c->input_dev);

#if defined(CONFIG_GLOVE_TOUCH)
	/*cancel or waiting before pwr off*/
	tkey_i2c->tsk_glove_mode_status = false;
	cancel_delayed_work(&tkey_i2c->glove_change_work);
#endif

	/* disable ldo18 */
	tkey_i2c->pdata->led_power_on(0);

	/* disable ldo11 */
	tkey_i2c->pdata->power_on(tkey_i2c, 0);

	tkey_i2c->enabled = false;
	tkey_i2c->status_update = false;

 err_stop_out:
	mutex_unlock(&tkey_i2c->lock);

	return 0;
}

static int touchkey_start(struct touchkey_i2c *tkey_i2c)
{
	mutex_lock(&tkey_i2c->lock);

	if (tkey_i2c->enabled) {
		dev_err(&tkey_i2c->client->dev, "Touch key already enabled\n");
		goto err_start_out;
	}
	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:wake_lock active\n");
		goto err_start_out;
	}

	/* enable ldo11 */
	tkey_i2c->pdata->power_on(tkey_i2c, 1);
	msleep(50);
	tkey_i2c->pdata->led_power_on(1);

	tkey_i2c->enabled = true;

	if (touchled_cmd_reversed) {
		touchled_cmd_reversed = 0;
		i2c_touchkey_write(tkey_i2c->client,
			(u8 *) &touchkey_led_status, 1);
		dev_err(&tkey_i2c->client->dev, "%s: Turning LED is reserved\n", __func__);
		msleep(30);
	}

#ifdef TEST_JIG_MODE
    touchkey_enable_status_update(tkey_i2c);
#endif

#if defined(TK_INFORM_CHARGER)
	touchkey_ta_setting(tkey_i2c);
#endif

#if defined(CONFIG_GLOVE_TOUCH)
	//tkey_i2c->tsk_glove_lock_status = false;
	touchkey_glovemode(tkey_i2c->tsk_glove_mode_status);
#endif
	enable_irq(tkey_i2c->irq);
 err_start_out:
	mutex_unlock(&tkey_i2c->lock);

	return 0;
}

#ifdef TK_USE_OPEN_DWORK
static void touchkey_open_work(struct work_struct *work)
{
	int retval;
	struct touchkey_i2c *tkey_i2c =
			container_of(work, struct touchkey_i2c,
			open_work.work);

	if (tkey_i2c->enabled) {
		dev_err(&tkey_i2c->client->dev, "Touch key already enabled\n");
		return;
	}

	retval = touchkey_start(tkey_i2c);
	if (retval < 0)
		dev_err(&tkey_i2c->client->dev,
				"%s: Failed to start device\n", __func__);
}
#endif

static int touchkey_input_open(struct input_dev *dev)
{
	struct touchkey_i2c *data = input_get_drvdata(dev);
	int ret;

	if (!touchkey_probe) {
		dev_err(&data->client->dev, "%s: Touchkey is not probed\n", __func__);
		return 0;
	}

/*	ret = wait_for_completion_interruptible_timeout(&data->init_done,
			msecs_to_jiffies(90 * MSEC_PER_SEC));

	if (ret < 0) {
		dev_err(&data->client->dev,
			"error while waiting for device to init (%d)\n", ret);
		ret = -ENXIO;
		goto err_open;
	}
	if (ret == 0) {
		dev_err(&data->client->dev,
			"timedout while waiting for device to init\n");
		ret = -ENXIO;
		goto err_open;
	}*/
#ifdef TK_USE_OPEN_DWORK
	schedule_delayed_work(&data->open_work,
					msecs_to_jiffies(TK_OPEN_DWORK_TIME));
#else
	ret = touchkey_start(data);
	if (ret)
		goto err_open;
#endif

	dev_dbg(&data->client->dev, "%s\n", __func__);

	return 0;

err_open:
	return ret;
}

static void touchkey_input_close(struct input_dev *dev)
{
	struct touchkey_i2c *data = input_get_drvdata(dev);

	if (!touchkey_probe) {
		dev_err(&data->client->dev, "%s: Touchkey is not probed\n", __func__);
		return;
	}

#ifdef TK_USE_OPEN_DWORK
	cancel_delayed_work(&data->open_work);
#endif
	touchkey_stop(data);

	dev_dbg(&data->client->dev, "%s\n", __func__);
}

#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
#define touchkey_suspend	NULL
#define touchkey_resume	NULL

static int sec_touchkey_early_suspend(struct early_suspend *h)
{
	struct touchkey_i2c *tkey_i2c =
		container_of(h, struct touchkey_i2c, early_suspend);

	touchkey_stop(tkey_i2c);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	return 0;
}

static int sec_touchkey_late_resume(struct early_suspend *h)
{
	struct touchkey_i2c *tkey_i2c =
		container_of(h, struct touchkey_i2c, early_suspend);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	touchkey_start(tkey_i2c);

	return 0;
}
#else
static int touchkey_suspend(struct device *dev)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	if (touchkey_probe != true) {
		printk(KERN_ERR "%s Touchkey is not enabled. \n",
		       __func__);
		return 0;
	}
	mutex_lock(&tkey_i2c->input_dev->mutex);

	if (tkey_i2c->input_dev->users)
		touchkey_stop(tkey_i2c);

	mutex_unlock(&tkey_i2c->input_dev->mutex);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	return 0;
}

static int touchkey_resume(struct device *dev)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	if (touchkey_probe != true) {
		printk(KERN_ERR "%s Touchkey is not enabled. \n",
		       __func__);
		return 0;
	}
	mutex_lock(&tkey_i2c->input_dev->mutex);

	if (tkey_i2c->input_dev->users)
		touchkey_start(tkey_i2c);

	mutex_unlock(&tkey_i2c->input_dev->mutex);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	return 0;
}
#endif
static SIMPLE_DEV_PM_OPS(touchkey_pm_ops, touchkey_suspend, touchkey_resume);
#endif

static ssize_t touchkey_led_control(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;
	int ret;
	static const int ledCmd[] = {TK_CMD_LED_OFF, TK_CMD_LED_ON};

	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:%s, wakelock active\n", __func__);
		return size;
	}

	ret = sscanf(buf, "%d", &data);

	if (ret != 1) {
		dev_err(&tkey_i2c->client->dev, "%s, %d err\n",
			__func__, __LINE__);
		return size;
	}

	if (data != 0 && data != 1) {
		dev_err(&tkey_i2c->client->dev, "%s wrong cmd %x\n",
			__func__, data);
		return size;
	}

	data = ledCmd[data];

	if (!tkey_i2c->enabled) {
		touchled_cmd_reversed = 1;
		goto out;
	}

	ret = i2c_touchkey_write(tkey_i2c->client, (u8 *) &data, 1);
	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev, "%s: Error turn on led %d\n",
			__func__, ret);
		touchled_cmd_reversed = 1;
		goto out;
	}
	msleep(30);

out:
	touchkey_led_status = data;

	return size;
}

#if defined(CONFIG_GLOVE_TOUCH)
static ssize_t glove_mode_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;

	sscanf(buf, "%d\n", &data);
	dev_dbg(&tkey_i2c->client->dev, "%s %d\n", __func__, data);

	touchkey_glovemode(data);

	return size;
}
#endif

#ifdef TKEY_FLIP_MODE
static ssize_t flip_cover_mode_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;

	sscanf(buf, "%d\n", &data);
	dev_info(&tkey_i2c->client->dev, "%s %d\n", __func__, data);

	touchkey_flip_cover(data);

	return size;
}
#endif

static ssize_t touch_sensitivity_control(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int ret, enable;
	u8 data[4] = { 0, };

	sscanf(buf, "%d\n", &enable);
	dev_info(&tkey_i2c->client->dev, "%s %d\n", __func__, enable);

/*    ret = touchkey_enable_status_update(tkey_i2c, data);
    if (ret < 0)
        return ret;
*/
	if (enable == 1) {
		data[1] = TK_BIT_CMD_INSPECTION;
		data[2] = TK_BIT_WRITE_CONFIRM;
		tkey_i2c->status_update = true;
	} else {
		data[1] = TK_BIT_CMD_REGULAR;
		data[2] = TK_BIT_WRITE_CONFIRM;
		tkey_i2c->status_update = false;
	}

    ret = i2c_touchkey_write(tkey_i2c->client, data, 4);
	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev, "%s, err(%d)\n", __func__, ret);
		tkey_i2c->status_update = false;
		return ret;
	}

	ret = i2c_smbus_read_i2c_block_data(tkey_i2c->client,
				BASE_REG, 4, data);
	if (ret < 0) {
		dev_info(&tkey_i2c->client->dev,
			"[Touchkey] fail to CYPRESS_DATA_UPDATE.\n");
		return ret;
	}
	if ((data[1] & 0x20))
		dev_info(&tkey_i2c->client->dev,
			"[Touchkey] Control Enabled!!\n");
	else
		dev_info(&tkey_i2c->client->dev,
			"[Touchkey] Control Disabled!!\n");

	return size;
}

static ssize_t set_touchkey_firm_version_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	printk(KERN_DEBUG"touchkey:firm_ver_bin %0#4x\n", tkey_i2c->src_fw_ver);
	return sprintf(buf, "%0#4x\n", tkey_i2c->src_fw_ver);
}

static ssize_t set_touchkey_update_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	u8 fw_path;

	switch(*buf) {
	case 's':
	case 'S':
		fw_path = FW_BUILT_IN;
		break;
	case 'i':
	case 'I':
		fw_path = FW_IN_SDCARD;
		break;
	default:
		return size;
	}

	touchkey_fw_update(tkey_i2c, fw_path, true);

	msleep(3000);
	cancel_work_sync(&tkey_i2c->update_work);

	return size;
}

static ssize_t set_touchkey_firm_version_read_show(struct device *dev,
						   struct device_attribute
						   *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	char data[3] = { 0, };
	int count;

	i2c_touchkey_read(tkey_i2c->client, TK_FW_VER, data, 2);
	count = sprintf(buf, "0x%02x\n", data[0]);

	dev_info(&tkey_i2c->client->dev, "Touch_version_read 0x%02x\n", data[0]);
	dev_info(&tkey_i2c->client->dev, "Module_version_read 0x%02x\n", data[1]);
	return count;
}

static ssize_t set_touchkey_firm_status_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int count = 0;

	dev_info(&tkey_i2c->client->dev, "Touch_update_read: update_status %d\n",
	       tkey_i2c->update_status);

	if (tkey_i2c->update_status == TK_UPDATE_PASS)
		count = sprintf(buf, "PASS\n");
	else if (tkey_i2c->update_status == TK_UPDATE_DOWN)
		count = sprintf(buf, "Downloading\n");
	else if (tkey_i2c->update_status == TK_UPDATE_FAIL)
		count = sprintf(buf, "Fail\n");

	return count;
}
#ifdef TKEY_1MM_MODE
static ssize_t touchkey_1mm_mode_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;

	sscanf(buf, "%d\n", &data);
	dev_info(&tkey_i2c->client->dev, "%s %d\n", __func__, data);

	touchkey_1mm_mode(tkey_i2c, data);

	return size;
}
#endif
#ifdef TOUCHKEY_BOOSTER
static ssize_t touchkey_boost_level(struct device *dev,
						struct device_attribute *attr, const char *buf,
						size_t count)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int level;

	sscanf(buf, "%d", &level);

	if (level < 0 || level >= BOOSTER_LEVEL_MAX) {
		dev_err(&tkey_i2c->client->dev, "err to set boost_level %d\n", level);
		return count;
	}
	change_booster_level_for_tsk(level);

	dev_info(&tkey_i2c->client->dev, "%s %d\n", __func__, level);

	return count;
}
#endif

static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   touchkey_led_control);
static DEVICE_ATTR(touch_sensitivity, S_IRUGO | S_IWUSR | S_IWGRP,
		   NULL, touch_sensitivity_control);
static DEVICE_ATTR(touchkey_firm_update, S_IRUGO | S_IWUSR | S_IWGRP,
		   NULL, set_touchkey_update_store);
static DEVICE_ATTR(touchkey_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP,
		   set_touchkey_firm_status_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO | S_IWUSR | S_IWGRP,
		   set_touchkey_firm_version_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP,
		   set_touchkey_firm_version_read_show, NULL);
#ifdef LED_LDO_WITH_REGULATOR
static DEVICE_ATTR(touchkey_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
		   NULL, brightness_control);
#endif
static DEVICE_ATTR(touchkey_recent, S_IRUGO, touchkey_diff_data1_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, touchkey_diff_data3_show, NULL);
static DEVICE_ATTR(touchkey_recent_outer, S_IRUGO, touchkey_diff_data0_show, NULL);
static DEVICE_ATTR(touchkey_recent_inner, S_IRUGO, touchkey_diff_data1_show, NULL);
static DEVICE_ATTR(touchkey_back_outer, S_IRUGO, touchkey_diff_data2_show, NULL);
static DEVICE_ATTR(touchkey_back_inner, S_IRUGO, touchkey_diff_data3_show, NULL);
static DEVICE_ATTR(touchkey_recent_raw_outer, S_IRUGO, touchkey_raw_data0_show, NULL);
static DEVICE_ATTR(touchkey_recent_raw_inner, S_IRUGO, touchkey_raw_data1_show, NULL);
static DEVICE_ATTR(touchkey_back_raw_outer, S_IRUGO, touchkey_raw_data2_show, NULL);
static DEVICE_ATTR(touchkey_back_raw_inner, S_IRUGO, touchkey_raw_data3_show, NULL);
static DEVICE_ATTR(touchkey_baseline_data0, S_IRUGO, touchkey_baseline_data0_show, NULL);
static DEVICE_ATTR(touchkey_baseline_data1, S_IRUGO, touchkey_baseline_data1_show, NULL);
static DEVICE_ATTR(touchkey_baseline_data2, S_IRUGO, touchkey_baseline_data2_show, NULL);
static DEVICE_ATTR(touchkey_baseline_data3, S_IRUGO, touchkey_baseline_data3_show, NULL);
static DEVICE_ATTR(touchkey_recent_idac_outer, S_IRUGO, touchkey_idac0_show, NULL);
static DEVICE_ATTR(touchkey_recent_idac_inner, S_IRUGO, touchkey_idac1_show, NULL);
static DEVICE_ATTR(touchkey_back_idac_outer, S_IRUGO, touchkey_idac2_show, NULL);
static DEVICE_ATTR(touchkey_back_idac_inner, S_IRUGO, touchkey_idac3_show, NULL);
static DEVICE_ATTR(touchkey_comp_idac0, S_IRUGO, touchkey_comp_idac0_show, NULL);
static DEVICE_ATTR(touchkey_comp_idac1, S_IRUGO, touchkey_comp_idac1_show, NULL);
static DEVICE_ATTR(touchkey_comp_idac2, S_IRUGO, touchkey_comp_idac2_show, NULL);
static DEVICE_ATTR(touchkey_comp_idac3, S_IRUGO, touchkey_comp_idac3_show, NULL);
static DEVICE_ATTR(touchkey_recent_threshold_outer, S_IRUGO, touchkey_threshold_show, NULL);
static DEVICE_ATTR(touchkey_recent_threshold_inner, S_IRUGO, touchkey_threshold2_show, NULL);
static DEVICE_ATTR(touchkey_back_threshold_outer, S_IRUGO, touchkey_threshold3_show, NULL);
static DEVICE_ATTR(touchkey_back_threshold_inner, S_IRUGO, touchkey_threshold4_show, NULL);
#if defined(CONFIG_GLOVE_TOUCH)
static DEVICE_ATTR(glove_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   glove_mode_enable);
#endif
#ifdef TKEY_FLIP_MODE
static DEVICE_ATTR(flip_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   flip_cover_mode_enable);
#endif
#ifdef TKEY_1MM_MODE
static DEVICE_ATTR(1mm_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		touchkey_1mm_mode_enable);
#endif
#ifdef TOUCHKEY_BOOSTER
static DEVICE_ATTR(boost_level, S_IWUSR | S_IWGRP, NULL, touchkey_boost_level);
#endif

static struct attribute *touchkey_attributes[] = {
	&dev_attr_brightness.attr,
	&dev_attr_touch_sensitivity.attr,
	&dev_attr_touchkey_firm_update.attr,
	&dev_attr_touchkey_firm_update_status.attr,
	&dev_attr_touchkey_firm_version_phone.attr,
	&dev_attr_touchkey_firm_version_panel.attr,
#ifdef LED_LDO_WITH_REGULATOR
	&dev_attr_touchkey_brightness.attr,
#endif
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_recent_outer.attr,
	&dev_attr_touchkey_recent_inner.attr,
	&dev_attr_touchkey_back_outer.attr,
	&dev_attr_touchkey_back_inner.attr,
	&dev_attr_touchkey_recent_raw_outer.attr,
	&dev_attr_touchkey_recent_raw_inner.attr,
	&dev_attr_touchkey_back_raw_outer.attr,
	&dev_attr_touchkey_back_raw_inner.attr,
	&dev_attr_touchkey_baseline_data0.attr,
	&dev_attr_touchkey_baseline_data1.attr,
	&dev_attr_touchkey_baseline_data2.attr,
	&dev_attr_touchkey_baseline_data3.attr,
	&dev_attr_touchkey_recent_idac_outer.attr,
	&dev_attr_touchkey_recent_idac_inner.attr,
	&dev_attr_touchkey_back_idac_outer.attr,
	&dev_attr_touchkey_back_idac_inner.attr,
	&dev_attr_touchkey_comp_idac0.attr,
	&dev_attr_touchkey_comp_idac1.attr,
	&dev_attr_touchkey_comp_idac2.attr,
	&dev_attr_touchkey_comp_idac3.attr,
	&dev_attr_touchkey_recent_threshold_outer.attr,
	&dev_attr_touchkey_recent_threshold_inner.attr,
	&dev_attr_touchkey_back_threshold_outer.attr,
	&dev_attr_touchkey_back_threshold_inner.attr,
#if defined(CONFIG_GLOVE_TOUCH)
	&dev_attr_glove_mode.attr,
#endif
#ifdef TKEY_FLIP_MODE
	&dev_attr_flip_mode.attr,
#endif
#ifdef TKEY_1MM_MODE
	&dev_attr_1mm_mode.attr,
#endif
#ifdef TOUCHKEY_BOOSTER
	&dev_attr_boost_level.attr,
#endif
	NULL,
};

static struct attribute_group touchkey_attr_group = {
	.attrs = touchkey_attributes,
};
static int touchkey_power_on(void *data, bool on) {

	struct touchkey_i2c *tkey_i2c = (struct touchkey_i2c *)data;
	struct device *dev = &tkey_i2c->client->dev;
	struct pinctrl *pinctrl_irq;
	struct regulator *regulator;
	static bool enabled;
	int ret = 0;

	if (enabled == on) {
		printk(KERN_ERR "%s : TK power already %s\n", __func__,(on)?"on":"off");
		return ret;
	}

	printk(KERN_ERR "%s: %s",__func__,(on)?"on":"off");

	if (on) {
		regulator = regulator_get(NULL, TK_REGULATOR_NAME);
		if (IS_ERR(regulator)) {
			printk(KERN_ERR
				"[Touchkey] %s : TK regulator_get failed\n", __func__);
			return -EIO;
		}
		ret = regulator_enable(regulator);
		if (ret) {
			printk(KERN_ERR "%s: Failed to enable avdd: %d\n", __func__, ret);
			return ret;
		}

		pinctrl_irq = devm_pinctrl_get_select(dev, "on_irq");
		if (IS_ERR(pinctrl_irq))
			printk(KERN_ERR "%s: Failed to configure irq pin\n", __func__);
	} else {
		regulator = regulator_get(NULL, TK_REGULATOR_NAME);
		if (IS_ERR(regulator)) {
			printk(KERN_ERR
				"[Touchkey] %s : TK regulator_get failed\n", __func__);
			return -EIO;
		}
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);

		pinctrl_irq = devm_pinctrl_get_select(dev, "off_irq");
		if (IS_ERR(pinctrl_irq))
			printk(KERN_ERR "%s: Failed to configure irq pin\n", __func__);
	}

	enabled = on;
	regulator_put(regulator);

	return 1;
}
static int touchkey_led_power_on(bool on)
{
	struct regulator *regulator;
	static bool enabled;
	int ret = 0;

	if (enabled == on) {
		printk(KERN_ERR "%s : TK led power already %s\n", __func__,(on)?"on":"off");
		return ret;
	}

	printk(KERN_ERR "%s: %s",__func__,(on)?"on":"off");

	if (on) {
		regulator = regulator_get(NULL, TK_LED_REGULATOR_NAME);
		if (IS_ERR(regulator))
			return 0;
		ret = regulator_enable(regulator);
		if (ret) {
			printk(KERN_ERR "%s: Failed to enable led: %d\n", __func__, ret);
			return ret;
		}
		regulator_put(regulator);
	} else {
		regulator = regulator_get(NULL, TK_LED_REGULATOR_NAME);
		if (IS_ERR(regulator))
			return 0;
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
		regulator_put(regulator);
	}

	enabled = on;
	return 1;
}
static void cypress_config_gpio_i2c(struct touchkey_i2c *tkey_i2c, int onoff)
{
	struct device *i2c_dev;
	struct pinctrl *pinctrl_i2c;

	if (tkey_i2c->pdata->i2c_gpio)
		i2c_dev = &tkey_i2c->client->dev;
	else {
		i2c_dev = tkey_i2c->client->dev.parent->parent;
	}

	if (onoff) {
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "on_i2c");
		if (IS_ERR(pinctrl_i2c))
			printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
	} else {
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "off_i2c");
		if (IS_ERR(pinctrl_i2c))
			printk(KERN_ERR "%s: Failed to configure i2c pin\n", __func__);
	}
}

#ifdef CONFIG_OF
static void cypress_request_gpio(struct i2c_client *client, struct touchkey_platform_data *pdata)
{
	int ret;

	if (!pdata->i2c_gpio) {
		ret = gpio_request(pdata->gpio_scl, "touchkey_scl");
		if (ret) {
			printk(KERN_ERR "%s: unable to request touchkey_scl [%d]\n",
					__func__, pdata->gpio_scl);
			return;
		}

		ret = gpio_request(pdata->gpio_sda, "touchkey_sda");
		if (ret) {
			printk(KERN_ERR "%s: unable to request touchkey_sda [%d]\n",
					__func__, pdata->gpio_sda);
			return;
		}
	}

	ret = gpio_request(pdata->gpio_int, "touchkey_irq");
	if (ret) {
		printk(KERN_ERR "%s: unable to request touchkey_irq [%d]\n",
				__func__, pdata->gpio_int);
		return;
	}
}

static struct touchkey_platform_data *cypress_parse_dt(struct i2c_client *client)
{

	struct device *dev = &client->dev;
	struct touchkey_platform_data *pdata;
	struct device_node *np = dev->of_node;

	if (!np)
		return ERR_PTR(-ENOENT);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "failed to allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}
	dev->platform_data = pdata;

	/* reset, irq gpio info */
	pdata->gpio_scl = of_get_named_gpio_flags(np, "cypress,scl-gpio",
				0, &pdata->scl_gpio_flags);
	pdata->gpio_sda = of_get_named_gpio_flags(np, "cypress,sda-gpio",
				0, &pdata->sda_gpio_flags);
	pdata->gpio_int = of_get_named_gpio_flags(np, "cypress,irq-gpio",
				0, &pdata->irq_gpio_flags);
	pdata->i2c_gpio = of_property_read_bool(np, "cypress,i2c-gpio");

	cypress_request_gpio(client, pdata);

	return pdata;
}
#endif
static int i2c_touchkey_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct touchkey_platform_data *pdata = client->dev.platform_data;
	struct touchkey_i2c *tkey_i2c;
	bool bforced = false;
	struct input_dev *input_dev;

	int i;
	int ret = 0;

	if (lpcharge == 1) {
		dev_err(&client->dev, "%s : Do not load driver due to : lpm %d\n",
			 __func__, lpcharge);
		return -ENODEV;
	}

	if (!pdata) {
		pdata = cypress_parse_dt(client);
		if (!pdata) {
			dev_err(&client->dev, "%s: no pdata\n", __func__);
			return -EINVAL;
		}
	}

	/*Check I2C functionality */
	ret = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (ret == 0) {
		dev_err(&client->dev, "No I2C functionality found\n");
		return -ENODEV;
	}

	/*Obtain kernel memory space for touchkey i2c */
	tkey_i2c = kzalloc(sizeof(struct touchkey_i2c), GFP_KERNEL);
	if (NULL == tkey_i2c) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

#ifdef CONFIG_GLOVE_TOUCH
	tkey_i2c_global = tkey_i2c;
#endif

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_allocate_input_device;
	}

	client->irq = gpio_to_irq(pdata->gpio_int);

	input_dev->name = "sec_touchkey";
	input_dev->phys = "sec_touchkey/input0";
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = &client->dev;
	input_dev->open = touchkey_input_open;
	input_dev->close = touchkey_input_close;

	/*tkey_i2c*/
	tkey_i2c->pdata = pdata;
	tkey_i2c->input_dev = input_dev;
	tkey_i2c->client = client;
	tkey_i2c->irq = client->irq;
	tkey_i2c->name = "sec_touchkey";
	tkey_i2c->status_update = false;
	tkey_i2c->pdata->power_on = touchkey_power_on;
	tkey_i2c->pdata->led_power_on = touchkey_led_power_on;

	init_completion(&tkey_i2c->init_done);
	mutex_init(&tkey_i2c->lock);
#ifdef TK_USE_OPEN_DWORK
	INIT_DELAYED_WORK(&tkey_i2c->open_work, touchkey_open_work);
#endif
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_LED, input_dev->evbit);
	set_bit(LED_MISC, input_dev->ledbit);
	set_bit(EV_KEY, input_dev->evbit);
#ifdef CONFIG_VT_TKEY_SKIP_MATCH
	set_bit(EV_TOUCHKEY, input_dev->evbit);
#endif
	INIT_WORK(&tkey_i2c->update_work, touchkey_i2c_update_work);
	wake_lock_init(&tkey_i2c->fw_wakelock, WAKE_LOCK_SUSPEND, "touchkey");

	for (i = 1; i < touchkey_count; i++)
		set_bit(touchkey_keycode[i], input_dev->keybit);

	input_set_drvdata(input_dev, tkey_i2c);
	i2c_set_clientdata(client, tkey_i2c);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "Failed to register input device\n");
		goto err_register_device;
	}

	tkey_i2c->pdata->power_on(tkey_i2c, 1);
	msleep(50);

	tkey_i2c->enabled = true;

	tkey_i2c->fw_wq = create_singlethread_workqueue(client->name);
	if (!tkey_i2c->fw_wq) {
		dev_err(&client->dev, "fail to create workqueue for fw_wq\n");
		ret = -ENOMEM;
		goto err_create_fw_wq;
	}

	dev_err(&client->dev, "LCD type Print : 0x%06X\n", lcdtype);
	if (lcdtype == 0) {
		dev_err(&client->dev, "Device wasn't connected to board\n");
		goto err_i2c_check;
	}

	ret = touchkey_i2c_check(tkey_i2c);
	if (ret < 0) {
		dev_err(&client->dev, "i2c_check failed\n");
		bforced = true;
	}

	cypress_config_gpio_i2c(tkey_i2c, 1);

#if defined(CONFIG_GLOVE_TOUCH)
		mutex_init(&tkey_i2c->tsk_glove_lock);
		INIT_DELAYED_WORK(&tkey_i2c->glove_change_work, touchkey_glove_change_work);
		tkey_i2c->tsk_glove_mode_status = false;
#endif

	ret =
		request_threaded_irq(tkey_i2c->irq, NULL, touchkey_interrupt,
				IRQF_DISABLED | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, tkey_i2c->name, tkey_i2c);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to request irq(%d) - %d\n",
			tkey_i2c->irq, ret);
		goto err_request_threaded_irq;
	}

#if defined(TK_HAS_FIRMWARE_UPDATE)
/*tkey_firmupdate_retry_byreboot:
	touchkey_init_fw_name(tkey_i2c);*/
	ret = touchkey_fw_update(tkey_i2c, FW_BUILT_IN, bforced);
	if (ret < 0) {
		dev_err(&client->dev, "fw update fail\n");
		goto err_firmware_update;
	}
#endif

#if defined(TK_INFORM_CHARGER)
	tkey_i2c->callbacks.inform_charger = touchkey_ta_cb;
	if (tkey_i2c->pdata->register_cb) {
		dev_info(&client->dev, "Touchkey TA information\n");
		tkey_i2c->pdata->register_cb(&tkey_i2c->callbacks);
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	tkey_i2c->early_suspend.suspend =
		(void *)sec_touchkey_early_suspend;
	tkey_i2c->early_suspend.resume =
		(void *)sec_touchkey_late_resume;
	register_early_suspend(&tkey_i2c->early_suspend);
#endif

	/*sysfs*/
	tkey_i2c->dev = sec_device_create(tkey_i2c, "sec_touchkey");

	ret = IS_ERR(tkey_i2c->dev);
	if (ret) {
		dev_err(&client->dev, "Failed to create device(tkey_i2c->dev)!\n");
	} else {
		ret = sysfs_create_group(&tkey_i2c->dev->kobj,
					&touchkey_attr_group);
		if (ret)
			dev_err(&client->dev, "Failed to create sysfs group\n");
	}

/*	touchkey_stop(tkey_i2c); */
	complete_all(&tkey_i2c->init_done);
	touchkey_probe = true;

	return 0;

#if defined(TK_HAS_FIRMWARE_UPDATE)
err_firmware_update:
	tkey_i2c->pdata->led_power_on(0);
	disable_irq(tkey_i2c->irq);
	free_irq(tkey_i2c->irq, tkey_i2c);
#endif
err_request_threaded_irq:
	mutex_destroy(&tkey_i2c->tsk_glove_lock);
err_create_fw_wq:
err_i2c_check:
	destroy_workqueue(tkey_i2c->fw_wq);
	tkey_i2c->pdata->power_on(tkey_i2c, 0);
	input_unregister_device(input_dev);
	input_dev = NULL;
err_register_device:
	wake_lock_destroy(&tkey_i2c->fw_wakelock);
	mutex_destroy(&tkey_i2c->lock);
	input_free_device(input_dev);
err_allocate_input_device:
	kfree(tkey_i2c);
	return ret;
}

void touchkey_shutdown(struct i2c_client *client)
{
	struct touchkey_i2c *tkey_i2c = i2c_get_clientdata(client);

	if (!touchkey_probe)
		return;

	tkey_i2c->pdata->power_on(tkey_i2c, 0);
	tkey_i2c->pdata->led_power_on(0);

	printk(KERN_DEBUG"touchkey:%s\n", __func__);
}
#ifdef CONFIG_OF
static struct of_device_id cypress_touchkey_dt_ids[] = {
	{ .compatible = "cypress,cypress_touchkey" },
	{ }
};
#endif

struct i2c_driver touchkey_i2c_driver = {
	.driver = {
		.name = "sec_touchkey_driver",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &touchkey_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(cypress_touchkey_dt_ids),
#endif

	},
	.id_table = sec_touchkey_id,
	.probe = i2c_touchkey_probe,
	.shutdown = &touchkey_shutdown,
};

static int __init touchkey_init(void)
{
#ifdef TEST_JIG_MODE
	int ret;
#endif

	i2c_add_driver(&touchkey_i2c_driver);

#ifdef TEST_JIG_MODE
    ret = touchkey_enable_status_update(tkey_i2c);
	if (ret < 0)
		return ret;
#endif
	return 0;
}

static void __exit touchkey_exit(void)
{
	i2c_del_driver(&touchkey_i2c_driver);
	touchkey_probe = false;
}

late_initcall(touchkey_init);
module_exit(touchkey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("@@@");
MODULE_DESCRIPTION("touch keypad");
