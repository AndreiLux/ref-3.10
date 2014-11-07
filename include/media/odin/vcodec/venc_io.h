/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _VENC_IO_H_
#define _VENC_IO_H_

#define VENC_IOCTL_MAGIC			'E'	/* 0x45 */
#define VENC_IO_OPEN				_IO(VENC_IOCTL_MAGIC, 0)
#define VENC_IO_CLOSE				_IO(VENC_IOCTL_MAGIC, 1)
#define VENC_IO_RESUME				_IO(VENC_IOCTL_MAGIC, 2)
#define VENC_IO_PAUSE				_IO(VENC_IOCTL_MAGIC, 3)
#define VENC_IO_FLUSH				_IO(VENC_IOCTL_MAGIC, 4)
#define VENC_IO_UPDATE_FB			_IO(VENC_IOCTL_MAGIC, 5)
#define VENC_IO_UPDATE_EPB			_IO(VENC_IOCTL_MAGIC, 6)
#define VENC_IO_GET_EPB				_IO(VENC_IOCTL_MAGIC, 7)
#define VENC_IO_GET_BUFFER_INFO		_IO(VENC_IOCTL_MAGIC, 8)
#define VENC_IO_SET_CONFIG			_IO(VENC_IOCTL_MAGIC, 9)

enum venc_io_codec
{
	VENC_IO_CODEC_AVC,
	VENC_IO_CODEC_MPEG4,
	VENC_IO_CODEC_H263,
	VENC_IO_CODEC_JPEG,
};

enum venc_io_pixel_format
{
	VENC_IO_FORMAT_420,
	VENC_IO_FORMAT_422,
	VENC_IO_FORMAT_224,
	VENC_IO_FORMAT_444,
	VENC_IO_FORMAT_400,
};

struct venc_io_running_arg
{
	enum {
		VENC_IO_RUNNING_PARAM_BITRATE = 0,
		VENC_IO_RUNNING_PARAM_INTRA_REFRESH,
	} index;

	union {
		unsigned int bit_rate_kbps;
		unsigned int intra_refresh;
	} data;
};

struct venc_io_open_arg
{
	enum venc_io_codec codec_type;

	unsigned int pic_width;
	unsigned int pic_height;
	unsigned int buf_width;
	unsigned int buf_height;

	union {

		struct {
			unsigned int gop_size;
			unsigned int mb_num_per_slice;
			unsigned int cabac_init_idx;
			enum {
				VENC_IO_LOOP_FILTER_ENABLE = 0,
				VENC_IO_LOOP_FILTER_DISABLE,
				VENC_IO_LOOP_FILTER_DISABLE_SLICE_BOUNDARY,
			} loop_filter_mode;
			uint8_t const_intra_pred_enable;

			enum {
				VENC_IO_H264_PROFILE_BASE = 0,
				VENC_IO_H264_PROFILE_MAIN,
				VENC_IO_H264_PROFILE_HIGH,
			} profile;
			unsigned int level_idc;

			uint8_t field_enable;
			uint8_t cabac_enable;
		}h264;

		struct {
			unsigned int gop_size;
			enum {
				VENC_IO_H263_PROFILE_BASE = 0,
				VENC_IO_H263_PROFILE_SWV2,
			} profile;
		} h263;

		struct {
			unsigned int gop_size;
			uint8_t short_video_header_enable;
			unsigned int intra_dc_vlc_threshold;
		} mpeg4;
	}codec_param;

	struct {
		enum {
			VENC_IO_RATE_CONTROL_DISABLE = 0,
			VENC_IO_RATE_CONTROL_CBR,
			VENC_IO_RATE_CONTROL_ABR,
		} control_mode;
		uint8_t auto_skip_enable;
		unsigned int target_kbps;
	} bit_rate;

	struct {
		uint8_t hec_enable;
		uint8_t data_partitioning_enable;
		uint8_t reversible_vlc_enble;
	} error_correction;

	struct {
		uint8_t enable;
		unsigned int mb_num;
	} cyclic_intra_block_refresh;

	struct {
		unsigned int residual;
		unsigned int divider;
	} frame_rate;

	enum {
		VENC_IO_CBCR_INTERLEAVE_NONE = 0,
		VENC_IO_CBCR_INTERLEAVE_NV12,
		VENC_IO_CBCR_INTERLEAVE_NV21,
	} cbcr_interleave;

	enum venc_io_pixel_format format;
	void *vevent_id;
};

struct venc_io_update_fb
{
	int ion_fd;
	unsigned int size;
	void *private_in_meta;
	uint8_t eos;
	unsigned long long timestamp;
	unsigned int uid;
};

enum venc_io_es_type
{
	VENC_IO_ES_SEQ_HDR,
	VENC_IO_ES_PIC_I,
	VENC_IO_ES_PIC_P,
	VENC_IO_ES_PIC_B,
	VENC_IO_ES_EOS,
};

struct venc_io_free_esq
{
	unsigned int num_of_free_esq_node;
};

struct venc_io_epb
{
	// in/out
	char *buf_ptr;
	// in
	unsigned int buf_size;	// buffer size
	// out
	unsigned int es_size;
	enum venc_io_es_type type;
	unsigned long long timestamp;
	unsigned int uid;
	unsigned long input_time;	// jiffies
	unsigned long feed_time;	// jiffies
	unsigned long encode_time;	// jiffies
};

enum venc_io_cb_type
{
	VENC_IO_CB_TYPE_USED_FB_EP,
	VENC_IO_CB_TYPE_FLUSH_DONE,
};

struct venc_io_cb_encoded
{
	enum venc_io_cb_type type;

	struct {
		int ion_fd;
		unsigned int size;
		void *private_in_meta;
		uint8_t eos;
		unsigned long long timestamp;
		unsigned int uid;
		unsigned long input_time;	// jiffies
		unsigned long feed_time;	// jiffies
		unsigned int num_of_pic;
	} fb;

	struct {
		unsigned int last_es_size;
		unsigned int num_of_es;
		unsigned int epb_occupancy;
		unsigned int epb_size;
	} es;
};

struct venc_io_get_buffer_status
{
	// input
	unsigned int num_of_pic;
	// output
	unsigned int last_es_size;
	unsigned int num_of_es;
	unsigned int epb_occupancy;
	unsigned int epb_size;
};
#endif /* #ifndef _VENC_IO_H_ */
