/*
 * ov9760.c - ov9760 sensor driver
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
#include <media/ov9760.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <media/nvc.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/debugfs.h>

#define SIZEOF_I2C_TRANSBUF 32

struct ov9760_reg {
	u16 addr;
	u16 val;
};

struct ov9760_info {
	struct miscdevice		miscdev_info;
	int mode;
	struct ov9760_power_rail	power;
	struct i2c_client *i2c_client;
	struct clk			*mclk;
	struct ov9760_platform_data *pdata;
	u8 i2c_trans_buf[SIZEOF_I2C_TRANSBUF];
	struct ov9760_sensordata sensor_data;
	atomic_t			in_use;
#ifdef CONFIG_DEBUG_FS
	struct dentry			*debugfs_root;
	u32				debug_i2c_offset;
#endif
};

#define OV9760_TABLE_WAIT_MS 0
#define OV9760_TABLE_END 1
#define OV9760_MAX_RETRIES 3

static struct ov9760_reg mode_1472x1104[] = {
	{0x0103, 0x01},
	{OV9760_TABLE_WAIT_MS, 10},
	{0X0340, 0X04},
	{0X0341, 0XCC},
	{0X0342, 0X07},
	{0X0343, 0X00},
	{0X0344, 0X00},
	{0X0345, 0X10},
	{0X0346, 0X00},
	{0X0347, 0X08},
	{0X0348, 0X05},
	{0X0349, 0Xd7},
	{0X034a, 0X04},
	{0X034b, 0X68},
	{0X3811, 0X04},
	{0X3813, 0X04},
	{0X034c, 0X05},
	{0X034d, 0XC0},
	{0X034e, 0X04},
	{0X034f, 0X50},
	{0X0383, 0X01},
	{0X0387, 0X01},
	{0X3820, 0X00},
	{0X3821, 0X00},
	{0X3660, 0X80},
	{0X3680, 0Xf4},
	{0X0100, 0X00},
	{0X0101, 0X01},
	{0X3002, 0X80},
	{0X3012, 0X08},
	{0X3014, 0X04},
	{0X3022, 0X02},
	{0X3023, 0X0f},
	{0X3080, 0X00},
	{0X3090, 0X02},
	{0X3091, 0X2c},
	{0X3092, 0X02},
	{0X3093, 0X02},
	{0X3094, 0X00},
	{0X3095, 0X00},
	{0X3096, 0X01},
	{0X3097, 0X00},
	{0X3098, 0X04},
	{0X3099, 0X14},
	{0X309a, 0X03},
	{0X309c, 0X00},
	{0X309d, 0X00},
	{0X309e, 0X01},
	{0X309f, 0X00},
	{0X30a2, 0X01},
	{0X30b0, 0X0a},
	{0X30b3, 0Xc8},
	{0X30b4, 0X06},
	{0X30b5, 0X00},
	{0X3503, 0X17},
	{0X3509, 0X00},
	{0X3600, 0X7c},
	{0X3621, 0Xb8},
	{0X3622, 0X23},
	{0X3631, 0Xe2},
	{0X3634, 0X03},
	{0X3662, 0X14},
	{0X366b, 0X03},
	{0X3682, 0X82},
	{0X3705, 0X20},
	{0X3708, 0X64},
	{0X371b, 0X60},
	{0X3732, 0X40},
	{0X3745, 0X00},
	{0X3746, 0X18},
	{0X3780, 0X2a},
	{0X3781, 0X8c},
	{0X378f, 0Xf5},
	{0X3823, 0X37},
	{0X383d, 0X88},
	{0X4000, 0X23},
	{0X4001, 0X04},
	{0X4002, 0X45},
	{0X4004, 0X08},
	{0X4005, 0X40},
	{0X4006, 0X40},
	{0X4009, 0X40},
	{0X404F, 0X8F},
	{0X4058, 0X44},
	{0X4101, 0X32},
	{0X4102, 0Xa4},
	{0X4520, 0Xb0},
	{0X4580, 0X08},
	{0X4582, 0X00},
	{0X4307, 0X30},
	{0X4605, 0X00},
	{0X4608, 0X02},
	{0X4609, 0X00},
	{0X4801, 0X0f},
	{0X4819, 0XB6},
	{0X4837, 0X19},
	{0X4906, 0Xff},
	{0X4d00, 0X04},
	{0X4d01, 0X4b},
	{0X4d02, 0Xfe},
	{0X4d03, 0X09},
	{0X4d04, 0X1e},
	{0X4d05, 0Xb7},
	{0X3503, 0X17},
	{0X3a04, 0X04},
	{0X3a05, 0Xa4},
	{0X3a06, 0X00},
	{0X3a07, 0Xf8},
	{0X5781, 0X17},
	{0X5792, 0X00},
	{0X0100, 0X01},
	{OV9760_TABLE_END, 0x0000}
};

enum {
	OV9760_MODE_1472x1104,
};

static struct ov9760_reg *mode_table[] = {
	[OV9760_MODE_1472x1104] = mode_1472x1104,
};

#define OV9760_ENTER_GROUP_HOLD(group_hold) \
	do {	\
		if (group_hold) {   \
			reg_list[offset].addr = 0x104; \
			reg_list[offset].val = 0x01;\
			offset++;  \
		}   \
	} while (0)

#define OV9760_LEAVE_GROUP_HOLD(group_hold) \
	do {	\
		if (group_hold) {   \
			reg_list[offset].addr = 0x104; \
			reg_list[offset].val = 0x00;\
			offset++;  \
		} \
	} while (0)

static void ov9760_mclk_disable(struct ov9760_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int ov9760_mclk_enable(struct ov9760_info *info)
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

static inline int ov9760_get_frame_length_regs(struct ov9760_reg *regs,
						u32 frame_length)
{
	regs->addr = 0x0340;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = 0x0341;
	(regs + 1)->val = (frame_length) & 0xff;
	return 2;
}

static inline int ov9760_get_coarse_time_regs(struct ov9760_reg *regs,
					       u32 coarse_time)
{
	regs->addr = 0x3500;
	regs->val = (coarse_time >> 12) & 0xff;
	(regs + 1)->addr = 0x3501;
	(regs + 1)->val = (coarse_time >> 4) & 0xff;
	(regs + 2)->addr = 0x3502;
	(regs + 2)->val = (coarse_time & 0xf) << 4;
	return 3;
}

static inline int ov9760_get_gain_reg(struct ov9760_reg *regs, u16 gain)
{
	regs->addr = 0x350a;
	regs->val = (gain >> 8) & 0xff;
	(regs + 1)->addr = 0x350b;
	(regs + 1)->val = (gain) & 0xff;
	return 1;
}

static int ov9760_read_reg(struct i2c_client *client, u16 addr, u8 *val)
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

static int ov9760_write_reg(struct i2c_client *client, u16 addr, u8 val)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[3];
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;
	dev_info(&client->dev, "%s\n", __func__);

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry++;
		pr_err("ov9760: i2c transfer failed, retrying %x %x\n",
			addr, val);

		usleep_range(3000, 3100);
	} while (retry <= OV9760_MAX_RETRIES);

	return err;
}

static int ov9760_write_bulk_reg(struct i2c_client *client, u8 *data, int len)
{
	int err;
	struct i2c_msg msg;

	if (!client->adapter)
		return -ENODEV;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err == 1)
		return 0;

	pr_err("ov9760: i2c bulk transfer failed at %x\n",
		(int)data[0] << 8 | data[1]);

	return err;
}

static int ov9760_write_table(struct ov9760_info *info,
			      const struct ov9760_reg table[],
			      const struct ov9760_reg override_list[],
			      int num_override_regs)
{
	int err;
	const struct ov9760_reg *next, *n_next;
	u8 *b_ptr = info->i2c_trans_buf;
	unsigned int buf_filled = 0;
	unsigned int i;
	u16 val;

	for (next = table; next->addr != OV9760_TABLE_END; next++) {
		if (next->addr == OV9760_TABLE_WAIT_MS) {
			msleep(next->val);
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

		if (!buf_filled) {
			b_ptr = info->i2c_trans_buf;
			*b_ptr++ = next->addr >> 8;
			*b_ptr++ = next->addr & 0xff;
			buf_filled = 2;
		}
		*b_ptr++ = val;
		buf_filled++;

		n_next = next + 1;
		if (n_next->addr != OV9760_TABLE_END &&
			n_next->addr != OV9760_TABLE_WAIT_MS &&
			buf_filled < SIZEOF_I2C_TRANSBUF &&
			n_next->addr == next->addr + 1) {
			continue;
		}

		err = ov9760_write_bulk_reg(info->i2c_client,
			info->i2c_trans_buf, buf_filled);
		if (err)
			return err;
		buf_filled = 0;
	}
	return 0;
}

static int ov9760_set_mode(struct ov9760_info *info, struct ov9760_mode *mode)
{
	int sensor_mode;
	int err;
	struct ov9760_reg reg_list[7];

	sensor_mode = OV9760_MODE_1472x1104;
	/* get a list of override regs for the asking frame length, */
	/* coarse integration time, and gain.                       */
	ov9760_get_frame_length_regs(reg_list, mode->frame_length);
	ov9760_get_coarse_time_regs(reg_list + 2, mode->coarse_time);
	ov9760_get_gain_reg(reg_list + 5, mode->gain);

	err = ov9760_write_table(info, mode_table[sensor_mode],
	reg_list, 7);
	if (err)
		return err;

	info->mode = sensor_mode;
	return 0;
}

static int ov9760_set_frame_length(struct ov9760_info *info, u32 frame_length)
{
	int ret;
	struct ov9760_reg reg_list[2];
	u8 *b_ptr = info->i2c_trans_buf;

	ov9760_get_frame_length_regs(reg_list, frame_length);

	*b_ptr++ = reg_list[0].addr >> 8;
	*b_ptr++ = reg_list[0].addr & 0xff;
	*b_ptr++ = reg_list[0].val & 0xff;
	*b_ptr++ = reg_list[1].val & 0xff;
	ret = ov9760_write_bulk_reg(info->i2c_client, info->i2c_trans_buf, 4);

	return ret;
}

static int ov9760_set_coarse_time(struct ov9760_info *info, u32 coarse_time)
{
	int ret;
	struct ov9760_reg reg_list[3];
	u8 *b_ptr = info->i2c_trans_buf;

	dev_info(&info->i2c_client->dev, "coarse_time 0x%x\n", coarse_time);
	ov9760_get_coarse_time_regs(reg_list, coarse_time);

	*b_ptr++ = reg_list[0].addr >> 8;
	*b_ptr++ = reg_list[0].addr & 0xff;
	*b_ptr++ = reg_list[0].val & 0xff;
	*b_ptr++ = reg_list[1].val & 0xff;
	*b_ptr++ = reg_list[2].val & 0xff;

	ret = ov9760_write_bulk_reg(info->i2c_client, info->i2c_trans_buf, 5);
	return ret;

}

static int ov9760_set_gain(struct ov9760_info *info, u16 gain)
{
	int ret;
	struct ov9760_reg reg_list[2];
	int i = 0;
	ov9760_get_gain_reg(reg_list, gain);

	for (i = 0; i < 2; i++)	{
		ret = ov9760_write_reg(info->i2c_client, reg_list[i].addr,
			reg_list[i].val);
		if (ret)
			return ret;
	}
	return ret;
}

static int ov9760_set_group_hold(struct ov9760_info *info, struct ov9760_ae *ae)
{
	int err = 0;
	struct ov9760_reg reg_list[12];
	int offset = 0;
	bool group_hold = true;

	OV9760_ENTER_GROUP_HOLD(group_hold);
	if (ae->gain_enable)
		offset += ov9760_get_gain_reg(reg_list + offset,
					  ae->gain);
	if (ae->frame_length_enable)
		offset += ov9760_get_frame_length_regs(reg_list + offset,
						  ae->frame_length);
	if (ae->coarse_time_enable)
		offset += ov9760_get_coarse_time_regs(reg_list + offset,
			ae->coarse_time);
	OV9760_LEAVE_GROUP_HOLD(group_hold);

	reg_list[offset].addr = OV9760_TABLE_END;
	err |= ov9760_write_table(info, reg_list, NULL, 0);

	return err;
}

static int ov9760_get_sensor_id(struct ov9760_info *info)
{
	int ret = 0;
	int i;
	u8  bak;

	dev_info(&info->i2c_client->dev, "%s\n", __func__);
	if (info->sensor_data.fuse_id_size)
		return 0;

	ov9760_write_reg(info->i2c_client, 0x0100, 0x01);
	/* select bank 31 */
	ov9760_write_reg(info->i2c_client, 0x3d84, 0xc0 & 31);
	ov9760_write_reg(info->i2c_client, 0x3d81, 0x01);
	msleep(10);
	for (i = 0; i < 8; i++) {
		ret |= ov9760_read_reg(info->i2c_client, 0x3d00 + i, &bak);
		info->sensor_data.fuse_id[i] = bak;
	}

	if (!ret)
		info->sensor_data.fuse_id_size = 8;

	return ret;
}

static int ov9760_get_status(struct ov9760_info *info, u8 *status)
{
	int err;

	*status = 0;
	err = ov9760_read_reg(info->i2c_client, 0x002, status);
	return err;
}

static long ov9760_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err;
	struct ov9760_info *info = file->private_data;

	switch (cmd) {
	case OV9760_IOCTL_SET_MODE:
	{
		struct ov9760_mode mode;
		if (copy_from_user(&mode,
				   (const void __user *)arg,
				   sizeof(struct ov9760_mode))) {
			return -EFAULT;
		}

		return ov9760_set_mode(info, &mode);
	}
	case OV9760_IOCTL_SET_FRAME_LENGTH:
		return ov9760_set_frame_length(info, (u32)arg);
	case OV9760_IOCTL_SET_COARSE_TIME:
		return ov9760_set_coarse_time(info, (u32)arg);
	case OV9760_IOCTL_SET_GAIN:
		return ov9760_set_gain(info, (u16)arg);
	case OV9760_IOCTL_SET_GROUP_HOLD:
	{
		struct ov9760_ae ae;
		if (copy_from_user(&ae,
				(const void __user *)arg,
				sizeof(struct ov9760_ae))) {
			return -EFAULT;
		}
		return ov9760_set_group_hold(info, &ae);
	}
	case OV9760_IOCTL_GET_STATUS:
	{
		u8 status;

		err = ov9760_get_status(info, &status);
		if (err)
			return err;
		if (copy_to_user((void __user *)arg, &status,
				 2)) {
			return -EFAULT;
		}
		return 0;
	}
	case OV9760_IOCTL_GET_FUSEID:
	{
		err = ov9760_get_sensor_id(info);
		if (err)
			return err;
		if (copy_to_user((void __user *)arg,
				&info->sensor_data,
				sizeof(struct ov9760_sensordata))) {
			return -EFAULT;
		}
		return 0;
	}
	default:
		return -EINVAL;
	}
	return 0;
}

static int ov9760_open(struct inode *inode, struct file *file)
{
	struct miscdevice	*miscdev = file->private_data;
	struct ov9760_info	*info;

	info = container_of(miscdev, struct ov9760_info, miscdev_info);
	/* check if the device is in use */
	if (atomic_xchg(&info->in_use, 1)) {
		dev_info(&info->i2c_client->dev, "%s:BUSY!\n", __func__);
		return -EBUSY;
	}

	file->private_data = info;

	if (info->pdata && info->pdata->power_on) {
		ov9760_mclk_enable(info);
		info->pdata->power_on(&info->power);
	} else {
		dev_err(&info->i2c_client->dev,
			"%s:no valid power_on function.\n", __func__);
		return -EEXIST;
	}

	return 0;
}

int ov9760_release(struct inode *inode, struct file *file)
{
	struct ov9760_info *info = file->private_data;

	if (info->pdata && info->pdata->power_off) {
		ov9760_mclk_disable(info);
		info->pdata->power_off(&info->power);
	}
	file->private_data = NULL;

	/* warn if device is already released */
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}


static int ov9760_power_put(struct ov9760_power_rail *pw)
{
	if (likely(pw->avdd))
		regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		regulator_put(pw->iovdd);

	pw->dvdd = NULL;
	pw->avdd = NULL;
	pw->iovdd = NULL;

	return 0;
}

static int ov9760_regulator_get(struct ov9760_info *info,
	struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = regulator_get(&info->i2c_client->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
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

static int ov9760_power_get(struct ov9760_info *info)
{
	struct ov9760_power_rail *pw = &info->power;

	ov9760_regulator_get(info, &pw->avdd, "vana"); /* ananlog 2.7v */
	ov9760_regulator_get(info, &pw->iovdd, "vif"); /* interface 1.8v */
	return 0;
}


static const struct file_operations ov9760_fileops = {
	.owner = THIS_MODULE,
	.open = ov9760_open,
	.unlocked_ioctl = ov9760_ioctl,
	.release = ov9760_release,
};

static struct miscdevice ov9760_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ov9760",
	.fops = &ov9760_fileops,
};

#ifdef CONFIG_DEBUG_FS
static int ov9760_stats_show(struct seq_file *s, void *data)
{
	static struct ov9760_info *info;

	seq_printf(s, "%-20s : %-20s\n", "Name", "ov9760-debugfs-testing");
	seq_printf(s, "%-20s : 0x%X\n", "Current i2c-offset Addr",
			info->debug_i2c_offset);
	seq_printf(s, "%-20s : 0x%X\n", "DC BLC Enabled",
			info->debug_i2c_offset);
	return 0;
}

static int ov9760_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ov9760_stats_show, inode->i_private);
}

static const struct file_operations ov9760_stats_fops = {
	.open       = ov9760_stats_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static int debug_i2c_offset_w(void *data, u64 val)
{
	struct ov9760_info *info = (struct ov9760_info *)(data);
	dev_info(&info->i2c_client->dev,
			"ov9760:%s setting i2c offset to 0x%X\n",
			__func__, (u32)val);
	info->debug_i2c_offset = (u32)val;
	dev_info(&info->i2c_client->dev,
			"ov9760:%s new i2c offset is 0x%X\n", __func__,
			info->debug_i2c_offset);
	return 0;
}

static int debug_i2c_offset_r(void *data, u64 *val)
{
	struct ov9760_info *info = (struct ov9760_info *)(data);
	*val = (u64)info->debug_i2c_offset;
	dev_info(&info->i2c_client->dev,
			"ov9760:%s reading i2c offset is 0x%X\n", __func__,
			info->debug_i2c_offset);
	return 0;
}

static int debug_i2c_read(void *data, u64 *val)
{
	struct ov9760_info *info = (struct ov9760_info *)(data);
	u8 temp1 = 0;
	u8 temp2 = 0;
	dev_info(&info->i2c_client->dev,
			"ov9760:%s reading offset 0x%X\n", __func__,
			info->debug_i2c_offset);
	if (ov9760_read_reg(info->i2c_client,
				info->debug_i2c_offset, &temp1)
		|| ov9760_read_reg(info->i2c_client,
			info->debug_i2c_offset+1, &temp2)) {
		dev_err(&info->i2c_client->dev,
				"ov9760:%s failed\n", __func__);
		return -EIO;
	}
	dev_info(&info->i2c_client->dev,
			"ov9760:%s read value is 0x%X\n", __func__,
			temp1<<8 | temp2);
	*val = (u64)(temp1<<8 | temp2);
	return 0;
}

static int debug_i2c_write(void *data, u64 val)
{
	struct ov9760_info *info = (struct ov9760_info *)(data);
	dev_info(&info->i2c_client->dev,
			"ov9760:%s writing 0x%X to offset 0x%X\n", __func__,
			(u8)val, info->debug_i2c_offset);
	if (ov9760_write_reg(info->i2c_client,
				info->debug_i2c_offset, (u8)val)) {
		dev_err(&info->i2c_client->dev, "ov9760:%s failed\n", __func__);
		return -EIO;
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i2c_offset_fops, debug_i2c_offset_r,
		debug_i2c_offset_w, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(i2c_read_fops, debug_i2c_read,
		/*debug_i2c_dummy_w*/ NULL, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(i2c_write_fops, /*debug_i2c_dummy_r*/NULL,
		debug_i2c_write, "0x%llx\n");

static int ov9760_debug_init(struct ov9760_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s", __func__);

	info->debugfs_root = debugfs_create_dir(ov9760_device.name, NULL);

	if (!info->debugfs_root)
		goto err_out;

	if (!debugfs_create_file("stats", S_IRUGO,
			info->debugfs_root, info, &ov9760_stats_fops))
		goto err_out;

	if (!debugfs_create_file("offset", S_IRUGO | S_IWUSR,
			info->debugfs_root, info, &i2c_offset_fops))
		goto err_out;

	if (!debugfs_create_file("read", S_IRUGO,
			info->debugfs_root, info, &i2c_read_fops))
		goto err_out;

	if (!debugfs_create_file("write", S_IWUSR,
			info->debugfs_root, info, &i2c_write_fops))
		goto err_out;

	return 0;

err_out:
	dev_err(&info->i2c_client->dev, "ERROR:%s failed", __func__);
	debugfs_remove_recursive(info->debugfs_root);
	return -ENOMEM;
}
#endif

static int ov9760_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ov9760_info *info;
	const char *mclk_name;
	int err = 0;

	pr_info("ov9760: probing sensor.\n");

	info = devm_kzalloc(&client->dev,
		sizeof(struct ov9760_info), GFP_KERNEL);
	if (!info) {
		pr_err("ov9760:%s:Unable to allocate memory!\n", __func__);
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

	ov9760_power_get(info);

	memcpy(&info->miscdev_info,
		&ov9760_device,
		sizeof(struct miscdevice));

	err = misc_register(&info->miscdev_info);
	if (err) {
		ov9760_power_put(&info->power);
		pr_err("ov9760:%s:Unable to register misc device!\n",
		__func__);
	}

	i2c_set_clientdata(client, info);

#ifdef CONFIG_DEBUG_FS
	ov9760_debug_init(info);
#endif

	return err;
}

static int ov9760_remove(struct i2c_client *client)
{
	struct ov9760_info *info = i2c_get_clientdata(client);

	ov9760_power_put(&info->power);
	misc_deregister(&ov9760_device);
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(info->debugfs_root);
#endif
	return 0;
}

static const struct i2c_device_id ov9760_id[] = {
	{ "ov9760", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ov9760_id);

static struct i2c_driver ov9760_i2c_driver = {
	.driver = {
		.name = "ov9760",
		.owner = THIS_MODULE,
	},
	.probe = ov9760_probe,
	.remove = ov9760_remove,
	.id_table = ov9760_id,
};

static int __init ov9760_init(void)
{
	pr_info("ov9760 sensor driver loading\n");
	return i2c_add_driver(&ov9760_i2c_driver);
}

static void __exit ov9760_exit(void)
{
	i2c_del_driver(&ov9760_i2c_driver);
}

module_init(ov9760_init);
module_exit(ov9760_exit);
MODULE_LICENSE("GPL v2");
