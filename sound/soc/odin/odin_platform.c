/*
 * odin_platform.c - ODIN DSP Platform driver
 *
 * Copyright (C) 2013 LG Electronics Co. Ltd.
 *
 * Based on sst_platform.c by:
 * Copyright (C) 2010-2012 Intel Corp
 * Author: Vinod Koul <vinod.koul@intel.com>
 * Author: Harsha Priya <priya.harsha@intel.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 *
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/compress_driver.h>
#include "odin_platform.h"

static struct odin_device *odin_dsp;
static DEFINE_MUTEX(odin_lock);

int odin_register_dsp(struct odin_device *dev)
{
	BUG_ON(!dev);
	if (!try_module_get(dev->dev->driver->owner))
		return -ENODEV;
	mutex_lock(&odin_lock);
	if (odin_dsp) {
		pr_err("we already have a device %s\n", odin_dsp->name);
		module_put(dev->dev->driver->owner);
		mutex_unlock(&odin_lock);
		return -EEXIST;
	}
	pr_debug("registering device %s\n", dev->name);
	odin_dsp = dev;
	mutex_unlock(&odin_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(odin_register_dsp);

int odin_unregister_dsp(struct odin_device *dev)
{
	BUG_ON(!dev);
	if (dev != odin_dsp)
		return -EINVAL;

	mutex_lock(&odin_lock);

	if (!odin_dsp) {
		mutex_unlock(&odin_lock);
		return -EIO;
	}

	module_put(odin_dsp->dev->driver->owner);
	pr_debug("unreg %s\n", odin_dsp->name);
	odin_dsp = NULL;
	mutex_unlock(&odin_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(odin_unregister_dsp);

static struct snd_pcm_hardware odin_platform_pcm_hw = {
	.info =	(SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_RESUME |
			SNDRV_PCM_INFO_BLOCK_TRANSFER),
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SNDRV_PCM_RATE_8000_48000,
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min =	1,
	.channels_max =	2,
	.buffer_bytes_max = 8192 * 16 * 4,
	.period_bytes_min = 1024,
	.period_bytes_max = 8192 * 4,
	.periods_min = 2,
	.periods_max = 128,
};

static int odin_platform_dai_startup(struct snd_pcm_substream *substream,
						struct snd_soc_dai *dai)
{
	dai->runtime = substream->runtime;
	return 0;
}

static void odin_platform_dai_shutdown(struct snd_pcm_substream *substream,
						struct snd_soc_dai *dai)
{
	dai->runtime = NULL;
}

static int odin_platform_dai_set_sysclk(struct snd_soc_dai *dai, int clk_id,
					unsigned int freq, int dir)
{
	struct odin_platform_data *odin_pdata = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	if(!odin_pdata)
		return -ENXIO;

	ret = clk_set_rate(odin_pdata->aud_clk, freq);
	if (ret)
		dev_err(dai->dev, "Can't set clock rate: %d %d\n", freq, ret);

	return ret;
}

static int odin_platform_dai_set_fmt(struct snd_soc_dai *dai,
							unsigned int fmt)
{
	struct odin_runtime_stream *stream = dai->runtime->private_data;
	struct odin_pcm_dev_params dev_params = {0};

	if(!stream)
		return -ENXIO;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		dev_params.i2s_format = SND_ODIN_FMT_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		dev_params.i2s_format = SND_ODIN_FMT_RIGHT_J;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		dev_params.i2s_format = SND_ODIN_FMT_LEFT_J;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		dev_params.i2s_format = SND_ODIN_FMT_DSP_A;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		dev_params.i2s_format = SND_ODIN_FMT_DSP_B;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		dev_params.master_mode = ODIN_PLATFORM_SLAVE;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		dev_params.master_mode = ODIN_PLATFORM_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
		dev_err(dai->dev, "Unsupported clock master mask\n");
	default:
		return -EINVAL;
	}

	return stream->ops->device_control(ODIN_SND_HW_SET, &dev_params);
}

static struct snd_soc_dai_ops odin_dai_ops = {
	.startup        = odin_platform_dai_startup,
	.shutdown       = odin_platform_dai_shutdown,
	.set_sysclk	= odin_platform_dai_set_sysclk,
	.set_fmt	= odin_platform_dai_set_fmt,
};

static struct snd_soc_dai_driver odin_platform_dai[] = {
{
	.name = "pcm-cpu-dai",
	.id = 0,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &odin_dai_ops,
},
#if defined(CONFIG_ODIN_COMPRESS)
{
	.name = "Compress-cpu-dai",
	.compress_dai = 1,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
},
#endif
};

/* helper functions */
static inline void odin_set_stream_status(struct odin_runtime_stream *stream,
					int state)
{
	unsigned long flags;
	spin_lock_irqsave(&stream->status_lock, flags);
	stream->stream_status = state;
	spin_unlock_irqrestore(&stream->status_lock, flags);
}

static inline int odin_get_stream_status(struct odin_runtime_stream *stream)
{
	int state;
	unsigned long flags;
	spin_lock_irqsave(&stream->status_lock, flags);
	state = stream->stream_status;
	spin_unlock_irqrestore(&stream->status_lock, flags);
	return state;
}

static void odin_fill_pcm_params(struct snd_pcm_substream *substream,
				struct odin_pcm_params *param)
{
	param->codec = ODIN_CODEC_TYPE_PCM;
	param->num_chan = (u8) substream->runtime->channels;
	param->pcm_wd_sz = substream->runtime->sample_bits;
	param->reserved = 0;
	param->sfreq = substream->runtime->rate;
	param->ring_buffer_size = snd_pcm_lib_buffer_bytes(substream);
	param->ring_buffer_count = substream->runtime->buffer_size;
	param->period_size = snd_pcm_lib_period_bytes(substream);
	param->period_count = substream->runtime->period_size;
	param->ring_buffer_addr = substream->dma_buffer.addr;
	pr_debug("period_cnt = %d\n", param->period_count);
	pr_debug("sfreq= %d, wd_sz = %d\n", param->sfreq, param->pcm_wd_sz);
}

static int odin_platform_alloc_stream(struct snd_pcm_substream *substream)
{
	struct odin_runtime_stream *stream =
			substream->runtime->private_data;
	struct odin_pcm_params param = {0};
	struct odin_stream_params str_params = {0};
	int ret;

	odin_fill_pcm_params(substream, &param);
	substream->runtime->dma_area = substream->dma_buffer.area;
	str_params.sparams = param;
	str_params.codec =  param.codec;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		str_params.ops = STREAM_OPS_PLAYBACK;
		str_params.device_type = SND_ODIN_DEVICE_PLAYBACK;
		pr_debug("Playbck stream,Device %d\n",
					substream->pcm->device);
	} else {
		str_params.ops = STREAM_OPS_CAPTURE;
		str_params.device_type = SND_ODIN_DEVICE_CAPTURE;
		pr_debug("Capture stream,Device %d\n",
					substream->pcm->device);
	}
	ret = stream->ops->open(&str_params);
	pr_debug("ODIN_SND_PLAY/CAPTURE ret = %x\n", ret);
	if (ret < 0)
		return ret;

	stream->stream_info.str_id = ret;
	pr_debug("str id :  %d\n", stream->stream_info.str_id);
	return ret;
}

static void odin_period_elapsed(void *alsa_substream)
{
	struct snd_pcm_substream *substream = alsa_substream;
	struct odin_runtime_stream *stream;
	int status;

#if 0
	//heewan park
	printk("%s\n", __func__);
#endif

	if (!substream || !substream->runtime)
		return;
	stream = substream->runtime->private_data;
	if (!stream)
		return;
	status = odin_get_stream_status(stream);
	if (status != ODIN_PLATFORM_RUNNING)
		return;
	snd_pcm_period_elapsed(substream);
}

static int odin_platform_init_stream(struct snd_pcm_substream *substream)
{
	struct odin_runtime_stream *stream =
			substream->runtime->private_data;
	int ret;

	pr_debug("setting buffer ptr param\n");
	odin_set_stream_status(stream, ODIN_PLATFORM_INIT);
	stream->stream_info.period_elapsed = odin_period_elapsed;
	stream->stream_info.alsa_substream = substream;
	stream->stream_info.buffer_ptr = 0;
	stream->stream_info.sfreq = substream->runtime->rate;
	ret = stream->ops->device_control(
			ODIN_SND_STREAM_INIT, &stream->stream_info);
	if (ret)
		pr_err("control_set ret error %d\n", ret);
	return ret;

}
/* end -- helper functions */

static int odin_platform_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct odin_runtime_stream *stream;
	int ret;

	pr_debug("odin_platform_open called\n");

	snd_soc_set_runtime_hwparams(substream, &odin_platform_pcm_hw);
	ret = snd_pcm_hw_constraint_integer(runtime,
						SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		return ret;

	stream = kzalloc(sizeof(*stream), GFP_KERNEL);
	if (!stream)
		return -ENOMEM;
	spin_lock_init(&stream->status_lock);

	/* get the odin_dsp ops */
	mutex_lock(&odin_lock);
	if (!odin_dsp) {
		pr_err("no device available to run\n");
		mutex_unlock(&odin_lock);
		kfree(stream);
		return -ENODEV;
	}
	if (!try_module_get(odin_dsp->dev->driver->owner)) {
		mutex_unlock(&odin_lock);
		kfree(stream);
		return -ENODEV;
	}
	stream->ops = odin_dsp->ops;
	mutex_unlock(&odin_lock);

	stream->stream_info.str_id = 0;
	odin_set_stream_status(stream, ODIN_PLATFORM_INIT);
	stream->stream_info.alsa_substream = substream;
	runtime->private_data = stream;

	return 0;
}

static int odin_platform_close(struct snd_pcm_substream *substream)
{
	struct odin_runtime_stream *stream;
	int ret = 0, str_id;

	pr_debug("odin_platform_close called\n");
	stream = substream->runtime->private_data;
	str_id = stream->stream_info.str_id;
	if (str_id)
		ret = stream->ops->close(str_id);
	module_put(odin_dsp->dev->driver->owner);
	kfree(stream);
	return ret;
}

static int odin_platform_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct odin_runtime_stream *stream;
	int ret = 0, str_id;

	pr_debug("odin_platform_pcm_prepare called\n");
	stream = substream->runtime->private_data;
	str_id = stream->stream_info.str_id;
	if (stream->stream_info.str_id) {
		ret = stream->ops->device_control(
				ODIN_SND_DROP, &str_id);
		return ret;
	}

	ret = odin_platform_alloc_stream(substream);
	if (ret < 0)
		return ret;
	snprintf(substream->pcm->id, sizeof(substream->pcm->id),
			"%d", stream->stream_info.str_id);

	ret = odin_platform_init_stream(substream);
	if (ret)
		return ret;

	return ret;
}

static int odin_platform_pcm_trigger(struct snd_pcm_substream *substream,
					int cmd)
{
	int ret = 0, str_id;
	struct odin_runtime_stream *stream;
	int str_cmd, status;

	pr_debug("odin_platform_pcm_trigger called\n");
	stream = substream->runtime->private_data;
	str_id = stream->stream_info.str_id;
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		pr_debug("odin_dsp: Trigger Start\n");
		str_cmd = ODIN_SND_START;
		status = ODIN_PLATFORM_RUNNING;
		stream->stream_info.alsa_substream = substream;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("odin_dsp: in stop\n");
		str_cmd = ODIN_SND_DROP;
		status = ODIN_PLATFORM_DROPPED;
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_debug("odin_dsp: in pause\n");
		str_cmd = ODIN_SND_PAUSE;
		status = ODIN_PLATFORM_PAUSED;
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_debug("odin_dsp: in pause release\n");
		str_cmd = ODIN_SND_RESUME;
		status = ODIN_PLATFORM_RUNNING;
		break;
	default:
		return -EINVAL;
	}
	ret = stream->ops->device_control(str_cmd, &str_id);
	if (!ret)
		odin_set_stream_status(stream, status);

	return ret;
}


static snd_pcm_uframes_t odin_platform_pcm_pointer
			(struct snd_pcm_substream *substream)
{
	struct odin_runtime_stream *stream;
	int ret, status;
	struct pcm_stream_info *str_info;

	snd_pcm_uframes_t offset;


	stream = substream->runtime->private_data;
	status = odin_get_stream_status(stream);
	if (status == ODIN_PLATFORM_INIT)
		return 0;
	str_info = &stream->stream_info;
	ret = stream->ops->device_control(
				ODIN_SND_BUFFER_POINTER, str_info);
	if (ret) {
		pr_err("odin_dsp: error code = %d\n", ret);
		return ret;
	}

	return stream->stream_info.buffer_ptr;
}

static int odin_platform_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	memset(substream->dma_buffer.area, 0, params_buffer_bytes(params));
	return 0;
}

static struct snd_pcm_ops odin_platform_ops = {
	.open = odin_platform_open,
	.close = odin_platform_close,
	.ioctl = snd_pcm_lib_ioctl,
	.prepare = odin_platform_pcm_prepare,
	.trigger = odin_platform_pcm_trigger,
	.pointer = odin_platform_pcm_pointer,
	.hw_params = odin_platform_pcm_hw_params,
};

static int odin_pcm_preallocate_dma_buffer(struct snd_pcm_substream *substream)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = odin_platform_pcm_hw.buffer_bytes_max;

	buf->area = dma_alloc_writecombine(dev, size, &buf->addr, GFP_KERNEL);
	if (!buf->area) {
		dev_err(dev , "Failed to alloc the dma_alloc");
		return -ENOMEM;
	}

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = dev;
	buf->private_data = NULL;
	buf->bytes = size;

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

static void odin_pcm_free(struct snd_pcm *pcm)
{
	pr_debug("odin_pcm_free called\n");
	odin_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
	odin_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
}

static int odin_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_substream *substream;
	int retval = 0;

	pr_debug("odin_pcm_new called\n");
	substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if (substream)
		retval = odin_pcm_preallocate_dma_buffer(substream);

	substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	if (!retval && substream)
		retval = odin_pcm_preallocate_dma_buffer(substream);

	return retval;
}

/* compress stream operations */
static void odin_compr_fragment_elapsed(void *arg)
{
	struct snd_compr_stream *cstream = (struct snd_compr_stream *)arg;

	pr_debug("fragment elapsed by driver\n");
	if (cstream)
		snd_compr_fragment_elapsed(cstream);
}

static int odin_platform_compr_open(struct snd_compr_stream *cstream)
{
	int ret = 0;
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream;

	stream = kzalloc(sizeof(*stream), GFP_KERNEL);
	if (!stream)
		return -ENOMEM;

	spin_lock_init(&stream->status_lock);

	/* get the odin_dsp ops */
	if (!odin_dsp || !try_module_get(odin_dsp->dev->driver->owner)) {
		pr_err("no device available to run\n");
		ret = -ENODEV;
		goto out_ops;
	}
	stream->compr_ops = odin_dsp->compr_ops;

	stream->id = 0;
	odin_set_stream_status(stream, ODIN_PLATFORM_INIT);
	runtime->private_data = stream;
	return 0;
out_ops:
	kfree(stream);
	return ret;
}

static int odin_platform_compr_free(struct snd_compr_stream *cstream)
{
	struct odin_runtime_stream *stream;
	int ret = 0, str_id;

	stream = cstream->runtime->private_data;
	/*need to check*/
	str_id = stream->id;
	if (str_id)
		ret = stream->compr_ops->close(str_id);
	module_put(odin_dsp->dev->driver->owner);
	kfree(stream);
	pr_debug("%s: %d\n", __func__, ret);
	return 0;
}

static int odin_platform_compr_set_params(struct snd_compr_stream *cstream,
					struct snd_compr_params *params)
{
	struct odin_runtime_stream *stream;
	int retval;
	struct snd_odin_params str_params;
	struct odin_compress_cb cb;

	stream = cstream->runtime->private_data;
	/* construct fw structure for this*/
	memset(&str_params, 0, sizeof(str_params));

	str_params.ops = STREAM_OPS_PLAYBACK;
	str_params.stream_type = ODIN_STREAM_TYPE_MUSIC;
	str_params.device_type = SND_ODIN_DEVICE_COMPRESS;

	switch (params->codec.id) {
	case SND_AUDIOCODEC_MP3: {
		str_params.codec = ODIN_CODEC_TYPE_MP3;
		str_params.sparams.uc.mp3_params.codec = ODIN_CODEC_TYPE_MP3;
		str_params.sparams.uc.mp3_params.num_chan = params->codec.ch_in;
		str_params.sparams.uc.mp3_params.pcm_wd_sz = 16;
		break;
	}

	case SND_AUDIOCODEC_AAC: {
		str_params.codec = ODIN_CODEC_TYPE_AAC;
		str_params.sparams.uc.aac_params.codec = ODIN_CODEC_TYPE_AAC;
		str_params.sparams.uc.aac_params.num_chan = params->codec.ch_in;
		str_params.sparams.uc.aac_params.pcm_wd_sz = 16;
		if (params->codec.format == SND_AUDIOSTREAMFORMAT_MP4ADTS)
			str_params.sparams.uc.aac_params.bs_format =
							AAC_BIT_STREAM_ADTS;
		else if (params->codec.format == SND_AUDIOSTREAMFORMAT_RAW)
			str_params.sparams.uc.aac_params.bs_format =
							AAC_BIT_STREAM_RAW;
		else {
			pr_err("Undefined format%d\n", params->codec.format);
			return -EINVAL;
		}
		str_params.sparams.uc.aac_params.externalsr =
						params->codec.sample_rate;
		break;
	}

	default:
		pr_err("codec not supported, id =%d\n", params->codec.id);
		return -EINVAL;
	}

	str_params.aparams.ring_buf_info[0].addr  =
					virt_to_phys(cstream->runtime->buffer);
	str_params.aparams.ring_buf_info[0].size =
					cstream->runtime->buffer_size;
	str_params.aparams.sg_count = 1;
	str_params.aparams.frag_size = cstream->runtime->fragment_size;

	cb.param = cstream;
	cb.compr_cb = odin_compr_fragment_elapsed;

	retval = stream->compr_ops->open(&str_params, &cb);
	if (retval < 0) {
		pr_err("stream allocation failed %d\n", retval);
		return retval;
	}

	stream->id = retval;
	return 0;
}

static int odin_platform_compr_trigger(struct snd_compr_stream *cstream, int cmd)
{
	struct odin_runtime_stream *stream =
		cstream->runtime->private_data;

	return stream->compr_ops->control(cmd, stream->id);
}

static int odin_platform_compr_pointer(struct snd_compr_stream *cstream,
					struct snd_compr_tstamp *tstamp)
{
	struct odin_runtime_stream *stream;

	stream  = cstream->runtime->private_data;
	stream->compr_ops->tstamp(stream->id, tstamp);
	tstamp->byte_offset = tstamp->copied_total %
				 (u32)cstream->runtime->buffer_size;
	pr_debug("calc bytes offset/copied bytes as %d\n", tstamp->byte_offset);
	return 0;
}

static int odin_platform_compr_ack(struct snd_compr_stream *cstream,
					size_t bytes)
{
	struct odin_runtime_stream *stream;

	stream  = cstream->runtime->private_data;
	stream->compr_ops->ack(stream->id, (unsigned long)bytes);
	stream->bytes_written += bytes;

	return 0;
}

static int odin_platform_compr_get_caps(struct snd_compr_stream *cstream,
					struct snd_compr_caps *caps)
{
	struct odin_runtime_stream *stream =
		cstream->runtime->private_data;

	return stream->compr_ops->get_caps(caps);
}

static int odin_platform_compr_get_codec_caps(struct snd_compr_stream *cstream,
					struct snd_compr_codec_caps *codec)
{
	struct odin_runtime_stream *stream =
		cstream->runtime->private_data;

	return stream->compr_ops->get_codec_caps(codec);
}

static struct snd_compr_ops odin_platform_compr_ops = {
	.open = odin_platform_compr_open,
	.free = odin_platform_compr_free,
	.set_params = odin_platform_compr_set_params,
	.trigger = odin_platform_compr_trigger,
	.pointer = odin_platform_compr_pointer,
	.ack = odin_platform_compr_ack,
	.get_caps = odin_platform_compr_get_caps,
	.get_codec_caps = odin_platform_compr_get_codec_caps,
};

static struct snd_soc_platform_driver odin_soc_platform_drv = {
	.ops		= &odin_platform_ops,
	.compr_ops	= &odin_platform_compr_ops,
	.pcm_new	= odin_pcm_new,
	.pcm_free	= odin_pcm_free,
};

static int odin_platform_probe(struct platform_device *pdev)
{
	int ret;
	struct odin_platform_data *odin_pdata;

	pr_debug("odin_platform_probe called\n");
	odin_dsp = NULL;

	odin_pdata = kzalloc(sizeof(struct odin_platform_data), GFP_KERNEL);
	if (!odin_pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		return ret;
	}
	dev_set_drvdata(&pdev->dev, odin_pdata);

	odin_pdata->aud_clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(odin_pdata->aud_clk)) {
		dev_err(&pdev->dev, "Failed to get clock\n");
		ret = PTR_ERR(odin_pdata->aud_clk);
		return ret;
	}

	ret = snd_soc_register_platform(&pdev->dev, &odin_soc_platform_drv);
	if (ret) {
		pr_err("registering soc platform failed\n");
		return ret;
	}

	ret = snd_soc_register_dais(&pdev->dev,
			odin_platform_dai, ARRAY_SIZE(odin_platform_dai));
	if (ret) {
		pr_err("registering cpu dais failed\n");
		snd_soc_unregister_platform(&pdev->dev);
		return ret;
	}
	dev_info(&pdev->dev, "Success!\n");
	return 0;
}

static int odin_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dais(&pdev->dev, ARRAY_SIZE(odin_platform_dai));
	snd_soc_unregister_platform(&pdev->dev);
	pr_debug("odin_platform_remove success\n");
	return 0;
}

static const struct of_device_id odin_platform_match[] = {
	{ .compatible = "lge,odin-platform", },
	{},
};

static struct platform_driver odin_platform_driver = {
	.driver		= {
		.name		= "odin-platform",
		.owner		= THIS_MODULE,
		.of_match_table = of_match_ptr(odin_platform_match),
	},
	.probe		= odin_platform_probe,
	.remove		= odin_platform_remove,
};

module_platform_driver(odin_platform_driver);
MODULE_DESCRIPTION("ASoC ODIN Platform driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:odin-platform");
