/*
 * Flash-led driver for Maxim MAX77819
 *
 * Copyright (C) 2013 Maxim Integrated Product
 * Gyungoh Yoo <jack.yoo@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/mfd/max77819.h>
#include <linux/leds-max77819-flash.h>

/* Registers */
#define MAX77819_IFLASH			0x00
#define MAX77819_ITORCH			0x02
#define MAX77819_TORCH_TMR		0x03
#define MAX77819_FLASH_TMR		0x04
#define MAX77819_FLASH_EN		0x05
#define MAX77819_MAX_FLASH1		0x06
#define MAX77819_MAX_FLASH2		0x07
#define MAX77819_MAX_FLASH3		0x08
#define MAX77819_VOUT_CNTL		0x0A
#define MAX77819_VOUT_FLASH		0x0B
#define MAX77819_FLASH_INT		0x0E
#define MAX77819_FLASH_INT_MASK		0x0F
#define MAX77819_FLASH_STATUS		0x10

/* MAX77819_IFLASH */
#define MAX77819_FLASH_I		0x3F

/* MAX77819_ITORCH */
#define MAX77819_TORCH_I		0x0F

/* MAX77819_TORCH_TMR */
#define MAX77819_TORCH_TMR_DUR		0x0F
#define MAX77819_DIS_TORCH_TMR		0x40
#define MAX77819_TORCH_TMR_MODE		0x80
#define MAX77819_TORCH_TMR_ONESHOT	0x00
#define MAX77819_TORCH_TMR_MAXTIMER	0x80

/* MAX77819_FLASH_TMR */
#define MAX77819_FLASH_TMR_DUR		0x0F
#define MAX77819_FLASH_TMR_MODE		0x80
#define MAX77819_FLASH_TMR_ONESHOT	0x00
#define MAX77819_FLASH_TMR_MAXTIMER	0x80

/* MAX77819_FLASH_EN */
#define MAX77819_TORCH_FLED_EN		0x0C
#define MAX77819_FLASH_FLED_EN		0xC0
#define MAX77819_OFF			0x00
#define MAX77819_BY_FLASHEN		0x01
#define MAX77819_BY_TORCHEN		0x02
#define MAX77819_BY_I2C			0X03

/* MAX77819_MAX_FLASH1 */
#define MAX77819_MAX_FLASH_HYS		0x03
#define MAX77819_MAX_FLASH_TH		0x7C
#define MAX77819_MAX_FLASH_TH_FROM_VOLTAGE(uV) \
		((((uV) - 2400000) / 33333) << M2SH(MAX77819_MAX_FLASH_TH))
#define MAX77819_MAX_FL_EN		0x80

/* MAX77819_MAX_FLASH2 */
#define MAX77819_LB_TMR_F		0x07
#define MAX77819_LB_TMR_R		0x38
#define MAX77819_LB_TME_FROM_TIME(uSec) ((uSec) / 256)

/* MAX77819_MAX_FLASH3 */
#define MAX77819_FLED_MIN_OUT		0x3F
#define MAX77819_FLED_MIN_MODE		0x80

/* MAX77819_VOUT_CNTL */
#define MAX77819_BOOST_FLASH_MDOE	0x07
#define MAX77819_BOOST_FLASH_MODE_OFF	0x00
#define MAX77819_BOOST_FLASH_MODE_FLED1	0x01
#define MAX77819_BOOST_FLASH_MODE_FIXED	0x04

/* MAX77819_VOUT_FLASH */
#define MAX77819_BOOST_VOUT_FLASH	0x7F
#define MAX77819_BOOST_VOUT_FLASH_FROM_VOLTAGE(uV)				\
		((uV) <= 3300000 ? 0x00 :					\
		((uV) <= 5500000 ? (((mV) - 3300000) / 25000 + 0x0C) : 0x7F))

/* MAX77819_FLASH_INT_MASK */
#define MAX77819_FLED_OPEN_M		0x04
#define MAX77819_FLED_SHORT_M		0x08
#define MAX77819_MAX_FLASH_M		0x10
#define MAX77819_FLED_FAIL_M		0x20

/* MAX77819_FLASH_STATAUS */
#define MAX77819_TORCH_ON_STAT		0x04
#define MAX77819_FLASH_ON_STAT		0x08

#define MAX_FLASH_CURRENT	1000	// 1000mA(0x1f)
#define MAX_TORCH_CURRENT	250	// 250mA(0x0f)
#define MAX_FLASH_DRV_LEVEL	63	/* 15.625 + 15.625*63 mA */
#define MAX_TORCH_DRV_LEVEL	15	/* 15.625 + 15.625*15 mA */

struct max77819_flash
{
	struct led_classdev led_flash;
	struct led_classdev led_torch;
	struct max77819 *max77819;
	struct max77819_flash_data *flash_data;
	bool flash_en_control;
	bool torch_en_control;
};

static void max77819_flash_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct max77819_flash *max77819_flash = container_of(led_cdev, struct max77819_flash, led_flash);
	struct regmap *regmap = max77819_flash->max77819->regmap;
	struct device *dev = max77819_flash->led_flash.dev;
	int ret;

	if (brightness == LED_OFF)
	{
		/* Flash OFF */
		ret = regmap_update_bits(regmap, MAX77819_FLASH_EN,
				MAX77819_FLASH_FLED_EN, 0);
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "can't write FLASH_EN : %d\n", ret);
	}
	else
	{
		/* Set current */
		ret = regmap_update_bits(regmap, MAX77819_IFLASH,
				MAX77819_FLASH_I,
				(unsigned int)brightness << M2SH(MAX77819_FLASH_I));
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "can't write IFLASH : %d\n", ret);

		/* Flash ON */
		ret = regmap_update_bits(regmap, MAX77819_FLASH_EN,
				MAX77819_FLASH_FLED_EN,
				max77819_flash->flash_en_control ?
					(MAX77819_BY_FLASHEN << M2SH(MAX77819_FLASH_FLED_EN)) :
					(MAX77819_BY_I2C << M2SH(MAX77819_FLASH_FLED_EN)));
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "can't write FLASH_EN : %d\n", ret);
	}
}

static enum led_brightness max77819_flash_get(struct led_classdev *led_cdev)
{
	struct max77819_flash *max77819_flash = container_of(led_cdev, struct max77819_flash, led_flash);
	struct regmap *regmap = max77819_flash->max77819->regmap;
	struct device *dev = max77819_flash->led_flash.dev;
	unsigned int value;
	int ret;

	/* Get status */
	ret = regmap_read(regmap, MAX77819_FLASH_EN, &value);
	if (IS_ERR_VALUE(ret))
		dev_err(dev, "can't read FLASH_EN : %d\n", ret);

	if ((value & MAX77819_FLASH_FLED_EN) == (MAX77819_OFF << M2SH(MAX77819_FLASH_FLED_EN)))
		return LED_OFF;

	/* Get current */
	ret = regmap_read(regmap, MAX77819_IFLASH, &value);
	if (IS_ERR_VALUE(ret))
		dev_err(dev, "can't read IFLASH : %d\n", ret);

	return (enum led_brightness)((value & MAX77819_FLASH_I) >> M2SH(MAX77819_FLASH_I));
}

static void max77819_torch_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct max77819_flash *max77819_flash = container_of(led_cdev, struct max77819_flash, led_torch);
	struct regmap *regmap = max77819_flash->max77819->regmap;
	struct device *dev = max77819_flash->led_torch.dev;
	int ret;

	if (brightness == LED_OFF)
	{
		/* Flash OFF */
		ret = regmap_update_bits(regmap, MAX77819_FLASH_EN,
				MAX77819_TORCH_FLED_EN, 0);
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "can't write FLASH_EN : %d\n", ret);
	}
	else
	{
		/* Set current */
		ret = regmap_update_bits(regmap, MAX77819_ITORCH,
				MAX77819_TORCH_I,
				(unsigned int)brightness << M2SH(MAX77819_TORCH_I));
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "can't write IFLASH : %d\n", ret);

		/* Flash ON */
		ret = regmap_update_bits(regmap, MAX77819_FLASH_EN,
				MAX77819_TORCH_FLED_EN,
				max77819_flash->flash_en_control ?
					(MAX77819_BY_TORCHEN << M2SH(MAX77819_TORCH_FLED_EN)) :
					(MAX77819_BY_I2C << M2SH(MAX77819_TORCH_FLED_EN)));
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "can't write FLASH_EN : %d\n", ret);
	}
}

static enum led_brightness max77819_torch_get(struct led_classdev *led_cdev)
{
	struct max77819_flash *max77819_flash = container_of(led_cdev, struct max77819_flash, led_torch);
	struct regmap *regmap = max77819_flash->max77819->regmap;
	struct device *dev = max77819_flash->led_torch.dev;
	unsigned int value;
	int ret;

	/* Get status */
	ret = regmap_read(regmap, MAX77819_FLASH_EN, &value);
	if (IS_ERR_VALUE(ret))
		dev_err(dev, "can't read FLASH_EN : %d\n", ret);

	if ((value & MAX77819_TORCH_FLED_EN) == (MAX77819_OFF << M2SH(MAX77819_TORCH_FLED_EN)))
		return LED_OFF;

	/* Get current */
	ret = regmap_read(regmap, MAX77819_ITORCH, &value);
	if (IS_ERR_VALUE(ret))
		dev_err(dev, "can't read ITORCH : %d\n", ret);

	return (enum led_brightness)((value & MAX77819_TORCH_I) >> M2SH(MAX77819_TORCH_I));
}

static int max77819_flash_hw_setup(struct max77819 *max77819, struct max77819_flash_platform_data *pdata)
{
	struct regmap *regmap = max77819->regmap;
	unsigned int value;
	int ret = 0;

	/* Torch Safty Timer Disabled, run for MAX timer */
	ret = regmap_write(regmap, MAX77819_TORCH_TMR,
			MAX77819_TORCH_TMR_DUR | MAX77819_DIS_TORCH_TMR | MAX77819_TORCH_TMR_MODE);
	if (IS_ERR_VALUE(ret))
		return ret;

	/* Flash Safty Timer = 1000ms, run for MAX timer */
	ret = regmap_write(regmap, MAX77819_TORCH_TMR,
			MAX77819_FLASH_TMR_DUR | MAX77819_FLASH_TMR_MODE);
	if (IS_ERR_VALUE(ret))
		return ret;

	/* Max Flash setting */
	switch(pdata->maxflash_hysteresis)
	{
	case 100000:
		value = 0 << MAX77819_MAX_FLASH_HYS;
		break;
	case 200000:
		value = 1 << MAX77819_MAX_FLASH_HYS;
		break;
	case 300000:
		value = 2 << MAX77819_MAX_FLASH_HYS;
		break;
	case -1:
		value = 3 << MAX77819_MAX_FLASH_HYS;
		break;
	default:
		return -EINVAL;
	}

	value |= MAX77819_MAX_FLASH_TH_FROM_VOLTAGE(pdata->maxflash_threshold);

	if (pdata->enable_maxflash)
		value |= MAX77819_MAX_FL_EN;

	ret = regmap_write(regmap, MAX77819_MAX_FLASH1, value);
	if (IS_ERR_VALUE(ret))
		return ret;

	/* Low battery mask timer for Max Flash */
	value = MAX77819_LB_TME_FROM_TIME(pdata->maxflash_falling_timer)
			<< M2SH(MAX77819_LB_TMR_F);
	value |= MAX77819_LB_TME_FROM_TIME(pdata->maxflash_rising_timer)
			<< M2SH(MAX77819_LB_TMR_R);
	ret = regmap_write(regmap, MAX77819_MAX_FLASH1, value);
	if (IS_ERR_VALUE(ret))
		return ret;

	return ret;
}

#ifdef CONFIG_OF
static struct max77819_flash_platform_data *max77819_flash_parse_dt(struct device *dev)
{
	struct max77819_flash_platform_data *pdata;
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *np;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	np = of_find_node_by_name(nproot, "flash");
	if (unlikely(np == NULL))
	{
		dev_err(dev, "flash node not found\n");
		return ERR_PTR(-EINVAL);
	}

	pdata->flash_en_control = of_property_read_bool(np, "flash-en-control");
	pdata->torch_en_control = of_property_read_bool(np, "torch-en-control");
	pdata->enable_maxflash = of_property_read_bool(np, "enable-maxflash");

	if (!of_property_read_u32(np, "maxflash-threshold", &pdata->maxflash_threshold))
		pdata->maxflash_threshold = 2400000;

	if (!of_property_read_u32(np, "maxflash-hysteresis", &pdata->maxflash_hysteresis))
		pdata->maxflash_hysteresis = 100000;

	if (!of_property_read_u32(np, "maxflash-falling-timer", &pdata->maxflash_falling_timer))
		pdata->maxflash_falling_timer = 256;

	if (!of_property_read_u32(np, "maxflash-rising-timer", &pdata->maxflash_rising_timer))
		pdata->maxflash_rising_timer = 256;

	return pdata;
}
#endif

static int max77819_flash_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77819_flash_platform_data *pdata;
	struct max77819_flash *max77819_flash;
	struct max77819 *max77819 = dev_get_drvdata(dev->parent);
	int ret;

#ifdef CONFIG_OF
	pdata = max77819_flash_parse_dt(dev);
	if (unlikely(IS_ERR(pdata)))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev);
#endif

	max77819_flash = devm_kzalloc(dev, sizeof(struct max77819_flash), GFP_KERNEL);
	if (unlikely(!max77819_flash))
		return -ENOMEM;

	// initial setup
	ret = max77819_flash_hw_setup(max77819, pdata);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(&pdev->dev, "hw init fail : %d\n", ret);
		return ret;
	}

	max77819_flash->max77819 = max77819;
	max77819_flash->led_flash.name = "max77819:white:flash";
	max77819_flash->led_flash.default_trigger = "flash";
	max77819_flash->led_flash.brightness_set = max77819_flash_set;
	max77819_flash->led_flash.brightness_get = max77819_flash_get;
	max77819_flash->led_flash.max_brightness = MAX77819_FLASH_I >> M2SH(MAX77819_FLASH_I);
	max77819_flash->led_torch.name = "max77819:white:torch";
	max77819_flash->led_torch.default_trigger = "torch";
	max77819_flash->led_torch.brightness_set = max77819_torch_set;
	max77819_flash->led_torch.brightness_get = max77819_torch_get;
	max77819_flash->led_torch.max_brightness = MAX77819_TORCH_I >> M2SH(MAX77819_TORCH_I);

	ret = led_classdev_register(dev, &max77819_flash->led_flash);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "unable to register FLASH : %d\n", ret);
		return ret;
	}

	ret = led_classdev_register(dev, &max77819_flash->led_torch);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "unable to register TORCH : %d\n", ret);
		goto out_unregister_flash;
	}

	dev_set_drvdata(max77819_flash->led_flash.dev, max77819_flash);
	dev_set_drvdata(max77819_flash->led_torch.dev, max77819_flash);
	platform_set_drvdata(pdev, max77819_flash);

#ifdef CONFIG_OF
	devm_kfree(dev, pdata);
#endif
	return 0;

out_unregister_flash:
	led_classdev_unregister(&max77819_flash->led_flash);
	return ret;
}

static int __exit max77819_flash_remove(struct platform_device *pdev)
{
	struct max77819_flash *max77819_flash = platform_get_drvdata(pdev);

	led_classdev_unregister(&max77819_flash->led_flash);
	led_classdev_unregister(&max77819_flash->led_torch);

	return 0;
}

static struct platform_driver max77819_fled_driver =
{
	.driver		=
	{
		.name	= "max77819-flash",
		.owner	= THIS_MODULE,
	},
	.probe		= max77819_flash_probe,
	.remove		= max77819_flash_remove,
};

static int __init max77819_flash_init(void)
{
	return platform_driver_register(&max77819_fled_driver);
}
subsys_initcall(max77819_flash_init);
//module_init(max77819_flash_init);

static void __exit max77819_flash_exit(void)
{
	platform_driver_unregister(&max77819_fled_driver);
}
module_exit(max77819_flash_exit);

MODULE_ALIAS("platform:max77819-flash");
MODULE_AUTHOR("Gyungoh Yoo<jack.yoo@maximintegrated.com>");
MODULE_DESCRIPTION("MAX77819 FLED driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
