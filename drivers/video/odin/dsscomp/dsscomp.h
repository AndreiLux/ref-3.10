/*
 * linux/drivers/video/odin/dsscomp/dsscomp.h
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

#ifndef _DSSCOMP_H
#define _DSSCOMP_H

#include <linux/miscdevice.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#ifdef CONFIG_DSSCOMP_DEBUG_LOG
#include <linux/hrtimer.h>
#endif
#include <video/odin-overlay.h>
#include "../dss/dss.h"

#define DEBUG_OVERLAYS          (1 << 0)
#define DEBUG_COMPOSITIONS      (1 << 1)
#define DEBUG_PHASES            (1 << 2)
#define DEBUG_WAITS             (1 << 3)
#define DEBUG_GRALLOC_PHASES    (1 << 4)

#define DSSCOMP_MMAP_SIZE	0x00180000*10 /*0x00A80000 */ /* 0x0A000000 */
#ifdef CONFIG_ODIN_DSS_FPGA
#define DSSCOMP_MMAP_PHYS	0x84000000 /* 0x9C600000 */ /* 0x84000000 */
#else
#define DSSCOMP_MMAP_PHYS	0x9F000000 /* 0x9C600000 */ /*0x84000000 */
#endif

/*
 * Utility macros
 */
#define ZERO(c)         memset(&c, 0, sizeof(c))
#define ZEROn(c, n)     memset(c, 0, sizeof(*c) * n)
#define DEV(c)          (c->dev.this_device)

/**
 * DSS Composition Device Driver
 *
 * @dev:   misc device base
 * @dbgfs: debugfs hook
 */
struct fence_cdev {
#ifdef DSSCOMP_FENCE_SYNC
	struct mutex sync_mutex;
	struct sw_sync_timeline *timeline;
	int timeline_value;

	u32 acq_fen_cnt;
	struct sync_fence *acq_fen[DSSCOMP_MAX_FENCE_FD];

	int cur_rel_fen_fd;
	struct sync_fence *cur_rel_fence;
	struct sync_pt *cur_rel_sync_pt;
	struct sync_fence *last_rel_fence;
	int rel_sync_pt_value;
	int sync_margin;

	/* for non-blocking */
	u32 is_committing;
	struct completion commit_comp;
	struct work_struct commit_work;
	void *cdev_backup;

#endif
};

enum {
    PRIMARY_DISPLAY     = 0,
    EXTERNAL_DISPLAY    = 1,    /* HDMI, DP, etc.*/
#if 0
    NUM_DISPLAY_TYPES	= 2,	/* it should be changed to 3
				   for VIRTUAL_DISPLAY later. */
#else
    VIRTUAL_DISPLAY     = 2,

    NUM_PHYSICAL_DISPLAY_TYPES = 2,
    NUM_DISPLAY_TYPES          = 3,
#endif
};

struct dsscomp_dev {
	struct miscdevice dev;
	struct dentry *dbgfs;

    	/* cached DSS objects */
    	u32 num_ovls;
    	struct odin_overlay *ovls[MAX_OVERLAYS];
    	u32 num_wbs;
	struct odin_writeback *wbs[MAX_WRITEBACKS];
	u32 num_mgrs;
	struct odin_overlay_manager *mgrs[MAX_MANAGERS];
	u32 num_displays;
	struct odin_dss_device *displays[MAX_DISPLAYS];
	u32 display_ix[MAX_MANAGERS]; /* display idx per manager */
	struct notifier_block state_notifiers[MAX_DISPLAYS];

	/* primary & external & virtual = 3 */
	struct fence_cdev fence_cdev[NUM_DISPLAY_TYPES];
};
#ifdef DSSCOMP_FENCE_SYNC
struct dsscomp_backup_type {
	/*struct fb_info info;*/
	struct dsscomp_display_commit disp_commit;
};
#endif

extern int debug;

#ifdef CONFIG_DEBUG_FS
extern struct mutex dbg_mtx;
extern struct list_head dbg_comps;
#define DO_IF_DEBUG_FS(cmd) {   \
    mutex_lock(&dbg_mtx);   \
    cmd;                    \
    mutex_unlock(&dbg_mtx); \
}
#else
#define DO_IF_DEBUG_FS(cmd)
#endif

/* queuing operations */
typedef struct dsscomp_data *dsscomp_t;		/* handle */

enum dsscomp_state {
        DSSCOMP_STATE_ACTIVE            = 0xAC54156E,
        DSSCOMP_STATE_APPLYING          = 0xB554C591,
        DSSCOMP_STATE_APPLIED           = 0xB60504C1,
        DSSCOMP_STATE_PROGRAMMED        = 0xC0520652,
        DSSCOMP_STATE_DISPLAYED         = 0xD15504CA,
};

struct dsscomp_data {
        enum dsscomp_state state;
    /*
     * :TRICKY: before applying, overlays used in a composition are stored
     * in ovl_mask and the other masks are empty.  Once composition is
     * applied, blank is set to see if all overlays are to be disabled on
     * this composition, any disabled overlays in the composition are set in
     * ovl_dmask, and ovl_mask is updated to include ALL overlays that are
     * actually on the display - even if they are not part of the
     * composition. The reason: we use ovl_mask to see if an overlay is used
     * or planned to be used on a manager.  We update ovl_mask when
     * composition is programmed (removing the disabled overlays).
     */
        bool blank;             /* true if all overlays are to be disabled */
        u32 ovl_mask;           /* overlays used on this frame */
        u32 ovl_dmask;          /* overlays disabled on this frame */
        u32 ix;                 /* manager index that this frame is on */
        u32 wb_mask;            /* writebacks used on this frame */
        u32 wb_dmask;           /* writebacks disabled on this frame */
        struct dsscomp_setup_mgr_data frm;
        struct dss_ovl_info ovls[7];
	u64 debug_addr1[7]; /* Physical Address For Debug */
        struct dss_wb_info wbs[3];
	u64 wb_debug_addr1[3]; /* Physical Address For Debug */
        void (*extra_cb)(void *data, int status);
        void *extra_cb_data;
        bool must_apply;        /* whether composition must be applied */
	int manual_update;
	bool mem2mem_mode;
        bool cabc_en;
        u32 mem_dfs;

	struct ion_handle *imported_hnd[7];
	int imported_cnt;

	struct ion_handle *imported_hnd_backup[7];
	int imported_cnt_backup;

#ifdef CONFIG_DEBUG_FS
        struct list_head dbg_q;
        u32 dbg_used;
        struct {
                u32 t, state;
        } dbg_log[8];
#endif
};

struct dsscomp_sync_obj {
        int state;
        int fd;
        atomic_t refs;
};


/*
 * Kernel interface
 */
int dsscomp_queue_init(struct dsscomp_dev *cdev);
void dsscomp_queue_exit(void);
void dsscomp_gralloc_init(struct dsscomp_dev *cdev);
void dsscomp_gralloc_exit(void);
int dsscomp_gralloc_queue_ioctl(struct dsscomp_setup_du_data *d);
int dsscomp_wait(struct dsscomp_sync_obj *sync,
	enum dsscomp_wait_phase phase, int timeout);
int dsscomp_state_notifier(struct notifier_block *nb,
                                                unsigned long arg, void *ptr);

dsscomp_t dsscomp_new(struct odin_overlay_manager *mgr);
u32 dsscomp_get_ovls(dsscomp_t comp);
int dsscomp_set_ovl(dsscomp_t comp, struct dss_ovl_info *ovl,
	u64 debug_addr1);
int dsscomp_get_ovl(dsscomp_t comp, u32 ix, struct dss_ovl_info *ovl);
int dsscomp_set_wb(dsscomp_t comp, struct dss_wb_info *wb,
	u64 debug_addr1);
int dsscomp_set_mgr(dsscomp_t comp, struct dss_mgr_info *mgr);
int dsscomp_get_mgr(dsscomp_t comp, struct dss_mgr_info *mgr);
int dsscomp_setup(dsscomp_t comp, enum dsscomp_setup_mode mode,
			struct dss_rect_t win);
int dsscomp_delayed_apply(dsscomp_t comp);
int dsscomp_direct_apply(dsscomp_t comp);
void dsscomp_drop(dsscomp_t c);

int dsscomp_gralloc_queue(struct dsscomp_setup_du_data *d,
			bool early_callback,
			void (*cb_fn)(void *, int), void *cb_arg);
bool dsscomp_is_wb_pending_wait(void);
int dsscomp_wait_for_vsync(struct dsscomp_wait_for_vsync_data *vsync);
int dsscomp_vsync_ctrl(struct dsscomp_vsync_ctrl_data *vsync_ctrl);
int dsscomp_wait_for_wb_done(int wb_ix);
#ifdef LCD_ESD_CHECK
int dsscomp_get_esd_check(void);
#endif

/* basic operation - if not using queues */
int set_dss_ovl_info(struct dss_ovl_info *oi, u64 debug_addr1,
	enum dsscomp_setup_mode mode);
int set_dss_wb_info(struct dss_wb_info *oi, u64 debug_addr1,
	enum dsscomp_setup_mode mode);
int set_dss_mgr_info(struct dss_mgr_info *mi, struct odindss_ovl_cb *cb,
	u32 display_ix, enum dsscomp_setup_mode mode);
int odin_dss_wb_apply(struct odin_overlay_manager *mgr);
int odin_dss_mgr_set_manual_state_irq(struct odin_overlay_manager *mgr);
int odin_dss_mgr_set_state_irq(struct odin_overlay_manager *mgr,
	enum dsscomp_setup_mode mode);
struct odin_overlay_manager *find_dss_mgr(int display_ix);
void swap_rb_in_ovl_info(struct dss_ovl_info *oi);
void swap_rb_in_mgr_info(struct dss_mgr_info *mi);

/*
 * Debug functions
 */
void dump_ovl_info(struct dsscomp_dev *cdev, struct dss_ovl_info *oi);
void dump_comp_info(struct dsscomp_dev *cdev, struct dsscomp_setup_mgr_data *d,
                                const char *phase);
#if 0
void dump_total_comp_info(struct dsscomp_dev *cdev,
                                struct dsscomp_setup_du *d,
                                const char *phase);
#endif
const char *dsscomp_get_color_name(enum odin_color_mode m);

void dsscomp_dbg_comps(struct seq_file *s);
void dsscomp_dbg_gralloc(struct seq_file *s);
#ifdef DSSCOMP_FENCE_SYNC
int dsscomp_wait_for_fence (struct fence_cdev *fence_cdev);
void dsscomp_release_pre_buff(struct fence_cdev *fence_cdev);
void dsscomp_release_all_active_buff(struct fence_cdev *fence_cdev);
#endif
int dsscomp_get_pipe_status(struct dsscomp_get_pipe_status *ptr_pipe_status);

#define log_state_str(s) (\
        (s) == DSSCOMP_STATE_ACTIVE             ? "ACTIVE"      : \
        (s) == DSSCOMP_STATE_APPLYING           ? "APPLY'N"     : \
        (s) == DSSCOMP_STATE_APPLIED            ? "APPLIED"     : \
        (s) == DSSCOMP_STATE_PROGRAMMED         ? "PROGR'D"     : \
        (s) == DSSCOMP_STATE_DISPLAYED          ? "DISPL'D"     : "INVALID")

#define log_status_str(ev) ( \
        ((ev) & DSS_COMPLETION_DISPLAYING)      ? "DISPLAYING"     : \
        (ev) == DSS_COMPLETION_PROGRAMMED       ? "PROGRAMMED"  : \
        ((ev) & DSS_COMPLETION_PROGRAM_ERROR)	? "PROG_ERROR"    : "???")

#ifdef CONFIG_DSSCOMP_DEBUG_LOG
extern struct dbg_event_t {
        u32 ms, a1, a2, ix;
        void *data;
        const char *fmt;
} dbg_events[128];
extern u32 dbg_event_ix;

void dsscomp_dbg_events(struct seq_file *s);
#endif

static inline
void __log_event(u32 ix, u32 ms, void *data, const char *fmt, u32 a1, u32 a2)
{
#ifdef CONFIG_DSSCOMP_DEBUG_LOG
        if (!ms)
                ms = ktime_to_ms(ktime_get());
        dbg_events[dbg_event_ix].ms = ms;
        dbg_events[dbg_event_ix].data = data;
        dbg_events[dbg_event_ix].fmt = fmt;
        dbg_events[dbg_event_ix].a1 = a1;
        dbg_events[dbg_event_ix].a2 = a2;
        dbg_events[dbg_event_ix].ix = ix;
        dbg_event_ix = (dbg_event_ix + 1) % ARRAY_SIZE(dbg_events);
#endif
}

#define log_event(ix, ms, data, fmt, a1, a2) \
        DO_IF_DEBUG_FS(__log_event(ix, ms, data, fmt, a1, a2))

#endif
