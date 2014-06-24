/* driver/i2c/chip/tfa9895.c
 *
 * NXP tfa9895 Speaker Amp
 *
 * Copyright (C) 2012 HTC Corporation
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
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include "tfa9895.h"
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_gpio.h>

/* htc audio ++ */
#undef pr_info
#undef pr_err
#define pr_aud_fmt(fmt) "[AUD] " KBUILD_MODNAME ": " fmt
#define pr_info(fmt, ...) printk(KERN_INFO pr_aud_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...) printk(KERN_ERR pr_aud_fmt(fmt), ##__VA_ARGS__)
/* htc audio -- */

static struct i2c_client *this_client;
static struct tfa9895_platform_data *pdata;
struct mutex spk_amp_lock;
static int last_spkamp_state;
static int dsp_enabled;
static int tfa9895_i2c_write(char *txdata, int length);
static int tfa9895_i2c_read(char *rxdata, int length);
#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_tpa_dent;
static struct dentry *debugfs_peek;
static struct dentry *debugfs_poke;
static unsigned char read_data;

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
			}
		else
			return -EINVAL;
	}
	return 0;
}

static int codec_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t codec_debug_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char lbuf[8];

	snprintf(lbuf, sizeof(lbuf), "0x%x\n", read_data);
	return simple_read_from_buffer(ubuf, count, ppos, lbuf, strlen(lbuf));
}

static ssize_t codec_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *access_str = filp->private_data;
	char lbuf[32];
	unsigned char reg_idx[2] = {0x00, 0x00};
	int rc;
	long int param[5];

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';

	if (!strcmp(access_str, "poke")) {
		/* write */
		rc = get_parameters(lbuf, param, 2);
		if ((param[0] <= 0xFF) && (param[1] <= 0xFF) &&
			(rc == 0)) {
			reg_idx[0] = param[0];
			reg_idx[1] = param[1];
			tfa9895_i2c_write(reg_idx, 2);
		} else
			rc = -EINVAL;
	} else if (!strcmp(access_str, "peek")) {
		/* read */
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] <= 0xFF) && (rc == 0)) {
			reg_idx[0] = param[0];
			tfa9895_i2c_read(&read_data, 1);
		} else
			rc = -EINVAL;
	}

	if (rc == 0)
		rc = cnt;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static const struct file_operations codec_debug_ops = {
	.open = codec_debug_open,
	.write = codec_debug_write,
	.read = codec_debug_read
};
#endif
#ifdef CONFIG_AMP_TFA9895L
unsigned char cf_dsp_bypass[3][3] = {
	{0x04, 0x88, 0x53}, /* for Rchannel */
	{0x09, 0x06, 0x19},
	{0x09, 0x06, 0x18}
};
#else
unsigned char cf_dsp_bypass[3][3] = {
	{0x04, 0x88, 0x0B},
	{0x09, 0x06, 0x19},
	{0x09, 0x06, 0x18}
};
#endif
unsigned char amp_off[1][3] = {
	{0x09, 0x06, 0x19}
};

static int tfa9895_i2c_write(char *txdata, int length)
{
	int rc;
	struct i2c_msg msg[] = {
		{
			.addr = this_client->addr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};

	rc = i2c_transfer(this_client->adapter, msg, 1);
	if (rc < 0) {
		pr_err("%s: transfer error %d\n", __func__, rc);
		return rc;
	}

	return 0;
}

static int tfa9895_i2c_read(char *rxdata, int length)
{
	int rc;
	struct i2c_msg msgs[] = {
		{
		 .addr = this_client->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxdata,
		},
	};

	rc = i2c_transfer(this_client->adapter, msgs, 1);
	if (rc < 0) {
		pr_err("%s: transfer error %d\n", __func__, rc);
		return rc;
	}

	return 0;
}

static int tfa9895_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int tfa9895_release(struct inode *inode, struct file *file)
{
	return 0;
}

int set_tfa9895_spkamp(int en, int dsp_mode)
{
	int i = 0;
	unsigned char mute_reg[1] = {0x06};
	unsigned char mute_data[3] = {0, 0, 0};
	unsigned char power_reg[1] = {0x09};
	unsigned char power_data[3] = {0, 0, 0};
	unsigned char SPK_CR[3] = {0x8, 0x8, 0};

	pr_info("%s: en = %d dsp_enabled = %d\n", __func__, en, dsp_enabled);
	mutex_lock(&spk_amp_lock);
	if (en && !last_spkamp_state) {
		last_spkamp_state = 1;
		/* NXP CF DSP Bypass mode */
		if (dsp_enabled == 0) {
			for (i = 0; i < 3; i++)
				tfa9895_i2c_write(cf_dsp_bypass[i], 3);
		/* Enable NXP PVP Bit10 of Reg 8 per acoustic's request in bypass mode.(Hboot loopback & MFG ROM) */
				tfa9895_i2c_write(SPK_CR, 1);
				tfa9895_i2c_read(SPK_CR + 1, 2);
				SPK_CR[1] |= 0x4; /* Enable PVP bit10 */
				tfa9895_i2c_write(SPK_CR, 3);
		} else {
			tfa9895_i2c_write(power_reg, 1);
			tfa9895_i2c_read(power_data + 1, 2);
			tfa9895_i2c_write(mute_reg, 1);
			tfa9895_i2c_read(mute_data + 1, 2);
			mute_data[0] = 0x6;
			mute_data[2] &= 0xdf;  /* bit 5 dn = un=mute */
			power_data[0] = 0x9;
			power_data[2] &= 0xfe; /* bit 0 dn = power up */
			tfa9895_i2c_write(power_data, 3);
			tfa9895_i2c_write(mute_data, 3);
			power_data[2] |= 0x8;  /* bit 3 Up = AMP on */
			tfa9895_i2c_write(power_data, 3);
		}
	} else if (!en && last_spkamp_state) {
		last_spkamp_state = 0;
		if (dsp_enabled == 0) {
			tfa9895_i2c_write(amp_off[0], 3);
		} else {
			tfa9895_i2c_write(power_reg, 1);
			tfa9895_i2c_read(power_data + 1, 2);
			tfa9895_i2c_write(mute_reg, 1);
			tfa9895_i2c_read(mute_data + 1, 2);
			mute_data[0] = 0x6;
			mute_data[2] |= 0x20; /* bit 5 up = mute */
			tfa9895_i2c_write(mute_data, 3);
			tfa9895_i2c_write(power_reg, 1);
			tfa9895_i2c_read(power_data + 1, 2);
			power_data[0] = 0x9;
			power_data[2] &= 0xf7;	/* bit 3 down = AMP off */
			tfa9895_i2c_write(power_data, 3);
			power_data[2] |= 0x1;  /* bit 0 up = power down */
			tfa9895_i2c_write(power_data, 3);
		}
	}
	mutex_unlock(&spk_amp_lock);
	return 0;
}

int tfa9895_disable(bool disable)
{
	int rc = 0;

	unsigned char amp_on[1][3] = {
	{0x09, 0x06, 0x18}
	};

	if (disable) {
		pr_info("%s: speaker switch off!\n", __func__);
		rc = tfa9895_i2c_write(amp_off[0], 3);
	} else {
		pr_info("%s: speaker switch on!\n", __func__);
		rc = tfa9895_i2c_write(amp_on[0], 3);
	}

	return rc;
}

static long tfa9895_ioctl(struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	int rc = 0;
	unsigned char *buf;
	void __user *argp = (void __user *)arg;

	if (_IOC_TYPE(cmd) != TFA9895_IOCTL_MAGIC)
		return -ENOTTY;

	if (_IOC_SIZE(cmd) > sizeof(struct tfa9895_i2c_buffer))
		return -EINVAL;

	buf = kzalloc(_IOC_SIZE(cmd), GFP_KERNEL);

	if (buf == NULL) {
		pr_err("%s %d: allocate kernel buffer failed.\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		rc = copy_from_user(buf, argp, _IOC_SIZE(cmd));
		if (rc) {
			kfree(buf);
			return -EFAULT;
		}
	}

	switch (_IOC_NR(cmd)) {
	case TFA9895_WRITE_CONFIG_NR:
		pr_debug("%s: TFA9895_WRITE_CONFIG\n", __func__);
		rc = tfa9895_i2c_write(((struct tfa9895_i2c_buffer *)buf)->buffer, ((struct tfa9895_i2c_buffer *)buf)->size);
		break;
	case TFA9895_READ_CONFIG_NR:
		pr_debug("%s: TFA9895_READ_CONFIG\n", __func__);
		rc = tfa9895_i2c_read(((struct tfa9895_i2c_buffer *)buf)->buffer, ((struct tfa9895_i2c_buffer *)buf)->size);
		break;
	case TFA9895_WRITE_L_CONFIG_NR:
		pr_debug("%s: TFA9895_WRITE_CONFIG_L\n", __func__);
		rc = tfa9895_l_write(((struct tfa9895_i2c_buffer *)buf)->buffer, ((struct tfa9895_i2c_buffer *)buf)->size);
		break;
	case TFA9895_READ_L_CONFIG_NR:
		pr_debug("%s: TFA9895_READ_CONFIG_L\n", __func__);
		rc = tfa9895_l_read(((struct tfa9895_i2c_buffer *)buf)->buffer, ((struct tfa9895_i2c_buffer *)buf)->size);
		break;
	case TFA9895_ENABLE_DSP_NR:
		pr_info("%s: TFA9895_ENABLE_DSP %d\n", __func__, *(int *)buf);
		dsp_enabled = *(int *)buf;
		break;
	default:
		kfree(buf);
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		rc = copy_to_user(argp, buf, _IOC_SIZE(cmd));
		if (rc) {
			kfree(buf);
			return -EFAULT;
		}
	}
	kfree(buf);
	return rc;
}

static const struct file_operations tfa9895_fops = {
	.owner = THIS_MODULE,
	.open = tfa9895_open,
	.release = tfa9895_release,
	.unlocked_ioctl = tfa9895_ioctl,
	.compat_ioctl = tfa9895_ioctl,
};

static struct miscdevice tfa9895_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tfa9895",
	.fops = &tfa9895_fops,
};

int tfa9895_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i;
	unsigned char SPK_CR[3] = {0x8, 0x8, 0};

	int ret = 0;
	char temp[6] = {0x4, 0x88};
	pdata = client->dev.platform_data;

	if (pdata == NULL) {
		ret = -ENOMEM;
		pr_err("%s: platform data is NULL\n", __func__);
		goto err_alloc_data_failed;
	}

	if (gpio_is_valid(pdata->tfa9895_power_enable)) {
		ret = gpio_request(pdata->tfa9895_power_enable, "tfa9895-power-enable");
		if (ret) {
			pr_err("%s: Fail gpio_request tfa9895-power-enable\n", __func__);
			goto err_free_gpio_all;
		}

		ret = gpio_direction_output(pdata->tfa9895_power_enable, 1);
		if (ret) {
			pr_err("%s: Fail gpio_direction tfa9895-power-enable\n", __func__);
			goto err_free_gpio_all;
		}
		pr_info("%s: tfa9895 power pin is enabled\n", __func__);

		mdelay(20);
	}

	this_client = client;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c check functionality error\n", __func__);
		ret = -ENODEV;
		goto err_free_gpio_all;
	}

	ret = misc_register(&tfa9895_device);
	if (ret) {
		pr_err("%s: tfa9895_device register failed\n", __func__);
		goto err_free_gpio_all;
	}
	ret = tfa9895_i2c_write(temp, 2);
	ret = tfa9895_i2c_read(temp, 5);
	if (ret < 0)
		pr_info("%s:i2c read fail\n", __func__);
	else
		pr_info("%s:i2c read successfully\n", __func__);

#ifdef CONFIG_DEBUG_FS
	debugfs_tpa_dent = debugfs_create_dir("tfa9895", 0);
	if (!IS_ERR(debugfs_tpa_dent)) {
		debugfs_peek = debugfs_create_file("peek",
		S_IFREG | S_IRUGO, debugfs_tpa_dent,
		(void *) "peek", &codec_debug_ops);

		debugfs_poke = debugfs_create_file("poke",
		S_IFREG | S_IRUGO, debugfs_tpa_dent,
		(void *) "poke", &codec_debug_ops);

	}
#endif

	for (i = 0; i < 3; i++)
		tfa9895_i2c_write(cf_dsp_bypass[i], 3);

	/* Enable NXP PVP Bit10 of Reg 8 per acoustic's request in bypass mode.(Hboot loopback & MFG ROM) */
	tfa9895_i2c_write(SPK_CR, 1);
	tfa9895_i2c_read(SPK_CR + 1, 2);
	SPK_CR[1] |= 0x4; /* Enable PVP bit10 */
	tfa9895_i2c_write(SPK_CR, 3);

	return 0;

err_free_gpio_all:
	return ret;
err_alloc_data_failed:
	return ret;
}

static int tfa9895_remove(struct i2c_client *client)
{
	struct tfa9895_platform_data *p9895data = i2c_get_clientdata(client);
	kfree(p9895data);

	return 0;
}

static void tfa9895_shutdown(struct i2c_client *client)
{
	int ret = 0;
	pdata = client->dev.platform_data;

	if (pdata == NULL) {
		ret = -ENOMEM;
		pr_err("%s: platform data is NULL, could not disable tfa9895-power-enable pin\n", __func__);
	} else if (gpio_is_valid(pdata->tfa9895_power_enable)) {
		ret = gpio_request(pdata->tfa9895_power_enable, "tfa9895-power-enable");
		if (ret)
			pr_err("%s: Fail gpio_request tfa9895-power-enable\n", __func__);

		ret = gpio_direction_output(pdata->tfa9895_power_enable, 0);
		if (ret)
			pr_err("%s: Fail gpio_direction tfa9895-power-enable\n", __func__);
		else
			pr_info("%s: tfa9895 power pin is disabled\n", __func__);

		mdelay(20);
	}

	return;
}

static int tfa9895_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int tfa9895_resume(struct i2c_client *client)
{
	return 0;
}

static struct of_device_id tfa9895_match_table[] = {
	{ .compatible = "nxp,tfa9895-amp",},
	{ },
};

static const struct i2c_device_id tfa9895_id[] = {
	{ TFA9895_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver tfa9895_driver = {
	.probe = tfa9895_probe,
	.remove = tfa9895_remove,
	.shutdown = tfa9895_shutdown,
	.suspend = tfa9895_suspend,
	.resume = tfa9895_resume,
	.id_table = tfa9895_id,
	.driver = {
		.name = TFA9895_I2C_NAME,
		.of_match_table = tfa9895_match_table,
	},
};

static int __init tfa9895_init(void)
{
	pr_info("%s\n", __func__);
	mutex_init(&spk_amp_lock);
	dsp_enabled = 0;
	return i2c_add_driver(&tfa9895_driver);
}

static void __exit tfa9895_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove(debugfs_peek);
	debugfs_remove(debugfs_poke);
	debugfs_remove(debugfs_tpa_dent);
#endif
	i2c_del_driver(&tfa9895_driver);
}

module_init(tfa9895_init);
module_exit(tfa9895_exit);

MODULE_DESCRIPTION("tfa9895 Speaker Amp driver");
MODULE_LICENSE("GPL");
