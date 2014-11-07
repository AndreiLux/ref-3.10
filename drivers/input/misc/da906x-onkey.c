/*
 * OnKey device driver for Dialog DA906x PMIC
 *
 * Copyright 2012 Dialog Semiconductors Ltd.
 *
 * Author:  <michal.hajduk@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

#include <linux/mfd/da906x/core.h>
#include <linux/mfd/da906x/registers.h>


struct da906x_onkey {
	struct	da906x *da906x;
	struct	input_dev *input;
	int irq;
};

static irqreturn_t da906x_onkey_irq_handler(int irq, void *data)
{
	struct da906x_onkey *onkey = data;
	unsigned int code;
	int ret;

	ret = da906x_reg_read(onkey->da906x, DA906X_REG_STATUS_A);
	if ((ret >= 0) && (ret & DA906X_NONKEY)) {
		dev_notice(&onkey->input->dev, "KEY_POWER pressed.\n");
		code = KEY_POWER;
	} else {
		dev_notice(&onkey->input->dev, "KEY_SLEEP pressed.\n");
		code = KEY_SLEEP;
	}

	/* Interrupt raised for key release only,
	   so report consecutive button press and release. */
	input_report_key(onkey->input, code, 1);
	input_report_key(onkey->input, code, 0);
	input_sync(onkey->input);

	return IRQ_HANDLED;
}

static int __init da906x_onkey_probe(struct platform_device *pdev)
{
	struct da906x *da906x = dev_get_drvdata(pdev->dev.parent);
	struct da906x_onkey *onkey;
	int ret = 0;

	onkey = devm_kzalloc(&pdev->dev, sizeof(struct da906x_onkey),
			     GFP_KERNEL);
	if (!onkey) {
		dev_err(&pdev->dev, "Failed to allocate memory.\n");
		return -ENOMEM;
	}

	onkey->input = input_allocate_device();
	if (!onkey->input) {
		dev_err(&pdev->dev, "Failed to allocated inpute device.\n");
		return -ENOMEM;
	}

	onkey->irq = platform_get_irq_byname(pdev, DA906X_DRVNAME_ONKEY);
	onkey->da906x = da906x;

	onkey->input->evbit[0] = BIT_MASK(EV_KEY);
	onkey->input->name = DA906X_DRVNAME_ONKEY;
	onkey->input->phys = DA906X_DRVNAME_ONKEY "/input0";
	onkey->input->dev.parent = &pdev->dev;
	input_set_capability(onkey->input, EV_KEY, KEY_POWER);
	input_set_capability(onkey->input, EV_KEY, KEY_SLEEP);

	ret = request_threaded_irq(onkey->irq, NULL, da906x_onkey_irq_handler,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, "ONKEY", onkey);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ.\n");
		goto err_input;
	}

	ret = input_register_device(onkey->input);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ.\n");
		goto err_irq;
	}

	platform_set_drvdata(pdev, onkey);

	/* Interrupt reacts on button release */
	da906x_reg_update(da906x, DA906X_REG_CONFIG_I,
			  DA906X_NONKEY_PIN_MASK, DA906X_NONKEY_PIN_SWDOWN);

	return 0;

err_irq:
	free_irq(onkey->da906x->irq_base + onkey->irq , onkey);
err_input:
	input_free_device(onkey->input);
	return ret;
}

static int __exit da906x_onkey_remove(struct platform_device *pdev)
{
	struct	da906x_onkey *onkey = platform_get_drvdata(pdev);

	free_irq(onkey->irq, onkey);
	input_unregister_device(onkey->input);
	return 0;
}

static struct platform_driver da906x_onkey_driver = {
	.probe	= da906x_onkey_probe,
	.remove	= da906x_onkey_remove,
	.driver	= {
		.name	= DA906X_DRVNAME_ONKEY,
		.owner	= THIS_MODULE,
	},
};

static int __init da906x_onkey_init(void)
{
	return platform_driver_register(&da906x_onkey_driver);
}
subsys_initcall(da906x_onkey_init);

MODULE_AUTHOR("Dialog Semiconductor <michal.hajduk@diasemi.com>");
MODULE_DESCRIPTION("Onkey driver for DA906X");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DA906X_DRVNAME_ONKEY);
