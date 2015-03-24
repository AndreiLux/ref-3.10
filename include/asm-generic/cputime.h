#ifndef _ASM_GENERIC_CPUTIME_H
#define _ASM_GENERIC_CPUTIME_H

#include <linux/time.h>
#include <linux/jiffies.h>

#ifndef CONFIG_VIRT_CPU_ACCOUNTING
# include <asm-generic/cputime_jiffies.h>
#endif

#ifdef CONFIG_VIRT_CPU_ACCOUNTING_GEN
# include <asm-generic/cputime_nsecs.h>
#endif
/*
 * Convert cputime to ms and back.
 */
#define cputime_to_msecs(__ct)		jiffies_to_msecs(__ct)
#define msecs_to_cputime(__msecs)	msecs_to_jiffies(__msecs)
#define cputime_sub(__a, __b)		((__a) -  (__b))

/*
 * Convert cputime to msecs(u64)
 * Convert cputime to usecs(u64)
 */
#ifdef CONFIG_PERFSTATS_PERTASK_PERFREQ
#define cputime_to_msecs_64(__ct)		jiffies_to_msecs_64(__ct)
#define cputime_to_usecs_64(__ct)		\
	jiffies_to_usecs_64(cputime_to_jiffies(__ct))

#endif

#endif
