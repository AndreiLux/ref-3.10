/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regmap.h>
#include <linux/mfd/max77525/max77525.h>
#include <linux/mfd/max77525/max77525-regulator.h>
#include <linux/module.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of.h>

#define DVS_SIZE	9

enum max77525_regulator_id {
	MAX77525_REG_BUCK1A,
#if 1
	MAX77525_REG_BUCK1B,
	MAX77525_REG_BUCK1C,
	MAX77525_REG_BUCK1D,
#endif
	MAX77525_REG_BUCK2A,
	MAX77525_REG_BUCK2B,
	MAX77525_REG_BUCK2C,
	MAX77525_REG_BUCK2D,
	MAX77525_REG_BUCK3,
#if 0	/* Need to be checked */
	MAX77525_REG_BUCK1ADVS,
	MAX77525_REG_BUCK1BDVS,
	MAX77525_REG_BUCK1CDVS,
	MAX77525_REG_BUCK1DDVS,
	MAX77525_REG_BUCK2ADVS,
	MAX77525_REG_BUCK2BDVS,
	MAX77525_REG_BUCK2CDVS,
	MAX77525_REG_BUCK2DDVS,
	MAX77525_REG_BUCK3DVS,
#endif
	MAX77525_REG_BUCK4,
	MAX77525_REG_BUCK5,
	MAX77525_REG_BUCK6,
	MAX77525_REG_LDO1,
	MAX77525_REG_LDO2,
	MAX77525_REG_LDO3,
	MAX77525_REG_LDO4,
	MAX77525_REG_LDO5,
	MAX77525_REG_LDO6,
	MAX77525_REG_LDO7,
	MAX77525_REG_LDO8,
	MAX77525_REG_LDO9,
	MAX77525_REG_LDO10,
	MAX77525_REG_LDO11,
	MAX77525_REG_LDO12,
	MAX77525_REG_LDO13,
	MAX77525_REG_LDO14,
	MAX77525_REG_LDO15,
	MAX77525_REG_LDO16,
	MAX77525_REG_LDO17,
	MAX77525_REG_LDO18,
	MAX77525_REG_LDO19,
	MAX77525_REG_LDO20,
	MAX77525_REG_LDO21,
	MAX77525_REG_LDO22,
	MAX77525_REG_LDO23,
	MAX77525_REG_LDO24,
	MAX77525_REG_LDO25,
	MAX77525_REG_BOOST,
	MAX77525_REG_LVS1,
	MAX77525_REG_LVS2,
	MAX77525_REG_LVS3,
	MAX77525_REG_HVS,
	MAX77525_REG_REFOUT1,
	MAX77525_REG_REFOUT2,
	MAX77525_REG_REFOUT3,
	MAX77525_REG_MAX
};

enum max77525_buck_id {
	MAX77525_BUCK1A,
	MAX77525_BUCK1B,
	MAX77525_BUCK1C,
	MAX77525_BUCK1D,
	MAX77525_BUCK2A,
	MAX77525_BUCK2B,
	MAX77525_BUCK2C,
	MAX77525_BUCK2D,
	MAX77525_BUCK3,
#if 0	/* Need to be checked */
	MAX77525_BUCK1ADVS,
	MAX77525_BUCK1BDVS,
	MAX77525_BUCK1CDVS,
	MAX77525_BUCK1DDVS,
	MAX77525_BUCK2ADVS,
	MAX77525_BUCK2BDVS,
	MAX77525_BUCK2CDVS,
	MAX77525_BUCK2DDVS,
	MAX77525_BUCK3DVS,
#endif
	MAX77525_BUCK4,
	MAX77525_BUCK5,
	MAX77525_BUCK6,
	MAX77525_BUCK_MAX
};

enum max77525_ldo_id {
	MAX77525_LDO1,
	MAX77525_LDO2,
	MAX77525_LDO3,
	MAX77525_LDO4,
	MAX77525_LDO5,
	MAX77525_LDO6,
	MAX77525_LDO7,
	MAX77525_LDO8,
	MAX77525_LDO9,
	MAX77525_LDO10,
	MAX77525_LDO11,
	MAX77525_LDO12,
	MAX77525_LDO13,
	MAX77525_LDO14,
	MAX77525_LDO15,
	MAX77525_LDO16,
	MAX77525_LDO17,
	MAX77525_LDO18,
	MAX77525_LDO19,
	MAX77525_LDO20,
	MAX77525_LDO21,
	MAX77525_LDO22,
	MAX77525_LDO23,
	MAX77525_LDO24,
	MAX77525_LDO25,
	MAX77525_LDO_MAX
};

enum max77525_boost_id {
	MAX77525_BOOST,
	MAX77525_BOOST_MAX
};

enum max77525_vs_id {
	MAX77525_LVS1,
	MAX77525_LVS2,
	MAX77525_LVS3,
	MAX77525_HVS,
	MAX77525_VS_MAX
};

enum max77525_refout_id {
	MAX77525_REFOUT1,
	MAX77525_REFOUT2,
	MAX77525_REFOUT3,
	MAX77525_REFOUT_MAX
};


/* BUCK */
#define REG_BUCK1A_CNFG		0x5E
#define REG_BUCK_CNFG(X)	(REG_BUCK1A_CNFG + X)

/* BUCKxx are BUCK1A/1B/1C/1D, BUCK2A/2B/2C/2D, and BUCK3 */
#define BUCKXX_RAMP_MASK	0xC0
#define BUCKXX_ADDIS_MASK	0x08
#define BUCKXX_FPWMEN_MASK	0x04
#define BUCKXX_RSNSEN_MASK	0x02
#define BUCKXX_FSREN_MASK	0x01
#define BUCKXX_VOUT_MASK	0xFF

/* BUCKx are BUCK4/5/6 */
#define BUCKX_FPWMEN_MASK	0x80
#define BUCKX_ADDIS_MASK	0x40
#define BUCKX_VOUT_MASK		0x3F

#define REG_BUCK1A_VOUT		0x6C
#define REG_BUCK_VOUT(X)	(REG_BUCK1A_VOUT + X)

/* BUCK DVS */
#define REG_DVSCNFG1		0x22
#define REG_DVSCNFG2		0x23
#define BUCK3DVSEN_MASK		0x01

#define REG_BUCK1A_VOUTDVS	0x75
#define REG_BUCK_VOUTDVS(X)	(REG_BUCK1A_VOUTDVS + X)

/* LDO */
#define REG_LDO1_CNFG	0x7F
#define REG_LDO_CNFG(X)	(REG_LDO1_CNFG + X)

#define LDO_ADEN_MASK	0x40
#define LDO_VOUT_MASK	0X3F

/* BOOST */
#define REG_BOOST_CNFG		0x9F
#define BOOST_ADEN_MASK		0x01
#define BOOST_FORCEBYP_MASK	0x04

#define REG_BOOST_VOUT 		0xA0
#define BOOST_VOUT_MASK		0x3F

/* SWITCH */
#define REG_LVS1_CNFG	0xA3
#define REG_VS_CNFG(X)	(REG_LVS1_CNFG + X)
#define VS_ADEN_MASK	0x08
#define VS_RT_MASK		0x06

#define REG_HVS_CNFG	0xA6

/* OPMODE */
#define REG_BUCKOPMD1		0x11
#define BUCK1A_OPMD_MASK	0x03
#define BUCK1B_OPMD_MASK	0x0C
#define BUCK1C_OPMD_MASK	0x30
#define BUCK1D_OPMD_MASK	0xC0

#define BUCK1ADVS_OPMD_MASK	0x03
#define BUCK1BDVS_OPMD_MASK	0x0C
#define BUCK1CDVS_OPMD_MASK	0x30
#define BUCK1DDVS_OPMD_MASK	0xC0

#define REG_BUCKOPMD2		0X12
#define BUCK2A_OPMD_MASK	0x03
#define BUCK2B_OPMD_MASK	0x0C
#define BUCK2C_OPMD_MASK	0x30
#define BUCK2D_OPMD_MASK	0xC0

#define BUCK2ADVS_OPMD_MASK	0x03
#define BUCK2BDVS_OPMD_MASK	0x0C
#define BUCK2CDVS_OPMD_MASK	0x30
#define BUCK2DDVS_OPMD_MASK	0xC0

#define REG_BUCKOPMD3		0x13
#define BUCK3_OPMD_MASK		0x03
#define BUCK4_OPMD_MASK		0x0C
#define BUCK5_OPMD_MASK		0x30
#define BUCK6_OPMD_MASK		0xC0

#define BUCK3DVS_OPMD_MASK	0x03

#define REG_LDOOPMD1		0x14
#define LDO1_OPMD_MASK		0x03
#define LDO2_OPMD_MASK		0x0C
#define LDO3_OPMD_MASK		0x30
#define LDO4_OPMD_MASK		0xC0

#define REG_LDOOPMD2		0x15
#define LDO5_OPMD_MASK		0x03
#define LDO6_OPMD_MASK		0x0C
#define LDO7_OPMD_MASK		0x30
#define LDO8_OPMD_MASK		0xC0

#define REG_LDOOPMD3		0x16
#define LDO9_OPMD_MASK		0x03
#define LDO10_OPMD_MASK		0x0C
#define LDO11_OPMD_MASK		0x30
#define LDO12_OPMD_MASK		0xC0

#define REG_LDOOPMD4		0x17
#define LDO13_OPMD_MASK		0x03
#define LDO14_OPMD_MASK		0x0C
#define LDO15_OPMD_MASK		0x30
#define LDO16_OPMD_MASK		0xC0

#define REG_LDOOPMD5		0x18
#define LDO17_OPMD_MASK		0x03
#define LDO18_OPMD_MASK		0x0C
#define LDO19_OPMD_MASK		0x30
#define LDO20_OPMD_MASK		0xC0

#define REG_LDOOPMD6		0x19
#define LDO21_OPMD_MASK		0x03
#define LDO22_OPMD_MASK		0x0C
#define LDO23_OPMD_MASK		0x30
#define LDO24_OPMD_MASK		0xC0

#define REG_LDOOPMD7		0x1A
#define LDO25_OPMD_MASK		0x03

#define REG_BSTOPMD			0x1C
#define BOOST_OPMD_MASK		0x01

#define REG_LVSOPMD			0x1D
#define LVS1_OPMD_MASK		0x01
#define LVS2_OPMD_MASK		0x02
#define LVS3_OPMD_MASK		0x04

#define HVS_OPMD_MASK		0x01

#define REG_REFOUTOPMD		0x1E
#define REFOUT1_OPMD_MASK	0x01
#define REFOUT2_OPMD_MASK	0x02
#define REFOUT3_OPMD_MASK	0x04

#define BUCK_DEFAULT_ENABLE_MODE	0x03
#define LDO_DEFAULT_ENABLE_MODE		0x03
#define DEFAULT_ENABLE_MODE	0x01

#define MAX77525_COMMON_DISABLE	0

/*
 * This voltage in uV is returned by get_voltage functions when there is no way
 * to determine the current voltage level.  It is needed because the regulator
 * framework treats a 0 uV voltage as an error.
 */
#define VOLTAGE_UNKNOWN 1

/* define voltage in uV */
/* BUCK1 are 1A/1B/1C/1D, 2A/2B/2C/2D, and 3 */
#define MAX77525_BUCK1_MIN_UV		500000
#define MAX77525_BUCK1_MAX_UV		1500000
#define MAX77525_BUCK1_STEP_UV		6250
/* BUCK2 are 4, 5, and 6 */
#define MAX77525_BUCK2_MIN_UV		500000
#define MAX77525_BUCK2_MAX_UV		3650000
#define MAX77525_BUCK2_STEP_UV		50000

/* NLDO are LDO 1 and 4 */
#define MAX77525_NMOS_LDO_MIN_UV	600000
#define MAX77525_NMOS_LDO_MAX_UV	2175000
#define MAX77525_NMOS_LDO_STEP_UV	25000

/* PLDO are other LDOs  */
#define MAX77525_PMOS_LDO_MIN_UV	800000
#define MAX77525_PMOS_LDO_MAX_UV	3950000
#define MAX77525_PMOS_LDO_STEP_UV	50000

/* Boost Voltage table */
const static unsigned int max77525_boost_volt_table[] =
{
	3000000, 3000000, 3050000, 3100000, 3150000, 3200000, 3250000, 3300000,
	3350000, 3400000, 3450000, 3500000, 3550000, 3600000, 3650000, 3700000,
	3750000, 3800000, 3850000, 3900000, 3950000, 4000000, 4000000, 4000000,
	4000000, 4000000, 4000000, 4000000, 4000000, 4000000, 4000000, 4000000
};


/* Undefined register address value */
#define REG_RESERVED 0xFF

struct max77525_reg_data {
	struct regulator_dev *rdev;
	u8 enable_mode;
};

struct max77525_reg {
	struct max77525_reg_data *reg_data;
	int num_regulators;
};

static int max77525_regulator_common_enable(struct regulator_dev *rdev)
{
	int rc;

	rc = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
							rdev->desc->enable_mask,
							rdev->desc->enable_mask);
	if (rc)
		dev_err(&rdev->dev, "max77525 regulator enable failed, rc=%d\n", rc);

	return rc;
}

static int max77525_regulator_ldo_enable(struct regulator_dev *rdev)
{
	int rc;

	rc = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
							rdev->desc->enable_mask,
							rdev->desc->enable_mask);
	if (rc)
		dev_err(&rdev->dev, "max77525 regulator enable failed, rc=%d\n", rc);

	return rc;
}

static int max77525_buck_set_ramp_delay(struct regulator_dev *rdev,
				int ramp_delay)
{
	const int ramp_table[] = {
		12500, 25000, 50000, 100000
	};
	int i, rc;

    for (i = 0; i < ARRAY_SIZE(ramp_table) && ramp_delay > ramp_table[i]; i++);

    rc = regmap_update_bits(rdev->regmap, REG_BUCK_CNFG(rdev->desc->id),
			BUCKXX_RAMP_MASK, i << MASK2SHIFT(BUCKXX_RAMP_MASK));

	return rc;
}

static struct regulator_ops max77525_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.is_enabled			= regulator_is_enabled_regmap,
	.enable				= max77525_regulator_common_enable,
	.disable			= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
	.set_ramp_delay 	= max77525_buck_set_ramp_delay,
};

static struct regulator_ops max77525_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.is_enabled			= regulator_is_enabled_regmap,
	.enable				= max77525_regulator_common_enable,
	.disable			= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
};

static struct regulator_ops max77525_vs_ops = {
	.is_enabled			= regulator_is_enabled_regmap,
	.enable				= max77525_regulator_common_enable,
	.disable			= regulator_disable_regmap,
};

static struct regulator_ops max77525_boost_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.is_enabled			= regulator_is_enabled_regmap,
	.enable				= max77525_regulator_common_enable,
	.disable			= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
};

static struct regulator_ops max77525_refout_ops = {
	.is_enabled			= regulator_is_enabled_regmap,
	.enable				= max77525_regulator_common_enable,
	.disable			= regulator_disable_regmap,
};


#define REGULATOR_DESC_BUCK1(num, vout_reg, ctrl_reg)	{	\
	.name		= "BUCK"#num,		    	\
	.id		    = MAX77525_REG_BUCK##num,	\
	.ops		= &max77525_buck_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,		    	\
	.n_voltages	= ((MAX77525_BUCK1_MAX_UV - MAX77525_BUCK1_MIN_UV) / \
					MAX77525_BUCK1_STEP_UV + 1),	\
	.min_uV		= MAX77525_BUCK1_MIN_UV,	\
	.uV_step	= MAX77525_BUCK1_STEP_UV,	\
	.ramp_delay	= 25,		        		\
	.vsel_reg	= (vout_reg),		    	\
	.vsel_mask	= BUCKXX_VOUT_MASK,			\
	.enable_reg	= (ctrl_reg),				\
	.enable_mask= BUCK##num##_OPMD_MASK,		\
}

#define REGULATOR_DESC_BUCK2(num, vout_reg, ctrl_reg) {	\
	.name		= "BUCK"#num,		    	\
	.id		    = MAX77525_REG_BUCK##num,	\
	.ops		= &max77525_buck_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,		    	\
	.n_voltages	= ((MAX77525_BUCK2_MAX_UV - MAX77525_BUCK2_MIN_UV) / \
					MAX77525_BUCK2_STEP_UV + 1),	\
	.min_uV		= MAX77525_BUCK2_MIN_UV,	\
	.uV_step	= MAX77525_BUCK2_STEP_UV,	\
	.vsel_reg	= (vout_reg),		    	\
	.vsel_mask	= BUCKX_VOUT_MASK,			\
	.enable_reg	= (ctrl_reg),				\
	.enable_mask= BUCK##num##_OPMD_MASK,		\
}

#define REGULATOR_DESC_NMOS_LDO(num, vout_reg, ctrl_reg) {	\
	.name		= "LDO"#num,		    	\
	.id		    = MAX77525_REG_LDO##num,	\
	.ops		= &max77525_ldo_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,		    	\
	.n_voltages	= ((MAX77525_NMOS_LDO_MAX_UV - MAX77525_NMOS_LDO_MIN_UV) / \
					MAX77525_NMOS_LDO_STEP_UV + 1),	\
	.min_uV		= MAX77525_NMOS_LDO_MIN_UV,	\
	.uV_step	= MAX77525_NMOS_LDO_STEP_UV,\
	.ramp_delay	= 30,		        		\
	.vsel_reg	= (vout_reg),		    	\
	.vsel_mask	= LDO_VOUT_MASK,			\
	.enable_reg	= (ctrl_reg),		    	\
	.enable_mask= LDO##num##_OPMD_MASK,		\
}

#define REGULATOR_DESC_PMOS_LDO(num, vout_reg, ctrl_reg) {	\
	.name		= "LDO"#num,		    	\
	.id		    = MAX77525_REG_LDO##num,	\
	.ops		= &max77525_ldo_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,		    	\
	.n_voltages	= ((MAX77525_PMOS_LDO_MAX_UV - MAX77525_PMOS_LDO_MIN_UV) / \
					MAX77525_PMOS_LDO_STEP_UV + 1),	\
	.min_uV		= MAX77525_PMOS_LDO_MIN_UV,	\
	.uV_step	= MAX77525_PMOS_LDO_STEP_UV,	\
	.ramp_delay	= 30,		        		\
	.vsel_reg	= (vout_reg),		    	\
	.vsel_mask	= LDO_VOUT_MASK,			\
	.enable_reg	= (ctrl_reg),		    	\
	.enable_mask= LDO##num##_OPMD_MASK,		\
}

#define REGULATOR_DESC_BOOST() {		\
	.name		= "boost",		    	\
	.id		    = MAX77525_REG_BOOST,	\
	.ops		= &max77525_boost_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		    \
	.n_voltages	= sizeof(max77525_boost_volt_table),	\
	.volt_table	= max77525_boost_volt_table,	\
	.vsel_reg	= REG_BOOST_VOUT,		\
	.vsel_mask	= BOOST_VOUT_MASK,	    \
	.enable_reg	= REG_BSTOPMD,			\
	.enable_mask= BOOST_OPMD_MASK,	    \
}

#define REGULATOR_DESC_LVS(num) {		\
	.name		= "LVS"#num,		    \
	.id		    = MAX77525_REG_LVS##num,\
	.ops		= &max77525_vs_ops,		\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		    \
	.enable_reg	= REG_LVSOPMD,			\
	.enable_mask= LVS##num##_OPMD_MASK,	\
}

#define REGULATOR_DESC_HVS() {			\
	.name		= "HVS",		    	\
	.id		    = MAX77525_REG_HVS,		\
	.ops		= &max77525_vs_ops,		\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		    \
	.enable_reg	= REG_HVS_CNFG,			\
	.enable_mask= HVS_OPMD_MASK,	    \
}

#define REGULATOR_DESC_REFOUT(num) {	\
	.name		= "REFOUT"#num,    	\
	.id		    = MAX77525_REG_REFOUT##num,	\
	.ops		= &max77525_refout_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		    \
	.enable_reg	= REG_REFOUTOPMD,		\
	.enable_mask= REFOUT##num##_OPMD_MASK,\
}

static struct regulator_desc max77525_reg_desc[] = {
	REGULATOR_DESC_BUCK1(1A, REG_BUCK_VOUT(MAX77525_BUCK1A), REG_BUCKOPMD1),
	REGULATOR_DESC_BUCK1(2A, REG_BUCK_VOUT(MAX77525_BUCK2A), REG_BUCKOPMD2),
	REGULATOR_DESC_BUCK1(2C, REG_BUCK_VOUT(MAX77525_BUCK2C), REG_BUCKOPMD2),
	REGULATOR_DESC_BUCK1(2D, REG_BUCK_VOUT(MAX77525_BUCK2D), REG_BUCKOPMD2),
	REGULATOR_DESC_BUCK1(3, REG_BUCK_VOUT(MAX77525_BUCK3), REG_BUCKOPMD3),
#if 0	/* Need to be checked */
	REGULATOR_DESC_BUCK1(1ADVS, REG_BUCK_VOUTDVS(MAX77525_BUCK1A), REG_BUCKOPMD1),
	REGULATOR_DESC_BUCK1(1BDVS, REG_BUCK_VOUTDVS(MAX77525_BUCK1B), REG_BUCKOPMD1),
	REGULATOR_DESC_BUCK1(1CDVS, REG_BUCK_VOUTDVS(MAX77525_BUCK1C), REG_BUCKOPMD1),
	REGULATOR_DESC_BUCK1(1DDVS, REG_BUCK_VOUTDVS(MAX77525_BUCK1D), REG_BUCKOPMD1),
	REGULATOR_DESC_BUCK1(2ADVS, REG_BUCK_VOUTDVS(MAX77525_BUCK2A), REG_BUCKOPMD2),
	REGULATOR_DESC_BUCK1(2BDVS, REG_BUCK_VOUTDVS(MAX77525_BUCK2B), REG_BUCKOPMD2),
	REGULATOR_DESC_BUCK1(2CDVS, REG_BUCK_VOUTDVS(MAX77525_BUCK2C), REG_BUCKOPMD2),
	REGULATOR_DESC_BUCK1(2DDVS, REG_BUCK_VOUTDVS(MAX77525_BUCK2D), REG_BUCKOPMD2),
	REGULATOR_DESC_BUCK1(3DVS, REG_BUCK_VOUTDVS(MAX77525_BUCK3), REG_BUCKOPMD3),
#endif
	REGULATOR_DESC_BUCK2(4, REG_BUCK_CNFG(MAX77525_BUCK4), REG_BUCKOPMD3),
	REGULATOR_DESC_BUCK2(5, REG_BUCK_CNFG(MAX77525_BUCK5), REG_BUCKOPMD3),
	REGULATOR_DESC_BUCK2(6, REG_BUCK_CNFG(MAX77525_BUCK6), REG_BUCKOPMD3),

	REGULATOR_DESC_NMOS_LDO(1, REG_LDO_CNFG(MAX77525_LDO1), REG_LDOOPMD1),
	REGULATOR_DESC_PMOS_LDO(2, REG_LDO_CNFG(MAX77525_LDO2), REG_LDOOPMD1),
	REGULATOR_DESC_PMOS_LDO(3, REG_LDO_CNFG(MAX77525_LDO3), REG_LDOOPMD1),
	REGULATOR_DESC_NMOS_LDO(4, REG_LDO_CNFG(MAX77525_LDO4), REG_LDOOPMD1),

	REGULATOR_DESC_PMOS_LDO(5, REG_LDO_CNFG(MAX77525_LDO5), REG_LDOOPMD2),
	REGULATOR_DESC_PMOS_LDO(6, REG_LDO_CNFG(MAX77525_LDO6), REG_LDOOPMD2),
	REGULATOR_DESC_PMOS_LDO(7, REG_LDO_CNFG(MAX77525_LDO7), REG_LDOOPMD2),
	REGULATOR_DESC_PMOS_LDO(8, REG_LDO_CNFG(MAX77525_LDO8), REG_LDOOPMD2),

	REGULATOR_DESC_PMOS_LDO(9, REG_LDO_CNFG(MAX77525_LDO9), REG_LDOOPMD3),
	REGULATOR_DESC_PMOS_LDO(10, REG_LDO_CNFG(MAX77525_LDO10), REG_LDOOPMD3),
	REGULATOR_DESC_PMOS_LDO(11, REG_LDO_CNFG(MAX77525_LDO11), REG_LDOOPMD3),
	REGULATOR_DESC_PMOS_LDO(12, REG_LDO_CNFG(MAX77525_LDO12), REG_LDOOPMD3),

	REGULATOR_DESC_PMOS_LDO(13, REG_LDO_CNFG(MAX77525_LDO13), REG_LDOOPMD4),
	REGULATOR_DESC_PMOS_LDO(14, REG_LDO_CNFG(MAX77525_LDO14), REG_LDOOPMD4),
	REGULATOR_DESC_PMOS_LDO(15, REG_LDO_CNFG(MAX77525_LDO15), REG_LDOOPMD4),
	REGULATOR_DESC_PMOS_LDO(16, REG_LDO_CNFG(MAX77525_LDO16), REG_LDOOPMD4),

	REGULATOR_DESC_PMOS_LDO(17, REG_LDO_CNFG(MAX77525_LDO17), REG_LDOOPMD5),
	REGULATOR_DESC_PMOS_LDO(18, REG_LDO_CNFG(MAX77525_LDO18), REG_LDOOPMD5),
	REGULATOR_DESC_PMOS_LDO(19, REG_LDO_CNFG(MAX77525_LDO19), REG_LDOOPMD5),
	REGULATOR_DESC_PMOS_LDO(20, REG_LDO_CNFG(MAX77525_LDO20), REG_LDOOPMD5),

	REGULATOR_DESC_PMOS_LDO(21, REG_LDO_CNFG(MAX77525_LDO21), REG_LDOOPMD6),
	REGULATOR_DESC_PMOS_LDO(22, REG_LDO_CNFG(MAX77525_LDO22), REG_LDOOPMD6),
	REGULATOR_DESC_PMOS_LDO(23, REG_LDO_CNFG(MAX77525_LDO23), REG_LDOOPMD6),
	REGULATOR_DESC_PMOS_LDO(24, REG_LDO_CNFG(MAX77525_LDO24), REG_LDOOPMD6),

	REGULATOR_DESC_PMOS_LDO(25, REG_LDO_CNFG(MAX77525_LDO25), REG_LDOOPMD7),

	REGULATOR_DESC_BOOST(),

	REGULATOR_DESC_LVS(1),
	REGULATOR_DESC_LVS(2),
	REGULATOR_DESC_LVS(3),

	REGULATOR_DESC_HVS(),

	REGULATOR_DESC_REFOUT(1),
	REGULATOR_DESC_REFOUT(2),
	REGULATOR_DESC_REFOUT(3),
};

static int max77525_reg_hw_init(struct max77525 *max77525,
		struct max77525_regulators_platform_data *pdata)
{
	int rc, i;

    /* Set the BUCK DVS register */
    for (i = 0; i < 8; i++) {
		/* BUCK 1A/1B/1C/1D, BUCK 2A/2B/2C/2D DVS */
		rc = regmap_update_bits(max77525->regmap, REG_DVSCNFG1, 1<<i,
								pdata->dvs_enable[i]<<i);
        if (IS_ERR_VALUE(rc))
            return rc;
    }

	if (i == 8) {
		/* BUCK3 DVS */
		rc = regmap_update_bits(max77525->regmap, REG_DVSCNFG2, BUCK3DVSEN_MASK,
								pdata->dvs_enable[i]);
        if (IS_ERR_VALUE(rc))
            return rc;
	}

	return rc;
}


static int max77525_reg_init_registers(struct max77525 *max77525,
				struct max77525_regulator_data *reg_data)
{
	int rc = 0;
	int val;
	unsigned int test;

	/* switch (reg_data->reg_id) */
	switch (max77525_reg_desc[reg_data->reg_id].id)
	{
	case MAX77525_REG_BUCK1A ... MAX77525_REG_BUCK3:
		val = 0;
		/* Set up active discharge control */
		if (reg_data->pull_down_enable != MAX77525_REGULATOR_USE_HW_DEFAULT)
			val = (val & ~BUCKXX_ADDIS_MASK) |
					(reg_data->pull_down_enable == 0? BUCKXX_ADDIS_MASK : 0);

		/* Set up ramp rate */
		if (reg_data->buck_ramp_rate != MAX77525_REGULATOR_USE_HW_DEFAULT)
			val = (val & ~BUCKXX_RAMP_MASK) | (reg_data->buck_ramp_rate<<6);

		/* Set up forced PWM enable */
		if (reg_data->forced_pwm_enable != MAX77525_REGULATOR_USE_HW_DEFAULT)
			val = (val & ~BUCKXX_FPWMEN_MASK) |
					(reg_data->forced_pwm_enable == 0? 0 : BUCKXX_FPWMEN_MASK);

		/* Set up remote output voltage sense enable */
		if (reg_data->rsns_enable != MAX77525_REGULATOR_USE_HW_DEFAULT)
			val = (val & ~BUCKXX_RSNSEN_MASK) |
					(reg_data->rsns_enable == 0? 0 : BUCKXX_RSNSEN_MASK);

		/* Set up falling slew active-discharge enable */
		if (reg_data->fsr_ad_enable!= MAX77525_REGULATOR_USE_HW_DEFAULT)
			val = (val & ~BUCKXX_FSREN_MASK) |
					(reg_data->fsr_ad_enable == 0? 0 : BUCKXX_FSREN_MASK);

		rc = regmap_write(max77525->regmap,
				REG_BUCK_CNFG(max77525_reg_desc[reg_data->reg_id].id), val);
		if (rc) {
			dev_err(max77525->dev, "I2C write failed, rc=%d\n", rc);
			return rc;
		}
		break;
	case MAX77525_REG_BUCK4:
	case MAX77525_REG_BUCK5:
	case MAX77525_REG_BUCK6:
		val = 0;
		/* Set up active discharge control */
		if (reg_data->pull_down_enable != MAX77525_REGULATOR_USE_HW_DEFAULT)
			val = (val & ~BUCKXX_ADDIS_MASK) |
					(reg_data->pull_down_enable == 0? BUCKX_ADDIS_MASK : 0);

		/* Set up forced PWM enable */
		if (reg_data->forced_pwm_enable != MAX77525_REGULATOR_USE_HW_DEFAULT)
			val = (val & ~BUCKXX_FPWMEN_MASK) |
					(reg_data->forced_pwm_enable == 0? 0 : BUCKX_FPWMEN_MASK);

		rc = regmap_update_bits(max77525->regmap,
				REG_BUCK_CNFG(max77525_reg_desc[reg_data->reg_id].id),
								BUCKX_ADDIS_MASK | BUCKX_FPWMEN_MASK, val);
		if (rc) {
			dev_err(max77525->dev, "I2C write failed, rc=%d\n", rc);
			return rc;
		}
		break;
	case MAX77525_REG_LDO1 ... MAX77525_REG_LDO25:
		val = 0;
		if (reg_data->pull_down_enable != MAX77525_REGULATOR_USE_HW_DEFAULT) {
			val = (reg_data->pull_down_enable == 0? 0 : LDO_ADEN_MASK);
		}

		rc = regmap_update_bits(max77525->regmap,
				max77525_reg_desc[reg_data->reg_id].vsel_reg, LDO_ADEN_MASK, val);
		if (rc) {
			dev_err(max77525->dev, "I2C write failed, rc=%d\n", rc);
			return rc;
		}
		break;
	case MAX77525_REG_BOOST:
		val = 0;
		/* Set up forced bypass mode */
		if (reg_data->boost_force_bypass != MAX77525_REGULATOR_USE_HW_DEFAULT)
			val = (reg_data->boost_force_bypass == 0? 0 : BOOST_FORCEBYP_MASK);

		/* Set up active discharge control */
		if (reg_data->pull_down_enable != MAX77525_REGULATOR_USE_HW_DEFAULT)
			val = (val & ~BOOST_ADEN_MASK) |
							(reg_data->pull_down_enable == 0? 0 : BOOST_ADEN_MASK);

		rc = regmap_update_bits(max77525->regmap, REG_BOOST_CNFG,
								BOOST_FORCEBYP_MASK | BOOST_ADEN_MASK, val);
		if (rc) {
			dev_err(max77525->dev, "I2C write failed, rc=%d\n", rc);
			return rc;
		}
		break;

	default:
		break;
	}

	return rc;
}

#ifdef CONFIG_OF
/* Fill in pdata elements based on values found in device tree. */
static int max77525_regulator_dt_parse_pdata(struct device *dev,
				struct max77525_regulators_platform_data *rpdata)
{
	struct device_node *pmic_node = dev->parent->of_node;
	struct device_node *regulators_node, *reg_node;
	struct max77525_regulator_data *rdata;
	unsigned int i;

	if (!pmic_node) {
		dev_err(dev, "could not find pmic node\n");
		return -ENODEV;
	}

	regulators_node = of_find_node_by_name(pmic_node, "regulators");
	if (!regulators_node) {
		dev_err(dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	/* count the number of regulators to be supported in pmic */
	rpdata->num_regulators = 0;
	for_each_child_of_node(regulators_node, reg_node)
		rpdata->num_regulators++;

	rdata = devm_kzalloc(dev, sizeof(*rdata) *
				rpdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		dev_err(dev, "could not allocate memory for regulator data\n");
		return -ENOMEM;
	}

	rpdata->reg_data = rdata;
	for_each_child_of_node(regulators_node, reg_node) {
		for (i = 0; i < ARRAY_SIZE(max77525_reg_desc); i++)
			if (!of_node_cmp(reg_node->name, max77525_reg_desc[i].name))
				break;

		if (i == ARRAY_SIZE(max77525_reg_desc)) {
			dev_warn(dev, "don't know how to configure regulator %s\n",
				 reg_node->name);
			continue;
		}

		rdata->reg_id = i;
		rdata->initdata = of_get_regulator_init_data(dev, reg_node);
		rdata->reg_node = reg_node;

		/*
		 * Initialize configuration parameters to use hardware default in case
		 * no value is specified via device tree.
		 */

		rdata->enable_mode = (rdata->reg_id < MAX77525_REG_BOOST) ?
			MAX77525_REGULATOR_NM : DEFAULT_ENABLE_MODE;
		rdata->pull_down_enable 	= MAX77525_REGULATOR_USE_HW_DEFAULT;
		rdata->fsr_ad_enable		= MAX77525_REGULATOR_USE_HW_DEFAULT;
		rdata->rsns_enable			= MAX77525_REGULATOR_USE_HW_DEFAULT;
		rdata->forced_pwm_enable	= MAX77525_REGULATOR_USE_HW_DEFAULT;
		rdata->buck_ramp_rate		= MAX77525_REGULATOR_USE_HW_DEFAULT;
		rdata->boost_force_bypass	= MAX77525_REGULATOR_USE_HW_DEFAULT;

		/* These bindings are optional, so it is okay if they are not found. */
		of_property_read_u32(reg_node, "max77525,enable-mode",
			&rdata->enable_mode);
		of_property_read_u32(reg_node, "max77525,pull-down-enable",
			&rdata->pull_down_enable);
		of_property_read_u32(reg_node, "max77525,falling-slew-rate-ad-enable",
			&rdata->fsr_ad_enable);
		of_property_read_u32(reg_node, "max77525,remote-output-voltage-sense-enable",
			&rdata->rsns_enable);
		of_property_read_u32(reg_node, "max77525,forced-pwm-enable",
			&rdata->forced_pwm_enable);
		of_property_read_u32(reg_node, "max77525,buck-ramp-rate",
			&rdata->buck_ramp_rate);
		of_property_read_u32(reg_node, "max77525,boost-forced-bypass",
			&rdata->boost_force_bypass);

		rdata++;
	}

	/*
	 * Initialize configuration parameters to use hardware default in case
	 * no value is specified via device tree.
	 */

	for (i = 0; i < DVS_SIZE; i++)
		rpdata->dvs_enable[i] = 0;

	rpdata->dvs_gpio = 0;

	of_property_read_u32_array(reg_node, "maxim,dvs-enable",
							rpdata->dvs_enable, DVS_SIZE);
	of_property_read_u32(reg_node, "maxim,dvs-gpio", &rpdata->dvs_gpio);


	return 0;
}
#else
static int max77525_regulator_dt_parse_pdata(struct device *dev,
				struct max77525_regulators_platform_data *pdata)
{
	return 0;
}
#endif

static int max77525_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77525 *max77525 = dev_get_drvdata(dev->parent);
	struct max77525_regulators_platform_data *reg_pdata;
	struct max77525_reg *max77525_regulator;
	struct max77525_reg_data *reg_data;
	struct regulator_dev *rdev;
	struct regulator_config config;

	int i = 0, rc = 0;

	reg_pdata = devm_kzalloc(dev, sizeof(*reg_pdata), GFP_KERNEL);
	if (unlikely(reg_pdata == NULL))
		return -ENOMEM;

	if (dev->parent->of_node) {
		rc = max77525_regulator_dt_parse_pdata(dev, reg_pdata);
		if (rc)
			return rc;
	}

	max77525_regulator = devm_kzalloc(dev, sizeof(*max77525_regulator),
											GFP_KERNEL);
	if (unlikely(max77525_regulator == NULL))
		return -ENOMEM;

	reg_data = devm_kzalloc(dev, sizeof(struct max77525_reg_data)
								* reg_pdata->num_regulators, GFP_KERNEL);
	if (unlikely(reg_data == NULL))
		return -ENOMEM;

	platform_set_drvdata(pdev, max77525_regulator);
	max77525_regulator->reg_data = reg_data;
	max77525_regulator->num_regulators = reg_pdata->num_regulators;

	rc = max77525_reg_hw_init(max77525, reg_pdata);
	if (IS_ERR_VALUE(rc))
		return rc;

	for (i = 0; i < reg_pdata->num_regulators; i++)
	{
		reg_data->enable_mode = reg_pdata->reg_data[i].enable_mode;
		config.dev = dev;
		config.regmap = max77525->regmap;
		config.init_data = reg_pdata->reg_data[i].initdata;
        config.driver_data = &reg_data[i];
		config.of_node = reg_pdata->reg_data[i].reg_node;

		rc = max77525_reg_init_registers(max77525, &reg_pdata->reg_data[i]);
		if (IS_ERR_VALUE(rc))
			return rc;

		rdev = regulator_register(&max77525_reg_desc[reg_pdata->reg_data[i].reg_id],
							&config);
		if (unlikely(IS_ERR(rdev)))	{
			rc = PTR_ERR(rdev);
			dev_err(dev, "regulator register failed to register regulator %d\n",
					reg_pdata->reg_data[i].reg_id);
			goto err;
		}
		reg_data[i].rdev = rdev;
	}

	return 0;

err:
	while (--i >= 0)
		regulator_unregister(reg_data[i].rdev);

	dev_err(dev, "%s returns %d\n", __func__, rc);
	return rc;
}

static int max77525_regulator_remove(struct platform_device *pdev)
{
	struct max77525_reg *max77525_regulator = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < max77525_regulator->num_regulators; i++)
		regulator_unregister(max77525_regulator->reg_data[i].rdev);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id max77525_regulator_match[] = {
	{ .compatible = "maxim,max77525-regulator", },
	{ }
};
MODULE_DEVICE_TABLE(of, max77525_regulator_match);
#endif
static const struct platform_device_id max77525_regulator_id[] =
{
	{ "max77525-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, max77525_regulator_id);

static struct platform_driver max77525_regulator_driver =
{
	.driver =
	{
		.name = "max77525-regulator",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max77525_regulator_match),
	},
	.probe = max77525_regulator_probe,
	.remove = max77525_regulator_remove,
	.id_table = max77525_regulator_id,
};

static int __init max77525_regulator_init(void)
{
	return platform_driver_register(&max77525_regulator_driver);
}
subsys_initcall(max77525_regulator_init);

static void __exit max77525_regulator_exit(void)
{
	platform_driver_unregister(&max77525_regulator_driver);
}
module_exit(max77525_regulator_exit);

static int __init max77525_test(void)
{
	static struct regulator *vt_cam_dvdd_1v05;
	int ret = 0;
	unsigned int vt_cam_dvdd_volt = 900000;

	vt_cam_dvdd_1v05 = regulator_get(NULL, "+1.05V_CAM_DVDD");
	if (IS_ERR(vt_cam_dvdd_1v05)) {
		printk("%s: failed to get resource vt_cam_dvdd_1v05\n", __func__);
		goto err;
	}

	ret = regulator_set_voltage(vt_cam_dvdd_1v05, vt_cam_dvdd_volt,
			vt_cam_dvdd_volt);
	if (ret) {
		printk("%s: failed to set cpu voltage to %d\n",
				__func__, vt_cam_dvdd_1v05);
		goto err;
	}

	ret = regulator_enable(vt_cam_dvdd_1v05);
	if (ret) {
		printk("%s: failed to enable regulator: %d\n", __func__, ret);
		goto err;
	}

err:
	return ret;
}

/* late_initcall(max77525_test); */

MODULE_DESCRIPTION("MAXIM 77525 Regulator Driver");
MODULE_AUTHOR("Clark Kim <clark.kim@maximintegrated.com>");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:max77525-regulator");
