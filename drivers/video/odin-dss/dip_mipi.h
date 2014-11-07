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

#ifndef _DIP_MIPI_H_
#define _DIP_MIPI_H_

#include <linux/types.h>
#include <linux/platform_device.h>
#include <video/odin-dss/dss_types.h>

#define dss_dip_mipi_verify_dip_data_path(dip_data_path)	((dip_data_path) < DIP_NUM ? true : false)

/// The DIP  have 3 port.
enum dip_data_path
{
	DIP0_LCD,
	DIP1_LCD,
	DIP_HDMI,
	DIP_NUM,
};

struct dssfb_dev_entry
{
	char *name;
	unsigned int gpio;

	struct {
		unsigned int xres;
		unsigned int yres;

		unsigned int height;
		unsigned int width;

		unsigned int bpp;

		unsigned int pixel_clock;
		unsigned int left_margin;
		unsigned int right_margin;
		unsigned int upper_margin;
		unsigned int lower_margin;
		unsigned int hsync_len;
		unsigned int vsync_len;

		enum {
			DSSFB_HSYNC_POSSITIVE,
			DSSFB_HSYNC_NEGATIVE,
		} hsync_polarity;
		enum {
			DSSFB_VSYNC_POSSITIVE,
			DSSFB_VSYNC_NEGATIVE,
		} vsync_polarity;
	} panel;

	bool (*blank)(bool blank);
	bool (*is_command_mode)(void);
};

bool dss_dip_mipi_init(struct platform_device *pdev_dip[], bool (*cb_dssfb_register)(enum dip_data_path dip, struct dssfb_dev_entry *fbdev), void (*cb_dssfb_unregister)(enum dip_data_path dip));
bool dss_dip_mipi_cleanup(void);

#endif /* #ifndef _DIP_MIPI_H_ */
