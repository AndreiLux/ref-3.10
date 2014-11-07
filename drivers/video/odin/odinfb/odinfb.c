/*
 * linux/drivers/video/odin/odinfb/odinfb.c
 *
 * Copyright (C) 2012 LGE Electronics
 * Author:
 *
 * Some code and ideas taken from drivers/video/odin2/ driver
 * by Seungwon Shin.
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/odinfb.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/ion.h>
#include <linux/odin_iommu.h>
#include <linux/sw_sync.h>
#include <linux/odin_dfs.h>

#include "odinfb.h"
#include <video/odindss.h>
#include "../dss/dss.h"

#define MODULE_NAME     "odinfb"

#define ODINFB_PLANE_XRES_MIN		8
#define ODINFB_PLANE_YRES_MIN		8
#define ODINFB_BUFFER_NUMBER		2

#if RUNTIME_CLK_GATING
extern bool clk_gated;
extern spinlock_t clk_gating_lock;
#endif

static char *def_mode;
static int def_mirror;

#ifdef DEBUG
unsigned int odinfb_debug = 0;
module_param_named(debug, odinfb_debug, bool, 0644);
static unsigned int odinfb_test_pattern;
module_param_named(test, odinfb_test_pattern, bool, 0644);
#else
unsigned int odinfb_debug = 0;
#endif

static int odinfb_fb_init(struct odinfb_device *fbdev, struct fb_info *fbi);
static int odinfb_get_recommended_bpp(struct odinfb_device *fbdev,
		struct odin_dss_device *dssdev);

#ifdef DEBUG
static void draw_pixel(struct fb_info *fbi, int x, int y, unsigned color)
{
	struct fb_var_screeninfo *var = &fbi->var;
	struct fb_fix_screeninfo *fix = &fbi->fix;
	void __iomem *addr = fbi->screen_base;
	const unsigned bytespp = var->bits_per_pixel >> 3;
	const unsigned line_len = fix->line_length / bytespp;

	int r = (color >> 16) & 0xff;
	int g = (color >> 8) & 0xff;
	int b = (color >> 0) & 0xff;

	if (var->bits_per_pixel == 16) {
		u16 __iomem *p = (u16 __iomem *)addr;
		p += y * line_len + x;

		r = r * 32 / 256;
		g = g * 64 / 256;
		b = b * 32 / 256;

		__raw_writew((r << 11) | (g << 5) | (b << 0), p);
	} else if (var->bits_per_pixel == 24) {
		u8 __iomem *p = (u8 __iomem *)addr;
		p += (y * line_len + x) * 3;

		__raw_writeb(b, p + 0);
		__raw_writeb(g, p + 1);
		__raw_writeb(r, p + 2);
	} else if (var->bits_per_pixel == 32) {
		u32 __iomem *p = (u32 __iomem *)addr;
		p += y * line_len + x;
		__raw_writel(color, p);
	}
}

static void fill_fb(struct fb_info *fbi)
{
	struct fb_var_screeninfo *var = &fbi->var;
	const short w = var->xres_virtual;
	const short h = var->yres_virtual;
	void __iomem *addr = fbi->screen_base;
	int y, x;

	if (!addr)
		return;

	FB_DBG("fill_fb %dx%d, line_len %d bytes\n", w, h, fbi->fix.line_length);

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (x < 20 && y < 20)
				draw_pixel(fbi, x, y, 0xffffff);
			else if (x < 20 && (y > 20 && y < h - 20))
				draw_pixel(fbi, x, y, 0xff);
			else if (y < 20 && (x > 20 && x < w - 20))
				draw_pixel(fbi, x, y, 0xff00);
			else if (x > w - 20 && (y > 20 && y < h - 20))
				draw_pixel(fbi, x, y, 0xff0000);
			else if (y > h - 20 && (x > 20 && x < w - 20))
				draw_pixel(fbi, x, y, 0xffff00);
			else if (x == 20 || x == w - 20 ||
					y == 20 || y == h - 20)
				draw_pixel(fbi, x, y, 0xffffff);
			else if (x == y || w - x == h - y)
				draw_pixel(fbi, x, y, 0xff00ff);
			else if (w - x == y || x == h - y)
				draw_pixel(fbi, x, y, 0x00ffff);
			else if (x > 20 && y > 20 && x < w - 20 && y < h - 20) {
				int t = x * 3 / w;
				unsigned r = 0, g = 0, b = 0;
				unsigned c;
				if (var->bits_per_pixel == 16) {
					if (t == 0)
						b = (y % 32) * 256 / 32;
					else if (t == 1)
						g = (y % 64) * 256 / 64;
					else if (t == 2)
						r = (y % 32) * 256 / 32;
				} else {
					if (t == 0)
						b = (y % 256);
					else if (t == 1)
						g = (y % 256);
					else if (t == 2)
						r = (y % 256);
				}
				c = (r << 16) | (g << 8) | (b << 0);
				draw_pixel(fbi, x, y, c);
			} else {
				draw_pixel(fbi, x, y, 0);
			}
		}
	}
}
#endif

#if 0
static u32 odinfb_get_region_rot_paddr(const struct odinfb_info *ofbi, int rot)
{
	return ofbi->region->paddr;
}
#endif

static u32 odinfb_get_region_size(const struct odinfb_info *ofbi)
{
	return ofbi->region->size;
}

static u32 odinfb_get_region_paddr(const struct odinfb_info *ofbi)
{
	return ofbi->region->paddr;
}

static void __iomem *odinfb_get_region_vaddr(const struct odinfb_info *ofbi)
{
	return ofbi->region->vaddr;
}

static struct odinfb_colormode odinfb_colormodes[] = {
	{
		/* YUV */
		.dssmode = ODIN_DSS_COLOR_YUV422S,
		.bits_per_pixel = 16,
		.nonstd = ODINFB_COLOR_YUV422S,
	}, {
		.dssmode = ODIN_DSS_COLOR_YUV422P_2,
		.bits_per_pixel = 16,
		.nonstd = ODINFB_COLOR_YUV422P_2,
	}, {
		.dssmode = ODIN_DSS_COLOR_YUV224P_2,
		.bits_per_pixel = 16,
		.nonstd = ODINFB_COLOR_YUV224P_2,
	}, {
		.dssmode = ODIN_DSS_COLOR_YUV420P_2,
		.bits_per_pixel = 12,
		.nonstd = ODINFB_COLOR_YUV420P_2,
	}, {
		.dssmode = ODIN_DSS_COLOR_YUV444P_2,
		.bits_per_pixel = 24,
		.nonstd = ODINFB_COLOR_YUV444P_2,
	}, {
		.dssmode = ODIN_DSS_COLOR_YUV422P_3,
		.bits_per_pixel = 16,
		.nonstd = ODINFB_COLOR_YUV422P_3,
	}, {
		.dssmode = ODIN_DSS_COLOR_YUV224P_3,
		.bits_per_pixel = 16,
		.nonstd = ODINFB_COLOR_YUV224P_3,
	}, {
		.dssmode = ODIN_DSS_COLOR_YUV420P_3,
		.bits_per_pixel = 12,
		.nonstd = ODINFB_COLOR_YUV420P_3,
	}, {
		.dssmode = ODIN_DSS_COLOR_YUV444P_3,
		.bits_per_pixel = 24,
		.nonstd = ODINFB_COLOR_YUV444P_3,
	},
	{
		.dssmode = ODIN_DSS_COLOR_RGB555 | ODIN_DSS_COLOR_PIXEL_ALPHA,
		.bits_per_pixel = 16,
		.red	= {.length = 5,.offset = 10,.msb_right = 0 },
		.green	= {.length = 5,.offset = 5,.msb_right = 0 },
		.blue	= {.length = 5,.offset = 0,.msb_right = 0 },
		.transp	= {.length = 1,.offset = 15,.msb_right = 0 },
		.nonstd = ODINFB_COLOR_NONSTD,
	}, {
		.dssmode = ODIN_DSS_COLOR_RGB555,
		.bits_per_pixel = 16,
		.red	= {.length = 5,.offset = 10,.msb_right = 0 },
		.green	= {.length = 5,.offset = 5,.msb_right = 0 },
		.blue	= {.length = 5,.offset = 0,.msb_right = 0 },
		.transp	= {.length = 0,.offset = 0,.msb_right = 0 },
		.nonstd = ODINFB_COLOR_NONSTD,
	}, {
		.dssmode = ODIN_DSS_COLOR_RGB444 | ODIN_DSS_COLOR_PIXEL_ALPHA,
		.bits_per_pixel = 16,
		.red	= {.length = 4,.offset = 8,.msb_right = 0 },
		.green	= {.length = 4,.offset = 4,.msb_right = 0 },
		.blue	= {.length = 4,.offset = 0,.msb_right = 0 },
		.transp	= {.length = 4,.offset = 12,.msb_right = 0 },
		.nonstd = ODINFB_COLOR_NONSTD,
	}, {
		.dssmode = ODIN_DSS_COLOR_RGB444,
		.bits_per_pixel = 16,
		.red	= {.length = 4,.offset = 8,.msb_right = 0 },
		.green	= {.length = 4,.offset = 4,.msb_right = 0 },
		.blue	= {.length = 4,.offset = 0,.msb_right = 0 },
		.transp	= {.length = 0,.offset = 0,.msb_right = 0 },
		.nonstd = ODINFB_COLOR_NONSTD,
	}, {
		.dssmode = ODIN_DSS_COLOR_RGB888 | ODIN_DSS_COLOR_PIXEL_ALPHA,
		.bits_per_pixel = 32,
		.red	= {.length = 8,.offset = 16,.msb_right = 0 },
		.green	= {.length = 8,.offset = 8,.msb_right = 0 },
		.blue	= {.length = 8,.offset = 0,.msb_right = 0 },
		.transp	= {.length = 8,.offset = 24,.msb_right = 0 },
		.nonstd = ODINFB_COLOR_NONSTD,
	}, {
		.dssmode = ODIN_DSS_COLOR_RGB888,
		.bits_per_pixel = 32,
		.red	= {.length = 8,.offset = 16,.msb_right = 0 },
		.green	= {.length = 8,.offset = 8,.msb_right = 0 },
		.blue	= {.length = 8,.offset = 0,.msb_right = 0 },
		.transp = {.length = 0,.offset = 0,.msb_right = 0 },
		.nonstd = ODINFB_COLOR_NONSTD,
	}, {
		.dssmode = ODIN_DSS_COLOR_RGB888 | ODIN_DSS_COLOR_PIXEL_ALPHA
						| ODIN_DSS_COLOR_ALPHA_RGBA,
		.bits_per_pixel = 32,
		.red	= {.length = 8,.offset = 24,.msb_right = 0 },
		.green	= {.length = 8,.offset = 16,.msb_right = 0 },
		.blue	= {.length = 8,.offset = 8,.msb_right = 0 },
		.transp = {.length = 8,.offset = 0,.msb_right = 0 },
		.nonstd = ODINFB_COLOR_NONSTD,
	},
	{
		.dssmode = ODIN_DSS_COLOR_RGB565,
		.bits_per_pixel = 16,
		.red	= {.length = 5,.offset = 11,.msb_right = 0 },
		.green	= {.length = 6,.offset = 5,.msb_right = 0 },
		.blue	= {.length = 5,.offset = 0,.msb_right = 0 },
		.transp	= {.length = 0,.offset = 0,.msb_right = 0 },
		.nonstd = ODINFB_COLOR_NONSTD,
	},
};

static bool cmp_var_to_colormode(struct fb_var_screeninfo *var,
		struct odinfb_colormode *color)
{
	bool cmp_component(struct fb_bitfield *f1, struct fb_bitfield *f2)
	{
		return f1->length == f2->length &&
			f1->offset == f2->offset &&
			f1->msb_right == f2->msb_right;
	}

	if (var->bits_per_pixel == 0 ||
			var->red.length == 0 ||
			var->blue.length == 0 ||
			var->green.length == 0)
		return 0;

	return var->bits_per_pixel == color->bits_per_pixel &&
		cmp_component(&var->red, &color->red) &&
		cmp_component(&var->green, &color->green) &&
		cmp_component(&var->blue, &color->blue) &&
		cmp_component(&var->transp, &color->transp);
}

static void assign_colormode_to_var(struct fb_var_screeninfo *var,
		struct odinfb_colormode *color)
{
	var->bits_per_pixel = color->bits_per_pixel;
	var->nonstd = color->nonstd;
	var->red = color->red;
	var->green = color->green;
	var->blue = color->blue;
	var->transp = color->transp;
}

static int fb_mode_to_dss_mode(struct fb_var_screeninfo *var,
		enum odin_color_mode *mode)
{
	int i;

	/* first match with nonstd field */
	if (var->nonstd) {
		for (i = 0; i < ARRAY_SIZE(odinfb_colormodes); ++i) {
			struct odinfb_colormode *m = &odinfb_colormodes[i];
			if (var->nonstd == m->nonstd) {
				assign_colormode_to_var(var, m);
				*mode = m->dssmode;
				return 0;
			}
		}

		return -EINVAL;
	}

	/* then try exact match of bpp and colors */
	for (i = 0; i < ARRAY_SIZE(odinfb_colormodes); ++i) {
		struct odinfb_colormode *m = &odinfb_colormodes[i];
		if (cmp_var_to_colormode(var, m)) {
			assign_colormode_to_var(var, m);
			*mode = m->dssmode;
			FB_DBG("fb_mode_to_dss_mode: dssmode = %d \n", *mode);
			return 0;
		}
	}

	printk("fail: unregisterd color-format input in fb\n");

	return -EINVAL;
}

static int check_fb_res_bounds(struct fb_var_screeninfo *var)
{
	int xres_min = ODINFB_PLANE_XRES_MIN;
	int xres_max = 2048;
	int yres_min = ODINFB_PLANE_YRES_MIN;
	int yres_max = 2048;

	/* XXX: some applications seem to set virtual res to 0. */
	if (var->xres_virtual == 0)
		var->xres_virtual = var->xres;

	if (var->yres_virtual == 0)
		var->yres_virtual = var->yres;

	if (var->xres_virtual < xres_min || var->yres_virtual < yres_min)
		return -EINVAL;

	if (var->xres < xres_min)
		var->xres = xres_min;
	if (var->yres < yres_min)
		var->yres = yres_min;
	if (var->xres > xres_max)
		var->xres = xres_max;
	if (var->yres > yres_max)
		var->yres = yres_max;

	if (var->xres > var->xres_virtual)
		var->xres = var->xres_virtual;
	if (var->yres > var->yres_virtual)
		var->yres = var->yres_virtual;

	return 0;
}

static void shrink_height(unsigned long max_frame_size,
		struct fb_var_screeninfo *var)
{
	FB_DBG("can't fit FB into memory, reducing y\n");
	var->yres_virtual = max_frame_size /
		(var->xres_virtual * var->bits_per_pixel >> 3);

	if (var->yres_virtual < ODINFB_PLANE_YRES_MIN)
		var->yres_virtual = ODINFB_PLANE_YRES_MIN;

	if (var->yres > var->yres_virtual)
		var->yres = var->yres_virtual;
}

static void shrink_width(unsigned long max_frame_size,
		struct fb_var_screeninfo *var)
{
	FB_DBG("can't fit FB into memory, reducing x\n");
	var->xres_virtual = max_frame_size / var->yres_virtual /
		(var->bits_per_pixel >> 3);

	if (var->xres_virtual < ODINFB_PLANE_XRES_MIN)
		var->xres_virtual = ODINFB_PLANE_XRES_MIN;

	if (var->xres > var->xres_virtual)
		var->xres = var->xres_virtual;
}

static int check_fb_size(const struct odinfb_info *ofbi,
		struct fb_var_screeninfo *var)
{
	unsigned long max_frame_size = ofbi->region->size;
	int bytespp = var->bits_per_pixel >> 3;
	unsigned long line_size = var->xres_virtual * bytespp;

	FB_DBG("max frame size %lu, line size %lu\n", max_frame_size, line_size);

	if (line_size * var->yres_virtual > max_frame_size)
		shrink_height(max_frame_size, var);

	if (line_size * var->yres_virtual > max_frame_size) {
		shrink_width(max_frame_size, var);
		line_size = var->xres_virtual * bytespp;
	}

	if (line_size * var->yres_virtual > max_frame_size) {
		FB_ERR("cannot fit FB to memory\n");
		return -EINVAL;
	}

	return 0;
}

int dss_mode_to_fb_mode(enum odin_color_mode dssmode,
			struct fb_var_screeninfo *var)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(odinfb_colormodes); ++i) {
		struct odinfb_colormode *mode = &odinfb_colormodes[i];
		if (dssmode == mode->dssmode) {
			assign_colormode_to_var(var, mode);
			return 0;
		}
	}
	return -ENOENT;
}

void set_fb_fix(struct fb_info *fbi)
{
	struct fb_fix_screeninfo *fix = &fbi->fix;
	struct fb_var_screeninfo *var = &fbi->var;
	struct odinfb_info *ofbi = FB2OFB(fbi);

	FB_DBG("set_fb_fix\n");

	/* used by open/write in fbmem.c */
	fbi->screen_base = (char __iomem *)odinfb_get_region_vaddr(ofbi);

	/* used by mmap in fbmem.c */
	fix->line_length =
		(var->xres_virtual * var->bits_per_pixel) >> 3;

	fix->smem_len =odinfb_get_region_size(ofbi);

	fix->smem_start = odinfb_get_region_paddr(ofbi);

	FB_DBG("set_fb_fix : fix->smem_len = %d\n", fix->smem_len);

	fix->type = FB_TYPE_PACKED_PIXELS;

	if (var->nonstd)
		fix->visual = FB_VISUAL_PSEUDOCOLOR;
	else {
		switch (var->bits_per_pixel) {
		case 32:
		case 24:
		case 16:
		case 12:
			fix->visual = FB_VISUAL_TRUECOLOR;
			/* 12bpp is stored in 16 bits */
			break;
		case 1:
		case 2:
		case 4:
		case 8:
			fix->visual = FB_VISUAL_PSEUDOCOLOR;
			break;
		}
	}

	fix->accel = FB_ACCEL_NONE;

	fix->xpanstep = 1;
	fix->ypanstep = 1;
}

/* check new var and possibly modify it to be ok */
int check_fb_var(struct fb_info *fbi, struct fb_var_screeninfo *var)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odin_dss_device *display = fb2display(fbi);
	enum odin_color_mode mode = 0;
	int i;
	int r;

	FB_DBG("check_fb_var %d\n", ofbi->id);

	if (ofbi->region->size == 0)
		return 0;

	r = fb_mode_to_dss_mode(var, &mode);
	if (r) {
		FB_ERR("cannot convert var to odin dss mode\n");
		return r;
	}

	for (i = 0; i < ofbi->num_overlays; ++i) {
		if ((ofbi->overlays[i]->supported_modes & mode) == 0) {
			FB_ERR("invalid mode = %d \n", mode);
			return -EINVAL;
		}
	}

	if (var->rotate > 3)	/* var->rotate should be one of 0,1,2,3 */
		return -EINVAL;

	if (check_fb_res_bounds(var))
		return -EINVAL;

	if (check_fb_size(ofbi, var))
		return -EINVAL;

	if (var->xres + var->xoffset > var->xres_virtual)
		var->xoffset = var->xres_virtual - var->xres;
	if (var->yres + var->yoffset > var->yres_virtual)
		var->yoffset = var->yres_virtual - var->yres;

	FB_DBG("xres = %d, yres = %d, vxres = %d, vyres = %d\n",
			var->xres, var->yres,
			var->xres_virtual, var->yres_virtual);

	var->grayscale          = 0;

	if (display && display->driver->get_timings) {
		struct odin_video_timings timings;
		display->driver->get_timings(display, &timings);

		var->height = timings.height;
		var->width = timings.width;

		/* pixclock in ps, the rest in pixclock */
		var->pixclock = timings.pixel_clock != 0 ?
			KHZ2PICOS(timings.pixel_clock) :
			0;
		var->left_margin = timings.hfp;
		var->right_margin = timings.hbp;
		var->upper_margin = timings.vfp;
		var->lower_margin = timings.vbp;
		var->hsync_len = timings.hsw;
		var->vsync_len = timings.vsw;
	} else {
		var->height = -1;
		var->width = -1;
		var->pixclock = 0;
		var->left_margin = 0;
		var->right_margin = 0;
		var->upper_margin = 0;
		var->lower_margin = 0;
		var->hsync_len = 0;
		var->vsync_len = 0;
	}

	/* TODO: get these from panel->config */
	var->vmode              = FB_VMODE_NONINTERLACED;
	var->sync               = 0;

	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * fbdev framework callbacks
 * ---------------------------------------------------------------------------
 */
static int odinfb_open(struct fb_info *fbi, int user)
{
	return 0;
}

static int odinfb_release(struct fb_info *fbi, int user)
{
#if 0
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb2_device *fbdev = ofbi->fbdev;
	struct odin_dss_device *display = fb2display(fbi);

	FB_DBG("Closing fb with plane index %d\n", ofbi->id);

	odinfb_lock(fbdev);

	if (display && display->get_update_mode && display->update) {
		/* XXX this update should be removed, I think. But it's
		 * good for debugging */
		if (display->get_update_mode(display) ==
				OMAP_DSS_UPDATE_MANUAL) {
			u16 w, h;

			if (display->sync)
				display->sync(display);

			display->get_resolution(display, &w, &h);
			display->update(display, 0, 0, w, h);
		}
	}

	if (display && display->sync)
		display->sync(display);

	odinfb_unlock(fbdev);
#endif
	return 0;
}


/* setup overlay according to the fb */
int odinfb_setup_overlay(struct fb_info *fbi, struct odin_overlay *ovl,
			u16 crop_x, u16 crop_y,	u16 crop_w, u16 crop_h,
			u16 win_x, u16 win_y, u16 win_w, u16 win_h, bool info_enabled)
{
	int r = 0;
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct fb_var_screeninfo *var = &fbi->var;
	struct fb_fix_screeninfo *fix = &fbi->fix;
	enum odin_color_mode mode = 0;
	int offset;
	u32 data_start_p;
	void __iomem *data_start_v;
	struct odin_overlay_info info;
	int xres, yres;
	int screen_width;
	int mirror;
	unsigned int translen;

	FB_DBG("setup_overlay %d, posx %d, posy %d, outw %d, outh %d\n", ofbi->id,
			win_x, win_x, win_w, win_h);

	xres = var->xres;
	yres = var->yres;

	data_start_p = odinfb_get_region_paddr(ofbi);
	data_start_v = odinfb_get_region_vaddr(ofbi);

	offset = var->yoffset;

	translen = var->xres * offset * var->bits_per_pixel / 8;

	data_start_p += translen;
	data_start_v += translen;

	if (offset)
		FB_DBG("offset %d, %d = %d\n",
				var->xoffset, var->yoffset, offset);

	FB_DBG("paddr %x, vaddr %p\n", data_start_p, data_start_v);

	r = fb_mode_to_dss_mode(var, &mode);
	if (r) {
		FB_ERR("fb_mode_to_dss_mode failed");
		goto err;
	}

	screen_width = fix->line_length / (var->bits_per_pixel >> 3);
	ovl->get_overlay_info(ovl, &info);

	mirror = ofbi->mirror;

	info.enabled = info_enabled;

	info.p_y_addr = data_start_p;
	info.screen_width = screen_width;

	info.width = xres;
	info.height = yres;
	info.color_mode = mode;
	info.mirror = mirror;

	info.crop_x = crop_x;
	info.crop_y = crop_y;
	info.crop_w = crop_w;
	info.crop_h = crop_h;

	info.out_x = win_x;
	info.out_y = win_y;
	info.out_w = win_w;
	info.out_h = win_h;

	r = ovl->set_overlay_info(ovl, &info);
	if (r) {
		FB_ERR("ovl->setup_overlay_info failed\n");
		goto err;
	}

	return 0;

err:
	FB_ERR("setup_overlay failed\n");
	return r;
}

/* apply var to the overlay */
int odinfb_apply_changes(struct fb_info *fbi, int init)
{
	/* param. 'init' means whether it is init booting or not */

	int r = 0, r2 = 0;
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odin_dss_device	*dssdev;
	struct fb_var_screeninfo *var = &fbi->var;
	struct odin_overlay *ovl;
	u16 crop_x, crop_y, crop_w, crop_h;
	u16 win_x, win_y, win_w, win_h;
	bool info_enabled = false;
	int i;
	bool manually_update_mode;
	unsigned long timeout = msecs_to_jiffies(500);

	for (i = 0; i < ofbi->num_overlays; i++) {
		ovl = ofbi->overlays[i];
		dssdev = ovl->manager->device;
		manually_update_mode = dssdev_manually_updated(dssdev);

		FB_DBG("apply_changes, fb %d, ovl %d\n", ofbi->id, ovl->id);

		if (ofbi->region->size == 0) {
			/* the fb is not available. disable the overlay */
			odinfb_overlay_enable(ovl, 0);
			if (!init && ovl->manager)
				ovl->manager->apply(ovl->manager, DSSCOMP_SETUP_MODE_DISPLAY);
			continue;
		}

		if (init || (ovl->caps & ODIN_DSS_OVL_CAP_SCALE) == 0) {
			/* TODO: Consider Rotation */
			crop_w = var->xres;
			crop_h = var->yres;
			win_w = var->xres;
			win_h = var->yres;
		} else {
			crop_w = ovl->info.crop_w;
			crop_h = ovl->info.crop_h;
			win_w = ovl->info.out_w;
			win_h = ovl->info.out_h;
		}

		if (init) {
			crop_x = 0;
			crop_y = 0;
			win_x = 0;
			win_y = 0;

		} else {
			crop_x = ovl->info.crop_x;
			crop_y = ovl->info.crop_y;
			win_x = ovl->info.out_x;
			win_y = ovl->info.out_y;
			info_enabled = true;
		}

		r = odinfb_setup_overlay(fbi, ovl, crop_x, crop_y,
			crop_w, crop_h, win_x, win_y, win_w, win_h, info_enabled);
		if (r)
			goto err;

		if (!init && manually_update_mode)
		{
#if RUNTIME_CLK_GATING
			enable_runtime_clk (ovl->manager, false); /* mem2mem mode is false */
#endif
			ovl->manager->clk_enable(1 << ODIN_DSS_GRA0, 0, false);
		}
		if (!init) /* for general case, not init booting */
		{
			if (ovl->manager)
			{
				ovl->manager->update_manager_cache(ovl->manager);
				ovl->manager->apply(ovl->manager, DSSCOMP_SETUP_MODE_DISPLAY);
			}

			if (manually_update_mode && dssdev->driver->update)
			{
#ifdef DSS_DFS_MUTEX_LOCK
				mutex_lock(&dfs_mtx);
#elif defined(DSS_DFS_SPINLOCK)
				get_mem_dfs();
#endif
				odin_crg_underrun_set_info(dssdev->channel, (1 << ODIN_DSS_GRA0), 800000);

				dssdev->driver->sync(dssdev);
				dssdev->driver->update(dssdev, win_x, win_y, win_w, win_h);

				r2 = dssdev->manager->wait_for_framedone(dssdev->manager, timeout);
				r = dssdev->driver->flush_pending(dssdev);
				if (r2 < 0)
					DSSERR("odinfb_apply framedone timeout error\n");

				if (r < 0)
					DSSERR("odinfb_apply flush_pending error\n");
#ifdef DSS_DFS_MUTEX_LOCK
				mutex_unlock(&dfs_mtx);
#elif defined(DSS_DFS_SPINLOCK)
				put_mem_dfs();
#endif
#if RUNTIME_CLK_GATING
				disable_runtime_clk (ovl->manager->id);
#endif

			}
		}
	}
	return 0;
err:
	FB_ERR("apply_changes failed\n");
	return r;
}

/* checks var and eventually tweaks it to something supported,
 * DO NOT MODIFY PAR */
static int odinfb_check_var(struct fb_var_screeninfo *var, struct fb_info *fbi)
{
	int r;

	FB_DBG("check_var(%d)\n", FB2OFB(fbi)->id);

	r = check_fb_var(fbi, var);

	return r;
}

/* set the video mode according to info->var */
static int odinfb_set_par(struct fb_info *fbi)
{
	int r;

	FB_DBG("set_par(%d)\n", FB2OFB(fbi)->id);

	set_fb_fix(fbi);

	r = odinfb_apply_changes(fbi, 0);

	return r;
}

void odinfb_fb2dss_timings(struct fb_videomode *fb_timings,
			struct odin_video_timings *dss_timings)
{
	dss_timings->x_res = fb_timings->xres;
	dss_timings->y_res = fb_timings->yres;
	if (fb_timings->vmode & FB_VMODE_INTERLACED)
		dss_timings->y_res /= 2;
	dss_timings->pixel_clock = fb_timings->pixclock ?
					PICOS2KHZ(fb_timings->pixclock) : 0;
	dss_timings->hfp = fb_timings->right_margin;
	dss_timings->hbp = fb_timings->left_margin;
	dss_timings->hsw = fb_timings->hsync_len;
	dss_timings->vfp = fb_timings->lower_margin;
	dss_timings->vbp = fb_timings->upper_margin;
	dss_timings->vsw = fb_timings->vsync_len;

	pr_info("size:%d X %d pclk:%d KHz\n", dss_timings->x_res,
				dss_timings->y_res, dss_timings->pixel_clock);
	pr_info("hfp:%d hbp:%d vfp:%d vbp:%d hsync_len:%d vsync_len:%d\n",
			dss_timings->hfp, dss_timings->hbp, dss_timings->vfp,
			dss_timings->vbp, dss_timings->hsw, dss_timings->vsw);

}
EXPORT_SYMBOL(odinfb_fb2dss_timings);

void odinfb_dss2fb_timings(struct odin_video_timings *dss_timings,
			struct fb_videomode *fb_timings)
{
	memset(fb_timings, 0, sizeof(*fb_timings));
	fb_timings->xres = dss_timings->x_res;
	fb_timings->yres = dss_timings->y_res;
	fb_timings->pixclock = dss_timings->pixel_clock ?
					KHZ2PICOS(dss_timings->pixel_clock) : 0;
	fb_timings->right_margin = dss_timings->hfp;
	fb_timings->left_margin = dss_timings->hbp;
	fb_timings->hsync_len = dss_timings->hsw;
	fb_timings->lower_margin = dss_timings->vfp;
	fb_timings->upper_margin = dss_timings->vbp;
	fb_timings->vsync_len = dss_timings->vsw;
}
EXPORT_SYMBOL(odinfb_dss2fb_timings);


static int odinfb_pan_display(struct fb_var_screeninfo *var,
		struct fb_info *fbi)
{
	struct fb_var_screeninfo new_var;
	int r;

	FB_DBG("pan_display(%d)\n", FB2OFB(fbi)->id);
	FB_DBG("var->xoffset = %d / var->yoffset = %d\n", var->xoffset, var->yoffset);
	if (var->xoffset == fbi->var.xoffset &&
	    var->yoffset == fbi->var.yoffset)
		return 0;

	new_var = fbi->var;
	new_var.xoffset = var->xoffset;
	new_var.yoffset = var->yoffset;

	fbi->var = new_var;

	r = odinfb_apply_changes(fbi, 0);

	return r;
}

#ifndef CONFIG_ODIN_ION_SMMU
static void mmap_user_open(struct vm_area_struct *vma)
{
	struct odinfb_info *ofbi = (struct odinfb_info *)vma->vm_private_data;

	atomic_inc(&ofbi->region->map_count);
}

static void mmap_user_close(struct vm_area_struct *vma)
{
	struct odinfb_info *ofbi = (struct odinfb_info *)vma->vm_private_data;

	atomic_dec(&ofbi->region->map_count);
}

static struct vm_operations_struct mmap_user_ops = {
	.open = mmap_user_open,
	.close = mmap_user_close,
};
#endif

static int odinfb_mmap(struct fb_info *fbi, struct vm_area_struct *vma)
{
#ifdef CONFIG_ODIN_ION_SMMU

	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_device *fbdev = ofbi->fbdev;

	if (vma->vm_end - vma->vm_start == 0)
		return 0;

	odin_ion_map_user(fbdev->alloc_data[ofbi->id].handle, vma);
	FB_DBG("vm_start:0x%x\n", vma->vm_start);

	return 0;

#else
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct fb_fix_screeninfo *fix = &fbi->fix;
	unsigned long off;
	unsigned long start;
	u32 len;

	FB_DBG("odinfb_mmap\n");

	if (vma->vm_end - vma->vm_start == 0)
		return 0;
	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;
	off = vma->vm_pgoff << PAGE_SHIFT;

	start = ofbi->region->paddr;
	len = fix->smem_len;
	if (off >= len)
		return -EINVAL;
	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;

	off += start;

	FB_DBG("user mmap region start %lx, len %d, off %lx\n", start, len, off);

	vma->vm_pgoff = off >> PAGE_SHIFT;
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;	/* for kernel 3.8 */
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_ops = &mmap_user_ops;
	vma->vm_private_data = ofbi;
	if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
			     vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;
	/* vm_ops.open won't be called for mmap itself. */
	atomic_inc(&ofbi->region->map_count);
	return 0;
#endif
}

/* Store a single color palette entry into a pseudo palette or the hardware
 * palette if one is available. For now we support only 16bpp and thus store
 * the entry only to the pseudo palette.
 */
static int _setcolreg(struct fb_info *fbi, u_int regno, u_int red, u_int green,
		u_int blue, u_int transp, int update_hw_pal)
{
	/*struct odinfb_info *ofbi = FB2OFB(fbi);*/
	/*struct odinfb2_device *fbdev = ofbi->fbdev;*/
	int r = 0;

#if 0
	enum odinfb_color_format mode = ODINFB_COLOR_RGB24U; /* XXX */

	/*switch (plane->color_mode) {*/
	switch (mode) {
	case ODINFB_COLOR_YUV422:
	case OMAPFB_COLOR_YUV420:
	case OMAPFB_COLOR_YUY422:
		r = -EINVAL;
		break;
	case OMAPFB_COLOR_CLUT_8BPP:
	case OMAPFB_COLOR_CLUT_4BPP:
	case OMAPFB_COLOR_CLUT_2BPP:
	case OMAPFB_COLOR_CLUT_1BPP:
		/*
		   if (fbdev->ctrl->setcolreg)
		   r = fbdev->ctrl->setcolreg(regno, red, green, blue,
		   transp, update_hw_pal);
		   */
		/* Fallthrough */
		r = -EINVAL;
		break;
	case OMAPFB_COLOR_RGB565:
	case OMAPFB_COLOR_RGB444:
	case OMAPFB_COLOR_RGB24P:
	case OMAPFB_COLOR_RGB24U:
		if (r != 0)
			break;

		if (regno < 0) {
			r = -EINVAL;
			break;
		}

		if (regno < 16) {
			u16 pal;
			pal = ((red >> (16 - var->red.length)) <<
					var->red.offset) |
				((green >> (16 - var->green.length)) <<
				 var->green.offset) |
				(blue >> (16 - var->blue.length));
			((u32 *)(fbi->pseudo_palette))[regno] = pal;
		}
		break;
	default:
		BUG();
	}
#endif
	return r;
}

static int odinfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
		u_int transp, struct fb_info *info)
{
	FB_DBG("setcolreg\n");

	return _setcolreg(info, regno, red, green, blue, transp, 1);
}

static int odinfb_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	int count, index, r;
	u16 *red, *green, *blue, *transp;
	u16 trans = 0xffff;

	FB_DBG("setcmap\n");

	red     = cmap->red;
	green   = cmap->green;
	blue    = cmap->blue;
	transp  = cmap->transp;
	index   = cmap->start;

	for (count = 0; count < cmap->len; count++) {
		if (transp)
			trans = *transp++;
		r = _setcolreg(info, index++, *red++, *green++, *blue++, trans,
				count == cmap->len - 1);
		if (r != 0)
			return r;
	}

	return 0;
}

static int odinfb_blank(int blank, struct fb_info *fbi)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_device *fbdev = ofbi->fbdev;
	struct odin_dss_device *display = fb2display(fbi);
	int do_update = 0;
	int r = 0;

	FB_DBG("odinfb_blank\n");

	/* null point error check */
	if (!display)
	{
		FB_ERR("no display connected with fbdev[%d]\n", ofbi->id);

		return -ENODEV;
	}

	odinfb_lock(fbdev);
	switch (blank) {
	case FB_BLANK_UNBLANK:
		if (display->state == ODIN_DSS_DISPLAY_SUSPENDED) {
			if (display->driver->resume)
				r = display->driver->resume(display);
		} else if (display->state == ODIN_DSS_DISPLAY_DISABLED) {
			if (display->driver->enable)
				r = display->driver->enable(display);
		}

		break;

	case FB_BLANK_NORMAL:
		/* FB_BLANK_NORMAL could be implemented.
		 * Needs DSS additions. */
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
		if (display->state != ODIN_DSS_DISPLAY_ACTIVE)
			goto exit;

		if (display->driver->suspend)
			r = display->driver->suspend(display);

		break;

	default:
		r = -EINVAL;
	}

exit:
	odinfb_unlock(fbdev);

	if (r == 0 && do_update && display->driver->update) {
		u16 w, h;
		display->driver->get_resolution(display, &w, &h);

		odin_du_set_channel_sync_enable(display->channel, 0);
		odin_du_set_channel_sync_enable(display->channel, 1);

		r = display->driver->update(display, 0, 0, w, h);
	}

	return r;
}

#if 0
/* XXX fb_read and fb_write are needed for VRFB */
ssize_t odinfb_write(struct fb_info *info, const char __user *buf,
		size_t count, loff_t *ppos)
{
	FB_DBG("odinfb_write %d, %lu\n", count, (unsigned long)*ppos);
	/* XXX needed for VRFB */
	return count;
}
#endif

static struct fb_ops odinfb_ops = {
	.owner          = THIS_MODULE,
	.fb_open        = odinfb_open,
	.fb_release     = odinfb_release,
	.fb_fillrect    = cfb_fillrect,
	.fb_copyarea    = cfb_copyarea,
	.fb_imageblit   = cfb_imageblit,
	.fb_blank       = odinfb_blank,
	.fb_ioctl       = odinfb_ioctl,
	.fb_check_var   = odinfb_check_var,
	.fb_set_par     = odinfb_set_par,
	.fb_pan_display = odinfb_pan_display,
	.fb_mmap	= odinfb_mmap,
	.fb_setcolreg	= odinfb_setcolreg,
	.fb_setcmap	= odinfb_setcmap,
	/*.fb_write	= odinfb_write,*/
};

static void odinfb_free_fbmem(struct fb_info *fbi)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_device *fbdev = ofbi->fbdev;

	if (ofbi->region->paddr)
		dma_free_writecombine(fbdev->dev, ofbi->region->size,
			fbi->screen_base, ofbi->region->paddr);

	if (ofbi->region->vaddr)
		iounmap(ofbi->region->vaddr);

	ofbi->region->vaddr = NULL;
	ofbi->region->paddr = 0;
	ofbi->region->alloc = 0;
	ofbi->region->size = 0;
}

static void clear_fb_info(struct fb_info *fbi)
{
	memset(&fbi->var, 0, sizeof(fbi->var));
	memset(&fbi->fix, 0, sizeof(fbi->fix));
	strlcpy(fbi->fix.id, MODULE_NAME, sizeof(fbi->fix.id));
}

static int odinfb_free_all_fbmem(struct odinfb_device *fbdev)
{
	int i;

	FB_DBG("free all fbmem\n");

	odinfb_free_fbmem(fbdev->fbs[0]);

	for (i = 0; i < fbdev->num_fbs; i++) {
		struct fb_info *fbi = fbdev->fbs[i];
		clear_fb_info(fbi);
	}

	return 0;
}

int odinfb_alloc_fbmem(struct fb_info *fbi, unsigned long size,
		unsigned long paddr, int index)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_device *fbdev = ofbi->fbdev;
	struct odinfb_mem_region *rg;
#ifdef CONFIG_ODIN_ION_SMMU
	struct ion_client *client;
	unsigned long ul_iova;
	unsigned long ul_kernel_va;
#endif

	rg = ofbi->region;

	rg->paddr = 0;
#ifndef CONFIG_ODIN_ION_SMMU
	rg->vaddr = NULL;
#endif
	rg->size = 0;
	rg->type = 0;
	rg->alloc = false;
	rg->map = false;

	size = PAGE_ALIGN(size);

	fbdev->dev->dma_mask = (u64 *)0xffffffff;
	fbdev->dev->coherent_dma_mask = 0xffffffff;

#ifdef CONFIG_ODIN_ION_SMMU

	client = odin_ion_client_create( "dss_fb" );

	fbdev->alloc_data[index].handle = ion_alloc(client, SZ_16M, 0x1000,
				(1<<ODIN_ION_CARVEOUT_FB), ION_FLAG_DONT_INV,
				ODIN_SUBSYS_DSS
				);

	if ( IS_ERR(fbdev->alloc_data[index].handle) ){
		dev_err(fbdev->dev, "failed to allocate framebuffer\n");
		return -ENOMEM;
	}

	if ( IS_ERR(fbdev->alloc_data[index].handle) ){
		dev_err(fbdev->dev, "failed to allocate framebuffer\n");
		return -ENOMEM;
	}

	fbdev->fd_data[index].handle = fbdev->alloc_data[index].handle;
	fbdev->fd_data[index].fd = (int)ion_share_dma_buf(client,
				fbdev->fd_data[index].handle);

	ul_iova = odin_ion_get_iova_of_buffer( fbdev->alloc_data[index].handle,
						ODIN_SUBSYS_DSS);
	/* FB_INFO("ul_iova : 0x%x\n", ul_iova); */

	ul_kernel_va = (unsigned long)ion_map_kernel(client, fbdev->fd_data[index].handle);
	/* FB_INFO("ul_kernel_va : 0x%x\n", ul_kernel_va);*/

/* Setting argument from ul_kernel_VA & ul_IOVA */
	fbi->screen_base = (char __iomem *)ul_kernel_va;

	rg->paddr = ul_iova;
	rg->vaddr = (void __iomem *)ul_kernel_va;
	rg->size = size;
	FB_DBG("odinfb_alloc_fbmem rg_size = %d, size = %d\n",rg->size, size);
	rg->alloc = 1;

	return 0;

#else	/* not SMMU / static DMC0 Framebuffer (0x8F000000 & 0x8F600000) */

	if (!paddr)
	{
		vaddr = dma_alloc_writecombine(fbdev->dev, size,
		(unsigned int*)&paddr, GFP_KERNEL);
	}
	else
	{
		if (!request_mem_region(paddr, size, rg->id)) {
			dev_err(fbdev->dev, "failed to request memory\n");
			return -ENOMEM;
		}

		vaddr = ioremap(paddr, size);

		if (!vaddr) {
			dev_err(fbdev->dev, "failed to ioremap framebuffer\n");
			return -ENOMEM;
		}

	}

	fbi->screen_base = vaddr;

	rg->paddr = paddr;
	rg->vaddr = vaddr;
	rg->size = size;
	FB_DBG("odinfb_alloc_fbmem rg_size = %d, size = %d\n",rg->size, size);
	rg->alloc = 1;

	return 0;

#endif
}

/* allocate fbmem using display resolution as reference */
static int odinfb_alloc_fbmem_display(struct fb_info *fbi, unsigned long size,
		unsigned long paddr, int index)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_device *fbdev = ofbi->fbdev;
	struct odin_dss_device *display;
	int bytespp;
	u16 w, h;

	FB_DBG("odinfb_alloc_fbmem_display\n");

	display =  fb2display(fbi);

	if (!display)
		return 0;

	switch (odinfb_get_recommended_bpp(fbdev, display)) {
	case 16:
		bytespp = 2;
		break;
	case 24:
		bytespp = 4;
		break;
	default:
		bytespp = 4;
		break;
	}

	if (!size) {
		display->driver->get_resolution(display, &w, &h);
		size = w * h * bytespp*2;
	}

	FB_DBG("odinfb_alloc_fbmem_display size = %d\n",size);

	if (!size)
		return 0;

	return odinfb_alloc_fbmem(fbi, size, paddr, index);
}

int odinfb_realloc_fbmem(struct fb_info *fbi, unsigned long size, int type)
{
	struct odinfb_info *ofbi = FB2OFB(fbi);
	struct odinfb_device *fbdev = ofbi->fbdev;
	struct odin_dss_device *display = fb2display(fbi);
	struct odinfb_mem_region *rg = ofbi->region;
	unsigned long old_size = rg->size;
	unsigned long old_paddr = rg->paddr;
	int old_type = rg->type;
	int r;

	if (type > ODINFB_MEMTYPE_MAX)
		return -EINVAL;

	size = PAGE_ALIGN(size);

	if (old_size == size && old_type == type)
		return 0;

	if (display && display->driver->sync)
			display->driver->sync(display);

	odinfb_free_fbmem(fbi);

	if (size == 0) {
		clear_fb_info(fbi);
		return 0;
	}

	r = odinfb_alloc_fbmem(fbi, size, 0, 0);

	if (r) {
		if (old_size)
			odinfb_alloc_fbmem(fbi, old_size, old_paddr, 0);

		if (rg->size == 0)
			clear_fb_info(fbi);

		return r;
	}

	if (old_size == size)
		return 0;

	if (old_size == 0) {
		FB_DBG("initializing fb %d\n", ofbi->id);
		r = odinfb_fb_init(fbdev, fbi);
		if (r) {
			FB_ERR("odinfb_fb_init failed\n");
			goto err;
		}
		r = odinfb_apply_changes(fbi, 1);
		if (r) {
			FB_ERR("odinfb_apply_changes failed\n");
			goto err;
		}
	} else {
		struct fb_var_screeninfo new_var;
		memcpy(&new_var, &fbi->var, sizeof(new_var));
		r = check_fb_var(fbi, &new_var);
		if (r)
			goto err;
		memcpy(&fbi->var, &new_var, sizeof(fbi->var));
		set_fb_fix(fbi);
	}

	return 0;
err:
	odinfb_free_fbmem(fbi);
	clear_fb_info(fbi);
	return r;
}

enum odin_color_fmt fb_format_to_dss_mode(enum odinfb_color_format fmt)
{
	enum odin_color_fmt mode;

	switch (fmt) {
	case ODINFB_COLOR_RGB565:
		mode = ODIN_DSS_COLOR_RGB565;
		break;

	case ODINFB_COLOR_YUV422S:
		mode = ODIN_DSS_COLOR_YUV422S;
		break;
	case ODINFB_COLOR_YUV422P_2:
		mode = ODIN_DSS_COLOR_YUV422P_2;
		break;
	case ODINFB_COLOR_YUV224P_2:
		mode = ODIN_DSS_COLOR_YUV224P_2;
		break;
	case ODINFB_COLOR_YUV420P_2:
		mode = ODIN_DSS_COLOR_YUV420P_2;
		break;
	case ODINFB_COLOR_YUV444P_2:
		mode = ODIN_DSS_COLOR_YUV444P_2;
		break;
	case ODINFB_COLOR_YUV422P_3:
		mode = ODIN_DSS_COLOR_YUV422P_3;
		break;
	case ODINFB_COLOR_YUV224P_3:
		mode = ODIN_DSS_COLOR_YUV224P_3;
		break;
	case ODINFB_COLOR_YUV420P_3:
		mode = ODIN_DSS_COLOR_YUV420P_3;
		break;
	case ODINFB_COLOR_YUV444P_3:
		mode = ODIN_DSS_COLOR_YUV444P_3;
		break;

	case ODINFB_COLOR_ARGB1555:
		mode = ODIN_DSS_COLOR_RGB555 | ODIN_DSS_COLOR_PIXEL_ALPHA;
		break;
	case ODINFB_COLOR_XRGB1555:
		mode = ODIN_DSS_COLOR_RGB555;
		break;
	case ODINFB_COLOR_ARGB4444:
		mode = ODIN_DSS_COLOR_RGB444 | ODIN_DSS_COLOR_PIXEL_ALPHA;
		break;
	case ODINFB_COLOR_XRGB4444:
		mode = ODIN_DSS_COLOR_RGB444;
		break;
	case ODINFB_COLOR_ARGB8888:
		mode = ODIN_DSS_COLOR_RGB888 | ODIN_DSS_COLOR_PIXEL_ALPHA;
		break;
	case ODINFB_COLOR_XRGB8888:
		mode = ODIN_DSS_COLOR_RGB888;
		break;
	case ODINFB_COLOR_RGBA8888:
		mode = ODIN_DSS_COLOR_RGB888 | ODIN_DSS_COLOR_ALPHA_RGBA |
						ODIN_DSS_COLOR_PIXEL_ALPHA;
		break;

	default:
		mode = -EINVAL;
	}

	return mode;
}

static int odinfb_allocate_all_fbs(struct odinfb_device *fbdev)
{
	int i, r;

	unsigned long sizes[10] = {0,};
	unsigned long paddrs[10] = {0,};

	if (fbdev->dev->platform_data) {
		struct odinfb_platform_data *opd;
		opd = fbdev->dev->platform_data;
		for (i = 0; i < opd->mem_desc.region_cnt; ++i) {
			sizes[i] = opd->mem_desc.region[i].size;
			paddrs[i] = opd->mem_desc.region[i].paddr;
		}
	}

	/* allocate memory automatically only for fb0, or if
	 * excplicitly defined with vram or plat data option */
	r = odinfb_alloc_fbmem_display(fbdev->fbs[0], sizes[0],
			(unsigned long)paddrs[0], 0);
	if (r)
		return r;
#ifdef DEBUG
	for (i = 0; i < fbdev->num_fbs; i++) {
		struct odinfb_info *ofbi = (struct odinfb_info *)FB2OFB(fbdev->fbs[i]);
		FB_DBG("framebuffer %d phys %08x virt %p size=%lu\n",
				i,
				ofbi->region->paddr,
				ofbi->region->vaddr,
				ofbi->region->size);
	}
#endif
	return 0;
}

/* initialize fb_info, var, fix to something sane based on the display */
static int odinfb_fb_init(struct odinfb_device *fbdev, struct fb_info *fbi)
{
	struct fb_var_screeninfo *var = &fbi->var;
	struct odin_dss_device *display = fb2display(fbi);
	struct odinfb_info *ofbi = FB2OFB(fbi);
	int r = 0;

	fbi->fbops = &odinfb_ops;
	fbi->flags = FBINFO_FLAG_DEFAULT;
	fbi->pseudo_palette = fbdev->pseudo_palette;
#if 0
	if (ofbi->region->size == 0) {
		clear_fb_info(fbi);
		return 0;
	}
#endif
	var->nonstd = 0;
	var->bits_per_pixel = 0;

	/*
	 * Check if there is a default color format set in the board file,
	 * and use this format instead the default deducted from the
	 * display bpp.
	 */
	if (fbdev->dev->platform_data) {
		struct odinfb_platform_data *opd;
		int id = ofbi->id;

		opd = fbdev->dev->platform_data;
		if (opd->mem_desc.region[id].format_used) {
			enum odin_color_mode mode;
			enum odinfb_color_format format;

			format = opd->mem_desc.region[id].format;
			mode = fb_format_to_dss_mode(format);

			FB_DBG("current mode = %d \n", mode);
			if (mode < 0) {
				r = mode;
				goto err;
			}
			r = dss_mode_to_fb_mode(mode, var);
			if (r < 0)
				goto err;
		}
	}

	if (display) {
		u16 w, h;

		display->driver->get_resolution(display, &w, &h);

		var->xres = w;
		var->yres = h;

		var->xres_virtual = var->xres;
		var->yres_virtual = var->yres * ODINFB_BUFFER_NUMBER;

		if (!var->bits_per_pixel) {
			switch (odinfb_get_recommended_bpp(fbdev, display)) {
			case 16:
				var->bits_per_pixel = 16;
				break;
			case 24:
				var->bits_per_pixel = 32;
				break;
			default:
				dev_err(fbdev->dev, "illegal display "
						"bpp\n");
				return -EINVAL;
			}
		}
		if (display->driver->set_fb_info)
			display->driver->set_fb_info(display, fbi);
	} else {
		/* if there's no display, let's just guess some basic values */
		var->xres = 320;
		var->yres = 240;
		var->xres_virtual = var->xres;
		var->yres_virtual = var->yres;
		if (!var->bits_per_pixel)
			var->bits_per_pixel = 16;
	}

	r = check_fb_var(fbi, var);
	if (r)
		goto err;

	set_fb_fix(fbi);
	r = fb_alloc_cmap(&fbi->cmap, 256, 0);
	if (r)
		dev_err(fbdev->dev, "unable to allocate color map memory\n");

err:
	return r;
}

static void fbinfo_cleanup(struct odinfb_device *fbdev, struct fb_info *fbi)
{
	fb_dealloc_cmap(&fbi->cmap);
}


static void odinfb_free_resources(struct odinfb_device *fbdev)
{
	int i;

	FB_DBG("free_resources\n");

	if (fbdev == NULL)
		return;

	for (i = 0; i < fbdev->num_fbs; i++)
		unregister_framebuffer(fbdev->fbs[i]);

	/* free the reserved fbmem */
	odinfb_free_all_fbmem(fbdev);

	for (i = 0; i < fbdev->num_fbs; i++) {
		fbinfo_cleanup(fbdev, fbdev->fbs[i]);
		framebuffer_release(fbdev->fbs[i]);
	}

	for (i = 0; i < fbdev->num_displays; i++) {
		if (fbdev->displays[i]->state != ODIN_DSS_DISPLAY_DISABLED)
			fbdev->displays[i]->driver->disable(fbdev->displays[i]);

		odin_dss_put_device(fbdev->displays[i]);
	}

	dev_set_drvdata(fbdev->dev, NULL);
	kfree(fbdev);
}

static int odinfb_create_framebuffers(struct odinfb_device *fbdev)
{
	int r, i;

	fbdev->num_fbs = 0;

	FB_DBG("create %d framebuffers\n",	CONFIG_ODIN_FB_NUM_FBS);

	/* allocate fb_infos */
	for (i = 0; i < CONFIG_ODIN_FB_NUM_FBS; i++) {
		struct fb_info *fbi;
		struct odinfb_info *ofbi;

		fbi = framebuffer_alloc(sizeof(struct odinfb_info),
				fbdev->dev);

		if (fbi == NULL) {
			dev_err(fbdev->dev,
				"unable to allocate memory for plane info\n");
			return -ENOMEM;
		}

		clear_fb_info(fbi);

		fbdev->fbs[i] = fbi;

		ofbi = FB2OFB(fbi);
		ofbi->fbdev = fbdev;
		ofbi->id = i;

		ofbi->region = &fbdev->regions[i];
		ofbi->region->id = i;
		init_rwsem(&ofbi->region->lock);

		ofbi->mirror = def_mirror;

		fbdev->num_fbs++;
	}

	FB_DBG("fb_infos allocated\n");

	/* assign overlays for the fbs */
	for (i = 0; i < min(fbdev->num_fbs, fbdev->num_overlays); i++) {
		struct odinfb_info *ofbi = FB2OFB(fbdev->fbs[i]);
		struct odin_overlay_manager *mgr;

		if (i == 0)
		{
			ofbi->overlays[0] = fbdev->overlays[PRIMARY_DEFAULT_OVERLAY];
		}
		else
		{
			ofbi->overlays[0] = fbdev->overlays[EXTERNAL_DEFAULT_OVERLAY];
			mgr = fbdev->displays[ODIN_EXTERNAL_LCD]->manager;
			ofbi->overlays[0]->set_manager(ofbi->overlays[0], mgr);
		}

		ofbi->num_overlays = 1;
	}

	/* allocate fb memories */
	r = odinfb_allocate_all_fbs(fbdev);
	if (r) {
		dev_err(fbdev->dev, "failed to allocate fbmem\n");
		return r;
	}

	FB_DBG("fbmems allocated\n");

	/* setup fb_infos */
	for (i = 0; i < fbdev->num_fbs; i++) {
		r = odinfb_fb_init(fbdev, fbdev->fbs[i]);
		if (r) {
			dev_err(fbdev->dev, "failed to setup fb_info\n");
			return r;
		}
	}

	FB_DBG("fb_infos initialized\n");

	for (i = 0; i < fbdev->num_fbs; i++) {
		r = register_framebuffer(fbdev->fbs[i]);
		if (r != 0) {
			dev_err(fbdev->dev,
				"registering framebuffer %d failed\n", i);
			return r;
		}
	}

	FB_DBG("framebuffers registered\n");

	for (i = 0; i < fbdev->num_fbs; i++) {
		r = odinfb_apply_changes(fbdev->fbs[i], 1);
		if (r) {
			dev_err(fbdev->dev, "failed to change mode\n");
			return r;
		}
	}

	FB_DBG("create sysfs for fbs\n");
	r = odinfb_create_sysfs(fbdev);
	if (r) {
		dev_err(fbdev->dev, "failed to create sysfs entries\n");
		return r;
	}

	/* Enable fb0 */
	if (fbdev->num_fbs > 0) {
		struct odinfb_info *ofbi = FB2OFB(fbdev->fbs[0]);

		if (ofbi->num_overlays > 0) {
			struct odin_overlay *ovl = ofbi->overlays[0];

			r = odinfb_overlay_enable(ovl, 1);

			if (r) {
				dev_err(fbdev->dev,
						"failed to enable overlay\n");
				return r;
			}
		}
	}

	FB_DBG("create_framebuffers done\n");

	return 0;
}

static int odinfb_mode_to_timings(const char *mode_str,
		struct odin_video_timings *timings, u8 *bpp)
{
	struct fb_info fbi;
	struct fb_var_screeninfo var;
	struct fb_ops fbops;
	int r;

	/* this is quite a hack, but I wanted to use the modedb and for
	 * that we need fb_info and var, so we create dummy ones */

	memset(&fbi, 0, sizeof(fbi));
	memset(&var, 0, sizeof(var));
	memset(&fbops, 0, sizeof(fbops));
	fbi.fbops = &fbops;

	r = fb_find_mode(&var, &fbi, mode_str, NULL, 0, NULL, 24);

	if (r != 0) {
		timings->pixel_clock = PICOS2KHZ(var.pixclock);
		timings->hfp = var.left_margin;
		timings->hbp = var.right_margin;
		timings->vfp = var.upper_margin;
		timings->vbp = var.lower_margin;
		timings->hsw = var.hsync_len;
		timings->vsw = var.vsync_len;
		timings->x_res = var.xres;
		timings->y_res = var.yres;

		switch (var.bits_per_pixel) {
		case 16:
			*bpp = 16;
			break;
		case 24:
		case 32:
		default:
			*bpp = 24;
			break;
		}

		return 0;
	} else {
		return -EINVAL;
	}
}

static int odinfb_set_def_mode(struct odinfb_device *fbdev,
		struct odin_dss_device *display, char *mode_str)
{
	int r;
	u8 bpp;
	struct odin_video_timings timings;

	r = odinfb_mode_to_timings(mode_str, &timings, &bpp);
	if (r)
		return r;

	fbdev->bpp_overrides[fbdev->num_bpp_overrides].dssdev = display;
	fbdev->bpp_overrides[fbdev->num_bpp_overrides].bpp = bpp;
	++fbdev->num_bpp_overrides;

	if (!display->driver->check_timings || !display->driver->set_timings)
		return -EINVAL;

	r = display->driver->check_timings(display, &timings);
	if (r)
		return r;

	display->driver->set_timings(display, &timings);

	return 0;
}

static int odinfb_get_recommended_bpp(struct odinfb_device *fbdev,
		struct odin_dss_device *dssdev)
{
	int i;

	BUG_ON(dssdev->driver->get_recommended_bpp == NULL);

	for (i = 0; i < fbdev->num_bpp_overrides; ++i) {
		if (dssdev == fbdev->bpp_overrides[i].dssdev)
			return fbdev->bpp_overrides[i].bpp;
	}

	return dssdev->driver->get_recommended_bpp(dssdev);
}

static int odinfb_parse_def_modes(struct odinfb_device *fbdev)
{
	char *str, *options, *this_opt;
	int r = 0;

	str = kmalloc(strlen(def_mode) + 1, GFP_KERNEL);
	strcpy(str, def_mode);
	options = str;

	while (!r && (this_opt = strsep(&options, ",")) != NULL) {
		char *p, *display_str, *mode_str;
		struct odin_dss_device *display;
		int i;

		p = strchr(this_opt, ':');
		if (!p) {
			r = -EINVAL;
			break;
		}

		*p = 0;
		display_str = this_opt;
		mode_str = p + 1;

		display = NULL;
		for (i = 0; i < fbdev->num_displays; ++i) {
			if (strcmp(fbdev->displays[i]->name,
						display_str) == 0) {
				display = fbdev->displays[i];
				break;
			}
		}

		if (!display) {
			r = -EINVAL;
			break;
		}

		r = odinfb_set_def_mode(fbdev, display, mode_str);
		if (r)
			break;
	}

	kfree(str);

	return r;
}

#ifdef ODINFB_FENCE_SYNC
static void odinfb_commit_wq_handler(struct work_struct *work)
{
	struct odinfb_device *fbdev;
	struct fb_var_screeninfo *var;
	struct fb_info *info;
	struct odinfb_backup_type *fb_backup;
	int ret;

	fbdev = container_of(work, struct odinfb_device, commit_work);
	fb_backup = (struct odinfb_backup_type *)fbdev->odinfb_backup;
/*	info = &fb_backup->info; */
	if (fb_backup->disp_commit.flags &
			ODINFB_DISPLAY_COMMIT_OVERLAY) {
		/*odinfb_wait_for_fence(fbdev);*/
		/*if (mfd->mdp.kickoff_fnc)
			mfd->mdp.kickoff_fnc(mfd); */
		odinfb_signal_timeline(fbdev);
	}

	mutex_lock(&fbdev->sync_mutex);
	fbdev->is_committing = 0;
	complete_all(&fbdev->commit_comp);
	mutex_unlock(&fbdev->sync_mutex);
}
#endif

#if 0
static void odinfb_send_vsync_work(struct work_struct *work)
{
	struct odinfb_device *fbdev =
		container_of(work, typeof(*fbdev), vsync_work);
	char buf[64];
	char *envp[2];

	snprintf(buf, sizeof(buf), "VSYNC=%llu",
		ktime_to_ns(fbdev->vsync_timestamp));
	envp[0] = buf;
	envp[1] = NULL;
	kobject_uevent_env(&fbdev->dev->kobj, KOBJ_CHANGE, envp);

	/* DSSERR("odinfb_send_vsync_work = %s\n", buf); */
}
#endif

static void odinfb_vsync_isr(void *data, u32 mask, u32 sub_mask)
{
	struct odinfb_device *fbdev = data;
	fbdev->vsync_timestamp = ktime_get();

	/*complete(&fbdev->vsync_comp);*/
	/*schedule_work(&fbdev->vsync_work);*/
}

int odinfb_enable_vsync(struct odinfb_device *fbdev)
{
	int r;
	if (dssdev_manually_updated(fbdev->displays[0]))
	{
		r = 0;
		/* TODO */
	}
	else{
		r = odin_crg_register_isr(odinfb_vsync_isr, fbdev, CRG_IRQ_VSYNC_MIPI0, 0);
	}
	return r;
}

void odinfb_disable_vsync(struct odinfb_device *fbdev)
{
	if (dssdev_manually_updated(fbdev->displays[0]))
	{
		/* TODO */
	}
	else {
		odin_crg_unregister_isr(odinfb_vsync_isr, fbdev, CRG_IRQ_VSYNC_MIPI0, 0);
	}
}

bool odinfb_ovl_iova_check(struct ion_handle_data handle_data)
{
	u32 iova_addr;
	bool ret_val;
	iova_addr = odin_ion_get_iova_of_buffer(handle_data.handle, ODIN_SUBSYS_DSS);

#if 0
	ret_val =  odin_du_iova_check(iova_addr);

	if (ret_val == false)
		msleep(50);
#else
	ret_val  = odin_mgr_iova_check(iova_addr);
#endif

	return ret_val;
}

extern atomic_t	vsync_ready;

#define FB_RAM_SIZE                SZ_16M /* 1920 * 1080 *4 * 2 */
#define FB_RAM_SIZE_FPGA           0x300000 /* 800 * 480 *4 * 2 */

struct odinfb_platform_data fb_pdata = {
        .mem_desc = {
                .region_cnt = 2,
                .region = {
                        [0] = {
#if defined (CONFIG_MACH_HUINS_FPGA) || defined (CONFIG_MACH_CLAIR_FPGA)
                                .paddr = 0x8F000000,
				.size = FB_RAM_SIZE_FPGA,
#else
                                .paddr = 0xFB000000,
				.size = FB_RAM_SIZE,
#endif
                                .format = ODINFB_COLOR_ARGB8888,
                                .format_used = 1,
                        },
                        [1] = {
#if defined (CONFIG_MACH_HUINS_FPGA) || defined (CONFIG_MACH_CLAIR_FPGA)
                                .paddr = 0x8F300000,
				.size = FB_RAM_SIZE_FPGA,
#else
				.paddr = 0xFC000000,
				.size = FB_RAM_SIZE,
#endif
                                .format = ODINFB_COLOR_ARGB8888,
                                .format_used = 1,
                        },
                },
        },
};

static struct platform_device odinfb_device = {
	.name		= "odinfb",
	.id		= -1,
};

static int odinfb_probe(struct platform_device *pdev)
{
	struct odinfb_device *fbdev = NULL;
	int r = 0;
	int r2 = 0;
	int i;
	struct odin_overlay *ovl;
	struct odin_dss_device *def_display;
	struct odin_dss_device *dssdev;

	/* FB_INFO("odinfb_probe\n"); */

	pdev->dev.platform_data = &fb_pdata;

	fbdev = kzalloc(sizeof(struct odinfb_device), GFP_KERNEL);
	if (fbdev == NULL) {
		r = -ENOMEM;
		goto err0;
	}

	mutex_init(&fbdev->mtx);
#if 0
	mutex_init(&fbdev->sync_mutex);
#endif
	fbdev->dev = &pdev->dev;
	platform_set_drvdata(pdev, fbdev);

	r = 0;
	fbdev->num_displays = 0;
	dssdev = NULL;

	/* search for odin_dss_device registered to odindss bus */
	for_each_dss_dev(dssdev) {
		odin_dss_get_device(dssdev);

		if (!dssdev->driver) {
			dev_err(&pdev->dev, "no driver for display\n");
			r = -ENODEV;
		}
		/* in case of odin, the number of display is two or three. */
		fbdev->displays[fbdev->num_displays++] = dssdev;
	}

	if (r)
		goto cleanup;

	if (fbdev->num_displays == 0) {
		dev_err(&pdev->dev, "no displays\n");
		r = -EINVAL;
		goto cleanup;
	}

	/* not fixed. may be overlay is 7 */
	fbdev->num_overlays = odin_ovl_dss_get_num_overlays();
	for (i = 0; i < fbdev->num_overlays; i++)
		fbdev->overlays[i] = odin_ovl_dss_get_overlay(i);

	/* overlay hw device is consist of 3*/
	fbdev->num_managers = odin_mgr_dss_get_num_overlay_managers();
	for (i = 0; i < fbdev->num_managers; i++)
		fbdev->managers[i] = odin_mgr_dss_get_overlay_manager(i);

	/* TODO: I don't know what effects to this driver by below code */

	if (def_mode && strlen(def_mode) > 0) {
		if (odinfb_parse_def_modes(fbdev))
			dev_warn(&pdev->dev, "cannot parse default modes\n");
	}

	r = odinfb_create_framebuffers(fbdev);
	if (r)
		goto cleanup;

	/* We wil move to bootolader */
#if 0
	r = odinfb_boot_logo_image(fbdev);
	if (r)
		goto cleanup;
#endif

	/* ODINFB_DEFAULT_OVERLAY should be the default one. find a display
	 * connected to that, and use it as default display */

	ovl = odin_ovl_dss_get_overlay(PRIMARY_DEFAULT_OVERLAY);
	if (ovl->manager && ovl->manager->device) {
		def_display = ovl->manager->device;
	} else {
		dev_warn(&pdev->dev, "cannot find default display\n");
		def_display = NULL;
	}

	if (def_display) {
		def_display->boot_display = true;
		r = def_display->driver->enable(def_display);
		if (r) {
			dev_warn(fbdev->dev, "Failed to enable display '%s'\n",
					def_display->name);
			goto cleanup;
		}

		odin_dsscomp_set_default_overlay(ovl->id, ovl->manager->id);
	}

	for (i = 0; i < fbdev->num_managers; i++) {
		u16 w, h;
		struct odin_overlay_manager *mgr;
		struct odin_overlay_manager_info info;
		mgr = fbdev->managers[i];
#if 1
		mgr->fbdev = fbdev;
#endif
		mgr->get_manager_info(mgr, &info);
		if ((mgr->device) && (mgr->device->driver))
			mgr->device->driver->get_resolution(def_display, &w, &h);
		info.size_x = w;
		info.size_y = h;
		mgr->set_manager_info(mgr, &info);
		r = mgr->apply(mgr, DSSCOMP_SETUP_MODE_DISPLAY);
		if (r)
			dev_dbg(fbdev->dev, "failed to apply duconfig\n");
	}

	if (def_display) {
		if (dssdev_manually_updated(def_display)) {
			struct odin_dss_driver *dssdrv = def_display->driver;
			u16 w, h;
			unsigned long flags;
			unsigned long timeout = msecs_to_jiffies(500);

			dssdrv->get_resolution(def_display, &w, &h);

#if RUNTIME_CLK_GATING
			/* in booting time, clk_status started as 1,
			   so this if loop may not be executed. */
			if (!check_clk_gating_status())
			{
				enable_runtime_clk (def_display->manager, false);
				def_display->manager->clk_enable(1 << ODIN_DSS_GRA0, 0, false);
			}
#endif

#ifdef DSS_DFS_MUTEX_LOCK
			mutex_lock(&dfs_mtx);
#elif defined(DSS_DFS_SPINLOCK)
			get_mem_dfs();
#endif
			dssdrv->sync(def_display);
			def_display->driver->update(def_display, 0, 0, w, h);

			r2 = def_display->manager->wait_for_framedone(def_display->manager, timeout);
			r = dssdrv->flush_pending(def_display);
			if (r2 < 0)
				DSSERR("odinfb_apply framedone timeout error\n");

			if (r < 0)
				DSSERR("odinfb_apply flush_pending error\n");
#ifdef DSS_DFS_MUTEX_LOCK
			mutex_unlock(&dfs_mtx);
#elif defined(DSS_DFS_SPINLOCK)
			put_mem_dfs();
#endif
#if RUNTIME_CLK_GATING
			disable_runtime_clk (ovl->manager->id); /* 1->0 */
#endif
		}
	}
	FB_DBG("mgr->apply'ed\n");

	/*INIT_WORK(&fbdev->vsync_work, odinfb_send_vsync_work);*/
	/*init_completion(&fbdev->vsync_comp);*/

#ifdef ODINFB_FENCE_SYNC
	init_completion(&fbdev->commit_comp);
	INIT_WORK(&fbdev->commit_work, odinfb_commit_wq_handler);
	/* odinfb_backup initialize */
	fbdev->odinfb_backup = kzalloc(sizeof(struct odinfb_backup_type),
			GFP_KERNEL);
	if (fbdev->odinfb_backup == 0) {
		pr_err("error: not enough memory!\n");
		return -ENOMEM;
	}

	/* timeline init */
	if (fbdev->timeline == NULL) {
		fbdev->timeline = sw_sync_timeline_create("odinfb-timeline");
		if (fbdev->timeline == NULL) {
			pr_err("%s: cannot create time line", __func__);
			return -ENOMEM;
		} else {
			fbdev->timeline_value = 0;
		}
	}
#endif

	r = dsscomp_ion_client_create();
	if (r < 0)
		goto cleanup;

	return 0;

cleanup:
	odinfb_free_resources(fbdev);
err0:
	dev_err(&pdev->dev, "failed to setup odinfb\n");
	return r;
}

static int odinfb_remove(struct platform_device *pdev)
{
	struct odinfb_device *fbdev = platform_get_drvdata(pdev);

	/* FIXME: wait till completion of pending events */

	odinfb_remove_sysfs(fbdev);
	odinfb_free_resources(fbdev);

	return 0;
}

static struct platform_driver odinfb_driver = {
	.probe          = odinfb_probe,
	.remove         = odinfb_remove,
	.driver         = {
		.name   = "odinfb",
		.owner  = THIS_MODULE,
	},
};

static int __init odinfb_init(void)
{
	//FB_WARN("odinfb_init\n");

	if (platform_device_register(&odinfb_device)) {
		printk(KERN_ERR "failed to register odinfb device\n");
		return -ENODEV;
	}

	if (platform_driver_register(&odinfb_driver)) {
		printk(KERN_ERR "failed to register odinfb driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit odinfb_exit(void)
{
	FB_WARN("odinfb_exit\n");
	platform_driver_unregister(&odinfb_driver);
}


/* late_initcall to let panel/ctrl drivers loaded first.
 * I guess better option would be a more dynamic approach,
 * so that odinfb reacts to new panels when they are loaded */
late_initcall(odinfb_init);
/*module_init(odinfb_init);*/
module_exit(odinfb_exit);

MODULE_AUTHOR("Seungwon Shin <m4seungwon.shin@lge.com");
MODULE_DESCRIPTION("ODIN Framebuffer");
MODULE_LICENSE("GPL v2");

