/* linux/max77686-muic.h
 *
 * Functions to access MAX77665 MUIC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX77665_MUIC_H
#define __MAX77665_MUIC_H

#define MAX77665_POWER_SLAVE_ADDR	0x4A

#define MAX77665_REMOTE_KEYS		12

struct max77665_muic_platform_data
{
	unsigned int keys[MAX77665_REMOTE_KEYS];
#ifdef CONFIG_OF
	int irq;
#endif
};

#endif
