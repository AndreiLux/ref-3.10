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

#ifndef _VEC_PICQ_H_
#define _VEC_PICQ_H_

#include <linux/types.h>

struct vec_picq_node
{
	unsigned int pic_phy_addr;
	unsigned char *pic_vir_ptr;
	unsigned int pic_size;
	void *private_in_meta;
	bool eos;
	unsigned long long timestamp;
	unsigned int uid;
	unsigned long input_time;	/*  jiffies */
	unsigned long feed_time;	/*  jiffies */
};

void vec_picq_init( void );
void vec_picq_cleanup( void );

void *vec_picq_open( void );
void vec_picq_close( void *vec_picq_id );

void vec_picq_debug_print_pic_phy_addr( void *vec_picq_id );
bool vec_picq_push_encoded( void *vec_picq_id,
							struct vec_picq_node *picq_data );
struct vec_picq_node *vec_picq_pop_encoded( void *vec_picq_id,
											unsigned int pic_phy_addr );
bool vec_picq_push_waiting( void *vec_picq_id,
							struct vec_picq_node *picq_data );
struct vec_picq_node *vec_picq_peep_waiting( void *vec_picq_id );
struct vec_picq_node *vec_picq_pop_waiting( void *vec_picq_id );
bool vec_picq_push_encoding( void *vec_picq_id,
							struct vec_picq_node *picq_data );
struct vec_picq_node *vec_picq_pop_encoding( void *vec_picq_id );

unsigned int vec_picq_waiting_occupancy( void *vec_picq_id );

#endif /* #ifndef _VEC_PICQ_H_ */

