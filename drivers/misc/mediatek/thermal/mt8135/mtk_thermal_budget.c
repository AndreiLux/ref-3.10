#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define NUM_COOLERS 3
#define MAXIMUM_CPU_POWER 4600

struct thermal_dev_t {
	int major, minor;
	int flag;
	dev_t devno;
	struct class *cls;
	struct mutex mutex;
	struct cdev cdev;
	wait_queue_head_t queue;
};

struct cool_dev_t {
	char name[THERMAL_NAME_LENGTH];
	int budget;
};

static struct thermal_dev_t *tdev;
static int _thermal_budget;
static struct cool_dev_t coolers[NUM_COOLERS] = {
	{"thermal_budget", MAXIMUM_CPU_POWER},
	{"cpu_adaptive_cooler", MAXIMUM_CPU_POWER},
	{"tmp103-cpu-cooler", MAXIMUM_CPU_POWER},
};

void thermal_budget_notify(int budget, char *type)
{
	int i = 0;
	int bud = 0;
	if (type == NULL)
		return;
	for (i = 0; i < NUM_COOLERS; i++) {
		if (!strcmp(type, coolers[i].name))
			coolers[i].budget = budget;
		if (coolers[i].budget <= 0)
			coolers[i].budget = MAXIMUM_CPU_POWER;

		bud = bud ? min(bud, coolers[i].budget) : coolers[i].budget;
	}
	_thermal_budget = bud;
	tdev->flag = 1;
	wake_up_interruptible(&tdev->queue);
}
int thermal_budget_get(void)
{
	return _thermal_budget;
}
static int _thermal_budget_open(struct inode *inod, struct file *filp)
{
	/* pr_notice("thermal_budget: device opened\n"); */
	return 0;
}

static ssize_t _thermal_budget_write(struct file *filp, const char __user *buf, size_t len,
				     loff_t *off)
{
	unsigned int budget = 0;
	int ret;

	ret = kstrtouint_from_user(buf, len, 10, &budget);
	thermal_budget_notify(budget, "thermal_budget");
	return len;
}

static ssize_t _thermal_budget_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	char budget[30];
	int s;

	wait_event_interruptible(tdev->queue, (tdev->flag == 1));
	tdev->flag = 0;
	s = snprintf(budget, 30, "%d\n", _thermal_budget);
	if (s > len)
		s = len;
	if (copy_to_user(buf, budget, s))
		return -EFAULT;
	/* pr_notice("thermal_budget: device read\n"); */
	return s;
}

static unsigned int _thermal_budget_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	/* pr_notice("thermal_budget: device polled\n"); */
	poll_wait(filp, &tdev->queue, wait);
	if (tdev->flag == 1)
		mask = POLLIN | POLLRDNORM;
	return mask;
}

static int _thermal_budget_release(struct inode *inod, struct file *filp)
{
	/* pr_notice("thermal_budget: device closed\n"); */
	return 0;
}

static int _thermal_budget_show_(struct seq_file *s, void *v)
{
	seq_printf(s, "%d\n", _thermal_budget);
	return 0;
}

static int _thermal_budget_open_(struct inode *inode, struct file *file)
{
	return single_open(file, _thermal_budget_show_, NULL);
}

static const struct file_operations _thermal_fops = {
	.owner = THIS_MODULE,
	.open = _thermal_budget_open,
	.read = _thermal_budget_read,
	.write = _thermal_budget_write,
	.poll = _thermal_budget_poll,
	.release = _thermal_budget_release,
};

static const struct file_operations thermal_budget_fops = {
	.owner      = THIS_MODULE,
	.open       = _thermal_budget_open_,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static int __init thermal_budget_init(void)
{
	int result;
	struct proc_dir_entry *thermal_entry = NULL;
	struct proc_dir_entry *thermal_dir = NULL;

	tdev = kzalloc(sizeof(struct thermal_dev_t), GFP_KERNEL);
	result = alloc_chrdev_region(&tdev->devno, tdev->minor, 1, "thermal_budget");
	tdev->major = MAJOR(tdev->devno);
	tdev->minor = MINOR(tdev->devno);

	cdev_init(&tdev->cdev, &_thermal_fops);
	tdev->cdev.owner = THIS_MODULE;
	tdev->cdev.ops = &_thermal_fops;
	mutex_init(&tdev->mutex);
	init_waitqueue_head(&tdev->queue);
	result = cdev_add(&tdev->cdev, tdev->devno, 1);

	if (result) {
		pr_notice("thermal_budget: Error %d adding thermal_budget\n", result);
		return result;
	}
	pr_info("thermal_budget: %d, %d\n", tdev->major, tdev->minor);

	tdev->cls = class_create(THIS_MODULE, "thermal_notify");
	if (IS_ERR(tdev->cls)) {
		pr_alert("thermal_budget: failed in creating class\n");
		return -1;
	}
	device_create(tdev->cls, NULL, tdev->devno, NULL, "thermal_budget");

	thermal_dir = proc_mkdir("thermal", NULL);
	if (!thermal_dir) {
		pr_err("[%s]: mkdir /proc/thermal failed\n", __func__);
	} else {
		thermal_entry = proc_create("thermal_budget",
					    S_IRUGO | S_IWUSR | S_IWGRP,
					    thermal_dir,
					    &thermal_budget_fops);
		if (!thermal_entry)
			pr_err("[%s]: failed thermal_budget procfs\n",
			       __func__);
	}
	return result;
}

static void __exit thermal_budget_exit(void)
{
	cdev_del(&tdev->cdev);
	unregister_chrdev_region(tdev->devno, 1);

	device_destroy(tdev->cls, tdev->devno);
	class_destroy(tdev->cls);

	kfree(tdev);
}
module_init(thermal_budget_init);
module_exit(thermal_budget_exit);
