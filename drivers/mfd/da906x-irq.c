/* da906x-irq.c: Interrupts support for Dialog DA906X
 *
 * Copyright 2012 Dialog Semiconductor Ltd.
 *
 * Author: Michal Hajduk <michal.hajduk@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/mfd/da906x/core.h>
#include <linux/mfd/da906x/pdata.h>

#define	DA906X_REG_EVENT_A_OFFSET	0
#define	DA906X_REG_EVENT_B_OFFSET	1
#define	DA906X_REG_EVENT_C_OFFSET	2
#define	DA906X_REG_EVENT_D_OFFSET	3

#define	DA9210_REG_EVENT_A_OFFSET	0
#define	DA9210_REG_EVENT_B_OFFSET	1

#define EVENTS_BUF_LEN	(DA906X_EVENT_REG_NUM > DA9210_EVENT_REG_NUM ? \
			 DA906X_EVENT_REG_NUM : DA9210_EVENT_REG_NUM)
static const u8 mask_events_buf[] = { [0 ... (EVENTS_BUF_LEN - 1)] = ~0 };

struct da906x_irq_data {
	u16 reg;
	u8 mask;
};

static struct regmap_irq da906x_irqs[] = {
	/* DA906x event A register */
	[DA906X_IRQ_ONKEY] = {
		.reg_offset = DA906X_REG_EVENT_A_OFFSET,
		.mask = DA906X_M_ONKEY,
	},
	[DA906X_IRQ_ALARM] = {
		.reg_offset = DA906X_REG_EVENT_A_OFFSET,
		.mask = DA906X_M_ALARM,
	},
	[DA906X_IRQ_TICK] = {
		.reg_offset = DA906X_REG_EVENT_A_OFFSET,
		.mask = DA906X_M_TICK,
	},
	[DA906X_IRQ_ADC_RDY] = {
		.reg_offset = DA906X_REG_EVENT_A_OFFSET,
		.mask = DA906X_M_ADC_RDY,
	},
	[DA906X_IRQ_SEQ_RDY] = {
		.reg_offset = DA906X_REG_EVENT_A_OFFSET,
		.mask = DA906X_M_SEQ_RDY,
	},
	/* DA906x event B register */
	[DA906X_IRQ_WAKE] = {
		.reg_offset = DA906X_REG_EVENT_B_OFFSET,
		.mask = DA906X_M_WAKE,
	},
	[DA906X_IRQ_TEMP] = {
		.reg_offset = DA906X_REG_EVENT_B_OFFSET,
		.mask = DA906X_M_TEMP,
	},
	[DA906X_IRQ_COMP_1V2] = {
		.reg_offset = DA906X_REG_EVENT_B_OFFSET,
		.mask = DA906X_M_COMP_1V2,
	},
	[DA906X_IRQ_LDO_LIM] = {
		.reg_offset = DA906X_REG_EVENT_B_OFFSET,
		.mask = DA906X_M_LDO_LIM,
	},
	[DA906X_IRQ_REG_UVOV] = {
		.reg_offset = DA906X_REG_EVENT_B_OFFSET,
		.mask = DA906X_M_UVOV,
	},
	[DA906X_IRQ_VDD_MON] = {
		.reg_offset = DA906X_REG_EVENT_B_OFFSET,
		.mask = DA906X_M_VDD_MON,
	},
	[DA906X_IRQ_WARN] = {
		.reg_offset = DA906X_REG_EVENT_B_OFFSET,
		.mask = DA906X_M_VDD_WARN,
	},
	/* DA906x event C register */
	[DA906X_IRQ_GPI0] = {
		.reg_offset = DA906X_REG_EVENT_C_OFFSET,
		.mask = DA906X_M_GPI0,
	},
	[DA906X_IRQ_GPI1] = {
		.reg_offset = DA906X_REG_EVENT_C_OFFSET,
		.mask = DA906X_M_GPI1,
	},
	[DA906X_IRQ_GPI2] = {
		.reg_offset = DA906X_REG_EVENT_C_OFFSET,
		.mask = DA906X_M_GPI2,
	},
	[DA906X_IRQ_GPI3] = {
		.reg_offset = DA906X_REG_EVENT_C_OFFSET,
		.mask = DA906X_M_GPI3,
	},
	[DA906X_IRQ_GPI4] = {
		.reg_offset = DA906X_REG_EVENT_C_OFFSET,
		.mask = DA906X_M_GPI4,
	},
	[DA906X_IRQ_GPI5] = {
		.reg_offset = DA906X_REG_EVENT_C_OFFSET,
		.mask = DA906X_M_GPI5,
	},
	[DA906X_IRQ_GPI6] = {
		.reg_offset = DA906X_REG_EVENT_C_OFFSET,
		.mask = DA906X_M_GPI6,
	},
	[DA906X_IRQ_GPI7] = {
		.reg_offset = DA906X_REG_EVENT_C_OFFSET,
		.mask = DA906X_M_GPI7,
	},
	/* DA906x event D register */
	[DA906X_IRQ_GPI8] = {
		.reg_offset = DA906X_REG_EVENT_D_OFFSET,
		.mask = DA906X_M_GPI8,
	},
	[DA906X_IRQ_GPI9] = {
		.reg_offset = DA906X_REG_EVENT_D_OFFSET,
		.mask = DA906X_M_GPI9,
	},
	[DA906X_IRQ_GPI10] = {
		.reg_offset = DA906X_REG_EVENT_D_OFFSET,
		.mask = DA906X_M_GPI10,
	},
	[DA906X_IRQ_GPI11] = {
		.reg_offset = DA906X_REG_EVENT_D_OFFSET,
		.mask = DA906X_M_GPI11,
	},
	[DA906X_IRQ_GPI12] = {
		.reg_offset = DA906X_REG_EVENT_D_OFFSET,
		.mask = DA906X_M_GPI12,
	},
	[DA906X_IRQ_GPI13] = {
		.reg_offset = DA906X_REG_EVENT_D_OFFSET,
		.mask = DA906X_M_GPI13,
	},
	[DA906X_IRQ_GPI14] = {
		.reg_offset = DA906X_REG_EVENT_D_OFFSET,
		.mask = DA906X_M_GPI14,
	},
	[DA906X_IRQ_GPI15] = {
		.reg_offset = DA906X_REG_EVENT_D_OFFSET,
		.mask = DA906X_M_GPI15,
	},

	/* CoPMIC IRQs */
	/* DA9210 event A register */
	[DA9210_IRQ_GPI0] = {
		.reg_offset = DA9210_REG_EVENT_A_OFFSET,
		.mask = DA9210_M_GPI0,
	},
	[DA9210_IRQ_GPI1] = {
		.reg_offset = DA9210_REG_EVENT_A_OFFSET,
		.mask = DA9210_M_GPI1,
	},
	[DA9210_IRQ_GPI2] = {
		.reg_offset = DA9210_REG_EVENT_A_OFFSET,
		.mask = DA9210_M_GPI2,
	},
	[DA9210_IRQ_GPI3] = {
		.reg_offset = DA9210_REG_EVENT_A_OFFSET,
		.mask = DA9210_M_GPI3,
	},
	[DA9210_IRQ_GPI4] = {
		.reg_offset = DA9210_REG_EVENT_A_OFFSET,
		.mask = DA9210_M_GPI4,
	},
	[DA9210_IRQ_GPI5] = {
		.reg_offset = DA9210_REG_EVENT_A_OFFSET,
		.mask = DA9210_M_GPI5,
	},
	[DA9210_IRQ_GPI6] = {
		.reg_offset = DA9210_REG_EVENT_A_OFFSET,
		.mask = DA9210_M_GPI6,
	},
	/* DA9210 event B register */
	[DA9210_IRQ_OVCURR] = {
		.reg_offset = DA9210_REG_EVENT_B_OFFSET,
		.mask = DA9210_M_OVCURR,
	},
	[DA9210_IRQ_NPWRGOOD] = {
		.reg_offset = DA9210_REG_EVENT_B_OFFSET,
		.mask = DA9210_M_NPWRGOOD,
	},
	[DA9210_IRQ_TEMP_WARN] = {
		.reg_offset = DA9210_REG_EVENT_B_OFFSET,
		.mask = DA9210_M_TEMP_WARN,
	},
	[DA9210_IRQ_TEMP_CRIT] = {
		.reg_offset = DA9210_REG_EVENT_B_OFFSET,
		.mask = DA9210_M_TEMP_CRIT,
	},
	[DA9210_IRQ_VMAX] = {
		.reg_offset = DA9210_REG_EVENT_B_OFFSET,
		.mask = DA9210_M_VMAX,
	},
};

static struct regmap_irq_chip da906x_irq_chip = {
	.name = "da906x-irq",
	.irqs = da906x_irqs,
	.num_irqs = DA906X_NUM_IRQ,

	.num_regs = 4,
	.status_base = DA906X_REG_EVENT_A + DA906X_MAPPING_BASE,
	.mask_base = DA906X_REG_IRQ_MASK_A + DA906X_MAPPING_BASE,
	.ack_base = DA906X_REG_EVENT_A + DA906X_MAPPING_BASE,
};

static struct regmap_irq_chip da9210_irq_chip = {
	.name = "da9210-irq",
	.irqs = &da906x_irqs[DA9210_IRQ_BASE_OFFSET],
	.num_irqs = DA9210_NUM_IRQ,

	.num_regs = 2,
	.status_base = DA9210_REG_EVENT_A + DA906X_MAPPING_BASE,
	.mask_base = DA9210_REG_MASK_A + DA906X_MAPPING_BASE,
	.ack_base = DA9210_REG_EVENT_A + DA906X_MAPPING_BASE,
};

int da906x_irq_init(struct da906x *da906x)
{
	int ret;

	if (!da906x->chip_irq) {
		dev_err(da906x->dev, "No IRQ configured\n");
		return -EINVAL;
	}

	ret = regmap_add_irq_chip(da906x->regmap, da906x->chip_irq,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_SHARED,
			da906x->irq_base, &da906x_irq_chip,
			&da906x->regmap_irq);
	if (ret) {
		dev_err(da906x->dev, "Failed to reguest IRQ %d: %d\n",
				da906x->chip_irq, ret);
		return ret;
	}

	da906x->irq_base =
		(unsigned int)(regmap_irq_chip_get_base(da906x->regmap_irq));

	if (da906x->da9210) {
		int da9210_irq_base = da906x->irq_base + DA9210_IRQ_BASE_OFFSET;

		ret = regmap_add_irq_chip(da906x->regmap, da906x->chip_irq,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_SHARED,
				da9210_irq_base, &da9210_irq_chip,
				&da906x->da9210_regmap_irq);
		if (ret) {
			dev_err(da906x->dev, "Failed to reguest IRQ %d: %d\n",
					da906x->chip_irq, ret);
			regmap_del_irq_chip(da906x->chip_irq,
					    da906x->regmap_irq);
			return ret;
		}
	}

	return 0;
}

void da906x_irq_exit(struct da906x *da906x)
{
	regmap_del_irq_chip(da906x->chip_irq, da906x->regmap_irq);
	if (da906x->da9210)
		regmap_del_irq_chip(da906x->chip_irq,
				    da906x->da9210_regmap_irq);
}

