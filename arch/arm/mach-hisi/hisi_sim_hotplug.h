/*
 * Header file for device driver SIM HOTPLUG
 *
 * Copyright (c) 2013 Linaro Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef	__HISI_SIM_HOTPLUG_H
#define	__HISI_SIM_HOTPLUG_H

#include <linux/irqdomain.h>

#define SIM0	0
#define SIM1	1

#define SIM_DET(n)		(GPIO_039 + (n))
#define SIM_CLK(n)		(USIM0_CLK + 3 * (n))
#define SIM_RST(n)		(USIM0_RST + 3 * (n))
#define SIM_DATA(n)		(USIM0_DATA + 3 * (n))
#define HPD_FALL_EDGE_IRQEN(n)	(0x02 << ((n) * 4))
#define IRQ_MASK(n)		(0x0F << ((n) * 4))
#define SIM_IN_POSITION		0
#define SIM_LEAVE_POSITION	1
#define FALL_EDGE		0
#define RAISE_EDGE		1
#define SIM_CTRL1		0x0FE

#endif		/* __HISI_SIM_HOTPLUG_H */
