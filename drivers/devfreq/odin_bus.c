#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <linux/opp.h>
#include <linux/devfreq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/pm_qos.h>
#include <linux/cpu.h>
#include <linux/odin_gpufreq.h>

#include "odin_bus_monitor.h"
#include "governor.h"

#define MAX_FREQ_TARGET 4

enum odin_bus_type {
	ODIN_BUS_TYPE_MEM,
	ODIN_BUS_TYPE_CCI,
	ODIN_BUS_TYPE_MAX,
};

struct odin_clk_info {
	struct list_head list;
	struct clk *clk;
	unsigned long max_freq;
};

struct busfreq_data {
	enum odin_bus_type type;
	struct device *dev;
	struct devfreq *devfreq;
	bool disabled;
	unsigned long cur_freq;

	struct list_head clks;

	int qos_class;
	struct pm_qos_request qos_req[3];

	struct notifier_block pm_notifier;
	struct notifier_block qos_notifier;
	struct notifier_block cpuidle_notifier;
	struct notifier_block cpufreq_notifier;
	struct notifier_block gpufreq_notifier;

	unsigned long max_kbps;
	unsigned long qos_timeout_us;
	unsigned long ca15_freq_target[MAX_FREQ_TARGET];
	unsigned long ca7_freq_target[MAX_FREQ_TARGET];
	unsigned long gpu_freq_target[MAX_FREQ_TARGET];

	unsigned long gpu_weight;
	unsigned long cpu_weight;

	struct mutex lock;
};

static unsigned long odin_mem_clk_table[] = {
	800000,
	600000,
	400000,
	300000,
	200000,
	150000,
	0,
};

static unsigned long odin_cci_clk_table[] = {
	400000,
	200000,
	100000,
	50000,
	25000,
	0,
};

/*
 * set   == active
 * clear == idle
 */
static cpumask_t ca7_idle;
static cpumask_t ca15_idle;

static unsigned int cpufreq[8];
static unsigned int gpufreq;

static unsigned int cur_freq[ODIN_BUS_TYPE_MAX];

/**
 *
 * devfreq ->status/->target Functions
 *
 **/

static int odin_mem_get_dev_status(struct device *dev,
				   struct devfreq_dev_status *stat)
{
	struct busfreq_data *data = dev_get_drvdata(dev);

	stat->current_frequency = data->cur_freq;
	stat->busy_time  = max(odin_current_mem_bandwidth(),
		odin_normalized_mem_bandwidth()) >> 10; /* scale to kbps */

	if (data->cpu_weight)
		stat->busy_time += div_u64(odin_normalized_cpu_bandwidth() *
					data->cpu_weight, 100) >> 10;

	if (data->gpu_weight)
		stat->busy_time += div_u64(odin_normalized_gpu_bandwidth() *
					data->gpu_weight, 100) >> 10;

	stat->total_time = div_u64((u64)data->max_kbps * data->cur_freq,
		odin_mem_clk_table[0]);

	return 0;
}

static int odin_cci_get_dev_status(struct device *dev,
				   struct devfreq_dev_status *stat)
{
	int div;
	struct busfreq_data *data = dev_get_drvdata(dev);

	div = fls(odin_cci_clk_table[0] / data->cur_freq) - 1;

	stat->current_frequency = data->cur_freq;
	stat->busy_time  = max(odin_current_cci_bandwidth(),
		odin_normalized_cci_bandwidth()) >> 10;
	stat->total_time = data->max_kbps >> div;

	return 0;
}

static int odin_bus_set_clk(struct busfreq_data *data,
			    unsigned long freq)
{
	int div;
	struct odin_clk_info *first;
	struct odin_clk_info *info;

	/* first entry is the main clock that devfreq controls */
	first = list_first_entry(&data->clks, struct odin_clk_info, list);
	clk_set_rate(first->clk, freq * 1000);

	div = fls(first->max_freq / freq) - 1;

	/* all others, if any, move togther (^2 divisor) with the main clock */
	list_for_each_entry(info, &data->clks, list) {
		if (info == first)
			continue;

		clk_set_rate(info->clk, (info->max_freq >> div) * 1000);
	}

	return 0;
}

static int odin_bus_target(struct device *dev, unsigned long *_freq,
			   u32 flags)
{
	int err = 0;
	struct busfreq_data *data = dev_get_drvdata(dev);
	struct opp *opp;
	unsigned long old_freq = data->cur_freq;
	unsigned long new_freq;

	rcu_read_lock();
	opp = devfreq_recommended_opp(dev, _freq, flags);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		return PTR_ERR(opp);
	}
	new_freq = opp_get_freq(opp);
	rcu_read_unlock();

	if (old_freq == new_freq)
		return 0;

	dev_dbg(dev, "targeting %lukHz\n", new_freq);

	mutex_lock(&data->lock);

	if (data->disabled)
		goto out;

	err = odin_bus_set_clk(data, new_freq);
	if (err)
		goto out;

	data->cur_freq = new_freq;
	cur_freq[data->type] = new_freq;
out:
	mutex_unlock(&data->lock);
	return err;
}

/**
 *
 * Notifier Call-backs
 *
 **/

static int odin_busfreq_pm_notifier_event(struct notifier_block *this,
					  unsigned long event, void *ptr)
{
	int err;
	struct odin_clk_info *info;
	struct busfreq_data *data = container_of(this, struct busfreq_data,
						 pm_notifier);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&data->lock);
		data->disabled = true;

		info = list_first_entry(&data->clks, struct odin_clk_info, list);

		err = odin_bus_set_clk(data, info->max_freq);
		if (err)
			goto out;

		data->cur_freq = info->max_freq;
	out:
		mutex_unlock(&data->lock);
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		mutex_lock(&data->lock);
		data->disabled = false;
		mutex_unlock(&data->lock);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static int odin_busfreq_qos_notifier_event(struct notifier_block *nb,
					   unsigned long curr_value, void *null)
{
	struct busfreq_data *data = container_of(nb, struct busfreq_data,
						 qos_notifier);

#ifdef CONFIG_DEVFREQ_GOV_USERSPACE
	/* ignore QoS when using userspace governor */
	if (strcmp(data->devfreq->governor_name, "userspace") == 0)
		return NOTIFY_DONE;
#endif

	/* when QoS changes, apply min_freq change right away */
	mutex_lock(&data->devfreq->lock);
	data->devfreq->min_freq = curr_value;
	if (curr_value > data->cur_freq)
		update_devfreq(data->devfreq);
	else
		odin_bus_monitor_kick();
	mutex_unlock(&data->devfreq->lock);
	return NOTIFY_OK;
}

static int odin_busfreq_cpuidle_notifier_event(struct notifier_block *nb,
					       unsigned long event, void *data)
{
	cpumask_t *idle;
	unsigned int cpu = smp_processor_id();

	if (cpu_topology[cpu].socket_id)
		idle = &ca7_idle;
	else
		idle = &ca15_idle;

	switch (event) {
	case IDLE_START:
		cpumask_clear_cpu(cpu, idle);
		break;
	case IDLE_END:
		cpumask_set_cpu(cpu, idle);
		break;
	}

	return NOTIFY_OK;
}

static void update_qos(unsigned long *freq_table, unsigned long *target,
		       struct pm_qos_request *qos, unsigned int new_freq,
		       unsigned long qos_timeout)
{
	while (*freq_table && *target) {
		if (new_freq >= *target) {
			if (!qos_timeout)
				pm_qos_update_request(qos, *freq_table);
			else
				pm_qos_update_request_timeout(qos, *freq_table, qos_timeout);
			return;
		}
		freq_table++;
		target++;
	}

	pm_qos_update_request(qos, PM_QOS_DEFAULT_VALUE);
}

static int odin_busfreq_cpufreq_notifier_event(struct notifier_block *nb,
					       unsigned long action, void *data)
{
	int i;
	struct cpufreq_freqs *freqs = data;
	struct busfreq_data *bus_data = container_of(nb, struct busfreq_data,
						     cpufreq_notifier);
	struct pm_qos_request *qos;
	struct cpufreq_policy *policy;
	unsigned long *freq_table;
	unsigned long *target;

	if (freqs->old < freqs->new && action != CPUFREQ_PRECHANGE)
		return NOTIFY_DONE;

	if (freqs->old > freqs->new && action != CPUFREQ_POSTCHANGE)
		return NOTIFY_DONE;

	if (freqs->cpu != cpumask_first(&cpu_topology[freqs->cpu].core_sibling))
		return NOTIFY_DONE;

	if (bus_data->type == ODIN_BUS_TYPE_MEM)
		freq_table = odin_mem_clk_table;
	else if (bus_data->type == ODIN_BUS_TYPE_CCI)
		freq_table = odin_cci_clk_table;
	else
		BUG_ON(1);

	policy  = cpufreq_cpu_get(freqs->cpu);
	if (!policy)
		return NOTIFY_DONE;

	/* related_cpus freq move together */
	for_each_cpu(i, policy->related_cpus)
		cpufreq[i] = freqs->new;

	cpufreq_cpu_put(policy);

	if (cpu_topology[freqs->cpu].socket_id) {
		int active, total;

		target = bus_data->ca7_freq_target;
		qos    = &bus_data->qos_req[1];

		active = cpumask_weight(&ca7_idle);
		total  = cpumask_weight(&cpu_topology[freqs->cpu].core_sibling);

		/* too many cores are idle */
		if (active <= (total >> 1))
			goto out;
	} else {
		target = bus_data->ca15_freq_target;
		qos    = &bus_data->qos_req[0];
	}
out:
	update_qos(freq_table, target, qos, freqs->new, bus_data->qos_timeout_us);
	return NOTIFY_OK;
}

static int odin_busfreq_gpufreq_notifier_event(struct notifier_block *nb,
					       unsigned long action, void *data)
{
	struct odin_gpufreq_freqs *freqs = data;
	struct busfreq_data *bus_data = container_of(nb, struct busfreq_data,
						     gpufreq_notifier);
	unsigned long *freq_table;

	if (freqs->old < freqs->new && action != ODIN_GPUFREQ_PRECHANGE)
		return NOTIFY_DONE;

	if (freqs->old > freqs->new && action != ODIN_GPUFREQ_POSTCHANGE)
		return NOTIFY_DONE;

	if (bus_data->type == ODIN_BUS_TYPE_MEM)
		freq_table = odin_mem_clk_table;
	else if (bus_data->type == ODIN_BUS_TYPE_CCI)
		freq_table = odin_cci_clk_table;
	else
		BUG_ON(1);

	gpufreq = freqs->new / 1000;

	update_qos(freq_table, bus_data->gpu_freq_target, &bus_data->qos_req[2],
		gpufreq, bus_data->qos_timeout_us);

	return NOTIFY_OK;
}

/**
 *
 * SYSFS
 *
 **/

#define SHOW_STORE_BUSFREQ_DATA(name)                      \
static ssize_t show_##name(struct device *dev,             \
			   struct device_attribute *attr,  \
			   char *buf)                      \
{                                                          \
	struct busfreq_data *data = dev_get_drvdata(dev);  \
	return sprintf(buf, "%lu\n", data->name);          \
}                                                          \
static ssize_t store_##name(struct device *dev,            \
			    struct device_attribute *attr, \
			    const char *buf, size_t count) \
{                                                          \
	unsigned long val;                                 \
	struct busfreq_data *data = dev_get_drvdata(dev);  \
	if (!kstrtoul(buf, 0, &val)) {                     \
		data->name = val;                          \
		return count;                              \
	}                                                  \
	return 0;                                          \
}                                                          \
static DEVICE_ATTR(name, 0644, show_##name, store_##name)

SHOW_STORE_BUSFREQ_DATA(max_kbps);
SHOW_STORE_BUSFREQ_DATA(qos_timeout_us);
SHOW_STORE_BUSFREQ_DATA(cpu_weight);
SHOW_STORE_BUSFREQ_DATA(gpu_weight);

#define SHOW_STORE_FREQ_TARGET(name)                                     \
static ssize_t show_##name##_freq_target(struct device *dev,             \
					 struct device_attribute *attr,  \
					 char *buf)                      \
{                                                                        \
	int i;                                                           \
	int pos = 0;                                                     \
	struct busfreq_data *data = dev_get_drvdata(dev);                \
                                                                         \
	for (i = 0; i < MAX_FREQ_TARGET; i++) {                          \
		if (!data->name##_freq_target[i])                        \
			break;                                           \
                                                                         \
		pos += sprintf(buf + pos, "%lu ", data->name##_freq_target[i]); \
	}                                                                \
                                                                         \
	buf[pos++] = '\n';                                               \
	buf[pos] = '\0';                                                 \
	return pos;                                                      \
}                                                                        \
static ssize_t store_##name##_freq_target(struct device *dev,            \
					  struct device_attribute *attr, \
					  const char *buf, size_t count) \
{                                                                        \
	int i;                                                           \
	unsigned long val[MAX_FREQ_TARGET];                              \
	struct busfreq_data *data = dev_get_drvdata(dev);                \
                                                                         \
	memset(val, 0, sizeof(val));                                     \
	/* TODO: this is not dynamic .. */                               \
	sscanf(buf, "%lu %lu %lu %lu", &val[0], &val[1], &val[2], &val[3]); \
                                                                         \
	for (i = 0; i < MAX_FREQ_TARGET; i++)                            \
		data->name##_freq_target[i] = val[i];                    \
                                                                         \
	return count;                                                    \
}                                                                        \
static DEVICE_ATTR(name##_freq_target, 0644, show_##name##_freq_target,  \
		   store_##name##_freq_target)

SHOW_STORE_FREQ_TARGET(ca15);
SHOW_STORE_FREQ_TARGET(ca7);
SHOW_STORE_FREQ_TARGET(gpu);

/**
 *
 * Device Initialization
 *
 **/

static struct odin_clk_info *alloc_odin_clk_info(const char *name,
						 unsigned long max_freq)
{
	struct odin_clk_info *info;

	info = kzalloc(sizeof(struct odin_clk_info), GFP_KERNEL);
	if (info == NULL)
		return NULL;

	info->clk = clk_get(NULL, name);
	info->max_freq = max_freq;
	return info;
}

/* mem specific clk init */
static int odin_mem_init_clk(struct busfreq_data *data)
{
	struct odin_clk_info *info;

	INIT_LIST_HEAD(&data->clks);

	info = alloc_odin_clk_info("memc_clk", odin_mem_clk_table[0]);
	list_add(&info->list, &data->clks);

	info = alloc_odin_clk_info("scfg", 200000);
	list_add_tail(&info->list, &data->clks);

	info = alloc_odin_clk_info("snoc", 200000);
	list_add_tail(&info->list, &data->clks);

	return 0;
}

/* cci specific clk init */
static int odin_cci_init_clk(struct busfreq_data *data)
{
	struct odin_clk_info *info;

	INIT_LIST_HEAD(&data->clks);

	info = alloc_odin_clk_info("cclk", odin_cci_clk_table[0]);
	list_add(&info->list, &data->clks);

	info = alloc_odin_clk_info("hclk", 200000);
	list_add_tail(&info->list, &data->clks);

	return 0;
}

static int odin_bus_init_tables(struct busfreq_data *data, unsigned long *table)
{
	int i;
	int err;
	struct device *dev = data->dev;

	i = 0;
	while (table[i]) {
		err = opp_add(dev, table[i], 0);
		if (err) {
			dev_err(dev, "Cannot add opp entries.\n");
			return err;
		}
		i++;
	}

	return 0;
}

static int odin_bus_init_notifiers(struct busfreq_data *data)
{
	int err;
	struct device *dev = data->dev;

	devfreq_register_opp_notifier(dev, data->devfreq);

	err = register_pm_notifier(&data->pm_notifier);
	if (err) {
		dev_err(dev, "Failed to setup pm notifier\n");
		return err;
	}

	pm_qos_add_request(&data->qos_req[0], data->qos_class, 0);
	pm_qos_add_request(&data->qos_req[1], data->qos_class, 0);
	pm_qos_add_request(&data->qos_req[2], data->qos_class, 0);

	err = pm_qos_add_notifier(data->qos_class, &data->qos_notifier);
	if (err) {
		dev_err(dev, "Failed to setup qos notifier\n");
		return err;
	}

	idle_notifier_register(&data->cpuidle_notifier);

	err = cpufreq_register_notifier(&data->cpufreq_notifier,
					CPUFREQ_TRANSITION_NOTIFIER);
	if (err) {
		dev_err(dev, "Failed to setup cpufreq notifier\n");
		return err;
	}

	err = odin_gpufreq_register_notifier(&data->gpufreq_notifier);
	if (err) {
		dev_err(dev, "Failed to setup gpufreq notifier\n");
		return err;
	}

	return 0;
}

static void odin_bus_exit(struct device *dev)
{
	struct busfreq_data *data = dev_get_drvdata(dev);

	devfreq_unregister_opp_notifier(dev, data->devfreq);
}

unsigned int odin_get_memclk(void)
{
	return cur_freq[ODIN_BUS_TYPE_MEM];
}

unsigned int odin_get_cciclk(void)
{
	return cur_freq[ODIN_BUS_TYPE_CCI];
}

static struct devfreq_dev_profile odin_mem_dev_profile = {
	.initial_freq	= 800000,
	.polling_ms	= 100,
	.target		= odin_bus_target,
	.get_dev_status	= odin_mem_get_dev_status,
	.exit		= odin_bus_exit,
};

static struct devfreq_dev_profile odin_cci_dev_profile = {
	.initial_freq	= 400000,
	.polling_ms	= 100,
	.target		= odin_bus_target,
	.get_dev_status	= odin_cci_get_dev_status,
	.exit		= odin_bus_exit,
};

struct devfreq *odin_devfreq[2];

static int odin_busfreq_probe(struct platform_device *pdev)
{
	struct busfreq_data *data;
	struct opp *opp;
	struct device *dev = &pdev->dev;
	struct devfreq_dev_profile *profile;
	int err = 0;

	data = devm_kzalloc(&pdev->dev, sizeof(struct busfreq_data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(dev, "Cannot allocate memory.\n");
		return -ENOMEM;
	}

	/* mem or cci ? */
	data->type = pdev->id_entry->driver_data;
	data->dev  = dev;
	mutex_init(&data->lock);

	/* TODO: separate out mem vs cci notifier callbacks if necessary */
	data->pm_notifier.notifier_call      = odin_busfreq_pm_notifier_event;
	data->qos_notifier.notifier_call     = odin_busfreq_qos_notifier_event;
	data->cpuidle_notifier.notifier_call = odin_busfreq_cpuidle_notifier_event;
	data->cpufreq_notifier.notifier_call = odin_busfreq_cpufreq_notifier_event;
	data->gpufreq_notifier.notifier_call = odin_busfreq_gpufreq_notifier_event;

	if (data->type == ODIN_BUS_TYPE_MEM) {
		data->qos_class = PM_QOS_ODIN_MEM_MIN_FREQ;
		data->max_kbps = 4200000;

		data->ca15_freq_target[0] = 1308000;
		data->ca15_freq_target[1] = ~0;
		data->ca15_freq_target[2] = 1092000; /* CA15 hispeed  */
		data->ca7_freq_target[0]  = ~0;
		data->ca7_freq_target[1]  = ~0;
		data->ca7_freq_target[2]  = 996000; /* CA7 hispeed */

		odin_bus_init_tables(data, odin_mem_clk_table);
		odin_mem_init_clk(data);
		profile = &odin_mem_dev_profile;
		data->cpu_weight = 50;
		data->gpu_weight = 0;
		err |= device_create_file(dev, &dev_attr_cpu_weight);
		err |= device_create_file(dev, &dev_attr_gpu_weight);
	} else if (data->type == ODIN_BUS_TYPE_CCI) {
		data->qos_class = PM_QOS_ODIN_CCI_MIN_FREQ;
		data->max_kbps = 2000000;

		data->ca15_freq_target[0] = 1092000; /* CA15 hispeed  */
		data->ca15_freq_target[1] = 900000;
		data->ca7_freq_target[0]  = ~0;
		data->ca7_freq_target[1]  = 996000; /* CA7 hispeed */

		odin_bus_init_tables(data, odin_cci_clk_table);
		odin_cci_init_clk(data);
		profile = &odin_cci_dev_profile;
	} else {
		BUG_ON(1);
	}

	data->gpu_freq_target[0]  = 240000;
	data->gpu_freq_target[1]  = 180000;
	data->qos_timeout_us      = 100000;

	err |= device_create_file(dev, &dev_attr_max_kbps);
	err |= device_create_file(dev, &dev_attr_qos_timeout_us);
	err |= device_create_file(dev, &dev_attr_ca15_freq_target);
	err |= device_create_file(dev, &dev_attr_ca7_freq_target);
	err |= device_create_file(dev, &dev_attr_gpu_freq_target);

	if (err)
		dev_err(dev, "Failed to create sysfs files\n");

	rcu_read_lock();
	opp = opp_find_freq_floor(dev, &profile->initial_freq);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "Invalid initial frequency %lu kHz.\n",
			profile->initial_freq);
		return PTR_ERR(opp);
	}
	data->cur_freq = opp_get_freq(opp);
	rcu_read_unlock();

	platform_set_drvdata(pdev, data);

	/*
	 * default to performance for quick boot time, at the end of android init,
	 * it will need to change to powersave.
	 */
	data->devfreq = devfreq_add_device(dev, profile, "performance", NULL);
	if (IS_ERR(data->devfreq))
		return PTR_ERR(data->devfreq);
	odin_devfreq[data->type] = data->devfreq;

	err = odin_bus_init_notifiers(data);
	if (err) {
		dev_err(dev, "Failed to setup notifiers\n");
		devfreq_remove_device(data->devfreq);
		return err;
	}

	return 0;
}

static int odin_busfreq_remove(struct platform_device *pdev)
{
	struct busfreq_data *data = platform_get_drvdata(pdev);
	struct odin_clk_info *info, *n;

	list_for_each_entry_safe(info, n, &data->clks, list) {
		list_del(&info->list);
		kfree(info);
	}

	unregister_pm_notifier(&data->pm_notifier);
	pm_qos_remove_notifier(data->qos_class, &data->qos_notifier);
	cpufreq_unregister_notifier(&data->cpufreq_notifier,
				CPUFREQ_TRANSITION_NOTIFIER);
	odin_gpufreq_unregister_notifier(&data->gpufreq_notifier);
	idle_notifier_unregister(&data->cpuidle_notifier);
	devfreq_remove_device(data->devfreq);

	return 0;
}

static int odin_busfreq_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops odin_busfreq_pm = {
	.resume	= odin_busfreq_resume,
};

static const struct platform_device_id odin_busfreq_id[] = {
	{ "odin-mem", ODIN_BUS_TYPE_MEM },
	{ "odin-cci", ODIN_BUS_TYPE_CCI },
	{ },
};

static struct platform_driver odin_busfreq_driver = {
	.probe	= odin_busfreq_probe,
	.remove	= odin_busfreq_remove,
	.id_table = odin_busfreq_id,
	.driver = {
		.name	= "odin-busfreq",
		.owner	= THIS_MODULE,
		.pm	= &odin_busfreq_pm,
	},
};

static struct platform_device odin_mem_device = {
	.name = "odin-mem",
};

static struct platform_device odin_cci_device = {
	.name = "odin-cci",
};

static struct platform_device *odin_busfreq_devices[] __initdata = {
	&odin_mem_device,
	&odin_cci_device,
};

static int __init odin_busfreq_init(void)
{
	int err;

	err = platform_driver_register(&odin_busfreq_driver);
	if (err)
		return err;

	err = platform_add_devices(odin_busfreq_devices,
				ARRAY_SIZE(odin_busfreq_devices));
	if (err)
		return err;

	return 0;
}
late_initcall(odin_busfreq_init);

static void __exit odin_busfreq_exit(void)
{
	platform_driver_unregister(&odin_busfreq_driver);
}
module_exit(odin_busfreq_exit);
