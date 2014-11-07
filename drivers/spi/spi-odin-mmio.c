/*
 *  drivers/spi/spi-odin-mmio.c
 *
 * Memory-mapped interface driver for DW SPI Core
 *
 * Copyright (c) 2010, Octasic semiconductor.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include "spi-odin.h"

#define AUDIO_BLOCK    1
#define PERI_BLOCK     0

struct dw_spi_mmio {
	struct dw_spi  dws;
	struct clk     *clk;	/* core clock */
	struct clk     *pclk;	/* APB clock */
	int    block_type;      /* peri : 0, audio :1 */
};

static int dw_spi_runtime_suspend(struct device *dev);
static int dw_spi_runtime_resume(struct device *dev);

static int __init dw_spi_mmio_probe(struct platform_device *pdev)
{
	struct dw_spi_mmio *dwsmmio;
	struct dw_spi *dws;
	struct resource *mem, *ioarea;
	int ret;
	const char *pclk_name;
	u32 enable_rpm;

	dwsmmio = kzalloc(sizeof(struct dw_spi_mmio), GFP_KERNEL);
	if (!dwsmmio) {
		ret = -ENOMEM;
		goto err_end;
	}

	dws = &dwsmmio->dws;

	/* Get basic io resource and map it */
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource?\n");
		ret = -EINVAL;
		goto err_kfree;
	}

	if ((u32)0x34000000 < (u32)mem->start)
		dwsmmio->block_type = AUDIO_BLOCK;
	else
		dwsmmio->block_type = PERI_BLOCK;

	ioarea = request_mem_region(mem->start, resource_size(mem),
			pdev->name);
	if (!ioarea) {
		dev_err(&pdev->dev, "SPI region already claimed\n");
		ret = -EBUSY;
		goto err_kfree;
	}

	dws->regs = ioremap_nocache(mem->start, resource_size(mem));
	if (!dws->regs) {
		dev_err(&pdev->dev, "SPI region already mapped\n");
		ret = -ENOMEM;
		goto err_release_reg;
	}

	dws->irq = platform_get_irq(pdev, 0);
	if (dws->irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		ret = dws->irq; /* -ENXIO */
		goto err_unmap;
	}
	/* PERI SPI APB clock */
	if (!of_property_read_string(pdev->dev.of_node, "pclk_name",
		&pclk_name)) {
		dwsmmio->pclk = clk_get(NULL, pclk_name);
		if (IS_ERR(dwsmmio->pclk)) {
			dev_err(&pdev->dev, "can't find APB clock\n");
			goto err_irq;
		}
		dev_dbg(&pdev->dev, "found APB clock\n");
		clk_prepare_enable(dwsmmio->pclk);
	}

	/* PERI SPI core clock */
	dwsmmio->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(dwsmmio->clk)) {
		ret = PTR_ERR(dwsmmio->clk);
		goto err_clk;
	}
	clk_prepare_enable(dwsmmio->clk);

	dws->parent_dev = &pdev->dev;
#if CONFIG_OF
	of_property_read_u32(pdev->dev.of_node, "id",&pdev->id);
#endif
	dws->bus_num = pdev->id;
	dws->num_cs = 4;
	dws->max_freq = clk_get_rate(dwsmmio->clk);

	ret = dw_spi_add_host(dws);
	if (ret)
		goto err_clk;

	platform_set_drvdata(pdev, dwsmmio);

	if (dwsmmio->block_type != AUDIO_BLOCK){
		clk_disable_unprepare(dwsmmio->clk);
		if (dwsmmio->pclk)
			clk_disable_unprepare(dwsmmio->pclk);
	}else{
		return 0;
	}

	enable_rpm = 0;

	of_property_read_u32(pdev->dev.of_node, "enable_rpm", &enable_rpm);
	if (!enable_rpm) {
		dev_dbg(&pdev->dev, "disable runtime-pm\n");
		return 0;
	}

	clk_disable_unprepare(dwsmmio->clk);

	if (dwsmmio->pclk)
		clk_disable_unprepare(dwsmmio->pclk);

	dev_dbg(&pdev->dev, "enable runtime-pm\n");
	pm_runtime_set_autosuspend_delay(&pdev->dev, 1000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_irq_safe(&pdev->dev);

	return 0;

err_clk:
	if (dwsmmio->clk) {
		clk_disable_unprepare(dwsmmio->clk);
		clk_put(dwsmmio->clk);
		dwsmmio->clk = NULL;
	}
	if (dwsmmio->pclk) {
		clk_disable_unprepare(dwsmmio->pclk);
		clk_put(dwsmmio->pclk);
		dwsmmio->pclk = NULL;
	}
err_irq:
	free_irq(dws->irq, dws);
err_unmap:
	iounmap(dws->regs);
err_release_reg:
	release_mem_region(mem->start, resource_size(mem));
err_kfree:
	kfree(dwsmmio);
err_end:
	return ret;
}

static int __exit dw_spi_mmio_remove(struct platform_device *pdev)
{
	struct dw_spi_mmio *dwsmmio = platform_get_drvdata(pdev);
	struct resource *mem;

	if (dwsmmio->block_type !=1)
	{
		if (!pm_runtime_suspended(dwsmmio->dws.parent_dev))
			dw_spi_runtime_suspend(dwsmmio->dws.parent_dev);
		pm_runtime_disable(dwsmmio->dws.parent_dev);

		clk_unprepare(dwsmmio->clk);
		if (dwsmmio->pclk)
			clk_unprepare(dwsmmio->pclk);
	}
	platform_set_drvdata(pdev, NULL);

	free_irq(dwsmmio->dws.irq, &dwsmmio->dws);
	dw_spi_remove_host(&dwsmmio->dws);
	iounmap(dwsmmio->dws.regs);
	kfree(dwsmmio);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem->start, resource_size(mem));
	return 0;
}

#ifdef CONFIG_PM
static int dw_spi_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_spi_mmio *dwsmmio = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);

	dw_spi_suspend_host(&dwsmmio->dws);

	if (!pm_runtime_enabled(dev) || dwsmmio->block_type == AUDIO_BLOCK  ) {
		clk_disable_unprepare(dwsmmio->clk);
		if (dwsmmio->pclk)
			clk_disable_unprepare(dwsmmio->pclk);
	}

	return 0;
}

static int dw_spi_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_spi_mmio *dwsmmio = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);
	if (dwsmmio->pclk)
		clk_prepare_enable(dwsmmio->pclk);
	clk_prepare_enable(dwsmmio->clk);

	dw_spi_resume_host(&dwsmmio->dws);

	if (pm_runtime_enabled(dev) && (dwsmmio->block_type == PERI_BLOCK)) {
		clk_disable_unprepare(dwsmmio->clk);
		if (dwsmmio->pclk)
			clk_disable_unprepare(dwsmmio->pclk);
	}

	return 0;
}
#endif

static int dw_spi_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_spi_mmio *dwsmmio = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);

	clk_disable_unprepare(dwsmmio->clk);
	if (dwsmmio->pclk)
		clk_disable_unprepare(dwsmmio->pclk);

	return 0;
}

static int dw_spi_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_spi_mmio *dwsmmio = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);

	if (dwsmmio->pclk)
		clk_prepare_enable(dwsmmio->pclk);
	clk_prepare_enable(dwsmmio->clk);

	return 0;
}

static struct dev_pm_ops dw_spi_dev_pm_ops = {
	.suspend = dw_spi_suspend,
	.resume = dw_spi_resume,
	.runtime_suspend = dw_spi_runtime_suspend,
	.runtime_resume = dw_spi_runtime_resume
};

/* work with hotplug and coldplug */
MODULE_ALIAS("platform:spi_odin");

#ifdef CONFIG_OF
static const struct of_device_id odin_spi_dt_match[] __initdata = {
	{.compatible = "octasic,odin-spi",},
	{}
};
#endif
static struct platform_driver dw_spi_mmio_driver = {
	.probe		= dw_spi_mmio_probe,
	.remove		= __exit_p(dw_spi_mmio_remove),
	.driver		= {
		.name	= "spi_odin",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(odin_spi_dt_match),
		.pm = &dw_spi_dev_pm_ops,
	},
};
module_platform_driver(dw_spi_mmio_driver);

MODULE_AUTHOR("Jean-Hugues Deschenes <jean-hugues.deschenes@octasic.com>");
MODULE_DESCRIPTION("Memory-mapped I/O interface driver for DW SPI Core");
MODULE_LICENSE("GPL v2");
