/* driver/i2c/chip/rt5506.c
 *
 * Richtek Headphone Amp
 *
 * Copyright (C) 2010 HTC Corporation
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

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include "rt5506.h"
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/of_gpio.h>

#define AMP_ON_CMD_LEN 7
#define RETRY_CNT 5

#define DRIVER_NAME "RT5506"

enum AMP_REG_MODE {
	REG_PWM_MODE = 0,
	REG_AUTO_MODE,
};

struct headset_query {
	struct mutex mlock;
	struct mutex gpiolock;
	struct delayed_work hs_imp_detec_work;
	struct wake_lock hs_wake_lock;
	struct wake_lock gpio_wake_lock;
	enum HEADSET_QUERY_STATUS hs_qstatus;
	enum AMP_STATUS rt5506_status;
	enum HEADSET_OM headsetom;
	enum PLAYBACK_MODE curmode;
	enum AMP_GPIO_STATUS gpiostatus;
	enum AMP_REG_MODE regstatus;
	int action_on;
	int gpio_off_cancel;
	struct mutex actionlock;
	struct delayed_work volume_ramp_work;
	struct delayed_work gpio_off_work;
};

static struct i2c_client *this_client;
static struct rt5506_platform_data *pdata;
static int rt5506Connect;

struct rt5506_config_data rt5506_config_data;
static struct mutex hp_amp_lock;
static int rt5506_opened;
static int last_spkamp_state;
struct rt5506_config RT5506_AMP_ON = {7, {{0x0, 0xc0},
					{0x1, 0x1c}, {0x2, 0x00}, {0x7, 0x7f},
					{0x9, 0x1}, {0xa, 0x0},
					{0xb, 0xc7}, } };
struct rt5506_config RT5506_AMP_INIT = {11, {{0, 0xc0},
					{0x81, 0x30}, {0x87, 0xf6},
					{0x93, 0x8d}, {0x95, 0x7d},
					{0xa4, 0x52}, {0x96, 0xae},
					{0x97, 0x13}, {0x99, 0x35},
					{0x9b, 0x68}, {0x9d, 0x68}, } };

struct rt5506_config RT5506_AMP_MUTE = {1, {{0x1, 0xC7}, } };
struct rt5506_config RT5506_AMP_OFF = {1, {{0x0, 0x1}, } };

static int rt5506_write_reg(u8 reg, u8 val);
static int rt5506_i2c_read_addr(unsigned char *rxdata, unsigned char addr);
static int rt5506_i2c_write(struct rt5506_reg_data *txdata, int length);
static void set_amp(int on, struct rt5506_config *i2c_command);

struct headset_query rt5506_query;
static struct workqueue_struct *hs_wq;

static int high_imp;

static int rt5506_write_reg(u8 reg, u8 val)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[2];

	msg->addr = this_client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = data;
	data[0] = reg;
	data[1] = val;
	pr_info("%s: write reg 0x%x val 0x%x\n", __func__, data[0], data[1]);
	err = i2c_transfer(this_client->adapter, msg, 1);
	if (err >= 0)
		return 0;
	else {
		pr_info("%s: write error error %d\n", __func__, err);
		return err;
	}
}

static int rt5506_i2c_write(struct rt5506_reg_data *txdata, int length)
{
	int i, retry, pass = 0;
	char buf[2];
	struct i2c_msg msg[] = {
		{
			.addr = this_client->addr,
			.flags = 0,
			.len = 2,
			.buf = buf,
		},
	};
	for (i = 0; i < length; i++) {
		/*if (i == 2)  According to rt5506 Spec */
		/*	mdelay(1);*/
		buf[0] = txdata[i].addr;
		buf[1] = txdata[i].val;

		msg->buf = buf;
		retry = RETRY_CNT;
		pass = 0;
		while (retry--) {
			if (i2c_transfer(this_client->adapter, msg, 1) < 0) {
				pr_err("%s: I2C transfer error %d retry %d\n",
						__func__, i, retry);
				msleep(20);
			} else {
				pass = 1;
				break;
			}
		}
		if (pass == 0) {
			pr_err("I2C transfer error, retry fail\n");
			return -EIO;
		}
	}
	return 0;
}

static int rt5506_i2c_read_addr(unsigned char *rxdata, unsigned char addr)
{
	int rc;
	struct i2c_msg msgs[] = {
		{
			.addr = this_client->addr,
			.flags = 0,
			.len = 1,
			.buf = rxdata,
		},
		{
			.addr = this_client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = rxdata,
		},
	};

	if (!rxdata)
		return -1;

	*rxdata = addr;

	rc = i2c_transfer(this_client->adapter, msgs, 2);
	if (rc < 0) {
		pr_err("%s:[1] transfer error %d\n", __func__, rc);
		return rc;
	}

	pr_info("%s:i2c_read addr 0x%x value = 0x%x\n",
	__func__, addr, *rxdata);
	return 0;
}

static int rt5506_open(struct inode *inode, struct file *file)
{
	int rc = 0;

	mutex_lock(&hp_amp_lock);

	if (rt5506_opened) {
		pr_err("%s: busy\n", __func__);
		rc = -EBUSY;
		goto done;
	}
	rt5506_opened = 1;
done:
	mutex_unlock(&hp_amp_lock);
	return rc;
}

static int rt5506_release(struct inode *inode, struct file *file)
{
	mutex_lock(&hp_amp_lock);
	rt5506_opened = 0;
	mutex_unlock(&hp_amp_lock);

	return 0;
}

static void set_amp(int on, struct rt5506_config *i2c_command)
{
	pr_info("%s: %d\n", __func__, on);
	mutex_lock(&rt5506_query.mlock);
	mutex_lock(&hp_amp_lock);

	if (rt5506_query.hs_qstatus == QUERY_HEADSET)
		rt5506_query.hs_qstatus = QUERY_FINISH;

	if (on) {
		rt5506_query.rt5506_status = STATUS_PLAYBACK;
		if (rt5506_i2c_write(i2c_command->reg,
			i2c_command->reg_len) == 0) {
			last_spkamp_state = 1;
			pr_info("%s: ON\n", __func__);
		}

	} else {

		if (high_imp) {
			rt5506_write_reg(1, 0x7);
			rt5506_write_reg(0xb1, 0x81);
		} else {
			rt5506_write_reg(1, 0xc7);
		}

		if (rt5506_query.rt5506_status == STATUS_PLAYBACK) {
			last_spkamp_state = 0;
			pr_info("%s: OFF\n", __func__);
		}
		rt5506_query.rt5506_status = STATUS_OFF;
		rt5506_query.curmode = PLAYBACK_MODE_OFF;
	}
	mutex_unlock(&hp_amp_lock);
	mutex_unlock(&rt5506_query.mlock);
}

int query_rt5506(void)
{
	return rt5506Connect;
}

static long rt5506_ioctl(struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	int rc = 0;

	return rc;
}

static const struct file_operations rt5506_fops = {
	.owner = THIS_MODULE,
	.open = rt5506_open,
	.release = rt5506_release,
	.unlocked_ioctl = rt5506_ioctl,
};

static struct miscdevice rt5506_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "rt5501",
	.fops = &rt5506_fops,
};

int rt5506_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	unsigned char temp[2];
	pr_info("rt5506_probe");

	if (gpio_is_valid(138)) {
		pr_info("rt5506 set 138 on");
		ret = gpio_request(138, "rt5677");
		if (ret)
			pr_err("Fail gpio_request AUDIO_LDO1\n");

		ret = gpio_direction_output(138, 1);
		if (ret)
			pr_err("Fail gpio_direction AUDIO_LDO1\n");

		msleep(20);

	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c check functionality error\n", __func__);
		ret = -ENODEV;
		goto err_alloc_data_failed;
	}
	pr_info("rt5506 %s %d\n", __func__, __LINE__);
	pr_info("i2c check functionality PASS");

	if (pdata == NULL) {
		pr_info("rt5506 %s %d\n", __func__, __LINE__);
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL) {
			ret = -ENOMEM;
			pr_err("%s: platform data is NULL\n", __func__);
			goto err_alloc_data_failed;
		}
	}

	this_client = client;

	pr_info("rt5506 %s %d\n", __func__, __LINE__);

		mdelay(10);
		pr_info("%s:[2]current gpio %d value %d\n", __func__,
		pdata->gpio_rt5506_enable,
		gpio_get_value(pdata->gpio_rt5506_enable));
		rt5506_write_reg(0, 0x04);
		mdelay(5);
		rt5506_write_reg(0x0, 0xc0);
		rt5506_write_reg(0x81, 0x30);
		/*rt5506_write_reg(0x87, 0xf6);*/
		rt5506_write_reg(0x90, 0xd0);
		rt5506_write_reg(0x93, 0x9d);
		rt5506_write_reg(0x95, 0x7b);
		rt5506_write_reg(0xa4, 0x52);
		/*rt5506_write_reg(0x96, 0xae);*/
		rt5506_write_reg(0x97, 0x00);
		rt5506_write_reg(0x98, 0x22);
		rt5506_write_reg(0x99, 0x33);
		rt5506_write_reg(0x9a, 0x55);
		rt5506_write_reg(0x9b, 0x66);
		rt5506_write_reg(0x9c, 0x99);
		rt5506_write_reg(0x9d, 0x66);
		rt5506_write_reg(0x9e, 0x99);
		rt5506_write_reg(0x1, 0xc7);
		mdelay(10);
		ret = rt5506_i2c_read_addr(temp, 0x1);
		if (ret < 0) {
			pr_info("rt5506 is not connected\n");
			rt5506Connect = 0;
		} else {
			pr_info("rt5506 is connected\n");
			rt5506Connect = 1;
		}
		rt5506Connect = 1;

	if (rt5506Connect) {
		/*htc_acoustic_register_hs_amp(set_rt5506_amp,&rt5506_fops);*/
		ret = misc_register(&rt5506_device);
		if (ret) {
			pr_err("%s: rt5506_device register failed\n", __func__);
			goto err_free_allocated_mem;
		}

		set_amp(1, &RT5506_AMP_ON);
		pr_info("rt5506 %s %d\n", __func__, __LINE__);
	}
	return 0;

err_free_allocated_mem:
	kfree(pdata);
	pr_info("rt5506 %s %d\n", __func__, __LINE__);
err_alloc_data_failed:
	rt5506Connect = 0;
	pr_info("rt5506 %s %d\n", __func__, __LINE__);
	return ret;
}

static int rt5506_remove(struct i2c_client *client)
{
	struct rt5506_platform_data *p5501data = i2c_get_clientdata(client);
	pr_info("%s:\n", __func__);

	kfree(p5501data);

	if (rt5506Connect) {
		misc_deregister(&rt5506_device);
		cancel_delayed_work_sync(&rt5506_query.hs_imp_detec_work);
		destroy_workqueue(hs_wq);
	}
	return 0;
}

static void rt5506_shutdown(struct i2c_client *client)
{
}

static int rt5506_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int rt5506_resume(struct i2c_client *client)
{
	return 0;
}

static struct of_device_id rt5506_match_table[] = {
	{ .compatible = "richtek,rt5506-amp",},
	{ },
};

static const struct i2c_device_id rt5506_id[] = {
	{ RT5506_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver rt5506_driver = {
	.probe = rt5506_probe,
	.remove = rt5506_remove,
	.shutdown = rt5506_shutdown,
	.suspend = rt5506_suspend,
	.resume = rt5506_resume,
	.id_table = rt5506_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = RT5506_I2C_NAME,
		.of_match_table = rt5506_match_table,
	},
};

static int __init rt5506_init(void)
{
	pr_info("%s\n", __func__);
	mutex_init(&hp_amp_lock);
	mutex_init(&rt5506_query.mlock);
	mutex_init(&rt5506_query.gpiolock);
	mutex_init(&rt5506_query.actionlock);
	rt5506_query.rt5506_status = STATUS_OFF;
	rt5506_query.hs_qstatus = QUERY_OFF;
	rt5506_query.headsetom = HEADSET_8OM;
	rt5506_query.curmode = PLAYBACK_MODE_OFF;
	rt5506_query.gpiostatus = AMP_GPIO_OFF;
	rt5506_query.regstatus = REG_AUTO_MODE;
	return i2c_add_driver(&rt5506_driver);
}

static void __exit rt5506_exit(void)
{
	i2c_del_driver(&rt5506_driver);
}

module_init(rt5506_init);
module_exit(rt5506_exit);

MODULE_DESCRIPTION("rt5506 Headphone Amp driver");
MODULE_LICENSE("GPL");
