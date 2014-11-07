/*
 * linux/drivers/video/odin/dss/dss_features.c
 *
 * Copyright (C) 2010 Texas Instruments
 * Author: Archit Taneja <archit@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/slab.h>

#include <video/odindss.h>

#include "du.h"
#include "dss-features.h"

/* Defines a generic odin register field */
struct dss_reg_field {
	u8 start, end;
};

struct dss_param_range {
	int min, max;
};

struct odin_dss_features {
	const int num_mgrs;
	const int num_ovls;
	const int num_wbs;
	const enum odin_display_type *supported_displays;
	const enum odin_color_mode *supported_color_modes;
	const enum odin_overlay_caps *overlay_caps;
	const struct dss_param_range *dss_params;

	const u32 buffer_size_unit;
	const u32 burst_size_unit;
};

/* This struct is assigned to one of the below during initialization */
static const struct odin_dss_features *odin_current_dss_features;


static const enum odin_display_type odin_dss_supported_displays[] = {
	/* ODIN_DSS_CHANNEL_LCD0*/
	ODIN_DISPLAY_TYPE_DSI | ODIN_DISPLAY_TYPE_RGB |
	ODIN_DISPLAY_TYPE_MEM2MEM,

	/* ODIN_DSS_CHANNEL_LCD1*/
	ODIN_DISPLAY_TYPE_DSI | ODIN_DISPLAY_TYPE_MEM2MEM,

	/* ODIN_DSS_CHANNEL_DIGIT */
	ODIN_DISPLAY_TYPE_HDMI | ODIN_DISPLAY_TYPE_MEM2MEM,
};


static const enum odin_color_mode odin_dss_supported_color_modes[] = {
	/* ODIN_DSS_GRA0 */
	ODIN_DSS_COLOR_RGB_ALL,

	/* ODIN_DSS_GRA1 */
	ODIN_DSS_COLOR_RGB_ALL,

	/* ODIN_DSS_GRA2 */
	ODIN_DSS_COLOR_RGB_ALL,

	/* ODIN_DSS_VIDEO0 */
	ODIN_DSS_COLOR_RGB_ALL | ODIN_DSS_COLOR_YUV_ALL,

	/* ODIN_DSS_VIDEO1 */
	ODIN_DSS_COLOR_RGB_ALL | ODIN_DSS_COLOR_YUV_ALL,

	/* ODIN_DSS_VIDEO2 */
	ODIN_DSS_COLOR_RGB_ALL | ODIN_DSS_COLOR_YUV_ALL,

	/* ODIN_DSS_FORMATTER */
	ODIN_DSS_COLOR_YUV_ALL,

	/* ODIN_DSS_mSCALER */
	ODIN_DSS_COLOR_RGB_ALL | ODIN_DSS_COLOR_YUV_ALL,
};

static const enum odin_overlay_caps odin_dss_overlay_caps[] = {
	/* ODIN_DSS_GRA0 */
	ODIN_DSS_OVL_CAP_NONE,

	/* ODIN_DSS_GRA1 */
	ODIN_DSS_OVL_CAP_NONE,

	/* ODIN_DSS_GRA2 */
	ODIN_DSS_OVL_CAP_NONE,

	/* ODIN_DSS_VIDEO0_1 */
	ODIN_DSS_OVL_CAP_SCALE,

	/* ODIN_DSS_VIDEO1_2 */
	ODIN_DSS_OVL_CAP_SCALE,

	/* ODIN_DSS_mXD_FORMATTER */
	ODIN_DSS_OVL_CAP_XDENGINE | ODIN_DSS_OVL_CAP_SCALE |
	ODIN_DSS_OVL_CAP_S3D,

	/* ODIN_DSS_mSCALER */
	ODIN_DSS_OVL_CAP_SCALE
};


/* For all the other ODIN versions */
/* ODIN1 */
static const struct odin_dss_features odin_dss_features = {
	.num_mgrs = 3,
	.num_ovls = 7,
	.num_wbs = 3,
	.supported_displays = odin_dss_supported_displays,
	.supported_color_modes = odin_dss_supported_color_modes,
	.overlay_caps = odin_dss_overlay_caps,
};

/* Functions returning values related to a DSS feature */
int dss_feat_get_num_mgrs(void)
{
	return odin_current_dss_features->num_mgrs;
}

int dss_feat_get_num_ovls(void)
{
	return odin_current_dss_features->num_ovls;
}

int dss_feat_get_num_wbs(void)
{
	return odin_current_dss_features->num_wbs;
}

enum odin_display_type dss_feat_get_supported_displays
	(enum odin_channel channel)
{
	return odin_current_dss_features->supported_displays[channel];
}

enum odin_color_mode dss_feat_get_supported_color_modes
	(enum odin_dma_plane plane)
{
	return odin_current_dss_features->supported_color_modes[plane];
}

enum odin_overlay_caps dss_feat_get_overlay_caps(enum odin_dma_plane plane)
{
	return odin_current_dss_features->overlay_caps[plane];
}

bool dss_feat_color_mode_supported(enum odin_dma_plane plane,
		enum odin_color_mode color_mode)
{
	return odin_current_dss_features->supported_color_modes[plane] &
			color_mode;
}

void dss_features_init(void)
{
		odin_current_dss_features = &odin_dss_features;
}
