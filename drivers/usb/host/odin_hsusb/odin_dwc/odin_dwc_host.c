/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "dwc_otg_core_if.h"
#include "dwc_otg_cil.h"

#include "odin_dwc_host.h"

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <asm/io.h>

#if defined(CONFIG_IMC_LOG_OFF)
int odin_dwc_dbg_mask = 0;
#elif defined(ODIN_DWC_HOST_DEBUG)
int odin_dwc_dbg_mask = (ODIN_DWC_UERR | ODIN_DWC_UINFO | ODIN_DWC_UDBG);
#else
int odin_dwc_dbg_mask = (ODIN_DWC_UERR | ODIN_DWC_UINFO);
#endif

void odin_dwc_host_reset(odin_dwc_ddata_t *ddata, bool dbg)
{
	void __iomem *base, *reset_base;
	u32 val, i;

	base = ddata->odin_dev.crg_base;
	reset_base = base + ddata->odin_dev.reset_offset;

	val = readl(reset_base);
	writel((val & ~ddata->odin_dev.reset_mask), reset_base);
	udelay(10);

	val = readl(reset_base);
	writel((val | ddata->odin_dev.phy_rst_val), reset_base);
	udelay(40);

	val = readl(reset_base);
	writel((val | ddata->odin_dev.core_rst_val), reset_base);

	val = readl(reset_base);
	writel((val | ddata->odin_dev.bus_rst_val), reset_base);

	if (dbg) {
		for (i = 0; i <= (0x2c8/4); i++) {
			val = readl(base + (i*4));
			udbg(UDBG, "(%d) 0x3ff000%02x = 0x%08x\n", i, i*4, val);
		}
	}
}

static void odin_dwc_host_init_work(struct work_struct *work)
{
	odin_dwc_ddata_t *ddata =
		container_of(work, struct odin_dwc_drvdata, init_work.work);
	struct platform_device *pdev = ddata->pdev;

	switch (ddata->state) {
	case HOST_CORE_INIT1:
		udbg(UDBG, "Init work: HOST_CORE_INIT1\n");
		dwc_otg_disable_global_interrupts(ddata->dwc_dev.core_if);
		odin_dwc_host_core_init1(ddata->dwc_dev.core_if);
		ddata->state = HOST_CORE_INIT2;
		schedule_delayed_work(&ddata->init_work, msecs_to_jiffies(100));
		break;
	case HOST_CORE_INIT2:
		udbg(UDBG, "Init work: HOST_CORE_INIT2\n");
		odin_dwc_host_core_init2(ddata->dwc_dev.core_if);
		odin_dwc_host_set_host_mode(ddata->dwc_dev.core_if);
		ddata->state = HCD_INIT;
		schedule_delayed_work(&ddata->init_work, msecs_to_jiffies(30));
		break;
	case HCD_INIT:
		udbg(UDBG, "Init work: : HCD_INIT\n");
		odin_dwc_host_post_set_param(ddata->dwc_dev.core_if);
		if (odin_dwc_host_hcd_init(&pdev->dev, ddata)) {
			udbg(UERR, "failed to initialize hcd\n");
			ddata->dwc_dev.hcd = NULL;
			ddata->state = INIT_FAILED;
		} else {
			dwc_otg_enable_global_interrupts(ddata->dwc_dev.core_if);
			dwc_otg_set_hsic_connect(ddata->dwc_dev.core_if, 0);

			platform_set_drvdata(pdev, ddata);

			if (odin_dwc_host_create_attr(pdev))
				udbg(UERR, "failed to create attr\n");

			ddata->b_state = DISCONNECT;
			ddata->state = INIT_DONE;
			udbg(UINFO, "Init work: Done ...\n");
		}
		break;
	default:
		udbg(UERR, "Init work: Unknown state (%d)\n", ddata->state);
		break;
	}
}

int odin_dwc_host_clk_enable(odin_dwc_ddata_t *ddata)
{
	int ret;

	ret = clk_prepare_enable(ddata->odin_dev.bus_clk);
	if (ret) {
		udbg(UERR, "failed to enable bus clk (%d)\n", ret);
		goto bus_clk_err;
	}

	ret = clk_prepare_enable(ddata->odin_dev.refclkdig);
	if (ret) {
		udbg(UERR, "failed to enable ref clk (%d)\n", ret);
		goto ref_clk_err;
	}

	ret = clk_prepare_enable(ddata->odin_dev.phy_clk);
	if (ret) {
		udbg(UERR, "failed to enable phy clk (%d)\n", ret);
		goto phy_clk_err;
	}

	return ret;

phy_clk_err:
	clk_disable_unprepare(ddata->odin_dev.refclkdig);
ref_clk_err:
	clk_disable_unprepare(ddata->odin_dev.bus_clk);
bus_clk_err:

	return ret;
}

void odin_dwc_host_clk_disable(odin_dwc_ddata_t *ddata)
{
	clk_disable_unprepare(ddata->odin_dev.phy_clk);
	clk_disable_unprepare(ddata->odin_dev.refclkdig);
	clk_disable_unprepare(ddata->odin_dev.bus_clk);
}

static void odin_dwc_host_clk_remove(odin_dwc_ddata_t *ddata)
{
	if (ddata->odin_dev.phy_clk) {
		clk_disable_unprepare(ddata->odin_dev.phy_clk);
		clk_put(ddata->odin_dev.phy_clk);
	}

	if (ddata->odin_dev.refclkdig) {
		clk_disable_unprepare(ddata->odin_dev.refclkdig);
		clk_put(ddata->odin_dev.refclkdig);
	}

	if (ddata->odin_dev.bus_clk) {
		clk_disable_unprepare(ddata->odin_dev.bus_clk);
		clk_put(ddata->odin_dev.bus_clk);
	}
}

static int odin_dwc_host_clk_init(odin_dwc_ddata_t *ddata)
{
	int ret;

	ddata->odin_dev.bus_clk = clk_get(NULL, "usbhsic_bus_clk");
	if (IS_ERR(ddata->odin_dev.bus_clk)) {
		udbg(UERR, "failed to get usbhsic_bus_clk\n");
		ret = PTR_ERR(ddata->odin_dev.bus_clk);
		goto bus_clk_err;
	}

	ddata->odin_dev.refclkdig = clk_get(NULL, "usbhsic_refdig");
	if (IS_ERR(ddata->odin_dev.refclkdig)) {
		udbg(UERR, "failed to get usbhsic_refdig\n");
		ret = PTR_ERR(ddata->odin_dev.refclkdig);
		goto ref_clk_err;
	}

	ddata->odin_dev.phy_clk = clk_get(NULL, "usbhsic_phy");
	if (IS_ERR(ddata->odin_dev.phy_clk)) {
		udbg(UERR, "failed to get usbhsic_phy\n");
		ret = PTR_ERR(ddata->odin_dev.phy_clk);
		goto phy_clk_err;
	}

	ret = odin_dwc_host_clk_enable(ddata);
	if (ret)
		goto clk_en_err;

	return ret;

clk_en_err:
	clk_put(ddata->odin_dev.phy_clk);
phy_clk_err:
	clk_put(ddata->odin_dev.refclkdig);
ref_clk_err:
	clk_put(ddata->odin_dev.bus_clk);
bus_clk_err:

	return ret;
}

int odin_dwc_host_ldo_enable(odin_dwc_ddata_t *ddata)
{
	int ret;

	ret = regulator_enable(ddata->odin_dev.io_reg);
	if (ret) {
		udbg(UERR, "failed to enable +1.2V_VDDIO_USBHSIC\n");
		regulator_put(ddata->odin_dev.io_reg);
	}

	return ret;
}

int odin_dwc_host_ldo_disable(odin_dwc_ddata_t *ddata)
{
	int ret;

	ret = regulator_disable(ddata->odin_dev.io_reg);
	if (ret)
		udbg(UERR, "failed to disable +1.2V_VDDIO_USBHSIC\n");

	return ret;
}

static void odin_dwc_host_ldo_remove(odin_dwc_ddata_t *ddata)
{
	if (!IS_ERR(ddata->odin_dev.io_reg)) {
		regulator_disable(ddata->odin_dev.io_reg);
		regulator_put(ddata->odin_dev.io_reg);
	}
}

static int odin_dwc_host_ldo_init(odin_dwc_ddata_t *ddata)
{
	int ret = 0;

	ddata->odin_dev.io_reg = regulator_get(NULL, "+1.2V_VDDIO_USBHSIC");
	if (IS_ERR(ddata->odin_dev.io_reg)) {
		udbg(UERR, "failed to get +1.2V_VDDIO_USBHSIC\n");
		ret = PTR_ERR(ddata->odin_dev.io_reg);
		return ret;
	}

	ret = odin_dwc_host_ldo_enable(ddata);

	return ret;
}

static int odin_dwc_host_remove(struct platform_device *pdev)
{
	odin_dwc_ddata_t *ddata = dev_get_drvdata(&pdev->dev);
	struct resource *res;

	udbg(UINFO, "ODIN DWC USB Host device removed\n");

	if (!ddata) {
		udbg(UERR, "data is NULL\n");
		return -ENOMEM;
	}

	usb_unregister_notify(&ddata->usb_nb);

	cancel_delayed_work_sync(&ddata->init_work);

	free_irq(ddata->odin_dev.irq, &ddata->dwc_dev);

	odin_dwc_host_hcd_remove(ddata->dwc_dev.hcd);

	if (ddata->dwc_dev.core_if)
		odin_dwc_host_cil_remove(ddata->dwc_dev.core_if);
	 else
	 	udbg(UERR, "core_if is NULL\n");

	odin_dwc_host_remove_attr(pdev);

	if (ddata->dwc_dev.base) {
		iounmap(ddata->dwc_dev.base);
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		release_mem_region(res->start, resource_size(res));
	}

	if (ddata->odin_dev.crg_base) {
		iounmap(ddata->odin_dev.crg_base);
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		release_mem_region(res->start, resource_size(res));
	}

	odin_dwc_host_clk_remove(ddata);
	odin_dwc_host_ldo_remove(ddata);

	platform_set_drvdata(pdev, NULL);

	kfree(ddata);

	return 0;
}

static int odin_dwc_get_dev_info(struct platform_device *pdev,
			odin_dwc_ddata_t *ddata)
{
	int ret = -1;

	if (of_property_read_u32(pdev->dev.of_node, "reset_offset",
			&ddata->odin_dev.reset_offset)) {
		udbg(UERR, "failed to get reset_offset (%d)\n", ret);
	} else if (of_property_read_u32(pdev->dev.of_node, "reset_mask",
			&ddata->odin_dev.reset_mask)) {
		udbg(UERR, "failed to get reset_mask (%d)\n", ret);
	} else if ( of_property_read_u32(pdev->dev.of_node, "phy_rst_val",
			&ddata->odin_dev.phy_rst_val)) {
		udbg(UERR, "failed to get phy_rst_val (%d)\n", ret);
	} else if (of_property_read_u32(pdev->dev.of_node, "core_rst_val",
			&ddata->odin_dev.core_rst_val)) {
		udbg(UERR, "failed to get core_rst_val (%d)\n", ret);
	} else if (of_property_read_u32(pdev->dev.of_node, "bus_rst_val",
			&ddata->odin_dev.bus_rst_val)) {
		udbg(UERR, "failed to get bus_rst_val (%d)\n", ret);
	} else {
		ret = 0;
		udbg(UDBG, "succeeded to get dev info\n");
	}

	return ret;
}

static int odin_dwc_host_probe(struct platform_device *pdev)
{
	odin_dwc_ddata_t *ddata;
	struct resource *dwc_res, *odin_res, *irq_res;
	int ret = 0;

	udbg(UINFO, "ODIN DWC USB Host device probed\n");

	ddata = kzalloc(sizeof(odin_dwc_ddata_t), GFP_KERNEL);
	if (!ddata) {
		udbg(UERR, "failed to allocate odin dwc data\n");
		return -ENOMEM;
	}

	dwc_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!dwc_res) {
		udbg(UERR, "failed to get dwc memory resource\n");
		ret = -ENXIO;
		goto free_mem;
	}

	if (!request_mem_region(dwc_res->start, resource_size(dwc_res),
			pdev->name)) {
		udbg(UERR, "failed to request memory 0 region\n");
		ret = -EBUSY;
		goto free_mem;
	}

	ddata->dwc_dev.base =
			ioremap(dwc_res->start, resource_size(dwc_res));
	if (!ddata->dwc_dev.base) {
		udbg(UERR, "ioremap for dwc physical address failed\n");
		ret = -ENOMEM;
		goto release_dwc_mem;
	}

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	ddata->pdev = pdev;

	ret = odin_dwc_host_ldo_init(ddata);
	if (ret)
		goto unmap_dwc;

	ret = odin_dwc_host_clk_init(ddata);
	if (ret)
		goto remove_ldo;

	odin_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!odin_res) {
		udbg(UERR, "failed to get memory 1 resource\n");
		ret = -ENXIO;
		goto remove_clk;
	}

	if (!request_mem_region(odin_res->start, resource_size(odin_res),
			pdev->name)) {
		udbg(UERR, "failed to request odin memory region\n");
		ret = -EBUSY;
		goto remove_clk;
	}

	ddata->odin_dev.crg_base = ioremap(odin_res->start,
			resource_size(odin_res));
	if (!ddata->odin_dev.crg_base) {
		udbg(UERR, "ioremap for odin physical address failed\n");
		ret = -ENOMEM;
		goto release_odin_mem;
	}

	if (odin_dwc_get_dev_info(pdev, ddata)) {
		ret = -EINVAL;
		goto unmap_odin;
	}

	odin_dwc_host_reset(ddata, false);

	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq_res) {
		udbg(UERR, "failed to get irq resource\n");
		goto unmap_odin;
	}

	ddata->odin_dev.irq = irq_res->start;

	ddata->dwc_dev.core_if = kzalloc(sizeof(dwc_otg_core_if_t), GFP_KERNEL);
	if (!ddata->dwc_dev.core_if) {
		udbg(UERR, "failed to allocate dwc core interface\n");
		ret = -ENOMEM;
		goto free_odin_irq;
	}

	ret = odin_dwc_host_cil_init(ddata->dwc_dev.core_if,
			ddata->dwc_dev.base);
	if (ret) {
		udbg(UERR, "failed to initialize cil (%d)\n", ret);
		goto free_mem_core_if;
	}

	spin_lock_init(&ddata->lock);

	ddata->usb_nb.notifier_call = odin_dwc_host_usb_notify;
	usb_register_notify(&ddata->usb_nb);

	INIT_DELAYED_WORK(&ddata->init_work, odin_dwc_host_init_work);
	ddata->state = HOST_CORE_INIT1;
	schedule_delayed_work(&ddata->init_work, msecs_to_jiffies(0));

	return ret;

free_mem_core_if:
	kfree(ddata->dwc_dev.core_if);
free_odin_irq:
	free_irq(ddata->odin_dev.irq, &ddata->dwc_dev);
unmap_odin:
	iounmap(ddata->odin_dev.crg_base);
release_odin_mem:
	release_mem_region(odin_res->start, resource_size(odin_res));
remove_clk:
	odin_dwc_host_clk_remove(ddata);
remove_ldo:
	odin_dwc_host_ldo_remove(ddata);
unmap_dwc:
	iounmap(ddata->dwc_dev.base);
release_dwc_mem:
	release_mem_region(dwc_res->start, resource_size(dwc_res));
free_mem:
	kfree(ddata);

	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id odin_dwc_host_match[] = {
	{ .compatible = "odin_dwc_host"},
	{},
};
#endif

static struct platform_driver odin_dwc_host_driver = {
	.probe		= odin_dwc_host_probe,
	.remove		= odin_dwc_host_remove,
	.driver		= {
		.name	= "odin_dwc_host",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(odin_dwc_host_match),
	},
};
module_platform_driver(odin_dwc_host_driver);

MODULE_AUTHOR("Jinjong Lee <jinjong.lee@lge.com>");
MODULE_DESCRIPTION("DWC OTG USB Host driver");
MODULE_LICENSE("GPL");
