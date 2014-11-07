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
#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/time.h>
#include <linux/math64.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <linux/odin_mailbox.h>
#include <linux/odin_pd.h>
#include <asm/io.h>
#include <linux/mm.h>
#if 0
#include <linux/wakelock.h>
#endif
#if 0
#include <linux/odin_lpa.h>
#endif

#include <sound/tlv.h>
#include <sound/compress_params.h>
#include <sound/compress_offload.h>
#include <sound/compress_driver.h>
#include <linux/pm_runtime.h>

#include "odin_compress.h"
static struct odin_device *odin_dsp;
struct snd_compr_stream *gstream;
struct compr_stream_info stream_info;

#if 0
struct wake_lock odin_compress_wake_lock;
char odin_compress_wake_lock_name[32];
#endif

const DECLARE_TLV_DB_LINEAR(odin_compr_vol_gain, 0,
		COMPRESSED_LR_VOL_MAX_STEPS);

struct class *dsp_dev_class;

static ssize_t get_dsp_dump(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
#if 1
	void __iomem *dsp_base;
	int i;
	dsp_base = ioremap(0xDB000000, 0x100000);
	if (!dsp_base)
	{
		pr_err("%s:dsp_base ioremap failed\n",__func__);
		return -ENOMEM;
	}

	for (i=0;i<0x100000;i++)
		sprintf(buf,"%x",*((unsigned char *)dsp_base+i));

	iounmap(dsp_base);
#endif
	return 0;
}

static CLASS_ATTR(dsp_dump, S_IWUSR|S_IRUGO, get_dsp_dump, NULL);

/* helper functions */
void odin_set_stream_status(struct odin_runtime_stream *stream,
					int state)
{
	unsigned long flags;

	if (stream)
	{
		spin_lock_irqsave(&stream->status_lock, flags);
		stream->stream_status = state;
		spin_unlock_irqrestore(&stream->status_lock, flags);
	}
}

int odin_get_stream_status(struct odin_runtime_stream *stream)
{
	int state;
	unsigned long flags;

	if (stream)
	{
		spin_lock_irqsave(&stream->status_lock, flags);
		state = stream->stream_status;
		spin_unlock_irqrestore(&stream->status_lock, flags);
		return state;
	}
	else
		return -1;
}

static int odin_compr_set_volume(struct snd_compr_stream *cstream,
				uint32_t volume_l, uint32_t volume_r)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;

	pr_debug("%s\n",__func__);
	pr_debug("volume_l = %d\n", (int)volume_l);

	if (stream)
	{
		if (stream->compr_ops->volume_command(cstream,
					(uint32_t)volume_l,(uint32_t)volume_l))
		{
			pr_err("Set volume failed\n");
		}
	}

	return 0;
}

int odin_register_dsp(struct odin_device *dev)
{
	BUG_ON(!dev);
	if (!try_module_get(dev->dev->driver->owner))
		return -ENODEV;
	if (odin_dsp) {
		pr_err("we already have a device %s\n", odin_dsp->name);
		module_put(dev->dev->driver->owner);
		return -EEXIST;
	}
	pr_debug("registering device %s\n", dev->name);
	odin_dsp = dev;
	return 0;
}
EXPORT_SYMBOL_GPL(odin_register_dsp);

int odin_unregister_dsp(struct odin_device *dev)
{
	BUG_ON(!dev);
	if (dev != odin_dsp)
		return -EINVAL;

	if (!odin_dsp) {
		return -EIO;
	}

	module_put(odin_dsp->dev->driver->owner);
	pr_debug("unreg %s\n", odin_dsp->name);
	odin_dsp = NULL;
	return 0;
}
EXPORT_SYMBOL_GPL(odin_unregister_dsp);

static void odin_compr_fragment_elapsed(void *alsa_substream)
{
	struct snd_compr_stream *cstream = alsa_substream;
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;
	size_t buffer_size;

	buffer_size = (stream->fragment_size)*(stream->fragments);

	if (stream->byte_offset >= buffer_size)
		stream->byte_offset -= buffer_size;

	snd_compr_fragment_elapsed(cstream);
}


static int odin_compr_open(struct snd_compr_stream *cstream)
{
	int ret = 0;
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream;

	pr_debug("%s\n", __func__);

	stream = kzalloc(sizeof(*stream), GFP_KERNEL);
	if (!stream)
		return -ENOMEM;

	spin_lock_init(&stream->status_lock);
	spin_lock_init(&stream->lock);

	atomic_set(&stream_info.eos, 0);
	atomic_set(&stream_info.drain, 0);

	init_waitqueue_head(&stream_info.eos_wait);
	init_waitqueue_head(&stream_info.drain_wait);
	init_waitqueue_head(&stream->flush_wait);
	
	/* get the odin_dsp ops */
	if (!odin_dsp || !try_module_get(odin_dsp->dev->driver->owner)) {
		pr_err("no device available to run\n");
		ret = -ENODEV;
		goto out_ops;
	}

	pm_runtime_get_sync(odin_dsp->dev);

	stream->compr_ops = odin_dsp->compr_ops;
	stream->id = 0;
	odin_set_stream_status(stream, ODIN_PLATFORM_OPENED);
	stream->stream_info.fragment_elapsed = odin_compr_fragment_elapsed;
	stream->stream_info.str_id = 0;
	stream->stream_info.buffer_ptr = 0;
	stream->stream_info.alsa_substream = cstream;

	runtime->private_data = stream;

#if 0
	snprintf(odin_compress_wake_lock_name, sizeof(odin_compress_wake_lock_name),
			"odin-arm-lpa");
	wake_lock_init(&odin_compress_wake_lock, WAKE_LOCK_SUSPEND,
			odin_compress_wake_lock_name);
#endif

	if (cstream->direction == SND_COMPRESS_PLAYBACK)
	{
		if (stream->compr_ops->open(cstream))
		{
			ret = -EFAULT;
			goto out_ops;
		}

		gstream = cstream;
		odin_set_stream_status(stream, ODIN_PLATFORM_OPEN);

		return 0;
	}
	else
	{
		pr_err("%s : Unsupported stream type\n", __func__);
		ret = -EFAULT;
		goto out_ops;
	}

out_ops:
	kfree(stream);
	return ret;

}

static int odin_compr_free(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;

	pr_debug("%s\n", __func__);
#if 0
	wake_lock_destroy(&odin_compress_wake_lock);
#endif

	pm_runtime_mark_last_busy(odin_dsp->dev);
	pm_runtime_put_autosuspend(odin_dsp->dev);

	if (cstream->direction == SND_COMPRESS_PLAYBACK)
	{
		stream->compr_ops->close(cstream);
	}
	
	memset(&stream_info,0,sizeof(struct compr_stream_info));

	gstream = NULL;

	kfree(stream);	
	runtime->private_data = NULL;

	return 0;
}

static int odin_compr_set_params(struct snd_compr_stream *cstream,
				struct snd_compr_params *params)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;
	pr_debug("%s\n", __func__);

	stream->fragments = runtime->fragments;
	stream->fragment_size = runtime->fragment_size;
	stream->app_pointer = 0;
	stream->copied_total = 0;
	stream->byte_received = 0;
	stream->temp_size = 0;

	runtime->private_data = stream;

	return stream->compr_ops->set_params(cstream, params);
}

static int odin_compr_trigger(struct snd_compr_stream *cstream, int cmd)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct odin_compr_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	uint32_t *volume = pdata->volume;

	switch (cmd)
	{
		case SNDRV_PCM_TRIGGER_START:
			stream->stream_info.alsa_substream = cstream;

		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			pr_debug("%s: left_vol %d right_vol %d\n",
					__func__, volume[0], volume[1]);
			odin_compr_set_volume(cstream, volume[0], volume[1]);

#if 0
			wake_lock(&odin_compress_wake_lock);
#endif
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
#if 0
			wake_unlock(&odin_compress_wake_lock);
#endif
			break;
		case SND_COMPR_TRIGGER_PARTIAL_DRAIN:
		case SND_COMPR_TRIGGER_DRAIN:
			break;
		case SNDRV_COMPRESS_AVAIL:
			break;
	}
	return stream->compr_ops->control(cmd, cstream);
}

static int odin_compr_pointer(struct snd_compr_stream *cstream,
				struct snd_compr_tstamp *tstamp)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;

	if (odin_get_stream_status(stream)==ODIN_PLATFORM_RUNNING ||
			odin_get_stream_status(stream) == ODIN_PLATFORM_PAUSED)
	{
		if (stream->compr_ops->tstamp(cstream, tstamp))
			return -1;
	}
	else
	{
		tstamp->pcm_io_frames = 0;
	}

	tstamp->sampling_rate = stream->sample_rate;
	tstamp->copied_total = stream->copied_total;
	tstamp->byte_offset = stream->byte_offset;

	return 0;
}
#if 0
static int odin_compr_ack(struct snd_compr_stream *cstream,
			size_t count)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;

	printk("%s : count = %d\n", __func__, count);


	return 0;
}
#endif

static int odin_compr_copy(struct snd_compr_stream *cstream,
			  char __user *buf, size_t count)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct odin_compr_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	uint32_t *volume = pdata->volume;

	void *dstn;
	uint32_t dstn_paddr;
	size_t copy;
	unsigned long flags;
	size_t buffer_size;

	pr_debug("%s : count = %d\n",__func__, count);
	pr_debug("%s : stream->app_pointer = %d\n",__func__, stream->app_pointer);

	stream->start_count = count;
	if (!stream->buffer)
	{
		pr_err("%s: Buffer is not allocated yet ??\n", __func__);
		return -ENOMEM;
	}

	if (odin_get_stream_status(stream) == ODIN_PLATFORM_INIT ||
			odin_get_stream_status(stream) == ODIN_PLATFORM_STOP)
	{
#if 0
		struct mp3_header header;
		unsigned int channels, rate, bits;

		memcpy((unsigned char *)&header, (unsigned char *)buf,
				sizeof(struct mp3_header));

		if (!stream->compr_ops->parser_mp3_header(&header,
					&channels, &rate, &bits))
		{
			stream->sample_rate = rate;
			stream->num_channels = channels;
			stream->bit_rate = bits/1000;
			stream_info.sfreq = stream->sample_rate;
		}
#endif
		if (stream->compr_ops->init_command(cstream))
			return -EFAULT;
	}

	buffer_size = stream->fragment_size*stream->fragments;
	pr_debug("stream->app_pointer = 0x%08X\n", stream->app_pointer);
	dstn = stream->buffer + stream->app_pointer;
	pr_debug("stream->buffer_paddr + stream->app_pointer = 0x%08X\n",
			stream->buffer_paddr+stream->app_pointer);

	dstn_paddr = stream->buffer_paddr + stream->app_pointer;

	if (count < (buffer_size - stream->app_pointer))
	{
		if (copy_from_user(dstn, buf, count))
			return -EFAULT;
		
		spin_lock_irqsave(&stream->lock, flags);
		stream->byte_received += count;
		spin_unlock_irqrestore(&stream->lock, flags);

		if (odin_get_stream_status(stream)==ODIN_PLATFORM_RUNNING)
		{
			if (stream->compr_ops->play_command(cstream, dstn_paddr, count))
				return -EFAULT;
		}
		stream->app_pointer += count;
		/*stream->app_pointer += stream->fragment_size;*/
	} else {
		copy = buffer_size - stream->app_pointer;
		if (copy_from_user(dstn, buf, copy))
			return -EFAULT;
		
		spin_lock_irqsave(&stream->lock, flags);
		stream->byte_received += copy;
		spin_unlock_irqrestore(&stream->lock, flags);

		if ((odin_get_stream_status(stream)==ODIN_PLATFORM_RUNNING) && copy)
		{
			if (stream->compr_ops->play_command(cstream, dstn_paddr, copy))
				return -EFAULT;
		}

		if (copy_from_user(stream->buffer, buf+copy, count-copy))
			return -EFAULT;

		spin_lock_irqsave(&stream->lock, flags);
		stream->byte_received += (count-copy);
		spin_unlock_irqrestore(&stream->lock, flags);

		if ((odin_get_stream_status(stream)==
				ODIN_PLATFORM_RUNNING) && (count-copy))
		{
			if (stream->compr_ops->play_command(cstream,
						stream->buffer_paddr, (count-copy)))
				return -EFAULT;
		}
		stream->app_pointer = count - copy;
	}
	
	if (odin_get_stream_status(stream) == ODIN_PLATFORM_INIT)
	{
		stream_info.drain_ready = 0;
		stream_info.eos_ready = 0;

		if (stream->compr_ops->start_command(cstream))
			return -EFAULT;

		if (odin_compr_set_volume(cstream, volume[0], volume[1]))
		{
			pr_err("Set volume failed\n");
		}
		odin_set_stream_status(stream, ODIN_PLATFORM_RUNNING);

		return count;
	}

	if (count >= stream->fragment_size)
	{
#if 0
		pr_debug("odin_lpa_write_done\n");
		odin_lpa_write_done(PD_AUD, 0);
#endif
	}

	return count;
}

static int odin_compr_get_caps(struct snd_compr_stream *cstream,
				struct snd_compr_caps *arg)
{
	return 0;
}

static int odin_compr_get_codec_caps(struct snd_compr_stream *cstream,
				struct snd_compr_codec_caps *codec)
{
	return 0;
}

static int odin_compr_set_metadata(struct snd_compr_stream *cstream,
				struct snd_compr_metadata *metadata)
{
	return 0;
}

static int odin_compr_volume_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct odin_compr_data *pdata = (struct odin_compr_data *)
			snd_soc_platform_get_drvdata(platform);
	struct snd_compr_stream *cstream;
	uint32_t *volume = pdata->volume;

	volume[0] = ucontrol->value.integer.value[0];
	volume[1] = ucontrol->value.integer.value[1];

	if ((int32_t)volume[0] < -64)
	{
		pdata->volume[0] = -64;
		pdata->volume[1] = -64;
	}
	else if ((int32_t)volume[0] > 0)
	{
		pdata->volume[0] = 0;
		pdata->volume[1] = 0;
	}
	else
	{
		pdata->volume[0] = volume[0];
		pdata->volume[1] = volume[1];
	}
	pr_debug("%s: left_vol %d right_vol %d\n",
			__func__, volume[0], volume[1]);

	if (gstream)
	{
		cstream = gstream;
		odin_compr_set_volume(cstream, volume[0], volume[1]);
	}

	return 0;
}

static int odin_compr_volume_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct odin_compr_data *pdata =
		snd_soc_platform_get_drvdata(platform);
	uint32_t *volume = pdata->volume;
	pr_debug("%s\n", __func__);
	ucontrol->value.integer.value[0] = volume[0];
	ucontrol->value.integer.value[1] = volume[1];

	return 0;
}

static int odin_compr_normalizer_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct odin_compr_data *pdata =
		snd_soc_platform_get_drvdata(platform);

	ucontrol->value.integer.value[0]  = pdata->is_normalizer_enable;

	return 0;
}

static int odin_compr_normalizer_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct odin_compr_data *pdata =
		snd_soc_platform_get_drvdata(platform);
	struct snd_compr_stream *cstream;

#if 0
	if (pdata->is_normalizer_enable == ucontrol->value.integer.value[0])
	      return 0;
#endif

	pdata->is_normalizer_enable = ucontrol->value.integer.value[0];

	if (gstream)
	{
		struct snd_compr_runtime *runtime = gstream->runtime;
		struct odin_runtime_stream *stream = runtime->private_data;
		cstream = gstream;
		if (stream->compr_ops->set_normalizer(cstream, pdata->is_normalizer_enable))
		{
			pr_err("Set Normalizer failed\n");
		}
	}
	return 0;
}


static const struct snd_kcontrol_new odin_compr_controls[] = {
	SOC_DOUBLE_EXT_TLV("Compress Playback Volume",
			0,
			0, 8, COMPRESSED_LR_VOL_MAX_STEPS, 0,
			odin_compr_volume_get,
			odin_compr_volume_put,
			odin_compr_vol_gain),
	SOC_SINGLE_EXT("Compress Playback LG Normalizer Switch",
			0,0,1,0,odin_compr_normalizer_get, odin_compr_normalizer_set),

};

static struct snd_compr_ops odin_compr_ops = {
	.open		= odin_compr_open,
	.free		= odin_compr_free,
	.trigger	= odin_compr_trigger,
	.pointer	= odin_compr_pointer,
	.set_params	= odin_compr_set_params,
	.set_metadata	= odin_compr_set_metadata,
	.copy		= odin_compr_copy,
	.get_caps	= odin_compr_get_caps,
	.get_codec_caps = odin_compr_get_codec_caps,
};

static struct snd_soc_platform_driver odin_compr_platform = {
	.compr_ops	= &odin_compr_ops,
	.controls	= odin_compr_controls,
	.num_controls	= ARRAY_SIZE(odin_compr_controls),
};

static int odin_compr_probe(struct platform_device *pdev)
{
	int ret;
	struct odin_compr_data *pdata;

	pdata = kzalloc(sizeof(struct odin_compr_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		return ret;
	}
	pdata->volume[0] = 0;
	pdata->volume[1] = 0;
	pdata->is_normalizer_enable = 0;

	dev_set_name(&pdev->dev,"%s","odin-compress-dsp");
	dev_set_drvdata(&pdev->dev, pdata);

	ret = snd_soc_register_platform(&pdev->dev,
					&odin_compr_platform);
	if (ret)
	{
		dev_err(&pdev->dev, "Failed to set the soc register platform");
		kfree(pdata);
		pdata = NULL;
	}
	else
		dev_info(&pdev->dev, "Probing success!");

	return 0;
}

static int odin_compr_remove(struct platform_device *pdev)
{
	struct odin_compr_data *pdata =
		snd_soc_platform_get_drvdata(&pdev->dev);

	kfree(pdata);
	pdata = NULL;

	class_remove_file(dsp_dev_class, &class_attr_dsp_dump);

	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id odin_compr_dt_match[] = {
	{.compatible = "lge,odin-compress-dsp", },
	{},
};

static struct platform_driver odin_compr_driver = {
	.driver = {
		.name = "odin-compress-dsp",
		.owner = THIS_MODULE,
		.of_match_table = odin_compr_dt_match,
	},
	.probe = odin_compr_probe,
	.remove = odin_compr_remove,
};

static int odin_compr_platform_open(struct inode *inode,
		struct file *filp)
{
	return 0;
}

static int odin_compr_platform_close(struct inode *inode,
		struct file *filp)
{
	return 0;
}

static int odin_compr_platform_mmap(struct file *filp,
		struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;

	vma->vm_flags |= VM_IO;
	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);

	vma->vm_page_prot =
		phys_mem_access_prot(filp,
				vma->vm_pgoff,
				size,
				vma->vm_page_prot);

	if (remap_pfn_range(vma,
				vma->vm_start,
				vma->vm_pgoff,
				size,
				vma->vm_page_prot))
	{
		return -EAGAIN;
	}

	return 0;
}

static struct file_operations odin_compr_platform_fops =
{
	.owner			= THIS_MODULE,
	.open			= odin_compr_platform_open,
	.release		= odin_compr_platform_close,
	.mmap			= odin_compr_platform_mmap,
};

static int __init odin_compr_platform_init(void)
{
	if (register_chrdev(141,"odin_compress", &odin_compr_platform_fops))
		return -1;

	dsp_dev_class = class_create(THIS_MODULE, "odin_compress");
	if (IS_ERR(dsp_dev_class)) {
		pr_err("%s: Failed to create class(odin_compress)\n", __func__);
	}

	if (IS_ERR(device_create(dsp_dev_class,
			NULL,
			MKDEV(141,0),
			NULL,
			"dsp_dump")))
		pr_err("%s : Failed to device_create\n",__func__);

	return platform_driver_register(&odin_compr_driver);
}

module_init(odin_compr_platform_init);

static void __exit odin_compr_platform_exit(void)
{
	class_remove_file(dsp_dev_class, &class_attr_dsp_dump);

	platform_driver_unregister(&odin_compr_driver);

	unregister_chrdev(141,"odin_compress");
}
module_exit(odin_compr_platform_exit);

MODULE_AUTHOR("Heewan Park <heewan.park@lge.com>");
MODULE_DESCRIPTION("Odin Compress Offload platform driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:odin-compr");
