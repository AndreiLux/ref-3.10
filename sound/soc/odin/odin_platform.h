/*
 * odin_platform.h - ODIN DSP Platform driver header file
 *
 * Copyright (C) 2013 LG Electronics Co. Ltd.
 *
 * Author: Jongho Kim <m4jongho.kim@lge.com>
 *
 * Based on sst_platform.h by:
 * Copyright (C) 2010 Intel Corp
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

#ifndef __ODIN_PLATFORMDRV_H__
#define __ODIN_PLATFORMDRV_H__

#include "odin_dsp.h"

#define ODIN_MAX_BUFFER		(800*1024)
#define ODIN_MIN_BUFFER		(800*1024)
#define ODIN_MIN_PERIOD_BYTES	32
#define ODIN_MAX_PERIOD_BYTES	ODIN_MAX_BUFFER
#define ODIN_MIN_PERIODS	2
#define ODIN_MAX_PERIODS	(1024*2)
#define ODIN_FIFO_SIZE		0

#define ODIN_PLATFORM_MASTER	1
#define ODIN_PLATFORM_SLAVE	0

struct pcm_stream_info {
	int str_id;
	void *alsa_substream;
	void (*period_elapsed) (void *alsa_substream);
	unsigned long long buffer_ptr;
	int sfreq;
};

enum odin_drv_status {
	ODIN_PLATFORM_INIT = 1,
	ODIN_PLATFORM_STARTED,
	ODIN_PLATFORM_RUNNING,
	ODIN_PLATFORM_PAUSED,
	ODIN_PLATFORM_DROPPED,
};

enum odin_controls {
	ODIN_SND_ALLOC =			0x00,
	ODIN_SND_PAUSE =			0x01,
	ODIN_SND_RESUME =		0x02,
	ODIN_SND_DROP =			0x03,
	ODIN_SND_FREE =			0x04,
	ODIN_SND_BUFFER_POINTER =	0x05,
	ODIN_SND_STREAM_INIT =		0x06,
	ODIN_SND_START	 =		0x07,
	ODIN_SND_HW_SET	 =		0x08,
	ODIN_MAX_CONTROLS =		0x09,
};

enum odin_stream_ops {
	STREAM_OPS_PLAYBACK = 0,
	STREAM_OPS_CAPTURE,
};

enum odin_audio_device_type {
	SND_ODIN_DEVICE_PLAYBACK = 1,
	SND_ODIN_DEVICE_CAPTURE,
	SND_ODIN_DEVICE_COMPRESS,
};

enum odin_audio_i2s_format {
	SND_ODIN_FMT_I2S = 1,
	SND_ODIN_FMT_RIGHT_J,
	SND_ODIN_FMT_LEFT_J,
	SND_ODIN_FMT_DSP_A,
	SND_ODIN_FMT_DSP_B,
};

struct odin_pcm_dev_params {
	u32 master_mode;
	u32 i2s_format;
};

/* PCM Parameters */
struct odin_pcm_params {
	u16 codec;	/* codec type */
	u8 num_chan;	/* 1=Mono, 2=Stereo */
	u8 pcm_wd_sz;	/* 16/24 - bit*/
	u32 reserved;	/* Bitrate in bits per second */
	u32 sfreq;	/* Sampling rate in Hz */
	u32 ring_buffer_size;
	u32 ring_buffer_count;
	u32 period_size;
	u32 period_count;	/* period elapsed in samples*/
	u32 period_index;	/* period elapsed in samples*/
	u32 ring_buffer_addr;
};

struct odin_stream_params {
	u32 result;
	u32 stream_id;
	u8 codec;
	u8 ops;
	u8 stream_type;
	u8 device_type;
	struct odin_pcm_params sparams;
};

struct odin_compress_cb {
	void *param;
	void (*compr_cb)(void *param);
};

struct compress_odin_ops {
	const char *name;
	int (*open) (struct snd_odin_params *str_params,
			struct odin_compress_cb *cb);
	int (*control) (unsigned int cmd, unsigned int str_id);
	int (*tstamp) (unsigned int str_id, struct snd_compr_tstamp *tstamp);
	int (*ack) (unsigned int str_id, unsigned long bytes);
	int (*close) (unsigned int str_id);
	int (*get_caps) (struct snd_compr_caps *caps);
	int (*get_codec_caps) (struct snd_compr_codec_caps *codec);

};

struct odin_dsp_ops {
	int (*open) (struct odin_stream_params *str_param);
	int (*device_control) (int cmd, void *arg);
	int (*close) (unsigned int str_id);
};

struct odin_runtime_stream {
	int     stream_status;
	unsigned int id;
	size_t bytes_written;
	struct pcm_stream_info stream_info;
	struct odin_dsp_ops *ops;
	struct compress_odin_ops *compr_ops;
	spinlock_t	status_lock;
};

struct odin_device {
	char *name;
	struct device *dev;
	struct odin_dsp_ops *ops;
	struct compress_odin_ops *compr_ops;
};

struct odin_platform_data {
	struct clk *aud_clk;
};

int odin_register_dsp(struct odin_device *odin);
int odin_unregister_dsp(struct odin_device *odin);
#endif
