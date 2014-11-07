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

#ifndef _DSS_IRQ_H_
#define _DSS_IRQ_H_

#include "hal/crg_hal.h"

void dss_request_irq(enum dss_irq irq, void(*isr)(enum dss_irq irq, void *arg), void *arg);
void dss_free_irq(enum dss_irq irq);

bool dss_irq_init(unsigned int gic_irq);
void dss_irq_cleanup(void);

#endif /* #ifndef _DSS_IRQ_H_ */
