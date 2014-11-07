/*
 *  ONKEY driver for Dialog D2260 PMIC
 *
 *  Copyright (C) 2013 Dialog Semiconductor Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <linux/mfd/d2260/pmic.h>
#include <linux/mfd/d2260/registers.h>
#include <linux/mfd/d2260/core.h>
#include <asm/io.h>
#include <linux/platform_data/odin_tz.h>

#include <linux/wakelock.h>
#include <linux/board_lge.h>
#define CHECK_EVENT		0x200f4038

/* main resume reason */
#define NORMAL_GPIO			0
#define MAXIM_KNOCK_ON		1
#define AAA_AUDIO			2

static void __iomem *check_onkey;
struct wake_lock off_mode_lock;

static int powerkey_pressed;
struct d2260_onkey {
	struct platform_device *pdev;
	struct input_dev *input;
	struct d2260 *d2260;
};

static struct d2260_onkey *onkey_checker;

extern void d2260_set_bbat_chrg_current(void);

int d2260_onkey_check(void)
{
	return powerkey_pressed;
}
EXPORT_SYMBOL(d2260_onkey_check);

int d2260_onkey_display_on_event(void)
{
	input_event(onkey_checker->input, EV_KEY, KEY_POWER, 1);
	input_sync(onkey_checker->input);

	input_event(onkey_checker->input, EV_KEY, KEY_POWER, 0);
	input_sync(onkey_checker->input);

	pr_info("%s: display on event generated\n", __func__);
	return 0;
}
EXPORT_SYMBOL(d2260_onkey_display_on_event);

static irqreturn_t d2260_onkey_event_lo_handler(int irq, void *data)
{
	struct d2260_onkey *onkey = data;
	struct d2260 *d2260 = onkey->d2260;

	if(lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO){
		printk("[D2260] Power Key Pressed wake lock timout 500msec \n");
		wake_lock_timeout(&off_mode_lock,msecs_to_jiffies(500));
	}
	DIALOG_INFO(d2260->dev, "Onkey LO Interrupt Event generated\n");
	printk("[D2260] Power Key Pressed \n");
	input_event(onkey->input, EV_KEY, KEY_POWER, 1);
	input_sync(onkey->input);

	powerkey_pressed = 1;

	return IRQ_HANDLED;
}

static irqreturn_t d2260_onkey_event_hi_handler(int irq, void *data)
{
	struct d2260_onkey *onkey = data;
	struct d2260 *d2260 = onkey->d2260;
	if(lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO){
		printk("[D2260] Power Key Pressed wake lock timout 500msec \n");
		wake_lock_timeout(&off_mode_lock,msecs_to_jiffies(500));
	}

	DIALOG_INFO(d2260->dev, "Onkey HI Interrupt Event generated\n");
	printk("[D2260] Power Key Released \n");
	input_event(onkey->input, EV_KEY, KEY_POWER, 0);
	input_sync(onkey->input);

	powerkey_pressed = 0;

	return IRQ_HANDLED;
}

static int d2260_onkey_probe(struct platform_device *pdev)
{
	struct d2260 *d2260 = dev_get_drvdata(pdev->dev.parent);
	struct d2260_onkey *onkey;
	struct input_dev *input_dev;
	int ret = 0;

	if (!d2260) {
		dev_err(&pdev->dev, "Failed to get the driver's data\n");
		return -EINVAL;
	}
	DIALOG_DEBUG(d2260->dev, "%s(Starting ONKEY)\n",  __FUNCTION__);

#ifdef CONFIG_ODIN_TEE
	check_onkey= CHECK_EVENT;
#else
	check_onkey = ioremap(CHECK_EVENT, 0x4);
#endif

	onkey = kzalloc(sizeof(*onkey), GFP_KERNEL);
	onkey_checker = onkey;
	input_dev = input_allocate_device();
	if (!onkey || !input_dev) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_free_mem;
	}

	onkey->input = input_dev;
	onkey->d2260 = d2260;

	input_dev->name = "d2260-onkey";
	input_dev->phys = "d2260-onkey/input0";
	input_dev->dev.parent = &pdev->dev;

	input_set_capability(onkey->input, EV_KEY, KEY_POWER);

	ret = d2260_request_irq(d2260, D2260_IRQ_ENONKEY_HI,
			"ONKEY HI", d2260_onkey_event_hi_handler, onkey);
	if (ret < 0) {
		dev_err(d2260->dev,
				"Failed to register ONKEY HI IRQ: %d\n", ret);
		goto err_free_mem;
	}
	ret = d2260_request_irq(onkey->d2260, D2260_IRQ_ENONKEY_LO,
			"ONKEY LO", d2260_onkey_event_lo_handler, onkey);
	if (ret < 0) {
		dev_err(d2260->dev,
				"Failed to register ONKEY LO IRQ: %d\n", ret);
		goto err_free_irq;
	}

	ret = input_register_device(onkey->input);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register input device,error: %d\n", ret);
		input_free_device(onkey->input);
		return ret;
	}
	if(lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO){
		printk("[D2260] Power Key init for off mode wake lock timeout \n");
		wake_lock_init(&off_mode_lock, WAKE_LOCK_SUSPEND, "off_mode_lock");
	}

	platform_set_drvdata(pdev, onkey);
	DIALOG_DEBUG(d2260->dev, "Onkey Driver registered\n");
	return 0;

err_free_irq:
	d2260_free_irq(d2260, D2260_IRQ_ENONKEY_HI, onkey);
err_free_mem:
	input_free_device(input_dev);
        kfree(onkey);

	return ret;
}

static int d2260_onkey_remove(struct platform_device *pdev)
{
	struct d2260_onkey *onkey = platform_get_drvdata(pdev);
	struct d2260 *d2260 = onkey->d2260;

	d2260_free_irq(d2260, D2260_IRQ_ENONKEY_LO, onkey);
	d2260_free_irq(d2260, D2260_IRQ_ENONKEY_HI, onkey);
	input_unregister_device(onkey->input);
	kfree(onkey);

	return 0;
}

static int d2260_onkey_suspend(struct device *dev)
{
	/* Set Backup Battery Charging Current 100uA for Sleep Current*/
	d2260_set_bbat_chrg_current();
	return 0;
}

static int d2260_onkey_resume(struct device *dev)
{
	struct d2260_onkey *onkey =	dev_get_drvdata(dev);
	unsigned int event;
	unsigned int main_resume_reason;
	unsigned int sub_resume_reason;

#ifdef CONFIG_ODIN_TEE
	event = tz_read(check_onkey);
#else
	event = readw(check_onkey);
#endif
	main_resume_reason = (event & 0xFFFF);
	sub_resume_reason = ((event >> 16) & 0xFFFF);
	if (main_resume_reason == MAXIM_KNOCK_ON || main_resume_reason == AAA_AUDIO) {
		input_event(onkey->input, EV_KEY, KEY_POWER, 1);
		input_sync(onkey->input);

		input_event(onkey->input, EV_KEY, KEY_POWER, 0);
		input_sync(onkey->input);

		pr_info("%s: display on event generated\n", __func__);
	}
	if (event) {
		pr_info("[RESUME REASON] main: %d, sub: %d\n",
				main_resume_reason, sub_resume_reason);
	}
#ifdef CONFIG_ODIN_TEE
	tz_write(0, check_onkey);
#else
	writew(0, check_onkey);
#endif
	return 0;
}

static const struct dev_pm_ops d2260_onkey_ops = {
	.suspend = d2260_onkey_suspend,
	.resume	= d2260_onkey_resume,
};

static struct platform_driver d2260_onkey_driver = {
	.probe		= d2260_onkey_probe,
	.remove		= d2260_onkey_remove,
	.driver		= {
		.name	= "d2260-onkey",
		.owner	= THIS_MODULE,
		.pm		= &d2260_onkey_ops,
	}
};

static int __init d2260_onkey_init(void)
{
	return platform_driver_register(&d2260_onkey_driver);
}

static void __exit d2260_onkey_exit(void)
{
	platform_driver_unregister(&d2260_onkey_driver);
}

subsys_initcall(d2260_onkey_init);
module_exit(d2260_onkey_exit);

MODULE_AUTHOR("Tony Olech <anthony.olech@diasemi.com>");
MODULE_DESCRIPTION("Onkey driver for the Dialog D2260 PMIC");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:d2260-onkey");
