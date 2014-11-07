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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 */

#ifndef _DSS_COMP_TYPES_H_
#define _DSS_COMP_TYPES_H_

#include <linux/types.h>
#include <linux/list.h>
#include <video/odin-dss/dss_types.h>
#include "hal/sync_hal.h"
#include "hal/ovl_hal.h"

#define	DSS_MAX_PIPE_DEPTH					3

struct dss_image
{
	struct dss_image_size image_size;
	enum dss_display_port port;
	/* if (port == DSS_DISPLAY_PORT_MEM) */
	enum dss_image_format pixel_format;
	unsigned long phy_addr;
	bool ext_buf;
};

struct dss_input_meta
{
	struct dss_image_rect crop_rect;// input
	struct dss_image_rect dst_rect;	// output

	bool mxd_op;
	enum dss_image_op_blend blend_op;
	enum dss_image_op_rotate rotate_op;
	enum dss_image_op_flip flip_op;

	unsigned char bpp;
	unsigned int zorder;
	unsigned char global_alpha;
	bool premult_alpha;
	unsigned char chrom_enable;
	unsigned int chrom_data;
	unsigned int chrom_mask;
};

struct dss_output_meta
{
	enum ovl_ch_id ovl_ch;
	unsigned int pattern_data;
};

struct dss_node_id
{
	void *instance_id;
	void *layers_id;
	unsigned int layer_id;
	unsigned int node_id;
};

struct dss_node_input
{
	struct dss_image image;
	struct dss_input_meta in_meta;
};

struct dss_node_output
{
	struct dss_image image;
	struct dss_output_meta out_meta;
};

struct dss_node
{
	bool undo;
	struct dss_node_input input;
	struct dss_node_output output;
	unsigned long int_out_buf[DSS_MAX_PIPE_DEPTH-1];	// node_id th
};

#endif /* #ifndef _DSS_COMP_TYPES_H_ */

