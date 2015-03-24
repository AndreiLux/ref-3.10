#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <asm/system.h>
#include <asm/signal.h>
#include <asm/ptrace.h>
#include <mach/hw_watchpoint.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#define DBGWVR      0x180
#define DBGWCR      0x1C0

#define DBGLAR      0xFB0
#define DBGLSR      0xFB4
#define DBGDSCR     0x088
#define DBGWFAR     0x018
#define DBGOSLAR    0x300
#define DBGOSSAR    0x304
#define DBGOSSRR    0x308
#define MAX_NR_WATCH_POINT 4
#define NUM_CPU    4

static unsigned int reg_cpudbg[] = { 0xF0170000, 0xF0172000, 0xF0150000, 0xF0152000 };

#define UNLOCK_KEY 0xC5ACCE55
#define HDBGEN (1 << 14)
#define MDBGEN (1 << 15)
#define DBGWCR_VAL 0x000001E7
 /**/
#define WP_EN (1 << 0)
#define LSC_LDR (1 << 3)
#define LSC_STR (2 << 3)
#define LSC_ALL (3 << 3)
#define OS_LOCK_IMPLEMENTED (1 << 0)
#define LSR_LOCKED (1 << 1);
#define WATCHPOINT_DEBUG  0
#if (WATCHPOINT_DEBUG == 1)
#define dbgmsg printk
#else
#define dbgmsg(...)
#endif
#define HW_WATCHPOINT_TEST
#ifdef HW_WATCHPOINT_TEST
struct wp_event wp_event;
int err;
volatile int my_watch_data;

static struct platform_driver mt_wp_drv = {
	.driver = {
		   .name = "wp",
		   .bus = &platform_bus_type,
		   .owner = THIS_MODULE,
		   },
};


int my_wp_handler1(unsigned int addr)
{

	pr_info("cpu %d Access my data from an instruction at 0x%x\n", raw_smp_processor_id(),
		addr);
	return 0;
}

int my_wp_handler2(unsigned int addr)
{
	pr_info("Access my data from an instruction at 0x%x\n", addr);
	/* trigger exception */
	return 1;
}

int cpu_task(void)
{
	pr_info("cpu %d set my_watch_data\n", raw_smp_processor_id());
	my_watch_data = 1;
}


static ssize_t foo_show(struct device_driver *driver, char *buf)
{

	int test;
	int i;
	init_wp_event(&wp_event, &my_watch_data, &my_watch_data, WP_EVENT_TYPE_ALL, my_wp_handler1);

	err = add_hw_watchpoint(&wp_event);
	if (err) {
		pr_info("add hw watch point failed...\n");
		/* fail to add watchpoing */
	} else {
		/* the memory address is under watching */
		pr_info("add hw watch point success...\n");

		/* del_hw_watchpoint(&wp_event); */
	}
	/* test watchpoint */

	for (i = 0; i < NUM_CPU; i++) {
		err = -1;
		err = smp_call_function_single(i, cpu_task, NULL, 1);
		if (err == 0) {
			dbgmsg("cpu %d task finish scuess\n", i);
		} else {
			dbgmsg("cpu %d task fasiled\n", i);
		}
	}
	del_hw_watchpoint(&wp_event);
	return 0;

}


static ssize_t foo_store(struct device_driver *driver, const char *buf, size_t count)
{
	dbgmsg("In foo_store\n");
	return 0;
}

DRIVER_ATTR(wpt_test, 0644, foo_show, foo_store);

#endif




static struct wp_event wp_events[MAX_NR_WATCH_POINT];
static spinlock_t wp_lock;

/*
 * enable_hw_watchpoint: Enable the H/W watchpoint.
 * Return error code.
 */
static int enable_hw_watchpoint(void)
{
	int i;

	for (i = 0; i < NUM_CPU; i++) {
		if (*(volatile unsigned int *)(reg_cpudbg[i] + DBGDSCR) & HDBGEN) {
			dbgmsg(KERN_ALERT
			       "halting debug mode enabled. Unable to access hardware resources.\n");
			return -EPERM;
		}

		if (*(volatile unsigned int *)(reg_cpudbg[i] + DBGDSCR) & MDBGEN) {
			dbgmsg("cpu %d already enabled DBGDSCR: 0x%x\n", i,
			       *(volatile unsigned int *)(reg_cpudbg[i] + DBGDSCR));
			return 0;
		}
		*(volatile unsigned int *)(reg_cpudbg[i] + DBGLAR) = UNLOCK_KEY;
		*(volatile unsigned int *)(reg_cpudbg[i] + DBGOSLAR) = ~UNLOCK_KEY;
		*(volatile unsigned int *)(reg_cpudbg[i] + DBGDSCR) |= MDBGEN;
	}
	return 0;
}

/*
 * add_hw_watchpoint: add a watch point.
 * @wp_event: pointer to the struct wp_event.
 * Return error code.
 */
int add_hw_watchpoint(struct wp_event *wp_event)
{
	int ret, i, j;
	unsigned long flags;
	unsigned int ctl;

	if (!wp_event) {
		return -EINVAL;
	}
	if (!(wp_event->handler)) {
		return -EINVAL;
	}

	ret = enable_hw_watchpoint();
	if (ret) {
		return ret;
	}

	ctl = DBGWCR_VAL;
	if (wp_event->type == WP_EVENT_TYPE_READ) {
		ctl |= LSC_LDR;
	} else if (wp_event->type == WP_EVENT_TYPE_WRITE) {
		ctl |= LSC_STR;
	} else if (wp_event->type == WP_EVENT_TYPE_ALL) {
		ctl |= LSC_ALL;
	} else {
		return -EINVAL;
	}

	spin_lock_irqsave(&wp_lock, flags);
	for (i = 0; i < MAX_NR_WATCH_POINT; i++) {
		if (!wp_events[i].in_use) {
			wp_events[i].in_use = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&wp_lock, flags);

	if (i == MAX_NR_WATCH_POINT) {
		return -EAGAIN;
	}

	wp_events[i].virt = wp_event->virt & ~3;	/* enforce word-aligned */
	wp_events[i].phys = wp_event->phys;	/* no use currently */
	wp_events[i].type = wp_event->type;
	wp_events[i].handler = wp_event->handler;
	wp_events[i].auto_disable = wp_event->auto_disable;


	pr_alert("Add watchpoint %d at address 0x%x\n", i, wp_events[i].virt);
	for (j = 0; j < NUM_CPU; j++) {
		*(((volatile unsigned int *)(reg_cpudbg[j] + DBGWVR)) + i) = wp_events[i].virt;
		*(((volatile unsigned int *)(reg_cpudbg[j] + DBGWCR)) + i) = ctl;

		pr_alert("*(((volatile unsigned int *)(DBGWVR + 0x%4lx)) + %d) = 0x%x\n",
			 reg_cpudbg[j], i,
			 *(((volatile unsigned int *)(reg_cpudbg[j] + DBGWVR)) + i));
		pr_alert("*(((volatile unsigned int *)(DBGWCR + 0x%4lx)) + %d) = 0x%x\n",
			 reg_cpudbg[j], i,
			 *(((volatile unsigned int *)(reg_cpudbg[j] + DBGWCR)) + i));
	}
	return 0;
}

/*
 * del_hw_watchpoint: delete a watch point.
 * @wp_event: pointer to the struct wp_event.
 * Return error code.
 */
int del_hw_watchpoint(struct wp_event *wp_event)
{
	unsigned long flags;
	int i, j;

	if (!wp_event) {
		return -EINVAL;
	}

	spin_lock_irqsave(&wp_lock, flags);
	for (i = 0; i < MAX_NR_WATCH_POINT; i++) {
		if (wp_events[i].in_use && (wp_events[i].virt == wp_event->virt)) {
			wp_events[i].virt = 0;
			wp_events[i].phys = 0;
			wp_events[i].type = 0;
			wp_events[i].handler = NULL;
			wp_events[i].in_use = 0;
			for (j = 0; j < NUM_CPU; j++)
				*(((volatile unsigned int *)(reg_cpudbg[j] + DBGWCR)) + i) &=
				    ~WP_EN;
			break;
		}
	}

	spin_unlock_irqrestore(&wp_lock, flags);

	if (i == MAX_NR_WATCH_POINT) {
		return -EINVAL;
	} else {
		return 0;
	}

}

int watchpoint_handler(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
{
	unsigned int wfar, daddr, iaddr;
	int i, ret, j;
	int cpu;
#if defined CONFIG_ARCH_MT8135
/* Notes
 *v7 Debug the address of instruction that triggered the watchpoint is in DBGWFAR
 *v7.1 Debug the address is in DFAR
*/
	asm volatile ("MRC p15, 0, %0, c6, c0, 0\n":"=r" (wfar)
 :  : "cc");
#else
	cpu = raw_smp_processor_id();
	wfar = *(volatile unsigned int *)(DBGWFAR + reg_cpudbg[cpu]);
#endif
	daddr = addr & ~3;
	iaddr = regs->ARM_pc;
	dbgmsg(KERN_ALERT "addr = 0x%x, DBGWFAR/DFAR = 0x%x\n", (unsigned int)addr, wfar);
	dbgmsg(KERN_ALERT "daddr = 0x%x, iaddr = 0x%x\n", daddr, iaddr);

	/* update PC to avoid re-execution of the instruction under watching */
	regs->ARM_pc += thumb_mode(regs) ? 2 : 4;

	for (i = 0; i < MAX_NR_WATCH_POINT; i++) {
		if (wp_events[i].in_use && wp_events[i].virt == (daddr)) {
			dbgmsg(KERN_ALERT "Watchpoint %d triggers.\n", i);
			if (wp_events[i].handler) {
				if (wp_events[i].auto_disable) {
					for (j = 0; j < NUM_CPU; j++)
						*(((volatile unsigned int *)(reg_cpudbg[j] +
									     DBGWCR)) + i) &=
						    ~WP_EN;
				}
				ret = wp_events[i].handler(iaddr);
				if (wp_events[i].auto_disable) {
					for (j = 0; j < NUM_CPU; j++)
						*(((volatile unsigned int *)(reg_cpudbg[j] +
									     DBGWCR)) + i) |= WP_EN;
				}
				return ret;
			} else {
				dbgmsg(KERN_ALERT "No watchpoint handler. Ignore.\n");
				return 0;
			}
		}
	}

	return 0;
}


static void __exit wpt_exit(void)
{
	return;
}

static int __init hw_watchpoint_init(void)
{
	int ret;
	spin_lock_init(&wp_lock);

	hook_fault_code(2, watchpoint_handler, SIGTRAP, 0, "watchpoint debug exception");
	pr_alert(" watchpoint handler init.\n");


#ifdef HW_WATCHPOINT_TEST
	ret = driver_register(&mt_wp_drv.driver);
	if (ret) {
		pr_err("Fail to register mt_eint_drv");
	}

	ret = driver_create_file(&mt_wp_drv.driver, &driver_attr_wpt_test);
	if (ret) {
		pr_err("Fail to create mt_eint_drv sysfs files");
	}

	dbgmsg("create sysfs interface complete\n");
#endif
	return 0;
}
arch_initcall(hw_watchpoint_init);
