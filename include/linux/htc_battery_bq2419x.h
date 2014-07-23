/*
 * htc_battery_bq2419x.h -- BQ24190/BQ24192/BQ24192i/BQ24193 Charger policy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA	02110-1301, USA.
 *
 */

#ifndef __HTC_BATTERY_BQ2419X_CHARGER_H
#define __HTC_BATTERY_BQ2419X_CHARGER_H

/*
 * struct bq2419x_thermal_prop - bq1481x thermal properties
 *                               for battery-charger-gauge-comm.
 */
struct htc_battery_thermal_prop {
	int temp_hot_dc;
	int temp_cold_dc;
	int temp_warm_dc;
	int temp_cool_dc;
	unsigned int temp_hysteresis_dc;
	unsigned int warm_voltage_mv;
	unsigned int cool_voltage_mv;
	bool disable_warm_current_half;
	bool disable_cool_current_half;
	unsigned int otp_output_current_ma;
};

/*
 * struct htc_battery_charge_full_threshold -
 * used for charging full/recharge check
 */
struct htc_battery_charge_full_threshold {
	int chg_done_voltage_min_mv;
	int chg_done_current_min_ma;
	int chg_done_low_current_min_ma;
	int recharge_voltage_min_mv;
};

/*
 * struct htc_battery_charge_input_switch - used for adjust input voltage
 */
struct htc_battery_charge_input_switch {
	int input_switch_threshold_mv;
	int input_vmin_high_mv;
	int input_vmin_low_mv;
};

/*
 * struct htc_battery_bq2419x_platform_data - bq2419x platform data.
 */
struct htc_battery_bq2419x_platform_data {
	int input_voltage_limit_mv;
	int fast_charge_current_limit_ma;
	int pre_charge_current_limit_ma;
	int termination_current_limit_ma; /* 0 means disable current check */
	int charge_voltage_limit_mv;
	int max_charge_current_ma;
	int rtc_alarm_time;
	int num_consumer_supplies;
	struct regulator_consumer_supply *consumer_supplies;
	int chg_restart_time;
	int auto_recharge_time_power_off;
	bool disable_suspend_during_charging;
	int charge_suspend_polling_time_sec;
	int temp_polling_time_sec;
	u32 auto_recharge_time_supend;
	struct htc_battery_thermal_prop thermal_prop;
	struct htc_battery_charge_full_threshold full_thr;
	struct htc_battery_charge_input_switch input_switch;
	const char *batt_id_channel_name;
	int unknown_batt_id_min;
	const char *gauge_psy_name;
};

struct htc_battery_bq2419x_ops {
	int (*set_charger_enable)(bool enable, void *data);
	int (*set_charger_hiz)(bool is_hiz, void *data);
	int (*set_fastcharge_current)(unsigned int current_ma, void *data);
	int (*set_charge_voltage)(unsigned int voltage_mv, void *data);
	int (*set_precharge_current)(unsigned int current_ma, void *data);
	int (*set_termination_current)(
		unsigned int current_ma, void *data); /* 0 means disable */
	int (*set_input_current)(unsigned int current_ma, void *data);
	int (*set_dpm_input_voltage)(unsigned int voltage_mv, void *data);
	int (*set_safety_timer_enable)(bool enable, void *data);
	int (*get_charger_state)(unsigned int *state, void *data);
	int (*get_input_current)(unsigned *current_ma, void *data);
};

enum htc_battery_bq2419x_notify_event {
	HTC_BATTERY_BQ2419X_SAFETY_TIMER_TIMEOUT,
};

enum htc_battery_bq2419x_charger_state {
	HTC_BATTERY_BQ2419X_IN_REGULATION	= (0x1U << 0),
	HTC_BATTERY_BQ2419X_DPM_MODE		= (0x1U << 1),
	HTC_BATTERY_BQ2419X_POWER_GOOD		= (0x1U << 2),
	HTC_BATTERY_BQ2419X_CHARGING		= (0x1U << 3),
	HTC_BATTERY_BQ2419X_KNOWN_VBUS		= (0x1U << 4),
};

void htc_battery_bq2419x_notify(enum htc_battery_bq2419x_notify_event);
int htc_battery_bq2419x_charger_register(struct htc_battery_bq2419x_ops *ops,
								void *data);
int htc_battery_bq2419x_charger_unregister(void *data);
#endif /* __HTC_BATTERY_BQ2419X_CHARGER_H */
