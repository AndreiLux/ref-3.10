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
#include <sound/soc.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <linux/pid.h>


/* information about */
static pid_t pid_old;

/*
 *    function implementation
 */
static int mtk_btsco_probe(struct platform_device *pdev);
static int mtk_btsco_pcm_close(struct snd_pcm_substream *substream);

/* defaults */
#define BTSCO_MAX_BUFFER_SIZE       (16*1024)
#define BTSCO_CAPTURE_MAX_BUFFER_SIZE       (2048)
#define BTSCO_MIN_PERIOD_SIZE       64
#define BTSCO_MAX_PERIOD_SIZE       BTSCO_MAX_BUFFER_SIZE
#define BTSCO_CAPTURE_MAX_PERIOD_SIZE       BTSCO_CAPTURE_MAX_BUFFER_SIZE
#define BTSCO_FORMATS         (SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE)
#define BTSCO_RATE            SNDRV_PCM_RATE_8000
#define BTSCO_RATE_MIN        8000
#define BTSCO_RATE_MAX        8000
#define BTSCO_CHANNELS_MIN    1
#define BTSCO_CHANNELS_MAX    2
#define BTSCO_CAPTURE_CHANNELS_MAX    1
#define BTSCO_PERIODS_MIN     1
#define BTSCO_PERIODS_MAX     16
#define BTSCO_CAPTURE_PERIODS_MAX     4

static struct snd_pcm_hardware mtk_btsco_pcm_hardware = {
	.info =
	    (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_RESUME |
	     SNDRV_PCM_INFO_MMAP_VALID),
	.formats = BTSCO_FORMATS,
	.rates = BTSCO_RATE,
	.rate_min = BTSCO_RATE_MIN,
	.rate_max = BTSCO_RATE_MAX,
	.channels_min = BTSCO_CHANNELS_MIN,
	.channels_max = BTSCO_CHANNELS_MAX,
	.buffer_bytes_max = BTSCO_MAX_BUFFER_SIZE,
	.period_bytes_max = BTSCO_MAX_PERIOD_SIZE,
	.periods_min = BTSCO_PERIODS_MIN,
	.periods_max = BTSCO_PERIODS_MAX,
	.fifo_size = 0,
};

static struct snd_pcm_hardware mtk_btsco_capture_hardware = {
	.info =
	    (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_RESUME |
	     SNDRV_PCM_INFO_MMAP_VALID),
	.formats = BTSCO_FORMATS,
	.rates = BTSCO_RATE,
	.rate_min = BTSCO_RATE_MIN,
	.rate_max = BTSCO_RATE_MAX,
	.channels_min = BTSCO_CHANNELS_MIN,
	.channels_max = BTSCO_CAPTURE_CHANNELS_MAX,
	.buffer_bytes_max = BTSCO_CAPTURE_MAX_BUFFER_SIZE,
	.period_bytes_max = BTSCO_CAPTURE_MAX_PERIOD_SIZE,
	.periods_min = BTSCO_PERIODS_MIN,
	.periods_max = BTSCO_CAPTURE_PERIODS_MAX,
	.fifo_size = 0,
};

static int mtk_btsco_pcm_stop(struct snd_pcm_substream *substream)
{
	PRINTK_AUDDRV("AudDrv mtk_btsco_pcm_stop %d\n", substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* here to turn off digital part */
		SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05,
			      Soc_Aud_InterConnectionOutput_O02);
		SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06,
			      Soc_Aud_InterConnectionOutput_O02);
		SetDAIBTEnable(false);

		SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

		EnableAfe(false);

		/* here close audio clocks */
		/* AudDrv_Clk_Off(); */

		/* clean audio hardware buffer */
		Auddrv_ResetBuffer(Soc_Aud_Digital_Block_MEM_DL1);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/* set interconnection */
		SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I02,
			      Soc_Aud_InterConnectionOutput_O11);
		SetDAIBTEnable(false);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI, false);

		/* here to set interrupt */
		SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, false);
		EnableAfe(false);

		/* clean audio hardware buffer */
		Auddrv_ResetBuffer(Soc_Aud_Digital_Block_MEM_DAI);

	}
	return 0;
}

static snd_pcm_uframes_t mtk_btsco_pcm_pointer(struct snd_pcm_substream *substream)
{
	kal_int32 HwPtr = 0;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		HwPtr = Auddrv_UpdateHwPtr(Soc_Aud_Digital_Block_MEM_DL1, substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		HwPtr = Auddrv_UpdateHwPtr(Soc_Aud_Digital_Block_MEM_DAI, substream);

	/* PRINTK_AUDDRV("Aud mt_soc_pcm_btsco callback mtk_btsco_pcm_pointer , return= 0x%x\n",HwPtr); */
	return HwPtr;
}


static void mtk_btsco_set_dai_buffer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct AFE_BLOCK_T *pblock = &AFE_DAI_Control_context.rBlock;

	pblock->pucPhysBufAddr = runtime->dma_addr;
	pblock->pucVirtBufAddr = runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;

	Afe_Set_Reg(AFE_DAI_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_DAI_END, pblock->pucPhysBufAddr + (runtime->dma_bytes - 1), 0xffffffff);
}

static int mtk_btsco_pcm_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;
	size_t buffer_size = params_buffer_bytes(hw_params);
	PRINTK_AUDDRV("Aud mt_soc_pcm_btsco callback mtk_btsco_pcm_hw_params %d\n",
	     substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (buffer_size > AFE_INTERNAL_SRAM_SIZE) {
			pr_warn("%s request size %d > max size %d\n", __func__,
				buffer_size, AFE_INTERNAL_SRAM_SIZE);
		}
		AudDrv_Allocate_DL1_Buffer(AFE_INTERNAL_SRAM_SIZE);

		substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE;
		substream->runtime->dma_area = (unsigned char *)AFE_SRAM_ADDRESS;
		substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;
		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;
		dma_buf->private_data = NULL;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;

		ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
		if (ret >= 0)
			mtk_btsco_set_dai_buffer(substream);
		else
			pr_err("%s snd_pcm_lib_malloc_pages fail %d\n", __func__, ret);
	}

	PRINTK_AUDDRV("%s dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
		      __func__, runtime->dma_bytes, runtime->dma_area, runtime->dma_addr);
	return ret;
}

static int mtk_btsco_pcm_hw_free(struct snd_pcm_substream *substream)
{
	PRINTK_AUDDRV("Aud mt_soc_pcm_btsco callback  mtk_btsco_pcm_hw_free %d\n",
		      substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* SRAM no need free */
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		memset(&AFE_DAI_Control_context, 0, sizeof(AFE_DAI_Control_context));
		AFE_DAI_Control_context.MemIfNum = Soc_Aud_Digital_Block_MEM_DAI;
		snd_pcm_lib_free_pages(substream);

	}
	return 0;
}

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	8000
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static int mtk_btsco_pcm_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct pcm_afe_info *prtd = NULL;
	struct task_struct *tsk;
	pid_t current_pid = 0;

	PRINTK_AUDDRV("Aud mt_soc_pcm_btsco callback mtk_btsco_pcm_open %d\n", substream->stream);


	prtd = kzalloc(sizeof(struct pcm_afe_info), GFP_KERNEL);
	if (prtd == NULL) {
		pr_err("%s failed to allocate memory\n", __func__);
		return -ENOMEM;
	} else {
		PRINTK_AUDDRV("AudDrvprtd %x\n", (unsigned int)prtd);
	}
	prtd->substream = substream;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		Auddrv_DL_SetPlatformInfo(Soc_Aud_Digital_Block_MEM_DL1, substream);
		snd_soc_set_runtime_hwparams(substream, &mtk_btsco_pcm_hardware);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		prtd->pAfe_MEM_Control = &AFE_DAI_Control_context;
		Auddrv_DL_SetPlatformInfo(Soc_Aud_Digital_Block_MEM_DAI, substream);
		snd_soc_set_runtime_hwparams(substream, &mtk_btsco_capture_hardware);
	}
	runtime->private_data = prtd;

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_err("snd_pcm_hw_constraint_integer failed\n");

	/* here open audio clocks */
	AudDrv_Clk_On();

	/* print for hw pcm information */
	PRINTK_AUDDRV
	    ("mtk_btsco_pcm_open runtime rate = %d channels = %d substream->pcm->device = %d\n",
	     runtime->rate, runtime->channels, substream->pcm->device);

	PRINTK_AUDDRV("AudDrv mtk_btsco_pcm_open return\n");

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		current_pid = task_pid_nr(current);
		if (pid_old != current_pid) {
			tsk = find_task_by_vpid(1);
			if (tsk) {
				cgroup_attach_task_all(tsk, current);
				pr_info("%s change to root! (pid %d -> %d)\n", __func__, pid_old, current_pid);
				pid_old = current_pid;
			}
		}
	}

	SetDeepIdleEnableForAfe(false);

	return 0;
}

static int mtk_btsco_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct pcm_afe_info *prtd = runtime->private_data;

	PRINTK_AUDDRV("Aud mt_soc_pcm_btsco callback mtk_btsco_pcm_close %d\n", substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		Auddrv_DL_ResetPlatformInfo(Soc_Aud_Digital_Block_MEM_DL1);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		Auddrv_DL_ResetPlatformInfo(Soc_Aud_Digital_Block_MEM_DAI);
	/* korocheck afe capture close will also call  mtk_capture_alsa_stop(substream);   need? */
	AudDrv_Clk_Off();
	kfree(prtd);
	SetDeepIdleEnableForAfe(true);
	return 0;
}

static int mtk_btsco_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtimeStream = substream->runtime;
	/* struct pcm_afe_info *prtd = NULL; */
	struct task_struct *tsk;
	pid_t current_pid = 0;

	PRINTK_AUDDRV("Aud mt_soc_pcm_btsco callback mtk_btsco_pcm_prepare %d\n",
		      substream->stream);
	PRINTK_AUDDRV("mtk_alsa_prepare rate = %u  channels = %u period_size = %lu\n",
		      runtimeStream->rate, runtimeStream->channels, runtimeStream->period_size);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		current_pid = task_pid_nr(current);
		if (pid_old != current_pid) {
			tsk = find_task_by_vpid(1);
			if (tsk) {
				cgroup_attach_task_all(tsk, current);
				pr_info("%s change to root! (pid %d -> %d)\n", __func__, pid_old, current_pid);
				pid_old = current_pid;
			}
		}
	}
	return 0;
}

static int mtk_btsco_pcm_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct AudioIrqMcuMode IrqStatus;
	PRINTK_AUDDRV
	    ("mtk_btsco_pcm_start %d period = %lu,runtime->rate= %u, runtime->channels=%u\n",
	     substream->stream, runtime->period_size, runtime->rate, runtime->channels);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
			      Soc_Aud_InterConnectionOutput_O02);
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
			      Soc_Aud_InterConnectionOutput_O02);
		SetConnection(Soc_Aud_InterCon_ConnectionShift, Soc_Aud_InterConnectionInput_I05,
			      Soc_Aud_InterConnectionOutput_O02);
		SetConnection(Soc_Aud_InterCon_ConnectionShift, Soc_Aud_InterConnectionInput_I06,
			      Soc_Aud_InterConnectionOutput_O02);

		SetDAIBTAttribute();
		SetDAIBTEnable(true);
		EnableAfe(true);

		/* set btsco sample ratelimit_state */
		SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
		SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);

		/* here to set interrupt */
		SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->period_size);
		SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->rate);

		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);
		SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, true);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		SetDAIBTAttribute();
		SetDAIBTEnable(true);
		SetSampleRate(Soc_Aud_Digital_Block_MEM_DAI, runtime->rate);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI, true);

		/* here to set interrupt */
		GetIrqStatus(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, &IrqStatus);

		if (IrqStatus.mStatus == false) {
			SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, runtime->period_size);
			SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, runtime->rate);
			SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, true);
		} else {
			PRINTK_AUDDRV
			    ("IRQ2_MCU_MODE is enabled , use original irq2 interrupt mode");
		}
		/* set interconnection */
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I02,
			      Soc_Aud_InterConnectionOutput_O11);
		EnableAfe(true);
	}
	/* Afe_Log_Print(); */
	return 0;
}

static int mtk_btsco_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	PRINTK_AUDDRV("Aud mt_soc_pcm_btsco callback mtk_btsco_pcm_trigger %d cmd =%d\n",
	     substream->stream, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_btsco_pcm_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_btsco_pcm_stop(substream);
	}
	return -EINVAL;
}

static struct snd_pcm_ops mtk_btsco_pcm_ops = {
	.open = mtk_btsco_pcm_open,
	.close = mtk_btsco_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_btsco_pcm_hw_params,
	.hw_free = mtk_btsco_pcm_hw_free,
	.prepare = mtk_btsco_pcm_prepare,
	.trigger = mtk_btsco_pcm_trigger,
	.pointer = mtk_btsco_pcm_pointer,
};

static struct snd_soc_platform_driver mtk_soc_btsco_platform = {
	.ops = &mtk_btsco_pcm_ops,
};

static int mtk_btsco_probe(struct platform_device *pdev)
{
	pr_notice("%s dev name %s\n", __func__, dev_name(&pdev->dev));
	InitAfeControl();
	/* AudDrv_Clk_On  (); */
	/* AudDrv_ANA_Clk_On(); */
	return snd_soc_register_platform(&pdev->dev, &mtk_soc_btsco_platform);
}

static int mtk_btsco_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_btsco_driver = {
	.driver = {
		   .name = MT_SOC_BTSCO,
		   .owner = THIS_MODULE,
		   },
	.probe = mtk_btsco_probe,
	.remove = mtk_btsco_remove,
};

module_platform_driver(mtk_btsco_driver);

MODULE_DESCRIPTION("AFE BTSCO module platform driver");
MODULE_LICENSE("GPL");
