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

#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>

#define UNLOCK_MAGIC	0xc5acce55
#define NR_CORE		4
#define NR_CLUSTER	2

/* Debug register base address */
#define CORESIGHT_BASE	0x38030000
#define OFFSET_CLUSTER	0x20000
#define OFFSET_CPU		0x2000
#define OFFSET_LAR		0xfb0

enum core_break_reg {
	DBGBCR0 = 0,
	DBGBCR1,
	DBGBCR2,
	DBGBCR3,
	DBGBCR4,
	DBGBCR5,
	DBGBVR0,
	DBGBVR1,
	DBGBVR2,
	DBGBVR3,
	DBGBVR4,
	DBGBVR5,

	DBGBXVR0,
	DBGBXVR1,

	DBGWVR0,
	DBGWVR1,
	DBGWVR2,
	DBGWVR3,
	DBGWCR0,
	DBGWCR1,
	DBGWCR2,
	DBGWCR3,
	DBGVCR,

	NR_BRK_REG,
};

enum common_break_reg {
	DBGDSCR = 0,
	DBGWFAR,
	DBGCLAIM,
	DBGTRTX,
	DBGTRRX,

	FLAG_SAVED,
	NR_COMM_REG,
};


/* Default setting for external debugger
   debug_en = 1 : External debugger is supported
   debug_en = 0 : External debugger is not supported*/
int debug_en = 1;
static struct kobject *odin_debug_kobj;
static void __iomem *odin_debug_regs[NR_CORE * NR_CLUSTER];

static DEFINE_RAW_SPINLOCK(debug_lock);

static unsigned int per_core_break[NR_CORE * NR_CLUSTER][NR_COMM_REG];
static unsigned int common_break[NR_BRK_REG];

inline void common_break_save(void)
{
	asm volatile("mrc p14, 0, %0, c0, c0, 5"  : "=r" (common_break[DBGBCR0]));
	asm volatile("mrc p14, 0, %0, c0, c1, 5"  : "=r" (common_break[DBGBCR1]));
	asm volatile("mrc p14, 0, %0, c0, c2, 5"  : "=r" (common_break[DBGBCR2]));
	asm volatile("mrc p14, 0, %0, c0, c3, 5"  : "=r" (common_break[DBGBCR3]));
	asm volatile("mrc p14, 0, %0, c0, c4, 5"  : "=r" (common_break[DBGBCR4]));
	asm volatile("mrc p14, 0, %0, c0, c5, 5"  : "=r" (common_break[DBGBCR5]));
	asm volatile("mrc p14, 0, %0, c0, c0, 4"  : "=r" (common_break[DBGBVR0]));
	asm volatile("mrc p14, 0, %0, c0, c1, 4"  : "=r" (common_break[DBGBVR1]));
	asm volatile("mrc p14, 0, %0, c0, c2, 4"  : "=r" (common_break[DBGBVR2]));
	asm volatile("mrc p14, 0, %0, c0, c3, 4"  : "=r" (common_break[DBGBVR3]));
	asm volatile("mrc p14, 0, %0, c0, c4, 4" : "=r" (common_break[DBGBVR4]));
	asm volatile("mrc p14, 0, %0, c0, c5, 4" : "=r" (common_break[DBGBVR5]));

	asm volatile("mrc p14, 0, %0, c0, c0, 6" : "=r" (common_break[DBGWVR0]));
	asm volatile("mrc p14, 0, %0, c0, c1, 6" : "=r" (common_break[DBGWVR1]));
	asm volatile("mrc p14, 0, %0, c0, c2, 6" : "=r" (common_break[DBGWVR2]));
	asm volatile("mrc p14, 0, %0, c0, c3, 6" : "=r" (common_break[DBGWVR3]));
	asm volatile("mrc p14, 0, %0, c0, c0, 7" : "=r" (common_break[DBGWCR0]));
	asm volatile("mrc p14, 0, %0, c0, c1, 7" : "=r" (common_break[DBGWCR1]));
	asm volatile("mrc p14, 0, %0, c0, c2, 7" : "=r" (common_break[DBGWCR2]));
	asm volatile("mrc p14, 0, %0, c0, c3, 7" : "=r" (common_break[DBGWCR3]));
	asm volatile("mrc p14, 0, %0, c0, c7, 0" : "=r" (common_break[DBGVCR]));

	pr_debug("%s: save done\n", __func__);
}

inline void common_break_restore(void)
{
	asm volatile("mcr p14, 0, %0, c0, c0, 5"  : : "r" (common_break[DBGBCR0]));
	asm volatile("mcr p14, 0, %0, c0, c1, 5"  : : "r" (common_break[DBGBCR1]));
	asm volatile("mcr p14, 0, %0, c0, c2, 5"  : : "r" (common_break[DBGBCR2]));
	asm volatile("mcr p14, 0, %0, c0, c3, 5"  : : "r" (common_break[DBGBCR3]));
	asm volatile("mcr p14, 0, %0, c0, c4, 5"  : : "r" (common_break[DBGBCR4]));
	asm volatile("mcr p14, 0, %0, c0, c5, 5"  : : "r" (common_break[DBGBCR5]));
	asm volatile("mcr p14, 0, %0, c0, c0, 4"  : : "r" (common_break[DBGBVR0]));
	asm volatile("mcr p14, 0, %0, c0, c1, 4"  : : "r" (common_break[DBGBVR1]));
	asm volatile("mcr p14, 0, %0, c0, c2, 4"  : : "r" (common_break[DBGBVR2]));
	asm volatile("mcr p14, 0, %0, c0, c3, 4"  : : "r" (common_break[DBGBVR3]));
	asm volatile("mcr p14, 0, %0, c0, c4, 4" : : "r" (common_break[DBGBVR4]));
	asm volatile("mcr p14, 0, %0, c0, c5, 4" : : "r" (common_break[DBGBVR5]));

	asm volatile("mcr p14, 0, %0, c0, c0, 6" : : "r" (common_break[DBGWVR0]));
	asm volatile("mcr p14, 0, %0, c0, c1, 6" : : "r" (common_break[DBGWVR1]));
	asm volatile("mcr p14, 0, %0, c0, c2, 6" : : "r" (common_break[DBGWVR2]));
	asm volatile("mcr p14, 0, %0, c0, c3, 6" : : "r" (common_break[DBGWVR3]));
	asm volatile("mcr p14, 0, %0, c0, c0, 7" : : "r" (common_break[DBGWCR0]));
	asm volatile("mcr p14, 0, %0, c0, c1, 7" : : "r" (common_break[DBGWCR1]));
	asm volatile("mcr p14, 0, %0, c0, c2, 7" : : "r" (common_break[DBGWCR2]));
	asm volatile("mcr p14, 0, %0, c0, c3, 7" : : "r" (common_break[DBGWCR3]));
	asm volatile("mcr p14, 0, %0, c0, c7, 0" : : "r" (common_break[DBGVCR]));

	pr_debug("%s: restore done\n", __func__);
}

inline void debug_register_save(int cpu)
{
	asm volatile("mrc p14, 0, %0, c0, c2, 2" : "=r" (per_core_break[cpu][DBGDSCR]));
	asm volatile("mrc p14, 0, %0, c0, c6, 0" : "=r" (per_core_break[cpu][DBGWFAR]));

	raw_spin_lock(&debug_lock);
	common_break_save();
	raw_spin_unlock(&debug_lock);

	/* In save sequence, read DBGCLAIMCLR. */
	asm volatile("mrc p14, 0, %0, c7, c9, 6" : "=r" (per_core_break[cpu][DBGCLAIM]));

	asm volatile("mrc p14, 0, %0, c0, c3, 2" : "=r" (per_core_break[cpu][DBGTRTX]));
	asm volatile("mrc p14, 0, %0, c0, c0, 2" : "=r" (per_core_break[cpu][DBGTRRX]));

	per_core_break[cpu][FLAG_SAVED] = 1;

	pr_debug("%s: cpu %d save done\n", __func__, cpu);
}

inline void debug_register_restore(int cpu)
{
	int cpu_0 = 0;
	if (!per_core_break[cpu_0][FLAG_SAVED])
		goto pass;

	asm volatile("mcr p14, 0, %0, c0, c2, 2"
			: : "r" (per_core_break[cpu_0][DBGDSCR] | (1 << 14)));
	asm volatile("mcr p14, 0, %0, c0, c6, 0" : : "r" (per_core_break[cpu_0][DBGWFAR]));

	raw_spin_lock(&debug_lock);
	common_break_restore();
	raw_spin_unlock(&debug_lock);

	/* In save sequence, write DBGCLAIMSET. */
	asm volatile("mcr p14, 0, %0, c7, c8, 6" : : "r" (per_core_break[cpu_0][DBGCLAIM]));

	asm volatile("mcr p14, 0, %0, c0, c3, 2" : : "r" (per_core_break[cpu_0][DBGTRTX]));
	asm volatile("mcr p14, 0, %0, c0, c0, 2" : : "r" (per_core_break[cpu_0][DBGTRRX]));

pass:
	per_core_break[cpu][FLAG_SAVED] = 0;

	pr_debug("%s: cpu %d restore done\n", __func__, cpu);
}

static void odin_debug_suspend_cpu(int cpu)
{
	if (cpu == 0) {
		/* Set OS lock */
		asm volatile("mcr p14, 0, %0, c1, c0, 4" : : "r" (UNLOCK_MAGIC));

		/* ISB execution */
		asm volatile("mcr p15, 0, r0, c7, c5, 4" : : );

		debug_register_save(cpu);
	}
}

void odin_debug_resume_cpu(int cpu)
{
	int cpu_0 = 0;
	if (!per_core_break[cpu_0][FLAG_SAVED]) {
		unsigned int reg_dscr;

		/* Debug lock clear */
		writel(UNLOCK_MAGIC, odin_debug_regs[cpu] + OFFSET_LAR);

		/* Set OS lock */
		asm volatile("mcr p14, 0, %0, c1, c0, 4" : : "r" (UNLOCK_MAGIC));

		raw_spin_lock(&debug_lock);
		common_break_restore();
		raw_spin_unlock(&debug_lock);

		asm volatile("mrc p14, 0, %0, c0, c2, 2" : "=r"(reg_dscr));
		asm volatile("mcr p14, 0, %0, c0, c2, 2" : : "r"(reg_dscr | (1 << 14)));

		/* Clear OS lock */
		asm volatile("mcr p14, 0, %0, c1, c0, 4" : : "r" (0x1));
	} else {
		/* Debug lock clear */
		writel(UNLOCK_MAGIC, odin_debug_regs[cpu] + OFFSET_LAR);

		/* Set OS lock */
		asm volatile("mcr p14, 0, %0, c1, c0, 4" : : "r" (UNLOCK_MAGIC));

		debug_register_restore(cpu);

#if 0
		/* ISB execution */
		asm volatile("mcr p15, 0, r0, c7, c5, 4" : : );
#endif

		/* Clear OS lock */
		asm volatile("mcr p14, 0, %0, c1, c0, 4" : : "r" (0x1));
	}
}

/* external debugger sysfs */
static ssize_t odin_debug_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%du", &debug_en);
	return count;
}

static ssize_t odin_debug_enable_show(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	return sprintf(buf, "%d\n", debug_en);
}

static struct kobj_attribute odin_debug_enable_attr =
__ATTR(debug_en, 0660, odin_debug_enable_show, odin_debug_enable_store);

static struct attribute *odin_debug_attrs[] = {
	&odin_debug_enable_attr.attr,
	NULL,
};

static struct attribute_group odin_debug_attr_group = {
	.attrs = odin_debug_attrs,
};

static int __init odin_debug_init(void)
{
	int ret;

	if (!of_find_compatible_node(NULL, NULL, "lge,odin")) {
		pr_info("%s: No compatible node found\n", __func__);
		return -ENODEV;
	}

	odin_debug_kobj = kobject_create_and_add("external_dbg", kernel_kobj);
	if (!odin_debug_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(odin_debug_kobj, &odin_debug_attr_group);
	if (ret)
		kobject_put(odin_debug_kobj);

	return 0;
}


static void __exit odin_debug_exit(void)
{
	if (odin_debug_kobj)
		kobject_put(odin_debug_kobj);
}

module_init(odin_debug_init);
module_exit(odin_debug_exit);
