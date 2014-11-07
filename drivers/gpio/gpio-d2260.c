/*
 * GPIO Driver for Dialog D2260 PMICs.
 *
 * Copyright(c) 2013 Dialog Semiconductor Ltd.
 *
 * Author: Alvin Park <alvin.park@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/syscalls.h>
#include <linux/seq_file.h>
#include <linux/regmap.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#endif

#include <linux/mfd/d2260/core.h>
#include <linux/mfd/d2260/registers.h>
#include <linux/mfd/d2260/pdata.h>

#define D2260_INPUT					1
#define D2260_OUTPUT_OPENDRAIN		2
#define D2260_OUTPUT_PUSHPULL		3

#define D2260_DEBOUNCING_OFF		0
#define D2260_DEBOUNCING_ON			1

#define D2260_ACTIVE_LOW			0
#define D2260_ACTIVE_HIGH			1

#define D2260_GPIO_MAX_PORTS_PER_REGISTER	8
#define D2260_GPIO_SHIFT_COUNT(no)		(no%8)

#define D2260_GPIO_MASK_UPPER_NIBBLE		0xF0
#define D2260_GPIO_MASK_LOWER_NIBBLE		0x0F
#define D2260_GPIO_NIBBLE_SHIFT		4

struct d2260_gpio {
	struct d2260 *d2260;
	struct gpio_chip gp;
};

enum {
	D2260_GPIO_0,
	D2260_GPIO_1,
	D2260_GPIO_MAX
};

struct gpiomap {
	u16 reg;
	u8 pin_mask;
	u8 pin_bit;
	u8 mode_mask;
	u8 mode_bit;
	u8 type_mask;
	u8 type_bit;
	u8 io_mask;
	u8 io_bit;
	u8 nibble;
};

#define D2260_GPIO(_name, _reg, _mask, _nibble) \
[D2260_##_name] = \
	{ \
		.reg = D2260_##_reg##_REG, \
		.pin_mask = D2260_##_mask##_PIN_MASK, \
		.pin_bit =  D2260_##_mask##_PIN_BIT, \
		.mode_mask = D2260_##_mask##_MODE_MASK, \
		.mode_bit = D2260_##_mask##_MODE_BIT, \
		.type_mask = D2260_##_mask##_TYPE_MASK, \
		.type_bit = D2260_##_mask##_TYPE_BIT, \
		.io_mask = D2260_##_name##_MASK, \
		.io_bit = D2260_##_name##_BIT, \
		.nibble = D2260_GPIO_MASK_##_nibble##_NIBBLE, \
	}

static struct gpiomap d2260_gpio_desc[D2260_GPIO_MAX] = {
	D2260_GPIO(GPIO_0, GPIO_0_NON, 	GPIO0, LOWER),
	D2260_GPIO(GPIO_1, TA_GPIO_1, 	GPIO1, UPPER),
};

static inline struct d2260_gpio *to_d2260_gpio(struct gpio_chip *chip)
{
	return container_of(chip, struct d2260_gpio, gp);
}

static inline struct gpiomap *d2260_get_gpio_desc(unsigned offset) {
	return (offset < D2260_GPIO_MAX) ? &d2260_gpio_desc[offset] : NULL;
}

static int d2260_gpio_get(struct gpio_chip *gc, unsigned offset)
{
	struct d2260_gpio *gpio = to_d2260_gpio(gc);
	struct gpiomap *desc = NULL;
	u8 reg_value;
	int d2260_port_direction = 0;
	int ret = 0;

	desc = d2260_get_gpio_desc(offset);
	if (desc == NULL)	return -EINVAL;

	ret = d2260_reg_read(gpio->d2260, desc->reg, &reg_value);
	if (ret < 0)
		return ret;

	d2260_port_direction = (reg_value & desc->pin_mask) >> desc->pin_bit;
	switch (d2260_port_direction) {
		case D2260_INPUT:
			ret = d2260_reg_read(gpio->d2260, D2260_STATUS_C_REG, &reg_value);
			if (ret < 0)
				return ret;

			ret = ((reg_value & desc->io_mask) >> desc->io_bit);
			break;

		case D2260_OUTPUT_PUSHPULL:
			ret = ((reg_value & desc->mode_mask) >> desc->mode_bit);
			break;

		default:
			return -EINVAL;
	}

	return ret;
}

static void d2260_gpio_set(struct gpio_chip *gc, unsigned offset, int value)
{
	struct d2260_gpio *gpio = to_d2260_gpio(gc);
	struct gpiomap *desc = NULL;
	int ret = 0;

	desc = d2260_get_gpio_desc(offset);
	if (desc == NULL)	return;

	if (value) {
		ret = d2260_set_bits(gpio->d2260, desc->reg,
			desc->mode_mask & (value << desc->mode_bit));
	} else {
		ret = d2260_clear_bits(gpio->d2260, desc->reg, desc->mode_mask);
	}
	if (ret != 0)
		dev_err(gpio->d2260->dev, "Failed to updated gpio reg, %d", ret);
}

static int d2260_gpio_direction_input(struct gpio_chip *gc, unsigned offset)
{
	struct d2260_gpio *gpio = to_d2260_gpio(gc);
	struct gpiomap *desc = NULL;
	unsigned char register_value;
	u8 reg_value = 0;
	int ret = 0;

	desc = d2260_get_gpio_desc(offset);
	if (desc == NULL)	return -EINVAL;

	ret = d2260_reg_read(gpio->d2260, desc->reg, &reg_value);
	if (ret != 0)
		dev_err(gpio->d2260->dev, "Failed to read gpio reg, %d", ret);

	/* Format: function - 2 bits type - 1 bit mode - 1 bit */
	register_value = (reg_value & desc->nibble) |
		D2260_INPUT << desc->pin_bit |
		D2260_ACTIVE_LOW << desc->type_bit |
		D2260_DEBOUNCING_ON << desc->mode_bit;

	DIALOG_DEBUG(gpio->d2260->dev, "%s: reg[0x%02x] value=0x%02x", __func__,
		desc->reg, register_value);

	ret = d2260_reg_write(gpio->d2260, desc->reg, register_value);
	if (ret != 0)
		dev_err(gpio->d2260->dev, "Failed to updated gpio reg, %d", ret);

	return ret;
}

static int d2260_gpio_direction_output(struct gpio_chip *gc,
					unsigned offset, int value)
{
	struct d2260_gpio *gpio = to_d2260_gpio(gc);
	struct gpiomap *desc = NULL;
	unsigned char register_value;
	u8 reg_value = 0;
	int ret = 0;

	desc = d2260_get_gpio_desc(offset);
	if (desc == NULL)	return -EINVAL;

	ret = d2260_reg_read(gpio->d2260, desc->reg, &reg_value);
	if (ret != 0)
		dev_err(gpio->d2260->dev, "Failed to read gpio reg, %d", ret);

	/* Format: Function - 2 bits Type - 1 bit Mode - 1 bit */
	register_value = (reg_value & desc->nibble) |
		D2260_OUTPUT_PUSHPULL << desc->pin_bit |
		value << desc->mode_bit;

	DIALOG_DEBUG(gpio->d2260->dev, "%s: reg[0x%02x] value=0x%02x", __func__,
		desc->reg, register_value);

	ret = d2260_reg_write(gpio->d2260, desc->reg, register_value);
	if (ret != 0)
		dev_err(gpio->d2260->dev, "Failed to updated gpio reg, %d", ret);

	return ret;
}

static int d2260_gpio_to_irq(struct gpio_chip *gc, u32 offset)
{
	struct d2260_gpio *gpio = to_d2260_gpio(gc);
	struct d2260 *d2260 = gpio->d2260;

	int irq;

	irq = regmap_irq_get_virq(d2260->irq_data, D2260_IRQ_GPI0 + offset);

	return irq;
}

static struct gpio_chip reference_gp = {
	.label = "d2260-gpio",
	.owner = THIS_MODULE,
	.get = d2260_gpio_get,
	.set = d2260_gpio_set,
	.direction_input = d2260_gpio_direction_input,
	.direction_output = d2260_gpio_direction_output,
	.to_irq = d2260_gpio_to_irq,
	.can_sleep = 1,
	.ngpio = D2260_GPIO_MAX,
	.base = -1,
};

static int d2260_gpio_probe(struct platform_device *pdev)
{
	struct d2260_gpio *gpio;
	struct d2260_pdata *pdata;
	int ret = 0;


	gpio = devm_kzalloc(&pdev->dev, sizeof(*gpio), GFP_KERNEL);
	if (gpio == NULL)
		return -ENOMEM;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	gpio->d2260 = dev_get_drvdata(pdev->dev.parent);

	pdata = gpio->d2260->dev->platform_data;
	gpio->gp = reference_gp;
	if (pdata && pdata->gpio_base)
		gpio->gp.base = pdata->gpio_base;

#if CONFIG_OF
	else {
		struct device_node *np = gpio->d2260->dev->of_node;
		if (np) {
			u32 gpio_base;
			struct device_node *gpio_np = of_find_node_by_name(np,"gpios");
			ret = of_property_read_u32(gpio_np, "gpio-base", &gpio_base);
			if (ret < 0)
				gpio->gp.base = -1;
			else
				gpio->gp.base = gpio_base;
		}
	}
#endif

	ret = gpiochip_add(&gpio->gp);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not register gpiochip, %d\n", ret);
		goto err;
	}

	platform_set_drvdata(pdev, gpio);

	return 0;

err:
	kfree(pdata);
out:
	kfree(gpio);
	return ret;
}

static int d2260_gpio_remove(struct platform_device *pdev)
{
	struct d2260_gpio *gpio = platform_get_drvdata(pdev);

	return gpiochip_remove(&gpio->gp);
}

#ifdef CONFIG_OF
static const struct of_device_id d2260_gpiochip_match[] = {
	{ .compatible = "dlg,d2260-gpio", },
	{}
};
MODULE_DEVICE_TABLE(of, d2260_gpiochip_match);
#endif

static struct platform_driver d2260_gpio_driver = {
	.probe = d2260_gpio_probe,
	.remove = d2260_gpio_remove,
	.driver = {
		.name	= "d2260-gpio",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(d2260_gpiochip_match),
#endif
	},
};

static int __init d2260_gpio_init(void)
{
	return platform_driver_register(&d2260_gpio_driver);
}
subsys_initcall(d2260_gpio_init);

static void __exit d2260_gpio_exit(void)
{
	platform_driver_unregister(&d2260_gpio_driver);
}
module_exit(d2260_gpio_exit);

/* TEST Code for DIALOG GPIO */
/* Select range of GPIOs 511 */
#if 0
static int __init d2260_gpiotest_init(void)
{
    int ret;
	int gpio1 = 511;

	ret = gpio_request(gpio1, "EXT_PWR_EN");
	if (ret < 0)
		goto err;

	ret = gpio_direction_output(gpio1, 0);
	if (ret < 0)
		goto err;

	return 0;
err:
	BUG_ON(gpio1);
	printk("TEST Fail \n");
    return ret;
}

late_initcall(d2260_gpiotest_init);
#endif

MODULE_AUTHOR("Alvin Park <alvin.park@diasemi.com>");
MODULE_DESCRIPTION("D2260 GPIO Device Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:d2260-gpio");
