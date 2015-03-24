#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/cpufreq.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include <asm/smp_plat.h>

#include <mach/rq_stats.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#define ENABLE_RQ_LENGTH 1
#else
#define ENABLE_RQ_LENGTH 0
#endif


#define MAX_LONG_SIZE 24


#if ENABLE_RQ_LENGTH

#define DEFAULT_RQ_POLL_JIFFIES 1
#define DEFAULT_DEF_TIMER_JIFFIES 5

#else				/* !ENABLE_RQ_LENGTH */

struct rq_data rq_info;

#endif				/* ENABLE_RQ_LENGTH */


struct notifier_block freq_transition;
struct notifier_block cpu_hotplug;

struct cpu_load_data {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_iowait;
	unsigned int avg_load_maxfreq;
	unsigned int samples;
	unsigned int window_size;
	unsigned int cur_freq;
	unsigned int policy_max;
	cpumask_var_t related_cpus;
	spinlock_t cpu_load_lock;
};

static DEFINE_PER_CPU(struct cpu_load_data, cpuload);

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

/* MTK NOTE: */
/* get_cpu_idle_time() is defined and declared in cpufreq since kernel-3.10. */
/* Due to compatiblity concern (3.8 and 3.4), */
/* changing function name (to my_XXX) is used instead of using APIs of cpufreq. */

static inline cputime64_t my_get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

static int update_average_load(unsigned int freq, unsigned int cpu)
{

	struct cpu_load_data *pcpu = &per_cpu(cpuload, cpu);
	cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
	unsigned int idle_time, wall_time, iowait_time;
	unsigned int cur_load, load_at_max_freq;

	cur_idle_time = my_get_cpu_idle_time(cpu, &cur_wall_time);
	cur_iowait_time = get_cpu_iowait_time(cpu, &cur_wall_time);

	wall_time = (unsigned int)(cur_wall_time - pcpu->prev_cpu_wall);
	pcpu->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int)(cur_idle_time - pcpu->prev_cpu_idle);
	pcpu->prev_cpu_idle = cur_idle_time;

	iowait_time = (unsigned int)(cur_iowait_time - pcpu->prev_cpu_iowait);
	pcpu->prev_cpu_iowait = cur_iowait_time;

	if (idle_time >= iowait_time)
		idle_time -= iowait_time;

	if (unlikely(!wall_time || wall_time < idle_time))
		return 0;

	cur_load = 1000 * (wall_time - idle_time) / wall_time;

	/* Calculate the scaled load across CPU */
	load_at_max_freq = (cur_load * freq) / pcpu->policy_max;

	if (!pcpu->avg_load_maxfreq) {
		/* This is the first sample in this window */
		pcpu->avg_load_maxfreq = load_at_max_freq;
		pcpu->window_size = wall_time;
	} else {
		/*
		 * The is already a sample available in this window.
		 * Compute weighted average with prev entry, so that we get
		 * the precise weighted load.
		 */
		pcpu->avg_load_maxfreq =
		    ((pcpu->avg_load_maxfreq * pcpu->window_size) +
		     (load_at_max_freq * wall_time)) / (wall_time + pcpu->window_size);

		pcpu->window_size += wall_time;
	}

	return 0;
}

#if 0
static unsigned int report_load_at_max_freq(void)
{
	int cpu;
	unsigned long flags;
	struct cpu_load_data *pcpu;
	unsigned int total_load = 0;

	for_each_online_cpu(cpu) {
		pcpu = &per_cpu(cpuload, cpu);
		spin_lock_irqsave(&pcpu->cpu_load_lock, flags);
		update_average_load(pcpu->cur_freq, cpu);
		total_load += pcpu->avg_load_maxfreq;
		pcpu->avg_load_maxfreq = 0;
		spin_unlock_irqrestore(&pcpu->cpu_load_lock, flags);
	}
	return total_load;
}
#endif
static unsigned int report_load_per_cpu(char *buf, int max_len)
{
	int cpu;
	unsigned long flags;
	struct cpu_load_data *pcpu;
	unsigned int len = 0;

	for_each_possible_cpu(cpu) {
		pcpu = &per_cpu(cpuload, cpu);
		spin_lock_irqsave(&pcpu->cpu_load_lock, flags);
		update_average_load(pcpu->cur_freq, cpu);
		/* TODO: add iowait info */
		len +=
		    snprintf(buf + len, max_len - len, "%d %d\n", cpu_online(cpu),
			     pcpu->avg_load_maxfreq);
		pcpu->avg_load_maxfreq = 0;
		spin_unlock_irqrestore(&pcpu->cpu_load_lock, flags);
	}
	return len;

}

static int cpufreq_transition_handler(struct notifier_block *nb, unsigned long val, void *data)
{
	struct cpufreq_freqs *freqs = data;
	struct cpu_load_data *this_cpu = &per_cpu(cpuload, freqs->cpu);
	unsigned long flags;
	int j;


	switch (val) {
	case CPUFREQ_POSTCHANGE:
		for_each_cpu(j, this_cpu->related_cpus) {
			struct cpu_load_data *pcpu = &per_cpu(cpuload, j);
			spin_lock_irqsave(&pcpu->cpu_load_lock, flags);
			update_average_load(freqs->old, freqs->cpu);
			pcpu->cur_freq = freqs->new;
			spin_unlock_irqrestore(&pcpu->cpu_load_lock, flags);
		}
		break;
	}
	return 0;
}

static int cpu_hotplug_handler(struct notifier_block *nb, unsigned long val, void *data)
{
	unsigned int cpu = (unsigned long)data;
	struct cpu_load_data *this_cpu = &per_cpu(cpuload, cpu);

	switch (val) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		this_cpu->avg_load_maxfreq = 0;
	}

	return NOTIFY_OK;
}


#if ENABLE_RQ_LENGTH


static void def_work_fn(struct work_struct *work)
{
	int64_t diff;

	diff = ktime_to_ns(ktime_get()) - rq_info.def_start_time;
	do_div(diff, 1000 * 1000);
	rq_info.def_interval = (unsigned int)diff;

	/* Notify polling threads on change of value */
	sysfs_notify(rq_info.kobj, NULL, "def_timer_ms");
}

unsigned int get_run_queue_avg(void)
{
	unsigned int val = 0;
	unsigned long flags = 0;
	unsigned int avg_val = 0;
	spin_lock_irqsave(&rq_lock, flags);
	/* rq avg currently available only on one core */
	val = rq_info.rq_avg;
	/* rq_info.rq_avg = 0; */
	spin_unlock_irqrestore(&rq_lock, flags);

	avg_val = val / 10;
	if (val % 10)		/* get ceiling */
		avg_val += 1;
	if (avg_val == 0 || avg_val == 1)
		avg_val = 2;

	return avg_val;
}
EXPORT_SYMBOL(get_run_queue_avg);


static ssize_t run_queue_avg_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned int val = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&rq_lock, flags);
	/* rq avg currently available only on one core */
	val = rq_info.rq_avg;
	rq_info.rq_avg = 0;
	spin_unlock_irqrestore(&rq_lock, flags);

	return snprintf(buf, PAGE_SIZE, "%d.%d\n", val / 10, val % 10);
}

static struct kobj_attribute run_queue_avg_attr = __ATTR_RO(run_queue_avg);

static ssize_t show_run_queue_poll_ms(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&rq_lock, flags);
	ret = snprintf(buf, MAX_LONG_SIZE, "%u\n", jiffies_to_msecs(rq_info.rq_poll_jiffies));
	spin_unlock_irqrestore(&rq_lock, flags);

	return ret;
}

static ssize_t store_run_queue_poll_ms(struct kobject *kobj,
				       struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;
	unsigned long flags = 0;
	static DEFINE_MUTEX(lock_poll_ms);

	mutex_lock(&lock_poll_ms);

	spin_lock_irqsave(&rq_lock, flags);
	if (sscanf(buf, "%u", &val) == 1)
		rq_info.rq_poll_jiffies = msecs_to_jiffies(val);
	spin_unlock_irqrestore(&rq_lock, flags);

	mutex_unlock(&lock_poll_ms);

	return count;
}

static struct kobj_attribute run_queue_poll_ms_attr =
	__ATTR(run_queue_poll_ms, S_IWUSR | S_IRUSR, show_run_queue_poll_ms, store_run_queue_poll_ms);

static ssize_t show_def_timer_ms(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_LONG_SIZE, "%u\n", rq_info.def_interval);
}

static ssize_t store_def_timer_ms(struct kobject *kobj,
				  struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	if (sscanf(buf, "%u", &val) == 1) {
		rq_info.def_timer_jiffies = msecs_to_jiffies(val);
		rq_info.def_start_time = ktime_to_ns(ktime_get());
	}
	return count;
}

static struct kobj_attribute def_timer_ms_attr =
	__ATTR(def_timer_ms, S_IWUSR | S_IRUSR, show_def_timer_ms, store_def_timer_ms);


#else				/* !ENABLE_RQ_LENGTH */


/* dummy functions for sysfs nodes */

static ssize_t run_queue_avg_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "63.9\n");
}

static struct kobj_attribute run_queue_avg_attr = __ATTR_RO(run_queue_avg);

static ssize_t show_run_queue_poll_ms(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_LONG_SIZE, "200\n");
}

static ssize_t store_run_queue_poll_ms(struct kobject *kobj, struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	return count;
}

static struct kobj_attribute run_queue_poll_ms_attr =
	__ATTR(run_queue_poll_ms, S_IWUSR | S_IRUSR, show_run_queue_poll_ms, store_run_queue_poll_ms);

static ssize_t show_def_timer_ms(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_LONG_SIZE, "200\n");
}

static ssize_t store_def_timer_ms(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	return count;
}

static struct kobj_attribute def_timer_ms_attr =
	__ATTR(def_timer_ms, S_IWUSR | S_IRUSR, show_def_timer_ms, store_def_timer_ms);


#endif				/* ENABLE_RQ_LENGTH */


static ssize_t show_cpu_normalized_load(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	/* return snprintf(buf, MAX_LONG_SIZE, "%u\n", report_load_at_max_freq()); */
	return report_load_per_cpu(buf, 4096);
}

static struct kobj_attribute cpu_normalized_load_attr =
	__ATTR(cpu_normalized_load, S_IWUSR | S_IRUSR, show_cpu_normalized_load, NULL);

static struct attribute *rq_attrs[] = {
	&cpu_normalized_load_attr.attr,
	&def_timer_ms_attr.attr,
	&run_queue_avg_attr.attr,
	&run_queue_poll_ms_attr.attr,
	NULL,
};

static struct attribute_group rq_attr_group = {
	.attrs = rq_attrs,
};

static int init_rq_attribs(void)
{
	int err;

	rq_info.rq_avg = 0;
	rq_info.attr_group = &rq_attr_group;

	/* Create /sys/devices/system/cpu/cpu0/rq-stats/... */
	rq_info.kobj = kobject_create_and_add("rq-stats", &get_cpu_device(0)->kobj);
	if (!rq_info.kobj)
		return -ENOMEM;

	err = sysfs_create_group(rq_info.kobj, rq_info.attr_group);
	if (err)
		kobject_put(rq_info.kobj);
	else
		kobject_uevent(rq_info.kobj, KOBJ_ADD);

	return err;
}

static int __init __rq_stats_init(void)
{
	int ret;
	int i;
	struct cpufreq_policy cpu_policy;
	/* Bail out if this is not an SMP Target */
	if (!is_smp()) {
		rq_info.init = 0;
		return -ENOSYS;
	}
#if ENABLE_RQ_LENGTH

	rq_wq = create_singlethread_workqueue("rq_stats");
	BUG_ON(!rq_wq);
	INIT_WORK(&rq_info.def_timer_work, def_work_fn);
	spin_lock_init(&rq_lock);
	rq_info.rq_poll_jiffies = DEFAULT_RQ_POLL_JIFFIES;
	rq_info.def_timer_jiffies = DEFAULT_DEF_TIMER_JIFFIES;
	rq_info.rq_poll_last_jiffy = 0;
	rq_info.def_timer_last_jiffy = 0;

#endif				/* ENABLE_RQ_LENGTH */

	ret = init_rq_attribs();

	rq_info.init = 1;

	for_each_possible_cpu(i) {
		struct cpu_load_data *pcpu = &per_cpu(cpuload, i);
		spin_lock_init(&pcpu->cpu_load_lock);
		cpufreq_get_policy(&cpu_policy, i);
		pcpu->policy_max = cpu_policy.cpuinfo.max_freq;
		cpumask_copy(pcpu->related_cpus, cpu_policy.cpus);
	}
	freq_transition.notifier_call = cpufreq_transition_handler;
	cpu_hotplug.notifier_call = cpu_hotplug_handler;
	cpufreq_register_notifier(&freq_transition, CPUFREQ_TRANSITION_NOTIFIER);
	register_hotcpu_notifier(&cpu_hotplug);

	return ret;
}
late_initcall(__rq_stats_init);
