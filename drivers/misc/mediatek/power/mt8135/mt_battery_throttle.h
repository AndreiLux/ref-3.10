#ifndef MT_BATTERY_THROTTLE_H
#define MT_BATTERY_THROTTLE_H

#define BATTERY_BUDGET "battery_budget"
#define BATTERY_NOTIFY "battery_notify"
#define BATTERY_FG_BAT_ZCV "/sys/devices/platform/battery_meter/FG_g_fg_dbg_bat_zcv"
#define BATTERY_FG_BAT_VOL "/sys/devices/platform/battery_meter/FG_g_fg_dbg_bat_volt"
#define BATTERY_BAT_AVG_VOL "/sys/class/power_supply/battery/InstatVolt"

typedef enum _ENUM_PBUDGET_FACTOR {
	PBUDGET_FACTOR_INIT,
	PBUDGET_FACTOR_MAX,
	PBUDGET_FACTOR_MEDIUM,
	PBUDGET_FACTOR_MIN,
	PBUDGET_FACTOR_ADD,
	PBUDGET_FACTOR_SUBTRACT,
	PBUDGET_FACTOR_NO_CHANGE,
	PBUDGET_FACTOR_NUM
} ENUM_PBUDGET_FACTOR, *P_ENUM_PBUDGET_FACTOR;

typedef struct {
	int major, minor;
	int flag;
	dev_t devno;
	struct class *cls;
	struct cdev cdev;
	wait_queue_head_t queue;
} battery_throttle_dev_t;

typedef struct _BATTERY_THROTTLE_DATA_STRUC {
	int previous_power_budget_factor;
	int previous_power_budget;
	int previous_battery_voltage;
	int power_budget_factor;
	int battery_voltage;
	int battery_average_vol;
	int power_budget;
} BATTERY_THROTTLE_DATA_STRUC;

extern kal_bool upmu_is_chr_det(void);

#endif
