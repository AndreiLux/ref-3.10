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
#include "mt_soc_digital_type.h"
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <sound/soc.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <linux/pid.h>
#include <linux/reboot.h>


/* MTK add for dummy drivers */
#define AUDIO_MEMORY_SRAM
#define AUDIO_MEM_IOREMAP

/* information about */
static const bool fake_buffer = true;
static pid_t pid_old;
static int afe_emergency_stop;
/*
 *    function implementation
 */
static int mtk_afe_probe(struct platform_device *pdev);
static int mtk_pcm_close(struct snd_pcm_substream *substream);
static int mtk_pcm_dl1_prestart(struct snd_pcm_substream *substream);

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
#define USE_PERIODS_MIN     512
#define USE_PERIODS_MAX     8192

static const struct snd_pcm_hardware mtk_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
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

static int mtk_pcm_dl1_stop(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);
	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);

	/* clean audio hardware buffer */
	Auddrv_ResetBuffer(Soc_Aud_Digital_Block_MEM_DL1);

	return 0;
}

static int mtk_pcm_dl1_post_stop(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	if (!GetI2SDacEnable())
		SetI2SDacEnable(false);

	/* here to turn off digital part */
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O04);

	EnableAfe(false);
	return 0;
}

static snd_pcm_uframes_t mtk_pcm_pointer(struct snd_pcm_substream *substream)
{
	kal_int32 HwPtr;

	HwPtr = Auddrv_UpdateHwPtr(Soc_Aud_Digital_Block_MEM_DL1, substream);

	return HwPtr;
}

static int mtk_pcm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;

	pr_debug("%s\n", __func__);
	if (fake_buffer) {
		if (unlikely(params_buffer_bytes(hw_params) > AFE_INTERNAL_SRAM_SIZE)) {
			pr_warn("%s request size %d > max size %d\n", __func__,
				params_buffer_bytes(hw_params), AFE_INTERNAL_SRAM_SIZE);
		}

		/* here to allcoate sram to hardware --------------------------- */
		AudDrv_Allocate_DL1_Buffer(AFE_INTERNAL_SRAM_SIZE);
		substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE;
		substream->runtime->dma_area = (unsigned char *)AFE_SRAM_ADDRESS;
		substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;
		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;
		dma_buf->private_data = NULL;
	} else {
		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;
		dma_buf->private_data = NULL;
		ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
	}
	pr_debug("%s dma_bytes = %d dma_area = %p dma_addr = 0x%x\n",
		__func__, substream->runtime->dma_bytes,
		substream->runtime->dma_area, substream->runtime->dma_addr);

	pr_debug("%s runtime rate = %d channels = %d substream->pcm->device = %d\n",
		__func__, runtime->rate, runtime->channels, substream->pcm->device);
	return ret;
}

static int mtk_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);
	if (fake_buffer)
		return 0;

	return snd_pcm_lib_free_pages(substream);
}

/* Conventional and unconventional sample rate supported */
static const unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct pcm_afe_info *prtd = NULL;
	struct task_struct *tsk;
	pid_t current_pid = 0;
	int ret = 0;

	pr_debug("%s\n", __func__);

	Auddrv_DL_SetPlatformInfo(Soc_Aud_Digital_Block_MEM_DL1, substream);

	prtd = kzalloc(sizeof(struct pcm_afe_info), GFP_KERNEL);

	if (unlikely(!prtd)) {
		pr_err("%s failed to allocate memory for pcm_afe_info\n", __func__);
		return -ENOMEM;
	}

	prtd->substream = substream;
	prtd->prepared = false;
	runtime->hw = mtk_pcm_hardware;

	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hardware,
		   sizeof(struct snd_pcm_hardware));

	runtime->private_data = prtd;

	pr_info("%s rates= 0x%x mtk_pcm_hardware= %p hw= %p\n",
		__func__, runtime->hw.rates, &mtk_pcm_hardware, &runtime->hw);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	if (unlikely(ret < 0))
		pr_err("snd_pcm_hw_constraint_list failed: 0x%x\n", ret);

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (unlikely(ret < 0))
		pr_err("snd_pcm_hw_constraint_integer failed: 0x%x\n", ret);

	/* here open audio clocks */
	AudDrv_Clk_On();
	AudDrv_ANA_Clk_On();

	/* print for hw pcm information */
	pr_debug("%s runtime rate = %d channels = %d substream->pcm->device = %d\n",
		__func__, runtime->rate, runtime->channels, substream->pcm->device);

	runtime->hw.info |= (SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_NONINTERLEAVED |
				SNDRV_PCM_INFO_MMAP_VALID);

	if (unlikely(ret < 0)) {
		pr_err("%s mtk_pcm_close\n", __func__);
		mtk_pcm_close(substream);
		return ret;
	}

	current_pid = task_pid_nr(current);
	if (pid_old != current_pid) {
		tsk = find_task_by_vpid(1);
		if (tsk) {
			cgroup_attach_task_all(tsk, current);
			pid_old = current_pid;
		}
	}

	pr_debug("%s return\n", __func__);
	return 0;
}

static int mtk_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct pcm_afe_info *prtd = runtime->private_data;

	pr_info("%s\n", __func__);
	Auddrv_DL_ResetPlatformInfo(Soc_Aud_Digital_Block_MEM_DL1);
	mtk_pcm_dl1_post_stop(substream);

	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();	/* move to codec driver */
	kfree(prtd);
	return 0;
}

static int mtk_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtimeStream = substream->runtime;
	struct pcm_afe_info *prtd = NULL;

	pr_debug("%s rate= %u channels= %u period_size= %lu\n",
		__func__, runtimeStream->rate, runtimeStream->channels, runtimeStream->period_size);

	/* HW sequence: */
	/* mtk_pcm_dl1_prestart->codec->mtk_pcm_dl1_start */
	prtd = runtimeStream->private_data;
	if (likely(!prtd->prepared)) {
		mtk_pcm_dl1_prestart(substream);
		prtd->prepared = true;
	}
	return 0;
}

static int mtk_pcm_dl1_prestart(struct snd_pcm_substream *substream)
{
	/* here start digital part */
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O04);

	/* start I2S DAC out */
	SetI2SDacOut(substream->runtime->rate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
	SetI2SDacEnable(true);

	EnableAfe(true);
	return 0;
}

static int mtk_pcm_dl1_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct timespec curr_tstamp;

	if (unlikely(afe_emergency_stop))
		return -EINVAL;

	pr_debug("%s period = %lu,runtime->rate= %u, runtime->channels=%u\n",
		__func__, runtime->period_size, runtime->rate, runtime->channels);

	/* set dl1 sample ratelimit_state */
	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);

	/* here to set interrupt */
	SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->period_size);
	SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->rate);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);
	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, true);
	snd_pcm_gettime(substream->runtime, (struct timespec *)&curr_tstamp);

	pr_debug("%s curr_tstamp %ld %ld\n", __func__, curr_tstamp.tv_sec, curr_tstamp.tv_nsec);

	return 0;
}

static int mtk_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_info("%s cmd=%d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_dl1_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_dl1_stop(substream);
	}
	return -EINVAL;
}

#ifdef CONFIG_PM
static int mtk_afe_suspend(struct snd_soc_dai *dai)
{
	DoAfeSuspend();
	return 0;
}

static int mtk_afe_resume(struct snd_soc_dai *dai)
{
	DoAfeResume();
	return 0;
}
#else
#define mtk_afe_suspend	NULL
#define mtk_afe_resume	NULL
#endif


static struct snd_pcm_ops mtk_afe_ops = {
	.open = mtk_pcm_open,
	.close = mtk_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_hw_params,
	.hw_free = mtk_pcm_hw_free,
	.prepare = mtk_pcm_prepare,
	.trigger = mtk_pcm_trigger,
	.pointer = mtk_pcm_pointer,
};

static int mtk_afe_reboot(struct notifier_block *nb,
	unsigned long action, void *data)
{
	afe_emergency_stop = 1;
	mtk_pcm_dl1_stop(NULL);

	return NOTIFY_DONE;
}

static struct notifier_block mtk_afe_reboot_notifier = {
	.notifier_call		= mtk_afe_reboot,
	.next			= NULL,
	.priority		= INT_MAX,
};

static struct snd_soc_platform_driver mtk_soc_platform = {
	.ops = &mtk_afe_ops,
	.suspend = mtk_afe_suspend,
	.resume = mtk_afe_resume,
};

static int mtk_afe_probe(struct platform_device *pdev)
{
	pr_notice("AudDrv %s: dev name %s\n", __func__, dev_name(&pdev->dev));

	/* 20130430 by Daniel: for PDN_AFE default 0, disable it. */
	if (disable_clock(MT_CG_AUDIO_PDN_AFE, "AUDIO"))
		pr_err("%s disable_clock MT_CG_AUDIO_PDN_AFE fail\n", __func__);

	if (disable_clock(MT_CG_INFRA_AUDIO_PDN, "AUDIO"))
		pr_err("%s disable_clock MT_CG_INFRA_AUDIO_PDN fail\n", __func__);

	InitAfeControl();
	Register_Aud_Irq(&pdev->dev);
	register_reboot_notifier(&mtk_afe_reboot_notifier);
	return snd_soc_register_platform(&pdev->dev, &mtk_soc_platform);
}

static int mtk_afe_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_afe_driver = {
	.driver = {
		   .name = MT_SOC_DL1_PCM,
		   .owner = THIS_MODULE,
		   },
	.probe = mtk_afe_probe,
	.remove = mtk_afe_remove,
};

module_platform_driver(mtk_afe_driver);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
