/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * Scheduler hook for average runqueue determination
 */
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/math64.h>

static DEFINE_PER_CPU(u64, nr_prod_sum);
static DEFINE_PER_CPU(u64, last_time);
static DEFINE_PER_CPU(u64, nr);
static DEFINE_PER_CPU(unsigned long, iowait_prod_sum);
static DEFINE_PER_CPU(spinlock_t, nr_lock) = __SPIN_LOCK_UNLOCKED(nr_lock);
static DEFINE_PER_CPU(u64, sgnra_last_time);

/**
 * sched_get_nr_running_avg
 * @return: Average nr_running and iowait value since last poll.
 *	    Returns the avg * 100 to return up to two decimal points
 *	    of accuracy.
 *
 * Obtains the average nr_running value since the last poll.
 * This function may not be called concurrently with itself
 */
void sched_get_nr_running_avg(int *avg, int *iowait_avg)
{
	int cpu;
	u64 curr_time;
	u64 diff_sgnra_last;
	u64 diff_last;
	u32 faultyclk_cpumask = 0;
	u64 tmp;

	*avg = 0;
	*iowait_avg = 0;
	/* read and reset nr_running counts */
	for_each_possible_cpu(cpu) {
		unsigned long flags;
		spin_lock_irqsave(&per_cpu(nr_lock, cpu), flags);
		curr_time = sched_clock();
		/* error handling for problematic clock violation */
		if (curr_time > per_cpu(sgnra_last_time, cpu) && curr_time >= per_cpu(last_time, cpu)) {
			diff_last = curr_time - per_cpu(last_time, cpu);
			diff_sgnra_last = curr_time - per_cpu(sgnra_last_time, cpu);
			tmp = per_cpu(nr, cpu) * diff_last;
			tmp += per_cpu(nr_prod_sum, cpu);
			*avg += (int)div64_u64(tmp * 100, diff_sgnra_last);
			tmp = nr_iowait_cpu(cpu) * diff_last;
			tmp += per_cpu(iowait_prod_sum, cpu);
			*iowait_avg += (int)div64_u64(tmp * 100, diff_sgnra_last);
		} else {
			faultyclk_cpumask |= 1 << cpu;
			pr_warn("[%s]**** (curr_time %lld), (per_cpu(sgnra_last_time, %d), %lld), (per_cpu(last_time, %d), %lld)\n", __func__, curr_time, cpu, per_cpu(sgnra_last_time, cpu), cpu, per_cpu(last_time, cpu));
		}
		per_cpu(sgnra_last_time, cpu) = curr_time;
		per_cpu(last_time, cpu) = curr_time;
		per_cpu(nr_prod_sum, cpu) = 0;
		per_cpu(iowait_prod_sum, cpu) = 0;
		spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
	}
	/* error handling for problematic clock violation*/
	if (faultyclk_cpumask) {
		*avg = 0;
		*iowait_avg = 0;
		pr_warn("[%s]**** CPU (%d) clock may unstable !!\n", __func__, faultyclk_cpumask);
		return;
	}
	WARN(*avg < 0, "[sched_get_nr_running_avg] avg:%d", *avg);
	WARN(*iowait_avg < 0, "[sched_get_nr_running_avg] iowait_avg:%d", *iowait_avg);
}
EXPORT_SYMBOL(sched_get_nr_running_avg);

/**
 * sched_update_nr_prod
 * @cpu: The core id of the nr running driver.
 * @nr: Updated nr running value for cpu.
 * @inc: Whether we are increasing or decreasing the count
 * @return: N/A
 *
 * Update average with latest nr_running value for CPU
 */
void sched_update_nr_prod(int cpu, unsigned long nr_running, bool inc)
{
	u64 diff;
	u64 curr_time;
	unsigned long flags;

	spin_lock_irqsave(&per_cpu(nr_lock, cpu), flags);
	curr_time = sched_clock();
	diff = curr_time - per_cpu(last_time, cpu);
	/* skip this problematic clock violation */
	if (curr_time < per_cpu(last_time, cpu)) {
		pr_warn("[%s]**** CPU (%d) clock may unstable!! (curr_time %lld) < (per_cpu(last_time, %d), %lld)\n", __func__, cpu, curr_time, cpu, per_cpu(last_time, cpu));
		spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
		return;
	}
	BUG_ON(nr_running == 0 && inc == 0);
	per_cpu(last_time, cpu) = curr_time;
	per_cpu(nr, cpu) = nr_running + (inc ? 1 : -1);
	per_cpu(nr_prod_sum, cpu) += nr_running * diff;
	per_cpu(iowait_prod_sum, cpu) += nr_iowait_cpu(cpu) * diff;
	spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
}
EXPORT_SYMBOL(sched_update_nr_prod);
