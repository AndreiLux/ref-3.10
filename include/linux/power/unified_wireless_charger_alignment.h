/*
 * Copyright (C) 2014
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#ifndef __LINUX_POWER_UNIFIED_WLC_CHARGER_ALIGNMENT_H__
#define __LINUX_POWER_UNIFIED_WLC_CHARGER_ALIGNMENT_H__

#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT_IDT9025A
extern  int idtp9025_align_start(void);
extern  int idtp9025_align_stop(void);
extern  int idtp9025_align_get_value(void);
#endif

#endif
