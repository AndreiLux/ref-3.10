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

#ifndef AUDDRV_KERNEL_H
#define AUDDRV_KERNEL_H

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

struct AudAfe_Suspend_Reg {
	uint32_t Suspend_AUDIO_TOP_CON0;
	/* DANIEL_TODO: AUDIO_TOP_CON1 */
	uint32_t Suspend_AUDIO_TOP_CON3;
	uint32_t Suspend_AFE_DAC_CON0;
	uint32_t Suspend_AFE_DAC_CON1;
	uint32_t Suspend_AFE_I2S_CON;
	uint32_t Suspend_AFE_DAIBT_CON0;

	uint32_t Suspend_AFE_CONN0;
	uint32_t Suspend_AFE_CONN1;
	uint32_t Suspend_AFE_CONN2;
	uint32_t Suspend_AFE_CONN3;
	uint32_t Suspend_AFE_CONN4;

	uint32_t Suspend_AFE_I2S_CON1;
	uint32_t Suspend_AFE_I2S_CON2;
	uint32_t Suspend_AFE_MRGIF_CON;

	uint32_t Suspend_AFE_DL1_BASE;
	uint32_t Suspend_AFE_DL1_CUR;
	uint32_t Suspend_AFE_DL1_END;
	uint32_t Suspend_AFE_DL2_BASE;
	uint32_t Suspend_AFE_DL2_CUR;
	uint32_t Suspend_AFE_DL2_END;
	uint32_t Suspend_AFE_AWB_BASE;
	uint32_t Suspend_AFE_AWB_CUR;
	uint32_t Suspend_AFE_AWB_END;
	uint32_t Suspend_AFE_VUL_BASE;
	uint32_t Suspend_AFE_VUL_CUR;
	uint32_t Suspend_AFE_VUL_END;
	uint32_t Suspend_AFE_DAI_BASE;
	uint32_t Suspend_AFE_DAI_CUR;
	uint32_t Suspend_AFE_DAI_END;

	uint32_t Suspend_AFE_IRQ_CON;
	uint32_t Suspend_AFE_MEMIF_MON0;
	uint32_t Suspend_AFE_MEMIF_MON1;
	uint32_t Suspend_AFE_MEMIF_MON2;
	uint32_t Suspend_AFE_MEMIF_MON3;
	uint32_t Suspend_AFE_MEMIF_MON4;

	uint32_t Suspend_AFE_ADDA_DL_SRC2_CON0;
	uint32_t Suspend_AFE_ADDA_DL_SRC2_CON1;
	uint32_t Suspend_AFE_ADDA_UL_SRC_CON0;
	uint32_t Suspend_AFE_ADDA_UL_SRC_CON1;
	uint32_t Suspend_AFE_ADDA_TOP_CON0;
	uint32_t Suspend_AFE_ADDA_UL_DL_CON0;
	uint32_t Suspend_AFE_ADDA_NEWIF_CFG0;
	uint32_t Suspend_AFE_ADDA_NEWIF_CFG1;

	uint32_t Suspend_AFE_FOC_CON;
	uint32_t Suspend_AFE_FOC_CON1;
	uint32_t Suspend_AFE_FOC_CON2;
	uint32_t Suspend_AFE_FOC_CON3;
	uint32_t Suspend_AFE_FOC_CON4;
	uint32_t Suspend_AFE_FOC_CON5;
	uint32_t Suspend_AFE_MON_STEP;

	uint32_t Suspend_AFE_SIDETONE_DEBUG;
	uint32_t Suspend_AFE_SIDETONE_MON;
	uint32_t Suspend_AFE_SIDETONE_CON0;
	uint32_t Suspend_AFE_SIDETONE_COEFF;
	uint32_t Suspend_AFE_SIDETONE_CON1;
	uint32_t Suspend_AFE_SIDETONE_GAIN;
	uint32_t Suspend_AFE_SGEN_CON0;

	/* uint32_t Suspend_AFE_TOP_CON0; */
	/* DANIEL_TODO: why no AFE_TOP_CON0 */

	uint32_t Suspend_AFE_ADDA_PREDIS_CON0;
	uint32_t Suspend_AFE_ADDA_PREDIS_CON1;

	uint32_t Suspend_AFE_MRGIF_MON0;
	uint32_t Suspend_AFE_MRGIF_MON1;
	uint32_t Suspend_AFE_MRGIF_MON2;

	uint32_t Suspend_AFE_MOD_PCM_BASE;
	uint32_t Suspend_AFE_MOD_PCM_END;
	uint32_t Suspend_AFE_MOD_PCM_CUR;

	uint32_t Suspend_AFE_HDMI_OUT_CON0;
	uint32_t Suspend_AFE_HDMI_OUT_BASE;
	uint32_t Suspend_AFE_HDMI_OUT_CUR;
	uint32_t Suspend_AFE_HDMI_OUT_END;
	uint32_t Suspend_AFE_SPDIF_OUT_CON0;
	uint32_t Suspend_AFE_SPDIF_BASE;
	uint32_t Suspend_AFE_SPDIF_CUR;
	uint32_t Suspend_AFE_SPDIF_END;
	uint32_t Suspend_AFE_HDMI_CONN0;
	uint32_t Suspend_AFE_8CH_I2S_OUT_CON;

	uint32_t Suspend_AFE_IRQ_MCU_CON;
	uint32_t Suspend_AFE_IRQ_STATUS;
	uint32_t Suspend_AFE_IRQ_CLR;
	uint32_t Suspend_AFE_IRQ_CNT1;
	uint32_t Suspend_AFE_IRQ_CNT2;
	uint32_t Suspend_AFE_IRQ_MON2;
	uint32_t Suspend_AFE_IRQ_CNT5;
	uint32_t Suspend_AFE_IRQ1_CNT_MON;
	uint32_t Suspend_AFE_IRQ2_CNT_MON;
	uint32_t Suspend_AFE_IRQ1_EN_CNT_MON;
	uint32_t Suspend_AFE_IRQ5_EN_CNT_MON;
	uint32_t Suspend_AFE_MEMIF_MINLEN;
	uint32_t Suspend_AFE_MEMIF_MAXLEN;
	uint32_t Suspend_AFE_MEMIF_PBUF_SIZE;

	uint32_t Suspend_AFE_GAIN1_CON0;
	uint32_t Suspend_AFE_GAIN1_CON1;
	uint32_t Suspend_AFE_GAIN1_CON2;
	uint32_t Suspend_AFE_GAIN1_CON3;
	uint32_t Suspend_AFE_GAIN1_CONN;
	uint32_t Suspend_AFE_GAIN1_CUR;
	uint32_t Suspend_AFE_GAIN2_CON0;
	uint32_t Suspend_AFE_GAIN2_CON1;
	uint32_t Suspend_AFE_GAIN2_CON2;
	uint32_t Suspend_AFE_GAIN2_CON3;
	uint32_t Suspend_AFE_GAIN2_CONN;
	/* DANIEL_TODO: why no AFE_GAIN2_CUR,AFE_GAIN2_CONN, */
	/* FPGA_CFG0, FPGA_CFG1, FPGA_VERSION, FPGA_STC, DBG_MON0~6 */

	uint32_t Suspend_AFE_IEC_CFG;
	uint32_t Suspend_AFE_IEC_NSNUM;
	uint32_t Suspend_AFE_IEC_BURST_INFO;
	uint32_t Suspend_AFE_IEC_BURST_LEN;
	uint32_t Suspend_AFE_IEC_NSADR;
	uint32_t Suspend_AFE_IEC_CHL_STAT0;
	uint32_t Suspend_AFE_IEC_CHL_STAT1;
	uint32_t Suspend_AFE_IEC_CHR_STAT0;
	uint32_t Suspend_AFE_IEC_CHR_STAT1;

	uint32_t Suspend_AFE_ASRC_CON0;
	uint32_t Suspend_AFE_ASRC_CON1;
	uint32_t Suspend_AFE_ASRC_CON2;
	uint32_t Suspend_AFE_ASRC_CON3;
	uint32_t Suspend_AFE_ASRC_CON4;
	/* DANIEL_TOD: why no AFE_ASRC_CON5 */
	uint32_t Suspend_AFE_ASRC_CON6;
	uint32_t Suspend_AFE_ASRC_CON7;
	uint32_t Suspend_AFE_ASRC_CON8;
	uint32_t Suspend_AFE_ASRC_CON9;
	uint32_t Suspend_AFE_ASRC_CON10;
	uint32_t Suspend_AFE_ASRC_CON11;
	uint32_t Suspend_PCM_INTF_CON1;
	uint32_t Suspend_PCM_INTF_CON2;
	uint32_t Suspend_PCM2_INTF_CON;
	/* DANIEL_TODO: why no FOC_ROM_SIG */
};

struct AudAna_Suspend_Reg {
	uint16_t Suspend_Ana_AFE_UL_DL_CON0;
	uint16_t Suspend_Ana_AFE_DL_SRC2_CON0_H;
	uint16_t Suspend_Ana_AFE_DL_SRC2_CON0_L;
	uint16_t Suspend_Ana_AFE_DL_SDM_CON0;
	uint16_t Suspend_Ana_AFE_DL_SDM_CON1;
	uint16_t Suspend_Ana_AFE_UL_SRC_CON0_H;
	uint16_t Suspend_Ana_AFE_UL_SRC_CON0_L;
	uint16_t Suspend_Ana_AFE_UL_SRC_CON1_H;
	uint16_t Suspend_Ana_AFE_UL_SRC_CON1_L;
	uint16_t Suspend_Ana_AFE_TOP_CON0;
	uint16_t Suspend_Ana_AUDIO_TOP_CON0;
	uint16_t Suspend_Ana_AFUNC_AUD_CON0;
	uint16_t Suspend_Ana_AFUNC_AUD_CON1;
	uint16_t Suspend_Ana_AFUNC_AUD_CON2;
	uint16_t Suspend_Ana_AFUNC_AUD_CON3;
	uint16_t Suspend_Ana_AFUNC_AUD_CON4;
	uint16_t Suspend_Ana_AFE_UP8X_FIFO_CFG0;
	uint16_t Suspend_Ana_AFE_DL_DC_COMP_CFG0;
	uint16_t Suspend_Ana_AFE_DL_DC_COMP_CFG1;
	uint16_t Suspend_Ana_AFE_DL_DC_COMP_CFG2;
	uint16_t Suspend_Ana_AFE_PMIC_NEWIF_CFG0;
	uint16_t Suspend_Ana_AFE_PMIC_NEWIF_CFG1;
	uint16_t Suspend_Ana_AFE_PMIC_NEWIF_CFG2;
	uint16_t Suspend_Ana_AFE_PMIC_NEWIF_CFG3;
	uint16_t Suspend_Ana_AFE_SGEN_CFG0;
	uint16_t Suspend_Ana_AFE_SGEN_CFG1;

	uint16_t Suspend_Ana_TOP_CKPDN;
	uint16_t Suspend_Ana_TOP_CKPDN_SET;
	uint16_t Suspend_Ana_TOP_CKPDN_CLR;
	uint16_t Suspend_Ana_TOP_CKPDN2;
	uint16_t Suspend_Ana_TOP_CKPDN2_SET;
	uint16_t Suspend_Ana_TOP_CKPDN2_CLR;
	uint16_t Suspend_Ana_TOP_CKCON1;
	uint16_t Suspend_Ana_SPK_CON0;
	uint16_t Suspend_Ana_SPK_CON1;
	uint16_t Suspend_Ana_SPK_CON2;
	uint16_t Suspend_Ana_SPK_CON3;
	uint16_t Suspend_Ana_SPK_CON4;
	uint16_t Suspend_Ana_SPK_CON5;
	uint16_t Suspend_Ana_SPK_CON6;
	uint16_t Suspend_Ana_SPK_CON7;
	uint16_t Suspend_Ana_SPK_CON8;
	uint16_t Suspend_Ana_SPK_CON9;
	uint16_t Suspend_Ana_SPK_CON10;
	uint16_t Suspend_Ana_SPK_CON11;
	uint16_t Suspend_Ana_AUDDAC_CON0;
	uint16_t Suspend_Ana_AUDBUF_CFG0;
	uint16_t Suspend_Ana_AUDBUF_CFG1;
	uint16_t Suspend_Ana_AUDBUF_CFG2;
	uint16_t Suspend_Ana_AUDBUF_CFG3;
	uint16_t Suspend_Ana_AUDBUF_CFG4;
	uint16_t Suspend_Ana_IBIASDIST_CFG0;
	uint16_t Suspend_Ana_AUDACCDEPOP_CFG0;
	uint16_t Suspend_Ana_AUD_IV_CFG0;
	uint16_t Suspend_Ana_AUDCLKGEN_CFG0;
	uint16_t Suspend_Ana_AUDLDO_CFG0;
	uint16_t Suspend_Ana_AUDLDO_CFG1;
	uint16_t Suspend_Ana_AUDNVREGGLB_CFG0;
	uint16_t Suspend_Ana_AUD_NCP0;
	uint16_t Suspend_Ana_AUDPREAMP_CON0;
	uint16_t Suspend_Ana_AUDADC_CON0;
	uint16_t Suspend_Ana_AUDADC_CON1;
	uint16_t Suspend_Ana_AUDADC_CON2;
	uint16_t Suspend_Ana_AUDADC_CON3;
	uint16_t Suspend_Ana_AUDADC_CON4;
	uint16_t Suspend_Ana_AUDADC_CON5;
	uint16_t Suspend_Ana_AUDADC_CON6;
	uint16_t Suspend_Ana_AUDDIGMI_CON0;
	uint16_t Suspend_Ana_AUDLSBUF_CON0;
	uint16_t Suspend_Ana_AUDLSBUF_CON1;
	uint16_t Suspend_Ana_AUDENCSPARE_CON0;
	uint16_t Suspend_Ana_AUDENCCLKSQ_CON0;
	uint16_t Suspend_Ana_AUDPREAMPGAIN_CON0;
	uint16_t Suspend_Ana_ZCD_CON0;
	uint16_t Suspend_Ana_ZCD_CON1;
	uint16_t Suspend_Ana_ZCD_CON2;
	uint16_t Suspend_Ana_ZCD_CON3;
	uint16_t Suspend_Ana_ZCD_CON4;
	uint16_t Suspend_Ana_ZCD_CON5;
	uint16_t Suspend_Ana_NCP_CLKDIV_CON0;
	uint16_t Suspend_Ana_NCP_CLKDIV_CON1;

};

enum IRQ_MCU_TYPE {
	INTERRUPT_IRQ1_MCU = 1,
	INTERRUPT_IRQ2_MCU = 2,
	INTERRUPT_IRQ_MCU_DAI_SET = 4,
	INTERRUPT_IRQ_MCU_DAI_RST = 8,
	INTERRUPT_HDMI_IRQ = 16,
	INTERRUPT_SPDF_IRQ = 32
};

enum {
	CLOCK_AUD_AFE = 0,
	CLOCK_AUD_I2S,
	CLOCK_AUD_ADC,
	CLOCK_AUD_DAC,
	CLOCK_AUD_LINEIN,
	CLOCK_AUD_HDMI,
	CLOCK_AUD_26M,		/* core clock */
	CLOCK_TYPE_MAX
};

#endif
