/*
 * arch/arm/mach-odin/odin_irq.c
 *
 * ODIN IRQ
 *
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/irqchip.h>

#include <asm/irq.h>

#include <linux/platform_data/gpio-odin.h>

int odin_gic_irq_wake(struct irq_data *d, unsigned int on)
{
	int ret = -ENXIO;
	int irq = d->irq;
	struct irq_desc *desc = irq_to_desc(irq);

	if(!desc)
		return -EINVAL;

	if(on) {
		if(desc->action) {
			desc->action->flags |= IRQF_NO_SUSPEND;
			} else {
			pr_info("No irq action chain!!\n");
			}
		ret = 0;
	} else {
		if(desc->action) {
			desc->action->flags &= ~IRQF_NO_SUSPEND;
			} else {
			pr_info("No irq action chain!!\n");
			}
		ret = 0;
	}

	return ret;
}

void odin_gic_irq_mask(struct irq_data *d)
{
	if (is_gic_direct_irq(d->irq) == 1)
		odin_gpio_smsgic_irq_disable(d->irq);
}

void odin_gic_irq_unmask(struct irq_data *d)
{
	if (is_gic_direct_irq(d->irq) == 1)
		odin_gpio_smsgic_irq_enable(d->irq);
}

void __init odin_irq_init(void)
{
	irqchip_init();

	gic_arch_extn.irq_set_wake = odin_gic_irq_wake;
}
