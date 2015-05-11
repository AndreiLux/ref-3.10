#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/of_device.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/qpnp/qpnp-adc.h>
#include "charger-controller.h"
#include <linux/wakelock.h>
#ifdef CONFIG_LGE_PM_USB_ID
#include <soc/qcom/lge/board_lge.h>
#endif
#if defined(CONFIG_LGE_PM_BATTERY_ID_CHECKER)
#include <linux/power/lge_battery_id.h>
#endif
#include <soc/qcom/smem.h>
#if defined(CONFIG_BATTERY_MAX17048)
#include <linux/power/max17048_battery.h>
#endif
#ifdef CONFIG_LGE_PM_UNIFIED_WLC
#include <linux/power/unified_wireless_charger.h>
#endif
#ifdef CONFIG_LGE_PM_CHARGE_INFO
#include <linux/power/max17048_battery.h>
#include <soc/qcom/lge/board_lge.h>
#endif

#ifdef CONFIG_QPNP_SMBCHARGER
#define CONFIG_LGE_PM_SMB_PROP
#define CONFIG_LGE_PM_VFLOAT_CHANGE
#endif

#define CHARGER_CONTROLLER_NAME "lge,charger-controller"
#define DRIVER_DESC			"charger IC driver current controller"
#define DRIVER_AUTHOR		"kwangmin.jeong@lge.com"
#define DRIVER_VERSION	"0.91"

#define CHARGER_CONTR_PSY_ONLINE_ON  1
#define CHARGER_CONTR_PSY_ONLINE_OFF  0

/* status of battery temperature */
/* battery temperature from -10 to 45 */
#define CHARGER_CONTR_NORMAL_TEMP			0
/* battery temperature from 45  to 55 */
#define CHARGER_CONTR_HIGH_TEMP			1
/* battery temperature under -10, or over 55 */
#define CHARGER_CONTR_ABNORMAL_TEMP		2

/* cable type */
#define CHARGER_CONTR_CABLE_NORMAL 0
#define CHARGER_CONTR_CABLE_FACTORY_CABLE	1

/* mA is unit of current. but uA is used in power_supply property */
#define CHARGER_CONTR_ZERO_CURRENT 0

/* mitigation temperture (unit is 0.1C , 200 = 20C) */
/* this value is changed depend on thermal-engine conf.*/
#define CHARGER_CONTR_LIMIT_TEMP_45		450
#define CHARGER_CONTR_LIMIT_TEMP_55		550
#define CHARGER_CONTR_LIMIT_TEMP_MINUS_10		-100

#ifdef CONFIG_LGE_PM_UNIFIED_WLC
#define TX_PAD_TURN_OFF_TEMP	600
#endif

/* predefined voltage (unit is mA, 3700 = 3.7V) */
#define CHARGER_CONTR_VOLTAGE_4V			4000
#define CHARGER_CONTR_VOLTAGE_4_1V		4100
#define CHARGER_CONTR_VOLTAGE_3_7V		3700

#define WIRE_OTP_LIMIT			500
#define WIRE_IUSB_NORMAL		900
#define WIRE_IUSB_THERMAL_1		700
#define WIRE_IUSB_THERMAL_2		500
#define WIRE_IBAT_THERMAL_1		1000
#define WIRE_IBAT_THERMAL_2		700

#define USB_PSY_NAME "usb"
#define BATT_PSY_NAME "battery"
#define FUELGAUGE_PSY_NAME "fuelgauge"
#define WIRELESS_PSY_NAME "dc"
#define CC_PSY_NAME	"charger_controller"
#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
#define BATT_ID_PSY_NAME "battery_id"
#endif

#define DEFAULT_BATT_TEMP               20
#define DEFAULT_BATT_VOLT               3999

enum print_reason {
	PR_DEBUG        = BIT(0),
	PR_INFO         = BIT(1),
	PR_ERR          = BIT(2),
};

static int cc_debug_mask = PR_INFO|PR_ERR;
module_param_named(
	debug_mask, cc_debug_mask, int, S_IRUSR | S_IWUSR
);

#define pr_cc(reason, fmt, ...)                                          \
    do {                                                                 \
	if (cc_debug_mask & (reason))                                    \
	    pr_info("[CC] " fmt, ##__VA_ARGS__);                         \
	else                                                             \
	    pr_debug("[CC] " fmt, ##__VA_ARGS__);                        \
    } while (0)

#define cc_psy_setprop(_psy, _psp, _val) \
	({\
		struct power_supply *__psy = chgr_contr->_psy;\
		union power_supply_propval __propval = { .intval = _val };\
		int __rc = -ENXIO;\
		if (likely(__psy && __psy->set_property)) {\
			__rc = __psy->set_property(__psy, \
				POWER_SUPPLY_PROP_##_psp, \
				&__propval);\
		} \
		__rc;\
	})

#define cc_psy_getprop(_psy, _psp, _val)	\
	({\
		struct power_supply *__psy = chgr_contr->_psy;\
		int __rc = -ENXIO;\
		if (likely(__psy && __psy->get_property)) {\
			__rc = __psy->get_property(__psy, \
				POWER_SUPPLY_PROP_##_psp, \
				_val);\
		} \
		__rc;\
	})

/* To set init charging current according to temperature and voltage of battery */
#define CONFIG_LGE_SET_INIT_CURRENT

#ifdef CONFIG_LGE_PM_VZW_REQ
typedef enum vzw_chg_state {
	VZW_NO_CHARGER,
	VZW_NORMAL_CHARGING,
	VZW_NOT_CHARGING,
	VZW_UNDER_CURRENT_CHARGING,
	VZW_USB_DRIVER_UNINSTALLED,
	VZW_CHARGER_STATUS_MAX,
} chg_state;
#endif

static enum power_supply_property pm_power_props_charger_contr_pros[] = {
	POWER_SUPPLY_PROP_STATUS, /* not used */
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_PRESENT, /* ChargerController init or not */
	POWER_SUPPLY_PROP_ONLINE, /* usb current is controlled by ChargerController or not */
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX, /* changed current by ChargerController */
	POWER_SUPPLY_PROP_INPUT_CURRENT_TRIM,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, /* limited current by external */
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL, /* temperature status */
#ifdef CONFIG_LGE_PM_SMB_PROP
	POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED,
#else
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
#endif
#ifdef CONFIG_LGE_PM_LLK_MODE
	POWER_SUPPLY_PROP_STORE_DEMO_ENABLED,
#endif
#ifdef CONFIG_LGE_PM_VZW_REQ
	POWER_SUPPLY_PROP_VZW_CHG,
#endif
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	POWER_SUPPLY_PROP_PSEUDO_BATT,
#endif
};

struct charger_contr {
	struct device		*dev;
	struct kobject		*kobj;
	struct power_supply charger_contr_psy;
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct power_supply *fg_psy;
	struct power_supply *wireless_psy;
#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	struct power_supply *batt_id_psy;
#endif
	struct power_supply *cc_psy;

	const char			*fg_psy_name;

	struct work_struct		cc_notify_work;
	struct mutex			notify_work_lock;
	int requester;
#ifdef CONFIG_LGE_PM_CHARGE_INFO
	struct delayed_work		charging_inform_work;
	struct qpnp_vadc_chip		*vadc_usbin_dev;
#endif
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	struct delayed_work		otp_work;
#endif

	/* pre-defined value by DT*/
	int current_ibat;
	int current_limit;
	int current_wlc_preset;
	int current_wlc_limit;
	int current_iusb_factory;
	int current_ibat_factory;

	int thermal_status; /* status of lg charging scenario. but not used */
	int batt_temp;
	int origin_batt_temp;
	int batt_volt;
	int xo_temperature;

	struct qpnp_vadc_chip *vadc_dev; /* for getting temperatrue of battery */

	int current_iusb_max; /* not used */
	int current_iusb_limit[MAX_IUSB_NODE];
	int currnet_iusb_wlc[MAX_IUSB_NODE_WIRELESS];
	int current_ibat_max;
	int current_ibat_limit[MAX_IBAT_NODE];
	int current_ibat_max_wireless;
	int current_wireless_limit;
	int ibat_limit_lcs;
	int wireless_lcs;
#ifdef CONFIG_LGE_PM_QC20_SCENARIO
	struct qc20_info qc20;
	struct delayed_work 	highvol_check_work;
#endif

	int current_ibat_changed_by_cc;
	int changed_ibat_by_cc;
	int current_iusb_changed_by_cc;
	int changed_iusb_by_cc;

	int current_real_usb_psy;
	int usb_cable_info; /* normal cable (0) factory cable(1), 130k factory cable(2) */
	int battery_status_cc;
	int battery_status_charger;

#if defined(CONFIG_LGE_PM_BATTERY_ID_CHECKER)
	int batt_id_smem;
	int batt_smem_present;
#endif
	int usb_online; /* same as is_usb */
	int wlc_online; /* same as is_wireless */
	int is_usb;
	int is_wireless;
	int batt_eoc;

	int current_usb_psy;
	int current_wireless_psy;

	int batt_temp_state;
	int batt_volt_state;
	int charge_type;

	struct wake_lock chg_wake_lock;
#ifdef CONFIG_LGE_PM_VFLOAT_CHANGE
	int vfloat_mv;
#endif
#ifdef CONFIG_LGE_PM_LLK_MODE
	int store_demo_enabled;
#endif
	int aicl_done_current;
#ifdef CONFIG_LGE_PM_VZW_REQ
	int vzw_chg_mode;
	int vzw_under_current_count;
#endif
};

static char *pm_batt_supplied_to[] = {
		"battery1", /* todo need to be changed */
};

/* charger controller node */
static int cc_ibat_limit = -1;
static int cc_iusb_limit = -1;
#ifdef CONFIG_LGE_PM_QC20_SCENARIO
static int cc_ibat_qc20_limit = -1;
#endif
static int cc_wireless_limit = -1;

static int cc_init_ok;
struct charger_contr *chgr_contr;

static int get_prop_fuelgauge_capacity(void);
#ifdef CONFIG_LGE_PM_CHARGE_INFO
static bool is_usb_present(void);
static bool is_wireless_present(void);
#define CHARGING_INFORM_NORMAL_TIME	60000
#define DEFAULT_VBUS_UV                 5000

static char *cable_type_str[] = {
	"NOT INIT", "MHL 1K", "U_28P7K", "28P7K", "56K",
	"100K", "130K", "180K", "200K", "220K",
	"270K", "330K", "620K", "910K", "OPEN"
};

static char *power_supply_type_str[] = {
	"Unknown", "Battery", "UPS", "Mains", "USB",
	"USB_DCP", "USB_CDP", "USB_ACA", "USB_HVDCP", "Wireless",
	"BMS", "USB_Parallel", "fuelgauge", "Wipower"
};

static char *batt_fake_str[] = {
	"FAKE_OFF", "FAKE_ON"
};

static char* get_usb_type(void)
{
	union power_supply_propval ret = {0,};

	if (!chgr_contr->usb_psy) {
		pr_cc(PR_ERR, "usb power supply is not registerd\n");
		return "null";
	}

	cc_psy_getprop(usb_psy, TYPE, &ret);

	return power_supply_type_str[ret.intval];
}

static int cc_get_usb_adc(void)
{
	struct qpnp_vadc_result results;
	int rc, usbin_vol;

	if (!is_usb_present())
		return DEFAULT_VBUS_UV;

	if (IS_ERR_OR_NULL(chgr_contr->vadc_usbin_dev)) {
		chgr_contr->vadc_usbin_dev = qpnp_get_vadc(chgr_contr->dev, "chg");
		if (IS_ERR_OR_NULL(chgr_contr->vadc_usbin_dev)) {
			pr_cc(PR_ERR, "vadc is not init yet");
			return DEFAULT_VBUS_UV;
		}
	}

	rc = qpnp_vadc_read(chgr_contr->vadc_usbin_dev, USBIN, &results);
	if (rc < 0) {
		dev_err(chgr_contr->dev, "failed to read usb in voltage. rc = %d\n",
			rc);
		return DEFAULT_VBUS_UV;
	}

	usbin_vol = results.physical/1000;
	pr_cc(PR_DEBUG, "usb in voltage = %dmV\n", usbin_vol);

	return usbin_vol;
}

static void charging_information(struct work_struct *work)
{
	union power_supply_propval val = {0, };

	bool usb_present = is_usb_present();
	bool wireless_present = is_wireless_present();
	char *usb_type_name = get_usb_type();
	char *cable_type_name = cable_type_str[lge_pm_get_cable_type()];
	int usbin_vol = cc_get_usb_adc();
	char *batt_fake = batt_fake_str[pseudo_batt_info.mode];
	int batt_soc = max17048_get_capacity();
	int batt_vol = max17048_get_voltage();
	int pmi_iusb_set = chgr_contr->current_iusb_changed_by_cc;
	int pmi_iusb_aicl = chgr_contr->aicl_done_current;
	int pmi_ibat_set = chgr_contr->current_ibat_changed_by_cc;
	int total_ibat_now = 0;
	int batt_temp = 0;
	cc_psy_getprop(batt_psy, TEMP, &val);
	batt_temp = chgr_contr->origin_batt_temp / 10;
	cc_psy_getprop(batt_psy, CURRENT_NOW, &val);
	total_ibat_now = val.intval/1000;

	pr_cc(PR_INFO, "[C], USB_PRESENT, WIRELESS_PRESENT, USB_TYPE, "
		"CABLE_INFO, USBIN_VOL, BATT_FAKE, BATT_TEMP, BATT_SOC, BATT_VOL, "
		"PMI_IUSB_SET, PMI_IUSB_AICL, PMI_IBAT_SET, TOTAL_IBAT_NOW\n");
	pr_cc(PR_INFO, "[I], %d, %d, %s, %s, %d, "
		"%s, %d, %d, %d, "
		"%d, %d, %d, %d\n",
		usb_present, wireless_present, usb_type_name, cable_type_name, usbin_vol,
		batt_fake, batt_temp, batt_soc, batt_vol,
		pmi_iusb_set, pmi_iusb_aicl, pmi_ibat_set, total_ibat_now);

	schedule_delayed_work(&chgr_contr->charging_inform_work,
		round_jiffies_relative(msecs_to_jiffies(CHARGING_INFORM_NORMAL_TIME)));
	power_supply_changed(chgr_contr->batt_psy);
}
#endif

#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
#define OTP_POLLING_TIME                30000
#define OTP_INIT_TIME                   3000
static int charger_contr_get_battery_temperature(void);
static bool otp_enabled;

static void check_charging_state(struct work_struct *work)
{
	/* battery termperature state check */
	int temp_changed = 0;
	int volt_changed = 0;
	int temp = charger_contr_get_battery_temperature();
	int volt = max17048_get_voltage();

	if (temp != 0)
		chgr_contr->batt_temp = temp/10;
	else
		chgr_contr->batt_temp = 0;
	chgr_contr->batt_volt = volt;

	pr_cc(PR_INFO, "current batt_temp[%d], batt_temp_state[%d], "
			"current batt_volt[%d], batt_volt_state[%d]\n",
			chgr_contr->batt_temp, chgr_contr->batt_temp_state,
			chgr_contr->batt_volt, chgr_contr->batt_volt_state);

#ifdef CONFIG_MACH_MSM8992_P1_SPR_US
	if (chgr_contr->batt_temp <= -5) {
#else
	if (chgr_contr->batt_temp <= -10) {
#endif
		if (chgr_contr->batt_temp_state != CC_BATT_TEMP_STATE_COLD) {
			pr_cc(PR_INFO, "need to change batt_temp_state[%d->%d]\n",
					chgr_contr->batt_temp_state,
					CC_BATT_TEMP_STATE_COLD);
			temp_changed = 1;
		}
#ifdef CONFIG_MACH_MSM8992_P1_SPR_US
	} else if (chgr_contr->batt_temp >= -2 && chgr_contr->batt_temp <= 42) {
#else
	} else if (chgr_contr->batt_temp >= -5 && chgr_contr->batt_temp <= 42) {
#endif
		if (chgr_contr->batt_temp_state != CC_BATT_TEMP_STATE_NORMAL) {
			pr_cc(PR_INFO, "need to change batt_temp_state[%d->%d]\n",
					chgr_contr->batt_temp_state,
					CC_BATT_TEMP_STATE_NORMAL);
			temp_changed = 1;
		}
	} else if (chgr_contr->batt_temp >= 45 && chgr_contr->batt_temp <= 52) {
		if (chgr_contr->batt_temp_state != CC_BATT_TEMP_STATE_HIGH) {
			pr_cc(PR_INFO, "need to change batt_temp_state[%d->%d]\n",
					chgr_contr->batt_temp_state,
					CC_BATT_TEMP_STATE_HIGH);
			temp_changed = 1;
		}
	} else if (chgr_contr->batt_temp >= 55) {
		if (chgr_contr->batt_temp_state != CC_BATT_TEMP_STATE_OVERHEAT) {
			pr_cc(PR_INFO, "need to change batt_temp_state[%d->%d]\n",
					chgr_contr->batt_temp_state,
					CC_BATT_TEMP_STATE_OVERHEAT);
			temp_changed = 1;
		}
	} else {
		temp_changed = 0;
	}

	/* battery voltage status check */
	if (chgr_contr->batt_volt <= 3950) {
		if (chgr_contr->batt_volt_state != CC_BATT_VOLT_UNDER_4_0) {
			pr_cc(PR_INFO, "need to change batt_volt_state[%d->%d]\n",
					chgr_contr->batt_volt_state,
					CC_BATT_VOLT_UNDER_4_0);
			volt_changed = 1;
		}
	} else if (chgr_contr->batt_volt >= 4050) {
		if (chgr_contr->batt_volt_state != CC_BATT_VOLT_OVER_4_0) {
			pr_cc(PR_INFO, "need to change batt_volt_state[%d->%d]\n",
					chgr_contr->batt_volt_state,
					CC_BATT_VOLT_OVER_4_0);
			volt_changed = 1;
		}
	} else {
		volt_changed = 0;
	}

	if (temp_changed) {
		get_init_condition_theraml_engine(chgr_contr->batt_temp,
				chgr_contr->batt_volt);
		update_thermal_condition(CC_BATT_TEMP_CHANGED);
	}

	if (volt_changed) {
		get_init_condition_theraml_engine(chgr_contr->batt_temp,
				chgr_contr->batt_volt);
		update_thermal_condition(CC_BATT_VOLT_CHANGED);
	}

	if (temp == 200) {
		schedule_delayed_work(&chgr_contr->otp_work,
			round_jiffies_relative(msecs_to_jiffies(OTP_INIT_TIME)));
	} else {
		schedule_delayed_work(&chgr_contr->otp_work,
			round_jiffies_relative(msecs_to_jiffies(OTP_POLLING_TIME)));
	}

	return;
}
#endif

static bool is_usb_present(void)
{
	union power_supply_propval val = {0, };

	cc_psy_getprop(usb_psy, PRESENT, &val);

	if (val.intval == 1)
		return true;
	else
		return false;
}

static bool is_wireless_present(void)
{
	union power_supply_propval val = {0, };

	cc_psy_getprop(wireless_psy, PRESENT, &val);

	if (val.intval == 1)
		return true;
	else
		return false;
}

struct charger_contr *get_charger_contr(void) /* todo: not used */
{
	if (!chgr_contr) {
		return ERR_PTR(-EPROBE_DEFER);
		pr_cc(PR_ERR, "Fail to get charger contr\n");
	}
	return chgr_contr;
}

static int pm_set_property_charger_contr(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct charger_contr *cc = container_of(psy, struct charger_contr, charger_contr_psy);

	pr_cc(PR_DEBUG, "charger_contr_set_property\n");
	if (cc == ERR_PTR(-EPROBE_DEFER))
		return -EPROBE_DEFER;

	switch (psp) {
#ifdef CONFIG_LGE_PM_LLK_MODE
	case POWER_SUPPLY_PROP_STORE_DEMO_ENABLED:
		cc->store_demo_enabled = val->intval;
		break;
#endif
#ifdef CONFIG_LGE_PM_SMB_PROP
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
#else
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
#endif
		if (val->intval == 0)
			chgr_contr->current_ibat_limit[IBAT_NODE_EXT_CTRL] = 0;
		else
			chgr_contr->current_ibat_limit[IBAT_NODE_EXT_CTRL] = -1;
#ifdef CONFIG_LGE_PM_SMB_PROP
		cc_psy_setprop(batt_psy, BATTERY_CHARGING_ENABLED, val->intval);
#else
		cc_psy_setprop(batt_psy, CHARGING_ENABLED, val->intval);
#endif
		break;
	default:
		pr_cc(PR_INFO, "Invalid property(%d)\n", psp);
		return -EINVAL;
	}
	return 0;
}

static int charger_contr_property_is_writerable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
#ifdef CONFIG_LGE_PM_SMB_PROP
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
#else
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
#endif
		return 1;
	default:
		break;
	}

	return 0;
}

static int pm_get_property_charger_contr(struct power_supply *psy,
	enum power_supply_property cc_property, union power_supply_propval *val)
{
	struct charger_contr *cc = container_of(psy, struct charger_contr, charger_contr_psy);

	pr_cc(PR_DEBUG, "charger_contr_get_property(%d)\n", cc_property);
	if (cc == ERR_PTR(-EPROBE_DEFER))
		return -EPROBE_DEFER;

	switch (cc_property) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = chgr_contr->charge_type;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = cc_init_ok;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = cc->current_iusb_max;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		val->intval = cc->current_real_usb_psy;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
/* todo */
		val->intval = cc->changed_ibat_by_cc;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		val->intval = cc->current_ibat_limit[IBAT_NODE_LGE_CHG];
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_TRIM:
		val->intval = cc->current_ibat_max;
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		val->intval = cc->thermal_status;
		break;
#ifdef CONFIG_LGE_PM_SMB_PROP
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		cc_psy_getprop(batt_psy, BATTERY_CHARGING_ENABLED, val);
		break;
#else
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		cc_psy_getprop(batt_psy, CHARGING_ENABLED, val);
		break;
#endif
#ifdef CONFIG_LGE_PM_LLK_MODE
	case POWER_SUPPLY_PROP_STORE_DEMO_ENABLED:
		val->intval = cc->store_demo_enabled;
		break;
#endif
#ifdef CONFIG_LGE_PM_VZW_REQ
	case POWER_SUPPLY_PROP_VZW_CHG:
		pr_cc(PR_DEBUG, "POWER_SUPPLY_PROP_VZW_CHG(%d)\n", cc_property);
		val->intval = cc->vzw_chg_mode;
		break;
#endif
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	case POWER_SUPPLY_PROP_PSEUDO_BATT:
		val->intval = pseudo_batt_info.mode;
		break;
#endif
	default:
		pr_cc(PR_INFO, "Invalid property(%d)\n", cc_property);
		return -EINVAL;
	}
	return 0;
}


/* todo need to be changed depend on adc of battery temperature */
#define CHARGER_CONTROLLER_BATTERY_DEFAULT_TEMP		250
static int charger_contr_get_battery_temperature(void)
{
	union power_supply_propval val = {0,};
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
	int rc = 0;
	struct qpnp_vadc_result results;
#endif

	rc = cc_psy_getprop(batt_psy, TEMP, &val);
#ifdef CONFIG_LGE_PM_UNIFIED_WLC
	/* if triggered when temperature to reach 60C, should be turn off TX_PAD */
	if (chgr_contr->wlc_online) {
		if (val.intval >= TX_PAD_TURN_OFF_TEMP) {
			pr_cc(PR_INFO, "High temp, tx-pad off!\n");
			wireless_chg_term_handler();
		}
	}
#endif
	if (!rc) {
		pr_cc(PR_DEBUG, "battery temp =%d\n", val.intval);
		return val.intval;
	}
#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
	chgr_contr->vadc_dev = qpnp_get_vadc(chgr_contr->dev, "charger-contr");
	if (chgr_contr->vadc_dev == NULL) {
		chgr_contr->vadc_dev = qpnp_get_vadc(chgr_contr->dev,
			"charger-contr");
		return CHARGER_CONTROLLER_BATTERY_DEFAULT_TEMP;
	}

	rc = qpnp_vadc_read(chgr_contr->vadc_dev, LR_MUX6_AMUX_THM3, &results);
	if (rc) {
		chgr_contr->vadc_dev = qpnp_get_vadc(chgr_contr->dev, "charger-contr");
		pr_cc(PR_DEBUG, "Report default %d (rc:%d)\n",
			CHARGER_CONTROLLER_BATTERY_DEFAULT_TEMP, rc);
		return CHARGER_CONTROLLER_BATTERY_DEFAULT_TEMP;
	} else {
		pr_cc(PR_DEBUG, "get_bat_temp %d %lld\n",
			results.adc_code, results.physical);
		return (int)results.physical;
	}
#else
	pr_cc(PR_ERR, "CONFIG_SENSORS_QPNP_ADC_VOLTAGE is not defined.\n");
	return CHARGER_CONTROLLER_BATTERY_DEFAULT_TEMP;
#endif
}

void update_thermal_condition(int state_changed)
{
	int ibat_limit_lcs;
	int wireless_lcs;

	/* Whend temp is changed , limit value should be updated */
	if (state_changed == CC_BATT_TEMP_CHANGED) {
		pr_cc(PR_INFO, "update_thermal_condition(batt_temp_state=%d)\n",
			chgr_contr->batt_temp_state);

		if (chgr_contr->batt_temp_state == CC_BATT_TEMP_STATE_HIGH) {
			if (chgr_contr->batt_volt_state == CC_BATT_VOLT_UNDER_4_0)
				ibat_limit_lcs = 450;
			else
				ibat_limit_lcs = 0;
		} else if ((chgr_contr->batt_temp_state == CC_BATT_TEMP_STATE_OVERHEAT) ||
				(chgr_contr->batt_temp_state == CC_BATT_TEMP_STATE_COLD)) {
			ibat_limit_lcs = 0;
		} else /* normal temperature(-10C ~ 45C) */
			ibat_limit_lcs = -1;
	}

	/* Whend voltage is changed , limit value should be updated */
	if (state_changed == CC_BATT_VOLT_CHANGED) {
		if (chgr_contr->batt_volt_state == CC_BATT_VOLT_OVER_4_0) {
			if (chgr_contr->batt_temp_state == CC_BATT_TEMP_STATE_NORMAL)
				ibat_limit_lcs = -1;
			else /* over 45C or under -10C*/
				ibat_limit_lcs = 0;
		} else if (chgr_contr->batt_volt_state == CC_BATT_VOLT_OVER_4_1) {
			if (chgr_contr->is_usb) { /* USB psy */
				if (chgr_contr->batt_temp_state == CC_BATT_TEMP_STATE_NORMAL)
					ibat_limit_lcs = -1;
				else /* over 45C or under -10C */
					ibat_limit_lcs = 0;
			}
		} else { /* under 4.0 V */
			if (chgr_contr->batt_temp_state == CC_BATT_TEMP_STATE_NORMAL)
				ibat_limit_lcs = -1;
			else if (chgr_contr->batt_temp_state == CC_BATT_TEMP_STATE_HIGH)
				ibat_limit_lcs = 450;
			else if ((chgr_contr->batt_temp_state == CC_BATT_TEMP_STATE_OVERHEAT) ||
			(chgr_contr->batt_temp_state == CC_BATT_TEMP_STATE_COLD))
				ibat_limit_lcs = 0;
		}
	}
	pr_cc(PR_INFO, "update_thermal_condition(ibat_limit_charging_scenario=%d)\n",
		ibat_limit_lcs);

	if (ibat_limit_lcs == 450)
		wireless_lcs = chgr_contr->current_wlc_limit;
	else
		wireless_lcs = ibat_limit_lcs;

	chgr_contr->ibat_limit_lcs = ibat_limit_lcs;
	chgr_contr->wireless_lcs = wireless_lcs;

	notify_charger_controller(&chgr_contr->charger_contr_psy,
		REQUEST_BY_IBAT_LIMIT);
	power_supply_changed(chgr_contr->batt_psy);
}

/* return is max current cound provide to battery psy */
/* Update usb_psy, qc20, cable type, wireless */
static int update_pm_psy_status(int requester)
{
	union power_supply_propval val = {0,};
#ifdef CONFIG_LGE_PM_USB_ID
	unsigned int cable_info;
#endif
	int is_usb_on, is_wireless_on;

	pr_cc(PR_DEBUG, "Before update_pm_psy_status (iusb=%d)(CC=%d)(org=%d)\n",
		chgr_contr->current_usb_psy, chgr_contr->current_iusb_changed_by_cc,
		chgr_contr->current_real_usb_psy);

	cc_psy_getprop(wireless_psy, PRESENT, &val);
	chgr_contr->wlc_online = val.intval;
	if (chgr_contr->wlc_online) {
		chgr_contr->is_wireless = 1;
	} else {
		chgr_contr->is_wireless = 0;
	}

	cc_psy_getprop(usb_psy, ONLINE, &val);

	if (chgr_contr->usb_online != val.intval)
		pr_cc(PR_INFO, "update usb online. cc->online = %d, online = %d.\n",
			chgr_contr->usb_online, val.intval);

	chgr_contr->usb_online = val.intval;
	if (chgr_contr->usb_online) {
		cc_psy_getprop(usb_psy, CURRENT_MAX, &val);
		if (val.intval &&
			chgr_contr->current_usb_psy != val.intval/1000) {
			pr_cc(PR_INFO, "Present iusb current =%d\n",
				val.intval/1000);
			chgr_contr->current_usb_psy = val.intval/1000;
		}
		chgr_contr->is_usb = 1;
	} else {
		chgr_contr->current_usb_psy = 0;
		chgr_contr->is_usb = 0;
#ifdef CONFIG_LGE_PM_QC20_SCENARIO
		chgr_contr->qc20.is_qc20 = 0;
		chgr_contr->qc20.is_highvol= 0;
		chgr_contr->qc20.check_count = 0;
		cancel_delayed_work(&chgr_contr->highvol_check_work);
#endif
#ifdef CONFIG_LGE_PM_VZW_REQ
		chgr_contr->vzw_chg_mode = VZW_NO_CHARGER;
#endif
	}

	pr_cc(PR_INFO, "After update_pm_psy_status (iusb=%d)(CC=%d)(org=%d)(wlc=%d)\n",
		chgr_contr->current_usb_psy, chgr_contr->current_iusb_changed_by_cc,
		chgr_contr->current_real_usb_psy, chgr_contr->current_wireless_psy);

	if (chgr_contr->current_usb_psy != chgr_contr->current_iusb_changed_by_cc) {
		chgr_contr->changed_iusb_by_cc = 0;
		chgr_contr->current_real_usb_psy =
			(chgr_contr->current_usb_psy < 0) ? 0 : chgr_contr->current_usb_psy;
		pr_cc(PR_INFO, "USB PSY changed by original dwc (%d)\n",
			chgr_contr->current_real_usb_psy);
	}

#ifdef CONFIG_LGE_PM_USB_ID
	cable_info = lge_pm_get_cable_type();
	if ((cable_info == CABLE_56K) || (cable_info == CABLE_130K)
			|| (cable_info == CABLE_910K))
		chgr_contr->usb_cable_info = CHARGER_CONTR_CABLE_FACTORY_CABLE;
	else
		chgr_contr->usb_cable_info = CHARGER_CONTR_CABLE_NORMAL;
#endif

	is_usb_on = is_usb_present();
	is_wireless_on = is_wireless_present();

	if (!is_usb_on)
		chgr_contr->current_iusb_limit[IUSB_NODE_1A_TA] = -1; /* should be zero, when usb removed */
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO

	pr_cc(PR_DEBUG, "usb_present[%d], wireless_present[%d], otp_enabled[%d]\n",
			is_usb_on, is_wireless_on, otp_enabled);
	if (is_usb_on || is_wireless_on) {
		if (!otp_enabled) {
			pr_cc(PR_INFO, "OTP scenario enabled\n");
			schedule_delayed_work(&chgr_contr->otp_work,
				round_jiffies_relative(msecs_to_jiffies(OTP_INIT_TIME)));
			otp_enabled = true;
		}
	} else {
		if (otp_enabled) {
			pr_cc(PR_INFO, "OTP scenario disabled\n");
			cancel_delayed_work(&chgr_contr->otp_work);
			otp_enabled = false;
		}
	}
#endif

	return 0;
}
static bool is_factory_cable_910k(void)
{
	int cable_info;
	bool ret;

	cable_info = lge_pm_get_cable_type();
	if (cable_info == CABLE_910K)
		ret = true;
	else
		ret = false;

	return ret;
}

#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
#define PSEUDO_BATT_USB_ICL	900
#endif

static void change_iusb(int value)
{
	pr_cc(PR_DEBUG, "change_iusb(value[%d])\n", value);

#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	if ((pseudo_batt_info.mode) && (value < PSEUDO_BATT_USB_ICL)
			&& chgr_contr->usb_online) {
		mutex_lock(&chgr_contr->notify_work_lock);
		power_supply_set_current_limit(chgr_contr->usb_psy,
				PSEUDO_BATT_USB_ICL * 1000);
		chgr_contr->current_iusb_changed_by_cc = value;
		chgr_contr->current_iusb_limit[IUSB_NODE_1A_TA] = -1;
		chgr_contr->changed_iusb_by_cc = 1;
		mutex_unlock(&chgr_contr->notify_work_lock);
		pr_cc(PR_INFO, "pseudo_current setting[%d], "
				"current_iusb_changed_by_cc[%d], "
				"value[%d]\n", PSEUDO_BATT_USB_ICL,
				chgr_contr->current_iusb_changed_by_cc, value);
		return;
	}
#endif

	if (chgr_contr->current_iusb_changed_by_cc == value)
		return;

	if (chgr_contr->usb_cable_info == CHARGER_CONTR_CABLE_FACTORY_CABLE) {
		power_supply_set_current_limit(chgr_contr->usb_psy,
				chgr_contr->current_iusb_factory * 1000);
	} else {
		mutex_lock(&chgr_contr->notify_work_lock);
		if(chgr_contr->wlc_online && !chgr_contr->usb_online){
			power_supply_set_current_limit(chgr_contr->wireless_psy,
					value*1000);
			pr_cc(PR_INFO, "Set iUSB wireless current value = %d\n",
					value);
		}
		else{
			power_supply_set_current_limit(chgr_contr->usb_psy,
					value*1000);
			pr_cc(PR_INFO, "Set iUSB usb current value = %d\n",
					value);
		}
		mutex_unlock(&chgr_contr->notify_work_lock);
	}
	chgr_contr->current_iusb_changed_by_cc = value;
	chgr_contr->changed_iusb_by_cc = 1;

}

static void change_ibat(int value)
{
	union power_supply_propval val;

	pr_cc(PR_DEBUG, "change_ibat (value=%d)\n", value);
	if (chgr_contr->current_ibat_changed_by_cc != value) {
		pr_cc(PR_INFO, "Ibat max = %d, OTP = %d, thermal = %d, "
			"set current = (%d) -> (%d)\n",
			chgr_contr->current_ibat_limit[IBAT_NODE_NO],
			chgr_contr->current_ibat_limit[IBAT_NODE_LGE_CHG],
			chgr_contr->current_ibat_limit[IBAT_NODE_THERMAL],
			chgr_contr->current_ibat_changed_by_cc,
			value);

		if (chgr_contr->usb_cable_info ==
				CHARGER_CONTR_CABLE_FACTORY_CABLE) {
#ifdef CONFIG_LGE_PM_SMB_PROP
			cc_psy_setprop(batt_psy, CONSTANT_CHARGE_CURRENT_MAX,
				chgr_contr->current_ibat_factory * 1000);
#else
			cc_psy_setprop(batt_psy, CURRENT_MAX,
				chgr_contr->current_ibat_factory * 1000);
#endif
		} else if (value != 0) {
#ifdef CONFIG_LGE_PM_SMB_PROP
			cc_psy_getprop(batt_psy, BATTERY_CHARGING_ENABLED, &val);
#else
			cc_psy_getprop(batt_psy, CHARGING_ENABLED, &val);
#endif
			if (val.intval == 0)
#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
				if (!is_factory_cable_910k() ||
					chgr_contr->batt_smem_present)
#else
				if (!is_factory_cable_910k())
#endif
#ifdef CONFIG_LGE_PM_SMB_PROP
					cc_psy_setprop(batt_psy,
						BATTERY_CHARGING_ENABLED, 1);
#else
					cc_psy_setprop(batt_psy,
						CHARGING_ENABLED, 1);
#endif
			mutex_lock(&chgr_contr->notify_work_lock);
#ifdef CONFIG_LGE_PM_SMB_PROP
			cc_psy_setprop(batt_psy, CONSTANT_CHARGE_CURRENT_MAX,
								value * 1000);
#else
			cc_psy_setprop(batt_psy, CURRENT_MAX, value * 1000);
#endif
			mutex_unlock(&chgr_contr->notify_work_lock);
		} else { /* value == 0 */
		/* todo: charging disable , but now 300mA charging for test */
			mutex_lock(&chgr_contr->notify_work_lock);
#ifdef CONFIG_LGE_PM_SMB_PROP
			cc_psy_setprop(batt_psy, BATTERY_CHARGING_ENABLED, 0);
#else
			cc_psy_setprop(batt_psy, CHARGING_ENABLED, 0);
#endif
			mutex_unlock(&chgr_contr->notify_work_lock);
		}
		chgr_contr->current_ibat_changed_by_cc = value;
		chgr_contr->changed_ibat_by_cc = 1;
	}
}

static int set_charger_control_current(int limit, int requester)
{
	int i;
	int ibat_limit = 0;
	int iusb_limit  = 0;
	int iusb_limit_wireless = 0;

#ifdef CONFIG_LGE_PM_QC20_SCENARIO
	if (chgr_contr->qc20.is_qc20 && chgr_contr->qc20.is_highvol) {
		pr_cc(PR_INFO, "QC2.0 Current Set!\n");
		chgr_contr->current_iusb_max =
			chgr_contr->qc20.iusb[chgr_contr->qc20.current_status];
		chgr_contr->current_ibat_max =
			chgr_contr->qc20.ibat[chgr_contr->qc20.current_status];
		if (chgr_contr->qc20.current_status == QC20_CURRENT_NORMAL)
			chgr_contr->current_ibat_limit[IBAT_NODE_THERMAL] = cc_ibat_qc20_limit;
		else
			chgr_contr->current_ibat_limit[IBAT_NODE_THERMAL] = cc_ibat_limit;

		chgr_contr->current_ibat_limit[IBAT_NODE_LGE_CHG] = chgr_contr->ibat_limit_lcs;
		chgr_contr->current_iusb_limit[IUSB_NODE_THERMAL] = cc_iusb_limit;
	} else
#endif
	if (chgr_contr->is_usb) {
		pr_cc(PR_INFO, "USB Current Set!\n");
		chgr_contr->current_iusb_max = chgr_contr->current_real_usb_psy;
		chgr_contr->current_ibat_max = chgr_contr->current_ibat;
		chgr_contr->current_iusb_limit[IUSB_NODE_THERMAL] = cc_iusb_limit;
		chgr_contr->current_ibat_limit[IBAT_NODE_THERMAL] = cc_ibat_limit;
		chgr_contr->current_ibat_limit[IBAT_NODE_LGE_CHG] = chgr_contr->ibat_limit_lcs;
	} else if (chgr_contr->is_wireless) {
		pr_cc(PR_INFO, "Wireless Current Set!\n");
		/* Normal */
		chgr_contr->current_iusb_max = chgr_contr->current_wireless_psy;
		chgr_contr->current_ibat_max = chgr_contr->current_ibat_max_wireless;
		/* Thermal */
		if (cc_wireless_limit == -1 || cc_wireless_limit == WIRE_IUSB_NORMAL) {
			chgr_contr->currnet_iusb_wlc[IUSB_NODE_THERMAL_WIRELSS] =
				chgr_contr->current_wireless_psy;
			chgr_contr->current_ibat_limit[IBAT_NODE_THERMAL] =
				chgr_contr->current_ibat_max_wireless;
		} else if (cc_wireless_limit == WIRE_IUSB_THERMAL_1) {
			chgr_contr->currnet_iusb_wlc[IUSB_NODE_THERMAL_WIRELSS] =
				WIRE_IUSB_THERMAL_1;
			chgr_contr->current_ibat_limit[IBAT_NODE_THERMAL] =
				WIRE_IBAT_THERMAL_1;
		} else if (cc_wireless_limit == WIRE_IUSB_THERMAL_2) {
			chgr_contr->currnet_iusb_wlc[IUSB_NODE_THERMAL_WIRELSS] =
				WIRE_IUSB_THERMAL_2;
			chgr_contr->current_ibat_limit[IBAT_NODE_THERMAL] =
				WIRE_IBAT_THERMAL_2;
		} else {
			chgr_contr->currnet_iusb_wlc[IUSB_NODE_THERMAL_WIRELSS] = cc_wireless_limit;
			chgr_contr->current_ibat_limit[IBAT_NODE_THERMAL] = cc_wireless_limit;
		}
		/* OTP */
		if (chgr_contr->wireless_lcs == -1) {
			chgr_contr->currnet_iusb_wlc[IUSB_NODE_OTP_WIRELESS] = chgr_contr->current_wireless_psy;
			chgr_contr->current_ibat_limit[IBAT_NODE_LGE_CHG_WLC] =
				chgr_contr->current_ibat_max_wireless;
		} else if (chgr_contr->wireless_lcs == WIRE_OTP_LIMIT) {
			chgr_contr->currnet_iusb_wlc[IUSB_NODE_OTP_WIRELESS] = chgr_contr->wireless_lcs;
			chgr_contr->current_ibat_limit[IBAT_NODE_LGE_CHG_WLC] =
				chgr_contr->wireless_lcs;
		} else {
			chgr_contr->currnet_iusb_wlc[IUSB_NODE_OTP_WIRELESS] = chgr_contr->current_wlc_limit;
			chgr_contr->current_ibat_limit[IBAT_NODE_LGE_CHG_WLC] =
				chgr_contr->wireless_lcs;
		}
	} else {
		pr_cc(PR_INFO, "Charger Off_line!\n");
		chgr_contr->current_iusb_max = 0;
		chgr_contr->current_ibat_max = 0;
		chgr_contr->current_iusb_limit[IUSB_NODE_THERMAL] = cc_iusb_limit;
		chgr_contr->current_ibat_limit[IBAT_NODE_THERMAL] = cc_ibat_limit;
		chgr_contr->current_ibat_limit[IBAT_NODE_LGE_CHG] = chgr_contr->ibat_limit_lcs;
	}

	chgr_contr->current_iusb_limit[IUSB_NODE_NO] = chgr_contr->current_iusb_max;
	iusb_limit = chgr_contr->current_iusb_limit[IUSB_NODE_NO];
	chgr_contr->current_ibat_limit[IBAT_NODE_NO] = chgr_contr->current_ibat_max;
	ibat_limit = chgr_contr->current_ibat_limit[IBAT_NODE_NO];

	if (requester != REQUEST_BY_IBAT_LIMIT) { /* change iusb */
		for (i = 1; i < MAX_IUSB_NODE; i++) {
			if (chgr_contr->current_iusb_limit[i] >= 0 )
				iusb_limit = min(iusb_limit, chgr_contr->current_iusb_limit[i]);
		}
		change_iusb(iusb_limit);
	}

	if (requester != REQUEST_BY_IUSB_LIMIT) { /* change ibat */
		for (i = 1; i < IBAT_NODE_EXCEPT_WLC; i++) {
			if (chgr_contr->current_ibat_limit[i] >= 0 )
				ibat_limit = min(ibat_limit, chgr_contr->current_ibat_limit[i]);
		}
		/* check IBAT/IUSB NODE  for wireless charger */
		if (!chgr_contr->usb_online && chgr_contr->wlc_online) {
			ibat_limit = min(chgr_contr->current_ibat_limit[IBAT_NODE_LGE_CHG_WLC],
						chgr_contr->current_ibat_limit[IBAT_NODE_THERMAL]);
			iusb_limit_wireless =
				min(chgr_contr->currnet_iusb_wlc[IUSB_NODE_THERMAL_WIRELSS],
						chgr_contr->currnet_iusb_wlc[IUSB_NODE_OTP_WIRELESS]);
			change_iusb(iusb_limit_wireless);
		}
		change_ibat(ibat_limit);
	}
	return ibat_limit;
}

int charger_controller_main(int requester)
{
	pr_cc(PR_DEBUG, "charger_controller_main (requester=%d)\n",
		requester);

	if (!cc_init_ok)
		return 0;

	if (requester == REQUEST_BY_POWER_SUPPLY_CHANGED) {
		update_pm_psy_status(requester);
		set_charger_control_current(chgr_contr->current_iusb_limit[IUSB_NODE_NO], requester);
	}

	if (requester == REQUEST_BY_IBAT_LIMIT)
		set_charger_control_current(chgr_contr->current_ibat_limit[IBAT_NODE_NO],
			requester);

	if (requester == REQUEST_BY_IUSB_LIMIT)
		set_charger_control_current(chgr_contr->current_iusb_limit[IUSB_NODE_NO], requester);

#ifdef CONFIG_LGE_PM_QC20_SCENARIO
	if (requester == REQUEST_BY_QC20)
		set_charger_control_current(chgr_contr->current_iusb_limit[IUSB_NODE_NO],
			requester);
#endif

	return 0;
}

static void charger_contr_notify_worker(struct work_struct *work)
{
	struct charger_contr *chip = container_of(work,
				struct charger_contr,
				cc_notify_work);

	charger_controller_main(chip->requester);
}

void changed_by_power_source(int requester)
{
	chgr_contr->requester = requester;
	schedule_work(&chgr_contr->cc_notify_work);
}

#ifdef CONFIG_LGE_PM_UNIFIED_WLC
void changed_by_wireless_psy(void)
{
	bool wireless_present = is_wireless_present();

	wireless_interrupt_handler(wireless_present);
	pr_cc(PR_INFO, "[WLC] changed wireless state = %d\n", wireless_present);
}
#endif

#ifdef CONFIG_LGE_PM_VFLOAT_CHANGE
#define VFLOAT_ADJUSTMENT 50
#endif
#ifdef CONFIG_LGE_PM_LLK_MODE
#define LLK_MAX_THR_SOC 35
#define LLK_MIN_THR_SOC 30
#else
#define LLK_MAX_THR_SOC 75
#define LLK_MIN_THR_SOC 70
#endif
#ifdef CONFIG_LGE_PM_VZW_REQ
#define VZW_CHG_MAX_CURRENT	2100
#define VZW_CHG_MIN_CURRENT	400
#define VZW_SLOW_CHARGER_RESET_COUNT	5
#define VZW_SLOW_CHARGER_MAX_COUNT	27000
#endif
void changed_by_batt_psy(void)
{
	int batt_status = POWER_SUPPLY_STATUS_DISCHARGING;
	union power_supply_propval val = {0,};
	bool wireless_present, usb_present, charger_present;
#ifdef CONFIG_LGE_PM_VZW_REQ
	char *usb_type_name = get_usb_type();
	int vzw_aicl_complete;
#endif
#ifdef CONFIG_LGE_PM_LLK_MODE
	int capacity_level;
#endif

	wireless_present = is_wireless_present();
	usb_present = is_usb_present();
	/* update battery status information */
	cc_psy_getprop(batt_psy, STATUS, &val);
	batt_status = val.intval;

	if (chgr_contr->battery_status_cc != batt_status) {
		pr_cc(PR_INFO, "changed batt_status_cc = (%d) -> (%d)\n",
			chgr_contr->battery_status_cc, batt_status);
		chgr_contr->battery_status_cc = batt_status;
	}

	if (chgr_contr->batt_eoc == 1) {
#ifdef CONFIG_LGE_PM_UNIFIED_WLC
		if (wireless_present)
			wireless_chg_term_handler();
#endif
	}

	charger_present = wireless_present || usb_present;

	if (!charger_present || (charger_present && chgr_contr->batt_eoc == 1)) {
		if (wake_lock_active(&chgr_contr->chg_wake_lock)) {
			pr_cc(PR_INFO, "chg_wake_unlocked\n");
			wake_unlock(&chgr_contr->chg_wake_lock);
		}
#ifdef CONFIG_LGE_PM_VFLOAT_CHANGE
		cc_psy_setprop(batt_psy, VOLTAGE_MAX, chgr_contr->vfloat_mv
							+ VFLOAT_ADJUSTMENT);
#endif
	} else if(chgr_contr->batt_eoc != 2) {
		if (!wake_lock_active(&chgr_contr->chg_wake_lock)) {
			pr_cc(PR_INFO, "chg_wake_locked\n");
			wake_lock(&chgr_contr->chg_wake_lock);
		}
#ifdef CONFIG_LGE_PM_VFLOAT_CHANGE
		cc_psy_setprop(batt_psy, VOLTAGE_MAX, chgr_contr->vfloat_mv);
#endif
	}

	cc_psy_getprop(batt_psy, INPUT_CURRENT_SETTLED, &val);
#ifdef CONFIG_LGE_PM_VZW_REQ
	vzw_aicl_complete = val.intval;
#endif
	if (val.intval) {
		cc_psy_getprop(batt_psy, INPUT_CURRENT_TRIM, &val);
		chgr_contr->aicl_done_current = val.intval;
		pr_cc(PR_INFO, "aicl_done_current = %d\n", val.intval);
	}
#ifdef CONFIG_LGE_PM_VZW_REQ
	/*floated charger detect*/
	if (!strcmp(usb_type_name, "USB")) {
		pr_cc(PR_INFO, "get usb type for"\
			"float charger detect = %s\n", usb_type_name);
		if (chgr_contr->usb_psy->is_floated_charger) {
			chgr_contr->vzw_chg_mode = VZW_NOT_CHARGING;
			pr_cc(PR_INFO, "float charger detect = %d\n",
				chgr_contr->usb_psy->is_floated_charger);
			cc_psy_setprop(batt_psy, BATTERY_CHARGING_ENABLED, 0);
		} else {
			chgr_contr->vzw_chg_mode = VZW_NO_CHARGER;
			pr_cc(PR_INFO, "float charger value = %d\n",
				chgr_contr->usb_psy->is_floated_charger);
		}
	}
	/* Under current charger and normal charger detect */
	if (vzw_aicl_complete) {
		if (chgr_contr->aicl_done_current <= VZW_CHG_MIN_CURRENT) {
			chgr_contr->vzw_under_current_count++;
			/* Occasionally, once the code below taken to correct the error value is 300 AICL Done. */
			/* VZW_SLOW_CHARGER_MAX_COUNT : The approximate time when 400mA charge*/
			/* VZW_SLOW_CHARGER_RESET_COUNT : Maximum frequency of aicl malfunction*/
			if (chgr_contr->vzw_under_current_count == VZW_SLOW_CHARGER_MAX_COUNT)
				chgr_contr->vzw_under_current_count = VZW_SLOW_CHARGER_RESET_COUNT;
			pr_cc(PR_INFO, "under current count = %d\n", chgr_contr->vzw_under_current_count);
		}
		if ((chgr_contr->aicl_done_current <= VZW_CHG_MAX_CURRENT &&
					chgr_contr->vzw_under_current_count < 5) ||
					(chgr_contr->vzw_under_current_count >= 5 &&
					chgr_contr->aicl_done_current > 400)) {
			chgr_contr->vzw_chg_mode = VZW_NORMAL_CHARGING;
		} else if (chgr_contr->vzw_under_current_count >= 5) {
				chgr_contr->vzw_chg_mode = VZW_UNDER_CURRENT_CHARGING;
				pr_cc(PR_INFO, "under current set count= %d\n", chgr_contr->vzw_under_current_count);
		} else {
			chgr_contr->vzw_chg_mode = VZW_NOT_CHARGING;
			pr_cc(PR_INFO, "Can's set VZW_CHG_STATE\n");
			return;
		}
	}
	if (!strcmp(usb_type_name, "Unknown")) {
		chgr_contr->vzw_under_current_count = 0;
		pr_cc(PR_INFO, "under current set count= %d\n", chgr_contr->vzw_under_current_count);
	}
	pr_cc(PR_INFO, "Set VZW_CHG_STATE = %d :"\
			" 0=NO_CHAGER,1=NORMAL, 2=NOT_CHARGING,"\
				"3=UNDER_CURRENT\n", chgr_contr->vzw_chg_mode);
	power_supply_changed(chgr_contr->cc_psy);
#endif

#ifdef CONFIG_LGE_PM_LLK_MODE
	/* LLK mode code */
	cc_psy_getprop(cc_psy, STORE_DEMO_ENABLED, &val);
	if (val.intval) {
		cc_psy_getprop(usb_psy, PRESENT, &val);
		if (val.intval) {
			cc_psy_getprop(batt_psy, CAPACITY, &val);
			capacity_level = get_prop_fuelgauge_capacity();
			if (capacity_level > LLK_MAX_THR_SOC) {
#ifdef CONFIG_LGE_PM_SMB_PROP
				cc_psy_setprop(batt_psy,
					BATTERY_CHARGING_ENABLED, 0);
#else
				cc_psy_setprop(batt_psy,
					CHARGING_ENABLED, 0);
#endif
				pr_cc(PR_INFO, "Stop charging by LLK_mode. "
						"capacity[%d]\n",
						capacity_level);
			}
			if (capacity_level < LLK_MIN_THR_SOC) {
#ifdef CONFIG_LGE_PM_SMB_PROP
				cc_psy_setprop(batt_psy,
					BATTERY_CHARGING_ENABLED, 1);
#else
				cc_psy_setprop(batt_psy,
					CHARGING_ENABLED, 1);
#endif
				pr_cc(PR_INFO, "Start charging by LLK_mode. "
						"capacity[%d]\n",
						capacity_level);
			}
		}
	}
	return;
#endif
}

int notify_charger_controller(struct power_supply *psy, int requester)
{
#ifdef CONFIG_LGE_PM_QC20_SCENARIO
	union power_supply_propval val = {0,};
	int rc;
#endif

	if (!cc_init_ok)
		return -EFAULT;

	pr_cc(PR_DEBUG, "notify_charger_controller(%s)(%d)\n", psy->name, requester);

	if ((!strcmp(psy->name, USB_PSY_NAME)) ||
		(!strcmp(psy->name, WIRELESS_PSY_NAME)) ||
		(!strcmp(psy->name, CC_PSY_NAME))) {
		changed_by_power_source(requester);
	}

	if (!strcmp(psy->name, BATT_PSY_NAME)) {
		changed_by_batt_psy();
	}
#ifdef CONFIG_LGE_PM_UNIFIED_WLC
	if (!strcmp(psy->name, WIRELESS_PSY_NAME)) {
		changed_by_wireless_psy();
	}
#endif

	/* if now usb is QC20, current should be set by QC20 value.*/
#ifdef CONFIG_LGE_PM_QC20_SCENARIO
	if (!strcmp(psy->name, USB_PSY_NAME) && !chgr_contr->qc20.is_qc20) {
		rc = cc_psy_getprop(usb_psy, TYPE, &val);
		if (rc) {
			pr_cc(PR_ERR, "[QC20] TYPE getprop error =%d\n", rc);
			return 0;
		}

		if (val.intval == POWER_SUPPLY_TYPE_USB_HVDCP) {
			pr_cc(PR_INFO, "[QC20] Detecting QC 2.0.\n");
			chgr_contr->qc20.is_qc20 = 1;
			schedule_delayed_work(&chgr_contr->highvol_check_work,
				msecs_to_jiffies(100));
		} else {
			chgr_contr->qc20.is_qc20 = 0;
		}
	}
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(notify_charger_controller);

static int set_ibat_limit(const char *val, struct kernel_param *kp)
{
	int ret;

	if (!cc_init_ok)
		return 0;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_cc(PR_ERR, "error setting value %d\n", ret);
		return ret;
	}
	pr_cc(PR_ERR, "set iBAT limit current(%d)\n", cc_ibat_limit);

	if (chgr_contr->usb_online) {
		notify_charger_controller(&chgr_contr->charger_contr_psy,
			REQUEST_BY_IBAT_LIMIT);
	}
	return 0;

}
module_param_call(cc_ibat_limit, set_ibat_limit,
	param_get_int, &cc_ibat_limit, 0644);

void set_iusb_limit_cc(int value)
{
	chgr_contr->current_iusb_limit[IUSB_NODE_1A_TA] = value;
	set_charger_control_current(value,
		REQUEST_BY_IUSB_LIMIT);
}
EXPORT_SYMBOL_GPL(set_iusb_limit_cc);

static int set_iusb_limit(const char *val, struct kernel_param *kp)
{
	int ret;

	if (!cc_init_ok)
		return 0;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_cc(PR_ERR, "error setting value %d\n", ret);
		return ret;
	}
	pr_cc(PR_INFO, "set iUSB limit current(%d)\n", cc_iusb_limit);

	chgr_contr->current_iusb_limit[IUSB_NODE_THERMAL] = cc_iusb_limit;
	if (chgr_contr->usb_online || chgr_contr->wlc_online) {
		notify_charger_controller(&chgr_contr->charger_contr_psy,
			REQUEST_BY_IUSB_LIMIT);
	}
	return 0;
}
module_param_call(cc_iusb_limit, set_iusb_limit,
	param_get_int, &cc_iusb_limit, 0644);

static int set_wireless_limit (const char *val, struct kernel_param *kp)
{
	int ret;

	if(!cc_init_ok)
		return 0;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_cc(PR_ERR, "error setting value %d\n", ret);
		return ret;
	}

	pr_cc(PR_INFO, "set Wireless limit current(%d)\n", cc_wireless_limit);

	if (!chgr_contr->usb_online && chgr_contr->wlc_online) {
		notify_charger_controller(&chgr_contr->charger_contr_psy,
			REQUEST_BY_IBAT_LIMIT);
	}

	return 0;
}
module_param_call(cc_wireless_limit, set_wireless_limit,
	param_get_int, &cc_wireless_limit, 0644);

static int cc_batt_temp_state;
static int update_battery_temp(const char *val, struct kernel_param *kp)
{
	int ret;

	if (!cc_init_ok)
		return 0;

	pr_cc(PR_ERR, "not use thermal-engine\n");
	return 0;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_cc(PR_ERR, "error setting value %d\n", ret);
		return ret;
	}
	pr_cc(PR_INFO, "Changed battery temp state(%d)\n", cc_batt_temp_state);

	chgr_contr->batt_temp_state = cc_batt_temp_state;
	if (cc_batt_temp_state == CC_BATT_TEMP_STATE_NODEFINED)
		get_init_condition_theraml_engine(chgr_contr->batt_temp,
				chgr_contr->batt_volt);

	update_thermal_condition(CC_BATT_TEMP_CHANGED);

	return 0;
}
module_param_call(cc_batt_temp_state, update_battery_temp,
	param_get_int, &cc_batt_temp_state, 0644);

static int cc_batt_volt_state;
static int update_battery_volt(const char *val, struct kernel_param *kp)
{
	int ret;

	if (!cc_init_ok)
		return 0;

	pr_cc(PR_ERR, "not use thermal-engine\n");
	return 0;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_cc(PR_ERR, "error setting value %d\n", ret);
		return ret;
	}
	pr_cc(PR_INFO, "Change battery volt (%d)\n", cc_batt_volt_state);

	chgr_contr->batt_volt_state = cc_batt_volt_state;
	update_thermal_condition(CC_BATT_VOLT_CHANGED);

	return 0;
}
module_param_call(cc_batt_volt_state, update_battery_volt,
	param_get_int, &cc_batt_volt_state, 0644);

#ifdef CONFIG_LGE_PM_QC20_SCENARIO
static void cc_highvol_check_work(struct work_struct *work)
{
	// mostly copy from func. check_usbin_evp_chg()
	int usbin_vol = 0;
	struct charger_contr *chip =
			container_of(work, struct charger_contr, highvol_check_work.work);

	if (chip->qc20.check_count >= QC20_RETRY_COUNT) {
		pr_cc(PR_INFO, "[QC20]  failed to set QC2.0 high voltage, input voltage is 5V\n");
		/* QC20 ta plugged */
		chip->qc20.check_count = 0;
		return;
	}

	usbin_vol = cc_get_usb_adc();
	if (usbin_vol > QC20_USBIN_VOL_THRSHD) {
		pr_cc(PR_INFO, "[QC20] usbin_vol %d is over %d\n",
			usbin_vol, QC20_USBIN_VOL_THRSHD);
		chip->qc20.check_count = 0;
		chip->qc20.is_highvol= 1;
		notify_charger_controller(&chip->charger_contr_psy,
			REQUEST_BY_QC20);
	}
	else  {
		pr_cc(PR_INFO, "[QC20] usbin_vol %d is under %d\n",
			usbin_vol, QC20_USBIN_VOL_THRSHD);
		chip->qc20.check_count++;
		schedule_delayed_work(&chip->highvol_check_work,
			MONITOR_USBIN_QC20_CHG);
	}
}

static int set_ibat_qc20_limit (const char *val, struct kernel_param *kp)
{
	int ret;

	if (!cc_init_ok)
		return 0;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_cc(PR_ERR, "error setting value %d\n", ret);
		return ret;
	}
	pr_cc(PR_INFO, "[QC20] set iBAT limit current(%d)\n", cc_ibat_qc20_limit);

	if (chgr_contr->qc20.is_qc20 && chgr_contr->qc20.is_highvol) {
		notify_charger_controller(&chgr_contr->charger_contr_psy,
			REQUEST_BY_IBAT_LIMIT);
	}
	return 0;

}
module_param_call(cc_ibat_qc20_limit, set_ibat_qc20_limit,
	param_get_int, &cc_ibat_qc20_limit, 0644);

static int quick_charging_state;
static int set_quick_charging_state(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_cc(PR_ERR, "[QC20] qc_state error = %d\n", ret);
		return ret;
	}

	switch (quick_charging_state) {
	case QC20_STATUS_NONE:
		break;

	case QC20_STATUS_LCD_ON:
		chgr_contr->qc20.status  |= QC20_LCD_STATE;
		break;

	case QC20_STATUS_LCD_OFF:
		chgr_contr->qc20.status &= ~QC20_LCD_STATE;
		break;

	case QC20_STATUS_CALL_ON:
		chgr_contr->qc20.status  |= QC20_CALL_STATE;
		break;

	case QC20_STATUS_CALL_OFF:
		chgr_contr->qc20.status  &= ~QC20_CALL_STATE;
		break;
	default:
		pr_cc(PR_ERR, "[QC20] qpnp_qc20_state_update state err\n");
	}
	pr_cc(PR_INFO, "[QC20] set_quick_charging_state %d, status %d\n",
		quick_charging_state, chgr_contr->qc20.status);

	if (chgr_contr->qc20.status)
		chgr_contr->qc20.current_status = QC20_CURRENT_LIMMITED;
	else
		chgr_contr->qc20.current_status = QC20_CURRENT_NORMAL;

	if (chgr_contr->qc20.is_qc20 && chgr_contr->qc20.is_highvol)
		notify_charger_controller(&chgr_contr->charger_contr_psy,
			REQUEST_BY_QC20);

	return 0;
}
module_param_call(quick_charging_state, set_quick_charging_state,
	param_get_int, &quick_charging_state, 0644);
#endif

#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
int battery_id_check(void)
{
	int rc;
	union power_supply_propval val = {0,};

	rc = cc_psy_getprop(batt_id_psy, BATTERY_ID, &val);
	if (rc) {
		pr_cc(PR_ERR, "%s : fail to get BATTERY_ID from battery_id."
			"rc = %d.\n", __func__, rc);
		return 0;
	} else {
		chgr_contr->batt_id_smem = val.intval;
	}

	if (chgr_contr->batt_id_smem == BATT_NOT_PRESENT)
		chgr_contr->batt_smem_present = 0;
	else
		chgr_contr->batt_smem_present = 1;

#ifdef CONFIG_MACH_MSM8992_P1
	/*  P1 use the battery of 4.35V for developer,
	  * but the battery of customer is 4.4V.
	  *  So, the device must update vfloat_mv to 4.35V in battery of developer.
	  */
	if(chgr_contr->batt_id_smem  == BATT_ID_DS2704_N ||
		chgr_contr->batt_id_smem  == BATT_ID_DS2704_L ||
		chgr_contr->batt_id_smem  == BATT_ID_DS2704_C ||
		chgr_contr->batt_id_smem  == BATT_ID_ISL6296_N ||
		chgr_contr->batt_id_smem  == BATT_ID_ISL6296_L ||
		chgr_contr->batt_id_smem  == BATT_ID_ISL6296_C) {
		rc = cc_psy_setprop(batt_psy, VOLTAGE_MAX, 4350);
		if (rc) {
			pr_cc(PR_ERR, "%s : fail to set VOLTAGE_MAX at battery."
				"rc = %d.\n", __func__, rc);
		}
	}
#endif

	return 0;
}
#endif

#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
static ssize_t at_chg_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int r;
	bool b_chg_ok = false;
	int chg_type;
	union power_supply_propval val = {0,};

	cc_psy_getprop(batt_psy, CHARGE_TYPE, &val);
	chg_type = val.intval;

	if (chg_type != POWER_SUPPLY_CHARGE_TYPE_NONE) {
		b_chg_ok = true;
		r = snprintf(buf, 3, "%d\n", b_chg_ok);
		pr_cc(PR_INFO, "[Diag] true ! buf = %s, charging=1\n", buf);
	} else {
		b_chg_ok = false;
		r = snprintf(buf, 3, "%d\n", b_chg_ok);
		pr_cc(PR_INFO, "[Diag] false ! buf = %s, charging=0\n", buf);
	}

	return r;
}

static ssize_t at_chg_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret = 0;
	int intval;

	if (strncmp(buf, "0", 1) == 0) {
		/* stop charging */
		pr_cc(PR_INFO, "[Diag] stop charging start\n");
		intval = 0;

	} else if (strncmp(buf, "1", 1) == 0) {
		/* start charging */
		pr_cc(PR_INFO, "[Diag] start charging start\n");
		intval = 1;
	}

#ifdef CONFIG_LGE_PM_SMB_PROP
	cc_psy_setprop(batt_psy, BATTERY_CHARGING_ENABLED, intval);
#else
	cc_psy_setprop(batt_psy, CHARGING_ENABLED, intval);
#endif

	if (ret)
		return -EINVAL;

	return 1;
}

static ssize_t at_chg_complete_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int guage_level = get_prop_fuelgauge_capacity();

	if (guage_level == 100) {
		ret = snprintf(buf, 3, "%d\n", 0);
		pr_cc(PR_INFO, "[Diag] buf = %s, gauge==100\n", buf);
	} else {
		ret = snprintf(buf, 3, "%d\n", 1);
		pr_cc(PR_INFO, "[Diag] buf = %s, gauge<=100\n", buf);
	}

	return ret;
}

static ssize_t at_chg_complete_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret = 0;
	int intval;

	if (strncmp(buf, "0", 1) == 0) {
		/* charging not complete */
		pr_cc(PR_INFO, "[Diag] charging not complete start\n");
		intval = 1;
	} else if (strncmp(buf, "1", 1) == 0) {
		/* charging complete */
		pr_cc(PR_INFO, "[Diag] charging complete start\n");
		intval = 0;
	}

#ifdef CONFIG_LGE_PM_SMB_PROP
	cc_psy_setprop(batt_psy, BATTERY_CHARGING_ENABLED, intval);
#else
	cc_psy_setprop(batt_psy, CHARGING_ENABLED, intval);
#endif
	if (ret)
		return -EINVAL;

	return 1;
}

static ssize_t at_pmic_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r = 0;
	bool pm_reset = true;

	msleep(3000); /* for waiting return values of testmode */

	machine_restart(NULL);

	r = snprintf(buf, 3, "%d\n", pm_reset);

	return r;
}

#ifdef CONFIG_LGE_PM_AT_OTG_SUPPORT
static ssize_t at_otg_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int otg_mode;
	int r = 0;
	union power_supply_propval val = {0,};

	cc_psy_getprop(batt_psy, OTG_MODE, &val);
	otg_mode = val.intval;
	if (otg_mode) {
		otg_mode = 1;
		r = snprintf(buf, 3, "%d\n", otg_mode);
		pr_cc(PR_INFO, "[Diag] true ! buf = %s, OTG Enabled\n", buf);
	} else {
		otg_mode = 0;
		r = snprintf(buf, 3, "%d\n", otg_mode);
		pr_cc(PR_INFO, "[Diag] false ! buf = %s, OTG Disabled\n", buf);
	}
	return r;
}

static ssize_t at_otg_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret = 0;
	int intval;

	if (strncmp(buf, "0", 1) == 0) {
		pr_cc(PR_INFO, "[Diag] OTG Disable start\n");
		intval = 0;
	} else if (strncmp(buf, "1", 1) == 0) {
		pr_cc(PR_INFO, "[Diag] OTG Enable start\n");
		intval = 1;
	}

	cc_psy_setprop(batt_psy, OTG_MODE, intval);

	if (ret)
		return -EINVAL;
	return 1;
}
DEVICE_ATTR(at_otg, 0644, at_otg_status_show, at_otg_status_store);
#endif

DEVICE_ATTR(at_charge, 0644, at_chg_status_show, at_chg_status_store);
DEVICE_ATTR(at_chcomp, 0644, at_chg_complete_show, at_chg_complete_store);
DEVICE_ATTR(at_pmrst, 0440, at_pmic_reset_show, NULL);
#endif

/* #ifdef CONFIG_LGE_BATTERY_PROP */
#define DEFAULT_FAKE_BATT_CAPACITY		50
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
#define LGE_FAKE_BATT_PRES		1
#endif

static int get_prop_fuelgauge_capacity(void)
{
	union power_supply_propval val = {0, };
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	union power_supply_propval usb_online = {0, };
	int rc;
#endif

	if (chgr_contr->fg_psy == NULL) {
		pr_err("\n%s: fg_psy == NULL\n", __func__);
		chgr_contr->fg_psy = power_supply_get_by_name(chgr_contr->fg_psy_name);
		if (chgr_contr->fg_psy == NULL) {
			pr_err("[ChargerController]########## Not Ready(fg_psy)\n");
			return DEFAULT_FAKE_BATT_CAPACITY;
		}
	}

#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	if (pseudo_batt_info.mode) {
		rc = cc_psy_getprop(fg_psy, CAPACITY, &val);
		if (rc)
			val.intval = DEFAULT_FAKE_BATT_CAPACITY;
		rc = cc_psy_getprop(usb_psy, ONLINE, &usb_online);
		if (rc)
			usb_online.intval = 0;

		if (usb_online.intval == 0 && val.intval == 0) {
			pr_cc(PR_INFO, "CAPACITY[%d], USB_ONLINE[%d]. "
					"System will power off\n",
					val.intval, usb_online.intval);
			return val.intval;
		} else {
			return pseudo_batt_info.capacity;
		}
	}
#endif

	if (!cc_psy_getprop(fg_psy, CAPACITY, &val))
		return val.intval;
	else
		return DEFAULT_FAKE_BATT_CAPACITY;
}

#ifdef CONFIG_LGE_PM_USB_ID
static bool is_factory_cable(void)
{
	unsigned int cable_info;
	cable_info = lge_pm_get_cable_type();

	if (cable_info == CABLE_56K ||
		cable_info == CABLE_130K ||
		cable_info == CABLE_910K)
		return true;
	else
		return false;
}
#endif

int lge_battery_get_property(enum power_supply_property prop,
				       union power_supply_propval *val)
{
	if (!cc_init_ok)
		return 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		if (chgr_contr->battery_status_charger != val->intval) {
			pr_cc(PR_INFO, "changed battery_status_charger = (%d) -> (%d)\n",
				chgr_contr->battery_status_charger,
				val->intval);
			chgr_contr->battery_status_charger = val->intval;
			if (val->intval == POWER_SUPPLY_STATUS_FULL)
				pr_cc(PR_INFO, "End Of Charging!\n");
		}

		if (val->intval == POWER_SUPPLY_STATUS_FULL){
			chgr_contr->batt_eoc = 1;
		} else if (val->intval == POWER_SUPPLY_STATUS_DISCHARGING){
			chgr_contr->batt_eoc = 2;
		} else {
			chgr_contr->batt_eoc = 0;
		}
		if ((get_prop_fuelgauge_capacity() >= 100) &&
				(is_usb_present()||is_wireless_present())) {
			val->intval = POWER_SUPPLY_STATUS_FULL;
			pr_cc(PR_DEBUG, "Battery full\n");
		}
		/* todo */
		if ((chgr_contr->batt_temp_state == CC_BATT_TEMP_STATE_HIGH) &&
					(is_usb_present() || is_wireless_present())) {
			pr_cc(PR_DEBUG, "over 45 temp and over 4.0V,"
					"pseudo charging\n");
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
#if defined(CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY) && defined(CONFIG_LGE_PM_USB_ID)
		if (pseudo_batt_info.mode && !is_factory_cable()) {
			val->intval = LGE_FAKE_BATT_PRES;
			pr_cc(PR_DEBUG, "Fake battery is exist\n");
		}
#endif
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		chgr_contr->charge_type = val->intval;
		pr_cc(PR_INFO, "charge_type = %d\n", chgr_contr->charge_type);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_prop_fuelgauge_capacity();
		pr_cc(PR_DEBUG, "Battery Capacity = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		chgr_contr->origin_batt_temp = val->intval;
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
		if (pseudo_batt_info.mode) {
			val->intval = pseudo_batt_info.temp;
			pr_cc(PR_DEBUG, "Temp is %d in Fake battery"
						"\n", val->intval);
		}
#endif
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
		if (pseudo_batt_info.mode) {
				val->intval = pseudo_batt_info.volt * 1000;
		}
#endif
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (chgr_contr->batt_temp_state == CC_BATT_TEMP_STATE_OVERHEAT)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (chgr_contr->batt_temp_state == CC_BATT_TEMP_STATE_COLD)
			val->intval = POWER_SUPPLY_HEALTH_COLD;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(lge_battery_get_property);



static int chargercontroller_parse_dt(struct device_node *dev_node, struct charger_contr *cc)
{
	int ret;

#ifdef CONFIG_LGE_PM_QC20_SCENARIO
	int i;
	u32 current_value[2];

	of_property_read_u32_array(dev_node, "lge,chargercontroller-iusb-qc20",
			current_value, 2);
	for (i = 0; i < QC20_CURRENT_MAX; i++)
		cc->qc20.iusb[i] = current_value[i];

	of_property_read_u32_array(dev_node, "lge,chargercontroller-ibat-qc20",
			current_value, 2);
	for (i = 0; i < QC20_CURRENT_MAX; i++)
		cc->qc20.ibat[i] = current_value[i];

	pr_cc(PR_DEBUG, "[QC20]  iusb-qc20[%d %d], ibat-qc20[%d %d]\n",
			cc->qc20.iusb[0], cc->qc20.iusb[1],
			cc->qc20.ibat[0], cc->qc20.ibat[1]);
#endif
	ret = of_property_read_u32(dev_node,
		"lge,chargercontroller-current-ibat-max",
		&(cc->current_ibat));
	pr_cc(PR_DEBUG, "current_ibat = %d from DT\n",
				cc->current_ibat);
	if (ret) {
		pr_cc(PR_ERR, "Unable to read current_ibat_max. Set 1600.\n");
		cc->current_ibat = 1600;
	}

	ret = of_property_read_u32(dev_node, "lge,chargercontroller-current-limit",
				   &(cc->current_limit));
	pr_cc(PR_DEBUG, "current_limit = %d from DT\n",
				cc->current_limit);
	if (ret) {
		pr_cc(PR_ERR, "Unable to read current_limit. Set 450.\n");
		cc->current_limit = 450;
	}

	ret = of_property_read_u32(dev_node, "lge,chargercontroller-current-wlc-limit",
				   &(cc->current_wlc_limit));
	pr_cc(PR_DEBUG, "current_wlc_limit = %d from DT\n",
				cc->current_wlc_limit);
	if (ret) {
		pr_cc(PR_ERR, "Unable to read current_wlc_limit. Set 500.\n");
		cc->current_wlc_limit = 500;
	}

	ret = of_property_read_u32(dev_node, "lge,chargercontroller-current-ibat-max-wireless",
				   &(cc->current_ibat_max_wireless));
	pr_cc(PR_DEBUG, "current-ibat-max-wireless = %d from DT\n",
				cc->current_ibat_max_wireless);
	if (ret) {
		pr_cc(PR_ERR, "Unable to read current-ibat-max-wireless. Set 1200.\n");
		cc->current_ibat_max_wireless = 1200;
	}

	ret = of_property_read_u32(dev_node,
			"lge,chargercontroller-current-iusb-factory",
				   &(cc->current_iusb_factory));
	pr_cc(PR_DEBUG, "current_iusb_factory = %d from DT\n",
				cc->current_iusb_factory);
	if (ret) {
		pr_cc(PR_ERR, "Unable to read current_iusb_factory. Set 1500.\n");
		cc->current_iusb_factory = 1500;
	}

	ret = of_property_read_u32(dev_node,
			"lge,chargercontroller-current-ibat-factory",
				   &(cc->current_ibat_factory));
	pr_cc(PR_DEBUG, "current_ibat_factory = %d from DT\n",
				cc->current_ibat_factory);
	if (ret) {
		pr_cc(PR_ERR, "Unable to read current_ibat_factory. Set 500.\n");
		cc->current_ibat_factory = 500;
	}

	/* read the fuelgauge power supply name */
	ret = of_property_read_string(dev_node, "lge,fuelgauge-psy-name",
						&cc->fg_psy_name);
	if (ret)
		cc->fg_psy_name = FUELGAUGE_PSY_NAME;
	return ret;
}

void get_init_condition_theraml_engine(int batt_temp, int batt_volt)
{
	int temp;

	if (batt_temp == 0) {
		temp = charger_contr_get_battery_temperature();
		if (temp != 0)
			chgr_contr->batt_temp = temp / 10;
		else
			chgr_contr->batt_temp = 0;
	} else
		chgr_contr->batt_temp = batt_temp;

#ifdef CONFIG_MACH_MSM8992_P1_SPR_US
	if (chgr_contr->batt_temp < -5)
#else
	if (chgr_contr->batt_temp < -10)
#endif
		chgr_contr->batt_temp_state = CC_BATT_TEMP_STATE_COLD;
#ifdef CONFIG_MACH_MSM8992_P1_SPR_US
	else if (chgr_contr->batt_temp >= -2 &&
#else
	else if (chgr_contr->batt_temp >= -5 &&
#endif
		chgr_contr->batt_temp <= 42)
		chgr_contr->batt_temp_state = CC_BATT_TEMP_STATE_NORMAL;
	else if (chgr_contr->batt_temp >= 45 && chgr_contr->batt_temp <= 52)
		chgr_contr->batt_temp_state = CC_BATT_TEMP_STATE_HIGH;
	else if (chgr_contr->batt_temp >= 55)
		chgr_contr->batt_temp_state = CC_BATT_TEMP_STATE_OVERHEAT;

	if (batt_volt == 0)
#if defined(CONFIG_BATTERY_MAX17048)
		chgr_contr->batt_volt = max17048_get_voltage();
#else
		chgr_contr->batt_volt = CHARGER_CONTR_VOLTAGE_3_7V;
#endif
	else
		chgr_contr->batt_volt = batt_volt;

	if (chgr_contr->batt_volt == 3999) /* default batt_volt_state */
		chgr_contr->batt_volt_state = CC_BATT_VOLT_UNDER_4_0;
	if (chgr_contr->batt_volt <= 3950)
		chgr_contr->batt_volt_state = CC_BATT_VOLT_UNDER_4_0;
	else if (chgr_contr->batt_volt >= 4050)
		chgr_contr->batt_volt_state = CC_BATT_VOLT_OVER_4_0;

	pr_cc(PR_INFO, "change batt_temp[%d], batt_temp_state[%d], "
			"change batt_volt[%d], batt_volt_state[%d]\n",
			chgr_contr->batt_temp,
			chgr_contr->batt_temp_state,
			chgr_contr->batt_volt,
			chgr_contr->batt_volt_state);
}

#define LT_CABLE_56K		6
#define LT_CABLE_130K		7
#define LT_CABLE_910K		11
static int charger_contr_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct charger_contr *cc;
	union power_supply_propval val = {0,};
	struct device_node *dev_node = pdev->dev.of_node;
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	unsigned int cable_type;
	unsigned int cable_smem_size;
	unsigned int *p_cable_type = (unsigned int *)
		(smem_get_entry(SMEM_ID_VENDOR1, &cable_smem_size, 0, 0));
#endif

	pr_cc(PR_INFO, "charger_contr_probe start\n");

#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	if (p_cable_type) {
		cable_type = *p_cable_type;
	} else {
		cable_type = 0;
	}

	if (cable_type == LT_CABLE_56K || cable_type == LT_CABLE_130K ||
						cable_type == LT_CABLE_910K) {
		pseudo_batt_info.mode = 1;
	}
	pr_info("cable_type = %d, pseudo_batt_info.mode = %d\n", cable_type,
							pseudo_batt_info.mode);
#endif

	cc = kzalloc(sizeof(struct charger_contr), GFP_KERNEL);

	if (!cc) {
		pr_cc(PR_ERR, "failed to alloc memory\n");
		return -ENOMEM;
	}

	chgr_contr = cc;

	wake_lock_init(&cc->chg_wake_lock,
			WAKE_LOCK_SUSPEND, "charging_wake_lock");

	cc->dev = &pdev->dev;

	cc->usb_psy = power_supply_get_by_name(USB_PSY_NAME);
	if (cc->usb_psy == NULL) {
		pr_cc(PR_ERR, "Not Ready(usb_psy)\n");
		ret =	-EPROBE_DEFER;
		goto error;
	}

	cc->batt_psy = power_supply_get_by_name(BATT_PSY_NAME);
	if (cc->batt_psy == NULL) {
		pr_cc(PR_ERR, "Not Ready(batt_psy)\n");
		ret =	-EPROBE_DEFER;
		goto error;
	}

	cc->wireless_psy = power_supply_get_by_name(WIRELESS_PSY_NAME);
	if (cc->wireless_psy == NULL) {
		pr_cc(PR_ERR, "Not Ready(wireless_psy)\n");
		ret =	-EPROBE_DEFER;
		goto error;
	}

#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	cc->batt_id_psy = power_supply_get_by_name(BATT_ID_PSY_NAME);
	if (cc->batt_id_psy == NULL) {
		pr_cc(PR_ERR, "Not Ready(batt_id_psy)\n");
		ret =	-EPROBE_DEFER;
		goto error;
	}
#endif

if (cc->wireless_psy != NULL) {
		cc_psy_getprop(wireless_psy, CURRENT_MAX, &val);
		if (val.intval) {
			pr_info("[ChargerController] Present wireless current =%d\n", val.intval/1000);
			cc->current_wireless_psy = val.intval/1000;
		}
	}

	ret = chargercontroller_parse_dt(dev_node, cc);
	if (ret)
		pr_cc(PR_ERR, "failed to parse dt\n");


/*	cc->current_iusb_max = 0; todo check init value*/
	cc->usb_online = 0;
	cc->wlc_online = 0;
	cc->current_iusb_changed_by_cc = 0;
	cc->wireless_lcs = -1;
	cc->ibat_limit_lcs = -1;
	/* initial value needed if CONFIG_LGE_SET_INIT_CURRENT is not used */
	for (i=0; i<MAX_IBAT_NODE; i++)
		cc->current_ibat_limit[i] = -1;

	for (i=0; i<MAX_IUSB_NODE; i++)
		cc->current_iusb_limit[i] = -1;

	/* power_supply register for controlling charger(batt psy) */
	cc->charger_contr_psy.name = CC_PSY_NAME;
/* cc->charger_contr_psy.type = POWER_SUPPLY_TYPE_CHARGER_CONTROL; // ???? */
	cc->charger_contr_psy.supplied_to = pm_batt_supplied_to;
	cc->charger_contr_psy.num_supplicants = ARRAY_SIZE(pm_batt_supplied_to);
	cc->charger_contr_psy.properties = pm_power_props_charger_contr_pros;
	cc->charger_contr_psy.num_properties = ARRAY_SIZE(pm_power_props_charger_contr_pros);
	cc->charger_contr_psy.get_property = pm_get_property_charger_contr;
	cc->charger_contr_psy.set_property = pm_set_property_charger_contr;
/* cc->charger_contr_psy.get_event_property = pm_get_event_property_charger_contr; */
/* cc->charger_contr_psy.set_event_property = pm_set_event_property_charger_contr; */
	cc->charger_contr_psy.property_is_writeable = charger_contr_property_is_writerable;

	ret = power_supply_register(cc->dev, &cc->charger_contr_psy);
	if (ret < 0) {
		pr_cc(PR_ERR, "%s power_supply_register charger controller failed ret=%d\n",
			__func__, ret);
		goto error;
	}

	cc->cc_psy = power_supply_get_by_name(CC_PSY_NAME);
	if (cc->cc_psy == NULL) {
		pr_cc(PR_ERR, "Not Ready(cc_psy)\n");
		ret = -EPROBE_DEFER;
		goto error;
	}

	mutex_init(&cc->notify_work_lock);

	platform_set_drvdata(pdev, cc);

#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	battery_id_check();
#endif
#ifdef CONFIG_LGE_PM_VFLOAT_CHANGE
	ret = cc_psy_getprop(batt_psy, VOLTAGE_MAX, &val);
	if (ret) {
		pr_cc(PR_ERR, "%s : fail to get VOLTAGE_MAX at battery."
					"ret = %d.\n", __func__, ret);
		cc->vfloat_mv = 4400;
		pr_cc(PR_ERR, "%s : cc->vfloat_mv initialize %d by default.",
						 __func__, cc->vfloat_mv);
	} else {
		cc->vfloat_mv = val.intval;
	}
#endif

	INIT_WORK(&cc->cc_notify_work, charger_contr_notify_worker);
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	INIT_DELAYED_WORK(&cc->otp_work, check_charging_state);
#endif
#ifdef CONFIG_LGE_PM_CHARGE_INFO
	INIT_DELAYED_WORK(&cc->charging_inform_work, charging_information);
#endif
#ifdef CONFIG_LGE_PM_QC20_SCENARIO
	if (lge_get_boot_mode() == LGE_BOOT_MODE_NORMAL)
		cc->qc20.status  |= QC20_LCD_STATE;

	INIT_DELAYED_WORK(&cc->highvol_check_work, cc_highvol_check_work);
#endif

#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
	ret = device_create_file(cc->dev, &dev_attr_at_charge);
	if (ret < 0) {
		pr_cc(PR_ERR, "%s:File dev_attr_at_charge creation failed: %d\n",
				__func__, ret);
		ret = -ENODEV;
		goto err_at_charge;
	}

	ret = device_create_file(cc->dev, &dev_attr_at_chcomp);
	if (ret < 0) {
		pr_cc(PR_ERR, "%s:File dev_attr_at_chcomp creation failed: %d\n",
				__func__, ret);
		ret = -ENODEV;
		goto err_at_chcomp;
	}

	ret = device_create_file(cc->dev, &dev_attr_at_pmrst);
	if (ret < 0) {
		pr_cc(PR_ERR, "%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_pmrst;
	}
#ifdef CONFIG_LGE_PM_AT_OTG_SUPPORT
	ret = device_create_file(cc->dev, &dev_attr_at_otg);
	if (ret < 0) {
		pr_cc(PR_ERR, "%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_otg;
	}
#endif
#endif

	get_init_condition_theraml_engine(DEFAULT_BATT_TEMP, DEFAULT_BATT_VOLT);
	cc_init_ok = 1;
	pr_cc(PR_INFO, "charger_contr_probe done\n");

#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	otp_enabled = false;
#endif

#ifdef CONFIG_LGE_PM_CHARGE_INFO
	schedule_delayed_work(&chgr_contr->charging_inform_work,
		round_jiffies_relative(msecs_to_jiffies(CHARGING_INFORM_NORMAL_TIME)));
#endif

	return ret;
#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
#ifdef CONFIG_LGE_PM_AT_OTG_SUPPORT
err_at_otg:
	device_remove_file(cc->dev, &dev_attr_at_pmrst);
#endif
err_at_pmrst:
	device_remove_file(cc->dev, &dev_attr_at_chcomp);
err_at_chcomp:
	device_remove_file(cc->dev, &dev_attr_at_charge);
err_at_charge:
	power_supply_unregister(&cc->charger_contr_psy);
#endif
error:
	wake_lock_destroy(&cc->chg_wake_lock);
	kfree(cc);
	return ret;

}

static void charger_contr_kfree(struct charger_contr *cc)
{
	pr_cc(PR_INFO, "kfree\n");
	kfree(cc);
}

static int charger_contr_remove(struct platform_device *pdev)
{
	struct charger_contr *cc = platform_get_drvdata(pdev);
#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
	device_remove_file(cc->dev, &dev_attr_at_charge);
	device_remove_file(cc->dev, &dev_attr_at_chcomp);
	device_remove_file(cc->dev, &dev_attr_at_pmrst);
#ifdef CONFIG_LGE_PM_AT_OTG_SUPPORT
	device_remove_file(cc->dev, &dev_attr_at_otg);
#endif
#endif
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	cancel_delayed_work(&chgr_contr->otp_work);
#endif
#ifdef CONFIG_LGE_PM_CHARGE_INFO
	cancel_delayed_work(&chgr_contr->charging_inform_work);
#endif

	power_supply_unregister(&cc->charger_contr_psy);
	charger_contr_kfree(cc);
	return 0;
}

static struct of_device_id charger_contr_match_table[] = {
		{.compatible = CHARGER_CONTROLLER_NAME ,},
		{},
};

static struct platform_driver charger_contr_device_driver = {
	.probe = charger_contr_probe,
	.remove = charger_contr_remove,

	.driver = {
		.name = CHARGER_CONTROLLER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = charger_contr_match_table,
	},
};

static int __init charger_contr_init(void)
{
	return platform_driver_register(&charger_contr_device_driver);
}

static void __exit charger_contr_exit(void)
{
	platform_driver_unregister(&charger_contr_device_driver);
}

module_init(charger_contr_init);
module_exit(charger_contr_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_DEVICE_TABLE(of, charger_contr_match_table);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_VERSION(DRIVER_VERSION);
