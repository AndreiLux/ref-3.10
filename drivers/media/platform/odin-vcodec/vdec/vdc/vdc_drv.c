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

#include "vdc_drv.h"
#include "vdc_auq.h"
#include "vdc_dq.h"
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
#include <linux/err.h>
#include <asm/atomic.h>

#include <media/odin/vcodec/vlog.h>

#define	VDC_MAX_SEQHDR_SIZE						0x1000
#define	VDC_CORRECT_PIC_SEARCHING_THRESHOLD		0x10
#define	VDC_MAX_SCHEDULE_RETRY					0x10000


#define ___vdc_debug_q_status(log_level, vdc_ch)				\
	do {											\
		vlog_print( log_level, "state:%d-%d-%d-%d-%d, auq:%u, dq:%u/%u, receive/send/report:%u/%u/%u\n",				\
			vdc_ch->reset_state, vdc_ch->close_state, vdc_ch->flush_state, vdc_ch->pause_state, vdc_ch->vcore_state,								\
			vdc_ch->status.num_of_auq_alive_node, vdc_ch->status.num_of_dq_alive_node, vdc_ch->status.num_of_dq_free_node,	\
			vdc_ch->debug.received_cnt, vdc_ch->debug.sent_cnt, vdc_ch->debug.reported_au_cnt);							\
	} while(0)

struct vdc_channel
{
	struct list_head list;

	volatile enum
	{
		VDC_RESETTING,
		VDC_NORMAL,
	} reset_state;

	volatile enum
	{
		VDC_OPEN,
		VDC_CLOSING,
		VDC_CLOSED,
	} close_state;

	volatile enum
	{
		VDC_FLUSHED,
		VDC_RECEIVED_FLUSH_FROM_TOP,
		VDC_SENT_FLUSH_CMD_TO_VCORE,
	} flush_state;

	volatile enum
	{
		VDC_PAUSED,
		VDC_PLAYING,
	} pause_state;

	volatile enum
	{
		VDC_IDLE,
		VDC_RUNNING,
		VDC_RUNNING_FEED,
	} vcore_state;

	struct completion vcore_completion;
	struct mutex lock;

	struct
	{
		enum vcore_dec_codec codec_type;
		unsigned int cpb_phy_addr;
		unsigned char *cpb_vir_ptr;
		unsigned int cpb_size;
		bool reordering;
		bool rendering_dpb;
		bool secure_buf;
		unsigned int flush_age;
		void *top_id;
		void (*top_used_au)( void *top_id, struct vdc_report_au *report_au );
		void (*top_report_dec)( void *top_id, struct vdc_report_dec *report_dec );
	} config;

	struct vcore_dec_ops vcore_ops;
	struct vcore_dec_au vcore_au;

	struct workqueue_struct *vcore_report_wq;

	unsigned int timer_cnt;

	struct
	{
		unsigned int last_au_end_addr;
		unsigned int end_phy_addr;
		unsigned int flush_age;
		unsigned int chunk_id;

		unsigned int correct_pic_search_cnt;
		unsigned int vcore_update_buffer_au_failed_cnt;

		void *vdc_auq_id;
		void *vdc_dq_id;
		void *vdc_rate_id;
		void *vdc_timer_id;
		void *vcore_id;

		unsigned int total_dpb_num;

		bool remaining_data_in_es;
		unsigned int num_of_auq_alive_node;
		unsigned int num_of_dq_alive_node;
		unsigned int num_of_dq_free_node;

		unsigned int num_of_au_per_1pic;
	} status;

	struct vcore_dec_dpb dpb[VDEC_MAX_NUM_OF_DPB];
	unsigned int register_dpb_num;		/* head */
	unsigned int unregister_dpb_num;	/* tail */

	struct
	{
		bool init_done;
		bool over_spec;

		unsigned int dec_fail_cnt;
		unsigned int ref_frame_cnt;

		unsigned int seq_phy_addr;
		unsigned char *seq_vir_ptr;
		unsigned int sid;
		unsigned int seq_size;
		unsigned char seq_data[VDC_MAX_SEQHDR_SIZE];
	} seq_info;

	struct
	{
		unsigned int received_cnt;
		unsigned int sent_cnt;
		unsigned int reported_au_cnt;
		unsigned int reported_pic_cnt;
		unsigned int rotated_pic_cnt;

		unsigned long input_rate;		// * 100
		unsigned long decode_rate;	// * 100
		unsigned long timestamp_rate;	// * 100

		unsigned int flushing_feed_cnt;

		unsigned int request_feed_log;
	} debug;
};

struct vdc_vcore_report_work
{
	struct work_struct vcore_report_work;
	struct vcore_dec_report vcore_report;
	struct vdc_channel *vdc_ch;
};

struct list_head vdc_ch_list;

static void _vdc_reset( struct vdc_channel *vdc_ch )
{
	vdc_dq_reset( vdc_ch->status.vdc_dq_id );
	vcodec_rate_reset_timestamp( vdc_ch->status.vdc_rate_id );

	vdc_ch->reset_state = VDC_NORMAL;
	vdc_ch->close_state = VDC_OPEN;
	vdc_ch->flush_state = VDC_FLUSHED;
	vdc_ch->pause_state = VDC_PLAYING;
	vdc_ch->vcore_state = VDC_IDLE;

	vdc_ch->status.chunk_id = 0;
	vdc_ch->status.correct_pic_search_cnt = 0;
	vdc_ch->status.total_dpb_num = 0;
	vdc_ch->status.vcore_update_buffer_au_failed_cnt = 0;

	vdc_ch->status.num_of_auq_alive_node = 0;
	vdc_ch->status.num_of_dq_alive_node = 0;
	vdc_ch->status.num_of_dq_free_node = 0;

	vdc_ch->seq_info.init_done = false;
	vdc_ch->seq_info.over_spec = false;
	vdc_ch->seq_info.dec_fail_cnt = 0;
	vdc_ch->seq_info.ref_frame_cnt = 0;
	vdc_ch->seq_info.seq_phy_addr = 0x0;
	vdc_ch->seq_info.seq_vir_ptr = NULL;
	vdc_ch->seq_info.seq_size = 0;

	vdc_ch->debug.received_cnt = 0;
	vdc_ch->debug.sent_cnt = 0;
	vdc_ch->debug.reported_au_cnt = 0;
	vdc_ch->debug.reported_pic_cnt = 0;

	vdc_ch->status.num_of_au_per_1pic = 1;

	vdc_ch->debug.input_rate = (unsigned long)(-1);
	vdc_ch->debug.decode_rate = (unsigned long)(-1);
	vdc_ch->debug.timestamp_rate = (unsigned long)(-1);

	vdc_ch->debug.request_feed_log = 0;
}

static unsigned int _vdc_flush_auq( struct vdc_channel *vdc_ch )
{
	struct vdc_auq_node auq_node;
	struct vdc_report_au report_au;
	unsigned int flush_au_cnt = 0;

	if (vdc_ch->seq_info.init_done == false) {
		vlog_warning( "not initialised seq\n" );
	}

	vdc_ch->status.correct_pic_search_cnt = 0;

	___vdc_debug_q_status( VLOG_TRACE, vdc_ch );
	vdc_ch->debug.sent_cnt = 0;

	vcodec_rate_reset_codec_done( vdc_ch->status.vdc_rate_id );

	report_au.end_phy_addr = vdc_ch->status.last_au_end_addr;

	while (vdc_auq_pop_decoding( vdc_ch->status.vdc_auq_id, &auq_node ) == true) {
		vlog_error( "exist the node in decoding au q\n" );

		flush_au_cnt++;

		if (vdc_ch->config.top_used_au) {
			report_au.type = auq_node.type;
			report_au.p_buf = auq_node.p_buf;
			report_au.start_phy_addr = auq_node.start_phy_addr;
			report_au.start_vir_ptr = auq_node.start_vir_ptr;
			report_au.size = auq_node.size;
			report_au.end_phy_addr = auq_node.end_phy_addr;
			report_au.private_in_meta = auq_node.private_in_meta;
			report_au.timestamp = auq_node.timestamp;
			report_au.uid = auq_node.uid;
			report_au.input_time = auq_node.input_time;
			report_au.used_time = jiffies;
			report_au.num_of_au = vdc_auq_occupancy( vdc_ch->status.vdc_auq_id );
			vdc_ch->config.top_used_au( vdc_ch->config.top_id, &report_au );
			vlog_print(VLOG_VDEC_FLUSH, "flush decoding au, buf:0x%X, chunk:%u\n", (unsigned int)auq_node.p_buf, auq_node.chunk_id);
		}
		else {
			vlog_error( "no callback path for feeding au info\n" );
		}
	}

	if (vdc_auq_peep_waiting( vdc_ch->status.vdc_auq_id, &auq_node ) == true) {
		while  (auq_node.flush_age != vdc_ch->config.flush_age) {
			vdc_auq_pop_waiting( vdc_ch->status.vdc_auq_id, &auq_node );
			flush_au_cnt++;

			if (vdc_ch->config.top_used_au) {
				report_au.type = auq_node.type;
				report_au.p_buf = auq_node.p_buf;
				report_au.start_phy_addr = auq_node.start_phy_addr;
				report_au.start_vir_ptr = auq_node.start_vir_ptr;
				report_au.size = auq_node.size;
				report_au.end_phy_addr = auq_node.end_phy_addr;
				report_au.private_in_meta = auq_node.private_in_meta;
				report_au.timestamp = auq_node.timestamp;
				report_au.uid = auq_node.uid;
				report_au.input_time = auq_node.input_time;
				report_au.used_time = jiffies;
				report_au.num_of_au = vdc_auq_occupancy( vdc_ch->status.vdc_auq_id );
				vdc_ch->config.top_used_au( vdc_ch->config.top_id, &report_au );
				vlog_print(VLOG_VDEC_FLUSH, "flush waiting au, buf:0x%X, chunk:%u\n", (unsigned int)auq_node.p_buf, auq_node.chunk_id);
			}
			else {
				vlog_error( "no callback path for feeding au info\n" );
			}

			if (vdc_auq_peep_waiting( vdc_ch->status.vdc_auq_id, &auq_node ) == false) {
				break;
			}
		}
	}

	return report_au.end_phy_addr;
}

static void _vdc_flush_dq( struct vdc_channel *vdc_ch )
{
	struct vdc_dq_node *dq_node;

	while ((dq_node = vdc_dq_pop_for_flush( vdc_ch->status.vdc_dq_id )) != NULL) {
		if (vdc_ch->config.top_report_dec) {
			struct vdc_report_dec report_dec;

			report_dec.hdr = VDC_RETURN_DEC_BUF;
			report_dec.u.buf.dpb_addr = dq_node->dpb_addr;
			report_dec.u.buf.private_out_meta = dq_node->private_out_meta;
			report_dec.u.buf.rotation_angle = vdc_ch->vcore_au.meta.rotation_angle;

			vdc_ch->config.top_report_dec( vdc_ch->config.top_id, &report_dec );
			vlog_print(VLOG_VDEC_FLUSH, "flush dpb, addr:0x%X\n", dq_node->dpb_addr);
		}
		else {
			vlog_error( "no callback path for report\n" );
		}

		vdc_dq_push_displaying( vdc_ch->status.vdc_dq_id, dq_node );
	}
}

static void _vdc_flush_enter_flush_state( struct vdc_channel *vdc_ch )
{
	vlog_trace( "\n" );

	vdc_ch->debug.flushing_feed_cnt = 0;
}

static bool _vdc_flush_do_flush_state( struct vdc_channel *vdc_ch )
{
	unsigned int next_node_start_addr;
	enum vcore_dec_ret vcore_ret;
	unsigned int retry_count = 0;

	vlog_trace( "\n" );

	next_node_start_addr = _vdc_flush_auq( vdc_ch );

	do {
		vcore_ret = vdc_ch->vcore_ops.flush( vdc_ch->status.vcore_id, next_node_start_addr);
		if (vcore_ret != VCORE_DEC_RETRY)
			break;

		retry_count++;
		if ((retry_count % 1000) == 0)
			vlog_print( VLOG_MULTI_INSTANCE, "retry: %u\n", retry_count );

	} while (retry_count < VDC_MAX_SCHEDULE_RETRY);

	if (retry_count) {
		vlog_trace( "retry: %u\n", retry_count );
		if (retry_count == VDC_MAX_SCHEDULE_RETRY)
			vlog_error( "retry: %u\n", retry_count );
	}

	return (vcore_ret == VCORE_DEC_SUCCESS) ? true : false;
}

static void _vdc_flush_exit_flush_state( struct vdc_channel *vdc_ch )
{
	vlog_trace( "\n" );

	_vdc_flush_dq( vdc_ch );
}

static bool ___vdc_feed_register_dpb( struct vdc_channel *vdc_ch, struct vcore_dec_dpb *dpb )
{
	bool ret = false;
	enum vcore_dec_ret vcore_ret;
	unsigned int retry_count = 0;

	do {
		vcore_ret = vdc_ch->vcore_ops.register_dpb( vdc_ch->status.vcore_id, dpb );
		if (vcore_ret != VCORE_DEC_RETRY)
			break;

		retry_count++;
		if ((retry_count % 1000) == 0)
			vlog_print( VLOG_MULTI_INSTANCE, "retry: %u\n", retry_count );

	} while (retry_count < VDC_MAX_SCHEDULE_RETRY);

	if (retry_count) {
		vlog_trace( "retry: %u\n", retry_count );
		if (retry_count == VDC_MAX_SCHEDULE_RETRY)
			vlog_error( "retry: %u\n", retry_count );
	}

	if (vcore_ret == VCORE_DEC_SUCCESS ) {
		ret = true;
	}
	else {
		vlog_error( "failed to register\n" );
		ret = false;
	}

	return ret;
}

static bool __vdc_feed_register_dpb( struct vdc_channel *vdc_ch )
{
	bool ret = false;

	while (vdc_ch->register_dpb_num <  vdc_ch->unregister_dpb_num ) {
		if ((ret = ___vdc_feed_register_dpb(vdc_ch, &vdc_ch->dpb[vdc_ch->register_dpb_num])) == false)
			break;

		vdc_ch->register_dpb_num++;
	}

	return ret;
}

static bool __vdc_feed_clear_dpb( struct vdc_channel *vdc_ch )
{
	struct vdc_dq_node *dq_node = NULL;

	dq_node = vdc_dq_pop_clear( vdc_ch->status.vdc_dq_id );

	while (dq_node != NULL) {
		if (vdc_ch->vcore_ops.clear_dpb( vdc_ch->status.vcore_id, dq_node->dpb_addr, dq_node->sid ) == VCORE_DEC_FAIL) {
			vlog_error( "clear_dpb:0x%X\n", dq_node->dpb_addr );
			vdc_dq_push_clear( vdc_ch->status.vdc_dq_id, dq_node );
			return false;
		}
		vdc_dq_push_vcore( vdc_ch->status.vdc_dq_id, dq_node );

		dq_node = vdc_dq_pop_clear( vdc_ch->status.vdc_dq_id );
	}

	return true;
}

static bool __vdc_feed_try_to_send_au( struct vdc_channel *vdc_ch )
{
	struct vdc_auq_node auq_node;
	struct vcore_dec_au *vcore_au = &vdc_ch->vcore_au;
	bool run;
	vcore_bool_t running;
	enum vcore_dec_ret ret;

	if (vdc_ch->status.remaining_data_in_es == true) {
		vcore_au = NULL;
	}
	else
	{
		if (vdc_auq_peep_waiting( vdc_ch->status.vdc_auq_id, &auq_node ) == false) {
			vlog_error( "failed to pop auq\n" );
			return false;
		}

		if (auq_node.flush_age != vdc_ch->config.flush_age)
			vlog_warning( "config flush age:%u, au q - flush age:%u, chunk id:%u\n", vdc_ch->config.flush_age, auq_node.flush_age, auq_node.chunk_id );

		switch (auq_node.type)
		{
		case VDC_AU_SEQ :
			if (vdc_ch->seq_info.init_done == true) {
				vlog_warning( "force to convert au type to pic\n" );
				vcore_au->buf.au_type = VCORE_DEC_AU_PICTURE;
			}
			else {
				vcore_au->buf.au_type = VCORE_DEC_AU_SEQUENCE;
			}
			break;
		case VDC_AU_PIC :
			if (vdc_ch->seq_info.init_done == false) {
				vlog_warning( "force to convert au type to seq\n" );
				vcore_au->buf.au_type = VCORE_DEC_AU_SEQUENCE;
			}
			else {
				vcore_au->buf.au_type = VCORE_DEC_AU_PICTURE;
			}
			break;
		case VDC_AU_UNKNOWN :
			if (vdc_ch->seq_info.init_done == false)
				vcore_au->buf.au_type = VCORE_DEC_AU_SEQUENCE;
			else
				vcore_au->buf.au_type = VCORE_DEC_AU_PICTURE;
			break;
		default :
			vlog_error( "au type: %u\n", auq_node.type );
			return false;
		}

		vcore_au->buf.start_phy_addr = auq_node.start_phy_addr;
		vcore_au->buf.start_vir_ptr = auq_node.start_vir_ptr;
		vcore_au->buf.size = auq_node.size;
		vcore_au->buf.end_phy_addr = auq_node.end_phy_addr;
		vcore_au->eos = (auq_node.eos == true) ? VCORE_TRUE : VCORE_FALSE;
		vcore_au->meta.timestamp = auq_node.timestamp;
		vcore_au->meta.uid = auq_node.uid;
		vcore_au->meta.input_time = auq_node.input_time;
		vcore_au->meta.feed_time = jiffies;
		vcore_au->meta.flush_age = auq_node.flush_age;
		vcore_au->meta.chunk_id = auq_node.chunk_id;

		if (auq_node.chunk_id != 0 && auq_node.eos == false && vdc_ch->config.secure_buf == false)
			if (auq_node.start_phy_addr != vdc_ch->status.last_au_end_addr)
				vlog_error("Not continuous au !!! >> old 0x%08x, new 0x%08x (%d)\n",
						vdc_ch->status.last_au_end_addr,
						auq_node.start_phy_addr, auq_node.chunk_id);

		vlog_print( VLOG_VDEC_VDC, "au type:%d, addr:0x%X/0x%X++0x%X, time stamp:%llu, flush_age:%u, chunk id:%u\n",
				auq_node.type, auq_node.start_phy_addr, (unsigned int)auq_node.start_vir_ptr, auq_node.size,
				auq_node.timestamp, auq_node.flush_age, auq_node.chunk_id );
	}

	ret = vdc_ch->vcore_ops.update_buffer(vdc_ch->status.vcore_id, vcore_au, &running);
	run = (running == VCORE_TRUE) ? true : false;
	if (vcore_au) {
		if (ret == VCORE_DEC_SUCCESS) {
			vdc_ch->status.last_au_end_addr = auq_node.end_phy_addr;

			vdc_ch->status.vcore_update_buffer_au_failed_cnt = 0;
			if (vdc_auq_move_waiting_to_decoding( vdc_ch->status.vdc_auq_id, &auq_node ) == false) {
				vlog_error( "failed to pop auq\n" );
			}
			else {
				if (vdc_ch->seq_info.init_done == false) {
					if (vdc_ch->seq_info.seq_phy_addr == 0x0) {
						vdc_ch->seq_info.seq_phy_addr = auq_node.start_phy_addr;
						vdc_ch->seq_info.seq_vir_ptr = auq_node.start_vir_ptr;
					}

					vdc_ch->seq_info.seq_size += auq_node.size;
				}
			}
		}
		else if (ret == VCORE_DEC_RETRY) {
			vdc_ch->status.vcore_update_buffer_au_failed_cnt++;
			if (vdc_ch->status.vcore_update_buffer_au_failed_cnt > VDC_MAX_SCHEDULE_RETRY) {
				vdc_ch->status.vcore_update_buffer_au_failed_cnt = 0;
				if (vdc_auq_move_waiting_to_decoding( vdc_ch->status.vdc_auq_id, &auq_node ) == false) {
					vlog_error( "failed to pop auq\n" );
				}
			}
		}
		else { /* if (ret == VCORE_DEC_FAIL) */
			vlog_error( "failed to update au to vcore - instance:0x%X, au type:%d, addr:0x%X++0x%X=0x%X, flush_age:%u, chunk id:%u\n",
					(unsigned int)vdc_ch, auq_node.type, auq_node.start_phy_addr, auq_node.size, auq_node.end_phy_addr,
					auq_node.flush_age, auq_node.chunk_id );
		}

		vdc_ch->debug.sent_cnt++;
	}
	else {
		if (ret == VCORE_DEC_FAIL) {
			vlog_print( VLOG_VDEC_VDC, "failed to update au to vcore - instance:0x%X\n", (unsigned int)vdc_ch );
			vdc_ch->status.remaining_data_in_es = false;
		}
	}

	return run;
}

static unsigned int __vdc_calculate_num_of_free_dpb(struct vdc_channel *vdc_ch )
{
	unsigned int max_of_free_dq_node;

	if (vdc_ch->status.total_dpb_num <= vdc_ch->seq_info.ref_frame_cnt) {
		vlog_error( "total(%d) < ref(%d)\n",  vdc_ch->status.total_dpb_num, vdc_ch->seq_info.ref_frame_cnt);
		return 0;
	}

	max_of_free_dq_node = vdc_ch->status.total_dpb_num - vdc_ch->seq_info.ref_frame_cnt;
	vdc_ch->status.num_of_dq_alive_node = vdc_dq_displaying_occupancy( vdc_ch->status.vdc_dq_id );

	if (vdc_ch->status.num_of_dq_alive_node > max_of_free_dq_node) {
		unsigned int num_of_vcore_dq_node;

		num_of_vcore_dq_node = vdc_dq_vcore_occupancy( vdc_ch->status.vdc_dq_id );
		if (num_of_vcore_dq_node)
			vlog_warning( "almost overflow dq - a%u + v%u(r%u) = t%u\n",
						vdc_ch->status.num_of_dq_alive_node,
						num_of_vcore_dq_node, vdc_ch->seq_info.ref_frame_cnt,
						vdc_ch->status.total_dpb_num );

		vdc_ch->status.num_of_dq_free_node = 0;
	}
	else {
		const unsigned int dq_margin = 0;

		vdc_ch->status.num_of_dq_free_node = max_of_free_dq_node - vdc_ch->status.num_of_dq_alive_node;

		if ((vdc_ch->status.total_dpb_num >= vdc_ch->seq_info.ref_frame_cnt) &&
			(vdc_ch->status.num_of_dq_free_node >= dq_margin))
			vdc_ch->status.num_of_dq_free_node -= dq_margin;
	}

	return vdc_ch->status.num_of_dq_free_node;
}

static bool __vdc_feed_evaluate_inout_q(struct vdc_channel *vdc_ch, unsigned int auq_margin)
{
	unsigned int num_of_auq_alive_node_prev;
	unsigned int num_of_dq_free_node;
	bool run = false;

	num_of_auq_alive_node_prev = vdc_ch->status.num_of_auq_alive_node;
	vdc_ch->status.num_of_auq_alive_node = vdc_auq_occupancy( vdc_ch->status.vdc_auq_id );

	if (vdc_ch->seq_info.init_done == true) {
		num_of_dq_free_node = vdc_ch->status.num_of_dq_free_node;
	}
	else if (vdc_ch->seq_info.over_spec == true) {
		vlog_error("over capability stream\n");
		num_of_dq_free_node = 0;
	}
	else {
		num_of_dq_free_node = (vdc_ch->vcore_state == VDC_IDLE) ? 1 : 0;
	}

	if (((vdc_ch->status.num_of_auq_alive_node > auq_margin) || vdc_ch->status.remaining_data_in_es) &&
		num_of_dq_free_node) {
		run = __vdc_feed_try_to_send_au( vdc_ch );
	}
	else {
		if ((num_of_auq_alive_node_prev) && (!vdc_ch->status.num_of_auq_alive_node) &&
			(num_of_dq_free_node))
			vlog_warning( "auq empty\n" );
	}

	return run;
}

static bool __vdc_feed_check_state(struct vdc_channel *vdc_ch )
{
	bool run = false;
	unsigned int auq_margin = 0;

	// check 1st state
	switch( vdc_ch->reset_state ) {
	case VDC_NORMAL:

		// check 2nd state
		switch( vdc_ch->close_state ) {
		case VDC_OPEN:

			// check 3rd state
			switch( vdc_ch->flush_state )
			{
			case VDC_FLUSHED:

				// check 4th state
				switch( vdc_ch->pause_state )
				{
				case VDC_PAUSED:
					vlog_trace("paused\n");
					break;
				case VDC_PLAYING:

					// check 5th state
					switch (vdc_ch->vcore_state) {
					case VDC_IDLE:
						__vdc_feed_register_dpb( vdc_ch );
						__vdc_feed_clear_dpb( vdc_ch );

						if (vdc_ch->status.num_of_au_per_1pic > 1)
							auq_margin = 1;
					case VDC_RUNNING_FEED:
						if ((run = __vdc_feed_evaluate_inout_q( vdc_ch, auq_margin )) == true) {
							if (vdc_ch->vcore_state == VDC_IDLE) {
								vcodec_update_timer_status(vdc_ch->status.vdc_timer_id);
							}
							vdc_ch->vcore_state = VDC_RUNNING;
						}
						else if (vdc_ch->vcore_state == VDC_RUNNING_FEED) {
							vlog_error("reject to update buffer at RUNNING_FEED state\n");
						}
						break;
					case VDC_RUNNING:
//						vlog_warning("play - vcore decoding\n");
						break;
					default:
						vlog_error("vcore_state:%d\n", vdc_ch->vcore_state);
						break;
					}
					break;

				default:
					vlog_error("pause_state:%d\n", vdc_ch->pause_state);
					break;
				}
				break;

			case VDC_RECEIVED_FLUSH_FROM_TOP:

				// check 4th state
				switch (vdc_ch->vcore_state) {
				case VDC_IDLE:
					run = true;
					if (_vdc_flush_do_flush_state( vdc_ch ) == true) {
						vdc_ch->flush_state = VDC_SENT_FLUSH_CMD_TO_VCORE;
					}
					else {
						vlog_error("failed to flush vcore\n");
					}
					break;
				case VDC_RUNNING:
					vlog_warning("flush - vcore decoding\n");
					break;
				case VDC_RUNNING_FEED:
					__vdc_feed_evaluate_inout_q( vdc_ch, auq_margin );
					break;
				default:
					vlog_error("vcore_state:%d\n", vdc_ch->vcore_state);
					break;
				}
				break;

			case VDC_SENT_FLUSH_CMD_TO_VCORE:
				vlog_trace("flushing\n");
				break;
			default :
				vlog_error("flush_state:%d\n", vdc_ch->flush_state);
				break;
			}

			break;
		case VDC_CLOSING:

			switch (vdc_ch->vcore_state) {
			case VDC_IDLE:
				vlog_error("idle - closing\n");
				break;
			case VDC_RUNNING_FEED:
				run = __vdc_feed_evaluate_inout_q( vdc_ch, auq_margin );
				break;
			case VDC_RUNNING:
				vlog_warning("close - vcore decoding\n");
				break;
			default:
				vlog_error("vcore_state:%d\n", vdc_ch->vcore_state);
				break;
			}

			break;
		case VDC_CLOSED:
			vlog_warning("already closed state\n");
			break;
		default:
			vlog_error("close_state:%d\n", vdc_ch->close_state);
			break;
		}

		break;
	case VDC_RESETTING:
		vlog_warning("vcore resetting\n");
		break;
	default:
		vlog_error("reset_state:%d\n", vdc_ch->reset_state);
		break;
	}

	return run;
}

static unsigned int __vdc_report_used_au( void *vdc_id )
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;
	struct vdc_auq_node auq_node;
	unsigned int used_au_cnt = 0;

	while (vdc_auq_pop_decoding( vdc_ch->status.vdc_auq_id, &auq_node ) == true) {
		used_au_cnt++;

		if (vdc_ch->config.top_used_au) {
			struct vdc_report_au report_au;

			report_au.type = auq_node.type;
			report_au.p_buf = auq_node.p_buf;
			report_au.start_phy_addr = auq_node.start_phy_addr;
			report_au.start_vir_ptr = auq_node.start_vir_ptr;
			report_au.size = auq_node.size;
			report_au.end_phy_addr = auq_node.end_phy_addr;
			report_au.private_in_meta = auq_node.private_in_meta;
			report_au.timestamp = auq_node.timestamp;
			report_au.uid = auq_node.uid;
			report_au.input_time = auq_node.input_time;
			report_au.used_time = jiffies;
			report_au.num_of_au = vdc_auq_occupancy( vdc_ch->status.vdc_auq_id );
			vdc_ch->config.top_used_au( vdc_ch->config.top_id, &report_au );
		}
		else {
			vlog_error( "no callback path for feeding au info\n" );
		}

		vdc_ch->debug.reported_au_cnt++;
	}

	return used_au_cnt;
}
static bool __vdc_pack_seqhdr( struct vdc_channel *vdc_ch, struct vdc_report_dec *report_dec )
{
	vlog_warning( "auq:%u, dq:%u, receive/send/report:%u/%u/%u, au:0x%X/0x%X++0x%X, time stamp:%llu, chunk id:%u\n",
				vdc_auq_occupancy( vdc_ch->status.vdc_auq_id ),
				vdc_dq_displaying_occupancy( vdc_ch->status.vdc_dq_id ),
				vdc_ch->debug.received_cnt, vdc_ch->debug.sent_cnt, vdc_ch->debug.reported_au_cnt,
				vdc_ch->vcore_au.buf.start_phy_addr, (unsigned int)vdc_ch->vcore_au.buf.start_vir_ptr, vdc_ch->vcore_au.buf.size,
				vdc_ch->vcore_au.meta.timestamp, vdc_ch->vcore_au.meta.chunk_id );

	if (vdc_ch->seq_info.init_done == false) {
		report_dec->u.reset.seq_vir_ptr = NULL;
		report_dec->u.reset.seq_size = 0;
		return false;
	}

	report_dec->u.reset.seq_vir_ptr = vdc_ch->seq_info.seq_vir_ptr;
	report_dec->u.reset.seq_size = vdc_ch->seq_info.seq_size;
	return true;
}

static bool __vdc_parse_seq_info( struct vdc_channel *vdc_ch, struct vcore_dec_report *vcore_report, struct vdc_report_dec *report_dec )
{
	vlog_trace( "%s, profile:%u, level:%u\n",
				(vcore_report->info.done.info.seq.success == VCORE_TRUE) ? "success" : "fail",
				vcore_report->info.done.info.seq.profile, vcore_report->info.done.info.seq.level );

	if (vcore_report->info.done.info.seq.success == VCORE_TRUE) {
		vdc_ch->seq_info.init_done = true;
		vdc_ch->seq_info.ref_frame_cnt = vcore_report->info.done.info.seq.ref_frame_cnt;

		if (vdc_ch->seq_info.seq_size > VDC_MAX_SEQHDR_SIZE) {
			vdc_ch->seq_info.seq_phy_addr += (vdc_ch->seq_info.seq_size - VDC_MAX_SEQHDR_SIZE);
			vdc_ch->seq_info.seq_vir_ptr += (vdc_ch->seq_info.seq_size - VDC_MAX_SEQHDR_SIZE);
			vdc_ch->seq_info.seq_size = VDC_MAX_SEQHDR_SIZE;
		}

		if (vdc_ch->config.secure_buf == false)
			memcpy( vdc_ch->seq_info.seq_data, vdc_ch->seq_info.seq_vir_ptr, vdc_ch->seq_info.seq_size );
	}
	else {
		vdc_ch->seq_info.init_done = false;
		return false;
	}

	vlog_info( "uid:%u, ref:%u, buf:%u * %u, pic:%u * %u, crop:(%u,%u) (%u,%u)\n",
				vcore_report->info.done.info.seq.sid,
				vcore_report->info.done.info.seq.ref_frame_cnt,
				vcore_report->info.done.info.seq.buf_width, vcore_report->info.done.info.seq.buf_height,
				vcore_report->info.done.info.seq.pic_width, vcore_report->info.done.info.seq.pic_height,
				vcore_report->info.done.info.seq.crop.left, vcore_report->info.done.info.seq.crop.top,
				vcore_report->info.done.info.seq.crop.right, vcore_report->info.done.info.seq.crop.bottom);

	report_dec->u.seq.sid = vcore_report->info.done.info.seq.sid;
	vdc_ch->seq_info.sid = vcore_report->info.done.info.seq.sid;
	report_dec->u.seq.success = (vcore_report->info.done.info.seq.success == VCORE_TRUE) ? true : false;
	vdc_ch->seq_info.over_spec = (vcore_report->info.done.info.seq.over_spec == VCORE_TRUE) ? true : false;
	if (vdc_ch->seq_info.over_spec == true) {
		report_dec->u.seq.success = false;
	}
	report_dec->u.seq.format = vcore_report->info.done.info.seq.format;
	report_dec->u.seq.buf_width = vcore_report->info.done.info.seq.buf_width;
	report_dec->u.seq.buf_height = vcore_report->info.done.info.seq.buf_height;
	report_dec->u.seq.pic_width = vcore_report->info.done.info.seq.pic_width;
	report_dec->u.seq.pic_height = vcore_report->info.done.info.seq.pic_height;
	report_dec->u.seq.internal_buf_size.once_buf_size = \
				vcore_report->info.done.info.seq.internal_buf_size.once_buf_size;
	report_dec->u.seq.internal_buf_size.each_buf_size = \
				vcore_report->info.done.info.seq.internal_buf_size.each_buf_size;
	report_dec->u.seq.crop.left = vcore_report->info.done.info.seq.crop.left;
	report_dec->u.seq.crop.right = vcore_report->info.done.info.seq.crop.right;
	report_dec->u.seq.crop.top = vcore_report->info.done.info.seq.crop.top;
	report_dec->u.seq.crop.bottom = vcore_report->info.done.info.seq.crop.bottom;
	report_dec->u.seq.ref_frame_cnt = vcore_report->info.done.info.seq.ref_frame_cnt;
	report_dec->u.seq.profile = vcore_report->info.done.info.seq.profile;
	report_dec->u.seq.level = vcore_report->info.done.info.seq.level;
	report_dec->u.seq.frame_rate.residual = vcore_report->info.done.info.seq.frame_rate.residual;
	report_dec->u.seq.frame_rate.divider = vcore_report->info.done.info.seq.frame_rate.divider;
	report_dec->u.seq.uid = vcore_report->info.done.info.seq.meta.uid;
	report_dec->u.seq.input_time = vcore_report->info.done.info.seq.meta.input_time;
	report_dec->u.seq.feed_time = vcore_report->info.done.info.seq.meta.feed_time;
	report_dec->u.seq.decode_time = jiffies;

	return true;
}

static bool __vdc_parse_pic_info( struct vdc_channel *vdc_ch, struct vcore_dec_report *vcore_report, struct vdc_report_dec *report_dec )
{
	struct vdc_dq_node *dq_node = NULL;
	unsigned long dts_rate;
	unsigned long pts_rate;

	if (vcore_report->info.done.info.pic.success == VCORE_FALSE) {
		vlog_error( "failed to decode\n" );
		___vdc_debug_q_status( VLOG_ERROR, vdc_ch );
		vdc_dq_debug_print_dpb_addr( vdc_ch->status.vdc_dq_id );
		return false;
	}
	vdc_ch->debug.decode_rate = vcodec_rate_update_codec_done( vdc_ch->status.vdc_rate_id, jiffies );

	dq_node = vdc_dq_pop_decoded( vdc_ch->status.vdc_dq_id, vcore_report->info.done.info.pic.dpb_addr );
	if (dq_node == NULL) {
		vlog_error( "failed to pop dq, dpb_addr:0x%X\n", vcore_report->info.done.info.pic.dpb_addr );
		vdc_dq_debug_print_dpb_addr( vdc_ch->status.vdc_dq_id );
		return false;
	}

	if (vdc_ch->status.flush_age != vdc_ch->config.flush_age) {
		if (vcore_report->info.done.info.pic.meta.flush_age == vdc_ch->config.flush_age) {
			if ( (vcore_report->info.done.info.pic.pic_type == VCORE_DEC_PIC_I) &&
				(vcore_report->info.done.info.pic.err_mbs == 0)) {
				vdc_ch->status.flush_age = vdc_ch->config.flush_age;

				___vdc_debug_q_status( VLOG_TRACE, vdc_ch );
				vdc_ch->debug.reported_pic_cnt = 0;
				vdc_ch->debug.rotated_pic_cnt= 0;

				vcodec_rate_reset_timestamp( vdc_ch->status.vdc_rate_id );
				vlog_trace( "apply new flush age:%u\n", vdc_ch->config.flush_age );
			}
			else {

				vdc_ch->status.correct_pic_search_cnt++;
				if (vdc_ch->status.correct_pic_search_cnt > VDC_CORRECT_PIC_SEARCHING_THRESHOLD) {
					vlog_error( "give up searching I-Pic with no macro block error\n" );
					vdc_ch->status.flush_age = vdc_ch->config.flush_age;

					___vdc_debug_q_status( VLOG_ERROR, vdc_ch );
					vdc_ch->debug.reported_pic_cnt = 0;
					vdc_ch->debug.rotated_pic_cnt= 0;

					vcodec_rate_reset_timestamp( vdc_ch->status.vdc_rate_id );
				}
				else {
					vlog_warning( "discard frame(0x%X) with error mb(%u) and pic type(%u)\n",
								vcore_report->info.done.info.pic.dpb_addr, vcore_report->info.done.info.pic.err_mbs, vcore_report->info.done.info.pic.pic_type );

					vdc_dq_push_clear( vdc_ch->status.vdc_dq_id, dq_node );
					return false;
				}
			}
		}
		else {
			vlog_warning( "flush transition %u --> %u\n",
						vcore_report->info.done.info.pic.meta.flush_age, vdc_ch->config.flush_age );
		}
	}
	else {
		if (vcore_report->info.done.info.pic.err_mbs)
			vlog_warning( "display frame(0x%X) with error mb(%u)\n",
						vcore_report->info.done.info.pic.dpb_addr, vcore_report->info.done.info.pic.err_mbs );
	}

	dts_rate = vcodec_rate_update_timestamp_input( vdc_ch->status.vdc_rate_id, vcore_report->info.done.info.pic.timestamp_dts );
	pts_rate = vcodec_rate_update_timestamp_output( vdc_ch->status.vdc_rate_id, vcore_report->info.done.info.pic.meta.timestamp );
	vdc_ch->debug.timestamp_rate = pts_rate;
	if (pts_rate) {
		report_dec->u.pic.timestamp = vcore_report->info.done.info.pic.meta.timestamp;
	}
	else if (dts_rate) {
		vlog_warning( "select dts(%lu) - pts(%lu)\n", dts_rate, pts_rate );
		report_dec->u.pic.timestamp = vcore_report->info.done.info.pic.timestamp_dts;
	}
	else {
		vlog_warning( "no valid dts(%lu), pts(%lu) rate\n", dts_rate, pts_rate );
		report_dec->u.pic.timestamp = vcore_report->info.done.info.pic.meta.timestamp;
	}

	report_dec->u.pic.sid = vcore_report->info.done.info.pic.sid;
	report_dec->u.pic.pic_width = vcore_report->info.done.info.pic.pic_width;
	report_dec->u.pic.pic_height = vcore_report->info.done.info.pic.pic_height;
	report_dec->u.pic.aspect_ratio = vcore_report->info.done.info.pic.aspect_ratio;
	report_dec->u.pic.err_mbs = vcore_report->info.done.info.pic.err_mbs;
	report_dec->u.pic.pic_type = vcore_report->info.done.info.pic.pic_type;
	report_dec->u.pic.crop.left = vcore_report->info.done.info.pic.crop.left;
	report_dec->u.pic.crop.right = vcore_report->info.done.info.pic.crop.right;
	report_dec->u.pic.crop.top = vcore_report->info.done.info.pic.crop.top;
	report_dec->u.pic.crop.bottom = vcore_report->info.done.info.pic.crop.bottom;
	report_dec->u.pic.frame_rate.residual = vcore_report->info.done.info.pic.frame_rate.residual;
	report_dec->u.pic.frame_rate.divider = vcore_report->info.done.info.pic.frame_rate.divider;
	report_dec->u.pic.uid = vcore_report->info.done.info.pic.meta.uid;
	report_dec->u.pic.input_time = vcore_report->info.done.info.pic.meta.input_time;
	report_dec->u.pic.feed_time = vcore_report->info.done.info.pic.meta.feed_time;
	report_dec->u.pic.decode_time = jiffies;

	report_dec->u.pic.dpb_addr = vcore_report->info.done.info.pic.dpb_addr;
	report_dec->u.pic.private_out_meta = dq_node->private_out_meta;
	report_dec->u.pic.rotation_angle = vcore_report->info.done.info.pic.meta.rotation_angle;

	vlog_print( VLOG_VDEC_VDC, "dpb addr:0x%X, resolution:%u*%u, pic type:%d, time stamp:%llu, chunk id:%u\n",
			vcore_report->info.done.info.pic.dpb_addr, vcore_report->info.done.info.pic.pic_width, vcore_report->info.done.info.pic.pic_height,
			vcore_report->info.done.info.pic.pic_type, vcore_report->info.done.info.pic.meta.timestamp, vcore_report->info.done.info.pic.meta.chunk_id );

	if (vdc_dq_push_displaying( vdc_ch->status.vdc_dq_id, dq_node ) == false) {
		vlog_error( "failed to push dq, dpb addr:0x%X\n", dq_node->dpb_addr );
		vdc_dq_debug_print_dpb_addr( vdc_ch->status.vdc_dq_id );
		return false;
	}

	vdc_ch->debug.reported_pic_cnt++;
	if (vcore_report->info.done.info.pic.meta.rotation_angle != VCORE_DEC_ROTATE_0){
		vdc_ch->debug.rotated_pic_cnt++;
	}
	return true;
}

static void _vdc_wq_vcore_report(struct work_struct *work)
{
	struct vdc_vcore_report_work *wk = container_of(work, struct vdc_vcore_report_work, vcore_report_work);
	struct vdc_channel *vdc_ch = wk->vdc_ch;
	struct vcore_dec_report *vcore_report = &wk->vcore_report;
	struct vdc_report_dec report_dec;

	report_dec.hdr = VDC_REPORT_INVALID;

	mutex_lock( &vdc_ch->lock );

	vlog_print( VLOG_VDEC_VDC, "reset_state:%u, vcore_complete:%u, need_more_au:%u, info_hdr:%u\n",
				vcore_report->info.reset, vcore_report->info.done.vcore_complete, vcore_report->info.done.need_more_au, vcore_report->info.done.hdr );

	if(IS_ERR_OR_NULL(vdc_ch)){
		vlog_error("vdc_ch is NULL\n");
		return;
	}

	switch (vcore_report->hdr) {
	case VCORE_DEC_RESET :
		switch (vcore_report->info.reset) {
		case VCORE_DEC_REPORT_RESET_START :
			vlog_error( "reset start\n" );
			___vdc_debug_q_status( VLOG_ERROR, vdc_ch );
			vdc_ch->reset_state = VDC_RESETTING;
			vdc_ch->vcore_state = VDC_RUNNING;
			goto _vdc_wq_vcore_report_exit;
			break;
		case VCORE_DEC_REPORT_RESET_END :
			vlog_error( "reset end - reset_state:%u, vcore_complete:%u, need_more_au:%u, info_hdr:%u\n",
					vcore_report->info.reset, vcore_report->info.done.vcore_complete, vcore_report->info.done.need_more_au, vcore_report->info.done.hdr );

/*			_vdc_flush_auq( vdc_ch );*/

			report_dec.hdr = VDC_UPDATE_SEQHDR;
			__vdc_pack_seqhdr( vdc_ch, &report_dec );

			vdc_ch->reset_state = VDC_NORMAL;
			vdc_ch->vcore_state = VDC_IDLE;
			break;
		default :
			vlog_error("vcore_report->info.reset : %d\n", vcore_report->info.reset);
			break;
		}
		vdc_ch->status.remaining_data_in_es = false;
		break;
	case VCORE_DEC_DONE :
		if (vcore_report->info.done.vcore_complete == VCORE_TRUE) {
			switch ( vdc_ch->vcore_state ) {
			case VDC_IDLE :
				vlog_trace("idle - reset_state:%u, vcore_complete:%u, need_more_au:%u, info_hdr:%u\n",
							vcore_report->info.reset, vcore_report->info.done.vcore_complete, vcore_report->info.done.need_more_au, vcore_report->info.done.hdr );
				break;
			case VDC_RUNNING_FEED :
			case VDC_RUNNING :
				if (vcore_report->info.done.remaining_data_in_es == VCORE_FALSE) {
					unsigned int num_of_au_per_1pic = __vdc_report_used_au( vdc_ch );
					if (vdc_ch->status.num_of_au_per_1pic && num_of_au_per_1pic)
						vdc_ch->status.num_of_au_per_1pic = num_of_au_per_1pic;
				}

				__vdc_feed_register_dpb( vdc_ch );
				__vdc_feed_clear_dpb( vdc_ch );

				vdc_ch->vcore_state = VDC_IDLE;
				break;
			default :
				vlog_error("vcore_state:%d\n", vdc_ch->vcore_state);
				break;
			}

			switch ( vdc_ch->close_state ) {
			case VDC_OPEN:
				vcodec_update_timer_status(vdc_ch->status.vdc_timer_id);
				break;
			case VDC_CLOSING:
				_vdc_flush_auq( vdc_ch );
				vlog_warning("completion of vcore close processing\n");
				complete_all(&vdc_ch->vcore_completion);
				vdc_ch->close_state = VDC_CLOSED;
				break;
			case VDC_CLOSED:
				vlog_error("already closed state\n");
				break;
			default:
				vlog_error("close_state:%d\n", vdc_ch->close_state);
				break;
			}
		}
		else if (vcore_report->info.done.need_more_au == VCORE_TRUE) {
			vdc_ch->vcore_state = VDC_RUNNING_FEED;
		}
		vdc_ch->status.remaining_data_in_es =
			(vcore_report->info.done.remaining_data_in_es == VCORE_TRUE) ? true : false;

		switch (vcore_report->info.done.hdr) {
		case VCORE_DEC_REPORT_SEQ :
			if (__vdc_parse_seq_info(vdc_ch, vcore_report, &report_dec) == true) {
				report_dec.hdr = VDC_REPORT_DEC_SEQ;
			}
			else {
				/*
					When the first keyframe has been corrupted, vender can discard video from frames 0-149
					The number 150 is specified in the Divx certification document (5.8.2.2)
					160 = 150 (picture) + 10 (margin for header data)
				*/
				if (++vdc_ch->seq_info.dec_fail_cnt % 160 == 0) {
					vlog_error("sequence init fail (cnt:%d)\n", vdc_ch->seq_info.dec_fail_cnt);
					report_dec.hdr = VDC_REPORT_DEC_SEQ;
					report_dec.u.seq.success = false;
				}
				else {
					vcore_report->info.done.hdr = VCORE_DEC_REPORT_NONE;
					vlog_trace("retry to decode sequence header (cnt:%d)\n", vdc_ch->seq_info.dec_fail_cnt);
				}
			}
			break;
		case VCORE_DEC_REPORT_SEQ_CHANGE :
			vlog_trace( "sequence change\n" );
			report_dec.hdr  = VDC_REPORT_DEC_SEQ_CHANGE;

			_vdc_reset(vdc_ch);
			break;
		case VCORE_DEC_REPORT_PIC :
			if (__vdc_parse_pic_info(vdc_ch, vcore_report, &report_dec) == true) {
				report_dec.hdr = VDC_REPORT_DEC_PIC;
			}
			break;
		case VCORE_DEC_REPORT_EOS :
			report_dec.hdr = VDC_REPORT_DEC_EOS;
			vlog_trace( "end of stream\n" );
			break;
		case VCORE_DEC_REPORT_FLUSH_DONE:
			vlog_trace( "flushed\n" );
			report_dec.hdr = VDC_REPORT_FLUSH_DONE;
			switch( vdc_ch->flush_state )
			{
			case VDC_SENT_FLUSH_CMD_TO_VCORE:
				_vdc_flush_exit_flush_state( vdc_ch );
				vdc_ch->flush_state = VDC_FLUSHED;
				break;
			case VDC_FLUSHED:
			case VDC_RECEIVED_FLUSH_FROM_TOP:
			default :
				vlog_error("flush_state:%d\n", vdc_ch->flush_state);
				break;
			}
			break;
		case VCORE_DEC_REPORT_NONE:
			break;
		default :
			vlog_error( "vcore_report->info.done.hdr:%u\n", vcore_report->info.done.hdr);
		}

		switch (vcore_report->info.done.hdr) {
		case VCORE_DEC_REPORT_SEQ :
		case VCORE_DEC_REPORT_SEQ_CHANGE :
		case VCORE_DEC_REPORT_FLUSH_DONE:
			break;
		case VCORE_DEC_REPORT_PIC :
		case VCORE_DEC_REPORT_EOS :
		case VCORE_DEC_REPORT_NONE:
			if (vdc_ch->status.total_dpb_num <= vdc_ch->seq_info.ref_frame_cnt) {
				if ((vdc_ch->status.num_of_dq_free_node) && (vcore_report->info.done.need_more_au == VCORE_FALSE))
					vdc_ch->status.num_of_dq_free_node--;
				else
					vlog_warning( "not enough free dpb\n");
			}
			else {
				__vdc_calculate_num_of_free_dpb(vdc_ch);
			}
			break;
		default :
			vlog_error( "vcore_report->info.done.hdr:%u\n", vcore_report->info.done.hdr);
		}

		break;
	case VCORE_DEC_FEED :
		__vdc_feed_check_state( vdc_ch );
		goto _vdc_wq_vcore_report_exit;
		break;
	default :
		vlog_error("vcore_report->hdr : %d\n", vcore_report->hdr);
		break;
	}

_vdc_wq_vcore_report_exit:
	mutex_unlock( &vdc_ch->lock );

	if (report_dec.hdr != VDC_REPORT_INVALID) {
		if (vdc_ch->config.top_report_dec) {
			vdc_ch->config.top_report_dec( vdc_ch->config.top_id, &report_dec );
		}
		else {
			vlog_error( "no callback path for report(%u)\n", report_dec.hdr );
		}
	}

	kfree(wk);
}

static void _vdc_cb_vcore_report( void *vdc_id, struct vcore_dec_report *vcore_report )
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;
	struct vdc_vcore_report_work *wk;

	if (!vdc_ch) {
		vlog_error("vdc_ch is NULL\n");
		return;
	}
	else if (vdc_ch->close_state == VDC_CLOSED) {
		vlog_error("closed state\n");
		return;
	}

	if (vcore_report->hdr == VCORE_DEC_DONE)
		if (vcore_report->info.done.hdr == VCORE_DEC_REPORT_PIC) {
			struct vdc_dq_node *dq_node = NULL;

			dq_node = vdc_dq_pop_vcore( vdc_ch->status.vdc_dq_id, vcore_report->info.done.info.pic.dpb_addr );
			if (dq_node == NULL) {
				vlog_error( "failed to pop dq, dpb_addr:0x%X\n", vcore_report->info.done.info.pic.dpb_addr );
		//			vdc_dq_debug_print_dpb_addr( vdc_ch->status.vdc_dq_id );
			}
			else {
				vdc_dq_push_decoded( vdc_ch->status.vdc_dq_id, dq_node );
			}
		}

	wk = kzalloc(sizeof(struct vdc_vcore_report_work), GFP_ATOMIC);
	if (!wk) {
		vlog_error("failed to alloc buf for callback data\n");
		return;
	}
	wk->vcore_report = *vcore_report;
	wk->vdc_ch = vdc_ch;
	INIT_WORK(&wk->vcore_report_work, _vdc_wq_vcore_report);

	queue_work(vdc_ch->vcore_report_wq, &wk->vcore_report_work);
}

static bool _vdc_check_vcore_status(void *vdc_id)
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;
	bool running = true;

	mutex_lock( &vdc_ch->lock );

	switch (vdc_ch->vcore_state) {
	case VDC_IDLE:
		running = false;
		break;
	case VDC_RUNNING_FEED:
	case VDC_RUNNING:
		running = true;
		break;
	default:
		vlog_error("vcore_state:%d\n", vdc_ch->vcore_state);
		break;
	}

	mutex_unlock( &vdc_ch->lock );

	/* debug message */
	___vdc_debug_q_status( VLOG_VDEC_MONITOR, vdc_ch );

	return running;
}

static void _vdc_watchdog_reset(void *vdc_id)
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;

	mutex_lock( &vdc_ch->lock );

	___vdc_debug_q_status( VLOG_ERROR, vdc_ch );

	vdc_ch->reset_state = VDC_RESETTING;
	vdc_ch->vcore_state = VDC_RUNNING;
	vdc_ch->vcore_ops.reset( vdc_ch->status.vcore_id );

	mutex_unlock( &vdc_ch->lock );
}

void *vdc_open( struct vcore_dec_ops *vcore_ops,
					enum vcore_dec_codec codec_type,
					unsigned int cpb_phy_addr, unsigned char *cpb_vir_ptr,
					unsigned int cpb_size,
					bool reordering,
					bool rendering_dpb,
					bool secure_buf,
					void *top_id,
					void (*top_used_au)( void *top_id, struct vdc_report_au *report_au ),
					void (*top_report_dec)( void *top_id, struct vdc_report_dec *report_dec ) )
{
	struct vdc_channel *vdc_ch;
	enum vcore_dec_ret vcore_ret = VCORE_DEC_SUCCESS;
	unsigned int retry_count = 0;

	vlog_info( "codec :%u, cpb:0x%X++0x%X vaddr(0x%X), sec %d, rod %d\n",
			codec_type, cpb_phy_addr, cpb_size, (unsigned int)cpb_vir_ptr,
			secure_buf, reordering);

	vdc_ch = (struct vdc_channel *)vzalloc( sizeof( struct vdc_channel ) );
	if (!vdc_ch) {
		vlog_error( "failed to allocate memory\n" );
		return (void *)NULL;
	}

	mutex_init( &vdc_ch->lock );
	mutex_lock( &vdc_ch->lock );

	vdc_ch->status.vdc_auq_id = vdc_auq_open();
	if (!vdc_ch->status.vdc_auq_id) {
		vlog_error( "failed to get vdc_auq_id\n" );
		mutex_unlock( &vdc_ch->lock );
		mutex_destroy( &vdc_ch->lock );
		vfree( vdc_ch );
		return (void *)NULL;
	}

	vdc_ch->status.vdc_dq_id = vdc_dq_open( VDEC_MAX_NUM_OF_DPB );
	if (!vdc_ch->status.vdc_dq_id) {
		vlog_error( "failed to get vdc_dq_id\n" );
		vdc_auq_close( vdc_ch->status.vdc_auq_id );
		mutex_unlock( &vdc_ch->lock );
		mutex_destroy( &vdc_ch->lock );
		vfree( vdc_ch );
		return (void *)NULL;
	}

	vdc_ch->status.vdc_rate_id = vcodec_rate_open();
	if (!vdc_ch->status.vdc_rate_id) {
		vlog_error( "failed to get vdc_rate_id\n" );
		vdc_auq_close( vdc_ch->status.vdc_auq_id );
		vdc_dq_close( vdc_ch->status.vdc_dq_id );
		mutex_unlock( &vdc_ch->lock );
		mutex_destroy( &vdc_ch->lock );
		vfree( vdc_ch );
		return (void *)NULL;
	}

	vdc_ch->status.vdc_timer_id = vcodec_timer_open( (void *)vdc_ch,
		_vdc_check_vcore_status, _vdc_watchdog_reset );
	if (!vdc_ch->status.vdc_timer_id) {
		vlog_error( "failed to get vdc_timer_id\n" );
		vdc_auq_close( vdc_ch->status.vdc_auq_id );
		vdc_dq_close( vdc_ch->status.vdc_dq_id );
		vcodec_rate_close( vdc_ch->status.vdc_rate_id );
		mutex_unlock( &vdc_ch->lock );
		mutex_destroy( &vdc_ch->lock );
		vfree( vdc_ch );
		return (void *)NULL;
	}

	vdc_ch->config.codec_type = codec_type;
	vdc_ch->config.cpb_phy_addr = cpb_phy_addr;
	vdc_ch->config.cpb_vir_ptr = cpb_vir_ptr;
	vdc_ch->config.cpb_size = cpb_size;
	vdc_ch->config.reordering = reordering;
	vdc_ch->config.rendering_dpb = rendering_dpb;
	vdc_ch->config.flush_age = (unsigned int)jiffies;
	vdc_ch->config.top_id = top_id;
	vdc_ch->config.top_used_au = top_used_au;
	vdc_ch->config.top_report_dec = top_report_dec;
	vdc_ch->config.secure_buf = secure_buf;

	vdc_ch->status.last_au_end_addr = cpb_phy_addr;

	vdc_ch->status.flush_age = vdc_ch->config.flush_age - 1;
	vdc_ch->status.chunk_id = 0;
	vdc_ch->status.correct_pic_search_cnt = 0;
	vdc_ch->status.vcore_update_buffer_au_failed_cnt = 0;

	vlog_trace( "rendering_dpb:%d, flush age - config:%u, status:%u\n", rendering_dpb, vdc_ch->config.flush_age, vdc_ch->status.flush_age );

	vdc_ch->status.remaining_data_in_es = false;
	vdc_ch->status.num_of_auq_alive_node = 0;
	vdc_ch->status.num_of_dq_alive_node = 0;
	vdc_ch->status.num_of_dq_free_node = 0;

	vdc_ch->status.num_of_au_per_1pic = 1;

	vdc_ch->seq_info.init_done = false;
	vdc_ch->seq_info.over_spec = false;
	vdc_ch->seq_info.dec_fail_cnt = 0;
	vdc_ch->seq_info.ref_frame_cnt = 0;
	vdc_ch->seq_info.seq_phy_addr = 0x0;
	vdc_ch->seq_info.seq_vir_ptr = NULL;
	vdc_ch->seq_info.seq_size = 0;

	vdc_ch->debug.received_cnt = 0;
	vdc_ch->debug.sent_cnt = 0;
	vdc_ch->debug.reported_au_cnt = 0;
	vdc_ch->debug.reported_pic_cnt = 0;

	vdc_ch->debug.input_rate = (unsigned long)(-1);
	vdc_ch->debug.decode_rate = (unsigned long)(-1);
	vdc_ch->debug.timestamp_rate = (unsigned long)(-1);

	vdc_ch->debug.request_feed_log = 0;

	vdc_ch->vcore_report_wq = create_singlethread_workqueue( "vdc_vcore_report_wq" );
	vdc_ch->vcore_au.meta.rotation_angle = VCORE_DEC_ROTATE_0;

	vdc_ch->vcore_ops = *vcore_ops;

	do {
		vcore_ret = vdc_ch->vcore_ops.open(&vdc_ch->status.vcore_id,
								codec_type, cpb_phy_addr,
								cpb_vir_ptr, cpb_size,
								(reordering == true) ? VCORE_TRUE : VCORE_FALSE,
								(rendering_dpb == true) ? VCORE_TRUE : VCORE_FALSE,
								(secure_buf == true) ? VCORE_TRUE : VCORE_FALSE,
								vdc_ch, _vdc_cb_vcore_report);
		if (vcore_ret != VCORE_DEC_RETRY)
			break;

		retry_count++;
		if ((retry_count % 1000) == 0)
			vlog_print( VLOG_MULTI_INSTANCE, "retry: %u\n", retry_count );

	} while (retry_count < VDC_MAX_SCHEDULE_RETRY);

	if (retry_count) {
		vlog_trace( "retry: %u\n", retry_count );
		if (retry_count == VDC_MAX_SCHEDULE_RETRY)
			vlog_error( "retry: %u\n", retry_count );
	}

	if (!vdc_ch->status.vcore_id) {
		vlog_error( "failed to get vcore_id\n" );
		mutex_unlock( &vdc_ch->lock );
		vdc_auq_close( vdc_ch->status.vdc_auq_id );
		vdc_dq_close( vdc_ch->status.vdc_dq_id );
		vcodec_rate_close( vdc_ch->status.vdc_rate_id );
		vcodec_timer_close( vdc_ch->status.vdc_timer_id );
		vfree( vdc_ch );
		return (void *)NULL;
	}

	init_completion(&vdc_ch->vcore_completion);

	vdc_ch->reset_state = VDC_NORMAL;
	vdc_ch->close_state = VDC_OPEN;
	vdc_ch->flush_state = VDC_FLUSHED;
	vdc_ch->pause_state = VDC_PLAYING;
	vdc_ch->vcore_state = VDC_IDLE;

	vdc_ch->timer_cnt = 0;

	list_add_tail( &vdc_ch->list, &vdc_ch_list );

	mutex_unlock( &vdc_ch->lock );

	vlog_trace("success - vdc:0x%X, vcore:0x%X\n", (unsigned int)vdc_ch, (unsigned int)vdc_ch->status.vcore_id);

	return (void *)vdc_ch;
}

void vdc_close( void *vdc_id )
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;
	unsigned int retry_count = 0;
	enum vcore_dec_ret vcore_ret;

	mutex_lock( &vdc_ch->lock );
	vlog_trace( "start, receive/send/report:%u/%u/%u\n",
				vdc_ch->debug.received_cnt, vdc_ch->debug.sent_cnt, vdc_ch->debug.reported_au_cnt );

	switch (vdc_ch->vcore_state) {
	case VDC_IDLE:
		_vdc_flush_auq( vdc_ch );
		vdc_ch->close_state = VDC_CLOSED;
		break;
	case VDC_RUNNING_FEED:
	case VDC_RUNNING:
		vlog_warning("vcore decoding\n");
		vdc_ch->close_state = VDC_CLOSING;
		break;
	default:
		vlog_error("vcore_state:%d\n", vdc_ch->vcore_state);
		break;
	}

	list_del( &vdc_ch->list );

	if (vdc_ch->close_state == VDC_CLOSING) {
		vlog_warning("waiting for completion of vcore processing\n");
		if (wait_for_completion_interruptible_timeout(&vdc_ch->vcore_completion, HZ / 3) <= 0) {
			vlog_error("error waiting for reply to vcore done - close_state:%d\n", vdc_ch->close_state);
		}
		vdc_ch->close_state = VDC_CLOSED;
	}
	mutex_unlock( &vdc_ch->lock );

	vcodec_timer_close( vdc_ch->status.vdc_timer_id );

	mutex_lock( &vdc_ch->lock );
	do {
		vcore_ret = vdc_ch->vcore_ops.close( vdc_ch->status.vcore_id );
		if (vcore_ret != VCORE_DEC_RETRY)
			break;

		retry_count++;
		if ((retry_count % 1000) == 0)
			vlog_print( VLOG_MULTI_INSTANCE, "retry: %u\n", retry_count );

	} while (retry_count < VDC_MAX_SCHEDULE_RETRY);

	if (retry_count) {
		vlog_trace( "retry: %u\n", retry_count );
		if (retry_count == VDC_MAX_SCHEDULE_RETRY)
			vlog_error( "retry: %u\n", retry_count );
	}

	drain_workqueue(vdc_ch->vcore_report_wq);
	destroy_workqueue(vdc_ch->vcore_report_wq);

	vcodec_rate_close( vdc_ch->status.vdc_rate_id );
	vdc_dq_close( vdc_ch->status.vdc_dq_id );
	vdc_auq_close( vdc_ch->status.vdc_auq_id );

	mutex_unlock( &vdc_ch->lock );
	mutex_destroy( &vdc_ch->lock );

	vfree( vdc_ch );

	vlog_trace( "end\n" );
}

void vdc_pause( void *vdc_id )
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;

	mutex_lock( &vdc_ch->lock );

	vlog_trace("reset_state:%d, close_state:%d, flush_state:%d, pause_state:%d\n",
				vdc_ch->reset_state, vdc_ch->close_state, vdc_ch->flush_state, vdc_ch->pause_state);

	if (vdc_ch->pause_state == VDC_PAUSED)
		vlog_trace("already paused state\n");

	vdc_ch->pause_state = VDC_PAUSED;

	mutex_unlock( &vdc_ch->lock );
}

void vdc_resume( void *vdc_id )
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;

	mutex_lock( &vdc_ch->lock );

	vlog_trace("reset_state:%d, close_state:%d, flush_state:%d, pause_state:%d\n",
				vdc_ch->reset_state, vdc_ch->close_state, vdc_ch->flush_state, vdc_ch->pause_state);

	if (vdc_ch->pause_state == VDC_PLAYING)
		vlog_trace("already playing state\n");

	vdc_ch->pause_state = VDC_PLAYING;

	__vdc_feed_check_state( vdc_ch );

	mutex_unlock( &vdc_ch->lock );
}

void vdc_flush( void *vdc_id )
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;

	mutex_lock( &vdc_ch->lock );

	vdc_ch->config.flush_age++;
	vlog_trace( "flush age:%u\n", vdc_ch->config.flush_age );

	___vdc_debug_q_status( VLOG_TRACE, vdc_ch );
	vdc_ch->debug.received_cnt = 0;
	vcodec_rate_reset_input( vdc_ch->status.vdc_rate_id );

	vdc_ch->status.chunk_id = 0;

	_vdc_flush_enter_flush_state( vdc_ch );
	switch( vdc_ch->flush_state )
	{
	case VDC_FLUSHED:
		vdc_ch->flush_state = VDC_RECEIVED_FLUSH_FROM_TOP;

		switch (vdc_ch->vcore_state) {
		case VDC_IDLE:
			__vdc_feed_check_state( vdc_ch );
			break;
		case VDC_RUNNING_FEED:
		case VDC_RUNNING:
			vlog_trace("vcore decoding\n");
			break;
		default:
			vlog_error("vcore_state:%d\n", vdc_ch->vcore_state);
			break;
		}
		break;

	case VDC_RECEIVED_FLUSH_FROM_TOP:
		vlog_trace("already received flush\n");
		break;
	case VDC_SENT_FLUSH_CMD_TO_VCORE:
		vlog_trace("flushing\n");
		break;
	default :
		vlog_error("flush_state:%d\n", vdc_ch->flush_state);
		break;
	}

	mutex_unlock( &vdc_ch->lock );
}

bool vdc_update_compressed_buffer( void *vdc_id, struct vdc_update_au *update_au )
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;
	struct vdc_auq_node auq_node;
	bool ret;

	mutex_lock( &vdc_ch->lock );

	if (update_au->eos == true) {
		vlog_trace( "end of stream, au q occupancy:%u\n", vdc_auq_occupancy( vdc_ch->status.vdc_auq_id ) );
		vdc_ch->status.num_of_au_per_1pic = 0;
	}

	auq_node.p_buf = update_au->p_buf;
	auq_node.type = update_au->type;
	auq_node.start_phy_addr = update_au->start_phy_addr;
	auq_node.start_vir_ptr = update_au->start_vir_ptr;
	auq_node.size = update_au->size;
	auq_node.end_phy_addr = update_au->end_phy_addr;
	auq_node.private_in_meta = update_au->private_in_meta;
	auq_node.eos = update_au->eos;
	auq_node.timestamp = update_au->timestamp;
	auq_node.uid = update_au->uid;
	auq_node.input_time = update_au->input_time;
	auq_node.flush_age = vdc_ch->config.flush_age;
	auq_node.chunk_id = vdc_ch->status.chunk_id;

	vlog_print( VLOG_VDEC_VDC, "au type:%d, addr:0x%X/0x%X++0x%X=0x%X, time stamp:%llu, flush_age:%u, chunk id:%u\n",
			auq_node.type, auq_node.start_phy_addr, (unsigned int)auq_node.start_vir_ptr, auq_node.size, auq_node.end_phy_addr,
			auq_node.timestamp, auq_node.flush_age, auq_node.chunk_id );

	ret = vdc_auq_push_waiting( vdc_ch->status.vdc_auq_id, &auq_node );
	if (ret == false)
		vlog_error("vdc_auq_push_waiting error\n");

	vdc_ch->status.chunk_id++;

	vdc_ch->debug.input_rate = vcodec_rate_update_input( vdc_ch->status.vdc_rate_id, jiffies );
	vdc_ch->debug.received_cnt++;

	__vdc_feed_check_state( vdc_ch );

	mutex_unlock( &vdc_ch->lock );

	return ret;
}

bool vdc_init_decompressed_buffer( void *vdc_id,
										struct vcore_dec_init_dpb *init_dpb)
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;
	bool ret = false;
	enum vcore_dec_ret vcore_ret;
	unsigned int retry_count = 0;

	mutex_lock( &vdc_ch->lock );
	vlog_info( "ref_frame_cnt:%u\n", vdc_ch->seq_info.ref_frame_cnt );

	if (vdc_ch->seq_info.init_done == false) {
		vlog_error( "not initialised seq\n" );
		mutex_unlock( &vdc_ch->lock );
		return false;
	}

	do {
		vcore_ret = vdc_ch->vcore_ops.init_dpb( vdc_ch->status.vcore_id,
										init_dpb);
		if (vcore_ret != VCORE_DEC_RETRY)
			break;

		retry_count++;
		if ((retry_count % 1000) == 0)
			vlog_print( VLOG_MULTI_INSTANCE, "retry: %u\n", retry_count );

	} while (retry_count < VDC_MAX_SCHEDULE_RETRY);

	if (retry_count) {
		vlog_trace( "retry: %u\n", retry_count );
		if (retry_count == VDC_MAX_SCHEDULE_RETRY)
			vlog_error( "retry: %u\n", retry_count );
	}

	if (vcore_ret == VCORE_DEC_SUCCESS ) {
		vdc_ch->register_dpb_num = 0;
		vdc_ch->unregister_dpb_num = 0;

		vdc_ch->status.total_dpb_num = 0;
		vdc_ch->status.num_of_dq_free_node = 0;
		ret = true;
	}
	else {
		vlog_error( "failed to initialise\n" );
	}

	mutex_unlock( &vdc_ch->lock );

	return ret;
}

bool vdc_register_decompressed_buffer( void *vdc_id, struct vcore_dec_dpb *dpb )
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;
	enum vcore_dec_ret vcore_ret;
	unsigned int retry_count = 0;
	struct vdc_dq_node *dq_node;

	mutex_lock( &vdc_ch->lock );

	vlog_trace( "sid:%u/%u buf:%ux%u\n", vdc_ch->seq_info.sid, dpb->sid, dpb->buf_width, dpb->buf_height );
	___vdc_debug_q_status( VLOG_TRACE, vdc_ch );

	if (vdc_ch->seq_info.init_done == false) {
		vlog_error( "not initialised seq\n" );
		mutex_unlock( &vdc_ch->lock );
		return false;
	}

	if (vdc_ch->seq_info.sid > dpb->sid) {
		vlog_error( "old sequence id\n" );
		mutex_unlock( &vdc_ch->lock );
		return false;
	}

	memcpy(&vdc_ch->dpb[vdc_ch->unregister_dpb_num], dpb, sizeof(struct vcore_dec_dpb));
	vdc_ch->unregister_dpb_num++;

	vdc_ch->status.total_dpb_num++;

	dq_node = vdc_dq_get_free_node( vdc_ch->status.vdc_dq_id );
	dq_node->dpb_addr = dpb->external_dbp_addr;
	if (vdc_ch->config.rendering_dpb == true)
		vdc_dq_push_displaying( vdc_ch->status.vdc_dq_id, dq_node );
	else
		vdc_dq_push_vcore( vdc_ch->status.vdc_dq_id, dq_node );

	mutex_unlock( &vdc_ch->lock );

	return true;
}

bool vdc_clear_decompressed_buffer( void *vdc_id,
									unsigned int sid,
									unsigned int dpb_addr,
									void *private_out_meta,
									enum vcore_dec_rotate rotation_angle)
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;
	struct vdc_dq_node *dq_node;
	bool ret;

	mutex_lock( &vdc_ch->lock );

	if (vdc_ch->seq_info.init_done == false) {
		vlog_error( "not initialised seq\n" );
		mutex_unlock( &vdc_ch->lock );
		return false;
	}

	if (vdc_ch->status.total_dpb_num == 0) {
		vlog_error( "not registered, dpb_addr:0x%X\n", dpb_addr );
		mutex_unlock( &vdc_ch->lock );
		return false;
	}

	if (vdc_ch->seq_info.sid > sid) {
		vlog_error( "old sequence id\n" );
		mutex_unlock( &vdc_ch->lock );
		return false;
	}

	dq_node = vdc_dq_pop_displaying( vdc_ch->status.vdc_dq_id, dpb_addr );
	if (dq_node == NULL) {
		vlog_error( "failed to pop dq, dpb_addr:0x%X\n", dpb_addr );
		vdc_dq_debug_print_dpb_addr( vdc_ch->status.vdc_dq_id );
		mutex_unlock( &vdc_ch->lock );
		return false;
	}

	dq_node->private_out_meta = private_out_meta;
	dq_node->sid = sid;

	ret = vdc_dq_push_clear( vdc_ch->status.vdc_dq_id, dq_node );

	if (vdc_ch->status.total_dpb_num <= vdc_ch->seq_info.ref_frame_cnt) {
		vdc_ch->status.num_of_dq_free_node++;
	}
	else {
		__vdc_calculate_num_of_free_dpb(vdc_ch);
	}

	vdc_ch->vcore_au.meta.rotation_angle = rotation_angle;

	__vdc_feed_check_state( vdc_ch );

	mutex_unlock( &vdc_ch->lock );

	return ret;
}

unsigned int vdc_get_auq_occupancy( void *vdc_id )
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;

	return vdc_auq_occupancy( vdc_ch->status.vdc_auq_id );
}

unsigned int vdc_get_dq_occupancy( void *vdc_id )
{
	struct vdc_channel *vdc_ch = (struct vdc_channel *)vdc_id;

	return vdc_dq_displaying_occupancy( vdc_ch->status.vdc_dq_id );
}

void vdc_init( void )
{
	INIT_LIST_HEAD( &vdc_ch_list );

	vdc_auq_init();
	vdc_dq_init();
}

void vdc_cleanup( void )
{
	vdc_auq_cleanup();
	vdc_dq_cleanup();
}

