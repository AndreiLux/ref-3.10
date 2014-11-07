/*
 * Copyright 2013 LG Electronics.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/syscore_ops.h>
#ifdef CONFIG_CPU_IDLE_ODIN_WORKAROUND
#include <linux/odin_cpuidle.h>
#endif

#include <asm/cpu.h>
#include <asm/cputype.h>
#include <asm/cpuidle.h>
#include <asm/mcpm.h>
#include <asm/smp_plat.h>
#include <asm/suspend.h>

#define ODIN_CPUIDLE_MAX_OFF_TIME_US    0xffffffff

static int odin_enter_powerdown(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx);

static struct cpuidle_driver odin_idle_little_driver = {
	.name = "odin_little_idle",
	.owner = THIS_MODULE,
	.states[0] = ARM_CPUIDLE_WFI_STATE,
	.states[1] = {
		.enter			= odin_enter_powerdown,
		.exit_latency		= 1000,
		.target_residency	= 3000,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "C1",
		.desc			= "Odin little-cluster power down",
	},
	.state_count = 2,
};

static struct cpuidle_driver odin_idle_big_driver = {
	.name = "odin_big_idle",
	.owner = THIS_MODULE,
	.states[0] = ARM_CPUIDLE_WFI_STATE,
	.states[1] = {
		.enter			= odin_enter_powerdown,
		.exit_latency		= 1000,
		.target_residency	= 3000,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "C1",
		.desc			= "Odin big-cluster power down",
	},
	.state_count = 2,
};

static void odin_cpuidle_shutdown(void)
{
	cpuidle_pause();
}

static struct syscore_ops odin_cpuidle_syscore_ops = {
	.shutdown	= odin_cpuidle_shutdown,
};

static int notrace odin_powerdown_finisher(unsigned long arg)
{
	unsigned int mpidr = read_cpuid_mpidr();
	unsigned int cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	unsigned int cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);

	mcpm_set_entry_vector(cpu, cluster, cpu_resume);
	mcpm_cpu_suspend(ODIN_CPUIDLE_MAX_OFF_TIME_US);
	return 1;
}

/*
 * odin_enter_powerdown - Programs CPU to enter the specified state
 * @dev: cpuidle device
 * @drv: The target state to be programmed
 * @idx: state index
 *
 * Called from the CPUidle framework to program the device to the
 * specified target state selected by the governor.
 */
static int odin_enter_powerdown(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx)
{
	cpu_pm_enter();

	cpu_suspend(0, odin_powerdown_finisher);

	mcpm_cpu_powered_up();

	cpu_pm_exit();

	return idx;
}

#ifdef CONFIG_CPU_IDLE_ODIN_WORKAROUND
void odin_idle_disable(int cpu_id, bool disable)
{
	struct cpuidle_device *dev;

	dev = per_cpu(cpuidle_devices, cpu_id);

#ifdef CONFIG_PSTORE_FTRACE
	if (disable) {
		dev->states_usage[0].disable++;
		dev->states_usage[1].disable++;
	} else {
		if (dev->states_usage[0].disable)
			dev->states_usage[0].disable--;
		if (dev->states_usage[1].disable)
			dev->states_usage[1].disable--;
	}
	printk("lowpower states of cpuidle for %d is %s\n", cpu_id,
			disable ? "disabled" : "enabled");
#else
	if (disable)
		dev->states_usage[1].disable++;
	else
		if (dev->states_usage[1].disable)
			dev->states_usage[1].disable--;
#endif
}
#endif

static int __init odin_idle_driver_init(struct cpuidle_driver *drv, int cpu_id)
{
	struct cpuinfo_arm *cpu_info;
	struct cpumask *cpumask;
	unsigned long cpuid;
	int cpu;

	cpumask = kzalloc(cpumask_size(), GFP_KERNEL);
	if (!cpumask)
		return -ENOMEM;

	for_each_possible_cpu(cpu) {
		cpu_info = &per_cpu(cpu_data, cpu);
		cpuid = is_smp() ? cpu_info->cpuid : read_cpuid_id();

		/* read cpu id part number */
		if ((cpuid & 0xFFF0) == cpu_id) {
			pr_info("CPUidle for CPU%d registered\n", cpu);
			cpumask_set_cpu(cpu, cpumask);
		}
	}

	drv->cpumask = cpumask;

	return 0;
}

/*
 * odin_idle_init
 *
 * Registers the odin specific cpuidle driver with the cpuidle
 * framework with the valid set of states.
 */
int __init odin_idle_init(void)
{
	int ret;

	if (!of_find_compatible_node(NULL, NULL, "lge,odin")) {
		pr_info("%s: No compatible node found\n", __func__);
		return -ENODEV;
	}

	register_syscore_ops(&odin_cpuidle_syscore_ops);

	pr_info("%s: little-cluster driver initialize\n", __func__);

	ret = odin_idle_driver_init(&odin_idle_little_driver,
			ARM_CPU_PART_CORTEX_A7);
	if (ret)
		goto out_uninit_little;

	pr_info("%s: big-cluster driver initialize\n", __func__);

	ret = odin_idle_driver_init(&odin_idle_big_driver,
			ARM_CPU_PART_CORTEX_A15);
	if (ret)
		goto out_uninit_big;

	ret = cpuidle_register(&odin_idle_little_driver, NULL);
	if (ret)
		goto out_unregister_little;

	ret = cpuidle_register(&odin_idle_big_driver, NULL);
	if (ret)
		goto out_unregister_big;

	return 0;

out_unregister_little:
	cpuidle_unregister(&odin_idle_little_driver);
out_unregister_big:
	cpuidle_unregister(&odin_idle_big_driver);
out_uninit_big:
	kfree(odin_idle_big_driver.cpumask);
out_uninit_little:
	kfree(odin_idle_little_driver.cpumask);

	return ret;
}
device_initcall(odin_idle_init);
