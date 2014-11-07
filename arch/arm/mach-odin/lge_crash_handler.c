/*
 * arch/arm/mach-odin/lge_crash_handler.c
 *
 * Copyright (C) 2013 LGE, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

//-----------------------------------------------------------
// Date              Version           Author
//                                                                        
//-----------------------------------------------------------
#include <linux/device.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/setup.h>

#include <linux/module.h>
#include <linux/slab.h>
#include "pm.h"

static volatile void *	crash_ram_dump = NULL;
static volatile void *	crash_dump_log = NULL;
static unsigned int crash_buf_size = 0;

static DEFINE_SPINLOCK(lge_crash_lock);


static int lge_hidden_reset_enable = 0;
static int lge_crash_handler_enable = 1;

/* sysfs for crash handler & hidden reset control */
module_param_named(lge_crash_handler_enable, lge_crash_handler_enable, int, S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(lge_hidden_reset_enable, lge_hidden_reset_enable, int, S_IRUGO | S_IWUSR | S_IWGRP);

/* sysfs for generate panic & null-ptr exception */
static int dummy_arg;
static int gen_bug(const char *val, struct kernel_param *kp)
{
	BUG();
	return 0;
}
module_param_call(gen_bug, gen_bug, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_panic(const char *val, struct kernel_param *kp)
{
	panic("generate test-panic");
	return 0;
}
module_param_call(gen_panic, gen_panic, param_get_bool, &dummy_arg, S_IWUSR | S_IRUGO);

static int gen_null(const char *val, struct kernel_param *kp)
{
	int * ptr = NULL;
	ptr = 0x00000000;
	*(ptr) = 1;
	return 0;
}
module_param_call(gen_null, gen_null, param_get_bool, &dummy_arg, S_IWUSR | S_IRUGO);

/* import external functions... */
extern void lge_set_reboot_reason(unsigned int reason);

static int lge_crash_handler(struct notifier_block *this, unsigned long event,
		void *ptr)
{
	unsigned long flags;

	spin_lock_irqsave(&lge_crash_lock, flags);

	printk(KERN_EMERG"%p ", crash_ram_dump);
	printk(KERN_EMERG"%p ", crash_dump_log);
	printk(KERN_EMERG"%s: %d\n",__func__, crash_buf_size);

	if (lge_hidden_reset_enable)
		lge_set_reboot_reason(HIDDEN_REBOOT);
	else
		lge_set_reboot_reason(CRASH_REBOOT);

	spin_unlock_irqrestore(&lge_crash_lock, flags);

	return NOTIFY_DONE;
}

/* register lge crash handler */
static struct notifier_block lge_crash_handler_block = {
	.notifier_call  = lge_crash_handler,
};

int lge_crash_handler_init(unsigned long mem_address, unsigned long mem_size, unsigned long console_size)
{
	crash_ram_dump = (void*)mem_address;
	crash_dump_log = (void*)mem_address+console_size;
	crash_buf_size = console_size;

	/* Setup panic notifier */
	atomic_notifier_chain_register(&panic_notifier_list, &lge_crash_handler_block);
}
EXPORT_SYMBOL(lge_crash_handler_init);

/* check hidden reset mode in boot time */
static int __init lge_set_hidden_reset_mode(char *hreset_on)
{
	if( !strncmp(hreset_on, "on", 2) ) {
		lge_hidden_reset_enable = 1;
		printk("reboot mode : hidden reset %s\n", "on");
	}

	return 1;
}
__setup("lge.hreset=", lge_set_hidden_reset_mode);

