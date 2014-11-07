/*
 * linux/drivers/video/odin/hdmi/hdmi.h
 *
 * Copyright (C) 2012 LGE
 * Author:
 *
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

#ifndef __ODIN_HDMI_H__
#define __ODIN_HDMI_H__


#include "hdmi-regs.h"

struct hdmi_panel_config {
	const char *name;
	int type;

	struct odin_video_timings timings;

	struct {
		unsigned int sleep_in;
		unsigned int sleep_out;
		unsigned int hw_reset;
		unsigned int enable_te;
	} sleep;

	struct {
		unsigned int high;
		unsigned int low;
	} reset_sequence;
};


int 	odin_hdmi_init_platform_driver(void);
void 	odin_hdmi_uninit_platform_driver(void);
int 	hdmi_start(u8 fixed_color_screen);
int 	hdmi_edid_done(void);

#endif	/* __ODIN_HDMI_H__*/

