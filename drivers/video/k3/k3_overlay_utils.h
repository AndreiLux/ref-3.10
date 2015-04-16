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
#ifndef K3_OVERLAY_UTILS_H
#define K3_OVERLAY_UTILS_H

#include "k3_fb.h"


/*******************************************************************************
**
*/
extern u32 g_dss_module_base[DSS_CHN_MAX][MODULE_CHN_MAX];
extern u32 g_dss_module_ovl_base[DSS_OVL_MAX][MODULE_OVL_MAX];
extern u32 g_dss_module_eng_base[DSS_ENG_MAX][MODULE_ENG_MAX];


void dumpDssOverlay(dss_overlay_t *ov_data);

int k3_get_hal_format(struct fb_info *info);
int k3_overlay_init(struct k3_fb_data_type *k3fd);
int k3_overlay_deinit(struct k3_fb_data_type *k3fd);
int k3_overlay_on(struct k3_fb_data_type *k3fd);
int k3_overlay_off(struct k3_fb_data_type *k3fd);
bool k3_dss_check_reg_reload_status(struct k3_fb_data_type *k3fd);
void k3_writeback_isr_handler(struct k3_fb_data_type *k3fd);
void k3_vactive0_start_isr_handler(struct k3_fb_data_type *k3fd);
int k3_vactive0_start_config(struct k3_fb_data_type *k3fd);
void k3_dss_dirty_region_updt_config(struct k3_fb_data_type *k3fd);

int k3_ov_compose_handler(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req, 	dss_layer_t *layer,
	dss_rect_ltrb_t *clip_rect,
	dss_rect_t *aligned_rect,
	bool *rdma_stretch_enable);
int k3_ov_compose_handler_wb(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req, 	dss_wb_layer_t *wb_layer,
	dss_rect_t *aligned_rect);

int k3_overlay_pan_display(struct k3_fb_data_type *k3fd);
int k3_ov_online_play(struct k3_fb_data_type *k3fd, unsigned long *argp);
int k3_ov_offline_play(struct k3_fb_data_type *k3fd, unsigned long *argp);
int k3_ov_online_writeback(struct k3_fb_data_type *k3fd, unsigned long *argp);
int k3_online_writeback_control(struct k3_fb_data_type *k3fd, int *argp);
int k3_overlay_ioctl_handler(struct k3_fb_data_type *k3fd,
	u32 cmd, void __user *argp);

void k3_dss_unflow_handler(struct k3_fb_data_type *k3fd, bool unmask);

void k3_dss_config_ok_begin(struct k3_fb_data_type *k3fd);
void k3_dss_config_ok_end(struct k3_fb_data_type *k3fd);

void k3_dfs_wbe_enable(struct k3_fb_data_type *k3fd);
void k3_dfs_wbe_disable(struct k3_fb_data_type *k3fd);

void k3_adp_offline_start_enable(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req);
void k3_adp_offline_start_disable(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req);

int k3_dss_handle_prev_ovl_req(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req);
int k3_dss_handle_cur_ovl_req(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req);
int k3_dss_handle_prev_ovl_req_wb(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req);
int k3_dss_handle_cur_ovl_req_wb(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req);
int k3_dss_rptb_handler(struct k3_fb_data_type *k3fd, bool is_wb, int rptb_info_idx);

int k3_dss_check_layer_par(struct k3_fb_data_type *k3fd, dss_layer_t *layer);
dss_rect_t k3_dss_rdma_out_rect(struct k3_fb_data_type *k3fd, dss_layer_t *layer);

int k3_dss_rdma_bridge_config(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req);
int k3_dss_mmu_config(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req,
	dss_layer_t *layer, dss_wb_layer_t *wb_layer, bool rdma_stretch_enable);
int k3_dss_rdma_config(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req,
	dss_layer_t *layer, dss_rect_ltrb_t *clip_rect, dss_rect_t *aligned_rect,
	bool *rdma_stretch_enable);
int k3_dss_fbdc_config(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req,
	dss_layer_t *layer, dss_rect_t aligned_rect);
int k3_dss_rdfc_config(struct k3_fb_data_type *k3fd, dss_layer_t *layer,
	dss_rect_t *aligned_rect, dss_rect_ltrb_t clip_rect);
int k3_dss_scf_coef_load(struct k3_fb_data_type *k3fd);
int k3_dss_scf_coef_unload(struct k3_fb_data_type *k3fd);
int k3_dss_scf_config(struct k3_fb_data_type *k3fd, dss_layer_t *layer,
	dss_wb_layer_t *wb_layer, dss_rect_t *aligned_rect, bool rdma_stretch_enable);
int k3_dss_scp_config(struct k3_fb_data_type *k3fd, dss_layer_t *layer, bool first_block);
int k3_dss_csc_config(struct k3_fb_data_type *k3fd,
	dss_layer_t *layer, dss_wb_layer_t *wb_layer);

int k3_dss_ovl_base_config(struct k3_fb_data_type *k3fd,
	dss_rect_t *wb_block_rect, u32 ovl_flags);
int k3_dss_ovl_layer_config(struct k3_fb_data_type *k3fd,
	dss_layer_t *layer, dss_rect_t *wb_block_rect);
int k3_dss_ovl_optimized(struct k3_fb_data_type *k3fd);
int k3_dss_ovl_check_pure_layer(struct k3_fb_data_type *k3fd, int layer_index,
	u16* alpha_flag, u16* color_flag, u32* color);

int k3_dss_mux_config(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req, dss_layer_t *layer);

int k3_dss_module_default(struct k3_fb_data_type *k3fd);
int k3_dss_offline_module_default(struct k3_fb_data_type *k3fd);

int k3_dss_module_init(struct k3_fb_data_type *k3fd);
int k3_dss_module_config(struct k3_fb_data_type *k3fd);

int k3_dss_wbe_mux_config(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req, dss_wb_layer_t *layer);
int k3_dss_wdfc_config(struct k3_fb_data_type *k3fd, dss_wb_layer_t *layer,
	dss_rect_t *aligned_rect, dss_rect_t *block_rect);
int k3_dss_fbc_config(struct k3_fb_data_type *k3fd,
	dss_wb_layer_t *layer, dss_rect_t aligned_rect);
int k3_dss_wdma_config(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req,
	dss_wb_layer_t *layer, dss_rect_t aligned_rect, dss_rect_t *block_rect);
int k3_dss_wdma_bridge_config(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req);

void* k3_dss_rptb_init(void);
void k3_dss_rptb_deinit(void *handle);
u32 k3_dss_rptb_alloc(void *handle, u32 size);
void k3_dss_rptb_free(void *handle, u32 addr, u32 size);
int k3_get_rot_index(int chn_idx);
int k3_get_scf_index(int chn_idx);

bool isYUVPackage(int format);
bool isYUVSemiPlanar(int format);
bool isYUVPlanar(int format);
bool isYUV(int format);


#endif /* K3_OVERLAY_UTILS_H */
