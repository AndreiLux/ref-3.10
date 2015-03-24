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
#include "AudDrv_Afe.h"
#include "AudDrv_Clk.h"
#include "mt_soc_afe_control.h"

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

/*****************************************************************************
 *                         FUNCTION DEFINITION
 *****************************************************************************/

/*****************************************************************************
 *                         FUNCTION IMPLEMENTATION
 *****************************************************************************/

void Afe_Set_Reg(uint32_t offset, uint32_t value, uint32_t mask)
{
#ifdef AUDIO_MEM_IOREMAP
	/* PRINTK_AUDDRV("Afe_Set_Reg AUDIO_MEM_IOREMAP AFE_BASE_ADDRESS = %p\n",AFE_BASE_ADDRESS); */
	const uint32_t address = (uint32_t) ((char *)AFE_BASE_ADDRESS + offset);
#else
	const uint32_t address = (AFE_BASE + offset);
#endif
	uint32_t val_tmp;

	/* PRINTK_AFE_REG("AudDrv Afe_Set_Reg offset=%x, value=%x, mask=%x\n",offset,value,mask); */
	val_tmp = Afe_Get_Reg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt65xx_reg_sync_writel(val_tmp, address);
}
EXPORT_SYMBOL(Afe_Set_Reg);

uint32_t Afe_Get_Reg(uint32_t offset)
{
#ifdef AUDIO_MEM_IOREMAP
	/* PRINTK_AUDDRV("Afe_Get_Reg AUDIO_MEM_IOREMAP AFE_BASE_ADDRESS = %p offset = %x\n",
	   AFE_BASE_ADDRESS,offset); */
	const uint32_t address = (uint32_t) ((char *)AFE_BASE_ADDRESS + offset);
#else
	const uint32_t address = (AFE_BASE + offset);
#endif
	return readl((const void __iomem *)address);
}
EXPORT_SYMBOL(Afe_Get_Reg);

void Afe_Log_Print(void)
{
	AudDrv_Clk_On();
	pr_notice("+AudDrv Afe_Log_Print\n");
	pr_notice("AUDIO_TOP_CON0  = 0x%x\n", Afe_Get_Reg(AUDIO_TOP_CON0));
	pr_notice("AUDIO_TOP_CON3  = 0x%x\n", Afe_Get_Reg(AUDIO_TOP_CON3));
	pr_notice("AFE_DAC_CON0  = 0x%x\n", Afe_Get_Reg(AFE_DAC_CON0));
	pr_notice("AFE_DAC_CON1  = 0x%x\n", Afe_Get_Reg(AFE_DAC_CON1));
	pr_notice("AFE_I2S_CON  = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON));
	pr_notice("AFE_DAIBT_CON0  = 0x%x\n", Afe_Get_Reg(AFE_DAIBT_CON0));
	pr_notice("AFE_CONN0  = 0x%x\n", Afe_Get_Reg(AFE_CONN0));
	pr_notice("AFE_CONN1  = 0x%x\n", Afe_Get_Reg(AFE_CONN1));
	pr_notice("AFE_CONN2  = 0x%x\n", Afe_Get_Reg(AFE_CONN2));
	pr_notice("AFE_CONN3  = 0x%x\n", Afe_Get_Reg(AFE_CONN3));
	pr_notice("AFE_CONN4  = 0x%x\n", Afe_Get_Reg(AFE_CONN4));
	pr_notice("AFE_I2S_CON1  = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON1));
	pr_notice("AFE_I2S_CON2  = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON2));
	pr_notice("AFE_MRGIF_CON  = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_CON));

	pr_notice("AFE_DL1_BASE  = 0x%x\n", Afe_Get_Reg(AFE_DL1_BASE));
	pr_notice("AFE_DL1_CUR  = 0x%x\n", Afe_Get_Reg(AFE_DL1_CUR));
	pr_notice("AFE_DL1_END  = 0x%x\n", Afe_Get_Reg(AFE_DL1_END));
	pr_notice("AFE_DL2_BASE  = 0x%x\n", Afe_Get_Reg(AFE_DL2_BASE));
	pr_notice("AFE_DL2_CUR  = 0x%x\n", Afe_Get_Reg(AFE_DL2_CUR));
	pr_notice("AFE_DL2_END  = 0x%x\n", Afe_Get_Reg(AFE_DL2_END));
	pr_notice("AFE_AWB_BASE  = 0x%x\n", Afe_Get_Reg(AFE_AWB_BASE));
	pr_notice("AFE_AWB_END  = 0x%x\n", Afe_Get_Reg(AFE_AWB_END));
	pr_notice("AFE_AWB_CUR  = 0x%x\n", Afe_Get_Reg(AFE_AWB_CUR));
	pr_notice("AFE_VUL_BASE  = 0x%x\n", Afe_Get_Reg(AFE_VUL_BASE));
	pr_notice("AFE_VUL_END  = 0x%x\n", Afe_Get_Reg(AFE_VUL_END));
	pr_notice("AFE_VUL_CUR  = 0x%x\n", Afe_Get_Reg(AFE_VUL_CUR));
	pr_notice("AFE_DAI_BASE  = 0x%x\n", Afe_Get_Reg(AFE_DAI_BASE));
	pr_notice("AFE_DAI_END  = 0x%x\n", Afe_Get_Reg(AFE_DAI_END));
	pr_notice("AFE_DAI_CUR  = 0x%x\n", Afe_Get_Reg(AFE_DAI_CUR));
	pr_notice("AFE_IRQ_CON  = 0x%x\n", Afe_Get_Reg(AFE_IRQ_CON));

	pr_notice("AFE_MEMIF_MON0  = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON0));
	pr_notice("AFE_MEMIF_MON1  = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON1));
	pr_notice("AFE_MEMIF_MON2  = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON2));
	pr_notice("AFE_MEMIF_MON3  = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON3));
	pr_notice("AFE_MEMIF_MON4  = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON4));

#ifdef MT8135_AUD_REG
	pr_notice("AFE_ADDA_DL_SRC2_CON0  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON0));
	pr_notice("AFE_ADDA_DL_SRC2_CON1  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON1));
	pr_notice("AFE_ADDA_UL_SRC_CON0  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_UL_SRC_CON0));
	pr_notice("AFE_ADDA_UL_SRC_CON1  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_UL_SRC_CON1));
	pr_notice("AFE_ADDA_TOP_CON0  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_TOP_CON0));
	pr_notice("AFE_ADDA_UL_DL_CON0  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_UL_DL_CON0));
	pr_notice("AFE_ADDA_SRC_DEBUG  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_SRC_DEBUG));
	pr_notice("AFE_ADDA_SRC_DEBUG_MON0  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON0));
	pr_notice("AFE_ADDA_SRC_DEBUG_MON1  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON1));
	pr_notice("AFE_ADDA_NEWIF_CFG0  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_NEWIF_CFG0));
	pr_notice("AFE_ADDA_NEWIF_CFG1  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_NEWIF_CFG1));
#endif

	pr_notice("AFE_FOC_CON  = 0x%x\n", Afe_Get_Reg(AFE_FOC_CON));
	pr_notice("AFE_FOC_CON1  = 0x%x\n", Afe_Get_Reg(AFE_FOC_CON1));
	pr_notice("AFE_FOC_CON2  = 0x%x\n", Afe_Get_Reg(AFE_FOC_CON2));
	pr_notice("AFE_FOC_CON3  = 0x%x\n", Afe_Get_Reg(AFE_FOC_CON3));
	pr_notice("AFE_FOC_CON4  = 0x%x\n", Afe_Get_Reg(AFE_FOC_CON4));
	pr_notice("AFE_FOC_CON5  = 0x%x\n", Afe_Get_Reg(AFE_FOC_CON5));
	pr_notice("AFE_MON_STEP  = 0x%x\n", Afe_Get_Reg(AFE_MON_STEP));

	pr_notice("AFE_SIDETONE_DEBUG  = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_DEBUG));
	pr_notice("AFE_SIDETONE_MON  = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_MON));
	pr_notice("AFE_SIDETONE_CON0  = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_CON0));
	pr_notice("AFE_SIDETONE_COEFF  = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_COEFF));
	pr_notice("AFE_SIDETONE_CON1  = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_CON1));
	pr_notice("AFE_SIDETONE_GAIN  = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_GAIN));
	pr_notice("AFE_SIDETONE_GAIN  = 0x%x\n", Afe_Get_Reg(AFE_SGEN_CON0));
	pr_notice("AFE_TOP_CON0  = 0x%x\n", Afe_Get_Reg(AFE_TOP_CON0));
#ifdef MT8135_AUD_REG
	pr_notice("AFE_ADDA_PREDIS_CON0  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_PREDIS_CON0));
	pr_notice("AFE_ADDA_PREDIS_CON1  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_PREDIS_CON1));
#else
	pr_notice("AFE_PREDIS_CON0  = 0x%x\n", Afe_Get_Reg(AFE_PREDIS_CON0));
	pr_notice("AFE_PREDIS_CON1  = 0x%x\n", Afe_Get_Reg(AFE_PREDIS_CON1));
#endif
	pr_notice("AFE_MRGIF_MON0  = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_MON0));
	pr_notice("AFE_MRGIF_MON1  = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_MON1));
	pr_notice("AFE_MRGIF_MON2  = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_MON2));
	pr_notice("AFE_MOD_PCM_BASE  = 0x%x\n", Afe_Get_Reg(AFE_MOD_PCM_BASE));
	pr_notice("AFE_MOD_PCM_END  = 0x%x\n", Afe_Get_Reg(AFE_MOD_PCM_END));
	pr_notice("AFE_MOD_PCM_CUR  = 0x%x\n", Afe_Get_Reg(AFE_MOD_PCM_CUR));

#ifdef MT8135_AUD_REG
	pr_notice("AFE_HDMI_OUT_CON0  = 0x%x\n", Afe_Get_Reg(AFE_HDMI_OUT_CON0));
	pr_notice("AFE_HDMI_OUT_BASE  = 0x%x\n", Afe_Get_Reg(AFE_HDMI_OUT_BASE));
	pr_notice("AFE_HDMI_OUT_CUR  = 0x%x\n", Afe_Get_Reg(AFE_HDMI_OUT_CUR));
	pr_notice("AFE_HDMI_OUT_END  = 0x%x\n", Afe_Get_Reg(AFE_HDMI_OUT_END));
	pr_notice("AFE_SPDIF_OUT_CON0  = 0x%x\n", Afe_Get_Reg(AFE_SPDIF_OUT_CON0));
	pr_notice("AFE_SPDIF_BASE  = 0x%x\n", Afe_Get_Reg(AFE_SPDIF_BASE));
	pr_notice("AFE_SPDIF_CUR  = 0x%x\n", Afe_Get_Reg(AFE_SPDIF_CUR));
	pr_notice("AFE_SPDIF_END  = 0x%x\n", Afe_Get_Reg(AFE_SPDIF_END));
	pr_notice("AFE_HDMI_CONN0  = 0x%x\n", Afe_Get_Reg(AFE_HDMI_CONN0));
	pr_notice("AFE_8CH_I2S_OUT_CON  = 0x%x\n", Afe_Get_Reg(AFE_8CH_I2S_OUT_CON));
#endif

	pr_notice("AFE_IRQ_MCU_CON  = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CON));
	pr_notice("AFE_IRQ_STATUS  = 0x%x\n", Afe_Get_Reg(AFE_IRQ_STATUS));
	pr_notice("AFE_IRQ_CLR  = 0x%x\n", Afe_Get_Reg(AFE_IRQ_CLR));
	pr_notice("AFE_IRQ_MCU_CNT1  = 0x%x\n", Afe_Get_Reg(AFE_IRQ_CNT1));
	pr_notice("AFE_IRQ_MCU_CNT2  = 0x%x\n", Afe_Get_Reg(AFE_IRQ_CNT2));
	pr_notice("AFE_IRQ_MCU_MON2  = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MON2));
	pr_notice("AFE_IRQ_MCU_CNT5  = 0x%x\n", Afe_Get_Reg(AFE_IRQ_CNT5));
	pr_notice("AFE_IRQ1_MCU_CNT_MON  = 0x%x\n", Afe_Get_Reg(AFE_IRQ1_CNT_MON));
	pr_notice("AFE_IRQ2_MCU_CNT_MON  = 0x%x\n", Afe_Get_Reg(AFE_IRQ2_CNT_MON));
	pr_notice("AFE_IRQ1_MCU_EN_CNT_MON  = 0x%x\n", Afe_Get_Reg(AFE_IRQ1_EN_CNT_MON));
	pr_notice("AFE_IRQ5_MCU_EN_CNT_MON  = 0x%x\n", Afe_Get_Reg(AFE_IRQ5_EN_CNT_MON));
	pr_notice("AFE_MEMIF_MINLEN  = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MINLEN));
	pr_notice("AFE_MEMIF_MAXLEN  = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MAXLEN));
	pr_notice("AFE_MEMIF_PBUF_SIZE  = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_PBUF_SIZE));

	pr_notice("AFE_GAIN1_CON0  = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CON0));
	pr_notice("AFE_GAIN1_CON1  = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CON1));
	pr_notice("AFE_GAIN1_CON2  = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CON2));
	pr_notice("AFE_GAIN1_CON3  = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CON3));
	pr_notice("AFE_GAIN1_CONN  = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CONN));
	pr_notice("AFE_GAIN1_CUR   = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CUR));
	pr_notice("AFE_GAIN2_CON0  = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CON0));
	pr_notice("AFE_GAIN2_CON1  = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CON1));
	pr_notice("AFE_GAIN2_CON2  = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CON2));
	pr_notice("AFE_GAIN2_CON3  = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CON3));
	pr_notice("AFE_GAIN2_CONN  = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CONN));
	pr_notice("AFE_GAIN2_CUR   = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CUR));
	pr_notice("AFE_GAIN2_CONN2 = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CONN2));

#ifdef MT8135_AUD_REG
	pr_notice("AFE_IEC_CFG = 0x%x\n", Afe_Get_Reg(AFE_IEC_CFG));
	pr_notice("AFE_IEC_NSNUM = 0x%x\n", Afe_Get_Reg(AFE_IEC_NSNUM));
	pr_notice("AFE_IEC_BURST_INFO = 0x%x\n", Afe_Get_Reg(AFE_IEC_BURST_INFO));
	pr_notice("AFE_IEC_BURST_LEN = 0x%x\n", Afe_Get_Reg(AFE_IEC_BURST_LEN));
	pr_notice("AFE_IEC_NSADR = 0x%x\n", Afe_Get_Reg(AFE_IEC_NSADR));
	pr_notice("AFE_IEC_CHL_STAT0 = 0x%x\n", Afe_Get_Reg(AFE_IEC_CHL_STAT0));
	pr_notice("AFE_IEC_CHL_STAT1 = 0x%x\n", Afe_Get_Reg(AFE_IEC_CHL_STAT1));
	pr_notice("AFE_IEC_CHR_STAT0 = 0x%x\n", Afe_Get_Reg(AFE_IEC_CHR_STAT0));
	pr_notice("AFE_IEC_CHR_STAT1 = 0x%x\n", Afe_Get_Reg(AFE_IEC_CHR_STAT1));
#endif

	pr_notice("AFE_ASRC_CON0  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON0));
	pr_notice("AFE_ASRC_CON1  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON1));
	pr_notice("AFE_ASRC_CON2  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON2));
	pr_notice("AFE_ASRC_CON3  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON3));
	pr_notice("AFE_ASRC_CON4  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON4));
	pr_notice("AFE_ASRC_CON5  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON5));
	pr_notice("AFE_ASRC_CON6  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON6));
	pr_notice("AFE_ASRC_CON7  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON7));
	pr_notice("AFE_ASRC_CON8  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON8));
	pr_notice("AFE_ASRC_CON9  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON9));
	pr_notice("AFE_ASRC_CON10 = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON10));
	pr_notice("AFE_ASRC_CON11 = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON11));
	pr_notice("PCM_INTF_CON1  = 0x%x\n", Afe_Get_Reg(PCM_INTF_CON1));
	pr_notice("PCM_INTF_CON2  = 0x%x\n", Afe_Get_Reg(PCM_INTF_CON2));
	pr_notice("PCM2_INTF_CON  = 0x%x\n", Afe_Get_Reg(PCM2_INTF_CON));
	AudDrv_Clk_Off();
	pr_notice("-AudDrv Afe_Log_Print\n");
}
EXPORT_SYMBOL(Afe_Log_Print);
