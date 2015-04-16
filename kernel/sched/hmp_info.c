/* Copyright (c) 2013, Hisilicon technoligies co,. lto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifdef CONFIG_SCHED_HMP
#ifdef CONFIG_HOTPLUG_CPU
/*
 * Scheduler hook for average runqueue determination
 */
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/math64.h>

#include "sched.h"

static u32 max_hmp_ratio = 0;
static u32 hmp_ratio_threshold = 0;

#define MAX_RECORD_NUM	4

struct ratio_info {
	struct sched_entity *se[MAX_RECORD_NUM];
	int index;
};

static struct ratio_info g_ratio_info;

extern int hmp_get_upthreshold_num(u32 threshold);
/**
 * sched_hmp_get_ratio
 * @return: ratio_num from last num.
 *
 * Obtains the average nr_running value since the last poll.
 * This function may not be called concurrently with itself
 */
void sched_hmp_get_ratio(int *ratio_num)
{
#if 1
	*ratio_num = g_ratio_info.index;

	g_ratio_info.index = 0;

	/* printk("max_ratio %d, num %d\n", max_hmp_ratio, *ratio_num); */
#else
	*ratio_num = hmp_get_upthreshold_num(hmp_ratio_threshold);

	/* printk("up_hold %d, num %d\n", hmp_ratio_threshold, *ratio_num); */
#endif

	max_hmp_ratio = 0;
}
EXPORT_SYMBOL(sched_hmp_get_ratio);

static int find_and_add_ratio(struct sched_entity *se)
{
	int i;

	for (i = 0; i < g_ratio_info.index; i++) {
		if (se == g_ratio_info.se[i])
			return 1;
	}

	if (g_ratio_info.index < MAX_RECORD_NUM) {
		g_ratio_info.se[g_ratio_info.index++] = se;
		return 0;
	}

	return 1;
}

/**
 * sched_update_nr_prod
 * @cpu: The core id of the nr running driver.
 * @nr: Updated nr running value for cpu.
 * @inc: Whether we are increasing or decreasing the count
 * @return: N/A
 *
 * Update average with latest nr_running value for CPU
 */
void sched_update_hmp_ratio(int fast_cpu, unsigned int hmp_ratio, struct sched_entity *se)
{
#ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
	if (fast_cpu && cpu_freq_scale.dhry_multi) {
		hmp_ratio = hmp_ratio * cpu_freq_scale.freq_scale * cpu_freq_scale.dhry_scale;
		hmp_ratio >>= cpu_freq_scale.freq_multi_bit;
		hmp_ratio /= cpu_freq_scale.dhry_multi;
	}

	if (hmp_ratio > 1023)
		hmp_ratio = 1023;
#endif /* CONFIG_HMP_FREQUENCY_INVARIANT_SCALE */

	if ((hmp_ratio > hmp_ratio_threshold)
		&& (g_ratio_info.index < MAX_RECORD_NUM))
		find_and_add_ratio(se);
	if (max_hmp_ratio < hmp_ratio)
		max_hmp_ratio = hmp_ratio;
}
EXPORT_SYMBOL(sched_update_hmp_ratio);

void sched_set_hmp_ratio_threshold(unsigned int up_threshold)
{
	hmp_ratio_threshold = up_threshold;
}
EXPORT_SYMBOL(sched_set_hmp_ratio_threshold);

#endif
#endif
