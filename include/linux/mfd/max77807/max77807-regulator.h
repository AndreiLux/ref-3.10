/*
 * linux/power/max77807-regulator.h
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MAX77807_REGULATOR_H
#define __LINUX_MAX77807_REGULATOR_H

struct max77807_regulator_data {
	int id;
	bool active_discharge;
	struct regulator_init_data *initdata;
	struct device_node *of_node;

};
struct max77807_regulator_platform_data
{
	int num_regulators;
	struct max77807_regulator_data *regulators;
};

#endif
