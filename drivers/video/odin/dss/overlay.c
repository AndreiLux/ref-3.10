/*
 * /drivers/video/odin/dss/overlay.c
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

#define DSS_SUBSYS_NAME "OVERLAY"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <video/odindss.h>

#include "dss.h"
#include "dss-features.h"

static int num_overlays;
static struct list_head overlay_list;

/*  Sysfs */
static ssize_t overlay_name_show(struct odin_overlay *ovl, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", ovl->name);
}

static ssize_t overlay_manager_show(struct odin_overlay *ovl, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n",
			ovl->manager ? ovl->manager->name : "<none>");
}

static ssize_t overlay_manager_store(struct odin_overlay *ovl, const char *buf,
		size_t size)
{
#if 0
	int i, r;

	struct odin_overlay_manager *mgr = NULL;
	struct odin_overlay_manager *old_mgr;
	int len = size;

	if (buf[size-1] == '\n')
		--len;

	if (len > 0) {
		for (i = 0; i < odin_dss_mgr_get_num_overlay_managers(); ++i) {
			mgr = odin_dss_get_overlay_manager(i);

			if (sysfs_streq(buf, mgr->name))
				break;

			mgr = NULL;
		}
	}

	if (len > 0 && mgr == NULL)
		return -EINVAL;

	if (mgr)
		DSSDBG("manager %s found\n", mgr->name);

	if (mgr == ovl->manager)
		return size;

	old_mgr = ovl->manager;

	r = du_runtime_get();
	if (r)
		return r;

	/* detach old manager */
	if (old_mgr) {
		r = ovl->unset_manager(ovl);
		if (r) {
			DSSERR("detach failed\n");
			goto err;
		}

		r = old_mgr->apply(old_mgr);
		if (r)
			goto err;
	}

	if (mgr) {
		r = ovl->set_manager(ovl, mgr);
		if (r) {
			DSSERR("Failed to attach overlay\n");
			goto err;
		}

		r = mgr->apply(mgr);
		if (r)
			goto err;
	}

	du_runtime_put();

	return size;

err:

	du_runtime_put();
	return r;
#else
	return 0;
#endif
}

static ssize_t overlay_input_size_show(struct odin_overlay *ovl, char *buf)
{
	struct odin_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n",
			info.width, info.height);
}

static ssize_t overlay_screen_width_show(struct odin_overlay *ovl, char *buf)
{
	struct odin_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d\n", info.screen_width);
}

static ssize_t overlay_position_show(struct odin_overlay *ovl, char *buf)
{
	struct odin_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n",
			info.out_x, info.out_y);
}

static ssize_t overlay_position_store(struct odin_overlay *ovl,
		const char *buf, size_t size)
{
	int r;
	char *last;
	struct odin_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	info.out_x = simple_strtoul(buf, &last, 10);
	++last;
	if (last - buf >= size)
		return -EINVAL;

	info.out_y = simple_strtoul(last, &last, 10);

	r = ovl->set_overlay_info(ovl, &info);
	if (r)
		return r;

	if (ovl->manager) {
		r = ovl->manager->apply(ovl->manager, DSSCOMP_SETUP_MODE_DISPLAY);
		if (r)
			return r;
	}

	return size;
}

static ssize_t overlay_output_size_show(struct odin_overlay *ovl, char *buf)
{
	struct odin_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n",
			info.out_w, info.out_h);
}

static ssize_t overlay_output_size_store(struct odin_overlay *ovl,
		const char *buf, size_t size)
{
	int r;
	char *last;
	struct odin_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	info.out_w = simple_strtoul(buf, &last, 10);
	++last;
	if (last - buf >= size)
		return -EINVAL;

	info.out_h = simple_strtoul(last, &last, 10);

	r = ovl->set_overlay_info(ovl, &info);
	if (r)
		return r;

	if (ovl->manager) {
		r = ovl->manager->apply(ovl->manager,
					DSSCOMP_SETUP_MODE_DISPLAY);
		if (r)
			return r;
	}

	return size;
}

static ssize_t overlay_enabled_show(struct odin_overlay *ovl, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", ovl->info.enabled);
}

static ssize_t overlay_enabled_store(struct odin_overlay *ovl, const char *buf,
		size_t size)
{
	int r, enable;
	struct odin_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	r = kstrtoint(buf, 0, &enable);
	if (r)
		return r;

	info.enabled = !!enable;

	r = ovl->set_overlay_info(ovl, &info);
	if (r)
		return r;

	if (ovl->manager) {
		r = ovl->manager->apply(ovl->manager,
					DSSCOMP_SETUP_MODE_DISPLAY);
		if (r)
			return r;
	}

	return size;
}

static ssize_t overlay_global_alpha_show(struct odin_overlay *ovl, char *buf)
{
	struct odin_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			info.global_alpha);
}

static ssize_t overlay_global_alpha_store(struct odin_overlay *ovl,
		const char *buf, size_t size)
{
	int r;
	u8 alpha;
	struct odin_overlay_info info;

	r = kstrtou8(buf, 0, &alpha);
	if (r)
		return r;

	ovl->get_overlay_info(ovl, &info);

	info.global_alpha = alpha;

	r = ovl->set_overlay_info(ovl, &info);
	if (r)
		return r;

	if (ovl->manager) {
		r = ovl->manager->apply(ovl->manager,
					DSSCOMP_SETUP_MODE_DISPLAY);
		if (r)
			return r;
	}

	return size;
}

static ssize_t overlay_pre_mult_alpha_show(struct odin_overlay *ovl,
		char *buf)
{
	struct odin_overlay_info info;

	ovl->get_overlay_info(ovl, &info);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			info.pre_mult_alpha);
}

static ssize_t overlay_pre_mult_alpha_store(struct odin_overlay *ovl,
		const char *buf, size_t size)
{
	int r;
	u8 alpha;
	struct odin_overlay_info info;

	if ((ovl->caps & ODIN_DSS_OVL_CAP_PRE_MULT_ALPHA) == 0)
		return -ENODEV;

	r = kstrtou8(buf, 0, &alpha);
	if (r)
		return r;

	ovl->get_overlay_info(ovl, &info);

	info.pre_mult_alpha = alpha;

	r = ovl->set_overlay_info(ovl, &info);
	if (r)
		return r;

	if (ovl->manager) {
		r = ovl->manager->apply(ovl->manager,
					DSSCOMP_SETUP_MODE_DISPLAY);
		if (r)
			return r;
	}

	return size;
}


struct overlay_attribute {
	struct attribute attr;
	ssize_t (*show)(struct odin_overlay *, char *);
	ssize_t	(*store)(struct odin_overlay *, const char *, size_t);
};

#define OVERLAY_ATTR(_name, _mode, _show, _store) \
	struct overlay_attribute overlay_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)

static OVERLAY_ATTR(name, S_IRUGO, overlay_name_show, NULL);
static OVERLAY_ATTR(manager, S_IRUGO|S_IWUSR,
		overlay_manager_show, overlay_manager_store);
static OVERLAY_ATTR(input_size, S_IRUGO, overlay_input_size_show, NULL);
static OVERLAY_ATTR(screen_width, S_IRUGO, overlay_screen_width_show, NULL);
static OVERLAY_ATTR(position, S_IRUGO|S_IWUSR,
		overlay_position_show, overlay_position_store);
static OVERLAY_ATTR(output_size, S_IRUGO|S_IWUSR,
		overlay_output_size_show, overlay_output_size_store);
static OVERLAY_ATTR(enabled, S_IRUGO|S_IWUSR,
		overlay_enabled_show, overlay_enabled_store);
static OVERLAY_ATTR(global_alpha, S_IRUGO|S_IWUSR,
		overlay_global_alpha_show, overlay_global_alpha_store);
static OVERLAY_ATTR(pre_mult_alpha, S_IRUGO|S_IWUSR,
		overlay_pre_mult_alpha_show,
		overlay_pre_mult_alpha_store);

static struct attribute *overlay_sysfs_attrs[] = {
	&overlay_attr_name.attr,
	&overlay_attr_manager.attr,
	&overlay_attr_input_size.attr,
	&overlay_attr_screen_width.attr,
	&overlay_attr_position.attr,
	&overlay_attr_output_size.attr,
	&overlay_attr_enabled.attr,
	&overlay_attr_global_alpha.attr,
	&overlay_attr_pre_mult_alpha.attr,
	NULL
};

static ssize_t overlay_attr_show(struct kobject *kobj, struct attribute *attr,
		char *buf)
{
	struct odin_overlay *overlay;
	struct overlay_attribute *overlay_attr;

	overlay = container_of(kobj, struct odin_overlay, kobj);
	overlay_attr = container_of(attr, struct overlay_attribute, attr);

	if (!overlay_attr->show)
		return -ENOENT;

	return overlay_attr->show(overlay, buf);
}

static ssize_t overlay_attr_store(struct kobject *kobj, struct attribute *attr,
		const char *buf, size_t size)
{
	struct odin_overlay *overlay;
	struct overlay_attribute *overlay_attr;

	overlay = container_of(kobj, struct odin_overlay, kobj);
	overlay_attr = container_of(attr, struct overlay_attribute, attr);

	if (!overlay_attr->store)
		return -ENOENT;

	return overlay_attr->store(overlay, buf, size);
}

static const struct sysfs_ops overlay_sysfs_ops = {
	.show = overlay_attr_show,
	.store = overlay_attr_store,
};

static struct kobj_type overlay_ktype = {
	.sysfs_ops = &overlay_sysfs_ops,
	.default_attrs = overlay_sysfs_attrs,
};


struct odin_overlay *odin_ovl_dss_get_overlay(int num)
{
	int i = 0;
	struct odin_overlay *ovl;

	list_for_each_entry(ovl, &overlay_list, list) {
		if (i++ == num)
			return ovl;
	}

	return NULL;

}
EXPORT_SYMBOL(odin_ovl_dss_get_overlay);

static void odin_dss_add_overlay(struct odin_overlay *overlay)
{
	++num_overlays;
	list_add_tail(&overlay->list, &overlay_list);
}

int odin_ovl_dss_check_overlay(struct odin_overlay *ovl,
	struct odin_dss_device *dssdev)
{
	/* TODO: */
	return 0;
}

static void dss_ovl_get_overlay_info(struct odin_overlay *ovl,
	struct odin_overlay_info *info)
{
	*info = ovl->info;
}

static int odin_dss_set_manager(struct odin_overlay *ovl,
	struct odin_overlay_manager *mgr)
{
	if (!mgr)
		return -EINVAL;

	if (ovl->manager) {
		DSSERR("overlay '%s' already has a manager '%s'\n",
				ovl->name, ovl->manager->name);
		return -EINVAL;
	}

	if (ovl->info.enabled) {
		DSSERR("overlay has to be disabled to change the manager\n");
		return -EINVAL;
	}

	ovl->manager = mgr;

	/* odin_du_set_channel_out(ovl->id, mgr->id); */

	return 0;
}


static int odin_dss_unset_manager(struct odin_overlay *ovl)
{
	int r;

	if (!ovl->manager) {
		DSSERR("failed to detach overlay: manager not set\n");
		return -EINVAL;
	}

	if (ovl->info.enabled) {
		DSSERR("overlay has to be disabled to unset the manager\n");
		return -EINVAL;
	}

	r = ovl->manager->wait_for_vsync(ovl->manager);
	if (r)
		return r;

	ovl->manager = NULL;

	return 0;
}

int odin_ovl_dss_get_num_overlays(void)
{
	return num_overlays;
}
EXPORT_SYMBOL(odin_ovl_dss_get_num_overlays);

static struct odin_overlay *du_overlays[MAX_DSS_OVERLAYS];

void odin_ovl_dss_overlay_setup_du_manager(struct odin_overlay_manager *mgr)
{
	mgr->num_overlays = MAX_DSS_OVERLAYS;
	mgr->overlays = du_overlays;
}

void odin_ovl_dss_init_overlays(struct platform_device *pdev)
{
	int i, r;

	DSSDBG("odin_ovl_dss_init_overlays\n");

	INIT_LIST_HEAD(&overlay_list);

	for (i = 0; i < dss_feat_get_num_ovls(); ++i) {
		struct odin_overlay *ovl;
		ovl = kzalloc(sizeof(*ovl), GFP_KERNEL);
		BUG_ON(ovl == NULL);

		switch (i) {
		case ODIN_DSS_GRA0:
			ovl->name = "gra0";
			ovl->id = ODIN_DSS_GRA0;
			ovl->caps = ODIN_DSS_OVL_CAP_NONE;
			ovl->info.global_alpha = 255;
			ovl->info.zorder = ODIN_DSS_OVL_ZORDER_0; /* for FB0 */
			break;
		case ODIN_DSS_GRA1:
			ovl->name = "gra1";
			ovl->id = ODIN_DSS_GRA1;
			ovl->caps = ODIN_DSS_OVL_CAP_NONE;
			ovl->info.global_alpha = 255;
			ovl->info.zorder = ODIN_DSS_OVL_ZORDER_0;
			break;
		case ODIN_DSS_GRA2:
			ovl->name = "gra2";
			ovl->id = ODIN_DSS_GRA2;
			ovl->caps = ODIN_DSS_OVL_CAP_NONE;
			ovl->info.global_alpha = 255;
			ovl->info.zorder = ODIN_DSS_OVL_ZORDER_0;
			break;
		case ODIN_DSS_VID0:
			ovl->name = "vid0";
			ovl->id = ODIN_DSS_VID0;
			ovl->caps = ODIN_DSS_OVL_CAP_SCALE|
				    ODIN_DSS_OVL_CAP_XDENGINE;
			ovl->info.global_alpha = 255;
			ovl->info.zorder = ODIN_DSS_OVL_ZORDER_0;  /* for FB0 in FPGA Board */
			break;
		case ODIN_DSS_VID1:
			ovl->name = "vid1";
			ovl->id = ODIN_DSS_VID1;
			ovl->caps = ODIN_DSS_OVL_CAP_SCALE|
				    ODIN_DSS_OVL_CAP_XDENGINE;
			ovl->info.global_alpha = 255;
			break;
		case ODIN_DSS_VID2_FMT:
			ovl->name = "vid2";
			ovl->id = ODIN_DSS_VID2_FMT;
			ovl->caps = ODIN_DSS_OVL_CAP_NONE;
			ovl->info.global_alpha = 255;
			break;
 		case ODIN_DSS_mSCALER:
			ovl->name = "mScaler";
			ovl->id = ODIN_DSS_mSCALER;
			ovl->caps = ODIN_DSS_OVL_CAP_SCALE;  /* for FB1 */
			ovl->info.global_alpha = 255;
			break;

		}

		ovl->set_manager = &odin_dss_set_manager;
		ovl->unset_manager = &odin_dss_unset_manager;
		ovl->set_overlay_info = &odin_mgr_dss_ovl_set_info;
		ovl->get_overlay_info = &dss_ovl_get_overlay_info;

		ovl->supported_modes =
			dss_feat_get_supported_color_modes(ovl->id);

		odin_dss_add_overlay(ovl);

		r = kobject_init_and_add(&ovl->kobj, &overlay_ktype,
				&pdev->dev.kobj, "overlay%d", i);

		if (r) {
			DSSERR("failed to create sysfs file\n");
			continue;
		}

		du_overlays[i] = ovl;
	}
}



/* connect overlays to the new device, if not already connected. if force
 * selected, connect always. */
void odin_ovl_dss_recheck_connections(struct odin_dss_device *dssdev,
	bool force)
{
	int i;
	struct odin_overlay_manager *lcd0_mgr;
	struct odin_overlay_manager *lcd1_mgr = NULL;
	struct odin_overlay_manager *hdmi_mgr;
	struct odin_overlay_manager *mgr = NULL;

	lcd0_mgr = odin_mgr_dss_get_overlay_manager(ODIN_DSS_OVL_MGR_LCD0);
	lcd1_mgr = odin_mgr_dss_get_overlay_manager(ODIN_DSS_OVL_MGR_LCD1);
	hdmi_mgr = odin_mgr_dss_get_overlay_manager(ODIN_DSS_OVL_MGR_HDMI);

	if (dssdev->channel == ODIN_DSS_CHANNEL_LCD0) {
		if (!lcd0_mgr->device || force) {
			if (lcd0_mgr->device)
				lcd0_mgr->unset_device(lcd0_mgr);
			lcd0_mgr->set_device(lcd0_mgr, dssdev);
			mgr = lcd0_mgr;
		}
	} else if (dssdev->channel == ODIN_DSS_CHANNEL_LCD1) {
		if (!lcd1_mgr->device || force) {
			if (lcd1_mgr->device)
				lcd1_mgr->unset_device(lcd1_mgr);
			lcd1_mgr->set_device(lcd1_mgr, dssdev);
			mgr = lcd1_mgr;
		}
	} else if (dssdev->channel == ODIN_DSS_CHANNEL_HDMI) {
		if (!hdmi_mgr->device || force) {
			if (hdmi_mgr->device)
				hdmi_mgr->unset_device(hdmi_mgr);
			hdmi_mgr->set_device(hdmi_mgr, dssdev);
			mgr = hdmi_mgr;
		}
	}

	if (mgr && force) {
		/* du_runtime_get(); */

		for (i = 0; i < MAX_DSS_OVERLAYS; i++) {
			struct odin_overlay *ovl;
			if (i == EXTERNAL_DEFAULT_OVERLAY)
				continue;
			ovl = odin_ovl_dss_get_overlay(i);
			if (!ovl->manager || force) {
				if (ovl->manager)
					odin_dss_unset_manager(ovl);
				odin_dss_set_manager(ovl, mgr);
			}
		}

		/*  du_runtime_put(); */
	}
}

void odin_ovl_dss_uninit_overlays(struct platform_device *pdev)
{
	struct odin_overlay *ovl;

	while (!list_empty(&overlay_list)) {
		ovl = list_first_entry(&overlay_list,
				struct odin_overlay, list);
		list_del(&ovl->list);
		kobject_del(&ovl->kobj);
		kobject_put(&ovl->kobj);
		kfree(ovl);
	}

	num_overlays = 0;
}

