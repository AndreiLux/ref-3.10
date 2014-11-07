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

#include "ves_drv.h"
#include "ves_cpb.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <media/odin/vcodec/vlog.h>

#define	VES_MAX_NUM_OF_SEQHDR		32

struct ves_channel
{
	struct
	{
		void *top_id;
		bool (*cb_report_au)(void *top_id, struct ves_report_au *report_au);
	} config;

	struct
	{
		void *ves_cpb_id;
	} status;

	struct
	{
		unsigned int received_cnt;
	} debug;
};

struct ves_channel_node
{
	struct list_head list;
	struct ves_channel data;
};

struct mutex ves_lock;
struct list_head ves_instance_list;

void ves_init( void )
{
	mutex_init( &ves_lock );
	INIT_LIST_HEAD( &ves_instance_list );

	ves_cpb_init();
}

void ves_cleanup( void )
{
	ves_cpb_cleanup();
}

void *ves_open( unsigned int cpb_phy_addr, unsigned char *cpb_vir_ptr, unsigned int cpb_size,
				void *top_id, bool (*cb_report_au)( void *top_id, struct ves_report_au *report_au ) )
{
	struct ves_channel_node *ves_instance;

//	vlog_info( "cpb:0x%X++0x%X\n", cpb_phy_addr, cpb_size );

	mutex_lock( &ves_lock );

	ves_instance = (struct ves_channel_node *)vzalloc( sizeof( struct ves_channel_node ) );
	if (!ves_instance) {
		vlog_error( "failed to allocate memory\n" );
		mutex_unlock( &ves_lock );
		return (void *)NULL;
	}

	ves_instance->data.status.ves_cpb_id = ves_cpb_open( cpb_phy_addr, cpb_vir_ptr, cpb_size );
	if (!ves_instance->data.status.ves_cpb_id) {
		vlog_error( "failed to get cpb_id\n" );
		vfree( ves_instance );
		mutex_unlock( &ves_lock );
		return (void *)NULL;
	}

	ves_instance->data.config.top_id = top_id;
	ves_instance->data.config.cb_report_au = cb_report_au;
	ves_instance->data.debug.received_cnt = 0;

	list_add_tail( &ves_instance->list, &ves_instance_list );

	mutex_unlock( &ves_lock );

	return (void *)ves_instance;
}

void ves_close( void *ves_id )
{
	struct ves_channel_node *ves_instance = (struct ves_channel_node *)ves_id;

	vlog_trace( "instance:0x%X\n", (unsigned int)ves_instance );

	mutex_lock( &ves_lock );

	ves_cpb_close( ves_instance->data.status.ves_cpb_id );

	list_del( &ves_instance->list );
	vfree( ves_instance );

	mutex_unlock( &ves_lock );
}

void ves_flush( void *ves_id )
{
	struct ves_channel_node *ves_instance = (struct ves_channel_node *)ves_id;

	mutex_lock( &ves_lock );

	vlog_trace( "received:%u\n", ves_instance->data.debug.received_cnt );
	ves_instance->data.debug.received_cnt = 0;

	mutex_unlock( &ves_lock );
}

bool ves_update_buffer( void *ves_id, struct ves_update_au *update_au, bool user_space_buf, bool ring_buffer, unsigned int align_bytes )
{
	struct ves_channel_node *ves_instance = (struct ves_channel_node *)ves_id;
	struct ves_report_au report_au;

	mutex_lock( &ves_lock );

	if (update_au->es_size) {
		if (ves_cpb_write( ves_instance->data.status.ves_cpb_id,
						update_au->es_addr, update_au->es_size, user_space_buf, ring_buffer, align_bytes,
						&report_au.start_phy_addr, &report_au.start_vir_ptr, &report_au.size, &report_au.end_phy_addr ) == false) {
			vlog_error( "write compressed date to cpb\n" );
			mutex_unlock( &ves_lock );
			return false;
		}
		ves_cpb_update_wrptr( ves_instance->data.status.ves_cpb_id, report_au.end_phy_addr );
	}
	else {
		ves_cpb_get_wrptr(ves_instance->data.status.ves_cpb_id,
							&report_au.start_phy_addr,
							&report_au.start_vir_ptr);
		report_au.size = 0;
		report_au.end_phy_addr = report_au.start_phy_addr;
	}

	report_au.p_buf = update_au->es_addr;
	report_au.private_in_meta = update_au->private_in_meta;
	report_au.eos = update_au->eos;
	report_au.type = update_au->type;
	report_au.timestamp = update_au->timestamp;
	report_au.uid = update_au->uid;
	report_au.input_time = jiffies;

	ves_cpb_get_cbp_status(ves_instance->data.status.ves_cpb_id, &report_au.cpb_occupancy, &report_au.cpb_size);

	ves_instance->data.config.cb_report_au( ves_instance->data.config.top_id, &report_au );

	vlog_print( VLOG_VDEC_VES, "es:0x%X++0x%X --> cpb:0x%X++0x%X=0x%X, data:0x%08X, time stamp:%llu\n",
				(unsigned int)update_au->es_addr, update_au->es_size,
				report_au.start_phy_addr, report_au.size, report_au.end_phy_addr,
				(report_au.start_vir_ptr == NULL) ? 0x0 : *(unsigned int*)report_au.start_vir_ptr, update_au->timestamp );

	ves_instance->data.debug.received_cnt++;
	if ((ves_instance->data.debug.received_cnt % 0x100) == 0x0) {
		unsigned int cpb_occupancy, cpb_size;

		ves_cpb_get_cbp_status( ves_instance->data.status.ves_cpb_id, &cpb_occupancy, &cpb_size );
		vlog_print( VLOG_VDEC_MONITOR, "received au cnt:%u, cpb occupancy:0x%X/0x%X\n",
					ves_instance->data.debug.received_cnt, cpb_occupancy, cpb_size );
	}

	mutex_unlock( &ves_lock );

	return true;
}

void ves_update_rdptr( void *ves_id, unsigned int rd_addr )
{
	struct ves_channel_node *ves_instance = (struct ves_channel_node *)ves_id;

	mutex_lock( &ves_lock );

	ves_cpb_update_rdptr( ves_instance->data.status.ves_cpb_id, rd_addr );

	mutex_unlock( &ves_lock );
}

void ves_get_cbp_status( void *ves_id, unsigned int *cpb_occupancy, unsigned int *cpb_size )
{
	struct ves_channel_node *ves_instance = (struct ves_channel_node *)ves_id;

	mutex_lock( &ves_lock );

	ves_cpb_get_cbp_status( ves_instance->data.status.ves_cpb_id, cpb_occupancy, cpb_size );

	mutex_unlock( &ves_lock );
}


