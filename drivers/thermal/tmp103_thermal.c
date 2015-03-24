/*
 * tmp103_thermal_zone driver file
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

#include <linux/input/tmp103_temp_sensor.h>
#include <linux/thermal_framework.h>
#include <mach/mt_board.h>

#define DRIVER_NAME "tmp103-thermal"
#define THERMAL_NAME "tmp103"
#define TMP103_TEMP_PROC_FILE  "vs_read_temp"
#define TMP103_CONF_PROC_FILE  "vs_config"
#define TMP103_TZ_PROC_FILE    "thermal_zone"
#define BUF_SIZE 512
#define TMP103_MAX_TEMP         60000
#define NUM_SENSORS 3
#define DMF 1000
#define DECIMAL_BASE 10

static LIST_HEAD(thermal_sensor_list);
static DEFINE_MUTEX(thermal_lock);

struct trip_t {
	char name[THERMAL_NAME_LENGTH];
	unsigned long temp;
	enum thermal_trip_type type;
};
struct tmp103_thermal_zone {
	struct thermal_zone_device *tdev;
	int num_trips;
	int delay;
	struct trip_t trip[THERMAL_MAX_TRIPS];
	enum thermal_device_mode mode;
	struct work_struct twork;
};
struct vs_config_t {
	unsigned long temp;
	int offset;
	int alpha;
	int weight;
};
static struct vs_config_t vs_data[NUM_SENSORS] = {
	{0, 0.0*DMF, 1.0*DMF, 0.333*DMF},
	{0, 0.0*DMF, 1.0*DMF, 0.333*DMF},
	{0, 0.0*DMF, 1.0*DMF, 0.333*DMF},
};
static struct vs_config_t ariel_vs_data[NUM_SENSORS] = {
	{0, 8.100*DMF, 0.700*DMF, 0.333*DMF},
	{0, 12.300*DMF, 0.200*DMF, 0.333*DMF},
	{0, 10.060*DMF, 0.700*DMF, 0.334*DMF},
};
static struct vs_config_t aston_vs_data[NUM_SENSORS] = {
	{0, 3.400*DMF, 0.700*DMF, 0.333*DMF},
	{0, 8.250*DMF, 0.200*DMF, 0.333*DMF},
	{0, 5.800*DMF, 0.700*DMF, 0.334*DMF},
};
static int sk[NUM_SENSORS] = {0, 0, 0};

static char *vs_conf_names[] = {
	"offsets",
	"alphas",
	"weights",
};
static char *coolers[] = {
	"tmp103-cpu-cool-0",
	"tmp103-cpu-cool-1",
	"tmp103-cpu-cool-2",
	"tmp103-cpu-cool-3",
	"tmp103-cpu-cool-4",
	"tmp103-cpu-cool-5",
	"tmp103-bl-cool-0",
	"tmp103-bc-cool-0",
	"tmp103-bc-cool-1",
	"tmp103-bc-cool-2",
};
static int tmp103_cdev_bind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int ret = 0;
	int i = 0;
	struct tmp103_thermal_zone *tzone = thermal->devdata;
	if (tzone == NULL)
		return -EINVAL;
	for (i = 0; i < thermal->trips; i++) {
		if (!strcmp(cdev->type, tzone->trip[i].name)) {
			ret =
				thermal_zone_bind_cooling_device(thermal,
								 i,
								 cdev,
								 THERMAL_NO_LIMIT,
								 THERMAL_NO_LIMIT);
			if (ret < 0) {
				pr_err("%s: Failed to bind cdev %s\n",
				       __func__,
				       tzone->trip[i].name);
				return -EINVAL;
			}
		}
	}
	return ret;
}
static int tmp103_cdev_unbind(struct thermal_zone_device *thermal,
			      struct thermal_cooling_device *cdev)
{
	int ret = 0;
	int i = 0;
	struct tmp103_thermal_zone *tzone = thermal->devdata;

	if (tzone == NULL)
		return -EINVAL;
	for (i = 0; i < thermal->trips; i++) {
		if (!strcmp(cdev->type, tzone->trip[i].name)) {
			ret = thermal_zone_unbind_cooling_device(thermal,
								 i,
								 cdev);
			if (ret < 0) {
				pr_err("%s: Failed to unbind cdev %s\n",
				       __func__,
				       tzone->trip[i].name);
				return -EINVAL;
			}
		}
	}
	return ret;
}
static int _vs_get_temp_(unsigned long *t)
{
	int i = 0;
	int tsenv = 0;
	struct thermal_dev *tdev;

	list_for_each_entry(tdev, &thermal_sensor_list, node) {
		vs_data[i].temp = tdev->dev_ops->report_temp(tdev);
		if (sk[i] == 0)
			sk[i] = vs_data[i].temp - vs_data[i].offset;
		else {
			sk[i] =  vs_data[i].alpha * (vs_data[i].temp - vs_data[i].offset) +
				(DMF - vs_data[i].alpha) * sk[i];
			sk[i] /= DMF;
		}
		i++;
	}
	for (i = 0; i < NUM_SENSORS; i++)
		tsenv += (vs_data[i].weight * sk[i]);
	*t = tsenv/DMF;
	return 0;
}
static int tmp103_thermal_get_temp(struct thermal_zone_device *thermal,
				   unsigned long *t)
{
	return _vs_get_temp_(t);
}
static int tmp103_thermal_get_mode(struct thermal_zone_device *thermal,
				   enum thermal_device_mode *mode)
{
	struct tmp103_thermal_zone *tzone = thermal->devdata;
	mutex_lock(&thermal_lock);
	*mode = (tzone) ? tzone->mode : THERMAL_DEVICE_DISABLED;
	mutex_unlock(&thermal_lock);
	return 0;
}
static int tmp103_thermal_set_mode(struct thermal_zone_device *thermal,
				   enum thermal_device_mode mode)
{
	struct tmp103_thermal_zone *tzone = thermal->devdata;
	mutex_lock(&thermal_lock);
	if (tzone) {
		tzone->mode = mode;
		if (mode == THERMAL_DEVICE_ENABLED)
			schedule_work(&tzone->twork);
	}
	mutex_unlock(&thermal_lock);
	return 0;
}
static int tmp103_thermal_get_trip_type(struct thermal_zone_device *thermal,
					int trip,
					enum thermal_trip_type *type)
{
	*type = THERMAL_TRIP_ACTIVE;
	return 0;
}
static int tmp103_thermal_get_trip_temp(struct thermal_zone_device *thermal,
					int trip,
					unsigned long *temp)
{
	struct tmp103_thermal_zone *tzone = thermal->devdata;
	*temp = (tzone) ? tzone->trip[trip].temp : TMP103_MAX_TEMP;
	return 0;
}
static int tmp103_thermal_get_crit_temp(struct thermal_zone_device *thermal,
					unsigned long *temperature)
{
	*temperature = TMP103_MAX_TEMP;
	return 0;
}
static struct thermal_zone_device_ops tmp103_tz_dev_ops = {
	.bind = tmp103_cdev_bind,
	.unbind = tmp103_cdev_unbind,
	.get_temp = tmp103_thermal_get_temp,
	.get_mode = tmp103_thermal_get_mode,
	.set_mode = tmp103_thermal_set_mode,
	.get_trip_type = tmp103_thermal_get_trip_type,
	.get_trip_temp = tmp103_thermal_get_trip_temp,
	.get_crit_temp = tmp103_thermal_get_crit_temp,
};
static int temp_show(struct device *dev,
		     struct device_attribute *devattr, char *buf)
{
	unsigned long vs_temp = 0;
	_vs_get_temp_(&vs_temp);
	return sprintf(buf, "%d\n", (int)vs_temp);
}
static int config_show(struct device *dev,
		       struct device_attribute *devattr, char *buf)
{
	int i = 0;
	int ret = 0;
	int d, d0, d1, d2;
	char pbuf[BUF_SIZE];

	ret += sprintf(pbuf + ret, "offsets ");
	for (i = 0; i < NUM_SENSORS; i++) {
		d = (int)abs(vs_data[i].offset%DMF);
		d0 = (d < 100) ? 0 : d/100;
		d = (d%100);
		d1 = (d < 10) ? 0 : d/10;
		d2 = (d%10);
		ret += sprintf(pbuf + ret, "%d.%d%d%d ",
		vs_data[i].offset/DMF, d0, d1, d2);
	}
	ret += sprintf(pbuf + ret, "\nalphas  ");
	for (i = 0; i < NUM_SENSORS; i++) {
		d = (int)abs(vs_data[i].alpha%DMF);
		d0 = (d < 100) ? 0 : d/100;
		d = (d%100);
		d1 = (d < 10) ? 0 : d/10;
		d2 = (d%10);
		ret += sprintf(pbuf + ret, "%d.%d%d%d ",
		vs_data[i].alpha/DMF, d0, d1, d2);
	}
	ret += sprintf(pbuf + ret, "\nweights ");
	for (i = 0; i < NUM_SENSORS; i++) {
		d = (int)abs(vs_data[i].weight%DMF);
		d0 = (d < 100) ? 0 : d/100;
		d = (d%100);
		d1 = (d < 10) ? 0 : d/10;
		d2 = (d%10);
		ret += sprintf(pbuf + ret, "%d.%d%d%d ",
		vs_data[i].weight/DMF, d0, d1, d2);
	}

	return sprintf(buf, "%s\n", pbuf);
}
static int thermal_zone_show(struct device *dev,
			     struct device_attribute *devattr,
			     char *buf)
{
	int len = 0;
	char pbuf[BUF_SIZE];
	int i;
	struct platform_device *pdev = to_platform_device(dev);
	struct tmp103_thermal_zone *tzone = platform_get_drvdata(pdev);

	if (tzone == NULL) {
		pr_err("%s Error platform data\n", __func__);
		return -EINVAL;
	}

	len += sprintf((pbuf + len), "trips=%d ", tzone->num_trips);
	for (i = 0; i < tzone->num_trips; i++)
		len += sprintf((pbuf + len), "temp=%d type=%d cdev=%s ",
			       (int)tzone->trip[i].temp,
			       tzone->trip[i].type,
			       tzone->trip[i].name);
	len += sprintf((pbuf + len), "delay=%d", tzone->delay);
	return sprintf(buf, "%s\n", pbuf);
}
static int cmd_check_parsing(const char *buf, int *idx, int *x, int *y, int *z)
{
	int ret = 0;
	int index = -1;
	const char dot[] = ".";
	const char space[] = " ";
	char *token, *str;
	int r, d, i, j, k, fact;
	int conf[3];
	char dec[2];
	size_t len;
	int t[3] = {0, 0, 0};

	dec[1] = '\0';
	str = kstrdup(buf, GFP_KERNEL);
	token = strsep(&str, space);
	if (token == NULL) {
		pr_err("%s: Error NULL token => %s\n", __func__, token);
		return -1;
	}
	for (i = 0; i < NUM_SENSORS; i++) {
		if (!strcmp(token, vs_conf_names[i]))
			index = i;

	}
	if ((index < 0) || (index > 2))
		pr_err("%s: Error Invalid Index %d config %s\n",
		       __func__,
		       index,
		       token);
	for (i = 0; i < NUM_SENSORS; i++) {
		d = 0;
		r = 0;
		token = strsep(&str, dot);
		if (token) {
			ret = kstrtoint(token, 10, &r);
			if (ret < 0) {
				pr_err("%s: Error Invalid Real Val%d Ret %d token => %s str =>%s\n",
				       __func__,
				       i,
				       ret,
				       token,
				       str);
				return ret;
			}
		} else
			return -1;
		token = strsep(&str, space);
		if (token) {
			fact = 100;
			len = strlen(token);
			len = (len < 3) ? len : 3;
			for (k = 0; k < 3; k++)
				t[k] = 0;
			for (j = 0; j < len; j++) {
				if (token[j] == '\n')
					break;
				dec[0] = token[j];
				ret = kstrtoint(dec, 10, &t[j]);
				if (ret < 0) {
					pr_err("%s: Error Invalid Dec Val%d Ret%d len:%d\n",
					       __func__,
					       j,
					       ret,
					       len);
					pr_err("%s: Error Invalid Dec = %s token = %s str = %s\n",
					       __func__,
					       dec,
					       token,
					       str);
					return ret;
				}
				d += t[j] * fact;
				fact /= 10;
			}
		} else
			return -1;
		if (d < 0)
			return -1;
		if (r < 0)
			conf[i] = r*DMF - d;
		else
			conf[i] = r*DMF + d;
	}

	*idx = index;
	*x = conf[0];
	*y = conf[1];
	*z = conf[2];

	pr_debug("%s: Wrote Index%d off%d alp=%d wei%d\n",
		 __func__,
		 index,
		 conf[0],
		 conf[1],
		 conf[2]);
	return ret;
}
static ssize_t config_set(struct device *dev,
			  struct device_attribute *devattr,
			  const char *pbuf,
			  size_t count)
{
	int ret, index;
	int i = 0;
	int d[3];

	ret = cmd_check_parsing(pbuf, &index, &d[0], &d[1], &d[2]);
	if ((ret < 0) || (index < 0) || (index > 2))
		return -1;
	for (i = 0; i < NUM_SENSORS; i++) {
		if (index == 0)
			vs_data[i].offset = d[i];
		if (index == 1)
			vs_data[i].alpha =  d[i];
		if (index == 2)
			vs_data[i].weight = d[i];
	}
	return count;
}
static ssize_t thermal_zone_set(struct device *dev,
				 struct device_attribute *devattr,
				const char *pbuf, size_t count)
{
	int off, i, j;
	char name[THERMAL_NAME_LENGTH];
	int temp;
	int type;
	struct platform_device *pdev = to_platform_device(dev);
	struct tmp103_thermal_zone *tzone = platform_get_drvdata(pdev);

	if (tzone == NULL) {
		pr_err("%s Error platform data\n", __func__);
		return -EINVAL;
	}
	if (sscanf(pbuf, "%d%n", &(tzone->num_trips), &off) == 1) {
		pbuf += off;
		for (i = 0; i < tzone->num_trips; i++) {
			if (sscanf(pbuf, "%d %d %19s%n",
				   &temp,
				   &type, name,
				   &off) != 3) {
				pr_err("%s Failed to write thermal zone config  %s\n",
				       __func__,
				       pbuf);
				return -EINVAL;
			}
			for (j = 0; j < NUM_COOLERS; j++) {
				if (!strcmp(coolers[j], name)) {
					tzone->trip[i].temp = temp;
					tzone->trip[i].type = type;
					break;
				}
			}
			pbuf += off;
		}
		if (sscanf(pbuf, "%d%n", &tzone->delay, &off) == 1) {
			pbuf += off;
			if (pbuf[0] != '\n') {
				pr_err("%s Failed to detect EOF %s %c\n",
				       __func__,
				       pbuf,
				       pbuf[0]);
				return -EINVAL;
			}
		} else {
			pr_err("%s Failed to write thermal zone config delay %s\n",
			       __func__,
			       pbuf);
			return -EINVAL;
		}
		tzone->tdev->polling_delay = tzone->delay;
		schedule_work(&tzone->twork);
		return count;
	}
	pr_err("%s Failed to write thermal zone config %s\n",
	       __func__,
	       pbuf);
	return -EINVAL;
}
static void tmp103_thermal_work(struct work_struct *work)
{
	struct tmp103_thermal_zone *tzone;
	mutex_lock(&thermal_lock);
	tzone = container_of(work, struct tmp103_thermal_zone, twork);
	if (tzone) {
		if (tzone->mode == THERMAL_DEVICE_DISABLED)
			return;
		thermal_zone_device_update(tzone->tdev);
	}
	mutex_unlock(&thermal_lock);
}

static DEVICE_ATTR(temp, S_IRUGO, temp_show, NULL);
static DEVICE_ATTR(config, S_IWUSR | S_IRUGO, config_show, config_set);
static DEVICE_ATTR(thermal_zone,
		   S_IWUSR | S_IRUGO,
		   thermal_zone_show,
		   thermal_zone_set);
static struct attribute *tmp103_thermal_attributes[] = {
	&dev_attr_temp.attr,
	&dev_attr_config.attr,
	&dev_attr_thermal_zone.attr,
	NULL
};
static const struct attribute_group tmp103_thermal_attr_group = {
	.attrs = tmp103_thermal_attributes,
};
static int tmp103_thermal_probe(struct platform_device *pdev)
{
	int ret;
	int i;
	struct tmp103_thermal_zone *tzone;
	unsigned int board_type;

#ifdef CONFIG_IDME
	board_type = idme_get_board_type();
#else
	board_type = BOARD_ID_ARIEL;
#endif
	if (board_type == BOARD_ID_ASTON)
		memcpy(vs_data, aston_vs_data, sizeof(vs_data));
	else
		memcpy(vs_data, ariel_vs_data, sizeof(vs_data));

	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;
	memset(tzone, 0, sizeof(*tzone));
	tzone->mode = THERMAL_DEVICE_ENABLED;
	tzone->num_trips = NUM_COOLERS;
	tzone->delay = 100;
	for (i = 0; i < NUM_COOLERS; i++) {
		strlcpy(tzone->trip[i].name, coolers[i], sizeof(tzone->trip[i].name));
		tzone->trip[i].temp = TMP103_MAX_TEMP;
		tzone->trip[i].type = THERMAL_TRIP_ACTIVE;
	}
	tzone->tdev = thermal_zone_device_register(THERMAL_NAME,
						   NUM_COOLERS,
						   0,
						   tzone,
						   &tmp103_tz_dev_ops,
						   NULL,
						   0,
						   0);
	if (IS_ERR(tzone->tdev)) {
		pr_err("%s Failed to register thermal zone device\n", __func__);
		ret = -EINVAL;
	}
	ret = sysfs_create_group(&pdev->dev.kobj, &tmp103_thermal_attr_group);
	if (ret)
		return ret;
	INIT_WORK(&tzone->twork, tmp103_thermal_work);
	tzone->mode = THERMAL_DEVICE_ENABLED;
	platform_set_drvdata(pdev, tzone);
	return ret;
}
static int tmp103_thermal_remove(struct platform_device *pdev)
{
	struct tmp103_thermal_zone *tzone = platform_get_drvdata(pdev);
	if ((tzone) && (tzone->tdev))
		thermal_zone_device_unregister(tzone->tdev);
	cancel_work_sync(&tzone->twork);
	return 0;
}
int thermal_sensor_register(struct thermal_dev *tdev)
{
	if (unlikely(IS_ERR_OR_NULL(tdev))) {
		pr_err("%s: NULL sensor thermal device\n", __func__);
		return -ENODEV;
	}
	if (!tdev->dev_ops->report_temp) {
		pr_err("%s: Error getting report_temp()\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&thermal_lock);
	list_add_tail(&tdev->node, &thermal_sensor_list);
	mutex_unlock(&thermal_lock);
	return 0;
}
EXPORT_SYMBOL(thermal_sensor_register);

static struct platform_device tmp103_tz_device = {
	.name   = DRIVER_NAME,
	.id     = -1,
	.dev    = {
		.platform_data = NULL
	},
};
static struct platform_driver tmp103_tz_driver = {
	.probe = tmp103_thermal_probe,
	.remove = tmp103_thermal_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown   = NULL,
	.driver     = {
		.name  = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};
static int __init tmp103_thermal_init(void)
{
	int err;
	err = platform_device_register(&tmp103_tz_device);
	if (err) {
		pr_err("%s: Failed to register tmp103 tz device\n", __func__);
		return err;
	}
	err = platform_driver_register(&tmp103_tz_driver);
	if (err) {
		pr_err("%s: Failed to register tmp103 tz driver\n", __func__);
		return err;
	}
	return err;
}
static void __exit tmp103_thermal_exit(void)
{
	platform_device_unregister(&tmp103_tz_device);
	platform_driver_unregister(&tmp103_tz_driver);
}

late_initcall(tmp103_thermal_init);
module_exit(tmp103_thermal_exit);

MODULE_DESCRIPTION("TMP103 pcb virtual sensor thermal zone driver");
MODULE_AUTHOR("Akwasi Boateng <boatenga@lab126.com>");
MODULE_LICENSE("GPL");
