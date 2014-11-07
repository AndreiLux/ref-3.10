/*
 * ISP interface driver
 *
 * Copyright (C) 2010 - 2013 Mtekvision
 * Author: DongHyung Ko <kodh@mtekvision.com>
 *         Jinyoung Park <parkjyb@mtekvision.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/******************************************************************************
	Internal header files
******************************************************************************/
#if CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#endif

#include <linux/module.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include "odin-css.h"
#include "odin-isp.h"
#include "odin-isp-wrap.h"


struct ispwrap_info {
	spinlock_t slock;
	s32 sensor_sel;
	u32 outpath_sel;
	u32 mem_crtl;
	struct css_isp_wrap_config wrap0_config;
	struct css_isp_wrap_config wrap1_config;
};

/******************************************************************************
	Static variables
******************************************************************************/
static struct ispwrap_hardware *ispwrapper_hardware = NULL;
static struct ispwrap_info		ispwrapinfo;

/******************************************************************************
	Function definitions
******************************************************************************/
static inline void set_ispwrap_hw(struct ispwrap_hardware *hw)
{
	ispwrapper_hardware = hw;
}

static inline struct ispwrap_hardware *get_ispwrap_hw(void)
{
	BUG_ON(!ispwrapper_hardware);
	return ispwrapper_hardware;
}

static inline struct ispwrap_info *get_ispwarp_info(void)
{
	return &ispwrapinfo;
}

static inline struct css_isp_wrap_config *get_ispwarp0_config(void)
{
	return &ispwrapinfo.wrap0_config;
}

static inline struct css_isp_wrap_config *get_ispwarp1_config(void)
{
	return &ispwrapinfo.wrap1_config;
}

static inline u32 odin_isp_wrap_get_reg(u32 path, u32 offset)
{
	struct ispwrap_hardware *ispwrap_hw = get_ispwrap_hw();
	u32 val;

	val = readl(ispwrap_hw->iobase[path] + offset);

	/*
	css_afifo("%s(): [%s] Offset(%p) readl(0x%08X)\n", __func__,
		(path == 1) ? "AFIFO_1" : "AFIFO_0",
		(void *)(ispwrap_hw->iobase[path] + offset), val);
	*/

	return val;
}

static inline void odin_isp_wrap_set_reg(u32 val, u32 path, u32 offset)
{
	struct ispwrap_hardware *ispwrap_hw = get_ispwrap_hw();

	writel(val, ispwrap_hw->iobase[path] + offset);

	/*
	css_afifo("%s(): [%s] Offset(%p) writel(0x%08X)\n", __func__,
		(path == 1) ? "AFIFO_1" : "AFIFO_0",
		(void *)(ispwrap_hw->iobase[path] + offset), val);
	*/
}


void odin_isp_warp_reg_dump(u32 idx)
{
	int i = 0;
	struct ispwrap_hardware *ispwrap_hw = get_ispwrap_hw();

	if (ispwrap_hw == NULL)
		return;

	css_info("isp wrap%d reg dump\n", idx);
	for (i = 0; i < 4; i++) {
		printk("0x%08x ", readl(ispwrap_hw->iobase[idx] + (i * 0x10)));
		printk("0x%08x ", readl(ispwrap_hw->iobase[idx] + (i * 0x10) + 0x4));
		printk("0x%08x ", readl(ispwrap_hw->iobase[idx] + (i * 0x10) + 0x8));
		printk("0x%08x\n", readl(ispwrap_hw->iobase[idx] + (i * 0x10) + 0xC));
		schedule();
	}
}

u32 odin_isp_wrap_get_primary_sensor_size(void)
{
	u32 val;
	struct ispwrap_hardware *ispwrap_hw = get_ispwrap_hw();

	val = readl(ispwrap_hw->iobase[0] + OFFSET_PRIMARY_SENSOR_IMG_SIZE);

	return val;
}

u32 odin_isp_wrap_get_secondary_sensor_size(void)
{
	u32 val;
	struct ispwrap_hardware *ispwrap_hw = get_ispwrap_hw();

	val = readl(ispwrap_hw->iobase[1] + OFFSET_SECONDARY_SENSOR_IMG_SIZE);

	return val;
}

s32 odin_isp_wrap_control_param(struct css_afifo_ctrl *phandle)
{
	struct css_afifo_ctrl *config = phandle;
	CSS_ISP_WRAP_CTRL ctrl;
	u32 path = config->index;
	s32 ret = CSS_SUCCESS;

	ctrl.as32bits = odin_isp_wrap_get_reg(path, OFFSET_ASYNCBUF_CTRL);

	switch (path) {
	case ISP_WRAP0:
		ctrl.asbits.fr_sensor = OFF;
		ctrl.asbits.senclksel = OFF;
		break;

	case ISP_WRAP1:
		ctrl.asbits.fr_sensor = ON;
		ctrl.asbits.senclksel = ON;
		break;
	}

	switch (config->format) {
	case CAMERA_FORMAT_BAYER_RAW10:
		css_afifo("Sensor source is BAYER\n");
		ctrl.asbits.sensor_type = SENSOR_TYPE_BAYER;
		ctrl.asbits.yuvsensel = OFF;
		break;
	case CAMERA_FORMAT_YUV420:
		css_afifo("Sensor source is YUV\n");
		ctrl.asbits.sensor_type = SENSOR_TYPE_YUV;
		ctrl.asbits.yuvsensel = ON;
		return CSS_FAILURE;
	default:
		css_err("Unsupported sensor source\n");
		return CSS_FAILURE;
	}

	css_afifo("%s(): [%s]Value(0x%08X)\n", __func__,
		(path == ISP_WRAP0) ? "AFIFO_0" : "AFIFO_1", ctrl.as32bits);

	odin_isp_wrap_set_reg(ctrl.as32bits, path, OFFSET_ASYNCBUF_CTRL);

	return ret;
}

s32 odin_isp_wrap_pre_param(struct css_afifo_pre *phandle)
{
	struct css_afifo_pre *config = phandle;
	CSS_ISP_WRAP_BAYER_PRE_CTRL pre_buf;
	u32 path = config->index;
	s32 ret = CSS_SUCCESS;

	pre_buf.as32bits = 0;

	switch (config->format) {
	case CAMERA_FORMAT_BAYER_RAW8:
		pre_buf.asbits.raw_align = RAW8_DATA_CUT_ALIGN_MSB;
		pre_buf.asbits.raw_mode_sel = RAW_BIT_MODE8;
		pre_buf.asbits.raw_mask_sel = RAW8_DATA_CUT_7_0;
		break;

	case CAMERA_FORMAT_BAYER_RAW10:
		pre_buf.asbits.raw_align = RAW8_DATA_CUT_ALIGN_2BIT;
		pre_buf.asbits.raw_mode_sel = RAW_BIT_MODE10;
		pre_buf.asbits.raw_mask_sel = RAW10_DATA_CUT_9_0;
		break;

	case CAMERA_FORMAT_BAYER_RAW12:
		pre_buf.asbits.raw_align = RAW8_DATA_CUT_ALIGN_LSB;
		pre_buf.asbits.raw_mode_sel = RAW_BIT_MODE12;
		break;
	default:
		css_err("Raw Align and Bit mode is not set");
		break;
	}
	pre_buf.asbits.in_width = config->width;	/* Must be sensor size */
	pre_buf.asbits.bayer_en = 0x01;
	pre_buf.asbits.vsync_pol = OFF;
	pre_buf.asbits.param_set = ON;
	pre_buf.asbits.vsync_delay = 0x1;

	css_afifo("%s(): [%s]Value(0x%08X)\n", __func__,
		(path == ISP_WRAP0) ? "AFIFO_0" : "AFIFO_1",
		pre_buf.as32bits);

	odin_isp_wrap_set_reg(pre_buf.as32bits, path,
						OFFSET_ASYNCBUF_BAYER_PRE_PATHCTRL);

	return ret;
}

s32 odin_isp_wrap_post_param(struct css_afifo_post *phandle)
{
	struct css_afifo_post *config = phandle;
	CSS_ISP_WRAP_BAYER_POST_CTRL post_buf;
	u32 path = config->index;
	s32 ret = CSS_SUCCESS;

	post_buf.as32bits			= 0;
	post_buf.asbits.out_width	= config->width;	/* Inputted ISP Width Size */
	post_buf.asbits.bayer_en	= ON;
	post_buf.asbits.param_set	= ON;
	post_buf.asbits.vsync_delay	= 0x8;

	css_afifo("%s(): [%s]Value(0x%08X)\n", __func__,
		(path == ISP_WRAP0) ? "AFIFO_0" : "AFIFO_1",
		post_buf.as32bits);

	odin_isp_wrap_set_reg(post_buf.as32bits, path,
						OFFSET_ASYNCBUF_BAYER_POST_PATHCTRL);

	return ret;
}

s32 odin_isp_wrap_bayer_param(struct css_bayer_scaler *phandle)
{
	struct css_bayer_scaler *config = phandle;
	CSS_ISP_WRAP_BAYER_SCALER param;
	CSS_ISP_WRAP_BAYER_SCALER_ENABLE enable;
	CSS_ISP_WRAP_BAYER_SCALER_OFFSET offset;
	CSS_ISP_WRAP_BAYER_SCALER_MASK mask;
	CSS_ISP_WRAP_BAYER_SCALER_RATIO ratio;
	u32 path = config->index;
	s32 ret = CSS_SUCCESS;

	param.as32bits	= 0;
	enable.as32bits	= 0;
	offset.as32bits	= 0;
	mask.as32bits	= 0;
	ratio.as32bits	= 0;

	/* Param Set */
	param.asbits.enable = ON;
	param.asbits.vsyncdly = 0x01;

	if (CAMERA_FORMAT_YUV422 == config->format)
		param.asbits.outwidth = config->dest_w * 2;
	else
		param.asbits.outwidth = config->dest_w;

	/* Enable Control */
	enable.asbits.enable = ON;

	if (config->crop_en == ON) {
		/* Crop Enable */
		enable.asbits.masken = ON;
		offset.asbits.offsetx = config->offset_x;
		offset.asbits.offsety = config->offset_y;

		css_afifo("Cropping ON\n");
	} else {
		/* Crop Disable */
		enable.asbits.masken = OFF;
		offset.asbits.offsetx = 0;
		offset.asbits.offsety = 0;

		css_afifo("Cropping OFF\n");
	}

	mask.asbits.maskx = config->src_w;	/* Input Sensor Width */
	mask.asbits.masky = config->src_h;	/* Input Sensor Height */

	if (config->scl_en == ON) {
		if (config->dest_w <= (config->src_w >> 1) &&
			config->dest_h <= (config->src_h >> 1)) {
			/* Down scale Enable */
			enable.asbits.sclen = ON;
			enable.asbits.avgen = ON;
			css_afifo("Donw Scaling ON\n");
		} else {
			css_err("Could not resizing W(%d) H(%d) -> W(%d) H(%d)",
				config->src_w, config->src_h, config->dest_w, config->dest_h);

			enable.asbits.sclen = OFF;
		}
	} else {
		/* Down scale Disable */
		enable.asbits.sclen = OFF;

		css_afifo("Down Scaling OFF\n");
	}

	/* Ratio */
	ratio.asbits.ratiox = (config->src_w * 2048) / config->dest_w;
	ratio.asbits.ratioy = (config->src_h * 2048) / config->dest_h;

	css_afifo("Bayer[%d] PARAM Value(0x%08X)\n", path, param.as32bits);
	css_afifo("Bayer[%d] ENABLE Value(0x%08X)\n", path, enable.as32bits);
	css_afifo("Bayer[%d] OFFSET Value(0x%08X)\n", path, offset.as32bits);
	css_afifo("Bayer[%d] MASK Value(0x%08X)\n", path, mask.as32bits);
	css_afifo("Bayer[%d] RATIO Value(0x%08X)\n", path, ratio.as32bits);

	odin_isp_wrap_set_reg(param.as32bits, path, OFFSET_ASYNCBUF_BAYER_SCALER);
	odin_isp_wrap_set_reg(enable.as32bits, path, OFFSET_BAYER_SCALER_ENABLE);
	odin_isp_wrap_set_reg(offset.as32bits, path, OFFSET_BAYER_SCALER_OFFSET);
	odin_isp_wrap_set_reg(mask.as32bits, path, OFFSET_BAYER_SCALER_MASK);
	odin_isp_wrap_set_reg(ratio.as32bits, path, OFFSET_BAYER_SCALER_RATIO);

	return ret;
}

s32 odin_isp_wrap_change_resolution(u32 path,
									struct css_bayer_scaler *bayer_scl)
{
	struct css_isp_wrap_config *config = NULL;
	struct ispwrap_info *info = NULL;
	u32 sensor_type = -1;
	u32 val = 0x0;
	unsigned long flags;

	info = get_ispwarp_info();

	if (info == NULL) {
		css_err("Failed to get info handler\n");
		return CSS_FAILURE;
	}

	switch (path) {
	case ISP_WRAP0:
		config = &info->wrap0_config;
		break;
	case ISP_WRAP1:
		config = &info->wrap1_config;
		break;
	default:
		css_err("Invalid AsyncFIFO path\n");
		return CSS_FAILURE;
	}

	if (config == NULL) {
		css_err("Failed to get config handler\n");
		return CSS_FAILURE;
	}

	val = odin_isp_wrap_get_reg(path, OFFSET_ASYNCBUF_CTRL);

	sensor_type = (val & 0x00000001U);

	spin_lock_irqsave(&info->slock, flags);
	/* Input Description */
	config->input.margin.start_x	= bayer_scl->margin_x;
	config->input.margin.start_y	= bayer_scl->margin_y;
	config->input.size.width	= bayer_scl->src_w;
	config->input.size.height	= bayer_scl->src_h;
	config->input.format		= CAMERA_FORMAT_BAYER_RAW10;

	/* Output Description */
	config->output.offset.start_x	= bayer_scl->offset_x;
	config->output.offset.start_y	= bayer_scl->offset_y;
	config->output.mask.width	= bayer_scl->dest_w;
	config->output.mask.height	= bayer_scl->dest_h;

	config->wrap_sel	= path;
	config->delay		= 0x00;
	config->crop_enable		= bayer_scl->crop_en;
	config->scaling_enable	= bayer_scl->scl_en;

	switch (sensor_type) {
	case SENSOR_TYPE_BAYER:
		css_afifo("BAYER sensor source is selected\n");
		config->output.format = CAMERA_FORMAT_BAYER_RAW10;
		spin_unlock_irqrestore(&info->slock, flags);
		odin_isp_wrap_bayer_param(config);
		break;
	case SENSOR_TYPE_YUV:
		css_afifo("YUV sensor source is selected\n");
		config->output.format = CAMERA_FORMAT_YUV420;
		spin_unlock_irqrestore(&info->slock, flags);
		return CSS_FAILURE;
	/*
		default:
		css_err("Not supported sensor source\n");
		spin_unlock_irqrestore(&info->slock, flags);
		return CSS_FAILURE;
	 */
	}

	css_afifo("%s(): [%s]Path: %s\n", __func__,
		(path == ISP_WRAP0) ? "AFIFO_0" : "AFIFO_1",
		(config->wrap_sel == ISP_WRAP0) ? "AFIFO_0" : "AFIFO_1");

	css_afifo("%s(): [%s]Src W(%d)H(%d) FMT(0x%02X) MarginX(%d) MarginY(%d)\n",
		__func__,
		(path == ISP_WRAP0) ? "AFIFO_0" : "AFIFO_1",
		config->input.size.width,
		config->input.size.height,
		config->input.format,
		config->input.margin.start_x,
		config->input.margin.start_y);

	css_afifo("%s(): [%s]Dest W(%d)H(%d) FMT(0x%02X) OffsetX(%d) OffsetY(%d)\n",
		__func__,
		(path == ISP_WRAP0) ? "AFIFO_0" : "AFIFO_1",
		config->output.mask.width,
		config->output.mask.height,
		config->output.format,
		config->output.offset.start_x,
		config->output.offset.start_y);

	odin_isp_wrap_pre_param(config);
	odin_isp_wrap_post_param(config);

	return CSS_SUCCESS;
}

s32 odin_isp_wrap_get_read_path_status(u32 path)
{
	CSS_ISP_WRAP_CTRL ctrl;
	ctrl.as32bits = odin_isp_wrap_get_reg(path, OFFSET_ASYNCBUF_CTRL);
	css_sys("isp %d readpath en: %d\n", path, ctrl.asbits.readpath_en);
	return (s32)ctrl.asbits.readpath_en;
}

s32 odin_isp_wrap_set_read_path_enable(u32 path)
{
	CSS_ISP_WRAP_CTRL ctrl0;
	CSS_ISP_WRAP_CTRL ctrl1;

	switch (path) {
	case ISP_WRAP0:
		ctrl0.as32bits = odin_isp_wrap_get_reg(ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
		ctrl0.asbits.readpath_en = 1;
		odin_isp_wrap_set_reg(ctrl0.as32bits, ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
		break;
	case ISP_WRAP1:
		ctrl1.as32bits = odin_isp_wrap_get_reg(ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);
		ctrl1.asbits.readpath_en = 1;
		odin_isp_wrap_set_reg(ctrl1.as32bits, ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);
		break;
	default:
		css_err("Could not set async buffer path 0x%2d...", path);
		return CSS_FAILURE;
	}
	return CSS_SUCCESS;
}

s32 odin_isp_wrap_get_isp_idx(u32 scaler)
{
	CSS_ISP_WRAP_CTRL ctrl;

	ctrl.as32bits = 0;

	if (scaler > CSS_SCALER_1) {
		css_warn("invalid scaler index %d\n", scaler);
	}

	if (scaler == 0)
		ctrl.as32bits = odin_isp_wrap_get_reg(ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
	else
		ctrl.as32bits = odin_isp_wrap_get_reg(ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);

	if (ctrl.asbits.path1_mode == 1)
		return !scaler;
	else
		return scaler;
}

s32 odin_isp_wrap_set_read_path_disable(u32 path)
{
	CSS_ISP_WRAP_CTRL ctrl0;
	CSS_ISP_WRAP_CTRL ctrl1;

	switch (path) {
	case ISP_WRAP0:
		ctrl0.as32bits = odin_isp_wrap_get_reg(ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
		ctrl0.asbits.readpath_en = 0;
		odin_isp_wrap_set_reg(ctrl0.as32bits, ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
		break;
	case ISP_WRAP1:
		ctrl1.as32bits = odin_isp_wrap_get_reg(ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);
		ctrl1.asbits.readpath_en = 0;
		odin_isp_wrap_set_reg(ctrl1.as32bits, ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);
		break;
	default:
		css_err("Could not set async buffer path 0x%2d...", path);
		return CSS_FAILURE;
	}
	return CSS_SUCCESS;
}

s32 odin_isp_wrap_set_mem_ctrl(u32 mode)
{
	struct ispwrap_info *info = NULL;
	CSS_ISP_WRAP_CTRL ctrl;
	u32 val0 = 0;
	u32 val1 = 0;
	unsigned long flags;

	info = get_ispwarp_info();
	if (info == NULL) {
		css_err("get ispwrap info fail!\n");
		return -1;
	}

	spin_lock_irqsave(&info->slock, flags);
	info->mem_crtl = mode;
	spin_unlock_irqrestore(&info->slock, flags);

	ctrl.as32bits			= 0x0;
	ctrl.asbits.linememmode	= SRAM_DISABLE;
	val0 = odin_isp_wrap_get_reg(ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
	val0 &= ~(ctrl.asbits.linememmode << 6);
	val1 = odin_isp_wrap_get_reg(ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);
	val1 &= ~(ctrl.asbits.linememmode << 6);

	switch (mode) {
	case DUAL_SRAM_ENABLE:
		ctrl.asbits.linememmode = DUAL_SRAM_ENABLE;
		val0 |= (ctrl.asbits.linememmode << 6);
		val1 |= (ctrl.asbits.linememmode << 6);
		odin_isp_wrap_set_reg(val0, ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
		odin_isp_wrap_set_reg(val1, ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);

		css_afifo("%s(): [AFIFO_0] DUAL_SRAM_ENABLE(0x%08X)\n", __func__, val0);
		css_afifo("%s(): [AFIFO_1] DUAL_SRAM_ENABLE(0x%08X)\n", __func__, val1);

		break;
	case S3DC_SRAM_ENABLE:
		ctrl.asbits.linememmode = S3DC_SRAM_ENABLE;
		val0 |= (ctrl.asbits.linememmode << 6);
		val1 |= (ctrl.asbits.linememmode << 6);
		odin_isp_wrap_set_reg(val0, ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
		odin_isp_wrap_set_reg(val1, ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);

		css_afifo("%s(): [AFIFO_0] S3DC_SRAM_ENABLE(0x%08X)\n", __func__, val0);
		css_afifo("%s(): [AFIFO_1] S3DC_SRAM_ENABLE(0x%08X)\n", __func__, val1);

		break;
	case SRAM_DISABLE:
		ctrl.asbits.linememmode = SRAM_DISABLE;
		val0 &= ~(ctrl.asbits.linememmode << 6);
		val1 &= ~(ctrl.asbits.linememmode << 6);
		odin_isp_wrap_set_reg(val0, ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
		odin_isp_wrap_set_reg(val1, ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);

		css_afifo("%s(): [AFIFO_0] SRAM_DISABLE(0x%08X)\n", __func__, val0);
		css_afifo("%s(): [AFIFO_1] SRAM_DISABLE(0x%08X)\n", __func__, val1);

		break;
	}

	return CSS_SUCCESS;
}

s32 odin_isp_wrap_config(u32 path, u32 sensor_w, u32 sensor_h)
{
	struct css_isp_wrap_config *config = NULL;
	struct ispwrap_info *info = NULL;
	unsigned long flags;

	info = get_ispwarp_info();
	if (info == NULL) {
		css_err("get ispwrap info fail!\n");
		return -1;
	}

	switch (path) {
	case ISP_WRAP0:
		config = &info->wrap0_config;
		break;
	case ISP_WRAP1:
		config = &info->wrap1_config;
		break;
	default:
		css_err("invalid ispwrap num(%d)!\n", path);
		return -1;
	}

	if (config == NULL) {
		css_err("get ispwrap config fail!\n");
		return -1;
	}

	spin_lock_irqsave(&info->slock, flags);
	/* Input Description */
	config->input.margin.start_x	= 0;
	config->input.margin.start_y	= 0;
	config->input.size.width	= sensor_w;
	config->input.size.height	= sensor_h;
	config->input.format		= CAMERA_FORMAT_BAYER_RAW10;

	/* Output Description */
	config->output.offset.start_x	= 0;
	config->output.offset.start_y	= 0;
	config->output.mask.width	= sensor_w;	/* Default setting (1:1 - By pass)*/
	config->output.mask.height	= sensor_h; /* Default setting (1:1 - By pass)*/
	config->output.format		= CAMERA_FORMAT_BAYER_RAW10;

	config->wrap_sel	= path;
	config->delay		= 0x00;
	config->crop_enable	= OFF;
	config->scaling_enable	= OFF;
	spin_unlock_irqrestore(&info->slock, flags);

	css_afifo("%s(): [%s]Path: %s\n", __func__,
		(path == ISP_WRAP0) ? "AFIFO_0" : "AFIFO_1",
		(config->wrap_sel == ISP_WRAP0) ? "AFIFO_0" : "AFIFO_1");

	css_afifo("%s(): [%s]Src W(%d)H(%d) FMT(0x%02X) MarginX(%d) MarginY(%d)\n",
		__func__,
		(path == ISP_WRAP0) ? "AFIFO_0" : "AFIFO_1",
		config->input.size.width,
		config->input.size.height,
		config->input.format,
		config->input.margin.start_x,
		config->input.margin.start_y);

	css_afifo("%s(): [%s]Dest W(%d)H(%d) FMT(0x%02X) OffsetX(%d) OffsetY(%d)\n",
		__func__,
		(path == ISP_WRAP0) ? "AFIFO_0" : "AFIFO_1",
		config->output.mask.width,
		config->output.mask.height,
		config->output.format,
		config->output.offset.start_x,
		config->output.offset.start_y);

	odin_isp_wrap_control_param(config);
	odin_isp_wrap_bayer_param(config);
	odin_isp_wrap_pre_param(config);
	odin_isp_wrap_post_param(config);

	return CSS_SUCCESS;
}

s32 odin_isp_wrap_get_status(void)
{
	u32 val0 = 0x0;
	u32 val1 = 0x0;

	val0 = odin_isp_wrap_get_reg(ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
	val1 = odin_isp_wrap_get_reg(ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);

	return CSS_SUCCESS;
}

s32 odin_isp_wrap_set_sensor_sel(u32 index)
{
	struct ispwrap_info *info = NULL;
	CSS_ISP_WRAP_CTRL ctrl;
	u32 val0 = 0x0;
	u32 val1 = 0x0;
	unsigned long flags;

	info = get_ispwarp_info();
	if (info == NULL) {
		css_err("get ispwrap info fail!\n");
		return -1;
	}

	spin_lock_irqsave(&info->slock, flags);
	info->sensor_sel = index;
	spin_unlock_irqrestore(&info->slock, flags);

	ctrl.as32bits			= 0x0;
	ctrl.asbits.fr_sensor	= 1;
	ctrl.asbits.senclksel	= 1;

	val0 = odin_isp_wrap_get_reg(ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
	val1 = odin_isp_wrap_get_reg(ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);

	switch (index) {
	case SENSOR_REAR:
		css_afifo("%s(): REAR\n", __func__);
		val0 &= ~(ctrl.as32bits);
		odin_isp_wrap_set_reg(val0, ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
		break;
	case SENSOR_FRONT:
		css_afifo("%s(): FRONT\n", __func__);
		val1 |= ctrl.as32bits;
		odin_isp_wrap_set_reg(val1, ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);
		break;
	case SENSOR_S3D:
		css_afifo("%s(): S3D\n", __func__);
		val0 &= ~(ctrl.as32bits);
		val1 &= ~(ctrl.as32bits);
		odin_isp_wrap_set_reg(val0, ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
		odin_isp_wrap_set_reg(val1, ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);
		break;
	case SENSOR_DUMMY:
		 css_afifo("SENSOR_DUMMY: Nothing to do...\n");
		 break;
	default:
		css_err("Unknown sensor ID...\n");
		break;
	}

	return CSS_SUCCESS;
}

void odin_isp_wrap_set_live_path(struct css_afifo_pre *phandle, u32 enable)
{
	struct ispwrap_hardware 	*ispwrap_hw = get_ispwrap_hw();
	struct css_afifo_pre		*config 	= phandle;
	CSS_ISP_WRAP_BAYER_PRE_CTRL pre_buf;
	CSS_ISP_WRAP_CTRL			ctrl;

	if (ispwrap_hw->livepath == 0) {
		css_warn("live path not supported!");
		return;
	}

	pre_buf.as32bits = odin_isp_wrap_get_reg(ISP_WRAP1,
							OFFSET_ASYNCBUF_BAYER_PRE_PATHCTRL);
	ctrl.as32bits = odin_isp_wrap_get_reg(ISP_WRAP1,
							OFFSET_ASYNCBUF_CTRL);

	if (enable) {
		pre_buf.asbits.raw_mode_sel = LIVE_PATH_SET;
		css_info("live path enabled 0x%x\n", pre_buf.asbits.raw_mode_sel);
	}
	else {
		switch (config->format) {
		case CAMERA_FORMAT_BAYER_RAW8:
			pre_buf.asbits.raw_align   = RAW8_DATA_CUT_ALIGN_MSB;
			pre_buf.asbits.raw_mode_sel = RAW_BIT_MODE8;
			pre_buf.asbits.raw_mask_sel = RAW8_DATA_CUT_7_0;
			break;

		case CAMERA_FORMAT_BAYER_RAW10:
			pre_buf.asbits.raw_align   = RAW8_DATA_CUT_ALIGN_2BIT;
			pre_buf.asbits.raw_mode_sel = RAW_BIT_MODE10;
			pre_buf.asbits.raw_mask_sel = RAW10_DATA_CUT_9_0;
			break;

		case CAMERA_FORMAT_BAYER_RAW12:
			pre_buf.asbits.raw_align   = RAW8_DATA_CUT_ALIGN_LSB;
			pre_buf.asbits.raw_mode_sel = RAW_BIT_MODE12;
			break;
		default:
			css_err("Raw Align and Bit mode is not set");
			break;
		}
		css_info("live path disabled 0x%x\n", pre_buf.asbits);
	}

	odin_isp_wrap_set_reg(pre_buf.as32bits, ISP_WRAP1,
						  OFFSET_ASYNCBUF_BAYER_PRE_PATHCTRL);

	ctrl.asbits.preupdate = 1;
	odin_isp_wrap_set_reg(ctrl.as32bits, ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);

	ctrl.asbits.preupdate = 0;
	odin_isp_wrap_set_reg(ctrl.as32bits, ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);

	return;
}

s32 odin_isp_wrap_set_path_sel(u32 path)
{
	struct ispwrap_info *info = NULL;
	CSS_ISP_WRAP_CTRL ctrl;
	u32 val0 = 0x0;
	u32 val1 = 0x0;
	unsigned long flags;

	info = get_ispwarp_info();
	if (info == NULL) {
		css_err("get ispwrap info fail!\n");
		return -1;
	}

	spin_lock_irqsave(&info->slock, flags);
	info->outpath_sel = path;
	spin_unlock_irqrestore(&info->slock, flags);

	ctrl.as32bits = 0x0;
	ctrl.asbits.path1_mode = 1;

	val0 = odin_isp_wrap_get_reg(ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
	val1 = odin_isp_wrap_get_reg(ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);

	css_info("isp wrap set path = %d\n", path);

	switch (path) {
	case ISP0_LINKTO_SC0_ISP0_LINKTO_SC1:
		val0 &= ~(ctrl.asbits.path1_mode << 8);
		val1 |= (ctrl.asbits.path1_mode	<< 8);
		css_afifo("%s(): [AFIFO_0] 00(0x%08X)\n", __func__, val0);
		css_afifo("%s(): [AFIFO_1] 00(0x%08X)\n", __func__, val1);
		break;
	case ISP0_LINKTO_SC0_ISP1_LINKTO_SC1:
		 val0 &= ~(ctrl.asbits.path1_mode << 8);
		 val1 &= ~(ctrl.asbits.path1_mode << 8);
		 css_afifo("%s(): [AFIFO_0] 01(0x%08X)\n", __func__, val0);
		 css_afifo("%s(): [AFIFO_1] 01(0x%08X)\n", __func__, val1);
		 break;
	case ISP0_LINKTO_SC1_ISP1_LINKTO_SC0:
		val0 |= (ctrl.asbits.path1_mode << 8);
		val1 |= (ctrl.asbits.path1_mode << 8);
		css_afifo("%s(): [AFIFO_0] 10(0x%08X)\n", __func__, val0);
		css_afifo("%s(): [AFIFO_1] 10(0x%08X)\n", __func__, val1);
		break;
	case ISP1_LINKTO_SC0_ISP1_LINKTO_SC1:
		val0 |= (ctrl.asbits.path1_mode << 8);
		val1 &= ~(ctrl.asbits.path1_mode << 8);
		css_afifo("%s(): [AFIFO_0] 11(0x%08X)\n", __func__, val0);
		css_afifo("%s(): [AFIFO_1] 11(0x%08X)\n", __func__, val1);
		break;
	default:
		css_err("Could not set async buffer path 0x%2d...", path);
		break;
	}

	odin_isp_wrap_set_reg(val0, ISP_WRAP0, OFFSET_ASYNCBUF_CTRL);
	odin_isp_wrap_set_reg(val1, ISP_WRAP1, OFFSET_ASYNCBUF_CTRL);

	return CSS_SUCCESS;
}

s32 odin_isp_wrap_get_sensor_sel(void)
{
	struct ispwrap_info *info = NULL;

	info = get_ispwarp_info();
	if (info == NULL) {
		css_err("get ispwrap info fail!\n");
		return -1;
	}

	return info->sensor_sel;
}

s32 odin_isp_wrap_get_resolution(u32 path, struct css_image_size *img_size)
{
	struct css_device *cssdev = get_css_plat();

	if (img_size == NULL) {
		css_err("invalid input param!\n");
		return FAIL;
	}

	/* Bayer Scaler output size */
	img_size->width  = cssdev->afifo.bayer_scl[path].dest_w;
	img_size->height = cssdev->afifo.bayer_scl[path].dest_h;

	css_afifo("Bayer[%d]: Dest W(%d) H(%d)\n", path,
			cssdev->afifo.bayer_scl[path].dest_w,
			cssdev->afifo.bayer_scl[path].dest_h);

	return 0;
}

s32 odin_isp_wrap_get_path_sel(void)
{
	struct ispwrap_info *info = NULL;

	info = get_ispwarp_info();
	if (info == NULL) {
		css_err("get ispwrap info fail!\n");
		return -1;
	}

	return info->outpath_sel;
}

s32 odin_isp_wrap_get_mem_ctrl(void)
{
	struct ispwrap_info *info = NULL;

	info = get_ispwarp_info();
	if (info == NULL) {
		css_err("get ispwrap info fail!\n");
		return -1;
	}

	return info->mem_crtl;
}

s32 odin_isp_wrap_init_resources(struct platform_device *pdev)
{
	struct ispwrap_info *info = NULL;
	struct ispwrap_hardware *ispwrap_hw = NULL;
	struct css_device *cssdev = NULL;
	struct resource *res_ispwrap = NULL;
	unsigned long size_ispwrap = 0;
	s32 idx = 0;
	s32 ret = CSS_SUCCESS;

	idx = pdev->id;
	if (idx == 0) {
		info = get_ispwarp_info();
		if (info == NULL)
			return CSS_FAILURE;
		memset(info, 0x0, sizeof(struct ispwrap_info));
		spin_lock_init(&info->slock);
	}

	cssdev = get_css_plat();
	if (cssdev == NULL) {
		css_err("can't get cssdev!\n");
		return CSS_FAILURE;
	}

	ispwrap_hw = (struct ispwrap_hardware *)&cssdev->ispwrap_hw;
	if (ispwrap_hw == NULL) {
		css_err("can't get ispwrap_hw!\n");
		return CSS_FAILURE;
	}

	ispwrap_hw->pdev[idx] = pdev;

	platform_set_drvdata(pdev, ispwrap_hw);

	res_ispwrap = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res_ispwrap == NULL) {
		css_err("failed to get isp-wrap[%02d] resource!\n", idx);
		return CSS_ERROR_GET_RESOURCE;
	}
	size_ispwrap = res_ispwrap->end - res_ispwrap->start + 1;

	ispwrap_hw->iores[idx] = request_mem_region(res_ispwrap->start,
												size_ispwrap, pdev->name);
	if (ispwrap_hw->iores[idx] == NULL) {
		css_err("failed to request isp-wrap[%02d] mem region : %s!\n",
			idx, pdev->name);
		return CSS_ERROR_GET_RESOURCE;
	}

	ispwrap_hw->iobase[idx] = ioremap_nocache(res_ispwrap->start,
											  size_ispwrap);
	if (ispwrap_hw->iobase[idx] == NULL) {
		css_err("failed to isp-wrap[%02d] ioremap!\n", idx);
		ret = CSS_ERROR_IOREMAP;
		goto error_ioremap;
	}

	ispwrap_hw->livepath = cssdev->livepath_support;

	set_ispwrap_hw(ispwrap_hw);

	return CSS_SUCCESS;

error_ioremap:
	if (NULL != ispwrap_hw->iores[idx]) {
		release_mem_region(ispwrap_hw->iores[idx]->start,
						   ispwrap_hw->iores[idx]->end -
						   ispwrap_hw->iores[idx]->start + 1);

		ispwrap_hw->iores[idx] = NULL;
	}

	return ret;
}

s32 odin_isp_wrap_deinit_resources(struct platform_device *pdev)
{
	struct ispwrap_hardware *ispwrap_hw = NULL;
	s32 idx = 0;
	s32 ret = CSS_SUCCESS;

	idx = pdev->id;

	ispwrap_hw = platform_get_drvdata(pdev);
	if (ispwrap_hw->iobase[idx]) {
		iounmap(ispwrap_hw->iobase[idx]);
		ispwrap_hw->iobase[idx] = NULL;
	}

	if (ispwrap_hw->iores[idx]) {
		release_mem_region(ispwrap_hw->iores[idx]->start,
						   ispwrap_hw->iores[idx]->end -
						   ispwrap_hw->iores[idx]->start + 1);

		ispwrap_hw->iores[idx] = NULL;
	}
	return ret;
}

s32 odin_isp_wrap_probe(struct platform_device *pdev)
{
	s32 val, idx;
	s32 ret = CSS_SUCCESS;

#if CONFIG_OF
	of_property_read_u32(pdev->dev.of_node, "id",&val);
	pdev->id = val;
#endif

	idx = pdev->id;

	ret = odin_isp_wrap_init_resources(pdev);
	if (ret == CSS_SUCCESS) {
		css_info("isp-wrap%02d probe ok!!\n", idx);
	} else {
		css_err("isp-wrap%02d probe err!!\n", idx);
	}

	return ret;
}

s32 odin_isp_wrap_remove(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;

	ret = odin_isp_wrap_deinit_resources(pdev);
	if (ret == CSS_SUCCESS) {
		css_info("isp-wrap%02d remove ok!!\n", pdev->id);
	} else {
		css_err("isp-wrap%02d remove err!!\n", pdev->id);
	}

	return ret;
}

s32 odin_isp_wrap_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	return ret;
}

s32 odin_isp_wrap_resume(struct platform_device *pdev)
{
	int ret = 0;
	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id odin_isp_wrap_match[] = {
	{ .compatible = CSS_OF_ISP_WRAP_NAME},
	{},
};
#endif /* CONFIG_OF */

struct platform_driver odin_isp_wrap_driver =
{
	.probe		= odin_isp_wrap_probe,
	.remove 	= odin_isp_wrap_remove,
	.suspend	= odin_isp_wrap_suspend,
	.resume 	= odin_isp_wrap_resume,
	.driver 	= {
		.name	= CSS_PLATDRV_ISP_WRAP,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_isp_wrap_match),
#endif
	},
};

static int __init odin_isp_wrap_init(void)
{
	return platform_driver_register(&odin_isp_wrap_driver);
}

static void __exit odin_isp_wrap_exit(void)
{
	platform_driver_unregister(&odin_isp_wrap_driver);
	return;
}

MODULE_DESCRIPTION("odin isp wrap driver");
MODULE_AUTHOR("MTEKVISION<http://www.mtekvision.com>");

late_initcall(odin_isp_wrap_init);
module_exit(odin_isp_wrap_exit);

MODULE_LICENSE("GPL");

