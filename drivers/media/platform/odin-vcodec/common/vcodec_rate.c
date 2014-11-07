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

#include "vcodec_rate.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <asm-generic/param.h>
#include <linux/mutex.h>
#include <asm/div64.h>

#include <media/odin/vcodec/vlog.h>

#define	VCODEC_RATE_INVALID_VALUE	0x80000000
#define	VCODEC_RATE_FILTER_ORDER	0x100
#define	VCODEC_RATE_DIFF_MAX_NUM	(0xFFFFFFFF / VCODEC_RATE_FILTER_ORDER)

struct vcodec_rate_time_node
{
	struct list_head list;
	unsigned long event_diff_time;	/* jiffies */
};

struct vcodec_rate_average
{
	struct list_head time_list;
	unsigned long event_time_prev;	/* jiffies */
	unsigned long diff_time_sum;	/* jiffies */
	unsigned int list_cnt;
};

struct vcodec_rate_channel_node
{
	struct list_head list;

	struct vcodec_rate_average input;
	struct vcodec_rate_average decode;
	struct vcodec_rate_average timestamp_input;
	struct vcodec_rate_average timestamp_output;
	unsigned long long timestamp_input_prev;
	unsigned long long timestamp_output_prev;

	unsigned int timestamp_jitter_log;;
};

struct mutex vcodec_rate_lock;
struct list_head vcodec_rate_instance_list;

void vcodec_rate_init( void )
{
	mutex_init( &vcodec_rate_lock );
	INIT_LIST_HEAD( &vcodec_rate_instance_list );
}

void vcodec_rate_cleanup( void )
{
	mutex_destroy( &vcodec_rate_lock );
}

void *vcodec_rate_open( void )
{
	struct vcodec_rate_channel_node *vcodec_rate_instance;

	mutex_lock( &vcodec_rate_lock );

	vcodec_rate_instance =
			(struct vcodec_rate_channel_node *)
				vzalloc( sizeof(struct vcodec_rate_channel_node) );

	if (!vcodec_rate_instance) {
		vlog_error( "failed to allocate memory\n" );
		mutex_unlock( &vcodec_rate_lock );
		return (void *)NULL;
	}

	INIT_LIST_HEAD( &vcodec_rate_instance->input.time_list );
	vcodec_rate_instance->input.event_time_prev = VCODEC_RATE_INVALID_VALUE;
	vcodec_rate_instance->input.diff_time_sum = 0;
	vcodec_rate_instance->input.list_cnt = 0;
	INIT_LIST_HEAD( &vcodec_rate_instance->decode.time_list );
	vcodec_rate_instance->decode.event_time_prev = VCODEC_RATE_INVALID_VALUE;
	vcodec_rate_instance->decode.diff_time_sum = 0;
	vcodec_rate_instance->decode.list_cnt = 0;
	INIT_LIST_HEAD( &vcodec_rate_instance->timestamp_input.time_list );
	vcodec_rate_instance->timestamp_input.event_time_prev
		= VCODEC_RATE_INVALID_VALUE;
	vcodec_rate_instance->timestamp_input.diff_time_sum = 0;
	vcodec_rate_instance->timestamp_input.list_cnt = 0;
	INIT_LIST_HEAD( &vcodec_rate_instance->timestamp_output.time_list );
	vcodec_rate_instance->timestamp_output.event_time_prev
		= VCODEC_RATE_INVALID_VALUE;
	vcodec_rate_instance->timestamp_output.diff_time_sum = 0;
	vcodec_rate_instance->timestamp_output.list_cnt = 0;
	vcodec_rate_instance->timestamp_input_prev = 0xFFFFFFFFFFFFFFFFLL;
	vcodec_rate_instance->timestamp_output_prev = 0xFFFFFFFFFFFFFFFFLL;

	vcodec_rate_instance->timestamp_jitter_log = 0;

	list_add_tail( &vcodec_rate_instance->list, &vcodec_rate_instance_list );

	mutex_unlock( &vcodec_rate_lock );

	return (void *)vcodec_rate_instance;
}

void vcodec_rate_close( void *vcodec_rate_id )
{
	struct vcodec_rate_channel_node *vcodec_rate_instance
			= (struct vcodec_rate_channel_node *)vcodec_rate_id;
	struct vcodec_rate_time_node *time_node, *tmp;

	mutex_lock( &vcodec_rate_lock );

	list_for_each_entry_safe( time_node, tmp,
			&vcodec_rate_instance->input.time_list, list ) {
		list_del( &time_node->list );
		vfree( time_node );
	}
	list_for_each_entry_safe( time_node, tmp,
			&vcodec_rate_instance->decode.time_list, list ) {
		list_del( &time_node->list );
		vfree( time_node );
	}
	list_for_each_entry_safe( time_node, tmp,
			&vcodec_rate_instance->timestamp_input.time_list, list ) {
		list_del( &time_node->list );
		vfree( time_node );
	}
	list_for_each_entry_safe( time_node, tmp,
			&vcodec_rate_instance->timestamp_output.time_list, list ) {
		list_del( &time_node->list );
		vfree( time_node );
	}

	list_del( &vcodec_rate_instance->list );
	vfree( vcodec_rate_instance );

	mutex_unlock( &vcodec_rate_lock );
}

static void _vcodec_rate_reset_average(
								struct vcodec_rate_average *rate_average )
{
	struct vcodec_rate_time_node *time_node, *tmp;

	list_for_each_entry_safe( time_node, tmp, &rate_average->time_list, list ) {
		list_del( &time_node->list );
		vfree( time_node );
	}
	rate_average->event_time_prev = VCODEC_RATE_INVALID_VALUE;
	rate_average->diff_time_sum = 0;
	rate_average->list_cnt = 0;
}

void vcodec_rate_reset_input( void *vcodec_rate_id )
{
	struct vcodec_rate_channel_node *vcodec_rate_instance
		= (struct vcodec_rate_channel_node *)vcodec_rate_id;

	mutex_lock( &vcodec_rate_lock );

	_vcodec_rate_reset_average( &vcodec_rate_instance->input );

	mutex_unlock( &vcodec_rate_lock );
}

void vcodec_rate_reset_codec_done( void *vcodec_rate_id )
{
	struct vcodec_rate_channel_node *vcodec_rate_instance
		= (struct vcodec_rate_channel_node *)vcodec_rate_id;

	mutex_lock( &vcodec_rate_lock );

	_vcodec_rate_reset_average( &vcodec_rate_instance->decode );

	mutex_unlock( &vcodec_rate_lock );
}

void vcodec_rate_reset_timestamp( void *vcodec_rate_id )
{
	struct vcodec_rate_channel_node *vcodec_rate_instance
		= (struct vcodec_rate_channel_node *)vcodec_rate_id;

	mutex_lock( &vcodec_rate_lock );

	_vcodec_rate_reset_average( &vcodec_rate_instance->timestamp_input );
	_vcodec_rate_reset_average( &vcodec_rate_instance->timestamp_output );
	vcodec_rate_instance->timestamp_input_prev = 0xFFFFFFFFFFFFFFFFLL;
	vcodec_rate_instance->timestamp_output_prev = 0xFFFFFFFFFFFFFFFFLL;

	mutex_unlock( &vcodec_rate_lock );
}

static unsigned long _vcodec_rate_update_average(
		struct vcodec_rate_average *rate_average,
		unsigned long event_time)
{
	struct vcodec_rate_time_node *time_node;
	unsigned long event_rate;	/* 100 */

	event_time &= 0x7FFFFFFF;

	if (rate_average->event_time_prev == VCODEC_RATE_INVALID_VALUE) {
		rate_average->event_time_prev = event_time;
		return 0;
	}

	if (rate_average->list_cnt > VCODEC_RATE_FILTER_ORDER) {
		time_node = list_entry( rate_average->time_list.next,
								struct vcodec_rate_time_node,
								list );
		list_del( &time_node->list );
		rate_average->list_cnt--;
		rate_average->diff_time_sum -= time_node->event_diff_time;
	}
	else {
		time_node
			= (struct vcodec_rate_time_node *)
			vzalloc( sizeof(struct vcodec_rate_time_node) );
		if (!time_node) {
			vlog_error( "failed to allocate memory\n" );
			return 0;
		}
	}

	time_node->event_diff_time = (event_time >= rate_average->event_time_prev) ? \
							(event_time - rate_average->event_time_prev) : \
							(event_time + 0x80000000 - rate_average->event_time_prev);
	if (time_node->event_diff_time > VCODEC_RATE_DIFF_MAX_NUM) {
		time_node->event_diff_time = VCODEC_RATE_DIFF_MAX_NUM;
	}

	list_add_tail( &time_node->list, &rate_average->time_list );
	rate_average->list_cnt++;
	rate_average->diff_time_sum += time_node->event_diff_time;

	rate_average->event_time_prev = event_time;

	if (rate_average->diff_time_sum)
		event_rate =
			rate_average->list_cnt * (HZ) * 100 / rate_average->diff_time_sum;
	else
		event_rate = 0;

	return event_rate;
}

unsigned long vcodec_rate_update_input(
										void *vcodec_rate_id,
										unsigned long event_time )
{
	struct vcodec_rate_channel_node *vcodec_rate_instance
		= (struct vcodec_rate_channel_node *)vcodec_rate_id;
	unsigned long event_rate;	/* 100 */

	mutex_lock( &vcodec_rate_lock );

	event_rate = _vcodec_rate_update_average(
					&vcodec_rate_instance->input, event_time );

	mutex_unlock( &vcodec_rate_lock );

	return event_rate;
}

unsigned long vcodec_rate_update_codec_done(
				void *vcodec_rate_id, unsigned long event_time )
{
	struct vcodec_rate_channel_node *vcodec_rate_instance
		= (struct vcodec_rate_channel_node *)vcodec_rate_id;
	unsigned long event_rate;	/* 100 */

	mutex_lock( &vcodec_rate_lock );

	event_rate = _vcodec_rate_update_average(
					&vcodec_rate_instance->decode, event_time );

	mutex_unlock( &vcodec_rate_lock );

	return event_rate;
}

static unsigned long _vcodec_rate_convert_us_to_jiffies(
					unsigned long long timestamp ) /*micro second*/
{
	timestamp *= (unsigned long long)(HZ);
	do_div( timestamp, 1000000 );

	return (unsigned long)timestamp;
}

unsigned long vcodec_rate_update_timestamp_input(
				void *vcodec_rate_id, unsigned long long timestamp )
{
	struct vcodec_rate_channel_node *vcodec_rate_instance
		= (struct vcodec_rate_channel_node *)vcodec_rate_id;
	unsigned long event_time;
	unsigned long event_rate;	/* 100 */

	mutex_lock( &vcodec_rate_lock );

	event_time = _vcodec_rate_convert_us_to_jiffies( timestamp );

	event_rate = _vcodec_rate_update_average(
					&vcodec_rate_instance->timestamp_input, event_time );

	if (vcodec_rate_instance->timestamp_input_prev != 0xFFFFFFFFFFFFFFFFLL) {
		if (vcodec_rate_instance->timestamp_input_prev >= timestamp) {
			vcodec_rate_instance->timestamp_jitter_log++;
			if ((vcodec_rate_instance->timestamp_jitter_log % 0x20) == 0x1)
				vlog_warning( "time stamp jitter - prev:%llu, curr:%llu\n",
					vcodec_rate_instance->timestamp_input_prev, timestamp );
		}
	}
	vcodec_rate_instance->timestamp_input_prev = timestamp;

	mutex_unlock( &vcodec_rate_lock );

	return event_rate;
}

unsigned long vcodec_rate_update_timestamp_output(
				void *vcodec_rate_id, unsigned long long timestamp )
{
	struct vcodec_rate_channel_node *vcodec_rate_instance
		= (struct vcodec_rate_channel_node *)vcodec_rate_id;
	unsigned long event_time;
	unsigned long event_rate;	/* 100 */

	mutex_lock( &vcodec_rate_lock );

	event_time = _vcodec_rate_convert_us_to_jiffies( timestamp );

	event_rate = _vcodec_rate_update_average(
					&vcodec_rate_instance->timestamp_output, event_time );

	if (vcodec_rate_instance->timestamp_output_prev != 0xFFFFFFFFFFFFFFFFLL) {
		if (vcodec_rate_instance->timestamp_output_prev >= timestamp) {
			vcodec_rate_instance->timestamp_jitter_log++;
			if ((vcodec_rate_instance->timestamp_jitter_log % 0x20) == 0x1)
				vlog_error( "time stamp jitter - prev:%llu, curr:%llu\n",
					vcodec_rate_instance->timestamp_output_prev, timestamp );
		}
	}
	vcodec_rate_instance->timestamp_output_prev = timestamp;

	mutex_unlock( &vcodec_rate_lock );

	return event_rate;
}

