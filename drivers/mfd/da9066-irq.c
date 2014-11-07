/*
 * da9066-irq.c: IRQ support for Dialog DA9066
 *
 * Copyright(c) 2013 Dialog Semiconductor Ltd.
 *
 * Author: Dialog Semiconductor Ltd. D. Chen, D. Patel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <asm/gpio.h>

#include <linux/mfd/da9066/pmic.h>
#include <linux/mfd/da9066/da9066_reg.h>
#include <linux/mfd/da9066/rtc.h>
#include <linux/mfd/da9066/core.h>

#define DA9066_NUM_IRQ_MASK_REGS			3

/* Number of IRQ and IRQ offset */
enum {
	DA9066_INT_OFFSET_1 = 0,
	DA9066_INT_OFFSET_2,
	DA9066_INT_OFFSET_3,
	DA9066_NUM_IRQ_EVT_REGS
};

/* struct of IRQ's register and mask */
struct da9066_irq_data {
	int reg;
	int mask;
};

static struct da9066_irq_data da9066_irqs[] = {
	/* EVENT Register A start */
	[DA9066_IRQ_EVF] = {
		.reg = DA9066_INT_OFFSET_1,
		.mask = DA9066_M_VF_MASK,
	},
	[DA9066_IRQ_EADCIN1] = {
		.reg = DA9066_INT_OFFSET_1,
		.mask = DA9066_M_ADCIN1_MASK,
	},
	[DA9066_IRQ_ETBAT2] = {
		.reg = DA9066_INT_OFFSET_1,
		.mask = DA9066_M_TBAT2_MASK,
	},
	[DA9066_IRQ_EVDD_LOW] = {
		.reg = DA9066_INT_OFFSET_1,
		.mask = DA9066_M_VDD_LOW_MASK,
	},
	[DA9066_IRQ_EVDD_MON] = {
		.reg = DA9066_INT_OFFSET_1,
		.mask = DA9066_M_VDD_MON_MASK,
	},
	[DA9066_IRQ_EALARM] = {
		.reg = DA9066_INT_OFFSET_1,
		.mask = DA9066_M_ALARM_MASK,
	},
	[DA9066_IRQ_ESEQRDY] = {
		.reg = DA9066_INT_OFFSET_1,
		.mask = DA9066_M_SEQ_RDY_MASK,
	},
	[DA9066_IRQ_ETICK] = {
		.reg = DA9066_INT_OFFSET_1,
		.mask = DA9066_M_TICK_MASK,
	},
	/* EVENT Register B start */
	[DA9066_IRQ_ENONKEY_LO] = {
		.reg = DA9066_INT_OFFSET_2,
		.mask = DA9066_M_NONKEY_LO_MASK,
	},
	[DA9066_IRQ_ENONKEY_HI] = {
		.reg = DA9066_INT_OFFSET_2,
		.mask = DA9066_M_NONKEY_HI_MASK,
	},
	[DA9066_IRQ_ENONKEY_HOLDON] = {
		.reg = DA9066_INT_OFFSET_2,
		.mask = DA9066_M_NONKEY_HOLD_ON_MASK,
	},
	[DA9066_IRQ_ENONKEY_HOLDOFF] = {
		.reg = DA9066_INT_OFFSET_2,
		.mask = DA9066_M_NONKEY_HOLD_OFF_MASK,
	},
	[DA9066_IRQ_ETBAT1] = {
		.reg = DA9066_INT_OFFSET_2,
		.mask = DA9066_M_TBAT1_MASK,
	},
	[DA9066_IRQ_EADCEOM] = {
		.reg = DA9066_INT_OFFSET_2,
		.mask = DA9066_M_ADC_EOM_MASK,
	},
	/* EVENT Register C start */
	[DA9066_IRQ_ETA] = {
		.reg = DA9066_INT_OFFSET_3,
		.mask = DA9066_M_GPI_3_M_TA_MASK,
	},
	[DA9066_IRQ_ENJIGON] = {
		.reg = DA9066_INT_OFFSET_3,
		.mask = DA9066_M_GPI_4_M_NJIG_ON_MASK,
	},
	[DA9066_IRQ_EACCDET] = {
		.reg = DA9066_INT_OFFSET_3,
		.mask = DA9066_M_ACC_DET_MASK,
	},
	[DA9066_IRQ_EJACKDET] = {
		.reg = DA9066_INT_OFFSET_3,
		.mask = DA9066_M_JACK_DET_MASK,
	},
};


static void da9066_irq_call_handler(struct da9066 *da9066, int irq)
{
	mutex_lock(&da9066->irq_mutex);

	if (da9066->irq[irq].handler) {
		da9066->irq[irq].handler(irq, da9066->irq[irq].data);

	} else {
		da9066_mask_irq(da9066, irq);
	}
	mutex_unlock(&da9066->irq_mutex);
}

/*
 * This is a threaded IRQ handler so can access I2C.  Since all
 * interrupts are clear on read the IRQ line will be reasserted and
 * the physical IRQ will be handled again if another interrupt is
 * asserted while we run - in the normal course of events this is a
 * rare occurrence so we save I2C/SPI reads.
 */
void da9066_irq_worker(struct work_struct *work)
{
	struct da9066 *da9066 = container_of(work, struct da9066, irq_work);
	u8 reg_val;
	u8 sub_reg[DA9066_NUM_IRQ_EVT_REGS] = {0,};
	int read_done[DA9066_NUM_IRQ_EVT_REGS];
	struct da9066_irq_data *data;
	int i;

	memset(&read_done, 0, sizeof(read_done));

	for (i = 0; i < ARRAY_SIZE(da9066_irqs); i++) {
		data = &da9066_irqs[i];

		if (!read_done[data->reg]) {
			da9066_reg_read(da9066, DA9066_EVENT_A_REG + data->reg, &reg_val);
			sub_reg[data->reg] = reg_val;
			da9066_reg_read(da9066, DA9066_IRQ_MASK_A_REG + data->reg, &reg_val);
			sub_reg[data->reg] &= ~reg_val;
			read_done[data->reg] = 1;
		}

		if (sub_reg[data->reg] & data->mask) {
			da9066_irq_call_handler(da9066, i);
			/* Now clear EVENT registers */
			da9066_set_bits(da9066, DA9066_EVENT_A_REG + data->reg, da9066_irqs[i].mask);
		}
	}
	enable_irq(da9066->chip_irq);
	dev_info(da9066->dev, "IRQ Generated [da9066_irq_worker EXIT]\n");
}



static irqreturn_t da9066_irq(int irq, void *data)
{
	struct da9066 *da9066 = data;
	u8 reg_val;
	u8 sub_reg[DA9066_NUM_IRQ_EVT_REGS] = {0,};
	int read_done[DA9066_NUM_IRQ_EVT_REGS];
	struct da9066_irq_data *pirq;
	int i;

	memset(&read_done, 0, sizeof(read_done));

	for (i = 0; i < ARRAY_SIZE(da9066_irqs); i++) {
		pirq = &da9066_irqs[i];

		if (!read_done[pirq->reg]) {
			da9066_reg_read(da9066, DA9066_EVENT_A_REG + pirq->reg, &reg_val);
			sub_reg[pirq->reg] = reg_val;
			da9066_reg_read(da9066, DA9066_IRQ_MASK_A_REG + pirq->reg, &reg_val);
			sub_reg[pirq->reg] &= ~reg_val;
			read_done[pirq->reg] = 1;
		}

		if (sub_reg[pirq->reg] & pirq->mask) {
			da9066_irq_call_handler(da9066, i);
			/* Now clear EVENT registers */
			da9066_set_bits(da9066, DA9066_EVENT_A_REG + pirq->reg, da9066_irqs[i].mask);
		}
	}
	return IRQ_HANDLED;
}


int da9066_register_irq(struct da9066 * const da9066, int const irq,
			irq_handler_t handler, unsigned long flags,
			const char * const name, void * const data)
{
	if (irq < 0 || irq >= DA9066_NUM_IRQ || !handler)
		return -EINVAL;

	if (da9066->irq[irq].handler)
		return -EBUSY;
	mutex_lock(&da9066->irq_mutex);
	da9066->irq[irq].handler = handler;
	da9066->irq[irq].data = data;
	mutex_unlock(&da9066->irq_mutex);
	/* DLG Test Print */
    dev_info(da9066->dev, "\nIRQ After MUTEX UNLOCK [%s]\n", __func__);

	da9066_unmask_irq(da9066, irq);
	return 0;
}
EXPORT_SYMBOL_GPL(da9066_register_irq);

int da9066_free_irq(struct da9066 *da9066, int irq)
{
	if (irq < 0 || irq >= DA9066_NUM_IRQ)
		return -EINVAL;

	da9066_mask_irq(da9066, irq);

	mutex_lock(&da9066->irq_mutex);
	da9066->irq[irq].handler = NULL;
	mutex_unlock(&da9066->irq_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(da9066_free_irq);

int da9066_mask_irq(struct da9066 *da9066, int irq)
{
	return da9066_set_bits(da9066, DA9066_IRQ_MASK_A_REG + da9066_irqs[irq].reg,
			       da9066_irqs[irq].mask);
}
EXPORT_SYMBOL_GPL(da9066_mask_irq);

int da9066_unmask_irq(struct da9066 *da9066, int irq)
{
	dev_info(da9066->dev, "\nIRQ[%d] Register [%d] MASK [%d]\n",irq,
		DA9066_IRQ_MASK_A_REG + da9066_irqs[irq].reg, da9066_irqs[irq].mask);
	return da9066_clear_bits(da9066, DA9066_IRQ_MASK_A_REG + da9066_irqs[irq].reg,
				 da9066_irqs[irq].mask);
}
EXPORT_SYMBOL_GPL(da9066_unmask_irq);

int da9066_irq_init(struct da9066 *da9066, int irq,
		    struct da9066_platform_data *pdata)
{
	int ret = -EINVAL;
	int reg_data, maskbit;

	if (!irq) {
		dev_err(da9066->dev, "No IRQ configured\n");
	    return -EINVAL;
	}
	reg_data = 0xFFFFFF;
	da9066_block_write(da9066, DA9066_EVENT_A_REG,
			DA9066_NUM_IRQ_EVT_REGS, (u8 *)&reg_data);
	reg_data = 0;
	da9066_block_write(da9066, DA9066_EVENT_A_REG,
			DA9066_NUM_IRQ_EVT_REGS, (u8 *)&reg_data);

	/* Clear Mask register starting with Mask A*/
	maskbit = 0xFFFFFF;
	da9066_block_write(da9066, DA9066_IRQ_MASK_A_REG,
			DA9066_NUM_IRQ_MASK_REGS, (u8 *)&maskbit);

	mutex_init(&da9066->irq_mutex);

	if (irq) {
		ret = request_threaded_irq(irq, NULL, da9066_irq,
									IRQF_TRIGGER_LOW|IRQF_ONESHOT,
				  					"da9066", da9066);
		if (ret != 0) {
			dev_err(da9066->dev, "Failed to request IRQ: %d\n", irq);
			return ret;
		}
		dev_info(da9066->dev, "# IRQ configured [%d] \n", irq);
	}

	enable_irq_wake(irq);

	da9066->chip_irq = irq;

	return ret;
}

int da9066_irq_exit(struct da9066 *da9066)
{
	free_irq(da9066->chip_irq, da9066);
	return 0;
}
