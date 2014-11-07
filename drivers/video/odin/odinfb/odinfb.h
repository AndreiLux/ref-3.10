/*
 * linux/drivers/video/odin/odinfb.h
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

#ifndef __DRIVERS_VIDEO_ODIN_ODINFB_H__
#define __DRIVERS_VIDEO_ODIN_ODINFB_H__

#ifdef CONFIG_ODIN_FB_DEBUG_SUPPORT
#define DEBUG
#endif

#include <video/odindss.h>
#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <linux/ion.h>

extern unsigned int odinfb_debug;
/*#define ODINFB_FENCE_SYNC*/	/* fence sync in dsscomp node */
#ifdef ODINFB_FENCE_SYNC
#define ODINFB_FENCE_SYNC_LOG	0	/* fence sync log */
#endif

#ifdef DEBUG
#define FB_DBG(format, ...) \
	if (odinfb_debug) \
		printk(KERN_DEBUG "ODINFB: " format, ## __VA_ARGS__)
#else
#define FB_DBG(format, ...)
#endif

#define FB_ERR(format, ...) \
	printk(KERN_ERR "ODINFB error: " format, ## __VA_ARGS__)
#define FB_WARN(format, ...) \
	printk(KERN_WARNING "ODINFB warn: " format, ## __VA_ARGS__)
#define FB_INFO(format, ...) \
	printk(KERN_INFO "ODINFB: " format, ## __VA_ARGS__)
#define FB2OFB(fb_info) ((struct odinfb_info *)(fb_info->par))

/* max number of overlays to which a framebuffer data can be direct */
#define ODINFB_MAX_OVL_PER_FB 3
#define ODINFB_BUFFER_NUMBER 2

struct odinfb_device {
	struct device *dev;
	struct mutex  mtx;

	u32 pseudo_palette[17];

	int state;

	unsigned num_fbs;
	struct fb_info *fbs[5];
	struct odinfb_mem_region regions[10];

	unsigned num_displays;				/* number of display device  (3) */
	struct odin_dss_device *displays[3];

	unsigned num_overlays;				/* overlay layer (5) */
	struct odin_overlay *overlays[10];
	unsigned num_managers;
	struct odin_overlay_manager *managers[10];

	unsigned num_bpp_overrides;
	struct {
		struct odin_dss_device *dssdev;
		u8 bpp;
	} bpp_overrides[10];

	bool vsync_active;
	ktime_t vsync_timestamp;
	struct work_struct vsync_work;

	struct ion_allocation_data 	alloc_data[ODINFB_BUFFER_NUMBER];
	struct ion_fd_data 		fd_data[ODINFB_BUFFER_NUMBER];
	struct completion vsync_comp;
#ifdef ODINFB_FENCE_SYNC
	/* buf sync */
	/*int op_enable;*/
	/*int panel_power_on;*/
	struct mutex sync_mutex;
	struct sw_sync_timeline *timeline;
	int timeline_value;

	u32 acq_fen_cnt;
	struct sync_fence *acq_fen[10];/*[ODINFB_MAX_FENCE_FD];*/

	int cur_rel_fen_fd;
	struct sync_fence *cur_rel_fence;
	struct sync_pt *cur_rel_sync_pt;

	struct sync_fence *last_rel_fence;

	/* for non-blocking */
	u32 is_committing;
	struct completion commit_comp;
	struct work_struct commit_work;
	void *odinfb_backup;
#endif
};

#ifdef ODINFB_FENCE_SYNC
struct odinfb_backup_type {
	/*struct fb_info info;*/
	struct odinfb_display_commit disp_commit;
};
#endif
/* appended to fb_info */
struct odinfb_info {
	int id;
	struct odinfb_mem_region *region;
	int num_overlays;
	struct odin_overlay *overlays[ODINFB_MAX_OVL_PER_FB];
	struct odinfb_device *fbdev;
	u8 rotation[ODINFB_MAX_OVL_PER_FB];
	bool mirror;
};

struct odinfb_colormode {
	enum odin_color_mode dssmode;
	u32 bits_per_pixel;
	u32 nonstd;
	struct fb_bitfield red;
	struct fb_bitfield green;
	struct fb_bitfield blue;
	struct fb_bitfield transp;
};

void set_fb_fix(struct fb_info *fbi);
int check_fb_var(struct fb_info *fbi, struct fb_var_screeninfo *var);
int odinfb_realloc_fbmem(struct fb_info *fbi, unsigned long size, int type);
int odinfb_apply_changes(struct fb_info *fbi, int init);

int odinfb_create_sysfs(struct odinfb_device *fbdev);
void odinfb_remove_sysfs(struct odinfb_device *fbdev);

int odinfb_ioctl(struct fb_info *fbi, unsigned int cmd, unsigned long arg);

int odinfb_update_window(struct fb_info *fbi,
		u32 x, u32 y, u32 w, u32 h);

int dss_mode_to_fb_mode(enum odin_color_mode dssmode,
			struct fb_var_screeninfo *var);

int odinfb_setup_overlay(struct fb_info *fbi, struct odin_overlay *ovl,
		u16 crop_x, u16 crop_y, u16 crop_w, u16 crop_h,
		u16 win_x, u16 win_y, u16 win_w, u16 win_h, bool info_enabled);


int odinfb_enable_vsync(struct odinfb_device *fbdev);
void odinfb_disable_vsync(struct odinfb_device *fbdev);
bool odinfb_ovl_iova_check(struct ion_handle_data handle_data);

int odinfb_set_update_mode(struct fb_info *fbi,
				   enum odinfb_update_mode mode);
int odinfb_get_update_mode(struct fb_info *fbi,
		enum odinfb_update_mode *mode);
#ifdef ODINFB_FENCE_SYNC
int odinfb_wait_for_fence(struct odinfb_device *fbdev);
int odinfb_signal_timeline(struct odinfb_device *fbdev);
#endif

/* find the display connected to this fb, if any */
static inline struct odin_dss_device *fb2display(struct fb_info *fbi)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	int i;

	/* XXX: returns the display connected to first attached overlay */
	for (i = 0; i < ofbi->num_overlays; i++) {
		if (ofbi->overlays[i]->manager)
		{
			return ofbi->overlays[i]->manager->device;
		}
	}

	return NULL;
}

static inline void odinfb_lock(struct odinfb_device *fbdev)
{
	mutex_lock(&fbdev->mtx);
}

static inline void odinfb_unlock(struct odinfb_device *fbdev)
{
	mutex_unlock(&fbdev->mtx);
}

static inline int odinfb_overlay_enable(struct odin_overlay *ovl,
		int enable)
{
	struct odin_overlay_info info;

	ovl->get_overlay_info(ovl, &info);
	info.enabled = enable;
	return ovl->set_overlay_info(ovl, &info);
}

static inline struct odinfb_mem_region *
odinfb_get_mem_region(struct odinfb_mem_region *rg)
{
	down_read_nested(&rg->lock, rg->id);
	atomic_inc(&rg->lock_count);
	return rg;
}

static inline void odinfb_put_mem_region(struct odinfb_mem_region *rg)
{
	atomic_dec(&rg->lock_count);
	up_read(&rg->lock);
}

#endif	/* __DRIVERS_VIDEO_ODIN_ODINFB_H__ */

