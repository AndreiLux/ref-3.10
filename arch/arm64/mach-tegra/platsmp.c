/*
 *  linux/arch/arm64/mach-tegra/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 *  Copyright (C) 2009 Palm
 *  All Rights Reserved
 *
 *  Copyright (C) 2013-2014, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/cpumask.h>

#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/soc.h>

#include "common.h"
#include "pm-soc.h"

void (*tegra_boot_secondary_cpu)(int cpu);

static int tegra_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	BUG_ON(cpu == smp_processor_id());

	if (tegra_boot_secondary_cpu) {
		cpu = cpu_logical_map(cpu);
		tegra_boot_secondary_cpu(cpu);
	}

	return 0;
}

struct smp_operations tegra_smp_ops __initdata = {
	.smp_boot_secondary	= tegra_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= tegra_cpu_die,
#endif
};
