/************************************************************************************************
*
*  FileName:		drivers/cpufreq/cpu_hotplug_governor.c
*  Description:	hi3630 CPU hotplug governor
*  Author:		z00209631 <ziming.zhou@hisilicon.com>
*  Version:		v1.0
*  Date:			20140302
*  History:
*				cpufreq_hotplug v1.0
*					Author:	Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>
*							Alexey Starikovskiy <alexey.y.starikovskiy@intel.com>
*					Description: cpufreq_hotplug' - A dynamic cpufreq governor for
*							    Low Latency Frequency Transition capable processors
*
*  Copyright (C), 2014, Hisilicon Technologies Co., Ltd. All rights reserved.
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License version 2 as
*  published by the Free Software Foundation.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*************************************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/cpuidle.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/suspend.h>

#undef HI3630_CFG_DEBUG
#ifdef HI3630_CFG_DEBUG
#define HI3630_CFG_PRINT_ERR(format, args...)		do { printk(KERN_ERR format, args); } while (0)
#define HI3630_CFG_PRINT_INFO(format, args...)		do { printk(KERN_ERR format, args); } while (0)
#define HI3630_CFG_PRINT_DBG(format, args...)		do { ; } while (0)
/*#define HI3630_CFG_PRINT(x...)                                printk(x)*/
#else
#define HI3630_CFG_PRINT_ERR(format, args...)		do { printk(KERN_ERR format, args); } while (0)
#define HI3630_CFG_PRINT_INFO(format, args...)		do { ; } while (0)
#define HI3630_CFG_PRINT_DBG(format, args...)		do { ; } while (0)
#endif

/* dbs is used in this file as a shortform for demandbased switching. It helps to keep variable names smaller, simpler */

#define INIT_DELAYED_WORK_DEFERRABLE INIT_DEFERRABLE_WORK
#define CFGHP_CLUSTER_CORENUM (4)

#ifdef CONFIG_SCHED_HMP
#ifdef CONFIG_USE_BIG_CORE_DEPTH
#define HMP_POLICY_UP_THRESHOLD		(512)
#define HMP_POLICY_DOWN_THRESHOLD	(256)
#endif
#define HMP_POLICY_STATE_OFF			(0)
#define HMP_POLICY_STATE_ON			(1)
#define HMP_POLICY_PRIO_2				(2)

extern void sched_set_hmp_ratio_threshold(unsigned int up_threshold);
extern void sched_hmp_get_ratio(int *ratio_num);
extern int set_hmp_policy(const char *pname, int prio, int state, int up_thresholds, int down_thresholds);
#endif

extern void hisi_cpu_hotplug_lock(void);
extern void hisi_cpu_hotplug_unlock(void);

#ifdef CONFIG_HISI_BUS_SWITCH
extern int hisi_cpu_down(int cpuid);
extern int hisi_cpu_up(int cpuid);
#define cpu_hotplug_down(x)		hisi_cpu_down(x)
#define cpu_hotplug_up(x)		hisi_cpu_up(x)
#else
int cpu_hotplug_down(int cpuid)
{
	int ret = 0;

	HI3630_CFG_PRINT_DBG("[HI3630_CFG]-ERROR: function=%s, line=%d, cpu_hotplug_up--\n", __FUNCTION__, __LINE__);
	hisi_cpu_hotplug_lock();
	cpuidle_pause();
	ret = cpu_down(cpuid);
	cpuidle_resume();
	hisi_cpu_hotplug_unlock();
	kick_all_cpus_sync();
	HI3630_CFG_PRINT_DBG("[HI3630_CFG]-ERROR: function=%s, line=%d, cpu_hotplug_up++\n", __FUNCTION__, __LINE__);

	return ret;
}
int cpu_hotplug_up(int cpuid)
{
	int ret = 0;

	HI3630_CFG_PRINT_DBG("[HI3630_CFG]-ERROR: function=%s, line=%d, cpu_hotplug_up++\n", __FUNCTION__, __LINE__);
	hisi_cpu_hotplug_lock();
	ret = cpu_up(cpuid);
	hisi_cpu_hotplug_unlock();
	HI3630_CFG_PRINT_DBG("[HI3630_CFG]-ERROR: function=%s, line=%d, cpu_hotplug_up--\n", __FUNCTION__, __LINE__);

	return ret;
}
#endif

#define CPU_HP_GOV_RESUME					(3)
#define CPU_HP_GOV_SUSPEND				(2)
#define CPU_HP_GOV_START					(1)
#define CPU_HP_GOV_STOP					(0)

#define DEF_TRUE							(1)
#define DEF_FALSE							(0)

#define DEF_CPU_SAMPLING_RATE				(30000)
#define MICRO_CPU_SAMPLING_RATE			(30000)
#define MIN_CPU_SAMPLING_RATE				(30000)
#define MAX_CPU_SAMPLING_RATE				(1000000)

#define DEF_CPU_DOWN_DIFFERENTIAL		(20)		//(10)
#define MICRO_CPU_DOWN_DIFFERENTIAL		(20)		//(10)
#define MIN_CPU_DOWN_DIFFERENTIAL		(0)
#define MAX_CPU_DOWN_DIFFERENTIAL		(30)
#define DEF_CPU_UP_THRESHOLD				(80)		//(90)
#define MICRO_CPU_UP_THRESHOLD			(80)		//(90)
#define MIN_CPU_UP_THRESHOLD				(50)
#define MAX_CPU_UP_THRESHOLD				(100)

#define DEF_CPU_UP_AVG_TIMES				(5)		//(10)
#define MIN_CPU_UP_AVG_TIMES				(1)
#define MAX_CPU_UP_AVG_TIMES				(20)
#define DEF_CPU_DOWN_AVG_TIMES			(30)	//(100)
#define MIN_CPU_DOWN_AVG_TIMES			(20)
#define MAX_CPU_DOWN_AVG_TIMES			(200)

/*cpu input*/
#define DEF_CPU_INPUT_BOOST_NUM			(2)

/*cpu rush*/
#define DEF_CPU_RUSH_THRESHOLD			(90)		//(98)
#define MICRO_CPU_RUSH_THRESHOLD			(90)		//(98)
#define MIN_CPU_RUSH_THRESHOLD			(80)
#define MAX_CPU_RUSH_THRESHOLD			(100)
#define DEF_CPU_RUSH_AVG_TIMES			(3)		//(5)
#define MIN_CPU_RUSH_AVG_TIMES			(1)
#define MAX_CPU_RUSH_AVG_TIMES			(10)
#define DEF_CPU_RUSH_TLP_TIMES			(3)		//(5)
#define MIN_CPU_RUSH_TLP_TIMES			(1)
#define MAX_CPU_RUSH_TLP_TIMES			(10)

/*cpu hmp*/
#define DEF_CPU_HMP_THRESHOLD			(1020)	//(1020)
#define MICRO_CPU_HMP_THRESHOLD			(1020)	//(1020)
#define MIN_CPU_HMP_THRESHOLD			(512)
#define MAX_CPU_HMP_THRESHOLD			(1024)
#define DEF_CPU_HMP_AVG_TIMES				(3)		//(5)
#define MIN_CPU_HMP_AVG_TIMES				(1)
#define MAX_CPU_HMP_AVG_TIMES			(10)
#define DEF_CPU_HMP_TIMEOUT				(1000000)
#define MIN_CPU_HMP_TIMEOUT				(100000)
#define MAX_CPU_HMP_TIMEOUT				(20000000)
#define DEF_CPU_HMP_BASE				(2)
#define MIN_CPU_HMP_BASE				(1)
#define MAX_CPU_HMP_BASE				CFGHP_CLUSTER_CORENUM

/* cpu hotplug - enum */
typedef enum {
	CPU_HOTPLUG_WORK_TYPE_NONE = 0,
	CPU_HOTPLUG_WORK_TYPE_BASE,
	CPU_HOTPLUG_WORK_TYPE_LIMIT,
	CPU_HOTPLUG_WORK_TYPE_UP,
	CPU_HOTPLUG_WORK_TYPE_DOWN,
	CPU_HOTPLUG_WORK_TYPE_RUSH,
	CPU_HOTPLUG_WORK_TYPE_HMP,
} cpu_hotplug_work_type_t;


/* cpu hotplug - global variable, function declaration & definition */
static int g_cpus_sum_load_current = 0;   //set global for information purpose

static long g_cpu_up_sum_load = 0;
static int g_cpu_up_count = 0;
static int g_cpu_up_load_index = 0;
static long g_cpu_up_load_history[MAX_CPU_UP_AVG_TIMES] = {0};

static long g_cpu_down_sum_load = 0;
static int g_cpu_down_count = 0;
static int g_cpu_down_load_index = 0;
static long g_cpu_down_load_history[MAX_CPU_DOWN_AVG_TIMES] = {0};

static cpu_hotplug_work_type_t g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_NONE;
static unsigned int g_next_hp_action = 0;
static struct delayed_work hp_work;

/*cpu rush*/
static int g_rush_tlp_avg_current = 0;       //set global for information purpose
static int g_rush_tlp_avg_sum = 0;
static int g_rush_tlp_avg_count = 0;
static int g_rush_tlp_avg_index = 0;
static int g_rush_tlp_avg_average = 0;       //set global for information purpose
static int g_rush_tlp_avg_history[MAX_CPU_RUSH_TLP_TIMES] = {0};
static int g_rush_tlp_iowait_av = 0;
static int g_cpu_rush_count = 0;

/*cpu hmp*/
static unsigned long g_cpu_hmp_jiffies_timeout = 0;
static int g_cpu_hmp_count = 0;

static void hp_reset_strategy_nolock(void);
static void hp_reset_strategy(void);

struct cpu_dbs_info_s {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_iowait;
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_nice;
};

struct delayed_work dbs_info_work;
struct mutex timer_mutex;

static DEFINE_PER_CPU(struct cpu_dbs_info_s, hp_cpu_dbs_info);

/* dbs_hotplug protects all hotplug related global variables */
static DEFINE_MUTEX(hp_mutex);
DEFINE_MUTEX(hp_onoff_mutex);

static struct dbs_tuners {
	unsigned int sampling_rate;
	unsigned int io_is_busy;
	unsigned int ignore_nice;
	unsigned int cpu_up_threshold;
	unsigned int cpu_down_differential;
	unsigned int cpu_up_avg_times;
	unsigned int cpu_down_avg_times;
	unsigned int cpu_num_limit;
	unsigned int cpu_num_base;
	unsigned int cpu_hp_disable;
	unsigned int cpu_input_boost_enable;
	unsigned int cpu_input_boost_num;
	unsigned int cpu_rush_boost_enable;
	unsigned int cpu_rush_threshold;
	unsigned int cpu_rush_tlp_times;
	unsigned int cpu_rush_avg_times;
	unsigned int cpu_hmp_boost_enable;
	unsigned int cpu_hmp_threshold;
	unsigned int cpu_hmp_avg_times;
	unsigned int cpu_hmp_timeout;
	unsigned int cpu_hmp_base;
} dbs_tuners_ins = {
	.sampling_rate = DEF_CPU_SAMPLING_RATE,
	.ignore_nice = 0,
	.cpu_up_threshold = DEF_CPU_UP_THRESHOLD,
	.cpu_down_differential = DEF_CPU_DOWN_DIFFERENTIAL,
	.cpu_up_avg_times = DEF_CPU_UP_AVG_TIMES,
	.cpu_down_avg_times = DEF_CPU_DOWN_AVG_TIMES,
	.cpu_num_limit = CFGHP_CLUSTER_CORENUM * 2,
	.cpu_num_base = CFGHP_CLUSTER_CORENUM,
	.cpu_hp_disable = DEF_TRUE,
	.cpu_input_boost_enable = DEF_TRUE,
	.cpu_input_boost_num = DEF_CPU_INPUT_BOOST_NUM,
/*cpu rush*/
	.cpu_rush_boost_enable = DEF_TRUE,
	.cpu_rush_threshold = DEF_CPU_RUSH_THRESHOLD,
	.cpu_rush_tlp_times = DEF_CPU_RUSH_TLP_TIMES,
	.cpu_rush_avg_times = DEF_CPU_RUSH_AVG_TIMES,
/*cpu hmp*/
	.cpu_hmp_boost_enable = DEF_TRUE,
	.cpu_hmp_threshold = DEF_CPU_HMP_THRESHOLD,
	.cpu_hmp_avg_times = DEF_CPU_HMP_AVG_TIMES,
	.cpu_hmp_timeout = DEF_CPU_HMP_TIMEOUT,
	.cpu_hmp_base = DEF_CPU_HMP_BASE,
};


static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
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


static inline cputime64_t get_cpu_iowait_time(unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}


/************************** sysfs interface ************************/
/* CPU hotplug Governor Tunables */
#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{													\
	return sprintf(buf, "%u\n", dbs_tuners_ins.object);	\
}
show_one(sampling_rate, sampling_rate);
show_one(io_is_busy, io_is_busy);
show_one(ignore_nice_load, ignore_nice);
show_one(cpu_up_threshold, cpu_up_threshold);
show_one(cpu_down_differential, cpu_down_differential);
show_one(cpu_up_avg_times, cpu_up_avg_times);
show_one(cpu_down_avg_times, cpu_down_avg_times);
show_one(cpu_num_limit, cpu_num_limit);
show_one(cpu_num_base, cpu_num_base);
show_one(cpu_hp_disable, cpu_hp_disable);
show_one(cpu_input_boost_enable, cpu_input_boost_enable);
show_one(cpu_input_boost_num, cpu_input_boost_num);
show_one(cpu_rush_boost_enable, cpu_rush_boost_enable);
show_one(cpu_rush_threshold, cpu_rush_threshold);
show_one(cpu_rush_tlp_times, cpu_rush_tlp_times);
show_one(cpu_rush_avg_times, cpu_rush_avg_times);
show_one(cpu_hmp_boost_enable, cpu_hmp_boost_enable);
show_one(cpu_hmp_threshold, cpu_hmp_threshold);
show_one(cpu_hmp_avg_times, cpu_hmp_avg_times);
show_one(cpu_hmp_timeout, cpu_hmp_timeout);
show_one(cpu_hmp_base, cpu_hmp_base);


/**
 * update_sampling_rate - update sampling rate effective immediately if needed.
 * @new_rate: new sampling rate
 *
 * If new rate is smaller than the old, simply updaing
 * dbs_tuners_int.sampling_rate might not be appropriate. For example,
 * if the original sampling_rate was 1 second and the requested new sampling
 * rate is 10 ms because the user needs immediate reaction from hotplug
 * governor, but not sure if higher frequency will be required or not,
 * then, the governor may change the sampling rate too late; up to 1 second
 * later. Thus, if we are reducing the sampling rate, we need to make the
 * new value effective immediately.
 */
static void update_sampling_rate(unsigned int new_rate)
{
	unsigned long next_sampling, appointed_at;

	dbs_tuners_ins.sampling_rate = new_rate;
	mutex_lock(&timer_mutex);
	if (!delayed_work_pending(&dbs_info_work)) {
		mutex_unlock(&timer_mutex);
		return;
	}
	next_sampling  = jiffies + usecs_to_jiffies(new_rate);
	appointed_at = dbs_info_work.timer.expires;
	if (time_before(next_sampling, appointed_at)) {
		mutex_unlock(&timer_mutex);
		cancel_delayed_work_sync(&dbs_info_work);
		mutex_lock(&timer_mutex);
		HI3630_CFG_PRINT_INFO("[HI3630_CFG]-dbs_info_work: function=%s, line=%d\n", __FUNCTION__, __LINE__);
		schedule_delayed_work_on(0, &dbs_info_work, usecs_to_jiffies(new_rate));
	}
	mutex_unlock(&timer_mutex);
	return;
}


static ssize_t store_sampling_rate(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_CPU_SAMPLING_RATE || input < MIN_CPU_SAMPLING_RATE) {
		return -EINVAL;
	}
	update_sampling_rate(input);
	return count;
}


static ssize_t store_io_is_busy(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	dbs_tuners_ins.io_is_busy = !!input;
	return count;
}


static ssize_t store_ignore_nice_load(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	if (input > 1)
		input = 1;
	if (input == dbs_tuners_ins.ignore_nice) { /* nothing to do */
		return count;
	}
	dbs_tuners_ins.ignore_nice = input;
	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(hp_cpu_dbs_info, j);
		dbs_info->prev_cpu_idle = get_cpu_idle_time(j, &dbs_info->prev_cpu_wall, 0);
		if (dbs_tuners_ins.ignore_nice)
			dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];
	}
	return count;
}


static ssize_t store_cpu_up_threshold(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_CPU_UP_THRESHOLD || input < MIN_CPU_UP_THRESHOLD) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_up_threshold = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_down_differential(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_CPU_DOWN_DIFFERENTIAL || input < MIN_CPU_DOWN_DIFFERENTIAL) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_down_differential = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_up_avg_times(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_CPU_UP_AVG_TIMES || input < MIN_CPU_UP_AVG_TIMES) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_up_avg_times = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_down_avg_times(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_CPU_DOWN_AVG_TIMES || input < MIN_CPU_DOWN_AVG_TIMES) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_down_avg_times = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_num_limit(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > num_possible_cpus() || input < 1) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_num_limit = input;
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_num_base(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	unsigned int online_cpus_count;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > num_possible_cpus() || input < 1) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_num_base = input;
	online_cpus_count = num_online_cpus();
	if (online_cpus_count < input && online_cpus_count < dbs_tuners_ins.cpu_num_limit) {
		g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_BASE;
		HI3630_CFG_PRINT_INFO("[HI3630_CFG]-hp_work store_cpu_num_base: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_BASE\n", __FUNCTION__, __LINE__);
		schedule_delayed_work_on(0, &hp_work, 0);
	}
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_hp_disable(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || (input != DEF_TRUE && input != DEF_FALSE)) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	if (dbs_tuners_ins.cpu_hp_disable == DEF_TRUE && input == DEF_FALSE)
		hp_reset_strategy_nolock();
	dbs_tuners_ins.cpu_hp_disable = input;
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_input_boost_enable(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || (input != DEF_TRUE && input != DEF_FALSE)) {
		return -EINVAL;
	}

	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_input_boost_enable = input;
	mutex_unlock(&hp_mutex);

	return count;
}


static ssize_t store_cpu_input_boost_num(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > num_possible_cpus() ||input < 2) {
		return -EINVAL;
	}

	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_input_boost_num = input;
	mutex_unlock(&hp_mutex);

	return count;
}


static ssize_t store_cpu_rush_boost_enable(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || (input != DEF_TRUE && input != DEF_FALSE)) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_rush_boost_enable = input;
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_rush_threshold(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_CPU_RUSH_THRESHOLD || input < MIN_CPU_RUSH_THRESHOLD) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_rush_threshold = input;
	//hp_reset_strategy_nolock(); //no need
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_rush_tlp_times(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_CPU_RUSH_TLP_TIMES || input < MIN_CPU_RUSH_TLP_TIMES) {
		return -EINVAL;
	}

	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_rush_tlp_times = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);

	return count;
}


static ssize_t store_cpu_rush_avg_times(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_CPU_RUSH_AVG_TIMES || input < MIN_CPU_RUSH_AVG_TIMES) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_rush_avg_times = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_hmp_boost_enable(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || (input != DEF_TRUE && input != DEF_FALSE)) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_hmp_boost_enable = input;
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_hmp_threshold(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_CPU_HMP_THRESHOLD || input < MIN_CPU_HMP_THRESHOLD) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_hmp_threshold = input;
#ifdef CONFIG_SCHED_HMP
	sched_set_hmp_ratio_threshold(dbs_tuners_ins.cpu_hmp_threshold);
#endif
	//hp_reset_strategy_nolock(); //no need
	mutex_unlock(&hp_mutex);
	return count;
}

#ifdef CONFIG_SCHED_HMP
int change_cpu_hmp_threshold(unsigned int threshold)
{
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_hmp_threshold = threshold;
	sched_set_hmp_ratio_threshold(dbs_tuners_ins.cpu_hmp_threshold);
	mutex_unlock(&hp_mutex);

	return 0;
}
EXPORT_SYMBOL(change_cpu_hmp_threshold);
#endif


static ssize_t store_cpu_hmp_avg_times(struct kobject *a, struct attribute *b, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_CPU_HMP_AVG_TIMES || input < MIN_CPU_HMP_AVG_TIMES) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_hmp_avg_times = input;
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_hmp_timeout(struct kobject *a, struct attribute *b,const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_CPU_HMP_TIMEOUT || input < MIN_CPU_HMP_TIMEOUT) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_hmp_timeout = input;
	g_cpu_hmp_jiffies_timeout = jiffies + usecs_to_jiffies(dbs_tuners_ins.cpu_hmp_timeout);
	hp_reset_strategy_nolock();
	mutex_unlock(&hp_mutex);
	return count;
}


static ssize_t store_cpu_hmp_base(struct kobject *a, struct attribute *b,const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_CPU_HMP_BASE || input < MIN_CPU_HMP_BASE) {
		return -EINVAL;
	}
	mutex_lock(&hp_mutex);
	dbs_tuners_ins.cpu_hmp_base = input;
	mutex_unlock(&hp_mutex);
	return count;
}


define_one_global_rw(sampling_rate);
define_one_global_rw(io_is_busy);//okok
define_one_global_rw(ignore_nice_load);
define_one_global_rw(cpu_up_threshold);
define_one_global_rw(cpu_down_differential);
define_one_global_rw(cpu_up_avg_times);
define_one_global_rw(cpu_down_avg_times);
define_one_global_rw(cpu_num_limit);
define_one_global_rw(cpu_num_base);
define_one_global_rw(cpu_hp_disable);
define_one_global_rw(cpu_input_boost_enable);
define_one_global_rw(cpu_input_boost_num);
define_one_global_rw(cpu_rush_boost_enable);
define_one_global_rw(cpu_rush_threshold);
define_one_global_rw(cpu_rush_tlp_times);
define_one_global_rw(cpu_rush_avg_times);
define_one_global_rw(cpu_hmp_boost_enable);
define_one_global_rw(cpu_hmp_threshold);
define_one_global_rw(cpu_hmp_avg_times);
define_one_global_rw(cpu_hmp_timeout);
define_one_global_rw(cpu_hmp_base);


static struct attribute *dbs_attributes[] = {
	&sampling_rate.attr,
	&io_is_busy.attr,
	&ignore_nice_load.attr,
	&cpu_up_threshold.attr,
	&cpu_down_differential.attr,
	&cpu_up_avg_times.attr,
	&cpu_down_avg_times.attr,
	&cpu_num_limit.attr,
	&cpu_num_base.attr,
	&cpu_hp_disable.attr,
	&cpu_input_boost_enable.attr,
	&cpu_input_boost_num.attr,
	&cpu_rush_boost_enable.attr,
	&cpu_rush_threshold.attr,
	&cpu_rush_tlp_times.attr,
	&cpu_rush_avg_times.attr,
	&cpu_hmp_boost_enable.attr,
	&cpu_hmp_threshold.attr,
	&cpu_hmp_avg_times.attr,
	&cpu_hmp_timeout.attr,
	&cpu_hmp_base.attr,
	NULL
};


static struct attribute_group dbs_attr_group = {
	.attrs = dbs_attributes,
	.name = "cpuhotplug",
};


#ifdef CONFIG_SCHED_RQ_AVG_INFO
extern void sched_get_nr_running_avg(int *avg, int *iowait_avg);
#else /* CONFIG_SCHED_RQ_AVG_INFO */
static void sched_get_nr_running_avg(int *avg, int *iowait_avg)
{
	*avg = num_possible_cpus() * 100;
}
#endif /* CONFIG_SCHED_RQ_AVG_INFO */


static void hp_reset_strategy_nolock(void)
{
	g_cpu_up_count = 0;
	g_cpu_up_sum_load = 0;
	g_cpu_up_load_index = 0;
	g_cpu_up_load_history[dbs_tuners_ins.cpu_up_avg_times - 1] = 0;

	g_cpu_down_count = 0;
	g_cpu_down_sum_load = 0;
	g_cpu_down_load_index = 0;
	g_cpu_down_load_history[dbs_tuners_ins.cpu_down_avg_times - 1] = 0;

	g_rush_tlp_avg_sum = 0;
	g_rush_tlp_avg_count = 0;
	g_rush_tlp_avg_index = 0;
	g_rush_tlp_avg_history[dbs_tuners_ins.cpu_rush_tlp_times - 1] = 0;

	g_cpu_rush_count = 0;
	g_cpu_hmp_count = 0;

	g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_NONE;
}


static void hp_reset_strategy(void)
{
	mutex_lock(&hp_mutex);

	hp_reset_strategy_nolock();

	mutex_unlock(&hp_mutex);
}


static void hp_work_handler(struct work_struct *work)
{
	if (mutex_trylock(&hp_onoff_mutex)) {
		if (dbs_tuners_ins.cpu_hp_disable == DEF_FALSE) {
			unsigned int online_cpus_count = num_online_cpus();
			unsigned int i;
			int ret;

			HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, hp_work_handler++ (g_next_hp_action %d)(online_cpus_count %d)(smp_processor_id %d)\n", __FUNCTION__, __LINE__, g_next_hp_action, online_cpus_count, smp_processor_id());

			switch (g_trigger_hp_work) {
			case CPU_HOTPLUG_WORK_TYPE_HMP:
				for (i = online_cpus_count; i < min(g_next_hp_action, dbs_tuners_ins.cpu_num_limit); ++i) {
					HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_HMP cpu_up++(i %d)\n", __FUNCTION__, __LINE__, i);
					ret = cpu_hotplug_up(i);
					if (ret) {
						pr_err("%s cannot bring up cpu%d\n", __func__, i);
						break;
					}
					HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_HMP cpu_up--(i %d)\n", __FUNCTION__, __LINE__, i);
				}
				break;
			case CPU_HOTPLUG_WORK_TYPE_RUSH:
				for (i = online_cpus_count; i < min(g_next_hp_action, dbs_tuners_ins.cpu_num_limit); ++i) {
					HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_RUSH cpu_up++(i %d)\n", __FUNCTION__, __LINE__, i);
					ret = cpu_hotplug_up(i);
					if (ret) {
						pr_err("%s cannot bring up cpu%d\n", __func__, i);
						break;
					}
					HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_RUSH cpu_up--(i %d)\n", __FUNCTION__, __LINE__, i);
				}
				break;
			case CPU_HOTPLUG_WORK_TYPE_BASE:
				for (i = online_cpus_count; i < min(dbs_tuners_ins.cpu_num_base, dbs_tuners_ins.cpu_num_limit); ++i) {
					HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_BASE cpu_up++(i %d)\n", __FUNCTION__, __LINE__, i);
					ret = cpu_hotplug_up(i);
					if (ret) {
						pr_err("%s cannot bring up cpu%d\n", __func__, i);
						break;
					}
					HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_BASE cpu_up--(i %d)\n", __FUNCTION__, __LINE__, i);
				}
				break;
			case CPU_HOTPLUG_WORK_TYPE_LIMIT:
				for (i = online_cpus_count - 1; i >= dbs_tuners_ins.cpu_num_limit; --i) {
					HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_LIMIT cpu_down++(i %d)\n", __FUNCTION__, __LINE__, i);
					ret = cpu_hotplug_down(i);
					if (ret) {
						pr_err("%s cannot take down cpu%d\n", __func__, i);
						break;
					}
					HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_LIMIT cpu_down++(i %d)\n", __FUNCTION__, __LINE__, i);
				}
				break;
			case CPU_HOTPLUG_WORK_TYPE_UP:
				for (i = online_cpus_count; i < g_next_hp_action; ++i) {
					HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_UP cpu_up++(i %d)\n", __FUNCTION__, __LINE__, i);
					ret = cpu_hotplug_up(i);
					if (ret) {
						pr_err("%s cannot bring up cpu%d\n", __func__, i);
						break;
					}
					HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_UP cpu_up--(i %d)\n", __FUNCTION__, __LINE__, i);
				}
				break;
			case CPU_HOTPLUG_WORK_TYPE_DOWN:
				for (i = online_cpus_count - 1; i >= g_next_hp_action; --i) {
					HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_DOWN cpu_down++(i %d)\n", __FUNCTION__, __LINE__, i);
					ret = cpu_hotplug_down(i);
					if (ret) {
						pr_err("%s cannot take down cpu%d\n", __func__, i);
						break;
					}
					HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_DOWN cpu_down--(i %d)\n", __FUNCTION__, __LINE__, i);
				}
				break;
			default:
#ifdef CFGHP_CLUSTER_CORENUM
				HI3630_CFG_PRINT_ERR("[HI3630_CFG]-ERROR default: function=%s, line=%d\n", __FUNCTION__, __LINE__);
#else
				for (i = online_cpus_count; i < min(dbs_tuners_ins.cpu_input_boost_num, dbs_tuners_ins.cpu_num_limit); ++i) {
					HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, default cpu_up++(i %d)\n", __FUNCTION__, __LINE__, i);
					ret = cpu_hotplug_up(i);
					if (ret) {
						pr_err("%s cannot bring up cpu%d\n", __func__, i);
						break;
					}
					HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, default cpu_up++(i %d)\n", __FUNCTION__, __LINE__, i);
				}
#endif
				break;
			}
			hp_reset_strategy();
		}else {
			HI3630_CFG_PRINT_DBG("[HI3630_CFG]-WARNING cpu_hp_disable: function=%s, line=%d\n", __FUNCTION__, __LINE__);
		}
		mutex_unlock(&hp_onoff_mutex);
	}
	HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, hp_work_handler--\n", __FUNCTION__, __LINE__);
}


static void dbs_check_cpu(void)
{
	unsigned int j;
	long cpus_sum_load_last_up = 0;
	long cpus_sum_load_last_down = 0;
	unsigned int online_cpus_count;
	int v_rush_tlp_avg_last = 0;
#ifdef CONFIG_SCHED_HMP
	int ratio_num = 0;
#ifdef CONFIG_USE_BIG_CORE_DEPTH
	static int set_hmp = 0;
#endif
#endif

	/* Get Absolute Load*/
	g_cpus_sum_load_current = 0;

	for_each_online_cpu(j) {
		struct cpu_dbs_info_s *j_dbs_info;
		cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
		unsigned int idle_time, wall_time, iowait_time;
		unsigned int load;

		j_dbs_info = &per_cpu(hp_cpu_dbs_info, j);

		cur_wall_time = 0;
		cur_idle_time = get_cpu_idle_time(j, &cur_wall_time, 0);
		cur_iowait_time = get_cpu_iowait_time(j, &cur_wall_time);

		wall_time = (unsigned int) (cur_wall_time - j_dbs_info->prev_cpu_wall);
		j_dbs_info->prev_cpu_wall = cur_wall_time;

		idle_time = (unsigned int) (cur_idle_time - j_dbs_info->prev_cpu_idle);
		j_dbs_info->prev_cpu_idle = cur_idle_time;

		iowait_time = (unsigned int) (cur_iowait_time - j_dbs_info->prev_cpu_iowait);
		j_dbs_info->prev_cpu_iowait = cur_iowait_time;

		if (dbs_tuners_ins.ignore_nice) {
			u64 cur_nice;
			unsigned long cur_nice_jiffies;

			cur_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE] - j_dbs_info->prev_cpu_nice;
			/* Assumption: nice time between sampling periods will be less than 2^32 jiffies for 32 bit sys */
			cur_nice_jiffies = (unsigned long) cputime64_to_jiffies64(cur_nice);

			j_dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];
			idle_time += jiffies_to_usecs(cur_nice_jiffies);
			HI3630_CFG_PRINT_INFO("[HI3630_CFG]-WARNING dbs_tuners_ins.ignore_nice: function=%s, line=%d\n", __FUNCTION__, __LINE__);
		}

		/* For the purpose of hotplug, waiting for disk IO is an indication that you're performance critical, and not that the system is actually idle. So subtract the iowait time from the cpu idle time. */
		if (dbs_tuners_ins.io_is_busy && idle_time >= iowait_time) {
			idle_time -= iowait_time;
		}

		if (unlikely(!wall_time || wall_time < idle_time)) {
			HI3630_CFG_PRINT_DBG("[HI3630_CFG]-WARNING: function=%s, line=%d, !wall_time || wall_time < idle_time)\n", __FUNCTION__, __LINE__);
			continue;
		}

		load = 100 * (wall_time - idle_time) / wall_time;
		g_cpus_sum_load_current += load;

#ifdef HI3630_CFG_DEBUG
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, (cpu %d)(wall_time %d)(idle_time %d)(load %d)(g_cpus_sum_load_current %d)\n", __FUNCTION__, __LINE__, j, wall_time, idle_time, load, g_cpus_sum_load_current);
#endif
	}

	/* If Hot Plug policy disable, return directly */
	if (dbs_tuners_ins.cpu_hp_disable == DEF_TRUE) {
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]-WARNING dbs_tuners_ins.cpu_hp_disable: function=%s, line=%d, dbs_check_cpu--\n", __FUNCTION__, __LINE__);
		return;
	}

	if (g_trigger_hp_work != CPU_HOTPLUG_WORK_TYPE_NONE) {
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]-BUSY: function=%s, line=%d, dbs_check_cpu--\n", __FUNCTION__, __LINE__);
		return;
	}

	mutex_lock(&hp_mutex);

	online_cpus_count = num_online_cpus();

	HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, (online_cpus_count %d)(smp_processor_id() %d)\n", __FUNCTION__, __LINE__, online_cpus_count, smp_processor_id());

	/*limit*/
	if (online_cpus_count > dbs_tuners_ins.cpu_num_limit) {
		g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_LIMIT;
		HI3630_CFG_PRINT_INFO("[HI3630_CFG]-hp_work limit: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_LIMIT, (online_cpus_count %d)\n", __FUNCTION__, __LINE__, online_cpus_count);
		schedule_delayed_work_on(0, &hp_work, 0);
		goto hp_check_end;
	}

	/*rush*/
	sched_get_nr_running_avg(&g_rush_tlp_avg_current, &g_rush_tlp_iowait_av);
	v_rush_tlp_avg_last = g_rush_tlp_avg_history[g_rush_tlp_avg_index];
	g_rush_tlp_avg_history[g_rush_tlp_avg_index] = g_rush_tlp_avg_current;
	g_rush_tlp_avg_sum += g_rush_tlp_avg_current;

	g_rush_tlp_avg_index = (g_rush_tlp_avg_index + 1 == dbs_tuners_ins.cpu_rush_tlp_times)? 0 : g_rush_tlp_avg_index + 1;
	g_rush_tlp_avg_count++;
	if (g_rush_tlp_avg_count >= dbs_tuners_ins.cpu_rush_tlp_times) {
		if (g_rush_tlp_avg_sum > v_rush_tlp_avg_last)
			g_rush_tlp_avg_sum -= v_rush_tlp_avg_last;
		else
			g_rush_tlp_avg_sum = 0;
	}
	g_rush_tlp_avg_average = g_rush_tlp_avg_sum / dbs_tuners_ins.cpu_rush_tlp_times;
	HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, RUSH get (g_rush_tlp_avg_current %d)(g_rush_tlp_avg_sum %d)(g_rush_tlp_avg_average %d)\n", __FUNCTION__, __LINE__, g_rush_tlp_avg_current, g_rush_tlp_avg_sum, g_rush_tlp_avg_average);

	if (dbs_tuners_ins.cpu_rush_boost_enable == DEF_TRUE) {
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, RUSH need (g_cpus_sum_load_current %d) > (dbs_tuners_ins.cpu_rush_threshold * online_cpus_count %d)\n", __FUNCTION__, __LINE__, g_cpus_sum_load_current, dbs_tuners_ins.cpu_rush_threshold * online_cpus_count);
		if (g_cpus_sum_load_current > dbs_tuners_ins.cpu_rush_threshold * online_cpus_count)
			++g_cpu_rush_count;
		else
			g_cpu_rush_count = 0;
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, RUSH get (g_cpu_rush_count %d)\n", __FUNCTION__, __LINE__, g_cpu_rush_count);
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, RUSH need (g_rush_tlp_avg_average %d) > (online_cpus_count * 100 %d)\n", __FUNCTION__, __LINE__, g_rush_tlp_avg_average, online_cpus_count * 100);
		if ((g_cpu_rush_count >= dbs_tuners_ins.cpu_rush_avg_times)
		     && (online_cpus_count * 100 < g_rush_tlp_avg_average)
		     && (online_cpus_count < dbs_tuners_ins.cpu_num_limit)
		     && (online_cpus_count < num_possible_cpus())) {
			g_next_hp_action = (g_rush_tlp_avg_average + 100 - 1) / 100;
			if (g_next_hp_action > dbs_tuners_ins.cpu_num_limit)
				g_next_hp_action = dbs_tuners_ins.cpu_num_limit;
			if (g_next_hp_action > num_possible_cpus())
				g_next_hp_action = num_possible_cpus();
			g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_RUSH;
			HI3630_CFG_PRINT_INFO("[HI3630_CFG]-hp_work rush: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_RUSH (g_next_hp_action %d)(online_cpus_count %d)\n", __FUNCTION__, __LINE__, g_next_hp_action, online_cpus_count);
			schedule_delayed_work_on(0, &hp_work, 0);
			goto hp_check_end;
		}
	}else {
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]-WARNING !dbs_tuners_ins.cpu_rush_boost_enable: function=%s, line=%d\n", __FUNCTION__, __LINE__);
	}

	/*hmp*/
#ifdef CONFIG_SCHED_HMP
	if (dbs_tuners_ins.cpu_hmp_boost_enable == DEF_TRUE && CFGHP_CLUSTER_CORENUM < dbs_tuners_ins.cpu_num_limit) {
		sched_hmp_get_ratio(&ratio_num);
		if (ratio_num > 0) {
			if (ratio_num > 1)
				g_cpu_hmp_count = dbs_tuners_ins.cpu_hmp_avg_times;
			else
				++g_cpu_hmp_count;
			ratio_num = max(dbs_tuners_ins.cpu_hmp_base, (unsigned int)ratio_num);
			ratio_num += CFGHP_CLUSTER_CORENUM;
			g_cpu_hmp_jiffies_timeout = jiffies + usecs_to_jiffies(dbs_tuners_ins.cpu_hmp_timeout);
		} else {
			g_cpu_hmp_count = 0;
		}
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, HMP check (ratio_num %d)(g_cpu_hmp_count %d)\n", __FUNCTION__, __LINE__, ratio_num, g_cpu_hmp_count);
		if (g_cpu_hmp_count >= dbs_tuners_ins.cpu_hmp_avg_times) {
		#ifdef CONFIG_USE_BIG_CORE_DEPTH
			if (!set_hmp && (HMP_POLICY_UP_THRESHOLD < dbs_tuners_ins.cpu_hmp_threshold)) {
				set_hmp_policy("cpuhotplug", HMP_POLICY_PRIO_2, HMP_POLICY_STATE_ON, HMP_POLICY_UP_THRESHOLD, HMP_POLICY_DOWN_THRESHOLD);
				set_hmp = 1;
			}
		#endif
			if ((online_cpus_count < dbs_tuners_ins.cpu_num_limit)
				&& (online_cpus_count < num_possible_cpus())
				&& (online_cpus_count < ratio_num)) {
				g_next_hp_action = ratio_num;
				if (g_next_hp_action > dbs_tuners_ins.cpu_num_limit)
					g_next_hp_action = dbs_tuners_ins.cpu_num_limit;
				if (g_next_hp_action > num_possible_cpus())
					g_next_hp_action = num_possible_cpus();
				g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_HMP;
				HI3630_CFG_PRINT_INFO("[HI3630_CFG]-hp_work hmp: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_HMP (g_next_hp_action %d)(online_cpus_count %d)\n", __FUNCTION__, __LINE__, g_next_hp_action, online_cpus_count);
				schedule_delayed_work_on(0, &hp_work, 0);
				goto hp_check_end;
			}
		}
	} else {
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]-WARNING: function=%s, line=%d, disbale hmp\n", __FUNCTION__, __LINE__);
	}
#endif

	/*base*/
	if (online_cpus_count < dbs_tuners_ins.cpu_num_base && online_cpus_count < dbs_tuners_ins.cpu_num_limit) {
		g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_BASE;
		HI3630_CFG_PRINT_INFO("[HI3630_CFG]-hp_work base: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_BASE, (g_next_hp_action %d)(online_cpus_count %d)\n", __FUNCTION__, __LINE__, g_next_hp_action, online_cpus_count);
		schedule_delayed_work_on(0, &hp_work, 0);
		goto hp_check_end;
	}

	/*up*/
	if (online_cpus_count < num_possible_cpus()) {
		cpus_sum_load_last_up = g_cpu_up_load_history[g_cpu_up_load_index];
		g_cpu_up_load_history[g_cpu_up_load_index] = g_cpus_sum_load_current;
		g_cpu_up_sum_load += g_cpus_sum_load_current;

		g_cpu_up_count++;
		g_cpu_up_load_index = (g_cpu_up_load_index + 1 == dbs_tuners_ins.cpu_up_avg_times) ? 0 : g_cpu_up_load_index + 1;

		/* ready to add cpu, please don't decrease cpu */
		if (g_cpus_sum_load_current > dbs_tuners_ins.cpu_up_threshold * online_cpus_count) {
			g_cpu_down_count = 0;
			g_cpu_down_sum_load = 0;
			g_cpu_down_load_index = 0;
			g_cpu_down_load_history[dbs_tuners_ins.cpu_down_avg_times - 1] = 0;
		}

		if (g_cpu_up_count >= dbs_tuners_ins.cpu_up_avg_times) {
			if (g_cpu_up_sum_load > cpus_sum_load_last_up)
				g_cpu_up_sum_load -= cpus_sum_load_last_up;
			else
				g_cpu_up_sum_load = 0;

			HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, UP need (g_cpu_up_sum_load %ld) > (%ld)\n", __FUNCTION__, __LINE__, g_cpu_up_sum_load, (long)(dbs_tuners_ins.cpu_up_threshold * online_cpus_count  * dbs_tuners_ins.cpu_up_avg_times));
			if (g_cpu_up_sum_load > (dbs_tuners_ins.cpu_up_threshold * online_cpus_count  * dbs_tuners_ins.cpu_up_avg_times)) {
				if (online_cpus_count < dbs_tuners_ins.cpu_num_limit) {
					g_next_hp_action = online_cpus_count + 1;
					g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_UP;
					HI3630_CFG_PRINT_INFO("[HI3630_CFG]-hp_work up: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_UP (g_next_hp_action %d)(online_cpus_count %d)\n", __FUNCTION__, __LINE__, g_next_hp_action, online_cpus_count);
					schedule_delayed_work_on(0, &hp_work, 0);
					goto hp_check_end;
				}
			}
		}
	}

	/*down*/
	if (online_cpus_count > 1) {
		cpus_sum_load_last_down = g_cpu_down_load_history[g_cpu_down_load_index];
		g_cpu_down_load_history[g_cpu_down_load_index] = g_cpus_sum_load_current;
		g_cpu_down_sum_load += g_cpus_sum_load_current;

		g_cpu_down_count++;
		g_cpu_down_load_index = (g_cpu_down_load_index + 1 == dbs_tuners_ins.cpu_down_avg_times)? 0 : g_cpu_down_load_index + 1;

		if (g_cpu_down_count >= dbs_tuners_ins.cpu_down_avg_times) {
			long cpu_down_threshold;
			if (g_cpu_down_sum_load > cpus_sum_load_last_down)
				g_cpu_down_sum_load -= cpus_sum_load_last_down;
			else
				g_cpu_down_sum_load = 0;

			if (time_after(jiffies, g_cpu_hmp_jiffies_timeout)) {
#ifdef CONFIG_USE_BIG_CORE_DEPTH
				if (set_hmp) {
					set_hmp = 0;
					set_hmp_policy("cpuhotplug", HMP_POLICY_PRIO_2, HMP_POLICY_STATE_OFF, HMP_POLICY_UP_THRESHOLD, HMP_POLICY_DOWN_THRESHOLD);
				}
#endif
				g_next_hp_action = online_cpus_count;
				cpu_down_threshold = ((dbs_tuners_ins.cpu_up_threshold - dbs_tuners_ins.cpu_down_differential) * dbs_tuners_ins.cpu_down_avg_times);
				HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, DOWN need (g_cpu_down_sum_load %ld) < (%ld)\n", __FUNCTION__, __LINE__, g_cpu_down_sum_load, cpu_down_threshold * online_cpus_count);
				while ((g_cpu_down_sum_load < cpu_down_threshold * (g_next_hp_action - 1)) && (g_next_hp_action > dbs_tuners_ins.cpu_num_base)) {
					--g_next_hp_action;
				}

				if (g_next_hp_action < online_cpus_count) {
#ifdef CONFIG_SCHED_HMP
					if (CFGHP_CLUSTER_CORENUM < dbs_tuners_ins.cpu_num_limit) {
						if (ratio_num > 0) {
							HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, DOWN check (g_next_hp_action %d) ~ (ratio_num %d)\n", __FUNCTION__, __LINE__, g_next_hp_action, ratio_num);
							if (g_next_hp_action < ratio_num) {
								g_cpu_hmp_jiffies_timeout = jiffies + usecs_to_jiffies(dbs_tuners_ins.cpu_hmp_timeout);
								if (online_cpus_count <= ratio_num) {
									goto hp_check_end;
								}
								g_next_hp_action = ratio_num;
							}
						}
					}
#endif
					g_trigger_hp_work = CPU_HOTPLUG_WORK_TYPE_DOWN;
					HI3630_CFG_PRINT_INFO("[HI3630_CFG]-hp_work down: function=%s, line=%d, CPU_HOTPLUG_WORK_TYPE_DOWN, (g_next_hp_action %d)(online_cpus_count %d)\n", __FUNCTION__, __LINE__, g_next_hp_action, online_cpus_count);
					schedule_delayed_work_on(0, &hp_work, 0);
				}
			}
		}
	}

hp_check_end:
	mutex_unlock(&hp_mutex);
	HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, dbs_check_cpu--\n", __FUNCTION__, __LINE__);

	return;
}


static void do_dbs_timer(struct work_struct *work)
{
	unsigned long delay;

	mutex_lock(&timer_mutex);
	dbs_check_cpu();
	delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);
	schedule_delayed_work_on(0, &dbs_info_work, delay);
	mutex_unlock(&timer_mutex);
}


static inline void dbs_timer_init(void)
{
	unsigned long delay;

	HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, dbs_timer_init++ (smp_processor_id %d)\n", __FUNCTION__, __LINE__, smp_processor_id());
	INIT_DELAYED_WORK_DEFERRABLE(&dbs_info_work, do_dbs_timer);
	delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);
	schedule_delayed_work_on(0, &dbs_info_work, delay);
	HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, dbs_timer_init--\n", __FUNCTION__, __LINE__);
}


static inline void dbs_timer_exit(void)
{
	HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, dbs_timer_exit++ (smp_processor_id %d)\n", __FUNCTION__, __LINE__, smp_processor_id());
	cancel_delayed_work_sync(&dbs_info_work);
	HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, dbs_timer_exit--\n", __FUNCTION__, __LINE__);
}


/*
 * Not all CPUs want IO time to be accounted as busy; this dependson how efficient idling at a higher frequency/voltage is.
 * Pavel Machek says this is not so for various generations of AMD and old Intel systems.
 * Mike Chan (androidlcom) calis this is also not true for ARM.
 * Because of this, whitelist specific known (series) of CPUs by default, and leave all others up to the user.
 */
static int should_io_be_busy(void)
{
#if defined(CONFIG_X86)
	/* For Intel, Core 2 (model 15) andl later have an efficient idle. */
	if (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL && boot_cpu_data.x86 == 6 && boot_cpu_data.x86_model >= 15)
		return 1;
#endif
	return 1; // io wait time should be subtracted from idle time
}


#ifndef CFGHP_CLUSTER_CORENUM
static void dbs_input_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, dbs_input_event++\n", __FUNCTION__, __LINE__);
	if (type == EV_KEY && code == BTN_TOUCH && value == 1 && dbs_tuners_ins.cpu_input_boost_enable == DEF_TRUE) {
		unsigned int online_cpus_count = num_online_cpus();
		if (online_cpus_count < dbs_tuners_ins.cpu_input_boost_num && online_cpus_count < dbs_tuners_ins.cpu_num_limit) {
			HI3630_CFG_PRINT_INFO("[HI3630_CFG]-hp_work input: function=%s, line=%d\n", __FUNCTION__, __LINE__);
			schedule_delayed_work_on(0, &hp_work, 0);
		}
	}
	HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, dbs_input_event--\n", __FUNCTION__, __LINE__);
}


static int dbs_input_connect(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, dbs_input_connect++(handler %d)(dev %d)(id %d)\n", __FUNCTION__, __LINE__, (int)handler, (int)dev, (int)id);

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "cpufreq";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, dbs_input_connect--\n", __FUNCTION__, __LINE__);
	return 0;
err1:
	pr_err("[HI3630_CFG]-ERROR: function=%s, line=%d, err1\n", __FUNCTION__, __LINE__);
	input_unregister_handle(handle);
err2:
	pr_err("[HI3630_CFG]-ERROR: function=%s, line=%d, err2\n", __FUNCTION__, __LINE__);
	kfree(handle);
	return error;
}


static void dbs_input_disconnect(struct input_handle *handle)
{
	HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, dbs_input_disconnect++(handler %d)\n", __FUNCTION__, __LINE__, (int)handle);
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
	HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, dbs_input_disconnect--\n", __FUNCTION__, __LINE__);
}


static const struct input_device_id dbs_ids[] = {
	{ .driver_info = 1 },
	{ },
};


static struct input_handler dbs_input_handler = {
	.event		= dbs_input_event,
	.connect		= dbs_input_connect,
	.disconnect	= dbs_input_disconnect,
	.name		= "cpufreq_ond",
	.id_table		= dbs_ids,
};
#endif


static int cpu_hp_governor_dbs(unsigned int event)
{
	unsigned int cpu = 0;
	unsigned int j;
	int rc;
	unsigned long delay;

	HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, cpu_hp_governor_dbs++\n", __FUNCTION__, __LINE__);

	switch (event) {
	case CPU_HP_GOV_START:
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HP_GOV_START\n", __FUNCTION__, __LINE__);
		if (!cpu_online(cpu))
			return -EINVAL;

		for_each_online_cpu(j) {
			struct cpu_dbs_info_s *j_dbs_info;
			j_dbs_info = &per_cpu(hp_cpu_dbs_info, j);
			j_dbs_info->prev_cpu_idle = get_cpu_idle_time(j, &j_dbs_info->prev_cpu_wall, 0);
			if (dbs_tuners_ins.ignore_nice)
				j_dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];
		}
		rc = sysfs_create_group(&cpu_subsys.dev_root->kobj, &dbs_attr_group);
		if (rc) {
			pr_err("[HI3630_CFG]-ERROR: function=%s, line=%d\n", __FUNCTION__, __LINE__);
			return rc;
		}
#ifndef CFGHP_CLUSTER_CORENUM
		HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, input_register_handler\n", __FUNCTION__, __LINE__);
		rc = input_register_handler(&dbs_input_handler);
#endif
		mutex_init(&timer_mutex);
		dbs_timer_init();
		break;

	case CPU_HP_GOV_RESUME:
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HP_GOV_RESUME\n", __FUNCTION__, __LINE__);
#ifndef CFGHP_CLUSTER_CORENUM
		HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, input_register_handler\n", __FUNCTION__, __LINE__);
		rc = input_register_handler(&dbs_input_handler);
#endif
		mutex_lock(&timer_mutex);
		delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);
		schedule_delayed_work_on(0, &dbs_info_work, delay);
		mutex_unlock(&timer_mutex);
		break;

	case CPU_HP_GOV_SUSPEND:
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HP_GOV_SUSPEND\n", __FUNCTION__, __LINE__);
		mutex_lock(&timer_mutex);
		if (!delayed_work_pending(&dbs_info_work)) {
			mutex_unlock(&timer_mutex);
		} else {
			mutex_unlock(&timer_mutex);
			cancel_delayed_work_sync(&dbs_info_work);
		}
		mutex_lock(&hp_mutex);
		if (!delayed_work_pending(&hp_work)) {
			mutex_unlock(&hp_mutex);
		} else {
			mutex_unlock(&hp_mutex);
			cancel_delayed_work_sync(&hp_work);
		}

#ifndef CFGHP_CLUSTER_CORENUM
		HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, input_unregister_handler\n", __FUNCTION__, __LINE__);
		input_unregister_handler(&dbs_input_handler);
#endif
		break;

	case CPU_HP_GOV_STOP:
		HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, CPU_HP_GOV_STOP\n", __FUNCTION__, __LINE__);
		dbs_timer_exit();
		mutex_destroy(&timer_mutex);
#ifndef CFGHP_CLUSTER_CORENUM
		HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, input_unregister_handler\n", __FUNCTION__, __LINE__);
		input_unregister_handler(&dbs_input_handler);
#endif
		sysfs_remove_group(&cpu_subsys.dev_root->kobj, &dbs_attr_group);
		break;

	}

	HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, cpu_hp_governor_dbs--\n", __FUNCTION__, __LINE__);
	return 0;
}

static int hisi_cpu_hotplug_notify(struct notifier_block *this,
				      unsigned long code, void *unused)
{
	if (code == SYS_HALT || code == SYS_POWER_OFF) {
		/* disable the hotplug governor */
		cpu_hp_governor_dbs(CPU_HP_GOV_STOP);
	}

	return NOTIFY_DONE;
}

static struct notifier_block hisi_cpu_hotplug_gov_notifier = {
	.notifier_call = hisi_cpu_hotplug_notify,
};

static int cpuhotplug_gov_pm_callback(struct notifier_block *nb,
			unsigned long action, void *ptr)
{

	switch (action) {
	case PM_SUSPEND_PREPARE:
		cpu_hp_governor_dbs(CPU_HP_GOV_SUSPEND);
		break;

	case PM_POST_SUSPEND:
		cpu_hp_governor_dbs(CPU_HP_GOV_RESUME);
		break;

	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block pm_notifer_block = {
	.notifier_call = cpuhotplug_gov_pm_callback,
};


static int __init cpu_hp_gov_dbs_init(void)
{
#ifdef CONFIG_HOTPLUG_CPU
	u64 idle_time;
	int cpu = get_cpu();

	HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, cpu_hp_gov_dbs_init++\n", __FUNCTION__, __LINE__);
	g_cpu_hmp_jiffies_timeout = jiffies;
	idle_time = get_cpu_idle_time_us(cpu, NULL);
	put_cpu();
	if (idle_time != -1ULL) {
		/* Idle micro accounting is supported. Use finer thresholds */
		dbs_tuners_ins.sampling_rate = MICRO_CPU_SAMPLING_RATE;
		dbs_tuners_ins.cpu_up_threshold = MICRO_CPU_UP_THRESHOLD;
		dbs_tuners_ins.cpu_down_differential = MICRO_CPU_DOWN_DIFFERENTIAL;
		dbs_tuners_ins.cpu_rush_threshold = MICRO_CPU_RUSH_THRESHOLD;
		dbs_tuners_ins.cpu_hmp_threshold = MICRO_CPU_HMP_THRESHOLD;
#ifdef CONFIG_SCHED_HMP
		sched_set_hmp_ratio_threshold(dbs_tuners_ins.cpu_hmp_threshold);
#endif
	} else {
		HI3630_CFG_PRINT_ERR("[HI3630_CFG]-ERROR: function=%s, line=%d\n", __FUNCTION__, __LINE__);
	}
	dbs_tuners_ins.io_is_busy = should_io_be_busy();
	dbs_tuners_ins.cpu_num_limit = num_possible_cpus();
	dbs_tuners_ins.cpu_num_base = CFGHP_CLUSTER_CORENUM;
	if (dbs_tuners_ins.cpu_num_limit <= dbs_tuners_ins.cpu_num_base) {
		dbs_tuners_ins.cpu_num_base = dbs_tuners_ins.cpu_num_limit;
	}
	/* first we will close hotplug, and we will open it after system booted. */
	dbs_tuners_ins.cpu_hp_disable = DEF_TRUE;
	register_reboot_notifier(&hisi_cpu_hotplug_gov_notifier);
	register_pm_notifier(&pm_notifer_block);

	INIT_DELAYED_WORK_DEFERRABLE(&hp_work, hp_work_handler);
	g_next_hp_action = num_online_cpus();
#ifdef HI3630_CFG_DEBUG
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.sampling_rate = %d\n", dbs_tuners_ins.sampling_rate);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.io_is_busy = %d\n", dbs_tuners_ins.io_is_busy);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_up_threshold = %d\n", dbs_tuners_ins.cpu_up_threshold);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_down_differential = %d\n", dbs_tuners_ins.cpu_down_differential);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_up_avg_times = %d\n", dbs_tuners_ins.cpu_up_avg_times);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_down_avg_times = %d\n", dbs_tuners_ins.cpu_down_avg_times);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_num_limit = %d\n", dbs_tuners_ins.cpu_num_limit);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_num_base = %d\n", dbs_tuners_ins.cpu_num_base);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_hp_disable = %d\n", dbs_tuners_ins.cpu_hp_disable);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_input_boost_enable = %d\n", dbs_tuners_ins.cpu_input_boost_enable);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_input_boost_num = %d\n", dbs_tuners_ins.cpu_input_boost_num);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_rush_boost_enable = %d\n", dbs_tuners_ins.cpu_rush_boost_enable);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_rush_threshold = %d\n", dbs_tuners_ins.cpu_rush_threshold);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_rush_tlp_times = %d\n", dbs_tuners_ins.cpu_rush_tlp_times);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_rush_avg_times = %d\n", dbs_tuners_ins.cpu_rush_avg_times);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_hmp_boost_enable = %d\n", dbs_tuners_ins.cpu_hmp_boost_enable);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_hmp_threshold = %d\n", dbs_tuners_ins.cpu_hmp_threshold);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_hmp_avg_times = %d\n", dbs_tuners_ins.cpu_hmp_avg_times);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_hmp_timeout = %d\n", dbs_tuners_ins.cpu_hmp_timeout);
	printk("cpu_hp_gov_dbs_init: dbs_tuners_ins.cpu_hmp_base = %d\n", dbs_tuners_ins.cpu_hmp_base);
#endif
	HI3630_CFG_PRINT_DBG("[HI3630_CFG]: function=%s, line=%d, cpu_hp_gov_dbs_init--\n", __FUNCTION__, __LINE__);
	return cpu_hp_governor_dbs(CPU_HP_GOV_START);
#else
	return 0;
#endif
}


static void __exit cpu_hp_gov_dbs_exit(void)
{
#ifdef CONFIG_HOTPLUG_CPU
	HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, cpu_hp_gov_dbs_exit++\n", __FUNCTION__, __LINE__);
	cancel_delayed_work_sync(&hp_work);
	cpu_hp_governor_dbs(CPU_HP_GOV_STOP);
	unregister_reboot_notifier(&hisi_cpu_hotplug_gov_notifier);
	unregister_pm_notifier(&pm_notifer_block);
	HI3630_CFG_PRINT_INFO("[HI3630_CFG]: function=%s, line=%d, cpu_hp_gov_dbs_exit--\n", __FUNCTION__, __LINE__);
#endif
}


MODULE_AUTHOR("z00209631 <ziming.zhou@hisilicon.com>");
MODULE_DESCRIPTION("'cpu_hp_hotplug' - A dynamic cpu hotplug governor");
MODULE_LICENSE("GPL");


/*fs_initcall(cpu_hp_gov_dbs_init);*/
module_init(cpu_hp_gov_dbs_init);
module_exit(cpu_hp_gov_dbs_exit);
