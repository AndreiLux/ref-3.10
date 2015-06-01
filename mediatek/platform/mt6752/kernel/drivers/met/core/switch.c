//#include <asm/percpu.h>
#include <trace/events/sched.h>

#include "interface.h"
#include "met_drv.h"
#include "cpu_pmu.h"
//#include "trace.h"

extern struct cpu_pmu_hw *cpu_pmu;
static DEFINE_PER_CPU(unsigned int, first_log);

#ifdef __aarch64__
#include <asm/compat.h>
#endif

noinline static void mt_switch(struct task_struct *prev, struct task_struct *next)
{
	int cpu;
	int prev_state=0, next_state=0;
#ifdef __aarch64__
	prev_state = !(is_compat_thread(task_thread_info(prev)));
	next_state = !(is_compat_thread(task_thread_info(next)));
#endif

	cpu = smp_processor_id();
	if (per_cpu(first_log, cpu)) {
		MET_PRINTK("%d, %d, %d, %d\n", prev->pid, prev_state, next->pid, next_state);
		per_cpu(first_log, cpu) = 0;
	}
	if (prev_state != next_state)
		MET_PRINTK("%d, %d, %d, %d\n", prev->pid, prev_state, next->pid, next_state);
}


extern noinline void mp_cpu(unsigned char cnt, unsigned int *value);
MET_DEFINE_PROBE(sched_switch, TP_PROTO(struct task_struct *prev, struct task_struct *next))
{
	if (met_switch.mode & 0x2) {
		mt_switch(prev, next);
	}

	if (met_switch.mode & 0x1) {
		int count;
		unsigned int pmu_value[8];
		count = cpu_pmu->polling(cpu_pmu->pmu, cpu_pmu->nr_cnt, pmu_value);
		mp_cpu(count, pmu_value);
	}
}


static int met_switch_create_subfs(struct kobject *parent)
{
	// register tracepoints
	if (MET_REGISTER_TRACE(sched_switch)) {
		pr_err("can not register callback of sched_switch\n");
		return -ENODEV;
	}
	return 0;
}


static void met_switch_delete_subfs(void)
{
	MET_UNREGISTER_TRACE(sched_switch);
}


static void (*cpu_timed_polling) (unsigned long long stamp, int cpu);
static void (*cpu_tagged_polling) (unsigned long long stamp, int cpu);

static void met_switch_start(void)
{
	int cpu;

	if (met_switch.mode & 0x1) {
		cpu_timed_polling = met_cpupmu.timed_polling;
		cpu_tagged_polling = met_cpupmu.tagged_polling;
		met_cpupmu.timed_polling = NULL;
		met_cpupmu.tagged_polling = NULL;
	}

	for_each_possible_cpu(cpu) {
		per_cpu(first_log, cpu) = 1;
	}

}


static void met_switch_stop(void)
{
	int cpu;

	if (met_switch.mode & 0x1) {
		met_cpupmu.timed_polling = cpu_timed_polling;
		met_cpupmu.tagged_polling = cpu_tagged_polling;
	}

	for_each_possible_cpu(cpu) {
		per_cpu(first_log, cpu) = 0;
	}

}


static int met_switch_process_argument(const char *arg, int len)
{
	unsigned int value;


	if (met_parse_num(arg, &value, len) < 0) {
		goto arg_switch_exit;
	}

	if ((value<1)||(value>3)) {
		goto arg_switch_exit;
	}

	met_switch.mode = value;
	return 0;

arg_switch_exit:
	met_switch.mode = 0;
	return -EINVAL;
}

static const char header[] =
	"met-info [000] 0.0: met_switch_header: prev_pid,prev_state,next_pid,next_state\n";

static const char help[] = "  --switch=mode                         mode:1 - output CPUPMU whenever sched_switch\n"
                           "                                        mode:2 - output Aarch 32/64 state whenever state changed (no CPUMPU)\n"
                           "                                        mode:3 - mode 1 + mode 2\n";

static int met_switch_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
}

static int met_switch_print_header(char *buf, int len)
{
	len = 0;
	if (met_switch.mode & 0x2) {
		len = snprintf(buf, PAGE_SIZE, header);
	}
	met_switch.mode = 0;
	return len;
}


struct metdevice met_switch = {
	.name = "switch",
	.type = MET_TYPE_PMU,
	.create_subfs = met_switch_create_subfs,
	.delete_subfs = met_switch_delete_subfs,
	.start = met_switch_start,
	.stop = met_switch_stop,
	.process_argument= met_switch_process_argument,
	.print_help = met_switch_print_help,
	.print_header = met_switch_print_header,
};

