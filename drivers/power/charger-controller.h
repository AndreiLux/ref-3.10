/*
 * Copyright (C) LG Electronics 2014
 *
 * Charger Controller interface
 *
 */

enum {
	REQUEST_BY_POWER_SUPPLY_CHANGED = 1,
	REQUEST_BY_IBAT_LIMIT,
	REQUEST_BY_IUSB_LIMIT,
	REQUEST_BY_WIRELESS_LIMIT,
#ifdef CONFIG_LGE_PM_QC20_SCENARIO
	REQUEST_BY_QC20,
#endif
};

#ifdef CONFIG_LGE_PM_QC20_SCENARIO
#define MONITOR_USBIN_QC20_CHG		4*HZ
#define QC20_RETRY_COUNT		5
#define QC20_USBIN_VOL_THRSHD		7500

#define QC20_LCD_STATE  BIT(0)
#define QC20_CALL_STATE  BIT(1)
#define QC20_THERMAL_STATE  BIT(2)

enum qpnp_quick_current_status {
	QC20_CURRENT_NORMAL,
	QC20_CURRENT_LIMMITED,
	QC20_CURRENT_MAX,
};

enum qpnp_quick_charging_status {
	QC20_STATUS_NONE,
	QC20_STATUS_LCD_ON,
	QC20_STATUS_LCD_OFF,
	QC20_STATUS_CALL_ON,
	QC20_STATUS_CALL_OFF,
	QC20_STATUS_THERMAL_ON,
	QC20_STATUS_THERMAL_OFF,
};

struct qc20_info {
	int is_qc20;
	int is_highvol;
	int check_count;
	int status;
	int current_status;
	u32 iusb[QC20_CURRENT_MAX];
	u32 ibat[QC20_CURRENT_MAX];
};
#endif

enum {
	CC_BATT_TEMP_CHANGED = 1,
	CC_BATT_VOLT_CHANGED,
};

enum {
	CC_BATT_TEMP_STATE_COLD = -1,
	CC_BATT_TEMP_STATE_NORMAL = 0,
	CC_BATT_TEMP_STATE_HIGH,
	CC_BATT_TEMP_STATE_OVERHEAT,
	CC_BATT_TEMP_STATE_NODEFINED,
};

enum {
	CC_BATT_VOLT_UNDER_4_0 = 0,
	CC_BATT_VOLT_OVER_4_0,
	CC_BATT_VOLT_OVER_4_1,
};

/* Node for iUSB limit */
enum {
	IUSB_NODE_NO = 0,
	IUSB_NODE_LGE_CHG,
	IUSB_NODE_THERMAL,
	IUSB_NODE_1A_TA,
	MAX_IUSB_NODE,
};

/* Node for iBAT limit */
enum {
	IBAT_NODE_NO = 0,
	IBAT_NODE_LGE_CHG,
	IBAT_NODE_THERMAL,
	IBAT_NODE_EARJACK,
	IBAT_NODE_EXT_CTRL,
	IBAT_NODE_LGE_CHG_WLC, /* WLC must be placed last of list. */
	IBAT_NODE_EXCEPT_WLC = IBAT_NODE_LGE_CHG_WLC,
	MAX_IBAT_NODE,
};

/* Node for Wireless limit*/
enum {
	IUSB_NODE_THERMAL_WIRELSS = 0,
	IUSB_NODE_OTP_WIRELESS,
	MAX_IUSB_NODE_WIRELESS,
};

int notify_charger_controller(struct power_supply *psy, int requester);
int lge_battery_get_property(enum power_supply_property prop,
				       union power_supply_propval *val);
void update_thermal_condition(int state_changed);
void get_init_condition_theraml_engine(int batt_temp, int batt_volt);
void set_iusb_limit_cc(int value);

#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
extern struct pseudo_batt_info_type pseudo_batt_info;
extern int safety_timer;
#endif
