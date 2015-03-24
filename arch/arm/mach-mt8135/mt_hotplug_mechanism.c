/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*********************************
* include
**********************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/earlysuspend.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <mach/hotplug.h>
#include <mach/dynamic_boost.h>



/*********************************
* macro
**********************************/
#ifdef CONFIG_HAS_EARLYSUSPEND

#define STATE_INIT                  0
#define STATE_ENTER_EARLY_SUSPEND   1
#define STATE_ENTER_LATE_RESUME     2

#define SAMPLE_MS_EARLY_SUSPEND "sample_ms=3000"
#define SAMPLE_MS_LATE_RESUME   "sample_ms=115"

#endif				/* #ifdef CONFIG_HAS_EARLYSUSPEND */


/*********************************
* glabal variable
**********************************/
#ifdef CONFIG_HAS_EARLYSUSPEND
static int g_enable;
static struct early_suspend mt_hotplug_mechanism_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 250,
	.suspend = NULL,
	.resume = NULL,
};

static int g_cur_state = STATE_ENTER_LATE_RESUME;
static struct work_struct hotplug_sample_ms_adj_work;
#endif /* #ifdef CONFIG_HAS_EARLYSUSPEND */

static int g_limited_ca7_ncpu = 2;
static int g_limited_ca15_ncpu = 2;



/*********************************
* extern function
**********************************/


void mt_send_hotplug_cmd(const char *cmd)
{
	static const char *fn_hotplug_cmd = "/dev/hotplug/cmd";
	struct file *fp = NULL;
	mm_segment_t old_fs;

	fp = filp_open(fn_hotplug_cmd, O_WRONLY, 0);
	if (IS_ERR(fp)) {
		HOTPLUG_INFO("open %s failed\n", fn_hotplug_cmd);
		return;
	}

	old_fs = get_fs();
	set_fs(get_ds());

	fp->f_op->write(fp, cmd, strlen(cmd), &fp->f_pos);

	set_fs(old_fs);

	filp_close(fp, NULL);
}
EXPORT_SYMBOL(mt_send_hotplug_cmd);


#ifdef CONFIG_HAS_EARLYSUSPEND


static void mt_hotplug_mechanism_sample_ms_adjust(struct work_struct *data)
{
	if (STATE_ENTER_EARLY_SUSPEND == g_cur_state)
		mt_send_hotplug_cmd(SAMPLE_MS_EARLY_SUSPEND);
	else
		mt_send_hotplug_cmd(SAMPLE_MS_LATE_RESUME);

	HOTPLUG_INFO("sample freq is changed %s\n",
		(STATE_ENTER_EARLY_SUSPEND == g_cur_state) ?
			SAMPLE_MS_EARLY_SUSPEND : SAMPLE_MS_LATE_RESUME);
}



/*********************************
* early suspend callback function
**********************************/
static void __ref mt_hotplug_mechanism_early_suspend(struct early_suspend *h)
{
	HOTPLUG_INFO("mt_hotplug_mechanism_early_suspend");
	set_dynamic_boost(-1, PRIO_RESET);
	if (g_enable) {
		int i = 0;

		for (i = (num_possible_cpus() - 1); i > 0; i--) {
			if (cpu_online(i))
				cpu_down(i);
		}
	}
	g_cur_state = STATE_ENTER_EARLY_SUSPEND;
	schedule_work(&hotplug_sample_ms_adj_work);
	return;
}



/*******************************
* late resume callback function
********************************/
static void __ref mt_hotplug_mechanism_late_resume(struct early_suspend *h)
{
	HOTPLUG_INFO("mt_hotplug_mechanism_late_resume");

	if (g_enable) {
		/* temp solution for turn on 4 cores */
		int i = 1;

		for (i = 1; i < num_possible_cpus(); i++) {
			if (!cpu_online(i))
				cpu_up(i);
		}
	}
	set_dynamic_boost(-1, PRIO_TWO_LITTLES);
	g_cur_state = STATE_ENTER_LATE_RESUME;
	schedule_work(&hotplug_sample_ms_adj_work);
	return;
}


#endif /* #ifdef CONFIG_HAS_EARLYSUSPEND */


/*******************************
* kernel module init function
********************************/
static int __init mt_hotplug_mechanism_init(void)
{
	HOTPLUG_INFO("mt_hotplug_mechanism_init");

#ifdef CONFIG_HAS_EARLYSUSPEND
	mt_hotplug_mechanism_early_suspend_handler.suspend = mt_hotplug_mechanism_early_suspend;
	mt_hotplug_mechanism_early_suspend_handler.resume = mt_hotplug_mechanism_late_resume;
	register_early_suspend(&mt_hotplug_mechanism_early_suspend_handler);
	INIT_WORK(&hotplug_sample_ms_adj_work, mt_hotplug_mechanism_sample_ms_adjust);
#endif /* #ifdef CONFIG_HAS_EARLYSUSPEND */

	return 0;
}
module_init(mt_hotplug_mechanism_init);



/*******************************
* kernel module exit function
********************************/
static void __exit mt_hotplug_mechanism_exit(void)
{
	HOTPLUG_INFO("mt_hotplug_mechanism_exit");
}
module_exit(mt_hotplug_mechanism_exit);



/**************************************************************
* mt hotplug mechanism control interface for thermal protect
***************************************************************/
void mt_hotplug_mechanism_thermal_protect(int limited_ca7_cpus, int limited_ca15_cpus)
{
	HOTPLUG_INFO("mt_hotplug_mechanism_thermal_protect\n");
	g_limited_ca7_ncpu = limited_ca7_cpus;
	g_limited_ca15_ncpu = limited_ca15_cpus;
}
EXPORT_SYMBOL(mt_hotplug_mechanism_thermal_protect);



#ifdef CONFIG_HAS_EARLYSUSPEND
module_param(g_enable, int, 0644);
module_param(g_limited_ca7_ncpu, int, 0644);
module_param(g_limited_ca15_ncpu, int, 0644);
#endif /* #ifdef CONFIG_HAS_EARLYSUSPEND */



MODULE_DESCRIPTION("MediaTek CPU Hotplug Mechanism");
MODULE_LICENSE("GPL");
