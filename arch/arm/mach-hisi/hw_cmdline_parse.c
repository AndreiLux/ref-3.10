#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/smp.h>
#include <linux/memblock.h>
#include <linux/irqchip.h>

#include <asm/hardware/arm_timer.h>
#include <asm/arch_timer.h>
#include <asm/hardware/timer-sp.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/smp_plat.h>
#include <asm/smp_twd.h>
#include <asm/proc-fns.h>
#include <asm/sched_clock.h>
#include <asm/hw_cmdline_parse.h>

#include <linux/hw_log.h>
#define HWLOG_TAG	cmdline_parse
HWLOG_REGIST();


static unsigned int runmode_factory = RUNMODE_FLAG_NORMAL;

static int __init early_parse_runmode_cmdline(char * p)
{
    if(p)
    {
        if(!strcmp(p,"factory"))
        {
            runmode_factory = RUNMODE_FLAG_FACTORY;
        }

        printk("runmode is %s, runmode_factory = %d\n", p, runmode_factory);
    }

    return 0;
}

early_param("androidboot.swtype", early_parse_runmode_cmdline);

/* the function interface to check factory/normal mode in kernel */
unsigned int runmode_is_factory(void)
{
    return runmode_factory;
}

EXPORT_SYMBOL(runmode_is_factory);


static int logctl_flag = 0;	//default 0, switch logger off
int get_logctl_value(void)
{
    return logctl_flag;
}
EXPORT_SYMBOL(get_logctl_value);

int enter_recovery_flag;
/**
* parse boot_into_recovery cmdline which is passed from boot_recovery() of boot.c *
* Format : boot_into_recovery_flag=0 or 1             *
*/
static int __init early_parse_enterrecovery_cmdline(char * p)
{
    char enter_recovery[HEX_STRING_MAX + 1];
    char *endptr = NULL;

    memset(enter_recovery, 0, HEX_STRING_MAX + 1);

    memcpy(enter_recovery, p, HEX_STRING_MAX);
    enter_recovery[HEX_STRING_MAX] = '\0';

    enter_recovery_flag = simple_strtoull(enter_recovery, &endptr, TRANSFER_BASE);

    printk("enter recovery p:%s, enter_recovery_flag :%d\n", p, enter_recovery_flag);

    return 0;
}
early_param("enter_recovery", early_parse_enterrecovery_cmdline);

unsigned int get_boot_into_recovery_flag(void)
{
    return enter_recovery_flag;
}
EXPORT_SYMBOL(get_boot_into_recovery_flag);

unsigned int recovery_update_flag;
/**
* Format : recovery_update=0 or 1 *
*/
static int __init early_parse_recovery_update_cmdline(char * p)
{
    char recovery_update[HEX_STRING_MAX + 1];
    char *endptr = NULL;

    memset(recovery_update, 0, HEX_STRING_MAX + 1);

    memcpy(recovery_update, p, HEX_STRING_MAX);
    recovery_update[HEX_STRING_MAX] = '\0';

    recovery_update_flag = simple_strtoull(recovery_update, &endptr, TRANSFER_BASE);

    printk("recovery_update p:%s, recovery_update_flag :%d\n", p, recovery_update_flag);

    return 0;
}
early_param("recovery_update", early_parse_recovery_update_cmdline);

unsigned int get_recovery_update_flag(void)
{
    return recovery_update_flag;
}
EXPORT_SYMBOL(get_recovery_update_flag);

static int __init early_parse_logctl_cmdline(char * p)
{
    char logctl[HEX_STRING_MAX + 1];
    char *endptr = NULL;

    if (p == NULL) {
        logctl_flag = 0;	//error parameter, switch logger off
        hwlog_err("%s: setup_logctl parameter error!\n", __func__);
        return 0;
    }

    memset(logctl, 0, HEX_STRING_MAX + 1);

    memcpy(logctl, p, HEX_STRING_MAX);

    logctl_flag = simple_strtoull(logctl, &endptr, TRANSFER_BASE);

    hwlog_info("%s: p: %s, logctl: %d\n", __func__, p, logctl_flag);

    return 0;
}
early_param("setup_logctl", early_parse_logctl_cmdline);


/*******************
*0: battery is normal;
*1: battery is low;
*********************/
static int g_lowbattery = 0;

int get_lowbatteryflag(void)
{
    return g_lowbattery;
}
EXPORT_SYMBOL(get_lowbatteryflag);

void set_lowBatteryflag(int flag)
{
	g_lowbattery = flag;
}
EXPORT_SYMBOL(set_lowBatteryflag);

static int __init early_parse_lowpower_cmdline(char *p)
{
	char clow[HEX_STRING_MAX + 1];
    char *endptr = NULL;

    if (p == NULL) {
        g_lowbattery = 0;	//error parameter, switch logger off
        pr_err("%s input is NULL\n", __func__);
        return 0;
    }

    memset(clow, 0, HEX_STRING_MAX + 1);
    memcpy(clow, p, HEX_STRING_MAX);

    g_lowbattery = simple_strtoull(clow, &endptr, TRANSFER_BASE);

	pr_info("%s %d\n", __func__, g_lowbattery);
	return 0;
}
early_param("low_volt_flag", early_parse_lowpower_cmdline);