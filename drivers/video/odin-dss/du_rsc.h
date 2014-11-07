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

#ifndef _DU_RSC_H_
#define _DU_RSC_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <video/odin-dss/dss_types.h>
#include "dss_comp_types.h"
#include "hal/dss_hal_ch_id.h"

enum du_src_ch_id du_rsc_ch_alloc(struct dss_image *input_img, struct dss_input_meta *input_meta, struct dss_image *output_img);
bool du_rsc_ch_free(enum du_src_ch_id du_src_ch);

void du_rsc_init(struct platform_device *pdev_ch[], struct platform_device *pdev_mxd);
void du_rsc_cleanup(void);

#endif /* #ifndef _DU_RSC_H_ */


