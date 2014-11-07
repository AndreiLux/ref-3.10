/*
 * max77235.h - MFD Driver for the Maxim 77235
 *
 * Copyright (C) 2013 Maxim Integrated
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max77686.h
 *
 * MAX77235 has PMIC, RTC devices.
 * The devices share the same I2C bus and included in
 * this mfd driver.
 */

#ifndef __LINUX_MFD_MAX77235_H
#define __LINUX_MFD_MAX77235_H

#include <linux/platform_device.h>
#include <linux/regmap.h>

#define MAX77235_IRQ_PWRONF	0
#define MAX77235_IRQ_PWRONR	1
#define MAX77235_IRQ_JIGONBF	2
#define MAX77235_IRQ_JIGONBR	3
#define MAX77235_IRQ_ACOKBF	4
#define MAX77235_IRQ_ACOKBR	5
#define MAX77235_IRQ_ONKEY1S	6
#define MAX77235_IRQ_MRSTB	7
#define MAX77235_IRQ_140C	8
#define MAX77235_IRQ_120C	9
#define MAX77235_IRQ_RTC	10

/* argument 'm' should not be zero. */
#define MASK2SHIFT(m) ((m) & 0x0F ? ((m) & 0x03 ? ((m) & 0x01 ? 0 : 1) : ((m) & 0x04 ? 2 : 3)) : \
		((m) & 0x30 ? ((m) & 0x10 ? 4 : 5) : ((m) & 0x40 ? 6 : 7)))

struct max77235
{
	struct device *dev;
	struct regmap *regmap;
	struct regmap_irq_chip_data *irq_data;
	struct regmap_irq_chip_data *rtc_irq_data;
	int irq;
	int num_gpio;
};

struct max77235_platform_data
{
	const struct mfd_cell *cells;
	const int num_cells;
	struct max77235_platform_data *pdata;
};

#endif
