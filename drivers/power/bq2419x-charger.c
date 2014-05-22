/*
 * bq2419x-charger.c -- BQ24190/BQ24192/BQ24192i/BQ24193 Charger driver
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 * Author: Syed Rafiuddin <srafiuddin@nvidia.com>
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
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power/bq2419x-charger.h>
#include <linux/regmap.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/alarmtimer.h>
#include <linux/power/battery-charger-gauge-comm.h>
#include <linux/workqueue.h>

#define MAX_STR_PRINT 50

#define bq_chg_err(bq, fmt, ...)			\
		dev_err(bq->dev, "Charging Fault: " fmt, ##__VA_ARGS__)

enum charging_states {
	STATE_INIT = 0,
	ENABLED_HALF_IBAT,
	ENABLED_FULL_IBAT,
	DISABLED,
};

#define CHG_DISABLE_REASON_THERMAL		BIT(0)
#define CHG_DISABLE_REASON_USER			BIT(1)
#define CHG_DISABLE_REASON_UNKNOWN_BATTERY	BIT(2)
#define CHG_DISABLE_REASON_CHG_FULL_STOP	BIT(3)

#define BQ2419X_INPUT_VINDPM_OFFSET	3880
#define BQ2419X_CHARGE_ICHG_OFFSET	512
#define BQ2419X_PRE_CHG_IPRECHG_OFFSET	128
#define BQ2419X_PRE_CHG_TERM_OFFSET	128
#define BQ2419X_CHARGE_VOLTAGE_OFFSET	3504

#define BQ2419X_SAFETY_TIMER_ENABLE_CURRENT_MIN	(500)

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
	int				gpio_otg_iusb;
	int				wdt_refresh_timeout;
	int				wdt_time_sec;
	int				auto_recharge_time_power_off;
	bool				emulate_input_disconnected;

	struct mutex			mutex;
	int				in_current_limit;

	struct regulator_dev		*chg_rdev;
	struct regulator_desc		chg_reg_desc;
	struct regulator_init_data	chg_reg_init_data;

	struct regulator_dev		*vbus_rdev;
	struct regulator_desc		vbus_reg_desc;
	struct regulator_init_data	vbus_reg_init_data;

	struct battery_charger_dev	*bc_dev;
	int				chg_status;

	struct delayed_work		wdt_restart_wq;

	int				chg_restart_time;
	int				battery_presense;
	bool				cable_connected;
	int				last_charging_current;
	bool				disable_suspend_during_charging;
	int				last_temp;
	u32				auto_recharge_time_supend;
	int				charging_state;
	struct bq2419x_reg_info		last_chg_voltage;
	struct bq2419x_reg_info		last_input_src;
	struct bq2419x_reg_info		input_src;
	struct bq2419x_reg_info		chg_current_control;
	struct bq2419x_reg_info		prechg_term_control;
	struct bq2419x_reg_info		ir_comp_therm;
	struct bq2419x_reg_info		chg_voltage_control;
	struct bq2419x_vbus_platform_data *vbus_pdata;
	struct bq2419x_charger_platform_data *charger_pdata;
	bool				otp_control_no_thermister;
	struct bq2419x_reg_info		otp_output_current;
	int				charge_suspend_polling_time;
	int				charge_polling_time;
	unsigned int			charging_disabled_reason;
	bool				enable_batt_status_monitor;
	bool				chg_full_done;
	bool				chg_full_stop;
	bool				safety_timeout_happen;
	bool				safety_timer_reset_disable;
	struct dentry			*dentry;
};

static int current_to_reg(const unsigned int *tbl,
			size_t size, unsigned int val)
{
	size_t i;

	for (i = 0; i < size; i++)
		if (val < tbl[i])
			break;
	return i > 0 ? i - 1 : -EINVAL;
}

static int __bq2419x_charger_enable_locked(struct bq2419x_chip *bq2419x,
				unsigned int disable_reason, bool enable)
{
	int ret;

	dev_info(bq2419x->dev, "Charging %s with reason 0x%x\n",
			enable ? "enable" : "disable",
			disable_reason);
	if (enable)
		bq2419x->charging_disabled_reason &= ~disable_reason;
	else
		bq2419x->charging_disabled_reason |= disable_reason;

	if (!bq2419x->charging_disabled_reason)
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
				BQ2419X_ENABLE_CHARGE_MASK,
				BQ2419X_ENABLE_CHARGE);
	else {
		dev_info(bq2419x->dev, "Charging disabled reason 0x%x\n",
					bq2419x->charging_disabled_reason);
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
				BQ2419X_ENABLE_CHARGE_MASK,
				BQ2419X_DISABLE_CHARGE);
	}
	if (ret < 0)
		dev_err(bq2419x->dev, "register update failed, err %d\n", ret);
	return ret;
}

static int bq2419x_charger_enable_locked(struct bq2419x_chip *bq2419x,
				unsigned int reason)
{
	int ret;

	if (bq2419x->battery_presense) {
		/* set default/overtemp Charge regulation voltage */
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_VOLT_CTRL_REG,
			bq2419x->last_chg_voltage.mask,
			bq2419x->last_chg_voltage.val);
		if (ret < 0) {
			dev_err(bq2419x->dev,
				"VOLT_CTRL_REG update failed %d\n", ret);
			return ret;
		}

		reason |= CHG_DISABLE_REASON_UNKNOWN_BATTERY;
		ret = __bq2419x_charger_enable_locked(bq2419x,
					reason, true);
	} else {
		ret = __bq2419x_charger_enable_locked(bq2419x,
				CHG_DISABLE_REASON_UNKNOWN_BATTERY, false);
		if (ret < 0) {
			dev_err(bq2419x->dev,
				"charger enable failed %d\n", ret);
			return ret;
		}

		reason &= ~CHG_DISABLE_REASON_UNKNOWN_BATTERY;
		ret = __bq2419x_charger_enable_locked(bq2419x,
					reason, true);
	}

	return ret;
}

static inline int bq2419x_charger_enable(struct bq2419x_chip *bq2419x)
{
	int ret;

	mutex_lock(&bq2419x->mutex);
	ret = bq2419x_charger_enable_locked(bq2419x,
			CHG_DISABLE_REASON_CHG_FULL_STOP);
	mutex_unlock(&bq2419x->mutex);

	return ret;
}

static inline int bq2419x_charger_enable_suspend(struct bq2419x_chip *bq2419x)
{
	int ret;

	mutex_lock(&bq2419x->mutex);
	ret = bq2419x_charger_enable_locked(bq2419x, 0);
	mutex_unlock(&bq2419x->mutex);

	return ret;
}

static int bq2419x_vbus_enable(struct regulator_dev *rdev)
{
	struct bq2419x_chip *bq2419x = rdev_get_drvdata(rdev);
	int ret;

	dev_info(bq2419x->dev, "VBUS enabled, charging disabled\n");

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
			BQ2419X_ENABLE_CHARGE_MASK, BQ2419X_ENABLE_VBUS);
	if (ret < 0)
		dev_err(bq2419x->dev, "PWR_ON_REG update failed %d", ret);
	return ret;
}

static int bq2419x_vbus_disable(struct regulator_dev *rdev)
{
	struct bq2419x_chip *bq2419x = rdev_get_drvdata(rdev);
	int ret;

	dev_info(bq2419x->dev, "VBUS disabled, charging enabled\n");
	ret = bq2419x_charger_enable(bq2419x);
	if (ret < 0)
		dev_err(bq2419x->dev, "Charger enable failed %d", ret);
	return ret;
}

static int bq2419x_vbus_is_enabled(struct regulator_dev *rdev)
{
	struct bq2419x_chip *bq2419x = rdev_get_drvdata(rdev);
	int ret;
	unsigned int data;

	ret = regmap_read(bq2419x->regmap, BQ2419X_PWR_ON_REG, &data);
	if (ret < 0) {
		dev_err(bq2419x->dev, "PWR_ON_REG read failed %d", ret);
		return ret;
	}
	return (data & BQ2419X_ENABLE_CHARGE_MASK) == BQ2419X_ENABLE_VBUS;
}

static struct regulator_ops bq2419x_vbus_ops = {
	.enable		= bq2419x_vbus_enable,
	.disable	= bq2419x_vbus_disable,
	.is_enabled	= bq2419x_vbus_is_enabled,
};

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

static int bq2419x_process_charger_plat_data(struct bq2419x_chip *bq2419x,
		struct bq2419x_charger_platform_data *chg_pdata)
{
	int voltage_input;
	int fast_charge_current;
	int pre_charge_current;
	int termination_current;
	int ir_compensation_resistor;
	int ir_compensation_voltage;
	int thermal_regulation_threshold;
	int charge_voltage_limit;
	int vindpm, ichg, iprechg, iterm, bat_comp, vclamp, treg, vreg;
	int otp_output_current;

	if (chg_pdata) {
		voltage_input = chg_pdata->input_voltage_limit_mV ?: 4200;
		fast_charge_current =
			chg_pdata->fast_charge_current_limit_mA ?: 4544;
		pre_charge_current =
			chg_pdata->pre_charge_current_limit_mA ?: 256;
		termination_current =
			chg_pdata->termination_current_limit_mA ?: 128;
		ir_compensation_resistor =
			chg_pdata->ir_compensation_resister_ohm ?: 70;
		ir_compensation_voltage =
			chg_pdata->ir_compensation_voltage_mV ?: 112;
		thermal_regulation_threshold =
			chg_pdata->thermal_regulation_threshold_degC ?: 100;
		charge_voltage_limit =
			chg_pdata->charge_voltage_limit_mV ?: 4208;
		otp_output_current =
			chg_pdata->thermal_prop.otp_output_current_ma ?: 1344;
	} else {
		voltage_input = 4200;
		fast_charge_current = 4544;
		pre_charge_current = 256;
		termination_current = 128;
		ir_compensation_resistor = 70;
		ir_compensation_voltage = 112;
		thermal_regulation_threshold = 100;
		charge_voltage_limit = 4208;
		otp_output_current = 1344;
	}

	vindpm = bq2419x_val_to_reg(voltage_input,
			BQ2419X_INPUT_VINDPM_OFFSET, 80, 4, 0);
	bq2419x->input_src.mask = BQ2419X_INPUT_VINDPM_MASK;
	bq2419x->input_src.val = vindpm << 3;

	ichg = bq2419x_val_to_reg(fast_charge_current,
			BQ2419X_CHARGE_ICHG_OFFSET, 64, 6, 0);
	bq2419x->chg_current_control.mask = BQ2419X_CHRG_CTRL_ICHG_MASK;
	bq2419x->chg_current_control.val = ichg << 2;

	iprechg = bq2419x_val_to_reg(pre_charge_current,
			BQ2419X_PRE_CHG_IPRECHG_OFFSET, 128, 4, 0);
	bq2419x->prechg_term_control.mask = BQ2419X_CHRG_TERM_PRECHG_MASK;
	bq2419x->prechg_term_control.val = iprechg << 4;
	iterm =  bq2419x_val_to_reg(termination_current,
			BQ2419X_PRE_CHG_TERM_OFFSET, 128, 4, 0);
	bq2419x->prechg_term_control.mask |= BQ2419X_CHRG_TERM_TERM_MASK;
	bq2419x->prechg_term_control.val |= iterm;

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

	vreg = bq2419x_val_to_reg(charge_voltage_limit,
			BQ2419X_CHARGE_VOLTAGE_OFFSET, 16, 6, 1);
	bq2419x->chg_voltage_control.mask = BQ2419X_CHG_VOLT_LIMIT_MASK;
	bq2419x->chg_voltage_control.val = vreg << 2;

	ichg = bq2419x_val_to_reg(otp_output_current,
			BQ2419X_CHARGE_ICHG_OFFSET, 64, 6, 0);
	bq2419x->otp_output_current.mask = BQ2419X_CHRG_CTRL_ICHG_MASK;
	bq2419x->otp_output_current.val = ichg << 2;

	return 0;
}

static int bq2419x_charger_init(struct bq2419x_chip *bq2419x)
{
	int ret;
	struct bq2419x_reg_info reg;

	if (bq2419x->charging_state != ENABLED_HALF_IBAT)
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_CHRG_CTRL_REG,
				bq2419x->chg_current_control.mask,
				bq2419x->chg_current_control.val);
	else
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_CHRG_CTRL_REG,
				bq2419x->otp_output_current.mask,
				bq2419x->otp_output_current.val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "CHRG_CTRL_REG write failed %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_CHRG_TERM_REG,
			bq2419x->prechg_term_control.mask,
			bq2419x->prechg_term_control.val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "CHRG_TERM_REG write failed %d\n", ret);
		return ret;
	}

	reg.mask = bq2419x->last_input_src.mask;
	reg.val = bq2419x->last_input_src.val;
	reg.mask |= BQ2419X_INPUT_IINLIM_MASK;
	reg.val |= 0x2;
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
			reg.mask, reg.val);
	if (ret < 0)
		dev_err(bq2419x->dev, "INPUT_SRC_REG write failed %d\n", ret);

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_THERM_REG,
		    bq2419x->ir_comp_therm.mask, bq2419x->ir_comp_therm.val);
	if (ret < 0)
		dev_err(bq2419x->dev, "THERM_REG write failed: %d\n", ret);

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_VOLT_CTRL_REG,
			bq2419x->last_chg_voltage.mask,
			bq2419x->last_chg_voltage.val);
	if (ret < 0)
		dev_err(bq2419x->dev, "VOLT_CTRL update failed: %d\n", ret);

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_TIME_CTRL_REG,
			BQ2419X_TIME_JEITA_ISET, 0);
	if (ret < 0)
		dev_err(bq2419x->dev, "TIME_CTRL update failed: %d\n", ret);

	if (bq2419x->enable_batt_status_monitor) {
		ret = regmap_update_bits(bq2419x->regmap,
				BQ2419X_TIME_CTRL_REG,
				BQ2419X_EN_TERM, 0);
		if (ret < 0) {
			dev_err(bq2419x->dev, "TIME_CTRL_REG update failed: %d\n",
				ret);
			return ret;
		}
	}

	return ret;
}

static int bq2419x_configure_charging_current(struct bq2419x_chip *bq2419x,
	int in_current_limit)
{
	int val = 0;
	int ret = 0;
	int floor = 0;

	/* Clear EN_HIZ */
	if (!bq2419x->emulate_input_disconnected) {
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
			BQ2419X_EN_HIZ, 0);
		if (ret < 0) {
			dev_err(bq2419x->dev,
				"INPUT_SRC_REG update failed %d\n", ret);
			return ret;
		}
	}

	/* Configure input current limit in steps */
	val = current_to_reg(iinlim, ARRAY_SIZE(iinlim), in_current_limit);
	floor = current_to_reg(iinlim, ARRAY_SIZE(iinlim), 500);
	if (val < 0 || floor < 0)
		return 0;

	for (; floor <= val; floor++) {
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
				BQ2419x_CONFIG_MASK, floor);
		if (ret < 0)
			dev_err(bq2419x->dev,
				"INPUT_SRC_REG update failed: %d\n", ret);
		udelay(BQ2419x_CHARGING_CURRENT_STEP_DELAY_US);
	}
	bq2419x->in_current_limit = in_current_limit;
	return ret;
}

int bq2419x_full_current_enable(struct bq2419x_chip *bq2419x)
{
	int ret;

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_CHRG_CTRL_REG,
			bq2419x->chg_current_control.mask,
			bq2419x->chg_current_control.val);
	if (ret < 0) {
		dev_err(bq2419x->dev,
				"Failed to write CHRG_CTRL_REG %d\n", ret);
		return ret;
	}

	bq2419x->charging_state = ENABLED_FULL_IBAT;

	return 0;
}

int bq2419x_half_current_enable(struct bq2419x_chip *bq2419x)
{
	int ret;

	if (bq2419x->chg_current_control.val
			> bq2419x->otp_output_current.val) {
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_CHRG_CTRL_REG,
				bq2419x->otp_output_current.mask,
				bq2419x->otp_output_current.val);
		if (ret < 0) {
			dev_err(bq2419x->dev,
				"Failed to write CHRG_CTRL_REG %d\n", ret);
			return ret;
		}
	}
	bq2419x->charging_state = ENABLED_HALF_IBAT;

	return 0;
}

static void bq2419x_monitor_work_control(struct bq2419x_chip *bq2419x,
								bool start)
{
	if (!bq2419x->battery_presense)
		return;

	if (start) {
		if (bq2419x->enable_batt_status_monitor)
			battery_charger_batt_status_start_monitoring(
				bq2419x->bc_dev,
				bq2419x->last_charging_current / 1000);
		else
			battery_charger_thermal_start_monitoring(
				bq2419x->bc_dev);
	} else {
		if (bq2419x->enable_batt_status_monitor)
			battery_charger_batt_status_stop_monitoring(
				bq2419x->bc_dev);
		else
			battery_charger_thermal_stop_monitoring(
				bq2419x->bc_dev);
	}
}

static int bq2419x_set_charging_current_locked(struct bq2419x_chip *bq2419x,
			int min_uA, int max_uA)
{
	int in_current_limit;
	int old_current_limit;
	int ret = 0;
	int val;
	bool check_charge_done;

	bq2419x->chg_status = BATTERY_DISCHARGING;
	bq2419x->charging_state = STATE_INIT;
	bq2419x->chg_full_stop = false;
	bq2419x->chg_full_done = false;
	bq2419x->last_chg_voltage.mask = bq2419x->chg_voltage_control.mask;
	bq2419x->last_chg_voltage.val = bq2419x->chg_voltage_control.val;

	ret = bq2419x_charger_enable_locked(bq2419x,
			CHG_DISABLE_REASON_CHG_FULL_STOP);
	if (ret < 0)
		goto error;

	ret = regmap_read(bq2419x->regmap, BQ2419X_SYS_STAT_REG, &val);
	if (ret < 0)
		dev_err(bq2419x->dev, "SYS_STAT_REG read failed: %d\n", ret);

	if (max_uA == 0 && val != 0)
		goto done;

	old_current_limit = bq2419x->in_current_limit;
	bq2419x->last_charging_current = max_uA;
	check_charge_done = !bq2419x->enable_batt_status_monitor &&
				(!bq2419x->otp_control_no_thermister ||
				bq2419x->last_chg_voltage.val ==
				bq2419x->chg_voltage_control.val);
	if ((val & BQ2419x_VBUS_STAT) == BQ2419x_VBUS_UNKNOWN) {
		battery_charging_restart_cancel(bq2419x->bc_dev);
		in_current_limit = 500;
		bq2419x->cable_connected = 0;
		bq2419x->chg_status = BATTERY_DISCHARGING;
		bq2419x_monitor_work_control(bq2419x, false);
	} else if (((val & BQ2419x_CHRG_STATE_MASK) ==
				BQ2419x_CHRG_STATE_CHARGE_DONE) &&
				check_charge_done) {
		dev_info(bq2419x->dev, "Charging completed\n");
		bq2419x->chg_status = BATTERY_CHARGING_DONE;
		bq2419x->cable_connected = 1;
		in_current_limit = max_uA/1000;
		battery_charging_restart(bq2419x->bc_dev,
					bq2419x->chg_restart_time);
		bq2419x_monitor_work_control(bq2419x, false);
	} else {
		in_current_limit = max_uA/1000;
		bq2419x->cable_connected = 1;
		bq2419x->chg_status = BATTERY_CHARGING;
		bq2419x_monitor_work_control(bq2419x, true);
	}
	ret = bq2419x_configure_charging_current(bq2419x, in_current_limit);
	if (ret < 0)
		goto error;

	if (!bq2419x->battery_presense)
		bq2419x->chg_status = BATTERY_UNKNOWN;

	if (bq2419x->safety_timer_reset_disable) {
		if (in_current_limit > BQ2419X_SAFETY_TIMER_ENABLE_CURRENT_MIN)
			val = BQ2419X_EN_SFT_TIMER_MASK;
		else
			val = 0;
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_TIME_CTRL_REG,
				BQ2419X_EN_SFT_TIMER_MASK, val);
		if (ret < 0)
			dev_err(bq2419x->dev,
				"TIME_CTRL_REG update failed: %d\n", ret);
	}

	battery_charging_status_update(bq2419x->bc_dev, bq2419x->chg_status);
	if (bq2419x->disable_suspend_during_charging &&
		bq2419x->battery_presense) {
		if (bq2419x->cable_connected && in_current_limit > 500
			&& (bq2419x->chg_status != BATTERY_CHARGING_DONE ||
				!check_charge_done))
			battery_charger_acquire_wake_lock(bq2419x->bc_dev);
		else if (!bq2419x->cable_connected && old_current_limit > 500)
			battery_charger_release_wake_lock(bq2419x->bc_dev);
	}

	return 0;
error:
	dev_err(bq2419x->dev, "Charger enable failed, err = %d\n", ret);
done:
	return ret;
}

static int bq2419x_set_charging_current(struct regulator_dev *rdev,
			int min_uA, int max_uA)
{
	struct bq2419x_chip *bq2419x = rdev_get_drvdata(rdev);
	int ret = 0;

	dev_info(bq2419x->dev, "Setting charging current %d\n", max_uA/1000);
	msleep(200);

	mutex_lock(&bq2419x->mutex);
	bq2419x->safety_timeout_happen = false;
	ret = bq2419x_set_charging_current_locked(bq2419x, min_uA, max_uA);
	mutex_unlock(&bq2419x->mutex);

	return ret;
}

static struct regulator_ops bq2419x_tegra_regulator_ops = {
	.set_current_limit = bq2419x_set_charging_current,
};

static int bq2419x_set_charging_current_suspend(struct bq2419x_chip *bq2419x,
			int in_current_limit)
{
	int ret = 0;
	int val;

	dev_info(bq2419x->dev, "Setting charging current %d mA\n",
			in_current_limit);

	ret = bq2419x_charger_enable_suspend(bq2419x);
	if (ret < 0) {
		dev_err(bq2419x->dev, "Charger enable failed %d", ret);
		return ret;
	}

	ret = regmap_read(bq2419x->regmap, BQ2419X_SYS_STAT_REG, &val);
	if (ret < 0)
		dev_err(bq2419x->dev, "SYS_STAT_REG read failed: %d\n", ret);

	if (!bq2419x->cable_connected) {
		battery_charging_restart_cancel(bq2419x->bc_dev);
		ret = bq2419x_configure_charging_current(bq2419x,
				in_current_limit);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int bq2419x_reset_wdt(struct bq2419x_chip *bq2419x, const char *from)
{
	int ret = 0;
	unsigned int reg01;
	int timeout;

	mutex_lock(&bq2419x->mutex);
	if (!bq2419x->battery_presense || !bq2419x->wdt_refresh_timeout) {
		mutex_unlock(&bq2419x->mutex);
		return ret;
	}

	dev_dbg(bq2419x->dev, "%s() from %s()\n", __func__, from);

	/* Clear EN_HIZ */
	if (bq2419x->emulate_input_disconnected)
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
				BQ2419X_EN_HIZ, BQ2419X_EN_HIZ);
	else
		ret = regmap_update_bits(bq2419x->regmap,
				BQ2419X_INPUT_SRC_REG, BQ2419X_EN_HIZ, 0);
	if (ret < 0) {
		dev_err(bq2419x->dev, "INPUT_SRC_REG update failed:%d\n", ret);
		goto scrub;
	}

	if (!bq2419x->wdt_refresh_timeout)
		goto scrub;

	ret = regmap_read(bq2419x->regmap, BQ2419X_PWR_ON_REG, &reg01);
	if (ret < 0) {
		dev_err(bq2419x->dev, "PWR_ON_REG read failed: %d\n", ret);
		goto scrub;
	}

	reg01 |= BIT(6);

	/* Write two times to make sure reset WDT */
	ret = regmap_write(bq2419x->regmap, BQ2419X_PWR_ON_REG, reg01);
	if (ret < 0) {
		dev_err(bq2419x->dev, "PWR_ON_REG write failed: %d\n", ret);
		goto scrub;
	}
	ret = regmap_write(bq2419x->regmap, BQ2419X_PWR_ON_REG, reg01);
	if (ret < 0) {
		dev_err(bq2419x->dev, "PWR_ON_REG write failed: %d\n", ret);
		goto scrub;
	}

scrub:
	mutex_unlock(&bq2419x->mutex);

	timeout = bq2419x->wdt_refresh_timeout ? : 100;
	schedule_delayed_work(&bq2419x->wdt_restart_wq, timeout * HZ);
	return ret;
}

static int bq2419x_fault_clear_sts(struct bq2419x_chip *bq2419x,
	unsigned int *reg09_val)
{
	int ret;
	unsigned int reg09_1, reg09_2;

	ret = regmap_read(bq2419x->regmap, BQ2419X_FAULT_REG, &reg09_1);
	if (ret < 0) {
		dev_err(bq2419x->dev, "FAULT_REG read failed: %d\n", ret);
		return ret;
	}

	ret = regmap_read(bq2419x->regmap, BQ2419X_FAULT_REG, &reg09_2);
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

static int bq2419x_watchdog_init(struct bq2419x_chip *bq2419x,
			int timeout, const char *from)
{
	int ret, val;
	unsigned int reg05;

	if (!timeout) {
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_TIME_CTRL_REG,
				BQ2419X_WD_MASK, 0);
		if (ret < 0)
			dev_err(bq2419x->dev, "TIME_CTRL_REG read failed: %d\n",
				ret);
		bq2419x->wdt_refresh_timeout = 0;
		return ret;
	}

	if (timeout <= 60) {
		val = BQ2419X_WD_40ms;
		bq2419x->wdt_refresh_timeout = 25;
	} else if (timeout <= 120) {
		val = BQ2419X_WD_80ms;
		bq2419x->wdt_refresh_timeout = 50;
	} else {
		val = BQ2419X_WD_160ms;
		bq2419x->wdt_refresh_timeout = 125;
	}

	ret = regmap_read(bq2419x->regmap, BQ2419X_TIME_CTRL_REG, &reg05);
	if (ret < 0) {
		dev_err(bq2419x->dev, "TIME_CTRL_REG read failed:%d\n", ret);
		return ret;
	}

	/* Reset WDT to be safe if about to end */
	ret = bq2419x_reset_wdt(bq2419x, from);
	if (ret < 0)
		dev_err(bq2419x->dev, "bq2419x_reset_wdt failed: %d\n", ret);

	if ((reg05 & BQ2419X_WD_MASK) != val) {
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_TIME_CTRL_REG,
				BQ2419X_WD_MASK, val);
		if (ret < 0) {
			dev_err(bq2419x->dev, "TIME_CTRL_REG read failed: %d\n",
				ret);
			return ret;
		}
	}

	ret = bq2419x_reset_wdt(bq2419x, from);
	if (ret < 0)
		dev_err(bq2419x->dev, "bq2419x_reset_wdt failed: %d\n", ret);

	return ret;
}

static void bq2419x_wdt_restart_wq(struct work_struct *work)
{
	struct bq2419x_chip *bq2419x;
	int ret;

	bq2419x = container_of(work, struct bq2419x_chip, wdt_restart_wq.work);
	ret = bq2419x_reset_wdt(bq2419x, "THREAD");
	if (ret < 0)
		dev_err(bq2419x->dev, "bq2419x_reset_wdt failed: %d\n", ret);

}
static int bq2419x_reconfigure_charger_param(struct bq2419x_chip *bq2419x,
		const char *from)
{
	int ret;

	dev_info(bq2419x->dev, "Reconfiguring charging param from %s\n", from);
	ret = bq2419x_watchdog_init(bq2419x, bq2419x->wdt_time_sec, from);
	if (ret < 0) {
		dev_err(bq2419x->dev, "BQWDT init failed %d\n", ret);
		return ret;
	}

	ret = bq2419x_charger_init(bq2419x);
	if (ret < 0) {
		dev_err(bq2419x->dev, "Charger init failed: %d\n", ret);
		return ret;
	}

	ret = bq2419x_configure_charging_current(bq2419x,
			bq2419x->in_current_limit);
	if (ret < 0) {
		dev_err(bq2419x->dev, "Current config failed: %d\n", ret);
		return ret;
	}
	return ret;
}

static int bq2419x_handle_safety_timer_expire(struct bq2419x_chip *bq2419x)
{
	struct device *dev = bq2419x->dev;
	int ret;

	/* Reset saftty timer by setting 0 and then making 1 */
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_TIME_CTRL_REG,
			BQ2419X_EN_SFT_TIMER_MASK, 0);
	if (ret < 0) {
		dev_err(dev, "TIME_CTRL_REG update failed: %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_TIME_CTRL_REG,
			BQ2419X_EN_SFT_TIMER_MASK, BQ2419X_EN_SFT_TIMER_MASK);
	if (ret < 0) {
		dev_err(dev, "TIME_CTRL_REG update failed: %d\n", ret);
		return ret;
	}

	/* Need to toggel the Charging-enable bit from 1 to 0 to 1 */
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
			BQ2419X_ENABLE_CHARGE_MASK, 0);
	if (ret < 0) {
		dev_err(dev, "PWR_ON_REG update failed %d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
			BQ2419X_ENABLE_CHARGE_MASK, BQ2419X_ENABLE_CHARGE);
	if (ret < 0) {
		dev_err(dev, "PWR_ON_REG update failed %d\n", ret);
		return ret;
	}

	ret = bq2419x_reconfigure_charger_param(bq2419x, "SAFETY-TIMER_EXPIRE");
	if (ret < 0) {
		dev_err(dev, "Reconfig of BQ parm failed: %d\n", ret);
		return ret;
	}
	return ret;
}

static inline void bq2419x_charging_done_update_locked(
					struct bq2419x_chip *bq2419x)
{
	if (!bq2419x->otp_control_no_thermister
			|| bq2419x->last_chg_voltage.val ==
			bq2419x->chg_voltage_control.val) {
		dev_info(bq2419x->dev, "Charging completed\n");
		bq2419x->chg_status = BATTERY_CHARGING_DONE;
		battery_charging_status_update(bq2419x->bc_dev,
				bq2419x->chg_status);
	} else
		dev_info(bq2419x->dev, "OTP charging completed\n");
}

static irqreturn_t bq2419x_irq(int irq, void *data)
{
	struct bq2419x_chip *bq2419x = data;
	int ret;
	unsigned int val;
	int check_chg_state = 0;

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
	if (!bq2419x->battery_presense) {
		bq2419x_charger_enable_locked(bq2419x, 0);
		goto done;
	}
	mutex_unlock(&bq2419x->mutex);

	if (val & BQ2419x_FAULT_WATCHDOG_FAULT) {
		bq_chg_err(bq2419x, "WatchDog Expired\n");
		ret = bq2419x_reconfigure_charger_param(bq2419x, "WDT-EXP-ISR");
		if (ret < 0)
			dev_err(bq2419x->dev, "BQ reconfig failed %d\n", ret);
	}

	switch (val & BQ2419x_FAULT_CHRG_FAULT_MASK) {
	case BQ2419x_FAULT_CHRG_INPUT:
		bq_chg_err(bq2419x,
			"Input Fault (VBUS OVP or VBAT<VBUS<3.8V)\n");
		break;
	case BQ2419x_FAULT_CHRG_THERMAL:
		bq_chg_err(bq2419x, "Thermal shutdown\n");
		check_chg_state = 1;
		break;
	case BQ2419x_FAULT_CHRG_SAFTY:
		if (!bq2419x->safety_timer_reset_disable) {
			bq_chg_err(bq2419x, "Safety timer expiration\n");
			ret = bq2419x_handle_safety_timer_expire(bq2419x);
			if (ret < 0)
				dev_err(bq2419x->dev,
					"Handling of safty timer expire failed: %d\n",
					ret);
		} else {
			bq_chg_err(bq2419x,
				"Safety timer expiration, stop charging\n");
			if (bq2419x->disable_suspend_during_charging)
				battery_charger_release_wake_lock(
						bq2419x->bc_dev);
			bq2419x_monitor_work_control(bq2419x, false);
			bq2419x->safety_timeout_happen = true;
		}
		check_chg_state = 1;
		break;
	default:
		break;
	}

	if (val & BQ2419x_FAULT_NTC_FAULT) {
		bq_chg_err(bq2419x, "NTC fault %d\n",
				val & BQ2419x_FAULT_NTC_FAULT);
		check_chg_state = 1;
	}

	ret = regmap_read(bq2419x->regmap, BQ2419X_SYS_STAT_REG, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "SYS_STAT_REG read failed %d\n", ret);
		val = 0;
	}

	mutex_lock(&bq2419x->mutex);
	if ((val & BQ2419x_CHRG_STATE_MASK) == BQ2419x_CHRG_STATE_CHARGE_DONE &&
		bq2419x->battery_presense) {
		bq2419x_charging_done_update_locked(bq2419x);
		if ((!bq2419x->otp_control_no_thermister
				|| bq2419x->last_chg_voltage.val ==
				bq2419x->chg_voltage_control.val)
				&& !bq2419x->enable_batt_status_monitor) {
			battery_charging_restart(bq2419x->bc_dev,
					bq2419x->chg_restart_time);
			if (bq2419x->disable_suspend_during_charging)
				battery_charger_release_wake_lock(
						bq2419x->bc_dev);
			bq2419x_monitor_work_control(bq2419x, false);
		}
	}

	if ((val & BQ2419x_VSYS_STAT_MASK) == BQ2419x_VSYS_STAT_BATT_LOW)
		dev_info(bq2419x->dev,
			"In VSYSMIN regulation, battery is too low\n");

	/* Update Charging status based on STAT register */
	if (check_chg_state &&
	  ((val & BQ2419x_CHRG_STATE_MASK) == BQ2419x_CHRG_STATE_NOTCHARGING) &&
		bq2419x->battery_presense) {
		bq2419x->chg_status = BATTERY_DISCHARGING;
		battery_charging_status_update(bq2419x->bc_dev,
				bq2419x->chg_status);
		battery_charging_restart(bq2419x->bc_dev,
					bq2419x->chg_restart_time);
		if (bq2419x->disable_suspend_during_charging)
			battery_charger_release_wake_lock(bq2419x->bc_dev);
	}

	if (!bq2419x->battery_presense)
		bq2419x_charger_enable_locked(bq2419x, 0);

done:
	mutex_unlock(&bq2419x->mutex);

	return IRQ_HANDLED;
}

static int bq2419x_init_charger_regulator(struct bq2419x_chip *bq2419x,
		struct bq2419x_platform_data *pdata)
{
	int ret = 0;
	struct regulator_config rconfig = { };

	if (!pdata->bcharger_pdata) {
		dev_err(bq2419x->dev, "No charger platform data\n");
		return 0;
	}

	bq2419x->chg_reg_desc.name  = "bq2419x-charger";
	bq2419x->chg_reg_desc.ops   = &bq2419x_tegra_regulator_ops;
	bq2419x->chg_reg_desc.type  = REGULATOR_CURRENT;
	bq2419x->chg_reg_desc.owner = THIS_MODULE;

	bq2419x->chg_reg_init_data.supply_regulator	= NULL;
	bq2419x->chg_reg_init_data.regulator_init	= NULL;
	bq2419x->chg_reg_init_data.num_consumer_supplies =
				pdata->bcharger_pdata->num_consumer_supplies;
	bq2419x->chg_reg_init_data.consumer_supplies	=
				pdata->bcharger_pdata->consumer_supplies;
	bq2419x->chg_reg_init_data.driver_data		= bq2419x;
	bq2419x->chg_reg_init_data.constraints.name	= "bq2419x-charger";
	bq2419x->chg_reg_init_data.constraints.min_uA	= 0;
	bq2419x->chg_reg_init_data.constraints.max_uA	=
			pdata->bcharger_pdata->max_charge_current_mA * 1000;

	bq2419x->chg_reg_init_data.constraints.ignore_current_constraint_init =
							true;
	bq2419x->chg_reg_init_data.constraints.valid_modes_mask =
						REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_STANDBY;

	bq2419x->chg_reg_init_data.constraints.valid_ops_mask =
						REGULATOR_CHANGE_MODE |
						REGULATOR_CHANGE_STATUS |
						REGULATOR_CHANGE_CURRENT;

	rconfig.dev = bq2419x->dev;
	rconfig.of_node = NULL;
	rconfig.init_data = &bq2419x->chg_reg_init_data;
	rconfig.driver_data = bq2419x;
	bq2419x->chg_rdev = devm_regulator_register(bq2419x->dev,
				&bq2419x->chg_reg_desc, &rconfig);
	if (IS_ERR(bq2419x->chg_rdev)) {
		ret = PTR_ERR(bq2419x->chg_rdev);
		dev_err(bq2419x->dev,
			"vbus-charger regulator register failed %d\n", ret);
	}
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
	bq2419x->vbus_reg_desc.name = "bq2419x-vbus";
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

	bq2419x->vbus_reg_init_data.constraints.name	= "bq2419x-vbus";
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

	/* Disable the VBUS regulator and enable charging */
	ret = bq2419x_charger_enable(bq2419x);
	if (ret < 0) {
		dev_err(bq2419x->dev, "Charging enable failed %d", ret);
		goto scrub;
	}
	return ret;

scrub:
	if (gpio_is_valid(bq2419x->gpio_otg_iusb))
		gpio_free(bq2419x->gpio_otg_iusb);
	return ret;
}

static int bq2419x_show_chip_version(struct bq2419x_chip *bq2419x)
{
	int ret;
	unsigned int val;

	ret = regmap_read(bq2419x->regmap, BQ2419X_REVISION_REG, &val);
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

	ret = regmap_read(bq2419x->regmap, BQ2419X_INPUT_SRC_REG, &reg_val);
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
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	int ret;
	int in_current_limit;
	char *p = (char *)buf;

	in_current_limit = memparse(p, &p);
	ret = bq2419x_configure_charging_current(bq2419x, in_current_limit);
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

	ret = regmap_read(bq2419x->regmap, BQ2419X_PWR_ON_REG, &reg_val);
	if (ret < 0) {
		dev_err(dev, "BQ2419X_PWR_ON register read failed: %d\n", ret);
		return ret;
	}

	if ((reg_val & BQ2419X_ENABLE_CHARGE_MASK) == BQ2419X_ENABLE_CHARGE)
		return snprintf(buf, MAX_STR_PRINT, "enabled\n");
	else
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

	mutex_lock(&bq2419x->mutex);
	if (enabled)
		ret = __bq2419x_charger_enable_locked(bq2419x,
				CHG_DISABLE_REASON_USER, true);
	else
		ret = __bq2419x_charger_enable_locked(bq2419x,
				CHG_DISABLE_REASON_USER, false);
	mutex_unlock(&bq2419x->mutex);
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

	ret = regmap_read(bq2419x->regmap, BQ2419X_INPUT_SRC_REG, &reg_val);
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

	if (connect) {
		bq2419x->emulate_input_disconnected = false;
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
				BQ2419X_EN_HIZ, 0);
	} else {
		bq2419x->emulate_input_disconnected = true;
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
				BQ2419X_EN_HIZ, BQ2419X_EN_HIZ);
	}
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

	ret = regmap_read(bq2419x->regmap, BQ2419X_CHRG_CTRL_REG, &data);
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
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_CHRG_CTRL_REG,
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

static DEVICE_ATTR(input_charging_current_mA, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		bq2419x_show_input_charging_current,
		bq2419x_set_input_charging_current);

static DEVICE_ATTR(charging_state, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		bq2419x_show_charging_state, bq2419x_set_charging_state);

static DEVICE_ATTR(input_cable_state, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		bq2419x_show_input_cable_state, bq2419x_set_input_cable_state);

static struct attribute *bq2419x_attributes[] = {
	&dev_attr_output_charging_current.attr,
	&dev_attr_output_current_allowed_values.attr,
	&dev_attr_input_charging_current_mA.attr,
	&dev_attr_charging_state.attr,
	&dev_attr_input_cable_state.attr,
	NULL
};

static const struct attribute_group bq2419x_attr_group = {
	.attrs = bq2419x_attributes,
};

static int bq2419x_charger_get_status(struct battery_charger_dev *bc_dev)
{
	struct bq2419x_chip *bq2419x = battery_charger_get_drvdata(bc_dev);

	return bq2419x->chg_status;
}

static int bq2419x_charger_thermal_configure(
		struct battery_charger_dev *bc_dev,
		int temp, bool enable_charger, bool enable_charg_half_current,
		int battery_voltage)
{
	struct bq2419x_chip *bq2419x = battery_charger_get_drvdata(bc_dev);
	struct bq2419x_charger_platform_data *chg_pdata;
	int fast_charge_current = 0;
	u32 charge_voltage_limit = 0;
	int ichg;
	int ret;
	int i;
	int curr_ichg, vreg;

	chg_pdata = bq2419x->charger_pdata;
	if (!bq2419x->cable_connected || !chg_pdata->n_temp_profile ||
			!bq2419x->battery_presense)
		return 0;

	if (bq2419x->last_temp == temp)
		return 0;

	bq2419x->last_temp = temp;

	dev_info(bq2419x->dev, "Battery temp %d\n", temp);

	for (i = 0; i < chg_pdata->n_temp_profile; ++i) {
		if (temp <= chg_pdata->temp_range[i]) {
			fast_charge_current = chg_pdata->chg_current_limit[i];
			if (chg_pdata->chg_thermal_voltage_limit)
				charge_voltage_limit =
					chg_pdata->chg_thermal_voltage_limit[i];
			break;
		}
	}
	if (!fast_charge_current || !temp) {
		dev_info(bq2419x->dev, "Disable charging done by HW\n");
		return 0;
	}

	/* Fast charger become 50% when temp is at < 10 degC */
	if (temp <= 10)
		fast_charge_current *= 2;

	curr_ichg = bq2419x->chg_current_control.val >> 2;
	ichg = bq2419x_val_to_reg(fast_charge_current,
			BQ2419X_CHARGE_ICHG_OFFSET, 64, 6, 0);
	if (curr_ichg == ichg)
		return 0;

	bq2419x->chg_current_control.val = ichg << 2;
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_CHRG_CTRL_REG,
			BQ2419X_CHRG_CTRL_ICHG_MASK, ichg << 2);
	if (ret < 0) {
		dev_err(bq2419x->dev, "CHRG_CTRL_REG update failed %d\n", ret);
		return ret;
	}

	if (!charge_voltage_limit)
		return 0;

	/* Charge voltage limit */
	vreg = bq2419x_val_to_reg(charge_voltage_limit,
			BQ2419X_CHARGE_VOLTAGE_OFFSET, 16, 6, 1);
	bq2419x->chg_voltage_control.mask = BQ2419X_CHG_VOLT_LIMIT_MASK;
	bq2419x->chg_voltage_control.val = vreg << 2;
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_VOLT_CTRL_REG,
				bq2419x->chg_voltage_control.mask,
				bq2419x->chg_voltage_control.val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "VOLT_CTRL_REG update failed %d\n", ret);
		return ret;
	}

	return 0;
}

static int bq2419x_charger_thermal_configure_no_thermister(
		struct battery_charger_dev *bc_dev,
		int temp, bool enable_charger, bool enable_charg_half_current,
		int battery_voltage)
{
	struct bq2419x_chip *bq2419x = battery_charger_get_drvdata(bc_dev);
	int ret = 0;
	int vreg_val;

	mutex_lock(&bq2419x->mutex);
	if (!bq2419x->cable_connected)
		goto done;

	if (bq2419x->last_temp == temp && bq2419x->charging_state != STATE_INIT)
		goto done;

	bq2419x->last_temp = temp;

	dev_info(bq2419x->dev, "Battery temp %d\n", temp);
	if (enable_charger) {

		vreg_val = bq2419x_val_to_reg(battery_voltage,
				BQ2419X_CHARGE_VOLTAGE_OFFSET, 16, 6, 1) << 2;
		if (vreg_val > 0
			&& vreg_val <= bq2419x->chg_voltage_control.val
			&& vreg_val != bq2419x->last_chg_voltage.val) {
			/*Set charge voltage */
			ret = regmap_update_bits(bq2419x->regmap,
						BQ2419X_VOLT_CTRL_REG,
						BQ2419x_VOLTAGE_CTRL_MASK,
						vreg_val);
			if (ret < 0)
				goto error;
			bq2419x->last_chg_voltage.val =	vreg_val;
		}

		if (bq2419x->charging_state == DISABLED) {
			ret = __bq2419x_charger_enable_locked(bq2419x,
					CHG_DISABLE_REASON_THERMAL, true);
			if (ret < 0)
				goto error;
		}

		if (!enable_charg_half_current &&
			bq2419x->charging_state != ENABLED_FULL_IBAT) {
			bq2419x_full_current_enable(bq2419x);
			battery_charging_status_update(bq2419x->bc_dev,
							BATTERY_CHARGING);
		} else if (enable_charg_half_current &&
			bq2419x->charging_state != ENABLED_HALF_IBAT) {
			bq2419x_half_current_enable(bq2419x);
			battery_charging_status_update(bq2419x->bc_dev,
							BATTERY_CHARGING);
		}
	} else {
		if (bq2419x->charging_state != DISABLED) {
			ret = __bq2419x_charger_enable_locked(bq2419x,
					CHG_DISABLE_REASON_THERMAL, false);

			if (ret < 0)
				goto error;
			bq2419x->charging_state = DISABLED;
			battery_charging_status_update(bq2419x->bc_dev,
						BATTERY_DISCHARGING);
		}
	}

error:
done:
	mutex_unlock(&bq2419x->mutex);
	return ret;
}

static int bq2419x_charging_restart(struct battery_charger_dev *bc_dev)
{
	struct bq2419x_chip *bq2419x = battery_charger_get_drvdata(bc_dev);
	int ret = 0;

	mutex_lock(&bq2419x->mutex);
	if (!bq2419x->cable_connected || !bq2419x->battery_presense
		|| (bq2419x->safety_timer_reset_disable
			&& bq2419x->safety_timeout_happen))
		goto done;

	dev_info(bq2419x->dev, "Restarting the charging\n");
	ret = bq2419x_set_charging_current_locked(bq2419x,
			bq2419x->last_charging_current,
			bq2419x->last_charging_current);
	if (ret < 0) {
		dev_err(bq2419x->dev,
			"Restarting of charging failed: %d\n", ret);
		battery_charging_restart(bq2419x->bc_dev,
				bq2419x->chg_restart_time);
	}

done:
	mutex_unlock(&bq2419x->mutex);
	return ret;
}

static int bq2419x_charger_charging_full_configure(
		struct battery_charger_dev *bc_dev,
		bool charge_full_done, bool charge_full_stop)
{
	struct bq2419x_chip *bq2419x = battery_charger_get_drvdata(bc_dev);

	mutex_lock(&bq2419x->mutex);

	if (!bq2419x->enable_batt_status_monitor ||
		!bq2419x->battery_presense)
		goto done;

	if (!bq2419x->cable_connected)
		goto done;

	if (charge_full_done != bq2419x->chg_full_done) {
		bq2419x->chg_full_done = charge_full_done;
		if (charge_full_done)
			bq2419x_charging_done_update_locked(bq2419x);
		else {
			bq2419x->chg_status = BATTERY_CHARGING;
			battery_charging_status_update(bq2419x->bc_dev,
					bq2419x->chg_status);
		}
	}

	if (charge_full_stop != bq2419x->chg_full_stop) {
		bq2419x->chg_full_stop = charge_full_stop;
		if (charge_full_stop) {
			__bq2419x_charger_enable_locked(bq2419x,
					CHG_DISABLE_REASON_CHG_FULL_STOP,
					false);
			if (bq2419x->disable_suspend_during_charging)
				battery_charger_release_wake_lock(
						bq2419x->bc_dev);
		} else {
			__bq2419x_charger_enable_locked(bq2419x,
					CHG_DISABLE_REASON_CHG_FULL_STOP,
					true);
			if (bq2419x->disable_suspend_during_charging)
				battery_charger_acquire_wake_lock(
						bq2419x->bc_dev);
		}
	}
done:
	mutex_unlock(&bq2419x->mutex);
	return 0;
}

static int bq2419x_input_control(struct battery_charger_dev *bc_dev,
							int voltage_min)
{
	struct bq2419x_chip *bq2419x = battery_charger_get_drvdata(bc_dev);
	int vindpm, mask;
	unsigned int val, data;
	int ret;

	mutex_lock(&bq2419x->mutex);

	if (!bq2419x->battery_presense)
		goto done;

	if (!bq2419x->cable_connected)
		goto done;

	/* input source checking */
	vindpm = bq2419x_val_to_reg(voltage_min,
			BQ2419X_INPUT_VINDPM_OFFSET, 80, 4, 0);
	vindpm <<= 3;
	mask = BQ2419X_INPUT_VINDPM_MASK;
	if (vindpm != bq2419x->last_input_src.val) {
		bq2419x->last_input_src.val = vindpm;
		ret = regmap_update_bits(bq2419x->regmap,
					BQ2419X_INPUT_SRC_REG,
					mask, vindpm);
		if (ret < 0)
			dev_err(bq2419x->dev,
					"INPUT_SRC_REG update failed: %d\n",
					ret);
	}

	/* Check input current limit if reset */
	val = current_to_reg(iinlim, ARRAY_SIZE(iinlim),
			bq2419x->in_current_limit);
	if (val > 0) {
		ret = regmap_read(bq2419x->regmap,
				BQ2419X_INPUT_SRC_REG, &data);
		if (ret < 0)
			dev_err(bq2419x->dev,
					"INPUT_SRC_REG read failed %d\n", ret);
		else if ((data & BQ2419x_CONFIG_MASK) != val) {
			ret = regmap_update_bits(bq2419x->regmap,
					BQ2419X_INPUT_SRC_REG,
					BQ2419x_CONFIG_MASK,
					val);
			if (ret < 0)
				dev_err(bq2419x->dev,
					"INPUT_SRC_REG update failed: %d\n",
					ret);
		}
	}

done:
	mutex_unlock(&bq2419x->mutex);

	return 0;
}

static int bq2419x_unknown_battery_handle(struct battery_charger_dev *bc_dev)
{
	int ret = 0;
	struct bq2419x_chip *bq2419x = battery_charger_get_drvdata(bc_dev);

	mutex_lock(&bq2419x->mutex);
	bq2419x->battery_presense = false;
	bq2419x->chg_status = BATTERY_UNKNOWN;
	bq2419x->charging_state = STATE_INIT;

	ret = __bq2419x_charger_enable_locked(bq2419x,
			CHG_DISABLE_REASON_UNKNOWN_BATTERY, false);
	if (ret < 0) {
		dev_err(bq2419x->dev,
				"charger enable failed %d\n", ret);
		goto error;
	}

	cancel_delayed_work(&bq2419x->wdt_restart_wq);
	battery_charging_restart_cancel(bq2419x->bc_dev);
	battery_charger_batt_status_stop_monitoring(
			bq2419x->bc_dev);

	battery_charging_status_update(bq2419x->bc_dev, bq2419x->chg_status);

	if (bq2419x->disable_suspend_during_charging
			&& bq2419x->last_charging_current > 500)
			battery_charger_release_wake_lock(bq2419x->bc_dev);

	bq2419x->last_charging_current = 0;
error:
	mutex_unlock(&bq2419x->mutex);
	return ret;
}

static struct battery_charging_ops bq2419x_charger_bci_ops = {
	.get_charging_status = bq2419x_charger_get_status,
	.restart_charging = bq2419x_charging_restart,
	.thermal_configure = bq2419x_charger_thermal_configure,
	.charging_full_configure = bq2419x_charger_charging_full_configure,
	.input_voltage_configure = bq2419x_input_control,
	.unknown_battery_handle = bq2419x_unknown_battery_handle,
};

static struct battery_charger_info bq2419x_charger_bci = {
	.cell_id = 0,
	.bc_ops = &bq2419x_charger_bci_ops,
};

static struct bq2419x_platform_data *bq2419x_dt_parse(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct bq2419x_platform_data *pdata;
	struct device_node *batt_reg_node;
	struct device_node *vbus_reg_node;
	struct device_node *charge_policy_node;
	int ret;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	batt_reg_node = of_find_node_by_name(np, "charger");
	if (batt_reg_node) {
		int temp_range_len, chg_current_lim_len, chg_voltage_lim_len;
		int count;
		int wdt_timeout;
		int chg_restart_time;
		int suspend_polling_time;
		int auto_recharge_time_power_off;
		int temp_polling_time;
		int thermal_temp;
		unsigned int thermal_volt;
		unsigned int otp_output_current;
		struct regulator_init_data *batt_init_data;
		struct bq2419x_charger_platform_data *chg_pdata;
		const char *status_str;
		struct bq2419x_charger_platform_data *bcharger_pdata;
		u32 pval;

		status_str = of_get_property(batt_reg_node, "status", NULL);
		if (status_str && !(!strcmp(status_str, "okay"))) {
			dev_info(&client->dev,
				"charger node status is disabled\n");
			goto  vbus_node;
		}

		pdata->bcharger_pdata = devm_kzalloc(&client->dev,
				sizeof(*(pdata->bcharger_pdata)), GFP_KERNEL);
		if (!pdata->bcharger_pdata)
			return ERR_PTR(-ENOMEM);
		bcharger_pdata = pdata->bcharger_pdata;

		chg_pdata = pdata->bcharger_pdata;
		batt_init_data = of_get_regulator_init_data(&client->dev,
								batt_reg_node);
		if (!batt_init_data)
			return ERR_PTR(-EINVAL);

		ret = of_property_read_u32(batt_reg_node,
				"ti,input-voltage-limit-millivolt", &pval);
		if (!ret)
			bcharger_pdata->input_voltage_limit_mV = pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,fast-charge-current-limit-milliamp", &pval);
		if (!ret)
			bcharger_pdata->fast_charge_current_limit_mA =
							pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,pre-charge-current-limit-milliamp", &pval);
		if (!ret)
			bcharger_pdata->pre_charge_current_limit_mA = pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,charge-term-current-limit-milliamp", &pval);
		if (!ret)
			bcharger_pdata->termination_current_limit_mA = pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,ir-comp-resister-ohm", &pval);
		if (!ret)
			bcharger_pdata->ir_compensation_resister_ohm = pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,ir-comp-voltage-millivolt", &pval);
		if (!ret)
			bcharger_pdata->ir_compensation_voltage_mV = pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,thermal-regulation-threshold-degc", &pval);
		if (!ret)
			bcharger_pdata->thermal_regulation_threshold_degC =
						pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,charge-voltage-limit-millivolt", &pval);
		if (!ret)
			pdata->bcharger_pdata->charge_voltage_limit_mV = pval;

		pdata->bcharger_pdata->disable_suspend_during_charging =
				of_property_read_bool(batt_reg_node,
				"ti,disbale-suspend-during-charging");

		pdata->bcharger_pdata->safety_timer_reset_disable =
				of_property_read_bool(batt_reg_node,
				"ti,safety-timer-reset-disable");

		ret = of_property_read_u32(batt_reg_node,
				"ti,watchdog-timeout", &wdt_timeout);
		if (!ret)
			pdata->bcharger_pdata->wdt_timeout = wdt_timeout;

		ret = of_property_read_u32(batt_reg_node,
			"ti,auto-recharge-time-power-off",
			&auto_recharge_time_power_off);
		if (!ret)
			pdata->bcharger_pdata->auto_recharge_time_power_off =
					auto_recharge_time_power_off;
		else
			pdata->bcharger_pdata->auto_recharge_time_power_off =
					3600;

		ret = of_property_read_u32(batt_reg_node,
				"ti,auto-recharge-time", &chg_restart_time);
		if (!ret)
			pdata->bcharger_pdata->chg_restart_time =
							chg_restart_time;

		ret = of_property_read_u32(batt_reg_node,
			"ti,charge-suspend-polling-time-sec",
			&suspend_polling_time);
		if (!ret)
			bcharger_pdata->charge_suspend_polling_time_sec =
						suspend_polling_time;

		ret = of_property_read_u32(batt_reg_node,
				"ti,auto-recharge-time-suspend",
				&chg_restart_time);
		if (!ret)
			pdata->bcharger_pdata->auto_recharge_time_supend =
							chg_restart_time;
		else
			pdata->bcharger_pdata->auto_recharge_time_supend =
					3600;

		ret = of_property_read_u32(batt_reg_node,
			"ti,temp-polling-time-sec", &temp_polling_time);
		if (!ret)
			bcharger_pdata->temp_polling_time_sec =
						temp_polling_time;

		chg_pdata->tz_name = of_get_property(batt_reg_node,
						"ti,thermal-zone", NULL);

		count = of_property_count_u32(batt_reg_node, "ti,temp-range");
		temp_range_len = (count > 0) ? count : 0;

		count = of_property_count_u32(batt_reg_node,
					"ti,charge-current-limit");
		if (count <= 0)
			count = of_property_count_u32(batt_reg_node,
					"ti,charge-thermal-current-limit");
		chg_current_lim_len = (count > 0) ? count : 0;

		count = of_property_count_u32(batt_reg_node,
					"ti,charge-thermal-voltage-limit");
		chg_voltage_lim_len = (count > 0) ? count : 0;

		if (!temp_range_len)
			goto skip_therm_profile;

		if (temp_range_len != chg_current_lim_len) {
			dev_info(&client->dev,
				"current thermal profile is not correct\n");
			goto skip_therm_profile;
		}

		if (chg_voltage_lim_len && (temp_range_len != chg_voltage_lim_len)) {
			dev_info(&client->dev,
				"voltage thermal profile is not correct\n");
			goto skip_therm_profile;
		}

		chg_pdata->temp_range = devm_kzalloc(&client->dev,
				sizeof(u32) * temp_range_len, GFP_KERNEL);
		if (!chg_pdata->temp_range)
			return ERR_PTR(-ENOMEM);

		ret = of_property_read_u32_array(batt_reg_node, "ti,temp-range",
				chg_pdata->temp_range, temp_range_len);
		if (ret < 0)
			return ERR_PTR(ret);

		chg_pdata->chg_current_limit = devm_kzalloc(&client->dev,
				sizeof(u32) * temp_range_len, GFP_KERNEL);
		if (!chg_pdata->chg_current_limit)
			return ERR_PTR(-ENOMEM);

		ret = of_property_read_u32_array(batt_reg_node,
				"ti,charge-current-limit",
				chg_pdata->chg_current_limit,
				temp_range_len);
		if (ret < 0)
			ret = of_property_read_u32_array(batt_reg_node,
				"ti,charge-thermal-current-limit",
					chg_pdata->chg_current_limit,
					temp_range_len);
		if (ret < 0)
			return ERR_PTR(ret);

		if (!chg_voltage_lim_len)
			goto skip_thermal_volt_profle;

		chg_pdata->chg_thermal_voltage_limit =
					devm_kzalloc(&client->dev,
					sizeof(u32) * temp_range_len,
					GFP_KERNEL);
		if (!chg_pdata->chg_thermal_voltage_limit)
			return ERR_PTR(-ENOMEM);

		ret = of_property_read_u32_array(batt_reg_node,
				"ti,charge-thermal-voltage-limit",
				chg_pdata->chg_thermal_voltage_limit,
				temp_range_len);
		if (ret < 0)
			return ERR_PTR(ret);

skip_thermal_volt_profle:
		chg_pdata->n_temp_profile = temp_range_len;

skip_therm_profile:
		pdata->bcharger_pdata->consumer_supplies =
					batt_init_data->consumer_supplies;
		pdata->bcharger_pdata->num_consumer_supplies =
					batt_init_data->num_consumer_supplies;
		pdata->bcharger_pdata->max_charge_current_mA =
				batt_init_data->constraints.max_uA / 1000;

		bcharger_pdata->otp_control_no_thermister =
			of_property_read_bool(batt_reg_node,
			"thermal-overtemp-control-no-thermister");

		ret = of_property_read_u32(batt_reg_node,
			"thermal-temperature-hot-deciC", &thermal_temp);
		if (!ret)
			bcharger_pdata->thermal_prop.temp_hot_dc =
						thermal_temp;

		ret = of_property_read_u32(batt_reg_node,
			"thermal-temperature-cold-deciC", &thermal_temp);
		if (!ret)
			bcharger_pdata->thermal_prop.temp_cold_dc =
						thermal_temp;

		ret = of_property_read_u32(batt_reg_node,
			"thermal-temperature-warm-deciC", &thermal_temp);
		if (!ret)
			bcharger_pdata->thermal_prop.temp_warm_dc =
						thermal_temp;

		ret = of_property_read_u32(batt_reg_node,
			"thermal-temperature-cool-deciC", &thermal_temp);
		if (!ret)
			bcharger_pdata->thermal_prop.temp_cool_dc =
						thermal_temp;

		ret = of_property_read_u32(batt_reg_node,
			"thermal-temperature-hysteresis-deciC", &thermal_temp);
		if (!ret)
			bcharger_pdata->thermal_prop.temp_hysteresis_dc =
						thermal_temp;

		ret = of_property_read_u32(batt_reg_node,
			"thermal-warm-voltage-millivolt", &thermal_volt);
		if (!ret)
			bcharger_pdata->thermal_prop.warm_voltage_mv =
						thermal_volt;

		ret = of_property_read_u32(batt_reg_node,
			"thermal-cool-voltage-millivolt", &thermal_volt);
		if (!ret)
			bcharger_pdata->thermal_prop.cool_voltage_mv =
						thermal_volt;

		bcharger_pdata->thermal_prop.disable_warm_current_half =
			of_property_read_bool(batt_reg_node,
			"thermal-disable-warm-current-half");

		bcharger_pdata->thermal_prop.disable_cool_current_half =
			of_property_read_bool(batt_reg_node,
			"thermal-disable-cool-current-half");

		ret = of_property_read_u32(batt_reg_node,
			"thermal-overtemp-output-current-milliamp",
				&otp_output_current);
		if (!ret)
			bcharger_pdata->thermal_prop.otp_output_current_ma =
						otp_output_current;
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

	/* TODO: seperate product policy from chip driver */
	charge_policy_node = of_find_node_by_name(np, "policy");
	if (charge_policy_node) {
		struct bq2419x_charge_policy_platform_data *policy_pdata;
		unsigned int full_thr_val;
		unsigned int input_switch_val;
		int unknown_batt_id_min;

		pdata->cpolicy_pdata = devm_kzalloc(&client->dev,
			sizeof(*(pdata->cpolicy_pdata)), GFP_KERNEL);
		if (!pdata->cpolicy_pdata)
			return ERR_PTR(-ENOMEM);

		policy_pdata = pdata->cpolicy_pdata;

		policy_pdata->enable_battery_status_monitor =
				of_property_read_bool(charge_policy_node,
				"enable-battery-status-monitor");

		if (!policy_pdata->enable_battery_status_monitor)
			goto unknown_battery_id;

		ret = of_property_read_u32(charge_policy_node,
				"charge-full-done-voltage-min-millivolt",
				&full_thr_val);
		if (!ret)
			policy_pdata->full_thr.chg_done_voltage_min_mv =
								full_thr_val;

		ret = of_property_read_u32(charge_policy_node,
				"charge-full-done-current-min-milliamp",
				&full_thr_val);
		if (!ret)
			policy_pdata->full_thr.chg_done_current_min_ma =
								full_thr_val;

		ret = of_property_read_u32(charge_policy_node,
				"charge-full-done-low-current-min-milliamp",
				&full_thr_val);
		if (!ret)
			policy_pdata->full_thr.chg_done_low_current_min_ma =
								full_thr_val;

		ret = of_property_read_u32(charge_policy_node,
				"charge-full-recharge-voltage-min-millivolt",
				&full_thr_val);
		if (!ret)
			policy_pdata->full_thr.recharge_voltage_min_mv =
								full_thr_val;

		ret = of_property_read_u32(charge_policy_node,
				"input-voltage-min-high-battery-millivolt",
				&input_switch_val);
		if (!ret)
			policy_pdata->input_switch.input_vmin_high_mv =
				input_switch_val;

		ret = of_property_read_u32(charge_policy_node,
				"input-voltage-min-low-battery-millivolt",
				&input_switch_val);
		if (!ret)
			policy_pdata->input_switch.input_vmin_low_mv =
				input_switch_val;

		ret = of_property_read_u32(charge_policy_node,
				"input-voltage-switch-millivolt",
				&input_switch_val);
		if (!ret)
			policy_pdata->input_switch.input_switch_threshold_mv =
				input_switch_val;

unknown_battery_id:
		policy_pdata->batt_id_channel_name =
			of_get_property(charge_policy_node,
					"battery-id-channel-name", NULL);

		ret = of_property_read_u32(charge_policy_node,
					"unknown-battery-id-minimum",
					&unknown_batt_id_min);
		if (!ret)
			policy_pdata->unknown_batt_id_min =
							unknown_batt_id_min;
	}

	return pdata;
}

static inline void bq2419x_battery_charger_info_init(
		struct bq2419x_charger_platform_data *bcharger_pdata,
		struct bq2419x_charge_policy_platform_data *cpolicy_pdata)
{
	if (bcharger_pdata->otp_control_no_thermister)
		bq2419x_charger_bci.bc_ops->thermal_configure =
			bq2419x_charger_thermal_configure_no_thermister;

	bq2419x_charger_bci.polling_time_sec =
			bcharger_pdata->temp_polling_time_sec;
	bq2419x_charger_bci.tz_name = bcharger_pdata->tz_name;

	bq2419x_charger_bci.thermal_prop.temp_hot_dc =
			bcharger_pdata->thermal_prop.temp_hot_dc;
	bq2419x_charger_bci.thermal_prop.temp_cold_dc =
			bcharger_pdata->thermal_prop.temp_cold_dc;
	bq2419x_charger_bci.thermal_prop.temp_warm_dc =
			bcharger_pdata->thermal_prop.temp_warm_dc;
	bq2419x_charger_bci.thermal_prop.temp_cool_dc =
			bcharger_pdata->thermal_prop.temp_cool_dc;
	bq2419x_charger_bci.thermal_prop.temp_hysteresis_dc =
			bcharger_pdata->thermal_prop.temp_hysteresis_dc;
	bq2419x_charger_bci.thermal_prop.regulation_voltage_mv =
			bcharger_pdata->charge_voltage_limit_mV;
	bq2419x_charger_bci.thermal_prop.warm_voltage_mv =
			bcharger_pdata->thermal_prop.warm_voltage_mv;
	bq2419x_charger_bci.thermal_prop.cool_voltage_mv =
			bcharger_pdata->thermal_prop.cool_voltage_mv;
	bq2419x_charger_bci.thermal_prop.disable_warm_current_half =
			bcharger_pdata->thermal_prop.disable_warm_current_half;
	bq2419x_charger_bci.thermal_prop.disable_cool_current_half =
			bcharger_pdata->thermal_prop.disable_cool_current_half;

	bq2419x_charger_bci.full_thr.chg_done_voltage_min_mv =
			cpolicy_pdata->full_thr.chg_done_voltage_min_mv ?:
			(bcharger_pdata->charge_voltage_limit_mV ?
			bcharger_pdata->charge_voltage_limit_mV - 102 : 0);
	bq2419x_charger_bci.full_thr.chg_done_current_min_ma =
			cpolicy_pdata->full_thr.chg_done_current_min_ma;
	bq2419x_charger_bci.full_thr.chg_done_low_current_min_ma =
			cpolicy_pdata->full_thr.chg_done_low_current_min_ma;
	bq2419x_charger_bci.full_thr.recharge_voltage_min_mv =
			cpolicy_pdata->full_thr.recharge_voltage_min_mv ?:
			(bcharger_pdata->charge_voltage_limit_mV ?
			bcharger_pdata->charge_voltage_limit_mV - 48 : 0);

	bq2419x_charger_bci.input_switch.input_vmin_high_mv =
			cpolicy_pdata->input_switch.input_vmin_high_mv;
	bq2419x_charger_bci.input_switch.input_vmin_low_mv =
			cpolicy_pdata->input_switch.input_vmin_low_mv;
	bq2419x_charger_bci.input_switch.input_switch_threshold_mv =
			cpolicy_pdata->input_switch.input_switch_threshold_mv;

	bq2419x_charger_bci.batt_id_channel_name =
			cpolicy_pdata->batt_id_channel_name;
	bq2419x_charger_bci.unknown_batt_id_min =
			cpolicy_pdata->unknown_batt_id_min;

	if (cpolicy_pdata->enable_battery_status_monitor)
		bq2419x_charger_bci.enable_batt_status_monitor = true;
	else
		bq2419x_charger_bci.enable_thermal_monitor = true;
}

static int bq2419x_debugfs_show(struct seq_file *s, void *unused)
{
	struct bq2419x_chip *bq2419x = s->private;
	int ret;
	u8 reg;
	unsigned int data;

	for (reg = BQ2419X_INPUT_SRC_REG; reg <= BQ2419X_REVISION_REG; reg++) {
		ret = regmap_read(bq2419x->regmap, reg, &data);
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
	bq2419x->charger_pdata = pdata->bcharger_pdata;
	bq2419x->vbus_pdata = pdata->vbus_pdata;

	bq2419x->regmap = devm_regmap_init_i2c(client, &bq2419x_regmap_config);
	if (IS_ERR(bq2419x->regmap)) {
		ret = PTR_ERR(bq2419x->regmap);
		dev_err(&client->dev, "regmap init failed with err %d\n", ret);
		return ret;
	}

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

	if (!pdata->bcharger_pdata) {
		dev_info(&client->dev, "No battery charger supported\n");
		ret = bq2419x_watchdog_init(bq2419x, 0, "PROBE");
		if (ret < 0) {
			dev_err(bq2419x->dev, "WDT disable failed: %d\n", ret);
			goto scrub_mutex;
		}

		ret = bq2419x_fault_clear_sts(bq2419x, NULL);
		if (ret < 0) {
			dev_err(bq2419x->dev, "fault clear status failed %d\n", ret);
			goto scrub_mutex;
		}
		goto skip_bcharger_init;
	}

	bq2419x->auto_recharge_time_power_off =
			pdata->bcharger_pdata->auto_recharge_time_power_off;
	bq2419x->wdt_time_sec = pdata->bcharger_pdata->wdt_timeout;
	bq2419x->chg_restart_time = pdata->bcharger_pdata->chg_restart_time;
	bq2419x->battery_presense = true;
	bq2419x->last_temp = -1000;
	bq2419x->disable_suspend_during_charging =
			pdata->bcharger_pdata->disable_suspend_during_charging;
	bq2419x->safety_timer_reset_disable =
			pdata->bcharger_pdata->safety_timer_reset_disable;
	bq2419x->charge_suspend_polling_time =
			pdata->bcharger_pdata->charge_suspend_polling_time_sec;
	bq2419x->charge_polling_time =
			pdata->bcharger_pdata->temp_polling_time_sec;
	bq2419x->auto_recharge_time_supend =
			pdata->bcharger_pdata->auto_recharge_time_supend;

	bq2419x_process_charger_plat_data(bq2419x, pdata->bcharger_pdata);

	bq2419x->enable_batt_status_monitor =
			pdata->cpolicy_pdata->enable_battery_status_monitor;

	bq2419x->last_chg_voltage.mask = bq2419x->chg_voltage_control.mask;
	bq2419x->last_chg_voltage.val = bq2419x->chg_voltage_control.val;
	bq2419x->last_input_src.mask = bq2419x->input_src.mask;
	bq2419x->last_input_src.val = bq2419x->input_src.val;

	ret = bq2419x_charger_init(bq2419x);
	if (ret < 0) {
		dev_err(bq2419x->dev, "Charger init failed: %d\n", ret);
		goto scrub_mutex;
	}

	ret = bq2419x_init_charger_regulator(bq2419x, pdata);
	if (ret < 0) {
		dev_err(&client->dev,
			"Charger regualtor init failed %d\n", ret);
		goto scrub_mutex;
	}

	bq2419x->otp_control_no_thermister =
		pdata->bcharger_pdata->otp_control_no_thermister;

	bq2419x_battery_charger_info_init(pdata->bcharger_pdata,
					pdata->cpolicy_pdata);

	bq2419x->bc_dev = battery_charger_register(bq2419x->dev,
			&bq2419x_charger_bci, bq2419x);
	if (IS_ERR(bq2419x->bc_dev)) {
		ret = PTR_ERR(bq2419x->bc_dev);
		dev_err(bq2419x->dev, "battery charger register failed: %d\n",
			ret);
		goto scrub_mutex;
	}

	INIT_DELAYED_WORK(&bq2419x->wdt_restart_wq, bq2419x_wdt_restart_wq);
	ret = bq2419x_watchdog_init(bq2419x, bq2419x->wdt_time_sec, "PROBE");
	if (ret < 0) {
		dev_err(bq2419x->dev, "BQWDT init failed %d\n", ret);
		goto scrub_wq;
	}

	ret = bq2419x_fault_clear_sts(bq2419x, NULL);
	if (ret < 0) {
		dev_err(bq2419x->dev, "fault clear status failed %d\n", ret);
		goto scrub_wq;
	}

	ret = devm_request_threaded_irq(bq2419x->dev, bq2419x->irq, NULL,
		bq2419x_irq, IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
			dev_name(bq2419x->dev), bq2419x);
	if (ret < 0) {
		dev_warn(bq2419x->dev, "request IRQ %d fail, err = %d\n",
				bq2419x->irq, ret);
		dev_info(bq2419x->dev,
			"Supporting bq driver without interrupt\n");
		ret = 0;
	}

skip_bcharger_init:
	ret = bq2419x_init_vbus_regulator(bq2419x, pdata);
	if (ret < 0) {
		dev_err(&client->dev, "VBUS regulator init failed %d\n", ret);
		goto scrub_wq;
	}

	/* enable charging */
	ret = bq2419x_charger_enable(bq2419x);
	if (ret < 0)
		goto scrub_wq;

	bq2419x->dentry = debugfs_create_file("bq2419x-regs", S_IRUSR, NULL,
					      bq2419x, &bq2419x_debugfs_fops);
	return 0;
scrub_wq:
	if (pdata->bcharger_pdata) {
		cancel_delayed_work(&bq2419x->wdt_restart_wq);
		battery_charger_unregister(bq2419x->bc_dev);
	}
scrub_mutex:
	mutex_destroy(&bq2419x->mutex);
	return ret;
}

static int bq2419x_remove(struct i2c_client *client)
{
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);

	debugfs_remove(bq2419x->dentry);
	if (bq2419x->battery_presense) {
		battery_charger_unregister(bq2419x->bc_dev);
		cancel_delayed_work(&bq2419x->wdt_restart_wq);
	}
	mutex_destroy(&bq2419x->mutex);
	return 0;
}

static void bq2419x_shutdown(struct i2c_client *client)
{
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	struct device *dev = &client->dev;
	int ret;
	int next_poweron_time = 0;

	if (!bq2419x->battery_presense)
		return;

	if (!bq2419x->cable_connected)
		goto end;

	ret = bq2419x_reset_wdt(bq2419x, "SHUTDOWN");
	if (ret < 0)
		dev_err(bq2419x->dev, "Reset WDT failed: %d\n", ret);

	if (bq2419x->chg_status == BATTERY_CHARGING_DONE) {
		dev_info(bq2419x->dev, "Battery charging done\n");
		goto end;
	}

	if (bq2419x->in_current_limit <= 500) {
		dev_info(bq2419x->dev, "Battery charging with 500mA\n");
		next_poweron_time = bq2419x->auto_recharge_time_power_off;
	} else {
		dev_info(bq2419x->dev, "Battery charging with high current\n");
		next_poweron_time = bq2419x->wdt_refresh_timeout;
	}

	if (!next_poweron_time)
		goto end;

	ret = battery_charging_system_reset_after(bq2419x->bc_dev,
				next_poweron_time);
	if (ret < 0)
		dev_err(dev, "System poweron after %d config failed %d\n",
			next_poweron_time, ret);
end:
	if (next_poweron_time)
		dev_info(dev, "System-charger will power-ON after %d sec\n",
				next_poweron_time);
	else
		dev_info(bq2419x->dev, "System-charger will not power-ON\n");

	battery_charging_system_power_on_usb_event(bq2419x->bc_dev);
}

#ifdef CONFIG_PM_SLEEP
static int bq2419x_suspend(struct device *dev)
{
	struct bq2419x_chip *bq2419x = dev_get_drvdata(dev);
	int next_wakeup = 0;
	int ret;

	if (!bq2419x->battery_presense)
		return 0;

	battery_charging_restart_cancel(bq2419x->bc_dev);

	if (!bq2419x->cable_connected)
		goto end;

	ret = bq2419x_reset_wdt(bq2419x, "Suspend");
	if (ret < 0)
		dev_err(bq2419x->dev, "Reset WDT failed: %d\n", ret);

	if (bq2419x->enable_batt_status_monitor) {
		if (bq2419x->chg_full_stop)
			next_wakeup =
				bq2419x->charge_suspend_polling_time;
		else if (bq2419x->wdt_refresh_timeout)
			next_wakeup = bq2419x->wdt_refresh_timeout;
		else
			next_wakeup = bq2419x->charge_polling_time;
	} else if (bq2419x->otp_control_no_thermister) {
		if (bq2419x->chg_status == BATTERY_CHARGING_DONE)
			next_wakeup =
				bq2419x->auto_recharge_time_supend;
		else if (bq2419x->wdt_refresh_timeout)
			next_wakeup = bq2419x->wdt_refresh_timeout;
		else
			next_wakeup = bq2419x->charge_polling_time;
	} else {
		if (bq2419x->chg_status == BATTERY_CHARGING_DONE) {
			dev_info(bq2419x->dev, "Battery charging done\n");
			goto end;
		}

		if (bq2419x->in_current_limit <= 500) {
			dev_info(bq2419x->dev, "Battery charging with 500mA\n");
			next_wakeup = bq2419x->auto_recharge_time_supend;
		} else {
			dev_info(bq2419x->dev,
					"Battery charging with high current\n");
			next_wakeup = bq2419x->wdt_refresh_timeout;
		}

	}

	battery_charging_wakeup(bq2419x->bc_dev, next_wakeup);
end:
	if (next_wakeup)
		dev_info(dev, "System-charger will resume after %d sec\n",
				next_wakeup);
	else
		dev_info(dev, "System-charger will not have resume time\n");

	if (next_wakeup == bq2419x->wdt_refresh_timeout ||
			bq2419x->enable_batt_status_monitor ||
			bq2419x->otp_control_no_thermister)
		return 0;

	ret = bq2419x_set_charging_current_suspend(bq2419x, 500);
	if (ret < 0)
		dev_err(bq2419x->dev, "Config of charging failed: %d\n", ret);
	return ret;
}

static int bq2419x_resume(struct device *dev)
{
	int ret = 0;
	struct bq2419x_chip *bq2419x = dev_get_drvdata(dev);
	unsigned int val;

	if (!bq2419x->battery_presense)
		return 0;

	ret = bq2419x_fault_clear_sts(bq2419x, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "fault clear status failed %d\n", ret);
		return ret;
	}

	if (val & BQ2419x_FAULT_WATCHDOG_FAULT) {
		bq_chg_err(bq2419x, "Watchdog Timer Expired\n");

		ret = bq2419x_reconfigure_charger_param(bq2419x,
				"WDT-EXP-RESUME");
		if (ret < 0) {
			dev_err(bq2419x->dev, "BQ reconfig failed %d\n", ret);
			return ret;
		}
	} else {
		ret = bq2419x_reset_wdt(bq2419x, "Resume");
		if (ret < 0)
			dev_err(bq2419x->dev, "Reset WDT failed: %d\n", ret);
	}

	if (val & BQ2419x_FAULT_CHRG_SAFTY) {
		if (!bq2419x->safety_timer_reset_disable) {
			bq_chg_err(bq2419x, "Safety timer Expired\n");
			ret = bq2419x_handle_safety_timer_expire(bq2419x);
			if (ret < 0)
				dev_err(bq2419x->dev,
					"Handling of safty timer expire failed: %d\n",
					ret);
		} else {
			bq_chg_err(bq2419x,
				"Safety timer expiration, stop charging\n");
			mutex_lock(&bq2419x->mutex);
			bq2419x_monitor_work_control(bq2419x, false);
			bq2419x->safety_timeout_happen = true;
			mutex_unlock(&bq2419x->mutex);
		}
	}

	return 0;
};
#endif

static const struct dev_pm_ops bq2419x_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bq2419x_suspend, bq2419x_resume)
};

static const struct i2c_device_id bq2419x_id[] = {
	{.name = "bq2419x",},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq2419x_id);

static struct i2c_driver bq2419x_i2c_driver = {
	.driver = {
		.name	= "bq2419x",
		.owner	= THIS_MODULE,
		.pm = &bq2419x_pm_ops,
	},
	.probe		= bq2419x_probe,
	.remove		= bq2419x_remove,
	.shutdown	= bq2419x_shutdown,
	.id_table	= bq2419x_id,
};

static int __init bq2419x_module_init(void)
{
	return i2c_add_driver(&bq2419x_i2c_driver);
}
subsys_initcall(bq2419x_module_init);

static void __exit bq2419x_cleanup(void)
{
	i2c_del_driver(&bq2419x_i2c_driver);
}
module_exit(bq2419x_cleanup);

MODULE_DESCRIPTION("BQ24190/BQ24192/BQ24192i/BQ24193 battery charger driver");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_AUTHOR("Syed Rafiuddin <srafiuddin@nvidia.com");
MODULE_LICENSE("GPL v2");
