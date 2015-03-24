#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/cpu.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>


#define MAX_FREQ_TABLE  8


/* per-CPU data for /sys/devices/system/cpu/cpufreq/stats/cpu# */
struct cpufreq_stats_cpu {
	struct kobject kobj;
	spinlock_t lock;

	uint32_t cpu;		/* cpu id */
	uint32_t freq_table[MAX_FREQ_TABLE];

	/* /sys/devices/system/cpu/cpufreq/stats/cpu#/time_in_state */
	uint64_t accum_freq[MAX_FREQ_TABLE];
	uint64_t last_update_freq_timestamp;
	int32_t current_freq_index;

	/* /sys/devices/system/cpu/cpufreq/stats/cpu#/latency_table */
	uint64_t accum_chfreq_latency[MAX_FREQ_TABLE][MAX_FREQ_TABLE];
	uint64_t chfreq_begin_timestamp;
};


static DEFINE_PER_CPU(struct cpufreq_stats_cpu *, cpufreq_stats_cpu_table);


struct cpufreq_stats_cpu_attr {
	struct attribute attr;
	 ssize_t(*show) (struct cpufreq_stats_cpu *, char *);
	 ssize_t(*store) (struct cpufreq_stats_cpu *, const char *, size_t count);
};


#define CPUFREQ_STATS_CPU_ATTR_RO(_name)                  \
	static struct cpufreq_stats_cpu_attr _name =          \
		__ATTR(_name, 0444, show_##_name, NULL)


#define to_cpufreq_stats_cpu(k) container_of(k, struct cpufreq_stats_cpu, kobj)
#define to_cpufreq_stats_cpu_attr(a) container_of(a, struct cpufreq_stats_cpu_attr, attr)


static struct kobject *g_kobj_cpufreq_stats;


static uint64_t get_time_us(void)
{
	uint64_t r;
	struct timespec ts;
	getnstimeofday(&ts);
	r = ts.tv_sec;
	r = r * 1000000 + ts.tv_nsec / 1000;
	return r;
}


static uint64_t get_time_diff_us(uint64_t end, uint64_t begin)
{
	uint64_t r = end - begin;
	if (r > 100000000000000)	/* timer workaround */
		return 0;
	else
		return r;
}


static void init_freq_table_locked(struct cpufreq_stats_cpu *stats)
{
	int i, num_freq = 0;
	struct cpufreq_frequency_table *table = cpufreq_frequency_get_table(stats->cpu);

	if (!table)
		return;

	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		stats->freq_table[num_freq++] = table[i].frequency;
	}

	BUG_ON(num_freq > MAX_FREQ_TABLE);
}


static int index_of_freq_locked(struct cpufreq_stats_cpu *stats, uint32_t freq)
{
	int i;

	if (stats->freq_table[0] == CPUFREQ_ENTRY_INVALID)
		init_freq_table_locked(stats);

	for (i = 0; i < MAX_FREQ_TABLE; i++) {
		if (stats->freq_table[i] == freq)
			return i;
	}

	return -1;
}


static void update_stats_accum_freq_locked(struct cpufreq_stats_cpu *stats, uint64_t now,
					   int new_idx)
{
	/* update time_in_state */
	if (stats->last_update_freq_timestamp && stats->current_freq_index >= 0) {
		stats->accum_freq[stats->current_freq_index] +=
		    get_time_diff_us(now, stats->last_update_freq_timestamp);
	}

	stats->last_update_freq_timestamp = now;
	stats->current_freq_index = new_idx;
}


static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct cpufreq_stats_cpu *stats = to_cpufreq_stats_cpu(kobj);
	struct cpufreq_stats_cpu_attr *stats_attr = to_cpufreq_stats_cpu_attr(attr);
	ssize_t ret = -EINVAL;

	if (stats_attr->show)
		ret = stats_attr->show(stats, buf);
	else
		ret = -EIO;

	return ret;
}


static ssize_t show_time_in_state(struct cpufreq_stats_cpu *stats, char *buf)
{
	ssize_t n = 0;
	int i;
	uint64_t now = get_time_us();

	spin_lock(&stats->lock);

	/* update time_in_state */
	update_stats_accum_freq_locked(stats, now, stats->current_freq_index);

	for (i = 0; i < MAX_FREQ_TABLE; i++) {
		if (stats->freq_table[i] == CPUFREQ_ENTRY_INVALID)
			break;

		n += snprintf(buf + n, PAGE_SIZE - n, "%9u   %llu\n", stats->freq_table[i],
			      stats->accum_freq[i]);
	}

	spin_unlock(&stats->lock);

	return n;
}


static ssize_t show_latency_table(struct cpufreq_stats_cpu *stats, char *buf)
{
	ssize_t n = 0;
	int i, j;

	spin_lock(&stats->lock);

	n += snprintf(buf + n, PAGE_SIZE - n, "   From  :    To\n");
	n += snprintf(buf + n, PAGE_SIZE - n, "         : ");

	for (i = 0; i < MAX_FREQ_TABLE; i++) {
		if (stats->freq_table[i] == CPUFREQ_ENTRY_INVALID)
			break;

		n += snprintf(buf + n, PAGE_SIZE - n, "%9u ", stats->freq_table[i]);
	}

	n += snprintf(buf + n, PAGE_SIZE - n, "\n");

	for (i = 0; i < MAX_FREQ_TABLE; i++) {
		if (stats->freq_table[i] == CPUFREQ_ENTRY_INVALID)
			break;

		n += snprintf(buf + n, PAGE_SIZE - n, "%9u: ", stats->freq_table[i]);

		for (j = 0; j < MAX_FREQ_TABLE; j++) {
			if (stats->freq_table[j] == CPUFREQ_ENTRY_INVALID)
				break;

			n += snprintf(buf + n, PAGE_SIZE - n, "%9llu ",
				      stats->accum_chfreq_latency[i][j]);
		}

		n += snprintf(buf + n, PAGE_SIZE - n, "\n");
	}

	spin_unlock(&stats->lock);

	return n;
}


/* attributes for /sys/devices/system/cpu/cpufreq/stats/cpu#/# */
CPUFREQ_STATS_CPU_ATTR_RO(time_in_state);
CPUFREQ_STATS_CPU_ATTR_RO(latency_table);


static struct attribute *default_attrs[] = {
	&time_in_state.attr,
	&latency_table.attr,
	NULL
};


static const struct sysfs_ops sysfs_ops = {
	.show = show,
};


static struct kobj_type ktype_cpufreq_stats_cpu = {
	.sysfs_ops = &sysfs_ops,
	.default_attrs = default_attrs,
};


static int cpu_stats_cpu_callback(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int new_idx;
	unsigned int cpu = (unsigned long)hcpu;
	struct cpufreq_stats_cpu *stats = per_cpu(cpufreq_stats_cpu_table, cpu);
	uint64_t now = get_time_us();

	spin_lock(&stats->lock);

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		new_idx = index_of_freq_locked(stats, cpufreq_quick_get(cpu));
		update_stats_accum_freq_locked(stats, now, new_idx);
		break;

	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		update_stats_accum_freq_locked(stats, now, -1);
		break;

	default:
		break;
	}

	spin_unlock(&stats->lock);

	return NOTIFY_OK;
}


static int cpufreq_stats_cpu_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct cpufreq_stats_cpu *stats;
	uint64_t now = get_time_us();

	stats = per_cpu(cpufreq_stats_cpu_table, freq->cpu);

	spin_lock(&stats->lock);

	if (val == CPUFREQ_PRECHANGE) {
		stats->chfreq_begin_timestamp = now;
	} else if (val == CPUFREQ_POSTCHANGE) {
		int old_idx = index_of_freq_locked(stats, freq->old);
		int new_idx = index_of_freq_locked(stats, freq->new);

		/* update latency_table */
		if (stats->chfreq_begin_timestamp) {
			if (old_idx >= 0 && new_idx >= 0) {
				stats->accum_chfreq_latency[old_idx][new_idx] +=
				    get_time_diff_us(now, stats->chfreq_begin_timestamp);
			}
			stats->chfreq_begin_timestamp = 0;
		}
		/* update time_in_state */
		update_stats_accum_freq_locked(stats, now, new_idx);
	}

	spin_unlock(&stats->lock);

	return 0;
}


static struct notifier_block cpu_stats_cpu_notifer = {
	.notifier_call = cpu_stats_cpu_callback,
};


static struct notifier_block cpufreq_notifier_block = {
	.notifier_call = cpufreq_stats_cpu_notifier,
};


static const char *cpu_name(int cpu)
{
	static const char *const name[] = {
		"cpu0",
		"cpu1",
		"cpu2",
		"cpu3"
	};

	BUG_ON(cpu >= ARRAY_SIZE(name));

	return name[cpu];
}


static void free_all_cpufreq_stats_cpu_table(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		struct cpufreq_stats_cpu *stat = per_cpu(cpufreq_stats_cpu_table, cpu);
		per_cpu(cpufreq_stats_cpu_table, cpu) = NULL;
		if (stat) {
			kobject_put(&stat->kobj);
			kfree(stat);
		}
	}
}


static int alloc_all_cpufreq_stats_cpu_table(void)
{
	int i;
	unsigned int cpu;
	int ret = 0;
	uint64_t now = get_time_us();

	/* create per-cpu sysfs nodes: /sys/devices/system/cpu/cpufreq/stats/cpu#/# */
	for_each_possible_cpu(cpu) {
		struct cpufreq_stats_cpu *stats;

		stats = kzalloc(sizeof(struct cpufreq_stats_cpu), GFP_KERNEL);
		if (stats == NULL) {
			ret = -ENOMEM;
			goto err_out;
		}

		ret = kobject_init_and_add(&stats->kobj, &ktype_cpufreq_stats_cpu,
					   g_kobj_cpufreq_stats, cpu_name(cpu));
		if (ret) {
			kfree(stats);
			goto err_out;
		}
		/* init stats data */

		spin_lock_init(&stats->lock);

		stats->cpu = cpu;

		for (i = 0; i < MAX_FREQ_TABLE; i++)
			stats->freq_table[i] = CPUFREQ_ENTRY_INVALID;

		init_freq_table_locked(stats);
		stats->current_freq_index = index_of_freq_locked(stats, cpufreq_quick_get(cpu));
		update_stats_accum_freq_locked(stats, now, stats->current_freq_index);

		per_cpu(cpufreq_stats_cpu_table, cpu) = stats;
	}

	return ret;

 err_out:
	free_all_cpufreq_stats_cpu_table();

	return ret;
}


static void remove_cpufreq_stats(void)
{
	if (g_kobj_cpufreq_stats) {
		kobject_put(g_kobj_cpufreq_stats);
		g_kobj_cpufreq_stats = NULL;
	}
}


static int create_cpufreq_stats(void)
{
	g_kobj_cpufreq_stats = kobject_create_and_add("stats", cpufreq_global_kobject);
	BUG_ON(!g_kobj_cpufreq_stats);
	return g_kobj_cpufreq_stats ? 0 : -1;
}


static int __init mt_cpufreq_stats_cpu_init(void)
{
	int err = 0;

	err = create_cpufreq_stats();
	if (err)
		goto err_out;

	err = alloc_all_cpufreq_stats_cpu_table();
	if (err)
		goto err_out_1;

	err = cpufreq_register_notifier(&cpufreq_notifier_block, CPUFREQ_TRANSITION_NOTIFIER);
	if (err)
		goto err_out_2;

	register_hotcpu_notifier(&cpu_stats_cpu_notifer);

	return err;

 err_out_2:
	free_all_cpufreq_stats_cpu_table();
 err_out_1:
	remove_cpufreq_stats();
 err_out:
	return err;
}


static void __exit mt_cpufreq_stats_cpu_exit(void)
{
	unregister_hotcpu_notifier(&cpu_stats_cpu_notifer);
	cpufreq_unregister_notifier(&cpufreq_notifier_block, CPUFREQ_TRANSITION_NOTIFIER);
	free_all_cpufreq_stats_cpu_table();
	remove_cpufreq_stats();
}


#ifdef MODULE
module_init(mt_cpufreq_stats_cpu_init);
#else				/* !MODULE */
late_initcall(mt_cpufreq_stats_cpu_init);
#endif				/* MODULE */

module_exit(mt_cpufreq_stats_cpu_exit);

MODULE_LICENSE("GPL");
