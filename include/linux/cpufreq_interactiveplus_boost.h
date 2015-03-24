#ifndef _LINUX_CPUFREQ_INTERACTIVEPLUS_BOOST_H
#define _LINUX_CPUFREQ_INTERACTIVEPLUS_BOOST_H

#ifdef CONFIG_CPU_FREQ_GOV_INTERACTIVEPLUS
extern int interactiveplus_boost_cpu(int boost);
#else
static inline int interactiveplus_boost_cpu(int boost);
{
	return 0;
}
#endif

#endif /* _LINUX_CPUFREQ_INTERACTIVEPLUS_BOOST_H */
