/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"

/* define this to use wrapper to control */
#define AUDIO_USING_WRAP_DRIVER
#ifdef AUDIO_USING_WRAP_DRIVER
#include <mach/mt_pmic_wrap.h>
#endif

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

void Ana_Set_Reg(uint32_t offset, uint32_t value, uint32_t mask)
{
	/* set pmic register or analog CONTROL_IFACE_PATH */
	int ret = 0;
#ifdef AUDIO_USING_WRAP_DRIVER
	uint32_t Reg_Value = Ana_Get_Reg(offset);
	Reg_Value &= (~mask);
	Reg_Value |= (value & mask);
	ret = pwrap_write(offset, Reg_Value);
	Reg_Value = Ana_Get_Reg(offset);
	if ((Reg_Value & mask) != (value & mask)) {
		pr_debug("%s offset= 0x%x value = 0x%x mask = 0x%x ret = %d Reg_Value = 0x%x\n",
			__func__, offset, value, mask, ret, Reg_Value);
	}
#endif
}
EXPORT_SYMBOL(Ana_Set_Reg);

void Ana_Enable_Clk(uint32_t mask, bool enable)
{
	/* set pmic register or analog CONTROL_IFACE_PATH */
#ifdef AUDIO_USING_WRAP_DRIVER
	u32 val;
	u16 reg = enable ? TOP_CKPDN_CLR : TOP_CKPDN_SET;
	pwrap_write(reg, mask);
	pwrap_read(TOP_CKPDN, &val);
	if ((val & mask) != (enable ? 0 : mask))
		pr_err("%s: data mismatch: mask=%04X, val=%04X, enable=%d\n",
			__func__, mask, val, enable);
#endif
}
EXPORT_SYMBOL(Ana_Enable_Clk);

uint32_t Ana_Get_Reg(uint32_t offset)
{
	/* get pmic register */
	int ret = 0;
	uint32_t Rdata = 0;
#ifdef AUDIO_USING_WRAP_DRIVER
	ret = pwrap_read(offset, &Rdata);
#endif
	/* PRINTK_ANA_REG ("Ana_Get_Reg offset= 0x%x  Rdata = 0x%x ret = %d\n",offset,Rdata,ret); */
	return Rdata;
}
EXPORT_SYMBOL(Ana_Get_Reg);

void Ana_Log_Print(void)
{
	AudDrv_ANA_Clk_On();
	pr_notice("+Ana_Log_Print\n");
	pr_notice("AFE_UL_DL_CON0	= 0x%x\n", Ana_Get_Reg(AFE_UL_DL_CON0));
	pr_notice("AFE_DL_SRC2_CON0_H	= 0x%x\n", Ana_Get_Reg(AFE_DL_SRC2_CON0_H));
	pr_notice("AFE_DL_SRC2_CON0_L	= 0x%x\n", Ana_Get_Reg(AFE_DL_SRC2_CON0_L));
#ifndef MT8135_AUD_REG
	pr_notice("AFE_DL_SRC2_CON1_H	= 0x%x\n", Ana_Get_Reg(AFE_DL_SRC2_CON1_H));
	pr_notice("AFE_DL_SRC2_CON1_L	= 0x%x\n", Ana_Get_Reg(AFE_DL_SRC2_CON1_L));
#endif
	pr_notice("AFE_DL_SDM_CON0  = 0x%x\n", Ana_Get_Reg(AFE_DL_SDM_CON0));
	pr_notice("AFE_DL_SDM_CON1  = 0x%x\n", Ana_Get_Reg(AFE_DL_SDM_CON1));
	pr_notice("AFE_UL_SRC_CON0_H  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC_CON0_H));
	pr_notice("AFE_UL_SRC_CON0_L  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC_CON0_L));
	pr_notice("AFE_UL_SRC_CON1_H  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC_CON1_H));
	pr_notice("AFE_UL_SRC_CON1_L  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC_CON1_L));
#ifndef MT8135_AUD_REG
	pr_notice("AFE_PREDIS_CON0_H  = 0x%x\n", Ana_Get_Reg(AFE_PREDIS_CON0_H));
	pr_notice("AFE_PREDIS_CON0_L  = 0x%x\n", Ana_Get_Reg(AFE_PREDIS_CON0_L));
	pr_notice("AFE_PREDIS_CON1_H  = 0x%x\n", Ana_Get_Reg(AFE_PREDIS_CON1_H));
	pr_notice("AFE_PREDIS_CON1_L  = 0x%x\n", Ana_Get_Reg(AFE_PREDIS_CON1_L));

	pr_notice("ANA_AFE_I2S_CON1  = 0x%x\n", Ana_Get_Reg(ANA_AFE_I2S_CON1));

	pr_notice("AFE_I2S_FIFO_UL_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_I2S_FIFO_UL_CFG0));
	pr_notice("AFE_I2S_FIFO_DL_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_I2S_FIFO_DL_CFG0));
#endif
	pr_notice("ANA_AFE_TOP_CON0  = 0x%x\n", Ana_Get_Reg(ANA_AFE_TOP_CON0));
	pr_notice("ANA_AUDIO_TOP_CON0	= 0x%x\n", Ana_Get_Reg(ANA_AUDIO_TOP_CON0));

	pr_notice("AFUNC_AUD_CON0	= 0x%x\n", Ana_Get_Reg(AFUNC_AUD_CON0));
	pr_notice("AFUNC_AUD_CON1	= 0x%x\n", Ana_Get_Reg(AFUNC_AUD_CON1));
	pr_notice("AFUNC_AUD_CON2	= 0x%x\n", Ana_Get_Reg(AFUNC_AUD_CON2));
	pr_notice("AFUNC_AUD_CON3	= 0x%x\n", Ana_Get_Reg(AFUNC_AUD_CON3));
	pr_notice("AFUNC_AUD_CON4	= 0x%x\n", Ana_Get_Reg(AFUNC_AUD_CON4));

#ifdef MT8135_AUD_REG
	pr_notice("AFE_PMIC_NEWIF_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG0));
	pr_notice("AFE_PMIC_NEWIF_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG1));
	pr_notice("AFE_PMIC_NEWIF_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG2));
	pr_notice("AFE_PMIC_NEWIF_CFG3  = 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG3));
	pr_notice("AFE_SGEN_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_SGEN_CFG0));
	pr_notice("AFE_SGEN_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_SGEN_CFG1));
#endif

#ifndef MT8135_AUD_REG
	pr_notice("AFE_I2S_FIFO_MON0  = 0x%x\n", Ana_Get_Reg(AFE_I2S_FIFO_MON0));
	pr_notice("AFE_I2S_FIFO_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_I2S_FIFO_CFG0));
#endif

	pr_notice("TOP_CKPDN  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN));
	pr_notice("TOP_CKPDN_SET  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN_SET));
	pr_notice("TOP_CKPDN_CLR  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN_CLR));
	pr_notice("TOP_CKPDN2	= 0x%x\n", Ana_Get_Reg(TOP_CKPDN2));
	pr_notice("TOP_CKCON1	= 0x%x\n", Ana_Get_Reg(TOP_CKCON1));
	pr_notice("SPK_CON0  = 0x%x\n", Ana_Get_Reg(SPK_CON0));
	pr_notice("SPK_CON1  = 0x%x\n", Ana_Get_Reg(SPK_CON1));
	pr_notice("SPK_CON2  = 0x%x\n", Ana_Get_Reg(SPK_CON2));
	pr_notice("SPK_CON3  = 0x%x\n", Ana_Get_Reg(SPK_CON3));
	pr_notice("SPK_CON4  = 0x%x\n", Ana_Get_Reg(SPK_CON4));
	pr_notice("SPK_CON5  = 0x%x\n", Ana_Get_Reg(SPK_CON5));
	pr_notice("SPK_CON6  = 0x%x\n", Ana_Get_Reg(SPK_CON6));
	pr_notice("SPK_CON7  = 0x%x\n", Ana_Get_Reg(SPK_CON7));
	pr_notice("SPK_CON8  = 0x%x\n", Ana_Get_Reg(SPK_CON8));
	pr_notice("SPK_CON9  = 0x%x\n", Ana_Get_Reg(SPK_CON9));
	pr_notice("SPK_CON10  = 0x%x\n", Ana_Get_Reg(SPK_CON10));
	pr_notice("SPK_CON11  = 0x%x\n", Ana_Get_Reg(SPK_CON11));

	pr_notice("AUDDAC_CON0  = 0x%x\n", Ana_Get_Reg(AUDDAC_CON0));
	pr_notice("AUDBUF_CFG0  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG0));
	pr_notice("AUDBUF_CFG1  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG1));
	pr_notice("AUDBUF_CFG2  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG2));
	pr_notice("AUDBUF_CFG3  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG3));
	pr_notice("AUDBUF_CFG4  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG4));

	pr_notice("IBIASDIST_CFG0	= 0x%x\n", Ana_Get_Reg(IBIASDIST_CFG0));
	pr_notice("AUDACCDEPOP_CFG0  = 0x%x\n", Ana_Get_Reg(AUDACCDEPOP_CFG0));
	pr_notice("AUD_IV_CFG0  = 0x%x\n", Ana_Get_Reg(AUD_IV_CFG0));
	pr_notice("AUDCLKGEN_CFG0	= 0x%x\n", Ana_Get_Reg(AUDCLKGEN_CFG0));
	pr_notice("AUDLDO_CFG0  = 0x%x\n", Ana_Get_Reg(AUDLDO_CFG0));
	pr_notice("AUDLDO_CFG1  = 0x%x\n", Ana_Get_Reg(AUDLDO_CFG1));
	pr_notice("AUDNVREGGLB_CFG0  = 0x%x\n", Ana_Get_Reg(AUDNVREGGLB_CFG0));
	pr_notice("AUD_NCP0  = 0x%x\n", Ana_Get_Reg(AUD_NCP0));
	pr_notice("AUDPREAMP_CON0	= 0x%x\n", Ana_Get_Reg(AUDPREAMP_CON0));
	pr_notice("AUDADC_CON0  = 0x%x\n", Ana_Get_Reg(AUDADC_CON0));
	pr_notice("AUDADC_CON1  = 0x%x\n", Ana_Get_Reg(AUDADC_CON1));
	pr_notice("AUDADC_CON2  = 0x%x\n", Ana_Get_Reg(AUDADC_CON2));
	pr_notice("AUDADC_CON3  = 0x%x\n", Ana_Get_Reg(AUDADC_CON3));
	pr_notice("AUDADC_CON4  = 0x%x\n", Ana_Get_Reg(AUDADC_CON4));
	pr_notice("AUDADC_CON5  = 0x%x\n", Ana_Get_Reg(AUDADC_CON5));
	pr_notice("AUDADC_CON6  = 0x%x\n", Ana_Get_Reg(AUDADC_CON6));
	pr_notice("AUDDIGMI_CON0  = 0x%x\n", Ana_Get_Reg(AUDDIGMI_CON0));
	pr_notice("AUDLSBUF_CON0  = 0x%x\n", Ana_Get_Reg(AUDLSBUF_CON0));
	pr_notice("AUDLSBUF_CON1  = 0x%x\n", Ana_Get_Reg(AUDLSBUF_CON1));
	pr_notice("AUDENCSPARE_CON0  = 0x%x\n", Ana_Get_Reg(AUDENCSPARE_CON0));
	pr_notice("AUDENCCLKSQ_CON0  = 0x%x\n", Ana_Get_Reg(AUDENCCLKSQ_CON0));
	pr_notice("AUDPREAMPGAIN_CON0	= 0x%x\n", Ana_Get_Reg(AUDPREAMPGAIN_CON0));
	pr_notice("ZCD_CON0  = 0x%x\n", Ana_Get_Reg(ZCD_CON0));
	pr_notice("ZCD_CON1  = 0x%x\n", Ana_Get_Reg(ZCD_CON1));
	pr_notice("ZCD_CON2  = 0x%x\n", Ana_Get_Reg(ZCD_CON2));
	pr_notice("ZCD_CON3  = 0x%x\n", Ana_Get_Reg(ZCD_CON3));
	pr_notice("ZCD_CON4  = 0x%x\n", Ana_Get_Reg(ZCD_CON4));
	pr_notice("ZCD_CON5  = 0x%x\n", Ana_Get_Reg(ZCD_CON5));
	pr_notice("NCP_CLKDIV_CON0  = 0x%x\n", Ana_Get_Reg(NCP_CLKDIV_CON0));
	pr_notice("NCP_CLKDIV_CON1  = 0x%x\n", Ana_Get_Reg(NCP_CLKDIV_CON1));
	AudDrv_ANA_Clk_Off();
	pr_notice("-Ana_Log_Print\n");
}
EXPORT_SYMBOL(Ana_Log_Print);
