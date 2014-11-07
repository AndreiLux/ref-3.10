/*
 * Copyright 2010-2011 Calxeda, Inc.
 * Based on platsmp.c, Copyright (C) 2002 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/delay.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/platform_data/odin_tz.h>

#include <asm/io.h>
#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <asm/cacheflush.h>
#include <asm/mach/map.h>
#include <asm/mcpm.h>

#include "core.h"

static DEFINE_SPINLOCK(boot_lock);

static void __iomem *odin_cbr_base= 0x0;
extern void odin_secondary_startup(void);

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen"
 */
const static char *odin_dt_cbr_match[] __initconst = {
        "lge,cbr",
        NULL,
};

static int __init odin_cbr(unsigned long node, const char *uname,
				int depth, void *data)
{
        __be32 *reg;

        if (of_flat_dt_match(node, odin_dt_cbr_match)) {
                reg = of_get_flat_dt_prop(node, "reg", NULL);
                if (WARN_ON(!reg))
                        return -EINVAL;

                *(phys_addr_t *)data = be32_to_cpup(reg);
        }

        return 0;
}

void __init odin_smp_map_io(void)
{
	phys_addr_t phys_addr = 0;

	WARN_ON(of_scan_flat_dt(odin_cbr, (void *)&phys_addr));

	if (phys_addr) {
		/*
		 * TODO
		 * now, ODIN_SCR_VIRT_BASE is set in odin-fpga.c
		 * pte for ODIN_SCR_VIRT_BASE and phys_addr have to set
                * here
		 */
		odin_cbr_base = ODIN_SCR_VIRT_BASE + 0xfc0;
	}
}

/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
static void __cpuinit write_pen_release(int val)
{
        pen_release = val;
        smp_wmb();
        __cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
        outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

void __cpuinit odin_secondary_init(unsigned int cpu)
{
        /*
         * let the primary processor know we're out of the
         * pen, then head off into the C entry point
         */
        write_pen_release(-1);

        /*
         * Synchronise with the boot thread.
         */
        spin_lock(&boot_lock);
        spin_unlock(&boot_lock);
}

int __cpuinit odin_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
        unsigned long timeout;

        /*
         * Set synchronisation state between this boot processor
         * and the secondary one
         */
        spin_lock(&boot_lock);

        /*
         * This is really belt and braces; we hold unintended secondary
         * CPUs in the holding pen until we're ready for them.  However,
         * since we haven't sent them a soft interrupt, they shouldn't
         * be there.
         */
        write_pen_release(cpu_logical_map(cpu));

        /*
         * Send the secondary CPU a soft interrupt, thereby causing
         * the boot monitor to read the system wide flags register,
         * and branch to the address found there.
         */
		arch_send_wakeup_ipi_mask(cpumask_of(cpu));

        timeout = jiffies + (1 * HZ);
        while (time_before(jiffies, timeout)) {
                smp_rmb();
                if (pen_release == -1)
                        break;

                udelay(10);
        }

        /*
         * now the secondary core is starting up let it run its
         * calibrations, then wait for it to finish
         */
        spin_unlock(&boot_lock);

        return pen_release != -1 ? -ENOSYS : 0;
}


static int __init odin_dt_cpus_num(unsigned long node, const char *uname,
                int depth, void *data)
{
        static int prev_depth = -1;
        static int nr_cpus = -1;

        if (prev_depth > depth && nr_cpus > 0)
                return nr_cpus;

        if (nr_cpus < 0 && strcmp(uname, "cpus") == 0)
                nr_cpus = 0;

        if (nr_cpus >= 0) {
                const char *device_type = of_get_flat_dt_prop(node,
                                "device_type", NULL);

                if (device_type && strcmp(device_type, "cpu") == 0)
                        nr_cpus++;
        }

        prev_depth = depth;

        return 0;
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init odin_smp_init_cpus(void)
{
	unsigned int i, ncores;

	ncores = of_scan_flat_dt(odin_dt_cpus_num, NULL);

	/* sanity check */
	if (ncores > NR_CPUS) {
		printk(KERN_WARNING
		       "odin: no. of cores (%d) greater than configured "
		       "maximum of %d - clipping\n",
		       ncores, NR_CPUS);
		ncores = NR_CPUS;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);
}

void __init odin_smp_prepare_cpus(unsigned int max_cpus)
{
	int i, cluster, cpu;

	for (i = 1; i < max_cpus; i++) {
		cluster = (cpu_logical_map(i) & 0xFF00) >> 8;
		cpu = cpu_logical_map(i) & 0xFF;
#ifdef CONFIG_ODIN_TEE
		tz_secondary((cluster * 4 + cpu),
				virt_to_phys(odin_secondary_startup));
#else
		writel(virt_to_phys(odin_secondary_startup),
		odin_cbr_base + ((cluster*4+cpu ) << 2));
#endif
	}
}


struct smp_operations __initdata odin_smp_ops = {
	.smp_init_cpus      = odin_smp_init_cpus,
	.smp_prepare_cpus   = odin_smp_prepare_cpus,
	.smp_secondary_init = odin_secondary_init,
	.smp_boot_secondary = odin_boot_secondary,
};

void __init odin_smp_init(void)
{
	struct smp_operations *ops = &odin_smp_ops;

#ifdef CONFIG_MCPM
	if (of_find_compatible_node(NULL, NULL, "arm,cci-400"))
		mcpm_smp_set_ops();
#else
	smp_set_ops(ops);
#endif
}
