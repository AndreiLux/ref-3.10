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

#include "vdc_auq.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <media/odin/vcodec/vlog.h>

struct vdc_auq_list_node
{
	struct list_head list;
	struct vdc_auq_node data;
};

struct vdc_auq_channel
{
	struct list_head waiting_list;
	struct list_head decoding_list;
	struct list_head free_list;
	struct
	{
		unsigned int cpb_wr_addr;
		unsigned int cpb_rd_addr;
	} status;

	struct
	{
	} debug;
};

struct vdc_auq_channel_node
{
	struct list_head list;
	struct vdc_auq_channel data;
};

struct mutex vdc_auq_lock;
struct list_head vdc_auq_instance_list;

void vdc_auq_init( void )
{
	mutex_init( &vdc_auq_lock );
	INIT_LIST_HEAD( &vdc_auq_instance_list );
}

void vdc_auq_cleanup( void )
{
	mutex_destroy( &vdc_auq_lock );
}

void *vdc_auq_open( void )
{
	struct vdc_auq_channel_node *vdc_auq_instance;
	unsigned int node_index;

	mutex_lock( &vdc_auq_lock );

	vdc_auq_instance
		= (struct vdc_auq_channel_node *)vzalloc( \
									sizeof(struct vdc_auq_channel_node));
	if (!vdc_auq_instance)
	{
		vlog_error( "failed to allocate memory\n" );
		mutex_unlock( &vdc_auq_lock );
		return (void *)NULL;
	}

	INIT_LIST_HEAD( &vdc_auq_instance->data.waiting_list );
	INIT_LIST_HEAD( &vdc_auq_instance->data.decoding_list );
	INIT_LIST_HEAD( &vdc_auq_instance->data.free_list );
#if 1	/* for debug */
	for (node_index = 0; node_index < 0x10; node_index++) {
		struct vdc_auq_list_node *auq_list_node;

		auq_list_node = (struct vdc_auq_list_node *)vzalloc( \
											sizeof(struct vdc_auq_list_node));
		if (!auq_list_node) {
			vlog_error( "failed to allocate memory\n" );
			mutex_unlock( &vdc_auq_lock );
			return false;
		}

		list_add_tail( &auq_list_node->list, &vdc_auq_instance->data.free_list );
	}
#endif

	list_add_tail( &vdc_auq_instance->list, &vdc_auq_instance_list );

	mutex_unlock( &vdc_auq_lock );

	return (void *)vdc_auq_instance;
}

void vdc_auq_close( void *vdc_auq_id )
{
	struct vdc_auq_channel_node *vdc_auq_instance
			= (struct vdc_auq_channel_node *)vdc_auq_id;
	struct vdc_auq_list_node *auq_list_node, *tmp;

	mutex_lock( &vdc_auq_lock );

	list_for_each_entry_safe( auq_list_node, tmp, \
			&vdc_auq_instance->data.waiting_list, list ) {
		list_del( &auq_list_node->list );
		vfree( auq_list_node );
	}
	list_for_each_entry_safe( auq_list_node, tmp, \
			&vdc_auq_instance->data.decoding_list, list ) {
		list_del( &auq_list_node->list );
		vfree( auq_list_node );
	}
	list_for_each_entry_safe( auq_list_node, tmp, \
			&vdc_auq_instance->data.free_list, list ) {
		list_del( &auq_list_node->list );
		vfree( auq_list_node );
	}

	list_del( &vdc_auq_instance->list );
	vfree( vdc_auq_instance );

	mutex_unlock( &vdc_auq_lock );
}

bool vdc_auq_push_waiting( void *vdc_auq_id, struct vdc_auq_node *auq_data )
{
	struct vdc_auq_channel_node *vdc_auq_instance
			= (struct vdc_auq_channel_node *)vdc_auq_id;
	struct vdc_auq_list_node *auq_list_node;

	mutex_lock( &vdc_auq_lock );

	if (list_empty( &vdc_auq_instance->data.free_list )) {
		vlog_warning( "shortage of free node buffer\n" );

		auq_list_node = (struct vdc_auq_list_node *)vzalloc( \
											sizeof(struct vdc_auq_list_node));
		if (!auq_list_node) {
			vlog_error( "failed to allocate memory\n" );
			mutex_unlock( &vdc_auq_lock );
			return false;
		}
	}
	else {
		auq_list_node = list_entry( vdc_auq_instance->data.free_list.next, \
									struct vdc_auq_list_node, list );
		list_del( &auq_list_node->list );
	}

	memcpy( &auq_list_node->data, auq_data, sizeof(struct vdc_auq_node) );
	list_add_tail( &auq_list_node->list, &vdc_auq_instance->data.waiting_list );

	vdc_auq_instance->data.status.cpb_wr_addr = auq_data->end_phy_addr;

	mutex_unlock( &vdc_auq_lock );
	return true;
}

bool vdc_auq_peep_waiting( void *vdc_auq_id, struct vdc_auq_node *auq_data )
{
	struct vdc_auq_channel_node *vdc_auq_instance
			= (struct vdc_auq_channel_node *)vdc_auq_id;
	struct vdc_auq_list_node *auq_list_node;

	mutex_lock( &vdc_auq_lock );

	if (list_empty( &vdc_auq_instance->data.waiting_list )) {
		vlog_warning( "waiting queue empty\n" );
		mutex_unlock( &vdc_auq_lock );
		return false;
	}

	auq_list_node = list_entry( vdc_auq_instance->data.waiting_list.next, \
							struct vdc_auq_list_node, list );
	memcpy( auq_data, &auq_list_node->data, sizeof(struct vdc_auq_node) );

	mutex_unlock( &vdc_auq_lock );
	return true;
}

bool vdc_auq_pop_waiting( void *vdc_auq_id, struct vdc_auq_node *auq_data )
{
	struct vdc_auq_channel_node *vdc_auq_instance
			= (struct vdc_auq_channel_node *)vdc_auq_id;
	struct vdc_auq_list_node *auq_list_node;

	mutex_lock( &vdc_auq_lock );

	if (list_empty( &vdc_auq_instance->data.waiting_list )) {
		vlog_error( "waiting queue empty\n" );
		mutex_unlock( &vdc_auq_lock );
		return false;
	}

	auq_list_node = list_entry( vdc_auq_instance->data.waiting_list.next, \
								struct vdc_auq_list_node, list );
	memcpy( auq_data, &auq_list_node->data, sizeof(struct vdc_auq_node) );

	list_del( &auq_list_node->list );
	vfree( auq_list_node );

	vdc_auq_instance->data.status.cpb_rd_addr = auq_data->end_phy_addr;

	mutex_unlock( &vdc_auq_lock );
	return true;
}

bool vdc_auq_move_waiting_to_decoding( void *vdc_auq_id,
										struct vdc_auq_node *auq_data )
{
	struct vdc_auq_channel_node *vdc_auq_instance
			= (struct vdc_auq_channel_node *)vdc_auq_id;
	struct vdc_auq_list_node *auq_list_node
			= container_of(auq_data,
							 struct vdc_auq_list_node, data);

	mutex_lock( &vdc_auq_lock );

	if (list_empty( &vdc_auq_instance->data.waiting_list )) {
		vlog_error( "waiting queue empty\n" );
		mutex_unlock( &vdc_auq_lock );
		return false;
	}

	auq_list_node = list_entry( vdc_auq_instance->data.waiting_list.next, \
								struct vdc_auq_list_node, list );
	memcpy( auq_data, &auq_list_node->data, sizeof(struct vdc_auq_node) );

	list_del( &auq_list_node->list );
	list_add_tail( &auq_list_node->list,
					&vdc_auq_instance->data.decoding_list );

	vdc_auq_instance->data.status.cpb_rd_addr = auq_data->end_phy_addr;

	mutex_unlock( &vdc_auq_lock );
	return true;
}

bool vdc_auq_pop_decoding( void *vdc_auq_id, struct vdc_auq_node *auq_data )
{
	struct vdc_auq_channel_node *vdc_auq_instance
			= (struct vdc_auq_channel_node *)vdc_auq_id;
	struct vdc_auq_list_node *auq_list_node;

	mutex_lock( &vdc_auq_lock );

	if (list_empty( &vdc_auq_instance->data.decoding_list )) {
		mutex_unlock( &vdc_auq_lock );
		return false;
	}

	auq_list_node = list_entry( vdc_auq_instance->data.decoding_list.next,
								struct vdc_auq_list_node, list );
	memcpy( auq_data, &auq_list_node->data, sizeof(struct vdc_auq_node) );

	list_del( &auq_list_node->list );
	list_add_tail( &auq_list_node->list, &vdc_auq_instance->data.free_list );

	mutex_unlock( &vdc_auq_lock );
	return true;
}

unsigned int vdc_auq_get_latest_wraddr( void *vdc_auq_id )
{
	struct vdc_auq_channel_node *vdc_auq_instance
			= (struct vdc_auq_channel_node *)vdc_auq_id;

	return vdc_auq_instance->data.status.cpb_wr_addr;
}

unsigned int vdc_auq_occupancy( void *vdc_auq_id )
{
	struct vdc_auq_channel_node *vdc_auq_instance
			= (struct vdc_auq_channel_node *)vdc_auq_id;
	struct list_head *pos;
	unsigned int num_of_waiting_auq_node = 0;

	mutex_lock( &vdc_auq_lock );

	list_for_each( pos, &vdc_auq_instance->data.waiting_list ) {
		num_of_waiting_auq_node++;
	}
	list_for_each( pos, &vdc_auq_instance->data.decoding_list ) {
		num_of_waiting_auq_node++;
	}

	mutex_unlock( &vdc_auq_lock );

	return num_of_waiting_auq_node;
}


