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

#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "mt_soc_afe_control.h"
#include <linux/module.h>
#include <sound/soc.h>
#include <linux/debugfs.h>
#include <linux/input.h>
#include <sound/jack.h>
#include <sound/max97236.h>

#ifdef CONFIG_MT_ENG_BUILD
static struct dentry *mt_sco_audio_debugfs;
static struct dentry *mt_sco_dl_audio_debugfs;
#endif

#define DEBUG_FS_NAME "mtksocaudio"
#define DEBUG_DL_FS_NAME "mtksocdlaudio"

#define MCLK_RATE 2000000

static struct snd_soc_jack ariel_jack;

#ifdef CONFIG_MT_ENG_BUILD
static int mt_soc_debug_open(struct inode *inode, struct file *file)
{
	pr_notice("mt_soc_debug_open\n");
	return 0;
}

static ssize_t mt_soc_debug_read(struct file *file, char __user *buf,
				 size_t count, loff_t *pos)
{
	const int size = 4096;
	char buffer[size];
	int n = 0;

	pr_notice("mt_soc_debug_read\n");
	AudDrv_Clk_On();

	n = scnprintf(buffer, size - n, "mt_soc_debug_read\n");

	n += scnprintf(buffer + n, size - n, "Afe_Mem_Pwr_on =0x%x\n",
		       Afe_Mem_Pwr_on);
	n += scnprintf(buffer + n, size - n, "Aud_AFE_Clk_cntr = 0x%x\n",
		       Aud_AFE_Clk_cntr);
	n += scnprintf(buffer + n, size - n, "Aud_ANA_Clk_cntr = 0x%x\n",
		       Aud_ANA_Clk_cntr);
	n += scnprintf(buffer + n, size - n, "Aud_HDMI_Clk_cntr = 0x%x\n",
		       Aud_HDMI_Clk_cntr);
	n += scnprintf(buffer + n, size - n, "Aud_I2S_Clk_cntr = 0x%x\n",
		       Aud_I2S_Clk_cntr);
	n += scnprintf(buffer + n, size - n, "Aud_APLL_Tuner_Clk_cntr = 0x%x\n",
		       Aud_APLL_Tuner_Clk_cntr);
	n += scnprintf(buffer + n, size - n, "Aud_SPDIF_Clk_cntr = 0x%x\n",
		       Aud_SPDIF_Clk_cntr);
	/* n += scnprintf(buffer+n, size-n,"AuddrvSpkStatus = 0x%x\n", AuddrvSpkStatus); */

	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON0  = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON1  = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_CON1));
	/* n += scnprintf(buffer+n, size-n, "AUDIO_TOP_CON2  = 0x%x\n", Afe_Get_Reg(AUDIO_TOP_CON2)); */
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON3  = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_DAC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_DAC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON  = 0x%x\n",
		       Afe_Get_Reg(AFE_I2S_CON));
	n += scnprintf(buffer + n, size - n, "AFE_DAIBT_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_DAIBT_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN0  = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN1  = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN2  = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_CONN3  = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN3));
	n += scnprintf(buffer + n, size - n, "AFE_CONN4  = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN4));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_I2S_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON2  = 0x%x\n",
		       Afe_Get_Reg(AFE_I2S_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_CON  = 0x%x\n",
		       Afe_Get_Reg(AFE_MRGIF_CON));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_BASE  = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_CUR  = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_END  = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_END));
	/* n += scnprintf(buffer+n, size-n, "AFE_I2S_CON3  = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON3)); */
	n += scnprintf(buffer + n, size - n, "AFE_DL2_BASE  = 0x%x\n",
		       Afe_Get_Reg(AFE_DL2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_CUR  = 0x%x\n",
		       Afe_Get_Reg(AFE_DL2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_END  = 0x%x\n",
		       Afe_Get_Reg(AFE_DL2_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_BASE  = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_END  = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_CUR  = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_BASE  = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_END  = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_CUR  = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_BASE  = 0x%x\n",
		       Afe_Get_Reg(AFE_DAI_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_END  = 0x%x\n",
		       Afe_Get_Reg(AFE_DAI_END));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_CUR  = 0x%x\n",
		       Afe_Get_Reg(AFE_DAI_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CON= 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_CON));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON0));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON1));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON2));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON3 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON3));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON4 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON4));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_UL_SRC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_UL_SRC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_TOP_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_DL_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_SRC_DEBUG));
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_SRC_DEBUG_MON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON0));
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_SRC_DEBUG_MON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_NEWIF_CFG1));
	n += scnprintf(buffer + n, size - n, "SIDETONE_DEBUG = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_DEBUG));
	n += scnprintf(buffer + n, size - n, "SIDETONE_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_MON));
	n += scnprintf(buffer + n, size - n, "SIDETONE_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_CON0));
	n += scnprintf(buffer + n, size - n, "SIDETONE_COEFF = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_COEFF));
	n += scnprintf(buffer + n, size - n, "SIDETONE_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_CON1));
	n += scnprintf(buffer + n, size - n, "SIDETONE_GAIN = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_GAIN));
	n += scnprintf(buffer + n, size - n, "SGEN_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_SGEN_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_MRGIF_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_MRGIF_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_MRGIF_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_TOP_CON0));

	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_PREDIS_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_PREDIS_CON1));

	n += scnprintf(buffer + n, size - n, "HDMI_OUT_CON0   = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "HDMI_OUT_BASE   = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_OUT_BASE));
	n += scnprintf(buffer + n, size - n, "HDMI_OUT_CUR    = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_OUT_CUR));
	n += scnprintf(buffer + n, size - n, "HDMI_OUT_END    = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_OUT_END));
	n += scnprintf(buffer + n, size - n, "SPDIF_OUT_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_SPDIF_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "SPDIF_BASE      = 0x%x\n",
		       Afe_Get_Reg(AFE_SPDIF_BASE));
	n += scnprintf(buffer + n, size - n, "SPDIF_CUR       = 0x%x\n",
		       Afe_Get_Reg(AFE_SPDIF_CUR));
	n += scnprintf(buffer + n, size - n, "SPDIF_END       = 0x%x\n",
		       Afe_Get_Reg(AFE_SPDIF_END));
	n += scnprintf(buffer + n, size - n, "HDMI_CONN0      = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_CONN0));
	n += scnprintf(buffer + n, size - n, "8CH_I2S_OUT_CON = 0x%x\n",
		       Afe_Get_Reg(AFE_8CH_I2S_OUT_CON));
	n += scnprintf(buffer + n, size - n, "IEC_CFG         = 0x%x\n",
		       Afe_Get_Reg(AFE_IEC_CFG));
	n += scnprintf(buffer + n, size - n, "IEC_NSNUM       = 0x%x\n",
		       Afe_Get_Reg(AFE_IEC_NSNUM));
	n += scnprintf(buffer + n, size - n, "IEC_BURST_INFO  = 0x%x\n",
		       Afe_Get_Reg(AFE_IEC_BURST_INFO));
	n += scnprintf(buffer + n, size - n, "IEC_BURST_LEN   = 0x%x\n",
		       Afe_Get_Reg(AFE_IEC_BURST_LEN));
	n += scnprintf(buffer + n, size - n, "IEC_NSADR       = 0x%x\n",
		       Afe_Get_Reg(AFE_IEC_NSADR));
	n += scnprintf(buffer + n, size - n, "IEC_CHL_STAT0   = 0x%x\n",
		       Afe_Get_Reg(AFE_IEC_CHL_STAT0));
	n += scnprintf(buffer + n, size - n, "IEC_CHL_STAT1   = 0x%x\n",
		       Afe_Get_Reg(AFE_IEC_CHL_STAT1));
	n += scnprintf(buffer + n, size - n, "IEC_CHR_STAT0   = 0x%x\n",
		       Afe_Get_Reg(AFE_IEC_CHR_STAT0));
	n += scnprintf(buffer + n, size - n, "IEC_CHR_STAT1   = 0x%x\n",
		       Afe_Get_Reg(AFE_IEC_CHR_STAT1));
	/* n += scnprintf(buffer+n, size-n, "AFE_MOD_PCM_BASE = 0x%x\n", Afe_Get_Reg(AFE_MOD_PCM_BASE)); */
	/* n += scnprintf(buffer+n, size-n, "AFE_MOD_PCM_END = 0x%x\n", Afe_Get_Reg(AFE_MOD_PCM_END)); */
	/* n += scnprintf(buffer+n, size-n, "AFE_MOD_PCM_CUR = 0x%x\n", Afe_Get_Reg(AFE_MOD_PCM_CUR)); */
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CON));	/* ccc */
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_STATUS = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_STATUS));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CLR = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_CLR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CNT1 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_CNT1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CNT2 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_CNT2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CNT5 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_CNT5));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ1_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ2_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ2_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_EN_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ1_EN_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ5_EN_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ5_EN_CNT_MON));

	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MINLEN  = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MINLEN));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MAXLEN  = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MAXLEN));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_PBUF_SIZE  = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_PBUF_SIZE));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON2  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON3  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CONN  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CUR  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON2  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON3  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN2  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CUR  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CUR));

	/*
	   n += scnprintf(buffer+n, size-n, "FPGA_CFG2  = 0x%x\n", Afe_Get_Reg(FPGA_CFG2));
	   n += scnprintf(buffer+n, size-n, "FPGA_CFG3  = 0x%x\n", Afe_Get_Reg(FPGA_CFG3));
	   n += scnprintf(buffer+n, size-n, "FPGA_CFG0  = 0x%x\n", Afe_Get_Reg(FPGA_CFG0));
	   n += scnprintf(buffer+n, size-n, "FPGA_CFG1  = 0x%x\n", Afe_Get_Reg(FPGA_CFG1));
	   n += scnprintf(buffer+n, size-n, "FPGA_STC  = 0x%x\n", Afe_Get_Reg(FPGA_STC));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON0  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON0));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON1  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON1));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON2  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON2));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON3  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON3));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON4  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON4));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON5  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON5));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON6  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON6));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON7  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON7));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON8  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON8));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON9  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON9));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON10  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON10));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON11  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON11));
	   n += scnprintf(buffer+n, size-n, "PCM_INTF_CON1 = 0x%x\n", Afe_Get_Reg(PCM_INTF_CON1));
	   n += scnprintf(buffer+n, size-n, "PCM_INTF_CON2 = 0x%x\n", Afe_Get_Reg(PCM_INTF_CON2));
	   n += scnprintf(buffer+n, size-n, "PCM2_INTF_CON = 0x%x\n", Afe_Get_Reg(PCM2_INTF_CON));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON13  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON13));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON14  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON14));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON15  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON15));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON16  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON16));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON17  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON17));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON18  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON18));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON19  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON19));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON20  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON20));
	   n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON21  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON21));
	 */
	n += scnprintf(buffer + n, size - n,
		       "======PMIC digital registers====\n");
	n += scnprintf(buffer + n, size - n, "UL_DL_CON0 = 0x%x\n",
		       Ana_Get_Reg(AFE_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "DL_SRC2_CON0_H = 0x%x\n",
		       Ana_Get_Reg(AFE_DL_SRC2_CON0_H));
	n += scnprintf(buffer + n, size - n, "DL_SRC2_CON0_L = 0x%x\n",
		       Ana_Get_Reg(AFE_DL_SRC2_CON0_L));
	n += scnprintf(buffer + n, size - n, "DL_SDM_CON0 = 0x%x\n",
		       Ana_Get_Reg(AFE_DL_SDM_CON0));
	n += scnprintf(buffer + n, size - n, "DL_SDM_CON1 = 0x%x\n",
		       Ana_Get_Reg(AFE_DL_SDM_CON1));
	n += scnprintf(buffer + n, size - n, "UL_SRC_CON0_H = 0x%x\n",
		       Ana_Get_Reg(AFE_UL_SRC_CON0_H));
	n += scnprintf(buffer + n, size - n, "UL_SRC_CON0_L = 0x%x\n",
		       Ana_Get_Reg(AFE_UL_SRC_CON0_L));
	n += scnprintf(buffer + n, size - n, "UL_SRC_CON1_H = 0x%x\n",
		       Ana_Get_Reg(AFE_UL_SRC_CON1_H));
	n += scnprintf(buffer + n, size - n, "UL_SRC_CON1_L = 0x%x\n",
		       Ana_Get_Reg(AFE_UL_SRC_CON1_L));
	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0 = 0x%x\n",
		       Ana_Get_Reg(ANA_AFE_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON0 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_CON0));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON1 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_CON1));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON2 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_CON2));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON3 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_CON3));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON4 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_CON4));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_MON0 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_MON0));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_MON1 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_MON1));
	n += scnprintf(buffer + n, size - n, "AUDRC_TUNE_MON0 = 0x%x\n",
		       Ana_Get_Reg(AUDRC_TUNE_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_UP8X_FIFO_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_UP8X_FIFO_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_UP8X_FIFO_LOG_MON0 = 0x%x\n",
		       Ana_Get_Reg(AFE_UP8X_FIFO_LOG_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_UP8X_FIFO_LOG_MON1 = 0x%x\n",
		       Ana_Get_Reg(AFE_UP8X_FIFO_LOG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_DL_DC_COMP_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_DL_DC_COMP_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_DL_DC_COMP_CFG1 = 0x%x\n",
		       Ana_Get_Reg(AFE_DL_DC_COMP_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_DL_DC_COMP_CFG2 = 0x%x\n",
		       Ana_Get_Reg(AFE_DL_DC_COMP_CFG2));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_PMIC_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG1 = 0x%x\n",
		       Ana_Get_Reg(AFE_PMIC_NEWIF_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG2 = 0x%x\n",
		       Ana_Get_Reg(AFE_PMIC_NEWIF_CFG2));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG3 = 0x%x\n",
		       Ana_Get_Reg(AFE_PMIC_NEWIF_CFG3));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_SGEN_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CFG1 = 0x%x\n",
		       Ana_Get_Reg(AFE_SGEN_CFG1));
	n += scnprintf(buffer + n, size - n,
		       "======PMIC analog registers====\n");
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN = 0x%x\n",
		       Ana_Get_Reg(TOP_CKPDN));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN2 = 0x%x\n",
		       Ana_Get_Reg(TOP_CKPDN2));
	n += scnprintf(buffer + n, size - n, "TOP_CKCON1 = 0x%x\n",
		       Ana_Get_Reg(TOP_CKCON1));
	n += scnprintf(buffer + n, size - n, "SPK_CON0 = 0x%x\n",
		       Ana_Get_Reg(SPK_CON0));
	n += scnprintf(buffer + n, size - n, "SPK_CON1 = 0x%x\n",
		       Ana_Get_Reg(SPK_CON1));
	n += scnprintf(buffer + n, size - n, "SPK_CON2 = 0x%x\n",
		       Ana_Get_Reg(SPK_CON2));
	n += scnprintf(buffer + n, size - n, "SPK_CON3 = 0x%x\n",
		       Ana_Get_Reg(SPK_CON3));
	n += scnprintf(buffer + n, size - n, "SPK_CON4 = 0x%x\n",
		       Ana_Get_Reg(SPK_CON4));
	n += scnprintf(buffer + n, size - n, "SPK_CON5 = 0x%x\n",
		       Ana_Get_Reg(SPK_CON5));
	n += scnprintf(buffer + n, size - n, "SPK_CON6 = 0x%x\n",
		       Ana_Get_Reg(SPK_CON6));
	n += scnprintf(buffer + n, size - n, "SPK_CON7 = 0x%x\n",
		       Ana_Get_Reg(SPK_CON7));
	n += scnprintf(buffer + n, size - n, "SPK_CON8 = 0x%x\n",
		       Ana_Get_Reg(SPK_CON8));
	n += scnprintf(buffer + n, size - n, "SPK_CON9 = 0x%x\n",
		       Ana_Get_Reg(SPK_CON9));
	n += scnprintf(buffer + n, size - n, "SPK_CON10 = 0x%x\n",
		       Ana_Get_Reg(SPK_CON10));
	n += scnprintf(buffer + n, size - n, "SPK_CON11 = 0x%x\n",
		       Ana_Get_Reg(SPK_CON11));
	n += scnprintf(buffer + n, size - n, "AUDDAC_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUDDAC_CON0));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AUDBUF_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG1 = 0x%x\n",
		       Ana_Get_Reg(AUDBUF_CFG1));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG2 = 0x%x\n",
		       Ana_Get_Reg(AUDBUF_CFG2));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG3 = 0x%x\n",
		       Ana_Get_Reg(AUDBUF_CFG3));
	n += scnprintf(buffer + n, size - n, "AUDBUF_CFG4 = 0x%x\n",
		       Ana_Get_Reg(AUDBUF_CFG4));
	n += scnprintf(buffer + n, size - n, "IBIASDIST_CFG0 = 0x%x\n",
		       Ana_Get_Reg(IBIASDIST_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDACCDEPOP_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AUDACCDEPOP_CFG0));
	n += scnprintf(buffer + n, size - n, "AUD_IV_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AUD_IV_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDCLKGEN_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AUDCLKGEN_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDLDO_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AUDLDO_CFG0));
	n += scnprintf(buffer + n, size - n, "AUDLDO_CFG1 = 0x%x\n",
		       Ana_Get_Reg(AUDLDO_CFG1));
	n += scnprintf(buffer + n, size - n, "AUDNVREGGLB_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AUDNVREGGLB_CFG0));
	n += scnprintf(buffer + n, size - n, "AUD_NCP0 = 0x%x\n",
		       Ana_Get_Reg(AUD_NCP0));
	n += scnprintf(buffer + n, size - n, "AUDPREAMP_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUDPREAMP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUDADC_CON0));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON1 = 0x%x\n",
		       Ana_Get_Reg(AUDADC_CON1));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON2 = 0x%x\n",
		       Ana_Get_Reg(AUDADC_CON2));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON3 = 0x%x\n",
		       Ana_Get_Reg(AUDADC_CON3));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON4 = 0x%x\n",
		       Ana_Get_Reg(AUDADC_CON4));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON5 = 0x%x\n",
		       Ana_Get_Reg(AUDADC_CON5));
	n += scnprintf(buffer + n, size - n, "AUDADC_CON6 = 0x%x\n",
		       Ana_Get_Reg(AUDADC_CON6));
	n += scnprintf(buffer + n, size - n, "AUDDIGMI_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUDDIGMI_CON0));
	n += scnprintf(buffer + n, size - n, "AUDLSBUF_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUDLSBUF_CON0));
	n += scnprintf(buffer + n, size - n, "AUDLSBUF_CON1 = 0x%x\n",
		       Ana_Get_Reg(AUDLSBUF_CON1));
	n += scnprintf(buffer + n, size - n, "AUDENCSPARE_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUDENCSPARE_CON0));
	n += scnprintf(buffer + n, size - n, "AUDENCCLKSQ_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUDENCCLKSQ_CON0));
	n += scnprintf(buffer + n, size - n, "AUDPREAMPGAIN_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUDPREAMPGAIN_CON0));
	n += scnprintf(buffer + n, size - n, "ZCD_CON0 = 0x%x\n",
		       Ana_Get_Reg(ZCD_CON0));
	n += scnprintf(buffer + n, size - n, "ZCD_CON1 = 0x%x\n",
		       Ana_Get_Reg(ZCD_CON1));
	n += scnprintf(buffer + n, size - n, "ZCD_CON2 = 0x%x\n",
		       Ana_Get_Reg(ZCD_CON2));
	n += scnprintf(buffer + n, size - n, "ZCD_CON3 = 0x%x\n",
		       Ana_Get_Reg(ZCD_CON3));
	n += scnprintf(buffer + n, size - n, "ZCD_CON4 = 0x%x\n",
		       Ana_Get_Reg(ZCD_CON4));
	n += scnprintf(buffer + n, size - n, "ZCD_CON5 = 0x%x\n",
		       Ana_Get_Reg(ZCD_CON5));
	n += scnprintf(buffer + n, size - n, "NCP_CLKDIV_CON0 = 0x%x\n",
		       Ana_Get_Reg(NCP_CLKDIV_CON0));
	n += scnprintf(buffer + n, size - n, "NCP_CLKDIV_CON1 = 0x%x\n",
		       Ana_Get_Reg(NCP_CLKDIV_CON1));
	/* n += scnprintf(buffer+n, size-n, "AUDPLL_CON0 = 0x%x\n", AP_Get_Reg(AUDPLL_CON0)); */
	/* n+= scnprintf(buffer+n, size-n, "AUDPLL_CON1 = 0x%x\n", AP_Get_Reg(AUDPLL_CON1)); */
	/* n+= scnprintf(buffer+n, size-n, "AUDPLL_CON4 = 0x%x\n", AP_Get_Reg(AUDPLL_CON4)); */
	/* n += scnprintf(buffer+n, size-n, "CLK_CFG_9 = 0x%x\n", AP_Get_Reg(CLK_CFG_9)); */
	pr_notice("mt_soc_debug_read len = %d\n", n);

	AudDrv_Clk_Off();

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static ssize_t mt_soc_dl_debug_read(struct file *file, char __user *buf,
				    size_t count, loff_t *pos)
{
	const int size = 4096;
	char buffer[size];
	int n = 0;

	pr_notice("mt_soc_debug_read\n");
	AudDrv_Clk_On();

	n = scnprintf(buffer, size - n, "mt_soc_dl_debug_read\n");

	n += scnprintf(buffer + n, size - n, "Afe_Mem_Pwr_on =0x%x\n",
		       Afe_Mem_Pwr_on);
	n += scnprintf(buffer + n, size - n, "Aud_AFE_Clk_cntr = 0x%x\n",
		       Aud_AFE_Clk_cntr);
	n += scnprintf(buffer + n, size - n, "Aud_ANA_Clk_cntr = 0x%x\n",
		       Aud_ANA_Clk_cntr);
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON0  = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON1  = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_DAC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_DAC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON  = 0x%x\n",
		       Afe_Get_Reg(AFE_I2S_CON));
	n += scnprintf(buffer + n, size - n, "AFE_CONN0  = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN1  = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN2  = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_CONN3  = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN3));
	n += scnprintf(buffer + n, size - n, "AFE_CONN4  = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN4));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_I2S_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON2  = 0x%x\n",
		       Afe_Get_Reg(AFE_I2S_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_BASE  = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_CUR  = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_END  = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_END));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CON= 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_CON));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON0));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON1));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON2));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON3 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON3));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON4 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON4));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_TOP_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_DL_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_SRC_DEBUG));
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_SRC_DEBUG_MON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON0));
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_SRC_DEBUG_MON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_NEWIF_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_TOP_CON0));

	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CON));	/* ccc */
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_STATUS = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_STATUS));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CLR = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_CLR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CNT1 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_CNT1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ1_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_EN_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ1_EN_CNT_MON));

	pr_notice("mt_soc_dl_debug_read len = %d\n", n);

	AudDrv_Clk_Off();
	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static const struct file_operations mtaudio_debug_ops = {
	.open = mt_soc_debug_open,
	.read = mt_soc_debug_read,
};

static const struct file_operations mtaudio_dl_debug_ops = {
	.open = mt_soc_debug_open,
	.read = mt_soc_dl_debug_read,
};
#endif
static int ariel_jack_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	int ret;

	/* Disable folowing pins at init time for power save
	 * They will be enabled if is requared later */
	snd_soc_dapm_disable_pin(&codec->dapm, "AMP_HPL");
	snd_soc_dapm_disable_pin(&codec->dapm, "AMP_HPR");
	snd_soc_dapm_disable_pin(&codec->dapm, "AMP_MIC");

	snd_soc_dapm_sync(&codec->dapm);

	max97236_set_key_div(codec, MCLK_RATE);

	ret = snd_soc_jack_new(codec, "h2w",
			       SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2
			       | SND_JACK_BTN_3 | SND_JACK_BTN_4 |
			       SND_JACK_BTN_5 | SND_JACK_HEADSET |
			       SND_JACK_LINEOUT, &ariel_jack);
	if (ret) {
		dev_err(codec->dev, "Failed to create jack: %d\n", ret);
		return ret;
	}

	snd_jack_set_key(ariel_jack.jack, SND_JACK_BTN_0, KEY_MEDIA);

	max97236_set_jack(codec, &ariel_jack);

	max97236_detect_jack(codec);

	return 0;
}

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt_soc_dai_common[] = {
	/* FrontEnd DAI Links */
	{
	 .name = "MultiMedia1",
	 .stream_name = MT_SOC_DL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DAI_NAME,
	 .platform_name = MT_SOC_DL1_PCM,
	 .codec_dai_name = MT_SOC_CODEC_TXDAI_NAME,
	 .codec_name = MT_SOC_CODEC_NAME,
	 },
	{
	 .name = "MultiMedia2",
	 .stream_name = MT_SOC_UL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DAI_NAME,
	 .platform_name = MT_SOC_UL1_PCM,
	 .codec_dai_name = MT_SOC_CODEC_RXDAI_NAME,
	 .codec_name = MT_SOC_CODEC_NAME,
	 },
	{
	 .name = "HDMI",
	 .stream_name = MT_SOC_HDMI_PLAYBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_HDMI_CPU_DAI_NAME,
	 .platform_name = MT_SOC_HDMI_PLATFORM_NAME,
	 .codec_dai_name = MT_SOC_HDMI_CODEC_DAI_NAME,
	 .codec_name = MT_SOC_CODEC_STUB_NAME,
	 },
	{
	 .name = "BTSCO",
	 .stream_name = MT_SOC_BTSCO_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_BTSCO_CPU_DAI_NAME,
	 .platform_name = MT_SOC_BTSCO,
	 .codec_dai_name = MT_SOC_CODEC_BTSCODAI_NAME,
	 .codec_name = MT_SOC_CODEC_STUB_NAME,
	 },
	{
	 .name = "DL1AWB_CAPTURE",
	 .stream_name = MT_SOC_DL1_AWB_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_STUB_CPU_DAI_NAME,
	 .platform_name = MT_SOC_DL1_AWB_NAME,
	 .codec_dai_name = MT_SOC_DL1_AWB_CODEC_DAI_NAME,
	 .codec_name = MT_SOC_CODEC_STUB_NAME,
	 },
	{
	 .name = "PLATOFRM_CONTROL",
	 .stream_name = MT_SOC_ROUTING_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_STUB_CPU_DAI_NAME,
	 .platform_name = MT_SOC_ROUTING_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 .no_pcm = 1,
	 },
	{
	 .name = "max97236-hpamp",
	 .stream_name = "max97236",
	 .cpu_dai_name = MT_SOC_STUB_CPU_DAI_NAME,
	 .platform_name = MT_SOC_ROUTING_PCM,
	 .codec_dai_name = "max97236-hifi",
	 .codec_name = "max97236.2-0040",
	 .init = &ariel_jack_init,
	 .no_pcm = 1,
	 },
};
static const char *const mt_soc_ariel_channel_cap[] = {
	"Stereo", "MonoLeft", "MonoRight"
};

static int mt_soc_ariel_channel_cap_set(struct snd_kcontrol *kcontrol,
							   struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt_soc_ariel_channel_cap_get(struct snd_kcontrol *kcontrol,
							   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct mt_audio_platform_data *pdata = dev_get_platdata(card->dev);

	ucontrol->value.integer.value[0] = pdata->mt_audio_board_channel_type;
	return 0;
}

static const struct soc_enum mt_soc_ariel_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt_soc_ariel_channel_cap),
	mt_soc_ariel_channel_cap),
};

static const struct snd_kcontrol_new mt_soc_controls[] = {
	SOC_ENUM_EXT("Board Channel Config", mt_soc_ariel_enum[0],
	mt_soc_ariel_channel_cap_get,
	mt_soc_ariel_channel_cap_set),

};

static struct snd_soc_card snd_soc_card_mt = {
	.name = "mt-snd-card",
	.dai_link = mt_soc_dai_common,
	.num_links = ARRAY_SIZE(mt_soc_dai_common),
	.controls = mt_soc_controls,
	.num_controls = ARRAY_SIZE(mt_soc_controls),
};

static int mtk_soc_machine_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_mt;
	struct mt_audio_platform_data *pdata;
	int ret;

	pr_notice("%s dev name %s\n", __func__, dev_name(&pdev->dev));

	card->dev = &pdev->dev;

	ret = snd_soc_register_card(card);
	if (ret) {
		pr_err("%s snd_soc_register_card fail %d\n", __func__, ret);
		return ret;
	}

#ifdef CONFIG_MT_ENG_BUILD
	mt_sco_audio_debugfs =
	    debugfs_create_file(DEBUG_FS_NAME, S_IFREG | S_IRUGO, NULL,
				(void *)DEBUG_FS_NAME, &mtaudio_debug_ops);

	mt_sco_dl_audio_debugfs = debugfs_create_file(DEBUG_DL_FS_NAME,
						      S_IFREG | S_IRUGO, NULL,
						      (void *)DEBUG_DL_FS_NAME,
						      &mtaudio_dl_debug_ops);
#endif

	/* Getting platform data */
	pdata = dev_get_platdata(&pdev->dev);
	if (pdata)
		InitAudioGpioData(&pdata->gpio_data);

	return 0;
}

static int mtk_soc_machine_dev_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	snd_soc_unregister_card(card);

#ifdef CONFIG_MT_ENG_BUILD
	debugfs_remove(mt_sco_audio_debugfs);
	debugfs_remove(mt_sco_dl_audio_debugfs);
#endif

	return 0;
}

static struct platform_driver mtk_soc_machine_driver = {
	.driver = {
		   .name = MT_SOC_MACHINE_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
		   },
	.probe = mtk_soc_machine_dev_probe,
	.remove = mtk_soc_machine_dev_remove,
};

static int __init mtk_soc_machine_driver_init(void)
{
	return platform_driver_register(&mtk_soc_machine_driver);
}
late_initcall(mtk_soc_machine_driver_init);

static void __exit mtk_soc_machine_driver_exit(void)
{
	platform_driver_unregister(&mtk_soc_machine_driver);
}
module_exit(mtk_soc_machine_driver_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC driver for mtxxxx");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt-snd-card");
