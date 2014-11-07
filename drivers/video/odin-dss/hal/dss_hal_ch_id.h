/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
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

#ifndef _DSS_HAL_CH_ID_H_
#define _DSS_HAL_CH_ID_H_

enum du_src_ch_id
{
	DU_SRC_VID0 = 0,
	DU_SRC_VID1,
	DU_SRC_VID2,
	DU_SRC_GRA0,
	DU_SRC_GRA1,
	DU_SRC_GRA2,
	DU_SRC_GSCL,
	DU_SRC_NUM,
	DU_SRC_INVALID,
};

enum ovl_ch_id
{
	OVL_CH_ID0 = 0,
	OVL_CH_ID1,
	OVL_CH_ID2,
	OVL_CH_NUM,
	OVL_CH_INVALID,
};

enum sync_ch_id
{
	SYNC_CH_ID0 = 0,
	SYNC_CH_ID1,
	SYNC_CH_ID2,
	SYNC_CH_NUM,
	SYNC_CH_INVALID,
};

#endif // _DSS_HAL_CH_ID_H_
