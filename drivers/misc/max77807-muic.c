/*
 * Maxim MAX77807 MUIC Driver
 *
 * Copyright (C) 2014 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/max77807/max77807.h>

#define M2SH  __CONST_FFS

#define DRIVER_DESC    "MAX77807 USBSW Driver"
#define DRIVER_NAME    MAX77807_USBSW_NAME
#define DRIVER_VERSION "1.0"
#define DRIVER_AUTHOR  "TaiEup Kim <clark.kim@maximintegrated.com>"

struct max77807_usbsw
{
    struct device               *dev;
    struct max77807_io          *io;;
};

static struct max77807_usbsw *usbsw = NULL;

int max77807_usbsw_read(int addr, u8 *val)
{
	unsigned int buf = 0;
	int rc = 0;

	if(!usbsw) {
		printk("%s Fails by usbsw\n", __func__);
		return -EINVAL;
	}

	rc = regmap_read(usbsw->io->regmap, (unsigned int)addr, &buf);

	if (likely(!IS_ERR_VALUE(rc))) {
		*val = (u8)buf;
	}
	return rc;
}
EXPORT_SYMBOL_GPL(max77807_usbsw_read);

int max77807_usbsw_write(int addr, u8 val)
{
	unsigned int buf = (unsigned int)val;

	if(!usbsw) {
		printk("%s Fails by usbsw\n", __func__);
		return -EINVAL;
	}

	return regmap_write(usbsw->io->regmap, (unsigned int)addr, buf);
}
EXPORT_SYMBOL_GPL(max77807_usbsw_write);


#ifdef CONFIG_OF
static struct of_device_id max77807_muic_of_ids[] = {
	{ .compatible = "maxim,"MAX77807_USBSW_NAME },
	{ },
};
MODULE_DEVICE_TABLE(of, max77807_muic_of_ids);
#endif /* CONFIG_OF */

static int max77807_muic_probe (struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77807_dev *chip = dev_get_drvdata(dev->parent);
	struct max77807_io *io = max77807_get_io(chip);
	struct max77807_usbsw *local_usbsw = NULL;
	int rc;
	u8 val;

	local_usbsw = devm_kzalloc(dev, sizeof(*local_usbsw), GFP_KERNEL);
	if (unlikely(!io)) {
		log_err("out of memory (%uB requested)\n", (unsigned int)sizeof(*local_usbsw));
		return -ENOMEM;
	}

	local_usbsw->io = io;
	local_usbsw->dev = dev;

	dev_set_drvdata(dev, local_usbsw);

	usbsw = local_usbsw;

	/* read ID */
	rc = max77807_usbsw_read(REG_USBSW_ID, &val);
	if (rc != 0)
		goto abort;

	/* write interrupt mask */
	rc = max77807_usbsw_write(REG_USBSW_INTMASK2, BIT_INT2_VBVOLT);
	if (rc != 0)
		goto abort;

	printk(DRIVER_DESC "driver "DRIVER_VERSION" installed\n");

	return 0;

abort:
	dev_set_drvdata(dev, NULL);
	devm_kfree(dev, local_usbsw);
	usbsw = NULL;
	printk(DRIVER_DESC "driver "DRIVER_VERSION" not installed, rc=%d\n", rc);
	return rc;
}

static int max77807_muic_remove (struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_set_drvdata(dev, NULL);
	usbsw = NULL;
	return 0;
}

static struct platform_driver max77807_muic_driver =
{
	.driver.name            = DRIVER_NAME,
	.driver.owner           = THIS_MODULE,
#ifdef CONFIG_OF
	.driver.of_match_table  = max77807_muic_of_ids,
#endif /* CONFIG_OF */
	.probe                  = max77807_muic_probe,
	.remove                 = max77807_muic_remove,
};

static int __init max77807_muic_init(void)
{
	return platform_driver_register(&max77807_muic_driver);
}
module_init(max77807_muic_init);

static void __exit max77807_muic_exit(void)
{
	platform_driver_unregister(&max77807_muic_driver);
}
module_exit(max77807_muic_exit);

MODULE_ALIAS("platform:max77807-vibrator");
MODULE_AUTHOR("TaiEup Kim<clark.kim@maximintegrated.com>");
MODULE_DESCRIPTION("MAX77807 vibrator driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
