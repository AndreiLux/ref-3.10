/*
 * linux/drivers/video/odin/ dss/cabc.c
 * Junglark Park <junglark.park@lgepartner.com>
  * Copyright (C) 2013 LG Electronics
 * Author: junglark.park  <junglark.park@lgepartner.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/err.h>

static struct class *cabc_class;

struct device *cabc_device_register(struct device * dev, const char *dev_name)
{
	struct device * cabc_dev;
	int id;

	cabc_dev = device_create(cabc_class, dev, MKDEV(0,0), NULL, dev_name);
	
	return cabc_dev;
}

EXPORT_SYMBOL_GPL(cabc_device_register);

void cabc_device_unregister(struct device *dev)
{
	device_unregister(dev);
}

EXPORT_SYMBOL_GPL(cabc_device_unregister);

static int __init cabc_init(void)
{
	cabc_class = class_create(THIS_MODULE, "cabc");

	if(IS_ERR(cabc_class)) {
		pr_err("couldn't create sysfs class \n");
		return PTR_ERR(cabc_class);
	}

	return 0;
}
static int __exit cabc_exit(void)
{
	class_destroy(cabc_class);

	return 0;
}
subsys_initcall(cabc_init);
module_exit(cabc_init);
