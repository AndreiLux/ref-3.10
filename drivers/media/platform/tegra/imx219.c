/*
 * imx219.c - imx219 sensor driver
 *
 * Copyright (c) 2013, NVIDIA CORPORATION, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <media/imx219.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "nvc_utilities.h"

#include "imx219_tables.h"

static bool bank_a = true;

struct imx219_info {
	struct miscdevice		miscdev_info;
	int				mode;
	struct imx219_power_rail	power;
	struct nvc_fuseid		fuse_id;
	struct i2c_client		*i2c_client;
	struct imx219_platform_data	*pdata;
	struct clk			*mclk;
	struct mutex			imx219_camera_lock;
	struct dentry			*debugdir;
	atomic_t			in_use;
};
static struct imx219_reg flash_strobe_mod[] = {
	{0x0324, 0x01},	/* flash strobe output enble on ERS mode */
	{0x0322, 0x01},	/* reference dividor fron EXT CLK */
	{0x032F, 0x01},	/* shutter sync mode */
	{0x0330, 0x00},	/* ref point hi */
	{0x0331, 0x00},	/* ref point lo */
	{0x0332, 0x00},	/* latency hi from ref point */
	{0x0333, 0x00},	/* latency lo from ref point */
	{0x0334, 0x09},	/* high period of XHS for ERS hi */
	{0x0335, 0x60},	/* high period of XHS for ERS lo */
	{0x0336, 0x00},	/* low period of XHS for ERS hi */
	{0x0337, 0x00},	/* low period of XHS for ERS lo */
	{0x0338, 0x01},	/* num of ERS flash pulse */
	{IMX219_TABLE_END, 0x00}
};

static inline void
msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base*1000, delay_base*1000+500);
}

static inline void
imx219_get_frame_length_regs(struct imx219_reg *regs, u32 frame_length)
{
	regs->addr = 0x0160;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = 0x0161;
	(regs + 1)->val = (frame_length) & 0xff;
}

static inline void
imx219_get_coarse_time_regs(struct imx219_reg *regs, u32 coarse_time)
{
	regs->addr = 0x15a;
	regs->val = (coarse_time >> 8) & 0xff;
	(regs + 1)->addr = 0x15b;
	(regs + 1)->val = (coarse_time) & 0xff;
}

static inline void
imx219_get_gain_reg(struct imx219_reg *regs, u16 gain)
{
	regs->addr = 0x157;
	regs->val = gain;
}

static int
imx219_read_reg(struct i2c_client *client, u16 addr, u8 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[3];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	*val = data[2];
	return 0;
}

static int
imx219_write_reg(struct i2c_client *client, u16 addr, u8 val)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[3];

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err == 1)
		return 0;

	pr_err("%s:i2c write failed, %x = %x\n",
			__func__, addr, val);

	return err;
}

static int
imx219_write_table(struct i2c_client *client,
				 const struct imx219_reg table[],
				 const struct imx219_reg override_list[],
				 int num_override_regs)
{
	int err;
	const struct imx219_reg *next;
	int i;
	u16 val;

	for (next = table; next->addr != IMX219_TABLE_END; next++) {
		if (next->addr == IMX219_TABLE_WAIT_MS) {
			msleep_range(next->val);
			continue;
		}

		val = next->val;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list            */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		err = imx219_write_reg(client, next->addr, val);
		if (err) {
			pr_err("%s:imx219_write_table:%d", __func__, err);
			return err;
		}
	}
	return 0;
}

static int imx219_set_flash_output(struct imx219_info *info)
{
	struct imx219_flash_control *fctl;

	if (!info->pdata)
		return 0;

	fctl = &info->pdata->flash_cap;
	dev_dbg(&info->i2c_client->dev, "%s: %x\n", __func__, fctl->enable);
	dev_dbg(&info->i2c_client->dev, "edg: %x, st: %x, rpt: %x, dly: %x\n",
		fctl->edge_trig_en, fctl->start_edge,
		fctl->repeat, fctl->delay_frm);
	return imx219_write_table(info->i2c_client, flash_strobe_mod, NULL, 0);
}

static int imx219_get_flash_cap(struct imx219_info *info)
{
	struct imx219_flash_control *fctl;

	dev_dbg(&info->i2c_client->dev, "%s: %p\n", __func__, info->pdata);
	if (info->pdata) {
		fctl = &info->pdata->flash_cap;
		dev_dbg(&info->i2c_client->dev,
			"edg: %x, st: %x, rpt: %x, dl: %x\n",
			fctl->edge_trig_en,
			fctl->start_edge,
			fctl->repeat,
			fctl->delay_frm);

		if (fctl->enable)
			return 0;
	}
	return -ENODEV;
}

static inline int imx219_set_flash_control(
	struct imx219_info *info, struct imx219_flash_control *fc)
{
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	return imx219_write_reg(info->i2c_client, 0x0320, 0x01);
}

static int
imx219_set_mode(struct imx219_info *info, struct imx219_mode *mode)
{
	int sensor_mode;
	int err;
	struct imx219_reg reg_list[8];

	pr_info("%s: xres %u yres %u framelength %u coarsetime %u gain %u\n",
			 __func__, mode->xres, mode->yres, mode->frame_length,
			 mode->coarse_time, mode->gain);

	if (mode->xres == 3280 && mode->yres == 2464) {
		sensor_mode = IMX219_MODE_3280x2464;
	} else if (mode->xres == 1640 && mode->yres == 1232) {
		sensor_mode = IMX219_MODE_1640x1232;
	} else if (mode->xres == 1920 && mode->yres == 1080) {
		sensor_mode = IMX219_MODE_1920x1080;
	} else {
		pr_err("%s: invalid resolution supplied to set mode %d %d\n",
			 __func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	/* get a list of override regs for the asking frame length, */
	/* coarse integration time, and gain.                       */
	imx219_get_frame_length_regs(reg_list, mode->frame_length);
	imx219_get_coarse_time_regs(reg_list + 2, mode->coarse_time);
	imx219_get_gain_reg(reg_list + 4, mode->gain);

	bank_a = true;
	err = imx219_write_table(info->i2c_client,
				mode_table[sensor_mode],
				reg_list, 5);
		return err;

	imx219_set_flash_output(info);

	info->mode = sensor_mode;
	pr_info("[IMX219]: stream on.\n");
	return 0;
}

static int
imx219_get_status(struct imx219_info *info, u8 *dev_status)
{
	*dev_status = 0;
	return 0;
}

static int
imx219_set_frame_length(struct imx219_info *info, u32 frame_length,
						 bool bank_a)
{
	struct imx219_reg reg_list[2];
	int i = 0;
	int ret;

	imx219_get_frame_length_regs(reg_list, frame_length);

	for (i = 0; i < 2; i++) {
		if (!bank_a)
			reg_list[i].addr += 0x100;

		ret = imx219_write_reg(info->i2c_client, reg_list[i].addr,
			 reg_list[i].val);
		if (ret)
			return ret;
	}

	return 0;
}

static int
imx219_set_coarse_time(struct imx219_info *info, u32 coarse_time,
						 bool bank_a)
{
	int ret;

	struct imx219_reg reg_list[2];
	int i = 0;

	imx219_get_coarse_time_regs(reg_list, coarse_time);

	for (i = 0; i < 2; i++) {
		if (!bank_a)
			reg_list[i].addr += 0x100;

		ret = imx219_write_reg(info->i2c_client, reg_list[i].addr,
			 reg_list[i].val);
		if (ret)
			return ret;
	}

	return 0;
}

static int
imx219_set_gain(struct imx219_info *info, u16 gain, bool bank_a)
{
	int ret;
	struct imx219_reg reg_list;

	imx219_get_gain_reg(&reg_list, gain);
	if (!bank_a)
		reg_list.addr += 0x100;
	ret = imx219_write_reg(info->i2c_client, reg_list.addr, reg_list.val);

	return ret;
}

static int
imx219_set_group_hold(struct imx219_info *info, struct imx219_ae *ae)
{
	int ret;

	bank_a = !bank_a;

	if (ae->gain_enable)
		imx219_set_gain(info, ae->gain, bank_a);
	if (ae->coarse_time_enable)
		imx219_set_coarse_time(info, ae->coarse_time, bank_a);
	if (ae->frame_length_enable)
		imx219_set_frame_length(info, ae->frame_length, bank_a);

	ret = imx219_write_reg(info->i2c_client, 0x150, bank_a ? 0 : 1);

	/* sync the setting after switch bank */
	if (ae->gain_enable)
		imx219_set_gain(info, ae->gain, !bank_a);
	if (ae->coarse_time_enable)
		imx219_set_coarse_time(info, ae->coarse_time, !bank_a);
	if (ae->frame_length_enable)
		imx219_set_frame_length(info, ae->frame_length, !bank_a);

	return ret;
}

static int imx219_get_sensor_id(struct imx219_info *info)
{
	int ret = 0;
	int i;
	u8 bak = 0;

	pr_info("%s\n", __func__);
	if (info->fuse_id.size)
		return 0;

	/* Note 1: If the sensor does not have power at this point
	Need to supply the power, e.g. by calling power on function */

	for (i = 0; i < 4 ; i++) {
		ret |= imx219_read_reg(info->i2c_client, 0x0004 + i, &bak);
		pr_info("chip unique id 0x%x = 0x%02x\n", i, bak);
		info->fuse_id.data[i] = bak;
	}

	for (i = 0; i < 2 ; i++) {
		ret |= imx219_read_reg(info->i2c_client, 0x000d + i , &bak);
		pr_info("chip unique id 0x%x = 0x%02x\n", i + 4, bak);
		info->fuse_id.data[i + 4] = bak;
	}

	if (!ret)
		info->fuse_id.size = 6;

	/* Note 2: Need to clean up any action carried out in Note 1 */

	return ret;
}

static void imx219_mclk_disable(struct imx219_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int imx219_mclk_enable(struct imx219_info *info)
{
	int err;
	unsigned long mclk_init_rate = 24000000;

	dev_dbg(&info->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
		__func__, mclk_init_rate);

	err = clk_set_rate(info->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(info->mclk);
	return err;
}

static long
imx219_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct imx219_info *info = file->private_data;

	switch (cmd) {
	case IMX219_IOCTL_SET_POWER:
		if (!info->pdata)
			break;
		if (arg && info->pdata->power_on) {
			err = imx219_mclk_enable(info);
			if (!err)
				err = info->pdata->power_on(&info->power);
			if (err < 0)
				imx219_mclk_disable(info);
		}
		if (!arg && info->pdata->power_off) {
			info->pdata->power_off(&info->power);
			imx219_mclk_disable(info);
		}
		break;
	case IMX219_IOCTL_SET_MODE:
	{
		struct imx219_mode mode;
		if (copy_from_user(&mode, (const void __user *)arg,
			sizeof(struct imx219_mode))) {
			pr_err("%s:Failed to get mode from user.\n", __func__);
			return -EFAULT;
		}
		return imx219_set_mode(info, &mode);
	}
	case IMX219_IOCTL_SET_FRAME_LENGTH:
		err = imx219_set_frame_length(info, (u32)arg, true);
		err = imx219_set_frame_length(info, (u32)arg, false);
	case IMX219_IOCTL_SET_COARSE_TIME:
		err = imx219_set_coarse_time(info, (u32)arg, true);
		err = imx219_set_coarse_time(info, (u32)arg, false);
	case IMX219_IOCTL_SET_GAIN:
		err = imx219_set_gain(info, (u16)arg, true);
		err = imx219_set_gain(info, (u16)arg, false);
		return err;
	case IMX219_IOCTL_GET_STATUS:
	{
		u8 status;

		err = imx219_get_status(info, &status);
		if (err)
			return err;
		if (copy_to_user((void __user *)arg, &status, 1)) {
			pr_err("%s:Failed to copy status to user\n", __func__);
			return -EFAULT;
			}
		return 0;
	}
	case IMX219_IOCTL_GET_FUSEID:
	{
		err = imx219_get_sensor_id(info);

		if (err) {
			pr_err("%s:Failed to get fuse id info.\n", __func__);
			return err;
		}
		if (copy_to_user((void __user *)arg, &info->fuse_id,
				sizeof(struct nvc_fuseid))) {
			pr_info("%s:Failed to copy fuse id to user space\n",
				__func__);
			return -EFAULT;
		}
		return 0;
	}
	case IMX219_IOCTL_SET_GROUP_HOLD:
	{
		struct imx219_ae ae;
		if (copy_from_user(&ae, (const void __user *)arg,
			sizeof(struct imx219_ae))) {
			pr_info("%s:fail group hold\n", __func__);
			return -EFAULT;
		}
		return imx219_set_group_hold(info, &ae);
	}
	case IMX219_IOCTL_SET_FLASH_MODE:
	{
		struct imx219_flash_control values;

		dev_dbg(&info->i2c_client->dev,
			"IMX219_IOCTL_SET_FLASH_MODE\n");
		if (copy_from_user(&values,
			(const void __user *)arg,
			sizeof(struct imx219_flash_control))) {
			err = -EFAULT;
			break;
		}
		err = imx219_set_flash_control(info, &values);
		break;
	}
	case IMX219_IOCTL_GET_FLASH_CAP:
		err = imx219_get_flash_cap(info);
		break;
	default:
		pr_err("%s:unknown cmd.\n", __func__);
		err = -EINVAL;
	}

	return err;
}

static int imx219_debugfs_show(struct seq_file *s, void *unused)
{
	struct imx219_info *dev = s->private;

	dev_dbg(&dev->i2c_client->dev, "%s: ++\n", __func__);

	mutex_lock(&dev->imx219_camera_lock);
	mutex_unlock(&dev->imx219_camera_lock);

	return 0;
}

static ssize_t imx219_debugfs_write(
	struct file *file,
	char const __user *buf,
	size_t count,
	loff_t *offset)
{
	struct imx219_info *dev =
			((struct seq_file *)file->private_data)->private;
	struct i2c_client *i2c_client = dev->i2c_client;
	int ret = 0;
	char buffer[24];
	u32 address;
	u32 data;
	u8 readback;

	pr_info("%s: ++\n", __func__);

	if (copy_from_user(&buffer, buf, sizeof(buffer)))
		goto debugfs_write_fail;

	if (sscanf(buf, "0x%x 0x%x", &address, &data) == 2)
		goto set_attr;
	if (sscanf(buf, "0X%x 0X%x", &address, &data) == 2)
		goto set_attr;
	if (sscanf(buf, "%d %d", &address, &data) == 2)
		goto set_attr;

	if (sscanf(buf, "0x%x 0x%x", &address, &data) == 1)
		goto read;
	if (sscanf(buf, "0X%x 0X%x", &address, &data) == 1)
		goto read;
	if (sscanf(buf, "%d %d", &address, &data) == 1)
		goto read;

	pr_err("SYNTAX ERROR: %s\n", buf);
	return -EFAULT;

set_attr:
	pr_info("new address = %x, data = %x\n", address, data);
	ret |= imx219_write_reg(i2c_client, address, data);
read:
	ret |= imx219_read_reg(i2c_client, address, &readback);
	pr_info("wrote to address 0x%x with value 0x%x\n",
			address, readback);

	if (ret)
		goto debugfs_write_fail;

	return count;

debugfs_write_fail:
	pr_err("%s: test pattern write failed\n", __func__);
	return -EFAULT;
}

static int imx219_debugfs_open(struct inode *inode, struct file *file)
{
	struct imx219_info *dev = inode->i_private;
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);

	return single_open(file, imx219_debugfs_show, inode->i_private);
}

static const struct file_operations imx219_debugfs_fops = {
	.open		= imx219_debugfs_open,
	.read		= seq_read,
	.write		= imx219_debugfs_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void imx219_remove_debugfs(struct imx219_info *dev)
{
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);

	debugfs_remove_recursive(dev->debugdir);
	dev->debugdir = NULL;
}

static void imx219_create_debugfs(struct imx219_info *dev)
{
	struct dentry *ret;
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s\n", __func__);

	dev->debugdir =
		debugfs_create_dir(dev->miscdev_info.this_device->kobj.name,
							NULL);
	if (!dev->debugdir)
		goto remove_debugfs;

	ret = debugfs_create_file("d",
				S_IWUSR | S_IRUGO,
				dev->debugdir, dev,
				&imx219_debugfs_fops);
	if (!ret)
		goto remove_debugfs;

	return;
remove_debugfs:
	dev_err(&i2c_client->dev, "couldn't create debugfs\n");
	imx219_remove_debugfs(dev);
}

static int
imx219_open(struct inode *inode, struct file *file)
{
	struct miscdevice	*miscdev = file->private_data;
	struct imx219_info *info;

	info = container_of(miscdev, struct imx219_info, miscdev_info);
	/* check if the device is in use */
	if (atomic_xchg(&info->in_use, 1)) {
		pr_info("%s:BUSY!\n", __func__);
		return -EBUSY;
	}

	file->private_data = info;

	return 0;
}

static int
imx219_release(struct inode *inode, struct file *file)
{
	struct imx219_info *info = file->private_data;

	file->private_data = NULL;

	/* warn if device is already released */
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static int imx219_power_put(struct imx219_power_rail *pw)
{
	if (unlikely(!pw))
		return -EFAULT;

	if (likely(pw->avdd))
		regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		regulator_put(pw->iovdd);

	if (likely(pw->dvdd))
		regulator_put(pw->dvdd);

	pw->avdd = NULL;
	pw->iovdd = NULL;
	pw->dvdd = NULL;

	return 0;
}

static int imx219_regulator_get(struct imx219_info *info,
	struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = regulator_get(&info->i2c_client->dev, vreg_name);
	if (unlikely(IS_ERR_OR_NULL(reg))) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);

	*vreg = reg;
	return err;
}

static int imx219_power_get(struct imx219_info *info)
{
	struct imx219_power_rail *pw = &info->power;

	imx219_regulator_get(info, &pw->avdd, "vana"); /* ananlog 2.7v */
	imx219_regulator_get(info, &pw->iovdd, "vif"); /* interface 1.8v */

	return 0;
}

static const struct file_operations imx219_fileops = {
	.owner = THIS_MODULE,
	.open = imx219_open,
	.unlocked_ioctl = imx219_ioctl,
	.release = imx219_release,
};

static struct miscdevice imx219_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "imx219",
	.fops = &imx219_fileops,
};


static int
imx219_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct imx219_info *info;
	int err;
	const char *mclk_name;

	pr_info("[IMX219]: probing sensor.\n");

	info = devm_kzalloc(&client->dev,
			sizeof(struct imx219_info), GFP_KERNEL);
	if (!info) {
		pr_err("%s:Unable to allocate memory!\n", __func__);
		return -ENOMEM;
	}

	info->pdata = client->dev.platform_data;
	info->i2c_client = client;
	atomic_set(&info->in_use, 0);
	info->mode = -1;

	mclk_name = info->pdata->mclk_name ?
		    info->pdata->mclk_name : "default_mclk";
	info->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(info->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		return PTR_ERR(info->mclk);
	}

	imx219_power_get(info);

	memcpy(&info->miscdev_info,
		&imx219_device,
		sizeof(struct miscdevice));

	err = misc_register(&info->miscdev_info);
	if (err) {
		pr_err("%s:Unable to register misc device!\n", __func__);
		goto imx219_probe_fail;
	}

	i2c_set_clientdata(client, info);
	/* create debugfs interface */
	imx219_create_debugfs(info);
	return 0;

imx219_probe_fail:
	imx219_power_put(&info->power);

	return err;
}

static int
imx219_remove(struct i2c_client *client)
{
	struct imx219_info *info;
	info = i2c_get_clientdata(client);
	misc_deregister(&imx219_device);

	imx219_power_put(&info->power);

	imx219_remove_debugfs(info);
	return 0;
}

static const struct i2c_device_id imx219_id[] = {
	{ "imx219", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, imx219_id);

static struct i2c_driver imx219_i2c_driver = {
	.driver = {
		.name = "imx219",
		.owner = THIS_MODULE,
	},
	.probe = imx219_probe,
	.remove = imx219_remove,
	.id_table = imx219_id,
};

static int __init imx219_init(void)
{
	pr_info("[IMX219] sensor driver loading\n");
	return i2c_add_driver(&imx219_i2c_driver);
}

static void __exit imx219_exit(void)
{
	i2c_del_driver(&imx219_i2c_driver);
}

module_init(imx219_init);
module_exit(imx219_exit);
