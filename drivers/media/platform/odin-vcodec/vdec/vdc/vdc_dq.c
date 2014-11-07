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

#include "vdc_dq.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

#include <media/odin/vcodec/vlog.h>

/* #define	VDC_DISPLAY_WORKAROUND */

#ifdef VDC_DISPLAY_WORKAROUND
#define	VDC_DELAYED_CLEAR_PIC_CNT	4
#endif

struct vdc_dq_list_node
{
	struct list_head list;
	struct vdc_dq_node data;
};

struct vdc_dq_channel
{
	struct
	{
		unsigned int num_of_node;
	} config;

	spinlock_t list_lock;
	struct list_head free_list;
	struct list_head vcore_list;
	struct list_head decoded_list;
	struct list_head displaying_list;
	struct list_head displayed_list;
	unsigned int num_of_pic;

	struct
	{
	} debug;
};

struct vdc_dq_channel_node
{
	struct list_head list;
	struct vdc_dq_channel data;
};

struct mutex vdc_dq_lock;
struct list_head vdc_dq_instance_list;

void vdc_dq_init( void )
{
	mutex_init( &vdc_dq_lock );
	INIT_LIST_HEAD( &vdc_dq_instance_list );
}

void vdc_dq_cleanup( void )
{
	mutex_destroy( &vdc_dq_lock );
}

void vdc_dq_reset( void *vdc_dq_id )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node, *tmp;
	unsigned long flags;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	list_for_each_entry_safe( \
					dq_list_node, tmp, &vdc_dq_instance->data.vcore_list, list ) {
		list_del( &dq_list_node->list );
		list_add_tail( &dq_list_node->list, &vdc_dq_instance->data.free_list );
	}

	list_for_each_entry_safe( \
			dq_list_node, tmp, &vdc_dq_instance->data.decoded_list, list ) {
		list_del( &dq_list_node->list );
		list_add_tail( &dq_list_node->list, &vdc_dq_instance->data.free_list );
	}

	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.displaying_list, list ) {
		list_del( &dq_list_node->list );
		list_add_tail( &dq_list_node->list, &vdc_dq_instance->data.free_list );
	}

	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.displayed_list, list ) {
		list_del( &dq_list_node->list );
		list_add_tail( &dq_list_node->list, &vdc_dq_instance->data.free_list );
	}

	vdc_dq_instance->data.num_of_pic = 0;

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
}

void *vdc_dq_open( unsigned int num_of_node )
{
	struct vdc_dq_channel_node *vdc_dq_instance;
	struct vdc_dq_list_node *dq_list_node;
	unsigned int node_index;

	mutex_lock( &vdc_dq_lock );

	vdc_dq_instance = (struct vdc_dq_channel_node *)vzalloc( \
									sizeof(struct vdc_dq_channel_node));
	if (!vdc_dq_instance) {
		vlog_error( "failed to allocate memory\n" );
		mutex_unlock( &vdc_dq_lock );
		return (void *)NULL;
	}

	vdc_dq_instance->data.config.num_of_node = num_of_node;
	vdc_dq_instance->data.num_of_pic = 0;

	spin_lock_init( &vdc_dq_instance->data.list_lock );

	INIT_LIST_HEAD( &vdc_dq_instance->data.free_list );
	INIT_LIST_HEAD( &vdc_dq_instance->data.vcore_list );
	INIT_LIST_HEAD( &vdc_dq_instance->data.decoded_list );
	INIT_LIST_HEAD( &vdc_dq_instance->data.displaying_list );
	INIT_LIST_HEAD( &vdc_dq_instance->data.displayed_list );

	for (node_index = 0; node_index < num_of_node; node_index++) {
		dq_list_node = (struct vdc_dq_list_node *)vzalloc( \
										sizeof(struct vdc_dq_list_node));
		if (!dq_list_node) {
			vlog_error( "failed to allocate memory\n" );
			mutex_unlock( &vdc_dq_lock );
			return (void *)NULL;
		}

		list_add_tail( &dq_list_node->list, &vdc_dq_instance->data.free_list );
	}

	list_add_tail( &vdc_dq_instance->list, &vdc_dq_instance_list );

	mutex_unlock( &vdc_dq_lock );

	return (void *)vdc_dq_instance;
}

void vdc_dq_close( void *vdc_dq_id )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node, *tmp;
	unsigned long flags;
	struct list_head delete_list;

	mutex_lock( &vdc_dq_lock );

	INIT_LIST_HEAD( &delete_list );

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.free_list, list ) {
		list_del( &dq_list_node->list );
		list_add_tail( &dq_list_node->list, &delete_list );
	}
	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.vcore_list, list ) {
		list_del( &dq_list_node->list );
		list_add_tail( &dq_list_node->list, &delete_list );
	}
	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.decoded_list, list ) {
		list_del( &dq_list_node->list );
		list_add_tail( &dq_list_node->list, &delete_list );
	}
	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.displaying_list, list ) {
		list_del( &dq_list_node->list );
		list_add_tail( &dq_list_node->list, &delete_list );
	}
	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.displayed_list, list ) {
		list_del( &dq_list_node->list );
		list_add_tail( &dq_list_node->list, &delete_list );
	}

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	list_del( &vdc_dq_instance->list );
	vfree( vdc_dq_instance );

	list_for_each_entry_safe( dq_list_node, tmp, \
			&delete_list, list ) {
		list_del( &dq_list_node->list );
		vfree( dq_list_node );
	}

	mutex_unlock( &vdc_dq_lock );
}

struct vdc_dq_node *vdc_dq_pop_for_flush( void *vdc_dq_id )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node;
	unsigned long flags;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	if (list_empty( &vdc_dq_instance->data.vcore_list )) {
		if (list_empty( &vdc_dq_instance->data.decoded_list )) {
			if (list_empty( &vdc_dq_instance->data.displayed_list )) {
				dq_list_node = NULL;
			}
			else {
				dq_list_node = list_entry( \
							vdc_dq_instance->data.displayed_list.next, \
							struct vdc_dq_list_node, list );
				list_del( &dq_list_node->list );
			}
 		}
		else {
			dq_list_node = list_entry( \
							vdc_dq_instance->data.decoded_list.next, \
							struct vdc_dq_list_node, list );
			list_del( &dq_list_node->list );
		}
 	}
	else {
		dq_list_node = list_entry( vdc_dq_instance->data.vcore_list.next, \
								struct vdc_dq_list_node, list );
		list_del( &dq_list_node->list );
	}

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return (dq_list_node == NULL) ? \
		(struct vdc_dq_node *)NULL : (struct vdc_dq_node *)&dq_list_node->data;
}

void vdc_dq_debug_print_dpb_addr( void *vdc_dq_id )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node, *tmp;
	unsigned long flags;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.vcore_list, list ) {
		vlog_warning( "vcore    list - dpb addr:0x%08X\n",
			dq_list_node->data.dpb_addr );
	}
	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.decoded_list, list ) {
		vlog_warning( "decoded  list - dpb addr:0x%08X\n",
			dq_list_node->data.dpb_addr );
	}
	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.displaying_list, list ) {
		vlog_warning( "renderer list - dpb addr:0x%08X\n",
			dq_list_node->data.dpb_addr );
	}
	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.displayed_list, list ) {
		vlog_warning( "clear    list - dpb addr:0x%08X\n",
			dq_list_node->data.dpb_addr );
	}

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
}

static unsigned int _vdc_dq_validate_list(
						struct vdc_dq_channel_node *vdc_dq_instance,
						unsigned int dpb_addr )
{
	struct vdc_dq_list_node *dq_list_node, *tmp;
	unsigned int addr_matched_cnt = 0;

	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.vcore_list, list ) {
		if ( dq_list_node->data.dpb_addr == dpb_addr )
			addr_matched_cnt++;
	}
	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.decoded_list, list ) {
		if ( dq_list_node->data.dpb_addr == dpb_addr )
			addr_matched_cnt++;
	}
	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.displaying_list, list ) {
		if ( dq_list_node->data.dpb_addr == dpb_addr )
			addr_matched_cnt++;
	}
	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.displayed_list, list ) {
		if ( dq_list_node->data.dpb_addr == dpb_addr )
			addr_matched_cnt++;
	}

	if (addr_matched_cnt) {
		vlog_error( "dpb addr:0x%X, not matched dq list, matched count:%u\n",
					dpb_addr, addr_matched_cnt );
	}

	return addr_matched_cnt;
}

bool vdc_dq_put_free_node( void *vdc_dq_id, struct vdc_dq_node *dq_data )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node
			= container_of(dq_data, struct vdc_dq_list_node, data);
	unsigned long flags;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	list_add_tail( &dq_list_node->list, &vdc_dq_instance->data.free_list );

	vdc_dq_instance->data.num_of_pic--;

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return true;
}

struct vdc_dq_node *vdc_dq_get_free_node( void *vdc_dq_id )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node;
	unsigned long flags;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	if (list_empty( &vdc_dq_instance->data.free_list )) {
		vlog_error( "free node queue empty\n" );
		spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
		return (struct vdc_dq_node *)NULL;
	}

	dq_list_node = list_entry( vdc_dq_instance->data.free_list.next,
							struct vdc_dq_list_node, list );
	list_del( &dq_list_node->list );

	vdc_dq_instance->data.num_of_pic++;

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return (struct vdc_dq_node *)&dq_list_node->data;
}

bool vdc_dq_push_vcore( void *vdc_dq_id, struct vdc_dq_node *dq_data )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node
			= container_of(dq_data, struct vdc_dq_list_node, data);
	unsigned long flags;
	unsigned int addr_matched_cnt;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	addr_matched_cnt = _vdc_dq_validate_list( vdc_dq_instance, dq_data->dpb_addr );
	if (addr_matched_cnt) {
		vlog_error( "failed to push dq, dpb_addr:0x%X, matched count:%u\n",
					dq_data->dpb_addr, addr_matched_cnt );
		spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
		vdc_dq_debug_print_dpb_addr( vdc_dq_id );
		return false;
	}

	list_add_tail( &dq_list_node->list, &vdc_dq_instance->data.vcore_list );

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return true;
}

struct vdc_dq_node *vdc_dq_pop_vcore( void *vdc_dq_id, unsigned int fb_addr )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node, *tmp;
	unsigned long flags;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	list_for_each_entry_safe( \
				dq_list_node, tmp, &vdc_dq_instance->data.vcore_list, list ) {
		if ( dq_list_node->data.dpb_addr == fb_addr ) {
			list_del( &dq_list_node->list );
			spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
			return (struct vdc_dq_node *)&dq_list_node->data;
		}
	}

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return (struct vdc_dq_node *)NULL;
}

bool vdc_dq_push_decoded( void *vdc_dq_id, struct vdc_dq_node *dq_data )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node
			= container_of(dq_data, struct vdc_dq_list_node, data);
	unsigned long flags;
	unsigned int addr_matched_cnt;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	addr_matched_cnt = _vdc_dq_validate_list( \
							vdc_dq_instance, dq_data->dpb_addr );
	if (addr_matched_cnt) {
		vlog_error( "failed to push dq, dpb_addr:0x%X, matched count:%u\n",
					dq_data->dpb_addr, addr_matched_cnt );
		spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
		vdc_dq_debug_print_dpb_addr( vdc_dq_id );
		return false;
	}

	list_add_tail( &dq_list_node->list, &vdc_dq_instance->data.decoded_list );

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return true;
}

struct vdc_dq_node *vdc_dq_pop_decoded( void *vdc_dq_id, unsigned int fb_addr )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node, *tmp;
	unsigned long flags;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	list_for_each_entry_safe( \
			dq_list_node, tmp, &vdc_dq_instance->data.decoded_list, list ) {
		if ( dq_list_node->data.dpb_addr == fb_addr ) {
			list_del( &dq_list_node->list );
			spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
			return (struct vdc_dq_node *)&dq_list_node->data;
		}
	}

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return (struct vdc_dq_node *)NULL;
}

bool vdc_dq_push_displaying( void *vdc_dq_id, struct vdc_dq_node *dq_data )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node
			= container_of(dq_data, struct vdc_dq_list_node, data);
	unsigned long flags;
	unsigned int addr_matched_cnt;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	addr_matched_cnt = _vdc_dq_validate_list( \
							vdc_dq_instance, dq_data->dpb_addr );
	if (addr_matched_cnt) {
		vlog_error( "failed to push dq, dpb_addr:0x%X, matched count:%u\n",
					dq_data->dpb_addr, addr_matched_cnt );
		spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
		vdc_dq_debug_print_dpb_addr( vdc_dq_id );
		return false;
	}

	list_add_tail( &dq_list_node->list, &vdc_dq_instance->data.displaying_list );

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return true;
}

struct vdc_dq_node *vdc_dq_pop_displaying( void *vdc_dq_id,
											unsigned int fb_addr )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node, *tmp;
	unsigned long flags;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	list_for_each_entry_safe( dq_list_node, tmp, \
			&vdc_dq_instance->data.displaying_list, list ) {
		if ( dq_list_node->data.dpb_addr == fb_addr ) {
			list_del( &dq_list_node->list );
			spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
			return (struct vdc_dq_node *)&dq_list_node->data;
		}
	}

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return (struct vdc_dq_node *)NULL;
}

bool vdc_dq_push_clear( void *vdc_dq_id, struct vdc_dq_node *dq_data )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node
			= container_of(dq_data, struct vdc_dq_list_node, data);
	unsigned long flags;
	unsigned int addr_matched_cnt;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	addr_matched_cnt = _vdc_dq_validate_list( \
							vdc_dq_instance, dq_data->dpb_addr );
	if (addr_matched_cnt) {
		vlog_error( "failed to push dq, dpb_addr:0x%X, matched count:%u\n",
					dq_data->dpb_addr, addr_matched_cnt );
		spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
		vdc_dq_debug_print_dpb_addr( vdc_dq_id );
		return false;
	}

	list_add_tail( &dq_list_node->list, &vdc_dq_instance->data.displayed_list );

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return true;
}

struct vdc_dq_node *vdc_dq_pop_clear( void *vdc_dq_id )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	struct vdc_dq_list_node *dq_list_node;
	unsigned long flags;
#ifdef VDC_DISPLAY_WORKAROUND
	struct list_head *pos;
	unsigned int num_of_dq_node = 0;
#endif

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );
#ifdef VDC_DISPLAY_WORKAROUND
	if (vdc_dq_instance->data.num_of_pic > VDC_DELAYED_CLEAR_PIC_CNT) {
		list_for_each( pos, &vdc_dq_instance->data.displayed_list ) {
			num_of_dq_node++;
		}
		if (num_of_dq_node <= VDC_DELAYED_CLEAR_PIC_CNT) {
			spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
			return (struct vdc_dq_node *)NULL;
		}
	}
	else {
		if (list_empty( &vdc_dq_instance->data.displayed_list )) {
			spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
			return (struct vdc_dq_node *)NULL;
		}
	}
#else
	if (list_empty( &vdc_dq_instance->data.displayed_list )) {
		spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );
		return (struct vdc_dq_node *)NULL;
	}
#endif
	dq_list_node = list_entry( vdc_dq_instance->data.displayed_list.next, \
							struct vdc_dq_list_node, list );
	list_del( &dq_list_node->list );

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return (struct vdc_dq_node *)&dq_list_node->data;
}

unsigned int vdc_dq_displaying_occupancy( void *vdc_dq_id )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	unsigned long flags;
	struct list_head *pos;
	unsigned int num_of_alive_dq_node = 0;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

#ifdef VDC_DISPLAY_WORKAROUND
	if (vdc_dq_instance->data.num_of_pic > VDC_DELAYED_CLEAR_PIC_CNT) {
		list_for_each( pos, &vdc_dq_instance->data.displayed_list ) {
			num_of_alive_dq_node++;
		}
		if (num_of_alive_dq_node > VDC_DELAYED_CLEAR_PIC_CNT)
			num_of_alive_dq_node = VDC_DELAYED_CLEAR_PIC_CNT;
	}
#endif

	list_for_each( pos, &vdc_dq_instance->data.decoded_list ) {
		num_of_alive_dq_node++;
	}
	list_for_each( pos, &vdc_dq_instance->data.displaying_list ) {
		num_of_alive_dq_node++;
	}

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return num_of_alive_dq_node;
}

unsigned int vdc_dq_vcore_occupancy( void *vdc_dq_id )
{
	struct vdc_dq_channel_node *vdc_dq_instance
			= (struct vdc_dq_channel_node *)vdc_dq_id;
	unsigned long flags;
	struct list_head *pos;
	unsigned int num_of_alive_dq_node = 0;

	spin_lock_irqsave( &vdc_dq_instance->data.list_lock, flags );

	list_for_each( pos, &vdc_dq_instance->data.vcore_list ) {
		num_of_alive_dq_node++;
	}

	spin_unlock_irqrestore( &vdc_dq_instance->data.list_lock, flags );

	return num_of_alive_dq_node;
}


