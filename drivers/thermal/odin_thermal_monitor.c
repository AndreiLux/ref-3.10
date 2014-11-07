/* arch/arm/mach-odin/odin_thermal_monitor.c
 *
 * ODIN thermal monitor
 *
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/odin_thermal.h>
#include <linux/odin_thermal_monitor.h>
#include <linux/board_lge.h>
/* FIXME: rename and move arm_big_little.h to mach-odin/include/mach */
#include "../cpufreq/arm_big_little.h"

#define THERMAL_MODEL_LIGER

#define RCPU_LITTLE	0
#define RCPU_BIG	4
#define MAX_AVAIL_FREQS		16
#define EMERG_UP_THRESHOLD	1024
#define EMERG_DOWN_THRESHOLD 1023

struct odin_tmon_drvdata {
	u32 tsens_id;
	u32 sampling_period;
	u32 threshold_hi;
	u32 threshold_lo;
	int freq_step;
};

#ifdef CONFIG_MACH_ODIN_LIGER
static bool no_skip_first_disable = true;
#endif
static struct odin_tmon_drvdata *tmondev;

static struct cpufreq_frequency_table *cpufreq_tbl;
static int max_freq_idx;	/* index of available max cpufreq */
static int target_freq_idx;	/* index of throttled max cpufreq */

static struct delayed_work monitor_work;
static bool disable;
static bool thermal_emergency;

/* CA15 scaling_available_frequencies
  * 684000, 804000, 900000, 996000, 1092000, 1188000, 1308000, 1404000, 1500000
  */
#ifdef THERMAL_MODEL_LIGER
static unsigned int hmp_up_threshold[MAX_AVAIL_FREQS] = {990, 975, 960, 945, 930, 915, 900, 885, 870};
static unsigned int hmp_down_threshold[MAX_AVAIL_FREQS] = {750, 705, 690, 675, 660, 645, 630, 615, 600};
#endif

extern int set_hmp_thermal(unsigned int value);
extern int set_hmp_thermal_up_threshold(unsigned int value);
extern int set_hmp_thermal_down_threshold(unsigned int value);

static int throttle_cpufreq(int cpu, int idx);
static int boot_mode=0 ;
static int batt_status=0 ;

/* Currently, we don't support restarting thermal monitor */
static int set_disable(const char *val, const struct kernel_param *kp)
{
	struct cpufreq_policy policy;
	int rc;

	if (!val || !kp || !kp->name)
		return -EINVAL;

	rc = param_set_bool(val, kp);
	if (rc < 0) {
		pr_err("TMON: can't set %s to %s\n", kp->name, val);
		return rc;
	}

#ifdef CONFIG_MACH_ODIN_LIGER
	if (disable && no_skip_first_disable) {
#else
	if (disable) {
#endif
		if (!tmondev)
			return 0;

		cancel_delayed_work(&monitor_work);
		flush_scheduled_work();

	if((batt_status ==BATT_NOT_PRESENT) && (boot_mode >= LGE_BOOT_MODE_FACTORY &&  boot_mode <= LGE_BOOT_MODE_PIFBOOT3 )){
			printk("boot mode is factory, so skip thermal set_disable func \n");
			return 0;
		}

        if ((cpufreq_get_policy(&policy, RCPU_BIG) == 0) &&
            policy.governor && policy.governor->initialized > 0) {
            (void)throttle_cpufreq(RCPU_BIG, max_freq_idx);
            pr_info("TMON: stop thermal monitor and restore cpu%d max freq to %u\n",
                RCPU_BIG, cpufreq_tbl[max_freq_idx].frequency);
        }

        thermal_emergency = 0;

		if (tmondev) {
			kfree(tmondev);
			tmondev = NULL;
		}
	} else {
#ifdef CONFIG_MACH_ODIN_LIGER
		no_skip_first_disable = true;
#endif
		pr_warn("TMON: can't restart thermal monitor\n");
	}

	return 0;
}

//cpuinit
static int get_disable(char *buffer, const struct kernel_param *kp)
{
	int rc;

	if (!buffer || !kp || !kp->name)
		return -EINVAL;

	rc = param_get_bool(buffer, kp);
	if (rc < 0) {
		pr_err("TMON: can't get %s\n", kp->name);
		return rc;
	}

	return rc;
}

static void handle_thermal_emergency(unsigned long temp)
{
	(void)set_hmp_thermal_up_threshold(EMERG_UP_THRESHOLD);
	(void)set_hmp_thermal_down_threshold(EMERG_DOWN_THRESHOLD);
	pr_info("TMON: disable forking on big cores (TS%u=%lumC)\n",tmondev->tsens_id, temp);
}

static struct kernel_param_ops module_ops = {
	.set = set_disable,
	.get = get_disable,
};

module_param_cb(disable, &module_ops, &disable, 0644);
MODULE_PARM_DESC(disable, "Switch for stopping thermal monitor");

static int init_cpufreq_info(void)
{
	int i;

	cpufreq_tbl = cpufreq_frequency_get_table(RCPU_BIG);
	if (!cpufreq_tbl) {
		pr_debug("TMON: cpufreq isn't initialized yet\n");
		return -ENODEV;
	}

	i = 0;
	while (cpufreq_tbl[i].frequency != CPUFREQ_TABLE_END)
		i++;

	max_freq_idx = i - 1;
	if (max_freq_idx < 0) {
		pr_err("TMON: cpufreq table is empty\n");
		cpufreq_tbl = NULL;
		return -EINVAL;
	}

	target_freq_idx = max_freq_idx;
	return 0;
}

static int throttle_cpufreq(int cpu, int idx)
{
    static int hmp_thermal = 0;
	u32 max_freq;
	int rc;

	if (idx < 0 || idx > max_freq_idx)
		return -EINVAL;

	max_freq = cpufreq_tbl[idx].frequency;
	rc = bL_cpufreq_set_limit(cpu, max_freq);
	if (rc)
		return rc;

	rc = cpufreq_update_policy(cpu);
	if (rc)
		return rc;

	(void)set_hmp_thermal_up_threshold(hmp_up_threshold[idx]);
	(void)set_hmp_thermal_down_threshold(hmp_down_threshold[idx]);

    if (idx == max_freq_idx) {
        (void)set_hmp_thermal(0);
        hmp_thermal = 0;
    } else {
        if (hmp_thermal == 0) {
            (void)set_hmp_thermal(1);
            hmp_thermal = 1;
        }
    }
    return 0;
}

//cpuinit
static void thermal_monitor(struct work_struct *work)
{
	struct cpufreq_policy policy;
	unsigned long temp;
	int prev_target_freq_idx;
	int rc;
	boot_mode = lge_get_boot_mode();
	batt_status = lge_get_batt_id() ;

	if((batt_status ==BATT_NOT_PRESENT) && (boot_mode >= LGE_BOOT_MODE_FACTORY &&  boot_mode <= LGE_BOOT_MODE_PIFBOOT3 )){
		printk("boot mode is factory, so skip thermal monitor func \n");
		return 0;
	}

	/*
	 * Thermal monitor starts before initializing cpufreq driver,
	 * so we should retry until cpufreq driver is ready.
	 */
	if (!cpufreq_tbl) {
		if (init_cpufreq_info() < 0)
			goto reschedule;
	}

	if (cpufreq_get_policy(&policy, RCPU_BIG) < 0)
		goto reschedule;

	if (!policy.governor || policy.governor->initialized == 0)
		goto reschedule;

	if (odin_tsens_get_temp(tmondev->tsens_id, &temp) < 0)
		goto reschedule;

	pr_debug("TMON: TS%u=%lumC\n", tmondev->tsens_id, temp);

	prev_target_freq_idx = target_freq_idx;

	if (temp >= tmondev->threshold_hi) {
		if (target_freq_idx == 0) {
			if (thermal_emergency == 0) {
				thermal_emergency = 1;
				handle_thermal_emergency(temp);
			}
			goto reschedule;
		}
		target_freq_idx -= tmondev->freq_step;
		if (target_freq_idx < 0)
			target_freq_idx = 0;
	} else if (temp <= tmondev->threshold_lo) {
	    thermal_emergency = 0;
		if (target_freq_idx == max_freq_idx)
			goto reschedule;
		target_freq_idx += tmondev->freq_step;
		if (target_freq_idx > max_freq_idx)
			target_freq_idx = max_freq_idx;
	} else
		goto reschedule;

	if (target_freq_idx != max_freq_idx)
		pr_info("TMON: throttle cpu%d max freq to %u (TS%u=%lumC)\n",
			RCPU_BIG, cpufreq_tbl[target_freq_idx].frequency,
			tmondev->tsens_id, temp);
	else
		pr_info("TMON: restore cpu%d max freq to %u (TS%u=%lumC)\n",
			RCPU_BIG, cpufreq_tbl[max_freq_idx].frequency,
			tmondev->tsens_id, temp);

#ifdef CONFIG_MACH_ODIN_LIGER
extern unsigned long lge_get_ca15_maxfreq(void);
	if(lge_get_ca15_maxfreq() < 1500000)
	{
		no_skip_first_disable = false;
		rc = throttle_cpufreq(RCPU_BIG, 0);
	}
	else
#endif
	{
		no_skip_first_disable = true;
		rc = throttle_cpufreq(RCPU_BIG, target_freq_idx);
	}

	if (rc) {
		pr_err("TMON: can't throttle cpu%d max freq to %d, rc(%d)\n", RCPU_BIG,
			cpufreq_tbl[target_freq_idx].frequency, rc);
		target_freq_idx = prev_target_freq_idx;
	}

reschedule:
#ifdef CONFIG_MACH_ODIN_LIGER
		if(lge_get_ca15_maxfreq() < 1500000)
		{
			no_skip_first_disable = false;
		}
		else
		{
			no_skip_first_disable = true;
		}
#endif
	if (!disable) {
		schedule_delayed_work(&monitor_work,
			msecs_to_jiffies(tmondev->sampling_period));
	}
}

static int odin_tmon_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	char *prop;
	int rc;

	if (tmondev) {
		dev_err(&pdev->dev, "TMON: device is busy\n");
		return -EBUSY;
	}

	if (!np) {
		dev_err(&pdev->dev, "TMON: can't find device node\n");
		return -ENODEV;
	}

	tmondev = kzalloc(sizeof(struct odin_tmon_drvdata), GFP_KERNEL);
	if (!tmondev) {
		dev_err(&pdev->dev, "TMON: can't alloc drvdata\n");
		return -ENOMEM;
	}

	prop = "tsens-id";
	rc = of_property_read_u32(np, prop, &tmondev->tsens_id);
	if (rc)
		goto err_read_prop;

	prop = "sampling-period";
	rc = of_property_read_u32(np, prop, &tmondev->sampling_period);
	if (rc)
		goto err_read_prop;

	prop = "threshold-hi";
	rc = of_property_read_u32(np, prop, &tmondev->threshold_hi);
	if (rc)
		goto err_read_prop;

	prop = "threshold-lo";
	rc = of_property_read_u32(np, prop, &tmondev->threshold_lo);
	if (rc)
		goto err_read_prop;

	prop = "freq-step";
	rc = of_property_read_u32(np, prop, (u32 *)(&tmondev->freq_step));
	if (rc)
		goto err_read_prop;

	pr_info("TMON: id=%u, poll=%u, hi=%u, lo=%u, step=%d\n",
		tmondev->tsens_id, tmondev->sampling_period,
		tmondev->threshold_hi, tmondev->threshold_lo,
		tmondev->freq_step);

	platform_set_drvdata(pdev, tmondev);

	thermal_emergency = 0;

	INIT_DELAYED_WORK(&monitor_work, thermal_monitor);
	schedule_delayed_work(&monitor_work, 0);

	return 0;

err_read_prop:

	kfree(tmondev);
	tmondev = NULL;
	dev_err(&pdev->dev, "TMON: can't read prop %s, rc(%d)\n", prop, rc);
	return rc;
}

static int odin_tmon_remove(struct platform_device *pdev)
{
	if (tmondev) {
		kfree(tmondev);
		tmondev = NULL;
	}

	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id odin_tmon_of_match[] = {
	{ .compatible = "lge,odin-thermal-monitor" },
	{ /* end of table */ },
};

static struct platform_driver odin_tmon_driver = {
	.probe = odin_tmon_probe,
	.remove = odin_tmon_remove,
	.driver = {
		.name = "odin-thermal-monitor",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(odin_tmon_of_match),
	},
};

int __init odin_tmon_driver_init(void)
{
	return platform_driver_register(&odin_tmon_driver);
}

int __init odin_tmon_driver_late_init(void)
{
	return 0;
}
late_initcall(odin_tmon_driver_late_init);

MODULE_DESCRIPTION("ODIN Thermal Monitor");
MODULE_AUTHOR("Changwon Lee <changwon.lee@lge.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:odin-thermal-monitor");
