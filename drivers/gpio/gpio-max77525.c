/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/mfd/max77525/max77525.h>

/* register map */
#define REG_GPIO0CNFG		0xBB

#define REG_GPIOPUEN1		0xD3
#define REG_GPIOPUEN2		0xD4
#define REG_GPIOPUEN3		0xD5

#define REG_GPIOPDEN1		0xD6
#define REG_GPIOPDEN2		0xD7
#define REG_GPIOPDEN3		0xD8

#define REG_GPIOAMEN1		0xD9
#define REG_GPIOAMEN2		0xDA
#define REG_GPIOAMEN3		0xDB

#define REG_GPIOINT1		0xDD
#define REG_GPIOINT2		0xDE
#define REG_GPIOINT3		0xDF

#define REG_GPIOxCNFG(X)	(REG_GPIO0CNFG + X)
#define REG_GPIOxPUEN(X)	(REG_GPIOPUEN1 + X/8)
#define REG_GPIOxPDEN(X)	(REG_GPIOPDEN1 + X/8)
#define REG_GPIOxAMEN(X)	(REG_GPIOAMEN1 + X/8)
#define REG_GPIOxINT(X)		(REG_GPIOINT1 + X/8)

/* bit mask */
#define DRV_MASK			0x01
#define DIR_MASK			0x02
#define DI_MASK				0x04
#define DO_MASK				0x08
#define INTCNFG_MASK		0x30
#define DBNC_MASK			0xC0

#define BIT_GPIO0			BIT(0)
#define BIT_GPIO1			BIT(1)
#define BIT_GPIO2			BIT(2)
#define BIT_GPIO3			BIT(3)
#define BIT_GPIO4			BIT(4)
#define BIT_GPIO5			BIT(5)
#define BIT_GPIO6			BIT(6)
#define BIT_GPIO7			BIT(7)

#define BIT_GPIO8			BIT(0)
#define BIT_GPIO9			BIT(1)
#define BIT_GPIO10			BIT(2)
#define BIT_GPIO11			BIT(3)
#define BIT_GPIO12			BIT(4)
#define BIT_GPIO13			BIT(5)
#define BIT_GPIO14			BIT(6)
#define BIT_GPIO15			BIT(7)

#define BIT_GPIO16			BIT(0)
#define BIT_GPIO17			BIT(1)
#define BIT_GPIO18			BIT(2)
#define BIT_GPIO19			BIT(3)
#define BIT_GPIO20			BIT(4)
#define BIT_GPIO21			BIT(5)

#define BIT_GPIO(X)			BIT(X%8)

#define DIR_OUTPUT			0
#define DIR_INPUT			DIR_MASK

#define DO_HIGH				DO_MASK
#define DO_LOW				0

#define NUM_OF_GPIOS		22

enum {
	PULL_UP,
	PULL_DOWN
};

enum {
	GPIO_DIR_OUTPUT,
	GPIO_DIR_INPUT
};

enum {
	MASK_INT,
	FALLING_EDGE_INT,
	RISING_EDGE_INT,
	BOTH_EDGE_INT
};

#define REG_OUTPUT_TYPE_SHIFT	0
#define REG_OUTPUT_TYPE_MASK	0x01
#define REG_MODE_SHIFT			1
#define REG_MODE_MASK			0x02
#define REG_INTCONFIG_SHIFT		4
#define REG_INTCONFIG_MASK		0x30
#define REG_DEBOUNCE_SHIFT		6
#define REG_DEBOUNCE_MASK		0xC0

struct max77525_gpio_chip {
	struct max77525		*max77525;
	struct regmap		*regmap;
	struct gpio_chip	gpio_chip;
};

/**
 * struct max77525_gpio_cfg - structure to specify pin configurtion values
 * @output_type:	indicates pin should be configured as push-pull or open
 *			-drain. This setting applies for gpio output only.
 *			0 - Open-Drain
 *			1 - Push-Pull
 * @mode:		indicates whether the pin should be input or output. This value
 *			should be 0(Output) or 1(Input).
 * @int_config:	indicates the GPI interrupt behavior. This setting applies for
 *			gpio input only. This value should be one of the follow value.
 *			0 - Mask Interrupt
 *			1 - Falling Edge Interrupt
 *			2 - Rising Edge Interrupt
 *			3 - Both Edges interrupt
 * @debounce:	indicate the GPI debounce setting for both rising and falling
 *			edges. This value is ignored when the GPIO is configued as a GPO.
 *			0 = No Debounce
 *			1 = 6ms
 *			2 = 12ms
 *			3 = 24ms
 */
struct max77525_gpio_cfg {
	int output_type;
	int mode;
	int int_config;
	int debounce;
};

struct max77525_gpio_ctrl {
	u8 	gpio_pin;				/* GPIO pin number */
	u8	pull_up;
	u8 	pull_down;
	u8	alternative_mode;
};

static int __max77525_gpio_set_config_bit(struct max77525_gpio_chip *m_chip,
		unsigned offset, u8 mask, u8 value)
{
	int rc;
	unsigned int buf;

	if (WARN_ON(!m_chip))
		return -ENODEV;

	if (offset >= NUM_OF_GPIOS)
		return -EINVAL;

	/* Read GPIOxCNFG register */
	rc = regmap_read(m_chip->regmap, REG_GPIOxCNFG(offset), &buf);
	if (rc) {
		dev_err(m_chip->gpio_chip.dev, "I2C read failed\n");
		return rc;
	}

	buf = (buf & ~mask) | value;

	rc = regmap_write(m_chip->regmap, REG_GPIOxCNFG(offset), buf);
	if (rc)
		dev_err(m_chip->gpio_chip.dev, "%s: I2C write failed\n",
								__func__);
	return rc;
}

static int __max77525_gpio_pull_enable(struct max77525_gpio_chip *m_chip,
		unsigned offset, u8 pull_type, u8 enable)
{
	int rc;
	unsigned int buf, addr;

	if (WARN_ON(!m_chip))
		return -ENODEV;

	if (offset >= NUM_OF_GPIOS)
		return -EINVAL;

	addr = (pull_type == PULL_UP) ? REG_GPIOxPUEN(offset) : REG_GPIOxPDEN(offset);

	/* Read register */
	rc = regmap_read(m_chip->regmap, addr, &buf);
	if (rc) {
		dev_err(m_chip->gpio_chip.dev, "I2C read failed\n");
		return rc;
	}

	buf = buf | ((enable == 1)? BIT_GPIO(offset) : 0);

	/* Write register */
	rc = regmap_write(m_chip->regmap, addr, buf);
	if (rc)
		dev_err(m_chip->gpio_chip.dev, "%s: I2C write failed\n",
								__func__);
	return rc;
}

static int __max77525_gpio_alternative_mode_enable(struct max77525_gpio_chip
		*m_chip, unsigned offset, u8 enable)
{
	int rc;
	unsigned int buf, addr;

	if (WARN_ON(!m_chip))
		return -ENODEV;

	if (offset >= NUM_OF_GPIOS)
		return -EINVAL;

	addr = REG_GPIOxAMEN(offset);

	/* Read register */
	rc = regmap_read(m_chip->regmap, addr, &buf);
	if (rc) {
		dev_err(m_chip->gpio_chip.dev, "I2C read failed\n");
		return rc;
	}

	buf = buf | ((enable == 1)? BIT_GPIO(offset) : 0);

	/* Write register */
	rc = regmap_write(m_chip->regmap, addr, buf);
	if (rc)
		dev_err(m_chip->gpio_chip.dev, "%s: I2C write failed\n",
								__func__);
	return rc;
}


static int max77525_gpio_get(struct gpio_chip *gpio_chip, unsigned offset)
{
	int rc, ret_val;
	unsigned int buf;
	unsigned int buf_test, i;
	struct max77525_gpio_chip *m_chip = dev_get_drvdata(gpio_chip->dev);

	if (WARN_ON(!m_chip))
		return -ENODEV;

	if (offset >= NUM_OF_GPIOS)
		return -EINVAL;

	/* Read GPIOxCNFG register */
	rc = regmap_read(m_chip->regmap, REG_GPIOxCNFG(offset), &buf);
	if (rc) {
		dev_err(m_chip->gpio_chip.dev, "I2C read failed\n");
		return rc;
	}

	return buf;
}

static void max77525_gpio_set(struct gpio_chip *gpio_chip,
		unsigned offset, int value)
{
	int rc;
	unsigned int buf;
	struct max77525_gpio_chip *m_chip = dev_get_drvdata(gpio_chip->dev);

	if (WARN_ON(!m_chip))
		return;

	if (offset >= NUM_OF_GPIOS)
		return;

	buf = (value == 0) ? DO_LOW : DO_HIGH;

	rc = __max77525_gpio_set_config_bit(m_chip, offset, DO_MASK, buf);
	if (rc)
		dev_err(m_chip->gpio_chip.dev, "%s: I2C write failed\n",
								__func__);
	return;
}

static int max77525_gpio_direction_input(struct gpio_chip *gpio_chip,
		unsigned offset)
{
	int rc;
	struct max77525_gpio_chip *m_chip = dev_get_drvdata(gpio_chip->dev);

	if (WARN_ON(!m_chip))
		return -ENODEV;

	rc = __max77525_gpio_set_config_bit(m_chip, offset, DIR_MASK, DIR_INPUT);
	if (rc)
		dev_err(m_chip->gpio_chip.dev, "%s: I2C write failed\n",
								__func__);

	return rc;
}

static int max77525_gpio_direction_output(struct gpio_chip *gpio_chip,
		unsigned offset,
		int val)
{
	int rc;
	unsigned int buf;
	struct max77525_gpio_chip *m_chip = dev_get_drvdata(gpio_chip->dev);

	if (WARN_ON(!m_chip))
		return -ENODEV;

	buf = (val == 0) ? DO_LOW : DO_HIGH;

	rc = __max77525_gpio_set_config_bit(m_chip, offset, (DIR_MASK | DO_MASK),
			(DIR_OUTPUT | buf));
	if (rc)
		dev_err(m_chip->gpio_chip.dev, "%s: I2C write failed\n",
								__func__);

	return rc;
}

static int max77525_gpio_to_irq(struct gpio_chip *gpio_chip, unsigned offset)
{
	struct max77525_gpio_chip *m_chip = dev_get_drvdata(gpio_chip->dev);

	if (WARN_ON(!m_chip))
		return -ENODEV;

#if 1
	return 0;
#else
	return regmap_irq_get_virq(m_chip->max77525->irq_data,
			MAX77525_TOPIRQ_NR + offset);
#endif
}

static void max77525_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	static const char *cmode[] = { "out", "in" };
	u8 mode, state, val, addr;
	const char *label;
	int i;

	for (i = 0; i < NUM_OF_GPIOS; i++) {
		label = gpiochip_is_requested(chip, i);
        addr = REG_GPIOxCNFG(i);
        val = max77525_gpio_get(chip, i);
        mode = (val & REG_MODE_MASK)>>REG_MODE_SHIFT;
		if (mode == GPIO_DIR_OUTPUT)
			state = val & DO_MASK;
		else {
			state = val & DI_MASK;
		}
        seq_printf(s, "gpio-%-3d (%-12.12s) %-10.10s"
				" %s",
				chip->base + i,
				label ? label : "--",
				cmode[mode],
				state ? "hi" : "lo");

        seq_printf(s, "\n");
	}
}

/**
 * max77525_gpio_config - Apply pin configuration for Linux gpio
 * @gpio: Linux gpio number to configure.
 * @param: parameters to configure.
 *
 * This routine takes a Linux gpio number that corresponds with a
 * PMIC pin and applies the configuration specified in 'param'.
 */
static int __max77525_gpio_config(struct max77525_gpio_chip *m_chip,
			    int offset, struct max77525_gpio_cfg *param)
{
	int rc;
	u8 buf;

	if (WARN_ON(!m_chip))
		return -ENODEV;

	if (offset >= NUM_OF_GPIOS)
		return -EINVAL;

	buf = (param->debounce<<6 | param->int_config<<4 |
		param->mode<<1 | param->output_type);

	rc = regmap_write(m_chip->regmap, REG_GPIOxCNFG(offset), buf);
	if (rc) {
		dev_err(m_chip->gpio_chip.dev, "%s: I2C write failed\n",
								__func__);
		dev_err(m_chip->gpio_chip.dev,
				"%s: unable to set default config for pmic pin %d\n",
							__func__, offset);
	}

	return rc;
}


static int max77525_gpio_apply_spec(struct max77525_gpio_chip *m_chip,
					struct max77525_gpio_ctrl *gpio_ctrl)
{
	int rc;
	u8 buf;

	/* Set pull up enable */
	buf = (gpio_ctrl->pull_up == 0) ? 0 : 1;
	rc = __max77525_gpio_pull_enable(m_chip, gpio_ctrl->gpio_pin, PULL_UP, buf);
	if (rc) {
		dev_err(m_chip->gpio_chip.dev,
			"PMIC GPIO=%d pull up enable failed\n", gpio_ctrl->gpio_pin);
		return rc;
	}

	/* Set pull down enable */
	buf = (gpio_ctrl->pull_down == 0) ? 0 : 1;
	rc = __max77525_gpio_pull_enable(m_chip, gpio_ctrl->gpio_pin, PULL_DOWN, buf);
	if (rc) {
		dev_err(m_chip->gpio_chip.dev,
			"PMIC GPIO=%d pull down enable failed\n", gpio_ctrl->gpio_pin);
		return rc;
	}

	/* Set alternative mode enable */
	buf = (gpio_ctrl->alternative_mode == 0) ? 0 : 1;
	rc = __max77525_gpio_alternative_mode_enable(m_chip, gpio_ctrl->gpio_pin, buf);
	if (rc) {
		dev_err(m_chip->gpio_chip.dev,
			"PMIC GPIO=%d alternative mode enable failed\n", gpio_ctrl->gpio_pin);
		return rc;
	}

	return rc;
}

static int max77525_gpio_probe(struct platform_device *pdev)
{
	struct max77525_gpio_chip *m_chip;
	struct max77525_gpio_ctrl *gpio_ctrl = 0;
	struct max77525_gpio_cfg *param = 0;
	struct device_node *pmic_node = pdev->dev.parent->of_node;
	struct device_node *node, *child;
	int rc, num_gpios = 0;

	m_chip = devm_kzalloc(&pdev->dev, sizeof(*m_chip), GFP_KERNEL);
	if (!m_chip) {
		dev_err(&pdev->dev, "%s: Can't allocate gpio_chip\n",
								__func__);
		return -ENOMEM;
	}

	m_chip->max77525 = dev_get_drvdata(pdev->dev.parent);
	m_chip->regmap = m_chip->max77525->regmap;
	dev_set_drvdata(&pdev->dev, m_chip);

    param = devm_kzalloc(&pdev->dev, sizeof(*param), GFP_KERNEL);
    if (!param) {
        dev_err(&pdev->dev, "%s: Can't allocate gpio_cfg\n",
                                __func__);
        return -ENOMEM;
    }

    gpio_ctrl = devm_kzalloc(&pdev->dev, sizeof(*gpio_ctrl), GFP_KERNEL);
    if (!gpio_ctrl) {
        dev_err(&pdev->dev, "%s: Can't allocate gpio_cfg\n",
                                __func__);
        return -ENOMEM;
    }

	node = of_find_node_by_name(pmic_node, "gpios");
	if (!node) {
		dev_err(&pdev->dev, "could not find gpio sub-node\n");
		return -ENODEV;
	}

	for_each_child_of_node(node, child)
		num_gpios++;

	if (!num_gpios)
		return -ECHILD;

	printk("[HUGH] Num gpios = %d \n", num_gpios);

	for_each_child_of_node(node, child) {
		/* get the configuration data from device tree */
		rc = of_property_read_u32(child, "maxim,gpio-num",
				(u32 *)&gpio_ctrl->gpio_pin);
		if (rc) {
			dev_err(&pdev->dev, "%s: unable to get maxim,gpio-num property\n",
								__func__);
			goto err_probe;
		}

		rc = of_property_read_u32(child, "maxim,pull-up",
				(u32 *)&gpio_ctrl->pull_up);
		if (rc) {
			dev_err(&pdev->dev, "%s: unable to get maxim,pull-up property\n",
								__func__);
			goto err_probe;
		}

		rc = of_property_read_u32(child, "maxim,pull-down",
				(u32 *)&gpio_ctrl->pull_down);
		if (rc) {
			dev_err(&pdev->dev, "%s: unable to get maxim,pull-down property\n",
								__func__);
			goto err_probe;
		}

		rc = of_property_read_u32(child, "maxim,alternative-mode",
				(u32 *)&gpio_ctrl->alternative_mode);
		if (rc) {
			dev_err(&pdev->dev, "%s: unable to get maxim,alternative-mode property\n",
								__func__);
			goto err_probe;
		}

		rc = of_property_read_u32(child, "maxim,output-type", &param->output_type);
		if (rc) {
			dev_err(&pdev->dev, "%s: unable to get maxim,output-type property\n",
								__func__);
			goto err_probe;
		}

		rc = of_property_read_u32(child, "maxim,io-mode", &param->mode);
		if (rc) {
			dev_err(&pdev->dev, "%s: unable to get maxim,io-mode property\n",
								__func__);
			goto err_probe;
		}

		rc = of_property_read_u32(child, "maxim,int-config", &param->int_config);
		if (rc) {
			dev_err(&pdev->dev, "%s: unable to get maxim,int-config property\n",
								__func__);
			goto err_probe;
		}

		rc = of_property_read_u32(child, "maxim,debounce", &param->debounce);
		if (rc) {
			dev_err(&pdev->dev, "%s: unable to get maxim,debounce property\n",
								__func__);
			goto err_probe;
		}
		/* now configure gpio config defaults if they exist */
		rc = __max77525_gpio_config(m_chip, gpio_ctrl->gpio_pin, param);
		if (rc)
			goto err_probe;

		rc = max77525_gpio_apply_spec(m_chip, gpio_ctrl);
		if (rc)
			goto err_probe;
	}

	m_chip->gpio_chip.base = -1;
	m_chip->gpio_chip.label = "max77525-gpio";
	m_chip->gpio_chip.direction_input = max77525_gpio_direction_input;
	m_chip->gpio_chip.direction_output = max77525_gpio_direction_output;
	m_chip->gpio_chip.to_irq = max77525_gpio_to_irq;
	m_chip->gpio_chip.get = max77525_gpio_get;
	m_chip->gpio_chip.set = max77525_gpio_set;
	m_chip->gpio_chip.dbg_show = max77525_gpio_dbg_show;
	m_chip->gpio_chip.dev = &pdev->dev;
	m_chip->gpio_chip.can_sleep = 0;
	m_chip->gpio_chip.ngpio = num_gpios;

	rc = gpiochip_add(&m_chip->gpio_chip);
	if (rc) {
		dev_err(&pdev->dev, "%s: Can't add gpio chip, rc = %d\n",
								__func__, rc);
		goto err_probe;
	}

	return 0;

err_probe:
	return rc;
}

static int max77525_gpio_remove(struct platform_device *pdev)
{
	struct max77525_gpio_chip *m_chip = dev_get_drvdata(&pdev->dev);

	return gpiochip_remove(&m_chip->gpio_chip);
}

#ifdef CONFIG_OF
static struct of_device_id max77525_gpio_match_table[] = {
	{	.compatible = "maxim,max77525-gpio", },
	{ }
};
MODULE_DEVICE_TABLE(of, max77525_gpio_match_table);
#endif

static const struct platform_device_id max77525_gpio_id[] = {
	{ "max77525-gpio", 0},
	{ }
};
MODULE_DEVICE_TABLE(platform, max77525_gpio_id);

static struct platform_driver max77525_gpio_driver = {
	.driver		= {
		.name	= "maxim,max77525-gpio",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max77525_gpio_match_table),
	},
	.probe		= max77525_gpio_probe,
	.remove		= max77525_gpio_remove,
	.id_table	= max77525_gpio_id,
};

static int __init max77525_gpio_init(void)
{
	return platform_driver_register(&max77525_gpio_driver);
}
subsys_initcall(max77525_gpio_init);

static void __exit max77525_gpio_exit(void)
{
	platform_driver_unregister(&max77525_gpio_driver);
}
module_exit(max77525_gpio_exit);


/* TEST Code for MAXIM GPIO */
/* Select range of GPIOs 506-527 */
static int __init max77525_gpiotest_init(void)
{
    int ret;
	int gpio4 = 510;

	ret = gpio_request(gpio4, "SENSOR_LDO_EN_MAX");
	if (ret < 0)
		goto err;

	ret = gpio_direction_output(gpio4, 1);
	if (ret < 0)
		goto err;

	return 0;
err:
	printk("TEST Fail \n");
    return ret;
}

/* late_initcall(max77525_gpiotest_init); */
MODULE_DESCRIPTION("MAX77525 PMIC gpio driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Clark Kim <clark.kim@maximintegrated.com>");
MODULE_VERSION("1.0");

