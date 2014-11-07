/*
 * Definitions for DA906X MFD driver
 *
 * Copyright 2012 Dialog Semiconductor Ltd.
 *
 * Author: Michal Hajduk <michal.hajduk@diasemi.com>
 *	   Krystian Garbaciak <krystian.garbaciak@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __MFD_DA906X_CORE_H__
#define __MFD_DA906X_CORE_H__

#include <linux/interrupt.h>
#include <linux/mfd/da906x/registers.h>

/* DA906x modules */
#define DA906X_DRVNAME_CORE		"da906x-core"
#define DA906X_DRVNAME_REGULATORS	"da906x-regulators"
#define DA906X_DRVNAME_LEDS		"da906x-leds"
#define DA906X_DRVNAME_WATCHDOG		"da906x-watchdog"
#define DA906X_DRVNAME_HWMON		"da906x-hwmon"
#define DA906X_DRVNAME_ONKEY		"da906x-onkey"
#define DA906X_DRVNAME_RTC		"da906x-rtc"
#define DA906X_DRVNAME_VIBRATION	"da906x-vibration"

enum da906x_models {
	PMIC_DA9063 = 0x61,
};

/* Interrupts */
enum da906x_irqs {
	DA906X_IRQ_ONKEY = 0,
	DA906X_IRQ_ALARM,
	DA906X_IRQ_TICK,
	DA906X_IRQ_ADC_RDY,
	DA906X_IRQ_SEQ_RDY,
	DA906X_IRQ_WAKE,
	DA906X_IRQ_TEMP,
	DA906X_IRQ_COMP_1V2,
	DA906X_IRQ_LDO_LIM,
	DA906X_IRQ_REG_UVOV,
	DA906X_IRQ_VDD_MON,
	DA906X_IRQ_WARN,
	DA906X_IRQ_GPI0,
	DA906X_IRQ_GPI1,
	DA906X_IRQ_GPI2,
	DA906X_IRQ_GPI3,
	DA906X_IRQ_GPI4,
	DA906X_IRQ_GPI5,
	DA906X_IRQ_GPI6,
	DA906X_IRQ_GPI7,
	DA906X_IRQ_GPI8,
	DA906X_IRQ_GPI9,
	DA906X_IRQ_GPI10,
	DA906X_IRQ_GPI11,
	DA906X_IRQ_GPI12,
	DA906X_IRQ_GPI13,
	DA906X_IRQ_GPI14,
	DA906X_IRQ_GPI15,

	/* CoPMIC IRQs */
	DA9210_IRQ_GPI0,
	DA9210_IRQ_GPI1,
	DA9210_IRQ_GPI2,
	DA9210_IRQ_GPI3,
	DA9210_IRQ_GPI4,
	DA9210_IRQ_GPI5,
	DA9210_IRQ_GPI6,
	DA9210_IRQ_OVCURR,
	DA9210_IRQ_NPWRGOOD,
	DA9210_IRQ_TEMP_WARN,
	DA9210_IRQ_TEMP_CRIT,
	DA9210_IRQ_VMAX,
};

#define DA906X_IRQ_BASE_OFFSET	0
#define DA906X_NUM_IRQ		(DA906X_IRQ_GPI15 + 1)

#define DA9210_IRQ_BASE_OFFSET	DA9210_IRQ_GPI0
#define DA9210_NUM_IRQ		(DA9210_IRQ_VMAX + 1 - DA9210_IRQ_BASE_OFFSET)

struct da906x {
	/* Device */
	struct device	*dev;
	unsigned short	model;
	unsigned short	revision;
	unsigned short	addr;
	int		da9210;

	/* Control interface */
	struct mutex	io_mutex;
	struct regmap	*regmap;

	/* Interrupts */
	int		chip_irq;
	unsigned int	irq_base;
	struct regmap_irq_chip_data *regmap_irq;
	struct regmap_irq_chip_data *da9210_regmap_irq;
};

static inline unsigned da906x_model(struct da906x *da906x)
{
	return da906x->model;
}

static inline unsigned da906x_revision(struct da906x *da906x)
{
	return da906x->revision;
}

static inline void da906x_set_model_rev(struct da906x *da906x,
			enum da906x_models model, unsigned short revision)
{
	da906x->model = model;
	da906x->revision = revision;
}

int da906x_reg_read(struct da906x *da906x, u16 reg);
int da906x_reg_write(struct da906x *da906x, u16 reg, u8 val);

int da906x_block_write(struct da906x *da906x, u16 reg,
		int bytes, const u8 *buf);
int da906x_block_read(struct da906x *da906x, u16 reg, int bytes, u8 *buf);

int da906x_reg_set_bits(struct da906x *da906x, u16 reg, u8 mask);
int da906x_reg_clear_bits(struct da906x *da906x, u16 reg, u8 mask);
int da906x_reg_update(struct da906x *da906x, u16 reg, u8 mask, u8 val);

int da906x_device_init(struct da906x *da906x, unsigned int irq);
int da906x_irq_init(struct da906x *da906x);

void da906x_device_exit(struct da906x *da906x);
void da906x_irq_exit(struct da906x *da906x);

#endif /* __MFD_DA906X_CORE_H__ */
