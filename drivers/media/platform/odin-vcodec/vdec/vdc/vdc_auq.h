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

#ifndef _VDC_AUQ_H_
#define _VDC_AUQ_H_

#include <linux/types.h>

enum vdc_au_type
{
	VDC_AU_SEQ,
	VDC_AU_PIC,
	VDC_AU_UNKNOWN,
};

struct vdc_auq_node
{
	unsigned char *p_buf;
	enum vdc_au_type type;
	unsigned int start_phy_addr;
	unsigned char *start_vir_ptr;
	unsigned int size;
	unsigned int end_phy_addr;
	void *private_in_meta;
	bool eos;
	unsigned long long timestamp;
	unsigned int uid;
	unsigned long input_time;	/* jiffies */
	unsigned int flush_age;
	unsigned int chunk_id;
};

void vdc_auq_init( void );
void vdc_auq_cleanup( void );

void *vdc_auq_open( void );
void vdc_auq_close( void *vdc_auq_id );

bool vdc_auq_push_waiting( void *vdc_auq_id, struct vdc_auq_node *auq_data );
bool vdc_auq_peep_waiting( void *vdc_auq_id, struct vdc_auq_node *auq_data );
bool vdc_auq_pop_waiting( void *vdc_auq_id, struct vdc_auq_node *auq_data );
bool vdc_auq_move_waiting_to_decoding( void *vdc_auq_id,
											struct vdc_auq_node *auq_data );
bool vdc_auq_pop_decoding( void *vdc_auq_id, struct vdc_auq_node *auq_data );

unsigned int vdc_auq_get_latest_wraddr( void *vdc_auq_id );
unsigned int vdc_auq_occupancy( void *vdc_auq_id );

#endif /*#ifndef _VDC_AUQ_H_ */
