/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * junglark.park <junglark.park@lgepartner.com>
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

// perform each display driver command using dpy_drv handle..
// perform each DIP, MIPI DSI HALs using dpy_port handle.
// FB 을 이용하여 output 제어...

// Runtime PM framework uses abstract states of devices
// ACTIVE - Device can do I/O (presumably in the full-power state).
// SUSPENDED - Device cannot do I/O (presumably in a low-power state).
// SUSPENDING - Device state is changing from ACTIVE to SUSPENDED.
// RESUMING - Device state is changing from SUSPENDED to ACTIVE.
//
// Runtime PM framework is oblivious to the actual states of devices
// The real states of devices at any given time depend on the subsystems and
// drivers that handle them.

/*------------------------------------------------------------------------------
  File Inclusions
  ------------------------------------------------------------------------------*/

#include <asm/io.h>

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/odin_pmic.h>
#include <linux/odin_pm_domain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/vmalloc.h>
#include <linux/clk-private.h>

#include "dip_mipi.h"
#include "hal/mipi_dsi_common.h"
#include "mipi_dsi_drv.h"

#include <video/odin-dss/mipi_device.h>

#define MIPI_HAL_INF_DEFINE(dip_num) \
	static bool _dss_dip_mipi_hal_##dip_num##_open(struct mipi_dsi_desc *dsi_desc) { \
		return __dss_dip_mipi_hal_open(dip_num, dsi_desc); \
	}; \
	static bool _dss_dip_mipi_hal_##dip_num##_close(void) { \
		return __dss_dip_mipi_hal_close(dip_num); \
	}; \
	static bool _dss_dip_mipi_hal_##dip_num##_send(struct dsi_packet *packet) { \
		return __dss_dip_mipi_hal_send(dip_num, packet); \
	}; \
	static bool _dss_dip_mipi_hal_##dip_num##_start_video_mode(void) { \
		return __dss_dip_mipi_hal_start_video_mode(dip_num); \
	}; \
	static bool _dss_dip_mipi_hal_##dip_num##_stop_video_mode(void) { \
		return __dss_dip_mipi_hal_stop_video_mode(dip_num); \
	}

#define MIPI_HAL_INF_ASSIGN(dip_num) { \
	.open = _dss_dip_mipi_hal_##dip_num##_open, \
	.close = _dss_dip_mipi_hal_##dip_num##_close, \
	.send = _dss_dip_mipi_hal_##dip_num##_send, \
	.start_video_mode = _dss_dip_mipi_hal_##dip_num##_start_video_mode, \
	.stop_video_mode = _dss_dip_mipi_hal_##dip_num##_stop_video_mode, \
}

#define MIPI_DEV_INF_DEFINE(dip_num) \
	static bool _dss_dip_mipi_dev_##dip_num##_blank(bool blank) { \
		return __dss_dip_mipi_dev_blank(dip_num, blank) ;\
	}; \
	static bool _dss_dip_mipi_dev_##dip_num##_is_command_mode(void) { \
		return __dss_dip_mipi_dev_is_command_mode(dip_num) ;\
	}

struct dip_mipi_desc
{
	bool (*cb_dssfb_register)(enum dip_data_path dip, struct dssfb_dev_entry *fbdev);
	void (*cb_dssfb_unregister)(enum dip_data_path dip);
};
static struct dip_mipi_desc *d;

static bool __dss_dip_mipi_hal_open(enum dip_data_path dip_num, struct mipi_dsi_desc *dsi_desc)
{
	bool ret = true;
	if (dss_dip_mipi_verify_dip_data_path(dip_num) == false) {
		pr_err("invalid dip_num(%d)\n", dip_num);
		ret = false;
	} else {
		ret = mipi_dsi_hal_open(dip_num, dsi_desc);
	}
	return ret;
}

static bool __dss_dip_mipi_hal_close(enum dip_data_path dip_num)
{
	bool ret = true;
	if (dss_dip_mipi_verify_dip_data_path(dip_num) == false) {
		pr_err("invalid dip_num(%d)\n", dip_num);
		ret = false;
	} else {
		ret = mipi_dsi_hal_close(dip_num);
	}
	return ret;
}

static bool __dss_dip_mipi_hal_send(enum dip_data_path dip_num, struct dsi_packet *packet)
{
	bool ret = true;
	if (dss_dip_mipi_verify_dip_data_path(dip_num) == false) {
		pr_err("invalid dip_num(%d)\n", dip_num);
		ret = false;
	} else {
		ret = mipi_dsi_hal_send(dip_num, packet);
	}
	return ret;
}

static bool __dss_dip_mipi_hal_start_video_mode(enum dip_data_path dip_num)
{
	bool ret = true;
	if (dss_dip_mipi_verify_dip_data_path(dip_num) == false) {
		pr_err("invalid dip_num(%d)\n", dip_num);
		ret = false;
	} else {
		ret = mipi_dsi_hal_start_video_mode((enum dsi_module)dip_num);
	}
	return ret;
}

static bool __dss_dip_mipi_hal_stop_video_mode(enum dip_data_path dip_num)
{
	bool ret = true;
	if (dss_dip_mipi_verify_dip_data_path(dip_num) == false) {
		pr_err("invalid dip_num(%d)\n", dip_num);
		ret = false;
	} else {
		ret = mipi_dsi_hal_stop_video_mode((enum dsi_module)dip_num);
	} 
	return ret;
}

MIPI_HAL_INF_DEFINE(DIP0_LCD);
MIPI_HAL_INF_DEFINE(DIP1_LCD);

static struct dip_dev
{
	struct device *dev;
	struct mipi_hal_interface mipi_inf;
	struct mipi_device_interface ops;

	char *clk[2];
} dip_dev[DIP_NUM] = {
#if 1
	{.mipi_inf = MIPI_HAL_INF_ASSIGN(DIP0_LCD), .clk = {"dip0_clk", NULL},},
	{.mipi_inf = MIPI_HAL_INF_ASSIGN(DIP1_LCD), .clk = {"dip1_clk", NULL},},
#else
	{.mipi_inf = MIPI_HAL_INF_ASSIGN(DIP0_LCD), .clk = {"dip0_clk", "disp0_clk", "dss_mipi0_clk", "dphy0_osc_clk","tx_esc0_disp", NULL},},
	{.mipi_inf = MIPI_HAL_INF_ASSIGN(DIP1_LCD), .clk = {"dip1_clk", "disp1_clk", "dss_mipi1_clk", "dphy1_osc_clk","tx_esc1_disp", NULL},},
#endif	
};

static bool __dss_dip_mipi_dev_blank(enum dip_data_path dip_num, bool blank)
{
	struct dip_dev *dip_device = NULL;

	if (dss_dip_mipi_verify_dip_data_path(dip_num) == false) {
		pr_err("invalid dip_num(%d)\n", dip_num);
		return false;
	}

	dip_device = &dip_dev[dip_num];

	if (dip_device->ops.blank == NULL) {
		pr_warn("dip_device->ops.blank is NULL\n");
		return false;
	}

	if (blank) {
		dip_device->ops.blank(blank);
		pm_runtime_put_sync(dip_device->dev);
	}
	else {
		pm_runtime_get_sync(dip_device->dev);
		dip_device->ops.blank(blank);
	}

	return true;
}

static unsigned int __dss_dip_mipi_dev_is_command_mode(enum dip_data_path dip_num)
{
	struct dip_dev *dip_device = NULL;

	if (dss_dip_mipi_verify_dip_data_path(dip_num) == false) {
		pr_err("invalid dip_num(%d)\n", dip_num);
		return 0xffffffff;
	}

	dip_device = &dip_dev[dip_num];

	if (dip_device->ops.is_command_mode == NULL) {
		pr_warn("dip_device->ops.is_command_mode is NULL\n");
		return 0xffffffff;
	}

	return dip_device->ops.is_command_mode();
}

MIPI_DEV_INF_DEFINE(DIP0_LCD);
MIPI_DEV_INF_DEFINE(DIP1_LCD);

bool mipi_device_ops_register(enum dsi_module dsi_idx, struct mipi_device_interface *mipi_ops)
{
	struct dssfb_dev_entry dssfb_dev;
	struct panel_info info;
	struct dip_dev *dip_device = NULL;

	if( dsi_idx >= MIPI_DSI_MAX) {
		pr_err("invalid ds_idx(%d\n", dsi_idx);
		return false;
	}

	dip_device = &dip_dev[dsi_idx];

//	pm_runtime_get_sync(dip_device->dev);

	dip_device->ops = *mipi_ops;
	dip_device->ops.init(&dip_device->mipi_inf);
	dip_device->ops.get_screen_info(&info);

	dssfb_dev.name = info.name;
	dssfb_dev.panel.xres = info.x_res;
	dssfb_dev.panel.yres = info.y_res;
	dssfb_dev.panel.bpp = info.bpp;
	dssfb_dev.panel.pixel_clock=  info.pixel_clock;
	dssfb_dev.panel.left_margin=  info.hbp;
	dssfb_dev.panel.right_margin= info.hfp;
	dssfb_dev.panel.upper_margin= info.vbp;
	dssfb_dev.panel.lower_margin= info.vfp;
	dssfb_dev.panel.hsync_len= info.hsw;
	dssfb_dev.panel.vsync_len= info.vsw;
	dssfb_dev.panel.hsync_polarity = info.hsync_pol;
	dssfb_dev.panel.vsync_polarity = info.vsync_pol;

	switch(dsi_idx) {
	case MIPI_DSI_DIP0:
		dssfb_dev.blank = _dss_dip_mipi_dev_DIP0_LCD_blank;
		dssfb_dev.is_command_mode = _dss_dip_mipi_dev_DIP0_LCD_is_command_mode;
		break;
	case MIPI_DSI_DIP1:
		dssfb_dev.blank = _dss_dip_mipi_dev_DIP1_LCD_blank;
		dssfb_dev.is_command_mode = _dss_dip_mipi_dev_DIP1_LCD_is_command_mode;
		break;
	default:
		pm_runtime_put_sync_suspend(dip_device->dev);
		BUG();
		return false;
	}

	d->cb_dssfb_register(dsi_idx, &dssfb_dev);

	return true;
}

static int _dip_runtime_resume(struct device *dev)
{
	int i = 0;
	struct clk *clk = NULL;
	struct dip_dev *dip_device = NULL;

	dip_device = (struct dip_dev *)dev->platform_data;

	while(dip_device->clk[i]) {
		clk  = clk_get(NULL, dip_device->clk[i]);
		if (IS_ERR_OR_NULL(clk)) {
			pr_err("clk_get failed. clk:%s\n", dip_device->clk[i]);
			return -1;
		}

		clk_prepare_enable(clk);

		i++;
	}

	return 0;
}

static int _dip_runtime_suspend(struct device *dev)
{
	int i = 0;
	struct clk *clk = NULL;
	struct dip_dev *dip_device = NULL;

	pr_info("%s \n",__func__);

	dip_device = (struct dip_dev *)dev->platform_data;

	while(dip_device->clk[i]) {
		clk  = clk_get(NULL, dip_device->clk[i]);
		if (IS_ERR_OR_NULL(clk)) {
			pr_err("clk_get failed. clk:%s\n", dip_device->clk[i]);
			return -1;
		}

		clk_disable_unprepare(clk);

		i++;
	}

	return 0;
}

static const struct dev_pm_ops _dip_pm_ops =
{
	.runtime_resume = _dip_runtime_resume,
	.runtime_suspend = _dip_runtime_suspend,
};

static struct of_device_id _dip0_match[]=
{
	{ .compatible="odin,dss,dip0_lcd"},
	{},
};

static struct of_device_id _dip1_match[]=
{
	{ .compatible="odin,dss,dip1_lcd"},
	{},
};

static struct platform_driver _dip0_drv =
{
	.driver = {
		.name = "dip0_lcd",
		.owner = THIS_MODULE,
		.pm = &_dip_pm_ops,
		.of_match_table = of_match_ptr(_dip0_match),
	},
};

static struct platform_driver _dip1_drv =
{
	.driver = {
		.name = "dip1_lcd",
		.owner = THIS_MODULE,
		.pm = &_dip_pm_ops,
		.of_match_table = of_match_ptr(_dip1_match),
	},
};

bool dss_dip_mipi_init(struct platform_device *pdev_dip[],
		bool (*cb_dssfb_register)(enum dip_data_path dip, struct dssfb_dev_entry *fbdev),
		void (*cb_dssfb_unregister)(enum dip_data_path dip))
{
	int ret = 0;
	if (d) {
		pr_warn("dip_mipi is alread intialized\n");
		return true;
	}

	d = vzalloc(sizeof(struct dip_mipi_desc));
	if (d == NULL) {
		pr_err("vzalloc failed. size:%d\n", sizeof(struct dip_mipi_desc));
		return false;
	}

	d->cb_dssfb_register = cb_dssfb_register;
	d->cb_dssfb_unregister = cb_dssfb_unregister;

	dip_dev[DIP0_LCD].dev= &(pdev_dip[DIP0_LCD]->dev);
	dip_dev[DIP1_LCD].dev= &(pdev_dip[DIP1_LCD]->dev);

	dip_dev[DIP0_LCD].dev->platform_data = &dip_dev[DIP0_LCD];
	dip_dev[DIP1_LCD].dev->platform_data = &dip_dev[DIP1_LCD];

	ret = odin_pd_register_dev(dip_dev[DIP0_LCD].dev, &odin_pd_dss3_dip0_lcd);
	if (ret < 0) {
		pr_err("DPI_LCD0 odin_pd_register_dev failed ret:%d\n", ret);
		return false;
	}

	ret = odin_pd_register_dev(dip_dev[DIP1_LCD].dev, &odin_pd_dss3_dip1_lcd);
	if (ret < 0) {
		pr_err("DPI_LCD1 odin_pd_register_dev failed ret:%d\n", ret);
		return false;
	}

	// add panel device
	platform_driver_register(&_dip0_drv);
	platform_driver_register(&_dip1_drv);

	pm_runtime_enable(dip_dev[DIP0_LCD].dev);
	pm_runtime_enable(dip_dev[DIP1_LCD].dev);

	return true;
}

bool dss_dip_mipi_cleanup(void)
{
	return true;
}

MODULE_AUTHOR("LGE");
MODULE_DESCRIPTION("DIP MIPI driver");
MODULE_LICENSE("GPL");

