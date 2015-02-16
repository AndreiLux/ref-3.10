/*
 * ALSA SoC Texas Instruments TAS2552 Mono Audio Amplifier
 *
 * Copyright (C) 2014 Texas Instruments Inc.
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __TAS2552_H__
#define __TAS2552_H__

/* Register Address Map */
#define TAS2552_DEVICE_STATUS	0x00
#define TAS2552_CFG_1			0x01
#define TAS2552_CFG_2			0x02
#define TAS2552_CFG_3			0x03
#define TAS2552_DOUT			0x04
#define TAS2552_SER_CTRL_1		0x05
#define TAS2552_SER_CTRL_2		0x06
#define TAS2552_OUTPUT_DATA		0x07
#define TAS2552_PLL_CTRL_1		0x08
#define TAS2552_PLL_CTRL_2		0x09
#define TAS2552_PLL_CTRL_3		0x0a
#define TAS2552_BTIP			0x0b
#define TAS2552_BTS_CTRL		0x0c
#define TAS2552_LIMIT_LVL_CTRL	0x0d
#define TAS2552_LIMIT_RATE_HYS	0x0e
#define TAS2552_LIMIT_RELEASE	0x0f
#define TAS2552_LIMIT_INT_COUNT	0x10
#define TAS2552_PDM_CFG			0x11
#define TAS2552_PGA_GAIN		0x12
#define TAS2552_EDGE_RATE_CTRL	0x13
#define TAS2552_BOOST_PT_CTRL	0x14
#define TAS2552_VER_NUM			0x16
#define TAS2552_VBAT_DATA		0x19
#define TAS2552_MAX_REG			0x20

/* CFG1 Register Masks */
#define TAS2552_MUTE_MASK		0xfb
#define TAS2552_SWS_MASK		0xfd
#define TAS2552_WCLK_MASK		0x07

#define TAS2552_PLL_ENABLE		0x08

/* CFG3 Register Masks */
#define TAS2552_WORD_CLK_MASK		0x80
#define TAS2552_BIT_CLK_MASK		0x40
#define TAS2552_DATA_FORMAT_MASK	0x0c

#define TAS2552_DAIFMT_DSP			0x04
#define TAS2552_DAIFMT_RIGHT_J		0x08
#define TAS2552_DAIFMT_LEFT_J		0x0c

/* WCLK Dividers */
#define TAS2552_8KHZ		0x00
#define TAS2552_11_12KHZ	0x01
#define TAS2552_16KHZ		0x02
#define TAS2552_22_24KHZ	0x03
#define TAS2552_32KHZ		0x04
#define TAS2552_44_48KHZ	0x05
#define TAS2552_88_96KHZ	0x06
#define TAS2552_176_192KHZ	0x07

#endif
