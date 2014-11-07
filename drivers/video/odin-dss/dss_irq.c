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

#include <linux/interrupt.h>

#include "hal/crg_hal.h"
#include "dss_irq.h"

struct dss_irq_db
{
	enum dss_irq irq;
	void *arg;
	void (*isr)(enum dss_irq irq, void *arg);
};
static struct dss_irq_db irq_db[DSS_IRQ_MAX_NO];

static irqreturn_t _dss_gic_isr(int irq, void *dev)
{
	enum dss_irq index ;
	unsigned int pending;

	pending = dss_crg_hal_irq_pending_get();

	for (index=0; index<DSS_IRQ_MAX_NO; index++) {
		if (pending & (1<<index)) {
			if (irq_db[index].isr) 
				irq_db[index].isr(index, irq_db[index].arg);
			else
				pr_warn("%d intr occur. but no isr\n", index);
		}
	}

	dss_crg_hal_irq_pending_clear();

	return IRQ_HANDLED;
}

void dss_request_irq(enum dss_irq irq, void(*isr)(enum dss_irq irq, void *arg), void *arg)
{
	if (isr == NULL)
		return;

	if (irq >= DSS_IRQ_MAX_NO)
		return;

	irq_db[irq].arg = arg;
	irq_db[irq].isr = isr;

	dss_crg_hal_irq_enable(irq);
}

void dss_free_irq(enum dss_irq irq)
{
	if (irq >= DSS_IRQ_MAX_NO)
		return;

	irq_db[irq].isr = NULL;

	dss_crg_hal_irq_disable(irq);
}

bool dss_irq_init(unsigned int gic_irq)
{
	memset(irq_db, 0, sizeof(irq_db));

	dss_crg_hal_irq_mask_clear();
	dss_crg_hal_irq_pending_clear();

	if (request_irq(gic_irq, _dss_gic_isr, 0, "dss gic isr", NULL) < 0) {
		pr_err("[dss_irq] request_irq failed\n");
		return false;
	}

	return true;
}

void dss_irq_cleanup(void)
{
}
