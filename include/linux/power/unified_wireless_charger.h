/*
 * Copyright (C) 2012, Kyungtae Oh <kyungtae.oh@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#ifndef __LINUX_POWER_UNIFIED_WLC_CHARGER_H__
#define __LINUX_POWER_UNIFIED_WLC_CHARGER_H__

#define UNIFIED_WLC_DEV_NAME "unified_wlc"

struct unified_wlc_platform_data {
	unsigned int wlc_full_chg;
};
#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT
#define WLC_ALIGN_INTERVAL	(300)
#endif

extern void wireless_interrupt_handler(bool dc_present);
extern void wireless_chg_term_handler(void);
#endif
