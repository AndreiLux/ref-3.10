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

#ifndef _VDEC_IO_H_
#define _VDEC_IO_H_

#define VDEC_IOCTL_MAGIC			'D'	/* 0x44 */
#define VDEC_IO_OPEN				_IO(VDEC_IOCTL_MAGIC, 0)
#define VDEC_IO_CLOSE				_IO(VDEC_IOCTL_MAGIC, 1)
#define VDEC_IO_RESUME				_IO(VDEC_IOCTL_MAGIC, 2)
#define VDEC_IO_PAUSE				_IO(VDEC_IOCTL_MAGIC, 3)
#define VDEC_IO_FLUSH				_IO(VDEC_IOCTL_MAGIC, 4)
#define VDEC_IO_REGISTER_DPB		_IO(VDEC_IOCTL_MAGIC, 5)
#define VDEC_IO_UPDATE_CPB			_IO(VDEC_IOCTL_MAGIC, 6)
#define VDEC_IO_FREE_DPB			_IO(VDEC_IOCTL_MAGIC, 7)
#define VDEC_IO_GET_BUFFER_INFO		_IO(VDEC_IOCTL_MAGIC, 8)

enum vdec_io_codec
{
	VDEC_IO_CODEC_AVC,
	VDEC_IO_CODEC_MPEG4,
	VDEC_IO_CODEC_H263,
	VDEC_IO_CODEC_VP8,
	VDEC_IO_CODEC_MPEG2,
	VDEC_IO_CODEC_DIVX3,
	VDEC_IO_CODEC_DIVX,
	VDEC_IO_CODEC_SORENSON,
	VDEC_IO_CODEC_VC1,
	VDEC_IO_CODEC_AVS,
	VDEC_IO_CODEC_MVC,
	VDEC_IO_CODEC_THO,
	VDEC_IO_CODEC_JPEG,
	VDEC_IO_CODEC_PNG,
	VDEC_IO_CODEC_UNKNOWN,
};

enum vdec_io_rotate
{
	VCORE_IO_ROTATE_0,
	VCORE_IO_ROTATE_90,
	VCORE_IO_ROTATE_180,
	VCORE_IO_ROTATE_270
};

struct vdec_io_open_arg
{
	enum vdec_io_codec codec;
	void *vevent_id;
	int reordering;
	int rendering_dpb;
	int secure_buf;
};

struct vdec_io_get_buffer_status
{
	unsigned int cpb_size;
	unsigned int cpb_occupancy;
	unsigned int num_of_au;
};

struct vdec_io_update_dpb
{
	int dpb_fd;
	void *private_out_meta;
	unsigned int sid;
	enum vdec_io_rotate rotation_angle;
};

struct vdec_io_register_dpb
{
	int width;
	int height;
	int dpb_fd;
	unsigned int sid;
};

enum vdec_io_au_type
{
	VDEC_IO_AU_SEQ_HDR,
	VDEC_IO_AU_SEQ_END,
	VDEC_IO_AU_PIC,
	VDEC_IO_AU_PIC_I,
	VDEC_IO_AU_PIC_P,
	VDEC_IO_AU_PIC_B,
	VDEC_IO_AU_UNKNOWN,
};

enum vdec_io_pixel_format
{
	VDEC_IO_FORMAT_420,
	VDEC_IO_FORMAT_422,
	VDEC_IO_FORMAT_224,
	VDEC_IO_FORMAT_444,
	VDEC_IO_FORMAT_400,
};

struct vdec_io_update_cpb
{
	union {
		unsigned char *user_addr;
		int ion_fd;	// secured mode
	} es;
	unsigned int es_size;
	void *private_in_meta;
	enum vdec_io_au_type type;
	uint8_t eos;
	unsigned long long timestamp;
	unsigned int uid;
	uint8_t ring_buffer;
	unsigned int align_bytes;
};

enum vdec_io_cb_type
{
	VDEC_IO_CB_TYPE_FEEDED_AU,
	VDEC_IO_CB_TYPE_DECODED_DPB,
	VDEC_IO_CB_TYPE_FLUSH_DONE,
};

struct vdec_io_cb_feeded_au
{
	unsigned char *p_buf;
	unsigned int start_phy_addr;
	unsigned int size;
	unsigned int end_phy_addr;
	void *private_in_meta;
	enum vdec_io_au_type type;
	uint8_t eos;
	unsigned long long timestamp;
	unsigned int uid;
	unsigned long input_time;	// jiffies
	unsigned long feed_time;	// jiffies
	unsigned int num_of_au;
};

struct vdec_io_cb_decoded_dpb
{
	enum
	{
		VDEC_IO_DEDCODED_SEQ,
		VDEC_IO_DEDCODED_PIC,
		VDEC_IO_DEDCODED_EOS,
		VDEC_IO_DEDCODED_NONE,
	} hdr;

	union
	{
		struct
		{
			uint8_t success;
			enum vdec_io_pixel_format format;
			unsigned int profile;
			unsigned int level;
			unsigned int ref_frame_cnt;
			unsigned int buf_width;
			unsigned int buf_height;
			unsigned int width;
			unsigned int height;
		} sequence;

		struct
		{
			unsigned int width;
			unsigned int height;
			unsigned int aspect_ratio;
			unsigned int err_mbs;
			unsigned long long timestamp;
		} picture;
	} u;

	struct
	{
		unsigned int residual;
		unsigned int divider;
	} frame_rate;

	struct
	{
		unsigned int left;
		unsigned int right;
		unsigned int top;
		unsigned int bottom;
	} crop;

	unsigned int sid;
	unsigned int uid;
	unsigned long input_time;
	unsigned long feed_time;
	unsigned long decode_time;

	struct vdec_io_update_dpb dpb;
};

struct vdec_io_cb_data
{
	enum vdec_io_cb_type type;
	union
	{
		struct vdec_io_cb_feeded_au feeded_au;
		struct vdec_io_cb_decoded_dpb decoded_dpb;
	};
};

#endif /* #ifndef _VDEC_IO_H_ */
