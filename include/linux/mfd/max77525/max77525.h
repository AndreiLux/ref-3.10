/*
 * max77525.h - MFD Driver for the Maxim 77525
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
 *
 * The devices share the same I2C bus and included in
 * this mfd driver.
 */

#ifndef __LINUX_MFD_MAX77525_H
#define __LINUX_MFD_MAX77525_H

#include <linux/platform_device.h>
#include <linux/regmap.h>

#define PMIC_I2C_SLOT			0
#define MAX77525_SLAVE_ADDRESS	0xB0

/* IRQ definitaions */
enum {
    MAX77525_IRQ_MBATDETINT,
    MAX77525_IRQ_TOPSYSINT,
    MAX77525_IRQ_GPIOINT,
    MAX77525_IRQ_ADCINT,
    MAX77525_IRQ_RTCINT,

    MAX77525_TOPIRQ_NR,
};

/* argument 'm' should not be zero. */
#define MASK2SHIFT(m) ((m) & 0x0F ? ((m) & 0x03 ? ((m) & 0x01 ? 0 : 1) : \
		((m) & 0x04 ? 2 : 3)) : \
		((m) & 0x30 ? ((m) & 0x10 ? 4 : 5) : ((m) & 0x40 ? 6 : 7)))

struct max77525
{
	struct device *dev;
	struct regmap *regmap;
	struct regmap_irq_chip_data *irq_data;
	int irq_gpio;
	int irq;
};

struct max77525_platform_data
{
	int sysuvlo;
	struct max77235_platform_data *pdata;
};

#endif
