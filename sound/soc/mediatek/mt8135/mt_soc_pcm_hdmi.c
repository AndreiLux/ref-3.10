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

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_hdmi_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_hdmi_def.h"
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <sound/soc.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <linux/pid.h>

static struct pcm_afe_info *hdmi_pcm_info;
struct AFE_MEM_CONTROL_T afe_hdmi_control_context;
static pid_t pid_old;

static void mtk_hdmi_set_interconnection(unsigned int connection_state, unsigned int channels);


static struct snd_pcm_hardware mtk_hdmi_pcm_hardware = {
	.info =
	    (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_RESUME |
	     SNDRV_PCM_INFO_MMAP_VALID),
	.formats = HDMI_FORMATS,
	.rates = HDMI_RATES,
	.rate_min = HDMI_RATE_MIN,
	.rate_max = HDMI_RATE_MAX,
	.channels_min = HDMI_CHANNELS_MIN,
	.channels_max = HDMI_CHANNELS_MAX,
	.buffer_bytes_max = HDMI_BUFFER_SIZE_MAX,
	.period_bytes_max = HDMI_PERIOD_SIZE_MAX,
	.periods_min = HDMI_PERIODS_MIN,
	.periods_max = HDMI_PERIODS_MAX,
	.fifo_size = 0,
};

void Auddrv_HDMI_Interrupt_Handler(void)
{
	kal_int32 Afe_consumed_bytes = 0;
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	struct AFE_BLOCK_T * const Afe_Block = &(afe_hdmi_control_context.rBlock);

	HW_Cur_ReadIdx = Afe_Get_Reg(AFE_HDMI_OUT_CUR);
	if (HW_Cur_ReadIdx == 0) {
		pr_notice("%s HW_Cur_ReadIdx = 0\n", __func__);
		HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
	}

	HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

	/*
	pr_info("%s HW_Cur_ReadIdx = 0x%x HW_memory_index = 0x%x pucPhysBufAddr = 0x%x\n",
		 __func__, HW_Cur_ReadIdx, HW_memory_index, Afe_Block->pucPhysBufAddr);
	*/

	/* get hw consume bytes */
	if (HW_memory_index > Afe_Block->u4DMAReadIdx) {
		Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
	} else {
		Afe_consumed_bytes =
		    Afe_Block->u4BufferSize + HW_memory_index - Afe_Block->u4DMAReadIdx;
	}

	if ((Afe_consumed_bytes & 0x1f) != 0)
		pr_warn("%s DMA address is not aligned 32 bytes\n", __func__);

	/*
	pr_info("%s ReadIdx:%x Afe_consumed_bytes:%x HW_memory_index:%x\n",
		__func__, Afe_Block->u4DMAReadIdx, Afe_consumed_bytes, HW_memory_index);
	*/

	Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
	Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;

	snd_pcm_period_elapsed(hdmi_pcm_info->substream);
}

static void mtk_hdmi_set_interconnection(unsigned int connection_state, unsigned int channels)
{
	/* O20~O27: L/R/LS/RS/C/LFE/CH7/CH8 */
	switch (channels) {
	case 8:
		SetConnection(connection_state, Soc_Aud_InterConnectionInput_I26,
			      Soc_Aud_InterConnectionOutput_O26);
		SetConnection(connection_state, Soc_Aud_InterConnectionInput_I27,
			      Soc_Aud_InterConnectionOutput_O27);
	case 6:
		SetConnection(connection_state, Soc_Aud_InterConnectionInput_I24,
			      Soc_Aud_InterConnectionOutput_O22);
		SetConnection(connection_state, Soc_Aud_InterConnectionInput_I25,
			      Soc_Aud_InterConnectionOutput_O23);
	case 4:
		SetConnection(connection_state, Soc_Aud_InterConnectionInput_I22,
			      Soc_Aud_InterConnectionOutput_O24);
		SetConnection(connection_state, Soc_Aud_InterConnectionInput_I23,
			      Soc_Aud_InterConnectionOutput_O25);
	case 2:
		SetConnection(connection_state, Soc_Aud_InterConnectionInput_I20,
			      Soc_Aud_InterConnectionOutput_O20);
		SetConnection(connection_state, Soc_Aud_InterConnectionInput_I21,
			      Soc_Aud_InterConnectionOutput_O21);
		break;
	case 1:
		SetConnection(connection_state, Soc_Aud_InterConnectionInput_I20,
			      Soc_Aud_InterConnectionOutput_O20);
		break;
	default:
		pr_warn("%s unsupported channels %u\n", __func__, channels);
		break;
	}
}

static int mtk_hdmi_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct pcm_afe_info *prtd = NULL;
	int ret = 0;
	struct task_struct *tsk;
	pid_t current_pid = 0;

	prtd = kzalloc(sizeof(struct pcm_afe_info), GFP_KERNEL);
	if (prtd == NULL) {
		pr_err("%s failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	prtd->substream = substream;
	runtime->private_data = prtd;
	hdmi_pcm_info = prtd;

	snd_soc_set_runtime_hwparams(substream, &mtk_hdmi_pcm_hardware);

	memset(&afe_hdmi_control_context, 0, sizeof(afe_hdmi_control_context));

	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		pr_err("%s snd_pcm_hw_constraint_integer fail %d\n", __func__, ret);
		return ret;
	}

	pr_info("%s runtime rate = %d channels = %d substream->pcm->device = %d\n",
		  __func__, runtime->rate, runtime->channels, substream->pcm->device);

	SetDeepIdleEnableForHdmi(false);
	current_pid = task_pid_nr(current);
	if (pid_old != current_pid) {
		tsk = find_task_by_vpid(1);
		if (tsk) {
			cgroup_attach_task_all(tsk, current);
			pid_old = current_pid;
		}
	}

	return 0;
}

static int mtk_hdmi_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct pcm_afe_info *prtd = runtime->private_data;
	kfree(prtd);
	hdmi_pcm_info = NULL;

	SetDeepIdleEnableForHdmi(true);

	return 0;
}

static int mtk_hdmi_pcm_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct AFE_BLOCK_T *block = &afe_hdmi_control_context.rBlock;
	int ret = 0;

	memset(block, 0, sizeof(struct AFE_BLOCK_T));

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));

	if (ret >= 0) {
		block->pucVirtBufAddr = runtime->dma_area;
		block->pucPhysBufAddr = runtime->dma_addr;
		block->u4BufferSize = runtime->dma_bytes;
	} else {
		pr_err("%s snd_pcm_lib_malloc_pages fail %d\n", __func__, ret);
	}

	pr_info("%s dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
		__func__, runtime->dma_bytes, runtime->dma_area, runtime->dma_addr);
	return ret;
}

static int mtk_hdmi_pcm_hw_free(struct snd_pcm_substream *substream)
{
	memset(&afe_hdmi_control_context, 0, sizeof(afe_hdmi_control_context));
	return snd_pcm_lib_free_pages(substream);
}

static int mtk_hdmi_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	pr_info("%s rate = %u  channels = %u period_size = %lu\n",
		  __func__, runtime->rate, runtime->channels, runtime->period_size);
	return 0;
}

static int mtk_hdmi_pcm_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_info("%s period_size = %lu\n", __func__, runtime->period_size);

	/* enable audio clock */
	AudDrv_Clk_On();
	set_hdmi_clock_source(runtime->rate);
	AudDrv_APLL_TUNER_Clk_On();
	AudDrv_HDMI_Clk_On();

	Afe_Set_Reg(AFE_HDMI_OUT_BASE, runtime->dma_addr, 0xffffffff);
	Afe_Set_Reg(AFE_HDMI_OUT_END, runtime->dma_addr + (runtime->dma_bytes - 1), 0xffffffff);

	/* here to set interrupt */
	SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE, runtime->period_size);
	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE, true);

	/* interconnection */
	mtk_hdmi_set_interconnection(Soc_Aud_InterCon_Connection, runtime->channels);

	/* HDMI Out control */
	set_hdmi_out_control(runtime->channels);
	set_hdmi_out_control_enable(true);

	/* HDMI I2S */
	set_hdmi_i2s();
	set_hdmi_bck_div(runtime->rate);
	set_hdmi_i2s_enable(true);

	SetHdmiPathEnable(true);
	EnableAfe(true);
	return 0;
}

static int mtk_hdmi_pcm_stop(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_info("%s\n", __func__);

	mtk_hdmi_set_interconnection(Soc_Aud_InterCon_DisConnect, runtime->channels);

	set_hdmi_i2s_enable(false);

	set_hdmi_out_control_enable(false);

	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE, false);

	SetHdmiPathEnable(false);

	EnableAfe(false);

	/* clean audio hardware buffer */
	{
		struct AFE_BLOCK_T *afe_block = &(afe_hdmi_control_context.rBlock);
		memset(afe_block->pucVirtBufAddr, 0, afe_block->u4BufferSize);
		afe_block->u4DMAReadIdx = 0;
		afe_block->u4WriteIdx = 0;
		afe_block->u4DataRemained = 0;
	}

	AudDrv_HDMI_Clk_Off();
	AudDrv_APLL_TUNER_Clk_Off();
	AudDrv_Clk_Off();

	return 0;
}

static int mtk_hdmi_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_info("%s cmd = %d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_hdmi_pcm_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_hdmi_pcm_stop(substream);
	default:
		pr_warn("%s command %d not handled\n", __func__, cmd);
		break;
	}

	return -EINVAL;
}

static snd_pcm_uframes_t mtk_hdmi_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct AFE_BLOCK_T *afe_block = &(afe_hdmi_control_context.rBlock);
	return bytes_to_frames(runtime, afe_block->u4DMAReadIdx);
}

static struct snd_pcm_ops mtk_hdmi_pcm_ops = {
	.open = mtk_hdmi_pcm_open,
	.close = mtk_hdmi_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_hdmi_pcm_hw_params,
	.hw_free = mtk_hdmi_pcm_hw_free,
	.prepare = mtk_hdmi_pcm_prepare,
	.trigger = mtk_hdmi_pcm_trigger,
	.pointer = mtk_hdmi_pcm_pointer,
};

static struct snd_soc_platform_driver mtk_soc_hdmi_platform = {
	.ops = &mtk_hdmi_pcm_ops,
};

static int mtk_hdmi_probe(struct platform_device *pdev)
{
	pr_notice("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	InitAfeControl();

	return snd_soc_register_platform(&pdev->dev, &mtk_soc_hdmi_platform);
}

static int mtk_hdmi_remove(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_hdmi_driver = {
	.driver = {
		   .name = MT_SOC_HDMI_PLATFORM_NAME,
		   .owner = THIS_MODULE,
		   },
	.probe = mtk_hdmi_probe,
	.remove = mtk_hdmi_remove,
};

module_platform_driver(mtk_hdmi_driver);

MODULE_DESCRIPTION("AFE HDMI module platform driver");
MODULE_LICENSE("GPL");
