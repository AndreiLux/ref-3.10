/* da906x-ipc.c: Interrupt support for Dialog DA906x
 *
 * Copyright 2012 Dialog Semiconductor Ltd.
 *
 * Author: Krystian Garbaciak <krystian.garbaciak@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/regmap-ipc.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/of.h>

#include <linux/mfd/core.h>
#include <linux/mfd/da906x/core.h>
#include <linux/mfd/da906x/registers.h>
#include <linux/mfd/da906x/pdata.h>

#define COPMIC	0x01	/* DA9210 register */
#define RD		0x02	/* Readable register */
#define WR		0x04	/* Writable register */
#define VOL		0x08	/* Volatile register */

#define DA906X_I2C_SLOT	 	0

#define DA906X_SLAVE_ADDRESS 0xB0

/* Flags for virtual registers (mapped starting from DA906X_MAPPING_BASE) */
const unsigned char da906x_reg_flg[] = {
	[DA906X_REG_STATUS_A] =		RD | VOL,
	[DA906X_REG_STATUS_B] =		RD | VOL,
	[DA906X_REG_STATUS_C] =		RD | VOL,
	[DA906X_REG_STATUS_D] =		RD | VOL,
	[DA906X_REG_FAULT_LOG] =	RD | WR | VOL,
	[DA906X_REG_EVENT_A] =		RD | WR | VOL,
	[DA906X_REG_EVENT_B] =		RD | WR | VOL,
	[DA906X_REG_EVENT_C] =		RD | WR | VOL,
	[DA906X_REG_EVENT_D] =		RD | WR | VOL,
	[DA906X_REG_IRQ_MASK_A] =	RD | WR,
	[DA906X_REG_IRQ_MASK_B] =	RD | WR,
	[DA906X_REG_IRQ_MASK_C] =	RD | WR,
	[DA906X_REG_IRQ_MASK_D] =	RD | WR,
	[DA906X_REG_CONTROL_A] =	RD | WR,
	[DA906X_REG_CONTROL_B] =	RD | WR,
	[DA906X_REG_CONTROL_C] =	RD | WR,
	[DA906X_REG_CONTROL_D] =	RD | WR,
	[DA906X_REG_CONTROL_E] =	RD | WR,
	[DA906X_REG_CONTROL_F] =	RD | WR | VOL,
	[DA906X_REG_PD_DIS] =		RD | WR,

	[DA906X_REG_GPIO_0_1] =		RD | WR,
	[DA906X_REG_GPIO_2_3] =		RD | WR,
	[DA906X_REG_GPIO_4_5] =		RD | WR,
	[DA906X_REG_GPIO_6_7] =		RD | WR,
	[DA906X_REG_GPIO_8_9] =		RD | WR,
	[DA906X_REG_GPIO_10_11] =	RD | WR,
	[DA906X_REG_GPIO_12_13] =	RD | WR,
	[DA906X_REG_GPIO_14_15] =	RD | WR,
	[DA906X_REG_GPIO_MODE_0_7] =	RD | WR,
	[DA906X_REG_GPIO_MODE_8_15] =	RD | WR,
	[DA906X_REG_GPIO_SWITCH_CONT] =	RD | WR,

	[DA906X_REG_BCORE2_CONT] =	RD | WR,
	[DA906X_REG_BCORE1_CONT] =	RD | WR,
	[DA906X_REG_BPRO_CONT] =	RD | WR,
	[DA906X_REG_BMEM_CONT] =	RD | WR,
	[DA906X_REG_BIO_CONT] =		RD | WR,
	[DA906X_REG_BPERI_CONT] =	RD | WR,
	[DA906X_REG_LDO1_CONT] =	RD | WR,
	[DA906X_REG_LDO2_CONT] =	RD | WR,
	[DA906X_REG_LDO3_CONT] =	RD | WR,
	[DA906X_REG_LDO4_CONT] =	RD | WR,
	[DA906X_REG_LDO5_CONT] =	RD | WR,
	[DA906X_REG_LDO6_CONT] =	RD | WR,
	[DA906X_REG_LDO7_CONT] =	RD | WR,
	[DA906X_REG_LDO8_CONT] =	RD | WR,
	[DA906X_REG_LDO9_CONT] =	RD | WR,
	[DA906X_REG_LDO10_CONT] =	RD | WR,
	[DA906X_REG_LDO11_CONT] =	RD | WR,
	[DA906X_REG_VIB] =		RD | WR,
	[DA906X_REG_DVC_1] =		RD | WR,
	[DA906X_REG_DVC_2] =		RD | WR,

	[DA906X_REG_ADC_MAN] =		RD | WR | VOL,
	[DA906X_REG_ADC_CONT] =		RD | WR,
	[DA906X_REG_VSYS_MON] =		RD | WR,
	[DA906X_REG_ADC_RES_L] =	RD | VOL,
	[DA906X_REG_ADC_RES_H] =	RD | VOL,
	[DA906X_REG_VSYS_RES] =		RD | VOL,
	[DA906X_REG_ADCIN1_RES] =	RD | VOL,
	[DA906X_REG_ADCIN2_RES] =	RD | VOL,
	[DA906X_REG_ADCIN3_RES] =	RD | VOL,
	[DA906X_REG_MON1_RES] =		RD | VOL,
	[DA906X_REG_MON2_RES] =		RD | VOL,
	[DA906X_REG_MON3_RES] =		RD | VOL,

	[DA906X_REG_COUNT_S] =		RD | WR | VOL,
	[DA906X_REG_COUNT_MI] =		RD | WR | VOL,
	[DA906X_REG_COUNT_H] =		RD | WR | VOL,
	[DA906X_REG_COUNT_D] =		RD | WR | VOL,
	[DA906X_REG_COUNT_MO] =		RD | WR | VOL,
	[DA906X_REG_COUNT_Y] =		RD | WR | VOL,
	[DA906X_REG_ALARM_MI] =		RD | WR | VOL,
	[DA906X_REG_ALARM_H] =		RD | WR | VOL,
	[DA906X_REG_ALARM_D] =		RD | WR | VOL,
	[DA906X_REG_ALARM_MO] =		RD | WR | VOL,
	[DA906X_REG_ALARM_Y] =		RD | WR | VOL,
	[DA906X_REG_SECOND_A] =		RD | VOL,
	[DA906X_REG_SECOND_B] =		RD | VOL,
	[DA906X_REG_SECOND_C] =		RD | VOL,
	[DA906X_REG_SECOND_D] =		RD | VOL,

	[DA9210_REG_STATUS_A] =		COPMIC | RD | VOL,
	[DA9210_REG_STATUS_B] =		COPMIC | RD | VOL,
	[DA9210_REG_EVENT_A] =		COPMIC | RD | WR | VOL,
	[DA9210_REG_EVENT_B] =		COPMIC | RD | WR | VOL,
	[DA9210_REG_MASK_A] =		COPMIC | RD | WR,
	[DA9210_REG_MASK_B] =		COPMIC | RD | WR,
	[DA9210_REG_CONTROL_A] =	COPMIC | RD | WR,
	[DA9210_REG_GPIO_0_1] =		COPMIC | RD | WR,
	[DA9210_REG_GPIO_2_3] =		COPMIC | RD | WR,
	[DA9210_REG_GPIO_4_5] =		COPMIC | RD | WR,
	[DA9210_REG_GPIO_6] =		COPMIC | RD | WR,

	[DA9210_REG_BUCK_CONT] =	COPMIC | RD | WR,

	[DA906X_REG_SEQ] =		RD | WR,
	[DA906X_REG_SEQ_TIMER] =	RD | WR,
	[DA906X_REG_ID_2_1] =		RD | WR,
	[DA906X_REG_ID_4_3] =		RD | WR,
	[DA906X_REG_ID_6_5] =		RD | WR,
	[DA906X_REG_ID_8_7] =		RD | WR,
	[DA906X_REG_ID_10_9] =		RD | WR,
	[DA906X_REG_ID_12_11] =		RD | WR,
	[DA906X_REG_ID_14_13] =		RD | WR,
	[DA906X_REG_ID_16_15] =		RD | WR,
	[DA906X_REG_ID_18_17] =		RD | WR,
	[DA906X_REG_ID_20_19] =		RD | WR,
	[DA906X_REG_ID_22_21] =		RD | WR,
	[DA906X_REG_ID_24_23] =		RD | WR,
	[DA906X_REG_ID_26_25] =		RD | WR,
	[DA906X_REG_ID_28_27] =		RD | WR,
	[DA906X_REG_ID_30_29] =		RD | WR,
	[DA906X_REG_ID_32_31] =		RD | WR,
	[DA906X_REG_SEQ_A] =		RD | WR,
	[DA906X_REG_SEQ_B] =		RD | WR,
	[DA906X_REG_WAIT] =		RD | WR,
	[DA906X_REG_EN_32K] =		RD | WR,
	[DA906X_REG_RESET] =		RD | WR,

	[DA906X_REG_BUCK_ILIM_A] =	RD | WR,
	[DA906X_REG_BUCK_ILIM_B] =	RD | WR,
	[DA906X_REG_BUCK_ILIM_C] =	RD | WR,
	[DA906X_REG_BCORE2_CFG] =	RD | WR,
	[DA906X_REG_BCORE1_CFG] =	RD | WR,
	[DA906X_REG_BPRO_CFG] =		RD | WR,
	[DA906X_REG_BIO_CFG] =		RD | WR,
	[DA906X_REG_BMEM_CFG] =		RD | WR,
	[DA906X_REG_BPERI_CFG] =	RD | WR,
	[DA906X_REG_VBCORE2_A] =	RD | WR,
	[DA906X_REG_VBCORE1_A] =	RD | WR,
	[DA906X_REG_VBPRO_A] =		RD | WR,
	[DA906X_REG_VBMEM_A] =		RD | WR,
	[DA906X_REG_VBIO_A] =		RD | WR,
	[DA906X_REG_VBPERI_A] =		RD | WR,
	[DA906X_REG_VLDO1_A] =		RD | WR,
	[DA906X_REG_VLDO2_A] =		RD | WR,
	[DA906X_REG_VLDO3_A] =		RD | WR,
	[DA906X_REG_VLDO4_A] =		RD | WR,
	[DA906X_REG_VLDO5_A] =		RD | WR,
	[DA906X_REG_VLDO6_A] =		RD | WR,
	[DA906X_REG_VLDO7_A] =		RD | WR,
	[DA906X_REG_VLDO8_A] =		RD | WR,
	[DA906X_REG_VLDO9_A] =		RD | WR,
	[DA906X_REG_VLDO10_A] =		RD | WR,
	[DA906X_REG_VLDO11_A] =		RD | WR,
	[DA906X_REG_VBCORE2_B] =	RD | WR,
	[DA906X_REG_VBCORE1_B] =	RD | WR,
	[DA906X_REG_VBPRO_B] =		RD | WR,
	[DA906X_REG_VBMEM_B] =		RD | WR,
	[DA906X_REG_VBIO_B] =		RD | WR,
	[DA906X_REG_VBPERI_B] =		RD | WR,
	[DA906X_REG_VLDO1_B] =		RD | WR,
	[DA906X_REG_VLDO2_B] =		RD | WR,
	[DA906X_REG_VLDO3_B] =		RD | WR,
	[DA906X_REG_VLDO4_B] =		RD | WR,
	[DA906X_REG_VLDO5_B] =		RD | WR,
	[DA906X_REG_VLDO6_B] =		RD | WR,
	[DA906X_REG_VLDO7_B] =		RD | WR,
	[DA906X_REG_VLDO8_B] =		RD | WR,
	[DA906X_REG_VLDO9_B] =		RD | WR,
	[DA906X_REG_VLDO10_B] =		RD | WR,
	[DA906X_REG_VLDO11_B] =		RD | WR,

	[DA906X_REG_BBAT_CONT] =	RD | WR,

	[DA906X_REG_GPO11_LED] =	RD | WR,
	[DA906X_REG_GPO14_LED] =	RD | WR,
	[DA906X_REG_GPO15_LED] =	RD | WR,

	[DA906X_REG_ADC_CFG] =		RD | WR,
	[DA906X_REG_AUTO1_HIGH] =	RD | WR,
	[DA906X_REG_AUTO1_LOW] =	RD | WR,
	[DA906X_REG_AUTO2_HIGH] =	RD | WR,
	[DA906X_REG_AUTO2_LOW] =	RD | WR,
	[DA906X_REG_AUTO3_HIGH] =	RD | WR,
	[DA906X_REG_AUTO3_LOW] =	RD | WR,

	[DA9210_REG_BUCK_ILIM] =	COPMIC | RD | WR,
	[DA9210_REG_BUCK_CONF1] =	COPMIC | RD | WR,
	[DA9210_REG_BUCK_CONF2] =	COPMIC | RD | WR,
	[DA9210_REG_VBUCK_AUTO] =	COPMIC | RD | WR,
	[DA9210_REG_VBUCK_BASE] =	COPMIC | RD | WR,
	[DA9210_REG_VBUCK_MAX_DVC] =	COPMIC | RD | WR,
	[DA9210_REG_VBUCK_DVC] =	COPMIC | RD | VOL,
	[DA9210_REG_VBUCK_A] =		COPMIC | RD | WR,
	[DA9210_REG_VBUCK_B] =		COPMIC | RD | WR,

	[DA906X_REG_T_OFFSET] =		RD,
	[DA906X_REG_CONFIG_H] =		RD,
	[DA906X_REG_CONFIG_I] =		RD | WR,
	[DA906X_REG_MON_REG_1] =	RD | WR,
	[DA906X_REG_MON_REG_2] =	RD | WR,
	[DA906X_REG_MON_REG_3] =	RD | WR,
	[DA906X_REG_MON_REG_4] =	RD | WR,
	[DA906X_REG_MON_REG_5] =	RD | VOL,
	[DA906X_REG_MON_REG_6] =	RD | VOL,
	[DA906X_REG_TRIM_CLDR] =	RD,

	[DA906X_REG_GP_ID_0] =		RD | WR,
	[DA906X_REG_GP_ID_1] =		RD | WR,
	[DA906X_REG_GP_ID_2] =		RD | WR,
	[DA906X_REG_GP_ID_3] =		RD | WR,
	[DA906X_REG_GP_ID_4] =		RD | WR,
	[DA906X_REG_GP_ID_5] =		RD | WR,
	[DA906X_REG_GP_ID_6] =		RD | WR,
	[DA906X_REG_GP_ID_7] =		RD | WR,
	[DA906X_REG_GP_ID_8] =		RD | WR,
	[DA906X_REG_GP_ID_9] =		RD | WR,
	[DA906X_REG_GP_ID_10] =		RD | WR,
	[DA906X_REG_GP_ID_11] =		RD | WR,
	[DA906X_REG_GP_ID_12] =		RD | WR,
	[DA906X_REG_GP_ID_13] =		RD | WR,
	[DA906X_REG_GP_ID_14] =		RD | WR,
	[DA906X_REG_GP_ID_15] =		RD | WR,
	[DA906X_REG_GP_ID_16] =		RD | WR,
	[DA906X_REG_GP_ID_17] =		RD | WR,
	[DA906X_REG_GP_ID_18] =		RD | WR,
	[DA906X_REG_GP_ID_19] =		RD | WR,

	[DA906X_REG_CHIP_ID] =		RD,
	[DA906X_REG_CHIP_VARIANT] =	RD,
};

static bool da906x_reg_readable(struct device *dev, unsigned int reg)
{
	if (reg < DA906X_MAPPING_BASE) {
		return true;
	} else {
		reg -= DA906X_MAPPING_BASE;
		if (da906x_reg_flg[reg] & RD) {
			if (da906x_reg_flg[reg] & COPMIC) {
				struct da906x *da906x = dev_get_drvdata(dev);

				if (!da906x->da9210)
					return false;
			}

			return true;
		}

		return false;
	}
}

static bool da906x_reg_writable(struct device *dev, unsigned int reg)
{
	if (reg < DA906X_MAPPING_BASE) {
		return true;
	} else {
		reg -= DA906X_MAPPING_BASE;
		if (da906x_reg_flg[reg] & WR) {
			if (da906x_reg_flg[reg] & COPMIC) {
				struct da906x *da906x = dev_get_drvdata(dev);

				if (!da906x->da9210)
					return false;
			}

			return true;
		}

		return false;
	}
}

static bool da906x_reg_volatile(struct device *dev, unsigned int reg)
{
	if (reg < DA906X_MAPPING_BASE) {
		switch (reg) {
		case DA906X_REG_PAGE_CON:    /* Use cache for page selector */
			return false;
		default:
			return true;
		}
	} else {
		reg -= DA906X_MAPPING_BASE;
		if (da906x_reg_flg[reg] & VOL) {
			if (da906x_reg_flg[reg] & COPMIC) {
				struct da906x *da906x = dev_get_drvdata(dev);

				if (!da906x->da9210)
					return false;
			}

			return true;
		}

		return false;
	}
}

static const struct regmap_range_cfg da906x_range_cfg[] = {
	{
		.range_min = DA906X_MAPPING_BASE,
		.range_max = DA906X_MAPPING_BASE +
			     ARRAY_SIZE(da906x_reg_flg) - 1,
		.selector_reg = DA906X_REG_PAGE_CON,
		.selector_mask = 1 << DA906X_I2C_PAGE_SEL_SHIFT,
		.selector_shift = DA906X_I2C_PAGE_SEL_SHIFT,
		.window_start = 0,
		.window_len = 256,
	}
};

struct regmap_config da906x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.ranges = da906x_range_cfg,
	.num_ranges = ARRAY_SIZE(da906x_range_cfg),
	.max_register = DA906X_MAPPING_BASE + ARRAY_SIZE(da906x_reg_flg) - 1,

	.cache_type = REGCACHE_RBTREE,

	.writeable_reg = &da906x_reg_writable,
	.readable_reg = &da906x_reg_readable,
	.volatile_reg = &da906x_reg_volatile,
};

#ifdef OF_CONFIG
static struct of_device_if odin_dialog_dt_ids[] = {
	{.compatible = "da906x-regulator", },
	{.compatible = "da906x-misc", },
	{.compatible = "da906x-gpio", },
	{.compatible = "da906x-rtc", },
}
#endif

static int da906x_ipc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct ipc_client *ipc;
	struct device *dev = &pdev->dev;
	struct da906x_pdata *pdata;
	struct da906x *da906x;
	int ret = 0, irq;

	if (pdev->dev.of_node) {
		pdata = kzalloc(sizeof(struct da906x_pdata), GFP_KERNEL);
		if (pdata == NULL)
			return -ENOMEM;

		ret = of_property_read_u32(pdev->dev.of_node, "num_gpio", &pdata->num_gpio);
		if (ret < 0) {
			ret = -ENODEV;
			goto out_free;
		}
	} else {
		pdata = pdev->dev.platform_data;
		if (pdata == NULL)
			return -ENODEV;
	}

	ret = gpio_request(pdata->num_gpio, "pmic-irq");
	if (ret < 0)
		goto err;

	ret = gpio_direction_input(pdata->num_gpio);
	if (ret < 0)
		goto err;

	irq = gpio_to_irq(pdata->num_gpio);
	if (irq < 0)
		goto err;

	ipc = kzalloc(sizeof(struct ipc_client), GFP_KERNEL);
	if (ipc == NULL) {
		printk(" %s Platform data not specified.\n",__func__);
		ret = -ENOMEM;
		goto err;
	}
	ipc->slot = DA906X_I2C_SLOT;
	ipc->addr = DA906X_SLAVE_ADDRESS;
	ipc->dev  = dev;

	da906x = kzalloc(sizeof(struct da906x), GFP_KERNEL);
	if (da906x == NULL) {
		ret = -ENOMEM;
		goto free_mem_ipc;
	}

	platform_set_drvdata(pdev, da906x);
	da906x->dev = dev;

	da906x->regmap = devm_regmap_init_ipc(ipc, &da906x_regmap_config);
	if (IS_ERR(da906x->regmap)) {
		ret = PTR_ERR(da906x->regmap);
		dev_err(da906x->dev, "Failed to allocate register map: %d\n",
			ret);
		goto free_mem;
	}

	kfree(pdata);

	return da906x_device_init(da906x, irq);

free_mem:
	kfree(da906x);
free_mem_ipc:
	kfree(ipc);
err:
	gpio_free(pdata->num_gpio);
out_free:
	kfree(pdata);

	return ret;
}

static int da906x_ipc_remove(struct platform_device *pdev)
{
	struct da906x *da906x = platform_get_drvdata(pdev);

	da906x_device_exit(da906x);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id da906x_dt_match[] __initdata = {
	{.compatible = "LG,odin-da906x-pmic",},
	{}
};
#endif

static struct platform_driver da906x_ipc_driver = {
	.driver = {
		.name = "da906x",
		.owner = THIS_MODULE,
	},
#ifdef CONFIG_OF
	.driver.of_match_table = of_match_ptr(da906x_dt_match),
#endif
	.probe    = da906x_ipc_probe,
	.remove   = da906x_ipc_remove,
};

static int __init da906x_ipc_init(void)
{
	int ret;

	ret = platform_driver_register(&da906x_ipc_driver);
	if (ret != 0)
		pr_err("Failed to register da906x IPC driver\n");

	return ret;
}
module_init(da906x_ipc_init);

static void __exit da906x_ipc_exit(void)
{
	platform_driver_unregister(&da906x_ipc_driver);
}
module_exit(da906x_ipc_exit);
