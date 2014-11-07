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

#ifndef _VCODEC_RATE_H_
#define _VCODEC_RATE_H_

#include <linux/types.h>

void vcodec_rate_init( void );
void vcodec_rate_cleanup( void );

void *vcodec_rate_open( void );
void vcodec_rate_close( void *vcodec_rate_id );

void vcodec_rate_reset_input( void *vcodec_rate_id );
void vcodec_rate_reset_codec_done( void *vcodec_rate_id );
void vcodec_rate_reset_timestamp( void *vcodec_rate_id );
unsigned long vcodec_rate_update_input(
				void *vcodec_rate_id, unsigned long event_time );
unsigned long vcodec_rate_update_codec_done(
				void *vcodec_rate_id, unsigned long event_time );
unsigned long vcodec_rate_update_timestamp_input(
				void *vcodec_rate_id, unsigned long long timestamp );
unsigned long vcodec_rate_update_timestamp_output(
				void *vcodec_rate_id, unsigned long long timestamp );

#endif /* #ifndef _VCODEC_RATE_H_ */

