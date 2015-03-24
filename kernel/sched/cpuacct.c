#include <linux/cgroup.h>
#include <linux/slab.h>
#include <linux/percpu.h>
#include <linux/spinlock.h>
#include <linux/cpumask.h>
#include <linux/seq_file.h>
#include <linux/rcupdate.h>
#include <linux/kernel_stat.h>
#include <linux/err.h>

#include "sched.h"

#ifdef CONFIG_PERFSTATS_PERTASK_PERFREQ
#include <linux/cpufreq.h>
#include <linux/cgroup.h>
#include <asm/div64.h>
#include <linux/jiffies.h>
#include <linux/pid.h>
#include <linux/vmalloc.h>
#endif

/*
 * CPU accounting code for task groups.
 *
 * Based on the work by Paul Menage (menage@google.com) and Balbir Singh
 * (balbir@in.ibm.com).
 */

/* Time spent by the tasks of the cpu accounting group executing in ... */
enum cpuacct_stat_index {
	CPUACCT_STAT_USER,	/* ... user mode */
	CPUACCT_STAT_SYSTEM,	/* ... kernel mode */

	CPUACCT_STAT_NSTATS,
};

/* track cpu usage of a group of tasks and its child groups */
struct cpuacct {
	struct cgroup_subsys_state css;
	/* cpuusage holds pointer to a u64-type object on every cpu */
	u64 __percpu *cpuusage;
	struct kernel_cpustat __percpu *cpustat;
};

/* return cpu accounting group corresponding to this container */
static inline struct cpuacct *cgroup_ca(struct cgroup *cgrp)
{
	return container_of(cgroup_subsys_state(cgrp, cpuacct_subsys_id),
			    struct cpuacct, css);
}

/* return cpu accounting group to which this task belongs */
static inline struct cpuacct *task_ca(struct task_struct *tsk)
{
	return container_of(task_subsys_state(tsk, cpuacct_subsys_id),
			    struct cpuacct, css);
}

static inline struct cpuacct *__parent_ca(struct cpuacct *ca)
{
	return cgroup_ca(ca->css.cgroup->parent);
}

static inline struct cpuacct *parent_ca(struct cpuacct *ca)
{
	if (!ca->css.cgroup->parent)
		return NULL;
	return cgroup_ca(ca->css.cgroup->parent);
}

static DEFINE_PER_CPU(u64, root_cpuacct_cpuusage);
static struct cpuacct root_cpuacct = {
	.cpustat	= &kernel_cpustat,
	.cpuusage	= &root_cpuacct_cpuusage,
};

/* create a new cpu accounting group */
static struct cgroup_subsys_state *cpuacct_css_alloc(struct cgroup *cgrp)
{
	struct cpuacct *ca;

	if (!cgrp->parent)
		return &root_cpuacct.css;

	ca = kzalloc(sizeof(*ca), GFP_KERNEL);
	if (!ca)
		goto out;

	ca->cpuusage = alloc_percpu(u64);
	if (!ca->cpuusage)
		goto out_free_ca;

	ca->cpustat = alloc_percpu(struct kernel_cpustat);
	if (!ca->cpustat)
		goto out_free_cpuusage;

	return &ca->css;

out_free_cpuusage:
	free_percpu(ca->cpuusage);
out_free_ca:
	kfree(ca);
out:
	return ERR_PTR(-ENOMEM);
}

/* destroy an existing cpu accounting group */
static void cpuacct_css_free(struct cgroup *cgrp)
{
	struct cpuacct *ca = cgroup_ca(cgrp);

	free_percpu(ca->cpustat);
	free_percpu(ca->cpuusage);
	kfree(ca);
}

static u64 cpuacct_cpuusage_read(struct cpuacct *ca, int cpu)
{
	u64 *cpuusage = per_cpu_ptr(ca->cpuusage, cpu);
	u64 data;

#ifndef CONFIG_64BIT
	/*
	 * Take rq->lock to make 64-bit read safe on 32-bit platforms.
	 */
	raw_spin_lock_irq(&cpu_rq(cpu)->lock);
	data = *cpuusage;
	raw_spin_unlock_irq(&cpu_rq(cpu)->lock);
#else
	data = *cpuusage;
#endif

	return data;
}

static void cpuacct_cpuusage_write(struct cpuacct *ca, int cpu, u64 val)
{
	u64 *cpuusage = per_cpu_ptr(ca->cpuusage, cpu);

#ifndef CONFIG_64BIT
	/*
	 * Take rq->lock to make 64-bit write safe on 32-bit platforms.
	 */
	raw_spin_lock_irq(&cpu_rq(cpu)->lock);
	*cpuusage = val;
	raw_spin_unlock_irq(&cpu_rq(cpu)->lock);
#else
	*cpuusage = val;
#endif
}

/* return total cpu usage (in nanoseconds) of a group */
static u64 cpuusage_read(struct cgroup *cgrp, struct cftype *cft)
{
	struct cpuacct *ca = cgroup_ca(cgrp);
	u64 totalcpuusage = 0;
	int i;

	for_each_present_cpu(i)
		totalcpuusage += cpuacct_cpuusage_read(ca, i);

	return totalcpuusage;
}

static int cpuusage_write(struct cgroup *cgrp, struct cftype *cftype,
								u64 reset)
{
	struct cpuacct *ca = cgroup_ca(cgrp);
	int err = 0;
	int i;

	if (reset) {
		err = -EINVAL;
		goto out;
	}

	for_each_present_cpu(i)
		cpuacct_cpuusage_write(ca, i, 0);

out:
	return err;
}

static int cpuacct_percpu_seq_read(struct cgroup *cgroup, struct cftype *cft,
				   struct seq_file *m)
{
	struct cpuacct *ca = cgroup_ca(cgroup);
	u64 percpu;
	int i;

	for_each_present_cpu(i) {
		percpu = cpuacct_cpuusage_read(ca, i);
		seq_printf(m, "%llu ", (unsigned long long) percpu);
	}
	seq_printf(m, "\n");
	return 0;
}

static const char * const cpuacct_stat_desc[] = {
	[CPUACCT_STAT_USER] = "user",
	[CPUACCT_STAT_SYSTEM] = "system",
};

static int cpuacct_stats_show(struct cgroup *cgrp, struct cftype *cft,
			      struct cgroup_map_cb *cb)
{
	struct cpuacct *ca = cgroup_ca(cgrp);
	int cpu;
	s64 val = 0;

	for_each_online_cpu(cpu) {
		struct kernel_cpustat *kcpustat = per_cpu_ptr(ca->cpustat, cpu);
		val += kcpustat->cpustat[CPUTIME_USER];
		val += kcpustat->cpustat[CPUTIME_NICE];
	}
	val = cputime64_to_clock_t(val);
	cb->fill(cb, cpuacct_stat_desc[CPUACCT_STAT_USER], val);

	val = 0;
	for_each_online_cpu(cpu) {
		struct kernel_cpustat *kcpustat = per_cpu_ptr(ca->cpustat, cpu);
		val += kcpustat->cpustat[CPUTIME_SYSTEM];
		val += kcpustat->cpustat[CPUTIME_IRQ];
		val += kcpustat->cpustat[CPUTIME_SOFTIRQ];
	}

	val = cputime64_to_clock_t(val);
	cb->fill(cb, cpuacct_stat_desc[CPUACCT_STAT_SYSTEM], val);

	return 0;
}

#ifdef CONFIG_PERFSTATS_PERTASK_PERFREQ
/* freq_trans */
static inline void _detail_freq_stat_add(struct task_struct *task,
			unsigned int cpu,
			unsigned int frq_idx,
			struct perfstats_pertask_percore_perfreq_s *perfreq)
{
	struct perfstats_pertask_s *perf_stats = task_perfstats_info(task);

	perfreq->utime += perf_stats->percore[cpu].perfreq[frq_idx].utime;
	perfreq->stime += perf_stats->percore[cpu].perfreq[frq_idx].stime;
	perfreq->utrans += perf_stats->percore[cpu].perfreq[frq_idx].utrans;
	perfreq->strans += perf_stats->percore[cpu].perfreq[frq_idx].strans;
}

static inline u64 scale_time64(u64 bintime, cputime_t total_adj, cputime_t total)
{
	u64 temp = (__force u64) total_adj;

	if (!total)
		return (__force cputime_t) total;

	temp *= bintime;

	if (sizeof(cputime_t) == 4)
		temp = div_u64(temp, (__force u32) total);
	else
		temp = div64_u64(temp, (__force u64) total);

	return temp;
}

static unsigned long long do_power_consumption(struct task_struct *task, int is_user, int whole)
{
	int i;
	unsigned long flags;
	unsigned long long total_pc = 0;
	cputime_t utime, stime;
	struct task_cputime cputime;

	/* task time */
	if (whole && lock_task_sighand(task, &flags)) {
		thread_group_cputime(task, &cputime);
		thread_group_cputime_adjusted(task, &utime, &stime);
		unlock_task_sighand(task, &flags);
	} else  {
		cputime.utime = task->utime;
		cputime.stime = task->stime;
		task_cputime_adjusted(task, &utime, &stime);
	}

	/* print per cpu cputime info */
	for_each_possible_cpu(i) {
		struct cpufreq_frequency_table *table;
		struct perfstats_pertask_percore_perfreq_s frq;
		int idx;

		table = cpufreq_get_drivertable(i);
		if (!table)
			continue;

		if (whole) {
			if (!lock_task_sighand(task, &flags))
				continue;
			rcu_read_lock();
			if (!likely(pid_alive(task)))
				goto done;
		}

		/* cover all freq table entries */
		for (idx = 0; table[idx].frequency != CPUFREQ_TABLE_END; idx++) {

			if (table[idx].frequency == CPUFREQ_ENTRY_INVALID)
				continue;

			/* clear first */
			memset(&frq, 0, sizeof(frq));
			/* check for size */
			if (idx >= CONFIG_PERFSTATS_PERTASK_PERFREQ_FNUM)
				break;
			/* thread stats */
			_detail_freq_stat_add(task, i, idx, &frq);
			/* add group if present */
			if (whole) {
				struct task_struct *t = task;
				while_each_thread(task, t)
				_detail_freq_stat_add(t, i, idx, &frq);
			}

			if (is_user)
				total_pc += jiffies_to_msecs(frq.utime) * get_nonidle_power(i, table[idx].frequency);
			else
				total_pc += jiffies_to_msecs(frq.stime) * get_nonidle_power(i, table[idx].frequency);
		}
done:
		if (whole) {
			rcu_read_unlock();
			unlock_task_sighand(task, &flags);
		}
	}

	return total_pc;
}

static unsigned long long do_cpufreq_usage(struct task_struct *task, int is_user, int whole)
{
	int i;
	unsigned long flags;
	unsigned long long total_time = 0;
	cputime_t utime, stime;
	struct task_cputime cputime;

	/* task time */
	if (whole && lock_task_sighand(task, &flags)) {
		thread_group_cputime(task, &cputime);
		thread_group_cputime_adjusted(task, &utime, &stime);
		unlock_task_sighand(task, &flags);
	} else  {
		cputime.utime = task->utime;
		cputime.stime = task->stime;
		task_cputime_adjusted(task, &utime, &stime);
	}

	/* print per cpu cputime info */
	for_each_possible_cpu(i) {
		struct cpufreq_frequency_table *table;
		struct perfstats_pertask_percore_perfreq_s frq;
		int idx;

		table = cpufreq_get_drivertable(i);
		if (!table)
			continue;

		if (whole) {
			if (!lock_task_sighand(task, &flags))
				continue;
			rcu_read_lock();
			if (!likely(pid_alive(task)))
				goto done;
		}

		/* cover all freq table entries */
		for (idx = 0; table[idx].frequency != CPUFREQ_TABLE_END; idx++) {

			if (table[idx].frequency == CPUFREQ_ENTRY_INVALID)
				continue;

			/* clear first */
			memset(&frq, 0, sizeof(frq));
			/* check for size */
			if (idx >= CONFIG_PERFSTATS_PERTASK_PERFREQ_FNUM)
				break;
			/* thread stats */
			_detail_freq_stat_add(task, i, idx, &frq);
			/* add group if present */
			if (whole) {
				struct task_struct *t = task;
				while_each_thread(task, t)
				_detail_freq_stat_add(t, i, idx, &frq);
			}

			/* change to micro-second then scale, it is more precise. */
			if (is_user)
				total_time += scale_time64(cputime_to_usecs_64(frq.utime), utime, cputime.utime);
			else
				total_time += scale_time64(cputime_to_usecs_64(frq.stime), stime, cputime.stime);
		}
done:
		if (whole) {
			rcu_read_unlock();
			unlock_task_sighand(task, &flags);
		}
	}

	return total_time;
}

static int do_scaled_usage(struct task_struct *task, int is_user, int whole)
{
	int i;
	unsigned long flags;
	unsigned long long total_scaled_time = 0;
	cputime_t utime, stime;
	struct task_cputime cputime;

	u64 utime_tmp = 0, stime_tmp = 0;
	u64 utime_scaled = 0, stime_scaled = 0;
	u64 utime_scaled_total = 0, stime_scaled_total = 0;

	/* task time */
	if (whole && lock_task_sighand(task, &flags)) {
		thread_group_cputime(task, &cputime);
		thread_group_cputime_adjusted(task, &utime, &stime);
		unlock_task_sighand(task, &flags);
	} else  {
		cputime.utime = task->utime;
		cputime.stime = task->stime;
		task_cputime_adjusted(task, &utime, &stime);
	}

	/* print per cpu cputime info */
	for_each_possible_cpu(i) {
		struct cpufreq_frequency_table *table;
		struct perfstats_pertask_percore_perfreq_s frq;
		int idx;

		table = cpufreq_get_drivertable(i);
		if (!table)
			continue;

		if (whole) {
			if (!lock_task_sighand(task, &flags))
				continue;
			rcu_read_lock();
			if (!likely(pid_alive(task)))
				goto done;
		}

		/* cover all freq table entries */
		for (idx = 0; table[idx].frequency != CPUFREQ_TABLE_END; idx++) {

			if (table[idx].frequency == CPUFREQ_ENTRY_INVALID)
				continue;

			/* clear first */
			memset(&frq, 0, sizeof(frq));
			/* check for size */
			if (idx >= CONFIG_PERFSTATS_PERTASK_PERFREQ_FNUM)
				break;
			/* thread stats */
			_detail_freq_stat_add(task, i, idx, &frq);
			/* add group if present */
			if (whole) {
				struct task_struct *t = task;
				while_each_thread(task, t)
					_detail_freq_stat_add(t, i, idx, &frq);
			}

			if (is_user) {
				/* micro-second unit, avoid data missing */
				utime_scaled = scale_time64(cputime_to_usecs_64(frq.utime), utime, cputime.utime);
				/* us*kHz */
				utime_tmp = utime_scaled * table[idx].frequency;
				utime_scaled_total += utime_tmp;
			} else {
				/* micro-second unit, avoid data missing */
				stime_scaled = scale_time64(cputime_to_usecs_64(frq.stime), stime, cputime.stime);
				/* us*kHz */
				stime_tmp = stime_scaled * table[idx].frequency;
				stime_scaled_total += stime_tmp;
			}
		}
done:
		if (whole) {
			rcu_read_unlock();
			unlock_task_sighand(task, &flags);
		}
	}

	if (is_user)
		total_scaled_time = utime_scaled_total;
	else
		total_scaled_time = stime_scaled_total;

	/* s*kHz */
	do_div(total_scaled_time, 1000000);

	return total_scaled_time;
}

static int cpuacct_scaled_usage(struct cgroup *cgrp, struct cftype *cft,
			struct seq_file *m)
{
	int i;
	struct cgroup_tasklist ct;
	struct pid *t_pid;
	unsigned long long u_scaled_time = 0, s_scaled_time = 0;

	cgroup_load_tasks(cgrp, &ct);

	/* pidList stores PID instead of TGID, therefore, third param. is 0 */
	for (i = 0; i < ct.length; i++) {
		t_pid = find_get_pid(ct.list[i]);
		/* user time */
		u_scaled_time += do_scaled_usage(pid_task(t_pid, PIDTYPE_PID), 1, 0);
		/* system time */
		s_scaled_time += do_scaled_usage(pid_task(t_pid, PIDTYPE_PID), 0, 0);
	}

	/* s*kHz */
	seq_printf(m, "utimescaled:  %lld\n", u_scaled_time);
	/* s*kHz */
	seq_printf(m, "stimescaled:  %lld\n", s_scaled_time);

	if (ct.list)
		vfree(ct.list);

	return 0;
}

static int cpuacct_cpufreq_usage(struct cgroup *cgrp, struct cftype *cft,
			struct seq_file *m)
{
	int i;
	struct cgroup_tasklist ct;
	struct pid *t_pid;
	unsigned long long u_time = 0, s_time = 0;

	cgroup_load_tasks(cgrp, &ct);

	/* pidList stores PID instead of TGID, therefore, third param. is 0 */
	for (i = 0; i < ct.length; i++) {
		t_pid = find_get_pid(ct.list[i]);
		/* user time */
		u_time += do_cpufreq_usage(pid_task(t_pid, PIDTYPE_PID), 1, 0);
		/* system time */
		s_time += do_cpufreq_usage(pid_task(t_pid, PIDTYPE_PID), 0, 0);
	}

	/* format is from microsecond to nanosecond */
	seq_printf(m, "%lld000 ", u_time);
	/* format is from microsecond to nanosecond */
	seq_printf(m, "%lld000\n", s_time);

	if (ct.list)
		vfree(ct.list);

	return 0;
}

static int cpuacct_power_consumption(struct cgroup *cgrp, struct cftype *cft,
			struct seq_file *m)
{
	int i;
	struct cgroup_tasklist ct;
	struct pid *t_pid;
	unsigned long long u_pc = 0, s_pc = 0;

	cgroup_load_tasks(cgrp, &ct);

	/* pidList stores PID instead of TGID, therefore, third param. is 0 */
	for (i = 0; i < ct.length; i++) {

		t_pid = find_get_pid(ct.list[i]);
		u_pc += do_power_consumption(pid_task(t_pid, PIDTYPE_PID), 1, 0);
		s_pc += do_power_consumption(pid_task(t_pid, PIDTYPE_PID), 0, 0);
	}

	/* convert from mW to mWh */
	do_div(u_pc, 3600);
	do_div(s_pc, 3600);

	/* User */
	seq_printf(m, "%lld ", u_pc);
	/* System */
	seq_printf(m, "%lld\n", s_pc);

	if (ct.list)
		vfree(ct.list);

	return 0;
}

#endif

static struct cftype files[] = {
	{
		.name = "usage",
		.read_u64 = cpuusage_read,
		.write_u64 = cpuusage_write,
	},
	{
		.name = "usage_percpu",
		.read_seq_string = cpuacct_percpu_seq_read,
	},
	{
		.name = "stat",
		.read_map = cpuacct_stats_show,
	},
#ifdef CONFIG_PERFSTATS_PERTASK_PERFREQ
	{
		.name = "power_consumption",
		.read_seq_string = cpuacct_power_consumption,
	},
	{
		.name = "cpufreq_usage",
		.read_seq_string = cpuacct_cpufreq_usage,
	},
	{
		.name = "scaled_usage",
		.read_seq_string = cpuacct_scaled_usage,
	},
#endif
	{ }	/* terminate */
};

/*
 * charge this task's execution time to its accounting group.
 *
 * called with rq->lock held.
 */
void cpuacct_charge(struct task_struct *tsk, u64 cputime)
{
	struct cpuacct *ca;
	int cpu;

	cpu = task_cpu(tsk);

	rcu_read_lock();

	ca = task_ca(tsk);

	while (true) {
		u64 *cpuusage = per_cpu_ptr(ca->cpuusage, cpu);
		*cpuusage += cputime;

		ca = parent_ca(ca);
		if (!ca)
			break;
	}

	rcu_read_unlock();
}

/*
 * Add user/system time to cpuacct.
 *
 * Note: it's the caller that updates the account of the root cgroup.
 */
void cpuacct_account_field(struct task_struct *p, int index, u64 val)
{
	struct kernel_cpustat *kcpustat;
	struct cpuacct *ca;

	rcu_read_lock();
	ca = task_ca(p);
	while (ca != &root_cpuacct) {
		kcpustat = this_cpu_ptr(ca->cpustat);
		kcpustat->cpustat[index] += val;
		ca = __parent_ca(ca);
	}
	rcu_read_unlock();
}

struct cgroup_subsys cpuacct_subsys = {
	.name		= "cpuacct",
	.css_alloc	= cpuacct_css_alloc,
	.css_free	= cpuacct_css_free,
	.subsys_id	= cpuacct_subsys_id,
	.base_cftypes	= files,
	.early_init	= 1,
};
