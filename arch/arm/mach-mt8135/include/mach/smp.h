#ifndef __MT_SMP_H
#define __MT_SMP_H

#include <linux/cpumask.h>
#include <mach/irqs.h>

extern void mt_secondary_startup(void);
extern void irq_raise_softirq(const struct cpumask *mask, unsigned int irq);

/* use Soft IRQ1 as the IPI */
static inline void smp_cross_call(const struct cpumask *mask)
{
	irq_raise_softirq(mask, CPU_BRINGUP_SGI);
}

static inline int get_HW_cpuid(void)
{
	int id;
 asm("mrc     p15, 0, %0, c0, c0, 5 @ Get CPUID\n":"=r"(id));
	return (id & 0x1) + ((id & 0x100) >> 7);
}

#endif				/* !__MT_SMP_H */
