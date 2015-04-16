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

extern struct dsm_client *lcd_dclient;

/*******************************************************************************
** handle isr
*/
irqreturn_t dss_pdp_isr(int irq, void *ptr)
{
	struct k3_fb_data_type *k3fd = NULL;
	u32 isr_s1 = 0;
	u32 isr_s2 = 0;
	u32 isr_s2_dpe0_mmu = 0;
	u32 isr_s2_dpe2_mmu = 0;
	u32 mask = 0;
	u32 isr_s1_cmp = 0;
	u32 vsync_isr_bit = 0;

	k3fd = (struct k3_fb_data_type *)ptr;
	BUG_ON(k3fd == NULL);

	if (g_debug_mmu_error) {
		isr_s1_cmp = (BIT_PDP_LDI0_IRQ_CPU |
			BIT_DPE0_MMU_IRQ_CPU | BIT_DPE2_MMU_IRQ_CPU);
	} else {
		isr_s1_cmp = BIT_PDP_LDI0_IRQ_CPU;
	}

	if (is_mipi_cmd_panel(k3fd)) {
		vsync_isr_bit = BIT_LDI_TE0_PIN_INT;
	} else {
		vsync_isr_bit = BIT_VSYNC_INT;
	}

	isr_s1 = inp32(k3fd->dss_base + DSS_GLB_PDP_CPU_IRQ);
	if (isr_s1 & isr_s1_cmp) {
		isr_s2 = inp32(k3fd->dss_base + PDP_LDI_CPU_IRQ);
		outp32(k3fd->dss_base + PDP_LDI_CPU_IRQ_CLR, ~0);

		if (g_debug_mmu_error) {
			isr_s2_dpe0_mmu = inp32(k3fd->dss_base + DSS_DPE0_OFFSET +
				DSS_DP_CTRL_OFFSET + PDP_DPE0_MMU_CPU_IRQ);
			isr_s2_dpe2_mmu = inp32(k3fd->dss_base + DSS_DPE2_OFFSET +
				DSS_DP_CTRL_OFFSET + PDP_DPE2_MMU_CPU_IRQ);

			outp32(k3fd->dss_base + DSS_DPE0_OFFSET + DSS_DP_CTRL_OFFSET +
				PDP_DPE0_MMU_CPU_IRQ_CLR, ~0);
			outp32(k3fd->dss_base + DSS_DPE2_OFFSET + DSS_DP_CTRL_OFFSET +
				PDP_DPE2_MMU_CPU_IRQ_CLR, ~0);

			if ((isr_s2_dpe0_mmu & 0xFFFFF) || (isr_s2_dpe2_mmu & 0xFFFFF)) {
				/* ldi disable */
				set_reg(k3fd->dss_base + PDP_LDI_CTRL, 0, 1, 0);

				K3_FB_ERR("mmu error: isr_s2_dpe0_mmu=0x%x, isr_s2_dpe2_mmu=0x%x!\n",
					isr_s2_dpe0_mmu, isr_s2_dpe2_mmu);
			}
		}

		if (isr_s2 & BIT_VACTIVE0_START_INT) {
			if (k3fd->ov_vactive0_start_isr_handler)
				k3fd->ov_vactive0_start_isr_handler(k3fd);
		}

		if (isr_s2 & vsync_isr_bit) {
			if (k3fd->vsync_isr_handler) {
				k3fd->vsync_isr_handler(k3fd);
			}

			if (g_enable_ovl_optimized) {
				if (k3fd->ov_optimized)
					k3fd->ov_optimized(k3fd);
			}

			if (k3fd->buf_sync_signal) {
				k3fd->buf_sync_signal(k3fd);
			}
		}

		if (isr_s2 & BIT_BACKLIGHT_INT) {
			if (k3fd->sbl_isr_handler) {
				k3fd->sbl_isr_handler(k3fd);
			}
		}

		if (isr_s2 & BIT_LDI_UNFLOW_INT) {
			if (g_debug_ldi_underflow) {
				/* ldi disable */
				set_reg(k3fd->dss_base + PDP_LDI_CTRL, 0, 1, 0);
			}

			mask = inp32(k3fd->dss_base + PDP_LDI_CPU_IRQ_MSK);
			mask |= BIT_LDI_UNFLOW_INT;
			outp32(k3fd->dss_base + PDP_LDI_CPU_IRQ_MSK, mask);

			if (k3fd->ldi_data_gate_en == 0) {
				k3fd->dss_exception.underflow_exception = 1;
				if (!dsm_client_ocuppy(lcd_dclient)) {
					dsm_client_record(lcd_dclient, "ldi underflow!\n");
					dsm_client_notify(lcd_dclient, DSM_LCD_LDI_UNDERFLOW_ERROR_NO);
				}
				K3_FB_ERR("ldi underflow!\n");
			}
		}
	}

	if (isr_s1 & BIT_WBE0_MMU_IRQ_CPU) {
		isr_s2 = inp32(k3fd->dss_base + DSS_DPE0_OFFSET +
			DSS_DP_CTRL_OFFSET+ PDP_WBE0_CPU_IRQ);
		outp32(k3fd->dss_base + DSS_DPE0_OFFSET +
			DSS_DP_CTRL_OFFSET + PDP_WBE0_CPU_IRQ_CLR, ~0);

		if (isr_s2 & BIT_WBE0_FRAME_END_CPU_CH1) {
			if (k3fd->ov_wb_isr_handler) {
				k3fd->ov_wb_isr_handler(k3fd);
			}
		}
	}

	return IRQ_HANDLED;
}

irqreturn_t dss_sdp_isr(int irq, void *ptr)
{
	struct k3_fb_data_type *k3fd = NULL;
	u32 isr_s1 = 0;
	u32 isr_s2 = 0;
	u32 mask = 0;

	k3fd = (struct k3_fb_data_type *)ptr;
	BUG_ON(k3fd == NULL);

	isr_s1 = inp32(k3fd->dss_base + DSS_GLB_SDP_CPU_IRQ);
	if (isr_s1 & BIT_SDP_LDI1_IRQ_MCU) {
		isr_s2 = inp32(k3fd->dss_base + SDP_LDI_CPU_IRQ);
		outp32(k3fd->dss_base + SDP_LDI_CPU_IRQ_CLR, ~0);

		if (isr_s2 & BIT_VACTIVE0_START_INT) {
			if (k3fd->ov_vactive0_start_isr_handler)
				k3fd->ov_vactive0_start_isr_handler(k3fd);
		}

		if (isr_s2 & BIT_VSYNC_INT) {
			if (k3fd->vsync_isr_handler) {
				k3fd->vsync_isr_handler(k3fd);
			}

			if (g_enable_ovl_optimized) {
				if (k3fd->ov_optimized)
					k3fd->ov_optimized(k3fd);
			}

			if (k3fd->buf_sync_signal) {
				k3fd->buf_sync_signal(k3fd);
			}
		}

		if (isr_s2 & BIT_LDI_UNFLOW_INT) {
			k3fd->dss_exception.underflow_exception = 1;

			mask = inp32(k3fd->dss_base + SDP_LDI_CPU_IRQ_MSK);
			mask |= BIT_LDI_UNFLOW_INT;
			outp32(k3fd->dss_base + SDP_LDI_CPU_IRQ_MSK, mask);

			if (!dsm_client_ocuppy(lcd_dclient)) {
				dsm_client_record(lcd_dclient, "ldi underflow!\n");
				dsm_client_notify(lcd_dclient, DSM_LCD_LDI_UNDERFLOW_ERROR_NO);
			}
			K3_FB_ERR("ldi underflow!\n");
		}
	}

	if (isr_s1 & BIT_WBE0_MMU_IRQ_CPU) {
		isr_s2 = inp32(k3fd->dss_base + DSS_DPE0_OFFSET +
			DSS_DP_CTRL_OFFSET+ SDP_WBE0_CPU_IRQ);
		outp32(k3fd->dss_base + DSS_DPE0_OFFSET +
			DSS_DP_CTRL_OFFSET + SDP_WBE0_CPU_IRQ_CLR, ~0);

		if (isr_s2 & BIT_WBE0_FRAME_END_CPU_CH1) {
			if (k3fd->ov_wb_isr_handler) {
				k3fd->ov_wb_isr_handler(k3fd);
			}
		}
	}

	return IRQ_HANDLED;
}

static void offline_write_back_isr_handler(struct k3_fb_data_type *k3fd, u32 wbe_chn)
{
	dss_cmdlist_node_t * cmdlist_node, *_node_;

	BUG_ON(k3fd == NULL);
	BUG_ON(wbe_chn >= K3_DSS_OFFLINE_MAX_NUM);

	list_for_each_entry_safe(cmdlist_node, _node_, &k3fd->offline_cmdlist_head[wbe_chn], list_node) {
		if(cmdlist_node->list_node_type == e_node_frame
				&& cmdlist_node->list_header->flag.bits.pending == 0x01){

			k3fd->offline_wb_done[wbe_chn] = cmdlist_node->list_node_flag;
			wake_up_interruptible_all(&k3fd->offline_writeback_wq[wbe_chn]);
			break;
		}
	}

	return;
}

irqreturn_t dss_adp_isr(int irq, void *ptr)
{
	struct k3_fb_data_type *k3fd = NULL;
	char __iomem *cmd_list_base = NULL;
	u32 isr_s1 = 0;
	u32 isr_s2 = 0;
	u32 isr_cmdlist = 0;

	k3fd = (struct k3_fb_data_type *)ptr;
	BUG_ON(k3fd == NULL);

	cmd_list_base = k3fd->dss_base + DSS_CMD_LIST_OFFSET;

	isr_s1 = inp32(k3fd->dss_base + DSS_GLB_OFFLINE_CPU_IRQ);
	if (isr_s1 & BIT_OFFLINE_S2_IRQ_CPU_OFF) {
		isr_s2 = inp32(k3fd->dss_base + DSS_GLB_OFFLINE_S2_CPU_IRQ);
		outp32(k3fd->dss_base + DSS_GLB_OFFLINE_S2_CPU_IRQ_CLR, ~0);

		if (isr_s2 & 1) {
			K3_FB_INFO("OFFLINE last cfg ok\n");
		}
	}

	if (isr_s1 & BIT_CMDLIST_IRQ_CPU_OFF) {
		isr_s2 = inp32(cmd_list_base + CMDLIST_INTS);
		if (isr_s2 & BIT_CH4_INTS) {
			isr_cmdlist = inp32(cmd_list_base + CMDLIST_CH4_INTS);

			if(isr_cmdlist & BIT_PENDING_INTS){
				set_reg(cmd_list_base + CMDLIST_CH4_INTC, 0x1, 1, 3);
				offline_write_back_isr_handler(k3fd, WBE1_CHN0 - WBE1_CHN0);

				if (g_debug_ovl_offline_composer == 1)
					K3_FB_INFO("cmd_list CH_4 has irq = 0x%08x \n", isr_cmdlist);
			}
		}

		if (isr_s2 & BIT_CH5_INTS) {
			isr_cmdlist = inp32(cmd_list_base + CMDLIST_CH5_INTS);

			if(isr_cmdlist & BIT_PENDING_INTS){
				set_reg(cmd_list_base + CMDLIST_CH5_INTC, 0x1, 1, 3);
				offline_write_back_isr_handler(k3fd, WBE1_CHN1 - WBE1_CHN0);

				if (g_debug_ovl_offline_composer == 1)
					K3_FB_INFO("cmd_list CH_5 has irq = 0x%08x \n", isr_cmdlist);
			}
		}

		//K3_FB_INFO("cmd_list has irq = 0x%08x \n", isr_s2);

		/* clear cmdlist irq */
		//outp32(cmd_list_base + CMDLIST_CH0_INTC, 0x3f);
		//outp32(cmd_list_base + CMDLIST_CH1_INTC, 0x3f);
		//outp32(cmd_list_base + CMDLIST_CH2_INTC, 0x3f);
		//outp32(cmd_list_base + CMDLIST_CH3_INTC, 0x3f);
		//outp32(cmd_list_base + CMDLIST_CH4_INTC, 0x3f);
		//outp32(cmd_list_base + CMDLIST_CH5_INTC, 0x3f);
	}

	if (isr_s1 & BIT_WBE1_MMU_IRQ_CPU_OFF) {
		isr_s2 = inp32(k3fd->dss_base + DSS_DPE3_OFFSET +
			DSS_DP_CTRL_OFFSET + OFFLINE_WBE1_CPU_IRQ);
		outp32(k3fd->dss_base + DSS_DPE3_OFFSET +
			DSS_DP_CTRL_OFFSET + OFFLINE_WBE1_CPU_IRQ_CLR, ~0);

		if (isr_s2 & BIT_OFFLINE_WBE1_CH0_FRM_END_CPU) {
			if (g_debug_ovl_offline_composer == 1)
				K3_FB_INFO("WBE1 DMA CH0 frame end\n");

		}

		if (isr_s2 & BIT_OFFLINE_WBE1_CH1_FRM_END_CPU) {
			if (g_debug_ovl_offline_composer == 1)
				K3_FB_INFO("WBE1 DMA CH1 frame end\n");

		}
	}

	return IRQ_HANDLED;
}
