/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/moduleparam.h>
#include <linux/kallsyms.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>

#include <plat/cpu.h>
#include <mach/map.h>
#include "common.h"

#define CS_OSLOCK		(0x300)
#define CS_PC_VAL		(0xA0)

#define CS_SJTAG_STATUS		(0x8004)
#define CS_SJTAG_SOFT_LOCK	(1<<2)

#define ITERATION		(5)

static DEFINE_SPINLOCK(lock);
static int exynos_cs_init;
static unsigned int exynos_cs_base[NR_CPUS];
unsigned int exynos_cs_pc[NR_CPUS][ITERATION];

void exynos_cs_show_pcval(void)
{
	unsigned long flags;
	unsigned int cpu, iter;
	unsigned int val = 0;

	if (exynos_cs_init < 0)
		return;

	spin_lock_irqsave(&lock, flags);

	for (iter = 0; iter < ITERATION; iter++) {
		for (cpu = 0; cpu < NR_CPUS; cpu++) {
			void __iomem *base = (void *) exynos_cs_base[cpu];

			if (base == NULL || !exynos_cpu.power_state(cpu)) {
				exynos_cs_base[cpu] = 0;
				continue;
			}

			/* Release OSlock */
			writel(0x1, base + CS_OSLOCK);

			/* Read current PC value */
			val = __raw_readl(base + CS_PC_VAL);

			if (cpu >= 4) {
				/* The PCSR of A15 shoud be substracted 0x8 from
				 * curretnly PCSR value */
				val -= 0x8;
			}

			exynos_cs_pc[cpu][iter] = val;
		}
	}

	spin_unlock_irqrestore(&lock, flags);

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if (exynos_cs_base[cpu] == 0)
			continue;

		pr_err("CPU[%d] saved pc value\n", cpu);
		for (iter = 0; iter < ITERATION; iter++) {
			char buf[KSYM_SYMBOL_LEN];

			sprint_symbol(buf, exynos_cs_pc[cpu][iter]);
			pr_err("      0x%08x : %s\n", exynos_cs_pc[cpu][iter], buf);
		}
	}
}
EXPORT_SYMBOL(exynos_cs_show_pcval);

static int exynos_cs_init_dt(struct device *dev)
{
	struct device_node *node = NULL;
	const unsigned int *cs_reg;
	unsigned int cs_addr, sjtag;
	void __iomem *sjtag_base;
	int len, i = 0;

	if (!dev->of_node) {
		pr_err("Couldn't get device node!\n");
		return -ENODEV;
	}

	cs_reg = of_get_property(dev->of_node, "reg", &len);
	if (!cs_reg)
		return -ESPIPE;

	cs_addr = be32_to_cpup(cs_reg);

	/* Check Secure JTAG */
	sjtag_base = devm_ioremap(dev, cs_addr + CS_SJTAG_STATUS, SZ_4);
	if (!sjtag_base) {
		pr_err("%s: cannot ioremap cs base.\n", __func__);
		return -ENOMEM;
	}

	sjtag = readl(sjtag_base);
	devm_iounmap(dev, sjtag_base);

	if (sjtag & CS_SJTAG_SOFT_LOCK)
		return -EIO;

	while ((node = of_find_node_by_type(node, "cs"))) {
		const unsigned int *offset;
		void __iomem *cs_base;
		unsigned int cs_offset;

		offset = of_get_property(node, "offset", &len);
		if (!offset)
			return -ESPIPE;

		cs_offset = be32_to_cpup(offset);
		cs_base = devm_ioremap(dev, cs_addr + cs_offset, SZ_4K);
		if (!cs_base) {
			pr_err("%s: cannot ioremap cs base.\n", __func__);
			return -ENOMEM;
		}

		exynos_cs_base[i] = (unsigned int) cs_base;

		i++;
	}

	return 0;
}

static int exynos_cs_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	if (!pdev->dev.of_node) {
		pr_crit("[%s]No such device\n", __func__);
		return -ENODEV;
	}

	exynos_cs_init = exynos_cs_init_dt(&pdev->dev);
	if (exynos_cs_init < 0)
		return exynos_cs_init;

	/* Impossible to access KFC coresight before DVFS v2.5. */
	if (soc_is_exynos5422()) {
		unsigned int dvfs_id;

		dvfs_id = __raw_readl(S5P_VA_CHIPID + 4);
		dvfs_id = (dvfs_id >> 8) & 0x3;
		if (dvfs_id != 0x3) {
			exynos_cs_base[0] = 0;
			exynos_cs_base[1] = 0;
			exynos_cs_base[2] = 0;
			exynos_cs_base[3] = 0;
		}
	}
	return exynos_cs_init;
}

static const struct of_device_id of_exynos_cs_matches[] = {
	{.compatible = "exynos,coresight"},
	{},
};
MODULE_DEVICE_TABLE(of, of_exynos_cs_id);

static struct platform_driver exynos_cs_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "coresight",
		.of_match_table = of_match_ptr(of_exynos_cs_matches),
	},
	.probe = exynos_cs_probe,
};

module_platform_driver(exynos_cs_driver);
