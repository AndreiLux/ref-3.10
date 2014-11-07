/*
 * linux/drivers/video/odin/dsscomp/queue.c
 *
 * DSS Composition queueing support
 *
 * Copyright (C) 2011 Texas Instruments, Inc
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

#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/odin_iommu.h>
#include <linux/delay.h>
#include <video/odindss.h>

#include <linux/debugfs.h>

#include "dsscomp.h"
#include "../dss/dss.h"
#include "../dss/dss-features.h"
#include "../dss/mipi-dsi.h"
#include <linux/pm_qos.h>
#include <linux/odin_dfs.h>
#include "../dss/cabc/cabc.h"

#ifdef DSSCOMP_FENCE_SYNC
#include "dsscomp.h"
#include <linux/sw_sync.h>
#endif

/* queue state */

static DEFINE_MUTEX(mtx);

/* free overlay structs */
struct maskref {
	u32 mask;
	u32 refs[MAX_OVERLAYS];
};

static struct {
	struct workqueue_struct *apply_workq;

	u32 ovl_mask;		/* overlays used on this display */
	u32 ovl_dmask;		/* overlays disabled on this display */
	u32 wb_mask;		/* overlays used on this display */
	struct maskref ovl_qmask;		/* overlays queued to this display */
	struct maskref wb_qmask;		/* writebacks queued to this display */
	bool blanking;
	bool cabc_en;
} mgrq[MAX_MANAGERS];

/*Workqueue for Independent Capture*/
static struct workqueue_struct *capture_wkq;		/* capture_wkq work queue */
static struct workqueue_struct *cabc_wkq;			/* capture_wkq work queue */

#if RUNTIME_CLK_GATING
bool wb_status = false;
/* at first booting, clks starts from ungating status */
static int clk_gating_status = 1;
spinlock_t clk_gating_lock;
#endif
#if 1
atomic_t vsync_ready = ATOMIC_INIT(0);
#endif

static struct workqueue_struct *cb_wkq;		/* callback work queue */
static struct dsscomp_dev *cdev;
#if 0
static int overlay_num_debug = 0;
#endif
static u32 current_min_freq = 0; /* 200000; */
#ifdef DSS_TIME_CHECK_LOG
enum odin_dss_time_stamp check_time = ODIN_VIDEO_FPS;
#endif

#ifdef CONFIG_DEBUG_FS
LIST_HEAD(dbg_comps);
DEFINE_MUTEX(dbg_mtx);
#endif

#ifdef CONFIG_DSSCOMP_DEBUG_LOG
struct dbg_event_t dbg_events[128];
u32 dbg_event_ix;
#endif

static inline void __log_state(dsscomp_t c, void *fn, u32 ev)
{
#ifdef CONFIG_DSSCOMP_DEBUG_LOG
	if (c->dbg_used < ARRAY_SIZE(c->dbg_log)) {
		u32 t = (u32) ktime_to_ms(ktime_get());
		c->dbg_log[c->dbg_used].t = t;
		c->dbg_log[c->dbg_used++].state = c->state;
		__log_event(20 * c->ix + 20, t, c, ev ? "%pf on %s" : "%pf",
				(u32) fn, (u32) log_status_str(ev));
	}
#endif
}
#define log_state(c, fn, ev) DO_IF_DEBUG_FS(__log_state(c, fn, ev))

static inline void maskref_incbit(struct maskref *om, u32 ix)
{
	om->refs[ix]++;
	om->mask |= 1 << ix;
}

static void maskref_decmask(struct maskref *om, u32 mask)
{
	while (mask) {
		u32 ix = fls(mask) - 1, m = 1 << ix;
		mask &= ~m;
		if (!om->refs[ix])
			continue;
		if (!--om->refs[ix])
			om->mask &= ~m;
	}
}

static void maskref_reset(struct maskref *om,  u32 mask)
{
	while (mask) {
		u32 ix = fls(mask) - 1, m = 1 << ix;
		om->refs[ix] = 0;
		om->mask &= ~m;
		mask &= ~m;
	}
}

#if RUNTIME_CLK_GATING
void get_clk_status(void)
{
	unsigned long flags;

	spin_lock_irqsave(&clk_gating_lock, flags);
	clk_gating_status++;
	spin_unlock_irqrestore(&clk_gating_lock, flags);
}

void put_clk_status(void)
{
	unsigned long flags;

	spin_lock_irqsave(&clk_gating_lock, flags);
	clk_gating_status--;
	/* printk("(-):%d\n",clk_gating_status);*/
	spin_unlock_irqrestore(&clk_gating_lock, flags);

}

void release_clk_status (int value)
{
	unsigned long flags;

	spin_lock_irqsave(&clk_gating_lock, flags);
	clk_gating_status = value; /* value maybe 0 or 1 */
	spin_unlock_irqrestore(&clk_gating_lock, flags);

}

bool check_clk_gating_status(void)
{
	unsigned long flags, r;
	spin_lock_irqsave(&clk_gating_lock, flags);
	r = (clk_gating_status)? true : false;
	spin_unlock_irqrestore(&clk_gating_lock, flags);

	DSSINFO("clk_gating_status:%d\n", clk_gating_status);
	return r;
}

void enable_runtime_clk (struct odin_overlay_manager *mgr, bool mem2mem_mode)
{
	unsigned long flags;
	spin_lock_irqsave(&clk_gating_lock, flags);

	if (!mem2mem_mode)
	{
		if (!clk_gating_status)
		{
			odin_crg_runtime_clk_enable(mgr->id, true);
			mgr->clk_enable(mgrq[mgr->id].ovl_mask, 0, false);
		}
		clk_gating_status++;
	}
	else /* WB */
	{
		if (!clk_gating_status)
			odin_crg_dss_clk_ena(CRG_CORE_CLK, SYNCGEN_CLK, true);

#if 0 /* Don't use debug because of spinlock */
#ifdef RUNTIME_CLK_GATING_LOG
		printk("w");
#endif
#endif
	}
#if 0 /* Don't use debug because of spinlock */
#ifdef RUNTIME_CLK_GATING_LOG
	printk("[r+]%d):%d,%x, %x\n", wb_status, clk_gating_status,
				odin_crg_get_dss_clk_ena(CRG_CORE_CLK),
				odin_crg_get_dss_clk_ena(CRG_OTHER_CLK));
#endif
#endif
	spin_unlock_irqrestore(&clk_gating_lock, flags);
}

void disable_runtime_clk (enum odin_channel channel)
{
	unsigned long flags;
	spin_lock_irqsave(&clk_gating_lock, flags);
	clk_gating_status--;
	if (!clk_gating_status)
	{
		u32 clk_mask;

		odin_crg_runtime_clk_enable(channel, 0);
		clk_mask = odin_crg_du_plane_to_clk(mgrq[channel].ovl_mask, false);
		odin_crg_dss_clk_ena(CRG_CORE_CLK, clk_mask, false);
	}
#if 0 /* Don't use debug because of spinlock */
#ifdef RUNTIME_CLK_GATING_LOG
	printk("[r-]%d):%d,%x, %x\n", wb_status, clk_gating_status,
			odin_crg_get_dss_clk_ena(CRG_CORE_CLK),
				odin_crg_get_dss_clk_ena(CRG_OTHER_CLK));
#endif
#endif
	spin_unlock_irqrestore(&clk_gating_lock, flags);
}
#endif
/*
 * ===========================================================================
 *		EXIT
 * ===========================================================================
 */

/* Initialize queue structures, and set up state of the displays */
int dsscomp_queue_init(struct dsscomp_dev *cdev_)
{
	u32 i, j;
	cdev = cdev_;

	if (ARRAY_SIZE(mgrq) < cdev->num_mgrs)
		return -EINVAL;

	ZERO(mgrq);
	for (i = 0; i < cdev->num_mgrs; i++) {
		struct odin_overlay_manager *mgr;
		mgrq[i].apply_workq = create_singlethread_workqueue("dsscomp_apply");
		if (!mgrq[i].apply_workq)
			goto error;

		/* record overlays on this display */
		mgr = cdev->mgrs[i];
		for (j = 0; j < cdev->num_ovls; j++) {
			if (cdev->ovls[j]->info.enabled &&
			    mgr &&
			    cdev->ovls[j]->manager == mgr)
				mgrq[i].ovl_mask |= 1 << j;
		}
	}

	capture_wkq = create_singlethread_workqueue("dsscomp_apply_capture");
	cabc_wkq = create_singlethread_workqueue("dsscomp_cabc");

	cb_wkq = create_singlethread_workqueue("dsscomp_cb");
	if (!cb_wkq)
		goto error;
#if RUNTIME_CLK_GATING
	spin_lock_init(&clk_gating_lock);
#endif

	return 0;
error:
	while (i--)
		destroy_workqueue(mgrq[i].apply_workq);
	return -ENOMEM;
}

/* get display index from manager */
static u32 get_display_ix(struct odin_overlay_manager *mgr)
{
	u32 i;

	/* handle if manager is not attached to a display */
	if (!mgr || !mgr->device)
		return cdev->num_displays;

	/* find manager's display */
	for (i = 0; i < cdev->num_displays; i++)
		if (cdev->displays[i] == mgr->device)
			break;

	return i;
}

/*
 * ===========================================================================
 *		QUEUING SETUP OPERATIONS
 * ===========================================================================
 */

/* create a new composition for a display */
dsscomp_t dsscomp_new(struct odin_overlay_manager *mgr)
{
	struct dsscomp_data *comp = NULL;
	u32 display_ix = get_display_ix(mgr);

	/* check manager */
	u32 ix = mgr ? mgr->id : cdev->num_mgrs;
	if (ix >= cdev->num_mgrs || display_ix >= cdev->num_displays)
		return ERR_PTR(-EINVAL);

	/* allocate composition */
	comp = kzalloc(sizeof(*comp), GFP_KERNEL);
	if (!comp)
		return NULL;

	/* initialize new composition */
	comp->ix = ix;	/* save where this composition came from */
	comp->ovl_mask = comp->ovl_dmask = 0;
	comp->frm.sync_id = 0;
#if 0
	comp->frm.mgr.ix = display_ix;
#else
	comp->frm.mgr.ix = mgr->id;
#endif

	comp->state = DSSCOMP_STATE_ACTIVE;

	DO_IF_DEBUG_FS({
		__log_state(comp, dsscomp_new, 0);
		list_add(&comp->dbg_q, &dbg_comps);
	});

	return comp;
}
EXPORT_SYMBOL(dsscomp_new);

/* returns overlays used in a composition */
u32 dsscomp_get_ovls(dsscomp_t comp)
{
	u32 mask;

	mutex_lock(&mtx);
	BUG_ON(comp->state != DSSCOMP_STATE_ACTIVE);
	mask = comp->ovl_mask;
	mutex_unlock(&mtx);

	return mask;
}
EXPORT_SYMBOL(dsscomp_get_ovls);

/* set overlay info */
int dsscomp_set_ovl(dsscomp_t comp, struct dss_ovl_info *ovl,
	u64 debug_addr1)
{
	int r = -EBUSY;
	u32 i, mask, oix, ix;
	struct odin_overlay *o;

	mutex_lock(&mtx);

	BUG_ON(!ovl);
	BUG_ON(comp->state != DSSCOMP_STATE_ACTIVE);

	ix = comp->ix;

	if (ovl->cfg.ix >= cdev->num_ovls) {
		r = -EINVAL;
		goto done;
	}

	/* if overlay is already part of the composition */
	mask = 1 << ovl->cfg.ix;
	if (mask & comp->ovl_mask) {
		/* look up overlay */
		for (oix = 0; oix < comp->frm.num_ovls; oix++) {
			if (comp->ovls[oix].cfg.ix == ovl->cfg.ix)
				break;
		}
		BUG_ON(oix == comp->frm.num_ovls);
	} else {
		/* check if ovl is free to use */
		if (comp->frm.num_ovls >= ARRAY_SIZE(comp->ovls))
		{
			DSSERR("ovl is not free to use \n");
			goto done;
		}

		/* not in any other displays queue */
		if (mask & ~mgrq[ix].ovl_qmask.mask) {
			for (i = 0; i < cdev->num_mgrs; i++) {
				if (i == ix)
					continue;
				if (mgrq[i].ovl_qmask.mask & mask)
				{
					DSSERR("using in any other displays queue \n");
					goto done;
				}
			}
		}

		/* and disabled (unless forced) if on another manager */
		o = cdev->ovls[ovl->cfg.ix];
		if (o->info.enabled &&
		   (!o->manager || o->manager->id != ix))
		{
			DSSERR("pipe to use on mgr %d is using on another manager %d \n",
				ix, o->manager->id);
			goto done;
		}

		/* add overlay to composition & display */
		comp->ovl_mask |= mask;
		oix = comp->frm.num_ovls++;
		maskref_incbit(&mgrq[ix].ovl_qmask, ovl->cfg.ix);
	}

	comp->ovls[oix] = *ovl;
	comp->debug_addr1[oix] = debug_addr1;
	r = 0;

done:
	mutex_unlock(&mtx);

	return r;
}
EXPORT_SYMBOL(dsscomp_set_ovl);

/* get overlay info */
int dsscomp_get_ovl(dsscomp_t comp, u32 ix, struct dss_ovl_info *ovl)
{
	int r;
	u32 oix;

	mutex_lock(&mtx);

	BUG_ON(!ovl);
	BUG_ON(comp->state != DSSCOMP_STATE_ACTIVE);

	if (ix >= cdev->num_ovls) {
		r = -EINVAL;
	} else if (comp->ovl_mask & (1 << ix)) {
		r = 0;
		for (oix = 0; oix < comp->frm.num_ovls; oix++)
			if (comp->ovls[oix].cfg.ix == ovl->cfg.ix) {
				*ovl = comp->ovls[oix];
				break;
			}
		BUG_ON(oix == comp->frm.num_ovls);
	} else {
		r = -ENOENT;
	}

	mutex_unlock(&mtx);

	return r;
}
EXPORT_SYMBOL(dsscomp_get_ovl);

/* set writeback info */
int dsscomp_set_wb(dsscomp_t comp, struct dss_wb_info *wb,
	u64 debug_addr1)
{
	int r = -EBUSY;
	u32 i, mask, wix, ix;
	struct odin_writeback *w;

	mutex_lock(&mtx);

	BUG_ON(!wb);
	BUG_ON(comp->state != DSSCOMP_STATE_ACTIVE);

	ix = comp->ix;

	if (wb->cfg.ix >= cdev->num_wbs) {
		r = -EINVAL;
		goto done;
	}

	/* if writeback is already part of the composition */
	mask = 1 << wb->cfg.ix;
	if (mask & comp->wb_mask) {
		/* look up overlay */
		for (wix = 0; wix < comp->frm.num_wbs; wix++) {
			if (comp->wbs[wix].cfg.ix == wb->cfg.ix)
				break;
		}
		BUG_ON(wix == comp->frm.num_wbs);
	} else {
		/* check if wb is free to use */
		if (comp->frm.num_wbs >= ARRAY_SIZE(comp->wbs))
		{
			DSSERR("num_wbs is over : num_wbs = %d\n", comp->frm.num_wbs);
			goto done;
		}

		/* not in any other displays queue */
		if (mask & ~mgrq[ix].wb_qmask.mask) {
			for (i = 0; i < cdev->num_mgrs; i++) {
				if (i == ix)
					continue;
				if (mgrq[i].wb_qmask.mask & mask)
				{
					DSSERR("other displays queue is using : mgr_ix = %d", i);
					goto done;
				}
			}
		}

#if 0 /* writeback check is skipped */
		/* and disabled (unless forced) if on another manager */
		w = cdev->wbs[wb->cfg.ix];
		if (w->info.enabled &&
		   (w->info.channel != ix))
			{
				DSSERR("another manager : cdev->mgr = %d, wb_info->mgr = %d\n",
					w->info.channel, ix);
				goto done;
			}
#endif
		/* add writeback to composition & display */
		comp->wb_mask |= mask;
		wix = comp->frm.num_wbs++;
		maskref_incbit(&mgrq[ix].wb_qmask, wb->cfg.ix);
	}

	comp->wbs[wix] = *wb;
	comp->wb_debug_addr1[wix] = debug_addr1;
	r = 0;
done:
	mutex_unlock(&mtx);

	return r;
}
EXPORT_SYMBOL(dsscomp_set_wb);

/* set manager info */
int dsscomp_set_mgr(dsscomp_t comp, struct dss_mgr_info *mgr)
{
	mutex_lock(&mtx);

	DSSDBG("mgr->ix = %d, comp->frm.mgr.ix = %d \n", mgr->ix, comp->frm.mgr.ix);
	BUG_ON(comp->state != DSSCOMP_STATE_ACTIVE);
	BUG_ON(mgr->ix != comp->frm.mgr.ix);

	comp->frm.mgr = *mgr;

	mutex_unlock(&mtx);

	return 0;
}
EXPORT_SYMBOL(dsscomp_set_mgr);

/* get manager info */
int dsscomp_get_mgr(dsscomp_t comp, struct dss_mgr_info *mgr)
{
	mutex_lock(&mtx);

	BUG_ON(!mgr);
	BUG_ON(comp->state != DSSCOMP_STATE_ACTIVE);

	*mgr = comp->frm.mgr;

	mutex_unlock(&mtx);

	return 0;
}
EXPORT_SYMBOL(dsscomp_get_mgr);

/* get manager info */
int dsscomp_setup(dsscomp_t comp, enum dsscomp_setup_mode mode,
			struct dss_rect_t win)
{
	mutex_lock(&mtx);

	BUG_ON(comp->state != DSSCOMP_STATE_ACTIVE);

	comp->frm.mode = mode;
	comp->frm.win = win;

	mutex_unlock(&mtx);

	return 0;
}
EXPORT_SYMBOL(dsscomp_setup);

/*
 * ===========================================================================
 *		QUEUING COMMITTING OPERATIONS
 * ===========================================================================
 */

void dsscomp_drop(dsscomp_t comp)
{
	int i;
	struct ion_client *client = dsscomp_get_ion_client();
	/* decrement unprogrammed references */
	if (comp->state < DSSCOMP_STATE_PROGRAMMED)
	{
		maskref_decmask(&mgrq[comp->ix].ovl_qmask, comp->ovl_mask);
		maskref_decmask(&mgrq[comp->ix].wb_qmask, comp->wb_mask);
	}
	comp->state = 0;

	if (comp->mem2mem_mode || comp->manual_update)
	{
		for (i = 0; i < comp->imported_cnt; i++)
		{
			ion_free(client, comp->imported_hnd[i]);
		}
	}
	else /* video mode */
	{
		for (i = 0; i < comp->imported_cnt_backup; i++)
		{
			ion_free(client, comp->imported_hnd_backup[i]);
		}
	}

	if (debug & DEBUG_COMPOSITIONS)
		dev_info(DEV(cdev), "[%p] released\n", comp);

	DO_IF_DEBUG_FS(list_del(&comp->dbg_q));

	kfree(comp);
}
EXPORT_SYMBOL(dsscomp_drop);

struct dsscomp_cb_work {
	struct work_struct work;
	struct dsscomp_data *comp;
	int status;
	int fence_lcd_type;
	bool static_work;
};

#define NUM_DSSCOMP_CB 4
static struct dsscomp_cb_work reserved_work[NUM_DSSCOMP_CB];

static struct ion_handle *imported_hnd_backup[7];
static int imported_cnt_backup = 0;

static void dsscomp_mgr_delayed_cb(struct work_struct *work)
{
	struct dsscomp_cb_work *wk = container_of(work, typeof(*wk), work);
	struct dsscomp_data *comp = wk->comp;
	int status = wk->status;
	int lcd_type = wk->fence_lcd_type;
	u32 ix;
	int i;

	bool manual_update_mode, mem2mem_mode;

	if (!wk->static_work)
		kfree(wk);
	else
		wk->static_work = false;

	if ((status == DSS_COMPLETION_FENCE_RELEASE) ||
			(status == DSS_COMPLETION_FENCE_RELEASE_N_REFRESH))
	{
		struct fence_cdev *fence_dev = &cdev->fence_cdev[lcd_type];
#ifdef DSSCOMP_FENCE_SYNC
		dsscomp_release_pre_buff(fence_dev);	/* cdev->timeline_value */
#if 1
		if (status == DSS_COMPLETION_FENCE_RELEASE_N_REFRESH)
		{
			mutex_lock(&fence_dev->sync_mutex);
			fence_dev->rel_sync_pt_value = fence_dev->timeline_value +
						fence_dev->sync_margin; /*refresh rel_sync_pt_value */
			mutex_unlock(&fence_dev->sync_mutex);
		}
#endif
		return;
#endif
	}

	mutex_lock(&mtx);

	BUG_ON(comp->state == DSSCOMP_STATE_ACTIVE);
	ix = comp->ix;
	manual_update_mode = comp->manual_update;
	mem2mem_mode = comp->mem2mem_mode;

	/* call extra callbacks if requested */
	if (comp->extra_cb)
		comp->extra_cb(comp->extra_cb_data, status);

	/* handle programming & release */
	if (status == DSS_COMPLETION_PROGRAMMED) {
		comp->state = DSSCOMP_STATE_PROGRAMMED;
		log_state(comp, dsscomp_mgr_delayed_cb, status);

		/* update used overlay mask */
		mgrq[ix].ovl_mask = comp->ovl_mask & ~comp->ovl_dmask;
		mgrq[ix].ovl_dmask = comp->ovl_dmask;
		maskref_decmask(&mgrq[ix].ovl_qmask, comp->ovl_mask);

		/* update used wb mask */
		mgrq[ix].wb_mask = comp->wb_mask & ~comp->wb_dmask;
		maskref_decmask(&mgrq[ix].wb_qmask, comp->wb_mask);

		if (debug & DEBUG_PHASES)
			dev_info(DEV(cdev), "[%p] programmed\n", comp);
	} else if ((status == DSS_COMPLETION_DISPLAYING) &&
		   comp->state == DSSCOMP_STATE_PROGRAMMED) {
#ifdef DSSCOMP_FENCE_SYNC
		/* release HERE in video mode && not WB */
		if (!mem2mem_mode) {
			if (ix != ODIN_DSS_CHANNEL_LCD0)
#if 0
				dsscomp_release_pre_buff(&cdev->fence_cdev[PRIMARY_DISPLAY]);
			else	/* HDMI */
#endif
				dsscomp_release_pre_buff(&cdev->fence_cdev[EXTERNAL_DISPLAY]);
		}
#endif
		/* composition is 1st displayed */
		comp->state = DSSCOMP_STATE_DISPLAYED;
		log_state(comp, dsscomp_mgr_delayed_cb, status);
		if (debug & DEBUG_PHASES)
			dev_info(DEV(cdev), "[%p] displayed\n", comp);
		/* composition is no longer displayed */
		log_event(20 * comp->ix + 20, 0, comp, "%pf on %s",
				(u32) dsscomp_mgr_delayed_cb,
				(u32) log_status_str(status));
#if 1
		if ((!manual_update_mode) && (!mem2mem_mode)) {
			comp->imported_cnt_backup = imported_cnt_backup;
			for (i = 0; i < imported_cnt_backup; i++)
				comp->imported_hnd_backup[i] = imported_hnd_backup[i];

			imported_cnt_backup = comp->imported_cnt;
			for (i = 0; i < comp->imported_cnt; i++)
				imported_hnd_backup[i] = comp->imported_hnd[i];
		}
#endif
		dsscomp_drop(comp);
	} else if (status & DSS_COMPLETION_PROGRAM_ERROR) {
#ifdef DSSCOMP_FENCE_SYNC
		DSSWARN("dsscomp_apply return error_callback\n");
		/* release HERE in video mode && not WB */
		if (!mem2mem_mode) {
			if (ix == ODIN_DSS_CHANNEL_LCD0)
				dsscomp_release_pre_buff(&cdev->fence_cdev[PRIMARY_DISPLAY]);
			else	/* HDMI */
				dsscomp_release_pre_buff(&cdev->fence_cdev[EXTERNAL_DISPLAY]);
		}
#endif
		/* composition is no longer displayed */
		log_event(20 * comp->ix + 20, 0, comp, "%pf on %s",
				(u32) dsscomp_mgr_delayed_cb,
				(u32) log_status_str(status));
#if 1
		if ((!manual_update_mode) && (!mem2mem_mode)) {
			comp->imported_cnt_backup = imported_cnt_backup;
			for (i = 0; i < imported_cnt_backup; i++)
				comp->imported_hnd_backup[i] = imported_hnd_backup[i];

			imported_cnt_backup = comp->imported_cnt;
			for (i = 0; i < comp->imported_cnt; i++)
				imported_hnd_backup[i] = comp->imported_hnd[i];
		}
#endif
		dsscomp_drop(comp);
	}
	else if (status & DSS_COMPLETION_MANUAL_DISPLAYED) {
		/* composition is no longer displayed */
		comp->state = DSSCOMP_STATE_DISPLAYED;
		log_event(20 * comp->ix + 20, 0, comp, "%pf on %s",
				(u32) dsscomp_mgr_delayed_cb,
				(u32) log_status_str(status));
		dsscomp_drop(comp);
	}

	mutex_unlock(&mtx);
}

static u32 dsscomp_mgr_callback(void *data, int id, int status)
{
	struct dsscomp_data *comp = data;
	int i;
	enum dsscomp_state comp_state;

	if (comp)
		comp_state = comp->state;
	else
		comp_state = DSSCOMP_STATE_DISPLAYED;

	if (status == DSS_COMPLETION_PROGRAMMED ||
	    (status == DSS_COMPLETION_DISPLAYING &&
	     comp_state != DSSCOMP_STATE_DISPLAYED) ||
		(status & DSS_COMPLETION_PROGRAM_ERROR) ||
		(status & DSS_COMPLETION_MANUAL_DISPLAYED) ||
		(status == DSS_COMPLETION_FENCE_RELEASE) ||
		(status == DSS_COMPLETION_FENCE_RELEASE_N_REFRESH)) {
		struct dsscomp_cb_work *wk = kzalloc(sizeof(*wk), GFP_NOWAIT);
		if (!wk)
		{
			DSSINFO(" dsscomp_mgr_callback atomic is fulled \n");
			for (i = 0; i < NUM_DSSCOMP_CB; i++) {
				if (reserved_work[i].static_work)
				continue;
				wk = &reserved_work[i];
				break;
			}
			if (!wk)
			{
				DSSERR(" dsscomp_mgr_callback static is fulled \n");
				return 0xffffffff;
			}

			wk->static_work = true;
		}
		else
			wk->static_work = false;

		wk->comp = comp;
		wk->status = status;
		wk->fence_lcd_type = id;

		INIT_WORK(&wk->work, dsscomp_mgr_delayed_cb);
		queue_work(cb_wkq, &wk->work);
	}

	/* get each callback only once */
	return ~status;
}

void dsscomp_release_fence_display(int display_num, bool refresh)
{
	if (refresh) /* needs for refresh rel_sync_pt count value. */
		dsscomp_mgr_callback(NULL, display_num,
			DSS_COMPLETION_FENCE_RELEASE_N_REFRESH);
	else /* default case of just releasing fence */
		dsscomp_mgr_callback(NULL, display_num,
			DSS_COMPLETION_FENCE_RELEASE);
}

void dsscomp_hdmi_free(void)
{
	int i;
	struct ion_client *client = dsscomp_get_ion_client();
	mutex_lock(&mtx);
	for (i = 0; i < imported_cnt_backup; i++)
	{
		ion_free(client, imported_hnd_backup[i]);
		DSSINFO("hdmi imported_hnd free = %x \n", imported_hnd_backup[i]);
	}
	imported_cnt_backup = 0;
	mutex_unlock(&mtx);
	return;
}

void dsscomp_refresh_syncpt_value (int display_num)
{
	struct fence_cdev *fence_cdev;
	if ((display_num < 0) ||
			(display_num >= VIRTUAL_DISPLAY))
		return;

	fence_cdev = &cdev->fence_cdev[display_num];

	mutex_lock(&fence_cdev->sync_mutex);
	/*refresh rel_sync_pt_value */
	fence_cdev->rel_sync_pt_value = fence_cdev->timeline_value +
					fence_cdev->sync_margin;
	mutex_unlock(&fence_cdev->sync_mutex);
}

void odin_dss_pm_qos_set (int mgr_ix, bool manual_update_mode, u32 mem_dfs)
{
	int i;
	struct odin_overlay_manager *mgr;
	int mgr_cnt = 0;
	int ovl_cnt = 0;
	u32 ovl_total_mask = 0;
	u32 interediate_mask = 0;

	for (i = 0; i < dss_feat_get_num_mgrs(); ++i) {
		mgr = odin_mgr_dss_get_overlay_manager(i);
		if (mgr->device->state == ODIN_DSS_DISPLAY_ACTIVE)
			mgr_cnt++;
	}

	for (i = 0; i < dss_feat_get_num_mgrs(); ++i)
	{
		interediate_mask = (mgrq[i].ovl_qmask.mask | mgrq[i].ovl_mask);
		ovl_total_mask |= interediate_mask;
	}

	while (ovl_total_mask) {
		u32 ix = fls(ovl_total_mask) - 1, m = 1 << ix;
		ovl_total_mask &= ~m;
		ovl_cnt++;
	}

	if (mgr_cnt > 2)  /* dual lcd */
	{
#ifdef MIPI_PKT_SIZE_CONTROL  /* Not setting */
#else
		current_min_freq = 800000;
		pm_qos_update_request(&dss_mem_qos, 800000);
#endif
	}
	else if (mgr_ix > 0)
	{
#ifdef MIPI_PKT_SIZE_CONTROL  /* Not setting */
#else
		/* DSSINFO("800 Mhz Setting time out 100ms \n "); */
		pm_qos_update_request_timeout(&sync1_mem_qos, 800000, 100000);
#endif
	}
	else
	{
		if (manual_update_mode) /* commnad mode */
		{
#ifdef MIPI_PKT_SIZE_CONTROL  /* Not setting */
#else
#ifdef DSS_REGION_CHECK_DFS_REQUEST_CLK
			pm_qos_update_request_timeout(&dss_mem_qos, mem_dfs, 100000);
#else
			pm_qos_update_request_timeout(&dss_mem_qos, 400000, 100000);
#endif
#endif
		}
		else /* video mode */
		{
			if ((ovl_cnt > 1) && (current_min_freq < 800000))
			{
				current_min_freq = 800000;
				pm_qos_update_request(&dss_mem_qos, 800000);
			}
			else if ((ovl_cnt == 1) && (current_min_freq > 0))/* > 200000)) */
			{
				current_min_freq = 0;/* 200000; */
				pm_qos_update_request(&dss_mem_qos, 0);
			}

		}
	}
}

/* apply composition */
/* at this point the composition is not on any queue */
#if FENCE_SYNC_LOG
int Q_num = 0;
#endif
int wait_vsync_before_setting_cnt = 0;
int dsscomp_apply(dsscomp_t comp)
{
	int i, rw, r2, r = -EFAULT;
	u32 dmask, wb_dmask, display_ix;
	struct odin_dss_device *dssdev;
	struct odin_dss_driver *drv;
	struct odin_overlay_manager *mgr;
	struct odin_overlay *ovl;
	struct dsscomp_setup_mgr_data *d;
	struct odin_writeback *wb = NULL;
	struct odin_writeback_info wb_info;
	u32 oix;
	u32 wix;
	bool cb_programmed = false;
	bool wb_apply = false;
	bool mem2mem_mode = 0;
	bool capture_mode = 0;
	bool manually_update_mode = 0;
	u32 clk_mask, clk_dmask;
	unsigned long timeout = msecs_to_jiffies(500);
	unsigned long flags;
	u32 vsync_after_time;
	bool scale_delay = false;
	bool vsync_ctrl_status;

	struct odindss_ovl_cb cb = {
		.fn = dsscomp_mgr_callback,
		.data = comp,
		.mask = DSS_COMPLETION_DISPLAYING |
		DSS_COMPLETION_PROGRAMMED,
	};

#if 0
	u32 mask = (mgrq[0].ovl_mask | mgrq[1].ovl_mask | mgrq[2].ovl_mask);
	while (mask) {
		u32 ix = fls(mask) - 1, m = 1 << ix;
		enabled_ovl_num++;
		mask &= ~m;
	}

	if (overlay_num_debug != enabled_ovl_num)
		DSSINFO("current overlay_num = %d \n", enabled_ovl_num);

	overlay_num_debug = enabled_ovl_num;
#endif

	BUG_ON(comp->state != DSSCOMP_STATE_APPLYING);

	/* check if the display is valid and used */
	r = -ENODEV;
	d = &comp->frm;
#if 0
	display_ix = d->mgr.ix;
#else
	display_ix = cdev->display_ix[d->mgr.ix];
#endif

	if (display_ix >= cdev->num_displays)
		goto done;
	dssdev = cdev->displays[display_ix];
	if (!dssdev)
		goto done;

	drv = dssdev->driver;
	mgr = dssdev->manager;
	if (!mgr || !drv || mgr->id >= cdev->num_mgrs)
		goto done;

	if (odin_crg_get_error_handling_status(mgr->id))
		goto done;

#if FENCE_SYNC_LOG
	struct fence_cdev *fence_cdev = (mgr->id == ODIN_DSS_CHANNEL_LCD0)?
				&cdev->fence_cdev[PRIMARY_DISPLAY] :
					&cdev->fence_cdev[EXTERNAL_DISPLAY];
	printk("4)Q(%d)\n",fence_cdev->timeline_value);
#endif

	if (d->mode & DSSCOMP_SETUP_MODE_MEM2MEM) /* mem2mem mode only */
		mem2mem_mode = true;
	else if (d->mode & DSSCOMP_SETUP_MODE_CAPTURE)
		capture_mode = true;

	r = 0;
	dmask = 0;
	comp->wb_dmask = 0;

#if 0
	manually_update_mode = dssdev_manually_updated(dssdev);
	comp->manual_update = manually_update_mode;
	comp->mem2mem_mode = mem2mem_mode;
#else
	manually_update_mode = comp->manual_update;
#endif


#if 1	/* return when display panel is dead */
	if (!mem2mem_mode &&
		(!mgr->device || (mgr->device->state != ODIN_DSS_DISPLAY_ACTIVE))) {
		DSSERR("cannot apply mgr(%s) on inactive device\n", mgr->name);
		r = -ENODEV;
		goto done;
	}
#endif

	for (wix = 0; wix < comp->frm.num_wbs; wix++) {
		struct dss_wb_info *wi = comp->wbs + wix;
		u64 wb_debug_addr1 = comp->wb_debug_addr1[wix];

		if (!comp->must_apply)
			continue;

		if (!wi->cfg.enabled)
			comp->wb_dmask |= (1 << wi->cfg.ix);
		else
			comp->wb_dmask &= ~(1 << wi->cfg.ix);

		wb_apply = true;

		wb = odin_dss_get_writeback(wi->cfg.ix);
		wb->get_wb_info(wb, &wb_info);

		if (mem2mem_mode) {
			if (wi->cfg.enabled)
				wb->initial_completion(wb);
		}

		r = set_dss_wb_info(wi, wb_debug_addr1, d->mode);
		break;
	}

	for (oix = 0; oix < comp->frm.num_ovls; oix++) {
		struct dss_ovl_info *oi = comp->ovls + oix;
		u64 debug_addr1 = comp->debug_addr1[oix];

		/* keep track of disabled overlays */
		if (!oi->cfg.enabled)
			dmask |= 1 << oi->cfg.ix;
		else if ((oi->cfg.ix <= ODIN_DSS_VID1) && (oi->cfg.enabled))
		{
			if (oi->cfg.win.y == 1)
				scale_delay = true;
		}

		if (r && !comp->must_apply)
			continue;

			/* dump_ovl_info(cdev, oi); */

		if (oi->cfg.ix >= cdev->num_ovls) {
			r = -EINVAL;
			continue;
		}

		ovl = cdev->ovls[oi->cfg.ix];

		/* set overlays' manager & info */
		if (ovl->info.enabled && ovl->manager != mgr) {
			r = -EBUSY;
			goto skip_ovl_set;
		}

		if (ovl->manager != mgr) {
			mutex_lock(&mtx);
			if (!mgrq[comp->ix].blanking || mem2mem_mode) {
				/*
				 * Ideally, we should call
				 * ovl->unset_manager(ovl),
				 * but it may block on go
				 * even though the disabling
				 * of the overlay already
				 * went through. So instead,
				 * we are just clearing the manager.
				 */
				ovl->manager = NULL;
				r = ovl->set_manager(ovl, mgr);
			} else	{
				/* Ignoring manager change
				during blanking. */
				pr_info_ratelimited("dsscomp_apply "
					"skip set_manager(%s) for "
					"ovl%d while blank."
					, mgr->name, oi->cfg.ix);
				r = -ENODEV;
			}
			mutex_unlock(&mtx);

			if (r)
				goto skip_ovl_set;
		}
		r = set_dss_ovl_info(oi, debug_addr1, d->mode);

skip_ovl_set:
		if (r && comp->must_apply) {
			if (r == -EFAULT)
				dev_err(DEV(cdev), "[%p]set ovl%d failed %d(ion_addr NULL)",
						comp, oi->cfg.ix, r);
			else
				dev_err(DEV(cdev), "[%p]set ovl%d failed %d", comp,
								oi->cfg.ix, r);
			oi->cfg.enabled = false;
			dmask |= 1 << oi->cfg.ix;
			r = set_dss_ovl_info(oi, debug_addr1, d->mode);
		}
	}

	/*
	 * set manager's info - this also sets the completion callback,
	 * so if it succeeds, we will use the callback to complete the
	 * composition.  Otherwise, we can skip the composition now.
	 */
	if (!r || comp->must_apply) {
		r = set_dss_mgr_info(&d->mgr, &cb, display_ix, d->mode);
		cb_programmed = r == 0;
	}

	if (r && !comp->must_apply) {
		dev_err(DEV(cdev), "[%p] set failed %d\n", comp, r);
		goto done;
	} else {
		if (r)
			dev_warn(DEV(cdev), "[%p] ignoring set failure %d\n",
								comp, r);
		comp->blank = dmask == comp->ovl_mask;
		comp->ovl_dmask = dmask;

		/*
		 * Check other overlays that may also use this display.
		 * NOTE: This is only needed in case someone changes
		 * overlays via sysfs.	We use comp->ovl_mask to refresh
		 * the overlays actually used on a manager when the
		 * composition is programmed.
		 */
		for (i = 0; i < cdev->num_ovls; i++) {
			u32 mask = 1 << i;
			if ((~comp->ovl_mask & mask) &&
				cdev->ovls[i]->info.enabled &&
				cdev->ovls[i]->manager == mgr) {
				mutex_lock(&mtx);
				comp->ovl_mask |= mask;
				maskref_incbit(&mgrq[comp->ix].ovl_qmask, i);
				mutex_unlock(&mtx);
			}
		}

		/* Check other writebacks */
		for (i = 0; i < cdev->num_wbs; i++) {
			u32 mask = 1 << i;
			if ((~comp->wb_mask & mask) &&
				cdev->wbs[i]->info.enabled &&
				cdev->wbs[i]->info.channel == mgr->id) {
				mutex_lock(&mtx);
				comp->wb_mask |= mask;
				maskref_incbit(&mgrq[comp->ix].wb_qmask, i);
				mutex_unlock(&mtx);
			}
		}

	}

	/* apply changes and call update on manual panels */
	/* no need for mutex as no callbacks are scheduled yet */
	comp->state = DSSCOMP_STATE_APPLIED;
	log_state(comp, dsscomp_apply, 0);

	if (!d->win.w && !d->win.x)
		d->win.w = dssdev->panel.timings.x_res - d->win.x;
	if (!d->win.h && !d->win.y)
		d->win.h = dssdev->panel.timings.y_res - d->win.y;

#if 0
	if (wb_apply) {
		struct odin_writeback_info wb_info;
		struct odin_writeback *wb;

		wb = odin_dss_get_writeback(0);
		wb->get_wb_info(wb, &wb_info);
	}
#endif
#if 1
	if ((!mem2mem_mode) && (!manually_update_mode))
	{
		u32 du_vsync_after_time = odin_du_vsync_after_time();
		if (du_vsync_after_time > 14000) {
#ifdef DSS_TIME_CHECK_LOG
			wait_vsync_before_setting_cnt++;
#endif
			r = mgr->wait_for_vsync(mgr);
		}
	}
#endif
	if (mem2mem_mode)
		odin_dss_pm_qos_set(mgr->id, manually_update_mode, comp->mem_dfs);

	mutex_lock(&mtx);

	/* current-frame mask & dmask */
	clk_mask = (comp->ovl_mask | comp->ovl_dmask);
	clk_dmask = ((mgrq[comp->ix].ovl_dmask) & (~clk_mask)); /* pre-frame dmask */

	/* printk("ovl_m:%x, ovl_dm:%x, mgrq[%d].ovl_m:%x, mgrq[%d].ovl_dm:%x, wb_m:%x, wb_dm:%x, wb_apply:%d, mem2memmode=%d\n",
		comp->ovl_mask, comp->ovl_dmask, comp->ix, mgrq[comp->ix].ovl_mask,
		comp->ix, mgrq[comp->ix].ovl_dmask, comp->wb_mask, comp->wb_dmask, wb_apply, mem2mem_mode); */

	if ((mgrq[comp->ix].ovl_mask != clk_mask) || (mem2mem_mode))
		r = mgr->clk_enable(clk_mask, 0, mem2mem_mode);

#if RUNTIME_CLK_GATING
	/* command mode panel or WB */
	if (((!mem2mem_mode) && (manually_update_mode))
			|| mem2mem_mode)
		enable_runtime_clk (mgr, mem2mem_mode);
#ifdef RUNTIME_CLK_GATING_LOG
	if (mem2mem_mode)
		wb_status = true;
#endif
#endif

	if (mgrq[comp->ix].blanking && !mem2mem_mode) {
		pr_info_ratelimited("ignoring apply mgr(%s) while blanking\n",
								mgr->name);
		r = -ENODEV;
	} else {
		if (wb_apply) {
			r = odin_dss_wb_apply(mgr);
			if (r)
				dev_err(DEV(cdev),
					"odin_dss_wb_apply failed %d", r);
		}
		r = mgr->apply(mgr, d->mode);
		if (r)
		{
			if (mgr->device->state == ODIN_DSS_DISPLAY_ACTIVE)
				dev_err(DEV(cdev),
						"failed while applying mgr[%d] r:%d\n",
									mgr->id, r);
			else
			{
				mutex_unlock(&mtx);
				r = -EINVAL;
				goto err_done2;
			}
		}
		/* keep error if set_mgr_info failed */
		if (!r && !cb_programmed && !capture_mode  && !mem2mem_mode)
			r = -EINVAL;
	}

	mutex_unlock(&mtx);

	/* if failed to apply, kick out prior composition */
	if (comp->must_apply && r)
	{
		mgr->blank(mgr, true);
		DSSDBG("failed to apply, blank \n");
	}

	if ((!mem2mem_mode) && (manually_update_mode))
	{
		/* mgr->wait_for_vsync(mgr); */
#if 0
		r = mgr->wait_for_framedone(mgr, timeout);
		if (r < 0)
			DSSERR("dsscomp_apply framedone timeout error\n");
#endif

		r = drv->flush_pending(mgr->device);
		if (r < 0)
		{
			DSSERR("dsscomp_apply flush_pending1 error\n");
			goto err_done2;
		}
	}

#ifdef DSS_TIME_CHECK_LOG
	odin_mgr_dss_time_start(ODIN_CMD_QOS_LOCK_TIME);
#endif

	odin_dss_pm_qos_set(mgr->id, manually_update_mode, comp->mem_dfs);

#ifdef DSS_TIME_CHECK_LOG
	if (check_time == ODIN_CMD_QOS_LOCK_TIME)
		DSSINFO("check_time:%d, elapsed time = %lld",
			check_time, odin_mgr_dss_time_end(ODIN_CMD_QOS_LOCK_TIME));
#endif

	if ((d->mode & (DSSCOMP_SETUP_MODE_DISPLAY |
					DSSCOMP_SETUP_MODE_DISPLAY_MEM2MEM)) && !mem2mem_mode) {
		odin_crg_underrun_set_info(mgr->id, comp->ovl_mask, comp->mem_dfs);
		if (manually_update_mode && drv->update) {
#ifdef DSS_DFS_MUTEX_LOCK
			mutex_lock(&dfs_mtx);
#elif defined(DSS_DFS_SPINLOCK)
			get_mem_dfs();
#endif
			/*update*/
#if 0
			r = drv->update(dssdev, d->win.x,
					d->win.y, d->win.w, d->win.h);
			if (r) {
				/* if failed to update, kick out
				 * prior composition
				 */
				mgr->blank(mgr, false);
				/* clear error as no need to
				 * handle error state now
				 */
				r = 0;
			}
#else
			drv->sync(dssdev);
			if (scale_delay)
				udelay(50);

			/* update */
			vsync_after_time = odin_mipi_dsi_vsync_after_time();
			if ( vsync_after_time < 11500)
				r = drv->update(dssdev, d->win.x,
						d->win.y, d->win.w, d->win.h);
			else
			{
				atomic_set(&vsync_ready, 1);
				if (!(vsync_ctrl_status = mipi_dsi_vsync_ctrl_status(dssdev)))
					mipi_dsi_vsync_enable(dssdev);
			}

#endif
		}
	}

	if ((!mem2mem_mode) && (manually_update_mode))
	{
		/* mgr->wait_for_vsync(mgr); */
		r2 = mgr->wait_for_framedone(mgr, timeout);
		if (r2 < 0) {
			DSSERR("dsscomp_apply framedone timeout error, direct update in wq? (%d, %dus)\n",
					(bool)(vsync_after_time < 11500), vsync_after_time);
			/* if not direct update in wq, update in vsync isr,
			   we check the vsync irq status.*/
			if (!(vsync_after_time < 11500))
				printk("(dsi_dev.irq_enabled:%d, mgr->vsync_ctrl:%d)\n",
						mipi_dsi_get_vsync_irq_status(), vsync_ctrl_status);

		}

		r = drv->flush_pending(mgr->device);
		if (r < 0)
		{
			DSSERR("dsscomp_apply flush_pending2 error\n");
			goto err_done;
		}

		if (r2 < 0) {
			r = r2;
			goto err_done;
		}
	}
#if 0
	else if ((!mem2mem_mode) && (!manually_update_mode))
		mgr->wait_for_vsync(mgr);
#endif

	if (!mem2mem_mode) {
		r = mgr->clk_enable(0, clk_dmask, mem2mem_mode);
	}
	else
	{
		if (!comp->wb_dmask)
		{
			if ((wb) && (rw = wb->wait_framedone(wb)))
			{
				dev_warn(DEV(cdev),
					"WB Framedone expired:%d\n", rw);
				DSSINFO("core_clk = %x, other_clk = %x \n", odin_crg_get_dss_clk_ena(CRG_CORE_CLK), odin_crg_get_dss_clk_ena(CRG_OTHER_CLK));
				struct odin_overlay_info info;
				ovl = odin_ovl_dss_get_overlay(wb->id);

				/* just in case there are new fields, we get the current info */
				ovl->get_overlay_info(ovl, &info);
				DSSINFO("enabled:%x, width:%d, height:%d. color_mode:%d \n", info.enabled, info.width, info.height, info.color_mode, odin_crg_get_dss_clk_ena(CRG_OTHER_CLK));
				DSSINFO("cropx:%d, cropy:%d, cropw:%d, cropw:%d \n", info.crop_x, info.crop_y, info.crop_w, info.crop_h);
				DSSINFO("outx:%d, outy:%d, outw:%d. outh:%d ,clk_mask:%x \n", info.out_x, info.out_y, info.out_w, info.out_h, clk_mask);
			}
		}

		wb_dmask = comp->wb_dmask;
		if (comp->wb_dmask)
		{
			r = mgr->clk_enable(0, wb_dmask, true);
#ifdef RUNTIME_CLK_GATING_LOG
			wb_status = false;
#endif
		}
	}

	odin_dss_mgr_set_state_irq(mgr, d->mode);

err_done:
	if ((!mem2mem_mode) && manually_update_mode) {
#ifdef DSS_DFS_MUTEX_LOCK
		mutex_unlock(&dfs_mtx);
#elif defined(DSS_DFS_SPINLOCK)
		put_mem_dfs();
#endif
	}

err_done2:
#if RUNTIME_CLK_GATING
	if ((!mem2mem_mode) && manually_update_mode)
		disable_runtime_clk (mgr->id);
#endif
done:
	return r;
}

struct dsscomp_apply_work {
	struct work_struct work;
	dsscomp_t comp;
};

int dsscomp_state_notifier(struct notifier_block *nb,
						unsigned long arg, void *ptr)
{
	struct odin_dss_device *dssdev = ptr;
	enum odin_dss_display_state state = arg;
	struct odin_overlay_manager *mgr = dssdev->manager;
	if (mgr) {
		mutex_lock(&mtx);
		if (state == ODIN_DSS_DISPLAY_DISABLED) {
			mgr->blank(mgr, true);
			mgrq[mgr->id].blanking = true;
		} else if (state == ODIN_DSS_DISPLAY_ACTIVE) {
			mgrq[mgr->id].blanking = false;
		}
		mutex_unlock(&mtx);
	}
	return 0;
}

int dsscomp_cabc(struct odin_overlay_manager *mgr, bool cabc_en)
{
	mgr->cabc(mgr, cabc_en);

	return 0;
}

int dsscomp_unplug_state_reset(void *ptr)
{
	struct odin_dss_device *dssdev = ptr;
	struct odin_overlay_manager *mgr = dssdev->manager;
	if (mgr) {
		mgr->blank(mgr, true);
		mutex_lock(&mtx);
		maskref_reset(&mgrq[mgr->id].ovl_qmask, 0x7f);
		maskref_reset(&mgrq[mgr->id].wb_qmask, 0x7f);
		mgrq[mgr->id].ovl_mask = 0;
		mgrq[mgr->id].wb_mask = 0;
		DSSINFO("dsscomp_unplug_state_reset \n");
		mutex_unlock(&mtx);
	}
	return 0;
}

static void dsscomp_do_apply(struct work_struct *work)
{
	struct dsscomp_apply_work *wk = container_of(work, typeof(*wk), work);

	/* complete compositions that failed to apply */
	if (dsscomp_apply(wk->comp))
		dsscomp_mgr_callback(wk->comp, -1, DSS_COMPLETION_PROGRAM_ERROR);

	kfree(wk);
}

static void dsscomp_do_cabc(struct work_struct *work)
{
	struct dsscomp_apply_work *wk = container_of(work, typeof(*wk), work);
	u32 mgr_ix = wk->comp->ix;
	bool cabc_en = wk->comp->cabc_en;
	struct odin_overlay_manager *mgr =
		odin_mgr_dss_get_overlay_manager(mgr_ix);
	dsscomp_cabc(mgr, cabc_en);

	kfree(wk);
}

int dsscomp_delayed_apply(dsscomp_t comp)
{
	int r = 0;
	/* don't block in case we are called from interrupt context */
	struct dsscomp_apply_work *wk = kzalloc(sizeof(*wk), GFP_NOWAIT);
	if (!wk)
		return -ENOMEM;

	mutex_lock(&mtx);

	BUG_ON(comp->state != DSSCOMP_STATE_ACTIVE);
	comp->state = DSSCOMP_STATE_APPLYING;
	log_state(comp, dsscomp_delayed_apply, 0);

	if (debug & DEBUG_PHASES)
		dev_info(DEV(cdev), "[%p] applying\n", comp);
	mutex_unlock(&mtx);

	wk->comp = comp;
	INIT_WORK(&wk->work, dsscomp_do_apply);
	if (comp->frm.mode & DSSCOMP_SETUP_MODE_CAPTURE)
		r = queue_work(capture_wkq, &wk->work) ? 0 : -EBUSY;
	else
		r = queue_work(mgrq[comp->ix].apply_workq, &wk->work) ? 0 : -EBUSY;

#ifdef DSS_VIDEO_PLAYBACK_CABC
	if ((odin_cabc_is_on()) && (comp->ix == 0))
	{
		if ((mgrq[comp->ix].cabc_en) && (comp->ix == 0))
		{
			struct dsscomp_apply_work *wk_cabc = kzalloc(sizeof(*wk), GFP_NOWAIT);
			wk_cabc->comp = comp;
			INIT_WORK(&wk_cabc->work, dsscomp_do_cabc);
			r = queue_work(cabc_wkq, &wk_cabc->work) ? 0 : -EBUSY;
		}
		mgrq[comp->ix].cabc_en = comp->cabc_en;
	}
#endif

	return r;
}
EXPORT_SYMBOL(dsscomp_delayed_apply);

int dsscomp_direct_apply(dsscomp_t comp)
{
	int r = 0;
	mutex_lock(&mtx);

	BUG_ON(comp->state != DSSCOMP_STATE_ACTIVE);
	comp->state = DSSCOMP_STATE_APPLYING;
	log_state(comp, dsscomp_direct_apply, 0);
	if (debug & DEBUG_PHASES)
		dev_info(DEV(cdev), "[%p] applying\n", comp);
	mutex_unlock(&mtx);

	/* complete compositions that failed to apply */
	if (dsscomp_apply(comp))
		dsscomp_mgr_callback(comp, -1, DSS_COMPLETION_PROGRAM_ERROR);

	return r;
}
EXPORT_SYMBOL(dsscomp_direct_apply);

int dsscomp_complete_workqueue(struct odin_dss_device *dssdev, bool wb_pending_wait, bool release)
{
	flush_workqueue(mgrq[dssdev->manager->id].apply_workq);

	/* if you goes to be suspend states */
	if (dssdev_manually_updated(dssdev) && wb_pending_wait)
		dsscomp_is_wb_pending_wait();


	/* if you need to release pre buff */
	if (release)
	{
#ifdef DSSCOMP_FENCE_SYNC
		if (dssdev->channel == ODIN_DSS_CHANNEL_LCD0)
			dsscomp_mgr_callback(NULL, PRIMARY_DISPLAY,
				DSS_COMPLETION_FENCE_RELEASE);
		else
			dsscomp_mgr_callback(NULL, EXTERNAL_DISPLAY,
				DSS_COMPLETION_FENCE_RELEASE);
#endif
	}
	return 0;
}


int dsscomp_get_pipe_status(struct dsscomp_get_pipe_status *ptr_pipe_status)
{
	int i;
	for (i = 0; i < MAX_MANAGERS; i++)
		ptr_pipe_status->mgr_pipe[i] = mgrq[i].ovl_mask;

	return 0;
}

/*
 * ===========================================================================
 *		DEBUGFS
 * ===========================================================================
 */

#ifdef CONFIG_DEBUG_FS
void seq_print_comp(struct seq_file *s, dsscomp_t c)
{
	struct dsscomp_setup_mgr_data *d = &c->frm;
	int i;

	seq_printf(s, "  [%p]: %s%s\n", c, c->blank ? "blank " : "",
		   c->state == DSSCOMP_STATE_ACTIVE ? "ACTIVE" :
		   c->state == DSSCOMP_STATE_APPLYING ? "APPLYING" :
		   c->state == DSSCOMP_STATE_APPLIED ? "APPLIED" :
		   c->state == DSSCOMP_STATE_PROGRAMMED ? "PROGRAMMED" :
		   c->state == DSSCOMP_STATE_DISPLAYED ? "DISPLAYED" :
		   "???");
	seq_printf(s, "    sync_id=%x, flags=%c%c%c\n",
		   d->sync_id,
		   (d->mode & DSSCOMP_SETUP_MODE_DISPLAY) ? 'D' : '-',
		   (d->mode & DSSCOMP_SETUP_MODE_CAPTURE) ? 'C' : '-',
   		   (d->mode & DSSCOMP_SETUP_MODE_MEM2MEM) ? 'M' : '-');
	for (i = 0; i < d->num_ovls; i++) {
		struct dss_ovl_info *oi;
		struct dss_ovl_cfg *g;
		oi = d->ovls + i;
		g = &oi->cfg;
		if (g->zonly) {
			seq_printf(s, "    ovl%d={%s z%d}\n",
				   g->ix, g->enabled ? "ON" : "off", g->zorder);
		} else {
			seq_printf(s, "    ovl%d={%s z%d %s%s *%d%%"
						" %d*%d:%d,%d+%d,%d rot%d%s"
						" => %d,%d+%d,%d |%d}\n",
				g->ix, g->enabled ? "ON" : "off", g->zorder,
				/* dsscomp_get_color_name(g->color_mode) ? : "N/A", */
				"N/A",
				g->pre_mult_alpha ? " premult" : "",
				(g->global_alpha * 100 + 128) / 255,
				g->width, g->height, g->crop.x, g->crop.y,
				g->crop.w, g->crop.h,
				g->rotation, g->mirror ? "+mir" : "",
				g->win.x, g->win.y, g->win.w, g->win.h,
				g->stride);
		}
	}
	if (c->extra_cb)
		seq_printf(s, "    gsync=[%p] %pf\n\n", c->extra_cb_data,
								c->extra_cb);
	else
		seq_printf(s, "    gsync=[%p] (called)\n\n", c->extra_cb_data);
}
#endif

void dsscomp_dbg_comps(struct seq_file *s)
{
#ifdef CONFIG_DEBUG_FS
	dsscomp_t c;
	u32 i;
	u32 display_ix;

	mutex_lock(&dbg_mtx);
	for (i = 0; i < cdev->num_mgrs; i++) {
		struct odin_overlay_manager *mgr = cdev->mgrs[i];
		seq_printf(s, "ACTIVE COMPOSITIONS on %s\n\n", mgr->name);
		list_for_each_entry(c, &dbg_comps, dbg_q) {
			struct dss_mgr_info *mi = &c->frm.mgr;
			display_ix = cdev->display_ix[mi->ix];
			if (display_ix < cdev->num_displays &&
			    cdev->displays[display_ix]->manager == mgr)
				seq_print_comp(s, c);
		}

		/* print manager cache */
		mgr->dump_cb(mgr, s);
	}
	mutex_unlock(&dbg_mtx);
#endif
}

void dsscomp_dbg_events(struct seq_file *s)
{
#ifdef CONFIG_DSSCOMP_DEBUG_LOG
	u32 i;
	struct dbg_event_t *d;

	mutex_lock(&dbg_mtx);
	for (i = dbg_event_ix; i < dbg_event_ix + ARRAY_SIZE(dbg_events); i++) {
		d = dbg_events + (i % ARRAY_SIZE(dbg_events));
		if (!d->ms)
			continue;
		seq_printf(s, "[% 5d.%03d] %*s[%08x] ",
			   d->ms / 1000, d->ms % 1000,
			   d->ix + ((u32) d->data) % 7,
			   "", (u32) d->data);
		seq_printf(s, d->fmt, d->a1, d->a2);
		seq_printf(s, "\n");
	}
	mutex_unlock(&dbg_mtx);
#endif
}

/*
 * ===========================================================================
 *		EXIT
 * ===========================================================================
 */
void dsscomp_queue_exit(void)
{
	if (cdev) {
		int i;
		for (i = 0; i < cdev->num_displays; i++)
			destroy_workqueue(mgrq[i].apply_workq);
		destroy_workqueue(cb_wkq);
		cdev = NULL;
	}
}
EXPORT_SYMBOL(dsscomp_queue_exit);
