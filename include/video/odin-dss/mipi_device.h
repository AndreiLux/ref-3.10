/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * junglark.park <junglark.park@lgepartner.com>
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

#ifndef _MIPI_DEVICE_H_
#define _MIPI_DEVICE_H_

struct panel_info {
    char *name;
    unsigned int x_res;
    unsigned int y_res;
    unsigned int height;
    unsigned int width;
    unsigned int pixel_clock;
    unsigned int data_width;
    unsigned char bpp;
    unsigned int hsw;
    unsigned int hfp;
    unsigned int hbp;
    unsigned int vsw;
    unsigned int vfp;
    unsigned int vbp;
    bool pclk_pol; 
    bool vsync_pol;
    bool hsync_pol;
};

struct mipi_hal_interface {
    bool (*open)( struct mipi_dsi_desc * dsi_desc);
    bool (*send)( struct dsi_packet *pPkt);
    bool (*send_multi)( struct dsi_packet *pPkt, unsigned int cnt_pkt);
    bool (*start_video_mode)(void);
    bool (*stop_video_mode)(void);
    bool (*close)(void);
};

struct mipi_device_interface {
    bool (*init)( struct mipi_hal_interface *hal_ops);
    bool (*get_screen_info)(struct panel_info *info);
    bool (*blank)(bool blank);
    bool (*is_command_mode)(void);
};

// init  callback to use on DIPI_MIPI
bool mipi_device_ops_register(enum dsi_module dsi_idx, struct mipi_device_interface *mipi_ops);

#endif //#ifdef _MIPI_DEVICE_H_
