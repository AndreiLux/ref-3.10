/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Jungmin Park <jungmin016.park@lge.com>
 * Youngki Lyu <youngki.lyu@lge.com>
 * 
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License 
 * version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 */

#ifndef _OVL_RSC_H_
#define _OVL_RSC_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <video/odin-dss/dss_types.h>
#include "hal/dss_hal_ch_id.h"

bool ovl_rsc_enable(enum ovl_ch_id ovl_ch);
bool ovl_rsc_disable(enum ovl_ch_id ovl_ch);
void ovl_rsc_init(struct platform_device *pdev_ovl[]);
void ovl_rsc_cleanup(void);
void ovl_rsc_set(enum ovl_ch_id ovl_ch, struct dss_image_size *ovl_size, unsigned char pattern_r, unsigned char pattern_g, unsigned char pattern_b);

#endif /* #ifndef _OVL_RSC_H_ */

