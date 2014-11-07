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

#ifndef _VES_CPB_H_
#define _VES_CPB_H_

#include <linux/types.h>

void ves_cpb_init( void );
void ves_cpb_cleanup( void );

void *ves_cpb_open( unsigned int cpb_phy_addr, unsigned char *cpb_vir_ptr,
					unsigned int cpb_size );
void ves_cpb_close( void *ves_cpb_id );
void ves_cpb_flush( void *ves_cpb_id );
bool ves_cpb_write( void *ves_cpb_id,
					unsigned char *buf_addr, unsigned int buf_size, bool user_space_buf,
					bool ring_buffer, unsigned int align_bytes,
					unsigned int *start_phy_addr, unsigned char **start_vir_ptr,
					unsigned int *buf_modified_size, unsigned int *end_phy_addr );
bool ves_cpb_get_wrptr( void *ves_cpb_id,
						unsigned int *wr_phy_addr,
						unsigned char **wr_vir_ptr);
bool ves_cpb_update_wrptr( void *ves_cpb_id, unsigned int wr_addr );
bool ves_cpb_update_rdptr( void *ves_cpb_id, unsigned int rd_addr );
void ves_cpb_get_cbp_status( void *ves_cpb_id, unsigned int *cpb_occupancy,
								unsigned int *cpb_size );

#endif /*#ifndef _VES_CPB_H_*/

