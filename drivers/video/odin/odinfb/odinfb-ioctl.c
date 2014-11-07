/*
 * linux/drivers/video/odin/odinfb-ioctl.c
 *
 * Copyright (C) 2008 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
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

#include <linux/fb.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/odinfb.h>
#include <linux/vmalloc.h>
#include <linux/sync.h>
#include <linux/sw_sync.h>
#include <linux/file.h>

#include <video/odindss.h>

#include "odinfb.h"
#include "../dss/dss.h"
#include "../dss/du.h"

#ifdef ODINFB_FENCE_SYNC
#define WAIT_FENCE_TIMEOUT 100
#define WAIT_FENCE_FIRST_TIMEOUT MSEC_PER_SEC
#define WAIT_FENCE_FINAL_TIMEOUT (10 * MSEC_PER_SEC)
/* Display op timeout should be greater than total timeout */
#define WAIT_DISP_OP_TIMEOUT ((WAIT_FENCE_FIRST_TIMEOUT + \
		WAIT_FENCE_FINAL_TIMEOUT) * ODINFB_MAX_FENCE_FD)
#endif
static u8 get_mem_idx(struct odinfb_info *ofbi)
{
	if (ofbi->id == ofbi->region->id)
		return 0;

	return ODINFB_MEM_IDX_ENABLED | ofbi->region->id;
}

static struct odinfb_mem_region *get_mem_region(struct odinfb_info *ofbi,
						 u8 mem_idx)
{
	struct odinfb_device *fbdev = ofbi->fbdev;

	if (mem_idx & ODINFB_MEM_IDX_ENABLED)
		mem_idx &= ODINFB_MEM_IDX_MASK;
	else
		mem_idx = ofbi->id;

	if (mem_idx >= fbdev->num_fbs)
		return NULL;

	return &fbdev->regions[mem_idx];
}

static int odinfb_setup_plane(struct fb_info *fbi,
				struct odinfb_plane_info *pi)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_device *fbdev = ofbi->fbdev;
	struct odin_overlay *ovl;
	struct odin_overlay_info old_info;
	struct odinfb_mem_region *old_rg, *new_rg;
	int r = 0;

	FB_DBG("odinfb_setup_plane\n");

	if (ofbi->num_overlays != 1) {
		r = -EINVAL;
		goto out;
	}

	/* XXX uses only the first overlay */
	ovl = ofbi->overlays[0];

	old_rg = ofbi->region;
	new_rg = get_mem_region(ofbi, pi->mem_idx);
	if (!new_rg) {
		r = -EINVAL;
		goto out;
	}


#if 0
	/* Take the locks in a specific order to keep lockdep happy */
	if (old_rg->id < new_rg->id) {
		odinfb_get_mem_region(old_rg);
		odinfb_get_mem_region(new_rg);
	} else if (new_rg->id < old_rg->id) {
		odinfb_get_mem_region(new_rg);
		odinfb_get_mem_region(old_rg);
	} else
		odinfb_get_mem_region(old_rg);
#endif
	if (pi->enabled && !new_rg->size) {
		/*
		 * This plane's memory was freed, can't enable it
		 * until it's reallocated.
		 */
		r = -EINVAL;
		goto put_mem;
	}

	ovl->get_overlay_info(ovl, &old_info);

	if (old_rg != new_rg) {
		ofbi->region = new_rg;
		set_fb_fix(fbi);
	}

	if (pi->enabled) {
		struct odin_overlay_info info;

		r = odinfb_setup_overlay(fbi, ovl, pi->crop_x, pi->crop_y,
			pi->crop_w, pi->crop_h, pi->win_x, pi->win_y, pi->win_w, pi->win_h, pi->enabled);
		if (r)
			goto undo;

		ovl->get_overlay_info(ovl, &info);

		if (!info.enabled) {
			info.enabled = pi->enabled;
			r = ovl->set_overlay_info(ovl, &info);
			if (r)
				goto undo;
		}
	} else {
		struct odin_overlay_info info;

		ovl->get_overlay_info(ovl, &info);

		info.enabled = pi->enabled;

		info.crop_x = pi->crop_x;
		info.crop_y = pi->crop_y;
		info.crop_w = pi->crop_w;
		info.crop_h = pi->crop_h;

		info.out_x = pi->win_x;
		info.out_y = pi->win_y;
		info.out_w = pi->win_w;
		info.out_h = pi->win_h;

		r = ovl->set_overlay_info(ovl, &info);
		if (r)
			goto undo;
	}

	if (ovl->manager)
		ovl->manager->apply(ovl->manager, DSSCOMP_SETUP_MODE_DISPLAY);


#if 0
	/* Release the locks in a specific order to keep lockdep happy */
	if (old_rg->id > new_rg->id) {
		odinfb_put_mem_region(old_rg);
		odinfb_put_mem_region(new_rg);
	} else if (new_rg->id > old_rg->id) {
		odinfb_put_mem_region(new_rg);
		odinfb_put_mem_region(old_rg);
	} else
		odinfb_put_mem_region(old_rg);
#endif

	return 0;

 undo:
	if (old_rg != new_rg) {
		ofbi->region = old_rg;
		set_fb_fix(fbi);
	}

	ovl->set_overlay_info(ovl, &old_info);
 put_mem:

#if 0
	/* Release the locks in a specific order to keep lockdep happy */
	if (old_rg->id > new_rg->id) {
		odinfb_put_mem_region(old_rg);
		odinfb_put_mem_region(new_rg);
	} else if (new_rg->id > old_rg->id) {
		odinfb_put_mem_region(new_rg);
		odinfb_put_mem_region(old_rg);
	} else
		odinfb_put_mem_region(old_rg);
#endif
 out:
	dev_err(fbdev->dev, "setup_plane failed\n");

	return r;
}

static int odinfb_query_plane(struct fb_info *fbi,
				struct odinfb_plane_info *pi)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);

	if (ofbi->num_overlays != 1) {
		memset(pi, 0, sizeof(*pi));
	} else {
		struct odin_overlay *ovl;
		struct odin_overlay_info *ovli;

		ovl = ofbi->overlays[0];
		ovli = &ovl->info;

		pi->crop_x = ovli->crop_x;
		pi->crop_y = ovli->crop_y;
		pi->crop_w = ovli->crop_w;
		pi->crop_h = ovli->crop_h;

		pi->win_x = ovli->out_x;
		pi->win_y = ovli->out_y;
		pi->win_w = ovli->out_w;
		pi->win_h = ovli->out_h;

		pi->enabled = ovli->enabled;
		pi->channel_out = 0; /* xxx */
		pi->mirror = 0;
		pi->mem_idx = get_mem_idx(ofbi);
	}

	return 0;
}

static int odinfb_setup_mem(struct fb_info *fbi, struct odinfb_mem_info *mi)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_device *fbdev = ofbi->fbdev;
	struct odinfb_mem_region *rg;
	int r = 0, i;
	size_t size;

	if (mi->type > ODINFB_MEMTYPE_MAX)
		return -EINVAL;

	size = PAGE_ALIGN(mi->size);

	rg = ofbi->region;

	down_write_nested(&rg->lock, rg->id);
	atomic_inc(&rg->lock_count);

	if (atomic_read(&rg->map_count)) {
		r = -EBUSY;
		goto out;
	}

	for (i = 0; i < fbdev->num_fbs; i++) {
		struct odinfb_info *ofbi2 = FB2OFB(fbdev->fbs[i]);
		int j;

		if (ofbi2->region != rg)
			continue;

		for (j = 0; j < ofbi2->num_overlays; j++) {
			if (ofbi2->overlays[j]->info.enabled) {
				r = -EBUSY;
				goto out;
			}
		}
	}

	if (rg->size != size || rg->type != mi->type) {
		r = odinfb_realloc_fbmem(fbi, size, mi->type);
		if (r) {
			dev_err(fbdev->dev, "realloc fbmem failed\n");
			goto out;
		}
	}

 out:
	atomic_dec(&rg->lock_count);
	up_write(&rg->lock);

	return r;
}

static int odinfb_query_mem(struct fb_info *fbi, struct odinfb_mem_info *mi)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_mem_region *rg;

	rg = odinfb_get_mem_region(ofbi->region);
	memset(mi, 0, sizeof(*mi));

	mi->size = rg->size;
	mi->type = rg->type;

	odinfb_put_mem_region(rg);

	return 0;
}

static int odinfb_update_window_nolock(struct fb_info *fbi,
		u32 x, u32 y, u32 w, u32 h)
{
	struct odin_dss_device *display = fb2display(fbi);
	u16 dw, dh;

	if (!display)
		return 0;

	if (w == 0 || h == 0)
		return 0;

	display->driver->get_resolution(display, &dw, &dh);

	if (x + w > dw || y + h > dh)
		return -EINVAL;

	odin_du_set_channel_sync_enable(display->channel, 0);
	odin_du_set_channel_sync_enable(display->channel, 1);

	return display->driver->update(display, x, y, w, h);
}

/* This function is exported for SGX driver use */
int odinfb_update_window(struct fb_info *fbi,
		u32 x, u32 y, u32 w, u32 h)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_device *fbdev = ofbi->fbdev;
	int r;

	if (!lock_fb_info(fbi))
		return -ENODEV;
	odinfb_lock(fbdev);

	r = odinfb_update_window_nolock(fbi, x, y, w, h);

	odinfb_unlock(fbdev);
	unlock_fb_info(fbi);

	return r;
}
EXPORT_SYMBOL(odinfb_update_window);

int odinfb_set_update_mode(struct fb_info *fbi,
				   enum odinfb_update_mode mode)
{
	struct odin_dss_device *display = fb2display(fbi);
	enum odin_dss_update_mode um;
	int r;

	if (!display || !display->driver->set_update_mode)
		return -EINVAL;

	switch (mode) {
	case ODINFB_UPDATE_DISABLED:
		um = ODIN_DSS_UPDATE_DISABLED;
		break;

	case ODINFB_AUTO_UPDATE:
		um = ODIN_DSS_UPDATE_AUTO;
		break;

	case ODINFB_MANUAL_UPDATE:
		um = ODIN_DSS_UPDATE_MANUAL;
		break;

	default:
		return -EINVAL;
	}

	r = display->driver->set_update_mode(display, um);

	return r;
}

int odinfb_get_update_mode(struct fb_info *fbi,
		enum odinfb_update_mode *mode)
{
	struct odin_dss_device *display = fb2display(fbi);
	enum odin_dss_update_mode m;

	if (!display)
		return -EINVAL;

	if (!display->driver->get_update_mode) {
		*mode = ODINFB_AUTO_UPDATE;
		return 0;
	}

	m = display->driver->get_update_mode(display);

	switch (m) {
	case ODIN_DSS_UPDATE_DISABLED:
		*mode = ODINFB_UPDATE_DISABLED;
		break;
	case ODIN_DSS_UPDATE_AUTO:
		*mode = ODINFB_AUTO_UPDATE;
		break;
	case ODIN_DSS_UPDATE_MANUAL:
		*mode = ODINFB_MANUAL_UPDATE;
		break;
	default:
		BUG();
	}

	return 0;
}

/* XXX this color key handling is a hack... */
static struct odinfb_color_key odinfb_color_keys[2];

static int _odinfb_set_color_key(struct odin_overlay_manager *mgr,
		struct odinfb_color_key *ck)
{
	struct odin_overlay_manager_info info;
	enum odin_dss_trans_key_type kt;
	int r;

	mgr->get_manager_info(mgr, &info);

	if (ck->key_type == ODINFB_COLOR_KEY_DISABLED) {
		info.trans_enabled = false;
		odinfb_color_keys[mgr->id] = *ck;

		r = mgr->set_manager_info(mgr, &info);
		if (r)
			return r;

		r = mgr->apply(mgr, DSSCOMP_SETUP_MODE_DISPLAY);

		return r;
	}

	switch (ck->key_type) {
	case ODINFB_COLOR_KEY_GFX_DST:
		kt = ODIN_DSS_COLOR_KEY_GFX_DST;
		break;
	case ODINFB_COLOR_KEY_VID_SRC:
		kt = ODIN_DSS_COLOR_KEY_VID_SRC;
		break;
	default:
		return -EINVAL;
	}

	info.default_color = ck->background;
	info.trans_key = ck->trans_key;
	info.trans_key_type = kt;
	info.trans_enabled = true;

	odinfb_color_keys[mgr->id] = *ck;

	r = mgr->set_manager_info(mgr, &info);
	if (r)
		return r;

	r = mgr->apply(mgr, DSSCOMP_SETUP_MODE_DISPLAY);

	return r;
}

static int odinfb_set_color_key(struct fb_info *fbi,
		struct odinfb_color_key *ck)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_device *fbdev = ofbi->fbdev;
	int r;
	int i;
	struct odin_overlay_manager *mgr = NULL;

	odinfb_lock(fbdev);

	for (i = 0; i < ofbi->num_overlays; i++) {
		if (ofbi->overlays[i]->manager) {
			mgr = ofbi->overlays[i]->manager;
			break;
		}
	}

	if (!mgr) {
		r = -EINVAL;
		goto err;
	}

	r = _odinfb_set_color_key(mgr, ck);
err:
	odinfb_unlock(fbdev);

	return r;
}

static int odinfb_get_color_key(struct fb_info *fbi,
		struct odinfb_color_key *ck)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_device *fbdev = ofbi->fbdev;
	struct odin_overlay_manager *mgr = NULL;
	int r = 0;
	int i;

	odinfb_lock(fbdev);

	for (i = 0; i < ofbi->num_overlays; i++) {
		if (ofbi->overlays[i]->manager) {
			mgr = ofbi->overlays[i]->manager;
			break;
		}
	}

	if (!mgr) {
		r = -EINVAL;
		goto err;
	}

	*ck = odinfb_color_keys[mgr->id];
err:
	odinfb_unlock(fbdev);

	return r;
}

static int odinfb_memory_read(struct fb_info *fbi,
		struct odinfb_memory_read *mr)
{
	struct odin_dss_device *display = fb2display(fbi);
	void *buf;
	int r;

	if (!display || !display->driver->memory_read)
		return -ENOENT;

	if (!access_ok(VERIFY_WRITE, mr->buffer, mr->buffer_size))
		return -EFAULT;

	if (mr->w * mr->h * 3 > mr->buffer_size)
		return -EINVAL;

	buf = vmalloc(mr->buffer_size);
	if (!buf) {
		FB_ERR("vmalloc failed\n");
		return -ENOMEM;
	}

	r = display->driver->memory_read(display, buf, mr->buffer_size,
			mr->x, mr->y, mr->w, mr->h);

	if (r > 0) {
		if (copy_to_user(mr->buffer, buf, mr->buffer_size))
			r = -EFAULT;
	}

	vfree(buf);

	return r;
}

static int odinfb_get_ovl_colormode(struct odinfb_device *fbdev,
			     struct odinfb_ovl_colormode *mode)
{
	int ovl_idx = mode->overlay_idx;
	int mode_idx = mode->mode_idx;
	struct odin_overlay *ovl;
	enum odin_color_mode supported_modes;
	struct fb_var_screeninfo var;
	int i;

	if (ovl_idx >= fbdev->num_overlays)
		return -ENODEV;
	ovl = fbdev->overlays[ovl_idx];
	supported_modes = ovl->supported_modes;

	mode_idx = mode->mode_idx;

	for (i = 0; i < sizeof(supported_modes) * 8; i++) {
		if (!(supported_modes & (1 << i)))
			continue;
		/*
		 * It's possible that the FB doesn't support a mode
		 * that is supported by the overlay, so call the
		 * following here.
		 */
		if (dss_mode_to_fb_mode(1 << i, &var) < 0)
			continue;

		mode_idx--;
		if (mode_idx < 0)
			break;
	}

	if (i == sizeof(supported_modes) * 8)
		return -ENOENT;

	mode->bits_per_pixel = var.bits_per_pixel;
	mode->nonstd = var.nonstd;
	mode->red = var.red;
	mode->green = var.green;
	mode->blue = var.blue;
	mode->transp = var.transp;

	return 0;
}

#ifdef ODINFB_FENCE_SYNC
int odinfb_wait_for_fence(struct odinfb_device *fbdev)
{
	int i, ret = 0;
	/* buf sync */
	for (i = 0; i < fbdev->acq_fen_cnt; i++) {
		ret = sync_fence_wait(fbdev->acq_fen[i],
				WAIT_FENCE_FIRST_TIMEOUT);
		if (ret == -ETIME) {
			pr_warn("sync_fence_wait timed out! ");
			pr_cont("Waiting %ld more seconds\n",
					WAIT_FENCE_FINAL_TIMEOUT/MSEC_PER_SEC);
			ret = sync_fence_wait(fbdev->acq_fen[i],
					WAIT_FENCE_FINAL_TIMEOUT);
		}
		if (ret < 0) {
			pr_err("%s: sync_fence_wait failed! ret = %x\n",
				__func__, ret);
			break;
		}
		sync_fence_put(fbdev->acq_fen[i]);
	}

	if (ret < 0) {
		while (i < fbdev->acq_fen_cnt) {
			sync_fence_put(fbdev->acq_fen[i]);
			i++;
		}
	}

	fbdev->acq_fen_cnt = 0;
	return ret;
}

static void odinfb_signal_timeline_locked(struct odinfb_device *fbdev)
{
	if (fbdev->timeline && !list_empty((const struct list_head *)
				(&(fbdev->timeline->obj.active_list_head)))) {
		sw_sync_timeline_inc(fbdev->timeline, 1);
		fbdev->timeline_value++;
	}
	fbdev->last_rel_fence = fbdev->cur_rel_fence;
	fbdev->cur_rel_fence = 0;
}

int odinfb_signal_timeline(struct odinfb_device *fbdev)
{
	mutex_lock(&fbdev->sync_mutex);
	odinfb_signal_timeline_locked(fbdev);
	mutex_unlock(&fbdev->sync_mutex);
	return 0;
}

static int odinfb_handle_buf_sync_ioctl(struct odinfb_device *fbdev,
						struct odin_buf_sync *buf_sync)
{
	int i, fence_cnt = 0, ret = 0;
	int acq_fen_fd[ODINFB_MAX_FENCE_FD];
	struct sync_fence *fence;

#if ODINFB_FENCE_SYNC_LOG
	printk("cnt:%d, acq_fen_fd:0x%p, rel_fen_fd:0x%p, flags:%d\n",
		buf_sync->acq_fen_fd_cnt, buf_sync->acq_fen_fd,
			buf_sync->rel_fen_fd, buf_sync->flags);
#endif
	if ((buf_sync->acq_fen_fd_cnt > ODINFB_MAX_FENCE_FD) ||
		(fbdev->timeline == NULL))
		return -EINVAL;

/*	if ((!fbdev->op_enable) || (!fbdev->panel_power_on))
		return -EPERM;	*/

	/* get acq_fen_fd[n]s for layers from HWC */
	if (buf_sync->acq_fen_fd_cnt)
		ret = copy_from_user(acq_fen_fd, buf_sync->acq_fen_fd,
				buf_sync->acq_fen_fd_cnt * sizeof(int));
	if (ret) {
		pr_err("%s:copy_from_user failed", __func__);
		return ret;
	}

	mutex_lock(&fbdev->sync_mutex);
	for (i = 0; i < buf_sync->acq_fen_fd_cnt; i++) {
		fence = sync_fence_fdget(acq_fen_fd[i]);
		if (fence == NULL) {
			pr_info("%s: null fence! i=%d fd=%d\n", __func__, i,
				acq_fen_fd[i]);
			ret = -EINVAL;
			break;
		}
		fbdev->acq_fen[i] = fence;
	}

	fence_cnt = i;
	if (ret)
		goto buf_sync_err_1;

	fbdev->acq_fen_cnt = fence_cnt;

	/* block */
	if (buf_sync->flags & ODINFB_BUF_SYNC_FLAG_WAIT) {
		odinfb_wait_for_fence(fbdev);
	}

	/**** send release fd to HWC ****/
	/* if panel is writeback panel, threshold of timeline_Value should be 1 */
	fbdev->cur_rel_sync_pt = sw_sync_pt_create(fbdev->timeline,
			fbdev->timeline_value + 2);
	if (fbdev->cur_rel_sync_pt == NULL) {
		pr_err("%s: cannot create sync point", __func__);
		ret = -ENOMEM;
		goto buf_sync_err_1;
	}

	/* create fence */
	fbdev->cur_rel_fence = sync_fence_create("odinfb-fence",
			fbdev->cur_rel_sync_pt);
	if (fbdev->cur_rel_fence == NULL) {
		sync_pt_free(fbdev->cur_rel_sync_pt);
		fbdev->cur_rel_sync_pt = NULL;
		pr_err("%s: cannot create fence", __func__);
		ret = -ENOMEM;
		goto buf_sync_err_1;
	}

	/* create fd */
	fbdev->cur_rel_fen_fd = get_unused_fd_flags(0);
	sync_fence_install(fbdev->cur_rel_fence, fbdev->cur_rel_fen_fd);
	ret = copy_to_user(buf_sync->rel_fen_fd,
		&fbdev->cur_rel_fen_fd, sizeof(int));
	if (ret) {
		pr_err("%s:copy_to_user failed", __func__);
		goto buf_sync_err_2;
	}
	mutex_unlock(&fbdev->sync_mutex);

	return ret;

buf_sync_err_2:

	sync_fence_put(fbdev->cur_rel_fence);
	put_unused_fd(fbdev->cur_rel_fen_fd);
	fbdev->cur_rel_fence = NULL;
	fbdev->cur_rel_fen_fd = 0;

buf_sync_err_1:

	for (i = 0; i < fence_cnt; i++)
		sync_fence_put(fbdev->acq_fen[i]);
	fbdev->acq_fen_cnt = 0;
	mutex_unlock(&fbdev->sync_mutex);
	return ret;
}

static void odinfb_pan_idle(struct odinfb_device *fbdev)
{
	int ret;

	if (fbdev->is_committing) {
		ret = wait_for_completion_timeout(
				&fbdev->commit_comp,
			msecs_to_jiffies(WAIT_DISP_OP_TIMEOUT));
		if (ret < 0)
			ret = -ERESTARTSYS;
		else if (!ret)
			pr_err("%s wait for commit_comp timeout %d %d",
				__func__, ret, fbdev->is_committing);
		if (ret <= 0) {
			mutex_lock(&fbdev->sync_mutex);
			odinfb_signal_timeline_locked(fbdev);
			fbdev->is_committing = 0;
			complete_all(&fbdev->commit_comp);
			mutex_unlock(&fbdev->sync_mutex);
		}
	}
}

static int odinfb_pan_display_ex(struct odinfb_device *fbdev,
		struct odinfb_display_commit *disp_commit)
{
	struct odinfb_backup_type *fb_backup;
	struct fb_var_screeninfo *var = &disp_commit->var;
	u32 wait_for_finish = disp_commit->wait_for_finish;
	int ret = 0;

/*	if ((!fbdev->op_enable) || (!fbdev->panel_power_on))
		return -EPERM;

	if (var->xoffset > (info->var.xres_virtual - info->var.xres))
		return -EINVAL;

	if (var->yoffset > (info->var.yres_virtual - info->var.yres))
		return -EINVAL;
*/

	odinfb_pan_idle(fbdev);

	mutex_lock(&fbdev->sync_mutex);
/*	if (info->fix.xpanstep)
		info->var.xoffset =
		(var->xoffset / info->fix.xpanstep) * info->fix.xpanstep;

	if (info->fix.ypanstep)
		info->var.yoffset =
		(var->yoffset / info->fix.ypanstep) * info->fix.ypanstep;
*/
	fb_backup = (struct odinfb_backup_type *)fbdev->odinfb_backup;
/*	memcpy(&fb_backup->info, info, sizeof(struct fb_info));*/
	memcpy(&fb_backup->disp_commit, disp_commit,
		sizeof(struct odinfb_display_commit));
	INIT_COMPLETION(fbdev->commit_comp);
	fbdev->is_committing = 1;
	schedule_work(&fbdev->commit_work);
	mutex_unlock(&fbdev->sync_mutex);
	if (wait_for_finish)
		odinfb_pan_idle(fbdev);
	return ret;

}


static int odinfb_display_commit(struct odinfb_device *fbdev,
						unsigned long *argp)
{
	int ret;
	struct odinfb_display_commit disp_commit;
	ret = copy_from_user(&disp_commit, argp,
			sizeof(disp_commit));
	if (ret) {
		pr_err("%s:copy_from_user failed", __func__);
		return ret;
	}
	ret = odinfb_pan_display_ex(fbdev, &disp_commit);
	return ret;
}
#endif
static int odinfb_wait_for_vsync (struct odinfb_device *fbdev,
					struct odin_dss_device *display)
{
	unsigned long timeout = msecs_to_jiffies(500);
	int r;
	timeout = wait_for_completion_interruptible_timeout(
					&fbdev->vsync_comp, timeout);

	if (timeout == 0)
		r = -ETIMEDOUT;
	else
		r = timeout;

	return r;
}

int odinfb_ioctl(struct fb_info *fbi, unsigned int cmd, unsigned long arg)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_device *fbdev = ofbi->fbdev;
	struct odin_dss_device *display = fb2display(fbi);

#ifdef ODINFB_FENCE_SYNC
	struct odin_buf_sync buf_sync;
#endif
	union {
		struct odinfb_update_window_old	uwnd_o;
		struct odinfb_update_window	uwnd;
		struct odinfb_plane_info	plane_info;
		struct odinfb_caps		caps;
		struct odinfb_mem_info          mem_info;
		struct odinfb_color_key		color_key;
		struct odinfb_ovl_colormode	ovl_colormode;
		enum odinfb_update_mode		update_mode;
		int test_num;
		struct odinfb_memory_read	memory_read;
		struct odinfb_vram_info		vram_info;
		struct odinfb_tearsync_info	tearsync_info;
		struct odinfb_display_info	display_info;
		u32				crt;
		struct odinfb_addr_check iova_check;
	} p;

	int r = 0;
	FB_DBG("odinfb_ioctl\n");

#ifdef ODINFB_FENCE_SYNC
	odinfb_pan_idle(fbdev);
#endif
	switch (cmd) {
	case ODINFB_SYNC_GFX:
		FB_DBG("ioctl SYNC_GFX\n");
		if (!display || !display->driver->sync) {
			/* DSS1 never returns an error here, so we neither */
			/*r = -EINVAL;*/
			break;
		}

		r = display->driver->sync(display);
		break;

	case ODINFB_UPDATE_WINDOW_OLD:
		FB_DBG("ioctl UPDATE_WINDOW_OLD\n");
		if (!display || !display->driver->update) {
			r = -EINVAL;
			break;
		}

		if (copy_from_user(&p.uwnd_o,
					(void __user *)arg,
					sizeof(p.uwnd_o))) {
			r = -EFAULT;
			break;
		}

		r = odinfb_update_window_nolock(fbi, p.uwnd_o.x, p.uwnd_o.y,
				p.uwnd_o.width, p.uwnd_o.height);
		break;

	case ODINFB_UPDATE_WINDOW:
		FB_DBG("ioctl UPDATE_WINDOW\n");
		if (!display || !display->driver->update) {
			r = -EINVAL;
			break;
		}

		if (copy_from_user(&p.uwnd, (void __user *)arg,
					sizeof(p.uwnd))) {
			r = -EFAULT;
			break;
		}

		r = odinfb_update_window_nolock(fbi, p.uwnd.x, p.uwnd.y,
				p.uwnd.width, p.uwnd.height);
		break;

	case ODINFB_SETUP_PLANE:
		FB_DBG("ioctl SETUP_PLANE\n");
		if (copy_from_user(&p.plane_info, (void __user *)arg,
					sizeof(p.plane_info)))
			r = -EFAULT;
		else
			r = odinfb_setup_plane(fbi, &p.plane_info);
		break;

	case ODINFB_QUERY_PLANE:
		FB_DBG("ioctl QUERY_PLANE\n");
		r = odinfb_query_plane(fbi, &p.plane_info);
		if (r < 0)
			break;
		if (copy_to_user((void __user *)arg, &p.plane_info,
					sizeof(p.plane_info)))
			r = -EFAULT;
		break;

	case ODINFB_SETUP_MEM:
		FB_DBG("ioctl SETUP_MEM\n");
		if (copy_from_user(&p.mem_info, (void __user *)arg,
					sizeof(p.mem_info)))
			r = -EFAULT;
		else
			r = odinfb_setup_mem(fbi, &p.mem_info);
		break;

	case ODINFB_QUERY_MEM:
		FB_DBG("ioctl QUERY_MEM\n");
		r = odinfb_query_mem(fbi, &p.mem_info);
		if (r < 0)
			break;
		if (copy_to_user((void __user *)arg, &p.mem_info,
					sizeof(p.mem_info)))
			r = -EFAULT;
		break;

	case ODINFB_GET_CAPS:
		FB_DBG("ioctl GET_CAPS\n");
		if (!display) {
			r = -EINVAL;
			break;
		}

		memset(&p.caps, 0, sizeof(p.caps));
		if (display->caps & ODIN_DSS_DISPLAY_CAP_MANUAL_UPDATE)
			p.caps.ctrl |= ODINFB_CAPS_MANUAL_UPDATE;
		if (display->caps & ODIN_DSS_DISPLAY_CAP_TEAR_ELIM)
			p.caps.ctrl |= ODINFB_CAPS_TEARSYNC;

		if (copy_to_user((void __user *)arg, &p.caps, sizeof(p.caps)))
			r = -EFAULT;
		break;

	case ODINFB_GET_OVERLAY_COLORMODE:
		FB_DBG("ioctl GET_OVERLAY_COLORMODE\n");
		if (copy_from_user(&p.ovl_colormode, (void __user *)arg,
				   sizeof(p.ovl_colormode))) {
			r = -EFAULT;
			break;
		}
		r = odinfb_get_ovl_colormode(fbdev, &p.ovl_colormode);
		if (r < 0)
			break;
		if (copy_to_user((void __user *)arg, &p.ovl_colormode,
				 sizeof(p.ovl_colormode)))
			r = -EFAULT;
		break;

	case ODINFB_SET_UPDATE_MODE:
		FB_DBG("ioctl SET_UPDATE_MODE\n");
		if (get_user(p.update_mode, (int __user *)arg))
			r = -EFAULT;
		else
			r = odinfb_set_update_mode(fbi, p.update_mode);
		break;

	case ODINFB_GET_UPDATE_MODE:
		FB_DBG("ioctl GET_UPDATE_MODE\n");
		r = odinfb_get_update_mode(fbi, &p.update_mode);
		if (r)
			break;
		if (put_user(p.update_mode,
					(enum odinfb_update_mode __user *)arg))
			r = -EFAULT;
		break;

	case ODINFB_SET_COLOR_KEY:
		FB_DBG("ioctl SET_COLOR_KEY\n");
		if (copy_from_user(&p.color_key, (void __user *)arg,
				   sizeof(p.color_key)))
			r = -EFAULT;
		else
			r = odinfb_set_color_key(fbi, &p.color_key);
		break;

	case ODINFB_GET_COLOR_KEY:
		FB_DBG("ioctl GET_COLOR_KEY\n");
		r = odinfb_get_color_key(fbi, &p.color_key);
		if (r)
			break;
		if (copy_to_user((void __user *)arg, &p.color_key,
				 sizeof(p.color_key)))
			r = -EFAULT;
		break;

	case FBIO_WAITFORVSYNC:
		FB_DBG("ioctl FBIO_WAITFORVSYNC\n");
		if (get_user(p.crt, (__u32 __user *)arg)) {
			r = -EFAULT;
			break;
		}
		if (p.crt != 0) {
			r = -ENODEV;
			break;
		}
		/* FALLTHROUGH */

	case ODINFB_WAITFORVSYNC:
		FB_DBG("ioctl ODINFB_WAITFORVSYNC\n");
		if (!display) {
			r = -EINVAL;
			break;
		}
		r = odinfb_wait_for_vsync(fbdev, display);
		/*r = display->manager->wait_for_vsync(display->manager);*/
		break;

	/* LCD and CTRL tests do the same thing for backward
	 * compatibility */
	case ODINFB_LCD_TEST:
		FB_DBG("ioctl LCD_TEST\n");
		if (get_user(p.test_num, (int __user *)arg)) {
			r = -EFAULT;
			break;
		}
		if (!display || !display->driver->run_test) {
			r = -EINVAL;
			break;
		}

		r = display->driver->run_test(display, p.test_num);

		break;

	case ODINFB_CTRL_TEST:
		FB_DBG("ioctl CTRL_TEST\n");
		if (get_user(p.test_num, (int __user *)arg)) {
			r = -EFAULT;
			break;
		}
		if (!display || !display->driver->run_test) {
			r = -EINVAL;
			break;
		}

		r = display->driver->run_test(display, p.test_num);

		break;

	case ODINFB_MEMORY_READ:
		FB_DBG("ioctl MEMORY_READ\n");

		if (copy_from_user(&p.memory_read, (void __user *)arg,
					sizeof(p.memory_read))) {
			r = -EFAULT;
			break;
		}

		r = odinfb_memory_read(fbi, &p.memory_read);

		break;

	case ODINFB_GET_VRAM_INFO: {
#if 0
		unsigned long vram, free, largest;
#endif
		FB_DBG("ioctl GET_VRAM_INFO\n");

#if 0
		odin_vram_get_info(&vram, &free, &largest);
		p.vram_info.total = vram;
		p.vram_info.free = free;
		p.vram_info.largest_free_block = largest;

		if (copy_to_user((void __user *)arg, &p.vram_info,
					sizeof(p.vram_info)))
			r = -EFAULT;
#endif
		break;
	}

	case ODINFB_SET_TEARSYNC: {
		FB_DBG("ioctl SET_TEARSYNC\n");

		if (copy_from_user(&p.tearsync_info, (void __user *)arg,
					sizeof(p.tearsync_info))) {
			r = -EFAULT;
			break;
		}

		if (!display || !display->driver->enable_te) {
			r = -ENODEV;
			break;
		}

		r = display->driver->enable_te(display,
				!!p.tearsync_info.enabled);

		break;
	}

	case ODINFB_GET_DISPLAY_INFO: {
		u16 xres, yres;
		u32 w, h;

		FB_DBG("ioctl GET_DISPLAY_INFO\n");

		if (display == NULL) {
			r = -ENODEV;
			break;
		}

		display->driver->get_resolution(display, &xres, &yres);

		p.display_info.xres = xres;
		p.display_info.yres = yres;

		odindss_display_get_dimensions(display, &w, &h);
		p.display_info.width = w;
		p.display_info.height = h;

		if (copy_to_user((void __user *)arg, &p.display_info,
					sizeof(p.display_info)))
			r = -EFAULT;
		break;
	}

	case ODINFB_ENABLE_VSYNC:
		if (get_user(p.crt, (__u32 __user *)arg)) {
			r = -EFAULT;
			break;
		}

		if (!display) {
			r = -ENODEV;
			break;
		}
#if 0
		printk("*****************************enable_vsync(%d)\n", p.crt);
#endif
		odinfb_lock(fbdev);
		fbdev->vsync_active = !!p.crt;

		if (display->state == ODIN_DSS_DISPLAY_ACTIVE) {
			if (p.crt)
				odinfb_enable_vsync(fbdev);
			else
				odinfb_disable_vsync(fbdev);
		}
		odinfb_unlock(fbdev);
		break;

	case ODINFB_ADDR_USING:
		if (copy_from_user(&p.iova_check, (void __user *)arg,
					sizeof(p.iova_check))) {
			r = -EFAULT;
			break;
		}

		p.iova_check.iova_using = odinfb_ovl_iova_check(p.iova_check.handle_data);

		if (copy_to_user((void __user *)arg, &p.iova_check,
					sizeof(p.iova_check)))
			r = -EFAULT;

		break;

#ifdef ODINFB_FENCE_SYNC
	case ODINFB_BUFFER_SYNC:
		r = copy_from_user(&buf_sync, arg, sizeof(buf_sync));
		if (r)
			return r;

		r = odinfb_handle_buf_sync_ioctl(fbdev, &buf_sync);

		if (!r)
			r = copy_to_user(arg, &buf_sync, sizeof(buf_sync));

		break;

	case ODINFB_DISPLAY_COMMIT:
		r = odinfb_display_commit(fbdev, arg);

		break;
#endif
	default:
		dev_err(fbdev->dev, "Unknown ioctl 0x%x\n", cmd);
		r = -EINVAL;
	}

	if (r < 0)
		FB_ERR("ioctl failed: %d\n", r);

	return r;
}



