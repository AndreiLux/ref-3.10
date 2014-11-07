/*
 * drivers/cpufreq/odin-cpufreq.c
 *
 * ODIN cpufreq driver
 *
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
#include <linux/cpufreq.h>
#include <linux/of.h>
#include <linux/export.h>
#include <linux/opp.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/odin_clk.h>
#include "arm_big_little.h"

static int odin_init_opp_table(struct device *cpu_dev)
{
	int i = -1, count, cluster = cpu_to_cluster(cpu_dev->id);
	struct clk_pll_table *table;
	int ret;

	if (cluster)
		table = odin_ca7_clk_t;
	else
		table = odin_ca15_clk_t;

	count = get_table_size(table);
	if (!count) {
		dev_warn(cpu_dev, "%s: count of clk_pll_table is zero!", __func__);
		return -ENOENT;
	}

	while (++i < count) {
		/* FIXME: Dummy voltage value */
		ret = opp_add(cpu_dev, table[i].clk_rate, 900000);
		if (ret) {
			dev_warn(cpu_dev, "%s: Failed to add OPP %d, err: %d\n",
				 __func__, table[i].clk_rate, ret);
			return ret;
		}
	}

	return 0;
}

static int odin_get_transition_latency(struct device *cpu_dev)
{
	/* 1 ms */
	return 1000000;
}

static struct cpufreq_arm_bL_ops odin_cpufreq_ops = {
	.name	= "odin-bl",
	.get_transition_latency = odin_get_transition_latency,
	.init_opp_table = odin_init_opp_table,
};

static int odin_cpufreq_init(void)
{
	if (!of_find_compatible_node(NULL, NULL, "lge,odin")) {
		pr_info("%s: No compatible node found\n", __func__);
		return -ENODEV;
	}

	if (!odin_ca7_clk_t || !odin_ca15_clk_t) {
		pr_info("%s: No clk_pll_table found!\n", __func__);
		return -ENOENT;
	}

	return bL_cpufreq_register(&odin_cpufreq_ops);
}
module_init(odin_cpufreq_init);

static void odin_cpufreq_exit(void)
{
	return bL_cpufreq_unregister(&odin_cpufreq_ops);
}
module_exit(odin_cpufreq_exit);

MODULE_DESCRIPTION("Odin big LITTLE cpufreq driver");
MODULE_LICENSE("GPL");
