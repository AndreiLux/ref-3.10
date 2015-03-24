/*
 * tmp103_cpu_cooling_device driver file
 *
 * Copyright (C) 2013 Lab126, Inc.  All rights reserved.
 * Author: Akwasi Boateng <boatenga@lab126.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/lp855x.h>

#include <linux/thermal_framework.h>
#include "mach/mt_thermal.h"
#include <mach/charging.h>
#include <mach/battery_common.h>
#include <mach/tmp103_cooler.h>

#include "thermal_core.h"

#define DRIVER_NAME "tmp103-cooling"
#define MAX_STATE 1

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>

#ifndef THERMO_METRICS_STR_LEN
#define THERMO_METRICS_STR_LEN 128
#endif
#endif

struct led_classdev *ldev;

struct cooler_range {
	int min, max;
};

struct cooler_calc {
	struct cooler_range range;
	enum tmp103_cooler_type ctype;
	int value;
};

static struct cooler_range cooler_range[] = {
	[TMP103_COOLER_CPU] = {MIN_CPU_POWER, MAX_CPU_POWER},
	[TMP103_COOLER_BL]  = {MIN_BRIGHTNESS, MAX_BRIGHTNESS},
	[TMP103_COOLER_BC]  = {MIN_CHARGING_LIMIT, MAX_CHARGING_LIMIT},
};

struct tmp103_platform_cooler_ctx {
	struct list_head list;
};

struct tmp103_cooler_ctx {
	enum tmp103_cooler_type ctype;
	struct list_head entry;
	struct thermal_cooling_device *cdev;
	unsigned int state;
	int action;
	int clear;
#ifdef CONFIG_AMAZON_METRICS_LOG
	ktime_t last_time;
	int bucket;
#endif
};

static struct thermal_cooling_device_ops cooling_ops;

void lp855x_led_cooler_register(struct led_classdev *led_cdev)
{
	ldev = led_cdev;
}
EXPORT_SYMBOL(lp855x_led_cooler_register);

static int cooler_calc_value(struct device *dev, void *data)
{
	struct cooler_calc *calc = data;
	struct thermal_cooling_device *cdev;
	struct tmp103_cooler_ctx *cctx;
	int val;

	/* Get device structure of current cooling device */
	cdev = container_of(dev, struct thermal_cooling_device, device);

	/* Get context structure of cooling device */
	cctx = cdev->devdata;

	/* Check for current driver type */
	if (cdev->ops != &cooling_ops || cctx->ctype != calc->ctype)
		return 0;

	val = (cctx->state) ? min(calc->value, cctx->action) : max(calc->value, cctx->clear);
	calc->value = clamp(val, calc->range.min, calc->range.max);

	return 0;
}

static int cooler_update(struct class *class,
			 struct tmp103_cooler_ctx *cctx,
			 unsigned long state)
{
	struct cooler_calc calc;
	int err;

#ifdef CONFIG_AMAZON_METRICS_LOG
	unsigned long prev_state = cctx->state;
	char *metrics_prefix = "def:monitor=1";
	char mBuf[THERMO_METRICS_STR_LEN];
	char vBuf[THERMO_METRICS_STR_LEN];
	long timer_value = 0;
	struct thermal_instance *instance;
#endif

	calc.value = INT_MAX;			/* Set startup value */
	calc.ctype = cctx->ctype;		/* Choice calc for that ctype */
	calc.range = cooler_range[cctx->ctype];	/* choice valid value range */

	switch (cctx->ctype) {
	case TMP103_COOLER_CPU:
		calc.value = thermal_budget_get();
		calc.value = calc.value ? : MAX_CPU_POWER;
		break;
	case TMP103_COOLER_BL:
		calc.value = lp855x_led_get_brightness(ldev);
		/* Get current levels before 0 -> 1 */
		if (state && !cctx->state)
			cctx->clear = calc.value;
		break;
	case TMP103_COOLER_BC:
		calc.value = MAX_CHARGING_LIMIT;
		break;
	default:
		pr_err("%s: Invalid %s type %d\n", __func__,
		       cctx->cdev->type, cctx->ctype);
		return -EINVAL;
	}

	/* Update state */
	cctx->state = state;

	err = class_for_each_device(class, NULL, &calc, cooler_calc_value);
	if (err)
		return err;

	switch (cctx->ctype) {
	case TMP103_COOLER_CPU:
#ifdef CONFIG_AMAZON_METRICS_LOG
		list_for_each_entry(instance, &cctx->cdev->thermal_instances, cdev_node) {
			timer_value = ((long)ktime_to_ms(ktime_get_boottime()))/MSEC_PER_SEC;
			if (!prev_state && cctx->state) {
				cctx->last_time = ktime_get();
				snprintf(mBuf, THERMO_METRICS_STR_LEN,
					 "%s:%s;throttling_start=1:CT;1;NR",
					 cctx->cdev->type,
					 metrics_prefix);
				log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", mBuf);
				snprintf(vBuf,
					 THERMO_METRICS_STR_LEN,
					 "zone%d",
					 cctx->bucket);
				log_timer_to_vitals(ANDROID_LOG_INFO,
						    "thermal engine",
						    "thermal",
						    "time_in_thermal_bucket",
						    vBuf,
						    timer_value,
						    "s",
						    VITALS_TIME_BUCKET);
			} else if (prev_state && !cctx->state) {
				ktime_t delta = ktime_sub(ktime_get(), cctx->last_time);
				snprintf(mBuf, THERMO_METRICS_STR_LEN,
					 "%s:%s;throttling_stop=1;for_ms=%lld:TI;1;NR",
					 cctx->cdev->type,
					 metrics_prefix,
					 ktime_to_ms(delta));
				log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", mBuf);
				snprintf(vBuf,
					 THERMO_METRICS_STR_LEN,
					 "zone%d",
					 cctx->bucket);
				log_timer_to_vitals(ANDROID_LOG_INFO,
						    "thermal engine",
						    "thermal",
						    "time_in_thermal_bucket",
						    vBuf,
						    timer_value,
						    "s",
						    VITALS_TIME_BUCKET);
			}
		}
#endif
		if (calc.value != thermal_budget_get())
			thermal_budget_notify(calc.value, "tmp103-cpu-cooler");
		break;

	case TMP103_COOLER_BL:
		if (!ldev)
			break;
#ifdef CONFIG_AMAZON_METRICS_LOG
		if (!prev_state && cctx->state) {
			cctx->last_time = ktime_get();
			snprintf(mBuf, THERMO_METRICS_STR_LEN,
				 "%s:%s;throttling_start=1;CT;1:NR",
				 cctx->cdev->type,
				 metrics_prefix);
			log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", mBuf);
		} else if (prev_state && !cctx->state) {
			ktime_t delta = ktime_sub(ktime_get(), cctx->last_time);
			snprintf(mBuf, THERMO_METRICS_STR_LEN,
				 "%s:%s;throttling_stop=1;for_ms=%lld;TI;1:NR",
				 cctx->cdev->type,
				 metrics_prefix,
				 ktime_to_ms(delta));
			log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", mBuf);
		}
#endif
		if (calc.value != lp855x_led_get_brightness(ldev))
			lp855x_led_set_brightness(ldev, calc.value);
		break;

	case TMP103_COOLER_BC:
#ifdef CONFIG_AMAZON_METRICS_LOG
		if (!prev_state && cctx->state) {
			cctx->last_time = ktime_get();
			snprintf(mBuf, THERMO_METRICS_STR_LEN,
				 "%s:%s;throttling_start=1;CT;1:NR",
				 cctx->cdev->type,
				 metrics_prefix);
			log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", mBuf);
		} else if (prev_state && !cctx->state) {
			ktime_t delta = ktime_sub(ktime_get(), cctx->last_time);
			snprintf(mBuf, THERMO_METRICS_STR_LEN,
				 "%s:%s;throttling_stop=1;for_ms=%lld;TI;1:NR",
				 cctx->cdev->type,
				 metrics_prefix,
				 ktime_to_ms(delta));
			log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", mBuf);
		}
#endif
		set_bat_charging_current_limit(calc.value);
		break;

	default:
		pr_err("%s: Invalid %s type %d\n", __func__,
			cctx->cdev->type, cctx->ctype);
		return -EINVAL;
	}

	return 0;
}

static int _get_max_state_(struct thermal_cooling_device *cdev,
			   unsigned long *state)
{
	*state = MAX_STATE;

	return 0;
}

static int _get_cur_state_(struct thermal_cooling_device *cdev,
			   unsigned long *state)
{
	struct tmp103_cooler_ctx *cctx = cdev->devdata;

	if (cctx)
		*state = cctx->state;

	return 0;
}

static int _set_cur_state_(struct thermal_cooling_device *cdev,
			   unsigned long state)
{
	struct tmp103_cooler_ctx *cctx = cdev->devdata;

	if (!cctx) {
		pr_err("%s: NULL %s device data\n", __func__, cdev->type);
		return -EINVAL;
	}

	if (!state && !cctx->state) {
		cctx->state = state;
		return 0;
	}

	cooler_update(cdev->device.class, cctx, state);

	return 0;
}

static struct thermal_cooling_device_ops cooling_ops = {
	.get_max_state = _get_max_state_,
	.get_cur_state = _get_cur_state_,
	.set_cur_state = _set_cur_state_,
};

static int cooler_show(struct device *dev,
		       struct device_attribute *devattr, char *buf)
{
	struct tmp103_platform_cooler_ctx *ctx = dev_get_drvdata(dev);
	struct tmp103_cooler_ctx *cctx;
	int len = 0;

	list_for_each_entry(cctx, &ctx->list, entry) {
		len += sprintf(buf + len, "%s action=%d clear=%d state=%d\n",
			       cctx->cdev->type, cctx->action, cctx->clear, cctx->state);
	}

	return len;
}

static ssize_t cooler_set(struct device *dev,
			  struct device_attribute *devattr,
			  const char *buf,
			  size_t count)
{
	struct tmp103_platform_cooler_ctx *ctx = dev_get_drvdata(dev);
	struct tmp103_cooler_ctx *cctx;
	char cdev[THERMAL_NAME_LENGTH];
	int action, clear;

	if (sscanf(buf, "%19s %d %d", cdev, &action, &clear) != 3)
		return -EINVAL;

	list_for_each_entry(cctx, &ctx->list, entry) {
		if (strcmp(cdev, cctx->cdev->type))
			continue;

		cctx->action = action;
		cctx->clear = clear;

		return count;
	}

	return -EINVAL;
}

static DEVICE_ATTR(cooler, S_IWUSR | S_IRUGO, cooler_show, cooler_set);
static struct attribute *cooler_attributes[] = {
	&dev_attr_cooler.attr,
	NULL
};

static const struct attribute_group cooler_attr_group = {
	.attrs = cooler_attributes,
};

static int cooler_probe(struct platform_device *pdev)
{
	struct tmp103_cooler_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct tmp103_cooler_pdev *pctx = pdata->list;
	struct tmp103_cooler_ctx *cctx;
	struct tmp103_platform_cooler_ctx *ctx;
	int i, err;

	ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -EINVAL;

	INIT_LIST_HEAD(&ctx->list);

	for (i = 0; i < pdata->count; i++, pctx++) {
		cctx = kzalloc(sizeof(cctx), GFP_KERNEL);
		if (!cctx)
			continue;

		INIT_LIST_HEAD(&cctx->entry);
		cctx->ctype = pctx->ctype;
		cctx->action = pctx->action;
		cctx->clear = pctx->clear;
		cctx->state = 0;
		cctx->bucket = i;

		cctx->cdev = thermal_cooling_device_register(pctx->name,
			cctx, &cooling_ops);
		if (!cctx->cdev) {
			dev_err(&pdev->dev, "%s: Error %s registeration\n",
				__func__, pctx->name);
			kfree(cctx);
			continue;
		}

		list_add_tail(&cctx->entry, &ctx->list);
	}

	err = sysfs_create_group(&pdev->dev.kobj, &cooler_attr_group);
	if (err)
		dev_err(&pdev->dev, "Can't create sysfs group");

	dev_set_drvdata(&pdev->dev, ctx);

	return 0;
}

static int cooler_remove(struct platform_device *pdev)
{
	struct tmp103_platform_cooler_ctx *ctx = dev_get_drvdata(&pdev->dev);
	struct tmp103_cooler_ctx *cctx, *cctx_tmp;

	dev_set_drvdata(&pdev->dev, NULL);

	sysfs_remove_group(&pdev->dev.kobj, &cooler_attr_group);

	list_for_each_entry_safe(cctx, cctx_tmp, &ctx->list, entry) {
		if (cctx->cdev)
			thermal_cooling_device_unregister(cctx->cdev);
		list_del_init(&cctx->entry);
		kfree(cctx);
	}

	kfree(ctx);

	return 0;
}

static struct platform_driver cooler_driver = {
	.probe    = cooler_probe,
	.remove   = cooler_remove,
	.shutdown = NULL,
	.suspend  = NULL,
	.resume   = NULL,
	.driver     = {
		.name  = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init cooler_init(void)
{
	int err;

	err = platform_driver_register(&cooler_driver);
	if (err) {
		pr_err("%s: Failed to register driver %s\n", __func__,
			DRIVER_NAME);
		return err;
	}
	return err;
}
static void __exit cooler_exit(void)
{
	platform_driver_unregister(&cooler_driver);
}

module_init(cooler_init);
module_exit(cooler_exit);

MODULE_DESCRIPTION("TMP103 pcb cpu cooling device driver");
MODULE_AUTHOR("Akwasi Boateng <boatenga@lab126.com>");
MODULE_LICENSE("GPL");
