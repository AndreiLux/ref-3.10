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

#include <linux/errno.h>

#include "k3_overlay_utils.h"
#include "k3_block_algorithm.h"
#include "k3_ov_cmd_list_utils.h"


#define OFFLINE_COMPOSE_TIMEOUT 500
extern u32 g_dss_module_base[DSS_CHN_MAX][MODULE_CHN_MAX];

static void offline_chn_lock(struct k3_fb_data_type *k3fd)
{
	int i = 0;

	BUG_ON(k3fd == NULL);

	for(i = 0; i < k3fd->ov_req.layer_nums; i++){
		if(k3fd->ov_req.layer_infos[i].chn_idx == DPE3_CHN0)
			down(&k3fd->dpe3_ch0_sem);

		if(k3fd->ov_req.layer_infos[i].chn_idx == DPE3_CHN1)
			down(&k3fd->dpe3_ch1_sem);
	}
}

static void offline_chn_unlock(struct k3_fb_data_type *k3fd)
{
	int i = 0;

	BUG_ON(k3fd == NULL);
	for(i = 0; i < k3fd->ov_req.layer_nums; i++){
		if(k3fd->ov_req.layer_infos[i].chn_idx == DPE3_CHN0)
			up(&k3fd->dpe3_ch0_sem);

		if(k3fd->ov_req.layer_infos[i].chn_idx == DPE3_CHN1)
			up(&k3fd->dpe3_ch1_sem);
	}
}

static void offline_save_bin_file(char * filename, char * str_line, u32 len)
{
	ssize_t write_len;
	struct file* fd = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;

	fd = filp_open(filename, O_CREAT|O_RDWR, 0644);
	if( IS_ERR( fd )){
		printk("offline_save_bin_file filp_open err!\n");
		return;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	write_len = vfs_write(fd, (char __user*)str_line,  len, &pos);

	pos = 0;
	set_fs(old_fs);
	filp_close(fd, NULL);
}

static void offline_dump_fail_info_to_file(dss_overlay_t *req, long timestamp)
{
	u32 i = 0;
	char * str_line = NULL;
	u32 len = 0;
	char filename[64] = {0};

	snprintf(filename, sizeof(filename), "/data/hwcdump/offline_%ld.txt", timestamp);
	str_line = kmalloc(SZ_16K, GFP_KERNEL);
	if(IS_ERR_OR_NULL(str_line)){
		printk("offline_dump_fail_info_to_file alloc buffer fail!\n");
		return;
	}

	len += snprintf(str_line + len, SZ_1K, "######################################################\n");
	len += snprintf(str_line + len, SZ_1K, "TEST START############################################\n");
	len += snprintf(str_line + len, SZ_1K, "\t TEST_ENABLE = %d;\n", 1);
	len += snprintf(str_line + len, SZ_1K, "\t case_name = \"%s\";\n", "offline.000");

	len += snprintf(str_line + len, SZ_1K,  "#\n");
	len += snprintf(str_line + len, SZ_1K,  "#\t wb_enable = %d;\n",req->wb_enable);
	len += snprintf(str_line + len, SZ_1K,  "#\t ovl_flags = %d;\n",req->ovl_flags);
	len += snprintf(str_line + len, SZ_1K,  "#\t hdr_start_addr = %d;\n",req->hdr_start_addr);
	len += snprintf(str_line + len, SZ_1K,  "#\t hdr_stride = %d;\n",req->hdr_stride);

	len += snprintf(str_line + len, SZ_1K,  "#\n");
	len += snprintf(str_line + len, SZ_1K,  "#write back info:\n");
	len += snprintf(str_line + len, SZ_1K,  "\t format = %d;\n", req->wb_layer_info.dst.format);
	len += snprintf(str_line + len, SZ_1K,  "\t wbe_chn = %d;\n", req->wb_layer_info.chn_idx);
	len += snprintf(str_line + len, SZ_1K,  "\t width = %d;\n", req->wb_layer_info.dst.width);
	len += snprintf(str_line + len, SZ_1K,  "\t height = %d;\n", req->wb_layer_info.dst.height);
	len += snprintf(str_line + len, SZ_1K,  "\t is_tile = %d;\n", req->wb_layer_info.dst.is_tile);
	len += snprintf(str_line + len, SZ_1K,  "\t mmu_enable = %d;\n", req->wb_layer_info.dst.mmu_enable);
	len += snprintf(str_line + len, SZ_1K,  "\t stride = %d;\n", req->wb_layer_info.dst.stride);
	len += snprintf(str_line + len, SZ_1K,  "\t is_display = %d;\n", 1);
	len += snprintf(str_line + len, SZ_1K,  "\t display_time_s = %d;\n", 0);
	len += snprintf(str_line + len, SZ_1K,  "\t need_compare = %d;\n", 0);
	len += snprintf(str_line + len, SZ_1K,  "\t result_file_name = \"%s\";\n", "xxx.bin");
	len += snprintf(str_line + len, SZ_1K,  "\t need_dump_bin_file = %d;\n", 0);
	len += snprintf(str_line + len, SZ_1K,  "\t dump_bin_file_name = \"%s\";\n", "xxx.bin");
	len += snprintf(str_line + len, SZ_1K,  "\t need_dump_bitmap = %d;\n", 0);
	len += snprintf(str_line + len, SZ_1K,  "\t dump_bitmap_file_name = \"%s\";\n", "xxx.bin");
	len += snprintf(str_line + len, SZ_1K,  "\t print_buffer_block = %d;\n", 0);

	len += snprintf(str_line + len, SZ_1K,  "#\t flags = %d;\n",req->wb_layer_info.flags);
	len += snprintf(str_line + len, SZ_1K,  "#\t need_cap = %d;\n",req->wb_layer_info.need_cap);
	len += snprintf(str_line + len, SZ_1K,  "#\t transform = %d;\n",req->wb_layer_info.transform);

	len += snprintf(str_line + len, SZ_1K,  "#\t src_x = %d;\n", req->wb_layer_info.src_rect.x);
	len += snprintf(str_line + len, SZ_1K,  "#\t src_y = %d;\n", req->wb_layer_info.src_rect.y);
	len += snprintf(str_line + len, SZ_1K,  "#\t src_w = %d;\n", req->wb_layer_info.src_rect.w);
	len += snprintf(str_line + len, SZ_1K,  "#\t src_h = %d;\n", req->wb_layer_info.src_rect.h);

	len += snprintf(str_line + len, SZ_1K,  "#\t dst_x = %d;\n", req->wb_layer_info.dst_rect.x);
	len += snprintf(str_line + len, SZ_1K,  "#\t dst_y = %d;\n", req->wb_layer_info.dst_rect.y);
	len += snprintf(str_line + len, SZ_1K,  "#\t dst_w = %d;\n", req->wb_layer_info.dst_rect.w);
	len += snprintf(str_line + len, SZ_1K,  "#\t dst_h = %d;\n", req->wb_layer_info.dst_rect.h);

	len += snprintf(str_line + len, SZ_1K,  "#\n");
	len += snprintf(str_line + len, SZ_1K,  "#input layer info:\n");
	len += snprintf(str_line + len, SZ_1K,  "\t lyaer_num = %d;\n", req->layer_nums);
	len += snprintf(str_line + len, SZ_1K,  "#\n");

	for(i = 0; i < req->layer_nums; i++){
		len += snprintf(str_line + len, SZ_1K,  "#layer_%d\n", i);
		len += snprintf(str_line + len, SZ_1K,  "\t file_name = \"error_%ld_%d.bin\"\n", timestamp, i);
		len += snprintf(str_line + len, SZ_1K,  "\t format = %d;\n", req->layer_infos[i].src.format);
		len += snprintf(str_line + len, SZ_1K,  "\t chn_index = %d;\n", req->layer_infos[i].chn_idx);
		len += snprintf(str_line + len, SZ_1K,  "\t width = %d;\n", req->layer_infos[i].src.width);
		len += snprintf(str_line + len, SZ_1K,  "\t height = %d;\n", req->layer_infos[i].src.height);
		len += snprintf(str_line + len, SZ_1K,  "\t is_tile = %d;\n", req->layer_infos[i].src.is_tile);
		len += snprintf(str_line + len, SZ_1K,  "\t mmu_enable = %d;\n", req->layer_infos[i].src.mmu_enable);
		len += snprintf(str_line + len, SZ_1K,  "\t stride = %d;\n", req->layer_infos[i].src.stride);
		len += snprintf(str_line + len, SZ_1K,  "\t transform = %d;\n", req->layer_infos[i].transform);

		len += snprintf(str_line + len, SZ_1K,  "\t src_x = %d;\n", req->layer_infos[i].src_rect.x);
		len += snprintf(str_line + len, SZ_1K,  "\t src_y = %d;\n", req->layer_infos[i].src_rect.y);
		len += snprintf(str_line + len, SZ_1K,  "\t src_w = %d;\n", req->layer_infos[i].src_rect.w);
		len += snprintf(str_line + len, SZ_1K,  "\t src_h = %d;\n", req->layer_infos[i].src_rect.h);

		len += snprintf(str_line + len, SZ_1K,  "\t dst_x = %d;\n", req->layer_infos[i].dst_rect.x);
		len += snprintf(str_line + len, SZ_1K,  "\t dst_y = %d;\n", req->layer_infos[i].dst_rect.y);
		len += snprintf(str_line + len, SZ_1K,  "\t dst_w = %d;\n", req->layer_infos[i].dst_rect.w);
		len += snprintf(str_line + len, SZ_1K,  "\t dst_h = %d;\n", req->layer_infos[i].dst_rect.h);

		len += snprintf(str_line + len, SZ_1K,  "\t is_dim = %d;\n", (req->layer_infos[i].need_cap & CAP_DIM) == CAP_DIM);
		len += snprintf(str_line + len, SZ_1K,  "\t src_mask_x = %d;\n", req->layer_infos[i].src_rect_mask.x);
		len += snprintf(str_line + len, SZ_1K,  "\t src_mask_y = %d;\n", req->layer_infos[i].src_rect_mask.y);
		len += snprintf(str_line + len, SZ_1K,  "\t src_mask_w = %d;\n", req->layer_infos[i].src_rect_mask.w);
		len += snprintf(str_line + len, SZ_1K,  "\t src_mask_h = %d;\n", req->layer_infos[i].src_rect_mask.h);
		len += snprintf(str_line + len, SZ_1K,  "\t print_buffer_block = %d;\n", 0);

		len += snprintf(str_line + len, SZ_1K,  "\t need_cap = %x;\n",req->layer_infos[i].need_cap);
		len += snprintf(str_line + len, SZ_1K,  "#\t flags = %d;\n",req->layer_infos[i].flags);
		len += snprintf(str_line + len, SZ_1K,  "\t blending = %d;\n",req->layer_infos[i].blending);
		len += snprintf(str_line + len, SZ_1K,  "#\t glb_alpha = %d;\n",req->layer_infos[i].glb_alpha);
		len += snprintf(str_line + len, SZ_1K,  "#\t color = %d;\n",req->layer_infos[i].color);
	}

	len += snprintf(str_line + len, SZ_1K,  "######################################################\n");

	offline_save_bin_file(filename, str_line, len);
	kfree(str_line);

	return;
}

static int k3_dss_offline_one_layer_config(struct k3_fb_data_type *k3fd,
	dss_layer_t *layer, dss_rect_t *wb_block_rect)
{
	int ret = 0;
	bool rdma_stretch_enable = false;
	dss_rect_ltrb_t clip_rect;
	dss_rect_t aligned_rect;
	dss_overlay_t *pov_req = NULL;
	int scf_idx = 0;

	BUG_ON(k3fd == NULL);
	pov_req = &(k3fd->ov_req);
	BUG_ON(pov_req == NULL);

	ret = k3_dss_check_layer_par(k3fd, layer);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_check_layer_par failed! ret = %d\n", ret);
		return ret;
	}

	if (layer->need_cap & (CAP_BASE | CAP_DIM)) {
		ret = k3_dss_ovl_layer_config(k3fd, layer, wb_block_rect);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_ovl_config failed! need_cap=0x%x, ret=%d\n",
				layer->need_cap, ret);
			return ret;
		}

		return ret;
	}

	ret = k3_dss_rdma_config(k3fd, pov_req, layer, &clip_rect, &aligned_rect, &rdma_stretch_enable);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_rdma_config failed! ret = %d\n", ret);
		return ret;
	}

	if (layer->src.mmu_enable) {
		ret = k3_dss_mmu_config(k3fd, pov_req, layer, NULL, rdma_stretch_enable);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_mmu_config failed! ret = %d\n", ret);
			return ret;
		}
	}

	ret = k3_dss_rdfc_config(k3fd, layer, &aligned_rect, clip_rect);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_rdfc_config failed! ret = %d\n", ret);
		return ret;
	}

	scf_idx = k3_get_scf_index(layer->chn_idx);
	if(scf_idx >= 0)
		k3fd->dss_module.scf_used[scf_idx] = 1;

	ret = k3_dss_scf_config(k3fd, layer, NULL, &aligned_rect, rdma_stretch_enable);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_scf_config failed! ret = %d\n", ret);
		return ret;
	}

	ret = k3_dss_scp_config(k3fd, layer, false);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_scp_config failed! ret = %d\n", ret);
		return ret;
	}

	k3fd->dss_module.csc_used[layer->chn_idx] = 1;
	if(k3fd->ov_req.wb_layer_info.chn_idx == WBE1_CHN0
		&& isYUV(k3fd->ov_req.wb_layer_info.dst.format)
		&& k3fd->ov_req.wb_enable){

		if(!isYUV(layer->src.format)){
			k3fd->dss_module.csc_used[k3fd->ov_req.wb_layer_info.chn_idx] = 1;
			ret = k3_dss_csc_config(k3fd, NULL, &k3fd->ov_req.wb_layer_info);
			if (ret != 0) {
				K3_FB_ERR("k3_dss_csc_config failed! ret = %d\n", ret);
				return ret;
			}
		}
	}else{

		ret = k3_dss_csc_config(k3fd, layer, NULL);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_csc_config failed! ret = %d\n", ret);
			return ret;
		}
	}

	ret = k3_dss_mux_config(k3fd, pov_req,layer);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_mux_config failed, ret = %d\n", ret);
		return ret;
	}

	ret = k3_dss_ovl_layer_config(k3fd, layer, wb_block_rect);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_ovl_config failed, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int k3_dss_write_back_config(struct k3_fb_data_type *k3fd,
	dss_wb_layer_t *wb_layer, dss_rect_t *wb_block_rect)
{
	int ret = 0;
	dss_rect_t wdfc_aligned_rect;
	dss_overlay_t *pov_req = NULL;

	BUG_ON(k3fd == NULL);
	pov_req = &(k3fd->ov_req);
	BUG_ON(pov_req == NULL);

	/**********************Write Back***************************/
	ret = k3_dss_wbe_mux_config(k3fd, pov_req,wb_layer);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_wbe_mux_config failed! ret = %d\n", ret);
		return ret;
	}

	if(wb_layer->chn_idx != WBE1_CHN0 && k3fd->ov_req.wb_enable){
		k3fd->dss_module.csc_used[wb_layer->chn_idx] = 1;
		ret = k3_dss_csc_config(k3fd, NULL, wb_layer);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_csc_config failed! ret = %d\n", ret);
			return ret;
		}
	}

	ret = k3_dss_scf_config(k3fd, NULL, wb_layer, NULL, false);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_wb_scf_config failed! ret = %d\n", ret);
		return ret;
	}

	ret = k3_dss_wdfc_config(k3fd, wb_layer, &wdfc_aligned_rect, wb_block_rect);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_wdfc_config failed, ret = %d\n", ret);
		return ret;
	}

	ret = k3_dss_wdma_config(k3fd, pov_req, wb_layer, wdfc_aligned_rect, wb_block_rect);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_wdma_config failed, ret = %d\n", ret);
		return ret;
	}

	if (wb_layer->dst.mmu_enable) {
		ret = k3_dss_mmu_config(k3fd, pov_req, NULL, wb_layer, false);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_mmu_config failed! ret = %d\n", ret);
			return ret;
		}
	}

	ret = k3_dss_wdma_bridge_config(k3fd, pov_req);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_wdma_bridge_config failed! ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int offline_add_pending_frame(struct k3_fb_data_type *k3fd, u32 wbe_chn, u32 flag)
{
	dss_overlay_t *pov_req = NULL;
	char __iomem *dss_base = NULL;
	u32 i = 0;
	int ret = 0;

	BUG_ON(k3fd == NULL);
	pov_req = &(k3fd->ov_req);
	BUG_ON(wbe_chn >= K3_DSS_OFFLINE_MAX_NUM);
	BUG_ON(pov_req == NULL);
	dss_base = k3fd->dss_base;

	ret = cmdlist_add_new_list(k3fd, &k3fd->offline_cmdlist_head[wbe_chn], TRUE, flag);
	if(ret != 0){
		K3_FB_ERR("cmdlist_add_new_list err:%d \n",ret);
		return ret;
	}
	//k3_adp_offline_start_disable(k3fd, pov_req);
	ret = k3_dss_module_init(k3fd);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_module_init failed! ret = %d\n", ret);
	}

	for (i = 0; i < pov_req->layer_nums; i++) {
		int chn_idx = pov_req->layer_infos[i].chn_idx - DPE2_CHN0;
		int rot_idx = 0;

		if(chn_idx > DPE3_CHN3 - DPE2_CHN0 || chn_idx < 0){
			K3_FB_ERR("offline_add_virtual_frame  chn:%d err\n", pov_req->layer_infos[i].chn_idx);
			return -1;
		}

		k3fd->set_reg(k3fd, dss_base +DSS_GLB_DPE2_CH0_CTL + chn_idx * 0x04, 0, 32, 0);

		if(isYUVPlanar(pov_req->layer_infos[i].src.format))
			k3fd->set_reg(k3fd, dss_base +DSS_GLB_DPE2_CH0_CTL + (chn_idx + 1) * 0x04, 0, 32, 0);

		rot_idx = k3_get_rot_index(pov_req->layer_infos[i].chn_idx);
		if(rot_idx > 0){
			k3fd->set_reg(k3fd, dss_base + DSS_GLB_ROT_TLB0_SCENE + rot_idx * 4, 0, 32, 0);
		}
	}

	k3fd->set_reg(k3fd, dss_base + g_dss_module_base[pov_req->wb_layer_info.chn_idx][MODULE_DMA0]
		+ WDMA_CTRL, 0, 1, 25);
	if(pov_req->wb_layer_info.dst.mmu_enable)
		k3fd->set_reg(k3fd, dss_base + g_dss_module_base[pov_req->wb_layer_info.chn_idx][MODULE_MMU_DMA0], 0, 1, 0);

	if(pov_req->wb_layer_info.chn_idx == WBE1_CHN1){

		k3fd->set_reg(k3fd, dss_base +DSS_GLB_OV2_SCENE, 0, 32, 0);
	}

	memset(&(k3fd->dss_exception), 0, sizeof(dss_exception_t));
	k3fd->dss_exception.underflow_exception = 1;
	k3_dss_handle_prev_ovl_req(k3fd, pov_req);
	memset(&(k3fd->dss_exception), 0, sizeof(dss_exception_t));

	ret = k3_dss_module_config(k3fd);
	if (ret != 0) {
		K3_FB_ERR("k3_dss_module_config failed! ret = %d\n", ret);
	}

	k3_adp_offline_start_enable(k3fd, pov_req);

	return 0;
}

static void offline_stop_glb(struct k3_fb_data_type *k3fd, u32 wbe_chn)
{
	char __iomem *ctl_base = NULL;

	BUG_ON(k3fd == NULL);

	ctl_base = k3fd->dss_base + DSS_GLB_WBE1_CH0_CTL +
			wbe_chn * (DSS_GLB_WBE1_CH1_CTL - DSS_GLB_WBE1_CH0_CTL);
	k3fd->set_reg(k3fd, ctl_base, 0x1, 1, 8);
	k3fd->set_reg(k3fd, ctl_base, 0x0, 1, 8);
	/*according to chip feature,need double stop.*/
	k3fd->set_reg(k3fd, ctl_base, 0x1, 1, 8);
	k3fd->set_reg(k3fd, ctl_base, 0x0, 1, 8);
}

static u32 print_serial_reg(char * str_line, u32 len, char __iomem * dss_base, u32 from, u32 to)
{
	u32 index = 0;
	u32 tmp_len = 0;
	char __iomem * addr = NULL;

	BUG_ON(str_line == NULL);
	BUG_ON(dss_base == NULL);

	for(index = from; index <= to; index += 0x4){
		addr = dss_base + index;
		if(tmp_len >= SZ_64K) {
			printk("----len:0x%x dss_base:0x%x index:0x%x",tmp_len,(u32)dss_base,index);
			BUG_ON(tmp_len >= SZ_64K);
		}
		tmp_len += snprintf(str_line + len + tmp_len, SZ_1K, "0x%8x=>0x%8x\n", (u32)addr, inp32(addr));
	}

	return tmp_len;
}

void offline_dump_fail_reg(char __iomem *dss_base, dss_overlay_t *req, long timestamp)
{

	char filename[128] = {0};
	u32 i = 0;
	char * str_line = NULL;
	u32 len = 0;

	BUG_ON(dss_base == NULL);
	BUG_ON(req == NULL);

	snprintf(filename, sizeof(filename), "/data/hwcdump/offline_%ld_reg.txt", timestamp);
	str_line = kmalloc(SZ_64K, GFP_KERNEL);
	if(IS_ERR_OR_NULL(str_line)){
		printk("offline_dump_fail_reg alloc buffer fail!\n");
		return;
	}

	outp32(dss_base + 0x404, 0x0);

	//GLB
	len += snprintf(str_line + len, SZ_1K, "CLB reg:\n");
	len += print_serial_reg(str_line, len, dss_base, 0x484, 0x484);
	len += print_serial_reg(str_line, len, dss_base, 0x4c4, 0x4c4);
	len += print_serial_reg(str_line, len, dss_base, 0x730, 0x758);

	//ADP
	len += snprintf(str_line + len, SZ_1K, "ADP reg:\n");
	len += print_serial_reg(str_line, len, dss_base, 0x42530, 0x42538);

	//RDMA BRG
	len += snprintf(str_line + len, SZ_1K, "RDMA BRG reg:\n");
	len += print_serial_reg(str_line, len, dss_base +
		g_dss_module_eng_base[DSS_ENG_DPE2][MODULE_ENG_DMA_BRG], 0x1C, 0x2C);

	len += print_serial_reg(str_line, len, dss_base +
		g_dss_module_eng_base[DSS_ENG_DPE3][MODULE_ENG_DMA_BRG], 0x1C, 0x2C);

	for(i = 0; i < req->layer_nums; i++){
		int chn = req->layer_infos[i].chn_idx;
		len += snprintf(str_line + len, SZ_1K, "layer:%d chn:%d\n", i, chn);
		//RDMA
		len += snprintf(str_line + len, SZ_1K, "RDMA reg:\n");
		len += print_serial_reg(str_line, len, dss_base +
			g_dss_module_base[chn][MODULE_DMA0], 0x04, 0x30);

		if( isYUVSemiPlanar(req->layer_infos[i].src.format))
			len += print_serial_reg(str_line, len, dss_base +
				g_dss_module_base[chn][MODULE_DMA1], 0x04, 0x30);

		if(isYUVPlanar(req->layer_infos[i].src.format))
			len += print_serial_reg(str_line, len, dss_base +
				g_dss_module_base[chn + 1][MODULE_DMA0], 0x04, 0x30);

		//RDFC
		len += snprintf(str_line + len, SZ_1K, "RDFC reg:\n");
		len += print_serial_reg(str_line, len, dss_base +
			g_dss_module_base[chn][MODULE_DFC], 0x0, 0x24);

		//CSC
		len += snprintf(str_line + len, SZ_1K, "CSC reg:\n");
		len += print_serial_reg(str_line, len, dss_base +
			g_dss_module_base[chn][MODULE_CSC], 0x1C, 0x1C);

		//SCF
		if(req->layer_infos[i].need_cap & CAP_SCL){
			len += snprintf(str_line + len, SZ_1K, "SCF reg:\n");
			len += print_serial_reg(str_line, len, dss_base +
				g_dss_module_base[chn][MODULE_SCF], 0x10, 0x14);
		}
	}

	//WDMA
	len += snprintf(str_line + len, SZ_1K, "WDMA reg:\n");
	if(WBE1_CHN0 == req->wb_layer_info.chn_idx){
		len += print_serial_reg(str_line, len, dss_base +
			g_dss_module_base[WBE1_CHN0][MODULE_DMA0], 0x04, 0x30);

		len += snprintf(str_line + len, SZ_1K, "WDFC reg:\n");
		len += print_serial_reg(str_line, len, dss_base +
			g_dss_module_base[WBE1_CHN0][MODULE_DFC], 0x0, 0x24);
	}else if(WBE1_CHN1 == req->wb_layer_info.chn_idx) {
		len += print_serial_reg(str_line, len, dss_base +
			g_dss_module_base[WBE1_CHN1][MODULE_DMA0], 0x04, 0x30);

		len += snprintf(str_line + len, SZ_1K, "WDFC reg:\n");
		len += print_serial_reg(str_line, len, dss_base +
			g_dss_module_base[WBE1_CHN1][MODULE_DFC], 0x0, 0x24);

		//CSC
		len += snprintf(str_line + len, SZ_1K, "W_CSC reg:\n");
		len += print_serial_reg(str_line, len, dss_base +
			g_dss_module_base[WBE1_CHN1][MODULE_CSC], 0x1C, 0x1C);

		len += snprintf(str_line + len, SZ_1K, "OV reg:\n");
		len += print_serial_reg(str_line, len, dss_base +
			g_dss_module_ovl_base[DSS_OVL_ADP][MODULE_OVL_BASE], 0x0, 0x150);
	}

	outp32(dss_base + 0x404, 0x2);

	offline_save_bin_file(filename, str_line, len);
	kfree(str_line);

	return;
}

static void offline_fail_proccess(struct k3_fb_data_type *k3fd, u32 wbe_chn, int block_num)
{
	dss_overlay_t *pov_req = NULL;
	char __iomem *dss_base = NULL;
	u32 i = 0;
	u32 ready = 0;

	BUG_ON(k3fd == NULL);
	pov_req = &(k3fd->ov_req);
	BUG_ON(wbe_chn >= K3_DSS_OFFLINE_MAX_NUM);
	BUG_ON(pov_req == NULL);
	dss_base = k3fd->dss_base;

	if(g_debug_ovl_offline_composer == 1) {
		u32 test1 = inp32(k3fd->dss_base + DSS_CMD_LIST_OFFSET +CMDLIST_CH5_STATUS);
		K3_FB_ERR("offline fail cmdlist status:0x%x\n", test1);
		cmdlist_print_all_node(&k3fd->offline_cmdlist_head[wbe_chn]);
	}

	/*single channel fail.*/
	cmdlist_config_stop(k3fd, wbe_chn);

	for (i = 0; i < block_num; i++) {
		char __iomem *ctl_base = k3fd->dss_base + DSS_GLB_WBE1_CH0_CTL +
				wbe_chn * (DSS_GLB_WBE1_CH1_CTL - DSS_GLB_WBE1_CH0_CTL);
		if(inp32(dss_base + DSS_GLB_GLB_STATUS) & (0x1 << (2+wbe_chn))) {
			set_reg(ctl_base, 0x1, 1, 8);
			set_reg(ctl_base, 0x0, 1, 8);
			udelay(10);
		}
		set_reg(ctl_base, 0x1, 1, 8);
		set_reg(ctl_base, 0x0, 1, 8);
	}

	for (i = 0; i < pov_req->layer_nums; i++) {
		int chn_idx = pov_req->layer_infos[i].chn_idx - DPE2_CHN0;
		int rot_idx = 0;

		if(chn_idx > DPE3_CHN3 - DPE2_CHN0 || chn_idx < 0){
			K3_FB_ERR("offline_add_virtual_frame  chn:%d err\n", pov_req->layer_infos[i].chn_idx);
			return;
		}

		set_reg(dss_base +DSS_GLB_DPE2_CH0_CTL + chn_idx * 0x04, 0, 32, 0);

		if(isYUVPlanar(pov_req->layer_infos[i].src.format))
			set_reg(dss_base +DSS_GLB_DPE2_CH0_CTL + (chn_idx + 1) * 0x04, 0, 32, 0);

		rot_idx = k3_get_rot_index(pov_req->layer_infos[i].chn_idx);
		if(rot_idx > 0){
			set_reg(dss_base + DSS_GLB_ROT_TLB0_SCENE + rot_idx * 4, 0, 32, 0);
		}
	}

	set_reg(dss_base + g_dss_module_base[pov_req->wb_layer_info.chn_idx][MODULE_DMA0]
		+ WDMA_CTRL, 0, 1, 25);
	if(pov_req->wb_layer_info.dst.mmu_enable)
		set_reg(dss_base + g_dss_module_base[pov_req->wb_layer_info.chn_idx][MODULE_MMU_DMA0], 0, 1, 0);

	if(pov_req->wb_layer_info.chn_idx == WBE1_CHN1){

		set_reg(dss_base +DSS_GLB_OV2_SCENE, 0, 32, 0);
	}

	//start
	set_reg(dss_base + DSS_GLB_OFFLINE_S2_CPU_IRQ_CLR, 0x44 << wbe_chn, 32, 0);
	set_reg(dss_base + DSS_GLB_ADP_OFFLINE_START0 + wbe_chn * 0x04, 0x1, 32, 0);

	// wait start irq
	ready = 0;
	i=0;
	do {
		udelay(10);
		i++;
		if(inp32(dss_base + DSS_GLB_OFFLINE_S2_CPU_IRQ_RAWSTAT) & BIT(2 + wbe_chn))
			ready = 1;
	} while((!ready) & (i < 100));

	if(!ready) {
		K3_FB_ERR("offline wait start irq timeout:0x%x,i=%d\n",
			inp32(dss_base + DSS_GLB_OFFLINE_S2_CPU_IRQ_RAWSTAT), i);

		if(g_debug_ovl_offline_composer > 0)
			BUG_ON(1);
	}

	k3fd->offline_wb_status[wbe_chn] = e_status_idle;

	return;
}

int k3_ov_offline_play(struct k3_fb_data_type *k3fd, unsigned long *argp)
{
	char __iomem *dss_base = NULL;
	dss_overlay_t *pov_req = NULL;
	dss_layer_t *layer = NULL;
	dss_wb_layer_t *wb_layer = NULL;
	int i = 0;
	int k = 0;
	int ret = 0;
	struct fb_info *fbi = NULL;
	int block_num = 0;
	dss_rect_t wb_block_rect;
	struct timeval tv;
	u32 flag;
	u32 wbe_chn = 0;
	bool first_valid_block = true;

	BUG_ON(k3fd == NULL);
	fbi = k3fd->fbi;
	BUG_ON(fbi == NULL);
	pov_req = &(k3fd->ov_req);
	BUG_ON(pov_req == NULL);

	dss_base = k3fd->dss_base;

	do_gettimeofday(&tv);
	flag = tv.tv_usec;

	/*lock for k3fd data*/
	down(&k3fd->ov_wb_sem);

	ret = copy_from_user(pov_req, argp, sizeof(dss_overlay_t));
	if (ret) {
		K3_FB_ERR("copy_from_user failed!\n");
		goto err_nodump;
	}

	wb_layer = &(k3fd->ov_req.wb_layer_info);
	wbe_chn = wb_layer->chn_idx - WBE1_CHN0;
	if(wbe_chn >= K3_DSS_OFFLINE_MAX_NUM){
		K3_FB_ERR("write back chn:%d not surport!\n",wb_layer->chn_idx);
		goto err_nodump;
	}

	if (fbi->fbops->fb_blank)
		fbi->fbops->fb_blank(FB_BLANK_UNBLANK, fbi);

	/*in case of single channel fail.*/
	if (k3fd->offline_wb_status[wbe_chn] == e_status_idle) {
		cmdlist_config_start(k3fd, wbe_chn);
		k3fd->offline_wb_status[wbe_chn] = e_status_wait;
	}

	ret = get_block_rect(pov_req, (wb_layer->dst_rect.x + wb_layer->dst_rect.w),
			(wb_layer->dst_rect.y + wb_layer->dst_rect.h), &block_num, k3fd->block_rects);
	if ((ret != 0) || (block_num == 0)|| block_num >= K3_DSS_OFFLINE_MAX_BLOCK) {
		K3_FB_ERR("get_block_rect failed! ret = %d, block_num[%d]\n", ret, block_num);
		goto err_return;
	}

	for (k = 0; k < block_num; k++) {
		ret = get_block_layers(pov_req, *k3fd->block_rects[k], k3fd->block_overlay);
		if (ret != 0) {
			K3_FB_ERR("get_block_layers err ret = %d\n", ret);
			goto err_return;
		}

		ret = rect_across_rect(*k3fd->block_rects[k], wb_layer->src_rect, &wb_block_rect);
		if (ret == 0) {
			K3_FB_ERR("no cross! block_rects[%d]{%d %d %d %d}, wb src_rect{%d %d %d %d}\n", k,
				k3fd->block_rects[k]->x, k3fd->block_rects[k]->y,
				k3fd->block_rects[k]->w, k3fd->block_rects[k]->h,
				wb_layer->src_rect.x, wb_layer->src_rect.y, wb_layer->src_rect.w, wb_layer->src_rect.h);
			continue;
		}

		if (true == first_valid_block) {
			ret = k3_dss_module_init(k3fd);
			if (ret != 0) {
				K3_FB_ERR("k3_dss_module_init failed! ret = %d\n", ret);
				goto err_return;
			}
		}

		if(g_debug_ovl_offline_cmdlist - 1 == k){
			ret = cmdlist_add_new_list(k3fd, &k3fd->offline_cmdlist_head[wbe_chn], TRUE, flag);
		}else{
			ret = cmdlist_add_new_list(k3fd, &k3fd->offline_cmdlist_head[wbe_chn], FALSE, flag);
		}
		if(ret != 0){
			K3_FB_ERR("cmdlist_add_new_list err:%d \n",ret);
			return ret;
		}

		if (true == first_valid_block) {
			offline_stop_glb(k3fd, wbe_chn);
			k3_dss_scf_coef_load(k3fd);
			first_valid_block = false;
		}

		k3_adp_offline_start_disable(k3fd, pov_req);

		if (g_debug_ovl_offline_composer == 1) {
			K3_FB_INFO("dump block_overlay:\n");

			K3_FB_INFO("{%d %d %d %d} cross {%d %d %d %d} = {%d %d %d %d}\n",
				k3fd->block_rects[k]->x, k3fd->block_rects[k]->y,
				k3fd->block_rects[k]->w, k3fd->block_rects[k]->h,
				wb_layer->src_rect.x, wb_layer->src_rect.y,
				wb_layer->src_rect.w, wb_layer->src_rect.h,
				wb_block_rect.x, wb_block_rect.y,
				wb_block_rect.w, wb_block_rect.h);
		}

		k3_dss_handle_cur_ovl_req(k3fd, k3fd->block_overlay);
		k3_dss_handle_cur_ovl_req_wb(k3fd, k3fd->block_overlay);

		ret = k3_dss_ovl_base_config(k3fd, &wb_block_rect, k3fd->ov_req.ovl_flags);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_ovl_init failed! ret = %d\n", ret);
			goto err_return;
		}

		ret = k3_dss_rdma_bridge_config(k3fd, k3fd->block_overlay);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_rdma_bridge_config failed! ret = %d\n", ret);
			goto err_return;
		}

		/* Go through all layers */
		for (i = 0; i < k3fd->block_overlay->layer_nums; i++) {
			layer = &k3fd->block_overlay->layer_infos[i];

			ret = k3_dss_offline_one_layer_config(k3fd, layer, &wb_block_rect);
			if (ret != 0) {
				K3_FB_ERR("k3_dss_offline_one_layer_config failed, ret = %d\n", ret);
				goto err_return;
			}
		}

		ret = k3_dss_write_back_config(k3fd, wb_layer, &wb_block_rect);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_offline_one_layer_config failed, ret = %d\n", ret);
			goto err_return;
		}

		ret = k3_dss_module_config(k3fd);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_module_config failed! ret = %d\n", ret);
			goto err_return;
		}

		k3_adp_offline_start_enable(k3fd, k3fd->block_overlay);

		if (k < (block_num - 1)) {
			ret = k3_dss_module_init(k3fd);
			if (ret != 0) {
				K3_FB_ERR("k3_dss_module_init failed! ret = %d\n", ret);
				goto err_return;
			}

			k3_dss_handle_prev_ovl_req(k3fd, k3fd->block_overlay);
			k3_dss_handle_prev_ovl_req_wb(k3fd, k3fd->block_overlay);
		}
	}

	ret = offline_add_pending_frame(k3fd, wbe_chn, flag);
	if (ret != 0) {
		K3_FB_ERR("offline_add_virtual_frame failed! ret = %d\n", ret);
		goto err_return;
	}

	cmdlist_frame_valid(&k3fd->offline_cmdlist_head[wbe_chn]);

	cmdlist_add_nop_list(k3fd, &k3fd->offline_cmdlist_head[wbe_chn], FALSE);

#ifdef CONFIG_BUF_SYNC_USED
	for (i = 0; i < pov_req->layer_nums; i++) {
		if (pov_req->layer_infos[i].acquire_fence >= 0){
			k3fb_buf_sync_wait(pov_req->layer_infos[i].acquire_fence);
		}
	}
#endif

	//flush cache before start frame
	cmdlist_flush_cmdlistmemory_cache(&k3fd->offline_cmdlist_head[wbe_chn], k3fd->ion_client);

	cmdlist_start_frame(k3fd, wbe_chn);

	/*unlock for k3fd data*/
	up(&k3fd->ov_wb_sem);

	/*lock if use dpe3 chn0 and chn1*/
	offline_chn_lock(k3fd);

	ret = wait_event_interruptible_timeout(k3fd->offline_writeback_wq[wbe_chn],
			(k3fd->offline_wb_done[wbe_chn] == flag), msecs_to_jiffies(OFFLINE_COMPOSE_TIMEOUT));

	/*unlock if use dpe3 chn0 and chn1*/
	offline_chn_unlock(k3fd);

	/*lock for k3fd data*/
	down(&k3fd->ov_wb_sem);

	cmdlist_prepare_next_frame(k3fd, wbe_chn);
	cmdlist_free_nop(&k3fd->offline_cmdlist_head[wbe_chn], k3fd->ion_client);

	if (ret <= 0) {
		ret = -ETIMEDOUT;

		if(g_debug_ovl_offline_composer > 0)
			offline_dump_fail_reg(dss_base, pov_req, tv.tv_sec);

		offline_fail_proccess(k3fd, wbe_chn, block_num);
		goto err_return;
	}

	ret = 0;

err_return:
	if (ret != 0 && g_debug_ovl_offline_composer > 0) {
		do_gettimeofday(&tv);
		offline_dump_fail_info_to_file(pov_req, tv.tv_sec);
	}
err_nodump:
	cmdlist_free_frame(&k3fd->offline_cmdlist_head[wbe_chn], k3fd->ion_client);

	k3_dss_rptb_handler(k3fd, false, wbe_chn);
	k3_dss_rptb_handler(k3fd, true, wbe_chn);

	if(g_debug_ovl_offline_composer == 3) {
		do_gettimeofday(&tv);
		offline_dump_fail_info_to_file(pov_req, tv.tv_sec);
		g_debug_ovl_offline_composer = 0;
	}

	if(first_valid_block == false)
		k3_dss_scf_coef_unload(k3fd);

	if ((k3fd->offline_wb_status[0] <= e_status_wait) &&
		(k3fd->offline_wb_status[1] <= e_status_wait) &&
		fbi->fbops->fb_blank &&
		!g_debug_dss_adp_sr) {
		fbi->fbops->fb_blank(FB_BLANK_POWERDOWN, fbi);
	}

	/*unlock for k3fd data*/
	up(&k3fd->ov_wb_sem);

	if(g_debug_ovl_offline_composer == 2) {
		struct timeval tv_last;
		u32 use_time = 0;
		do_gettimeofday(&tv_last);
		use_time = (tv_last.tv_sec - tv.tv_sec) * 1000 + (tv_last.tv_usec - tv.tv_usec) /1000;
		printk("--use:%d ms \n",use_time);
	}
	return ret;
}
