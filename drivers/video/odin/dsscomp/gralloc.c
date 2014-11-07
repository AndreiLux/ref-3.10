#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/ion.h>
#include <linux/odin_iommu.h>
#include <video/odindss.h>
#include "dsscomp.h"
#include "../dss/dss.h"
#include "../dss/cabc/cabc.h"
#include "../dss/mipi-dsi.h"
#include "../odinfb/odinfb.h"
#include "linux/delay.h"
#include "linux/hrtimer.h"
#include <linux/odin_dfs.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

static struct dsscomp_dev *cdev;
DEFINE_MUTEX(mem2mem_mtx);
static struct semaphore free_slots_sem =
				__SEMAPHORE_INITIALIZER(free_slots_sem, 0);

/* gralloc composition sync object */
struct dsscomp_gralloc_t {
	void (*cb_fn)(void *, int);
	void *cb_arg;
	struct list_head q;
	struct list_head slots;
	atomic_t refs;
	bool early_callback;
	bool programmed;
};


/* queued gralloc compositions */
static LIST_HEAD(flip_queue);

static u32 ovl_use_mask[MAX_MANAGERS];
static u32 wb_use_mask[MAX_MANAGERS];

#if 0
static struct {
	bool enable;
	u32 mgr_ix;
} mem2mem_propery_data;
#endif

#ifdef ODIN_ION_FD
static struct ion_client *dss_client;
#endif

static struct hrtimer hr_timer;
static struct completion fps_ctrl_completion;
static unsigned long fps_to_ns[60] = {
	/* fps 0, 1, 2, 3, 4
		5, 6, 7, 8, 9
			...
		55, 56, 57, 58, 59 */
	0, 1000000000, 500000000, 333333333, 250000000,
	200000000, 166666666, 142857142, 125000000, 111111111,
	100000000, 90909090, 83333333, 76923076, 71428571,
	66666666, 62500000, 58823529, 55555555,	52631578,
	50000000, 47619047, 45454545, 43478260, 41666666,
	40000000, 38461538, 37037037, 35714285, 34482758,
	33333333, 32258064, 31250000, 30303030, 29411764,
	28571428, 27777777, 27027027, 26315789, 25641025,
	25000000, 24390243, 23809523, 23255813, 22727272,
	22222222, 21739130, 21276595, 20833333, 20408163,
	20000000, 19607843, 19230769, 18867924, 18518518,
	18181818, 17857142, 17543859, 17241379, 16949152

};
#ifdef LCD_ESD_CHECK
#define VSYNC_MAX_TIMEOUT 2
unsigned int vsync_timeout_cnt;
#endif

#ifdef ODIN_ION_FD
int dsscomp_ion_client_create(void)
{
	dss_client = odin_ion_client_create( "dss_comp" );
	if (IS_ERR_OR_NULL(dss_client))
	{
		DSSERR("dsscomp_ion_client_create failed\n");
		return -EINVAL;
	}
	return 0;
}

void dsscomp_ion_client_destory(void)
{
	odin_ion_client_destroy( dss_client );
}

struct ion_client* dsscomp_get_ion_client(void)
{
	return dss_client;
}
#endif

int odin_dsscomp_set_default_overlay(int ovl_ix, int mgr_ix)
{
	DSSDBG("odin_dsscomp_set_default_overlay \n");
	ovl_use_mask[mgr_ix] |= (1 << ovl_ix);
	return 0;
}

int dsscomp_gralloc_queue_ioctl(struct dsscomp_setup_du_data *d)
{
	s32 ret;

	ret = dsscomp_gralloc_queue(d, false, NULL, NULL);
	return ret;
}

static bool dsscomp_is_device_active(int display_ix)
{
	struct odin_dss_device *dssdev;
	dssdev = cdev->displays[display_ix];
	if (dssdev && dssdev->state == ODIN_DSS_DISPLAY_ACTIVE)
		return true;
	else
		return false;
}
static bool dsscomp_is_any_device_active(void)
{
	u32 display_ix;
	for (display_ix = 0 ; display_ix < cdev->num_displays ; display_ix++) {
		if (dsscomp_is_device_active(display_ix)) {
			return true;
		}
	}

	return false;
}

int dsscomp_gralloc_queue(struct dsscomp_setup_du_data *d,
			bool early_callback,
			void (*cb_fn)(void *, int), void *cb_arg)
{
	u32 i;
	int r = 0;
	struct odin_dss_device *dev;
	struct odin_overlay_manager *mgr;
	static DEFINE_MUTEX(local_mtx);
	dsscomp_t comp[MAX_MANAGERS];
	u32 ovl_new_use_mask[MAX_MANAGERS];
	u32 wb_new_use_mask[MAX_MANAGERS];
	u32 mgr_set_mask = 0;
	u32 ovl_set_mask = 0;
	u32 wb_set_mask = 0;
	u32 channels[ARRAY_SIZE(d->mgrs)], ch;
	u32 display_ix;
	struct dss_rect_t win = { .w = 0 };
	int mem2mem_mode = 0;
	unsigned char destroy_index;
	struct coordinate layers[MAX_OVERLAYS];
	int layer_cnt = 0;
	struct ion_handle *ion_fd_handle = NULL;
	u64 debug_addr1 = 0;

	mutex_lock(&local_mtx);

	d->num_mgrs = min(d->num_mgrs, (u16) ARRAY_SIZE(d->mgrs));
	d->num_ovls = min(d->num_ovls, (u16) ARRAY_SIZE(d->ovls));

	memset(comp, 0, sizeof(comp));
	memset(ovl_new_use_mask, 0, sizeof(ovl_new_use_mask));
	memset(wb_new_use_mask, 0, sizeof(wb_new_use_mask));

	if (d->mode & DSSCOMP_SETUP_MODE_MEM2MEM)
		mem2mem_mode = true;
	if (mem2mem_mode)
		mutex_lock(&mem2mem_mtx);

	/* Mem2mem mode, All inactive */
	if (!dsscomp_is_any_device_active() && !mem2mem_mode)
		goto skip_comp;

	/* Mem2mem mode, primary inactive */
	if (!dsscomp_is_device_active(ODIN_PRIMARY_LCD) &&
			mem2mem_mode && !wb_use_mask[ODIN_MEM2MEM_SYNC])
	{
		r = -EBUSY;
		goto skip_comp;
	}

	if ((d->num_ovls == 0) && (d->num_wbs == 0))
		goto skip_comp;

	/* mark managers we are using */
	for (i = 0; i < d->num_mgrs; i++) {
		display_ix = cdev->display_ix[d->mgrs[i].ix];
		/* verify display is valid & connected, ignore if not */
		if (display_ix >= cdev->num_displays)
			continue;
		dev = cdev->displays[display_ix];
		if (!dev) {
			dev_warn(DEV(cdev), "failed to get display%d\n",
								display_ix);
			continue;
		}
		mgr = dev->manager;
		if (!mgr) {
			dev_warn(DEV(cdev), "no manager for display%d\n",
								display_ix);
			continue;
		}

		channels[i] = ch = mgr->id;
		mgr_set_mask |= 1 << ch;
	}

	/* create dsscomp objects for set managers (including active ones) */
	for (ch = 0; ch < MAX_MANAGERS; ch++) {
		if (!(mgr_set_mask & (1 << ch)))
			continue;

		mgr = cdev->mgrs[ch];

		comp[ch] = dsscomp_new(mgr);
		if (IS_ERR(comp[ch])) {
			comp[ch] = NULL;
			dev_warn(DEV(cdev), "failed to get composition on %s\n",
								mgr->name);
			continue;
		}
		comp[ch]->mem2mem_mode = mem2mem_mode;
		if (mgr->id == ODIN_DSS_CHANNEL_LCD0)
			comp[ch]->manual_update = dssdev_manually_updated(mgr->device);
		else
			comp[ch]->manual_update = 0;
		/* set basic manager information for blanked managers */
		if (!(mgr_set_mask & (1 << ch))) {
			struct dss_mgr_info mi = {
				.alpha_blending = true,
				.ix = comp[ch]->frm.mgr.ix,
			};
			dsscomp_set_mgr(comp[ch], &mi);
		}

		comp[ch]->must_apply = true;
		comp[ch]->imported_cnt = 0;

		r = dsscomp_setup(comp[ch], d->mode, win);
		if (r)
			dev_err(DEV(cdev), "failed to setup comp (%d)\n", r);
	}

	/* configure manager data from gralloc composition */
	for (i = 0; i < d->num_mgrs; i++) {
		ch = channels[i];
		r = dsscomp_set_mgr(comp[ch], d->mgrs + i);
		if (r)
			dev_err(DEV(cdev), "failed to set mgr%d (%d)\n", ch, r);
	}

	/* NOTE: none of the dsscomp sets should fail as composition is new */
	for (i = 0; i < d->num_ovls; i++) {
		struct dss_ovl_info *oi = d->ovls + i;
		u32 mgr_ix = oi->cfg.mgr_ix;

		ch = mgr_ix;
		/* skip overlays on compositions we could not create */

		if (!comp[ch])
			continue;

		if (oi->cfg.enabled)
			ovl_new_use_mask[ch] |= 1 << oi->cfg.ix;

#ifdef ODIN_ION_FD
		if (oi->addr_type != ODIN_DSS_BUFADDR_FB)
		{
			if (oi->fd > 2)
			{
				ion_fd_handle = ion_import_dma_buf(dss_client, oi->fd);

				if (IS_ERR_OR_NULL(ion_fd_handle))
				{
					DSSERR("can't get ovl import_dma_buf handle(%p) %p (%d)\n",
						ion_fd_handle, oi->client, oi->fd);
					goto skip_comp;
				}
				else
				{
					if (!(odin_ion_get_mapped_subsys_of_buffer(ion_fd_handle)
						& ODIN_SUBSYS_DSS))
					{
						DSSINFO("(oi%d)DSS unmapping frame\n", i);
						odin_ion_map_iommu(ion_fd_handle, ODIN_SUBSYS_DSS);
					}
				}
				oi->fd = odin_ion_get_iova_of_buffer(ion_fd_handle, ODIN_SUBSYS_DSS);
				debug_addr1 = odin_ion_get_pa_mbyte_of_buffer(ion_fd_handle);
				comp[ch]->imported_hnd[comp[ch]->imported_cnt++] = ion_fd_handle;
			}
			else
			{
				dev_err(DEV(cdev), "un-supported oi->fd %d \n", oi->fd);
				goto skip_comp;
			}
		}
#endif

		r = dsscomp_set_ovl(comp[ch], oi, debug_addr1);
		if (r)
			dev_err(DEV(cdev), "failed to set ovl%d (%d)\n",
								oi->cfg.ix, r);
		else
			ovl_set_mask |= 1 << oi->cfg.ix;

		if ((oi->cfg.enabled) && (!oi->cfg.invisible))
		{
			if (oi->addr_type == ODIN_DSS_BUFADDR_FB)
			{
				layers[layer_cnt].x_left = 0;
				layers[layer_cnt].y_top = 0;
				layers[layer_cnt].x_right = 1080;
				layers[layer_cnt].y_bottom =1920;
				layer_cnt++;
			}
			else
			{
				layers[layer_cnt].x_left = oi->cfg.win.x;
				layers[layer_cnt].y_top = oi->cfg.win.y;
				layers[layer_cnt].x_right = oi->cfg.win.x + oi->cfg.win.w;
				layers[layer_cnt].y_bottom = oi->cfg.win.y + oi->cfg.win.h;
				layer_cnt++;
			}
		}
	}

	for (i = 0; i < d->num_wbs; i++) {
		struct dss_wb_info *wi = d->wbs + i;
		u32 mgr_ix = wi->cfg.mgr_ix;

		ch = mgr_ix;

		/* skip overlays on compositions we could not create */
		if (!comp[ch])
			continue;

		if (wi->cfg.enabled)
			wb_new_use_mask[ch] |= 1 << wi->cfg.ix;

#ifdef ODIN_ION_FD
		if (wi->addr_type != ODIN_DSS_BUFADDR_FB)
		{
			if (wi->fd > 2) {
				ion_fd_handle = ion_import_dma_buf(dss_client, wi->fd);
				if (IS_ERR_OR_NULL(ion_fd_handle))
				{
					DSSERR("can't get wb import_dma_buf handle(%p) %p (%d)\n",
						ion_fd_handle, wi->client, wi->fd);
					goto skip_comp;
				}
				else
				{
					if (!(odin_ion_get_mapped_subsys_of_buffer(ion_fd_handle)
						& ODIN_SUBSYS_DSS))
					{
						DSSINFO("(wi%d)DSS unmapping frame\n", i);
						odin_ion_map_iommu(ion_fd_handle, ODIN_SUBSYS_DSS);
					}
				}
				wi->fd = odin_ion_get_iova_of_buffer(ion_fd_handle, ODIN_SUBSYS_DSS);
				debug_addr1 = odin_ion_get_pa_mbyte_of_buffer(ion_fd_handle);
				comp[ch]->imported_hnd[comp[ch]->imported_cnt++] = ion_fd_handle;
			}
			else
			{
				dev_err(DEV(cdev), "un-supported wi->fd %d \n", wi->fd);
				goto skip_comp;
			}
		}
#endif
		r = dsscomp_set_wb(comp[ch], wi, debug_addr1);
		if (r)
			dev_err(DEV(cdev), "failed to set wb%d (%d)\n",
								wi->cfg.ix, r);
		else
			wb_set_mask |= 1 << wi->cfg.ix;
	}

	/* disabled wb  is not inputed in display&mem2mem */
	if (!(d->mode & (DSSCOMP_SETUP_MODE_CAPTURE
		| DSSCOMP_SETUP_MODE_DISPLAY_CAPTURE)))
		wb_set_mask &= ovl_set_mask;

	for (ch = 0; ch < MAX_MANAGERS; ch++) {
		/* disable all overlays not specifically set from prior frame */
		u32 mask = ovl_use_mask[ch] & ~ovl_set_mask;
		u32 wb_mask = wb_use_mask[ch] & ~wb_set_mask;

		if (!comp[ch])
			continue;

		/* Don' touch other overlay and writeback */
		if (d->mode & DSSCOMP_SETUP_MODE_CAPTURE)
		{
			mask = 0;
			wb_mask = 0;

		}

		while (mask) {
			struct dss_ovl_info oi = {
				.cfg.zonly = true,
				.cfg.enabled = false,
				.cfg.ix = fls(mask) - 1,
			};
			dsscomp_set_ovl(comp[ch], &oi, 0);
			mask &= ~(1 << oi.cfg.ix);
		}

		while (wb_mask) {
			struct dss_wb_info wi = {
				.cfg.enabled = false,
				.cfg.ix = fls(wb_mask) - 1,
			};
			dsscomp_set_wb(comp[ch], &wi, 0);
			wb_mask &= ~(1 << wi.cfg.ix);
		}

#ifdef DSS_VIDEO_PLAYBACK_CABC
		comp[ch]->cabc_en = false;
		if(odin_cabc_is_on()) {
			if (ch == 0)
			{
				if (ovl_use_mask[ch] & ((1 << ODIN_DSS_VID0)
				    | (1 << ODIN_DSS_VID1) | (1 << ODIN_DSS_VID2_FMT)))
					comp[ch]->cabc_en = true;
				else
					comp[ch]->cabc_en = false;
			}
		}
#else
		comp[ch]->cabc_en = false;
#endif

		if (ch == 0)
		{
#ifdef DSS_REGION_CHECK_DFS_REQUEST_CLK
#if DSS_LAYER_CNT_DFS_REQUEST_CLK /* it is 0 */
			if (layer_cnt > 2)
#else /* real running code */
			/*if (dss_intersection_overlap_check(&layers, layer_cnt))*/
			if (layer_cnt >= 4)
#endif
			{
				/* DSSINFO("overlap 800Mhz Setting , layer_cnt:%d\n", layer_cnt);*/
				comp[ch]->mem_dfs = 800000;
			}
			else if (layer_cnt == 3)
			{
				if (dss_intersection_overlap_check(&layers, layer_cnt))
					comp[ch]->mem_dfs = 600000;
				else
					comp[ch]->mem_dfs = 400000;
				/* DSSINFO("overlap %dMhz Setting, layer_cnt:%d\n", comp[ch]->mem_dfs/1000, layer_cnt);*/
			}
			else if (layer_cnt <= 2)
			{
				/* DSSINFO("overlap 300Mhz Setting, layer_cnt:%d\n", layer_cnt);*/
				comp[ch]->mem_dfs = 300000;
			}
#endif
		}

		if (!mem2mem_mode)
			r = dsscomp_delayed_apply(comp[ch]);
		else
			r = dsscomp_direct_apply(comp[ch]);

		if (r)
			dev_err(DEV(cdev), "failed to apply comp (%d)\n", r);
		else
		{
			if (!(d->mode & (DSSCOMP_SETUP_MODE_CAPTURE
					| DSSCOMP_SETUP_MODE_DISPLAY_CAPTURE)))
			{
				ovl_use_mask[ch] = ovl_new_use_mask[ch];
				wb_use_mask[ch] = wb_new_use_mask[ch];
			}
		}
	}
	if (mem2mem_mode)
		mutex_unlock(&mem2mem_mtx);
	mutex_unlock(&local_mtx);

	return r;

skip_comp:
	dev_dbg(DEV(cdev), "skip a comp image, not insert workqueue\n");

	for (ch = 0; ch < MAX_MANAGERS; ch++) {
		if (!comp[ch])
			continue;
		dsscomp_drop(comp[ch]);
	}

#ifdef DSSCOMP_FENCE_SYNC
	if (!mem2mem_mode)
	{
		/* we assume that buf_sync_ioctl have not been called because of numHWLayer = 1,
		   so rel_sync_pt_value might be kept in last value state.
		   for releasing pre_buff, rel_sync_pt_value should be also refreshed */
		for (i = 0; i < NUM_DISPLAY_TYPES; i++) {
			struct fence_cdev *fence_cdev = &cdev->fence_cdev[i];

			dsscomp_release_pre_buff(fence_cdev);
			mutex_lock(&fence_cdev->sync_mutex);
			/*refresh rel_sync_pt_value */
			fence_cdev->rel_sync_pt_value = fence_cdev->timeline_value
								+ fence_cdev->sync_margin;
			mutex_unlock(&fence_cdev->sync_mutex);
		}
	}
#endif
	if (mem2mem_mode)
		mutex_unlock(&mem2mem_mtx);
	mutex_unlock(&local_mtx);

	return r;
}

bool dsscomp_is_wb_pending_wait(void)
{
	int retry_cnt = 0;

	while (wb_use_mask[ODIN_MEM2MEM_SYNC])
	{
		msleep(1);
		retry_cnt++;
		if (retry_cnt > 50)
		{
			DSSINFO("mem2mem timeout 50ms \n");
			break;
		}
	}

	return true;
}

#ifdef CONFIG_EARLYSUSPEND
static int blank_complete;
static DECLARE_WAIT_QUEUE_HEAD(early_suspend_wq);

static void dsscomp_early_suspend_cb(void *data, int status)
{
	blank_complete = true;
	wake_up(&early_suspend_wq);
}

static void dsscomp_early_suspend(struct early_suspend *h)
{
	struct dsscomp_setup_du_data d = {
		.num_mgrs = 0,
	};
	int err;

	pr_info("DSSCOMP: %s\n", __func__);

	/* use gralloc queue as we need to blank all screens */
	blank_complete = false;
	dsscomp_gralloc_queue(&d, false, dsscomp_early_suspend_cb, NULL);

	/* wait until composition is displayed */
	err = wait_event_timeout(early_suspend_wq, blank_complete,
				 msecs_to_jiffies(500));
	if (err == 0)
		pr_warn("DSSCOMP: timeout blanking screen\n");
	else
		pr_info("DSSCOMP: blanked screen\n");
}

static void dsscomp_late_resume(struct early_suspend *h)
{
	pr_info("DSSCOMP: %s\n", __func__);
	blanked = false;
}

static struct early_suspend early_suspend_info = {
	.suspend = dsscomp_early_suspend,
	.resume = dsscomp_late_resume,
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
};
#endif

void dsscomp_dbg_gralloc(struct seq_file *s)
{
	return;
}

enum hrtimer_restart vsync_timer_callback(struct hrtimer *timer)
{
	complete(&fps_ctrl_completion);
	return HRTIMER_NORESTART;
}

int dsscomp_ctrl_wait_for_vsync(struct odin_overlay_manager *mgr, int fps_value)
{
	int r = 0;
	ktime_t ktime;
	unsigned long timeout = msecs_to_jiffies(500);

	if ((fps_value < 1) || (fps_value > 59))
	{
		DSSERR("invalid fps_value :%d\n", fps_value);
		return -EINVAL;
	}

	ktime = ktime_set(0, fps_to_ns[fps_value]);

	INIT_COMPLETION(fps_ctrl_completion);
	hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);

	timeout = wait_for_completion_interruptible_timeout(&fps_ctrl_completion,
			timeout);
#if 0
	printk("timer wakeup(fps:%d)\n", fps_value);
#endif

	if (timeout == 0)
		r = -ETIMEDOUT;
	else if (timeout == -ERESTARTSYS)
		r = timeout;

	if ((r < 0) && (mgr->device->state == ODIN_DSS_DISPLAY_ACTIVE))
		DSSERR("vsync time out (%d)\n", r);

	hrtimer_cancel(&hr_timer);
	return r;
}
extern void lm3697_lcd_backlight_set_level(int level);
extern int bl_get_brightness(void);
extern void lm3697_lcd_backlight_set_level_For_ESD(int level,int on_off);


int dsscomp_wait_for_vsync(struct dsscomp_wait_for_vsync_data *vsync)
{
	int r = -EINVAL;
	int i;
	struct odin_dss_device *dev;
	struct odin_overlay_manager *mgr;
	u32 display_ix;
	int dfs_ref_cnt;
	extern atomic_t vsync_ready;

	if (!(dsscomp_is_device_active(vsync->mgr_ix)))
	{
		r = -EBUSY;
		msleep(16);
		//DSSWARN("%s:%d (lcd device is not active)\n",__func__, r);
		return r;
	}

	display_ix = cdev->display_ix[vsync->mgr_ix];
	/* verify display is valid & connected, ignore if not */
	if (display_ix >= cdev->num_displays)
		goto skip_wait_for_vsync;
	dev = cdev->displays[display_ix];
	if (!dev) {
		dev_warn(DEV(cdev), "failed to get display%d\n",
							display_ix);
		goto skip_wait_for_vsync;
	}
	mgr = dev->manager;
	if (!mgr) {
		dev_warn(DEV(cdev), "no manager for display%d\n",
							display_ix);
		goto skip_wait_for_vsync;
	}

	if (dssdev_manually_updated(dev))
	{
		for (i = 0; i < vsync->wait_vsync_try; i++)
		{
			if (vsync->fps_value >= 60)
				r = mgr->wait_for_vsync(mgr);
			else /* for ctrl vsync period */
				r = dsscomp_ctrl_wait_for_vsync(mgr, vsync->fps_value);
			if (r < 0 && (mgr->device->state == ODIN_DSS_DISPLAY_ACTIVE)) /* exception case */
			{
#if defined(DSS_DFS_SPINLOCK)
				dfs_ref_cnt = get_mem_dfs_ref_cnt();
#endif
				if (atomic_read(&vsync_ready))
					dev->driver->flush_pending(dev);
#if defined(DSS_DFS_SPINLOCK)
				DSSINFO("dfs lock count is before=%d, after=%d \n",
					dfs_ref_cnt, get_mem_dfs_ref_cnt());
#endif

#ifdef LCD_ESD_CHECK
        			if (r == -ETIMEDOUT) {
                			vsync_timeout_cnt++;
					printk("vsync_timeout_cnt:%d\n", vsync_timeout_cnt);
					int ret = dsscomp_get_esd_check();
					if (ret >= 0) /* mipi bta check detect error status */
					{
						lm3697_lcd_backlight_set_level_For_ESD(bl_get_brightness(),0);
						DSSINFO(">>>>>>>>>>>>>>>>>>>>>>>>>get_esd_check returns:%d<<<<<<<<<<<<<<<<<<<<<<<\n", ret);
						mutex_lock(&((struct odinfb_device *)mgr->fbdev)->mtx);
						if (mgr->device->state != ODIN_DSS_DISPLAY_SUSPENDED) {
							mgr->device->driver->suspend(mgr->device);

							msleep(200);

							if (mgr->device->state == ODIN_DSS_DISPLAY_SUSPENDED) {
								mgr->device->driver->resume(mgr->device);
							}

						}
						mutex_unlock(&((struct odinfb_device *)mgr->fbdev)->mtx);
						lm3697_lcd_backlight_set_level_For_ESD(bl_get_brightness(),1);
						DSSINFO(">>>>>>>>>>>>>>>restore routine: sleep->delay->resume done.<<<<<<<<<<<<<<<\n");
					}

				}
#endif
			}
#ifdef LCD_ESD_CHECK
			if (r >= 0)
				vsync_timeout_cnt = 0;
#endif
		}
	}
	else
	{
		for (i = 0; i < vsync->wait_vsync_try; i++)
		{
			if (vsync->fps_value >= 60)
				r = mgr->wait_for_vsync(mgr);
			else
				r = dsscomp_ctrl_wait_for_vsync(mgr, vsync->fps_value);
		}
	}

skip_wait_for_vsync:
	return r;
}

int dsscomp_vsync_ctrl(struct dsscomp_vsync_ctrl_data *vsync_ctrl)
{
    int r = 0;
	struct odin_dss_device *dssdev = cdev->displays[vsync_ctrl->ix];
	struct odin_dss_driver *drv = dssdev->driver;
	struct odin_overlay_manager *mgr = cdev->mgrs[vsync_ctrl->ix];

	if (!(dsscomp_is_device_active(vsync_ctrl->ix)))
	{
		if (vsync_ctrl->enable)
			mipi_dsi_vsync_enable(dssdev);
		else
			mipi_dsi_vsync_disable(dssdev);
		return r;
	}

    if (vsync_ctrl->enable) {
		mipi_dsi_vsync_enable(dssdev);
	}
	else {
        r = dsscomp_complete_workqueue(mgr->device, false, false);
        if (r < 0)
        {
            DSSERR("dsscomp_vsync_ctrl complete workqueue error\n");
			return r;
        }
        /* doesn't need to disable clk 'case of complete_wq func. doesn't call enable clk */
        /* disable_runtime_clk(mgr->id);*/
		mipi_dsi_vsync_disable(dssdev);
	}

    return r;
}

int dsscomp_wait_for_wb_done(int wb_ix)
{

	struct odin_writeback *wb;
	wb = odin_dss_get_writeback(wb_ix);

	wb->wait_framedone(wb);

	return 0;
}
#ifdef LCD_ESD_CHECK
int dsscomp_get_esd_check(void)
{
       int r = -EAGAIN;
       int read_val = 0;

       struct odin_overlay_manager *mgr = cdev->mgrs[ODIN_PRIMARY_LCD];
       if ((vsync_timeout_cnt >= VSYNC_MAX_TIMEOUT) &&
               (dsscomp_is_device_active(mgr->id)))
       {
                r = dsscomp_complete_workqueue(mgr->device, true, false);
                if (r < 0)
                {
                          DSSERR("dsscomp_get_esd_check complete workqueue error\n");
                          return r;
                }
                DSSINFO("vsync timeout cnt is expired for ESD \n");
                vsync_timeout_cnt = 0;

                /*MIPI BTA 1More Check */ /*no err:0, err:1*/
                read_val = odin_mipi_dsi_check_AwER(0);
                if(read_val) {
                    DSSERR("BTA AwER value = %x \n",read_val);
                }
                r = (read_val)? 1 : 0;
                return r;
       }
       return r;
}
#endif

void dsscomp_gralloc_init(struct dsscomp_dev *cdev_)
{
	/* save at least cdev pointer */
	if (!cdev && cdev_) {
		cdev = cdev_;
	}

	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = &vsync_timer_callback;

	init_completion(&fps_ctrl_completion);

	return;
}

void dsscomp_gralloc_exit(void)
{
	return;
}
