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

#include "vec_epb.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <asm/string.h>

#include <media/odin/vcodec/vlog.h>

struct vec_epb_channel
{
	struct
	{
		unsigned int epb_phy_addr;
		unsigned char *epb_vir_ptr;
		unsigned int epb_size;
	} config;

	struct
	{
		unsigned int wr_offset;
		unsigned int rd_offset;
	} status;
};

struct vec_epb_channel_node
{
	struct list_head list;
	struct vec_epb_channel data;
};

struct mutex vec_epb_lock;
struct list_head vec_epb_instance_list;

void vec_epb_init( void )
{
	mutex_init( &vec_epb_lock );
	INIT_LIST_HEAD( &vec_epb_instance_list );
}

void vec_epb_cleanup( void )
{
}

void *vec_epb_open( unsigned int epb_phy_addr, unsigned char *epb_vir_ptr,
					unsigned int epb_size )
{
	struct vec_epb_channel_node *vec_epb_instance;

	mutex_lock( &vec_epb_lock );

	vec_epb_instance = (struct vec_epb_channel_node *)vzalloc \
									(sizeof(struct vec_epb_channel_node));
	if (!vec_epb_instance) {
		vlog_error( "failed to allocate memory\n" );
		mutex_unlock( &vec_epb_lock );
		return (void *)NULL;
	}

	vec_epb_instance->data.config.epb_phy_addr = epb_phy_addr;
	vec_epb_instance->data.config.epb_vir_ptr = epb_vir_ptr;
	vec_epb_instance->data.config.epb_size = epb_size;

	vec_epb_instance->data.status.wr_offset = 0;
	vec_epb_instance->data.status.rd_offset = 0;

	list_add_tail( &vec_epb_instance->list, &vec_epb_instance_list );

	mutex_unlock( &vec_epb_lock );

	return (void *)vec_epb_instance;
}

void vec_epb_close( void *vec_epb_id )
{
	struct vec_epb_channel_node *vec_epb_instance = \
							(struct vec_epb_channel_node *)vec_epb_id;

	mutex_lock( &vec_epb_lock );

	list_del( &vec_epb_instance->list );
	vfree( vec_epb_instance );

	mutex_unlock( &vec_epb_lock );
}

bool vec_epb_read( void *vec_epb_id,
						unsigned int es_start_addr, unsigned int es_size,
						unsigned int es_end_addr,
						char *buf_ptr, unsigned int buf_size )
{
	struct vec_epb_channel_node *vec_epb_instance = \
							(struct vec_epb_channel_node *)vec_epb_id;
	unsigned int wr_offset;
	unsigned int rd_offset;
	unsigned char *epb_rd_vir_ptr;

	mutex_lock( &vec_epb_lock );

	if (buf_size < es_size) {
		vlog_error( "small buf size:%u, es size:%u\n", buf_size, es_size );
		mutex_unlock( &vec_epb_lock );
		return false;
	}

	wr_offset = vec_epb_instance->data.status.wr_offset;
	rd_offset = vec_epb_instance->data.status.rd_offset;

	epb_rd_vir_ptr = vec_epb_instance->data.config.epb_vir_ptr + rd_offset;

	if ((rd_offset + es_size) > vec_epb_instance->data.config.epb_size) {
		unsigned int buf_size_front;
		unsigned int buf_size_back;

		vlog_print( VLOG_VENC_EPB, "wrap around, es:0x%X++0x%X = 0x%X\n",
				es_start_addr, es_size, es_end_addr );

		buf_size_front = vec_epb_instance->data.config.epb_size - rd_offset;
		if (copy_to_user( (void __user*)buf_ptr, epb_rd_vir_ptr,
					buf_size_front ))
			vlog_error( "copy_to_user - src:0x%X/0x%X++0x%X --> dst:0x%X\n",
				(unsigned int)epb_rd_vir_ptr, es_start_addr, buf_size_front,
				(unsigned int)buf_ptr );

		epb_rd_vir_ptr = vec_epb_instance->data.config.epb_vir_ptr;
		buf_ptr += buf_size_front;

		buf_size_back = rd_offset + es_size - \
						vec_epb_instance->data.config.epb_size;
		if (copy_to_user( (void __user*)buf_ptr, epb_rd_vir_ptr, buf_size_back ))
			vlog_error( "copy_to_user - src:0x%X/0x%X++0x%X --> dst:0x%X\n",
				(unsigned int)epb_rd_vir_ptr, es_start_addr, buf_size_back,
				(unsigned int)buf_ptr );

	}
	else {
		if (copy_to_user( (void __user*)buf_ptr, epb_rd_vir_ptr, es_size ))
			vlog_error( "copy_to_user - src:0x%X/0x%X++0x%X --> dst:0x%X\n",
				(unsigned int)epb_rd_vir_ptr, es_start_addr, es_size,
				(unsigned int)buf_ptr );
	}

	mutex_unlock( &vec_epb_lock );

	return true;
}

static bool _vec_epb_is_within_range( unsigned int start_addr,
									unsigned int end_addr,
									unsigned int target_addr )
{
	if (start_addr <= end_addr) {
		if ((target_addr > start_addr) && (target_addr <= end_addr))
			return true;
	}
	else {
		if ((target_addr > start_addr) || (target_addr <= end_addr))
			return true;
	}

	return false;
}

bool vec_epb_update_wraddr( void *vec_epb_id, unsigned int wr_addr_next )
{
	struct vec_epb_channel_node *vec_epb_instance = \
							(struct vec_epb_channel_node *)vec_epb_id;
	unsigned int wr_offset, wr_offset_next;
	unsigned int rd_offset;

	mutex_lock( &vec_epb_lock );

	wr_offset_next = wr_addr_next - vec_epb_instance->data.config.epb_phy_addr;
	if (wr_offset_next > vec_epb_instance->data.config.epb_size) {
		vlog_error( "invalid wr_addr_next:0x%X\n", wr_addr_next );
		mutex_unlock( &vec_epb_lock );
		return false;
	}

	if (wr_offset_next == vec_epb_instance->data.config.epb_size)
		wr_offset_next = 0;

	wr_offset = vec_epb_instance->data.status.wr_offset;
	rd_offset = vec_epb_instance->data.status.rd_offset;

	if (_vec_epb_is_within_range( wr_offset, wr_offset_next, rd_offset ) \
			== true) {
		vlog_error( "overwrite - write:0x%X, write_next:0x%X, read:0x%X\n",
				wr_offset, wr_offset_next, rd_offset );
		mutex_unlock( &vec_epb_lock );
		return false;
	}

	vec_epb_instance->data.status.wr_offset = wr_offset_next;

	vlog_print( VLOG_VENC_EPB, "epb:0x%X++0x%X, wr:0x%X, rd:0x%X\n",
		vec_epb_instance->data.config.epb_phy_addr,
		vec_epb_instance->data.config.epb_size,
		vec_epb_instance->data.status.wr_offset,
		vec_epb_instance->data.status.rd_offset );

	mutex_unlock( &vec_epb_lock );
	return true;
}

bool vec_epb_update_rdaddr( void *vec_epb_id, unsigned int rd_addr_next )
{
	struct vec_epb_channel_node *vec_epb_instance = \
							(struct vec_epb_channel_node *)vec_epb_id;
	unsigned int wr_offset;
	unsigned int rd_offset, rd_offset_next;

	mutex_lock( &vec_epb_lock );

	rd_offset_next = rd_addr_next - vec_epb_instance->data.config.epb_phy_addr;
	if (rd_offset_next > vec_epb_instance->data.config.epb_size) {
		vlog_error( "invalid rd_addr_next:0x%X\n", rd_offset_next );
		mutex_unlock( &vec_epb_lock );
		return false;
	}

	if (rd_offset_next == vec_epb_instance->data.config.epb_size)
		rd_offset_next = 0;

	wr_offset = vec_epb_instance->data.status.wr_offset;
	rd_offset = vec_epb_instance->data.status.rd_offset;

	if (_vec_epb_is_within_range( rd_offset, rd_offset_next, wr_offset ) == true) {
		if (rd_offset_next != wr_offset) {
			vlog_error( "overread - read:0x%X, read_next:0x%X, write:0x%X\n",
					rd_offset, rd_offset_next, wr_offset );
			mutex_unlock( &vec_epb_lock );
			return false;
		}
	}

	vec_epb_instance->data.status.rd_offset = rd_offset_next;

	vlog_print( VLOG_VENC_EPB, "epb:0x%X++0x%X, wr:0x%X, rd:0x%X\n",
		vec_epb_instance->data.config.epb_phy_addr,
		vec_epb_instance->data.config.epb_size,
		vec_epb_instance->data.status.wr_offset,
		vec_epb_instance->data.status.rd_offset );

	mutex_unlock( &vec_epb_lock );
	return true;
}

void vec_epb_get_buffer_status( void *vec_epb_id, unsigned int *epb_occupancy,
								unsigned int *epb_size )
{
	struct vec_epb_channel_node *vec_epb_instance = \
							(struct vec_epb_channel_node *)vec_epb_id;

	mutex_lock( &vec_epb_lock );

	if (vec_epb_instance->data.status.wr_offset >= \
			vec_epb_instance->data.status.rd_offset )
		*epb_occupancy = vec_epb_instance->data.status.wr_offset - \
						vec_epb_instance->data.status.rd_offset;
	else
		*epb_occupancy = vec_epb_instance->data.status.wr_offset + \
						vec_epb_instance->data.config.epb_size - \
						vec_epb_instance->data.status.rd_offset;

	*epb_size = vec_epb_instance->data.config.epb_size;

	mutex_unlock( &vec_epb_lock );
}

unsigned int vec_epb_is_available( void *vec_epb_id )
{
	struct vec_epb_channel_node *vec_epb_instance = \
							(struct vec_epb_channel_node *)vec_epb_id;
	unsigned int epb_occupancy;
	unsigned int epb_free_buf_size;

	mutex_lock( &vec_epb_lock );

	if (vec_epb_instance->data.status.wr_offset >= \
			vec_epb_instance->data.status.rd_offset )
		epb_occupancy = vec_epb_instance->data.status.wr_offset - \
					vec_epb_instance->data.status.rd_offset;
	else
		epb_occupancy = vec_epb_instance->data.status.wr_offset + \
					vec_epb_instance->data.config.epb_size - \
					vec_epb_instance->data.status.rd_offset;

	epb_free_buf_size = vec_epb_instance->data.config.epb_size - epb_occupancy;

	mutex_unlock( &vec_epb_lock );

	return epb_free_buf_size;
}

