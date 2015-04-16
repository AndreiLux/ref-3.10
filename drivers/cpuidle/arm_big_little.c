/*
 * big.LITTLE CPU idle driver.
 *
 * Copyright (C) 2012 ARM Ltd.
 * Author: Lorenzo Pieralisi <lorenzo.pieralisi@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/arm-cci.h>
#include <linux/bitmap.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/clockchips.h>
#include <linux/debugfs.h>
#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/tick.h>
#include <linux/vexpress.h>
#include <asm/mcpm.h>
#include <asm/cpuidle.h>
#include <asm/cputype.h>
#include <asm/idmap.h>
#include <asm/proc-fns.h>
#include <asm/suspend.h>
#include <linux/of.h>
#include <asm/smp_plat.h>
#include <asm/cputype.h>
#include <linux/slab.h>
#include <asm/cpu.h>

#ifndef CONFIG_ARCH_HI3630FPGA
#define MAX_CPU_NR		8
#else
#define MAX_CPU_NR		2
#endif

#define RUNNING_STATE_IDX	-1
#define CLUSTER_DOWN_IDX	2

#define CLUSTER_DOWN_ENABLE 1
static int bl_cpuidle_cpu_desired_state[MAX_CPU_NR];


/* extern functions */
extern void get_resource_lock(unsigned int cluster, unsigned int core);
extern void put_resource_lock(unsigned int cluster, unsigned int core);
extern void set_cluster_idle_bit(unsigned int cluster);

static int bl_enter_powerdown(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx);

#ifdef CLUSTER_DOWN_ENABLE
static int bl_enter_cluster_powerdown(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx);
#endif

static struct cpuidle_driver bl_idle_little_driver = {
	.name = "little_idle",
	.owner = THIS_MODULE,
	.states[0] = ARM_CPUIDLE_WFI_STATE,
	.states[1] = {
		.enter			= bl_enter_powerdown,
		.exit_latency		= 700,
		.target_residency	= 2500,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "C1",
		.desc			= "little-cluster core pwrdn",
	},
	.states[2] = {
		.enter			= bl_enter_cluster_powerdown,
		.exit_latency		= 5000,
		.target_residency	= 100000,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "C2",
		.desc			= "little-cluster cluster pwrdn",
	},
	.state_count = 3,
};

static struct cpuidle_driver bl_idle_big_driver = {
	.name = "big_idle",
	.owner = THIS_MODULE,
	.states[0] = ARM_CPUIDLE_WFI_STATE,
	.states[1] = {
		.enter			= bl_enter_powerdown,
		.exit_latency		= 500,
		.target_residency	= 2000,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "C1",
		.desc			= "big-cluster core pwrdn",
	},
	.states[2] = {
		.enter			= bl_enter_cluster_powerdown,
		.exit_latency		= 4000,
		.target_residency	= 80000,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "C2",
		.desc			= "big-cluster cluster pwrdn",
	},
	.state_count = 3,
};

#ifdef CLUSTER_DOWN_ENABLE
static int notrace bl_finisher(unsigned long arg)
{
	unsigned int mpidr = read_cpuid_mpidr();
	unsigned int cluster = (mpidr >> 8) & 0xf;
	unsigned int cpu = mpidr & 0xf;

	mcpm_set_entry_vector(cpu, cluster, cpu_resume);

	/* set cluster idle bit */
	get_resource_lock(cluster, cpu);
	set_cluster_idle_bit(cluster);
	put_resource_lock(cluster, cpu);

	mcpm_cpu_suspend(arg);

	return 1;
}

#define CLUSTER_MASK		0xff00

extern ktime_t menu_state_ok_until(int cpuid);

static int check_other_cpus(ktime_t when, struct cpuidle_driver *drv, int idx)
{
	unsigned int cpu = smp_processor_id();
	unsigned long mpidr = cpu_logical_map(cpu);
	unsigned int cluster = mpidr & CLUSTER_MASK;
	unsigned int i;

	for_each_cpu(i, cpu_online_mask) {
		if ( (cpu_logical_map(i) & CLUSTER_MASK) != cluster)
			continue;

		if (cpu_logical_map(i) == mpidr)
			continue;

		/* if not cpudown, bail out */
		if (bl_cpuidle_cpu_desired_state[i] < CLUSTER_DOWN_IDX)
			return -EPERM;
#if 0
		if (ktime_compare(when, menu_state_ok_until(i)) > 0)
			return -EPERM;
#endif
	}

	return 0;
}

static int bl_enter_cluster_powerdown(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx)
{
	int ret;

	BUG_ON(!irqs_disabled());

	/* if cluster powerdown conditions is not statified,
	 * then power down the cpu instead.
	 */

	bl_cpuidle_cpu_desired_state[smp_processor_id()] = CLUSTER_DOWN_IDX;

	if (check_other_cpus(ktime_get(), drv, idx))
		return drv->states[idx - 1].enter(dev, drv, idx - 1);

	cpu_pm_enter();

	ret = cpu_suspend(idx - 1, bl_finisher);
	if (ret)
		BUG();

	bl_cpuidle_cpu_desired_state[smp_processor_id()] = RUNNING_STATE_IDX;

	mcpm_cpu_powered_up();

	cpu_pm_exit();

	return idx;
}
#endif

static int notrace bl_powerdown_finisher(unsigned long arg)
{
	/* MCPM works with HW CPU identifiers */
	unsigned int mpidr = read_cpuid_mpidr();
	unsigned int cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	unsigned int cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);

	mcpm_set_entry_vector(cpu, cluster, cpu_resume);
	mcpm_cpu_suspend(0);  /* 0 AFFINITY0 */
	return 1;
}

/*
 * bl_enter_powerdown - Programs CPU to enter the specified state
 * @dev: cpuidle device
 * @drv: The target state to be programmed
 * @idx: state index
 *
 * Called from the CPUidle framework to program the device to the
 * specified target state selected by the governor.
 */
static int bl_enter_powerdown(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx)
{
	int cpuid = smp_processor_id();

	BUG_ON(!irqs_disabled());

	cpu_pm_enter();

	cpu_suspend(0, bl_powerdown_finisher);

	bl_cpuidle_cpu_desired_state[cpuid] = RUNNING_STATE_IDX;

	mcpm_cpu_powered_up();

	cpu_pm_exit();

	return idx;
}

static int __init bl_idle_driver_init(struct cpuidle_driver *drv, int cpu_id)
{
	struct cpuinfo_arm *cpu_info;
	struct cpumask *cpumask;
	unsigned long cpuid;
	int cpu;

	cpumask = kzalloc(cpumask_size(), GFP_KERNEL);
	if (!cpumask)
		return -ENOMEM;

	for_each_possible_cpu(cpu) {
		cpu_info = &per_cpu(cpu_data, cpu);
		cpuid = is_smp() ? cpu_info->cpuid : read_cpuid_id();

		/* read cpu id part number */
		if ((cpuid & 0xFFF0) == cpu_id)
			cpumask_set_cpu(cpu, cpumask);
	}

	drv->cpumask = cpumask;

	return 0;
}
static int __init bl_idle_init(void)
{
	int ret;

	/*
	 * For now the differentiation between little and big cores
	 * is based on the part number. A7 cores are considered little
	 * cores, A15 are considered big cores. This distinction may
	 * evolve in the future with a more generic matching approach.
	 */
	ret = bl_idle_driver_init(&bl_idle_little_driver,
				  ARM_CPU_PART_CORTEX_A7);
	if (ret)
		return ret;

	ret = bl_idle_driver_init(&bl_idle_big_driver, ARM_CPU_PART_CORTEX_A15);
	if (ret)
		goto out_uninit_little;

	pr_err("%s reigster  little driver sucessfully!\n", __func__);
	ret = cpuidle_register(&bl_idle_little_driver, NULL);
	if (ret)
		goto out_uninit_big;

	pr_err("%s reigster  little driver sucessfully!\n", __func__);
	ret = cpuidle_register(&bl_idle_big_driver, NULL);
	if (ret)
		goto out_unregister_little;

	pr_err("%s reigster  little driver sucessfully!\n", __func__);
	return 0;

out_unregister_little:
	cpuidle_unregister(&bl_idle_little_driver);
out_uninit_big:
	kfree(bl_idle_big_driver.cpumask);
out_uninit_little:
	kfree(bl_idle_little_driver.cpumask);

	return ret;
}
device_initcall(bl_idle_init);
