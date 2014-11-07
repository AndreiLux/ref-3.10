/*
 * linux/drivers/video/odin2/dsscomp/base.c
 *
 * DSS Composition basic operation support
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

#include <linux/notifier.h>
#include <linux/ion.h>
#include <linux/odin_iommu.h>

#include <video/odindss.h>

#include "dsscomp.h"
#include "../xdengine/mxd.h"
#include "../dss/dss.h"
#include <linux/fb.h>

int debug;
#if 0
module_param(debug, int, 0644);

/* color formats supported - bitfield info is used for truncation logic */
static const struct color_info {
	int a_ix, a_bt;	/* bitfields */
	int r_ix, r_bt;
	int g_ix, g_bt;
	int b_ix, b_bt;
	int x_bt;
	enum odin_color_mode mode;
	const char *name;
} fmts[2][16] = { {
	{ 0,  0, 0,  0, 0,  0, 0, 0, 1, ODIN_DSS_COLOR_CLUT1, "BITMAP1" },
	{ 0,  0, 0,  0, 0,  0, 0, 0, 2, ODIN_DSS_COLOR_CLUT2, "BITMAP2" },
	{ 0,  0, 0,  0, 0,  0, 0, 0, 4, ODIN_DSS_COLOR_CLUT4, "BITMAP4" },
	{ 0,  0, 0,  0, 0,  0, 0, 0, 8, ODIN_DSS_COLOR_CLUT8, "BITMAP8" },
	{ 0,  0, 8,  4, 4,  4, 0, 4, 4, ODIN_DSS_COLOR_RGB12U, "xRGB12-4444" },
	{ 12, 4, 8,  4, 4,  4, 0, 4, 0, ODIN_DSS_COLOR_ARGB16, "ARGB16-4444" },
	{ 0,  0, 11, 5, 5,  6, 0, 5, 0, ODIN_DSS_COLOR_RGB16, "RGB16-565" },
	{ 15, 1, 10, 5, 5,  5, 0, 5, 0, ODIN_DSS_COLOR_ARGB16_1555,
								"ARGB16-1555" },
	{ 0,  0, 16, 8, 8,  8, 0, 8, 8, ODIN_DSS_COLOR_RGB24U, "xRGB24-8888" },
	{ 0,  0, 16, 8, 8,  8, 0, 8, 0, ODIN_DSS_COLOR_RGB24P, "RGB24-888" },
	{ 0,  0, 12, 4, 8,  4, 4, 4, 4, ODIN_DSS_COLOR_RGBX16, "RGBx12-4444" },
	{ 0,  4, 12, 4, 8,  4, 4, 4, 0, ODIN_DSS_COLOR_RGBA16, "RGBA16-4444" },
	{ 24, 8, 16, 8, 8,  8, 0, 8, 0, ODIN_DSS_COLOR_ARGB32, "ARGB32-8888" },
	{ 0,  8, 24, 8, 16, 8, 8, 8, 0, ODIN_DSS_COLOR_RGBA32, "RGBA32-8888" },
	{ 0,  0, 24, 8, 16, 8, 8, 8, 8, ODIN_DSS_COLOR_RGBX32, "RGBx24-8888" },
	{ 0,  0, 10, 5, 5,  5, 0, 5, 1, ODIN_DSS_COLOR_XRGB16_1555,
								"xRGB15-1555" },
}, {
	{ 0,  0, 0,  0, 0,  0, 0, 0, 12, ODIN_DSS_COLOR_NV12, "NV12" },
	{ 0,  0, 12, 4, 8,  4, 4, 4, 4, ODIN_DSS_COLOR_RGBX16, "RGBx12-4444" },
	{ 0,  4, 12, 4, 8,  4, 4, 4, 0, ODIN_DSS_COLOR_RGBA16, "RGBA16-4444" },
	{ 0,  0, 0,  0, 0,  0, 0, 0, 0, 0, "invalid" },
	{ 0,  0, 8,  4, 4,  4, 0, 4, 4, ODIN_DSS_COLOR_RGB12U, "xRGB12-4444" },
	{ 12, 4, 8,  4, 4,  4, 0, 4, 0, ODIN_DSS_COLOR_ARGB16, "ARGB16-4444" },
	{ 0,  0, 11, 5, 5,  6, 0, 5, 0, ODIN_DSS_COLOR_RGB16, "RGB16-565" },
	{ 15, 1, 10, 5, 5,  5, 0, 5, 0, ODIN_DSS_COLOR_ARGB16_1555,
								"ARGB16-1555" },
	{ 0,  0, 16, 8, 8,  8, 0, 8, 8, ODIN_DSS_COLOR_RGB24U, "xRGB24-8888" },
	{ 0,  0, 16, 8, 8,  8, 0, 8, 0, ODIN_DSS_COLOR_RGB24P, "RGB24-888" },
	{ 0,  0, 0,  0, 0,  0, 0, 0, 16, ODIN_DSS_COLOR_YUV2, "YUYV" },
	{ 0,  0, 0,  0, 0,  0, 0, 0, 16, ODIN_DSS_COLOR_UYVY, "UYVY" },
	{ 24, 8, 16, 8, 8,  8, 0, 8, 0, ODIN_DSS_COLOR_ARGB32, "ARGB32-8888" },
	{ 0,  8, 24, 8, 16, 8, 8, 8, 0, ODIN_DSS_COLOR_RGBA32, "RGBA32-8888" },
	{ 0,  0, 24, 8, 16, 8, 8, 8, 8, ODIN_DSS_COLOR_RGBX32, "RGBx24-8888" },
	{ 0,  0, 10, 5, 5,  5, 0, 5, 1, ODIN_DSS_COLOR_XRGB16_1555,
								"xRGB15-1555" },
} };

static const struct color_info *get_color_info(enum odin_color_mode mode)
{
	int i;
	for (i = 0; i < sizeof(fmts) / sizeof(fmts[0][0]); i++)
		if (fmts[0][i].mode == mode)
			return fmts[0] + i;
	return NULL;
}

static int color_mode_to_bpp(enum odin_color_mode color_mode)
{
	const struct color_info *ci = get_color_info(color_mode);
	BUG_ON(!ci);

	return ci->a_bt + ci->r_bt + ci->g_bt + ci->b_bt + ci->x_bt;
}

#ifdef CONFIG_DEBUG_FS
const char *dsscomp_get_color_name(enum odin_color_mode m)
{
	const struct color_info *ci = get_color_info(m);
	return ci ? ci->name : NULL;
}
#endif

#endif

union rect {
	struct {
		s32 x;
		s32 y;
		s32 w;
		s32 h;
	};
	struct {
		s32 xy[2];
		s32 wh[2];
	};
	struct dss_rect_t r;
};

int cal_uv_offset(enum odin_color_mode color_mode, u32 width, u32 height,
						u32 y_addr, u32 *get_u, u32 *get_v)
{
/*	ODIN_DSS_COLOR_YUV422P_2 = 0x8,  // YUV422 2PLANE
	ODIN_DSS_COLOR_YUV224P_2 = 0x9,  // YUV224 2PLANE
	ODIN_DSS_COLOR_YUV420P_2 = 0xa,  // YUV420 2PLANE
	ODIN_DSS_COLOR_YUV444P_2 = 0xb,  // YUV444 2PLANE
	ODIN_DSS_COLOR_YUV422P_3 = 0xc,  // YUV422 3PLANE
	ODIN_DSS_COLOR_YUV224P_3 = 0xd,  // YUV224 3PLANE
	ODIN_DSS_COLOR_YUV420P_3 = 0xe,  // YUV420 3PLANE
	ODIN_DSS_COLOR_YUV444P_3 = 0xf,  // YUV444 3PLANE*/

	u32 offset = width * height;

	switch (CHECK_COLOR_VALUE(color_mode, ODIN_COLOR_MODE))
	{
		case ODIN_DSS_COLOR_YUV444P_3:
			*get_v = y_addr + (offset<<1);
			break;

		case ODIN_DSS_COLOR_YUV420P_3:
			*get_v = y_addr + offset + (offset>>2);
			break;

		case ODIN_DSS_COLOR_YUV224P_3:
			*get_v = y_addr + offset + (offset>>1);
			break;

		case ODIN_DSS_COLOR_YUV422P_3:
		case ODIN_DSS_COLOR_YUV444P_2:
		case ODIN_DSS_COLOR_YUV420P_2:
		case ODIN_DSS_COLOR_YUV224P_2:
		case ODIN_DSS_COLOR_YUV422P_2:
			break;

		default:
			goto fail;
	}
	*get_u = y_addr + offset;

	return 0;
fail:
	return -1;
}

int crop_to_rect(union rect *crop, union rect *win, union rect *vis,
						int rotation, int mirror)
{
	int c, swap = rotation & 1;

	/* align crop window with display coordinates */
	if (swap)
		crop->y -= (crop->h = -crop->h);
	if (rotation & 2)
		crop->xy[!swap] -= (crop->wh[!swap] = -crop->wh[!swap]);
	if ((!mirror) ^ !(rotation & 2))
		crop->xy[swap] -= (crop->wh[swap] = -crop->wh[swap]);

	for (c = 0; c < 2; c++) {
		/* see if complete buffer is outside the vis or it is
		   fully cropped or scaled to 0 */
		if (win->wh[c] <= 0 || vis->wh[c] <= 0 ||
		    win->xy[c] + win->wh[c] <= vis->xy[c] ||
		    win->xy[c] >= vis->xy[c] + vis->wh[c] ||
		    !crop->wh[c ^ swap])
			return -ENOENT;

		/* crop left/top */
		if (win->xy[c] < vis->xy[c]) {
			/* correction term */
			int a = (vis->xy[c] - win->xy[c]) *
						crop->wh[c ^ swap] / win->wh[c];
			crop->xy[c ^ swap] += a;
			crop->wh[c ^ swap] -= a;
			win->wh[c] -= vis->xy[c] - win->xy[c];
			win->xy[c] = vis->xy[c];
		}
		/* crop right/bottom */
		if (win->xy[c] + win->wh[c] > vis->xy[c] + vis->wh[c]) {
			crop->wh[c ^ swap] = crop->wh[c ^ swap] *
				(vis->xy[c] + vis->wh[c] - win->xy[c]) /
								win->wh[c];
			win->wh[c] = vis->xy[c] + vis->wh[c] - win->xy[c];
		}

		if (!crop->wh[c ^ swap] || !win->wh[c])
			return -ENOENT;
	}

	/* realign crop window to buffer coordinates */
	if (rotation & 2)
		crop->xy[!swap] -= (crop->wh[!swap] = -crop->wh[!swap]);
	if ((!mirror) ^ !(rotation & 2))
		crop->xy[swap] -= (crop->wh[swap] = -crop->wh[swap]);
	if (swap)
		crop->y -= (crop->h = -crop->h);
	return 0;
}

int set_dss_ovl_info(struct dss_ovl_info *oi, u64 debug_addr1,
	enum dsscomp_setup_mode mode)
{
	struct odin_overlay_info info;
	struct odin_overlay *ovl;
	struct dss_ovl_cfg *cfg;
	union rect crop, win, vis;
	int out_w, out_h;
	struct fb_info *fbi = NULL;
 	u32 color_fmt=0, uv_plane_swap;

	/* check overlay number */
	if (!oi || oi->cfg.ix >= odin_ovl_dss_get_num_overlays())
		return -EINVAL;
	cfg = &oi->cfg;
	ovl = odin_ovl_dss_get_overlay(cfg->ix);

	/* just in case there are new fields, we get the current info */
	ovl->get_overlay_info(ovl, &info);

	info.enabled = cfg->enabled;
	if (!cfg->enabled)
	{
		if((cfg->ix == ODIN_DSS_VID0_RES) || (cfg->ix == ODIN_DSS_VID1_RES))
		{
			if(info.mxd_use)
				info.mxd_use = false;
		}

		goto done;
	}

#if 0
	DSSINFO("zorder = %d, blending = %d, premult = %d, global_alpha = %d \n",
		cfg->zorder, cfg->blending, cfg->pre_mult_alpha, cfg->global_alpha);
#endif

	info.invisible = cfg->invisible;

	/* copied params */
	info.zorder = cfg->zorder;

	info.formatter_use = cfg->formatter_use;

	if (cfg->zonly)
		goto done;

	info.blending =	cfg->blending;
	info.global_alpha = cfg->global_alpha;
	info.pre_mult_alpha = cfg->pre_mult_alpha;
	info.rotation = cfg->rotation;
	info.mirror = cfg->mirror;
	info.color_mode = cfg->color_mode;

	/* crop to screen */
	crop.r = cfg->crop;
	win.r = cfg->win;
	vis.x = vis.y = 0;
	vis.w = ovl->manager->device->panel.timings.x_res;
	vis.h = ovl->manager->device->panel.timings.y_res;

	info.width  = cfg->width;
	info.height = cfg->height;

	if (cfg->crop.w != 0)
	{
		info.crop_x = cfg->crop.x;
		info.crop_y = cfg->crop.y;
		out_w = info.crop_w = cfg->crop.w;
		out_h = info.crop_h = cfg->crop.h;
	}
	else
	{
		info.crop_x = 0;
		info.crop_y = 0;
		out_w = info.crop_w = cfg->width;
		out_h = info.crop_h = cfg->height;
	}

#if 0 /* No rotation */
	if (cfg->rotation & 1)
		/* DISPC uses swapped height/width for 90/270 degrees */
		swap(info.width, info.height);
#endif

	if (ovl->manager->mgr_flip & ODIN_DSS_V_FLIP)
		info.out_y = ovl->manager->info.size_y - (cfg->win.y + cfg->win.h);
	else
		info.out_y = cfg->win.y;

	if (ovl->manager->mgr_flip & ODIN_DSS_H_FLIP)
		info.out_x = ovl->manager->info.size_x -  (cfg->win.x + cfg->win.w);
	else
		info.out_x = cfg->win.x;

	if (cfg->win.w != 0)
	{
		info.out_w = cfg->win.w;
		info.out_h = cfg->win.h;
	}
	else
	{
		info.out_w = out_w;
		info.out_h = out_h;
	}

	if (info.crop_w == info.out_w && info.crop_h == info.out_h)
		info.gscl_en = false;
	else
		info.gscl_en = true;

	if(cfg->mxd_use)
	{
		if ((get_mxd_pd_status()) && (get_mxd_clk_status()))
		{
			if (oi->cfg.ix == ODIN_DSS_VID0_RES
               		|| oi->cfg.ix == ODIN_DSS_VID1_RES)
			{
				if( (info.crop_w % 4) || (info.crop_h % 4) )
				{
					printk("*** info.crop_w = %d, info.crop_h = %d \n",
						info.crop_w, info.crop_h);

					info.mxd_use = false;
				}

				else if( info.out_w % 4 )
				{
					printk("+++ info.out_w = %d, info.out_h = %d \n",
						info.out_w, info.out_h);

					info.mxd_use = false;
				}
				else
				{
					if (odin_mxd_adaptedness((enum odin_resource_index)oi->cfg.ix,
							info.color_mode, info.out_w, info.out_h, info.gscl_en,
							info.mxd_scenario=MXDDU_SCENARIO_TEST1) == 0)
					{
						info.mxd_use = true;
						odin_mxd_enable(true);
					}
				}
			}
		}
		else
			info.mxd_use = false;
	}
	else
	{
		info.mxd_use = false;
	}

#if 0 /*  only test */
	if (cfg->mxd_use)
	{

		if (odin_mxd_adaptedness((enum odin_resource_index)oi->cfg.ix,
				info.color_mode, info.out_w, info.out_h, info.gscl_en,
				info.mxd_scenario=0) == 0)
		{
			info.mxd_use = true;
			odin_mxd_enable(true);
			info.level_data.ce_en = true;
			info.level_data.ce_level = LEVEL_STRONG;
			info.level_data.nr_en = false;
			info.level_data.nr_level = LEVEL_MEDIUM;
			info.level_data.re_en = false;
			info.level_data.re_level = LEVEL_MEDIUM;
			odin_mxd_algorithm_setup(&info.level_data);
		}
	}
#endif

	if (oi->addr_type == ODIN_DSS_BUFADDR_ION)
	{
#ifdef ODIN_ION_FD
 		if (oi->fd)
 			info.p_y_addr = oi->fd;
		else
			goto err;
		info.debug_addr1 =debug_addr1;;
#endif
 	}
	else if (oi->addr_type == ODIN_DSS_BUFADDR_FB)
	{
		fbi = registered_fb[0];
		info.p_y_addr = (u32) fbi->fix.smem_start + (u32) oi->addr;
		info.debug_addr1 = (u32)fbi->screen_base + (u32) oi->addr;
	}
	else
	{
		info.p_y_addr = (u32)oi->addr;
	}

	color_fmt = CHECK_COLOR_VALUE(oi->cfg.color_mode, ODIN_COLOR_MODE);
	uv_plane_swap = CHECK_COLOR_VALUE(oi->cfg.color_mode,
						ODIN_COLOR_PLANE_UV_SWAP);

	/* calculate addresses and cropping */
	if (color_fmt >= ODIN_DSS_COLOR_YUV422P_2 &&
			color_fmt <= ODIN_DSS_COLOR_YUV444P_3)
	{
		if (uv_plane_swap)
			cal_uv_offset(color_fmt, oi->cfg.width, oi->cfg.height,
					(u32)info.p_y_addr, &info.p_v_addr, &info.p_u_addr);
		else
			cal_uv_offset(color_fmt, oi->cfg.width, oi->cfg.height,
					(u32)info.p_y_addr, &info.p_u_addr, &info.p_v_addr);
	}

	/* no rotation on DMA buffer */
	if (cfg->rotation & 3 || cfg->mirror)
		return -EINVAL;

done:
	/* set overlay info */
	return ovl->set_overlay_info(ovl, &info);

err:
	/* DSSERR("get ion_addr is null \n");*/
	return -EFAULT;
}

int set_dss_wb_info(struct dss_wb_info *wi, u64 debug_addr1,
	enum dsscomp_setup_mode mode)
{
	struct odin_writeback_info info;
	struct odin_writeback *wb;
	struct dss_wb_cfg *cfg;
	int wb_ix;
	struct fb_info *fbi = NULL;

	/* Wrteback is not supported in YUV420_3P, YUV422_3P, so dummy v_addr */
	u32 p_v_addr;

	/* check overlay number */
	if (!wi || wi->cfg.ix > ODIN_WB_mSCALER)
		return -EINVAL;

	cfg = &wi->cfg;
	wb_ix = cfg->ix;
	wb = odin_dss_get_writeback(wb_ix);

	/* just in case there are new fields, we get the current info */
	wb->get_wb_info(wb, &info);

	info.enabled = cfg->enabled;

	if (mode & DSSCOMP_SETUP_MODE_CAPTURE)
		info.mode = ODIN_WB_CAPTURE_MODE;
	else if (mode & DSSCOMP_SETUP_MODE_MEM2MEM)
		info.mode = ODIN_WB_MEM2MEM_MODE;
	else if (mode & DSSCOMP_SETUP_MODE_DISPLAY_MEM2MEM)
		info.mode = ODIN_WB_DISPLAY_MEM2MEM_MODE;
	else
		return -EINVAL;

	if (!cfg->enabled)
	{
		DSSDBG("set_dss_wb_info is disabled \n");
		goto done;
	}

	info.color_mode = cfg->color_mode;

	/* crop to screen */
	info.out_x = cfg->win.x;
	info.out_y = cfg->win.y;
	info.out_w = cfg->win.w;
	info.out_h = cfg->win.h;

	info.width  = cfg->width;
	info.height = cfg->height;

	info.channel = cfg->mgr_ix;

	/* calculate addresses and cropping */
	if (wi->addr_type == ODIN_DSS_BUFADDR_ION)
	{
#ifdef ODIN_ION_FD
		if (wi->fd)
			info.p_y_addr = wi->fd;
		else
			goto err;

		info.debug_addr1 = debug_addr1;
#endif
	}
	else if (wi->addr_type == ODIN_DSS_BUFADDR_FB)
	{
		fbi = registered_fb[0];
		info.p_y_addr = (u32) fbi->fix.smem_start + (u32) wi->addr;
		info.debug_addr1 = (u32)fbi->screen_base + (u32) wi->addr;
	}
	else
	{
		info.p_y_addr = (u32)wi->addr;
	}

	/* calculate addresses and cropping */
	if (wi->cfg.color_mode >= ODIN_DSS_COLOR_YUV422P_2 &&
		wi->cfg.color_mode <= ODIN_DSS_COLOR_YUV444P_3)
	{
		cal_uv_offset(wi->cfg.color_mode, wi->cfg.width, wi->cfg.height,
				(u32)info.p_y_addr, &info.p_uv_addr, &p_v_addr);
	}

done:

	DSSDBG("Writeback: en=%d %x/%x (%dx%d => (%dx%d) "
		"col=%x wb_ix=%d channel=%d mode=%d",
		info.enabled, info.p_y_addr, info.p_uv_addr, info.width,
		info.height, info.out_w, info.out_h, info.color_mode,
		wb_ix, info.channel, info.mode);

	/* set overlay info */
	return wb->set_wb_info(wb, &info);

err:
	DSSERR("get wb_ion_addr is null \n");
	return -EFAULT;
}

void swap_rb_in_ovl_info(struct dss_ovl_info *oi)
{
#if 0
	/* we need to swap YUV color matrix if we are swapping R and B */
	if (oi->cfg.color_mode &
	    (ODIN_DSS_COLOR_NV12 | ODIN_DSS_COLOR_YUV2 | ODIN_DSS_COLOR_UYVY)) {
		swap(oi->cfg.cconv.ry, oi->cfg.cconv.by);
		swap(oi->cfg.cconv.rcr, oi->cfg.cconv.bcr);
		swap(oi->cfg.cconv.rcb, oi->cfg.cconv.bcb);
	}
#endif
}

struct odin_overlay_manager *find_dss_mgr(int display_ix)
{
	struct odin_overlay_manager *mgr;
	char name[32];
	int i;

	sprintf(name, "display%d", display_ix);

	for (i = 0; i < odin_mgr_dss_get_num_overlay_managers(); i++) {
		mgr = odin_mgr_dss_get_overlay_manager(i);
		if (mgr->device && !strcmp(name, dev_name(&mgr->device->dev)))
			return mgr;
	}
	return NULL;
}

int set_dss_mgr_info(struct dss_mgr_info *mi, struct odindss_ovl_cb *cb,
	u32 display_ix, enum dsscomp_setup_mode mode)
{
	struct odin_overlay_manager_info info;
	struct odin_overlay_manager *mgr;

	if (!mi)
		return -EINVAL;
	mgr = find_dss_mgr(display_ix);
	if (!mgr)
		return -EINVAL;

	/* just in case there are new fields, we get the current info */
	mgr->get_manager_info(mgr, &info);

	info.alpha_enabled = mi->alpha_blending;
	info.default_color = mi->default_color;
	info.trans_enabled = mi->trans_enabled && !mi->alpha_blending;
	info.trans_key = mi->trans_key;
	info.cb = *cb;
	return mgr->set_manager_info(mgr, &info);
}

bool intersection_check(struct coordinate *src, struct coordinate *dst)
{
	if ((dst->x_right > src->x_left) &&
		(dst->x_left < src->x_right) &&
		(dst->y_bottom > src->y_top) &&
		(dst->y_top < src->y_bottom))
		return true;
	else
		return false;
}

bool intersection_get(struct coordinate *src, struct coordinate *dst,
	struct coordinate *intersection)
{
	*(intersection) = *(dst);

	if (src->x_left < dst->x_left)
		intersection->x_left = src->x_left;
	if (src->x_right < dst->x_right)
		intersection->x_right = src->x_right;
	if (src->y_top < dst->y_top)
		intersection->y_top = src->y_top;
	if (src->y_bottom < dst->y_bottom)
		intersection->y_bottom = src->y_bottom;

	return true;
}

bool intersection_three_overlap(struct coordinate *first,
		struct coordinate *second, struct coordinate *third)
{
	struct coordinate intersection;

	if (intersection_check(first, second))
	{
		intersection_get(first, second, &intersection);
		/* DSSINFO("intection = %d %d %d %d", intersection.x_left,
			intersection.y_top, intersection.x_right, intersection.y_bottom); */
		if (intersection_check(third, &intersection))
			return true;
	}

	return false;
}

bool intersection_four_overlap(struct coordinate *first,
		struct coordinate *second, struct coordinate *third,
		struct coordinate *forth)
{
	bool overlap_check;
	overlap_check = intersection_three_overlap(first, second, third);
	if (overlap_check)
		return true;
	overlap_check = intersection_three_overlap(first, second, forth);
	if (overlap_check)
		return true;
	overlap_check = intersection_three_overlap(second, third, forth);
	if (overlap_check)
		return true;

	return false;
}

bool dss_intersection_overlap_check(struct coordinate *layers, int layer_cnt)
{
	bool overlap_check = false;

	if (layer_cnt == 3)
		overlap_check = intersection_three_overlap(&layers[0],
							&layers[1], &layers[2]);
#if 1
        else if (layer_cnt >= 4)
                overlap_check = true;
#else
	else if (layer_cnt == 4)
		overlap_check = intersection_four_overlap(&layers[0],
						&layers[1], &layers[2], &layers[3]);
	else if (layer_cnt > 5)
		overlap_check = true;
#endif
	else
		overlap_check = false;

	return overlap_check;
}

void swap_rb_in_mgr_info(struct dss_mgr_info *mi)
{
#if 0
	const struct odin_dss_cpr_coefs c = { 256, 0, 0, 0, 256, 0, 0, 0, 256 };

	/* set default CPR */
	if (!mi->cpr_enabled)
		mi->cpr_coefs = c;
	mi->cpr_enabled = true;

	/* swap red and blue */
	swap(mi->cpr_coefs.rr, mi->cpr_coefs.br);
	swap(mi->cpr_coefs.rg, mi->cpr_coefs.bg);
	swap(mi->cpr_coefs.rb, mi->cpr_coefs.bb);
#endif
}

/*
 * ===========================================================================
 *				DEBUG METHODS
 * ===========================================================================
 */
#if 0
void dump_ovl_info(struct dsscomp_dev *cdev, struct dss_ovl_info *oi)
{
	struct dss_ovl_cfg *c = &oi->cfg;
	const struct color_info *ci;

	if (!(debug & DEBUG_OVERLAYS) ||
	    !(debug & DEBUG_COMPOSITIONS))
		return;

	ci = get_color_info(c->color_mode);
	if (c->zonly) {
		dev_info(DEV(cdev), "ovl%d(%s z%d)\n",
			c->ix, c->enabled ? "ON" : "off", c->zorder);
		return;
	}
	dev_info(DEV(cdev), "ovl%d(%s z%d %s%s *%d%% %d*%d:%d,%d+%d,%d rot%d%s"
						" => %d,%d+%d,%d %p/%p|%d)\n",
		c->ix, c->enabled ? "ON" : "off", c->zorder,
		ci->name ? : "(none)",
		c->pre_mult_alpha ? " premult" : "",
		(c->global_alpha * 100 + 128) / 255,
		c->width, c->height, c->crop.x, c->crop.y,
		c->crop.w, c->crop.h,
		c->rotation, c->mirror ? "+mir" : "",
		c->win.x, c->win.y, c->win.w, c->win.h,
		(void *) oi->ba, (void *) oi->uv, c->stride);
}

static void print_mgr_info(struct dsscomp_dev *cdev,
			struct dss_mgr_info *mi)
{
	printk("(dis%d(%s) alpha=%d col=%08x ilace=%d) ",
		mi->ix,
		(mi->ix < cdev->num_displays && cdev->displays[mi->ix]) ?
		cdev->displays[mi->ix]->name : "NONE",
		mi->alpha_blending, mi->default_color,
		mi->interlaced);
}

void dump_comp_info(struct dsscomp_dev *cdev, struct dsscomp_setup_mgr_data *d,
			const char *phase)
{
	if (!(debug & DEBUG_COMPOSITIONS))
		return;

	dev_info(DEV(cdev), "[%p] %s: %c%c%c ",
		 *phase == 'q' ? (void *) d->sync_id : d, phase,
		 (d->mode & DSSCOMP_SETUP_MODE_APPLY) ? 'A' : '-',
		 (d->mode & DSSCOMP_SETUP_MODE_DISPLAY) ? 'D' : '-',
		 (d->mode & DSSCOMP_SETUP_MODE_CAPTURE) ? 'C' : '-');
	print_mgr_info(cdev, &d->mgr);
	printk("n=%d\n", d->num_ovls);
}

#if 0
void dump_total_comp_info(struct dsscomp_dev *cdev,
			struct dsscomp_setup_dispc_data *d,
			const char *phase)
{
	int i;

	if (!(debug & DEBUG_COMPOSITIONS))
		return;

	dev_info(DEV(cdev), "[%p] %s: %c%c%c ",
		 *phase == 'q' ? (void *) d->sync_id : d, phase,
		 (d->mode & DSSCOMP_SETUP_MODE_APPLY) ? 'A' : '-',
		 (d->mode & DSSCOMP_SETUP_MODE_DISPLAY) ? 'D' : '-',
		 (d->mode & DSSCOMP_SETUP_MODE_CAPTURE) ? 'C' : '-');

	for (i = 0; i < d->num_mgrs && i < ARRAY_SIZE(d->mgrs); i++)
		print_mgr_info(cdev, d->mgrs + i);
	printk("n=%d\n", d->num_ovls);
}
#endif
#endif
