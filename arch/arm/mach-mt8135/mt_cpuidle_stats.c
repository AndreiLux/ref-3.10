#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <mach/mt_idle_trace.h>
#define NUM_STATE 4

MODULE_LICENSE("GPL");

extern uint64_t get_time_in_C_state(int cpu, int c_state);

// per-CPU data for /sys/devices/system/cpu/cpu*/cpuidle/stats/*
struct cpuidle_stats {
	struct kobject kobj;
	struct kobject *parent;
	spinlock_t lock;
	unsigned int latency_table[NUM_STATE][NUM_STATE];
	unsigned int trans_table[NUM_STATE][NUM_STATE];
	int cpuID;
};

/* this is for static latency */
struct latency_stats {
	struct kobject kobj;
	unsigned int static_latency_table[NUM_STATE][NUM_STATE];
};

unsigned int mt8135_latency_table[NUM_STATE][NUM_STATE];


/*
Part 1: static latency stats
Actually, these values are the same,
so it is not necessary to be precpu structure.
Just for future flexibility
*/
static DEFINE_PER_CPU(struct latency_stats *, latency_stats_table);

struct latency_stats_attr {
	struct attribute attr;
	ssize_t (*show)(struct latency_stats *, char *);
	ssize_t (*store)(struct latency_stats *, const char *, size_t count);
};

/* Note that there is a hack in it because naming
conflict with CPUIDLE_STATS_ATTR_RO: _name -> static_name */
#define LATENCY_STATS_ATTR_RO(_name)                  \
	static struct latency_stats_attr static_##_name =          \
		__ATTR(_name, 0444, show_static_##_name, NULL)


#define to_latency_stats(k) container_of(k, struct latency_stats, kobj)
#define to_latency_stats_attr(a) \
			container_of(a, struct latency_stats_attr, attr)

static ssize_t _show_table(char *buf, ssize_t space, unsigned int X[4][4])
{
	ssize_t copied = 0;

	copied += snprintf(buf + copied, space - copied
		, "From     : To\n");
	copied += snprintf(buf + copied, space - copied,
	"         :   Not-idle         C1         C2         C3\n");
	copied += snprintf(buf + copied, space - copied
		, "Not-idle : %10u %10u %10u %10u\n"
		, 0, X[0][1], X[0][2], X[0][3]);
	copied += snprintf(buf + copied, space - copied
		, "C1       : %10u %10u %10u %10u\n"
		, X[1][0], 0, 0, 0);
	copied += snprintf(buf + copied, space - copied
		, "C2       : %10u %10u %10u %10u\n"
		, X[2][0], 0, 0, 0);
	copied += snprintf(buf + copied, space - copied
		, "C3       : %10u %10u %10u %10u\n"
		, X[3][0], 0, 0, 0);

	return (copied >= space) ? space : copied;
}



static ssize_t show_static_latency_table(struct latency_stats *stats, char *buf)
{
	ssize_t n = 0;
	n += _show_table(buf, PAGE_SIZE, stats->static_latency_table);
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;
}

static ssize_t show2(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct latency_stats *stats = to_latency_stats(kobj);
	struct latency_stats_attr *stats_attr = to_latency_stats_attr(attr);
	ssize_t ret = -EINVAL;

	if (stats_attr->show)
		ret = stats_attr->show(stats, buf);
	else
		ret = -EIO;

	return ret;
}

static void latency_stats_sysfs_release(struct kobject *kobj)
{
	/* nothing to do... */
}

/* attribute for /sys/devices/system/cpu/cpuX/cpuidle/latency_table */
LATENCY_STATS_ATTR_RO(latency_table);

static struct attribute *static_attrs[] = {
	&static_latency_table.attr,
	NULL
};


static const struct sysfs_ops sysfs_latency_ops = {
	.show   = show2,
	.store  = NULL,
};


static struct kobj_type ktype_latency_stats = {
	.sysfs_ops      = &sysfs_latency_ops,
	.default_attrs  = static_attrs,
	.release        = latency_stats_sysfs_release,
};


static void free_all_latency_stats_table(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu)
	{
		struct latency_stats *latency_stats
		= per_cpu(latency_stats_table, cpu);
		per_cpu(latency_stats_table, cpu) = NULL;
		if (latency_stats) {
			kobject_put(&latency_stats->kobj);
			kfree(latency_stats);
		}
	}
}



/* Part 2: cpu stats */
static DEFINE_PER_CPU(struct cpuidle_stats *, cpuidle_stats_table);

struct cpuidle_stats_attr {
	struct attribute attr;
	ssize_t (*show)(struct cpuidle_stats *, char *);
	ssize_t (*store)(struct cpuidle_stats *, const char *, size_t count);
};


#define CPUIDLE_STATS_ATTR_RO(_name)                  \
	static struct cpuidle_stats_attr _name =          \
		__ATTR(_name, 0444, show_##_name, NULL)


#define to_cpuidle_stats(k) container_of(k, struct cpuidle_stats, kobj)
#define to_cpuidle_stats_attr(a) \
		container_of(a, struct cpuidle_stats_attr, attr)

static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct cpuidle_stats *stats = to_cpuidle_stats(kobj);
	struct cpuidle_stats_attr *stats_attr = to_cpuidle_stats_attr(attr);
	ssize_t ret = -EINVAL;

	if (stats_attr->show)
		ret = stats_attr->show(stats, buf);
	else
		ret = -EIO;

	return ret;
}

static void cpuidle_stats_sysfs_release(struct kobject *kobj)
{
	/* nothing to do... */
}

static ssize_t show_latency_table(struct cpuidle_stats *stats, char *buf)
{
	/* Note that this is for "dynamic" latency */
	ssize_t n = 0, i = 0, j = 0;
	unsigned int X[NUM_STATE][NUM_STATE];
	for (i = 0; i < NUM_STATE; i++)
		for (j = 0; j < NUM_STATE; j++)
			X[i][j] = stats->trans_table[i][j]
			* mt8135_latency_table[i][j];
	n += _show_table(buf, PAGE_SIZE, X);
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;
}


static ssize_t show_trans_table(struct cpuidle_stats *stats, char *buf)
{
	ssize_t n = 0;
	n += _show_table(buf, PAGE_SIZE, stats->trans_table);
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;
}

static ssize_t show_time_in_state(struct cpuidle_stats *stats, char *buf)
{
	ssize_t n = 0, i = 0;
	uint64_t time_C[NUM_STATE];
	for (i = 1; i < 4; i++) {
		time_C[i] = get_time_in_C_state(stats->cpuID, i);
		n += snprintf(buf + n, PAGE_SIZE - n, "C%d %llu\n"
		, i, time_C[i]);
	}
	n += snprintf(buf + n, PAGE_SIZE - n, "C4 %u\n", 0);
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;
}

/* attribute for /sys/devices/system/cpu/cpuX/cpuidle/stats/X */
CPUIDLE_STATS_ATTR_RO(trans_table);
CPUIDLE_STATS_ATTR_RO(latency_table);
CPUIDLE_STATS_ATTR_RO(time_in_state);

static struct attribute *default_attrs[] = {
	&trans_table.attr,
	&latency_table.attr,
	&time_in_state.attr,
	NULL
};


static const struct sysfs_ops sysfs_ops = {
	.show   = show,
	.store  = NULL,
};


static struct kobj_type ktype_cpuidle_stats = {
	.sysfs_ops      = &sysfs_ops,
	.default_attrs  = default_attrs,
	.release        = cpuidle_stats_sysfs_release,
};




static void free_all_cpuidle_stats_table(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu)
	{
		struct cpuidle_stats *stat = per_cpu(cpuidle_stats_table, cpu);
		per_cpu(cpuidle_stats_table, cpu) = NULL;
		if (stat) {
			kobject_put(&stat->kobj);
			kfree(stat);
		}
	}
}


static ssize_t show_static_latency(struct kobject *kobj
, struct attribute *attr, char *buf)
{
	ssize_t n = 0;
	n += _show_table(buf, PAGE_SIZE, mt8135_latency_table);
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;
}




static const struct sysfs_ops sysfs_static_latency_ops = {
	.show    = show_static_latency,
};

static int alloc_all_cpuidle_stats_table(void)
{
	int ret = 0;
	unsigned int cpu;
	unsigned int values[4][4] = {
		{  0, 1, 1, 50},
		{  1, 0, 0, 0},
		{  1, 0, 0, 0},
		{150, 0, 0, 0}
	};
	/* create per-cpu sysfs nodes:
	/sys/devices/system/cpu/cpuX/cpuidle/stats/X */
	for_each_possible_cpu(cpu)
	{
		struct device *cpu_dev;
		struct cpuidle_stats *stats;
		struct latency_stats *lat_stats;

		cpu_dev = get_cpu_device(cpu);

		lat_stats = kzalloc(sizeof(struct latency_stats), GFP_KERNEL);
		if (lat_stats == NULL) {
			ret = -ENOMEM;
			goto err_out;
		}

		/* stats->parent is "cpuidle" */
		ret = kobject_init_and_add(&lat_stats->kobj
		, &ktype_latency_stats, &cpu_dev->kobj, "cpuidle");

		if (ret) {
			kfree(lat_stats);
			goto err_out;
		}

		per_cpu(latency_stats_table, cpu) = lat_stats;

		stats = kzalloc(sizeof(struct cpuidle_stats), GFP_KERNEL);
		if (stats == NULL) {
			ret = -ENOMEM;
			goto err_out;
		}

		/* .../cpuidle/stats/X */
		stats->parent = &lat_stats->kobj;
		ret = kobject_init_and_add(&stats->kobj
		, &ktype_cpuidle_stats, stats->parent, "stats");
		if (ret) {
			kfree(stats);
			goto err_out;
		}


		memcpy(lat_stats->static_latency_table, values, sizeof(values));
		memcpy(mt8135_latency_table, values, sizeof(values));

		stats->cpuID = cpu;
		spin_lock_init(&stats->lock);

		per_cpu(cpuidle_stats_table, cpu) = stats;
	}

	return ret;

err_out:
	free_all_cpuidle_stats_table();
	free_all_latency_stats_table();

	return ret;
}


static void probe_Cx_to_Cy_event(void *ignore, int cpu, int from, int to)
{
	struct cpuidle_stats *stats = per_cpu(cpuidle_stats_table, cpu);
	spin_lock(&stats->lock);
	(stats->trans_table[from][to])++;
	spin_unlock(&stats->lock);
}


static int mt_cpuidle_stats_init(void)
{
	int ret = 0;

	ret = alloc_all_cpuidle_stats_table();
	ret = register_trace_Cx_to_Cy_event(probe_Cx_to_Cy_event, NULL);
	return ret;
}


static void mt_cpuidle_stats_exit(void)
{
	unregister_trace_Cx_to_Cy_event(probe_Cx_to_Cy_event, NULL);
	tracepoint_synchronize_unregister();
	free_all_cpuidle_stats_table();
	free_all_latency_stats_table();
}


#ifdef MODULE
	module_init(mt_cpuidle_stats_init);
#else /* !MODULE */
	late_initcall(mt_cpuidle_stats_init);
#endif /* MODULE */

module_exit(mt_cpuidle_stats_exit);


