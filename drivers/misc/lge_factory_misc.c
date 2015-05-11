/* drivers/misc/lge_factory_misc.c
 *
 * Copyright (C) 2015 LGE
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <soc/qcom/lge/board_lge.h>
#include "lge_factory_misc.h"

static struct platform_device factory_misc_platrom_device = {
	.name   = "factory_misc",
};

static int __init factory_misc_init(void)
{
	return platform_device_register(&factory_misc_platrom_device);
}

static ssize_t key_lock_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int value;
	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	pr_err("************************* Key lock %d *************************\n",
		value);

	evdev_set_event_lock(value);

	return count;
}

static ssize_t key_lock_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	ssize_t ret_len = 0;
	bool key_lock_status = evdev_get_event_lock();
	ret_len = snprintf(buf, PAGE_SIZE,
			"Key lock status %s\n",
			key_lock_status ? "Enabled" : "Disabled");

	return ret_len;
}

static DEVICE_ATTR(key_lock, 0644, key_lock_show, key_lock_store);

static int factory_misc_ctrl_probe(struct platform_device *pdev)
{
	if (device_create_file(&pdev->dev, &dev_attr_key_lock) != 0)
		return -EPERM;

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = factory_misc_ctrl_probe,
	.driver = {
		.name = "factory_misc",
	},
};

int __init factory_misc_ctrl_init(void)
{
	return platform_driver_register(&this_driver);
}

device_initcall(factory_misc_init);
device_initcall(factory_misc_ctrl_init);


MODULE_DESCRIPTION("LGE factory misc driver");
MODULE_LICENSE("GPL v2");
