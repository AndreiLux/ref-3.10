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

#ifndef _VDC_DRV_H_
#define _VDC_DRV_H_

#include <linux/types.h>

#include "vdc_auq.h"
#include "media/odin/vcodec/vcore/decoder.h"

struct vdc_update_au
{
	unsigned char *p_buf;
	enum vdc_au_type type;
	unsigned int start_phy_addr;
	unsigned char *start_vir_ptr;
	unsigned int size;
	unsigned int end_phy_addr;
	void *private_in_meta;
	bool eos;
	unsigned long long timestamp;
	unsigned int uid;
	unsigned long input_time;	// jiffies
};

struct vdc_report_au
{
	unsigned char *p_buf;
	enum vdc_au_type type;
	unsigned int start_phy_addr;
	unsigned char *start_vir_ptr;
	unsigned int size;
	unsigned int end_phy_addr;
	void *private_in_meta;
	bool eos;
	unsigned long long timestamp;
	unsigned int uid;
	unsigned long input_time;	// jiffies
	unsigned long used_time;	// jiffies
	unsigned int num_of_au;
};

struct vdc_report_dec
{
	enum
	{
		VDC_REPORT_DEC_SEQ,
		VDC_REPORT_DEC_SEQ_CHANGE,
		VDC_REPORT_DEC_PIC,
		VDC_REPORT_DEC_EOS,
		VDC_RETURN_DEC_BUF,
		VDC_UPDATE_SEQHDR,
		VDC_REPORT_FLUSH_DONE,
		VDC_REPORT_INVALID,
	} hdr;

	union
	{
		struct
		{
			bool success;
			enum vcore_dec_pixel_format format;
			unsigned int sid;
			unsigned int profile;
			unsigned int level;
			unsigned int ref_frame_cnt;
			unsigned int buf_width;
			unsigned int buf_height;
			unsigned int pic_width;
			unsigned int pic_height;

			struct
			{
				unsigned int once_buf_size;
				unsigned int each_buf_size;
			}internal_buf_size;
			struct
			{
				unsigned int left;
				unsigned int right;
				unsigned int top;
				unsigned int bottom;
			} crop;
			unsigned int uid;
			unsigned long input_time;	// jiffies
			unsigned long feed_time;	// jiffies
			unsigned long decode_time;	// jiffies
			struct
			{
				unsigned int residual;
				unsigned int divider;
			} frame_rate;
		} seq;
		struct
		{
			unsigned int pic_width;
			unsigned int pic_height;
			unsigned int aspect_ratio;
			unsigned int err_mbs;		// percentage
			enum vocre_dec_pic_type pic_type;
			struct
			{
				unsigned int left;
				unsigned int right;
				unsigned int top;
				unsigned int bottom;
			} crop;
			struct
			{
				unsigned int residual;
				unsigned int divider;
			} frame_rate;
			unsigned long long timestamp;
			unsigned int sid;
			unsigned int uid;
			unsigned long input_time;	// jiffies
			unsigned long feed_time;	// jiffies
			unsigned long decode_time;	// jiffies

			unsigned int dpb_addr;
			void *private_out_meta;
			enum vcore_dec_rotate rotation_angle;
		} pic;
		struct
		{
			unsigned int dpb_addr;
			void *private_out_meta;
			enum vcore_dec_rotate rotation_angle;
		} buf;
		struct
		{
			unsigned char *seq_vir_ptr;
			unsigned int seq_size;
		} reset;
	} u;
};

void *vdc_open( struct vcore_dec_ops *vcore_ops,
					enum vcore_dec_codec codec_type,
					unsigned int cpb_phy_addr, unsigned char *cpb_vir_ptr,
					unsigned int cpb_size,
					bool reordering,
					bool rendering_dpb,
					bool secure_buf,
					void *top_id,
					void (*top_used_au)( void *top_id,
										struct vdc_report_au *report_au ),
					void (*top_report_dec)( void *top_id,
										struct vdc_report_dec *vdc_report ) );
void vdc_close( void *vdc_id );

void vdc_pause( void *vdc_id );
void vdc_resume( void *vdc_id );

void vdc_flush( void *vdc_id );

bool vdc_update_compressed_buffer( void *vdc_id,
									struct vdc_update_au *update_au );
bool vdc_init_decompressed_buffer( void *vdc_id,
										struct vcore_dec_init_dpb *init_dpb);
bool vdc_register_decompressed_buffer( void *vdc_id,
									struct vcore_dec_dpb *dpb);
bool vdc_clear_decompressed_buffer( void *vdc_id,
										unsigned int sid,
										unsigned int dpb_addr,
										void *private_out_meta,
										enum vcore_dec_rotate rotation_angle);

unsigned int vdc_get_auq_occupancy( void *vdc_id );
unsigned int vdc_get_dq_occupancy( void *vdc_id );

void vdc_init( void );
void vdc_cleanup( void );

#endif //#ifndef _VDC_DRV_H_

