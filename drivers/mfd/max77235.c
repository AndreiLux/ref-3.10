/*
 * max77235.c - mfd core driver for the Maxim 77235
 *
 * Copyright (C) 2013 Maxim Integrated
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max77686.c
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/regmap-ipc.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77235.h>

#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/mailbox.h>
#include <linux/completion.h>

/* registers */
#define MAX77235_REG_DEVICE_ID	0x00
#define MAX77235_REG_INTSRC		0x01
#define MAX77235_REG_INT1		0x02
#define MAX77235_REG_INT2		0x03
#define MAX77235_REG_INT1MSK	0x04
#define MAX77235_REG_INT2MSK	0x05
#define MAX77235_REG_STATUS1	0x06
#define MAX77235_REG_STATUS2	0x07
#define MAX77235_REG_PWRON		0x08

/* MAX77235_REG_DEVICE_ID */
#define MAX77235_VERSION_M		0x78
#define MAX77235_CHIP_REV_M		0x07
/* MAX77235_REG_INTSRC */
#define MAX77235_RTC_M			0x01

/* MAX77235_REG_INT1 */
#define MAX77235_PWRONF_M		0x01
#define MAX77235_PWRONR_M		0x02
#define MAX77235_JIGONF_M		0x04
#define MAX77235_JIGONR_M		0x08
#define MAX77235_ACOKBF_M		0x10
#define MAX77235_ACOKBR_M		0x20
#define MAX77235_ONKEY1S_M		0x40
#define MAX77235_MRSTB_M		0x80

/* MAX77235_REG_INT2 */
#define MAX77235_140C_M			0x01
#define MAX77235_120C_M			0x02

/* MAX77235_REG_INT1MSK */
#define MAX77235_PWRONFM_M		0x01
#define MAX77235_PWRONRM_M		0x02
#define MAX77235_JIGONFM_M		0x04
#define MAX77235_JIGONRM_M		0x08
#define MAX77235_ACOKBFM_M		0x10
#define MAX77235_ACOKBRM_M		0x20
#define MAX77235_ONKEY1SM_M		0x40
#define MAX77235_MRSTBM_M		0x80

/* MAX77235_REG_INT2MSK */
#define MAX77235_140CM_M		0x01
#define MAX77235_120CM_M		0x02

/* MAX77235_REG_STATUS2 */
#define MAX77235_SMPLEVENT_M	0x10
#define MAX77235_WTSREVENT_M	0x20

/* MAX77235_REG_PWRON */
#define MAX77235_PWRON_M		0x01
#define MAX77235_JIGON_M		0x02
#define MAX77235_ACOKB_M		0x04
#define MAX77235_MRSTB_PWRON_M	0x08
#define MAX77235_RTCA1_M		0x10
#define MAX77235_RTCA2_M		0x20
#define MAX77235_SMPLON_M		0x40
#define MAX77235_WSTRON_M		0x80

#define PMIC_I2C_SLOT			0
#define MAX77235_SLAVE_ADDRESS	0x12

const static struct regmap_irq max77235_irqs[] =
{
	{ .mask = MAX77235_PWRONFM_M, },
	{ .mask = MAX77235_PWRONRM_M, },
	{ .mask = MAX77235_JIGONFM_M, },
	{ .mask = MAX77235_JIGONRM_M, },
	{ .mask = MAX77235_ACOKBFM_M, },
	{ .mask = MAX77235_ACOKBRM_M, },
	{ .mask = MAX77235_ONKEY1SM_M, },
	{ .mask = MAX77235_MRSTBM_M, },
	{ .mask = MAX77235_140CM_M, .reg_offset = 1, },
	{ .mask = MAX77235_120CM_M, .reg_offset = 1, },
};

const static struct regmap_irq_chip max77235_irq_chip =
{
	.name = "max77235",
	.irqs = max77235_irqs,
	.num_irqs = ARRAY_SIZE(max77235_irqs),
	.num_regs = 2,
	.status_base = MAX77235_REG_INT1,
	.mask_base = MAX77235_REG_INT1MSK,
};

const static struct regmap_irq max77235_rtc_irqs[] =
{
	{ .mask = MAX77235_RTC_M, },
};

const static struct regmap_irq_chip max77235_rtc_irq_chip =
{
	.name = "max77235-rtc",
	.irqs = max77235_rtc_irqs,
	.num_irqs = ARRAY_SIZE(max77235_rtc_irqs),
	.num_regs = 1,
	.status_base = MAX77235_REG_INTSRC,
};

const struct regmap_config max77235_regmap_config =
{
	.reg_bits = 8,
	.val_bits = 8,
};

static int max77235_hw_init(const struct max77235 *max77235,
		const struct max77235_platform_data *pdata)
{
	struct regmap *regmap = max77235->regmap;
	unsigned int val;
	int ret;

	ret = regmap_read(regmap, MAX77235_REG_DEVICE_ID, &val);
	if (IS_ERR_VALUE(ret))
		return -ENODEV;

	dev_info(max77235->dev, "device found. version %d revision PASS%d.\n",
			(val & MAX77235_VERSION_M) >> MASK2SHIFT(MAX77235_VERSION_M),
			(val & MAX77235_CHIP_REV_M) >> MASK2SHIFT(MAX77235_CHIP_REV_M));

	return ret;
}

#ifdef CONFIG_OF
static struct mfd_cell max77235_cells[] =
{
	{ .name = "max77235-regulator", },
	{ .name = "max77235-misc", },
	{ .name = "max77235-clk", },
	{ .name = "max77235-rtc", },
};

static struct max77235_platform_data *max77235_parse_dt(struct device *dev)
{
	struct max77235_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	pdata->cells = max77235_cells;
	//pdata->num_cells = ARRAY_SIZE(max77235_cells);

	return pdata;
}
#endif

static int max77235_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct max77235_platform_data *pdata;
	struct max77235 *max77235;
	struct regmap *regmap;
	struct ipc_client *ipc;
	int ret = 0, irq = 0;

	max77235 = kzalloc(sizeof(*max77235), GFP_KERNEL);
	if (unlikely(max77235 == NULL))
		return -ENOMEM;

	if(pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node, "gpio_num", &max77235->num_gpio);
		if(ret < 0)
			return -ENOMEM;
		}
	else {
		pdata = pdev->dev.platform_data;
		if(pdata == NULL)
			return -ENODEV;
	}

	ret = gpio_request(max77235->num_gpio, "pmic_irq");
	if (ret < 0)
		goto out_free;

	ret = gpio_direction_input(max77235->num_gpio);
	if (ret < 0)
		goto out_free;

	irq = gpio_to_irq(max77235->num_gpio);
	if (irq < 0)
		goto out_free;

	ipc = kzalloc(sizeof(struct ipc_client), GFP_KERNEL);
	if (ipc == NULL) {
		dev_err(max77235->dev, "Platform data not specified.\n");
		return -ENOMEM;
	}
	ipc->slot = PMIC_I2C_SLOT;
	ipc->addr = MAX77235_SLAVE_ADDRESS;
	ipc->dev  = dev;

	regmap = devm_regmap_init_ipc(ipc, &max77235_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	max77235->dev = dev;
	max77235->regmap = regmap;
	max77235->irq = irq;

	platform_set_drvdata(pdev, max77235);

	ret = max77235_hw_init(max77235, pdata);
	if (IS_ERR_VALUE(ret))
		return ret;

#ifndef CONFIG_MFD_MAX77235_IRQ_DISABLE
	ret = regmap_add_irq_chip(max77235->regmap, max77235->irq,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_SHARED,
		 -1, &max77235_irq_chip, &max77235->irq_data);
	if (IS_ERR_VALUE(ret))
		goto err;

	ret = regmap_add_irq_chip(max77235->regmap, max77235->irq,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_SHARED,
		-1, &max77235_rtc_irq_chip, &max77235->rtc_irq_data);
	if (IS_ERR_VALUE(ret))
		goto err;
#endif

	ret = mfd_add_devices(dev, -1, max77235_cells, ARRAY_SIZE(max77235_cells), NULL, 0, NULL);
	if (IS_ERR_VALUE(ret))
		return ret;

	return ret;

out_free:
	gpio_free(max77235->num_gpio);

err:
	if (max77235->irq_data)
		regmap_del_irq_chip(max77235->irq, max77235->irq_data);

	if (max77235->rtc_irq_data)
		regmap_del_irq_chip(max77235->irq, max77235->rtc_irq_data);
}

static int max77235_remove(struct platform_device *pdev)
{
	struct max77235 *max77235 = platform_get_drvdata(pdev);

	mfd_remove_devices(max77235->dev);
	regmap_del_irq_chip(max77235->irq, max77235->irq_data);
	regmap_del_irq_chip(max77235->irq, max77235->rtc_irq_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id max77235_dt_match[] __initdata = {
	{.compatible = "LG,odin-max77235-pmic",},
	{}
};
#endif


static struct platform_driver max77235_driver =
{
	.driver =
	{
		.name = "max77235",
		.owner = THIS_MODULE,
	},
	.driver.of_match_table	= of_match_ptr(max77235_dt_match),
	.probe					= max77235_probe,
	.remove = max77235_remove,
};

static int __init max77235_ipc_init(void)
{
	return platform_driver_register(&max77235_driver);
}

module_init(max77235_ipc_init);

static void __exit max77235_ipc_exit(void)
{
	platform_driver_unregister(&max77235_driver);
}
module_exit(max77235_ipc_exit);

MODULE_DESCRIPTION("MAXIM 77235 multi-function core driver");
MODULE_AUTHOR("Gyungoh Yoo <jack.yoo@maximintegrated.com>");
MODULE_LICENSE("GPL");
