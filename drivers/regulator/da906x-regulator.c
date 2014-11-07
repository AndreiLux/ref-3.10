/*
 * Regulator driver for DA906x PMIC series
 *
 * Copyright 2012 Dialog Semiconductors Ltd.
 *
 * Author: Krystian Garbaciak <krystian.garbaciak@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/da906x/core.h>
#include <linux/mfd/da906x/pdata.h>
#include <linux/mfd/da906x/registers.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/regulator/of_regulator.h>
#endif

/* Definition for registering bit fields */
struct bfield {
	unsigned short	addr;
	unsigned char	mask;
};
#define BFIELD(_addr, _mask) \
	{ .addr = _addr, .mask = _mask }

struct field {
	unsigned short	addr;
	unsigned char	mask;
	unsigned char	shift;
	unsigned char	offset;
};
#define FIELD(_addr, _mask, _shift, _offset) \
	{ .addr = _addr, .mask = _mask, .shift = _shift, .offset = _offset }

/* Regulator capabilities and registers description */
struct da906x_regulator_info {
	int			id;
	char			*name;
	struct regulator_ops	*ops;

	/* Voltage adjust range */
	int		min_uv;
	int		max_uv;
	unsigned	step_uv;
	unsigned	n_steps;

	/* Current limiting */
	unsigned	n_current_limits;
	const int	*current_limits;

	/* DA906x main register fields */
	struct bfield	enable;		/* bit used to enable regulator,
					   it returns actual state when read */
	struct field	mode;		/* buck mode of operation */
	struct bfield	suspend;
	struct bfield	sleep;
	struct bfield	suspend_sleep;
	struct field	voltage;
	struct field	suspend_voltage;
	struct field	ilimit;

	/* DA906x event detection bit */
	struct bfield	oc_event;
};

/* Macro for switch regulator */
#define DA906X_SWITCH(chip, regl_name) \
	.id = chip##_ID_##regl_name, \
	.name = __stringify(chip##_##regl_name), \
	.ops = &da906x_switch_ops, \
	.n_steps = 0

/* Macros for LDO */
#define DA906X_LDO(chip, regl_name, min_mV, step_mV, max_mV) \
	.id = chip##_ID_##regl_name, \
	.name = __stringify(chip##_##regl_name), \
	.ops = &da906x_ldo_ops, \
	.min_uv = (min_mV) * 1000, \
	.max_uv = (max_mV) * 1000, \
	.step_uv = (step_mV) * 1000, \
	.n_steps = (((max_mV) - (min_mV))/(step_mV) + 1)

#define DA906X_LDO_COMMON_FIELDS(regl_name) \
	.enable = BFIELD(DA906X_REG_##regl_name##_CONT, DA906X_LDO_EN), \
	.sleep = BFIELD(DA906X_REG_V##regl_name##_A, DA906X_LDO_SL), \
	.suspend_sleep = BFIELD(DA906X_REG_V##regl_name##_B, DA906X_LDO_SL), \
	.voltage = FIELD(DA906X_REG_V##regl_name##_A, \
			DA906X_V##regl_name##_MASK, \
			DA906X_V##regl_name##_SHIFT, \
			DA906X_V##regl_name##_BIAS), \
	.suspend_voltage = FIELD(DA906X_REG_V##regl_name##_B, \
			DA906X_V##regl_name##_MASK,\
			DA906X_V##regl_name##_SHIFT, \
			DA906X_V##regl_name##_BIAS)

/* Macros for voltage DC/DC converters (BUCKs) */
#define DA906X_BUCK(chip, regl_name, min_mV, step_mV, max_mV, limits_array) \
	.id = chip##_ID_##regl_name, \
	.name = __stringify(chip##_##regl_name), \
	.ops = &da906x_buck_ops, \
	.min_uv = (min_mV) * 1000, \
	.max_uv = (max_mV) * 1000, \
	.step_uv = (step_mV) * 1000, \
	.n_steps = ((max_mV) - (min_mV))/(step_mV) + 1, \
	.current_limits = limits_array, \
	.n_current_limits = ARRAY_SIZE(limits_array)

#define DA906X_BUCK_COMMON_FIELDS(regl_name) \
	.enable = BFIELD(DA906X_REG_##regl_name##_CONT, DA906X_BUCK_EN), \
	.sleep = BFIELD(DA906X_REG_V##regl_name##_A, DA906X_BUCK_SL), \
	.suspend_sleep = BFIELD(DA906X_REG_V##regl_name##_B, DA906X_BUCK_SL), \
	.voltage = FIELD(DA906X_REG_V##regl_name##_A, \
			 DA906X_VBUCK_MASK, \
			 DA906X_VBUCK_SHIFT, \
			 DA906X_VBUCK_BIAS), \
	.suspend_voltage = FIELD(DA906X_REG_V##regl_name##_B, \
				 DA906X_VBUCK_MASK,\
				 DA906X_VBUCK_SHIFT, \
				 DA906X_VBUCK_BIAS), \
	.mode = FIELD(DA906X_REG_##regl_name##_CFG, DA906X_BUCK_MODE_MASK, \
		      DA906X_BUCK_MODE_SHIFT, 0)

/* Defines asignment of regulators info table to chip model */
struct da906x_dev_model {
	const struct da906x_regulator_info	*regulator_info;
	unsigned				n_regulators;
	unsigned				dev_model;
};

/* Single regulator settings */
struct da906x_regulator {
	struct regulator_desc			desc;
	struct regulator_dev			*rdev;
	struct da906x				*hw;
	const struct da906x_regulator_info	*info;

	unsigned long				flags;
	unsigned				mode;
	unsigned				suspend_mode;
	struct da906x_regulator_data *da906x_regulator_info;
	struct da906x_regulators_pdata *regulator_data;
	struct device_node *of_node;
	int n_regulators;
};

/* Encapsulates all information for the regulators driver */
struct da906x_regulators {
	int					irq_ldo_lim;
	int					irq_uvov;
	int					irq_da9210_oc;
	int					irq_da9210_temp;

	unsigned				n_regulators;
	/* Array size to be defined during init. Keep at end. */
	struct da906x_regulator			regulator[0];
};

/* System states for da906x_update_mode_internal()
   and for da906x_get_mode_internal() */
enum {
	SYS_STATE_NORMAL,
	SYS_STATE_SUSPEND,
	SYS_STATE_CURRENT
};

/* Regulator operations */
static int da906x_list_voltage(struct regulator_dev *rdev, unsigned selector);
static int da906x_set_voltage(struct regulator_dev *rdev, int min_uv,
			      int max_uv, unsigned *selector);
static int da906x_get_voltage_sel(struct regulator_dev *rdev);
static int da9210_get_voltage_sel(struct regulator_dev *rdev);
static int da906x_set_current_limit(struct regulator_dev *rdev,
				    int min_uA, int max_uA);
static int da906x_get_current_limit(struct regulator_dev *rdev);
static int da9210_set_current_limit(struct regulator_dev *rdev,
				    int min_uA, int max_uA);
static int da9210_get_current_limit(struct regulator_dev *rdev);
static int da906x_enable(struct regulator_dev *rdev);
static int da9210_enable(struct regulator_dev *rdev);
static int da906x_disable(struct regulator_dev *rdev);
static int da906x_is_enabled(struct regulator_dev *rdev);
static int da906x_set_mode(struct regulator_dev *rdev, unsigned int mode);
static unsigned da906x_get_mode(struct regulator_dev *rdev);
static int da906x_get_status(struct regulator_dev *rdev);
static int da906x_set_suspend_voltage(struct regulator_dev *rdev, int uv);
static int da906x_suspend_enable(struct regulator_dev *rdev);
static int da906x_set_suspend_mode(struct regulator_dev *rdev, unsigned mode);

static struct regulator_ops da906x_switch_ops = {
	.enable			= da906x_enable,
	.disable		= da906x_disable,
	.is_enabled		= da906x_is_enabled,
	.set_suspend_enable	= da906x_enable,
	.set_suspend_disable	= da906x_disable,
};

static struct regulator_ops da906x_ldo_ops = {
	.enable			= da906x_enable,
	.disable		= da906x_disable,
	.is_enabled		= da906x_is_enabled,
	.set_voltage		= da906x_set_voltage,
	.get_voltage_sel	= da906x_get_voltage_sel,
	.list_voltage		= da906x_list_voltage,
	.set_mode		= da906x_set_mode,
	.get_mode		= da906x_get_mode,
	.get_status		= da906x_get_status,
	.set_suspend_voltage	= da906x_set_suspend_voltage,
	.set_suspend_enable	= da906x_suspend_enable,
	.set_suspend_disable	= da906x_disable,
	.set_suspend_mode	= da906x_set_suspend_mode,
};

static struct regulator_ops da906x_buck_ops = {
	.enable			= da906x_enable,
	.disable		= da906x_disable,
	.is_enabled		= da906x_is_enabled,
	.set_voltage		= da906x_set_voltage,
	.get_voltage_sel	= da906x_get_voltage_sel,
	.list_voltage		= da906x_list_voltage,
	.set_current_limit	= da906x_set_current_limit,
	.get_current_limit	= da906x_get_current_limit,
	.set_mode		= da906x_set_mode,
	.get_mode		= da906x_get_mode,
	.get_status		= da906x_get_status,
	.set_suspend_voltage	= da906x_set_suspend_voltage,
	.set_suspend_enable	= da906x_suspend_enable,
	.set_suspend_disable	= da906x_disable,
	.set_suspend_mode	= da906x_set_suspend_mode,
};

/* DA9210 supports external DVC control interface, that overrides voltage set
   by software interface.
   When it is enabled, we shall return current voltage based on it's setting.
   Voltage set by regulator_set_voltage is only relevant just after
   regulator enable and before first external DVC voltage request. */
static struct regulator_ops da9210_buck_ops = {
	.enable			= da9210_enable,
	.disable		= da906x_disable,
	.is_enabled		= da906x_is_enabled,
	.set_voltage		= da906x_set_voltage,
	.get_voltage_sel	= da9210_get_voltage_sel,
	.list_voltage		= da906x_list_voltage,
	.set_current_limit	= da9210_set_current_limit,
	.get_current_limit	= da9210_get_current_limit,
	.set_mode		= da906x_set_mode,
	.get_mode		= da906x_get_mode,
	.get_status		= da906x_get_status,
	.set_suspend_voltage	= da906x_set_suspend_voltage,
	.set_suspend_enable	= da906x_suspend_enable,
	.set_suspend_disable	= da906x_disable,
	.set_suspend_mode	= da906x_set_suspend_mode,
};

/* Current limits array (in uA) for BCORE1, BCORE2, BPRO.
   Entry indexes corresponds to register values. */
static const int da9063_buck_a_limits[] = {
	 500000,  600000,  700000,  800000,  900000, 1000000, 1100000, 1200000,
	1300000, 1400000, 1500000, 1600000, 1700000, 1800000, 1900000, 2000000
};

/* Current limits array (in uA) for BMEM, BIO, BPERI.
   Entry indexes corresponds to register values. */
static const int da9063_buck_b_limits[] = {
	1500000, 1600000, 1700000, 1800000, 1900000, 2000000, 2100000, 2200000,
	2300000, 2400000, 2500000, 2600000, 2700000, 2800000, 2900000, 3000000
};

/* Current limits array (in uA) for merged BCORE1 and BCORE2.
   Entry indexes corresponds to register values. */
static const int da9063_bcores_merged_limits[] = {
	1000000, 1200000, 1400000, 1600000, 1800000, 2000000, 2200000, 2400000,
	2600000, 2800000, 3000000, 3200000, 3400000, 3600000, 3800000, 4000000
};

/* Current limits array (in uA) for merged BMEM and BIO.
   Entry indexes corresponds to register values. */
static const int da9063_bmem_bio_merged_limits[] = {
	3000000, 3200000, 3400000, 3600000, 3800000, 4000000, 4200000, 4400000,
	4600000, 4800000, 5000000, 5200000, 5400000, 5600000, 5800000, 6000000
};

/* Current limits array (in uA) for DA9210 buck per one phase.
   Multiply by configured phase number to get total buck limitation.
   Entry indexes corresponds to register values. */
static const int da9210_buck_limits[] = {
	1600000, 1800000, 2000000, 2200000, 2400000, 2600000, 2800000, 3000000,
	3200000, 3400000, 3600000, 3800000, 4000000, 4200000, 4400000, 4600000
};

/* Info of regulators for DA9063 */
static const struct da906x_regulator_info da9063_regulator_info[] = {
	{
		DA906X_BUCK(DA9063, BCORE1, 300, 10, 1570,
			    da9063_buck_a_limits),
		DA906X_BUCK_COMMON_FIELDS(BCORE1),
		.ilimit = FIELD(DA906X_REG_BUCK_ILIM_C, DA906X_BCORE1_ILIM_MASK,
				DA906X_BCORE1_ILIM_SHIFT, 0),
	},
	{
		DA906X_BUCK(DA9063, BCORE2, 300, 10, 1570,
			    da9063_buck_a_limits),
		DA906X_BUCK_COMMON_FIELDS(BCORE2),

		.ilimit = FIELD(DA906X_REG_BUCK_ILIM_C,
				DA906X_BCORE2_ILIM_MASK,
				DA906X_BCORE2_ILIM_SHIFT,
				0),
	},
	{
		DA906X_BUCK(DA9063, BPRO, 530, 10, 1800,
			    da9063_buck_a_limits),
		DA906X_BUCK_COMMON_FIELDS(BPRO),
		.ilimit = FIELD(DA906X_REG_BUCK_ILIM_B, DA906X_BPRO_ILIM_MASK,
				DA906X_BPRO_ILIM_SHIFT, 0),
	},
	{
		DA906X_BUCK(DA9063, BMEM, 800, 20, 3340,
			    da9063_buck_b_limits),
		DA906X_BUCK_COMMON_FIELDS(BMEM),
		.ilimit = FIELD(DA906X_REG_BUCK_ILIM_A, DA906X_BMEM_ILIM_MASK,
				DA906X_BMEM_ILIM_SHIFT, 0),
	},
	{
		DA906X_BUCK(DA9063, BIO, 800, 20, 3340,
			    da9063_buck_b_limits),
		DA906X_BUCK_COMMON_FIELDS(BIO),
		.ilimit = FIELD(DA906X_REG_BUCK_ILIM_A, DA906X_BIO_ILIM_MASK,
				DA906X_BIO_ILIM_SHIFT, 0),
	},
	{
		DA906X_BUCK(DA9063, BPERI, 800, 20, 3340,
			    da9063_buck_b_limits),
		DA906X_BUCK_COMMON_FIELDS(BPERI),
		.ilimit = FIELD(DA906X_REG_BUCK_ILIM_B, DA906X_BPERI_ILIM_MASK,
				DA906X_BPERI_ILIM_SHIFT, 0),
	},
	{
		DA906X_LDO(DA9063, LDO1, 600, 20, 1860),
		DA906X_LDO_COMMON_FIELDS(LDO1),
	},
	{
		DA906X_LDO(DA9063, LDO2, 600, 20, 1860),
		DA906X_LDO_COMMON_FIELDS(LDO2),
	},
	{
		DA906X_LDO(DA9063, LDO3, 900, 20, 3440),
		DA906X_LDO_COMMON_FIELDS(LDO3),
		.oc_event = BFIELD(DA906X_REG_STATUS_D, DA906X_LDO3_LIM),
	},
	{
		DA906X_LDO(DA9063, LDO4, 900, 20, 3440),
		DA906X_LDO_COMMON_FIELDS(LDO4),
		.oc_event = BFIELD(DA906X_REG_STATUS_D, DA906X_LDO4_LIM),
	},
	{
		DA906X_LDO(DA9063, LDO5, 900, 50, 3600),
		DA906X_LDO_COMMON_FIELDS(LDO5),
	},
	{
		DA906X_LDO(DA9063, LDO6, 900, 50, 3600),
		DA906X_LDO_COMMON_FIELDS(LDO6),
	},
	{
		DA906X_LDO(DA9063, LDO7, 900, 50, 3600),
		DA906X_LDO_COMMON_FIELDS(LDO7),
		.oc_event = BFIELD(DA906X_REG_STATUS_D, DA906X_LDO7_LIM),
	},
	{
		DA906X_LDO(DA9063, LDO8, 900, 50, 3600),
		DA906X_LDO_COMMON_FIELDS(LDO8),
		.oc_event = BFIELD(DA906X_REG_STATUS_D, DA906X_LDO8_LIM),
	},
	{
		DA906X_LDO(DA9063, LDO9, 950, 50, 3600),
		DA906X_LDO_COMMON_FIELDS(LDO9),
	},
	{
		DA906X_LDO(DA9063, LDO10, 900, 50, 3600),
		DA906X_LDO_COMMON_FIELDS(LDO10),
	},
	{
		DA906X_LDO(DA9063, LDO11, 900, 50, 3600),
		DA906X_LDO_COMMON_FIELDS(LDO11),
		.oc_event = BFIELD(DA906X_REG_STATUS_D, DA906X_LDO11_LIM),
	},
	{
		DA906X_SWITCH(DA9063, 32K_OUT),
		.enable = BFIELD(DA906X_REG_EN_32K, DA906X_OUT_32K_EN),
	},
};

/* CoPMIC DA9210 info */
static const struct da906x_regulator_info da9210_regulator_info = {
	.id = DA9210_ID_BUCK,
	.name = "DA9210_BUCK",
	.ops = &da9210_buck_ops,
	.min_uv = 300000,
	.max_uv = 1570000,
	.step_uv = 10000,
	.n_steps = ((1570000 - 300000)/10000 + 1),
	.current_limits = da9210_buck_limits,
	.n_current_limits = ARRAY_SIZE(da9210_buck_limits),

	.enable = BFIELD(DA9210_REG_BUCK_CONT, DA9210_BUCK_EN),
	.sleep = BFIELD(DA9210_REG_VBUCK_A, DA9210_BUCK_SL),
	.suspend_sleep = BFIELD(DA9210_REG_VBUCK_B, DA9210_BUCK_SL),
	.voltage = FIELD(DA9210_REG_VBUCK_A,
			 DA9210_VBUCK_MASK,
			 DA9210_VBUCK_SHIFT,
			 DA9210_VBUCK_BIAS),
	.suspend_voltage = FIELD(DA9210_REG_VBUCK_B,
				 DA9210_VBUCK_MASK,
				 DA9210_VBUCK_SHIFT,
				 DA9210_VBUCK_BIAS),
	.mode = FIELD(DA9210_REG_BUCK_CONF1, DA9210_BUCK_MODE_MASK,
		      DA9210_BUCK_MODE_SHIFT, 0),
	.suspend = BFIELD(DA9210_REG_BUCK_CONT, DA9210_VBUCK_SEL),
	.ilimit = FIELD(DA9210_REG_BUCK_ILIM, DA9210_BUCK_ILIM_MASK,
			DA9210_BUCK_ILIM_SHIFT, 0)
};

/* Link chip model with regulators info table */
static struct da906x_dev_model regulators_models[] = {
	{
		.regulator_info = da9063_regulator_info,
		.n_regulators = ARRAY_SIZE(da9063_regulator_info),
		.dev_model = PMIC_DA9063,
	},
	{NULL, 0, 0}	/* End of list */
};


/*
 * Regulator internal functions
 */
static int da906x_sel_to_vol(struct da906x_regulator *regl, unsigned selector)
{
	return regl->info->step_uv * selector + regl->info->min_uv;
}

static unsigned da906x_min_val_to_sel(int in, int base, int step)
{
	if (step == 0)
		return 0;

	return (in - base + step - 1) / step;
}

static int da906x_update_mode_internal(struct da906x_regulator *regl,
				       int sys_state)
{
	const struct da906x_regulator_info *rinfo = regl->info;
	unsigned val;
	unsigned mode;
	int ret;

	if (sys_state == SYS_STATE_SUSPEND)
		mode = regl->suspend_mode;
	else
		mode = regl->mode;

	if (rinfo->mode.addr) {
		/* Set mode for BUCK - 3 modes are supported */
		switch (mode) {
		case REGULATOR_MODE_FAST:
			val = DA906X_BUCK_MODE_SYNC;
			break;
		case REGULATOR_MODE_NORMAL:
			val = DA906X_BUCK_MODE_AUTO;
			break;
		case REGULATOR_MODE_STANDBY:
			val = DA906X_BUCK_MODE_SLEEP;
			break;
		default:
			return -EINVAL;
		}

		ret = da906x_reg_update(regl->hw, rinfo->mode.addr,
						rinfo->mode.mask, val);
	} else {
		/* Set mode for LDO - 2 modes are supported */
		switch (mode) {
		case REGULATOR_MODE_NORMAL:
			val = 0;
			break;
		case REGULATOR_MODE_STANDBY:
			val = DA906X_LDO_SL;
			break;
		default:
			return -EINVAL;
		}

		if (sys_state == SYS_STATE_SUSPEND) {
			if (!rinfo->suspend_sleep.addr)
				return -EINVAL;
			ret = da906x_reg_update(regl->hw,
						rinfo->suspend_sleep.addr,
						rinfo->suspend_sleep.mask,
						val);
		} else {
			if (!rinfo->sleep.addr)
				return -EINVAL;
			ret = da906x_reg_update(regl->hw,
						rinfo->sleep.addr,
						rinfo->sleep.mask, val);
		}
	}

	if (ret == 0) {
		if (sys_state == SYS_STATE_SUSPEND)
			regl->suspend_mode = mode;
		else
			regl->mode = mode;
	}

	return ret;
}

static unsigned da906x_get_mode_internal(struct da906x_regulator *regl,
					 int sys_state)
{
	const struct da906x_regulator_info *rinfo = regl->info;
	int val;
	int addr;
	int mask;
	unsigned mode = 0;

	if (rinfo->mode.addr) {
		/* 3 modes are mapped for BUCKs */
		val = da906x_reg_read(regl->hw, rinfo->mode.addr);
		if (val < 0)
			return val;

		switch (val & rinfo->mode.mask) {
		default:
		case DA906X_BUCK_MODE_MANUAL:
			mode = REGULATOR_MODE_FAST | REGULATOR_MODE_STANDBY;
			break;
		case DA906X_BUCK_MODE_SLEEP:
			return REGULATOR_MODE_STANDBY;
		case DA906X_BUCK_MODE_SYNC:
			return REGULATOR_MODE_FAST;
		case DA906X_BUCK_MODE_AUTO:
			return REGULATOR_MODE_NORMAL;
		}
	} else if (rinfo->sleep.addr) {
		/* For LDOs there are 2 modes to map to */
		mode = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY;
	} else {
		/* No support */
		return 0;
	}

	/* Choose sleep bit to read from. May be forced by input parameter
	   or left for detection (sys_state == SYS_STATE_CURRENT) */
	if (sys_state == SYS_STATE_CURRENT && rinfo->suspend.addr) {
		val = da906x_reg_read(regl->hw,
					  rinfo->suspend.addr);
		if (val < 0)
			return val;

		if (val & rinfo->suspend.mask)
			sys_state = SYS_STATE_SUSPEND;
		else
			sys_state = SYS_STATE_NORMAL;
	}

	if (sys_state == SYS_STATE_SUSPEND && rinfo->suspend_sleep.addr) {
		addr = rinfo->suspend_sleep.addr;
		mask = rinfo->suspend_sleep.mask;
	} else {
		addr = rinfo->sleep.addr;
		mask = rinfo->sleep.mask;
	}

	val = da906x_reg_read(regl->hw, addr);
	if (val < 0)
		return val;

	if (val & mask)
		mode &= REGULATOR_MODE_STANDBY;
	else
		mode &= REGULATOR_MODE_NORMAL | REGULATOR_MODE_FAST;

	return mode;
}

static int da906x_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	struct da906x_regulator *regl;
	regl = rdev_get_drvdata(rdev);

	return da906x_sel_to_vol(regl, selector);
}

static int da906x_set_voltage(struct regulator_dev *rdev,
				int min_uv, int max_uv, unsigned *selector)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	const struct field *fvol = &regl->info->voltage;
	int ret;
	unsigned val;
	unsigned sel = da906x_min_val_to_sel(min_uv, regl->info->min_uv,
							regl->info->step_uv);

	if (da906x_sel_to_vol(regl, sel) > max_uv)
		return -EINVAL;

	val = (sel + fvol->offset) << fvol->shift & fvol->mask;
	ret = da906x_reg_update(regl->hw, fvol->addr, fvol->mask, val);
	if (ret >= 0)
		*selector = sel;

	return ret;
}

static int da906x_get_voltage_sel(struct regulator_dev *rdev)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	const struct da906x_regulator_info *rinfo = regl->info;
	int sel;

	sel = da906x_reg_read(regl->hw, rinfo->voltage.addr);
	if (sel < 0)
		return sel;

	sel = (sel & rinfo->voltage.mask) >> rinfo->voltage.shift;
	sel -= rinfo->voltage.offset;
	if (sel < 0)
		sel = 0;
	if (sel >= rinfo->n_steps)
		sel = rinfo->n_steps - 1;

	return sel;
}

static int da9210_get_voltage_sel(struct regulator_dev *rdev)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	int sel;

	if (regl->flags & DA9210_FLG_DVC_IF) {
		sel = da906x_reg_read(regl->hw, DA9210_REG_VBUCK_DVC);
		if (sel < 0)
			return sel;

		sel = (sel & DA9210_VBUCK_DVC_MASK) >> DA9210_VBUCK_DVC_SHIFT;
	} else {
		sel = da906x_get_voltage_sel(rdev);
	}

	return sel;
}

static int da906x_set_current_limit(struct regulator_dev *rdev,
							int min_uA, int max_uA)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	const struct da906x_regulator_info *rinfo = regl->info;
	int val = INT_MAX;
	unsigned sel = 0;
	int n;
	int tval;

	if (!rinfo->current_limits)
		return -EINVAL;

	for (n = 0; n < rinfo->n_current_limits; n++) {
		tval = rinfo->current_limits[n];
		if (tval >= min_uA && tval <= max_uA && val > tval) {
			val = tval;
			sel = n;
		}
	}
	if (val == INT_MAX)
		return -EINVAL;

	sel = (sel + rinfo->ilimit.offset) << rinfo->ilimit.shift;
	return da906x_reg_update(regl->hw, rinfo->ilimit.addr,
						rinfo->ilimit.mask, sel);
}

static int da906x_get_current_limit(struct regulator_dev *rdev)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	const struct da906x_regulator_info *rinfo = regl->info;
	int sel;

	sel = da906x_reg_read(regl->hw, rinfo->ilimit.addr);
	if (sel < 0)
		return sel;

	sel = (sel & rinfo->ilimit.mask) >> rinfo->ilimit.shift;
	sel -= rinfo->ilimit.offset;
	if (sel < 0)
		sel = 0;
	if (sel >= rinfo->n_current_limits)
		sel = rinfo->n_current_limits - 1;

	return rinfo->current_limits[sel];
}

static int da9210_set_current_limit(struct regulator_dev *rdev,
				    int min_uA, int max_uA)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	int n_phases = regl->flags & DA9210_FLG_PHASES_MASK;

	/* Divide current limit on number of phases */
	min_uA = min_uA / n_phases;
	max_uA = max_uA / n_phases;

	return da906x_set_current_limit(rdev, min_uA, max_uA);
}

static int da9210_get_current_limit(struct regulator_dev *rdev)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	int n_phases = regl->flags & DA9210_FLG_PHASES_MASK;

	/* Multiply current limit by nominal phase number */
	return da906x_get_current_limit(rdev) * n_phases;
}

static int da906x_enable(struct regulator_dev *rdev)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	const struct da906x_regulator_info *rinfo = regl->info;
	int ret;

	if (rinfo->suspend.mask) {
		/* Make sure to exit from suspend mode on enable */
		ret = da906x_reg_clear_bits(regl->hw, rinfo->suspend.addr,
					    rinfo->suspend.mask);
		if (ret < 0)
			return ret;

		ret = da906x_update_mode_internal(regl, SYS_STATE_NORMAL);
		if (ret < 0)
			return ret;
	}

	return da906x_reg_set_bits(regl->hw, rinfo->enable.addr,
				   rinfo->enable.mask);
}

static int da9210_enable(struct regulator_dev *rdev)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	unsigned dvc_en;
	int ret = da906x_enable(rdev);
	if (ret < 0)
		return ret;

	/* Make sure, that DVC interface is on after enabling DA9210 buck */
	dvc_en = (regl->flags & DA9210_FLG_DVC_IF ? DA9210_DVC_CTRL_EN : 0);
	return da906x_reg_update(regl->hw, DA9210_REG_BUCK_CONT,
				 DA9210_DVC_CTRL_EN, dvc_en);
}

static int da906x_disable(struct regulator_dev *rdev)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	const struct bfield *benable = &regl->info->enable;

	return da906x_reg_clear_bits(regl->hw, benable->addr, benable->mask);
}

static int da906x_is_enabled(struct regulator_dev *rdev)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	const struct bfield *benable = &regl->info->enable;
	int val = da906x_reg_read(regl->hw, benable->addr);

	if (val < 0)
		return val;

	return val & (benable->mask);
}

static int da906x_set_mode(struct regulator_dev *rdev, unsigned mode)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);

	regl->mode = mode;
	return da906x_update_mode_internal(regl, SYS_STATE_NORMAL);
}

static unsigned da906x_get_mode(struct regulator_dev *rdev)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);

	return da906x_get_mode_internal(regl, SYS_STATE_CURRENT);
}

static int da906x_get_status(struct regulator_dev *rdev)
{
	int ret = da906x_is_enabled(rdev);

	if (ret == 0) {
		ret = REGULATOR_STATUS_OFF;
	} else if (ret > 0) {
		ret = da906x_get_mode(rdev);
		if (ret > 0)
			ret = regulator_mode_to_status(ret);
		else if (ret == 0)
			ret = -EIO;
	}

	return ret;
}

static int da906x_set_suspend_voltage(struct regulator_dev *rdev, int uv)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	const struct da906x_regulator_info *rinfo = regl->info;
	const struct field *fsusvol = &rinfo->suspend_voltage;
	unsigned sel;
	int ret;

	if (uv < rinfo->min_uv)
		uv = rinfo->min_uv;
	else if (uv > rinfo->max_uv)
		return -EINVAL;

	sel = da906x_min_val_to_sel(uv, rinfo->min_uv, rinfo->step_uv);
	sel = (sel + fsusvol->offset) << fsusvol->shift & fsusvol->mask;

	ret = da906x_reg_update(regl->hw, fsusvol->addr, fsusvol->mask, sel);

	return ret;
}

static int da906x_suspend_enable(struct regulator_dev *rdev)
{
	int ret;
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	const struct bfield *bsuspend = &regl->info->suspend;

	ret = da906x_reg_set_bits(regl->hw, bsuspend->addr, bsuspend->mask);
	return ret;
}

static int da906x_set_suspend_mode(struct regulator_dev *rdev, unsigned mode)
{
	struct da906x_regulator *regl = rdev_get_drvdata(rdev);
	int ret;

	regl->suspend_mode = mode;
	ret = da906x_update_mode_internal(regl, SYS_STATE_SUSPEND);
	return ret;
}

/* Regulator event handlers */
static irqreturn_t da906x_ldo_lim_event(int irq, void *data)
{
	struct da906x_regulators *regulators = data;
	struct da906x *hw = regulators->regulator[0].hw;
	struct da906x_regulator *regl;
	int bits;
	int i;

	bits = da906x_reg_read(hw, DA906X_REG_STATUS_D);
	if (bits < 0)
		return IRQ_HANDLED;

	for (i = regulators->n_regulators - 1; i >= 0; i--) {
		regl = &regulators->regulator[i];
		if (regl->info->oc_event.addr != DA906X_REG_STATUS_D)
			continue;

		if (regl->info->oc_event.mask & bits)
			regulator_notifier_call_chain(regl->rdev,
					REGULATOR_EVENT_OVER_CURRENT, NULL);
	}

	return IRQ_HANDLED;
}

static irqreturn_t da9210_oc_event(int irq, void *data)
{
	struct da906x_regulator *regl = data;

	regulator_notifier_call_chain(regl->rdev,
				      REGULATOR_EVENT_OVER_CURRENT, NULL);

	return IRQ_HANDLED;
}

static irqreturn_t da9210_temp_event(int irq, void *data)
{
	struct da906x_regulator *regl = data;

	regulator_notifier_call_chain(regl->rdev,
				      REGULATOR_EVENT_OVER_TEMP, NULL);

	return IRQ_HANDLED;
}

/*
 * Probing and Initialisation
 */
static __init const struct da906x_regulator_info
		*da906x_get_regl_info(const struct da906x *da906x,
		const struct da906x_dev_model *model, int id)
{
	int m;

	switch (id) {
	case DA9210_ID_BUCK:
		if (da906x->da9210)
			return &da9210_regulator_info;
		else
			return NULL;

	default:
		for (m = model->n_regulators - 1;
		     model->regulator_info[m].id != id; m--) {
			if (m <= 0)
				return NULL;
		}
		return &model->regulator_info[m];
	}
}

static __init int da9210_set_nominal_phases(struct device *dev,
					       struct da906x_regulator *regl)
{
	unsigned val;

	if ((regl->flags & DA9210_FLG_PHASES_MASK) == 0) {
		/* Read configured number of phases from DA9210 */
		val = da906x_reg_read(regl->hw, DA9210_REG_BUCK_CONF2);

		switch (val & DA9210_PHASE_SEL_MASK) {
		case DA9210_PHASE_SEL_1:
			regl->flags |= DA9210_FLG_1_PHASE;
			break;
		case DA9210_PHASE_SEL_2:
			regl->flags |= DA9210_FLG_2_PHASES;
			break;
		case DA9210_PHASE_SEL_4:
			regl->flags |= DA9210_FLG_4_PHASES;
			break;
		case DA9210_PHASE_SEL_8:
			regl->flags |= DA9210_FLG_8_PHASES;
			break;
		}

		return 0;

	} else {
		/* Configure number of phases in DA9210 */
		switch (regl->flags & DA9210_FLG_PHASES_MASK) {
		default:
			dev_err(dev, "Invalid flags in platform data\n");
			return -EINVAL;
		case DA9210_FLG_1_PHASE:
			val = DA9210_PHASE_SEL_1;
			break;
		case DA9210_FLG_2_PHASES:
			val = DA9210_PHASE_SEL_2;
			break;
		case DA9210_FLG_4_PHASES:
			val = DA9210_PHASE_SEL_4;
			break;
		case DA9210_FLG_8_PHASES:
			val = DA9210_PHASE_SEL_8;
			break;
		}

		return da906x_reg_update(regl->hw, DA9210_REG_BUCK_CONF2,
					 DA9210_PHASE_SEL_MASK, val);
	}
}

/* Dynamic Voltage Configuration may use external input to control
   CoPMIC voltage */
static __init int da9210_set_dvc_if(struct device *dev,
		struct da906x_regulator *regl, int dvc_base_uv, int dvc_max_uv)
{
	unsigned char dvc_conf[2];
	int ret = 0;

	if (regl->flags & DA9210_FLG_DVC_IF) {
		if (dvc_base_uv < regl->info->min_uv ||
		    dvc_max_uv < regl->info->min_uv ||
		    dvc_base_uv > regl->info->max_uv ||
		    dvc_max_uv > regl->info->max_uv) {
			dev_warn(dev,
				 "Using default DVC_BASE and DVC_MAX "
				 "voltages for %s regulator\n",
				 regl->info->name);
		} else {
			dvc_conf[0] = da906x_min_val_to_sel(dvc_base_uv,
				regl->info->min_uv, regl->info->step_uv);
			dvc_conf[1] = da906x_min_val_to_sel(dvc_max_uv,
				regl->info->min_uv, regl->info->step_uv);

			dvc_conf[1] &= ~DA9210_DVC_STEP_SIZE_MASK;
			if (regl->flags & DA9210_FLG_DVC_STEP_20MV)
				dvc_conf[1] |= DA9210_DVC_STEP_SIZE_20MV;
			else
				dvc_conf[1] |= DA9210_DVC_STEP_SIZE_10MV;

			ret = da906x_block_write(regl->hw,
					DA9210_REG_VBUCK_BASE, 2, dvc_conf);
		}
	}

	return ret;
}

#ifdef CONFIG_OF
static struct da906x_regulators_pdata *da906x_reg_parse_dt(struct device *dev)
{
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *regulators_np, *np;
	struct da906x_regulators_pdata *pdata;
	const struct da906x_regulator_info *rinfo;
	struct da906x_regulator_data *regulator_data;
	int i;

	if (unlikely(nproot == NULL))
		return ERR_PTR(-EINVAL);

	regulators_np = of_find_node_by_name(nproot, "regulators");
	if (!regulators_np) {
		dev_err(dev, "regulators node not found\n");
		return -ENODEV;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	pdata->n_regulators = of_get_child_count(regulators_np);
	pdata->regulator_data = regulator_data = devm_kzalloc(dev,
			sizeof(*pdata->regulator_data) * pdata->n_regulators,
			GFP_KERNEL);
	if (unlikely(pdata->regulator_data == NULL)) {
		of_node_put(regulators_np);
		return ERR_PTR(-ENOMEM);
	}

	for_each_child_of_node(regulators_np, np) {
		for (i = 0; i < ARRAY_SIZE(da9063_regulator_info); i++) {
			if (!of_node_cmp(np->name,da9063_regulator_info[i].name)) {
				break;
			}
		}
		if (unlikely(i >= ARRAY_SIZE(da9063_regulator_info))) {
			dev_warn(dev, "Cannot find the regulator information for %s\n",
				np->name);
			continue;
		}

		regulator_data->id = i;
		regulator_data->initdata = of_get_regulator_init_data(dev, np);
		regulator_data->of_node = np;
		regulator_data++;
	}
	return pdata;
}
#endif



static __init int da906x_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct da906x *da906x = dev_get_drvdata(pdev->dev.parent);
	struct da906x_regulators_pdata *pdata;
	struct da906x_regulator_data *rdata;
	const struct da906x_dev_model *model;
	struct da906x_regulators *regulators;
	struct da906x_regulator *regl;
	struct regulator_config config;
	size_t size;
	int n;
	int ret;

#ifdef CONFIG_OF
	pdata = da906x_reg_parse_dt(dev);
#endif
	if (!pdata || pdata->n_regulators == 0) {
		dev_err(dev,
			"No regulators defined for the platform\n");
		return -ENODEV;
	}
	/* Find regulators set for particular device model */
	for (model = regulators_models; model->regulator_info; model++) {
		if (model->dev_model == da906x_model(da906x))
			break;
	}
	if (!model->regulator_info) {
		dev_err(dev, "Chip model not recognised (%u)\n",
			da906x_model(da906x));
		return -ENODEV;
	}

	ret = da906x_reg_read(da906x, DA906X_REG_CONFIG_H);
	if (ret < 0) {
		dev_err(dev,
			"Error while reading BUCKs configuration\n");
		return -EIO;
	}

	/* Allocate memory required by usable regulators */
	size = sizeof(struct da906x_regulators) +
		pdata->n_regulators * sizeof(struct da906x_regulator);
	regulators = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!regulators) {
		dev_err(dev, "No memory for regulators\n");
		return -ENOMEM;
	}

	regulators->n_regulators = pdata->n_regulators;
	platform_set_drvdata(pdev, regulators);

	/* Register all regulators declared in platform information */
	n = 0;
	while (n < regulators->n_regulators) {
		rdata = &pdata->regulator_data[n];

		/* Initialise regulator structure */
		regl = &regulators->regulator[n];
		regl->hw = da906x;
		regl->flags = rdata->flags;
		regl->info = da906x_get_regl_info(da906x, model, rdata->id);
		if (!regl->info) {
			dev_err(dev,
				"Invalid regulator ID in platform data\n");
			ret = -EINVAL;
			goto err;
		}
		regl->desc.name = regl->info->name;
		regl->desc.id = rdata->id;
		regl->desc.ops = regl->info->ops;
		regl->desc.n_voltages = regl->info->n_steps;
		regl->desc.type = REGULATOR_VOLTAGE;
		regl->desc.owner = THIS_MODULE;

		/* Register regulator */
		config.dev = dev;
		config.init_data = rdata->initdata;
		config.driver_data = regl;
		config.of_node = rdata->of_node;
		config.regmap = da906x->regmap;
		regl->rdev = regulator_register(&regl->desc, &config);
		if (IS_ERR_OR_NULL(regl->rdev)) {
			dev_err(dev,
				"Failed to register %s regulator\n",
				regl->info->name);
			ret = PTR_ERR(regl->rdev);
			goto err;
		}
		n++;

		/* Get current modes of operation (A/B voltage selection)
		   for normal and suspend states */
		ret = da906x_get_mode_internal(regl, SYS_STATE_NORMAL);
		if (ret < 0) {
			dev_err(dev,
				"Failed to read %s regulator's mode\n",
				regl->info->name);
			goto err;
		}
		if (ret == 0)
			regl->mode = REGULATOR_MODE_NORMAL;
		else
			regl->mode = ret;

		ret = da906x_get_mode_internal(regl, SYS_STATE_SUSPEND);
		if (ret < 0) {
			dev_err(dev,
				"Failed to read %s regulator's mode\n",
				regl->info->name);
			goto err;
		}
		if (ret == 0)
			regl->suspend_mode = REGULATOR_MODE_NORMAL;
		else
			regl->mode = ret;

		if (rdata->id == DA9210_ID_BUCK && da906x->da9210) {
			/* CoPMIC init */
			ret = da9210_set_nominal_phases(dev, regl);
			if (ret < 0) {
				dev_err(dev,
					"Failed to configure %s regulator\n",
					regl->info->name);
				goto err;
			}

			ret = da9210_set_dvc_if(dev, regl,
					rdata->dvc_base_uv, rdata->dvc_max_uv);

			regulators->irq_da9210_oc =
				platform_get_irq_byname(pdev, "DA9210_OC");
			if (regulators->irq_da9210_oc >= 0) {
				ret = request_threaded_irq(
					regulators->irq_da9210_oc,
					NULL, da9210_oc_event,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					"DA9210_OC", regl);
				if (ret) {
					dev_err(dev,
					"Failed to request DA9210_OC IRQ.\n");
					regulators->irq_da9210_oc = -ENXIO;
				}
			}

			regulators->irq_da9210_temp =
				platform_get_irq_byname(pdev, "DA9210_TEMP");
			if (regulators->irq_da9210_temp >= 0) {
				ret = request_threaded_irq(
					regulators->irq_da9210_temp,
					NULL, da9210_temp_event,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"DA9210_TEMP", regl);
				if (ret) {
					dev_err(&pdev->dev,
						"Failed to request "
							"DA9210_TEMP IRQ.\n");
					regulators->irq_da9210_temp = -ENXIO;
				}
			}
		}
	}
#ifndef CONFIG_MFD_DA906X_IRQ_DISABLE
	/* LDOs overcurrent event support */
	regulators->irq_ldo_lim = platform_get_irq_byname(pdev, "LDO_LIM");
	if (regulators->irq_ldo_lim >= 0) {
		ret = request_threaded_irq(regulators->irq_ldo_lim,
					   NULL, da906x_ldo_lim_event,
					   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					   "LDO_LIM", regulators);
		if (ret) {
			dev_err(dev,
					"Failed to request LDO_LIM IRQ.\n");
			regulators->irq_ldo_lim = -ENXIO;
		}
	}
#endif
	return 0;

err:
	/* Wind back regulators registeration */
	while (--n >= 0)
		regulator_unregister(regulators->regulator[n].rdev);

	return ret;
}

static int da906x_regulator_remove(struct platform_device *pdev)
{
	struct da906x_regulators *regulators = platform_get_drvdata(pdev);
	struct da906x_regulator *regl;

	free_irq(regulators->irq_ldo_lim, regulators);
	free_irq(regulators->irq_uvov, regulators);

	for (regl = &regulators->regulator[regulators->n_regulators - 1];
	     regl >= &regulators->regulator[0]; regl--) {
		if (regl->desc.id == DA9210_ID_BUCK) {
			/* CoPMIC exit */
			free_irq(regulators->irq_da9210_oc, regl);
			free_irq(regulators->irq_da9210_temp, regl);
		}

		regulator_unregister(regl->rdev);
	}

	return 0;
}

static struct platform_driver da906x_regulator_driver = {
	.driver = {
		.name = DA906X_DRVNAME_REGULATORS,
		.owner = THIS_MODULE,
	},
	.probe = da906x_regulator_probe,
	.remove = da906x_regulator_remove,
};

static int __init da906x_regulator_init(void)
{
	return platform_driver_register(&da906x_regulator_driver);
}
subsys_initcall(da906x_regulator_init);

static void __exit da906x_regulator_cleanup(void)
{
	platform_driver_unregister(&da906x_regulator_driver);
}
module_exit(da906x_regulator_cleanup);


/* Module information */
MODULE_AUTHOR("Krystian Garbaciak <krystian.garbaciak@diasemi.com>");
MODULE_DESCRIPTION("DA906x regulators driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DA906X_DRVNAME_REGULATORS);
