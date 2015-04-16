/*
 * rdr unit test . (RDR: kernel run data recorder.)
 *
 * Copyright (c) 2013 Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/thread_info.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/kallsyms.h>
#include <linux/huawei/rdr.h>
#include "rdr_internal.h"

#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>

long exec_ut_func(unsigned long addr, long arg0, long arg1, long arg2)
{
	register long rr0 asm("r0") = arg0;
	register long rr1 asm("r1") = arg1;
	register long rr2 asm("r2") = arg2;
	register long ret0 asm("r0");
	/*register long ret1 asm("r1");*/
	unsigned long cpsr = 0;

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[cpsr]\n\t"
		"mov	lr, pc\n\t"
		"mov	pc, %[addr]\n\t"
		: "=r" (ret0)/*, "=r" (ret1)*/
		: "0" (rr0), "r" (rr1), "r" (rr2),
		  [cpsr] "r" (cpsr), [addr] "r" (addr)
		: "lr", "cc"
	);
	return ret0;
}

long call_rdr_ut_func(char *func, long arg0, long arg1, long arg2)
{
	unsigned long addr = kallsyms_lookup_name("rdr_open_dir");
	if (addr == 0) {
		pr_err("this function is not exist or too simple, skip.\n");
		return 0;
	}

	return exec_ut_func(addr, arg0, arg1, arg2);
}

int rdr_ut_om_open_dir(void)
{
	int ret;
	/* prepare */
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* call func: */
	ret = call_rdr_ut_func("rdr_open_dir",
			(int)("/data/hisi_logs/memorydump/"), 0, 0);

	/* clear */
	if (ret >= 0)
		(void)sys_close((unsigned int)ret);
	set_fs(old_fs);

	/* assert and return */
	return (ret < 0) ? -1 : 0;
}

static ssize_t rdr_ut(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	int ret;
	ret = snprintf(buf, PAGE_SIZE - 1, "test:%s\n", __func__);
	pr_err("test rdr_open_dir():%d\n", rdr_ut_om_open_dir());
	return ret;
}

static struct kobj_attribute rdr_ut_attr =
	__ATTR(ut, 0444, rdr_ut, NULL);

static const struct attribute *rdr_test_attrs[] = {
	&rdr_ut_attr.attr,
	NULL
};

int sys_add_rdr_ut(void)
{
	while (rdr_kobj == NULL)
		msleep(1000);
	return sysfs_create_files(rdr_kobj, rdr_test_attrs);
}

void sys_del_rdr_ut(void)
{
	if (rdr_kobj != NULL)
		sysfs_remove_files(rdr_kobj, rdr_test_attrs);
}
