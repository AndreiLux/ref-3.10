/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/kernel.h>
#include <linux/cpu_pm.h>
#include <linux/odin_mailbox.h>
#include <linux/delay.h>
#include <linux/odin_wdt.h>

#include <asm/proc-fns.h>
#include <asm/smp_scu.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#include <asm/mcpm.h>

#include <linux/platform_data/gpio-odin.h>

#include "pm.h"

#if defined(CONFIG_LGE_CRASH_HANDLER)
#include <linux/mdm_ctrl.h>
extern unsigned char get_watchdog_crash_status();
extern unsigned char get_mailbox_status();
#endif


#define CONFIG_ODIN_IPC
/*
               
                              
                                    
 */
#ifdef CONFIG_LGE_CRASH_HANDLER
static unsigned int restart_reason = 0x776655AA; /* Normal reboot todo */

void lge_set_reboot_reason(unsigned int reason)
{
	if (reason == HIDDEN_REBOOT)
		restart_reason = reason;
	else
	{
		if (get_watchdog_crash_status())
			restart_reason = WATCH_DOG_REBOOT;
		else if (get_modem_crash_status())
			restart_reason = MODEM_CRASH_REBOOT;
		else if (get_mailbox_status())
			restart_reason = MAILBOX_TIMEOUT_REBOOT;
		else
			restart_reason = reason;
	}
}
#endif
/*              */

static inline void odin_set_pwr_suspend(void)
{
	printk("odin_set_pwr_suspend\n");
}

static inline void odin_set_pwr_resume(void)
{
	printk("odin_set_pwr_resume\n");
}

int odin_send_data(u32 system_cmd, u32 state_cmd)
{
	int ret;
	int ipc_data[7] = {0,};

	ipc_data[0] = PM_CMD;
	ipc_data[1] = system_cmd;
	ipc_data[2] = state_cmd;

	ret = arch_ipc_call_fast(ipc_data);
	if (ret < 0)
		printk("Failed to IPC err (%d) \n", ret);

	mdelay(2000); /* Waiting for being set by PMIC */

	return ret;
}

void odin_set_pwr_shutdown(void)
{
	int ret = -1;

	printk("ODIN_SHUTDOWN\n");

	ret = odin_send_data(SYSTEM_POWER_OFF, 0);
	if (ret < 0)
		printk("Fail to send data (%d) \n", ret);
}

void odin_restart(char mode, const char *cmd)
{
	int ret = -1;
	u32 state_cmd = 0x0;
	u32 system_cmd = 0x0;
	extern int odin_check_panic(void);

	/* Disable interrupts */
	local_irq_disable();
	local_fiq_disable();

        //comment code below because of kernel panic
	//printk("%s : restart cmd = %s(%d)\n", __func__, cmd, strlen(cmd));

	if (cmd && strlen(cmd) != 0) {
		if (!strncmp(cmd,"recovery",8)) {
			system_cmd = SYSTEM_RESET;
			state_cmd = RECOVERY_REBOOT;
		} else if (!strncmp(cmd,"download",8)) {
			system_cmd = SYSTEM_RESET;
			state_cmd = DOWNLOAD_REBOOT;
		} else if (!strncmp(cmd,"bootloader",10)) {
			system_cmd = SYSTEM_RESET;
			state_cmd = FASTBOOT_REBOOT;
		} else if (!strncmp(cmd,"crash",5)) {
			system_cmd = SYSTEM_RESET;
			state_cmd = CRASH_REBOOT;
		} else if (!strncmp(cmd,"laf_cmd",7) || !strncmp(cmd,"oem-90466252",12)) {
			system_cmd = SYSTEM_RESET;
			state_cmd = LAF_CMD_REBOOT;
		} else if (!strncmp(cmd,"normal_cmd",10)) {
			system_cmd = SYSTEM_RESET;
			state_cmd = NORMAL_CMD_REBOOT;
		} else if (!strncmp(cmd,"--bnr_recovery",14)) {
			system_cmd = SYSTEM_RESET;
			state_cmd = BNR_CMD_REBOOT;
		} else if (!strncmp(cmd,"charge_reset",12)) {
			system_cmd = SYSTEM_RESET;
			state_cmd = SOFTWARE_REBOOT;
		} else {
			state_cmd = UNKNOWN_REBOOT;
		}
	}
	#ifdef CONFIG_LGE_CRASH_HANDLER
	else if (odin_check_panic()) {
		system_cmd = SYSTEM_RESET;
		if (restart_reason == HIDDEN_REBOOT)
			state_cmd = HIDDEN_REBOOT;
		else
		{
			if (get_modem_crash_status())
				state_cmd = MODEM_CRASH_REBOOT;
			else if (get_watchdog_crash_status())
				state_cmd = WATCH_DOG_REBOOT;
			else if (get_mailbox_status())
				state_cmd = MAILBOX_TIMEOUT_REBOOT;
			else
				state_cmd = CRASH_REBOOT;
		}

		printk(KERN_EMERG "%s : restart reason = %08lx\n",__func__,  restart_reason);
	}
	#endif
	else
	{
		dump_stack();
		state_cmd = SOFTWARE_REBOOT;
	}

	printk("ODIN_RESTART & CMD = 0x%x\n", state_cmd);

	ret = odin_send_data(SYSTEM_RESET, state_cmd);
	if (ret < 0)
		printk("Fail to send data (%d) \n", ret);
}

static int odin_suspend_finish(unsigned long val)
{
#ifdef CONFIG_ODIN_IPC
	unsigned int mpidr = read_cpuid_mpidr();
	unsigned int cluster = (mpidr >> 8) & 0xf;
	unsigned int cpu = mpidr & 0xf;

	pr_debug("%s: cpu %u cluster %u, residency: %u\n",
			__func__, cpu, cluster, (unsigned int)val);
	mcpm_set_entry_vector(cpu, cluster, cpu_resume);
	mcpm_cpu_suspend(0);

	/* we should never get past here */
	panic("Failed to enter suspend!!!\n");

#else
	cpu_do_idle();

#endif

	return 0;
}

static int odin_gpio_suspend(void)
{
	odin_gpio_regs_suspend(ODIN_GPIO_PERI_START, \
				ODIN_GPIO_PERI_END, ODIN_GPIO_NR);
#if 0
	odin_gpio_regs_suspend(ODIN_GPIO_ICS_START, \
				ODIN_GPIO_ICS_END, ODIN_GPIO_NR);
#endif
#ifndef CONFIG_AUD_RPM
	odin_gpio_regs_suspend(ODIN_GPIO_AUD_START, \
				ODIN_GPIO_AUD_END, ODIN_GPIO_NR);
#endif

	return 0;
}

static int odin_gpio_resume(void)
{
	odin_gpio_regs_resume(ODIN_GPIO_PERI_START, \
				ODIN_GPIO_PERI_END, ODIN_GPIO_NR);
#if 0
	odin_gpio_regs_resume(ODIN_GPIO_ICS_START, \
				ODIN_GPIO_ICS_END, ODIN_GPIO_NR);
#endif
#ifndef CONFIG_AUD_RPM
	odin_gpio_regs_resume(ODIN_GPIO_AUD_START, \
				ODIN_GPIO_AUD_END, ODIN_GPIO_NR);
#endif

	return 0;
}

static int odin_pm_enter(suspend_state_t state)
{
	int ret;

	odin_set_pwr_suspend();

	odin_gpio_suspend();

	ret = cpu_suspend(0, odin_suspend_finish);
	if (ret)
		BUG();

	mcpm_cpu_powered_up();

	odin_gpio_resume();

	odin_set_pwr_resume();

	odin_wdt_external_enable();
	return 0;
}

static int odin_restore_cpu_affinity(suspend_state_t state)
{
	cpumask_var_t new_value;

	cpumask_clear(new_value);
	cpumask_set_cpu(0, new_value);
	cpumask_set_cpu(1, new_value);
	cpumask_set_cpu(2, new_value);
	cpumask_set_cpu(3, new_value);
	irq_set_affinity(ipc_irq, new_value);

	return 0;
}

static const struct platform_suspend_ops odin_pm_ops = {
	.enter = odin_pm_enter,
	.valid = suspend_valid_only_mem,
	.wake = odin_restore_cpu_affinity,
};

static int __init odin_pm_init(void)
{
	suspend_set_ops(&odin_pm_ops);

	return 0;
}
module_init(odin_pm_init);
