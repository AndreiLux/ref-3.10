/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Younghyun Jo <younghyun.jo@lge.com>
 * Youngki Lyu <youngki.lyu@lge.com>
 * 
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License 
 * version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 */
#include <asm/io.h>
#include <asm/uaccess.h>

#include <linux/fb.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>

#include <video/odin-dss/dss_fb_io.h>
#include "hal/mipi_dsi_common.h"
#include "hal/sync_hal.h"
#include "hal/ovl_hal.h"

#include "mipi_dsi_drv.h"
#include "dip_mipi.h"
#include "sync_src.h"
#include "ovl_rsc.h"
#include "vevent.h"

//#define ANDROID_DISABLE

struct dssfb_dev_vevent_node
{
	struct list_head node;
	void *vevent_id;
};

struct dssfb_dev
{
	enum sync_ch_id sync;
	enum ovl_ch_id ovl;
	struct dssfb_dev_entry entry;

	spinlock_t lock_vevent_id_head;
	struct list_head vevent_id_head;

	int fb_blank;
};

struct dssfb_desc
{
	struct dssfb_dev *dssfb_dev[DSS_DISPLAY_PORT_MEM];
};
static struct dssfb_desc *desc = NULL;

bool dssfb_get_ovl_sync(enum dss_display_port port, enum ovl_ch_id *ovl_ch, enum sync_ch_id *sync_ch, bool *command_mode)
{
	struct dssfb_dev *dssfb_dev;

	if  (port > DSS_DISPLAY_PORT_MEM || ovl_ch == NULL || sync_ch == NULL) {
		pr_err("invalid port(%d), ovl(0x%X), sync(0x%X)\n", port, (unsigned int)ovl_ch, (unsigned int)sync_ch);
		return false;
	}

	if (port < DSS_DISPLAY_PORT_MEM && desc->dssfb_dev[port] == NULL) {
		*ovl_ch = OVL_CH_INVALID;
		*sync_ch = SYNC_CH_INVALID;
		pr_err("dssfb_dev == NULL\n");
		return false;
	}

	switch(port)
	{
	case DSS_DISPLAY_PORT_FB0:
	case DSS_DISPLAY_PORT_FB1:
		dssfb_dev = desc->dssfb_dev[port];
		break;

	case DSS_DISPLAY_PORT_MEM:
		if (desc->dssfb_dev[DSS_DISPLAY_PORT_FB0] && desc->dssfb_dev[DSS_DISPLAY_PORT_FB1]) 
			dssfb_dev = desc->dssfb_dev[DSS_DISPLAY_PORT_FB0];
		else 
			dssfb_dev = desc->dssfb_dev[DSS_DISPLAY_PORT_FB0] ? 
				desc->dssfb_dev[DSS_DISPLAY_PORT_FB1] : desc->dssfb_dev[DSS_DISPLAY_PORT_FB0];
		break;
	default:
		pr_err("invalid port(%d)\n", port);
		return false;
	}

	*ovl_ch = dssfb_dev->ovl;
	*sync_ch = dssfb_dev->sync;
	*command_mode = dssfb_dev->entry.is_command_mode();

	return true;
}

static int dssfb_blank(int blank, struct fb_info *info)
{
	struct dssfb_dev *dev = info->par;

	if(dev->fb_blank == blank) {
		pr_err("%s: occurred same blank operation(%d)\n",__func__,blank);
		return 0;
	}

	if (dev->entry.blank)
		dev->entry.blank(blank);

	if (blank) {
		ovl_rsc_disable(dev->ovl);
/* screen: unblanked, hsync: on,  vsync: on */
		dev->fb_blank = FB_BLANK_POWERDOWN;
	} else {
		ovl_rsc_enable(dev->ovl);
/* screen: blanked,   hsync: off, vsync: off */
		dev->fb_blank = FB_BLANK_UNBLANK;
	}
	return 0;
}

static int dssfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	if (var->xres != info->var.xres)
		return -1;
	if (var->yres != info->var.yres)
		return -1;

	if (var->xres_virtual != info->var.xres_virtual)
		return -1;
	if (var->yres_virtual != info->var.yres_virtual)
		return -1;

	if (var->bits_per_pixel != info->var.bits_per_pixel)
		return -1;

	if (var->red.offset != info->var.red.offset)
		return -1;
	if (var->red.length != info->var.red.length)
		return -1;
	if (var->green.offset != info->var.green.offset)
		return -1;
	if (var->green.length != info->var.green.length)
		return -1;
	if (var->blue.offset != info->var.blue.offset)
		return -1;
	if (var->blue.length != info->var.blue.length)
		return -1;

	if (var->pixclock != info->var.pixclock)
		return -1;
	if (var->left_margin != info->var.left_margin)
		return -1;
	if (var->right_margin != info->var.right_margin)
		return -1;
	if (var->upper_margin != info->var.upper_margin)
		return -1;
	if (var->lower_margin != info->var.lower_margin)
		return -1;
	if (var->hsync_len != info->var.hsync_len)
		return -1;
	if (var->vsync_len != info->var.vsync_len)
		return -1;

	return 0;
}

static void dssfb_sync_isr(enum sync_ch_id sync_ch, void *arg)
{
	struct fb_info *info = arg;
	struct dssfb_dev *dev = info->par;
	unsigned long flags;
	struct dssfb_dev_vevent_node *node, *tmp;
	struct dss_fb_io_vsync_cb_data cb_data;

	if (list_empty(&dev->vevent_id_head)) {
		//pr_info("vevent_id is not set yet\n");
		return;
	}

	cb_data.timestamp = jiffies * TICK_NSEC;
	cb_data.sync_tick = jiffies * TICK_NSEC;
	cb_data.port = info->node;

	spin_lock_irqsave(&dev->lock_vevent_id_head, flags);
	list_for_each_entry_safe(node, tmp, &dev->vevent_id_head, node)
		if (vevent_send(node->vevent_id, &cb_data, sizeof(struct dss_fb_io_vsync_cb_data)) == -ENOTCONN) {
			list_del(&node->node);
			vfree(node);
		}
	spin_unlock_irqrestore(&dev->lock_vevent_id_head, flags);
}

static int dss_fb_io_vevent_id_set(struct fb_info *info, unsigned long arg)
{
	int ret;
	struct dss_fb_io_vevent_id_set a;
	struct dssfb_dev *dev;
	struct dssfb_dev_vevent_node *vevent_node;
	unsigned long flags;

	ret = copy_from_user(&a, (void*)arg, sizeof(struct dss_fb_io_vevent_id_set));
	if (ret != 0) {
		pr_err("copy_from_user failed\n");
		return -EINVAL;
	}

	if (a.vevent_id_vsync == NULL) {
		pr_err("vevent_id is NULL\n");
		return -EINVAL;
	}

	vevent_node = vzalloc(sizeof(struct dssfb_dev_vevent_node));
	if (vevent_node == NULL) {
		pr_err("vzalloc failed\n");
		return -1;
	}

	vevent_node->vevent_id = a.vevent_id_vsync;

	dev = info->par;

	spin_lock_irqsave(&dev->lock_vevent_id_head, flags);
	list_add_tail(&vevent_node->node, &dev->vevent_id_head);
	spin_unlock_irqrestore(&dev->lock_vevent_id_head, flags);

	return 0;
}

static int (*dss_fb_io_fp[])(struct fb_info *info, unsigned long arg) = 
{
	dss_fb_io_vevent_id_set,
};

static int dssfb_ioctl(struct fb_info *info, unsigned cmd, unsigned long arg)
{
	int errno = 0;
	unsigned long nr = _IOC_NR(cmd);

	if (nr < sizeof(dss_fb_io_fp)/sizeof(int*) && dss_fb_io_fp[nr] != NULL)
		errno = dss_fb_io_fp[nr](info, arg);
	else 
		errno = -EPERM;

	if (errno < 0)
		pr_err("dss_fb_io failed. nr:%lu\n", nr);

	return errno;
}

static int dssfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	pr_info("%s\n", __func__);

	return 0;
}

static struct fb_ops fb_ops = 
{
	.fb_blank = dssfb_blank,
	.fb_check_var = dssfb_check_var,
	.fb_ioctl = dssfb_ioctl,
	.fb_pan_display = dssfb_pan_display,
};

bool dssfb_dev_register(enum dip_data_path dip, struct dssfb_dev_entry *entry)
{
	struct fb_info *fb_info = NULL;
	struct dssfb_dev *dssfb_dev = NULL;
	int cmap_len = 256;
	struct dss_image_size panel_size;

	//TODO entry validity check;

	fb_info = framebuffer_alloc(sizeof(struct dssfb_dev), NULL);
	if (fb_info == NULL) {
		pr_err("failed to framebuffer_alloc\n");
		goto err_framebuffer_alloc;
	}

	fb_info->screen_base = 0;
	fb_info->fbops = &fb_ops;
	fb_info->flags = 0;

	fb_info->var.xres = entry->panel.xres;
	fb_info->var.yres = entry->panel.yres;
	fb_info->var.xres_virtual = entry->panel.xres;
	fb_info->var.yres_virtual = entry->panel.yres;
	fb_info->var.xoffset = 0;
	fb_info->var.yoffset = 0;
	fb_info->var.bits_per_pixel = 32; /* argb888 */
	fb_info->var.grayscale = 0;
	fb_info->var.red.offset = 16;
	fb_info->var.red.length  = 8;
	fb_info->var.green.offset = 8;
	fb_info->var.green.length  = 8;
	fb_info->var.blue.offset = 0;
	fb_info->var.blue.length  = 8;
	fb_info->var.nonstd = 0;
	fb_info->var.activate = FB_ACTIVATE_NOW;
	fb_info->var.height = entry->panel.height;
	fb_info->var.width = entry->panel.width;
	fb_info->var.pixclock = entry->panel.pixel_clock;
	fb_info->var.left_margin = entry->panel.left_margin;
	fb_info->var.right_margin = entry->panel.right_margin;
	fb_info->var.upper_margin = entry->panel.upper_margin;
	fb_info->var.lower_margin = entry->panel.lower_margin;
	fb_info->var.hsync_len = entry->panel.hsync_len;
	fb_info->var.vsync_len = entry->panel.vsync_len;

	if (entry->panel.hsync_polarity == DSSFB_HSYNC_POSSITIVE)
		fb_info->var.sync |= FB_SYNC_HOR_HIGH_ACT;
	if (entry->panel.vsync_polarity == DSSFB_VSYNC_POSSITIVE)
		fb_info->var.sync |= FB_SYNC_VERT_HIGH_ACT;
	fb_info->var.vmode = FB_VMODE_NONINTERLACED;

#ifdef ANDROID_DISABLE
	fb_info->fix.smem_len = 0;
#else
	fb_info->fix.smem_len = 4096;
	fb_info->fix.smem_start = kzalloc(fb_info->fix.smem_len, GFP_KERNEL);
#endif
	fb_info->fix.type = FB_TYPE_PACKED_PIXELS;
	fb_info->fix.visual = FB_VISUAL_TRUECOLOR;
	fb_info->fix.line_length = entry->panel.bpp * entry->panel.xres; 

	dssfb_dev = fb_info->par;
	memset(dssfb_dev, 0, sizeof(struct dssfb_dev));
	spin_lock_init(&dssfb_dev->lock_vevent_id_head);
	INIT_LIST_HEAD(&dssfb_dev->vevent_id_head);

	switch (fb_info->node)
	{
	case DSS_DISPLAY_PORT_FB0:	// /dev/fb0
	case DSS_DISPLAY_PORT_FB1:	// /dev/fb1
		desc->dssfb_dev[fb_info->node] = dssfb_dev;
		break;
	default:
		pr_err("[dssfb] Out of capability\n");
		goto err_register_framebuffer;
	}

	switch(dip)
	{
	case DIP0_LCD:
		dssfb_dev->sync = SYNC_CH_ID0;
		dssfb_dev->ovl = OVL_CH_ID0;
		break;
	case DIP1_LCD:
		dssfb_dev->sync = SYNC_CH_ID1;
		dssfb_dev->ovl = OVL_CH_ID1;
		break;
	case DIP_HDMI:
		dssfb_dev->sync = SYNC_CH_ID2;
		dssfb_dev->ovl = OVL_CH_ID2;
		break;
	default:
		pr_err("invalid dip(%d)\n", dip);
		goto err_register_framebuffer;
	}

	dssfb_dev->fb_blank = FB_BLANK_POWERDOWN;

	memcpy(&dssfb_dev->entry, entry, sizeof(struct dssfb_dev_entry));

	if (fb_alloc_cmap(&fb_info->cmap, cmap_len, 0) < 0) {
		pr_err("[dssfb] fb_alloc_cmap failed\n");
		//TODO ERR HANDLING
	}

	if (register_framebuffer(fb_info) < 0) {
		pr_err("[dssfb] register_framebuffer failed\n");
		goto err_register_framebuffer;
	}

	panel_size.w = entry->panel.xres;
	panel_size.h = entry->panel.yres;
	ovl_rsc_set(dssfb_dev->ovl, &panel_size, 0, 0, 0);

	sync_isr_register(dssfb_dev->sync, fb_info, dssfb_sync_isr);
	sync_enable(dssfb_dev->sync, 0);

	return true;

err_register_framebuffer:
	framebuffer_release(fb_info);

err_framebuffer_alloc:

	return false;
}

void dssfb_dev_unregister(enum dip_data_path dip)
{
#if 0
	TODO call list_del();
#endif

	return;
}

int dssfb_init(struct platform_device *pdev_dip[])
{
	desc = vzalloc(sizeof(struct dssfb_desc));
	if (desc == NULL) {
		pr_err("vzalloc failed\n");
		return -ENOMEM;
	}

	if (dss_dip_mipi_init(pdev_dip, dssfb_dev_register, dssfb_dev_unregister) == false) {
		pr_err("dss_dip_mipi_init failed\n");
		vfree(desc);
		return -1;
	}

	return 0;
}

void dssfb_cleanup(void)
{
	vfree(desc);
}

