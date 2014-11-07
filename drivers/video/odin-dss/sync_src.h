/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Younghyun Jo <younghyun.jo@lge.com>
 * Youngki Lyu <youngki.lyu@lge.com>
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

#ifndef _SYNC_ISR_H_
#define _SYNC_ISR_H_

#include <linux/types.h>
#include <linux/platform_device.h>
#include <video/odin-dss/dss_types.h>
#include "hal/dss_hal_ch_id.h"

#define SYNC_INVALID_TICK	0x80000000

enum sync_ch_id sync_enable(enum sync_ch_id sync_ch, unsigned long frequency); /* if frequency  is 0, it`s depends on DIP */
bool sync_disable(enum sync_ch_id sync_ch);
bool sync_is_enable(enum sync_ch_id sync_ch);

bool sync_isr_register(enum sync_ch_id sync_ch, void *arg, void (*isr)(enum sync_ch_id sync_ch, void *arg));
bool sync_isr_unregister(enum sync_ch_id sync_ch, void (*isr)(enum sync_ch_id sync_ch, void *arg));

unsigned int sync_global_tick_get(void);
bool sync_global_isr_register(void (*isr)(enum sync_ch_id sync_ch));
void sync_global_isr_unregister(void (*isr)(enum sync_ch_id sync_ch));

bool sync_init(struct platform_device *pdev[]);
void sync_cleanup(void);

#endif /* #ifndef _SYNC_ISR_H_ */
