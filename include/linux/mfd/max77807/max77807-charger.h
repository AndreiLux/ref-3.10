/*
 * Maxim MAX77807 Charger Driver Header
 *
 * Copyright (C) 2014 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/power_supply.h>
#include <linux/wakelock.h>

#ifndef __MAX77807_CHARGER_H__
#define __MAX77807_CHARGER_H__

#define MAX77807_CHARGER_CURRENT_SUSPEND  (0)
#define MAX77807_CHARGER_CURRENT_UNLIMIT  (1)
#define MAX77807_CHARGER_CURRENT_MAXIMUM  (2)
#define MAX77807_CHARGER_CURRENT_MINIMUM  (3)

#define CHG_SRC_INT_MASK_REG		0x23
#define CHG_INT_REG 				0xB0
#define CHG_INT_MASK_REG			0xB1
#define CHG_INT_OK_REG				0xB2
#define CHG_DETAILS_00_REG			0xB3
#define CHG_DETAILS_01_REG			0xB4
#define CHG_DETAILS_02_REG			0xB5
#define CHG_DETAILS_03_REG			0xB6
#define CHG_CNFG_00_REG				0xB7
#define CHG_CNFG_01_REG				0xB8
#define CHG_CNFG_02_REG				0xB9
#define CHG_CNFG_03_REG				0xBA
#define CHG_CNFG_04_REG				0xBB
#define CHG_CNFG_05_REG				0xBC
#define CHG_CNFG_06_REG				0xBD
#define CHG_CNFG_07_REG				0xBE
#define CHG_CNFG_08_REG				0xBF
#define CHG_CNFG_09_REG				0xC0
#define CHG_CNFG_10_REG				0xC1
#define CHG_CNFG_11_REG				0xC2
#define CHG_CNFG_12_REG				0xC3
#define CHG_CNFG_13_REG				0xC4
#define CHG_CNFG_14_REG				0xC5
#define SAFEOUTCTRL_REG				0xC6

extern int max77807_odin_charger_enable; /* Global Charger enable function */
struct max77807_charger_platform_data {
    int irq;

    char *psy_name;
    char *ext_psy_name;

    char **supplied_to;
    size_t num_supplicants;

	u32 fast_charge_timer;			/* in hour; disable, 4hr ~ 16hr */
    u32 fast_charge_current;        /* in uA; 0.00A ~ 1.80A */
    u32 charge_termination_voltage; /* in uV; 4.10V ~ 4.35V */
    u32 topoff_timer;               /* in min; 0min ~ 60min, infinite */
    u32 topoff_current;             /* in uA; 50mA ~ 400mA */
    u32 charge_restart_threshold;   /* in uV; 100mV ~ 150mV */

    /* Co-operating charger */
    bool enable_coop;
    char *coop_psy_name;

	/* Temperature regulation */
	bool enable_thermistor;

	/* AICL control */
	 bool enable_aicl;
	 u32 aicl_detection_voltage;     /* in uV; 3.9V ~ 4.8V  */
	 u32 aicl_reset_threshold;       /* in uV; 100mV or 200mV */

	 u32 current_limit_usb;          /* in uA; 0.00A ~ 1.80A */
	 u32 current_limit_ac;           /* in uA; 0.00A ~ 1.80A */

};


/* for use vibrator */
struct max77807_charger {
    struct mutex                           lock;
    struct max77807_dev                   *chip;
    struct max77807_io                    *io;
    struct device                         *dev;
    struct kobject                        *kobj;
    struct attribute_group                *attr_grp;
    struct max77807_charger_platform_data *pdata;
    int                                    irq;
	int                                     irq_mask;
	int                                     details_0;
	int                                     details_1;
	int                                     details_2;
    u8                                     irq1_saved;
    u8                                     irq2_saved;
    spinlock_t                             irq_lock;
    struct delayed_work                    irq_work;
    struct delayed_work                    check_cable_work;
    struct delayed_work                    check_chgin_work;
    struct delayed_work                    off_mode_work;
	struct delayed_work                    wlc_reset_work;
	struct delayed_work                    wlc_unmask_work;
	struct delayed_work                    wlc_chrg_stop_work;
    struct delayed_work                    log_work;
	struct delayed_work                    wireless_align_work;
	struct dentry  						  *dent;
	struct workqueue_struct               *chg_internal_wq;
	/* struct power_supply                    psy; */
	/* struct power_supply                   *psy_this; */
    struct power_supply                   *psy_ext;
    struct power_supply                   *psy_coop; /* cooperating charger */
	struct power_supply                    ac;
	struct power_supply                    usb;
	struct power_supply                    wireless;
	bool                                   chg_enabled;
	enum power_supply_type                 dcin_type;
    bool                                   dev_enabled;
    bool                                   dev_initialized;
    int                                    current_limit_volatile;
    int                                    current_limit_permanent;
    int                                    charge_current_volatile;
    int                                    charge_current_permanent;
    int                                    present;
    int                                    health;
    int                                    status;
    int                                    charge_type;
    int                                    vichg;
	bool                                   is_factory_cable;
	bool                                   vbus_check;
	unsigned int                           align_values;
	struct mutex                           align_lock;
	bool                                   slimport_old_info;
	bool                                   wireless_old_info;
	bool                                   off_mode;
#ifdef CONFIG_USB_ODIN_DRD
	unsigned int                           usb_init : 1;
	struct odin_usb3_otg                   *odin_otg;
	struct wakeup_source                   ws;
	struct notifier_block                  nb;
	bool                                   otg_vbus;
#endif
	struct wake_lock                       chg_wake_lock;
	struct wake_lock                       wlc_align_wakelock;
	struct wake_lock                       off_mode_lock;
#ifdef CONFIG_MAX77807_USBSW
	int                                    usbsw_irq;
	int                                    usbsw_status;
#endif
	int 				thrmgr_input_current;
	int 				thrmgr_chrg_current;
	int 				thrmgr_up_trig ;
	int				thrmgr_down_trig ;
	int 				thrmgr_batt_temp_state ;
};

void batt_temp_ctrl_start(int reset);
void d2260_hwmon_set_bat_threshold(int min, int max);
void d2260_hwmon_set_bat_threshold_max(void);
void vbus_noty(int status);
int max77807_cable_type_return(void);
void set_fake_battery_flag(bool flag);
void set_off_mode_flag(bool off_flag);
int get_voltage(void);


/* for compatibility with kernel not from android/kernel/msm */
#ifndef POWER_SUPPLY_PROP_CHARGING_ENABLED
#define POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED
#define POWER_SUPPLY_PROP_CHARGING_ENABLED \
        POWER_SUPPLY_PROP_ONLINE
#endif
#ifndef POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT
#define POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT \
        POWER_SUPPLY_PROP_CURRENT_NOW
#endif
#ifndef POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX
#define POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX \
        POWER_SUPPLY_PROP_CURRENT_MAX
#endif

#endif /* !__MAX77807_CHARGER_H__ */

