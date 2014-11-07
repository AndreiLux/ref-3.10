/*
 * TI LM3631 MFD Driver
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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/lm3631.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

#define LM3631_DEV_LCD_BIAS(_id)		\
{						\
	.name = "lm3631-regulator",		\
	.id   = _id,				\
	.of_compatible = "ti,lm3631-regulator",	\
}

#define LM3631_DEV_BL				\
{						\
	.name = "lm3631-backlight",		\
	.of_compatible = "ti,lm3631-backlight",	\
}

static struct mfd_cell lm3631_devs[] = {
	/* 5 Regulators */
	LM3631_DEV_LCD_BIAS(1),
	LM3631_DEV_LCD_BIAS(4),
	LM3631_DEV_LCD_BIAS(5),

	/* Backlight */
	LM3631_DEV_BL,
};

int lm3631_read_byte(struct lm3631 *lm3631, u8 reg, u8 *read)
{
	int ret;
	unsigned int val;

	ret = regmap_read(lm3631->regmap, reg, &val);
	if (ret < 0)
		return ret;

	*read = (u8)val;
	return 0;
}
EXPORT_SYMBOL_GPL(lm3631_read_byte);

int lm3631_write_byte(struct lm3631 *lm3631, u8 reg, u8 data)
{
	return regmap_write(lm3631->regmap, reg, data);
}
EXPORT_SYMBOL_GPL(lm3631_write_byte);

int lm3631_update_bits(struct lm3631 *lm3631, u8 reg, u8 mask, u8 data)
{
	return regmap_update_bits(lm3631->regmap, reg, mask, data);
}
EXPORT_SYMBOL_GPL(lm3631_update_bits);

static int lm3631_init_device(struct lm3631 *lm3631)
{
	int ret;

	/*
	 * Sequence
	 *
	 * 1) Enable nRST pin
	 * 2) Delay about 1ms (bias delay 200us + EPROM read time 700us)
	 * 3) Set LCD_EN bit to 1
	 */

	ret = gpio_request_one(lm3631->pdata->en_gpio,
			GPIOF_OUT_INIT_HIGH, "lm3631_hwen");
	if (ret){
		pr_info("%s: failed to request gpio.\n", __func__);
		return ret;
	}

	usleep_range(1000, 1500);

/*
              
                                                  
                                
*/
#if 1
	return 0;
#else
	return lm3631_update_bits(lm3631, LM3631_REG_DEVCTRL,
				  LM3631_LCD_EN_MASK,
				  1 << LM3631_LCD_EN_SHIFT);
#endif
/*              */
}

static void lm3631_deinit_device(struct lm3631 *lm3631)
{
	gpio_set_value(lm3631->pdata->en_gpio, 0);
	gpio_free(lm3631->pdata->en_gpio);
}

static int lm3631_parse_dt(struct device *dev, struct lm3631 *lm3631)
{
	struct device_node *node = dev->of_node;
	struct lm3631_platform_data *pdata;
	int ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
/*
              
                                                
                                
*/

	/* Reutnrs one of the errno value if return value is negative */
#if 1
	of_property_read_u32(node, "ti,en-gpio", &pdata->en_gpio);
#else
	pdata->en_gpio = of_get_named_gpio(node, "ti,en-gpio", 0);
#endif

	/* Reutnrs one of the errno value if return value is negative */
	ret = of_property_read_u32(node, "ti,en-gpio",   &pdata->en_gpio);
	ret = of_property_read_u32(node, "ti,lcd-bl-en", &pdata->lcd_bl_en);

	if (pdata->en_gpio < 0)
		return pdata->en_gpio;

#if 1
	of_property_read_u32(node, "ti,bl-en-gpio", &pdata->bl_en_gpio);
#else
	pdata->bl_en_gpio = of_get_named_gpio(node, "ti,bl-en-gpio", 0);
#endif

	if (pdata->bl_en_gpio < 0)
		return pdata->bl_en_gpio;
/*              */

	lm3631->pdata = pdata;

	return 0;
}

static struct regmap_config lm3631_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = LM3631_MAX_REGISTERS,
};

static int lm3631_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct lm3631 *lm3631;
	struct device *dev = &cl->dev;
	struct lm3631_platform_data *pdata = dev_get_platdata(dev);
	int ret;

	pr_info("%s: start.\n", __func__);

	lm3631 = devm_kzalloc(dev, sizeof(*lm3631), GFP_KERNEL);
	if (!lm3631)
		return -ENOMEM;

	lm3631->pdata = pdata;
	if (!pdata) {
		if (IS_ENABLED(CONFIG_OF))
			ret = lm3631_parse_dt(dev, lm3631);
		else
			ret = -ENODEV;

		if (ret) {
			dev_err(dev, "Invalid platform data\n");
			return ret;
		}
	}

	lm3631->regmap = devm_regmap_init_i2c(cl, &lm3631_regmap_config);
	if (IS_ERR(lm3631->regmap))
		return PTR_ERR(lm3631->regmap);

	lm3631->dev = &cl->dev;
	i2c_set_clientdata(cl, lm3631);

	ret = lm3631_init_device(lm3631);
	if (ret) {
		pr_info("%s: failed to init device.\n", __func__);
		return ret;
	}

	pr_info("%s: end.\n", __func__);

/*
              
                                                                      
                                
*/
	return mfd_add_devices(dev, -1, lm3631_devs, ARRAY_SIZE(lm3631_devs),
			NULL, 0, NULL);
/*              */
}

static int lm3631_remove(struct i2c_client *cl)
{
	struct lm3631 *lm3631 = i2c_get_clientdata(cl);

	lm3631_deinit_device(lm3631);
	mfd_remove_devices(lm3631->dev);

	return 0;
}

static const struct i2c_device_id lm3631_ids[] = {
	{ "lm3631", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm3631_ids);

#ifdef CONFIG_OF
static const struct of_device_id lm3631_of_match[] = {
	{ .compatible = "ti,lm3631", },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3631_of_match);
#endif

static struct i2c_driver lm3631_driver = {
	.probe = lm3631_probe,
	.remove = lm3631_remove,
	.driver = {
		.name = "lm3631",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lm3631_of_match),
	},
	.id_table = lm3631_ids,
};

module_i2c_driver(lm3631_driver);

MODULE_DESCRIPTION("TI LM3631 MFD Core");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
