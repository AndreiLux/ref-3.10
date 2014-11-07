 /*
 * da9066-i2c.c: I2C (Serial Communication) driver for DA9066
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
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regmap-ipc.h>
#include <linux/gpio.h>

#include <linux/mfd/da9066/pmic.h>
#include <linux/mfd/da9066/da9066_reg.h>
#include <linux/mfd/da9066/rtc.h>
#include <linux/mfd/da9066/core.h>

#define DA9066_I2C_SLOT		 0
#define DA9066_SLAVE_ADDRESS 0x92

struct regmap_config da9066_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int da9066_ipc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct ipc_client *ipc;
	struct device *dev = &pdev->dev;
	struct da9066_platform_data *pdata;
	struct da9066 *da9066;
	int ret = 0, irq;

	if (pdev->dev.of_node) {
		pdata = kzalloc(sizeof(struct da9066_platform_data), GFP_KERNEL);
		if (pdata == NULL) {
			return -ENOMEM;
		}

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
		printk("%s Platform data not specified.\n",__func__);
		ret = -ENOMEM;
		goto err;
	}
	ipc->slot = DA9066_I2C_SLOT;
	ipc->addr = DA9066_SLAVE_ADDRESS;
	ipc->dev  = dev;

	da9066 = kzalloc(sizeof(struct da9066), GFP_KERNEL);
	if (da9066 == NULL) {
		ret = -ENOMEM;
		goto free_mem_ipc;
	}

	platform_set_drvdata(pdev, da9066);
	da9066->dev = dev;

	da9066->regmap = devm_regmap_init_ipc(ipc, &da9066_regmap_config);
	if (IS_ERR(da9066->regmap)) {
		ret = PTR_ERR(da9066->regmap);
		dev_err(da9066->dev, "Failed to allocate register map: %d\n",
			ret);
		goto free_mem;
	}

	ret = da9066_device_init(da9066, irq, pdata);
	dev_info(da9066->dev, "IPC initialized ret=%d\n",ret);
	if (ret < 0)
		goto free_mem;

	return ret;

free_mem:
	kfree(da9066);
free_mem_ipc:
	kfree(ipc);
err:
	gpio_free(pdata->num_gpio);
out_free:
	kfree(pdata);

	return ret;
}

static int da9066_ipc_remove(struct platform_device *pdev)
{
	struct da9066 *da9066 = platform_get_drvdata(pdev);

	da9066_device_exit(da9066);
	kfree(da9066);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id da9066_dt_match[] __initdata = {
	{.compatible = "LG,odin-da9066-pmic",},
	{}
};
#endif

static struct platform_driver da9066_ipc_driver = {
	.driver = {
		.name = "da9066",
		.owner = THIS_MODULE,
	},
#ifdef CONFIG_OF
	.driver.of_match_table = of_match_ptr(da9066_dt_match),
#endif
	.probe = da9066_ipc_probe,
	.remove = da9066_ipc_remove,
};

static int __init da9066_ipc_init(void)
{
	int ret;

	ret = platform_driver_register(&da9066_ipc_driver);
	if (ret != 0)
		pr_err("Failed to register da906x IPC driver\n");

	return ret;
}

/* Initialised very early during bootup (in parallel with Subsystem init) */
module_init(da9066_ipc_init);

static void __exit da9066_ipc_exit(void)
{
	platform_driver_unregister(&da9066_ipc_driver);
}
module_exit(da9066_ipc_exit);

MODULE_AUTHOR("LG Electronics <hugh.kang@lge.com >");
MODULE_DESCRIPTION("IPC MFD driver for Dialog DA9066 PMIC");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DA9066_IPC);
