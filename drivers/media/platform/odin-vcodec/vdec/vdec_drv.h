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

#ifndef _VDEC_DRV_H_
#define _VDEC_DRV_H_

#include <media/odin/vcodec/vcore/decoder.h>

enum vdec_au_type
{
	VDEC_AU_SEQ_HDR,
	VDEC_AU_SEQ_END,
	VDEC_AU_PIC,
	VDEC_AU_PIC_I,
	VDEC_AU_PIC_P,
	VDEC_AU_PIC_B,
	VDEC_AU_UNKNOWN,
};

struct vdec_update_dpb
{
	unsigned int sid;
	int dpb_fd;
	void *private_out_meta;
	enum vcore_dec_rotate rotation_angle;
};

enum vdec_cb_type
{
	VDEC_CB_TYPE_FEEDED_AU,
	VDEC_CB_TYPE_DECODED_DPB,
	VDEC_CB_TYPE_FLUSH_DONE,
	VDEC_CB_TYPE_INVALID,
};

struct vdec_cb_written_au
{
	unsigned char *p_buf;
	unsigned int start_phy_addr;
	unsigned int size;
	unsigned int end_phy_addr;
	unsigned long long timestamp;
	unsigned int uid;
	unsigned long input_time;
	unsigned int cpb_occupancy;
	unsigned int cpb_size;
};

struct vdec_cb_feeded_au
{
	unsigned char *p_buf;
	enum vdec_au_type type;
	unsigned int start_phy_addr;
	unsigned int size;
	unsigned int end_phy_addr;
	void *private_in_meta;
	bool eos;
	unsigned long long timestamp;
	unsigned int uid;
	unsigned long input_time;	// jiffies
	unsigned long feed_time;	// jiffies
	unsigned int num_of_au;
};

struct vdec_cb_decoded_dpb
{
	enum
	{
		VDEC_DEDCODED_SEQ,
		VDEC_DEDCODED_PIC,
		VDEC_DEDCODED_EOS,
		VDEC_DEDCODED_NONE,
	} hdr;

	union
	{
		struct
		{
			uint8_t success;
			enum vcore_dec_pixel_format format;
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

	unsigned int uid;
	unsigned int sid;
	unsigned long input_time;
	unsigned long feed_time;
	unsigned long decode_time;

	struct vdec_update_dpb dpb;
};

struct vdec_cb_data
{
	enum vdec_cb_type type;
	union
	{
		struct vdec_cb_feeded_au feeded_au;
		struct vdec_cb_decoded_dpb decoded_dpb;
	};
};

void* vdec_open(enum vcore_dec_codec codec,
					bool reordering,
					bool rendering_dpb,
					bool secure_buf,
					void *cb_arg,
					void (*callback)(void *cb_arg,
									struct vdec_cb_data *cb_data));
void vdec_close(void *id);

void vdec_resume(void *id);
void vdec_pause(void *id);
void vdec_flush(void *id);

int vdec_update_cpb(void *id,
					unsigned char *es_addr,
					int ion_fd,
					unsigned int es_size,
					void *private_in_meta,
					enum vdec_au_type au_type,
					bool eos,
					unsigned long long timestamp,
					unsigned int uid,
					bool ring_buffer,
					unsigned int align_bytes);
int vdec_free_dpb(void *id,
					struct vdec_update_dpb *dpb);
int vdec_register_dpb(void *id,
					int width,
					int height,
					int dpb_fd,
					int sid);
int vdec_get_buffer_info(void* id,
						unsigned int *cpb_size,
						unsigned int *cpb_occupancy,
						unsigned int *num_of_au);

int vdec_init(void* (*get_vcore)(enum vcore_dec_codec codec,
								struct vcore_dec_ops *ops,
								bool rendering_dpb),
			void (*put_vcore)(void *select_id));
int vdec_cleanup(void);

#endif /* #ifndef _VDEC_DRV_H_ */


