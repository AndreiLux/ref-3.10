/*
 * platform/tegra/fiq_debugger.c
 *
 * Serial Debugger Interface for Tegra
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdarg.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/serial_reg.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>
#include <linux/irqchip/tegra.h>
#include <linux/tegra_fiq_debugger.h>
#include <linux/trusty/trusty.h>
#include <linux/trusty/smcall.h>

#include "../../../drivers/staging/android/fiq_debugger/fiq_debugger.h"

#include <linux/uaccess.h>

struct tegra_fiq_debugger {
	struct fiq_debugger_pdata pdata;
	void __iomem *debug_port_base;
	bool break_seen;
	struct device *trusty_dev;
};

static inline void tegra_write(struct tegra_fiq_debugger *t,
	unsigned int val, unsigned int off)
{
	__raw_writeb(val, t->debug_port_base + off * 4);
}

static inline unsigned int tegra_read(struct tegra_fiq_debugger *t,
	unsigned int off)
{
	return __raw_readb(t->debug_port_base + off * 4);
}

static inline unsigned int tegra_read_lsr(struct tegra_fiq_debugger *t)
{
	unsigned int lsr;

	lsr = tegra_read(t, UART_LSR);
	if (lsr & UART_LSR_BI)
		t->break_seen = true;

	return lsr;
}

static int debug_port_init(struct platform_device *pdev)
{
	struct tegra_fiq_debugger *t;
	t = container_of(dev_get_platdata(&pdev->dev), typeof(*t), pdata);

	/* enable and clear FIFO */
	tegra_write(t, UART_FCR_ENABLE_FIFO, UART_FCR);
	tegra_write(t, UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR |
				UART_FCR_CLEAR_XMIT, UART_FCR);
	tegra_write(t, 0, UART_FCR);
	tegra_write(t, UART_FCR_ENABLE_FIFO, UART_FCR);

	/* clear LSR */
	tegra_read(t, UART_LSR);
	t->break_seen = false;

	/* enable rx interrupt */
	tegra_write(t, UART_IER_RDI, UART_IER);

	return 0;
}

static int debug_getc(struct platform_device *pdev)
{
	unsigned int lsr;
	struct tegra_fiq_debugger *t;
	t = container_of(dev_get_platdata(&pdev->dev), typeof(*t), pdata);

	lsr = tegra_read_lsr(t);

	if (lsr & UART_LSR_BI || t->break_seen) {
		t->break_seen = false;
		return FIQ_DEBUGGER_BREAK;
	}

	if (lsr & UART_LSR_DR)
		return tegra_read(t, UART_RX);

	return FIQ_DEBUGGER_NO_CHAR;
}

static void debug_putc(struct platform_device *pdev, unsigned int c)
{
	struct tegra_fiq_debugger *t;
	t = container_of(dev_get_platdata(&pdev->dev), typeof(*t), pdata);

	while (!(tegra_read_lsr(t) & UART_LSR_THRE))
		cpu_relax();

	tegra_write(t, c, UART_TX);
}

static void debug_flush(struct platform_device *pdev)
{
	struct tegra_fiq_debugger *t;
	t = container_of(dev_get_platdata(&pdev->dev), typeof(*t), pdata);

	while (!(tegra_read_lsr(t) & UART_LSR_TEMT))
		cpu_relax();
}

static int debug_suspend(struct platform_device *pdev)
{
	struct tegra_fiq_debugger *t;
	t = container_of(dev_get_platdata(&pdev->dev), typeof(*t), pdata);

	tegra_write(t, 0, UART_IER);

	return 0;
}

static int debug_resume(struct platform_device *pdev)
{
	return debug_port_init(pdev);
}


#ifdef CONFIG_FIQ
static void fiq_enable(struct platform_device *pdev, unsigned int irq, bool on)
{
	if (on)
		tegra_fiq_enable(irq);
	else
		tegra_fiq_disable(irq);
}
#endif

static void trusty_fiq_enable(struct platform_device *pdev,
				  unsigned int fiq, bool enable)
{
	int ret;
	struct tegra_fiq_debugger *t;

	t = container_of(dev_get_platdata(&pdev->dev), typeof(*t), pdata);

	ret = trusty_fast_call32(t->trusty_dev, SMC_FC_REQUEST_FIQ,
				 fiq, enable, 0);
	if (ret)
		dev_err(&pdev->dev, "SMC_FC_REQUEST_FIQ failed: %d\n", ret);
}

static int tegra_fiq_debugger_id;

static void __tegra_serial_debug_init(unsigned int base, int fiq, int irq,
			   struct clk *clk, int signal_irq, int wakeup_irq,
			   struct device *trusty_dev)
{
	struct tegra_fiq_debugger *t;
	struct platform_device *pdev;
	struct resource *res;
	int res_count;

	t = kzalloc(sizeof(struct tegra_fiq_debugger), GFP_KERNEL);
	if (!t) {
		pr_err("Failed to allocate for fiq debugger\n");
		return;
	}

	t->pdata.uart_init = debug_port_init;
	t->pdata.uart_getc = debug_getc;
	t->pdata.uart_putc = debug_putc;
	t->pdata.uart_flush = debug_flush;
	t->pdata.uart_dev_suspend = debug_suspend;
	t->pdata.uart_dev_resume = debug_resume;

#ifdef CONFIG_FIQ
	t->pdata.fiq_enable = fiq_enable;
#else
	BUG_ON(fiq >= 0 && !trusty_dev);
#endif
	if (trusty_dev)
		t->pdata.fiq_enable = trusty_fiq_enable;
	t->trusty_dev = trusty_dev;

	t->debug_port_base = ioremap(base, PAGE_SIZE);
	if (!t->debug_port_base) {
		pr_err("Failed to ioremap for fiq debugger\n");
		goto out1;
	}

	res = kzalloc(sizeof(struct resource) * 3, GFP_KERNEL);
	if (!res) {
		pr_err("Failed to alloc fiq debugger resources\n");
		goto out2;
	}

	pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	if (!pdev) {
		pr_err("Failed to alloc fiq debugger platform device\n");
		goto out3;
	};

	if (fiq >= 0) {
		res[0].flags = IORESOURCE_IRQ;
		res[0].start = fiq;
		res[0].end = fiq;
		res[0].name = "fiq";
	} else {
		res[0].flags = IORESOURCE_IRQ;
		res[0].start = irq;
		res[0].end = irq;
		res[0].name = "uart_irq";
	}

	res[1].flags = IORESOURCE_IRQ;
	res[1].start = signal_irq;
	res[1].end = signal_irq;
	res[1].name = "signal";
	res_count = 2;

	if (wakeup_irq >= 0) {
		res[2].flags = IORESOURCE_IRQ;
		res[2].start = wakeup_irq;
		res[2].end = wakeup_irq;
		res[2].name = "wakeup";
		res_count++;
	}

	pdev->name = "fiq_debugger";
	pdev->id = tegra_fiq_debugger_id++;
	pdev->dev.platform_data = &t->pdata;
	pdev->resource = res;
	pdev->num_resources = res_count;

	if (platform_device_register(pdev)) {
		pr_err("Failed to register fiq debugger\n");
		goto out4;
	}

	return;

out4:
	kfree(pdev);
out3:
	kfree(res);
out2:
	iounmap(t->debug_port_base);
out1:
	kfree(t);
}

#ifdef CONFIG_FIQ
void tegra_serial_debug_init(unsigned int base, int fiq,
			   struct clk *clk, int signal_irq, int wakeup_irq)
{
	__tegra_serial_debug_init(base, fiq, -1, clk, signal_irq,
				  wakeup_irq, NULL);
}
#endif

void tegra_serial_debug_init_irq_mode(unsigned int base, int irq,
			   struct clk *clk, int signal_irq, int wakeup_irq)
{
	__tegra_serial_debug_init(base, -1, irq, clk, signal_irq,
				  wakeup_irq, NULL);
}

static int tegra_serial_debug_trusty_probe(struct platform_device *pdev)
{
	struct resource *res;
	int fiq;
	int signal_irq;
	int wakeup_irq;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No mem resource\n");
		return -EINVAL;
	}

	fiq = platform_get_irq_byname(pdev, "fiq");
	if (fiq < 0) {
		dev_err(&pdev->dev, "No IRQ for fiq, error=%d\n", fiq);
		return fiq;
	}

	signal_irq = platform_get_irq_byname(pdev, "signal");
	if (signal_irq < 0) {
		dev_err(&pdev->dev, "No signal IRQ, error=%d\n", signal_irq);
		return signal_irq;
	}

	wakeup_irq = platform_get_irq_byname(pdev, "wakeup");
	if (wakeup_irq < 0)
		wakeup_irq = -1;

	__tegra_serial_debug_init(res->start, fiq, -1, NULL, signal_irq,
				  wakeup_irq, pdev->dev.parent->parent);

	return 0;
}

static const struct of_device_id tegra_serial_debug_trusty_match[] = {
	{ .compatible = "android,trusty-fiq-v1-tegra-uart", },
	{}
};

static struct platform_driver tegra_serial_debug_trusty_driver = {
	.probe		= tegra_serial_debug_trusty_probe,
	.driver		= {
		.name	= "tegra-fiq-debug",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_serial_debug_trusty_match),
	},
};

module_platform_driver(tegra_serial_debug_trusty_driver);
