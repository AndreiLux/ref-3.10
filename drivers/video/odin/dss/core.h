/*
 *  drivers/video/odin/dss/core.h
 *
 * Copyright (C) 2012 LG Electronics
 * Author: Seungwon Shin <m4seungwon.shin@lge.com>
 *
 * Some code and ideas taken from drivers/video/omap2/ driver
 * by Tomi Valkeinen.
 * Copyright (C) 2009 Texas Instruments
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ODIN_CORE_H__
#define __ODIN_CORE_H__


/* core */
struct bus_type *dss_get_bus(void);
int odin_dss_register_driver(struct odin_dss_driver *dssdriver);
void odin_dss_unregister_driver(struct odin_dss_driver *dssdriver);


/* display */
int dss_suspend_all_devices(void);
int dss_resume_all_devices(void);
void dss_disable_all_devices(void);

void dss_init_device(struct platform_device *pdev,
		struct odin_dss_device *dssdev);
void dss_uninit_device(struct platform_device *pdev,
		struct odin_dss_device *dssdev);
bool dss_use_replication(struct odin_dss_device *dssdev,
		enum odin_color_mode mode);
void default_get_overlay_fifo_thresholds(enum odin_dma_plane plane,
		u32 fifo_size, enum odin_burst_size *burst_size,
		u32 *fifo_low, u32 *fifo_high);


void odindss_default_get_resolution(struct odin_dss_device *dssdev,
			u16 *xres, u16 *yres);
int odindss_default_get_recommended_bpp(struct odin_dss_device *dssdev);

/* manager */
int dss_init_overlay_managers(struct platform_device *pdev);
void dss_uninit_overlay_managers(struct platform_device *pdev);
int dss_mgr_wait_for_go_ovl(struct odin_overlay *ovl);
void dss_setup_partial_planes(struct odin_dss_device *dssdev,
				u16 *x, u16 *y, u16 *w, u16 *h,
				bool enlarge_update_area);
void dss_start_update(struct odin_dss_device *dssdev);
int odin_dss_ovl_set_info(struct odin_overlay *ovl,
			  struct odin_overlay_info *info);

/* overlay */
void dss_init_overlays(struct platform_device *pdev);
void dss_uninit_overlays(struct platform_device *pdev);
int dss_check_overlay(struct odin_overlay *ovl, struct odin_dss_device *dssdev);
void dss_overlay_setup_dispc_manager(struct odin_overlay_manager *mgr);
void dss_recheck_connections(struct odin_dss_device *dssdev, bool force);


/* DU */
int 	odin_du_init_platform_driver(void);
void 	odin_du_uninit_platform_driver(void);


/* XD_Engine */
int 	odin_xdengine_init_platform_driver(void);
void 	odin_xdengine_uninit_platform_driver(void);


/* DSI */
int 	odin_mipi_dsi_init_platform_driver(void);
void 	odin_mipi_dsi_uninit_platform_driver(void);


/* DIPC */
int 	odin_dipc_display_init(struct odin_dss_device *dss_dev);
int	odin_dipc_enable(struct odin_dip_channel ch);
int	odin_dipc_disable(struct odin_dip_channel ch);
int 	odin_dipc_init_platform_driver(void);
void 	odin_dipc_uninit_platform_driver(void);

/* S3D */
int 	odin_2dto3d_init_platform_driver(void);
void 	odin_2dto3d_uninit_platform_driver(void);

int 	odin_formatter_init_platform_driver(void);
void 	odin_formatter_uninit_platform_driver(void);

/* HDMI */
int 	odin_hdmi_init_platform_driver(void);
void 	odin_hdmi_uninit_platform_driver(void);

int 	odin_hdmi_display_check_timing(struct odin_dss_device *dssdev,
					struct odin_video_timings *timings);
int 	odin_hdmi_display_set_mode(struct odin_dss_device *dssdev,
					struct fb_videomode *vm);
void 	odin_hdmi_display_set_timing(struct odin_dss_device *dssdev);
int 	odin_hdmi_display_enable(struct odin_dss_device *dssdev);
void 	odin_hdmi_display_disable(struct odin_dss_device *dssdev);

