/*
 * TI LM3631 Backlight Driver
 *
 * Copyright 2013 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/mfd/lm3631.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/leds.h>

#define LM3631_MAX_BRIGHTNESS	1511
#define LM3631_MIN_BRIGHTNESS   0x00

#define DEFAULT_BL_NAME			"lcd-backlight"

struct lm3631_bl {
	struct device *dev;
	struct backlight_device *bl_dev;

	struct lm3631 *lm3631;
	struct lm3631_backlight_platform_data *pdata;

	struct led_classdev *led_dev;
};

static int lm3631_bl_set_ovp(struct lm3631_bl *lm3631_bl)
{
	/* Set OVP to 25V by default */
	return lm3631_update_bits(lm3631_bl->lm3631, LM3631_REG_BL_BOOST,
				  LM3631_BOOST_OVP_MASK, LM3631_BOOST_OVP_25V);
}

static int lm3631_bl_set_ctrl_mode(struct lm3631_bl *lm3631_bl)
{
	struct lm3631_backlight_platform_data *pdata = lm3631_bl->pdata;

	/* Brightness control mode is I2C only by default */
	if (!pdata) {
		return lm3631_update_bits(lm3631_bl->lm3631,
					  LM3631_REG_BRT_MODE, LM3631_BRT_MASK,
					  LM3631_I2C_ONLY);
	}

	return lm3631_update_bits(lm3631_bl->lm3631, LM3631_REG_BRT_MODE,
				  LM3631_BRT_MASK, pdata->mode);
}

static int lm3631_bl_string_configure(struct lm3631_bl *lm3631_bl)
{
	u8 val;

	if (lm3631_bl->pdata->is_full_strings)
		val = LM3631_BL_TWO_STRINGS;
	else
		val = LM3631_BL_ONE_STRING;

	return lm3631_update_bits(lm3631_bl->lm3631, LM3631_REG_BL_CFG,
				  LM3631_BL_STRING_MASK, val);
}

static int lm3631_bl_configure(struct lm3631_bl *lm3631_bl)
{
	int ret;

	ret = lm3631_bl_set_ovp(lm3631_bl);
	if (ret) {
		pr_err("%s: failed to bl_set_ovp.\n", __func__);
		return ret;
	}

	ret = lm3631_bl_set_ctrl_mode(lm3631_bl);
	if (ret) {
		pr_err("%s: failed to bl_set_ctrl_mode.\n", __func__);
		return ret;
	}

	return lm3631_bl_string_configure(lm3631_bl);
}

static int lm3631_bl_enable(struct lm3631_bl *lm3631_bl, int enable)
{
	int ret;

	if (enable) {
		ret = lm3631_bl_configure(lm3631_bl);
		if (ret) {
			dev_err(lm3631_bl->dev, "backlight config err: %d\n", ret);
			return ret;
		}
	}
/*	pr_info("%s : register address : %x\n value : %x\n",
			__func__, LM3631_REG_DEVCTRL, enable << LM3631_BL_EN_SHIFT); */
	ret = lm3631_update_bits(lm3631_bl->lm3631, LM3631_REG_DEVCTRL,
				  LM3631_BL_EN_MASK,
				  enable << LM3631_BL_EN_SHIFT);
	return ret;
}

static inline int lm3631_bl_set_brightness(struct lm3631_bl *lm3631_bl, int val)
{
	u8 data;
	int ret;
	int offset_brightness;

	lm3631_bl->led_dev->brightness = val;

	offset_brightness = (val << 3) + 7;

	if (offset_brightness > 2047)
		offset_brightness = 2047;

	if (offset_brightness < 83)
		offset_brightness = 83;

        pr_info("%s: val:%d, offset_brightness:%d \n",__func__, val, offset_brightness);
	/* Only I2C brightness control supported */
	data = offset_brightness & LM3631_BRT_LSB_MASK;
	ret = lm3631_update_bits(lm3631_bl->lm3631, LM3631_REG_BRT_LSB,
				 LM3631_BRT_LSB_MASK, data);
	if (ret) {
		pr_err("%s : failed to set_brightness.\n", __func__);
		return ret;
	}

	data = (offset_brightness >> LM3631_BRT_MSB_SHIFT) & 0xFF;
	return lm3631_write_byte(lm3631_bl->lm3631, LM3631_REG_BRT_MSB,
				 data);
}

static int lm3631_bl_update_status(struct backlight_device *bl_dev)
{
	struct lm3631_bl *lm3631_bl = bl_get_data(bl_dev);
	int brt;
	int ret;

	if (bl_dev->props.state & BL_CORE_SUSPENDED)
		bl_dev->props.brightness = 0;

	brt = bl_dev->props.brightness;

	if (brt > 0)
		ret = lm3631_bl_enable(lm3631_bl, 1);
	else
		ret = lm3631_bl_enable(lm3631_bl, 0);

	if (ret) {
		pr_err("%s : failed to bl_enable.\n", __func__);
		return ret;
	}

	return lm3631_bl_set_brightness(lm3631_bl, brt);
}

static int lm3631_bl_get_brightness(struct backlight_device *bl_dev)
{
	return bl_dev->props.brightness;
}

static const struct backlight_ops lm3631_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3631_bl_update_status,
	.get_brightness = lm3631_bl_get_brightness,
};

static void lm3631_bl_update_status_led(struct led_classdev *led_cdev,
						enum led_brightness value)
{
	struct lm3631_bl *lm3631_bl = dev_get_drvdata(led_cdev->dev->parent);
	if (value > LM3631_MAX_BRIGHTNESS)
		value = LM3631_MAX_BRIGHTNESS;

	lm3631_bl->bl_dev->props.brightness = value;

	lm3631_bl_update_status(lm3631_bl->bl_dev);
}

static struct led_classdev lm3631_bl_led = {
	.name			= DEFAULT_BL_NAME,
	.brightness		= 0,
	.max_brightness	= LM3631_MAX_BRIGHTNESS,
	.brightness_set	= lm3631_bl_update_status_led,
};

static int lm3631_bl_register(struct lm3631_bl *lm3631_bl)
{
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	struct lm3631_backlight_platform_data *pdata = lm3631_bl->pdata;
	char name[20];
	int ret = 0;

	props.type = BACKLIGHT_PLATFORM;
	props.brightness = pdata ? pdata->init_brightness : 0;
	props.max_brightness = LM3631_MAX_BRIGHTNESS;

	if (!pdata || !pdata->name)
		snprintf(name, sizeof(name), "%s", DEFAULT_BL_NAME);
	else
		snprintf(name, sizeof(name), "%s", pdata->name);

	bl_dev = backlight_device_register(name, lm3631_bl->dev, lm3631_bl,
					   &lm3631_bl_ops, &props);
	if (IS_ERR(bl_dev))
		return PTR_ERR(bl_dev);

	lm3631_bl->bl_dev = bl_dev;

	lm3631_bl_led.name = name;
	lm3631_bl_led.brightness = props.brightness;

	ret = led_classdev_register(lm3631_bl->dev, &lm3631_bl_led);
	if (ret) {
		 pr_err("led_classdev_register failed\n");
		 return ret;
	}

	lm3631_bl->led_dev = &lm3631_bl_led;

	return 0;
}

static void lm3631_bl_unregister(struct lm3631_bl *lm3631_bl)
{
	if (lm3631_bl->bl_dev)
		backlight_device_unregister(lm3631_bl->bl_dev);
	if (lm3631_bl->led_dev)
		led_classdev_unregister(&lm3631_bl_led);
}

static int lm3631_bl_parse_dt(struct device *dev, struct lm3631_bl *lm3631_bl)
{
	struct device_node *node;
	struct lm3631_backlight_platform_data *pdata;
	unsigned init_brightness;
	int dt_found = 0;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	for_each_child_of_node(dev->of_node, node) {
		if (of_device_is_compatible(node, "ti,lm3631-backlight")) {
			dev->of_node = node;
			dt_found = 1;
			break;
		}
	}

	if (!dt_found) {
		dev_err(dev, "Backlight device tree node is not found\n");
		return -EINVAL;
	}

	of_property_read_string(node, "bl-name", &pdata->name);

	if (of_find_property(node, "full-strings-used", NULL))
		pdata->is_full_strings = true;

	if (of_find_property(node, "mode-pwm-only", NULL))
		pdata->mode = LM3631_PWM_ONLY;
	else if (of_find_property(node, "mode-comb1", NULL))
		pdata->mode = LM3631_COMB1;
	else if (of_find_property(node, "mode-comb2", NULL))
		pdata->mode = LM3631_COMB2;

	of_property_read_u32(node, "initial-brightness", &init_brightness);
	pdata->init_brightness = (u8)init_brightness;

	lm3631_bl->pdata = pdata;

	return 0;
}

static int lm3631_bl_probe(struct platform_device *pdev)
{
	struct lm3631 *lm3631 = dev_get_drvdata(pdev->dev.parent);
	struct lm3631_backlight_platform_data *pdata = lm3631->pdata->bl_pdata;
	struct lm3631_bl *lm3631_bl;
	int ret;

	pr_info("%s: start.\n", __func__);

	lm3631_bl = devm_kzalloc(&pdev->dev, sizeof(*lm3631_bl), GFP_KERNEL);
	if (!lm3631_bl)
		return -ENOMEM;

	lm3631_bl->pdata = pdata;
	if (!lm3631_bl->pdata) {
		if (IS_ENABLED(CONFIG_OF))
			ret = lm3631_bl_parse_dt(lm3631->dev, lm3631_bl);
		else
			return -ENODEV;

		if (ret)
			return ret;
	}

	lm3631_bl->dev = &pdev->dev;
	lm3631_bl->lm3631 = lm3631;
	platform_set_drvdata(pdev, lm3631_bl);

	ret = lm3631_bl_configure(lm3631_bl);
	if (ret) {
		dev_err(&pdev->dev, "backlight config err: %d\n", ret);
		return ret;
	}

	ret = lm3631_bl_register(lm3631_bl);
	if (ret) {
		dev_err(&pdev->dev, "register backlight err: %d\n", ret);
		return ret;
	}

	backlight_update_status(lm3631_bl->bl_dev);

	pr_info("%s: end.\n", __func__);

	return 0;
}

static int lm3631_bl_remove(struct platform_device *pdev)
{
	struct lm3631_bl *lm3631_bl = platform_get_drvdata(pdev);
	struct backlight_device *bl_dev = lm3631_bl->bl_dev;

	bl_dev->props.brightness = 0;
	backlight_update_status(bl_dev);
	lm3631_bl_unregister(lm3631_bl);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id lm3631_bl_of_match[] = {
	{ .compatible = "ti,lm3631-backlight", },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3631_bl_of_match);
#endif

static struct platform_driver lm3631_bl_driver = {
	.probe = lm3631_bl_probe,
	.remove = lm3631_bl_remove,
	.driver = {
		.name = "lm3631-backlight",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lm3631_bl_of_match),
	},
};
module_platform_driver(lm3631_bl_driver);

MODULE_DESCRIPTION("TI LM3631 Backlight Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lm3631-backlight");
