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

#include "ves_cpb.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <asm/string.h>

#include <media/odin/vcodec/vlog.h>

struct ves_cpb_channel
{
	struct
	{
		unsigned int cpb_phy_addr;
		unsigned char *cpb_vir_ptr;
		unsigned int cpb_size;
	} config;

	struct
	{
		unsigned int wr_offset;
		unsigned int rd_offset;
	} status;

	struct
	{
		bool ring_buffer;
		unsigned int align_bytes;
	} debug;
};

struct ves_cpb_channel_node
{
	struct list_head list;
	struct ves_cpb_channel data;
};

struct mutex ves_cpb_lock;
struct list_head ves_cpb_instance_list;

void ves_cpb_init( void )
{
	mutex_init( &ves_cpb_lock );
	INIT_LIST_HEAD( &ves_cpb_instance_list );
}

void ves_cpb_cleanup( void )
{
}

void *ves_cpb_open( unsigned int cpb_phy_addr, unsigned char *cpb_vir_ptr,
					unsigned int cpb_size )
{
	struct ves_cpb_channel_node *ves_cpb_instance;

	mutex_lock( &ves_cpb_lock );

	ves_cpb_instance = (struct ves_cpb_channel_node *)vzalloc( \
									sizeof(struct ves_cpb_channel_node));
	if (!ves_cpb_instance) {
		vlog_error( "failed to allocate memory\n" );
		mutex_unlock( &ves_cpb_lock );
		return (void *)NULL;
	}

	ves_cpb_instance->data.config.cpb_phy_addr = cpb_phy_addr;
	ves_cpb_instance->data.config.cpb_vir_ptr = cpb_vir_ptr;
	ves_cpb_instance->data.config.cpb_size = cpb_size;

	ves_cpb_instance->data.status.wr_offset = 0;
	ves_cpb_instance->data.status.rd_offset = 0;

	ves_cpb_instance->data.debug.ring_buffer = true;
	ves_cpb_instance->data.debug.align_bytes = 0xFFFFFFFF;

	list_add_tail( &ves_cpb_instance->list, &ves_cpb_instance_list );

	mutex_unlock( &ves_cpb_lock );

	return (void *)ves_cpb_instance;
}

void ves_cpb_close( void *ves_cpb_id )
{
	struct ves_cpb_channel_node *ves_cpb_instance = \
								(struct ves_cpb_channel_node *)ves_cpb_id;

	mutex_lock( &ves_cpb_lock );

	list_del( &ves_cpb_instance->list );
	vfree( ves_cpb_instance );

	mutex_unlock( &ves_cpb_lock );
}

void ves_cpb_flush( void *ves_cpb_id )
{
	struct ves_cpb_channel_node *ves_cpb_instance = \
								(struct ves_cpb_channel_node *)ves_cpb_id;
	unsigned int wr_offset = ves_cpb_instance->data.status.wr_offset;
	unsigned int rd_offset = ves_cpb_instance->data.status.rd_offset;
	unsigned char *cpb_rd_vir_ptr;

	mutex_lock( &ves_cpb_lock );

	if (ves_cpb_instance->data.config.cpb_vir_ptr) {
		cpb_rd_vir_ptr = ves_cpb_instance->data.config.cpb_vir_ptr + rd_offset ;

		if (wr_offset >= rd_offset) {
			memset( cpb_rd_vir_ptr, 0x0, (wr_offset - rd_offset) );
		}
		else {
			memset( cpb_rd_vir_ptr, 0x0,
					(ves_cpb_instance->data.config.cpb_size - rd_offset));
			if (wr_offset) {
				cpb_rd_vir_ptr = ves_cpb_instance->data.config.cpb_vir_ptr;
				memset( cpb_rd_vir_ptr, 0x0, wr_offset );
			}
		}
	}

	mutex_unlock( &ves_cpb_lock );
}

static bool _ves_cpb_is_within_range( unsigned int start_addr,
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

static bool _ves_cpb_check_overflow(
					struct ves_cpb_channel_node *ves_cpb_instance,
					unsigned int buf_modified_size, bool ring_buffer,
					unsigned int wr_offset, unsigned int rd_offset,
					unsigned int *wr_offset_start, unsigned int *wr_offset_end )
{
	if ((wr_offset + buf_modified_size) >= \
			ves_cpb_instance->data.config.cpb_size) {
		if (ring_buffer == true) {
			*wr_offset_start = wr_offset;
			*wr_offset_end = (wr_offset + buf_modified_size) \
							- ves_cpb_instance->data.config.cpb_size;
		}
		else {
			*wr_offset_start = 0;
			*wr_offset_end = buf_modified_size;
		}
	}
	else {
		*wr_offset_start = wr_offset;
		*wr_offset_end = wr_offset + buf_modified_size;
	}

	if (_ves_cpb_is_within_range( wr_offset, *wr_offset_end, rd_offset ) \
				== true) {
		vlog_error( "overflow - write:0x%X, size:0x%X, read:0x%X\n",
				wr_offset, buf_modified_size, rd_offset );
		return true;
	}

	return false;
}

bool ves_cpb_write( void *ves_cpb_id,
					unsigned char *buf_addr, unsigned int buf_size,
					bool user_space_buf,
					bool ring_buffer, unsigned int align_bytes,
					unsigned int *start_phy_addr,
					unsigned char **start_vir_ptr,
					unsigned int *buf_modified_size,
					unsigned int *end_phy_addr )
{
	struct ves_cpb_channel_node *ves_cpb_instance = \
							(struct ves_cpb_channel_node *)ves_cpb_id;
	unsigned int wr_offset, wr_offset_start, wr_offset_end, \
				wr_offset_aligned_end;
	unsigned int rd_offset;
	unsigned int cpb_wr_phy_addr;
	unsigned char *cpb_wr_vir_ptr;

	mutex_lock( &ves_cpb_lock );

	if (buf_size > ves_cpb_instance->data.config.cpb_size) {
		vlog_error( "big au size:%u\n", buf_size );
		mutex_unlock( &ves_cpb_lock );
		return false;
	}
	else if (buf_size == 0) {
		vlog_warning( "au size:%u\n", buf_size );
	}

	if ((ves_cpb_instance->data.debug.ring_buffer != ring_buffer) ||
		(ves_cpb_instance->data.debug.align_bytes != align_bytes)) {
		vlog_warning( "ring buffer:%d-->%d, aligned bytes:%u-->%u\n",
				ves_cpb_instance->data.debug.ring_buffer, ring_buffer,
				ves_cpb_instance->data.debug.align_bytes, align_bytes );
	}
	ves_cpb_instance->data.debug.ring_buffer = ring_buffer;
	ves_cpb_instance->data.debug.align_bytes = align_bytes;

	*buf_modified_size = buf_size;
	/* check buffer size for alinged mode */
	if (align_bytes) {
		if (buf_size % align_bytes) {
			*buf_modified_size = ((buf_size / align_bytes) + 1) * align_bytes;
		}
	}

	wr_offset = ves_cpb_instance->data.status.wr_offset;
	rd_offset = ves_cpb_instance->data.status.rd_offset;

	/* check buffer overflow*/
	if (_ves_cpb_check_overflow( ves_cpb_instance, *buf_modified_size,
				ring_buffer,
				wr_offset, rd_offset,
				&wr_offset_start, &wr_offset_aligned_end ) == true) {
		mutex_unlock( &ves_cpb_lock );
		return false;
	}

	/* secure buffer mode */
	if (ves_cpb_instance->data.config.cpb_vir_ptr == NULL) {
		*start_phy_addr = ves_cpb_instance->data.config.cpb_phy_addr + \
						  wr_offset_start;
		*start_vir_ptr = NULL;
		*end_phy_addr = ves_cpb_instance->data.config.cpb_phy_addr + \
						wr_offset_aligned_end;
		mutex_unlock( &ves_cpb_lock );
		return true;
	}

	/* pad  buffer end */
	if (wr_offset > wr_offset_start) {
		unsigned int pad_size;

		if (wr_offset_start != 0) {
			vlog_error( "write:0x%X, start/end:0x%X/0x%X, read:0x%X\n",
					wr_offset,
					wr_offset_start, wr_offset_aligned_end, rd_offset );
			mutex_unlock( &ves_cpb_lock );
			return false;
		}

		cpb_wr_vir_ptr = ves_cpb_instance->data.config.cpb_vir_ptr + wr_offset;
		pad_size = ves_cpb_instance->data.config.cpb_size - wr_offset;

		memset( cpb_wr_vir_ptr, 0x0, pad_size );
	}

	cpb_wr_phy_addr = ves_cpb_instance->data.config.cpb_phy_addr + \
					  wr_offset_start;
	cpb_wr_vir_ptr = ves_cpb_instance->data.config.cpb_vir_ptr + \
					 wr_offset_start;

	/* write user space date to cpb */
	if ((wr_offset_start + buf_size) > ves_cpb_instance->data.config.cpb_size) {
		unsigned int buf_size_front;
		unsigned int buf_size_back;

		buf_size_front = ves_cpb_instance->data.config.cpb_size - \
							wr_offset_start;
		if (user_space_buf == true) {
			if (copy_from_user( cpb_wr_vir_ptr, buf_addr, buf_size_front ))
				vlog_error(
					"copy_from_user - src:0x%X++0x%X --> dst:0x%X/0x%X\n",
					(unsigned int)buf_addr, buf_size_front, cpb_wr_phy_addr,
					(unsigned int)cpb_wr_vir_ptr );
		}
		else {
			memcpy( cpb_wr_vir_ptr, buf_addr, buf_size_front );
		}

		cpb_wr_vir_ptr = ves_cpb_instance->data.config.cpb_vir_ptr;
		buf_addr += buf_size_front;

		buf_size_back = wr_offset_start + buf_size - \
							ves_cpb_instance->data.config.cpb_size;
		if (user_space_buf == true) {
			if (copy_from_user( cpb_wr_vir_ptr, buf_addr, buf_size_back ))
				vlog_error(
					"copy_from_user - src:0x%X++0x%X --> dst:0x%X/0x%X\n",
					(unsigned int)buf_addr, buf_size_back, cpb_wr_phy_addr,
					(unsigned int)cpb_wr_vir_ptr );
		}
		else {
			memcpy( cpb_wr_vir_ptr, buf_addr, buf_size_back );
		}

		cpb_wr_vir_ptr = ves_cpb_instance->data.config.cpb_vir_ptr + \
						 buf_size_back;
		wr_offset_end = buf_size_back;
	}
	else {
		if (user_space_buf == true) {
			if (copy_from_user( cpb_wr_vir_ptr, buf_addr, buf_size ))
				vlog_error(
					"copy_from_user - src:0x%X++0x%X --> dst:0x%X/0x%X\n",
					(unsigned int)buf_addr, buf_size, cpb_wr_phy_addr,
					(unsigned int)cpb_wr_vir_ptr );
		}
		else {
			memcpy( cpb_wr_vir_ptr, buf_addr, buf_size );
		}

		cpb_wr_vir_ptr += buf_size;
		wr_offset_end = wr_offset_start + buf_size;
	}

	/* pad au end */
	if (wr_offset_end > wr_offset_aligned_end) {
		memset( cpb_wr_vir_ptr, 0x0,
				(ves_cpb_instance->data.config.cpb_size - wr_offset_end) );

		if (wr_offset_aligned_end > 0) {
			cpb_wr_vir_ptr = ves_cpb_instance->data.config.cpb_vir_ptr;
			memset( cpb_wr_vir_ptr, 0x0, wr_offset_aligned_end );
		}
	}
	else if (wr_offset_end < wr_offset_aligned_end) {
		memset( cpb_wr_vir_ptr, 0x0, (wr_offset_aligned_end - wr_offset_end) );
	}

	*start_phy_addr = ves_cpb_instance->data.config.cpb_phy_addr + \
					  wr_offset_start;
	*start_vir_ptr = ves_cpb_instance->data.config.cpb_vir_ptr + \
					 wr_offset_start;
	*end_phy_addr = ves_cpb_instance->data.config.cpb_phy_addr + \
					wr_offset_aligned_end;

	mutex_unlock( &ves_cpb_lock );
	return true;
}

bool ves_cpb_get_wrptr( void *ves_cpb_id,
						unsigned int *wr_phy_addr,
						unsigned char **wr_vir_ptr)
{
	struct ves_cpb_channel_node *ves_cpb_instance = \
						(struct ves_cpb_channel_node *)ves_cpb_id;

	mutex_lock( &ves_cpb_lock );

	*wr_phy_addr = ves_cpb_instance->data.config.cpb_phy_addr + \
					  ves_cpb_instance->data.status.wr_offset;
	*wr_vir_ptr = ves_cpb_instance->data.config.cpb_vir_ptr + \
					 ves_cpb_instance->data.status.wr_offset;

	mutex_unlock( &ves_cpb_lock );
	return true;
}

bool ves_cpb_update_wrptr( void *ves_cpb_id, unsigned int wr_addr )
{
	struct ves_cpb_channel_node *ves_cpb_instance = \
						(struct ves_cpb_channel_node *)ves_cpb_id;
	unsigned int wr_offset, wr_offset_next;
	unsigned int rd_offset;

	mutex_lock( &ves_cpb_lock );

	wr_offset_next = wr_addr - ves_cpb_instance->data.config.cpb_phy_addr;
	if (wr_offset_next > ves_cpb_instance->data.config.cpb_size) {
		vlog_error( "invalid write addr:0x%X\n", wr_addr );
		mutex_unlock( &ves_cpb_lock );
		return false;
	}

	if (wr_offset_next == ves_cpb_instance->data.config.cpb_size)
		wr_offset_next = 0;

	wr_offset = ves_cpb_instance->data.status.wr_offset;
	rd_offset = ves_cpb_instance->data.status.rd_offset;

	if (_ves_cpb_is_within_range( wr_offset, wr_offset_next, rd_offset ) == \
			true) {
		vlog_error( "overwrite - write:0x%X, write_next:0x%X, read:0x%X\n",
				wr_offset, wr_offset_next, rd_offset );
		mutex_unlock( &ves_cpb_lock );
		return false;
	}

	ves_cpb_instance->data.status.wr_offset = wr_offset_next;

/*
	vlog_print( VLOG_VDEC_VES, "cpb:0x%X++0x%X, wr:0x%X, rd:0x%X\n",
		ves_cpb_instance->data.config.cpb_phy_addr,
		ves_cpb_instance->data.config.cpb_size,
		ves_cpb_instance->data.status.wr_offset,
		ves_cpb_instance->data.status.rd_offset );
*/
	mutex_unlock( &ves_cpb_lock );
	return true;
}

bool ves_cpb_update_rdptr( void *ves_cpb_id, unsigned int rd_addr )
{
	struct ves_cpb_channel_node *ves_cpb_instance = \
					(struct ves_cpb_channel_node *)ves_cpb_id;
	unsigned int wr_offset;
	unsigned int rd_offset, rd_offset_next;

	mutex_lock( &ves_cpb_lock );

	rd_offset_next = rd_addr - ves_cpb_instance->data.config.cpb_phy_addr;
	if (rd_offset_next > ves_cpb_instance->data.config.cpb_size) {
		vlog_error( "invalid read addr:0x%X\n", rd_addr );
		mutex_unlock( &ves_cpb_lock );
		return false;
	}

	if (rd_offset_next == ves_cpb_instance->data.config.cpb_size)
		rd_offset_next = 0;

	wr_offset = ves_cpb_instance->data.status.wr_offset;
	rd_offset = ves_cpb_instance->data.status.rd_offset;

	if (_ves_cpb_is_within_range( rd_offset, rd_offset_next, wr_offset ) == \
			true) {
		if (rd_offset_next != wr_offset) {
			vlog_error( "overread - read:0x%X, read_next:0x%X, write:0x%X\n",
					rd_offset, rd_offset_next, wr_offset );
			mutex_unlock( &ves_cpb_lock );
			return false;
		}
	}

	ves_cpb_instance->data.status.rd_offset = rd_offset_next;

/*
	vlog_print( VLOG_VDEC_VES, "cpb:0x%X++0x%X, wr:0x%X, rd:0x%X\n",
		ves_cpb_instance->data.config.cpb_phy_addr,
		ves_cpb_instance->data.config.cpb_size,
		ves_cpb_instance->data.status.wr_offset,
		ves_cpb_instance->data.status.rd_offset );
*/
	mutex_unlock( &ves_cpb_lock );
	return true;
}

void ves_cpb_get_cbp_status( void *ves_cpb_id, unsigned int *cpb_occupancy,
							unsigned int *cpb_size )
{
	struct ves_cpb_channel_node *ves_cpb_instance = \
									(struct ves_cpb_channel_node *)ves_cpb_id;

	mutex_lock( &ves_cpb_lock );

	if (ves_cpb_instance->data.status.wr_offset >= \
			ves_cpb_instance->data.status.rd_offset )
		*cpb_occupancy = ves_cpb_instance->data.status.wr_offset \
						- ves_cpb_instance->data.status.rd_offset;
	else
		*cpb_occupancy = ves_cpb_instance->data.status.wr_offset + \
						ves_cpb_instance->data.config.cpb_size - \
						ves_cpb_instance->data.status.rd_offset;

	*cpb_size = ves_cpb_instance->data.config.cpb_size;

	mutex_unlock( &ves_cpb_lock );
}


