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

#ifndef _VEC_EPB_H_
#define _VEC_EPB_H_

#include <linux/types.h>

void vec_epb_init( void );
void vec_epb_cleanup( void );

void *vec_epb_open( unsigned int epb_phy_addr, unsigned char *epb_vir_ptr,
					unsigned int epb_size );
void vec_epb_close( void *vec_epb_id );
bool vec_epb_read( void *vec_epb_id,
						unsigned int es_start_addr, unsigned int es_size,
						unsigned int es_end_addr,
						char *buf_ptr, unsigned int buf_size );

bool vec_epb_update_wraddr( void *vec_epb_id, unsigned int wr_addr_next );
bool vec_epb_update_rdaddr( void *vec_epb_id, unsigned int rd_addr_next );
void vec_epb_get_buffer_status( void *vec_epb_id, unsigned int *epb_occupancy,
								unsigned int *epb_size );
unsigned int vec_epb_is_available( void *vec_epb_id );

#endif /* #ifndef _VEC_EPB_H_ */

