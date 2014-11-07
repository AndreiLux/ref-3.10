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

#ifndef _VCODEC_TIMER_H_
#define _VCODEC_TIMER_H_

#include <linux/types.h>

void *vcodec_timer_open( void *vcodec_id,
						bool (*feed_timer)(void *vcodec_id),
						void (*expire_watchdog_timer)(void *vcodec_id) );
void vcodec_timer_close( void *vcodec_timer_id );

void vcodec_update_timer_status( void *vcodec_timer_id );

void vcodec_timer_init( void );
void vcodec_timer_cleanup( void );

#endif /*#ifndef _VCODEC_TIMER_H_*/

