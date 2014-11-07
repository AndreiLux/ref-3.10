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

#ifndef _VEC_DRV_H_
#define _VEC_DRV_H_

#include <linux/types.h>
#include "vec_picq.h"
#include "media/odin/vcodec/vcore/encoder.h"

struct vec_pic
{
	unsigned int pic_phy_addr;
	unsigned char *pic_vir_ptr;
	unsigned int pic_size;
	void *private_in_meta;
	bool eos;
	unsigned long long timestamp;
	unsigned int uid;
};

enum vec_pic_type
{
	VEC_ENC_SEQ_HDR,
	VEC_ENC_PIC_I,
	VEC_ENC_PIC_P,
	VEC_ENC_PIC_B,
	VEC_ENC_EOS,
};

struct vec_epb
{
	char *buf_ptr;
	unsigned int buf_size;	// buffer size
	unsigned int es_size;
	enum vec_pic_type pic_type;
	unsigned long long timestamp;
	unsigned int uid;
	unsigned long input_time;	// jiffies
	unsigned long feed_time;	// jiffies
	unsigned long encode_time;	// jiffies
};

struct vec_report_enc
{
	enum
	{
		VEC_REPORT_SEQ,
		VEC_REPORT_USED_FB_EP,
		VEC_REPORT_FLUSH_DONE,
		VEC_REQUEST_RESET,
		VEC_REPORT_INVALID,
	} hdr;

	union
	{
		struct {
			unsigned int buf_width;
			unsigned int buf_height;
			unsigned int ref_frame_size;
			unsigned int ref_frame_cnt;
			unsigned int scratch_buf_size;
		} seq;

		struct {
			unsigned int pic_phy_addr;
			unsigned char *pic_vir_ptr;
			unsigned int pic_size;
			void *private_in_meta;
			bool eos;
			unsigned long long timestamp;
			unsigned int uid;
			unsigned long input_time;	// jiffies
			unsigned long feed_time;	// jiffies
			// input
			unsigned int num_of_pic;
			// output
			unsigned int last_es_size;
			unsigned int num_of_es;
			unsigned int epb_occupancy;
			unsigned int epb_size;
		} used_fb_ep;
	} u;
};

struct vec_esq_info
{
	unsigned int last_es_size;
	unsigned int num_of_es;
	unsigned int epb_occupancy;
	unsigned int epb_size;
};

void vec_init( void );
void vec_cleanup( void );

void *vec_open( struct vcore_enc_ops *vcore_ops,
				struct vcore_enc_config *config,
				unsigned int workbuf_paddr, unsigned long *workbuf_vaddr,
				unsigned int workbuf_size,
				void *top_id,
				void (*top_report_enc)( void *top_id,
									struct vec_report_enc *report_enc ) );
void vec_close( void *vec_id );

void vec_resume( void *vec_id );
void vec_pause( void *vec_id );
void vec_flush( void *vec_id );

bool vec_update_pic_buffer( void *vec_id, struct vec_pic *pic );
void vec_request_epb_cb( void *vec_id, unsigned int num_of_free_esq_node );
bool vec_register_decompressed_buffer( void *vec_id,
										struct vcore_enc_dpb *dpb );
bool vec_get_encoded_buffer( void *vec_id, struct vec_epb *epb );

unsigned int vec_get_picq_occupancy( void *vec_id );
void vec_get_esq_info( void *vec_id, struct vec_esq_info *esq_info );
void vec_set_config(void *vec_id, struct vcore_enc_running_config *config);
#endif //#ifndef _VEC_DRV_H_

