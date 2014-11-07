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

#include "vec_picq.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <media/odin/vcodec/vlog.h>

/*#define	VEC_CAMERA_WORKAROUND*/

struct vec_picq_list_node
{
	struct list_head list;
	struct vec_picq_node data;
};

struct vec_picq_channel
{
	struct list_head waiting_list;
	struct list_head encoding_list;
	struct list_head encoded_list;

	struct
	{
	} debug;
};

struct vec_picq_channel_node
{
	struct list_head list;
	struct vec_picq_channel data;
};

struct mutex vec_picq_lock;
struct list_head vec_picq_instance_list;

void vec_picq_init( void )
{
	mutex_init( &vec_picq_lock );
	INIT_LIST_HEAD( &vec_picq_instance_list );
}

void vec_picq_cleanup( void )
{
}

void *vec_picq_open( void )
{
	struct vec_picq_channel_node *vec_picq_instance;

	mutex_lock( &vec_picq_lock );

	vec_picq_instance = (struct vec_picq_channel_node *)vzalloc( \
							sizeof(struct vec_picq_channel_node) );
	if (!vec_picq_instance) {
		vlog_error( "failed to allocate memory\n" );
		mutex_unlock( &vec_picq_lock );
		return (void *)NULL;
	}

	INIT_LIST_HEAD( &vec_picq_instance->data.waiting_list );
	INIT_LIST_HEAD( &vec_picq_instance->data.encoding_list );
	INIT_LIST_HEAD( &vec_picq_instance->data.encoded_list );

	list_add_tail( &vec_picq_instance->list, &vec_picq_instance_list );

	mutex_unlock( &vec_picq_lock );

	return (void *)vec_picq_instance;
}

void vec_picq_close( void *vec_picq_id )
{
	struct vec_picq_channel_node *vec_picq_instance = \
				(struct vec_picq_channel_node *)vec_picq_id;
	struct vec_picq_list_node *picq_list_node, *tmp;

	mutex_lock( &vec_picq_lock );

	list_for_each_entry_safe( picq_list_node, tmp,
				&vec_picq_instance->data.waiting_list, list ) {
		list_del( &picq_list_node->list );
		vfree( picq_list_node );
	}
	list_for_each_entry_safe( picq_list_node, tmp,
				&vec_picq_instance->data.encoding_list, list ) {
		list_del( &picq_list_node->list );
		vfree( picq_list_node );
	}
	list_for_each_entry_safe( picq_list_node, tmp,
				&vec_picq_instance->data.encoded_list, list ) {
		list_del( &picq_list_node->list );
		vfree( picq_list_node );
	}

	list_del( &vec_picq_instance->list );
	vfree( vec_picq_instance );

	mutex_unlock( &vec_picq_lock );
}

static bool _vec_picq_validate_list(
					struct vec_picq_channel_node *vec_picq_instance,
					unsigned int pic_phy_addr )
{
	struct vec_picq_list_node *picq_list_node, *tmp;
	unsigned int addr_matched_cnt = 0;

	list_for_each_entry_safe( picq_list_node, tmp,
					&vec_picq_instance->data.waiting_list, list) {
		if ( picq_list_node->data.pic_phy_addr == pic_phy_addr )
			addr_matched_cnt++;
	}
	list_for_each_entry_safe( picq_list_node, tmp,
					&vec_picq_instance->data.encoding_list, list) {
		if ( picq_list_node->data.pic_phy_addr == pic_phy_addr )
			addr_matched_cnt++;
	}
	list_for_each_entry_safe( picq_list_node, tmp,
					&vec_picq_instance->data.encoded_list, list) {
		if ( picq_list_node->data.pic_phy_addr == pic_phy_addr )
			addr_matched_cnt++;
	}

	if (addr_matched_cnt != 1) {
		vlog_error( "pic addr:0x%X, not matched picq list, \
					 matched count:%u\n",
						pic_phy_addr, addr_matched_cnt );
		return false;
	}

	return true;
}

void vec_picq_debug_print_pic_phy_addr( void *vec_picq_id )
{
	struct vec_picq_channel_node *vec_picq_instance = \
				(struct vec_picq_channel_node *)vec_picq_id;
	struct vec_picq_list_node *picq_list_node, *tmp;

	mutex_lock( &vec_picq_lock );

	list_for_each_entry_safe( picq_list_node, tmp,
				&vec_picq_instance->data.waiting_list, list ) {
		vlog_warning( "waiting list - pic addr:0x%08X\n",
					 	picq_list_node->data.pic_phy_addr );
	}
	list_for_each_entry_safe( picq_list_node, tmp,
				&vec_picq_instance->data.encoding_list, list ) {
		vlog_warning( "encoding(vcore) list - pic addr:0x%08X\n",
						picq_list_node->data.pic_phy_addr );
	}
	list_for_each_entry_safe( picq_list_node, tmp,
						&vec_picq_instance->data.encoded_list, list ) {
		vlog_warning( "encoded(camera) list - pic addr:0x%08X\n",
						picq_list_node->data.pic_phy_addr );
	}

	mutex_unlock( &vec_picq_lock );
}

bool vec_picq_push_encoded( void *vec_picq_id, struct vec_picq_node *picq_data )
{
	struct vec_picq_channel_node *vec_picq_instance = \
				(struct vec_picq_channel_node *)vec_picq_id;
	struct vec_picq_list_node *picq_list_node = \
				container_of(picq_data,
							struct vec_picq_list_node, data);
	bool ret;

	mutex_lock( &vec_picq_lock );

	list_add_tail( &picq_list_node->list, \
					&vec_picq_instance->data.encoded_list );

	ret = _vec_picq_validate_list( vec_picq_instance,
									 picq_data->pic_phy_addr );

	mutex_unlock( &vec_picq_lock );

	return ret;
}

struct vec_picq_node *vec_picq_pop_encoded( void *vec_picq_id,
											unsigned int pic_phy_addr )
{
	struct vec_picq_channel_node *vec_picq_instance = \
				(struct vec_picq_channel_node *)vec_picq_id;
	struct vec_picq_list_node *picq_list_node, *tmp;

	mutex_lock( &vec_picq_lock );

	list_for_each_entry_safe( picq_list_node, tmp,
					&vec_picq_instance->data.encoded_list, list ) {
		if ( picq_list_node->data.pic_phy_addr == pic_phy_addr ) {
			list_del( &picq_list_node->list );
			mutex_unlock( &vec_picq_lock );
			return (struct vec_picq_node *)&picq_list_node->data;
		}
	}

	picq_list_node = (struct vec_picq_list_node *)vzalloc( \
									sizeof(struct vec_picq_list_node) );
	if (!picq_list_node) {
		vlog_error( "failed to allocate memory\n" );
		mutex_unlock( &vec_picq_lock );
		return (struct vec_picq_node *)NULL;
	}

	picq_list_node->data.pic_phy_addr = pic_phy_addr;

/*	vlog_warning( "new pic addr:0x%08X\n", pic_phy_addr ); */

	mutex_unlock( &vec_picq_lock );

	return (struct vec_picq_node *)&picq_list_node->data;
}

bool vec_picq_push_waiting( void *vec_picq_id,
			struct vec_picq_node *picq_data )
{
	struct vec_picq_channel_node *vec_picq_instance = \
				(struct vec_picq_channel_node *)vec_picq_id;
	struct vec_picq_list_node *picq_list_node = \
			container_of(picq_data, struct vec_picq_list_node, data);
	bool ret;

	mutex_lock( &vec_picq_lock );

	list_add_tail( &picq_list_node->list,
					&vec_picq_instance->data.waiting_list );

	ret = _vec_picq_validate_list( vec_picq_instance,
									picq_data->pic_phy_addr );

	mutex_unlock( &vec_picq_lock );

	return ret;
}

struct vec_picq_node *vec_picq_peep_waiting( void *vec_picq_id )
{
	struct vec_picq_channel_node *vec_picq_instance = \
				(struct vec_picq_channel_node *)vec_picq_id;
	struct vec_picq_list_node *picq_list_node;
#ifdef VEC_CAMERA_WORKAROUND
	struct list_head *pos;
	unsigned int num_of_waiting_picq_node = 0;
#endif

	mutex_lock( &vec_picq_lock );
#ifdef VEC_CAMERA_WORKAROUND
	list_for_each( pos, &vec_picq_instance->data.waiting_list ) {
		num_of_waiting_picq_node++;
	}
	if (num_of_waiting_picq_node <= 2) {
		vlog_warning( "waiting queue(%d) empty\n",
						num_of_waiting_picq_node );
		mutex_unlock( &vec_picq_lock );
		return (struct vec_picq_node *)NULL;
	}
#else
	if (list_empty( &vec_picq_instance->data.waiting_list )) {
		vlog_warning( "waiting queue empty\n" );
		mutex_unlock( &vec_picq_lock );
		return (struct vec_picq_node *)NULL;
	}
#endif

	picq_list_node = list_entry( vec_picq_instance->data.waiting_list.next,
								struct vec_picq_list_node, list );

	mutex_unlock( &vec_picq_lock );

	return (struct vec_picq_node *)&picq_list_node->data;
}

struct vec_picq_node *vec_picq_pop_waiting( void *vec_picq_id )
{
	struct vec_picq_channel_node *vec_picq_instance = \
				(struct vec_picq_channel_node *)vec_picq_id;
	struct vec_picq_list_node *picq_list_node;
#ifdef VEC_CAMERA_WORKAROUND
	struct list_head *pos;
	unsigned int num_of_waiting_picq_node = 0;
#endif

	mutex_lock( &vec_picq_lock );
#ifdef VEC_CAMERA_WORKAROUND
	list_for_each( pos, &vec_picq_instance->data.waiting_list ) {
		num_of_waiting_picq_node++;
	}
	if (num_of_waiting_picq_node <= 2) {
		vlog_warning( "waiting queue(%d) empty\n",
						num_of_waiting_picq_node );
		mutex_unlock( &vec_picq_lock );
		return (struct vec_picq_node *)NULL;
	}
#else
	if (list_empty( &vec_picq_instance->data.waiting_list )) {
		vlog_warning( "waiting queue empty\n" );
		mutex_unlock( &vec_picq_lock );
		return (struct vec_picq_node *)NULL;
	}
#endif

	picq_list_node = list_entry( vec_picq_instance->data.waiting_list.next,
								 struct vec_picq_list_node, list );
	list_del( &picq_list_node->list );

	mutex_unlock( &vec_picq_lock );

	return (struct vec_picq_node *)&picq_list_node->data;
}

bool vec_picq_push_encoding( void *vec_picq_id,
			struct vec_picq_node *picq_data )
{
	struct vec_picq_channel_node *vec_picq_instance = \
				(struct vec_picq_channel_node *)vec_picq_id;
	struct vec_picq_list_node *picq_list_node = \
			container_of(picq_data,
						struct vec_picq_list_node, data);
	bool ret;

	mutex_lock( &vec_picq_lock );
#if 0
	if (!list_empty( &vec_picq_instance->data.encoding_list )) {
		struct vec_picq_list_node *picq_list_node_i, *tmp;

		list_for_each_entry_safe( picq_list_node_i, tmp,
							 &vec_picq_instance->data.encoding_list,
							 list ) {
			vlog_warning( "encoding list - pic addr:0x%08X\n",
							picq_list_node_i->data.pic_phy_addr );
		}
		vlog_warning( "new encoding pic addr:0x%08X\n",
						picq_data->pic_phy_addr );
	}
#endif
	list_add_tail( &picq_list_node->list,
			&vec_picq_instance->data.encoding_list );

	ret = _vec_picq_validate_list( vec_picq_instance,
					 				picq_data->pic_phy_addr );

	mutex_unlock( &vec_picq_lock );

	return ret;
}

struct vec_picq_node *vec_picq_pop_encoding( void *vec_picq_id )
{
	struct vec_picq_channel_node *vec_picq_instance = \
				(struct vec_picq_channel_node *)vec_picq_id;
	struct vec_picq_list_node *picq_list_node;

	mutex_lock( &vec_picq_lock );

	if (list_empty( &vec_picq_instance->data.encoding_list )) {
		vlog_warning( "encoding queue empty\n" );
		mutex_unlock( &vec_picq_lock );
		return (struct vec_picq_node *)NULL;
	}

	picq_list_node = list_entry(
								vec_picq_instance->data.encoding_list.next,
								struct vec_picq_list_node, list );
	list_del( &picq_list_node->list );

	mutex_unlock( &vec_picq_lock );

	return (struct vec_picq_node *)&picq_list_node->data;
}

unsigned int vec_picq_waiting_occupancy( void *vec_picq_id )
{
	struct vec_picq_channel_node *vec_picq_instance = \
				(struct vec_picq_channel_node *)vec_picq_id;
	struct list_head *pos;
	unsigned int num_of_waiting_picq_node = 0;

	mutex_lock( &vec_picq_lock );

	list_for_each( pos, &vec_picq_instance->data.waiting_list ) {
		num_of_waiting_picq_node++;
	}
#ifdef VEC_CAMERA_WORKAROUND
	if (num_of_waiting_picq_node >= 2)
		num_of_waiting_picq_node -= 2;
	else
		num_of_waiting_picq_node = 0;
#endif
	mutex_unlock( &vec_picq_lock );

	return num_of_waiting_picq_node;
}


