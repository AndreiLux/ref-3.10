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

extern struct dsm_client *lcd_dclient;
/* rptb gen pool */
static struct gen_pool * g_rptb_gen_pool;
static int g_scf_coef_load_refcount;
u32 frame_count = 0;

u32 g_dss_module_base[DSS_CHN_MAX][MODULE_CHN_MAX] = {
	/******************DPE0*******************/
	/* DPE0 CH0 */
	{DSS_DPE0_OFFSET + DSS_DPE_RDMA0_OFFSET,
	  DSS_DPE0_OFFSET + DSS_DPE_RDMA1_OFFSET,
	  0,
	  0,
	  DSS_DPE0_OFFSET + DSS_DPE_RDFC0_OFFSET,
	  DSS_DPE0_OFFSET + DSS_SCF0_OFFSET,
	  DSS_DPE0_OFFSET + DSS_DPE_SCP_OFFSET,
	  DSS_DPE0_OFFSET + DSS_DPE_CSC0_OFFSET,
	  DSS_DPE0_OFFSET + DSS_MMU_RDMA0_OFFSET,
	  DSS_DPE0_OFFSET + DSS_MMU_RDMA1_OFFSET,
	  DSS_GLB2_OFFSET + DSS_MMU_RTLB_OFFSET,
	  DSS_GLB_DPE0_CH0_CTL,
	  DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET + DPE0_CH1_MUX,
	  DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET,
	  DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET + ROT0_PM_CTRL,
	  0,
	  0,
	  DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET + DPE0_SWITCH},
	/* DPE0 CH1 */
	{DSS_DPE0_OFFSET + DSS_DPE_RDMA2_OFFSET,
	  0,
	  0,
	  0,
	  DSS_DPE0_OFFSET + DSS_DPE_RDFC1_OFFSET,
	  DSS_DPE0_OFFSET + DSS_SCF0_OFFSET,
	  DSS_DPE0_OFFSET + DSS_DPE_SCP_OFFSET,
	  DSS_DPE0_OFFSET + DSS_DPE_CSC1_OFFSET,
	  DSS_DPE0_OFFSET + DSS_MMU_RDMA2_OFFSET,
	  0,
	  0,
	  DSS_GLB_DPE0_CH1_CTL,
	  DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET + DPE0_CH1_MUX,
	  DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  0,
	  DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET + DPE0_SWITCH},
	/* DPE0 CH2 */
	{DSS_DPE0_OFFSET + DSS_DPE_RDMA3_OFFSET,
	  0,
	  0,
	  0,
	  DSS_DPE0_OFFSET + DSS_DPE_RDFC2_OFFSET,
	  0,
	  0,
	  DSS_DPE0_OFFSET + DSS_DPE_CSC2_OFFSET,
	  DSS_DPE0_OFFSET + DSS_MMU_RDMA3_OFFSET,
	  0,
	  0,
	  DSS_GLB_DPE0_CH2_CTL,
	  0,
	  DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  0,
	  0},
	/* DPE0 CH3 */
	{DSS_DPE0_OFFSET + DSS_DPE_RDMA4_OFFSET,
	  0,
	  DSS_DPE0_OFFSET + DSS_DPE_FBDC_OFFSET,
	  0,
	  DSS_DPE0_OFFSET + DSS_DPE_RDFC3_OFFSET,
	  0,
	  0,
	  DSS_DPE0_OFFSET + DSS_DPE_CSC3_OFFSET,
	  DSS_DPE0_OFFSET + DSS_MMU_RDMA4_OFFSET,
	  0,
	  0,
	  DSS_GLB_DPE0_CH3_CTL,
	  0,
	  DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  0,
	  0},
	/* DPE0 CH4 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},


	/******************DPE1*******************/
	/* DPE1 CH0 */
	{DSS_DPE1_OFFSET + DSS_DPE_RDMA0_OFFSET,
	  DSS_DPE1_OFFSET + DSS_DPE_RDMA1_OFFSET,
	  0,
	  0,
	  DSS_DPE1_OFFSET + DSS_DPE_RDFC0_OFFSET,
	  DSS_WBE0_OFFSET + DSS_SCF1_OFFSET,
	  DSS_DPE1_OFFSET + DSS_DPE_SCP_OFFSET,
	  DSS_DPE1_OFFSET + DSS_DPE_CSC0_OFFSET,
	  DSS_DPE1_OFFSET + DSS_MMU_RDMA0_OFFSET,
	  DSS_DPE1_OFFSET + DSS_MMU_RDMA1_OFFSET,
	  0,
	  DSS_GLB_DPE1_CH0_CTL,
	  DSS_DPE1_OFFSET + DSS_DP_CTRL_OFFSET + DPE1_CH0_MUX,
	  DSS_DPE1_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  DSS_WBE0_OFFSET + DSS_DP_CTRL_OFFSET + WBE0_SCF1_SEL,
	  0},
	/* DPE1 CH1 */
	{DSS_DPE1_OFFSET + DSS_DPE_RDMA4_OFFSET,
	  0,
	  0,
	  0,
	  DSS_DPE1_OFFSET + DSS_DPE_RDFC3_OFFSET,
	  0,
	  0,
	  DSS_DPE1_OFFSET + DSS_DPE_CSC3_OFFSET,
	  DSS_DPE1_OFFSET + DSS_MMU_RDMA2_OFFSET,
	  0,
	  0,
	  DSS_GLB_DPE1_CH3_CTL,
	  0,
	  DSS_DPE1_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  0,
	  0},
	/* DPE1 CH2 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},


	/******************DPE2*******************/
	/* DPE2 CH0 */
	{DSS_DPE2_OFFSET + DSS_DPE_RDMA0_OFFSET,
	  DSS_DPE2_OFFSET + DSS_DPE_RDMA1_OFFSET,
	  0,
	  0,
	  DSS_DPE2_OFFSET + DSS_DPE_RDFC0_OFFSET,
	  DSS_DPE2_OFFSET + DSS_SCF2_OFFSET,
	  DSS_DPE2_OFFSET + DSS_DPE_SCP_OFFSET,
	  DSS_DPE2_OFFSET + DSS_DPE_CSC0_OFFSET,
	  DSS_DPE2_OFFSET + DSS_MMU_RDMA0_OFFSET,
	  DSS_DPE2_OFFSET + DSS_MMU_RDMA1_OFFSET,
	  DSS_GLB2_OFFSET + DSS_MMU_RTLB_OFFSET,
	  DSS_GLB_DPE2_CH0_CTL,
	  DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET + DPE2_CH0_MUX,
	  DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET,
	  DSS_GLB_ROT1_PM_CTRL,
	  DSS_GLB_ROT1_SEL,
	  0,
	  DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET + DPE2_SWITCH},
	/* DPE2 CH1 */
	{DSS_DPE2_OFFSET + DSS_DPE_RDMA2_OFFSET,
	  0,
	  DSS_DPE2_OFFSET + DSS_DPE_FBDC_OFFSET,
	  0,
	  DSS_DPE2_OFFSET + DSS_DPE_RDFC1_OFFSET,
	  DSS_DPE2_OFFSET + DSS_SCF2_OFFSET,
	  DSS_DPE2_OFFSET + DSS_DPE_SCP_OFFSET,
	  DSS_DPE2_OFFSET + DSS_DPE_CSC1_OFFSET,
	  DSS_DPE2_OFFSET + DSS_MMU_RDMA2_OFFSET,
	  0,
	  0,
	  DSS_GLB_DPE2_CH1_CTL,
	  DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET + DPE2_CH1_MUX,
	  DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  0,
	  DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET + DPE2_SWITCH},
	/* DPE2 CH2 */
	{DSS_DPE2_OFFSET + DSS_DPE_RDMA3_OFFSET,
	  0,
	  0,
	  0,
	  DSS_DPE2_OFFSET + DSS_DPE_RDFC2_OFFSET,
	  0,
	  0,
	  DSS_DPE2_OFFSET + DSS_DPE_CSC2_OFFSET,
	  DSS_DPE2_OFFSET + DSS_MMU_RDMA3_OFFSET,
	  0,
	  0,
	  DSS_GLB_DPE2_CH2_CTL,
	  DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET + DPE2_CH2_MUX,
	  DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  0,
	  0},
	/* DPE2 CH3 */
	{DSS_DPE2_OFFSET + DSS_DPE_RDMA4_OFFSET,
	  0,
	  0,
	  0,
	  DSS_DPE2_OFFSET + DSS_DPE_RDFC3_OFFSET,
	  0,
	  0,
	  DSS_DPE2_OFFSET + DSS_DPE_CSC3_OFFSET,
	  DSS_DPE2_OFFSET + DSS_MMU_RDMA4_OFFSET,
	  0,
	  0,
	  DSS_GLB_DPE2_CH3_CTL,
	  DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET + DPE2_CH3_MUX,
	  DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  0,
	  0},


	/******************DPE3*******************/
	/* DPE3_CHN0 */
	{DSS_DPE3_OFFSET + DSS_DPE_RDMA0_OFFSET,
	  DSS_DPE3_OFFSET + DSS_DPE_RDMA1_OFFSET,
	  0,
	  0,
	  DSS_DPE3_OFFSET + DSS_DPE_RDFC0_OFFSET,
	  DSS_DPE3_OFFSET + DSS_SCF4_OFFSET,
	  DSS_DPE3_OFFSET + DSS_DPE_SCP_OFFSET,
	  DSS_DPE3_OFFSET + DSS_DPE_CSC0_OFFSET,
	  DSS_DPE3_OFFSET + DSS_MMU_RDMA0_OFFSET,
	  DSS_DPE3_OFFSET + DSS_MMU_RDMA1_OFFSET,
	  DSS_GLB2_OFFSET + DSS_MMU_RTLB_OFFSET,
	  DSS_GLB_DPE3_CH0_CTL,
	  DSS_DPE3_OFFSET + DSS_DP_CTRL_OFFSET + DPE3_CH0_MUX,
	  DSS_DPE3_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  0,
	  0},
	/* DPE3_CHN1 */
	{DSS_DPE3_OFFSET + DSS_DPE_RDMA2_OFFSET,
	  0,
	  0,
	  0,
	  DSS_DPE3_OFFSET + DSS_DPE_RDFC1_OFFSET,
	  DSS_DPE3_OFFSET + DSS_SCF3_OFFSET,
	  0,
	  DSS_DPE3_OFFSET + DSS_DPE_CSC1_OFFSET,
	  DSS_DPE3_OFFSET + DSS_MMU_RDMA2_OFFSET,
	  0,
	  DSS_GLB2_OFFSET + DSS_MMU_RTLB_OFFSET,
	  DSS_GLB_DPE3_CH1_CTL,
	  0,
	  DSS_DPE3_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  DSS_DPE3_OFFSET + DSS_DP_CTRL_OFFSET + ADP_ROT2_SEL,
	  DSS_DPE3_OFFSET + DSS_DP_CTRL_OFFSET + ADP_SCF3_SEL,
	  0},
	/* DPE3_CHN2 */
	{DSS_DPE3_OFFSET + DSS_DPE_RDMA3_OFFSET,
	  0,
	  0,
	  0,
	  DSS_DPE3_OFFSET + DSS_DPE_RDFC2_OFFSET,
	  0,
	  0,
	  DSS_DPE3_OFFSET + DSS_DPE_CSC2_OFFSET,
	  DSS_DPE3_OFFSET + DSS_MMU_RDMA3_OFFSET,
	  0,
	  DSS_GLB2_OFFSET + DSS_MMU_RTLB_OFFSET,
	  DSS_GLB_DPE3_CH2_CTL,
	  0,
	  DSS_DPE3_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  0,
	  0},
	/* DPE3_CHN3 */
	{DSS_DPE3_OFFSET + DSS_DPE_RDMA4_OFFSET,
	  0,
	  DSS_DPE3_OFFSET + DSS_DPE_FBDC_OFFSET,
	  0,
	  DSS_DPE3_OFFSET + DSS_DPE_RDFC3_OFFSET,
	  0,
	  0,
	  DSS_DPE3_OFFSET + DSS_DPE_CSC3_OFFSET,
	  DSS_DPE3_OFFSET + DSS_MMU_RDMA4_OFFSET,
	  0,
	  0,
	  DSS_GLB_DPE3_CH3_CTL,
	  0,
	  DSS_DPE3_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  0,
	  0},


	/******************WBE0*******************/
	/* WBE0_CHN0 */
	{DSS_WBE0_OFFSET + DSS_WBE_WDMA0_OFFSET,
	  0,
	  0,
	  0,
	  DSS_WBE0_OFFSET + DSS_WBE_WDFC0_OFFSET,
	  0,
	  0,
	  0,
	  DSS_WBE0_OFFSET + DSS_MMU_WDMA0_OFFSET,
	  0,
	  0,
	  DSS_GLB_WBE0_CH0_CTL,
	  0,
	  DSS_WBE0_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  0,
	  0},
	/* WBE0_CHN1 */
	{DSS_WBE0_OFFSET + DSS_WBE_WDMA1_OFFSET,
	  0,
	  0,
	  DSS_WBE0_OFFSET + DSS_WBE_FBC0_OFFSET,
	  DSS_WBE0_OFFSET + DSS_WBE_WDFC1_OFFSET,
	  DSS_WBE0_OFFSET + DSS_SCF1_OFFSET,
	  0,
	  DSS_WBE0_OFFSET + DSS_WBE_CSC1_OFFSET,
	  DSS_WBE0_OFFSET + DSS_MMU_WDMA1_OFFSET,
	  0,
	  DSS_GLB2_OFFSET + DSS_MMU_RTLB_OFFSET,
	  DSS_GLB_WBE0_CH1_CTL,
	  DSS_WBE0_OFFSET + DSS_DP_CTRL_OFFSET + WBE0_MUX,
	  DSS_WBE0_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  DSS_GLB_ROT1_SEL,
	  DSS_WBE0_OFFSET + DSS_DP_CTRL_OFFSET + WBE0_SCF1_SEL,
	  0},


	/******************WBE1*******************/
	/* WBE1_CHN0 */
	{DSS_WBE1_OFFSET + DSS_WBE_WDMA0_OFFSET,
	  0,
	  0,
	  0,
	  DSS_WBE1_OFFSET + DSS_WBE_WDFC0_OFFSET,
	  0,
	  0,
	  DSS_DPE3_OFFSET + DSS_DPE_CSC0_OFFSET,
	  DSS_WBE1_OFFSET + DSS_MMU_WDMA0_OFFSET,
	  0,
	  0,
	  DSS_GLB_WBE1_CH0_CTL,
	  0,
	  DSS_WBE1_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  0,
	  0,
	  0},
	/* WBE1_CHN1 */
	{DSS_WBE1_OFFSET + DSS_WBE_WDMA1_OFFSET,
	  0,
	  0,
	  DSS_WBE1_OFFSET + DSS_WBE_FBC0_OFFSET,
	  DSS_WBE1_OFFSET + DSS_WBE_WDFC1_OFFSET,
	  DSS_WBE1_OFFSET + DSS_SCF3_OFFSET,
	  0,
	  DSS_WBE1_OFFSET + DSS_WBE_CSC1_OFFSET,
	  DSS_WBE1_OFFSET + DSS_MMU_WDMA1_OFFSET,
	  0,
	  DSS_GLB2_OFFSET + DSS_MMU_RTLB_OFFSET,
	  DSS_GLB_WBE1_CH1_CTL,
	  0,
	  DSS_WBE1_OFFSET + DSS_DP_CTRL_OFFSET,
	  0,
	  DSS_DPE3_OFFSET + DSS_DP_CTRL_OFFSET + ADP_ROT2_SEL,
	  DSS_DPE3_OFFSET + DSS_DP_CTRL_OFFSET + ADP_SCF3_SEL,
	  0},
};

u32 g_dss_module_ovl_base[DSS_OVL_MAX][MODULE_OVL_MAX] = {
	/* OVL PDP */
	{DSS_OVL0_OFFSET,
	DSS_GLB_OV0_SCENE,
	DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET + OV0_MUX,
	DSS_GLB_PDP_ONLINE_CFG_OK,
	DSS_DPE0_OFFSET + DFS_WBE_EN},
	/* OVL SDP */
	{DSS_OVL1_OFFSET,
	DSS_GLB_OV1_SCENE,
	DSS_DPE1_OFFSET + DSS_DP_CTRL_OFFSET + OV1_MUX,
	DSS_GLB_SDP_ONLINE_CFG_OK,
	DSS_DPE1_OFFSET + DFS_WBE_EN},
	/* OVL ADP */
	{DSS_OVL2_OFFSET,
	DSS_GLB_OV2_SCENE,
	DSS_DPE3_OFFSET + DSS_DP_CTRL_OFFSET + OV2_MUX,
	0,
	0},
};

u32 g_dss_module_eng_base[DSS_ENG_MAX][MODULE_ENG_MAX] = {
	/* DPE0 */
	{DSS_DPE0_OFFSET,
	DSS_DPE0_OFFSET + DSS_DPE_RDMA_BRG_OFFSET,
	DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET},
	/* DPE1 */
	{DSS_DPE1_OFFSET,
	DSS_DPE1_OFFSET + DSS_DPE_RDMA_BRG_OFFSET,
	DSS_DPE1_OFFSET + DSS_DP_CTRL_OFFSET},
	/* DPE2 */
	{DSS_DPE2_OFFSET,
	DSS_DPE2_OFFSET + DSS_DPE_RDMA_BRG_OFFSET,
	DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET},
	/* DPE3 */
	{DSS_DPE3_OFFSET,
	DSS_DPE3_OFFSET + DSS_DPE_RDMA_BRG_OFFSET,
	DSS_DPE3_OFFSET + DSS_DP_CTRL_OFFSET},

	/* WBE0 */
	{DSS_WBE0_OFFSET,
	DSS_WBE_WDMA_BRG0_OFFSET,
	DSS_WBE0_OFFSET + DSS_DP_CTRL_OFFSET},
	/* WBE1 */
	{DSS_WBE1_OFFSET,
	DSS_WBE_WDMA_BRG1_OFFSET,
	DSS_WBE1_OFFSET + DSS_DP_CTRL_OFFSET},
};


/*******************************************************************************
**
*/
void dumpDssOverlay(dss_overlay_t *ov_data)
{
	uint32_t j = 0;
	dss_layer_t const *dssLayerInfo = NULL;

	BUG_ON(ov_data == NULL);

	printk("\n");
	printk("layer_nums = %d\n", ov_data->layer_nums);

	for (j = 0; j < ov_data->layer_nums; j++) {
		dssLayerInfo = &ov_data->layer_infos[j];

		printk("\n");
		printk("    LayerInfo[%d]:\n", j);

		printk("\t  flags | layer_idx | chn_idx | transform | blending | alpha |   color  | need_cap \n");
		printk("\t--------+-----------+---------+-----------+----------+-------+------------+----------\n");
		printk("\t %6u | %9d | %7d | %9d | %8u | %5d | 0x%8x | %8u \n",
			dssLayerInfo->flags, dssLayerInfo->layer_idx, dssLayerInfo->chn_idx,
			dssLayerInfo->transform, dssLayerInfo->blending, dssLayerInfo->glb_alpha,
			dssLayerInfo->color, dssLayerInfo->need_cap);

		printk("\n");
		printk("\t format |  width | height |  bpp   | stride |  phy_addr  |  vir_addr  | uStride | uOffset | vStride | vOffset \n");
		printk("\t--------+--------+--------+--------+--------+------------+------------+---------+---------+---------+---------\n");
		printk("\t %6u | %6u | %6u | %6u | %6u |  0x%8x  |  0x%8x  | %7u | %7u | %7u | %7u \n",
			dssLayerInfo->src.format, dssLayerInfo->src.width, dssLayerInfo->src.height, dssLayerInfo->src.bpp,
			dssLayerInfo->src.stride, dssLayerInfo->src.phy_addr, dssLayerInfo->src.vir_addr, dssLayerInfo->src.uStride,
			dssLayerInfo->src.vStride, dssLayerInfo->src.uOffset, dssLayerInfo->src.vOffset);

		printk("\n");
		printk("\t         src_rect         |          dst_rect         |       src_rect_mask       \n");
		printk("\t--------------------------+---------------------------+---------------------------\n");
		printk("\t[%5d,%5d,%5d,%5d] | [%5d,%5d,%5d,%5d] | [%5d,%5d,%5d,%5d] \n",
			dssLayerInfo->src_rect.x, dssLayerInfo->src_rect.y,
			dssLayerInfo->src_rect.w, dssLayerInfo->src_rect.h,
			dssLayerInfo->dst_rect.x, dssLayerInfo->dst_rect.y,
			dssLayerInfo->dst_rect.w, dssLayerInfo->dst_rect.h,
			dssLayerInfo->src_rect_mask.x, dssLayerInfo->src_rect_mask.y,
			dssLayerInfo->src_rect_mask.w, dssLayerInfo->src_rect_mask.h);
	}

	printk("\n");

	if (!ov_data->wb_enable)
		return;

	printk("    wb info:\n");

	printk("\n");
	printk("\t format |  width | height |  bpp   | stride |  phy_addr  |  vir_addr  | uStride | uOffset | vStride | vOffset \n");
	printk("\t--------+--------+--------+--------+--------+------------+------------+---------+---------+---------+---------\n");
	printk("\t %6u | %6u | %6u | %6u | %6u |  0x%8x  |  0x%8x  | %7u | %7u | %7u | %7u \n",
			ov_data->wb_layer_info.dst.format, ov_data->wb_layer_info.dst.width, ov_data->wb_layer_info.dst.height, ov_data->wb_layer_info.dst.bpp,
			ov_data->wb_layer_info.dst.stride, ov_data->wb_layer_info.dst.phy_addr, ov_data->wb_layer_info.dst.vir_addr, ov_data->wb_layer_info.dst.uStride,
			ov_data->wb_layer_info.dst.vStride, ov_data->wb_layer_info.dst.uOffset, ov_data->wb_layer_info.dst.vOffset);

	printk("\n");
	printk("\t         src_rect         |          dst_rect         | chn_idx |transform\n");
	printk("\t--------------------------+---------------------------|---------|---------|\n");
	printk("\t[%5d,%5d,%5d,%5d] | [%5d,%5d,%5d,%5d] |  %7d|%9d\n",
		ov_data->wb_layer_info.src_rect.x, ov_data->wb_layer_info.src_rect.y,
		ov_data->wb_layer_info.src_rect.w, ov_data->wb_layer_info.src_rect.h,
		ov_data->wb_layer_info.dst_rect.x, ov_data->wb_layer_info.dst_rect.y,
		ov_data->wb_layer_info.dst_rect.w, ov_data->wb_layer_info.dst_rect.h,
		ov_data->wb_layer_info.chn_idx, ov_data->wb_layer_info.transform);

	printk("\n");
}

int k3_dss_check_layer_par(struct k3_fb_data_type *k3fd, dss_layer_t *layer)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(layer == NULL);

	if (layer->need_cap & (CAP_BASE | CAP_DIM))
		return 0;

	if (layer->src_rect.x < 0 || layer->src_rect.y < 0 ||
		layer->src_rect.w <= 0 || layer->src_rect.h <= 0) {
		K3_FB_ERR("src_rect(%d, %d, %d, %d) is out of range!\n",
			layer->src_rect.x, layer->src_rect.y,
			layer->src_rect.w, layer->src_rect.h);
		return -EINVAL;
	}

	if (layer->dst_rect.x < 0 || layer->dst_rect.y < 0 ||
		layer->dst_rect.w <= 0 || layer->dst_rect.h <= 0) {
		K3_FB_ERR("dst_rect(%d, %d, %d, %d) is out of range!\n",
			layer->dst_rect.x, layer->dst_rect.y,
			layer->dst_rect.w, layer->dst_rect.h);
		return -EINVAL;
	}

	return 0;
}


/*******************************************************************************
**
*/
static bool isChannelBelongtoDPE(int dpe_type, int chn_idx)
{
	if (dpe_type == DSS_ENG_DPE0) {
		if (chn_idx >= DPE0_CHN0 && chn_idx <= DPE0_CHN3)
			return true;
	} else if (dpe_type == DSS_ENG_DPE1) {
		if (chn_idx >= DPE1_CHN0 && chn_idx <= DPE1_CHN1)
			return true;
	} else if (dpe_type == DSS_ENG_DPE2) {
		if (chn_idx >= DPE2_CHN0 && chn_idx <= DPE2_CHN3)
			return true;
	} else if (dpe_type == DSS_ENG_DPE3) {
		if (chn_idx >= DPE3_CHN0 && chn_idx <= DPE3_CHN3)
			return true;
	} else if (dpe_type == DSS_ENG_WBE0) {
		if (chn_idx >= WBE0_CHN0 && chn_idx <= WBE0_CHN1)
			return true;
	} else if (dpe_type ==	DSS_ENG_WBE1) {
		if (chn_idx >= WBE1_CHN0 && chn_idx <= WBE1_CHN1)
			return true;
	} else {
		K3_FB_ERR("not support this dpe_type(%d)!\n", dpe_type);
	}

	return false;
}

static bool hasChannelBelongtoDPE(int dpe_type, struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req)
{
	dss_layer_t *layer = NULL;
	int chn_idx = 0;
	int i = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);

	for (i = 0; i < pov_req->layer_nums; i++) {
		layer = &(pov_req->layer_infos[i]);
		chn_idx = layer->chn_idx;

		if (dpe_type == DSS_ENG_DPE0) {
			if (chn_idx >= DPE0_CHN0 && chn_idx <= DPE0_CHN3)
				return true;
		} else if (dpe_type == DSS_ENG_DPE1) {
			if (chn_idx >= DPE1_CHN0 && chn_idx <= DPE1_CHN1)
				return true;
		} else if (dpe_type == DSS_ENG_DPE2) {
			if (chn_idx >= DPE2_CHN0 && chn_idx <= DPE2_CHN3)
				return true;
		} else if (dpe_type == DSS_ENG_DPE3) {
			if (chn_idx >= DPE3_CHN0 && chn_idx <= DPE3_CHN3)
				return true;
		} else {
			K3_FB_ERR("not support this dpe_type(%d)!\n", dpe_type);
			break;
		}
	}

	return false;
}

int k3_get_hal_format(struct fb_info *info)
{
	struct fb_var_screeninfo *var = NULL;
	int hal_format = 0;
	int rgb_order = 0;

	BUG_ON(info == NULL);
	var = &info->var;

	switch (var->bits_per_pixel) {
	case 16:
		if (var->blue.offset == 0) {
			rgb_order = 0;
			if (var->red.offset == 8) {
				hal_format = (var->transp.offset == 12) ?
					K3_FB_PIXEL_FORMAT_BGRA_4444 : K3_FB_PIXEL_FORMAT_BGRX_4444;
			} else if (var->red.offset == 10) {
				hal_format = (var->transp.offset == 12) ?
					K3_FB_PIXEL_FORMAT_BGRA_5551 : K3_FB_PIXEL_FORMAT_BGRX_5551;
			} else if (var->red.offset == 11) {
				hal_format = K3_FB_PIXEL_FORMAT_RGB_565;
			} else {
				goto err_return;
			}
		} else {
			rgb_order = 1;
			if (var->blue.offset == 8) {
				hal_format = (var->transp.offset == 12) ?
					K3_FB_PIXEL_FORMAT_RGBA_4444 : K3_FB_PIXEL_FORMAT_RGBX_4444;
			} else if (var->blue.offset == 10) {
				hal_format = (var->transp.offset == 12) ?
					K3_FB_PIXEL_FORMAT_RGBA_5551 : K3_FB_PIXEL_FORMAT_RGBX_5551;
			} else if (var->blue.offset == 11) {
				hal_format = K3_FB_PIXEL_FORMAT_BGR_565;
			} else {
				goto err_return;
			}
		}
		break;

	case 32:
		if (var->blue.offset == 0) {
			rgb_order = 0;
			hal_format = (var->transp.length == 8) ?
				K3_FB_PIXEL_FORMAT_BGRA_8888 : K3_FB_PIXEL_FORMAT_BGRX_8888;
		} else {
			rgb_order = 1;
			hal_format = (var->transp.length == 8) ?
				K3_FB_PIXEL_FORMAT_RGBA_8888 : K3_FB_PIXEL_FORMAT_RGBA_8888;
		}
		break;

	default:
		goto err_return;
		break;
	}

	return hal_format;

err_return:
	K3_FB_ERR("not support this bits_per_pixel(%d)!\n", var->bits_per_pixel);
	return -1;
}

static bool hal_format_has_alpha(int format)
{
	switch (format) {
	case K3_FB_PIXEL_FORMAT_RGBA_4444:
	case K3_FB_PIXEL_FORMAT_RGBA_5551:
	case K3_FB_PIXEL_FORMAT_RGBA_8888:

	case K3_FB_PIXEL_FORMAT_BGRA_4444:
	case K3_FB_PIXEL_FORMAT_BGRA_5551:
	case K3_FB_PIXEL_FORMAT_BGRA_8888:
		return true;

	default:
		return false;
	}
}

bool isYUVPackage(int format)
{
	switch (format) {
	case K3_FB_PIXEL_FORMAT_YUV_422_I:
		return true;

	default:
		return false;
	}
}

bool isYUVSemiPlanar(int format)
{
	switch (format) {
	case K3_FB_PIXEL_FORMAT_YCbCr_422_SP:
	case K3_FB_PIXEL_FORMAT_YCrCb_422_SP:
	case K3_FB_PIXEL_FORMAT_YCbCr_420_SP:
	case K3_FB_PIXEL_FORMAT_YCrCb_420_SP:
		return true;

	default:
		return false;
	}
}

bool isYUVPlanar(int format)
{
	switch (format) {
	case K3_FB_PIXEL_FORMAT_YCbCr_422_P:
	case K3_FB_PIXEL_FORMAT_YCrCb_422_P:
	case K3_FB_PIXEL_FORMAT_YCbCr_420_P:
	case K3_FB_PIXEL_FORMAT_YCrCb_420_P:
		return true;

	default:
		return false;
	}
}

bool isYUV(int format)
{
	return isYUVPackage(format) ||
		isYUVSemiPlanar(format) ||
		isYUVPlanar(format);
}

static bool is_YUV_SP_420(int format)
{
	switch (format) {
	case K3_FB_PIXEL_FORMAT_YCbCr_420_SP:
	case K3_FB_PIXEL_FORMAT_YCrCb_420_SP:
		return true;

	default:
		return false;
	}
}

#if 0
static bool is_YUV_SP_422(int format)
{
	switch (format) {
	case K3_FB_PIXEL_FORMAT_YCbCr_422_SP:
	case K3_FB_PIXEL_FORMAT_YCrCb_422_SP:
		return true;

	default:
		return false;
	}
}
#endif

static bool is_YUV_P_420(int format)
{
	switch (format) {
	case K3_FB_PIXEL_FORMAT_YCbCr_420_P:
	case K3_FB_PIXEL_FORMAT_YCrCb_420_P:
		return true;

	default:
		return false;
	}
}

static bool is_YUV_P_422(int format)
{
	switch (format) {
	case K3_FB_PIXEL_FORMAT_YCbCr_422_P:
	case K3_FB_PIXEL_FORMAT_YCrCb_422_P:
		return true;

	default:
		return false;
	}
}

bool isNeedDither(int fmt)
{
	return (fmt == DFC_PIXEL_FORMAT_RGB_565) ||
		(fmt == DFC_PIXEL_FORMAT_BGR_565);
}

static bool isNeedRectClip(dss_rect_ltrb_t clip_rect)
{
	return ((clip_rect.left > 0) || (clip_rect.top > 0) ||
		(clip_rect.right > 0) || (clip_rect.bottom > 0));
}

static bool isSrcRectMasked(dss_layer_t *dss_layer)
{
	BUG_ON(dss_layer == NULL);

	return ((dss_layer->src_rect_mask.w != 0) &&
		(dss_layer->src_rect_mask.h != 0));
}

static u32 isNeedRdmaStretchBlt(struct k3_fb_data_type *k3fd, dss_layer_t *layer)
{
	u32 v_stretch_ratio_threshold = 0;
	u32 v_stretch_ratio = 0;

	BUG_ON(layer == NULL);

	//FIXME:
	if (k3fd->ovl_type == DSS_OVL_ADP)
		return 0;

	if (is_YUV_SP_420(layer->src.format) || is_YUV_P_420(layer->src.format)) {
		if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
			v_stretch_ratio_threshold = ((layer->src_rect.h + layer->dst_rect.w - 1) / layer->dst_rect.w);
			//v_stretch_ratio = ((layer->src_rect.h / layer->dst_rect.w) / 2) * 2;
			v_stretch_ratio = (((layer->src_rect.h / layer->dst_rect.w) + 1) / 2) * 2;
		} else {
			v_stretch_ratio_threshold = ((layer->src_rect.h + layer->dst_rect.h - 1) / layer->dst_rect.h);
			//v_stretch_ratio = ((layer->src_rect.h / layer->dst_rect.h) / 2) * 2;
			v_stretch_ratio = (((layer->src_rect.h / layer->dst_rect.h) + 1) / 2) * 2;
		}
	} else {
		if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
			v_stretch_ratio_threshold = ((layer->src_rect.h + layer->dst_rect.w - 1) / layer->dst_rect.w);
			v_stretch_ratio = (layer->src_rect.h / layer->dst_rect.w);
		} else {
			v_stretch_ratio_threshold = ((layer->src_rect.h + layer->dst_rect.h - 1) / layer->dst_rect.h);
			v_stretch_ratio = (layer->src_rect.h / layer->dst_rect.h);
		}
	}

	if (v_stretch_ratio_threshold <= g_scf_stretch_threshold)
		v_stretch_ratio = 0;

	return v_stretch_ratio;
}

static bool is_need_rdma_rot_burst4(dss_layer_t *layer)
{
	BUG_ON(layer == NULL);

	if ((layer->chn_idx >= DPE3_CHN0) && (layer->chn_idx <= DPE3_CHN3) &&
		(layer->transform & K3_FB_TRANSFORM_ROT_90) &&
		(!layer->src.is_tile) && (layer->src.bpp == 4)) {
		return true;
	}

	return false;
}

static void k3_adjust_clip_rect(dss_layer_t *layer, dss_rect_ltrb_t *clip_rect)
{
	uint32_t temp = 0;

	switch (layer->transform) {
	case K3_FB_TRANSFORM_NOP:
		// do nothing
		break;
	case K3_FB_TRANSFORM_FLIP_H:
		{
			temp = clip_rect->top;
			clip_rect->top = clip_rect->bottom;
			clip_rect->bottom = temp;
		}
		break;
	case K3_FB_TRANSFORM_FLIP_V:
		{
			temp = clip_rect->left;
			clip_rect->left = clip_rect->right;
			clip_rect->right = temp;
		}
		break;
	case K3_FB_TRANSFORM_ROT_90:
		{
			temp = clip_rect->left;
			clip_rect->left = clip_rect->bottom;
			clip_rect->bottom = clip_rect->right;
			clip_rect->right = clip_rect->top;
			clip_rect->top = temp;
		}
		break;
	case K3_FB_TRANSFORM_ROT_180:
		{
			temp = clip_rect->left;
			clip_rect->left =  clip_rect->right;
			clip_rect->right = temp ;

			temp = clip_rect->top;
			clip_rect->top =  clip_rect->bottom;
			clip_rect->bottom = temp;
		}
		break;
	case K3_FB_TRANSFORM_ROT_270:
		{
			temp = clip_rect->left;
			clip_rect->left = clip_rect->top;
			clip_rect->top = clip_rect->right;
			clip_rect->right = clip_rect->bottom;
			clip_rect->bottom = temp;
		}
		break;
	case (K3_FB_TRANSFORM_ROT_90 | K3_FB_TRANSFORM_FLIP_H):
		{
			temp = clip_rect->left;
			clip_rect->left = clip_rect->bottom;
			clip_rect->bottom = clip_rect->right;
			clip_rect->right = clip_rect->top;
			clip_rect->top = temp;

			temp = clip_rect->top;
			clip_rect->top = clip_rect->bottom;
			clip_rect->bottom = temp;
		}
		break;
	case (K3_FB_TRANSFORM_ROT_90 | K3_FB_TRANSFORM_FLIP_V):
		{
			temp = clip_rect->left;
			clip_rect->left = clip_rect->bottom;
			clip_rect->bottom = clip_rect->right;
			clip_rect->right = clip_rect->top;
			clip_rect->top = temp;

			temp = clip_rect->left;
			clip_rect->left = clip_rect->right;
			clip_rect->right = temp;
		}
		break;
	default:
		K3_FB_ERR("not supported this transform(%d)!", layer->transform);
		break;
	}
}

static u32 k3_calculate_display_addr(bool mmu_enable, dss_layer_t *layer,
	dss_rect_ltrb_t *aligned_rect, int add_type)
{
	u32 addr = 0;
	u32 src_addr = 0;
	u32 stride = 0;
	int bpp = 0;
	int left = 0;
	int right = 0;
	int top = 0;
	int bottom = 0;

	left = aligned_rect->left;
	right = aligned_rect->right;
	top = aligned_rect->top;
	bottom = aligned_rect->bottom;

	if (add_type == DSS_ADDR_PLANE0) {
		stride = layer->src.stride;
		src_addr = mmu_enable ? layer->src.vir_addr : layer->src.phy_addr;
		bpp = layer->src.bpp;
	} else if (add_type == DSS_ADDR_PLANE1) {
		stride = layer->src.uStride;
		src_addr = mmu_enable ? (layer->src.vir_addr + layer->src.uOffset) :
			(layer->src.phy_addr + layer->src.uOffset);
		bpp = 1;

		if (is_YUV_P_420(layer->src.format) || is_YUV_P_422(layer->src.format)) {
			left /= 2;
			right /= 2;
		}

		if (is_YUV_SP_420(layer->src.format) || is_YUV_P_420(layer->src.format)) {
			top /= 2;
			bottom /= 2;
		}
	} else if (add_type == DSS_ADDR_PLANE2) {
		stride = layer->src.vStride;
		src_addr = mmu_enable ? (layer->src.vir_addr + layer->src.vOffset) :
			(layer->src.phy_addr + layer->src.vOffset);
		bpp = 1;

		if (is_YUV_P_420(layer->src.format) || is_YUV_P_422(layer->src.format)) {
			left /= 2;
			right /= 2;
		}

		if (is_YUV_SP_420(layer->src.format) || is_YUV_P_420(layer->src.format)) {
			top /= 2;
			bottom /= 2;
		}
	} else {
		K3_FB_ERR("NOT SUPPORT this add_type(%d).\n", add_type);
		BUG_ON(1);
	}

	switch(layer->transform) {
	case K3_FB_TRANSFORM_NOP:
		addr = src_addr + top * stride + left * bpp;
		break;
	case K3_FB_TRANSFORM_FLIP_H:
		addr = src_addr + top * stride + right * bpp;
		break;
	case K3_FB_TRANSFORM_FLIP_V:
		addr = src_addr + bottom * stride + left * bpp;
		break;
	case K3_FB_TRANSFORM_ROT_90:
		addr = src_addr + bottom * stride + left * bpp;
		break;
	case K3_FB_TRANSFORM_ROT_180:
		addr = src_addr + bottom * stride + right * bpp;
		break;
	case K3_FB_TRANSFORM_ROT_270:
		addr = src_addr + top * stride + right * bpp;
		break;
	case (K3_FB_TRANSFORM_ROT_90 | K3_FB_TRANSFORM_FLIP_H):
		addr = src_addr + bottom * stride + right * bpp;
		break;
	case (K3_FB_TRANSFORM_ROT_90 | K3_FB_TRANSFORM_FLIP_V):
		addr = src_addr + top * stride + left * bpp;
		break;
	default:
		K3_FB_ERR("not supported this transform(%d)!", layer->transform);
		break;
	}

	return addr;
}

static int k3_get_chnIndex4crossSwitch(uint32_t need_cap, int32_t chn_idx)
{
	int ret = -1;

	if (need_cap & CAP_CROSS_SWITCH) {
		if (chn_idx == DPE0_CHN0)
			ret = DPE0_CHN1;
		else if (chn_idx == DPE0_CHN1)
			ret = DPE0_CHN0;
		else if (chn_idx == DPE2_CHN0)
			ret = DPE2_CHN1;
		else if (chn_idx== DPE2_CHN1)
			ret = DPE2_CHN0;
		else
			K3_FB_ERR("CAP_CROSS_SWITCH not support this chn_idx(%d)!", chn_idx);
	} else {
		ret = chn_idx;
	}

	return  ret;
}

int k3_get_rot_index(int chn_idx)
{
	int ret = 0;

	switch(chn_idx) {
	case DPE0_CHN0:
		ret = 0;
		break;

	case DPE2_CHN0:
	case WBE0_CHN1:
		ret = 1;
		break;

	case DPE3_CHN0:
		ret = 4;
		break;
	case DPE3_CHN1:
	case WBE1_CHN1:
		ret = 2;
		break;
	case DPE3_CHN2:
		ret = 3;
		break;

	default:
		ret = -1;
		//K3_FB_ERR("not support this chn_idx(%d)!\n", chn_idx);
		break;
	}

	return ret;
}

int k3_get_scf_index(int chn_idx)
{
	int ret = 0;

	switch(chn_idx) {
	case DPE0_CHN0:
	case DPE0_CHN1:
		ret = 0;
		break;

	case DPE1_CHN0:
	case WBE0_CHN1:
		ret = 1;
		break;

	case DPE2_CHN0:
	case DPE2_CHN1:
		ret = 2;
		break;

	case DPE3_CHN1:
	case WBE1_CHN1:
		ret = 3;
		break;

	case DPE3_CHN0:
		ret = 4;
		break;

	default:
		ret = -1;
		//K3_FB_ERR("not support this chn_idx(%d)!\n", chn_idx);
		break;
	}

	return ret;
}

static int k3_pixel_format_hal2mmu(int format)
{
	int ret = 0;

	switch(format) {
	case K3_FB_PIXEL_FORMAT_RGB_565:
	case K3_FB_PIXEL_FORMAT_RGBX_4444:
	case K3_FB_PIXEL_FORMAT_RGBA_4444:
	case K3_FB_PIXEL_FORMAT_RGBX_5551:
	case K3_FB_PIXEL_FORMAT_RGBA_5551:

	case K3_FB_PIXEL_FORMAT_BGR_565:
	case K3_FB_PIXEL_FORMAT_BGRX_4444:
	case K3_FB_PIXEL_FORMAT_BGRA_4444:
	case K3_FB_PIXEL_FORMAT_BGRX_5551:
	case K3_FB_PIXEL_FORMAT_BGRA_5551:

	case K3_FB_PIXEL_FORMAT_RGBX_8888:
	case K3_FB_PIXEL_FORMAT_RGBA_8888:
	case K3_FB_PIXEL_FORMAT_BGRX_8888:
	case K3_FB_PIXEL_FORMAT_BGRA_8888:

	case K3_FB_PIXEL_FORMAT_YUV_422_I:
		ret = MMU_PIXEL_FORMAT_PACKAGE;
		break;

	/* Planar */
	case K3_FB_PIXEL_FORMAT_YCbCr_422_P:
	case K3_FB_PIXEL_FORMAT_YCrCb_422_P:

	case K3_FB_PIXEL_FORMAT_YCbCr_420_P:
	case K3_FB_PIXEL_FORMAT_YCrCb_420_P:
		ret = MMU_PIXEL_FORMAT_YUV_PLANAR;
		break;

	/* Semi-Planar */
	case K3_FB_PIXEL_FORMAT_YCbCr_422_SP:
	case K3_FB_PIXEL_FORMAT_YCrCb_422_SP:

	case K3_FB_PIXEL_FORMAT_YCrCb_420_SP:
	case K3_FB_PIXEL_FORMAT_YCbCr_420_SP:
		ret = MMU_PIXEL_FORMAT_YUV_SEMI_PLANAR;
		break;

	default:
		K3_FB_ERR("not support format(%d)!\n", format);
		ret = -1;
		break;
	}

	return ret;
}

static int k3_pixel_format_hal2dma(int format)
{
	int ret = 0;

	switch(format) {
	case K3_FB_PIXEL_FORMAT_RGB_565:
	case K3_FB_PIXEL_FORMAT_RGBX_4444:
	case K3_FB_PIXEL_FORMAT_RGBA_4444:
	case K3_FB_PIXEL_FORMAT_RGBX_5551:
	case K3_FB_PIXEL_FORMAT_RGBA_5551:

	case K3_FB_PIXEL_FORMAT_BGR_565:
	case K3_FB_PIXEL_FORMAT_BGRX_4444:
	case K3_FB_PIXEL_FORMAT_BGRA_4444:
	case K3_FB_PIXEL_FORMAT_BGRX_5551:
	case K3_FB_PIXEL_FORMAT_BGRA_5551:
		ret = DMA_PIXEL_FORMAT_RGB16BIT;
		break;

	case K3_FB_PIXEL_FORMAT_RGBX_8888:
	case K3_FB_PIXEL_FORMAT_RGBA_8888:
	case K3_FB_PIXEL_FORMAT_BGRX_8888:
	case K3_FB_PIXEL_FORMAT_BGRA_8888:
		ret = DMA_PIXEL_FORMAT_RGB32BIT;
		break;

	/* Packet */
	case K3_FB_PIXEL_FORMAT_YUV_422_I:
		ret = DMA_PIXEL_FORMAT_YUV_422_I;
		break;
	/* Planar */
	case K3_FB_PIXEL_FORMAT_YCbCr_422_P:
	case K3_FB_PIXEL_FORMAT_YCrCb_422_P:
		ret = DMA_PIXEL_FORMAT_YUV_422_P_HP_Y;
		break;
	case K3_FB_PIXEL_FORMAT_YCbCr_420_P:
	case K3_FB_PIXEL_FORMAT_YCrCb_420_P:
		ret = DMA_PIXEL_FORMAT_YUV_420_P_Y;
		break;
	/* Semi-Planar */
	case K3_FB_PIXEL_FORMAT_YCbCr_422_SP:
	case K3_FB_PIXEL_FORMAT_YCrCb_422_SP:
		ret = DMA_PIXEL_FORMAT_YUV_422_SP_HP_Y;
		break;
	case K3_FB_PIXEL_FORMAT_YCrCb_420_SP:
	case K3_FB_PIXEL_FORMAT_YCbCr_420_SP:
		ret = DMA_PIXEL_FORMAT_YUV_420_SP_Y;
		break;

	default:
		K3_FB_ERR("not support format(%d)!\n", format);
		ret = -1;
		break;
	}

	return ret;
}

static int k3_transform_hal2dma(int transform, int chn_index)
{
	int ret = 0;

	switch(transform) {
	case K3_FB_TRANSFORM_NOP:
		ret = DSS_TRANSFORM_NOP;
		break;
	case K3_FB_TRANSFORM_FLIP_H:
		ret = DSS_TRANSFORM_FLIP_H;
		break;
	case K3_FB_TRANSFORM_FLIP_V:
		ret = DSS_TRANSFORM_FLIP_V;
		break;
	case K3_FB_TRANSFORM_ROT_90:
		if ((chn_index >= WBE0_CHN0) && (chn_index <= WBE1_CHN1))
			ret = DSS_TRANSFORM_ROT | DSS_TRANSFORM_FLIP_H;
		else
			ret = DSS_TRANSFORM_ROT | DSS_TRANSFORM_FLIP_V;
		break;
	case K3_FB_TRANSFORM_ROT_180:
		ret = DSS_TRANSFORM_FLIP_V |DSS_TRANSFORM_FLIP_H;
		break;
	case K3_FB_TRANSFORM_ROT_270:
		if ((chn_index >= WBE0_CHN0) && (chn_index <= WBE1_CHN1))
			ret = DSS_TRANSFORM_ROT | DSS_TRANSFORM_FLIP_V;
		else
			ret = DSS_TRANSFORM_ROT | DSS_TRANSFORM_FLIP_H;
		break;
	case (K3_FB_TRANSFORM_ROT_90 | K3_FB_TRANSFORM_FLIP_H):
		if ((chn_index >= WBE0_CHN0) && (chn_index <= WBE1_CHN1))
			ret = DSS_TRANSFORM_ROT;
		else
			ret =  DSS_TRANSFORM_ROT | DSS_TRANSFORM_FLIP_V | DSS_TRANSFORM_FLIP_H;
		break;
	case (K3_FB_TRANSFORM_ROT_90 | K3_FB_TRANSFORM_FLIP_V):
		if ((chn_index >= WBE0_CHN0) && (chn_index <= WBE1_CHN1))
			ret = DSS_TRANSFORM_ROT | DSS_TRANSFORM_FLIP_H | DSS_TRANSFORM_FLIP_V;
		else
			ret = DSS_TRANSFORM_ROT;
		break;
	default:
		ret = -1;
		K3_FB_ERR("Transform %d is not supported", transform);
		break;
	}

	return ret;
}

static int k3_chnIndex2dmaIndex(int chn_idx)
{
	int ret = 0;

	switch(chn_idx) {
	case DPE0_CHN0:
	case DPE1_CHN0:
	case DPE2_CHN0:
	case DPE3_CHN0:
		ret = 0;
		break;
	case DPE0_CHN1:
	case DPE2_CHN1:
	case DPE3_CHN1:
		ret = 2;
		break;
	case DPE0_CHN2:
	case DPE2_CHN2:
	case DPE3_CHN2:
		ret = 3;
		break;
	case DPE0_CHN3:
	case DPE1_CHN1:
	case DPE2_CHN3:
	case DPE3_CHN3:
		ret = 4;
		break;
	default:
		ret = -1;
		K3_FB_ERR("not support this chn_idx(%d)!\n", chn_idx);
		break;
	}

	return ret;
}

static int k3_pixel_format_hal2dfc(int format, int transform)
{
	int ret = 0;

	switch (format) {
	case K3_FB_PIXEL_FORMAT_RGB_565:
		ret = DFC_PIXEL_FORMAT_RGB_565;
		break;
	case K3_FB_PIXEL_FORMAT_RGBX_4444:
		ret = DFC_PIXEL_FORMAT_XBGR_4444;
		break;
	case K3_FB_PIXEL_FORMAT_RGBA_4444:
		ret = DFC_PIXEL_FORMAT_ABGR_4444;
		break;
	case K3_FB_PIXEL_FORMAT_RGBX_5551:
		ret = DFC_PIXEL_FORMAT_XBGR_5551;
		break;
	case K3_FB_PIXEL_FORMAT_RGBA_5551:
		ret = DFC_PIXEL_FORMAT_ABGR_5551;
		break;
	case K3_FB_PIXEL_FORMAT_RGBX_8888:
		ret = DFC_PIXEL_FORMAT_XBGR_8888;
		break;
	case K3_FB_PIXEL_FORMAT_RGBA_8888:
		ret = DFC_PIXEL_FORMAT_ABGR_8888;
		break;

	case K3_FB_PIXEL_FORMAT_BGR_565:
		ret = DFC_PIXEL_FORMAT_BGR_565;
		break;
	case K3_FB_PIXEL_FORMAT_BGRX_4444:
		ret = DFC_PIXEL_FORMAT_XRGB_4444;
		break;
	case K3_FB_PIXEL_FORMAT_BGRA_4444:
		ret = DFC_PIXEL_FORMAT_ARGB_4444;
		break;
	case K3_FB_PIXEL_FORMAT_BGRX_5551:
		ret = DFC_PIXEL_FORMAT_XRGB_5551;
		break;
	case K3_FB_PIXEL_FORMAT_BGRA_5551:
		ret = DFC_PIXEL_FORMAT_ARGB_5551;
		break;
	case K3_FB_PIXEL_FORMAT_BGRX_8888:
		ret = DFC_PIXEL_FORMAT_XRGB_8888;
		break;
	case K3_FB_PIXEL_FORMAT_BGRA_8888:
		ret = DFC_PIXEL_FORMAT_ARGB_8888;
		break;

	/* YUV */
	case K3_FB_PIXEL_FORMAT_YUV_422_I:
		ret = (transform & K3_FB_TRANSFORM_ROT_90) ?
			DFC_PIXEL_FORMAT_YUV444 : DFC_PIXEL_FORMAT_YUYV422;
		break;

	/* YUV Semi-planar */
	case K3_FB_PIXEL_FORMAT_YCbCr_422_SP: /* NV16 */
		ret = (transform & K3_FB_TRANSFORM_ROT_90) ?
			DFC_PIXEL_FORMAT_YUV444 : DFC_PIXEL_FORMAT_YUYV422;
		break;
	case K3_FB_PIXEL_FORMAT_YCrCb_422_SP:
		ret = (transform & K3_FB_TRANSFORM_ROT_90) ?
			DFC_PIXEL_FORMAT_YVU444 : DFC_PIXEL_FORMAT_YVYU422;
		break;
	case K3_FB_PIXEL_FORMAT_YCbCr_420_SP:
		ret = DFC_PIXEL_FORMAT_YUYV422;
		break;
	case K3_FB_PIXEL_FORMAT_YCrCb_420_SP:
		ret = DFC_PIXEL_FORMAT_YVYU422;
		break;

	/* YUV Planar */
	case K3_FB_PIXEL_FORMAT_YCbCr_422_P:
	case K3_FB_PIXEL_FORMAT_YCrCb_422_P:
	case K3_FB_PIXEL_FORMAT_YCbCr_420_P:
	case K3_FB_PIXEL_FORMAT_YCrCb_420_P: /* K3_FB_PIXEL_FORMAT_YV12 */
		ret = DFC_PIXEL_FORMAT_YUYV422;
		break;

	default:
		K3_FB_ERR("not support format(%d)!\n", format);
		ret = -1;
		break;
	}

	return ret;
}

static int k3_dfc_get_bpp(int dfc_format)
{
	int ret = 0;

	switch (dfc_format) {
	case DFC_PIXEL_FORMAT_RGB_565:
	case DFC_PIXEL_FORMAT_XRGB_4444:
	case DFC_PIXEL_FORMAT_ARGB_4444:
	case DFC_PIXEL_FORMAT_XRGB_5551:
	case DFC_PIXEL_FORMAT_ARGB_5551:

	case DFC_PIXEL_FORMAT_BGR_565:
	case DFC_PIXEL_FORMAT_XBGR_4444:
	case DFC_PIXEL_FORMAT_ABGR_4444:
	case DFC_PIXEL_FORMAT_XBGR_5551:
	case DFC_PIXEL_FORMAT_ABGR_5551:
		ret = 2;
		break;

	case DFC_PIXEL_FORMAT_XRGB_8888:
	case DFC_PIXEL_FORMAT_ARGB_8888:
	case DFC_PIXEL_FORMAT_XBGR_8888:
	case DFC_PIXEL_FORMAT_ABGR_8888:
		ret = 4;
		break;

	case DFC_PIXEL_FORMAT_YUV444:
	case DFC_PIXEL_FORMAT_YVU444:
		ret = 3;
		break;

	case DFC_PIXEL_FORMAT_YUYV422:
	case DFC_PIXEL_FORMAT_YVYU422:
	case DFC_PIXEL_FORMAT_VYUY422:
	case DFC_PIXEL_FORMAT_UYVY422:
		ret = 2;
		break;

	default:
		K3_FB_ERR("not support format(%d)!\n", dfc_format);
		ret = -1;
		break;
	}

	return ret;
}


/******************************************************************************/
void k3_dss_unflow_handler(struct k3_fb_data_type *k3fd, bool unmask)
{
	u32 tmp = 0;

	BUG_ON(k3fd == NULL);

	if (k3fd->ovl_type == DSS_OVL_PDP) {
		tmp = inp32(k3fd->dss_base + PDP_LDI_CPU_IRQ_MSK);
		if (unmask) {
			tmp &= ~BIT_LDI_UNFLOW_INT;
		} else {
			tmp |= BIT_LDI_UNFLOW_INT;
		}
		outp32(k3fd->dss_base + PDP_LDI_CPU_IRQ_MSK, tmp);

		if (g_debug_mmu_error) {
			outp32(k3fd->dss_base + DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET +
				PDP_DPE0_MMU_CPU_IRQ_MSK, 0xFFFFF);
			outp32(k3fd->dss_base + DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET +
				PDP_DPE2_MMU_CPU_IRQ_MSK, 0xFFFFF);
		}
	} else if (k3fd->ovl_type == DSS_OVL_SDP) {
		tmp = inp32(k3fd->dss_base + SDP_LDI_CPU_IRQ_MSK);
		if (unmask) {
			tmp &= ~BIT_LDI_UNFLOW_INT;
		} else {
			tmp |= BIT_LDI_UNFLOW_INT;
		}
		outp32(k3fd->dss_base + SDP_LDI_CPU_IRQ_MSK, tmp);
	} else {
		; /* do nothing */
	}

	//k3fd->dss_exception.underflow_exception = 1;
}

void k3_dss_config_ok_begin(struct k3_fb_data_type *k3fd)
{
	char __iomem *ovl_cfg_ok_base = NULL;
	u32 module_base = 0;

	BUG_ON(k3fd == NULL);

	module_base = g_dss_module_ovl_base[k3fd->ovl_type][MODULE_OVL_CFG_OK];
	ovl_cfg_ok_base = k3fd->dss_base + module_base;
	BUG_ON(module_base == 0);

	k3fd->set_reg(k3fd, ovl_cfg_ok_base, 0x0, 1, 0);
}

void k3_dss_config_ok_end(struct k3_fb_data_type *k3fd)
{
	char __iomem *ovl_cfg_ok_base = NULL;
	u32 module_base = 0;

	BUG_ON(k3fd == NULL);

	module_base = g_dss_module_ovl_base[k3fd->ovl_type][MODULE_OVL_CFG_OK];
	ovl_cfg_ok_base = k3fd->dss_base + module_base;
	BUG_ON(module_base == 0);

	k3fd->set_reg(k3fd, ovl_cfg_ok_base, 0x1, 1, 0);
}

void k3_dfs_wbe_enable(struct k3_fb_data_type *k3fd)
{
	char __iomem *wbe_en_base = NULL;
	u32 module_base = 0;

	BUG_ON(k3fd == NULL);

	module_base = g_dss_module_ovl_base[k3fd->ovl_type][MODULE_OVL_WBE_EN];
	wbe_en_base = k3fd->dss_base + module_base;
	BUG_ON(module_base == 0);

	k3fd->set_reg(k3fd, wbe_en_base, 0x1, 1, 0);
}

void k3_dfs_wbe_disable(struct k3_fb_data_type *k3fd)
{
	char __iomem *wbe_en_base = NULL;
	u32 module_base = 0;

	BUG_ON(k3fd == NULL);

	module_base = g_dss_module_ovl_base[k3fd->ovl_type][MODULE_OVL_WBE_EN];
	wbe_en_base = k3fd->dss_base + module_base;
	BUG_ON(module_base == 0);

	k3fd->set_reg(k3fd, wbe_en_base, 0x0, 1, 0);
}

void k3_adp_offline_start_enable(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req)
{
	dss_wb_layer_t *wb_layer = NULL;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);

	wb_layer = &(pov_req->wb_layer_info);
	BUG_ON(wb_layer == NULL);

	if (wb_layer->chn_idx == WBE1_CHN0) {
		k3fd->set_reg(k3fd, k3fd->dss_base + DSS_GLB_ADP_OFFLINE_START0, 0x1, 32, 0);
	} else if (wb_layer->chn_idx == WBE1_CHN1) {
		k3fd->set_reg(k3fd, k3fd->dss_base + DSS_GLB_ADP_OFFLINE_START1, 0x1, 32, 0);
	} else {
		K3_FB_ERR("not support this chn_idx(%d)!\n", wb_layer->chn_idx);
	}
}

void k3_adp_offline_start_disable(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req)
{
	dss_wb_layer_t *wb_layer = NULL;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);

	wb_layer = &(pov_req->wb_layer_info);
	BUG_ON(wb_layer == NULL);

	if (wb_layer->chn_idx == WBE1_CHN0) {
		k3fd->set_reg(k3fd, k3fd->dss_base + DSS_GLB_ADP_OFFLINE_START0, 0x0, 32, 0);
	} else if (wb_layer->chn_idx == WBE1_CHN1) {
		k3fd->set_reg(k3fd, k3fd->dss_base + DSS_GLB_ADP_OFFLINE_START1, 0x0, 32, 0);
	} else {
		K3_FB_ERR("not support this chn_idx(%d)!\n", wb_layer->chn_idx);
	}
}

int k3_dss_rptb_handler(struct k3_fb_data_type *k3fd, bool is_wb, int rptb_info_idx)
{
	int i = 0;
	dss_rptb_info_t *rptb_info_prev = NULL;
	dss_rptb_info_t *rptb_info_cur = NULL;

	if (is_wb) {
		rptb_info_prev = &(k3fd->dss_wb_rptb_info_prev[rptb_info_idx]);
		rptb_info_cur = &(k3fd->dss_wb_rptb_info_cur[rptb_info_idx]);
	} else {
		rptb_info_prev = &(k3fd->dss_rptb_info_prev[rptb_info_idx]);
		rptb_info_cur = &(k3fd->dss_rptb_info_cur[rptb_info_idx]);
	}

	if (k3fd->ovl_type == DSS_OVL_ADP) {
		for (i = 0; i < DSS_ROT_MAX; i++) {
			if (rptb_info_cur->rptb_size[i] > 0) {
				k3_dss_rptb_free(g_rptb_gen_pool,
					rptb_info_cur->rptb_offset[i], rptb_info_cur->rptb_size[i]);
				rptb_info_cur->rptb_size[i] = 0;
				rptb_info_cur->rptb_offset[i] = 0;
			}
		}

		memset(rptb_info_prev, 0, sizeof(dss_rptb_info_t));
		memset(rptb_info_cur, 0, sizeof(dss_rptb_info_t));
	} else {
	#if 0
		for (i = 0; i < DSS_ROT_MAX; i++) {
			if (rptb_info_cur->rptb_size[i] > 0) {
				k3_dss_rptb_free(g_rptb_gen_pool,
					rptb_info_cur->rptb_offset[i], rptb_info_cur->rptb_size[i]);
				rptb_info_cur->rptb_size[i] = 0;
				rptb_info_cur->rptb_offset[i] = 0;
			}
		}

		memset(rptb_info_prev, 0, sizeof(dss_rptb_info_t));
		memset(rptb_info_cur, 0, sizeof(dss_rptb_info_t));
	#else
		for (i = 0; i < DSS_ROT_MAX; i++) {
			if (rptb_info_prev->rptb_size[i] > 0) {
				k3_dss_rptb_free(g_rptb_gen_pool,
					rptb_info_prev->rptb_offset[i], rptb_info_prev->rptb_size[i]);
				rptb_info_prev->rptb_size[i] = 0;
				rptb_info_prev->rptb_offset[i] = 0;
			}
		}
		memcpy(rptb_info_prev, rptb_info_cur, sizeof(dss_rptb_info_t));
		memset(rptb_info_cur, 0, sizeof(dss_rptb_info_t));
	#endif
	}

	return 0;
}

int k3_dss_handle_prev_ovl_req(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req)
{
	dss_layer_t *layer = NULL;
	int chn_idx = 0;
	int chn_idx_tmp = 0;
	int i = 0;
	int k = 0;
	int idx_tmp = 0;
	u32 format = 0;
	u32 need_cap = 0;
	u32 tmp = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);

	if (k3fd->dss_exception.underflow_handler == 1) {
		for (k = 0; k < DSS_CHN_MAX; k++) {
			if (k3fd->dss_exception.chn_used[k] == 1) {
				k3fd->dss_glb.chn_ctl_used[k] = 1;
				if (isYUVPlanar(k3fd->dss_exception.chn_format_used[k])
					&& (k == DPE3_CHN0)) {
					k3fd->dss_glb.chn_ctl_used[k + 1] = 1;
				}
			}
		}

		k3fd->dss_exception.underflow_handler = 0;
	}

	for (i = 0; i < pov_req->layer_nums; i++) {
		layer = &pov_req->layer_infos[i];
		chn_idx = layer->chn_idx;
		format = layer->src.format;
		need_cap = layer->need_cap;

		/* CHN CTRL */
		if (k3fd->dss_exception.underflow_exception == 1) {
			k3fd->dss_exception.chn_used[chn_idx] = 1;
			k3fd->dss_exception.chn_format_used[chn_idx] = format;
		} else {
			k3fd->dss_glb.chn_ctl_used[chn_idx] = 1;
			/*if (isYUVSemiPlanar(format) || isYUVPlanar(format)) {
				k3fd->dss_glb.chn_ctl_used[chn_idx] = 1;
			}*/
			if (isYUVPlanar(format) && (chn_idx == DPE3_CHN0)) {
				chn_idx_tmp = chn_idx + 1;
				k3fd->dss_glb.chn_ctl_used[chn_idx_tmp] = 1;
			}
		}

		/* RDMA */
		k3fd->dss_module.rdma0_used[chn_idx] = 1;
		if (isYUVSemiPlanar(format) || isYUVPlanar(format)) {
			k3fd->dss_module.rdma1_used[chn_idx] = 1;
		}
		if (isYUVPlanar(format) && (chn_idx == DPE3_CHN0)) {
			chn_idx_tmp = chn_idx + 1;
			k3fd->dss_module.rdma0_used[chn_idx_tmp] = 1;
		}

		if (layer->src.mmu_enable) {
			/* MMU_DMA */
			k3fd->dss_module.mmu_dma0_used[chn_idx] = 1;

			if (isYUVSemiPlanar(format) || isYUVPlanar(format)) {
				k3fd->dss_module.mmu_dma1_used[chn_idx] = 1;
			}

			if (isYUVPlanar(format) && (chn_idx == DPE3_CHN0)) {
				chn_idx_tmp = chn_idx + 1;
				k3fd->dss_module.mmu_dma0_used[chn_idx_tmp] = 1;
			}

			/* MMU_RPTB */
			if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
				idx_tmp = k3_get_rot_index(chn_idx);
				if (idx_tmp < 0) {
					K3_FB_ERR("k3_get_rot_index failed! chn_idx=%d.\n", chn_idx);
				} else {
					if (pov_req->wb_enable && (pov_req->wb_layer_info.chn_idx == WBE1_CHN1))
						tmp = k3fd->dss_rptb_info_cur[1].rptb_size[idx_tmp];
					else
						tmp = k3fd->dss_rptb_info_cur[0].rptb_size[idx_tmp];

					if (tmp > 0) {
						k3fd->dss_module.mmu_rptb_used = 1;
						k3fd->dss_module.mmu_rptb.mmu_rptb_idx_used[idx_tmp] = 1;
						k3fd->dss_module.mmu_rptb.mmu_rptb_ctl =
							set_bits32(k3fd->dss_module.mmu_rptb.mmu_rptb_ctl,
								(0 << idx_tmp) | (1 << (26 + idx_tmp)), 32, 0);

						k3fd->dss_glb.rot_tlb_scene_used[idx_tmp] = 1;
					}
				}
			}
		}

		/* FBDC */
		if (need_cap & CAP_FBDC) {
			k3fd->dss_module.fbdc_used[chn_idx] = 1;
		}

		/* DFC */
		k3fd->dss_module.dfc_used[chn_idx] = 1;

		if (need_cap & CAP_SCL) {
			/* SCF */
			idx_tmp = k3_get_scf_index(chn_idx);
			if (idx_tmp < 0) {
				K3_FB_ERR("k3_get_scf_index failed! chn_idx=%d.\n", chn_idx);
			} else {
				k3fd->dss_module.scf_used[idx_tmp] = 1;
			}
		}

		if (need_cap & CAP_SCP) {
			/* SCP */
			k3fd->dss_module.scp_used[chn_idx] = 1;
		}

		/* CSC */
		if (isYUV(format)) {
			chn_idx_tmp = k3_get_chnIndex4crossSwitch(need_cap, chn_idx);
			if (chn_idx_tmp >= 0) {
				k3fd->dss_module.csc_used[chn_idx_tmp] = 1;
			}
		}

		/* OV */
		k3fd->dss_module.ov_used[k3fd->ovl_type] = 1;
	}

	if (k3fd->ovl_type != DSS_OVL_ADP)
		k3_dss_rptb_handler(k3fd, false, 0);

	if (k3fd->dss_exception.underflow_exception == 1) {
		k3fd->dss_exception.underflow_exception = 0;
		k3fd->dss_exception.underflow_handler = 1;
	}

	return 0;
}

int k3_dss_handle_cur_ovl_req(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req)
{
	dss_layer_t *layer = NULL;
	int i = 0;
	int rot_idx = 0;
	int chn_idx = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);

	for (i = 0; i < pov_req->layer_nums; i++) {
		layer = &pov_req->layer_infos[i];
		chn_idx = layer->chn_idx;

		if (k3fd->dss_exception.underflow_handler == 1) {
			k3fd->dss_exception.chn_used[chn_idx] = 0;
			k3fd->dss_exception.chn_format_used[chn_idx] = 0;
		}

		if (isChannelBelongtoDPE(DSS_ENG_DPE2, layer->chn_idx)) {
			k3fd->dss_module.dpe2_on_ref++;
		}

		if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
			rot_idx = k3_get_rot_index(layer->chn_idx);
			if (rot_idx == 1) {
				k3fd->dss_module.rot1_on_ref++;
			}
		}
	}

	return 0;
}

int k3_dss_handle_prev_ovl_req_wb(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req)
{
	dss_wb_layer_t *layer = NULL;
	int chn_idx = 0;
	int idx_tmp = 0;
	u32 format = 0;
	u32 need_cap = 0;
	u32 tmp = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);

	if (pov_req->wb_enable != 1)
		return 0;

	layer = &(pov_req->wb_layer_info);
	chn_idx = layer->chn_idx;
	format = layer->dst.format;
	need_cap = layer->need_cap;

	/* CSC */
	if (isYUV(format) && chn_idx != WBE1_CHN0) {
		k3fd->dss_module.csc_used[chn_idx] = 1;
	}

	if (need_cap & CAP_SCL) {
		/* SCF */
		idx_tmp = k3_get_scf_index(chn_idx);
		if (idx_tmp < 0) {
			K3_FB_ERR("k3_get_scf_index failed! chn_idx=%d.\n", chn_idx);
		} else {
			k3fd->dss_module.scf_used[idx_tmp] = 1;
		}
	}

	/* DFC */
	k3fd->dss_module.dfc_used[chn_idx] = 1;

	/* FBC */
	if (need_cap & CAP_FBC) {
		k3fd->dss_module.fbc_used[chn_idx] = 1;
		k3fd->dss_module.fbc[chn_idx].fbc_pm_ctrl =
			set_bits32(k3fd->dss_module.fbc[chn_idx].fbc_pm_ctrl, 0x83AA, 17, 0);
	}

	/* CHN CTRL */
	if (k3fd->dss_wb_exception.underflow_exception == 1) {
		k3fd->dss_wb_exception.chn_used[chn_idx] = 1;
		k3fd->dss_wb_exception.chn_format_used[chn_idx] = format;
	} else {
		k3fd->dss_glb.chn_ctl_used[chn_idx] = 1;
	}

	/* WDMA */
	k3fd->dss_module.wdma_used[chn_idx] = 1;

	if (layer->dst.mmu_enable) {
		/* MMU WDMA */
		k3fd->dss_module.mmu_dma0_used[chn_idx] = 1;

		/* MMU RPTB */
		if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
			idx_tmp = k3_get_rot_index(chn_idx);
			if (idx_tmp < 0) {
				K3_FB_ERR("k3_get_rot_index failed! chn_idx=%d.\n", chn_idx);
			} else {
				if (pov_req->wb_enable && (pov_req->wb_layer_info.chn_idx == WBE1_CHN1))
					tmp = k3fd->dss_wb_rptb_info_cur[1].rptb_size[idx_tmp];
				else
					tmp = k3fd->dss_wb_rptb_info_cur[0].rptb_size[idx_tmp];

				if (tmp > 0) {
					k3fd->dss_module.mmu_rptb_used = 1;
					k3fd->dss_module.mmu_rptb.mmu_rptb_idx_used[idx_tmp] = 1;
					k3fd->dss_module.mmu_rptb.mmu_rptb_ctl =
						set_bits32(k3fd->dss_module.mmu_rptb.mmu_rptb_ctl,
							(0 << idx_tmp) | (1 << (26 + idx_tmp)), 32, 0);

					k3fd->dss_glb.rot_tlb_scene_used[idx_tmp] = 1;
				}
			}
		}
	}

	if (k3fd->ovl_type != DSS_OVL_ADP)
		k3_dss_rptb_handler(k3fd, true, 0);

	return 0;
}

int k3_dss_handle_cur_ovl_req_wb(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req)
{
	dss_wb_layer_t *layer = NULL;
	int rot_idx = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);

	if (pov_req->wb_enable != 1)
		return 0;

	layer = &(pov_req->wb_layer_info);

	if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
		rot_idx = k3_get_rot_index(layer->chn_idx);
		if (rot_idx == 1) {
			k3fd->dss_module.rot1_on_ref++;
		}
	}

	return 0;
}


/*******************************************************************************
** DSS RDMA Bridge
*/
static int g_rdma_bridge_mid[RDMA_MID_NUM] = {
	0, 1, 0, 1, 2,
	3, 4, 5, 6, 7,
	6, 7, 8, 9, 10,
	11, 12, 13
};

static int k3_get_rdma_axi_port(int chn_idx, u32 flags)
{
	int axi_port = AXI_CH0;

	switch (flags) {
	case DSS_SCENE_PDP_ONLINE:
	case DSS_SCENE_SDP_ONLINE:
		axi_port = AXI_CH0;
		break;
	case DSS_SCENE_WBE0_WS_OFFLINE:
	case DSS_SCENE_WBE0_WO_OFFLINE:
	case DSS_SCENE_WBE1_WS_OFFLINE:
	case DSS_SCENE_WBE1_WO_OFFLINE:
		axi_port = AXI_CH1;
		break;
	default:
		axi_port = -1;
		K3_FB_ERR("chn%d not support this flags(%d)!\n", chn_idx, flags);
		break;
	}

	return axi_port;
}

static int k3_get_rdma_bridge_bpp(int format, int transform, int stretched_line_num)
{
	int bpp = 0;
	int rot_factor = 1;

	if (transform & K3_FB_TRANSFORM_ROT_90)
		rot_factor = 2;

	switch (format) {
	case DMA_PIXEL_FORMAT_RGB32BIT:
		bpp = 8;
		break;
	case DMA_PIXEL_FORMAT_RGB16BIT:
	case DMA_PIXEL_FORMAT_YUV_422_I:
		bpp = 4;
		break;

	case DMA_PIXEL_FORMAT_YUV_422_SP_HP_Y:
	case DMA_PIXEL_FORMAT_YUV_422_SP_HP_UV:
		bpp = 2 * rot_factor;
		break;

	case DMA_PIXEL_FORMAT_YUV_420_SP_Y:
		bpp = 2 * rot_factor;
		break;
	case DMA_PIXEL_FORMAT_YUV_420_SP_UV:
		bpp = (stretched_line_num > 0) ? 2 * rot_factor : 2;
		break;

	case DMA_PIXEL_FORMAT_YUV_422_P_HP_Y:
		bpp = 2 * rot_factor;
		break;
	case DMA_PIXEL_FORMAT_YUV_422_P_HP_U:
	case DMA_PIXEL_FORMAT_YUV_422_P_HP_V:
		bpp = 1 * rot_factor;
		break;

	case DMA_PIXEL_FORMAT_YUV_420_P_Y:
		bpp = 2 * rot_factor;
		break;
	case DMA_PIXEL_FORMAT_YUV_420_P_U:
	case DMA_PIXEL_FORMAT_YUV_420_P_V:
		bpp = (stretched_line_num > 0) ? 1 * rot_factor : 1;
		break;

	case DMA_PIXEL_FORMAT_YUV_422_SP_VP_Y:
	case DMA_PIXEL_FORMAT_YUV_422_SP_VP_UV:
	case DMA_PIXEL_FORMAT_YUV_422_P_VP_Y:
	case DMA_PIXEL_FORMAT_YUV_422_P_VP_U:
	case DMA_PIXEL_FORMAT_YUV_422_P_VP_V:
	default:
		bpp = -1;
		K3_FB_ERR("not support the format: %d\n", format);
		break;
	}

	return bpp;
}

static int k3_get_rdma_bridge_m(int chn_idx, int transform)
{
	int m = 1;

	if (transform & K3_FB_TRANSFORM_ROT_90) {
		switch (chn_idx) {
		case DPE0_CHN0:
		case DPE0_CHN1:
		case DPE0_CHN2:
		case DPE0_CHN3:
		case DPE1_CHN0:
		case DPE1_CHN1:
		case DPE2_CHN0:
		case DPE2_CHN1:
		case DPE2_CHN2:
		case DPE2_CHN3:
			m = 4;
			break;
		case DPE3_CHN0:
		case DPE3_CHN1:
		case DPE3_CHN2:
		case DPE3_CHN3:
			m = 2;
			break;
		default:
			m = -1;
			K3_FB_ERR("not support this chn%d!\n", chn_idx);
			break;
		}
	}

	return m;
}

static int k3_get_rdma_bridge_mid(dss_layer_t *layer, int dma_idx)
{
	int8_t chn_scene = 0;
	int8_t rotation = 0;
	int8_t uv_format = 0;
	int8_t scl = 0;
	int8_t index = 0;

	switch (layer->flags) {
	case DSS_SCENE_WBE0_WS_OFFLINE:
	case DSS_SCENE_WBE0_WO_OFFLINE:
		return g_rdma_bridge_mid[RDMA_MID_NUM - 2];

	case DSS_SCENE_WBE1_WS_OFFLINE:
	case DSS_SCENE_WBE1_WO_OFFLINE:
		return g_rdma_bridge_mid[RDMA_MID_NUM - 1];

	case DSS_SCENE_PDP_ONLINE:
		chn_scene = 0;
		break;
	case DSS_SCENE_SDP_ONLINE:
		chn_scene = 1;
		break;

	default:
		K3_FB_ERR("layer_idx(%d), not support this scene(%d)!",
			layer->layer_idx, layer->flags);
		return -EINVAL;
	}

	rotation = (layer->transform & K3_FB_TRANSFORM_ROT_90) ? 1 : 0;

	uv_format =((dma_idx == DSS_RDMA1) &&
		isYUVSemiPlanar(layer->src.format)) ? 1 : 0;

	scl = ((layer->src_rect.w != layer->dst_rect.w) ||
		(layer->src_rect.h != layer->dst_rect.h)) ? 1 : 0;

	index = (chn_scene << 3) | (rotation << 2) | (uv_format << 1) | scl;

	return g_rdma_bridge_mid[index];
}

static void k3_dss_rdma_bridge_init(char __iomem *rdma_bridge_base,
	dss_rdma_bridge_t *s_rdma_bridge)
{
	BUG_ON(s_rdma_bridge == NULL);
	BUG_ON(rdma_bridge_base == NULL);

	memset(s_rdma_bridge, 0, sizeof(dss_rdma_bridge_t));

	s_rdma_bridge->rbrg_rdma_ctl[0] =
		inp32(rdma_bridge_base + RBRG_RDMA0_CTL);
	s_rdma_bridge->rbrg_rdma_ctl[1] =
		inp32(rdma_bridge_base + RBRG_RDMA1_CTL);
	s_rdma_bridge->rbrg_rdma_ctl[2] =
		inp32(rdma_bridge_base + RBRG_RDMA2_CTL);
	s_rdma_bridge->rbrg_rdma_ctl[3] =
		inp32(rdma_bridge_base + RBRG_RDMA3_CTL);
	s_rdma_bridge->rbrg_rdma_ctl[4] =
		inp32(rdma_bridge_base + RBRG_RDMA4_CTL);
	s_rdma_bridge->rbrg_outstanding_dep[0] =
		inp32(rdma_bridge_base + RBRG_OUTSTANDING_DEP0);
	s_rdma_bridge->rbrg_outstanding_dep[1] =
		inp32(rdma_bridge_base + RBRG_OUTSTANDING_DEP1);
	s_rdma_bridge->rbrg_outstanding_dep[2] =
		inp32(rdma_bridge_base + RBRG_OUTSTANDING_DEP2);
	s_rdma_bridge->rbrg_outstanding_dep[3] =
		inp32(rdma_bridge_base + RBRG_OUTSTANDING_DEP3);
	s_rdma_bridge->rbrg_outstanding_dep[4] =
		inp32(rdma_bridge_base + RBRG_OUTSTANDING_DEP4);
}

static void k3_dss_rdma_bridge_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *rdma_bridge_base, dss_rdma_bridge_t *s_rdma_bridge)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(s_rdma_bridge == NULL);
	BUG_ON(rdma_bridge_base == NULL);

	k3fd->set_reg(k3fd, rdma_bridge_base + RBRG_RDMA0_CTL,
		s_rdma_bridge->rbrg_rdma_ctl[0], 32, 0);
	k3fd->set_reg(k3fd, rdma_bridge_base + RBRG_RDMA1_CTL,
		s_rdma_bridge->rbrg_rdma_ctl[1], 32, 0);
	k3fd->set_reg(k3fd, rdma_bridge_base + RBRG_RDMA2_CTL,
		s_rdma_bridge->rbrg_rdma_ctl[2], 32, 0);
	k3fd->set_reg(k3fd, rdma_bridge_base + RBRG_RDMA3_CTL,
		s_rdma_bridge->rbrg_rdma_ctl[3], 32, 0);
	k3fd->set_reg(k3fd, rdma_bridge_base + RBRG_RDMA4_CTL,
		s_rdma_bridge->rbrg_rdma_ctl[4], 32, 0);
	k3fd->set_reg(k3fd, rdma_bridge_base + RBRG_OUTSTANDING_DEP0,
		s_rdma_bridge->rbrg_outstanding_dep[0], 32, 0);
	k3fd->set_reg(k3fd, rdma_bridge_base + RBRG_OUTSTANDING_DEP1,
		s_rdma_bridge->rbrg_outstanding_dep[1], 32, 0);
	k3fd->set_reg(k3fd, rdma_bridge_base + RBRG_OUTSTANDING_DEP2,
		s_rdma_bridge->rbrg_outstanding_dep[2], 32, 0);
	k3fd->set_reg(k3fd, rdma_bridge_base + RBRG_OUTSTANDING_DEP3,
		s_rdma_bridge->rbrg_outstanding_dep[3], 32, 0);
	k3fd->set_reg(k3fd, rdma_bridge_base + RBRG_OUTSTANDING_DEP4,
		s_rdma_bridge->rbrg_outstanding_dep[4], 32, 0);
}

static int k3_set_dpe_rdma_bridge(struct k3_fb_data_type *k3fd, int dpe_type,
	dss_overlay_t *pov_req)
{
	dss_rdma_bridge_t *rdma_bridge = NULL;
	dss_layer_t *layer = NULL;

	int ret = 0;
	int i = 0;
	int k = 0;
	int dma_idx = 0;
	int dma_idx_tmp = 0;
	int dma_fomat = 0;

	int dma_axi_port[RDMA_NUM] = {0};
	int dma_bpp[RDMA_NUM] = {0};
	int dma_vscl[RDMA_NUM] = {0};
	int dma_hscl[RDMA_NUM] = {0};
	int dma_m[RDMA_NUM] = {0};
	int dma_osd[RDMA_NUM] = {0};
	int dma_mid[RDMA_NUM] = {0};
	int dma_rot[RDMA_NUM] = {0};
	int dma_osd_big[RDMA_NUM] = {0};
	int dma_rot_burst4[RDMA_NUM] = {0};

	int sum_axi0_bandwidth = 0;
	int sum_axi1_bandwidth = 0;
	int sum_axi0_osd = 0;
	int sum_axi1_osd = 0;
	int sum_axi0_osd_big = 0;
	int sum_axi1_osd_big = 0;

	int used_axi0_dma_num = 0;
	int used_axi1_dma_num = 0;
	int used_axi0_dma_rot_num = 0;
	int used_axi1_dma_rot_num = 0;

	int sum_axi0_remain_bandwidth = 0;
	int sum_axi1_remain_bandwidth = 0;
	u32 axi0_remain_bigid_num = 0;
	u32 axi1_remain_bigid_num = 0;

	u32 stretched_line_num = 0;
	dss_rect_t rdma_out_rect;
	bool need_dump = false;
	int tmp = 0;

	int req_min_cont = 0;
	int req_max_cont = 0;
	u32 rbrg_outstanding_dep = 0;
	u32 rbrg_rdma_ctl = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);
	BUG_ON((dpe_type < 0 || dpe_type >= DSS_ENG_MAX));

	if (!hasChannelBelongtoDPE(dpe_type, k3fd, pov_req))
		return 0;

	rdma_bridge = &(k3fd->dss_module.rdma_bridge[dpe_type]);
	k3fd->dss_module.rdma_bridge_used[dpe_type] = 1;

	for (k = 0; k < RDMA_NUM; k++) {
		dma_axi_port[k] = -1;
		dma_bpp[k] = 0;
		dma_vscl[k] = RDMA_OSD_MULTIPLE;
		dma_hscl[k] = RDMA_OSD_MULTIPLE;
		dma_m[k] = 1;
		dma_mid[k] = 0;
		dma_rot[k] = 0;
		dma_osd[k] = 0;
		dma_osd_big[k] = 0;
		dma_rot_burst4[k] = 0;
	}

	for (i = 0; i < pov_req->layer_nums; i++) {
		layer = &(pov_req->layer_infos[i]);

		if (layer->need_cap & (CAP_DIM |CAP_BASE))
			continue;

		if (!isChannelBelongtoDPE(dpe_type, layer->chn_idx))
			continue;

		ret = k3_dss_check_layer_par(k3fd, layer);
		if (ret != 0) {
			K3_FB_ERR("k3_dss_check_layer_par failed! ret=%d\n", ret);
			return -EINVAL;
		}

		stretched_line_num = isNeedRdmaStretchBlt(k3fd, layer);
		rdma_out_rect = k3_dss_rdma_out_rect(k3fd, layer);

		dma_idx = k3_chnIndex2dmaIndex(layer->chn_idx);
		if ((dma_idx < 0) || (dma_idx >= RDMA_NUM)) {
			K3_FB_ERR("chn_idx(%d) k3_chnIndex2dmaIndex failed! dma_idx=%d.\n",
				layer->chn_idx, dma_idx);
			return -EINVAL;
		}

		dma_fomat = k3_pixel_format_hal2dma(layer->src.format);
		if (dma_fomat < 0) {
			K3_FB_ERR("chn_idx(%d) k3_pixel_format_hal2dma failed!\n", layer->chn_idx);
			return -EINVAL;
		}

		dma_rot_burst4[dma_idx] = is_need_rdma_rot_burst4(layer) ? 1 : 0;
		dma_rot[dma_idx] = (layer->transform & K3_FB_TRANSFORM_ROT_90) ? 1 : 0;

		dma_axi_port[dma_idx] = k3_get_rdma_axi_port(layer->chn_idx, layer->flags);
		if (dma_axi_port[dma_idx] < 0) {
			K3_FB_ERR("chn_idx(%d) k3_get_rdma_axi_port failed!\n", layer->chn_idx);
			return -EINVAL;
		}

		dma_bpp[dma_idx] = k3_get_rdma_bridge_bpp(dma_fomat, layer->transform, stretched_line_num);
		if (dma_bpp[dma_idx] <= 0) {
			K3_FB_ERR("chn_idx(%d) k3_get_rdma_bridge_bpp failed!\n", layer->chn_idx);
			return -EINVAL;
		}

		if (rdma_out_rect.h > layer->dst_rect.h)
			dma_vscl[dma_idx] = RDMA_OSD_MULTIPLE * rdma_out_rect.h / layer->dst_rect.h;

		if (rdma_out_rect.w > k3fd->panel_info.xres)
			dma_hscl[dma_idx] = RDMA_OSD_MULTIPLE * rdma_out_rect.w / k3fd->panel_info.xres;

		if ((dma_vscl[dma_idx] <= 0) || (dma_hscl[dma_idx] <= 0)) {
			K3_FB_ERR("layer_idx%d, dma_idx%d, rot=%d, dma_vscl=%d, dma_hscl=%d, "
				"src_rect(%d,%d,%d,%d), dst_rect(%d,%d,%d,%d)!\n",
				layer->layer_idx, dma_idx, dma_rot[dma_idx], dma_vscl[dma_idx], dma_hscl[dma_idx],
				layer->src_rect.x, layer->src_rect.y, layer->src_rect.w, layer->src_rect.h,
				layer->dst_rect.x, layer->dst_rect.y, layer->dst_rect.w, layer->dst_rect.h);

			dma_vscl[dma_idx] = RDMA_OSD_MULTIPLE;
			dma_hscl[dma_idx] = RDMA_OSD_MULTIPLE;
		}

		dma_m[dma_idx] = k3_get_rdma_bridge_m(layer->chn_idx, layer->transform);
		if (dma_m[dma_idx] <= 0) {
			K3_FB_ERR("chn_idx(%d) k3_get_rdma_bridge_m failed!\n", layer->chn_idx);
			return -EINVAL;
		}

		dma_mid[dma_idx] = k3_get_rdma_bridge_mid(layer, dma_idx);
		if (dma_mid[dma_idx] < 0) {
			K3_FB_ERR("chn_idx(%d) dma_idx(%d) k3_get_rdma_bridge_mid failed!\n",
				layer->chn_idx, dma_idx);
			return -EINVAL;
		}

		if (dma_axi_port[dma_idx] == AXI_CH0) {
			used_axi0_dma_num++;
			if (dma_rot[dma_idx] == 1) {
				used_axi0_dma_rot_num++;
			}

			sum_axi0_bandwidth += (dma_bpp[dma_idx] * dma_vscl[dma_idx] * dma_hscl[dma_idx]);
		} else if (dma_axi_port[dma_idx] == AXI_CH1)  {
			used_axi1_dma_num++;
			if (dma_rot[dma_idx] == 1) {
				used_axi1_dma_rot_num++;
			}

			sum_axi1_bandwidth += (dma_bpp[dma_idx] * dma_vscl[dma_idx] * dma_hscl[dma_idx]);
		} else {
			WARN_ON(1);
		}

		if ((dma_idx == 0) && (isYUVSemiPlanar(layer->src.format) ||
			isYUVPlanar(layer->src.format))) {
			dma_idx_tmp = dma_idx + 1;
			dma_rot[dma_idx_tmp] = dma_rot[dma_idx];
			dma_rot_burst4[dma_idx_tmp] = dma_rot_burst4[dma_idx];
			dma_axi_port[dma_idx_tmp] = dma_axi_port[dma_idx];

			dma_bpp[dma_idx_tmp] = k3_get_rdma_bridge_bpp(dma_fomat + 1, layer->transform, stretched_line_num);
			if (dma_bpp[dma_idx_tmp] <= 0) {
				K3_FB_ERR("chn_idx(%d) dma_idx(%d) k3_get_rdma_bridge_bpp failed!\n",
					layer->chn_idx, dma_idx_tmp);
				return -EINVAL;
			}

			dma_vscl[dma_idx_tmp] = dma_vscl[dma_idx];
			dma_hscl[dma_idx_tmp] = dma_hscl[dma_idx];

			dma_m[dma_idx_tmp] = dma_m[dma_idx];
			dma_mid[dma_idx_tmp] = k3_get_rdma_bridge_mid(layer, dma_idx_tmp);
			if (dma_mid[dma_idx_tmp] < 0) {
				K3_FB_ERR("chn_idx(%d) dma_idx(%d) k3_get_rdma_bridge_mid failed!\n",
					layer->chn_idx, dma_idx_tmp);
				return -EINVAL;
			}

			if (dma_axi_port[dma_idx_tmp] == AXI_CH0) {
				used_axi0_dma_num++;
				if (dma_rot[dma_idx_tmp] == 1) {
					used_axi0_dma_rot_num++;
				}

				sum_axi0_bandwidth += (dma_bpp[dma_idx_tmp] * dma_vscl[dma_idx_tmp] * dma_hscl[dma_idx_tmp]);
			} else if (dma_axi_port[dma_idx_tmp] == AXI_CH1)  {
				used_axi1_dma_num++;
				if (dma_rot[dma_idx_tmp] == 1) {
					used_axi1_dma_rot_num++;
				}

				sum_axi1_bandwidth += (dma_bpp[dma_idx_tmp] * dma_vscl[dma_idx_tmp] * dma_hscl[dma_idx_tmp]);
			} else {
				WARN_ON(1);
			}
		}

		if ((dma_idx == 0) && isYUVPlanar(layer->src.format)) {
			dma_idx_tmp = dma_idx + 2;
			dma_rot[dma_idx_tmp] = dma_rot[dma_idx];
			dma_rot_burst4[dma_idx_tmp] = dma_rot_burst4[dma_idx];
			dma_axi_port[dma_idx_tmp] = dma_axi_port[dma_idx];

			dma_bpp[dma_idx_tmp] = k3_get_rdma_bridge_bpp(dma_fomat + 2, layer->transform, stretched_line_num);
			if (dma_bpp[dma_idx_tmp] <= 0) {
				K3_FB_ERR("chn_idx(%d) dma_idx(%d) k3_get_rdma_bridge_bpp failed!\n",
					layer->chn_idx, dma_idx_tmp);
				return -EINVAL;
			}

			dma_vscl[dma_idx_tmp] = dma_vscl[dma_idx];
			dma_hscl[dma_idx_tmp] = dma_hscl[dma_idx];

			dma_m[dma_idx_tmp] = dma_m[dma_idx];
			dma_mid[dma_idx_tmp] = k3_get_rdma_bridge_mid(layer, dma_idx_tmp);
			if (dma_mid[dma_idx_tmp] < 0) {
				K3_FB_ERR("chn_idx(%d) dma_idx(%d) k3_get_rdma_bridge_mid failed!\n",
					layer->chn_idx, dma_idx_tmp);
				return -EINVAL;
			}

			if (dma_axi_port[dma_idx_tmp] == AXI_CH0) {
				used_axi0_dma_num++;
				if (dma_rot[dma_idx_tmp] == 1) {
					used_axi0_dma_rot_num++;
				}

				sum_axi0_bandwidth += (dma_bpp[dma_idx_tmp] * dma_vscl[dma_idx_tmp] * dma_hscl[dma_idx_tmp]);
			} else if (dma_axi_port[dma_idx_tmp] == AXI_CH1)  {
				used_axi1_dma_num++;
				if (dma_rot[dma_idx_tmp] == 1) {
					used_axi1_dma_rot_num++;
				}

				sum_axi1_bandwidth += (dma_bpp[dma_idx_tmp] * dma_vscl[dma_idx_tmp] * dma_hscl[dma_idx_tmp]);
			} else {
				WARN_ON(1);
			}
		}
	}

	axi0_remain_bigid_num = AXI_OSD_BIG_SUM_MAX - 1 * used_axi0_dma_rot_num;
	axi1_remain_bigid_num = AXI_OSD_BIG_SUM_MAX - 1 * used_axi1_dma_rot_num;

	sum_axi0_remain_bandwidth = sum_axi0_bandwidth;
	sum_axi1_remain_bandwidth = sum_axi1_bandwidth;

	for (k = 0; k < RDMA_NUM; k++) {
		if (dma_axi_port[k] == AXI_CH0) {
			if (dma_rot[k] == 1) {
				dma_osd[k] = (RDMA_OSD_FORMULA - 1 * used_axi0_dma_rot_num) *
					dma_m[k] * (dma_bpp[k] * dma_vscl[k] * dma_hscl[k]) / sum_axi0_bandwidth;
				dma_osd[k] = MIN(dma_osd[k], DMA_OSD_MAX_64);
				if (dma_osd[k] <= 0) {
					dma_osd[k] = 1;
				}

				sum_axi0_remain_bandwidth -= (dma_bpp[k] * dma_vscl[k] * dma_hscl[k]);
				axi0_remain_bigid_num -= (dma_osd[k] /dma_m[k] + (((dma_osd[k] % dma_m[k]) > 0) ? 1 : 0));
			}
		} else if (dma_axi_port[k] == AXI_CH1) {
			if (dma_rot[k] == 1) {
				dma_osd[k] = (RDMA_OSD_FORMULA - 1 * used_axi1_dma_rot_num) *
					dma_m[k] * (dma_bpp[k] * dma_vscl[k] * dma_hscl[k]) / sum_axi1_bandwidth;
				dma_osd[k] = MIN(dma_osd[k], DMA_OSD_MAX_64);
				if (dma_osd[k] <= 0) {
					dma_osd[k] = 1;
				}

				sum_axi1_remain_bandwidth -= (dma_bpp[k] * dma_vscl[k] * dma_hscl[k]);
				axi1_remain_bigid_num -= (dma_osd[k] /dma_m[k] + (((dma_osd[k] % dma_m[k]) > 0) ? 1 : 0));
			}
		} else {
			; /* this channel is not used */
		}
	}

	for (k = 0; k < RDMA_NUM; k++) {
		if (dma_osd[k] != 0)
			continue;

		if (dma_axi_port[k] == AXI_CH0) {
			if ((axi0_remain_bigid_num <= 0) || (sum_axi0_remain_bandwidth <= 0)) {
				need_dump = true;
				goto err_adjust;
			}

			tmp = axi0_remain_bigid_num * (dma_bpp[k] * dma_vscl[k] * dma_hscl[k]) / sum_axi0_remain_bandwidth;
			if (tmp <= 0) {
				dma_osd[k] = 1;

				sum_axi0_remain_bandwidth -= (dma_bpp[k] * dma_vscl[k] * dma_hscl[k]);
				axi0_remain_bigid_num -= (dma_osd[k] /dma_m[k] + (((dma_osd[k] % dma_m[k]) > 0) ? 1 : 0));
			}
		} else if (dma_axi_port[k] == AXI_CH1) {
			if ((axi1_remain_bigid_num <= 0) || (sum_axi1_remain_bandwidth <= 0)) {
				need_dump = true;
				goto err_adjust;
			}

			tmp = axi1_remain_bigid_num * (dma_bpp[k] * dma_vscl[k] * dma_hscl[k]) / sum_axi1_remain_bandwidth;
			if (tmp <= 0) {
				dma_osd[k] = 1;

				sum_axi1_remain_bandwidth -= (dma_bpp[k] * dma_vscl[k] * dma_hscl[k]);
				axi1_remain_bigid_num -= (dma_osd[k] /dma_m[k] + (((dma_osd[k] % dma_m[k]) > 0) ? 1 : 0));
			}
		} else {
			; /* this channel is not used */
		}
	}

	for (k = 0; k < RDMA_NUM; k++) {
		if (dma_axi_port[k] == AXI_CH0) {
			if (dma_rot[k] == 0) {
				if (dma_osd[k] == 0)
					dma_osd[k] = axi0_remain_bigid_num * (dma_bpp[k] * dma_vscl[k] * dma_hscl[k]) / sum_axi0_remain_bandwidth;

				if ((dma_osd[k] <= 0) || (dma_osd[k] > DMA_OSD_MAX_32))
					need_dump = true;
			} else {
				if ((dma_osd[k] <= 0) || (dma_osd[k] > DMA_OSD_MAX_64))
					need_dump = true;
			}

			dma_osd_big[k] = dma_osd[k] / dma_m[k];

			sum_axi0_osd += dma_osd[k];
			sum_axi0_osd_big += dma_osd_big[k];
		} else if (dma_axi_port[k] == AXI_CH1) {
			if (dma_rot[k] == 0) {
				if (dma_osd[k] == 0)
					dma_osd[k] = axi1_remain_bigid_num * (dma_bpp[k] * dma_vscl[k] * dma_hscl[k]) / sum_axi1_remain_bandwidth;

				if ((dma_osd[k] <= 0) || (dma_osd[k] > DMA_OSD_MAX_32))
					need_dump = true;
			} else {
				if ((dma_osd[k] <= 0) || (dma_osd[k] > DMA_OSD_MAX_64))
					need_dump = true;
			}

			dma_osd_big[k] = dma_osd[k] / dma_m[k];

			sum_axi1_osd += dma_osd[k];
			sum_axi1_osd_big += dma_osd_big[k];
		} else {
			; /* this channel is not used */
		}
	}

	if ((sum_axi0_osd_big > AXI_OSD_BIG_SUM_MAX) ||
		(sum_axi1_osd_big > AXI_OSD_BIG_SUM_MAX) ||
		(sum_axi0_osd > AXI_OSD_SUM_MAX) ||
		(sum_axi1_osd > AXI_OSD_SUM_MAX)) {
		need_dump = true;
	}

	if (g_debug_ovl_osd) {
		K3_FB_WARNING("used_axi0_dma_num=%d, used_axi0_dma_rot_num=%d, "
			"sum_axi0_bandwidth=%d, axi0_remain_bigid_num=%d, sum_axi0_osd=%d, sum_axi0_osd_big=%d, !\n",
			used_axi0_dma_num, used_axi0_dma_rot_num, sum_axi0_bandwidth,
			axi0_remain_bigid_num, sum_axi0_osd, sum_axi0_osd_big);

		K3_FB_WARNING("used_axi1_dma_num=%d, used_axi1_dma_rot_num=%d, "
			"sum_axi1_bandwidth=%d, axi1_remain_bigid_num=%d, sum_axi1_osd=%d, sum_axi1_osd_big=%d, !\n",
			used_axi1_dma_num, used_axi1_dma_rot_num, sum_axi1_bandwidth,
			axi1_remain_bigid_num, sum_axi1_osd, sum_axi1_osd_big);

		for (k = 0; k < RDMA_NUM; k++) {
			K3_FB_WARNING("axi%d, dma%d, dma_osd=%d, dma_osd_big=%d, dma_rot=%d, dma_rot_burst4=%d, dma_mid=%d, "
				"(dma_m=%d, dma_bpp=%d, dma_vscl=%d, dma_hscl=%d)!\n",
				dma_axi_port[k], k, dma_osd[k],  dma_osd_big[k], dma_rot[k], dma_rot_burst4[k], dma_mid[k],
				dma_m[k], dma_bpp[k], dma_vscl[k], dma_hscl[k]);
		}
	}

err_adjust:
	if (need_dump) {
		K3_FB_WARNING("used_axi0_dma_num=%d, used_axi0_dma_rot_num=%d, "
			"sum_axi0_bandwidth=%d, axi0_remain_bigid_num=%d, sum_axi0_osd=%d, sum_axi0_osd_big=%d, !\n",
			used_axi0_dma_num, used_axi0_dma_rot_num, sum_axi0_bandwidth,
			axi0_remain_bigid_num, sum_axi0_osd, sum_axi0_osd_big);

		K3_FB_WARNING("used_axi1_dma_num=%d, used_axi1_dma_rot_num=%d, "
			"sum_axi1_bandwidth=%d, axi1_remain_bigid_num=%d, sum_axi1_osd=%d, sum_axi1_osd_big=%d, !\n",
			used_axi1_dma_num, used_axi1_dma_rot_num, sum_axi1_bandwidth,
			axi1_remain_bigid_num, sum_axi1_osd, sum_axi1_osd_big);

		for (k = 0; k < RDMA_NUM; k++) {
			K3_FB_WARNING("axi%d, dma%d, dma_osd=%d, dma_osd_big=%d, dma_rot=%d, dma_rot_burst4=%d, dma_mid=%d, "
				"(dma_m=%d, dma_bpp=%d, dma_vscl=%d, dma_hscl=%d)!\n",
				dma_axi_port[k], k, dma_osd[k], dma_osd_big[k], dma_rot[k], dma_rot_burst4[k], dma_mid[k],
				dma_m[k], dma_bpp[k], dma_vscl[k], dma_hscl[k]);

			if (dma_axi_port[k] == AXI_CH0) {
				if (dma_rot[k] == 1) {
					dma_osd[k]= (AXI_OSD_BIG_SUM_MAX / used_axi0_dma_num) * 2;
				} else {
					dma_osd[k]= (AXI_OSD_BIG_SUM_MAX / used_axi0_dma_num);
				}
			} else if (dma_axi_port[k] == AXI_CH1) {
				if (dma_rot[k] == 1) {
					dma_osd[k]= (AXI_OSD_BIG_SUM_MAX / used_axi1_dma_num) * 2;
				} else {
					dma_osd[k]= (AXI_OSD_BIG_SUM_MAX / used_axi1_dma_num);
				}
			} else {
				;
			}
		}
	}

	for (k = 0; k < RDMA_NUM; k++) {
		if (dma_axi_port[k] < 0) {
			rbrg_outstanding_dep = 0x0;
			rbrg_rdma_ctl = 0x0;
		} else {
			req_max_cont = (dma_osd[k] > RDMA_REQ_CONT_MAX) ? RDMA_REQ_CONT_MAX : dma_osd[k];
			req_max_cont -= 1;
			req_min_cont = req_max_cont / 2;

			rbrg_outstanding_dep = (dma_osd[k] | (dma_osd[k] << 8));
			rbrg_rdma_ctl = (req_min_cont | (req_max_cont << 4) | (dma_mid[k] << 8) | (dma_axi_port[k] << 12));
		}

		rdma_bridge->rbrg_outstanding_dep[k] = set_bits32(rdma_bridge->rbrg_outstanding_dep[k],
			rbrg_outstanding_dep, 16, 0);
		rdma_bridge->rbrg_rdma_ctl[k] = set_bits32(rdma_bridge->rbrg_rdma_ctl[k],
			rbrg_rdma_ctl, 13, 0);

		if (g_debug_ovl_osd) {
			K3_FB_INFO("dpe_type(%d), rdma%d, dma_axi_port(%d), dma_osd(%d), dma_mid(%d),"
				"rbrg_outstanding_dep(0x%x), rbrg_rdma_ctl(0x%x).\n",
				dpe_type, k, dma_axi_port[k], dma_osd[k], dma_mid[k],
				rbrg_outstanding_dep, rbrg_rdma_ctl);
		}
	}

	return 0;
}

int k3_dss_rdma_bridge_config(struct k3_fb_data_type *k3fd,
	dss_overlay_t *pov_req)
{
	BUG_ON(k3fd == NULL);

	if (k3fd->ovl_type == DSS_OVL_PDP) {
		if (k3_set_dpe_rdma_bridge(k3fd, DSS_ENG_DPE0, pov_req)) {
			K3_FB_ERR("k3_set_dpe_rdma_bridge for DSS_DPE0 failed!\n");
			return -EINVAL;
		}
	} else if (k3fd->ovl_type == DSS_OVL_SDP) {
		if (k3_set_dpe_rdma_bridge(k3fd, DSS_ENG_DPE1, pov_req)) {
			K3_FB_ERR("k3_set_dpe_rdma_bridge for DSS_DPE0 failed!\n");
			return -EINVAL;
		}
	} else if (k3fd->ovl_type == DSS_OVL_ADP) {
		if (k3_set_dpe_rdma_bridge(k3fd, DSS_ENG_DPE3, pov_req)) {
			K3_FB_ERR("k3_set_dpe_rdma_bridge for DSS_ENG_DPE3 failed!\n");
			return -EINVAL;
		}
	} else {
		K3_FB_ERR("not support this ovl_type %d \n", k3fd->ovl_type);
		return -EINVAL;
	}

	if (k3_set_dpe_rdma_bridge(k3fd, DSS_ENG_DPE2, pov_req)) {
		K3_FB_ERR("k3_set_dpe_rdma_bridge for DSS_ENG_DPE2 failed!\n");
		return -EINVAL;
	}

	return 0;
}


/*******************************************************************************
** DSS MMU
*/
void* k3_dss_rptb_init(void)
{
	struct gen_pool *pool = NULL;
	int order = 3;
	size_t size = MMU_RPTB_SIZE_MAX;
	u32 addr = MMU_RPTB_SIZE_ALIGN;

	pool = gen_pool_create(order, 0);
	if (pool == NULL) {
		return NULL;
	}

	if (gen_pool_add(pool, addr, size, 0) != 0) {
		gen_pool_destroy(pool);
		return NULL;
	}

	return pool;
}

void k3_dss_rptb_deinit(void *handle)
{
	if (handle) {
		gen_pool_destroy(handle);
		handle = NULL;
	}
}

u32 k3_dss_rptb_alloc(void *handle, u32 size)
{
	return gen_pool_alloc(handle, size);
}

void k3_dss_rptb_free(void *handle, u32 addr, u32 size)
{
	gen_pool_free(handle, addr, size);
}

static u32 k3_get_mmu_tlb_tag_val(u32 format, u32 transform,
	u8 is_tile, bool rdma_stretch_enable)
{
	u32 mmu_ch_ctrl_val = 0;
	int mmu_format = 0;
	u32 mmu_tlb_tag_org = 0;
	int tlb_tag_size = 0;
	int tlb_tag_num = 0;
	int tlb_tag_ctl = 0;

	mmu_format= k3_pixel_format_hal2mmu(format);
	if (mmu_format < 0) {
		K3_FB_ERR("k3_pixel_format_hal2mmu failed! mmu_format=%d.\n", mmu_format);
		return mmu_ch_ctrl_val;
	}

	if (((transform & 0x5) == 0x5) || ((transform & 0x6) == 0x6)) {
		transform = K3_FB_TRANSFORM_ROT_90;
	}

	mmu_tlb_tag_org = (mmu_format & 0x3) | ((transform & 0x7) << 2) |
		((is_tile ? 1 : 0) << 5) | ((rdma_stretch_enable ? 1 : 0) << 6);

#if 0
	K3_FB_ERR("mmu_format=%d, layer->transform=%d, layer->src.is_tile=%d,"
		" rdma_stretch_enable=%d, mmu_tlb_tag_org=0x%x\n",
		mmu_format, layer->transform, layer->src.is_tile,
		rdma_stretch_enable, mmu_tlb_tag_org);
#endif

	switch (mmu_tlb_tag_org) {
	case MMU_TLB_TAG_ORG_0x0:
	case MMU_TLB_TAG_ORG_0x4:
	case MMU_TLB_TAG_ORG_0x2:
	case MMU_TLB_TAG_ORG_0x6:
		tlb_tag_num = MMU_TAG_4;
		tlb_tag_size = MMU_TAG_128B;
		tlb_tag_ctl = MMU_TAG_RIGHT_ENDPOINT;
		break;
	case MMU_TLB_TAG_ORG_0x1:
	case MMU_TLB_TAG_ORG_0x5:
		tlb_tag_num = MMU_TAG_4;
		tlb_tag_size = MMU_TAG_64B;
		tlb_tag_ctl = MMU_TAG_RIGHT_ENDPOINT;
		break;
	case MMU_TLB_TAG_ORG_0x8:
	case MMU_TLB_TAG_ORG_0xC:
	case MMU_TLB_TAG_ORG_0xA:
	case MMU_TLB_TAG_ORG_0xE:
		tlb_tag_num = MMU_TAG_4;
		tlb_tag_size = MMU_TAG_128B;
		tlb_tag_ctl = MMU_TAG_LEFT_ENDPOINT;
		break;
	case MMU_TLB_TAG_ORG_0x9:
	case MMU_TLB_TAG_ORG_0xD:
		tlb_tag_num = MMU_TAG_4;
		tlb_tag_size = MMU_TAG_64B;
		tlb_tag_ctl = MMU_TAG_LEFT_ENDPOINT;
		break;
	case MMU_TLB_TAG_ORG_0x10:
	case MMU_TLB_TAG_ORG_0x1C:
	case MMU_TLB_TAG_ORG_0x12:
	case MMU_TLB_TAG_ORG_0x1E:
		tlb_tag_num = MMU_TAG_8;
		tlb_tag_size = MMU_TAG_64B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;
	case MMU_TLB_TAG_ORG_0x11:
	case MMU_TLB_TAG_ORG_0x1D:
		tlb_tag_num = MMU_TAG_8;
		tlb_tag_size = MMU_TAG_32B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;

	case MMU_TLB_TAG_ORG_0x20:
	case MMU_TLB_TAG_ORG_0x24:
	case MMU_TLB_TAG_ORG_0x22:
	case MMU_TLB_TAG_ORG_0x26:
		tlb_tag_num = MMU_TAG_2;
		tlb_tag_size = MMU_TAG_256B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;
	case MMU_TLB_TAG_ORG_0x21:
	case MMU_TLB_TAG_ORG_0x25:
		tlb_tag_num = MMU_TAG_2;
		tlb_tag_size = MMU_TAG_128B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;
	case MMU_TLB_TAG_ORG_0x28:
	case MMU_TLB_TAG_ORG_0x2C:
	case MMU_TLB_TAG_ORG_0x2A:
	case MMU_TLB_TAG_ORG_0x2E:
		tlb_tag_num = MMU_TAG_2;
		tlb_tag_size = MMU_TAG_256B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;
	case MMU_TLB_TAG_ORG_0x29:
	case MMU_TLB_TAG_ORG_0x2D:
		tlb_tag_num = MMU_TAG_2;
		tlb_tag_size = MMU_TAG_128B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;
	case MMU_TLB_TAG_ORG_0x30:
	case MMU_TLB_TAG_ORG_0x3C:
	case MMU_TLB_TAG_ORG_0x32:
	case MMU_TLB_TAG_ORG_0x3E:
		tlb_tag_num = MMU_TAG_8;
		tlb_tag_size = MMU_TAG_16B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;
	case MMU_TLB_TAG_ORG_0x31:
	case MMU_TLB_TAG_ORG_0x3D:
		tlb_tag_num = MMU_TAG_8;
		tlb_tag_size = MMU_TAG_16B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;

	case MMU_TLB_TAG_ORG_0x40:
	case MMU_TLB_TAG_ORG_0x44:
	case MMU_TLB_TAG_ORG_0x42:
	case MMU_TLB_TAG_ORG_0x46:
		tlb_tag_num = MMU_TAG_8;
		tlb_tag_size = MMU_TAG_32B;
		tlb_tag_ctl = MMU_TAG_RIGHT_ENDPOINT;
		break;
	case MMU_TLB_TAG_ORG_0x41:
	case MMU_TLB_TAG_ORG_0x45:
		tlb_tag_num = MMU_TAG_8;
		tlb_tag_size = MMU_TAG_32B;
		tlb_tag_ctl = MMU_TAG_RIGHT_ENDPOINT;
		break;
	case MMU_TLB_TAG_ORG_0x48:
	case MMU_TLB_TAG_ORG_0x4C:
	case MMU_TLB_TAG_ORG_0x4A:
	case MMU_TLB_TAG_ORG_0x4E:
		tlb_tag_num = MMU_TAG_8;
		tlb_tag_size = MMU_TAG_32B;
		tlb_tag_ctl = MMU_TAG_LEFT_ENDPOINT;
		break;
	case MMU_TLB_TAG_ORG_0x49:
	case MMU_TLB_TAG_ORG_0x4D:
		tlb_tag_num = MMU_TAG_8;
		tlb_tag_size = MMU_TAG_32B;
		tlb_tag_ctl = MMU_TAG_LEFT_ENDPOINT;
		break;
	case MMU_TLB_TAG_ORG_0x50:
	case MMU_TLB_TAG_ORG_0x5C:
	case MMU_TLB_TAG_ORG_0x52:
	case MMU_TLB_TAG_ORG_0x5E:
		tlb_tag_num = MMU_TAG_8;
		tlb_tag_size = MMU_TAG_32B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;
	case MMU_TLB_TAG_ORG_0x51:
	case MMU_TLB_TAG_ORG_0x5D:
		tlb_tag_num = MMU_TAG_8;
		tlb_tag_size = MMU_TAG_32B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;

	case MMU_TLB_TAG_ORG_0x60:
	case MMU_TLB_TAG_ORG_0x64:
	case MMU_TLB_TAG_ORG_0x62:
	case MMU_TLB_TAG_ORG_0x66:
		tlb_tag_num = MMU_TAG_2;
		tlb_tag_size = MMU_TAG_256B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;
	case MMU_TLB_TAG_ORG_0x61:
	case MMU_TLB_TAG_ORG_0x65:
		tlb_tag_num = MMU_TAG_2;
		tlb_tag_size = MMU_TAG_128B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;
	case MMU_TLB_TAG_ORG_0x68:
	case MMU_TLB_TAG_ORG_0x6C:
	case MMU_TLB_TAG_ORG_0x6A:
	case MMU_TLB_TAG_ORG_0x6E:
		tlb_tag_num = MMU_TAG_2;
		tlb_tag_size = MMU_TAG_256B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;
	case MMU_TLB_TAG_ORG_0x69:
	case MMU_TLB_TAG_ORG_0x6D:
		tlb_tag_num = MMU_TAG_2;
		tlb_tag_size = MMU_TAG_128B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;
	case MMU_TLB_TAG_ORG_0x70:
	case MMU_TLB_TAG_ORG_0x7C:
	case MMU_TLB_TAG_ORG_0x72:
	case MMU_TLB_TAG_ORG_0x7E:
		tlb_tag_num = MMU_TAG_8;
		tlb_tag_size = MMU_TAG_16B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;
	case MMU_TLB_TAG_ORG_0x71:
	case MMU_TLB_TAG_ORG_0x7D:
		tlb_tag_num = MMU_TAG_8;
		tlb_tag_size = MMU_TAG_16B;
		tlb_tag_ctl = MMU_TAG_NEW;
		break;

	default:
		K3_FB_ERR("not support this mmu_tlb_tag_org(0x%x)!\n", mmu_tlb_tag_org);
		break;
	}

	mmu_ch_ctrl_val = (tlb_tag_num << 4) | (tlb_tag_size << 6) | (tlb_tag_ctl << 14);

	return mmu_ch_ctrl_val;
}

static int k3_rptb_pt_dma_sel(int chn_idx, int format)
{
	int ret = 0;
	bool is_yuv_semi_planar = false;
	bool is_yuv_planar = false;

	BUG_ON((chn_idx < 0) || (chn_idx >= DSS_CHN_MAX));

	is_yuv_semi_planar = isYUVSemiPlanar(format);
	is_yuv_planar = isYUVPlanar(format);

	switch (chn_idx) {
	case DPE0_CHN0:
		if (is_yuv_semi_planar) {
			ret = 0x1;
		} else {
			ret = 0x0;
		}
		break;
	case WBE0_CHN1:
		ret = 0x2;
		break;
	case DPE2_CHN0:
		if (is_yuv_semi_planar) {
			ret = 0x4;
		} else {
			ret = 0x3;
		}
		break;
	case WBE1_CHN1:
		ret = 0x5;
		break;
	case DPE3_CHN0:
		if (is_yuv_planar) {
			ret = 0x8;
		} else {
			ret = 0x7;
		}
		break;
	case DPE3_CHN1:
		ret = 0x6;
		break;
	case DPE3_CHN2:
		ret = 0x9;
		break;

	default:
		break;
	}

	return ret;
}

static void k3_dss_mmu_dma_init(char __iomem *mmu_dma_base,
	dss_mmu_dma_t *s_mmu_dma)
{
	BUG_ON(mmu_dma_base == NULL);
	BUG_ON(s_mmu_dma == NULL);

	memset(s_mmu_dma, 0, sizeof(dss_mmu_dma_t));

	s_mmu_dma->mmu_ch_ctrl = inp32(mmu_dma_base + MMU_CH_CTRL);
	s_mmu_dma->mmu_ch_ptba = inp32(mmu_dma_base + MMU_CH_PTBA);
	s_mmu_dma->mmu_ch_ptva = inp32(mmu_dma_base + MMU_CH_PTVA);
	s_mmu_dma->mmu_ch_amsk = inp32(mmu_dma_base + MMU_CH_AMSK);
}

static void k3_dss_mmu_dma_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *mmu_dma_base, dss_mmu_dma_t *s_mmu_dma)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(mmu_dma_base == NULL);
	BUG_ON(s_mmu_dma == NULL);

	k3fd->set_reg(k3fd, mmu_dma_base + MMU_CH_CTRL,
		s_mmu_dma->mmu_ch_ctrl , 32, 0);
	k3fd->set_reg(k3fd, mmu_dma_base + MMU_CH_PTBA,
		s_mmu_dma->mmu_ch_ptba, 32, 0);
	k3fd->set_reg(k3fd, mmu_dma_base + MMU_CH_PTVA,
		s_mmu_dma->mmu_ch_ptva, 32, 0);
	k3fd->set_reg(k3fd, mmu_dma_base + MMU_CH_AMSK,
		s_mmu_dma->mmu_ch_amsk, 32, 0);
	k3fd->set_reg(k3fd, mmu_dma_base + MMU_AXI_ARB,
		0xD51, 32, 0);
}

static void k3_dss_mmu_rptb_init(char __iomem *mmu_rptb_base,
	dss_mmu_rptb_t *s_mmu_rptb)
{
	int i = 0;

	BUG_ON(mmu_rptb_base == NULL);
	BUG_ON(s_mmu_rptb == NULL);

	memset(s_mmu_rptb, 0, sizeof(dss_mmu_rptb_t));

	s_mmu_rptb->mmu_rptb_ctl = inp32(mmu_rptb_base + MMU_RPTB_CTL);
	for (i = 0; i < DSS_ROT_MAX; i++) {
		s_mmu_rptb->mmu_rptb_load[i] = inp32(mmu_rptb_base + MMU_RPTB_LOAD0 + i * 0x4);
		s_mmu_rptb->mmu_rptb_ba[i] = inp32(mmu_rptb_base + MMU_RPTB_BA0 + i * 0x4);
	}
	s_mmu_rptb->mmu_rtlb_sel = inp32(mmu_rptb_base + MMU_RTLB_SEL);
}

static void k3_dss_mmu_rptb_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *mmu_rptb_base, dss_mmu_rptb_t *s_mmu_rptb)
{
	int i = 0 ;

	BUG_ON(k3fd == NULL);
	BUG_ON(mmu_rptb_base == NULL);
	BUG_ON(s_mmu_rptb == NULL);

	k3fd->set_reg(k3fd, mmu_rptb_base + MMU_RPTB_CTL,
			s_mmu_rptb->mmu_rptb_ctl, 32, 0);

	for (i = 0; i < DSS_ROT_MAX; i++) {
		if (s_mmu_rptb->mmu_rptb_idx_used[i] == 1) {
			k3fd->set_reg(k3fd, mmu_rptb_base + MMU_RPTB_LOAD0 + i * 0x04,
				s_mmu_rptb->mmu_rptb_load[i], 32, 0);
			k3fd->set_reg(k3fd, mmu_rptb_base + MMU_RPTB_BA0 + i * 0x04,
				s_mmu_rptb->mmu_rptb_ba[i], 32, 0);
		}
	}

	k3fd->set_reg(k3fd, mmu_rptb_base + MMU_RTLB_SEL,
		s_mmu_rptb->mmu_rtlb_sel, 32, 0);
}

int k3_dss_mmu_config(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req,
	dss_layer_t *layer, dss_wb_layer_t *wb_layer, bool rdma_stretch_enable)
{
	dss_mmu_dma_t *mmu_dma0 = NULL;
	dss_mmu_dma_t *mmu_dma1 = NULL;
	dss_mmu_dma_t *mmu_dma2 = NULL;
	dss_mmu_rptb_t *mmu_rptb = NULL;
	bool is_yuv_semi_planar = false;
	bool is_yuv_planar = false;
	u32 ptba = 0;
	u32 ptva = 0;
	int rot_idx = 0;

	int axi_cmd_mid = 0;
	int tlb_pri_thr = 0;
	int tlb_pri_force = 0;

	int tlb_cmd_accu = 0;
	int tlb_rtlb_sel = 0;
	int pref_va_ctl = 0;
	int tlb_flush = 0;
	int tlb_rtlb_en = 0;
	int tlb_en = 0;

	u32 rptb_size = 0;
	u32 rptb_offset = 0;
	u32 rptb_ba = 0;
	u32 rptb_dma_sel = 0;
	u32 buf_size = 0;

	int chn_idx = 0;
	u32 transform = 0;
	u32 flags = 0;
	dss_img_t *img = 0;
	dss_rptb_info_t *rptb_info = NULL;
	u32 need_cap = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);
	BUG_ON((layer == NULL) && (wb_layer == NULL));
	BUG_ON(layer && wb_layer);

	if (wb_layer) {
		img = &(wb_layer->dst);
		chn_idx = wb_layer->chn_idx;
		transform = wb_layer->transform;
		flags = wb_layer->flags;
		need_cap = wb_layer->need_cap;
		if (chn_idx == WBE1_CHN1)
			rptb_info = &(k3fd->dss_wb_rptb_info_cur[1]);
		else
			rptb_info = &(k3fd->dss_wb_rptb_info_cur[0]);
	} else {
		img = &(layer->src);
		chn_idx = layer->chn_idx;
		transform = layer->transform;
		flags = layer->flags;
		need_cap = layer->need_cap;

		if (k3fd->ovl_type == DSS_OVL_ADP) {
			if (pov_req->wb_layer_info.chn_idx == WBE1_CHN1)
				rptb_info = &(k3fd->dss_rptb_info_cur[1]);
			else
				rptb_info = &(k3fd->dss_rptb_info_cur[0]);
		} else {
			rptb_info = &(k3fd->dss_rptb_info_cur[0]);
		}
	}

	is_yuv_semi_planar = isYUVSemiPlanar(img->format);
	is_yuv_planar = isYUVPlanar(img->format);

	mmu_dma0 = &(k3fd->dss_module.mmu_dma0[chn_idx]);
	k3fd->dss_module.mmu_dma0_used[chn_idx] = 1;
	if (is_yuv_semi_planar || is_yuv_planar) {
		mmu_dma1 = &(k3fd->dss_module.mmu_dma1[chn_idx]);
		k3fd->dss_module.mmu_dma1_used[chn_idx] = 1;
	}
	if (is_yuv_planar) {
		mmu_dma2 = &(k3fd->dss_module.mmu_dma0[chn_idx + 1]);
		k3fd->dss_module.mmu_dma0_used[chn_idx + 1] = 1;
	}

	if (img->ptba & (MMU_CH_PTBA_ALIGN - 1)) {
		K3_FB_ERR("chn%d ptba(0x%x) is not %d bytes aligned.\n",
			chn_idx, img->ptba, MMU_CH_PTBA_ALIGN);
		return -EINVAL;
	}

	if (img->ptva & (MMU_CH_PTVA_ALIGN - 1)) {
		K3_FB_ERR("chn%d ptva(0x%x) is not %d bytes aligned.\n",
			chn_idx, img->ptba, MMU_CH_PTVA_ALIGN);
		return -EINVAL;
	}

	tlb_en = 1;
	ptba = img->ptba;
	ptva = img->ptva;

	if (transform & K3_FB_TRANSFORM_ROT_90) {
		rot_idx = k3_get_rot_index(chn_idx);
		if (rot_idx < 0 || (need_cap & (CAP_BASE | CAP_DIM))) {
			K3_FB_ERR("chn(%d), failed to k3_get_rot_index! rot_idx=%d", chn_idx, rot_idx);
			goto err_rptb;
		}

		rptb_dma_sel = k3_rptb_pt_dma_sel(chn_idx, img->format);

		if (rptb_info->rptb_size[rot_idx] <= 0) {
			buf_size = img->buf_size;
			rptb_size = ALIGN_UP(((((buf_size + MMU_PAGE_SIZE - 1) / MMU_PAGE_SIZE) << 2) +
				(MMU_RPTB_WIDTH - 1)) / MMU_RPTB_WIDTH, MMU_RPTB_SIZE_ALIGN);

			rptb_offset = k3_dss_rptb_alloc(g_rptb_gen_pool, rptb_size);
			if (rptb_offset < MMU_RPTB_SIZE_ALIGN) {
				K3_FB_ERR("k3_dss_rptb_alloc failed! rot_idx=%d, rptb_siz=%d, rptb_offset=%d.\n",
					rot_idx, rptb_size, rptb_offset);
				goto err_rptb;
			}

			rptb_info->rptb_offset[rot_idx] = rptb_offset;
			rptb_info->rptb_size[rot_idx] = rptb_size;
		} else {
			rptb_offset = rptb_info->rptb_offset[rot_idx];
			rptb_size = rptb_info->rptb_size[rot_idx];
			//rptb_size = 0x0;
		}

		tlb_rtlb_sel = rot_idx;
		tlb_rtlb_en = 1;
		pref_va_ctl = 1;
		rptb_ba = (img->ptba + ((img->vir_addr - img->ptva) >> 10)) & 0xFFFFFF80;
		rptb_offset -= MMU_RPTB_SIZE_ALIGN;

		ptba = rptb_offset << 4;
		ptva = img->vir_addr & 0xFFFE0000;

		mmu_rptb = &(k3fd->dss_module.mmu_rptb);
		mmu_rptb->mmu_rptb_idx_used[rot_idx] = 1;
		k3fd->dss_module.mmu_rptb_used = 1;

		mmu_rptb->mmu_rptb_ctl = set_bits32(mmu_rptb->mmu_rptb_ctl, 0x1, 1, rot_idx);
		mmu_rptb->mmu_rptb_ctl = set_bits32(mmu_rptb->mmu_rptb_ctl, 0x1, 1, (26 + rot_idx));
		mmu_rptb->mmu_rptb_ctl = set_bits32(mmu_rptb->mmu_rptb_ctl, 0x7, 3, 13);
		mmu_rptb->mmu_rptb_load[rot_idx] = set_bits32(mmu_rptb->mmu_rptb_load[rot_idx],
			(rptb_offset | (rptb_size << 12)), 24, 0);
		mmu_rptb->mmu_rptb_ba[rot_idx] = set_bits32(mmu_rptb->mmu_rptb_ba[rot_idx],
			rptb_ba, 32, 0);
		mmu_rptb->mmu_rtlb_sel = set_bits32(mmu_rptb->mmu_rtlb_sel, 0x1, 1, (27 + rot_idx));
		mmu_rptb->mmu_rtlb_sel = set_bits32(mmu_rptb->mmu_rtlb_sel, rptb_dma_sel, 4, (rot_idx * 4));

		if (k3fd->dss_glb.rot_tlb_scene_base[rot_idx]) {
			k3fd->dss_glb.rot_tlb_scene_used[rot_idx] = 1;
			k3fd->dss_glb.rot_tlb_scene_val[rot_idx] =
				set_bits32(k3fd->dss_glb.rot_tlb_scene_val[rot_idx], flags, 3, 0);
		} else {
			K3_FB_ERR("rot_tlb_scene_base[%d] is invalid!\n", rot_idx);
		}
	}

err_rptb:
	mmu_dma0->mmu_ch_ctrl = set_bits32(mmu_dma0->mmu_ch_ctrl,
		k3_get_mmu_tlb_tag_val(img->format, transform, img->is_tile, rdma_stretch_enable) |
		tlb_en | (tlb_rtlb_en << 1) | (tlb_flush << 2) |(pref_va_ctl << 3) | (tlb_rtlb_sel << 9) |
		(tlb_cmd_accu << 12) | (tlb_pri_force << 21) | (tlb_pri_thr << 22) |
		(axi_cmd_mid << 25), 29, 0);
	mmu_dma0->mmu_ch_ptba = set_bits32(mmu_dma0->mmu_ch_ptba, ptba, 32, 0);
	mmu_dma0->mmu_ch_ptva = set_bits32(mmu_dma0->mmu_ch_ptva, ptva, 32, 0);
	mmu_dma0->mmu_ch_amsk = set_bits32(mmu_dma0->mmu_ch_amsk,
		((img->stride + MMU_CH_AMSK_THRESHOLD - 1) /
		MMU_CH_AMSK_THRESHOLD + 1), 7, 12);

	if (is_yuv_semi_planar || is_yuv_planar) {
		memcpy(mmu_dma1, mmu_dma0, sizeof(dss_mmu_dma_t));
	}
	if (is_yuv_planar) {
		memcpy(mmu_dma2, mmu_dma0, sizeof(dss_mmu_dma_t));
	}

	return 0;
}


/*******************************************************************************
** DSS RDMA
*/
static int k3_get_rdma_tile_interleave(u32 stride)
{
	int i = 0;
	u32 interleave[MAX_TILE_SURPORT_NUM] = {256, 512, 1024, 2048, 4096, 8192};

	for (i = 0; i < MAX_TILE_SURPORT_NUM; i++) {
		if (interleave[i] == stride)
			return MIN_INTERLEAVE + i;
	}

	return 0;
}

static int fix_scf_input(dss_layer_t *layer)
{
	if ((layer->need_cap & CAP_SCL) == 0)
		return 0;

	/* if layer has no h scf, do not pad 3 pix */
	if (((layer->block_info.h_ratio != SCF_CONST_FACTOR) && (layer->block_info.h_ratio != 0))
		|| ((layer->block_info.v_ratio != SCF_CONST_FACTOR) && (layer->block_info.v_ratio != 0))) {
		/* offline block case */
                if (layer->block_info.h_ratio == SCF_CONST_FACTOR)
                        return 0;
                else if (layer->src_rect.w >= SCF_MIN_INPUT
                        && (layer->transform & K3_FB_TRANSFORM_ROT_90) == 0)
                        return 0;
                else if (layer->src_rect.h >= SCF_MIN_INPUT
                        && layer->transform & K3_FB_TRANSFORM_ROT_90)
                        return 0;
	} else {
		/* online or offline no block case */
		if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
			if (layer->src_rect.w == layer->dst_rect.h)
				return 0;
		} else {
			if (layer->src_rect.w == layer->dst_rect.w)
				return 0;
		}
	}

	/* online and offline no block case */
	switch (layer->transform) {
		case K3_FB_TRANSFORM_NOP:
		case K3_FB_TRANSFORM_FLIP_V:
		case K3_FB_TRANSFORM_FLIP_H:
		case K3_FB_TRANSFORM_ROT_180:
			if (layer->src_rect.w >= SCF_MIN_INPUT)
				return 0;

			if (layer->block_info.first_tile && layer->block_info.last_tile) {
				K3_FB_ERR("scf layer[%d] size is too small{%d,%d,%d,%d}\n",
					layer->chn_idx,
					layer->src_rect.x, layer->src_rect.y,
					layer->src_rect.w, layer->src_rect.h);
				return 1;
			} else if (layer->block_info.first_tile) {
				if (layer->transform == K3_FB_TRANSFORM_FLIP_H
                                        || layer->transform == K3_FB_TRANSFORM_ROT_180)
					layer->src_rect.x = layer->src_rect.x - SCF_MIN_OFFSET;
			} else {
                                /* if here,last_tile must to be 1, because middle block pix > 6 */
				if (layer->transform == 0 || layer->transform == K3_FB_TRANSFORM_FLIP_V)
					layer->src_rect.x = layer->src_rect.x - SCF_MIN_OFFSET;

                                layer->block_info.acc_hscl += (SCF_MIN_OFFSET << 18);
			}
			layer->src_rect.w = layer->src_rect.w + SCF_MIN_OFFSET;

			break;
                case K3_FB_TRANSFORM_FLIP_H | K3_FB_TRANSFORM_ROT_90:
                case K3_FB_TRANSFORM_FLIP_V | K3_FB_TRANSFORM_ROT_90:
                case K3_FB_TRANSFORM_ROT_90:
                case K3_FB_TRANSFORM_ROT_270:
			if (layer->src_rect.h >= SCF_MIN_INPUT)
				return 0;

			if (layer->block_info.first_tile && layer->block_info.last_tile) {
				K3_FB_ERR("scf layer[%d] size is too small{%d,%d,%d,%d}\n",
					layer->chn_idx,
					layer->src_rect.x, layer->src_rect.y,
					layer->src_rect.w, layer->src_rect.h);
				return 1;
			} else if (layer->block_info.first_tile) {
				if (layer->transform == K3_FB_TRANSFORM_ROT_90
                                        || layer->transform == (K3_FB_TRANSFORM_ROT_90 | K3_FB_TRANSFORM_FLIP_H))
					layer->src_rect.y = layer->src_rect.y - SCF_MIN_OFFSET;
			} else {
                                /* if here,last_tile must to be 1, because middle block pix > 6 */
				if (layer->transform == K3_FB_TRANSFORM_ROT_270
                                        || layer->transform == (K3_FB_TRANSFORM_ROT_90 | K3_FB_TRANSFORM_FLIP_V))
					layer->src_rect.y = layer->src_rect.y - SCF_MIN_OFFSET;

                                layer->block_info.acc_hscl += (SCF_MIN_OFFSET << 18);
			}
			layer->src_rect.h = layer->src_rect.h + SCF_MIN_OFFSET;
			break;
		default:
			K3_FB_ERR("unknow dss_layer->transform: %d\n",layer->transform);
			return 1;
	}

	if (layer->src_rect.h < SCF_MIN_INPUT && layer->src_rect.w < SCF_MIN_INPUT) {
		K3_FB_ERR("read more data,but scf[%d] still not satisfy 6*6 ! dump layer{%d,%d,%d,%d},transform[%d]\n",
			layer->chn_idx,
			layer->src_rect.x, layer->src_rect.y,
			layer->src_rect.w, layer->src_rect.h,
			layer->transform);
		return 1;
	}

	return 0;
}

static void k3_dss_rdma_init(char __iomem *rdma_base, dss_rdma_t *s_rdma)
{
	BUG_ON(rdma_base == NULL);
	BUG_ON(s_rdma == NULL);

	s_rdma->rdma_data_addr = inp32(rdma_base + RDMA_DATA_ADDR);
	s_rdma->rdma_oft_x0 = inp32(rdma_base + RDMA_OFT_X0);
	s_rdma->rdma_oft_y0 = inp32(rdma_base + RDMA_OFT_Y0);
	s_rdma->rdma_rot_qos_lev = inp32(rdma_base + RDMA_ROT_QOS_LEV);
	s_rdma->rdma_stride = inp32(rdma_base + RDMA_STRIDE);
	s_rdma->rdma_oft_x1 = inp32(rdma_base + RDMA_OFT_X1);
	s_rdma->rdma_oft_y1 = inp32(rdma_base + RDMA_OFT_Y1);
	s_rdma->rdma_mask0 = inp32(rdma_base + RDMA_MASK0);
	s_rdma->rdma_mask1 = inp32(rdma_base + RDMA_MASK1);
	s_rdma->rdma_stretch_size_vrt = inp32(rdma_base + RDMA_STRETCH_SIZE_VRT);
	s_rdma->rdma_stretch_line_num = inp32(rdma_base + RDMA_STRETCH_LINE_NUM);
	//s_rdma->rdma_ctrl = inp32(rdma_base + RDMA_CTRL);
	s_rdma->rdma_ctrl = 0x0;
	s_rdma->rdma_data_num = inp32(rdma_base + RDMA_DATA_NUM);
	s_rdma->rdma_tile_scram = inp32(rdma_base + RDMA_TILE_SCRAM);
}

static void k3_dss_rdma_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *rdma_base, dss_rdma_t *s_dma)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(rdma_base == NULL);
	BUG_ON(s_dma == NULL);

	k3fd->set_reg(k3fd, rdma_base + RDMA_DATA_ADDR, s_dma->rdma_data_addr, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_OFT_X0, s_dma->rdma_oft_x0, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_OFT_Y0, s_dma->rdma_oft_y0, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_ROT_QOS_LEV, s_dma->rdma_rot_qos_lev, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_STRIDE, s_dma->rdma_stride, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_OFT_X1, s_dma->rdma_oft_x1, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_OFT_Y1, s_dma->rdma_oft_y1, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_MASK0, s_dma->rdma_mask0, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_MASK1, s_dma->rdma_mask1, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_STRETCH_SIZE_VRT, s_dma->rdma_stretch_size_vrt, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_STRETCH_LINE_NUM, s_dma->rdma_stretch_line_num, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_CTRL, s_dma->rdma_ctrl, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_DATA_NUM, s_dma->rdma_data_num, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_TILE_SCRAM, s_dma->rdma_tile_scram, 32, 0);
	k3fd->set_reg(k3fd, rdma_base + RDMA_BAK, 0x1, 32, 0);
}

static int k3_dss_dfc_clip_check(dss_rect_ltrb_t *clip_rect)
{
	if ((clip_rect->left < 0 || clip_rect->left > DFC_MAX_CLIP_NUM) ||
		(clip_rect->right < 0 || clip_rect->right > DFC_MAX_CLIP_NUM) ||
		(clip_rect->top < 0 || clip_rect->top > DFC_MAX_CLIP_NUM) ||
		(clip_rect->bottom < 0 || clip_rect->bottom > DFC_MAX_CLIP_NUM)) {
		return 1;
	}

	return 0;
}

dss_rect_t k3_dss_rdma_out_rect(struct k3_fb_data_type *k3fd, dss_layer_t *layer)
{
	dss_rect_t out_aligned_rect;
	dss_rect_ltrb_t aligned_rect;
	dss_rect_t new_src_rect;
	int aligned_pixel = 0;
	int bpp = 0;
	u32 stretched_line_num = 0;
	u32 temp = 0;

	int dfc_fmt = 0;
	int dfc_bpp = 0;
	int dfc_aligned = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(layer == NULL);

	new_src_rect = layer->src_rect;
	stretched_line_num = isNeedRdmaStretchBlt(k3fd, layer);

	bpp = (isYUVSemiPlanar(layer->src.format) || isYUVPlanar(layer->src.format))
		? 1 : layer->src.bpp;
	aligned_pixel = DMA_ALIGN_BYTES / bpp;

	dfc_fmt = k3_pixel_format_hal2dfc(layer->src.format, layer->transform);
	if (dfc_fmt < 0) {
		K3_FB_ERR("layer format (%d) not support !\n", layer->src.format);
	}

	dfc_bpp = k3_dfc_get_bpp(dfc_fmt);
	if (dfc_bpp <= 0) {
		K3_FB_ERR("dfc_bpp(%d) not support !\n", dfc_bpp);
	}

	dfc_aligned = (dfc_bpp <= 2) ? 4 : 2;

	/* aligned_rect */
	if (is_YUV_P_420(layer->src.format) || is_YUV_P_422(layer->src.format)) {
		aligned_rect.left = ALIGN_DOWN(new_src_rect.x, 2 * aligned_pixel);
		aligned_rect.right = ALIGN_UP(new_src_rect.x + new_src_rect.w, 2 * aligned_pixel) - 1;
	} else {
		aligned_rect.left = ALIGN_DOWN(new_src_rect.x, aligned_pixel);
		aligned_rect.right = ALIGN_UP(new_src_rect.x + new_src_rect.w, aligned_pixel) - 1;
	}

	if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
		if (stretched_line_num > 0) {
			new_src_rect.h = ALIGN_DOWN(new_src_rect.h / stretched_line_num, dfc_aligned) *
				stretched_line_num;

			if (is_YUV_SP_420(layer->src.format) || is_YUV_P_420(layer->src.format)) {
				aligned_rect.top = ALIGN_DOWN(new_src_rect.y, 2);
				aligned_rect.bottom = DSS_HEIGHT(aligned_rect.top + new_src_rect.h);
			} else {
				aligned_rect.top = new_src_rect.y;
				aligned_rect.bottom = DSS_HEIGHT(new_src_rect.y + new_src_rect.h);
			}
		} else {
			aligned_rect.top = ALIGN_DOWN(new_src_rect.y, aligned_pixel);
			aligned_rect.bottom = ALIGN_UP(new_src_rect.y + new_src_rect.h, aligned_pixel) - 1;
		}
	} else {
		if (is_YUV_SP_420(layer->src.format) || is_YUV_P_420(layer->src.format)) {
			aligned_rect.top = ALIGN_DOWN(new_src_rect.y, 2);
			aligned_rect.bottom = ALIGN_UP(new_src_rect.y + new_src_rect.h, 2) - 1;
		} else {
			aligned_rect.top = new_src_rect.y;
			aligned_rect.bottom = DSS_HEIGHT(new_src_rect.y + new_src_rect.h);
		}
	}

	/* out_rect */
	out_aligned_rect.x = 0;
	out_aligned_rect.y = 0;
	out_aligned_rect.w = aligned_rect.right - aligned_rect.left + 1;
	out_aligned_rect.h = aligned_rect.bottom - aligned_rect.top + 1;
	if (stretched_line_num > 0) {
		temp = (out_aligned_rect.h / stretched_line_num) +
			((out_aligned_rect.h % stretched_line_num) ? 1 : 0) - 1;

		out_aligned_rect.h = temp + 1;
	}

	if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
		temp = out_aligned_rect.w;
		out_aligned_rect.w = out_aligned_rect.h;
		out_aligned_rect.h = temp;
	}

	return out_aligned_rect;
}

int k3_dss_rdma_config(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req,
	dss_layer_t *layer, dss_rect_ltrb_t *clip_rect,
	dss_rect_t *out_aligned_rect, bool *rdma_stretch_enable)
{
	dss_rdma_t *dma0 = NULL;
	dss_rdma_t *dma1 = NULL;
	dss_rdma_t *dma2 = NULL;

	bool mmu_enable = false;
	bool is_yuv_semi_planar = false;
	bool is_yuv_planar = false;
	bool src_rect_mask_enable = false;

	u32 rdma_addr = 0;
	u32 rdma_stride = 0;
	int rdma_format = 0;
	int rdma_transform = 0;
	int rdma_data_num = 0;
	u32 stretch_size_vrt = 0;
	u32 stretched_line_num = 0;
	u32 stretched_stride = 0;
	u32 rdma_rot_burst4 = 0;

	int bpp = 0;
	int aligned_pixel = 0;
	int rdma_oft_x0 = 0;
	int rdma_oft_y0 = 0;
	int rdma_oft_x1 = 0;
	int rdma_oft_y1 = 0;
	int rdma_mask_x0 = 0;
	int rdma_mask_y0 = 0;
	int rdma_mask_x1 = 0;
	int rdma_mask_y1 = 0;

	int chn_idx = 0;
	u32 l2t_interleave_n = 0;

	dss_rect_ltrb_t aligned_rect;
	dss_rect_t new_src_rect;
	u32 temp = 0;
	int tmp = 0;

	int dfc_fmt = 0;
	int dfc_bpp = 0;
	int dfc_aligned = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(layer == NULL);
	BUG_ON(pov_req == NULL);

	if (k3fd->ovl_type == DSS_OVL_ADP) {
		if (fix_scf_input(layer) != 0) {
			K3_FB_ERR("fix_scf_input err\n");
			return -EINVAL;
		}
	}

	chn_idx = layer->chn_idx;
	new_src_rect = layer->src_rect;

	stretched_line_num = isNeedRdmaStretchBlt(k3fd, layer);
	*rdma_stretch_enable = (stretched_line_num > 0) ? true : false;
	src_rect_mask_enable = isSrcRectMasked(layer);

	mmu_enable = (layer->src.mmu_enable == 1) ? true : false;
	is_yuv_semi_planar = isYUVSemiPlanar(layer->src.format);
	is_yuv_planar = isYUVPlanar(layer->src.format);

#if 0
	if (is_yuv_semi_planar || is_yuv_planar) {
		if (!IS_EVEN(new_src_rect.h)) {
			K3_FB_ERR("YUV semi_planar or planar format(%d), src_rect's height(%d) is invalid!\n",
				layer->src.format, new_src_rect.h);
		}
	}
#endif

	dma0 = &(k3fd->dss_module.rdma0[chn_idx]);
	k3fd->dss_module.rdma0_used[chn_idx] = 1;
	if (is_yuv_semi_planar || is_yuv_planar) {
		dma1 = &(k3fd->dss_module.rdma1[chn_idx]);
		k3fd->dss_module.rdma1_used[chn_idx] = 1;
	}
	if (is_yuv_planar) {
		dma2 = &(k3fd->dss_module.rdma0[chn_idx + 1]);
		k3fd->dss_module.rdma0_used[chn_idx + 1] = 1;
	}

	if ((chn_idx >= DPE3_CHN0) && (chn_idx <= DPE3_CHN3) &&
		(layer->transform & K3_FB_TRANSFORM_ROT_90) &&
		(!layer->src.is_tile) && (layer->src.bpp == 4)) {
		rdma_rot_burst4 = 1;
	}

	rdma_addr = mmu_enable ? layer->src.vir_addr : layer->src.phy_addr;
	if (rdma_addr & (DMA_ADDR_ALIGN - 1)) {
		K3_FB_ERR("layer%d rdma_addr(0x%x) is not %d bytes aligned.\n",
			layer->layer_idx, rdma_addr, DMA_ADDR_ALIGN);
		return -EINVAL;
	}

	if (layer->src.stride & (DMA_STRIDE_ALIGN - 1)) {
		K3_FB_ERR("layer%d stride(0x%x) is not %d bytes aligned.\n",
			layer->layer_idx, layer->src.stride, DMA_STRIDE_ALIGN);
		return -EINVAL;
	}

	rdma_format = k3_pixel_format_hal2dma(layer->src.format);
	if (rdma_format < 0) {
		K3_FB_ERR("layer format(%d) not support !\n", layer->src.format);
		return -EINVAL;
	}

	rdma_transform = k3_transform_hal2dma(layer->transform, chn_idx);
	if (rdma_transform < 0) {
		K3_FB_ERR("layer transform(%d) not support!\n", layer->transform);
		return -EINVAL;
	}

	if (layer->src.is_tile) {
		l2t_interleave_n = k3_get_rdma_tile_interleave(layer->src.stride);
		if (l2t_interleave_n < MIN_INTERLEAVE) {
			K3_FB_ERR("tile stride should be 256*2^n, error stride:%d!\n", layer->src.stride);
			return -EINVAL;
		}
	}

	dfc_fmt = k3_pixel_format_hal2dfc(layer->src.format, layer->transform);
	if (dfc_fmt < 0) {
		K3_FB_ERR("layer format (%d) not support !\n", layer->src.format);
		return -EINVAL;
	}

	dfc_bpp = k3_dfc_get_bpp(dfc_fmt);
	if (dfc_bpp <= 0) {
		K3_FB_ERR("dfc_bpp(%d) not support !\n", dfc_bpp);
		return -EINVAL;
	}

	dfc_aligned = (dfc_bpp <= 2) ? 4 : 2;

	bpp = (is_yuv_semi_planar || is_yuv_planar) ? 1 : layer->src.bpp;
	aligned_pixel = DMA_ALIGN_BYTES / bpp;

	/* handle aligned_rect and clip_rect */
	{
		/* aligned_rect */
		if (is_YUV_P_420(layer->src.format) || is_YUV_P_422(layer->src.format)) {
			aligned_rect.left = ALIGN_DOWN(new_src_rect.x, 2 * aligned_pixel);
			aligned_rect.right = ALIGN_UP(new_src_rect.x + new_src_rect.w, 2 * aligned_pixel) - 1;
		} else {
			aligned_rect.left = ALIGN_DOWN(new_src_rect.x, aligned_pixel);
			aligned_rect.right = ALIGN_UP(new_src_rect.x + new_src_rect.w, aligned_pixel) - 1;
		}

		if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
			if (stretched_line_num > 0) {
				new_src_rect.h = ALIGN_DOWN(new_src_rect.h / stretched_line_num, dfc_aligned) *
					stretched_line_num;

				if (is_YUV_SP_420(layer->src.format) || is_YUV_P_420(layer->src.format)) {
					aligned_rect.top = ALIGN_DOWN(new_src_rect.y, 2);
					aligned_rect.bottom = DSS_HEIGHT(aligned_rect.top + new_src_rect.h);
				} else {
					aligned_rect.top = new_src_rect.y;
					aligned_rect.bottom = DSS_HEIGHT(new_src_rect.y + new_src_rect.h);
				}
			} else {
				aligned_rect.top = ALIGN_DOWN(new_src_rect.y, aligned_pixel);
				aligned_rect.bottom = ALIGN_UP(new_src_rect.y + new_src_rect.h, aligned_pixel) - 1;
			}
		} else {
			if (is_YUV_SP_420(layer->src.format) || is_YUV_P_420(layer->src.format)) {
				aligned_rect.top = ALIGN_DOWN(new_src_rect.y, 2);
				aligned_rect.bottom = ALIGN_UP(new_src_rect.y + new_src_rect.h, 2) - 1;
			} else {
				aligned_rect.top = new_src_rect.y;
				aligned_rect.bottom = DSS_HEIGHT(new_src_rect.y + new_src_rect.h);
			}
		}

		/* out_rect */
		out_aligned_rect->x = 0;
		out_aligned_rect->y = 0;
		out_aligned_rect->w = aligned_rect.right - aligned_rect.left + 1;
		out_aligned_rect->h = aligned_rect.bottom - aligned_rect.top + 1;
		if (stretched_line_num > 0) {
			stretch_size_vrt = (out_aligned_rect->h / stretched_line_num) +
				((out_aligned_rect->h % stretched_line_num) ? 1 : 0) - 1;

			out_aligned_rect->h = stretch_size_vrt + 1;
		} else {
			stretch_size_vrt = 0x0;
		}

		if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
			temp = out_aligned_rect->w;
			out_aligned_rect->w = out_aligned_rect->h;
			out_aligned_rect->h = temp;
		}

		/* clip_rect */
		tmp = new_src_rect.x - aligned_rect.left;
		clip_rect->left = (tmp > 0) ? tmp : 0;
		tmp = aligned_rect.right - DSS_WIDTH(new_src_rect.x + new_src_rect.w);
		clip_rect->right = (tmp > 0) ? tmp : 0;

		tmp = new_src_rect.y - aligned_rect.top;
		clip_rect->top = (tmp > 0) ? tmp : 0;
		tmp = aligned_rect.bottom - DSS_HEIGHT(new_src_rect.y + new_src_rect.h);
		clip_rect->bottom = (tmp > 0) ? tmp : 0;

		if (k3_dss_dfc_clip_check(clip_rect)) {
			K3_FB_ERR("clip rect unvalid => layer_idx=%d, chn_idx=%d, clip_rect(%d, %d, %d, %d).\n",
				layer->layer_idx, chn_idx, clip_rect->left, clip_rect->right,
				clip_rect->top, clip_rect->bottom);
			return -EINVAL;
		}

		k3_adjust_clip_rect(layer, clip_rect);

		if (g_debug_ovl_online_composer) {
			K3_FB_ERR("layer_idx%d, chn_idx=%d, format=%d, transform=%d, "
				"original_src_rect(%d,%d,%d,%d), aligned_rect(%d,%d,%d,%d), clip_rect(%d,%d,%d,%d), "
				"rdma_out_rect(%d,%d,%d,%d), dst_rect(%d,%d,%d,%d)!\n",
				layer->layer_idx, chn_idx, layer->src.format, layer->transform,
				layer->src_rect.x, layer->src_rect.y, layer->src_rect.w, layer->src_rect.h,
				aligned_rect.left, aligned_rect.top, aligned_rect.right, aligned_rect.bottom,
				clip_rect->left, clip_rect->top, clip_rect->right, clip_rect->bottom,
				out_aligned_rect->x, out_aligned_rect->y, out_aligned_rect->w, out_aligned_rect->h,
				layer->dst_rect.x, layer->dst_rect.y, layer->dst_rect.w, layer->dst_rect.h);
		}
	}

	rdma_oft_y0 = aligned_rect.top;
	rdma_oft_y1 = aligned_rect.bottom;
	rdma_oft_x0 = aligned_rect.left / aligned_pixel;
	rdma_oft_x1 = aligned_rect.right / aligned_pixel;

	if ((rdma_oft_x1 - rdma_oft_x0) < 0 ||
		(rdma_oft_x1 - rdma_oft_x0 + 1) > DMA_IN_WIDTH_MAX) {
		K3_FB_ERR("out of range, rdma_oft_x0 = %d, rdma_oft_x1 = %d!\n",
			rdma_oft_x0, rdma_oft_x1);
		return -EINVAL;
	}

	if ((rdma_oft_y1 - rdma_oft_y0) < 0 ||
		(rdma_oft_y1 - rdma_oft_y0 + 1) > DMA_IN_HEIGHT_MAX) {
		K3_FB_ERR("out of range, rdma_oft_y0 = %d, rdma_oft_y1 = %d\n",
			rdma_oft_y0, rdma_oft_y1);
		return -EINVAL;
	}

	rdma_addr = k3_calculate_display_addr(mmu_enable, layer, &aligned_rect, DSS_ADDR_PLANE0);
	rdma_stride = layer->src.stride;
	rdma_data_num = (rdma_oft_x1 - rdma_oft_x0 + 1) * (rdma_oft_y1- rdma_oft_y0 + 1);

	if (src_rect_mask_enable) {
		rdma_mask_x0 = ALIGN_UP(layer->src_rect_mask.x, aligned_pixel) / aligned_pixel;
		//rdma_mask_x1 = DSS_WIDTH(layer->src_rect_mask.x + layer->src_rect_mask.w) / aligned_pixel -1;
		rdma_mask_x1 = (ALIGN_DOWN(layer->src_rect_mask.x + layer->src_rect_mask.w, aligned_pixel) - 1) / aligned_pixel;
		rdma_mask_y0 = layer->src_rect_mask.y;
		rdma_mask_y1 = DSS_HEIGHT(layer->src_rect_mask.y + layer->src_rect_mask.h);
	}


	if (stretched_line_num > 0) {
		stretched_stride = stretched_line_num * layer->src.stride / DMA_ALIGN_BYTES;
		rdma_data_num = (rdma_oft_x1 - rdma_oft_x0 + 1) * (stretch_size_vrt + 1);
	} else {
		stretch_size_vrt = 0x0;
		stretched_line_num = 0x0;
		stretched_stride = 0x0;
	}

	dma0->rdma_data_addr = set_bits32(dma0->rdma_data_addr, rdma_addr / DMA_ALIGN_BYTES, 28, 0);
	dma0->rdma_oft_x0 = set_bits32(dma0->rdma_oft_x0, rdma_oft_x0, 12, 0);
	dma0->rdma_oft_y0 = set_bits32(dma0->rdma_oft_y0, rdma_oft_y0, 16, 0);
	dma0->rdma_stride = set_bits32(dma0->rdma_stride, rdma_stride / DMA_ALIGN_BYTES, 13, 0);
	dma0->rdma_oft_x1 = set_bits32(dma0->rdma_oft_x1, rdma_oft_x1, 12, 0);
	dma0->rdma_oft_y1 = set_bits32(dma0->rdma_oft_y1, rdma_oft_y1, 16, 0);
	dma0->rdma_mask0 = set_bits32(dma0->rdma_mask0,
		(rdma_mask_y0 | (rdma_mask_x0 << 16)), 28, 0);
	dma0->rdma_mask1 = set_bits32(dma0->rdma_mask1,
		(rdma_mask_y1 | (rdma_mask_x1 << 16)), 28, 0);
	dma0->rdma_stretch_size_vrt = set_bits32(dma0->rdma_stretch_size_vrt,
		stretch_size_vrt, 16, 0);
	dma0->rdma_stretch_line_num = set_bits32(dma0->rdma_stretch_line_num,
		(stretched_line_num | (stretched_stride << 6)), 25, 0);
	/* FIXME: rdma_page_8k */
	dma0->rdma_ctrl = set_bits32(dma0->rdma_ctrl, (layer->src.is_tile ? 0x1 : 0x0), 1, 1);
	dma0->rdma_ctrl = set_bits32(dma0->rdma_ctrl, ((layer->need_cap & CAP_FBDC) ? 0x1 : 0x0), 1, 2);
	dma0->rdma_ctrl = set_bits32(dma0->rdma_ctrl, rdma_format, 5, 3);
	dma0->rdma_ctrl = set_bits32(dma0->rdma_ctrl, (mmu_enable ? 0x1 : 0x0), 1, 8);
	dma0->rdma_ctrl = set_bits32(dma0->rdma_ctrl, rdma_transform, 3, 9);
	dma0->rdma_ctrl = set_bits32(dma0->rdma_ctrl, (*rdma_stretch_enable ? 0x1 : 0x0), 1, 12);
	dma0->rdma_ctrl = set_bits32(dma0->rdma_ctrl, l2t_interleave_n, 4, 13);
	dma0->rdma_ctrl = set_bits32(dma0->rdma_ctrl, (src_rect_mask_enable ? 0x1 : 0x0), 1, 17);
	dma0->rdma_ctrl = set_bits32(dma0->rdma_ctrl, rdma_rot_burst4, 1, 22);
	dma0->rdma_ctrl = set_bits32(dma0->rdma_ctrl, 0x1, 1, 25);
	dma0->rdma_data_num = set_bits32(dma0->rdma_data_num, rdma_data_num, 30, 0);
	dma0->rdma_tile_scram = set_bits32(dma0->rdma_tile_scram, (layer->src.is_tile ? 0x1 : 0x0), 1, 0);


	if (is_yuv_semi_planar || is_yuv_planar) {
		if (is_YUV_P_420(layer->src.format) || is_YUV_P_422(layer->src.format)) {
			rdma_oft_x0 /= 2;
			rdma_oft_x1 = (rdma_oft_x1 + 1) / 2 - 1;
		}

		if (is_YUV_SP_420(layer->src.format) || is_YUV_P_420(layer->src.format)) {
			rdma_oft_y0 /= 2;
			rdma_oft_y1 = (rdma_oft_y1 + 1) / 2 - 1;

			stretched_line_num /= 2;
		}

		rdma_addr = k3_calculate_display_addr(mmu_enable, layer, &aligned_rect, DSS_ADDR_PLANE1);
		rdma_stride = layer->src.uStride;

		if (src_rect_mask_enable) {
			if (is_YUV_P_420(layer->src.format) || is_YUV_P_422(layer->src.format)) {
				rdma_mask_x0 /= 2;
				rdma_mask_x1 = (rdma_mask_x1 + 1) / 2 - 1;
			}

			if (is_YUV_SP_420(layer->src.format) || is_YUV_P_420(layer->src.format)) {
				rdma_mask_y0 /= 2;
				rdma_mask_y1 = (rdma_mask_y1 + 1) / 2 - 1;
			}
		}

		if (*rdma_stretch_enable) {
			//stretch_size_vrt = stretch_size_vrt;
			stretched_stride = stretched_line_num * layer->src.uStride / DMA_ALIGN_BYTES;

			rdma_data_num = (rdma_oft_x1 - rdma_oft_x0 + 1) * (stretch_size_vrt + 1);
		} else {
			stretch_size_vrt = 0;
			stretched_line_num = 0;
			stretched_stride = 0;

			rdma_data_num = (rdma_oft_x1 - rdma_oft_x0 + 1) * (rdma_oft_y1- rdma_oft_y0 + 1);
		}

		if (is_YUV_SP_420(layer->src.format) || is_YUV_P_420(layer->src.format)) {
			if (!(layer->transform & K3_FB_TRANSFORM_ROT_90) && !(*rdma_stretch_enable)) {
				rdma_data_num *= 2;
			}
		}

		dma1->rdma_data_addr = set_bits32(dma1->rdma_data_addr, rdma_addr / DMA_ALIGN_BYTES, 28, 0);
		dma1->rdma_oft_x0 = set_bits32(dma1->rdma_oft_x0, rdma_oft_x0, 12, 0);
		dma1->rdma_oft_y0 = set_bits32(dma1->rdma_oft_y0, rdma_oft_y0, 16, 0);
		dma1->rdma_stride = set_bits32(dma1->rdma_stride, rdma_stride / DMA_ALIGN_BYTES, 13, 0);
		dma1->rdma_oft_x1 = set_bits32(dma1->rdma_oft_x1, rdma_oft_x1, 12, 0);
		dma1->rdma_oft_y1 = set_bits32(dma1->rdma_oft_y1, rdma_oft_y1, 16, 0);
		dma1->rdma_mask0 = set_bits32(dma1->rdma_mask0,
			(rdma_mask_y0 | (rdma_mask_x0 << 16)), 28, 0);
		dma1->rdma_mask1 = set_bits32(dma1->rdma_mask1,
			(rdma_mask_y1 | (rdma_mask_x1 << 16)), 28, 0);
		dma1->rdma_stretch_size_vrt = set_bits32(dma1->rdma_stretch_size_vrt,
			stretch_size_vrt, 16, 0);
		dma1->rdma_stretch_line_num = set_bits32(dma1->rdma_stretch_line_num,
			(stretched_line_num | (stretched_stride << 6)), 25, 0);
		/* FIXME: rdma_page_8k */
		dma1->rdma_ctrl = set_bits32(dma1->rdma_ctrl, (layer->src.is_tile ? 0x1 : 0x0), 1, 1);
		dma1->rdma_ctrl = set_bits32(dma1->rdma_ctrl, ((layer->need_cap & CAP_FBDC) ? 0x1 : 0x0), 1, 2);
		dma1->rdma_ctrl = set_bits32(dma1->rdma_ctrl, rdma_format + 1, 5, 3);
		dma1->rdma_ctrl = set_bits32(dma1->rdma_ctrl, (mmu_enable ? 0x1 : 0x0), 1, 8);
		dma1->rdma_ctrl = set_bits32(dma1->rdma_ctrl, rdma_transform, 3, 9);
		dma1->rdma_ctrl = set_bits32(dma1->rdma_ctrl, (*rdma_stretch_enable ? 0x1 : 0x0), 1, 12);
		dma1->rdma_ctrl = set_bits32(dma1->rdma_ctrl, l2t_interleave_n, 4, 13);
		dma1->rdma_ctrl = set_bits32(dma1->rdma_ctrl, (src_rect_mask_enable ? 0x1 : 0x0), 1, 17);
		dma1->rdma_ctrl = set_bits32(dma1->rdma_ctrl, rdma_rot_burst4, 1, 22);
		dma1->rdma_ctrl = set_bits32(dma1->rdma_ctrl, 0x1, 1, 25);
		dma1->rdma_data_num = set_bits32(dma1->rdma_data_num, rdma_data_num, 30, 0);
		dma1->rdma_tile_scram = set_bits32(dma1->rdma_tile_scram, (layer->src.is_tile ? 0x1 : 0x0), 1, 0);

		if (is_yuv_planar) {
			rdma_addr = k3_calculate_display_addr(mmu_enable, layer, &aligned_rect, DSS_ADDR_PLANE2);
			rdma_stride = layer->src.vStride;

			dma2->rdma_data_addr = set_bits32(dma2->rdma_data_addr, rdma_addr / DMA_ALIGN_BYTES, 28, 0);
			dma2->rdma_oft_x0 = set_bits32(dma2->rdma_oft_x0, rdma_oft_x0, 12, 0);
			dma2->rdma_oft_y0 = set_bits32(dma2->rdma_oft_y0, rdma_oft_y0, 16, 0);
			dma2->rdma_stride = set_bits32(dma2->rdma_stride, rdma_stride / DMA_ALIGN_BYTES, 13, 0);
			dma2->rdma_oft_x1 = set_bits32(dma2->rdma_oft_x1, rdma_oft_x1, 12, 0);
			dma2->rdma_oft_y1 = set_bits32(dma2->rdma_oft_y1, rdma_oft_y1, 16, 0);
			dma2->rdma_mask0 = set_bits32(dma2->rdma_mask0,
				(rdma_mask_y0 | (rdma_mask_x0 << 16)), 28, 0);
			dma2->rdma_mask1 = set_bits32(dma2->rdma_mask1,
				(rdma_mask_y1 | (rdma_mask_x1 << 16)), 28, 0);
			dma2->rdma_stretch_size_vrt = set_bits32(dma2->rdma_stretch_size_vrt,
				stretch_size_vrt, 16, 0);
			dma2->rdma_stretch_line_num = set_bits32(dma2->rdma_stretch_line_num,
				(stretched_line_num | (stretched_stride << 6)), 25, 0);
			/* FIXME: rdma_page_8k */
			dma2->rdma_ctrl = set_bits32(dma2->rdma_ctrl, (layer->src.is_tile ? 0x1 : 0x0), 1, 1);
			dma2->rdma_ctrl = set_bits32(dma2->rdma_ctrl, ((layer->need_cap & CAP_FBDC) ? 0x1 : 0x0), 1, 2);
			dma2->rdma_ctrl = set_bits32(dma2->rdma_ctrl, rdma_format + 2, 5, 3);
			dma2->rdma_ctrl = set_bits32(dma2->rdma_ctrl, (mmu_enable ? 0x1 : 0x0), 1, 8);
			dma2->rdma_ctrl = set_bits32(dma2->rdma_ctrl, rdma_transform, 3, 9);
			dma2->rdma_ctrl = set_bits32(dma2->rdma_ctrl, (*rdma_stretch_enable ? 0x1 : 0x0), 1, 12);
			dma2->rdma_ctrl = set_bits32(dma2->rdma_ctrl, l2t_interleave_n, 4, 13);
			dma2->rdma_ctrl = set_bits32(dma2->rdma_ctrl, (src_rect_mask_enable ? 0x1 : 0x0), 1, 17);
			dma2->rdma_ctrl = set_bits32(dma2->rdma_ctrl, rdma_rot_burst4, 1, 22);
			dma2->rdma_ctrl = set_bits32(dma2->rdma_ctrl, 0x1, 1, 25);
			dma2->rdma_data_num = set_bits32(dma2->rdma_data_num, rdma_data_num, 30, 0);
			dma2->rdma_tile_scram = set_bits32(dma2->rdma_tile_scram, (layer->src.is_tile ? 0x1 : 0x0), 1, 0);
		}
	}

	return 0;
}


/*******************************************************************************
** DSS FBC & FBDC
*/
static void k3_dss_fbdc_init(char __iomem *fbdc_base, dss_fbdc_t *s_fbdc)
{
	BUG_ON(fbdc_base == NULL);
	BUG_ON(s_fbdc == NULL);

	memset(s_fbdc, 0, sizeof(dss_fbdc_t));

	s_fbdc->fbdc_data_addr = inp32(fbdc_base + FBDC_DATA_ADDR);
	s_fbdc->fbdc_data_stride = inp32(fbdc_base + FBDC_DATA_STRIDE);
	s_fbdc->fbdc_head_addr = inp32(fbdc_base + FBDC_HEAD_ADDR);
	s_fbdc->fbdc_head_stride = inp32(fbdc_base + FBDC_HEAD_STRIDE);
	s_fbdc->fbdc_size = inp32(fbdc_base + FBDC_SIZE);
	s_fbdc->fbdc_crop_s = inp32(fbdc_base + FBDC_CROP_S);
	s_fbdc->fbdc_ctrl= inp32(fbdc_base + FBDC_CTRL);
	s_fbdc->fbdc_pm_ctrl = inp32(fbdc_base + FBDC_PM_CTRL);
	s_fbdc->fbdc_src_size = inp32(fbdc_base + FBDC_SRC_SIZE);
}

static void k3_dss_fbdc_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *fbdc_base, dss_fbdc_t *s_fbdc)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(fbdc_base == NULL);
	BUG_ON(s_fbdc == NULL);

	k3fd->set_reg(k3fd, fbdc_base + FBDC_DATA_ADDR, s_fbdc->fbdc_data_addr, 32, 0);
	k3fd->set_reg(k3fd, fbdc_base + FBDC_DATA_STRIDE, s_fbdc->fbdc_data_stride, 32, 0);
	k3fd->set_reg(k3fd, fbdc_base + FBDC_HEAD_ADDR, s_fbdc->fbdc_head_addr, 32, 0);
	k3fd->set_reg(k3fd, fbdc_base + FBDC_HEAD_STRIDE, s_fbdc->fbdc_head_stride, 32, 0);
	k3fd->set_reg(k3fd, fbdc_base + FBDC_SIZE, s_fbdc->fbdc_size, 32, 0);
	k3fd->set_reg(k3fd, fbdc_base + FBDC_CROP_S, s_fbdc->fbdc_crop_s, 32, 0);
	k3fd->set_reg(k3fd, fbdc_base + FBDC_CTRL, s_fbdc->fbdc_ctrl, 32, 0);
	k3fd->set_reg(k3fd, fbdc_base + FBDC_PM_CTRL, s_fbdc->fbdc_pm_ctrl, 32, 0);
	k3fd->set_reg(k3fd, fbdc_base + FBDC_SRC_SIZE, s_fbdc->fbdc_src_size, 32, 0);
}

int k3_dss_fbdc_config(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req,
	dss_layer_t *layer, dss_rect_t aligned_rect)
{
	dss_fbdc_t *fbdc = NULL;
	int chn_idx = 0;
	u32 fbdc_src_width = 0;
	u32 fbdc_src_height = 0;
	u32 rdma_addr = 0;
	bool mmu_enable = false;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);
	BUG_ON(layer == NULL);

	if (!(layer->need_cap & CAP_FBDC))
		return 0;

	chn_idx = layer->chn_idx;
	mmu_enable = (layer->src.mmu_enable == 1) ? true : false;
	rdma_addr = mmu_enable ? layer->src.vir_addr : layer->src.phy_addr;

	fbdc = &(k3fd->dss_module.fbdc[chn_idx]);
	k3fd->dss_module.fbdc_used[chn_idx] = 1;

	if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
		fbdc_src_width = layer->src.height;
		fbdc_src_height = layer->src.width;
	} else {
		fbdc_src_width = layer->src.width;
		fbdc_src_height = layer->src.height;
	}

	fbdc->fbdc_data_addr = set_bits32(fbdc->fbdc_data_addr,
		(rdma_addr / DMA_ALIGN_BYTES), 28, 0);
	fbdc->fbdc_data_stride = set_bits32(fbdc->fbdc_data_stride,
		(layer->src.stride / DMA_ALIGN_BYTES), 18, 0);
	fbdc->fbdc_head_addr = set_bits32(fbdc->fbdc_head_addr,
		(pov_req->hdr_start_addr / DMA_ALIGN_BYTES), 28, 0);
	/* FIXME: head_stride */
	fbdc->fbdc_head_stride = set_bits32(fbdc->fbdc_head_stride,
		pov_req->hdr_stride, 18, 0);
	fbdc->fbdc_size = set_bits32(fbdc->fbdc_size, (DSS_HEIGHT(aligned_rect.h) |
		(DSS_WIDTH(aligned_rect.w) << 16)), 32, 0);
	fbdc->fbdc_crop_s = set_bits32(fbdc->fbdc_crop_s,
		(aligned_rect.y | (aligned_rect.x << 16)), 32, 0);
	fbdc->fbdc_ctrl = set_bits32(fbdc->fbdc_ctrl, layer->src.format, 5, 0);
	fbdc->fbdc_ctrl = set_bits32(fbdc->fbdc_ctrl, 0x1, 1, 5);
	fbdc->fbdc_ctrl = set_bits32(fbdc->fbdc_ctrl, 0x1, 1, 7);
	fbdc->fbdc_pm_ctrl = set_bits32(fbdc->fbdc_pm_ctrl, 0x601AA, 19, 0);
	fbdc->fbdc_src_size = set_bits32(fbdc->fbdc_src_size, ((DSS_HEIGHT(fbdc_src_height) |
		(DSS_WIDTH(fbdc_src_width) << 16))), 32, 0);

	return 0;
}

static void k3_dss_fbc_init(char __iomem *fbc_base, dss_fbc_t *s_fbc)
{
	BUG_ON(fbc_base == NULL);
	BUG_ON(s_fbc == NULL);

	memset(s_fbc, 0, sizeof(dss_fbc_t));

	s_fbc->fbc_size = inp32(fbc_base + FBC_SIZE);
	s_fbc->fbc_ctrl = inp32(fbc_base + FBC_CTRL);
	s_fbc->fbc_en = inp32(fbc_base + FBC_EN);
	s_fbc->fbc_pm_ctrl = inp32(fbc_base + FBC_PM_CTRL);
	s_fbc->fbc_len = inp32(fbc_base + FBC_LEN);
}

void k3_dss_fbc_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *fbc_base, dss_fbc_t *s_fbc)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(fbc_base == NULL);
	BUG_ON(s_fbc == NULL);

	k3fd->set_reg(k3fd, fbc_base + FBC_SIZE, s_fbc->fbc_size, 32, 0);
	k3fd->set_reg(k3fd, fbc_base + FBC_CTRL, s_fbc->fbc_ctrl, 32, 0);
	k3fd->set_reg(k3fd, fbc_base + FBC_EN, s_fbc->fbc_en, 32, 0);
	k3fd->set_reg(k3fd, fbc_base + FBC_PM_CTRL, s_fbc->fbc_pm_ctrl, 32, 0);
	k3fd->set_reg(k3fd, fbc_base + FBC_LEN, s_fbc->fbc_len, 32, 0);
}

int k3_dss_fbc_config(struct k3_fb_data_type *k3fd, dss_wb_layer_t *layer,
	dss_rect_t aligned_rect)
{
	dss_fbc_t *fbc = NULL;
	int chn_idx = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(layer == NULL);

	if (!(layer->need_cap & CAP_FBC))
		return 0;

	chn_idx = layer->chn_idx;

	fbc = &(k3fd->dss_module.fbc[chn_idx]);
	k3fd->dss_module.fbc_used[chn_idx] = 1;

	fbc->fbc_size = set_bits32(fbc->fbc_size,
		(DSS_HEIGHT(aligned_rect.h) | (DSS_WIDTH(aligned_rect.w) << 16)), 32, 0);
	fbc->fbc_ctrl = set_bits32(fbc->fbc_ctrl, (layer->dst.format | (FBC_ARITH_SEL0 << 5) |
		(FBC_BLOCK_128_PIXELS << 7)), 23, 0);
	fbc->fbc_en = set_bits32(fbc->fbc_en, (0x1 | (0x1 << 1)), 2, 0);
	fbc->fbc_pm_ctrl = set_bits32(fbc->fbc_pm_ctrl, 0x81AA, 17, 0);
	fbc->fbc_len = set_bits32(fbc->fbc_len, (0x10 | (0x10 << 5)), 10, 0);

	return 0;
}


/*******************************************************************************
** DSS DFC
*/
static void k3_dss_dfc_init(char __iomem *dfc_base, dss_dfc_t *s_dfc)
{
	BUG_ON(dfc_base == NULL);
	BUG_ON(s_dfc == NULL);

	memset(s_dfc, 0, sizeof(dss_dfc_t));

	s_dfc->disp_size = inp32(dfc_base + DFC_DISP_SIZE);
	s_dfc->pix_in_num = inp32(dfc_base + DFC_PIX_IN_NUM);
	s_dfc->disp_fmt = inp32(dfc_base + DFC_DISP_FMT);
	s_dfc->clip_ctl_hrz = inp32(dfc_base + DFC_CLIP_CTL_HRZ);
	s_dfc->clip_ctl_vrz = inp32(dfc_base + DFC_CLIP_CTL_VRZ);
	s_dfc->ctl_clip_en = inp32(dfc_base + DFC_CTL_CLIP_EN);
	s_dfc->icg_module = inp32(dfc_base + DFC_ICG_MODULE);
	s_dfc->dither_enable = inp32(dfc_base + DFC_DITHER_ENABLE);
	s_dfc->padding_ctl = inp32(dfc_base + DFC_PADDING_CTL);
}

static void k3_dss_dfc_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *dfc_base, dss_dfc_t *s_dfc)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(dfc_base == NULL);
	BUG_ON(s_dfc == NULL);

	k3fd->set_reg(k3fd, dfc_base + DFC_DISP_SIZE, s_dfc->disp_size, 32, 0);
	k3fd->set_reg(k3fd, dfc_base + DFC_PIX_IN_NUM, s_dfc->pix_in_num, 32, 0);
	k3fd->set_reg(k3fd, dfc_base + DFC_DISP_FMT, s_dfc->disp_fmt, 32, 0);
	k3fd->set_reg(k3fd, dfc_base + DFC_CLIP_CTL_HRZ, s_dfc->clip_ctl_hrz, 32, 0);
	k3fd->set_reg(k3fd, dfc_base + DFC_CLIP_CTL_VRZ, s_dfc->clip_ctl_vrz, 32, 0);
	k3fd->set_reg(k3fd, dfc_base + DFC_CTL_CLIP_EN, s_dfc->ctl_clip_en, 32, 0);
	k3fd->set_reg(k3fd, dfc_base + DFC_ICG_MODULE, s_dfc->icg_module, 32, 0);
	k3fd->set_reg(k3fd, dfc_base + DFC_DITHER_ENABLE, s_dfc->dither_enable, 32, 0);
	k3fd->set_reg(k3fd, dfc_base + DFC_PADDING_CTL, s_dfc->padding_ctl, 32, 0);
}

int k3_dss_rdfc_config(struct k3_fb_data_type *k3fd, dss_layer_t *layer,
	dss_rect_t *aligned_rect, dss_rect_ltrb_t clip_rect)
{
	dss_dfc_t *dfc = NULL;
	int chn_idx = 0;
	int dfc_fmt = 0;
	int dfc_bpp = 0;
	int dfc_pix_in_num = 0;
	int dfc_aligned = 0;
	int size_hrz = 0;
	int size_vrt = 0;
	int dfc_hrz_clip = 0;
	bool need_clip = false;

	BUG_ON(k3fd == NULL);
	BUG_ON(layer == NULL);

	chn_idx = layer->chn_idx;

	dfc = &(k3fd->dss_module.dfc[chn_idx]);
	k3fd->dss_module.dfc_used[chn_idx] = 1;

	dfc_fmt = k3_pixel_format_hal2dfc(layer->src.format, layer->transform);
	if (dfc_fmt < 0) {
		K3_FB_ERR("layer format (%d) not support !\n", layer->src.format);
		return -EINVAL;
	}

	dfc_bpp = k3_dfc_get_bpp(dfc_fmt);
	if (dfc_bpp <= 0) {
		K3_FB_ERR("dfc_bpp(%d) not support !\n", dfc_bpp);
		return -EINVAL;
	}

	dfc_pix_in_num = (dfc_bpp <= 2) ? 0x1 : 0x0;
	dfc_aligned = (dfc_bpp <= 2) ? 4 : 2;

	need_clip = isNeedRectClip(clip_rect);

	size_hrz = DSS_WIDTH(aligned_rect->w);
	size_vrt = DSS_HEIGHT(aligned_rect->h);

	if (((size_hrz + 1) % dfc_aligned) != 0) {
		size_hrz -= 1;
		K3_FB_ERR("SIZE_HRT=%d mismatch!bpp=%d\n", size_hrz, layer->src.bpp);

		K3_FB_ERR("layer_idx%d, format=%d, transform=%d, "
			"original_src_rect(%d,%d,%d,%d), rdma_out_rect(%d,%d,%d,%d), dst_rect(%d,%d,%d,%d)!\n",
			layer->layer_idx, layer->src.format, layer->transform,
			layer->src_rect.x, layer->src_rect.y, layer->src_rect.w, layer->src_rect.h,
			aligned_rect->x, aligned_rect->y, aligned_rect->w, aligned_rect->h,
			layer->dst_rect.x, layer->dst_rect.y, layer->dst_rect.w, layer->dst_rect.h);
	}

	dfc_hrz_clip = (size_hrz + 1) % dfc_aligned;
	if (dfc_hrz_clip) {
		clip_rect.right += dfc_hrz_clip;
		size_hrz += dfc_hrz_clip;
		need_clip = true;
	}

	dfc->disp_size = set_bits32(dfc->disp_size, (size_vrt | (size_hrz << 13)), 26, 0);
	dfc->pix_in_num = set_bits32(dfc->pix_in_num, dfc_pix_in_num, 1, 0);
	dfc->disp_fmt = set_bits32(dfc->disp_fmt, dfc_fmt, 5, 1);
	if (need_clip) {
		dfc->clip_ctl_hrz = set_bits32(dfc->clip_ctl_hrz,
			(clip_rect.right | (clip_rect.left << 6)), 12, 0);
		dfc->clip_ctl_vrz = set_bits32(dfc->clip_ctl_vrz,
			(clip_rect.bottom | (clip_rect.top << 6)), 12, 0);
		dfc->ctl_clip_en = set_bits32(dfc->ctl_clip_en, 0x1, 1, 0);
	} else {
		dfc->clip_ctl_hrz = set_bits32(dfc->clip_ctl_hrz, 0x0, 12, 0);
		dfc->clip_ctl_vrz = set_bits32(dfc->clip_ctl_vrz, 0x0, 12, 0);
		dfc->ctl_clip_en = set_bits32(dfc->ctl_clip_en, 0x0, 1, 0);
	}
	dfc->icg_module = set_bits32(dfc->icg_module, 0x1, 1, 2);
	dfc->dither_enable = set_bits32(dfc->dither_enable, 0x0, 1, 0);
	dfc->padding_ctl = set_bits32(dfc->padding_ctl, 0x0, 17, 0);

	//update
	if (need_clip) {
		aligned_rect->w -= (clip_rect.left + clip_rect.right);
		aligned_rect->h -= (clip_rect.top + clip_rect.bottom);
	}

	return 0;
}

int k3_dss_wdfc_config(struct k3_fb_data_type *k3fd, dss_wb_layer_t *layer,
	dss_rect_t *aligned_rect, dss_rect_t *block_rect)
{
	dss_dfc_t *dfc = NULL;
	int chn_idx = 0;
	dss_rect_t in_rect;
	bool need_dither = false;

	int size_hrz = 0;
	int size_vrt = 0;
	int dfc_fmt = 0;
	int dfc_pix_in_num = 0;
	u32 dfc_w = 0;

	u32 left_pad = 0;
	u32 right_pad = 0;
	u32 top_pad = 0;
	u32 bottom_pad = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(layer == NULL);
	BUG_ON(aligned_rect == NULL);

	chn_idx = layer->chn_idx;

	dfc = &(k3fd->dss_module.dfc[chn_idx]);
	k3fd->dss_module.dfc_used[chn_idx] = 1;

	dfc_fmt = k3_pixel_format_hal2dfc(layer->dst.format, 0);
	if (dfc_fmt < 0) {
		K3_FB_ERR("layer format (%d) not support !\n", layer->dst.format);
		return -EINVAL;
	}

	need_dither = isNeedDither(dfc_fmt);
	if (block_rect) {
		memcpy(&in_rect, block_rect, sizeof(dss_rect_t));
	} else {
		in_rect = layer->dst_rect;
	}

	size_hrz = DSS_WIDTH(in_rect.w);
	size_vrt = DSS_HEIGHT(in_rect.h);

	/* for wdfc */
	if ((size_hrz + 1) % 2 == 1) {
		size_hrz += 1;
		dfc_w = 1;
	}
	dfc_pix_in_num = 0x0;

	aligned_rect->x = ALIGN_DOWN(in_rect.x * layer->dst.bpp,
		DMA_ALIGN_BYTES) / layer->dst.bpp;
	aligned_rect->y = in_rect.y;
	aligned_rect->w = ALIGN_UP((in_rect.w + in_rect.x - aligned_rect->x + dfc_w) *
		layer->dst.bpp , DMA_ALIGN_BYTES) / layer->dst.bpp;
	aligned_rect->h = in_rect.h;

	left_pad = in_rect.x - aligned_rect->x;
	right_pad = aligned_rect->w - in_rect.w - left_pad - dfc_w;
	top_pad = 0;
	bottom_pad = 0;

	dfc->disp_size = set_bits32(dfc->disp_size, (size_vrt | (size_hrz << 13)), 26, 0);
	dfc->pix_in_num = set_bits32(dfc->pix_in_num, dfc_pix_in_num, 1, 0);
	dfc->disp_fmt = set_bits32(dfc->disp_fmt, dfc_fmt, 5, 1);
	dfc->clip_ctl_hrz = set_bits32(dfc->clip_ctl_hrz, 0x0, 12, 0);
	dfc->clip_ctl_vrz = set_bits32(dfc->clip_ctl_vrz, 0x0, 12, 0);
	dfc->ctl_clip_en = set_bits32(dfc->ctl_clip_en, 0x0, 1, 0);
	dfc->icg_module = set_bits32(dfc->icg_module, 0x1, 1, 2);
	if (need_dither) {
		dfc->dither_enable = set_bits32(dfc->dither_enable, 0x1, 1, 0);
	} else {
		dfc->dither_enable = set_bits32(dfc->dither_enable, 0x0, 1, 0);
	}

	if (left_pad || right_pad || top_pad || bottom_pad) {
		dfc->padding_ctl = set_bits32(dfc->padding_ctl, (0x1 | (left_pad << 1) |
			(right_pad << 5) | (top_pad << 9) | (bottom_pad << 13)), 17, 0);
	} else {
		dfc->padding_ctl = set_bits32(dfc->padding_ctl, 0x0, 17, 0);
	}

	return 0;
}


/*******************************************************************************
** DSS SCF
*/

/* Filter coefficients for SCF */
#define PHASE_NUM	(66)
#define TAP4	(4)
#define TAP5	(5)
#define TAP6	(6)

static const int H0_Y_COEF[PHASE_NUM][TAP6] = {
	/* Row 0~32: coefficients for scale down */
	{   2,  264,  500, 264,   2,  -8},
	{   2,  257,  499, 268,   6,  -8},
	{   1,  252,  498, 274,   8,  -9},
	{  -1,  246,  498, 281,   9,  -9},
	{  -2,  241,  497, 286,  12, -10},
	{  -3,  235,  497, 292,  13, -10},
	{  -5,  230,  496, 298,  15, -10},
	{  -6,  225,  495, 303,  18, -11},
	{  -7,  219,  494, 309,  20, -11},
	{  -7,  213,  493, 314,  23, -12},
	{  -9,  208,  491, 320,  26, -12},
	{ -10,  203,  490, 325,  28, -12},
	{ -10,  197,  488, 331,  31, -13},
	{ -10,  192,  486, 336,  33, -13},
	{ -12,  186,  485, 342,  36, -13},
	{ -12,  181,  482, 347,  39, -13},
	{ -13,  176,  480, 352,  42, -13},
	{ -14,  171,  478, 358,  45, -14},
	{ -14,  166,  476, 363,  48, -15},
	{ -14,  160,  473, 368,  52, -15},
	{ -14,  155,  470, 373,  55, -15},
	{ -15,  150,  467, 378,  59, -15},
	{ -15,  145,  464, 383,  62, -15},
	{ -16,  141,  461, 388,  65, -15},
	{ -16,  136,  458, 393,  68, -15},
	{ -16,  131,  455, 398,  72, -16},
	{ -16,  126,  451, 402,  77, -16},
	{ -16,  122,  448, 407,  79, -16},
	{ -16,  117,  444, 411,  84, -16},
	{ -17,  113,  441, 416,  87, -16},
	{ -17,  108,  437, 420,  92, -16},
	{ -17,  104,  433, 424,  96, -16},
	{ -17,  100,  429, 429, 100, -17},

	/* Row 33~65: coefficients for scale up */
	{-187,  105, 1186, 105, -187,   2},
	{-182,   86, 1186, 124, -192,   2},
	{-176,   67, 1185, 143, -197,   2},
	{-170,   49, 1182, 163, -202,   2},
	{-166,   32, 1180, 184, -207,   1},
	{-160,   15, 1176, 204, -212,   1},
	{-155,   -2, 1171, 225, -216,   1},
	{-149,  -18, 1166, 246, -221,   0},
	{-145,  -34, 1160, 268, -225,   0},
	{-139,  -49, 1153, 290, -230,  -1},
	{-134,  -63, 1145, 312, -234,  -2},
	{-129,  -78, 1137, 334, -238,  -2},
	{-124,  -91, 1128, 357, -241,  -5},
	{-119, -104, 1118, 379, -245,  -5},
	{-114, -117, 1107, 402, -248,  -6},
	{-109, -129, 1096, 425, -251,  -8},
	{-104, -141, 1083, 448, -254,  -8},
	{-100, -152, 1071, 471, -257,  -9},
	{ -95, -162, 1057, 494, -259, -11},
	{ -90, -172, 1043, 517, -261, -13},
	{ -86, -181, 1028, 540, -263, -14},
	{ -82, -190, 1013, 563, -264, -16},
	{ -77, -199,  997, 586, -265, -18},
	{ -73, -207,  980, 609, -266, -19},
	{ -69, -214,  963, 632, -266, -22},
	{ -65, -221,  945, 655, -266, -24},
	{ -62, -227,  927, 678, -266, -26},
	{ -58, -233,  908, 700, -265, -28},
	{ -54, -238,  889, 722, -264, -31},
	{ -51, -243,  870, 744, -262, -34},
	{ -48, -247,  850, 766, -260, -37},
	{ -45, -251,  829, 787, -257, -39},
	{ -42, -255,  809, 809, -255, -42}
};

static const int Y_COEF[PHASE_NUM][TAP5] = {
	/* Row 0~32: coefficients for scale down */
	{  98, 415, 415,  98, -2},
	{  95, 412, 418, 103, -4},
	{  91, 408, 422, 107, -4},
	{  87, 404, 426, 111, -4},
	{  84, 399, 430, 115, -4},
	{  80, 395, 434, 119, -4},
	{  76, 390, 438, 124, -4},
	{  73, 386, 440, 128, -3},
	{  70, 381, 444, 132, -3},
	{  66, 376, 448, 137, -3},
	{  63, 371, 451, 142, -3},
	{  60, 366, 455, 146, -3},
	{  57, 361, 457, 151, -2},
	{  54, 356, 460, 156, -2},
	{  51, 351, 463, 161, -2},
	{  49, 346, 465, 165, -1},
	{  46, 341, 468, 170, -1},
	{  43, 336, 470, 175, 0},
	{  41, 331, 472, 180, 0},
	{  38, 325, 474, 186, 1},
	{  36, 320, 476, 191, 1},
	{  34, 315, 477, 196, 2},
	{  32, 309, 479, 201, 3},
	{  29, 304, 481, 206, 4},
	{  27, 299, 481, 212, 5},
	{  26, 293, 482, 217, 6},
	{  24, 288, 484, 222, 6},
	{  22, 282, 484, 228, 8},
	{  20, 277, 485, 233, 9},
	{  19, 271, 485, 238, 11},
	{  17, 266, 485, 244, 12},
	{  16, 260, 485, 250, 13},
	{  14, 255, 486, 255, 14},
	/* Row 33~65: coefficients for scale up */
	{ -94, 608, 608, -94, -4},
	{ -94, 594, 619, -91, -4},
	{ -96, 579, 635, -89, -5},
	{ -96, 563, 650, -87, -6},
	{ -97, 548, 665, -85, -7},
	{ -97, 532, 678, -82, -7},
	{ -98, 516, 693, -79, -8},
	{ -97, 500, 705, -75, -9},
	{ -97, 484, 720, -72, -11},
	{ -97, 468, 733, -68, -12},
	{ -96, 452, 744, -63, -13},
	{ -95, 436, 755, -58, -14},
	{ -94, 419, 768, -53, -16},
	{ -93, 403, 779, -48, -17},
	{ -92, 387, 789, -42, -18},
	{ -90, 371, 799, -36, -20},
	{ -89, 355, 809, -29, -22},
	{ -87, 339, 817, -22, -23},
	{ -86, 324, 826, -15, -25},
	{ -84, 308, 835,  -8, -27},
	{ -82, 293, 842,   0, -29},
	{ -80, 277, 849,   9, -31},
	{ -78, 262, 855,  18, -33},
	{ -75, 247, 860,  27, -35},
	{ -73, 233, 865,  36, -37},
	{ -71, 218, 870,  46, -39},
	{ -69, 204, 874,  56, -41},
	{ -66, 190, 876,  67, -43},
	{ -64, 176, 879,  78, -45},
	{ -62, 163, 882,  89, -48},
	{ -59, 150, 883, 100, -50},
	{ -57, 137, 883, 112, -51},
	{ -55, 125, 884, 125, -55}
};

static const int UV_COEF[PHASE_NUM][TAP4] = {
	{214, 599, 214, -3},
	{207, 597, 223, -3},
	{200, 596, 231, -3},
	{193, 596, 238, -3},
	{186, 595, 246, -3},
	{178, 594, 255, -3},
	{171, 593, 263, -3},
	{165, 591, 271, -3},
	{158, 589, 279, -2},
	{151, 587, 288, -2},
	{145, 584, 296, -1},
	{139, 582, 304, -1},
	{133, 578, 312, 1},
	{127, 575, 321, 1},
	{121, 572, 329, 2},
	{115, 568, 337, 4},
	{109, 564, 346, 5},
	{104, 560, 354, 6},
	{ 98, 555, 362, 9},
	{ 94, 550, 370, 10},
	{ 88, 546, 379, 11},
	{ 84, 540, 387, 13},
	{ 79, 535, 395, 15},
	{ 74, 530, 403, 17},
	{ 70, 524, 411, 19},
	{ 66, 518, 419, 21},
	{ 62, 512, 427, 23},
	{ 57, 506, 435, 26},
	{ 54, 499, 443, 28},
	{ 50, 492, 451, 31},
	{ 47, 486, 457, 34},
	{ 43, 479, 465, 37},
	{ 40, 472, 472, 40},
	{214, 599, 214, -3},
	{207, 597, 223, -3},
	{200, 596, 231, -3},
	{193, 596, 238, -3},
	{186, 595, 246, -3},
	{178, 594, 255, -3},
	{171, 593, 263, -3},
	{165, 591, 271, -3},
	{158, 589, 279, -2},
	{151, 587, 288, -2},
	{145, 584, 296, -1},
	{139, 582, 304, -1},
	{133, 578, 312, 1},
	{127, 575, 321, 1},
	{121, 572, 329, 2},
	{115, 568, 337, 4},
	{109, 564, 346, 5},
	{104, 560, 354, 6},
	{ 98, 555, 362, 9},
	{ 94, 550, 370, 10},
	{ 88, 546, 379, 11},
	{ 84, 540, 387, 13},
	{ 79, 535, 395, 15},
	{ 74, 530, 403, 17},
	{ 70, 524, 411, 19},
	{ 66, 518, 419, 21},
	{ 62, 512, 427, 23},
	{ 57, 506, 435, 26},
	{ 54, 499, 443, 28},
	{ 50, 492, 451, 31},
	{ 47, 486, 457, 34},
	{ 43, 479, 465, 37},
	{ 40, 472, 472, 40}
};

static void k3_dss_scf_init(char __iomem *scf_base, dss_scf_t *s_scf)
{
	BUG_ON(scf_base == NULL);
	BUG_ON(s_scf == NULL);

	memset(s_scf, 0, sizeof(dss_scf_t));

	s_scf->en_hscl_str = inp32(scf_base + SCF_EN_HSCL_STR);
	s_scf->en_vscl_str = inp32(scf_base + SCF_EN_VSCL_STR);
	s_scf->h_v_order = inp32(scf_base + SCF_H_V_ORDER);
	s_scf->input_width_height = inp32(scf_base + SCF_INPUT_WIDTH_HEIGHT);
	s_scf->output_width_heigth = inp32(scf_base + SCF_OUTPUT_WIDTH_HEIGTH);
	s_scf->en_hscl = inp32(scf_base + SCF_EN_HSCL);
	s_scf->en_vscl = inp32(scf_base + SCF_EN_VSCL);
	s_scf->acc_hscl = inp32(scf_base + SCF_ACC_HSCL);
	s_scf->inc_hscl = inp32(scf_base + SCF_INC_HSCL);
	s_scf->inc_vscl = inp32(scf_base + SCF_INC_VSCL);
}

static void k3_dss_scf_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *scf_base, dss_scf_t *s_scf)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(scf_base == NULL);
	BUG_ON(s_scf == NULL);

	k3fd->set_reg(k3fd, scf_base + SCF_EN_HSCL_STR, s_scf->en_hscl_str, 32, 0);
	k3fd->set_reg(k3fd, scf_base + SCF_EN_VSCL_STR, s_scf->en_vscl_str, 32, 0);
	k3fd->set_reg(k3fd, scf_base + SCF_H_V_ORDER, s_scf->h_v_order, 32, 0);
	k3fd->set_reg(k3fd, scf_base + SCF_INPUT_WIDTH_HEIGHT, s_scf->input_width_height, 32, 0);
	k3fd->set_reg(k3fd, scf_base + SCF_OUTPUT_WIDTH_HEIGTH, s_scf->output_width_heigth, 32, 0);
	k3fd->set_reg(k3fd, scf_base + SCF_EN_HSCL, s_scf->en_hscl, 32, 0);
	k3fd->set_reg(k3fd, scf_base + SCF_EN_VSCL, s_scf->en_vscl, 32, 0);
	k3fd->set_reg(k3fd, scf_base + SCF_ACC_HSCL, s_scf->acc_hscl, 32, 0);
	k3fd->set_reg(k3fd, scf_base + SCF_INC_HSCL, s_scf->inc_hscl, 32, 0);
	k3fd->set_reg(k3fd, scf_base + SCF_INC_VSCL, s_scf->inc_vscl, 32, 0);
}

int k3_dss_scf_write_coefs(struct k3_fb_data_type *k3fd,
	char __iomem *addr, const int **p, int row, int col)
{
	int groups[3] = {0};
	int offset = 0;
	int valid_num = 0;
	int i= 0;
	int j = 0;
	int k = 0;

	BUG_ON(addr == NULL);

	if ((row != PHASE_NUM) || (col < TAP4 || col > TAP6)) {
		K3_FB_ERR("SCF filter coefficients is err, phase_num = %d, tap_num = %d\n", row, col);
		return -EINVAL;
	}

	/*byte*/
	offset = (col == TAP4) ? 8 : 16;
	valid_num = (offset == 16) ? 3 : 2;

	for (i = 0; i < row; i++) {
		for (j = 0; j < col; j += 2) {
			if ((col % 2) && (j == col -1)) {
				groups[j / 2] = (*((int*)p + i * col + j) & 0xFFF) | (0 << 16);
			} else {
				groups[j / 2] = (*((int*)p + i * col + j) & 0xFFF) | (*((int*)p + i * col + j + 1) << 16);
			}
		}

		for (k = 0; k < valid_num; k++) {
			k3fd->set_reg(k3fd, addr + offset * i + k * sizeof(int), groups[k], 32, 0);
			groups[k] = 0;
		}
	}

	return 0;
}

int k3_dss_scf_load_filter_coef(struct k3_fb_data_type *k3fd, char __iomem *scf_base)
{
	char __iomem *h0_y_addr = NULL;
	char __iomem *y_addr = NULL;
	char __iomem *uv_addr = NULL;
	int ret = 0;

	BUG_ON(scf_base == NULL);

	h0_y_addr = scf_base + DSS_SCF_H0_Y_COEF_OFFSET;
	y_addr = scf_base + DSS_SCF_Y_COEF_OFFSET;
	uv_addr = scf_base + DSS_SCF_UV_COEF_OFFSET;

	ret = k3_dss_scf_write_coefs(k3fd, h0_y_addr, (const int **)H0_Y_COEF, PHASE_NUM, TAP6);
	if (ret < 0) {
		K3_FB_ERR("Error to write H0_Y_COEF coefficients.\n");
	}

	ret = k3_dss_scf_write_coefs(k3fd, y_addr, (const int **)Y_COEF, PHASE_NUM, TAP5);
	if (ret < 0) {
		K3_FB_ERR("Error to write Y_COEF coefficients.\n");
	}

	ret = k3_dss_scf_write_coefs(k3fd, uv_addr, (const int **)UV_COEF, PHASE_NUM, TAP4);
	if (ret < 0) {
		K3_FB_ERR("Error to write UV_COEF coefficients.\n");
	}

	return ret;
}

int k3_dss_scf_coef_load(struct k3_fb_data_type *k3fd)
{
	char __iomem *temp_base = NULL;
	int prev_refcount = 0;

	BUG_ON(k3fd == NULL);

	if (k3fd->ovl_type == DSS_OVL_ADP) {
		/*SCF4*/
		temp_base = k3fd->dss_base +
			g_dss_module_base[DPE3_CHN0][MODULE_SCF];
		k3_dss_scf_load_filter_coef(k3fd, temp_base);
		/*SCF3*/
		temp_base = k3fd->dss_base +
			g_dss_module_base[DPE3_CHN1][MODULE_SCF];
		k3_dss_scf_load_filter_coef(k3fd, temp_base);
	}

	prev_refcount = g_scf_coef_load_refcount++;
	if (!prev_refcount) {
		/*SCF0*/
		temp_base = k3fd->dss_base +
			g_dss_module_base[DPE0_CHN0][MODULE_SCF];
		k3_dss_scf_load_filter_coef(k3fd, temp_base);
		/*SCF1*/
		temp_base = k3fd->dss_base +
			g_dss_module_base[DPE1_CHN0][MODULE_SCF];
		k3_dss_scf_load_filter_coef(k3fd, temp_base);
		/*SCF2*/
		temp_base = k3fd->dss_base +
			g_dss_module_base[DPE2_CHN0][MODULE_SCF];
		k3_dss_scf_load_filter_coef(k3fd, temp_base);
	}

	return 0;
}

int k3_dss_scf_coef_unload(struct k3_fb_data_type *k3fd)
{
	int new_refcount = 0;

	BUG_ON(k3fd == NULL);

	new_refcount = --g_scf_coef_load_refcount;
	WARN_ON(new_refcount < 0);

	return 0;
}

int k3_dss_scf_config(struct k3_fb_data_type *k3fd, dss_layer_t *layer,
	dss_wb_layer_t *wb_layer, dss_rect_t *aligned_rect, bool rdma_stretch_enable)
{
	dss_scf_t *scf = NULL;
	int scf_idx = 0;
	dss_rect_t src_rect;
	dss_rect_t dst_rect;
	u32 need_cap = 0;
	int chn_idx = 0;
	u32 transform = 0;
	dss_block_info_t *pblock_info = NULL;

	bool en_hscl = false;
	bool en_vscl = false;
	u32 h_ratio = 0;
	u32 v_ratio = 0;
	u32 h_v_order = 0;
	u32 acc_hscl = 0;
	u32 acc_vscl = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON((layer == NULL) && (wb_layer == NULL));
	BUG_ON(layer && wb_layer);

	if (wb_layer) {
		need_cap = wb_layer->need_cap;
		chn_idx = wb_layer->chn_idx;
		transform = wb_layer->transform;
		if (aligned_rect)
			src_rect = *aligned_rect;
		else
			src_rect = wb_layer->src_rect;
		dst_rect = wb_layer->dst_rect;
	} else {
		need_cap = layer->need_cap;
		chn_idx = layer->chn_idx;
		transform = layer->transform;
		if (aligned_rect)
			src_rect = *aligned_rect;
		else
			src_rect = layer->src_rect;
		dst_rect = layer->dst_rect;
		pblock_info = &(layer->block_info);
	}

	if (!(need_cap & CAP_SCL))
		return 0;

	do {
		if (src_rect.w == dst_rect.w)
			break;

		en_hscl = true;

		if (pblock_info &&
			(pblock_info->h_ratio != 0) &&
			(pblock_info->h_ratio != SCF_CONST_FACTOR)) {
			h_ratio = pblock_info->h_ratio;
			break;
		}

		h_ratio = DSS_WIDTH(src_rect.w) * SCF_CONST_FACTOR / DSS_WIDTH(dst_rect.w);
		if ((dst_rect.w > (src_rect.w * SCF_UPSCALE_MAX))
			|| (src_rect.w > (dst_rect.w * SCF_DOWNSCALE_MAX))) {
			K3_FB_ERR("width out of range, original_src_rec(%d, %d, %d, %d) "
				"new_src_rect(%d, %d, %d, %d), dst_rect(%d, %d, %d, %d), rdma_stretch_enable=%d\n",
				layer->src_rect.x, layer->src_rect.y, layer->src_rect.w, layer->src_rect.h,
				src_rect.x, src_rect.y, src_rect.w, src_rect.h,
				dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h, rdma_stretch_enable);

			return -EINVAL;
		}
	} while(0);

	do {
		if (src_rect.h == dst_rect.h)
			break;

		en_vscl = true;
		if (pblock_info &&
			(pblock_info->v_ratio != 0) &&
			(pblock_info->v_ratio != SCF_CONST_FACTOR)) {
			v_ratio = pblock_info->v_ratio;
			break;
		}

		v_ratio = (DSS_HEIGHT(src_rect.h) * SCF_CONST_FACTOR + SCF_CONST_FACTOR / 2 - acc_vscl) /
			DSS_HEIGHT(dst_rect.h);
		if ((dst_rect.h > (src_rect.h * SCF_UPSCALE_MAX))
			|| (src_rect.h > (dst_rect.h * SCF_DOWNSCALE_MAX))) {
			K3_FB_ERR("height out of range, original_src_rec(%d, %d, %d, %d) "
				"new_src_rect(%d, %d, %d, %d), dst_rect(%d, %d, %d, %d), rdma_stretch_enable=%d.\n",
				layer->src_rect.x, layer->src_rect.y, layer->src_rect.w, layer->src_rect.h,
				src_rect.x, src_rect.y, src_rect.w, src_rect.h,
				dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h, rdma_stretch_enable);
			return -EINVAL;
		}
	} while(0);

	if (!en_hscl && !en_vscl)
		return 0;

	/* scale down, do hscl first; scale up, do vscl first*/
	if (pblock_info && ((pblock_info->h_ratio != 0) || (pblock_info->v_ratio != 0))) {
		h_v_order = pblock_info->h_v_order;
	} else {
		h_v_order = (src_rect.w > dst_rect.w) ? 0 : 1;
	}

	if (pblock_info && (pblock_info->acc_hscl != 0)) {
		acc_hscl = pblock_info->acc_hscl;
	}

	scf_idx = k3_get_scf_index(chn_idx);
	if (scf_idx < 0) {
		K3_FB_ERR("chn_idx(%d) k3_get_scf_index failed!\n", chn_idx);
		return -EINVAL;
	}

	scf = &(k3fd->dss_module.scf[scf_idx]);
	k3fd->dss_module.scf_used[scf_idx] = 1;

	//if (DSS_WIDTH(src_rect.w) * 2 >= DSS_WIDTH(dst_rect.w)) {
	if (h_ratio >= 2 * SCF_CONST_FACTOR) {
		scf->en_hscl_str = set_bits32(scf->en_hscl_str, 0x1, 1, 0);
	} else {
		scf->en_hscl_str = set_bits32(scf->en_hscl_str, 0x0, 1, 0);
	}

	//if (DSS_HEIGHT(src_rect.h) * 2 >= DSS_HEIGHT(dst_rect.h)) {
	if (v_ratio >= 2 * SCF_CONST_FACTOR) {
		scf->en_vscl_str = set_bits32(scf->en_vscl_str, 0x1, 1, 0);
	} else {
		scf->en_vscl_str = set_bits32(scf->en_vscl_str, 0x0, 1, 0);
	}

	scf->h_v_order = set_bits32(scf->h_v_order, h_v_order, 1, 0);
	scf->input_width_height = set_bits32(scf->input_width_height,
		DSS_HEIGHT(src_rect.h), 13, 0);
	scf->input_width_height = set_bits32(scf->input_width_height,
		DSS_WIDTH(src_rect.w), 13, 16);
	scf->output_width_heigth = set_bits32(scf->output_width_heigth,
		DSS_HEIGHT(dst_rect.h), 13, 0);
	scf->output_width_heigth = set_bits32(scf->output_width_heigth,
		DSS_WIDTH(dst_rect.w), 13, 16);
	scf->en_hscl = set_bits32(scf->en_hscl, (en_hscl ? 0x1 : 0x0), 1, 0);
	scf->en_vscl = set_bits32(scf->en_vscl, (en_vscl ? 0x1 : 0x0), 1, 0);
	scf->acc_hscl = set_bits32(scf->acc_hscl, acc_hscl, 31, 0);
	scf->inc_hscl = set_bits32(scf->inc_hscl, h_ratio, 24, 0);
	scf->inc_vscl = set_bits32(scf->inc_vscl, v_ratio, 24, 0);

	return 0;
}

static void k3_dss_scp_init(char __iomem *scp_base, dss_scp_t *s_scp)
{
	BUG_ON(scp_base == NULL);
	BUG_ON(s_scp == NULL);

	memset(s_scp, 0, sizeof(dss_scp_t));

	s_scp->scp_reg1 = inp32(scp_base + SCP_REG1);
	s_scp->scp_reg6 = inp32(scp_base + SCP_REG6);
	s_scp->scp_reg9 = inp32(scp_base + SCP_REG9);
}

static void k3_dss_scp_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *scp_base, dss_scp_t *s_scp)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(scp_base == NULL);
	BUG_ON(s_scp == NULL);

	k3fd->set_reg(k3fd, scp_base + SCP_REG1, s_scp->scp_reg1, 32, 0);
	k3fd->set_reg(k3fd, scp_base + SCP_REG6, s_scp->scp_reg6, 32, 0);
	k3fd->set_reg(k3fd, scp_base + SCP_REG9, s_scp->scp_reg9, 32, 0);
}

int k3_dss_scp_config(struct k3_fb_data_type *k3fd, dss_layer_t *layer, bool first_block)
{
	dss_scp_t *scp = NULL;

	BUG_ON(k3fd == NULL);

	if (!(layer->need_cap & CAP_SCP))
		return 0;

	scp = &(k3fd->dss_module.scp[layer->chn_idx]);
	k3fd->dss_module.scp_used[layer->chn_idx] = 1;

	scp->scp_reg1 = set_bits32(scp->scp_reg1, 0x1, 1, 10);
	if (first_block)
		scp->scp_reg1 = set_bits32(scp->scp_reg1, 0x1, 1, 22);
	else
		scp->scp_reg1 = set_bits32(scp->scp_reg1, 0x0, 1, 22);
	scp->scp_reg1 = set_bits32(scp->scp_reg1, 0x2, 2, 23);
	scp->scp_reg1 = set_bits32(scp->scp_reg1, 0x2, 2, 25);
	scp->scp_reg6 = set_bits32(scp->scp_reg6, 0x1, 1, 2);
	scp->scp_reg6 = set_bits32(scp->scp_reg6,
		DSS_WIDTH(layer->dst_rect.w), 16, 9);
	scp->scp_reg9 = set_bits32(scp->scp_reg9, 0x1, 1, 21);

	return 0;
}


/*******************************************************************************
** DSS CSC
*/
#define CSC_ROW	(3)
#define CSC_COL	(5)

/*	Rec.601 for Computer
*	 [ p00 p01 p02 cscidc2 cscodc2 ]
*	 [ p10 p11 p12 cscidc1 cscodc1 ]
*	 [ p20 p21 p22 cscidc0 cscodc0 ]
*/

static int CSC_COE_YUV2RGB601_NARROW[CSC_ROW][CSC_COL] = {
	{0x12a, 0x000, 0x199, 0x1f0, 0x0},
	{0x12a, 0x79d, 0x731, 0x180, 0x0},
	{0x12a, 0x205, 0x000, 0x180, 0x0}
};

static int CSC_COE_RGB2YUV601_NARROW[CSC_ROW][CSC_COL] = {
	{0x042, 0x081, 0x019, 0x0, 0x010},
	{0x7DB, 0x7B7, 0x070, 0x0, 0x080},
	{0x070, 0x7A3, 0x7EF, 0x0, 0x080}
};

static int CSC_COE_YUV2RGB709_NARROW[CSC_ROW][CSC_COL] = {
	{0x12A, 0x000, 0x1CB, 0x1F0, 0x0},
	{0x12A, 0x7CA, 0x778, 0x180, 0x0},
	{0x12A, 0x21D, 0x000, 0x180, 0x0}
};

static int CSC_COE_RGB2YUV709_NARROW[CSC_ROW][CSC_COL] = {
	{0x02F, 0x09D, 0x010, 0x0, 0x010},
	{0x7E7, 0x7AA, 0x070, 0x0, 0x080},
	{0x070, 0x79B, 0x7F7, 0x0, 0x080}
};


static int CSC_COE_YUV2RGB601_WIDE[CSC_ROW][CSC_COL] = {
	{0x100, 0x000, 0x15f, 0x000, 0x0},
	{0x100, 0x7ab, 0x74e, 0x180, 0x0},
	{0x100, 0x1bb, 0x000, 0x180, 0x0}
};

static int CSC_COE_RGB2YUV601_WIDE[CSC_ROW][CSC_COL] = {
	{0x04d, 0x096, 0x01d, 0x0, 0x000},
	{0x7d5, 0x79b, 0x083, 0x0, 0x080},
	{0x083, 0x793, 0x7ec, 0x0, 0x080},
};

static int CSC_COE_YUV2RGB709_WIDE[CSC_ROW][CSC_COL] = {
	{0x100, 0x000, 0x18a, 0x000, 0x0},
	{0x100, 0x7d2, 0x78b, 0x180, 0x0},
	{0x100, 0x1d1, 0x000, 0x180, 0x0},
};

static int CSC_COE_RGB2YUV709_WIDE[CSC_ROW][CSC_COL] = {
	{0x037, 0x0b7, 0x012, 0x0, 0x000},
	{0x7e3, 0x79c, 0x083, 0x0, 0x080},
	{0x083, 0x78a, 0x7f5, 0x0, 0x080},
};

static void k3_dss_csc_init(char __iomem *csc_base, dss_csc_t *s_csc)
{
	BUG_ON(csc_base == NULL);
	BUG_ON(s_csc == NULL);

	memset(s_csc, 0, sizeof(dss_csc_t));

	s_csc->vhdcscidc = inp32(csc_base + CSC_VHDCSCIDC);
	s_csc->vhdcscodc = inp32(csc_base + CSC_VHDCSCODC);
	s_csc->vhdcscp0 = inp32(csc_base + CSC_VHDCSCP0);
	s_csc->vhdcscp1 = inp32(csc_base + CSC_VHDCSCP1);
	s_csc->vhdcscp2 = inp32(csc_base + CSC_VHDCSCP2);
	s_csc->vhdcscp3 = inp32(csc_base + CSC_VHDCSCP3);
	s_csc->vhdcscp4 = inp32(csc_base + CSC_VHDCSCP4);
	s_csc->icg_module = inp32(csc_base + CSC_ICG_MODULE);
}

static void k3_dss_csc_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *csc_base, dss_csc_t *s_csc)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(csc_base == NULL);
	BUG_ON(s_csc == NULL);

	k3fd->set_reg(k3fd, csc_base + CSC_VHDCSCIDC, s_csc->vhdcscidc, 32, 0);
	k3fd->set_reg(k3fd, csc_base + CSC_VHDCSCODC, s_csc->vhdcscodc, 32, 0);
	k3fd->set_reg(k3fd, csc_base + CSC_VHDCSCP0, s_csc->vhdcscp0, 32, 0);
	k3fd->set_reg(k3fd, csc_base + CSC_VHDCSCP1, s_csc->vhdcscp1, 32, 0);
	k3fd->set_reg(k3fd, csc_base + CSC_VHDCSCP2, s_csc->vhdcscp2, 32, 0);
	k3fd->set_reg(k3fd, csc_base + CSC_VHDCSCP3, s_csc->vhdcscp3, 32, 0);
	k3fd->set_reg(k3fd, csc_base + CSC_VHDCSCP4, s_csc->vhdcscp4, 32, 0);
	k3fd->set_reg(k3fd, csc_base + CSC_ICG_MODULE, s_csc->icg_module, 32, 0);
}

int k3_dss_csc_config(struct k3_fb_data_type *k3fd,
	dss_layer_t *layer, dss_wb_layer_t *wb_layer)
{
	dss_csc_t *csc = NULL;
	int chn_idx = 0;
	u32 need_cap = 0;
	u32 format = 0;
	u32 csc_mode = 0;
	int (*csc_coe_yuv2rgb)[CSC_COL];
	int (*csc_coe_rgb2yuv)[CSC_COL];

	BUG_ON(k3fd == NULL);
	BUG_ON((layer == NULL) && (wb_layer == NULL));
	BUG_ON(layer && wb_layer);

	if (wb_layer) {
		chn_idx = wb_layer->chn_idx;
		need_cap = wb_layer->need_cap;
		format = wb_layer->dst.format;
		csc_mode = wb_layer->dst.csc_mode;
	} else {
		chn_idx = layer->chn_idx;
		need_cap = layer->need_cap;
		format = layer->src.format;
		csc_mode = layer->src.csc_mode;
	}

	if (!isYUV(format))
		return 0;

	if (csc_mode == DSS_CSC_601_WIDE) {
		csc_coe_yuv2rgb = CSC_COE_YUV2RGB601_WIDE;
		csc_coe_rgb2yuv = CSC_COE_RGB2YUV601_WIDE;
	} else if (csc_mode == DSS_CSC_601_NARROW) {
		csc_coe_yuv2rgb = CSC_COE_YUV2RGB601_NARROW;
		csc_coe_rgb2yuv = CSC_COE_RGB2YUV601_NARROW;
	} else if (csc_mode == DSS_CSC_709_WIDE) {
		csc_coe_yuv2rgb = CSC_COE_YUV2RGB709_WIDE;
		csc_coe_rgb2yuv = CSC_COE_RGB2YUV709_WIDE;
	} else if (csc_mode == DSS_CSC_709_NARROW) {
		csc_coe_yuv2rgb = CSC_COE_YUV2RGB709_NARROW;
		csc_coe_rgb2yuv = CSC_COE_RGB2YUV709_NARROW;
	} else {
		K3_FB_ERR("not support this csc_mode(%d)!\n", csc_mode);
		csc_coe_yuv2rgb = CSC_COE_YUV2RGB601_WIDE;
		csc_coe_rgb2yuv = CSC_COE_RGB2YUV601_WIDE;
	}

	chn_idx = k3_get_chnIndex4crossSwitch(need_cap, chn_idx);
	if (chn_idx < 0) {
		K3_FB_ERR("k3_get_chnIndex4crossSwitch failed!\n");
		return -EINVAL;
	}

	csc = &(k3fd->dss_module.csc[chn_idx]);
	k3fd->dss_module.csc_used[chn_idx] = 1;

	if (layer) {
		csc->vhdcscidc = set_bits32(csc->vhdcscidc, 0x1, 1, 27);
		csc->vhdcscidc = set_bits32(csc->vhdcscidc,
			(csc_coe_yuv2rgb[2][3] |
			(csc_coe_yuv2rgb[1][3] << 9) |
			(csc_coe_yuv2rgb[0][3] << 18)), 27, 0);

		csc->vhdcscodc = set_bits32(csc->vhdcscodc,
			(csc_coe_yuv2rgb[2][4] |
			(csc_coe_yuv2rgb[1][4] << 9) |
			(csc_coe_yuv2rgb[0][4] << 18)), 27, 0);

		csc->vhdcscp0 = set_bits32(csc->vhdcscp0, csc_coe_yuv2rgb[0][0], 11, 0);
		csc->vhdcscp0 = set_bits32(csc->vhdcscp0, csc_coe_yuv2rgb[0][1], 11, 16);

		csc->vhdcscp1 = set_bits32(csc->vhdcscp1, csc_coe_yuv2rgb[0][2], 11, 0);
		csc->vhdcscp1 = set_bits32(csc->vhdcscp1, csc_coe_yuv2rgb[1][0], 11, 16);

		csc->vhdcscp2 = set_bits32(csc->vhdcscp2, csc_coe_yuv2rgb[1][1], 11, 0);
		csc->vhdcscp2 = set_bits32(csc->vhdcscp2, csc_coe_yuv2rgb[1][2], 11, 16);

		csc->vhdcscp3 = set_bits32(csc->vhdcscp3, csc_coe_yuv2rgb[2][0], 11, 0);
		csc->vhdcscp3 = set_bits32(csc->vhdcscp3, csc_coe_yuv2rgb[2][1], 11, 16);

		csc->vhdcscp4 = set_bits32(csc->vhdcscp4, csc_coe_yuv2rgb[2][2], 11, 0);
	}

	if (wb_layer) {
		csc->vhdcscidc = set_bits32(csc->vhdcscidc, 0x1, 1, 27);
		csc->vhdcscidc = set_bits32(csc->vhdcscidc,
			(csc_coe_rgb2yuv[2][3] |
			(csc_coe_rgb2yuv[1][3] << 9) |
			(csc_coe_rgb2yuv[0][3] << 18)), 27, 0);

		csc->vhdcscodc = set_bits32(csc->vhdcscodc,
			(csc_coe_rgb2yuv[2][4] |
			(csc_coe_rgb2yuv[1][4] << 9) |
			(csc_coe_rgb2yuv[0][4] << 18)), 27, 0);

		csc->vhdcscp0 = set_bits32(csc->vhdcscp0, csc_coe_rgb2yuv[0][0], 11, 0);
		csc->vhdcscp0 = set_bits32(csc->vhdcscp0, csc_coe_rgb2yuv[0][1], 11, 16);

		csc->vhdcscp1 = set_bits32(csc->vhdcscp1, csc_coe_rgb2yuv[0][2], 11, 0);
		csc->vhdcscp1 = set_bits32(csc->vhdcscp1, csc_coe_rgb2yuv[1][0], 11, 16);

		csc->vhdcscp2 = set_bits32(csc->vhdcscp2, csc_coe_rgb2yuv[1][1], 11, 0);
		csc->vhdcscp2 = set_bits32(csc->vhdcscp2, csc_coe_rgb2yuv[1][2], 11, 16);

		csc->vhdcscp3 = set_bits32(csc->vhdcscp3, csc_coe_rgb2yuv[2][0], 11, 0);
		csc->vhdcscp3 = set_bits32(csc->vhdcscp3, csc_coe_rgb2yuv[2][1], 11, 16);

		csc->vhdcscp4 = set_bits32(csc->vhdcscp4, csc_coe_rgb2yuv[2][2], 27, 0);
	}

	csc->icg_module = set_bits32(csc->icg_module, 0x1, 1, 2);

	return 0;
}


/*******************************************************************************
** DSS OVL
*/
static int k3_get_ovl_src_select(int chn_idx, int ovl_type)
{
	int ret = 0;

	if (ovl_type == DSS_OVL_PDP || ovl_type == DSS_OVL_ADP) {
		switch (chn_idx) {
		case DPE0_CHN0:
		case DPE3_CHN0:
			ret = 0;
			break;
		case DPE0_CHN1:
		case DPE3_CHN1:
			ret = 1;
			break;
		case DPE0_CHN2:
		case DPE3_CHN2:
			ret = 2;
			break;
		case DPE0_CHN3:
		case DPE3_CHN3:
			ret = 3;
			break;

		case DPE2_CHN0:
			ret = 4;
			break;
		case DPE2_CHN1:
			ret = 5;
			break;
		case DPE2_CHN2:
			ret = 6;
			break;
		case DPE2_CHN3:
			ret = 7;
			break;

		default:
			K3_FB_ERR("not support, chn_idx=%d, ovl_type=%d!\n", chn_idx, ovl_type);
			ret = -1;
			break;
		}
	} else if (ovl_type == DSS_OVL_SDP) {
		switch (chn_idx) {
		case DPE1_CHN0:
			ret = 0;
			break;
		case DPE1_CHN1:
			ret = 1;
			break;

		case DPE2_CHN0:
			ret = 2;
			break;
		case DPE2_CHN1:
			ret = 3;
			break;
		default:
			K3_FB_ERR("not support, chn_idx=%d, ovl_type=%d!\n", chn_idx, ovl_type);
			ret = -1;
			break;
		}
	} else {
		K3_FB_ERR("not support this ovl_type(%d)!\n", ovl_type);
		ret = -1;
	}

	return ret;
}

static u32 get_ovl_blending_alpha_val(dss_layer_t *layer)
{
	int blend_mode = 0;

	int alpha_smode = 0;
	int src_pmode = 0;
	int src_gmode = 0;
	int src_amode = 0;
	int fix_mode = 0;
	int dst_pmode = 0;
	int dst_gmode = 0;
	int dst_amode = 0;
	int src_glb_alpha = 0;
	int dst_glb_alpha = 0;

	bool has_per_pixel_alpha = false;

	int x = 0;
	u32 alpha_val = 0;

	BUG_ON(layer == NULL);

	if (layer->glb_alpha < 0) {
		layer->glb_alpha = 0;
		K3_FB_ERR("layer's glb_alpha(0x%x) is out of range!", layer->glb_alpha);
	} else if (layer->glb_alpha > 0xFF) {
		layer->glb_alpha = 0xFF;
		K3_FB_ERR("layer's glb_alpha(0x%x) is out of range!", layer->glb_alpha);
	}

	has_per_pixel_alpha = hal_format_has_alpha(layer->src.format);

	src_glb_alpha = layer->glb_alpha;
	dst_glb_alpha = layer->glb_alpha;

	if (layer->layer_idx == 0) {
		blend_mode = DSS_BLEND_SRC;
	} else {
		if (layer->blending == K3_FB_BLENDING_PREMULT) {
			if (has_per_pixel_alpha) {
				blend_mode = (layer->glb_alpha < 0xFF) ? DSS_BLEND_FIX_PER12 : DSS_BLEND_SRC_OVER_DST;
			} else {
				blend_mode = (layer->glb_alpha < 0xFF) ? DSS_BLEND_FIX_PER8 : DSS_BLEND_SRC;
			}
		} else if (layer->blending == K3_FB_BLENDING_COVERAGE) {
			if (has_per_pixel_alpha) {
				blend_mode = (layer->glb_alpha < 0xFF) ? DSS_BLEND_FIX_PER13 : DSS_BLEND_FIX_OVER;
			} else {
				blend_mode = (layer->glb_alpha < 0xFF) ? DSS_BLEND_FIX_PER8 : DSS_BLEND_SRC;
			}
		} else {
			blend_mode = DSS_BLEND_SRC;
		}
	}

	if (g_debug_ovl_online_composer) {
		K3_FB_INFO("layer_idx=%d, chn_idx=%d, has_per_pixel_alpha=%d, src_glb_alpha=%d, dst_glb_alpha=%d, blend_mode=%d.\n",
			layer->layer_idx, layer->chn_idx, has_per_pixel_alpha, src_glb_alpha, dst_glb_alpha, blend_mode);
	}

	switch (blend_mode) {
	case DSS_BLEND_CLEAR:
		alpha_smode = 0,
		src_pmode = x, src_gmode = x, src_amode = 0, fix_mode = x,
		dst_pmode = x, dst_gmode = x, dst_amode = 0;
		break;
	case DSS_BLEND_SRC:
		alpha_smode = 0,
		src_pmode = x, src_gmode = x, src_amode = 0, fix_mode = x,
		dst_pmode = x, dst_gmode = x, dst_amode = 1;
		break;
	case DSS_BLEND_DST:
		alpha_smode = 0,
		src_pmode = x, src_gmode = x, src_amode = 1, fix_mode = x,
		dst_pmode = x, dst_gmode = x, dst_amode = 0;
		break;
	case DSS_BLEND_SRC_OVER_DST:
		alpha_smode = 0,
		src_pmode = 1, src_gmode = 0, src_amode = 3, fix_mode = x,
		dst_pmode = x, dst_gmode = x, dst_amode = 1;
		break;
	case DSS_BLEND_DST_OVER_SRC:
		alpha_smode = 0,
		src_pmode = x, src_gmode = x, src_amode = 1, fix_mode = 0,
		dst_pmode = 1, dst_gmode = 0, dst_amode = 3;
		break;
	case DSS_BLEND_SRC_IN_DST:
		alpha_smode = 0,
		src_pmode = x, src_gmode = x, src_amode = 0, fix_mode = 0,
		dst_pmode = 0, dst_gmode = 0, dst_amode = 3;
		break;
	case DSS_BLEND_DST_IN_SRC:
		alpha_smode = 0,
		src_pmode = 0, src_gmode = 0, src_amode = 3, fix_mode = x,
		dst_pmode = x, dst_gmode = x, dst_amode = 0;
		break;
	case DSS_BLEND_SRC_OUT_DST:
		alpha_smode = 0,
		src_pmode = x, src_gmode = x, src_amode = 0, fix_mode = 0,
		dst_pmode = 1, dst_gmode = 0, dst_amode = 3;
		break;
	case DSS_BLEND_DST_OUT_SRC:
		alpha_smode = 0,
		src_pmode = 1, src_gmode = 0, src_amode = 3, fix_mode = x,
		dst_pmode = x, dst_gmode = x, dst_amode = 0;
		break;
	case DSS_BLEND_SRC_ATOP_DST:
		alpha_smode = 0,
		src_pmode = 1, src_gmode = 0, src_amode = 3, fix_mode = 0,
		dst_pmode = 0, dst_gmode = 0, dst_amode = 3;
		break;
	case DSS_BLEND_DST_ATOP_SRC:
		alpha_smode = 0,
		src_pmode = 0, src_gmode = 0, src_amode = 3, fix_mode = 0,
		dst_pmode = 1, dst_gmode = 0, dst_amode = 3;
		break;
	case DSS_BLEND_SRC_XOR_DST:
		alpha_smode = 0,
		src_pmode = 1, src_gmode = 0, src_amode = 3, fix_mode = 0,
		dst_pmode = 1, dst_gmode = 0, dst_amode = 3;
		break;
	case DSS_BLEND_SRC_ADD_DST:
		alpha_smode = 0,
		src_pmode = x, src_gmode = x, src_amode = 1, fix_mode = x,
		dst_pmode = x, dst_gmode = x, dst_amode = 1;
		break;
	case DSS_BLEND_FIX_OVER:
		alpha_smode = 0,
		src_pmode = 1, src_gmode = 0, src_amode = 3, fix_mode = 1,
		dst_pmode = 0, dst_gmode = 0, dst_amode = 3;
		break;
	case DSS_BLEND_FIX_PER0:
		alpha_smode = 0,
		src_pmode = 1, src_gmode = 0, src_amode = 3, fix_mode = x,
		dst_pmode = x, dst_gmode = 2, dst_amode = 3;
		break;
	case DSS_BLEND_FIX_PER1:
		alpha_smode = 1,
		src_pmode = 1, src_gmode = 0, src_amode = 3, fix_mode = 1,
		dst_pmode = 0, dst_gmode = 1, dst_amode = 3;
		break;
	case DSS_BLEND_FIX_PER2:
		alpha_smode = 0,
		src_pmode = x, src_gmode = 2, src_amode = 2, fix_mode = x,
		dst_pmode = x, dst_gmode = x, dst_amode = 1;
		break;
	case DSS_BLEND_FIX_PER3:
		alpha_smode = 0,
		src_pmode = x, src_gmode = x, src_amode = 1, fix_mode = x,
		dst_pmode = x, dst_gmode = 2, dst_amode = 2;
		break;
	case DSS_BLEND_FIX_PER4:
		alpha_smode = 0,
		src_pmode = x, src_gmode = x, src_amode = 0, fix_mode = x,
		dst_pmode = x, dst_gmode = 2, dst_amode = 3;
		break;
	case DSS_BLEND_FIX_PER5:
		alpha_smode = 0,
		src_pmode = x, src_gmode = 2, src_amode = 3, fix_mode = x,
		dst_pmode = x, dst_gmode = x, dst_amode = 0;
		break;
	case DSS_BLEND_FIX_PER6:
		alpha_smode = 0,
		src_pmode = x, src_gmode = x, src_amode = 0, fix_mode = x,
		dst_pmode = x, dst_gmode = 2, dst_amode = 2;
		break;
	case DSS_BLEND_FIX_PER7:
		alpha_smode = 0,
		src_pmode = x, src_gmode = 2, src_amode = 2, fix_mode = x,
		dst_pmode = x, dst_gmode = x, dst_amode = 0;
		break;
	case DSS_BLEND_FIX_PER8:
		alpha_smode = 0,
		src_pmode = x, src_gmode = 2, src_amode = 2, fix_mode = x,
		dst_pmode = x, dst_gmode = 2, dst_amode = 3;
		break;
	case DSS_BLEND_FIX_PER9:
		alpha_smode = 0,
		src_pmode = x, src_gmode = 2, src_amode = 3, fix_mode = x,
		dst_pmode = x, dst_gmode = 2, dst_amode = 2;
		break;
	case DSS_BLEND_FIX_PER10:
		alpha_smode = 0,
		src_pmode = x, src_gmode = 2, src_amode = 2, fix_mode = x,
		dst_pmode = x, dst_gmode = 2, dst_amode = 2;
		break;
	case DSS_BLEND_FIX_PER11:
		alpha_smode = 0,
		src_pmode = x, src_gmode = 2, src_amode = 3, fix_mode = x,
		dst_pmode = x, dst_gmode = 2, dst_amode = 3;
		break;
	case DSS_BLEND_FIX_PER12:
		alpha_smode = 0,
		src_pmode = 0, src_gmode = 1, src_amode = 2, fix_mode = x,
		dst_pmode = x, dst_gmode = 2, dst_amode = 3;
		break;
	case DSS_BLEND_FIX_PER13:
		alpha_smode = 0,
		src_pmode = 0, src_gmode = 1, src_amode = 2, fix_mode = 1,
		dst_pmode = 0, dst_gmode = 1, dst_amode = 3;
		break;
	default:
		K3_FB_ERR("not support this blend mode(%d)!", blend_mode);
		break;
	}

	alpha_val = (dst_glb_alpha << 0) | (fix_mode << 8) | (dst_pmode << 9) | (dst_gmode << 12) | (dst_amode << 14) |
		(src_glb_alpha << 16) | (alpha_smode << 24) | (src_pmode << 25) | (src_gmode << 28) | (src_amode << 30);

	return alpha_val;
}

static void k3_dss_ovl_init(char __iomem *ovl_base, dss_ovl_t *s_ovl)
{
	int i = 0;

	BUG_ON(ovl_base == NULL);
	BUG_ON(s_ovl == NULL);

	memset(s_ovl, 0, sizeof(dss_ovl_t));

	s_ovl->ovl_ov_size = inp32(ovl_base + OVL_OV_SIZE);
	s_ovl->ovl_ov_bg_color = inp32(ovl_base + OVL_OV_BG_COLOR);
	s_ovl->ovl_dst_startpos = inp32(ovl_base + OVL_DST_STARTPOS);
	s_ovl->ovl_dst_endpos = inp32(ovl_base + OVL_DST_ENDPOS);
	s_ovl->ovl_ov_gcfg = inp32(ovl_base + OVL_OV_GCFG);

	for (i = 0; i < OVL_LAYER_NUM; i++) {
		s_ovl->ovl_layer[i].layer_pos =
			inp32(ovl_base + OVL_LAYER0_POS + i * 0x2C);
		s_ovl->ovl_layer[i].layer_size =
			inp32(ovl_base + OVL_LAYER0_SIZE + i * 0x2C);
		s_ovl->ovl_layer[i].layer_pattern =
			inp32(ovl_base + OVL_LAYER0_PATTERN + i * 0x2C);
		s_ovl->ovl_layer[i].layer_alpha =
			inp32(ovl_base + OVL_LAYER0_ALPHA + i * 0x2C);
		s_ovl->ovl_layer[i].layer_cfg =
			inp32(ovl_base + OVL_LAYER0_CFG + i * 0x2C);

		s_ovl->ovl_layer_position[i].ovl_layer_pspos =
			inp32(ovl_base + OVL_LAYER0_PSPOS + i * 0x8);
		s_ovl->ovl_layer_position[i].ovl_layer_pepos =
			inp32(ovl_base + OVL_LAYER0_PEPOS + i * 0x8);
	}

}

static void k3_dss_ovl_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *ovl_base, dss_ovl_t *s_ovl)
{
	int i = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(ovl_base == NULL);
	BUG_ON(s_ovl == NULL);

	k3fd->set_reg(k3fd, ovl_base + OVL_OV_SIZE, s_ovl->ovl_ov_size, 32, 0);
	k3fd->set_reg(k3fd, ovl_base + OVL_OV_BG_COLOR, s_ovl->ovl_ov_bg_color, 32, 0);
	k3fd->set_reg(k3fd, ovl_base + OVL_DST_STARTPOS, s_ovl->ovl_dst_startpos, 32, 0);
	k3fd->set_reg(k3fd, ovl_base + OVL_DST_ENDPOS, s_ovl->ovl_dst_endpos, 32, 0);
	k3fd->set_reg(k3fd, ovl_base + OVL_OV_GCFG, s_ovl->ovl_ov_gcfg, 32, 0);

	for (i = 0; i < OVL_LAYER_NUM; i++) {
		k3fd->set_reg(k3fd, ovl_base + OVL_LAYER0_POS + i * 0x2C,
			s_ovl->ovl_layer[i].layer_pos, 32, 0);
		k3fd->set_reg(k3fd, ovl_base + OVL_LAYER0_SIZE + i * 0x2C,
			s_ovl->ovl_layer[i].layer_size, 32, 0);
		k3fd->set_reg(k3fd, ovl_base + OVL_LAYER0_PATTERN + i * 0x2C,
			s_ovl->ovl_layer[i].layer_pattern, 32, 0);
		k3fd->set_reg(k3fd, ovl_base + OVL_LAYER0_ALPHA + i * 0x2C,
			s_ovl->ovl_layer[i].layer_alpha, 32, 0);
		k3fd->set_reg(k3fd, ovl_base + OVL_LAYER0_CFG + i * 0x2C,
			s_ovl->ovl_layer[i].layer_cfg, 32, 0);

		k3fd->set_reg(k3fd, ovl_base + OVL_LAYER0_PSPOS + i * 0x8,
			s_ovl->ovl_layer_position[i].ovl_layer_pspos, 32, 0);
		k3fd->set_reg(k3fd, ovl_base + OVL_LAYER0_PEPOS + i * 0x8,
			s_ovl->ovl_layer_position[i].ovl_layer_pepos, 32, 0);
	}
}

int k3_dss_ovl_base_config(struct k3_fb_data_type *k3fd,
	dss_rect_t *wb_block_rect, u32 ovl_flags)
{
	dss_ovl_t *ovl = NULL;
	u8 ovl_type = 0;
	int img_width = 0;
	int img_height = 0;

	BUG_ON(k3fd == NULL);

	ovl_type = k3fd->ovl_type;

	ovl = &(k3fd->dss_module.ov[ovl_type]);
	k3fd->dss_module.ov_used[ovl_type] = 1;

	if (wb_block_rect) {
		img_width = wb_block_rect->w;
		img_height = wb_block_rect->h;
	} else {
		img_width = k3fd->panel_info.xres;
		img_height = k3fd->panel_info.yres;
	}

	ovl->ovl_ov_size = set_bits32(ovl->ovl_ov_size, DSS_WIDTH(img_width), 15, 0);
	ovl->ovl_ov_size = set_bits32(ovl->ovl_ov_size, DSS_HEIGHT(img_height), 15, 16);
	ovl->ovl_ov_bg_color= set_bits32(ovl->ovl_ov_bg_color, 0xFF000000, 32, 0);
	ovl->ovl_dst_startpos = set_bits32(ovl->ovl_dst_startpos, 0x0, 32, 0);
	ovl->ovl_dst_endpos = set_bits32(ovl->ovl_dst_endpos, DSS_WIDTH(img_width), 15, 0);
	ovl->ovl_dst_endpos = set_bits32(ovl->ovl_dst_endpos, DSS_HEIGHT(img_height), 15, 16);
	ovl->ovl_ov_gcfg = set_bits32(ovl->ovl_ov_gcfg, 0x1, 1, 0);
	ovl->ovl_ov_gcfg = set_bits32(ovl->ovl_ov_gcfg, 0x1, 1, 16);

	/*
	** handle glb: OVL_MUX
	** OV0_MUX
	** OV1_MUX
	** OV2_MUX
	*/
	if (k3fd->dss_glb.ovl_mux_base) {
		if (ovl_type == DSS_OVL_PDP) {
			k3fd->dss_glb.ovl_mux_val =
				set_bits32(k3fd->dss_glb.ovl_mux_val, (0x1 | (0x1 << 2)), 4, 0);
			k3fd->dss_glb.ovl_mux_used = 1;
		} else if (ovl_type == DSS_OVL_SDP) {
			k3fd->dss_glb.ovl_mux_val =
				set_bits32(k3fd->dss_glb.ovl_mux_val, (0x1 | (0x1 << 2)), 3, 0);
			k3fd->dss_glb.ovl_mux_used = 1;
		} else if (ovl_type == DSS_OVL_ADP) {
			k3fd->dss_glb.ovl_mux_val =
				set_bits32(k3fd->dss_glb.ovl_mux_val, 0x1, 1, 3);
			k3fd->dss_glb.ovl_mux_used = 1;
		} else {
			K3_FB_ERR("ovl_mux, ovl_type(%d) is invalid!\n", ovl_type);
		}
	} else {
		K3_FB_ERR("ovl_type(%d), ovl_mux_base is invalid!\n", ovl_type);
	}

	/*
	** handle glb: OVL_SCENE
	** DSS_GLB_OV0_SCENE
	** DSS_GLB_OV1_SCENE
	** DSS_GLB_OV2_SCENE
	*/
	if (k3fd->dss_glb.ovl_scene_base) {
		k3fd->dss_glb.ovl_scene_val =
			set_bits32(k3fd->dss_glb.ovl_scene_val, ovl_flags, 3, 0);
		k3fd->dss_glb.ovl_scene_used = 1;
	} else {
		K3_FB_ERR("ovl_type(%d), ovl_scene_base is invalid!\n", ovl_type);
	}

	return 0;
}

int k3_dss_ovl_layer_config(struct k3_fb_data_type *k3fd,
	dss_layer_t *layer, dss_rect_t *wb_block_rect)
{
	dss_ovl_t *ovl = NULL;
	u8 ovl_type = 0;
	int chn_idx = 0;
	int chn_idx_tmp = 0;
	int src_select = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(layer == NULL);

	ovl_type = k3fd->ovl_type;
	chn_idx = layer->chn_idx;

	ovl = &(k3fd->dss_module.ov[ovl_type]);
	k3fd->dss_module.ov_used[ovl_type] = 1;

	if (layer->need_cap & CAP_BASE) {
		if ((layer->layer_idx == 0) &&
			(layer->dst_rect.x == 0) &&
			(layer->dst_rect.y == 0)  &&
			(layer->dst_rect.w == k3fd->panel_info.xres) &&
			(layer->dst_rect.h == k3fd->panel_info.yres)) {

			ovl->ovl_ov_bg_color = set_bits32(ovl->ovl_ov_bg_color, layer->color, 32, 0);
			ovl->ovl_ov_gcfg = set_bits32(ovl->ovl_ov_gcfg, 0x1, 1, 16);
		} else {
			K3_FB_ERR("layer%d not a base layer!", layer->layer_idx);
		}

		return 0;
	}

	if (wb_block_rect) {
		ovl->ovl_layer[layer->layer_idx].layer_pos = set_bits32(ovl->ovl_layer[layer->layer_idx].layer_pos,
			(layer->dst_rect.x - wb_block_rect->x), 15, 0);
		ovl->ovl_layer[layer->layer_idx].layer_pos = set_bits32(ovl->ovl_layer[layer->layer_idx].layer_pos,
			(layer->dst_rect.y - wb_block_rect->y), 15, 16);

		ovl->ovl_layer[layer->layer_idx].layer_size = set_bits32(ovl->ovl_layer[layer->layer_idx].layer_size,
			(layer->dst_rect.x - wb_block_rect->x + DSS_WIDTH(layer->dst_rect.w)), 15, 0);
		ovl->ovl_layer[layer->layer_idx].layer_size = set_bits32(ovl->ovl_layer[layer->layer_idx].layer_size,
			(layer->dst_rect.y - wb_block_rect->y + DSS_HEIGHT(layer->dst_rect.h)), 15, 16);
	} else {
		ovl->ovl_layer[layer->layer_idx].layer_pos = set_bits32(ovl->ovl_layer[layer->layer_idx].layer_pos,
			layer->dst_rect.x, 15, 0);
		ovl->ovl_layer[layer->layer_idx].layer_pos = set_bits32(ovl->ovl_layer[layer->layer_idx].layer_pos,
			layer->dst_rect.y, 15, 16);

		ovl->ovl_layer[layer->layer_idx].layer_size = set_bits32(ovl->ovl_layer[layer->layer_idx].layer_size,
			DSS_WIDTH(layer->dst_rect.x + layer->dst_rect.w), 15, 0);
		ovl->ovl_layer[layer->layer_idx].layer_size = set_bits32(ovl->ovl_layer[layer->layer_idx].layer_size,
			DSS_HEIGHT(layer->dst_rect.y + layer->dst_rect.h), 15, 16);
	}

	ovl->ovl_layer[layer->layer_idx].layer_alpha = set_bits32(ovl->ovl_layer[layer->layer_idx].layer_alpha,
		get_ovl_blending_alpha_val(layer), 32, 0);

	if (layer->need_cap & CAP_DIM) {
		ovl->ovl_layer[layer->layer_idx].layer_pattern =
			set_bits32(ovl->ovl_layer[layer->layer_idx].layer_pattern, layer->color, 32, 0);
		ovl->ovl_layer[layer->layer_idx].layer_cfg =
			set_bits32(ovl->ovl_layer[layer->layer_idx].layer_cfg, 0x1, 1, 0);
		ovl->ovl_layer[layer->layer_idx].layer_cfg =
			set_bits32(ovl->ovl_layer[layer->layer_idx].layer_cfg, 0x1, 1, 8);
	} else {
		chn_idx_tmp = k3_get_chnIndex4crossSwitch(layer->need_cap, chn_idx);
		if (chn_idx_tmp < 0) {
			K3_FB_ERR("layer%d chn%d k3_get_chnIndex4crossSwitch failed!\n",
				layer->layer_idx, chn_idx);
			return -EINVAL;
		}

		src_select = k3_get_ovl_src_select(chn_idx_tmp, ovl_type);
		if (src_select < 0) {
			K3_FB_ERR("layer%d chn%d k3_get_ovl_src_select failed!\n",
				layer->layer_idx, chn_idx);
			return -EINVAL;
		}

		ovl->ovl_layer[layer->layer_idx].layer_pattern =
			set_bits32(ovl->ovl_layer[layer->layer_idx].layer_pattern, 0x0, 32, 0);
		ovl->ovl_layer[layer->layer_idx].layer_cfg =
			set_bits32(ovl->ovl_layer[layer->layer_idx].layer_cfg, 0x1, 1, 0);
		ovl->ovl_layer[layer->layer_idx].layer_cfg =
			set_bits32(ovl->ovl_layer[layer->layer_idx].layer_cfg, 0x0, 1, 8);
		ovl->ovl_layer[layer->layer_idx].layer_cfg =
			set_bits32(ovl->ovl_layer[layer->layer_idx].layer_cfg, src_select, 3, 9);
	}

	ovl->ovl_layer_position[layer->layer_idx].ovl_layer_pspos =
		set_bits32(ovl->ovl_layer_position[layer->layer_idx].ovl_layer_pspos, layer->dst_rect.x, 15, 0);
	ovl->ovl_layer_position[layer->layer_idx].ovl_layer_pspos =
		set_bits32(ovl->ovl_layer_position[layer->layer_idx].ovl_layer_pspos, layer->dst_rect.y, 15, 16);
	ovl->ovl_layer_position[layer->layer_idx].ovl_layer_pepos =
		set_bits32(ovl->ovl_layer_position[layer->layer_idx].ovl_layer_pepos,
		DSS_WIDTH(layer->dst_rect.x + layer->dst_rect.w), 15, 0);
	ovl->ovl_layer_position[layer->layer_idx].ovl_layer_pepos =
		set_bits32(ovl->ovl_layer_position[layer->layer_idx].ovl_layer_pepos,
		DSS_HEIGHT(layer->dst_rect.y + layer->dst_rect.h), 15, 16);

	return 0;
}

int k3_dss_ovl_optimized(struct k3_fb_data_type *k3fd)
{
	char __iomem *ovl_base = NULL;
	int offset = 0;
	int i = 0;

	u32 layer_cfg_val = 0;
	u32 layer_info_alpha_val = 0;
	u32 layer_info_srccolor_val = 0;
	u32 layer_pattern = 0;

	BUG_ON(k3fd == NULL);

	ovl_base = k3fd->dss_base +
		g_dss_module_ovl_base[k3fd->ovl_type][MODULE_OVL_BASE];

	for (i = 0; i < OVL_LAYER_NUM; i++) {
		offset = i * 0x2C;

		layer_cfg_val = inp32(ovl_base + OVL_LAYER0_CFG + offset);
		layer_info_alpha_val = inp32(ovl_base + OVL_LAYER0_INFO_ALPHA + offset);
		layer_info_srccolor_val = inp32(ovl_base + OVL_LAYER0_INFO_SRCCOLOR + offset);

		if ((layer_cfg_val & BIT_OVL_LAYER_ENABLE) &&
			!(layer_cfg_val & BIT_OVL_LAYER_SRC_CFG) &&
			(layer_info_alpha_val & BIT_OVL_LAYER_SRCALPHA_FLAG) &&
			(layer_info_srccolor_val & BIT_OVL_LAYER_SRCCOLOR_FLAG)) {

			layer_pattern = ((layer_info_alpha_val << 8) & 0xFF000000) |
				((layer_info_srccolor_val >> 8) & 0x00FFFFFF);
			set_reg(ovl_base + OVL_LAYER0_PATTERN + offset, layer_pattern, 32, 0);

			layer_cfg_val |= BIT_OVL_LAYER_SRC_CFG;
			set_reg(ovl_base + OVL_LAYER0_CFG + offset, layer_cfg_val, 32, 0);

			if (g_debug_ovl_optimized) {
				K3_FB_INFO("ovl src%d: layer_cfg_val(0x%x), layer_info_alpha(0x%x), "
					"layer_info_srccolor(0x%x), layer_pattern(0x%x).\n",
					i, layer_cfg_val, layer_info_alpha_val, layer_info_srccolor_val, layer_pattern);
			}
		}
	}

	return 0;
}

int k3_dss_ovl_check_pure_layer(struct k3_fb_data_type *k3fd, int layer_index,
	u16* alpha_flag, u16* color_flag, u32* color)
{
	u32 layer_info_alpha_val = 0;
	u32 layer_info_srccolor_val = 0;
	char __iomem *ovl_base = NULL;
	int offset = 0;

	BUG_ON(k3fd == NULL);

	ovl_base = k3fd->dss_base +
		g_dss_module_ovl_base[k3fd->ovl_type][MODULE_OVL_BASE];

	if (!k3fd->panel_power_on) {
		K3_FB_ERR("fb%d panel is power off!", k3fd->index);
		return -EINVAL;
	}

	if (layer_index < 0 || layer_index >= OVL_LAYER_NUM) {
		K3_FB_ERR("fb%d, invalid layer index: %d", k3fd->index, layer_index);
		return -EINVAL;
	}

	k3fb_activate_vsync(k3fd);

	offset= layer_index * 0x2C;

	layer_info_alpha_val = inp32(ovl_base + OVL_LAYER0_INFO_ALPHA + offset);
	layer_info_srccolor_val = inp32(ovl_base + OVL_LAYER0_INFO_SRCCOLOR + offset);

	if (layer_info_alpha_val & BIT_OVL_LAYER_SRCALPHA_FLAG)
		*alpha_flag = 1;
	else
		*alpha_flag = 0;

	if (layer_info_srccolor_val & BIT_OVL_LAYER_SRCCOLOR_FLAG)
		*color_flag = 1;
	else
		*color_flag = 0;

	*color = ((layer_info_alpha_val << 8) & 0xFF000000) |
		((layer_info_srccolor_val >> 8) & 0x00FFFFFF);

	k3fb_deactivate_vsync(k3fd);

	return 0;
}


/*******************************************************************************
** DSS MUX
*/
int k3_dss_mux_config(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req, dss_layer_t *layer)
{
	int tmp_val = 0;
	bool is_yuv_semi_planar = false;
	bool is_yuv_planar = false;
	int tmp_chn_idx = 0;

	int layer_idx = 0;
	int chn_idx = 0;
	u32 need_cap = 0;
	u32 flags = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(k3fd == NULL);
	BUG_ON(layer == NULL);

	layer_idx = layer->layer_idx;
	chn_idx = layer->chn_idx;
	need_cap = layer->need_cap;
	flags = layer->flags;
	is_yuv_semi_planar = isYUVSemiPlanar(layer->src.format);
	is_yuv_planar = isYUVPlanar(layer->src.format);

	/* handle channel ctrl and channel scene */
	if (k3fd->dss_glb.chn_ctl_base[chn_idx]) {
		if (is_yuv_semi_planar || is_yuv_planar) {
			k3fd->dss_glb.chn_ctl_val[chn_idx] =
				set_bits32(k3fd->dss_glb.chn_ctl_val[chn_idx], 0x3, 2, 0);
			k3fd->dss_glb.chn_ctl_used[chn_idx] = 1;
		} else {
			k3fd->dss_glb.chn_ctl_val[chn_idx] =
				set_bits32(k3fd->dss_glb.chn_ctl_val[chn_idx], 0x1, 1, 0);
			k3fd->dss_glb.chn_ctl_used[chn_idx] = 1;
		}
		k3fd->dss_glb.chn_ctl_val[chn_idx] =
			set_bits32(k3fd->dss_glb.chn_ctl_val[chn_idx], layer->flags, 3, 4);
		k3fd->dss_glb.chn_ctl_used[chn_idx] = 1;

		if (is_yuv_planar && (chn_idx == DPE3_CHN0)) {
			tmp_chn_idx = chn_idx + 1;
			if (k3fd->dss_glb.chn_ctl_base[tmp_chn_idx]) {
				k3fd->dss_glb.chn_ctl_val[tmp_chn_idx] =
					set_bits32(k3fd->dss_glb.chn_ctl_val[tmp_chn_idx], 0x1, 1, 0);
				k3fd->dss_glb.chn_ctl_val[tmp_chn_idx] =
					set_bits32(k3fd->dss_glb.chn_ctl_val[tmp_chn_idx], layer->flags, 3, 4);
				k3fd->dss_glb.chn_ctl_used[tmp_chn_idx] = 1;
			} else {
				K3_FB_ERR("layer_idx=%d, chn_idx=%d, chn_ctl_base is null!\n",
					layer_idx, tmp_chn_idx);
			}
		}
	}

#if 0
	/* OVL MUX */
	if (need_cap & (CAP_FB_DMA | CAP_FB_POST)) {
		set_reg(ovl_mux_base, 0x0, 2, 0);
	} else {
		set_reg(ovl_mux_base, 0x5, 4, 0);
	}
#endif

	switch (chn_idx) {
	case DPE0_CHN0:
	case DPE0_CHN1:
		{
			/* CROSS SWITCH */
			if (k3fd->dss_glb.dpe0_cross_switch_base) {
				if (need_cap & CAP_CROSS_SWITCH) {
					k3fd->dss_glb.dpe0_cross_switch_val =
						set_bits32(k3fd->dss_glb.dpe0_cross_switch_val, 0x1, 1, 0);
					k3fd->dss_glb.dpe0_cross_switch_used = 1;
				} else {
					k3fd->dss_glb.dpe0_cross_switch_val =
						set_bits32(k3fd->dss_glb.dpe0_cross_switch_val, 0x0, 1, 0);
					k3fd->dss_glb.dpe0_cross_switch_used = 1;
				}
			} else {
				K3_FB_ERR("chn_idx(%d), dpe0_cross_switch_base is invalid!\n", chn_idx);
			}

			/* CHN MUX */
			if (k3fd->dss_glb.chn_mux_base[chn_idx]) {
				/* 0:to ov0 1:to wbe0 dma0 */
				if (pov_req->wb_enable &&
					(pov_req->wb_layer_info.chn_idx == WBE0_CHN0)) {
					k3fd->dss_glb.chn_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.chn_mux_val[chn_idx], 0x1, 1, 0);
					k3fd->dss_glb.chn_mux_used[chn_idx] = 1;
				} else {
					k3fd->dss_glb.chn_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.chn_mux_val[chn_idx], 0x0, 1, 0);
					k3fd->dss_glb.chn_mux_used[chn_idx] = 1;
				}
			} else {
				K3_FB_ERR("layer_idx(%d), chn_mux_base is NULL!\n", layer_idx);
			}
		}
		break;
	case DPE0_CHN2:
	case DPE0_CHN3:
		break;

	/* bg layer */
	case DPE0_CHN4:
		break;

	case DPE1_CHN0:
		{
			/* CHN MUX */
			if (k3fd->dss_glb.chn_mux_base[chn_idx]) {
				/* 0:to ov1 1:to wbe0 dma1 */
				if (pov_req->wb_enable &&
					(pov_req->wb_layer_info.chn_idx == WBE0_CHN1)) {
					k3fd->dss_glb.chn_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.chn_mux_val[chn_idx], 0x1, 1, 0);
					k3fd->dss_glb.chn_mux_used[chn_idx] = 1;
				} else {
					k3fd->dss_glb.chn_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.chn_mux_val[chn_idx], 0x0, 1, 0);
					k3fd->dss_glb.chn_mux_used[chn_idx] = 1;
				}
			} else {
				K3_FB_ERR("layer_idx(%d), chn_mux_base is NULL!\n", layer_idx);
			}

			/* SCF MUX */
			if (need_cap & CAP_SCL) {
				if (k3fd->dss_glb.scf_mux_base[chn_idx]) {
					/* 1:WBE0, 2:DPE1 */
					k3fd->dss_glb.scf_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.scf_mux_val[chn_idx], 0x2, 2, 0);
					k3fd->dss_glb.scf_mux_used[chn_idx] = 1;
				} else {
					K3_FB_ERR("layer_idx(%d), scf_mux_base is NULL!\n", layer_idx);
				}
			}
		}
		break;
	case DPE1_CHN1:
		break;

	/* bg layer */
	case DPE1_CHN2:
		break;

	case DPE2_CHN0:
		{
			/* ROT mux */
			if (need_cap & CAP_ROT) {
				if (k3fd->dss_glb.rot_mux_base[chn_idx]) {
					/* 0: DPE2; 1:WBE0 */
					k3fd->dss_glb.rot_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.rot_mux_val[chn_idx], 0x0, 1, 0);
					k3fd->dss_glb.rot_mux_used[chn_idx] = 1;
				} else {
					K3_FB_ERR("layer_idx(%d), rot_mux_base is NULL!\n", layer_idx);
				}
			}
		}
		//break;
	case DPE2_CHN1:
		{
			/* CROSS SWITCH */
			if (k3fd->dss_glb.dpe2_cross_switch_base) {
				if (need_cap & CAP_CROSS_SWITCH) {
					k3fd->dss_glb.dpe2_cross_switch_val =
						set_bits32(k3fd->dss_glb.dpe2_cross_switch_val, 0x1, 1, 0);
					k3fd->dss_glb.dpe2_cross_switch_used = 1;
				} else {
					k3fd->dss_glb.dpe2_cross_switch_val =
						set_bits32(k3fd->dss_glb.dpe2_cross_switch_val, 0x0, 1, 0);
					k3fd->dss_glb.dpe2_cross_switch_used = 1;
				}
			} else {
				K3_FB_ERR("chn_idx(%d), dpe2_cross_switch_base is invalid!\n", chn_idx);
			}
		}
		//break;
	case DPE2_CHN2:
	case DPE2_CHN3:
		{
			/* CHN MUX */
			tmp_chn_idx = chn_idx;
			if (need_cap & CAP_CROSS_SWITCH) {
				if (tmp_chn_idx == DPE2_CHN0) {
					tmp_chn_idx = DPE2_CHN1;
				} else if (tmp_chn_idx == DPE2_CHN1) {
					tmp_chn_idx = DPE2_CHN0;
				} else {
					; /* do nothing */
				}
			}

			if (k3fd->dss_glb.chn_mux_base[tmp_chn_idx]) {
				/* 0:to pdp 1:to sdp 2:reserved 3:to adp */
				if (k3fd->ovl_type == DSS_OVL_PDP) {
					tmp_val = 0x0;
				} else if (k3fd->ovl_type == DSS_OVL_SDP) {
					tmp_val = 0x1;
				} else if (k3fd->ovl_type == DSS_OVL_ADP) {
					tmp_val = 0x3;
				} else {
					K3_FB_ERR("layer_idx(%d): not support this ovl_type(%d)!\n",
						layer_idx, k3fd->ovl_type);
					tmp_val = -1;
				}

				if (tmp_val >= 0) {
					k3fd->dss_glb.chn_mux_val[tmp_chn_idx] =
						set_bits32(k3fd->dss_glb.chn_mux_val[tmp_chn_idx], tmp_val, 2, 0);
					k3fd->dss_glb.chn_mux_used[tmp_chn_idx] = 1;
				}
			} else {
				K3_FB_ERR("layer_idx(%d), chn_mux_base is NULL!\n", layer_idx);
			}
		}
		break;

	case DPE3_CHN0:
		{
			/* CHN MUX */
			if (k3fd->dss_glb.chn_mux_base[chn_idx]) {
				/* 0:to ov2 1:to wbe1_dma0 */
				if (pov_req->wb_enable &&
					(pov_req->wb_layer_info.chn_idx == WBE1_CHN0)) {
					k3fd->dss_glb.chn_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.chn_mux_val[chn_idx], 0x1, 1, 0);
					k3fd->dss_glb.chn_mux_used[chn_idx] = 1;
				} else {
					k3fd->dss_glb.chn_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.chn_mux_val[chn_idx], 0x0, 1, 0);
					k3fd->dss_glb.chn_mux_used[chn_idx] = 1;
				}
			} else {
				K3_FB_ERR("layer_idx(%d), chn_mux_base is NULL!\n", layer_idx);
			}
		}
		break;
	case DPE3_CHN1:
		{
			/* ROT mux */
			if (need_cap & CAP_ROT) {
				/* ROT mux */
				if (k3fd->dss_glb.rot_mux_base[chn_idx]) {
					/* 0:DPE3 1:WBE1 */
					k3fd->dss_glb.rot_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.rot_mux_val[chn_idx], 0x0, 1, 0);
					k3fd->dss_glb.rot_mux_used[chn_idx] = 1;
				} else {
					K3_FB_ERR("layer_idx(%d), rot_mux_base is NULL!\n", layer_idx);
				}
			}

			/* SCF mux */
			if (need_cap & CAP_SCL) {
				if (k3fd->dss_glb.scf_mux_base[chn_idx]) {
					/* 1:WBE1, 2:DPE3 */
					k3fd->dss_glb.scf_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.scf_mux_val[chn_idx], 0x2, 2, 0);
					k3fd->dss_glb.scf_mux_used[chn_idx] = 1;
				} else {
					K3_FB_ERR("layer_idx(%d), scf_mux_base is NULL!\n", layer_idx);
				}
			}
		}
		break;
	case DPE3_CHN2:
	case DPE3_CHN3:
		break;

	default:
		K3_FB_ERR("not support this chn(%d)!\n", chn_idx);
		break;
	}

	return 0;
}

int k3_dss_wbe_mux_config(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req, dss_wb_layer_t *layer)
{
	int tmp_val = 0;
	int chn_idx = 0;
	u32 need_cap = 0;
	u8 ovl_type = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);
	BUG_ON(layer == NULL);

	chn_idx = layer->chn_idx;
	need_cap = layer->need_cap;
	ovl_type = k3fd->ovl_type;

	/*
	** handle glb: OVL_MUX
	** OV0_MUX
	** OV1_MUX
	** OV2_MUX
	**
	** handle glb:
	** WBE0_MUX: 0:ov0 offline 1:pdp dfs write back 2:sdp
	** SDP_MUX: 0:dpe1 chn0; 2:sdp dfs write back
	*/
	if (ovl_type == DSS_OVL_PDP) {
		if (k3fd->dss_glb.ovl_mux_base) {
			k3fd->dss_glb.ovl_mux_val =
				set_bits32(k3fd->dss_glb.ovl_mux_val, (0x1 | (0x1 << 2)), 4, 0);
			k3fd->dss_glb.ovl_mux_used = 1;
		}

		if (k3fd->dss_glb.chn_mux_base[chn_idx]) {
			if (pov_req->ovl_flags == DSS_SCENE_WBE0_WO_OFFLINE) {
				tmp_val = 0x0;
			} else if (pov_req->ovl_flags == DSS_SCENE_PDP_ONLINE) {
				tmp_val = 0x1;
			} else {
				tmp_val = 0x1;
			}

			k3fd->dss_glb.chn_mux_val[chn_idx] =
				set_bits32(k3fd->dss_glb.chn_mux_val[chn_idx], tmp_val, 2, 0);
			k3fd->dss_glb.chn_mux_used[chn_idx] = 1;
		} else {
			K3_FB_ERR("ovl_type(%d), WBE0_MUX is invalid!\n", ovl_type);
		}
	} else if (ovl_type == DSS_OVL_SDP) {
		if (k3fd->dss_glb.ovl_mux_base) {
			k3fd->dss_glb.ovl_mux_val =
				set_bits32(k3fd->dss_glb.ovl_mux_val, (0x1 | (0x1 << 2)), 3, 0);
			k3fd->dss_glb.ovl_mux_used = 1;
		}

		if (k3fd->dss_glb.chn_mux_base[chn_idx]) {
			k3fd->dss_glb.chn_mux_val[chn_idx] =
				set_bits32(k3fd->dss_glb.chn_mux_val[chn_idx], 0x2, 2, 0);
			k3fd->dss_glb.chn_mux_used[chn_idx] = 1;
		} else {
			K3_FB_ERR("ovl_type(%d), WBE0_MUX is invalid!\n", ovl_type);
		}

		if (k3fd->dss_glb.sdp_mux_base) {
			k3fd->dss_glb.sdp_mux_val =
				set_bits32(k3fd->dss_glb.sdp_mux_val, 0x2, 2, 0);
			k3fd->dss_glb.sdp_mux_used = 1;
		} else {
			K3_FB_ERR("ovl_type(%d), SDP_MUX is invalid!\n", ovl_type);
		}
	} else if (ovl_type == DSS_OVL_ADP) {
		if (k3fd->dss_glb.ovl_mux_base) {
			k3fd->dss_glb.ovl_mux_val =
				set_bits32(k3fd->dss_glb.ovl_mux_val, 0x1, 1, 3);
			k3fd->dss_glb.ovl_mux_used = 1;
		}
	} else {
		K3_FB_ERR("ovl_mux, ovl_type(%d) is invalid!\n", ovl_type);
	}

	/*
	** handle glb: OVL_SCENE
	** DSS_GLB_OV0_SCENE
	** DSS_GLB_OV1_SCENE
	** DSS_GLB_OV2_SCENE
	*/
	if (k3fd->dss_glb.ovl_scene_base) {
		k3fd->dss_glb.ovl_scene_val =
			set_bits32(k3fd->dss_glb.ovl_scene_val, pov_req->ovl_flags, 3, 0);
		k3fd->dss_glb.ovl_scene_used = 1;
	} else {
		K3_FB_ERR("ovl_type(%d), ovl_scene_base is invalid!\n", ovl_type);
	}

	/*
	** handle glb: CHN_CTL and CHN_SCENE
	** WBE0_CH0_CTL WBE0_CH1_CTL
	** WBE1_CH0_CTL WBE1_CH1_CTL
	*/
	if (k3fd->dss_glb.chn_ctl_base[chn_idx]) {
		k3fd->dss_glb.chn_ctl_val[chn_idx] =
			set_bits32(k3fd->dss_glb.chn_ctl_val[chn_idx], 0x1, 1, 0);
		k3fd->dss_glb.chn_ctl_val[chn_idx] =
			set_bits32(k3fd->dss_glb.chn_ctl_val[chn_idx], layer->flags, 3, 4);
		k3fd->dss_glb.chn_ctl_used[chn_idx] = 1;
	} else {
		K3_FB_ERR("chn_idx(%d), chn_ctl_base is NULL!\n", chn_idx);
	}

	switch (chn_idx) {
	case WBE0_CHN0:
		break;
	case WBE0_CHN1:
		{
			/*
			** handle glb: ROT mux
			** 0: DPE2; 1:WBE0
			*/
			if (need_cap & CAP_ROT) {
				if (k3fd->dss_glb.rot_mux_base[chn_idx]) {
					/* 0:DPE3 1:WBE1 */
					k3fd->dss_glb.rot_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.rot_mux_val[chn_idx], 0x1, 1, 0);
					k3fd->dss_glb.rot_mux_used[chn_idx] = 1;
				} else {
					K3_FB_ERR("chn_idx(%d), rot_mux_base is null!\n", chn_idx);
				}
			}

			/*
			** handle glb: SCF mux
			** 1:WBE0, 2:DPE1
			*/
			if (need_cap & CAP_SCL) {
				if (k3fd->dss_glb.scf_mux_base[chn_idx]) {
					/* 1:WBE1, 2:DPE3 */
					k3fd->dss_glb.scf_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.scf_mux_val[chn_idx], 0x1, 2, 0);
					k3fd->dss_glb.scf_mux_used[chn_idx] = 1;
				} else {
					K3_FB_ERR("chn_idx(%d), scf_mux_base is null!\n", chn_idx);
				}
			}
		}
		break;
	case WBE1_CHN0:
		break;
	case WBE1_CHN1:
		{
			/*
			** handle glb: ROT mux
			** 0:DPE3 1:WBE1
			*/
			if (need_cap & CAP_ROT) {
				if (k3fd->dss_glb.rot_mux_base[chn_idx]) {
					/* 0:DPE3 1:WBE1 */
					k3fd->dss_glb.rot_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.rot_mux_val[chn_idx], 0x1, 1, 0);
					k3fd->dss_glb.rot_mux_used[chn_idx] = 1;
				} else {
					K3_FB_ERR("chn_idx(%d), rot_mux_base is null!\n", chn_idx);
				}
			}

			/*
			** handle glb: SCF mux
			** 1:WBE1, 2:DPE3
			*/
			if (need_cap & CAP_SCL) {
				if (k3fd->dss_glb.scf_mux_base[chn_idx]) {
					/* 1:WBE1, 2:DPE3 */
					k3fd->dss_glb.scf_mux_val[chn_idx] =
						set_bits32(k3fd->dss_glb.scf_mux_val[chn_idx], 0x1, 2, 0);
					k3fd->dss_glb.scf_mux_used[chn_idx] = 1;
				} else {
					K3_FB_ERR("chn_idx(%d), scf_mux_base is null!\n", chn_idx);
				}
			}
		}
		break;
	default:
		K3_FB_ERR("not support this chn_idx(%d)!", chn_idx);
		break;
	}

	return 0;
}


/*******************************************************************************
** DSS SBL
*/
static void k3_dss_sbl_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *sbl_base, dss_sbl_t *s_sbl)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(sbl_base == NULL);
	BUG_ON(s_sbl == NULL);

	k3fd->set_reg(k3fd, sbl_base + SBL_BACKLIGHT_L, s_sbl->sbl_backlight_l, 8, 0);
	k3fd->set_reg(k3fd, sbl_base + SBL_BACKLIGHT_H, s_sbl->sbl_backlight_h, 8, 0);
	k3fd->set_reg(k3fd, sbl_base + SBL_AMBIENT_LIGHT_L, s_sbl->sbl_ambient_light_l, 8, 0);
	k3fd->set_reg(k3fd, sbl_base + SBL_AMBIENT_LIGHT_H, s_sbl->sbl_ambient_light_h, 8, 0);
	k3fd->set_reg(k3fd, sbl_base + SBL_CALIBRATION_A_L, s_sbl->sbl_calibration_a_l, 8, 0);
	k3fd->set_reg(k3fd, sbl_base + SBL_CALIBRATION_A_H, s_sbl->sbl_calibration_a_h, 8, 0);
	k3fd->set_reg(k3fd, sbl_base + SBL_CALIBRATION_B_L, s_sbl->sbl_calibration_b_l, 8, 0);
	k3fd->set_reg(k3fd, sbl_base + SBL_CALIBRATION_B_H, s_sbl->sbl_calibration_b_h, 8, 0);
	k3fd->set_reg(k3fd, sbl_base + SBL_CALIBRATION_C_L, s_sbl->sbl_calibration_c_l, 8, 0);
	k3fd->set_reg(k3fd, sbl_base + SBL_CALIBRATION_C_H, s_sbl->sbl_calibration_c_h, 8, 0);
	k3fd->set_reg(k3fd, sbl_base + SBL_CALIBRATION_D_L, s_sbl->sbl_calibration_d_l, 8, 0);
	k3fd->set_reg(k3fd, sbl_base + SBL_CALIBRATION_D_H, s_sbl->sbl_calibration_d_h, 8, 0);
	k3fd->set_reg(k3fd, sbl_base + SBL_ENABLE, s_sbl->sbl_enable, 1, 0);

	k3fd->sbl_enable = (s_sbl->sbl_enable > 0) ? 1 : 0;
}


/*******************************************************************************
** DSS WDMA BRIDGE
*/
static void k3_dss_wdma_bridge_init(char __iomem *wdma_bridge_base,
	dss_wdma_bridge_t *s_wdma_bridge)
{
	BUG_ON(wdma_bridge_base == NULL);
	BUG_ON(s_wdma_bridge == NULL);

	memset(s_wdma_bridge, 0, sizeof(dss_wdma_bridge_t));

	s_wdma_bridge->wbrg_comm_ctl =
		inp32(wdma_bridge_base + WBRG_COMM_CTL);
	s_wdma_bridge->wbrg_ctl0 =
		inp32(wdma_bridge_base + WBRG_CTL0);
	s_wdma_bridge->wbrg_ctl1 =
		inp32(wdma_bridge_base + WBRG_CTL1);
	s_wdma_bridge->wbrg_wo_ch_ctl =
		inp32(wdma_bridge_base + WBRG_WO_CH_CTL);
	s_wdma_bridge->wbrg_ws_ch_ctl =
		inp32(wdma_bridge_base + WBRG_WS_CH_CTL);
}

static void k3_dss_wdma_bridge_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *wdma_bridge_base, dss_wdma_bridge_t *s_wdma_bridge)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(wdma_bridge_base == NULL);
	BUG_ON(s_wdma_bridge == NULL);

	/* FIXME: not reload */
	k3fd->set_reg(k3fd, wdma_bridge_base + WBRG_COMM_CTL,
		s_wdma_bridge->wbrg_comm_ctl, 32, 0);
	/* FIXME: not reload */
	k3fd->set_reg(k3fd, wdma_bridge_base + WBRG_CTL0,
		s_wdma_bridge->wbrg_ctl0, 32, 0);
	/* FIXME: not reload */
	k3fd->set_reg(k3fd, wdma_bridge_base + WBRG_CTL1,
		s_wdma_bridge->wbrg_ctl1, 32, 0);

	k3fd->set_reg(k3fd, wdma_bridge_base + WBRG_WO_CH_CTL,
		s_wdma_bridge->wbrg_wo_ch_ctl , 32, 0);
	k3fd->set_reg(k3fd, wdma_bridge_base + WBRG_WS_CH_CTL,
		s_wdma_bridge->wbrg_ws_ch_ctl, 32, 0);
}

int k3_set_dpe_wdma_bridge(struct k3_fb_data_type *k3fd, int dpe_type, dss_overlay_t *pov_req)
{
	dss_wdma_bridge_t *wdma_bridge = NULL;
	dss_wb_layer_t *wb_layer = NULL;
	int chn_idx = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);
	BUG_ON((dpe_type < 0 || dpe_type >= DSS_ENG_MAX));

	wdma_bridge = &(k3fd->dss_module.wdma_bridge[dpe_type]);
	k3fd->dss_module.wdma_bridge_used[dpe_type] = 1;

	wb_layer = &(pov_req->wb_layer_info);
	chn_idx = wb_layer->chn_idx;

	if ((chn_idx == WBE0_CHN0) || (chn_idx == WBE1_CHN0)) {
		wdma_bridge->wbrg_comm_ctl = set_bits32(wdma_bridge->wbrg_comm_ctl, 0x1, 1, 4);
		wdma_bridge->wbrg_comm_ctl = set_bits32(wdma_bridge->wbrg_comm_ctl, 0x1, 1, 5);
		wdma_bridge->wbrg_ctl0 = set_bits32(wdma_bridge->wbrg_ctl0, 0x1, 1, 0);
		wdma_bridge->wbrg_ctl0 = set_bits32(wdma_bridge->wbrg_ctl0, 0x7, 4, 1);
		wdma_bridge->wbrg_ctl0 = set_bits32(wdma_bridge->wbrg_ctl0, 0x1, 1, 5);
		wdma_bridge->wbrg_ws_ch_ctl = set_bits32(wdma_bridge->wbrg_ws_ch_ctl, AXI_CH0, 1, 0);
		if (wb_layer->transform & K3_FB_TRANSFORM_ROT_90)
			wdma_bridge->wbrg_ws_ch_ctl = set_bits32(wdma_bridge->wbrg_ws_ch_ctl, 0x8, 5, 7);
		else
			wdma_bridge->wbrg_ws_ch_ctl = set_bits32(wdma_bridge->wbrg_ws_ch_ctl, 0x3, 5, 7);
	} else if ((chn_idx == WBE0_CHN1) || (chn_idx == WBE1_CHN1)) {
		wdma_bridge->wbrg_comm_ctl = set_bits32(wdma_bridge->wbrg_comm_ctl, 0x1, 1, 2);
		wdma_bridge->wbrg_comm_ctl = set_bits32(wdma_bridge->wbrg_comm_ctl, 0x1, 1, 3);
		wdma_bridge->wbrg_ctl1 = set_bits32(wdma_bridge->wbrg_ctl1, 0x1, 1, 0);
		wdma_bridge->wbrg_ctl1 = set_bits32(wdma_bridge->wbrg_ctl1, 0x7, 4, 1);
		wdma_bridge->wbrg_ctl1 = set_bits32(wdma_bridge->wbrg_ctl1, 0x1, 1, 5);
		wdma_bridge->wbrg_wo_ch_ctl = set_bits32(wdma_bridge->wbrg_wo_ch_ctl, AXI_CH1, 1, 0);
		if (wb_layer->transform & K3_FB_TRANSFORM_ROT_90)
			wdma_bridge->wbrg_wo_ch_ctl = set_bits32(wdma_bridge->wbrg_wo_ch_ctl, 0x8, 5, 7);
		else
			wdma_bridge->wbrg_wo_ch_ctl = set_bits32(wdma_bridge->wbrg_wo_ch_ctl, 0x3, 5, 7);
	} else {
		K3_FB_ERR("chn_idx=%d is invalid!\n", chn_idx);
		return -EINVAL;
	}

	return 0;
}

int k3_dss_wdma_bridge_config(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req)
{
	BUG_ON(k3fd == NULL);

	if (k3fd->ovl_type == DSS_OVL_PDP) {
		if (k3_set_dpe_wdma_bridge(k3fd, DSS_ENG_WBE0, pov_req)) {
			K3_FB_ERR("k3_set_dpe_wdma_bridge for DSS_WBE0 failed!\n");
			return -EINVAL;
		}
	} else if (k3fd->ovl_type == DSS_OVL_SDP) {
		if (k3_set_dpe_wdma_bridge(k3fd, DSS_ENG_WBE0, pov_req)) {
			K3_FB_ERR("k3_set_dpe_wdma_bridge for DSS_WBE0 failed!\n");
			return -EINVAL;
		}
	} else if (k3fd->ovl_type == DSS_OVL_ADP) {
		if (k3_set_dpe_wdma_bridge(k3fd, DSS_ENG_WBE1, pov_req)) {
			K3_FB_ERR("k3_set_dpe_wdma_bridge for DSS_WBE1 failed!\n");
			return -EINVAL;
		}
	} else {
		K3_FB_ERR("not support this ovl_type %d \n", k3fd->ovl_type);
		return -EINVAL;
	}

	return 0;
}


/*******************************************************************************
** DSS WDMA
*/
static u32 k3_calculate_display_addr_wb(bool mmu_enable,
	dss_wb_layer_t *wb_layer, dss_rect_t aligned_rect)
{
	u32 addr = 0;
	u32 dst_addr = 0;
	int left = 0, top = 0, right = 0, bottom = 0;
	int bpp = 0;

	left = aligned_rect.x;
	top = aligned_rect.y;
	right = DSS_WIDTH(left + aligned_rect.w);
	bottom = DSS_HEIGHT(top + aligned_rect.h);

	dst_addr = mmu_enable ? wb_layer->dst.vir_addr : wb_layer->dst.phy_addr;
	bpp = wb_layer->dst.bpp;

	switch(wb_layer->transform) {
	case K3_FB_TRANSFORM_NOP:
		addr = dst_addr + top * wb_layer->dst.stride + left * bpp;
		break;
	case K3_FB_TRANSFORM_FLIP_H:
		addr = dst_addr + top * wb_layer->dst.stride + right * bpp;
		break;
	case K3_FB_TRANSFORM_FLIP_V:
		addr = dst_addr +
			(aligned_rect.h - 1) * wb_layer->dst.stride + left * bpp;
		break;
	case K3_FB_TRANSFORM_ROT_90:
		addr = dst_addr + top * wb_layer->dst.stride + right * bpp;
		break;
	case K3_FB_TRANSFORM_ROT_180:
		addr = dst_addr +
			bottom * wb_layer->dst.stride + right * bpp;
		break;
	case K3_FB_TRANSFORM_ROT_270:
		addr = dst_addr +
			(aligned_rect.h - 1) * wb_layer->dst.stride + left * bpp;
		break;
	case (K3_FB_TRANSFORM_ROT_90 | K3_FB_TRANSFORM_FLIP_H):
		addr = dst_addr +
			bottom * wb_layer->dst.stride + right * bpp;
		break;
	case (K3_FB_TRANSFORM_ROT_90 | K3_FB_TRANSFORM_FLIP_V):
		addr = dst_addr + top * wb_layer->dst.stride + left * bpp;
		break;
	default:
		addr = dst_addr + top * wb_layer->dst.stride + left * bpp;
		break;
	}

	return addr;
}

static void k3_dss_wdma_init(char __iomem *wdma_base, dss_wdma_t *s_wdma)
{
	BUG_ON(wdma_base == NULL);
	BUG_ON(s_wdma == NULL);

	memset(s_wdma, 0, sizeof(dss_wdma_t));

	s_wdma->wdma_data_addr = inp32(wdma_base + WDMA_DATA_ADDR);
	s_wdma->wdma_oft_x0 = inp32(wdma_base + WDMA_OFT_X0);
	s_wdma->wdma_oft_y0 = inp32(wdma_base + WDMA_OFT_Y0);
	s_wdma->wdma_rot_qos_lev = inp32(wdma_base + WDMA_ROT_QOS_LEV);
	s_wdma->wdma_stride = inp32(wdma_base + WDMA_STRIDE);
	s_wdma->wdma_oft_x1 = inp32(wdma_base + WDMA_OFT_X1);
	s_wdma->wdma_oft_y1 = inp32(wdma_base + WDMA_OFT_Y1);
	s_wdma->wdma_ctrl = inp32(wdma_base + WDMA_CTRL);
	s_wdma->wdma_data_num = inp32(wdma_base + WDMA_DATA_NUM);
	s_wdma->wdma_tile_scram = inp32(wdma_base + WDMA_TILE_SCRAM);
	s_wdma->wdma_sw_mask_en = inp32(wdma_base + WDMA_SW_MASK_EN);
	s_wdma->wdma_hdr_start_addr = inp32(wdma_base + WDMA_HDR_START_ADDR);
	s_wdma->wdma_hdr_stride = inp32(wdma_base + WDMA_HDR_STRIDE);
	s_wdma->wdma_start_mask = inp32(wdma_base + WDMA_START_MASK);
	s_wdma->wdma_end_mask = inp32(wdma_base + WDMA_END_MASK);
	s_wdma->wdma_wbrg_credit_ctl = inp32(wdma_base + WDMA_WBRG_CREDIT_CTL);
}

static void k3_dss_wdma_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *wdma_base, dss_wdma_t *s_wdma)
{
	BUG_ON(k3fd == NULL);
	BUG_ON(wdma_base == NULL);
	BUG_ON(s_wdma == NULL);

	k3fd->set_reg(k3fd, wdma_base + WDMA_DATA_ADDR, s_wdma->wdma_data_addr, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_OFT_X0, s_wdma->wdma_oft_x0, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_OFT_Y0, s_wdma->wdma_oft_y0, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_ROT_QOS_LEV, s_wdma->wdma_rot_qos_lev, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_STRIDE, s_wdma->wdma_stride, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_OFT_X1, s_wdma->wdma_oft_x1, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_OFT_Y1, s_wdma->wdma_oft_y1, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_CTRL, s_wdma->wdma_ctrl, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_DATA_NUM, s_wdma->wdma_data_num, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_TILE_SCRAM, s_wdma->wdma_tile_scram, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_SW_MASK_EN, s_wdma->wdma_sw_mask_en, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_HDR_START_ADDR, s_wdma->wdma_hdr_start_addr, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_HDR_STRIDE, s_wdma->wdma_hdr_stride, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_START_MASK, s_wdma->wdma_start_mask, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_END_MASK, s_wdma->wdma_end_mask, 32, 0);
	k3fd->set_reg(k3fd, wdma_base + WDMA_WBRG_CREDIT_CTL, s_wdma->wdma_wbrg_credit_ctl, 32, 0);
}

int k3_dss_wdma_config(struct k3_fb_data_type *k3fd, dss_overlay_t *pov_req, dss_wb_layer_t *layer,
	dss_rect_t aligned_rect, dss_rect_t *block_rect)
{
	dss_wdma_t *wdma = NULL;
	int chn_idx = 0;
	int wdma_format = 0;
	int wdma_transform = 0;
	u32 wdma_oft_x0 = 0;
	u32 wdma_oft_x1 = 0;
	u32 wdma_oft_y0 = 0;
	u32 wdma_oft_y1 = 0;
	u32 wdma_data_num = 0;
	u32 wdma_addr = 0;
	u32 wdma_stride = 0;

	int wbrg_credit_qos_en = 1;
	int wbrg_credit_shaper = 0x20;
	int wbrg_credit_lower = 0x7F;
	int wbrg_credit_upper = 0xFF;

	u32 mask_left = 0;
	u32 mask_right = 0;

	dss_rect_t in_rect;
	int temp = 0;
	int aligned_pixel = 0;
	int l2t_interleave_n = 0;
	bool mmu_enable = false;

	BUG_ON(k3fd == NULL);
	BUG_ON(pov_req == NULL);
	BUG_ON(layer == NULL);

	chn_idx = layer->chn_idx;

	wdma = &(k3fd->dss_module.wdma[chn_idx]);
	k3fd->dss_module.wdma_used[chn_idx] = 1;

	wdma_format = k3_pixel_format_hal2dma(layer->dst.format);
	if (wdma_format < 0) {
		K3_FB_ERR("k3_pixel_format_hal2dma failed!\n");
		return -EINVAL;
	}

	wdma_transform = k3_transform_hal2dma(layer->transform, chn_idx);
	if (wdma_transform < 0) {
		K3_FB_ERR("k3_transform_hal2dma failed!\n");
		return -EINVAL;
	}

	if (layer->dst.is_tile) {
		l2t_interleave_n = k3_get_rdma_tile_interleave(layer->dst.stride);
		if (l2t_interleave_n < MIN_INTERLEAVE) {
			K3_FB_ERR("tile stride should be 256*2^n, error stride:%d!\n", layer->dst.stride);
			return -EINVAL;
		}
	}

	in_rect = aligned_rect;
	aligned_pixel = DMA_ALIGN_BYTES / layer->dst.bpp;

	if (layer->transform & K3_FB_TRANSFORM_ROT_90) {
		temp = in_rect.w;
		in_rect.w = in_rect.h;
		in_rect.h = temp;

		temp = layer->dst_rect.h;
		layer->dst_rect.h = layer->dst_rect.w;
		layer->dst_rect.w = temp;
	}

	mmu_enable = (layer->dst.mmu_enable == 1) ? true : false;
	wdma_addr =  k3_calculate_display_addr_wb(mmu_enable, layer, in_rect) /
		DMA_ALIGN_BYTES;
	wdma_stride = layer->dst.stride / DMA_ALIGN_BYTES;

	wdma_oft_x0 = in_rect.x / aligned_pixel;
	wdma_oft_x1 = DSS_WIDTH(in_rect.x + in_rect.w) / aligned_pixel;
	wdma_oft_y0 = in_rect.y;
	wdma_oft_y1 = DSS_HEIGHT(in_rect.y + in_rect.h);

	wdma_data_num = (wdma_oft_x1 - wdma_oft_x0 + 1) * (wdma_oft_y1- wdma_oft_y0 + 1);

	if (block_rect) {
		if ((aligned_rect.x != block_rect->x) || (aligned_rect.w != block_rect->w)) {
			mask_left = (block_rect->x - aligned_rect.x) * layer->dst.bpp;
			mask_right = (aligned_rect.w - block_rect->w) * layer->dst.bpp - mask_left;
		}
	} else {
		if ((aligned_rect.x != layer->dst_rect.x) || (aligned_rect.w != layer->dst_rect.w)) {
			mask_left = (layer->dst_rect.x - aligned_rect.x) * layer->dst.bpp;
			mask_right = (aligned_rect.w - layer->dst_rect.w) * layer->dst.bpp - mask_left;
		}
	}

	wdma->wdma_data_addr = set_bits32(wdma->wdma_data_addr, wdma_addr, 28, 0);
	wdma->wdma_oft_x0 = set_bits32(wdma->wdma_oft_x0, wdma_oft_x0, 12, 0);
	wdma->wdma_oft_y0 = set_bits32(wdma->wdma_oft_y0, wdma_oft_y0, 16, 0);
	wdma->wdma_rot_qos_lev = set_bits32(wdma->wdma_rot_qos_lev,
		GET_RDMA_ROT_HQOS_REMOVE_LEV(layer->src_rect.w * layer->src_rect.h) |
		(GET_RDMA_ROT_HQOS_ASSERT_LEV(layer->src_rect.w * layer->src_rect.h) << 12), 24, 0);
	wdma->wdma_stride = set_bits32(wdma->wdma_stride, wdma_stride, 13, 0);
	wdma->wdma_oft_x1 = set_bits32(wdma->wdma_oft_x1, wdma_oft_x1, 12, 0);
	wdma->wdma_oft_y1 = set_bits32(wdma->wdma_oft_y1, wdma_oft_y1, 16, 0);
	wdma->wdma_ctrl = set_bits32(wdma->wdma_ctrl, 0x1, 1, 25);
	wdma->wdma_ctrl = set_bits32(wdma->wdma_ctrl, wdma_format, 5, 3);
	wdma->wdma_ctrl = set_bits32(wdma->wdma_ctrl, wdma_transform, 3, 9);
	wdma->wdma_ctrl = set_bits32(wdma->wdma_ctrl, (mmu_enable ? 0x1 : 0x0), 1, 8);
	wdma->wdma_data_num = set_bits32(wdma->wdma_data_num, wdma_data_num, 30, 0);
	if (layer->dst.is_tile) {
		wdma->wdma_ctrl = set_bits32(wdma->wdma_ctrl, 0x1, 1, 1);
		wdma->wdma_ctrl = set_bits32(wdma->wdma_ctrl, l2t_interleave_n, 4, 13);
		wdma->wdma_tile_scram = set_bits32(wdma->wdma_tile_scram, 0x1, 1, 0);
	} else {
		wdma->wdma_ctrl = set_bits32(wdma->wdma_ctrl, 0x0, 1, 1);
		wdma->wdma_ctrl = set_bits32(wdma->wdma_ctrl, 0x0, 4, 13);
		wdma->wdma_tile_scram = set_bits32(wdma->wdma_tile_scram, 0x0, 1, 0);
	}
	if (layer->need_cap & CAP_FBC) {
		wdma->wdma_ctrl = set_bits32(wdma->wdma_ctrl, 0x1, 1, 2);
		wdma->wdma_hdr_start_addr = set_bits32(wdma->wdma_hdr_start_addr,
			pov_req->hdr_start_addr / DMA_ALIGN_BYTES, 28, 0);
		wdma->wdma_hdr_stride = set_bits32(wdma->wdma_hdr_stride,
			pov_req->hdr_stride, 16, 0);
	} else {
		wdma->wdma_ctrl = set_bits32(wdma->wdma_ctrl, 0x0, 1, 2);
		wdma->wdma_hdr_start_addr = set_bits32(wdma->wdma_hdr_start_addr,
			0x0, 28, 0);
		wdma->wdma_hdr_stride = set_bits32(wdma->wdma_hdr_stride,
			0x0, 16, 0);
	}

	if ((mask_left <= 16) && (mask_right <= 16) &&
		(layer->transform == K3_FB_TRANSFORM_NOP)) {
		wdma->wdma_sw_mask_en = set_bits32(wdma->wdma_sw_mask_en,
			0x1, 1, 0);
		wdma->wdma_start_mask = set_bits32(wdma->wdma_start_mask,
			0xFFFFFFFF << mask_left, 32 , 0);
		wdma->wdma_end_mask = set_bits32(wdma->wdma_end_mask,
			0xFFFFFFFF >> mask_right, 32, 0);
	} else if ((mask_left <= 32) && (mask_right <= 32) &&
		(layer->transform & K3_FB_TRANSFORM_ROT_90)) {
		wdma->wdma_sw_mask_en = set_bits32(wdma->wdma_sw_mask_en,
			0x1, 1, 0);
		wdma->wdma_start_mask = set_bits32(wdma->wdma_start_mask,
			0xFFFFFFFF << mask_left, 32 , 0);
		wdma->wdma_end_mask = set_bits32(wdma->wdma_end_mask,
			0xFFFFFFFF >> mask_right, 32, 0);
	} else {
		wdma->wdma_sw_mask_en = set_bits32(wdma->wdma_sw_mask_en,
			0x0, 1, 0);
		wdma->wdma_start_mask = set_bits32(wdma->wdma_start_mask,
			0xFFFFFFFF, 32 , 0);
		wdma->wdma_end_mask = set_bits32(wdma->wdma_end_mask,
			0xFFFFFFFF, 32, 0);
	}
	wdma->wdma_wbrg_credit_ctl = set_bits32(wdma->wdma_wbrg_credit_ctl,
		((wbrg_credit_upper << 16) | (wbrg_credit_lower<< 8) |
		(wbrg_credit_shaper<< 1) | wbrg_credit_qos_en), 24, 0);

	return 0;
}


/*******************************************************************************
** DSS GLOBAL
*/
int k3_dss_module_init(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	memcpy(&(k3fd->dss_glb), &(k3fd->dss_glb_default), sizeof(dss_global_reg_t));
	memcpy(&(k3fd->dss_module), &(k3fd->dss_module_default), sizeof(dss_module_reg_t));

	return 0;
}

int k3_dss_offline_module_default(struct k3_fb_data_type *k3fd)
{
	dss_global_reg_t *ovl_glb = NULL;
	dss_module_reg_t *dss_module = NULL;
	u32 module_base = 0;
	char __iomem *dss_base = NULL;
	u8 ovl_type = 0;
	int i = 0;
	int scf_idx = 0;

	BUG_ON(k3fd == NULL);
	dss_base = k3fd->dss_base;
	BUG_ON(dss_base == NULL);

	ovl_type = k3fd->ovl_type;

	ovl_glb = &(k3fd->dss_glb_default);
	dss_module = &(k3fd->dss_module_default);
	memset(ovl_glb, 0, sizeof(dss_global_reg_t));
	memset(dss_module, 0, sizeof(dss_module_reg_t));

	/**************************************************************************/
	module_base = g_dss_module_ovl_base[ovl_type][MODULE_OVL_MUX];
	if (module_base != 0) {
		ovl_glb->ovl_mux_base = dss_base + module_base;
	}

	module_base = g_dss_module_ovl_base[ovl_type][MODULE_OVL_SCENE];
	if (module_base != 0) {
		ovl_glb->ovl_scene_base = dss_base + module_base;
	}

	for (i = 0; i < ROT_TLB_SCENE_NUM; i++) {
		module_base = DSS_GLB_ROT_TLB0_SCENE + i * 0x4;
		if (module_base != 0) {
			ovl_glb->rot_tlb_scene_base[i] = dss_base + module_base;
		}
	}

	/* DPE2 */
	module_base = DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET + DPE2_SWITCH;
	if (module_base != 0) {
		ovl_glb->dpe2_cross_switch_base = dss_base + module_base;
	}

	if (ovl_type == DSS_OVL_PDP) {
		module_base = DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET + DPE0_SWITCH;
		if (module_base != 0) {
			ovl_glb->dpe0_cross_switch_base = dss_base + module_base;
		}
	} else if (ovl_type == DSS_OVL_SDP) {
		module_base = DSS_DPE1_OFFSET + DSS_DP_CTRL_OFFSET + SDP_MUX;
		if (module_base != 0) {
			ovl_glb->sdp_mux_base = dss_base + module_base;
		}
	} else if (ovl_type == DSS_OVL_ADP) {
		;
	} else {
		K3_FB_ERR("fb%d not support this ovl_type(%d)!", k3fd->index, ovl_type);
	}

	for (i = DPE0_CHN0; i < DSS_CHN_MAX; i++) {
		if (ovl_type == DSS_OVL_ADP) {
			if ((!isChannelBelongtoDPE(DSS_ENG_DPE3, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_DPE2, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_WBE1, i))) {
				continue;
			}
		} else {
			K3_FB_ERR("not support this ovl_type(%d)!", ovl_type);
			break;
		}

		module_base = g_dss_module_base[i][MODULE_GLB_CHN_CTL];
		if (module_base != 0) {
			ovl_glb->chn_ctl_base[i] = dss_base + module_base;
		}

		module_base = g_dss_module_base[i][MODULE_GLB_CHN_MUX];
		if (module_base != 0) {
			ovl_glb->chn_mux_base[i] = dss_base + module_base;
		}

		module_base = g_dss_module_base[i][MODULE_ROT_MUX];
		if (module_base != 0) {
			ovl_glb->rot_mux_base[i] = dss_base + module_base;
		}

		module_base = g_dss_module_base[i][MODULE_SCF_MUX];
		if (module_base != 0) {
			ovl_glb->scf_mux_base[i] = dss_base + module_base;
		}

		module_base = g_dss_module_base[i][MODULE_ROT_PM_CTL];
		if (module_base != 0) {
			ovl_glb->rot_pm_ctl_base[i] = dss_base + module_base;
		}
	}


	/**************************************************************************/
	for (i = 0; i < DSS_ENG_MAX; i++) {
		if (ovl_type == DSS_OVL_ADP) {
			if ((i != DSS_ENG_DPE3) &&
				(i != DSS_ENG_DPE2) &&
				(i != DSS_ENG_WBE1)) {
				continue;
			}
		} else {
			K3_FB_ERR("not support this ovl_type(%d)!", ovl_type);
			break;
		}

		module_base = g_dss_module_eng_base[i][MODULE_ENG_DMA_BRG];
		if (module_base != 0) {
			dss_module->rdma_bridge_base[i] = dss_base + module_base;
		}

		module_base = g_dss_module_eng_base[i][MODULE_ENG_DMA_BRG];
		if (module_base != 0) {
			dss_module->wdma_bridge_base[i] = dss_base + module_base;
		}
	}

	for (i = DPE0_CHN0; i < DSS_CHN_MAX; i++) {
		if (ovl_type == DSS_OVL_ADP) {
			if ((!isChannelBelongtoDPE(DSS_ENG_DPE3, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_DPE2, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_WBE1, i))) {
				continue;
			}
		} else {
			K3_FB_ERR("not support this ovl_type(%d)!", ovl_type);
			break;
		}

		module_base = g_dss_module_base[i][MODULE_MMU_DMA0];
		if (module_base != 0) {
			dss_module->mmu_dma0_base[i] = dss_base + module_base;
		}

		module_base = g_dss_module_base[i][MODULE_MMU_DMA1];
		if (module_base != 0) {
			dss_module->mmu_dma1_base[i] = dss_base + module_base;
		}

		module_base = g_dss_module_base[i][MODULE_DMA0];
		if (module_base != 0) {
			if (i >= DPE0_CHN0 && i <= DPE3_CHN3) {
				dss_module->rdma0_base[i] = dss_base + module_base;
			} else {
				dss_module->wdma_base[i] = dss_base + module_base;
			}
		}

		module_base = g_dss_module_base[i][MODULE_DMA1];
		if (module_base != 0) {
			if (i >= DPE0_CHN0 && i <= DPE3_CHN3) {
				dss_module->rdma1_base[i] = dss_base + module_base;
			}
		}
		module_base = g_dss_module_base[i][MODULE_FBDC];
		if (module_base != 0) {
			dss_module->fbdc_base[i] = dss_base + module_base;
		}

		module_base = g_dss_module_base[i][MODULE_DFC];
		if (module_base != 0) {
			dss_module->dfc_base[i] = dss_base + module_base;
		}

		scf_idx = k3_get_scf_index(i);
		if (scf_idx >= 0) {
			module_base = g_dss_module_base[i][MODULE_SCF];
			if (module_base != 0) {
				dss_module->scf_base[scf_idx] = dss_base + module_base;
			}
		}

		module_base = g_dss_module_base[i][MODULE_SCP];
		if (module_base != 0) {
			dss_module->scp_base[i] = dss_base + module_base;
		}

		module_base = g_dss_module_base[i][MODULE_CSC];
		if (module_base != 0) {
			dss_module->csc_base[i] = dss_base + module_base;
		}

		module_base = g_dss_module_base[i][MODULE_FBC];
		if (module_base != 0) {
			dss_module->fbc_base[i] = dss_base + module_base;
		}
	}

	i = ovl_type;
	module_base = g_dss_module_ovl_base[i][MODULE_OVL_BASE];
	if (module_base != 0) {
		dss_module->ov_base[i] = dss_base + module_base;
	}

	module_base = DSS_GLB2_OFFSET + DSS_MMU_RTLB_OFFSET;
	if (module_base != 0) {
		dss_module->mmu_rptb_base = dss_base + module_base;
	}

	return 0;
}

int k3_dss_module_default(struct k3_fb_data_type *k3fd)
{
	dss_global_reg_t *ovl_glb = NULL;
	dss_module_reg_t *dss_module = NULL;
	u32 module_base = 0;
	char __iomem *dss_base = NULL;
	u8 ovl_type = 0;
	int i = 0;
	int scf_idx = 0;

	BUG_ON(k3fd == NULL);
	dss_base = k3fd->dss_base;
	BUG_ON(dss_base == NULL);

	ovl_type = k3fd->ovl_type;

	ovl_glb = &(k3fd->dss_glb_default);
	dss_module = &(k3fd->dss_module_default);
	memset(ovl_glb, 0, sizeof(dss_global_reg_t));
	memset(dss_module, 0, sizeof(dss_module_reg_t));

	/**************************************************************************/
	module_base = g_dss_module_ovl_base[ovl_type][MODULE_OVL_MUX];
	if (module_base != 0) {
		ovl_glb->ovl_mux_base = dss_base + module_base;
		ovl_glb->ovl_mux_val = inp32(ovl_glb->ovl_mux_base);
	}

	module_base = g_dss_module_ovl_base[ovl_type][MODULE_OVL_SCENE];
	if (module_base != 0) {
		ovl_glb->ovl_scene_base = dss_base + module_base;
		ovl_glb->ovl_scene_val = inp32(ovl_glb->ovl_scene_base);
	}

	for (i = 0; i < ROT_TLB_SCENE_NUM; i++) {
		module_base = DSS_GLB_ROT_TLB0_SCENE + i * 0x4;
		if (module_base != 0) {
			ovl_glb->rot_tlb_scene_base[i] = dss_base + module_base;
			ovl_glb->rot_tlb_scene_val[i] = inp32(ovl_glb->rot_tlb_scene_base[i]);
		}
	}

	/* DPE2 */
	module_base = DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET + DPE2_SWITCH;
	if (module_base != 0) {
		ovl_glb->dpe2_cross_switch_base = dss_base + module_base;
		ovl_glb->dpe2_cross_switch_val = inp32(ovl_glb->dpe2_cross_switch_base);
	}

	if (ovl_type == DSS_OVL_PDP) {
		module_base = DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET + DPE0_SWITCH;
		if (module_base != 0) {
			ovl_glb->dpe0_cross_switch_base = dss_base + module_base;
			ovl_glb->dpe0_cross_switch_val = inp32(ovl_glb->dpe0_cross_switch_base);
		}
	} else if (ovl_type == DSS_OVL_SDP) {
		module_base = DSS_DPE1_OFFSET + DSS_DP_CTRL_OFFSET + SDP_MUX;
		if (module_base != 0) {
			ovl_glb->sdp_mux_base = dss_base + module_base;
			ovl_glb->sdp_mux_val = inp32(ovl_glb->sdp_mux_base);
		}
	} else if (ovl_type == DSS_OVL_ADP) {
		;
	} else {
		K3_FB_ERR("fb%d not support this ovl_type(%d)!", k3fd->index, ovl_type);
	}

	for (i = DPE0_CHN0; i < DSS_CHN_MAX; i++) {
		if (ovl_type == DSS_OVL_PDP) {
			if ((!isChannelBelongtoDPE(DSS_ENG_DPE0, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_DPE2, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_WBE0, i))) {
				continue;
			}
		} else if (ovl_type == DSS_OVL_SDP) {
			if ((!isChannelBelongtoDPE(DSS_ENG_DPE1, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_DPE2, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_WBE0, i))) {
				continue;
			}
		} else if (ovl_type == DSS_OVL_ADP) {
			if ((!isChannelBelongtoDPE(DSS_ENG_DPE3, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_DPE2, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_WBE1, i))) {
				continue;
			}
		} else {
			K3_FB_ERR("not support this ovl_type(%d)!", ovl_type);
			break;
		}

		module_base = g_dss_module_base[i][MODULE_GLB_CHN_CTL];
		if (module_base != 0) {
			ovl_glb->chn_ctl_base[i] = dss_base + module_base;
			ovl_glb->chn_ctl_val[i] = inp32(ovl_glb->chn_ctl_base[i]);
		}

		module_base = g_dss_module_base[i][MODULE_GLB_CHN_MUX];
		if (module_base != 0) {
			ovl_glb->chn_mux_base[i] = dss_base + module_base;
			ovl_glb->chn_mux_val[i] = inp32(ovl_glb->chn_mux_base[i]);
		}

		module_base = g_dss_module_base[i][MODULE_ROT_MUX];
		if (module_base != 0) {
			ovl_glb->rot_mux_base[i] = dss_base + module_base;
			ovl_glb->rot_mux_val[i] = inp32(ovl_glb->rot_mux_base[i]);
		}

		module_base = g_dss_module_base[i][MODULE_SCF_MUX];
		if (module_base != 0) {
			ovl_glb->scf_mux_base[i] = dss_base + module_base;
			ovl_glb->scf_mux_val[i] = inp32(ovl_glb->scf_mux_base[i]);
		}

		module_base = g_dss_module_base[i][MODULE_ROT_PM_CTL];
		if (module_base != 0) {
			ovl_glb->rot_pm_ctl_base[i] = dss_base + module_base;
			ovl_glb->rot_pm_ctl_val[i] = inp32(ovl_glb->rot_pm_ctl_base[i]);
		}
	}


	/**************************************************************************/
	for (i = 0; i < DSS_ENG_MAX; i++) {
		if (ovl_type == DSS_OVL_PDP) {
			if ((i != DSS_ENG_DPE0) &&
				(i != DSS_ENG_DPE2) &&
				(i != DSS_ENG_WBE0)) {
				continue;
			}
		} else if (ovl_type == DSS_OVL_SDP) {
			if ((i != DSS_ENG_DPE1) &&
				(i != DSS_ENG_DPE2) &&
				(i != DSS_ENG_WBE0)) {
				continue;
			}
		} else if (ovl_type == DSS_OVL_ADP) {
			if ((i != DSS_ENG_DPE3) &&
				(i != DSS_ENG_DPE2) &&
				(i != DSS_ENG_WBE1)) {
				continue;
			}
		} else {
			K3_FB_ERR("not support this ovl_type(%d)!", ovl_type);
			break;
		}

		module_base = g_dss_module_eng_base[i][MODULE_ENG_DMA_BRG];
		if (module_base != 0) {
			dss_module->rdma_bridge_base[i] = dss_base + module_base;
			k3_dss_rdma_bridge_init(dss_module->rdma_bridge_base[i],
				&(dss_module->rdma_bridge[i]));
		}

		module_base = g_dss_module_eng_base[i][MODULE_ENG_DMA_BRG];
		if (module_base != 0) {
			dss_module->wdma_bridge_base[i] = dss_base + module_base;
			k3_dss_wdma_bridge_init(dss_module->wdma_bridge_base[i],
				&(dss_module->wdma_bridge[i]));
		}
	}

	for (i = DPE0_CHN0; i < DSS_CHN_MAX; i++) {
		if (ovl_type == DSS_OVL_PDP) {
			if ((!isChannelBelongtoDPE(DSS_ENG_DPE0, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_DPE2, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_WBE0, i))) {
				continue;
			}
		} else if (ovl_type == DSS_OVL_SDP) {
			if ((!isChannelBelongtoDPE(DSS_ENG_DPE1, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_DPE2, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_WBE0, i))) {
				continue;
			}
		} else if (ovl_type == DSS_OVL_ADP) {
			if ((!isChannelBelongtoDPE(DSS_ENG_DPE3, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_DPE2, i)) &&
				(!isChannelBelongtoDPE(DSS_ENG_WBE1, i))) {
				continue;
			}
		} else {
			K3_FB_ERR("not support this ovl_type(%d)!", ovl_type);
			break;
		}

		module_base = g_dss_module_base[i][MODULE_MMU_DMA0];
		if (module_base != 0) {
			dss_module->mmu_dma0_base[i] = dss_base + module_base;
			k3_dss_mmu_dma_init(dss_module->mmu_dma0_base[i], &(dss_module->mmu_dma0[i]));
		}

		module_base = g_dss_module_base[i][MODULE_MMU_DMA1];
		if (module_base != 0) {
			dss_module->mmu_dma1_base[i] = dss_base + module_base;
			k3_dss_mmu_dma_init(dss_module->mmu_dma1_base[i], &(dss_module->mmu_dma1[i]));
		}

		module_base = g_dss_module_base[i][MODULE_DMA0];
		if (module_base != 0) {
			if (i >= DPE0_CHN0 && i <= DPE3_CHN3) {
				dss_module->rdma0_base[i] = dss_base + module_base;
				k3_dss_rdma_init(dss_module->rdma0_base[i], &(dss_module->rdma0[i]));
			} else {
				dss_module->wdma_base[i] = dss_base + module_base;
				k3_dss_wdma_init(dss_module->wdma_base[i], &(dss_module->wdma[i]));
			}
		}

		module_base = g_dss_module_base[i][MODULE_DMA1];
		if (module_base != 0) {
			if (i >= DPE0_CHN0 && i <= DPE3_CHN3) {
				dss_module->rdma1_base[i] = dss_base + module_base;
				k3_dss_rdma_init(dss_module->rdma1_base[i], &(dss_module->rdma1[i]));
			}
		}
		module_base = g_dss_module_base[i][MODULE_FBDC];
		if (module_base != 0) {
			dss_module->fbdc_base[i] = dss_base + module_base;
			k3_dss_fbdc_init(dss_module->fbdc_base[i], &(dss_module->fbdc[i]));
		}

		module_base = g_dss_module_base[i][MODULE_DFC];
		if (module_base != 0) {
			dss_module->dfc_base[i] = dss_base + module_base;
			k3_dss_dfc_init(dss_module->dfc_base[i], &(dss_module->dfc[i]));
		}

		scf_idx = k3_get_scf_index(i);
		if (scf_idx >= 0) {
			module_base = g_dss_module_base[i][MODULE_SCF];
			if (module_base != 0) {
				dss_module->scf_base[scf_idx] = dss_base + module_base;
				k3_dss_scf_init(dss_module->scf_base[scf_idx], &(dss_module->scf[scf_idx]));
			}
		}

		module_base = g_dss_module_base[i][MODULE_SCP];
		if (module_base != 0) {
			dss_module->scp_base[i] = dss_base + module_base;
			k3_dss_scp_init(dss_module->scp_base[i], &(dss_module->scp[i]));
		}

		module_base = g_dss_module_base[i][MODULE_CSC];
		if (module_base != 0) {
			dss_module->csc_base[i] = dss_base + module_base;
			k3_dss_csc_init(dss_module->csc_base[i], &(dss_module->csc[i]));
		}

		module_base = g_dss_module_base[i][MODULE_FBC];
		if (module_base != 0) {
			dss_module->fbc_base[i] = dss_base + module_base;
			k3_dss_fbc_init(dss_module->fbc_base[i], &(dss_module->fbc[i]));
		}
	}

	i = ovl_type;
	module_base = g_dss_module_ovl_base[i][MODULE_OVL_BASE];
	if (module_base != 0) {
		dss_module->ov_base[i] = dss_base + module_base;
		k3_dss_ovl_init(dss_module->ov_base[i], &(dss_module->ov[i]));
	}

	module_base = DSS_GLB2_OFFSET + DSS_MMU_RTLB_OFFSET;
	if (module_base != 0) {
		dss_module->mmu_rptb_base = dss_base + module_base;
		k3_dss_mmu_rptb_init(dss_module->mmu_rptb_base, &(dss_module->mmu_rptb));
	}

	return 0;
}

int k3_dss_module_config(struct k3_fb_data_type *k3fd)
{
	dss_module_reg_t *dss_module = NULL;
	dss_global_reg_t *ovl_glb = NULL;
	u8 ovl_type = 0;
	int i = 0;

	BUG_ON(k3fd == NULL);

	ovl_type = k3fd->ovl_type;
	ovl_glb = &(k3fd->dss_glb);
	dss_module = &(k3fd->dss_module);

	/**************************************************************************/
	if (dss_module->dpe2_on_ref > 0) {
		if (k3fd->ovl_type == DSS_OVL_ADP)
			k3fd->set_reg(k3fd, k3fd->dss_base + DSS_GLB_DPE2_CLK_SW, 0x1, 1, 0);
		else
			k3fd->set_reg(k3fd, k3fd->dss_base + DSS_GLB_DPE2_CLK_SW, 0x0, 1, 0);

		dss_module->dpe2_on_ref = 0;
	}

	if (dss_module->rot1_on_ref > 0) {
		if (k3fd->ovl_type == DSS_OVL_ADP)
			k3fd->set_reg(k3fd, k3fd->dss_base + DSS_GLB_ROT1_CLK_SW, 0x1, 1, 0);
		else
			k3fd->set_reg(k3fd, k3fd->dss_base + DSS_GLB_ROT1_CLK_SW, 0x0, 1, 0);

		dss_module->rot1_on_ref = 0;
	}

	/**************************************************************************/
	if (ovl_glb->ovl_mux_used == 1) {
		K3_FB_ASSERT(ovl_glb->ovl_mux_base);
		k3fd->set_reg(k3fd, ovl_glb->ovl_mux_base, ovl_glb->ovl_mux_val, 32, 0);
	}

	if (ovl_glb->ovl_scene_used == 1) {
		K3_FB_ASSERT(ovl_glb->ovl_scene_base);
		k3fd->set_reg(k3fd, ovl_glb->ovl_scene_base, ovl_glb->ovl_scene_val, 32, 0);
	}

	for (i = 0; i < ROT_TLB_SCENE_NUM; i++) {
		if (ovl_glb->rot_tlb_scene_used[i] == 1) {
			K3_FB_ASSERT(ovl_glb->rot_tlb_scene_base[i]);
			k3fd->set_reg(k3fd, ovl_glb->rot_tlb_scene_base[i],
				ovl_glb->rot_tlb_scene_val[i], 32, 0);
		}
	}

	/* DPE2 */
	if (ovl_glb->dpe2_cross_switch_used == 1) {
		K3_FB_ASSERT(ovl_glb->dpe2_cross_switch_base);
		k3fd->set_reg(k3fd, ovl_glb->dpe2_cross_switch_base,
			ovl_glb->dpe2_cross_switch_val, 32, 0);
	}

	if (ovl_type == DSS_OVL_PDP) {
		if (ovl_glb->dpe0_cross_switch_used == 1) {
			K3_FB_ASSERT(ovl_glb->dpe0_cross_switch_base);
			k3fd->set_reg(k3fd, ovl_glb->dpe0_cross_switch_base,
				ovl_glb->dpe0_cross_switch_val, 32, 0);
		}
	} else if (ovl_type == DSS_OVL_SDP) {
		if (ovl_glb->sdp_mux_used == 1) {
			K3_FB_ASSERT(ovl_glb->sdp_mux_base);
			k3fd->set_reg(k3fd, ovl_glb->sdp_mux_base, ovl_glb->sdp_mux_val, 32, 0);
		}
	} else if (ovl_type == DSS_OVL_ADP) {
		;
	} else {
		K3_FB_ERR("fb%d not support this ovl_type(%d)!", k3fd->index, ovl_type);
	}

	for (i = DPE0_CHN0; i < DSS_CHN_MAX; i++) {
		if (ovl_glb->chn_ctl_used[i] == 1) {
			K3_FB_ASSERT(ovl_glb->chn_ctl_base[i]);
			k3fd->set_reg(k3fd, ovl_glb->chn_ctl_base[i], ovl_glb->chn_ctl_val[i], 32, 0);
		}

		if (ovl_glb->chn_mux_used[i] == 1) {
			K3_FB_ASSERT(ovl_glb->chn_mux_base[i]);
			k3fd->set_reg(k3fd, ovl_glb->chn_mux_base[i], ovl_glb->chn_mux_val[i], 32, 0);
		}

		if (ovl_glb->rot_mux_used[i] == 1) {
			K3_FB_ASSERT(ovl_glb->rot_mux_base[i]);
			k3fd->set_reg(k3fd, ovl_glb->rot_mux_base[i], ovl_glb->rot_mux_val[i], 32, 0);
		}

		if (ovl_glb->scf_mux_used[i] == 1) {
			K3_FB_ASSERT(ovl_glb->scf_mux_base[i]);
			k3fd->set_reg(k3fd, ovl_glb->scf_mux_base[i], ovl_glb->scf_mux_val[i], 32, 0);
		}

		if (ovl_glb->rot_pm_ctl_used[i] == 1) {
			K3_FB_ASSERT(ovl_glb->rot_pm_ctl_base[i]);
			k3fd->set_reg(k3fd, ovl_glb->rot_pm_ctl_base[i], ovl_glb->rot_pm_ctl_val[i], 32, 0);
		}
	}

	/**************************************************************************/
	for (i = 0; i < DSS_ENG_MAX; i++) {
		if (dss_module->rdma_bridge_used[i] == 1) {
			K3_FB_ASSERT(dss_module->rdma_bridge_base[i]);
			k3_dss_rdma_bridge_set_reg(k3fd,
				dss_module->rdma_bridge_base[i], &(dss_module->rdma_bridge[i]));
		}

		if (dss_module->wdma_bridge_used[i] == 1) {
			K3_FB_ASSERT(dss_module->wdma_bridge_base[i]);
			k3_dss_wdma_bridge_set_reg(k3fd,
				dss_module->wdma_bridge_base[i], &(dss_module->wdma_bridge[i]));
		}
	}

	for (i = DPE0_CHN0; i < DSS_CHN_MAX; i++) {
		if (dss_module->rdma0_used[i] == 1) {
			K3_FB_ASSERT(dss_module->rdma0_base[i]);
			k3_dss_rdma_set_reg(k3fd, dss_module->rdma0_base[i], &(dss_module->rdma0[i]));
		}

		if (dss_module->rdma1_used[i] == 1) {
			K3_FB_ASSERT(dss_module->rdma1_base[i]);
			k3_dss_rdma_set_reg(k3fd, dss_module->rdma1_base[i], &(dss_module->rdma1[i]));
		}

		if (dss_module->fbdc_used[i] == 1) {
			K3_FB_ASSERT(dss_module->fbdc_base[i]);
			k3_dss_fbdc_set_reg(k3fd, dss_module->fbdc_base[i], &(dss_module->fbdc[i]));
		}

		if (dss_module->dfc_used[i] == 1) {
			K3_FB_ASSERT(dss_module->dfc_base[i]);
			k3_dss_dfc_set_reg(k3fd, dss_module->dfc_base[i], &(dss_module->dfc[i]));
		}

		if (dss_module->scp_used[i] == 1) {
			K3_FB_ASSERT(dss_module->scp_base[i]);
			k3_dss_scp_set_reg(k3fd, dss_module->scp_base[i], &(dss_module->scp[i]));
		}

		if (dss_module->csc_used[i] == 1) {
			K3_FB_ASSERT(dss_module->csc_base[i]);
			k3_dss_csc_set_reg(k3fd, dss_module->csc_base[i], &(dss_module->csc[i]));
		}

		if (dss_module->fbc_used[i] == 1) {
			K3_FB_ASSERT(dss_module->fbc_base[i]);
			k3_dss_fbc_set_reg(k3fd, dss_module->fbc_base[i], &(dss_module->fbc[i]));
		}

		if (dss_module->wdma_used[i] == 1) {
			K3_FB_ASSERT(dss_module->wdma_base[i]);
			k3_dss_wdma_set_reg(k3fd, dss_module->wdma_base[i], &(dss_module->wdma[i]));
		}

		if (dss_module->mmu_dma0_used[i] == 1) {
			K3_FB_ASSERT(dss_module->mmu_dma0_base[i]);
			k3_dss_mmu_dma_set_reg(k3fd, dss_module->mmu_dma0_base[i],
				&(dss_module->mmu_dma0[i]));
		}

		if (dss_module->mmu_dma1_used[i] == 1) {
			K3_FB_ASSERT(dss_module->rdma1_base[i]);
			k3_dss_mmu_dma_set_reg(k3fd, dss_module->mmu_dma1_base[i],
				&(dss_module->mmu_dma1[i]));
		}
	}

	if (dss_module->mmu_rptb_used == 1) {
		K3_FB_ASSERT(dss_module->mmu_rptb_base);
		k3_dss_mmu_rptb_set_reg(k3fd, dss_module->mmu_rptb_base,
			&(dss_module->mmu_rptb));
	}

	for (i = 0; i < DSS_SCF_MAX; i++) {
		if (dss_module->scf_used[i] == 1) {
			K3_FB_ASSERT(dss_module->scf_base[i]);
			k3_dss_scf_set_reg(k3fd, dss_module->scf_base[i], &(dss_module->scf[i]));
		}
	}

	for (i = 0; i < DSS_OVL_MAX; i++) {
		if (dss_module->ov_used[i] == 1) {
			K3_FB_ASSERT(dss_module->ov_base[i]);
			k3_dss_ovl_set_reg(k3fd, dss_module->ov_base[i], &(dss_module->ov[i]));
		}
	}

	/**************************************************************************/
	if ((PRIMARY_PANEL_IDX == k3fd->index) &&
		(is_mipi_cmd_panel(k3fd)) && k3fd->panel_info.sbl_support) {
			k3_dss_sbl_set_reg(k3fd, k3fd->dss_base + DSS_DPP_SBL_OFFSET, &(k3fd->sbl));
	}

	return 0;
}


/*******************************************************************************
**
*/
int k3_overlay_on(struct k3_fb_data_type *k3fd)
{
	char __iomem *dss_base = 0;
	char __iomem *ovl_base = 0;
	u32 ovl_flags = 0;
	int ret = 0;
	dss_global_reg_t *ovl_glb_default = 0;
	dss_module_reg_t *dss_module_default = 0;
	int i = 0;

	BUG_ON(k3fd == NULL);

	dss_base = k3fd->dss_base;
	ovl_glb_default = &(k3fd->dss_glb_default);
	dss_module_default = &(k3fd->dss_module_default);

	k3fd->vactive0_start_flag = 0;
	k3fd->ldi_data_gate_en = 0;

	//k3fd->ov_wb_enabled = 0;
	k3fd->ov_wb_done = 0;

	k3fd->dirty_region_updt.x = 0;
	k3fd->dirty_region_updt.y = 0;
	k3fd->dirty_region_updt.w = k3fd->panel_info.xres;
	k3fd->dirty_region_updt.h = k3fd->panel_info.yres;
	k3fd->dirty_region_updt_enable = 0;

	memset(&(k3fd->sbl), 0, sizeof(dss_sbl_t));
	k3fd->sbl_enable = 0;

	memset(&k3fd->dss_exception, 0, sizeof(dss_exception_t));
	memset(&k3fd->dss_wb_exception, 0, sizeof(dss_exception_t));

	for (i = 0; i < K3_DSS_OFFLINE_MAX_NUM; i++) {
		memset(&(k3fd->dss_rptb_info_prev[i]), 0, sizeof(dss_rptb_info_t));
		memset(&(k3fd->dss_rptb_info_cur[i]), 0, sizeof(dss_rptb_info_t));
		memset(&(k3fd->dss_wb_rptb_info_prev[i]), 0, sizeof(dss_rptb_info_t));
		memset(&(k3fd->dss_wb_rptb_info_cur[i]), 0, sizeof(dss_rptb_info_t));
	}

	if ((k3fd->index == PRIMARY_PANEL_IDX) || (k3fd->index == EXTERNAL_PANEL_IDX)) {
		if (!k3fd->dss_module_resource_initialized) {
			k3_dss_module_default(k3fd);
			k3fd->dss_module_resource_initialized = true;

			if (k3fd->index == PRIMARY_PANEL_IDX) {
				ovl_glb_default->chn_ctl_val[DPE0_CHN0] = 0x0;
				ovl_glb_default->ovl_scene_val = 0x0;
				ovl_glb_default->ovl_mux_val = 0x0;

				dss_module_default->rdma_bridge[DSS_ENG_DPE0] =
					dss_module_default->rdma_bridge[DSS_ENG_DPE2];
				dss_module_default->rdma0[DPE0_CHN0] =
					dss_module_default->rdma0[DPE0_CHN2];
				dss_module_default->dfc[DPE0_CHN0] =
					dss_module_default->dfc[DPE0_CHN2];
				dss_module_default->ov[DSS_OVL_PDP].ovl_layer[0] =
					dss_module_default->ov[DSS_OVL_PDP].ovl_layer[1];
				dss_module_default->ov[DSS_OVL_PDP].ovl_layer_position[0] =
					dss_module_default->ov[DSS_OVL_PDP].ovl_layer_position[1];
			}
		}

		ovl_base = dss_base +
			g_dss_module_ovl_base[k3fd->ovl_type][MODULE_OVL_BASE];

		if (k3fd->index == PRIMARY_PANEL_IDX) {
			ovl_flags = DSS_SCENE_PDP_ONLINE;
			outp32(k3fd->dss_base + DSS_GLB_PDP_S2_CPU_IRQ_CLR, 0xFFFFFFFF);
		} else {
			ovl_flags = DSS_SCENE_SDP_ONLINE;
			outp32(k3fd->dss_base + DSS_GLB_SDP_S2_CPU_IRQ_CLR, 0xFFFFFFFF);
		}

		ret = k3_dss_module_init(k3fd);
		if (ret != 0) {
			K3_FB_ERR("fb%d, failed to k3_dss_module_init! ret = %d\n", k3fd->index, ret);
			return ret;
		}

		k3_dss_config_ok_begin(k3fd);

		k3_dss_handle_prev_ovl_req(k3fd, &(k3fd->ov_req));
		k3_dss_handle_prev_ovl_req_wb(k3fd, &(k3fd->ov_req));

		ret = k3_dss_ovl_base_config(k3fd, NULL, ovl_flags);
		if (ret != 0) {
			K3_FB_ERR("fb%d, faile to k3_dss_ovl_base_config! ret=%d\n", k3fd->index, ret);
			return ret;
		}

		k3fd->dss_module.fbc_used[WBE0_CHN1] = 1;
		k3fd->dss_module.fbc[WBE0_CHN1].fbc_pm_ctrl =
			set_bits32(k3fd->dss_module.fbc[WBE0_CHN1].fbc_pm_ctrl, 0x83AA, 32, 0);

		if (k3fd->index == PRIMARY_PANEL_IDX) {
			k3fd->dss_module.fbdc_used[DPE0_CHN3] = 1;
			k3fd->dss_module.fbdc[DPE0_CHN3].fbdc_pm_ctrl =
				set_bits32(k3fd->dss_module.fbdc[DPE0_CHN3].fbdc_pm_ctrl, 0x601AA, 32, 0);
		}

		ret = k3_dss_module_config(k3fd);
		if (ret != 0) {
			K3_FB_ERR("fb%d, failed to k3_dss_module_config! ret = %d\n", k3fd->index, ret);
			return ret;
		}

		k3_dss_config_ok_end(k3fd);
		single_frame_update(k3fd);

		k3_dss_scf_coef_load(k3fd);

		if (!k3_dss_check_reg_reload_status(k3fd)) {
			mdelay(17);
		}
	}

	return 0;
}

int k3_overlay_off(struct k3_fb_data_type *k3fd)
{
	int ret = 0;
	u32 ovl_flags = 0;
	char __iomem *dss_base = 0;
	int i = 0;
	int chn_idx = 0;
	u32 module_base = 0;

	BUG_ON(k3fd == NULL);

	dss_base = k3fd->dss_base;

	if ((k3fd->index == PRIMARY_PANEL_IDX) || (k3fd->index == EXTERNAL_PANEL_IDX)) {
		k3fb_activate_vsync(k3fd);

		memset(&k3fd->dss_exception, 0, sizeof(dss_exception_t));
		k3fd->dss_exception.underflow_exception = 1;

		if (k3fd->index == PRIMARY_PANEL_IDX) {
			outp32(dss_base + DSS_GLB_PDP_S2_CPU_IRQ_CLR, 0xFFFFFFFF);
			ovl_flags = DSS_SCENE_PDP_ONLINE;
		} else {
			outp32(dss_base + DSS_GLB_SDP_S2_CPU_IRQ_CLR, 0xFFFFFFFF);
			ovl_flags = DSS_SCENE_SDP_ONLINE;
		}

		ret = k3_dss_module_init(k3fd);
		if (ret != 0) {
			K3_FB_ERR("fb%d, failed to k3_dss_module_init! ret = %d\n", k3fd->index, ret);
			return ret;
		}

		k3_dss_config_ok_begin(k3fd);

		k3_dss_handle_prev_ovl_req(k3fd, &(k3fd->ov_req));
		k3_dss_handle_prev_ovl_req_wb(k3fd, &(k3fd->ov_req));

		ret = k3_dss_ovl_base_config(k3fd, NULL, ovl_flags);
		if (ret != 0) {
			K3_FB_ERR("fb%d, faile to k3_dss_ovl_base_config! ret=%d\n", k3fd->index, ret);
			return ret;
		}

		ret = k3_dss_module_config(k3fd);
		if (ret != 0) {
			K3_FB_ERR("fb%d, failed to k3_dss_module_config! ret = %d\n", k3fd->index, ret);
			return ret;
		}

		k3_dss_config_ok_end(k3fd);
		single_frame_update(k3fd);

		if (!k3_dss_check_reg_reload_status(k3fd)) {
			mdelay(17);
		}

		for (i = 0; i < k3fd->ov_req.layer_nums; i++) {
			chn_idx = k3fd->ov_req.layer_infos[i].chn_idx;
			module_base = g_dss_module_base[chn_idx][MODULE_GLB_CHN_CTL];
			if (module_base != 0)
				set_reg(dss_base + module_base, 0x0, 3, 4);
		}

		k3fb_deactivate_vsync(k3fd);

		k3_dss_rptb_handler(k3fd, false, 0);
		k3_dss_rptb_handler(k3fd, true, 0);

		k3_dss_scf_coef_unload(k3fd);
	}

	return 0;
}

bool k3_dss_check_reg_reload_status(struct k3_fb_data_type *k3fd)
{
	u32 tmp = 0;
	u32 offset = 0;
	u32 try_times = 0;
	bool is_ready = false;

	BUG_ON(k3fd == NULL);

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		offset = DSS_GLB_PDP_S2_CPU_IRQ_RAWSTAT;
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		offset = DSS_GLB_SDP_S2_CPU_IRQ_RAWSTAT;
	} else {
		K3_FB_ERR("fb%d, not support!\n", k3fd->index);
		return false;
	}

	/* check reload status */
	is_ready = true;
	try_times= 0;
	tmp = inp32(k3fd->dss_base + offset);
	while ((tmp & BIT(0)) != 0x1) {
		udelay(10);
		if (++try_times > 4000) {
			K3_FB_ERR("fb%d, check reload status failed, PDP_S2_CPU_IRQ_RAWSTAT=0x%x.\n",
				k3fd->index, tmp);
			is_ready= false;
			break;
		}

		tmp = inp32(k3fd->dss_base + offset);
	}

	return is_ready;
}

static int k3_overlay_test(struct k3_fb_data_type *k3fd, unsigned long *argp)
{
	int ret = 0;
	dss_overlay_test_t ov_test;
	dss_reg_t *pdss_reg = NULL;
	int i = 0;

	BUG_ON(k3fd == NULL);

	pdss_reg = (dss_reg_t *)kmalloc(sizeof(dss_reg_t) * K3_FB_DSS_REG_TEST_NUM,
		GFP_ATOMIC);
	if (!pdss_reg) {
		K3_FB_ERR("pdss_reg failed to alloc!\n");
		return -ENOMEM;
	}

	memset(&ov_test, 0, sizeof(dss_overlay_test_t));
	memset(pdss_reg, 0, sizeof(dss_reg_t) * K3_FB_DSS_REG_TEST_NUM);
	ret = copy_from_user(&ov_test, argp, sizeof(ov_test));
	if (ret) {
		K3_FB_ERR("copy_from_user failed!\n");
		goto err_return;
	}

	if ((ov_test.reg_cnt <= 0) || (ov_test.reg_cnt > K3_FB_DSS_REG_TEST_NUM)) {
		K3_FB_ERR("dss_reg_cnt(%d) is larger than DSS_REG_TEST_NUM(%d)!\n",
			ov_test.reg_cnt, K3_FB_DSS_REG_TEST_NUM);
		goto err_return;
	}

	if (ov_test.reg_cnt > 0) {
		ret = copy_from_user(pdss_reg, ov_test.reg,
			ov_test.reg_cnt * sizeof(dss_reg_t));
		if (ret) {
			K3_FB_ERR("copy_from_user failed!\n");
			goto err_return;
		}
	}

	//K3_FB_DEBUG("dss_reg_cnt=%d\n", ov_test.reg_cnt);
	for (i = 0; i < ov_test.reg_cnt; i++) {
		if (pdss_reg[i].reg_addr) {
			//K3_FB_DEBUG("(0x%x, 0x%x)\n", pdss_reg[i].reg_addr, pdss_reg[i].reg_val);
			outp32((char __iomem *)pdss_reg[i].reg_addr, pdss_reg[i].reg_val);
		}
	}

err_return:
	if (pdss_reg) {
		kfree(pdss_reg);
		pdss_reg = NULL;
	}

	return ret;
}

int k3_overlay_ioctl_handler(struct k3_fb_data_type *k3fd,
	u32 cmd, void __user *argp)
{
	int ret = 0;

	BUG_ON(k3fd == NULL);

	switch (cmd) {
	case K3FB_OV_ONLINE_PLAY:
		if (k3fd->ov_online_play) {
			down(&k3fd->blank_sem);
			ret = k3fd->ov_online_play(k3fd, argp);
			if (ret != 0) {
				K3_FB_ERR("fb%d ov_online_play failed!\n", k3fd->index);
			}
			up(&k3fd->blank_sem);

			if (ret == 0) {
				if (k3fd->bl_update) {
					k3fd->bl_update(k3fd);
				}
			}
		}
		break;
	case K3FB_OV_OFFLINE_PLAY:
		if (k3fd->ov_offline_play) {
			ret = k3fd->ov_offline_play(k3fd, argp);
			if (ret != 0) {
				K3_FB_ERR("fb%d ov_offline_play failed!\n", k3fd->index);
			}
		}
		break;
	case K3FB_OV_ONLINE_WB:
		if (k3fd->ov_online_wb) {
			down(&k3fd->blank_sem);
			ret = k3fd->ov_online_wb(k3fd, argp);
			if (ret != 0) {
				K3_FB_ERR("fb%d ov_online_wb failed!\n", k3fd->index);
			}
			up(&k3fd->blank_sem);
		}
		break;

	case K3FB_ONLINE_WB_CTRL:
		if (k3fd->ov_online_wb_ctrl) {
			down(&k3fd->blank_sem);
			ret = k3fd->ov_online_wb_ctrl(k3fd, argp);
			if (ret != 0) {
				K3_FB_ERR("fb%d ov_online_wb_ctrl failed!\n", k3fd->index);
			}
			up(&k3fd->blank_sem);
		}
		break;

	case K3FB_OV_TEST:
		{
			ret = k3_overlay_test(k3fd, argp);
			if (ret != 0) {
				K3_FB_ERR("fb%d k3_overlay_test failed!\n", k3fd->index);
			}
		}
		break;

	case K3FB_DSS_PURE_LAYER_CHECK:
		{
			dss_pure_layer_check_t check_req;
			int ret = copy_from_user(&check_req, argp, sizeof(check_req));
			if (ret) {
				K3_FB_ERR("copy_from_user failed!\n");
				break;
			}
			ret = k3_dss_ovl_check_pure_layer(k3fd, check_req.layer_index,
				&check_req.alpha_flag, &check_req.color_flag, &check_req.color);

			if (ret == 0)
				ret = copy_to_user(argp, &check_req, sizeof(check_req));
		}
		break;

	default:
		break;
	}

	return ret;
}

int k3_overlay_init(struct k3_fb_data_type *k3fd)
{
	dss_layer_t *layer = NULL;
	dss_overlay_t *pov_req = NULL;
	int i = 0;

	BUG_ON(k3fd == NULL);
	BUG_ON(k3fd->dss_base == NULL);

	k3fd->pan_display_fnc = k3_overlay_pan_display;
	k3fd->ov_ioctl_handler = k3_overlay_ioctl_handler;

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		k3fd->ovl_type = DSS_OVL_PDP;
		k3fd->set_reg = k3fb_set_reg;
		k3fd->ov_online_play = k3_ov_online_play;
		k3fd->ov_offline_play = NULL;
		k3fd->ov_online_wb = k3_ov_online_writeback;
		k3fd->ov_wb_isr_handler = k3_writeback_isr_handler;
		k3fd->ov_vactive0_start_isr_handler = k3_vactive0_start_isr_handler;
		k3fd->ov_optimized = k3_dss_ovl_optimized;
		k3fd->ov_online_wb_ctrl = k3_online_writeback_control;
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		k3fd->ovl_type = DSS_OVL_SDP;
		k3fd->set_reg = k3fb_set_reg;
		k3fd->ov_online_play = k3_ov_online_play;
		k3fd->ov_offline_play = NULL;
		k3fd->ov_online_wb = k3_ov_online_writeback;
		k3fd->ov_wb_isr_handler = k3_writeback_isr_handler;
		k3fd->ov_vactive0_start_isr_handler = k3_vactive0_start_isr_handler;
		k3fd->ov_optimized = k3_dss_ovl_optimized;
		k3fd->ov_online_wb_ctrl = NULL;
	} else if (k3fd->index == AUXILIARY_PANEL_IDX) {
		k3fd->ovl_type = DSS_OVL_ADP;
		k3fd->set_reg = ov_cmd_list_output_32;
		k3fd->ov_online_play = NULL;
		k3fd->ov_offline_play = k3_ov_offline_play;
		k3fd->ov_online_wb = NULL;
		k3fd->ov_wb_isr_handler = k3_writeback_isr_handler;
		k3fd->ov_vactive0_start_isr_handler = NULL;
		k3fd->ov_optimized = NULL;
		k3fd->ov_online_wb_ctrl = NULL;
	} else {
		K3_FB_ERR("fb%d not support this device!\n", k3fd->index);
		return -EINVAL;
	}

	k3fd->dss_module_resource_initialized = false;

	k3fd->vactive0_start_flag = 0;
	init_waitqueue_head(&k3fd->vactive0_start_wq);
	k3fd->ldi_data_gate_en = 0;

	sema_init(&k3fd->ov_wb_sem, 1);
	sema_init(&k3fd->dpe3_ch0_sem, 1);
	sema_init(&k3fd->dpe3_ch1_sem, 1);

	k3fd->ov_wb_enabled = 0;
	k3fd->ov_wb_done = 0;
	init_waitqueue_head(&k3fd->ov_wb_wq);

	memset(&k3fd->dss_exception, 0, sizeof(dss_exception_t));
	memset(&k3fd->dss_wb_exception, 0, sizeof(dss_exception_t));
	memset(&k3fd->ov_req, 0, sizeof(dss_overlay_t));
	memset(&k3fd->ov_wb_req, 0, sizeof(dss_overlay_t));
	memset(&k3fd->dss_glb, 0, sizeof(dss_global_reg_t));
	memset(&k3fd->dss_module, 0, sizeof(dss_module_reg_t));
	memset(&k3fd->dss_glb_default, 0, sizeof(dss_global_reg_t));
	memset(&k3fd->dss_module_default, 0, sizeof(dss_module_reg_t));
	for (i = 0; i < K3_DSS_OFFLINE_MAX_NUM; i++) {
		memset(&(k3fd->dss_rptb_info_prev[i]), 0, sizeof(dss_rptb_info_t));
		memset(&(k3fd->dss_rptb_info_cur[i]), 0, sizeof(dss_rptb_info_t));
		memset(&(k3fd->dss_wb_rptb_info_prev[i]), 0, sizeof(dss_rptb_info_t));
		memset(&(k3fd->dss_wb_rptb_info_cur[i]), 0, sizeof(dss_rptb_info_t));
	}

	k3fd->dirty_region_updt.x = 0;
	k3fd->dirty_region_updt.y = 0;
	k3fd->dirty_region_updt.w = k3fd->panel_info.xres;
	k3fd->dirty_region_updt.h = k3fd->panel_info.yres;
	k3fd->dirty_region_updt_enable = 0;

	if (g_rptb_gen_pool == 0) {
		g_rptb_gen_pool = k3_dss_rptb_init();
	}

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		pov_req = &(k3fd->ov_req);
		layer = &(pov_req->layer_infos[0]);
		pov_req->layer_nums = 1;
		layer->src.format = K3_FB_PIXEL_FORMAT_BGRA_8888;
		layer->src.mmu_enable = 1;
		layer->chn_idx = DPE0_CHN0;
	}

	return 0;
}

int k3_overlay_deinit(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	if (g_rptb_gen_pool)
		k3_dss_rptb_deinit(g_rptb_gen_pool);

	return 0;
}

void k3_writeback_isr_handler(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);
	k3fd->ov_wb_done = 1;
	wake_up_interruptible_all(&k3fd->ov_wb_wq);
}

void k3_vactive0_start_isr_handler(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	if (is_mipi_cmd_panel(k3fd)) {
		k3fd->vactive0_start_flag = 1;

	#ifdef CONFIG_FIX_DIRTY_REGION_UPDT
		disable_ldi(k3fd);
	#endif
	} else {
		k3fd->vactive0_start_flag++;
	}
	wake_up_interruptible_all(&k3fd->vactive0_start_wq);
	frame_count++;
}

int k3_vactive0_start_config(struct k3_fb_data_type *k3fd)
{
	int ret = 0;
	u32 prev_vactive0_start = 0;
	u32 isr_s1 = 0;
	u32 isr_s2 = 0;
	u32 isr_s1_mask = 0;
	u32 isr_s2_mask = 0;

	BUG_ON(k3fd == NULL);

	if (is_mipi_cmd_panel(k3fd)) {
		if (k3fd->vactive0_start_flag == 0) {
			ret = wait_event_interruptible_timeout(k3fd->vactive0_start_wq,
				k3fd->vactive0_start_flag, HZ / 5);
			if (ret <= 0) {
				K3_FB_ERR("fb%d, wait_for vactive0_start_flag timeout!ret=%d, vactive0_start_flag=%d\n",
					k3fd->index, ret, k3fd->vactive0_start_flag);

				set_reg(k3fd->dss_base + PDP_LDI_CTRL, 0x0, 1, 2);

				ret = -ETIMEDOUT;
			} else {
				k3fd->ldi_data_gate_en = 1;
				ret = 0;
			}
		} else {
			k3fd->ldi_data_gate_en = 1;
		}
		k3fd->vactive0_start_flag = 0;
	} else {
		prev_vactive0_start = k3fd->vactive0_start_flag;
		ret = wait_event_interruptible_timeout(k3fd->vactive0_start_wq,
			(prev_vactive0_start != k3fd->vactive0_start_flag), HZ / 5);
		if (ret <= 0) {
			K3_FB_ERR("fb%d, wait_for vactive0_start_flag timeout!ret=%d, "
				"prev_vactive0_start=%d, vactive0_start_flag=%d\n",
				k3fd->index, ret, prev_vactive0_start, k3fd->vactive0_start_flag);
			ret = -ETIMEDOUT;
		} else {
			ret = 0;
		}
	}

	if (ret == -ETIMEDOUT) {
		if (k3fd->ovl_type == DSS_OVL_PDP) {
			isr_s1_mask = inp32(k3fd->dss_base + DSS_GLB_PDP_CPU_IRQ_MSK);
			isr_s1 = inp32(k3fd->dss_base + DSS_GLB_PDP_CPU_IRQ);
			isr_s2_mask = inp32(k3fd->dss_base + PDP_LDI_CPU_IRQ_MSK);
			isr_s2 = inp32(k3fd->dss_base + PDP_LDI_CPU_IRQ);
		} else if (k3fd->ovl_type == DSS_OVL_SDP) {
			isr_s1_mask = inp32(k3fd->dss_base + DSS_GLB_SDP_CPU_IRQ_MSK);
			isr_s1 = inp32(k3fd->dss_base + DSS_GLB_SDP_CPU_IRQ);
			isr_s2_mask = inp32(k3fd->dss_base + SDP_LDI_CPU_IRQ_MSK);
			isr_s2 = inp32(k3fd->dss_base + SDP_LDI_CPU_IRQ);
		}

		if(!dsm_client_ocuppy(lcd_dclient)){
			dsm_client_record(lcd_dclient, "fb%d, isr_s1_mask=0x%x, isr_s2_mask=0x%x, \
				isr_s1=0x%x, isr_s2=0x%x.\n", k3fd->index, isr_s1_mask, isr_s2_mask,\
				isr_s1,	isr_s2);
			dsm_client_notify(lcd_dclient, DSM_LCD_TE_TIME_OUT_ERROR_NO);
		}

		K3_FB_ERR("fb%d, isr_s1_mask=0x%x, isr_s2_mask=0x%x, isr_s1=0x%x, isr_s2=0x%x.\n",
			k3fd->index, isr_s1_mask, isr_s2_mask, isr_s1, isr_s2);
	}

	return ret;
}

void k3_dss_dirty_region_updt_config(struct k3_fb_data_type *k3fd)
{
	struct k3_fb_panel_data *pdata = NULL;
	char __iomem *ovl_base = NULL;
	dss_rect_t dirty = {0};

	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (!k3fd->panel_info.dirty_region_updt_support)
		return ;

	if (!g_enable_dirty_region_updt
		|| !k3fd->dirty_region_updt_enable
		|| k3fd->sbl_enable
		|| k3fd->ov_wb_enabled) {
		dirty.x = 0;
		dirty.y = 0;
		dirty.w = k3fd->panel_info.xres;
		dirty.h = k3fd->panel_info.yres;

		k3fd->ldi_data_gate_en = 0;
		set_reg(k3fd->dss_base + PDP_LDI_CTRL, 0x0, 1, 2);
	} else {
		dirty = k3fd->ov_req.dirty_region_updt_info.dirty_rect;
	}

	ovl_base = k3fd->dss_base +
		g_dss_module_ovl_base[k3fd->ovl_type][MODULE_OVL_BASE];
	set_reg(ovl_base + OVL_OV_SIZE,
		(DSS_WIDTH(dirty.w) | DSS_HEIGHT(dirty.h) << 16), 31, 0);

	if (k3fd->ldi_data_gate_en == 1) {
		set_reg(k3fd->dss_base + PDP_LDI_CTRL, 0x1, 1, 2);
	}

	if (g_debug_dirty_region_updt) {
		K3_FB_INFO("dirty_region(%d,%d, %d,%d). ldi_data_gate_en=%d.\n",
			dirty.x, dirty.y, dirty.w, dirty.h, k3fd->ldi_data_gate_en);
	}

	if ((dirty.x == k3fd->dirty_region_updt.x)
		&& (dirty.y == k3fd->dirty_region_updt.y)
		&& (dirty.w == k3fd->dirty_region_updt.w)
		&& (dirty.h == k3fd->dirty_region_updt.h)) {
		return ;
	}

	if (pdata && pdata->set_display_region) {
		pdata->set_display_region(k3fd->pdev, &dirty);
	}

	k3fd->dirty_region_updt = dirty;
}
