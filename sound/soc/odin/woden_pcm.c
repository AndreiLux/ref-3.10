/*
 * woden_pcm.c - Woden PCM driver
 *
 * Copyright (c) 2012 LG Electronics Co., Ltd.
 *		http://www.lge.com
 *
 * Copyright (C) 2012 LG Electronics Co. Ltd.
 *	Hyunhee Jeon <hyunhee.jeon@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/dmaengine.h>
#include "woden_pcm.h"

#define DRV_NAME "woden-pcm-audio"

static const struct snd_pcm_hardware woden_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME |
				  SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.channels_min		= 2,
	.channels_max		= 2,
	.period_bytes_min	= 128,
	.period_bytes_max	= PAGE_SIZE,
	.periods_min		= 2,
	.periods_max		= 8,
	.buffer_bytes_max	= PAGE_SIZE * 8,
	.fifo_size		= 4,
};

static void dma_complete_callback(void *data);

static void woden_pcm_queue_dma(struct woden_runtime_data *prtd)
{
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	dma_addr_t addr;
	struct dma_chan *chan;
	dma_cookie_t cookie;

	chan = prtd->dma_chan;
	addr = buf->addr;
	prtd->dma_pos = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		prtd->desc = dmaengine_prep_dma_cyclic(chan,
				addr, //substream->runtime->dma_addr,
				snd_pcm_lib_buffer_bytes(substream),
				snd_pcm_lib_period_bytes(substream), DMA_TO_DEVICE, NULL);
	} else {
		prtd->desc = dmaengine_prep_dma_cyclic(chan,
				addr, //substream->runtime->dma_addr,
				snd_pcm_lib_buffer_bytes(substream),
				snd_pcm_lib_period_bytes(substream), DMA_FROM_DEVICE, NULL);
	}

	if (!prtd->desc) {
		dev_err(&chan->dev->device, "Failed to allocate dma descriptor\n");
		return;
	}

	prtd->desc->callback = dma_complete_callback;
	prtd->desc->callback_param = prtd;

	cookie = dmaengine_submit(prtd->desc);

	dma_async_issue_pending(prtd->dma_chan);
}

static void dma_complete_callback(void *data)
{
	struct woden_runtime_data *prtd = (struct woden_runtime_data *)data;
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (!prtd->running)
		return;

	if (++prtd->period_index >= runtime->periods)
		prtd->period_index = 0;

	snd_pcm_period_elapsed(substream);
}

static bool filter(struct dma_chan *chan, void *param)
{
	struct woden_runtime_data *prtd = param;
	struct woden_pcm_dma_params *dma_params = prtd->dma_params;

	if(strcmp(dev_name(chan->device->dev), "34600000.adma")==0) {
		if (chan->chan_id == dma_params->chan_num) {
			return true;
		} else {
			return false;
		}
	}
	else {
		return false;
	}
}

static int woden_dma_alloc(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct woden_runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	dma_cap_mask_t mask;

	prtd->dma_params = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_CYCLIC, mask);
	prtd->dma_chan = dma_request_channel(mask, filter, prtd);
	if (!prtd->dma_chan)
		return -EINVAL;

	return 0;
}

static int woden_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct woden_runtime_data *prtd;
	int ret = 0;

	prtd = kzalloc(sizeof(struct woden_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	runtime->private_data = prtd;
	prtd->substream = substream;

	spin_lock_init(&prtd->lock);

	ret = woden_dma_alloc(substream);
	if (ret < 0)
		goto err;

	/* Set HW params now that initialization is complete */
	snd_soc_set_runtime_hwparams(substream, &woden_pcm_hardware);

	/* Ensure period size is multiple of 8 */
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 0x8);
	if (ret < 0)
		goto err;

	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
						SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto err;

#ifdef CONFIG_PM_WAKELOCKS
	snprintf(prtd->woden_wake_lock_name, sizeof(prtd->woden_wake_lock_name),
		"woden-pcm-%s-%d",
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? "out" : "in",
		substream->pcm->device);
	wake_lock_init(&prtd->woden_wake_lock, WAKE_LOCK_SUSPEND,
		prtd->woden_wake_lock_name);
#endif

	return 0;

err:

	kfree(prtd);

	return ret;
}

static int woden_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct woden_runtime_data *prtd = runtime->private_data;

#ifdef CONFIG_PM_WAKELOCKS
	wake_lock_destroy(&prtd->woden_wake_lock);
#endif

	if (prtd->dma_chan)
		dma_release_channel(prtd->dma_chan);

	kfree(prtd);

	return 0;
}

static int woden_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct woden_runtime_data *prtd = runtime->private_data;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	prtd->period_bytes = params_period_bytes(params);

	return 0;
}

static int woden_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);

	return 0;
}

static int woden_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct woden_runtime_data *prtd = runtime->private_data;
	unsigned long flags;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->dma_pos = 0;
		prtd->dma_pos_end =
			frames_to_bytes(runtime, runtime->periods * runtime->period_size);
		prtd->period_index = 0;
		/* Fall-through */
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
#ifdef CONFIG_PM_WAKELOCKS
	wake_lock(&prtd->woden_wake_lock);
#endif
		spin_lock_irqsave(&prtd->lock, flags);
		prtd->running = 1;
		spin_unlock_irqrestore(&prtd->lock, flags);
		if (prtd->dma_chan)
			woden_pcm_queue_dma(prtd);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		spin_lock_irqsave(&prtd->lock, flags);
		prtd->running = 0;
		spin_unlock_irqrestore(&prtd->lock, flags);
		if (prtd->dma_chan)
			dmaengine_terminate_all(prtd->dma_chan);
#ifdef CONFIG_PM_WAKELOCKS
		wake_unlock(&prtd->woden_wake_lock);
#endif
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static snd_pcm_uframes_t woden_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct woden_runtime_data *prtd = runtime->private_data;
	snd_pcm_uframes_t offset;

	/* If we can know the right current transmission byte,
	we can return the right position exactly. But dma_engine hasn't that API now. */
	offset = prtd->period_index * runtime->period_size;

	if (offset >= runtime->buffer_size)
		offset = 0;

	return offset;
}

static int woden_pcm_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
					runtime->dma_area,
					runtime->dma_addr,
					runtime->dma_bytes);
}

static struct snd_pcm_ops woden_pcm_ops = {
	.open		= woden_pcm_open,
	.close		= woden_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= woden_pcm_hw_params,
	.hw_free	= woden_pcm_hw_free,
	.trigger	= woden_pcm_trigger,
	.pointer	= woden_pcm_pointer,
	.mmap		= woden_pcm_mmap,
};

static int woden_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = woden_pcm_hardware.buffer_bytes_max;

	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
						&buf->addr, GFP_KERNEL);

	if (!buf->area)
		return -ENOMEM;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->bytes = size;

	return 0;
}

static void woden_pcm_deallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;

	substream = pcm->streams[stream].substream;
	if (!substream)
		return;

	buf = &substream->dma_buffer;
	if (!buf->area)
		return;

	dma_free_writecombine(pcm->card->dev, buf->bytes,
				buf->area, buf->addr);
	buf->area = NULL;
}

static u64 woden_dma_mask = DMA_BIT_MASK(32);

static int woden_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &woden_dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = woden_pcm_preallocate_dma_buffer(pcm,
						SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto err;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = woden_pcm_preallocate_dma_buffer(pcm,
						SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto err_free_play;
	}

	return 0;

err_free_play:
	woden_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
err:
	return ret;
}

static void woden_pcm_free(struct snd_pcm *pcm)
{
	woden_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
	woden_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
}

static struct snd_soc_platform_driver woden_pcm_platform = {
	.ops		= &woden_pcm_ops,
	.pcm_new	= woden_pcm_new,
	.pcm_free	= woden_pcm_free,
};

int woden_pcm_platform_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &woden_pcm_platform);
}
EXPORT_SYMBOL_GPL(woden_pcm_platform_register);

void woden_pcm_platform_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(woden_pcm_platform_unregister);

MODULE_AUTHOR("Hyunhee Jeon <hyunhee.jeon@lge.com>");
MODULE_DESCRIPTION("Woden PCM ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
