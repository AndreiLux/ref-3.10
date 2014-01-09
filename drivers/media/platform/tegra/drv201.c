/*
 * drv201.c - a NVC kernel driver for focuser device drv201.
 *
 * Copyright (c) 2011-2013 NVIDIA Corporation. All rights reserved.
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
/* Implementation
 * --------------
 * The board level details about the device need to be provided in the board
 * file with the <device>_platform_data structure.
 * Standard among NVC kernel drivers in this structure is:
 * .cfg = Use the NVC_CFG_ defines that are in nvc.h.
 *  Descriptions of the configuration options are with the defines.
 *      This value is typically 0.
 * .num = The number of the instance of the device.  This should start at 1 and
 *      and increment for each device on the board.  This number will be
 *      appended to the MISC driver name, Example: /dev/focuser.1
 *      If not used or 0, then nothing is appended to the name.
 * .sync = If there is a need to synchronize two devices, then this value is
 *       the number of the device instance (.num above) this device is to
 *       sync to.  For example:
 *       Device 1 platform entries =
 *       .num = 1,
 *       .sync = 2,
 *       Device 2 platfrom entries =
 *       .num = 2,
 *       .sync = 1,
 *       The above example sync's device 1 and 2.
 *       To disable sync, set .sync = 0.  Note that the .num = 0 device is not
 *       allowed to be synced to.
 *       This is typically used for stereo applications.
 * .dev_name = The MISC driver name the device registers as.  If not used,
 *       then the part number of the device is used for the driver name.
 *       If using the NVC user driver then use the name found in this
 *       driver under _default_pdata.
 * .gpio_count = The ARRAY_SIZE of the nvc_gpio_pdata table.
 * .gpio = A pointer to the nvc_gpio_pdata structure's platform GPIO data.
 *       The GPIO mechanism works by cross referencing the .gpio_type key
 *       among the nvc_gpio_pdata GPIO data and the driver's nvc_gpio_init
 *       GPIO data to build a GPIO table the driver can use.  The GPIO's
 *       defined in the device header file's _gpio_type enum are the
 *       gpio_type keys for the nvc_gpio_pdata and nvc_gpio_init structures.
 *       These need to be present in the board file's nvc_gpio_pdata
 *       structure for the GPIO's that are used.
 *       The driver's GPIO logic uses assert/deassert throughout until the
 *       low level _gpio_wr/rd calls where the .assert_high is used to
 *       convert the value to the correct signal level.
 *       See the GPIO notes in nvc.h for additional information.
 *
 * The following is specific to NVC kernel focus drivers:
 * .nvc = Pointer to the nvc_focus_nvc structure.  This structure needs to
 *      be defined and populated if overriding the driver defaults.
 * .cap = Pointer to the nvc_focus_cap structure.  This structure needs to
 *      be defined and populated if overriding the driver defaults.
 *
 * Power Requirements:
 * The device's header file defines the voltage regulators needed with the
 * enumeration <device>_vreg.  The order these are enumerated is the order
 * the regulators will be enabled when powering on the device.  When the
 * device is powered off the regulators are disabled in descending order.
 * The <device>_vregs table in this driver uses the nvc_regulator_init
 * structure to define the regulator ID strings that go with the regulators
 * defined with <device>_vreg.  These regulator ID strings (or supply names)
 * will be used in the regulator_get function in the _vreg_init function.
 * The board power file and <device>_vregs regulator ID strings must match.
 */

#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <media/drv201.h>

#define DRV201_FOCAL_LENGTH_FLOAT	(4.570f)
#define DRV201_FNUMBER_FLOAT		(2.8f)
#define DRV201_FOCAL_LENGTH		(0x40923D71) /* 4.570f */
#define DRV201_FNUMBER			(0x40333333) /* 2.8f */
#define DRV201_ACTUATOR_RANGE	1023
#define DRV201_SETTLETIME		15
#define DRV201_FOCUS_MACRO		810
#define DRV201_FOCUS_INFINITY		50
#define DRV201_POS_LOW_DEFAULT		0
#define DRV201_POS_HIGH_DEFAULT		1023
#define DRV201_POS_CLAMP		0x03ff

#define DRV201_CONTROL_RING		0x0200BC


struct drv201_info {
	struct device *dev;
	struct i2c_client *i2c_client;
	struct drv201_platform_data *pdata;
	struct miscdevice miscdev;
	struct list_head list;
	struct drv201_power_rail power;
	struct nvc_focus_nvc nvc;
	struct nvc_focus_cap cap;
	struct nv_focuser_config nv_config;
	struct nv_focuser_config cfg_usr;
	atomic_t in_use;
	bool reset_flag;
	int pwr_dev;
	s32 pos;
	u16 dev_id;
};

/**
 * The following are default values
 */

static struct nvc_focus_cap drv201_default_cap = {
	.version = NVC_FOCUS_CAP_VER2,
	.actuator_range = DRV201_ACTUATOR_RANGE,
	.settle_time = DRV201_SETTLETIME,
	.focus_macro = DRV201_FOCUS_MACRO,
	.focus_infinity = DRV201_FOCUS_INFINITY,
	.focus_hyper = DRV201_FOCUS_INFINITY,
};

static struct nvc_focus_nvc drv201_default_nvc = {
	.focal_length = DRV201_FOCAL_LENGTH,
	.fnumber = DRV201_FNUMBER,
};

static struct drv201_platform_data drv201_default_pdata = {
	.cfg = 0,
	.num = 0,
	.sync = 0,
	.dev_name = "focuser",
};
static LIST_HEAD(drv201_info_list);
static DEFINE_SPINLOCK(drv201_spinlock);

static int drv201_i2c_wr8(struct drv201_info *info, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];
	buf[0] = reg;
	buf[1] = val;
	msg.addr = info->i2c_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = &buf[0];
	if (i2c_transfer(info->i2c_client->adapter, &msg, 1) != 1)
		return -EIO;
	return 0;
}

static int drv201_i2c_wr16(struct drv201_info *info, u8 reg, u16 val)
{
	struct i2c_msg msg;
	u8 buf[3];
	buf[0] = reg;
	buf[1] = (u8)(val >> 8);
	buf[2] = (u8)(val & 0xff);
	msg.addr = info->i2c_client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = &buf[0];
	if (i2c_transfer(info->i2c_client->adapter, &msg, 1) != 1)
		return -EIO;
	return 0;
}

void drv201_set_arc_mode(struct drv201_info *info)
{
	u32 sr = info->nv_config.slew_rate;
	int err;


	/* set ARC enable */
	err = drv201_i2c_wr8(info, CONTROL, (sr >> 16) & 0xff);
	if (err)
		dev_err(info->dev, "%s: CONTROL reg write failed\n", __func__);

	/* set the ARC RES mode */
	err = drv201_i2c_wr8(info, MODE, (sr >> 8) & 0xff);
	if (err)
		dev_err(info->dev, "%s: MODE reg write failed\n", __func__);

	/* set the VCM_FREQ */
	err = drv201_i2c_wr8(info, VCM_FREQ, sr & 0xff);
	if (err)
		dev_err(info->dev, "%s: VCM_FREQ reg write failed\n", __func__);
}

static int drv201_position_wr(struct drv201_info *info, u16 position)
{
	int err = drv201_i2c_wr16(
		info, VCM_CODE_MSB, position & DRV201_POS_CLAMP);

	if (!err)
		info->pos = position & DRV201_POS_CLAMP;
	return err;
}

static int drv201_pm_wr(struct drv201_info *info, int pwr)
{
	int err = 0;
	if ((info->pdata->cfg & (NVC_CFG_OFF2STDBY | NVC_CFG_BOOT_INIT)) &&
		(pwr == NVC_PWR_OFF || pwr == NVC_PWR_STDBY_OFF))
			pwr = NVC_PWR_STDBY;

	if (pwr == info->pwr_dev)
		return 0;

	switch (pwr) {
	case NVC_PWR_OFF_FORCE:
	case NVC_PWR_OFF:
		if (info->pdata && info->pdata->power_off)
			info->pdata->power_off(&info->power);
		break;
	case NVC_PWR_STDBY_OFF:
	case NVC_PWR_STDBY:
		if (info->pdata && info->pdata->power_off)
			info->pdata->power_off(&info->power);
		break;
	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		if (info->pdata && info->pdata->power_on)
			info->pdata->power_on(&info->power);
		usleep_range(1000, 1020);
		drv201_set_arc_mode(info);
		drv201_position_wr(info, (u16)info->nv_config.pos_working_low);
		break;
	default:
		err = -EINVAL;
		break;
	}

	if (err < 0) {
		dev_err(info->dev, "%s err %d\n", __func__, err);
		pwr = NVC_PWR_ERR;
	}

	info->pwr_dev = pwr;
	dev_dbg(info->dev, "%s pwr_dev=%d\n", __func__, info->pwr_dev);

	return err;
}

static int drv201_power_put(struct drv201_power_rail *pw)
{
	if (unlikely(!pw))
		return -EFAULT;

	if (likely(pw->vdd))
		regulator_put(pw->vdd);

	if (likely(pw->vdd_i2c))
		regulator_put(pw->vdd_i2c);

	pw->vdd = NULL;
	pw->vdd_i2c = NULL;

	return 0;
}

static int drv201_regulator_get(struct drv201_info *info,
	struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = regulator_get(info->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(info->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(info->dev, "%s: %s\n", __func__, vreg_name);

	*vreg = reg;
	return err;
}

static int drv201_power_get(struct drv201_info *info)
{
	struct drv201_power_rail *pw = &info->power;

	drv201_regulator_get(info, &pw->vdd, "avdd_cam1_cam");
	drv201_regulator_get(info, &pw->vdd_i2c, "pwrdet_sdmmc3");

	return 0;
}

static int drv201_pm_dev_wr(struct drv201_info *info, int pwr)
{
	if (pwr < info->pwr_dev)
		pwr = info->pwr_dev;
	return drv201_pm_wr(info, pwr);
}

static inline void drv201_pm_exit(struct drv201_info *info)
{
	drv201_pm_wr(info, NVC_PWR_OFF_FORCE);
	drv201_power_put(&info->power);
}

static inline void drv201_pm_init(struct drv201_info *info)
{
	drv201_power_get(info);
}

static int drv201_reset(struct drv201_info *info, u32 level)
{
	int err = 0;

	if (level == NVC_RESET_SOFT)
		err |= drv201_i2c_wr8(info, CONTROL, 0x01); /* SW reset */
	else
		err = drv201_pm_wr(info, NVC_PWR_OFF_FORCE);

	return err;
}

static void drv201_dump_focuser_capabilities(struct drv201_info *info)
{
	dev_dbg(info->dev, "%s:\n", __func__);
	dev_dbg(info->dev, "focal_length:               0x%x\n",
		info->nv_config.focal_length);
	dev_dbg(info->dev, "fnumber:                    0x%x\n",
		info->nv_config.fnumber);
	dev_dbg(info->dev, "max_aperture:               0x%x\n",
		info->nv_config.max_aperture);
	dev_dbg(info->dev, "pos_working_low:            %d\n",
		info->nv_config.pos_working_low);
	dev_dbg(info->dev, "pos_working_high:           %d\n",
		info->nv_config.pos_working_high);
	dev_dbg(info->dev, "pos_actual_low:             %d\n",
		info->nv_config.pos_actual_low);
	dev_dbg(info->dev, "pos_actual_high:            %d\n",
		info->nv_config.pos_actual_high);
	dev_dbg(info->dev, "slew_rate:                  0x%x\n",
		info->nv_config.slew_rate);
	dev_dbg(info->dev, "circle_of_confusion:        %d\n",
		info->nv_config.circle_of_confusion);
	dev_dbg(info->dev, "num_focuser_sets:           %d\n",
		info->nv_config.num_focuser_sets);
	dev_dbg(info->dev, "focuser_set[0].macro:       %d\n",
		info->nv_config.focuser_set[0].macro);
	dev_dbg(info->dev, "focuser_set[0].hyper:       %d\n",
		info->nv_config.focuser_set[0].hyper);
	dev_dbg(info->dev, "focuser_set[0].inf:         %d\n",
		info->nv_config.focuser_set[0].inf);
	dev_dbg(info->dev, "focuser_set[0].settle_time: %d\n",
		info->nv_config.focuser_set[0].settle_time);
}

static void drv201_get_focuser_capabilities(struct drv201_info *info)
{
	memset(&info->nv_config, 0, sizeof(info->nv_config));

	info->nv_config.focal_length = info->nvc.focal_length;
	info->nv_config.fnumber = info->nvc.fnumber;
	info->nv_config.max_aperture = info->nvc.fnumber;
	info->nv_config.range_ends_reversed = 0;

	info->nv_config.pos_working_low = info->cap.focus_infinity;
	info->nv_config.pos_working_high = info->cap.focus_macro;

	info->nv_config.pos_actual_low = DRV201_POS_LOW_DEFAULT;
	info->nv_config.pos_actual_high = DRV201_POS_HIGH_DEFAULT;

	info->nv_config.circle_of_confusion = -1;
	info->nv_config.num_focuser_sets = 1;
	info->nv_config.focuser_set[0].macro = info->cap.focus_macro;
	info->nv_config.focuser_set[0].hyper = info->cap.focus_hyper;
	info->nv_config.focuser_set[0].inf = info->cap.focus_infinity;
	info->nv_config.focuser_set[0].settle_time = info->cap.settle_time;

	/* set drive mode to linear */
	info->nv_config.slew_rate = DRV201_CONTROL_RING;

	drv201_dump_focuser_capabilities(info);
}

static int drv201_set_focuser_capabilities(struct drv201_info *info,
					struct nvc_param *params)
{
	if (copy_from_user(&info->cfg_usr,
		(const void __user *)params->p_value, sizeof(info->cfg_usr))) {
			dev_err(info->dev, "%s Err: copy_from_user bytes %d\n",
			__func__, sizeof(info->cfg_usr));
			return -EFAULT;
	}

	if (info->cfg_usr.focal_length)
		info->nv_config.focal_length = info->cfg_usr.focal_length;
	if (info->cfg_usr.fnumber)
		info->nv_config.fnumber = info->cfg_usr.fnumber;
	if (info->cfg_usr.max_aperture)
		info->nv_config.max_aperture = info->cfg_usr.max_aperture;

	if (info->cfg_usr.pos_working_low != AF_POS_INVALID_VALUE)
		info->nv_config.pos_working_low = info->cfg_usr.pos_working_low;
	if (info->cfg_usr.pos_working_high != AF_POS_INVALID_VALUE)
		info->nv_config.pos_working_high =
			info->cfg_usr.pos_working_high;
	if (info->cfg_usr.pos_actual_low != AF_POS_INVALID_VALUE)
		info->nv_config.pos_actual_low = info->cfg_usr.pos_actual_low;
	if (info->cfg_usr.pos_actual_high != AF_POS_INVALID_VALUE)
		info->nv_config.pos_actual_high = info->cfg_usr.pos_actual_high;

	if (info->cfg_usr.circle_of_confusion != AF_POS_INVALID_VALUE)
		info->nv_config.circle_of_confusion =
			info->cfg_usr.circle_of_confusion;

	if (info->cfg_usr.focuser_set[0].macro != AF_POS_INVALID_VALUE)
		info->nv_config.focuser_set[0].macro =
			info->cfg_usr.focuser_set[0].macro;
	if (info->cfg_usr.focuser_set[0].hyper != AF_POS_INVALID_VALUE)
		info->nv_config.focuser_set[0].hyper =
			info->cfg_usr.focuser_set[0].hyper;
	if (info->cfg_usr.focuser_set[0].inf != AF_POS_INVALID_VALUE)
		info->nv_config.focuser_set[0].inf =
			info->cfg_usr.focuser_set[0].inf;
	if (info->cfg_usr.focuser_set[0].settle_time != AF_POS_INVALID_VALUE)
		info->nv_config.focuser_set[0].settle_time =
			info->cfg_usr.focuser_set[0].settle_time;

	dev_dbg(info->dev,
		"%s: copy_from_user bytes %d info->cap.settle_time %d\n",
		__func__, sizeof(struct nv_focuser_config),
		info->cap.settle_time);

	drv201_dump_focuser_capabilities(info);
	return 0;
}

static int drv201_param_rd(struct drv201_info *info, unsigned long arg)
{
	struct nvc_param params;
	const void *data_ptr = NULL;
	u32 data_size = 0;
	int err = 0;

	if (copy_from_user(&params,
		(const void __user *)arg,
		sizeof(struct nvc_param))) {
		dev_err(info->dev, "%s %d copy_from_user err\n",
			__func__, __LINE__);
		return -EFAULT;
	}

	switch (params.param) {
	case NVC_PARAM_LOCUS:
		data_ptr = &info->pos;
		data_size = sizeof(info->pos);
		dev_dbg(info->dev, "%s LOCUS: %d\n", __func__, info->pos);
		break;
	case NVC_PARAM_FOCAL_LEN:
		data_ptr = &info->nv_config.focal_length;
		data_size = sizeof(info->nv_config.focal_length);
		break;
	case NVC_PARAM_MAX_APERTURE:
		data_ptr = &info->nv_config.max_aperture;
		data_size = sizeof(info->nv_config.max_aperture);
		dev_dbg(info->dev, "%s MAX_APERTURE: %x\n",
			__func__, info->nv_config.max_aperture);
		break;
	case NVC_PARAM_FNUMBER:
		data_ptr = &info->nv_config.fnumber;
		data_size = sizeof(info->nv_config.fnumber);
		dev_dbg(info->dev, "%s FNUMBER: %u\n",
			__func__, info->nv_config.fnumber);
		break;
	case NVC_PARAM_CAPS:
		/* send back just what's requested or our max size */
		data_ptr = &info->nv_config;
		data_size = sizeof(info->nv_config);
		dev_err(info->dev, "%s CAPS\n", __func__);
		break;
	case NVC_PARAM_STS:
		/*data_ptr = &info->sts;
		data_size = sizeof(info->sts);*/
		dev_dbg(info->dev, "%s\n", __func__);
		break;
	case NVC_PARAM_STEREO:
	default:
		dev_err(info->dev, "%s unsupported parameter: %d\n",
			__func__, params.param);
		err = -EINVAL;
		break;
	}
	if (!err && params.sizeofvalue < data_size) {
		dev_err(info->dev,
			"%s data size mismatch %d != %d Param: %d\n",
			__func__, params.sizeofvalue, data_size, params.param);
		return -EINVAL;
	}
	if (!err && copy_to_user((void __user *)params.p_value,
		data_ptr, data_size)) {
		dev_err(info->dev,
			"%s copy_to_user err line %d\n", __func__, __LINE__);
		return -EFAULT;
	}
	return err;
}

static int drv201_param_wr(struct drv201_info *info, unsigned long arg)
{
	struct nvc_param params;
	u8 u8val;
	u32 u32val;
	int err = 0;

	if (copy_from_user(&params, (const void __user *)arg,
		sizeof(struct nvc_param))) {
		dev_err(info->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	if (copy_from_user(&u32val,
		(const void __user *)params.p_value, sizeof(u32val))) {
		dev_err(info->dev, "%s %d copy_from_user err\n",
			__func__, __LINE__);
		return -EFAULT;
	}
	u8val = (u8)u32val;

	/* parameters independent of sync mode */
	switch (params.param) {
	case NVC_PARAM_CAPS:
		if (drv201_set_focuser_capabilities(info, &params)) {
			dev_err(info->dev,
				"%s: Error: copy_from_user bytes %d\n",
				__func__, params.sizeofvalue);
			err = -EFAULT;
		}
		break;
	case NVC_PARAM_LOCUS:
		dev_dbg(info->dev, "%s LOCUS: %d\n", __func__, u32val);
		err = drv201_position_wr(info, (u16)u32val);
		break;
	case NVC_PARAM_RESET:
		err = drv201_reset(info, u32val);
		dev_dbg(info->dev, "%s RESET: %d\n", __func__, err);
		break;
	case NVC_PARAM_SELF_TEST:
		err = 0;
		dev_dbg(info->dev, "%s SELF_TEST: %d\n", __func__, err);
		break;
	default:
		dev_dbg(info->dev, "%s unsupported parameter: %d\n",
			__func__, params.param);
		err = -EINVAL;
		break;
	}
	return err;
}

static long drv201_ioctl(struct file *file,
					unsigned int cmd,
					unsigned long arg)
{
	struct drv201_info *info = file->private_data;
	int pwr;
	int err = 0;
	switch (cmd) {
	case NVC_IOCTL_PARAM_WR:
		drv201_pm_dev_wr(info, NVC_PWR_ON);
		err = drv201_param_wr(info, arg);
		drv201_pm_dev_wr(info, NVC_PWR_OFF);
		return err;
	case NVC_IOCTL_PARAM_RD:
		drv201_pm_dev_wr(info, NVC_PWR_ON);
		err = drv201_param_rd(info, arg);
		drv201_pm_dev_wr(info, NVC_PWR_OFF);
		return err;
	case NVC_IOCTL_PWR_WR:
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(info->dev, "%s PWR_WR: %d\n", __func__, pwr);
		err = drv201_pm_wr(info, pwr);
		return err;
	case NVC_IOCTL_PWR_RD:
		pwr = info->pwr_dev;
		dev_dbg(info->dev, "%s PWR_RD: %d\n", __func__, pwr);
		if (copy_to_user((void __user *)arg,
			(const void *)&pwr, sizeof(pwr))) {
			dev_err(info->dev, "%s copy_to_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		return 0;
	default:
		dev_dbg(info->dev, "%s unsupported ioctl: %x\n", __func__, cmd);
	}
	return -EINVAL;
}


static void drv201_sdata_init(struct drv201_info *info)
{
	/* set defaults */
	memcpy(&info->nvc, &drv201_default_nvc, sizeof(info->nvc));
	memcpy(&info->cap, &drv201_default_cap, sizeof(info->cap));

	/* set to proper value */
	info->cap.actuator_range =
		DRV201_POS_HIGH_DEFAULT - DRV201_POS_LOW_DEFAULT;

	/* set overrides if any */
	if (info->pdata->nvc) {
		if (info->pdata->nvc->fnumber)
			info->nvc.fnumber = info->pdata->nvc->fnumber;
		if (info->pdata->nvc->focal_length)
			info->nvc.focal_length = info->pdata->nvc->focal_length;
		if (info->pdata->nvc->max_aperature)
			info->nvc.max_aperature =
				info->pdata->nvc->max_aperature;
	}

	if (info->pdata->cap) {
		if (info->pdata->cap->actuator_range)
			info->cap.actuator_range =
				info->pdata->cap->actuator_range;
		if (info->pdata->cap->settle_time)
			info->cap.settle_time = info->pdata->cap->settle_time;
		if (info->pdata->cap->focus_macro)
			info->cap.focus_macro = info->pdata->cap->focus_macro;
		if (info->pdata->cap->focus_hyper)
			info->cap.focus_hyper = info->pdata->cap->focus_hyper;
		if (info->pdata->cap->focus_infinity)
			info->cap.focus_infinity =
				info->pdata->cap->focus_infinity;
	}

	drv201_get_focuser_capabilities(info);
}

static int drv201_open(struct inode *inode, struct file *file)
{
	struct drv201_info *info = NULL;
	struct drv201_info *pos = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &drv201_info_list, list) {
		if (pos->miscdev.minor == iminor(inode)) {
			info = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (!info)
		return -ENODEV;

	if (atomic_xchg(&info->in_use, 1))
		return -EBUSY;
	file->private_data = info;

	dev_dbg(info->dev, "%s\n", __func__);
	return 0;
}

static int drv201_release(struct inode *inode, struct file *file)
{
	struct drv201_info *info = file->private_data;
	dev_dbg(info->dev, "%s\n", __func__);
	drv201_pm_wr(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static const struct file_operations drv201_fileops = {
	.owner = THIS_MODULE,
	.open = drv201_open,
	.unlocked_ioctl = drv201_ioctl,
	.release = drv201_release,
};

static void drv201_del(struct drv201_info *info)
{
	drv201_pm_exit(info);
	spin_lock(&drv201_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&drv201_spinlock);
	synchronize_rcu();
}

static int drv201_remove(struct i2c_client *client)
{
	struct drv201_info *info = i2c_get_clientdata(client);
	dev_dbg(info->dev, "%s\n", __func__);
	misc_deregister(&info->miscdev);
	drv201_del(info);
	return 0;
}

static int drv201_probe(
		struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct drv201_info *info;
	char dname[16];
	int err = 0;

	dev_dbg(&client->dev, "%s\n", __func__);
	pr_info("drv201: probing focuser.\n");

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}
	info->i2c_client = client;
	info->dev = &client->dev;
	if (client->dev.platform_data)
		info->pdata = client->dev.platform_data;
	else {
		info->pdata = &drv201_default_pdata;
		dev_dbg(info->dev, "%s No platform data. Using defaults.\n",
			__func__);
	}

	i2c_set_clientdata(client, info);
	INIT_LIST_HEAD(&info->list);
	spin_lock(&drv201_spinlock);
	list_add_rcu(&info->list, &drv201_info_list);
	spin_unlock(&drv201_spinlock);
	drv201_pm_init(info);

	if (info->pdata->cfg & (NVC_CFG_NODEV | NVC_CFG_BOOT_INIT)) {
		drv201_pm_wr(info, NVC_PWR_COMM);
		drv201_pm_wr(info, NVC_PWR_OFF);
		if (err < 0) {
			dev_err(info->dev, "%s device not found\n",
				__func__);
			if (info->pdata->cfg & NVC_CFG_NODEV) {
				drv201_del(info);
				return -ENODEV;
			}
		} else {
			dev_dbg(info->dev, "%s device found\n", __func__);
			if (info->pdata->cfg & NVC_CFG_BOOT_INIT) {
				/* initial move causes full initialization */
				drv201_pm_wr(info, NVC_PWR_ON);
				drv201_position_wr(info,
					(u16)info->nv_config.pos_working_low);
				drv201_pm_wr(info, NVC_PWR_OFF);
			}
		}
	}

	drv201_sdata_init(info);

	if (info->pdata->dev_name != 0)
		strcpy(dname, info->pdata->dev_name);
	else
		strcpy(dname, "drv201");

	if (info->pdata->num)
		snprintf(dname, sizeof(dname),
			"%s.%u", dname, info->pdata->num);

	info->miscdev.name = dname;
	info->miscdev.fops = &drv201_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(info->dev, "%s unable to register misc device %s\n",
			__func__, dname);
		drv201_del(info);
		return -ENODEV;
	}

	return 0;
}


static const struct i2c_device_id drv201_id[] = {
	{ "drv201", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, drv201_id);

static struct i2c_driver drv201_i2c_driver = {
	.driver = {
		.name = "drv201",
		.owner = THIS_MODULE,
	},
	.id_table = drv201_id,
	.probe = drv201_probe,
	.remove = drv201_remove,
};

module_i2c_driver(drv201_i2c_driver);
MODULE_LICENSE("GPL v2");
