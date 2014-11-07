/*
 * Copyright (C) 2013, Kyungtae Oh <kyungtae.oh@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#ifndef __LINUX_POWER_MAX14670_CHARGER_H__
#define __LINUX_POWER_MAX14670_CHARGER_H__

#define MAX14670_WLC_DEV_NAME "max14670_wlc"

#define WLC_DEG

#ifdef WLC_DEG
#define WLC_DBG_INFO(fmt, args...) \
	pr_info("wlc: %s: " fmt, __func__, ##args)
#define WLC_DBG(fmt, args...) \
	pr_debug("wlc: %s: " fmt, __func__, ##args)
#else
#define WLC_DBG_INFO(fmt, args...)  do { } while(0)
#define WLC_DBG(fmt, arges...)      do { } while(0)
#endif

//                                  
#include "linux/wakelock.h"

struct max14670_wlc_chip {
    struct device *dev;
    struct power_supply wireless_psy;
    struct work_struct wireless_interrupt_work;
	struct delayed_work wireless_eoc_work;
#ifdef LGE_USED_WAKELOCK_MAX14670
    struct wake_lock wireless_chip_wake_lock;
	struct wake_lock wireless_eoc_wake_lock;
#endif
    unsigned int wl_chg_int;        //wireless charger notification gpio_pin
    unsigned int wl_chg_full;       //wireless charger full notification gpio_pin
    unsigned int present;
	unsigned int online;
    };

int wlc_is_plugged(void);
int set_wireless_charger_status(int value);

#endif
