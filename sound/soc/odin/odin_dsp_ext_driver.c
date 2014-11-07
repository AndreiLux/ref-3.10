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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/of.h>
#include "odin_platform.h"
#include "../../../drivers/dsp/tensilica/dsp_alsa_drv.h"

struct device *ddev = NULL;

#if 1
struct timer_run_info {
	u32     is_running;
	u32     ring_buffer_addr;
	u32     ring_buffer_size;
	u32     ring_buffer_count;
	u32		buffer_ptr;
	u32	total_buffer_count;
	u32	period_count;
	u32     period_msec;
	u32     period_size;
	u32     period_index;
	u32     elapsed_count;
	u32     ops;
	void	*alsa_substream;
	void	(*period_elapsed) (void *alsa_substream);
	struct timer_list	timer;
};

struct timer_run_info run_info = { 0 };


void timer_run_info_timer(unsigned long data)
{
	struct timer_run_info *trun_info = (struct timer_run_info *)data;
 
	//dev_info(ddev, "dummy_run_test timer, period_msec %d\n",	run_info.period_msec);

	trun_info->elapsed_count += trun_info->period_count;
	trun_info->period_elapsed(trun_info->alsa_substream);
	trun_info->elapsed_count = 0;
	
	if(++trun_info->period_index == trun_info->period_size/trun_info->period_count) 
		trun_info->period_index = 0;

}
#endif

static int odin_dsp_ext_pcm_open(struct odin_stream_params *str_param)
{
	int stream_id = 1;
	struct odin_stream_params *stream_p = str_param;
	dev_info(ddev, "odin_dsp_ext_pcm_open called\n");

	dev_info(ddev, "odin_stream_params result %d\n", stream_p->result);
	dev_info(ddev, "odin_stream_params stream_id %d\n",
 							stream_p->stream_id);
	dev_info(ddev, "odin_stream_params codec %d\n", stream_p->codec);
	dev_info(ddev, "odin_stream_params ops %d\n", stream_p->ops);
	dev_info(ddev, "odin_stream_params stream_type %d\n",
							stream_p->stream_type);
	dev_info(ddev, "odin_stream_params device_type %d\n",
							stream_p->device_type);

	dev_info(ddev, "odin_pcm_params codec %d\n", stream_p->sparams.codec);
	dev_info(ddev, "odin_pcm_params num_chan %d\n",
						stream_p->sparams.num_chan);
	dev_info(ddev, "odin_pcm_params pcm_wd_sz %d\n",
						stream_p->sparams.pcm_wd_sz);
	dev_info(ddev, "odin_pcm_params sfreq %d\n", stream_p->sparams.sfreq);

	dev_info(ddev, "odin_pcm_params ring_buffer_size %d\n",
					stream_p->sparams.ring_buffer_size);
	dev_info(ddev, "odin_pcm_params ring_buffer_count %d\n",
					stream_p->sparams.ring_buffer_count);
	dev_info(ddev, "odin_pcm_params period_size %d\n",
					stream_p->sparams.period_size);
	dev_info(ddev, "odin_pcm_params period_count %d\n",
					stream_p->sparams.period_count);
	dev_info(ddev, "odin_pcm_params ring_buffer_addr 0x%x\n",
					stream_p->sparams.ring_buffer_addr);


//#if defined(CONFIG_TEST_TIMER_RUN)
#if 1
	memset(&run_info,0x0,sizeof(struct timer_run_info));
	run_info.ring_buffer_addr = stream_p->sparams.ring_buffer_addr;
	run_info.ring_buffer_size = stream_p->sparams.ring_buffer_size;
	run_info.ring_buffer_count = stream_p->sparams.ring_buffer_count;
	run_info.total_buffer_count = stream_p->sparams.ring_buffer_count;
	run_info.period_count =	stream_p->sparams.period_count;
	run_info.period_msec = ((stream_p->sparams.period_count*1000)/
						stream_p->sparams.sfreq);
	run_info.period_size = stream_p->sparams.period_size;
	run_info.ops = stream_p->ops;
	if(run_info.period_msec == 0)
		run_info.period_msec = 10;
	dev_info(ddev, "dummy_run_test period_msec %d\n", run_info.period_msec);
#endif

	return stream_id;
}

static int odin_dsp_ext_pcm_device_control(int cmd, void *arg)
{
	switch(cmd) {
	case ODIN_SND_RESUME:
	case ODIN_SND_START: {
		int stream_id = *(int *)arg;
#if 0
		dev_info(ddev, "DSP device control ODIN_SND_START\n");
		dev_info(ddev, "stream_id %d\n", stream_id);
#endif
		run_info.is_running = 1;
		run_info.period_index = 0;
		if(run_info.ops == STREAM_OPS_PLAYBACK)
		{
			//dsp_alsa_pcm_write(run_info.ring_buffer_addr+run_info.buffer_ptr, run_info.ring_buffer_size, run_info.period_size, timer_run_info_timer, (unsigned long *)&run_info);
			dsp_alsa_pcm_write(run_info.ring_buffer_addr, run_info.ring_buffer_size, run_info.period_size, timer_run_info_timer, (unsigned long *)&run_info);
		}
		else
		{
			dsp_alsa_pcm_read(run_info.ring_buffer_addr, run_info.ring_buffer_size, run_info.period_size, timer_run_info_timer, (unsigned long *)&run_info);
		}
		break;
	}
	case ODIN_SND_PAUSE:
	case ODIN_SND_DROP: {
		int stream_id = *(int *)arg;
#if 0
		dev_info(ddev, "DSP device control ODIN_SND_DROP\n");
		dev_info(ddev, "stream_id %d\n", stream_id);
#endif
		run_info.is_running = 0;

		dsp_alsa_pcm_stop();
		break;
	}
	case ODIN_SND_ALLOC:
		dev_info(ddev, "DSP device control ODIN_SND_ALLOC\n");
		break;
	case ODIN_SND_FREE:
		dev_info(ddev, "DSP device control ODIN_SND_FREE\n");
		break;
	case ODIN_SND_BUFFER_POINTER: {
		struct pcm_stream_info *stream_info;
		stream_info = (struct pcm_stream_info *)arg;
#if 0
		stream_info->buffer_ptr += run_info.elapsed_count;
		if(stream_info->buffer_ptr >= run_info.total_buffer_count)
		{
			stream_info->buffer_ptr = 0;
		}
#endif
		stream_info->buffer_ptr = run_info.period_index*run_info.period_count; 
		//dev_info(ddev, "DSP device control ODIN_SND_BUFFER_POINTER %d\n",stream_info->buffer_ptr);

		run_info.buffer_ptr = stream_info->buffer_ptr;
		break;
	}
	case ODIN_SND_STREAM_INIT: {
		struct pcm_stream_info *stream_info;
		stream_info = (struct pcm_stream_info *)arg;
		dev_info(ddev, "DSP device control ODIN_SND_STREAM_INIT\n");
		dev_info(ddev, "pcm_stream_info str_id %d\n",
							stream_info->str_id);
		dev_info(ddev, "pcm_stream_info buffer_ptr %lld\n",
						stream_info->buffer_ptr);
		dev_info(ddev, "pcm_stream_info sfreq %d\n",
							stream_info->sfreq);

#if 1
		run_info.period_elapsed = stream_info->period_elapsed;
		run_info.alsa_substream = stream_info->alsa_substream;
#endif
		break;
	}
	case ODIN_SND_HW_SET: {
		struct odin_pcm_dev_params *dev_params;
		dev_params = (struct odin_pcm_dev_params *)arg;
		dev_info(ddev, "DSP device control ODIN_SND_HW_SET\n");
		dev_info(ddev, "odin_pcm_dev_params master_mode %d\n",
						dev_params->master_mode);
		dev_info(ddev, "odin_pcm_dev_params i2s_format %d\n",
						dev_params->i2s_format);
		break;
	}
	default:
		dev_err(ddev, "Unknown command\n");
	break;
	}

	return 0;
}

static int odin_dsp_ext_pcm_close(unsigned int str_id)
{
	dev_info(ddev, "odin_dsp_ext_pcm_close called\n");
	dev_info(ddev, "stream_id %d\n", str_id);
	return 0;
}

struct odin_dsp_ops dsp_pcm_ops = {
	.open = odin_dsp_ext_pcm_open,
	.device_control = odin_dsp_ext_pcm_device_control,
	.close = odin_dsp_ext_pcm_close,
};

static int odin_dsp_ext_probe(struct platform_device *pdev)
{
	struct odin_device *dsp_device = NULL;
	dsp_device = kzalloc(sizeof(struct odin_device), GFP_KERNEL);
	if (!dsp_device)
		return -ENOMEM;

	dsp_device->name = pdev->name;
	dsp_device->dev = &pdev->dev;
	dsp_device->ops = &dsp_pcm_ops;
	dsp_device->compr_ops = NULL;
	pdev->dev.platform_data = dsp_device;
	ddev = &pdev->dev;
	return odin_register_dsp(dsp_device);
}

static int odin_dsp_ext_remove(struct platform_device *pdev)
{
	struct odin_device *dsp_device = pdev->dev.platform_data;
	if(dsp_device)
		kfree(dsp_device);
	ddev = NULL;
	return 0;
}

static const struct of_device_id odin_dsp_ext_match[] = {
	{ .compatible = "lge,odin-dsp-ext", },
	{},
};

static struct platform_driver odin_dsp_ext_driver = {
	.driver		= {
		.name		= "odin-dsp-ext",
		.owner		= THIS_MODULE,
		.of_match_table = of_match_ptr(odin_dsp_ext_match),
	},
	.probe		= odin_dsp_ext_probe,
	.remove		= odin_dsp_ext_remove,
};

module_platform_driver(odin_dsp_ext_diver);
