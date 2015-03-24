/*
 * trapz.c
 *
 * TRAPZ (TRAcing and Profiling for Zpeed) Log Driver
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.
 * Andy Prunicki (prunicki@lab126.com)
 * Martin Unsal (munsal@lab126.com)
 * TODO: Add additional contributor's names.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/wait.h>
#include <linux/trapz.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/atomic.h>
#include <linux/module.h>

#include "trapz_device.h"
#ifdef CONFIG_TRAPZ_TRIGGER
#include "trapz_trigger.h"
#endif

#define TRAPZ_VERSION "0.2"
/* The buffer size is the number of components * 2 bits, giving 3 values
   for each component. */
#define TRAPZ_LOG_LEVEL_BUFF_SIZE ((TRAPZ_MAX_COMP_ID + 1) >> 2)
#define TRAPZ_DEFAULT_BUFFER_SIZE 10000

#define MAX_INT_DIGIT 10

#ifdef CONFIG_TRAPZ_TRIGGER
/* The trigger implementation is admittedly not performant for very many
	triggers.  We have weak requirements around triggers, and thus a simple,
	small linked list is our current implementation until we have more
	concrete requirements. */
LIST_HEAD(trigger_head);
static int g_trigger_count;
#endif

static struct _trapz_device_info {
	struct miscdevice trapz_device;
	struct device_attribute version_attr;
	struct device_attribute enabled_attr;
	struct device_attribute buff_size_attr;
	struct device_attribute count_attr;
	struct device_attribute total_attr;
	struct device_attribute clear_buff_attr;
	struct device_attribute reset_attr;
	/* spin lock for SMP */
	spinlock_t lock;
	atomic_t blocked;
	/* indicates if device is enabled */
	atomic_t enabled;
	/* indicates if there is a pending buffer resize */
	int pending_buffsize_change;
	wait_queue_head_t wq;
} trapz_device_info = {
	.enabled = ATOMIC_INIT(0),
	.blocked = ATOMIC_INIT(0),
	.pending_buffsize_change = 0
};

static struct trapz_data {
	/* base address of internal entry buffer */
	trapz_entry_t *pBuffer;
	/* first entry address past the end of the buffer */
	trapz_entry_t *pLimit;
	/* entries are added at the head */
	trapz_entry_t *pHead;
	/* entries are removed from the tail */
	trapz_entry_t *pTail;
	/* the buffer will hold this many entries */
	int bufferSize;
	/* the buffer contains this many entries */
	int count;
	/* count of entries added since the last reset */
	int total;
	/* the rolling data counter (1 thru 255) */
	u8 counter;
	/* log levels for OS components */
	char *os_comp_loglevels;
	/* log levels for app components */
	char *app_comp_loglevels;
} g_data = {
	.counter = 1
};

static int trapz_open(struct inode *, struct file *);
static int trapz_release(struct inode *, struct file *);
static ssize_t trapz_read(struct file *, char *, size_t, loff_t *);
static ssize_t trapz_write(struct file *, const char *, size_t, loff_t *);
static loff_t trapz_llseek(struct file *filp, loff_t off, int whence);
static long trapz_ioctl(struct file *, unsigned int, unsigned long);
static ssize_t attr_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t attr_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count);


#define TRAPZ_CREATE_RO_ATTR(_dev_info, _name) \
	_dev_info._name##_attr.attr.name = __stringify(_name); \
	_dev_info._name##_attr.attr.mode = (S_IRUSR|S_IRGRP|S_IROTH); \
	_dev_info._name##_attr.show = attr_show

#define TRAPZ_CREATE_RW_ATTR(_dev_info, _name) \
	_dev_info._name##_attr.attr.name = __stringify(_name); \
	_dev_info._name##_attr.attr.mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH); \
	_dev_info._name##_attr.show = attr_show; \
	_dev_info._name##_attr.store = attr_store

const struct file_operations fops = {
	.llseek = trapz_llseek,
	.read = trapz_read,
	.write = trapz_write,
	.open = trapz_open,
	.release = trapz_release,
	.unlocked_ioctl = trapz_ioctl,
};

/*============================================================================*/

/**
 * Kernel API used to log trapz trace points.
 */
SYSCALL_DEFINE6(trapz, unsigned int, ctrl, unsigned int, extra1, unsigned int,
	extra2, unsigned int, extra3, unsigned int,
	extra4, struct trapz_info __user *, ti)
{
	return systrapz(ctrl, extra1, extra2, extra3, extra4, ti);
}

static inline trapz_entry_t *get_entry(void)
{
	trapz_entry_t *entry;

	if (g_data.count >= g_data.bufferSize) {
		/* We've wrapped the circular buffer.
				Move the tail accordingly. */
		g_data.pTail++;
		if (g_data.pTail >= g_data.pLimit)
			g_data.pTail = g_data.pBuffer;
		g_data.count--;
	}

	entry = g_data.pHead++;
	if (g_data.pHead >= g_data.pLimit)
		g_data.pHead = g_data.pBuffer;
	g_data.count++;
	g_data.total++;
	memset(entry, 0, sizeof(trapz_entry_t));

	return entry;
}

/**
 * Internal kernel API used to log trapz trace points.
 */
long systrapz(unsigned int ctrl, unsigned int extra1, unsigned int extra2,
	unsigned int extra3, unsigned int extra4, struct trapz_info __user *ti)
{
	unsigned long flags;
	int filtered = 1, cpu = 0;
	int level, cat_id, comp_id, trace_id;
	trapz_entry_t *pEntry1 = NULL, *pEntry2 = NULL;
	trapz_info_t kti;
	struct timespec ts;
	u8 counter, extra_count = 0;
#ifdef CONFIG_TRAPZ_TRIGGER
	trapz_trigger_event_t trigger_event;
#endif

	if (atomic_read(&trapz_device_info.enabled) == 0)
		return -EBUSY;

	level = TRAPZ_LEVEL_OUT(ctrl);
	cat_id = TRAPZ_CAT_ID_OUT(ctrl);
	comp_id = TRAPZ_COMP_ID_OUT(ctrl);
	trace_id = TRAPZ_TRACE_ID_OUT(ctrl);

	/* Filter out events based on log level */
	if (trapz_check_loglevel(level, cat_id, comp_id)) {
		/* Not filtered by log level */
		getnstimeofday(&ts);
		filtered = 0;
		/*Determine if we need an additional
			record to hold the extras */
		if (extra3 != 0 || extra4 != 0
				|| extra1 > 0xff || extra2 > 0xff) {
			/* Need extra record */
			extra_count = 1;
		}
		spin_lock_irqsave(&trapz_device_info.lock, flags);
		{
#ifdef CONFIG_TRAPZ_TRIGGER
			trigger_event.trigger.start_trace_point = 0;
			process_trigger(ctrl, &trigger_event, &trigger_head,
				&g_trigger_count, &ts);
#endif

			pEntry1 = get_entry();
			if (extra_count > 0)
				pEntry2 = get_entry();
			counter = g_data.counter++;
			if (counter == 0) {
				/* 0 is invalid */
				counter = g_data.counter++;
			}
		}
		spin_unlock_irqrestore(&trapz_device_info.lock, flags);
	}
	if (filtered) {
		/* Trapz call filtered out, return time if requested */
		if (ti != NULL) {
			getnstimeofday(&ts);
			kti.ts = ts;
			kti.counter = -1;

			if (copy_to_user(ti, &kti, sizeof(kti)))
				return -EFAULT;
		}

		return 0;
	}

	cpu = raw_smp_processor_id();

	if (!in_interrupt()) {
		pEntry1->pid = current->tgid;
		pEntry1->tid = current->pid;
	}

	pEntry1->ctrl = TRAPZ_BUFF_REC_TYPE_MASK | (cat_id & 0x3) << 4
		| (extra_count & TRAPZ_BUFF_EXTRA_COUNT_MASK);
	pEntry1->counter = counter;
	if (extra_count == 0) {
		pEntry1->extra_1 = extra1;
		pEntry1->extra_2 = extra2;
	} else {
		pEntry1->extra_1 = 0;
		pEntry1->extra_2 = 0;
	}
	pEntry1->cpu = cpu;
	pEntry1->comp_trace_id[0] = comp_id >> 4;
	pEntry1->comp_trace_id[1] =
		((comp_id & 0x0f) << 4) | ((trace_id & 0xf00) >> 8);
	pEntry1->comp_trace_id[2] = (trace_id & 0xff);
	pEntry1->ts = ts;
	pEntry1->ctrl |= TRAPZ_BUFF_COMPLETE_MASK;
	if (pEntry2 != NULL) {
		pEntry2->counter = counter;
		pEntry2->format = TRAPZ_EXTRA_FORMAT_INT;
		pEntry2->extras[0] = extra1;
		pEntry2->extras[1] = extra2;
		pEntry2->extras[2] = extra3;
		pEntry2->extras[3] = extra4;
		pEntry2->ctrl = TRAPZ_BUFF_COMPLETE_MASK;
	}

#ifdef CONFIG_TRAPZ_TRIGGER
	if (trigger_event.trigger.start_trace_point != 0) {
		send_trigger_uevent(&trigger_event,
			&(trapz_device_info.trapz_device.this_device->kobj));
	}
#endif

	/* Return time and sequence info if requested */
	if (ti != NULL) {
		trapz_info_t kti;
		kti.ts = ts;
		kti.counter = counter;

		if (copy_to_user(ti, &kti, sizeof(kti)))
			return -EFAULT;
	}

	/* unblock if needed */
	if (atomic_read(&trapz_device_info.blocked) != 0) {
		atomic_set(&trapz_device_info.blocked, 0);
		wake_up_interruptible(&(trapz_device_info.wq));
	}

	return 0;
}
EXPORT_SYMBOL(systrapz);

static int clear_buffer(void)
{
	unsigned long flags;

	if (atomic_read(&trapz_device_info.enabled) == 0)
		return -EINVAL;

	spin_lock_irqsave(&trapz_device_info.lock, flags);
	{
		g_data.total = g_data.count = g_data.counter = 0;
		g_data.pHead = g_data.pTail = g_data.pBuffer;
	}
	spin_unlock_irqrestore(&trapz_device_info.lock, flags);
	return 0;
}

static int trapz_open(struct inode *inode, struct file *file)
{
	unsigned long flags;
	/* set offset to be the current lowest sequence */
	spin_lock_irqsave(&trapz_device_info.lock, flags);
	{
		file->f_pos = g_data.total - g_data.count;
	}
	spin_unlock_irqrestore(&trapz_device_info.lock, flags);
	return 0;
}

static int trapz_release(struct inode *inode, struct file *file)
{
	return 0;
}

static loff_t trapz_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;
	switch (whence) {
	case 0: /* SEEK_SET */
		newpos = off;
		break;

	case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	case 2: /* SEEK_END */
		newpos = g_data.total + off;
		break;

	default: /* can't happen */
		return -EINVAL;
	}
	if (newpos < 0)
		return -EINVAL;
	filp->f_pos = newpos;
	return newpos;
}

/*
 * reads out trapz entries from store
*/
static ssize_t trapz_read(struct file *filp, char *buffer,
	size_t length, loff_t *offset)
{
	for (;;) {
		int base = g_data.total - g_data.count;
		int index;
		trapz_entry_t *pEnd, *pStart;
		int objcnt, objlim;
		int size;

		objlim = length / sizeof(trapz_entry_t);

		if (*offset < base)
			*offset = base;
		if (*offset >= g_data.total) {
			if (filp->f_flags & O_NONBLOCK)
				return 0;
			else {
				atomic_set(&trapz_device_info.blocked, 1);
				if (wait_event_interruptible(
					trapz_device_info.wq,
					atomic_read(
					&trapz_device_info.blocked) == 0)
				    == -ERESTARTSYS)
					return 0;
			}
			continue;
		}
		index = *offset - g_data.total + g_data.count;
		if (index < 0)
			index = 0;
		pEnd = g_data.pHead;
		pStart = g_data.pTail + index;
		if (pStart >= g_data.pLimit)
			pStart -= g_data.bufferSize;
		if (pStart < pEnd) {
			/* We intentionally do not copy in the current entry as
			 * it is racy.
			 */
			objcnt = pEnd - pStart;
		} else {
			/* We slop up to the end of the buffer in the case that
			 * our entry point is before our tail, keeps it as one
			 * copy, ie simpler.
			 */
			objcnt = g_data.pLimit - pStart;
		}
		if (objcnt > objlim)
			objcnt = objlim;
		size = objcnt * sizeof(trapz_entry_t);
		if (copy_to_user(buffer, pStart, size))
			return -EFAULT;
		/* Now we figure out if we wrapped. */
		if (g_data.total - *offset > g_data.bufferSize) {
			/* Wrapped, start over :( */
			*offset = g_data.total - g_data.count;
			continue;
		}
		*offset += objcnt;
		return size;
	}
	return -EFAULT;
}

static ssize_t trapz_write(struct file *filp, const char *buffer,
	size_t length, loff_t *offset)
{
	/* writes are not allowed */
	return 0;
}

static void init_logbuffers(const int os_val, const int app_val)
{
	memset(g_data.os_comp_loglevels, os_val, TRAPZ_LOG_LEVEL_BUFF_SIZE);
	memset(g_data.app_comp_loglevels, app_val, TRAPZ_LOG_LEVEL_BUFF_SIZE);
}

int trapz_check_loglevel(const int level, const int cat_id,
	const int component_id)
{
	int rc = 0;
	int byteIdx, bitIdx, bufflevel;
	char *buffer;

	if (atomic_read(&trapz_device_info.enabled) != 0 && component_id >= 0) {
		if (level >= TRAPZ_LOG_VERBOSE && level <= TRAPZ_LOG_OFF) {
			if (cat_id == TRAPZ_CAT_APPS)
				buffer = g_data.app_comp_loglevels;
			else
				buffer = g_data.os_comp_loglevels;

			if (component_id <= TRAPZ_MAX_COMP_ID) {
				byteIdx = component_id >> 2;
				bitIdx = (component_id % 4) << 1;
				bufflevel
					= ((buffer[byteIdx]
						& (TRAPZ_LOG_OFF << bitIdx))
						>> bitIdx);
				if (bufflevel <= level)
					rc = 1;
			}
		}
	}

	return rc;
}
EXPORT_SYMBOL(trapz_check_loglevel);

static void set_comp_loglevel(char *buffer, const int level,
	const int component_id)
{
	int byteIdx, bitIdx, bufflevel;

	if (level >= TRAPZ_LOG_VERBOSE && level <= TRAPZ_LOG_OFF
		&&	component_id >= 0
		&& component_id <= TRAPZ_MAX_COMP_ID) {
		byteIdx = component_id >> 2;
		bitIdx = (component_id % 4) << 1;
		bufflevel = level << bitIdx;
		buffer[byteIdx]
			= (buffer[byteIdx]
				& ((TRAPZ_LOG_OFF << bitIdx) ^ 0xFF))
				| bufflevel;
	}
}

static void set_loglevel(const int level, const int cat_id,
	const int component_id)
{
	int buffval = 0;

	if (atomic_read(&trapz_device_info.enabled) == 0)
		return;

	if (component_id >= 0) {
		/* Setting the log level of just a component */
		if (cat_id == TRAPZ_CAT_APPS) {
			set_comp_loglevel(g_data.app_comp_loglevels,
				level, component_id);
		} else {
			set_comp_loglevel(g_data.os_comp_loglevels,
				level, component_id);
		}
	} else {
		switch (level) {
		case TRAPZ_LOG_OFF:
			buffval = 0xFF;
			break;
		case TRAPZ_LOG_INFO:
			buffval = 0xAA;
			break;
		case TRAPZ_LOG_DEBUG:
			buffval = 0x55;
			break;
		case TRAPZ_LOG_VERBOSE:
			buffval = 0x00;
			break;
		}

		if (cat_id >= 0) {
			/* Setting the log level of entire category */
			if (cat_id == TRAPZ_CAT_APPS) {
				memset(g_data.app_comp_loglevels, buffval,
					TRAPZ_LOG_LEVEL_BUFF_SIZE);
			} else {
				memset(g_data.os_comp_loglevels, buffval,
					TRAPZ_LOG_LEVEL_BUFF_SIZE);
			}
		} else {
			/* Setting the log level of the entire device */
			init_logbuffers(buffval, buffval);
		}
	}
}

static void trapz_get_g_data(struct trapz_data *pData)
{
	unsigned long flags;

	spin_lock_irqsave(&trapz_device_info.lock, flags);
	{
		*pData = g_data;
	}
	spin_unlock_irqrestore(&trapz_device_info.lock, flags);
}

static int allocate_mem(int init_flags)
{
	int ok = 1;

	if (atomic_read(&trapz_device_info.enabled) != 0)
		printk(KERN_ERR "trapz: attempted to allocate memory when enabled!\n");

	if (((init_flags & 1) != 0) && g_data.bufferSize > 0) {
		if (g_data.pBuffer) {

			kfree(g_data.pBuffer);
			g_data.pLimit
				= g_data.pHead
				= g_data.pTail
				= g_data.pBuffer
				= 0;
		}

		g_data.pLimit = g_data.pHead = g_data.pTail = g_data.pBuffer =
			(trapz_entry_t *)
			kmalloc(sizeof(trapz_entry_t) * g_data.bufferSize,
				GFP_KERNEL);

		if (g_data.pBuffer)
			g_data.pLimit += g_data.bufferSize;
		else
			ok = 0;
	}

	if (ok && ((init_flags & 2) != 0)) {
		if (g_data.os_comp_loglevels) {

			kfree(g_data.os_comp_loglevels);
			g_data.os_comp_loglevels = 0;
		}
		if (g_data.app_comp_loglevels) {

			kfree(g_data.app_comp_loglevels);
			g_data.app_comp_loglevels = 0;
		}

		g_data.os_comp_loglevels
			= kmalloc(TRAPZ_LOG_LEVEL_BUFF_SIZE, GFP_KERNEL);
		g_data.app_comp_loglevels
			= kmalloc(TRAPZ_LOG_LEVEL_BUFF_SIZE, GFP_KERNEL);

		if (!g_data.os_comp_loglevels || !g_data.app_comp_loglevels)
			ok = 0;
	}

	if (!ok) {
		printk(KERN_ERR "trapz: cannot allocate kernel memory\n");

		if (g_data.pBuffer) {

			kfree(g_data.pBuffer);
			g_data.pLimit
				= g_data.pHead
				= g_data.pTail
				= g_data.pBuffer
				= 0;
		}
		if (g_data.os_comp_loglevels) {

			kfree(g_data.os_comp_loglevels);
			g_data.os_comp_loglevels = 0;
		}
		if (g_data.app_comp_loglevels) {

			kfree(g_data.app_comp_loglevels);
			g_data.app_comp_loglevels = 0;
		}
		return -ENOMEM;
	}

	return 0;
}

static int enable_driver(int enable)
{
	int rc = 0;

	if (enable) {
		/* Enabling device.
			Check to see if there is a pending buffer size
			change. If so, we need to re-allocate memory. */
		if (trapz_device_info.pending_buffsize_change) {
			trapz_device_info.pending_buffsize_change = 0;
			rc = allocate_mem(1);
			if (rc == 0)
				atomic_set(&trapz_device_info.enabled, 1);
			else {
				printk(KERN_ERR
					"trapz: could not enable trapz - out of memory\n");
				g_data.bufferSize = 0;
			}
		} else {
			/* No need to re-allocate, just enable */
			atomic_set(&trapz_device_info.enabled, 1);
		}
	} else
		atomic_set(&trapz_device_info.enabled, 0);
	return rc;
}

static int set_buff_size(int new_buff_size)
{
	int rc = 0;

	if (atomic_read(&trapz_device_info.enabled) == 0) {
		if (new_buff_size > 0 && new_buff_size <= 1000000) {
			g_data.bufferSize = new_buff_size;
			trapz_device_info.pending_buffsize_change = 1;
		} else {
			printk(KERN_INFO
				"trapz: attempted to set attribute buffer size to %d "
				"value must be between 0 an 1000000\n",
				new_buff_size);
		}
	} else {
		printk(KERN_INFO
			"trapz: attempted to set buffer size when driver is active!\n");
		rc = -EINVAL;
	}

	return rc;
}

static void reset_defaults(void)
{
	unsigned long flags;

	spin_lock_irqsave(&trapz_device_info.lock, flags);
	{
		init_logbuffers(0xFF, 0xFF);
#ifdef CONFIG_TRAPZ_TRIGGER
		clear_triggers(&trigger_head, &g_trigger_count);
#endif
	}
	spin_unlock_irqrestore(&trapz_device_info.lock, flags);
}

static long trapz_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long rc = -EINVAL;
	int level, cat_id, component_id;
	trapz_config config;
	struct trapz_data data;
#ifdef CONFIG_TRAPZ_TRIGGER
	unsigned long flags;
	trapz_trigger_t trigger;
#endif

	switch (cmd) {
	case TRAPZ_ENABLE_DRIVER:
		enable_driver(arg);
		break;
	case TRAPZ_GET_CONFIG:
		trapz_get_g_data(&data);
		config.bufferSize = data.bufferSize;
		config.count = data.count;
		config.total = data.total;
		rc = 0;
		if (copy_to_user((void __user *) arg, &config,
				sizeof(trapz_config))) {
			rc = -EFAULT;
		}
		break;
	case TRAPZ_SET_BUFFER_SIZE:
		rc = set_buff_size(arg);
		break;
	case TRAPZ_RESET_DEFAULTS:
		rc = 0;
		reset_defaults();
		break;
	case TRAPZ_CLEAR_BUFFER:
		rc = clear_buffer();
		break;
	case TRAPZ_GET_VERSION:
		/* copy entire string with terminating null */
		rc = 0;
		if (copy_to_user((void __user *)arg, TRAPZ_VERSION,
			strlen(TRAPZ_VERSION)+1)) {
			rc = -EFAULT;
		}
		break;
	case TRAPZ_SET_LOG_LEVEL:
		rc = 0;
		level
		= (arg &
			(TRAPZ_LEVEL_MASK <<
				(TRAPZ_CAT_ID_SIZE + TRAPZ_COMP_ID_SIZE + 2)))
			>> (TRAPZ_CAT_ID_SIZE + TRAPZ_COMP_ID_SIZE + 2);
		cat_id = -1;
		component_id = -1;
		if (arg &
			(1 << (TRAPZ_CAT_ID_SIZE + TRAPZ_COMP_ID_SIZE + 1))) {
			cat_id
			= (arg & (TRAPZ_CAT_ID_MASK << TRAPZ_COMP_ID_SIZE))
				>> TRAPZ_COMP_ID_SIZE;
		}
		if (arg & (1 << (TRAPZ_CAT_ID_SIZE + TRAPZ_COMP_ID_SIZE)))
			component_id = arg & TRAPZ_COMP_ID_MASK;
		set_loglevel(level, cat_id, component_id);
		break;
	case TRAPZ_CHK_LOG_LEVEL:
		rc = 0;
		level
		= (arg &
			(TRAPZ_LEVEL_MASK <<
				(TRAPZ_CAT_ID_SIZE + TRAPZ_COMP_ID_SIZE + 2)))
			>> (TRAPZ_CAT_ID_SIZE + TRAPZ_COMP_ID_SIZE + 2);
		cat_id = -1;
		component_id = -1;
		if (arg & (1 << (TRAPZ_CAT_ID_SIZE + TRAPZ_COMP_ID_SIZE + 1))) {
			cat_id
			= (arg & (TRAPZ_CAT_ID_MASK << TRAPZ_COMP_ID_SIZE))
				>> TRAPZ_COMP_ID_SIZE;
		}
		if (arg & (1 << (TRAPZ_CAT_ID_SIZE + TRAPZ_COMP_ID_SIZE)))
			component_id = arg & TRAPZ_COMP_ID_MASK;
		if (cat_id >= 0 && component_id >= 0) {
			/* We only check for actual
				component ids - not general */
			rc = trapz_check_loglevel(level, cat_id, component_id);
		}
		break;
#ifdef CONFIG_TRAPZ_TRIGGER
	case TRAPZ_ADD_TRIGGER:
		if (copy_from_user(&trigger,
			(void __user *) arg, sizeof(trigger)))
			rc = -EFAULT;
		else {
			spin_lock_irqsave(&trapz_device_info.lock, flags);
			{
				rc
				= add_trigger(&trigger,
					&trigger_head, &g_trigger_count);
			}
			spin_unlock_irqrestore(&trapz_device_info.lock, flags);
		}
		break;
	case TRAPZ_DEL_TRIGGER:
		if (copy_from_user(&trigger,
			(void __user *) arg, sizeof(trigger)))
			rc = -EFAULT;
		else {
			spin_lock_irqsave(&trapz_device_info.lock, flags);
			{
				rc = delete_trigger(&trigger,
					&trigger_head, &g_trigger_count);
			}
			spin_unlock_irqrestore(&trapz_device_info.lock, flags);
		}
		break;
	case TRAPZ_CLR_TRIGGERS:
		spin_lock_irqsave(&trapz_device_info.lock, flags);
		{
			clear_triggers(&trigger_head, &g_trigger_count);
		}
		spin_unlock_irqrestore(&trapz_device_info.lock, flags);
		rc = 0;
		break;
	case TRAPZ_CNT_TRIGGERS:
		rc = g_trigger_count;
		break;
#endif
	default:
		break;
	}
	return rc;
}

static ssize_t attr_show(struct device *dev, struct device_attribute *attr,
		char *buf) {
	if (strcmp(attr->attr.name, "version") == 0)
		return snprintf(buf, MAX_INT_DIGIT, "%s", TRAPZ_VERSION);
	else if (strcmp(attr->attr.name, "enabled") == 0) {
		return snprintf(buf, MAX_INT_DIGIT, "%d",
			atomic_read(&trapz_device_info.enabled));
	} else if (strcmp(attr->attr.name, "buff_size") == 0)
		return snprintf(buf, MAX_INT_DIGIT, "%d", g_data.bufferSize);
	else if (strcmp(attr->attr.name, "count") == 0)
		return snprintf(buf, MAX_INT_DIGIT, "%d", g_data.count);
	else if (strcmp(attr->attr.name, "total") == 0)
		return snprintf(buf, MAX_INT_DIGIT, "%d", g_data.total);

	return snprintf(buf, MAX_INT_DIGIT, "0");
}

static ssize_t attr_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ret;
	unsigned long size_of_buff;
	if (strcmp(attr->attr.name, "enabled") == 0) {
		if (strncmp(buf, "0", 1) == 0)
			enable_driver(0);
		else
			enable_driver(1);
	} else if (strcmp(attr->attr.name, "buff_size") == 0) {
		ret = kstrtoul(buf, 0, &size_of_buff);
		if (ret == 0)
			set_buff_size(size_of_buff);
	} else if (strcmp(attr->attr.name, "clear_buff") == 0) {
		if (strncmp(buf, "1", 1) == 0)
			clear_buffer();
	} else if (strcmp(attr->attr.name, "reset") == 0) {
		if (strncmp(buf, "1", 1) == 0)
			reset_defaults();
	}
	return count;
}

/**
 * Setup device sysfs attributes
 */
static int __init trapz_sysfs_init(void)
{
	int rc = 0;
	TRAPZ_CREATE_RO_ATTR(trapz_device_info, version);
	TRAPZ_CREATE_RW_ATTR(trapz_device_info, enabled);
	TRAPZ_CREATE_RW_ATTR(trapz_device_info, buff_size);
	TRAPZ_CREATE_RO_ATTR(trapz_device_info, count);
	TRAPZ_CREATE_RO_ATTR(trapz_device_info, total);
	TRAPZ_CREATE_RW_ATTR(trapz_device_info, clear_buff);
	TRAPZ_CREATE_RW_ATTR(trapz_device_info, reset);
	rc = sysfs_create_file(
		&(trapz_device_info.trapz_device.this_device->kobj),
			&(trapz_device_info.version_attr.attr));
	if (!rc) {
		rc
		= sysfs_create_file(
			&(trapz_device_info.trapz_device.this_device->kobj),
				&(trapz_device_info.buff_size_attr.attr));
	}
	if (!rc) {
		rc
		= sysfs_create_file(
			&(trapz_device_info.trapz_device.this_device->kobj),
				&(trapz_device_info.count_attr.attr));
	}
	if (!rc) {
		rc
		= sysfs_create_file(
			&(trapz_device_info.trapz_device.this_device->kobj),
				&(trapz_device_info.total_attr.attr));
	}
	if (!rc) {
		rc
		= sysfs_create_file(
			&(trapz_device_info.trapz_device.this_device->kobj),
				&(trapz_device_info.clear_buff_attr.attr));
	}
	if (!rc) {
		rc
		= sysfs_create_file(
			&(trapz_device_info.trapz_device.this_device->kobj),
				&(trapz_device_info.reset_attr.attr));
	}
	if (!rc) {
		rc
		= sysfs_create_file(
			&(trapz_device_info.trapz_device.this_device->kobj),
				&(trapz_device_info.enabled_attr.attr));
	}

	if (rc)
		printk(KERN_ERR "trapz: cannot create attributes(%d).\n", rc);

	return rc;
}

/**
 * Initialize the TRAPZ device.
 */
static int __init device_init(void)
{
	int rc = 0;

	trapz_device_info.trapz_device.minor = MISC_DYNAMIC_MINOR;
	trapz_device_info.trapz_device.name = TRAPZ_DEV_NAME;
	trapz_device_info.trapz_device.fops = &fops;
	trapz_device_info.trapz_device.parent = NULL;

	rc = misc_register(&trapz_device_info.trapz_device);

	if (rc)
		printk(KERN_ERR "trapz: cannot register device (%d).\n", rc);

	return rc;
}

static int __init trapz_init(void)
{
	int rc;

	/* Set global info to defaults */
	g_data.bufferSize = TRAPZ_DEFAULT_BUFFER_SIZE;
	g_data.pLimit = g_data.pHead = g_data.pTail = g_data.pBuffer = 0;
	g_data.os_comp_loglevels = g_data.app_comp_loglevels = 0;
	g_data.total = g_data.count = 0;

	init_waitqueue_head(&trapz_device_info.wq);
	spin_lock_init(&trapz_device_info.lock);

	rc = allocate_mem(3);
	if (!rc) {
		init_logbuffers(0xFF, 0xFF);
		if (!device_init() && !trapz_sysfs_init()) {
			atomic_set(&trapz_device_info.enabled, 1);
			printk(KERN_INFO "trapz: initialized.  trapz enabled\n");
		}
	}

	if (!rc)
		printk(KERN_INFO "trapz: initialization problem.  trapz disabled\n");

	return rc;
}

device_initcall(trapz_init);

MODULE_LICENSE("GPL v2");

#undef TRAPZ_CREATE_RO_ATTR
#undef TRAPZ_CREATE_RW_ATTR
