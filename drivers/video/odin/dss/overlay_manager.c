/*
 * /drivers/video/odin/dss/overlay_manager.c
 *
 * Copyright (C) 2012 LG Electronics
 *
 * Copyright (C) 2009 Texas Instruments
 * Some code and ideas taken from drivers/video/omap2/dss
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

#define DSS_SUBSYS_NAME "OVERLAY_MANAGER"
#define BUFSYNC_DEBUGGING 0

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/pm_qos.h>
#include <linux/odin_dfs.h>

#include <video/odindss.h>
#include <linux/seq_file.h>
#include <linux/ratelimit.h>
#include <linux/ion.h>
#include <linux/clk.h>
#include <linux/clk-private.h>

#include "dss.h"
#include "cabc/cabc.h"
#include "dss-features.h"
#include "mipi-dsi.h"
#if BUFSYNC_DEBUGGING
#include "../odinfb/odinfb.h"
#endif

DEFINE_MUTEX(dfs_mtx);

static int num_managers;
static struct list_head manager_list;
static struct odin_overlay_manager *mgrs[MAX_DSS_MANAGERS];
struct pm_qos_request dss_mem_qos;
struct pm_qos_request sync1_mem_qos;
struct pm_qos_request sync2_mem_qos;
#ifdef DSS_TIME_CHECK_LOG
static ktime_t start_time_info[ODIN_MAX_TIME_NUM];
#endif

static ssize_t manager_name_show(struct odin_overlay_manager *mgr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", mgr->name);
}

static ssize_t manager_display_show(struct odin_overlay_manager *mgr,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n",
			mgr->device ? mgr->device->name : "<none>");
}

static ssize_t manager_display_store(struct odin_overlay_manager *mgr,
		const char *buf, size_t size)
{
	int r = 0;
	size_t len = size;
	struct odin_dss_device *dssdev = NULL;

	int match(struct odin_dss_device *dssdev, void *data)
	{
		const char *str = data;
		return sysfs_streq(dssdev->name, str);
	}

	if (buf[size-1] == '\n')
		--len;

	if (len > 0)
		dssdev = odin_dss_find_device((void *)buf, match);

	if (len > 0 && dssdev == NULL)
		return -EINVAL;

	if (dssdev)
		DSSDBG("display %s found\n", dssdev->name);

	if (mgr->device) {
		r = mgr->unset_device(mgr);
		if (r) {
			DSSERR("failed to unset display\n");
			goto put_device;
		}
	}

	if (dssdev) {
		r = mgr->set_device(mgr, dssdev);
		if (r) {
			DSSERR("failed to set manager\n");
			goto put_device;
		}

		r = mgr->apply(mgr, DSSCOMP_SETUP_MODE_DISPLAY);
		if (r) {
			DSSERR("failed to apply du config\n");
			goto put_device;
		}
	}

put_device:

#if 0
	if (dssdev)
		odin_dss_put_device(dssdev);
#endif
	return r ? r : size;
}

static ssize_t manager_default_color_show(struct odin_overlay_manager *mgr,
					  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", mgr->info.default_color);
}

static ssize_t manager_default_color_store(struct odin_overlay_manager *mgr,
					   const char *buf, size_t size)
{
	struct odin_overlay_manager_info info;
	u32 color;
	int r;

	r = kstrtouint(buf, 0, &color);
	if (r)
		return r;

	mgr->get_manager_info(mgr, &info);

	info.default_color = color;

	r = mgr->set_manager_info(mgr, &info);
	if (r)
		return r;

	r = mgr->apply(mgr, DSSCOMP_SETUP_MODE_DISPLAY);
	if (r)
		return r;

	return size;
}

static const char *trans_key_type_str[] = {
	"gfx-destination",
	"video-source",
};

static ssize_t manager_trans_key_type_show(struct odin_overlay_manager *mgr,
					   char *buf)
{
	enum odin_dss_trans_key_type key_type;

	key_type = mgr->info.trans_key_type;
	BUG_ON(key_type >= ARRAY_SIZE(trans_key_type_str));

	return snprintf(buf, PAGE_SIZE, "%s\n", trans_key_type_str[key_type]);
}

static ssize_t manager_trans_key_type_store(struct odin_overlay_manager *mgr,
					    const char *buf, size_t size)
{
	enum odin_dss_trans_key_type key_type;
	struct odin_overlay_manager_info info;
	int r;

	for (key_type = ODIN_DSS_COLOR_KEY_GFX_DST;
			key_type < ARRAY_SIZE(trans_key_type_str); key_type++) {
		if (sysfs_streq(buf, trans_key_type_str[key_type]))
			break;
	}

	if (key_type == ARRAY_SIZE(trans_key_type_str))
		return -EINVAL;

	mgr->get_manager_info(mgr, &info);

	info.trans_key_type = key_type;

	r = mgr->set_manager_info(mgr, &info);
	if (r)
		return r;

	r = mgr->apply(mgr, DSSCOMP_SETUP_MODE_DISPLAY);
	if (r)
		return r;

	return size;
}

static ssize_t manager_trans_key_value_show(struct odin_overlay_manager *mgr,
					    char *buf)
{
	struct odin_overlay_manager_info info;

	mgr->get_manager_info(mgr, &info);

	return snprintf(buf, PAGE_SIZE, "%#x\n", info.trans_key);
}

static ssize_t manager_trans_key_value_store(struct odin_overlay_manager *mgr,
					     const char *buf, size_t size)
{
	struct odin_overlay_manager_info info;
	u32 key_value;
	int r;

	r = kstrtouint(buf, 0, &key_value);
	if (r)
		return r;

	mgr->get_manager_info(mgr, &info);

	info.trans_key = key_value;

	r = mgr->set_manager_info(mgr, &info);
	if (r)
		return r;

	r = mgr->apply(mgr, DSSCOMP_SETUP_MODE_DISPLAY);
	if (r)
		return r;

	return size;
}

static ssize_t manager_trans_key_enabled_show(struct odin_overlay_manager *mgr,
					      char *buf)
{
	struct odin_overlay_manager_info info;

	mgr->get_manager_info(mgr, &info);

	return snprintf(buf, PAGE_SIZE, "%d\n", info.trans_enabled);
}

static ssize_t manager_trans_key_enabled_store
	(struct odin_overlay_manager *mgr, const char *buf, size_t size)
{
	struct odin_overlay_manager_info info;
	bool enable;
	int r;

	r = strtobool(buf, &enable);
	if (r)
		return r;

	mgr->get_manager_info(mgr, &info);

	info.trans_enabled = enable;

	r = mgr->set_manager_info(mgr, &info);
	if (r)
		return r;

	r = mgr->apply(mgr, DSSCOMP_SETUP_MODE_DISPLAY);
	if (r)
		return r;

	return size;
}

static ssize_t manager_alpha_blending_enabled_show(
		struct odin_overlay_manager *mgr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
		mgr->info.alpha_enabled);
}

static ssize_t manager_alpha_blending_enabled_store(
		struct odin_overlay_manager *mgr,
		const char *buf, size_t size)
{
	struct odin_overlay_manager_info info;
	bool enable;
	int r, temp_enable;

	if (sscanf(buf, "%d", &temp_enable) != 1)
		return -EINVAL;

	enable = temp_enable;

	r = strtobool(buf, &enable);
	if (r)
		return r;

	mgr->get_manager_info(mgr, &info);

	info.alpha_enabled = enable;

	r = mgr->set_manager_info(mgr, &info);
	if (r)
		return r;

	r = mgr->apply(mgr, DSSCOMP_SETUP_MODE_DISPLAY);
	if (r)
		return r;

	return size;
}

struct manager_attribute {
	struct attribute attr;
	ssize_t (*show)(struct odin_overlay_manager *, char *);
	ssize_t	(*store)(struct odin_overlay_manager *, const char *, size_t);
};

#define MANAGER_ATTR(_name, _mode, _show, _store) \
	struct manager_attribute manager_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)

static MANAGER_ATTR(name, S_IRUGO, manager_name_show, NULL);
static MANAGER_ATTR(display, S_IRUGO|S_IWUSR,
		manager_display_show, manager_display_store);
static MANAGER_ATTR(default_color, S_IRUGO|S_IWUSR,
		manager_default_color_show, manager_default_color_store);
static MANAGER_ATTR(trans_key_type, S_IRUGO|S_IWUSR,
		manager_trans_key_type_show, manager_trans_key_type_store);
static MANAGER_ATTR(trans_key_value, S_IRUGO|S_IWUSR,
		manager_trans_key_value_show, manager_trans_key_value_store);
static MANAGER_ATTR(trans_key_enabled, S_IRUGO|S_IWUSR,
		manager_trans_key_enabled_show,
		manager_trans_key_enabled_store);
static MANAGER_ATTR(alpha_blending_enabled, S_IRUGO|S_IWUSR,
		manager_alpha_blending_enabled_show,
		manager_alpha_blending_enabled_store);

static struct attribute *manager_sysfs_attrs[] = {
	&manager_attr_name.attr,
	&manager_attr_display.attr,
	&manager_attr_default_color.attr,
	&manager_attr_trans_key_type.attr,
	&manager_attr_trans_key_value.attr,
	&manager_attr_trans_key_enabled.attr,
	&manager_attr_alpha_blending_enabled.attr,
	NULL
};

static ssize_t manager_attr_show(struct kobject *kobj, struct attribute *attr,
		char *buf)
{
	struct odin_overlay_manager *manager;
	struct manager_attribute *manager_attr;

	manager = container_of(kobj, struct odin_overlay_manager, kobj);
	manager_attr = container_of(attr, struct manager_attribute, attr);

	if (!manager_attr->show)
		return -ENOENT;

	return manager_attr->show(manager, buf);
}

static ssize_t manager_attr_store(struct kobject *kobj, struct attribute *attr,
		const char *buf, size_t size)
{
	struct odin_overlay_manager *manager;
	struct manager_attribute *manager_attr;

	manager = container_of(kobj, struct odin_overlay_manager, kobj);
	manager_attr = container_of(attr, struct manager_attribute, attr);

	if (!manager_attr->store)
		return -ENOENT;

	return manager_attr->store(manager, buf, size);
}

static const struct sysfs_ops manager_sysfs_ops = {
	.show = manager_attr_show,
	.store = manager_attr_store,
};

static struct kobj_type manager_ktype = {
	.sysfs_ops = &manager_sysfs_ops,
	.default_attrs = manager_sysfs_attrs,
};

struct callback_states {
	/*
	 * Keep track of callbacks at the last 3 levels of pipeline:
	 * cache, shadow registers and in DU registers.
	 *
	 * Note: We zero the function pointer when moving from one level to
	 * another to avoid checking for dirty and shadow_dirty fields that
	 * are not common between overlay and manager cache structures.
	 */
	struct odindss_ovl_cb cache, du;
	bool du_displayed;
	bool du_enabled;
};

/*
 * We have 4 levels of cache for the du settings. First two are in SW and
 * the latter two in HW.
 *
 * +--------------------+
 * |overlay/manager_info|
 * +--------------------+
 *          v
 *        apply()
 *          v
 * +--------------------+
 * |     dss_cache      |
 * +--------------------+
 *          v
 *      configure()
 *          v
 * +--------------------+
 * |  shadow registers  |
 * +--------------------+
 *          v
 * VFP or lcd/digit_enable
 *          v
 * +--------------------+
 * |      registers     |
 * +--------------------+
 */
struct overlay_cache_data {
	/* If true, cache changed, but not written to shadow registers. Set
	 * in apply(), cleared when registers written. */
	bool dirty;
	bool du_dirty;

	bool enabled;
	bool invisible;

	bool gscl_en;
	struct ion_client *client;
	u32 p_y_addr;
	u32 p_u_addr; /* relevant for NV12 format only */
	u32 p_v_addr; /* relevant for NV12 format only */
	u32 pre_frm_addr;
	u64 debug_addr1; /* Physical Address For SMMU Debug, Only 2M byte is Valdi*/
	u16 width;
	u16 height;
	enum odin_color_mode color_mode;
	u8 rotation;
	bool mirror;

/* DMA Crop Data*/
	u16 crop_x;
	u16 crop_y;
	u16 crop_w;
	u16 crop_h;

/* Window Out Data*/
	u16 out_x;
	u16 out_y;
	u16 out_w;	/* if 0, out_width == width */
	u16 out_h; /* if 0, out_height == height */

	u8 blending;
	u8 global_alpha;
	u8 pre_mult_alpha;

	int du_channel; /* overlay's channel in DU */

	enum odin_channel channel;
	bool replication;

	enum odin_overlay_zorder zorder;
	bool mxd_use;
	bool formatter_use;
	u8 mgr_flip;
};

struct manager_cache_data {
	/* If true, cache changed, but not written to shadow registers. Set
	 * in apply(), cleared when registers written. */
	bool dirty;
	bool du_dirty;

	u32 default_color;

	enum odin_dss_trans_key_type trans_key_type;
	u32 trans_key;
	bool trans_enabled;

	bool alpha_enabled;

	u16 size_x;
	u16 size_y;

	/* manual update region */
	u16 x, y, w, h;

	/* enlarge the update area if the update area contains scaled
	 * overlays */
	bool enlarge_update_area;

	struct callback_states cb; /* callback data for the last 3 states */

	bool skip_init;
};

static struct {
	spinlock_t lock;
	struct overlay_cache_data overlay_cache[MAX_DSS_OVERLAYS];
	struct writeback_cache_data writeback_cache[MAX_DSS_OVERLAYS];
	struct manager_cache_data manager_cache[MAX_DSS_MANAGERS];
	bool irq_enabled;
	u32 comp_irq_enabled;
} dss_cache;

#ifdef DSS_CACHE_BACKUP
static struct {
	spinlock_t lock;
	struct overlay_cache_data overlay_cache[MAX_DSS_OVERLAYS];
	struct writeback_cache_data writeback_cache[MAX_DSS_OVERLAYS];
	struct manager_cache_data manager_cache[MAX_DSS_MANAGERS];
	bool irq_enabled;
	u32 comp_irq_enabled;
} dss_cache_backup;
#endif

/* propagating callback info between states */
static inline void
dss_ovl_configure_cb(struct callback_states *st, int i, bool enabled)
{
	/* complete info in shadow */
	dss_ovl_cb(&st->cache, i, DSS_COMPLETION_PROGRAMMED);

	/* propagate cache to shadow */
	st->du = st->cache;
	st->du_enabled = enabled;
	st->cache.fn = NULL;	/* info traveled to shadow */
	st->du_displayed = false;
}

static int odin_dss_set_device(struct odin_overlay_manager *mgr,
		struct odin_dss_device *dssdev)
{
	int i;
	int r;

	if (dssdev->manager) {
		DSSERR("display '%s' already has a manager '%s'\n",
			       dssdev->name, dssdev->manager->name);
		return -EINVAL;
	}

	if ((mgr->supported_displays & dssdev->type) == 0) {
		DSSERR("display '%s' does not support manager '%s'\n",
			       dssdev->name, mgr->name);
		return -EINVAL;
	}

	for (i = 0; i < mgr->num_overlays; i++) {
		struct odin_overlay *ovl = mgr->overlays[i];

		if (ovl->manager != mgr || !ovl->info.enabled)
			continue;

		r = odin_ovl_dss_check_overlay(ovl, dssdev);
		if (r)
			return r;
	}

	dssdev->manager = mgr;
	mgr->device = dssdev;
	mgr->device_changed = true;

#if 0
	if (dssdev->type == ODIN_DISPLAY_TYPE_DSI &&
	    !(dssdev->caps & OMAP_DSS_DISPLAY_CAP_MANUAL_UPDATE))
		odin_du_set_irq_type(mgr->id, ODIN_DU_IRQ_TYPE_VSYNC);
	else
		odin_du_set_irq_type(mgr->id, ODIN_DU_IRQ_TYPE_FRAMEDONE);

#endif
	return 0;
}

static int odin_dss_unset_device(struct odin_overlay_manager *mgr)
{
	if (!mgr->device) {
		DSSERR("failed to unset display, display not set.\n");
		return -EINVAL;
	}

	mgr->device->manager = NULL;
	mgr->device = NULL;
	mgr->device_changed = true;

	return 0;
}

int odin_dss_wait_for_vsync(struct odin_overlay_manager *mgr)
{
	unsigned long timeout = msecs_to_jiffies(500);
	u32 irqmask = 0;
	u32 sub_irqmask = 0;
	int r;

	switch (mgr->device->type) {
	case ODIN_DISPLAY_TYPE_HDMI:
		irqmask = CRG_IRQ_VSYNC_HDMI;
		break;
	case ODIN_DISPLAY_TYPE_DSI:
		switch (mgr->device->channel) {
		case ODIN_DSS_CHANNEL_LCD0:
			irqmask = CRG_IRQ_VSYNC_MIPI0;
			break;
		case ODIN_DSS_CHANNEL_LCD1:
			irqmask = CRG_IRQ_VSYNC_MIPI1;
			break;
		case ODIN_DSS_CHANNEL_HDMI:
		case ODIN_DSS_CHANNEL_MAX:
		/* should not reach here */
			DSSERR("MIPI is only supported in LCD0~1 \n");
			break;
		}
		break;
	case ODIN_DISPLAY_TYPE_RGB:
		switch (mgr->device->channel) {
		case ODIN_DSS_CHANNEL_LCD0:
			irqmask = CRG_IRQ_VSYNC_MIPI0;
			break;
		case ODIN_DSS_CHANNEL_LCD1:
		case ODIN_DSS_CHANNEL_HDMI:
		case ODIN_DSS_CHANNEL_MAX:
		/* should not reach here */
			DSSERR("RGB is only supported in LCD0 \n");
			break;
		}
		break;

	default:
			DSSERR("Vsync is not supported \n");
			return -EINVAL;
	}

	if (dssdev_manually_updated(mgr->device))
		r = odin_mipi_dsi_vsync_wait_for_irq_interruptible_timeout(timeout);
	else
		r = odin_crg_wait_for_irq_interruptible_timeout(irqmask, sub_irqmask,
			timeout);

	if ((r < 0) && (mgr->device->state == ODIN_DSS_DISPLAY_ACTIVE)) {
		DSSERR("vsync time out (%d)\n", r);
		printk("(dsi_dev.irq_enabled:%d, mgr->vsync_ctrl:%d)\n",
				mipi_dsi_get_vsync_irq_status(), mgr->vsync_ctrl);
	}

	if (!r)
		mgr->device->first_vsync = true;
	return r;
}

int odin_dss_wait_for_framedone(struct odin_overlay_manager *mgr,
	unsigned long timeout)
{
	u32 irqmask = 0;
	u32 sub_irqmask = 0;
	int r;

	switch (mgr->device->type) {
	case ODIN_DISPLAY_TYPE_HDMI:
		irqmask = CRG_IRQ_DIP2;
		sub_irqmask = CRG_IRQ_DIP_EOVF;
		break;
	case ODIN_DISPLAY_TYPE_DSI:
		switch (mgr->device->channel) {
		case ODIN_DSS_CHANNEL_LCD0:
			irqmask = CRG_IRQ_MIPI0;
			sub_irqmask = CRG_IRQ_MIPI_FRAME_COMPLETE;
			break;
		case ODIN_DSS_CHANNEL_LCD1:
			irqmask = CRG_IRQ_MIPI1;
			sub_irqmask = CRG_IRQ_MIPI_FRAME_COMPLETE;
			break;
		case ODIN_DSS_CHANNEL_HDMI:
		case ODIN_DSS_CHANNEL_MAX:
		/* should not reach here */
			DSSERR("MIPI is only supported in LCD0~1 \n");
			break;
		}
		break;
	case ODIN_DISPLAY_TYPE_RGB:
		switch (mgr->device->channel) {
		case ODIN_DSS_CHANNEL_LCD0:
			irqmask = CRG_IRQ_DIP0;
			sub_irqmask = CRG_IRQ_DIP_EOVF;
			break;
		case ODIN_DSS_CHANNEL_LCD1:
		case ODIN_DSS_CHANNEL_HDMI:
		case ODIN_DSS_CHANNEL_MAX:
		/* should not reach here */
			DSSERR("RGB is only supported in LCD0 \n");
			break;
		}
		break;
	case ODIN_DISPLAY_TYPE_MEM2MEM:
	case ODIN_DISPLAY_TYPE_HDMI_ON:
		break;
	}

	if (dssdev_manually_updated(mgr->device))
	{
		sub_irqmask = CRG_IRQ_MIPI_HS_DATA_COMPLETE;
		r = odin_mipi_cmd_wait_for_framedone(irqmask, sub_irqmask,
			timeout);
	}
	else
		r = odin_crg_wait_for_irq_interruptible_timeout(irqmask, sub_irqmask,
			timeout);

	if (r < 0){
		DSSERR("odin_dss_wait_for_framedone : Time out (%d)\n", r);
		check_clk_gating_status();
	}
	return r;
}

static int overlay_enabled(struct odin_overlay *ovl)
{
	return ovl->info.enabled && ovl->manager && ovl->manager->device;
}

#if 0
/* Is rect1 a subset of rect2? */
static bool rectangle_subset(int x1, int y1, int w1, int h1,
		int x2, int y2, int w2, int h2)
{
	if (x1 < x2 || y1 < y2)
		return false;

	if (x1 + w1 > x2 + w2)
		return false;

	if (y1 + h1 > y2 + h2)
		return false;

	return true;
}

/* Do rect1 and rect2 overlap? */
static bool rectangle_intersects(int x1, int y1, int w1, int h1,
		int x2, int y2, int w2, int h2)
{
	if (x1 >= x2 + w2)
		return false;

	if (x2 >= x1 + w1)
		return false;

	if (y1 >= y2 + h2)
		return false;

	if (y2 >= y1 + h1)
		return false;

	return true;
}

static bool du_is_overlay_scaled(struct overlay_cache_data *oc)
{
	if (oc->out_w != 0 && oc->crop_w != oc->out_w)
		return true;

	if (oc->out_h != 0 && oc->crop_h != oc->out_h)
		return true;

	return false;
}
#endif

int configure_overlay(enum odin_dma_plane plane, enum dsscomp_setup_mode mode,
	bool frame_skip)
{
	struct overlay_cache_data *c;
	struct writeback_cache_data *wbc;
	int r;

	DSSDBG("%d", plane);

	c = &dss_cache.overlay_cache[plane];
	wbc = &dss_cache.writeback_cache[plane];

	if (!c->enabled) {
        odin_du_enable_plane(plane, c->channel, 0, mode, c->invisible,
			frame_skip);

		odin_du_set_mxd_mux_switch_control(c->channel,
			plane, false, MXDDU_SCENARIO_TEST1,
			c->crop_w, c->crop_h, c->out_w, c->out_h);
		return 0;
	}

	r = odin_du_setup_plane(plane,
			c->width, c->height,
			c->crop_x,c->crop_y,
			c->crop_w,c->crop_h,
			c->out_x, c->out_y,
			c->out_w, c->out_h,
			c->color_mode,
			c->rotation,
			c->mirror,
			c->channel,
			c->p_y_addr,
			c->p_u_addr,
			c->p_v_addr,
			c->zorder,
			c->blending,
			c->global_alpha,
			c->pre_mult_alpha,
			c->mgr_flip,
			c->mxd_use,
			c->invisible,
			mode
			);

	if (r) {
		/* this shouldn't happen */
		DSSERR("du_setup_plane failed for ovl %d\n", plane);
		odin_du_enable_plane(plane, c->channel, 0,
				     mode, c->invisible, frame_skip);

		odin_du_set_mxd_mux_switch_control(c->channel,
			plane, false, MXDDU_SCENARIO_TEST1,
			c->crop_w, c->crop_h, c->out_w, c->out_h);
		return r;
	}

	/* zorder is overlay source number in this plane */
	odin_du_enable_plane(plane, c->channel, 1,
			     mode, c->invisible, frame_skip);

	odin_du_set_mxd_mux_switch_control(c->channel,
		plane, c->mxd_use, MXDDU_SCENARIO_TEST1,
		c->crop_w, c->crop_h, c->out_w, c->out_h);

	return 0;
}

#if 0
static enum odin_dma_plane dss_wb_idx2plane(enum odin_wb_idx wb_idx)
{
	int r = 0;
	enum odin_dma_plane wb_plane;

	switch (wb_idx) {
	case ODIN_WB_VID0:
		wb_plane = ODIN_DSS_VID0;
		break;
	case ODIN_WB_VID1:
		wb_plane = ODIN_DSS_VID1;
		break;

	case ODIN_WB_mSCALER:
		wb_plane = ODIN_DSS_mSCALER;
		break;
	default:
		DSSERR("not supported plane for wb_idx %d\n", wb_idx);
		wb_plane = ODIN_DSS_NONE;
		break;
	}
	return wb_plane;
}
#endif

int configure_wb(enum odin_wb_idx wb_idx)
{
	struct writeback_cache_data *wbc;
	int r;

    wbc = &dss_cache.writeback_cache[wb_idx];

	if (wbc->enabled && odin_dss_check_wb(wbc, wbc->wb_plane, wbc->channel))
		DSSDBG("wb->enabled=%d for plane:%d\n",
					wbc->enabled, wbc->wb_plane);

	if (!wbc->enabled) {
		odin_du_enable_wb_plane(wbc->channel, wbc->wb_plane,
					wbc->mode, 0);
		return 0;
	}

	r = odin_du_setup_wb_plane(wbc->wb_plane,
			wbc->width, wbc->height,
			wbc->out_x, wbc->out_y,
			wbc->out_w, wbc->out_h,
			wbc->color_mode,
			wbc->p_y_addr,
			wbc->p_uv_addr,
			wbc->channel,
			wbc->mode
			);

	if (r) {
		/* this shouldn't happen */
		DSSERR("du_setup_wb_plane failed for wb %d\n", wbc->wb_plane);
		odin_du_enable_wb_plane(wbc->channel,
					wbc->wb_plane, wbc->mode, 0);
		return r;
	}

	odin_du_enable_wb_plane(wbc->channel, wbc->wb_plane, wbc->mode, 1);
	return 0;
}

static void configure_default(enum odin_channel channel)
{
	odin_du_set_ovl_mixer_default(channel);
}

static void configure_manager(enum odin_channel channel)
{
	struct manager_cache_data *c;
	struct odin_overlay_manager *mgr =
		odin_mgr_dss_get_overlay_manager(channel);

	/* Don't use debug because of spinlock DSSDBG("%d", channel); */

	c = &dss_cache.manager_cache[channel];

	odin_du_set_trans_key(channel, c->trans_key);
	odin_du_enable_trans_key(channel, c->trans_enabled);

	/* odin_du_mxd_size_irq_set(channel); */

	if (!mgr->frame_skip)
		odin_du_set_ovl_cop_or_rop(channel, ODIN_DSS_OVL_COP);

	/* du_enable_channel(channel, 1); */
}

/* configure_du() tries to write values from cache to shadow registers.
 * It writes only to those managers/overlays that are not busy.
 * returns 0 if everything could be written to shadow registers.
 * returns 1 if not everything could be written to shadow registers. */
static int configure_du(struct odin_overlay_manager *mgr,
	enum dsscomp_setup_mode mode)
{
	struct overlay_cache_data *oc;
	struct manager_cache_data *mc;
	struct writeback_cache_data *wbc;
	const int num_ovls = dss_feat_get_num_ovls();
	const int num_mgrs = dss_feat_get_num_mgrs();
	const int num_wbs = dss_feat_get_num_wbs();
	int i;
	int r;
	int used_ovls, j;
	bool mgr_busy[MAX_DSS_MANAGERS];
	bool mgr_go[MAX_DSS_MANAGERS];
	bool busy;

	/* Don't use debug because of spinlock DSSDBG("configure_du\n"); */

	r = 0;
	busy = false;

	for (i = 0; i < num_mgrs; i++) {
		mc = &dss_cache.manager_cache[i];
		mgr_busy[i] = odin_du_go_busy(i);
		mgr_go[i] = false;

	if (!mc->dirty)
			continue;

		if (mgr_busy[i]) {
			busy = true;
			continue;
		}

		configure_default(i);
	}

	/* Commit overlay settings */
	for (i = 0; i < num_ovls; ++i) {
		oc = &dss_cache.overlay_cache[i];

		if (!oc->dirty)
			continue;

		if (mgr_busy[oc->channel]) {
			busy = true;
			continue;
		}

		r = configure_overlay(i, mode, mgr->frame_skip);
		if (r)
			DSSERR("configure_overlay %d failed\n", i);

		oc->dirty = false;
		oc->du_dirty = true;
		mgr_go[oc->channel] = true;
	}

	/* Commit writeback settings */
	for (i = 0; i < num_wbs; ++i) {
		wbc = &dss_cache.writeback_cache[i];

		if (!wbc->dirty)
			continue;

		r = configure_wb(i);
		if (r)
			DSSERR("configure_wrteback %d failed\n", i);

		wbc->dirty = false;
		wbc->du_dirty = true;
	}

	/* Commit manager settings */
	for (i = 0; i < num_mgrs; ++i) {
		mc = &dss_cache.manager_cache[i];

		if (!mc->dirty)
			continue;

		if (mgr_busy[i]) {
			busy = true;
			continue;
		}

		for (j = used_ovls = 0; j < num_ovls; j++) {
			oc = &dss_cache.overlay_cache[j];
			if (oc->channel == i && oc->enabled)
				used_ovls++;
		}

		configure_manager(i);
		dss_ovl_configure_cb(&mc->cb, i, used_ovls);

		mc->dirty = false;
		mc->du_dirty = true;
		mgr_go[i] = true;
	}

	/* Don't use debug because of spinlock DSSDBG( "configure_du end (%ld) \n", jiffies); */

	/* set GO */
	for (i = 0; i < num_mgrs; ++i) {
		mc = &dss_cache.manager_cache[i];

		if (!mgr_go[i])
			continue;

		/* We don't need GO with manual update display. LCD iface will
		 * always be turned off after frame, and new settings will be
		 * taken in to use at next update */
		if (mc->skip_init)
			mc->skip_init = false;
		else
			odin_du_go(i);
	}

	if (busy)
		r = 1;
	else
		r = 0;

	return r;
}

int configure_cabc(struct odin_overlay_manager *mgr, bool cabc_en, bool first)
{
	int r;
	if (cabc_en && first)
            r= odin_cabc_cmd(mgr->id, CABC_OPEN);
	else if (!cabc_en && first)
	{
            r= odin_cabc_cmd(mgr->id, CABC_CLOSE);
		return r;
	}
	r = odin_cabc_cmd(mgr->id, CABC_GAIN_CTRL);
	return r;
}

#if 0
/* Make the coordinates even. There are some strange problems with OMAP and
 * partial DSI update when the update widths are odd. */
static void make_even(u16 *x, u16 *w)
{
	u16 x1, x2;

	x1 = *x;
	x2 = *x + *w;

	x1 &= ~1;
	x2 = ALIGN(x2, 2);

	*x = x1;
	*w = x2 - x1;
}
#endif

void odin_mgr_dss_start_update(struct odin_dss_device *dssdev)
{
	struct manager_cache_data *mc;
	struct overlay_cache_data *oc;
	const int num_ovls = dss_feat_get_num_ovls();
	const int num_mgrs = dss_feat_get_num_mgrs();
	struct odin_overlay_manager *mgr;
	int i;
	unsigned long flags;

	mgr = dssdev->manager;

	spin_lock_irqsave(&dss_cache.lock, flags);

	for (i = 0; i < num_ovls; ++i) {
		oc = &dss_cache.overlay_cache[i];
		if (oc->channel != mgr->id)
			continue;
	}

	for (i = 0; i < num_mgrs; ++i) {
		mc = &dss_cache.manager_cache[i];
		if (mgr->id != i)
			continue;
	}

	spin_unlock_irqrestore(&dss_cache.lock, flags);

	dssdev->manager->enable(dssdev->manager);
}

#ifdef DSS_TIME_CHECK_LOG
void odin_mgr_dss_time_start(enum odin_dss_time_stamp time_item)
{
	start_time_info[time_item] = ktime_get();
}

s64 odin_mgr_dss_time_end(enum odin_dss_time_stamp time_item)
{
	ktime_t end_time;
	s64 lead_time_ns;

	end_time = ktime_get();
	lead_time_ns = ktime_to_ns(ktime_sub(
				   end_time, start_time_info[time_item]));
	return lead_time_ns;
}

s64 odin_mgr_dss_time_end_ms(enum odin_dss_time_stamp time_item)
{
	ktime_t end_time;
	s64 lead_time_ms;

	end_time = ktime_get();
	lead_time_ms = ktime_to_ms(ktime_sub(
				   end_time, start_time_info[time_item]));
	return lead_time_ms;
}
#endif

static void dss_completion_process(void *data, int mgr_ix)
{
	int i;
	struct manager_cache_data *mc;
	struct overlay_cache_data *oc;
	const int num_ovls = dss_feat_get_num_ovls();
	unsigned long flags;

	spin_lock_irqsave(&dss_cache.lock, flags);
	mc = &dss_cache.manager_cache[mgr_ix];
	mgrs[mgr_ix]->device->first_vsync = true;
	dss_ovl_cb(&mc->cb.du, mgr_ix, DSS_COMPLETION_DISPLAYING);
	mc->cb.du.fn = NULL;
	mc->cb.du_displayed = true;

	for (i = 0; i < num_ovls; i++) {
		oc = &dss_cache.overlay_cache[i];
		if (oc->channel != mgr_ix)
			continue;

		if (oc->enabled)
			oc->pre_frm_addr = oc->p_y_addr;
		else
			oc->pre_frm_addr = (u32)NULL;
	}

	spin_unlock_irqrestore(&dss_cache.lock, flags);

}

#if 0
static void dss_completion_irq_handler(void *data, u32 mask, u32 sub_mask)
{
	int i, r, mgr_ix;
	struct manager_cache_data *mc;
	struct overlay_cache_data *oc;
	const int num_mgrs = dss_feat_get_num_mgrs();
	const int num_ovls = dss_feat_get_num_ovls();
	unsigned long flags;
#if BUFSYNC_DEBUGGING
	struct odinfb_device *fbdev = data;
#endif
	u32 fb_ovl_mask;

	mgr_ix = fls(mask) - 1;
	spin_lock_irqsave(&dss_cache.lock, flags);

	mc = &dss_cache.manager_cache[mgr_ix];
	mgrs[mgr_ix]->device->first_vsync = true;
	dss_ovl_cb(&mc->cb.du, mgr_ix, DSS_COMPLETION_DISPLAYING);
	mc->cb.du.fn = NULL;
	mc->cb.du_displayed = true;

	for (i = 0; i < num_ovls; i++) {
		oc = &dss_cache.overlay_cache[i];
		if (oc->channel != mgr_ix)
			continue;

		if (oc->enabled)
			oc->pre_frm_addr = oc->p_y_addr;
		else
			oc->pre_frm_addr = NULL;
	}

#if BUFSYNC_DEBUGGING
	odinfb_signal_timeline(fbdev);
#endif
	if ((mgrs[mgr_ix]->device->driver->get_update_mode(mgrs[mgr_ix]->device))
		== ODIN_DSS_UPDATE_AUTO)
	{
#if BUFSYNC_DEBUGGING
		odin_crg_unregister_isr(dss_completion_irq_handler,
					fbdev, mask, sub_mask);
#else
		odin_crg_unregister_isr(dss_completion_irq_handler,
					NULL, mask, sub_mask);
#endif
	}
end:
	spin_unlock_irqrestore(&dss_cache.lock, flags);
}
#else
static void dss_completion_irq_handler(void *data, u32 mask, u32 sub_mask)
{
	int mgr_ix;

	mgr_ix = fls(mask) - 1;

	dss_completion_process(data, mgr_ix);

	odin_crg_unregister_isr(dss_completion_irq_handler,
				NULL, mask, sub_mask);
}

#ifdef DSS_CACHE_BACKUP
int odin_dss_cache_log(int mgr_ix)
{
	const int num_ovls = dss_feat_get_num_ovls();
	struct overlay_cache_data *oc;
	int i;

	for (i = 0; i < num_ovls; i++) {
		oc = &dss_cache_backup.overlay_cache[i];
		if (oc->channel != mgr_ix)
			continue;

		if (oc->enabled)
		{
			DSSINFO("pipe %d, src_width %d, src_height %d \n", i,
				oc->crop_w, oc->crop_h);
			DSSINFO("out_width %d, out_height %d \n",
				oc->out_w, oc->out_h);
			DSSINFO("out_x %d, out_y %d \n",
				oc->out_x, oc->out_y);
		}
	}

}
#endif

#endif

int odin_dss_mgr_set_state_irq(struct odin_overlay_manager *mgr,
	enum dsscomp_setup_mode mode)
{
	u32 mask;
	int r = 0;
#if 0
	dss_completion_process(NULL, mgr->id);
#else
	switch (mgr->device->channel) {
		case ODIN_DSS_CHANNEL_LCD0:
			mask = CRG_IRQ_VSYNC_MIPI0;
			break;
		case ODIN_DSS_CHANNEL_LCD1:
			mask = CRG_IRQ_VSYNC_MIPI1;
			break;
		case ODIN_DSS_CHANNEL_HDMI:
			mask = CRG_IRQ_VSYNC_HDMI;
			break;
		default:
			mask = MAIN_MAX_POS;
	}

	if (mask < MAIN_MAX_POS)
	{
		if (mode & DSSCOMP_SETUP_MODE_MEM2MEM)
			dss_completion_process(NULL, mgr->id);
		else if ((mgr->device->driver->get_update_mode(mgr->device))
			== ODIN_DSS_UPDATE_MANUAL)
			dss_completion_process(NULL, mgr->id);
		else
		{
#if 0
			r = odin_crg_register_isr(dss_completion_irq_handler,
				NULL, mask, (u32)NULL);
#else
			mgr->wait_for_vsync(mgr);
			dss_completion_process(NULL, mgr->id);
#endif
		}
	}
#endif

#ifdef DSS_CACHE_BACKUP
	memcpy(&dss_cache_backup, &dss_cache, sizeof(dss_cache));
#endif

	return r;
}

#if 1
extern atomic_t vsync_ready;
#endif
#if RUNTIME_CLK_GATING
extern bool clk_gated;
extern spinlock_t clk_gating_lock;
#endif
static int odin_dss_mgr_blank(struct odin_overlay_manager *mgr,
			bool wait_for_go)
{
	struct overlay_cache_data *oc;
	struct manager_cache_data *mc;
	struct writeback_cache_data *wc;
	unsigned long flags;
	int i;
	int r = 0, r_get = 0;
	u32 plane_clk_mask = 0;
	u32 plane_mask = 0;
	u16 w, h;
	unsigned long timeout = msecs_to_jiffies(500);

	DSSINFO("odin_dss_mgr_blank(%s,wait=%d)\n", mgr->name, wait_for_go);

	/* r_get = r = du_runtime_get(); */
	/* still clear cache even if failed to get clocks, just don't config */

	spin_lock_irqsave(&dss_cache.lock, flags);
	/* disable overlays in overlay info structs and in cache */
	for (i = 0; i < odin_ovl_dss_get_num_overlays(); i++) {
		struct odin_overlay_info oi = { .enabled = false };
		struct odin_overlay *ovl;
		struct odin_writeback_info wi = { .enabled = false };
		struct odin_writeback *wb;

		ovl = odin_ovl_dss_get_overlay(i);
		oc = &dss_cache.overlay_cache[ovl->id];

		if (ovl->manager != mgr)
			continue;

		plane_mask |= (1 << i);
		plane_clk_mask |= odin_crg_du_plane_to_clk((1 << i), true);

		ovl->info = oi;
		ovl->info_dirty = false;
		oc->dirty = true;
		oc->enabled = false;

		if ((i < ODIN_DSS_VID2_FMT) || (i == ODIN_DSS_mSCALER))
		{
			if (i < ODIN_DSS_VID2_FMT)
				wb = odin_dss_get_writeback(i);
			else
				wb = odin_dss_get_writeback(2);

			wc = &dss_cache.writeback_cache[ovl->id];
			if (wc->enabled)
			{
				wb->info = wi;
				wb->info_dirty = false;
				wc->dirty = true;
				wc->enabled = false;
			}
		}
	}

	odin_crg_dss_clk_ena(CRG_CORE_CLK, plane_clk_mask, true);
#if RUNTIME_CLK_GATING
	if (dssdev_manually_updated(mgr->device))/* && !check_clk_gating_status())*/
		enable_runtime_clk(mgr, false); /* mem2mem mode is false */
#endif
	/* dirty manager */
	mc = &dss_cache.manager_cache[mgr->id];
	mc->cb.cache.fn = NULL;
	mgr->info.cb.fn = NULL;
	mc->dirty = false;
	mgr->info_dirty = false;

	/*
	 * TRICKY: Enable apply irq even if not waiting for vsync, so that
	 * DU programming takes place in case GO bit was on.
	 */
	if (!r_get) {
		r = configure_du(mgr, DSSCOMP_SETUP_MODE_DISPLAY);
		if (r)
			pr_info("mgr_blank while GO is set");
	}

	if (r_get || !wait_for_go) {
		/* pretend that programming has happened */
		for (i = 0; i < odin_ovl_dss_get_num_overlays(); ++i) {
			oc = &dss_cache.overlay_cache[i];
			if (oc->channel != mgr->id)
				continue;
		}

		if (r && mc->dirty)
			dss_ovl_configure_cb(&mc->cb, i, false);
	}

	spin_unlock_irqrestore(&dss_cache.lock, flags);

#if 0
	if (!r_get)
		du_runtime_put();
#endif

	if (dssdev_manually_updated(mgr->device))
	{
		odin_du_set_ovl_cop_or_rop(mgr->id, ODIN_DSS_OVL_ROP);
		mgr->device->driver->get_resolution(mgr->device, &w, &h);

#ifdef DSS_DFS_MUTEX_LOCK
		mutex_lock(&dfs_mtx);
#elif defined(DSS_DFS_SPINLOCK)
		get_mem_dfs();
#endif
		mgr->device->driver->sync(mgr->device);

		mgr->device->driver->update(mgr->device, 0, 0, w, h);

		odin_du_set_ovl_cop_or_rop(mgr->id, ODIN_DSS_OVL_COP);
		if (wait_for_go)
			mgr->wait_for_framedone(mgr, timeout);
	}
	else
	{
		if (wait_for_go)
			mgr->wait_for_vsync(mgr);
	}

	/*odin_crg_dss_clk_ena(CRG_CORE_CLK, plane_clk_mask, false);*/
	if (dssdev_manually_updated(mgr->device)) {
#ifdef DSS_DFS_MUTEX_LOCK
		mutex_unlock(&dfs_mtx);
#elif defined(DSS_DFS_SPINLOCK)
		put_mem_dfs();
#endif
#if RUNTIME_CLK_GATING
		disable_runtime_clk(mgr->id);
#endif
	}

	mgr->clk_enable(0, plane_mask, false);	/* plane_clk_mask disable */
	return r;
}

int odin_dss_mgr_cabc(struct odin_overlay_manager *mgr,	bool cabc_en)
{
    int r = 0;

    DSSDBG("%s:\n", __func__);
#ifdef DSS_VIDEO_PLAYBACK_CABC
    if (!(mgr->cabc_en) && (cabc_en)) /*first enable  CABC_OPEN */
    {
        r= configure_cabc(mgr, cabc_en, true);
        if(r < 0) {
            DSSERR("%s: configure_cabc ret= %d\n", __func__, r);
            goto err;
        }
        mgr->cabc_en = true;
    }
    else if ((mgr->cabc_en) && !(cabc_en)) /*first disable  CABC_CLOSE*/
    {
        mgr->cabc_en = false;
        r= configure_cabc(mgr, cabc_en, true);
    }
    else
        r = configure_cabc(mgr, cabc_en, false);
    DSSDBG("CABC Value setting \n");
#endif

err:
    return r;
}

int odin_mgr_manager_unregister_callback(struct odin_overlay_manager *mgr,
					 struct odindss_ovl_cb *cb)
{
	unsigned long flags;
	int r = 0;
	spin_lock_irqsave(&dss_cache.lock, flags);
#if 0
	if (mgr->info_dirty &&
	    mgr->info.cb.fn == cb->fn &&
	    mgr->info.cb.data == cb->data)
		mgr->info.cb.fn = NULL;
	else
		r = -EPERM;
#endif
	spin_unlock_irqrestore(&dss_cache.lock, flags);
	return r;
}

static int odin_dss_plane_clk_ena (u32 plane_clk_mask, u32 plane_clk_dmask,
							bool mem2mem_mode)
{
	u32 clk_mask = 0;
	u32 clk_dmask = 0;
	struct clk *disp1_clk;

	DSSDBG("plane_clk_mask:0x%x, plane_clk_dmask:0x%x, wb:%d\n",
			plane_clk_mask, plane_clk_dmask, wb_apply);

	if (plane_clk_dmask)
	{
		/* core_clk */
		clk_dmask = odin_crg_du_plane_to_clk(plane_clk_dmask, false);
		/* In case of using WB thr. another sync gen (as like camera capture) */
		if (mem2mem_mode) /* sync_gen, disp1(sync1), vid1(0) */
		{
			if (plane_clk_dmask & (1 << ODIN_DSS_VID1))
			{
				/* dp2_pll (WB zoom capture || video scaler) */
				disp1_clk = clk_get(NULL, "disp1_clk");
				if (disp1_clk->prepare_count)
				{
					if (disp1_clk->enable_count)
						odin_crg_dss_pll_ena(disp1_clk, false);
					clk_unprepare(disp1_clk);
				}
			}
		}
		odin_crg_dss_clk_ena(CRG_CORE_CLK, clk_dmask, false);
	}

	if (plane_clk_mask)
	{
		/* core_clk */
		clk_mask = odin_crg_du_plane_to_clk(plane_clk_mask, true);

		/* In case of using WB thr. another sync gen (as like camera capture) */
		if (mem2mem_mode)
		{
			if (plane_clk_mask & (1 << ODIN_DSS_VID1))
			{
				/* dp2_pll (WB capture) */
				disp1_clk = clk_get(NULL, "disp1_clk");
				if (!disp1_clk->prepare_count)
				{
					clk_prepare(disp1_clk);
					if (!disp1_clk->enable_count)
						odin_crg_dss_pll_ena(disp1_clk, true);
				}
			}
		}
		odin_crg_dss_clk_ena(CRG_CORE_CLK, clk_mask, true);
	}

	/*printk("core:%x, other:%x\n", odin_crg_get_dss_clk_ena(CRG_CORE_CLK),
			odin_crg_get_dss_clk_ena(CRG_OTHER_CLK));*/
	return 0;
}

static int odin_dss_mgr_apply(struct odin_overlay_manager *mgr,
	enum dsscomp_setup_mode mode)
{
	struct overlay_cache_data *oc;
	struct manager_cache_data *mc;
	struct odin_dss_device *dssdev;
	int i;
	struct odin_overlay *ovl;
	int num_planes_enabled = 0;
	unsigned long flags;
	int r = 0;
	/* u32 mask; */

	DSSDBG("odin_dss_mgr_apply(%s)\n", mgr->name);
#if 0
	r = du_runtime_get();
	if (r)
		return r;
#endif
	spin_lock_irqsave(&dss_cache.lock, flags);

	if (!(mode & DSSCOMP_SETUP_MODE_MEM2MEM) &&
		(!mgr->device ||
		(mgr->device->state != ODIN_DSS_DISPLAY_ACTIVE))) {
		DSSDBG("cannot apply mgr(%s) on inactive device\n",
								mgr->name);
		r = -ENODEV;
		goto done;
	}

	/* Configure overlays */
	for (i = 0; i < odin_ovl_dss_get_num_overlays(); ++i) {
		struct odin_dss_device *dssdev;

		ovl = odin_ovl_dss_get_overlay(i);

		if (ovl->manager != mgr)
			continue;

		oc = &dss_cache.overlay_cache[ovl->id];
		dssdev = mgr->device;

		if (!overlay_enabled(ovl) || !dssdev) {
			ovl->info.enabled = false;
		} else if (!ovl->info_dirty) {
			if (oc->enabled)
				++num_planes_enabled;
			continue;
		} else if (odin_ovl_dss_check_overlay(ovl, dssdev)) {
			ovl->info.enabled = false;
		}

		ovl->info_dirty = false;
		if (ovl->info.enabled || oc->enabled)
			oc->dirty = true;

		oc->enabled = ovl->info.enabled;

		if (!oc->enabled)
			continue;

		oc->invisible = ovl->info.invisible;

		oc->p_y_addr = ovl->info.p_y_addr;
		oc->p_u_addr = ovl->info.p_u_addr;
		oc->p_v_addr = ovl->info.p_v_addr;
		oc->debug_addr1 = ovl->info.debug_addr1;
		oc->width = ovl->info.width;
		oc->height = ovl->info.height;
		oc->color_mode = ovl->info.color_mode;
		oc->rotation = ovl->info.rotation;
		oc->mirror = ovl->info.mirror;

		oc->crop_x = ovl->info.crop_x;
		oc->crop_y = ovl->info.crop_y;
		oc->crop_w = ovl->info.crop_w;
		oc->crop_h = ovl->info.crop_h;

		oc->out_x = ovl->info.out_x;
		oc->out_y = ovl->info.out_y;
		oc->out_w = ovl->info.out_w;
		oc->out_h = ovl->info.out_h;
		oc->blending = ovl->info.blending;
		oc->global_alpha = ovl->info.global_alpha;
		oc->pre_mult_alpha = ovl->info.pre_mult_alpha;
		oc->zorder = ovl->info.zorder;
		oc->mxd_use = ovl->info.mxd_use;
		oc->formatter_use = ovl->info.formatter_use;

		oc->replication = dss_use_replication(dssdev,
						      ovl->info.color_mode);

		oc->gscl_en = ovl->info.gscl_en;

		oc->channel = mgr->id;

		oc->mgr_flip = mgr->mgr_flip;

		++num_planes_enabled;
	}

	mc = &dss_cache.manager_cache[mgr->id];

	if (mgr->device_changed) {
		mgr->device_changed = false;
		mgr->info_dirty  = true;
	}

	if (!mgr->info_dirty)
		goto skip_mgr;

	if (!mgr->device)
		goto skip_mgr;

	dssdev = mgr->device;

	mc->cb.cache = mgr->info.cb;
	mgr->info.cb.fn = NULL;

	mgr->info_dirty = false;
	mc->dirty = true;

	mc->default_color = mgr->info.default_color;
	mc->trans_key = mgr->info.trans_key;
	mc->trans_enabled = mgr->info.trans_enabled;
	mc->alpha_enabled = mgr->info.alpha_enabled;

	mc->size_x = mgr->info.size_x;
	mc->size_y = mgr->info.size_y;

	mc->skip_init = dssdev->skip_init;

skip_mgr:

	configure_du(mgr, mode);

done:
	spin_unlock_irqrestore(&dss_cache.lock, flags);

	/* du_runtime_put(); */
	return r;
}

enum odin_channel channel;
enum odin_dma_plane wb_plane;

int odin_dss_wb_apply(struct odin_overlay_manager *mgr)
{
	struct writeback_cache_data *wbc;
	unsigned long flags;
	int r = 0;
	int i;
	struct odin_writeback *wb;

	DSSDBG("odin_dss_wb_apply(%s)\n", mgr->name);

	spin_lock_irqsave(&dss_cache.lock, flags);

	for (i = 0; i < odin_wb_dss_get_num_writebacks(); i++) {

		wb = odin_dss_get_writeback(i);

		if (wb == NULL)
		{
			r = -EINVAL;
			break;
		}

		/* skip composition, if manager is enabled. It happens when HDMI/TV
		 * physical layer is activated in the time, when MEM2MEM with manager
		 * mode is used.
		 */
		if (wb->info.channel== ODIN_DSS_CHANNEL_HDMI &&
				/* du_is_channel_enabled(ODIN_DSS_CHANNEL_DIGIT) &&  */
					wb->info.mode == ODIN_WB_MEM2MEM_MODE) {
			DSSERR("manager %d busy, dropping\n", mgr->id);
			r =  -EBUSY;
			break;
		}

		if (wb->info.channel != mgr->id)
			continue;

		wbc = &dss_cache.writeback_cache[i];

		if (wb && !wb->info.enabled) {
			wb->info.enabled = false;
		} else if (!wb->info_dirty) {
			continue;
		}

		wb->info_dirty = false;

		if (wb->info.enabled || wbc->enabled)
			wbc->dirty = true;

		wbc->enabled = wb->info.enabled;

		if (!wbc->enabled)
			continue;

		wb->info_dirty = false;

		wbc->p_y_addr = wb->info.p_y_addr;
		wbc->p_uv_addr = wb->info.p_uv_addr;

		wbc->width = wb->info.width;
		wbc->height = wb->info.height;

		wbc->out_x = wb->info.out_x;
		wbc->out_y = wb->info.out_y;
		wbc->out_w = wb->info.out_w;
		wbc->out_h = wb->info.out_h;

		wbc->color_mode = wb->info.color_mode;
		wbc->mode = wb->info.mode;
		wbc->channel = wb->info.channel;
		wbc->wb_plane = wb->plane; /* dss_wb_idx2plane(i); */

		wbc->debug_addr1 = wb->info.debug_addr1;
#if 0 /* Don't use debug because of spinlock */
		if (wbc->enabled)
			DSSDBG("wb driver addr = %x\n",wbc->p_y_addr);
#endif
	}

	spin_unlock_irqrestore(&dss_cache.lock, flags);
	return r;
}
EXPORT_SYMBOL(odin_dss_wb_apply);

int odin_dram_clk_change_before(void)
{
	enum odin_dss_update_mode dss_update_mode;
	/* unsigned long timeout = msecs_to_jiffies(100); */
	/* int r;*/

#ifdef CONFIG_ARM_ODIN_BUS_DEVFREQ
	/* DSSINFO("odin_dram_clk_change_before \n"); */

	struct odin_overlay_manager *mgr = odin_mgr_dss_get_overlay_manager(0);

   	if (mgr->device->state != ODIN_DSS_DISPLAY_ACTIVE)
		return 0;
#if 0
	if (odin_crg_get_error_handling_status())
		return 0;
#endif
	dss_update_mode = mgr->device->driver->get_update_mode(mgr->device);

	if (dss_update_mode == ODIN_DSS_UPDATE_MANUAL)
	{
#if 0
		r = mgr->wait_for_framedone(mgr, timeout);
		if (r < 0)
			DSSERR("dvfs odin_mipi_dsi_update_done_wait timeout \n");
#endif
#ifdef DSS_DFS_MUTEX_LOCK
		mutex_lock(&dfs_mtx);
#endif
		return 0;
	}

	if (mgr->frame_skip == true)
	{
		DSSERR("mipi-dsi off state \n");
		return 0;
	}

	/* dsscomp_complete_workqueue(mgr->device); */

	odin_mipi_hs_video_start_stop(ODIN_DSS_CHANNEL_LCD0, mgr, false);

    /* odin_du_set_ovl_cop_or_rop(ODIN_DSS_CHANNEL_LCD0, ODIN_DSS_OVL_ROP); */
	/* delay(1); */
	/* odin_mipi_dsi_phy_power(ODIN_DSS_CHANNEL_LCD0, false); */
#endif

	return 0;
}

int odin_dram_clk_change_after(void)
{
#ifdef CONFIG_ARM_ODIN_BUS_DEVFREQ
	/* DSSINFO("odin_dram_clk_change_after \n");  */
	enum odin_dss_update_mode dss_update_mode;
	struct odin_overlay_manager *mgr = odin_mgr_dss_get_overlay_manager(0);

	if (mgr->device->state != ODIN_DSS_DISPLAY_ACTIVE)
		return 0;
#if 0
	if (odin_crg_get_error_handling_status())
		return 0;
#endif

	dss_update_mode = mgr->device->driver->get_update_mode(mgr->device);
	if (dss_update_mode == ODIN_DSS_UPDATE_MANUAL)
	{
#ifdef DSS_DFS_MUTEX_LOCK
		mutex_unlock(&dfs_mtx);
#endif
		/* DSSINFO("odin_dram_clk_change_after \n");  */
		return 0;
	}

	 if (mgr->frame_skip == false)
		 return 0;

	 /* odin_mipi_dsi_phy_power(ODIN_DSS_CHANNEL_LCD0, true); */
	odin_mipi_hs_video_start_stop(ODIN_DSS_CHANNEL_LCD0, mgr, true);

	/* odin_du_set_ovl_cop_or_rop(ODIN_DSS_CHANNEL_LCD0, ODIN_DSS_OVL_ROP); */
#endif
	return 0;
}


#ifdef CONFIG_DEBUG_FS
static void seq_print_cb(struct seq_file *s, struct odindss_ovl_cb *cb)
{
#if 0
	if (!cb->fn) {
		seq_printf(s, "(none)\n");
		return;
	}

	seq_printf(s, "mask=%c%c%c%c [%p] %pf\n",
		   (cb->mask & DSS_COMPLETION_CHANGED) ? 'C' : '-',
		   (cb->mask & DSS_COMPLETION_PROGRAMMED) ? 'P' : '-',
		   (cb->mask & DSS_COMPLETION_DISPLAYING) ? 'D' : '-',
		   (cb->mask & DSS_COMPLETION_PROGRAM_ERROR) ? 'E' : '-',
		   cb->data,
		   cb->fn);
#endif
}
#endif

static void seq_print_cbs(struct odin_overlay_manager *mgr, struct seq_file *s)
{
#if 1
#ifdef CONFIG_DEBUG_FS
	struct manager_cache_data *mc;
	unsigned long flags;

	spin_lock_irqsave(&dss_cache.lock, flags);

	mc = &dss_cache.manager_cache[mgr->id];

	seq_printf(s, "  DU pipeline:\n\n"
		      "    info:%13s ", mgr->info_dirty ? "DIRTY" : "clean");
	seq_print_cb(s, &mgr->info.cb);
	seq_printf(s, "    cache:%12s ", mc->dirty ? "DIRTY" : "clean");

	seq_printf(s, "\n");

	spin_unlock_irqrestore(&dss_cache.lock, flags);
#endif
#endif
}

static int dss_check_manager(struct odin_overlay_manager *mgr)
{
	/* TODO: */
	return 0;
}

int odin_mgr_dss_ovl_set_info(struct odin_overlay *ovl,
		struct odin_overlay_info *info)
{
	int r;
	struct odin_overlay_info old_info;
	unsigned long flags;

	spin_lock_irqsave(&dss_cache.lock, flags);
	old_info = ovl->info;
	ovl->info = *info;

	if (ovl->manager) {
		r = odin_ovl_dss_check_overlay(ovl, ovl->manager->device);
		if (r) {
			ovl->info = old_info;
			spin_unlock_irqrestore(&dss_cache.lock, flags);
			return r;
		}
	}

	ovl->info_dirty = true;
	spin_unlock_irqrestore(&dss_cache.lock, flags);

	return 0;
}

static int odin_dss_mgr_set_info(struct odin_overlay_manager *mgr,
		struct odin_overlay_manager_info *info)
{
	int r;
	struct odin_overlay_manager_info old_info;
	unsigned long flags;

	spin_lock_irqsave(&dss_cache.lock, flags);
	old_info = mgr->info;
	mgr->info = *info;

	r = dss_check_manager(mgr);
	if (r) {
		mgr->info = old_info;
		spin_unlock_irqrestore(&dss_cache.lock, flags);
		return r;
	}

	mgr->info_dirty = true;
	spin_unlock_irqrestore(&dss_cache.lock, flags);

	return 0;
}

static void odin_dss_mgr_get_info(struct odin_overlay_manager *mgr,
		struct odin_overlay_manager_info *info)
{
	*info = mgr->info;
}

static int odin_dss_mgr_cache_update(struct odin_overlay_manager *mgr)
{
	struct manager_cache_data *mc;
	unsigned long flags;

	spin_lock_irqsave(&dss_cache.lock, flags);

	mc = &dss_cache.manager_cache[mgr->id];
	mc->dirty = true;	/* cache update */

	spin_unlock_irqrestore(&dss_cache.lock, flags);

	return 0;
}

bool odin_mgr_iova_check(u32 iova_addr)
{
#ifdef CONFIG_PANEL_LGLH600WF2
#else
	int i;
	struct overlay_cache_data *oc;
	struct writeback_cache_data *wc;
	for (i = 0; i< MAX_DSS_OVERLAYS ; i++)
	{
		oc = &dss_cache.overlay_cache[i];
		if (oc->pre_frm_addr == iova_addr)
		{
			DSSINFO("try to free iova = %x in using \n", iova_addr);
			return true;
		}

		if ((oc->enabled) && (oc->p_y_addr == iova_addr))
		{
			DSSINFO("try to free iova = %x to use soon \n", iova_addr);
			return true;
		}
	}

	for (i = 0; i< MAX_DSS_WRITEBACKS ; i++)
	{
		wc = &dss_cache.writeback_cache[i];
		if ((wc->enabled) && (wc->p_y_addr == iova_addr))
			return true;
	}
#endif

	return false;
}

static int dss_mgr_enable(struct odin_overlay_manager *mgr)
{
	odin_du_enable_channel(mgr->id, mgr->device->type, 1);
	return 0;
}

static int dss_mgr_disable(struct odin_overlay_manager *mgr)
{
	odin_du_enable_channel(mgr->id, mgr->device->type, 0);
	return 0;
}

static void odin_dss_add_overlay_manager(struct odin_overlay_manager *manager)
{
	++num_managers;
	list_add_tail(&manager->list, &manager_list);
	if (manager->id < ARRAY_SIZE(mgrs))
		mgrs[manager->id] = manager;
}

/* From odin_dss_probe in core.c */
int odin_mgr_dss_init_overlay_managers(struct platform_device *pdev)
{
	int i, r;

	DSSDBG("odin_mgr_dss_init_overlay_managers\n");

	spin_lock_init(&dss_cache.lock);

	INIT_LIST_HEAD(&manager_list);

	num_managers = 0;

	for (i = 0; i < dss_feat_get_num_mgrs(); i++) {
		struct odin_overlay_manager *mgr;
		mgr = kzalloc(sizeof(*mgr), GFP_KERNEL);

		BUG_ON(mgr == NULL);

		mgr->info.alpha_enabled = true;

		switch (i) {
		case 0:
			mgr->name = "lcd0";
			mgr->id = ODIN_DSS_CHANNEL_LCD0;
			break;
		case 1:
			mgr->name = "lcd1";
			mgr->id = ODIN_DSS_CHANNEL_LCD1;
			break;
		case 2:
			mgr->name = "hdmi";
			mgr->id = ODIN_DSS_CHANNEL_HDMI;
			break;
		default:
			break;
		}

		mgr->set_device = &odin_dss_set_device;
		mgr->unset_device = &odin_dss_unset_device;
		mgr->clk_enable = &odin_dss_plane_clk_ena;
		mgr->apply = &odin_dss_mgr_apply;
		mgr->set_manager_info = &odin_dss_mgr_set_info;
		mgr->get_manager_info = &odin_dss_mgr_get_info;
		mgr->update_manager_cache = &odin_dss_mgr_cache_update;
		mgr->wait_for_vsync = &odin_dss_wait_for_vsync;
		mgr->wait_for_framedone = &odin_dss_wait_for_framedone;
		mgr->blank = &odin_dss_mgr_blank;
		mgr->dump_cb = &seq_print_cbs;
		mgr->cabc = &odin_dss_mgr_cabc;

		mgr->enable = &dss_mgr_enable;
		mgr->disable = &dss_mgr_disable;

		mgr->supported_displays =
			dss_feat_get_supported_displays(mgr->id);

		odin_ovl_dss_overlay_setup_du_manager(mgr);

		odin_dss_add_overlay_manager(mgr);

		r = kobject_init_and_add(&mgr->kobj, &manager_ktype,
				&pdev->dev.kobj, "manager%d", i);

		if (r) {
			DSSERR("failed to create sysfs file\n");
			continue;
		}

        mgr->vsync_ctrl = true;
	}

	pm_qos_add_request(&dss_mem_qos, PM_QOS_ODIN_MEM_MIN_FREQ, 0);
	pm_qos_add_request(&sync1_mem_qos, PM_QOS_ODIN_MEM_MIN_FREQ, 0);
	pm_qos_add_request(&sync2_mem_qos, PM_QOS_ODIN_MEM_MIN_FREQ, 0);

	return 0;
}

void odin_mgr_dss_uninit_overlay_managers(struct platform_device *pdev)
{
	struct odin_overlay_manager *mgr;

	while (!list_empty(&manager_list)) {
		mgr = list_first_entry(&manager_list,
				struct odin_overlay_manager, list);
		list_del(&mgr->list);
		kobject_del(&mgr->kobj);
		kobject_put(&mgr->kobj);
		kfree(mgr);
	}

	num_managers = 0;
}

int odin_mgr_dss_get_num_overlay_managers(void)
{
	return num_managers;
}
EXPORT_SYMBOL(odin_mgr_dss_get_num_overlay_managers);

struct odin_overlay_manager *odin_mgr_dss_get_overlay_manager(int num)
{
	int i = 0;
	struct odin_overlay_manager *mgr;

	list_for_each_entry(mgr, &manager_list, list) {
		if (i++ == num)
			return mgr;
	}

	return NULL;
}
EXPORT_SYMBOL(odin_mgr_dss_get_overlay_manager);

