/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Younghyun Jo <younghyun.jo@lge.com>
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

#ifndef _DSS_FB_H_
#define _DSS_FB_H_

#include <linux/types.h>
#include <linux/platform_device.h>
#include <video/odin-dss/dss_types.h>
#include "hal/dss_hal_ch_id.h"

int dssfb_init(struct platform_device *pdev_dip[]);
void dssfb_cleanup(void);
int dssfb_get_ovl_sync(enum dss_display_port port, enum ovl_ch_id *ovl_ch, enum sync_ch_id *sync_ch, bool *command_mode);

#endif /* #ifndef _DSS_FB_H_ */
