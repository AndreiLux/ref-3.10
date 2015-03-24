/*****************************************************************************
 *
 * Filename:
 * ---------
 *    mt_battery_protect.c
 *
 * Description:
 * ------------
 *   This Module defines functions of battery protect algorithm
 *   for updating the CPU power budget
 *
 ****************************************************************************/
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/xlog.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <mach/charging.h>
#include "mt_battery_protect.h"
#include <mach/pmic_mt6397_sw.h>
/* #include "cust_battery_throttle.h" */

static int g_test_flag;
static int g_test_budget = BATTERY_MAX_BUDGET;
static int g_power_budget = BATTERY_MAX_BUDGET;
static battery_throttle_dev_t *tdev;

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Internal API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static void battery_budget_notify(void)
{
	tdev->flag = 1;
	wake_up_interruptible(&tdev->queue);
}

static void lowPowerCB(LOW_BATTERY_LEVEL lev)
{
	switch (lev) {
	case LOW_BATTERY_LEVEL_0:
		g_power_budget = 4200;
		break;

	case LOW_BATTERY_LEVEL_1:
		g_power_budget = 1700;
		break;

	case LOW_BATTERY_LEVEL_2:
	default:
		g_power_budget = 430;
		break;
	}

	battery_budget_notify();

	battery_xlog_printk(BAT_LOG_FULL, "battery_budget: level->%d, lowPowerCB->%d\n", lev,
			    g_power_budget);
}

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // fop API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static int _battery_budget_open(struct inode *inod, struct file *filp)
{
	battery_xlog_printk(BAT_LOG_FULL, "battery_budget: device opened\n");
	return 0;
}

static ssize_t _battery_budget_write(struct file *filp, const char __user *buf, size_t len,
				     loff_t *off)
{
	int budget = 0;
	int ret;

	ret = kstrtoint_from_user(buf, len, 10, &budget);

	if (budget < 0) {
		g_test_flag = 0x0;
	} else {
		/* For Test CMD */
		g_test_flag = 0x1;
		g_test_budget = budget;
	}

	battery_budget_notify();

	battery_xlog_printk(BAT_LOG_FULL, "battery_budget: device write, %d\n", budget);
	return len;
}

static ssize_t _battery_budget_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	char budget[30];
	int s;

	wait_event_interruptible(tdev->queue, (1 == tdev->flag));

	tdev->flag = 0;

	if (g_test_flag)
		s = snprintf(budget, 30, "%d\n", g_test_budget);
	else
		s = snprintf(budget, 30, "%d\n", g_power_budget);

	if (s > len)
		s = len;

	if (copy_to_user(buf, budget, s))
		return -EFAULT;

	battery_xlog_printk(BAT_LOG_FULL, "battery_budget: device read, CMD: %d, budget: %s\n",
			    g_test_flag, budget);

	return s;
}

static unsigned int _battery_budget_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	battery_xlog_printk(BAT_LOG_FULL, "battery_budget: device polled\n");

	poll_wait(filp, &tdev->queue, wait);
	if (tdev->flag == 1)
		mask = POLLIN | POLLRDNORM;
	return mask;
}

static int _battery_budget_release(struct inode *inod, struct file *filp)
{
	battery_xlog_printk(BAT_LOG_FULL, "battery_budget: device closed\n");
	return 0;
}

static const struct file_operations _battery_budget_fops = {
	.owner = THIS_MODULE,
	.open = _battery_budget_open,
	.read = _battery_budget_read,
	.write = _battery_budget_write,
	.poll = _battery_budget_poll,
	.release = _battery_budget_release,
};

static void _setup_battery_budget_device(void)
{
	int result = 0;

	tdev = kzalloc(sizeof(battery_throttle_dev_t), GFP_KERNEL);
	result = alloc_chrdev_region(&tdev->devno, tdev->minor, 1, BATTERY_BUDGET);
	tdev->major = MAJOR(tdev->devno);
	tdev->minor = MINOR(tdev->devno);

	cdev_init(&tdev->cdev, &_battery_budget_fops);
	tdev->cdev.owner = THIS_MODULE;
	tdev->cdev.ops = &_battery_budget_fops;
	init_waitqueue_head(&tdev->queue);
	result = cdev_add(&tdev->cdev, tdev->devno, 1);

	if (result)
		battery_xlog_printk(BAT_LOG_CRTI,
				    "battery_budget: Error %d adding battery_budget\n", result);

	battery_xlog_printk(BAT_LOG_FULL, "battery_budget_late_init: %d, %d\n", tdev->major,
			    tdev->minor);

	tdev->cls = class_create(THIS_MODULE, BATTERY_NOTIFY);
	if (IS_ERR(tdev->cls))
		battery_xlog_printk(BAT_LOG_CRTI, "battery_budget: failed in creating class\n");

	device_create(tdev->cls, NULL, tdev->devno, NULL, BATTERY_BUDGET);
}

static int mt_battery_throttle_probe(struct platform_device *dev)
{
	_setup_battery_budget_device();

	register_low_battery_notify(&lowPowerCB, LOW_BATTERY_PRIO_CPU);

	low_battery_protect_enable();

	battery_xlog_printk(BAT_LOG_CRTI, "****[mt_battery_throttle_probe] register Done\n");

	return 0;
}

struct platform_device mt_battery_throttle_device = {
	.name = "batteryThrottle-user",
	.id = -1,
};

static struct platform_driver mt_battery_throttle_driver = {
	.probe = mt_battery_throttle_probe,
	.driver = {
		   .name = "batteryThrottle-user",
		   },
};

static int __init battery_budget_init(void)
{
	int ret;

	ret = platform_device_register(&mt_battery_throttle_device);
	if (ret) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "****[battery_budget_init] Unable to device register(%d)\n",
				    ret);
		return ret;
	}

	ret = platform_driver_register(&mt_battery_throttle_driver);
	if (ret) {
		battery_xlog_printk(BAT_LOG_CRTI,
				    "****[battery_budget_init] Unable to register driver (%d)\n",
				    ret);
		return ret;
	}

	return 0;
}

static void __exit battery_budget_exit(void)
{
	cdev_del(&tdev->cdev);
	unregister_chrdev_region(tdev->devno, 1);

	device_destroy(tdev->cls, tdev->devno);
	class_destroy(tdev->cls);

	battery_xlog_printk(BAT_LOG_CRTI, "battery_budget exit\n");

	if (tdev)
		kfree(tdev);
}
module_init(battery_budget_init);
module_exit(battery_budget_exit);
