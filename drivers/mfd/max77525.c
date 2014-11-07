/*
 * max77525.c - mfd core driver for the Maxim 77525
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
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77525/max77525.h>
#include <linux/mfd/max77525/max77525_reg.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap-ipc.h>
#include <linux/gpio.h>

static struct mfd_cell max77525_cells[] = {
	{ .name = "max77525-regulator", },
	{ .name = "max77525-rtc", },
	{ .name = "max77525-clk", },
	{ .name = "max77525-led", },
	{ .name = "max77525-adc", },
	{ .name = "max77525-misc", },
	{ .name = "max77525-gpio", },
	{ .name = "max77525-power-key", },
};

#if 0 /* Need to be checked */
#ifdef CONFIG_OF
static struct max77525_platform_data
		*max77525_ipc_parse_dt_pdata(struct device *dev)
{
	struct max77525_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for pdata\n");
		return NULL;
	}

	pdata->dev.platform_data = pdata;
	of_property_read_u32(node, "max77525,sysuvlo",
		&pdata->sysuvlo);

	return pdata;
}
#endif
#endif

static const struct regmap_irq max77525_irqs[] =
{
    { .reg_offset = 0, .mask = BIT_MBATDETINT,  },
	{ .reg_offset = 0, .mask = BIT_TOPSYSINT,  },
	{ .reg_offset = 0, .mask = BIT_GPIOINT,  },
	{ .reg_offset = 0, .mask = BIT_ADCINT,  },
	{ .reg_offset = 0, .mask = BIT_RTCINT,  },
};

static const struct regmap_irq_chip max77525_irq_chip =
{
	.name = "max77525",
	.irqs = max77525_irqs,
	.num_irqs = ARRAY_SIZE(max77525_irqs),
	.num_regs = 1,
	.status_base = REG_INTTOPA,
	.mask_base = REG_INTTOPAM,
};

static const struct regmap_config max77525_regmap_config =
{
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = 0,
	.use_single_rw = 1,
	.reg_stride =1,
};

static int max77525_hw_init(struct max77525 *max77525,
	struct max77525_platform_data *pdata)
{
	struct regmap *regmap = max77525->regmap;
	unsigned int val;
	int ret;

	ret = regmap_read(regmap, REG_CHIPID, &val);
	if (IS_ERR_VALUE(ret))
		return -ENODEV;

	dev_info(max77525->dev, "device found. chipid=%d revision PASS%d.\n",
			val & 0x07, (val & 0x38) >> 3);

#if 0 /* Need to be checked */
	/* set SYS UVLO */
	val = pdata->sysuvlo;
	ret = regmap_write(regmap, REG_SYSUVLOCNFG, val);
#endif

	return ret;
}

static int max77525_ipc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77525_platform_data *pdata = dev_get_platdata(dev);
	struct max77525 *max77525;
	struct regmap *regmap;
	struct ipc_client *ipc;
	int ret = 0, irq = 0;

	max77525 = devm_kzalloc(dev, sizeof(struct max77525), GFP_KERNEL);
	if (unlikely(max77525 == NULL)) {
        ret = -ENOMEM;
        goto err_alloc_drvdata;
    }

#if 0 /* Need to be checked */
	if (pdev->dev.of_node)
		pdata = max77525_ipc_parse_dt_pdata(&pdev->dev);

	if (!pdata) {
		ret = -EIO;
		dev_err(max77525->dev, "No platform data found.\n");
		goto err_add_devices;
	}
#endif

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node, "irq_gpio",
						&max77525->irq_gpio);
		if (ret < 0)
			return -ENOMEM;
		}
	else {
		pdata = pdev->dev.platform_data;
		if (pdata == NULL)
			return -ENODEV;
	}

	ret = gpio_request(max77525->irq_gpio, "pmic_irq");
	if (ret < 0)
		goto out_free;

	ret = gpio_direction_input(max77525->irq_gpio);
	if (ret < 0)
		goto out_free;

	irq = gpio_to_irq(max77525->irq_gpio);
	if (irq < 0)
		goto out_free;

	ipc = kzalloc(sizeof(struct ipc_client), GFP_KERNEL);
	if (ipc == NULL) {
		dev_err(max77525->dev, "Platform data not specified.\n");
		return -ENOMEM;
	}
	ipc->slot = PMIC_I2C_SLOT;
	ipc->addr = MAX77525_SLAVE_ADDRESS;
	ipc->dev  = dev;

	regmap = devm_regmap_init_ipc(ipc, &max77525_regmap_config);
	if (IS_ERR(regmap)) {
        ret = PTR_ERR(regmap);
        dev_err(max77525->dev, "regmap init failed: %d\n", ret);
        goto err_regmap_init;
    }

	max77525->dev = dev;
	max77525->regmap = regmap;
	max77525->irq = irq;

	platform_set_drvdata(pdev, max77525);

	ret = max77525_hw_init(max77525, pdata);
    if (ret != 0) {
        dev_err(max77525->dev, "failed to initialize H/W %d\n", ret);
        goto err_hw_init;
    }

	ret = regmap_add_irq_chip(max77525->regmap, max77525->irq,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQF_SHARED, -1,
                &max77525_irq_chip,
                &max77525->irq_data);
	if (ret != 0) {
		dev_err(max77525->dev, "failed to add irq chip: %d\n", ret);
		goto err_add_irq;
	}

	ret = mfd_add_devices(dev, -1, max77525_cells,
						  ARRAY_SIZE(max77525_cells), NULL, 0, NULL);
	if (ret != 0) {
		dev_err(max77525->dev, "failed to add MFD devices %d\n", ret);
		goto err_add_devices;
	}

	return 0;

err_add_devices:
	regmap_del_irq_chip(max77525->irq, max77525->irq_data);
err_add_irq:
err_hw_init:
err_regmap_init:
err_alloc_drvdata:
out_free:
	gpio_free(max77525->irq_gpio);
	return ret;

}

static int max77525_ipc_remove(struct platform_device *pdev)
{
	const struct max77525 *max77525 = platform_get_drvdata(pdev);

	regmap_del_irq_chip(max77525->irq, max77525->irq_data);

    mfd_remove_devices(max77525->dev);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id max77525_of_match[] __initdata= {
	{ .compatible = "LG,odin-max77525-pmic" },
	{}
};
MODULE_DEVICE_TABLE(of, max77525_of_match);
#endif

static struct platform_driver max77525_ipc_driver =
{
	.driver = {
		.name = "max77525",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max77525_of_match),
	},
	.probe = max77525_ipc_probe,
	.remove = max77525_ipc_remove,
};

static int __init max77525_ipc_init(void)
{
	return platform_driver_register(&max77525_ipc_driver);
}
fs_initcall(max77525_ipc_init);

static void __exit max77525_ipc_exit(void)
{
	platform_driver_unregister(&max77525_ipc_driver);
}
module_exit(max77525_ipc_exit);

MODULE_DESCRIPTION("MAXIM 77525 multi-function core driver");
MODULE_AUTHOR("Clark Kim <clark.kim@maximintegrated.com>");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:max77525");
