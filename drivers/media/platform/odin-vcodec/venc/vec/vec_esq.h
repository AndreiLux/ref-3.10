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

#ifndef _VEC_ESQ_H_
#define _VEC_ESQ_H_

#include <linux/types.h>

enum vec_esq_pic_type
{
	VEC_ESQ_SEQ_HDR,
	VEC_ESQ_PIC_I,
	VEC_ESQ_PIC_P,
	VEC_ESQ_PIC_B,
	VEC_ESQ_EOS,
};

struct vec_esq_node
{
	/* output */
	unsigned int es_start_addr;
	unsigned int es_end_addr;
	unsigned int es_size;
	enum vec_esq_pic_type pic_type;
	unsigned long encode_time;	/*jiffies */

	/* input */
	unsigned long long timestamp;
	unsigned int uid;
	unsigned long input_time;	/* jiffies */
	unsigned long feed_time;	/* jiffies */
};

void vec_esq_init( void );
void vec_esq_cleanup( void );

void *vec_esq_open( unsigned int num_of_node, bool still_image );
void vec_esq_close( void *vec_esq_id );

bool vec_esq_push( void *vec_esq_id, struct vec_esq_node *esq_node );
bool vec_esq_pop( void *vec_esq_id, struct vec_esq_node *esq_node );

void vec_esq_node_info( void *vec_esq_id, unsigned int *num_of_es,
						unsigned int *last_es_size );

#endif /* #ifndef _VEC_ESQ_H_ */
