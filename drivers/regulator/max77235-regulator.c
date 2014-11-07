/*
 * max77235-regulator.c - Regulator driver for the Maxim 77235
 *
 * Copyright (C) 2013 Maxim Integrated
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regmap.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/mfd/max77235.h>
#include <linux/regulator/max77235-regulator.h>

#include <asm/delay.h>
#include <asm/gpio.h>

static struct max77235 *max77235;

struct max77235_reg_data
{
	struct regulator_dev *rdev;
	enum max77235_opmode opmode;
};

struct max77235_reg
{
	struct max77235 *max77235;
	struct device *dev;
	struct max77235_reg_data *reg_data;
	int num_regulators;
	int dvs[3];
	int selb[5];
	struct mutex dvs_lock;
	int dvs_for_buck[5];
};

/* max77235_set_dvs() changes the DVS settings for bucks
 * Arg:
 * reg : A pointer of regulator for from buck1 to buck6.
 * bucks : a OR-combination of MAX77235_DVS_BUCK1 ... MAX77235_DVS_BUCK6
 * dvs : The index of DVS items
 */
int max77235_set_dvs(struct regulator *reg, int bucks, int dvs)
{
	const struct max77235_reg_data *max77235_reg_data = regulator_get_drvdata(reg);
	struct max77235_reg *max77235_reg = dev_get_drvdata(rdev_get_dev(max77235_reg_data->rdev)->parent);
	int i;

	mutex_lock(&max77235_reg->dvs_lock);
#if 1
	for (i = 0; i < 3; i++)
		if (max77235_reg->dvs[i] >= 0)
			gpio_direction_output(max77235_reg->dvs[i], (dvs >> i) & 1);
#endif
	for (i = 0; i < 5; i++)
		if (max77235_reg->selb[i] >= 0)
			gpio_direction_output(max77235_reg->selb[i], (bucks & (1 << i)) ? 0 : 1);

	ndelay(MAX77235_DVS_DELAY);

	for (i = 0; i < 5; i++)
		if (max77235_reg->selb[i] >= 0)
		{
			gpio_direction_output(max77235_reg->selb[i], 1);
			max77235_reg->dvs_for_buck[i] = dvs;
		}
	mutex_unlock(&max77235_reg->dvs_lock);
	return 0;
}
EXPORT_SYMBOL(max77235_set_dvs);

static int max77235_reg_enable(struct regulator_dev *rdev)
{
	const struct max77235_reg_data *reg_data = rdev_get_drvdata(rdev);

	return regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
			rdev->desc->enable_mask,
			reg_data->opmode << MASK2SHIFT(rdev->desc->enable_mask));
}

static int max77235_reg_set_voltage_sel(struct regulator_dev *rdev, unsigned sel)
{
	struct max77235_reg *max77235_reg = dev_get_drvdata(rdev_get_dev(rdev)->parent);
	const int id = rdev_get_id(rdev);
	int buck, dvs;
	int i, ret;

	ret = regulator_set_voltage_sel_regmap(rdev, sel);

	switch(id)
	{
	case MAX77235_BUCK1DVS1 ... MAX77235_BUCK4DVS8:
		buck = (id - MAX77235_BUCK1DVS1) / (MAX77235_BUCK1DVS8 - MAX77235_BUCK1DVS1 + 1);
		dvs = (id - MAX77235_BUCK1DVS1) % (MAX77235_BUCK1DVS8 - MAX77235_BUCK1DVS1 + 1);
		break;
	case MAX77235_BUCK6DVS1 ... MAX77235_BUCK6DVS8:
		buck = 4;
		dvs = id - MAX77235_BUCK1DVS6;
		break;
	default:
		return ret;
	}

	mutex_lock(&max77235_reg->dvs_lock);

	if (max77235_reg->dvs_for_buck[buck] == dvs && max77235_reg->selb[buck] >= 0)
	{
		for (i = 0; i < 3; i++)
			if (max77235_reg->dvs[i] >= 0)
				gpio_direction_output(max77235_reg->dvs[i], (dvs >> i) & 1);

		gpio_direction_output(max77235_reg->selb[buck], 0);
		ndelay(MAX77235_DVS_DELAY);
		gpio_direction_output(max77235_reg->selb[buck], 1);
		max77235_reg->dvs_for_buck[buck] = dvs;
	}

	mutex_unlock(&max77235_reg->dvs_lock);
	return ret;
}

static int max77235_reg_buck1_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	const int ramp_table[] =
	{
		1000, 2000, 3030, 4000,
		5000, 5880, 7140, 8330,
		9090, 10000, 11110, 12500,
		16670, 25000, 50000, 100000
	};
	int i;

	for (i = sizeof(ramp_table); i >= 0 && i >= ramp_table[i] ; i--);
	if (unlikely(i < 0))
	{
		dev_err(&rdev->dev, "ramp_delay is too small. ramp_delay=%d\n", ramp_delay);
		return -EINVAL;
	}

	return regmap_update_bits(rdev->regmap, rdev->desc->enable_reg, MAX77235_BUCK1_RAMP_M,
			i << MASK2SHIFT(MAX77235_BUCK1_RAMP_M));
}

static int max77235_reg_buck2_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	const int ramp_table[] = {12500, 25000, 50000, 100000};
	int i;

	for (i = sizeof(ramp_table); i >= 0 && i >= ramp_table[i] ; i--);
	if (unlikely(i < 0))
	{
		dev_err(&rdev->dev, "ramp_delay is too small. ramp_delay=%d\n", ramp_delay);
		return -EINVAL;
	}

	return regmap_update_bits(rdev->regmap, rdev->desc->enable_reg, MAX77235_BUCK2_RAMP_M,
			i << MASK2SHIFT(MAX77235_BUCK2_RAMP_M));
}

static struct regulator_ops max77235_reg_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= max77235_reg_enable,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
};

static struct regulator_ops max77235_buck1_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= max77235_reg_enable,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= max77235_reg_set_voltage_sel,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
	.set_ramp_delay		= max77235_reg_buck1_set_ramp_delay,
};

static struct regulator_ops max77235_buck2_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= max77235_reg_enable,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= max77235_reg_set_voltage_sel,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
	.set_ramp_delay		= max77235_reg_buck2_set_ramp_delay,
};

static struct regulator_ops max77235_boost_ops = {
	.list_voltage		= regulator_list_voltage_table,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
};

const static unsigned int max77235_boost_volt_table[] =
{
	3000000, 3000000, 3050000, 3100000, 3150000, 3200000, 3250000, 3300000,
	3350000, 3400000, 3450000, 3500000, 3550000, 3600000, 3650000, 3700000,
	3750000, 3800000, 3850000, 3900000, 3950000, 4000000, 4000000, 4000000,
	4000000, 4000000, 4000000, 4000000, 4000000, 4000000, 4000000, 4000000
};

#define REGULATOR_DESC_PMOS_LDO(num, ctrl_reg) {	\
	.name		= "LDO"#num,		\
	.id		= MAX77235_LDO##num,	\
	.ops		= &max77235_reg_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
	.n_voltages	= ((MAX77256_PMOS_LDO_MAX_uV - MAX77256_PMOS_LDO_MIN_uV) / MAX77256_PMOS_LDO_STEP_uV + 1),	\
	.min_uV		= MAX77256_PMOS_LDO_MIN_uV,	\
	.uV_step	= MAX77256_PMOS_LDO_STEP_uV,	\
	.ramp_delay	= 100000,		\
	.vsel_reg	= (ctrl_reg),		\
	.vsel_mask	= MAX77235_Lx_TV_M,	\
	.enable_reg	= (ctrl_reg),		\
	.enable_mask	= MAX77235_OPMODE_M,	\
	.enable_time	= 10,			\
}

#define REGULATOR_DESC_NMOS_LDO(num, ctrl_reg) {	\
	.name		= "LDO"#num,		\
	.id		= MAX77235_LDO##num,	\
	.ops		= &max77235_reg_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
	.n_voltages	= ((MAX77256_NMOS_LDO_MAX_uV - MAX77256_NMOS_LDO_MIN_uV) / MAX77256_NMOS_LDO_STEP_uV + 1),	\
	.min_uV		= MAX77256_NMOS_LDO_MIN_uV,	\
	.uV_step	= MAX77256_NMOS_LDO_STEP_uV,	\
	.ramp_delay	= 100000,		\
	.vsel_reg	= (ctrl_reg),		\
	.vsel_mask	= MAX77235_Lx_TV_M,	\
	.enable_reg	= (ctrl_reg),		\
	.enable_mask	= MAX77235_OPMODE_M,	\
	.enable_time	= 10,			\
}

#define REGULATOR_DESC_BUCK1(num, v_reg, en_reg) {	\
	.name		= "BUCK"#num,		\
	.id		= MAX77235_BUCK##num,	\
	.ops		= &max77235_buck1_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
	.n_voltages	= ((MAX77256_BUCK1_MAX_uV - MAX77256_BUCK1_MIN_uV) / MAX77256_BUCK1_STEP_uV + 1),	\
	.min_uV		= MAX77256_BUCK1_MIN_uV,	\
	.uV_step	= MAX77256_BUCK1_STEP_uV,	\
	.ramp_delay	= 10000,		\
	.vsel_reg	= (v_reg),		\
	.vsel_mask	= MAX77235_BxDVS_M,	\
	.enable_reg	= (en_reg),		\
	.enable_mask	= MAX77235_BUCK1_EN_M,	\
	.enable_time	= 40,			\
}

#define REGULATOR_DESC_BUCK2(num, v_reg, en_reg) {	\
	.name		= "BUCK"#num,		\
	.id		= MAX77235_BUCK##num,	\
	.ops		= &max77235_buck2_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
	.n_voltages	= ((MAX77256_BUCK2_MAX_uV - MAX77256_BUCK2_MIN_uV) / MAX77256_BUCK2_STEP_uV + 1),	\
	.min_uV		= MAX77256_BUCK2_MIN_uV,	\
	.uV_step	= MAX77256_BUCK2_STEP_uV,	\
	.ramp_delay	= 25000,		\
	.vsel_reg	= (v_reg),		\
	.vsel_mask	= MAX77235_BxDVS_M,	\
	.enable_reg	= (en_reg),		\
	.enable_mask	= MAX77235_BUCK2_EN_M,	\
	.enable_time	= 10,			\
}

#define REGULATOR_DESC_BUCK5(num, v_reg, en_reg) {	\
	.name		= "BUCK"#num,		\
	.id		= MAX77235_BUCK##num,	\
	.ops		= &max77235_reg_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
	.n_voltages	= ((MAX77256_BUCK5_MAX_uV - MAX77256_BUCK5_MIN_uV) / MAX77256_BUCK5_STEP_uV + 1),	\
	.min_uV		= MAX77256_BUCK5_MIN_uV,	\
	.uV_step	= MAX77256_BUCK5_STEP_uV,	\
	.vsel_reg	= (v_reg),		\
	.vsel_mask	= MAX77235_BxOUT_M,	\
	.enable_reg	= (en_reg),		\
	.enable_mask	= MAX77235_BUCK2_EN_M,	\
	.enable_time	= 40,			\
}

#define REGULATOR_DESC_BOOST() {	\
	.name		= "boost",		\
	.id		= MAX77235_BOOST,	\
	.ops		= &max77235_boost_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
	.n_voltages	= ARRAY_SIZE(max77235_boost_volt_table),	\
	.volt_table	= max77235_boost_volt_table,	\
	.vsel_reg	= MAX77235_REG_BOOSTOUT,		\
	.vsel_mask	= MAX77235_BSTOUT_M,	\
	.enable_reg	= MAX77235_REG_BOOSTCTRL,		\
	.enable_mask	= MAX77235_BSTEN_M,	\
	.enable_time	= 230,			\
}

const static struct regulator_desc max77235_reg_desc[] =
{
	REGULATOR_DESC_NMOS_LDO(1, MAX77235_REG_LDO1CTRL1),
	REGULATOR_DESC_NMOS_LDO(2, MAX77235_REG_LDO2CTRL1),
	REGULATOR_DESC_PMOS_LDO(3, MAX77235_REG_LDO3CTRL1),
	REGULATOR_DESC_PMOS_LDO(4, MAX77235_REG_LDO4CTRL1),
	REGULATOR_DESC_PMOS_LDO(5, MAX77235_REG_LDO5CTRL1),
	REGULATOR_DESC_PMOS_LDO(6, MAX77235_REG_LDO6CTRL1),
	REGULATOR_DESC_PMOS_LDO(7, MAX77235_REG_LDO7CTRL1),
	REGULATOR_DESC_NMOS_LDO(8, MAX77235_REG_LDO8CTRL1),
	REGULATOR_DESC_PMOS_LDO(9, MAX77235_REG_LDO9CTRL1),
	REGULATOR_DESC_PMOS_LDO(10, MAX77235_REG_LDO10CTRL1),
	REGULATOR_DESC_PMOS_LDO(11, MAX77235_REG_LDO11CTRL1),
	REGULATOR_DESC_PMOS_LDO(12, MAX77235_REG_LDO12CTRL1),
	REGULATOR_DESC_PMOS_LDO(13, MAX77235_REG_LDO13CTRL1),
	REGULATOR_DESC_PMOS_LDO(14, MAX77235_REG_LDO14CTRL1),
	REGULATOR_DESC_NMOS_LDO(15, MAX77235_REG_LDO15CTRL1),
	REGULATOR_DESC_NMOS_LDO(17, MAX77235_REG_LDO17CTRL1),
	REGULATOR_DESC_PMOS_LDO(18, MAX77235_REG_LDO18CTRL1),
	REGULATOR_DESC_PMOS_LDO(19, MAX77235_REG_LDO19CTRL1),
	REGULATOR_DESC_PMOS_LDO(20, MAX77235_REG_LDO20CTRL1),
	REGULATOR_DESC_PMOS_LDO(21, MAX77235_REG_LDO21CTRL1),
	REGULATOR_DESC_PMOS_LDO(22, MAX77235_REG_LDO22CTRL1),
	REGULATOR_DESC_PMOS_LDO(23, MAX77235_REG_LDO23CTRL1),
	REGULATOR_DESC_PMOS_LDO(24, MAX77235_REG_LDO24CTRL1),
	REGULATOR_DESC_PMOS_LDO(25, MAX77235_REG_LDO25CTRL1),
	REGULATOR_DESC_PMOS_LDO(26, MAX77235_REG_LDO26CTRL1),
	REGULATOR_DESC_NMOS_LDO(27, MAX77235_REG_LDO27CTRL1),
	REGULATOR_DESC_PMOS_LDO(28, MAX77235_REG_LDO28CTRL1),
	REGULATOR_DESC_PMOS_LDO(29, MAX77235_REG_LDO29CTRL1),
	REGULATOR_DESC_NMOS_LDO(30, MAX77235_REG_LDO30CTRL1),
	REGULATOR_DESC_PMOS_LDO(32, MAX77235_REG_LDO32CTRL1),
	REGULATOR_DESC_PMOS_LDO(33, MAX77235_REG_LDO33CTRL1),
	REGULATOR_DESC_PMOS_LDO(34, MAX77235_REG_LDO34CTRL1),
	REGULATOR_DESC_NMOS_LDO(35, MAX77235_REG_LDO35CTRL1),
	REGULATOR_DESC_BUCK1(1DVS1, MAX77235_REG_BUCK1DVS1, MAX77235_REG_BUCK1CTRL),
	REGULATOR_DESC_BUCK1(1DVS2, MAX77235_REG_BUCK1DVS2, MAX77235_REG_BUCK1CTRL),
	REGULATOR_DESC_BUCK1(1DVS3, MAX77235_REG_BUCK1DVS3, MAX77235_REG_BUCK1CTRL),
	REGULATOR_DESC_BUCK1(1DVS4, MAX77235_REG_BUCK1DVS4, MAX77235_REG_BUCK1CTRL),
	REGULATOR_DESC_BUCK1(1DVS5, MAX77235_REG_BUCK1DVS5, MAX77235_REG_BUCK1CTRL),
	REGULATOR_DESC_BUCK1(1DVS6, MAX77235_REG_BUCK1DVS6, MAX77235_REG_BUCK1CTRL),
	REGULATOR_DESC_BUCK1(1DVS7, MAX77235_REG_BUCK1DVS7, MAX77235_REG_BUCK1CTRL),
	REGULATOR_DESC_BUCK1(1DVS8, MAX77235_REG_BUCK1DVS8, MAX77235_REG_BUCK1CTRL),
	REGULATOR_DESC_BUCK2(2DVS1, MAX77235_REG_BUCK2DVS1, MAX77235_REG_BUCK2CTRL1),
	REGULATOR_DESC_BUCK2(2DVS2, MAX77235_REG_BUCK2DVS2, MAX77235_REG_BUCK2CTRL1),
	REGULATOR_DESC_BUCK2(2DVS3, MAX77235_REG_BUCK2DVS3, MAX77235_REG_BUCK2CTRL1),
	REGULATOR_DESC_BUCK2(2DVS4, MAX77235_REG_BUCK2DVS4, MAX77235_REG_BUCK2CTRL1),
	REGULATOR_DESC_BUCK2(2DVS5, MAX77235_REG_BUCK2DVS5, MAX77235_REG_BUCK2CTRL1),
	REGULATOR_DESC_BUCK2(2DVS6, MAX77235_REG_BUCK2DVS6, MAX77235_REG_BUCK2CTRL1),
	REGULATOR_DESC_BUCK2(2DVS7, MAX77235_REG_BUCK2DVS7, MAX77235_REG_BUCK2CTRL1),
	REGULATOR_DESC_BUCK2(2DVS8, MAX77235_REG_BUCK2DVS8, MAX77235_REG_BUCK2CTRL1),
	REGULATOR_DESC_BUCK2(3DVS1, MAX77235_REG_BUCK3DVS1, MAX77235_REG_BUCK3CTRL1),
	REGULATOR_DESC_BUCK2(3DVS2, MAX77235_REG_BUCK3DVS2, MAX77235_REG_BUCK3CTRL1),
	REGULATOR_DESC_BUCK2(3DVS3, MAX77235_REG_BUCK3DVS3, MAX77235_REG_BUCK3CTRL1),
	REGULATOR_DESC_BUCK2(3DVS4, MAX77235_REG_BUCK3DVS4, MAX77235_REG_BUCK3CTRL1),
	REGULATOR_DESC_BUCK2(3DVS5, MAX77235_REG_BUCK3DVS5, MAX77235_REG_BUCK3CTRL1),
	REGULATOR_DESC_BUCK2(3DVS6, MAX77235_REG_BUCK3DVS6, MAX77235_REG_BUCK3CTRL1),
	REGULATOR_DESC_BUCK2(3DVS7, MAX77235_REG_BUCK3DVS7, MAX77235_REG_BUCK3CTRL1),
	REGULATOR_DESC_BUCK2(3DVS8, MAX77235_REG_BUCK3DVS8, MAX77235_REG_BUCK3CTRL1),
	REGULATOR_DESC_BUCK2(4DVS1, MAX77235_REG_BUCK4DVS1, MAX77235_REG_BUCK4CTRL1),
	REGULATOR_DESC_BUCK2(4DVS2, MAX77235_REG_BUCK4DVS2, MAX77235_REG_BUCK4CTRL1),
	REGULATOR_DESC_BUCK2(4DVS3, MAX77235_REG_BUCK4DVS3, MAX77235_REG_BUCK4CTRL1),
	REGULATOR_DESC_BUCK2(4DVS4, MAX77235_REG_BUCK4DVS4, MAX77235_REG_BUCK4CTRL1),
	REGULATOR_DESC_BUCK2(4DVS5, MAX77235_REG_BUCK4DVS5, MAX77235_REG_BUCK4CTRL1),
	REGULATOR_DESC_BUCK2(4DVS6, MAX77235_REG_BUCK4DVS6, MAX77235_REG_BUCK4CTRL1),
	REGULATOR_DESC_BUCK2(4DVS7, MAX77235_REG_BUCK4DVS7, MAX77235_REG_BUCK4CTRL1),
	REGULATOR_DESC_BUCK2(4DVS8, MAX77235_REG_BUCK4DVS8, MAX77235_REG_BUCK4CTRL1),
	REGULATOR_DESC_BUCK5(5, MAX77235_REG_BUCK5OUT, MAX77235_REG_BUCK5CTRL),
	REGULATOR_DESC_BUCK1(6DVS1, MAX77235_REG_BUCK6DVS1, MAX77235_REG_BUCK6CTRL),
	REGULATOR_DESC_BUCK1(6DVS2, MAX77235_REG_BUCK6DVS2, MAX77235_REG_BUCK6CTRL),
	REGULATOR_DESC_BUCK1(6DVS3, MAX77235_REG_BUCK6DVS3, MAX77235_REG_BUCK6CTRL),
	REGULATOR_DESC_BUCK1(6DVS4, MAX77235_REG_BUCK6DVS4, MAX77235_REG_BUCK6CTRL),
	REGULATOR_DESC_BUCK1(6DVS5, MAX77235_REG_BUCK6DVS5, MAX77235_REG_BUCK6CTRL),
	REGULATOR_DESC_BUCK1(6DVS6, MAX77235_REG_BUCK6DVS6, MAX77235_REG_BUCK6CTRL),
	REGULATOR_DESC_BUCK1(6DVS7, MAX77235_REG_BUCK6DVS7, MAX77235_REG_BUCK6CTRL),
	REGULATOR_DESC_BUCK1(6DVS8, MAX77235_REG_BUCK6DVS8, MAX77235_REG_BUCK6CTRL),
	REGULATOR_DESC_BUCK5(7, MAX77235_REG_BUCK7OUT, MAX77235_REG_BUCK7CTRL),
	REGULATOR_DESC_BUCK5(8, MAX77235_REG_BUCK8OUT, MAX77235_REG_BUCK8CTRL),
	REGULATOR_DESC_BUCK5(9, MAX77235_REG_BUCK9OUT, MAX77235_REG_BUCK9CTRL),
	REGULATOR_DESC_BUCK5(10, MAX77235_REG_BUCK10OUT, MAX77235_REG_BUCK10CTRL),
	REGULATOR_DESC_BOOST(),
};

static int max77235_reg_hw_init(struct max77235_reg *max77235_reg,
		const struct max77235_reg_platform_data *pdata)
{
	const struct max77235 *max77235 = max77235_reg->max77235;
	char label[5] = "dvs0";
	unsigned int val;
	int i;
	int ret;

	ret = regmap_update_bits(max77235->regmap, MAX77235_REG_BUCK234FREQ,
			MAX77235_BUCK234FREQ_M, pdata->buck234_frequency);
	if (IS_ERR_VALUE(ret))
		return ret;

	ret = regmap_update_bits(max77235->regmap, MAX77235_REG_BUCK2CTRL2,
			MAX77234_FORCED_PWM_M |
			MAX77234_B2_1PHASEEN_M |
			MAX77234_B2_3PHASEEN_M,
			(pdata->forced_pwm_3ph ? MAX77234_FORCED_PWM_M : 0) |
			(pdata->forced_single_phase_operation ? MAX77234_B2_1PHASEEN_M : 0) |
			(pdata->forced_three_phase_operation ? MAX77234_B2_3PHASEEN_M : 0));
	if (IS_ERR_VALUE(ret))
		return ret;
	val = (pdata->phase_transition - MAX77256_BUCK2_MIN_uV) / MAX77256_BUCK2_STEP_uV;
	ret = regmap_write(max77235->regmap, MAX77235_REG_BUCK2PHTRAN, val);
	if (IS_ERR_VALUE(ret))
		return ret;

	for (i = 0; i < 3; i++)
		if (max77235_reg->dvs[i] > 0)
		{
			label[3] = '0' + i;
			ret = devm_gpio_request(max77235_reg->dev, max77235_reg->dvs[i], label);
			if (IS_ERR_VALUE(ret))
				return ret;

			gpio_direction_output(max77235_reg->dvs[i], 0);
			if (IS_ERR_VALUE(ret))
				return ret;
		}

	return ret;
}

static int max77235_reg_rail_init(struct max77235_reg *max77235_reg,
		const struct max77235_regulator_data *reg_data)
{
	const struct max77235 *max77235 = max77235_reg->max77235;
	const int id = (int)reg_data->id;

	switch((enum max77235_regulators)reg_data->id)
	{
	case MAX77235_LDO1 ... MAX77235_LDO6:
	case MAX77235_LDO8:
	case MAX77235_LDO10 ... MAX77235_LDO15:
	case MAX77235_LDO30:
		return regmap_update_bits(max77235->regmap,
				max77235_reg_desc[id].enable_reg - MAX77235_REG_LDO1CTRL1 + MAX77235_REG_LDO1CTRL2,
				MAX77235_LDO_OVCLMP_EN_M |
				MAX77235_ADSLDO_M,
				(reg_data->overvoltage_clamp ? MAX77235_LDO_OVCLMP_EN_M : 0) |
				(reg_data->active_discharge ? MAX77235_ADSLDO_M : 0));

	case MAX77235_LDO7:
	case MAX77235_LDO9:
	case MAX77235_LDO17 ... MAX77235_LDO29:
	case MAX77235_LDO32 ... MAX77235_LDO35:
		return regmap_update_bits(max77235->regmap,
				max77235_reg_desc[id].enable_reg - MAX77235_REG_LDO1CTRL1 + MAX77235_REG_LDO1CTRL2,
				MAX77235_LDO_OVCLMP_EN_M |
				MAX77235_LDO_COMP_M |
				MAX77235_ADSLDO_M,
				(reg_data->overvoltage_clamp ? MAX77235_LDO_OVCLMP_EN_M : 0) |
				((unsigned int)reg_data->compensation << MASK2SHIFT(MAX77235_LDO_COMP_M)) |
				(reg_data->active_discharge ? MAX77235_ADSLDO_M : 0));

	case MAX77235_BUCK2DVS1:
	case MAX77235_BUCK3DVS1:
	case MAX77235_BUCK4DVS1:
		return regmap_update_bits(max77235->regmap, max77235_reg_desc[id].enable_reg,
				MAX77235_BUCK2_EN_M |
				MAX77235_SD2DIS_M |
				MAX77235_FPWM_M |
				MAX77235_ROVS_ENBx_M |
				MAX77235_FSRADE_Bx_M,
				(reg_data->opmode << MASK2SHIFT(MAX77235_BUCK2_EN_M)) |
				(reg_data->active_discharge ? MAX77235_SD2DIS_M : 0) |
				(reg_data->forced_pwm ? MAX77235_FPWM_M : 0) |
				(reg_data->remote_output_voltage_sense ? MAX77235_ROVS_ENBx_M : 0) |
				(reg_data->falling_slew_rate_active_discharge ? MAX77235_FSRADE_Bx_M : 0));

	case MAX77235_BUCK1DVS1:
	case MAX77235_BUCK6DVS1:
	case MAX77235_BUCK5:
	case MAX77235_BUCK7 ... MAX77235_BUCK10:
		return regmap_update_bits(max77235->regmap, max77235_reg_desc[id].enable_reg,
				MAX77235_SD1DIS_M |
				MAX77235_FPWM_M |
				MAX77235_BUCK1_EN_M,
				(reg_data->active_discharge ? MAX77235_SD1DIS_M : 0) |
				(reg_data->forced_pwm ? MAX77235_FPWM_M : 0) |
				((unsigned int)reg_data->opmode << MASK2SHIFT(MAX77235_BUCK1_EN_M)));

	case MAX77235_BOOST:
		return regmap_update_bits(max77235->regmap, max77235_reg_desc[id].enable_reg,
				MAX77235_ALLOWABM_M |
				MAX77235_FORCEDBYP_M |
				MAX77235_BSTDIS_M,
				(reg_data->automatic_bypass_mode ? MAX77235_ALLOWABM_M : 0) |
				(reg_data->forced_bypass ? MAX77235_FORCEDBYP_M : 0) |
				(reg_data->active_discharge ? MAX77235_BSTDIS_M : 0));

	default:
		break;
	}

	return 0;
}


#ifdef CONFIG_OF
static struct max77235_reg_platform_data *max77235_reg_parse_dt(struct device *dev)
{
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *regulators_np, *np;
	struct max77235_reg_platform_data *pdata;
	struct max77235_regulator_data *regulator_data;
	u32 val;
	const char *str;
	int i;

	if (unlikely(nproot == NULL))
		return ERR_PTR(-EINVAL);

	regulators_np = of_find_node_by_name(nproot, "regulators");
	if (unlikely(regulators_np == NULL)) {
		dev_err(dev, "regulators node not found\n");
		return ERR_PTR(-EINVAL);
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);

	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	pdata->num_regulators = of_get_child_count(regulators_np);
	pdata->regulator_data = regulator_data = devm_kzalloc(dev,
			sizeof(*pdata->regulator_data) * pdata->num_regulators,
			GFP_KERNEL);
	if (unlikely(pdata->regulator_data == NULL)) {
		of_node_put(regulators_np);
		return ERR_PTR(-ENOMEM);
	}

	for_each_child_of_node(regulators_np, np) {
		for (i = 0; i < ARRAY_SIZE(max77235_reg_desc); i++) {
			if (!of_node_cmp(np->name, max77235_reg_desc[i].name)) {
				break;
			}
		}
		if (unlikely(i >= ARRAY_SIZE(max77235_reg_desc))) {
			dev_warn(dev, "Cannot find the regulator information for %s\n",
				np->name);
			continue;
		}
		regulator_data->id = i;

		if (!of_property_read_string(np, "opmode", &str) || !strcmp(str, "on"))
			regulator_data->opmode = MAX77235_OPMODE_ON;
		else if (!strcmp(str, "on-by-pwrreq"))
			regulator_data->opmode = MAX77235_OPMODE_ON_BY_PWRREQ;
		else if (!strcmp(str, "lp-by-pwrreq"))
			regulator_data->opmode = MAX77235_OPMODE_LP_BY_PWRREQ;
		else
		{
			dev_warn(dev, "parsing error : %s in %s\n", str, np->name);
			regulator_data->opmode = MAX77235_OPMODE_ON;
		}

		regulator_data->active_discharge = of_property_read_bool(np, "active-discharge");
		regulator_data->forced_pwm = of_property_read_bool(np, "forced-pwm");
		regulator_data->remote_output_voltage_sense =
				of_property_read_bool(np, "remote-output-voltage-sense");
		regulator_data->falling_slew_rate_active_discharge =
				of_property_read_bool(np, "falling-slew-rate-active-discharge");
		regulator_data->automatic_bypass_mode = of_property_read_bool(np, "automatic-bypass-mode");
		regulator_data->forced_bypass = of_property_read_bool(np, "forced-bypass");
		regulator_data->initdata = of_get_regulator_init_data(dev, np);
		regulator_data++;
	}

	if (!of_property_read_u32(regulators_np, "buck234-frequency", &val))
	{
		dev_warn(dev, "cannot file node 'buck234-frequency'. default = 2000hz\n");
		val = 2000;
	}
	if (unlikely(val < 800))
		val = 800;
	else if (unlikely(val > 3600))
		val = 3600;
	pdata->buck234_frequency = (enum max77235_buck_frequency)(0x07 - (val - 800) / 400);

	pdata->forced_pwm_3ph = of_property_read_bool(regulators_np, "forced-pwm-3ph");
	pdata->forced_single_phase_operation =
			of_property_read_bool(regulators_np, "forced-single-phase-operation");
	pdata->forced_three_phase_operation =
			of_property_read_bool(regulators_np, "forced-three-phase-operation");

	if (!of_property_read_u32(regulators_np, "phase-transition", &val))
	{
		dev_warn(dev, "cannot file node 'phase-transition'. default = 900000uV\n");
		val = 900000;
	}
	if (unlikely(val < MAX77256_BUCK2_MIN_uV))
		val = MAX77256_BUCK2_MIN_uV;
	else if (unlikely(val > MAX77256_BUCK2_MAX_uV))
		val = MAX77256_BUCK2_MAX_uV;
	pdata->phase_transition = val;

	if (!of_property_read_u32_array(regulators_np, "gpio-for-dvs", pdata->dvs, ARRAY_SIZE(pdata->dvs)))
	{
		dev_warn(dev, "property 'gpio-for-dvs' is wrong. DVS is disabled.\n");
	}

	if (!of_property_read_u32_array(regulators_np, "gpio-for-selb", pdata->selb, ARRAY_SIZE(pdata->selb)))
	{
		dev_warn(dev, "property 'gpio-for-dvs' is wrong. all SELBx is low.\n");
		for (i = 0; i < ARRAY_SIZE(pdata->selb); i++)
			pdata->selb[i] = -1;
	}

	return pdata;
}
#endif

static __init int max77235_reg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77235 *max77235 = dev_get_drvdata(dev->parent);
	struct max77235_reg_platform_data *pdata;
	struct max77235_reg *max77235_reg;
	struct max77235_reg_data *reg_data;
	struct regulator_dev *rdev;
	struct regulator_config config;
	int i = 0, selb_idx = 0, dvs_idx = 0, ret;
	char *selb_name = "max77235-reg-SELB1";
	char *dvs_name = "max77235-reg-DVS1";

	max77235_reg = devm_kzalloc(dev, sizeof(*max77235_reg), GFP_KERNEL);
	if (unlikely(max77235_reg == NULL))
		return -ENOMEM;

#ifdef CONFIG_OF
	pdata = max77235_reg_parse_dt(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev)
#endif

	reg_data = devm_kzalloc(dev, sizeof(struct max77235_reg_data) * pdata->num_regulators, GFP_KERNEL);
	if (unlikely(reg_data == NULL))
		return -ENOMEM;

	platform_set_drvdata(pdev, max77235_reg);
	max77235_reg->dev = dev;
	max77235_reg->max77235 = max77235;
	max77235_reg->reg_data = reg_data;
	max77235_reg->num_regulators = pdata->num_regulators;
	mutex_init(&max77235_reg->dvs_lock);

	ret = max77235_reg_hw_init(max77235_reg, pdata);
	if (IS_ERR_VALUE(ret))
		goto err;

	for (i = 0; i < pdata->num_regulators; i++)
	{
		reg_data->opmode = pdata->regulator_data[i].opmode;
		ret = max77235_reg_rail_init(max77235_reg, &pdata->regulator_data[i]);

		if (IS_ERR_VALUE(ret))
			goto err;

		config.dev = dev;
		config.init_data = pdata->regulator_data[i].initdata;
		config.driver_data = &reg_data[i];
		config.regmap = max77235->regmap;
		config.of_node = pdata->of_node;

		rdev = regulator_register(&max77235_reg_desc[pdata->regulator_data[i].id], &config);
		if (unlikely(IS_ERR(rdev)))
		{
			ret = PTR_ERR(rdev);
			dev_err(dev, "regulator register failed to register regulator %d\n", i);
			goto err;
		}
		reg_data[i].rdev = rdev;
	}

	for (i = 0; selb_idx < 5; selb_idx++)
	{
		if (pdata->selb[selb_idx] > 0)
		{
			selb_name[sizeof(selb_name) - 1] = selb_idx == 4 ? '6' : '1' + selb_idx;
			ret = devm_gpio_request(dev, pdata->selb[selb_idx], selb_name);
			if (IS_ERR_VALUE(ret))
				goto err;

			gpio_direction_output(pdata->selb[selb_idx], 1);
			max77235_reg->selb[i] = pdata->selb[selb_idx];
		}
		else
			max77235_reg->selb[i] = -1;
		max77235_reg->dvs_for_buck[i] = 0;
	}

	for (i = 0; dvs_idx < 3; dvs_idx++)
	{
		if (pdata->dvs[dvs_idx] > 0)
		{
			dvs_name[sizeof(dvs_name) - 1] = '1' + dvs_idx;
			ret = devm_gpio_request(dev, pdata->dvs[dvs_idx], dvs_name);
			if (IS_ERR_VALUE(ret))
				goto err;

			gpio_direction_output(pdata->dvs[dvs_idx], 0);
			max77235_reg->dvs[i] = pdata->dvs[dvs_idx];
		}
		else
			max77235_reg->dvs[i] = -1;
	}

#ifdef CONFIG_OF
	devm_kfree(dev, pdata);
#endif
	return 0;
err:
	while(--i >= 0)
		regulator_unregister(reg_data[i].rdev);

	dev_err(dev, "%s returns %d\n", __func__, ret);
	return ret;
}

static int max77235_reg_remove(struct platform_device *pdev)
{
	const struct max77235_reg *max77235 = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < max77235->num_regulators; i++)
		regulator_unregister(max77235->reg_data[i].rdev);

	return 0;
}

static const struct platform_device_id max77235_reg_id[] =
{
	{ "max77235-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, max77235_reg_id);

static struct platform_driver max77235_reg_driver =
{
	.driver =
	{
		.name = "max77235-regulator",
		.owner = THIS_MODULE,
	},
	.probe = max77235_reg_probe,
	.remove = max77235_reg_remove,
	.id_table = max77235_reg_id,
};

static int __init max77235_reg_init(void)
{
	return platform_driver_register(&max77235_reg_driver);
}

subsys_initcall(max77235_reg_init);

static void __exit max77235_reg_exit(void)
{
	platform_driver_unregister(&max77235_reg_driver);
}
module_exit(max77235_reg_exit);

MODULE_DESCRIPTION("MAXIM 77235 Regulator Driver");
MODULE_AUTHOR("Gyungoh Yoo <jack.yoo@maximintegrated.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:max77235-regulator");
MODULE_VERSION("0.4");
