/*
 * bq2419x-charger-htc.c -- BQ24190/BQ24192/BQ24192i/BQ24193 Charger driver
 *
 * Copyright (c) 2014, HTC CORPORATION.  All rights reserved.
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power/bq2419x-charger-htc.h>
#include <linux/htc_battery_bq2419x.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#define MAX_STR_PRINT 50

#define bq_chg_err(bq, fmt, ...)			\
		dev_err(bq->dev, "Charging Fault: " fmt, ##__VA_ARGS__)

#define BQ2419X_INPUT_VINDPM_OFFSET	3880
#define BQ2419X_CHARGE_ICHG_OFFSET	512
#define BQ2419X_PRE_CHG_IPRECHG_OFFSET	128
#define BQ2419X_PRE_CHG_TERM_OFFSET	128
#define BQ2419X_CHARGE_VOLTAGE_OFFSET	3504

#define BQ2419X_I2C_RETRY_MAX_TIMES	(10)
#define BQ2419X_I2C_RETRY_DELAY_MS	(100)

/* input current limit */
static const unsigned int iinlim[] = {
	100, 150, 500, 900, 1200, 1500, 2000, 3000,
};

static const struct regmap_config bq2419x_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= BQ2419X_MAX_REGS,
};

struct bq2419x_reg_info {
	u8 mask;
	u8 val;
};

struct bq2419x_chip {
	struct device			*dev;
	struct regmap			*regmap;
	int				irq;
	struct mutex			mutex;

	int				gpio_otg_iusb;
	struct regulator_dev		*vbus_rdev;
	struct regulator_desc		vbus_reg_desc;
	struct regulator_init_data	vbus_reg_init_data;

	struct bq2419x_reg_info		ir_comp_therm;
	struct bq2419x_vbus_platform_data *vbus_pdata;
	struct bq2419x_charger_platform_data *charger_pdata;
	bool				emulate_input_disconnected;
	bool				is_charge_enable;
	bool				is_boost_enable;
	struct bq2419x_irq_notifier	*irq_notifier;
	struct dentry			*dentry;
};
struct bq2419x_chip *the_chip;

static int current_to_reg(const unsigned int *tbl,
			size_t size, unsigned int val)
{
	size_t i;

	for (i = 0; i < size; i++)
		if (val < tbl[i])
			break;
	return i > 0 ? i - 1 : -EINVAL;
}

static int bq2419x_val_to_reg(int val, int offset, int div, int nbits,
	bool roundup)
{
	int max_val = offset + (BIT(nbits) - 1) * div;

	if (val <= offset)
		return 0;

	if (val >= max_val)
		return BIT(nbits) - 1;

	if (roundup)
		return DIV_ROUND_UP(val - offset, div);
	else
		return (val - offset) / div;
}

static int bq2419x_update_bits(struct bq2419x_chip *bq2419x, unsigned int reg,
				unsigned int mask, unsigned int bits)
{
	int ret = 0;
	unsigned int retry;

	for (retry = 0; retry < BQ2419X_I2C_RETRY_MAX_TIMES; retry++) {
		ret = regmap_update_bits(bq2419x->regmap, reg, mask, bits);
		if (!ret)
			break;
		msleep(BQ2419X_I2C_RETRY_DELAY_MS);
	}

	if (ret)
		dev_err(bq2419x->dev,
			"i2c update error after retry %d times\n", retry);
	return ret;
}

static int bq2419x_read(struct bq2419x_chip *bq2419x, unsigned int reg,
				unsigned int *bits)
{
	int ret = 0;
	unsigned int retry;

	for (retry = 0; retry < BQ2419X_I2C_RETRY_MAX_TIMES; retry++) {
		ret = regmap_read(bq2419x->regmap, reg, bits);
		if (!ret)
			break;
		msleep(BQ2419X_I2C_RETRY_DELAY_MS);
	}

	if (ret)
		dev_err(bq2419x->dev,
			"i2c read error after retry %d times\n", retry);
	return ret;
}

#if 0	/* TODO: enable this if need later since it is unused now */
static int bq2419x_write(struct bq2419x_chip *bq2419x, unsigned int reg,
				unsigned int bits)
{
	int ret = 0;
	unsigned int retry;

	for (retry = 0; retry < BQ2419X_I2C_RETRY_MAX_TIMES; retry++) {
		ret = regmap_write(bq2419x->regmap, reg, bits);
		if (!ret)
			break;
		msleep(BQ2419X_I2C_RETRY_DELAY_MS);
	}

	if (ret)
		dev_err(bq2419x->dev,
			"i2c write error after retry %d times\n", retry);
	return ret;
}
#endif

static int bq2419x_set_charger_enable_config(struct bq2419x_chip *bq2419x,
						bool enable, bool is_otg)
{
	int ret;
	unsigned int bits;

	dev_info(bq2419x->dev, "charger configure, enable=%d is_otg=%d\n",
				enable, is_otg);

	if (!enable)
		bits = BQ2419X_DISABLE_CHARGE;
	else if (!is_otg)
		bits = BQ2419X_ENABLE_CHARGE;
	else
		bits = BQ2419X_ENABLE_VBUS;

	ret = bq2419x_update_bits(bq2419x, BQ2419X_PWR_ON_REG,
			BQ2419X_ENABLE_CHARGE_MASK, bits);
	if (ret < 0)
		dev_err(bq2419x->dev, "PWR_ON_REG update failed %d", ret);

	return ret;
}

static int bq2419x_set_charger_enable(bool enable, void *data)
{
	int ret;
	struct bq2419x_chip *bq2419x;

	if (!data)
		return -EINVAL;

	bq2419x = data;

	mutex_lock(&bq2419x->mutex);
	bq2419x->is_charge_enable = enable;
	bq2419x->is_boost_enable = false;

	ret = bq2419x_set_charger_enable_config(bq2419x, enable, false);
	mutex_unlock(&bq2419x->mutex);

	return ret;
}

static int bq2419x_set_charger_hiz(bool is_hiz, void *data)
{
	int ret;
	unsigned bits;
	struct bq2419x_chip *bq2419x;

	if (!data)
		return -EINVAL;

	bq2419x = data;

	if (bq2419x->emulate_input_disconnected || is_hiz)
		bits = BQ2419X_EN_HIZ;
	else
		bits = 0;

	ret = bq2419x_update_bits(bq2419x, BQ2419X_INPUT_SRC_REG,
			BQ2419X_EN_HIZ, bits);
	if (ret < 0)
		dev_err(bq2419x->dev,
				"INPUT_SRC_REG update failed %d\n", ret);

	return ret;
}

static int bq2419x_set_fastcharge_current(unsigned int current_ma, void *data)
{
	int ret;
	unsigned int bits;
	int ichg;
	struct bq2419x_chip *bq2419x;

	if (!data)
		return -EINVAL;

	bq2419x = data;

	ichg = bq2419x_val_to_reg(current_ma,
			BQ2419X_CHARGE_ICHG_OFFSET, 64, 6, 0);
	bits = ichg << 2;

	ret = bq2419x_update_bits(bq2419x, BQ2419X_CHRG_CTRL_REG,
			BQ2419X_CHRG_CTRL_ICHG_MASK, bits);
	if (ret < 0)
		dev_err(bq2419x->dev, "CHRG_CTRL_REG write failed %d\n", ret);

	return ret;
}

static int bq2419x_set_charge_voltage(unsigned int voltage_mv, void *data)
{
	int ret;
	unsigned int bits;
	int vreg;
	struct bq2419x_chip *bq2419x;

	if (!data)
		return -EINVAL;

	bq2419x = data;

	vreg = bq2419x_val_to_reg(voltage_mv,
			BQ2419X_CHARGE_VOLTAGE_OFFSET, 16, 6, 1);
	bits = vreg << 2;

	ret = bq2419x_update_bits(bq2419x, BQ2419X_VOLT_CTRL_REG,
			BQ2419x_VOLTAGE_CTRL_MASK, bits);

	if (ret < 0)
		dev_err(bq2419x->dev,
				"VOLT_CTRL_REG update failed %d\n", ret);

	return ret;
}

static int bq2419x_set_precharge_current(unsigned int current_ma, void *data)
{
	int ret;
	unsigned int bits;
	int iprechg;
	struct bq2419x_chip *bq2419x;

	if (!data)
		return -EINVAL;

	bq2419x = data;

	iprechg = bq2419x_val_to_reg(current_ma,
			BQ2419X_PRE_CHG_IPRECHG_OFFSET, 128, 4, 0);
	bits = iprechg << 4;

	ret = bq2419x_update_bits(bq2419x, BQ2419X_CHRG_TERM_REG,
			BQ2419X_CHRG_TERM_PRECHG_MASK, bits);
	if (ret < 0)
		dev_err(bq2419x->dev, "CHRG_TERM_REG write failed %d\n", ret);

	return ret;
}

static int bq2419x_set_termination_current(unsigned int current_ma, void *data)
{
	int ret;
	unsigned int bits;
	int iterm;
	struct bq2419x_chip *bq2419x;

	if (!data)
		return -EINVAL;

	bq2419x = data;

	if (current_ma == 0) {
		ret = bq2419x_update_bits(bq2419x, BQ2419X_TIME_CTRL_REG,
				BQ2419X_EN_TERM, 0);
		if (ret < 0)
			dev_err(bq2419x->dev,
				"TIME_CTRL_REG update failed %d\n", ret);
	} else {
		iterm = bq2419x_val_to_reg(current_ma,
				BQ2419X_PRE_CHG_TERM_OFFSET, 128, 4, 0);
		bits = iterm;
		ret = bq2419x_update_bits(bq2419x, BQ2419X_CHRG_TERM_REG,
				BQ2419X_CHRG_TERM_TERM_MASK, bits);
		if (ret < 0)
			dev_err(bq2419x->dev,
				"CHRG_TERM_REG update failed %d\n", ret);
	}

	return ret;
}

static int bq2419x_set_input_current(unsigned int current_ma, void *data)
{
	int ret = 0;
	int val;
	int floor = 0;
	struct bq2419x_chip *bq2419x;

	if (!data)
		return -EINVAL;

	bq2419x = data;

	val = current_to_reg(iinlim, ARRAY_SIZE(iinlim), current_ma);
	if (val < 0)
		return -EINVAL;

	floor = current_to_reg(iinlim, ARRAY_SIZE(iinlim), 500);
	if (floor < val && floor >= 0) {
		for (; floor <= val; floor++) {
			ret = bq2419x_update_bits(bq2419x,
					BQ2419X_INPUT_SRC_REG,
					BQ2419x_CONFIG_MASK, floor);
			if (ret < 0)
				dev_err(bq2419x->dev,
					"INPUT_SRC_REG update failed: %d\n",
					ret);
			udelay(BQ2419x_CHARGING_CURRENT_STEP_DELAY_US);
		}
	} else {
		ret = bq2419x_update_bits(bq2419x, BQ2419X_INPUT_SRC_REG,
				BQ2419x_CONFIG_MASK, val);
		if (ret < 0)
			dev_err(bq2419x->dev,
				"INPUT_SRC_REG update failed: %d\n", ret);
	}

	return ret;
}

static int bq2419x_set_dpm_input_voltage(unsigned int voltage_mv, void *data)
{
	int ret;
	unsigned int bits;
	int vindpm;
	struct bq2419x_chip *bq2419x;

	if (!data)
		return -EINVAL;

	bq2419x = data;

	vindpm = bq2419x_val_to_reg(voltage_mv,
			BQ2419X_INPUT_VINDPM_OFFSET, 80, 4, 0);
	bits = vindpm << 3;

	ret = bq2419x_update_bits(bq2419x, BQ2419X_INPUT_SRC_REG,
					BQ2419X_INPUT_VINDPM_MASK, bits);
	if (ret < 0)
		dev_err(bq2419x->dev, "INPUT_SRC_REG write failed %d\n",
									ret);

	return ret;
}

static int bq2419x_set_safety_timer_enable(bool enable, void *data)
{
	int ret;
	struct bq2419x_chip *bq2419x;

	if (!data)
		return -EINVAL;

	bq2419x = data;

	ret = bq2419x_update_bits(bq2419x, BQ2419X_TIME_CTRL_REG,
			BQ2419X_EN_SFT_TIMER_MASK, 0);
	if (ret < 0) {
		dev_err(bq2419x->dev, "TIME_CTRL_REG update failed: %d\n",
									ret);
		return ret;
	}

	if (!enable)
		return 0;

	/* reset saftty timer by setting 0 and then making 1 */
	ret = bq2419x_update_bits(bq2419x, BQ2419X_TIME_CTRL_REG,
			BQ2419X_EN_SFT_TIMER_MASK, BQ2419X_EN_SFT_TIMER_MASK);
	if (ret < 0) {
		dev_err(bq2419x->dev, "TIME_CTRL_REG update failed: %d\n",
									ret);
		return ret;
	}

	mutex_lock(&bq2419x->mutex);
	if (bq2419x->is_charge_enable && !bq2419x->is_boost_enable) {
		/* need to toggel the charging-enable bit from 1 to 0 to 1 */
		ret = bq2419x_update_bits(bq2419x, BQ2419X_PWR_ON_REG,
				BQ2419X_ENABLE_CHARGE_MASK, 0);
		if (ret < 0) {
			dev_err(bq2419x->dev, "PWR_ON_REG update failed %d\n",
									ret);
			goto done;
		}

		ret = bq2419x_update_bits(bq2419x, BQ2419X_PWR_ON_REG,
						BQ2419X_ENABLE_CHARGE_MASK,
						BQ2419X_ENABLE_CHARGE);
		if (ret < 0) {
			dev_err(bq2419x->dev, "PWR_ON_REG update failed %d\n",
									ret);
			goto done;
		}
	}
done:
	mutex_unlock(&bq2419x->mutex);

	return ret;
}

static int bq2419x_get_charger_state(unsigned int *state, void *data)
{
	int ret;
	unsigned int val;
	unsigned int now_state = 0;
	struct bq2419x_chip *bq2419x;

	if (!data || !state)
		return -EINVAL;

	bq2419x = data;

	ret = bq2419x_read(bq2419x, BQ2419X_SYS_STAT_REG, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "SYS_STAT_REG read failed: %d\n", ret);
		goto error;
	}

	if (((val & BQ2419x_VSYS_STAT_MASK) == BQ2419x_VSYS_STAT_BATT_LOW) ||
		((val & BQ2419x_THERM_STAT_MASK) ==
						BQ2419x_IN_THERM_REGULATION))
		now_state |= HTC_BATTERY_BQ2419X_IN_REGULATION;
	if ((val & BQ2419x_DPM_STAT_MASK) == BQ2419x_DPM_MODE)
		now_state |= HTC_BATTERY_BQ2419X_DPM_MODE;
	if ((val & BQ2419x_PG_STAT_MASK) == BQ2419x_POWER_GOOD)
		now_state |= HTC_BATTERY_BQ2419X_POWER_GOOD;
	if ((val & BQ2419x_CHRG_STATE_MASK) != BQ2419x_CHRG_STATE_NOTCHARGING)
		now_state |= HTC_BATTERY_BQ2419X_CHARGING;
	if ((val & BQ2419x_VBUS_STAT) != BQ2419x_VBUS_UNKNOWN)
		now_state |= HTC_BATTERY_BQ2419X_KNOWN_VBUS;

	*state = now_state;
error:
	return ret;
}

static int bq2419x_get_input_current(unsigned int *current_ma, void *data)
{
	int ret;
	unsigned int reg_val;
	struct bq2419x_chip *bq2419x;

	if (!data)
		return -EINVAL;

	bq2419x = data;

	if (!current_ma)
		return -EINVAL;

	ret = bq2419x_read(bq2419x, BQ2419X_INPUT_SRC_REG, &reg_val);
	if (ret < 0)
		dev_err(bq2419x->dev, "INPUT_SRC read failed: %d\n", ret);
	else
		*current_ma = iinlim[BQ2419x_CONFIG_MASK & reg_val];

	return ret;
}

struct htc_battery_bq2419x_ops bq2419x_ops = {
	.set_charger_enable = bq2419x_set_charger_enable,
	.set_charger_hiz = bq2419x_set_charger_hiz,
	.set_fastcharge_current = bq2419x_set_fastcharge_current,
	.set_charge_voltage = bq2419x_set_charge_voltage,
	.set_precharge_current = bq2419x_set_precharge_current,
	.set_termination_current = bq2419x_set_termination_current,
	.set_input_current = bq2419x_set_input_current,
	.set_dpm_input_voltage = bq2419x_set_dpm_input_voltage,
	.set_safety_timer_enable = bq2419x_set_safety_timer_enable,
	.get_charger_state = bq2419x_get_charger_state,
	.get_input_current = bq2419x_get_input_current,
};

static int bq2419x_vbus_enable(struct regulator_dev *rdev)
{
	struct bq2419x_chip *bq2419x = rdev_get_drvdata(rdev);
	int ret;

	dev_info(bq2419x->dev, "VBUS enabled, charging disabled\n");

	mutex_lock(&bq2419x->mutex);
	bq2419x->is_boost_enable = true;
	ret = bq2419x_set_charger_enable_config(bq2419x, true, true);
	if (ret < 0)
		dev_err(bq2419x->dev, "VBUS boost enable failed %d", ret);
	mutex_unlock(&bq2419x->mutex);

	return ret;
}

static int bq2419x_vbus_disable(struct regulator_dev *rdev)
{
	struct bq2419x_chip *bq2419x = rdev_get_drvdata(rdev);
	int ret;

	dev_info(bq2419x->dev, "VBUS disabled, charging enabled\n");

	mutex_lock(&bq2419x->mutex);
	bq2419x->is_boost_enable = false;
	ret = bq2419x_set_charger_enable_config(bq2419x,
			bq2419x->is_charge_enable, false);
	if (ret < 0)
		dev_err(bq2419x->dev, "Charger enable failed %d", ret);
	mutex_unlock(&bq2419x->mutex);

	return ret;
}

static int bq2419x_vbus_is_enabled(struct regulator_dev *rdev)
{
	struct bq2419x_chip *bq2419x = rdev_get_drvdata(rdev);
	int ret = 0;
	unsigned int val;

	ret = bq2419x_read(bq2419x, BQ2419X_PWR_ON_REG, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "PWR_ON_REG read failed %d", ret);
		ret = 0;
	} else
		ret = ((val & BQ2419X_ENABLE_CHARGE_MASK) ==
							BQ2419X_ENABLE_VBUS);

	return ret;
}

static struct regulator_ops bq2419x_vbus_ops = {
	.enable		= bq2419x_vbus_enable,
	.disable	= bq2419x_vbus_disable,
	.is_enabled	= bq2419x_vbus_is_enabled,
};

static int bq2419x_charger_init(struct bq2419x_chip *bq2419x)
{
	int ret;

	ret = bq2419x_update_bits(bq2419x, BQ2419X_THERM_REG,
		    bq2419x->ir_comp_therm.mask, bq2419x->ir_comp_therm.val);
	if (ret < 0)
		dev_err(bq2419x->dev, "THERM_REG write failed: %d\n", ret);

	ret = bq2419x_update_bits(bq2419x, BQ2419X_TIME_CTRL_REG,
			BQ2419X_TIME_JEITA_ISET, 0);
	if (ret < 0)
		dev_err(bq2419x->dev, "TIME_CTRL update failed: %d\n", ret);

	return ret;
}

static int bq2419x_init_vbus_regulator(struct bq2419x_chip *bq2419x,
		struct bq2419x_platform_data *pdata)
{
	int ret = 0;
	struct regulator_config rconfig = { };

	if (!pdata->vbus_pdata) {
		dev_err(bq2419x->dev, "No vbus platform data\n");
		return 0;
	}

	bq2419x->gpio_otg_iusb = pdata->vbus_pdata->gpio_otg_iusb;
	bq2419x->vbus_reg_desc.name = "data-vbus";
	bq2419x->vbus_reg_desc.ops = &bq2419x_vbus_ops;
	bq2419x->vbus_reg_desc.type = REGULATOR_VOLTAGE;
	bq2419x->vbus_reg_desc.owner = THIS_MODULE;
	bq2419x->vbus_reg_desc.enable_time = 8000;

	bq2419x->vbus_reg_init_data.supply_regulator	= NULL;
	bq2419x->vbus_reg_init_data.regulator_init	= NULL;
	bq2419x->vbus_reg_init_data.num_consumer_supplies	=
				pdata->vbus_pdata->num_consumer_supplies;
	bq2419x->vbus_reg_init_data.consumer_supplies	=
				pdata->vbus_pdata->consumer_supplies;
	bq2419x->vbus_reg_init_data.driver_data		= bq2419x;

	bq2419x->vbus_reg_init_data.constraints.name	= "data-vbus";
	bq2419x->vbus_reg_init_data.constraints.min_uV	= 0;
	bq2419x->vbus_reg_init_data.constraints.max_uV	= 5000000,
	bq2419x->vbus_reg_init_data.constraints.valid_modes_mask =
					REGULATOR_MODE_NORMAL |
					REGULATOR_MODE_STANDBY;
	bq2419x->vbus_reg_init_data.constraints.valid_ops_mask =
					REGULATOR_CHANGE_MODE |
					REGULATOR_CHANGE_STATUS |
					REGULATOR_CHANGE_VOLTAGE;

	if (gpio_is_valid(bq2419x->gpio_otg_iusb)) {
		ret = gpio_request_one(bq2419x->gpio_otg_iusb,
				GPIOF_OUT_INIT_HIGH, dev_name(bq2419x->dev));
		if (ret < 0) {
			dev_err(bq2419x->dev, "gpio request failed  %d\n", ret);
			return ret;
		}
	}

	/* Register the regulators */
	rconfig.dev = bq2419x->dev;
	rconfig.of_node = NULL;
	rconfig.init_data = &bq2419x->vbus_reg_init_data;
	rconfig.driver_data = bq2419x;
	bq2419x->vbus_rdev = devm_regulator_register(bq2419x->dev,
				&bq2419x->vbus_reg_desc, &rconfig);
	if (IS_ERR(bq2419x->vbus_rdev)) {
		ret = PTR_ERR(bq2419x->vbus_rdev);
		dev_err(bq2419x->dev,
			"VBUS regulator register failed %d\n", ret);
		goto scrub;
	}

	return ret;

scrub:
	if (gpio_is_valid(bq2419x->gpio_otg_iusb))
		gpio_free(bq2419x->gpio_otg_iusb);
	return ret;
}


static int bq2419x_fault_clear_sts(struct bq2419x_chip *bq2419x,
	unsigned int *reg09_val)
{
	int ret;
	unsigned int reg09_1, reg09_2;

	ret = bq2419x_read(bq2419x, BQ2419X_FAULT_REG, &reg09_1);
	if (ret < 0) {
		dev_err(bq2419x->dev, "FAULT_REG read failed: %d\n", ret);
		return ret;
	}

	ret = bq2419x_read(bq2419x, BQ2419X_FAULT_REG, &reg09_2);
	if (ret < 0)
		dev_err(bq2419x->dev, "FAULT_REG read failed: %d\n", ret);

	if (reg09_val) {
		unsigned int reg09 = 0;

		if ((reg09_1 | reg09_2) & BQ2419x_FAULT_WATCHDOG_FAULT)
			reg09 |= BQ2419x_FAULT_WATCHDOG_FAULT;
		if ((reg09_1 | reg09_2) & BQ2419x_FAULT_BOOST_FAULT)
			reg09 |= BQ2419x_FAULT_BOOST_FAULT;
		if ((reg09_1 | reg09_2) & BQ2419x_FAULT_BAT_FAULT)
			reg09 |= BQ2419x_FAULT_BAT_FAULT;
		if (((reg09_1 & BQ2419x_FAULT_CHRG_FAULT_MASK) ==
				BQ2419x_FAULT_CHRG_SAFTY) ||
			((reg09_2 & BQ2419x_FAULT_CHRG_FAULT_MASK) ==
				BQ2419x_FAULT_CHRG_SAFTY))
			reg09 |= BQ2419x_FAULT_CHRG_SAFTY;
		else if (((reg09_1 & BQ2419x_FAULT_CHRG_FAULT_MASK) ==
				BQ2419x_FAULT_CHRG_INPUT) ||
			((reg09_2 & BQ2419x_FAULT_CHRG_FAULT_MASK) ==
				BQ2419x_FAULT_CHRG_INPUT))
			reg09 |= BQ2419x_FAULT_CHRG_INPUT;
		else if (((reg09_1 & BQ2419x_FAULT_CHRG_FAULT_MASK) ==
				BQ2419x_FAULT_CHRG_THERMAL) ||
			((reg09_2 & BQ2419x_FAULT_CHRG_FAULT_MASK) ==
				BQ2419x_FAULT_CHRG_THERMAL))
			reg09 |= BQ2419x_FAULT_CHRG_THERMAL;

		reg09 |= reg09_2 & BQ2419x_FAULT_NTC_FAULT;
		*reg09_val = reg09;
	}
	return ret;
}

static int bq2419x_watchdog_init_disable(struct bq2419x_chip *bq2419x)
{
	int ret;

	/* TODO: support watch dog enable if need */
	ret = bq2419x_update_bits(bq2419x, BQ2419X_TIME_CTRL_REG,
			BQ2419X_WD_MASK, 0);
	if (ret < 0)
		dev_err(bq2419x->dev, "TIME_CTRL_REG read failed: %d\n",
				ret);

	return ret;
}

static irqreturn_t bq2419x_irq(int irq, void *data)
{
	struct bq2419x_chip *bq2419x = data;
	int ret;
	unsigned int val;

	ret = bq2419x_fault_clear_sts(bq2419x, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "fault clear status failed %d\n", ret);
		val = 0;
	}

	dev_info(bq2419x->dev, "%s() Irq %d status 0x%02x\n",
		__func__, irq, val);

	if (val & BQ2419x_FAULT_BOOST_FAULT)
		bq_chg_err(bq2419x, "VBUS Overloaded\n");

	mutex_lock(&bq2419x->mutex);
	if (!bq2419x->is_charge_enable) {
		bq2419x_set_charger_enable_config(bq2419x, false, false);
		mutex_unlock(&bq2419x->mutex);
		return IRQ_HANDLED;
	}
	mutex_unlock(&bq2419x->mutex);

	if (val & BQ2419x_FAULT_WATCHDOG_FAULT)
		bq_chg_err(bq2419x, "WatchDog Expired\n");

	switch (val & BQ2419x_FAULT_CHRG_FAULT_MASK) {
	case BQ2419x_FAULT_CHRG_INPUT:
		bq_chg_err(bq2419x,
			"input fault (VBUS OVP OR VBAT<VBUS<3.8V)\n");
		break;
	case BQ2419x_FAULT_CHRG_THERMAL:
		bq_chg_err(bq2419x, "Thermal shutdown\n");
		break;
	case BQ2419x_FAULT_CHRG_SAFTY:
		bq_chg_err(bq2419x, "Safety timer expiration\n");
		htc_battery_bq2419x_notify(
				HTC_BATTERY_BQ2419X_SAFETY_TIMER_TIMEOUT);
		break;
	default:
		break;
	}

	if (val & BQ2419x_FAULT_NTC_FAULT)
		bq_chg_err(bq2419x, "NTC fault %d\n",
				val & BQ2419x_FAULT_NTC_FAULT);

	ret = bq2419x_read(bq2419x, BQ2419X_SYS_STAT_REG, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "SYS_STAT_REG read failed %d\n", ret);
		val = 0;
	}

	if ((val & BQ2419x_CHRG_STATE_MASK) == BQ2419x_CHRG_STATE_CHARGE_DONE)
		dev_info(bq2419x->dev,
			"charging done interrupt found\n");

	if ((val & BQ2419x_VSYS_STAT_MASK) == BQ2419x_VSYS_STAT_BATT_LOW)
		dev_info(bq2419x->dev,
			"in VSYSMIN regulation, battery is too low\n");

	return IRQ_HANDLED;
}

static int bq2419x_show_chip_version(struct bq2419x_chip *bq2419x)
{
	int ret;
	unsigned int val;

	ret = bq2419x_read(bq2419x, BQ2419X_REVISION_REG, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "REVISION_REG read failed: %d\n", ret);
		return ret;
	}

	if ((val & BQ24190_IC_VER) == BQ24190_IC_VER)
		dev_info(bq2419x->dev, "chip type BQ24190 detected\n");
	else if ((val & BQ24192_IC_VER) == BQ24192_IC_VER)
		dev_info(bq2419x->dev, "chip type BQ24192/3 detected\n");
	else if ((val & BQ24192i_IC_VER) == BQ24192i_IC_VER)
		dev_info(bq2419x->dev, "chip type BQ24192i detected\n");
	return 0;
}


static ssize_t bq2419x_show_input_charging_current(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	unsigned int reg_val;
	int ret;

	ret = bq2419x_read(bq2419x, BQ2419X_INPUT_SRC_REG, &reg_val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "INPUT_SRC read failed: %d\n", ret);
		return ret;
	}
	ret = iinlim[BQ2419x_CONFIG_MASK & reg_val];
	return snprintf(buf, MAX_STR_PRINT, "%d mA\n", ret);
}

static ssize_t bq2419x_set_input_charging_current(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int in_current_limit;
	char *p = (char *)buf;
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);

	in_current_limit = memparse(p, &p);
	ret = bq2419x_set_input_current(in_current_limit, bq2419x);
	if (ret  < 0) {
		dev_err(dev, "Current %d mA configuration faild: %d\n",
			in_current_limit, ret);
		return ret;
	}
	return count;
}

static ssize_t bq2419x_show_charging_state(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	unsigned int reg_val;
	int ret;

	ret = bq2419x_read(bq2419x, BQ2419X_PWR_ON_REG, &reg_val);
	if (ret < 0) {
		dev_err(dev, "BQ2419X_PWR_ON register read failed: %d\n", ret);
		return ret;
	}

	if ((reg_val & BQ2419X_ENABLE_CHARGE_MASK) == BQ2419X_ENABLE_CHARGE)
		return snprintf(buf, MAX_STR_PRINT, "enabled\n");

	return snprintf(buf, MAX_STR_PRINT, "disabled\n");
}

static ssize_t bq2419x_set_charging_state(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	bool enabled;
	int ret;

	if ((*buf == 'E') || (*buf == 'e'))
		enabled = true;
	else if ((*buf == 'D') || (*buf == 'd'))
		enabled = false;
	else
		return -EINVAL;

	ret = bq2419x_set_charger_enable(enabled, bq2419x);
	if (ret < 0) {
		dev_err(bq2419x->dev, "user charging enable failed, %d\n", ret);
		return ret;
	}
	return count;
}

static ssize_t bq2419x_show_input_cable_state(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	unsigned int reg_val;
	int ret;

	ret = bq2419x_read(bq2419x, BQ2419X_INPUT_SRC_REG, &reg_val);
	if (ret < 0) {
		dev_err(dev, "BQ2419X_PWR_ON register read failed: %d\n", ret);
		return ret;
	}

	if ((reg_val & BQ2419X_EN_HIZ) == BQ2419X_EN_HIZ)
		return snprintf(buf, MAX_STR_PRINT, "Disconnected\n");
	else
		return snprintf(buf, MAX_STR_PRINT, "Connected\n");
}

static ssize_t bq2419x_set_input_cable_state(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	bool connect;
	int ret;

	if ((*buf == 'C') || (*buf == 'c'))
		connect = true;
	else if ((*buf == 'D') || (*buf == 'd'))
		connect = false;
	else
		return -EINVAL;

	bq2419x->emulate_input_disconnected = connect;

	ret = bq2419x_set_charger_hiz(connect, bq2419x);
	if (ret < 0) {
		dev_err(bq2419x->dev, "register update failed, %d\n", ret);
		return ret;
	}
	if (connect)
		dev_info(bq2419x->dev,
			"Emulation of charger cable disconnect disabled\n");
	else
		dev_info(bq2419x->dev,
			"Emulated as charger cable Disconnected\n");
	return count;
}

static ssize_t bq2419x_show_output_charging_current(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	int ret;
	unsigned int data;

	ret = bq2419x_read(bq2419x, BQ2419X_CHRG_CTRL_REG, &data);
	if (ret < 0) {
		dev_err(bq2419x->dev, "CHRG_CTRL read failed %d", ret);
		return ret;
	}
	data >>= 2;
	data = data * 64 + BQ2419X_CHARGE_ICHG_OFFSET;
	return snprintf(buf, MAX_STR_PRINT, "%u mA\n", data);
}

static ssize_t bq2419x_set_output_charging_current(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	int curr_val, ret;
	int ichg;

	if (kstrtouint(buf, 0, &curr_val)) {
		dev_err(dev, "\nfile: %s, line=%d return %s()",
					__FILE__, __LINE__, __func__);
		return -EINVAL;
	}

	ichg = bq2419x_val_to_reg(curr_val, BQ2419X_CHARGE_ICHG_OFFSET,
						64, 6, 0);
	ret = bq2419x_update_bits(bq2419x, BQ2419X_CHRG_CTRL_REG,
				BQ2419X_CHRG_CTRL_ICHG_MASK, ichg << 2);

	return count;
}

static ssize_t bq2419x_show_output_charging_current_values(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int i, ret = 0;

	for (i = 0; i <= 63; i++)
		ret += snprintf(buf + strlen(buf), MAX_STR_PRINT,
				"%d mA\n", i * 64 + BQ2419X_CHARGE_ICHG_OFFSET);

	return ret;
}

static DEVICE_ATTR(output_charging_current, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		bq2419x_show_output_charging_current,
		bq2419x_set_output_charging_current);

static DEVICE_ATTR(output_current_allowed_values, S_IRUGO,
		bq2419x_show_output_charging_current_values, NULL);

static DEVICE_ATTR(input_charging_current_ma, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		bq2419x_show_input_charging_current,
		bq2419x_set_input_charging_current);

static DEVICE_ATTR(charging_state, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		bq2419x_show_charging_state, bq2419x_set_charging_state);

static DEVICE_ATTR(input_cable_state, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		bq2419x_show_input_cable_state, bq2419x_set_input_cable_state);

static struct attribute *bq2419x_attributes[] = {
	&dev_attr_output_charging_current.attr,
	&dev_attr_output_current_allowed_values.attr,
	&dev_attr_input_charging_current_ma.attr,
	&dev_attr_charging_state.attr,
	&dev_attr_input_cable_state.attr,
	NULL
};

static const struct attribute_group bq2419x_attr_group = {
	.attrs = bq2419x_attributes,
};

static int bq2419x_debugfs_show(struct seq_file *s, void *unused)
{
	struct bq2419x_chip *bq2419x = s->private;
	int ret;
	u8 reg;
	unsigned int data;

	for (reg = BQ2419X_INPUT_SRC_REG; reg <= BQ2419X_REVISION_REG; reg++) {
		ret = bq2419x_read(bq2419x, reg, &data);
	if (ret < 0)
		dev_err(bq2419x->dev, "reg %u read failed %d", reg, ret);
	else
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, data);
	}

	return 0;
}

static int bq2419x_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, bq2419x_debugfs_show, inode->i_private);
}

static const struct file_operations bq2419x_debugfs_fops = {
	.open		= bq2419x_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct bq2419x_platform_data *bq2419x_dt_parse(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct bq2419x_platform_data *pdata;
	struct device_node *batt_reg_node;
	struct device_node *vbus_reg_node;
	int ret;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);


	batt_reg_node = of_find_node_by_name(np, "charger");
	if (batt_reg_node) {
		const char *status_str;
		u32 pval;
		struct bq2419x_charger_platform_data *charger_pdata;

		status_str = of_get_property(batt_reg_node, "status", NULL);
		if (status_str && !(!strcmp(status_str, "okay"))) {
			dev_info(&client->dev,
					"charger node status is disabled\n");
			goto vbus_node;
		}

		pdata->bcharger_pdata = devm_kzalloc(&client->dev,
				sizeof(*(pdata->bcharger_pdata)), GFP_KERNEL);
		if (!pdata->bcharger_pdata)
			return ERR_PTR(-ENOMEM);

		charger_pdata = pdata->bcharger_pdata;

		ret = of_property_read_u32(batt_reg_node,
				"ti,ir-comp-resister-ohm", &pval);
		if (!ret)
			charger_pdata->ir_compensation_resister_ohm = pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,ir-comp-voltage-millivolt",	&pval);
		if (!ret)
			charger_pdata->ir_compensation_voltage_mv = pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,thermal-regulation-threshold-degc",
				&pval);
		if (!ret)
			charger_pdata->thermal_regulation_threshold_degc =
				pval;
	}

vbus_node:
	vbus_reg_node = of_find_node_by_name(np, "vbus");
	if (vbus_reg_node) {
		struct regulator_init_data *vbus_init_data;

		pdata->vbus_pdata = devm_kzalloc(&client->dev,
			sizeof(*(pdata->vbus_pdata)), GFP_KERNEL);
		if (!pdata->vbus_pdata)
			return ERR_PTR(-ENOMEM);

		vbus_init_data = of_get_regulator_init_data(
					&client->dev, vbus_reg_node);
		if (!vbus_init_data)
			return ERR_PTR(-EINVAL);

		pdata->vbus_pdata->consumer_supplies =
				vbus_init_data->consumer_supplies;
		pdata->vbus_pdata->num_consumer_supplies =
				vbus_init_data->num_consumer_supplies;
		pdata->vbus_pdata->gpio_otg_iusb =
				of_get_named_gpio(vbus_reg_node,
					"ti,otg-iusb-gpio", 0);
	}

	return pdata;
}

static int bq2419x_process_charger_plat_data(struct bq2419x_chip *bq2419x,
		struct bq2419x_charger_platform_data *chg_pdata)
{
	int ir_compensation_resistor;
	int ir_compensation_voltage;
	int thermal_regulation_threshold;
	int bat_comp, vclamp, treg;

	if (chg_pdata) {
		ir_compensation_resistor =
			chg_pdata->ir_compensation_resister_ohm ?: 70;
		ir_compensation_voltage =
			chg_pdata->ir_compensation_voltage_mv ?: 112;
		thermal_regulation_threshold =
			chg_pdata->thermal_regulation_threshold_degc ?: 100;
	} else {
		ir_compensation_resistor = 70;
		ir_compensation_voltage = 112;
		thermal_regulation_threshold = 100;
	}

	bat_comp = ir_compensation_resistor / 10;
	bq2419x->ir_comp_therm.mask = BQ2419X_THERM_BAT_COMP_MASK;
	bq2419x->ir_comp_therm.val = bat_comp << 5;
	vclamp = ir_compensation_voltage / 16;
	bq2419x->ir_comp_therm.mask |= BQ2419X_THERM_VCLAMP_MASK;
	bq2419x->ir_comp_therm.val |= vclamp << 2;
	bq2419x->ir_comp_therm.mask |= BQ2419X_THERM_TREG_MASK;
	if (thermal_regulation_threshold <= 60)
		treg = 0;
	else if (thermal_regulation_threshold <= 80)
		treg = 1;
	else if (thermal_regulation_threshold <= 100)
		treg = 2;
	else
		treg = 3;
	bq2419x->ir_comp_therm.val |= treg;

	return 0;
}

static int bq2419x_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct bq2419x_chip *bq2419x;
	struct bq2419x_platform_data *pdata = NULL;
	int ret = 0;

	if (client->dev.platform_data)
		pdata = client->dev.platform_data;

	if (!pdata && client->dev.of_node) {
		pdata = bq2419x_dt_parse(client);
		if (IS_ERR(pdata)) {
			ret = PTR_ERR(pdata);
			dev_err(&client->dev, "Parsing of node failed, %d\n",
				ret);
			return ret;
		}
	}

	if (!pdata) {
		dev_err(&client->dev, "No Platform data");
		return -EINVAL;
	}

	bq2419x = devm_kzalloc(&client->dev, sizeof(*bq2419x), GFP_KERNEL);
	if (!bq2419x) {
		dev_err(&client->dev, "Memory allocation failed\n");
		return -ENOMEM;
	}

	bq2419x->regmap = devm_regmap_init_i2c(client, &bq2419x_regmap_config);
	if (IS_ERR(bq2419x->regmap)) {
		ret = PTR_ERR(bq2419x->regmap);
		dev_err(&client->dev, "regmap init failed with err %d\n", ret);
		return ret;
	}

	bq2419x->charger_pdata = pdata->bcharger_pdata;
	bq2419x->vbus_pdata = pdata->vbus_pdata;

	bq2419x->dev = &client->dev;
	i2c_set_clientdata(client, bq2419x);
	bq2419x->irq = client->irq;

	ret = bq2419x_show_chip_version(bq2419x);
	if (ret < 0) {
		dev_err(&client->dev, "version read failed %d\n", ret);
		return ret;
	}

	ret = sysfs_create_group(&client->dev.kobj, &bq2419x_attr_group);
	if (ret < 0) {
		dev_err(&client->dev, "sysfs create failed %d\n", ret);
		return ret;
	}

	mutex_init(&bq2419x->mutex);

	bq2419x_process_charger_plat_data(bq2419x, pdata->bcharger_pdata);

	ret = bq2419x_charger_init(bq2419x);
	if (ret < 0) {
		dev_err(bq2419x->dev, "Charger init failed: %d\n", ret);
		goto scrub_mutex;
	}

	ret = bq2419x_watchdog_init_disable(bq2419x);
	if (ret < 0) {
		dev_err(bq2419x->dev, "WDT init failed %d\n", ret);
		goto scrub_mutex;
	}

	ret = bq2419x_fault_clear_sts(bq2419x, NULL);
	if (ret < 0) {
		dev_err(bq2419x->dev, "fault clear status failed %d\n", ret);
		goto scrub_mutex;
	}

	ret = devm_request_threaded_irq(bq2419x->dev, bq2419x->irq, NULL,
		bq2419x_irq, IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
			dev_name(bq2419x->dev), bq2419x);
	if (ret < 0) {
		dev_warn(bq2419x->dev, "request IRQ %d fail, err = %d\n",
				bq2419x->irq, ret);
		dev_info(bq2419x->dev,
			"Supporting bq driver without interrupt\n");
	}

	the_chip = bq2419x;

	ret = htc_battery_bq2419x_charger_register(&bq2419x_ops, bq2419x);
	if (ret < 0) {
		dev_err(&client->dev,
			"htc_battery_bq2419x register failed %d\n", ret);
		goto scrub_mutex;
	}

	ret = bq2419x_init_vbus_regulator(bq2419x, pdata);
	if (ret < 0) {
		dev_err(&client->dev, "VBUS regulator init failed %d\n", ret);
		goto scrub_policy;
	}

	bq2419x->dentry = debugfs_create_file("bq2419x-regs", S_IRUSR, NULL,
					      bq2419x, &bq2419x_debugfs_fops);
	return 0;

scrub_policy:
	htc_battery_bq2419x_charger_unregister(bq2419x);

scrub_mutex:
	mutex_destroy(&bq2419x->mutex);
	return ret;
}

static int bq2419x_remove(struct i2c_client *client)
{
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);

	htc_battery_bq2419x_charger_unregister(bq2419x);
	debugfs_remove(bq2419x->dentry);
	mutex_destroy(&bq2419x->mutex);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int bq2419x_resume(struct device *dev)
{
	int ret = 0;
	struct bq2419x_chip *bq2419x = dev_get_drvdata(dev);
	unsigned int val;

	ret = bq2419x_fault_clear_sts(bq2419x, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "fault clear status failed %d\n", ret);
		return ret;
	}

	if (val & BQ2419x_FAULT_WATCHDOG_FAULT)
		bq_chg_err(bq2419x, "Watchdog Timer Expired\n");

	if (val & BQ2419x_FAULT_CHRG_SAFTY) {
		bq_chg_err(bq2419x,
				"Safety timer Expiration\n");
		htc_battery_bq2419x_notify(
				HTC_BATTERY_BQ2419X_SAFETY_TIMER_TIMEOUT);
	}

	return 0;
};

static const struct dev_pm_ops bq2419x_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(NULL, bq2419x_resume)
};
#endif

static const struct i2c_device_id bq2419x_id[] = {
	{.name = "bq2419x",},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq2419x_id);

static struct i2c_driver bq2419x_i2c_driver = {
	.driver = {
		.name	= "bq2419x",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm = &bq2419x_pm_ops,
#endif
	},
	.probe		= bq2419x_probe,
	.remove		= bq2419x_remove,
	.id_table	= bq2419x_id,
};

static int __init bq2419x_module_init(void)
{
	return i2c_add_driver(&bq2419x_i2c_driver);
}
subsys_initcall_sync(bq2419x_module_init);

static void __exit bq2419x_cleanup(void)
{
	i2c_del_driver(&bq2419x_i2c_driver);
}
module_exit(bq2419x_cleanup);

MODULE_DESCRIPTION("BQ24190/BQ24192/BQ24192i/BQ24193 battery charger driver");
MODULE_LICENSE("GPL v2");
