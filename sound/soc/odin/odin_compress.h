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

#ifndef __ODIN_COMPRESS_H__
#define __ODIN_COMPRESS_H__

#include <sound/compress_driver.h>
#include <sound/compress_offload.h>
#include "odin_dsp.h"

#define NEARSTOP_THRESHOLD (5*1024)

#define ODIN_COMPR_PLAYBACK_MIN_FRAGMENT_SIZE (8 * 1024)
#define ODIN_COMPR_PLAYBACK_MAX_FRAGMENT_SIZE (1024*1024)
#define ODIN_COMPR_PLAYBACK_MIN_NUM_FRAGMENTS (2)
#define ODIN_COMPR_PLAYBACK_MAX_NUM_FRAGMENTS (16*4)

#define COMPRESSED_LR_VOL_MAX_STEPS 0x2000

struct mp3_header {
	uint16_t sync;
	uint8_t format1;
	uint8_t format2;
};

struct compr_stream_info {
	int str_id;
	void *alsa_substream;
	void (*fragment_elapsed) (void *alsa_substream);
	unsigned long long buffer_ptr;
	int sfreq;
	uint32_t byte_received;
	size_t copied_total;
	size_t fragment_size;
	uint32_t drain_ready;
	uint32_t eos_ready;
	
	atomic_t eos;
	atomic_t drain;
	wait_queue_head_t eos_wait;
	wait_queue_head_t drain_wait;
};

enum odin_drv_status {
	ODIN_PLATFORM_OPENED = 1,
	ODIN_PLATFORM_READY,
	ODIN_PLATFORM_CREATE,
	ODIN_PLATFORM_OPEN,
	ODIN_PLATFORM_INIT,
	ODIN_PLATFORM_STARTED,
	ODIN_PLATFORM_RUNNING,
	ODIN_PLATFORM_PAUSED,
	ODIN_PLATFORM_STOP,
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

struct compress_odin_ops {
	const char *name;
	int (*open) (struct snd_compr_stream *cstream);
	int (*control) (int cmd, struct snd_compr_stream *cstream);
	int (*tstamp) (struct snd_compr_stream *cstream,
			struct snd_compr_tstamp *tstamp);
	int (*ack) (unsigned int str_id, unsigned long bytes);
	int (*close) (struct snd_compr_stream *cstream);
	int (*get_caps) (struct snd_compr_caps *caps);
	int (*get_codec_caps) (struct snd_compr_codec_caps *codec);
	int (*set_params) (struct snd_compr_stream *csteam,
			struct snd_compr_params *params);
	int (*start_command) (struct snd_compr_stream *cstream);
	int (*play_command) (struct snd_compr_stream *cstream,
			uint32_t dstn_paddr, size_t size);
	int (*init_command) (struct snd_compr_stream *cstream);
	int (*volume_command) (struct snd_compr_stream *cstream,
			uint32_t volume_l, uint32_t volume_r);
	int (*parser_mp3_header)(struct mp3_header *header,
			unsigned int *num_channels, unsigned int *sample_rate,
			unsigned int *bit_rate);
	int (*set_normalizer)(struct snd_compr_stream *cstream,
			unsigned int is_normalizer_enable);

};

struct odin_runtime_stream {
	int     stream_status;
	unsigned int id;
	uint32_t codec;
	size_t bytes_written;
	struct compr_stream_info stream_info;
	struct compress_odin_ops *compr_ops;
	void *buffer;
	uint32_t buffer_paddr;
	uint32_t app_pointer;
	uint32_t fragments;
	uint32_t fragment_size;
	uint32_t byte_offset;
	uint32_t copied_total;
	uint32_t byte_received;
	uint32_t start_count;
	uint32_t temp_size;

	uint32_t sample_rate;
	uint32_t num_channels;
	uint32_t bit_rate;

	spinlock_t	status_lock;
	spinlock_t	lock;

	wait_queue_head_t flush_wait;
};

struct odin_device {
	char *name;
	struct device *dev;
	struct compress_odin_ops *compr_ops;
};

struct odin_compr_data {
	struct clk *aud_clk;
	uint32_t volume[2]; /* For both L & R */
	uint16_t is_normalizer_enable;
};

int odin_register_dsp(struct odin_device *odin);
int odin_unregister_dsp(struct odin_device *odin);

void odin_set_stream_status(struct odin_runtime_stream *stream, int state);
int odin_get_stream_status(struct odin_runtime_stream *stream);

extern struct snd_compr_stream *gstream;

extern struct compr_stream_info stream_info;
#endif
