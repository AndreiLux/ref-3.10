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

#include <linux/types.h>
#include "crg_hal.h"

#define NON_SECURE_PENDING_OFFSET 0xf800
#define NON_SECURE_MASK_OFFSET 0xf804
#define SOFT_RESET_OFFSET 0x1000

static volatile unsigned int _dss_reg_crg;

void dss_crg_hal_init(unsigned int base)
{
	_dss_reg_crg = base;
}

unsigned int dss_crg_hal_irq_pending_get(void)
{
	return *((unsigned int*)(_dss_reg_crg + NON_SECURE_PENDING_OFFSET));
}

void dss_crg_hal_irq_pending_clear(void)
{
	*((unsigned int*)(_dss_reg_crg + NON_SECURE_PENDING_OFFSET)) = 0xffffffff;
}

void dss_crg_hal_irq_mask_clear(void)
{
	*((unsigned int*)(_dss_reg_crg + NON_SECURE_MASK_OFFSET)) = 0;
}

void dss_crg_hal_irq_enable(enum dss_irq mask)
{
	volatile unsigned int masking = *((unsigned int*)(_dss_reg_crg + NON_SECURE_MASK_OFFSET));
	masking |= (0x1 << mask);
	*((unsigned int*)(_dss_reg_crg + NON_SECURE_MASK_OFFSET)) = masking;
}

void dss_crg_hal_irq_disable(enum dss_irq mask)
{
	volatile unsigned int masking = *((unsigned int*)(_dss_reg_crg + NON_SECURE_MASK_OFFSET));
	masking &= (~(0x1 << mask));
	*((unsigned int*)(_dss_reg_crg + NON_SECURE_MASK_OFFSET)) = masking;
}

void dss_crg_hal_reset(enum dss_reset reset)
{
	volatile unsigned int reg_reset = *((unsigned int*)(_dss_reg_crg + SOFT_RESET_OFFSET));
	barrier();
	reg_reset |= (0x1 << reset);
	*((unsigned int*)(_dss_reg_crg + SOFT_RESET_OFFSET)) =  reg_reset;
	reg_reset &= (~(0x1 << reset));
	*((unsigned int*)(_dss_reg_crg + SOFT_RESET_OFFSET)) =  reg_reset;
	barrier();
}
