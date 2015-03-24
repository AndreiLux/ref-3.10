#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>

#include <asm/localtimer.h>


#define read_cntfrq(cntfrq) \
	do {    \
		__asm__ __volatile__(   \
			"MRC p15, 0, %0, c14, c0, 0\n"  \
			: "=r"(cntfrq)   \
			:   \
			: "memory"); \
	} while (0)

#define change_cntfrq(cntfrq) \
	do {    \
		__asm__ __volatile__(   \
			"MCR p15, 0, %0, c14, c0, 0\n"  \
			:   \
			: "r"(cntfrq)); \
	} while (0)

#define read_cntkctl(cntkctl)   \
	do {    \
		__asm__ __volatile__(   \
			"MRC p15, 0, %0, c14, c1, 0\n"  \
			: "=r"(cntkctl)  \
			:   \
			: "memory"); \
	} while (0)

#define read_cntpct(cntpct_lo, cntpct_hi)   \
	do {    \
		__asm__ __volatile__(   \
			"MRRC p15, 0, %0, %1, c14\n"    \
			: "=r"(cntpct_lo), "=r"(cntpct_hi)   \
			:   \
			: "memory"); \
	} while (0)

#define read_cntvct(cntvct_lo, cntvct_hi)   \
	do {    \
		__asm__ __volatile__(   \
			"MRRC p15, 1, %0, %1, c14\n"    \
			: "=r"(cntvct_lo), "=r"(cntvct_hi)   \
			:   \
			: "memory"); \
	} while (0)

#define read_cntp_ctl(cntp_ctl)   \
	do {    \
		__asm__ __volatile__(   \
			"MRC p15, 0, %0, c14, c2, 1\n"  \
			: "=r"(cntp_ctl) \
			:   \
			: "memory"); \
	} while (0)

#define write_cntp_ctl(cntp_ctl)  \
	do {    \
		__asm__ __volatile__(   \
		"MCR p15, 0, %0, c14, c2, 1\n"  \
		:   \
		: "r"(cntp_ctl)); \
	} while (0)

#define read_cntp_cval(cntp_cval_lo, cntp_cval_hi) \
	do {    \
		__asm__ __volatile__(   \
			"MRRC p15, 2, %0, %1, c14\n"    \
			: "=r"(cntp_cval_lo), "=r"(cntp_cval_hi) \
			:   \
			: "memory"); \
	} while (0)

#define write_cntp_cval(cntp_cval_lo, cntp_cval_hi) \
	do {    \
		__asm__ __volatile__(   \
			"MCRR p15, 2, %0, %1, c14\n"    \
			:   \
			: "r"(cntp_cval_lo), "r"(cntp_cval_hi)); \
	} while (0)

#define read_cntp_tval(cntp_tval) \
	do {    \
		__asm__ __volatile__(   \
			"MRC p15, 0, %0, c14, c2, 0"    \
			: "=r"(cntp_tval)	\
			:   \
			: "memory"); \
	} while (0)

#define write_cntp_tval(cntp_tval) \
	do {	\
		__asm__ __volatile__(   \
			"MCR p15, 0, %0, c14, c2, 0\n"	\
			:   \
			: "r"(cntp_tval));	\
	} while (0)


#define read_cntv_ctl(cntv_ctl)   \
	do {	\
		__asm__ __volatile__(   \
			"MRC p15, 0, %0, c14, c3, 1\n"  \
			: "=r"(cntv_ctl) \
			:   \
			: "memory"); \
	} while (0)

#define read_cntv_cval(cntv_cval_lo, cntv_cval_hi) \
	do {	\
		__asm__ __volatile__(   \
			"MRRC p15, 3, %0, %1, c14\n"	\
			: "=r"(cntv_cval_lo), "=r"(cntv_cval_hi) \
			:   \
			: "memory"); \
	} while (0)

#define read_cntv_tval(cntv_tval) \
	do {	\
		__asm__ __volatile__(   \
			"MRC p15, 0, %0, c14, c3, 0"	\
			: "=r"(cntv_tval)	\
			:   \
			: "memory"); \
	} while (0)


#define CNTP_CTL_ENABLE     (1 << 0)
#define CNTP_CTL_IMASK      (1 << 1)
#define CNTP_CTL_ISTATUS    (1 << 2)

static unsigned long generic_timer_rate;

static struct delay_timer arch_delay_timer;

static struct clock_event_device __percpu **timer_evt;
static int timer_ppi, timer_ns_ppi;

static void generic_timer_set_mode(enum clock_event_mode mode, struct clock_event_device *clk)
{
	unsigned long ctrl;

	switch (mode) {
	case CLOCK_EVT_MODE_ONESHOT:
		ctrl = CNTP_CTL_ENABLE;
		break;
	case CLOCK_EVT_MODE_PERIODIC:
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
	default:
		ctrl = CNTP_CTL_IMASK;
	}

	write_cntp_ctl(ctrl);
}

static int generic_timer_set_next_event(unsigned long evt, struct clock_event_device *unused)
{
	write_cntp_tval(evt);
	write_cntp_ctl(CNTP_CTL_ENABLE);

#ifdef LOCAL_TIME_DEBUG
	save_localtimer_register(evt, ctrl, 0);
#endif
	return 0;
}

int localtimer_set_next_event(unsigned long evt)
{
	generic_timer_set_next_event(evt, NULL);
#ifdef LOCAL_TIME_DEBUG
	save_localtimer_register(evt, 0, 1);
#endif
	return 0;
}

unsigned long localtimer_get_counter(void)
{
	unsigned long evt;
	read_cntp_tval(evt);

	return evt;
}

static unsigned long arch_timer_read_current_timer(void)
{
	unsigned long evtl, evth;
	read_cntpct(evtl, evth);

	return evtl;
}


/*
 * generic_timer_ack: checks for a local timer interrupt.
 *
 * If a local timer interrupt has occurred, acknowledge and return 1.
 * Otherwise, return 0.
 */
static int generic_timer_ack(void)
{
	unsigned int cntp_ctl;
	read_cntp_ctl(cntp_ctl);

	if (cntp_ctl & CNTP_CTL_ISTATUS) {
		write_cntp_ctl(CNTP_CTL_IMASK);
		return 1;
	}

	pr_info("WARNING: Generic Timer CNTP_CTL = 0x%x\n", cntp_ctl);
	return 0;
}

static void generic_timer_stop(struct clock_event_device *clk)
{
	generic_timer_set_mode(CLOCK_EVT_MODE_UNUSED, clk);
	disable_percpu_irq(GIC_PPI_PRIVATE_TIMER);
	disable_percpu_irq(GIC_PPI_NS_PRIVATE_TIMER);
}

static int get_generic_timer_rate(void)
{
	/* !!!!!FIXME!!!!! Hard code to 13Mhz for now, should change to
	   code get GPT6 frequency. */
	return 13000000;
}

static void __cpuinit generic_timer_calibrate_rate(void)
{
	/*
	 * If this is the first time round, get timer rate
	 */
	if (generic_timer_rate == 0) {
		generic_timer_rate = get_generic_timer_rate();
	}
}

static irqreturn_t timer_handler(int irq, void *dev_id)
{
	struct clock_event_device *evt = *(struct clock_event_device **)dev_id;
/* #ifdef CONFIG_MT_SCHED_MONITOR */
#if 0
	/* add timer event tracer for wdt debug */
	__raw_get_cpu_var(local_timer_ts) = sched_clock();
	if (generic_timer_ack()) {
		evt->event_handler(evt);
		__raw_get_cpu_var(local_timer_te) = sched_clock();
		return IRQ_HANDLED;
	}
	__raw_get_cpu_var(local_timer_te) = sched_clock();
	return IRQ_NONE;
#else
	if (generic_timer_ack()) {
		evt->event_handler(evt);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
#endif
}


/*
 * Setup the local clock events for a CPU.
 */
static int __cpuinit generic_timer_setup(struct clock_event_device *clk)
{
	struct clock_event_device **this_cpu_clk;

	generic_timer_calibrate_rate();

	write_cntp_ctl(0x0);

	clk->name = "generic_timer";
	clk->features = CLOCK_EVT_FEAT_ONESHOT;
	clk->rating = 350;
	clk->set_mode = generic_timer_set_mode;
	clk->set_next_event = generic_timer_set_next_event;
	clk->irq = timer_ppi;

	this_cpu_clk = __this_cpu_ptr(timer_evt);
	*this_cpu_clk = clk;

	clockevents_config_and_register(clk, generic_timer_rate, 0xf, 0x7fffffff);

	enable_percpu_irq(GIC_PPI_PRIVATE_TIMER, 0);
	enable_percpu_irq(GIC_PPI_NS_PRIVATE_TIMER, 0);

	return 0;
}

static struct local_timer_ops generic_timer_ops __cpuinitdata = {
	.setup = generic_timer_setup,
	.stop = generic_timer_stop,
};

int __init generic_timer_register(void)
{
	int err, err2;

	if (timer_evt)
		return -EBUSY;

	timer_ppi = GIC_PPI_PRIVATE_TIMER;
	timer_ns_ppi = GIC_PPI_NS_PRIVATE_TIMER;

	timer_evt = alloc_percpu(struct clock_event_device *);

	if (!timer_evt) {
		err = -ENOMEM;
		goto out_exit;
	}

	err = request_percpu_irq(timer_ppi, timer_handler, "timer", timer_evt);
	err2 = request_percpu_irq(timer_ns_ppi, timer_handler, "timer", timer_evt);
	if (err && err2) {
		pr_err("generic timer: can't register interrupt %d (%d) %d (%d)\n",
		       timer_ppi, err, timer_ns_ppi, err2);
		goto out_free;
	}

	err = local_timer_register(&generic_timer_ops);
	if (err)
		goto out_irq;

	/* Use the architected timer for the delay loop. */
	arch_delay_timer.read_current_timer = &arch_timer_read_current_timer;
	arch_delay_timer.freq = get_generic_timer_rate();
	register_current_timer_delay(&arch_delay_timer);

	return 0;

 out_irq:
	free_percpu_irq(timer_ppi, timer_evt);
	free_percpu_irq(timer_ns_ppi, timer_evt);
 out_free:
	free_percpu(timer_evt);
	timer_evt = NULL;
 out_exit:
	return err;
}

/* #include <linux/mt_sched_mon.h> */
/* #define LOCAL_TIME_DEBUG */
#ifdef LOCAL_TIME_DEBUG
static unsigned long long save_data[4][4] = { {0}, {0}, {0}, {0} };	/* max cpu 4 */

void save_localtimer_register(unsigned long count, unsigned long ctrl, unsigned long dpidle)
{
	int cpu = smp_processor_id();
	save_data[cpu][0] = count;
	save_data[cpu][1] = ctrl;
	save_data[cpu][2] = dpidle;
	save_data[cpu][3] = sched_clock();
}

int dump_localtimer_register(char *buffer, int size)
{
	int i;
	int len = 0;
	if (!buffer) {
		return -1;
	}

	for (i = 0; i < nr_cpu_ids; i++) {
		len += snprintf(buffer + len, size,
				"[local timer]old:cpu:%d,count:0x%x, ctrl:0x%x,dpidle:%d, time:%llu\n",
				i, (int)save_data[i][0], (int)save_data[i][1], (int)save_data[i][2],
				save_data[i][3]);
	}

	len += snprintf(buffer + len, size,
			"[local timer]new: count:0x%x,control:0x%x,load:0x%x\n",
			*((volatile u32 *)(TWD_TIMER_COUNTER)),
			*((volatile u32 *)(TWD_TIMER_CONTROL)),
			*((volatile u32 *)(TWD_TIMER_LOAD)));

	len += snprintf(buffer + len, size, "[local timer]NTSTAT:0x%x\n",
			*((volatile u32 *)(TWD_TIMER_INTSTAT)));
	len += snprintf(buffer + len, size, "[local timer]generic_timer_rate:%lu\n",
			generic_timer_rate);

	if (len >= size) {
		return -2;	/* buffer is full */
	}

	return len;
}
#endif


int dump_localtimer_info(char *buffer, int size)
{
	return 0;
}


#if 0

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/err.h>

#include <asm/uaccess.h>
#include <mach/mt_gpt.h>

static int cpuid[NR_CPUS] = { 0 };

static struct completion notify[NR_CPUS];
static struct completion ack;
static unsigned int opcode;
static unsigned int op1;
static unsigned int op2;

static DEFINE_MUTEX(opcode_lock);
static DEFINE_SPINLOCK(cpu_lock);


void dump_timer_regs(void)
{
#if 0
	unsigned int cntfrq = 0xFFFFFFFF;
	unsigned int cntkctl = 0xFFFFFFFF;
#endif
	unsigned int cntpct_lo = 0xFFFFFFFF;
	unsigned int cntpct_hi = 0xFFFFFFFF;
#if 0
	unsigned int cntvct_lo = 0xFFFFFFFF;
	unsigned int cntvct_hi = 0xFFFFFFFF;
#endif
	unsigned int cntp_ctl = 0xFFFFFFFF;
	unsigned int cntp_cval_lo = 0xFFFFFFFF;
	unsigned int cntp_cval_hi = 0xFFFFFFFF;
	unsigned int cntp_tval = 0xFFFFFFFF;
#if 0
	unsigned int cntv_ctl = 0xFFFFFFFF;
	unsigned int cntv_cval_lo = 0xFFFFFFFF;
	unsigned int cntv_cval_hi = 0xFFFFFFFF;
	unsigned int cntv_tval = 0xFFFFFFFF;
#endif

#if 0
	read_cntfrq(cntfrq);
	read_cntkctl(cntkctl);
#endif
	read_cntpct(cntpct_lo, cntpct_hi);
#if 0
	read_cntvct(cntvct_lo, cntvct_hi);
#endif
	read_cntp_ctl(cntp_ctl);
	read_cntp_cval(cntp_cval_lo, cntp_cval_hi);
	read_cntp_tval(cntp_tval);
#if 0
	read_cntv_ctl(cntv_ctl);
	read_cntv_cval(cntv_cval_lo, cntv_cval_hi);
	read_cntv_tval(cntv_tval);
#endif

#if 0
	pr_info("[ca7_timer]0. cntfrq = 0x%x\n", cntfrq);
	pr_info("[ca7_timer]1. cntkctl = 0x%x\n", cntkctl);
#endif
	pr_info("[ca7_timer]2. cntpct_lo = 0x%08x, cntpct_hi = 0x%08x\n", cntpct_lo, cntpct_hi);
#if 0
	pr_info("[ca7_timer]3. cntvct_lo = 0x%08x, cntvct_hi = 0x%08x\n", cntvct_lo, cntvct_hi);
#endif
	pr_info("[ca7_timer]4. cntp_ctl = 0x%x\n", cntp_ctl);
	pr_info("[ca7_timer]5. cntp_cval_lo = 0x%08x, cntp_cval_hi = 0x%08x\n", cntp_cval_lo,
		cntp_cval_hi);
	pr_info("[ca7_timer]6. cntp_tval = 0x%08x\n", cntp_tval);
#if 0
	pr_info("[ca7_timer]7. cntv_ctl = 0x%x\n", cntv_ctl);
	pr_info("[ca7_timer]8. cntv_cval_lo = 0x%08x, cntv_cval_hi = 0x%08x\n", cntv_cval_lo,
		cntv_cval_hi);
	pr_info("[ca7_timer]9. cntv_tval = 0x%08x\n", cntv_tval);
#endif
}


static int test_ack(void)
{
	unsigned int cntp_ctl;
	read_cntp_ctl(cntp_ctl);

	pr_info("test_ack: CNTP_CTL = 0x%x\n", cntp_ctl);

	if (cntp_ctl & CNTP_CTL_ISTATUS) {
		write_cntp_ctl(CNTP_CTL_IMASK);
		return 1;
	}

	return 0;
}

static irqreturn_t test_handler(int irq, void *dev_id)
{
	if (test_ack()) {
		/* unsigned int pos = gpt_get_cnt(GPT3); */
		/* pr_info("[generic_timer:%s]entry, pos=%lu\n", __func__, pos); */
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}


static void test_operation(void)
{
	if (opcode == 0) {
		dump_timer_regs();
	} else if (opcode == 1) {
		write_cntp_tval(op1);
		write_cntp_ctl(op2);
	}
}


static int local_timer_test(void *data)
{
	int cpu = *(int *)data;

	pr_info("[%s]: thread for cpu%d start\n", __func__, cpu);
	enable_percpu_irq(GIC_PPI_PRIVATE_TIMER, 0);
	enable_percpu_irq(GIC_PPI_NS_PRIVATE_TIMER, 0);

	while (1) {
		wait_for_completion(&notify[cpu]);
		test_operation();
		complete(&ack);
	}

	pr_info("[%s]: thread for cpu%d stop\n", __func__, cpu);
	return 0;
}

void local_timer_test_init(void)
{
	int err = 0, err2;
	int i = 0;
	unsigned char name[10] = { '\0' };
	struct task_struct *thread[nr_cpu_ids];
	static struct clock_event_device *evt;

	err = request_gpt(GPT6, GPT_FREE_RUN, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1, 0, NULL, 0);
	if (err) {
		pr_err("fail to request gpt, err=%d\n", err);
	}

	err = request_percpu_irq(GIC_PPI_PRIVATE_TIMER, test_handler, "timer", &evt);
	err2 = request_percpu_irq(GIC_PPI_NS_PRIVATE_TIMER, timer_handler, "timer", timer_evt);
	if (err && err2) {
		pr_err("can't register interrupt %d, err=%d, %d, err=%d\n",
		       GIC_PPI_PRIVATE_TIMER, err, GIC_PPI_NS_PRIVATE_TIMER, err2);
	}

	init_completion(&ack);
	for (i = 0; i < nr_cpu_ids; i++) {
		cpuid[i] = i;
		init_completion(&notify[i]);
		sprintf(name, "timer-%d", i);
		thread[i] = kthread_create(local_timer_test, &cpuid[i], name);
		if (IS_ERR(thread[i])) {
			err = PTR_ERR(thread[i]);
			thread[i] = NULL;
			pr_err("[%s]: kthread_create %s fail(%d)\n", __func__, name, err);
			return;
		}
		kthread_bind(thread[i], i);
		wake_up_process(thread[i]);
	}
}


static int local_timer_test_read(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	char *p = page;
	int len = 0;

	p += sprintf(p, "********** ca7 timer debug help *********\n");
	p += sprintf(p, "echo opcode cpumask > /proc/lttest\n");
	p += sprintf(p, "opcode:\n");
	p += sprintf(p, "0: dump register\n");
	p += sprintf(p, "1: count down\n");
	p += sprintf(p, "2: count up\n");

	*start = page + off;

	len = p - page;
	if (len > off)
		len -= off;
	else
		len = 0;

	*eof = 1;

	return len < count ? len : count;
}

static int local_timer_test_write(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	char desc[32];
	int len = 0;

	unsigned int i = 0;
	unsigned int cpu = 0;

	unsigned int mask = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len)) {
		return 0;
	}
	desc[len] = '\0';

	spin_lock(&cpu_lock);
	cpu = smp_processor_id();
	spin_unlock(&cpu_lock);
	pr_info("[%s]trigger test on cpu%u\n", __func__, cpu);

	mutex_lock(&opcode_lock);

	opcode = 0;
	op1 = 0;
	op2 = 0;

	i = sscanf(desc, "%u %x %x %x", &opcode, &mask, &op1, &op2);
	pr_info("opcode=%u, mask=%x, op1=%x, op2=%x\n", opcode, mask, op1, op2);

	for (i = 0; i < nr_cpu_ids; i++) {
		if (mask & (0x1 << i)) {
			complete(&notify[i]);
			wait_for_completion(&ack);
		}
	}

	mutex_unlock(&opcode_lock);

	return count;
}


static int __init local_timer_test_mod_init(void)
{
	struct proc_dir_entry *entry = NULL;

	entry = create_proc_entry("lttest", S_IRUGO | S_IWUSR, NULL);
	if (entry) {
		entry->read_proc = local_timer_test_read;
		entry->write_proc = local_timer_test_write;
	}

	local_timer_test_init();

	return 0;
}
module_init(local_timer_test_mod_init);
#endif
