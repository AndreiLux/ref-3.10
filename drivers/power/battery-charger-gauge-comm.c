/*
 * battery-charger-gauge-comm.c -- Communication between battery charger and
 *	battery gauge driver.
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
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

#include <linux/alarmtimer.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/thermal.h>
#include <linux/list.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/power/battery-charger-gauge-comm.h>
#include <linux/power/reset/system-pmic.h>
#include <linux/wakelock.h>
#include <linux/iio/consumer.h>
#include <linux/iio/types.h>
#include <linux/iio/iio.h>
#include <linux/power_supply.h>

#define JETI_TEMP_COLD		(0)
#define JETI_TEMP_COOL		(100)
#define JETI_TEMP_WARM		(450)
#define JETI_TEMP_HOT		(600)

#define DEFAULT_BATTERY_REGULATION_VOLTAGE	(4250)
#define DEFAULT_BATTERY_THERMAL_VOLTAGE		(4100)
#define DEFAULT_CHARGE_DONE_CURRENT		(670)
#define DEFAULT_CHARGE_DONE_LOW_CURRENT		(400)

#define MAX_STR_PRINT		(50)

#define BATT_INFO_NO_VALUE	(-1)

#define CHARGING_CURRENT_0_TORRENCE	(-10)
#define CHARGING_FULL_DONE_CURRENT_THRESHOLD_TORRENCE	(200)

#define CHARGING_FULL_DONE_CHECK_TIMES		(3)
#define CHARGING_FULL_DONE_TIMEOUT_S		(5400)
#define CHARGING_FULL_DONE_LEVEL_MIN		(96)

#define UNKNOWN_BATTERY_ID_CHECK_COUNT	(5)
#define UNKNOWN_BATTERY_ID_CHECK_DELAY	(3*HZ)

#define DEFAULT_INPUT_VMIN_MV	(4200)

enum battery_monitor_state {
	MONITOR_WAIT = 0,
	MONITOR_THERMAL,
	MONITOR_BATT_STATUS,
};

static DEFINE_MUTEX(charger_gauge_list_mutex);
static LIST_HEAD(charger_list);
static LIST_HEAD(gauge_list);

struct battery_info_ops {
	int (*get_battery_temp)(void);
};

struct battery_charger_dev {
	int				cell_id;
	char				*tz_name;
	struct device			*parent_dev;
	struct battery_charging_ops	*ops;
	struct list_head		list;
	void				*drv_data;
	struct delayed_work		restart_charging_wq;
	struct delayed_work		poll_temp_monitor_wq;
	struct delayed_work		poll_batt_status_monitor_wq;
	int				polling_time_sec;
	struct thermal_zone_device	*battery_tz;
	enum battery_monitor_state	start_monitoring;
	struct wake_lock		charger_wake_lock;
	bool				locked;
	struct rtc_device		*rtc;
	bool				enable_thermal_monitor;
	bool				enable_batt_status_monitor;
	ktime_t				batt_status_last_check_time;
	struct battery_thermal_prop	thermal_prop;
	enum charge_thermal_state	thermal_state;
	struct battery_info_ops		batt_info_ops;
	bool				chg_full_done;
	bool				chg_full_stop;
	int				chg_full_done_prev_check_count;
	ktime_t				chg_full_stop_expire_time;
	int				in_current_limit;
	struct charge_full_threshold	full_thr;
	struct mutex			mutex;
	const char			*batt_id_channel_name;
	struct charge_input_switch	input_switch;
	int				unknown_batt_id_min;
	struct delayed_work		unknown_batt_id_work;
	int				unknown_batt_id_check_count;
	const char			*gauge_psy_name;
	struct power_supply		*psy;
};

struct battery_gauge_dev {
	int				cell_id;
	char				*tz_name;
	struct device			*parent_dev;
	struct battery_gauge_ops	*ops;
	struct list_head		list;
	void				*drv_data;
	struct thermal_zone_device	*battery_tz;
	int				battery_voltage;
	int				battery_current;
	int				battery_capacity;
	int				battery_snapshot_voltage;
	int				battery_snapshot_current;
	int				battery_snapshot_capacity;
	const char			*bat_curr_channel_name;
	struct iio_channel		*bat_current_iio_channel;
};

struct battery_gauge_dev *bg_temp;

static inline int psy_get_property(struct power_supply *psy,
				enum power_supply_property psp, int *val)
{
	union power_supply_propval pv;

	if (!psy || !val)
		return -EINVAL;

	if (psy->get_property(psy, psp, &pv))
		return -EFAULT;

	*val = pv.intval;
	return 0;
}

static void battery_charger_restart_charging_wq(struct work_struct *work)
{
	struct battery_charger_dev *bc_dev;

	bc_dev = container_of(work, struct battery_charger_dev,
					restart_charging_wq.work);
	if (!bc_dev->ops->restart_charging) {
		dev_err(bc_dev->parent_dev,
				"No callback for restart charging\n");
		return;
	}
	bc_dev->ops->restart_charging(bc_dev);
}

static int battery_charger_thermal_monitor_func(
					struct battery_charger_dev *bc_dev)
{
	struct device *dev;
	long temperature;
	bool charger_enable_state;
	bool charger_current_half;
	int battery_thersold_voltage;
	int temp;
	enum charge_thermal_state thermal_state_new;
	int ret;

	dev = bc_dev->parent_dev;

	if (bc_dev->tz_name) {
		if (!bc_dev->battery_tz) {
			bc_dev->battery_tz = thermal_zone_device_find_by_name(
					bc_dev->tz_name);

			if (!bc_dev->battery_tz) {
				dev_info(dev,
					"Battery thermal zone %s is not registered yet\n",
					bc_dev->tz_name);
				schedule_delayed_work(
					&bc_dev->poll_temp_monitor_wq,
					msecs_to_jiffies(
					bc_dev->polling_time_sec * 1000));
				return -EINVAL;
			}
		}

		ret = thermal_zone_get_temp(bc_dev->battery_tz, &temperature);
		if (ret < 0) {
			dev_err(dev, "Temperature read failed: %d\n ", ret);
			return 0;
		}
		temperature = temperature / 100;
	} else if (bc_dev->batt_info_ops.get_battery_temp)
		temperature = bc_dev->batt_info_ops.get_battery_temp();
	else if (bc_dev->psy) {
		ret = psy_get_property(bc_dev->psy, POWER_SUPPLY_PROP_TEMP,
				&temp);
		if (ret) {
			dev_err(dev,
				"POWER_SUPPLY_PROP_TEMP read failed: %d\n ",
				ret);
			return ret;
		}
		temperature = temp;
	} else
		return -EINVAL;


	if (temperature <= bc_dev->thermal_prop.temp_cold_dc)
		thermal_state_new = CHARGE_THERMAL_COLD_STOP;
	else if (temperature <= bc_dev->thermal_prop.temp_cool_dc) {
		if (bc_dev->thermal_state == CHARGE_THERMAL_COLD_STOP &&
			temperature < bc_dev->thermal_prop.temp_cold_dc
				+ bc_dev->thermal_prop.temp_hysteresis_dc)
			thermal_state_new = CHARGE_THERMAL_COOL_STOP;
		else
			thermal_state_new = CHARGE_THERMAL_COOL;
	} else if (temperature < bc_dev->thermal_prop.temp_warm_dc)
		thermal_state_new = CHARGE_THERMAL_NORMAL;
	else if (temperature < bc_dev->thermal_prop.temp_hot_dc) {
		if (bc_dev->thermal_state == CHARGE_THERMAL_HOT_STOP &&
			temperature >= bc_dev->thermal_prop.temp_hot_dc
				- bc_dev->thermal_prop.temp_hysteresis_dc)
			thermal_state_new = CHARGE_THERMAL_WARM_STOP;
		else
			thermal_state_new = CHARGE_THERMAL_WARM;
	} else
		thermal_state_new = CHARGE_THERMAL_HOT_STOP;

	mutex_lock(&bc_dev->mutex);
	if (bc_dev->thermal_state != thermal_state_new)
		dev_info(bc_dev->parent_dev,
			"Battery charging state changed (%d -> %d)\n",
			bc_dev->thermal_state, thermal_state_new);

	bc_dev->thermal_state = thermal_state_new;
	mutex_unlock(&bc_dev->mutex);

	switch (thermal_state_new) {
	case CHARGE_THERMAL_COLD_STOP:
	case CHARGE_THERMAL_COOL_STOP:
		charger_enable_state = false;
		charger_current_half = false;
		battery_thersold_voltage = bc_dev->thermal_prop.cool_voltage_mv;
		break;
	case CHARGE_THERMAL_COOL:
		charger_enable_state = true;
		charger_current_half =
				!bc_dev->thermal_prop.disable_cool_current_half;
		battery_thersold_voltage = bc_dev->thermal_prop.cool_voltage_mv;
		break;
	case CHARGE_THERMAL_WARM:
		charger_enable_state = true;
		charger_current_half =
				!bc_dev->thermal_prop.disable_warm_current_half;
		battery_thersold_voltage = bc_dev->thermal_prop.warm_voltage_mv;
		break;
	case CHARGE_THERMAL_WARM_STOP:
	case CHARGE_THERMAL_HOT_STOP:
		charger_enable_state = false;
		charger_current_half = false;
		battery_thersold_voltage = bc_dev->thermal_prop.warm_voltage_mv;
		break;
	case CHARGE_THERMAL_NORMAL:
	default:
		charger_enable_state = true;
		charger_current_half = false;
		battery_thersold_voltage =
			bc_dev->thermal_prop.regulation_voltage_mv;
	}

	if (bc_dev->ops->thermal_configure)
		bc_dev->ops->thermal_configure(bc_dev, temperature,
			charger_enable_state, charger_current_half,
			battery_thersold_voltage);

	return 0;
}

static void battery_charger_thermal_monitor_wq(struct work_struct *work)
{
	struct battery_charger_dev *bc_dev;
	int ret;

	bc_dev = container_of(work, struct battery_charger_dev,
					poll_temp_monitor_wq.work);

	ret = battery_charger_thermal_monitor_func(bc_dev);

	mutex_lock(&bc_dev->mutex);
	if (!ret && bc_dev->start_monitoring == MONITOR_THERMAL)
		schedule_delayed_work(&bc_dev->poll_temp_monitor_wq,
			msecs_to_jiffies(bc_dev->polling_time_sec * 1000));
	mutex_unlock(&bc_dev->mutex);
}

static inline bool is_charge_thermal_normal(struct battery_charger_dev *bc_dev)
{
	switch (bc_dev->thermal_state) {
	case CHARGE_THERMAL_COLD_STOP:
	case CHARGE_THERMAL_COOL_STOP:
	case CHARGE_THERMAL_WARM_STOP:
	case CHARGE_THERMAL_HOT_STOP:
		return false;
	case CHARGE_THERMAL_COOL:
		if (bc_dev->thermal_prop.cool_voltage_mv !=
			bc_dev->thermal_prop.regulation_voltage_mv)
			return false;
		break;
	case CHARGE_THERMAL_WARM:
		if (bc_dev->thermal_prop.warm_voltage_mv !=
			bc_dev->thermal_prop.regulation_voltage_mv)
			return false;
		break;
	case CHARGE_THERMAL_START:
	case CHARGE_THERMAL_NORMAL:
	default:
		/* normal conditon, do nothing */
		break;
	};

	return true;
}

static int battery_charger_batt_status_monitor_func(
					struct battery_charger_dev *bc_dev)
{
	bool chg_full_done_check_match = false;
	bool chg_full_done, chg_full_stop;
	bool is_thermal_normal;
	ktime_t timeout, cur_boottime;
	int volt, curr, level;
	int ret = 0;

	mutex_lock(&bc_dev->mutex);
	ret  = psy_get_property(bc_dev->psy, POWER_SUPPLY_PROP_VOLTAGE_NOW,
			&volt);
	if (ret) {
		dev_err(bc_dev->parent_dev,
			"POWER_SUPPLY_PROP_VOLTAGE_NOW read failed: %d\n ",
			ret);
		goto error;
	}

	ret  = psy_get_property(bc_dev->psy, POWER_SUPPLY_PROP_CURRENT_NOW,
			&curr);
	if (ret) {
		dev_err(bc_dev->parent_dev,
			"POWER_SUPPLY_PROP_CURRENT_NOW read failed: %d\n ",
			ret);
		goto error;
	}

	ret  = psy_get_property(bc_dev->psy, POWER_SUPPLY_PROP_CAPACITY,
			&level);
	if (ret) {
		dev_err(bc_dev->parent_dev,
			"POWER_SUPPLY_PROP_CAPACITY read failed: %d\n ",
			ret);
		goto error;
	}

	volt /= 1000;
	curr /= 1000;

	cur_boottime = ktime_get_boottime();
	is_thermal_normal = is_charge_thermal_normal(bc_dev);
	if (level < CHARGING_FULL_DONE_LEVEL_MIN ||
			(!bc_dev->chg_full_done && !is_thermal_normal)) {
		bc_dev->chg_full_done_prev_check_count = 0;
		bc_dev->chg_full_done = false;
		bc_dev->chg_full_stop = false;
		goto done;
	}

	if (!bc_dev->chg_full_done &&
		volt >= bc_dev->full_thr.chg_done_voltage_min_mv &&
		curr >= CHARGING_CURRENT_0_TORRENCE &&
		level > CHARGING_FULL_DONE_LEVEL_MIN) {
		if (bc_dev->in_current_limit <
			bc_dev->full_thr.chg_done_current_min_ma
			+ CHARGING_FULL_DONE_CURRENT_THRESHOLD_TORRENCE) {
			if (curr <
				bc_dev->full_thr.chg_done_low_current_min_ma)
				chg_full_done_check_match = true;
		} else {
			if (curr <
				bc_dev->full_thr.chg_done_current_min_ma)
				chg_full_done_check_match = true;
		}

		if (!chg_full_done_check_match)
			bc_dev->chg_full_done_prev_check_count = 0;
		else {
			bc_dev->chg_full_done_prev_check_count++;
			if (bc_dev->chg_full_done_prev_check_count >=
					CHARGING_FULL_DONE_CHECK_TIMES) {
				bc_dev->chg_full_done = true;
				timeout = ktime_set(
					CHARGING_FULL_DONE_TIMEOUT_S, 0);
				bc_dev->chg_full_stop_expire_time =
					ktime_add(cur_boottime, timeout);
			}
		}
	}

	if (!bc_dev->chg_full_stop) {
		if (bc_dev->chg_full_done &&
			volt >= bc_dev->full_thr.recharge_voltage_min_mv) {
			if (ktime_compare(bc_dev->chg_full_stop_expire_time,
						cur_boottime) <= 0) {
				dev_info(bc_dev->parent_dev,
					"Charging full stop timeout\n");
				bc_dev->chg_full_stop = true;
			}
		}
	} else {
		if (volt < bc_dev->full_thr.recharge_voltage_min_mv) {
			dev_info(bc_dev->parent_dev,
					"Charging full recharging\n");
			bc_dev->chg_full_stop = false;
			timeout = ktime_set(
					CHARGING_FULL_DONE_TIMEOUT_S, 0);
			bc_dev->chg_full_stop_expire_time =
				ktime_add(cur_boottime, timeout);
		}
	}

done:
	chg_full_done = bc_dev->chg_full_done;
	chg_full_stop = bc_dev->chg_full_stop;
	bc_dev->batt_status_last_check_time = cur_boottime;
	mutex_unlock(&bc_dev->mutex);

	if (bc_dev->ops->charging_full_configure)
		bc_dev->ops->charging_full_configure(bc_dev,
				chg_full_done, chg_full_stop);

	return 0;
error:
	mutex_unlock(&bc_dev->mutex);
	return ret;
}

static int battery_charger_input_voltage_adjust_func(
					struct battery_charger_dev *bc_dev)
{
	int ret;
	int batt_volt, input_volt_min;

	mutex_lock(&bc_dev->mutex);
	ret  = psy_get_property(bc_dev->psy, POWER_SUPPLY_PROP_VOLTAGE_NOW,
			&batt_volt);
	if (ret) {
		dev_err(bc_dev->parent_dev,
			"POWER_SUPPLY_PROP_VOLTAGE_NOW read failed: %d\n ",
			ret);
		mutex_unlock(&bc_dev->mutex);
		return -EINVAL;
	}
	batt_volt /= 1000;

	if (batt_volt > bc_dev->input_switch.input_switch_threshold_mv)
		input_volt_min = bc_dev->input_switch.input_vmin_high_mv;
	else
		input_volt_min = bc_dev->input_switch.input_vmin_low_mv;
	mutex_unlock(&bc_dev->mutex);

	if (bc_dev->ops->input_voltage_configure)
		bc_dev->ops->input_voltage_configure(bc_dev, input_volt_min);

	return 0;
}

static void battery_charger_batt_status_monitor_wq(struct work_struct *work)
{
	struct battery_charger_dev *bc_dev;
	int ret;
	bool keep_monitor = false;

	bc_dev = container_of(work, struct battery_charger_dev,
					poll_batt_status_monitor_wq.work);

	if (!bc_dev->psy) {
		bc_dev->psy = power_supply_get_by_name(bc_dev->gauge_psy_name);

		if (!bc_dev->psy) {
			dev_warn(bc_dev->parent_dev, "Cannot get power_supply:%s\n",
					bc_dev->gauge_psy_name);
			keep_monitor = true;
			goto retry;
		}
	}

	ret = battery_charger_thermal_monitor_func(bc_dev);
	if (!ret)
		keep_monitor = true;

	ret = battery_charger_input_voltage_adjust_func(bc_dev);
	if (!ret)
		keep_monitor = true;

	ret = battery_charger_batt_status_monitor_func(bc_dev);
	if (!ret)
		keep_monitor = true;

retry:
	mutex_lock(&bc_dev->mutex);
	if (keep_monitor && bc_dev->start_monitoring == MONITOR_BATT_STATUS)
		schedule_delayed_work(&bc_dev->poll_batt_status_monitor_wq,
			msecs_to_jiffies(bc_dev->polling_time_sec * 1000));
	mutex_unlock(&bc_dev->mutex);
}

int battery_charger_set_current_broadcast(struct battery_charger_dev *bc_dev)
{
        struct battery_gauge_dev *bg_dev;
        int ret = 0;

        if (!bc_dev) {
                return -EINVAL;
        }

        mutex_lock(&charger_gauge_list_mutex);

        list_for_each_entry(bg_dev, &gauge_list, list) {
                if (bg_dev->cell_id != bc_dev->cell_id)
                        continue;
                if (bg_dev->ops && bg_dev->ops->set_current_broadcast)
                        ret = bg_dev->ops->set_current_broadcast(bg_dev);
        }

        mutex_unlock(&charger_gauge_list_mutex);
        return ret;
}
EXPORT_SYMBOL_GPL(battery_charger_set_current_broadcast);

int battery_charger_thermal_start_monitoring(
	struct battery_charger_dev *bc_dev)
{
	if (!bc_dev || !bc_dev->polling_time_sec || (!bc_dev->tz_name
			&& !bc_dev->batt_info_ops.get_battery_temp))
		return -EINVAL;

	mutex_lock(&bc_dev->mutex);
	if (bc_dev->start_monitoring == MONITOR_WAIT) {
		bc_dev->start_monitoring = MONITOR_THERMAL;
		bc_dev->thermal_state = CHARGE_THERMAL_START;
		bc_dev->batt_status_last_check_time = ktime_get_boottime();
		schedule_delayed_work(&bc_dev->poll_temp_monitor_wq,
				msecs_to_jiffies(1000));
	}
	mutex_unlock(&bc_dev->mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charger_thermal_start_monitoring);

int battery_charger_thermal_stop_monitoring(
	struct battery_charger_dev *bc_dev)
{
	if (!bc_dev || !bc_dev->polling_time_sec || (!bc_dev->tz_name
		&& !bc_dev->batt_info_ops.get_battery_temp))
		return -EINVAL;

	mutex_lock(&bc_dev->mutex);
	if (bc_dev->start_monitoring == MONITOR_THERMAL) {
		bc_dev->start_monitoring = MONITOR_WAIT;
		cancel_delayed_work(&bc_dev->poll_temp_monitor_wq);
	}
	mutex_unlock(&bc_dev->mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charger_thermal_stop_monitoring);

int battery_charger_batt_status_start_monitoring(
	struct battery_charger_dev *bc_dev,
	int in_current_limit)
{
	if (!bc_dev || !bc_dev->polling_time_sec || (!bc_dev->tz_name
			&& !bc_dev->batt_info_ops.get_battery_temp
			&& !bc_dev->gauge_psy_name))
		return -EINVAL;

	mutex_lock(&bc_dev->mutex);
	if (bc_dev->start_monitoring == MONITOR_WAIT) {
		bc_dev->chg_full_stop = false;
		bc_dev->chg_full_done = false;
		bc_dev->chg_full_done_prev_check_count = 0;
		bc_dev->in_current_limit = in_current_limit;
		bc_dev->start_monitoring = MONITOR_BATT_STATUS;
		bc_dev->thermal_state = CHARGE_THERMAL_START;
		schedule_delayed_work(&bc_dev->poll_batt_status_monitor_wq,
				msecs_to_jiffies(1000));
	}
	mutex_unlock(&bc_dev->mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charger_batt_status_start_monitoring);

int battery_charger_batt_status_stop_monitoring(
	struct battery_charger_dev *bc_dev)
{
	if (!bc_dev || !bc_dev->polling_time_sec || (!bc_dev->tz_name
			&& !bc_dev->batt_info_ops.get_battery_temp
			&& !bc_dev->gauge_psy_name))
		return -EINVAL;

	mutex_lock(&bc_dev->mutex);
	if (bc_dev->start_monitoring == MONITOR_BATT_STATUS) {
		bc_dev->start_monitoring = MONITOR_WAIT;
		cancel_delayed_work(&bc_dev->poll_batt_status_monitor_wq);
	}
	mutex_unlock(&bc_dev->mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charger_batt_status_stop_monitoring);

int battery_charger_batt_status_force_check(
	struct battery_charger_dev *bc_dev)
{
	if (!bc_dev)
		return -EINVAL;

	mutex_lock(&bc_dev->mutex);
	if (bc_dev->start_monitoring == MONITOR_BATT_STATUS) {
		cancel_delayed_work(&bc_dev->poll_batt_status_monitor_wq);
		schedule_delayed_work(&bc_dev->poll_batt_status_monitor_wq, 0);
	}
	mutex_unlock(&bc_dev->mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charger_batt_status_monitor_force_check);

int battery_charger_get_batt_status_no_update_time_ms(
	struct battery_charger_dev *bc_dev, s64 *time)
{
	int ret = 0;
	ktime_t cur_boottime;
	s64 delta;

	if (!bc_dev || !time)
		return -EINVAL;

	mutex_lock(&bc_dev->mutex);
	if (bc_dev->start_monitoring != MONITOR_BATT_STATUS)
		ret = -ENODEV;
	else {
		cur_boottime = ktime_get_boottime();
		if (ktime_compare(cur_boottime,
				bc_dev->batt_status_last_check_time) <= 0)
			*time = 0;
		else {
			delta = ktime_us_delta(
					cur_boottime,
					bc_dev->batt_status_last_check_time);
			do_div(delta, 1000);
			*time = delta;
		}
	}
	mutex_unlock(&bc_dev->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(battery_charger_get_batt_status_no_update_time_ms);

int battery_charger_acquire_wake_lock(struct battery_charger_dev *bc_dev)
{
	if (!bc_dev->locked) {
		wake_lock(&bc_dev->charger_wake_lock);
		bc_dev->locked = true;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charger_acquire_wake_lock);

int battery_charger_release_wake_lock(struct battery_charger_dev *bc_dev)
{
	if (bc_dev->locked) {
		wake_unlock(&bc_dev->charger_wake_lock);
		bc_dev->locked = false;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charger_release_wake_lock);

int battery_charging_restart(struct battery_charger_dev *bc_dev, int after_sec)
{
	if (!bc_dev->ops->restart_charging) {
		dev_err(bc_dev->parent_dev,
			"No callback for restart charging\n");
		return -EINVAL;
	}
	schedule_delayed_work(&bc_dev->restart_charging_wq,
			msecs_to_jiffies(after_sec * HZ));
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charging_restart);

static ssize_t battery_show_snapshot_voltage(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct battery_gauge_dev *bg_dev = bg_temp;

	return snprintf(buf, MAX_STR_PRINT, "%d\n",
				bg_dev->battery_snapshot_voltage);
}

static ssize_t battery_show_snapshot_current(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct battery_gauge_dev *bg_dev = bg_temp;

	return snprintf(buf, MAX_STR_PRINT, "%d\n",
				bg_dev->battery_snapshot_current);
}

static ssize_t battery_show_snapshot_capacity(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct battery_gauge_dev *bg_dev = bg_temp;

	return snprintf(buf, MAX_STR_PRINT, "%d\n",
				bg_dev->battery_snapshot_capacity);
}

static ssize_t battery_show_max_capacity(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct iio_channel *channel;
	int val, ret;

	channel = iio_channel_get(dev, "batt_id");
	if (IS_ERR(channel)) {
		dev_err(dev,
			"%s: Failed to get channel batt_id, %ld\n",
			__func__, PTR_ERR(channel));
			return 0;
	}

	ret = iio_read_channel_raw(channel, &val);
	if (ret < 0) {
		dev_err(dev,
			"%s: Failed to read channel, %d\n",
			__func__, ret);
			return 0;
	}

	return snprintf(buf, MAX_STR_PRINT, "%d\n", val);
}

static DEVICE_ATTR(battery_snapshot_voltage, S_IRUGO,
		battery_show_snapshot_voltage, NULL);

static DEVICE_ATTR(battery_snapshot_current, S_IRUGO,
		battery_show_snapshot_current, NULL);

static DEVICE_ATTR(battery_snapshot_capacity, S_IRUGO,
		battery_show_snapshot_capacity, NULL);

static DEVICE_ATTR(battery_max_capacity, S_IRUGO,
		battery_show_max_capacity, NULL);

static struct attribute *battery_snapshot_attributes[] = {
	&dev_attr_battery_snapshot_voltage.attr,
	&dev_attr_battery_snapshot_current.attr,
	&dev_attr_battery_snapshot_capacity.attr,
	&dev_attr_battery_max_capacity.attr,
	NULL
};

static const struct attribute_group battery_snapshot_attr_group = {
	.attrs = battery_snapshot_attributes,
};

int battery_gauge_record_voltage_value(struct battery_gauge_dev *bg_dev,
							int voltage)
{
	if (!bg_dev)
		return -EINVAL;

	bg_dev->battery_voltage = voltage;

	return 0;
}
EXPORT_SYMBOL_GPL(battery_gauge_record_voltage_value);

int battery_gauge_record_current_value(struct battery_gauge_dev *bg_dev,
						int battery_current)
{
	if (!bg_dev)
		return -EINVAL;

	bg_dev->battery_current = battery_current;

	return 0;
}
EXPORT_SYMBOL_GPL(battery_gauge_record_current_value);

int battery_gauge_record_capacity_value(struct battery_gauge_dev *bg_dev,
							int capacity)
{
	if (!bg_dev)
		return -EINVAL;

	bg_dev->battery_capacity = capacity;

	return 0;
}
EXPORT_SYMBOL_GPL(battery_gauge_record_capacity_value);

int battery_gauge_record_snapshot_values(struct battery_gauge_dev *bg_dev,
						int interval)
{
	msleep(interval);
	if (!bg_dev)
		return -EINVAL;

	bg_dev->battery_snapshot_voltage = bg_dev->battery_voltage;
	bg_dev->battery_snapshot_current = bg_dev->battery_current;
	bg_dev->battery_snapshot_capacity = bg_dev->battery_capacity;

	return 0;
}
EXPORT_SYMBOL_GPL(battery_gauge_record_snapshot_values);

int battery_gauge_get_scaled_soc(struct battery_gauge_dev *bg_dev,
	int actual_soc_semi, int thresod_soc)
{
	int thresod_soc_semi = thresod_soc * 100;

	if (actual_soc_semi >= 10000)
		return 100;

	if (actual_soc_semi <= thresod_soc_semi)
		return 0;

	return (actual_soc_semi - thresod_soc_semi) / (100 - thresod_soc);
}
EXPORT_SYMBOL_GPL(battery_gauge_get_scaled_soc);

int battery_gauge_get_adjusted_soc(struct battery_gauge_dev *bg_dev,
	int min_soc, int max_soc, int actual_soc_semi)
{
	int min_soc_semi = min_soc * 100;
	int max_soc_semi = max_soc * 100;

	if (actual_soc_semi >= max_soc_semi)
		return 100;

	if (actual_soc_semi <= min_soc_semi)
		return 0;

	return (actual_soc_semi - min_soc_semi + 50) / (max_soc - min_soc);
}
EXPORT_SYMBOL_GPL(battery_gauge_get_adjusted_soc);

void battery_charging_restart_cancel(struct battery_charger_dev *bc_dev)
{
	if (!bc_dev->ops->restart_charging) {
		dev_err(bc_dev->parent_dev,
			"No callback for restart charging\n");
		return;
	}
	cancel_delayed_work(&bc_dev->restart_charging_wq);
}
EXPORT_SYMBOL_GPL(battery_charging_restart_cancel);

int battery_charging_wakeup(struct battery_charger_dev *bc_dev, int after_sec)
{
	int ret;
	unsigned long now;
	struct rtc_wkalrm alm;
	int alarm_time = after_sec;

	if (!alarm_time)
		return 0;

	bc_dev->rtc = alarmtimer_get_rtcdev();
	if (!bc_dev->rtc) {
		dev_err(bc_dev->parent_dev, "No RTC device found\n");
		return -ENODEV;
	}

	alm.enabled = true;
	ret = rtc_read_time(bc_dev->rtc, &alm.time);
	if (ret < 0) {
		dev_err(bc_dev->parent_dev, "RTC read time failed %d\n", ret);
		return ret;
	}
	rtc_tm_to_time(&alm.time, &now);

	rtc_time_to_tm(now + alarm_time, &alm.time);
	ret = rtc_set_alarm(bc_dev->rtc, &alm);
	if (ret < 0) {
		dev_err(bc_dev->parent_dev, "RTC set alarm failed %d\n", ret);
		alm.enabled = false;
		return ret;
	}
	alm.enabled = false;
	return 0;
}
EXPORT_SYMBOL_GPL(battery_charging_wakeup);

int battery_charging_system_reset_after(struct battery_charger_dev *bc_dev,
	int after_sec)
{
	struct system_pmic_rtc_data rtc_data;
	int ret;

	dev_info(bc_dev->parent_dev, "Setting system on after %d sec\n",
		after_sec);
	battery_charging_wakeup(bc_dev, after_sec);
	rtc_data.power_on_after_sec = after_sec;

	ret = system_pmic_set_power_on_event(SYSTEM_PMIC_RTC_ALARM, &rtc_data);
	if (ret < 0)
		dev_err(bc_dev->parent_dev,
			"Setting power on event failed: %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(battery_charging_system_reset_after);

int battery_charging_system_power_on_usb_event(
	struct battery_charger_dev *bc_dev)
{
	int ret;

	dev_info(bc_dev->parent_dev,
		"Setting system on with USB connect/disconnect\n");

	ret = system_pmic_set_power_on_event(SYSTEM_PMIC_USB_VBUS_INSERTION,
		NULL);
	if (ret < 0)
		dev_err(bc_dev->parent_dev,
			"Setting power on event failed: %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(battery_charging_system_power_on_usb_event);

static void battery_charger_unknown_batt_id_work(struct work_struct *work)
{
	struct battery_charger_dev *bc_dev;
	int batt_id = 0;
	struct iio_channel *batt_id_channel;
	int ret;

	bc_dev = container_of(work, struct battery_charger_dev,
					unknown_batt_id_work.work);

	batt_id_channel = iio_channel_get(NULL, bc_dev->batt_id_channel_name);
	if (IS_ERR(batt_id_channel)) {
		if (bc_dev->unknown_batt_id_check_count > 0) {
			bc_dev->unknown_batt_id_check_count--;
			schedule_delayed_work(&bc_dev->unknown_batt_id_work,
				UNKNOWN_BATTERY_ID_CHECK_DELAY);
		} else
			dev_err(bc_dev->parent_dev,
					"Failed to get iio channel %s, %ld\n",
					bc_dev->batt_id_channel_name,
					PTR_ERR(batt_id_channel));
	} else {
		ret = iio_read_channel_processed(batt_id_channel, &batt_id);
		if (ret < 0)
			ret = iio_read_channel_raw(batt_id_channel, &batt_id);

		if (ret < 0) {
			dev_err(bc_dev->parent_dev,
				"Failed to read batt id, ret=%d\n",
				ret);
			return;
		}


		dev_info(bc_dev->parent_dev,
				"Battery id adc value is %d\n", batt_id);
		if (batt_id > bc_dev->unknown_batt_id_min) {
			dev_info(bc_dev->parent_dev,
				"Unknown battery detected(%d), no charging!\n",
				batt_id);

			if (bc_dev->ops->unknown_battery_handle)
				bc_dev->ops->unknown_battery_handle(bc_dev);
		}
	}
}

static void battery_charger_thermal_prop_init(
	struct battery_charger_dev *bc_dev,
	struct battery_thermal_prop thermal_prop)
{
	if (!bc_dev)
		return;

	if (thermal_prop.temp_hot_dc >= thermal_prop.temp_warm_dc
		&& thermal_prop.temp_warm_dc > thermal_prop.temp_cool_dc
		&& thermal_prop.temp_cool_dc >= thermal_prop.temp_cold_dc) {
		bc_dev->thermal_prop.temp_hot_dc = thermal_prop.temp_hot_dc;
		bc_dev->thermal_prop.temp_cold_dc = thermal_prop.temp_cold_dc;
		bc_dev->thermal_prop.temp_warm_dc = thermal_prop.temp_warm_dc;
		bc_dev->thermal_prop.temp_cool_dc = thermal_prop.temp_cool_dc;
		bc_dev->thermal_prop.temp_hysteresis_dc =
			thermal_prop.temp_hysteresis_dc;
	} else {
		bc_dev->thermal_prop.temp_hot_dc = JETI_TEMP_HOT;
		bc_dev->thermal_prop.temp_cold_dc = JETI_TEMP_COLD;
		bc_dev->thermal_prop.temp_warm_dc = JETI_TEMP_WARM;
		bc_dev->thermal_prop.temp_cool_dc = JETI_TEMP_COOL;
		bc_dev->thermal_prop.temp_hysteresis_dc = 0;
	}

	bc_dev->thermal_prop.regulation_voltage_mv =
		thermal_prop.regulation_voltage_mv ?:
		DEFAULT_BATTERY_REGULATION_VOLTAGE;

	bc_dev->thermal_prop.warm_voltage_mv =
		thermal_prop.warm_voltage_mv ?:
		DEFAULT_BATTERY_THERMAL_VOLTAGE;

	bc_dev->thermal_prop.cool_voltage_mv =
		thermal_prop.cool_voltage_mv ?:
		DEFAULT_BATTERY_THERMAL_VOLTAGE;

	bc_dev->thermal_prop.disable_warm_current_half =
		thermal_prop.disable_warm_current_half;

	bc_dev->thermal_prop.disable_cool_current_half =
		thermal_prop.disable_cool_current_half;
}

static void battery_charger_charge_full_threshold_init(
	struct battery_charger_dev *bc_dev,
	struct charge_full_threshold full_thr)
{
	if (!bc_dev)
		return;

	bc_dev->full_thr.chg_done_voltage_min_mv =
		full_thr.chg_done_voltage_min_mv ?:
		DEFAULT_BATTERY_REGULATION_VOLTAGE - 102;

	bc_dev->full_thr.chg_done_current_min_ma =
		full_thr.chg_done_current_min_ma ?:
		DEFAULT_CHARGE_DONE_CURRENT;

	bc_dev->full_thr.chg_done_low_current_min_ma =
		full_thr.chg_done_low_current_min_ma ?:
		DEFAULT_CHARGE_DONE_LOW_CURRENT;

	bc_dev->full_thr.recharge_voltage_min_mv =
		full_thr.recharge_voltage_min_mv ?:
		DEFAULT_BATTERY_REGULATION_VOLTAGE - 48;
}

static void battery_charger_charge_input_switch_init(
	struct battery_charger_dev *bc_dev,
	struct charge_input_switch input_switch)
{
	if (!bc_dev)
		return;

	bc_dev->input_switch.input_vmin_high_mv =
			input_switch.input_vmin_high_mv ?:
			DEFAULT_INPUT_VMIN_MV;
	bc_dev->input_switch.input_vmin_low_mv =
			input_switch.input_vmin_low_mv ?:
			DEFAULT_INPUT_VMIN_MV;
	bc_dev->input_switch.input_switch_threshold_mv =
			input_switch.input_switch_threshold_mv;
}

struct battery_charger_dev *battery_charger_register(struct device *dev,
	struct battery_charger_info *bci, void *drv_data)
{
	struct battery_charger_dev *bc_dev;
	struct battery_gauge_dev *bg_dev;

	dev_info(dev, "Registering battery charger driver\n");

	if (!dev || !bci) {
		dev_err(dev, "Invalid parameters\n");
		return ERR_PTR(-EINVAL);
	}

	bc_dev = kzalloc(sizeof(*bc_dev), GFP_KERNEL);
	if (!bc_dev) {
		dev_err(dev, "Memory alloc for bc_dev failed\n");
		return ERR_PTR(-ENOMEM);
	}

	mutex_lock(&charger_gauge_list_mutex);

	INIT_LIST_HEAD(&bc_dev->list);
	bc_dev->cell_id = bci->cell_id;
	bc_dev->ops = bci->bc_ops;
	bc_dev->parent_dev = dev;
	bc_dev->drv_data = drv_data;

	mutex_init(&bc_dev->mutex);

	/* Thermal monitoring */
	if (bci->tz_name)
		bc_dev->tz_name = kstrdup(bci->tz_name, GFP_KERNEL);

	list_for_each_entry(bg_dev, &gauge_list, list) {
		if (bg_dev->cell_id != bc_dev->cell_id)
			continue;
		if (bg_dev->ops && bg_dev->ops->get_battery_temp) {
			bc_dev->batt_info_ops.get_battery_temp =
				bg_dev->ops->get_battery_temp;
			break;
		}
	}

	if (bci->tz_name || bci->enable_thermal_monitor
				|| bci->enable_batt_status_monitor) {
		bc_dev->polling_time_sec = bci->polling_time_sec;
		if (!bci->enable_batt_status_monitor) {
			bc_dev->enable_thermal_monitor = true;
			INIT_DELAYED_WORK(&bc_dev->poll_temp_monitor_wq,
					battery_charger_thermal_monitor_wq);
		} else {
			bc_dev->enable_batt_status_monitor = true;
			INIT_DELAYED_WORK(&bc_dev->poll_batt_status_monitor_wq,
					battery_charger_batt_status_monitor_wq);
		}
	}

	battery_charger_thermal_prop_init(bc_dev, bci->thermal_prop);
	battery_charger_charge_full_threshold_init(bc_dev, bci->full_thr);
	battery_charger_charge_input_switch_init(bc_dev, bci->input_switch);

	INIT_DELAYED_WORK(&bc_dev->restart_charging_wq,
			battery_charger_restart_charging_wq);

	wake_lock_init(&bc_dev->charger_wake_lock, WAKE_LOCK_SUSPEND,
						"charger-suspend-lock");
	list_add(&bc_dev->list, &charger_list);
	mutex_unlock(&charger_gauge_list_mutex);

	if (bci->batt_id_channel_name && bci->unknown_batt_id_min > 0) {
		bc_dev->batt_id_channel_name =
				kstrdup(bci->batt_id_channel_name, GFP_KERNEL);
		if (bc_dev->batt_id_channel_name) {
			bc_dev->unknown_batt_id_min = bci->unknown_batt_id_min;
			bc_dev->unknown_batt_id_check_count =
					UNKNOWN_BATTERY_ID_CHECK_COUNT;
			INIT_DEFERRABLE_WORK(&bc_dev->unknown_batt_id_work,
					battery_charger_unknown_batt_id_work);
			schedule_delayed_work(&bc_dev->unknown_batt_id_work, 0);
		} else
			dev_err(dev,
				"Failed to duplicate batt id channel name\n");
	}

	bc_dev->gauge_psy_name = kstrdup(bci->gauge_psy_name, GFP_KERNEL);
	if (!bc_dev->gauge_psy_name)
		dev_err(dev, "Failed to duplicate gauge power_supply name\n");

	return bc_dev;
}
EXPORT_SYMBOL_GPL(battery_charger_register);

void battery_charger_unregister(struct battery_charger_dev *bc_dev)
{
	mutex_lock(&charger_gauge_list_mutex);
	list_del(&bc_dev->list);
	if (bc_dev->enable_thermal_monitor)
		cancel_delayed_work(&bc_dev->poll_temp_monitor_wq);
	if (bc_dev->enable_batt_status_monitor)
		cancel_delayed_work(&bc_dev->poll_batt_status_monitor_wq);
	if (bc_dev->batt_id_channel_name && bc_dev->unknown_batt_id_min > 0)
		cancel_delayed_work(&bc_dev->unknown_batt_id_work);
	cancel_delayed_work(&bc_dev->restart_charging_wq);
	wake_lock_destroy(&bc_dev->charger_wake_lock);
	mutex_unlock(&charger_gauge_list_mutex);
	kfree(bc_dev);
}
EXPORT_SYMBOL_GPL(battery_charger_unregister);

int battery_gauge_get_battery_temperature(struct battery_gauge_dev *bg_dev,
	int *temp)
{
	int ret;
	long temperature;

	if (!bg_dev || !bg_dev->tz_name)
		return -EINVAL;

	if (bg_dev->tz_name) {
		if (!bg_dev->battery_tz)
			bg_dev->battery_tz =
				thermal_zone_device_find_by_name(
					bg_dev->tz_name);

		if (!bg_dev->battery_tz) {
			dev_info(bg_dev->parent_dev,
					"Battery thermal zone %s is not registered yet\n",
					bg_dev->tz_name);
			return -ENODEV;
		}

		ret = thermal_zone_get_temp(bg_dev->battery_tz, &temperature);
		if (ret < 0)
			return ret;

		*temp = temperature / 1000;
	} else if (bg_dev->ops->get_battery_temp) {
		temperature = bg_dev->ops->get_battery_temp();
		*temp = temperature / 10;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(battery_gauge_get_battery_temperature);

int battery_gauge_get_battery_current(struct battery_gauge_dev *bg_dev,
	int *current_ma)
{
	int ret;

	if (!bg_dev || !bg_dev->bat_curr_channel_name)
		return -EINVAL;

	if (!bg_dev->bat_current_iio_channel)
		bg_dev->bat_current_iio_channel =
			iio_channel_get(bg_dev->parent_dev,
					bg_dev->bat_curr_channel_name);
	if (!bg_dev->bat_current_iio_channel || IS_ERR(bg_dev->bat_current_iio_channel)) {
		dev_info(bg_dev->parent_dev,
			"Battery IIO current channel %s not registered yet\n",
			bg_dev->bat_curr_channel_name);
		bg_dev->bat_current_iio_channel = NULL;
		return -ENODEV;
	}

	ret = iio_read_channel_processed(bg_dev->bat_current_iio_channel,
			current_ma);
	if (ret < 0) {
		dev_err(bg_dev->parent_dev, " The channel read failed: %d\n", ret);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(battery_gauge_get_battery_current);

struct battery_gauge_dev *battery_gauge_register(struct device *dev,
	struct battery_gauge_info *bgi, void *drv_data)
{
	struct battery_gauge_dev *bg_dev;
	struct battery_charger_dev *bc_dev;
	int ret;

	dev_info(dev, "Registering battery gauge driver\n");

	if (!dev || !bgi) {
		dev_err(dev, "Invalid parameters\n");
		return ERR_PTR(-EINVAL);
	}

	bg_dev = kzalloc(sizeof(*bg_dev), GFP_KERNEL);
	if (!bg_dev) {
		dev_err(dev, "Memory alloc for bg_dev failed\n");
		return ERR_PTR(-ENOMEM);
	}

	ret = sysfs_create_group(&dev->kobj, &battery_snapshot_attr_group);
	if (ret < 0)
		dev_info(dev, "Could not create battery snapshot sysfs group\n");

	mutex_lock(&charger_gauge_list_mutex);

	INIT_LIST_HEAD(&bg_dev->list);
	bg_dev->cell_id = bgi->cell_id;
	bg_dev->ops = bgi->bg_ops;
	bg_dev->parent_dev = dev;
	bg_dev->drv_data = drv_data;
	bg_dev->tz_name = NULL;

	if (bgi->current_channel_name)
		bg_dev->bat_curr_channel_name = bgi->current_channel_name;

	if (bgi->tz_name) {
		bg_dev->tz_name = kstrdup(bgi->tz_name, GFP_KERNEL);
		bg_dev->battery_tz = thermal_zone_device_find_by_name(
			bg_dev->tz_name);
		if (!bg_dev->battery_tz)
			dev_info(dev,
			"Battery thermal zone %s is not registered yet\n",
			bg_dev->tz_name);
	}

	list_for_each_entry(bc_dev, &charger_list, list) {
		if (bg_dev->cell_id != bc_dev->cell_id)
			continue;
		if (bg_dev->ops && bg_dev->ops->get_battery_temp
				&& !bc_dev->batt_info_ops.get_battery_temp) {
			bc_dev->batt_info_ops.get_battery_temp =
				bg_dev->ops->get_battery_temp;
			break;
		}
	}

	list_add(&bg_dev->list, &gauge_list);
	mutex_unlock(&charger_gauge_list_mutex);
	bg_temp = bg_dev;

	return bg_dev;
}
EXPORT_SYMBOL_GPL(battery_gauge_register);

void battery_gauge_unregister(struct battery_gauge_dev *bg_dev)
{
	mutex_lock(&charger_gauge_list_mutex);
	list_del(&bg_dev->list);
	mutex_unlock(&charger_gauge_list_mutex);
	kfree(bg_dev);
}
EXPORT_SYMBOL_GPL(battery_gauge_unregister);

int battery_charging_status_update(struct battery_charger_dev *bc_dev,
	enum battery_charger_status status)
{
	struct battery_gauge_dev *node;
	int ret = -EINVAL;

	if (!bc_dev) {
		dev_err(bc_dev->parent_dev, "Invalid parameters\n");
		return -EINVAL;
	}

	mutex_lock(&charger_gauge_list_mutex);

	list_for_each_entry(node, &gauge_list, list) {
		if (node->cell_id != bc_dev->cell_id)
			continue;
		if (node->ops && node->ops->update_battery_status)
			ret = node->ops->update_battery_status(node, status);
	}

	mutex_unlock(&charger_gauge_list_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(battery_charging_status_update);

void *battery_charger_get_drvdata(struct battery_charger_dev *bc_dev)
{
	if (bc_dev)
		return bc_dev->drv_data;
	return NULL;
}
EXPORT_SYMBOL_GPL(battery_charger_get_drvdata);

void battery_charger_set_drvdata(struct battery_charger_dev *bc_dev, void *data)
{
	if (bc_dev)
		bc_dev->drv_data = data;
}
EXPORT_SYMBOL_GPL(battery_charger_set_drvdata);

void *battery_gauge_get_drvdata(struct battery_gauge_dev *bg_dev)
{
	if (bg_dev)
		return bg_dev->drv_data;
	return NULL;
}
EXPORT_SYMBOL_GPL(battery_gauge_get_drvdata);

void battery_gauge_set_drvdata(struct battery_gauge_dev *bg_dev, void *data)
{
	if (bg_dev)
		bg_dev->drv_data = data;
}
EXPORT_SYMBOL_GPL(battery_gauge_set_drvdata);
