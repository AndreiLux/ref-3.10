/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/types.h>

#include <linux/regulator/consumer.h>

#include <linux/of.h>
#include "fpc1020.h"
#include "fpc1020_common.h"
#include "fpc1020_regs.h"
#include "fpc1020_capture.h"

#ifdef CONFIG_INPUT_FPC1020_NAV
#include "fpc1020_input.h"
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fingerprint Cards AB <tech@fingerprints.com>");
MODULE_DESCRIPTION("FPC1020 touch sensor driver.");


#define FPC1020_IRQ_GPIO (201)
#define FPC1020_RESET_GPIO (163)
#define FPC1020_CS_GPIO (66)
#define FPC1020_MODULE_ID_GPIO (72)
static struct fpc1020_platform_data fpc1020_pdata_default = {
	.irq_gpio = FPC1020_IRQ_GPIO,
	.reset_gpio = FPC1020_RESET_GPIO,
	.cs_gpio = FPC1020_CS_GPIO,
	.moduleID_gpio = FPC1020_MODULE_ID_GPIO,
};
/* -------------------------------------------------------------------- */
/* fpc1020 sensor commands and registers				*/
/* -------------------------------------------------------------------- */

typedef enum {
	FPC_1020_ERROR_REG_BIT_FIFO_UNDERFLOW = 1 << 0
} fpc1020_error_reg_t;



/* -------------------------------------------------------------------- */
/* global variables							*/
/* -------------------------------------------------------------------- */
static int fpc1020_device_count;


/* -------------------------------------------------------------------- */
/* fpc1020 data types							*/
/* -------------------------------------------------------------------- */
struct fpc1020_attribute {
	struct device_attribute attr;
	size_t offset;
};

enum {
	FPC1020_WORKER_IDLE_MODE = 0,
	FPC1020_WORKER_CAPTURE_MODE,
	FPC1020_WORKER_INPUT_MODE,
	FPC1020_WORKER_WAKEUP_MODE,
	FPC1020_WORKER_FINGER_DETECT_MODE,
	FPC1020_WORKER_EXIT
};

enum {
	FPC1020_FINGER_DETECT_SUCESS = 1,
	FPC1020_FINGER_DETECT_WORKING,
};

enum {
	FPC1020_FINGER_DETECT_BEGIN = 1,
	FPC1020_FINGER_DETECT_CANCLE,
};


/* -------------------------------------------------------------------- */
/* fpc1020 driver constants						*/
/* -------------------------------------------------------------------- */
#define FPC1020_CLASS_NAME				"fpsensor"
#define FPC1020_WORKER_THREAD_NAME		"fpc1020_worker"

#define FPC1020_MAJOR					235


/* -------------------------------------------------------------------- */
/* function prototypes							*/
/* -------------------------------------------------------------------- */
static int __init fpc1020_init(void);

static void __exit fpc1020_exit(void);

static int fpc1020_probe(struct spi_device *spi);

static int fpc1020_remove(struct spi_device *spi);

static int fpc1020_suspend(struct device *dev);

static int fpc1020_resume(struct device *dev);

static int fpc1020_open(struct inode *inode, struct file *file);

static ssize_t fpc1020_write(struct file *file, const char *buff,
					size_t count, loff_t *ppos);

static ssize_t fpc1020_read(struct file *file, char *buff,
				size_t count, loff_t *ppos);

static int fpc1020_release(struct inode *inode, struct file *file);

static unsigned int fpc1020_poll(struct file *file, poll_table *wait);

static int fpc1020_cleanup(fpc1020_data_t *fpc1020, struct spi_device *spidev);

static int fpc1020_reset_init(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata);

static int fpc1020_irq_init(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata);

static int fpc1020_spi_setup(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata);

static int fpc1020_worker_init(fpc1020_data_t *fpc1020);

static int fpc1020_worker_destroy(fpc1020_data_t *fpc1020);

static int fpc1020_create_class(fpc1020_data_t *fpc1020);

static int fpc1020_create_device(fpc1020_data_t *fpc1020);

static int fpc1020_manage_sysfs(fpc1020_data_t *fpc1020,
				struct spi_device *spi, bool create);

irqreturn_t fpc1020_interrupt(int irq, void *_fpc1020);

static ssize_t fpc1020_show_attr_setup(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t fpc1020_store_attr_setup(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count);

static ssize_t fpc1020_show_attr_diag(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t fpc1020_store_attr_diag(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count);

static u8 fpc1020_selftest_short(fpc1020_data_t *fpc1020);
static u8 fpc1020_fingerdetect(fpc1020_data_t *fpc1020);

static int fpc1020_start_capture(fpc1020_data_t *fpc1020);

static void fpc1020_new_job(fpc1020_data_t *fpc1020, int new_job);

static void fpc1020_worker_goto_idle(fpc1020_data_t *fpc1020);

static int fpc1020_worker_function(void *_fpc1020);

#ifdef CONFIG_INPUT_FPC1020_NAV
static void fpc1020_start_navigation(fpc1020_data_t *fpc1020);
#endif

#ifdef CONFIG_INPUT_FPC1020_WAKEUP
static void fpc1020_start_wakeup(fpc1020_data_t *fpc1020);
#endif

/* -------------------------------------------------------------------- */
/* External interface							*/
/* -------------------------------------------------------------------- */
module_init(fpc1020_init);
module_exit(fpc1020_exit);

static const struct dev_pm_ops fpc1020_pm = {
	.suspend = fpc1020_suspend,
	.resume = fpc1020_resume
};

#ifdef CONFIG_OF
static struct of_device_id fpc1020_of_match[] = {
	{ .compatible = "fpc,fpc1020", },
	{}
};

MODULE_DEVICE_TABLE(of, fpc1020_of_match);
#endif

static struct spi_driver fpc1020_driver = {
	.driver = {
		.name	= FPC1020_DEV_NAME,
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
		.pm     = &fpc1020_pm,
#ifdef CONFIG_OF
		.of_match_table = fpc1020_of_match,
#endif
	},
	.probe	= fpc1020_probe,
	.remove	= fpc1020_remove,
};

static const struct file_operations fpc1020_fops = {
	.owner          = THIS_MODULE,
	.open           = fpc1020_open,
	.write          = fpc1020_write,
	.read           = fpc1020_read,
	.release        = fpc1020_release,
	.poll           = fpc1020_poll,
};


/* -------------------------------------------------------------------- */
/* devfs								*/
/* -------------------------------------------------------------------- */
#define FPC1020_ATTR(__grp, __field, __mode)				\
{									\
	.attr = __ATTR(__field, (__mode),				\
	fpc1020_show_attr_##__grp,					\
	fpc1020_store_attr_##__grp),					\
	.offset = offsetof(struct fpc1020_##__grp, __field)		\
}

#define FPC1020_DEV_ATTR(_grp, _field, _mode)				\
struct fpc1020_attribute fpc1020_attr_##_field =			\
					FPC1020_ATTR(_grp, _field, (_mode))

#define DEVFS_SETUP_MODE (S_IWUSR|S_IWGRP|S_IWOTH|S_IRUSR|S_IRGRP|S_IROTH)

static FPC1020_DEV_ATTR(setup, adc_gain,		DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, adc_shift,		DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_mode,		DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_count,		DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_settings_mux,	DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, pxl_ctrl,		DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_row_start,	DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_row_count,	DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_col_start,	DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_col_groups,	DEVFS_SETUP_MODE);

static struct attribute *fpc1020_setup_attrs[] = {
	&fpc1020_attr_adc_gain.attr.attr,
	&fpc1020_attr_adc_shift.attr.attr,
	&fpc1020_attr_capture_mode.attr.attr,
	&fpc1020_attr_capture_count.attr.attr,
	&fpc1020_attr_capture_settings_mux.attr.attr,
	&fpc1020_attr_pxl_ctrl.attr.attr,
	&fpc1020_attr_capture_row_start.attr.attr,
	&fpc1020_attr_capture_row_count.attr.attr,
	&fpc1020_attr_capture_col_start.attr.attr,
	&fpc1020_attr_capture_col_groups.attr.attr,
	NULL
};

static const struct attribute_group fpc1020_setup_attr_group = {
	.attrs = fpc1020_setup_attrs,
	.name = "setup"
};

#define DEVFS_DIAG_MODE_RO (S_IRUSR|S_IRGRP|S_IROTH)
#define DEVFS_DIAG_MODE_RW (S_IWUSR|S_IWGRP|S_IWOTH|S_IRUSR|S_IRGRP|S_IROTH)

static FPC1020_DEV_ATTR(diag, chip_id,		DEVFS_DIAG_MODE_RO);
static FPC1020_DEV_ATTR(diag, selftest,		DEVFS_DIAG_MODE_RO);
static FPC1020_DEV_ATTR(diag, spi_register,	DEVFS_DIAG_MODE_RW);
static FPC1020_DEV_ATTR(diag, spi_regsize,	DEVFS_DIAG_MODE_RO);
static FPC1020_DEV_ATTR(diag, spi_data ,	DEVFS_DIAG_MODE_RW);
static FPC1020_DEV_ATTR(diag, fingerdetect ,	DEVFS_DIAG_MODE_RW);
static FPC1020_DEV_ATTR(diag, navigation_enable ,	DEVFS_DIAG_MODE_RW);
static FPC1020_DEV_ATTR(diag, wakeup_enable ,	 DEVFS_DIAG_MODE_RW);

static struct attribute *fpc1020_diag_attrs[] = {
	&fpc1020_attr_chip_id.attr.attr,
	&fpc1020_attr_selftest.attr.attr,
	&fpc1020_attr_spi_register.attr.attr,
	&fpc1020_attr_spi_regsize.attr.attr,
	&fpc1020_attr_spi_data.attr.attr,
	&fpc1020_attr_fingerdetect.attr.attr,
	&fpc1020_attr_navigation_enable.attr.attr,
	&fpc1020_attr_wakeup_enable.attr.attr,
	NULL
};

static const struct attribute_group fpc1020_diag_attr_group = {
	.attrs = fpc1020_diag_attrs,
	.name = "diag"
};


/* -------------------------------------------------------------------- */
/* SPI debug interface, prototypes					*/
/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_select(fpc1020_data_t *fpc1020,
				fpc1020_reg_t reg);

static int fpc1020_spi_debug_value_write(fpc1020_data_t *fpc1020, u64 data);

static int fpc1020_spi_debug_buffer_write(fpc1020_data_t *fpc1020,
					const char *data,
					size_t count);

static int fpc1020_spi_debug_value_read(fpc1020_data_t *fpc1020,
					u64 *data);

static int fpc1020_spi_debug_buffer_read(fpc1020_data_t *fpc1020,
					u8 *data,
					size_t max_count);

static void fpc1020_spi_debug_buffer_to_hex_string(char *string,
						u8 *buffer,
						size_t bytes);

static int fpc1020_spi_debug_hex_string_to_buffer(u8 *buffer,
						size_t buf_size,
						const char *string,
						size_t chars);

/* -------------------------------------------------------------------- */
/* function definitions							*/
/* -------------------------------------------------------------------- */
static int __init fpc1020_init(void)
{
	if (spi_register_driver(&fpc1020_driver))
		return -EINVAL;

	return 0;
}


/* -------------------------------------------------------------------- */
static void __exit fpc1020_exit(void)
{
	printk(KERN_INFO "%s\n", __func__);

	spi_unregister_driver(&fpc1020_driver);
}
/* -------------------------------------------------------------------- */

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	int error = 0;
	int i;
	struct fb_event *fb_event = data;
	int *blank = fb_event->data;
	fpc1020_data_t *fpc1020 = container_of(self, fpc1020_data_t, fb_notify);

	dev_dbg(&fpc1020->spi->dev,"fb_notifier_callback !\n");
	for (i = 0 ; i < FB_MAX; i++) {
		if (registered_fb[i] == fb_event->info) {
			if (i == 0) {
				dev_dbg(&fpc1020->spi->dev,"fingerprint-index:%d, go on !\n", i);
				break;
			} else {
				dev_dbg(&fpc1020->spi->dev,"fingerprint-index:%d, exit !\n", i);
				return error;
			}
		}
	}

	if (*blank == FB_BLANK_UNBLANK) {
			switch(event) {
			case FB_EARLY_EVENT_BLANK:
				atomic_set(&fpc1020->state, fp_LCD_UNBLANK);
				dev_dbg(&fpc1020->spi->dev,"fb_notifier_callback FB_EARLY_EVENT_BLANK resume: event = %lu\n", event);
				break;
			case FB_EVENT_BLANK:

				dev_dbg(&fpc1020->spi->dev,"fb_notifier_callback FB_EVENT_BLANK resume: event = %lu\n", event);
				break;
			default:
				dev_dbg(&fpc1020->spi->dev,"resume: event = %lu, not care\n", event);
				break;
			}
	} else  if (*blank == FB_BLANK_POWERDOWN){

			/*suspend device*/
			switch(event) {
			case FB_EARLY_EVENT_BLANK:
				atomic_set(&fpc1020->state, fp_LCD_POWEROFF);
				fpc1020_start_wakeup(fpc1020);

				dev_dbg(&fpc1020->spi->dev,"fb_notifier_callback FB_EARLY_EVENT_BLANK suspend: event = %lu\n", event);
				if (error)
					dev_err(&fpc1020->spi->dev,"ts suspend device err\n");
				break;
			case FB_EVENT_BLANK:
				dev_dbg(&fpc1020->spi->dev,"fb_notifier_callback FB_EVENT_BLANK suspend: event = %lu\n", event);
				break;
			default:
				dev_dbg(&fpc1020->spi->dev,"suspend: event = %lu, not care\n", event);
				break;
			}
	} else {
		dev_dbg(&fpc1020->spi->dev,"blank: %d, not care\n", *blank);
	}

	return 0;
}
#endif
/* -------------------------------------------------------------------- */
static int fpc1020_probe(struct spi_device *spi)
{
	struct fpc1020_platform_data *fpc1020_pdata;
	int error = 0;
	fpc1020_data_t *fpc1020 = NULL;
	int buffer_order;

	fpc1020 = kzalloc(sizeof(*fpc1020), GFP_KERNEL);
	if (!fpc1020) {
		dev_err(&spi->dev,
		"failed to allocate memory for struct fpc1020_data\n");

		return -ENOMEM;
	}

	printk(KERN_INFO "%s\n", __func__);

	buffer_order = get_order(FPC1020_IMAGE_BUFFER_SIZE);
	fpc1020->huge_buffer = (u8 *)__get_free_pages(GFP_KERNEL, buffer_order);
	fpc1020->huge_buffer_size = (fpc1020->huge_buffer) ?
					(size_t)PAGE_SIZE << buffer_order : 0;

	if (fpc1020->huge_buffer_size == 0) {
		error = -ENOMEM;
		dev_err(&fpc1020->spi->dev, "failed to get free pages error=%d\n",error);
		goto err;
	}

	spi_set_drvdata(spi, fpc1020);
	fpc1020->spi = spi;

	fpc1020->reset_gpio = -EINVAL;
	fpc1020->irq_gpio   = -EINVAL;
	fpc1020->cs_gpio    = -EINVAL;

	fpc1020->irq        = -EINVAL;
	atomic_set(&fpc1020->state, fp_UNINIT);
	atomic_set(&fpc1020->taskstate, fp_UNINIT);
	init_waitqueue_head(&fpc1020->wq_irq_return);

	error = fpc1020_init_capture(fpc1020);
	if (error)
		goto err;

	//fpc1020_pdata = spi->dev.platform_data;
	fpc1020_pdata = &fpc1020_pdata_default;

#if defined(CONFIG_FB)
	fpc1020->fb_notify.notifier_call = NULL;
#endif
	error = fpc1020_reset_init(fpc1020, fpc1020_pdata);
	if (error)
		goto err;

	error = fpc1020_irq_init(fpc1020, fpc1020_pdata);
	if (error)
		goto err;

	error = fpc1020_spi_setup(fpc1020, fpc1020_pdata);
	if (error)
		goto err;

	error = fpc1020_reset(fpc1020);
	if (error)
		goto err;

	error = fpc1020_check_hw_id(fpc1020);
	if (error)
		goto err;

	error = fpc1020_setup_defaults(fpc1020);
	if (error)
		goto err;

	error = fpc1020_create_class(fpc1020);
	if (error)
		goto err;

	error = fpc1020_create_device(fpc1020);
	if (error)
		goto err;

	sema_init(&fpc1020->mutex, 0);

	error = fpc1020_manage_sysfs(fpc1020, spi, true);
	if (error)
		goto err;

	error = register_chrdev_region(fpc1020->devno, 1, FPC1020_DEV_NAME);

	if (error) {
		dev_err(&fpc1020->spi->dev,
		  "register_chrdev_region failed.\n");

		goto err_sysfs;
	}

	cdev_init(&fpc1020->cdev, &fpc1020_fops);
	fpc1020->cdev.owner = THIS_MODULE;

	error = cdev_add(&fpc1020->cdev, fpc1020->devno, 1);
	if (error) {
		dev_err(&fpc1020->spi->dev, "cdev_add failed.\n");
		goto err_chrdev;
	}

	error = fpc1020_worker_init(fpc1020);
	if (error)
		goto err_cdev;

#ifdef CONFIG_INPUT_FPC1020_NAV
	error = fpc1020_input_init(fpc1020);
	if (error)
		goto err_cdev;

#else

	error = fpc1020_sleep(fpc1020, true);
	if (error)
		goto err_cdev;
#endif
	wake_lock_init(&fpc1020->fp_wake_lock, WAKE_LOCK_SUSPEND, "fingerprint_wakelock");
	up(&fpc1020->mutex);

	return 0;

err_cdev:
	cdev_del(&fpc1020->cdev);

err_chrdev:
	unregister_chrdev_region(fpc1020->devno, 1);

err_sysfs:
	fpc1020_manage_sysfs(fpc1020, spi, false);

err:
	fpc1020_cleanup(fpc1020, spi);
	return error;
}


/* -------------------------------------------------------------------- */
static int  fpc1020_remove(struct spi_device *spi)
{
	fpc1020_data_t *fpc1020 = spi_get_drvdata(spi);

	printk(KERN_INFO "%s\n", __func__);
	wake_lock_destroy(&fpc1020->fp_wake_lock);
	fpc1020_manage_sysfs(fpc1020, spi, false);

	fpc1020_sleep(fpc1020, true);

	cdev_del(&fpc1020->cdev);

	unregister_chrdev_region(fpc1020->devno, 1);

	fpc1020_cleanup(fpc1020, spi);

	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_suspend(struct device *dev)
{
	fpc1020_data_t *fpc1020 = dev_get_drvdata(dev);

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);
#if defined(CONFIG_FB)
	fpc1020_start_wakeup(fpc1020);
#else
	fpc1020_sleep(fpc1020, true);
#endif
	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_resume(struct device *dev)
{
	fpc1020_data_t *fpc1020 = dev_get_drvdata(dev);

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);
#if defined(CONFIG_FB)

#else
	fpc1020_wake_up(fpc1020);
	fpc1020->setup.capture_mode = FPC1020_MODE_IDLE;
#endif

	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_open(struct inode *inode, struct file *file)

{
	fpc1020_data_t *fpc1020;

	printk(KERN_INFO "%s\n", __func__);

	fpc1020 = container_of(inode->i_cdev, fpc1020_data_t, cdev);

	if(fp_CAPTURE== atomic_read(&fpc1020->taskstate)){
		dev_err(&fpc1020->spi->dev,
			"%s capturing return\n", __func__);
		return 0;
	}
	
	atomic_set(&fpc1020->taskstate, fp_CAPTURE);
	
	if (down_interruptible(&fpc1020->mutex))
		return -ERESTARTSYS;

	file->private_data = fpc1020;

	up(&fpc1020->mutex);

	return 0;
}


/* -------------------------------------------------------------------- */
static ssize_t fpc1020_write(struct file *file, const char *buff,
					size_t count, loff_t *ppos)
{
	printk(KERN_INFO "%s\n", __func__);

	return -ENOTTY;
}


/* -------------------------------------------------------------------- */
static ssize_t fpc1020_read(struct file *file, char *buff,
				size_t count, loff_t *ppos)
{
	fpc1020_data_t *fpc1020 = file->private_data;
	int error = 0;
	u32 max_data;
	u32 avail_data;

	if (down_interruptible(&fpc1020->mutex))
		return -ERESTARTSYS;

	if (fpc1020->capture.available_bytes > 0) {
		goto copy_data;
	} else {

		if (fpc1020->capture.read_pending_eof) {
			fpc1020->capture.read_pending_eof = false;
			error = 0;
			goto out;
		}

		if (file->f_flags & O_NONBLOCK) {
			if (fpc1020_capture_check_ready(fpc1020)) {
				error = fpc1020_start_capture(fpc1020);
				if (error)
					goto out;
			}

			error = -EWOULDBLOCK;
			goto out;

		} else {
			error = fpc1020_start_capture(fpc1020);
			if (error)
				goto out;
		}
	}

	error = wait_event_interruptible(
			fpc1020->capture.wq_data_avail,
			(fpc1020->capture.available_bytes > 0));

	if (error)
		goto out;

	if (fpc1020->capture.last_error != 0) {
		error = fpc1020->capture.last_error;
		goto out;
	}

copy_data:
	avail_data = fpc1020->capture.available_bytes;
	max_data = (count > avail_data) ? avail_data : count;

	if (max_data) {
		error = copy_to_user(buff,
			&fpc1020->huge_buffer[fpc1020->capture.read_offset],
			max_data);

		if (error)
			goto out;

		fpc1020->capture.read_offset += max_data;
		fpc1020->capture.available_bytes -= max_data;

		error = max_data;

		if (fpc1020->capture.available_bytes == 0)
			fpc1020->capture.read_pending_eof = true;
	}

out:
	up(&fpc1020->mutex);

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_release(struct inode *inode, struct file *file)
{
	fpc1020_data_t *fpc1020 = file->private_data;
	int status = 0;

	printk(KERN_INFO "%s\n", __func__);

	if (down_interruptible(&fpc1020->mutex))
		return -ERESTARTSYS;

	fpc1020_worker_goto_idle(fpc1020);
	if(fp_LCD_POWEROFF == atomic_read(&fpc1020->state)){
		dev_err(&fpc1020->spi->dev,
			"%s fp_LCD_POWEROFF startwakeup \n", __func__);
		fpc1020_start_wakeup(fpc1020);
	}

	up(&fpc1020->mutex);

	return status;
}


/* -------------------------------------------------------------------- */
static unsigned int fpc1020_poll(struct file *file, poll_table *wait)
{
	fpc1020_data_t *fpc1020 = file->private_data;
	unsigned int ret = 0;
	fpc1020_capture_mode_t mode = fpc1020->setup.capture_mode;
	bool blocking_op;

	if (down_interruptible(&fpc1020->mutex))
		return POLLHUP;

	if (fpc1020->capture.available_bytes > 0)
		ret |= (POLLIN | POLLRDNORM);
	else if (fpc1020->capture.read_pending_eof)
		ret |= POLLHUP;
	else { /* available_bytes == 0 && !pending_eof */

		blocking_op =
			(mode == FPC1020_MODE_WAIT_AND_CAPTURE || mode == FPC1020_MODE_WAIT_FINGER_UP_AND_CAPTURE) ? true : false;

		switch (fpc1020->capture.state) {
		case FPC1020_CAPTURE_STATE_IDLE:
			if (!blocking_op)
				ret |= POLLIN;
			break;

		case FPC1020_CAPTURE_STATE_STARTED:
		case FPC1020_CAPTURE_STATE_PENDING:
		case FPC1020_CAPTURE_STATE_WRITE_SETTINGS:
		case FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_DOWN:
		case FPC1020_CAPTURE_STATE_ACQUIRE:
		case FPC1020_CAPTURE_STATE_FETCH:
		case FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_UP:
		case FPC1020_CAPTURE_STATE_COMPLETED:
			ret |= POLLIN;

			poll_wait(file, &fpc1020->capture.wq_data_avail, wait);

			if (fpc1020->capture.available_bytes > 0)
				ret |= POLLRDNORM;
			else if (blocking_op)
				ret = 0;

			break;

		case FPC1020_CAPTURE_STATE_FAILED:
			if (!blocking_op)
				ret |= POLLIN;
			break;

		default:
			dev_err(&fpc1020->spi->dev,
				"%s unknown state\n", __func__);
			break;
		}
	}

	up(&fpc1020->mutex);

	return ret;
}


/* -------------------------------------------------------------------- */
static int fpc1020_cleanup(fpc1020_data_t *fpc1020, struct spi_device *spidev)
{
	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020_worker_destroy(fpc1020);

	if (!IS_ERR_OR_NULL(fpc1020->device))
		device_destroy(fpc1020->class, fpc1020->devno);

	class_destroy(fpc1020->class);

	if (fpc1020->irq >= 0)
		free_irq(fpc1020->irq, fpc1020);

	if (gpio_is_valid(fpc1020->irq_gpio))
		gpio_free(fpc1020->irq_gpio);

	if (gpio_is_valid(fpc1020->reset_gpio))
		gpio_free(fpc1020->reset_gpio);

	if (gpio_is_valid(fpc1020->cs_gpio))
		gpio_free(fpc1020->cs_gpio);

	if (gpio_is_valid(fpc1020->moduleID_gpio))
		gpio_free(fpc1020->moduleID_gpio);

	if (fpc1020->huge_buffer) {
		free_pages((unsigned long)fpc1020->huge_buffer,
			   get_order(FPC1020_IMAGE_BUFFER_SIZE));
	}

#ifdef CONFIG_INPUT_FPC1020_NAV
	fpc1020_input_destroy(fpc1020);
#endif


	kfree(fpc1020);

	spi_set_drvdata(spidev, NULL);

	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_reset_init(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata)
{
	int error = 0;

	if (gpio_is_valid(pdata->reset_gpio)) {

		dev_info(&fpc1020->spi->dev,
			"Assign HW reset -> GPIO%d\n", pdata->reset_gpio);

		fpc1020->soft_reset_enabled = false;

		error = gpio_request(pdata->reset_gpio, "fpc1020_reset");

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"gpio_request (reset) failed.\n");
			return error;
		}

		fpc1020->reset_gpio = pdata->reset_gpio;

		error = gpio_direction_output(fpc1020->reset_gpio, 1);

		if (error) {
			dev_err(&fpc1020->spi->dev,
			"gpio_direction_output(reset) failed.\n");
			return error;
		}
	} else {
		dev_info(&fpc1020->spi->dev, "Using soft reset\n");

		fpc1020->soft_reset_enabled = true;
	}

	if (gpio_is_valid(pdata->moduleID_gpio)) {

		dev_info(&fpc1020->spi->dev,
			"Assign moduleID -> GPIO%d\n",
			pdata->moduleID_gpio);

		error = gpio_request(pdata->moduleID_gpio, "fingerprint_moduleID_gpio");

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"gpio_request (moduleID_gpio) failed.\n");

			return 0;
		}

		fpc1020->moduleID_gpio = pdata->moduleID_gpio;

		error = gpio_direction_input(fpc1020->moduleID_gpio);

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"gpio_direction_input (moduleID_gpio) failed.\n");
			return 0;
		}
	} 

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_irq_init(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata)
{
	int error = 0;

	if (gpio_is_valid(pdata->irq_gpio)) {

		dev_info(&fpc1020->spi->dev,
			"Assign IRQ -> GPIO%d\n",
			pdata->irq_gpio);

		error = gpio_request(pdata->irq_gpio, "fpc1020_irq");

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"gpio_request (irq) failed.\n");

			return error;
		}

		fpc1020->irq_gpio = pdata->irq_gpio;

		error = gpio_direction_input(fpc1020->irq_gpio);

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"gpio_direction_input (irq) failed.\n");
			return error;
		}
	} else {
		return -EINVAL;
	}

	fpc1020->irq = gpio_to_irq(fpc1020->irq_gpio);

	if (fpc1020->irq < 0) {
		dev_err(&fpc1020->spi->dev, "gpio_to_irq failed.\n");
		error = fpc1020->irq;
		return error;
	}

	error = request_irq(fpc1020->irq, fpc1020_interrupt,
			IRQF_TRIGGER_RISING|IRQF_NO_SUSPEND, "fpc1020", fpc1020);

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"request_irq %i failed.\n",
			fpc1020->irq);

		fpc1020->irq = -EINVAL;

		return error;
	}

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_spi_setup(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata)
{
	int error = 0;

	printk(KERN_INFO "%s\n", __func__);

	fpc1020->spi->mode = SPI_MODE_0;
	fpc1020->spi->bits_per_word = 8;
	fpc1020->spi->max_speed_hz =8000000;
	error = spi_setup(fpc1020->spi);

	if (error) {
		dev_err(&fpc1020->spi->dev, "spi_setup failed\n");
		goto out_err;
	}
#if CS_CONTROL
	if (gpio_is_valid(pdata->cs_gpio)) {

		dev_info(&fpc1020->spi->dev,
			"Assign SPI.CS -> GPIO%d\n",
			pdata->cs_gpio);

		error = gpio_request(pdata->cs_gpio, "fpc1020_cs");
		if (error) {
			dev_err(&fpc1020->spi->dev,
				"gpio_request (cs) failed.\n");

			goto out_err;
		}

		fpc1020->cs_gpio = pdata->cs_gpio;

		error = gpio_direction_output(fpc1020->cs_gpio, 1);
		if (error) {
			dev_err(&fpc1020->spi->dev,
				"gpio_direction_output(cs) failed.\n");
			goto out_err;
		}
	} else {
		error = -EINVAL;
	}
#endif
out_err:
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_worker_init(fpc1020_data_t *fpc1020)
{
	int error = 0;

	printk(KERN_INFO "%s\n", __func__);

	init_waitqueue_head(&fpc1020->worker.wq_wait_job);
	sema_init(&fpc1020->worker.sem_idle, 0);

	fpc1020->worker.req_mode = FPC1020_WORKER_IDLE_MODE;

	fpc1020->worker.thread = kthread_run(fpc1020_worker_function,
					   fpc1020, "%s",
					   FPC1020_WORKER_THREAD_NAME);

	if (IS_ERR(fpc1020->worker.thread)) {
		dev_err(&fpc1020->spi->dev, "kthread_run failed.\n");
		error = (int)PTR_ERR(fpc1020->worker.thread);
	}

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_worker_destroy(fpc1020_data_t *fpc1020)
{
	int error = 0;

	printk(KERN_INFO "%s\n", __func__);

	if (fpc1020->worker.thread) {
		fpc1020_worker_goto_idle(fpc1020);

		fpc1020->worker.req_mode = FPC1020_WORKER_EXIT;
		wake_up_interruptible(&fpc1020->worker.wq_wait_job);
		kthread_stop(fpc1020->worker.thread);
	}

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_create_class(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020->class = class_create(THIS_MODULE, FPC1020_CLASS_NAME);

	if (IS_ERR(fpc1020->class)) {
		dev_err(&fpc1020->spi->dev, "failed to create class.\n");
		error = PTR_ERR(fpc1020->class);
	}

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_create_device(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020->devno = MKDEV(FPC1020_MAJOR, fpc1020_device_count++);

	fpc1020->device = device_create(fpc1020->class, NULL, fpc1020->devno,
						NULL, "%s", FPC1020_DEV_NAME);

	if (IS_ERR(fpc1020->device)) {
		dev_err(&fpc1020->spi->dev, "device_create failed.\n");
		error = PTR_ERR(fpc1020->device);
	}

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_manage_sysfs(fpc1020_data_t *fpc1020,
				struct spi_device *spi, bool create)
{
	int error = 0;

	if (create) {
		dev_dbg(&fpc1020->spi->dev, "%s create\n", __func__);

		error = sysfs_create_group(&spi->dev.kobj,
					&fpc1020_setup_attr_group);

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"sysf_create_group failed.\n");
			return error;
		}

		error = sysfs_create_group(&spi->dev.kobj,
					&fpc1020_diag_attr_group);

		if (error) {
			sysfs_remove_group(&spi->dev.kobj,
					&fpc1020_setup_attr_group);

			dev_err(&fpc1020->spi->dev,
				"sysf_create_group failed.\n");

			return error;
		}
	} else {
		dev_dbg(&fpc1020->spi->dev, "%s remove\n", __func__);

		sysfs_remove_group(&spi->dev.kobj, &fpc1020_setup_attr_group);
		sysfs_remove_group(&spi->dev.kobj, &fpc1020_diag_attr_group);
	}

	return error;
}


/* -------------------------------------------------------------------- */
irqreturn_t fpc1020_interrupt(int irq, void *_fpc1020)
{
	fpc1020_data_t *fpc1020 = _fpc1020;
	if (gpio_get_value(fpc1020->irq_gpio)) {
	if(fp_LONGPRESSWAKEUP== atomic_read(&fpc1020->taskstate))
	{
		printk("fpc1020_interrupt fpc1020_start_wakeup \n");

	}
		fpc1020->interrupt_done = true;
		wake_up_interruptible(&fpc1020->wq_irq_return);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}


/* -------------------------------------------------------------------- */
static ssize_t fpc1020_show_attr_setup(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	fpc1020_data_t *fpc1020 = dev_get_drvdata(dev);
	struct fpc1020_attribute *fpc_attr;
	int val = -1;
	int mux;

	fpc_attr = container_of(attr, struct fpc1020_attribute, attr);

	mux = fpc1020->setup.capture_settings_mux;

	if (fpc_attr->offset == offsetof(fpc1020_setup_t, adc_gain))
		val = fpc1020->setup.adc_gain[mux];

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, adc_shift))
		val = fpc1020->setup.adc_shift[mux];

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, pxl_ctrl))
		val = fpc1020->setup.pxl_ctrl[mux];

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_mode))
		val = fpc1020->setup.capture_mode;

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_count))
		val = fpc1020->setup.capture_count;

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_settings_mux))
		val = fpc1020->setup.capture_settings_mux;

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_row_start))
		val = fpc1020->setup.capture_row_start;

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_row_count))
		val = fpc1020->setup.capture_row_count;

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_col_start))
		val = fpc1020->setup.capture_col_start;

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_col_groups))
		val = fpc1020->setup.capture_col_groups;
	if (val >= 0)
		return scnprintf(buf, PAGE_SIZE, "%i\n", val);

	return -ENOENT;
}


/* -------------------------------------------------------------------- */
static ssize_t fpc1020_store_attr_setup(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	fpc1020_data_t *fpc1020 = dev_get_drvdata(dev);
	u64 val;
	int error = kstrtou64(buf, 0, &val);
	int mux;

	struct fpc1020_attribute *fpc_attr;
	fpc_attr = container_of(attr, struct fpc1020_attribute, attr);

	mux = fpc1020->setup.capture_settings_mux;

	if (!error) {
		if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, adc_gain)) {

			fpc1020->setup.adc_gain[mux] = (u8)val;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, adc_shift)) {

			fpc1020->setup.adc_shift[mux] = (u8)val;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, pxl_ctrl)) {

			fpc1020->setup.pxl_ctrl[mux] = (u16)val;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_mode)) {

			fpc1020->setup.capture_mode =
					(fpc1020_capture_mode_t)val;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_count)) {

			if (fpc1020_check_in_range_u64
				(val, 1, FPC1020_BUFFER_MAX_IMAGES)) {

				fpc1020->setup.capture_count = (u8)val;
			} else
				return -EINVAL;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_settings_mux)) {
			if (fpc1020_check_in_range_u64
				(val, 0, (FPC1020_BUFFER_MAX_IMAGES - 1))) {

				fpc1020->setup.capture_settings_mux = (u8)val;
			} else
				return -EINVAL;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_row_start)) {

			if (fpc1020_check_in_range_u64
				(val, 0, (FPC1020_PIXEL_ROWS - 1))) {

				fpc1020->setup.capture_row_start = (u8)val;
			} else
				return -EINVAL;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_row_count)) {

			if (fpc1020_check_in_range_u64
				(val, 1, FPC1020_PIXEL_ROWS)) {

				fpc1020->setup.capture_row_count = (u8)val;
			} else
				return -EINVAL;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_col_start)) {

			if (fpc1020_check_in_range_u64
				(val, 0, (FPC1020_PIXEL_COL_GROUPS - 1))) {

				fpc1020->setup.capture_col_start = (u8)val;
			} else
				return -EINVAL;
				
		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_col_groups)) {

			if (fpc1020_check_in_range_u64
				(val, 1, FPC1020_PIXEL_COL_GROUPS)) {

				fpc1020->setup.capture_col_groups = (u8)val;
			} else
				return -EINVAL;

		} else
			return -ENOENT;

		return strnlen(buf, count);
	}
	return error;
}


/* -------------------------------------------------------------------- */
static ssize_t fpc1020_show_attr_diag(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	fpc1020_data_t *fpc1020;
	struct fpc1020_attribute *fpc_attr;
	u64 val;
	int error = 0;
	bool is_buffer = false;
	u8 u8_buffer[FPC1020_REG_MAX_SIZE];
	char hex_string[sizeof("0x") + (FPC1020_REG_MAX_SIZE * 2)];
	char module[5];
	fpc1020 = dev_get_drvdata(dev);

	fpc_attr = container_of(attr, struct fpc1020_attribute, attr);

	if (fpc_attr->offset == offsetof(fpc1020_diag_t, chip_id)) {
		if (gpio_is_valid(fpc1020->moduleID_gpio)){
 			val = gpio_get_value(fpc1020->moduleID_gpio);
			if(val)
				strcpy(module, "CT");
			else
				strcpy(module , "LO");
		}
		else
			strcpy(module , "NN");
			
		return scnprintf(buf,
				PAGE_SIZE,
				"%s-%d-%s\n",
				fpc1020_hw_id_text(fpc1020),
				fpc1020->chip_revision,module);
	}

	else if (fpc_attr->offset == offsetof(fpc1020_diag_t, selftest)) {
		val = (u64)fpc1020_selftest_short(fpc1020);
	}

	else if (fpc_attr->offset == offsetof(fpc1020_diag_t, spi_register)) {
		val = (int)fpc1020->diag.spi_register;
	}

	else if (fpc_attr->offset == offsetof(fpc1020_diag_t, spi_regsize)) {
		val = (int)fpc1020->diag.spi_regsize;
	}

	else if (fpc_attr->offset == offsetof(fpc1020_diag_t, spi_data)) {

		is_buffer = (fpc1020->diag.spi_regsize > sizeof(val));

		if (!is_buffer) {
			error = fpc1020_spi_debug_value_read(fpc1020, &val);
		} else {
			error = fpc1020_spi_debug_buffer_read(fpc1020,
							u8_buffer,
							sizeof(u8_buffer));
		}
	}
	else if (fpc_attr->offset == offsetof(fpc1020_diag_t, fingerdetect)){
		val = (int)fpc1020->diag.fingerdetect;
	}
	else if(fpc_attr->offset == offsetof(fpc1020_diag_t, navigation_enable)){
		val = (int)fpc1020->diag.navigation_enable;
	}
	else if(fpc_attr->offset == offsetof(fpc1020_diag_t, wakeup_enable)){
		val = (int)fpc1020->diag.wakeup_enable;
	}

	if (error >= 0 && !is_buffer) {
		return scnprintf(buf,
				PAGE_SIZE,
				"%lu\n",
				(long unsigned int)val);
	}

	if (error >= 0 && is_buffer) {
		fpc1020_spi_debug_buffer_to_hex_string(hex_string,
						u8_buffer,
						fpc1020->diag.spi_regsize);

		return scnprintf(buf, PAGE_SIZE, "%s\n", hex_string);
	}

	return -ENOTTY;
}


/* -------------------------------------------------------------------- */
static ssize_t fpc1020_store_attr_diag(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	fpc1020_data_t *fpc1020 = dev_get_drvdata(dev);
	u64 val = 0;
	int error = 0;

	struct fpc1020_attribute *fpc_attr;
	fpc_attr = container_of(attr, struct fpc1020_attribute, attr);

	if (fpc_attr->offset == offsetof(fpc1020_diag_t, spi_register)) {
		error = kstrtou64(buf, 0, &val);

		if (!error) {
			error = fpc1020_spi_debug_select(fpc1020,
							(fpc1020_reg_t)val);
		}
	} else if (fpc_attr->offset == offsetof(fpc1020_diag_t, fingerdetect)) {
		error = kstrtou64(buf, 0, &val);
		if (!error)
		{
			if(val == FPC1020_FINGER_DETECT_BEGIN)
			{
				printk("fpc1020 fingerdetect \n");
				fpc1020->diag.fingerdetect = FPC1020_FINGER_DETECT_WORKING;
				fpc1020_new_job(fpc1020, FPC1020_WORKER_FINGER_DETECT_MODE);
			}
			else if(val == FPC1020_FINGER_DETECT_CANCLE)
			{
				printk("fpc1020 fingerdetect cancle \n");
				fpc1020_worker_goto_idle(fpc1020);
			}
		}

	} else if (fpc_attr->offset == offsetof(fpc1020_diag_t, spi_data)) {

		if (fpc1020->diag.spi_regsize <= sizeof(val)) {
			error = kstrtou64(buf, 0, &val);

			if (!error)
				error = fpc1020_spi_debug_value_write(fpc1020,
									 val);
		} else {
			error = fpc1020_spi_debug_buffer_write(fpc1020,
								buf,
								count);
		}
	} else if(fpc_attr->offset == offsetof(fpc1020_diag_t, navigation_enable)){
		error = kstrtou64(buf, 0, &val);
		if (!error)
		{
			if(val == 0)
			{
				if(fpc1020->diag.navigation_enable == 1)
				{
					fpc1020->diag.navigation_enable = val;
					fpc1020_worker_goto_idle(fpc1020);
				}
			}
			else if(val == 1)
			{
				fpc1020->diag.navigation_enable = val;
				fpc1020_start_navigation(fpc1020);
			}
			printk("fpc1020 bnavigation enable = %d \n",fpc1020->diag.navigation_enable);
		}
	} else if(fpc_attr->offset == offsetof(fpc1020_diag_t, wakeup_enable)){
		error = kstrtou64(buf, 0, &val);
		if (!error)
		{
			if(val == 0 )
			{
				fpc1020->diag.wakeup_enable = val;
				#if defined(CONFIG_FB)
				if(fpc1020->fb_notify.notifier_call != NULL)
				{
					fpc1020->fb_notify.notifier_call = NULL;

					if (fb_unregister_client(&fpc1020->fb_notify))
						printk("fingerprintk error occurred while unregistering fb_notifier.\n");
				}
				#endif
			}
			else if(val == 1)
			{
				fpc1020->diag.wakeup_enable = val;
				#if defined(CONFIG_FB)
				if(fpc1020->fb_notify.notifier_call == NULL)
				{
					fpc1020->fb_notify.notifier_call = fb_notifier_callback;

					if(fb_register_client(&fpc1020->fb_notify))
						printk("fingerprintk error occurred while registering fb_notifier.\n");
				}
				#endif
			}
			printk("fpc1020 bwakeup enable = %d \n",fpc1020->diag.wakeup_enable);
		}
	} else
		error = -EPERM;

	return (error < 0) ? error : strnlen(buf, count);
}


/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
static int fpc1020_check_for_deadPixels(fpc1020_data_t *fpc1020)
{
	int deadpixels = 0;
	int x, y;
	int i1, i2;
	u8 *buff = fpc1020->huge_buffer;
	int buffersize = FPC1020_FRAME_SIZE_MAX;
	bool odd;
	int maxDev = 0;
	int dev1, dev2;

	int config_max_deviation = 30;
	int config_min_checker_diff = 60;
	int config_max_dead_pixels_to_list = 20;

/*
	int i;
	for (i = 0; i < FPC1020_FRAME_SIZE; i += 8)
		printk("%s: %2x %2x %2x %2x %2x %2x %2x %x\n", __func__,
			buff[i], buff[i+1], buff[i+2], buff[i+3], buff[i+4], buff[i+5], buff[i+6], buff[i+7]);
*/

	int sum1 = 0;
    int sum2 = 0;

	for (y = 0; y < FPC1020_PIXEL_ROWS; y++) {
		for (x = 0; x < FPC1020_PIXEL_COLUMNS; x += 2) {
			odd = (y % 2) != 0;
            i1 = y * FPC1020_PIXEL_COLUMNS + x + ((odd) ? 1 : 0);
            i2 = y * FPC1020_PIXEL_COLUMNS + x + ((odd) ? 0 : 1);
			sum1 += buff[i1];
			sum2 += buff[i2];
		}
	}

	sum1 /= (buffersize / 2);
	sum2 /= (buffersize / 2);

	printk("%s: Average pixel values = %d and %d\n", __func__, sum1, sum2);

	if (abs(sum1 - sum2) < config_min_checker_diff) {
		printk("%s: Too small differance between white and black\n", __func__);
		return FPC_1020_MIN_CHECKER_DIFF_ERROR;
	}


	for (y = 0; y < FPC1020_PIXEL_ROWS; y++) {
		for (x = 0; x < FPC1020_PIXEL_COLUMNS; x += 2) {
			odd = (y % 2) != 0;
			i1 = y * FPC1020_PIXEL_COLUMNS + x + ((odd) ? 1 : 0);
			i2 = y * FPC1020_PIXEL_COLUMNS + x + ((odd) ? 0 : 1);

			dev1 = abs(sum1 - (int)buff[i1]);
			dev2 = abs(sum2 - (int)buff[i2]);

			if (dev1 > maxDev) maxDev = dev1;
			if (dev2 > maxDev) maxDev = dev2;

			if (dev1 > config_max_deviation) {
				if (deadpixels < config_max_dead_pixels_to_list) {
					printk("%s: Dead pixel found @ ImgXY[%d, %d]\n", __func__, (x + ((odd) ? 1 : 0)), y);
				}
				deadpixels++;
			}
			if (dev2 > config_max_deviation) {
				if (deadpixels < config_max_dead_pixels_to_list) {
					printk("%s: Dead pixel found @ imgXY[%d, %d]\n", __func__, (x + ((odd) ? 0 : 1)), y);
				}
				deadpixels++;
			}
		}
	}

	if (deadpixels > config_max_dead_pixels_to_list) {
		printk("%s: More dead pixels found... Not listing all, deadpixel num =%d .n", __func__,deadpixels);
	}

	if (deadpixels == 0) {
		printk("%s: Found no dead pixels, highest deviation = %d\n", __func__, maxDev);
	}

	return deadpixels > FPC1020_DEADPIXEL_THRESHOLD ? FPC_1020_TOO_MANY_DEADPIXEL_ERROR : 0;
}

/* -------------------------------------------------------------------- */
static int fpc1020_test_deadpixels(fpc1020_data_t *fpc1020)
{
	int error = 0;

	// Checkerboard Test
	fpc1020->setup.capture_mode = FPC1020_MODE_CHECKERBOARD_TEST_NORM;
	error = fpc1020_start_capture(fpc1020);
	if (error)
		goto out;

	error = wait_event_interruptible(
			fpc1020->capture.wq_data_avail,
			(fpc1020->capture.available_bytes > 0));

	if (error)
		goto out;

	if (fpc1020->capture.last_error != 0) {
		error = fpc1020->capture.last_error;
		goto out;
	}

	error = fpc1020_check_for_deadPixels(fpc1020);
	if (error)
		goto out;

	// INV Checkerboard Test
	fpc1020->setup.capture_mode = FPC1020_MODE_CHECKERBOARD_TEST_INV;
	error = fpc1020_start_capture(fpc1020);
	if (error)
		goto out;

	error = wait_event_interruptible(
			fpc1020->capture.wq_data_avail,
			(fpc1020->capture.available_bytes > 0));

	if (error)
		goto out;

	if (fpc1020->capture.last_error != 0) {
		error = fpc1020->capture.last_error;
		goto out;
	}

	error = fpc1020_check_for_deadPixels(fpc1020);
	if (error)
		goto out;

out:
	//up(&fpc1020->mutex);

	return error;
}
/*--------l00212801 add for detect the finger---------*/
static u8 fpc1020_fingerdetect(fpc1020_data_t *fpc1020)
{
	int error = 0;
	fpc1020->diag.fingerdetect = FPC1020_FINGER_DETECT_WORKING;
	fpc1020_wake_up(fpc1020);
	error = fpc1020_write_sensor_setup(fpc1020);
	dev_dbg(&fpc1020->spi->dev, "fpc1020 write sensor setup");
	if (error < 0)
	{
		fpc1020->diag.fingerdetect = FPC_1020_FINGER_DETECT_ERROR;
		goto out;
	}
	error = fpc1020_wait_finger_present(fpc1020);

	if (error < 0)
	{
		fpc1020->diag.fingerdetect = FPC_1020_WAIT_FOR_FINGER_ERROR;
		goto out;
	}
	dev_dbg(&fpc1020->spi->dev, "Finger down for MMI\n");

	error = 0;
	while (error <= FPC1020_FINGER_DOWN_THRESHOLD)
	{
		if (fpc1020->worker.stop_request)
		{
			error = -EINTR;
			goto out;
		}

		error = fpc1020_check_finger_present_sum(fpc1020);
		if (error < 0)
		{
			fpc1020->diag.fingerdetect = FPC_1020_FINGER_DETECT_ERROR;
			goto out;
		}
		msleep(FPC1020_POLLING_TIME_MS);
	}
	fpc1020_read_irq(fpc1020, true);
	fpc1020->diag.fingerdetect = FPC1020_FINGER_DETECT_SUCESS;
	dev_dbg(&fpc1020->spi->dev, "Finger detect for MMI\n");
out:
	return 0;
}
/*---------------------------------------------------*/
/* -------------------------------------------------------------------- */
static u8 fpc1020_selftest_short(fpc1020_data_t *fpc1020)
{
	const char *id_str = "selftest,";
	int error = 0;
	int err_no = 0;

#ifdef CONFIG_INPUT_FPC1020_NAV
	bool resume_nav = false;
	if(fpc1020->nav.enabled) {
		resume_nav = true;
		fpc1020_worker_goto_idle(fpc1020);
	}
#endif
	fpc1020->diag.selftest = 0;

	error = fpc1020_wake_up(fpc1020);

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s wake up fail on entry.\n", id_str);
		err_no = FPC_1020_WAKEUP_DEV_ERROR;
		goto out;
	}

	error = fpc1020_reset(fpc1020);

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s reset fail on entry.\n", id_str);
		err_no =FPC_1020_RESET_ENTRY_ERROR;
		goto out;
	}
	error = fpc1020_check_hw_id(fpc1020);


	if (error){
		err_no = FPC_1020_VERIFY_HW_ID_ERROR;
		goto out;
		}
	if (gpio_is_valid(fpc1020->moduleID_gpio))
		printk("fingerprint module ID = %d \n",gpio_get_value(fpc1020->moduleID_gpio));

	fpc1020_setup_defaults(fpc1020);

	error = fpc1020_cmd(fpc1020, FPC1020_CMD_CAPTURE_IMAGE, false);

	if (error < 0) {
		dev_err(&fpc1020->spi->dev,
			"%s capture command failed.\n", id_str);
		err_no =FPC_1020_CAPTURE_IMAGE_ERROR;
		goto out;
	}

	error = gpio_get_value(fpc1020->irq_gpio) ? 0 : -EIO;

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s IRQ not HIGH after capture.\n", id_str);
		err_no =FPC_1020_IRQ_NOT_PULL_UP_ERROR;
		goto out;
	}

	error = fpc1020_wait_for_irq(fpc1020, FPC1020_DEFAULT_IRQ_TIMEOUT_MS);

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s IRQ-wait after capture failed.\n", id_str);
		err_no =FPC_1020_WAIT_IRQ_ERROR;
		goto out;
	}

	error = fpc1020_read_irq(fpc1020, true);


	if (error < 0) {
		dev_err(&fpc1020->spi->dev,
			"%s IRQ clear fail\n", id_str);
		err_no =FPC_1020_CLEAR_IRQ_ERROR;
		goto out;
	} else
		error = 0;

	error = (gpio_get_value(fpc1020->irq_gpio) == 0) ? 0 : -EIO;

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s IRQ not LOW after clear.\n", id_str);
		err_no =FPC_1020_IRQ_NOT_PULL_LOW_ERROR;
		goto out;
	}

	error = fpc1020_reset(fpc1020);

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s reset fail on exit.\n", id_str);
		err_no =FPC_1020_RESET_EXIT_ERROR;
		goto out;
	}

	error = fpc1020_read_status_reg(fpc1020);

	if (error != FPC1020_STATUS_REG_RESET_VALUE)  {
		dev_err(&fpc1020->spi->dev,
			 "%s status check fail on exit.\n", id_str);
		err_no =FPC_1020_READ_STATUS_REG_ERROR;
		goto out;
	}
	/*--------l00212801 add for pixel check---------*/
	error = fpc1020_test_deadpixels(fpc1020);
	if (error != 0){
		err_no =error;
		goto out;
	}

	/*---------------------------------------------------*/
	error = 0;

out:
	fpc1020->diag.selftest = (error == 0) ? 1 : err_no;

	dev_info(&fpc1020->spi->dev, "%s %s\n", id_str,
				(1==fpc1020->diag.selftest) ? "PASS" : "FAIL");

#ifdef CONFIG_INPUT_FPC1020_NAV
	if (resume_nav && fpc1020->diag.selftest)
		fpc1020_start_navigation(fpc1020);
#endif

	return fpc1020->diag.selftest;
};


/* -------------------------------------------------------------------- */
static int fpc1020_start_capture(fpc1020_data_t *fpc1020)
{
	fpc1020_capture_mode_t mode = fpc1020->setup.capture_mode;
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s mode= %d\n", __func__, mode);

	/* Mode check (and pre-conditions if required) ? */
	switch (mode) {
	case FPC1020_MODE_WAIT_AND_CAPTURE:
	case FPC1020_MODE_SINGLE_CAPTURE:
	case FPC1020_MODE_WAIT_FINGER_UP_AND_CAPTURE:
	case FPC1020_MODE_CHECKERBOARD_TEST_NORM:
	case FPC1020_MODE_CHECKERBOARD_TEST_INV:
	case FPC1020_MODE_BOARD_TEST_ONE:
	case FPC1020_MODE_BOARD_TEST_ZERO:
		break;

	case FPC1020_MODE_IDLE:
	default:
		error = -EINVAL;
		break;
	}

	fpc1020->capture.current_mode = (error >= 0) ? mode : FPC1020_MODE_IDLE;

	fpc1020->capture.state = FPC1020_CAPTURE_STATE_STARTED;
	fpc1020->capture.available_bytes  = 0;
	fpc1020->capture.read_offset = 0;
	fpc1020->capture.read_pending_eof = false;

	fpc1020_new_job(fpc1020, FPC1020_WORKER_CAPTURE_MODE);

	return error;
}

/* -------------------------------------------------------------------- */
static void fpc1020_worker_goto_idle(fpc1020_data_t *fpc1020)
{
	const int wait_idle_us = 1;

	if (down_trylock(&fpc1020->worker.sem_idle)) {
		dev_dbg(&fpc1020->spi->dev, "%s, stop_request\n", __func__);

		fpc1020->worker.stop_request = true;
		fpc1020->worker.req_mode = FPC1020_WORKER_IDLE_MODE;

		while (down_trylock(&fpc1020->worker.sem_idle))	{

			fpc1020->worker.stop_request = true;
			fpc1020->worker.req_mode = FPC1020_WORKER_IDLE_MODE;

			msleep(wait_idle_us);
		}
		dev_dbg(&fpc1020->spi->dev, "%s, is idle\n", __func__);
		up(&fpc1020->worker.sem_idle);

	} else {
		dev_dbg(&fpc1020->spi->dev, "%s, already idle\n", __func__);
		up(&fpc1020->worker.sem_idle);
	}
	atomic_set(&fpc1020->taskstate, fp_UNINIT);
	return ;
}

/* -------------------------------------------------------------------- */
static void fpc1020_new_job(fpc1020_data_t *fpc1020, int new_job)
{

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, new_job);

	fpc1020_worker_goto_idle(fpc1020);

	fpc1020->worker.req_mode = new_job;
	fpc1020->worker.stop_request = false;

	wake_up_interruptible(&fpc1020->worker.wq_wait_job);

	return ;
}


/* -------------------------------------------------------------------- */
static int fpc1020_worker_function(void *_fpc1020)
{
	fpc1020_data_t *fpc1020 = _fpc1020;
	int error = 0;

	while (!kthread_should_stop()) {

		up(&fpc1020->worker.sem_idle);

		wait_event_interruptible(fpc1020->worker.wq_wait_job,
			fpc1020->worker.req_mode != FPC1020_WORKER_IDLE_MODE);

		down(&fpc1020->worker.sem_idle);

		switch (fpc1020->worker.req_mode) {
		case FPC1020_WORKER_CAPTURE_MODE:
			fpc1020->capture.state = FPC1020_CAPTURE_STATE_PENDING;
			error = fpc1020_capture_task(fpc1020);
			break;

#ifdef CONFIG_INPUT_FPC1020_NAV
		case FPC1020_WORKER_INPUT_MODE:
			fpc1020_input_enable(fpc1020, true);
			error = fpc1020_input_task(fpc1020);
			break;
#endif

#ifdef CONFIG_INPUT_FPC1020_WAKEUP
		case FPC1020_WORKER_WAKEUP_MODE:
			error = fpc1020_wakeup_task(fpc1020);

			break;
#endif
		case FPC1020_WORKER_FINGER_DETECT_MODE:
			error = fpc1020_fingerdetect(fpc1020);
			break;
		case FPC1020_WORKER_IDLE_MODE:
		case FPC1020_WORKER_EXIT:
		default:
			break;
		}
		if(error < 0)
			dev_err(&fpc1020->spi->dev,
				"%s error = %d mode = %d\n", __func__,error,fpc1020->worker.req_mode);

		if (fpc1020->worker.req_mode != FPC1020_WORKER_EXIT)
			fpc1020->worker.req_mode = FPC1020_WORKER_IDLE_MODE;
	}

	return 0;
}


/* -------------------------------------------------------------------- */
/* SPI debug interface, implementation					*/
/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_select(fpc1020_data_t *fpc1020, fpc1020_reg_t reg)
{
	u8 size = FPC1020_REG_SIZE(reg);

	if (size) {
		fpc1020->diag.spi_register = reg;
		fpc1020->diag.spi_regsize  = size;

		dev_dbg(&fpc1020->spi->dev, "%s : selected %d (%d byte(s))\n",
						 __func__
						, fpc1020->diag.spi_register
						, fpc1020->diag.spi_regsize);
		return 0;
	} else {
		dev_dbg(&fpc1020->spi->dev,
			"%s : reg %d not available\n", __func__, reg);

		return -ENOENT;
	}
}


/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_value_write(fpc1020_data_t *fpc1020, u64 data)
{
	int error = 0;
	fpc1020_reg_access_t reg;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	FPC1020_MK_REG_WRITE_BYTES(reg,
				fpc1020->diag.spi_register,
				fpc1020->diag.spi_regsize,
				(u8 *)&data);

	error = fpc1020_reg_access(fpc1020, &reg);

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_buffer_write(fpc1020_data_t *fpc1020,
						const char *data, size_t count)
{
	int error = 0;
	fpc1020_reg_access_t reg;
	u8 u8_buffer[FPC1020_REG_MAX_SIZE];

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = fpc1020_spi_debug_hex_string_to_buffer(u8_buffer,
						sizeof(u8_buffer),
						data,
						count);

	if (error < 0)
		return error;

	FPC1020_MK_REG_WRITE_BYTES(reg,
				fpc1020->diag.spi_register,
				fpc1020->diag.spi_regsize,
				u8_buffer);

	error = fpc1020_reg_access(fpc1020, &reg);

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_value_read(fpc1020_data_t *fpc1020, u64 *data)
{
	int error = 0;
	fpc1020_reg_access_t reg;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	*data = 0;

	FPC1020_MK_REG_READ_BYTES(reg,
				fpc1020->diag.spi_register,
				fpc1020->diag.spi_regsize,
				(u8 *)data);

	error = fpc1020_reg_access(fpc1020, &reg);

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_buffer_read(fpc1020_data_t *fpc1020,
						u8 *data, size_t max_count)
{
	int error = 0;
	fpc1020_reg_access_t reg;

	if (max_count < fpc1020->diag.spi_regsize)
		return -ENOMEM;

	FPC1020_MK_REG_READ_BYTES(reg,
				fpc1020->diag.spi_register,
				fpc1020->diag.spi_regsize,
				data);

	error = fpc1020_reg_access(fpc1020, &reg);

	return error;
}


/* -------------------------------------------------------------------- */
static void fpc1020_spi_debug_buffer_to_hex_string(char *string,
						u8 *buffer,
						size_t bytes)
{
	int count = bytes;
	int pos = 0;
	int src = (target_little_endian) ? (bytes - 1) : 0;
	u8 v1, v2;

	string[pos++] = '0';
	string[pos++] = 'x';

	while (count) {
		v1 = buffer[src] >> 4;
		v2 = buffer[src] & 0x0f;

		string[pos++] = (v1 >= 0x0a) ? ('a' - 0x0a + v1) : ('0' + v1);
		string[pos++] = (v2 >= 0x0a) ? ('a' - 0x0a + v2) : ('0' + v2);

		src += (target_little_endian) ? -1 : 1;

		count--;
	}

	string[pos] = '\0';
}


/* -------------------------------------------------------------------- */
static u8 fpc1020_char_to_u8(char in_char)
{
	if ((in_char >= 'A') && (in_char <= 'F'))
		return (u8)(in_char - 'A' + 0xa);

	if ((in_char >= 'a') && (in_char <= 'f'))
		return (u8)(in_char - 'a' + 0xa);

	if ((in_char >= '0') && (in_char <= '9'))
		return (u8)(in_char - '0');

	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_hex_string_to_buffer(u8 *buffer,
						size_t buf_size,
						const char *string,
						size_t chars)
{
	int bytes = 0;
	int count;
	int dst = (target_little_endian) ? 0 : (buf_size - 1);
	int pos;
	u8 v1, v2;

	if (string[1] != 'x' && string[1] != 'X')
		return -EINVAL;

	if (string[0] != '0')
		return -EINVAL;

	if (chars < sizeof("0x1"))
		return -EINVAL;

	count = buf_size;
	while (count)
		buffer[--count] = 0;

	count = chars - sizeof("0x");

	bytes = ((count % 2) == 0) ? (count / 2) : (count / 2) + 1;

	if (bytes > buf_size)
		return -EINVAL;

	pos = chars - 2;

	while (pos >= 2) {
		v1 = fpc1020_char_to_u8(string[pos--]);
		v2 = (pos >= 2) ? fpc1020_char_to_u8(string[pos--]) : 0;

		buffer[dst] = (v2 << 4) | v1;

		dst += (target_little_endian) ? 1 : -1;
	}
	return bytes;
}


/* -------------------------------------------------------------------- */
#ifdef CONFIG_INPUT_FPC1020_NAV
static void fpc1020_start_navigation(fpc1020_data_t *fpc1020)
{
	if(fp_CAPTURE== atomic_read(&fpc1020->taskstate)){
		dev_err(&fpc1020->spi->dev,
			"%s capturing return\n", __func__);
		return ;
	}

	if(fp_LONGPRESSWAKEUP== atomic_read(&fpc1020->taskstate)){
		dev_err(&fpc1020->spi->dev,
			"%s wakeuping return\n", __func__);
		return ;
	}
	if(fp_TOUCH == atomic_read(&fpc1020->taskstate)){
		dev_err(&fpc1020->spi->dev,
			"%s already navigation \n", __func__);
		return ;
	}
	atomic_set(&fpc1020->taskstate, fp_TOUCH);
	if(fpc1020->diag.navigation_enable == 1)
		fpc1020_new_job(fpc1020, FPC1020_WORKER_INPUT_MODE);
	else
		fpc1020_worker_goto_idle(fpc1020);
	return ;
}
#endif

#ifdef CONFIG_INPUT_FPC1020_WAKEUP
static void fpc1020_start_wakeup(fpc1020_data_t *fpc1020)
{
	if(fp_CAPTURE== atomic_read(&fpc1020->taskstate)){
		dev_err(&fpc1020->spi->dev,
			"%s capturing return\n", __func__);
		return ;
	}

	atomic_set(&fpc1020->taskstate, fp_LONGPRESSWAKEUP);
	if(fpc1020->diag.wakeup_enable == 1)
		fpc1020_new_job(fpc1020, FPC1020_WORKER_WAKEUP_MODE);
	return ;
}
#endif
/* -------------------------------------------------------------------- */

