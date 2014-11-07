/*
 * linux/power/max77819-charger.h
 *
 * Copyright (C) 2013 Maxim Integrated Product
 * Gyungoh Yoo <jack.yoo@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
#ifndef __LINUX_MAX77819_CHARGER_H
#define __LINUX_MAX77819_CHARGER_H

struct max77819_charger_pdata
{
	int fast_charge_timer;			/* One of 0, 4, 5, 6, 7, 8, 9, 16Hr */
	int restart_threshold;			/* 150000uV or 200000uV */
	int current_limit_ac;			/* 100000uA ~ 1875000uA */
	int current_limit_usb;			/* 100000uA ~ 1875000uA */
	int topoff_current_threshold;		/* 50000uA ~ 400000uA */
	int topoff_timer;			/* 0min to 60min, -1 is for not-done */
	int voltage;				/* 3550000uV ~ 4400000uV */
	bool enable_jeita;
	bool enable_aicl;
	int aicl_voltage;			/* 3900000uV ~ 4800000uV */
	int aicl_threshold;			/* 100000uV or 200000uV */
	int prequal_current;			/* one of 100000, 200000, 300000, 400000uA */
#ifdef CONFIG_OF
	int irq;
#endif
};

#endif
