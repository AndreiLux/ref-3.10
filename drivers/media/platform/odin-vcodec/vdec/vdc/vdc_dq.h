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

#ifndef _VDC_DQ_H_
#define _VDC_DQ_H_

#include <linux/types.h>

struct vdc_dq_node
{
	unsigned int dpb_addr;
	unsigned int sid;
	void *private_out_meta;
};

void vdc_dq_init( void );
void vdc_dq_cleanup( void );
void vdc_dq_reset( void *vdc_dq_id );

void *vdc_dq_open( unsigned int num_of_node );
void vdc_dq_close( void *vdc_dq_id );
struct vdc_dq_node *vdc_dq_pop_for_flush( void *vdc_dq_id );

bool vdc_dq_put_free_node( void *vdc_dq_,
							struct vdc_dq_node *dq_data );
struct vdc_dq_node *vdc_dq_get_free_node( void *vdc_dq_id );
bool vdc_dq_push_vcore( void *vdc_dq_id,
							struct vdc_dq_node *dq_data );
struct vdc_dq_node *vdc_dq_pop_vcore( void *vdc_dq_id,
							unsigned int dpb_addr );
bool vdc_dq_push_decoded( void *vdc_dq_id,
							struct vdc_dq_node *dq_data );
struct vdc_dq_node *vdc_dq_pop_decoded( void *vdc_dq_id,
							unsigned int dpb_addr );
bool vdc_dq_push_displaying( void *vdc_dq_id,
							struct vdc_dq_node *dq_data );
struct vdc_dq_node *vdc_dq_pop_displaying( void *vdc_dq_id,
							unsigned int dpb_addr );
bool vdc_dq_push_clear( void *vdc_dq_id,
							struct vdc_dq_node *dq_data );
struct vdc_dq_node *vdc_dq_pop_clear( void *vdc_dq_id );

void vdc_dq_debug_print_dpb_addr( void *vdc_dq_id );

unsigned int vdc_dq_displaying_occupancy( void *vdc_dq_id );
unsigned int vdc_dq_vcore_occupancy( void *vdc_dq_id );

#endif /* #ifndef _VDC_DQ_H_ */

