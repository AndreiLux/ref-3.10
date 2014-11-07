/*
 * linux/power/max77819-sfo.h
 *
 * Copyright 2013 Maxim Integrated Products, Inc.
 * Gyungoh Yoo <jack.yoo@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
#ifndef __LINUX_MAX77819_CHARGER_H
#define __LINUX_MAX77819_CHARGER_H

struct max77819_sfo_platform_data
{
	bool active_discharge;
	struct regulator_init_data *initdata;
};

#endif
