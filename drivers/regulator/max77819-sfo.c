/*
 * SFO regulator for Maxim MAX77819
 *
 * Copyright 2013 Maxim Integrated Products, Inc.
 * Gyungoh Yoo <jack.yoo@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of.h>
#include <linux/version.h>
#include <linux/mfd/max77819.h>
#include <linux/regulator/max77819-sfo.h>

/* Register */
#define MAX77819_SAFEOUTCTRL	0x4B

/* MAX77819_SAFEOUTCTRL */
#define MAX77819_SAFEOUT		0x03
#define MAX77819_ACTDISSAFE		0x10
#define MAX77819_SAFEOUT_EN		0x40

struct max77819_sfo
{
	struct regulator_dev *rdev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
	struct regmap *regmap;
#endif	
};

const static unsigned int max77819_sfo_volt_table[] =
{
	4850000, 4900000, 4950000, 3300000,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
static int max77819_list_voltage(struct regulator_dev *rdev, unsigned int selector)
{
	return max77819_sfo_volt_table[selector];
}

static int max77819_get_voltage_sel(struct regulator_dev *rdev)
{
	struct max77819_sfo *max77819_sfo = rdev_get_drvdata(rdev);
	unsigned int value;
	int ret;
	
	ret = regmap_read(max77819_sfo->regmap, MAX77819_SAFEOUTCTRL, &value);
	if (IS_ERR_VALUE(ret))
		return ret;
		
	return (value & MAX77819_SAFEOUT) >> M2SH(MAX77819_SAFEOUT);
}

static int max77819_set_voltage_sel(struct regulator_dev *rdev, unsigned sel)
{
	struct max77819_sfo *max77819_sfo = rdev_get_drvdata(rdev);
	
	return regmap_update_bits(max77819_sfo->regmap, MAX77819_SAFEOUTCTRL,
			MAX77819_SAFEOUT, sel << M2SH(MAX77819_SAFEOUT));
}

static int max77819_is_enabled(struct regulator_dev *rdev)
{
	struct max77819_sfo *max77819_sfo = rdev_get_drvdata(rdev);
	unsigned int value;
	int ret;
	
	ret = regmap_read(max77819_sfo->regmap, MAX77819_SAFEOUTCTRL, &value);
	if (IS_ERR_VALUE(ret))
		return ret;
		
	return (value & MAX77819_SAFEOUT_EN) >> M2SH(MAX77819_SAFEOUT_EN);
}

static int max77819_enable(struct regulator_dev *rdev)
{
	struct max77819_sfo *max77819_sfo = rdev_get_drvdata(rdev);
	
	return regmap_update_bits(max77819_sfo->regmap, MAX77819_SAFEOUTCTRL,
			MAX77819_SAFEOUT_EN, 0xFF);
}

static int max77819_disable(struct regulator_dev *rdev)
{
	struct max77819_sfo *max77819_sfo = rdev_get_drvdata(rdev);
	
	return regmap_update_bits(max77819_sfo->regmap, MAX77819_SAFEOUTCTRL,
			MAX77819_SAFEOUT_EN, 0x00);
}
#endif

static struct regulator_ops max77819_sfo_ops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
	.list_voltage		= max77819_list_voltage,
	.is_enabled		= max77819_is_enabled,
	.enable			= max77819_enable,
	.disable		= max77819_disable,
	.get_voltage_sel	= max77819_get_voltage_sel,
	.set_voltage_sel	= max77819_set_voltage_sel,
#else
	.list_voltage		= regulator_list_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
#endif
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
const
#endif
	static struct regulator_desc max77819_sfo_desc =
{
	.name		= "SFO",
	.ops		= &max77819_sfo_ops,
	.type		= REGULATOR_VOLTAGE,
	.owner		= THIS_MODULE,
	.n_voltages	= ARRAY_SIZE(max77819_sfo_volt_table),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
	.volt_table	= max77235_sfo_volt_table,
	.vsel_reg	= MAX77819_SAFEOUTCTRL,
	.vsel_mask	= MAX77819_SAFEOUT,
	.enable_reg	= MAX77819_SAFEOUTCTRL,
	.enable_mask	= MAX77819_SAFEOUT_EN,
#endif
};

#ifdef CONFIG_OF
static struct max77819_sfo_platform_data *max77819_reg_parse_dt(struct device *dev)
{
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *np;
	struct max77819_sfo_platform_data *pdata;

	if (unlikely(nproot == NULL))
		return ERR_PTR(-EINVAL);

	np = of_find_node_by_name(nproot, "safeout");
	if (unlikely(np == NULL))
	{
		dev_err(dev, "safeout node not found\n");
		return ERR_PTR(-EINVAL);
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	pdata->active_discharge = of_property_read_bool(np, "active-discharge");
	pdata->initdata = of_get_regulator_init_data(dev, np);

	return pdata;
}
#endif

static __devinit int max77819_sfo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77819_sfo_platform_data *pdata;
	struct max77819_sfo *max77819_sfo;
	struct max77819 *max77819 = dev_get_drvdata(dev->parent);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
	struct regulator_config config;
#endif
	struct regulator_dev *rdev;
	int ret;

	max77819_sfo = devm_kzalloc(dev, sizeof(struct max77819_sfo), GFP_KERNEL);
	if (unlikely(!max77819_sfo))
		return -ENOMEM;

#ifdef CONFIG_OF
	pdata = max77819_reg_parse_dt(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
	max77819_sfo->regmap = max77819->regmap;
#endif	
	platform_set_drvdata(pdev, max77819_sfo);

	ret = regmap_update_bits(max77819->regmap, MAX77819_SAFEOUTCTRL, MAX77819_ACTDISSAFE,
			pdata->active_discharge ? MAX77819_ACTDISSAFE : 0);
	if (IS_ERR_VALUE(ret))
		return ret;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
	rdev = regulator_register(&max77819_sfo_desc, dev,
			pdata->initdata, max77819_sfo, NULL);
#else
	config.dev = dev;
	config.init_data = pdata->initdata;
	config.regmap = max77819->regmap;

	rdev = regulator_register(&max77819_sfo_desc, &config);
#endif
	if (IS_ERR(rdev))
	{
		dev_err(dev, "regulator register failed to register regulator : %ld\n", PTR_ERR(rdev));
		return PTR_ERR(rdev);
	}
	max77819_sfo->rdev = rdev;

#ifdef CONFIG_OF
	devm_kfree(dev, pdata);
#endif
	return 0;
}

static __devexit int max77819_sfo_remove(struct platform_device *pdev)
{
	struct max77819_sfo *max77819_sfo = dev_get_platdata(&pdev->dev);
	
	regulator_unregister(max77819_sfo->rdev);
	
	return 0;
}

static struct platform_driver max77819_sfo_driver =
{
	.driver =
	{
		.name = "max77235-sfo",
		.owner = THIS_MODULE,
	},
	.probe	= max77819_sfo_probe,
	.remove	= __devexit_p(max77819_sfo_remove),
};

static int __init max77819_sfo_init(void)
{
	return platform_driver_register(&max77819_sfo_driver);
}
module_init(max77819_sfo_init);

static void __exit max77819_sfo_exit(void)
{
	platform_driver_unregister(&max77819_sfo_driver);
}
module_exit(max77819_sfo_exit);

MODULE_ALIAS("platform:max77819-sfo");
MODULE_DESCRIPTION("Safeout LDO driver for MAX77819");
MODULE_AUTHOR("Gyungoh Yoo<jack.yoo@maxim-ic.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
