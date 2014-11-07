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

#ifndef __ODIN_HDMI_DEBUG_H__
#define __ODIN_HDMI_DEBUG_H__


#define DEBUG

#ifdef DEBUG
enum LOG_LEVEL{
	LOG_OFF=0,
	LOG_INFO,
	LOG_DEBUG,
	LOG_ALL
};

extern int g_hdmi_debug;

#define hdmi_debug(level, fmt, args...)              \
	do {                            \
		if (g_hdmi_debug >= level)             \
		printk("%s:%d: " fmt,    \
				__func__, __LINE__, ##args);    \
	} while (0)
#else
#define hdmi_debug(level, fmt, args...)
#endif

#define hdmi_api_enter() hdmi_debug(LOG_ALL, "enter\n")
#define hdmi_api_leave() hdmi_debug(LOG_ALL, "leave\n")

#define hdmi_err(fmt, args...)               \
	do {                        \
		printk(KERN_ERR "%s:%d: " fmt,      \
				__func__, __LINE__, ##args); \
	} while (0)

#endif	/* __ODIN_HDMI_DEBUG_H__*/


