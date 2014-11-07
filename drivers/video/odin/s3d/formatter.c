/*
 * linux/drivers/video/odin/s3d/formatter.c
 *
 * Copyright (C) 2012 LG Electronics
 * Author:
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/ioport.h>
#include <linux/dnotify.h>
#include <linux/file.h>

#include <asm/uaccess.h> /* copy_from_user, copy_to_user */
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/unistd.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include "formatter.h"
#include "svc_api.h"


/*===========================================================================
  DEFINITIONS
===========================================================================*/

/*===========================================================================
  DATA
===========================================================================*/

static int drv_open (struct inode *inode, struct file *file);
static int drv_release (struct inode *inode, struct file *file);
static long drv_ioctl (struct file *file, unsigned int cmd,
					unsigned long arg);
static ssize_t drv_write  (struct file *file, const char *data,
					size_t len, loff_t *ppos);
static ssize_t drv_read   (struct file *file, char *data,
					size_t len, loff_t *ppos);


/*===========================================================================
 drv_open

===========================================================================*/
static int drv_open (struct inode *inode, struct file *file)
{

  if ( !(try_module_get(THIS_MODULE)) )
  {
    LOGE(" drv_open error !!! ");
    return -ENODEV;
  }

	printk("[M.F] drv_open - version : %s \n", MEPSI_DRIVER_VERSION);

/*  ret = svc_api_init(DRV_NULL, &parm1, &parm2); */

	return nonseekable_open(inode, file);
}

/*===========================================================================
 drv_release

===========================================================================*/
static int drv_release (struct inode *inode, struct file *file)
{
  int ret=RET_OK;
  int32 parm1 = DRV_NULL;
  int32 parm2 = DRV_NULL;

	printk("[M.F] drv_release \n");

	ret = svc_api_exit(DRV_NULL, &parm1, &parm2);

	module_put(THIS_MODULE);

  return 0;
}

/*===========================================================================
 drv_read

===========================================================================*/
static ssize_t drv_read (struct file *file, char *data,
				size_t len, loff_t *ppos)
{
  LOGE("[M.F] drv_read \n");

  return len;
}

/*===========================================================================
 drv_write

===========================================================================*/
static ssize_t drv_write (struct file *file, const char *data,
					size_t len, loff_t *ppos)
{
  LOGE("[M.F] drv_write \n");

  return len;
}

/*===========================================================================
  drv_ioctl

===========================================================================*/
static long drv_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
  int ret=0;

  printk("[M.F] drv_ioctl - cmd: %x  arg: %x \n", cmd, arg);

  ret = svc_api_set ( (uint16) cmd, arg, DRV_NULL );

  return ret;
}

static struct {
	struct platform_device *pdev;
	struct resource *iomem;
	void __iomem *base;
	int irq;
}fmt_dev;


void fmt_write_reg(u32 addr, u32 val)
{
	__raw_writel(val, fmt_dev.base + addr);
}

u32 fmt_read_reg(u32 addr)
{
	return __raw_readl(fmt_dev.base + addr);
}

static const struct file_operations drv_fops ={
    .owner   = THIS_MODULE,
    .open    = drv_open,
    .unlocked_ioctl   = drv_ioctl,
    .write   = drv_write,
    .read    = drv_read,
    .release = drv_release,
};

static struct miscdevice drv_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = NDRIVER_NAME,
    .fops  = &drv_fops
};


#ifdef CONFIG_OF
extern struct device odin_device_parent;

static struct of_device_id odindss_formatter_match[] = {
	{
		.compatible = "odindss-formatter",
	},
	{},
};
#endif

static int odin_formatter_suspend(struct device *dev)
{
	return 0;
}

static int odin_formatter_resume(struct device *dev)
{
	return 0;
}

static int odin_formatter_probe(struct platform_device *pdev)
{
	int ret;

#ifdef CONFIG_OF
	const struct of_device_id *of_id;
	of_id = of_match_device(odindss_formatter_match, &pdev->dev);
	pdev->dev.parent = &odin_device_parent;
#endif

#ifdef CONFIG_OF
	/* irq get later */
	/* hdmi_dev.irq = irq_of_parse_and_map(pdev->dev.of_node, 0); */
	fmt_dev.base = of_iomap(pdev->dev.of_node, 0);

	if (fmt_dev.base == NULL) {
		printk("can't ioremap Formatter\n");
		ret = -ENXIO;
	}
#endif

	platform_set_drvdata(pdev, &fmt_dev);
	ret = misc_register(&drv_device);

	printk("odin_formatter_probe\n");

	return ret;
}

static int odin_formatter_remove(struct platform_device *pdev)
{
	struct fmt_mdev *cdev = platform_get_drvdata(pdev);
	misc_deregister(&drv_device);
	kfree(cdev);

	return 0;
}

static struct platform_driver odin_formatter_driver = {
	.probe          = odin_formatter_probe,
	.remove         = odin_formatter_remove,
	.suspend		= odin_formatter_suspend,
	.resume			= odin_formatter_resume,
	.driver         = {
		.name   = "odindss-formatter",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odindss_formatter_match),
#endif
	},
};

int odin_formatter_init_platform_driver(void)
{
	return platform_driver_register(&odin_formatter_driver);
}

void odin_formatter_uninit_platform_driver(void)
{
	return platform_driver_unregister(&odin_formatter_driver);
}


MODULE_DESCRIPTION("NEXUS Device Driver");
MODULE_AUTHOR("Formatter");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
