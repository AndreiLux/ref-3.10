/*
 * max77525_reg.h - MFD Driver for the Maxim 77525
 *
 * Copyright (C) 2013 LG Electoronics
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
 * The devices share the same mailbox and included in
 * this mfd driver.
 */

#ifndef __LINUX_MFD_MAX77525_REG_H__
#define __LINUX_MFD_MAX77525_REG_H__

#include <linux/platform_device.h>
#include <linux/regmap.h>

/* registers */
#define REG_CHIPID		0x00
#define REG_INTTOPSEL	0x01
#define REG_INTTOPA		0x02
#define REG_INTTOPAM	0x03
#define REG_INTTOPSR	0x04
#define REG_INTTOPSRM	0x05
#define REG_SYSUVLOCNFG	0x06
#define REG_PWRONBCNFG	0x07
#define REG_RESETBCNFG	0x08
#define REG_GLPM		0x09
#define REG_WRSTBCNFG	0x0A
#define REG_TURNONSTTS	0x0B
#define REG_TURNOFFSTTS	0x0C
#define REG_SECPWRSEQ	0x0D

#define REG_TOPSYSINT	0x0E
#define REG_TOPSYSINTM	0x0F
#define REG_TOPSYSSTTS	0x10

/* REG_INTTOP */
#define BIT_MBATDETINT	BIT(5)
#define BIT_TOPSYSINT	BIT(4)
#define BIT_GPIOINT		BIT(3)
#define BIT_ADCINT		BIT(2)
#define BIT_RTCINT		BIT(1)

/* REG_TOPSYSINT */
#define BIT_WRST		BIT(5)
#define BIT_PWRONBON	BIT(4)
#define BIT_PWROFFBOFF	BIT(3)
#define BIT_AUXONB		BIT(2)
#define BIT_120C		BIT(1)
#define BIT_140C		BIT(0)

#endif
