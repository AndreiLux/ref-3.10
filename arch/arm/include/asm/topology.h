#ifndef _ASM_ARM_TOPOLOGY_H
#define _ASM_ARM_TOPOLOGY_H

#ifdef CONFIG_ARM_CPU_TOPOLOGY

#include <linux/cpumask.h>

struct cputopo_arm {
	int thread_id;
	int core_id;
	int socket_id;
	cpumask_t thread_sibling;
	cpumask_t core_sibling;
};

#ifdef CONFIG_MTK_SCHED_CMP_TGS
struct cpu_domain {
	int cluster_id;
	struct cpumask cpus;
	struct cpumask possible_cpus;
	struct list_head cpu_domains;
};

DECLARE_PER_CPU(struct cpu_domain *, cmp_cpu_domain);
#define cmp_cpu_domain(cpu)	(per_cpu(cmp_cpu_domain, (cpu)))
#endif

extern struct cputopo_arm cpu_topology[NR_CPUS];

#define topology_physical_package_id(cpu)	(cpu_topology[cpu].socket_id)
#define topology_core_id(cpu)		(cpu_topology[cpu].core_id)
#define topology_core_cpumask(cpu)	(&cpu_topology[cpu].core_sibling)
#define topology_thread_cpumask(cpu)	(&cpu_topology[cpu].thread_sibling)

#define mc_capable()	(cpu_topology[0].socket_id != -1)
#define smt_capable()	(cpu_topology[0].thread_id != -1)

void init_cpu_topology(void);
void store_cpu_topology(unsigned int cpuid);
const struct cpumask *cpu_coregroup_mask(int cpu);
int cluster_to_logical_mask(unsigned int socket_id, cpumask_t *cluster_mask);

#ifdef CONFIG_MTK_SCHED_CMP_TGS
int cluster_id(int cpu);
int nr_cpu_of_cluster(int cluster_id, bool exclusiveOffline);
struct cpu_domain *get_cpu_domain(int cpu);
struct cpumask *get_domain_cpus(int cluster_id, bool exclusiveOffline);
unsigned int cluster_nr(void);
void init_cpu_domain_early(void);
#endif

#ifdef CONFIG_DISABLE_CPU_SCHED_DOMAIN_BALANCE

#ifdef CONFIG_HMP_PACK_SMALL_TASK
/* Common values for CPUs */
#ifndef SD_CPU_INIT
#define SD_CPU_INIT (struct sched_domain) {				\
	.min_interval		= 1,					\
	.max_interval		= 4,					\
	.busy_factor		= 64,					\
	.imbalance_pct		= 125,					\
	.cache_nice_tries	= 1,					\
	.busy_idx		= 2,					\
	.idle_idx		= 1,					\
	.newidle_idx		= 0,					\
	.wake_idx		= 0,					\
	.forkexec_idx		= 0,					\
									\
	.flags			= 0*SD_LOAD_BALANCE			\
				| 1*SD_BALANCE_NEWIDLE			\
				| 1*SD_BALANCE_EXEC			\
				| 1*SD_BALANCE_FORK			\
				| 0*SD_BALANCE_WAKE			\
				| 1*SD_WAKE_AFFINE			\
				| 0*SD_SHARE_CPUPOWER			\
				| 0*SD_SHARE_PKG_RESOURCES		\
				| arch_sd_share_power_line()		\
				| 0*SD_SERIALIZE			\
				,					\
	.last_balance		 = jiffies,				\
	.balance_interval	= 1,					\
}
#endif
#endif /* CONFIG_HMP_PACK_SMALL_TASK */

#endif /* CONFIG_DISABLE_CPU_SCHED_DOMAIN_BALANCE */

#else

static inline void init_cpu_topology(void) { }
static inline void store_cpu_topology(unsigned int cpuid) { }
static inline int cluster_to_logical_mask(unsigned int socket_id,
	cpumask_t *cluster_mask) { return -EINVAL; }

#endif

#ifdef CONFIG_MTK_SCHED_CMP_TGS
#include <linux/cpumask.h>

struct cpu_domain {
	int cluster_id;
	struct cpumask cpus;
	struct cpumask possible_cpus;
	struct list_head cpu_domains;
};

static inline int cluster_id(int cpu) {return 0;}
static inline int nr_cpu_of_cluster(int cluster_id, bool exclusiveOffline){
	if(cluster_id == 0)
	{	
		if(exclusiveOffline)
			return num_online_cpus();
		else
			return num_possible_cpus();
	}
	else
		return 0;
}
static struct cpu_domain default_cluster;

static inline struct cpu_domain *get_cpu_domain(int cpu)
{
	default_cluster.cluster_id = 0;
	default_cluster.cpus = *cpu_online_mask;
	default_cluster.possible_cpus = *cpu_possible_mask;
	default_cluster.cpu_domains.next = &default_cluster.cpu_domains;
	default_cluster.cpu_domains.prev = &default_cluster.cpu_domains;
	return &default_cluster;
}
static inline void init_cpu_domain_early(void)
{
}
static inline struct cpumask *get_domain_cpus(int cluster_id, bool exclusiveOffline)
{
	if(cluster_id == 0)
	{	
		if(exclusiveOffline)
			return cpu_online_mask;
		else
			return cpu_possible_mask;
	}
	else
		return 0;
}
static inline unsigned int cluster_nr(void)
{
	return 1;
}
#endif

#include <asm-generic/topology.h>

#endif /* _ASM_ARM_TOPOLOGY_H */
