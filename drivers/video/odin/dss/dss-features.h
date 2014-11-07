/*
 * linux/drivers/video/odin/dss/dss-features.h
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

#ifndef __ODIN_DSS_FEATURES_H
#define __ODIN_DSS_FEATURES_H

#define MAX_DSS_MANAGERS	3
#define MAX_DSS_OVERLAYS	7
#define MAX_DSS_WRITEBACKS	3

#define MAX_DSS_LCD_MANAGERS	2
#define MAX_NUM_DSI		2

/* DSS Feature Functions */
int dss_feat_get_num_mgrs(void);
int dss_feat_get_num_ovls(void);
int dss_feat_get_num_wbs(void);
enum odin_display_type dss_feat_get_supported_displays
	(enum odin_channel channel);
enum odin_color_mode dss_feat_get_supported_color_modes
	(enum odin_dma_plane plane);
enum odin_overlay_caps dss_feat_get_overlay_caps(enum odin_dma_plane plane);
bool dss_feat_color_mode_supported(enum odin_dma_plane plane,
		enum odin_color_mode color_mode);
void dss_features_init(void);
#endif
