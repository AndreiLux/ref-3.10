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

#include <linux/types.h>
#include <linux/io.h>
#include <linux/printk.h>

#include <video/odin-dss/dss_types.h>

#include "du_hal.h"
#include "sync_hal.h"
#include "dss_hal_ch_id.h"
#include "reg_def/sync_gen_union_reg.h"

#define _sync_hal_verify_du_src_ch(du_src_idx)	((du_src_idx) < DU_SRC_NUM ? true : false)
#define _sync_hal_verify_du_wb_ch(du_src_idx)	(((du_src_idx == DU_SRC_VID0) || (du_src_idx == DU_SRC_VID1) ||(du_src_idx == DU_SRC_GSCL)) ? true : false)
#define _sync_hal_verify_sync_ch(sync_ch)	((sync_ch) < SYNC_CH_NUM ? true : false)

volatile DSS_DU_SYNC_GEN_REG_T *syncbase;

bool sync_hal_raster_size_set(enum sync_ch_id sync_ch, unsigned int vsize, unsigned int hsize)
{
	if (_sync_hal_verify_sync_ch(sync_ch) == false) {
		pr_err("invalid raster set sync_ch(%d)\n", sync_ch);
		return false;
	}

	switch (sync_ch) {
	case SYNC_CH_ID0:
		syncbase->sync_raster_size0.a32 = (vsize << 16 | hsize);
		break;
	case SYNC_CH_ID1:
		syncbase->sync_raster_size1.a32 = (vsize << 16 | hsize);
		break;
	case SYNC_CH_ID2:
		syncbase->sync_raster_size2.a32 = (vsize << 16 | hsize);
		break;
	default :
		pr_err("invalid raster set sync_ch(%d)\n", sync_ch);
		return false;
	}

	return true;
}

bool sync_hal_act_size_set(enum sync_ch_id sync_ch, unsigned int vsize, unsigned int hsize)
{
	if (_sync_hal_verify_sync_ch(sync_ch) == false) {
		pr_err("invalid act set sync_ch(%d)\n", sync_ch);
		return false;
	}

	switch (sync_ch) {
	case SYNC_CH_ID0:
		syncbase->sync0_act_size.a32 = (vsize << 16 | hsize);
		break;
	case SYNC_CH_ID1:
		syncbase->sync1_act_size.a32 = (vsize << 16 | hsize);
		break;
	case SYNC_CH_ID2:
		syncbase->sync2_act_size.a32 = (vsize << 16 | hsize);
		break;
	default :
		pr_err("invalid act set sync_ch(%d)\n", sync_ch);
		return false;
	}

	return true;
}

bool sync_hal_ovl_st_dly(enum ovl_ch_id ovl_idx, unsigned int vstart, unsigned int hstart)
{
	if (ovl_idx >= OVL_CH_NUM) {
		pr_err("invalid ovl_st_dly ovl_idx(%d)\n", ovl_idx);
		return false;
	}

	switch (ovl_idx) {
	case OVL_CH_ID0:
		syncbase->sync_ovl0_st_dly.a32 = (vstart << 16 | hstart);
		break;
	case OVL_CH_ID1:
		syncbase->sync_ovl1_st_dly.a32 = (vstart << 16 | hstart);
		break;		
	case OVL_CH_ID2:
		syncbase->sync_ovl2_st_dly.a32 = (vstart << 16 | hstart);
		break;		
	default:
		pr_err("invalid ovl_st_dly ovl_idx(%d)\n", ovl_idx);
	}

	return true;

}

bool sync_hal_channel_st_dly(enum du_src_ch_id  du_ch, unsigned int vstart, unsigned int hstart)
{
	if (_sync_hal_verify_du_src_ch(du_ch) == false) {
		pr_err("invalid ch_st_dly du_ch(%d)\n", du_ch);
		return false;
	}

	switch (du_ch) {
	case DU_SRC_VID0:
		syncbase->sync_vid0_st_dly0.a32 = (vstart << 16 | hstart);
		break;
	case DU_SRC_VID1:
		syncbase->sync_vid1_st_dly0.a32 = (vstart << 16 | hstart);
		break;
	case DU_SRC_VID2:
		syncbase->sync_vid2_st_dly.a32 = (vstart << 16 | hstart);
		break;
	case DU_SRC_GRA0:
		syncbase->sync_gra0_st_dly.a32 = (vstart << 16 | hstart);
		break;
	case DU_SRC_GRA1:
		syncbase->sync_gra1_st_dly.a32 = (vstart << 16 | hstart);
		break;
	case DU_SRC_GRA2:
		syncbase->sync_gra2_st_dly.a32 = (vstart << 16 | hstart);
		break;
	case DU_SRC_GSCL:
		syncbase->sync_gscl_st_dly0.a32 = (vstart << 16 | hstart);
		break;
	default :
		pr_err("invalid ch_st_dly du_ch(%d)\n", du_ch);
		return false;
	} 

	return true;
}

bool sync_hal_channel_wb_st_dly(enum du_src_ch_id  du_ch, unsigned int vstart, unsigned int hstart)
{
	if (_sync_hal_verify_du_wb_ch(du_ch) == false) {
		pr_err("invalid ch_wb_st_dly du_ch(%d)\n", du_ch);
		return false;
	}

	switch (du_ch) {
	case DU_SRC_VID0:
		syncbase->sync_vid0_st_dly1.a32 = (vstart << 16 | hstart);
		break;
	case DU_SRC_VID1:
		syncbase->sync_vid1_st_dly1.a32 = (vstart << 16 | hstart);
		break;
	case DU_SRC_GSCL:
		syncbase->sync_gscl_st_dly1.a32 = (vstart << 16 | hstart);
		break;
	default :
		pr_err("invalid ch_wb_st_dly du_ch(%d)\n", du_ch);
		return false;
	}

	return true;
}

bool sync_hal_channel_wb_enable(enum du_src_ch_id  du_ch)
{
	volatile sync_cfg2 sync_cfg2 = {.a32 = 0};

	if (_sync_hal_verify_du_wb_ch(du_ch) == false) {
		pr_err("invalid ch_wb_enable du_ch(%d)\n", du_ch);
		return false;
	}

	sync_cfg2 = syncbase->sync_cfg2;

	switch (du_ch) {
	case DU_SRC_VID0:
		sync_cfg2.af.vid0_sync_en1 = 1;
		break;
	case DU_SRC_VID1:
		sync_cfg2.af.vid1_sync_en1 = 1;
		break;
	case DU_SRC_GSCL:
		sync_cfg2.af.gscl_sync_en1 = 1;
	default :
		pr_err("invalid  ch_wb_enable du_ch(%d)\n", du_ch);
		return false;
	}

	syncbase->sync_cfg2 = sync_cfg2;

	return true;
}

bool sync_hal_channel_wb_disable(enum du_src_ch_id  du_ch)
{
	volatile sync_cfg2 sync_cfg2 = {.a32 = 0};

	if (_sync_hal_verify_du_wb_ch(du_ch) == false) {
		pr_err("invalid ch_wb_disable du_ch(%d)\n", du_ch);
		return false;
	}

	sync_cfg2 = syncbase->sync_cfg2;

	switch (du_ch){
	case DU_SRC_VID0:
		sync_cfg2.af.vid0_sync_en1 = 0;
		break;
	case DU_SRC_VID1:
		sync_cfg2.af.vid1_sync_en1 = 0;
		break;
	case DU_SRC_GSCL:
		sync_cfg2.af.gscl_sync_en1 = 0;
	default :
		pr_err("invalid ch_wb_disable du_ch(%d)\n", du_ch);
		return false;
	}

	syncbase->sync_cfg2 = sync_cfg2;

	return true;
}

bool sync_hal_channel_enable(enum du_src_ch_id  du_ch)
{
	volatile sync_cfg1 sync_cfg1 = {.a32 = 0};

	if (_sync_hal_verify_du_src_ch(du_ch) == false) {
		pr_err("invalid ch_enable du_ch(%d)\n", du_ch);
		return false;
	}

	sync_cfg1 = syncbase->sync_cfg1;

	switch (du_ch) {
	case DU_SRC_VID0:
		sync_cfg1.af.vid0_sync_en0 = 1;
		break;
	case DU_SRC_VID1:
		sync_cfg1.af.vid1_sync_en0 = 1;
		break;
	case DU_SRC_VID2:
		sync_cfg1.af.vid2_sync_en = 1;
		break;
	case DU_SRC_GRA0:
		sync_cfg1.af.gra0_sync_en = 1;
		break;
	case DU_SRC_GRA1:
		sync_cfg1.af.gra1_sync_en = 1;
		break;
	case DU_SRC_GRA2:
		sync_cfg1.af.gra2_sync_en = 1;
		break;
	case DU_SRC_GSCL:
		sync_cfg1.af.gscl_sync_en0 = 1;
		break;
	default :
		pr_err("invalid ch_enable du_ch(%d)\n", du_ch);
		return false;
	}

	syncbase->sync_cfg1 = sync_cfg1;

	return true;
}

bool sync_hal_channel_disable(enum du_src_ch_id  du_ch)
{
	volatile sync_cfg1 sync_cfg1 = {.a32 = 0};

	if (_sync_hal_verify_du_src_ch(du_ch) == false) {
		pr_err("invalid ch_disable du_ch(%d)\n", du_ch);
		return false;
	}

	sync_cfg1 = syncbase->sync_cfg1;

	switch (du_ch) {
	case DU_SRC_VID0:
		sync_cfg1.af.vid0_sync_en0 = 0;
		break;
	case DU_SRC_VID1:
		sync_cfg1.af.vid1_sync_en0 = 0;
		break;
	case DU_SRC_VID2:
		sync_cfg1.af.vid2_sync_en = 0;
		break;
	case DU_SRC_GRA0:
		sync_cfg1.af.gra0_sync_en = 0;
		break;
	case DU_SRC_GRA1:
		sync_cfg1.af.gra1_sync_en = 0;
		break;
	case DU_SRC_GRA2:
		sync_cfg1.af.gra2_sync_en = 0;
		break;
	case DU_SRC_GSCL:
		sync_cfg1.af.gscl_sync_en0 = 0;
		break;
	case DU_SRC_NUM:
	default :
		pr_err("invalid ch_disable du_ch(%d)\n", du_ch);
		return false;
	}

	syncbase->sync_cfg1 = sync_cfg1;

	return true;
}

bool sync_hal_ovl_enable(enum ovl_ch_id sync_ch)
{
	volatile sync_cfg1 sync_cfg1 = {.a32 = 0};

	sync_cfg1 = syncbase->sync_cfg1;

	switch (sync_ch) {
	case OVL_CH_ID0:
		sync_cfg1.af.ovl0_sync_en = 1;
		break;
	case OVL_CH_ID1:
		sync_cfg1.af.ovl1_sync_en = 1;
		break;
	case OVL_CH_ID2:
		sync_cfg1.af.ovl2_sync_en = 1;
		break;
	default :
		pr_err("invalid sync_ch(%d)\n", sync_ch);
		return false;
	}

	syncbase->sync_cfg1 = sync_cfg1;

	return true;
}

bool sync_hal_ovl_disable(enum ovl_ch_id sync_ch)
{
	volatile sync_cfg1 sync_cfg1 = {.a32 = 0};

	sync_cfg1 = syncbase->sync_cfg1;

	switch (sync_ch) {
	case OVL_CH_ID0:
		sync_cfg1.af.ovl0_sync_en = 0;
		break;
	case OVL_CH_ID1:
		sync_cfg1.af.ovl1_sync_en = 0;
		break;
	case OVL_CH_ID2:
		sync_cfg1.af.ovl2_sync_en = 0;
		break;
	default :
		pr_err("invalid sync_ch(%d)\n", sync_ch);
		return false;
	}

	syncbase->sync_cfg1 = sync_cfg1;

	return true;
}

bool sync_hal_enable(enum sync_ch_id sync_ch, enum sync_mode mode, bool int_enable, enum sync_intr_src int_src, enum sync_generater sync_src)
{
	volatile sync_cfg0 sync_cfg0 = {.a32 = 0};

	if (_sync_hal_verify_sync_ch(sync_ch) == false) {
		pr_err("invalid sync_hal_enable sync_ch(%d)\n", sync_ch);
		return false;
	}

	if (mode > SYNC_CONTINOUS || int_src > SYNC_ACTIVE_END_SIGNAL || sync_src > SYNC_DIP ) {
		pr_err("invalid sync_hal_enable sync_ch(%d), mode(%d), int_src(%d), sync_src(%d)\n", sync_ch, mode, int_src, sync_src);
		return false;
	}

	sync_cfg0 = syncbase->sync_cfg0;

	switch (sync_ch) {
	case SYNC_CH_ID0:
		sync_cfg0.af.sync0_enable = 1;
		sync_cfg0.af.sync0_mode = mode;
		sync_cfg0.af.sync0_int_en = int_enable;
		sync_cfg0.af.sync0_int_src_sel = int_src;
		sync_cfg0.af.sync0_disp_sync_src_sel = sync_src;
		break;
	case SYNC_CH_ID1:
		sync_cfg0.af.sync1_enable = 1;
		sync_cfg0.af.sync1_mode = mode;
		sync_cfg0.af.sync1_int_en = int_enable;
		sync_cfg0.af.sync1_int_src_sel = int_src;
		sync_cfg0.af.sync1_disp_sync_src_sel = sync_src;
		break;
	case SYNC_CH_ID2:
		sync_cfg0.af.sync2_enable = 1;
		sync_cfg0.af.sync2_mode = mode;
		sync_cfg0.af.sync2_int_en = int_enable;
		sync_cfg0.af.sync2_int_src_sel = int_src;
		sync_cfg0.af.sync2_disp_sync_src_sel = sync_src;
		break;
	default :
		pr_err("invalid sync_ch(%d)\n", sync_ch);
		return false;
	}

	syncbase->sync_cfg0 = sync_cfg0;	

	return true;
}

bool sync_hal_disable(enum sync_ch_id sync_ch)
{
	volatile sync_cfg0 sync_cfg0 = {.a32 = 0};
	volatile sync_cfg1 sync_cfg1 = {.a32 = 0};

	if (_sync_hal_verify_sync_ch(sync_ch) == false) {
		pr_err("invalid sync_ch(%d)\n", sync_ch);
		return false;
	}

	sync_cfg0 = syncbase->sync_cfg0;		
	sync_cfg1 = syncbase->sync_cfg1;

	switch (sync_ch) {
	case SYNC_CH_ID0:
		sync_cfg0.af.sync0_enable = 0;
		sync_cfg0.af.sync0_mode = 0;
		sync_cfg0.af.sync0_int_en = 0;
		sync_cfg0.af.sync0_int_src_sel = 0;
		sync_cfg0.af.sync0_disp_sync_src_sel = 0;
		sync_cfg1.af.ovl0_sync_en = 0;
		break;
	case SYNC_CH_ID1:
		sync_cfg0.af.sync1_enable = 0;
		sync_cfg0.af.sync1_mode = 0;
		sync_cfg0.af.sync1_int_en = 0;
		sync_cfg0.af.sync1_int_src_sel = 0;
		sync_cfg0.af.sync1_disp_sync_src_sel = 0;
		sync_cfg1.af.ovl1_sync_en = 0;
		break;
	case SYNC_CH_ID2:
		sync_cfg0.af.sync2_enable = 0;
		sync_cfg0.af.sync2_mode = 0;
		sync_cfg0.af.sync2_int_en = 0;
		sync_cfg0.af.sync2_int_src_sel = 0;
		sync_cfg0.af.sync2_disp_sync_src_sel = 0;
		sync_cfg1.af.ovl2_sync_en = 0;
		break;
	default :
		pr_err("invalid sync_ch(%d)\n", sync_ch);
		return false;
	}

	syncbase->sync_cfg0 = sync_cfg0;	
	syncbase->sync_cfg1 = sync_cfg1;

	return true;
}	

void sync_hal_init(unsigned int sync_base)
{
	enum sync_ch_id sync_i;
	enum du_src_ch_id  du_src_i;

	syncbase = (DSS_DU_SYNC_GEN_REG_T *)sync_base;

	for (sync_i=0; sync_i < SYNC_CH_NUM; sync_i++) {
		sync_hal_disable(sync_i);
		sync_hal_raster_size_set(sync_i, 0, 0);
		sync_hal_act_size_set(sync_i, 0, 0);
	}

	for (du_src_i=0; du_src_i < DU_SRC_NUM; du_src_i++) {
		sync_hal_channel_disable(du_src_i);
	}
}

void sync_hal_cleanup(void)
{
}
