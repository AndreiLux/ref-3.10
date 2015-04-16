/*
 * Hisilicon DDR TEST driver (master only).
 *
 * Copyright (c) 2012-2013 Linaro Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/mutex.h>


struct hi3630_ddr_dev {
	struct device		*dev;
	void __iomem		*base;
	spinlock_t 		state_lock;
	struct clk		*clk;
	resource_size_t         mapbase;
	int			irq;
	struct device           *platform_dev;
	struct work_struct	dump_work;
};

#define REG_AXI_INT_STATUS	(0x610)
#define REG_SEC_INT_EN		(0x1020)
#define REG_SEC_INT_STATUS	(0x1024)
#define REG_SEC_INT_CLEAR	(0x1028)
#define REG_QOSB_INTMSK		(0x4170)
#define REG_QOSB_RINT		(0x4174)
#define REG_QOSB_INTSTS		(0x4178)
#define REG_DDRC_INTMSK_DMC0	(0x8280)
#define REG_DDRC_RINT_DMC0	(0x8284)
#define REG_DDRC_INTSTS_DMC0	(0X8288)
#define REG_DDRC_INTSTS_DMC1	(0X9288)
#define REG_DDRC_INTMSK_DMC1	(0x9280)
#define REG_DDRC_RINT_DMC1	(0x9284)
#define REG_AXI_CFG_LOCK	(0x0004)
#define REG_SEC_FAIL_CMD_INF_0	(0x1080)
#define REG_SEC_FAIL_CMD_INF_1	(0x1084)
#define REG_SEC_FAIL_CMD_INF_2	(0x1088)

void parse_MasterID(u32 cmd, int port)
{
	u32 mid = 0;

	mid = ((cmd & 0xff000000) >> 24);

	if ((port == 2) || (port == 3)) {
		printk("port is 0x%x, mid is 0x%x\n", port, mid);
		if (mid < 0x10)
			printk("Master is DSS\n");
		else if (mid < 0x14)
			printk("Master is ISP\n");
		else
			printk("unknow Master\n");

		return;
	}

	switch(mid) {
	case 0x0 :
		printk("MasterID is 0x%x\n", mid);
		printk("Master is LP_M3\n");
		break;
	case 0x1:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is IO_M3_Cache\n");
		break;
	case 0x2:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is USB2\n");
		break;
	case 0x3:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is MODEM_SUBSYS\n");
		break;
	case 0x5:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is D_JTAG\n");
		break;
	case 0x6:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is EMMC\n");
		break;
	case 0x8:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is SD3\n");
		break;
	case 0x9:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is SDIO0\n");
		break;
	case 0xa:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is SEC_P\n");
		break;
	case 0xb:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is SEC_S\n");
		break;
	case 0xc:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is SOCP\n");
		break;
	case 0xd:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is HSIC\n");
		break;
	case 0xe:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is TOP_CSSYS\n");
		break;
	case 0xf:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is DMAC\n");
		break;
	case 0x10:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is G3D_Normal\n");
		break;
	case 0x11:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is G3D_writeTileBuffer\n");
		break;
	case 0x14:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is VENC\n");
		break;
	case 0x15:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is VDEC\n");
		break;
	case 0x16:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is VPP\n");
		break;
	case 0x17:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is JPU\n");
		break;
	case 0x18:
		if (port == 0) {
			printk("MasterID is 0x%x\n", mid);
			printk("Master is Modem_A9\n");
		} else {
			printk("MasterID is 0x%x\n", mid);
			printk("Master is A15\n");
		}
		break;
	case 0x19:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is A7\n");
		break;
	case 0x1d:
		printk("MasterID is 0x%x\n", mid);
		printk("Master is AUDIO_SYBSYS\n");
		break;
	default:
		printk("MasterID is 0x%x\n", mid);
		printk("unknow Master\n");
		break;
	}
	return;
}

static void dump_work(struct hi3630_ddr_dev *dev)
{
	u32 axi_int_status = 0;
	u32 sec_int_en = 0;
	u32 sec_int_status = 0;
	u32 qosb_intsts = 0;
	u32 ddrc_intsts_dmc0 = 0;
	u32 ddrc_intsts_dmc1 = 0;
	u32 sec_fail_cmd_inf_0 = 0;
	u32 sec_fail_cmd_inf_1 = 0;
	u32 sec_fail_cmd_inf_2 = 0;
	int sec_port_num = 0;
	u32 sec_port_base_addr = 0;

	axi_int_status = readl(dev->base + REG_AXI_INT_STATUS);
	qosb_intsts = readl(dev->base + REG_QOSB_INTSTS);
	ddrc_intsts_dmc0 = readl(dev->base + REG_DDRC_INTSTS_DMC0);
	ddrc_intsts_dmc1 = readl(dev->base + REG_DDRC_INTSTS_DMC1);

	printk("====================dump ddr register begin========================\n");

	printk("REG_AXI_INT_STATUS is 0x%x\n", axi_int_status);
	printk("REG_QOSB_INTSTS    is 0x%x\n", qosb_intsts);
	printk("REG_DDRC_INTSTS_DMC0    is 0x%x\n", ddrc_intsts_dmc0);
	printk("REG_DDRC_INTSTS_DMC1    is 0x%x\n", ddrc_intsts_dmc1);

	if (axi_int_status != 0) {
		while (axi_int_status) {
			axi_int_status = axi_int_status/2;
			sec_port_num++;
		}

		sec_port_num = sec_port_num - 1;

		sec_port_base_addr = 0x400 * sec_port_num;
		printk("the error port is 0x%x\n", sec_port_num);

		sec_int_en = readl(dev->base + sec_port_base_addr + REG_SEC_INT_EN);
		sec_int_status = readl(dev->base + sec_port_base_addr + REG_SEC_INT_STATUS);

		printk("REG_SEC_INT_EN    is 0x%x\n", sec_int_en);
		printk("REG_SEC_INT_STATUS    is 0x%x\n", sec_int_status);

		sec_fail_cmd_inf_0 = readl(dev->base + sec_port_base_addr + REG_SEC_FAIL_CMD_INF_0);
		sec_fail_cmd_inf_1 = readl(dev->base + sec_port_base_addr + REG_SEC_FAIL_CMD_INF_1);
		sec_fail_cmd_inf_2 = readl(dev->base + sec_port_base_addr + REG_SEC_FAIL_CMD_INF_2);
		printk("the fail command0(low 32 address)  is 0x%x\n", sec_fail_cmd_inf_0);
		printk("the fail command1(error type)	   is 0x%x\n", sec_fail_cmd_inf_1);
		printk("the fail command1(error type)      parse:\n");
		if (sec_fail_cmd_inf_1 & 0x10000)
			printk("access is privileged\n");
		else
			printk("access is unprivileged\n");

		if (sec_fail_cmd_inf_1 & 0x20000)
			printk("access is non-secure\n");
		else
			printk("access is secure\n");

		if (sec_fail_cmd_inf_1 & 0x100000)
			printk("write operation\n");
		else
			printk("read operation\n");

		printk("the fail command2(MasterID) 	   is 0x%x\n", sec_fail_cmd_inf_2);
		printk("the Master is :\n");

		parse_MasterID(sec_fail_cmd_inf_2, sec_port_num);
	}

	printk("====================dump ddr register end==========================\n");

	writel(0x0, dev->base + sec_port_base_addr + REG_SEC_INT_CLEAR);
	writel(0x7f,dev->base + REG_QOSB_RINT);
	writel(0xffffffff, dev->base + REG_DDRC_RINT_DMC0);
	writel(0xffffffff, dev->base + REG_DDRC_RINT_DMC1);
}

static irqreturn_t ddr_isr(int this_irq, void *dev_id)
{
	struct hi3630_ddr_dev *dev = dev_id;
	unsigned long flags;

	spin_lock_irqsave(&dev->state_lock, flags);
	printk("enter ddr_isr++++++\n");
	dump_work(dev);
	printk("enter ddr_isr------\n");
	spin_unlock_irqrestore(&dev->state_lock, flags);

	return IRQ_HANDLED;
}

static int hs_ddr_test_probe(struct platform_device *pdev)
{
	struct hi3630_ddr_dev *d;
	struct resource *res = NULL;
	int ret = 0;

	d = devm_kzalloc(&pdev->dev, sizeof(struct hi3630_ddr_dev), GFP_KERNEL);
	if (!d) {
		dev_err(&pdev->dev, "mem alloc failed for ddr data\n");
		return -ENOMEM;
	}

	/* NOTE: driver uses the static register mapping */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	d->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!d->base)
		return -EADDRNOTAVAIL;

	d->platform_dev = &pdev->dev;

	spin_lock_init(&d->state_lock);

	d->irq = platform_get_irq(pdev, 0);
	if (d->irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		return d->irq; /* -ENXIO */
	}

	ret = devm_request_irq(&pdev->dev, d->irq, ddr_isr, 0x4, pdev->name, d);
	if (ret) {
		dev_err(&pdev->dev, "failure requesting irq %i\n", d->irq);
		return -EINVAL;
	}

	dump_work(d);

	return ret;
}

static int hs_ddr_test_remove(struct platform_device *pdev)
{
	struct hi3630_ddr_dev *d = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	put_device(&pdev->dev);
	clk_disable_unprepare(d->clk);
	devm_clk_put(&pdev->dev, d->clk);
	d->clk = NULL;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id hs_ddr_of_match[] = {
	{ .compatible = "hisilicon,hi3630-ddr", },
	{},
};
MODULE_DEVICE_TABLE(of, hs_ddr_of_match);
#endif

static struct platform_driver hs_ddr_test_driver = {
	.probe		= hs_ddr_test_probe,
	.remove		= hs_ddr_test_remove,
	.driver		= {
		.name	= "hi3630-ddr",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(hs_ddr_of_match),
	},
};
module_platform_driver(hs_ddr_test_driver);

MODULE_DESCRIPTION("hisi hi3630 ddr test driver");
MODULE_ALIAS("platform:hi3630-ddr");
MODULE_LICENSE("GPL");
