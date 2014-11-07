/*
 * Maxim MAX77665 MFD driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77665.h>

/* registers */
#define MAX77665_PMICID		0x20
#define MAX77665_PMICREV	0x21
#define MAX77665_INTSRC		0x22
#define MAX77665_INTSRC_MASK	0x23

/* MAX77665_PMICID */
#define MAX77665_ID		0xFF

/* MAX77665_PMICREV */
#define MAX77665_REV		0x07
#define MAX77665_VERSION	0xF8

/* MAX77665_INTSRC */
#define MAX77665_CHGR_INT	0x01
#define MAX77665_TOP_INT	0x02
#define MAX77665_FLASH_INT	0x04
#define MAX77665_MUIC_INT	0x08

/* MAX77665_INTSRC_MASK */
#define MAX77665_CHGR_INT_MASK	0x01
#define MAX77665_TOP_INT_MASK	0x02
#define MAX77665_FLASH_INT_MASK	0x04
#define MAX77665_MUIC_INT_MASK	0x08

const static struct regmap_irq max77665_irqs[] =
{
	[MAX77665_INT_CHGR] = { .mask = MAX77665_CHGR_INT_MASK, },
	[MAX77665_INT_TOP] = { .mask = MAX77665_TOP_INT_MASK, },
	[MAX77665_INT_FLASH] = { .mask = MAX77665_FLASH_INT_MASK, },
	[MAX77665_INT_MUIC] = { .mask = MAX77665_MUIC_INT_MASK, },
};

const static struct regmap_irq_chip max77665_irq_chip =
{
	.name = "max77665",
	.irqs = max77665_irqs,
	.num_irqs = ARRAY_SIZE(max77665_irqs),
	.num_regs = 1,
	.status_base = MAX77665_INTSRC,
	.mask_base = MAX77665_INTSRC_MASK,
};

const struct regmap_config max77665_regmap_config =
{
	.reg_bits = 8,
	.val_bits = 8,
};

#ifdef CONFIG_OF
static struct mfd_cell max77665_devs[] =
{
	{ .name = "max77665-charger", },
	{ .name = "max77665-sfo", },
	{ .name = "max77665-flash", },
};

static struct max77665_platform_data *max77665_parse_dt(struct device *dev)
{
	struct max77665_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

#if defined(CONFIG_MACH_ODIN_HDK)
	if (of_property_read_u32(dev->of_node, "gpio-num", &pdata->irq_gpio) != 0)
		return ERR_PTR(-EINVAL);
	printk(KERN_CRIT "%s: get irq_gpio %d\n", __func__, pdata->irq_gpio);
#else
	/* original code */
	pdata->irq = irq_of_parse_and_map(dev->of_node, 0);
	if (pdata->irq == 0)
		return ERR_PTR(-EINVAL);
#endif

	pdata->sub_devices = max77665_devs;
	pdata->num_subdevs = ARRAY_SIZE(max77665_devs);

	return pdata;
}

#if defined(CONFIG_MACH_ODIN_HDK)
static int max77665_update_dt(struct device *dev, struct max77665 *max77665)
{
	struct property *pp;
	char *name = "irq-base";

	pp = NULL;
	printk(KERN_CRIT "%s: name len %d\n", __func__, strlen(name));
	pp = kzalloc(sizeof(struct property) + strlen(name) + 1 + 4, GFP_KERNEL);
	if (pp == NULL)
		return -EINVAL;

	pp->name = ((char *)pp) + sizeof(struct property);
	strcpy(pp->name, name);
	pp->length = 4;
	pp->value = pp->name + strlen(name) + 1;
	*((u32 *)pp->value) =
			cpu_to_be32(regmap_irq_chip_get_base(max77665->irq_data));
	pp->next = NULL;

	printk(KERN_CRIT "%s: pp->name %s, pp->value 0x%x -> 0x%x -> be32_to_cpu 0x%x\n",
		__func__, pp->name, (unsigned int)pp->value,
		*((u32 *)pp->value), be32_to_cpup(pp->value));

	if (of_update_property(dev->of_node, pp)) {
		printk(KERN_INFO "%s: failed creating dev property\n", __func__);
		return -EINVAL;
	}

	return 0;
}
#endif
#endif

static int max77665_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct max77665_platform_data *pdata;
	struct max77665 *max77665;
	int ret;

	max77665 = devm_kzalloc(dev, sizeof(struct max77665), GFP_KERNEL);
	if (unlikely(max77665 == NULL))
	{
		dev_err(dev, "%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, max77665);

#ifdef CONFIG_OF
	pdata = max77665_parse_dt(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
#if !defined(CONFIG_MACH_ODIN_HDK)
	client->irq = pdata->irq;
#endif
#else
	printk(KERN_INFO "%s: get pdata\n", __func__);
	pdata = dev_get_platdata(dev);
#endif

#if defined(CONFIG_MACH_ODIN_HDK)
	ret = gpio_request(pdata->irq_gpio, "max77665_irq");
	if (ret < 0)
		goto out_free;

	ret = gpio_direction_input(pdata->irq_gpio);
	if (ret < 0)
		goto out_free;

	client->irq = gpio_to_irq(pdata->irq_gpio);
	if (client->irq < 0)
		goto out_free;
#endif

	printk(KERN_INFO "%s: gpio_to_irq(%d) -> %d\n", __func__, pdata->irq_gpio, client->irq);
	max77665->regmap = devm_regmap_init_i2c(client, &max77665_regmap_config);
	if (IS_ERR(max77665->regmap))
		return PTR_ERR(max77665->regmap);

	ret = regmap_add_irq_chip(max77665->regmap, client->irq,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		-1, &max77665_irq_chip, &max77665->irq_data);

	printk(KERN_INFO "%s: irq_base %d\n", __func__, regmap_irq_chip_get_base(max77665->irq_data));
	if (IS_ERR_VALUE(ret))
		return ret;

#ifdef CONFIG_OF
#if defined(CONFIG_MACH_ODIN_HDK)
	max77665_update_dt(dev, max77665);
#endif
#endif

	ret = mfd_add_devices(&client->dev, -1, pdata->sub_devices,
			pdata->num_subdevs, NULL, 0, NULL);
	if (IS_ERR_VALUE(ret))
		dev_err(dev, "mfd_add_devices failed, rc=%d\n", ret);

	return ret;

#if defined(CONFIG_MACH_ODIN_HDK)
out_free:
	printk(KERN_ERR "%s: gpio_request failed %d\n", __func__, pdata->irq_gpio);
	gpio_free(pdata->irq_gpio);
	return -EINVAL;
#endif
}

static int max77665_remove(struct i2c_client *client)
{
	struct max77665 *max77665 = i2c_get_clientdata(client);

	regmap_del_irq_chip(client->irq, max77665->irq_data);
	mfd_remove_devices(&client->dev);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id max77665_of_match[] = {
        { .compatible = "maxim,max77665" },
        { },
};
MODULE_DEVICE_TABLE(of, max77665_of_match);
#endif

static const struct i2c_device_id max77665_id[] =
{
	{ "max77665", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max77665_id);

static struct i2c_driver max77665_driver =
{
	.driver =
	{
		.name = "max77665",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = max77665_of_match,
#endif
	},
	.id_table = max77665_id,
	.probe = max77665_probe,
	.remove = max77665_remove,
};

static int __init max77665_init(void)
{
	return i2c_add_driver(&max77665_driver);
}
arch_initcall(max77665_init);

static void __exit max77665_exit(void)
{
	i2c_del_driver(&max77665_driver);
}
module_exit(max77665_exit);

MODULE_DESCRIPTION("Maxim MAX77665 MFD driver");
MODULE_AUTHOR("Gyungoh Yoo <jack.yoo@maxim-ic.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.2");
