/* Copyright (c) 2013-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*
*/

#include "k3_fb.h"
#include "k3_overlay_utils.h"


#define DTS_COMP_K3_OFFLINECOMPOSER_PANEL	"hisilicon,k3offlinecomposer"


/*******************************************************************************
**
*/
static int k3_offlinecompser_panel_on(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	int i = 0;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("index=%d, enter!\n", k3fd->index);

	for (i = 0; i < K3_DSS_OFFLINE_MAX_NUM; i++) {
		char __iomem *ctl_base = k3fd->dss_base + DSS_GLB_WBE1_CH0_CTL +
				i * (DSS_GLB_WBE1_CH1_CTL - DSS_GLB_WBE1_CH0_CTL);

		cmdlist_config_start(k3fd, i);
		k3fd->offline_wb_status[i] = e_status_wait;
		set_reg(ctl_base, 0x1, 1, 8);
		set_reg(ctl_base, 0x0, 1, 8);
	}

	K3_FB_DEBUG("index=%d, exit!\n", k3fd->index);

	return ret;
}

static int k3_offlinecompser_panel_off(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	int i = 0;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("index=%d, enter!\n", k3fd->index);

	for (i = 0; i < K3_DSS_OFFLINE_MAX_NUM; i++) {
		char __iomem *ctl_base = k3fd->dss_base + DSS_GLB_WBE1_CH0_CTL +
				i * (DSS_GLB_WBE1_CH1_CTL - DSS_GLB_WBE1_CH0_CTL);

		cmdlist_config_stop(k3fd, i);
		k3fd->offline_wb_status[i] = e_status_idle;
		set_reg(ctl_base, 0x1, 1, 8);
		set_reg(ctl_base, 0x0, 1, 8);
	}

	K3_FB_DEBUG("index=%d, exit!\n", k3fd->index);

	return ret;
}

static int k3_offlinecompser_panel_remove(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	/*BUG_ON(k3fd == NULL);*/

	if (!k3fd) {
		return 0;
	}

	K3_FB_DEBUG("index=%d, enter!\n", k3fd->index);

	for (i = 0; i < K3_DSS_OFFLINE_MAX_BLOCK; i++) {
		kfree(k3fd->block_rects[i]);
		k3fd->block_rects[i] = NULL;
	}

	if (k3fd->block_overlay) {
		kfree(k3fd->block_overlay);
		k3fd->block_overlay = NULL;
	}

	for(i = 0; i < K3_DSS_OFFLINE_MAX_LIST; i++){
		cmdlist_free_one_list(k3fd->cmdlist_node[i], k3fd->ion_client);
	}

	K3_FB_DEBUG("index=%d, exit!\n", k3fd->index);

	return ret;
}

static struct k3_panel_info k3_offlinecompser_panel_info = {0};
static struct k3_fb_panel_data k3_offlinecomposer_panel_data = {
	.panel_info = &k3_offlinecompser_panel_info,
	.set_fastboot = NULL,
	.on = k3_offlinecompser_panel_on,
	.off = k3_offlinecompser_panel_off,
	.remove = k3_offlinecompser_panel_remove,
	.set_backlight = NULL,
	.sbl_ctrl = NULL,
	.vsync_ctrl = NULL,
	.frc_handle = NULL,
	.esd_handle = NULL,
};

static int k3_offlinecompser_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_panel_info *pinfo = NULL;
	struct k3_fb_data_type *k3fd = NULL;
	struct platform_device *reg_dev = NULL;
	int i = 0;

	if (k3_fb_device_probe_defer(PANEL_OFFLINECOMPOSER)) {
		goto err_probe_defer;
	}

	K3_FB_DEBUG("enter!\n");

	BUG_ON(pdev == NULL);

	pdev->id = 1;
	pinfo = k3_offlinecomposer_panel_data.panel_info;
	memset(pinfo, 0, sizeof(struct k3_panel_info));
	pinfo->xres = g_primary_lcd_xres;
	pinfo->yres = g_primary_lcd_yres;
	pinfo->type = PANEL_OFFLINECOMPOSER;

	/* alloc panel device data */
	ret = platform_device_add_data(pdev, &k3_offlinecomposer_panel_data,
		sizeof(struct k3_fb_panel_data));
	if (ret) {
		K3_FB_ERR("platform_device_add_data failed!\n");
		goto err_device_put;
	}

	reg_dev = k3_fb_add_device(pdev);
	if (!reg_dev) {
		K3_FB_ERR("k3_fb_add_device failed!\n");
		goto err_device_put;
	}

	k3fd = platform_get_drvdata(reg_dev);
	BUG_ON(k3fd == NULL);

	for (i = 0; i < K3_DSS_OFFLINE_MAX_NUM; i++) {
		INIT_LIST_HEAD(&k3fd->offline_cmdlist_head[i]);
		k3fd->offline_wb_status[i] = e_status_idle;
		init_waitqueue_head(&k3fd->offline_writeback_wq[i]);
	}

	for (i = 0; i < K3_DSS_OFFLINE_MAX_BLOCK; i++) {
		k3fd->block_rects[i] = (dss_rect_t *)kmalloc(sizeof(dss_rect_t), GFP_ATOMIC);
		if (!k3fd->block_rects[i]) {
			K3_FB_ERR("block_rects[%d] failed to alloc!", i);
			goto err_device_put;
		}
	}

	k3fd->block_overlay = (dss_overlay_t *)kmalloc(sizeof(dss_overlay_t), GFP_ATOMIC);
	if (!k3fd->block_overlay) {
		K3_FB_ERR("k3fd->block_overlay no mem.\n");
		goto err_device_put;
	}
	memset(k3fd->block_overlay, 0, sizeof(dss_overlay_t));

	for(i = 0; i < K3_DSS_OFFLINE_MAX_LIST; i++){
		ret = cmdlist_alloc_one_list(&k3fd->cmdlist_node[i], k3fd->ion_client);
		if(ret < 0){
			K3_FB_ERR("cmdlist_alloc_one_list no mem.\n");
			goto err_device_put;
		}
	}

	k3_dss_offline_module_default(k3fd);
	k3fd->dss_module_resource_initialized = true;

	K3_FB_DEBUG("exit!\n");

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
err_probe_defer:
	return -EPROBE_DEFER;
}

static const struct of_device_id k3_panel_match_table[] = {
	{
		.compatible = DTS_COMP_K3_OFFLINECOMPOSER_PANEL,
		.data = NULL,
	},
};
MODULE_DEVICE_TABLE(of, k3_panel_match_table);

static struct platform_driver this_driver = {
	.probe = k3_offlinecompser_probe,
	.remove = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		.name = "k3_offlinecompser_panel",
		.of_match_table = of_match_ptr(k3_panel_match_table),
	},
};

static int __init k3_offlinecompser_panel_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&this_driver);
	if (ret) {
		K3_FB_ERR("platform_driver_register failed, error=%d!\n", ret);
		return ret;
	}

	return ret;
}

module_init(k3_offlinecompser_panel_init);
