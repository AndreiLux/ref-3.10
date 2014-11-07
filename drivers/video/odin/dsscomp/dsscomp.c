/*
 * linux/drivers/video/odin/dsscomp/dsscomp.c
 * Copyright (C) 2012 LG Electronics
 * Some code and ideas taken from drivers/video/omap2/dsscomp
 * by Jaeky Oh(jaeky.oh@lge.com)
 *
 * Copyright (C) 2009 Texas Instruments
 * Author: Lajos Molnar <molnar@ti.com>
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

#define DEBUG

#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#define MODULE_NAME	"dsscomp"

#include <linux/module.h>

#include <video/odindss.h>
#include "dsscomp.h"
#include "../dss/dss.h"
#include "../dss/dss-features.h"

#include <linux/debugfs.h>
#include <linux/of.h>
#include "../xdengine/mxd.h"

#ifdef DSSCOMP_FENCE_SYNC
#include <linux/sync.h>
#include <linux/sw_sync.h>
#include <video/odin-overlay.h>
#define WAIT_FENCE_TIMEOUT 500 //100->500
#define WAIT_FENCE_FIRST_TIMEOUT MSEC_PER_SEC
#define WAIT_FENCE_FINAL_TIMEOUT (10 * MSEC_PER_SEC)
/* Display op timeout should be greater than total timeout */
#define WAIT_DISP_OP_TIMEOUT ((WAIT_FENCE_FIRST_TIMEOUT + \
		WAIT_FENCE_FINAL_TIMEOUT) * DSSCOMP_MAX_FENCE_FD)
#endif
static void dsscomp_signal_timeline_locked(struct fence_cdev *fence_cdev);
static void dsscomp_pan_idle(struct fence_cdev *fence_cdev);

static DECLARE_WAIT_QUEUE_HEAD(waitq);
static DEFINE_MUTEX(wait_mtx);

#if 0
static u32 hwc_virt_to_phys(u32 arg)
{
/*	pmd_t *pmd;
	pte_t *ptep;

	pgd_t *pgd = pgd_offset(current->mm, arg);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return 0;

	pmd = pmd_offset(pgd, arg);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return 0;

	ptep = pte_offset_map(pmd, arg);
	if (ptep && pte_present(*ptep))
		return (PAGE_MASK & *ptep) | (~PAGE_MASK & arg); */
	return 0;
}
#endif

static long setup_mgr(struct dsscomp_dev *cdev,
				struct dsscomp_setup_mgr_data *d)
{
	int r = 0;
#if 0
	int i;
	struct omap_dss_device *dev;
	struct omap_overlay_manager *mgr;
	dsscomp_t comp;
	struct dsscomp_sync_obj *sync = NULL;

	dump_comp_info(cdev, d, "queue");
	for (i = 0; i < d->num_ovls; i++)
			dump_ovl_info(cdev, d->ovls + i);

	/* verify display is valid and connected */
	if (d->mgr.ix >= cdev->num_displays)
			return -EINVAL;
	dev = cdev->displays[d->mgr.ix];
	if (!dev)
			return -EINVAL;
	mgr = dev->manager;
	if (!mgr)
			return -ENODEV;

	comp = dsscomp_new(mgr);
	if (IS_ERR(comp))
			return PTR_ERR(comp);

	/* swap red & blue if requested */
	if (d->mgr.swap_rb) {
			swap_rb_in_mgr_info(&d->mgr);
			for (i = 0; i < d->num_ovls; i++)
					swap_rb_in_ovl_info(d->ovls + i);
	}

	r = dsscomp_set_mgr(comp, &d->mgr);

	for (i = 0; i < d->num_ovls; i++) {
			struct dss_ovl_info *oi = d->ovls + i;
			u32 addr = (u32) oi->address;

			/* convert addresses to user space */
			if (oi->cfg.color_mode == OMAP_DSS_COLOR_NV12) {
					if (oi->uv_addr)
							oi->uv = hwc_virt_to_phys((u32) oi->uv_addr);
					else
							oi->uv = hwc_virt_to_phys(addr +
									oi->cfg.height * oi->cfg.stride);
			}
			oi->ba = hwc_virt_to_phys(addr);

			r = r ? : dsscomp_set_ovl(comp, oi, NULL);
	}

	r = r ? : dsscomp_setup(comp, d->mode, d->win);

	/* drop composition if failed to create */
	if (r) {
			dsscomp_drop(comp);
			return r;
	}

	if (sync) {
			sync->refs.counter++;
			comp->extra_cb = dsscomp_queue_cb;
			comp->extra_cb_data = sync;
	}
	if (d->mode & DSSCOMP_SETUP_APPLY)
			r = dsscomp_delayed_apply(comp);

	/* delete sync object if failed to apply or create file */
	if (sync) {
			r = sync_finalize(sync, r);
			if (r < 0)
					sync_drop(sync);
	}
#endif
	return r;
}

#if 0
static long query_display(struct dsscomp_dev *cdev,
			  struct dsscomp_display_info *dis)
{
#if 0
	struct odin_dss_device *dev;
	struct odin_overlay_manager *mgr;
	int i;

	/* get display */
	if (dis->ix >= cdev->num_displays)
			return -EINVAL;
	dev = cdev->displays[dis->ix];
	if (!dev)
			return -EINVAL;
	mgr = dev->manager;

	/* fill out display information */
	dis->channel = dev->channel;
	dis->enabled = (dev->state == ODIN_DSS_DISPLAY_SUSPENDED) ?
			dev->activate_after_resume :
			(dev->state == ODIN_DSS_DISPLAY_ACTIVE);
	dis->overlays_available = 0;
	dis->overlays_owned = 0;
	dis->state = dev->state;
	dis->timings = dev->panel.timings;

	dis->width_in_mm = DIV_ROUND_CLOSEST(dev->panel.width_in_um, 1000);
	dis->height_in_mm = DIV_ROUND_CLOSEST(dev->panel.height_in_um, 1000);

	/* find all overlays available for/owned by this display */
	for (i = 0; i < cdev->num_ovls && dis->enabled; i++) {
			if (cdev->ovls[i]->manager == mgr)
					dis->overlays_owned |= 1 << i;
			else if (!cdev->ovls[i]->info.enabled)
					dis->overlays_available |= 1 << i;
	}
	dis->overlays_available |= dis->overlays_owned;

	/* fill out manager information */
	if (mgr) {
			dis->mgr.alpha_blending = mgr->info.alpha_enabled;
			dis->mgr.default_color = mgr->info.default_color;
#if 0
			dis->mgr.interlaced = !strcmp(dev->name, "hdmi") &&
													is_hdmi_interlaced()
#else
			dis->mgr.interlaced =  0;
#endif
			dis->mgr.trans_enabled = mgr->info.trans_enabled;
			dis->mgr.trans_key = mgr->info.trans_key;
			dis->mgr.trans_key_type = mgr->info.trans_key_type;
	} else {
			/* display is disabled if it has no manager */
			memset(&dis->mgr, 0, sizeof(dis->mgr));
	}
	dis->mgr.ix = dis->ix;

	if (dis->modedb_len && dev->driver->get_modedb)
			dis->modedb_len = dev->driver->get_modedb(dev,
					(struct fb_videomode *) dis->modedb, dis->modedb_len);
#endif
	return 0;
}
#endif

static long check_ovl(struct dsscomp_dev *cdev,
					struct dsscomp_check_ovl_data *chk)
{
	/* for now return all overlays as possible */
	return (1 << cdev->num_ovls) - 1;
}

static long setup_display(struct dsscomp_dev *cdev,
				struct dsscomp_setup_display_data *dis)
{
	struct odin_dss_device *dev;

	/* get display */
	if (dis->ix >= cdev->num_displays)
		return -EINVAL;
	dev = cdev->displays[dis->ix];
	if (!dev)
		return -EINVAL;

	if (dev->driver->set_mode)
		return dev->driver->set_mode(dev,
					(struct fb_videomode *) &dis->mode);
	else
		return 0;
}

static void fill_cache(struct dsscomp_dev *cdev)
{
	unsigned long i;
	struct odin_dss_device *dssdev = NULL;

	cdev->num_ovls = min(odin_ovl_dss_get_num_overlays(), MAX_OVERLAYS);
	for (i = 0; i < cdev->num_ovls; i++)
			cdev->ovls[i] = odin_ovl_dss_get_overlay(i);

	cdev->num_wbs = min(odin_ovl_dss_get_num_overlays(), MAX_WRITEBACKS);
	for (i = 0; i < cdev->num_wbs; i++)
			cdev->wbs[i] = odin_dss_get_writeback(i);

	cdev->num_mgrs = min(odin_mgr_dss_get_num_overlay_managers(), MAX_MANAGERS);
	for (i = 0; i < cdev->num_mgrs; i++)
		cdev->mgrs[i] = odin_mgr_dss_get_overlay_manager(i);

	for_each_dss_dev(dssdev) {
			const char *name = dev_name(&dssdev->dev);
			if (strncmp(name, "display", 7) ||
				strict_strtoul(name + 7, 10, &i) ||
				i >= MAX_DISPLAYS)
					continue;

			if (cdev->num_displays <= i)
					cdev->num_displays = i + 1;
			cdev->displays[i] = dssdev;
			cdev->display_ix[dssdev->channel] = i;
			dev_dbg(DEV(cdev), "display%lu=%s\n", i, dssdev->driver_name);

	}
	dev_info(DEV(cdev), "found %d displays and %d overlays\n",
							cdev->num_displays, cdev->num_ovls);
}

static int dsscomp_mgr_blank(struct dsscomp_dev *cdev, int wait_for_go)
{
	struct odin_overlay_manager *mgr;
	struct odin_dss_device *dssdev;
	u32 display_ix;

	for (display_ix = 0; display_ix < cdev->num_displays; display_ix++)
	{
		dssdev = cdev->displays[display_ix];
		mgr = dssdev->manager;
		if ((mgr) && (mgr->device->state == ODIN_DSS_DISPLAY_ACTIVE))
			mgr->blank(mgr, (bool)wait_for_go);
	}
	return 0;
}

#ifdef DSSCOMP_FENCE_SYNC

void dsscomp_release_pre_buff(struct fence_cdev *fence_cdev)
{
	dsscomp_pan_idle(fence_cdev);

	mutex_lock(&fence_cdev->sync_mutex);
	INIT_COMPLETION(fence_cdev->commit_comp);

	fence_cdev->is_committing = 1;
	dsscomp_signal_timeline_locked(fence_cdev);
	fence_cdev->is_committing = 0;

	complete_all(&fence_cdev->commit_comp);

	mutex_unlock(&fence_cdev->sync_mutex);

}

void dsscomp_release_all_active_buff(struct fence_cdev *fence_cdev)
{
	int j = 0;

	mutex_lock(&fence_cdev->sync_mutex);

	while (!list_empty((const struct list_head *)
		(&(fence_cdev->timeline->obj.active_list_head))))
	{
		sw_sync_timeline_inc(fence_cdev->timeline, 1);
		fence_cdev->timeline_value++;
		j++;
	}

	if (j > 0)
	{
		fence_cdev->rel_sync_pt_value =
			fence_cdev->timeline_value + fence_cdev->sync_margin;

		DSSWARN("release %d fences in timeline (%d), next rel_sync_pt_value is:%d\n",
			j, fence_cdev->timeline_value, fence_cdev->rel_sync_pt_value);
	}
	mutex_unlock(&fence_cdev->sync_mutex);
}

static void dsscomp_signal_timeline_locked(struct fence_cdev *fence_cdev)
{
	if (fence_cdev->timeline && !list_empty((const struct list_head *)
				(&(fence_cdev->timeline->obj.active_list_head)))) {
		sw_sync_timeline_inc(fence_cdev->timeline, 1);
		fence_cdev->timeline_value++;
#if FENCE_SYNC_LOG
		printk("5)rel(%d)\n", fence_cdev->timeline_value);
#endif
	}
	fence_cdev->last_rel_fence = fence_cdev->cur_rel_fence;
	fence_cdev->cur_rel_fence = 0;
}

static void dsscomp_pan_idle(struct fence_cdev *fence_cdev)
{
	int ret;

	if (fence_cdev->is_committing) {
		ret = wait_for_completion_timeout(
				&fence_cdev->commit_comp,
			msecs_to_jiffies(WAIT_DISP_OP_TIMEOUT));
		if (ret < 0)
			ret = -ERESTARTSYS;
		else if (!ret)
			pr_err("%s wait for commit_comp timeout %d %d",
				__func__, ret, fence_cdev->is_committing);
		if (ret <= 0) {
			mutex_lock(&fence_cdev->sync_mutex);
			dsscomp_signal_timeline_locked(fence_cdev);

			fence_cdev->is_committing = 0;
			complete_all(&fence_cdev->commit_comp);
			mutex_unlock(&fence_cdev->sync_mutex);
		}
	}
}

int dsscomp_wait_for_fence (struct fence_cdev *fence_cdev)
{
	int i, ret = 0;
	/* buf sync */
	for (i = 0; i < fence_cdev->acq_fen_cnt; i++) {
		ret = sync_fence_wait(fence_cdev->acq_fen[i],
				WAIT_FENCE_FIRST_TIMEOUT);
		if (ret == -ETIME) {
			pr_warn("sync_fence_wait timed out! ");
#if 1
			pr_cont("Waiting %ld more seconds\n",
					WAIT_FENCE_FINAL_TIMEOUT/MSEC_PER_SEC);
			ret = sync_fence_wait(fence_cdev->acq_fen[i],
					WAIT_FENCE_FINAL_TIMEOUT);
#endif
			/*release all active sync_pts in current timeline*/
			if (ret == -ETIME)
				dsscomp_release_all_active_buff(fence_cdev);
		}
		if (ret < 0) {
			pr_err("%s: sync_fence_wait failed! ret = %x\n",
				__func__, ret);
			break;
		}
		sync_fence_put(fence_cdev->acq_fen[i]);
	}

	if (ret < 0) {

		while (i < fence_cdev->acq_fen_cnt) {
			sync_fence_put(fence_cdev->acq_fen[i]);
			i++;
		}
	}

	fence_cdev->acq_fen_cnt = 0;
	return ret;

}

#if FENCE_SYNC_LOG
int ictl_num = 0;
#endif
static int dsscomp_handle_buf_sync_ioctl(struct fence_cdev *fence_cdev, struct odin_buf_sync *buf_sync)
{
	int i, fence_cnt = 0, ret = 0;
	int acq_fen_fd[DSSCOMP_MAX_FENCE_FD];
	struct sync_fence *fence;
	char	buf[32];


	if ((buf_sync->acq_fen_fd_cnt > DSSCOMP_MAX_FENCE_FD) ||
		(fence_cdev->timeline == NULL))
		return -EINVAL;

	/* get acq_fen_fd[n]s for layers from HWC */
	if (buf_sync->acq_fen_fd_cnt)
		ret = copy_from_user(acq_fen_fd, buf_sync->acq_fen_fd,
				buf_sync->acq_fen_fd_cnt * sizeof(int));
	if (ret) {
		pr_err("%s:copy_from_user failed", __func__);
		return ret;
	}

	mutex_lock(&fence_cdev->sync_mutex);
	for (i = 0; i < buf_sync->acq_fen_fd_cnt; i++) {
		fence = sync_fence_fdget(acq_fen_fd[i]);
		if (fence == NULL) {
			DSSINFO("%s: null fence! i=%d fd=%d\n", __func__, i,
					acq_fen_fd[i]);
			ret = -EINVAL;
			break;
		}
		fence_cdev->acq_fen[i] = fence;
	}

	fence_cnt = i;
	if (ret)
		goto buf_sync_err_0;

	fence_cdev->acq_fen_cnt = fence_cnt;
	mutex_unlock(&fence_cdev->sync_mutex);

	/* block */
	if (buf_sync->flags & DSSCOMP_BUF_SYNC_FLAG_WAIT) {
		dsscomp_wait_for_fence(fence_cdev);
	}

	mutex_lock(&fence_cdev->sync_mutex);
	/**** send release fd to HWC ****/

	sprintf(buf, "dsscomp-fence:%d", fence_cdev->rel_sync_pt_value);
	/* if panel is writeback panel, threshold of timeline_Value should be 1 */
	fence_cdev->cur_rel_sync_pt = sw_sync_pt_create(fence_cdev->timeline,
						fence_cdev->rel_sync_pt_value++);
	if (fence_cdev->cur_rel_sync_pt == NULL) {
		pr_err("%s: cannot create sync point", __func__);
		ret = -ENOMEM;
		goto buf_sync_err_1;
	}

	/* create fence */
	fence_cdev->cur_rel_fence = sync_fence_create(buf,
				fence_cdev->cur_rel_sync_pt);
	if (fence_cdev->cur_rel_fence == NULL) {
		sync_pt_free(fence_cdev->cur_rel_sync_pt);
		fence_cdev->cur_rel_sync_pt = NULL;
		pr_err("%s: cannot create fence", __func__);
		ret = -ENOMEM;
		goto buf_sync_err_1;
	}

	/* create fd */
	fence_cdev->cur_rel_fen_fd = get_unused_fd_flags(0);
#if FENCE_SYNC_LOG
	printk("2)acq/crt:rel_fd:(%d)in %d\n",
		fence_cdev->rel_sync_pt_value-1, fence_cdev->timeline_value);
#endif
	if (fence_cdev->cur_rel_fen_fd > 0)
		sync_fence_install(fence_cdev->cur_rel_fence,
					fence_cdev->cur_rel_fen_fd);
	else {
		DSSERR("can't get unused fd : %d", fence_cdev->cur_rel_fen_fd);
		sync_fence_put(fence_cdev->cur_rel_fence);
		fence_cdev->cur_rel_fence = NULL;
		fence_cdev->cur_rel_fen_fd = 0;
		ret = -EMFILE;
		goto buf_sync_err_1;
	}

	ret = copy_to_user(buf_sync->rel_fen_fd,
		&fence_cdev->cur_rel_fen_fd, sizeof(int));
	if (ret) {
		pr_err("%s:copy_to_user failed", __func__);
		goto buf_sync_err_2;
	}
	mutex_unlock(&fence_cdev->sync_mutex);

	return ret;

buf_sync_err_2:
	/* release fence fd put */
	sync_fence_put(fence_cdev->cur_rel_fence);
	put_unused_fd(fence_cdev->cur_rel_fen_fd);
	fence_cdev->cur_rel_fence = NULL;
	fence_cdev->cur_rel_fen_fd = 0;

buf_sync_err_1:
	mutex_unlock(&fence_cdev->sync_mutex);
	return ret;

buf_sync_err_0:
	/* acquire fence fd put */
	for (i = 0; i < fence_cnt; i++)
		sync_fence_put(fence_cdev->acq_fen[i]);
	fence_cdev->acq_fen_cnt = 0;
	mutex_unlock(&fence_cdev->sync_mutex);
	return ret;
}
#endif

static int frame_cnt = 0;
extern int wait_vsync_before_setting_cnt;
static long comp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int r = 0;
	struct miscdevice *dev = filp->private_data;
	struct dsscomp_dev *cdev = container_of(dev, struct dsscomp_dev, dev);
	void __user *ptr = (void __user *)arg;
#ifdef DSSCOMP_FENCE_SYNC
	struct odin_buf_sync	buf_sync;
	struct fence_cdev *fence_cdev;
#endif
	int wait_for_go;
	int wb_ix;
	int vsync_ix;
	struct dsscomp_get_pipe_status	pipe_staus;
#ifdef DSS_TIME_CHECK_LOG
	unsigned int ms_interval = 0;
	s64 ms_time;
#endif
	__u8 m2m_enable;

	union {
		struct {
				struct dsscomp_setup_mgr_data set;
				struct dss_ovl_info ovl[MAX_OVERLAYS];
		} m;
		struct dsscomp_setup_du_data du;
		struct dsscomp_display_info dis;
		struct dsscomp_check_ovl_data chk;
		struct dsscomp_setup_display_data sdis;
		struct dsscomp_wait_for_vsync_data vsync;
        	struct dsscomp_vsync_ctrl_data vsync_ctrl;
	} u;

	switch (cmd) {
	case DSSCOMP_SETUP_MGR:
	{
		DSSDBG("enter DSSCOMP_SETUP_MGR:%x\n",cmd);
		r = copy_from_user(&u.m.set, ptr, sizeof(u.m.set)) ? :
			u.m.set.num_ovls >= ARRAY_SIZE(u.m.ovl) ? -EINVAL :
			copy_from_user(&u.m.ovl,
						(void __user *)arg + sizeof(u.m.set),
						sizeof(*u.m.ovl) * u.m.set.num_ovls) ? :
			setup_mgr(cdev, &u.m.set);
		break;
	}
	case DSSCOMP_SETUP_DU:
	{
		DSSDBG("enter DSSCOMP_SETUP_DISPC:%x\n",cmd);

#ifdef DSS_TIME_CHECK_LOG
		frame_cnt ++;
		if (frame_cnt == 1)
			odin_mgr_dss_time_start(ODIN_VIDEO_FPS);

		if (frame_cnt > 100)
		{
			ms_time = odin_mgr_dss_time_end_ms(ODIN_VIDEO_FPS);
			ms_interval = ((unsigned int) ms_time / (frame_cnt -1));
			DSSINFO("check_time:%d, ms = %ld, fps = %d , wait_vsync_bf_cnt:%d \n",
				check_time, ms_interval, 1000/ms_interval, wait_vsync_before_setting_cnt);
			frame_cnt = 0;
			wait_vsync_before_setting_cnt = 0;
		}
#endif

#if FENCE_SYNC_LOG
		/* printk("3)ictl\n"); */
#endif
		r = copy_from_user(&u.du, ptr, sizeof(u.du)) ? :
			dsscomp_gralloc_queue_ioctl(&u.du);
		break;
	}

	case DSSCOMP_QUERY_DISPLAY:
	{
#if 0
		struct dsscomp_display_info *dis = NULL;
		r = copy_from_user(&u.dis, ptr, sizeof(u.dis));
		if (!r)
				dis = kzalloc(sizeof(*dis->modedb) * u.dis.modedb_len +
										sizeof(*dis), GFP_KERNEL);
		if (dis) {
				*dis = u.dis;
				r = query_display(cdev, dis) ? :
					copy_to_user(ptr, dis, sizeof(*dis) +
						sizeof(*dis->modedb) * dis->modedb_len);
				kfree(dis);
		} else {
				r = r ? : -ENOMEM;
		}
#endif
		break;
	}
	case DSSCOMP_CHECK_OVL:
	{
		DSSDBG("enter DSSCOMP_CHECK_OVL:%x\n",cmd);
		r = copy_from_user(&u.chk, ptr, sizeof(u.chk)) ? :
			check_ovl(cdev, &u.chk);
		break;
	}
	case DSSCOMP_SETUP_DISPLAY:
	{
		DSSDBG("enter DSSCOMP_SETUP_DISPLAY:%x\n",cmd);
		r = copy_from_user(&u.sdis, ptr, sizeof(u.sdis)) ? :
			setup_display(cdev, &u.sdis);
		break;
	}

	case DSSCOMP_WAIT_VSYNC:
		r = copy_from_user(&u.vsync, ptr, sizeof(u.vsync)) ? :
			dsscomp_wait_for_vsync(&u.vsync);
		break;

    case DSSCOMP_VSYNC_CTRL:
        r = copy_from_user(&u.vsync_ctrl, ptr, sizeof(u.vsync_ctrl));
        r = dsscomp_vsync_ctrl(&u.vsync_ctrl);
        break;

	case DSSCOMP_MGR_BLANK:
		DSSINFO("enter DSSCOMP_MGR_BLANK:%x\n",cmd); /* change INFO -> DBG */
		r = copy_from_user(&wait_for_go, ptr, sizeof(int)) ? :
			dsscomp_mgr_blank(cdev, wait_for_go);
		break;

	case DSSCOMP_WAIT_WB_DONE:
		r = copy_from_user(&wb_ix, ptr, sizeof(int)) ? :
			dsscomp_wait_for_wb_done(wb_ix);
		break;
#ifdef DSSCOMP_FENCE_SYNC
	case DSSCOMP_BUFFER_SYNC:

		r = copy_from_user(&buf_sync, (void __user *)arg, sizeof(buf_sync));
		if (r)
		{
			printk("copy_from_user:%d\n", r);
			return r;
		}

		fence_cdev = &cdev->fence_cdev[buf_sync.dpy];
#if FENCE_SYNC_LOG
		/* printk("1)buffer_sync_ioctl(%d)\n", fence_cdev->timeline_value); */
#endif

		dsscomp_pan_idle(fence_cdev);

		r = dsscomp_handle_buf_sync_ioctl(fence_cdev, &buf_sync);

		if (!r)
		{
			r = copy_to_user((void __user *)arg, &buf_sync, sizeof(buf_sync));
		}
		else{
			printk("buf_sync_error:%d\n", r);
		}
		break;
	case DSSCOMP_RELEASE_SYNC:

		r = copy_from_user(&buf_sync, (void __user *)arg, sizeof(buf_sync));
		if (r)
		{
			printk("copy_from_user:%d\n", r);
			return r;
		}
		fence_cdev = &cdev->fence_cdev[buf_sync.dpy];
#if FENCE_SYNC_LOG
		printk("_)release_n_refresh_sync in dpy[%d](%d)\n", buf_sync.dpy, fence_cdev->timeline_value);
#endif
		dsscomp_release_fence_display (buf_sync.dpy, true); /* refresh = true */
		break;

	case DSSCOMP_REFRESH_SYNC:

		r = copy_from_user(&buf_sync, (void __user *)arg, sizeof(buf_sync));
		if (r)
		{
			printk("copy_from_user:%d\n", r);
			return r;
		}
#if FENCE_SYNC_LOG
		printk("_)refresh sync_pt_value in dpy[%d]\n", buf_sync.dpy);
#endif
		dsscomp_refresh_syncpt_value(buf_sync.dpy);
		break;

#endif

	case DSSCOMP_GET_PIPE_STATUS:
		r = dsscomp_get_pipe_status(&pipe_staus);
		if (!r)
		{
			r = copy_to_user((void __user *)arg, &pipe_staus, sizeof(dsscomp_get_pipe_status));
		}
		break;

	case DSSCOMP_MEM2MEM_ENABLE:
		r = copy_from_user(&m2m_enable, (void __user *)arg, sizeof(__u8));
		if (r)
		{
			printk("copy_from_user:%d\n", r);
			return r;
		}

		printk("DSSCOMP_MEM2MEM_ENABLE(%d)\n", m2m_enable);
#ifdef WRITEBACK_FRAMEDONE_POLLONG
#else
		r = mxd_on_for_m2m_on(m2m_enable);
#endif
		break;

	case 0xffffffff:
	{
		DSSDBG("dsscomp ioctl test : 0xffffff \n");
		break;
	}

	default:
		r = -EINVAL;
		break;
	}
	return r;
}


/* must implement open for filp->private_data to be filled */
static int comp_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int comp_mmap(struct file *filp, struct vm_area_struct *vma)
{
 	unsigned long off;
	unsigned long start;
	u32 len;

	DSSDBG("comp_mmap mmap enter enter\n");
	DSSDBG("vma_debug - vma->vm_end:%x, vma->vm_start:%x vma->vm_pgoff:%x\n",
			(u32)vma->vm_end, (u32)vma->vm_start, (u32)vma->vm_pgoff);

	if (vma->vm_end - vma->vm_start == 0)
		return 0;
	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;
	off = vma->vm_pgoff << PAGE_SHIFT;

	start = DSSCOMP_MMAP_PHYS;
	len   = DSSCOMP_MMAP_SIZE;
	if (off >= len)
		return -EINVAL;
	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;

	off += start;

	DSSDBG("user mmap region start %lx, len %d, off %lx\n", start, len, off);

	vma->vm_pgoff = off >> PAGE_SHIFT;
	/* vma->vm_flags |= VM_RESERVED | VM_READ | VM_WRITE; */
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, DSSCOMP_MMAP_PHYS >> PAGE_SHIFT,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;

 	return 0;
}


static const struct file_operations comp_fops = {
	.owner		= THIS_MODULE,
	.open		= comp_open,
	.unlocked_ioctl = comp_ioctl,
    .mmap		= comp_mmap,
};

static int dsscomp_debug_show(struct seq_file *s, void *unused)
{
	void (*fn)(struct seq_file *s) = s->private;
	fn(s);
	return 0;
}

static int dsscomp_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, dsscomp_debug_show, inode->i_private);
}

static const struct file_operations dsscomp_debug_fops = {
	.open           = dsscomp_debug_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int dsscomp_probe(struct platform_device *pdev)
{
	int ret;
#ifdef DSSCOMP_FENCE_SYNC
	int i;
#endif
	struct dsscomp_dev *cdev = kzalloc(sizeof(*cdev), GFP_KERNEL);
	if (!cdev) {
			pr_err("dsscomp: failed to allocate device.\n");
			return -ENOMEM;
	}
	cdev->dev.minor = MISC_DYNAMIC_MINOR;
	cdev->dev.name = "dsscomp";
	cdev->dev.mode = 0666;
	cdev->dev.fops = &comp_fops;

	ret = misc_register(&cdev->dev);
	if (ret) {
			pr_err("dsscomp: failed to register misc device.\n");
			return ret;
	}
	cdev->dbgfs = debugfs_create_dir("dsscomp", NULL);
	if (IS_ERR_OR_NULL(cdev->dbgfs))
			dev_warn(DEV(cdev), "failed to create debug files.\n");
	else {
			debugfs_create_file("comps", S_IRUGO,
					cdev->dbgfs, dsscomp_dbg_comps, &dsscomp_debug_fops);
			debugfs_create_file("gralloc", S_IRUGO,
					cdev->dbgfs, dsscomp_dbg_gralloc, &dsscomp_debug_fops);
#ifdef CONFIG_DSSCOMP_DEBUG_LOG
			debugfs_create_file("log", S_IRUGO,
					cdev->dbgfs, dsscomp_dbg_events, &dsscomp_debug_fops);
#endif
	}
	platform_set_drvdata(pdev, cdev);

	pr_info("dsscomp: initializing.\n");

	fill_cache(cdev);

#ifdef DSSCOMP_FENCE_SYNC
	for (i = PRIMARY_DISPLAY; i < NUM_DISPLAY_TYPES; i++) {	/* 0,1 */
		struct fence_cdev *fence_cdev = &cdev->fence_cdev[i];
		char *timeline_name;

		mutex_init(&fence_cdev->sync_mutex);
		init_completion(&fence_cdev->commit_comp);

		/* timeline init */
		if (fence_cdev->timeline == NULL) {
			timeline_name = (i == PRIMARY_DISPLAY) ? "dsscomp_timeline:primary" :
					(i == EXTERNAL_DISPLAY) ? "dsscomp_timeline:external" :
					(i == VIRTUAL_DISPLAY) ? "dsscomp_timeline:virtual" :
					"no_lcd_timeline";
			/*sprintf(timeline_name, "dsscomp_timeline:%d", i);*/
			fence_cdev->timeline = sw_sync_timeline_create(timeline_name);
			if (fence_cdev->timeline == NULL) {
				pr_err("%s: cannot create time line", __func__);
				return -ENOMEM;
			} else {
				fence_cdev->timeline_value = 0;
				fence_cdev->sync_margin = (i == PRIMARY_DISPLAY)? 1 :
							(i == EXTERNAL_DISPLAY)? 2 :
							(i == VIRTUAL_DISPLAY)? 1 : 0; /* it can be changed later. */
				fence_cdev->rel_sync_pt_value = fence_cdev->timeline_value + fence_cdev->sync_margin;
			}
		}
	}
#endif

	/* initialize queues */
	dsscomp_queue_init(cdev);
	dsscomp_gralloc_init(cdev);

	return 0;
}


static int dsscomp_remove(struct platform_device *pdev)
{
	struct dsscomp_dev *cdev = platform_get_drvdata(pdev);
	misc_deregister(&cdev->dev);
	debugfs_remove_recursive(cdev->dbgfs);
	dsscomp_queue_exit();
	dsscomp_gralloc_exit();
	kfree(cdev);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id dsscomp_match[] = {
	{
		.compatible = "dsscomp",
	},
	{},
};
#else
static struct platform_device dsscomp_pdev = {
	.name = MODULE_NAME,
	.id = -1
};
#endif

static struct platform_driver dsscomp_pdriver = {
	.probe = dsscomp_probe,
	.remove = dsscomp_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(dsscomp_match),
#endif
	}
};

static int __init dsscomp_init(void)
{
	int err = platform_driver_register(&dsscomp_pdriver);
	if (err)
		return err;

#ifdef CONFIG_OF
#else
	err = platform_device_register(&dsscomp_pdev);

	if (err)
		platform_driver_unregister(&dsscomp_pdriver);

#endif
	return err;
}

static void __exit dsscomp_exit(void)
{
#ifdef CONFIG_OF
#else
	platform_device_unregister(&dsscomp_pdev);
#endif
	platform_driver_unregister(&dsscomp_pdriver);
}

#define DUMP_CHUNK 256
static char dump_buf[64 * 1024];
void dsscomp_kdump(void)
{

	struct seq_file s = {
		.buf = dump_buf,
		.size = sizeof(dump_buf) - 1,
	};
	int i;

#ifdef CONFIG_DSSCOMP_DEBUG_LOG
	dsscomp_dbg_events(&s);
#endif
	dsscomp_dbg_comps(&s);
	dsscomp_dbg_gralloc(&s);

	for (i = 0; i < s.count; i += DUMP_CHUNK) {
		if ((s.count - i) > DUMP_CHUNK) {
			char c = s.buf[i + DUMP_CHUNK];
			s.buf[i + DUMP_CHUNK] = 0;
			pr_cont("%s", s.buf + i);
			s.buf[i + DUMP_CHUNK] = c;
		} else {
			s.buf[s.count] = 0;
			pr_cont("%s", s.buf + i);
		}
	}

}
EXPORT_SYMBOL(dsscomp_kdump);
MODULE_LICENSE("GPL v2");
module_init(dsscomp_init);
module_exit(dsscomp_exit);
