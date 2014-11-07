#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/perf_event.h>
#include <linux/suspend.h>
#include <linux/platform_data/odin_tz.h>
#include <linux/cpu.h>
#include <linux/odin_gpufreq.h>
#include <linux/devfreq.h>

#include <trace/events/power.h>

#include <asm/pmu.h>
#include <asm/mcpm.h>

#define SYS_PROBE_MEMC0_BASE 0x3500c000
#define SYS_PROBE_MEMC1_BASE 0x3501c000

#define AM_OFFSET 0x0
#define GM_OFFSET 0x1000
#define DM_OFFSET 0x2000

extern struct arm_pmu cci_pmu;
extern struct class *devfreq_class;
extern unsigned int odin_get_memclk(void);
extern unsigned int odin_get_cciclk(void);

struct perf_data {
	struct perf_event *event;
	u64 prev_val;
};

struct cci_slave_event {
	struct perf_data read;
	struct perf_data write;
};

static struct cci_slave_event cci_pmu_event[5];

#define CCI_SLAVE_GPU_START 1
#define CCI_SLAVE_GPU_END   2
#define CCI_SLAVE_CPU_START 3
#define CCI_SLAVE_CPU_END   4

static int cci_slave_start;
static int cci_slave_end;

static struct perf_data cpu_pmu_event[NR_CPUS];

#define DEFAULT_MONITOR_RATE 100
#define DEFAULT_SLACK_FACTOR 5
static unsigned long monitor_rate;
static unsigned long slack_factor;
static void bus_monitor_work_fn(struct work_struct *work);
static DECLARE_DEFERRABLE_WORK(bus_monitor_work, bus_monitor_work_fn);
static atomic_t working;
static atomic_t suspending;
static struct timer_list wakeup_timer;

static void __iomem *memc0_base;
static void __iomem *memc1_base;

static ktime_t prev;

static u64 mem_bps;
static u64 cci_bps;
static u64 gpu_bps;
static u64 cpu_bps;
static u64 noc_bps;
static u64 mem_bps_normalized;
static u64 cci_bps_normalized;
static u64 gpu_bps_normalized;
static u64 cpu_bps_normalized;
static u64 noc_bps_normalized;

static bool gpu_idle;
static cpumask_t ca7_idle, ca15_idle;

u64 odin_current_mem_bandwidth(void)
{
	return mem_bps;
}
EXPORT_SYMBOL(odin_current_mem_bandwidth);

u64 odin_current_cci_bandwidth(void)
{
	return cci_bps;
}
EXPORT_SYMBOL(odin_current_cci_bandwidth);

u64 odin_current_gpu_bandwidth(void)
{
	return gpu_bps;
}
EXPORT_SYMBOL(odin_current_gpu_bandwidth);

u64 odin_current_cpu_bandwidth(void)
{
	return cpu_bps;
}
EXPORT_SYMBOL(odin_current_cpu_bandwidth);

u64 odin_normalized_mem_bandwidth(void)
{
	return mem_bps_normalized;
}
EXPORT_SYMBOL(odin_normalized_mem_bandwidth);

u64 odin_normalized_cci_bandwidth(void)
{
	return cci_bps_normalized;
}
EXPORT_SYMBOL(odin_normalized_cci_bandwidth);

u64 odin_normalized_gpu_bandwidth(void)
{
	return gpu_bps_normalized;
}
EXPORT_SYMBOL(odin_normalized_gpu_bandwidth);

u64 odin_normalized_cpu_bandwidth(void)
{
	return cpu_bps_normalized;
}
EXPORT_SYMBOL(odin_normalized_cpu_bandwidth);

/* TODO: we need to create a tz_ "read alot of data" function .. */
static inline u32 __read(void __iomem *addr)
{
#ifndef CONFIG_ODIN_TEE
	return __raw_readl(addr);
#else
	return tz_read((u32)addr);
#endif
}

static inline void __write(void __iomem *addr, u32 val)
{
#ifndef CONFIG_ODIN_TEE
	__raw_writel(val, addr);
#else
	tz_write(val, (u32)addr);
#endif
}

static void pmu_overflow_handler(struct perf_event *event,
				 struct perf_sample_data *data,
				 struct pt_regs *regs)
{
	/* just in case hardware stops counting when overflow occurs */
	perf_event_disable(event);
	perf_event_enable(event);
}

static void cpu_pmu_release_one(unsigned int cpu)
{
	if (cpu_pmu_event[cpu].event)
		perf_event_release_kernel(cpu_pmu_event[cpu].event);
	cpu_pmu_event[cpu].event = NULL;
}

static void cpu_pmu_release(void)
{
	int i;

	for_each_online_cpu(i)
		cpu_pmu_release_one(i);
}

static int cpu_pmu_create_one(unsigned int cpu)
{
	struct perf_event *event;
	struct perf_event_attr attr;

	if (cci_slave_end == CCI_SLAVE_CPU_END)
		return 0;

	if (cpu_pmu_event[cpu].event)
		return 0;

	memset(&attr, 0, sizeof(struct perf_event_attr));
	attr.size   = sizeof(struct perf_event_attr);
	attr.type   = cpu_topology[cpu].socket_id ? 8 : 7;
	attr.config = 0x13;

	event = perf_event_create_kernel_counter(
		&attr, cpu, NULL, pmu_overflow_handler, NULL);

	if (IS_ERR(event)) {
		int err;
		err = PTR_ERR(event);
		cpu_pmu_event[cpu].event = NULL;
		pr_err("failed to create CPU%d PMU event (%d)\n", cpu, err);
		return err;
	}

	cpu_pmu_event[cpu].event = event;
	return 0;
}

static int cpu_pmu_create(void)
{
	int i;
	int err = 0;

	for_each_online_cpu(i)
		err = cpu_pmu_create_one(i);

	return err;
}

static void cpu_pmu_enable_one(unsigned int cpu)
{
	if (cpu_pmu_event[cpu].event)
		perf_event_enable(cpu_pmu_event[cpu].event);
}

static void cpu_pmu_enable(void)
{
	int i;

	for_each_online_cpu(i)
		cpu_pmu_enable_one(i);
}

static void cpu_pmu_disable_one(unsigned int cpu)
{
	if (cpu_pmu_event[cpu].event)
		perf_event_disable(cpu_pmu_event[cpu].event);
}

static void cpu_pmu_disable(void)
{
	int i;

	for_each_online_cpu(i)
		cpu_pmu_disable_one(i);
}

static void cci_pmu_release(void)
{
	int i;

	for (i = cci_slave_start; i <= cci_slave_end; i++) {
		if (cci_pmu_event[i].read.event)
			perf_event_release_kernel(cci_pmu_event[i].read.event);

		cci_pmu_event[i].read.event = NULL;

		if (cci_pmu_event[i].write.event)
			perf_event_release_kernel(cci_pmu_event[i].write.event);

		cci_pmu_event[i].write.event = NULL;
	}
}

static int cci_pmu_create(void)
{
	int i;
	int err;
	struct perf_event_attr attr;

	memset(&attr, 0, sizeof(struct perf_event_attr));
	attr.type = cci_pmu.pmu.type;
	attr.size = sizeof(struct perf_event_attr);

	for (i = cci_slave_start; i <= cci_slave_end; i++) {
		if (i >= CCI_SLAVE_GPU_START && i <= CCI_SLAVE_GPU_END)
			if (gpu_idle)
				continue;

		attr.config = (i << 5) | 0x00;
		cci_pmu_event[i].read.event = perf_event_create_kernel_counter(
			&attr, 0, NULL, pmu_overflow_handler, NULL);

		if (IS_ERR(cci_pmu_event[i].read.event)) {
			err = PTR_ERR(cci_pmu_event[i].read.event);
			cci_pmu_event[i].read.event = NULL;
			cci_pmu_release();
			pr_err("failed to create CCI PMU event (%d)\n", err);
			return err;
		}

		attr.config = (i << 5) | 0x12;
		cci_pmu_event[i].write.event = perf_event_create_kernel_counter(
			&attr, 0, NULL, pmu_overflow_handler, NULL);

		if (IS_ERR(cci_pmu_event[i].write.event)) {
			err = PTR_ERR(cci_pmu_event[i].write.event);
			cci_pmu_event[i].write.event = NULL;
			cci_pmu_release();
			pr_err("failed to create CCI PMU event (%d)\n", err);
			return err;
		}
	}

	return 0;
}

static void cci_pmu_enable(void)
{
	static bool flip;
	int i;

	flip = !flip;

	if (flip) {
		for (i = cci_slave_end; i >= cci_slave_start; i--) {
			if (cci_pmu_event[i].read.event)
				perf_event_enable(cci_pmu_event[i].read.event);

			if (cci_pmu_event[i].write.event)
				perf_event_enable(cci_pmu_event[i].write.event);
		}
	} else {
		for (i = cci_slave_start; i <= cci_slave_end; i++) {
			if (cci_pmu_event[i].read.event)
				perf_event_enable(cci_pmu_event[i].read.event);

			if (cci_pmu_event[i].write.event)
				perf_event_enable(cci_pmu_event[i].write.event);
		}
	}
}

static void cci_pmu_disable(void)
{
	int i;

	for (i = cci_slave_start; i <= cci_slave_end; i++) {
		if (cci_pmu_event[i].read.event)
			perf_event_disable(cci_pmu_event[i].read.event);

		if (cci_pmu_event[i].write.event)
			perf_event_disable(cci_pmu_event[i].write.event);
	}
}

static u64 cci_pmu_read(struct perf_event *event)
{
	u64 total, enabled, running;
	struct perf_data *data = container_of(&event, struct perf_data, event);

	total = perf_event_read_value(event, &enabled, &running);

	if (!running)
		return data->prev_val;

	if (enabled != running)
		total = div_u64(total * enabled, running);

	data->prev_val = total;

	return total;
}

static void sys_probe_port_init(void __iomem *base)
{
	__write(base + 0x8,   0x08);
	__write(base + 0x134, 0x01);
	__write(base + 0x138, 0x08);
	__write(base + 0x14c, 0x10);
	__write(base + 0x024, 0x00);
}

static void sys_probe_init(void)
{
	sys_probe_port_init(memc0_base + AM_OFFSET);
	sys_probe_port_init(memc1_base + AM_OFFSET);
	sys_probe_port_init(memc0_base + GM_OFFSET);
	sys_probe_port_init(memc1_base + GM_OFFSET);
	sys_probe_port_init(memc0_base + DM_OFFSET);
	sys_probe_port_init(memc1_base + DM_OFFSET);
}

static void sys_probe_enable(void)
{
	__write(memc0_base + AM_OFFSET + 0xc, 0x1);
	__write(memc1_base + AM_OFFSET + 0xc, 0x1);
	__write(memc0_base + GM_OFFSET + 0xc, 0x1);
	__write(memc1_base + GM_OFFSET + 0xc, 0x1);
	__write(memc0_base + DM_OFFSET + 0xc, 0x1);
	__write(memc1_base + DM_OFFSET + 0xc, 0x1);
}

static void sys_probe_disable(void)
{
	__write(memc0_base + AM_OFFSET + 0xc, 0x0);
	__write(memc1_base + AM_OFFSET + 0xc, 0x0);
	__write(memc0_base + GM_OFFSET + 0xc, 0x0);
	__write(memc1_base + GM_OFFSET + 0xc, 0x0);
	__write(memc0_base + DM_OFFSET + 0xc, 0x0);
	__write(memc1_base + DM_OFFSET + 0xc, 0x0);
}

static u32 sys_probe_port_counter_read(void __iomem *base)
{
	u32 high, low;

	high = __read(base + 0x150);
	low  = __read(base + 0x13c);

	return (high << 16) | (low & 0xffff);
}

void odin_bus_monitor_kick(void)
{
	if (atomic_read(&suspending))
		return;

	mod_timer(&wakeup_timer, jiffies + msecs_to_jiffies(monitor_rate));
}

static void bus_monitor_wakeup_fn(unsigned long data)
{
	if (atomic_read(&working))
		return;

	if (atomic_read(&suspending))
		return;

	cancel_delayed_work(&bus_monitor_work);
	queue_delayed_work_on(0, system_wq, &bus_monitor_work, 0);
}

extern struct devfreq *odin_devfreq[2];

static bool needs_wakeup(void)
{
	int i;

	for (i = 0; i < 2; i++) {
		if (!odin_devfreq[i])
			continue;

		if (odin_devfreq[i]->previous_freq > odin_devfreq[i]->min_freq)
			return true;
	}

	return false;
}

static void check_devfreq_pending(struct devfreq *devfreq)
{
	unsigned long expires, now;

	if (!devfreq)
		return;

	expires = devfreq->work.timer.expires;
	now     = jiffies;

	if (!work_pending(&devfreq->work.work))
		return;

	if (!timer_pending(&devfreq->work.timer))
		return;

	if (now - expires < msecs_to_jiffies(monitor_rate))
		return;

	flush_delayed_work(&devfreq->work);
}

static void bus_monitor_work_fn(struct work_struct *dummy)
{
	int i;
	u64 total, enabled, running;
	u64 reads, writes;
	u64 cpu_bdw, gpu_bdw, anoc_bdw, gnoc_bdw, dnoc_bdw;
	ktime_t curr;

	atomic_set(&working, 1);

	del_timer(&wakeup_timer);

	sys_probe_disable();
	cci_pmu_disable();

	curr = ktime_get();

	gpu_bdw = 0;
	cpu_bdw = 0;

	for (i = cci_slave_start; i <= cci_slave_end; i++) {
		if (cci_pmu_event[i].read.event  == NULL ||
		    cci_pmu_event[i].write.event == NULL)
			continue;

		reads  = cci_pmu_read(cci_pmu_event[i].read.event);
		writes = cci_pmu_read(cci_pmu_event[i].write.event);

		/* 1 count == 64 bytes (cache line) */
		total = (reads + writes) * 64;

		if (i >= CCI_SLAVE_GPU_START && i <= CCI_SLAVE_GPU_END)
			gpu_bdw += total;
		else
			cpu_bdw += total;
	}

	if (cci_slave_end != CCI_SLAVE_CPU_END) {
		for_each_online_cpu(i) {
			if (cpu_pmu_event[i].event == NULL)
				continue;

			if (__mcpm_cpu_state(cpu_topology[i].core_id,
					cpu_topology[i].socket_id) == CPU_DOWN)
				continue;

			total = perf_event_read_value(cpu_pmu_event[i].event,
						&enabled, &running);
			cpu_bdw += (total - cpu_pmu_event[i].prev_val) * 4;
			cpu_pmu_event[i].prev_val = total;
		}
	}

	/* 1 count == 1 byte */
	anoc_bdw  = sys_probe_port_counter_read(memc0_base + AM_OFFSET);
	anoc_bdw += sys_probe_port_counter_read(memc1_base + AM_OFFSET);

	gnoc_bdw  = sys_probe_port_counter_read(memc0_base + GM_OFFSET);
	gnoc_bdw += sys_probe_port_counter_read(memc1_base + GM_OFFSET);

	dnoc_bdw  = sys_probe_port_counter_read(memc0_base + DM_OFFSET);
	dnoc_bdw += sys_probe_port_counter_read(memc1_base + DM_OFFSET);

	/* convert to bytes per second */
	total   = gpu_bdw + cpu_bdw;
	cci_bps = div_u64(total * 1000000, ktime_us_delta(curr, prev));
	cci_bps_normalized = (cci_bps_normalized / 2) + (cci_bps / 2);

	total  += anoc_bdw + gnoc_bdw + dnoc_bdw;
	mem_bps = div_u64(total * 1000000, ktime_us_delta(curr, prev));
	mem_bps_normalized = (mem_bps_normalized / 2) + (mem_bps / 2);

	gpu_bps = div_u64(gpu_bdw * 1000000, ktime_us_delta(curr, prev));
	gpu_bps_normalized = (gpu_bps_normalized / 2) + (gpu_bps / 2);

	cpu_bps = div_u64(cpu_bdw * 1000000, ktime_us_delta(curr, prev));
	cpu_bps_normalized = (cpu_bps_normalized / 2) + (cpu_bps / 2);

	noc_bps = div_u64((anoc_bdw + gnoc_bdw + dnoc_bdw) * 1000000,
			ktime_us_delta(curr, prev));
	noc_bps_normalized = (noc_bps_normalized / 2) + (noc_bps / 2);

#ifdef CONFIG_ARM_ODIN_BUS_DEVFREQ_DEBUG
	total = div_u64(anoc_bdw * 1000000, ktime_us_delta(curr, prev));
	trace_odin_bus_bandwidth("odin-anoc bandwidth", total >> 10);

	total = div_u64(gnoc_bdw * 1000000, ktime_us_delta(curr, prev));
	trace_odin_bus_bandwidth("odin-gnoc bandwidth", total >> 10);

	total = div_u64(dnoc_bdw * 1000000, ktime_us_delta(curr, prev));
	trace_odin_bus_bandwidth("odin-dnoc bandwidth", total >> 10);

	trace_odin_bus_bandwidth("odin-cci bandwidth", cci_bps >> 10);
	trace_odin_bus_bandwidth("odin-mem bandwidth", mem_bps >> 10);
	trace_odin_bus_bandwidth("odin-gpu bandwidth", gpu_bps >> 10);
	trace_odin_bus_bandwidth("odin-cpu bandwidth", cpu_bps >> 10);
#endif

	cci_pmu_release();
	cci_pmu_create();
	cci_pmu_enable();
	sys_probe_enable();

	prev = ktime_get();

	for (i = 0; i < 2; i++) {
		if (!odin_devfreq[i])
			continue;

		if (odin_devfreq[i]->previous_freq > odin_devfreq[i]->min_freq)
			check_devfreq_pending(odin_devfreq[i]);
	}

	if (needs_wakeup())
		mod_timer(&wakeup_timer, jiffies + msecs_to_jiffies(
				monitor_rate * slack_factor));

	queue_delayed_work_on(0, system_wq, &bus_monitor_work,
			msecs_to_jiffies(monitor_rate));

	atomic_set(&working, 0);
}

static int odin_bus_monitor_pm_notifier_event(struct notifier_block *nb,
					      unsigned long action, void *data)
{
	switch (action) {
	case PM_SUSPEND_PREPARE:
		atomic_set(&suspending, 1);

		cancel_delayed_work_sync(&bus_monitor_work);
		del_timer(&wakeup_timer);

		cci_pmu_disable();
		cci_pmu_release();
		cpu_pmu_disable();
		cpu_pmu_release();
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		atomic_set(&suspending, 0);

		cci_pmu_create();
		cci_pmu_enable();
		cpu_pmu_create();
		cpu_pmu_enable();
		sys_probe_init();
		sys_probe_enable();

		queue_delayed_work_on(0, system_wq, &bus_monitor_work,
				msecs_to_jiffies(monitor_rate));
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static struct notifier_block odin_bus_monitor_pm_notifier_block = {
	.notifier_call = odin_bus_monitor_pm_notifier_event,
};

static int __cpuinit odin_bus_monitor_cpu_event(struct notifier_block *nfb,
						unsigned long action,
						void *hcpu)
{
	extern int sysctl_perf_event_paranoid;

	int tmp;
	unsigned int cpu = (unsigned long)hcpu;

	tmp = sysctl_perf_event_paranoid;
	sysctl_perf_event_paranoid = 0;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		cpu_pmu_create_one(cpu);
		cpu_pmu_enable_one(cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		cpu_pmu_disable_one(cpu);
		cpu_pmu_release_one(cpu);
		break;
	}

	sysctl_perf_event_paranoid = tmp;
	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata odin_bus_monitor_cpu_nb = {
	.notifier_call = odin_bus_monitor_cpu_event,
};

static int odin_bus_monitor_gpu_event(struct notifier_block *nb,
				      unsigned long action,
				      void *data)
{
	struct odin_gpufreq_freqs *freqs = data;

	if (freqs->old < freqs->new && action != ODIN_GPUFREQ_PRECHANGE)
		return NOTIFY_DONE;

	if (freqs->old > freqs->new && action != ODIN_GPUFREQ_POSTCHANGE)
		return NOTIFY_DONE;

	if (freqs->old > 0 && freqs->new == 0)
		gpu_idle = true;
	else if (freqs->old == 0 && freqs->new > 0)
		gpu_idle = false;

	return NOTIFY_OK;
}

static struct notifier_block odin_bus_monitor_gpu_nb = {
	.notifier_call = odin_bus_monitor_gpu_event,
};

static int odin_bus_monitor_cpuidle_event(struct notifier_block *nb,
					  unsigned long action,
					  void *data)
{
	cpumask_t *idle;
	unsigned int cpu = smp_processor_id();

	if (cpu_topology[cpu].socket_id)
		idle = &ca7_idle;
	else
		idle = &ca15_idle;

	switch (action) {
	case IDLE_START:
		cpumask_clear_cpu(cpu, idle);
		break;
	case IDLE_END:
		cpumask_set_cpu(cpu, idle);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block odin_bus_monitor_cpuidle_nb = {
	.notifier_call = odin_bus_monitor_cpuidle_event,
};

#define SHOW_ONE(name)                                                        \
static ssize_t show_##name(struct class *class, struct class_attribute *attr, \
			   char *buf)                                         \
{                                                                             \
	return sprintf(buf, "%llu\n", name);                                  \
}                                                                             \
static CLASS_ATTR(name, 0444, show_##name, NULL)

SHOW_ONE(mem_bps);
SHOW_ONE(cci_bps);
SHOW_ONE(gpu_bps);
SHOW_ONE(cpu_bps);
SHOW_ONE(noc_bps);
SHOW_ONE(mem_bps_normalized);
SHOW_ONE(cci_bps_normalized);
SHOW_ONE(gpu_bps_normalized);
SHOW_ONE(cpu_bps_normalized);
SHOW_ONE(noc_bps_normalized);

#define SHOW_STORE_ONE(name)                              \
static ssize_t show_##name(struct class *class,           \
			   struct class_attribute *attr,  \
			   char *buf)                     \
{                                                         \
	return sprintf(buf, "%lu\n", name);               \
}                                                         \
static ssize_t store_##name(struct class *class,          \
			    struct class_attribute *attr, \
			    const char *buf,              \
			    size_t count)                 \
{                                                         \
	unsigned long val;                                \
                                                          \
	if (!kstrtoul(buf, 0, &val)) {                    \
		name = val;                               \
		return count;                             \
	}                                                 \
                                                          \
	return 0;                                         \
}                                                         \
static CLASS_ATTR(name, 0644, show_##name, store_##name)

SHOW_STORE_ONE(monitor_rate);
SHOW_STORE_ONE(slack_factor);

static int __init odin_bus_monitor_init(void)
{
	int ret;

#ifndef CONFIG_ODIN_TEE
	memc0_base = ioremap(SYS_PROBE_MEMC0_BASE, 0x3000);
	memc1_base = ioremap(SYS_PROBE_MEMC1_BASE, 0x3000);
#else
	memc0_base = (void *)SYS_PROBE_MEMC0_BASE;
	memc1_base = (void *)SYS_PROBE_MEMC1_BASE;
#endif

	ret = class_create_file(devfreq_class, &class_attr_mem_bps);
	ret = class_create_file(devfreq_class, &class_attr_cci_bps);
	ret = class_create_file(devfreq_class, &class_attr_gpu_bps);
	ret = class_create_file(devfreq_class, &class_attr_cpu_bps);
	ret = class_create_file(devfreq_class, &class_attr_noc_bps);
	ret = class_create_file(devfreq_class, &class_attr_mem_bps_normalized);
	ret = class_create_file(devfreq_class, &class_attr_cci_bps_normalized);
	ret = class_create_file(devfreq_class, &class_attr_gpu_bps_normalized);
	ret = class_create_file(devfreq_class, &class_attr_cpu_bps_normalized);
	ret = class_create_file(devfreq_class, &class_attr_noc_bps_normalized);
	ret = class_create_file(devfreq_class, &class_attr_monitor_rate);
	ret = class_create_file(devfreq_class, &class_attr_slack_factor);

	cci_slave_start = CCI_SLAVE_GPU_START;
	cci_slave_end   = CCI_SLAVE_CPU_END;
	monitor_rate    = DEFAULT_MONITOR_RATE;
	slack_factor    = DEFAULT_SLACK_FACTOR;

	cpu_pmu_create();
	cpu_pmu_enable();
	cci_pmu_create();
	cci_pmu_enable();
	sys_probe_init();
	sys_probe_enable();

	prev = ktime_get();

	INIT_DEFERRABLE_WORK(&bus_monitor_work, bus_monitor_work_fn);
	setup_timer(&wakeup_timer, bus_monitor_wakeup_fn, 0);

	queue_delayed_work_on(0, system_wq, &bus_monitor_work,
			msecs_to_jiffies(monitor_rate));

	register_hotcpu_notifier(&odin_bus_monitor_cpu_nb);
	register_pm_notifier(&odin_bus_monitor_pm_notifier_block);
	odin_gpufreq_register_notifier(&odin_bus_monitor_gpu_nb);
	idle_notifier_register(&odin_bus_monitor_cpuidle_nb);

	return ret;
}
device_initcall(odin_bus_monitor_init);

static void __exit odin_bus_monitor_exit(void)
{
	class_remove_file(devfreq_class, &class_attr_mem_bps);
	class_remove_file(devfreq_class, &class_attr_cci_bps);
	class_remove_file(devfreq_class, &class_attr_gpu_bps);
	class_remove_file(devfreq_class, &class_attr_cpu_bps);
	class_remove_file(devfreq_class, &class_attr_noc_bps);
	class_remove_file(devfreq_class, &class_attr_mem_bps_normalized);
	class_remove_file(devfreq_class, &class_attr_cci_bps_normalized);
	class_remove_file(devfreq_class, &class_attr_gpu_bps_normalized);
	class_remove_file(devfreq_class, &class_attr_cpu_bps_normalized);
	class_remove_file(devfreq_class, &class_attr_noc_bps_normalized);
	class_remove_file(devfreq_class, &class_attr_monitor_rate);
	class_remove_file(devfreq_class, &class_attr_slack_factor);

	cancel_delayed_work_sync(&bus_monitor_work);
	del_timer(&wakeup_timer);

	unregister_hotcpu_notifier(&odin_bus_monitor_cpu_nb);
	unregister_pm_notifier(&odin_bus_monitor_pm_notifier_block);
	odin_gpufreq_unregister_notifier(&odin_bus_monitor_gpu_nb);
	idle_notifier_unregister(&odin_bus_monitor_cpuidle_nb);

	cpu_pmu_disable();
	cpu_pmu_release();
	cci_pmu_disable();
	cci_pmu_release();
	sys_probe_disable();
}
module_exit(odin_bus_monitor_exit);
