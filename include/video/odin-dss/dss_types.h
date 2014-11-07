/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Dae-Jong Seo <daejong.seo@lge.com>
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

#ifndef _DSS_TYPES_H_
#define _DSS_TYPES_H_

#include <linux/ion.h>

#define DSS_NUM_DU_CHANNEL 7

typedef unsigned char dss_bool;

struct dss_image_pos {
	uint32_t x;
	uint32_t y;
};

struct dss_image_size {
	uint32_t w;
	uint32_t h;
};

struct dss_image_rect {
	struct dss_image_pos pos;
	struct dss_image_size size;
};

enum dss_display_port {
	DSS_DISPLAY_PORT_FB0, 
	DSS_DISPLAY_PORT_FB1,
	DSS_DISPLAY_PORT_MEM,
	DSS_DISPLAY_PORT_NUM,
	DSS_DISPLAY_PORT_INVALID
};

enum dss_image_format {
    DSS_IMAGE_FORMAT_RGBA_8888,
    DSS_IMAGE_FORMAT_RGBX_8888,
    DSS_IMAGE_FORMAT_BGRA_8888,
    DSS_IMAGE_FORMAT_BGRX_8888,
    DSS_IMAGE_FORMAT_ARGB_8888,
    DSS_IMAGE_FORMAT_XRGB_8888,
    DSS_IMAGE_FORMAT_ABGR_8888,
    DSS_IMAGE_FORMAT_XBGR_8888,
    DSS_IMAGE_FORMAT_RGBA_5551,
    DSS_IMAGE_FORMAT_RGBX_5551,
    DSS_IMAGE_FORMAT_BGRA_5551,
    DSS_IMAGE_FORMAT_BGRX_5551,
    DSS_IMAGE_FORMAT_ARGB_1555,
    DSS_IMAGE_FORMAT_XRGB_1555,
    DSS_IMAGE_FORMAT_ABGR_1555,
    DSS_IMAGE_FORMAT_XBGR_1555,
    DSS_IMAGE_FORMAT_RGBA_4444,
    DSS_IMAGE_FORMAT_RGBX_4444,
    DSS_IMAGE_FORMAT_BGRA_4444,
    DSS_IMAGE_FORMAT_BGRX_4444,
    DSS_IMAGE_FORMAT_ARGB_4444,
    DSS_IMAGE_FORMAT_XRGB_4444,
    DSS_IMAGE_FORMAT_ABGR_4444,
    DSS_IMAGE_FORMAT_XBGR_4444,
    DSS_IMAGE_FORMAT_RGB_565,
    DSS_IMAGE_FORMAT_YUV422_S,
    DSS_IMAGE_FORMAT_YUV422_2P,
    DSS_IMAGE_FORMAT_YUV224_2P,
    DSS_IMAGE_FORMAT_YUV420_2P,
    DSS_IMAGE_FORMAT_YUV444_2P,
    DSS_IMAGE_FORMAT_YUV422_3P,
    DSS_IMAGE_FORMAT_YUV224_3P,
    DSS_IMAGE_FORMAT_YUV420_3P,
    DSS_IMAGE_FORMAT_YUV444_3P,
    DSS_IMAGE_FORMAT_YV_12,
    DSS_IMAGE_FORMAT_INVALID
};

enum dss_image_op_rotate {
	DSS_IMAGE_OP_ROTATE_NONE,
	DSS_IMAGE_OP_ROTATE_90,
	DSS_IMAGE_OP_ROTATE_180,
	DSS_IMAGE_OP_ROTATE_270
};

enum dss_image_op_flip {
	DSS_IMAGE_OP_FLIP_NONE,
	DSS_IMAGE_OP_FLIP_H,
	DSS_IMAGE_OP_FLIP_V,
	DSS_IMAGE_OP_FLIP_HV
};

enum dss_image_op_blend {
    DSS_IMAGE_OP_BLEND_NONE,
    DSS_IMAGE_OP_BLEND_CLEAR,
    DSS_IMAGE_OP_BLEND_SRC,
    DSS_IMAGE_OP_BLEND_DST,
    DSS_IMAGE_OP_BLEND_SRC_OVER,
    DSS_IMAGE_OP_BLEND_DST_OVER,
    DSS_IMAGE_OP_BLEND_SRC_IN,
    DSS_IMAGE_OP_BLEND_DST_IN,
    DSS_IMAGE_OP_BLEND_SRC_OUT,
    DSS_IMAGE_OP_BLEND_DST_OUT,
    DSS_IMAGE_OP_BLEND_SRC_ATOP,
    DSS_IMAGE_OP_BLEND_DST_ATOP,
    DSS_IMAGE_OP_BLEND_XOR
};

enum dss_image_assign_res {
	// Out
	DSS_IMAGE_ASSIGN_REJECTED,
	DSS_IMAGE_ASSIGN_RESERVED,
	DSS_IMAGE_ASSIGN_ASSIGNED,
	// In
	DSS_IMAGE_ASSIGN_RESERVED_EXTMERGE,
	DSS_IMAGE_ASSIGN_INVALID
};

#endif /* #ifndef _DSS_TYPES_H_ */
