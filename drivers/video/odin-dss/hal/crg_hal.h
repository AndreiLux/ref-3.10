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

#ifndef _DSS_CRG_HAL_H_
#define _DSS_CRG_HAL_H_

enum dss_irq
{
	DSS_IRQ_SYNC0,
	DSS_IRQ_SYNC1,
	DSS_IRQ_SYNC2,
	DSS_IRQ_FMT,
	DSS_IRQ_XD,
	DSS_IRQ_RTT,
	DSS_IRQ_DIP0,
	DSS_IRQ_MIPI0,
	DSS_IRQ_DIP1,
	DSS_IRQ_MIPI1,
	DSS_IRQ_DIP2,
	DSS_IRQ_HDMI,
	DSS_IRQ_WAKEUP,
	DSS_IRQ_CABC0,
	DSS_IRQ_CABC1,
	DSS_IRQ_MAX_NO,
};

enum dss_reset
{
	DSS_RESET_VSCL0,
	DSS_RESET_VSCL1,
	DSS_RESET_VDMA,
	DSS_RESET_MXD,
	DSS_RESET_CABC,
	DSS_RESET_FOMATTER,
	DSS_RESET_GDMA0,
	DSS_RESET_GDMA1,
	DSS_RESET_GDMA2,
	DSS_RESET_OVERLAY,
	DSS_RESET_GSCL,
	DSS_RESET_ROTATOR,
	DSS_RESET_SYNC_GEN,
	DSS_RESET_DIP0,
	DSS_RESET_MIPI0,
	DSS_RESET_DIP1,
	DSS_RESET_MIPI1,
	DSS_RESET_GFX0,
	DSS_RESET_GFX1,
	DSS_RESET_DIP2,
	DSS_RESET_HDMI,
};

void dss_crg_hal_init(unsigned int base);

void dss_crg_hal_irq_enable(enum dss_irq mask);
void dss_crg_hal_irq_disable(enum dss_irq mask);
void dss_crg_hal_irq_mask_clear(void);
unsigned int dss_crg_hal_irq_pending_get(void);
void dss_crg_hal_irq_pending_clear(void);

void dss_crg_hal_reset(enum dss_reset reset);

#endif /* #ifndef _DSS_CRG_HAL_H_ */
