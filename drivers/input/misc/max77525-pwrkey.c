/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/mfd/max77525/max77525.h>
#include <linux/mfd/max77525/max77525_reg.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include <linux/wakelock.h>
#include <linux/platform_data/odin_tz.h>
#include <asm/io.h>


#define SW_INPUT		(1 << 7)	/* 0/1 -- up/down */
#define HARDRESET_EN		(1 << 7)
#define PWREN_EN		(1 << 7)

#define REG_PWRONBCNFG	0x07
#define BIT_TMRST		0x1E

#define REG_WRSTCNFG	0x0A
#define BIT_SHDNRST		BIT(7)
#define BIT_WRSTEN		BIT(6)
#define BIT_WRSTDLT		(BIT(2) | BIT(1) | BIT(0)

#define WARM_RESET_EN	BIT_WRSTEN
#define WARM_RESET_DIS	0
#define WARM_RESET_VAL	BIT_SHDNRST
#define SHUTDOWN_VAL	0

#define REG_TOPSYSINT	0x0E
#define REG_TOPSYSINTM	0x0F
#define BIT_WRST		BIT(5)
#define BIT_PWRONBON	BIT(4)
#define BIT_PWRONBOFF	BIT(3)
#define BIT_AUXONB		BIT(2)
#define BIT_120C		BIT(1)
#define BIT_140C		BIT(0)

#define REG_TOPSYSSTTS	0x10
#define BIT_PWRONBSTTS	BIT(3)
#define BIT_AUXONBSTTS	BIT(2)
#define BIT_120CSTTS	BIT(1)
#define BIT_140CSTTS	BIT(0)

struct max77525_pwrkey_info {
	struct regmap		*regmap;
	struct input_dev	*idev;
	struct device		*dev;
	unsigned int		irq;
	int 				tmrst;	/* Manual Reset Time(TMRST) */
};

static struct max77525_pwrkey_info *sys_reset_dev;

static struct wakeup_source aaa_wakeup_lock;
static void __iomem *check_power_key;
struct max77525_pwrkey_info *onkey_checker;
/**
 * max77525_pon_system_pwr_off 
 * -> Configure system-reset PMIC for shutdown or reset
 * @reset: Configures for shutdown if 0, or reset if 1.
 *
 * This function will only configure a single PMIC. The other PMICs in the
 * system are slaved off of it and require no explicit configuration. Once
 * the system-reset PMIC is configured properly, the MSM can drop PS_HOLD to
 * activate the specified configuration.
 */
int max77525_pon_system_pwr_off(bool reset)
{
	struct max77525_pwrkey_info *pwrkey_info = sys_reset_dev;
	int rc;

	if (!pwrkey_info)
		return -ENODEV;

	rc = regmap_update_bits(pwrkey_info->regmap,
			REG_WRSTCNFG, BIT_WRSTEN | BIT_SHDNRST,
			reset ? (WARM_RESET_EN | WARM_RESET_VAL) :
			(WARM_RESET_DIS | SHUTDOWN_VAL));
	if (rc)
		dev_err(pwrkey_info->dev,
			"Unable to write to addr=%x, rc(%d)\n",
			REG_WRSTCNFG, rc);

	return rc;
}
EXPORT_SYMBOL(max77525_pon_system_pwr_off);

int max77525_onkey_display_on_event(void)
{
	input_report_key(onkey_checker->idev, KEY_SLEEP, 1);
	input_report_key(onkey_checker->idev, KEY_SLEEP, 0);
	input_sync(onkey_checker->idev);

	pr_info("%s: display on event generated\n", __func__);
	return 0;
}
EXPORT_SYMBOL(max77525_onkey_display_on_event);


/* pwrkey interrupt is in TOPSYSINT */
static irqreturn_t max77525_topsys_handler(int irq, void *data)
{
	struct max77525_pwrkey_info *max77525_pwrkey = data;
	int rc;
	unsigned int topsys_irq, state, code;

	rc = regmap_read(max77525_pwrkey->regmap, REG_TOPSYSINT, &topsys_irq);
	if (rc) {
		dev_err(max77525_pwrkey->dev, "REGMAP-IPC read failed, rc=%d\n", rc);
		return IRQ_NONE;
	}

	switch (topsys_irq) {
		case BIT_WRST:
			break;
		case BIT_PWRONBON:
			rc = regmap_read(max77525_pwrkey->regmap, REG_TOPSYSSTTS, &state);
			if (rc) {
				dev_err(max77525_pwrkey->dev, "REGMAP-IPC read failed, rc=%d\n", rc);
				return IRQ_NONE;
			}
			if (state & BIT_PWRONBSTTS) {
				dev_notice(max77525_pwrkey->dev, "KEY_POWER pressed.\n");
				code = KEY_POWER;
			}
			else {
				code = KEY_SLEEP;
				dev_notice(max77525_pwrkey->dev, "KEY_SLEEP pressed.\n");
			}
			input_report_key(max77525_pwrkey->idev, code, 1);
			input_report_key(max77525_pwrkey->idev, code, 0);
			input_sync(max77525_pwrkey->idev);
			break;
		case BIT_PWRONBOFF:
			/* power off sequence */
			break;
		case BIT_AUXONB:
			break;
		case BIT_120C:
			break;
		case BIT_140C:
			break;
		default:
			break;
	}
	return IRQ_HANDLED;
}

static int max77525_power_key_probe(struct platform_device *pdev)
{
	struct max77525 *max77525 = dev_get_drvdata(pdev->dev.parent);
	struct max77525_pwrkey_info *pwrkey_info;
	struct device_node *pmic_node = pdev->dev.parent->of_node;
	struct device_node *node;
	struct device_node *voice_detect;
	struct input_dev *input;
	unsigned int value;
	int rc, tmrst;

#if 0 /* Device Tree will be updated */
	voice_detect = of_find_compatible_node(NULL, NULL, "LG,3a-active");
	check_power_key = of_iomap(voice_detect, 0);
	if (check_power_key == NULL)
		return -ENOMEM;
#endif
#ifdef CONFIG_ODIN_TEE
	check_power_key = 0x200f4038;
#else
	check_power_key = ioremap(0x200f4038, 0x8);
#endif

	pwrkey_info = devm_kzalloc(&pdev->dev,
		sizeof(struct max77525_pwrkey_info), GFP_KERNEL);
	if (pwrkey_info == NULL) {
		dev_err(&pdev->dev, "Unable to allocate memory!\n");
		return -ENOMEM;
	}
	onkey_checker = pwrkey_info;

	sys_reset_dev = pwrkey_info;

	platform_set_drvdata(pdev, pwrkey_info);

	input = input_allocate_device();
	if (!input) {
		dev_err(&pdev->dev, "Unable to allocate memory!\n");
		rc = -ENOMEM;
		goto err_free_mem;
	}

	pwrkey_info->regmap = max77525->regmap;
	pwrkey_info->idev = input;
	pwrkey_info->dev = &pdev->dev;

	input->name = "max77525_pon";
	input->phys = "max77525_pon/input0";
	input->dev.parent = &pdev->dev;
	input_set_capability(input, EV_KEY, KEY_POWER);
	input_set_capability(input, EV_KEY, KEY_SLEEP);

	node = of_find_node_by_name(pmic_node, "power-key");
	if (!node) {
		dev_err(&pdev->dev, "could not find power-key sub-node\n");
		rc = -ENODEV;
		goto err_free_mem;
	}

#if 0
	rc = of_property_read_u32(node,
				"maxim,manual-reset-timer", &tmrst);
	if (IS_ERR_VALUE(rc))
		goto err_free_mem;

	/* set the manual reset time */
	rc = regmap_update_bits(pwrkey_info->regmap, REG_PWRONBCNFG, BIT_TMRST,
							tmrst <= 3 ? 0 :
							tmrst > 12 ? 12 : tmrst-3);
	if (IS_ERR_VALUE(rc))
		goto err_free_mem;
#endif
	pwrkey_info->irq = regmap_irq_get_virq(max77525->irq_data,
                                             MAX77525_IRQ_TOPSYSINT);
	if ((int)pwrkey_info->irq < 0) {
		rc = pwrkey_info->irq;
		goto err_free_mem;
	}

	rc = devm_request_threaded_irq(&pdev->dev, pwrkey_info->irq,
			NULL, max77525_topsys_handler,
			IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQF_SHARED,
			"max77525_topsys_int", pwrkey_info);
	if (IS_ERR_VALUE(rc))
		goto err_free_mem;

	rc = input_register_device(pwrkey_info->idev);
	if (rc) {
		dev_err(pwrkey_info->dev, "Can't register input device: %d\n", rc);
		goto err_free_irq;
	}

	device_init_wakeup(&pdev->dev, 1);

	wakeup_source_init(&aaa_wakeup_lock, "3a_wakeup_power");
	return 0;

err_free_irq:
	devm_free_irq(&pdev->dev, pwrkey_info->irq, pwrkey_info);
err_free_mem:
	input_free_device(input);
	kfree(pwrkey_info);

	return rc;
}

static int max77525_power_key_remove(struct platform_device *pdev)
{
	struct max77525_pwrkey_info *pwrkey_info = platform_get_drvdata(pdev);

	device_init_wakeup(&pdev->dev, 0);
	input_unregister_device(pwrkey_info->idev);
	free_irq(pwrkey_info->irq, pwrkey_info);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int max77525_powerkey_suspend(struct device *dev)
{
	return 0;
}

static int max77525_powerkey_resume(struct device *dev)
{
	struct max77525_pwrkey_info *max77525_3a = dev_get_drvdata(dev);
	u8 keyint;

	/* 3A Audio Check */
#ifdef CONFIG_ODIN_TEE
	keyint = tz_read(check_power_key + 0x0);
#else
	keyint = readw(check_power_key + 0x0);
#endif
	if (keyint == 0x1) {
		printk("3A Activating \n");
		input_report_key(max77525_3a->idev, KEY_SLEEP, 1);
		input_report_key(max77525_3a->idev, KEY_SLEEP, 0);
		input_sync(max77525_3a->idev);
		/* Clear keyint read bit */
#ifdef CONFIG_ODIN_TEE
		tz_write(0, check_power_key);
#else
		writew(0, check_power_key);
#endif
		__pm_wakeup_event(&aaa_wakeup_lock, 500);
	}
	return 0;
}

static const struct dev_pm_ops max77525_power_key_ops = {
	.suspend = max77525_powerkey_suspend,
	.resume  = max77525_powerkey_resume,
};

#ifdef CONFIG_OF
static struct of_device_id max77525_power_key_match_table[] = {
	{	.compatible = "maxim,max77525-power-key", },
	{ }
};
#endif
static struct platform_driver max77525_power_key_driver = {
	.driver		= {
		.name	= "max77525-power-key",
		.owner	= THIS_MODULE,
		.pm		= &max77525_power_key_ops,
		.of_match_table = max77525_power_key_match_table,
	},
	.probe		= max77525_power_key_probe,
	.remove		= max77525_power_key_remove,
};

static int __init max77525_power_key_init(void)
{
	return platform_driver_register(&max77525_power_key_driver);
}
subsys_initcall(max77525_power_key_init);

static void __exit max77525_power_key_exit(void)
{
	platform_driver_unregister(&max77525_power_key_driver);
}
module_exit(max77525_power_key_exit);

MODULE_DESCRIPTION("Maxim MAX77525 POWER KEY driver");
MODULE_AUTHOR("Clark Kim <clark.kim@maximintegrated.com>");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:max77525-power-key");
