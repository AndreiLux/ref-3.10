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

#include "k3_overlay_utils.h"
#include "k3_dpe_utils.h"


int k3_ov_compose_handler(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req, 	dss_layer_t *layer,
	dss_rect_ltrb_t *clip_rect,
	dss_rect_t *aligned_rect,
	bool *rdma_stretch_enable)
{
	int ret = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);
	BUG_ON(layer == NULL);
	BUG_ON(clip_rect == NULL);
	BUG_ON(aligned_rect == NULL);
	BUG_ON(rdma_stretch_enable == NULL);

	ret = k3_dss_check_layer_par(k3fd, layer);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_check_layer_par failed! ret = %d\n", ret);
		goto err_return;
	}

	if (layer->need_cap & (CAP_BASE | CAP_DIM)) {
		ret = k3_dss_ovl_layer_config(k3fd, layer, NULL);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_ovl_config failed! need_cap=0x%x, ret=%d\n",
				layer->need_cap, ret);
			return ret;
		}

		return ret;
	}

	ret = k3_dss_rdma_config(k3fd, pov_req, layer,
		clip_rect, aligned_rect, rdma_stretch_enable);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_rdma_config failed! ret = %d\n", ret);
		goto err_return;
	}

	if (layer->src.mmu_enable) {
		ret = k3_dss_mmu_config(k3fd, pov_req, layer, NULL, *rdma_stretch_enable);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_mmu_config failed! ret = %d\n", ret);
			goto err_return;
		}
	}

	ret = k3_dss_fbdc_config(k3fd, pov_req, layer, *aligned_rect);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_rdma_config failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_rdfc_config(k3fd, layer, aligned_rect, *clip_rect);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_rdfc_config failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_scf_config(k3fd, layer, NULL, aligned_rect, *rdma_stretch_enable);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_scf_config failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_scp_config(k3fd, layer, false);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_scp_config failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_csc_config(k3fd, layer, NULL);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_csc_config failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_mux_config(k3fd, pov_req, layer);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_mux_config failed, ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_ovl_layer_config(k3fd, layer, NULL);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_ovl_config failed, ret = %d\n", ret);
		goto err_return;
	}

	return 0;

err_return:
	return ret;
}

int k3_ov_compose_handler_wb(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req, 	dss_wb_layer_t *wb_layer,
	dss_rect_t *aligned_rect)
{
	int ret = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);
	BUG_ON(wb_layer == NULL);
	BUG_ON(aligned_rect == NULL);

	ret = k3_dss_wbe_mux_config(k3fd, pov_req, wb_layer);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_wbe_mux_config failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_csc_config(k3fd, NULL, wb_layer);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_csc_config failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_scf_config(k3fd, NULL, wb_layer, NULL, false);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_wb_scf_config failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_wdfc_config(k3fd, wb_layer, aligned_rect, NULL);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_wdfc_config failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_fbc_config(k3fd, wb_layer, *aligned_rect);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_fbc_config failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_wdma_config(k3fd, pov_req, wb_layer, *aligned_rect, NULL);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_wdma_config failed! ret = %d\n", ret);
		goto err_return;
	}

	if (wb_layer->dst.mmu_enable) {
		ret = k3_dss_mmu_config(k3fd, pov_req, NULL, wb_layer, false);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_mmu_config failed! ret = %d\n", ret);
			goto err_return;
		}
	}

	return 0;

err_return:
	return ret;
}

int k3_overlay_pan_display(struct k3_fb_data_type *k3fd)
{
	int ret = 0;
	struct fb_info *fbi = NULL;
	dss_overlay_t *pov_req = NULL;
	dss_layer_t *layer = NULL;
	dss_rect_ltrb_t clip_rect;
	dss_rect_t aligned_rect;
	bool rdma_stretch_enable = false;
	u32 offset = 0;
	u32 addr = 0;
	int hal_format = 0;
	u8 ovl_type = 0;

	BUG_ON(k3fd == NULL);
	fbi = k3fd->fbi;
	BUG_ON(fbi == NULL);
	pov_req = &(k3fd->ov_req);
	BUG_ON(pov_req == NULL);

	if (!k3fd->panel_power_on) {
		K3_FB_ERR("fb%d, panel is power off!", k3fd->index);
		return 0;
	}

	k3fb_activate_vsync(k3fd);

	ovl_type = k3fd->ovl_type;
	offset = fbi->var.xoffset * (fbi->var.bits_per_pixel >> 3) +
		fbi->var.yoffset * fbi->fix.line_length;
	addr = fbi->fix.smem_start + offset;
	if (!fbi->fix.smem_start) {
		K3_FB_ERR("smem_start is null!\n");
		goto err_return;
	}

	if (fbi->fix.smem_len <= 0) {
		K3_FB_ERR("smem_len(%d) is out of range!\n", fbi->fix.smem_len);
		goto err_return;
	}

	hal_format = k3_get_hal_format(fbi);
	if (hal_format < 0) {
		K3_FB_ERR("not support this fb_info's format!\n");
		goto err_return;
	}

	ret = k3_vactive0_start_config(k3fd);
	if (ret != 0) {
		K3_FB_ERR("1_k3_vactive0_start_config failed! ret = %d\n", ret);

		//try again
		ret = k3_vactive0_start_config(k3fd);
		if (ret != 0) {
			K3_FB_ERR("2_k3_vactive0_start_config failed! ret = %d\n", ret);
			goto err_return;
		}

		if (is_mipi_cmd_panel(k3fd))
			k3fd->ldi_data_gate_en = 0;
	}

	ret = k3_dss_module_init(k3fd);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_module_init failed! ret = %d\n", ret);
		goto err_return;
	}

	k3_dss_config_ok_begin(k3fd);

	k3_dss_handle_prev_ovl_req(k3fd, pov_req);

	memset(pov_req, 0, sizeof(dss_overlay_t));
	layer = &(pov_req->layer_infos[0]);
	pov_req->layer_nums = 1;
	if (ovl_type == DSS_OVL_PDP) {
		pov_req->ovl_flags = DSS_SCENE_PDP_ONLINE;
	} else if (ovl_type == DSS_OVL_SDP) {
		pov_req->ovl_flags = DSS_SCENE_SDP_ONLINE;
	} else {
		K3_FB_ERR("not support this ovl_type(%d)!\n", ovl_type);
	}

	layer->src.format = hal_format;
	layer->src.width = fbi->var.xres;
	layer->src.height = fbi->var.yres;
	layer->src.bpp = fbi->var.bits_per_pixel >> 3;
	layer->src.stride = fbi->fix.line_length;
	layer->src.phy_addr = addr;
	layer->src.vir_addr = addr;
	layer->src.ptba = k3fd->iommu_format.iommu_ptb_base;
	layer->src.ptva = k3fd->iommu_format.iommu_iova_base;
	layer->src.buf_size = fbi->fix.line_length * fbi->var.yres;
	layer->src.is_tile = 0;
#ifdef CONFIG_K3_FB_HEAP_CARVEOUT_USED
	layer->src.mmu_enable = 0;
#else
	layer->src.mmu_enable = 1;
#endif

	layer->src_rect.x = 0;
	layer->src_rect.y = 0;
	layer->src_rect.w = fbi->var.xres;
	layer->src_rect.h = fbi->var.yres;

	layer->dst_rect.x = 0;
	layer->dst_rect.y = 0;
	layer->dst_rect.w = fbi->var.xres;
	layer->dst_rect.h = fbi->var.yres;

	layer->transform = K3_FB_TRANSFORM_NOP;
	layer->blending = K3_FB_BLENDING_NONE;
	layer->glb_alpha = 0xFF;
	layer->color = 0x0;
	layer->layer_idx = 0x0;
	layer->chn_idx = DPE0_CHN3;
	layer->need_cap = CAP_FB_POST;
	layer->flags = DSS_SCENE_PDP_ONLINE;

	k3_dss_handle_cur_ovl_req(k3fd, pov_req);

	ret = k3_dss_ovl_base_config(k3fd, NULL, pov_req->ovl_flags);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_ovl_init failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_rdma_bridge_config(k3fd, pov_req);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_rdma_bridge_config failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_ov_compose_handler(k3fd, pov_req, layer,
		&clip_rect, &aligned_rect, &rdma_stretch_enable);
	if (ret != 0) {
		K3_FB_ERR("k3_ov_compose_handler failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_module_config(k3fd);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_module_config failed! ret = %d\n", ret);
		goto err_return;
	}

	k3_dss_config_ok_end(k3fd);
	single_frame_update(k3fd);
	k3_dss_unflow_handler(k3fd, true);

	k3fb_deactivate_vsync(k3fd);

	return 0;

err_return:
	single_frame_update(k3fd);
	k3fb_deactivate_vsync(k3fd);
	return ret;
}

int k3_ov_online_play(struct k3_fb_data_type *k3fd, unsigned long *argp)
{
	dss_overlay_t *pov_req = NULL;
	dss_layer_t *layer = NULL;
	dss_wb_layer_t *wb_layer = NULL;
	dss_rect_ltrb_t clip_rect;
	dss_rect_t aligned_rect;
	bool rdma_stretch_enable = false;
#ifdef CONFIG_BUF_SYNC_USED
	unsigned long flags = 0;
#endif
	int i = 0;
	int ret = 0;

	BUG_ON(k3fd == NULL);
	pov_req = &(k3fd->ov_req);
	BUG_ON(pov_req == NULL);

	if (!k3fd->panel_power_on) {
		K3_FB_ERR("fb%d panel is power off!", k3fd->index);
		return 0;
	}

	if (k3fd->index == EXTERNAL_PANEL_IDX && !k3fd->is_hdmi_power_full) {
		K3_FB_ERR("fb%d hdmi is not power full!", k3fd->index);
		return 0;
	}

	k3fb_activate_vsync(k3fd);

	ret = k3_vactive0_start_config(k3fd);
	if (ret != 0) {
		K3_FB_ERR("1_k3_vactive0_start_config failed! ret = %d\n", ret);

		//try again
		ret = k3_vactive0_start_config(k3fd);
		if (ret != 0) {
			K3_FB_ERR("2_k3_vactive0_start_config failed! ret = %d\n", ret);
			goto err_return;
		}

		if (is_mipi_cmd_panel(k3fd)) {
			k3fd->dss_exception.underflow_exception = 1;
			k3fd->ldi_data_gate_en = 0;
		}
	}

	ret = k3_dss_module_init(k3fd);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_module_init failed! ret = %d\n", ret);
		goto err_return;
	}

	k3_dss_config_ok_begin(k3fd);

	k3_dss_handle_prev_ovl_req(k3fd, pov_req);
	k3_dss_handle_prev_ovl_req_wb(k3fd, pov_req);
	if (pov_req->wb_enable && !k3fd->ov_wb_enabled) {
		k3_dss_rptb_handler(k3fd, true, 0);
	}

	ret = copy_from_user(pov_req, argp, sizeof(dss_overlay_t));
	if (ret) {
		K3_FB_ERR("copy_from_user failed!\n");
		goto err_return;
	}

	if (g_debug_ovl_online_composer)
		dumpDssOverlay(pov_req);

	k3_dss_handle_cur_ovl_req(k3fd, pov_req);

	ret = k3_dss_ovl_base_config(k3fd, NULL, pov_req->ovl_flags);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_ovl_init failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_rdma_bridge_config(k3fd, pov_req);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_rdma_bridge_config failed! ret = %d\n", ret);
		goto err_return;
	}

	/* Go through all layers */
	for (i = 0; i < pov_req->layer_nums; i++) {
		layer = &(pov_req->layer_infos[i]);
		memset(&clip_rect, 0, sizeof(dss_rect_ltrb_t));
		memset(&aligned_rect, 0, sizeof(dss_rect_ltrb_t));
		rdma_stretch_enable = false;

		ret = k3_ov_compose_handler(k3fd, pov_req, layer,
			&clip_rect, &aligned_rect, &rdma_stretch_enable);
		if (ret != 0) {
			K3_FB_ERR("k3_ov_compose_handler failed! ret = %d\n", ret);
			goto err_return;
		}
	}

	if (pov_req->wb_enable && k3fd->ov_wb_enabled) {
		wb_layer = &(pov_req->wb_layer_info);

		ret = k3_ov_compose_handler_wb(k3fd, pov_req, wb_layer, &aligned_rect);
		if (ret != 0) {
			K3_FB_ERR("k3_ov_compose_handler_wb failed! ret = %d\n", ret);
			goto err_return;
		}

		ret = k3_dss_wdma_bridge_config(k3fd, pov_req);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_wdma_bridge_config failed! ret = %d\n", ret);
			goto err_return;
		}
	}
	ret = k3_dss_module_config(k3fd);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_module_config failed! ret = %d\n", ret);
		goto err_return;
	}

	if (k3fd->panel_info.dirty_region_updt_support)
		k3_dss_dirty_region_updt_config(k3fd);

#ifdef CONFIG_BUF_SYNC_USED
	for (i = 0; i < pov_req->layer_nums; i++) {
		if (pov_req->layer_infos[i].acquire_fence >= 0){
			k3fb_buf_sync_wait(pov_req->layer_infos[i].acquire_fence);
		}
	}

	ret = k3fb_buf_sync_create_fence(k3fd, ++k3fd->buf_sync_ctrl.timeline_max);
	if (ret < 0) {
		K3_FB_ERR("k3_create_fence failed! ret = 0x%x\n", ret);
		goto err_return;
	}

	pov_req->release_fence = ret;

	if (pov_req->wb_enable && k3fd->ov_wb_enabled) {
		if (pov_req->wb_layer_info.acquire_fence >= 0) {
			k3fb_buf_sync_wait(pov_req->wb_layer_info.acquire_fence);
		}
		pov_req->wb_layer_info.release_fence = -1;
	}

	if (copy_to_user((struct dss_overlay_t __user *)argp,
			pov_req, sizeof(dss_overlay_t))) {
		ret = -EFAULT;
		put_unused_fd(pov_req->release_fence);
		goto err_return;
	}

	spin_lock_irqsave(&k3fd->buf_sync_ctrl.refresh_lock, flags);
	k3_dss_config_ok_end(k3fd);
	single_frame_update(k3fd);
	k3_dss_unflow_handler(k3fd, true);

	k3fd->buf_sync_ctrl.refresh++;
	spin_unlock_irqrestore(&k3fd->buf_sync_ctrl.refresh_lock, flags);
#else
	k3_dss_config_ok_end(k3fd);
	single_frame_update(k3fd);
	k3_dss_unflow_handler(k3fd, true);
#endif

	k3fb_deactivate_vsync(k3fd);

	return 0;

err_return:
	single_frame_update(k3fd);
	k3fb_deactivate_vsync(k3fd);
	return ret;
}

int k3_ov_online_writeback(struct k3_fb_data_type *k3fd, unsigned long *argp)
{
	dss_overlay_t *pov_req = NULL;
	dss_wb_layer_t *wb_layer = NULL;
	dss_rect_t aligned_rect;
	int ret = 0;

	BUG_ON(k3fd == NULL);
	pov_req = &(k3fd->ov_wb_req);
	BUG_ON(pov_req == NULL);

	if (!k3fd->panel_power_on) {
		K3_FB_ERR("fb%d, panel is power off!", k3fd->index);
		return 0;
	}

	if (!k3fd->ov_wb_enabled) {
		K3_FB_ERR("fb%d, wb disabled!", k3fd->index);
		return 0;
	}

	k3fb_activate_vsync(k3fd);

	ret = k3_dss_module_init(k3fd);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_module_init failed! ret = %d\n", ret);
		goto err_return;
	}

	k3_dss_config_ok_begin(k3fd);

	k3_dss_handle_prev_ovl_req_wb(k3fd, pov_req);

	ret = copy_from_user(pov_req, argp, sizeof(dss_overlay_t));
	if (ret) {
		K3_FB_ERR("copy_from_user failed!\n");
		goto err_return;
	}

	wb_layer = &(pov_req->wb_layer_info);

	k3_dss_handle_cur_ovl_req_wb(k3fd, pov_req);

	ret = k3_ov_compose_handler_wb(k3fd, pov_req, wb_layer, &aligned_rect);
	if (ret != 0) {
		K3_FB_ERR("k3_ov_compose_handler_wb failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_wdma_bridge_config(k3fd, pov_req);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_wdma_bridge_config failed! ret = %d\n", ret);
		goto err_return;
	}

	ret = k3_dss_module_config(k3fd);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_module_config failed! ret = %d\n", ret);
		goto err_return;
	}

	k3fd->ov_wb_done = 0;

	k3_dss_config_ok_end(k3fd);
	k3_dss_unflow_handler(k3fd, true);

	ret = wait_event_interruptible_timeout(k3fd->ov_wb_wq,
		(k3fd->ov_wb_done == 1), HZ);
	if (ret <= 0) {
		K3_FB_ERR("wait_for online write back done timeout, ret=%d!\n", ret);
		ret = -ETIMEDOUT;
		goto err_return;
	} else {
		ret = 0;
	}

	k3fb_deactivate_vsync(k3fd);

	return 0;

err_return:
	k3fb_deactivate_vsync(k3fd);
	return ret;
}

int k3_online_writeback_control(struct k3_fb_data_type *k3fd, int *argp)
{
	int ret = 0;
	int enable = 0;
	dss_overlay_t *pov_req = NULL;

	BUG_ON(k3fd == NULL);
	pov_req = &(k3fd->ov_wb_req);
	BUG_ON(pov_req == NULL);

	ret = copy_from_user(&enable, argp, sizeof(enable));
	if (ret) {
		K3_FB_ERR("copy_from_user ioctl failed!\n");
		return ret;
	}

	enable = (enable) ? 1 : 0;
	K3_FB_INFO("online WB switch, enable:%d, prev:%d.\n",
		enable, k3fd->ov_wb_enabled);

	if (k3fd->ov_wb_enabled == enable)
		return 0;

	k3fd->ov_wb_enabled = enable;

	if (!k3fd->panel_power_on) {
		k3fd->ov_wb_enabled = 0;
		K3_FB_ERR("fb%d, panel is power off!", k3fd->index);
		return -1;
	}

	k3fb_activate_vsync(k3fd);

	if (enable) {
		k3_dfs_wbe_enable(k3fd);
	} else {
		k3_dfs_wbe_disable(k3fd);
	}

	k3fb_deactivate_vsync(k3fd);

	return 0;
}