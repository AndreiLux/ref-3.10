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

#ifndef _VES_DRV_H_
#define _VES_DRV_H_

#include <linux/types.h>

enum ves_au_type
{
	VES_AU_SEQ_HDR,
	VES_AU_SEQ_END,
	VES_AU_PIC,
	VES_AU_PIC_I,
	VES_AU_PIC_P,
	VES_AU_PIC_B,
	VES_AU_UNKNOWN,
};

struct ves_report_au
{
	unsigned char *p_buf;
	unsigned int start_phy_addr;
	unsigned char *start_vir_ptr;
	unsigned int size;
	unsigned int end_phy_addr;
	void *private_in_meta;
	enum ves_au_type type;
	bool eos;
	unsigned long long timestamp;
	unsigned int uid;
	unsigned long input_time;	// jiffies
	unsigned int cpb_occupancy;
	unsigned int cpb_size;
};

struct ves_update_au
{
	unsigned char *es_addr;
	unsigned int es_size;
	void *private_in_meta;
	enum ves_au_type type;
	bool eos;
	unsigned long long timestamp;
	unsigned int uid;
};

void ves_init( void );
void ves_cleanup( void );

void *ves_open( unsigned int cpb_phy_addr, unsigned char *cpb_vir_ptr, unsigned int cpb_size,
				void *top_id, bool (*cb_report_au)( void *top_id, struct ves_report_au *report_au ) );
void ves_close( void *ves_id );
void ves_flush( void *ves_id );
bool ves_update_buffer( void *ves_id, struct ves_update_au *update_au, bool user_space_buf, bool ring_buffer, unsigned int align_bytes );
void ves_update_rdptr( void *ves_id, unsigned int rd_addr );
void ves_get_cbp_status( void *ves_id, unsigned int *cpb_occupancy, unsigned int *cpb_size );

#endif //#ifndef _VES_DRV_H_

