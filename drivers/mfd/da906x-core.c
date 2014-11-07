/*
 * da906x-core.c: Device access for Dialog DA906x modules
 *
 * Copyright 2012 Dialog Semiconductors Ltd.
 *
 * Author: Krystian Garbaciak <krystian.garbaciak@diasemi.com>,
 *         Michal Hajduk <michal.hajduk@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/regmap.h>

#include <linux/mfd/da906x/core.h>
#include <linux/mfd/da906x/pdata.h>
#include <linux/mfd/da906x/registers.h>

#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>


static struct resource da906x_regulators_resources[] = {
	{
		.name	= "LDO_LIM",
		.start	= DA906X_IRQ_LDO_LIM,
		.end	= DA906X_IRQ_LDO_LIM,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "DA9210_OC",
		.start	= DA9210_IRQ_OVCURR,
		.end	= DA9210_IRQ_OVCURR,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "DA9210_TEMP",
		.start	= DA9210_IRQ_TEMP_WARN,
		.end	= DA9210_IRQ_TEMP_WARN,
		.flags	= IORESOURCE_IRQ,
	}
};

static struct resource da906x_rtc_resources[] = {
	{
		.name	= "ALARM",
		.start	= DA906X_IRQ_ALARM,
		.end	= DA906X_IRQ_ALARM,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "TICK",
		.start	= DA906X_IRQ_TICK,
		.end	= DA906X_IRQ_TICK,
		.flags	= IORESOURCE_IRQ,
	}
};

static struct resource da906x_onkey_resources[] = {
	{
		.start	= DA906X_IRQ_ONKEY,
		.end	= DA906X_IRQ_ONKEY,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource da906x_hwmon_resources[] = {
	{
		.start	= DA906X_IRQ_ADC_RDY,
		.end	= DA906X_IRQ_ADC_RDY,
		.flags	= IORESOURCE_IRQ,
	},
};


static struct mfd_cell da906x_devs[] = {
	{
		.name		= DA906X_DRVNAME_REGULATORS,
		.num_resources	= ARRAY_SIZE(da906x_regulators_resources),
		.resources	= da906x_regulators_resources,
	},
	{
		.name		= DA906X_DRVNAME_LEDS,
	},
	{
		.name		= DA906X_DRVNAME_WATCHDOG,
	},
	{
		.name		= DA906X_DRVNAME_HWMON,
		.num_resources	= ARRAY_SIZE(da906x_hwmon_resources),
		.resources	= da906x_hwmon_resources,
	},
	{
		.name		= DA906X_DRVNAME_ONKEY,
		.num_resources	= ARRAY_SIZE(da906x_onkey_resources),
		.resources	= da906x_onkey_resources,
	},
	{
		.name		= DA906X_DRVNAME_RTC,
		.num_resources	= ARRAY_SIZE(da906x_rtc_resources),
		.resources	= da906x_rtc_resources,
	},
	{
		.name		= DA906X_DRVNAME_VIBRATION,
	},
};

inline unsigned int da906x_to_range_reg(u16 reg)
{
	return reg + DA906X_MAPPING_BASE;
}

int da906x_reg_read(struct da906x *da906x, u16 reg)
{
	unsigned int val;
	int ret;

	ret = regmap_read(da906x->regmap, da906x_to_range_reg(reg), &val);
	if (ret < 0)
		return ret;

	return val;
}

int da906x_reg_write(struct da906x *da906x, u16 reg, u8 val)
{
	return regmap_write(da906x->regmap, da906x_to_range_reg(reg), val);
}

int da906x_block_read(struct da906x *da906x, u16 reg, int bytes, u8 *dst)
{
	return regmap_bulk_read(da906x->regmap, da906x_to_range_reg(reg),
				dst, bytes);
}

int da906x_block_write(struct da906x *da906x, u16 reg, int bytes, const u8 *src)
{
	return regmap_bulk_write(da906x->regmap, da906x_to_range_reg(reg),
				 src, bytes);
}

int da906x_reg_update(struct da906x *da906x, u16 reg, u8 mask, u8 val)
{
	return regmap_update_bits(da906x->regmap, da906x_to_range_reg(reg),
				  mask, val);
}

int da906x_reg_set_bits(struct da906x *da906x, u16 reg, u8 mask)
{
	return da906x_reg_update(da906x, reg, mask, mask);
}

int da906x_reg_clear_bits(struct da906x *da906x, u16 reg, u8 mask)
{
	return da906x_reg_update(da906x, reg, mask, 0);
}

int da906x_page_sel(struct da906x *da906x, u8 val)
{
	return regmap_write(da906x->regmap, DA906X_REG_PAGE_CON, val);
}

int da906x_device_init(struct da906x *da906x, unsigned int irq)
{
	int ret = 0;
	int model;
	int page_sel;
	unsigned short revision;

	mutex_init(&da906x->io_mutex);

	da906x->irq_base = -1;
	da906x->chip_irq = irq;

	da906x_page_sel(da906x, DA906X_REG_PAGE2);
	model = da906x_reg_read(da906x, DA906X_REG_CHIP_ID);
	if (model < 0) {
		dev_err(da906x->dev, "Cannot read chip model id.\n");
		return -EIO;
	}

	ret = da906x_reg_read(da906x, DA906X_REG_CHIP_VARIANT);
	if (ret < 0) {
		dev_err(da906x->dev, "Cannot read chip revision id.\n");
		return -EIO;
	}
	da906x_page_sel(da906x, DA906X_REG_PAGE0);

	revision = ret >> DA906X_CHIP_VARIANT_SHIFT;

	da906x_set_model_rev(da906x, model, revision);

	dev_info(da906x->dev,
		 "Device detected (model-ID: 0x%02X  rev-ID: 0x%02X)\n",
		 model, revision);

#ifndef CONFIG_MFD_DA906X_IRQ_DISABLE
	ret = da906x_irq_init(da906x);
	if (ret) {
		dev_err(da906x->dev, "Cannot initialize interrupts.\n");
		return ret;
	}
#endif

	ret = mfd_add_devices(da906x->dev, -1, da906x_devs,
			      ARRAY_SIZE(da906x_devs), NULL, da906x->irq_base, NULL);
	if (ret)
		dev_err(da906x->dev, "Cannot add MFD cells\n");

	return ret;
}

void da906x_device_exit(struct da906x *da906x)
{
	mfd_remove_devices(da906x->dev);
	da906x_irq_exit(da906x);
}

MODULE_DESCRIPTION("PMIC driver for Dialog DA906X");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DA906X_DRVNAME_CORE);
