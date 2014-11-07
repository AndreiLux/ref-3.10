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

#ifndef _OVL_HAL_H_
#define _OVL_HAL_H_

#include <linux/types.h>
#include <video/odin-dss/dss_types.h>
#include "dss_hal_ch_id.h"

enum ovl_zorder
{
	OVL_ZORDER_0,
	OVL_ZORDER_1,
	OVL_ZORDER_2,
	OVL_ZORDER_3,
	OVL_ZORDER_4,
	OVL_ZORDER_5,
	OVL_ZORDER_6,
	OVL_ZORDER_NR,
};

enum ovl_operation
{
	OVL_OPS_CLEAR,
	OVL_OPS_SRC,
	OVL_OPS_DST,
	OVL_OPS_SRC_OVER,
	OVL_OPS_DST_OVER,
	OVL_OPS_SRC_IN,
	OVL_OPS_DST_IN,
	OVL_OPS_SRC_OUT,
	OVL_OPS_DST_OUT,
	OVL_OPS_SRC_ATOP,
	OVL_OPS_DST_ATOP,
	OVL_OPS_XOR
};

enum ovl_raster_ops
{
	OVL_RASTER_OPS_0,	/* 0 */
	OVL_RASTER_OPS_1,	/* ~(s0|s1) */
	OVL_RASTER_OPS_2,	/* s0 & s1 */
	OVL_RASTER_OPS_3,	/* ~s1 */
	OVL_RASTER_OPS_4,	/* s1 ^ pattern */
	OVL_RASTER_OPS_6,	/* s0 ^ s1 */
	OVL_RASTER_OPS_7,	/* s0 & s1 */
	OVL_RASTER_OPS_8,	/* ~s0 | s1 */
	OVL_RASTER_OPS_9,	/* s0 & pattern */
	OVL_RASTER_OPS_10,	/* s0 */
	OVL_RASTER_OPS_11,	/* s0 | s1 */
	OVL_RASTER_OPS_12,	/* pattern */
	OVL_RASTER_OPS_13,	/* ~s0 | s1 | pattern */
	OVL_RASTER_OPS_14,	/* 0xff */
	OVL_RASTER_OPS_NUM,
};

struct ovl_hal_set_param
{
	enum du_src_ch_id ch;

	struct dss_image_pos ovl_image_pos;
	struct dss_image_size ovl_pip_size;
	enum ovl_operation ops;
	bool ovl_alpha_enable;
	unsigned char alpha_value;
	bool pre_mul_enable;
	bool chromakey_enable;
	unsigned char chroma_data_r;
	unsigned char chroma_data_g;
	unsigned char chroma_data_b;
	unsigned char chroma_mask_r;
	unsigned char chroma_mask_g;
	unsigned char chroma_mask_b;
	bool se_src_secure;
	bool se_src_sel_secure;
	bool se_src_layer_secure;
};

unsigned int ovl_hal_empty_data_fifo_status(enum ovl_ch_id ovl_idx, enum ovl_zorder zorder);
unsigned int ovl_hal_full_data_fifo_status(enum ovl_ch_id ovl_idx, enum ovl_zorder zorder);
unsigned int ovl_hal_con_st_status(enum ovl_ch_id ovl_idx);

bool ovl_hal_set(enum ovl_ch_id ovl_idx, struct ovl_hal_set_param param[]);
bool ovl_hal_channel_set (enum ovl_ch_id ovl_idx, enum du_src_ch_id ch,
		struct dss_image_pos ovl_image_pos, struct dss_image_size ovl_pip_size,
		enum ovl_zorder zorder, enum ovl_operation ops,
		bool ovl_alpha_enable, unsigned char alpha_value, bool pre_mul_enable,
		bool chromakey_enable,
		unsigned char chroma_data_r, unsigned char chroma_data_g, unsigned char chroma_data_b,
		unsigned char chroma_mask_r, unsigned char chroma_mask_g, unsigned char chroma_mask_b,
		bool se_src_secure, bool se_src_sel_secure, bool se_src_layer_secure);
bool ovl_hal_channel_clear(enum ovl_ch_id ovl_idx, enum du_src_ch_id ch, enum ovl_zorder zorder);

bool ovl_hal_enable(enum ovl_ch_id ovl_idx, struct dss_image_size ovl_size, bool dual_op_enable, bool raster, enum ovl_raster_ops raster_op, unsigned char pattern_data_r, unsigned char pattern_data_g, unsigned char pattern_data_b);
bool ovl_hal_disable(enum ovl_ch_id ovl_idx);

void ovl_hal_init(unsigned int ovl_base);
void ovl_hal_cleanup(void);

#endif /* _OVL_HAL_H_ */
