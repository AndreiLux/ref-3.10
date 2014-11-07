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

#ifndef _DSS_IO_H_
#define _DSS_IO_H_

#include "dss_types.h"

#define DSS_IOCTL_MAGIC			        'S'

#define DSS_IO_OPEN                     _IO(DSS_IOCTL_MAGIC, 0)
#define DSS_IO_MGR_TRY_ASSIGN           _IO(DSS_IOCTL_MAGIC, 1)

#define DSS_NUM_OVERLAY_CH			    7

struct dss_io_image
{
	struct dss_image_size image_size;

	enum dss_display_port port;
	/* if (port == DSS_DISPLAY_PORT_MEM) */
//	struct ion_handle *ion;
	int ion_fd;
	enum dss_image_format pixel_format;
};

struct dss_io_overlay_ch
{
	struct dss_io_image input;
	struct dss_image_rect crop_rect;
	struct dss_image_rect dst_rect;

	enum dss_image_op_rotate rotate_op;
	enum dss_image_op_flip flip_op;
	dss_bool mxd_enable;
	unsigned char bpp;
	unsigned char global_alpha;
	dss_bool premult_alpha;
	unsigned int zorder;
	dss_bool chrom_enable;
	unsigned int chrom_data;
	unsigned int chrom_mask;
	enum dss_image_assign_res assign_req;
	/* Out */
	enum dss_image_assign_res assign_res;
};

struct dss_io_mgr_try_assign
{
	/* In */
	struct dss_io_image output;
	struct dss_io_overlay_ch overlay_ch[DSS_NUM_OVERLAY_CH];

	struct
	{
		unsigned int num_overlay_ch;
		enum dss_image_op_blend blend_ops[DSS_NUM_OVERLAY_CH - 1];
		unsigned int pattern_data;
	} overlay_mixer;
};

struct dss_io_open
{
	void *vevent_id; /* vevent_id for buf done */
};

#endif /* #ifndef _DSS_IO_H_ */
