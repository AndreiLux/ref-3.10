/*
 * linux/drivers/video/odin/dss/kcalctrl.h
 *
 * Copyright (C) 2012 LG Electronics, Inc.
 * Author:
 *
 * Some code and ideas taken from drivers/video/odin/ driver
 * by Imre Deak.
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

#ifndef __DRIVERS_VIDEO_ODIN_DSS_KCAL_CTRL_H__
#define __DRIVERS_VIDEO_ODIN_DSS_KCAL_CTRL_H__

#define DEBUG


/*#define KCAL_CTRL_FENCE_SYNC*/	/* fence sync in dsscomp node */
#ifdef KCAL_CTRL_FENCE_SYNC
#define KCAL_CTRL_FENCE_SYNC_LOG	0	/* fence sync log */
#endif

#define KCALERR(format, ...) \
	printk(KERN_ERR "############## KCAL_CTRL error ############ " format, ## __VA_ARGS__)
#define KCALWARN(format, ...) \
	printk(KERN_WARNING "############## KCAL_CTRL warn ############## " format, ## __VAR_ARGS__)
#define KCALINFO(format, ...) \
	printk(KERN_INFO "############## KCAL_CTRL info ############ " format, ## __VA_ARGS__)


struct kcal_platform_data { 
	int (*set_values) (int r, int g, int b);
	int (*get_values) (int *r, int *g, int *b);
	int (*refresh_display) (void);
};


extern int kcal_ctrl_create_file(struct platform_device *pdev);

extern int g_kcal_r;
extern int g_kcal_g;
extern int g_kcal_b;
extern int g_kcal_ctrl_mode;

#endif // __DRIVERS_VIDEO_ODIN_DSS_KCAL_CTRL_H__
