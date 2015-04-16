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
#include "k3_dpe_utils.h"
#include "k3_overlay_utils.h"


/*******************************************************************************
**
*/
static int dpe_init(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		enable_clk_pdp(k3fd);

		k3_dss_config_ok_begin(k3fd);
		init_dpp_pdp(k3fd);
		init_dfs_pdp(k3fd);
		init_sbl_pdp(k3fd);
		init_ifbc_pdp(k3fd);
		init_ldi_pdp(k3fd);
		k3_dss_config_ok_end(k3fd);
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		enable_clk_sdp(k3fd);
		k3_dss_config_ok_begin(k3fd);
		init_dfs_sdp(k3fd);
		init_ldi_sdp(k3fd);
		k3_dss_config_ok_end(k3fd);
	} else if (k3fd->index == AUXILIARY_PANEL_IDX) {
		enable_clk_adp(k3fd);
	} else {
		K3_FB_ERR("fb%d, not support this device!\n", k3fd->index);
	}

	return 0;
}

static int dpe_deinit(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		deinit_ldi(k3fd);

		disable_clk_pdp(k3fd);
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		deinit_ldi(k3fd);
		disable_clk_sdp(k3fd);
	} else if (k3fd->index == AUXILIARY_PANEL_IDX) {
		disable_clk_adp(k3fd);
	} else {
		K3_FB_ERR("fb%d, not support this device!\n", k3fd->index);
	}

	return 0;
}

static void dpe_interrupt_unmask(struct k3_fb_data_type *k3fd)
{
	char __iomem *dss_base = 0;
	u32 unmask = 0;

	BUG_ON(k3fd == NULL);

	dss_base = k3fd->dss_base;

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		/* Stage 1  interrupts */
		//unmask = ~0;
		//unmask &= ~(BIT_PDP_LDI0_IRQ_CPU | BIT_WBE0_MMU_IRQ_CPU);
		unmask = 0x0;
		outp32(dss_base + DSS_GLB_PDP_CPU_IRQ_MSK, unmask);

		/*
		** Stage 2 interrupts
		** BIT_LDI_UNFLOW_INT in k3_baselayer_init
		*/
		unmask = ~0;

		if (is_mipi_cmd_panel(k3fd))
			unmask &= ~(BIT_BACKLIGHT_INT | BIT_VACTIVE0_START_INT | BIT_LDI_TE0_PIN_INT);
		else
			unmask &= ~(BIT_BACKLIGHT_INT | BIT_VSYNC_INT | BIT_VACTIVE0_START_INT);
		outp32(dss_base + PDP_LDI_CPU_IRQ_MSK, unmask);

		unmask = ~0;
		unmask &= ~(BIT_WBE0_FRAME_END_CPU_CH1 | BIT_WBE0_FRAME_END_CPU_CH0);
		outp32(dss_base + DSS_DPE0_OFFSET +
			DSS_DP_CTRL_OFFSET + PDP_WBE0_CPU_IRQ_MSK, unmask);
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		/* Stage 1  interrupts */
		//unmask = ~0;
		//unmask &= ~(BIT_SDP_LDI1_IRQ_MCU | BIT_WBE0_MMU_IRQ_CPU);
		unmask = 0x0;
		outp32(dss_base + DSS_GLB_SDP_CPU_IRQ_MSK, unmask);

		/* Stage 2 interrupts */
		unmask = ~0;
		unmask &= ~(BIT_VSYNC_INT | BIT_VACTIVE0_START_INT);
		outp32(dss_base + SDP_LDI_CPU_IRQ_MSK, unmask);

		unmask = ~0;
		unmask &= ~(BIT_WBE0_FRAME_END_CPU_CH1 | BIT_WBE0_FRAME_END_CPU_CH0);
		outp32(dss_base + DSS_DPE1_OFFSET +
			DSS_DP_CTRL_OFFSET + SDP_WBE0_CPU_IRQ_MSK, unmask);
	} else if (k3fd->index == AUXILIARY_PANEL_IDX) {
		/* Stage 1  interrupts */
		//unmask = ~0;
		//unmask &= ~(BIT_OFFLINE_S2_IRQ_CPU_OFF |
		//	BIT_WBE1_MMU_IRQ_CPU_OFF | BIT_WBE0_MMU_IRQ_CPU_OFF |
		//	BIT_CMDLIST_IRQ_CPU_OFF);
		unmask = 0x0;
		outp32(dss_base + DSS_GLB_OFFLINE_CPU_IRQ_MSK, unmask);

		/* Stage 2 interrupts */
		unmask = ~0;
		unmask &= ~(BIT_OFFLINE_WBE1_CH0_FRM_END_CPU |
			BIT_OFFLINE_WBE1_CH1_FRM_END_CPU);
		outp32(dss_base + DSS_DPE3_OFFSET +
			DSS_DP_CTRL_OFFSET + OFFLINE_WBE1_CPU_IRQ_MSK, unmask);

		/* enable adp irq */
		//outp32(dss_base + DSS_GLB_OFFLINE_S2_CPU_IRQ_MSK, 0);
		/* enable cmdlist irq */
		//set_reg(dss_base + DSS_CMD_LIST_OFFSET + CMDLIST_CH4_INTE, 0x3F, 6, 0);
		//set_reg(dss_base + DSS_CMD_LIST_OFFSET + CMDLIST_CH5_INTE, 0x3F, 6, 0);
	} else {
		K3_FB_ERR("fb%d, not support this device!\n", k3fd->index);
	}
}

static void dpe_interrupt_mask(struct k3_fb_data_type *k3fd)
{
	char __iomem *dss_base = 0;
	u32 mask = 0;

	BUG_ON(k3fd == NULL);

	dss_base = k3fd->dss_base;

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		/* Stage 1  interrupts */
		mask = ~0;
		outp32(dss_base + DSS_GLB_PDP_CPU_IRQ_MSK, mask);

		/* Stage 2 interrupts */
		mask = ~0;
		outp32(dss_base + DSS_DPE0_OFFSET +
			DSS_DP_CTRL_OFFSET + PDP_WBE0_CPU_IRQ_MSK, mask);

		mask = ~0;
		outp32(dss_base + PDP_LDI_CPU_IRQ_MSK, mask);
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		/* Stage 1  interrupts */
		mask = ~0;
		outp32(dss_base + DSS_GLB_SDP_CPU_IRQ_MSK, mask);

		/* Stage 2 interrupts */
		mask = ~0;
		outp32(dss_base + DSS_DPE1_OFFSET +
			DSS_DP_CTRL_OFFSET + SDP_WBE0_CPU_IRQ_MSK, mask);

		mask = ~0;
		outp32(dss_base + SDP_LDI_CPU_IRQ_MSK, mask);
	} else if (k3fd->index == AUXILIARY_PANEL_IDX) {
		/* Stage 1  interrupts */
		mask = ~0;
		outp32(dss_base + DSS_GLB_OFFLINE_CPU_IRQ_MSK, mask);

		/* Stage 2 interrupts */
		mask = ~0;
		outp32(dss_base + DSS_DPE3_OFFSET +
			DSS_DP_CTRL_OFFSET + OFFLINE_WBE1_CPU_IRQ_MSK, mask);
	} else {
		K3_FB_ERR("fb%d, not support this device!\n", k3fd->index);
	}
}

static int dpe_irq_enable(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	if (k3fd->dpe_irq)
		enable_irq(k3fd->dpe_irq);

	return 0;
}

static int dpe_irq_disable(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	if (k3fd->dpe_irq)
		disable_irq(k3fd->dpe_irq);

	/*disable_irq_nosync(k3fd->dpe_irq);*/

	return 0;
}

static int dpe_irq_disable_nosync(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	if (k3fd->dpe_irq)
		disable_irq_nosync(k3fd->dpe_irq);

	return 0;
}

int dpe_regulator_enable(struct k3_fb_data_type *k3fd)
{
	int ret = 0;

	BUG_ON(k3fd == NULL);

	ret = regulator_bulk_enable(1, k3fd->dpe_regulator);
	if (ret) {
		K3_FB_ERR("fb%d regulator_enable failed, error=%d!\n", k3fd->index, ret);
		return ret;
	}

	return ret;
}

int dpe_regulator_disable(struct k3_fb_data_type *k3fd)
{
	int ret = 0;

	BUG_ON(k3fd == NULL);

	ret = regulator_bulk_disable(1, k3fd->dpe_regulator);
	if (ret != 0) {
		K3_FB_ERR("fb%d regulator_disable failed, error=%d!\n", k3fd->index, ret);
		return ret;
	}

	return ret;
}

int dpe_clk_enable(struct k3_fb_data_type *k3fd)
{
	int ret = 0;
	struct clk *clk_tmp = NULL;

	BUG_ON(k3fd == NULL);

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		clk_tmp = k3fd->dss_pri_clk;
		if (clk_tmp) {
			ret = clk_prepare(clk_tmp);
			if (ret) {
				K3_FB_ERR("fb%d dss_pri_clk clk_prepare failed, error=%d!\n",
					k3fd->index, ret);
				return -EINVAL;
			}

			ret = clk_enable(clk_tmp);
			if (ret) {
				K3_FB_ERR("fb%d dss_pri_clk clk_enable failed, error=%d!\n",
					k3fd->index, ret);
				return -EINVAL;
			}
		}

		clk_tmp = k3fd->dss_pxl0_clk;
		if (clk_tmp) {
			ret = clk_prepare(clk_tmp);
			if (ret) {
				K3_FB_ERR("fb%d dss_pxl0_clk clk_prepare failed, error=%d!\n",
					k3fd->index, ret);
				return -EINVAL;
			}

			ret = clk_enable(clk_tmp);
			if (ret) {
				K3_FB_ERR("fb%d dss_pxl0_clk clk_enable failed, error=%d!\n",
					k3fd->index, ret);
				return -EINVAL;
			}
		}
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		clk_tmp = k3fd->dss_pxl1_clk;
		if (clk_tmp) {
			ret = clk_prepare(clk_tmp);
			if (ret) {
				K3_FB_ERR("fb%d dss_pxl1_clk clk_prepare failed, error=%d!\n",
					k3fd->index, ret);
				return -EINVAL;
			}

			ret = clk_enable(clk_tmp);
			if (ret) {
				K3_FB_ERR("fb%d dss_pxl1_clk clk_enable failed, error=%d!\n",
					k3fd->index, ret);
				return -EINVAL;
			}
		}
	} else if (k3fd->index == AUXILIARY_PANEL_IDX) {
		clk_tmp = k3fd->dss_aux_clk;
		if (clk_tmp) {
			ret = clk_prepare(clk_tmp);
			if (ret) {
				K3_FB_ERR("fb%d dss_aux_clk clk_prepare failed, error=%d!\n",
					k3fd->index, ret);
				return -EINVAL;
			}

			ret = clk_enable(clk_tmp);
			if (ret) {
				K3_FB_ERR("fb%d dss_aux_clk clk_enable failed, error=%d!\n",
					k3fd->index, ret);
				return -EINVAL;
			}
		}
	} else {
		K3_FB_ERR("fb%d, not support this device!\n", k3fd->index);
		return -EINVAL;
	}

	return 0;
}

int dpe_clk_disable(struct k3_fb_data_type *k3fd)
{
	struct clk *clk_tmp = NULL;

	BUG_ON(k3fd == NULL);

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		clk_tmp = k3fd->dss_pri_clk;
		if (clk_tmp) {
			clk_disable(clk_tmp);
			clk_unprepare(clk_tmp);
		}

		clk_tmp = k3fd->dss_pxl0_clk;
		if (clk_tmp) {
			clk_disable(clk_tmp);
			clk_unprepare(clk_tmp);
		}
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		clk_tmp = k3fd->dss_pxl1_clk;
		if (clk_tmp) {
			clk_disable(clk_tmp);
			clk_unprepare(clk_tmp);
		}
	} else if (k3fd->index == AUXILIARY_PANEL_IDX) {
		clk_tmp = k3fd->dss_aux_clk;
		if (clk_tmp) {
			clk_disable(clk_tmp);
			clk_unprepare(clk_tmp);
		}
	} else {
		K3_FB_ERR("fb%d, not support this device!\n", k3fd->index);
		return -EINVAL;
	}

	return 0;
}


/*******************************************************************************
**
*/
static int dpe_set_fastboot(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	ret = panel_next_set_fastboot(pdev);

	dpe_interrupt_mask(k3fd);
	dpe_irq_enable(k3fd);
	dpe_interrupt_unmask(k3fd);

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return 0;
}

static int dpe_on(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	/* dis reset DSI */
	if (is_mipi_panel(k3fd)) {
		if (is_dual_mipi_panel(k3fd)) {
			outp32(k3fd->crgperi_base + PERRSTDIS3_OFFSET, 0x00030000);
		} else {
			outp32(k3fd->crgperi_base + PERRSTDIS3_OFFSET, 0x00020000);
		}
	}

	if (k3fd->index != PRIMARY_PANEL_IDX) {
		if (k3fd_list[PRIMARY_PANEL_IDX] &&
			(k3fd_list[PRIMARY_PANEL_IDX]->panel_info.vsync_ctrl_type & VSYNC_CTRL_CLK_OFF)) {
			K3_FB_DEBUG("fb%d, pdp clk enable!\n", k3fd->index);
			dpe_clk_enable(k3fd_list[PRIMARY_PANEL_IDX]);
		}
	}

	dpe_regulator_enable(k3fd);

	dpe_init(k3fd);

	if (is_ldi_panel(k3fd)) {
		k3fd->panel_info.lcd_init_step = LCD_INIT_POWER_ON;
		ret = panel_next_on(pdev);
	}

	ret = panel_next_on(pdev);

	if (k3fd->panel_info.vsync_ctrl_type == VSYNC_CTRL_NONE) {
		dpe_irq_enable(k3fd);
		dpe_interrupt_unmask(k3fd);
	}

#if 0
	if (k3fd->index == PRIMARY_PANEL_IDX) {
		enable_ldi_pdp(k3fd);
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		enable_ldi_sdp(k3fd);
	} else if (k3fd->index == AUXILIARY_PANEL_IDX) {
		; /* do nothing */
	} else {
		K3_FB_ERR("fb%d, not support this device!\n", k3fd->index);
	}
#endif

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return ret;
}

static int dpe_off(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	if (k3fd->panel_info.vsync_ctrl_type == VSYNC_CTRL_NONE) {
		dpe_interrupt_mask(k3fd);
		dpe_irq_disable(k3fd);
	} else {
		if (k3fd->vsync_ctrl.vsync_ctrl_enabled == 1) {
			if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_ISR_OFF) {
				dpe_interrupt_mask(k3fd);
				dpe_irq_disable(k3fd);
				K3_FB_INFO("fb%d, need to disable dpe irq! vsync_ctrl_enabled=%d.\n",
					k3fd->index, k3fd->vsync_ctrl.vsync_ctrl_enabled);
			}
		}
	}

	ret = panel_next_off(pdev);

	dpe_deinit(k3fd);

	dpe_regulator_disable(k3fd);

	if (k3fd->index != PRIMARY_PANEL_IDX) {
		if (k3fd_list[PRIMARY_PANEL_IDX] &&
			(k3fd_list[PRIMARY_PANEL_IDX]->panel_info.vsync_ctrl_type & VSYNC_CTRL_CLK_OFF)) {
			K3_FB_DEBUG("fb%d, pdp clk disable!\n", k3fd->index);
			dpe_clk_disable(k3fd_list[PRIMARY_PANEL_IDX]);
		}
	}

	/* reset DSI */
	if (is_mipi_panel(k3fd)) {
		if (is_dual_mipi_panel(k3fd)) {
			outp32(k3fd->crgperi_base + PERRSTEN3_OFFSET, 0x00030000);
		} else {
			outp32(k3fd->crgperi_base + PERRSTEN3_OFFSET, 0x00020000);
		}
	}

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return ret;
}

static int dpe_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	ret = panel_next_remove(pdev);

	if (k3fd->dss_axi_clk) {
		clk_put(k3fd->dss_axi_clk);
		k3fd->dss_axi_clk = NULL;
	}

	if (k3fd->dss_pclk_clk) {
		clk_put(k3fd->dss_pclk_clk);
		k3fd->dss_pclk_clk = NULL;
	}

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		if (k3fd->dss_pri_clk) {
			clk_put(k3fd->dss_pri_clk);
			k3fd->dss_pri_clk = NULL;
		}

		if (k3fd->dss_pxl0_clk) {
			clk_put(k3fd->dss_pxl0_clk);
			k3fd->dss_pxl0_clk = NULL;
		}
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		if (k3fd->dss_pxl1_clk) {
			clk_put(k3fd->dss_pxl1_clk);
			k3fd->dss_pxl1_clk = NULL;
		}
	} else if (k3fd->index == AUXILIARY_PANEL_IDX) {
		if (k3fd->dss_aux_clk) {
			clk_put(k3fd->dss_aux_clk);
			k3fd->dss_aux_clk = NULL;
		}
	} else {
		ret = -1;
		K3_FB_ERR("fb%d, not support this device!\n", k3fd->index);
	}

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return ret;
}

static int dpe_set_backlight(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	ret = panel_next_set_backlight(pdev);

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return ret;
}

static int dpe_sbl_ctrl(struct platform_device *pdev, int enable)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	char __iomem *sbl_base = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	sbl_base = k3fd->dss_base + DSS_DPP_SBL_OFFSET;

	if (k3fd->panel_info.sbl_support == 0) {
		K3_FB_ERR("fb%d not support SBL!\n", k3fd->index);
		return 0;
	}

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	set_reg(sbl_base + SBL_BACKLIGHT_L, k3fd->sbl.sbl_backlight_l, 8, 0);
	set_reg(sbl_base + SBL_BACKLIGHT_H, k3fd->sbl.sbl_backlight_h, 8, 0);
	set_reg(sbl_base + SBL_AMBIENT_LIGHT_L, k3fd->sbl.sbl_ambient_light_l, 8, 0);
	set_reg(sbl_base + SBL_AMBIENT_LIGHT_H, k3fd->sbl.sbl_ambient_light_h, 8, 0);
	set_reg(sbl_base + SBL_CALIBRATION_A_L, k3fd->sbl.sbl_calibration_a_l, 8, 0);
	set_reg(sbl_base + SBL_CALIBRATION_A_H, k3fd->sbl.sbl_calibration_a_h, 8, 0);
	set_reg(sbl_base + SBL_CALIBRATION_B_L, k3fd->sbl.sbl_calibration_b_l, 8, 0);
	set_reg(sbl_base + SBL_CALIBRATION_B_H, k3fd->sbl.sbl_calibration_b_h, 8, 0);
	set_reg(sbl_base + SBL_CALIBRATION_C_L, k3fd->sbl.sbl_calibration_c_l, 8, 0);
	set_reg(sbl_base + SBL_CALIBRATION_C_H, k3fd->sbl.sbl_calibration_c_h, 8, 0);
	set_reg(sbl_base + SBL_CALIBRATION_D_L, k3fd->sbl.sbl_calibration_d_l, 8, 0);
	set_reg(sbl_base + SBL_CALIBRATION_D_H, k3fd->sbl.sbl_calibration_d_h, 8, 0);
	set_reg(sbl_base + SBL_ENABLE, k3fd->sbl.sbl_enable, 1, 0);

	ret = panel_next_sbl_ctrl(pdev, enable);

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return ret;
}

static int dpe_vsync_ctrl(struct platform_device *pdev, int enable)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	if (enable) {
		ret = panel_next_vsync_ctrl(pdev, enable);
		if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_ISR_OFF) {
			dpe_interrupt_unmask(k3fd);
			dpe_irq_enable(k3fd);
		}
	} else {
		ret = panel_next_vsync_ctrl(pdev, enable);
		if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_ISR_OFF) {
			dpe_interrupt_mask(k3fd);
			dpe_irq_disable_nosync(k3fd);
		}
	}

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return ret;
}

static int dpe_frc_handle(struct platform_device *pdev, int fps)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	ret = panel_next_frc_handle(pdev, fps);

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return 0;
}

static int dpe_esd_handle(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	ret = panel_next_esd_handle(pdev);

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return ret;
}

static int dpe_set_display_region(struct platform_device *pdev,
	struct dss_rect *dirty)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	char __iomem *addr = NULL;

	BUG_ON(pdev == NULL || dirty == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("index=%d, enter!\n", k3fd->index);

	addr = k3fd->dss_base + DSS_DPE0_OFFSET + DFS_FRM_SIZE;
	outp32(addr, dirty->w * dirty->h);
	addr = k3fd->dss_base + DSS_DPE0_OFFSET + DFS_FRM_HSIZE;
	outp32(addr, DSS_WIDTH(dirty->w));

	addr = k3fd->dss_base + DSS_DPP_IFBC_OFFSET + IFBC_SIZE;
	outp32(addr, ((DSS_WIDTH(dirty->w) << 16) | DSS_HEIGHT(dirty->h)));

	addr = k3fd->dss_base + PDP_LDI_DPI0_HRZ_CTRL1;
	outp32(addr, DSS_WIDTH(dirty->w) | (DSS_WIDTH(k3fd->panel_info.ldi.h_pulse_width) << 16));
	addr = k3fd->dss_base + PDP_LDI_VRT_CTRL1;
	outp32(addr, DSS_HEIGHT(dirty->h) | (DSS_HEIGHT(k3fd->panel_info.ldi.v_pulse_width) << 16));

	ret = panel_next_set_display_region(pdev, dirty);

	K3_FB_DEBUG("index=%d, exit!\n", k3fd->index);

	return ret;
}

#ifdef CONFIG_LCD_CHECK_REG
static int dpe_lcd_check_reg(struct platform_device *pdev)
{

	BUG_ON(pdev == NULL);

	return panel_next_lcd_check_reg(pdev);
}
#endif

#ifdef CONFIG_LCD_MIPI_DETECT
static int dpe_lcd_mipi_detect(struct platform_device *pdev)
{

	BUG_ON(pdev == NULL);

	return panel_next_lcd_mipi_detect(pdev);
}
#endif

static int dpe_set_timing(struct platform_device *pdev)
{
	char __iomem *dss_base = 0;
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_panel_info *pinfo = NULL;
	struct dss_clk_rate *pdss_clk_rate = NULL;
	int ret = 0;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	if (k3fd->index != 1) {
		K3_FB_ERR("fb%d not support set timing.\n", k3fd->index);
		return -EINVAL;
	}

	if (!k3fd->panel_power_on) {
		K3_FB_ERR("fb%d is not power on.\n", k3fd->index);
		return -EINVAL;
	}

	dss_base = k3fd->dss_base;
	BUG_ON(dss_base == NULL);

	pinfo = &(k3fd->panel_info);
	pdss_clk_rate = get_dss_clk_rate(k3fd);
	BUG_ON(pdss_clk_rate == NULL);

	init_dfs_sdp(k3fd);

	if (IS_ERR(k3fd->dss_pxl1_clk)) {
		ret = PTR_ERR(k3fd->dss_pxl1_clk);
		return ret;
	} else {
		ret = clk_set_rate(k3fd->dss_pxl1_clk, pinfo->pxl_clk_rate);
		if (ret < 0) {
			K3_FB_ERR("fb%d dss_pxl1_clk clk_set_rate(%lu) failed, error=%d!\n",
				k3fd->index, pinfo->pxl_clk_rate, ret);
			return -EINVAL;
		}
		K3_FB_INFO("dss_pxl1_clk:[%lu]->[%lu].\n",
			pinfo->pxl_clk_rate, clk_get_rate(k3fd->dss_pxl1_clk));
	}

	outp32(dss_base + SDP_LDI_HRZ_CTRL0,
		pinfo->ldi.h_front_porch | (pinfo->ldi.h_back_porch << 16));
	outp32(dss_base + SDP_LDI_HRZ_CTRL1,
		DSS_WIDTH(pinfo->xres) | (DSS_WIDTH(pinfo->ldi.h_pulse_width) << 16));
	outp32(dss_base + SDP_LDI_VRT_CTRL0,
		pinfo->ldi.v_front_porch | (pinfo->ldi.v_back_porch << 16));
	outp32(dss_base + SDP_LDI_VRT_CTRL1,
		DSS_HEIGHT(pinfo->yres) | (DSS_HEIGHT(pinfo->ldi.v_pulse_width) << 16));
	outp32(dss_base + SDP_LDI_PLR_CTRL,
		pinfo->ldi.vsync_plr | (pinfo->ldi.hsync_plr << 1) |
		(pinfo->ldi.pixelclk_plr << 2) | (pinfo->ldi.data_en_plr << 3));

	return 0;
}

/*******************************************************************************
**
*/
static int dpe_regulator_clk_irq_setup(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_panel_info *pinfo = NULL;
	struct dss_clk_rate *pdss_clk_rate = NULL;
	const char *regulator_name = NULL;
	const char *irq_name = NULL;
	irqreturn_t (*isr_fnc)(int irq, void *ptr);
	int ret = 0;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	pinfo = &(k3fd->panel_info);
	pdss_clk_rate = get_dss_clk_rate(k3fd);
	BUG_ON(pdss_clk_rate == NULL);

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		regulator_name = REGULATOR_PDP_NAME;
		irq_name = IRQ_PDP_NAME;
		isr_fnc = dss_pdp_isr;
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		regulator_name = REGULATOR_SDP_NAME;
		irq_name = IRQ_SDP_NAME;
		isr_fnc = dss_sdp_isr;
	} else if (k3fd->index == AUXILIARY_PANEL_IDX) {
		regulator_name = REGULATOR_ADP_NAME;
		irq_name = IRQ_ADP_NAME;
		isr_fnc = dss_adp_isr;
	} else {
		K3_FB_ERR("fb%d, not support this device!\n", k3fd->index);
		return -EINVAL;
	}

	k3fd->dss_axi_clk = devm_clk_get(&pdev->dev, k3fd->dss_axi_clk_name);
	if (IS_ERR(k3fd->dss_axi_clk)) {
		ret = PTR_ERR(k3fd->dss_axi_clk);
		return ret;
	}

	k3fd->dss_pclk_clk = devm_clk_get(&pdev->dev, k3fd->dss_pclk_clk_name);
	if (IS_ERR(k3fd->dss_pclk_clk)) {
		ret = PTR_ERR(k3fd->dss_pclk_clk);
		return ret;
	} else {
		ret = clk_set_rate(k3fd->dss_pclk_clk, pdss_clk_rate->dss_pclk_clk_rate);
		if (ret < 0) {
			K3_FB_ERR("fb%d dss_pclk_clk clk_set_rate(%lu) failed, error=%d!\n",
				k3fd->index, pdss_clk_rate->dss_pclk_clk_rate, ret);
			return -EINVAL;
		}

		K3_FB_INFO("dss_pclk_clk:[%lu]->[%lu].\n",
			pdss_clk_rate->dss_pclk_clk_rate, clk_get_rate(k3fd->dss_pclk_clk));
	}

	if (k3fd->index == PRIMARY_PANEL_IDX) {
		k3fd->dss_pri_clk = devm_clk_get(&pdev->dev, k3fd->dss_pri_clk_name);
		if (IS_ERR(k3fd->dss_pri_clk)) {
			ret = PTR_ERR(k3fd->dss_pri_clk);
			return ret;
		} else {
			ret = clk_set_rate(k3fd->dss_pri_clk, pdss_clk_rate->dss_pri_clk_rate);
			if (ret < 0) {
				K3_FB_ERR("fb%d dss_pri_clk clk_set_rate(%lu) failed, error=%d!\n",
					k3fd->index, pdss_clk_rate->dss_pri_clk_rate, ret);
				return -EINVAL;
			}

			K3_FB_INFO("dss_pri_clk:[%lu]->[%lu].\n",
				pdss_clk_rate->dss_pri_clk_rate, clk_get_rate(k3fd->dss_pri_clk));
		}

		k3fd->dss_pxl0_clk = devm_clk_get(&pdev->dev, k3fd->dss_pxl0_clk_name);
		if (IS_ERR(k3fd->dss_pxl0_clk)) {
			ret = PTR_ERR(k3fd->dss_pxl0_clk);
			return ret;
		} else {
			ret = clk_set_rate(k3fd->dss_pxl0_clk, pinfo->pxl_clk_rate);
			if (ret < 0) {
				K3_FB_ERR("fb%d dss_pxl0_clk clk_set_rate(%lu) failed, error=%d!\n",
					k3fd->index, pinfo->pxl_clk_rate, ret);
				return -EINVAL;
			}

			K3_FB_INFO("dss_pxl0_clk:[%lu]->[%lu].\n",
				pinfo->pxl_clk_rate, clk_get_rate(k3fd->dss_pxl0_clk));
		}
	} else if (k3fd->index == EXTERNAL_PANEL_IDX) {
		k3fd->dss_pxl1_clk = devm_clk_get(&pdev->dev, k3fd->dss_pxl1_clk_name);
		if (IS_ERR(k3fd->dss_pxl1_clk)) {
			ret = PTR_ERR(k3fd->dss_pxl1_clk);
			return ret;
		} else {
			ret = clk_set_rate(k3fd->dss_pxl1_clk, pinfo->pxl_clk_rate);
			if (ret < 0) {
				K3_FB_ERR("fb%d dss_pxl1_clk clk_set_rate(%lu) failed, error=%d!\n",
					k3fd->index, pinfo->pxl_clk_rate, ret);
				return -EINVAL;
			}

			K3_FB_INFO("dss_pxl1_clk:[%lu]->[%lu].\n",
				pinfo->pxl_clk_rate, clk_get_rate(k3fd->dss_pxl1_clk));
		}
	} else if (k3fd->index == AUXILIARY_PANEL_IDX) {
		k3fd->dss_aux_clk = devm_clk_get(&pdev->dev, k3fd->dss_aux_clk_name);
		if (IS_ERR(k3fd->dss_aux_clk)) {
			ret = PTR_ERR(k3fd->dss_aux_clk);
			return ret;
		} else {
			ret = clk_set_rate(k3fd->dss_aux_clk, pdss_clk_rate->dss_aux_clk_rate);
			if (ret < 0) {
				K3_FB_ERR("fb%d dss_aux_clk clk_set_rate(%lu) failed, error=%d!\n",
					k3fd->index, pdss_clk_rate->dss_aux_clk_rate, ret);
				return -EINVAL;
			}

			K3_FB_INFO("dss_aux_clk:[%lu]->[%lu].\n",
				pdss_clk_rate->dss_aux_clk_rate, clk_get_rate(k3fd->dss_aux_clk));
		}
	} else {
		ret = -1;
		K3_FB_ERR("fb%d, not support this device!\n", k3fd->index);
	}

	ret = request_irq(k3fd->dpe_irq, isr_fnc, IRQF_DISABLED, irq_name, (void *)k3fd);
	if (ret != 0) {
		K3_FB_ERR("fb%d request_irq failed, irq_no=%d error=%d!\n",
			k3fd->index, k3fd->dpe_irq, ret);
		return ret;
	} else {
		if (k3fd->dpe_irq)
			disable_irq(k3fd->dpe_irq);
	}

	//hisi_irqaffinity_register(k3fd->dpe_irq, 1);

	return 0;
}

static int dpe_probe(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct platform_device *k3_fb_dev = NULL;
	struct k3_fb_panel_data *pdata = NULL;
	struct fb_info *fbi = NULL;
	int ret = 0;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	ret = dpe_regulator_clk_irq_setup(pdev);
	if (ret) {
		K3_FB_ERR("fb%d dpe_irq_clk_setup failed, error=%d!\n", k3fd->index, ret);
		goto err;
	}

	/* alloc device */
	k3_fb_dev = platform_device_alloc(DEV_NAME_FB, pdev->id);
	if (!k3_fb_dev) {
		K3_FB_ERR("fb%d platform_device_alloc failed, error=%d!\n", k3fd->index, ret);
		ret = -ENOMEM;
		goto err_device_alloc;
	}

	/* link to the latest pdev */
	k3fd->pdev = k3_fb_dev;

	/* alloc panel device data */
	ret = platform_device_add_data(k3_fb_dev, dev_get_platdata(&pdev->dev),
		sizeof(struct k3_fb_panel_data));
	if (ret) {
		K3_FB_ERR("fb%d platform_device_add_data failed, error=%d!\n", k3fd->index, ret);
		goto err_device_put;
	}

	/* data chain */
	pdata = dev_get_platdata(&k3_fb_dev->dev);
	pdata->set_fastboot = dpe_set_fastboot;
	pdata->on = dpe_on;
	pdata->off = dpe_off;
	pdata->remove = dpe_remove;
	pdata->set_backlight = dpe_set_backlight;
	pdata->sbl_ctrl = dpe_sbl_ctrl;
	pdata->vsync_ctrl = dpe_vsync_ctrl;
	pdata->frc_handle = dpe_frc_handle;
	pdata->esd_handle = dpe_esd_handle;
	pdata->set_display_region = dpe_set_display_region;
	pdata->set_timing = dpe_set_timing;
#ifdef CONFIG_LCD_CHECK_REG
	pdata->lcd_check_reg = dpe_lcd_check_reg;
#endif
#ifdef CONFIG_LCD_MIPI_DETECT
	pdata->lcd_mipi_detect = dpe_lcd_mipi_detect;
#endif

	pdata->next = pdev;

	/* get/set panel info */
	memcpy(&k3fd->panel_info, pdata->panel_info, sizeof(struct k3_panel_info));

	fbi = k3fd->fbi;
	fbi->var.pixclock = k3fd->panel_info.pxl_clk_rate;
	/*fbi->var.pixclock = clk_round_rate(k3fd->dpe_clk, k3fd->panel_info.pxl_clk_rate);*/
	fbi->var.left_margin = k3fd->panel_info.ldi.h_back_porch;
	fbi->var.right_margin = k3fd->panel_info.ldi.h_front_porch;
	fbi->var.upper_margin = k3fd->panel_info.ldi.v_back_porch;
	fbi->var.lower_margin = k3fd->panel_info.ldi.v_front_porch;
	fbi->var.hsync_len = k3fd->panel_info.ldi.h_pulse_width;
	fbi->var.vsync_len = k3fd->panel_info.ldi.v_pulse_width;

	/* set driver data */
	platform_set_drvdata(k3_fb_dev, k3fd);
	ret = platform_device_add(k3_fb_dev);
	if (ret) {
		K3_FB_ERR("fb%d platform_device_add failed, error=%d!\n", k3fd->index, ret);
		goto err_device_put;
	}

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return 0;

err_device_put:
	platform_device_put(k3_fb_dev);
err_device_alloc:
err:
	return ret;
}

static struct platform_driver this_driver = {
	.probe = dpe_probe,
	.remove = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		.name = DEV_NAME_DSS_DPE,
	},
};

static int __init dpe_driver_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&this_driver);
	if (ret) {
		K3_FB_ERR("platform_driver_register failed, error=%d!\n", ret);
		return ret;
	}

	return ret;
}

module_init(dpe_driver_init);
