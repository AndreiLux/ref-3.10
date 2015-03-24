#include <linux/init.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/of_fdt.h>
#include <asm/localtimer.h>
#include <mach/mt_reg_base.h>
#include <mach/mtk_boot_share_page.h>
#include <mach/smp.h>
#include <mach/sync_write.h>
#include <mach/hotplug.h>
#ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
#include <mach/mt_spm_mtcmos.h>
#endif
#include <asm/fiq_glue.h>
#include <asm/smp_plat.h>
#include <mach/wd_api.h>
#include <mach/mt_spm.h>
#include <mach/pmic_mt6397_sw.h>
#ifdef CONFIG_MTK_SCHED_TRACERS
#include <trace/events/mtk_events.h>
#include "kernel/trace/trace.h"
#endif
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#include <asm/hardware/gic.h>
#endif
#include <mach/mt_pm_ldo.h>
#include <drivers/misc/mediatek/power/mt8135/pmic.h>

#define SLAVE1_MAGIC_NUM 0x534C4131
#define SLAVE2_MAGIC_NUM 0x4C415332
#define SLAVE3_MAGIC_NUM 0x41534C33

/* Normal world register location. */
#define SLAVE1_MAGIC_REG 0xF0002038
#define SLAVE2_MAGIC_REG 0xF000203C
#define SLAVE3_MAGIC_REG 0xF0002040
#define SLAVE_JUMP_REG  0xF0002034

/* Secure world location, using SRAM now. */
#define NS_SLAVE_JUMP_REG  (BOOT_SHARE_BASE+1020)
#define NS_SLAVE_MAGIC_REG (BOOT_SHARE_BASE+1016)
#define NS_SLAVE_BOOT_ADDR (BOOT_SHARE_BASE+1012)


#ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
static unsigned int is_secondary_cpu_first_boot;
#endif
static DEFINE_SPINLOCK(boot_lock);

static const unsigned int secure_magic_reg[] = {
	SLAVE1_MAGIC_REG, SLAVE2_MAGIC_REG, SLAVE3_MAGIC_REG };
static const unsigned int secure_magic_num[] = {
	SLAVE1_MAGIC_NUM, SLAVE2_MAGIC_NUM, SLAVE3_MAGIC_NUM };
typedef int (*spm_mtcmos_ctrl_func) (int state);
static const spm_mtcmos_ctrl_func secure_ctrl_func[] = {
	spm_mtcmos_ctrl_cpu1, spm_mtcmos_ctrl_cpu2, spm_mtcmos_ctrl_cpu3 };


static void __cpuinit write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

static void __cpuinit mt_platform_secondary_init(unsigned int cpu)
{
	struct wd_api *wd_api = NULL;
	HOTPLUG_INFO("platform_secondary_init, cpu: %d\n", cpu);

	get_wd_api(&wd_api);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
	gic_secondary_init(0);
#endif

	write_pen_release(-1);


#ifdef CONFIG_MTK_WD_KICKER
	wd_api->wd_cpu_hot_plug_on_notify(cpu);
#endif

#ifdef CONFIG_FIQ_GLUE
	fiq_glue_resume();
#endif

#ifdef CONFIG_MTK_SCHED_TRACERS
	trace_cpu_hotplug(cpu, 1, per_cpu(last_event_ts, cpu));
	per_cpu(last_event_ts, cpu) = ns2usecs(ftrace_now(cpu));
#endif

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

static void __cpuinit mt_wakeup_cpu(int cpu)
{
	mt65xx_reg_sync_writel(secure_magic_num[cpu - 1], secure_magic_reg[cpu - 1]);
	*((unsigned int *)NS_SLAVE_MAGIC_REG) = secure_magic_num[cpu - 1];
	HOTPLUG_INFO("SLAVE%d_MAGIC_NUM:%x\n", cpu, secure_magic_num[cpu - 1]);
#ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
	if (is_secondary_cpu_first_boot) {
		--is_secondary_cpu_first_boot;
	} else {
		pmic_smp_lock();
		mt65xx_reg_sync_writel(virt_to_phys(mt_secondary_startup), BOOT_ADDR);
		*((unsigned int *)NS_SLAVE_BOOT_ADDR) = virt_to_phys(mt_secondary_startup);
		(*secure_ctrl_func[cpu - 1]) (STA_POWER_ON);
		pmic_smp_unlock();
	}
#endif
}

static int __cpuinit mt_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;

	atomic_inc(&hotplug_cpu_count);

	/*
	 * Set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	HOTPLUG_INFO("boot_secondary, cpu: %d\n", cpu);
	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 * Note that "pen_release" is the hardware CPU ID, whereas
	 * "cpu" is Linux's internal ID.
	 */
	write_pen_release(cpu_logical_map(cpu));

	switch (cpu) {
	case 1:
	case 2:
	case 3:
		mt_wakeup_cpu(cpu);
		break;
	default:
		break;

	}

	arch_send_wakeup_ipi_mask(cpumask_of(cpu));

	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (pen_release == -1)
			break;

		udelay(10);
	}
	/*
	 * Now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	if (pen_release == -1) {
		return 0;
	} else {
		/* to fix monitor register */
		pr_emerg("failed to boot.\n");
		mt65xx_reg_sync_writel(0x1, 0xF020021c);
		pr_emerg("CPU2, debug event: 0x%08x, debug monitor: 0x%08x\n",
			 *(volatile u32 *)(0xF020021c), *(volatile u32 *)(0xF0200014));
		mt65xx_reg_sync_writel(0x3, 0xF020021c);
		pr_emerg("CPU3, debug event: 0x%08x, debug monitor: 0x%08x\n",
			 *(volatile u32 *)(0xF020021c), *(volatile u32 *)(0xF0200014));
		on_each_cpu((smp_call_func_t) dump_stack, NULL, 0);

		return -ENOSYS;
	}

}

static int __init mt_dt_cpus_num(unsigned long node, const char *uname, int depth, void *data)
{
	static int prev_depth = -1;
	static int nr_cpus = -1;

	if (prev_depth > depth && nr_cpus > 0)
		return nr_cpus;

	if (nr_cpus < 0 && strcmp(uname, "cpus") == 0)
		nr_cpus = 0;

	if (nr_cpus >= 0) {
		const char *device_type = of_get_flat_dt_prop(node, "device_type", NULL);

		if (device_type && strcmp(device_type, "cpu") == 0)
			nr_cpus++;
	}

	prev_depth = depth;

	return 0;
}

static void __init mt_smp_init_cpus(void)
{
	unsigned int i, ncores;

	ncores = of_scan_flat_dt(mt_dt_cpus_num, NULL);

	if (ncores > NR_CPUS) {
		pr_warn("Get core count (%d) > NR_CPUS (%d)\n", ncores, NR_CPUS);
		pr_warn("set nr_cores to NR_CPUS (%d)\n", NR_CPUS);
		ncores = NR_CPUS;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);

#ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
	is_secondary_cpu_first_boot = num_possible_cpus() - 1;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
	set_smp_cross_call(gic_raise_softirq);
#endif
}

static void __init mt_platform_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;

	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);


	/* write the address of slave startup into the system-wide flags register */
	mt65xx_reg_sync_writel(virt_to_phys(mt_secondary_startup), SLAVE_JUMP_REG);
	*((unsigned int *)NS_SLAVE_JUMP_REG) = virt_to_phys(mt_secondary_startup);
}

struct smp_operations mt65xx_smp_ops __initdata = {
	.smp_init_cpus = mt_smp_init_cpus,
	.smp_prepare_cpus = mt_platform_smp_prepare_cpus,
	.smp_secondary_init = mt_platform_secondary_init,
	.smp_boot_secondary = mt_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_kill = mt_cpu_kill,
	.cpu_die = mt_cpu_die,
	.cpu_disable = mt_cpu_disable,
#endif
};
