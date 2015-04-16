 /* Copyright (c) 2013-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "k3_fb.h"
#include "k3_overlay_utils.h"
#include <linux/log_jank.h>

static struct dsm_dev dsm_lcd = {
	.name = "dsm_lcd",
	.fops = NULL,
	.buff_size = 1024,
};
struct dsm_client *lcd_dclient = NULL;

#if 0
/dev/graphics/fb0
/sys/class/graphics/fb0
/sys/devices/platform/
#endif

static int k3_fb_resource_initialized;
static struct platform_device *pdev_list[K3_FB_MAX_DEV_LIST] = {0};

static int pdev_list_cnt;
struct fb_info *fbi_list[K3_FB_MAX_FBI_LIST] = {0};
static int fbi_list_index;

struct k3_fb_data_type *k3fd_list[K3_FB_MAX_FBI_LIST] = {0};
static int k3fd_list_index;

#define K3_FB_ION_CLIENT_NAME	"k3_fb_ion"

u32 g_dts_resouce_ready = 0;

static char __iomem *k3fd_dss_base;
static char __iomem *k3fd_crgperi_base;

static u32 k3fd_irq_pdp;
static u32 k3fd_irq_sdp;
static u32 k3fd_irq_adp;
static u32 k3fd_irq_dsi0;
static u32 k3fd_irq_dsi1;

#define MAX_DPE_NUM	(3)
static struct regulator_bulk_data g_dpe_regulator[MAX_DPE_NUM] =
	{{0}, {0}, {0}};

static const char *g_dss_axi_clk_name;
static const char *g_dss_pri_clk_name;
static const char *g_dss_aux_clk_name;
static const char *g_dss_pxl0_clk_name;
static const char *g_dss_pxl1_clk_name;
static const char *g_dss_pclk_clk_name;
static const char *g_dss_dphy0_clk_name;
static const char *g_dss_dphy1_clk_name;

int g_primary_lcd_xres = 0;
int g_primary_lcd_yres = 0;


/*
** for debug, S_IRUGO
** /sys/module/k3fb/parameters
*/
/* Setting k3_fb_msg_level to 8 prints out ALL messages */
u32 k3_fb_msg_level = 7;
module_param_named(debug_msg_level, k3_fb_msg_level, int, 0644);
MODULE_PARM_DESC(debug_msg_level, "k3 fb msg level");

int g_debug_ldi_underflow = 0;
module_param_named(debug_ldi_underflow, g_debug_ldi_underflow, int, 0644);
MODULE_PARM_DESC(debug_ldi_underflow, "k3 ldi_underflow debug");

int g_debug_mmu_error = 0;
module_param_named(debug_mmu_error, g_debug_mmu_error, int, 0644);
MODULE_PARM_DESC(debug_mmu_error, "k3 mmu error");

int g_debug_ovl_osd = 0;
module_param_named(debug_ovl_osd, g_debug_ovl_osd, int, 0644);
MODULE_PARM_DESC(debug_ovl_osd, "k3 overlay osd debug");

int g_debug_online_vsync = 0;
module_param_named(debug_online_vsync, g_debug_online_vsync, int, 0644);
MODULE_PARM_DESC(debug_online_vsync, "k3 online vsync debug");

int g_debug_ovl_online_composer = 0;
module_param_named(debug_ovl_online_composer, g_debug_ovl_online_composer, int, 0644);
MODULE_PARM_DESC(debug_ovl_online_composer, "k3 overlay online composer debug");

int g_debug_ovl_offline_composer = 0;
module_param_named(debug_ovl_offline_composer, g_debug_ovl_offline_composer, int, 0644);
MODULE_PARM_DESC(debug_ovl_offline_composer, "k3 overlay offline composer debug");

int g_debug_ovl_offline_cmdlist = 0;
module_param_named(g_debug_ovl_offline_cmdlist, g_debug_ovl_offline_cmdlist, int, 0644);
MODULE_PARM_DESC(g_debug_ovl_offline_cmdlist, "k3 overlay offline cmdlist debug");

int g_debug_ovl_optimized = 0;
module_param_named(debug_ovl_optimized, g_debug_ovl_optimized, int, 0644);
MODULE_PARM_DESC(debug_ovl_optimized, "k3 overlay optimized debug");

int g_enable_ovl_optimized = 0;
module_param_named(enable_ovl_optimized, g_enable_ovl_optimized, int, 0644);
MODULE_PARM_DESC(enable_ovl_optimized, "k3 overlay optimized enable");

int g_scf_stretch_threshold = SCF_STRETCH_THRESHOLD;
module_param_named(scf_stretch_threshold, g_scf_stretch_threshold, int, 0644);
MODULE_PARM_DESC(scf_stretch_threshold, "k3 scf stretch threshold");

int g_enable_dirty_region_updt = 1;
module_param_named(enable_dirty_region_updt, g_enable_dirty_region_updt, int, 0644);
MODULE_PARM_DESC(enable_dirty_region_updt, "k3 dss dirty_region_updt enable");

int g_debug_dirty_region_updt = 0;
module_param_named(debug_dirty_region_updt, g_debug_dirty_region_updt, int, 0644);
MODULE_PARM_DESC(debug_dirty_region_updt, "k3 dss dirty_region_updt debug");

int g_debug_mipi_dphy_lp = 1;
module_param_named(debug_mipi_dphy_lp, g_debug_mipi_dphy_lp, int, 0644);
MODULE_PARM_DESC(debug_mipi_dphy_lp, "k3 mipi dphy lowpower debug");

int g_debug_dss_clk_lp = 1;
module_param_named(debug_dss_clk_lp, g_debug_dss_clk_lp, int, 0644);
MODULE_PARM_DESC(debug_dss_clk_lp, "k3 dss clock lowpower debug");

int g_debug_dss_adp_sr = 0;
module_param_named(debug_dss_adp_sr, g_debug_dss_adp_sr, int, 0644);
MODULE_PARM_DESC(debug_dss_adp_sr, "k3 dss adp suspend & resume debug");



//Multi Resolution====begin
/*added for resolution switch*/
static int k3_fb_switch_resolution_process(struct k3_fb_data_type * k3fd);
//Multi Resolution====end

/******************************************************************************
** FUNCTIONS PROTOTYPES
*/
static int k3_fb_register(struct k3_fb_data_type *k3fd);
static int k3_fb_blank_sub(int blank_mode, struct fb_info *info);

static int k3_fb_open(struct fb_info *info, int user);
static int k3_fb_release(struct fb_info *info, int user);
static int k3_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info);
static int k3_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info);
static int k3_fb_set_par(struct fb_info *info);
static int k3_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg);
static int k3_fb_mmap(struct fb_info *info, struct vm_area_struct * vma);

static int k3_fb_suspend_sub(struct k3_fb_data_type *k3fd);
static int k3_fb_resume_sub(struct k3_fb_data_type *k3fd);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void k3fb_early_suspend(struct early_suspend *h);
static void k3fb_early_resume(struct early_suspend *h);
#endif

static void k3fb_pm_runtime_get(struct k3_fb_data_type *k3fd);
static void k3fb_pm_runtime_put(struct k3_fb_data_type *k3fd);
static void k3fb_pm_runtime_register(struct platform_device *pdev);
static void k3fb_pm_runtime_unregister(struct platform_device *pdev);

static unsigned long k3fb_alloc_fb_buffer(struct k3_fb_data_type *k3fd);
static void k3fb_free_fb_buffer(struct k3_fb_data_type *k3fd);


/*******************************************************************************
**
*/

void k3_fb1_power_ctrl(bool power_full)
{
	struct fb_info *info = 0;
	struct k3_fb_data_type *k3fd = NULL;

	info = fbi_list[1];
	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	K3_FB_INFO("fb%d, power_full(%d) +.\n", k3fd->index, power_full);

	down(&k3fd->blank_sem);
	if (k3fd->is_hdmi_power_full == power_full){
		up(&k3fd->blank_sem);
		return;
	}

	if (power_full == true) {
		k3fd->is_hdmi_power_full = true;
	} else {
		k3fd->is_hdmi_power_full = false;

		if(k3fd->buf_sync_suspend)
			k3fd->buf_sync_suspend(k3fd);
	}
	up(&k3fd->blank_sem);
}
EXPORT_SYMBOL(k3_fb1_power_ctrl);

int k3_fb1_blank(int blank_mode)
{
	int ret = 0;
	struct fb_info *info = 0;
	struct k3_fb_data_type *k3fd = NULL;

	info = fbi_list[1];
	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	K3_FB_INFO("fb%d, blank_mode(%d) +.\n", k3fd->index, blank_mode);

	ret = k3_fb_blank_sub(blank_mode, info);
	if (ret != 0) {
		K3_FB_ERR("fb%d, blank_mode(%d) failed!\n", k3fd->index, blank_mode);
	}

	K3_FB_INFO("fb%d, blank_mode(%d) -.\n", k3fd->index, blank_mode);

	return ret;
}
EXPORT_SYMBOL(k3_fb1_blank);

struct platform_device *k3_fb_add_device(struct platform_device *pdev)
{
	struct k3_fb_panel_data *pdata = NULL;
	struct platform_device *this_dev = NULL;
	struct fb_info *fbi = NULL;
	struct k3_fb_data_type *k3fd = NULL;
	u32 type = 0;
	u32 id = 0;

	BUG_ON(pdev == NULL);
	pdata = dev_get_platdata(&pdev->dev);
	BUG_ON(pdata == NULL);

	if (fbi_list_index >= K3_FB_MAX_FBI_LIST) {
		K3_FB_ERR("no more framebuffer info list!\n");
		return NULL;
	}

	id = pdev->id;
	type = pdata->panel_info->type;

	/* alloc panel device data */
	this_dev = k3_fb_device_alloc(pdata, type, id);
	if (!this_dev) {
		K3_FB_ERR("failed to k3_fb_device_alloc!\n");
		return NULL;
	}

	/* alloc framebuffer info + par data */
	fbi = framebuffer_alloc(sizeof(struct k3_fb_data_type), NULL);
	if (fbi == NULL) {
		K3_FB_ERR("can't alloc framebuffer info data!\n");
		platform_device_put(this_dev);
		return NULL;
	}

	k3fd = (struct k3_fb_data_type *)fbi->par;
	memset(k3fd, 0, sizeof(struct k3_fb_data_type));
	k3fd->fbi = fbi;

	k3fd->fb_imgType = K3_FB_PIXEL_FORMAT_BGRA_8888;
	k3fd->index = fbi_list_index;
	k3fd->dss_base = k3fd_dss_base;
	k3fd->crgperi_base = k3fd_crgperi_base;

	k3fd->dss_axi_clk_name = g_dss_axi_clk_name;
	k3fd->dss_pri_clk_name = g_dss_pri_clk_name;
	k3fd->dss_aux_clk_name = g_dss_aux_clk_name;
	k3fd->dss_pxl0_clk_name = g_dss_pxl0_clk_name;
	k3fd->dss_pxl1_clk_name = g_dss_pxl1_clk_name;
	k3fd->dss_pclk_clk_name = g_dss_pclk_clk_name;
	k3fd->dss_dphy0_clk_name = g_dss_dphy0_clk_name;
	k3fd->dss_dphy1_clk_name = g_dss_dphy1_clk_name;

	k3fd->dsi0_irq = k3fd_irq_dsi0;
	k3fd->dsi1_irq = k3fd_irq_dsi1;
	if (k3fd->index == PRIMARY_PANEL_IDX) {
		k3fd->fb_num = K3_FB0_NUM;
		k3fd->dpe_irq = k3fd_irq_pdp;
		k3fd->dpe_regulator = &(g_dpe_regulator[0]);
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		k3fd->fb_num = K3_FB1_NUM;
		k3fd->dpe_irq = k3fd_irq_sdp;
		k3fd->dpe_regulator = &(g_dpe_regulator[1]);
	} else if (k3fd->index == AUXILIARY_PANEL_IDX) {
		k3fd->fb_num = K3_FB2_NUM;
		k3fd->dpe_irq = k3fd_irq_adp;
		k3fd->dpe_regulator = &(g_dpe_regulator[2]);
	} else {
		K3_FB_ERR("fb%d not support now!\n", k3fd->index);
		return NULL;
	}

	/* link to the latest pdev */
	k3fd->pdev = this_dev;

	k3fd_list[k3fd_list_index++] = k3fd;
	fbi_list[fbi_list_index++] = fbi;

	 /* get/set panel info */
	memcpy(&k3fd->panel_info, pdata->panel_info, sizeof(struct k3_panel_info));

	/* set driver data */
	platform_set_drvdata(this_dev, k3fd);

	if (platform_device_add(this_dev)) {
		K3_FB_ERR("failed to platform_device_add!\n");
		platform_device_put(this_dev);
		framebuffer_release(fbi);
		fbi_list_index--;
		return NULL;
	}

	return this_dev;
}

static int k3fb_set_timing(struct fb_info *info, unsigned long *argp)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_fb_panel_data *pdata = NULL;
	struct fb_var_screeninfo var;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	pdata = dev_get_platdata(&k3fd->pdev->dev);
	if ((!pdata) || (!pdata->set_timing)) {
		K3_FB_ERR("no panel operation detected!\n");
		return -ENODEV;
	}

	if (!k3fd->panel_power_on) {
		K3_FB_ERR("panel power off!\n");
		return -EPERM;
	}

	ret = copy_from_user(&var, argp, sizeof(var));
	if (ret) {
		K3_FB_ERR("copy from user failed!\n");
		return ret;
	}

	memcpy(&info->var, &var, sizeof(var));
	k3fd->panel_info.xres = var.xres;
	k3fd->panel_info.yres = var.yres;

	k3fd->panel_info.pxl_clk_rate = (var.pixclock == 0) ? k3fd->panel_info.pxl_clk_rate : var.pixclock;
	k3fd->panel_info.ldi.h_front_porch = var.right_margin;
	k3fd->panel_info.ldi.h_back_porch = var.left_margin;
	k3fd->panel_info.ldi.h_pulse_width = var.hsync_len;
	k3fd->panel_info.ldi.v_front_porch = var.lower_margin;
	k3fd->panel_info.ldi.v_back_porch = var.upper_margin;
	k3fd->panel_info.ldi.v_pulse_width = var.vsync_len;
#if 0
	k3fd->panel_info.ldi.vsync_plr = hdmi_get_vsync_bycode(var.reserved[3]);
	k3fd->panel_info.ldi.hsync_plr = hdmi_get_hsync_bycode(var.reserved[3]);
#endif
	K3_FB_INFO("HDMI ldi parameter!\n");
	K3_FB_INFO("\t  xres | yres | ri_mar | lef_mar | hsync | low_mar |   up_mar  | vsync \n");
	K3_FB_INFO("\t--------+-----------+---------+-----------+----------+-------+------------+----------\n");
	K3_FB_INFO("\t %6d | %6d | %8d | %9d | %7d | %9d | %8d | %7d \n",
		var.xres, var.yres, var.right_margin,
		var.left_margin, var.hsync_len, var.lower_margin,
		var.upper_margin, var.vsync_len);
	K3_FB_INFO("pxl_clk_rate:%ld\n", k3fd->panel_info.pxl_clk_rate);

	if (pdata && pdata->set_timing) {
		k3fb_activate_vsync(k3fd);
		if (pdata->set_timing(k3fd->pdev) != 0) {
			K3_FB_ERR("set timing failed!\n");
		}
		k3fb_deactivate_vsync(k3fd);
	}

	return 0;
}

#ifdef CONFIG_LCD_CHECK_REG
static int k3_fb_check_lcd_reg(struct fb_info *info, unsigned long *argp)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_fb_panel_data *pdata = NULL;
	int lcd_check = LCD_CHECK_REG_FAIL;

	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (!k3fd->panel_power_on) {
		K3_FB_ERR("fb%d, panel power off!\n", k3fd->index);
		goto err_out;
	}

	if (pdata && pdata->lcd_check_reg) {
		k3fb_activate_vsync(k3fd);
		lcd_check = pdata->lcd_check_reg(k3fd->pdev);
		k3fb_deactivate_vsync(k3fd);
	} else {
		K3_FB_ERR("lcd_check_reg is NULL\n");
	}

err_out:
	if (copy_to_user(argp, &lcd_check, sizeof(lcd_check))) {
		K3_FB_ERR("copy to user fail");
		return -EFAULT;
	}

	return 0;
}
#endif

#ifdef CONFIG_LCD_MIPI_DETECT
static int k3_fb_mipi_detect(struct fb_info *info, unsigned long *argp)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_fb_panel_data *pdata = NULL;
	int mipi_detect = LCD_MIPI_DETECT_FAIL;

	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (!k3fd->panel_power_on) {
		K3_FB_ERR("fb%d, panel power off!\n", k3fd->index);
		goto err_out;
	}

	if (pdata && pdata->lcd_mipi_detect) {
		k3fb_activate_vsync(k3fd);
		mipi_detect = pdata->lcd_mipi_detect(k3fd->pdev);
		k3fb_deactivate_vsync(k3fd);
	} else {
		K3_FB_ERR("lcd_mipi_detect is NULL\n");
	}

err_out:
	if (copy_to_user(argp, &mipi_detect, sizeof(mipi_detect))) {
		K3_FB_ERR("copy to user fail");
		return -EFAULT;
	}

	return 0;
}
#endif

static int k3_fb_blank_sub(int blank_mode, struct fb_info *info)
{
	struct k3_fb_data_type *k3fd = NULL;
	int ret = 0;
	int curr_pwr_state = 0;

	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	down(&k3fd->blank_sem);
	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		if (!k3fd->panel_power_on) {
			ret = k3fd->on_fnc(k3fd);
			if (ret == 0) {
				k3fd->panel_power_on = true;
			}
		}
		break;

	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_NORMAL:
	case FB_BLANK_POWERDOWN:
	default:
		if (k3fd->panel_power_on) {
			curr_pwr_state = k3fd->panel_power_on;
			k3fd->panel_power_on = false;

			if (k3fd->bl_cancel) {
				k3fd->bl_cancel(k3fd);
			}

			ret = k3fd->off_fnc(k3fd);
			if (ret)
				k3fd->panel_power_on = curr_pwr_state;

			if(k3fd->buf_sync_suspend)
				k3fd->buf_sync_suspend(k3fd);
		}
		break;
	}
	up(&k3fd->blank_sem);

	return ret;
}

#ifdef CONFIG_FASTBOOT_DISP_ENABLE
static int fastboot_set_needed;

static bool k3_fb_set_fastboot_needed(struct fb_info *info)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	if (fastboot_set_needed) {
		k3fb_ctrl_fastboot(k3fd);

		k3fd->panel_power_on = true;
#if 0
		if (info->screen_base && (info->fix.smem_len > 0))
			memset(info->screen_base, 0x0, info->fix.smem_len);
#endif
		if (!IS_ERR_OR_NULL(k3fd->ion_client)
			&& !IS_ERR_OR_NULL(k3fd->ion_handle)
			&& (info->fix.smem_len > 0)) {
			void* vaddr = NULL;
			vaddr = ion_map_kernel(k3fd->ion_client, k3fd->ion_handle);
			if (!IS_ERR_OR_NULL(vaddr)) {
				memset(vaddr, 0x0, info->fix.smem_len);
				ion_unmap_kernel(k3fd->ion_client, k3fd->ion_handle);
			}
		}
		fastboot_set_needed = 0;
		return true;
	}

	return false;
}
#endif

static int k3_fb_open_sub(struct fb_info *info)
{
	struct k3_fb_data_type *k3fd = NULL;
	int ret = 0;
	bool needed = false;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	if (k3fd->set_fastboot_fnc) {
		needed = k3fd->set_fastboot_fnc(info);
	}

	if (!needed) {
		ret = k3_fb_blank_sub(FB_BLANK_UNBLANK, info);
		if (ret != 0) {
			K3_FB_ERR("can't turn on display!\n");
			return ret;
		}
	}

	return 0;
}

static int k3_fb_release_sub(struct fb_info *info)
{
	int ret = 0;

	BUG_ON(info == NULL);

	ret = k3_fb_blank_sub(FB_BLANK_POWERDOWN, info);
	if (ret != 0) {
		K3_FB_ERR("can't turn off display!\n");
		return ret;
	}

	return 0;
}


/*******************************************************************************
**
*/
static int k3_fb_blank(int blank_mode, struct fb_info *info)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	if (k3fd->ovl_type == DSS_OVL_ADP) {
		K3_FB_DEBUG("fb%d, blank_mode(%d) +.\n", k3fd->index, blank_mode);
	} else {
		K3_FB_INFO("fb%d, blank_mode(%d) +.\n", k3fd->index, blank_mode);
	}

#if 0
	if (blank_mode == FB_BLANK_POWERDOWN) {
		struct fb_event event;
		event.info = info;
		event.data = &blank_mode;
		fb_notifier_call_chain(FB_EVENT_BLANK, &event);
	}
#endif

	ret = k3_fb_blank_sub(blank_mode, info);
	if (ret != 0) {
		K3_FB_ERR("fb%d, blank_mode(%d) failed!\n", k3fd->index, blank_mode);
		return ret;
	}

	if (k3fd->ovl_type == DSS_OVL_ADP) {
		K3_FB_DEBUG("fb%d, blank_mode(%d) -.\n", k3fd->index, blank_mode);
	} else {
		K3_FB_INFO("fb%d, blank_mode(%d) -.\n", k3fd->index, blank_mode);
	}

	return 0;
}

static int k3_fb_open(struct fb_info *info, int user)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

//	if (k3fd->pm_runtime_get)
//		k3fd->pm_runtime_get(k3fd);

	if (!k3fd->ref_cnt) {
		K3_FB_DEBUG("fb%d, +!\n", k3fd->index);
		if (k3fd->open_sub_fnc) {
            pr_jank(JL_KERNEL_LCD_OPEN, "%s", "JL_KERNEL_LCD_OPEN");
			ret = k3fd->open_sub_fnc(info);
		}
		K3_FB_DEBUG("fb%d, -!\n", k3fd->index);
	}

	k3fd->ref_cnt++;

	return ret;
}

static int k3_fb_release(struct fb_info *info, int user)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	if (!k3fd->ref_cnt) {
		K3_FB_INFO("try to close unopened fb%d!\n", k3fd->index);
		return -EINVAL;
	}

	k3fd->ref_cnt--;

	if (!k3fd->ref_cnt) {
		K3_FB_DEBUG("fb%d, +.\n", k3fd->index);
		if (k3fd->release_sub_fnc) {
			ret = k3fd->release_sub_fnc(info);
		}
		K3_FB_DEBUG("fb%d, -.\n", k3fd->index);
	}

//	if (k3fd->pm_runtime_put)
//		k3fd->pm_runtime_put(k3fd);

	return ret;
}

static int k3_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	if (var->rotate != FB_ROTATE_UR) {
		K3_FB_DEBUG("error rotate %d!\n", var->rotate);
		return -EINVAL;
	}

	if (var->grayscale != info->var.grayscale) {
		K3_FB_DEBUG("error grayscale %d!\n", var->grayscale);
		return -EINVAL;
	}

	if ((var->xres_virtual <= 0) || (var->yres_virtual <= 0)) {
		K3_FB_ERR("xres_virtual=%d yres_virtual=%d out of range!",
			var->xres_virtual, var->yres_virtual);
		return -EINVAL;
	}

#if 0
	if (info->fix.smem_len <
		(k3fb_line_length(k3fd->index, var->xres_virtual, (var->bits_per_pixel >> 3)) *
		var->yres_virtual)) {
		K3_FB_ERR("fb%d smem_len=%d is out of range!\n", k3fd->index, info->fix.smem_len);
		return -EINVAL;
	}
#endif

	if ((var->xres == 0) || (var->yres == 0)) {
		K3_FB_ERR("xres=%d, yres=%d is invalid!\n", var->xres, var->yres);
		return -EINVAL;
	}

	if (var->xoffset > (var->xres_virtual - var->xres)) {
		K3_FB_ERR("xoffset=%d(xres_virtual=%d, xres=%d) out of range!\n",
			var->xoffset, var->xres_virtual, var->xres);
		return -EINVAL;
	}

	if (var->yoffset > (var->yres_virtual - var->yres)) {
		K3_FB_ERR("yoffset=%d(yres_virtual=%d, yres=%d) out of range!\n",
			var->yoffset, var->yres_virtual, var->yres);
		return -EINVAL;
	}

	return 0;
}

static int k3_fb_set_par(struct fb_info *info)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct fb_var_screeninfo *var = NULL;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	var = &info->var;

	k3fd->fbi->fix.line_length = k3fb_line_length(k3fd->index, var->xres_virtual,
		var->bits_per_pixel >> 3);

	return 0;
}

static int k3_fb_pan_display(struct fb_var_screeninfo *var,
	struct fb_info *info)
{
	struct k3_fb_data_type *k3fd = NULL;

	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	if (!k3fd->panel_power_on)
		return -EPERM;

	if (var->xoffset > (info->var.xres_virtual - info->var.xres))
		return -EINVAL;

	if (var->yoffset > (info->var.yres_virtual - info->var.yres))
		return -EINVAL;

	if (info->fix.xpanstep)
		info->var.xoffset =
		(var->xoffset / info->fix.xpanstep) * info->fix.xpanstep;

	if (info->fix.ypanstep)
		info->var.yoffset =
		(var->yoffset / info->fix.ypanstep) * info->fix.ypanstep;

	if (k3fd->pan_display_fnc)
		k3fd->pan_display_fnc(k3fd);
	else
		K3_FB_ERR("fb%d pan_display_fnc not set!\n", k3fd->index);

	if (k3fd->bl_update) {
		k3fd->bl_update(k3fd);
	}

	return 0;
}

static int k3fb_query_dirty_region_updt(struct fb_info *info, void __user *argp)
{
	int enable = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	if (g_enable_dirty_region_updt
		&& k3fd->panel_info.dirty_region_updt_support
		&& !k3fd->sbl_enable
		&& !k3fd->ov_wb_enabled) {
		enable = 1;
	}

	k3fd->dirty_region_updt_enable = enable;

	if (copy_to_user(argp, &enable, sizeof(enable))) {
		K3_FB_ERR("copy to user fail");
		return -EFAULT;
	}

	return 0;
}

//Multi Resolution====begin
/*added for resolution switch*/
static int k3_fb_switch_resolution_process(struct k3_fb_data_type * k3fd)
{
	struct k3_panel_info * pInfo = NULL;
	struct fb_info *fbi = NULL;
	struct fb_var_screeninfo *var = NULL;
	struct fb_fix_screeninfo *fix = NULL;
	struct k3_fb_panel_data *pdata = NULL;

	if (NULL == k3fd) {
		K3_FB_ERR("k3_fb_switch_resolution_process:NULL pointer!\n");
		return -1;
	}

	fbi = k3fd->fbi;
	fix = &fbi->fix;
	var = &fbi->var;

	pInfo = &k3fd->panel_info;

	pdata = (struct k3_fb_panel_data *)k3fd->pdev->dev.platform_data;
	if ((!pdata) || (!pdata->set_disp_resolution)) {
		K3_FB_ERR("no panel operation detected!\n");
		return 0;
	}
	if (pdata->set_disp_resolution(k3fd->pdev) != 0) {
		K3_FB_ERR("set_disp_resolution error!\n");
		return 0;
	}

	fbi->var.pixclock = k3fd->panel_info.pxl_clk_rate;
	/*fbi->var.pixclock = clk_round_rate(k3fd->dpe_clk, k3fd->panel_info.pxl_clk_rate);*/
	fbi->var.left_margin = k3fd->panel_info.ldi.h_back_porch;
	fbi->var.right_margin = k3fd->panel_info.ldi.h_front_porch;
	fbi->var.upper_margin = k3fd->panel_info.ldi.v_back_porch;
	fbi->var.lower_margin = k3fd->panel_info.ldi.v_front_porch;
	fbi->var.hsync_len = k3fd->panel_info.ldi.h_pulse_width;
	fbi->var.vsync_len = k3fd->panel_info.ldi.v_pulse_width;

	var->height = k3fd->panel_info.height;  /* height of picture in mm */
	var->width = k3fd->panel_info.width;  /* width of picture in mm */
	var->xres = k3fd->panel_info.xres;
	var->yres = k3fd->panel_info.yres;
	var->xres_virtual = k3fd->panel_info.xres;
	var->yres_virtual = k3fd->panel_info.yres * k3fd->fb_num;

	fix->line_length = k3fb_line_length(k3fd->index, var->xres_virtual, var->bits_per_pixel >> 3);
	fix->smem_len = roundup(fix->line_length * var->yres_virtual, PAGE_SIZE);
	if (k3fd->index == PRIMARY_PANEL_IDX) {
		g_primary_lcd_xres = k3fd->panel_info.xres;
		g_primary_lcd_yres = k3fd->panel_info.yres;
	}

	k3fb_free_fb_buffer(k3fd);
	if (fix->smem_len > 0) {
		if (!k3fb_alloc_fb_buffer(k3fd)) {
			K3_FB_ERR("k3fb_alloc_buffer failed!\n");
			return -1;
		}
	}

	return 0;
}

static int k3fb_resolution_switch(struct fb_info *info, unsigned long *argp)
{
	int ret  = 0;
	int flag = 0;
	struct k3_fb_data_type * k3fd = NULL;

	BUG_ON(NULL == info);

	if (copy_from_user(&flag, argp, sizeof(flag))){
		K3_FB_ERR("copy from user failed\n");
		return -1;
	}

	k3fd = (struct k3_fb_data_type*)info->par;

	BUG_ON(NULL == k3fd);

	if (flag >= DISPLAY_LOW_POWER_LEVEL_MAX ) {
		K3_FB_ERR("Invalid level of Multi Resolution\n");
		return -1;
	}

	if (k3fd->index != 0){
		K3_FB_ERR("Invalid FB device\n");
		return -1;
	}

	if (k3fd->switch_res_flag == flag) {
		return 1;
	}
	//printk("set : %d bl_level: %d bl_update: %d old:%d \n", flag, k3fd->bl_level, k3fd->backlight.bl_updated, k3fd->backlight.bl_level_old);
	k3fd->switch_res_flag = flag;
	ret = k3_fb_blank_sub(FB_BLANK_POWERDOWN, info);
	if (ret != 0) {
		K3_FB_ERR("can't turn off display!\n");
		return -1;
	}

	ret = k3_fb_switch_resolution_process(k3fd);

	return ret;
}
#define FORCE_BACKLIGHT_VALUE 82
static int k3fb_resolution_update(struct fb_info *info, unsigned long *argp)
{
	int flag = 0;
	int ret = 0;
	struct k3_fb_data_type * k3fd;
	BUG_ON(NULL == info);
	if (copy_from_user(&flag, argp, sizeof(flag))){
		K3_FB_ERR("copy from user failed\n");
		return -EFAULT;
	}
	k3fd = (struct k3_fb_data_type*)info->par;
	BUG_ON(k3fd == NULL);
	//printk("update: %d bl_level: %d bl_update: %d old:%d \n", flag, k3fd->bl_level, k3fd->backlight.bl_updated, k3fd->backlight.bl_level_old);
	if (k3fd->switch_res_flag == flag) {
		ret = k3_fb_blank_sub(FB_BLANK_UNBLANK, info);
		k3fd->bl_level = FORCE_BACKLIGHT_VALUE;
		k3fd->backlight.bl_updated = 0;
		if (k3fd->bl_update) {
			k3fd->bl_update(k3fd);
		}
	}
	//printk("update finish: %d bl_level: %d bl_update: %d old:%d \n", flag, k3fd->bl_level, k3fd->backlight.bl_updated, k3fd->backlight.bl_level_old);
	if (ret != 0) {
		K3_FB_ERR("can't turn on display!\n");
	}
	return ret;
}
//Multi Resolution====end

static int k3_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	int ret = -ENOSYS;
	struct k3_fb_data_type *k3fd = NULL;
	void __user *argp = (void __user *)arg;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	switch (cmd) {
	case K3FB_VSYNC_CTRL:
		if (k3fd->vsync_ctrl_fnc) {
			ret = k3fd->vsync_ctrl_fnc(info, argp);
		}
		break;

	case K3FB_SBL_CTRL:
		if (k3fd->sbl_ctrl_fnc) {
			ret = k3fd->sbl_ctrl_fnc(info, argp);
		}
		break;

	case K3FB_DSS_CLK_RATE_SET:
		ret = k3fb_ctrl_dss_clk_rate(info, argp);
		break;

	case K3FB_TIMING_SET:
		down(&k3fd->blank_sem);
		ret = k3fb_set_timing(info, argp);
		up(&k3fd->blank_sem);
		break;

	case K3FB_QUERY_UPDT_STATE:
		ret = k3fb_query_dirty_region_updt(info, argp);
		break;
	case K3FB_CHECK_MIPI_TR:
	#ifdef CONFIG_LCD_CHECK_REG
		ret = k3_fb_check_lcd_reg(info, argp);
	#endif
		break;
	case K3FB_MIPI_DETECT:
	#ifdef CONFIG_LCD_MIPI_DETECT
		ret = k3_fb_mipi_detect(info, argp);
	#endif
		break;

	//Multi Resolution====begin
	case K3FB_RESOLUTION_SWITCH:
		ret = k3fb_resolution_switch(info, argp);
		break;
	case K3FB_RESOLUTION_UPDATE:
		ret = k3fb_resolution_update(info, argp);
		break;
	//Multi Resolution====end

	default:
		if (k3fd->ov_ioctl_handler)
			ret = k3fd->ov_ioctl_handler(k3fd, cmd, argp);
		break;
	}

	if (ret == -ENOSYS)
		K3_FB_ERR("unsupported ioctl (%x)\n", cmd);

	return ret;
}

static int k3_fb_mmap(struct fb_info *info, struct vm_area_struct * vma)
{
#ifndef CONFIG_K3_FB_HEAP_CARVEOUT_USED
	struct k3_fb_data_type *k3fd = NULL;
	struct sg_table *table = NULL;
	struct scatterlist *sg = NULL;
	struct page *page = NULL;
	unsigned long remainder = 0;
	unsigned long len = 0;
	unsigned long addr = 0;
	unsigned long offset = 0;
	int i = 0;
	int ret = 0;

	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);
	table = ion_sg_table(k3fd->ion_client, k3fd->ion_handle);
	BUG_ON(table == NULL);

	addr = vma->vm_start;
	offset = vma->vm_pgoff * PAGE_SIZE;

	for_each_sg(table->sgl, sg, table->nents, i) {
		page = sg_page(sg);
		remainder = vma->vm_end - addr;
		len = sg_dma_len(sg);

		if (offset >= sg_dma_len(sg)) {
			offset -= sg_dma_len(sg);
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg_dma_len(sg) - offset;
			offset = 0;
		}
		len = min(len, remainder);
		ret = remap_pfn_range(vma, addr, page_to_pfn(page), len,
			vma->vm_page_prot);
		if (ret != 0) {
			K3_FB_ERR("fb%d, failed to remap_pfn_range! ret=%d\n", k3fd->index, ret);
		}

		addr += len;
		if (addr >= vma->vm_end)
			return 0;
	}
#endif

	return 0;
}

static unsigned long k3fb_alloc_fb_buffer(struct k3_fb_data_type *k3fd)
{
	struct fb_info *fbi = NULL;
	struct ion_client *client = NULL;
	struct ion_handle *handle = NULL;
	size_t buf_len = 0;
	unsigned long buf_addr = 0;

	BUG_ON(k3fd == NULL);
	fbi = k3fd->fbi;
	BUG_ON(fbi == NULL);

	client = k3fd->ion_client;
	if (IS_ERR_OR_NULL(client)) {
		K3_FB_ERR("failed to create ion client!\n");
		goto err_return;
	}

	buf_len = fbi->fix.smem_len;

#ifdef CONFIG_K3_FB_HEAP_CARVEOUT_USED
	handle = ion_alloc(client, buf_len, PAGE_SIZE, ION_HEAP(ION_GRALLOC_HEAP_ID), 0);
#else
	handle = ion_alloc(client, buf_len, PAGE_SIZE, ION_HEAP(ION_SYSTEM_HEAP_ID), 0);
#endif
	if (IS_ERR_OR_NULL(handle)) {
		K3_FB_ERR("failed to ion_alloc!\n");
		goto err_return;
	}
#if 0
	fbi->screen_base = ion_map_kernel(client, handle);
	if (!fbi->screen_base) {
		K3_FB_ERR("failed to ion_map_kernel!\n");
		goto err_ion_map;
	}
#endif
#ifdef CONFIG_K3_FB_HEAP_CARVEOUT_USED
	if (ion_phys(client, handle, &buf_addr, &buf_len) < 0) {
		K3_FB_ERR("failed to get ion phys!\n");
		goto err_ion_get_addr;
	}
#else
	if (ion_map_iommu(client, handle, &(k3fd->iommu_format))) {
		K3_FB_ERR("failed to ion_map_iommu!\n");
		//goto err_ion_get_addr;
		goto err_ion_map;
	}

	buf_addr = k3fd->iommu_format.iova_start;
#endif

	fbi->fix.smem_start = buf_addr;
	fbi->screen_size = fbi->fix.smem_len;
	//memset(fbi->screen_base, 0x0, fbi->screen_size);

	k3fd->ion_handle = handle;

	return buf_addr;
#if 0
err_ion_get_addr:
	ion_unmap_kernel(k3fd->ion_client, k3fd->ion_handle);
#endif
err_ion_map:
	ion_free(k3fd->ion_client, k3fd->ion_handle);
err_return:
	return 0;
}

static void k3fb_free_fb_buffer(struct k3_fb_data_type *k3fd)
{
	if (k3fd->ion_client != NULL &&
		k3fd->ion_handle != NULL) {
	#ifndef CONFIG_K3_FB_HEAP_CARVEOUT_USED
		ion_unmap_iommu(k3fd->ion_client, k3fd->ion_handle);
	#endif
		//ion_unmap_kernel(k3fd->ion_client, k3fd->ion_handle);
		ion_free(k3fd->ion_client, k3fd->ion_handle);
		k3fd->ion_handle = NULL;
	}
}


/*******************************************************************************
**
*/
static struct fb_ops k3_fb_ops = {
	.owner = THIS_MODULE,
	.fb_open = k3_fb_open,
	.fb_release = k3_fb_release,
	.fb_read = NULL,
	.fb_write = NULL,
	.fb_cursor = NULL,
	.fb_check_var = k3_fb_check_var,
	.fb_set_par = k3_fb_set_par,
	.fb_setcolreg = NULL,
	.fb_blank = k3_fb_blank,
	.fb_pan_display = k3_fb_pan_display,
	.fb_fillrect = NULL,
	.fb_copyarea = NULL,
	.fb_imageblit = NULL,
	.fb_rotate = NULL,
	.fb_sync = NULL,
	.fb_ioctl = k3_fb_ioctl,
	.fb_mmap = k3_fb_mmap,
};

static int k3_fb_register(struct k3_fb_data_type *k3fd)
{
	int bpp = 0;
	struct k3_panel_info *panel_info = NULL;
	struct fb_info *fbi = NULL;
	struct fb_fix_screeninfo *fix = NULL;
	struct fb_var_screeninfo *var = NULL;

	BUG_ON(k3fd == NULL);
	panel_info = &k3fd->panel_info;
	BUG_ON(panel_info == NULL);

	/*
	 * fb info initialization
	 */
	fbi = k3fd->fbi;
	fix = &fbi->fix;
	var = &fbi->var;

	fix->type_aux = 0;
	fix->visual = FB_VISUAL_TRUECOLOR;
	fix->ywrapstep = 0;
	fix->mmio_start = 0;
	fix->mmio_len = 0;
	fix->accel = FB_ACCEL_NONE;

	var->xoffset = 0;
	var->yoffset = 0;
	var->grayscale = 0;
	var->nonstd = 0;
	var->activate = FB_ACTIVATE_VBL;
	var->height = panel_info->height;
	var->width = panel_info->width;
	var->accel_flags = 0;
	var->sync = 0;
	var->rotate = 0;

	switch (k3fd->fb_imgType) {
	case K3_FB_PIXEL_FORMAT_BGR_565:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;

		var->blue.offset = 0;
		var->green.offset = 5;
		var->red.offset = 11;
		var->transp.offset = 0;

		var->blue.length = 5;
		var->green.length = 6;
		var->red.length = 5;
		var->transp.length = 0;

		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;
		bpp = 2;
		break;

	case K3_FB_PIXEL_FORMAT_BGRX_4444:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;

		var->blue.offset = 0;
		var->green.offset = 4;
		var->red.offset = 8;
		var->transp.offset = 0;

		var->blue.length = 4;
		var->green.length = 4;
		var->red.length = 4;
		var->transp.length = 0;

		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;
		bpp = 2;
		break;

	case K3_FB_PIXEL_FORMAT_BGRA_4444:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;

		var->blue.offset = 0;
		var->green.offset = 4;
		var->red.offset = 8;
		var->transp.offset = 12;

		var->blue.length = 4;
		var->green.length = 4;
		var->red.length = 4;
		var->transp.length = 4;

		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;
		bpp = 2;
		break;

	case K3_FB_PIXEL_FORMAT_BGRX_5551:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;

		var->blue.offset = 0;
		var->green.offset = 5;
		var->red.offset = 10;
		var->transp.offset = 0;

		var->blue.length = 5;
		var->green.length = 5;
		var->red.length = 5;
		var->transp.length = 0;

		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;
		bpp = 2;
		break;

	case K3_FB_PIXEL_FORMAT_BGRA_5551:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;

		var->blue.offset = 0;
		var->green.offset = 5;
		var->red.offset = 10;
		var->transp.offset = 15;

		var->blue.length = 5;
		var->green.length = 5;
		var->red.length = 5;
		var->transp.length = 1;

		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;
		bpp = 2;
		break;

	case K3_FB_PIXEL_FORMAT_BGRX_8888:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;

		var->blue.offset = 0;
		var->green.offset = 8;
		var->red.offset = 16;
		var->transp.offset = 0;

		var->blue.length = 8;
		var->green.length = 8;
		var->red.length = 8;
		var->transp.length = 0;

		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;

		bpp = 3;
		break;

	case K3_FB_PIXEL_FORMAT_BGRA_8888:
		fix->type = FB_TYPE_PACKED_PIXELS;
		fix->xpanstep = 1;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;

		var->blue.offset = 0;
		var->green.offset = 8;
		var->red.offset = 16;
		var->transp.offset = 24;

		var->blue.length = 8;
		var->green.length = 8;
		var->red.length = 8;
		var->transp.length = 8;

		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;

		bpp = 4;
		break;

	case K3_FB_PIXEL_FORMAT_YUV_422_I:
		fix->type = FB_TYPE_INTERLEAVED_PLANES;
		fix->xpanstep = 2;
		fix->ypanstep = 1;
		var->vmode = FB_VMODE_NONINTERLACED;

		/* FIXME: R/G/B offset? */
		var->blue.offset = 0;
		var->green.offset = 5;
		var->red.offset = 11;
		var->transp.offset = 0;

		var->blue.length = 5;
		var->green.length = 6;
		var->red.length = 5;
		var->transp.length = 0;

		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;

		bpp = 2;
		break;

	default:
		K3_FB_ERR("fb%d, unkown image type!\n", k3fd->index);
		return -EINVAL;
	}

	var->xres = panel_info->xres;
	var->yres = panel_info->yres;
	var->xres_virtual = panel_info->xres;
	var->yres_virtual = panel_info->yres * k3fd->fb_num;
	var->bits_per_pixel = bpp * 8;

	snprintf(fix->id, sizeof(fix->id), "k3fb%d", k3fd->index);
	fix->line_length = k3fb_line_length(k3fd->index, var->xres_virtual, bpp);
	fix->smem_len = roundup(fix->line_length * var->yres_virtual, PAGE_SIZE);
	fix->smem_start = 0;

	fbi->screen_base = 0;
	fbi->fbops = &k3_fb_ops;
	fbi->flags = FBINFO_FLAG_DEFAULT;
	fbi->pseudo_palette = NULL;

	k3fd->ion_client = hisi_ion_client_create(K3_FB_ION_CLIENT_NAME);
	if (IS_ERR_OR_NULL(k3fd->ion_client)) {
		K3_FB_ERR("failed to create ion client!\n");
		return -ENOMEM;
	}
	k3fd->ion_handle = NULL;
	memset(&k3fd->iommu_format, 0, sizeof(struct iommu_map_format));

	if (fix->smem_len > 0) {
		if (!k3fb_alloc_fb_buffer(k3fd)) {
			K3_FB_ERR("k3fb_alloc_buffer failed!\n");
			return -ENOMEM;
		}
	}

	k3fd->ref_cnt = 0;
	k3fd->panel_power_on = false;
	sema_init(&k3fd->blank_sem, 1);

	memset(&k3fd->dss_clk_rate, 0, sizeof(struct dss_clk_rate));

	k3fd->on_fnc = k3fb_ctrl_on;
	k3fd->off_fnc = k3fb_ctrl_off;

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		g_primary_lcd_xres = panel_info->xres;
		g_primary_lcd_yres = panel_info->yres;

	#ifdef CONFIG_FASTBOOT_DISP_ENABLE
		fastboot_set_needed = 1;
		k3fd->set_fastboot_fnc = k3_fb_set_fastboot_needed;
	#else
		k3fd->set_fastboot_fnc = NULL;
	#endif
		k3fd->open_sub_fnc = k3_fb_open_sub;
		k3fd->release_sub_fnc = k3_fb_release_sub;
		k3fd->frc_fnc = k3fb_ctrl_frc;
		k3fd->esd_fnc = k3fb_ctrl_esd;
		k3fd->sbl_ctrl_fnc = k3fb_ctrl_sbl;

		k3fd->pm_runtime_register = k3fb_pm_runtime_register;
		k3fd->pm_runtime_unregister = k3fb_pm_runtime_unregister;
		k3fd->pm_runtime_get = k3fb_pm_runtime_get;
		k3fd->pm_runtime_put = k3fb_pm_runtime_put;
		k3fd->bl_register = k3fb_backlight_register;
		k3fd->bl_unregister = k3fb_backlight_unregister;
		k3fd->bl_update = k3fb_backlight_update;
		k3fd->bl_cancel = k3fb_backlight_cancel;
		k3fd->vsync_register = k3fb_vsync_register;
		k3fd->vsync_unregister = k3fb_vsync_unregister;
		k3fd->vsync_ctrl_fnc = k3fb_vsync_ctrl;
		k3fd->vsync_isr_handler = k3fb_vsync_isr_handler;
		k3fd->buf_sync_register = k3fb_buf_sync_register;
		k3fd->buf_sync_unregister = k3fb_buf_sync_unregister;
		k3fd->buf_sync_signal = k3fb_buf_sync_signal;
		k3fd->buf_sync_suspend = k3fb_buf_sync_suspend;
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		k3fd->set_fastboot_fnc = NULL;
		k3fd->open_sub_fnc = NULL;
		k3fd->release_sub_fnc = NULL;
		k3fd->frc_fnc = NULL;
		k3fd->esd_fnc = NULL;
		k3fd->sbl_ctrl_fnc = NULL;

		k3fd->pm_runtime_register = k3fb_pm_runtime_register;
		k3fd->pm_runtime_unregister = k3fb_pm_runtime_unregister;
		k3fd->pm_runtime_get = k3fb_pm_runtime_get;
		k3fd->pm_runtime_put = k3fb_pm_runtime_put;
		k3fd->bl_register = k3fb_backlight_register;
		k3fd->bl_unregister = k3fb_backlight_unregister;
		k3fd->bl_update = k3fb_backlight_update;
		k3fd->bl_cancel = k3fb_backlight_cancel;
		k3fd->vsync_register = k3fb_vsync_register;
		k3fd->vsync_unregister = k3fb_vsync_unregister;
		k3fd->vsync_ctrl_fnc = k3fb_vsync_ctrl;
		k3fd->vsync_isr_handler = k3fb_vsync_isr_handler;
		k3fd->buf_sync_register = k3fb_buf_sync_register;
		k3fd->buf_sync_unregister = k3fb_buf_sync_unregister;
		k3fd->buf_sync_signal = k3fb_buf_sync_signal;
		k3fd->buf_sync_suspend = k3fb_buf_sync_suspend;
	} else {
		k3fd->set_fastboot_fnc = NULL;
		k3fd->open_sub_fnc = NULL;
		k3fd->release_sub_fnc = NULL;
		k3fd->frc_fnc = NULL;
		k3fd->esd_fnc = NULL;
		k3fd->sbl_ctrl_fnc = NULL;

		k3fd->pm_runtime_register = NULL;
		k3fd->pm_runtime_unregister = NULL;
		k3fd->pm_runtime_get = NULL;
		k3fd->pm_runtime_put = NULL;
		k3fd->bl_register = NULL;
		k3fd->bl_unregister = NULL;
		k3fd->bl_update = NULL;
		k3fd->bl_cancel = NULL;
		k3fd->vsync_register = NULL;
		k3fd->vsync_unregister = NULL;
		k3fd->vsync_ctrl_fnc = NULL;
		k3fd->vsync_isr_handler = NULL;
		k3fd->buf_sync_register = NULL;
		k3fd->buf_sync_unregister = NULL;
		k3fd->buf_sync_signal = NULL;
		k3fd->buf_sync_suspend = NULL;
	}

	if (k3_overlay_init(k3fd)) {
		K3_FB_ERR("unable to init overlay!\n");
		return -EPERM;
	}

	if (register_framebuffer(fbi) < 0) {
		K3_FB_ERR("fb%d failed to register_framebuffer!", k3fd->index);
		return -EPERM;
	}

	/* backlight register */
	if (k3fd->bl_register)
		k3fd->bl_register(k3fd->pdev);
	/* vsync register */
	if (k3fd->vsync_register)
		k3fd->vsync_register(k3fd->pdev);
	/* buf_sync register */
	if (k3fd->buf_sync_register)
		k3fd->buf_sync_register(k3fd->pdev);
	/* pm runtime register */
	if (k3fd->pm_runtime_register)
		k3fd->pm_runtime_register(k3fd->pdev);

	K3_FB_INFO("FrameBuffer[%d] %dx%d size=%d bytes phy_addr=0x%x virt_addr=0x%x "
		"is registered successfully!\n",
		k3fd->index, var->xres, var->yres, fbi->fix.smem_len,
		(u32)fix->smem_start, (u32)fbi->screen_base);

	return 0;
}


/*******************************************************************************
**
*/
static int k3_fb_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	struct device_node *np = NULL;

	if (!k3_fb_resource_initialized) {
		K3_FB_DEBUG("initialized=%d, +.\n", k3_fb_resource_initialized);

		pdev->id = 0;

		np = of_find_compatible_node(NULL, NULL, DTS_COMP_FB_NAME);
		if (!np) {
			K3_FB_ERR("NOT FOUND device node %s!\n", DTS_COMP_FB_NAME);
			return -ENXIO;
		}

		/* get irq no */
		k3fd_irq_pdp = irq_of_parse_and_map(np, 0);
		if (!k3fd_irq_pdp) {
			K3_FB_ERR("failed to get k3fd_irq_pdp resource.\n");
			return -ENXIO;
		}
		k3fd_irq_sdp = irq_of_parse_and_map(np, 1);
		if (!k3fd_irq_sdp) {
			K3_FB_ERR("failed to get k3fd_irq_sdp resource.\n");
			return -ENXIO;
		}

		k3fd_irq_adp = irq_of_parse_and_map(np, 2);
		if (!k3fd_irq_sdp) {
			K3_FB_ERR("failed to get k3fd_irq_sdp resource.\n");
			return -ENXIO;
		}

		k3fd_irq_dsi0 = irq_of_parse_and_map(np, 3);
		if (!k3fd_irq_dsi0) {
			K3_FB_ERR("failed to get k3fd_irq_dsi0 resource.\n");
			return -ENXIO;
		}

		k3fd_irq_dsi1 = irq_of_parse_and_map(np, 4);
		if (!k3fd_irq_dsi1) {
			K3_FB_ERR("failed to get k3fd_irq_dsi1 resource.\n");
			return -ENXIO;
		}

		/* get dss reg base */
		k3fd_dss_base = of_iomap(np, 0);
		if (!k3fd_dss_base) {
			K3_FB_ERR("failed to get k3fd_dss_base resource.\n");
			return -ENXIO;
		}
		k3fd_crgperi_base = of_iomap(np, 1);
		if (!k3fd_crgperi_base) {
			K3_FB_ERR("failed to get k3fd_crgperi_base resource.\n");
			return -ENXIO;
		}

		/* get regulator resource */
		g_dpe_regulator[0].supply = REGULATOR_PDP_NAME;
		g_dpe_regulator[1].supply = REGULATOR_SDP_NAME;
		g_dpe_regulator[2].supply = REGULATOR_ADP_NAME;
		ret = devm_regulator_bulk_get(&(pdev->dev),
			ARRAY_SIZE(g_dpe_regulator), g_dpe_regulator);
		if (ret) {
			K3_FB_ERR("failed to get regulator resource! ret=%d.\n", ret);
			return -ENXIO;
		}

		/* get dss clk resource */
		ret = of_property_read_string_index(np, "clock-names", 0, &g_dss_axi_clk_name);
		if (ret != 0) {
			K3_FB_ERR("failed to get axi_clk resource! ret=%d.\n", ret);
			return -ENXIO;
		}
		ret = of_property_read_string_index(np, "clock-names", 1, &g_dss_pri_clk_name);
		if (ret != 0) {
			K3_FB_ERR("failed to get pri_clk resource! ret=%d.\n", ret);
			return -ENXIO;
		}
		ret = of_property_read_string_index(np, "clock-names", 2, &g_dss_aux_clk_name);
		if (ret != 0) {
			K3_FB_ERR("failed to get aux_clk resource! ret=%d.\n", ret);
			return -ENXIO;
		}
		ret = of_property_read_string_index(np, "clock-names", 3, &g_dss_pxl0_clk_name);
		if (ret != 0) {
			K3_FB_ERR("failed to get pxl0_clk resource! ret=%d.\n", ret);
			return -ENXIO;
		}
		ret = of_property_read_string_index(np, "clock-names", 4, &g_dss_pxl1_clk_name);
		if (ret != 0) {
			K3_FB_ERR("failed to get pxl1_clk resource! ret=%d.\n", ret);
			return -ENXIO;
		}
		ret = of_property_read_string_index(np, "clock-names", 5, &g_dss_pclk_clk_name);
		if (ret != 0) {
			K3_FB_ERR("failed to get pclk_clk resource! ret=%d.\n", ret);
			return -ENXIO;
		}
		ret = of_property_read_string_index(np, "clock-names", 6, &g_dss_dphy0_clk_name);
				if (ret != 0) {
			K3_FB_ERR("failed to get dphy0_clk resource! ret=%d.\n", ret);
			return -ENXIO;
		}
		ret = of_property_read_string_index(np, "clock-names", 7, &g_dss_dphy1_clk_name);
				if (ret != 0) {
			K3_FB_ERR("failed to get dphy1_clk resource! ret=%d.\n", ret);
			return -ENXIO;
		}

		k3_fb_resource_initialized = 1;

		k3_fb_device_set_status0(DTS_FB_RESOURCE_INIT_READY);

		K3_FB_DEBUG("initialized=%d, -.\n", k3_fb_resource_initialized);
		return 0;
	}

	if (pdev->id < 0) {
		K3_FB_ERR("WARNING: id=%d, name=%s!\n", pdev->id, pdev->name);
		return 0;
	}

	if (!k3_fb_resource_initialized) {
		K3_FB_ERR("fb resource not initialized!\n");
		return -EPERM;
	}

	if (pdev_list_cnt >= K3_FB_MAX_DEV_LIST) {
		K3_FB_ERR("too many fb devices, num=%d!\n", pdev_list_cnt);
		return -ENOMEM;
	}

	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	ret = k3_fb_register(k3fd);
	if (ret) {
		K3_FB_ERR("fb%d k3_fb_register failed, error=%d!\n", k3fd->index, ret);
		return ret;
	}

	/* config earlysuspend */
#ifdef CONFIG_HAS_EARLYSUSPEND
	k3fd->early_suspend.suspend = k3fb_early_suspend;
	k3fd->early_suspend.resume = k3fb_early_resume;
	k3fd->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 2;
	register_early_suspend(&k3fd->early_suspend);
#endif

	pdev_list[pdev_list_cnt++] = pdev;

	/* set device probe status */
	k3_fb_device_set_status1(k3fd);

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	if(!lcd_dclient) {
		lcd_dclient = dsm_register_client(&dsm_lcd);
	}

	return 0;
}

static int k3_fb_remove(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	/* pm_runtime unregister */
	if (k3fd->pm_runtime_unregister)
		k3fd->pm_runtime_unregister(pdev);

	/* stop the device */
	if (k3_fb_suspend_sub(k3fd) != 0)
		K3_FB_ERR("fb%d k3_fb_suspend_sub failed!\n", k3fd->index);

	/* overlay destroy */
	k3_overlay_deinit(k3fd);

	/* free framebuffer */
	k3fb_free_fb_buffer(k3fd);
	if (k3fd->ion_client) {
		ion_client_destroy(k3fd->ion_client);
		k3fd->ion_client = NULL;
	}

	/* remove /dev/fb* */
	unregister_framebuffer(k3fd->fbi);

	/* unregister buf_sync */
	if (k3fd->buf_sync_unregister)
		k3fd->buf_sync_unregister(pdev);
	/* unregister vsync */
	if (k3fd->vsync_unregister)
		k3fd->vsync_unregister(pdev);
	/* unregister backlight */
	if (k3fd->bl_unregister)
		k3fd->bl_unregister(pdev);

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return 0;
}

static int k3_fb_suspend_sub(struct k3_fb_data_type *k3fd)
{
	int ret = 0;

	BUG_ON(k3fd == NULL);

	ret = k3_fb_blank_sub(FB_BLANK_POWERDOWN, k3fd->fbi);
	if (ret) {
		K3_FB_ERR("fb%d can't turn off display, error=%d!\n", k3fd->index, ret);
		return ret;
	}

	return 0;
}

static int k3_fb_resume_sub(struct k3_fb_data_type *k3fd)
{
	int ret = 0;

	BUG_ON(k3fd == NULL);

	ret = k3_fb_blank_sub(FB_BLANK_UNBLANK, k3fd->fbi);
	if (ret) {
		K3_FB_ERR("fb%d can't turn on display, error=%d!\n", k3fd->index, ret);
	}

	return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void k3fb_early_suspend(struct early_suspend *h)
{
	struct k3_fb_data_type *k3fd = NULL;

	k3fd = container_of(h, struct k3_fb_data_type, early_suspend);
	BUG_ON(k3fd == NULL);

	K3_FB_INFO("fb%d, +.\n", k3fd->index);

	k3_fb_suspend_sub(k3fd);

	K3_FB_INFO("fb%d, -.\n", k3fd->index);
}

static void k3fb_early_resume(struct early_suspend *h)
{
	struct k3_fb_data_type *k3fd = NULL;

	k3fd = container_of(h, struct k3_fb_data_type, early_suspend);
	BUG_ON(k3fd == NULL);

	K3_FB_INFO("fb%d, +.\n", k3fd->index);

	k3_fb_resume_sub(k3fd);

	K3_FB_INFO("fb%d, -.\n", k3fd->index);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static int k3_fb_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_INFO("fb%d, +.\n", k3fd->index);

	console_lock();
	fb_set_suspend(k3fd->fbi, FBINFO_STATE_SUSPENDED);
	ret = k3_fb_suspend_sub(k3fd);
	if (ret != 0) {
		K3_FB_ERR("fb%d k3_fb_suspend_sub failed, error=%d!\n", k3fd->index, ret);
		fb_set_suspend(k3fd->fbi, FBINFO_STATE_RUNNING);
	} else {
		pdev->dev.power.power_state = state;
	}
	console_unlock();

	K3_FB_INFO("fb%d, -.\n", k3fd->index);

	return ret;
}

static int k3_fb_resume(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_INFO("fb%d, +.\n", k3fd->index);

	console_lock();
	ret = k3_fb_resume_sub(k3fd);
	pdev->dev.power.power_state = PMSG_ON;
	fb_set_suspend(k3fd->fbi, FBINFO_STATE_RUNNING);
	console_unlock();

	K3_FB_INFO("fb%d, -.\n", k3fd->index);

	return ret;
}
#else
#define k3_fb_suspend NULL
#define k3_fb_resume NULL
#endif


/*******************************************************************************
** pm_runtime
*/
static int k3_fb_runtime_suspend(struct device *dev)
{
	struct k3_fb_data_type *k3fd = NULL;
	int ret = 0;

	k3fd = dev_get_drvdata(dev);
	BUG_ON(k3fd == NULL);

	K3_FB_INFO("fb%d, +.\n", k3fd->index);

	ret = k3_fb_suspend_sub(k3fd);
	if (ret != 0) {
		K3_FB_ERR("fb%d, failed to k3_fb_suspend_sub! ret=%d\n", k3fd->index, ret);
	}

	K3_FB_INFO("fb%d, -.\n", k3fd->index);

	return 0;
}

static int k3_fb_runtime_resume(struct device *dev)
{
	struct k3_fb_data_type *k3fd = NULL;
	int ret = 0;

	k3fd = dev_get_drvdata(dev);
	BUG_ON(k3fd == NULL);

	K3_FB_INFO("fb%d, +.\n", k3fd->index);

	ret = k3_fb_resume_sub(k3fd);
	if (ret != 0) {
		K3_FB_ERR("fb%d, failed to k3_fb_resume_sub! ret=%d\n", k3fd->index, ret);
	}

	K3_FB_INFO("fb%d, -.\n", k3fd->index);

	return 0;
}

static int k3_fb_runtime_idle(struct device *dev)
{
	struct k3_fb_data_type *k3fd = NULL;

	k3fd = dev_get_drvdata(dev);
	BUG_ON(k3fd == NULL);

	K3_FB_INFO("fb%d, +.\n", k3fd->index);

	K3_FB_INFO("fb%d, -.\n", k3fd->index);

	return 0;
}

static void k3fb_pm_runtime_get(struct k3_fb_data_type *k3fd)
{
	int ret = 0;

	BUG_ON(k3fd == NULL);

	ret = pm_runtime_get_sync(k3fd->fbi->dev);
	if (ret < 0) {
		K3_FB_ERR("fb%d, failed to pm_runtime_get_sync! ret=%d.", k3fd->index, ret);
	}
}

static void k3fb_pm_runtime_put(struct k3_fb_data_type *k3fd)
{
	int ret = 0;

	BUG_ON(k3fd == NULL);

	ret = pm_runtime_put(k3fd->fbi->dev);
	if (ret < 0) {
		K3_FB_ERR("fb%d, failed to pm_runtime_put! ret=%d.", k3fd->index, ret);
	}
}

static void k3fb_pm_runtime_register(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	ret = pm_runtime_set_active(k3fd->fbi->dev);
	if (ret < 0)
		K3_FB_ERR("fb%d failed to pm_runtime_set_active.\n", k3fd->index);
	pm_runtime_enable(k3fd->fbi->dev);
}

static void k3fb_pm_runtime_unregister(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	pm_runtime_disable(k3fd->fbi->dev);
}


/*******************************************************************************
**
*/
static struct dev_pm_ops k3_fb_dev_pm_ops = {
	.runtime_suspend = k3_fb_runtime_suspend,
	.runtime_resume = k3_fb_runtime_resume,
	.runtime_idle = k3_fb_runtime_idle,
	.suspend = NULL,
	.resume = NULL,
};

static const struct of_device_id k3_fb_match_table[] = {
	{
		.compatible = DTS_COMP_FB_NAME,
		.data = NULL,
	},
};
MODULE_DEVICE_TABLE(of, k3_fb_match_table);

static struct platform_driver k3_fb_driver = {
	.probe = k3_fb_probe,
	.remove = k3_fb_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = k3_fb_suspend,
	.resume = k3_fb_resume,
#endif
	.shutdown = NULL,
	.driver = {
		.name = DEV_NAME_FB,
		.of_match_table = of_match_ptr(k3_fb_match_table),
		.pm = &k3_fb_dev_pm_ops,
	},
};

static int __init k3_fb_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&k3_fb_driver);
	if (ret) {
		K3_FB_ERR("platform_driver_register failed, error=%d!\n", ret);
		return ret;
	}

	return ret;
}

module_init(k3_fb_init);
