/*
 * SFO regulator for Maxim MAX77807
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
#include <linux/regmap.h>
#include <linux/mfd/max77807/max77807.h>
#include <linux/mfd/max77807/max77807-regulator.h>

#define M2SH  __CONST_FFS

/* Register */
#define MAX77807_SAFEOUTCTRL	0xC6

/* MAX77807_SAFEOUTCTRL */
#define SAFEOUTCTRL_ENSAFEOUT2			BIT (7)
#define SAFEOUTCTRL_ENSAFEOUT1			BIT (6)
#define SAFEOUTCTRL_ACTDISSAFEOUT2		BIT (5)
#define SAFEOUTCTRL_ACTDISSAFEOUT1		BIT (4)
#define SAFEOUTCTRL_SAFEOUT2			BITS(3,2)
#define SAFEOUTCTRL_SAFEOUT1			BITS(1,0)

enum {
	REG_SAFEOUT1,
	REG_SAFEOUT2,
	MAX77807_REGULATORS
};

struct max77807_regulator {
	struct regulator_dev *rdev[MAX77807_REGULATORS];
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
	struct regmap *regmap;
#endif
};

const static int max77807_ensafeout[] = {
	SAFEOUTCTRL_ENSAFEOUT1,
	SAFEOUTCTRL_ENSAFEOUT2,
};

const static int max77807_actdissafeout[] = {
	SAFEOUTCTRL_ACTDISSAFEOUT1,
	SAFEOUTCTRL_ACTDISSAFEOUT2,
};

const static int max77807_safeout[] = {
	SAFEOUTCTRL_SAFEOUT1,
	SAFEOUTCTRL_SAFEOUT2,
};

const static unsigned int max77807_sfo_volt_table[] =
{
	4850000, 4900000, 4950000, 3300000,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
static int max77807_list_voltage(struct regulator_dev *rdev, unsigned int selector)
{
	return max77807_sfo_volt_table[selector];
}

static int max77807_get_voltage_sel(struct regulator_dev *rdev)
{
	struct max77807_regulator *max77807_sfo = rdev_get_drvdata(rdev);
	unsigned int value;
	int ret;
	int rid = rdev_get_id(rdev);

	ret = regmap_read(max77807_sfo->regmap, MAX77807_SAFEOUTCTRL, &value);
	if (IS_ERR_VALUE(ret))
		return ret;

	value = (value & max77807_safeout[rid])>> M2SH(max77807_safeout[rid]);
	
	return value;
}

static int max77807_set_voltage_sel(struct regulator_dev *rdev, unsigned sel)
{
	struct max77807_regulator *max77807_sfo = rdev_get_drvdata(rdev);
	int rid = rdev_get_id(rdev);

	return regmap_update_bits(max77807_sfo->regmap, MAX77807_SAFEOUTCTRL,
			max77807_safeout[rid], sel << M2SH(max77807_safeout[rid]));
}

static int max77807_is_enabled(struct regulator_dev *rdev)
{
	struct max77807_regulator *max77807_sfo = rdev_get_drvdata(rdev);
	unsigned int value;
	int ret;
	int rid = rdev_get_id(rdev);

	ret = regmap_read(max77807_sfo->regmap, MAX77807_SAFEOUTCTRL, &value);
	if (IS_ERR_VALUE(ret))
		return ret;

	value = (value & max77807_ensafeout[rid]) >> M2SH(max77807_ensafeout[rid]);
	return value;
}

static int max77807_enable(struct regulator_dev *rdev)
{
	struct max77807_regulator *max77807_sfo = rdev_get_drvdata(rdev);
	int rid = rdev_get_id(rdev);

	return regmap_update_bits(max77807_sfo->regmap, MAX77807_SAFEOUTCTRL,
			max77807_ensafeout[rid], 0xFF);
}

static int max77807_disable(struct regulator_dev *rdev)
{
	struct max77807_regulator *max77807_sfo = rdev_get_drvdata(rdev);
	int rid = rdev_get_id(rdev);

	return regmap_update_bits(max77807_sfo->regmap, MAX77807_SAFEOUTCTRL,
			max77807_ensafeout[rid], 0x00);
}
#endif

static struct regulator_ops max77807_sfo_ops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
	.list_voltage		= max77807_list_voltage,
	.is_enabled		= max77807_is_enabled,
	.enable			= max77807_enable,
	.disable		= max77807_disable,
	.get_voltage_sel	= max77807_get_voltage_sel,
	.set_voltage_sel	= max77807_set_voltage_sel,
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
	static struct regulator_desc max77807_sfo_descs[] =
{
	{
		.name = "SAFEOUT1",
		.id = REG_SAFEOUT1,
		.ops = &max77807_sfo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.n_voltages	= ARRAY_SIZE(max77807_sfo_volt_table),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
		.volt_table	= max77807_sfo_volt_table,
		.vsel_reg	= MAX77807_SAFEOUTCTRL,
		.vsel_mask	= SAFEOUTCTRL_SAFEOUT1,
		.enable_reg	= MAX77807_SAFEOUTCTRL,
		.enable_mask	= SAFEOUTCTRL_ENSAFEOUT1,
#endif		
	},
	
	{
		.name = "SAFEOUT2",
		.id = REG_SAFEOUT2,
		.ops = &max77807_sfo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.n_voltages = ARRAY_SIZE(max77807_sfo_volt_table),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
		.volt_table = max77807_sfo_volt_table,
		.vsel_reg	= MAX77807_SAFEOUTCTRL,
		.vsel_mask	= SAFEOUTCTRL_SAFEOUT2,
		.enable_reg = MAX77807_SAFEOUTCTRL,
		.enable_mask	= SAFEOUTCTRL_ENSAFEOUT2,
#endif
	}, 
};

#ifdef CONFIG_OF
static struct max77807_regulator_platform_data *max77807_reg_parse_dt(struct device *dev)
{
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *regulators_np, reg_np;
	struct max77807_regulator_platform_data *pdata;
	struct max77807_regulator_data *rdata;
	
	if (unlikely(nproot == NULL))
		return ERR_PTR(-EINVAL);

	regulators_np = of_find_node_by_name(nproot, "regulators");
	if (unlikely(regulators_np == NULL))
	{
		dev_err(dev, "regulators node not found\n");
		return ERR_PTR(-EINVAL);
	}

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = of_get_child_count(regulators_np);

	rdata = devm_kzalloc(dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		of_node_put(regulators_np);
		dev_err(dev, "could not allocate memory for regulator data\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(max77807_sfo_desc); i++)
			if (!of_node_cmp(reg_np->name, max77807_sfo_desc[i].name))
				break;

		if (i == ARRAY_SIZE(max77807_sfo_desc)) {
			dev_warn(&pdev->dev, "don't know how to configure regulator %s\n",
				 reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(dev, reg_np);
		rdata->of_node = reg_np;
		rdata->active_discharge = of_property_read_bool(reg_np, "active-discharge");
		rdata++;
	}
	of_node_put(regulators_np);

	return pdata;
}
#endif

static int max77807_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77807_regulator_platform_data *pdata;
	struct max77807_regulator *max77807_sfo;
    struct max77807_dev *chip = dev_get_drvdata(dev->parent);
    struct max77807_io *io = max77807_get_io(chip);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
	struct regulator_config config;
#endif
	int ret, i, reg_id;

	max77807_sfo = devm_kzalloc(dev, sizeof(struct max77807_regulator), GFP_KERNEL);
	if (unlikely(!max77807_sfo))
		return -ENOMEM;

#ifdef CONFIG_OF
	pdata = max77807_reg_parse_dt(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
	max77807_sfo->regmap = io->regmap;
#endif
	platform_set_drvdata(pdev, max77807_sfo);

	for (i = 0; i < pdata->num_regulators; i++) {
		reg_id = pdata->regulators[i].id;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
		max77807_sfo->rdev[i] = regulator_register(&max77807_sfo_descs[reg_id], dev,
				pdata->regulators[i].initdata, max77807_sfo, NULL);
#else
		config.init_data = pdata->regulators[i].initdata;
		config.of_node = pdata->regulators[i].of_node;
		config.regmap = io->regmap;

		max77807_sfo->rdev[i] = regulator_register(&max77807_sfo_descs[reg_id], &config);
#endif		
		if (IS_ERR(max77807_sfo->rdev[i])) {
			ret = PTR_ERR(max77807_sfo->rdev[i]);
			dev_err(&pdev->dev,
				"regulator init failed for %d\n", i);
			max77807_sfo->rdev[i] = NULL;
			return ret;
		}

		ret = regmap_update_bits(io->regmap, MAX77807_SAFEOUTCTRL, max77807_actdissafeout[reg_id],
				pdata->regulators[i].active_discharge ? max77807_actdissafeout[reg_id] : 0);
		if (IS_ERR_VALUE(ret))
			return ret;		
	}

	return 0;
}

static int max77807_regulator_remove(struct platform_device *pdev)
{
	struct max77807_regulator *max77807_sfo = dev_get_platdata(&pdev->dev);
	int i;
	for (i = 0; i < ARRAY_SIZE(max77807_sfo_descs); i++)
		regulator_unregister(max77807_sfo->rdev[i]);

	return 0;
}

static struct platform_driver max77807_regulator_driver =
{
	.driver =
	{
		.name = "max77807-regulator",
		.owner = THIS_MODULE,
	},
	.probe	= max77807_regulator_probe,
	.remove	= max77807_regulator_remove,
};

static int __init max77807_regulator_init(void)
{
	return platform_driver_register(&max77807_regulator_driver);
}
module_init(max77807_regulator_init);

static void __exit max77807_regulator_exit(void)
{
	platform_driver_unregister(&max77807_regulator_driver);
}
module_exit(max77807_regulator_exit);

MODULE_ALIAS("platform:max77807-regulator");
MODULE_DESCRIPTION("Safeout LDO driver for MAX77807");
MODULE_AUTHOR("Tai Eup<clark.kim@maximintegrated.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");

