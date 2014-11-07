/*
 *  Platform data declarations for Dialog D2260 PMIC
 *
 *  Copyright (C) 2013 Dialog Semiconductor Ltd.
 *  Author: Tony Olech <anthony.olech@diasemi.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#ifndef __MFD_D2260_PDATA_H__
#define __MFD_D2260_PDATA_H__

#define D2260_MAX_REGULATORS	35

struct d2260;

struct d2260_pdata {
	struct led_platform_data *pled;
	int (*init) (struct d2260 *d2260);
	int	num_gpio;
	int irq_base;
	int use_for_apm;
	int number_of_regulators;
	struct regulator_init_data *regulators[D2260_MAX_REGULATORS];
	int gpio_base;
};

#endif
