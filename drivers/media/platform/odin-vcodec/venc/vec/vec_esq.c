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

#include "vec_esq.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <media/odin/vcodec/vlog.h>

struct vec_esq_channel
{
	struct
	{
		unsigned int num_of_node;
		bool still_image;
	} config;

	struct
	{
		struct vec_esq_node *base_ptr;

		struct vec_esq_node *wr_ptr;
		struct vec_esq_node *rd_ptr;
	} queue;
};

struct vec_esq_channel_node
{
	struct list_head list;
	struct vec_esq_channel data;
};

struct mutex vec_esq_lock;
struct list_head vec_esq_instance_list;

void vec_esq_init( void )
{
	mutex_init( &vec_esq_lock );
	INIT_LIST_HEAD( &vec_esq_instance_list );
}

void vec_esq_cleanup( void )
{
}

void *vec_esq_open( unsigned int num_of_node, bool still_image )
{
	struct vec_esq_channel_node *vec_esq_instance;

	mutex_lock( &vec_esq_lock );

	vec_esq_instance = (struct vec_esq_channel_node *)vzalloc( \
							sizeof(struct vec_esq_channel_node));
	if (!vec_esq_instance)
	{
		vlog_error( "failed to allocate memory\n" );
		mutex_unlock( &vec_esq_lock );
		return (void *)NULL;
	}

	vec_esq_instance->data.config.num_of_node = num_of_node;
	vec_esq_instance->data.config.still_image = still_image;

	vec_esq_instance->data.queue.base_ptr = (struct vec_esq_node *)vzalloc( \
								sizeof(struct vec_esq_node)  * num_of_node);
	vec_esq_instance->data.queue.wr_ptr = \
								vec_esq_instance->data.queue.base_ptr;
	vec_esq_instance->data.queue.rd_ptr = \
								vec_esq_instance->data.queue.base_ptr;

	list_add_tail( &vec_esq_instance->list, &vec_esq_instance_list );

	mutex_unlock( &vec_esq_lock );

	return (void *)vec_esq_instance;
}

void vec_esq_close( void *vec_esq_id )
{
	struct vec_esq_channel_node *vec_esq_instance = \
							(struct vec_esq_channel_node *)vec_esq_id;

	mutex_lock( &vec_esq_lock );

	vfree( vec_esq_instance->data.queue.base_ptr );
	list_del( &vec_esq_instance->list );
	vfree( vec_esq_instance );

	mutex_unlock( &vec_esq_lock );
}

bool vec_esq_push( void *vec_esq_id, struct vec_esq_node *esq_node )
{
	struct vec_esq_channel_node *vec_esq_instance = \
							(struct vec_esq_channel_node *)vec_esq_id;

	mutex_lock( &vec_esq_lock );

	if (esq_node->es_size == 0) {
		vlog_warning( "es size = 0, type: %d, buf:0x%X+0x%X=0x%X\n",
				esq_node->es_start_addr, esq_node->pic_type,
				esq_node->es_size, esq_node->es_end_addr );
	}

	if ((vec_esq_instance->data.queue.wr_ptr + 1) == \
			vec_esq_instance->data.queue.rd_ptr)
	{
		vlog_error( "free esq overflow base:0x%X, wr:0x%X, rd:0x%X\n",
				(unsigned int)vec_esq_instance->data.queue.base_ptr,
				(unsigned int)vec_esq_instance->data.queue.wr_ptr,
				(unsigned int)vec_esq_instance->data.queue.rd_ptr );
		mutex_unlock( &vec_esq_lock );
		return false;
	}

	*vec_esq_instance->data.queue.wr_ptr = *esq_node;

	if ((++vec_esq_instance->data.queue.wr_ptr) == \
			(vec_esq_instance->data.queue.base_ptr + \
			vec_esq_instance->data.config.num_of_node))
		vec_esq_instance->data.queue.wr_ptr = \
									 vec_esq_instance->data.queue.base_ptr;

	mutex_unlock( &vec_esq_lock );
	return true;
}

bool vec_esq_pop( void *vec_esq_id, struct vec_esq_node *esq_node )
{
	struct vec_esq_channel_node *vec_esq_instance = \
							(struct vec_esq_channel_node *)vec_esq_id;

	mutex_lock( &vec_esq_lock );

	if (vec_esq_instance->data.queue.rd_ptr == \
			vec_esq_instance->data.queue.wr_ptr)
	{
		vlog_error( "free esq base:0x%X, wr:0x%X, rd:0x%X\n",
				(unsigned int)vec_esq_instance->data.queue.base_ptr,
				(unsigned int)vec_esq_instance->data.queue.wr_ptr,
				(unsigned int)vec_esq_instance->data.queue.rd_ptr );
		mutex_unlock( &vec_esq_lock );
		return false;
	}

	*esq_node = *vec_esq_instance->data.queue.rd_ptr;

	if ((++vec_esq_instance->data.queue.rd_ptr) == \
			(vec_esq_instance->data.queue.base_ptr +\
			vec_esq_instance->data.config.num_of_node))
		vec_esq_instance->data.queue.rd_ptr = \
			vec_esq_instance->data.queue.base_ptr;

	mutex_unlock( &vec_esq_lock );
	return true;
}

void vec_esq_node_info( void *vec_esq_id, unsigned int *num_of_es,
						unsigned int *last_es_size )
{
	struct vec_esq_channel_node *vec_esq_instance = \
							(struct vec_esq_channel_node *)vec_esq_id;
	unsigned int esq_base;
	unsigned int wr_addr;
	unsigned int rd_addr;
	unsigned int node_size;
	unsigned int wr_index;
	unsigned int rd_index;

	mutex_lock( &vec_esq_lock );

	esq_base = (unsigned int)vec_esq_instance->data.queue.base_ptr;
	wr_addr = (unsigned int)vec_esq_instance->data.queue.wr_ptr;
	rd_addr = (unsigned int)vec_esq_instance->data.queue.rd_ptr;
	node_size = sizeof(struct vec_esq_node);
	wr_index = (wr_addr - esq_base) / node_size;
	rd_index = (rd_addr - esq_base) / node_size;

	*num_of_es = (wr_index >= rd_index) ? wr_index - rd_index : \
				wr_index + vec_esq_instance->data.config.num_of_node - rd_index;

	if (*num_of_es) {
		struct vec_esq_node *esq_node = NULL;
		struct vec_esq_node *esq_node_next = NULL;

		esq_node = vec_esq_instance->data.queue.rd_ptr;
		*last_es_size = esq_node->es_size;

		while (*last_es_size == 0) {
			vlog_warning( "free esq base:0x%X, wr:0x%X, rd:0x%X - idx:0x%X\n",
					(unsigned int)vec_esq_instance->data.queue.base_ptr,
					(unsigned int)vec_esq_instance->data.queue.wr_ptr,
					(unsigned int)vec_esq_instance->data.queue.rd_ptr,
					(unsigned int)esq_node );

			esq_node_next = esq_node + 1;
			if (esq_node_next == (vec_esq_instance->data.queue.base_ptr + \
							vec_esq_instance->data.config.num_of_node))
				esq_node_next = vec_esq_instance->data.queue.base_ptr;

			if (esq_node_next == vec_esq_instance->data.queue.wr_ptr)
				break;

			esq_node = esq_node_next;

			*last_es_size += esq_node->es_size;
		}

		if (vec_esq_instance->data.config.still_image == true) {
			while (esq_node->pic_type == VEC_ESQ_SEQ_HDR) {
				esq_node_next = esq_node + 1;
				if (esq_node_next == (vec_esq_instance->data.queue.base_ptr + \
								vec_esq_instance->data.config.num_of_node))
					esq_node_next = vec_esq_instance->data.queue.base_ptr;

				if (esq_node_next == vec_esq_instance->data.queue.wr_ptr)
					break;

				esq_node = esq_node_next;

				vlog_info( "merge pic type:%d, epb:0x%X++0x%X=0x%X\n",
						esq_node->pic_type, esq_node->es_start_addr,
						esq_node->es_size, esq_node->es_end_addr);

				*last_es_size += esq_node->es_size;
			}

			if (esq_node->pic_type == VEC_ESQ_SEQ_HDR) {
				*num_of_es = 0;
				*last_es_size = 0;
			}
		}
	}
	else {
		*last_es_size = 0;
	}

	mutex_unlock( &vec_esq_lock );
}


