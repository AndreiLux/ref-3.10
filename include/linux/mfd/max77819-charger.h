/*
 * Maxim MAX77819 Charger Driver Header
 *
 * Copyright (C) 2013 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX77819_CHARGER_H__
#define __MAX77819_CHARGER_H__

#include <linux/usb/odin_usb3.h>

/* Constant of special cases */
#define MAX77819_CHARGER_CURRENT_SUSPEND  (0)
#define MAX77819_CHARGER_CURRENT_UNLIMIT  (1)
#define MAX77819_CHARGER_CURRENT_MAXIMUM  (2)
#define MAX77819_CHARGER_CURRENT_MINIMUM  (3)

#define CHGINT_REG                      0x30
#define CHGINTM1_REG                    0x31
#define CHG_STAT_REG                    0x32
#define DC_BATT_DTLS_REG                0x33
#define CHG_DTLS_REG                    0x34
#define BAT2SYS_DTLS_REG                0x35
#define BAT2SOC_CTL_REG                 0x36
#define CHGCTL1_REG                     0x37
#define FCHGCRNT_REG                    0x38
#define TOPOFF_REG                      0x39
#define BATREG_REG                      0x3A
#define DCCRNT_REG                      0x3B
#define AICLCNTL_REG                    0x3C
#define RBOOST_CTL1_REG                 0x3D
#define CHGCTL2_REG                     0x3E
#define BATDET_REG                      0x3F
#define USBCHGCTL_REG                   0x40
#define MBATREGMAX_REG                  0x41
#define CHGCCMAX_REG                    0x42
#define RBOOST_CTL2_REG                 0x43
#define CHGINT2_REG                     0x44
#define CHGINTMSK2_REG                  0x45
#define CHG_WDTC_REG                    0x46
#define CHG_WDT_CTL_REG                 0x47
#define CHG_WDT_DTLS_REG                0x48

extern int odin_charger_enable; /* Global Charger enable function */

struct max77819_charger_platform_data {
    bool disable_interrupt;
    int irq;

    char *psy_name;
    char *ext_psy_name;

    char **supplied_to;
    size_t num_supplicants;

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


int get_voltage(void);
int get_soc(void);
int get_cap(void);
int get_temper(void);

int max77819_charger_set_enable_extern(int en);
//void batt_temp_ctrl_start(int reset);
//void d2260_hwmon_set_bat_threshold(int min, int max);
int max77819_cable_type_return(void);

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

#endif /* !__MAX77819_CHARGER_H__ */
