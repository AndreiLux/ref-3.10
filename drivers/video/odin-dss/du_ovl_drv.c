/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Cheolhyun park <cheolhyun.park@lge.com>
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

#include <linux/spinlock.h>
#include "hal/du_hal.h"
#include "hal/sync_hal.h"

#include "du_ovl_drv.h"
#include "du_rsc.h"

#include "sync_src.h"

//#define DEBUG
//#define DEBUG_RECT
#define		DU_OVL_MAX_PIPE_DEPTH	(30 * 3)
#define		_DU_OVL_SET_RGB( data, r, g, b )			\
{														\
	const unsigned int mask = (1 << 8) - 1;				\
														\
	r = (data >> 16) & mask;							\
	g = (data >> 8) & mask;								\
	b = data & mask;									\
}

static struct
{
	void (*cb_used_done)(struct dss_node_id *node_id, bool rsc_free, struct dss_image *input_image, struct dss_image *output_image);
	void (*cb_ready_to_work)(struct dss_node_id *node_id, struct dss_node *node);
} _du_ovl_db;

struct du_pipe
{
	struct dss_node_id node_id;
	struct dss_node node;
	bool blend;
	bool scale_down;
	bool disable;
};

struct du_rsc_node
{
	struct du_pipe pipe[DU_OVL_MAX_PIPE_DEPTH];
	unsigned int sw_prepare_t;
	unsigned int hw_prepare_t;
	unsigned int hw_working_t;
	unsigned int hw_release_t;

	struct {
		bool undoing;
	} debug;
};

static struct
{
	struct du_rsc_node rsc_node[DU_SRC_NUM];
	bool command_mode;
} du_ovl_sync[SYNC_CH_NUM];

static spinlock_t du_ovl_lock;

static bool __du_ovl_size_print(struct dss_image_size size)
{
	pr_err("w:%d h:%d\n", size.w, size.h);
	return true;
}

static bool __du_ovl_pos_print(struct dss_image_pos pos)
{
	pr_err("x:%d y:%d\n", pos.x, pos.y);
	return true;
}

static bool __du_ovl_rect_print(struct dss_image_rect rect)
{
	__du_ovl_pos_print(rect.pos);
	__du_ovl_size_print(rect.size);
	return true;
}

static enum ovl_zorder ___du_ovl_get_ovl_zorder(unsigned int zorder)
{
	enum ovl_zorder ret = OVL_ZORDER_0;

	switch (zorder) {
	case 0:
		ret = OVL_ZORDER_0;
		break;
	case 1:
		ret = OVL_ZORDER_1;
		break;
	case 2:
		ret = OVL_ZORDER_2;
		break;
	case 3:
		ret = OVL_ZORDER_3;
		break;
	case 4:
		ret = OVL_ZORDER_4;
		break;
	case 5:
		ret = OVL_ZORDER_5;
		break;
	case 6:
		ret = OVL_ZORDER_6;
		break;
	default:
		ret = OVL_ZORDER_0;
		pr_err("zorder: %d\n", zorder);
		break;
	}

	return ret;
}

static enum du_color_format ___du_ovl_get_color_format(enum dss_image_format image_format)
{
	enum du_color_format ret = DU_COLOR_RGB565;

	switch (image_format) {
	case DSS_IMAGE_FORMAT_RGBA_8888:
	case DSS_IMAGE_FORMAT_RGBX_8888:
	case DSS_IMAGE_FORMAT_BGRA_8888:
	case DSS_IMAGE_FORMAT_BGRX_8888:
	case DSS_IMAGE_FORMAT_ARGB_8888:
	case DSS_IMAGE_FORMAT_XRGB_8888:
	case DSS_IMAGE_FORMAT_ABGR_8888:
	case DSS_IMAGE_FORMAT_XBGR_8888:
		ret = DU_COLOR_RGB888;
		break;
	case DSS_IMAGE_FORMAT_RGBA_5551:
	case DSS_IMAGE_FORMAT_RGBX_5551:
	case DSS_IMAGE_FORMAT_BGRA_5551:
	case DSS_IMAGE_FORMAT_BGRX_5551:
	case DSS_IMAGE_FORMAT_ARGB_1555:
	case DSS_IMAGE_FORMAT_XRGB_1555:
	case DSS_IMAGE_FORMAT_ABGR_1555:
	case DSS_IMAGE_FORMAT_XBGR_1555:
		ret = DU_COLOR_RGB555;
		break;
	case DSS_IMAGE_FORMAT_RGBA_4444:
	case DSS_IMAGE_FORMAT_RGBX_4444:
	case DSS_IMAGE_FORMAT_BGRA_4444:
	case DSS_IMAGE_FORMAT_BGRX_4444:
	case DSS_IMAGE_FORMAT_ARGB_4444:
	case DSS_IMAGE_FORMAT_XRGB_4444:
	case DSS_IMAGE_FORMAT_ABGR_4444:
	case DSS_IMAGE_FORMAT_XBGR_4444:
		ret = DU_COLOR_RGB444;
		break;
	case DSS_IMAGE_FORMAT_RGB_565:
		ret = DU_COLOR_RGB565;
		break;
	case DSS_IMAGE_FORMAT_YUV422_S:
		ret = DU_COLOR_YUV422S;
		break;
	case DSS_IMAGE_FORMAT_YUV422_2P:
		ret = DU_COLOR_YUV422_2P;
		break;
	case DSS_IMAGE_FORMAT_YUV224_2P:
		ret = DU_COLOR_YUV224_2P;
		break;
	case DSS_IMAGE_FORMAT_YUV420_2P:
		ret = DU_COLOR_YUV420_2P;
		break;
	case DSS_IMAGE_FORMAT_YUV444_2P:
		ret = DU_COLOR_YUV444_2P;
		break;
	case DSS_IMAGE_FORMAT_YUV422_3P:
		ret = DU_COLOR_YUV422_3P;
		break;
	case DSS_IMAGE_FORMAT_YUV224_3P:
		ret = DU_COLOR_YUV224_3P;
		break;
	case DSS_IMAGE_FORMAT_YUV420_3P:
		ret = DU_COLOR_YUV420_3P;
		break;
	case DSS_IMAGE_FORMAT_YUV444_3P:
		ret = DU_COLOR_YUV444_3P;
		break;
	case DSS_IMAGE_FORMAT_YV_12:
		ret = DU_COLOR_RGB565;
		break;
	case DSS_IMAGE_FORMAT_INVALID:
	default:
		pr_err("image_format: %d\n", image_format);
		ret = DU_COLOR_RGB565;
		break;
	}

	return ret;
}

static enum du_rgb_swap ___du_ovl_get_rgb_swap(enum dss_image_format image_format)
{
	enum du_rgb_swap ret = DU_RGB_RGB;

	switch (image_format) {
	case DSS_IMAGE_FORMAT_RGBA_8888:
	case DSS_IMAGE_FORMAT_RGBX_8888:
	case DSS_IMAGE_FORMAT_ARGB_8888:
	case DSS_IMAGE_FORMAT_XRGB_8888:
	case DSS_IMAGE_FORMAT_RGBA_5551:
	case DSS_IMAGE_FORMAT_RGBX_5551:
	case DSS_IMAGE_FORMAT_ARGB_1555:
	case DSS_IMAGE_FORMAT_XRGB_1555:
	case DSS_IMAGE_FORMAT_RGBA_4444:
	case DSS_IMAGE_FORMAT_RGBX_4444:
	case DSS_IMAGE_FORMAT_ARGB_4444:
	case DSS_IMAGE_FORMAT_XRGB_4444:
	case DSS_IMAGE_FORMAT_RGB_565:
		ret = DU_RGB_RGB;
		break;
	case DSS_IMAGE_FORMAT_BGRA_8888:
	case DSS_IMAGE_FORMAT_BGRX_8888:
	case DSS_IMAGE_FORMAT_ABGR_8888:
	case DSS_IMAGE_FORMAT_XBGR_8888:
	case DSS_IMAGE_FORMAT_BGRA_5551:
	case DSS_IMAGE_FORMAT_BGRX_5551:
	case DSS_IMAGE_FORMAT_ABGR_1555:
	case DSS_IMAGE_FORMAT_XBGR_1555:
	case DSS_IMAGE_FORMAT_BGRA_4444:
	case DSS_IMAGE_FORMAT_BGRX_4444:
	case DSS_IMAGE_FORMAT_ABGR_4444:
	case DSS_IMAGE_FORMAT_XBGR_4444:
		ret = DU_RGB_BGR;
		break;
	case DSS_IMAGE_FORMAT_YUV422_S:
	case DSS_IMAGE_FORMAT_YUV422_2P:
	case DSS_IMAGE_FORMAT_YUV224_2P:
	case DSS_IMAGE_FORMAT_YUV420_2P:
	case DSS_IMAGE_FORMAT_YUV444_2P:
	case DSS_IMAGE_FORMAT_YUV422_3P:
	case DSS_IMAGE_FORMAT_YUV224_3P:
	case DSS_IMAGE_FORMAT_YUV420_3P:
	case DSS_IMAGE_FORMAT_YUV444_3P:
	case DSS_IMAGE_FORMAT_YV_12:
		ret = DU_RGB_RGB;
		break;
	case DSS_IMAGE_FORMAT_INVALID:
	default:
		pr_err("image_format: %d\n", image_format);
		ret = DU_RGB_RGB;
		break;
	}

	return ret;
}

static enum du_alpha_swap ___du_ovl_get_alpha_swap(enum dss_image_format image_format)
{
	enum du_alpha_swap ret = DU_ALPHA_ARGB;

	switch (image_format) {
	case DSS_IMAGE_FORMAT_RGBA_8888:
	case DSS_IMAGE_FORMAT_BGRA_8888:
	case DSS_IMAGE_FORMAT_RGBA_5551:
	case DSS_IMAGE_FORMAT_BGRA_5551:
	case DSS_IMAGE_FORMAT_RGBA_4444:
	case DSS_IMAGE_FORMAT_BGRA_4444:
	case DSS_IMAGE_FORMAT_RGBX_8888:
	case DSS_IMAGE_FORMAT_BGRX_8888:
	case DSS_IMAGE_FORMAT_RGBX_5551:
	case DSS_IMAGE_FORMAT_BGRX_5551:
	case DSS_IMAGE_FORMAT_RGBX_4444:
	case DSS_IMAGE_FORMAT_BGRX_4444:
		ret = DU_ALPHA_RGBA;
		break;
	case DSS_IMAGE_FORMAT_ARGB_8888:
	case DSS_IMAGE_FORMAT_ABGR_8888:
	case DSS_IMAGE_FORMAT_ARGB_1555:
	case DSS_IMAGE_FORMAT_ABGR_1555:
	case DSS_IMAGE_FORMAT_ARGB_4444:
	case DSS_IMAGE_FORMAT_ABGR_4444:
	case DSS_IMAGE_FORMAT_XRGB_8888:
	case DSS_IMAGE_FORMAT_XBGR_8888:
	case DSS_IMAGE_FORMAT_XRGB_1555:
	case DSS_IMAGE_FORMAT_XBGR_1555:
	case DSS_IMAGE_FORMAT_XRGB_4444:
	case DSS_IMAGE_FORMAT_XBGR_4444:
		ret = DU_ALPHA_ARGB;
		break;
	case DSS_IMAGE_FORMAT_RGB_565:
	case DSS_IMAGE_FORMAT_YUV422_S:
	case DSS_IMAGE_FORMAT_YUV422_2P:
	case DSS_IMAGE_FORMAT_YUV224_2P:
	case DSS_IMAGE_FORMAT_YUV420_2P:
	case DSS_IMAGE_FORMAT_YUV444_2P:
	case DSS_IMAGE_FORMAT_YUV422_3P:
	case DSS_IMAGE_FORMAT_YUV224_3P:
	case DSS_IMAGE_FORMAT_YUV420_3P:
	case DSS_IMAGE_FORMAT_YUV444_3P:
	case DSS_IMAGE_FORMAT_YV_12:
		ret = DU_ALPHA_ARGB;
		break;
	case DSS_IMAGE_FORMAT_INVALID:
	default:
		pr_err("image_format: %d\n", image_format);
		ret = DU_ALPHA_ARGB;
		break;
	}

	return ret;
}

static enum du_flip_mode ___du_ovl_get_flip_mode(enum dss_image_op_flip image_op_flip)
{
	enum du_flip_mode ret = DU_FLIP_NONE;

	switch (image_op_flip) {
	case DSS_IMAGE_OP_FLIP_NONE:
		ret = DU_FLIP_NONE;
		break;
	case DSS_IMAGE_OP_FLIP_H:
		ret = DU_FLIP_V;
		break;
	case DSS_IMAGE_OP_FLIP_V:
		ret = DU_FLIP_H;
		break;
	case DSS_IMAGE_OP_FLIP_HV:
		ret = DU_FLIP_HV;
		break;
	default:
		ret = DU_FLIP_NONE;
		pr_err("image_op_flip: %d\n", image_op_flip);
		break;
	}

	return ret;
}

static enum ovl_operation ___du_ovl_get_ovl_comp_mode(enum dss_image_op_blend image_op_blend)
{
	enum ovl_operation ret = OVL_OPS_CLEAR;

	switch (image_op_blend) {
	case DSS_IMAGE_OP_BLEND_NONE:
		ret = OVL_OPS_CLEAR;
		break;
	case DSS_IMAGE_OP_BLEND_CLEAR:
		ret = OVL_OPS_CLEAR;
		break;
	case DSS_IMAGE_OP_BLEND_SRC:
		ret = OVL_OPS_SRC;
		break;
	case DSS_IMAGE_OP_BLEND_DST:
		ret = OVL_OPS_DST;
		break;
	case DSS_IMAGE_OP_BLEND_SRC_OVER:
		ret = OVL_OPS_SRC_OVER;
		break;
	case DSS_IMAGE_OP_BLEND_DST_OVER:
		ret = OVL_OPS_DST_OVER;
		break;
	case DSS_IMAGE_OP_BLEND_SRC_IN:
		ret = OVL_OPS_SRC_IN;
		break;
	case DSS_IMAGE_OP_BLEND_DST_IN:
		ret = OVL_OPS_DST_IN;
		break;
	case DSS_IMAGE_OP_BLEND_SRC_OUT:
		ret = OVL_OPS_SRC_OUT;
		break;
	case DSS_IMAGE_OP_BLEND_DST_OUT:
		ret = OVL_OPS_DST_OUT;
		break;
	case DSS_IMAGE_OP_BLEND_SRC_ATOP:
		ret = OVL_OPS_SRC_ATOP;
		break;
	case DSS_IMAGE_OP_BLEND_DST_ATOP:
		ret = OVL_OPS_DST_ATOP;
		break;
	case DSS_IMAGE_OP_BLEND_XOR:
		ret = OVL_OPS_XOR;
		break;
	default:
		pr_err("image_op_blend: %d\n", image_op_blend);
		ret = OVL_OPS_CLEAR;
		break;
	}

	return ret;
}

static bool ___du_ovl_cal_uv_offset(enum du_color_format color_format, int width, int height, int y_addr, int *get_u, int *get_v)
{
	bool ret = false;
	int offset = width * height;
	*get_u = 0;
	if (get_v)
		*get_v = 0;

	switch (color_format) {
	case DU_COLOR_RGB565:
	case DU_COLOR_RGB555:
	case DU_COLOR_RGB444:
	case DU_COLOR_RGB888:
		ret = true;
		break;
	case DU_COLOR_YUV422S:
	case DU_COLOR_YUV422_2P:
	case DU_COLOR_YUV224_2P:
	case DU_COLOR_YUV420_2P:
	case DU_COLOR_YUV444_2P:
	case DU_COLOR_YUV422_3P:
		*get_u = y_addr + offset;
		ret = true;
		break;
	case DU_COLOR_YUV224_3P:
		*get_u = y_addr + offset;
		if (get_v)
			*get_v = y_addr + offset + (offset >> 1);
		ret = true;
		break;
	case DU_COLOR_YUV420_3P:
		*get_u = y_addr + offset;
		if (get_v)
			*get_v = y_addr + offset + (offset >> 2);
		ret = true;
		break;
	case DU_COLOR_YUV444_3P:
		*get_u = y_addr + offset;
		if (get_v)
			*get_v = y_addr + (offset << 1);
		ret = true;
		break;
	default:
		pr_err("color_format: %d\n", color_format);
		ret = false;
		break;
	}

	return ret;
}

static enum dss_image_format __du_ovl_get_scale_down_color_format(enum dss_image_format image_format)
{
	enum dss_image_format ret = DSS_IMAGE_FORMAT_RGBA_8888;

	switch (image_format) {
	case DSS_IMAGE_FORMAT_RGBA_8888:
	case DSS_IMAGE_FORMAT_RGBX_8888:
	case DSS_IMAGE_FORMAT_BGRA_8888:
	case DSS_IMAGE_FORMAT_BGRX_8888:
	case DSS_IMAGE_FORMAT_ARGB_8888:
	case DSS_IMAGE_FORMAT_XRGB_8888:
	case DSS_IMAGE_FORMAT_ABGR_8888:
	case DSS_IMAGE_FORMAT_XBGR_8888:
	case DSS_IMAGE_FORMAT_RGBA_5551:
	case DSS_IMAGE_FORMAT_RGBX_5551:
	case DSS_IMAGE_FORMAT_BGRA_5551:
	case DSS_IMAGE_FORMAT_BGRX_5551:
	case DSS_IMAGE_FORMAT_ARGB_1555:
	case DSS_IMAGE_FORMAT_XRGB_1555:
	case DSS_IMAGE_FORMAT_ABGR_1555:
	case DSS_IMAGE_FORMAT_XBGR_1555:
	case DSS_IMAGE_FORMAT_RGBA_4444:
	case DSS_IMAGE_FORMAT_RGBX_4444:
	case DSS_IMAGE_FORMAT_BGRA_4444:
	case DSS_IMAGE_FORMAT_BGRX_4444:
	case DSS_IMAGE_FORMAT_ARGB_4444:
	case DSS_IMAGE_FORMAT_XRGB_4444:
	case DSS_IMAGE_FORMAT_ABGR_4444:
	case DSS_IMAGE_FORMAT_XBGR_4444:
		ret = image_format;
		break;
	case DSS_IMAGE_FORMAT_YUV422_S:
	case DSS_IMAGE_FORMAT_YUV422_2P:
	case DSS_IMAGE_FORMAT_YUV224_2P:
	case DSS_IMAGE_FORMAT_YUV420_2P:
	case DSS_IMAGE_FORMAT_YUV444_2P:
	case DSS_IMAGE_FORMAT_YUV422_3P:
	case DSS_IMAGE_FORMAT_YUV224_3P:
	case DSS_IMAGE_FORMAT_YUV420_3P:
	case DSS_IMAGE_FORMAT_YUV444_3P:
	case DSS_IMAGE_FORMAT_YV_12:
		ret = DSS_IMAGE_FORMAT_ARGB_8888;
		break;
	case DSS_IMAGE_FORMAT_INVALID:
	default:
		pr_err("invalid input pixel format\n");
		ret = DSS_IMAGE_FORMAT_RGBA_8888;
		break;
	}
	return ret;
}

bool _du_ovl_du_rsc_disable(enum du_src_ch_id du_src_ch, struct dss_node *node)
{
	bool ret = false;

	switch (du_src_ch) {
	case DU_SRC_VID0:
	case DU_SRC_VID1:
	case DU_SRC_VID2:
		if (node->input.image.port == DSS_DISPLAY_PORT_MEM) {
			ret = du_hal_vid_disable(du_src_ch);
			if (node->output.image.port == DSS_DISPLAY_PORT_MEM) {
				sync_hal_channel_wb_disable(du_src_ch);
			}
		}
		break;
	case DU_SRC_GSCL:
		if (node->input.image.port != DSS_DISPLAY_PORT_MEM && node->output.image.port == DSS_DISPLAY_PORT_MEM) {
			ret = du_hal_gra_disable(du_src_ch);
			sync_hal_channel_wb_disable(du_src_ch);
			break;
		}
		/* no break */
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
		if (node->input.image.port == DSS_DISPLAY_PORT_MEM && node->output.image.port != DSS_DISPLAY_PORT_MEM) {
			ret = du_hal_gra_disable(du_src_ch);
		}
		break;
	case DU_SRC_NUM:
	default:
		ret = false;
		pr_err("invalid du_src_ch: %d\n", du_src_ch);
		break;
	}

	return ret;
}

static bool _du_ovl_du_rsc_enable(enum du_src_ch_id du_src_ch, enum sync_ch_id sync_ch, struct dss_node_id *node_id, struct dss_node *node)
{
	struct dss_image *input_image, *output_image;
	struct dss_input_meta *input_meta;
	struct dss_output_meta *output_meta;
	bool ret = false;

	struct dss_image_pos ovl_image_pos;
	unsigned int vstart, hstart;
	unsigned int addr_y, addr_u, addr_v;
	unsigned int out_addr_y, out_addr_u, out_addr_v;
	unsigned char pattern_data_r, pattern_data_g, pattern_data_b;
	unsigned char chroma_mask_r, chroma_mask_g, chroma_mask_b;
	unsigned char chroma_data_r, chroma_data_g, chroma_data_b;
	unsigned char alpha_value;
	struct dss_image_size src_image_size, pipe_image_size, ovl_size;
	struct dss_image_rect crop_image_rect, wb_crop_image_rect;
	enum ovl_raster_ops raster_op;
	enum ovl_operation ops;
	enum ovl_ch_id ovl_ch;
	enum du_yuv_swap yuv_swap;
	enum du_word_swap word_swap;
	enum du_rgb_swap rgb_swap;
	enum du_lsb_padding_sel lsb_sel;
	enum du_flip_mode flip_mode;
	enum du_color_format color_format;
	enum du_color_format output_color_format;
	enum du_byte_swap byte_swap;
	enum du_bnd_fill_mode bnd_fill_mode;
	enum du_alpha_swap alpha_swap;
	bool pre_mul_enable, se_src_secure, se_src_sel_secure, se_src_layer_secure, raster, ovl_alpha_enable, dual_op_enable, chromakey_enable,
			alpha_reg_enable;

	// null check //
	if (node_id->instance_id == NULL) {
		pr_err("pipe.node_id.instance_id is NULL\n");
		return false;
	}

	input_image = &node->input.image;
	output_image = &node->output.image;
	input_meta = &node->input.in_meta;
	output_meta = &node->output.out_meta;

	// input_image //
	rgb_swap = ___du_ovl_get_rgb_swap(input_image->pixel_format);
	alpha_swap = ___du_ovl_get_alpha_swap(input_image->pixel_format);
	color_format = ___du_ovl_get_color_format(input_image->pixel_format);
	if (output_image->port == DSS_DISPLAY_PORT_MEM) {
		output_color_format = ___du_ovl_get_color_format(output_image->pixel_format);
	}

	addr_y = input_image->phy_addr;
	___du_ovl_cal_uv_offset(color_format, input_image->image_size.w, input_image->image_size.h, addr_y, &addr_u, &addr_v);
	out_addr_y = output_image->phy_addr;
	___du_ovl_cal_uv_offset(color_format, input_image->image_size.w, input_image->image_size.h, out_addr_y, &out_addr_u, &out_addr_v);
	src_image_size = input_image->image_size;

	// input_meta //
	ops = ___du_ovl_get_ovl_comp_mode(input_meta->blend_op);
	flip_mode = ___du_ovl_get_flip_mode(input_meta->flip_op);
	chromakey_enable = input_meta->chrom_enable;
	_DU_OVL_SET_RGB(input_meta->chrom_data, chroma_data_r, chroma_data_g, chroma_data_b);
	_DU_OVL_SET_RGB(input_meta->chrom_mask, chroma_mask_r, chroma_mask_g, chroma_mask_b);
	pre_mul_enable = input_meta->premult_alpha;
	alpha_value = input_meta->global_alpha;
	ovl_alpha_enable = input_meta->global_alpha > 0;
	crop_image_rect = input_meta->crop_rect;
	wb_crop_image_rect = input_meta->dst_rect;
	ovl_image_pos.x = input_meta->dst_rect.pos.x;
	ovl_image_pos.y = input_meta->dst_rect.pos.y;
	pipe_image_size = input_meta->dst_rect.size;

	// output image //
	ovl_size = output_image->image_size;

	// ouput meta //
	ovl_ch = output_meta->ovl_ch;
	_DU_OVL_SET_RGB(output_meta->pattern_data, pattern_data_r, pattern_data_g, pattern_data_b);

	// hard coding : enum//
	byte_swap = DU_BYTE_ABCD;
	word_swap = DU_WORD_AB;
	yuv_swap = DU_YUV_UV;
	lsb_sel = DU_PAD_MSB;
	bnd_fill_mode = DU_FILL_BOUNDARY_COLOR;
	raster_op = OVL_RASTER_OPS_0;

	// hard coding : magic_number //
	alpha_reg_enable = 0;
	dual_op_enable = 0;
	se_src_secure = 0;
	se_src_sel_secure = 0;
	se_src_layer_secure = 0;
	raster = 0;
	vstart = 1;
	hstart = 1;

	switch (du_src_ch) {
	case DU_SRC_VID0:
	case DU_SRC_VID1:
	case DU_SRC_VID2:
		if (input_image->port != DSS_DISPLAY_PORT_MEM) {
			pr_err("invalid input port(%d)\n", input_image->port);
			return false;
		}
#ifdef DEBUG_RECT
		pr_err("[vid_enable]\n");
		__du_ovl_size_print(src_image_size);
		__du_ovl_rect_print(crop_image_rect);
		__du_ovl_size_print(pipe_image_size);
#endif
		ret = du_hal_vid_enable(du_src_ch, dual_op_enable, sync_ch, color_format, byte_swap, word_swap, alpha_swap, rgb_swap, yuv_swap, lsb_sel,
				bnd_fill_mode, flip_mode, src_image_size, crop_image_rect, pipe_image_size, addr_y, addr_u, addr_v);

		if (output_image->port == DSS_DISPLAY_PORT_MEM) {
			sync_hal_channel_wb_st_dly(du_src_ch, vstart, hstart);
			sync_hal_channel_wb_enable(du_src_ch);
#ifdef DEBUG_RECT
			pr_err("[vid_wb_enable]\n");
			__du_ovl_rect_print(wb_crop_image_rect);
#endif
			src_image_size = wb_crop_image_rect.size;
			ret = du_hal_vid_wb_enable(du_src_ch, alpha_reg_enable, sync_ch, output_color_format, byte_swap, word_swap, alpha_swap, rgb_swap,
					yuv_swap, 0, alpha_value, src_image_size, wb_crop_image_rect, out_addr_y, out_addr_u);
		}
		break;
	case DU_SRC_GSCL:
		if (input_image->port != DSS_DISPLAY_PORT_MEM && output_image->port == DSS_DISPLAY_PORT_MEM) {
			sync_hal_channel_wb_st_dly(du_src_ch, vstart, hstart);
			sync_hal_channel_wb_enable(du_src_ch);
			ret = du_hal_gscl_wb_enable(alpha_reg_enable, sync_ch, output_color_format, byte_swap, word_swap, alpha_swap, rgb_swap, yuv_swap, ovl_ch,
					alpha_value, src_image_size, crop_image_rect, out_addr_y, out_addr_u);
			break;
		}
		/* no break */
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
		if (input_image->port == DSS_DISPLAY_PORT_MEM && output_image->port != DSS_DISPLAY_PORT_MEM) {
#ifdef DEBUG_RECT
			pr_err("[gra_enable]\n");
			__du_ovl_size_print(src_image_size);
			__du_ovl_rect_print(crop_image_rect);
#endif
			ret = du_hal_gra_enable(du_src_ch, sync_ch, color_format, byte_swap, word_swap, alpha_swap, rgb_swap, lsb_sel, flip_mode, src_image_size,
					crop_image_rect, addr_y);
		} else {
			pr_err("DU_SRC_GRAx enable is failed.(input_port : %d, output_port : %d)\n", input_image->port, output_image->port);
		}
		break;
	case DU_SRC_NUM:
	default:
		ret = false;
		pr_err("du_src_ch: %d\n", du_src_ch);
		break;
	}

	return ret;
}

static bool _du_ovl_channel_param_set(struct ovl_hal_set_param *param, struct dss_node_id *node_id, struct dss_node *node)
{
	struct dss_image *input_image, *output_image;
	struct dss_input_meta *input_meta;
	struct dss_output_meta *output_meta;
	bool ret = false;

	unsigned int vstart, hstart;
	unsigned int addr_y, addr_u, addr_v;
	unsigned int out_addr_y, out_addr_u, out_addr_v;
	unsigned char pattern_data_r, pattern_data_g, pattern_data_b;
	unsigned char chroma_mask_r, chroma_mask_g, chroma_mask_b;
	unsigned char chroma_data_r, chroma_data_g, chroma_data_b;
	unsigned char alpha_value;
	struct dss_image_size src_image_size, pipe_image_size, ovl_size, ovl_pipe_gra_size, ovl_pipe_vid_size;
	struct dss_image_rect crop_image_rect, wb_crop_image_rect;
	struct dss_image_pos ovl_image_pos;
	enum ovl_zorder zorder;
	enum ovl_raster_ops raster_op;
	enum ovl_operation ops;
	enum ovl_ch_id ovl_ch;
	enum du_yuv_swap yuv_swap;
	enum du_word_swap word_swap;
	enum du_rgb_swap rgb_swap;
	enum du_lsb_padding_sel lsb_sel;
	enum du_flip_mode flip_mode;
	enum du_color_format color_format;
	enum du_color_format output_color_format;
	enum du_byte_swap byte_swap;
	enum du_bnd_fill_mode bnd_fill_mode;
	enum du_alpha_swap alpha_swap;
	bool pre_mul_enable, se_src_secure, se_src_sel_secure, se_src_layer_secure, raster, ovl_alpha_enable, dual_op_enable, chromakey_enable,
			alpha_reg_enable;

	// null check //
	if (node_id->instance_id == NULL) {
		pr_err("pipe.node_id.instance_id is NULL\n");
		return false;
	}

	if (node->output.image.port == DSS_DISPLAY_PORT_MEM) {
		pr_err("Invalid output port(%d)\n", node->output.image.port);
		return false;
	}

	input_image = &node->input.image;
	output_image = &node->output.image;
	input_meta = &node->input.in_meta;
	output_meta = &node->output.out_meta;

	// input_image //
	rgb_swap = ___du_ovl_get_rgb_swap(input_image->pixel_format);
	alpha_swap = ___du_ovl_get_alpha_swap(input_image->pixel_format);
	color_format = ___du_ovl_get_color_format(input_image->pixel_format);

	if (output_image->port == DSS_DISPLAY_PORT_MEM) {
		output_color_format = ___du_ovl_get_color_format(output_image->pixel_format);
	}

	addr_y = input_image->phy_addr;
	___du_ovl_cal_uv_offset(color_format, input_image->image_size.w, input_image->image_size.h, addr_y, &addr_u, &addr_v);
	out_addr_y = output_image->phy_addr;
	___du_ovl_cal_uv_offset(color_format, input_image->image_size.w, input_image->image_size.h, out_addr_y, &out_addr_u, &out_addr_v);
	src_image_size = input_image->image_size;

	// input_meta //
	ops = ___du_ovl_get_ovl_comp_mode(input_meta->blend_op);
	flip_mode = ___du_ovl_get_flip_mode(input_meta->flip_op);
	chromakey_enable = input_meta->chrom_enable;
	_DU_OVL_SET_RGB(input_meta->chrom_data, chroma_data_r, chroma_data_g, chroma_data_b);
	_DU_OVL_SET_RGB(input_meta->chrom_mask, chroma_mask_r, chroma_mask_g, chroma_mask_b);
	pre_mul_enable = input_meta->premult_alpha;
	alpha_value = input_meta->global_alpha;
	ovl_alpha_enable = input_meta->global_alpha > 0;
	crop_image_rect = input_meta->crop_rect;
	wb_crop_image_rect = input_meta->dst_rect;
	ovl_image_pos.x = input_meta->dst_rect.pos.x;
	ovl_image_pos.y = input_meta->dst_rect.pos.y;
	ovl_pipe_gra_size = input_meta->dst_rect.size;
	ovl_pipe_vid_size = input_meta->dst_rect.size;
	pipe_image_size = input_meta->dst_rect.size;
	zorder = ___du_ovl_get_ovl_zorder(input_meta->zorder);

	// output image //
	ovl_size = output_image->image_size;

	// ouput meta //
	ovl_ch = output_meta->ovl_ch;
	_DU_OVL_SET_RGB(output_meta->pattern_data, pattern_data_r, pattern_data_g, pattern_data_b);

	// hard coding : enum//
	byte_swap = DU_BYTE_ABCD;
	word_swap = DU_WORD_AB;
	yuv_swap = DU_YUV_UV;
	lsb_sel = DU_PAD_MSB;
	bnd_fill_mode = DU_FILL_BOUNDARY_COLOR;
	raster_op = OVL_RASTER_OPS_0;

	// hard coding : magic_number //
	alpha_reg_enable = 0;
	dual_op_enable = 0;
	se_src_secure = 0;
	se_src_sel_secure = 0;
	se_src_layer_secure = 0;
	raster = 0;
	vstart = 1;
	hstart = 1;

	param->ovl_image_pos = ovl_image_pos;
	param->ovl_pip_size = ovl_pipe_gra_size;
	param->ops = ops;
	param->ovl_alpha_enable = ovl_alpha_enable;
	param->alpha_value = alpha_value;
	param->pre_mul_enable = pre_mul_enable;
	param->chromakey_enable = chromakey_enable;
	param->chroma_data_r = chroma_data_r;
	param->chroma_data_g = chroma_data_g;
	param->chroma_data_b = chroma_data_b;
	param->chroma_mask_r = chroma_mask_r;
	param->chroma_mask_g = chroma_mask_g;
	param->chroma_mask_b = chroma_mask_b;
	param->se_src_secure = se_src_secure;
	param->se_src_sel_secure = se_src_sel_secure;
	param->se_src_layer_secure = se_src_layer_secure;

	return ret;
}

static void _du_ovl_work_sync(enum sync_ch_id sync_ch, void *arg)
{
#define du_ovl_tick_inc(t)	(((t)+1)%DU_OVL_MAX_PIPE_DEPTH)
	enum du_src_ch_id du_src_i;
	unsigned int sw_prepare_t[DU_SRC_NUM];
	unsigned int hw_prepare_t[DU_SRC_NUM];
	unsigned int hw_working_t[DU_SRC_NUM];
	enum ovl_ch_id ovl_ch_i;
	enum ovl_zorder ovl_zorder_i;
	unsigned int sync_tick;
	bool du_rsc_enabled[DU_SRC_NUM] = { false, };
	unsigned long flags;
	struct du_rsc_node *du_rsc_node;
	struct ovl_hal_set_param param[OVL_CH_NUM][OVL_ZORDER_NR];
	bool ovl_ch_active[OVL_CH_NUM] = { false, };
	bool ret = false;

	sync_tick = sync_global_tick_get();

	memset(param, 0, sizeof(param));
	for (ovl_ch_i = OVL_CH_ID0; ovl_ch_i < OVL_CH_NUM; ovl_ch_i++) {
		for (ovl_zorder_i = OVL_ZORDER_0; ovl_zorder_i < OVL_ZORDER_NR; ovl_zorder_i++) {
			param[ovl_ch_i][ovl_zorder_i].ch = DU_SRC_NUM;
		}
	}

	spin_lock_irqsave(&du_ovl_lock, flags);

	for (du_src_i = DU_SRC_VID0; du_src_i < DU_SRC_NUM; du_src_i++)
		sw_prepare_t[du_src_i] = du_ovl_sync[sync_ch].rsc_node[du_src_i].sw_prepare_t;

	for (du_src_i = DU_SRC_VID0; du_src_i < DU_SRC_NUM; du_src_i++) {
		hw_prepare_t[du_src_i] = du_ovl_sync[sync_ch].rsc_node[du_src_i].hw_prepare_t;
		hw_working_t[du_src_i] = du_ovl_sync[sync_ch].rsc_node[du_src_i].hw_working_t;
	}

	/* disable worked pipe */
	for (du_src_i = DU_SRC_VID0; du_src_i < DU_SRC_NUM; du_src_i++) {
		struct du_pipe *worked_pipe = NULL;

		if (hw_prepare_t[du_src_i] == hw_working_t[du_src_i])
			continue;

		du_rsc_node = &du_ovl_sync[sync_ch].rsc_node[du_src_i];
		worked_pipe = &du_rsc_node->pipe[hw_working_t[du_src_i]];

		if (worked_pipe->node.undo == true) {
			worked_pipe->disable = true;
			du_rsc_node->hw_working_t = hw_prepare_t[du_src_i];
		} else if ((worked_pipe->blend == false) || du_ovl_sync[sync_ch].command_mode) {
#ifdef DEBUG
			pr_err("[disable] rsc:%d tick:%u pipe:%u in:0x%x out:0x%x\n", du_src_i, sync_tick, hw_working_t[du_src_i],
					worked_pipe->node.input.image.phy_addr, worked_pipe->node.output.image.phy_addr);
#endif
			sync_hal_channel_disable(du_src_i);
			ret = _du_ovl_du_rsc_disable(du_src_i, &worked_pipe->node);
			if (ret == false) {
				pr_err("[ERROR] rsc_disable is failed : rsc:%d in-port:%d out-port:%d\n", du_src_i, worked_pipe->node.input.image.port,
						worked_pipe->node.output.image.port);
			}
			worked_pipe->disable = true;

			/* marking ovl channel disable */
			if (worked_pipe->blend) {
				enum ovl_ch_id ovl_ch;
				ovl_ch = worked_pipe->node.output.out_meta.ovl_ch;
				ovl_ch_active[ovl_ch] = true;
			}

			du_rsc_node->hw_working_t = hw_prepare_t[du_src_i];
		}
		else if(worked_pipe->blend) {
			/* marking ovl channel enable */
			enum ovl_ch_id ovl_ch;
			enum ovl_zorder zorder;

			ovl_ch = worked_pipe->node.output.out_meta.ovl_ch;
			zorder = ___du_ovl_get_ovl_zorder(worked_pipe->node.input.in_meta.zorder);

			param[ovl_ch][zorder].ch = du_src_i;
			_du_ovl_channel_param_set(&param[ovl_ch][zorder], &worked_pipe->node_id, &worked_pipe->node);
			ovl_ch_active[ovl_ch] = true;
		}
	}

	/* enable be wokring pipe */
	for (du_src_i = DU_SRC_VID0; du_src_i < DU_SRC_NUM; du_src_i++) {
		struct dss_node ready_to_work_node;
		struct dss_node_id ready_to_work_node_id;
		struct du_pipe *be_working_pipe = NULL;

		if (sw_prepare_t[du_src_i] == hw_prepare_t[du_src_i])
			continue;

		memset(&ready_to_work_node, 0, sizeof(struct dss_node));
		memset(&ready_to_work_node_id, 0, sizeof(struct dss_node_id));

		du_rsc_node = &du_ovl_sync[sync_ch].rsc_node[du_src_i];
		be_working_pipe = &du_rsc_node->pipe[hw_prepare_t[du_src_i]];

		ready_to_work_node = be_working_pipe->node;
		ready_to_work_node_id = be_working_pipe->node_id;

		if (be_working_pipe->scale_down == true && be_working_pipe->blend == false) {
			be_working_pipe->node.output.image.port = DSS_DISPLAY_PORT_MEM;
			be_working_pipe->node.output.image.phy_addr = be_working_pipe->node.int_out_buf[be_working_pipe->node_id.node_id];
			be_working_pipe->node.output.image.pixel_format = __du_ovl_get_scale_down_color_format(be_working_pipe->node.input.image.pixel_format);
			be_working_pipe->node.input.in_meta.dst_rect.pos.x = 0;
			be_working_pipe->node.input.in_meta.dst_rect.pos.y = 0;

			ready_to_work_node.input.in_meta.crop_rect = be_working_pipe->node.input.in_meta.dst_rect;
			ready_to_work_node.input.image.image_size = be_working_pipe->node.input.in_meta.dst_rect.size;
			ready_to_work_node.input.image.pixel_format = be_working_pipe->node.output.image.pixel_format;
		}

		if (be_working_pipe->node.undo) {
			unsigned int hw_prepare_prev_t = (hw_prepare_t[du_src_i] != 0) ? (hw_prepare_t[du_src_i] - 1) : (DU_OVL_MAX_PIPE_DEPTH - 1);
			struct du_pipe *prev_worked_pipe = &du_rsc_node->pipe[hw_prepare_prev_t];
			if (prev_worked_pipe->disable == false) {
#ifdef DEBUG
				pr_err("[disable] rsc:%d tick:%u pipe:%u in:0x%x out:0x%x\n", du_src_i, sync_tick, hw_prepare_prev_t,
						prev_worked_pipe->node.input.image.phy_addr, prev_worked_pipe->node.output.image.phy_addr);
#endif
				sync_hal_channel_disable(du_src_i);
				ret = _du_ovl_du_rsc_disable(du_src_i, &prev_worked_pipe->node);
				if (ret == false) {
					pr_err("[ERROR] pre-rsc_disable is failed : rsc:%d in-port:%d out-port:%d\n", du_src_i, prev_worked_pipe->node.input.image.port,
							prev_worked_pipe->node.output.image.port);
				}
				prev_worked_pipe->disable = true;

				/* marking ovl channel disable */
				if (prev_worked_pipe->blend) {
					enum ovl_ch_id ovl_ch;
					ovl_ch = prev_worked_pipe->node.output.out_meta.ovl_ch;
					ovl_ch_active[ovl_ch] = true;
				}
			}
		} else {
			if (be_working_pipe->node.output.image.port != DSS_DISPLAY_PORT_MEM && be_working_pipe->blend == true) {
				/* marking ovl channel enable */
				enum ovl_ch_id ovl_ch;
				enum ovl_zorder zorder;

				ovl_ch = be_working_pipe->node.output.out_meta.ovl_ch;
				zorder = ___du_ovl_get_ovl_zorder(be_working_pipe->node.input.in_meta.zorder);

				param[ovl_ch][zorder].ch = du_src_i;
				_du_ovl_channel_param_set(&param[ovl_ch][zorder], &be_working_pipe->node_id, &be_working_pipe->node);
				ovl_ch_active[ovl_ch] = true;
			}
#ifdef DEBUG
			pr_err("[enable] rsc:%d tick:%u pipe:%u in:0x%x out:0x%x\n", du_src_i, sync_tick, hw_prepare_t[du_src_i],
					be_working_pipe->node.input.image.phy_addr, be_working_pipe->node.output.image.phy_addr);
#endif
			if (du_rsc_node->debug.undoing)
				pr_err("doing undoing node\n");

			ret = _du_ovl_du_rsc_enable(du_src_i, sync_ch, &be_working_pipe->node_id, &be_working_pipe->node);
			if (ret == false) {
				pr_err("[ERROR] rsc_enable is failed : rsc:%d\n", du_src_i);
			}
			du_rsc_enabled[du_src_i] = true;
		}

		du_rsc_node->hw_working_t = hw_prepare_t[du_src_i];
		du_rsc_node->hw_prepare_t = du_ovl_tick_inc(hw_prepare_t[du_src_i]);

#ifdef DEBUG
		pr_err("[cb_ready_to_work] rsc:%d tick:%u pipe:%u in:0x%x out:0x%x\n", du_src_i, sync_tick, hw_prepare_t[du_src_i],
				ready_to_work_node.input.image.phy_addr, ready_to_work_node.output.image.phy_addr);
#endif
		_du_ovl_db.cb_ready_to_work(&ready_to_work_node_id, &ready_to_work_node);
	}

	for (ovl_ch_i = OVL_CH_ID0; ovl_ch_i < OVL_CH_NUM; ovl_ch_i++) {
		if (ovl_ch_active[ovl_ch_i] == false)
			continue;
#ifdef DEBUG_RECT
		pr_err("[ovl_hal_set] ovl_ch:%d tick:%u\n", ovl_ch_i, sync_tick);
		for (ovl_zorder_i = OVL_ZORDER_0; ovl_zorder_i < OVL_ZORDER_NR; ovl_zorder_i++) {
			if (param[ovl_ch_i][ovl_zorder_i].ch == DU_SRC_NUM)
				continue;
			pr_err("[zorder:%d]\n", ovl_zorder_i);
			__du_ovl_pos_print(param[ovl_ch_i][ovl_zorder_i].ovl_image_pos);
			__du_ovl_size_print(param[ovl_ch_i][ovl_zorder_i].ovl_pip_size);
		}
#endif
		ovl_hal_set(ovl_ch_i, param[ovl_ch_i]);
	}

	for (du_src_i = DU_SRC_VID0; du_src_i < DU_SRC_NUM; du_src_i++) {
		if (du_rsc_enabled[du_src_i]) {
			sync_hal_channel_st_dly(du_src_i, 1, 1);
			sync_hal_channel_enable(du_src_i);
		}
	}

	/* call used done callback */
	for (du_src_i = DU_SRC_VID0; du_src_i < DU_SRC_NUM; du_src_i++) {
		struct du_pipe *release_pipe = NULL;
		bool rsc_free;

		du_rsc_node = &du_ovl_sync[sync_ch].rsc_node[du_src_i];

		if (hw_working_t[du_src_i] == du_rsc_node->hw_release_t)
			continue;

		rsc_free = !du_rsc_enabled[du_src_i] && (sw_prepare_t[du_src_i] == hw_prepare_t[du_src_i])
				&& (hw_prepare_t[du_src_i] == hw_working_t[du_src_i]);

		release_pipe = &du_rsc_node->pipe[du_rsc_node->hw_release_t];
#ifdef DEBUG
		pr_err("[cb_used_done] rsc:%d tick:%u pipe:%u release:%d in:0x%x out:0x%x\n", du_src_i, sync_tick, du_rsc_node->hw_release_t, rsc_free,
				release_pipe->node.input.image.phy_addr, release_pipe->node.output.image.phy_addr);
#endif
		_du_ovl_db.cb_used_done(&release_pipe->node_id, rsc_free, &release_pipe->node.input.image, &release_pipe->node.output.image);
		du_rsc_node->hw_release_t = hw_working_t[du_src_i];
	}
	spin_unlock_irqrestore(&du_ovl_lock, flags);
}

bool du_ovl_do_imgs(struct dss_node_id node_id[], struct dss_node node[], enum sync_ch_id sync_ch, bool command_mode)
{
	unsigned long flags;
	enum du_src_ch_id du_src_i;

	spin_lock_irqsave(&du_ovl_lock, flags);

	for (du_src_i = 0; du_src_i < DU_SRC_NUM; du_src_i++) {
		if (node_id[du_src_i].instance_id == NULL)
			continue;

		if (node[du_src_i].undo == false) {
			// do //
			du_ovl_do_img(du_src_i, &node_id[du_src_i], &node[du_src_i], sync_ch, command_mode, true);
		} else {
			// undo //
			du_ovl_do_img(du_src_i, &node_id[du_src_i], &node[du_src_i], sync_ch, command_mode, true);
			if (node[du_src_i].input.image.phy_addr != 0) {
				node[du_src_i].undo = false;
				// do //
				du_ovl_do_img(du_src_i, &node_id[du_src_i], &node[du_src_i], sync_ch, command_mode, true);
			}
		}
	}

	spin_unlock_irqrestore(&du_ovl_lock, flags);

	return true;
}

bool du_ovl_do_img(enum du_src_ch_id du_src_ch, struct dss_node_id *node_id, struct dss_node *node, enum sync_ch_id sync_ch, bool command_mode,
		bool locked)
{
	unsigned int sw_prepare_t, sw_prepare_t_next;
	unsigned int hw_release_t;
	unsigned int hw_prepare_t, hw_prepare_t_next;
	unsigned int pipe_i;
	int crop_area, dst_area;
	struct du_rsc_node *rsc_node;
	struct du_pipe *pipe_sw_prepare_t;
	unsigned long flags;

	if (du_src_ch >= DU_SRC_NUM) {
		pr_err("invalid resource id(%d)\n", du_src_ch);
		return false;
	}

	if (sync_ch >= SYNC_CH_NUM) {
		pr_err("invalid sync_ch(%d)\n", sync_ch);
		return false;
	}

	if (sync_is_enable(sync_ch) == false) {
		pr_err("sync sync_ch(%d) is not ready\n", sync_ch);
		return false;
	}

	if (node->input.in_meta.mxd_op == true) {
		if ((du_src_ch != DU_SRC_VID0) && (du_src_ch != DU_SRC_VID1)) {
			pr_err("inappropriate resource id(%d) for mobile xd\n", du_src_ch);
			return false;
		}
	}

	if (node_id->node_id >= (DSS_MAX_PIPE_DEPTH)) {
		pr_err("invalid node id(%d)\n", node_id->node_id);
		return false;
	}

	if (locked == false) {
		spin_lock_irqsave(&du_ovl_lock, flags);
	}

	du_ovl_sync[sync_ch].command_mode = command_mode;
	rsc_node = &du_ovl_sync[sync_ch].rsc_node[du_src_ch];
	hw_release_t = rsc_node->hw_release_t;
	sw_prepare_t = rsc_node->sw_prepare_t;
	sw_prepare_t_next = (sw_prepare_t + 1) % DU_OVL_MAX_PIPE_DEPTH;
	if (sw_prepare_t_next == hw_release_t) {
		pr_err("too many work is reserved, hw:%u, sp:%u of resource id(%d)\n", hw_release_t, sw_prepare_t, du_src_ch);
		if (locked == false) {
			spin_unlock_irqrestore(&du_ovl_lock, flags);
		}
		return false;
	}

	if (node->undo)
		rsc_node->debug.undoing = true;
	else
		rsc_node->debug.undoing = false;

	hw_prepare_t = rsc_node->hw_prepare_t;
	hw_prepare_t_next = (hw_prepare_t + 1) % DU_OVL_MAX_PIPE_DEPTH;

	if ((hw_prepare_t_next != sw_prepare_t) && (hw_prepare_t != sw_prepare_t)) {
#ifdef DEBUG
		pr_err("reserved work exist, hp:%u, sp:%u of resource id(%d)\n", hw_prepare_t, sw_prepare_t, du_src_ch);
#endif
	}

	pipe_sw_prepare_t = &rsc_node->pipe[sw_prepare_t];
	pipe_sw_prepare_t->node_id = *node_id;
	pipe_sw_prepare_t->node = *node;
	pipe_sw_prepare_t->disable = false;
	pipe_sw_prepare_t->blend = (node->output.image.port == DSS_DISPLAY_PORT_FB0 || node->output.image.port == DSS_DISPLAY_PORT_FB1) ? true : false;
	crop_area = pipe_sw_prepare_t->node.input.in_meta.crop_rect.size.w * pipe_sw_prepare_t->node.input.in_meta.crop_rect.size.h;
	dst_area = pipe_sw_prepare_t->node.input.in_meta.dst_rect.size.w * pipe_sw_prepare_t->node.input.in_meta.dst_rect.size.h;
	pipe_sw_prepare_t->scale_down = (crop_area > dst_area) ? true : false;
	if (pipe_sw_prepare_t->blend == true && pipe_sw_prepare_t->scale_down == true)
		pipe_sw_prepare_t->blend = false;
	else
		pipe_sw_prepare_t->scale_down = false;

	if (pipe_sw_prepare_t->node.undo && hw_prepare_t != sw_prepare_t) {
#ifdef DEBUG
		pr_err("[undo-info] rsc:%d sw_p:%d hw_p:%d\n", du_src_ch, sw_prepare_t, hw_prepare_t);
#endif
		if (hw_prepare_t < sw_prepare_t) {
			for (pipe_i = hw_prepare_t; pipe_i < sw_prepare_t; pipe_i++) {
				rsc_node->pipe[pipe_i].node.undo = true;
			}
		} else if (hw_prepare_t > sw_prepare_t) {
			for (pipe_i = hw_prepare_t; pipe_i < DU_OVL_MAX_PIPE_DEPTH; pipe_i++) {
				rsc_node->pipe[pipe_i].node.undo = true;
			}

			for (pipe_i = 0; pipe_i < sw_prepare_t; pipe_i++) {
				rsc_node->pipe[pipe_i].node.undo = true;
			}
		}
	}

	rsc_node->sw_prepare_t = sw_prepare_t_next;

#ifdef DEBUG
	pr_err("[do_imgs] rsc:%d pipe:%u node_id:%u undo:%d in:0x%x out:0x%x\n", du_src_ch, rsc_node->sw_prepare_t, node_id->node_id, node->undo,
			node->input.image.phy_addr, node->output.image.phy_addr);
#endif

	if (locked == false) {
		spin_unlock_irqrestore(&du_ovl_lock, flags);
	}

	return true;
}

void du_ovl_init(void (*cb_used_done)(struct dss_node_id *node_id, bool rsc_free, struct dss_image *input_image, struct dss_image *output_image),
		void (*cb_ready_to_work)(struct dss_node_id *node_id, struct dss_node *node))
{
	enum sync_ch_id sync_i;
	enum du_src_ch_id du_src_i;
	unsigned int pipe_i;

	spin_lock_init(&du_ovl_lock);

	_du_ovl_db.cb_used_done = cb_used_done;
	_du_ovl_db.cb_ready_to_work = cb_ready_to_work;

	for (sync_i = SYNC_CH_ID0; sync_i < SYNC_CH_NUM; sync_i++) {
		sync_isr_register(sync_i, NULL, _du_ovl_work_sync);
		for (du_src_i = 0; du_src_i < DU_SRC_NUM; du_src_i++) {
			du_ovl_sync[sync_i].rsc_node[du_src_i].sw_prepare_t = 0;
			du_ovl_sync[sync_i].rsc_node[du_src_i].hw_prepare_t = 0;
			du_ovl_sync[sync_i].rsc_node[du_src_i].hw_working_t = 0;
			du_ovl_sync[sync_i].rsc_node[du_src_i].hw_release_t = 0;
			du_ovl_sync[sync_i].rsc_node[du_src_i].debug.undoing = false;
			for (pipe_i = 0; pipe_i < DU_OVL_MAX_PIPE_DEPTH; pipe_i++) {
				du_ovl_sync[sync_i].rsc_node[du_src_i].pipe[pipe_i].node_id.instance_id = NULL;
				du_ovl_sync[sync_i].rsc_node[du_src_i].pipe[pipe_i].disable = false;
				du_ovl_sync[sync_i].rsc_node[du_src_i].pipe[pipe_i].scale_down = false;
				du_ovl_sync[sync_i].rsc_node[du_src_i].pipe[pipe_i].blend = false;
				memset(&du_ovl_sync[sync_i].rsc_node[du_src_i].pipe[pipe_i].node, 0, sizeof(struct dss_node));
				memset(&du_ovl_sync[sync_i].rsc_node[du_src_i].pipe[pipe_i].node_id, 0, sizeof(struct dss_node_id));
			}
		}
	}
}

void du_ovl_cleanup(void)
{
	_du_ovl_db.cb_used_done = NULL;
	_du_ovl_db.cb_ready_to_work = NULL;
}
