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
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <sound/soc.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <linux/pid.h>


/* information about */
static struct AudioDigtalI2S *mAudioDigitalI2S;
static pid_t pid_old;

/*
 *    function implementation
 */
static void StartAudioCaptureHardware(struct snd_pcm_substream *substream);
static void StopAudioCaptureHardware(struct snd_pcm_substream *substream);
static int mtk_capture_probe(struct platform_device *pdev);
static int mtk_capture_pcm_close(struct snd_pcm_substream *substream);
static int mtk_asoc_capture_pcm_new(struct snd_soc_pcm_runtime *rtd);
static void mtk_asoc_capture_pcm_free(struct snd_pcm *pcm);

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
#define USE_PERIODS_MIN     1024
#define USE_PERIODS_MAX     (16*1024)

static struct snd_pcm_hardware mtk_capture_hardware = {
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

static void StopAudioCaptureHardware(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	SetI2SAdcEnable(false);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL, false);

	/* here to set interrupt */
	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, false);

	/* here to turn off digital part */
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O09);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I04,
		      Soc_Aud_InterConnectionOutput_O10);

	EnableAfe(false);

}

static void ConfigAdcI2S(struct snd_pcm_substream *substream)
{
	mAudioDigitalI2S->mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
	mAudioDigitalI2S->mBuffer_Update_word = 8;
	mAudioDigitalI2S->mFpga_bit_test = 0;
	mAudioDigitalI2S->mFpga_bit = 0;
	mAudioDigitalI2S->mloopback = 0;
	mAudioDigitalI2S->mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
	mAudioDigitalI2S->mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
	mAudioDigitalI2S->mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_16BITS;
	mAudioDigitalI2S->mI2S_SAMPLERATE = (substream->runtime->rate);
}

static void StartAudioCaptureHardware(struct snd_pcm_substream *substream)
{
	struct AudioIrqMcuMode IrqStatus;
	struct timespec curr_tstamp;
	pr_debug("%s\n", __func__);

	ConfigAdcI2S(substream);
	SetI2SAdcIn(mAudioDigitalI2S);
	/* EnableSideGenHw(Soc_Aud_InterConnectionInput_I03,Soc_Aud_MemIF_Direction_DIRECTION_INPUT,true); */

	SetI2SAdcEnable(true);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_VUL, substream->runtime->rate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL, true);

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
	/* here to turn off digital part */
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I03,
		      Soc_Aud_InterConnectionOutput_O09);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I04,
		      Soc_Aud_InterConnectionOutput_O10);

	EnableAfe(true);
}

static int mtk_capture_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct task_struct *tsk;
	pid_t current_pid = 0;
	pr_debug("%s\n", __func__);

	current_pid = task_pid_nr(current);
	if (pid_old != current_pid) {
		tsk = find_task_by_vpid(1);
		if (tsk) {
			cgroup_attach_task_all(tsk, current);
			pr_info("%s change to root! (pid %d -> %d)\n", __func__, pid_old, current_pid);
			pid_old = current_pid;
		}
	}
	return 0;
}

static int mtk_capture_alsa_stop(struct snd_pcm_substream *substream)
{
	struct AFE_BLOCK_T *Vul_Block = &(AFE_VUL_Control_context.rBlock);
	pr_debug("%s\n", __func__);
	StopAudioCaptureHardware(substream);
	/* AudDrv_Clk_Off(); */
	Vul_Block->u4DMAReadIdx = 0;
	Vul_Block->u4WriteIdx = 0;
	Vul_Block->u4DataRemained = 0;
	return 0;
}

static snd_pcm_uframes_t mtk_capture_pcm_pointer(struct snd_pcm_substream *substream)
{
	return Auddrv_UpdateHwPtr(Soc_Aud_Digital_Block_MEM_VUL, substream);
}


static void SetVULBuffer(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct AFE_BLOCK_T * const pblock = &AFE_VUL_Control_context.rBlock;

	pblock->pucPhysBufAddr = runtime->dma_addr;
	pblock->pucVirtBufAddr = runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;
	pr_debug("%s dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
		__func__, pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);
	/* set sram address top hardware */
	Afe_Set_Reg(AFE_VUL_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_VUL_END, pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1), 0xffffffff);

}

static int mtk_capture_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	pr_debug("%s\n", __func__);

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));

	if (likely(ret >= 0))
		SetVULBuffer(substream, hw_params);
	else
		pr_err("%s snd_pcm_lib_malloc_pages fail %d\n", __func__, ret);

	pr_debug("%s dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
		__func__, runtime->dma_bytes, runtime->dma_area, runtime->dma_addr);
	return ret;
}

static int mtk_capture_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_info("%s\n", __func__);
	return snd_pcm_lib_free_pages(substream);
}

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
};

static int mtk_capture_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct pcm_afe_info *prtd = NULL;
	struct task_struct *tsk;
	pid_t current_pid = 0;
	int ret = 0;

	pr_debug("%s\n", __func__);

	prtd = kzalloc(sizeof(struct pcm_afe_info), GFP_KERNEL);
	if (unlikely(!prtd)) {
		pr_err("%s failed to allocate memory for pcm_afe_info\n", __func__);
		return -ENOMEM;
	}

	prtd->substream = substream;
	runtime->private_data = prtd;
	snd_soc_set_runtime_hwparams(substream, &mtk_capture_hardware);
	prtd->pAfe_MEM_Control = &AFE_VUL_Control_context;
	runtime->private_data = prtd;
	Auddrv_DL_SetPlatformInfo(Soc_Aud_Digital_Block_MEM_VUL, substream);

	pr_debug("%s runtime->hw->rates = 0x%x mtk_capture_hardware = %p\n ",
		__func__, runtime->hw.rates, &mtk_capture_hardware);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	if (unlikely(ret < 0))
		pr_err("%s snd_pcm_hw_constraint_list failed %d\n", __func__, ret);

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	if (unlikely(ret < 0))
		pr_err("%s snd_pcm_hw_constraint_integer failed %d\n", __func__, ret);

	/* here open audio clocks */
	AudDrv_Clk_On();
	AudDrv_ANA_Clk_On();

	/* print for hw pcm information */
	pr_info("%s runtime rate = %u channels = %u device = %d\n",
		__func__, runtime->rate, runtime->channels, substream->pcm->device);

	if (substream->pcm->device & 1) {
		runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
		runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	}
	if (substream->pcm->device & 2) {
		runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
		runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	}

	if (unlikely(ret < 0)) {
		pr_err("%s mtk_capture_pcm_close\n", __func__);
		mtk_capture_pcm_close(substream);
		return ret;
	}
	current_pid = task_pid_nr(current);
	if (pid_old != current_pid) {
		tsk = find_task_by_vpid(1);
		if (tsk) {
			cgroup_attach_task_all(tsk, current);
			pr_info("%s change to root! (pid %d -> %d)\n", __func__, pid_old, current_pid);
			pid_old = current_pid;
		}
	}
	pr_debug("%s return\n", __func__);

	SetDeepIdleEnableForAfe(false);

	return 0;
}

static int mtk_capture_pcm_close(struct snd_pcm_substream *substream)
{
	pr_info("%s\n", __func__);

	mtk_capture_alsa_stop(substream);
	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();

	Auddrv_DL_ResetPlatformInfo(Soc_Aud_Digital_Block_MEM_VUL);

	SetDeepIdleEnableForAfe(true);

	return 0;
}

static int mtk_capture_alsa_start(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);
	/* AudDrv_Clk_On(); */
	StartAudioCaptureHardware(substream);
	return 0;
}

static int mtk_capture_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_info("%s cmd=%d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_capture_alsa_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_capture_alsa_stop(substream);
	}
	return -EINVAL;
}

static struct snd_pcm_ops mtk_afe_ops = {
	.open = mtk_capture_pcm_open,
	.close = mtk_capture_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_capture_pcm_hw_params,
	.hw_free = mtk_capture_pcm_hw_free,
	.prepare = mtk_capture_pcm_prepare,
	.trigger = mtk_capture_pcm_trigger,
	.pointer = mtk_capture_pcm_pointer,
};

static struct snd_soc_platform_driver mtk_soc_platform = {
	.ops = &mtk_afe_ops,
	.pcm_new = mtk_asoc_capture_pcm_new,
	.pcm_free = mtk_asoc_capture_pcm_free,
};

static int mtk_capture_probe(struct platform_device *pdev)
{
	pr_notice("%s dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev, &mtk_soc_platform);
}

static int mtk_asoc_capture_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret = 0;

	mAudioDigitalI2S = kzalloc(sizeof(struct AudioDigtalI2S), GFP_KERNEL);

	ret = snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV, card->dev,
							MAX_BUFFER_SIZE, MAX_BUFFER_SIZE);
	return ret;
}

static void mtk_asoc_capture_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
	kfree(mAudioDigitalI2S);
	mAudioDigitalI2S = NULL;
}

static int mtk_capture_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_afe_capture_driver = {
	.driver = {
		   .name = MT_SOC_UL1_PCM,
		   .owner = THIS_MODULE,
		   },
	.probe = mtk_capture_probe,
	.remove = mtk_capture_remove,
};

module_platform_driver(mtk_afe_capture_driver);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
