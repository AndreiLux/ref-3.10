/*
 *  IRQ support for Dialog D2260 PMIC
 *
 *  Copyright (C) 2013 Dialog Semiconductor Ltd.
 *  Author: William Seo <william.seo@diasemi.com>
 *  Author: Tony Olech <anthony.olech@diasemi.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/bug.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/regmap.h>

#include <asm/gpio.h>

#include <linux/mfd/d2260/pmic.h>
#include <linux/mfd/d2260/registers.h>
#include <linux/mfd/d2260/rtc.h>
#include <linux/mfd/d2260/core.h>

#define D2260_NUM_IRQ_MASK_REGS	3

#ifdef CONFIG_MACH_BCM2708
#define D2260_IRQ_FLAGS (IRQ_TYPE_EDGE_FALLING | IRQF_ONESHOT)
#else
#define D2260_IRQ_FLAGS (IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_SHARED | IRQF_NO_SUSPEND)
#endif

static struct regmap_irq d2260_irqs[] = {
	[D2260_IRQ_EVF] = {
		.reg_offset = 0,
		.mask = D2260_M_VF_MASK,
	},
	[D2260_IRQ_ETBAT2] = {
		.reg_offset = 0,
		.mask = D2260_M_TBAT2_MASK,
	},
	[D2260_IRQ_EVDD_LOW] = {
		.reg_offset = 0,
		.mask = D2260_M_VDD_LOW_MASK,
	},
	[D2260_IRQ_EVDD_MON] = {
		.reg_offset = 0,
		.mask = D2260_M_VDD_MON_MASK,
	},
	[D2260_IRQ_EALARM] = {
		.reg_offset = 0,
		.mask = D2260_M_ALARM_MASK,
	},
	[D2260_IRQ_ESEQRDY] = {
		.reg_offset = 0,
		.mask = D2260_M_SEQ_RDY_MASK,
	},
	[D2260_IRQ_ETICK] = {
		.reg_offset = 0,
		.mask = D2260_M_TICK_MASK,
	},
	[D2260_IRQ_ENONKEY_LO] = {
		.reg_offset = 1,
		.mask = D2260_M_NONKEY_LO_MASK,
	},
	[D2260_IRQ_ENONKEY_HI] = {
		.reg_offset = 1,
		.mask = D2260_M_NONKEY_HI_MASK,
	},
	[D2260_IRQ_ENONKEY_HOLDON] = {
		.reg_offset = 1,
		.mask = D2260_M_NONKEY_HOLD_ON_MASK,
	},
	[D2260_IRQ_ENONKEY_HOLDOFF] = {
		.reg_offset = 1,
		.mask = D2260_M_NONKEY_HOLD_OFF_MASK,
	},
	[D2260_IRQ_ETBAT1] = {
		.reg_offset = 1,
		.mask = D2260_M_TBAT1_MASK,
	},
	[D2260_IRQ_EADCEOM] = {
		.reg_offset = 1,
		.mask = D2260_M_ADC_EOM_MASK,
	},
 	[D2260_IRQ_ETA] = {
		.reg_offset = 2,
		.mask = D2260_M_TA_MASK,
	},
	[D2260_IRQ_ENJIGON] = {
		.reg_offset = 2,
		.mask = D2260_M_NON2_MASK,
	},
	[D2260_IRQ_GPI0] = {
		.reg_offset = 2,
		.mask = D2260_M_GPIO_0_MASK,
	},
	[D2260_IRQ_GPI1] = {
		.reg_offset = 2,
		.mask = D2260_M_GPIO_1_MASK,
	},
};
#define D2260_NUM_IRQ_REGS 3

static struct regmap_irq_chip d2260_regmap_irq_chip = {
	.name = "d2260_irq",
#ifdef D2260_USING_RANGED_REGMAP
	.status_base = D2260_EVENT_A_REG + D2260_MAPPING_BASE,
	.mask_base = D2260_IRQ_MASK_A_REG + D2260_MAPPING_BASE,
	.ack_base = D2260_EVENT_A_REG + D2260_MAPPING_BASE,
#else
	.status_base = D2260_EVENT_A_REG,
	.mask_base = D2260_IRQ_MASK_A_REG,
	.ack_base = D2260_EVENT_A_REG,
#endif
	.num_regs = D2260_NUM_IRQ_REGS,
	.irqs = d2260_irqs,
	.num_irqs = ARRAY_SIZE(d2260_irqs),
};

static int d2260_map_irq(struct d2260 *d2260, int irq)
{
	return regmap_irq_get_virq(d2260->irq_data, irq);
}

int d2260_enable_irq(struct d2260 *d2260, int irq)
{
	irq = d2260_map_irq(d2260, irq);
	if (irq < 0)
		return irq;

	enable_irq(irq);

	return 0;
}
EXPORT_SYMBOL_GPL(d2260_enable_irq);

int d2260_disable_irq(struct d2260 *d2260, int irq)
{
	irq = d2260_map_irq(d2260, irq);
	if (irq < 0)
		return irq;

	disable_irq(irq);

	return 0;
}
EXPORT_SYMBOL_GPL(d2260_disable_irq);

int d2260_disable_irq_nosync(struct d2260 *d2260, int irq)
{
	irq = d2260_map_irq(d2260, irq);
	if (irq < 0)
		return irq;

	disable_irq_nosync(irq);

	return 0;
}
EXPORT_SYMBOL_GPL(d2260_disable_irq_nosync);

int d2260_request_irq(struct d2260 *d2260, int irq, char *name,
			   irq_handler_t handler, void *data)
{
	irq = d2260_map_irq(d2260, irq);
	if (irq < 0)
		return irq;

	return request_threaded_irq(irq, NULL, handler,
				     D2260_IRQ_FLAGS,
				     name, data);
}
EXPORT_SYMBOL_GPL(d2260_request_irq);

void d2260_free_irq(struct d2260 *d2260, int irq, void *data)
{
	irq = d2260_map_irq(d2260, irq);
	if (irq < 0)
		return;

	free_irq(irq, data);
}
EXPORT_SYMBOL_GPL(d2260_free_irq);

int d2260_irq_init(struct d2260 *d2260)
{
	unsigned int reg_data;
	int ret = -EINVAL;
	int maskbit;
	int i;

	DIALOG_DEBUG(d2260->dev, "[%s] chip_irq[0x%x]\n", __func__, d2260->chip_irq);

	maskbit = 0xFFFFFF;
#ifdef D2260_USING_RANGED_REGMAP
	d2260_group_write(d2260, D2260_EVENT_A_REG,
			D2260_NUM_IRQ_MASK_REGS, &maskbit);
#else
	d2260_group_write(d2260, D2260_EVENT_A_REG,
			D2260_NUM_IRQ_MASK_REGS, (u8 *)&maskbit);
#endif

	reg_data = 0;

#ifdef D2260_USING_RANGED_REGMAP
	d2260_group_write(d2260, D2260_EVENT_A_REG,
			D2260_NUM_IRQ_MASK_REGS, &reg_data);
#else
	d2260_group_write(d2260, D2260_EVENT_A_REG,
			D2260_NUM_IRQ_MASK_REGS, (u8 *)&reg_data);
#endif

	/* Clear Mask register starting with Mask A*/
	maskbit = 0xFFFFFF;

#ifdef D2260_USING_RANGED_REGMAP
	d2260_group_write(d2260, D2260_IRQ_MASK_A_REG,
			D2260_NUM_IRQ_MASK_REGS, &maskbit);
#else
	d2260_group_write(d2260, D2260_IRQ_MASK_A_REG,
			D2260_NUM_IRQ_MASK_REGS, (u8 *)&maskbit);
#endif

	ret = regmap_add_irq_chip(d2260->regmap, d2260->chip_irq,
				D2260_IRQ_FLAGS,
				-1, &d2260_regmap_irq_chip,
				&d2260->irq_data);
	enable_irq_wake(d2260->chip_irq);

	if (ret < 0) {
		dev_err(d2260->dev, "regmap_add_irq_chip failed: %d\n", ret);
		goto regmap_err;
	}

	return 0;

regmap_err:
	return ret;

}

int d2260_irq_exit(struct d2260 *d2260)
{
	regmap_del_irq_chip(d2260->chip_irq, d2260->irq_data);

	return 0;
}
