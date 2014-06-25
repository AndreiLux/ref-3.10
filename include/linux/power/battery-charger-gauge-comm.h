/*
 * battery-charger-gauge-comm.h -- Communication APIS between battery charger
 *		and battery gauge driver.
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

#ifndef _LINUX_POWER_BATTERY_CHARGER_GAUGE_COMM_H
#define _LINUX_POWER_BATTERY_CHARGER_GAUGE_COMM_H

#define BATTERY_SNAPSHOT_INTERVAL	2*HZ

enum battery_charger_status {
	BATTERY_DISCHARGING,
	BATTERY_CHARGING,
	BATTERY_CHARGING_DONE,
	BATTERY_UNKNOWN,
};

enum charge_thermal_state {
	CHARGE_THERMAL_START = 0,
	CHARGE_THERMAL_NORMAL,
	CHARGE_THERMAL_COLD_STOP,
	CHARGE_THERMAL_COOL_STOP,
	CHARGE_THERMAL_COOL,
	CHARGE_THERMAL_WARM,
	CHARGE_THERMAL_WARM_STOP,
	CHARGE_THERMAL_HOT_STOP,
};

struct battery_gauge_dev;
struct battery_charger_dev;

struct battery_gauge_ops {
	int (*update_battery_status)(struct battery_gauge_dev *bg_device,
				enum battery_charger_status status);
	int (*set_current_broadcast) (struct battery_gauge_dev *bg_device);
	int (*get_battery_temp)(void);
};

struct battery_charging_ops {
	int (*get_charging_status)(struct battery_charger_dev *bc_dev);
	int (*restart_charging)(struct battery_charger_dev *bc_dev);
	int (*thermal_configure)(struct battery_charger_dev *bct_dev,
		int temp, bool enable_charger, bool enable_charg_half_current,
		int battery_voltage);
	int (*charging_full_configure)(struct battery_charger_dev *bc_dev,
		bool charge_full_done, bool charge_full_stop);
	int (*input_voltage_configure)(struct battery_charger_dev *bc_dev,
		int voltage_min);
	int (*unknown_battery_handle)(struct battery_charger_dev *bc_dev);
};

struct battery_thermal_prop {
	int temp_hot_dc;
	int temp_cold_dc;
	int temp_warm_dc;
	int temp_cool_dc;
	unsigned int temp_hysteresis_dc;
	unsigned int regulation_voltage_mv;
	unsigned int warm_voltage_mv;
	unsigned int cool_voltage_mv;
	bool disable_warm_current_half;
	bool disable_cool_current_half;
};

struct charge_full_threshold {
	int chg_done_voltage_min_mv;
	int chg_done_current_min_ma;
	int chg_done_low_current_min_ma;
	int recharge_voltage_min_mv;
};

struct charge_input_switch {
	int input_switch_threshold_mv;
	int input_vmin_high_mv;
	int input_vmin_low_mv;
};

struct battery_charger_info {
	const char *tz_name;
	int cell_id;
	int polling_time_sec;
	bool enable_thermal_monitor;
	bool enable_batt_status_monitor;
	struct battery_charging_ops *bc_ops;
	struct battery_thermal_prop thermal_prop;
	struct charge_full_threshold full_thr;
	struct charge_input_switch input_switch;
	const char *batt_id_channel_name;
	int unknown_batt_id_min;
};

struct battery_gauge_info {
	int cell_id;
	const char *tz_name;
	const char *current_channel_name;
	struct battery_gauge_ops *bg_ops;
};

struct battery_charger_dev *battery_charger_register(struct device *dev,
		struct battery_charger_info *bci, void *drv_data);
void battery_charger_unregister(struct battery_charger_dev *bc_dev);
int battery_charging_status_update(struct battery_charger_dev *bc_dev,
		enum battery_charger_status status);
int battery_charging_restart(struct battery_charger_dev *bc_dev, int after_sec);
void battery_charging_restart_cancel(struct battery_charger_dev *bc_dev);
int battery_charger_thermal_start_monitoring(
		struct battery_charger_dev *bc_dev);
int battery_charger_thermal_stop_monitoring(
		struct battery_charger_dev *bc_dev);
int battery_charger_batt_status_start_monitoring(
		struct battery_charger_dev *bc_dev,
		int in_current_limit);
int battery_charger_batt_status_stop_monitoring(
		struct battery_charger_dev *bc_dev);
int battery_charger_batt_status_force_check(
		struct battery_charger_dev *bc_dev);
int battery_charger_get_batt_status_no_update_time_ms(
		struct battery_charger_dev *bc_dev,
		s64 *time);
int battery_charger_acquire_wake_lock(struct battery_charger_dev *bc_dev);
int battery_charger_release_wake_lock(struct battery_charger_dev *bc_dev);

int battery_charging_wakeup(struct battery_charger_dev *bc_dev, int after_sec);
int battery_charging_system_reset_after(struct battery_charger_dev *bc_dev,
	int after_sec);
int battery_charging_system_power_on_usb_event(
	struct battery_charger_dev *bc_dev);
int battery_gauge_get_battery_temperature(struct battery_gauge_dev *bg_dev,
	int *temp);
int battery_gauge_get_battery_current(struct battery_gauge_dev *bg_dev,
	int *current_ma);
int battery_charger_set_current_broadcast(struct battery_charger_dev *bc_dev);
struct battery_gauge_dev *battery_gauge_register(struct device *dev,
		struct battery_gauge_info *bgi, void *drv_data);
void battery_gauge_unregister(struct battery_gauge_dev *bg_dev);

void *battery_charger_get_drvdata(struct battery_charger_dev *bc_dev);
void battery_charger_set_drvdata(struct battery_charger_dev *bc_dev,
			void *data);
void *battery_gauge_get_drvdata(struct battery_gauge_dev *bg_dev);
void battery_gauge_set_drvdata(struct battery_gauge_dev *bg_dev, void *data);
int battery_gauge_record_voltage_value(struct battery_gauge_dev *bg_dev,
								int voltage);
int battery_gauge_record_current_value(struct battery_gauge_dev *bg_dev,
							int batt_current);
int battery_gauge_record_capacity_value(struct battery_gauge_dev *bg_dev,
								int capacity);
int battery_gauge_record_snapshot_values(struct battery_gauge_dev *bg_dev,
								int interval);
int battery_gauge_update_record_to_charger(struct battery_gauge_dev *bg_dev);
int battery_gauge_get_scaled_soc(struct battery_gauge_dev *bg_dev,
		int actual_soc_semi, int thresod_soc);
int battery_gauge_get_adjusted_soc(struct battery_gauge_dev *bg_dev,
		int min_soc, int max_soc, int actual_soc_semi);

#endif /* _LINUX_POWER_BATTERY_CHARGER_GAUGE_COMM_H */
