/*
 * htc_battery_bq2419x.c
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
#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/htc_battery_bq2419x.h>
#include <linux/power/battery-charger-gauge-comm.h>
#include <linux/pm.h>
#include <linux/jiffies.h>
#include <linux/wakelock.h>
#include <linux/of.h>
#include <linux/ktime.h>
#include <linux/cable_vbus_monitor.h>

enum charging_states {
	STATE_INIT = 0,
	ENABLED_HALF_IBAT,
	ENABLED_FULL_IBAT,
	DISABLED,
};

enum aicl_phase {
	AICL_DISABLED,
	AICL_DETECT_INIT,
	AICL_DETECT,
	AICL_MATCH,
	AICL_NOT_MATCH,
};

#define CHG_DISABLE_REASON_THERMAL		BIT(0)
#define CHG_DISABLE_REASON_USER			BIT(1)
#define CHG_DISABLE_REASON_UNKNOWN_BATTERY	BIT(2)
#define CHG_DISABLE_REASON_CHG_FULL_STOP	BIT(3)
#define CHG_DISABLE_REASON_CHARGER_NOT_INIT	BIT(4)

#define BQ2419X_SLOW_CURRENT_MA		(500)
#define BQ2419X_IN_CURRENT_LIMIT_MIN_MA	(100)

#define AICL_STEP_DELAY_MS		(200)
#define AICL_STEP_RETRY_MS		(300000)
#define AICL_DETECT_INPUT_VOLTAGE	(4200)
#define AICL_INPUT_CURRENT_TRIGGER_MA	(1500)
#define AICL_INPUT_CURRENT_MIN_MA	(900)
#define AICL_CURRENT_STEP_MA		(300)
#define AICL_INPUT_CURRENT_INIT_MA	AICL_INPUT_CURRENT_MIN_MA
#define AICL_CABLE_OUT_MAX_COUNT	(3)
#define AICL_RECHECK_TIME_S		(300)
#define AICL_MAX_RETRY_COUNT		(3)
#define AICL_DETECT_RESET_CHARGING_S	(2)

struct htc_battery_bq2419x_data {
	struct device			*dev;
	int				irq;

	struct mutex			mutex;
	int				in_current_limit;
	struct wake_lock		charge_change_wake_lock;

	struct regulator_dev		*chg_rdev;
	struct regulator_desc		chg_reg_desc;
	struct regulator_init_data	chg_reg_init_data;

	struct battery_charger_dev	*bc_dev;
	int				chg_status;

	int				chg_restart_time;
	bool				charger_presense;
	bool				battery_unknown;
	bool				cable_connected;
	int				last_charging_current;
	bool				disable_suspend_during_charging;
	int				last_temp;
	int				charging_state;
	unsigned int			last_chg_voltage;
	unsigned int			last_input_src;
	unsigned int			input_src;
	unsigned int			chg_current_control;
	unsigned int			prechg_control;
	unsigned int			term_control;
	unsigned int			chg_voltage_control;
	struct htc_battery_bq2419x_platform_data *pdata;
	unsigned int			otp_output_current;
	int				charge_suspend_polling_time;
	int				charge_polling_time;
	unsigned int			charging_disabled_reason;
	bool				chg_full_done;
	bool				chg_full_stop;
	bool				safety_timeout_happen;
	bool				is_recheck_charge;
	ktime_t				recheck_charge_time;
	struct htc_battery_bq2419x_ops	*ops;
	void				*ops_data;

	struct delayed_work		aicl_wq;
	int				aicl_input_current;
	enum aicl_phase			aicl_current_phase;
	int				aicl_dpm_found;
	int				aicl_target_input;
	int				aicl_cable_out_count_in_detect;
	int				aicl_cable_out_found;
	struct wake_lock		aicl_wake_lock;
	bool				aicl_wake_lock_locked;
	int				aicl_retry_count;
	bool				aicl_disable_after_detect;
};

static struct htc_battery_bq2419x_data *htc_battery_data;

static int htc_battery_bq2419x_charger_enable(
				struct htc_battery_bq2419x_data *data,
				unsigned int reason, bool enable)
{
	int ret = 0;

	dev_info(data->dev, "Charging %s with reason 0x%x (origin:0x%x)\n",
			enable ? "enable" : "disable",
			reason, data->charging_disabled_reason);

	if (enable)
		data->charging_disabled_reason &= ~reason;
	else
		data->charging_disabled_reason |= reason;

	if (!data->ops || !data->ops->set_charger_enable)
		return -EINVAL;

	ret = data->ops->set_charger_enable(!data->charging_disabled_reason,
						data->ops_data);
	if (ret < 0)
		dev_err(data->dev,
				"register update failed, err %d\n", ret);

	return ret;
}

static int htc_battery_bq2419x_process_plat_data(
		struct htc_battery_bq2419x_data *data,
		struct htc_battery_bq2419x_platform_data *chg_pdata)
{
	int voltage_input;
	int fast_charge_current;
	int pre_charge_current;
	int termination_current;
	int charge_voltage_limit;
	int otp_output_current;

	if (chg_pdata) {
		voltage_input = chg_pdata->input_voltage_limit_mv ?: 4200;
		fast_charge_current =
			chg_pdata->fast_charge_current_limit_ma ?: 4544;
		pre_charge_current =
			chg_pdata->pre_charge_current_limit_ma ?: 256;
		termination_current =
			chg_pdata->termination_current_limit_ma;
		charge_voltage_limit =
			chg_pdata->charge_voltage_limit_mv ?: 4208;
		otp_output_current =
			chg_pdata->thermal_prop.otp_output_current_ma ?: 1344;
	} else {
		voltage_input = 4200;
		fast_charge_current = 4544;
		pre_charge_current = 256;
		termination_current = 128;
		charge_voltage_limit = 4208;
		otp_output_current = 1344;
	}


	data->input_src = voltage_input;
	data->chg_current_control = fast_charge_current;
	data->prechg_control = pre_charge_current;
	data->term_control = termination_current;
	data->chg_voltage_control = charge_voltage_limit;
	data->otp_output_current = otp_output_current;

	return 0;
}

static int htc_battery_bq2419x_charger_init(
		struct htc_battery_bq2419x_data *data)
{
	int ret;

	if (!data->ops)
		return -EINVAL;

	if (data->ops->set_fastcharge_current) {
		ret = data->ops->set_fastcharge_current(
						data->chg_current_control,
						data->ops_data);
		if (ret < 0) {
			dev_err(data->dev, "set charge current failed %d\n",
					ret);
			return ret;
		}
	}

	if (data->ops->set_precharge_current) {
		ret = data->ops->set_precharge_current(data->prechg_control,
							data->ops_data);
		if (ret < 0) {
			dev_err(data->dev, "set precharge current failed %d\n",
					ret);
			return ret;
		}
	}

	if (data->ops->set_termination_current) {
		ret = data->ops->set_termination_current(data->term_control,
							data->ops_data);
		if (ret < 0) {
			dev_err(data->dev,
					"set termination current failed %d\n",
					ret);
			return ret;
		}
	}

	if (data->ops->set_dpm_input_voltage) {
		ret = data->ops->set_dpm_input_voltage(data->last_input_src,
							data->ops_data);
		if (ret < 0) {
			dev_err(data->dev, "set input voltage failed: %d\n",
					ret);
			return ret;
		}
	}

	if (data->ops->set_charge_voltage) {
		ret = data->ops->set_charge_voltage(data->last_chg_voltage,
							data->ops_data);
		if (ret < 0) {
			dev_err(data->dev, "set charger voltage failed %d\n",
					ret);
			return ret;
		}
	}

	return ret;
}

static int htc_battery_bq2419x_configure_charging_current(
				struct htc_battery_bq2419x_data *data,
				int in_current_limit)
{
	int ret;

	if (!data->ops || !data->ops->set_input_current)
		return -EINVAL;

	if (data->ops->set_charger_hiz) {
		ret = data->ops->set_charger_hiz(false, data->ops_data);
		if (ret < 0)
			dev_err(data->dev,
					"clear hiz failed: %d\n", ret);
	}

	ret = data->ops->set_input_current(in_current_limit, data->ops_data);
	if (ret < 0)
		dev_err(data->dev,
				"set input_current failed: %d\n", ret);

	return ret;
}

int htc_battery_bq2419x_full_current_enable(
					struct htc_battery_bq2419x_data *data)
{
	int ret;

	if (!data->ops || !data->ops->set_fastcharge_current)
		return -EINVAL;

	ret = data->ops->set_fastcharge_current(data->chg_current_control,
							data->ops_data);
	if (ret < 0) {
		dev_err(data->dev,
				"Failed to set charge current %d\n", ret);
		return ret;
	}

	data->charging_state = ENABLED_FULL_IBAT;

	return 0;
}

int htc_battery_bq2419x_half_current_enable(
					struct htc_battery_bq2419x_data *data)
{
	int ret;

	if (!data->ops || !data->ops->set_fastcharge_current)
		return -EINVAL;

	if (data->chg_current_control > data->otp_output_current) {
		ret = data->ops->set_fastcharge_current(
						data->otp_output_current,
						data->ops_data);
		if (ret < 0) {
			dev_err(data->dev,
				"Failed to set charge current %d\n", ret);
			return ret;
		}
	}

	data->charging_state = ENABLED_HALF_IBAT;

	return 0;
}

static inline void htc_battery_bq2419x_aicl_wakelock(
			struct htc_battery_bq2419x_data *data, bool lock)
{
	if (lock != data->aicl_wake_lock_locked) {
		if (lock)
			wake_lock(&data->aicl_wake_lock);
		else
			wake_unlock(&data->aicl_wake_lock);
		data->aicl_wake_lock_locked = lock;
	}
}

static void htc_battery_bq2419x_aicl_wq(struct work_struct *work)
{
	struct htc_battery_bq2419x_data *data;
	int ret;
	enum aicl_phase current_phase;
	int current_input_current;
	ktime_t timeout, cur_boottime;


	data = container_of(work, struct htc_battery_bq2419x_data,
							aicl_wq.work);

	mutex_lock(&data->mutex);

	current_phase = data->aicl_current_phase;
	current_input_current = data->aicl_input_current;

	dev_dbg(data->dev, "aicl: phase=%d, retry=%d, input=%d\n",
			current_phase,
			data->aicl_retry_count,
			current_input_current);

	if (current_phase == AICL_DETECT_INIT ||
			(current_phase == AICL_NOT_MATCH &&
			data->aicl_retry_count > 0)) {
		htc_battery_bq2419x_aicl_wakelock(data, true);
		current_input_current = AICL_INPUT_CURRENT_INIT_MA;
		current_phase = AICL_DETECT;
		data->aicl_dpm_found = false;
		data->aicl_cable_out_found = 0;

		/* check once to clear old record */
		cable_vbus_monitor_is_vbus_latched();

		--data->aicl_retry_count;
		if (data->ops && data->ops->set_dpm_input_voltage) {
			ret = data->ops->set_dpm_input_voltage(
						AICL_DETECT_INPUT_VOLTAGE,
						data->ops_data);
			if (ret < 0)
				dev_err(data->dev,
					"set input voltage failed: %d\n",
					ret);
			else
				data->last_input_src =
						AICL_DETECT_INPUT_VOLTAGE;
		}
	} else if (current_phase != AICL_DETECT) {
		htc_battery_bq2419x_aicl_wakelock(data, false);
		goto no_detect;
	} else {
		if (data->aicl_cable_out_found > 0) {
			/* input current raising might cause charger latched  */
			data->aicl_cable_out_count_in_detect++;
			if (data->aicl_cable_out_count_in_detect >=
					AICL_CABLE_OUT_MAX_COUNT) {
				current_input_current -= AICL_CURRENT_STEP_MA;
				current_phase = AICL_NOT_MATCH;
			}
		} else {
			if (!data->aicl_dpm_found)
				data->aicl_dpm_found =
					cable_vbus_monitor_is_vbus_latched();

			if (data->aicl_dpm_found) {
				current_input_current -= AICL_CURRENT_STEP_MA;
				current_phase = AICL_NOT_MATCH;
			} else if (current_input_current <
						data->aicl_target_input)
				current_input_current += AICL_CURRENT_STEP_MA;
			else
				current_phase = AICL_MATCH;
		}
	}

	if (current_input_current > data->aicl_target_input)
		current_input_current = data->aicl_target_input;
	else if (current_input_current < AICL_INPUT_CURRENT_MIN_MA)
		current_input_current = AICL_INPUT_CURRENT_MIN_MA;

	if (current_phase == AICL_DETECT || current_phase == AICL_DETECT_INIT) {
		data->aicl_cable_out_found = 0;

		ret = htc_battery_bq2419x_configure_charging_current(data,
							current_input_current);
		if (ret < 0) {
			dev_err(data->dev,
				"input current configure fail, ret = %d\n",
				ret);
			goto error;
		}
		data->aicl_current_phase = current_phase;
		data->aicl_input_current = current_input_current;
		data->aicl_dpm_found = cable_vbus_monitor_is_vbus_latched();
		schedule_delayed_work(&data->aicl_wq,
					msecs_to_jiffies(AICL_STEP_DELAY_MS));
	} else {
		if (data->aicl_disable_after_detect) {
			current_phase = AICL_DISABLED;
			current_input_current = data->in_current_limit;
			data->aicl_target_input = -1;
			dev_info(data->dev,
					"aicl disabled, aicl_result=(%d, %d)\n",
					current_phase, current_input_current);
		} else if (data->in_current_limit != data->aicl_target_input) {
			data->aicl_target_input = data->in_current_limit;
			if (data->in_current_limit >=
					AICL_INPUT_CURRENT_TRIGGER_MA) {
				current_input_current =
						AICL_INPUT_CURRENT_MIN_MA;
				current_phase = AICL_NOT_MATCH;
			} else {
				current_input_current = data->in_current_limit;
				current_phase = AICL_MATCH;
			}
			dev_info(data->dev,
					"input changed, aicl_result=(%d, %d)\n",
					current_phase, current_input_current);
		} else
			dev_info(data->dev, "aicl_result=(%d, %d)\n",
					current_phase, current_input_current);

		if (current_phase == AICL_NOT_MATCH &&
				data->aicl_input_current >
						AICL_INPUT_CURRENT_MIN_MA) {
			dev_info(data->dev, "limit aicl current to %d\n",
						AICL_INPUT_CURRENT_MIN_MA);
			current_input_current = AICL_INPUT_CURRENT_MIN_MA;
		}

		ret = htc_battery_bq2419x_configure_charging_current(data,
							current_input_current);
		if (ret < 0) {
			dev_err(data->dev,
					"input current configure fail, ret = %d\n",
					ret);
			goto error;
		}

		data->aicl_current_phase = current_phase;
		data->aicl_input_current = current_input_current;

		if (current_phase == AICL_NOT_MATCH && data->aicl_retry_count > 0)
			schedule_delayed_work(&data->aicl_wq,
					msecs_to_jiffies(AICL_STEP_RETRY_MS));

		htc_battery_bq2419x_aicl_wakelock(data, false);
	}

no_detect:
	mutex_unlock(&data->mutex);
	return;
error:
	data->aicl_current_phase = AICL_NOT_MATCH;
	data->aicl_input_current = AICL_INPUT_CURRENT_INIT_MA;
	if (!data->is_recheck_charge) {
		data->is_recheck_charge = true;
		cur_boottime = ktime_get_boottime();
		timeout = ktime_set(data->chg_restart_time, 0);
		data->recheck_charge_time = ktime_add(cur_boottime, timeout);
		battery_charging_restart(data->bc_dev, data->chg_restart_time);
	}

	htc_battery_bq2419x_aicl_wakelock(data, false);
	mutex_unlock(&data->mutex);
}

static int htc_battery_bq2419x_aicl_enable(
			struct htc_battery_bq2419x_data *data, bool enable)
{
	int ret = 0;

	data->aicl_disable_after_detect = false;
	if (!enable) {
		if (data->aicl_current_phase == AICL_DETECT ||
			data->aicl_current_phase == AICL_DETECT_INIT) {
			data->aicl_disable_after_detect = true;
			ret = -EBUSY;
		} else {
			cancel_delayed_work_sync(&data->aicl_wq);
			htc_battery_bq2419x_aicl_wakelock(data, false);
			data->aicl_current_phase = AICL_DISABLED;
		}
	} else {
		if (data->aicl_current_phase == AICL_DETECT ||
			data->aicl_current_phase == AICL_DETECT_INIT)
			ret = -EBUSY;
		else {
			data->aicl_retry_count = AICL_MAX_RETRY_COUNT;
			data->aicl_input_current = -1;
			data->aicl_target_input = -1;
			data->aicl_current_phase = AICL_NOT_MATCH;
		}
	}

	return ret;
}

static int htc_battery_bq2419x_aicl_configure_current(
		struct htc_battery_bq2419x_data *data, int target_input)
{
	int ret = 0;

	if (target_input < BQ2419X_IN_CURRENT_LIMIT_MIN_MA)
		return 0;

	switch (data->aicl_current_phase) {
	case AICL_DISABLED:
		data->aicl_target_input = target_input;
		data->aicl_input_current = target_input;
		break;
	case AICL_DETECT:
	case AICL_DETECT_INIT:
		if (!data->cable_connected)
			data->aicl_cable_out_found++;
		goto detecting;
	case AICL_MATCH:
	case AICL_NOT_MATCH:
		data->aicl_target_input = target_input;
		if (target_input >= AICL_INPUT_CURRENT_TRIGGER_MA) {
			htc_battery_bq2419x_aicl_wakelock(data, true);
			data->aicl_current_phase = AICL_DETECT_INIT;
			data->aicl_input_current = AICL_INPUT_CURRENT_INIT_MA;
			cancel_delayed_work_sync(&data->aicl_wq);
			schedule_delayed_work(&data->aicl_wq, 0);
		} else {
			data->aicl_input_current = target_input;
			data->aicl_current_phase = AICL_MATCH;
		}
		break;
	default:
		break;
	}

	ret = htc_battery_bq2419x_configure_charging_current(data,
			data->aicl_input_current);
	if (ret < 0)
		goto error;

detecting:
	data->in_current_limit = target_input;
error:
	return ret;
}

static int __htc_battery_bq2419x_set_charging_current(
			struct htc_battery_bq2419x_data *data,
			int charging_current)
{
	int in_current_limit;
	int old_current_limit;
	int ret = 0;
	unsigned int charger_state = 0;
	bool enable_safety_timer;
	ktime_t timeout, cur_boottime;

	data->chg_status = BATTERY_DISCHARGING;
	data->charging_state = STATE_INIT;
	data->chg_full_stop = false;
	data->chg_full_done = false;
	data->last_chg_voltage = data->chg_voltage_control;
	data->is_recheck_charge = false;

	battery_charging_restart_cancel(data->bc_dev);

	if (data->ops && data->ops->set_charge_voltage) {
		ret = data->ops->set_charge_voltage(
				data->last_chg_voltage,
				data->ops_data);
		if (ret < 0)
			goto error;
	} else {
		ret = -EINVAL;
		goto error;
	}

	ret = htc_battery_bq2419x_charger_enable(data,
			CHG_DISABLE_REASON_CHG_FULL_STOP, true);
	if (ret < 0)
		goto error;

	if (data->ops && data->ops->get_charger_state) {
		ret = data->ops->get_charger_state(&charger_state,
							data->ops_data);
		if (ret < 0)
			dev_err(data->dev, "charger state get failed: %d\n",
					ret);
	} else {
		ret = -EINVAL;
		goto error;
	}

	if (charging_current == 0 && charger_state != 0)
		goto done;

	old_current_limit = data->in_current_limit;
	if ((charger_state & HTC_BATTERY_BQ2419X_KNOWN_VBUS) == 0) {
		in_current_limit = BQ2419X_SLOW_CURRENT_MA;
		data->cable_connected = 0;
		data->chg_status = BATTERY_DISCHARGING;
		battery_charger_batt_status_stop_monitoring(data->bc_dev);
		htc_battery_bq2419x_aicl_enable(data, false);
	} else {
		in_current_limit = charging_current / 1000;
		data->cable_connected = 1;
		data->chg_status = BATTERY_CHARGING;
		if (!data->battery_unknown)
			battery_charger_batt_status_start_monitoring(
				data->bc_dev,
				data->last_charging_current / 1000);
		htc_battery_bq2419x_aicl_enable(data, true);
	}
	ret = htc_battery_bq2419x_aicl_configure_current(data,
			in_current_limit);
	if (ret < 0)
		goto error;

	if (data->battery_unknown)
		data->chg_status = BATTERY_UNKNOWN;

	if (in_current_limit > BQ2419X_SLOW_CURRENT_MA)
		enable_safety_timer = true;
	else
		enable_safety_timer = false;
	if (data->ops->set_safety_timer_enable) {
		ret = data->ops->set_safety_timer_enable(enable_safety_timer,
								data->ops_data);
		if (ret < 0)
			dev_err(data->dev, "safety timer control failed: %d\n",
									ret);
	}

	battery_charging_status_update(data->bc_dev, data->chg_status);

	if (data->disable_suspend_during_charging && !data->battery_unknown) {
		if (data->cable_connected &&
			data->in_current_limit > BQ2419X_SLOW_CURRENT_MA)
			battery_charger_acquire_wake_lock(data->bc_dev);
		else if (!data->cable_connected &&
			old_current_limit > BQ2419X_SLOW_CURRENT_MA)
			battery_charger_release_wake_lock(data->bc_dev);
	}

	return 0;
error:
	dev_err(data->dev, "Charger enable failed, err = %d\n", ret);
	data->is_recheck_charge = true;
	cur_boottime = ktime_get_boottime();
	timeout = ktime_set(data->chg_restart_time, 0);
	data->recheck_charge_time = ktime_add(cur_boottime, timeout);
	battery_charging_restart(data->bc_dev,
			data->chg_restart_time);
done:
	return ret;
}

static int htc_battery_bq2419x_set_charging_current(struct regulator_dev *rdev,
			int min_ua, int max_ua)
{
	struct htc_battery_bq2419x_data *data = rdev_get_drvdata(rdev);
	int ret = 0;

	if (!data || !data->charger_presense)
		return -EINVAL;

	dev_info(data->dev, "Setting charging current %d\n", max_ua/1000);
	msleep(200);

	mutex_lock(&data->mutex);
	wake_lock(&data->charge_change_wake_lock);

	data->safety_timeout_happen = false;
	data->last_charging_current = max_ua;

	ret = __htc_battery_bq2419x_set_charging_current(data, max_ua);
	if (ret < 0)
		dev_err(data->dev, "set charging current fail, ret=%d\n", ret);
	wake_unlock(&data->charge_change_wake_lock);
	mutex_unlock(&data->mutex);

	return ret;
}

static struct regulator_ops htc_battery_bq2419x_tegra_regulator_ops = {
	.set_current_limit = htc_battery_bq2419x_set_charging_current,
};

static int htc_battery_bq2419x_init_charger_regulator(
		struct htc_battery_bq2419x_data *data,
		struct htc_battery_bq2419x_platform_data *pdata)
{
	int ret = 0;
	struct regulator_config rconfig = { };

	if (!pdata) {
		dev_err(data->dev, "No platform data\n");
		return 0;
	}

	data->chg_reg_desc.name  = "data-charger";
	data->chg_reg_desc.ops   = &htc_battery_bq2419x_tegra_regulator_ops;
	data->chg_reg_desc.type  = REGULATOR_CURRENT;
	data->chg_reg_desc.owner = THIS_MODULE;

	data->chg_reg_init_data.supply_regulator	= NULL;
	data->chg_reg_init_data.regulator_init	= NULL;
	data->chg_reg_init_data.num_consumer_supplies =
				pdata->num_consumer_supplies;
	data->chg_reg_init_data.consumer_supplies	=
				pdata->consumer_supplies;
	data->chg_reg_init_data.driver_data		= data;
	data->chg_reg_init_data.constraints.name	= "data-charger";
	data->chg_reg_init_data.constraints.min_uA	= 0;
	data->chg_reg_init_data.constraints.max_uA	=
			pdata->max_charge_current_ma * 1000;

	data->chg_reg_init_data.constraints.ignore_current_constraint_init =
							true;
	data->chg_reg_init_data.constraints.valid_modes_mask =
						REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_STANDBY;

	data->chg_reg_init_data.constraints.valid_ops_mask =
						REGULATOR_CHANGE_MODE |
						REGULATOR_CHANGE_STATUS |
						REGULATOR_CHANGE_CURRENT;

	rconfig.dev = data->dev;
	rconfig.of_node = NULL;
	rconfig.init_data = &data->chg_reg_init_data;
	rconfig.driver_data = data;
	data->chg_rdev = devm_regulator_register(data->dev,
				&data->chg_reg_desc, &rconfig);
	if (IS_ERR(data->chg_rdev)) {
		ret = PTR_ERR(data->chg_rdev);
		dev_err(data->dev,
			"vbus-charger regulator register failed %d\n", ret);
	}

	return ret;
}

void htc_battery_bq2419x_notify(enum htc_battery_bq2419x_notify_event event)
{
	unsigned int charger_state = 0;
	int ret;

	if (!htc_battery_data || !htc_battery_data->charger_presense)
		return;

	if (event == HTC_BATTERY_BQ2419X_SAFETY_TIMER_TIMEOUT) {
		mutex_lock(&htc_battery_data->mutex);
		htc_battery_data->safety_timeout_happen = true;
		if (htc_battery_data->ops &&
				htc_battery_data->ops->get_charger_state) {
			ret = htc_battery_data->ops->get_charger_state(
						&charger_state,
						htc_battery_data->ops_data);
			if (ret < 0)
				dev_err(htc_battery_data->dev,
					"charger state get failed: %d\n",
						ret);
		}
		if ((charger_state & HTC_BATTERY_BQ2419X_CHARGING)
					!= HTC_BATTERY_BQ2419X_CHARGING) {
			htc_battery_data->chg_status = BATTERY_DISCHARGING;

			battery_charger_batt_status_stop_monitoring(
						htc_battery_data->bc_dev);
			battery_charging_status_update(htc_battery_data->bc_dev,
						htc_battery_data->chg_status);

			if (htc_battery_data->disable_suspend_during_charging
					&& htc_battery_data->cable_connected
					&& htc_battery_data->in_current_limit >
							BQ2419X_SLOW_CURRENT_MA)
				battery_charger_release_wake_lock(
						htc_battery_data->bc_dev);
		}
		mutex_unlock(&htc_battery_data->mutex);
	}
}
EXPORT_SYMBOL_GPL(htc_battery_bq2419x_notify);

int htc_battery_bq2419x_charger_register(struct htc_battery_bq2419x_ops *ops,
					void *data)
{
	int ret;

	if (!htc_battery_data || !htc_battery_data->charger_presense)
		return -ENODEV;

	if (!ops || !ops->set_charger_enable || !ops->set_fastcharge_current ||
			!ops->set_charge_voltage || !ops->set_input_current ||
			!ops->get_charger_state) {
		dev_err(htc_battery_data->dev,
				"Gauge operation register fail!");
		return -EINVAL;
	}

	mutex_lock(&htc_battery_data->mutex);
	htc_battery_data->ops = ops;
	htc_battery_data->ops_data = data;

	ret = htc_battery_bq2419x_charger_init(htc_battery_data);
	if (ret < 0)
		dev_err(htc_battery_data->dev, "Charger init failed: %d\n",
									ret);

	ret = htc_battery_bq2419x_charger_enable(htc_battery_data, 0, true);
	if (ret < 0)
		dev_err(htc_battery_data->dev, "Charger enable failed: %d\n",
									ret);
	mutex_unlock(&htc_battery_data->mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(htc_battery_bq2419x_charger_register);

int htc_battery_bq2419x_charger_unregister(void *data)
{
	int ret = 0;

	if (!htc_battery_data || !htc_battery_data->charger_presense)
		return -ENODEV;

	mutex_lock(&htc_battery_data->mutex);
	if (htc_battery_data->ops_data != data) {
		ret = -EINVAL;
		goto error;
	}
	htc_battery_data->is_recheck_charge = false;
	battery_charging_restart_cancel(htc_battery_data->bc_dev);
	cancel_delayed_work_sync(&htc_battery_data->aicl_wq);
	htc_battery_data->ops = NULL;
	htc_battery_data->ops_data = NULL;
error:
	mutex_unlock(&htc_battery_data->mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(htc_battery_bq2419x_charger_unregister);

static int htc_battery_bq2419x_charger_get_status(
					struct battery_charger_dev *bc_dev)
{
	struct htc_battery_bq2419x_data *data =
					battery_charger_get_drvdata(bc_dev);

	if (data)
		return data->chg_status;

	return BATTERY_DISCHARGING;
}

static int htc_battery_bq2419x_charger_thermal_configure_no_thermister(
		struct battery_charger_dev *bc_dev,
		int temp, bool enable_charger, bool enable_charg_half_current,
		int battery_voltage)
{
	struct htc_battery_bq2419x_data *data =
					battery_charger_get_drvdata(bc_dev);
	int ret = 0;

	if (!data)
		return -EINVAL;

	mutex_lock(&data->mutex);
	wake_lock(&data->charge_change_wake_lock);
	if (!data->cable_connected)
		goto done;

	if (data->last_temp == temp && data->charging_state != STATE_INIT)
		goto done;

	data->last_temp = temp;

	dev_info(data->dev, "Battery temp %d\n", temp);
	if (enable_charger) {

		if (battery_voltage > 0
			&& battery_voltage <= data->chg_voltage_control
			&& battery_voltage != data->last_chg_voltage) {
			/*Set charge voltage */
			if (data->ops && data->ops->set_charge_voltage) {
				ret = data->ops->set_charge_voltage(
						battery_voltage,
						data->ops_data);
				if (ret < 0)
					goto error;
				data->last_chg_voltage = battery_voltage;
			}
		}

		if (data->charging_state == DISABLED) {
			ret = htc_battery_bq2419x_charger_enable(data,
					CHG_DISABLE_REASON_THERMAL, true);
			if (ret < 0)
				goto error;
		}

		if (!enable_charg_half_current &&
			data->charging_state != ENABLED_FULL_IBAT) {
			htc_battery_bq2419x_full_current_enable(data);
			battery_charging_status_update(data->bc_dev,
							BATTERY_CHARGING);
		} else if (enable_charg_half_current &&
			data->charging_state != ENABLED_HALF_IBAT) {
			htc_battery_bq2419x_half_current_enable(data);
			battery_charging_status_update(data->bc_dev,
							BATTERY_CHARGING);
		}
	} else {
		if (data->charging_state != DISABLED) {
			ret = htc_battery_bq2419x_charger_enable(data,
					CHG_DISABLE_REASON_THERMAL, false);

			if (ret < 0)
				goto error;
			data->charging_state = DISABLED;
			battery_charging_status_update(data->bc_dev,
						BATTERY_DISCHARGING);
		}
	}

error:
done:
	wake_unlock(&data->charge_change_wake_lock);
	mutex_unlock(&data->mutex);
	return ret;
}

static int htc_battery_bq2419x_charging_restart(
					struct battery_charger_dev *bc_dev)
{
	struct htc_battery_bq2419x_data *data =
					battery_charger_get_drvdata(bc_dev);
	int ret = 0;

	if (!data)
		return -EINVAL;

	mutex_lock(&data->mutex);
	wake_lock(&data->charge_change_wake_lock);
	if (!data->is_recheck_charge || data->safety_timeout_happen)
		goto done;

	dev_info(data->dev, "Restarting the charging\n");
	ret = __htc_battery_bq2419x_set_charging_current(data,
			data->last_charging_current);
	if (ret < 0)
		dev_err(data->dev,
			"Restarting of charging failed: %d\n", ret);

done:
	wake_unlock(&data->charge_change_wake_lock);
	mutex_unlock(&data->mutex);
	return ret;
}

static int htc_battery_bq2419x_charger_charging_full_configure(
		struct battery_charger_dev *bc_dev,
		bool charge_full_done, bool charge_full_stop)
{
	struct htc_battery_bq2419x_data *data =
					battery_charger_get_drvdata(bc_dev);

	if (!data)
		return -EINVAL;

	mutex_lock(&data->mutex);
	wake_lock(&data->charge_change_wake_lock);

	if (data->battery_unknown)
		goto done;

	if (!data->cable_connected)
		goto done;

	if (charge_full_done != data->chg_full_done) {
		data->chg_full_done = charge_full_done;
		if (charge_full_done) {
			if (data->last_chg_voltage ==
					data->chg_voltage_control) {
				dev_info(data->dev, "Charging completed\n");
				data->chg_status = BATTERY_CHARGING_DONE;
				battery_charging_status_update(data->bc_dev,
						data->chg_status);
			} else
				dev_info(data->dev, "OTP charging completed\n");
		} else {
			data->chg_status = BATTERY_CHARGING;
			battery_charging_status_update(data->bc_dev,
					data->chg_status);
		}
	}

	if (charge_full_stop != data->chg_full_stop) {
		data->chg_full_stop = charge_full_stop;
		if (charge_full_stop) {
			htc_battery_bq2419x_charger_enable(data,
					CHG_DISABLE_REASON_CHG_FULL_STOP,
					false);
			if (data->disable_suspend_during_charging
				&& data->in_current_limit >
						BQ2419X_SLOW_CURRENT_MA)
				battery_charger_release_wake_lock(
						data->bc_dev);
		} else {
			htc_battery_bq2419x_charger_enable(data,
					CHG_DISABLE_REASON_CHG_FULL_STOP,
					true);
			if (data->disable_suspend_during_charging &&
				data->in_current_limit >
						BQ2419X_SLOW_CURRENT_MA)
				battery_charger_acquire_wake_lock(
						data->bc_dev);
		}
	}
done:
	wake_unlock(&data->charge_change_wake_lock);
	mutex_unlock(&data->mutex);
	return 0;
}

static int htc_battery_bq2419x_input_control(
		struct battery_charger_dev *bc_dev, int voltage_min)
{
	struct htc_battery_bq2419x_data *data =
					battery_charger_get_drvdata(bc_dev);
	unsigned int val;
	int ret = 0;
	int now_input;

	if (!data)
		return -EINVAL;

	mutex_lock(&data->mutex);
	wake_lock(&data->charge_change_wake_lock);
	if (!data->ops || !data->ops->set_dpm_input_voltage) {
		ret = -EINVAL;
		goto error;
	}

	if (data->battery_unknown)
		goto done;

	if (!data->cable_connected)
		goto done;

	if (data->aicl_current_phase == AICL_DETECT ||
			data->aicl_current_phase == AICL_DETECT_INIT)
		goto done;

	/* input source checking */
	if (voltage_min != data->last_input_src) {
		ret = data->ops->set_dpm_input_voltage(voltage_min,
							data->ops_data);
		if (ret < 0)
			dev_err(data->dev,
					"set input voltage failed: %d\n",
					ret);
		else
			data->last_input_src = voltage_min;
	}

	/* Check input current limit if reset */
	if (data->ops->get_input_current) {
		if (data->aicl_input_current > 0)
			now_input = data->aicl_input_current;
		else
			now_input = data->in_current_limit;
		ret = data->ops->get_input_current(&val, data->ops_data);

		if (!ret && val != now_input)
			htc_battery_bq2419x_configure_charging_current(data,
								now_input);
	}

error:
done:
	wake_unlock(&data->charge_change_wake_lock);
	mutex_unlock(&data->mutex);

	return ret;
}

static int htc_battery_bq2419x_unknown_battery_handle(
					struct battery_charger_dev *bc_dev)
{
	int ret = 0;
	struct htc_battery_bq2419x_data *data =
					battery_charger_get_drvdata(bc_dev);

	if (!data)
		return -EINVAL;

	mutex_lock(&data->mutex);
	wake_lock(&data->charge_change_wake_lock);
	data->battery_unknown = true;
	data->chg_status = BATTERY_UNKNOWN;
	data->charging_state = STATE_INIT;

	ret = htc_battery_bq2419x_charger_enable(data,
			CHG_DISABLE_REASON_UNKNOWN_BATTERY, false);
	if (ret < 0) {
		dev_err(data->dev,
				"charger enable failed %d\n", ret);
		goto error;
	}

	battery_charger_batt_status_stop_monitoring(data->bc_dev);

	battery_charging_status_update(data->bc_dev, data->chg_status);

	if (data->disable_suspend_during_charging
			&& data->cable_connected
			&& data->in_current_limit > BQ2419X_SLOW_CURRENT_MA)
			battery_charger_release_wake_lock(data->bc_dev);
error:
	wake_unlock(&data->charge_change_wake_lock);
	mutex_unlock(&data->mutex);
	return ret;
}

static struct battery_charging_ops htc_battery_bq2419x_charger_bci_ops = {
	.get_charging_status = htc_battery_bq2419x_charger_get_status,
	.restart_charging = htc_battery_bq2419x_charging_restart,
	.thermal_configure =
		htc_battery_bq2419x_charger_thermal_configure_no_thermister,
	.charging_full_configure =
		htc_battery_bq2419x_charger_charging_full_configure,
	.input_voltage_configure = htc_battery_bq2419x_input_control,
	.unknown_battery_handle = htc_battery_bq2419x_unknown_battery_handle,
};

static struct battery_charger_info htc_battery_bq2419x_charger_bci = {
	.cell_id = 0,
	.bc_ops = &htc_battery_bq2419x_charger_bci_ops,
};

static struct htc_battery_bq2419x_platform_data
		*htc_battery_bq2419x_dt_parse(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct htc_battery_bq2419x_platform_data *pdata;
	int ret;
	int chg_restart_time;
	int suspend_polling_time;
	int temp_polling_time;
	int thermal_temp;
	unsigned int thermal_volt;
	unsigned int otp_output_current;
	unsigned int unknown_batt_id_min;
	unsigned int input_switch_val;
	unsigned int full_thr_val;
	struct regulator_init_data *batt_init_data;
	u32 pval;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	batt_init_data = of_get_regulator_init_data(&pdev->dev, np);
	if (!batt_init_data)
		return ERR_PTR(-EINVAL);

	ret = of_property_read_u32(np,
			"input-voltage-limit-millivolt", &pval);
	if (!ret)
		pdata->input_voltage_limit_mv = pval;

	ret = of_property_read_u32(np,
			"fast-charge-current-limit-milliamp", &pval);
	if (!ret)
		pdata->fast_charge_current_limit_ma = pval;

	ret = of_property_read_u32(np,
			"pre-charge-current-limit-milliamp", &pval);
	if (!ret)
		pdata->pre_charge_current_limit_ma = pval;

	ret = of_property_read_u32(np,
			"charge-term-current-limit-milliamp", &pval);
	if (!ret)
		pdata->termination_current_limit_ma = pval;

	ret = of_property_read_u32(np,
			"charge-voltage-limit-millivolt", &pval);
	if (!ret)
		pdata->charge_voltage_limit_mv = pval;

	pdata->disable_suspend_during_charging = of_property_read_bool(np,
				"disable-suspend-during-charging");

	ret = of_property_read_u32(np, "auto-recharge-time",
				&chg_restart_time);
	if (!ret)
		pdata->chg_restart_time = chg_restart_time;

	ret = of_property_read_u32(np, "charge-suspend-polling-time-sec",
			&suspend_polling_time);
	if (!ret)
		pdata->charge_suspend_polling_time_sec = suspend_polling_time;

	ret = of_property_read_u32(np, "temp-polling-time-sec",
			&temp_polling_time);
	if (!ret)
		pdata->temp_polling_time_sec = temp_polling_time;

	pdata->consumer_supplies = batt_init_data->consumer_supplies;
	pdata->num_consumer_supplies = batt_init_data->num_consumer_supplies;
	pdata->max_charge_current_ma =
			batt_init_data->constraints.max_uA / 1000;

	ret = of_property_read_u32(np, "thermal-temperature-hot-deciC",
			&thermal_temp);
	if (!ret)
		pdata->thermal_prop.temp_hot_dc = thermal_temp;

	ret = of_property_read_u32(np, "thermal-temperature-cold-deciC",
			&thermal_temp);
	if (!ret)
		pdata->thermal_prop.temp_cold_dc = thermal_temp;

	ret = of_property_read_u32(np, "thermal-temperature-warm-deciC",
			&thermal_temp);
	if (!ret)
		pdata->thermal_prop.temp_warm_dc = thermal_temp;

	ret = of_property_read_u32(np, "thermal-temperature-cool-deciC",
			&thermal_temp);
	if (!ret)
		pdata->thermal_prop.temp_cool_dc = thermal_temp;

	ret = of_property_read_u32(np, "thermal-temperature-hysteresis-deciC",
			&thermal_temp);
	if (!ret)
		pdata->thermal_prop.temp_hysteresis_dc = thermal_temp;

	ret = of_property_read_u32(np, "thermal-warm-voltage-millivolt",
			&thermal_volt);
	if (!ret)
		pdata->thermal_prop.warm_voltage_mv = thermal_volt;

	ret = of_property_read_u32(np, "thermal-cool-voltage-millivolt",
			&thermal_volt);
	if (!ret)
		pdata->thermal_prop.cool_voltage_mv = thermal_volt;

	pdata->thermal_prop.disable_warm_current_half =
		of_property_read_bool(np, "thermal-disable-warm-current-half");

	pdata->thermal_prop.disable_cool_current_half =
		of_property_read_bool(np, "thermal-disable-cool-current-half");

	ret = of_property_read_u32(np,
			"thermal-overtemp-output-current-milliamp",
			&otp_output_current);
	if (!ret)
		pdata->thermal_prop.otp_output_current_ma = otp_output_current;

	ret = of_property_read_u32(np,
			"charge-full-done-voltage-min-millivolt",
			&full_thr_val);
	if (!ret)
		pdata->full_thr.chg_done_voltage_min_mv = full_thr_val;

	ret = of_property_read_u32(np,
			"charge-full-done-current-min-milliamp",
			&full_thr_val);
	if (!ret)
		pdata->full_thr.chg_done_current_min_ma = full_thr_val;

	ret = of_property_read_u32(np,
			"charge-full-done-low-current-min-milliamp",
			&full_thr_val);
	if (!ret)
		pdata->full_thr.chg_done_low_current_min_ma = full_thr_val;

	ret = of_property_read_u32(np,
			"charge-full-recharge-voltage-min-millivolt",
			&full_thr_val);
	if (!ret)
		pdata->full_thr.recharge_voltage_min_mv = full_thr_val;

	ret = of_property_read_u32(np,
			"input-voltage-min-high-battery-millivolt",
			&input_switch_val);
	if (!ret)
		pdata->input_switch.input_vmin_high_mv = input_switch_val;

	ret = of_property_read_u32(np,
			"input-voltage-min-low-battery-millivolt",
			&input_switch_val);
	if (!ret)
		pdata->input_switch.input_vmin_low_mv = input_switch_val;

	ret = of_property_read_u32(np, "input-voltage-switch-millivolt",
			&input_switch_val);
	if (!ret)
		pdata->input_switch.input_switch_threshold_mv =
			input_switch_val;

	pdata->batt_id_channel_name =
		of_get_property(np, "battery-id-channel-name", NULL);

	ret = of_property_read_u32(np, "unknown-battery-id-minimum",
			&unknown_batt_id_min);
	if (!ret)
		pdata->unknown_batt_id_min = unknown_batt_id_min;

	return pdata;
}

static inline void htc_battery_bq2419x_battery_info_init(
	struct htc_battery_bq2419x_platform_data *pdata)
{
	htc_battery_bq2419x_charger_bci.polling_time_sec =
			pdata->temp_polling_time_sec;

	htc_battery_bq2419x_charger_bci.thermal_prop.temp_hot_dc =
			pdata->thermal_prop.temp_hot_dc;
	htc_battery_bq2419x_charger_bci.thermal_prop.temp_cold_dc =
			pdata->thermal_prop.temp_cold_dc;
	htc_battery_bq2419x_charger_bci.thermal_prop.temp_warm_dc =
			pdata->thermal_prop.temp_warm_dc;
	htc_battery_bq2419x_charger_bci.thermal_prop.temp_cool_dc =
			pdata->thermal_prop.temp_cool_dc;
	htc_battery_bq2419x_charger_bci.thermal_prop.temp_hysteresis_dc =
			pdata->thermal_prop.temp_hysteresis_dc;
	htc_battery_bq2419x_charger_bci.thermal_prop.regulation_voltage_mv =
			pdata->charge_voltage_limit_mv;
	htc_battery_bq2419x_charger_bci.thermal_prop.warm_voltage_mv =
			pdata->thermal_prop.warm_voltage_mv;
	htc_battery_bq2419x_charger_bci.thermal_prop.cool_voltage_mv =
			pdata->thermal_prop.cool_voltage_mv;
	htc_battery_bq2419x_charger_bci.thermal_prop.disable_warm_current_half =
			pdata->thermal_prop.disable_warm_current_half;
	htc_battery_bq2419x_charger_bci.thermal_prop.disable_cool_current_half =
			pdata->thermal_prop.disable_cool_current_half;

	htc_battery_bq2419x_charger_bci.full_thr.chg_done_voltage_min_mv =
			pdata->full_thr.chg_done_voltage_min_mv ?:
			(pdata->charge_voltage_limit_mv ?
			pdata->charge_voltage_limit_mv - 102 : 0);
	htc_battery_bq2419x_charger_bci.full_thr.chg_done_current_min_ma =
			pdata->full_thr.chg_done_current_min_ma;
	htc_battery_bq2419x_charger_bci.full_thr.chg_done_low_current_min_ma =
			pdata->full_thr.chg_done_low_current_min_ma;
	htc_battery_bq2419x_charger_bci.full_thr.recharge_voltage_min_mv =
			pdata->full_thr.recharge_voltage_min_mv ?:
			(pdata->charge_voltage_limit_mv ?
			pdata->charge_voltage_limit_mv - 48 : 0);

	htc_battery_bq2419x_charger_bci.input_switch.input_vmin_high_mv =
			pdata->input_switch.input_vmin_high_mv;
	htc_battery_bq2419x_charger_bci.input_switch.input_vmin_low_mv =
			pdata->input_switch.input_vmin_low_mv;
	htc_battery_bq2419x_charger_bci.input_switch.input_switch_threshold_mv =
			pdata->input_switch.input_switch_threshold_mv;

	htc_battery_bq2419x_charger_bci.batt_id_channel_name =
			pdata->batt_id_channel_name;
	htc_battery_bq2419x_charger_bci.unknown_batt_id_min =
			pdata->unknown_batt_id_min;

	htc_battery_bq2419x_charger_bci.enable_batt_status_monitor = true;
}

static int htc_battery_bq2419x_probe(struct platform_device *pdev)
{
	struct htc_battery_bq2419x_data *data;
	struct htc_battery_bq2419x_platform_data *pdata = NULL;

	int ret = 0;

	if (pdev->dev.platform_data)
		pdata = pdev->dev.platform_data;

	if (!pdata && pdev->dev.of_node) {
		pdata = htc_battery_bq2419x_dt_parse(pdev);
		if (IS_ERR(pdata)) {
			ret = PTR_ERR(pdata);
			dev_err(&pdev->dev, "Parsing of node failed, %d\n",
				ret);
			return ret;
		}
	}

	if (!pdata) {
		dev_err(&pdev->dev, "No Platform data");
		return -EINVAL;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Memory allocation failed\n");
		return -ENOMEM;
	}

	data->pdata = pdata;

	data->dev = &pdev->dev;

	mutex_init(&data->mutex);
	wake_lock_init(&data->charge_change_wake_lock, WAKE_LOCK_SUSPEND,
						"charge-change-lock");
	wake_lock_init(&data->aicl_wake_lock, WAKE_LOCK_SUSPEND,
						"charge-aicl-lock");
	INIT_DELAYED_WORK(&data->aicl_wq, htc_battery_bq2419x_aicl_wq);

	dev_set_drvdata(&pdev->dev, data);

	data->chg_restart_time = pdata->chg_restart_time;
	data->charger_presense = true;
	data->battery_unknown = false;
	data->last_temp = -1000;
	data->disable_suspend_during_charging =
			pdata->disable_suspend_during_charging;
	data->charge_suspend_polling_time =
			pdata->charge_suspend_polling_time_sec;
	data->charge_polling_time = pdata->temp_polling_time_sec;

	htc_battery_bq2419x_process_plat_data(data, pdata);

	data->last_chg_voltage = data->chg_voltage_control;
	data->last_input_src = data->input_src;

	ret = htc_battery_bq2419x_init_charger_regulator(data, pdata);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Charger regualtor init failed %d\n", ret);
		goto scrub_mutex;
	}

	htc_battery_bq2419x_battery_info_init(pdata);

	data->bc_dev = battery_charger_register(data->dev,
				&htc_battery_bq2419x_charger_bci, data);
	if (IS_ERR(data->bc_dev)) {
		ret = PTR_ERR(data->bc_dev);
		dev_err(data->dev, "battery charger register failed: %d\n",
			ret);
		data->bc_dev = NULL;
		goto scrub_mutex;
	}

	htc_battery_data = data;

	return 0;
scrub_mutex:
	data->charger_presense = false;
	ret = htc_battery_bq2419x_charger_enable(data,
			CHG_DISABLE_REASON_CHARGER_NOT_INIT, false);
	wake_lock_destroy(&data->charge_change_wake_lock);
	wake_lock_destroy(&data->aicl_wake_lock);
	mutex_destroy(&data->mutex);
	return ret;
}

static int htc_battery_bq2419x_remove(struct platform_device *pdev)
{
	struct htc_battery_bq2419x_data *data = dev_get_drvdata(&pdev->dev);

	if (data->charger_presense) {
		if (data->bc_dev) {
			battery_charger_unregister(data->bc_dev);
			data->bc_dev = NULL;
		}
		cancel_delayed_work_sync(&data->aicl_wq);
		wake_lock_destroy(&data->charge_change_wake_lock);
		wake_lock_destroy(&data->charge_change_wake_lock);
	}
	mutex_destroy(&data->mutex);
	return 0;
}

static void htc_battery_bq2419x_shutdown(struct platform_device *pdev)
{
	struct htc_battery_bq2419x_data *data = dev_get_drvdata(&pdev->dev);

	if (!data->charger_presense)
		return;

	battery_charging_system_power_on_usb_event(data->bc_dev);
}

#ifdef CONFIG_PM_SLEEP
static int htc_battery_bq2419x_suspend(struct device *dev)
{
	struct htc_battery_bq2419x_data *data = dev_get_drvdata(dev);
	int next_wakeup = 0;

	battery_charging_restart_cancel(data->bc_dev);

	if (!data->charger_presense || data->battery_unknown)
		return 0;

	if (!data->cable_connected)
		goto end;

	if (data->chg_full_stop)
		next_wakeup =
			data->charge_suspend_polling_time;
	else
		next_wakeup = data->charge_polling_time;

	battery_charging_wakeup(data->bc_dev, next_wakeup);
end:
	if (next_wakeup)
		dev_info(dev, "System-charger will resume after %d sec\n",
				next_wakeup);
	else
		dev_info(dev, "System-charger will not have resume time\n");

	return 0;
}

static int htc_battery_bq2419x_resume(struct device *dev)
{
	struct htc_battery_bq2419x_data *data = dev_get_drvdata(dev);
	ktime_t cur_boottime;
	s64 restart_time_s;
	s64 batt_status_check_pass_time;
	int ret;

	if (!data->charger_presense)
		return 0;

	if (data->is_recheck_charge) {
		cur_boottime = ktime_get_boottime();
		if (ktime_compare(cur_boottime, data->recheck_charge_time) >= 0)
			restart_time_s = 0;
		else {
			restart_time_s = ktime_us_delta(
					data->recheck_charge_time,
					cur_boottime);
			do_div(restart_time_s, 1000000);
		}

		dev_info(dev, "restart charging time is %lld\n",
				restart_time_s);
		battery_charging_restart(data->bc_dev, (int) restart_time_s);
	}

	if (data->cable_connected && !data->battery_unknown) {
		ret = battery_charger_get_batt_status_no_update_time_ms(
				data->bc_dev,
				&batt_status_check_pass_time);
		if (!ret) {
			if (batt_status_check_pass_time >
					data->charge_polling_time * 1000)
				battery_charger_batt_status_force_check(
						data->bc_dev);
		}
	}

	return 0;
};
#endif

static const struct dev_pm_ops htc_battery_bq2419x_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(htc_battery_bq2419x_suspend,
			htc_battery_bq2419x_resume)
};

static const struct of_device_id htc_battery_bq2419x_dt_match[] = {
	{ .compatible = "htc,bq2419x_battery" },
	{ },
};
MODULE_DEVICE_TABLE(of, htc_battery_bq2419x_dt_match);

static struct platform_driver htc_battery_bq2419x_driver = {
	.driver	= {
		.name	= "htc_battery_bq2419x",
		.of_match_table = of_match_ptr(htc_battery_bq2419x_dt_match),
		.owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm = &htc_battery_bq2419x_pm_ops,
#endif /* CONFIG_PM */
	},
	.probe		= htc_battery_bq2419x_probe,
	.remove		= htc_battery_bq2419x_remove,
	.shutdown	= htc_battery_bq2419x_shutdown,
};

static int __init htc_battery_bq2419x_init(void)
{
	return platform_driver_register(&htc_battery_bq2419x_driver);
}
subsys_initcall(htc_battery_bq2419x_init);

static void __exit htc_battery_bq2419x_exit(void)
{
	platform_driver_unregister(&htc_battery_bq2419x_driver);
}
module_exit(htc_battery_bq2419x_exit);

MODULE_DESCRIPTION("HTC battery bq2419x module");
MODULE_LICENSE("GPL");
