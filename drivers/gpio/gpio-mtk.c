/*
 * GPIO driver for MediaTeK MT8135 & MT6397 chips
 *
 * Copyright (C) 2014, Alexey Polyudov <alexeyp@lab126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*
 * Due to MTK chip peculiarities, GPIO and GPIO interrupts (EINT) are
 * distinct pieces of HW. They require separate sets of platform data
 * and hence separate drivers.
 * Normally GPIO driver handles SoC GPIO interrupts as well,
 * this is why both drivers are implemented in the same file.
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <mach/mt_pmic_wrap.h>
#include <mach/mt_gpio_def.h>
#include <mach/eint.h>
#include <mach/mt_irq.h>

static int mtk_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct mt_gpio_chip_def *mt_chip = dev_get_platdata(chip->dev);
	struct mt_eint_def *eint = mt_chip->base_pin[offset].eint;

	if (!eint || !eint->gpio)
		return -EINVAL;

	return EINT_IRQ(eint->id);
}

static int mtk_gpio_set_debounce(struct gpio_chip *chip,
					unsigned offset, unsigned debounce)
{
	struct mt_gpio_chip_def *mt_chip = dev_get_platdata(chip->dev);
	struct mt_eint_def *eint = mt_chip->base_pin[offset].eint;

	if (!eint)
		return -EINVAL;

	/* debounce is only supported for EINT mode, not for IO function */
	mt_eint_set_hw_debounce(eint->id, debounce);
	return 0;
}

static int mtk_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	struct mt_gpio_chip_def *mt_chip = dev_get_platdata(chip->dev);
	u32 val = 0;
	u32 bit = 1 << (offset & 0xF);
	addr_t reg = mt_chip->hw_base[0] + mt_chip->dir_offset
		+ (((offset >> 4) & mt_chip->port_mask) << mt_chip->port_shf);
	mt_chip->read_reg(reg, &val);
	return (val & bit) > 0 ? GPIOF_DIR_OUT : GPIOF_DIR_IN;
}

static int mtk_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct mt_gpio_chip_def *mt_chip = dev_get_platdata(chip->dev);
	u32 val = 0;
	u32 bit = 1 << (offset & 0xF);
	addr_t reg = mt_chip->hw_base[0]
		+ (((offset >> 4) & mt_chip->port_mask) << mt_chip->port_shf);
	reg += mtk_gpio_get_direction(chip, offset) == GPIOF_DIR_IN ?
			mt_chip->din_offset : mt_chip->dout_offset;

	mt_chip->read_reg(reg, &val);
	return (val & bit) > 0 ? 1 : 0;
}

static void mtk_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct mt_gpio_chip_def *mt_chip = dev_get_platdata(chip->dev);
	u32 bit = 1 << (offset & 0xF);
	addr_t reg = mt_chip->hw_base[0]
		+ (((offset >> 4) & mt_chip->port_mask) << mt_chip->port_shf);
	reg += mtk_gpio_get_direction(chip, offset) == GPIOF_DIR_IN ?
			mt_chip->din_offset : mt_chip->dout_offset;

	if (value)
		mt_chip->write_reg(reg + SET_REG(mt_chip), bit);
	else
		mt_chip->write_reg(reg + CLR_REG(mt_chip), bit);
}

static inline int mtk_gpio_set_direction(
	struct gpio_chip *chip, unsigned offset, int value)
{
	struct mt_gpio_chip_def *mt_chip = dev_get_platdata(chip->dev);
	u32 bit = 1 << (offset & 0xF);
	addr_t reg = mt_chip->hw_base[0] + mt_chip->dir_offset
		+ (((offset >> 4) & mt_chip->port_mask) << mt_chip->port_shf);

	reg +=  (value == GPIOF_DIR_IN) ? CLR_REG(mt_chip) : SET_REG(mt_chip);

	return mt_chip->write_reg(reg, bit);
}

static int mtk_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	return mtk_gpio_set_direction(chip, offset, GPIOF_DIR_IN);
}

static int mtk_gpio_direction_output(
	struct gpio_chip *chip, unsigned offset, int value)
{
	int err = mtk_gpio_set_direction(chip, offset, GPIOF_DIR_OUT);
	mtk_gpio_set(chip, offset, value);
	return err;
}

static int mtk_gpio_probe(struct platform_device *pdev)
{
	int err;
	struct gpio_chip *chip;
	struct mt_gpio_chip_def *mt_chip;

	mt_chip = dev_get_platdata(&pdev->dev);
	if (!mt_chip)
		return -ENODEV;

	chip = kzalloc(sizeof(struct gpio_chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	chip->label	= pdev->name;

	chip->get_direction		= mtk_gpio_get_direction;
	chip->direction_input	= mtk_gpio_direction_input;
	chip->direction_output	= mtk_gpio_direction_output;
	chip->get				= mtk_gpio_get;
	chip->set				= mtk_gpio_set;
	chip->set_debounce		= mtk_gpio_set_debounce;
	chip->to_irq			= mtk_gpio_to_irq;

	chip->base	= mt_chip->base_pin - mt_get_pin_def(0);
	chip->ngpio	= mt_chip->n_gpio;

	err = gpiochip_add(chip);

	if (err) {
		dev_err(&pdev->dev,
			"Failed to init GPIO chip: err=%d\n", err);
		goto err_free;
	}

	dev_err(&pdev->dev,
		"Init GPIO chip: OK; name=%s; total %d pins\n",
		pdev->name, chip->ngpio);
	return 0;

err_free:
	kfree(chip);
	return err;
}

struct platform_device_id mtk_gpio_id[] = {
	{
		.name = "mt8135-gpio",
		.driver_data = 0,
	},
	{
		.name = "mt6397-gpio",
		.driver_data = 1,
	},
	{
	},
};

static struct platform_driver mtk_gpio_driver = {
	.driver		= {
		.name	= "mtk-gpio",
		.owner	= THIS_MODULE,
	},
	.probe		= mtk_gpio_probe,
	.id_table   = mtk_gpio_id,
};

static int __init mtk_gpio_init(void)
{
	return platform_driver_register(&mtk_gpio_driver);
}
postcore_initcall(mtk_gpio_init);
