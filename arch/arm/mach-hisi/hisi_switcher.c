#include <linux/atomic.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/clockchips.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/notifier.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/moduleparam.h>

#include <asm/smp_plat.h>
#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <asm/suspend.h>
#include <asm/mcpm.h>

#include <linux/sched/rt.h>
#include <asm/psci.h>
#include <asm/suspend.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/huawei/hisi_rproc.h>
#include <linux/cpuidle.h>
#include <linux/completion.h>
#include <asm/bitops.h>
#include <asm/delay.h>

struct hisi_switcher_thread {
	spinlock_t lock;
	struct task_struct *task;
	wait_queue_head_t wq;
	int request;
};


#define MAX_SWITCH_TRY_NR		5
#define MAX_SWITCH_TIMEOUT		1000

volatile unsigned long cpu_status = 0;

volatile unsigned long abort_flag;
static int bus_switch_fail;

static struct completion switch_completion;
static int need_idle_resume;
#define NR_A7_CPUS	4
static struct hisi_switcher_thread switcher_thread_array[NR_A7_CPUS];

extern void wait_for_slave_cores(void);
extern int core_is_idle(void);

static int notrace hisi_switcher_finisher(unsigned long arg)
{
	unsigned int mpidr = read_cpuid_mpidr();
	unsigned int cluster = (mpidr >> 8) & 0xf;
	unsigned int cpu = mpidr & 0xf;

	mcpm_set_entry_vector(cpu, cluster, cpu_resume);
	mcpm_cpu_suspend(arg);
	return 1;
}

static int hisi_switcher_request(struct hisi_switcher_thread *th)
{
	int cpu, ret;
	struct hisi_switcher_thread *t;
	struct tick_device *tdev;
	enum clock_event_mode tdev_mode;
	int this_cpu = smp_processor_id();
	volatile unsigned long cpu_online_bit;
	volatile int try_again_nr = MAX_SWITCH_TRY_NR;
	volatile unsigned long take_cpu_timeout;

	bus_switch_fail = 0;

try_again:
	if (try_again_nr < 0) {
		pr_err("%s bus switch fialed.\n", __func__);

		bus_switch_fail = 1;

		complete(&switch_completion);

		if (need_idle_resume) {
			cpuidle_resume();
			need_idle_resume = 0;
		}

		spin_lock(&th->lock);
		th->request = -1;
		spin_unlock(&th->lock);

		return -1;
	}
	local_irq_disable();
	local_fiq_disable();
	try_again_nr--;
	abort_flag = 0;
	cpu_online_bit = 0;

	smp_wmb();
	/*
	 * wake up the slave cores thread to suspend themself
	 */
	for_each_online_cpu(cpu) {
		if (cpu == 0)
			continue;

		if (cpu == NR_A7_CPUS)
			break;

		t = &switcher_thread_array[cpu];

		set_bit(cpu, &cpu_online_bit);

		spin_lock(&t->lock);
		t->request = cpu;
		smp_wmb();
		spin_unlock(&t->lock);

		/* Wake up A7 other core bus switch thread. */
		wake_up(&t->wq);
	}

	take_cpu_timeout = MAX_SWITCH_TIMEOUT;
	while ((cpu_status & 0xf) != cpu_online_bit) {
		if (!take_cpu_timeout) {
			abort_flag = 1;
			smp_wmb();
			break;
		}

		udelay(1);
		take_cpu_timeout--;
		cpu_relax();
	}

	if (abort_flag) {
		while((cpu_status & 0xf) != 0x0)
			cpu_relax();

		local_irq_enable();
		local_fiq_enable();

		goto try_again;
	} else {
		set_bit(0, &cpu_status);
		smp_wmb();
	}

	wait_for_slave_cores();

	abort_flag = 1;
	clear_bit(0, &cpu_status);
	smp_wmb();

	tdev = tick_get_device(this_cpu);
	if (tdev && !cpumask_equal(tdev->evtdev->cpumask, cpumask_of(this_cpu)))
		tdev = NULL;
	if (tdev) {
		tdev_mode = tdev->evtdev->mode;
		clockevents_set_mode(tdev->evtdev, CLOCK_EVT_MODE_SHUTDOWN);
	}
	/* suspend main core */
	ret = cpu_pm_enter();
	if (ret)
		panic("%s: cpu_pm_enter() returned %d\n", __func__, ret);

	ret = cpu_suspend(HISI_BS_POWER_STATE_AFFINITY_LEVEL, hisi_switcher_finisher);
	if (ret > 0)
		panic("%s: cpu_suspend() returned %d\n", __func__, ret);

	ret = cpu_pm_exit();
	if (ret)
		panic("%s: cpu_pm_exit() returned %d\n", __func__, ret);

	if (tdev) {
		clockevents_set_mode(tdev->evtdev, tdev_mode);
		clockevents_program_event(tdev->evtdev,
					  tdev->evtdev->next_event, 1);
	}

	complete(&switch_completion);

	spin_lock(&th->lock);
	th->request = -1;
	spin_unlock(&th->lock);

	local_irq_enable();
	local_fiq_enable();

	if (need_idle_resume) {
		cpuidle_resume();
		need_idle_resume = 0;
	}

	return 0;

}


/* suspend slave core */
static int hisi_slave_switcher(struct hisi_switcher_thread *t)
{
	struct tick_device *tdev;
	enum clock_event_mode tdev_mode;
	int this_cpu = smp_processor_id();
	int ret;

	if (this_cpu != t->request)
		panic("%s cpu%d, request %d**************\n", __func__, this_cpu, t->request);

	local_irq_disable();
	local_fiq_disable();

	set_bit(this_cpu, &cpu_status);

	while(!(cpu_status & 0x1)) {
		if (abort_flag) {
			goto fail_slave;
		}
		cpu_relax();
	}

	tdev = tick_get_device(this_cpu);
	if (tdev && !cpumask_equal(tdev->evtdev->cpumask, cpumask_of(this_cpu)))
		tdev = NULL;
	if (tdev) {
		tdev_mode = tdev->evtdev->mode;
		clockevents_set_mode(tdev->evtdev, CLOCK_EVT_MODE_SHUTDOWN);
	}

	ret = cpu_pm_enter();
	if (ret)
		panic("%s: cpu_pm_enter() returned %d\n", __func__, ret);

	ret = cpu_suspend(HISI_BS_POWER_STATE_AFFINITY_LEVEL, hisi_switcher_finisher);
	if (ret > 0)
		panic("%s: cpu_suspend() returned %d\n", __func__, ret);

	ret = cpu_pm_exit();
	if (ret)
		panic("%s: cpu_pm_exit() returned %d\n", __func__, ret);

	if (tdev) {
		clockevents_set_mode(tdev->evtdev, tdev_mode);
		clockevents_program_event(tdev->evtdev,
					  tdev->evtdev->next_event, 1);
	}

fail_slave:
	/* put the thread to waitqueue. */
	spin_lock(&t->lock);
	t->request = -1;
	spin_unlock(&t->lock);

	clear_bit(this_cpu, &cpu_status);
	smp_wmb();
	local_irq_enable();
	local_fiq_enable();

	return 0;
}

extern int hisi_bus_switch_in_progress(void);

/* A7 bus switch thread */
static int hisi_bus_switcher_thread(void *arg)
{
	struct hisi_switcher_thread *t = arg;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	sched_setscheduler_nocheck(current, SCHED_FIFO, &param);

	do {
		wait_event_interruptible(t->wq,
				(t->request != -1) ||
				kthread_should_stop());
		if (t->request == -1)
			continue;

		if (smp_processor_id() == 0)
			hisi_switcher_request(t);
		else
			hisi_slave_switcher(t);

	} while (!kthread_should_stop());

	return 0;
}

static struct task_struct *hisi_switcher_thread_create(int cpu, void *arg)
{
	struct task_struct *task;

	task = kthread_create_on_cpu(hisi_bus_switcher_thread, arg,
					cpu, "bus_switcher_%d");
	if (!IS_ERR(task)) {
		get_task_struct(task);

		kthread_unpark(task);
	} else
		pr_err("%s failed for CPU %d\n", __func__, cpu);
	return task;
}

static int hisi_switcher_enable(void)
{
	int cpu, ret = 0;
	struct hisi_switcher_thread *t;

	cpu_hotplug_driver_lock();

	for_each_possible_cpu(cpu) {
		/* A15 should not need this thread */
		if (cpu == NR_A7_CPUS)
			break;

		t = &switcher_thread_array[cpu];
		spin_lock_init(&t->lock);
		init_waitqueue_head(&t->wq);
		t->request = -1;
		t->task = hisi_switcher_thread_create(cpu, t);
	}

	pr_info("hisi switcher initialized\n");
	cpu_hotplug_driver_unlock();

	return ret;
}

static int hisi_switcher_notifier(void)
{
	struct hisi_switcher_thread *t = &switcher_thread_array[0];

	if (IS_ERR(t->task))
		return PTR_ERR(t->task);
	if (!t->task)
		return -ESRCH;

	spin_lock(&t->lock);
	t->request = 1;
	spin_unlock(&t->lock);

	/* wake up the waitqueue&process */
	wake_up(&t->wq);

	return 0;
}

int a15_cluster_is_down(void)
{
	if (!cpu_online(4) && !cpu_online(5) && !cpu_online(6) && !cpu_online(7))
		return 1;
	else
		return 0;
}

extern void bus_switch2ace(void);
extern void bus_switch2axi(void);
enum BUS_MODE {
	AXI_MODE,
	ACE_MODE
};
enum BUS_MODE cur_mode = ACE_MODE;

int hisi_bus_switch_hook(int cpuid, int online)
{
	enum BUS_MODE switch_mode;

	/* ignore A7 hotplug */
	if (cpuid < 4)
		return 0;

	if (a15_cluster_is_down()) {
		if (((cur_mode == ACE_MODE) && online)
			|| ((cur_mode == AXI_MODE) && !online))
			return 0;

		if (online) {
			cpuidle_pause();
			need_idle_resume = 1;
			bus_switch2ace();

			switch_mode = ACE_MODE;
		} else {
			bus_switch2axi();
			switch_mode = AXI_MODE;
		}

		if (hisi_switcher_notifier())
			return -1;

		/* wait for switch process done. */
		wait_for_completion(&switch_completion);
		if (bus_switch_fail) {
			return -1;
		} else {
			cur_mode = switch_mode;
			return 0;
		}
	}

	return 0;
}
EXPORT_SYMBOL(hisi_bus_switch_hook);

extern void hisi_cpu_hotplug_lock(void);
extern void hisi_cpu_hotplug_unlock(void);

int hisi_cpu_down(int cpuid)
{
	int ret;
	hisi_cpu_hotplug_lock();

	cpuidle_pause();
	ret = cpu_down(cpuid);
	if (!ret)
		hisi_bus_switch_hook(cpuid, 0);
	cpuidle_resume();
	kick_all_cpus_sync();

	hisi_cpu_hotplug_unlock();

	return ret;
}
EXPORT_SYMBOL(hisi_cpu_down);

int hisi_cpu_up(int cpuid)
{
	int ret;
	hisi_cpu_hotplug_lock();

	ret = hisi_bus_switch_hook(cpuid, 1);
	if (!ret)
		cpu_up(cpuid);

	hisi_cpu_hotplug_unlock();
	return ret;
}
EXPORT_SYMBOL(hisi_cpu_up);

static int __init hisi_switcher_init(void)
{

	init_completion(&switch_completion);

	/* initialize the switcher threads. */
	hisi_switcher_enable();

	return 0;
}
fs_initcall(hisi_switcher_init);
