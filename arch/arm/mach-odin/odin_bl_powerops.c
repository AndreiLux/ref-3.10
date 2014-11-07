/*
 * arch/arm/mach-odin/odin_bl_powerops.c
 *
 * Copyright:	(C) 2013 LG Electronics
 * Create based on arch/arm/mach-vexpress/dcscb.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/arm-cci.h>
#include <linux/delay.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/cpu_pm.h>

#include <asm/mcpm.h>
#include <asm/proc-fns.h>
#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <asm/cp15.h>

#include <linux/odin_mailbox.h>
#include <linux/platform_data/odin_tz.h>

#include "external_debug.h"

#define  CONFIG_ODIN_IPC

static arch_spinlock_t odin_bl_lock = __ARCH_SPIN_LOCK_UNLOCKED;

static int odin_bl_use_count[4][2];
static int odin_bl_cluster_cpu_mask[2];
#ifndef CONFIG_ODIN_IPC
static int first_up[8];
#endif
#ifdef CONFIG_ODIN_TEE
static u32 reset_control_base;
extern bool mobicore_sleep_ready(void);
#else
static void __iomem *reset_control_base;
#endif
static void __iomem *reset_handler_base;

#define IPC_CPU_CLUSTER_BIT(cpu,cluster) ((cluster << 2) | cpu)

static int odin_suspend = 0;
static int odin_cpu_pm_notify(struct notifier_block *nb,
			unsigned long action, void *data)
{
	switch (action) {
	case CPU_CLUSTER_PM_ENTER:
		odin_suspend = 1;
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		odin_suspend = 0;
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block odin_cpu_pm_notifier = {
	.notifier_call = odin_cpu_pm_notify,
};

int odin_get_bl_use_count(cpu, cluster)
{
	if (cpu >= 4 || cluster >= 2)
		return 0;

	return odin_bl_use_count[cpu][cluster];
}
EXPORT_SYMBOL(odin_get_bl_use_count);

/* if residency > 0, odin_bl_power_control is initiated for cpuidle */
static int odin_bl_power_control(unsigned int cpu,
				 unsigned int cluster, int on, u64 residency, void* jump_addr)
{
	int ret = 0;
#ifdef CONFIG_ODIN_IPC
	unsigned int data[7] = {0, };

	data[0] = 0x0;
	data[1] |= IPC_CPU_CLUSTER_BIT(cpu,cluster);
	data[3] = (unsigned int)virt_to_phys(jump_addr);

	if (on) {
		data[1] |= (PWR_ON<<PWR_ONOFF_SHIFT);
	}
	else {
		data[1] &= ~(PWR_ON<<PWR_ONOFF_SHIFT);
	/* FIXME: After fixing ipc, residency can be fixed for u64 */
		data[2] = (unsigned int)residency;

		/* Check this call is for suspend */
		if (odin_suspend && residency == 0)
			data[4] = 1;
	}

	ret = arch_ipc_call_fast(data);

#else
	unsigned int reg, value, smp_id, cpumask = (1 << (cpu+4));
	volatile int temp;
	int i;

	smp_id = cluster*4+cpu;

	if (on) {
		if (first_up[smp_id]) {
			first_up[smp_id] = 0;
#if 1 /* FIXME: enable this code after mailbox implemented */
			unsigned int data[7] = {0, };
			data[0] = 0x0;
			data[1] |= IPC_CPU_CLUSTER_BIT(cpu,cluster);
			data[1] |= (PWR_ON<<PWR_ONOFF_SHIFT);
			data[3] = (unsigned int)virt_to_phys(jump_addr);
			ret = arch_ipc_call_fast(data);
#else
			reg = (unsigned)reset_control_base + cluster * 0x20;
			/* reset cpu */
			value = readl(reg);
			value &= (~cpumask);
			writel(value, reg);
			/* delay 20 cycles */
			for (i=0; i<40; i++) {
				temp++;
			}
			value |= cpumask;
			writel(value, reg);
#endif
		} else {
			reg = (unsigned)reset_control_base + cluster * 0x20;
			/* reset cpu */
#ifdef CONFIG_ODIN_TEE
			value = tz_read(reg);
			value &= (~cpumask);
			tz_write(value, reg);
#else
			value = readl(reg);
			value &= (~cpumask);
			writel(value, reg);
#endif
			/* delay 20 cycles */
			for (i=0; i<40; i++) {
				temp++;
			}
			value |= cpumask;
#ifdef CONFIG_ODIN_TEE
			tz_write(value, reg);
#else
			writel(value, reg);
#endif
		}
	} else {
		while (1) {
			wfi();
		}
	}
#endif
	return ret;
}

static void odin_set_wbaddr(int cpu, int cluster, void *jump_addr)
{
#ifndef CONFIG_ODIN_TEE
	void __iomem *reset_handler;

	reset_handler = reset_handler_base + cpu*4 + cluster*16;
	writel(virt_to_phys(jump_addr), reset_handler);
	__cpuc_flush_dcache_area(reset_handler, 4);
	outer_clean_range(__pa(reset_handler), __pa(reset_handler) + 3);
#else
	u32 reset_handler;
	reset_handler = 0x200f4000 + cpu*4 + cluster*16;
	tz_write(virt_to_phys(jump_addr), reset_handler);
	/* tz_secondary(cpu + cluster * 4, virt_to_phys(jump_addr)); */
#endif
}

static int odin_bl_power_up(unsigned int cpu, unsigned int cluster)
{
	volatile unsigned int *reset_handler;

	pr_debug("%s: cpu %u cluster %u\n", __func__, cpu, cluster);
	if (cpu >= 4 || cluster >= 2)
		return -EINVAL;

	local_irq_disable();
	arch_spin_lock(&odin_bl_lock);

	odin_bl_use_count[cpu][cluster]++;
	if (odin_bl_use_count[cpu][cluster] == 1) {
		odin_set_wbaddr(cpu, cluster, mcpm_entry_point);
		odin_bl_power_control(cpu, cluster, 1, 0, mcpm_entry_point);
	} else if (odin_bl_use_count[cpu][cluster] != 2) {
		/*
		 * The only possible values are:
		 * 0 = CPU down
		 * 1 = CPU (still) up
		 * 2 = CPU requested to be up before it had a chance
		 *     to actually make itself down.
		 * Any other value is a bug.
		 */
		BUG();
	}

	arch_spin_unlock(&odin_bl_lock);
	local_irq_enable();

	return 0;
}

static void odin_bl_down(u64 residency)
{
	unsigned int mpidr, cpu, cluster, active_cpu = 0;
	bool last_man = false, skip_wfi = false, locked = false;

	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	pr_debug("%s: cpu %u cluster %u\n", __func__, cpu, cluster);
	BUG_ON(cpu >= 4 || cluster >= 2);

	if (debug_en && cpu == 0)
		odin_debug_suspend_cpu(cpu);

	__mcpm_cpu_going_down(cpu, cluster);

	arch_spin_lock(&odin_bl_lock);

	odin_bl_use_count[cpu][cluster]--;
	if (odin_bl_use_count[cpu][cluster] == 0) {
		if (!odin_bl_use_count[0][cluster] &&
		   !odin_bl_use_count[1][cluster] &&
		   !odin_bl_use_count[2][cluster] &&
		   !odin_bl_use_count[3][cluster]) {
			/* BUG_ON(__bL_cluster_state(cluster) != CLUSTER_UP); */
			if (__mcpm_cluster_state(cluster) != CLUSTER_UP) {
				pr_crit("%s: last man but __mcpm_cluster_state(%d) returned %d\n",
					__func__, cluster, __mcpm_cluster_state(cluster));
				BUG();
			}
			last_man = true;
		}

#ifdef CONFIG_ODIN_IPC
		odin_bl_power_control(cpu, cluster, 0, residency, mcpm_entry_point);
#endif
	} else if (odin_bl_use_count[cpu][cluster] == 1) {
		/*
		 * A power_up request went ahead of us.
		 * Even if we do not want to shut this CPU down,
		 * the caller expects a certain state as if the WFI
		 * was aborted.  So let's continue with cache cleaning.
		 */
		skip_wfi = true;
	} else
		BUG();

	if (!skip_wfi)
		gic_cpu_if_down();

#ifdef CONFIG_ODIN_TEE
	if (last_man && !cluster && residency) {
		active_cpu = tz_sleep(ODIN_POWER_LOCK, NULL);
		locked=true;
		if (active_cpu > 0) {
			last_man = 0;
		}
	}
#endif

#ifdef CONFIG_ODIN_BL_POWEROPS_WORKAROUND
  if (!cluster || !residency) {
#endif
	if (last_man && __mcpm_outbound_enter_critical(cpu, cluster)) {
		arch_spin_unlock(&odin_bl_lock);

		set_cr(get_cr() & ~CR_C);
#ifdef CONFIG_ODIN_TEE
                if (!residency && cluster) {
			active_cpu = tz_sleep(ODIN_POWER_CLEAN_L2 |
						    ODIN_POWER_MARK_CPU_OFF |
						    ODIN_POWER_DISABLE_CCI |
						    ODIN_POWER_SAVE_STATE, NULL);
		} else {
			active_cpu = tz_sleep(ODIN_POWER_CLEAN_L2 |
						    ODIN_POWER_MARK_CPU_OFF |
						    ODIN_POWER_DISABLE_CCI, NULL);
		}
#else
		flush_cache_all();
		asm volatile ("clrex");
		set_auxcr(get_auxcr() & ~(1 << 6));
		cci_disable_port_by_cpu(mpidr);
#endif
		if (active_cpu == 0) {
			/*
			 * Ensure that both C & I bits are disabled in the SCTLR
			 * before disabling ACE snoops. This ensures that no
			 * coherency traffic will originate from this cpu after
			 * ACE snoops are turned off.
			 */
			cpu_proc_fin();

			__mcpm_outbound_leave_critical(cluster, CLUSTER_DOWN);
		} else {
			__mcpm_outbound_leave_critical(cluster, CLUSTER_UP);
                }
	} else {
		arch_spin_unlock(&odin_bl_lock);

		set_cr(get_cr() & ~CR_C);
#ifdef CONFIG_ODIN_TEE
		tz_sleep(ODIN_POWER_CLEAN_L1 | ODIN_POWER_MARK_CPU_OFF, NULL);
#else
		flush_cache_louis();
		asm volatile ("clrex");
		set_auxcr(get_auxcr() & ~(1 << 6));
#endif
	}
#ifdef CONFIG_ODIN_BL_POWEROPS_WORKAROUND
  }
  else {
	arch_spin_unlock(&odin_bl_lock);
	set_cr(get_cr() & ~CR_C);
#ifdef CONFIG_ODIN_TEE
	tz_sleep(ODIN_POWER_CLEAN_L1|ODIN_POWER_MARK_CPU_OFF, NULL);
#else
	flush_cache_louis();
	asm volatile ("clrex");
	set_auxcr(get_auxcr() & ~(1 << 6));
#endif
  }
#endif
#ifdef CONFIG_ODIN_TEE
	if (locked) {
		locked = false;
		tz_sleep(ODIN_POWER_UNLOCK, NULL);
	}
#endif
	__mcpm_cpu_down(cpu, cluster);

	/* Now we are prepared for power-down, do it: */
	if (!skip_wfi) {
#ifdef CONFIG_ODIN_IPC
		wfi();
#else
		odin_bl_power_control(cpu, cluster, 0, residency, mcpm_entry_point);
#endif
	}

#ifdef CONFIG_ODIN_TEE
	tz_sleep(ODIN_POWER_RECOVER, NULL);
#endif
	/* Not dead at this point?  Let our caller cope. */
}

static void odin_bl_power_down(void)
{
	odin_bl_down(0);
}

static void odin_bl_suspend(u64 residency)
{
	unsigned int mpidr, cpu, cluster;

	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	pr_debug("%s: cpu %u cluster %u, residency: %u\n",
			__func__, cpu, cluster, (unsigned int)residency);

	odin_set_wbaddr(cpu, cluster, mcpm_entry_point);
	odin_bl_down(residency);
}

static void odin_bl_powered_up(void)
{
	unsigned int mpidr, cpu, cluster;
	unsigned long flags;

	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	pr_debug("%s: cpu %u cluster %u\n", __func__, cpu, cluster);
	BUG_ON(cluster >= 2 || cpu >= 4);

	if (debug_en)
		odin_debug_resume_cpu(cpu);

	local_irq_save(flags);
	arch_spin_lock(&odin_bl_lock);

	if (!odin_bl_use_count[0][cluster] &&
	    !odin_bl_use_count[1][cluster] &&
	    !odin_bl_use_count[2][cluster] &&
	    !odin_bl_use_count[3][cluster]) {
		/* odin_bl_power_control(cpu, cluster, 1, 0); */
	}

	if (!odin_bl_use_count[cpu][cluster])
		odin_bl_use_count[cpu][cluster] = 1;

	arch_spin_unlock(&odin_bl_lock);
	local_irq_restore(flags);
}

static const struct mcpm_platform_ops odin_bl_power_ops = {
	.power_up	= odin_bl_power_up,
	.power_down	= odin_bl_power_down,
	.powered_up	= odin_bl_powered_up,
	.suspend	= odin_bl_suspend,
};

static void __init odin_bl_usage_count_init(void)
{
	unsigned int mpidr, cpu, cluster;

	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

    printk(KERN_INFO "odin_bl_usage_count_init: mpidr: %x, cpu: %u, cluster: %u\n", mpidr, cpu, cluster);

	pr_debug("%s: cpu %u cluster %u\n", __func__, cpu, cluster);
	BUG_ON(cpu >= 4 || cluster >= 2);
	odin_bl_use_count[cpu][cluster] = 1;
}

extern void odin_bl_power_up_setup(void);

static int __init odin_bl_init(void)
{
	int ret;
	volatile unsigned int *reset_handler;
	int cluster, cpu, i;

	printk(KERN_INFO "odin_bl_init start\n");

#ifndef CONFIG_ODIN_IPC
	first_up[4] = 0;
	for (i=0; i<8; i++) {
		first_up[i] = 1;
	}
#endif

	odin_bl_cluster_cpu_mask[0] = 0x0;
	odin_bl_cluster_cpu_mask[1] = 0x1;

#ifdef CONFIG_ODIN_TEE
	reset_control_base = 0x200e0000;
#else
	reset_control_base = ioremap(0x200e0000, 0x30);
#endif
	/* reset_control_base[1] = ioremap(0x200e0020, 4); */
	if (!reset_control_base) {
			printk(KERN_ERR "reset control base is NULL!!\n");
			BUG();
	}
	reset_handler_base = ioremap(0x200f4000, 32);
	if (!reset_handler_base) {
			printk(KERN_ERR "reset handler base is NULL!!\n");
			BUG();
	}

	printk(KERN_INFO "reset_control_base: 0x%08X, reset_handler_base: 0x%08X\n",
		   reset_control_base, reset_handler_base);

	odin_bl_usage_count_init();

	ret = mcpm_platform_register(&odin_bl_power_ops);
	if (!ret)
		ret = mcpm_sync_init(odin_bl_power_up_setup);

	cpu_pm_register_notifier(&odin_cpu_pm_notifier);

	/* External debugger init */
	if (debug_en) {
		for (i=0 ; i<NR_CPUS ; i++) {
			cluster = (cpu_logical_map(i) >> 8) & 0xff;
			cpu = cpu_logical_map(i) & 0xff;
			odin_debug_regs[i] = ioremap(CORESIGHT_BASE +
					(OFFSET_CLUSTER * cluster) + (OFFSET_CPU * cpu),
					SZ_4K);
		}
	}

	return ret;
}

early_initcall(odin_bl_init);
