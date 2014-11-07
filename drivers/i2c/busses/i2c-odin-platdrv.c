/*
 * drivers/i2c/busses/i2c-odin-platdrv.c
 *
 * Synopsys DesignWare I2C adapter driver (master only).
 *
 * Based on the TI DAVINCI I2C adapter driver.
 *
 * Copyright (C) 2006 Texas Instruments.
 * Copyright (C) 2007 MontaVista Software Inc.
 * Copyright (C) 2009 Provigent Ltd.
 *
 * ----------------------------------------------------------------------------
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
 * ----------------------------------------------------------------------------
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/of_i2c.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/odin_pm_domain.h>
#include "i2c-odin-core.h"

#define PERI_BLOCK  0xFFFF

static struct i2c_algorithm i2c_dw_algo = {
	.master_xfer	= i2c_dw_xfer,
	.functionality	= i2c_dw_func,
};

static int dw_i2c_runtime_suspend(struct device *dev);
static int dw_i2c_runtime_resume(struct device *dev);

static u32 i2c_dw_get_clk_rate_khz(struct dw_i2c_dev *dev)
{
	return clk_get_rate(dev->clk)/1000;
}

static int __init dw_i2c_probe(struct platform_device *pdev)
{
	struct dw_i2c_dev *dev;
	struct i2c_adapter *adap;
	struct resource *mem, *ioarea;
	int irq, r;
	u32 enable_rpm;
	const char *pclk_name;

	/* NOTE: driver uses the static register mapping */
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource?\n");
		return -EINVAL;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		return irq; /* -ENXIO */
	}

	ioarea = request_mem_region(mem->start, resource_size(mem),
			pdev->name);
	if (!ioarea) {
		dev_err(&pdev->dev, "I2C region already claimed\n");
		return -EBUSY;
	}

	dev = kzalloc(sizeof(struct dw_i2c_dev), GFP_KERNEL);
	if (!dev) {
		r = -ENOMEM;
		goto err_release_region;
	}

	init_completion(&dev->cmd_complete);
	mutex_init(&dev->lock);
	dev->dev = get_device(&pdev->dev);
	dev->irq = irq;
	platform_set_drvdata(pdev, dev);

	/* PERI I2C APB clock */
	if (!of_property_read_string(pdev->dev.of_node, "pclk_name",
		&pclk_name)) {
		dev->pclk = clk_get(NULL, pclk_name);
		if (IS_ERR(dev->pclk)) {
			dev_err(&pdev->dev, "can't find APB clock\n");
			goto err_free_mem;
		}
		dev_dbg(&pdev->dev, "found APB clock\n");
		clk_prepare_enable(dev->pclk);
	}

	/* PERI/AUD I2C core clock */
	dev->clk = clk_get(&pdev->dev, NULL);
	dev->get_clk_rate_khz = i2c_dw_get_clk_rate_khz;
	if (IS_ERR(dev->clk)) {
		r = -ENODEV;
		goto err_unuse_clocks;
	}
	clk_prepare_enable(dev->clk);

	dev->functionality =
		I2C_FUNC_I2C |
		I2C_FUNC_10BIT_ADDR |
		I2C_FUNC_SMBUS_BYTE |
		I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA |
		I2C_FUNC_SMBUS_I2C_BLOCK;
	dev->master_cfg =  DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE |
		DW_IC_CON_RESTART_EN | DW_IC_CON_SPEED_FAST;

	dev->base = ioremap(mem->start, resource_size(mem));
	if (dev->base == NULL) {
		dev_err(&pdev->dev, "failure mapping io resources\n");
		r = -EBUSY;
		goto err_unuse_clocks;
	}
	{
		u32 param1 = i2c_dw_read_comp_param(dev);

		dev->tx_fifo_depth = ((param1 >> 16) & 0xff) + 1;
		dev->rx_fifo_depth = ((param1 >> 8)  & 0xff) + 1;
	}
	r = i2c_dw_init(dev);
	if (r)
		goto err_iounmap;

	r = request_irq(dev->irq, i2c_dw_isr, IRQF_DISABLED, pdev->name, dev);
	if (r) {
		dev_err(&pdev->dev, "failure requesting irq %i\n", dev->irq);
		goto err_iounmap;
	}

	adap = &dev->adapter;
	i2c_set_adapdata(adap, dev);
	adap->owner = THIS_MODULE;
	adap->class = I2C_CLASS_HWMON;
	strlcpy(adap->name, "Synopsys DesignWare I2C adapter",
			sizeof(adap->name));
	adap->algo = &i2c_dw_algo;
	adap->dev.parent = &pdev->dev;
	adap->dev.of_node = pdev->dev.of_node;

#if CONFIG_OF
	of_property_read_u32(pdev->dev.of_node, "id",&pdev->id);
#endif
	adap->nr = pdev->id;
	r = i2c_add_numbered_adapter(adap);
	if (r) {
		dev_err(&pdev->dev, "failure adding adapter\n");
		goto err_free_irq;
	}
	of_i2c_register_devices(adap);


	if (mem->start == 0x34620000)
		dev->audio_id = 0;
	else if (mem->start == 0x34621000)
		dev->audio_id = 1;
	else
		dev->audio_id = PERI_BLOCK;

	if (dev->audio_id != PERI_BLOCK)
		return 0;

	enable_rpm = 0;
	/* FIXME: If we don't have related device node? */
	of_property_read_u32(pdev->dev.of_node, "enable_rpm", &enable_rpm);
	if (!enable_rpm) {
		dev_dbg(&pdev->dev, "disable runtime-pm\n");
		return 0;
	}

	clk_disable_unprepare(dev->clk);
	if (dev->pclk)
		clk_disable_unprepare(dev->pclk);
	/*
	 * SMS enabled PERI/AUD I2C clocks because LK uses I2C without
	 * controlling clocks.
	 * Disable clocks again to reset reference counts of the clocks to 0.
	 */
#ifdef CONFIG_AUD_RPM
	/* FIXME */
	if (mem->start != 0x34620000) {
#endif
	clk_disable_unprepare(dev->clk);
	if (dev->pclk)
		clk_disable_unprepare(dev->pclk);
#ifdef CONFIG_AUD_RPM
	}
#endif

#ifdef CONFIG_AUD_RPM
	/* FIXME */
	if (mem->start == 0x34620000)
		if (odin_pd_register_dev(&pdev->dev, &odin_pd_aud2_i2c) < 0)
			goto err_free_irq;
#endif

	dev_dbg(&pdev->dev, "enable runtime-pm\n");
	pm_runtime_set_autosuspend_delay(&pdev->dev, 3000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;

err_free_irq:
	free_irq(dev->irq, dev);
err_iounmap:
	iounmap(dev->base);
err_unuse_clocks:
	if (dev->clk) {
		clk_disable_unprepare(dev->clk);
		clk_put(dev->clk);
		dev->clk = NULL;
	}
	if (dev->pclk) {
		clk_disable_unprepare(dev->pclk);
		clk_put(dev->pclk);
		dev->pclk = NULL;
	}
err_free_mem:
	platform_set_drvdata(pdev, NULL);
	put_device(&pdev->dev);
	kfree(dev);
err_release_region:
	release_mem_region(mem->start, resource_size(mem));

	return r;
}

static int __exit dw_i2c_remove(struct platform_device *pdev)
{
	struct dw_i2c_dev *dev = platform_get_drvdata(pdev);
	struct resource *mem;

	if (!pm_runtime_suspended(&pdev->dev))
		dw_i2c_runtime_suspend(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	platform_set_drvdata(pdev, NULL);
	i2c_del_adapter(&dev->adapter);
	put_device(&pdev->dev);

	free_irq(dev->irq, dev);
	kfree(dev);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem->start, resource_size(mem));
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dw_i2c_of_match[] = {
	{ .compatible = "snps,odin-i2c", },
	{},
};
MODULE_DEVICE_TABLE(of, dw_i2c_of_match);
#endif

static int dw_i2c_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_i2c_dev *i_dev = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);

	if (!pm_runtime_enabled(dev) || i_dev->audio_id != PERI_BLOCK) {
		i2c_dw_disable(i_dev);
		clk_disable_unprepare(i_dev->clk);
		if (i_dev->pclk)
			clk_disable_unprepare(i_dev->pclk);
	}

	return 0;
}

static int dw_i2c_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_i2c_dev *i_dev = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);

	if (!pm_runtime_enabled(dev) || i_dev->audio_id != PERI_BLOCK) {
		if (i_dev->pclk)
			clk_prepare_enable(i_dev->pclk);
		clk_prepare_enable(i_dev->clk);
		i2c_dw_init(i_dev);
	}

	return 0;
}

static int dw_i2c_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_i2c_dev *i_dev = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);

	i2c_dw_disable(i_dev);
	clk_disable_unprepare(i_dev->clk);
	if (i_dev->pclk)
		clk_disable_unprepare(i_dev->pclk);

	return 0;
}

static int dw_i2c_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_i2c_dev *i_dev = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);

	if (i_dev->pclk)
		clk_prepare_enable(i_dev->pclk);
	clk_prepare_enable(i_dev->clk);
	i2c_dw_init(i_dev);

	return 0;
}

static struct dev_pm_ops dw_i2c_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dw_i2c_suspend,
		dw_i2c_resume)
	SET_RUNTIME_PM_OPS(dw_i2c_runtime_suspend,
		dw_i2c_runtime_resume, NULL)
};

/* work with hotplug and coldplug */
MODULE_ALIAS("platform:i2c_odin");

static struct platform_driver dw_i2c_driver = {
	.remove		= __exit_p(dw_i2c_remove),
	.driver		= {
		.name	= "i2c_odin",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(dw_i2c_of_match),
		.pm	= &dw_i2c_dev_pm_ops,
	},
};

static int __init dw_i2c_init_driver(void)
{
	return platform_driver_probe(&dw_i2c_driver, dw_i2c_probe);
}
subsys_initcall(dw_i2c_init_driver);

static void __exit dw_i2c_exit_driver(void)
{
	platform_driver_unregister(&dw_i2c_driver);
}
module_exit(dw_i2c_exit_driver);

MODULE_AUTHOR("Baruch Siach <baruch@tkos.co.il>");
MODULE_DESCRIPTION("Synopsys DesignWare I2C bus adapter");
MODULE_LICENSE("GPL");
