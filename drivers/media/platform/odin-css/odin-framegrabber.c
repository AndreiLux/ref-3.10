/*
 * Frame Grabber driver
 *
 * Copyright (C) 2013 Mtekvision
 * Author: Daeheon Kim <kimdh@mtekvision.com>
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

#include <linux/slab.h>
#include <linux/platform_device.h>
#include <asm/io.h>

#if CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#include <linux/delay.h>

#include "odin-css.h"
#include "odin-css-system.h"
#include "odin-css-clk.h"
#include "odin-framegrabber.h"
#include "odin-css-utils.h"

#define scaler_check_and_set_clk(scl_clk_ctrl, clk_src, check_src, member)	\
	 if (clk_src & check_src) {					\
		 if (scl_clk_ctrl.asbits.member == 0) {	\
			 scl_clk_ctrl.asbits.member = 1;	\
		 }										\
	 }

#define scaler_check_and_clr_clk(scl_clk_ctrl, clk_src, check_src, member)	\
	 if (clk_src & check_src) { 				\
		 if (scl_clk_ctrl.asbits.member == 1) {	\
			 scl_clk_ctrl.asbits.member = 0;	\
		 }										\
	 }

#define SCALER_0_VSYNC_DLY	0
#define SCALER_1_VSYNC_DLY	0
#define SCALER_FD_VSYNC_DLY	0
#define SCALER_STROBE_TIME	0
#define SCALER_STROBE_HIGH	0

static int scaler_crop_mask_configure(css_scaler_select scaler,
				struct css_scaler_config *scl_config,
				struct css_scaler_data *scl_data);
static int scaler_hw_set_instruction_mode(css_scaler_select scaler,
				css_scaler_instruction_mode inst_mode);
static int scaler_hw_set_parameter(CSS_SCALER_PATH_PARAM_SET param);
static int scaler_hw_set_crop_mask(css_scaler_select scaler,
				struct css_scaler_data* scl_data);
static int scaler_hw_set_effect(css_scaler_select scaler,
				struct css_effect_set *effects);
static int scaler_hw_set_color_format(css_scaler_select scaler,
				css_pixel_format pix_fmt);
static int scaler_hw_set_dither_enable(css_scaler_select scaler,
				struct css_dithering_set *DitherSet);
static int scaler_hw_set_dither_disable(css_scaler_select scaler);
static int scaler_hw_get_dither_set(css_scaler_select scaler,
				struct css_dithering_set *DitherSet);
static int scaler_hw_set_strobe(struct css_strobe_set *StrobeSet);
static int scaler_hw_get_strobe(struct css_strobe_set *StrobeSet);
static int scaler_hw_set_sw_address_reset(css_scaler_select scaler);
static int scaler_hw_set_sw_address(unsigned int index, unsigned int y_addr,
					unsigned int cb_addr, unsigned int cr_addr);
static int scaler_hw_set_fd_address(int addr);
static int scaler_hw_set_fd_source(int source);
static int scaler_hw_set_vsync_delay(css_scaler_select scaler,
				unsigned short delay);
static int scaler_hw_path_reset(unsigned int path_reset);
static int scaler_hw_path_clk_enable(unsigned int clk_src);
static int scaler_hw_path_clk_disable(unsigned int clk_src);
static int scaler_hw_set_cache_control(unsigned int on);
static int scaler_hw_set_int_enable(unsigned int int_flag);
static int scaler_hw_set_int_disable(unsigned int int_flag);
static int scaler_hw_set_int_clr(unsigned int int_flag);
static int scaler_bayer_load_reset(css_zsl_path r_path);
static int scaler_hw_set_raw0_store(unsigned int bit_mode, unsigned int width,
			unsigned int height, unsigned int store_addr);
static int scaler_hw_clr_raw0_store(void);
static int scaler_hw_set_raw1_store(unsigned int bit_mode, unsigned int width,
			unsigned int height, unsigned int store_addr);
static int scaler_hw_clr_raw1_store(void);
static int scaler_hw_clr_raw0_load(void);
static int scaler_hw_clr_raw1_load(void);
static int scaler_hw_set_raw0_load(unsigned int bit_mode,
			css_scaler_load_img_format ld_fmt,
			unsigned int width, unsigned int height,
			unsigned int load_addr, unsigned int blank_count);
static int scaler_hw_set_raw1_load(unsigned int bit_mode,
			css_scaler_load_img_format ld_fmt,
			unsigned int width, unsigned int height,
			unsigned int load_addr, unsigned int blank_count);
static unsigned int scaler_hw_get_scl_0_frame_count(void);
static unsigned int scaler_hw_get_scl_1_frame_count(void);
static unsigned int scaler_hw_reset_scl_0_frame_count(void);
static unsigned int scaler_hw_reset_scl_1_frame_count(void);
static void scaler_hw_get_destination_size(css_scaler_select scaler,
			unsigned int *dst_w, unsigned int *dst_h);
static int scaler_hw_get_pixel_format(css_scaler_select scaler);
static CSS_SCALER_REG_DATA *scaler_register = NULL;
static struct scaler_hardware *scl_hardware = NULL;

static inline void set_scl_hw(struct scaler_hardware *hw)
{
	scl_hardware = hw;
}

static inline struct scaler_hardware *get_scl_hw(void)
{
	BUG_ON(!scl_hardware);
	return scl_hardware;
}

irqreturn_t scaler_irq(int irq, void *dev_id)
{
	irqreturn_t ret = IRQ_NONE;
	struct scaler_hardware *scl_hw = get_scl_hw();

	if (scl_hw->irq_handler)
		ret = scl_hw->irq_handler(irq, dev_id);

	return ret;
}

irqreturn_t scaler_default_irq(int irq, void *dev_id)
{
	irqreturn_t ret = IRQ_NONE;
	css_warn("scalr isr is default!\n");
	return ret;
}

static int odin_scaler_init_resources(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;
	struct css_device *cssdev = NULL;
	struct scaler_hardware *scl_hw = NULL;
	struct resource *iores = NULL;
	u64 size_iores;

	cssdev = get_css_plat();
	if (!cssdev) {
		css_err("can't get cssdev!\n");
		return CSS_FAILURE;
	}

	scl_hw = (struct scaler_hardware *)&cssdev->scl_hw;
	if (!scl_hw) {
		css_err("can't get scl_hw!\n");
		return CSS_FAILURE;
	}

	scl_hw->pdev = pdev;
	platform_set_drvdata(pdev, scl_hw);

	atomic_set(&scl_hw->users[0], 0);
	atomic_set(&scl_hw->users[1], 0);
	atomic_set(&scl_hw->users[2], 0);

	spin_lock_init(&scl_hw->slock);
	mutex_init(&scl_hw->hwmutex);
	/* get register physical base address to resource of platform device */
	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iores) {
		css_err("failed to get resource!\n");
		return CSS_ERROR_GET_RESOURCE;
	}

	size_iores = iores->end - iores->start + 1;
	scl_hw->iores = request_mem_region(iores->start, size_iores, pdev->name);
	if (!scl_hw->iores) {
		css_err("failed to request mem region : %s!\n", pdev->name);
		return CSS_ERROR_GET_RESOURCE;
	}

	scl_hw->iobase = ioremap_nocache(iores->start, size_iores);
	if (!scl_hw->iobase) {
		css_err("failed to scaler ioremap!\n");
		ret = CSS_ERROR_IOREMAP;
		goto error_ioremap;
	}

	scl_hw->irq = platform_get_irq(pdev, 0);
	if (scl_hw->irq < 0) {
		css_err("failed to get scaler irq number\n");
		ret = CSS_ERROR_GET_RESOURCE;
		goto error_irq;
	}

	ret = request_irq(scl_hw->irq, scaler_irq , 0, "scaler", cssdev);
	if (ret) {
		css_err("failed to request irq %d\n", ret);
		goto error_irq;
	}

	disable_irq(scl_hw->irq);

	css_info("scaler irq=%d registered!\n", scl_hw->irq);

	set_scl_hw(scl_hw);

	scaler_register = (CSS_SCALER_REG_DATA*)scl_hw->iobase;

	return CSS_SUCCESS;

error_irq:
	if (scl_hw->iobase) {
		iounmap(scl_hw->iobase);
		scl_hw->iobase = NULL;
	}
error_ioremap:
	if (scl_hw->iores) {
		release_mem_region(iores->start, iores->end - iores->start + 1);
		scl_hw->iores = NULL;
	}
	return ret;
}

static int odin_scaler_deinit_resources(struct platform_device *pdev)
{
	struct scaler_hardware *scl_hw = NULL;
	int ret = CSS_SUCCESS;

	scl_hw = platform_get_drvdata(pdev);

	if (scl_hw) {
		if (scl_hw->iobase) {
			iounmap(scl_hw->iobase);
			scl_hw->iobase = NULL;
		}
		if (scl_hw->iores) {
			release_mem_region(scl_hw->iores->start,
								scl_hw->iores->end - scl_hw->iores->start + 1);
			scl_hw->iores = NULL;
		}
		set_scl_hw(NULL);
	}

	return ret;
}


void scaler_reg_dump(void)
{
	int i = 0;
	struct scaler_hardware *scl_hw = get_scl_hw();

	if (scl_hw == NULL)
		return;

	css_info("scaler reg dump 1\n");
	for (i = 0; i < 10; i++) {
		printk("0x%08x ", readl(scl_hw->iobase + (i * 0x10)));
		printk("0x%08x ", readl(scl_hw->iobase + (i * 0x10) + 0x4));
		printk("0x%08x ", readl(scl_hw->iobase + (i * 0x10) + 0x8));
		printk("0x%08x\n", readl(scl_hw->iobase + (i * 0x10) + 0xC));
		schedule();
	}
	printk("0x%08x ", readl(scl_hw->iobase + 0x168));
	printk("0x%08x ", readl(scl_hw->iobase + 0x16C));
	printk("0x%08x ", readl(scl_hw->iobase + 0x170));
	printk("0x%08x\n", readl(scl_hw->iobase + 0x174));
	msleep(300);
	css_info("scaler reg dump 2\n");
	for (i = 0; i < 10; i++) {
		printk("0x%08x ", readl(scl_hw->iobase + (i * 0x10)));
		printk("0x%08x ", readl(scl_hw->iobase + (i * 0x10) + 0x4));
		printk("0x%08x ", readl(scl_hw->iobase + (i * 0x10) + 0x8));
		printk("0x%08x\n", readl(scl_hw->iobase + (i * 0x10) + 0xC));
		schedule();
	}
	printk("0x%08x ", readl(scl_hw->iobase + 0x168));
	printk("0x%08x ", readl(scl_hw->iobase + 0x16C));
	printk("0x%08x ", readl(scl_hw->iobase + 0x170));
	printk("0x%08x\n", readl(scl_hw->iobase + 0x174));
}

unsigned int scaler_register_isr(struct scaler_hardware *scl_hw,
								irq_handler_t irq_handler)
{
	int ret = CSS_SUCCESS;

	if (scl_hw == NULL)
		return CSS_FAILURE;

	if (irq_handler == NULL)
		scl_hw->irq_handler = scaler_default_irq;
	else
		scl_hw->irq_handler = irq_handler;

	return ret;
}

unsigned int scaler_get_int_status(void)
{
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_INT_FLG scl_int_flag;

	scl_int_flag.as32bits = readl(scl_hw->iobase + INT_FLG);

	return scl_int_flag.as32bits;
}

unsigned int scaler_get_int_mask(void)
{
	int mask_val = 0;

	struct scaler_hardware *scl_hw = get_scl_hw();

	mask_val = readl(scl_hw->iobase + INT_MASK);

	return (mask_val & 0xFFFF);
}

unsigned int scaler_get_done_status(void)
{
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_DONE_STATE scl_done_state;

	scl_done_state.as32bits = readl(scl_hw->iobase + DONE_ST);

	return scl_done_state.as32bits;
}

unsigned int scaler_get_param_status(void)
{
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PARA_SET scl_param_state;

	scl_param_state.as32bits = readl(scl_hw->iobase + PARA_SET);

	return scl_param_state.as32bits;
}

unsigned int scaler_get_href_cnt(css_scaler_select scaler)
{
	struct scaler_hardware *scl_hw = get_scl_hw();
	unsigned int href_cnt = 0;
	CSS_SCALER_PATH0_SCALER_MONITOR scl_path0_scl_monitor;
	CSS_SCALER_PATH1_SCALER_MONITOR scl_path1_scl_monitor;

	switch (scaler) {
	case CSS_SCALER_0:
		scl_path0_scl_monitor.as32bits
						= readl(scl_hw->iobase + PATH0_SCL_MONITOR);
		href_cnt = scl_path0_scl_monitor.asbits.href_monitor_0;
		break;
	case CSS_SCALER_1:
		scl_path1_scl_monitor.as32bits
						= readl(scl_hw->iobase + PATH1_SCL_MONITOR);
		href_cnt = scl_path1_scl_monitor.asbits.href_monitor_1;
		break;
	default:
		break;
	}

	return href_cnt;
}

unsigned int scaler_get_err_status(void)
{
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_ERROR_STATE scl_err_state;

	scl_err_state.as32bits = readl(scl_hw->iobase + ERR_ST);

	return scl_err_state.as32bits;
}

int scaler_get_ready_state(void)
{
	struct scaler_hardware *scl_hw = get_scl_hw();

	if (atomic_read(&scl_hw->users[0]) == 0 &&
		atomic_read(&scl_hw->users[1]) == 0 &&
		atomic_read(&scl_hw->users[2]) == 0) {
		return 0;
	}
	return 1;
}

void scaler_int_clear(unsigned int int_flag)
{
	scaler_hw_set_int_clr(int_flag);
}

void scaler_int_enable(unsigned int int_flag)
{
	scaler_hw_set_int_enable(int_flag);
}

void scaler_int_disable(unsigned int int_flag)
{
	scaler_hw_set_int_disable(int_flag);
}

unsigned int scaler_get_frame_num(css_scaler_select scaler)
{
	unsigned int fcnt = 0;

	switch (scaler) {
	case CSS_SCALER_0:
		fcnt = scaler_hw_get_scl_0_frame_count();
		break;
	case CSS_SCALER_1:
		fcnt = scaler_hw_get_scl_1_frame_count();
		break;
	default:
		break;
	}
	return fcnt;
}

unsigned int scaler_get_frame_width(css_scaler_select scaler)
{
	unsigned int w, h;
	scaler_hw_get_destination_size(scaler, &w, &h);
	return w;
}

unsigned int scaler_get_frame_height(css_scaler_select scaler)
{
	unsigned int w, h;
	scaler_hw_get_destination_size(scaler, &w, &h);
	return h;
}

unsigned int scaler_get_frame_size(css_scaler_select scaler)
{
	unsigned int w, h;
	int fmt;
	unsigned int frmsize = 0;
	unsigned int img_size;

	scaler_hw_get_destination_size(scaler, &w, &h);
	fmt = scaler_hw_get_pixel_format(scaler);

	img_size = (w * h * 2);

	switch (fmt) {
	case -1:
		/* GS8 */
		frmsize = img_size/2;
		break;
	case CSS_COLOR_YUV422:
		frmsize = img_size;
		break;
	case CSS_COLOR_YUV420:
		frmsize = img_size * 3/4;
		break;
	case CSS_COLOR_RGB565:
		break;
	}

	return frmsize;
}

unsigned int scaler_reset_frame_num(css_scaler_select scaler)
{
	int ret = CSS_SUCCESS;

	switch (scaler) {
	case CSS_SCALER_0:
		scaler_hw_reset_scl_0_frame_count();
		break;
	case CSS_SCALER_1:
		scaler_hw_reset_scl_1_frame_count();
		break;
	default:
		break;
	}
	return ret;
}

int scaler_path_reset(unsigned int path)
{
	int ret = CSS_SUCCESS;

	path &= 0xFFF;

	ret = scaler_hw_path_reset(path);

	return ret;
}

int scaler_path_clk_control(unsigned int path, unsigned int onoff)
{
	int ret = CSS_SUCCESS;

	path &= 0xFFF;

	if (onoff == 0)
		scaler_hw_path_clk_disable(path);
	else
		scaler_hw_path_clk_enable(path);

	return ret;
}

int scaler_init_device(css_scaler_select scaler)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();

	if (scaler > CSS_SCALER_FD)
		return CSS_ERROR_INVALID_SCALER_NUM;

	mutex_lock(&scl_hw->hwmutex);
	switch (scaler) {
	case CSS_SCALER_0:
		atomic_inc(&scl_hw->users[CSS_SCALER_0]);
		break;
	case CSS_SCALER_1:
		atomic_inc(&scl_hw->users[CSS_SCALER_1]);
		break;
	case CSS_SCALER_FD:
		atomic_inc(&scl_hw->users[CSS_SCALER_FD]);
		break;
	}

	if (css_clock_available(CSS_CLK_FRGB) == 0) {
		css_clock_enable(CSS_CLK_FRGB);
		css_block_reset(BLK_RST_FRGB);

		scaler_path_reset(PATH_RESET_ALL);
		scaler_path_clk_control(PATH_ALL_CLK, CLOCK_OFF);
		scaler_hw_set_cache_control(1);
#ifdef ENABLE_ERR_INT_CODE
		scaler_int_enable(INT_ERROR);
#endif
		enable_irq(scl_hw->irq);
	}

	switch (scaler) {
	case CSS_SCALER_0:
		if (atomic_read(&scl_hw->users[CSS_SCALER_0]) == 1) {
			scaler_path_reset(PATH_0_RESET);
			scaler_path_clk_control(PATH_0_CLK, CLOCK_ON);
			scaler_hw_set_sw_address_reset(CSS_SCALER_0);
			scaler_int_disable(INT_PATH0_BUSI);
			css_info("frgb0 reset\n");
		}
		break;
	case CSS_SCALER_1:
		if (atomic_read(&scl_hw->users[CSS_SCALER_1]) == 1) {
			scaler_path_reset(PATH_1_RESET);
			scaler_path_clk_control(PATH_1_CLK, CLOCK_ON);
			scaler_hw_set_sw_address_reset(CSS_SCALER_1);
			scaler_int_disable(INT_PATH1_BUSI);
			css_info("frgb1 reset\n");
		}
		break;
	case CSS_SCALER_FD:
		if (atomic_read(&scl_hw->users[CSS_SCALER_FD]) == 1) {
			scaler_path_reset(PATH_FD_RESET);
			scaler_path_clk_control(PATH_FD_CLK, CLOCK_ON);
			css_info("frgb_fd reset\n");
		}
		break;
	default:
		ret = CSS_ERROR_INVALID_SCALER_NUM;
		break;
	}

	mutex_unlock(&scl_hw->hwmutex);
	return ret;
}

int scaler_deinit_device(css_scaler_select scaler)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();

	mutex_lock(&scl_hw->hwmutex);

	atomic_dec(&scl_hw->users[scaler]);

	if (atomic_read(&scl_hw->users[scaler]) == 0) {
		switch (scaler) {
		case CSS_SCALER_0:
			scaler_path_clk_control(PATH_0_CLK, CLOCK_OFF);
			break;
		case CSS_SCALER_1:
			scaler_path_clk_control(PATH_1_CLK, CLOCK_OFF);
			break;
		case CSS_SCALER_FD:
			scaler_path_clk_control(PATH_FD_CLK, CLOCK_OFF);
			break;
		default:
			break;
		}
	}

	if (atomic_read(&scl_hw->users[scaler]) < 0) {
		atomic_set(&scl_hw->users[scaler], 0);
	}

	mutex_unlock(&scl_hw->hwmutex);

	return ret;
}

void scaler_device_close(void)
{
	struct scaler_hardware *scl_hw = get_scl_hw();

	mutex_lock(&scl_hw->hwmutex);

	if (atomic_read(&scl_hw->users[0]) != 0) {
		atomic_set(&scl_hw->users[0], 0);
	}

	if (atomic_read(&scl_hw->users[1]) != 0) {
		atomic_set(&scl_hw->users[1], 0);
	}

	if (atomic_read(&scl_hw->users[2]) != 0) {
		atomic_set(&scl_hw->users[2], 0);
	}

	if (css_clock_available(CSS_CLK_FRGB)) {
		disable_irq(scl_hw->irq);
		scaler_hw_set_cache_control(0);
		scaler_path_clk_control(PATH_ALL_CLK, CLOCK_OFF);
		scaler_int_disable(INT_ALL);
		css_clock_disable(CSS_CLK_FRGB);
	}

	mutex_unlock(&scl_hw->hwmutex);
}

int scaler_configure(css_scaler_select scaler,
		struct css_scaler_config *scl_config, struct css_scaler_data *scl_data)
{
	int ret = CSS_SUCCESS;

	BUG_ON(scaler > CSS_SCALER_FD);

	if (scl_config == NULL || scl_data == NULL)
		return CSS_ERROR_INVALID_ARG;

	if (scl_config->action == CSS_SCALER_ACT_NONE)
		return ret;

	if (ret == CSS_SUCCESS)
		ret = scaler_default_configure(scaler);

	if (ret == CSS_SUCCESS)
		ret = scaler_crop_mask_configure(scaler, scl_config, scl_data);

	if (ret == CSS_SUCCESS)
		ret = scaler_effects_configure(scaler, scl_data);

	if (ret == CSS_SUCCESS)
		ret = scaler_colorformat_configure(scaler, scl_data,
											scl_config->pixel_fmt);

	if (ret == CSS_SUCCESS)
		ret = scaler_action_configure(scaler, scl_data, scl_config->action);

	return ret;
}

int fd_scaler_configure(css_scaler_select scaler,
					struct face_detect_config *config_data,
					struct css_scaler_data *fd_scaler_config)
{
	int ret = CSS_SUCCESS;
	struct css_crop_data crop_data;

	BUG_ON(scaler != CSS_SCALER_FD);

	if (config_data->action != CSS_SCALER_ACT_CAPTURE_FD)
		return ret;

	if (ret == CSS_SUCCESS)
		scaler_hw_set_vsync_delay(CSS_SCALER_FD, SCALER_FD_VSYNC_DLY);

	if (ret == CSS_SUCCESS) { /* FD scaler = no crop, only resize */
		crop_data.src.width = config_data->fd_scaler_src_width;
		crop_data.src.height = config_data->fd_scaler_src_height;

		crop_data.crop.start_x = config_data->crop.start_x;
		crop_data.crop.start_y = config_data->crop.start_y;
		crop_data.crop.width = config_data->crop.width;
		crop_data.crop.height = config_data->crop.height;

		crop_data.dst.width = config_data->width;
		crop_data.dst.height = config_data->height;

		ret = scaler_set_crop_n_masksize(scaler, crop_data, fd_scaler_config);
	}

	if (ret == CSS_SUCCESS)
		ret = fd_scaler_set_source(scaler, fd_scaler_config,
									config_data->fd_source);

	if (ret == CSS_SUCCESS)
		ret = scaler_action_configure(scaler, fd_scaler_config,
									config_data->action);

	return ret;
}

int fd_scaler_set_source(css_scaler_select scaler,
					struct css_scaler_data *fd_scaler_config,
					css_fd_source fd_source)
{
	int ret = CSS_SUCCESS;

	fd_scaler_config->fd_source = fd_source;
	ret = scaler_hw_set_fd_source(fd_scaler_config->fd_source);

	return ret;
}

int scaler_default_configure(css_scaler_select scaler)
{
	int ret = CSS_SUCCESS;
	struct css_strobe_set strobe_opt;

	strobe_opt.enable = CSS_MODULE_DISABLE;
	strobe_opt.time = SCALER_STROBE_TIME;
	strobe_opt.high = SCALER_STROBE_HIGH;

	switch (scaler) {
	case CSS_SCALER_0:
		scaler_hw_set_vsync_delay(CSS_SCALER_0, SCALER_0_VSYNC_DLY);
		break;
	case CSS_SCALER_1:
		scaler_hw_set_vsync_delay(CSS_SCALER_1, SCALER_1_VSYNC_DLY);
		break;
	case CSS_SCALER_FD:
		scaler_hw_set_vsync_delay(CSS_SCALER_FD, SCALER_FD_VSYNC_DLY);
		break;
	default:
		ret = CSS_ERROR_INVALID_SCALER_NUM;
		break;
	}

	if (ret == CSS_SUCCESS)
		ret = scaler_hw_set_strobe(&strobe_opt);

	return ret;
}

int scaler_action_configure(css_scaler_select scaler,
			struct css_scaler_data *scl_data, css_scaler_action_type action)
{
	int ret = CSS_SUCCESS;

	BUG_ON(scaler > CSS_SCALER_FD);
	/* BUG_ON(action == 0); */

	switch (action) {
	case CSS_SCALER_ACT_CAPTURE:
	case CSS_SCALER_ACT_SNAPSHOT:
		scl_data->instruction_mode = CSS_INSTRUCTION_CAPTURE;
		break;
	case CSS_SCALER_ACT_CAPTURE_3D:
		scl_data->instruction_mode = CSS_INSTRUCTION_3D_CREATOR_PATH;
		break;
	case CSS_SCALER_ACT_CAPTURE_FD:
		scl_data->instruction_mode = CSS_INSTRUCTION_CAPTURE_FD;
		break;
	/*
	case CSS_BEHAVIOR_SNAPSHOT:
		H/W can support OTF, buf not use
		scl_data->instruction_mode = CSS_INSTRUCTION_SNAPSHOT;
		break;
	*/
	default:
		scl_data->instruction_mode = CSS_INSTRUCTION_NO_OPERATION;
		break;
	}

	if (ret == CSS_SUCCESS)
		ret = scaler_hw_set_instruction_mode(scaler,
										scl_data->instruction_mode);

	return ret;
}

int scaler_set_capture_frame(css_scaler_select scaler)
{
	int ret = CSS_SUCCESS;

	BUG_ON(scaler > CSS_SCALER_FD);

	if (ret == CSS_SUCCESS) {
		switch (scaler) {
		case CSS_SCALER_0:
			scaler_hw_set_int_enable(INT_PATH0_BUSI);
			scaler_hw_set_parameter(PATH_0_PARAM_SET);
			break;
		case CSS_SCALER_1:
			scaler_hw_set_int_enable(INT_PATH1_BUSI);
			scaler_hw_set_parameter(PATH_1_PARAM_SET);
			break;
		case CSS_SCALER_FD:
			scaler_hw_set_int_enable(INT_PATH_FD);
			scaler_hw_set_parameter(PATH_FD_PARAM_SET);
			break;
		default:
			css_warn("undefined scaler!!\n");
			break;
		}
	}

	return ret;
}

int scaler_set_capture_frame_with_postview(css_scaler_select capture_scl,
					css_scaler_select postview_scl)
{
	int ret = CSS_SUCCESS;
	u32 set_int = 0;
	u32 set_param = 0;
	u32 mask_int = 0;

	switch (capture_scl) {
	case CSS_SCALER_0:
		set_int |= INT_PATH0_BUSI;
		set_param |= PATH_0_PARAM_SET;
		break;
	case CSS_SCALER_1:
		set_int |= INT_PATH1_BUSI;
		set_param |= PATH_1_PARAM_SET;
		break;
	case CSS_SCALER_FD:
	default:
		css_err("invalid scaler!!\n");
		break;
	}

	switch (postview_scl) {
	case CSS_SCALER_0:
		mask_int |= INT_PATH0_BUSI;
		set_param |= PATH_0_PARAM_SET;
		break;
	case CSS_SCALER_1:
		mask_int |= INT_PATH1_BUSI;
		set_param |= PATH_1_PARAM_SET;
		break;
	case CSS_SCALER_FD:
	default:
		css_err("invalid scaler!!\n");
		break;
	}

	scaler_hw_set_int_disable(mask_int);
	scaler_hw_set_int_enable(set_int);
	scaler_hw_set_parameter(set_param);

	return ret;
}

int scaler_set_vsync_delay(css_scaler_select scaler,
						struct css_scaler_data *scl_data, unsigned short delay)
{
	int ret = CSS_SUCCESS;

	if (delay <= 0xFFFF) {
		scl_data->vsync_delay = delay;
		ret = scaler_hw_set_vsync_delay(scaler,scl_data->vsync_delay);
	} else {
		css_err("invalid vsync delay range %d!!\n", delay);
		ret = CSS_ERROR_INVALID_RANGE;
	}

	return ret;
}

int scaler_effects_configure(css_scaler_select scaler,
					struct css_scaler_data *scl_data)
{
	int ret = CSS_SUCCESS;

	BUG_ON(scl_data == NULL);

	if (scaler >= CSS_SCALER_FD)
		ret = CSS_ERROR_INVALID_SCALER_NUM;

	if (ret == CSS_SUCCESS)
		ret = scaler_hw_set_effect(scaler, &scl_data->effect);

	return ret;
}

int scaler_colorformat_configure(css_scaler_select scaler,
					struct css_scaler_data *scl_data, css_pixel_format pix_fmt)
{
	int ret = CSS_SUCCESS;

	if (pix_fmt > CSS_COLOR_RGB565)
		return CSS_ERROR_INVALID_PIXEL_FORMAT;

	if (scaler >= CSS_SCALER_FD)
		return CSS_ERROR_INVALID_SCALER_NUM;

	scl_data->output_pixel_fmt = pix_fmt;
	if (ret == CSS_SUCCESS)
		ret = scaler_hw_set_color_format(scaler, scl_data->output_pixel_fmt);

	return ret;
}

int scaler_set_sensorstrobe(struct css_strobe_set *StrobeSet)
{
	int ret = CSS_SUCCESS;

	ret = scaler_hw_set_strobe(StrobeSet);

	return ret;
}

int scaler_get_sensorstrobe(struct css_strobe_set *StrobeSet)
{
	int ret = CSS_SUCCESS;

	ret = scaler_hw_get_strobe(StrobeSet);

	return ret;
}

int scaler_set_dithering(css_scaler_select scaler,
					struct css_dithering_set *DitherSet)
{
	int ret = CSS_SUCCESS;

	BUG_ON(DitherSet == NULL);

	if (scaler >= CSS_SCALER_FD)
		return CSS_ERROR_INVALID_SCALER_NUM;

	if (DitherSet->set == CSS_DITHERING_ENABLE)
		ret = scaler_hw_set_dither_enable(scaler, DitherSet);
	else
		ret = scaler_hw_set_dither_disable(scaler);

	return ret;
}

int scaler_get_dithering(css_scaler_select scaler,
					struct css_dithering_set *DitherSet)
{
	int ret = CSS_SUCCESS;

	if (scaler >= CSS_SCALER_FD)
		return CSS_ERROR_INVALID_SCALER_NUM;

	ret = scaler_hw_get_dither_set(scaler, DitherSet);

	return ret;
}

int fd_set_buffer(css_scaler_select scaler, unsigned int buf_addr)
{
	int ret = CSS_SUCCESS;

	if (scaler == CSS_SCALER_FD)
		ret = scaler_hw_set_fd_address(buf_addr);

	return ret;
}

int scaler_set_buffers(css_scaler_select scaler,
				struct css_scaler_config *scl_config, unsigned int buf_addr)
{
	int ret = CSS_SUCCESS;
	unsigned int y_size = 0;
	unsigned int cb_size = 0;
	unsigned int cr_size = 0;
	unsigned int y_addr = 0x0;
	unsigned int cb_addr = 0x0;
	unsigned int cr_addr = 0x0;
	struct css_image_size image_size;
	/*
	if (scaler == CSS_SCALER_FD) {
		ret = scaler_hw_set_fd_address(buf_addr);
		return ret;
	}
	*/
	image_size.width = scl_config->dst_width;
	image_size.height = scl_config->dst_height + scl_config->y_padding;

	switch (scl_config->pixel_fmt) {
	case CSS_COLOR_YUV420:
		y_addr = buf_addr;
		y_size = image_size.width * image_size.height;
		cb_size = y_size / 4;
		cr_size = cb_size;
		cb_addr = y_addr + y_size;
		cr_addr = cb_addr + cb_size;
		break;
	case CSS_COLOR_YUV422:
	case CSS_COLOR_RGB565:
		y_addr = buf_addr;
		cb_addr = 0x0;
		cr_addr = 0x0;
		break;
	default:
		css_err("not supported pixel format[%d]!\n", scl_config->pixel_fmt);
		return CSS_ERROR_INVALID_PIXEL_FORMAT;
		break;
	}

	if (scaler == CSS_SCALER_0) {
		if (scl_config->hw_buf_index == 0)
			ret = scaler_hw_set_sw_address(0, y_addr, cb_addr, cr_addr);
		else if (scl_config->hw_buf_index == 1)
			ret = scaler_hw_set_sw_address(2, y_addr, cb_addr, cr_addr);
		else if (scl_config->hw_buf_index == 2)
			ret = scaler_hw_set_sw_address(4, y_addr, cb_addr, cr_addr);
		else if (scl_config->hw_buf_index == 3)
			ret = scaler_hw_set_sw_address(6, y_addr, cb_addr, cr_addr);

		scl_config->hw_buf_index++;
		if (scl_config->hw_buf_index > 3)
			scl_config->hw_buf_index = 0;

	} else if (scaler == CSS_SCALER_1) {
		if (scl_config->hw_buf_index == 0)
			ret = scaler_hw_set_sw_address(1, y_addr, cb_addr, cr_addr);
		else if (scl_config->hw_buf_index == 1)
			ret = scaler_hw_set_sw_address(3, y_addr, cb_addr, cr_addr);
		else if (scl_config->hw_buf_index == 2)
			ret = scaler_hw_set_sw_address(5, y_addr, cb_addr, cr_addr);
		else if (scl_config->hw_buf_index == 3)
			ret = scaler_hw_set_sw_address(7, y_addr, cb_addr, cr_addr);

		scl_config->hw_buf_index++;
		if (scl_config->hw_buf_index > 3)
			scl_config->hw_buf_index = 0;
	}
	return ret;
}

static int scaler_check_crop_region(struct css_scaler_config *scl_config)
{
	int ret = CSS_SUCCESS;

	if (scl_config->dst_width == 0 || scl_config->dst_height == 0) {
		if (scl_config->crop.start_x % 2 != 0) {
			css_err("crop x must be multiple 2 [%d]\n",
					 scl_config->crop.start_x);
			return -EINVAL;
		}
		if (scl_config->crop.width % 2 != 0) {
			css_err("ave[1]: crop w must be multiple 2 [%d %d]\n",
					 scl_config->crop.width, scl_config->dst_width);
			return -EINVAL;
		}
	} else {
		if (scl_config->crop.start_x % 2 != 0) {
			css_err("crop x must be multiple 2 [%d]\n",
					 scl_config->crop.start_x);
			return -EINVAL;
		}

		if (scl_config->crop.width >> 3 >= scl_config->dst_width &&
			scl_config->crop.height >> 3 >= scl_config->dst_height) {
			 if (scl_config->crop.width % 16 != 0) {
				 css_err("ave[8]: crop w must be multiple 16 [%d %d]\n",
					scl_config->crop.width, scl_config->dst_width);
				return -EINVAL;
			 }
			 if (scl_config->crop.height % 16 != 0) {
				 css_err("ave[8]: crop h must be multiple 16 [%d %d]\n",
					scl_config->crop.height, scl_config->dst_height);
				 return -EINVAL;
			 }
		} else if (scl_config->crop.width >> 2
					>= scl_config->dst_width &&
				scl_config->crop.height >> 2
					>= scl_config->dst_height) {
			if (scl_config->crop.width % 8 != 0) {
				css_err("ave[4]: crop w must be multiple 8 [%d %d]\n",
					scl_config->crop.width, scl_config->dst_width);
				return -EINVAL;
			}
			if (scl_config->crop.height % 8 != 0) {
				css_err("ave[4]: crop h must be multiple 8 [%d %d]\n",
					scl_config->crop.height, scl_config->dst_height);
				return -EINVAL;
			}
		} else if (scl_config->crop.width >> 1
					>= scl_config->dst_width &&
				scl_config->crop.height >> 1
					>= scl_config->dst_height) {
			if (scl_config->crop.width % 4 != 0) {
				css_err("ave[2]: crop w must be multiple 4 [%d %d]\n",
					scl_config->crop.width, scl_config->dst_width);
				return -EINVAL;
			}
			if (scl_config->crop.height % 4 != 0) {
				css_err("ave[2]: crop h must be multiple 4 [%d %d]\n",
					scl_config->crop.height, scl_config->dst_height);
				return -EINVAL;
			}
		} else {
			if (scl_config->crop.width % 2 != 0) {
				css_err("ave[1]: crop w must be multiple 2 [%d %d]\n",
					 scl_config->crop.width, scl_config->dst_width);
				return -EINVAL;
			}
		}
	}
	return ret;
}

static int scaler_crop_mask_configure(css_scaler_select scaler,
					struct css_scaler_config *scl_config,
					struct css_scaler_data *scl_data)
{
	int ret = CSS_SUCCESS;
	struct css_crop_data crop_data;

	BUG_ON(scl_config == NULL);

	if (scl_config->action != CSS_SCALER_ACT_NONE) {
		crop_data.crop.start_x = scl_config->crop.start_x;
		crop_data.crop.start_y = scl_config->crop.start_y;
		crop_data.crop.width = scl_config->crop.width;
		crop_data.crop.height = scl_config->crop.height;

		crop_data.src.width = scl_config->src_width;
		crop_data.src.height = scl_config->src_height;

		crop_data.dst.width = scl_config->dst_width;
		crop_data.dst.height = scl_config->dst_height;

		/* scaler_check_crop_region(scl_config); */

		ret = scaler_set_crop_n_masksize(scaler, crop_data, scl_data);

		if (ret == CSS_SUCCESS) {
			scl_config->crop.start_x = scl_data->crop.m_crop_h_offset;
			scl_config->crop.start_y = scl_data->crop.m_crop_v_offset;
			scl_config->crop.width = scl_data->crop.m_crop_h_size;
			scl_config->crop.height = scl_data->crop.m_crop_v_size;
		}
	}

	return ret;
}

static int scaler_check_average(int in_w, int in_h, int out_w, int out_h)
{
	int average = 0;

	if (out_w <= ((in_w >> 3)) && out_h <= ((in_h >>3 ))) {
		average = 8;
	} else if (out_w <= ((in_w >> 2)) && out_h <= ((in_h >> 2))) {
		average = 4;
	} else if (out_w <= ((in_w >> 1)) && out_h <= ((in_h >> 1))) {
		average = 2;
	} else {
		average = 1;
	}

	return average;
}

int scaler_set_crop_n_masksize(css_scaler_select scaler,
					struct css_crop_data crop_data,
					struct css_scaler_data *scl_data)
{
	int ret = CSS_SUCCESS;
	/* dest_image_size: scaler's output image size */
	struct css_image_size dest_image_size;

	/* input_image_size: scaler's input image size
	 *
	 *	 (crop_data.crop.start_x,
	 *	  crop_data.crop.start_y)	 crop_data.crop.width
	 *							 ----------
	 *							|	crop   | crop_data.crop.height
	 *							|	rect   |
	 *							 ----------
	 *
	 *							 |----|----------|
	 *							 |	  | 		 |
	 *	   Bayer Scaler output >-|crop|  scaler  |--dest_image_size
	 *						   | |	  | 		 |
	 *						   | |----|----------|
	 *						   |	  |
	 *					   crop_data. input_image_size (cropped or not)
	 *					   src.width/
	 *					   height
	 */
	struct css_image_size input_image_size;
	/* to calculate scale factor */
	struct css_image_size factor_src_image_size;
	int average = 0;

	crop_data.crop.start_x = (crop_data.crop.start_x >> 1) << 1;

	/*
	css_info("crop information\n");
	css_info(" c region: (%d, %d, %d, %d)\n",
				crop_data.crop.start_x, crop_data.crop.start_y,
				crop_data.crop.width, crop_data.crop.height);
	*/

	if (crop_data.crop.width != 0 &&
		crop_data.crop.width == crop_data.src.width) {
		/* no crop 1:1 */
		input_image_size.width = crop_data.src.width;
	} else {
		input_image_size.width = crop_data.crop.width;
	}

	if (crop_data.crop.height != 0 &&
		crop_data.crop.height == crop_data.src.height) {
		/* no crop 1:1 */
		input_image_size.height = crop_data.src.height;
	} else {
		input_image_size.height = crop_data.crop.height;
	}

	/*
	css_info(" i size: (%d, %d)\n",
			input_image_size.width, input_image_size.height);
	*/

	if (crop_data.dst.width < crop_data.crop.width &&
		crop_data.dst.height > crop_data.crop.height) {
		u16 tmp_h;
		tmp_h = crop_data.dst.height;
		crop_data.dst.height = (crop_data.dst.width * crop_data.crop.height);
		crop_data.dst.height /= crop_data.crop.width;
		/*
		css_info(" d height ratio ajust: %d =>%d\n",
				tmp_h, crop_data.dst.height);
		*/
	} else if (crop_data.dst.width > crop_data.crop.width &&
		crop_data.dst.height < crop_data.crop.height) {
		u16 tmp_w;
		tmp_w = crop_data.dst.width;
		crop_data.dst.width = (crop_data.dst.height * crop_data.crop.width);
		crop_data.dst.width /= crop_data.crop.height;
		/*
		css_info(" d width ratio ajust: %d =>%d\n",
				tmp_w, crop_data.dst.width);
		*/
	} else if (crop_data.dst.width >= crop_data.crop.width &&
		crop_data.dst.height >= crop_data.crop.height) {
		u16 tmp_w, tmp_h;
		tmp_w = crop_data.dst.width;
		tmp_h = crop_data.dst.height;
		crop_data.dst.width = crop_data.crop.width;
		crop_data.dst.height = crop_data.crop.height;
		/*
		css_info(" d size ajust: %d,%d =>%d,%d\n",
				tmp_w, tmp_h, crop_data.dst.width, crop_data.dst.height);
		*/
	}

	switch (scaler_hw_get_pixel_format(scaler)) {
	case CSS_COLOR_YUV422:
	case CSS_COLOR_RGB565:
		/* destination width must be multiple of 4 */
		/* destination height must be multiple of 2 */
		crop_data.dst.width = (crop_data.dst.width >> 2) << 2;
		crop_data.dst.height = (crop_data.dst.height >> 1) << 1;
		break;
	case CSS_COLOR_YUV420:
		/* destination width must be multiple of 16 */
		/* destination height must be multiple of 2 */
		crop_data.dst.width = (crop_data.dst.width >> 4) << 4;
		crop_data.dst.height = (crop_data.dst.height >> 1) << 1;
		break;
	default:
		break;
	}

	dest_image_size.width = crop_data.dst.width;
	dest_image_size.height = crop_data.dst.height;

	/*
	css_info(" d size: (%d, %d)\n",
			dest_image_size.width, dest_image_size.height);
	*/

	factor_src_image_size = input_image_size;

	if (scaler == CSS_SCALER_FD) {
		scl_data->average_mode = CSS_AVERAGE_NO_OPERATION;
	} else {
		average = scaler_check_average(input_image_size.width,
						input_image_size.height, dest_image_size.width,
						dest_image_size.height);

		switch (average) {
		case 2:
			scl_data->average_mode = CSS_AVERAGE_ONE_HALF;
			input_image_size.width = (input_image_size.width >> 2) << 2;
			input_image_size.height = (input_image_size.height >> 2) << 2;
			factor_src_image_size.width = (input_image_size.width >> 1);
			factor_src_image_size.height = (input_image_size.height >> 1);
			break;
		case 4:
			scl_data->average_mode = CSS_AVERAGE_ONE_FOURTH;
			input_image_size.width = (input_image_size.width >> 3) << 3;
			input_image_size.height = (input_image_size.height >> 3) << 3;
			factor_src_image_size.width = (input_image_size.width >> 2);
			factor_src_image_size.height = (input_image_size.height >> 2);
			break;
		case 8:
			scl_data->average_mode = CSS_AVERAGE_ONE_EIGHTH;
			input_image_size.width = (input_image_size.width >> 4) << 4;
			input_image_size.height = (input_image_size.height >> 4) << 4;
			factor_src_image_size.width = (input_image_size.width >> 3);
			factor_src_image_size.height = (input_image_size.height >> 3);
			break;
		default:
			scl_data->average_mode = CSS_AVERAGE_NO_OPERATION;
			break;
		}
	}

	css_frgb("scaler_%d avg = %d in_h=%d in_v=%d out_h=%d out_v=%d\n",
			scaler, scl_data->average_mode, input_image_size.width,
			input_image_size.height, dest_image_size.width,
			dest_image_size.height);

	scl_data->crop.m_crop_h_offset	= crop_data.crop.start_x;
	scl_data->crop.m_crop_v_offset	= crop_data.crop.start_y;
	scl_data->crop.m_crop_h_size	= (input_image_size.width >> 1) << 1;
	scl_data->crop.m_crop_v_size	= input_image_size.height;
	scl_data->output_image_size		= dest_image_size;

	if (dest_image_size.width == 0 || dest_image_size.height == 0) {
		css_err("dst_img h_size = %d dst_img v_size = %d\n",
		dest_image_size.width, dest_image_size.height);
		return CSS_ERROR_INVALID_SIZE;
	}

	scl_data->scaling_factor.m_scl_h_factor
			= (factor_src_image_size.width	* 1024) / dest_image_size.width;
	scl_data->scaling_factor.m_scl_v_factor
			= (factor_src_image_size.height * 1024) / dest_image_size.height;
	/*
	css_info(" f size: (%d, %d)\n",
			scl_data->scaling_factor.m_scl_h_factor,
			scl_data->scaling_factor.m_scl_v_factor);
	*/

	scaler_hw_set_crop_mask(scaler, scl_data);

	return ret;
}

int scaler_bayer_store_enable(css_zsl_path s_path,
							struct css_zsl_config *zsl_config)
{
	int ret = CSS_SUCCESS;

	if (s_path > ZSL_STORE_PATH_ALL)
		return CSS_ERROR_INVALID_ARG;

	if (zsl_config->enable == 1) {
		css_warn("zsl[%d] already enabled!!\n", s_path);
		return ret;
	}
	scaler_bayer_store_reset(s_path);

	ret = scaler_bayer_store_configure(s_path, zsl_config);
	zsl_config->enable = 1;
	if (ret == CSS_SUCCESS)
		ret = scaler_bayer_store_capture(s_path, zsl_config);

	if (ret != CSS_SUCCESS)
		zsl_config->enable = 0;

	return ret;
}

int scaler_bayer_store_disable(css_zsl_path s_path,
					struct css_zsl_config *zsl_config)
{
	int ret = CSS_SUCCESS;

	if (s_path > ZSL_STORE_PATH_ALL)
		return CSS_ERROR_INVALID_ARG;

	if (zsl_config->enable)
		zsl_config->enable = 0;
	else
		ret = CSS_ERROR_ZSL_NOT_ENABLED;

	return ret;
}

int scaler_bayer_store_capture(css_zsl_path s_path,
					struct css_zsl_config *zsl_config)
{
	int ret = CSS_SUCCESS;

	if (s_path > ZSL_STORE_PATH_ALL)
		return CSS_ERROR_INVALID_ARG;

	if (zsl_config->enable == 0) {
		css_zsl("zsl disabled!! can't store!!\n");
		return ret;
	}

	switch (s_path) {
	case ZSL_STORE_PATH0:
		scaler_hw_set_int_enable(INT_PATH_RAW0_STORE);
		scaler_hw_set_parameter(PATH_RAW0_ST_PARAM_SET);
		break;
	case ZSL_STORE_PATH1:
		scaler_hw_set_int_enable(INT_PATH_RAW1_STORE);
		scaler_hw_set_parameter(PATH_RAW1_ST_PARAM_SET);
		break;
	case ZSL_STORE_PATH_ALL:
		scaler_hw_set_int_enable(INT_PATH_RAW0_STORE|INT_PATH_RAW1_STORE);
		scaler_hw_set_parameter(PATH_RAW0_ST_PARAM_SET|PATH_RAW1_ST_PARAM_SET);
		break;
	default:
		css_warn("undefined zsl store path!!\n");
		break;
	}

	return ret;
}

int scaler_bayer_load_configure(css_zsl_path r_path,
					struct css_zsl_config *zsl_config)
{
	int ret = CSS_SUCCESS;
	int szwidth = 0;
	int szheight = 0;

	if (zsl_config->bit_mode == CSS_RAWDATA_BIT_MODE_8BIT)
		szwidth = zsl_config->img_size.width << 1;
	else
		szwidth = zsl_config->img_size.width;

	szheight = zsl_config->img_size.height + BAYER_LOAD_OFFSET;

	if (r_path > ZSL_READ_PATH_ALL)
		return CSS_ERROR_INVALID_ARG;

 	/* scaler_bayer_load_reset(r_path); */
	switch (r_path) {
	case ZSL_READ_PATH0:
		ret = scaler_hw_set_raw0_load(zsl_config->bit_mode,
					zsl_config->ld_fmt,
					szwidth,
					szheight,
					zsl_config->mem.base32,
					zsl_config->blank_count);
		break;
	case ZSL_READ_PATH1:
		ret = scaler_hw_set_raw1_load(zsl_config->bit_mode,
					zsl_config->ld_fmt,
					szwidth,
					szheight,
					zsl_config->mem.base32,
					zsl_config->blank_count);
		break;
	case ZSL_READ_PATH_ALL:
		ret = scaler_hw_set_raw0_load(zsl_config->bit_mode,
					zsl_config->ld_fmt,
					szwidth,
					szheight,
					zsl_config->mem.base32st[0].base32,
					zsl_config->blank_count);
		if (ret != CSS_SUCCESS) {
			css_err("raw0_load fail %d\n", ret);
			goto gout;
		}
		ret = scaler_hw_set_raw1_load(zsl_config->bit_mode,
					zsl_config->ld_fmt,
					szwidth,
					szheight,
					zsl_config->mem.base32st[1].base32,
					zsl_config->blank_count);
		if (ret != CSS_SUCCESS) {
			css_err("raw1_load fail %d\n", ret);
			goto gout;
		}
		break;
	default:
		ret = CSS_FAILURE;
		break;
	}

gout:
	return ret;
}

int scaler_bayer_load_frame_for_stereo(int enable)
{
	int ret = CSS_SUCCESS;
	unsigned int int_set = 0;
	unsigned int param_set = 0;

	if (enable) {
		scaler_hw_set_instruction_mode(CSS_SCALER_0,
										CSS_INSTRUCTION_3D_CREATOR_PATH);
		scaler_hw_set_instruction_mode(CSS_SCALER_1,
										CSS_INSTRUCTION_3D_CREATOR_PATH);

		int_set |= INT_PATH_3D_0
				| INT_PATH_3D_1
				| INT_PATH_RAW0_LOAD
				| INT_PATH_RAW1_LOAD;
		param_set |= PATH_0_PARAM_SET
					| PATH_1_PARAM_SET
					| PATH_RAW0_LD_PARAM_SET
					| PATH_RAW1_LD_PARAM_SET;

		scaler_hw_set_int_enable(int_set);
		scaler_hw_set_parameter(param_set);
	} else {
		int_set |= INT_PATH_3D_0
				| INT_PATH_3D_1
				| INT_PATH_RAW0_LOAD
				| INT_PATH_RAW1_LOAD;
		scaler_hw_set_int_clr(int_set);
		scaler_hw_set_int_disable(int_set);
	}

	return ret;
}

int scaler_bayer_load_frame_for_1ch(int enable, css_zsl_path r_path,
					css_scaler_select capture_scl)
{
	int ret = CSS_SUCCESS;
	unsigned int int_set = 0;
	unsigned int param_set = 0;

	if (r_path > ZSL_READ_PATH_ALL)
		return CSS_ERROR_INVALID_ARG;

	switch (r_path) {
	case ZSL_READ_PATH0:
		int_set |= INT_PATH_RAW0_LOAD;
		param_set |= PATH_RAW0_LD_PARAM_SET;
		break;
	case ZSL_READ_PATH1:
		int_set |= INT_PATH_RAW1_LOAD;
		param_set |= PATH_RAW1_LD_PARAM_SET;
		break;
	default:
		css_warn("undefined read path!!\n");
		return CSS_FAILURE;
	}

	switch (capture_scl) {
	case CSS_SCALER_0:
		/* int_set |= INT_PATH0_BUSI; */
		param_set |= PATH_0_PARAM_SET;
		break;
	case CSS_SCALER_1:
		/* int_set |= INT_PATH1_BUSI; */
		param_set |= PATH_1_PARAM_SET;
		break;
	case CSS_SCALER_NONE:
		break;
	default:
		css_warn("undefined scaler!!\n");
		return CSS_FAILURE;
	}

	if (enable) {
		if (capture_scl != CSS_SCALER_NONE)
			scaler_hw_set_instruction_mode(capture_scl,
								CSS_INSTRUCTION_CAPTURE);

		scaler_hw_set_int_enable(int_set);
		scaler_hw_set_parameter(param_set);
	} else {
		scaler_hw_set_int_clr(int_set);
		scaler_hw_set_int_disable(int_set);
	}

	return ret;
}

int scaler_bayer_load_frame_for_2ch(int enable, css_zsl_path r_path,
			css_scaler_select capture_scl,
			css_scaler_select postview_scl)
{
	int ret = CSS_SUCCESS;
	unsigned int int_set = 0;
	unsigned int param_set = 0;

	if (r_path > ZSL_READ_PATH_ALL)
		return CSS_ERROR_INVALID_ARG;

	switch (r_path) {
	case ZSL_READ_PATH0:
		int_set |= INT_PATH_RAW0_LOAD;
		param_set |= PATH_RAW0_LD_PARAM_SET;
		break;
	case ZSL_READ_PATH1:
		int_set |= INT_PATH_RAW1_LOAD;
		param_set |= PATH_RAW1_LD_PARAM_SET;
		break;
	default:
		css_warn("undefined read path!!\n");
		return CSS_FAILURE;
	}

	switch (capture_scl) {
	case CSS_SCALER_0:
		/* int_set |= INT_PATH0_BUSI; */
		param_set |= PATH_0_PARAM_SET;
		break;
	case CSS_SCALER_1:
		/* int_set |= INT_PATH1_BUSI; */
		param_set |= PATH_1_PARAM_SET;
		break;
	default:
		css_warn("undefined scaler!!\n");
		return CSS_FAILURE;
	}

	switch (postview_scl) {
	case CSS_SCALER_0:
		/* int_set |= INT_PATH0_BUSI; */
		param_set |= PATH_0_PARAM_SET;
		break;
	case CSS_SCALER_1:
		/* int_set |= INT_PATH1_BUSI; */
		param_set |= PATH_1_PARAM_SET;
		break;
	default:
		css_warn("undefined scaler!!\n");
		return CSS_FAILURE;
	}

	if (enable) {
		scaler_hw_set_instruction_mode(capture_scl, CSS_INSTRUCTION_CAPTURE);
		scaler_hw_set_instruction_mode(postview_scl, CSS_INSTRUCTION_CAPTURE);
		scaler_hw_set_int_enable(int_set);
		scaler_hw_set_parameter(param_set);
	} else {
		scaler_hw_set_int_clr(int_set);
		scaler_hw_set_int_disable(int_set);
	}

	return ret;
}

int scaler_set_capture_stereo_frame(void)
{
	int ret = CSS_SUCCESS;

	scaler_hw_set_int_enable(INT_PATH_3D_0 | INT_PATH_3D_1);
	scaler_hw_set_parameter(PATH_0_PARAM_SET | PATH_1_PARAM_SET);

	return ret;
}

int scaler_bayer_store_reset(css_zsl_path s_path)
{
	int ret = CSS_SUCCESS;

	if (s_path > ZSL_STORE_PATH_ALL)
		return CSS_ERROR_INVALID_ARG;

	switch (s_path) {
	case ZSL_STORE_PATH0:
		scaler_hw_path_reset(PATH_RAW0_ST_RESET);
		scaler_hw_clr_raw0_store();
		break;
	case ZSL_STORE_PATH1:
		scaler_hw_path_reset(PATH_RAW1_ST_RESET);
		scaler_hw_clr_raw1_store();
		break;
	case ZSL_STORE_PATH_ALL:
		scaler_hw_path_reset(PATH_RAW0_ST_RESET | PATH_RAW1_ST_RESET);
		scaler_hw_clr_raw0_store();
		scaler_hw_clr_raw1_store();
		break;
	default:
		break;
	}

	return ret;
}

static int scaler_bayer_load_reset(css_zsl_path r_path)
{
	int ret = CSS_SUCCESS;

	if (r_path > ZSL_READ_PATH_ALL)
		return CSS_ERROR_INVALID_ARG;

	switch (r_path) {
	case ZSL_READ_PATH0:
		scaler_hw_path_reset(PATH_RAW0_LD_RESET);
		scaler_hw_clr_raw0_load();
		break;
	case ZSL_READ_PATH1:
		scaler_hw_path_reset(PATH_RAW1_LD_RESET);
		scaler_hw_clr_raw1_load();
		break;
	case ZSL_READ_PATH_ALL:
		scaler_hw_path_reset(PATH_RAW0_LD_RESET | PATH_RAW1_LD_RESET);
		scaler_hw_clr_raw0_load();
		scaler_hw_clr_raw1_load();
		break;
	default:
		break;
	}

	return ret;
}

int scaler_bayer_store_configure(css_zsl_path s_path,
					struct css_zsl_config *zsl_config)
{
	int ret = CSS_SUCCESS;
	int szwidth = 0;
	int szheight = 0;

	if (zsl_config->bit_mode == CSS_RAWDATA_BIT_MODE_8BIT)
		szwidth = zsl_config->img_size.width << 1;
	else
		szwidth = zsl_config->img_size.width;

	szheight = zsl_config->img_size.height - BAYER_STORE_OFFSET;

	if (s_path > ZSL_STORE_PATH_ALL)
		return CSS_ERROR_INVALID_ARG;

	switch (s_path) {
	case ZSL_STORE_PATH0:
		ret = scaler_hw_set_raw0_store(zsl_config->bit_mode,
					szwidth,
					szheight,
					zsl_config->mem.base32);
		break;
	case ZSL_STORE_PATH1:
		ret = scaler_hw_set_raw1_store(zsl_config->bit_mode,
					szwidth,
					szheight,
					zsl_config->mem.base32);
		break;
	case ZSL_STORE_PATH_ALL:
		ret = scaler_hw_set_raw0_store(zsl_config->bit_mode,
					szwidth,
					szheight,
					zsl_config->mem.base32st[0].base32);
		if (ret != CSS_SUCCESS) {
			css_err("raw0_store set fail %d\n", ret);
			goto gout;
		}
		ret = scaler_hw_set_raw1_store(zsl_config->bit_mode,
					szwidth,
					szheight,
					zsl_config->mem.base32st[1].base32);
		if (ret != CSS_SUCCESS) {
			css_err("raw1_store set fail %d\n", ret);
			goto gout;
		}
		css_zsl("zsl_st_buf : 0x%x", zsl_config->mem.base32st[0].base32);
		break;
	default:
		ret = CSS_FAILURE;
		break;
	}
gout:
	return ret;
}

static int scaler_hw_path_reset(unsigned int path_reset)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_RESET_CTRL scl_rst_ctrl;
	unsigned long flags;

	spin_lock_irqsave(&scl_hw->slock, flags);
	scl_rst_ctrl.as32bits = path_reset;
	writel(scl_rst_ctrl.as32bits, scl_hw->iobase + RESET);

	scl_rst_ctrl.as32bits = 0x00;
	writel(scl_rst_ctrl.as32bits, scl_hw->iobase + RESET);
	spin_unlock_irqrestore(&scl_hw->slock, flags);

	css_frgb("scaler path_reset %x!!\n", path_reset);

	return ret;
}

static int scaler_hw_path_clk_enable(unsigned int clk_src)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_CLK_CTRL	scl_clk_ctrl;
	unsigned long flags;

	clk_src = clk_src & 0xFFF;

	spin_lock_irqsave(&scl_hw->slock, flags);
	scl_clk_ctrl.as32bits =	readl(scl_hw->iobase + CLK_CTRL);

	scaler_check_and_set_clk(scl_clk_ctrl, clk_src,
								PATH_0_CLK, 	  path_0_clk);
	scaler_check_and_set_clk(scl_clk_ctrl, clk_src,
								PATH_1_CLK, 	  path_1_clk);
	scaler_check_and_set_clk(scl_clk_ctrl, clk_src,
								PATH_FD_CLK,	  path_fd_clk);
	scaler_check_and_set_clk(scl_clk_ctrl, clk_src,
								PATH_RAW0_ST_CLK, path_raw0_st_clk);
	scaler_check_and_set_clk(scl_clk_ctrl, clk_src,
								PATH_RAW1_ST_CLK, path_raw1_st_clk);
	scaler_check_and_set_clk(scl_clk_ctrl, clk_src,
								PATH_RAW0_LD_CLK, path_raw0_ld_clk);
	scaler_check_and_set_clk(scl_clk_ctrl, clk_src,
								PATH_RAW1_LD_CLK, path_raw1_ld_clk);
	scaler_check_and_set_clk(scl_clk_ctrl, clk_src,
								PATH_3D_CLK,	  path_3d_clk);
	scaler_check_and_set_clk(scl_clk_ctrl, clk_src,
								RAW0_ST_CTRL_CLK, raw0_st_ctrl_clk);
	scaler_check_and_set_clk(scl_clk_ctrl, clk_src,
								RAW1_ST_CTRL_CLK, raw1_st_ctrl_clk);
	scaler_check_and_set_clk(scl_clk_ctrl, clk_src,
								JPEG_ST_CTRL_CLK, jpeg_st_ctrl_clk);
	scaler_check_and_set_clk(scl_clk_ctrl, clk_src,
								JPEG_EN_CTRL_CLK, jpeg_enc_ctrl_clk);

	writel(scl_clk_ctrl.as32bits, scl_hw->iobase + CLK_CTRL);
	spin_unlock_irqrestore(&scl_hw->slock, flags);

	return ret;
}

static int scaler_hw_path_clk_disable(unsigned int clk_src)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_CLK_CTRL	scl_clk_ctrl;
	unsigned long flags;

	clk_src = clk_src & 0xFFF;

	spin_lock_irqsave(&scl_hw->slock, flags);
	scl_clk_ctrl.as32bits = readl(scl_hw->iobase + CLK_CTRL);

	scaler_check_and_clr_clk(scl_clk_ctrl, clk_src,
								PATH_0_CLK, 	  path_0_clk);
	scaler_check_and_clr_clk(scl_clk_ctrl, clk_src,
								PATH_1_CLK, 	  path_1_clk);
	scaler_check_and_clr_clk(scl_clk_ctrl, clk_src,
								PATH_FD_CLK,	  path_fd_clk);
	scaler_check_and_clr_clk(scl_clk_ctrl, clk_src,
								PATH_RAW0_ST_CLK, path_raw0_st_clk);
	scaler_check_and_clr_clk(scl_clk_ctrl, clk_src,
								PATH_RAW1_ST_CLK, path_raw1_st_clk);
	scaler_check_and_clr_clk(scl_clk_ctrl, clk_src,
								PATH_RAW0_LD_CLK, path_raw0_ld_clk);
	scaler_check_and_clr_clk(scl_clk_ctrl, clk_src,
								PATH_RAW1_LD_CLK, path_raw1_ld_clk);
	scaler_check_and_clr_clk(scl_clk_ctrl, clk_src,
								PATH_3D_CLK,	  path_3d_clk);
	scaler_check_and_clr_clk(scl_clk_ctrl, clk_src,
								RAW0_ST_CTRL_CLK, raw0_st_ctrl_clk);
	scaler_check_and_clr_clk(scl_clk_ctrl, clk_src,
								RAW1_ST_CTRL_CLK, raw1_st_ctrl_clk);
	scaler_check_and_clr_clk(scl_clk_ctrl, clk_src,
								JPEG_ST_CTRL_CLK, jpeg_st_ctrl_clk);
	scaler_check_and_clr_clk(scl_clk_ctrl, clk_src,
								JPEG_EN_CTRL_CLK, jpeg_enc_ctrl_clk);

	writel(scl_clk_ctrl.as32bits, scl_hw->iobase + CLK_CTRL);
	spin_unlock_irqrestore(&scl_hw->slock, flags);

	return ret;
}

static int scaler_hw_set_cache_control(unsigned int on)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();

	if (on)
		writel(0x03333333, scl_hw->iobase + AXI_CACHE_CTRL);
	else
		writel(0x00000000, scl_hw->iobase + AXI_CACHE_CTRL);

	return ret;
}

static int scaler_hw_set_int_enable(unsigned int int_flag)
{
	int ret = CSS_SUCCESS;
	unsigned int mask_val = int_flag & 0xFFFF;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_INT_MASK scl_int_mask;
	unsigned long flags;

	spin_lock_irqsave(&scl_hw->slock, flags);
	scl_int_mask.as32bits = readl(scl_hw->iobase + INT_MASK);
	scl_int_mask.as32bits &= ~mask_val;
	writel(scl_int_mask.as32bits, scl_hw->iobase + INT_MASK);
	spin_unlock_irqrestore(&scl_hw->slock, flags);

	return ret;
}

static int scaler_hw_set_int_disable(unsigned int int_flag)
{
	int ret = CSS_SUCCESS;
	unsigned int mask_val = int_flag & 0xFFFF;
	CSS_SCALER_INT_MASK scl_int_mask;
	struct scaler_hardware *scl_hw = get_scl_hw();
	unsigned long flags;

	spin_lock_irqsave(&scl_hw->slock, flags);
	scl_int_mask.as32bits = readl(scl_hw->iobase + INT_MASK);
	scl_int_mask.as32bits |= mask_val;
	writel(scl_int_mask.as32bits, scl_hw->iobase + INT_MASK);
	spin_unlock_irqrestore(&scl_hw->slock, flags);

	return ret;
}

static int scaler_hw_set_int_clr(unsigned int int_flag)
{
	int ret = CSS_SUCCESS;
	unsigned int clr_val = int_flag & 0xFFFF;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_INT_CLR scl_int_clr;
	unsigned long flags;

	spin_lock_irqsave(&scl_hw->slock, flags);
	scl_int_clr.as32bits = readl(scl_hw->iobase + INT_CLR);
	scl_int_clr.as32bits |= clr_val;
	writel(scl_int_clr.as32bits, scl_hw->iobase + INT_CLR);
	spin_unlock_irqrestore(&scl_hw->slock, flags);

	return ret;
}

static int scaler_hw_set_sw_address_reset(css_scaler_select scaler)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH01_DRAM_ADDR_SET scl_path01_dram_addr_set;
	unsigned long flags;

	spin_lock_irqsave(&scl_hw->slock, flags);
	scl_path01_dram_addr_set.as32bits =
					readl(scl_hw->iobase + PATH01_DRAM_ADDR_SET);

	if (scaler == CSS_SCALER_0) {
		scl_path01_dram_addr_set.asbits.path0_sw_addr_reset = 1;
		writel(scl_path01_dram_addr_set.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_SET);

		scl_path01_dram_addr_set.asbits.path0_sw_addr_reset = 0;
		writel(scl_path01_dram_addr_set.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_SET);

	} else if (scaler == CSS_SCALER_1) {
		scl_path01_dram_addr_set.asbits.path1_sw_addr_reset = 1;
		writel(scl_path01_dram_addr_set.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_SET);

		scl_path01_dram_addr_set.asbits.path1_sw_addr_reset = 0;
		writel(scl_path01_dram_addr_set.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_SET);
	} else {
		css_err("not supported scaler number!!\n");
		ret = CSS_ERROR_INVALID_SCALER_NUM;
	}
	spin_unlock_irqrestore(&scl_hw->slock, flags);

	return ret;
}

static int scaler_hw_set_fd_address(int addr)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();

	writel((unsigned int)addr, scl_hw->iobase + FD_DRAM_ADDR);

	return ret;
}

static int scaler_hw_set_fd_source(int source)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_FD_VSYNC_DLY scl_fd_vsync_dly;

	scl_fd_vsync_dly.as32bits =	readl(scl_hw->iobase + FD_VSYNC_DLY);
	scl_fd_vsync_dly.asbits.fd_source = source;
	writel(scl_fd_vsync_dly.as32bits, scl_hw->iobase + FD_VSYNC_DLY);

	return ret;
}

static int scaler_hw_set_sw_address(unsigned int index, unsigned int y_addr,
					unsigned int cb_addr, unsigned int cr_addr)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH01_DRAM_ADDR_SET scl_path01_dram_addr_set;
	unsigned long flags;

	switch (index) {
	case 0:
	{
		CSS_SCALER_PATH01_DRAM_ADDR_0_Y scl_path01_dram_addr_0_y;
		CSS_SCALER_PATH01_DRAM_ADDR_0_U scl_path01_dram_addr_0_u;
		CSS_SCALER_PATH01_DRAM_ADDR_0_V scl_path01_dram_addr_0_v;

		scl_path01_dram_addr_0_y.as32bits = y_addr;
		scl_path01_dram_addr_0_u.as32bits = cb_addr;
		scl_path01_dram_addr_0_v.as32bits = cr_addr;
		scl_path01_dram_addr_set.as32bits = 0x00000001;

		writel(scl_path01_dram_addr_0_y.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_0_Y);
		writel(scl_path01_dram_addr_0_u.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_0_U);
		writel(scl_path01_dram_addr_0_v.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_0_V);
		break;
	}
	case 1:
	{
		CSS_SCALER_PATH01_DRAM_ADDR_1_Y scl_path01_dram_addr_1_y;
		CSS_SCALER_PATH01_DRAM_ADDR_1_U scl_path01_dram_addr_1_u;
		CSS_SCALER_PATH01_DRAM_ADDR_1_V scl_path01_dram_addr_1_v;

		scl_path01_dram_addr_1_y.as32bits = y_addr;
		scl_path01_dram_addr_1_u.as32bits = cb_addr;
		scl_path01_dram_addr_1_v.as32bits = cr_addr;
		scl_path01_dram_addr_set.as32bits = 0x00020000;

		writel(scl_path01_dram_addr_1_y.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_1_Y);
		writel(scl_path01_dram_addr_1_u.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_1_U);
		writel(scl_path01_dram_addr_1_v.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_1_V);
		break;
	}
	case 2:
	{
		CSS_SCALER_PATH01_DRAM_ADDR_2_Y scl_path01_dram_addr_2_y;
		CSS_SCALER_PATH01_DRAM_ADDR_2_U scl_path01_dram_addr_2_u;
		CSS_SCALER_PATH01_DRAM_ADDR_2_V scl_path01_dram_addr_2_v;

		scl_path01_dram_addr_2_y.as32bits = y_addr;
		scl_path01_dram_addr_2_u.as32bits = cb_addr;
		scl_path01_dram_addr_2_v.as32bits = cr_addr;
		scl_path01_dram_addr_set.as32bits = 0x00000004;

		writel(scl_path01_dram_addr_2_y.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_2_Y);
		writel(scl_path01_dram_addr_2_u.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_2_U);
		writel(scl_path01_dram_addr_2_v.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_2_V);
		break;
	}
	case 3:
	{

		CSS_SCALER_PATH01_DRAM_ADDR_3_Y scl_path01_dram_addr_3_y;
		CSS_SCALER_PATH01_DRAM_ADDR_3_U scl_path01_dram_addr_3_u;
		CSS_SCALER_PATH01_DRAM_ADDR_3_V scl_path01_dram_addr_3_v;

		scl_path01_dram_addr_3_y.as32bits = y_addr;
		scl_path01_dram_addr_3_u.as32bits = cb_addr;
		scl_path01_dram_addr_3_v.as32bits = cr_addr;
		scl_path01_dram_addr_set.as32bits = 0x00080000;

		writel(scl_path01_dram_addr_3_y.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_3_Y);
		writel(scl_path01_dram_addr_3_u.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_3_U);
		writel(scl_path01_dram_addr_3_v.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_3_V);
		break;
	}
	case 4:
	{
		CSS_SCALER_PATH01_DRAM_ADDR_4_Y scl_path01_dram_addr_4_y;
		CSS_SCALER_PATH01_DRAM_ADDR_4_U scl_path01_dram_addr_4_u;
		CSS_SCALER_PATH01_DRAM_ADDR_4_V scl_path01_dram_addr_4_v;

		scl_path01_dram_addr_4_y.as32bits = y_addr;
		scl_path01_dram_addr_4_u.as32bits = cb_addr;
		scl_path01_dram_addr_4_v.as32bits = cr_addr;
		scl_path01_dram_addr_set.as32bits = 0x00000010;

		writel(scl_path01_dram_addr_4_y.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_4_Y);
		writel(scl_path01_dram_addr_4_u.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_4_U);
		writel(scl_path01_dram_addr_4_v.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_4_V);
		break;
	}
	case 5:
	{
		CSS_SCALER_PATH01_DRAM_ADDR_5_Y scl_path01_dram_addr_5_y;
		CSS_SCALER_PATH01_DRAM_ADDR_5_U scl_path01_dram_addr_5_u;
		CSS_SCALER_PATH01_DRAM_ADDR_5_V scl_path01_dram_addr_5_v;

		scl_path01_dram_addr_5_y.as32bits = y_addr;
		scl_path01_dram_addr_5_u.as32bits = cb_addr;
		scl_path01_dram_addr_5_v.as32bits = cr_addr;
		scl_path01_dram_addr_set.as32bits = 0x00200000;

		writel(scl_path01_dram_addr_5_y.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_5_Y);
		writel(scl_path01_dram_addr_5_u.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_5_U);
		writel(scl_path01_dram_addr_5_v.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_5_V);
		break;
	}
	case 6:
	{
		CSS_SCALER_PATH01_DRAM_ADDR_6_Y scl_path01_dram_addr_6_y;
		CSS_SCALER_PATH01_DRAM_ADDR_6_U scl_path01_dram_addr_6_u;
		CSS_SCALER_PATH01_DRAM_ADDR_6_V scl_path01_dram_addr_6_v;

		scl_path01_dram_addr_6_y.as32bits = y_addr;
		scl_path01_dram_addr_6_u.as32bits = cb_addr;
		scl_path01_dram_addr_6_v.as32bits = cr_addr;
		scl_path01_dram_addr_set.as32bits = 0x00000040;

		writel(scl_path01_dram_addr_6_y.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_6_Y);
		writel(scl_path01_dram_addr_6_u.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_6_U);
		writel(scl_path01_dram_addr_6_v.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_6_V);
		break;
	}
	case 7:
	{
		CSS_SCALER_PATH01_DRAM_ADDR_7_Y scl_path01_dram_addr_7_y;
		CSS_SCALER_PATH01_DRAM_ADDR_7_U scl_path01_dram_addr_7_u;
		CSS_SCALER_PATH01_DRAM_ADDR_7_V scl_path01_dram_addr_7_v;

		scl_path01_dram_addr_7_y.as32bits = y_addr;
		scl_path01_dram_addr_7_u.as32bits = cb_addr;
		scl_path01_dram_addr_7_v.as32bits = cr_addr;
		scl_path01_dram_addr_set.as32bits = 0x00800000;

		writel(scl_path01_dram_addr_7_y.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_7_Y);
		writel(scl_path01_dram_addr_7_u.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_7_U);
		writel(scl_path01_dram_addr_7_v.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_7_V);
		break;
	}
	default:
		css_err("not supported buf index!!\n");
		ret = CSS_ERROR_BUFFER_INDEX;
		break;
	}

	if (ret == CSS_SUCCESS) {
		spin_lock_irqsave(&scl_hw->slock, flags);
		writel(scl_path01_dram_addr_set.as32bits,
					scl_hw->iobase + PATH01_DRAM_ADDR_SET);

		writel(0x0, scl_hw->iobase + PATH01_DRAM_ADDR_SET);
		spin_unlock_irqrestore(&scl_hw->slock, flags);
	}

	return ret;
}

static int scaler_hw_set_instruction_mode(css_scaler_select scaler,
							css_scaler_instruction_mode inst_mode)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_SCALER_SET scl_path0_scl_set;
	CSS_SCALER_PATH1_SCALER_SET scl_path1_scl_set;
	CSS_SCALER_FD_VSYNC_DLY scl_fd_vsync_dly;

	switch (scaler) {
	case CSS_SCALER_0:
		scl_path0_scl_set.as32bits = readl(scl_hw->iobase + PATH0_SCL_SET);
		scl_path0_scl_set.asbits.scl_0_inst = inst_mode;
		writel(scl_path0_scl_set.as32bits, scl_hw->iobase + PATH0_SCL_SET);
		break;
	case CSS_SCALER_1:
		scl_path1_scl_set.as32bits = readl(scl_hw->iobase + PATH1_SCL_SET);
		scl_path1_scl_set.asbits.scl_1_inst = inst_mode;
		writel(scl_path1_scl_set.as32bits, scl_hw->iobase + PATH1_SCL_SET);
		break;
	case CSS_SCALER_FD:
		scl_fd_vsync_dly.as32bits =	readl(scl_hw->iobase + FD_VSYNC_DLY);
		scl_fd_vsync_dly.asbits.inst_fd = inst_mode;
		writel(scl_fd_vsync_dly.as32bits, scl_hw->iobase + FD_VSYNC_DLY);
		break;
	default:
		css_err("not supported scaler num!!\n");
		ret = CSS_ERROR_INVALID_SCALER_NUM;
		break;
	}

	return ret;
}

static int scaler_hw_set_parameter(CSS_SCALER_PATH_PARAM_SET param)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PARA_SET scl_para_set;

	scl_para_set.as32bits = param;
	css_low("setprm = 0x%x\n", param);
	writel(scl_para_set.as32bits, scl_hw->iobase + PARA_SET);

	return ret;
}

static int scaler_hw_set_crop_mask(css_scaler_select scaler,
					struct css_scaler_data *scl_data)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	void __iomem *regbase = NULL;

	regbase = scl_hw->iobase;

	if (scaler == CSS_SCALER_0) {
		CSS_SCALER_PATH0_SCALER_SET scl_path0_scl_set;
		CSS_SCALER_PATH0_CROP_OFFSET scl_path0_crop_offset;
		CSS_SCALER_PATH0_CROP_VALID scl_path0_crop_valid;
		CSS_SCALER_PATH0_SCALER_SIZE scl_path0_scl_size;
		CSS_SCALER_PATH0_SCALER_FACTOR scl_path0_scl_factor;

		scl_path0_scl_set.as32bits = readl(regbase + PATH0_SCL_SET);
		scl_path0_crop_offset.as32bits = 0;
		scl_path0_crop_offset.asbits.path_0_offset_v = \
								scl_data->crop.m_crop_v_offset;
		scl_path0_crop_offset.asbits.path_0_offset_h = \
								scl_data->crop.m_crop_h_offset;
		writel(scl_path0_crop_offset.as32bits, regbase + PATH0_CROP_OFFSET);

		scl_path0_crop_valid.as32bits = 0;
		scl_path0_crop_valid.asbits.path_0_valid_v = \
								scl_data->crop.m_crop_v_size;
		scl_path0_crop_valid.asbits.path_0_valid_h = \
								scl_data->crop.m_crop_h_size;
		writel(scl_path0_crop_valid.as32bits, regbase + PATH0_CROP_VALID);

		scl_path0_scl_set.asbits.scl_0_av_mode = scl_data->average_mode;
		writel(scl_path0_scl_set.as32bits, regbase + PATH0_SCL_SET);

		scl_path0_scl_size.as32bits = readl(regbase + PATH0_SCL_SIZE);
		scl_path0_scl_size.asbits.scl_0_dst_v
							= scl_data->output_image_size.height;
		scl_path0_scl_size.asbits.scl_0_dst_h
							= scl_data->output_image_size.width;
		writel(scl_path0_scl_size.as32bits, regbase + PATH0_SCL_SIZE);

		scl_path0_scl_factor.as32bits =	readl(regbase + PATH0_SCL_FACTOR);
		scl_path0_scl_factor.asbits.scl_0_factor_v = \
								scl_data->scaling_factor.m_scl_v_factor;
		scl_path0_scl_factor.asbits.scl_0_factor_h = \
								scl_data->scaling_factor.m_scl_h_factor;
		writel(scl_path0_scl_factor.as32bits, regbase + PATH0_SCL_FACTOR);

	} else if (scaler == CSS_SCALER_1) {
		CSS_SCALER_PATH1_SCALER_SET scl_path1_scl_set;
		CSS_SCALER_PATH1_CROP_OFFSET scl_path1_crop_offset;
		CSS_SCALER_PATH1_CROP_VALID scl_path1_crop_valid;
		CSS_SCALER_PATH1_SCALER_SIZE scl_path1_scl_size;
		CSS_SCALER_PATH1_SCALER_FACTOR scl_path1_scl_factor;

		scl_path1_scl_set.as32bits = readl(regbase + PATH1_SCL_SET);
		scl_path1_crop_offset.as32bits = 0;
		scl_path1_crop_offset.asbits.path_1_offset_v = \
								scl_data->crop.m_crop_v_offset;
		scl_path1_crop_offset.asbits.path_1_offset_h = \
								scl_data->crop.m_crop_h_offset;
		writel(scl_path1_crop_offset.as32bits, regbase + PATH1_CROP_OFFSET);

		scl_path1_crop_valid.as32bits = 0;
		scl_path1_crop_valid.asbits.path_1_valid_v = \
								scl_data->crop.m_crop_v_size;
		scl_path1_crop_valid.asbits.path_1_valid_h = \
								scl_data->crop.m_crop_h_size;
		writel(scl_path1_crop_valid.as32bits, regbase + PATH1_CROP_VALID);

		scl_path1_scl_set.asbits.scl_1_av_mode = scl_data->average_mode;
		writel(scl_path1_scl_set.as32bits, regbase + PATH1_SCL_SET);

		scl_path1_scl_size.as32bits = readl(regbase + PATH1_SCL_SIZE);
		scl_path1_scl_size.asbits.scl_1_dst_v = \
								scl_data->output_image_size.height;
		scl_path1_scl_size.asbits.scl_1_dst_h = \
								scl_data->output_image_size.width;
		writel(scl_path1_scl_size.as32bits, regbase + PATH1_SCL_SIZE);

		scl_path1_scl_factor.as32bits =	readl(regbase + PATH1_SCL_FACTOR);
		scl_path1_scl_factor.asbits.scl_1_factor_v = \
								scl_data->scaling_factor.m_scl_v_factor;
		scl_path1_scl_factor.asbits.scl_1_factor_h = \
								scl_data->scaling_factor.m_scl_h_factor;
		writel(scl_path1_scl_factor.as32bits, regbase + PATH1_SCL_FACTOR);

	} else if (scaler == CSS_SCALER_FD) {
		CSS_FD_SCALER_SIZE fd_crop_offset;
		CSS_FD_SCALER_SIZE fd_crop_valid;
		CSS_FD_SCALER_SIZE fd_dst_size;
		CSS_FD_SCALER_SIZE fd_factor;

		fd_crop_offset.as32bits = 0;
		fd_crop_offset.asbits.vertical = scl_data->crop.m_crop_v_offset;
		fd_crop_offset.asbits.horizontal = scl_data->crop.m_crop_h_offset;
		writel(fd_crop_offset.as32bits,	regbase + FD_CROP_OFFSET);

		fd_crop_valid.as32bits = 0;
		fd_crop_valid.asbits.vertical = scl_data->crop.m_crop_v_size;
		fd_crop_valid.asbits.horizontal = scl_data->crop.m_crop_h_size;
		writel(fd_crop_valid.as32bits, regbase + FD_CROP_VALID);

		fd_dst_size.as32bits = readl(regbase + FD_SCALER_SIZE);
		fd_dst_size.asbits.vertical = scl_data->output_image_size.height;
		fd_dst_size.asbits.horizontal = scl_data->output_image_size.width;
		writel(fd_dst_size.as32bits, regbase + FD_SCALER_SIZE);

		fd_factor.as32bits = readl(regbase + FD_SCALER_FACTOR);
		fd_factor.asbits.vertical = scl_data->scaling_factor.m_scl_v_factor;
		fd_factor.asbits.horizontal = scl_data->scaling_factor.m_scl_h_factor;
		writel(fd_factor.as32bits, regbase + FD_SCALER_FACTOR);
	}

	return ret;
}

static int scaler_hw_set_effect(css_scaler_select scaler,
					struct css_effect_set *effects)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_EFFECT scl_path0_effect;
	CSS_SCALER_PATH1_EFFECT scl_path1_effect;

	if (scaler >= CSS_SCALER_FD)
		return CSS_ERROR_INVALID_SCALER_NUM;

	if (effects->command > CSS_EFFECT_SKETCH ||
		effects->strength > CSS_EFFECT_STRENGTH_EIGHT) {
		css_err("not supported effect pram : cmd %d strength %d!!\n",
			effects->command, effects->strength);
		return CSS_ERROR_INVALID_ARG;
	}

	if (scaler == CSS_SCALER_0) {
		scl_path0_effect.as32bits =	readl(scl_hw->iobase + PATH0_EFFECT);
		scl_path0_effect.asbits.path_0_sepia_v	 = effects->cr_value;
		scl_path0_effect.asbits.path_0_sepia_u	 = effects->cb_value;
		scl_path0_effect.asbits.path_0_efft_inst = effects->command;
		scl_path0_effect.asbits.path_0_efft_stng = effects->strength;
		writel(scl_path0_effect.as32bits,scl_hw->iobase + PATH0_EFFECT);
	} else if (scaler == CSS_SCALER_1) {
		scl_path1_effect.as32bits =	readl(scl_hw->iobase + PATH1_EFFECT);
		scl_path1_effect.asbits.path_1_sepia_v	 = effects->cr_value;
		scl_path1_effect.asbits.path_1_sepia_u	 = effects->cb_value;
		scl_path1_effect.asbits.path_1_efft_inst = effects->command;
		scl_path1_effect.asbits.path_1_efft_stng = effects->strength;
		writel(scl_path1_effect.as32bits, scl_hw->iobase + PATH1_EFFECT);
	} else {
		css_err("unsupported scaler num %d!!\n", scaler);
		ret = CSS_ERROR_INVALID_SCALER_NUM;
	}

	return ret;
}

static int scaler_hw_set_color_format(css_scaler_select scaler,
					css_pixel_format pix_fmt)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_SCALER_SET scl_path0_scl_set;
	CSS_SCALER_PATH1_SCALER_SET scl_path1_scl_set;

	if (scaler >= CSS_SCALER_FD)
		return CSS_ERROR_INVALID_SCALER_NUM;

	if (scaler == CSS_SCALER_0) {
		scl_path0_scl_set.as32bits = readl(scl_hw->iobase + PATH0_SCL_SET);
		scl_path0_scl_set.asbits.scl_0_dst_format = pix_fmt;
		writel(scl_path0_scl_set.as32bits, scl_hw->iobase + PATH0_SCL_SET);
	} else if (scaler == CSS_SCALER_1) {
		scl_path1_scl_set.as32bits = readl(scl_hw->iobase + PATH1_SCL_SET);
		scl_path1_scl_set.asbits.scl_1_dst_format = pix_fmt;
		writel(scl_path1_scl_set.as32bits, scl_hw->iobase + PATH1_SCL_SET);
	} else {
		css_err("unsupported scaler num %d!!\n", scaler);
		ret = CSS_ERROR_INVALID_SCALER_NUM;
	}

	return ret;
}

static int scaler_hw_set_dither_enable(css_scaler_select scaler,
					struct css_dithering_set *DitherSet)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_DITHERING_SET scl_path0_dither_set;
	CSS_SCALER_PATH1_DITHERING_SET scl_path1_dither_set;

	if (scaler >= CSS_SCALER_FD)
		return CSS_ERROR_INVALID_SCALER_NUM;

	if (scaler == CSS_SCALER_0) {
		scl_path0_dither_set.as32bits
						= readl(scl_hw->iobase + PATH0_DITHERING_SET);
		scl_path0_dither_set.asbits.scl_0_yuv_cgain = DitherSet->c_gain;
		scl_path0_dither_set.asbits.scl_0_yuv_ygain = DitherSet->y_gain;
		scl_path0_dither_set.asbits.scl_0_yuv_pedi	= DitherSet->pedestal;
		scl_path0_dither_set.asbits.scl_0_yuv_sel	= DitherSet->select;
		scl_path0_dither_set.asbits.scl_0_dither_on = CSS_DITHERING_ENABLE;
		writel(scl_path0_dither_set.as32bits,
						scl_hw->iobase + PATH0_DITHERING_SET);
	} else if (scaler == CSS_SCALER_1) {
		scl_path1_dither_set.as32bits
						= readl(scl_hw->iobase + PATH1_DITHERING_SET);
		scl_path1_dither_set.asbits.scl_1_yuv_cgain = DitherSet->c_gain;
		scl_path1_dither_set.asbits.scl_1_yuv_ygain = DitherSet->y_gain;
		scl_path1_dither_set.asbits.scl_1_yuv_pedi	= DitherSet->pedestal;
		scl_path1_dither_set.asbits.scl_1_yuv_sel	= DitherSet->select;
		scl_path1_dither_set.asbits.scl_1_dither_on = CSS_DITHERING_ENABLE;
		writel(scl_path1_dither_set.as32bits,
						scl_hw->iobase + PATH1_DITHERING_SET);
	} else {
		css_err("unsupported scaler num %d!!\n", scaler);
		ret = CSS_ERROR_INVALID_SCALER_NUM;
	}
	return ret;
}

static int scaler_hw_set_dither_disable(css_scaler_select scaler)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_DITHERING_SET scl_path0_dither_set;
	CSS_SCALER_PATH1_DITHERING_SET scl_path1_dither_set;

	if (scaler >= CSS_SCALER_FD)
		return CSS_ERROR_INVALID_SCALER_NUM;

	if (scaler == CSS_SCALER_0) {
		scl_path0_dither_set.as32bits
						= readl(scl_hw->iobase + PATH0_DITHERING_SET);
		scl_path0_dither_set.asbits.scl_0_dither_on = CSS_DITHERING_DISABLE;
		writel(scl_path0_dither_set.as32bits,
						scl_hw->iobase + PATH0_DITHERING_SET);
	} else if (scaler == CSS_SCALER_1) {
		scl_path1_dither_set.as32bits
						= readl(scl_hw->iobase + PATH1_DITHERING_SET);
		scl_path1_dither_set.asbits.scl_1_dither_on = CSS_DITHERING_DISABLE;
		writel(scl_path1_dither_set.as32bits,
						scl_hw->iobase + PATH1_DITHERING_SET);
	} else {
		css_err("unsupported scaler num %d!!\n", scaler);
		ret = CSS_ERROR_INVALID_SCALER_NUM;
	}

	return ret;
}

static int scaler_hw_get_dither_set(css_scaler_select scaler,
					struct css_dithering_set *DitherSet)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_DITHERING_SET scl_path0_dither_set;
	CSS_SCALER_PATH1_DITHERING_SET scl_path1_dither_set;

	if (scaler >= CSS_SCALER_FD)
		return CSS_ERROR_INVALID_SCALER_NUM;

	if (scaler == CSS_SCALER_0) {
		scl_path0_dither_set.as32bits
						= readl(scl_hw->iobase + PATH0_DITHERING_SET);
		DitherSet->set = scl_path0_dither_set.asbits.scl_0_dither_on;
		DitherSet->select = scl_path0_dither_set.asbits.scl_0_yuv_sel;
		DitherSet->pedestal = scl_path0_dither_set.asbits.scl_0_yuv_pedi;
		DitherSet->y_gain = scl_path0_dither_set.asbits.scl_0_yuv_ygain;
		DitherSet->c_gain = scl_path0_dither_set.asbits.scl_0_yuv_cgain;
	} else if (scaler == CSS_SCALER_1) {
		scl_path1_dither_set.as32bits
						= readl(scl_hw->iobase + PATH1_DITHERING_SET);
		DitherSet->set = scl_path1_dither_set.asbits.scl_1_dither_on;
		DitherSet->select = scl_path1_dither_set.asbits.scl_1_yuv_sel;
		DitherSet->pedestal = scl_path1_dither_set.asbits.scl_1_yuv_pedi;
		DitherSet->y_gain = scl_path1_dither_set.asbits.scl_1_yuv_ygain;
		DitherSet->c_gain = scl_path1_dither_set.asbits.scl_1_yuv_cgain;
	} else {
		css_err("unsupported scaler num %d!!\n", scaler);
		ret = CSS_ERROR_INVALID_SCALER_NUM;
	}
	return ret;
}

static int scaler_hw_set_strobe(struct css_strobe_set *StrobeSet)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_VSYNC_DLY scl_path0_vsync_dly;
	CSS_SCALER_STROB scl_strob;

	scl_path0_vsync_dly.as32bits = readl(scl_hw->iobase + PATH0_VSYNC_DLY);
	scl_strob.as32bits			 = readl(scl_hw->iobase + STROB);
	scl_path0_vsync_dly.asbits.strobe_on = StrobeSet->enable;
	scl_strob.asbits.high		 = StrobeSet->high;
	scl_strob.asbits.time		 = StrobeSet->time;
	writel(scl_path0_vsync_dly.as32bits, scl_hw->iobase + PATH0_VSYNC_DLY);
	writel(scl_strob.as32bits, scl_hw->iobase + STROB);

	return ret;
}

static int scaler_hw_get_strobe(struct css_strobe_set *StrobeSet)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_VSYNC_DLY scl_path0_vsync_dly;
	CSS_SCALER_STROB scl_strob;

	scl_path0_vsync_dly.as32bits = readl(scl_hw->iobase + PATH0_VSYNC_DLY);
	scl_strob.as32bits			 = readl(scl_hw->iobase + STROB);
	StrobeSet->enable = scl_path0_vsync_dly.asbits.strobe_on;
	StrobeSet->high = scl_strob.asbits.high;
	StrobeSet->time = scl_strob.asbits.time;

	return ret;
}

static int scaler_hw_set_vsync_delay(css_scaler_select scaler,
					unsigned short delay)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_VSYNC_DLY scl_path0_vsync_dly;
	CSS_SCALER_PATH1_VSYNC_DLY scl_path1_vsync_dly;
	CSS_SCALER_FD_VSYNC_DLY scl_fd_vsync_dly;

	if (scaler == CSS_SCALER_0) {
		scl_path0_vsync_dly.as32bits = readl(scl_hw->iobase + PATH0_VSYNC_DLY);
		scl_path0_vsync_dly.asbits.vsync_delay_0 = delay;
		writel(scl_path0_vsync_dly.as32bits, scl_hw->iobase + PATH0_VSYNC_DLY);
	} else if (scaler == CSS_SCALER_1) {
		scl_path1_vsync_dly.as32bits =	readl(scl_hw->iobase + PATH1_VSYNC_DLY);
		scl_path1_vsync_dly.asbits.vsync_delay_1 = delay;
		writel(scl_path1_vsync_dly.as32bits, scl_hw->iobase + PATH1_VSYNC_DLY);
	} else if (scaler == CSS_SCALER_FD) {
		scl_fd_vsync_dly.as32bits =	readl(scl_hw->iobase + FD_VSYNC_DLY);
		scl_fd_vsync_dly.asbits.vsync_delay_fd = delay;
		writel(scl_fd_vsync_dly.as32bits, scl_hw->iobase + FD_VSYNC_DLY);
	} else {
		css_err("unsupported scaler num %d!!\n", scaler);
		ret = CSS_ERROR_INVALID_SCALER_NUM;
	}

	return ret;
}

static int scaler_hw_set_raw0_store(unsigned int bit_mode, unsigned int width,
					unsigned int height, unsigned int store_addr)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_RAW01_ST_SET scl_raw01_st_set;
	CSS_SCALER_RAW0_ST_SIZE scl_raw0_st_size;
	CSS_SCALER_RAW0_ST_DRAM_ADDR scl_raw0_st_dram_addr;
	unsigned long flags;

	spin_lock_irqsave(&scl_hw->slock, flags);
	scl_raw01_st_set.as32bits =	readl(scl_hw->iobase + RAW01_ST_SET);
	scl_raw0_st_size.as32bits =	readl(scl_hw->iobase + RAW0_ST_SIZE);

	scl_raw01_st_set.asbits.raw0_con_bit_mode	= bit_mode;
	scl_raw01_st_set.asbits.inst_raw0			= 1;
	scl_raw0_st_size.asbits.raw0_con_src_v		= height;
	scl_raw0_st_size.asbits.raw0_con_src_h		= width;
	scl_raw0_st_dram_addr.as32bits				= store_addr;

	writel(scl_raw01_st_set.as32bits, scl_hw->iobase + RAW01_ST_SET);
	writel(scl_raw0_st_size.as32bits, scl_hw->iobase + RAW0_ST_SIZE);
	writel(scl_raw0_st_dram_addr.as32bits, scl_hw->iobase + RAW0_ST_DRAM_ADDR);
	spin_unlock_irqrestore(&scl_hw->slock, flags);

	return ret;
}

static int scaler_hw_clr_raw0_store(void)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_RAW01_ST_SET scl_raw01_st_set;
	unsigned long flags;

	spin_lock_irqsave(&scl_hw->slock, flags);
	scl_raw01_st_set.as32bits =	readl(scl_hw->iobase + RAW01_ST_SET);
	scl_raw01_st_set.asbits.inst_raw0 = 0;
	writel(scl_raw01_st_set.as32bits, scl_hw->iobase + RAW01_ST_SET);
	spin_unlock_irqrestore(&scl_hw->slock, flags);

	return ret;
}

static int scaler_hw_set_raw1_store(unsigned int bit_mode, unsigned int width,
					unsigned int height, unsigned int store_addr)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_RAW01_ST_SET scl_raw01_st_set;
	CSS_SCALER_RAW1_ST_SIZE scl_raw1_st_size;
	CSS_SCALER_RAW1_ST_DRAM_ADDR scl_raw1_st_dram_addr;
	unsigned long flags;

	spin_lock_irqsave(&scl_hw->slock, flags);
	scl_raw01_st_set.as32bits =	readl(scl_hw->iobase + RAW01_ST_SET);
	scl_raw1_st_size.as32bits =	readl(scl_hw->iobase + RAW1_ST_SIZE);

	scl_raw01_st_set.asbits.raw1_con_bit_mode	= bit_mode;
	scl_raw01_st_set.asbits.inst_raw1			= 1;
	scl_raw1_st_size.asbits.raw1_con_src_v		= height;
	scl_raw1_st_size.asbits.raw1_con_src_h		= width;
	scl_raw1_st_dram_addr.as32bits				= store_addr;

	writel(scl_raw01_st_set.as32bits, scl_hw->iobase + RAW01_ST_SET);
	writel(scl_raw1_st_size.as32bits, scl_hw->iobase + RAW1_ST_SIZE);
	writel(scl_raw1_st_dram_addr.as32bits, scl_hw->iobase + RAW1_ST_DRAM_ADDR);
	spin_unlock_irqrestore(&scl_hw->slock, flags);

	return ret;
}

static int scaler_hw_clr_raw1_store(void)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_RAW01_ST_SET scl_raw01_st_set;
	unsigned long flags;

	spin_lock_irqsave(&scl_hw->slock, flags);
	scl_raw01_st_set.as32bits =	readl(scl_hw->iobase + RAW01_ST_SET);
	scl_raw01_st_set.asbits.inst_raw1 = 0;
	writel(scl_raw01_st_set.as32bits, scl_hw->iobase + RAW01_ST_SET);
	spin_unlock_irqrestore(&scl_hw->slock, flags);

	return ret;
}

static int scaler_hw_set_raw0_load(unsigned int bit_mode,
				css_scaler_load_img_format ld_fmt,
				unsigned int width, unsigned int height,
				unsigned int load_addr, unsigned int blank_count)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	unsigned int address = 0x0;
	unsigned int cb_addr = 0x0;
	unsigned int cr_addr = 0x0;
	CSS_SCALER_RAW0_LD_SET	scl_raw0_ld_set;
	CSS_SCALER_RAW0_LD_SIZE scl_raw0_ld_size;
	CSS_SCALER_RAW0_LD_DRAM_ADDR_Y scl_raw0_ld_dram_addr_y;
	CSS_SCALER_RAW0_LD_DRAM_ADDR_U scl_raw0_ld_dram_addr_u;
	CSS_SCALER_RAW0_LD_DRAM_ADDR_V scl_raw0_ld_dram_addr_v;

	if (ld_fmt > CSS_LOAD_IMG_RAW) {
		css_err("invalid raw0 load format\n");;
		return CSS_ERROR_INVALID_ARG;
	}

	scl_raw0_ld_set.as32bits					= 0;
	scl_raw0_ld_set.asbits.raw0_ld_raw_mode		= bit_mode;
	scl_raw0_ld_set.asbits.raw0_ld_h_bank_size	= blank_count;
	scl_raw0_ld_set.asbits.raw0_ld_img_format	= ld_fmt;
	scl_raw0_ld_size.asbits.raw0_ld_src_v		= height;
	scl_raw0_ld_size.asbits.raw0_ld_src_h		= width;

	writel(scl_raw0_ld_set.as32bits, scl_hw->iobase + RAW0_LD_SET);
	writel(scl_raw0_ld_size.as32bits, scl_hw->iobase + RAW0_LD_SIZE);

	switch (ld_fmt) {
	case CSS_LOAD_IMG_RGB565:
	case CSS_LOAD_IMG_RAW:
		address = load_addr;
		scl_raw0_ld_dram_addr_y.as32bits = address;
		writel(scl_raw0_ld_dram_addr_y.as32bits,
				scl_hw->iobase + RAW0_LD_DRAM_ADDR_Y);
		break;
	case CSS_LOAD_IMG_YUV422:
	case CSS_LOAD_IMG_YUV420:
		address = load_addr;
		cb_addr = address + (width * height);
		cr_addr = cb_addr + ((width * height)/4);
		scl_raw0_ld_dram_addr_y.as32bits = address;
		scl_raw0_ld_dram_addr_u.as32bits = cb_addr;
		scl_raw0_ld_dram_addr_v.as32bits = cr_addr;
		writel(scl_raw0_ld_dram_addr_y.as32bits,
				scl_hw->iobase + RAW0_LD_DRAM_ADDR_Y);
		writel(scl_raw0_ld_dram_addr_u.as32bits,
				scl_hw->iobase + RAW0_LD_DRAM_ADDR_U);
		writel(scl_raw0_ld_dram_addr_v.as32bits,
				scl_hw->iobase + RAW0_LD_DRAM_ADDR_V);
		break;
	default:
		ret = CSS_ERROR_INVALID_ARG;
		css_err("invalid raw0 load format!!\n");
		break;
	}

	return ret;
}

static int scaler_hw_clr_raw0_load(void)
{
	int ret = CSS_SUCCESS;
	/* NOTHING TO DO */
	return ret;
}

static int scaler_hw_set_raw1_load(unsigned int bit_mode,
					css_scaler_load_img_format ld_fmt,
					unsigned int width, unsigned int height,
					unsigned int load_addr, unsigned int blank_count)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	unsigned int address = 0x0;
	unsigned int cb_addr = 0x0;
	unsigned int cr_addr = 0x0;
	CSS_SCALER_RAW1_LD_SET scl_raw1_ld_set;
	CSS_SCALER_RAW1_LD_SIZE scl_raw1_ld_size;
	CSS_SCALER_RAW1_LD_DRAM_ADDR_Y scl_raw1_ld_dram_addr_y;
	CSS_SCALER_RAW1_LD_DRAM_ADDR_U scl_raw1_ld_dram_addr_u;
	CSS_SCALER_RAW1_LD_DRAM_ADDR_V scl_raw1_ld_dram_addr_v;

	if (ld_fmt > CSS_LOAD_IMG_RAW) {
		css_err("invalid raw1 load format\n");;
		return CSS_ERROR_INVALID_ARG;
	}

	scl_raw1_ld_set.as32bits					= 0;
	scl_raw1_ld_set.asbits.raw1_ld_raw_mode		= bit_mode;
	scl_raw1_ld_set.asbits.raw1_ld_h_bank_size	= blank_count;
	scl_raw1_ld_set.asbits.raw1_ld_img_format	= ld_fmt;
	scl_raw1_ld_size.asbits.raw1_ld_src_v		= height;
	scl_raw1_ld_size.asbits.raw1_ld_src_h		= width;

	writel(scl_raw1_ld_set.as32bits, scl_hw->iobase + RAW1_LD_SET);
	writel(scl_raw1_ld_size.as32bits, scl_hw->iobase + RAW1_LD_SIZE);

	switch (ld_fmt) {
	case CSS_LOAD_IMG_RGB565:
	case CSS_LOAD_IMG_RAW:
		address = load_addr;
		scl_raw1_ld_dram_addr_y.as32bits = address;
		writel(scl_raw1_ld_dram_addr_y.as32bits,
				scl_hw->iobase + RAW1_LD_DRAM_ADDR_Y);
		break;
	case CSS_LOAD_IMG_YUV422:
	case CSS_LOAD_IMG_YUV420:
		address = load_addr;
		cb_addr = address + (width * height);
		cr_addr = cb_addr + ((width * height) / 4);
		scl_raw1_ld_dram_addr_y.as32bits = address;
		scl_raw1_ld_dram_addr_u.as32bits = cb_addr;
		scl_raw1_ld_dram_addr_v.as32bits = cr_addr;
		writel(scl_raw1_ld_dram_addr_y.as32bits,
				scl_hw->iobase + RAW1_LD_DRAM_ADDR_Y);
		writel(scl_raw1_ld_dram_addr_u.as32bits,
				scl_hw->iobase + RAW1_LD_DRAM_ADDR_U);
		writel(scl_raw1_ld_dram_addr_v.as32bits,
				scl_hw->iobase + RAW1_LD_DRAM_ADDR_V);
		break;
	default:
		ret = CSS_ERROR_INVALID_ARG;
		css_err("invalid raw1 load format!!\n");
		break;
	}
	return ret;
}

static int scaler_hw_clr_raw1_load(void)
{
	int ret = CSS_SUCCESS;
	/* NOTHING TO DO */
	return ret;
}

static unsigned int scaler_hw_get_scl_0_frame_count(void)
{
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_SCALER_MONITOR scl_path0_scl_monitor;

	scl_path0_scl_monitor.as32bits = readl(scl_hw->iobase + PATH0_SCL_MONITOR);

	return scl_path0_scl_monitor.asbits.vsync_monitor_0;
}

static unsigned int scaler_hw_get_scl_1_frame_count(void)
{
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH1_SCALER_MONITOR scl_path1_scl_monitor;

	scl_path1_scl_monitor.as32bits = readl(scl_hw->iobase + PATH1_SCL_MONITOR);

	return scl_path1_scl_monitor.asbits.vsync_monitor_1;
}

static void scaler_hw_get_destination_size(css_scaler_select scaler,
					unsigned int *dst_w, unsigned int *dst_h)
{
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_SCALER_SIZE scl_path0_scl_size;
	CSS_SCALER_PATH1_SCALER_SIZE scl_path1_scl_size;
	CSS_FD_SCALER_SIZE fd_dst_size;

	BUG_ON(!dst_w);
	BUG_ON(!dst_h);

	switch (scaler) {
	case CSS_SCALER_0:
		scl_path0_scl_size.as32bits = readl(scl_hw->iobase + PATH0_SCL_SIZE);
		*dst_w = scl_path0_scl_size.asbits.scl_0_dst_h;
		*dst_h = scl_path0_scl_size.asbits.scl_0_dst_v;
		break;
	case CSS_SCALER_1:
		scl_path1_scl_size.as32bits = readl(scl_hw->iobase + PATH1_SCL_SIZE);
		*dst_w = scl_path1_scl_size.asbits.scl_1_dst_h;
		*dst_h = scl_path1_scl_size.asbits.scl_1_dst_v;
		break;
	case CSS_SCALER_FD:
		fd_dst_size.as32bits = readl(scl_hw->iobase + FD_SCALER_SIZE);
		*dst_w = fd_dst_size.asbits.horizontal;
		*dst_h = fd_dst_size.asbits.vertical;
		break;
	default:
		break;
	}
}

static int scaler_hw_get_pixel_format(css_scaler_select scaler)
{
	int fmt = -1;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_SCALER_SET scl_path0_scl_set;
	CSS_SCALER_PATH1_SCALER_SET scl_path1_scl_set;

	switch (scaler) {
	case CSS_SCALER_0:
		scl_path0_scl_set.as32bits = readl(scl_hw->iobase + PATH0_SCL_SET);
		fmt = scl_path0_scl_set.asbits.scl_0_dst_format;
		break;
	case CSS_SCALER_1:
		scl_path1_scl_set.as32bits = readl(scl_hw->iobase + PATH1_SCL_SET);
		fmt = scl_path1_scl_set.asbits.scl_1_dst_format;
		break;
	case CSS_SCALER_FD:
	default:
		break;
	}
	return fmt;
}

static unsigned int scaler_hw_reset_scl_0_frame_count(void)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH0_SCALER_MONITOR scl_path0_scl_monitor;

	scl_path0_scl_monitor.as32bits = 0;
	scl_path0_scl_monitor.asbits.monitor_rst_0 = 1;
	writel(scl_path0_scl_monitor.as32bits, scl_hw->iobase + PATH0_SCL_MONITOR);

	scl_path0_scl_monitor.asbits.monitor_rst_0 = 0;
	writel(scl_path0_scl_monitor.as32bits, scl_hw->iobase + PATH0_SCL_MONITOR);

	return ret;
}

static unsigned int scaler_hw_reset_scl_1_frame_count(void)
{
	int ret = CSS_SUCCESS;
	struct scaler_hardware *scl_hw = get_scl_hw();
	CSS_SCALER_PATH1_SCALER_MONITOR scl_path1_scl_monitor;

	scl_path1_scl_monitor.as32bits = 0;
	scl_path1_scl_monitor.asbits.monitor_rst_1 = 1;
	writel(scl_path1_scl_monitor.as32bits, scl_hw->iobase + PATH1_SCL_MONITOR);

	scl_path1_scl_monitor.asbits.monitor_rst_1 = 0;
	writel(scl_path1_scl_monitor.as32bits, scl_hw->iobase + PATH1_SCL_MONITOR);

	return ret;
}

int odin_scaler_probe(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;

	ret = odin_scaler_init_resources(pdev);
	if (ret == CSS_SUCCESS) {
		css_info("scaler probe ok!!\n");
	} else {
		css_err("scaler probe err!!\n");
	}

	return ret;
}

int odin_scaler_remove(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;

	ret = odin_scaler_deinit_resources(pdev);
	if (ret == CSS_SUCCESS) {
		css_info("scaler remove ok!!\n");
	} else {
		css_err("scaler remove err!!\n");
	}

	return ret;
}

int odin_scaler_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	return ret;
}

int odin_scaler_resume(struct platform_device *pdev)
{
	int ret = 0;
	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id odin_scaler_match[] = {
	{ .compatible = CSS_OF_SCALER_NAME},
	{},
};
#endif /* CONFIG_OF */

struct platform_driver odin_scaler_driver =
{
	.probe		= odin_scaler_probe,
	.remove 	= odin_scaler_remove,
	.suspend	= odin_scaler_suspend,
	.resume 	= odin_scaler_resume,
	.driver 	= {
		.name	= CSS_PLATDRV_SCALER,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_scaler_match),
#endif
	},
};

static int __init odin_scaler_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&odin_scaler_driver);

	return ret;
}

static void __exit odin_scaler_exit(void)
{
	platform_driver_unregister(&odin_scaler_driver);
	return;
}

MODULE_DESCRIPTION("odin scaler driver");
MODULE_AUTHOR("MTEKVISION<http://www.mtekvision.com>");

late_initcall(odin_scaler_init);
module_exit(odin_scaler_exit);

MODULE_LICENSE("GPL");

