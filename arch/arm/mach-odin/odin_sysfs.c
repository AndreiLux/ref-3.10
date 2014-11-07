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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/async.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/odin_pmic.h>

static struct dentry *odin_debugfs_root;

typedef enum BOOT_REASON {
	HW_BOOT = 0x0,
	SW_BOOT,
	HW_RESET,
	SMPL_BOOT,
}BOOT_REASON_INDEX;

int print_boot_reason(unsigned int boot_reason)
{
	switch (boot_reason) {
	case HW_BOOT:
		printk("HW Boot => Power on by POWER KEY \n");
		break;
	case SW_BOOT:
		printk("SW Reset => Power on by SW REBOOT \n");
		break;
	case HW_RESET:
		printk("HW Reset => Power on by HARDWARE RESET \n");
		break;
	case SMPL_BOOT:
		printk("SMPL Boot => Power on by SMPL \n");
		break;
	default:
		break;
	}

	return 0;
}

static ssize_t boot_reason_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	ssize_t len, ret = 0;

	if (!buf)
		return -ENOMEM;

	ret = print_boot_reason(odin_pmic_bootreason_get());
	if (ret < 0)
		goto err;

	kfree(buf);

	return ret;

err:
	kfree(buf);
	return 0;
}

static const struct file_operations show_information = {
	.read = boot_reason_read,
};

static __init int odin_sysfs_init(void)
{
	int ret;

	ret = print_boot_reason(odin_pmic_bootreason_get());
	if (ret < 0)
		return ret;

	odin_debugfs_root = debugfs_create_dir("odin_info", NULL);
	if (!odin_debugfs_root)
		pr_warn("%s: Failed to create debugfs directory\n",__func__);

	debugfs_create_file("boot_reason", 0444, odin_debugfs_root, NULL, &show_information);

	return ret;
}

subsys_initcall(odin_sysfs_init);

MODULE_DESCRIPTION("ODIN Information");
MODULE_AUTHOR("IN HYUK KANG <hugh.kang@lge.com>");
MODULE_LICENSE("GPL v2");
