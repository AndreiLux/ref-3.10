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
#include "mt_soc_digital_type.h"
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <sound/soc.h>

/* defaults */
#define MAX_BUFFER_SIZE     (16*1024)
#define MIN_PERIOD_SIZE     64
#define MAX_PERIOD_SIZE     MAX_BUFFER_SIZE
#define USE_FORMATS         (SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE)
#define USE_RATE            (SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000)
#define USE_RATE_MIN        8000
#define USE_RATE_MAX        48000
#define USE_CHANNELS_MIN    1
#define USE_CHANNELS_MAX    2

static struct snd_pcm_hardware mtk_dl1_awb_hardware = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = USE_FORMATS,
	.rates = USE_RATE,
	.rate_min = USE_RATE_MIN,
	.rate_max = USE_RATE_MAX,
	.channels_min = USE_CHANNELS_MIN,
	.channels_max = USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = 1,
	.periods_max = 4096,
	.fifo_size = 0,
};

/* Conventional and unconventional sample rate supported */
static const unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static int mtk_dl1_awb_pcm_close(struct snd_pcm_substream *substream);

static int mtk_dl1_awb_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct pcm_afe_info *prtd = NULL;
	int ret = 0;

	prtd = kzalloc(sizeof(struct pcm_afe_info), GFP_KERNEL);
	if (prtd == NULL) {
		pr_err("%s failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	prtd->substream = substream;
	runtime->private_data = prtd;

	snd_soc_set_runtime_hwparams(substream, &mtk_dl1_awb_hardware);
	Auddrv_DL_SetPlatformInfo(Soc_Aud_Digital_Block_MEM_AWB, substream);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	if (unlikely(ret < 0))
		pr_err("snd_pcm_hw_constraint_list failed: 0x%x\n", ret);

	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		pr_err("%s snd_pcm_hw_constraint_integer fail %d\n", __func__, ret);

	/* here open audio clocks */
	AudDrv_Clk_On();

	pr_info("%s runtime rate = %d channels = %d substream->pcm->device = %d\n",
		  __func__, runtime->rate, runtime->channels, substream->pcm->device);
	if (unlikely(ret < 0)) {
		pr_err("%s mtk_capture_pcm_close\n", __func__);
		mtk_dl1_awb_pcm_close(substream);
		return ret;
		}

	SetDeepIdleEnableForAfe(false);

	return 0;
}

static int mtk_dl1_awb_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct pcm_afe_info *prtd = runtime->private_data;
	kfree(prtd);

	Auddrv_DL_ResetPlatformInfo(Soc_Aud_Digital_Block_MEM_AWB);
	AudDrv_Clk_Off();

	SetDeepIdleEnableForAfe(true);

	return 0;
}

static void mtk_dl1_awb_set_awb_buffer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct AFE_BLOCK_T *pblock = &AFE_AWB_Control_context.rBlock;

	pblock->pucPhysBufAddr = runtime->dma_addr;
	pblock->pucVirtBufAddr = runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;

	Afe_Set_Reg(AFE_AWB_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_AWB_END, pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1), 0xffffffff);
}


static int mtk_dl1_awb_pcm_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));

	if (likely(ret >= 0))
		mtk_dl1_awb_set_awb_buffer(substream);
	else
		pr_err("%s snd_pcm_lib_malloc_pages fail %d\n", __func__, ret);

	pr_info("%s dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
		__func__, runtime->dma_bytes, runtime->dma_area, runtime->dma_addr);
	return ret;
}

static int mtk_dl1_awb_pcm_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int mtk_dl1_awb_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	pr_info("%s rate = %u  channels = %u period_size = %lu\n",
		  __func__, runtime->rate, runtime->channels, runtime->period_size);
	return 0;
}

static int mtk_dl1_awb_alsa_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct AudioIrqMcuMode IrqStatus;
	struct timespec curr_tstamp;

	pr_info("%s period_size = %lu\n", __func__, runtime->period_size);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_AWB, substream->runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_AWB, ((substream->runtime->channels == 2)?0:1));
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB, true);


	/* here to set interrupt */
	GetIrqStatus(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, &IrqStatus);

	if (likely(!IrqStatus.mStatus)) {
		SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE,
				 substream->runtime->period_size);
		SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, substream->runtime->rate);
		SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, true);
		snd_pcm_gettime(substream->runtime, (struct timespec *)&curr_tstamp);
		pr_debug("%s curr_tstamp %ld %ld\n", __func__, curr_tstamp.tv_sec, curr_tstamp.tv_nsec);

	} else {
		pr_debug("%s IRQ2_MCU_MODE is enabled , use original irq2 interrupt mode\n", __func__);
	}

	/* interconnection */
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O05);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O06);

	EnableAfe(true);
	return 0;
}

static int mtk_dl1_awb_alsa_stop(struct snd_pcm_substream *substream)
{

	pr_info("%s\n", __func__);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB, false);

	/* here to set interrupt */
	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, false);

	/* here to turn off digital part */
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O05);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O06);

	EnableAfe(false);

	/* clean audio hardware buffer */
	Auddrv_ResetBuffer(Soc_Aud_Digital_Block_MEM_AWB);

	return 0;
}

static int mtk_dl1_awb_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_info("%s cmd = %d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_dl1_awb_alsa_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_dl1_awb_alsa_stop(substream);
	default:
		pr_warn("%s command %d not handled\n", __func__, cmd);
		break;
	}

	return -EINVAL;
}

static snd_pcm_uframes_t mtk_dl1_awb_pcm_pointer(struct snd_pcm_substream *substream)
{
	kal_int32 HwPtr;

	HwPtr = Auddrv_UpdateHwPtr(Soc_Aud_Digital_Block_MEM_AWB, substream);

	return HwPtr;

}

static struct snd_pcm_ops mtk_dl1_awb_pcm_ops = {
	.open = mtk_dl1_awb_pcm_open,
	.close = mtk_dl1_awb_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_dl1_awb_pcm_hw_params,
	.hw_free = mtk_dl1_awb_pcm_hw_free,
	.prepare = mtk_dl1_awb_pcm_prepare,
	.trigger = mtk_dl1_awb_pcm_trigger,
	.pointer = mtk_dl1_awb_pcm_pointer,
};

static struct snd_soc_platform_driver mtk_soc_dl1_awb_platform = {
	.ops = &mtk_dl1_awb_pcm_ops,
};

static int mtk_dl1_awb_probe(struct platform_device *pdev)
{
	pr_notice("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	InitAfeControl();

	return snd_soc_register_platform(&pdev->dev, &mtk_soc_dl1_awb_platform);
}

static int mtk_dl1_awb_remove(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_dl1_awb_driver = {
	.driver = {
		   .name = MT_SOC_DL1_AWB_NAME,
		   .owner = THIS_MODULE,
		   },
	.probe = mtk_dl1_awb_probe,
	.remove = mtk_dl1_awb_remove,
};

module_platform_driver(mtk_dl1_awb_driver);

MODULE_DESCRIPTION("AFE Loopback module platform driver");
MODULE_LICENSE("GPL");
