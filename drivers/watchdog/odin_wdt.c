#include <linux/device.h>
#include <linux/resource.h>
#include <linux/amba/bus.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include <linux/amba/bus.h>
#include <linux/irq.h>
#include <linux/irqnr.h>
#include <linux/percpu.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/odin_panic.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/nmi.h>
#include <linux/odin_wdt.h>
#include <asm/cacheflush.h>
#include <asm/ptrace.h>
#ifdef CONFIG_PSTORE_FTRACE
#include <linux/reboot.h>

extern odin_push_panic(void);
#endif

struct sp805_wdt *odin_wdt_ext = NULL ;

static DEFINE_SPINLOCK(wdt_inter_lock);

/* default timeout in seconds */
#define DEFAULT_TIMEOUT		60

#define MODULE_NAME		"sp805-wdt"
#define BARK 0
#define BITE 1

/* watchdog register offsets and masks */
#define WDTLOAD			0x000
	#define LOAD_MIN	0x00000001
	#define LOAD_MAX	0xFFFFFFFF
#define WDTVALUE		0x004
#define WDTCONTROL		0x008
	/* control register masks */
	#define	INT_ENABLE	(1 << 0)
	#define	RESET_ENABLE	(1 << 1)
#define WDTINTCLR		0x00C
#define WDTRIS			0x010
#define WDTMIS			0x014
	#define INT_MASK	(1 << 0)
#define WDTLOCK			0xC00
	#define	UNLOCK		0x1ACCE551
	#define	LOCK		0x00000001

#define MASK_SIZE		32
#define SP805_BASE		0x20025000

static struct workqueue_struct *watchdog_wq;

/**
 * struct sp805_wdt: sp805 wdt device structure
 * @wdd: instance of struct watchdog_device
 * @lock: spin lock protecting dev structure and io access
 * @base: base address of wdt
 * @clk: clock structure of wdt
 * @adev: amba device structure of wdt
 * @status: current status of wdt
 * @load_val: load value to be set for current timeout
 * @timeout: current programmed timeout
 */
struct sp805_wdt {
	struct watchdog_device wdd;
	spinlock_t lock;
	void __iomem *base;
	struct clk *clk;
	struct amba_device *adev;
	struct device *dev;
	unsigned int load_val;
	unsigned int bite_irq;
	unsigned int timeout;
	unsigned int pet_time;
	unsigned int print_t_status;
	unsigned int bark_or_bite;
	bool print_lock;
	bool do_ipi_ping;
	void *scm_regsave;
	u64 clk_rate;
	cpumask_t alive_mask;
	struct work_struct init_dogwork_struct;
	struct delayed_work dogwork_struct;
	struct sp805_wdt __percpu **wdog_cpu_dd;
	struct notifier_block panic_blk;
	struct kobject		kobj;
	struct completion	kobj_unregister;
	struct mutex disable_lock;
};

/*
 * On the kernel command line specify
 * odin_watchdog.enable=1 to enable the watchdog
 * By default watchdog is turned on
 */
static int enable = 1;
module_param(enable, int, 0);
static int reset_flag = 0;
static int bark_cnt = 0;
static bool bark_panic = false;
unsigned long long last_pet_time;
void __iomem *odin_wdt_base;
u64 nmi_load;

/*wdt module struct*/
int curWDT = -1;

struct wdt_attr {
    struct attribute attr;
    char *wdt_name;
	int value;
};

static struct wdt_attr set_pettime = {
    .attr.name="set_pettime",
    .attr.mode = 0644,
    .value = 0,
};

static struct wdt_attr set_timeout = {
    .attr.name="set_timeout",
    .attr.mode = 0644,
    .value = 1,
};

static struct wdt_attr wdt_off = {
    .attr.name="wdt_off",
    .attr.mode = 0644,
    .value = 2,
};

static struct wdt_attr bark_panic_on = {
    .attr.name="bark_panic_on",
    .attr.mode = 0644,
    .value = 3,
};

static struct wdt_attr force_reset = {
    .attr.name="force_reset",
    .attr.mode = 0644,
    .value = 4,
};

static struct attribute * wdtattr[] = {
    &set_pettime.attr,
    &set_timeout.attr,
    &wdt_off.attr,
    &bark_panic_on.attr,
    &force_reset.attr,
    NULL
};

#define to_wdt_s(k) container_of(k, struct sp805_wdt, kobj)
/****************************[from sp805 driver]***************************/

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		"Set to 1 to keep watchdog running after device release");

unsigned char is_watchdog_crashed=0;
unsigned char get_watchdog_crash_status()
{
	return is_watchdog_crashed;
}
EXPORT_SYMBOL(get_watchdog_crash_status);


/* This routine finds load value that will reset system in required timout */
static int wdt_setload(struct watchdog_device *wdd, unsigned int timeout)
{
	struct sp805_wdt *wdt = watchdog_get_drvdata(wdd);
	unsigned long flags;
	u64 load;

	/*
	 * sp805 runs counter with given value twice, after the end of first
	 * counter it gives an interrupt and then starts counter again. If
	 * interrupt already occurred then it resets the system. This is why
	 * load is half of what should be required.
	 */
	load = wdt->clk_rate * timeout - 1;

	load = (load > LOAD_MAX) ? LOAD_MAX : load;
	load = (load < LOAD_MIN) ? LOAD_MIN : load;

	nmi_load = load;

	spin_lock_irqsave(&wdt->lock, flags);
	wdt->load_val = load;
	/* roundup timeout to closest positive integer value */
	wdt->timeout = div_u64((load + 1) + (wdt->clk_rate / 2), wdt->clk_rate);
	spin_unlock_irqrestore(&wdt->lock, flags);

	//printk(KERN_INFO "odin wdt timeout set: %ds / bark count:%d\n",
	//	wdt->timeout, bark_cnt);

	return 0;
}

/* returns number of seconds left for reset to occur */
static unsigned int wdt_timeleft(struct watchdog_device *wdd)
{
	struct sp805_wdt *wdt = watchdog_get_drvdata(wdd);
	unsigned long flags;
	u64 load, rate;

	rate = clk_get_rate(wdt->clk);

	spin_lock_irqsave(&wdt->lock, flags);
	load = readl_relaxed(wdt->base + WDTVALUE);

	/*If the interrupt is inactive then time left is WDTValue + WDTLoad. */
	if (!(readl_relaxed(wdt->base + WDTRIS) & INT_MASK))
		load += wdt->load_val + 1;
	spin_unlock_irqrestore(&wdt->lock, flags);

	return div_u64(load, rate);
}

static int wdt_config(struct watchdog_device *wdd, bool ping)
{
	struct sp805_wdt *wdt = watchdog_get_drvdata(wdd);
	unsigned long flags;

	spin_lock_irqsave(&wdt->lock, flags);
	__raw_writel(UNLOCK, wdt->base + WDTLOCK);
	__raw_writel(wdt->load_val, wdt->base + WDTLOAD);

	if (!ping) {
		__raw_writel(INT_MASK, wdt->base + WDTINTCLR);
		__raw_writel(INT_ENABLE | RESET_ENABLE, wdt->base +
				WDTCONTROL);
	}

	__raw_writel(LOCK, wdt->base + WDTLOCK);

	/* Flush posted writes. */
	readl_relaxed(wdt->base + WDTLOCK);
	spin_unlock_irqrestore(&wdt->lock, flags);

	return 0;
}

static int wdt_ping(struct watchdog_device *wdd)
{
	return wdt_config(wdd, true);
}

/* enables watchdog timers reset */
static int wdt_enable(struct watchdog_device *wdd)
{
	return wdt_config(wdd, false);
}

/* disables watchdog timers reset */
static int wdt_disable(struct watchdog_device *wdd)
{
	struct sp805_wdt *wdt = watchdog_get_drvdata(wdd);
	unsigned long flags;

	spin_lock_irqsave(&wdt->lock, flags);

	writel_relaxed(UNLOCK, wdt->base + WDTLOCK);
	writel_relaxed(0, wdt->base + WDTCONTROL);
	writel_relaxed(LOCK, wdt->base + WDTLOCK);

	/* Flush posted writes. */
	readl_relaxed(wdt->base + WDTLOCK);
	spin_unlock_irqrestore(&wdt->lock, flags);

	return 0;
}

static const struct watchdog_info wdt_info = {
	.options = WDIOF_MAGICCLOSE | WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
	.identity = MODULE_NAME,
};

static const struct watchdog_ops wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= wdt_enable,
	.stop		= wdt_disable,
	.ping		= wdt_ping,
	.set_timeout	= wdt_setload,
	.get_timeleft	= wdt_timeleft,
};

/****************************[from sp805 driver end]***************************/
void odin_show_task_state(bool print_lock, int print_t_status)
{
	struct task_struct *g, *p;

	console_loglevel = 7;
	rcu_read_lock();
	do_each_thread(g, p) {
		/*
		 * reset the NMI-timeout, listing all files on a slow
		 * console might take a lot of time:
		 */
		touch_nmi_watchdog();
		if (p->state == print_t_status)
			sched_show_task(p);
	} while_each_thread(g, p);

	touch_all_softlockup_watchdogs();

	rcu_read_unlock();
	/*
	 * Only show locks if all tasks are dumped:
	 */
	if(print_lock)
		debug_show_all_locks();
}

static void dump_cpu_alive_mask(struct sp805_wdt *wdt)
{
	static char alive_mask_buf[MASK_SIZE];
	cpumask_t dead_mask;

	cpumask_complement(&dead_mask, &wdt->alive_mask);
	cpulist_scnprintf(alive_mask_buf, MASK_SIZE,
						&wdt->alive_mask);

	if(0 == strcmp(alive_mask_buf, "1-7")) {
		printk(KERN_INFO "cpu work queue sechedule error!!\n");
		printk(KERN_INFO "cpu alive mask from last pet %s\n", alive_mask_buf);
		cpulist_scnprintf(alive_mask_buf, MASK_SIZE,
						&dead_mask);

		printk(KERN_INFO "The work is not executed by cpu no %s!!\n", alive_mask_buf);
		wdt->print_lock = true;
		wdt->print_t_status = TASK_UNINTERRUPTIBLE;
		wdt->bark_or_bite = BARK;
	} else {
		cpulist_scnprintf(alive_mask_buf, MASK_SIZE,
						&dead_mask);
		printk(KERN_INFO "cpu no %s has some issues!!\n", alive_mask_buf);
		wdt->print_lock = false;
		wdt->print_t_status = TASK_RUNNING;
		wdt->bark_or_bite = BITE;
	}
}

static int panic_wdog_handler(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	struct sp805_wdt *wdt = container_of(this,
				struct sp805_wdt, panic_blk);
	if (panic_timeout == 0) {
		printk(KERN_INFO "%s panic_time out\n", __func__);
		wdt_disable(&wdt->wdd);
		mb();
	} else {
		wdt_setload(&wdt->wdd, DEFAULT_TIMEOUT);
	}
	return NOTIFY_DONE;
}

static void wdog_disable(struct sp805_wdt *wdt)
{
	wdt_disable(&wdt->wdd);
	mb();

	enable = 0;
	/*Ensure all cpus see update to enable*/
	smp_mb();
	atomic_notifier_chain_unregister(&panic_notifier_list,
						&wdt->panic_blk);
	cancel_delayed_work_sync(&wdt->dogwork_struct);

	/* may be suspended after the first write above */
	wdt_disable(&wdt->wdd);
	mb();
	pr_info("ODIN Apps Watchdog deactivated.\n");
}

struct wdog_disable_work_data {
	struct work_struct work;
	struct completion complete;
	struct sp805_wdt *wdt;
};

static void wdog_disable_work(struct work_struct *work)
{
	struct wdog_disable_work_data *work_data =
		container_of(work, struct wdog_disable_work_data, work);
	wdog_disable(work_data->wdt);
	complete(&work_data->complete);
}

static ssize_t wdog_disable_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	struct sp805_wdt *wdt = dev_get_drvdata(dev);

	mutex_lock(&wdt->disable_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", enable == 0 ? 1 : 0);
	mutex_unlock(&wdt->disable_lock);
	return ret;
}

static ssize_t wdog_disable_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u8 disable;
	struct wdog_disable_work_data work_data;
	struct sp805_wdt *wdt = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 10, &disable);
	if (ret) {
		dev_err(wdt->dev, "invalid user input\n");
		return ret;
	}
	if (disable == 1) {
		mutex_lock(&wdt->disable_lock);
		if (enable == 0) {
			pr_info("ODIN Apps Watchdog already disabled\n");
			mutex_unlock(&wdt->disable_lock);
			return count;
		}
		disable = 1;

		work_data.wdt = wdt;
		init_completion(&work_data.complete);
		INIT_WORK_ONSTACK(&work_data.work, wdog_disable_work);
		queue_work_on(0, watchdog_wq, &work_data.work);
		wait_for_completion(&work_data.complete);
		mutex_unlock(&wdt->disable_lock);
	} else {
		pr_err("invalid operation, only disable = 1 supported\n");
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(disable, S_IWUSR | S_IRUSR, wdog_disable_get,
							wdog_disable_set);

static void keep_alive_response(void *info)
{
	int cpu = smp_processor_id();
	struct sp805_wdt *wdt = (struct sp805_wdt *)info;

	spin_lock(&wdt->lock);
	cpumask_set_cpu(cpu, &wdt->alive_mask);
	spin_unlock(&wdt->lock);
}

/*
 * If this function does not return, it implies one of the
 * other cpu's is not responsive.
 */
static void ping_other_cpus(struct sp805_wdt *wdt)
{
	unsigned long flags;

	spin_lock_irqsave(&wdt->lock, flags);
	cpumask_clear(&wdt->alive_mask);
	cpumask_set_cpu(0, &wdt->alive_mask);
	spin_unlock_irqrestore(&wdt->lock, flags);

	smp_call_function(keep_alive_response, wdt, 1);
}

static void pet_watchdog_work(struct work_struct *work)
{
	unsigned long delay_time;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct sp805_wdt *wdt = container_of(delayed_work,
						struct sp805_wdt,
							dogwork_struct);

	delay_time = msecs_to_jiffies(wdt->pet_time);
	if (enable) {
		if (wdt->do_ipi_ping)
			ping_other_cpus(wdt);

		last_pet_time = sched_clock();
		wdt_setload(&wdt->wdd, wdt->timeout);
		wdt_config(&wdt->wdd, true);
	}
	/* Check again before scheduling *
	 * Could have been changed on other cpu */
	if (enable)
		queue_delayed_work_on(0, watchdog_wq,
				&wdt->dogwork_struct, delay_time);

	cpumask_clear_cpu(0, &wdt->alive_mask);
}

void force_reset_by_wdt(void)
{
	unsigned long flags;

	spin_lock_irqsave(&wdt_inter_lock, flags);
	__raw_writel(UNLOCK, odin_wdt_base + WDTLOCK);
	__raw_writel(0x2, odin_wdt_base + WDTLOAD);
	spin_unlock_irqrestore(&wdt_inter_lock, flags);
}

void odin_touch_softlockup_watchdog(void)
{
	unsigned long flags;

	spin_lock_irqsave(&wdt_inter_lock, flags);
	__raw_writel(UNLOCK, odin_wdt_base + WDTLOCK);
	__raw_writel(nmi_load*2, odin_wdt_base + WDTLOAD);
	__raw_writel(LOCK, odin_wdt_base + WDTLOCK);
	spin_unlock_irqrestore(&wdt_inter_lock, flags);
}
EXPORT_SYMBOL(odin_touch_softlockup_watchdog);

void odin_wdt_external_enable(void)
{
	wdt_setload(&odin_wdt_ext->wdd, odin_wdt_ext->timeout);
	wdt_config(&odin_wdt_ext->wdd, false);
}

static irqreturn_t wdog_bark_handler(int irq, void *dev_id)
{
	if(reset_flag == 0) {
		struct sp805_wdt *wdt = (struct sp805_wdt *)dev_id;
		unsigned long nanosec_rem;
		unsigned long long t = sched_clock();

		nanosec_rem = do_div(t, 1000000000);
		nanosec_rem = do_div(last_pet_time, 1000000000);

		if (wdt->do_ipi_ping)
			dump_cpu_alive_mask(wdt);

		if(wdt->bark_or_bite == BARK) {
			printk(KERN_INFO "Watchdog bark! Now = %lu.%06lu\n", (unsigned long) t,
				nanosec_rem / 1000);

			printk(KERN_INFO "last pet time : %lu.%06lu\n", (unsigned long) last_pet_time,
				nanosec_rem / 1000);

			if(bark_panic) {
				odin_show_task_state(wdt->print_lock, wdt->print_t_status);
				reset_flag = 1;
				is_watchdog_crashed=1;
				BUG();
			} else {
				bark_cnt++;
				wdt_disable(&wdt->wdd);
				wdt_setload(&wdt->wdd, wdt->timeout);
				wdt_enable(&wdt->wdd);
				queue_delayed_work_on(0, watchdog_wq, &wdt->dogwork_struct, 0);
			}
		} else {
			printk(KERN_INFO "Watchdog bite! Now = %lu.%06lu\n", (unsigned long) t,
				nanosec_rem / 1000);

			printk(KERN_INFO "last pet time : %lu.%06lu\n", (unsigned long) last_pet_time,
				nanosec_rem / 1000);

			printk(KERN_INFO "Causing a watchdog bite! / bark_cnt:%d\n", bark_cnt);

			odin_show_task_state(wdt->print_lock, wdt->print_t_status);

			reset_flag = 1;
			is_watchdog_crashed=1;
#ifdef CONFIG_PSTORE_FTRACE
			printk(KERN_INFO "Kernel panic - not syncing: Watchdog timeout\n");
			odin_push_panic();
			emergency_restart();
#else
			panic("Watchdog timeout by bite\n");
#endif
		}
	}

	return IRQ_HANDLED;
}

static void init_watchdog_work(struct work_struct *work)
{
	struct sp805_wdt *wdt = container_of(work,
						struct sp805_wdt,
							init_dogwork_struct);
	unsigned long delay_time;
	int error;

	delay_time = msecs_to_jiffies(wdt->pet_time);

	wdt_setload(&wdt->wdd, wdt->timeout);

	wdt->panic_blk.notifier_call = panic_wdog_handler;
	atomic_notifier_chain_register(&panic_notifier_list,
				       &wdt->panic_blk);
	mutex_init(&wdt->disable_lock);
	queue_delayed_work_on(0, watchdog_wq, &wdt->dogwork_struct,
			delay_time);

	wdt_enable(&wdt->wdd);

	error = device_create_file(&wdt->adev->dev, &dev_attr_disable);

	if (error)
		dev_err(&wdt->adev->dev, "cannot create sysfs attribute\n");

	dev_info(&wdt->adev->dev, "ODIN Watchdog work Initialized\n");
	return;
}

static void dump_pdata(struct amba_device *adev, struct sp805_wdt *pdata)
{
	dev_dbg(&adev->dev, "wdog timeout %d", pdata->timeout);
	dev_dbg(&adev->dev, "wdog pet_time %d", pdata->pet_time);
	dev_dbg(&adev->dev, "wdog perform ipi ping %d", pdata->do_ipi_ping);
	dev_dbg(&adev->dev, "wdog base address is 0x%x\n", (unsigned int)
								pdata->base);
}

/*wdt module function start*/
static ssize_t show_wdt(struct kobject *kobj, struct attribute *attr,
        char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "\n");
}

static ssize_t store_wdt(struct kobject *kobj,
	struct attribute *attr, const char *buf, size_t len)
{
    struct wdt_attr *a = container_of(attr, struct wdt_attr, attr);
    struct sp805_wdt *wdt = to_wdt_s(kobj);
	int num = -1;

	sscanf(buf, "%d", &num);

	if (a->value==0) {
		wdt->pet_time = num;

		printk(KERN_INFO "set_pettime to: %dms\n", wdt->pet_time);
	} else if (a->value == 1) {
		wdt_setload(&wdt->wdd, num);
	} else if (a->value == 2) {
		if(num == 0) {
			cancel_delayed_work(&wdt->dogwork_struct);
			wdt_disable(&wdt->wdd);
			enable = 0;
		} else {
			wdt_setload(&wdt->wdd, wdt->timeout);
			wdt_enable(&wdt->wdd);
			queue_delayed_work_on(0, watchdog_wq, &wdt->dogwork_struct, 0);
			enable = 1;
		}

		printk(KERN_INFO "wdt is %s\n", num==1?"enabled":"disabled");
	} else if (a->value == 3) {
		if(num == 1)
			bark_panic = true;
		else
			bark_panic = false;

		printk(KERN_INFO "sp805 wdt bark panic %s\n", num==1?"on":"off");
	} else {
		force_reset_by_wdt();
	}

	return len;
}

static struct sysfs_ops wdt_module_ops = {
    .show = show_wdt,
    .store = store_wdt,
};

static struct kobj_type wdt_type = {
	.sysfs_ops = &wdt_module_ops,
	.default_attrs = wdtattr,
};

/*wdt module function end*/

static int
sp805_wdt_probe(struct amba_device *adev, const struct amba_id *id)
{
	struct sp805_wdt *wdt;
	struct device_node *node = adev->dev.of_node;
	int ret = 0;

	watchdog_wq = alloc_workqueue("watchdog", WQ_HIGHPRI, 0);

	if (!watchdog_wq) {
		dev_warn(&adev->dev, "Failed to allocate watchdog workqueue\n");
		ret = -EIO;
		goto err;
	}

	if (!devm_request_mem_region(&adev->dev, adev->res.start,
				resource_size(&adev->res), "sp805_wdt")) {
		dev_warn(&adev->dev, "Failed to get memory region resource\n");
		ret = -ENOENT;
		goto err;
	}

	wdt = devm_kzalloc(&adev->dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt) {
		dev_warn(&adev->dev, "Kzalloc failed\n");
		ret = -ENOMEM;
		goto err;
	}

	wdt->base = devm_ioremap(&adev->dev, adev->res.start,
			resource_size(&adev->res));

	odin_wdt_base = ioremap(SP805_BASE, SZ_1K);

	if (!wdt->base) {
		ret = -ENOMEM;
		dev_warn(&adev->dev, "ioremap fail\n");
		goto err;
	}

	wdt->clk = clk_get(&adev->dev, NULL);
	if (IS_ERR(wdt->clk)) {
		dev_warn(&adev->dev, "Clock not found\n");
		ret = PTR_ERR(wdt->clk);
		goto err;
	}
	wdt->clk_rate = clk_get_rate(wdt->clk);

	ret = clk_prepare_enable(wdt->clk);
	if (ret) {
		dev_err(&wdt->adev->dev, "clock enable fail");
		return ret;
	}

	wdt->adev = adev;
	wdt->wdd.info = &wdt_info;
	wdt->wdd.ops = &wdt_ops;

	spin_lock_init(&wdt->lock);
	watchdog_set_nowayout(&wdt->wdd, nowayout);
	watchdog_set_drvdata(&wdt->wdd, wdt);
	wdt_setload(&wdt->wdd, DEFAULT_TIMEOUT);

	wdt->bite_irq = adev->irq[0];

	ret = devm_request_irq(&wdt->adev->dev, wdt->bite_irq, wdog_bark_handler,
				IRQF_TRIGGER_NONE | IRQF_NO_SUSPEND,
				"sp805_wdt_bark", wdt);
	if (ret) {
		dev_err(&wdt->adev->dev, "failed to request bark irq second\n");
		return ret;
	}

	ret = of_property_read_u32(node, "odin,timeout", &wdt->timeout);
	if (ret) {
		dev_err(&adev->dev, "reading timeout failed\n");
		return -ENXIO;
	}

	ret = of_property_read_u32(node, "odin,pet-time", &wdt->pet_time);
	if (ret) {
		dev_err(&adev->dev, "reading pet time failed\n");
		return -ENXIO;
	}

	wdt->do_ipi_ping = of_property_read_bool(node, "odin,ipi-ping");

	bark_panic = of_property_read_bool(node, "odin,bark-panic");

	if(bark_panic) {
		printk(KERN_INFO "sp805_wdt bark painc is on\n");
	}

	dump_pdata(adev, wdt);

	printk(KERN_INFO "timeout:%d pet_time:%d\n",wdt->timeout, wdt->pet_time);

	cpumask_clear(&wdt->alive_mask);
	INIT_WORK(&wdt->init_dogwork_struct, init_watchdog_work);
	INIT_DELAYED_WORK(&wdt->dogwork_struct, pet_watchdog_work);
	queue_work_on(0, watchdog_wq, &wdt->init_dogwork_struct);

	ret = watchdog_register_device(&wdt->wdd);
	if (ret) {
		dev_err(&adev->dev, "watchdog_register_device() failed: %d\n",
				ret);
		return -ENXIO;
	}

	device_move(&adev->dev, NULL, DPM_ORDER_DEV_LAST);

	amba_set_drvdata(adev, wdt);

/*wdt module init*/
	kobject_init(&wdt->kobj, &wdt_type);
	if (kobject_add(&wdt->kobj, NULL, "%s", "wdt_module")) {
		ret = -1;
		printk(KERN_WARNING "Sysfs creation failed\n");
		kobject_put(&wdt->kobj);
	}

	dev_info(&adev->dev, "registration successful\n");

	odin_wdt_ext = wdt;

	return 0;

err:
	destroy_workqueue(watchdog_wq);
	dev_err(&adev->dev, "Probe Failed!!!\n");
	return ret;
}

static int sp805_wdt_remove(struct amba_device *adev)
{
	struct sp805_wdt *wdt = amba_get_drvdata(adev);
	struct wdog_disable_work_data work_data;

	mutex_lock(&wdt->disable_lock);
	if (enable) {
		work_data.wdt = wdt;
		init_completion(&work_data.complete);
		INIT_WORK_ONSTACK(&work_data.work, wdog_disable_work);
		queue_work_on(0, watchdog_wq, &work_data.work);
		wait_for_completion(&work_data.complete);
	}
	mutex_unlock(&wdt->disable_lock);
	device_remove_file(wdt->dev, &dev_attr_disable);

	watchdog_unregister_device(&wdt->wdd);
	amba_set_drvdata(adev, NULL);
	watchdog_set_drvdata(&wdt->wdd, NULL);
	clk_put(wdt->clk);

	printk(KERN_INFO "ODIN Watchdog Exit - Deactivated\n");
	kfree(wdt);
	return 0;
}

static int sp805_wdt_suspend(struct device *dev)
{
	struct sp805_wdt *wdt = dev_get_drvdata(dev);

	cancel_delayed_work(&wdt->dogwork_struct);

	return 0;
}

static int sp805_wdt_resume(struct device *dev)
{
	struct sp805_wdt *wdt = dev_get_drvdata(dev);

	queue_delayed_work_on(0, watchdog_wq, &wdt->dogwork_struct, 0);

	return 0;
}

static int sp805_wdt_suspend_noirq(struct device *dev)
{
	struct sp805_wdt *wdt = dev_get_drvdata(dev);

	/* wdt_disable(&wdt->wdd); */
	/* pr_info("%s: watchdog disabled.\n", __func__); */

	return 0;
}

#include <linux/delay.h>

static int sp805_wdt_resume_noirq(struct device *dev)
{
	struct sp805_wdt *wdt = dev_get_drvdata(dev);

	wdt_setload(&wdt->wdd, wdt->timeout);
	wdt_enable(&wdt->wdd);
	enable = 1;
	pr_info("%s: watchdog enabled.\n", __func__);

	return 0;
}

static void sp805_wdt_shutdown(struct device *dev)
{
	struct sp805_wdt *wdt = dev_get_drvdata(dev);

	cancel_delayed_work(&wdt->dogwork_struct);
	wdt_disable(&wdt->wdd);

	printk("%s \n", __func__);

	return ;
}

static const struct dev_pm_ops sp805_wdt_dev_pm_ops = {
       .suspend_late           = sp805_wdt_suspend,
       .resume_early           = sp805_wdt_resume,
       .suspend_noirq          = sp805_wdt_suspend_noirq,
       .resume_noirq           = sp805_wdt_resume_noirq,
};

static struct amba_id sp805_wdt_ids[] = {
	{
		.id	= 0x00bb824,
		.mask	= 0x00ffffff,
	},
	{ 0, 0 },
};

MODULE_DEVICE_TABLE(amba, sp805_wdt_ids);

static struct amba_driver sp805_wdt_driver = {
	.drv = {
		.name	= MODULE_NAME,
		.pm	= &sp805_wdt_dev_pm_ops,
		.shutdown = sp805_wdt_shutdown,
	},
	.id_table	= sp805_wdt_ids,
	.probe		= sp805_wdt_probe,
	.remove 	= sp805_wdt_remove,
};

module_amba_driver(sp805_wdt_driver);
