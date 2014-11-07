/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Sungwon Son <sungwon.son@lge.com>
 * Youngki Lyu <youngki.lyu@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SYNC_HAL_H_
#define _SYNC_HAL_H_

#include <linux/types.h>
#include "dss_hal_ch_id.h"

enum sync_mode
{
	SYNC_ONESHOT,
	SYNC_CONTINOUS
};

enum sync_intr_src
{
	SYNC_VSYNC_SIGNAL,
	SYNC_ACTIVE_END_SIGNAL
};

enum sync_generater
{
	SYNC_SYNC_GEN,
	SYNC_DIP
};

bool sync_hal_raster_size_set(enum sync_ch_id sync_ch, unsigned int vsize, unsigned int hsize);
bool sync_hal_act_size_set(enum sync_ch_id sync_ch, unsigned int vsize, unsigned int hsize);

bool sync_hal_channel_wb_enable(enum du_src_ch_id  du_ch);
bool sync_hal_channel_wb_disable(enum du_src_ch_id  du_ch);
bool sync_hal_channel_wb_st_dly(enum du_src_ch_id  du_ch, unsigned int vstart, unsigned int hstart);

bool sync_hal_channel_enable(enum du_src_ch_id  du_ch);
bool sync_hal_channel_disable(enum du_src_ch_id  du_ch);
bool sync_hal_channel_st_dly(enum du_src_ch_id  du_ch, unsigned int vstart, unsigned int hstart);
bool sync_hal_ovl_st_dly(enum ovl_ch_id ovl_idx, unsigned int vstart, unsigned int hstart);

bool sync_hal_ovl_enable(enum ovl_ch_id sync_ch);
bool sync_hal_ovl_disable(enum ovl_ch_id sync_ch);

bool sync_hal_enable(enum sync_ch_id sync_ch, enum sync_mode mode, bool int_enable, enum sync_intr_src int_src, enum sync_generater sync_src);
bool sync_hal_disable(enum sync_ch_id sync_ch);

void sync_hal_init(unsigned int sync_base);
void sync_hal_cleanup(void);

#endif /* __SYNC_HAL_H_ */
