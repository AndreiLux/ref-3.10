/*
 * MAX77819 Vibrator driver
 *
 * Copyright (C) 2013 Maxim Integrated Product
 * Gyungoh Yoo <jack.yoo@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/mfd/max77819.h>
#include <linux/max77819-vibrator.h>
#include "../staging/android/timed_output.h"

/* HAPTIC REGISTERS */
#define MAX77819_CURR_SINK	0x4F

/* MAX77819_CURR_SINK */
#define MAX77819_CUR		0x1F
#define MAX77819_SINKONOFF	0x20

struct max77819_vib
{
	struct timed_output_dev timed_dev;
	struct regmap *regmap;
	struct hrtimer timer;
	int vib_current;
	int state;
	int level;
};

static inline int max77819_vib_set(struct max77819_vib *max77819_vib, bool on)
{
	int ret;

	// Sink ON/OFF
	ret = regmap_update_bits(max77819_vib->regmap, MAX77819_CURR_SINK,
			MAX77819_SINKONOFF, on ? MAX77819_SINKONOFF : 0);
	if (IS_ERR_VALUE(ret))
		dev_err(max77819_vib->timed_dev.dev,
				"cannot set MAX77819_SINKONOFF : %d\n", ret);

	return ret;
}

static void max77819_vib_enable(struct timed_output_dev *dev, int value)
{
	struct max77819_vib *max77819_vib = container_of(dev, struct max77819_vib, timed_dev);

	hrtimer_cancel(&max77819_vib->timer);

	if (value > 0)
	{
		max77819_vib_set(max77819_vib, true);

		hrtimer_start(&max77819_vib->timer,
				ktime_set(value / 1000, (value % 1000) * 1000000),
				HRTIMER_MODE_REL);
	}
}

static int max77819_vib_get_time(struct timed_output_dev *dev)
{
	struct max77819_vib *max77819_vib = container_of(dev, struct max77819_vib, timed_dev);

	if (hrtimer_active(&max77819_vib->timer))
	{
		ktime_t r = hrtimer_get_remaining(&max77819_vib->timer);
		return ktime_to_ms(r);
	}
	else
		return 0;
}

static enum hrtimer_restart max77819_vib_timer_func(struct hrtimer *timer)
{
	struct max77819_vib *max77819_vib = container_of(timer,
			struct max77819_vib, timer);
			
	max77819_vib_set(max77819_vib, false);
	
	return HRTIMER_NORESTART;
}

static int max77819_vib_hw_init(struct max77819_vib *max77819_vib, struct max77819_vib_platform_data *pdata)
{
	int vib_current = pdata->vib_current / 1000;
	unsigned int value;
	
	if (vib_current >= 1 && vib_current <=20)
		value = vib_current - 1;
	else if (vib_current >= 25 && vib_current <= 40)
		value = (vib_current - 25) / 5 + 20;
	else if (vib_current >= 40 && vib_current <= 100)
		value = (vib_current - 40) / 10 + 23;
	else if (vib_current >= 120 && vib_current <= 140)
		value = (vib_current - 120) / 20 + 30;
	else
		return -EINVAL;

	return regmap_update_bits(max77819_vib->regmap, MAX77819_CURR_SINK,
			MAX77819_CUR, value << M2SH(MAX77819_CUR));
}

#ifdef CONFIG_OF
static struct max77819_vib_platform_data *max77819_vib_parse_dt(struct device *dev)
{
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *np;
	struct max77819_vib_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	np = of_find_node_by_name(nproot, "vibrator");
	if (unlikely(np == NULL))
	{
		dev_err(dev, "vibrator node not found\n");
		return ERR_PTR(-EINVAL);
	}

	if (!of_property_read_u32(np, "current", &pdata->vib_current))
		pdata->vib_current = 1000;

	return pdata;
}
#endif

static int __devinit max77819_vib_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77819_2nd *max77819_2nd = dev_get_drvdata(dev->parent);
	struct max77819_vib_platform_data *pdata;
	struct max77819_vib *max77819_vib;
	int ret;

#ifdef CONFIG_OF
	pdata = max77819_vib_parse_dt(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev);
#endif

	max77819_vib = devm_kzalloc(dev, sizeof(struct max77819_vib), GFP_KERNEL);
	if (unlikely(!max77819_vib))
		return -ENOMEM;

	max77819_vib->regmap = max77819_2nd->regmap;

	max77819_vib_hw_init(max77819_vib, pdata);

	hrtimer_init(&max77819_vib->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	max77819_vib->timer.function = max77819_vib_timer_func;
			
	max77819_vib->timed_dev.name = "vibrator";
	max77819_vib->timed_dev.get_time = max77819_vib_get_time;
	max77819_vib->timed_dev.enable = max77819_vib_enable;
	ret = timed_output_dev_register(&max77819_vib->timed_dev);
	if (IS_ERR_VALUE(ret))
		return ret;

	platform_set_drvdata(pdev, max77819_vib);

#ifdef CONFIG_OF
	devm_kfree(dev, pdata);
#endif
	return 0;
}

static int __devexit max77819_vib_remove(struct platform_device *pdev)
{
	struct max77819_vib *max7819_vib = platform_get_drvdata(pdev);
	
	timed_output_dev_unregister(&max7819_vib->timed_dev);

	return 0;
}

#ifdef CONFIG_PM
static int max77819_vib_suspend(struct platform_device *dev, pm_message_t state)
{
        struct max77819_vib *max77819_vib = platform_get_drvdata(dev);

	hrtimer_cancel(&max77819_vib->timer);
	return max77819_vib_set(max77819_vib, false);
}
#endif

static struct platform_driver max77819_vib_driver = {
	.driver	= {
		.name = "max77819-vibrator",
		.owner = THIS_MODULE,
	},
	.probe = max77819_vib_probe,
	.remove = __devexit_p(max77819_vib_remove),
#ifdef CONFIG_PM
        .suspend = max77819_vib_suspend,
#endif
};

static int __init max77819_vib_init(void)
{
	return platform_driver_register(&max77819_vib_driver);
}
module_init(max77819_vib_init);

static void __exit max77819_vib_exit(void)
{
	platform_driver_unregister(&max77819_vib_driver);
}
module_exit(max77819_vib_exit);

MODULE_ALIAS("platform:max77819-vibrator");
MODULE_AUTHOR("Gyungoh Yoo <jack.yoo@maximintegrated.com>");
MODULE_DESCRIPTION("MAX77819 vibrator driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
