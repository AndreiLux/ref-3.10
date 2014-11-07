/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/of.h>
#include <linux/pm_qos.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>
#include "odin_pcm.h"

static struct pm_qos_request mem_qos;

static const struct snd_pcm_hardware odin_pcm_s_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
					SNDRV_PCM_INFO_MMAP_VALID |
					SNDRV_PCM_INFO_PAUSE |
					SNDRV_PCM_INFO_RESUME |
					SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE |
					SNDRV_PCM_FMTBIT_S32_LE,
	.rates 			= SNDRV_PCM_RATE_8000_192000,
	.channels_min		= 1,
	.channels_max		= 2,
	.period_bytes_min	= 512,       /* 128 frm if 4bytes per frm */
	.period_bytes_max	= (32768/2), /* 4096 frm if 4bytes per frm */
	.periods_min		= 2,
	.periods_max		= (32768/512),
	.buffer_bytes_max	= 32768,
};

static const struct snd_pcm_hardware odin_pcm_m_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
					SNDRV_PCM_INFO_MMAP_VALID |
					SNDRV_PCM_INFO_PAUSE |
					SNDRV_PCM_INFO_RESUME |
					SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE |
					SNDRV_PCM_FMTBIT_S32_LE,
	.rates 			= SNDRV_PCM_RATE_8000_192000,
	.channels_min		= 2,
	.channels_max		= 8,
	.period_bytes_min	= 1024,   /* 8ch -   32 frames (32bit)*/
	.period_bytes_max	= 32768,  /* 8ch - 1024 frames (32bit)*/
	.periods_min		= 2,
	.periods_max		= (131072/1024),
	.buffer_bytes_max	= 131072, /* 8ch - 1024 frames * 4(32bit)*/
};

static bool odin_pcm_dma_filter(struct dma_chan *chan, void *param)
{
	struct odin_pcm_dma_params *dma_data;
	struct odin_pcm_platform_data *pdata;
	struct device *pcm_dev;
	struct device *dma_dev;
	struct device_node *pnode = NULL;

	dma_data = (struct odin_pcm_dma_params *)param;
	if (!dma_data)
		return false;

	pcm_dev = (struct device *)dma_data->pcm_device;
	dma_dev = chan->device->dev;

	if (!pcm_dev || !dma_dev)
		return false;

	if (dma_dev->of_node && pcm_dev->of_node) {
		pnode = of_parse_phandle(pcm_dev->of_node, "dma-device", 0);
		if (!pnode) {
			dev_warn(pcm_dev, "Faile to read propery 'dma-device' "
							"in device tree\n");
			return false;
		}

		if (pnode == dma_dev->of_node &&
			chan->chan_id == dma_data->chan_num)
			return true;
		else
			return false;
	}

	pdata = (struct odin_pcm_platform_data *)pcm_dev->platform_data;

	if (pdata) {
		if ((!strcmp(dev_name(dma_dev), pdata->dma_dev_name) ||
			pdata->dma_dev == dma_dev) &&
			(chan->chan_id == dma_data->chan_num))
			return true;
	}
	return false;
}

static int odin_pcm_open(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	struct odin_pcm_dma_params *dma_data = (struct odin_pcm_dma_params *)
		snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	if (dma_data->transfer_devtype == ODIN_PCM_FOR_SINGLE)
		snd_soc_set_runtime_hwparams(substream, &odin_pcm_s_hardware);
	else if (dma_data->transfer_devtype == ODIN_PCM_FOR_MULTI)
		snd_soc_set_runtime_hwparams(substream, &odin_pcm_m_hardware);
	else
		return -EINVAL;

	ret = snd_dmaengine_pcm_open_request_chan(substream,
						odin_pcm_dma_filter, dma_data);
	if (ret)
		return ret;

#if 0 /* DRAM 800MHz Fix */
	pm_qos_update_request(&mem_qos, 800000);
#endif

	return ret;
}

static int odin_pcm_close(struct snd_pcm_substream *substream)
{
	int ret;

	ret = snd_dmaengine_pcm_close_release_chan(substream);
	if (ret)
		return ret;
	pm_qos_update_request(&mem_qos, 0);

	return ret;
}

static void odin_pcm_hw_dump(struct snd_pcm_substream *substream)
{
	dmaengine_device_control(snd_dmaengine_pcm_get_chan(substream),
				 DMA_DEBUG, 0);
}

static int odin_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct dma_chan *chan = snd_dmaengine_pcm_get_chan(substream);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct odin_pcm_dma_params *dma_data;
	struct dma_slave_config slave_config = {0, };
	enum dma_slave_buswidth buswidth;
	int ret;

	dma_data = (struct odin_pcm_dma_params *)
			snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	if (!dma_data)
		return -EINVAL;

	if (!dma_data->addr) {
		dev_err((struct device *)dma_data->pcm_device,
						"address of dma_data invalid\n");
		return -EINVAL;
	}

	if (!substream->dma_buffer.area || !substream->dma_buffer.addr) {
		dev_err((struct device *)dma_data->pcm_device,
						"memory not allocated\n");
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		buswidth = DMA_SLAVE_BUSWIDTH_1_BYTE;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		buswidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
		break;
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		buswidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
		break;
	default:
		return -EINVAL;
	}

	slave_config.src_addr_width = buswidth;
	slave_config.dst_addr_width = buswidth;
	slave_config.slave_id = 0;

	ret = snd_hwparams_to_dma_slave_config(substream, params, &slave_config);
	if (ret)
		return ret;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		slave_config.dst_addr     = dma_data->addr;
		slave_config.dst_maxburst = dma_data->max_burst;
	} else {
		slave_config.src_addr	  = dma_data->addr;
		slave_config.src_maxburst = dma_data->max_burst;
	}

	slave_config.device_fc = 0;

	ret = dmaengine_slave_config(chan, &slave_config);
	if (ret)
		return ret;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return 0;
}

static int odin_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int odin_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	default:
		return -EINVAL;
	}

	return snd_dmaengine_pcm_trigger(substream, cmd);
}

static snd_pcm_uframes_t odin_pcm_pointer(struct snd_pcm_substream *substream)
{
	return snd_dmaengine_pcm_pointer(substream);
}

static int odin_pcm_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
					runtime->dma_area,
					runtime->dma_addr,
					runtime->dma_bytes);
}

static struct snd_pcm_ops odin_pcm_ops = {
	.open		= odin_pcm_open,
	.close		= odin_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= odin_pcm_hw_params,
	.hw_free	= odin_pcm_hw_free,
	.trigger	= odin_pcm_trigger,
	.pointer	= odin_pcm_pointer,
	.mmap		= odin_pcm_mmap,
	.hw_dump	= odin_pcm_hw_dump,
};

static int odin_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, size_t size,
								int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;


	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
						&buf->addr, GFP_KERNEL);
	if (!buf->area) {
		dev_err(pcm->card->dev, "Failed to alloc the dma_alloc");
		return -ENOMEM;
	}

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->bytes = size;

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	dev_info(pcm->card->dev, "va: 0x%pK, pa: 0x%llx, size: %zu\n",
						buf->area, buf->addr, size);
#else
	dev_info(pcm->card->dev, "va: 0x%pK, pa: 0x%lx, size: %zu\n",
						buf->area, buf->addr, size);
#endif
	return 0;
}

static void odin_pcm_deallocate_dma_buffer(struct snd_pcm *pcm, int stream)
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

static u64 odin_dma_mask = DMA_BIT_MASK(64);

static int odin_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct odin_pcm_dma_params *dma_data;
	size_t alloc_size = 0;
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &odin_dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(64);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		dma_data = (struct odin_pcm_dma_params *)
			snd_soc_dai_get_dma_data(rtd->cpu_dai,
			pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream);
		if (!dma_data)
			return -EINVAL;

		if (dma_data)
			dma_data->pcm_device = rtd->platform->dev;

		if (dma_data->transfer_devtype == ODIN_PCM_FOR_SINGLE)
			alloc_size = odin_pcm_s_hardware.buffer_bytes_max;
		else if (dma_data->transfer_devtype == ODIN_PCM_FOR_MULTI)
			alloc_size = odin_pcm_m_hardware.buffer_bytes_max;
		else
			return -EINVAL;

		ret = odin_pcm_preallocate_dma_buffer(pcm, alloc_size,
						SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto err;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		dma_data = (struct odin_pcm_dma_params *)
			snd_soc_dai_get_dma_data(rtd->cpu_dai,
			pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream);
		if (!dma_data)
			return -EINVAL;

		if (dma_data)
			dma_data->pcm_device = rtd->platform->dev;

		if (dma_data->transfer_devtype == ODIN_PCM_FOR_SINGLE)
			alloc_size = odin_pcm_s_hardware.buffer_bytes_max;
		else if (dma_data->transfer_devtype == ODIN_PCM_FOR_MULTI)
			alloc_size = odin_pcm_m_hardware.buffer_bytes_max;
		else
			return -EINVAL;

		ret = odin_pcm_preallocate_dma_buffer(pcm, alloc_size,
						SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto err_free_play;
	}

	return 0;

err_free_play:
	odin_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
err:
	return ret;
}

static void odin_pcm_free(struct snd_pcm *pcm)
{
	odin_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
	odin_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
}

static struct snd_soc_platform_driver odin_pcm_platform = {
	.ops		= &odin_pcm_ops,
	.pcm_new	= odin_pcm_new,
	.pcm_free	= odin_pcm_free,
};

static int odin_pcm_platform_probe(struct platform_device *pdev)
{
	int ret;

	ret = snd_soc_register_platform(&pdev->dev, &odin_pcm_platform);
	if (ret)
		dev_err(&pdev->dev, "Failed to set the soc pcm platform");
	else
		dev_info(&pdev->dev, "Probing success!");

	pm_qos_add_request(&mem_qos, PM_QOS_ODIN_MEM_MIN_FREQ, 0);

	return ret;
}

static int odin_pcm_platform_remove(struct platform_device *pdev)
{

	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id odin_pcm_match[] = {
	{ .compatible = "lge,odin-pcm", },
	{},
};

static struct platform_driver odin_pcm_driver = {
	.driver = {
		.name = "odin-pcm",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(odin_pcm_match),
	},
	.probe = odin_pcm_platform_probe,
	.remove = odin_pcm_platform_remove,
};
module_platform_driver(odin_pcm_driver);
MODULE_AUTHOR("Hyunhee Jeon <hyunhee.jeon@lge.com>");
MODULE_DESCRIPTION("Odin PCM ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:odin-pcm");

