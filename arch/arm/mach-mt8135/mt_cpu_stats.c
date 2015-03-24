#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/cpu.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <asm/div64.h>

#define MAX_FREQ_TABLE  8
#define TIMER_WORKAROUND (1000000000000LL)
#define CREATE_TRACE_POINTS
#define NUM_STATE 4
#define NUM_CLUSTER 2

#include <mach/mt_cpu_stats_trace.h>
#include <mach/mt_idle_trace.h>


enum {
	NON_IDLE, RG_IDLE, SL_IDLE, DP_IDLE
};

/* per-CPU data for /sys/devices/system/cpu/cpu?/stats/?  */
struct cpu_stats {
	struct kobject kobj;
	spinlock_t lock;

	/* cpu id */
	uint32_t cpu;

	/* /sys/devices/system/cpu/cpuX/stats/time_online */
	uint64_t accum_online;
	uint64_t accum_offline;
	uint64_t online_last_update;
	uint64_t offline_last_update;

	/* /sys/devices/system/cpu/cpuX/stats/latency_table */
	uint64_t accum_online_latency;
	uint64_t accum_offline_latency;
	uint64_t online_begin_timestamp;
	uint64_t offline_begin_timestamp;

	/* /sys/devices/system/cpu/cpuX/stats/trans_table */
	uint32_t online_count;
	uint32_t offline_count;
	/* in not-idle or x C-state */
	uint32_t in_state;
	uint32_t in_cpu_freq_level;
	uint64_t enter_state_time;
	/* 8 DVFS levels and 4 = 1 Non-Idle and 3 C-States */
	uint64_t time_in_C_state[MAX_FREQ_TABLE][NUM_STATE];
};


struct cpu_latency {
	/* /sys/devices/system/cpu/cpuX/stats/static_latency_table */
	uint32_t online_latency;
	uint32_t offline_latency;
};


static DEFINE_PER_CPU(struct cpu_stats *, cpu_stats_table);


static struct cpu_latency cpu_latency_table[NR_CPUS] = {
	{.online_latency = 7, .offline_latency = 5 },
	{.online_latency = 7, .offline_latency = 5 },
	{.online_latency = 9, .offline_latency = 6 },
	{.online_latency = 9, .offline_latency = 6 },
};


static int freq_cluster[NUM_CLUSTER][MAX_FREQ_TABLE]
= { {-1, -1, -1, -1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1, -1, -1, -1} };

/* 2 clusters, 4 states, 8 levels */
static unsigned int
power_state[NUM_CLUSTER][NUM_STATE][MAX_FREQ_TABLE]
= {{{0 } } };

struct cpu_stats_attr {
	struct attribute attr;
	ssize_t (*show)(struct cpu_stats *, char *);
	ssize_t (*store)(struct cpu_stats *, const char *, size_t count);
};


#define CPU_STATS_ATTR_RO(_name)                  \
	static struct cpu_stats_attr _name =          \
		__ATTR(_name, 0444, show_##_name, NULL)


#define to_cpu_stats(k) container_of(k, struct cpu_stats, kobj)
#define to_cpu_stats_attr(a) container_of(a, struct cpu_stats_attr, attr)


static ssize_t _show_table(struct cpu_stats *stats
, char *buf, ssize_t space, unsigned int X[])
{
	ssize_t copied = 0;

	copied += snprintf(buf + copied, space - copied,
	"From   :  To\n");
	copied += snprintf(buf + copied, space - copied,
	"       :    online    offline\n");
	copied += snprintf(buf + copied, space - copied,
	"online :%10u %10u\n", X[0], X[1]);
	copied += snprintf(buf + copied, space - copied,
	"offline:%10u %10u\n", X[2], X[3]);
	return (copied >= space) ? space : copied;
}

static int get_cluster(int cpu)
{
	if (cpu == 0 || cpu == 1)
		return 0;
	else
		return 1;
}

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
	/* timer workaround */
	if (r > TIMER_WORKAROUND)
		return 0;
	else
		return r;
}

static uint32_t get_cpu_level(int cpu)
{
	int i = 0;
	int cluster = 0;
	/* int *freq; */
	unsigned int freq_khz = cpufreq_quick_get(cpu);
	cluster = get_cluster(cpu);
	for (i = 0; i < MAX_FREQ_TABLE; i++) {
		/* printk(KERN_ALERT "freq_khz = %d, freq[i]
		= %d\n\n\n\n", freq_khz, freq[i]); */
		if (freq_khz == freq_cluster[cluster][i])
			return i;
	}

	return -1; /* should never happen */
}


/*  */
/* Udpating stats/time_online related data
according to now and cpu_is_online. */
/* stats->lock must be locked before invoking this function. */
static void update_stats_accum_onoffline_locked(struct cpu_stats *stats
, uint64_t now, int cpu_is_online)
{
	/* update accum_online */
	if (stats->online_last_update)
		stats->accum_online
		+= get_time_diff_us(now, stats->online_last_update);

	/* update accum_offline */
	if (stats->offline_last_update)
		stats->accum_offline
		+= get_time_diff_us(now, stats->offline_last_update);

	stats->online_last_update = cpu_is_online ? now : 0;
	stats->offline_last_update = cpu_is_online ? 0 : now;
}


static void update_cpu0_accum_onoffline(void)
{
	uint64_t now = get_time_us();
	struct cpu_stats *stats = per_cpu(cpu_stats_table, 0);
	int cpu_is_online = cpu_online(0);

	spin_lock(&stats->lock);
	update_stats_accum_onoffline_locked(stats, now, cpu_is_online);
	spin_unlock(&stats->lock);
}


static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct cpu_stats *stats = to_cpu_stats(kobj);
	struct cpu_stats_attr *stats_attr = to_cpu_stats_attr(attr);
	ssize_t ret = -EINVAL;

	if (stats_attr->show)
		ret = stats_attr->show(stats, buf);
	else
		ret = -EIO;

	return ret;
}


static ssize_t store(struct kobject *kobj, struct attribute *attr,
					 const char *buf, size_t count)
{
	struct cpu_stats *stats = to_cpu_stats(kobj);
	struct cpu_stats_attr *stats_attr = to_cpu_stats_attr(attr);
	ssize_t ret = -EINVAL;

	if (stats_attr->store)
		ret = stats_attr->store(stats, buf, count);
	else
		ret = -EIO;

	return ret;
}


static void cpu_stats_sysfs_release(struct kobject *kobj)
{
	/* nothing to do... */
}


static ssize_t show_time_online(struct cpu_stats *stats, char *buf)
{
	uint64_t now = get_time_us();
	uint64_t accum_online, accum_offline;
	ssize_t n = 0;
	int cpu_is_online;

	cpu_is_online = cpu_online(stats->cpu);

	spin_lock(&stats->lock);

	update_stats_accum_onoffline_locked(stats, now, cpu_is_online);

	accum_online = stats->accum_online;
	accum_offline = stats->accum_offline;

	spin_unlock(&stats->lock);

	n += snprintf(buf + n, PAGE_SIZE - n,
	"online     %llu\n", accum_online);
	n += snprintf(buf + n, PAGE_SIZE - n,
	"offline    %llu\n", accum_offline);

	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;
}


static ssize_t show_latency_table(struct cpu_stats *stats, char *buf)
{
	uint32_t accum_online_latency, accum_offline_latency;
	ssize_t n = 0;
	int X[4];

	spin_lock(&stats->lock);
	accum_online_latency = stats->accum_online_latency;
	accum_offline_latency = stats->accum_offline_latency;
	spin_unlock(&stats->lock);


	X[0] = 0;
	X[1] = accum_offline_latency;
	X[2] = accum_online_latency;
	X[3] = 0;
	n += _show_table(stats, buf, PAGE_SIZE, X);

	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;
}


static ssize_t show_trans_table(struct cpu_stats *stats, char *buf)
{
	uint32_t online_count, offline_count;
	ssize_t n = 0;
	int X[4];

	spin_lock(&stats->lock);
	online_count = stats->online_count;
	offline_count = stats->offline_count;
	spin_unlock(&stats->lock);

	X[0] = 0;
	X[1] = offline_count;
	X[2] = online_count;
	X[3] = 0;
	n += _show_table(stats, buf, PAGE_SIZE, X);

	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;
}



static ssize_t show_static_latency_table(struct cpu_stats *stats, char *buf)
{
	ssize_t n = 0;
	uint32_t cpu = stats->cpu;
	int X[4] = { 0, cpu_latency_table[cpu].offline_latency
		, cpu_latency_table[cpu].online_latency, 0 };
	n += _show_table(stats, buf, PAGE_SIZE, X);

	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;
}


int get_power_consumption(struct cpu_stats *stats)
{
	ssize_t i = 0;
	uint64_t energy = 0, mwh = 0; /* mwh = msec * watthour */
	unsigned int *power_nonidle, *power_rgidle;
	unsigned int *power_slidle, *power_dpidle;
	int cluster = 0;
	cluster = get_cluster(stats->cpu);
	power_nonidle = power_state[cluster][NON_IDLE];
	power_rgidle = power_state[cluster][RG_IDLE];
	power_slidle = power_state[cluster][SL_IDLE];
	power_dpidle = power_state[cluster][DP_IDLE];
	for (i = 0; i < MAX_FREQ_TABLE; i++) {
		energy
		/* non-idle */
		+= stats->time_in_C_state[i][NON_IDLE] * power_nonidle[i]
		/* C1 */
		+ stats->time_in_C_state[i][RG_IDLE] * power_rgidle[i]
		/* C2 */
		+ stats->time_in_C_state[i][SL_IDLE] * power_slidle[i]
		/* C3 */
		+ stats->time_in_C_state[i][DP_IDLE] * power_dpidle[i];
	}
	mwh = energy;
	do_div(mwh, 3600);
	do_div(mwh, 1000000);
	return mwh;
}

uint64_t get_power_consumption_by_cpu(int cpu)
{
	struct cpu_stats *stats = per_cpu(cpu_stats_table, cpu);
	return get_power_consumption(stats);
}
EXPORT_SYMBOL(get_power_consumption_by_cpu);

static ssize_t show_time_in_state(struct cpu_stats *stats, char *buf)
{
	ssize_t n = 0, i = 0;
	int *freq;
	int cluster = 0;
	cluster = get_cluster(stats->cpu);
	freq = freq_cluster[cluster];
	n += snprintf(buf + n, PAGE_SIZE - n,
	"Frequency :     not-idle           C1           C2           C3\n");
	for (i = 0; i < MAX_FREQ_TABLE; i++) {
		if (freq[i] ==  -1)
			continue;
		n += snprintf(buf + n, PAGE_SIZE - n,
		"%10u: %12llu %12llu %12llu %12llu\n",
		freq[i], stats->time_in_C_state[i][0],
		stats->time_in_C_state[i][1],
		stats->time_in_C_state[i][2],
		stats->time_in_C_state[i][3]);
	}
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;
}

static ssize_t show_power_consumption(struct cpu_stats *stats, char *buf)
{
	ssize_t n = 0;
	uint64_t mwh = 0;
	mwh = get_power_consumption_by_cpu(stats->cpu);
	n += snprintf(buf + n, PAGE_SIZE - n, "%llu\n", mwh);
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;
}

static ssize_t show_power(struct cpu_stats *stats, char *buf)
{
	ssize_t n = 0, i = 0;
	int *freq;
	int *power_nonidle, *power_rgidle, *power_slidle, *power_dpidle;
	int cluster = 0;
	cluster = get_cluster(stats->cpu);
	freq = freq_cluster[cluster];
	power_nonidle = power_state[cluster][NON_IDLE];
	power_rgidle = power_state[cluster][RG_IDLE];
	power_slidle = power_state[cluster][SL_IDLE];
	power_dpidle = power_state[cluster][DP_IDLE];
	n += snprintf(buf + n, PAGE_SIZE - n,
	"Frequency :   not-idle         C1         C2         C3\n");
	for (i = 0; i < MAX_FREQ_TABLE; i++) {
		if (freq[i] ==  -1)
			continue;
		n += snprintf(buf + n, PAGE_SIZE - n,
		"%10u: %10u %10u %10u %10u\n",
		freq[i], power_nonidle[i], power_rgidle[i],
		power_slidle[i], power_dpidle[i]);
	}
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;
}


uint64_t get_time_in_C_state(int cpu, int c_state)
{
	uint64_t sumOfTime = 0;
	int i = 0;
	struct cpu_stats *stats = per_cpu(cpu_stats_table, cpu);
	for (i = 0; i < MAX_FREQ_TABLE; i++)
		sumOfTime += stats->time_in_C_state[i][c_state];
	return sumOfTime;
}
EXPORT_SYMBOL(get_time_in_C_state);

/* attributes for /sys/devices/system/cpu/cpuX/stats/X */
CPU_STATS_ATTR_RO(time_online);
CPU_STATS_ATTR_RO(latency_table);
CPU_STATS_ATTR_RO(trans_table);
CPU_STATS_ATTR_RO(static_latency_table);
CPU_STATS_ATTR_RO(time_in_state);
CPU_STATS_ATTR_RO(power);
CPU_STATS_ATTR_RO(power_consumption);

static struct attribute *default_attrs[] = {
	&time_online.attr,
	&latency_table.attr,
	&trans_table.attr,
	&static_latency_table.attr,
	&time_in_state.attr,
	&power.attr,
	&power_consumption.attr,
	NULL
};


static const struct sysfs_ops sysfs_ops = {
	.show   = show,
	.store  = store,
};


static struct kobj_type ktype_cpu_stats = {
	.sysfs_ops      = &sysfs_ops,
	.default_attrs  = default_attrs,
	.release        = cpu_stats_sysfs_release,
};


static void cpu_online_begin(unsigned int cpu)
{
	uint64_t now = get_time_us();
	struct cpu_stats *stats = per_cpu(cpu_stats_table, cpu);

	spin_lock(&stats->lock);
	stats->online_begin_timestamp = now;
	spin_unlock(&stats->lock);
}


static void cpu_online_end(unsigned int cpu, int canceled)
{
	uint64_t now = get_time_us();
	struct cpu_stats *stats = per_cpu(cpu_stats_table, cpu);
	int cpu_is_online;


	cpu_is_online = cpu_online(cpu);

	spin_lock(&stats->lock);

	/* update online_count */
	if (!canceled)
		stats->online_count++;

	/* update accum_online_latency */
	if (stats->online_begin_timestamp) {
		stats->accum_online_latency
		+= get_time_diff_us(now, stats->online_begin_timestamp);
		stats->online_begin_timestamp = 0;
	}

	/* update accum_online / accum_offline */
	update_stats_accum_onoffline_locked(stats, now, cpu_is_online);

	spin_unlock(&stats->lock);

	if (cpu != 0)
		update_cpu0_accum_onoffline();  /* workaround */
}


static void cpu_offline_begin(unsigned int cpu)
{
	uint64_t now = get_time_us();
	struct cpu_stats *stats = per_cpu(cpu_stats_table, cpu);

	spin_lock(&stats->lock);
	stats->offline_begin_timestamp = now;
	spin_unlock(&stats->lock);
}


static void cpu_offline_end(unsigned int cpu, int canceled)
{
	uint64_t now = get_time_us();
	struct cpu_stats *stats = per_cpu(cpu_stats_table, cpu);
	int cpu_is_online;

	cpu_is_online = cpu_online(cpu);

	spin_lock(&stats->lock);

	/* update offline_count */
	if (!canceled)
		stats->offline_count++;

	/* update accum_offline_latency */
	if (stats->offline_begin_timestamp) {
		stats->accum_offline_latency
		+= get_time_diff_us(now, stats->offline_begin_timestamp);
		stats->offline_begin_timestamp = 0;
	}

	/* update accum_online / accum_offline */
	update_stats_accum_onoffline_locked(stats, now, cpu_is_online);

	spin_unlock(&stats->lock);

	if (cpu != 0)
		update_cpu0_accum_onoffline();  /* workaround */
}


static int cpu_stats_cpu_callback(struct notifier_block *nfb,
	unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		cpu_online_begin(cpu);
		break;

	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		cpu_online_end(cpu, 0);
		trace_cpu_online(cpu , 1);
		break;

	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
		cpu_online_end(cpu, 1);
		break;

	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		cpu_offline_begin(cpu);
		break;

	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		cpu_offline_end(cpu, 0);
		trace_cpu_online(cpu , 0);
		break;

	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
		cpu_offline_end(cpu, 1);
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}


static struct notifier_block cpu_stats_cpu_notifer = {
	.notifier_call = cpu_stats_cpu_callback,
};



static void free_all_cpu_stats_table(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		struct cpu_stats *stat = per_cpu(cpu_stats_table, cpu);
		per_cpu(cpu_stats_table, cpu) = NULL;
		if (stat) {
			kobject_put(&stat->kobj);
			kfree(stat);
		}
	}
}

static void set_available_frequencies_and_power(void)
{
	unsigned int i = 0;
	unsigned int cpu = 0;
	int cluster = 0;
	struct cpufreq_frequency_table *table;

	table = cpufreq_frequency_get_table(cpu);

	for (cluster = 0; cluster < 2; cluster++) {
		for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
			if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
				continue;
			freq_cluster[cluster][MAX_FREQ_TABLE-1-i]
			= table[i].frequency;
			power_state[cluster][NON_IDLE][MAX_FREQ_TABLE-1-i]
			= get_nonidle_power(cluster*2
			, freq_cluster[cluster][MAX_FREQ_TABLE-1-i]);
			power_state[cluster][RG_IDLE][MAX_FREQ_TABLE-1-i]
			= get_wfi_power(cluster*2
			, freq_cluster[cluster][MAX_FREQ_TABLE-1-i]);
			power_state[cluster][SL_IDLE][MAX_FREQ_TABLE-1-i]
			= power_state[cluster][RG_IDLE][MAX_FREQ_TABLE-1-i];
		}
	}

	return;

}


static int alloc_all_cpu_stats_table(void)
{
	int ret = 0;
	unsigned int cpu;
	uint64_t now = get_time_us();

	/* create per-cpu sysfs nodes: /sys/devices/system/cpu/cpuX/stats/X */
	for_each_possible_cpu(cpu)
	{
		struct device *cpu_dev;
		struct cpu_stats *stats;

		stats = kzalloc(sizeof(struct cpu_stats), GFP_KERNEL);
		if (stats == NULL) {
			ret = -ENOMEM;
			goto err_out;
		}

		cpu_dev = get_cpu_device(cpu);
		ret = kobject_init_and_add(&stats->kobj, &ktype_cpu_stats,
		&cpu_dev->kobj, "stats");
		if (ret) {
			kfree(stats);
			goto err_out;
		}

		/* init stats data */
		spin_lock_init(&stats->lock);
		stats->in_state = 0;
		stats->in_cpu_freq_level = get_cpu_level(cpu);
		stats->enter_state_time = get_time_us();
		stats->cpu = cpu;
		set_available_frequencies_and_power();

		update_stats_accum_onoffline_locked(stats
			, now, cpu_online(cpu));

		per_cpu(cpu_stats_table, cpu) = stats;
	}

	return ret;

err_out:
	free_all_cpu_stats_table();
	return ret;
}


/* Note that in this function we assume C0 as the non-idle state */
static void state_change_event(int toState, int cpu)
{
	uint64_t now, duration;
	uint32_t cpu_level, in_state;
	struct cpu_stats *stats = per_cpu(cpu_stats_table, cpu);
	now = get_time_us();

	/* Fill in the duration to the time-in-state table for C-State */
	spin_lock(&stats->lock);
	cpu_level = stats->in_cpu_freq_level;
	if (cpu_level != -1) {
		in_state = stats->in_state;
		duration = now - stats->enter_state_time;
		/* TODO: Workaround for get_time_us() */
		if (duration > TIMER_WORKAROUND) {
			/* printk( KERN_ALERT "Oops in get_time_us()
			-> Duration is %llu!\n\n\n\n\n", duration); */
		} else
			stats->time_in_C_state[cpu_level][in_state] += duration;
	}
	/* Change to C-State x */
	stats->in_state = toState;
	stats->in_cpu_freq_level = get_cpu_level(cpu);
	stats->enter_state_time = now;
	spin_unlock(&stats->lock);
}

static void probe_Cx_to_Cy_event(void *ignore, int cpu, int from, int to)
{
	state_change_event(to, cpu); /* currently from is not used */
}


static int __init mt_cpu_stats_init(void)
{
	int ret = 0;

	ret = alloc_all_cpu_stats_table();
	WARN_ON(ret);

	register_hotcpu_notifier(&cpu_stats_cpu_notifer);

	ret = register_trace_Cx_to_Cy_event(probe_Cx_to_Cy_event, NULL);
	return ret;
}


static void __exit mt_cpu_stats_exit(void)
{
	unregister_hotcpu_notifier(&cpu_stats_cpu_notifer);
	unregister_trace_Cx_to_Cy_event(probe_Cx_to_Cy_event, NULL);
	tracepoint_synchronize_unregister();
	free_all_cpu_stats_table();
}


#ifdef MODULE
	module_init(mt_cpu_stats_init);
#else /* !MODULE */
	late_initcall(mt_cpu_stats_init);
#endif /* MODULE */

module_exit(mt_cpu_stats_exit);

MODULE_LICENSE("GPL");

