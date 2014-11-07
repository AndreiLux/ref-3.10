/* linux/mfd/max77665.h
 *
 * Functions to access MAX77665 power management chip.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MFD_MAX77665_H
#define __LINUX_MFD_MAX77665_H

#include <linux/regmap.h>

/* 1st lev IRQ definitions at top level*/
enum {
	MAX77665_INT_CHGR = 0,
	MAX77665_INT_TOP = 1,
	MAX77665_INT_FLASH = 2,
	MAX77665_INT_MUIC = 3,
	MAX77665_INT_NUMS = 4,
};

#define M2SH(m) ((m) & 0x0F ? ((m) & 0x03 ? ((m) & 0x01 ? 0 : 1) :\
		((m) & 0x04 ? 2 : 3)) : ((m) & 0x30 ? ((m) & 0x10 ? 4 : 5) :\
		((m) & 0x40 ? 6 : 7)))

struct max77665_platform_data
{
	struct mfd_cell *sub_devices;
	int num_subdevs;
#ifdef CONFIG_OF
#if defined(CONFIG_MACH_ODIN_HDK)
	int irq_gpio;
	int irq_gpio_base;
#else
	int irq;
#endif
#endif
};

struct max77665
{
	struct regmap *regmap;
	struct regmap_irq_chip_data *irq_data;
};

#ifdef CONFIG_USB_ODIN_DRD
void max77665_charger_interrupt_onoff(int onoff);
void max77665_charger_otg_onoff(int onoff);
void max77665_charger_vbypset(u8 voltage);
#endif

#endif
