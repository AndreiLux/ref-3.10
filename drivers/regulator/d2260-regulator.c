/*
 *  Regulator driver for Dialog D2260 PMIC
 *
 *  Copyright (C) 2013 Dialog Semiconductor Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/stringify.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <linux/mfd/d2260/pmic.h>
#include <linux/mfd/d2260/registers.h>
#include <linux/mfd/d2260/core.h>
#include <linux/mfd/d2260/pdata.h>
#include <linux/platform_data/odin_tz.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/regulator/of_regulator.h>
#endif

/* Define Lists */
#define D2260_DYNAMIC_MCTL_SUPPORT

#ifdef D2260_DYNAMIC_MCTL_SUPPORT
/*
	- Define MCTL pin connected to Host
*/

#define M_CTL0 0x0	/* M_CTL mode == 00, [1:0] bit */
#define M_CTL1 0x1	/* M_CTL mode == 01, [3:2] bit */
#define M_CTL2 0x2	/* M_CTL mode == 10, [5:4] bit */
#define M_CTL3 0x3	/* M_CTL mode == 11, [7:6] bit */

#define D2260_ACTIVE_MCTL	(M_CTL1)
#define D2260_SLEEP_MCTL		(M_CTL0)


/******************************************************************************
* if new combination of M_CTL pin, add below
*/

#if (D2260_ACTIVE_MCTL == M_CTL1)
#define D2260_ACTIVE_MCTL_MASK	(D2260_REGULATOR_MCTL1)
#define D2260_ACTIVE_MCTL_ON	(D2260_REGULATOR_MCTL1_ON)
#endif

#if (D2260_ACTIVE_MCTL == (M_CTL1 | M_CTL3))
#define D2260_ACTIVE_MCTL_MASK (D2260_REGULATOR_MCTL1 | D2260_REGULATOR_MCTL3)
#define D2260_ACTIVE_MCTL_ON \
	(D2260_REGULATOR_MCTL1_ON | D2260_REGULATOR_MCTL3_ON)
#endif

#if (D2260_SLEEP_MCTL == M_CTL0)
#define D2260_DSM_MCTL_MASK		(D2260_REGULATOR_MCTL0)

#define D2260_DSM_LPM_MCTL_BIT		(D2260_REGULATOR_MCTL0_SLEEP)
#define D2260_DSM_OFF_MCTL_BIT		(D2260_REGULATOR_MCTL0)
#define D2260_DSM_ON_MCTL_BIT		(D2260_REGULATOR_MCTL0_ON)
#endif

#if (D2260_SLEEP_MCTL == (M_CTL0 | M_CTL2))
#define D2260_DSM_MCTL_MASK		(D2260_REGULATOR_MCTL0 | D2260_REGULATOR_MCTL2)

#define D2260_DSM_LPM_MCTL_BIT \
	(D2260_REGULATOR_MCTL0_SLEEP | D2260_REGULATOR_MCTL2_SLEEP)
#define D2260_DSM_OFF_MCTL_BIT \
	(D2260_REGULATOR_MCTL0 | D2260_REGULATOR_MCTL2)
#define D2260_DSM_ON_MCTL_BIT \
	(D2260_REGULATOR_MCTL0_ON | D2260_REGULATOR_MCTL2_ON)
#endif

/*****************************************************************************/

#endif	/* D2260_DYNAMIC_MCTL_SUPPORT */

#define CSS0_PD_POWER      (0x1C0)

struct d2260_regulator {
	struct d2260 *d2260;
	int mctl_reg;
	u8 dsm_opmode;
	struct regulator_dev *rdev;
	struct regulator_desc *rdesc;
	int gpio_num;
	int gpo_num;
	bool css_gpio_requested;
	bool d2260_gpio_requested;
};

enum gpo_num {
	GPO_0 = 0,
	GPO_1,
	GPO_2,
};

static inline int get_global_mctl_mode(struct d2260 *d2260)
{
#ifdef D2260_DYNAMIC_MCTL_SUPPORT
	return D2260_ACTIVE_MCTL;
#else
	u8 reg_val;

	d2260_reg_read(d2260, D2260_STATUS_A_REG, &reg_val);

	return ((reg_val & D2260_M_CTL_MASK) >> D2260_M_CTL_BIT);
#endif
}

static unsigned int get_regulator_mctl_mode(struct d2260_regulator *regulator)
{
	struct d2260 *d2260 = regulator->d2260;
	u8 reg_val, mctl_reg, m_ctl;
	int ret = 0;

	mctl_reg = regulator->mctl_reg;
	m_ctl = get_global_mctl_mode(d2260);

	ret = d2260_reg_read(d2260, mctl_reg, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= (0x3 << (m_ctl+m_ctl));
	reg_val >>= (m_ctl+m_ctl);

	DIALOG_DEBUG(d2260->dev,"%s(%d) old (%d) mode = %d\n", __func__,
					rdev_get_id(regulator->rdev), m_ctl, reg_val);
	return reg_val;
}

static int set_regulator_mctl_mode(struct d2260_regulator *regulator, u8 mode)
{
	struct d2260 *d2260 = regulator->d2260;
	u8 reg_val, mctl_reg, m_ctl;
	int ret = 0;

	mctl_reg = regulator->mctl_reg;
	m_ctl = get_global_mctl_mode(d2260);

	ret = d2260_reg_read(d2260, mctl_reg, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= ~(0x3 << (m_ctl+m_ctl));
	reg_val |= (mode << (m_ctl+m_ctl));

	ret = d2260_reg_write(d2260, mctl_reg, reg_val);
	DIALOG_DEBUG(d2260->dev,"%s(%d) new (%d) mode = %d\n", __func__,
					rdev_get_id(regulator->rdev), m_ctl, mode);

	return ret;
}

/*
 * d2260_clk32k_enable
 */
void d2260_clk32k_enable(struct d2260 *d2260, int onoff)
{
	int ret = 0;

	if (d2260) {
		DIALOG_DEBUG(d2260->dev,"[%s] OUT1_32K onoff ->[%d]\n", __func__, onoff);

		if (onoff == 1)
			ret = d2260_set_bits(d2260,  D2260_ONKEY_CONT2_REG,
							D2260_OUT2_32K_EN_MASK);
		else
			ret = d2260_clear_bits(d2260,  D2260_ONKEY_CONT2_REG,
							D2260_OUT2_32K_EN_MASK);

		if (ret < 0)
			printk("Failed OUT1_32K on/off\n");
		return ret;
	}
	else {
		printk("Failed OUT1_32K on/off\n");
		return;
	}
}
EXPORT_SYMBOL(d2260_clk32k_enable);


static int d2260_regulator_enable(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	struct d2260 *d2260 = regulator->d2260;
	u8 reg_val;
	int ret = 0;
	unsigned int regulator_id = rdev_get_id(rdev);
	unsigned int mctl_reg;

	if (!d2260->mctl_status) {
		ret = regulator_enable_regmap(rdev);
		DIALOG_DEBUG(d2260->dev,"%s(%d) ret=%d\n", __func__,rdev_get_id(rdev),ret);
	}else {
		mctl_reg = regulator->mctl_reg;

		ret = d2260_reg_read(d2260, mctl_reg, &reg_val);
		if (ret < 0) {
			dev_err(d2260->dev, "I2C read error\n");
			return ret;
		}

#ifdef D2260_DYNAMIC_MCTL_SUPPORT
		reg_val &= ~D2260_ACTIVE_MCTL_MASK;   /* Clear mapped MCTL bits */
		reg_val |= D2260_ACTIVE_MCTL_ON;

		switch (regulator->dsm_opmode) {
			case D2260_REGULATOR_LPM_IN_DSM :
				reg_val &= ~D2260_DSM_MCTL_MASK;
				reg_val |= D2260_DSM_LPM_MCTL_BIT;
				break;
			case D2260_REGULATOR_OFF_IN_DSM :
				reg_val &= ~D2260_DSM_MCTL_MASK;
				break;
			case D2260_REGULATOR_ON_IN_DSM :
				reg_val &= ~D2260_DSM_MCTL_MASK;
				reg_val |= D2260_DSM_ON_MCTL_BIT;
				break;
		}
#else
		reg_val &= ~(D2260_REGULATOR_MCTL1 | D2260_REGULATOR_MCTL3);
		/* Clear MCTL11 and MCTL01 */
		reg_val |= (D2260_REGULATOR_MCTL1_ON | D2260_REGULATOR_MCTL3_ON);

		switch (regulator->dsm_opmode) {
			case D2260_REGULATOR_LPM_IN_DSM :
				reg_val &= ~(D2260_REGULATOR_MCTL0 | D2260_REGULATOR_MCTL2);
				reg_val |= (D2260_REGULATOR_MCTL0_SLEEP | D2260_REGULATOR_MCTL2_SLEEP);
				break;
			case D2260_REGULATOR_OFF_IN_DSM :
				reg_val &= ~(D2260_REGULATOR_MCTL0 | D2260_REGULATOR_MCTL2);
				break;
			case D2260_REGULATOR_ON_IN_DSM :
				reg_val &= ~(D2260_REGULATOR_MCTL0 | D2260_REGULATOR_MCTL2);
				reg_val |= (D2260_REGULATOR_MCTL0_ON | D2260_REGULATOR_MCTL2_ON);
				break;
		}
#endif
		ret |= d2260_reg_write(d2260, mctl_reg, reg_val);

		DIALOG_DEBUG(d2260->dev,"regl_id[%d], mctl_reg[0x%x], reg_val[0x%x]\n",
					regulator_id, mctl_reg, reg_val);
	}

	return ret;
}

/*
 * d2260_regulator_disable
 */
static int d2260_regulator_disable(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	struct d2260 *d2260 = regulator->d2260;
	unsigned int mctl_reg;
	int ret;

	if (!d2260->mctl_status) {
		ret = regulator_disable_regmap(rdev);
		DIALOG_DEBUG(d2260->dev,"%s(%d) ret=%d\n", __func__,rdev_get_id(rdev),ret);
	} else {
		mctl_reg = regulator->mctl_reg;
		ret = d2260_reg_write(d2260, mctl_reg, 0x00);
	}

	return ret;
}

static unsigned int d2260_regulator_get_mode(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	struct d2260 *d2260 = regulator->d2260;
	unsigned int mode = 0;

	mode = get_regulator_mctl_mode(regulator);

	/* Map d2260 regulator mode to Linux framework mode */
	switch (mode) {
		case REGULATOR_MCTL_TURBO:
			mode = REGULATOR_MODE_FAST;
			break;
		case REGULATOR_MCTL_ON:
			mode = REGULATOR_MODE_NORMAL;
			break;
		case REGULATOR_MCTL_SLEEP:
			mode = REGULATOR_MODE_IDLE;
			break;
		case REGULATOR_MCTL_OFF:
			mode = REGULATOR_MODE_STANDBY;
			break;
		default:
			/* unsupported or unknown mode */
			break;
	}

	DIALOG_DEBUG(d2260->dev,"[REGULATOR] : [%s] >> MODE(%d)\n", __func__, mode);

	return mode;
}

/*
 * d2260_regulator_set_mode
 */
static int d2260_regulator_set_mode(struct regulator_dev *rdev,
						unsigned int mode)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	struct d2260 *d2260 = regulator->d2260;
	int ret;
	u8 mctl_mode;

	DIALOG_DEBUG(d2260->dev,"[REGULATOR]:regulator_set_mode. mode is %d\n", mode);

	switch (mode) {
		case REGULATOR_MODE_FAST :
			mctl_mode = REGULATOR_MCTL_TURBO;
			break;
		case REGULATOR_MODE_NORMAL :
			mctl_mode = REGULATOR_MCTL_ON;
			break;
		case REGULATOR_MODE_IDLE :
			mctl_mode = REGULATOR_MCTL_SLEEP;
			break;
		case REGULATOR_MODE_STANDBY:
			mctl_mode = REGULATOR_MCTL_OFF;
			break;
		default:
			return -EINVAL;
	}

	ret = set_regulator_mctl_mode(regulator, mode);

	return ret;
}

static int d2260_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	struct d2260 *d2260 = regulator->d2260;
	unsigned int mctl_reg;
	int ret = -EINVAL;
	u8 reg_val = 0;

	if (!d2260->mctl_status) {
		return regulator_is_enabled_regmap(rdev);
	} else {
		mctl_reg = regulator->mctl_reg;
		ret = d2260_reg_read(d2260, mctl_reg, &reg_val);
		if (ret < 0) {
			dev_err(d2260->dev, "IPC read error.\n");
			return ret;
		}

		/* 0x0 : Off    * 0x1 : On    * 0x2 : Sleep    * 0x3 : n/a */
#ifdef D2260_DYNAMIC_MCTL_SUPPORT
		ret = ((reg_val & (D2260_ACTIVE_MCTL_MASK)) >= 1) ? 1 : 0;
#else
		ret = ((reg_val & (D2260_REGULATOR_MCTL1 | D2260_REGULATOR_MCTL3)) >= 1) ?
				1 : 0;
#endif
		return ret;
	}

}

static int pd_check(unsigned int pd)
{
	int power_on = 1, power_off = 0;
	u32 pd_info;

#ifdef CONFIG_ODIN_TEE
	pd_info = tz_read(0x200f1000 + pd);
	if ((pd_info & 0x7) == 0x7)
		return power_on;
#endif

	return power_off;
}

static int css_gpio_reg_is_enabled(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	int power_on = 1;
	u32 css_pd, val = 0;
	int ret;

	ret = pd_check(CSS0_PD_POWER);
	if (ret && regulator->css_gpio_requested == true) {
		/* temporary checking gpio value
		 * This function will be chagned to gpio_get_value */
		val = tz_read(0x330f0910 + 0x4);
		if ((val & 0x1) == 0x1) {
			return power_on;
		}
	}

	return 0;
}

static int css_gpio_reg_enable(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	int ret = -EINVAL;

	if (regulator->css_gpio_requested == false) {
		regulator-> css_gpio_requested = true;
		ret = gpio_request(regulator->rdesc->gpio_num, regulator->rdesc->name);
		if (ret < 0) {
			printk("%s err %d\n", __func__, ret);
			return -ENODEV;
		}
	}

	if (regulator-> css_gpio_requested == true) {
		ret = gpio_direction_output(regulator->rdesc->gpio_num, 1);
		if (ret < 0)
			goto err;
	}

	return 0;

err:
	BUG_ON(regulator->rdesc->gpio_num);
	return 0;
}

static int css_gpio_reg_disable(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	int ret = -EINVAL;
	int power_on;

	power_on = pd_check(CSS0_PD_POWER);
	if (power_on && (regulator-> css_gpio_requested == true)) {
		regulator-> css_gpio_requested = false;
		ret = gpio_direction_output(regulator->rdesc->gpio_num, 0);
		if (ret < 0)
			goto err;

		gpio_free(regulator->rdesc->gpio_num);
	}

	return 0;

err:
	BUG_ON(regulator->rdesc->gpio_num);
	return 0;
}

static int gpio_reg_is_enabled(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	struct d2260 *d2260 = regulator->d2260;
	u8 reg_val = 0;
	int ret = -EINVAL, power_on;

	ret = d2260_reg_read(d2260, D2260_TA_GPIO_1_REG, &reg_val);
	if (ret < 0) {
		dev_err(d2260->dev, "IPC read error.\n");
		return ret;
	}
	if ((reg_val & D2260_GPIO1_MODE_MASK) >> D2260_GPIO1_MODE_BIT)
		power_on = 1;
	else
		power_on = 0;

	return power_on;
}

static int gpio_reg_enable(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	int ret = -EINVAL;

	if (regulator->d2260_gpio_requested == false) {
		regulator->d2260_gpio_requested = true;
		ret = gpio_request(regulator->rdesc->gpio_num, regulator->rdesc->name);
		if (ret < 0) {
			printk("%s err %d\n", __func__, ret);
			return -ENODEV;
		}
	}
	if (regulator->d2260_gpio_requested == true) {
		ret = gpio_direction_output(regulator->rdesc->gpio_num, 1);
		if (ret < 0)
			goto err;
	}

	return 0;

err:
	BUG_ON(regulator->rdesc->gpio_num);
	return 0;
}

static int gpio_reg_disable(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	int ret = -EINVAL;

	if (regulator->d2260_gpio_requested == true) {
		regulator->d2260_gpio_requested = false;
		ret = gpio_direction_output(regulator->rdesc->gpio_num, 0);
		if (ret < 0)
			goto err;

		gpio_free(regulator->rdesc->gpio_num);
	}

	return 0;

err:
	BUG_ON(regulator->rdesc->gpio_num);
	return 0;
}

static int gpo_reg_is_enabled(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	struct d2260 *d2260 = regulator->d2260;
	u8 reg_val = 0;
	int ret = -EINVAL, power_on;

	ret = d2260_reg_read(d2260, D2260_GEN_CONF_1_REG, &reg_val);
	if (ret < 0) {
		dev_err(d2260->dev, "IPC read error.\n");
		return ret;
	}
	switch (regulator->gpo_num) {
	case GPO_0:
		reg_val |= 0x1;
		break;
	case GPO_1:
		reg_val |= 0x2;
		break;
	case GPO_2:
		reg_val |= 0x4;
		break;
	}

	power_on = (reg_val >> regulator->gpo_num);

	return power_on;
}

static int gpo_reg_enable(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	struct d2260 *d2260 = regulator->d2260;
	int ret = -EINVAL;
	u8 reg_val = 0;

	ret = d2260_reg_read(d2260, D2260_GEN_CONF_1_REG, &reg_val);
	if (ret < 0)
		goto err;

	switch (regulator->gpo_num) {
	case GPO_0:
		reg_val |= 0x1;
		break;
	case GPO_1:
		reg_val |= 0x2;
		break;
	case GPO_2:
		reg_val |= 0x4;
		break;
	}

	ret = d2260_reg_write(d2260, D2260_GEN_CONF_1_REG, reg_val);
	if (ret < 0)
		goto err;

        return 0;
err:
        BUG_ON(regulator->rdesc->gpio_num);
		return 0;
}

static int gpo_reg_disable(struct regulator_dev *rdev)
{
	struct d2260_regulator *regulator = rdev_get_drvdata(rdev);
	struct d2260 *d2260 = regulator->d2260;
	int ret = -EINVAL;
	u8 reg_val = 0;

	ret = d2260_reg_read(d2260, D2260_GEN_CONF_1_REG, &reg_val);
	if (ret < 0)
		goto err;

	switch (regulator->gpo_num) {
	case GPO_0:
		reg_val &= 0x6;
		break;
	case GPO_1:
		reg_val &= 0x5;
		break;
	case GPO_2:
		reg_val &= 0x3;
		break;
	}

	ret = d2260_reg_write(d2260, D2260_GEN_CONF_1_REG, ~(regulator->gpo_num));
	if (ret < 0)
		goto err;

	return 0;

err:
	BUG_ON(regulator->rdesc->gpio_num);
	return 0;
}

static struct regulator_ops d2260_regulator_ops = {
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.enable = d2260_regulator_enable,
	.disable = d2260_regulator_disable,
	.is_enabled = d2260_regulator_is_enabled,
	.get_mode = d2260_regulator_get_mode,
	.set_mode = d2260_regulator_set_mode,
};

#ifdef D2260_USING_PUMA_REGISTERS
static struct regulator_ops d2260_ganged_ops = {
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.get_mode = d2260_regulator_get_mode,
	.set_mode = d2260_regulator_set_mode,
};
#endif

static struct regulator_ops d2260_hostcntrl_ops = {
	.enable = d2260_regulator_enable,
	.disable = d2260_regulator_disable,
	.is_enabled = d2260_regulator_is_enabled,
	.get_mode = d2260_regulator_get_mode,
	.set_mode = d2260_regulator_set_mode,
};

static struct regulator_ops d2260_gpiocntrl_ops = {
        .enable = gpio_reg_enable,
        .disable = gpio_reg_disable,
		.is_enabled = gpio_reg_is_enabled,
};

static struct regulator_ops css_gpiocntrl_ops = {
        .enable = css_gpio_reg_enable,
        .disable = css_gpio_reg_disable,
		.is_enabled = css_gpio_reg_is_enabled,
};

static struct regulator_ops d2260_gpocntrl_ops = {
        .enable = gpo_reg_enable,
        .disable = gpo_reg_disable,
		.is_enabled = gpo_reg_is_enabled,
};

#define REGL(_name, _v_reg_, _v_mask_, _e_reg_, \
				_e_mask_, _min_uv, _step_uv, _num) \
	[D2260_##_name] = \
		{ \
			.name = __stringify(D2260_##_name), \
			.id = D2260_##_name, \
			.ops = &d2260_regulator_ops, \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
			.vsel_reg = D2260_##_v_reg_##_REG, \
			.vsel_mask =  D2260_##_v_mask_##_MASK, \
			.enable_reg = D2260_##_e_reg_##_REG, \
			.enable_mask = D2260_##_e_mask_##_MASK, \
			.min_uV = _min_uv, \
			.uV_step = _step_uv, \
			.n_voltages = _num, \
		}
#ifdef D2260_USING_PUMA_REGISTERS
#define DUAL(_name, _v_reg_, _v_mask_, _min_uv, _step_uv, _num) \
	[D2260_##_name] = \
		{ \
			.name = __stringify(D2260_##_name), \
			.id = D2260_##_name, \
			.ops = &d2260_ganged_ops, \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
			.vsel_reg = D2260_##_v_reg_##_REG, \
			.vsel_mask =  D2260_##_v_mask_##_MASK, \
			.min_uV = _min_uv, \
			.uV_step = _step_uv, \
			.n_voltages = _num, \
		}
#endif
#define CTRL(_name, _e_reg_, _e_mask_) \
	[D2260_##_name] = \
		{ \
			.name = __stringify(D2260_##_name), \
			.id = D2260_##_name, \
			.ops = &d2260_hostcntrl_ops, \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
			.enable_reg = D2260_##_e_reg_##_REG, \
			.enable_mask = D2260_##_e_mask_##_MASK, \
		}

#define GPIO_CTRL_PMIC(_name, _gpio_num) \
        [D2260_##_name] = \
		{ \
			.name = __stringify(D2260_##_name), \
			.id = D2260_##_name, \
			.ops = &d2260_gpiocntrl_ops, \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
			.gpio_num = _gpio_num,\
		}

#define GPO_CTRL_PMIC(_name, _gpo_num) \
        [D2260_##_name] = \
		{ \
			.name = __stringify(D2260_##_name), \
			.id = D2260_##_name, \
			.ops = &d2260_gpocntrl_ops, \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
			.gpio_num = _gpo_num,\
		}


#define GPIO_CTRL_CSS(_name, _gpio_num) \
        [D2260_##_name] = \
		{ \
			.name = __stringify(D2260_##_name), \
			.id = D2260_##_name, \
			.ops = &css_gpiocntrl_ops, \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
			.gpio_num = _gpio_num,\
		}

#ifdef D2260_USING_PUMA_REGISTERS
static struct regulator_desc d2260_regulator_desc[] = {
 REGL(BUCK_0, BUCK0_CONF0, VBUCK0, BUCK0_CONF0,
									   BUCK0_EN,     600000,  6250, 128),
 REGL(BUCK_1, BUCK1_CONF0, VBUCK1, BUCK1_CONF0,
									   BUCK1_EN,     600000,  6250, 128),
 REGL(BUCK_2, BUCK2_CONF0, VBUCK2, BUCK2_CONF0,
									   BUCK2_EN,     600000,  6250, 128),
 REGL(BUCK_3, BUCK3_CONF0, VBUCK3, BUCK3_CONF0,
									   BUCK3_EN,     600000,  6250, 128),
 REGL(BUCK_4, BUCK4_CONF0, VBUCK4, BUCK4_CONF0,
									   BUCK4_EN,    1400000,  6250, 128),
 DUAL(BUCK_5, BUCK5_CONF0, VBUCK5,
										     	 1400000,  6250, 128),
 REGL(BUCK_6, BUCK6_CONF0, VBUCK6, BUCK6_CONF0,
									   BUCK5AND6_EN,1400000,  6250, 128),
 CTRL(BUCK_7, 					   BUCKRF_CONF,
									RFBUCK_EN),
 REGL(LDO_1,  LDO1,    VLDO1,  LDO1,   LDO1_EN,      650000, 25000,  64),
 REGL(LDO_2,  LDO2,    VLDO2,  LDO2,   LDO2_EN,      650000, 25000,  64),
 REGL(LDO_3,  LDO3,    VLDO3,  LDO3,   LDO3_EN,     1200000, 50000,  43),
 REGL(LDO_4,  LDO4,    VLDO4,  LDO4,   LDO4_EN,      650000, 25000,  64),
 REGL(LDO_5,  LDO5,    VLDO5,  LDO5,   LDO5_EN,     1200000, 50000,  43),
 REGL(LDO_6,  LDO6,    VLDO6,  LDO6,   LDO6_EN,     1200000, 50000,  43),
 REGL(LDO_7,  LDO7,    VLDO7,  LDO7,   LDO7_EN,     1200000, 50000,  43),
 REGL(LDO_8,  LDO8,    VLDO8,  LDO8,   LDO8_EN,     1200000, 50000,  43),
 REGL(LDO_9,  LDO9,    VLDO9,  LDO9,   LDO9_EN,     1200000, 50000,  43),
 REGL(LDO_10, LDO10,   VLDO10, LDO10,  LDO10_EN,    1200000, 50000,  43),
 REGL(LDO_11, LDO11,   VLDO11, LDO11,  LDO11_EN,    1200000, 50000,  43),
 REGL(LDO_12, LDO12,   VLDO12, LDO12,  LDO12_EN,    1200000, 50000,  43),
 REGL(LDO_13, LDO13,   VLDO13, LDO13,  LDO13_EN,    1200000, 50000,  43),
 REGL(LDO_14, LDO14,   VLDO14, LDO14,  LDO14_EN,    1200000, 50000,  43),
 REGL(LDO_15, LDO15,   VLDO15, LDO15,  LDO15_EN,    1200000, 50000,  43),
 REGL(LDO_16, LDO16,   VLDO16, LDO16,  LDO16_EN,    1200000, 50000,  43),
 REGL(LDO_17, LDO17,   VLDO17, LDO17,  LDO17_EN,    1200000, 50000,  43),
 REGL(LDO_18, LDO18,   VLDO18, LDO18,  LDO18_EN,    1200000, 50000,  43),
 REGL(LDO_19, LDO19,   VLDO19, LDO19,  LDO19_EN,    1200000, 50000,  43),
 REGL(LDO_20, LDO20,   VLDO20, LDO20,  LDO20_EN,    1200000, 50000,  43),
 REGL(LDO_21, LDO21,   VLDO21, LDO21,  LDO21_EN,    1200000, 50000,  43),
 REGL(LDO_22, LDO22,   VLDO22, LDO22,  LDO22_EN,    1200000, 50000,  43),
 REGL(LDO_23, LDO23,   VLDO23, LDO23,  LDO23_EN,    1200000, 50000,  43),
 REGL(LDO_24, LDO24,   VLDO24, LDO24,  LDO24_EN,    1000000, 50000,  43),
 REGL(LDO_25, LDO25,   VLDO25, LDO25,  LDO25_EN,    1200000, 50000,  43),
 GPIO_CTRL_PMIC(GPIOCTRL_0, 511),
 GPIO_CTRL_CSS(GPIOCTRL_1, 173),
#ifdef D2260_GPO_ENABLE
 GPO_CTRL_PMIC(GPOCTRL_0, 0),
 GPO_CTRL_PMIC(GPOCTRL_1, 1),
 GPO_CTRL_PMIC(GPOCTRL_2, 2),
#endif
};
#endif
#ifdef D2260_USING_CHEETAH_REGISTERS
static struct regulator_desc d2200_regulator_desc[] = {
 REGL(BUCK_0, BUCK0_CONF0, VBUCK0,  BUCK0_CONF0, BUCK0_EN,  600000,  6250, 128),
 REGL(BUCK_1, BUCK1_CONF0, VBUCK1,  BUCK1_CONF0, BUCK1_EN,  600000,  6250, 128),
 REGL(BUCK_2, BUCK2_CONF0, VBUCK2,  BUCK2_CONF0, BUCK2_EN,  600000,  6250, 128),
 REGL(BUCK_3, BUCK3_CONF0, VBUCK3,  BUCK3_CONF0, BUCK3_EN, 1400000,  6250, 128),
 REGL(BUCK_4, BUCK4_CONF0, VBUCK4,  BUCK4_CONF0, BUCK4_EN,  600000,  6250, 128),
 REGL(BUCK_5, BUCK5_CONF0, VBUCK5,  BUCK5_CONF0, BUCK5_EN, 1400000,  6250, 128),
 REGL(BUCK_6, BUCK6_CONF0, VBUCK6,  BUCK6_CONF0, BUCK6_EN, 1400000,  6250, 128),
 CTRL(BUCK_7,                       BUCKRF_CONF, RFBUCK_EN),
 REGL(LDO_1,  LDO1,        VLDO1,   LDO1,        LDO1_EN,   650000, 25000,  64),
 REGL(LDO_2,  LDO2,        VLDO2,   LDO2,        LDO2_EN,  1200000, 50000,  64),
 REGL(LDO_3,  LDO3,        VLDO3,   LDO3,        LDO3_EN,  1200000, 50000,  64),
 REGL(LDO_4,  LDO4,        VLDO4,   LDO4,        LDO4_EN,  1200000, 50000,  64),
 REGL(LDO_5,  LDO5,        VLDO5,   LDO5,        LDO5_EN,  1200000, 50000,  64),
 REGL(LDO_6,  LDO6,        VLDO6,   LDO6,        LDO6_EN,  1200000, 50000,  64),
 REGL(LDO_7,  LDO7,        VLDO7,   LDO7,        LDO7_EN,  1200000, 50000,  64),
 REGL(LDO_8,  LDO8,        VLDO8,   LDO8,        LDO8_EN,  1200000, 50000,  64),
 REGL(LDO_9,  LDO9,        VLDO9,   LDO9,        LDO9_EN,  1200000, 50000,  64),
 REGL(LDO_10, LDO10,       VLDO10,  LDO10,       LDO10_EN, 1200000, 50000,  64),
 REGL(LDO_11, LDO11,       VLDO11,  LDO11,       LDO11_EN, 1200000, 50000,  64),
 REGL(LDO_12, LDO12,       VLDO12,  LDO12,       LDO12_EN, 1200000, 50000,  64),
 REGL(LDO_13, LDO13,       VLDO13,  LDO13,       LDO13_EN, 1200000, 50000,  64),
 REGL(LDO_14, LDO14,       VLDO14,  LDO14,       LDO14_EN, 1200000, 50000,  64),
 REGL(LDO_15, LDO15,       VLDO15,  LDO15,       LDO15_EN, 1200000, 50000,  64),
 REGL(LDO_16, LDO16,       VLDO16,  LDO16,       LDO16_EN, 1200000, 50000,  64),
 REGL(LDO_17, LDO17,       VLDO17,  LDO17,       LDO17_EN, 1200000, 50000,  64),
 REGL(LDO_18, LDO18,       VLDO18,  LDO18,       LDO18_EN, 1200000, 50000,  64),
 REGL(LDO_19, LDO19,       VLDO19,  LDO19,       LDO19_EN, 1200000, 50000,  64),
 REGL(LDO_20, LDO20,       VLDO20,  LDO20,       LDO20_EN, 1200000, 50000,  64),
 REGL(LDO_21, LDO21,       VLDO21,  LDO21,       LDO21_EN, 1200000, 50000,  64),
 REGL(LDO_22, LDO22,       VLDO22,  LDO22,       LDO22_EN, 1200000, 50000,  64),
 REGL(LDO_23, LDO23,       VLDO23,  LDO23,       LDO23_EN, 1200000, 50000,  64),
 REGL(LDO_24, LDO24,       VLDO24,  LDO24,       LDO24_EN, 1200000, 50000,  64),
 REGL(LDO_25, LDO25,       VLDO25,  LDO25,       LDO25_EN, 1200000, 50000,  64),
 GPIO_CTRL_PMIC(GPIOCTRL_0, 511),
 GPIO_CTRL_CSS(GPIOCTRL_1, 173),
#ifdef D2260_GPO_ENABLE
 GPO_CTRL_PMIC(GPOCTRL_0, 0),
 GPO_CTRL_PMIC(GPOCTRL_1, 1),
 GPO_CTRL_PMIC(GPOCTRL_2, 2),
#endif
};
#endif
static struct regulator_desc *find_regulator_desc(u8 chip_id, int id)
{
	struct regulator_desc *rdesc;
	int i;

	switch (chip_id) {
	case D2260:
#ifdef D2260_USING_PUMA_REGISTERS
	case D2200_AA:
	case D2260_AA:
		for (i = 0; i < ARRAY_SIZE(d2260_regulator_desc); i++) {
			rdesc = &d2260_regulator_desc[i];
			if (rdesc->id == id)
				return rdesc;
		}
		break;
#endif
#ifdef D2260_USING_CHEETAH_REGISTERS
	case D2200_AA:
	case D2260_AA:
		for (i = 0; i < ARRAY_SIZE(d2200_regulator_desc); i++) {
			rdesc = &d2200_regulator_desc[i];
			if (rdesc->id == id)
				return rdesc;
		}
		break;
#endif
	}

	return NULL;
}

static u8 get_dsm_opmode_from_value(u8 reg_val)
{
	u8 ret = D2260_REGULATOR_MAX;

#ifdef D2260_DYNAMIC_MCTL_SUPPORT
	reg_val &= D2260_DSM_MCTL_MASK;

	if (reg_val == 0)
		ret = D2260_REGULATOR_OFF_IN_DSM;
	if (reg_val == D2260_DSM_LPM_MCTL_BIT)
		ret = D2260_REGULATOR_LPM_IN_DSM;
	else if (reg_val == D2260_DSM_ON_MCTL_BIT)
		ret = D2260_REGULATOR_ON_IN_DSM;
#else
	reg_val &= (D2260_REGULATOR_MCTL0 | D2260_REGULATOR_MCTL2);

	if (reg_val == 0)
		ret = D2260_REGULATOR_OFF_IN_DSM;
	if (reg_val == (D2260_REGULATOR_MCTL0_SLEEP | D2260_REGULATOR_MCTL2_SLEEP))
		ret = D2260_REGULATOR_LPM_IN_DSM;
	else if (reg_val == (D2260_REGULATOR_MCTL0_ON | D2260_REGULATOR_MCTL2_ON))
		ret = D2260_REGULATOR_ON_IN_DSM;
#endif

	return ret;
}


static int d2260_regulator_probe(struct platform_device *pdev)
{
	struct d2260 *d2260 = dev_get_drvdata(pdev->dev.parent);
	struct regulator_config config = { };
	struct d2260_regulator *regulator;
        struct d2260_pdata *pdata;
	int regulator_id = -1;
	u8 dsm_opmode = 0, old_dsm_opmode = 0;
	int ret;

	regulator = devm_kzalloc(&pdev->dev, sizeof(struct d2260_regulator),
				GFP_KERNEL);
	if (!regulator)
		return -ENOMEM;

	regulator_id = pdev->id;
	if (regulator_id > D2260_NUMBER_OF_REGULATORS) {
		ret = -EINVAL;
		goto err;
	}

	DIALOG_DEBUG(d2260->dev,"%s(Starting Regulator) Chip=%d ID=%d\n",
					__FUNCTION__,d2260->chip_id,regulator_id);

	pdata = d2260->dev->platform_data;
	regulator->d2260 = d2260;

	regulator->rdesc = find_regulator_desc(d2260->chip_id, pdev->id);
	if (regulator->rdesc == NULL) {
		dev_err(&pdev->dev, "invalid regulator ID %d,%d specified\n",
					d2260->chip_id, pdev->id);
		ret = -EINVAL;
		goto err;
	}

	if (regulator_id < 0) {
		ret = -EINVAL;
		goto err;
 	} else if (regulator_id < D2260_LDO_1) {
		regulator->mctl_reg = D2260_BUCK0_MCTL_REG + regulator_id;
	} else if (regulator_id < D2260_NUMBER_OF_REGULATORS) {
		regulator->mctl_reg = D2260_LDO1_MCTL_REG + regulator_id - D2260_LDO_1;
	} else {
		ret = -EINVAL;
		goto err;
	}

	config.dev = &pdev->dev;
	config.driver_data = regulator;
	config.regmap = d2260->regmap;
	if ((pdata && pdata->regulators[pdev->id])
		&& (of_node_get(d2260->dev->of_node) == NULL) ) {

		config.init_data = pdata->regulators[pdev->id];
		dsm_opmode = (unsigned)config.init_data->driver_data;

	} else {
#ifdef CONFIG_OF
		struct device_node *nproot, *np;

		nproot = of_node_get(d2260->dev->of_node);
		if (!nproot)
			return -ENODEV;

		nproot = of_find_node_by_name(nproot, "regulators");
		if (!nproot)
			return -ENODEV;

		for_each_child_of_node(nproot, np) {
			const __be32 *dsm_op;
			if (!of_node_cmp(np->name, regulator->rdesc->name)) {
				config.init_data = of_get_regulator_init_data(&pdev->dev, np);
				config.of_node = np;
				dsm_op = of_get_property(np, "regulator-dsm-mode", NULL);
				if (dsm_op)
					dsm_opmode = be32_to_cpu(*dsm_op);

				/* GPIO Voltage Control Check */
				of_property_read_u32(np, "regulator-enable-gpio",
										&regulator->rdesc->gpio_num);

				/* GPO Voltage Control Check */
				of_property_read_u32(np, "regulator-enable-gpo",
										&regulator->gpo_num);

				DIALOG_DEBUG(d2260->dev,"[%s] dsm_opmode[%x] -----------\n",
										__func__, dsm_opmode);
 				break;
			}
		}
		of_node_put(nproot);
#endif
	}

	ret = d2260_reg_read(d2260, regulator->mctl_reg, &old_dsm_opmode);
	if (ret < 0)
		goto err;

#if !defined(MCTL_ENABLE_IN_SMS)
	ret = d2260_reg_write(d2260, regulator->mctl_reg, dsm_opmode);
	if (ret < 0)
		goto err;
#endif

	regulator->dsm_opmode = get_dsm_opmode_from_value(dsm_opmode);
	regulator->css_gpio_requested = false;
	regulator->d2260_gpio_requested = false;

	regulator->rdev = regulator_register(regulator->rdesc, &config);
	if (IS_ERR(regulator->rdev)) {
		dev_err(&pdev->dev, "failed to register regulator %s\n",
			regulator->rdesc->name);
		ret = PTR_ERR(regulator->rdev);
		goto err;
	}

	return 0;

err:
	devm_kfree(&pdev->dev, regulator);
	DIALOG_DEBUG(d2260->dev,"%s(%d) dsm-mode %2X --> %2X ret=%d\n",
				__func__, regulator_id, old_dsm_opmode, dsm_opmode, ret);
	return ret;
}

static int d2260_regulator_remove(struct platform_device *pdev)
{
	struct d2260_regulator *regulator = platform_get_drvdata(pdev);

	DIALOG_DEBUG(&pdev->dev, "%s(Regulator removed)\n",__FUNCTION__);
	regulator_unregister(regulator->rdev);
	return 0;
}

static struct platform_driver d2260_regulator_driver = {
	.probe  = d2260_regulator_probe,
	.remove = d2260_regulator_remove,
	.driver = {
		.name = "d2260-regulator",
	},
};

static int __init d2260_regulator_init(void)
{
	return platform_driver_register(&d2260_regulator_driver);
}
subsys_initcall(d2260_regulator_init);

static void __exit d2260_regulator_exit(void)
{
	platform_driver_unregister(&d2260_regulator_driver);
}
module_exit(d2260_regulator_exit);

MODULE_AUTHOR("William Seo <william.seo@diasemi.com>");
MODULE_AUTHOR("Tony Olech <anthony.olech@diasemi.com>");
MODULE_DESCRIPTION("D2260 voltage and current regulator driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:d2260-regulator");
