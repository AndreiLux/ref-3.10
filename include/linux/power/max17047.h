/*
 *  max17047.h
 *
 *  Copyright (C) 2012 Maxim Integrated Product
 *  Gyungoh Yoo <jack.yoo@maxim-ic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX17047_BATTERY_H_
#define __MAX17047_BATTERY_H_

struct max17047_platform_data
{
	u16 capacity;
	u16 vf_fullcap;
	u16 qrtable00;
	u16 qrtable10;
	u16 qrtable20;
	u16 qrtable30;
	u16 vempty;
	u16 fullsocthr;
	u16 rcomp0;
	u16 tempco;
	u16 termcurr;
	u16 tempnom;
	u16 filtercfg;
	u16 relaxcfg;
	u16 tgain;
	u16 toff;
	u16 custome_model[16 * 3];
#ifdef CONFIG_MAX17047_JEITA
	char *charger_name;
#endif
	int r_sns;		/* sensing resistor, in mOhm */
	bool current_sensing;
#ifdef CONFIG_OF
	int irq;
#endif
};

#endif
