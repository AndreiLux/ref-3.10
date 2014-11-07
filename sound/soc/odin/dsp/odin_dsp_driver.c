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
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/odin_mailbox.h>
#include <linux/odin_pd.h>
#include <linux/odin_lpa.h>
#include <asm/io.h>
#include <linux/irq.h>			/**< For isr */
#include <linux/sched.h>			/**< For isr */
#include "odin_dsp_driver.h"
#include "dsp_cfg.h"
#include "../odin_compress.h"
#include "tinymp3.h"
#include "mailbox/mailbox_hal.h"
#include "firmware/ADSP_FW_data.h"
#include "firmware/ADSP_FW_inst.h"

#include <sound/compress_offload.h>
#include <sound/compress_params.h>
#include <sound/compress_driver.h>
#include <sound/pcm.h>
#include <linux/ion.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>

#include "../odin_aud_powclk.h"

static int odin_dsp_of_property_get(struct platform_device *pdev,
		struct odin_dsp_device *odin_dsp_device);

static void
odin_dsp_reset_off(struct odin_dsp_device *odin_dsp_device);

static int
odin_dsp_download_dram(struct odin_dsp_device *odin_dsp_device);

static int
odin_dsp_download_iram(struct odin_dsp_device *odin_dsp_device);

static int
odin_dsp_reset_on(struct odin_dsp_device *odin_dsp_device);

static int
odin_dsp_compr_reset_emergency(struct snd_compr_stream *cstream);

struct odin_dsp_device *odin_dsp_device;

DSP_MEM_CFG *pst_dsp_mem_cfg = &dsp_mem_cfg[1];

void odin_dsp_workqueue(struct work_struct *unused);
struct workqueue_struct *workqueue_odin_dsp;
DECLARE_WORK(odin_dsp_work, odin_dsp_workqueue);
struct work_struct odin_dsp_work;

EVENT_LPA *event_lpa;

static void
odin_dsp_reset_off(struct odin_dsp_device *odin_dsp_device)
{
	struct aud_control_odin *reg_ctl = odin_dsp_device->aud_reg_ctl;
	reg_ctl->dsp_control.dsp_reset = 0x0;
	reg_ctl->dsp_control.ocd_halt_on_reset = 0x1;
}

static int
odin_dsp_reset_on(struct odin_dsp_device *odin_dsp_device)
{
	struct aud_control_odin *reg_ctl = odin_dsp_device->aud_reg_ctl;

	reg_ctl->dsp_control.ocd_halt_on_reset = 0x0;
	reg_ctl->dsp_control.dsp_reset = 0x1;

	schedule_timeout_interruptible(5);

	return 0;
}

static int
odin_dsp_download_dram(struct odin_dsp_device *odin_dsp_device)
{
	int ret = 0;
	struct odin_dsp_mem_ctl *mem_ctl = odin_dsp_device->aud_mem_ctl;
	struct aud_control_odin *reg_ctl = odin_dsp_device->aud_reg_ctl;
	struct dsp_buffer firmware_buffer;
	unsigned int adsp_fw_data_size = 0;

	firmware_buffer.buffer_virtual_address = ADSP_FW_data;
	firmware_buffer.buffer_size = sizeof(ADSP_FW_data);

	reg_ctl->dsp_remap.dsp_remap = ODIN_REMAP_VALUE;
	memset(&reg_ctl->dsp_control, 0x0, sizeof(DSP_CONTROL));

	odin_dsp_reset_off(odin_dsp_device);

	/* Audio DSP DRAM Mux Enable */
	reg_ctl->dram_mem_ctl.dram_mux_control = 0xCF;

	/* Download Data */
	adsp_fw_data_size = firmware_buffer.buffer_size;

	if (adsp_fw_data_size >
			(mem_ctl->dsp_dram0_size + mem_ctl->dsp_dram1_size))
	{
		pr_err("overflow size of dsp data-ram binary : %s:%i \n",
				__FILE__,__LINE__);
		return -1;
	}

	if (adsp_fw_data_size > mem_ctl->dsp_dram1_size - 0x4000)
	{
		memcpy((unsigned char *)(mem_ctl->dsp_dram1_address + 0x4000/4),
				(unsigned char *)firmware_buffer.buffer_virtual_address,
				(mem_ctl->dsp_dram1_size - 0x4000));
		memcpy((unsigned char *)(mem_ctl->dsp_dram0_address),
				(unsigned char *)(firmware_buffer.buffer_virtual_address+
					(mem_ctl->dsp_dram1_size - 0x4000)),
				adsp_fw_data_size-(mem_ctl->dsp_dram1_size - 0x4000));
	}

	else
	{
		memcpy((unsigned char *)(mem_ctl->dsp_dram0_address),
				(unsigned char *)firmware_buffer.buffer_virtual_address,
				adsp_fw_data_size);
	}

	/* Audio DSP DRAM Mux Disable */
	reg_ctl->dram_mem_ctl.dram_mux_control = 0x0;

	return ret;
}

static int
odin_dsp_download_iram(struct odin_dsp_device *odin_dsp_device)
{
	int ret = 0;
	struct dsp_buffer firmware_buffer;
	struct odin_dsp_mem_ctl *mem_ctl = odin_dsp_device->aud_mem_ctl;
	struct aud_control_odin *reg_ctl = odin_dsp_device->aud_reg_ctl;
	unsigned int adsp_fw_inst_size = 0;

	firmware_buffer.buffer_virtual_address = ADSP_FW_inst;
	firmware_buffer.buffer_size = sizeof(ADSP_FW_inst);

	/* Audio DSP IRAM Mux Enable */
	reg_ctl->iram_mem_ctl.iram_mux_control = 0x3;

	/* Download Instruction */
	adsp_fw_inst_size = firmware_buffer.buffer_size;

	if (adsp_fw_inst_size > mem_ctl->dsp_iram0_size)
	{
		memcpy((unsigned char *)(mem_ctl->dsp_iram0_address),
				(unsigned char *)firmware_buffer.buffer_virtual_address,
				mem_ctl->dsp_iram0_size);
		memcpy((unsigned char *)(mem_ctl->dsp_iram1_address),
				(unsigned char *)(firmware_buffer.buffer_virtual_address+
					mem_ctl->dsp_iram0_size),
				adsp_fw_inst_size-mem_ctl->dsp_iram0_size);
	}
	else
	{
		memcpy((unsigned char *)(mem_ctl->dsp_iram0_address),
				(unsigned char *)firmware_buffer.buffer_virtual_address,
				adsp_fw_inst_size);
	}

	/* Audio DSP IRAM Mux Disable */
	reg_ctl->iram_mem_ctl.iram_mux_control = 0x0;

	return ret;
}

static int odin_dsp_compr_parse_mp3_header(
		struct mp3_header *header, unsigned int *num_channels,
		unsigned int *sample_rate, unsigned int *bit_rate)
{
	int ver_idx, mp3_version, layer, bit_rate_idx, sample_rate_idx, channel_idx;

	/* check sync bits */
	if ((header->sync & MP3_SYNC) != MP3_SYNC) {
		pr_err("%s:Error: Can't find MP3 header\n",__func__);
		return -1;
	}
	ver_idx = (header->sync >> 11) & 0x03;
	mp3_version = ver_idx == 0 ? MPEG25 : ((ver_idx & 0x1) ? MPEG1 : MPEG2);
	layer = 4 - ((header->sync >> 9) & 0x03);
	bit_rate_idx = ((header->format1 >> 4) & 0x0f);
	sample_rate_idx = ((header->format1 >> 2) & 0x03);
	channel_idx = ((header->format2 >> 6) & 0x03);

	if (sample_rate_idx == 3 || layer == 4 || bit_rate_idx == 15) {
		pr_err("%s:Error: Can't find valid header\n",__func__);
		return -1;
	}
	*num_channels = (channel_idx == MONO ? 1 : 2);
	*sample_rate = mp3_sample_rates[mp3_version][sample_rate_idx];
	*bit_rate = (mp3_bit_rates[mp3_version][layer - 1][bit_rate_idx]) * 1000;

	return 0;
}

static int odin_dsp_compr_ready_command(void)
{
	unsigned int transaction_number;
	MAILBOX_DATAREGISTER send_data_register;

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number = hal_set_command(&send_data_register, READY, REQUEST);

	hal_set_parameter(8,
			(void *)&send_data_register,
			READY,
			24,
			24,
			24,
			24,
			pst_dsp_mem_cfg->sys_table_addr,
			pst_dsp_mem_cfg->sys_table_size);

	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

	if (!wait_event_interruptible_timeout(event_lpa->wait,
				event_lpa->command == READY,
				HZ))
	{
		pr_err("%s:Can't received READY in timeout\n",__func__);
			return -EFAULT;
	}

	return 0;
}

static int odin_dsp_compr_create_command(void)
{
	unsigned int transaction_number;
	MAILBOX_DATAREGISTER send_data_register;

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number = hal_set_command(&send_data_register, CREATE, REQUEST);

	hal_set_parameter(5, &send_data_register, CREATE, DECODE, LPA_MODE, 48000);
	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

	if (!wait_event_interruptible_timeout(event_lpa->wait,
				event_lpa->command == CREATE,HZ))
	{
		pr_err("%s:Can't received CREATE in timeout\n",__func__);
			return -EFAULT;
	}

	return 0;
}

static int odin_dsp_compr_open_command(void)
{
	unsigned int transaction_number;
	MAILBOX_DATAREGISTER send_data_register;

	pr_debug("%s\n", __func__);

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number = hal_set_command(&send_data_register, OPEN, REQUEST);

	hal_set_parameter(3, &send_data_register, OPEN, MP3);
	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

	if (!wait_event_interruptible_timeout(event_lpa->wait,
				event_lpa->command == OPEN,HZ))
	{
		pr_err("%s:Can't received OPEN in timeout\n",__func__);
		return -EFAULT;
	}

	return 0;
}

static int odin_dsp_compr_init_command(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;
	unsigned int transaction_number;
	MAILBOX_DATAREGISTER send_data_register;
	LPA_AUDIO_INFO audio_info;
	unsigned int stereo;
	unsigned int woofer;

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number =
		hal_set_command(&send_data_register,
				CONTROL_INIT,
				REQUEST);

	audio_info.sampling_freq = stream->sample_rate;
	stereo = stream->num_channels;
	woofer = 0;
	audio_info.bit_rate = stream->bit_rate;

	hal_set_parameter(10,
		&send_data_register,
		CONTROL_INIT,
		stereo,
		woofer,
		audio_info.sampling_freq,
		16,
		audio_info.bit_rate,
		1,
		1,
		1);

	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

	if (!wait_event_interruptible_timeout(event_lpa->wait,
				event_lpa->command == CONTROL_INIT,HZ))
	{
		pr_err("%s:Can't received CONTROL_INIT in timeout\n",__func__);
		if (odin_dsp_compr_reset_emergency(cstream))
			return -EFAULT;
	}

	return 0;
}

static int odin_dsp_compr_open(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;

	stream->buffer_paddr = pst_dsp_mem_cfg->stream_buffer_addr;
	stream->buffer = pst_dsp_mem_cfg->stream_buffer_vaddr;

	return 0;
}

#if 0
static int odin_dsp_configure(struct snd_compr_stream *cstream)
{
#if 0
	struct ion_client *client;
	struct ion_handle *handle;
	void * kernel_va;

	client = odin_ion_client_create( "aud_ion" );
	handle= ion_alloc( client, ODIN_ION_CARVEOUT_AUD_SIZE, 0x1000,
			(1<<ODIN_ION_SYSTEM_HEAP1), 0, 0 );
	kernel_va = ion_map_kernel( client, handle );
#endif

}
#endif

static int odin_dsp_compr_set_params(struct snd_compr_stream *cstream,
		struct snd_compr_params *params)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;

	switch(params->codec.id)
	{
		case SND_AUDIOCODEC_PCM :
			stream->codec = WAV;
			break;
		case SND_AUDIOCODEC_MP3 :
			stream->codec = MP3;
			break;
		default:
			break;
	}

	switch(params->codec.sample_rate)
	{
		case SNDRV_PCM_RATE_5512:
			stream->sample_rate = KHZ_5512;
			break;
		case SNDRV_PCM_RATE_8000:
			stream->sample_rate = KHZ_8;
			break;
		case SNDRV_PCM_RATE_11025:
			stream->sample_rate = KHZ_11025;
			break;
		case SNDRV_PCM_RATE_16000:
			stream->sample_rate = KHZ_16;
			break;
		case SNDRV_PCM_RATE_22050:
			stream->sample_rate = KHZ_22050;
			break;
		case SNDRV_PCM_RATE_32000:
			stream->sample_rate = KHZ_32;
			break;
		case SNDRV_PCM_RATE_44100:
			stream->sample_rate = KHZ_441;
			break;
		case SNDRV_PCM_RATE_48000:
			stream->sample_rate = KHZ_48;
			break;
		case SNDRV_PCM_RATE_64000:
			stream->sample_rate = KHZ_64;
			break;
		case SNDRV_PCM_RATE_88200:
			stream->sample_rate = KHZ_882;
			break;
		case SNDRV_PCM_RATE_96000:
			stream->sample_rate = KHZ_96;
			break;
		case SNDRV_PCM_RATE_176400:
			stream->sample_rate = KHZ_1764;
			break;
		case SNDRV_PCM_RATE_192000:
			stream->sample_rate = KHZ_192;
			break;
	}

	stream->num_channels = params->codec.ch_out;
	stream->bit_rate = params->codec.bit_rate/1000;

	return odin_dsp_compr_init_command(cstream);
}

#if 0
static int odin_dsp_compr_tstamp(struct snd_compr_stream *cstream,
		struct snd_compr_tstamp *tstamp)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;
	MAILBOX_DATAREGISTER send_data_register;
	unsigned int transaction_number;
	long long timestamp;
	uint32_t frame_size;

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number =
		hal_set_command(&send_data_register,
				CONTROL_GET_TIMESTAMP,
				REQUEST);

	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

	if (!wait_event_interruptible_timeout(event_lpa->wait,
				event_lpa->command == CONTROL_GET_TIMESTAMP,
				msecs_to_jiffies(100)))
	{
		pr_err("%s:Can't received CONTROL_GET_TIMESTAMP in timeout\n",__func__);
	}

	timestamp = event_lpa->timestamp.value;

	//tstamp->sampling_rate = stream->sample_rate;
	//tstamp->byte_offset = stream->byte_offset;
	//tstamp->copied_total = stream->copied_total;

	tstamp->pcm_io_frames = (snd_pcm_uframes_t)div64_u64(
			(uint64_t)timestamp*stream->sample_rate,1000000);

	pr_debug("tstamp->pcm_io_frames = %lld\n",(int64_t)tstamp->pcm_io_frames);
	if (timestamp & 0xffff == 0xffff)
	{
		tstamp->pcm_io_frames = (snd_pcm_uframes_t)div64_u64(
				event_lpa->decoded_frame.value*stream->sample_rate,
				(uint64_t)(stream->bit_rate/8*1024));
	}

	return 0;
}
#else
static int odin_dsp_compr_tstamp(struct snd_compr_stream *cstream,
		struct snd_compr_tstamp *tstamp)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;
	long long timestamp;

	if (!odin_dsp_device)
		return -EFAULT;

	event_lpa->timestamp.field.timestamp_upper = odin_dsp_device->aud_reg_ctl->
		dsp_temp_reg2.temp_reg2;
	event_lpa->timestamp.field.timestamp_lower = odin_dsp_device->aud_reg_ctl->
		dsp_temp_reg3.temp_reg3;

	timestamp = event_lpa->timestamp.value;

	tstamp->pcm_io_frames = (snd_pcm_uframes_t)div64_u64(
			(uint64_t)timestamp*stream->sample_rate,1000000);

	return 0;
}
#endif


static int odin_dsp_compr_play_command(struct snd_compr_stream *cstream,
		uint32_t buffer_paddr, size_t size)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;
	unsigned int nearstop_threshold;
	unsigned int transaction_number;
	uint32_t temp_size;
	MAILBOX_DATAREGISTER send_data_register;

	memcpy(gstream,cstream,sizeof(struct snd_compr_stream));

	stream->stream_info.alsa_substream = cstream;
	stream_info.alsa_substream = stream->stream_info.alsa_substream;
	stream_info.fragment_elapsed = stream->stream_info.fragment_elapsed;

	temp_size = 0;
	if (stream->temp_size)
	{
		temp_size = stream->temp_size;
		stream->temp_size = 0;
	}

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number =
		hal_set_command(&send_data_register,
				CONTROL_PLAY_PARTIAL,
				REQUEST);

	if (size == stream->fragment_size)
		nearstop_threshold = NEARSTOP_THRESHOLD;
	else
		nearstop_threshold = 0;

	hal_set_parameter(5,
			&send_data_register,
			CONTROL_PLAY_PARTIAL,
			buffer_paddr-temp_size,
			size+temp_size,
			nearstop_threshold);

	hal_send_mailbox(CH0_REQ_RES, &send_data_register);
	pr_debug("%s:count = %d\n", __func__, size+temp_size);

	return 0;
}

static int odin_dsp_compr_set_volume(struct snd_compr_stream *cstream,
		uint32_t volume_l, uint32_t volume_r)
{
	unsigned int nearstop_threshold;
	unsigned int transaction_number;
	MAILBOX_DATAREGISTER send_data_register;

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number =
		hal_set_command(&send_data_register,
				CONTROL_GAIN,
				REQUEST);

	nearstop_threshold = NEARSTOP_THRESHOLD;
	hal_set_parameter(4,
			&send_data_register,
			CONTROL_GAIN,
			(unsigned int)volume_l);

	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

	if (!wait_event_interruptible_timeout(event_lpa->wait,
				event_lpa->command == CONTROL_GAIN,
				msecs_to_jiffies(100)))
	{
		pr_err("%s:Can't received CONTROL_GAIN in timeout\n",__func__);
		return -EFAULT;
	}
	return 0;
}

static int odin_dsp_compr_set_normalizer(struct snd_compr_stream *cstream,
		unsigned int is_normalizer_enable)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;
	unsigned int transaction_number;
	MAILBOX_DATAREGISTER send_data_register;
	void __iomem *aud_crg_base;

	aud_crg_base = ioremap(0x34670000, 0x100);
	if (!aud_crg_base) {
		pr_err("%s:audio_crg ioremap failed\n",__func__);
		return -ENOMEM;
	}

	/* Change PLL Clock if normalizer enable */
	if (is_normalizer_enable)
	{
		writel(0x1, aud_crg_base + 0x10);	/* pll clock select = 400 Mhz*/
		writel(0x6, aud_crg_base + 0x18);	/* div_CCLK 1 = 36.36 Mhz */
		writel(0x0, aud_crg_base + 0x20);	/* div_ACLK 1 = 36.36 Mhz*/
		writel(0x0, aud_crg_base + 0x28);	/* div_PCLK 2 = 36.36 Mhz*/
	}

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number =
		hal_set_command(&send_data_register,
				NORMALIZER,
				REQUEST);

	hal_set_parameter(4,
			&send_data_register,
			NORMALIZER,
			is_normalizer_enable);

	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

	if (!wait_event_interruptible_timeout(event_lpa->wait,
				event_lpa->command == NORMALIZER,
				msecs_to_jiffies(100)))
	{
		pr_err("%s:Can't received NORMALIZER in timeout\n",__func__);

		return -EFAULT;
	}

	/* Change OSC Clock if normalizer disable */
	if (!is_normalizer_enable)
	{
		writel(0x0, aud_crg_base + 0x10);	/* osc clodk select = 24Mhz*/
		writel(0x0, aud_crg_base + 0x18);	/* div_CCLK = 24Mhz */
		writel(0x0, aud_crg_base + 0x20);	/* div_ACLK = 24Mhz*/
		writel(0x0, aud_crg_base + 0x28);	/* div_PCLK = 24Mhz*/
	}

	iounmap(aud_crg_base);

	return 0;
}


static int odin_dsp_compr_start_command(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;
	unsigned int nearstop_threshold;
	unsigned int transaction_number;
	MAILBOX_DATAREGISTER send_data_register;

	gstream = cstream;
	stream->stream_info.alsa_substream = cstream;
	stream_info.alsa_substream = stream->stream_info.alsa_substream;
	stream_info.fragment_elapsed = stream->stream_info.fragment_elapsed;

	stream->temp_size = 0;
	if (stream->start_count < stream->fragment_size)
	{
		pr_info("%s:start_count is less than 32KB : %d\n",
				__func__, stream->start_count);
		stream->temp_size = stream->start_count;
		return 0;
	}

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number =
		hal_set_command(&send_data_register,
				CONTROL_PLAY_PARTIAL,
				REQUEST);

	nearstop_threshold = NEARSTOP_THRESHOLD;
	hal_set_parameter(5,
			&send_data_register,
			CONTROL_PLAY_PARTIAL,
			stream->buffer_paddr,
			stream->start_count,
			nearstop_threshold);

	hal_send_mailbox(CH0_REQ_RES, &send_data_register);
	pr_debug("%s:count = %d\n", __func__, stream->start_count);

#if 0
	pr_debug("odin_lpa_write_done\n");
	odin_lpa_write_done(PD_AUD, 0);
#endif
	return 0;
}

static int odin_dsp_compr_stop_command(struct snd_compr_stream *cstream)
{
	MAILBOX_DATAREGISTER send_data_register;
	unsigned int transaction_number;

	pr_debug("%s\n", __func__);

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number =
		hal_set_command(&send_data_register,
				CONTROL_STOP,
				REQUEST);

	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

	if (!wait_event_interruptible_timeout(event_lpa->wait,
				event_lpa->command == CONTROL_STOP,
				msecs_to_jiffies(100)))
	{
		pr_err("%s:Can't received CONTROL_STOP in timeout\n",__func__);

		if (odin_dsp_compr_reset_emergency(cstream))
			return -EFAULT;
	}

	return 0;
}

static int odin_dsp_compr_pause_command(void)
{
	MAILBOX_DATAREGISTER send_data_register;
	unsigned int transaction_number;

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number =
		hal_set_command(&send_data_register,
				CONTROL_PAUSE,
				REQUEST);

	hal_set_parameter(3,
			&send_data_register,
			CONTROL_PAUSE,
			0);

	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

	if (!wait_event_interruptible_timeout(event_lpa->wait,
				event_lpa->command == CONTROL_PAUSE,
				msecs_to_jiffies(100)))
	{
		pr_err("%s:Can't received CONTROL_PAUSE in timeout\n",__func__);
		return -1;
	}

	return 0;
}

static int odin_dsp_compr_close_command(void)
{
	MAILBOX_DATAREGISTER send_data_register;
	unsigned int transaction_number;

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number =
		hal_set_command(&send_data_register,
				CLOSE,
				REQUEST);

	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

	if (!wait_event_interruptible_timeout(event_lpa->wait,
				event_lpa->command == CLOSE,
				msecs_to_jiffies(100)))
	{
		pr_err("%s:Can't received CLOSE in timeout\n",__func__);
		return -1;
	}

	return 0;
}


static int odin_dsp_compr_resume_command(void)
{
	MAILBOX_DATAREGISTER send_data_register;
	unsigned int transaction_number;

	memset(&send_data_register, 0, sizeof(MAILBOX_DATAREGISTER));
	transaction_number =
		hal_set_command(&send_data_register,
				CONTROL_RESUME,
				REQUEST);

	hal_send_mailbox(CH0_REQ_RES, &send_data_register);

	if (!wait_event_interruptible_timeout(event_lpa->wait,
				event_lpa->command == CONTROL_RESUME,
				msecs_to_jiffies(100)))
	{
		pr_err("%s:Can't received CONTROL_RESUME in timeout\n",__func__);
		return -1;
	}

	return 0;
}

static int odin_dsp_compr_reset_emergency(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;

	odin_dsp_reset_off(odin_dsp_device);
	odin_dsp_reset_on(odin_dsp_device);

	switch (odin_get_stream_status(stream))
	{
		case ODIN_PLATFORM_OPENED:
			if (odin_dsp_compr_ready_command())
				return -EFAULT;
			break;
		case ODIN_PLATFORM_READY:
			if (odin_dsp_compr_ready_command())
				return -EFAULT;
			if (odin_dsp_compr_create_command())
				return -EFAULT;
			break;
		case ODIN_PLATFORM_CREATE:
		case ODIN_PLATFORM_STOP:
			if (odin_dsp_compr_ready_command())
				return -EFAULT;
			if (odin_dsp_compr_create_command())
				return -EFAULT;
			if (odin_dsp_compr_open_command())
				return -EFAULT;
			break;
		case ODIN_PLATFORM_OPEN:
		case ODIN_PLATFORM_INIT:
			if (odin_dsp_compr_ready_command())
				return -EFAULT;
			if (odin_dsp_compr_create_command())
				return -EFAULT;
			if (odin_dsp_compr_open_command())
				return -EFAULT;
			if (odin_dsp_compr_init_command(cstream))
				return -EFAULT;
			break;
		default:
			break;
	}

	return 0;
}

static int odin_dsp_compr_control(int cmd, struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;
	int rc;
	unsigned long flags;
	int bytes_available;
	unsigned int wait_time_ms;

	switch (cmd)
	{
		case SNDRV_PCM_TRIGGER_START:
			pr_info("%s : DSP_LPA_START\n",__func__);
			stream_info.drain_ready = 0;
			stream_info.eos_ready = 0;

			if (odin_dsp_compr_start_command(cstream))
				return -1;
			odin_set_stream_status(stream, ODIN_PLATFORM_RUNNING);
#if 0
			pr_debug("odin_lpa_start\n");
			odin_lpa_start(PD_AUD, 0);
#endif
			break;
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			pr_info("%s : DSP_LPA_PAUSE\n",__func__);
			if (odin_get_stream_status(stream)==ODIN_PLATFORM_RUNNING)
			{
				if (odin_dsp_compr_pause_command())
					return -1;
				odin_set_stream_status(stream, ODIN_PLATFORM_PAUSED);
			}
			break;
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			pr_info("%s : DSP_LPA_RESUME\n",__func__);
			if (odin_get_stream_status(stream)==ODIN_PLATFORM_PAUSED)
			{
				if (odin_dsp_compr_resume_command())
					return -1;
				odin_set_stream_status(stream, ODIN_PLATFORM_RUNNING);
			}
			break;
		case SNDRV_PCM_TRIGGER_STOP:
			pr_info("%s : DSP_LPA_STOP\n",__func__);

			if (atomic_read(&stream_info.eos))
			{
				stream_info.drain_ready = 2;
				stream_info.eos_ready = 2;
				wake_up(&stream_info.drain_wait);
				wake_up(&stream_info.eos_wait);
				atomic_set(&stream_info.drain,0);
				atomic_set(&stream_info.eos,0);
			}

			odin_set_stream_status(stream, ODIN_PLATFORM_STOP);
			if (odin_dsp_compr_stop_command(cstream))
				return -1;

			spin_lock_irqsave(&stream->lock, flags);
			stream->byte_offset = 0;
			stream->app_pointer = 0;
			stream->copied_total = 0;
			stream->byte_received = 0;
			spin_unlock_irqrestore(&stream->lock, flags);

			//odin_set_stream_status(stream, ODIN_PLATFORM_OPEN);
#if 0
			pr_debug("odin_lpa_stop\n");
			odin_lpa_stop(PD_AUD, 0);
#endif

			break;
		case SND_COMPR_TRIGGER_DRAIN:
			pr_debug("%s:SNDRV_COMPRESS_DRAIN\n",__func__);
		case SND_COMPR_TRIGGER_PARTIAL_DRAIN:
			pr_debug("%s:SND_COMPR_TRIGGER_PARTIAL\n",__func__);
			if (odin_get_stream_status(stream)!=ODIN_PLATFORM_RUNNING)
				return 0;

			spin_lock_irqsave(&stream->lock, flags);
			atomic_set(&stream_info.eos, 1);
			spin_unlock_irqrestore(&stream->lock, flags);
			bytes_available = stream->byte_received - stream->copied_total;

			if (bytes_available > stream->fragment_size)
			{
				pr_debug("bytes_available = %d\n", bytes_available);
				spin_lock_irqsave(&stream->lock, flags);
				atomic_set(&stream_info.drain,1);
				//stream_info.drain_ready = 0;
				spin_unlock_irqrestore(&stream->lock, flags);
				pr_debug("%s: wait till all the data is sent to dsp\n",
						__func__);

#if 1
				rc = wait_event_interruptible(stream_info.drain_wait,
						stream_info.drain_ready);
#else
				wait_time_ms =
					(stream->fragment_size / 1024 )*(8/stream->bit_rate);
				rc = wait_event_interruptible_timeout(stream_info.drain_wait,
						stream_info.drain_ready,
						msecs_to_jiffies(wait_time_ms));
#endif

				pr_debug("%s: wake_up drain_wait\n",__func__);
			}

			bytes_available = stream->byte_received - stream->copied_total;
			pr_debug("bytes_available = %d\n", bytes_available);
			pr_debug("%s: wait till all the data is rendered\n", __func__);

#if 1
			rc = wait_event_interruptible(stream_info.eos_wait,
					stream_info.eos_ready);
#else
			rc = wait_event_interruptible_timeout(stream_info.eos_wait,
					stream_info.eos_ready, msecs_to_jiffies(5000));
#endif
			if (rc < 0)
				pr_err("%s: EOS wait failed\n", __func__);

			pr_debug("%s: wake_up eos_wait\n",__func__);

			odin_set_stream_status(stream, ODIN_PLATFORM_STOP);
			if (odin_dsp_compr_stop_command(cstream))
				return -1;

			if (cmd == SND_COMPR_TRIGGER_DRAIN)
			{
				spin_lock_irqsave(&stream->lock, flags);
				stream->byte_offset = 0;
				stream->app_pointer = 0;
				stream->copied_total = 0;
				stream->byte_received = 0;
				spin_unlock_irqrestore(&stream->lock, flags);
			}
			else
			{
				spin_lock_irqsave(&stream->lock, flags);
				stream->app_pointer = 0;
				stream->byte_offset = 0;
				spin_unlock_irqrestore(&stream->lock, flags);
			}

			if ((stream_info.drain_ready==1)&&(stream_info.eos_ready==1))
			{
				odin_set_stream_status(stream, ODIN_PLATFORM_INIT);
			}
			else
			{
				stream_info.drain_ready = 0;
				stream_info.eos_ready = 0;
			}

			break;
		default:
			break;
	}

	return 0;
}

static int odin_dsp_compr_close(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct odin_runtime_stream *stream = runtime->private_data;
	pr_debug("%s\n", __func__);

	if (odin_get_stream_status(stream) == ODIN_PLATFORM_RUNNING ||
			odin_get_stream_status(stream) == ODIN_PLATFORM_PAUSED)
	{
		if (odin_dsp_compr_stop_command(cstream))
			return -1;
	}

	stream->buffer_paddr = 0;
	stream->buffer = NULL;

	return 0;
}

struct compress_odin_ops odin_dsp_compr_ops = {
	.open			= odin_dsp_compr_open,
	.set_normalizer = odin_dsp_compr_set_normalizer,
	.control		= odin_dsp_compr_control,
	.set_params		= odin_dsp_compr_set_params,
	.tstamp			= odin_dsp_compr_tstamp,
	.init_command	= odin_dsp_compr_init_command,
	.start_command	= odin_dsp_compr_start_command,
	.play_command	= odin_dsp_compr_play_command,
	.volume_command = odin_dsp_compr_set_volume,
	.parser_mp3_header = odin_dsp_compr_parse_mp3_header,
	.close			= odin_dsp_compr_close,
};

static int odin_dsp_of_property_get(struct platform_device *pdev,
		struct odin_dsp_device *odin_dsp_device)
{
	struct device_node *np = pdev->dev.of_node;
	struct odin_dsp_mem_ctl *mem_ctl = odin_dsp_device->aud_mem_ctl;
	struct aud_control_odin *reg_ctl = odin_dsp_device->aud_reg_ctl;
	unsigned int dsp_irq = odin_dsp_device->dsp_irq;
	struct resource *res;
	int ret;

	res = kmalloc(sizeof(struct resource), GFP_KERNEL);

	if (of_address_to_resource(np, ODIN_DSP_IOMEM_AUD_CTR, res))
	{
		dev_err(&pdev->dev, "No memory resource of ODIN_DSP_IOMEM_AUD_CTR\n");
		return -EINVAL;
	}

	reg_ctl = (struct aud_control_odin *)of_iomap(np,
			ODIN_DSP_IOMEM_AUD_CTR);

	mem_ctl->dsp_control_register_size = resource_size(res);
	mem_ctl->dsp_control_register = (unsigned int *)reg_ctl;

	if (of_address_to_resource(np, ODIN_DSP_IOMEM_AUD_DRAM0, res))
	{
		dev_err(&pdev->dev, "No memory resource of ODIN_DSP_IOMEM_AUD_DRAM0\n");
		return -EINVAL;
	}
	mem_ctl->dsp_dram0_address =
		(unsigned int *)of_iomap(np,
				ODIN_DSP_IOMEM_AUD_DRAM0);
	//mem_ctl->dsp_dram0_size = resource_size(res);
	mem_ctl->dsp_dram0_size = 0x10000;

	if (of_address_to_resource(np, ODIN_DSP_IOMEM_AUD_DRAM1, res))
	{
		dev_err(&pdev->dev, "No memory resource of ODIN_DSP_IOMEM_AUD_DRAM1\n");
		return -EINVAL;
	}
	mem_ctl->dsp_dram1_address =
		(unsigned int *)of_iomap(np,
				ODIN_DSP_IOMEM_AUD_DRAM1);
	mem_ctl->dsp_dram1_size = resource_size(res);

	if (of_address_to_resource(np, ODIN_DSP_IOMEM_AUD_IRAM0, res))
	{
		dev_err(&pdev->dev, "No memory resource of ODIN_DSP_IOMEM_AUD_IRAM0\n");
		ret=-EINVAL;
	}
	mem_ctl->dsp_iram0_address =
		(unsigned int *)of_iomap(np,
				ODIN_DSP_IOMEM_AUD_IRAM0);
	mem_ctl->dsp_iram0_size = resource_size(res);

	if (of_address_to_resource(np, ODIN_DSP_IOMEM_AUD_IRAM1, res))
	{
		dev_err(&pdev->dev, "No memory resource of ODIN_DSP_IOMEM_AUD_IRAM1\n");
		return -EINVAL;
	}
	mem_ctl->dsp_iram1_address =
		(unsigned int *)of_iomap(np,
				ODIN_DSP_IOMEM_AUD_IRAM1);
	mem_ctl->dsp_iram1_size = resource_size(res);

	if (of_address_to_resource(np,
				ODIN_DSP_IOMEM_AUD_MAILBOX, res))
	{
		dev_err(&pdev->dev,
				"No memory resource of ODIN_DSP_IOMEM_AUD_MAILBOX\n");
		return -EINVAL;
	}

	pst_audio_mailbox = (AUD_MAILBOX *)of_iomap(np,
			ODIN_DSP_IOMEM_AUD_MAILBOX);

	if (of_address_to_resource(np,ODIN_DSP_IOMEM_AUD_INTERRUPT, res))
	{
		dev_err(&pdev->dev,
				"No memory resource of ODIN_DSP_IOMEM_AUD_INTERRUPT\n");
		return -EINVAL;
	}
	pst_audio_mailbox_interrupt =
		(AUD_MAILBOX_INTERRUPT *)of_iomap(np,
				ODIN_DSP_IOMEM_AUD_INTERRUPT);

	dsp_irq = irq_of_parse_and_map(np, 0);

	odin_dsp_device->dsp_irq = dsp_irq;
	odin_dsp_device->aud_reg_ctl = (struct aud_control_odin *)reg_ctl;
	odin_dsp_device->aud_mem_ctl = mem_ctl;

	kfree(res);

	return 0;
}

void
odin_dsp_workqueue(struct work_struct *unused)
{
	MAILBOX_DATAREGISTER received_data_regsiter;
	MAILBOX_DATA_REGISTER0_CHANNEL0 data_register0;
	unsigned long flags;

	memcpy(&received_data_regsiter,
			pst_received_data_register,
			sizeof(MAILBOX_DATAREGISTER));

	pr_debug("Entered lpa workqueue\n");

	data_register0 = (MAILBOX_DATA_REGISTER0_CHANNEL0)
		(received_data_regsiter.data_register[0]);

	event_lpa->command = data_register0.field.command;
	event_lpa->type = data_register0.field.type;

	/* If RES Mailbox */
	if (event_lpa->type == RESPONSE)
	{
		if ((event_lpa->command == CONTROL_PLAY_PARTIAL)||
				(event_lpa->command==CONTROL_PLAY_PARTIAL_END))
		{

#if 0
			struct snd_compr_runtime *runtime = gstream->runtime;
			struct odin_runtime_stream *stream = runtime->private_data;
#else
			struct snd_compr_stream *cstream = stream_info.alsa_substream;
			struct snd_compr_runtime *runtime = cstream->runtime;
			struct odin_runtime_stream *stream = runtime->private_data;
#endif
			int bytes_available;
			MAILBOX_DATA_REGISTER2_CHANNEL0 data_register2;
			data_register2 = (MAILBOX_DATA_REGISTER2_CHANNEL0)
				(received_data_regsiter.data_register[2]);

			pr_debug("Recieved CONTROL_PLAY_PARTIAL RES\n");

			if (data_register2.field_play.length <= stream->fragment_size
				&& data_register2.field_play.length > 0)
			{
				spin_lock_irqsave(&stream->lock, flags);
				stream->byte_offset += data_register2.field_play.length;
				stream->copied_total += data_register2.field_play.length;
				spin_unlock_irqrestore(&stream->lock, flags);
				pr_debug("stream->copied_total = %d\n",stream->copied_total);
			}
			else {
				spin_lock_irqsave(&stream->lock, flags);
				stream->byte_offset += stream->fragment_size;
				stream->copied_total += stream->fragment_size;
				spin_unlock_irqrestore(&stream->lock, flags);
				pr_debug("stream->copied_total = %d\n",stream->copied_total);
			}

			stream_info.fragment_elapsed(stream_info.alsa_substream);

			pr_debug("data_register2.length = %d\n",
					data_register2.field_play.length);
			pr_debug("stream->byte_received = %d\n",stream->byte_received);
			pr_debug("stream->copied_total = %d\n",stream->copied_total);
			pr_debug("stream->fragment_size = %d\n", stream->fragment_size);
			bytes_available = stream->byte_received-stream->copied_total;

			pr_debug("bytes_available = %d\n", bytes_available);
			if (bytes_available < stream->fragment_size)
			{

				pr_debug("stream_info.drain = %d\n",
						atomic_read(&stream_info.drain));
				pr_debug("wake up on drain\n");
				stream_info.drain_ready=1;
				wake_up(&stream_info.drain_wait);
				atomic_set(&stream_info.drain,0);
			}

			//if (bytes_available == 0 && atomic_read(&stream_info.eos))
			if (bytes_available == 0)
			{
				pr_debug("wake up on eos\n");
				stream_info.eos_ready = 1;
				wake_up(&stream_info.eos_wait);
				atomic_set(&stream_info.eos,0);
			}

			//stream_info.fragment_elapsed(stream_info.alsa_substream);
		}
		else if (event_lpa->command == CONTROL_GET_TIMESTAMP)
		{
			MAILBOX_DATA_REGISTER1_CHANNEL0 data_register1;
			MAILBOX_DATA_REGISTER2_CHANNEL0 data_register2;
			MAILBOX_DATA_REGISTER3_CHANNEL0 data_register3;
			MAILBOX_DATA_REGISTER4_CHANNEL0 data_register4;

			data_register1 = (MAILBOX_DATA_REGISTER1_CHANNEL0)
				(received_data_regsiter.data_register[1]);
			data_register2 = (MAILBOX_DATA_REGISTER2_CHANNEL0)
				(received_data_regsiter.data_register[2]);
			data_register3 = (MAILBOX_DATA_REGISTER3_CHANNEL0)
				(received_data_regsiter.data_register[3]);
			data_register4 = (MAILBOX_DATA_REGISTER4_CHANNEL0)
				(received_data_regsiter.data_register[4]);

			event_lpa->timestamp.field.timestamp_upper =
				data_register1.field_timestamp.timestamp_upper;
			event_lpa->timestamp.field.timestamp_lower =
				data_register2.field_timestamp.timestamp_lower;
			event_lpa->decoded_frame.field.decoded_frame_upper =
				data_register3.field_decoded_frame.decoded_frame_upper;
			event_lpa->decoded_frame.field.decoded_frame_lower =
				data_register4.field_decoded_frame.decoded_frame_lower;

			pr_debug("event_lpa->timestamp = %lld\n",
					event_lpa->timestamp.value);
			wake_up_interruptible(&event_lpa->wait);
		}
		else
		{
			wake_up_interruptible(&event_lpa->wait);
		}
	}
	/* If REQ Mailbox */
	else if (event_lpa->type == REQUEST)
	{

	}

	return;
}


static int odin_dsp_probe(struct platform_device *pdev)
{
	struct odin_device *dsp_device = NULL;

	pst_dsp_mem_cfg = &dsp_mem_cfg[1];
	pst_dsp_mem_cfg->stream_buffer_vaddr =
		ioremap(pst_dsp_mem_cfg->stream_buffer_addr,
			ODIN_COMPR_PLAYBACK_MAX_FRAGMENT_SIZE);
	if (!pst_dsp_mem_cfg->stream_buffer_vaddr)
	{
		pr_err("%s:Can't allocate buffer memory in Audio DSP region\n",
				__func__);
		return -ENOMEM;
	}

	pst_dsp_mem_cfg->sys_table_vaddr =
		ioremap(pst_dsp_mem_cfg->sys_table_addr, 0x100000);

	if (!pst_dsp_mem_cfg->sys_table_vaddr)
	{
		pr_err("%s:Can't allocate system table memory in Audio DSP region\n",
				__func__);
		return -ENOMEM;
	}

	dsp_device = kzalloc(sizeof(struct odin_device), GFP_KERNEL);
	if (!dsp_device)
		return -ENOMEM;

	odin_dsp_device = kmalloc(sizeof(struct odin_dsp_device), GFP_KERNEL);
	if (!odin_dsp_device)
	{
		kfree(dsp_device);
		dsp_device = NULL;
		return -ENOMEM;
	}
	odin_dsp_device->aud_mem_ctl =
		kmalloc(sizeof(struct odin_dsp_mem_ctl), GFP_KERNEL);
	if (!odin_dsp_device->aud_mem_ctl)
	{
		kfree(dsp_device);
		dsp_device = NULL;
		kfree(odin_dsp_device);
		odin_dsp_device = NULL;

		return -ENOMEM;
	}


#ifdef CONFIG_OF
	odin_dsp_of_property_get(pdev, odin_dsp_device);
#endif

	if (request_irq(odin_dsp_device->dsp_irq,
				odin_dsp_interrupt,
				IRQF_SHARED,
				"odin-dsp",
				pst_audio_mailbox))
	{
		dev_err(&pdev->dev,"unable to allocate irq\n");
		kfree(dsp_device);
		dsp_device = NULL;
		kfree(odin_dsp_device);
		odin_dsp_device = NULL;

		return -EFAULT;
	}
	hal_mailbox_init();

	//dsp_device->name = pdev->name;
	dsp_device->dev = &pdev->dev;
	dsp_device->compr_ops = &odin_dsp_compr_ops;
	pdev->dev.platform_data = dsp_device;

	dev_info(&pdev->dev, "Probe success!");

	pm_runtime_set_autosuspend_delay(&pdev->dev, 1000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return odin_register_dsp(dsp_device);
}

static int odin_dsp_remove(struct platform_device *pdev)
{
	struct odin_device *dsp_device = pdev->dev.platform_data;

	pm_runtime_disable(&pdev->dev);

	hal_mailbox_exit();

	if (dsp_device)
	{
		kfree(dsp_device);
		dsp_device = NULL;
	}

	if (odin_dsp_device->dsp_irq)
	{
		free_irq(odin_dsp_device->dsp_irq, (void *)pst_audio_mailbox);
		odin_dsp_device->dsp_irq = 0;
	}

	if (odin_dsp_device->aud_mem_ctl)
	{
		kfree(odin_dsp_device->aud_mem_ctl);
		odin_dsp_device = NULL;
	}

	if (odin_dsp_device)
	{
		kfree(odin_dsp_device);
		odin_dsp_device = NULL;
	}
	iounmap(pst_dsp_mem_cfg->stream_buffer_vaddr);
	iounmap(pst_dsp_mem_cfg->sys_table_vaddr);

	return 0;
}


static int odin_dsp_pm_suspend(struct device *dev)
{
	struct clk *cclk_dsp;

	pr_debug("%s\n", __func__);

	if (odin_dsp_compr_close_command())
		return -1;

	if (odin_aud_powclk_clk_unprepare_disable(ODIN_AUD_CLK_SI2S2))
		pr_err("ODIN_AUD_CLK_SI2S2 disable failed\n");

	odin_dsp_reset_off(odin_dsp_device);

	kfree(event_lpa);
	event_lpa = NULL;

	if (workqueue_odin_dsp)
	{
		destroy_workqueue(workqueue_odin_dsp);
		workqueue_odin_dsp = NULL;
	}

	cclk_dsp = clk_get(NULL, "p_cclk_dsp");
	clk_disable_unprepare(cclk_dsp);

	odin_pd_off(PD_AUD, 1);
	return 0;
}

static int odin_dsp_pm_resume(struct device *dev)
{
	struct clk *cclk_dsp;
	struct clk *mclk_i2s2;
	void __iomem	*aud_crg_base;

	pr_debug("%s\n", __func__);

	memset(pst_dsp_mem_cfg->sys_table_vaddr, 0x0, 0x100000);

	aud_crg_base = ioremap(0x34670000, 0x100);
	if (!aud_crg_base) {
		pr_err("%s:audio_crg ioremap failed\n",__func__);
		return -ENOMEM;
	}
	__raw_writel(0xffffff80, aud_crg_base + 0x100);
	odin_pd_on(PD_AUD, 1);
	__raw_writel(0xffffffff, aud_crg_base + 0x100);

	__raw_writel(0x1, aud_crg_base + 0x7C);
	cclk_dsp = clk_get(NULL, "p_cclk_dsp");

	if (clk_prepare_enable(cclk_dsp))
	{
		pr_err("cclk_dsp enable failed\n");
	}
	iounmap(aud_crg_base);

	if (odin_aud_powclk_clk_prepare_enable(ODIN_AUD_CLK_SI2S2))
		pr_err("ODIN_AUD_CLK_SI2S2 enable failed\n");

	event_lpa = kmalloc(sizeof(EVENT_LPA),GFP_KERNEL);
	if (event_lpa == NULL)
	{
		pr_err("%s:Can't allocate kernel memory\n",__func__);
		return -ENOMEM;
	}
	memset(event_lpa, 0, sizeof(EVENT_LPA));
	init_waitqueue_head(&event_lpa->wait);
	event_lpa->lpa_readqueue_count = 0;

	workqueue_odin_dsp = create_workqueue("odin_dsp_work");
	if (workqueue_odin_dsp == NULL)
		return -ENOMEM;

	if (odin_dsp_download_dram(odin_dsp_device))
		return -ENOMEM;

	if (odin_dsp_download_iram(odin_dsp_device))
		return -ENOMEM;

	if (odin_dsp_reset_on(odin_dsp_device))
		return -ENOMEM;

	if (odin_dsp_compr_ready_command())
		return -EFAULT;

	if (odin_dsp_compr_create_command())
		return -EFAULT;

	if (odin_dsp_compr_open_command())
		return -EFAULT;

	return 0;
}

static const struct of_device_id odin_dsp_match[] = {
	{ .compatible = "tensilica,odin-dsp-compress", },
	{},
};

static struct dev_pm_ops odin_dsp_pm_ops = {
	SET_RUNTIME_PM_OPS(odin_dsp_pm_suspend,
		odin_dsp_pm_resume, NULL)
};

static struct platform_driver odin_dsp_driver = {
	.driver		= {
		.name		= "odin-dsp",
		.pm			= &odin_dsp_pm_ops,
		.owner		= THIS_MODULE,
		.of_match_table = of_match_ptr(odin_dsp_match),
	},
	.probe		= odin_dsp_probe,
	.remove		= odin_dsp_remove,
};

module_platform_driver(odin_dsp_driver);

MODULE_AUTHOR("Heewan Park <heewan.park@lge.com>");
MODULE_DESCRIPTION("Audio DSP driver");
MODULE_LICENSE("GPL v2");
//MODULE_ALIAS("platform:" DSP_MODULE);
