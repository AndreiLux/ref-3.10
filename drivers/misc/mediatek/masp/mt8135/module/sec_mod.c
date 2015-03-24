/******************************************************************************
 *  INCLUDE LINUX HEADER
 ******************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <mach/memory.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

/******************************************************************************
 *  INCLUDE LIBRARY
 ******************************************************************************/
#include <mach/sec_osal.h>
#include "sec_mod.h"
#ifdef MTK_SECURITY_MODULE_LITE
#include "masp_version.h"
#else
#include "sec_mod_core.h"
#endif
#include "sec_boot_core.h"

#define SEC_DEV_NAME                "sec"
#define SEC_MAJOR                   182
#define MOD                         "MASP"

#define TRACE_FUNC()                MSG_FUNC(SEC_DEV_NAME)

/**************************************************************************
 *  EXTERNAL VARIABLE
 **************************************************************************/

/*************************************************************************
 *  GLOBAL VARIABLE
 **************************************************************************/
static struct sec_mod sec = { 0 };

static struct cdev sec_dev;
static struct class *psSecClass;

/**************************************************************************
 *  SEC DRIVER OPEN
 **************************************************************************/
static int sec_open(struct inode *inode, struct file *file)
{
	return 0;
}

/**************************************************************************
 *  SEC DRIVER RELEASE
 **************************************************************************/
static int sec_release(struct inode *inode, struct file *file)
{
	return 0;
}

/**************************************************************************
 *  SEC DRIVER IOCTL
 **************************************************************************/
static long sec_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
#ifdef MTK_SECURITY_MODULE_LITE
	return -EIO;
#else
	return sec_core_ioctl(file, cmd, arg);
#endif
}

static const struct file_operations sec_fops = {
	.owner = THIS_MODULE,
	.open = sec_open,
	.release = sec_release,
	.write = NULL,
	.read = NULL,
	.unlocked_ioctl = sec_ioctl
};

/**************************************************************************
 *  SEC RID PROC FUNCTION
 **************************************************************************/
static int sec_proc_rid_show(struct seq_file *m, void *v)
{
	/* unsigned int rid[4] = {0}; */
	/* sec_get_random_id((unsigned int *)rid); */
	/* seq_printf(m,"%x%x%x%x",rid[0], rid[1], rid[2], rid[3]); */
	sec_get_random_id((unsigned int *)m);

	return 0;
}

static int sec_proc_rid_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_proc_rid_show, NULL);
}

static const struct file_operations sec_proc_rid_fops = {
	.open = sec_proc_rid_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};


/**************************************************************************
 *  SEC MODULE PARAMETER
 **************************************************************************/
static uint recovery_done;
module_param(recovery_done, uint, S_IRUSR | S_IWUSR /*|S_IWGRP */  | S_IRGRP | S_IROTH);	/* rw-r--r-- */
MODULE_PARM_DESC(recovery_done,
		 "A recovery sync parameter under sysfs (0=complete, 1=on-going, 2=error)");

/**************************************************************************
 *  SEC DRIVER INIT
 **************************************************************************/
static int sec_init(void)
{
	int ret = 0;
	dev_t id;
	struct device *psSecDev;

	id = MKDEV(SEC_MAJOR, 0);
	ret = register_chrdev_region(id, 1, SEC_DEV_NAME);

	if (ret) {
		pr_err("[%s] Regist Failed (%d)\n", SEC_DEV_NAME, ret);
		return ret;
	}

	cdev_init(&sec_dev, &sec_fops);
	sec_dev.owner = THIS_MODULE;

	ret = cdev_add(&sec_dev, id, 1);
	if (ret < 0)
		goto exit;

	sec.id = id;
	sec.init = 1;
	spin_lock_init(&sec.lock);

	psSecClass = class_create(THIS_MODULE, SEC_DEV_NAME);
	if (IS_ERR(psSecClass))
		goto class_fail;
	psSecDev = device_create(psSecClass, NULL, id,
				 NULL,
				 SEC_DEV_NAME);
	if (IS_ERR(psSecDev))
		goto class_fail;

	proc_create("rid", 0, NULL, &sec_proc_rid_fops);

#ifdef MTK_SECURITY_MODULE_LITE
	pr_info("[MASP Lite] version '%s%s', enter.\n", BUILD_TIME, BUILD_BRANCH);
#else
	sec_core_init();
#endif

	return ret;

 class_fail:
	ret = -1;
	if (!IS_ERR(psSecClass))
		class_destroy(psSecClass);

	cdev_del(&sec_dev);
 exit:
	if (ret != 0) {
		unregister_chrdev_region(id, 1);
		memset(&sec, 0, sizeof(sec));
	}

	return ret;
}


/**************************************************************************
 *  SEC DRIVER EXIT
 **************************************************************************/
static void sec_exit(void)
{
	device_destroy(psSecClass, MKDEV(SEC_MAJOR, 0));
	class_destroy(psSecClass);
	remove_proc_entry("rid", NULL);
	cdev_del(&sec_dev);
	unregister_chrdev_region(sec.id, 1);
	memset(&sec, 0, sizeof(sec));

#ifdef MTK_SECURITY_MODULE_LITE
	pr_info("[MASP Lite] version '%s%s', exit.\n", BUILD_TIME, BUILD_BRANCH);
#else
	sec_core_exit();
#endif
}
module_init(sec_init);
module_exit(sec_exit);

/**************************************************************************
 *  EXPORT FUNCTION
 **************************************************************************/
EXPORT_SYMBOL(sec_get_random_id);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
#ifdef MTK_SECURITY_MODULE_LITE
MODULE_DESCRIPTION("Mediatek Security Module Lite");
#else
MODULE_DESCRIPTION("Mediatek Security Module");
#endif
