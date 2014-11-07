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

#include "vec_drv.h"
#include "vec_picq.h"
#include "vec_epb.h"
#include "vec_esq.h"
#include "../../common/vcodec_timer.h"
#include "../../common/vcodec_rate.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <media/odin/vcodec/vlog.h>

#define	VEC_EPB_ALMOST_FULL_LEVEL		60 // percentage
#define VEC_MAX_SCHEDULE_RETRY			0x10000

#define ___vec_debug_q_status(log_level, vec_ch)				\
	do {											\
		vec_ch->status.num_of_picq_node = vec_picq_waiting_occupancy( vec_ch->status.vec_picq_id );		\
																										\
		vlog_print( log_level, "state:%d-%d-%d-%d-%d, picq:%u, free esq:0x%X, receive/send/report:%u/%u/%u\n", 	\
			vec_ch->reset_state, vec_ch->close_state, vec_ch->flush_state, vec_ch->pause_state, vec_ch->vcore_state, 							\
			vec_ch->status.num_of_picq_node, vec_ch->status.num_of_free_esq_node,													\
			vec_ch->debug.received_cnt, vec_ch->debug.sent_cnt, vec_ch->debug.reported_cnt );								\
	} while(0)

struct vec_channel
{
	struct list_head list;

	volatile enum
	{
		VEC_RESETTING,
		VEC_NORMAL,
	} reset_state;

	volatile enum
	{
		VEC_OPEN,
		VEC_CLOSING,
		VEC_CLOSED,
	} close_state;

	volatile enum
	{
		VEC_FLUSHED,
		VEC_FLUSHING,
	} flush_state;

	volatile enum
	{
		VEC_PAUSED,
		VEC_PLAYING,
	} pause_state;

	volatile enum
	{
		VEC_IDLE,
		VEC_RUNNING,
	} vcore_state;

	struct completion close_completion;

	struct
	{
		enum vcore_enc_codec codec_type;
		unsigned int epb_phy_addr;
		unsigned char *epb_vir_ptr;
		unsigned int epb_size;
		void *top_id;
		void (*top_report_enc)( void *top_id, struct vec_report_enc *report_enc );
	} config;

	struct vcore_enc_ops vcore_ops;

	struct workqueue_struct *vcore_report_wq;
	struct mutex lock;

	struct
	{
		void *vec_picq_id;
		void *vec_epb_id;
		void *vec_esq_id;
		void *vec_rate_id;
		void *vec_timer_id;
		void *vcore_id;

		unsigned int num_of_picq_node;
		unsigned int num_of_free_esq_node;
		unsigned int available_epb;

		struct vec_esq_node esq_node_seq;	// for jpeg seq hdr
	} status;

	struct
	{
		unsigned int received_cnt;
		unsigned int sent_cnt;
		unsigned int reported_cnt;

		unsigned long input_rate;		// * 100
		unsigned long decode_rate;	// * 100
		unsigned long timestamp_rate;	// * 100
	} debug;
};

struct vec_vcore_report_work
{
	struct work_struct vcore_report_work;
	struct vcore_enc_report vcore_report;
	struct vec_channel *vec_ch;
};

struct list_head vec_ch_list;

static unsigned int __vec_flush_picq( struct vec_channel *vec_ch )
{
	unsigned int flush_pic_cnt = 0;

	if (vec_ch->config.top_report_enc) {
		struct vec_report_enc report_enc;
		struct vec_picq_node *picq_node = NULL;

		vec_esq_node_info( vec_ch->status.vec_esq_id, &report_enc.u.used_fb_ep.num_of_es, &report_enc.u.used_fb_ep.last_es_size );
		vec_epb_get_buffer_status( vec_ch->status.vec_epb_id, &report_enc.u.used_fb_ep.epb_occupancy, &report_enc.u.used_fb_ep.epb_size );
		vec_ch->status.available_epb = report_enc.u.used_fb_ep.epb_size - report_enc.u.used_fb_ep.epb_occupancy;

		while ((picq_node = vec_picq_pop_encoding( vec_ch->status.vec_picq_id )) != NULL) {
			vlog_error( "exist the node in encoding pic q - addr:0x%08X\n", picq_node->pic_phy_addr );

			flush_pic_cnt++;

			report_enc.hdr = VEC_REPORT_USED_FB_EP;
			report_enc.u.used_fb_ep.pic_phy_addr = picq_node->pic_phy_addr;
			report_enc.u.used_fb_ep.pic_vir_ptr = picq_node->pic_vir_ptr;
			report_enc.u.used_fb_ep.pic_size = picq_node->pic_size;
			report_enc.u.used_fb_ep.private_in_meta = picq_node->private_in_meta;
			report_enc.u.used_fb_ep.eos = picq_node->eos;
			report_enc.u.used_fb_ep.timestamp = picq_node->timestamp;
			report_enc.u.used_fb_ep.uid = picq_node->uid;
			report_enc.u.used_fb_ep.input_time =  picq_node->input_time;
			report_enc.u.used_fb_ep.feed_time =  picq_node->feed_time;
			report_enc.u.used_fb_ep.num_of_pic =  vec_picq_waiting_occupancy( vec_ch->status.vec_picq_id );

			vec_ch->config.top_report_enc( vec_ch->config.top_id, &report_enc );

			if (vec_picq_push_encoded( vec_ch->status.vec_picq_id, picq_node ) == false) {
				vec_picq_debug_print_pic_phy_addr( vec_ch->status.vec_picq_id );
			}
		}
		while ((picq_node = vec_picq_pop_waiting( vec_ch->status.vec_picq_id )) != NULL) {
			vlog_trace( "return the waiting pic addr:0x%08X\n", picq_node->pic_phy_addr );

			flush_pic_cnt++;

			report_enc.hdr = VEC_REPORT_USED_FB_EP;
			report_enc.u.used_fb_ep.pic_phy_addr = picq_node->pic_phy_addr;
			report_enc.u.used_fb_ep.pic_vir_ptr = picq_node->pic_vir_ptr;
			report_enc.u.used_fb_ep.pic_size = picq_node->pic_size;
			report_enc.u.used_fb_ep.private_in_meta = picq_node->private_in_meta;
			report_enc.u.used_fb_ep.eos = picq_node->eos;
			report_enc.u.used_fb_ep.timestamp = picq_node->timestamp;
			report_enc.u.used_fb_ep.uid = picq_node->uid;
			report_enc.u.used_fb_ep.input_time =  picq_node->input_time;
			report_enc.u.used_fb_ep.feed_time =  jiffies;
			report_enc.u.used_fb_ep.num_of_pic =  vec_picq_waiting_occupancy( vec_ch->status.vec_picq_id );

			vec_ch->config.top_report_enc( vec_ch->config.top_id, &report_enc );

			if (vec_picq_push_encoded( vec_ch->status.vec_picq_id, picq_node ) == false) {
				vec_picq_debug_print_pic_phy_addr( vec_ch->status.vec_picq_id );
			}
		}
	}

	return flush_pic_cnt;
}

static void _vec_flush_do_flush_state( struct vec_channel *vec_ch )
{
	struct vec_report_enc report_enc;

	___vec_debug_q_status( VLOG_TRACE, vec_ch );

	__vec_flush_picq(vec_ch);

	vec_esq_node_info( vec_ch->status.vec_esq_id, &report_enc.u.used_fb_ep.num_of_es, &report_enc.u.used_fb_ep.last_es_size );
	vec_epb_get_buffer_status( vec_ch->status.vec_epb_id, &report_enc.u.used_fb_ep.epb_occupancy, &report_enc.u.used_fb_ep.epb_size );
	vec_ch->status.available_epb = report_enc.u.used_fb_ep.epb_size - report_enc.u.used_fb_ep.epb_occupancy;

	report_enc.hdr = VEC_REPORT_FLUSH_DONE;
	report_enc.u.used_fb_ep.pic_phy_addr = 0x0;
	report_enc.u.used_fb_ep.pic_vir_ptr = NULL;
	report_enc.u.used_fb_ep.pic_size = 0;
	report_enc.u.used_fb_ep.private_in_meta = NULL;
	report_enc.u.used_fb_ep.eos = false;
	report_enc.u.used_fb_ep.timestamp = 0xFFFFFFFFFFFFFFFFLL;
	report_enc.u.used_fb_ep.uid = 0xFFFFFFFF;
	report_enc.u.used_fb_ep.input_time =  jiffies;
	report_enc.u.used_fb_ep.feed_time =  report_enc.u.used_fb_ep.input_time;
	report_enc.u.used_fb_ep.num_of_pic =  vec_picq_waiting_occupancy( vec_ch->status.vec_picq_id );

	vec_ch->config.top_report_enc( vec_ch->config.top_id, &report_enc );
}

static bool __vec_feed_try_to_send_pic( void *vec_id )
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;
	struct vec_picq_node *picq_node;
	struct vcore_enc_fb vcore_fb;
	bool ret;

	picq_node = vec_picq_peep_waiting( vec_ch->status.vec_picq_id );
	if (picq_node == NULL) {
		vlog_error( "failed to pop waiting pic q\n" );
		return false;
	}

	if (picq_node->pic_size) {
		vcore_fb.fb_phy_addr = picq_node->pic_phy_addr;
		vcore_fb.fb_size = picq_node->pic_size;

		ret = ((vec_ch->vcore_ops.update_buffer( vec_ch->status.vcore_id, &vcore_fb)) == VCORE_ENC_SUCCESS ) ? true : false;
		if (ret == false) {
			vlog_print( VLOG_VENC_VEC, "failed to update pic to vcore - instance:0x%X, pic addr:0x%X++0x%X\n",
			(unsigned int)vec_ch, vcore_fb.fb_phy_addr, vcore_fb.fb_phy_addr );
		}
		else {
			picq_node = vec_picq_pop_waiting( vec_ch->status.vec_picq_id );
			if (picq_node == NULL) {
				vlog_error( "failed to pop waiting pic q\n" );
				return false;
			}

			picq_node->feed_time = jiffies;
			if (vec_picq_push_encoding( vec_ch->status.vec_picq_id, picq_node ) == false) {
				vec_picq_debug_print_pic_phy_addr( vec_ch->status.vec_picq_id );
			}
			vec_ch->debug.sent_cnt++;
		}
	}
	else if(picq_node->eos == true) {
		struct vec_report_enc report_enc;
		struct vec_esq_node esq_node;

		vlog_trace( "end of stream\n" );

		esq_node.es_start_addr = 0x0;
		esq_node.es_end_addr = 0x0;
		esq_node.es_size = 0;
		esq_node.pic_type = VEC_ESQ_EOS;
		esq_node.encode_time = jiffies;
		esq_node.timestamp = picq_node->timestamp;
		esq_node.uid = picq_node->uid;
		esq_node.input_time = picq_node->input_time;
		esq_node.feed_time = picq_node->feed_time;

		vec_esq_push( vec_ch->status.vec_esq_id, &esq_node );

		report_enc.hdr = VEC_REPORT_USED_FB_EP;

		report_enc.u.used_fb_ep.pic_phy_addr = picq_node->pic_phy_addr;
		report_enc.u.used_fb_ep.pic_vir_ptr = picq_node->pic_vir_ptr;
		report_enc.u.used_fb_ep.pic_size = picq_node->pic_size;
		report_enc.u.used_fb_ep.private_in_meta = picq_node->private_in_meta;
		report_enc.u.used_fb_ep.eos = picq_node->eos;
		report_enc.u.used_fb_ep.timestamp = picq_node->timestamp;
		report_enc.u.used_fb_ep.uid = picq_node->uid;
		report_enc.u.used_fb_ep.input_time =  picq_node->input_time;
		report_enc.u.used_fb_ep.feed_time =  jiffies;
		report_enc.u.used_fb_ep.num_of_pic =  vec_picq_waiting_occupancy( vec_ch->status.vec_picq_id );

		vec_esq_node_info( vec_ch->status.vec_esq_id, &report_enc.u.used_fb_ep.num_of_es, &report_enc.u.used_fb_ep.last_es_size );
		vec_epb_get_buffer_status( vec_ch->status.vec_epb_id, &report_enc.u.used_fb_ep.epb_occupancy, &report_enc.u.used_fb_ep.epb_size );
		vec_ch->status.available_epb = report_enc.u.used_fb_ep.epb_size - report_enc.u.used_fb_ep.epb_occupancy;

		vec_ch->config.top_report_enc( vec_ch->config.top_id, &report_enc );

		ret = false;

		picq_node = vec_picq_pop_waiting( vec_ch->status.vec_picq_id );
		if (picq_node == NULL) {
			vlog_error( "failed to pop waiting pic q\n" );
			return false;
		}

		if (vec_picq_push_encoded( vec_ch->status.vec_picq_id, picq_node ) == false) {
			vec_picq_debug_print_pic_phy_addr( vec_ch->status.vec_picq_id );
		}
		vec_ch->debug.sent_cnt++;
	}
	else {
		vlog_error( "invalid data into node of pic q\n" );
		return false;
	}

	return ret;
}

static bool __vec_feed_evaluate_inout_q( struct vec_channel *vec_ch )
{
//	unsigned int num_of_picq_node_prev;
	unsigned int num_of_free_esq_node_prev;
	unsigned int epb_occupancy, epb_size;
	bool ret = false;

	//	num_of_picq_node_prev = vec_ch->status.num_of_picq_node;
	num_of_free_esq_node_prev = vec_ch->status.num_of_free_esq_node;

	vec_ch->status.num_of_picq_node = vec_picq_waiting_occupancy( vec_ch->status.vec_picq_id );
	vec_epb_get_buffer_status( vec_ch->status.vec_epb_id, &epb_occupancy, &epb_size );
	vec_ch->status.available_epb = epb_size - epb_occupancy;

	if (vec_ch->status.num_of_picq_node && vec_ch->status.num_of_free_esq_node) {
		ret = __vec_feed_try_to_send_pic( vec_ch );
	}
	else {
//		if ((num_of_picq_node_prev) && (!vec_ch->status.num_of_picq_node))
//			vlog_warning( "picq empty, epb:0x%X/0x%X\n", epb_occupancy, epb_size );
		if ((num_of_free_esq_node_prev) && (!vec_ch->status.num_of_free_esq_node))
			vlog_warning( "epb almost full:0x%X/0x%X, picq:%u\n", epb_occupancy, epb_size, vec_ch->status.num_of_picq_node );
	}

	return ret;
}

static void __vec_feed_check_state(struct vec_channel *vec_ch )
{
	// check 1st state
	switch( vec_ch->reset_state ) {
	case VEC_NORMAL:

		// check 2nd state
		switch( vec_ch->close_state ) {
		case VEC_OPEN:

			// check 3rd state
			switch( vec_ch->flush_state )
			{
			case VEC_FLUSHED:

				// check 4th state
				switch( vec_ch->pause_state )
				{
				case VEC_PAUSED:
					vlog_trace("paused\n");
					break;
				case VEC_PLAYING:

					// check 5th state
					switch (vec_ch->vcore_state) {
					case VEC_IDLE:
						if (__vec_feed_evaluate_inout_q( vec_ch ) == true) {
							vcodec_update_timer_status(vec_ch->status.vec_timer_id);
							vec_ch->vcore_state = VEC_RUNNING;
						}
						break;
					case VEC_RUNNING:
//						vlog_warning("play - vcore encoding\n");
						break;
					default:
						vlog_error("vcore_state:%d\n", vec_ch->vcore_state);
						break;
					}
					break;

				default:
					vlog_error("pause_state:%d\n", vec_ch->pause_state);
					break;
				}
				break;

			case VEC_FLUSHING:
				switch (vec_ch->vcore_state) {
				case VEC_IDLE:
					_vec_flush_do_flush_state(vec_ch);
					vec_ch->flush_state = VEC_FLUSHED;
					break;
				case VEC_RUNNING:
					vlog_trace("flushing\n");
					break;
				default:
					vlog_error("vcore_state:%d\n", vec_ch->vcore_state);
					break;
				}
				break;

			default :
				vlog_error("flush_state:%d\n", vec_ch->flush_state);
				break;
			}

			break;
		case VEC_CLOSING:

			switch (vec_ch->vcore_state) {
			case VEC_IDLE:
				vlog_error("idle - closing\n");
				break;
			case VEC_RUNNING:
				vlog_warning("close - vcore encoding\n");
				break;
			default:
				vlog_error("vcore_state:%d\n", vec_ch->vcore_state);
				break;
			}

			break;
		case VEC_CLOSED:
			vlog_error("already closed state\n");
			break;
		default:
			vlog_error("close_state:%d\n", vec_ch->close_state);
			break;
		}

		break;
	case VEC_RESETTING:
		vlog_warning("vcore resetting\n");
		break;
	default:
		vlog_error("reset_state:%d\n", vec_ch->reset_state);
		break;
	}
}

static void __vec_parse_encoded_info( struct vec_channel *vec_ch,
										struct vcore_enc_report *vcore_report,
										struct vec_report_enc *report_enc )
{
	unsigned long current_time = jiffies;
	struct vec_picq_node *picq_node = NULL;

	vec_ch->debug.decode_rate
		= vcodec_rate_update_codec_done( vec_ch->status.vec_rate_id, \
											current_time );

	switch (vcore_report->info.done.info.pic.pic_type)
	{
	case VCORE_ENC_SEQ_HDR :
		vlog_trace( "seq hdr\n" );
		if (vec_ch->config.codec_type != VCORE_ENC_JPEG) {
			report_enc->hdr = VEC_REPORT_USED_FB_EP;
		}
		break;
	case VCORE_ENC_PIC_I :
	case VCORE_ENC_PIC_P :
	case VCORE_ENC_PIC_B :
		picq_node = vec_picq_pop_encoding( vec_ch->status.vec_picq_id );
		if (picq_node == NULL) {
			vlog_error( "failed to pop encoding pic q\n" );
			vec_picq_debug_print_pic_phy_addr( vec_ch->status.vec_picq_id );
		}
		report_enc->hdr = VEC_REPORT_USED_FB_EP;
		break;
	default :
		vlog_error( "pic type:%d\n", vcore_report->info.done.info.pic.pic_type );
		break;
	}

	// update epb
	vec_epb_update_wraddr( vec_ch->status.vec_epb_id, \
							vcore_report->info.done.info.pic.end_phy_addr );

	// update esq
	if (vcore_report->info.done.info.pic.success == VCORE_TRUE) {
		struct vec_esq_node esq_node;

		esq_node.es_start_addr = vcore_report->info.done.info.pic.start_phy_addr;
		esq_node.es_end_addr = vcore_report->info.done.info.pic.end_phy_addr;
		esq_node.es_size = vcore_report->info.done.info.pic.size;
		switch (vcore_report->info.done.info.pic.pic_type)
		{
		case VCORE_ENC_SEQ_HDR :
			esq_node.pic_type = VEC_ESQ_SEQ_HDR;
			break;
		case VCORE_ENC_PIC_I :
		case VCORE_ENC_PIC_P :
		case VCORE_ENC_PIC_B :
			esq_node.pic_type = VEC_ESQ_PIC_I + \
				(vcore_report->info.done.info.pic.pic_type - VCORE_ENC_PIC_I);
			break;
		default :
			vlog_error( "pic type:%d\n", vcore_report->info.done.info.pic.pic_type );
			break;
		}

		esq_node.encode_time = current_time;
		if (picq_node) {
			if (picq_node->pic_size) {
				esq_node.timestamp = picq_node->timestamp;
				esq_node.uid = picq_node->uid;
				esq_node.input_time = picq_node->input_time;
				esq_node.feed_time = picq_node->feed_time;

				vec_esq_push( vec_ch->status.vec_esq_id, &esq_node );
			}

			if (picq_node->eos == true) {
				esq_node.es_start_addr = vcore_report->info.done.info.pic.end_phy_addr;
				esq_node.es_end_addr = vcore_report->info.done.info.pic.end_phy_addr;
				esq_node.es_size = 0;
				esq_node.pic_type = VEC_ESQ_EOS;
				esq_node.timestamp = 0;
				esq_node.uid = 0;
				esq_node.input_time = 0;
				esq_node.feed_time = 0;

				vec_esq_push( vec_ch->status.vec_esq_id, &esq_node );
			}
		}
		else {
			esq_node.timestamp = 0;
			esq_node.uid = 0;
			esq_node.input_time = 0;
			esq_node.feed_time = 0;

			vec_esq_push( vec_ch->status.vec_esq_id, &esq_node );
		}

		if (vec_ch->status.num_of_free_esq_node)
			vec_ch->status.num_of_free_esq_node--;
		else
			vlog_error( "no free esq\n" );
	}
	else {
		vlog_error( "failed to encode\n" );
	}

	// report es
	if (report_enc->hdr == VEC_REPORT_USED_FB_EP) {
		vec_esq_node_info( vec_ch->status.vec_esq_id, &report_enc->u.used_fb_ep.num_of_es, &report_enc->u.used_fb_ep.last_es_size );
		vec_epb_get_buffer_status( vec_ch->status.vec_epb_id, &report_enc->u.used_fb_ep.epb_occupancy, &report_enc->u.used_fb_ep.epb_size );
		vec_ch->status.available_epb = report_enc->u.used_fb_ep.epb_size - report_enc->u.used_fb_ep.epb_occupancy;

		report_enc->u.used_fb_ep.num_of_pic =  vec_picq_waiting_occupancy( vec_ch->status.vec_picq_id );
		if (picq_node) {
			report_enc->u.used_fb_ep.pic_phy_addr = picq_node->pic_phy_addr;
			report_enc->u.used_fb_ep.pic_vir_ptr = picq_node->pic_vir_ptr;
			report_enc->u.used_fb_ep.pic_size = picq_node->pic_size;
			report_enc->u.used_fb_ep.private_in_meta = picq_node->private_in_meta;
			report_enc->u.used_fb_ep.eos = picq_node->eos;
			report_enc->u.used_fb_ep.timestamp = picq_node->timestamp;
			report_enc->u.used_fb_ep.uid = picq_node->uid;
			report_enc->u.used_fb_ep.input_time =  picq_node->input_time;
			report_enc->u.used_fb_ep.feed_time =  picq_node->feed_time;

//			vec_ch->debug.timestamp_rate = vcodec_rate_update_timestamp_output( vec_ch->status.vec_rate_id, picq_node->timestamp );

			vlog_print( VLOG_VENC_VEC, "input - pic:0x%X++0x%X, outpu - ptr:0x%X++0x%X, pic type:%d, time stamp:%llu, uid:%u\n",
				picq_node->pic_phy_addr, picq_node->pic_size, vcore_report->info.done.info.pic.start_phy_addr, vcore_report->info.done.info.pic.size,
				vcore_report->info.done.info.pic.pic_type, picq_node->timestamp, picq_node->uid );
		}
		else {
			report_enc->u.used_fb_ep.pic_phy_addr = 0x0;
			report_enc->u.used_fb_ep.pic_vir_ptr = NULL;
			report_enc->u.used_fb_ep.pic_size = 0;
			report_enc->u.used_fb_ep.private_in_meta = NULL;
			report_enc->u.used_fb_ep.eos = false;
			report_enc->u.used_fb_ep.timestamp = 0xFFFFFFFFFFFFFFFFLL;
			report_enc->u.used_fb_ep.uid = 0xFFFFFFFF;
			report_enc->u.used_fb_ep.input_time =  current_time;
			report_enc->u.used_fb_ep.feed_time =  report_enc->u.used_fb_ep.input_time;
		}
	}

	if (picq_node) {
		if (vec_picq_push_encoded( vec_ch->status.vec_picq_id, picq_node ) == false) {
			vec_picq_debug_print_pic_phy_addr( vec_ch->status.vec_picq_id );
		}
		vec_ch->debug.reported_cnt++;
	}
}

static void _vec_wq_vcore_report(struct work_struct *work)
{
	struct vec_vcore_report_work *wk = container_of(work, struct vec_vcore_report_work, vcore_report_work);
	struct vec_channel *vec_ch = wk->vec_ch;
	struct vcore_enc_report *vcore_report = &wk->vcore_report;
	struct vec_report_enc report_enc;

	report_enc.hdr = VEC_REPORT_INVALID;

	mutex_lock( &vec_ch->lock );

	switch (vcore_report->hdr) {
	case VCORE_ENC_RESET:
		switch (vcore_report->info.reset) {
		case VCORE_ENC_REPORT_RESET_START :
			vlog_error( "reset start\n" );
			___vec_debug_q_status( VLOG_ERROR, vec_ch );
			vec_ch->reset_state = VEC_RESETTING;
			vec_ch->vcore_state = VEC_RUNNING;
			goto _vec_wq_vcore_report_exit;
			break;
		case VCORE_ENC_REPORT_RESET_END :
			vlog_error( "reset end\n" );

			vec_ch->reset_state = VEC_NORMAL;
			vec_ch->vcore_state = VEC_IDLE;

			report_enc.hdr = VEC_REQUEST_RESET;
			break;
		default :
			vlog_error("vcore_report->info.reset:%d\n", vcore_report->info.reset);
			break;
		}
		break;
	case VCORE_ENC_DONE :
		switch( vec_ch->close_state ) {
		case VEC_OPEN:
			vcodec_update_timer_status(vec_ch->status.vec_timer_id);
			break;
		case VEC_CLOSING:
			vec_ch->close_state = VEC_CLOSED;
			break;
		case VEC_CLOSED:
			vlog_error("already closed state\n");
			break;
		default:
			vlog_error("close_state:%d\n", vec_ch->close_state);
			break;
		}

		switch (vcore_report->info.done.hdr) {
		case VCORE_ENC_REPORT_SEQ :
			switch( vec_ch->vcore_state ) {
			case VEC_IDLE :
				report_enc.hdr = VEC_REPORT_SEQ;
				report_enc.u.seq.buf_width = vcore_report->info.done.info.seq.buf_width;
				report_enc.u.seq.buf_height = vcore_report->info.done.info.seq.buf_height;
				report_enc.u.seq.ref_frame_size = vcore_report->info.done.info.seq.ref_frame_size;
				report_enc.u.seq.ref_frame_cnt = vcore_report->info.done.info.seq.ref_frame_cnt;
				report_enc.u.seq.scratch_buf_size = vcore_report->info.done.info.seq.scratch_buf_size;
				break;
			case VEC_RUNNING :
				vlog_warning( "seq - idle\n" );
				vec_ch->vcore_state = VEC_IDLE;
				___vec_debug_q_status( VLOG_WARNING, vec_ch );
				break;
			default :
				vlog_error("vcore_state:%d\n", vec_ch->vcore_state);
				break;
			}
			break;
		case VCORE_ENC_REPORT_PIC :
			switch( vec_ch->vcore_state ) {
			case VEC_IDLE :
				vlog_warning( "encoded - idle\n" );
				___vec_debug_q_status( VLOG_WARNING, vec_ch );
				break;
			case VEC_RUNNING :
				vec_ch->vcore_state = VEC_IDLE;
				break;
			default :
				vlog_error("vcore_state:%d\n", vec_ch->vcore_state);
				break;
			}

			__vec_parse_encoded_info(vec_ch, vcore_report, &report_enc);
			break;
		default :
			vlog_error("vcore_report->info.done.hdr:%d\n", vcore_report->info.done.hdr);
			break;
		}
		break;
	case VCORE_ENC_FEED :
		__vec_feed_check_state( vec_ch );
		goto _vec_wq_vcore_report_exit;
		break;
	default :
		vlog_error("vcore_report->hdr:%d\n", vcore_report->hdr);
		break;
	}

_vec_wq_vcore_report_exit:

	mutex_unlock( &vec_ch->lock );
	kfree(wk);

	if (report_enc.hdr != VEC_REPORT_INVALID) {
		vec_ch->config.top_report_enc( vec_ch->config.top_id, &report_enc );
	}

	if (vec_ch->close_state == VEC_CLOSED) {
		vlog_warning("completion of vcore processing\n");
		complete_all(&vec_ch->close_completion);
	}
}

static void _vec_cb_vcore_report( void *vec_id, struct vcore_enc_report *vcore_report )
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;
	struct vec_vcore_report_work *wk;

	if (!vec_ch) {
		vlog_error("vec_ch is NULL\n");
		return;
	}
	else if (vec_ch->close_state == VEC_CLOSED) {
		vlog_error("closed state\n");
		return;
	}

	wk = kzalloc(sizeof(struct vec_vcore_report_work), GFP_ATOMIC);
	if (!wk) {
		vlog_error("failed to alloc buf for callback data\n");
		return;
	}
	wk->vcore_report = *vcore_report;
	wk->vec_ch = vec_ch;
	INIT_WORK(&wk->vcore_report_work, _vec_wq_vcore_report);

	queue_work(vec_ch->vcore_report_wq, &wk->vcore_report_work);
}

static bool _vec_check_vcore_status(void *vec_id)
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;
	bool running = true;

	mutex_lock( &vec_ch->lock );

	switch (vec_ch->vcore_state) {
	case VEC_IDLE:
		running = false;
		break;
	case VEC_RUNNING:
		running = true;
		break;
	default:
		vlog_error("vcore_state:%d\n", vec_ch->vcore_state);
		break;
	}

	/* debug message */
	___vec_debug_q_status( VLOG_VENC_MONITOR, vec_ch );

	mutex_unlock( &vec_ch->lock );

	return running;
}

static void _vec_watchdog_reset(void *vec_id)
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;

	mutex_lock( &vec_ch->lock );

	___vec_debug_q_status( VLOG_ERROR, vec_ch );

	vec_ch->reset_state =  VEC_RESETTING;
	vec_ch->vcore_state = VEC_RUNNING;
	vec_ch->vcore_ops.reset( vec_ch->status.vcore_id );

	mutex_unlock( &vec_ch->lock );
}

void *vec_open( struct vcore_enc_ops *vcore_ops,
				struct vcore_enc_config *config,
				unsigned int workbuf_paddr, unsigned long *workbuf_vaddr,
				unsigned int workbuf_size,
				void *top_id,
				void (*top_report_enc)( void *top_id,
									struct vec_report_enc *report_enc ) )
{
	struct vec_channel *vec_ch;
	unsigned int retry_count = 0;
	enum vcore_enc_ret vcore_ret;

	vlog_info( "codec type:%u, epb:0x%X++0x%X\n", config->codec_type, config->epb_phy_addr, config->epb_size );

	vec_ch = (struct vec_channel *)vzalloc( sizeof( struct vec_channel ) );
	if (!vec_ch) {
		vlog_error( "failed to allocate memory\n" );
		return (void *)NULL;
	}

	mutex_init( &vec_ch->lock );
	mutex_lock( &vec_ch->lock  );

	vec_ch->status.vec_picq_id = vec_picq_open();
	if (!vec_ch->status.vec_picq_id) {
		vlog_error( "failed to get vec_picq_id\n" );
		mutex_unlock( &vec_ch->lock );
		mutex_destroy( &vec_ch->lock );
		vfree( vec_ch );
		return (void *)NULL;
	}

	vec_ch->status.vec_esq_id = vec_esq_open( 0x100,
		(config->codec_type == VCORE_ENC_JPEG) ? true : false );
	if (!vec_ch->status.vec_esq_id) {
		vlog_error( "failed to get vec_esq_id\n" );
		vec_picq_close( vec_ch->status.vec_picq_id );
		mutex_unlock( &vec_ch->lock );
		mutex_destroy( &vec_ch->lock );
		vfree( vec_ch );
		return (void *)NULL;
	}

	vec_ch->status.vec_epb_id = vec_epb_open( config->epb_phy_addr,
		config->epb_vir_ptr, config->epb_size );
	if (!vec_ch->status.vec_epb_id) {
		vlog_error( "failed to get vec_epb_id\n" );
		vec_picq_close( vec_ch->status.vec_picq_id );
		vec_esq_close( vec_ch->status.vec_esq_id );
		mutex_unlock( &vec_ch->lock );
		mutex_destroy( &vec_ch->lock );
		vfree( vec_ch );
		return (void *)NULL;
	}

	vec_ch->status.vec_rate_id = vcodec_rate_open();
	if (!vec_ch->status.vec_rate_id) {
		vlog_error( "failed to get vec_rate_id\n" );
		vec_picq_close( vec_ch->status.vec_picq_id );
		vec_esq_close( vec_ch->status.vec_esq_id );
		vec_epb_close( vec_ch->status.vec_epb_id );
		mutex_unlock( &vec_ch->lock );
		mutex_destroy( &vec_ch->lock );
		vfree( vec_ch );
		return (void *)NULL;
	}

	vec_ch->status.vec_timer_id = vcodec_timer_open( (void *)vec_ch,
		_vec_check_vcore_status, _vec_watchdog_reset );
	if (!vec_ch->status.vec_timer_id) {
		vlog_error( "failed to get vec_timer_id\n" );
		vec_picq_close( vec_ch->status.vec_picq_id );
		vec_esq_close( vec_ch->status.vec_esq_id );
		vec_epb_close( vec_ch->status.vec_epb_id );
		vcodec_rate_close( vec_ch->status.vec_rate_id );
		mutex_unlock( &vec_ch->lock );
		mutex_destroy( &vec_ch->lock );
		vfree( vec_ch );
		return (void *)NULL;
	}

	vec_ch->config.codec_type = config->codec_type;
	vec_ch->config.epb_phy_addr = config->epb_phy_addr;
	vec_ch->config.epb_vir_ptr = config->epb_vir_ptr;
	vec_ch->config.epb_size = config->epb_size;
	vec_ch->config.top_id = top_id;
	vec_ch->config.top_report_enc = top_report_enc;

	vec_ch->status.num_of_picq_node = 0;
	vec_ch->status.available_epb = 0;
	vec_ch->status.num_of_free_esq_node = 0;
	vec_ch->status.esq_node_seq.es_start_addr = 0x0;

	vec_ch->debug.received_cnt = 0;
	vec_ch->debug.sent_cnt = 0;
	vec_ch->debug.reported_cnt = 0;

	vec_ch->debug.input_rate = (unsigned long)(-1);
	vec_ch->debug.decode_rate = (unsigned long)(-1);
	vec_ch->debug.timestamp_rate = (unsigned long)(-1);

	vec_ch->vcore_report_wq = create_singlethread_workqueue( "vec_vcore_report_wq" );

	vec_ch->vcore_ops = *vcore_ops;

	do {
		vcore_ret = vec_ch->vcore_ops.open( &vec_ch->status.vcore_id,
							config, workbuf_paddr, workbuf_vaddr, workbuf_size,
							vec_ch, _vec_cb_vcore_report );
		if ( vcore_ret != VCORE_ENC_RETRY )
			break;

		retry_count++;
		if ((retry_count % 1000) == 0)
			vlog_print( VLOG_MULTI_INSTANCE, "retry: %u\n", retry_count );

		schedule();
	} while (retry_count < VEC_MAX_SCHEDULE_RETRY);

	if (retry_count)
		vlog_trace( "retry: %u\n", retry_count );

	if (!vec_ch->status.vcore_id) {
		vlog_error( "failed to get vcore_id\n" );
		mutex_unlock( &vec_ch->lock );
		vec_picq_close( vec_ch->status.vec_picq_id );
		vec_esq_close( vec_ch->status.vec_esq_id );
		vec_epb_close( vec_ch->status.vec_epb_id );
		vcodec_rate_close( vec_ch->status.vec_rate_id );
		vcodec_timer_close( vec_ch->status.vec_timer_id );
		vfree( vec_ch );
		return (void *)NULL;
	}

	init_completion(&vec_ch->close_completion);

	vec_ch->reset_state = VEC_NORMAL;
	vec_ch->close_state = VEC_OPEN;
	vec_ch->flush_state = VEC_FLUSHED;
	vec_ch->pause_state = VEC_PLAYING;
	vec_ch->vcore_state = VEC_IDLE;

	list_add_tail( &vec_ch->list, &vec_ch_list );

	mutex_unlock( &vec_ch->lock );

	vlog_trace("success - vec:0x%X, vcore:0x%X\n", (unsigned int)vec_ch, (unsigned int)vec_ch->status.vcore_id);

	return (void *)vec_ch;
}

void vec_close( void *vec_id )
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;
	enum vcore_enc_ret vcore_ret;
	unsigned int retry_count = 0;

	mutex_lock( &vec_ch->lock );
	vlog_trace( "start, receive/send/report:%u/%u/%u\n",
				vec_ch->debug.received_cnt, vec_ch->debug.sent_cnt, vec_ch->debug.reported_cnt );

	switch (vec_ch->vcore_state) {
	case VEC_IDLE:
		vec_ch->close_state = VEC_CLOSED;
		break;
	case VEC_RUNNING:
		vlog_warning("vcore encoding\n");
		vec_ch->close_state = VEC_CLOSING;
		break;
	default:
		vlog_error("vcore_state:%d\n", vec_ch->vcore_state);
		break;
	}

	list_del( &vec_ch->list );

	if (vec_ch->close_state == VEC_CLOSING) {
		vlog_warning("waiting for completion of vcore processing\n");
		if (wait_for_completion_interruptible_timeout(&vec_ch->close_completion, HZ /33) <= 0) {
			vlog_error("error waiting for reply to vcore done - state:%d\n", vec_ch->close_state);
		}
		vec_ch->close_state = VEC_CLOSED;
	}
	mutex_unlock( &vec_ch->lock );

	vcodec_timer_close( vec_ch->status.vec_timer_id );

	mutex_lock( &vec_ch->lock );
	do {
		vcore_ret = vec_ch->vcore_ops.close( vec_ch->status.vcore_id );
		if ( vcore_ret != VCORE_ENC_RETRY )
			break;

		retry_count++;
		if ((retry_count % 1000) == 0)
			vlog_print( VLOG_MULTI_INSTANCE, "retry: %u\n", retry_count );

		schedule();
	} while (retry_count < VEC_MAX_SCHEDULE_RETRY);

	if (retry_count)
		vlog_trace( "retry: %u\n", retry_count );

	drain_workqueue(vec_ch->vcore_report_wq);
	destroy_workqueue(vec_ch->vcore_report_wq);

	vcodec_rate_close( vec_ch->status.vec_rate_id );
	vec_esq_close( vec_ch->status.vec_esq_id );
	vec_epb_close( vec_ch->status.vec_epb_id );
	vec_picq_close( vec_ch->status.vec_picq_id );

	mutex_unlock( &vec_ch->lock );
	mutex_destroy( &vec_ch->lock );

	vfree( vec_ch );

	vlog_trace( "end\n" );
}

void vec_resume( void *vec_id )
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;

	mutex_lock( &vec_ch->lock );

	if (vec_ch->pause_state == VEC_PLAYING)
		vlog_trace("already playing state\n");

	vec_ch->pause_state = VEC_PLAYING;

	__vec_feed_check_state( vec_ch );

	mutex_unlock( &vec_ch->lock );
}

void vec_pause( void *vec_id )
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;

	mutex_lock( &vec_ch->lock );

	if (vec_ch->pause_state == VEC_PAUSED)
		vlog_trace("already paused state\n");

	vec_ch->pause_state = VEC_PAUSED;

	mutex_unlock( &vec_ch->lock );
}

void vec_flush( void *vec_id )
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;

	mutex_lock( &vec_ch->lock );

	___vec_debug_q_status( VLOG_TRACE, vec_ch );

	switch( vec_ch->flush_state )
	{
	case VEC_FLUSHED:
		vec_ch->flush_state = VEC_FLUSHING;
		__vec_feed_check_state( vec_ch );
		break;
	case VEC_FLUSHING:
		vlog_trace("already flushing state\n");
		break;
	default :
		vlog_error("flush_state:%d\n", vec_ch->flush_state);
		break;
	}

	mutex_unlock( &vec_ch->lock );
}

bool vec_update_pic_buffer( void *vec_id, struct vec_pic *pic )
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;
	struct vec_picq_node *picq_node;
	unsigned long current_time;
	bool ret = false;

	mutex_lock( &vec_ch->lock );

	current_time = jiffies;

	picq_node = vec_picq_pop_encoded( vec_ch->status.vec_picq_id, pic->pic_phy_addr );
	if (picq_node == NULL) {
		vlog_error( "failed to pop encoded pic q, pic_addr:0x%X\n", pic->pic_phy_addr );
		mutex_unlock( &vec_ch->lock );
		return false;
	}

	picq_node->pic_phy_addr = pic->pic_phy_addr;
	picq_node->pic_vir_ptr = pic->pic_vir_ptr;
	picq_node->pic_size = pic->pic_size;
	picq_node->private_in_meta = pic->private_in_meta;
	picq_node->eos = pic->eos;
	picq_node->timestamp = pic->timestamp;
	picq_node->uid = pic->uid;
	picq_node->input_time = current_time;

	ret = vec_picq_push_waiting( vec_ch->status.vec_picq_id, picq_node );
	if (ret == false) {
		vec_picq_debug_print_pic_phy_addr( vec_ch->status.vec_picq_id );
	}

	vec_ch->debug.input_rate = vcodec_rate_update_input( vec_ch->status.vec_rate_id, current_time );
	vec_ch->debug.timestamp_rate = vcodec_rate_update_timestamp_input( vec_ch->status.vec_rate_id, picq_node->timestamp );

	vec_ch->debug.received_cnt++;

	__vec_feed_check_state( vec_ch );

	mutex_unlock( &vec_ch->lock );

	return ret;
}

void vec_request_epb_cb( void *vec_id, unsigned int num_of_free_esq_node )
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;
	struct vec_report_enc report_enc;

	mutex_lock( &vec_ch->lock );

	report_enc.hdr = VEC_REPORT_USED_FB_EP;
	vec_esq_node_info( vec_ch->status.vec_esq_id, &report_enc.u.used_fb_ep.num_of_es, &report_enc.u.used_fb_ep.last_es_size );
	vec_epb_get_buffer_status( vec_ch->status.vec_epb_id, &report_enc.u.used_fb_ep.epb_occupancy, &report_enc.u.used_fb_ep.epb_size );
	vec_ch->status.available_epb = report_enc.u.used_fb_ep.epb_size - report_enc.u.used_fb_ep.epb_occupancy;
	vec_ch->status.num_of_free_esq_node = num_of_free_esq_node;

	report_enc.u.used_fb_ep.pic_phy_addr = 0x0;
	report_enc.u.used_fb_ep.pic_vir_ptr = NULL;
	report_enc.u.used_fb_ep.pic_size = 0;
	report_enc.u.used_fb_ep.private_in_meta = NULL;
	report_enc.u.used_fb_ep.eos = false;
	report_enc.u.used_fb_ep.timestamp = 0xFFFFFFFFFFFFFFFFLL;
	report_enc.u.used_fb_ep.uid = 0xFFFFFFFF;
	report_enc.u.used_fb_ep.input_time =  jiffies;
	report_enc.u.used_fb_ep.feed_time =  report_enc.u.used_fb_ep.input_time;
	report_enc.u.used_fb_ep.num_of_pic =  vec_picq_waiting_occupancy( vec_ch->status.vec_picq_id );

	__vec_feed_check_state( vec_ch );

	mutex_unlock( &vec_ch->lock );

	vec_ch->config.top_report_enc( vec_ch->config.top_id, &report_enc );
}

bool vec_get_encoded_buffer( void *vec_id, struct vec_epb *epb )
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;
	struct vec_esq_node esq_node;
	unsigned int es_size;
	char *buf_ptr = epb->buf_ptr;
	unsigned int buf_size = epb->buf_size;
	enum vcore_enc_ret vcore_ret;
	unsigned int retry_count = 0;

	mutex_lock( &vec_ch->lock );
_vec_merge_seq_pic :

	if (vec_esq_pop( vec_ch->status.vec_esq_id, &esq_node ) == false) {
		vlog_error( "failed to pop es q\n" );
		mutex_unlock( &vec_ch->lock );
		return false;
	}

	es_size = esq_node.es_size;

	if (buf_size < es_size) {
		vlog_error( "short to es buffer size - user buf:0x%X, encoded:0x%X\n", buf_size, esq_node.es_size );
		mutex_unlock( &vec_ch->lock );
		return false;
	}

	// still image = seq hdr + pic data
	if (vec_ch->config.codec_type == VCORE_ENC_JPEG) {
		if (esq_node.pic_type == VEC_ESQ_SEQ_HDR) {
			if (vec_ch->status.esq_node_seq.es_start_addr)
				vlog_error( "already exist jpeg seq hdr - pic type:%d, buf:0x%X++0x%X\n", vec_ch->status.esq_node_seq.pic_type, vec_ch->status.esq_node_seq.es_start_addr, vec_ch->status.esq_node_seq.es_size );

			vlog_trace( "backup seq hdr(size:%u) for jpeg\n", esq_node.es_size );

			vec_ch->status.esq_node_seq = esq_node;
			goto _vec_merge_seq_pic;
		}

		if (vec_ch->status.esq_node_seq.es_start_addr) {
			es_size += vec_ch->status.esq_node_seq.es_size;
			if (buf_size < es_size) {
				vlog_error( "short to es buffer size - user buf:0x%X, seq+encoded:0x%X\n", buf_size, es_size );
				mutex_unlock( &vec_ch->lock );
				return false;
			}

			if (vec_epb_read( vec_ch->status.vec_epb_id,
							vec_ch->status.esq_node_seq.es_start_addr, vec_ch->status.esq_node_seq.es_size, vec_ch->status.esq_node_seq.es_end_addr,
							buf_ptr, buf_size ) == false) {
				vlog_error( "read compressed date from epb sqh\n" );
				mutex_unlock( &vec_ch->lock );
				return false;
			}

			vec_epb_update_rdaddr( vec_ch->status.vec_epb_id, esq_node.es_start_addr );

			do {
				vcore_ret = vec_ch->vcore_ops.update_epb_rdaddr( vec_ch->status.vcore_id, esq_node.es_start_addr );
				if ( vcore_ret != VCORE_ENC_RETRY )
					break;

				retry_count++;
				if ((retry_count % 1000) == 0)
					vlog_print( VLOG_MULTI_INSTANCE, "retry: %u\n", retry_count );

				schedule();
			} while (retry_count < VEC_MAX_SCHEDULE_RETRY);

			if (retry_count)
				vlog_trace( "retry: %u\n", retry_count );

			buf_ptr += vec_ch->status.esq_node_seq.es_size;
			buf_size -= vec_ch->status.esq_node_seq.es_size;

			vec_ch->status.esq_node_seq.es_start_addr = 0x0;
		}
	}

	if (esq_node.pic_type == VEC_ESQ_EOS) {
		vlog_trace( "end of stream\n" );
	}
	else {
		if (vec_epb_read( vec_ch->status.vec_epb_id,
						esq_node.es_start_addr, esq_node.es_size, esq_node.es_end_addr,
						buf_ptr, buf_size ) == false) {
			vlog_error( "read compressed date from epb\n" );
			mutex_unlock( &vec_ch->lock );
			return false;
		}

		vec_epb_update_rdaddr( vec_ch->status.vec_epb_id, esq_node.es_end_addr );

		do {
			vcore_ret = vec_ch->vcore_ops.update_epb_rdaddr( vec_ch->status.vcore_id, esq_node.es_end_addr );
			if ( vcore_ret != VCORE_ENC_RETRY )
				break;

			retry_count++;
			if ((retry_count % 1000) == 0)
				vlog_print( VLOG_MULTI_INSTANCE, "retry: %u\n", retry_count );

			schedule();
		} while (retry_count < VEC_MAX_SCHEDULE_RETRY);

		if (retry_count)
			vlog_trace( "retry: %u\n", retry_count );
	}

	epb->es_size = es_size;
	switch (esq_node.pic_type)
	{
	case VEC_ESQ_SEQ_HDR :
		epb->pic_type = VEC_ENC_SEQ_HDR;
		break;
	case VEC_ESQ_PIC_I :
	case VEC_ESQ_PIC_P :
	case VEC_ESQ_PIC_B :
		epb->pic_type = VEC_ENC_PIC_I + (esq_node.pic_type - VEC_ESQ_PIC_I);
		break;
	case VEC_ESQ_EOS :
		epb->pic_type = VEC_ENC_EOS;
		break;
	default :
		vlog_error( "pic type:%d\n", esq_node.pic_type );
		break;
	}
	epb->encode_time = esq_node.encode_time;
	epb->timestamp = esq_node.timestamp;
	epb->uid = esq_node.uid;
	epb->input_time = esq_node.input_time;
	epb->feed_time = esq_node.feed_time;

	mutex_unlock( &vec_ch->lock );

	return true;
}

bool vec_register_decompressed_buffer( void *vec_id, struct vcore_enc_dpb *dpb )
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;
	bool ret = false;
	enum vcore_enc_ret vcore_ret;
	unsigned int retry_count = 0;

	mutex_lock( &vec_ch->lock );

	___vec_debug_q_status( VLOG_TRACE, vec_ch );

	do {
		vcore_ret = vec_ch->vcore_ops.register_dpb( vec_ch->status.vcore_id, dpb );
		if (vcore_ret != VCORE_ENC_RETRY)
			break;

		retry_count++;
		if ((retry_count % 1000) == 0)
			vlog_print( VLOG_MULTI_INSTANCE,"retry: %u\n", retry_count );

		schedule();
	} while (retry_count < VEC_MAX_SCHEDULE_RETRY);

	if (retry_count)
		vlog_trace( "retry: %u\n", retry_count );

	if (vcore_ret == VCORE_ENC_SUCCESS )
		ret = true;

	mutex_unlock( &vec_ch->lock );

	return ret;
}

unsigned int vec_get_picq_occupancy( void *vec_id )
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;

	return vec_picq_waiting_occupancy( vec_ch->status.vec_picq_id );
}

void vec_get_esq_info( void *vec_id, struct vec_esq_info *esq_info )
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;

	mutex_lock( &vec_ch->lock );
	vec_esq_node_info( vec_ch->status.vec_esq_id, &esq_info->num_of_es, &esq_info->last_es_size );
	vec_epb_get_buffer_status( vec_ch->status.vec_epb_id, &esq_info->epb_occupancy, &esq_info->epb_size );
	vec_ch->status.available_epb = esq_info->epb_size - esq_info->epb_occupancy;
	mutex_unlock( &vec_ch->lock );
}

void vec_set_config(void *vec_id, struct vcore_enc_running_config *config)
{
	struct vec_channel *vec_ch = (struct vec_channel *)vec_id;
	enum vcore_enc_ret vcore_ret;
	unsigned int retry_count = 0;

	do {
		vcore_ret = vec_ch->vcore_ops.set_config(vec_ch->status.vcore_id, config);
		if (vcore_ret != VCORE_ENC_RETRY)
			break;

		retry_count++;
		if ((retry_count % 1000) == 0)
			vlog_print( VLOG_MULTI_INSTANCE,"retry: %u\n", retry_count );

		schedule();
	} while (retry_count < VEC_MAX_SCHEDULE_RETRY);

	if (retry_count)
		vlog_trace( "retry: %u\n", retry_count );
}

void vec_init( void )
{
	INIT_LIST_HEAD( &vec_ch_list );

	vec_picq_init();
	vec_epb_init();
	vec_esq_init();
}

void vec_cleanup( void )
{
	vec_picq_cleanup();
	vec_epb_cleanup();
	vec_esq_cleanup();
}

