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
#include <linux/regulator/consumer.h>
#include <linux/htc_headset_mgr.h>

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
	int gpio_off_cancel;
	struct mutex actionlock;
	struct delayed_work volume_ramp_work;
	struct delayed_work gpio_off_work;
};

static struct i2c_client *this_client;
static struct rt5506_platform_data *pdata;
static int rt5506Connect;

struct rt5506_config_data rt5506_cfg_data;
static struct mutex hp_amp_lock;
static int rt5506_opened;

struct rt5506_config RT5506_AMP_ON = {7, {{0x0, 0xc0}, {0x1, 0x1c},
{0x2, 0x00}, {0x7, 0x7f}, {0x9, 0x1}, {0xa, 0x0}, {0xb, 0xc7} } };
struct rt5506_config RT5506_AMP_INIT = {11, {{0, 0xc0}, {0x81, 0x30},
{0x87, 0xf6}, {0x93, 0x8d}, {0x95, 0x7d}, {0xa4, 0x52}, {0x96, 0xae},
{0x97, 0x13}, {0x99, 0x35}, {0x9b, 0x68}, {0x9d, 0x68} } };
struct rt5506_config RT5506_AMP_MUTE = {1, {{0x1, 0xC7 } } };
struct rt5506_config RT5506_AMP_OFF = {1, {{0x0, 0x1 } } };

static int rt5506_valid_registers[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5,
0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC0, 0x81, 0x87, 0x90, 0x93, 0x95,
0xA4, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E};

static int rt5506_write_reg(u8 reg, u8 val);
static void hs_imp_detec_func(struct work_struct *work);
static int rt5506_i2c_read_addr(unsigned char *rxdata, unsigned char addr);
static int rt5506_i2c_write(struct rt5506_reg_data *txdata, int length);
static void set_amp(int on, struct rt5506_config *i2c_command);

struct headset_query rt5506_query;
static struct workqueue_struct *hs_wq;
static struct workqueue_struct *ramp_wq;
static struct workqueue_struct *gpio_wq;
static int high_imp;


int rt5506_headset_detect(int on)
{
	if (!rt5506Connect)
		return 0;

	if (on) {
		pr_info("%s: headset in ++\n", __func__);
		cancel_delayed_work_sync(&rt5506_query.hs_imp_detec_work);
		mutex_lock(&rt5506_query.gpiolock);
		mutex_lock(&rt5506_query.mlock);
		rt5506_query.hs_qstatus = QUERY_HEADSET;
		rt5506_query.headsetom = HEADSET_OM_UNDER_DETECT;

		if (rt5506_query.rt5506_status == STATUS_PLAYBACK) {
			/* AMP off */
			if (high_imp) {
				rt5506_write_reg(1, 0x7);
				rt5506_write_reg(0xb1, 0x81);
			} else {
				rt5506_write_reg(1, 0xc7);
			}

			pr_info("%s: OFF\n", __func__);

			rt5506_query.rt5506_status = STATUS_SUSPEND;
		}
		pr_info("%s: headset in --\n", __func__);
		mutex_unlock(&rt5506_query.mlock);
		mutex_unlock(&rt5506_query.gpiolock);
		queue_delayed_work(hs_wq,
		&rt5506_query.hs_imp_detec_work, msecs_to_jiffies(5));
		pr_info("%s: headset in --2\n", __func__);
	} else {
		pr_info("%s: headset remove ++\n", __func__);
		cancel_delayed_work_sync(&rt5506_query.hs_imp_detec_work);
		flush_work(&rt5506_query.volume_ramp_work.work);
		mutex_lock(&rt5506_query.gpiolock);
		mutex_lock(&rt5506_query.mlock);
		rt5506_query.hs_qstatus = QUERY_OFF;
		rt5506_query.headsetom = HEADSET_OM_UNDER_DETECT;

		if (rt5506_query.rt5506_status == STATUS_PLAYBACK) {
			/* AMP off */
			if (high_imp) {
				rt5506_write_reg(1, 0x7);
				rt5506_write_reg(0xb1, 0x81);
			} else {
				rt5506_write_reg(1, 0xc7);
			}

			pr_info("%s: OFF\n", __func__);

			rt5506_query.rt5506_status = STATUS_SUSPEND;
		}
		if (high_imp) {
			int closegpio = 0;

			if (rt5506_query.gpiostatus == AMP_GPIO_OFF) {
				pr_info("%s: enable gpio %d\n", __func__,
				pdata->rt5506_enable);
				gpio_set_value(pdata->rt5506_enable, 1);
				closegpio = 1;
				usleep_range(1000, 2000);
			}

			pr_info("%s: reset rt5506\n", __func__);
			rt5506_write_reg(0x0, 0x4);
			mdelay(1);
			rt5506_write_reg(0x1, 0xc7);
			high_imp = 0;

			if (closegpio) {
				pr_info("%s: disable gpio %d\n",
				__func__, pdata->rt5506_enable);
				gpio_set_value(pdata->rt5506_enable, 0);
			}
		}
		rt5506_query.curmode = PLAYBACK_MODE_OFF;
		pr_info("%s: headset remove --1\n", __func__);

		mutex_unlock(&rt5506_query.mlock);
		mutex_unlock(&rt5506_query.gpiolock);

		pr_info("%s: headset remove --2\n", __func__);
	}
	return 0;
}

void rt5506_set_gain(u8 data)
{
	pr_info("%s:before addr=%d, val=%d\n", __func__,
	RT5506_AMP_ON.reg[1].addr, RT5506_AMP_ON.reg[1].val);

	RT5506_AMP_ON.reg[1].val = data;

	pr_info("%s:after addr=%d, val=%d\n", __func__,
	RT5506_AMP_ON.reg[1].addr, RT5506_AMP_ON.reg[1].val);
}

u8 rt5506_get_gain(void)
{
	return RT5506_AMP_ON.reg[1].val;
}

static void rt5506_register_hs_notification(void)
{
	struct headset_notifier notifier;
	notifier.id = HEADSET_REG_HS_INSERT;
	notifier.func = rt5506_headset_detect;
	headset_notifier_register(&notifier);
}

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
#ifdef DEBUG
	pr_info("%s: write reg 0x%x val 0x%x\n", __func__, data[0], data[1]);
#endif
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
		buf[0] = txdata[i].addr;
		buf[1] = txdata[i].val;
#ifdef DEBUG
		pr_info("%s:i2c_write addr 0x%x val 0x%x\n", __func__,
		buf[0], buf[1]);
#endif
		msg->buf = buf;
		retry = RETRY_CNT;
		pass = 0;
		while (retry--) {
			if (i2c_transfer(this_client->adapter, msg, 1) < 0) {
				pr_err("%s: I2C transfer error %d retry %d\n",
				__func__, i, retry);
				usleep_range(20000, 21000);
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

static void hs_imp_gpio_off(struct work_struct *work)
{
	u64 timeout = get_jiffies_64() + 3*HZ;
	wake_lock(&rt5506_query.gpio_wake_lock);

	while (1) {
		if (time_after64(get_jiffies_64(), timeout))
			break;
		else if (rt5506_query.gpio_off_cancel) {
			wake_unlock(&rt5506_query.gpio_wake_lock);
			return;
		} else
			usleep_range(10000, 11000);
	}

	mutex_lock(&rt5506_query.gpiolock);
	pr_info("%s: disable gpio %d\n", __func__, pdata->rt5506_enable);
	gpio_set_value(pdata->rt5506_enable, 0);
	rt5506_query.gpiostatus = AMP_GPIO_OFF;
	mutex_unlock(&rt5506_query.gpiolock);
	wake_unlock(&rt5506_query.gpio_wake_lock);
}

static void hs_imp_detec_func(struct work_struct *work)
{
	struct headset_query *hs;
	unsigned char temp;
	unsigned char r_channel;
	int ret;
	pr_info("%s: read rt5506 hs imp\n", __func__);

	hs = container_of(work, struct headset_query, hs_imp_detec_work.work);
	wake_lock(&hs->hs_wake_lock);

	rt5506_query.gpio_off_cancel = 1;
	cancel_delayed_work_sync(&rt5506_query.gpio_off_work);
	mutex_lock(&hs->gpiolock);
	mutex_lock(&hs->mlock);

	if (hs->hs_qstatus != QUERY_HEADSET) {
		mutex_unlock(&hs->mlock);
		mutex_unlock(&hs->gpiolock);
		wake_unlock(&hs->hs_wake_lock);
		return;
	}

	if (hs->gpiostatus == AMP_GPIO_OFF) {
		pr_info("%s: enable gpio %d\n", __func__,
		pdata->rt5506_enable);
		gpio_set_value(pdata->rt5506_enable, 1);
		rt5506_query.gpiostatus = AMP_GPIO_ON;
	}

	usleep_range(1000, 2000);

	/*sense impedance start*/
	rt5506_write_reg(0, 0x04);
	rt5506_write_reg(0xa4, 0x52);
	rt5506_write_reg(1, 0x7);
	usleep_range(10000, 11000);
	rt5506_write_reg(0x3, 0x81);
	msleep(100);
	/*sense impedance end*/

	ret = rt5506_i2c_read_addr(&temp, 0x4);
	if (ret < 0) {
		pr_err("%s: read rt5506 status error %d\n", __func__, ret);
		if (hs->gpiostatus == AMP_GPIO_ON) {
			rt5506_query.gpio_off_cancel = 0;
			queue_delayed_work(gpio_wq,
			&rt5506_query.gpio_off_work, msecs_to_jiffies(0));
		}
		mutex_unlock(&hs->mlock);
		mutex_unlock(&hs->gpiolock);
		wake_unlock(&hs->hs_wake_lock);
		return;
	}

	/*identify stereo or mono headset*/
	rt5506_i2c_read_addr(&r_channel, 0x6);

	/* init start*/
	rt5506_write_reg(0x0, 0x4);
	mdelay(1);
	rt5506_write_reg(0x0, 0xc0);
	rt5506_write_reg(0x81, 0x30);
	rt5506_write_reg(0x90, 0xd0);
	rt5506_write_reg(0x93, 0x9d);
	rt5506_write_reg(0x95, 0x7b);
	rt5506_write_reg(0xa4, 0x52);
	rt5506_write_reg(0x97, 0x00);
	rt5506_write_reg(0x98, 0x22);
	rt5506_write_reg(0x99, 0x33);
	rt5506_write_reg(0x9a, 0x55);
	rt5506_write_reg(0x9b, 0x66);
	rt5506_write_reg(0x9c, 0x99);
	rt5506_write_reg(0x9d, 0x66);
	rt5506_write_reg(0x9e, 0x99);
	/* init end*/

	high_imp = 0;

	if (temp & AMP_SENSE_READY) {
		unsigned char om, hsmode;
		enum HEADSET_OM hsom;

		hsmode = (temp & 0x60) >> 5;
		om = (temp & 0xe) >> 1;

		if (r_channel == 0) {
			/*mono headset*/
			hsom = HEADSET_MONO;
		} else {
			switch (om) {
			case 0:
				hsom = HEADSET_8OM;
				break;
			case 1:
				hsom = HEADSET_16OM;
				break;
			case 2:
				hsom = HEADSET_32OM;
				break;
			case 3:
				hsom = HEADSET_64OM;
				break;
			case 4:
				hsom = HEADSET_128OM;
				break;
			case 5:
				hsom = HEADSET_256OM;
				break;
			case 6:
				hsom = HEADSET_500OM;
				break;
			case 7:
				hsom = HEADSET_1KOM;
				break;

			default:
				hsom = HEADSET_OM_UNDER_DETECT;
				break;
			}
		}

		hs->hs_qstatus = QUERY_FINISH;
		hs->headsetom = hsom;

		if (om >= HEADSET_256OM && om <= HEADSET_1KOM)
			high_imp = 1;

		pr_info("rt5506 hs imp value 0x%x hsmode %d om 0x%x hsom %d high_imp %d\n",
		temp & 0xf, hsmode, om, hsom, high_imp);

	} else {
		if (hs->hs_qstatus == QUERY_HEADSET)
			queue_delayed_work(hs_wq,
			&rt5506_query.hs_imp_detec_work, QUERY_LATTER);
	}

	if (high_imp) {
		rt5506_write_reg(0xb1, 0x81);
		rt5506_write_reg(0x80, 0x87);
		rt5506_write_reg(0x83, 0xc3);
		rt5506_write_reg(0x84, 0x63);
		rt5506_write_reg(0x89, 0x7);
		mdelay(9);
		rt5506_write_reg(0x83, 0xcf);
		rt5506_write_reg(0x89, 0x1d);
		mdelay(1);
		rt5506_write_reg(1, 0x7);
		rt5506_write_reg(0xb1, 0x81);
	} else {
		rt5506_write_reg(1, 0xc7);
	}

	if (hs->gpiostatus == AMP_GPIO_ON) {
		rt5506_query.gpio_off_cancel = 0;
		queue_delayed_work(gpio_wq,
		&rt5506_query.gpio_off_work, msecs_to_jiffies(0));
	}

	mutex_unlock(&hs->mlock);
	mutex_unlock(&hs->gpiolock);

	if (hs->rt5506_status == STATUS_SUSPEND)
		set_rt5506_amp(1, 0);

	wake_unlock(&hs->hs_wake_lock);
}

static void volume_ramp_func(struct work_struct *work)
{
	pr_info("%s\n", __func__);
	if (rt5506_query.rt5506_status != STATUS_PLAYBACK) {
		pr_info("%s:Not in STATUS_PLAYBACK\n", __func__);
		mdelay(1);
		/*start state machine and disable noise gate */
		if (high_imp)
			rt5506_write_reg(0xb1, 0x80);

		rt5506_write_reg(0x2, 0x0);
		mdelay(1);
	}
	set_amp(1, &RT5506_AMP_ON);
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
			pr_info("%s: ON\n", __func__);
		}
	} else {
		if (high_imp) {
			rt5506_write_reg(1, 0x7);
			rt5506_write_reg(0xb1, 0x81);
		} else {
			rt5506_write_reg(1, 0xc7);
		}
		if (rt5506_query.rt5506_status == STATUS_PLAYBACK)
			pr_info("%s: OFF\n", __func__);
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

int rt5506_dump_reg(void)
{
	int ret = 0, rt5506_already_enabled = 0, i = 0;
	unsigned char temp[2];

	pr_info("%s:current gpio %d value %d\n", __func__,
	pdata->rt5506_enable, gpio_get_value(pdata->rt5506_enable));

	if (gpio_get_value(pdata->rt5506_enable) == 1) {
		rt5506_already_enabled = 1;
	} else {
		ret = gpio_direction_output(pdata->rt5506_enable, 1);
		if (ret) {
			pr_err("%s: Fail set rt5506-enable to 1, ret=%d\n",
			__func__, ret);
			return ret;
		} else {
			pr_info("%s: rt5506-enable=1\n", __func__);
		}
	}

	mdelay(1);
	for (i = 0; i < sizeof(rt5506_valid_registers)/
		sizeof(rt5506_valid_registers[0]); i++) {
		ret = rt5506_i2c_read_addr(temp, rt5506_valid_registers[i]);
		if (ret < 0) {
				pr_err("%s: rt5506_i2c_read_addr(%x) fail\n",
				__func__, rt5506_valid_registers[i]);
				break;
		}
	}

	if (rt5506_already_enabled == 0)
		gpio_direction_output(pdata->rt5506_enable, 0);

	return ret;
}

int set_rt5506_amp(int on, int dsp)
{
	if (!rt5506Connect)
		return 0;

	pr_info("%s: %d\n", __func__, on);
	mutex_lock(&rt5506_query.actionlock);
	rt5506_query.gpio_off_cancel = 1;

	cancel_delayed_work_sync(&rt5506_query.gpio_off_work);
	cancel_delayed_work_sync(&rt5506_query.volume_ramp_work);
	mutex_lock(&rt5506_query.gpiolock);

	if (on) {
		if (rt5506_query.gpiostatus == AMP_GPIO_OFF) {
			pr_info("%s: enable gpio %d\n", __func__,
			pdata->rt5506_enable);
			gpio_set_value(pdata->rt5506_enable, 1);
			rt5506_query.gpiostatus = AMP_GPIO_ON;
			usleep_range(1000, 2000);
		}
		queue_delayed_work(ramp_wq,
		&rt5506_query.volume_ramp_work, msecs_to_jiffies(0));
	} else {
		set_amp(0, &RT5506_AMP_ON);
		if (rt5506_query.gpiostatus == AMP_GPIO_ON) {
			rt5506_query.gpio_off_cancel = 0;
			queue_delayed_work(gpio_wq,
			&rt5506_query.gpio_off_work, msecs_to_jiffies(0));
		}
	}

	mutex_unlock(&rt5506_query.gpiolock);
	mutex_unlock(&rt5506_query.actionlock);

	return 0;
}

static int update_amp_parameter(int mode)
{
	if (mode >= rt5506_cfg_data.mode_num)
		return -EINVAL;

	pr_info("%s: set mode %d\n", __func__, mode);

	if (mode == PLAYBACK_MODE_OFF) {
		memcpy(&RT5506_AMP_OFF,
		&rt5506_cfg_data.cmd_data[mode].config,
		sizeof(struct rt5506_config));
	} else if (mode == AMP_INIT) {
		memcpy(&RT5506_AMP_INIT,
		&rt5506_cfg_data.cmd_data[mode].config,
		sizeof(struct rt5506_config));
	} else if (mode == AMP_MUTE) {
		memcpy(&RT5506_AMP_MUTE,
		&rt5506_cfg_data.cmd_data[mode].config,
		sizeof(struct rt5506_config));
	} else {
		memcpy(&RT5506_AMP_ON,
		&rt5506_cfg_data.cmd_data[mode].config,
		sizeof(struct rt5506_config));
	}
	return 0;
}


static long rt5506_ioctl(struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int rc = 0, modeid = 0;
	int premode = 0;

	switch (cmd) {
	case AMP_SET_MODE:
		if (copy_from_user(&modeid, argp, sizeof(modeid)))
			return -EFAULT;

		if (!rt5506_cfg_data.cmd_data) {
			pr_err("%s: out of memory\n", __func__);
			return -ENOMEM;
		}

		if (modeid >= rt5506_cfg_data.mode_num || modeid < 0) {
			pr_err("unsupported rt5506 mode %d\n", modeid);
			return -EINVAL;
		}
		mutex_lock(&hp_amp_lock);
		premode = rt5506_query.curmode;
		rt5506_query.curmode = modeid;
		rc = update_amp_parameter(modeid);
		mutex_unlock(&hp_amp_lock);

		pr_info("%s:set rt5506 mode to %d curstatus %d\n",
		__func__, modeid, rt5506_query.rt5506_status);

		mutex_lock(&rt5506_query.actionlock);

		if (rt5506_query.rt5506_status == STATUS_PLAYBACK
			&& premode != rt5506_query.curmode) {
			flush_work(&rt5506_query.volume_ramp_work.work);
			queue_delayed_work(ramp_wq,
			&rt5506_query.volume_ramp_work, msecs_to_jiffies(280));
		}
		mutex_unlock(&rt5506_query.actionlock);
		break;
	case AMP_SET_PARAM:
		mutex_lock(&hp_amp_lock);
		if (copy_from_user(&rt5506_cfg_data.mode_num,
			argp, sizeof(unsigned int))) {
			pr_err("%s: copy from user failed.\n", __func__);
			mutex_unlock(&hp_amp_lock);
			return -EFAULT;
		}

		if (rt5506_cfg_data.mode_num <= 0) {
			pr_err("%s: invalid mode number %d\n",
					__func__, rt5506_cfg_data.mode_num);
			mutex_unlock(&hp_amp_lock);
			return -EINVAL;
		}
		if (rt5506_cfg_data.cmd_data == NULL)
			rt5506_cfg_data.cmd_data = kzalloc(
			sizeof(struct rt5506_comm_data)
			*rt5506_cfg_data.mode_num, GFP_KERNEL);

		if (!rt5506_cfg_data.cmd_data) {
			pr_err("%s: out of memory\n", __func__);
			mutex_unlock(&hp_amp_lock);
			return -ENOMEM;
		}

		if (copy_from_user(rt5506_cfg_data.cmd_data,
			((struct rt5506_config_data *)argp)->cmd_data,
			sizeof(struct rt5506_comm_data)
			* rt5506_cfg_data.mode_num)) {
			pr_err("%s: copy data from user failed.\n", __func__);
			kfree(rt5506_cfg_data.cmd_data);
			rt5506_cfg_data.cmd_data = NULL;
			mutex_unlock(&hp_amp_lock);
			return -EFAULT;
		}

		pr_info("%s: update rt5506 i2c commands #%d success.\n",
		__func__, rt5506_cfg_data.mode_num);
		/* update default paramater from csv*/
		update_amp_parameter(PLAYBACK_MODE_OFF);
		update_amp_parameter(AMP_MUTE);
		update_amp_parameter(AMP_INIT);
		mutex_unlock(&hp_amp_lock);
		rc = 0;
		break;
	case AMP_QUERY_OM:
		mutex_lock(&rt5506_query.mlock);
		rc = rt5506_query.headsetom;
		mutex_unlock(&rt5506_query.mlock);
		pr_info("%s: query headset om %d\n", __func__, rc);

		if (copy_to_user(argp, &rc, sizeof(rc)))
			rc = -EFAULT;
		else
			rc = 0;
		break;
	default:
		pr_err("%s: Invalid command\n", __func__);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static void rt5506_parse_pfdata(struct device *dev,
struct rt5506_platform_data *ppdata)
{
	struct device_node *dt = dev->of_node;
	enum of_gpio_flags flags;

	pdata->rt5506_enable = -EINVAL;
	pdata->rt5506_power_enable = -EINVAL;


	if (dt) {
		pr_info("%s: dt is used\n", __func__);
		pdata->rt5506_enable =
		of_get_named_gpio_flags(dt, "richtek,enable-gpio", 0, &flags);

		pdata->rt5506_power_enable =
		of_get_named_gpio_flags(dt,
		"richtek,enable-power-gpio", 0, &flags);
	} else {
		pr_info("%s: dt is NOT used\n", __func__);
		if (dev->platform_data) {
			pdata->rt5506_enable =
			((struct rt5506_platform_data *)
			dev->platform_data)->rt5506_enable;

			pdata->rt5506_power_enable =
			((struct rt5506_platform_data *)
			dev->platform_data)->rt5506_power_enable;
		}
	}

	if (gpio_is_valid(pdata->rt5506_enable)) {
		pr_info("%s: gpio_rt5506_enable %d\n",
		__func__, pdata->rt5506_enable);
	} else {
		pr_err("%s: gpio_rt5506_enable %d is invalid\n",
		__func__, pdata->rt5506_enable);
	}

	if (gpio_is_valid(pdata->rt5506_power_enable)) {
		pr_info("%s: rt5506_power_enable %d\n",
		__func__, pdata->rt5506_power_enable);
	} else {
		pr_err("%s: rt5506_power_enable %d is invalid\n",
		__func__, pdata->rt5506_power_enable);
	}
}

static const struct file_operations rt5506_fops = {
	.owner = THIS_MODULE,
	.open = rt5506_open,
	.release = rt5506_release,
	.unlocked_ioctl = rt5506_ioctl,
};

static struct miscdevice rt5506_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "rt5506",
	.fops = &rt5506_fops,
};

int rt5506_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	unsigned char temp[2];

	struct regulator *rt5506_reg;

	pr_info("%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c check functionality error\n", __func__);
		ret = -ENODEV;
		goto err_alloc_data_failed;
	}

	if (pdata == NULL) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL) {
			ret = -ENOMEM;
			pr_err("%s: platform data is NULL\n", __func__);
			goto err_alloc_data_failed;
		}
	}

	rt5506_parse_pfdata(&client->dev, pdata);

	this_client = client;

	if (!gpio_is_valid(pdata->rt5506_power_enable)) {
		rt5506_reg = regulator_get(&client->dev, "ldoen");
		if (IS_ERR(rt5506_reg)) {
			pr_err("%s: Fail regulator_get ldoen\n", __func__);
			goto err_free_allocated_mem;
		} else {
			ret = regulator_enable(rt5506_reg);
			if (ret) {
				pr_err("%s: Fail regulator_enable ldoen, %d\n",
				__func__, ret);
				goto err_free_allocated_mem;
			} else
				pr_info("rt5506_reg ldoen is enabled\n");
		}
	} else {
		ret = gpio_request(pdata->rt5506_power_enable,
		"rt5506-power-en");
		if (ret) {
			pr_err("%s: Fail gpio_request rt5506_power_enable, %d\n",
			__func__, ret);
			goto err_free_allocated_mem;
		} else {
			ret = gpio_direction_output(pdata->rt5506_power_enable,
			1);

			if (ret) {
				pr_err("%s: Fail set rt5506_power_enable to 1, ret=%d\n",
				__func__, ret);
				gpio_free(pdata->rt5506_power_enable);
				goto err_free_allocated_mem;
			} else {
				pr_info("rt5506_power_enable=1\n");
				gpio_free(pdata->rt5506_power_enable);
			}
		}
	}
	mdelay(10);

	if (gpio_is_valid(pdata->rt5506_enable)) {
		ret = gpio_request(pdata->rt5506_enable, "rt5506-enable");
		if (ret) {
			pr_err("%s: Fail gpio_request rt5506-enable, %d\n",
			__func__, ret);
			goto err_free_allocated_mem;
		} else {
			ret = gpio_direction_output(pdata->rt5506_enable, 1);
			if (ret) {
				pr_err("%s: Fail set rt5506-enable to 1, ret=%d\n",
				__func__, ret);
				goto err_free_allocated_mem;
			} else {
				pr_info("%s: rt5506-enable=1\n", __func__);
			}
		}
	} else {
		pr_err("%s: rt5506_enable is invalid\n", __func__);
		goto err_free_allocated_mem;
	}

	mdelay(1);

	pr_info("%s:current gpio %d value %d\n", __func__,
	pdata->rt5506_enable, gpio_get_value(pdata->rt5506_enable));

	/*init start*/
	rt5506_write_reg(0, 0x04);
	mdelay(1);
	rt5506_write_reg(0x0, 0xc0);
	rt5506_write_reg(0x81, 0x30);
	rt5506_write_reg(0x90, 0xd0);
	rt5506_write_reg(0x93, 0x9d);
	rt5506_write_reg(0x95, 0x7b);
	rt5506_write_reg(0xa4, 0x52);
	rt5506_write_reg(0x97, 0x00);
	rt5506_write_reg(0x98, 0x22);
	rt5506_write_reg(0x99, 0x33);
	rt5506_write_reg(0x9a, 0x55);
	rt5506_write_reg(0x9b, 0x66);
	rt5506_write_reg(0x9c, 0x99);
	rt5506_write_reg(0x9d, 0x66);
	rt5506_write_reg(0x9e, 0x99);
	/*init end*/

	rt5506_write_reg(0x1, 0xc7);
	mdelay(10);

	ret = rt5506_i2c_read_addr(temp, 0x1);
	if (ret < 0) {
		pr_err("%s: rt5506 is not connected\n", __func__);
		rt5506Connect = 0;
	} else {
		pr_info("%s: rt5506 is connected\n", __func__);
		rt5506Connect = 1;
	}

	ret = gpio_direction_output(pdata->rt5506_enable, 0);
	if (ret) {
		pr_err("%s: Fail set rt5506-enable to 0, ret=%d\n", __func__,
		ret);
		gpio_free(pdata->rt5506_enable);
		goto err_free_allocated_mem;
	} else {
		pr_info("%s: rt5506-enable=0\n", __func__);
		/*gpio_free(pdata->rt5506_enable);*/
	}


	if (rt5506Connect) {
		/*htc_acoustic_register_hs_amp(set_rt5506_amp,&rt5506_fops);*/
		ret = misc_register(&rt5506_device);
		if (ret) {
			pr_err("%s: rt5506_device register failed\n", __func__);
			goto err_free_allocated_mem;
		} else {
			pr_info("%s: rt5506 is misc_registered\n", __func__);
		}

		hs_wq = create_workqueue("rt5506_hsdetect");
		INIT_DELAYED_WORK(&rt5506_query.hs_imp_detec_work,
		hs_imp_detec_func);

		wake_lock_init(&rt5506_query.hs_wake_lock,
		WAKE_LOCK_SUSPEND, "rt5506 hs wakelock");

		wake_lock_init(&rt5506_query.gpio_wake_lock,
		WAKE_LOCK_SUSPEND, "rt5506 gpio wakelock");

		ramp_wq = create_workqueue("rt5506_volume_ramp");
		INIT_DELAYED_WORK(&rt5506_query.volume_ramp_work,
		volume_ramp_func);

		gpio_wq = create_workqueue("rt5506_gpio_off");
		INIT_DELAYED_WORK(&rt5506_query.gpio_off_work, hs_imp_gpio_off);

		rt5506_register_hs_notification();
	}
	return 0;

err_free_allocated_mem:
	kfree(pdata);
err_alloc_data_failed:
	rt5506Connect = 0;
	return ret;
}

static int rt5506_remove(struct i2c_client *client)
{
	struct rt5506_platform_data *p5506data = i2c_get_clientdata(client);
	struct regulator *rt5506_reg;
	int ret = 0;
	pr_info("%s:\n", __func__);

	if (gpio_is_valid(pdata->rt5506_enable)) {
		ret = gpio_request(pdata->rt5506_enable, "rt5506-enable");
		if (ret) {
			pr_err("%s: Fail gpio_request rt5506-enable, %d\n",
			__func__, ret);
		} else {
			ret = gpio_direction_output(pdata->rt5506_enable, 0);
			if (ret) {
				pr_err("%s: Fail set rt5506-enable to 0, ret=%d\n",
				__func__, ret);
			} else {
				pr_info("%s: rt5506-enable=0\n", __func__);
			}
			gpio_free(pdata->rt5506_enable);
		}
	}

	mdelay(1);

	if (!gpio_is_valid(pdata->rt5506_power_enable)) {
		rt5506_reg = regulator_get(&client->dev, "ldoen");
		if (IS_ERR(rt5506_reg)) {
			pr_err("%s: Fail regulator_get ldoen\n", __func__);
		} else {
			ret = regulator_disable(rt5506_reg);
			if (ret) {
				pr_err("%s: Fail regulator_disable ldoen, %d\n",
				__func__, ret);
			} else
				pr_info("rt5506_reg ldoen is disabled\n");
		}
	} else {
		ret = gpio_request(pdata->rt5506_power_enable,
		"rt5506-power-en");
		if (ret) {
			pr_err("%s: Fail gpio_request rt5506_power_enable, %d\n",
			__func__, ret);
		} else {
			ret = gpio_direction_output(pdata->rt5506_power_enable,
			0);
			if (ret) {
				pr_err("%s: Fail set rt5506_power_enable to 0, ret=%d\n",
				__func__, ret);
			} else {
				pr_info("rt5506_power_enable=0\n");
			}
			gpio_free(pdata->rt5506_power_enable);
		}
	}

	kfree(p5506data);

	if (rt5506Connect) {
		misc_deregister(&rt5506_device);
		cancel_delayed_work_sync(&rt5506_query.hs_imp_detec_work);
		destroy_workqueue(hs_wq);
	}
	return 0;
}

static void rt5506_shutdown(struct i2c_client *client)
{
	rt5506_query.gpio_off_cancel = 1;
	cancel_delayed_work_sync(&rt5506_query.gpio_off_work);
	cancel_delayed_work_sync(&rt5506_query.volume_ramp_work);

	mutex_lock(&rt5506_query.gpiolock);
	mutex_lock(&hp_amp_lock);
	mutex_lock(&rt5506_query.mlock);

	if (rt5506_query.gpiostatus == AMP_GPIO_OFF) {
		pr_info("%s: enable gpio %d\n", __func__,
		pdata->rt5506_enable);

		gpio_set_value(pdata->rt5506_enable, 1);
		rt5506_query.gpiostatus = AMP_GPIO_ON;
		usleep_range(1000, 2000);
	}
	pr_info("%s: reset rt5506\n", __func__);
	rt5506_write_reg(0x0, 0x4);
	mdelay(1);
	high_imp = 0;

	if (rt5506_query.gpiostatus == AMP_GPIO_ON) {
		pr_info("%s: disable gpio %d\n", __func__,
		pdata->rt5506_enable);

		gpio_set_value(pdata->rt5506_enable, 0);
		rt5506_query.gpiostatus = AMP_GPIO_OFF;
	}

	rt5506Connect = 0;

	mutex_unlock(&rt5506_query.mlock);
	mutex_unlock(&hp_amp_lock);
	mutex_unlock(&rt5506_query.gpiolock);

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
	.suspend = NULL,
	.resume = NULL,
	.id_table = rt5506_id,
	.driver = {
		.owner	= THIS_MODULE,
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
