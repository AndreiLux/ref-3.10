#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <asm/div64.h>

#define CREATE_TRACE_POINTS
#include <mach/mt_cluster_stats_trace.h>
#define NUM_OF_STATE 3
#define NUM_CLUSTER 2

MODULE_LICENSE("GPL");

/* prototype */
static void update_time_in_state(int cpu);

/* /sys/devices/system/cluster/clusterX/ */
struct dir_clusterN {
	struct kobject kobj;
	unsigned int clusterID;
};

/* /sys/devices/system/cluster/clusterX/stats/X */
struct dir_stats {
	struct kobject kobj;
	spinlock_t lock;
	unsigned int last_state;
	uint64_t time_in_state[NUM_OF_STATE];
	uint64_t last_state_change_time;
	unsigned int trans_table[NUM_OF_STATE][NUM_OF_STATE];
	unsigned int static_latency_table[NUM_OF_STATE][NUM_OF_STATE];
	unsigned int power[NUM_OF_STATE];
	unsigned int clusterID;
};


static struct kobject cluster_kobj;
static struct dir_stats *dir_stats_C[NUM_CLUSTER];
static struct dir_clusterN *dir_clusterN_C[NUM_CLUSTER];

struct dir_stats_attr {
	struct attribute attr;
	ssize_t (*show)(struct dir_stats *, char *);
	ssize_t (*store)(struct dir_stats *, const char *, size_t count);
};

#define to_dir_stats(k) container_of(k, struct dir_stats, kobj)
#define to_dir_stats_attr(a) container_of(a, struct dir_stats_attr, attr)

struct dir_clusterN_attr {
	struct attribute attr;
	ssize_t (*show)(struct dir_clusterN *, char *);
	ssize_t (*store)(struct dir_clusterN *, const char *, size_t count);
};

#define to_dir_clusterN(k) container_of(k, struct dir_clusterN, kobj)
#define to_dir_clusterN_attr(a) container_of(a, struct dir_clusterN_attr, attr)



/* utility function */
static ssize_t _show_time_power(char *buf, ssize_t space, int X[3])
{
	ssize_t copied = 0;

	copied += snprintf(buf + copied, space - copied, "offline  %u\n", X[0]);
	copied += snprintf(buf + copied, space - copied, "l2-only  %u\n", 0);
	copied += snprintf(buf + copied, space - copied, "1-core   %u\n", X[1]);
	copied += snprintf(buf + copied, space - copied, "2-cores  %u\n", X[2]);

	return (copied >= space) ? space : copied;

}


static ssize_t _general_show(struct dir_stats *stats
, char *buf, ssize_t space, unsigned int X[3][3]) {

	ssize_t copied = 0;

	copied += snprintf(buf + copied, space - copied, "From     : To\n");
	copied += snprintf(buf + copied, space - copied
	, "         :    offline    L2-only     1-core     2-cores\n");
	copied += snprintf(buf + copied, space - copied
	, " offline : %10u %10u %10u %10u\n", X[0][0], 0, X[0][1], X[0][2]);
	copied += snprintf(buf + copied, space - copied
	, " L2-Only : %10u %10u %10u %10u\n", 0, 0, 0, 0);
	copied += snprintf(buf + copied, space - copied
	, " 1-core  : %10u %10u %10u %10u\n", X[1][0], 0, X[1][1], X[1][2]);
	copied += snprintf(buf + copied, space - copied
	, " 2-cores : %10u %10u %10u %10u\n", X[2][0], 0, X[2][1], X[2][2]);

	return (copied >= space) ? space : copied;

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

static int get_cluster(int cpu)
{
	return (cpu == 0 || cpu == 1) ?  0 : 1;
}

/* In this task, attrbiutes can be shared by Cluster 0 and Cluster 1 */
#define dir_clusterN_ATTR_RO(_name)               \
	static struct dir_clusterN_attr _name =       \
		__ATTR(_name, 0444, show2_##_name, NULL)



/* /sys/device/system/cluster/clusterX */

static ssize_t show_clusterN(struct kobject *kobj
, struct attribute *attr, char *buf)
{
	struct dir_clusterN *stats = to_dir_clusterN(kobj);
	struct dir_clusterN_attr *stats_attr = to_dir_clusterN_attr(attr);
	ssize_t ret = -EINVAL;

	if (stats_attr->show)
		ret = stats_attr->show(stats, buf);
	else
		ret = -EIO;

	return ret;
}

static void dir_clusterN_sysfs_release(struct kobject *kobj)
{
	/* nothing to do... */
}

static const struct sysfs_ops sysfs_ops_clusterN = {
	.show   = show_clusterN,
	.store  = NULL,
};


/* because there is already a "show_state" function
in kernel/include/linux/sched.h ... so add a prefix "2" */
static ssize_t show2_state(struct dir_clusterN *stats, char *buf)
{
	ssize_t n = 0;
	int state = 0;
	const char *state_table[3] = { "offline\n", "1-core\n", "2-cores\n" };
	state = cpu_online(stats->clusterID*2)
		+ cpu_online(stats->clusterID*2+1);
	n += snprintf(buf + n, PAGE_SIZE - n, state_table[state]);
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;

}

dir_clusterN_ATTR_RO(state);

static struct attribute *attrs[] = {
	&state.attr,
	NULL
};

static struct kobj_type ktype_cluster = {
	.sysfs_ops    = &sysfs_ops_clusterN,
	.default_attrs  = attrs,
	.release        = dir_clusterN_sysfs_release,
};


/* /sys/devices/system/cluster/clusterX/stats */


static ssize_t show_power(struct dir_stats *s, char *buf)
{
	ssize_t n = 0;
	int X[4] = { s->power[0], 0, s->power[1], s->power[2] };
	n += _show_time_power(buf, PAGE_SIZE, X);
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;

}

static ssize_t show_power_consumption(struct dir_stats *s, char *buf)
{
	ssize_t n = 0;
	int i = 0;
	uint64_t mwh_cpu = 0, mwh_cluster = 0, energy = 0;
	mwh_cpu += get_power_consumption_by_cpu(s->clusterID*2)
	+ get_power_consumption_by_cpu(s->clusterID*2+1);
	for (i = 0; i < 3; i++) { /* 3 states: Offline, 1-core, 2-cores */
		energy += s->time_in_state[i] * s->power[i];
	}
	mwh_cluster = energy;
	do_div(mwh_cluster, 3600);
	do_div(mwh_cluster, 1000000);
	n += snprintf(buf + n, PAGE_SIZE - n, "%llu\n", mwh_cluster + mwh_cpu);

	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;

}

static ssize_t show_time_in_state(struct dir_stats *s, char *buf)
{
	ssize_t n = 0;
	int X[4];
	/* enforce to start the log process rather than
	waiting for cpu event triggreing */
	update_time_in_state(0); /* trigger cluster 0 */
	update_time_in_state(2); /* trigger cluster 1 */

	X[0] = s->time_in_state[2];
	X[1] = s->time_in_state[1];
	X[2] = 0;
	X[3] = s->time_in_state[0];
	n += _show_time_power(buf, PAGE_SIZE, X);
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;

}

static ssize_t show_latency_table(struct dir_stats *s, char *buf)
{
	ssize_t n = 0;
	int i = 0, j = 0;
	unsigned int latency[NUM_OF_STATE][NUM_OF_STATE];
	for (i = 0; i < NUM_OF_STATE; i++) {
		for (j = 0; j < NUM_OF_STATE; j++)
			latency[i][j] = s->trans_table[i][j]
			*s->static_latency_table[i][j];
	}
	n += _general_show(s, buf, PAGE_SIZE, latency);
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;

}


static ssize_t show_static_latency_table(struct dir_stats *stats, char *buf)
{
	ssize_t n = 0;
	n += _general_show(stats, buf, PAGE_SIZE, stats->static_latency_table);
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;

}

static ssize_t show_trans_table(struct dir_stats *stats, char *buf)
{
	ssize_t n = 0;
	n += _general_show(stats, buf, PAGE_SIZE, stats->trans_table);
	return (n >= PAGE_SIZE) ? PAGE_SIZE : n;

}


static ssize_t show_stats(struct kobject *kobj
, struct attribute *attr, char *buf)
{
	struct dir_stats *stats = to_dir_stats(kobj);
	struct dir_stats_attr *stats_attr = to_dir_stats_attr(attr);
	ssize_t ret = -EINVAL;

	if (stats_attr->show)
		ret = stats_attr->show(stats, buf);
	else
		ret = -EIO;

	return ret;
}


static void dir_stats_sysfs_release(struct kobject *kobj)
{
	/* nothing to do... */
}

static const struct sysfs_ops sysfs_ops_stats = {
	.show   = show_stats,
	.store  = NULL,
};


#define dir_stats_ATTR_RO(_name)                  \
	static struct dir_stats_attr _name =          \
		__ATTR(_name, 0444, show_##_name, NULL)



dir_stats_ATTR_RO(trans_table);
dir_stats_ATTR_RO(time_in_state);
dir_stats_ATTR_RO(static_latency_table);
dir_stats_ATTR_RO(power);
dir_stats_ATTR_RO(latency_table);
dir_stats_ATTR_RO(power_consumption);

static struct attribute *stats_attrs[] = {
	&trans_table.attr,
	&time_in_state.attr,
	&static_latency_table.attr,
	&power.attr,
	&latency_table.attr,
	&power_consumption.attr,
	NULL
};
static struct kobj_type ktype_stats = {
	.sysfs_ops    = &sysfs_ops_stats,
	.default_attrs  = stats_attrs,
	.release        = dir_stats_sysfs_release,
};


/*
	/sys/device/system/cluster
*/

static const struct sysfs_ops sysfs_ops_cluster = {
	.show   = NULL,
	.store  = NULL,
};


static struct attribute *cluster_attrs[] = {
	NULL
};

static struct kobj_type ktype_cluster_stats = {
	.sysfs_ops    = &sysfs_ops_cluster,
	.default_attrs  = cluster_attrs,
	.release        = dir_stats_sysfs_release,
};



static void update_time_in_state(int cpu)
{
	int numOfActiveCPU = 0;
	uint64_t diff;
	struct dir_stats *stat;
	uint64_t now = get_time_us();
	int cluster = get_cluster(cpu);

	stat = dir_stats_C[cluster];
	numOfActiveCPU = cpu_online(cluster*2) + cpu_online(cluster*2+1);
	spin_lock(&stat->lock);
	diff = now - stat->last_state_change_time;
	stat->last_state_change_time = now;
	/* timer workaround: 100000000 sec */
	if (diff > 100000000000000LL)
		;
	/* printk( KERN_ALERT "Oops in get_time_us()
	-> diff is %llu!\n\n\n\n\n", diff); */
	else
		stat->time_in_state[numOfActiveCPU] += diff;
	spin_unlock(&stat->lock);
}

static void cpu_transit(unsigned int cpu, int cancelled)
{
	int state;
	struct dir_stats *stat;
	int cluster;
	const char *state_table[3] = { "offline\n", "1-core\n", "2-cores\n" };

	if (cancelled == 1)
		return;
	update_time_in_state(cpu);
	cluster = get_cluster(cpu);
	state = 0;

	stat = dir_stats_C[cluster];
	if (cpu_online(cluster*2))
		state++;
	if (cpu_online(cluster*2+1))
		state++;

	spin_lock(&stat->lock);
	stat->trans_table[stat->last_state][state]++;
	stat->last_state = state;
	spin_unlock(&stat->lock);
	/* trace point event for cluster */
	trace_cluster_state(cluster, state_table[state]);
}


static int dir_stats_cpu_callback(struct notifier_block *nfb
, unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		break;

	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		cpu_transit(cpu, 0);
		break;

	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
		cpu_transit(cpu, 1);
		break;

	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		break;

	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		cpu_transit(cpu, 0);
		break;

	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
		cpu_transit(cpu, 1);
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}


static struct notifier_block dir_stats_cpu_notifer = {
	.notifier_call = dir_stats_cpu_callback,
};


static void free_all_dir_stats_C(int cluster)
{
	if (dir_stats_C[cluster] != NULL)	{
		kobject_put(&dir_stats_C[cluster]->kobj);
		kfree(dir_stats_C[cluster]);
		dir_stats_C[cluster] = NULL;
	}
	if (dir_clusterN_C[cluster] != NULL) {
		kobject_put(&dir_clusterN_C[cluster]->kobj);
		kfree(dir_clusterN_C[cluster]);
		dir_clusterN_C[cluster] = NULL;
	}
}

static void remove_symbolic_links_C(int cluster)
{
	int i = 0;
	const char *cpus[4] = { "cpu0", "cpu1", "cpu2", "cpu3"};
	struct device *cpu_dev;
	for (i = cluster*2; i <= cluster*2+1; i++) {
		cpu_dev = get_cpu_device(i);
		sysfs_remove_link(&cpu_dev->kobj, "cluster");
	}
	sysfs_remove_link(&dir_clusterN_C[cluster]->kobj, cpus[cluster*2]);
	sysfs_remove_link(&dir_clusterN_C[cluster]->kobj, cpus[cluster*2+1]);
}

static int alloc_all_dir_stats(void)
{
	int ret = 0;
	int i = 0;
	int cluster = 0;
	unsigned int values[3][3] = {
		{  0, 0, 0  },
		{  0, 0, 6  },
		{  0, 3, 0  }
	};
	unsigned int values2[3][3] = {
		{  0, 10, 0  },
		{  5, 0, 7  },
		{  0, 5, 0  }
	};
	unsigned int power1[3] = { 0, 17, 17 }; /* offline, 1-core, 2-cores */
	unsigned int power2[3] = { 0, 263, 263 }; /* offline, 1-core, 2-cores */
	/* For creating an empty "cluster" directory
	in /sys/devices/system/cluster/ */
	ret = kobject_init_and_add(&cluster_kobj, &ktype_cluster_stats
	, (&cpu_subsys.dev_root->kobj)->parent, "cluster");
	if (ret)
		return ret;
	for (cluster = 0; cluster < 2; cluster++) {
		dir_clusterN_C[cluster]
		= kzalloc(sizeof(struct dir_clusterN), GFP_KERNEL);
		if (dir_clusterN_C[cluster] == NULL) {
			ret = -ENOMEM;
			goto err_out;
		}

		/* kobject for cluster* */
		ret = kobject_init_and_add(&dir_clusterN_C[cluster]->kobj
		, &ktype_cluster, &cluster_kobj, "cluster%d", cluster);
		if (ret) {
			kfree(dir_clusterN_C[cluster]);
			dir_clusterN_C[cluster] = NULL;
			goto err_out;
		}


		/* malloc for /sys/devices/system/cluster/clusterN/stats */
		dir_stats_C[cluster]
		= kzalloc(sizeof(struct dir_stats), GFP_KERNEL);
		if (dir_stats_C[cluster] == NULL) {
			ret = -ENOMEM;
			kobject_put(&dir_clusterN_C[cluster]->kobj);
			kfree(dir_clusterN_C[cluster]);
			dir_clusterN_C[cluster] = NULL;
			goto err_out;
		}

		/* kobject for dir_stats */
		ret = kobject_init_and_add(&dir_stats_C[cluster]->kobj
		, &ktype_stats, &dir_clusterN_C[cluster]->kobj, "stats");
		if (ret) {
			kfree(dir_stats_C[cluster]);
			dir_stats_C[cluster] = NULL;
			kobject_put(&dir_clusterN_C[cluster]->kobj);
			kfree(dir_clusterN_C[cluster]);
			dir_clusterN_C[cluster] = NULL;
			goto err_out;
		}

	}

	for (i = 0; i < 2; i++) {
		dir_clusterN_C[i]->clusterID = i;
		dir_stats_C[i]->clusterID = i;
		dir_stats_C[i]->last_state
		= cpu_online(i) + cpu_online(cluster*2+1);
		spin_lock_init(&dir_stats_C[i]->lock);
	}
	memcpy(dir_stats_C[0]->static_latency_table, values, sizeof(values));
	memcpy(dir_stats_C[1]->static_latency_table, values2, sizeof(values));
	memcpy(dir_stats_C[0]->power, power1, sizeof(power1));
	memcpy(dir_stats_C[0]->power, power2, sizeof(power2));

	dir_stats_C[0]->last_state_change_time
	= dir_stats_C[1]->last_state_change_time = get_time_us();
	return ret;


err_out:
	if (cluster == 1)
		free_all_dir_stats_C(0);
	kobject_put(&cluster_kobj);
	return ret;
}

static int add_symbolic_links(void)
{
	struct device *cpu_dev;
	int ret = 0;
	int cluster = 0;
	const char *cpus[4] = { "cpu0", "cpu1", "cpu2", "cpu3" };
	for (cluster = 0; cluster < 2; cluster++) {
		/* cluster */
		/* Add symbolic links to
		/sys/devices/system/cluster/clusterX/cpuX */
		int x = cluster*2;
		int y = cluster*2+1;
		cpu_dev = get_cpu_device(cluster*2);
		ret = sysfs_create_link(&dir_clusterN_C[cluster]->kobj
		, &cpu_dev->kobj, cpus[x]);
		if (ret)
			goto err_out;
		cpu_dev = get_cpu_device(cluster*2+1);
		ret = sysfs_create_link(&dir_clusterN_C[cluster]->kobj
		, &cpu_dev->kobj, cpus[y]);
		if (ret) {
			sysfs_remove_link(&dir_clusterN_C[cluster]->kobj
			, cpus[x]);
			goto err_out;
		}
		/* Add symbolic links to /sys/devices/system/cpu/cpuX/cluster */
		cpu_dev = get_cpu_device(cluster*2);
		ret = sysfs_create_link(&cpu_dev->kobj
		, &dir_clusterN_C[cluster]->kobj, "cluster");
		if (ret) {
			sysfs_remove_link(&dir_clusterN_C[cluster]->kobj
			, cpus[y]);
			sysfs_remove_link(&dir_clusterN_C[cluster]->kobj
			, cpus[x]);
			goto err_out;
		}
		cpu_dev = get_cpu_device(cluster*2+1);
		ret = sysfs_create_link(&cpu_dev->kobj
		, &dir_clusterN_C[cluster]->kobj, "cluster");
		if (ret) {
			cpu_dev = get_cpu_device(cluster*2);
			sysfs_remove_link(&cpu_dev->kobj, "cluster");
			sysfs_remove_link(&dir_clusterN_C[cluster]->kobj
			, cpus[y]);
			sysfs_remove_link(&dir_clusterN_C[cluster]->kobj
			, cpus[x]);
			goto err_out;
		}
	}

	return ret;

err_out:
	if (cluster == 1)
		remove_symbolic_links_C(0);
	return ret;
}

static int mt_cluster_stats_init(void)
{
	int ret = 0, ret2 = 0;
	ret = alloc_all_dir_stats();
	register_hotcpu_notifier(&dir_stats_cpu_notifer);
	ret2 = add_symbolic_links();
	return ret | ret2;
}


static void mt_cluster_stats_exit(void)
{
	remove_symbolic_links_C(1);
	remove_symbolic_links_C(0);
	unregister_hotcpu_notifier(&dir_stats_cpu_notifer);
	free_all_dir_stats_C(0);
	free_all_dir_stats_C(1);
	kobject_put(&cluster_kobj);
}


#ifdef MODULE
	module_init(mt_cluster_stats_init);
#else /* !MODULE */
	late_initcall(mt_cluster_stats_init);
#endif /* MODULE */

module_exit(mt_cluster_stats_exit);
