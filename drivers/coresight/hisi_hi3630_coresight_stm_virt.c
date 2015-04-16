/* Copyright (c) 2013, hisi semiconductor co,ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/hisi_hi3630_of_coresight.h>
#include <linux/hisi_hi3630_coresight.h>
#include <linux/hisi_hi3630_coresight_stm.h>
#include <asm/unaligned.h>

#include "hisi_hi3630_coresight_priv.h"

#define stm_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define stm_readl(drvdata, off)		__raw_readl(drvdata->base + off)

#define STM_LOCK(drvdata)						\
do {									\
	mb();								\
	stm_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define STM_UNLOCK(drvdata)						\
do {									\
	stm_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

#define STMDMASTARTR		(0xC04)
#define STMDMASTOPR		(0xC08)
#define STMDMASTATR		(0xC0C)
#define STMDMACTLR		(0xC10)
#define STMDMAIDR		(0xCFC)
#define STMHEER			(0xD00)
#define STMHETER		(0xD20)
#define STMHEMCR		(0xD64)
#define STMHEMASTR		(0xDF4)
#define STMHEFEAT1R		(0xDF8)
#define STMHEIDR		(0xDFC)
#define STMSPER			(0xE00)
#define STMSPTER		(0xE20)
#define STMSPSCR		(0xE60)
#define STMSPMSCR		(0xE64)
#define STMSPOVERRIDER		(0xE68)
#define STMSPMOVERRIDER		(0xE6C)
#define STMSPTRIGCSR		(0xE70)
#define STMTCSR			(0xE80)
#define STMTSSTIMR		(0xE84)
#define STMTSFREQR		(0xE8C)
#define STMSYNCR		(0xE90)
#define STMAUXCR		(0xE94)
#define STMSPFEAT1R		(0xEA0)
#define STMSPFEAT2R		(0xEA4)
#define STMSPFEAT3R		(0xEA8)
#define STMITTRIGGER		(0xEE8)
#define STMITATBDATA0		(0xEEC)
#define STMITATBCTR2		(0xEF0)
#define STMITATBID		(0xEF4)
#define STMITATBCTR0		(0xEF8)

#define STMLOCK			(0xFB0)

#define NR_STM_CHANNEL		(32)
#define BYTES_PER_CHANNEL	(256)
#define STM_TRACE_BUF_SIZE	(1024)

#define OST_START_TOKEN		(0x30)
#define OST_VERSION		(0x1)

enum stm_pkt_type {
	STM_PKT_TYPE_DATA	= 0x98,
	STM_PKT_TYPE_FLAG	= 0xE8,
	STM_PKT_TYPE_TRIG	= 0xF8,
};

enum {
	STM_OPTION_MARKED	= 0x10,
};

#define stm_channel_addr(drvdata, ch)	(drvdata->chs.base +	\
					(ch * BYTES_PER_CHANNEL))
#define stm_channel_off(type, opts)	(type & ~opts)

#ifdef CONFIG_HISI_CS_STM_DEFAULT_ENABLE
static int boot_enable = 1;
#else
static int boot_enable;
#endif

module_param_named(
	boot_enable, boot_enable, int, S_IRUGO
);

static int boot_nr_channel;

module_param_named(
	boot_nr_channel, boot_nr_channel, int, S_IRUGO
);

struct channel_space {
	void __iomem		*base;
	unsigned long		*bitmap;
};

struct stm_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct miscdevice	miscdev;
	struct clk		*clk_at;
	struct clk		*clk_dbg;
	spinlock_t		spinlock;
	struct channel_space	chs;
	bool			enable;
	bool			status;
	uint32_t		entity;
};

static struct stm_drvdata *virt_stmdrvdata;


void top_tmc_enable(void)
{
	coresight_enable(virt_stmdrvdata->csdev);
}

EXPORT_SYMBOL_GPL(top_tmc_enable);

char *etb_file_path;
#define MAX_ETB_FILE_LEN (128)

void top_tmc_disable(char *pdir)
{
	if (MAX_ETB_FILE_LEN < strlen(pdir) + 1) {
		pr_err("ETB: file length is wrong(%d).\n", strlen(pdir));
		return;
	}
	etb_file_path = kmalloc(MAX_ETB_FILE_LEN, GFP_KERNEL);
	if (etb_file_path == NULL) {
		pr_err("etb:kmalloc mem failed\n");
		return;
	}
	strncpy(etb_file_path, pdir, strlen(pdir) + 1);

	coresight_disable(virt_stmdrvdata->csdev);

	kfree(etb_file_path);

	etb_file_path = NULL;
}

EXPORT_SYMBOL_GPL(top_tmc_disable);

static int stm_hwevent_isenable(struct stm_drvdata *drvdata)
{
	int ret = 0;

	return ret;
}

static int stm_hwevent_enable(struct stm_drvdata *drvdata)
{
	int ret = 0;

	return ret;
}

static int stm_port_isenable(struct stm_drvdata *drvdata)
{
	int ret = 0;

	return ret;
}

static int stm_port_enable(struct stm_drvdata *drvdata)
{
	int ret = 0;

	return ret;
}

static int stm_virt_enable(struct coresight_device *csdev)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	dev_info(drvdata->dev, "STM virt tracing enabled\n");
	return 0;
}

static void stm_hwevent_disable(struct stm_drvdata *drvdata)
{
}

static void stm_port_disable(struct stm_drvdata *drvdata)
{
}

static void stm_virt_disable(struct coresight_device *csdev)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	dev_info(drvdata->dev, "STM virt tracing disabled\n");
}

static const struct coresight_ops_source stm_virt_source_ops = {
	.enable		= stm_virt_enable,
	.disable	= stm_virt_disable,
};

static const struct coresight_ops stm_virt_cs_ops = {
	.source_ops	= &stm_virt_source_ops,
};

static inline int __stm_trace(uint32_t options, uint8_t entity_id,
			      uint8_t proto_id, const void *data, uint32_t size)
{
	return 0;
}

/**
 * stm_trace - trace the binary or string data through STM
 * @options: tracing options - guaranteed, timestamped, etc
 * @entity_id: entity representing the trace data
 * @proto_id: protocol id to distinguish between different binary formats
 * @data: pointer to binary or string data buffer
 * @size: size of data to send
 *
 * Packetizes the data as the payload to an OST packet and sends it over STM
 *
 * CONTEXT:
 * Can be called from any context.
 *
 * RETURNS:
 * number of bytes transfered over STM
 */
int stm_virt_trace(uint32_t options, uint8_t entity_id, uint8_t proto_id,
			const void *data, uint32_t size)
{
	struct stm_drvdata *drvdata = virt_stmdrvdata;

	/* we don't support sizes more than 24bits (0 to 23) */
	if (!(drvdata && drvdata->enable && (drvdata->entity & entity_id) &&
	      (size < 0x1000000)))
		return 0;

	return __stm_trace(options, entity_id, proto_id, data, size);
}
EXPORT_SYMBOL_GPL(stm_virt_trace);

static ssize_t stm_write(struct file *file, const char __user *data,
			 size_t size, loff_t *ppos)
{
	struct stm_drvdata *drvdata = container_of(file->private_data,
						   struct stm_drvdata, miscdev);
	char *buf;

	if (!drvdata->enable) {
		return -EINVAL;
	}

	if (!(drvdata->entity & OST_ENTITY_DEV_NODE)) {
		return size;
	}

	if (size > STM_TRACE_BUF_SIZE)
		size = STM_TRACE_BUF_SIZE;

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}

	if (copy_from_user(buf, data, size)) {
		kfree(buf);
		dev_err(drvdata->dev, "scopy_from_user failed\n");
		return -EFAULT;
	}

	__stm_trace(STM_OPTION_TIMESTAMPED, OST_ENTITY_DEV_NODE, 0, buf, size);

	kfree(buf);

	return size;
}

static const struct file_operations stm_virt_fops = {
	.owner		= THIS_MODULE,
	.open		= nonseekable_open,
	.write		= stm_write,
	.llseek		= no_llseek,
};

static ssize_t stm_show_hwevent_enable(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = stm_hwevent_isenable(drvdata);

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t stm_store_hwevent_enable(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret = 0;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	if (val)
		ret = stm_hwevent_enable(drvdata);
	else
		stm_hwevent_disable(drvdata);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(hwevent_enable, S_IRUGO | S_IWUSR, stm_show_hwevent_enable,
		   stm_store_hwevent_enable);

static ssize_t stm_show_port_enable(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = stm_port_isenable(drvdata);

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t stm_store_port_enable(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret = 0;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	if (val)
		ret = stm_port_enable(drvdata);
	else
		stm_port_disable(drvdata);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(port_enable, S_IRUGO | S_IWUSR, stm_show_port_enable,
		   stm_store_port_enable);

static ssize_t stm_show_entity(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->entity;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t stm_store_entity(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->entity = val;
	return size;
}
static DEVICE_ATTR(entity, S_IRUGO | S_IWUSR, stm_show_entity,
		   stm_store_entity);

static struct attribute *stm_virt_attrs[] = {
	&dev_attr_hwevent_enable.attr,
	&dev_attr_port_enable.attr,
	&dev_attr_entity.attr,
	NULL,
};

static struct attribute_group stm_virt_attr_grp = {
	.attrs = stm_virt_attrs,
};

static const struct attribute_group *stm_virt_attr_grps[] = {
	&stm_virt_attr_grp,
	NULL,
};

static int stm_virt_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata = NULL;
	struct stm_drvdata *drvdata = NULL;
	struct coresight_desc *desc = NULL;

	if (pdev->dev.of_node) {
		pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
		if (IS_ERR(pdata)) {
			dev_err(&pdev->dev, "of_get_coresight_platform_data error!\n");
			return PTR_ERR(pdata);
		}
		pdev->dev.platform_data = pdata;
	}

	if (!pdata) {
		dev_err(&pdev->dev, "coresight pdata is NULL\n");
		return -ENODEV;
	}

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		dev_err(&pdev->dev, "coresight kzalloc error!\n");
		return -ENOMEM;
	}
	/* Store the driver data pointer for use in exported functions */
	virt_stmdrvdata = drvdata;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	spin_lock_init(&drvdata->spinlock);

	drvdata->entity = OST_ENTITY_ALL;
	drvdata->status = false;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc) {
		dev_err(drvdata->dev, "coresight desc kzalloc error!\n");
		return -ENOMEM;
	}
	desc->type = CORESIGHT_DEV_TYPE_SOURCE;
	desc->subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_SOFTWARE;
	desc->ops = &stm_virt_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = stm_virt_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev)) {
		dev_err(drvdata->dev, "coresight_register error!\n");
		return PTR_ERR(drvdata->csdev);
	}

	drvdata->miscdev.name = ((struct coresight_platform_data *)
				 (pdev->dev.platform_data))->name;
	drvdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	drvdata->miscdev.fops = &stm_virt_fops;
	ret = misc_register(&drvdata->miscdev);
	if (ret) {
		dev_err(drvdata->dev, "coresight misc_register error!\n");
		goto err;
	}

	dev_info(drvdata->dev, "STM virt initialized\n");
#if 0
	/*
	 * Enable and disable STM to undo the temporary default STM enable
	 * done by RPM.
	 */
	coresight_enable(drvdata->csdev);
	coresight_disable(drvdata->csdev);
#endif
	boot_enable = 0;
	if (boot_enable)
		coresight_enable(drvdata->csdev);

	return 0;
err:
	coresight_unregister(drvdata->csdev);
	return ret;
}

static int  stm_virt_suspend(struct platform_device *pdev, pm_message_t message)
{

	struct stm_drvdata *drvdata = platform_get_drvdata(pdev);

	dev_info(drvdata->dev, "stm_virt_suspend+++\n");

	dev_info(drvdata->dev, "stm_virt_suspend---\n");
	return 0;
}

static int  stm_virt_resume(struct platform_device *pdev)
{
	struct stm_drvdata *drvdata = platform_get_drvdata(pdev);

	dev_info(drvdata->dev, "stm_virt_resume+++\n");

	dev_info(drvdata->dev, "stm_virt_resume---\n");
	return 0;
}

static int stm_virt_remove(struct platform_device *pdev)
{
	struct stm_drvdata *drvdata = platform_get_drvdata(pdev);

	misc_deregister(&drvdata->miscdev);
	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id stm_virt_match[] = {
	{.compatible = "huawei,coresight-stm-virt"},
	{}
};

static struct platform_driver stm_virt_driver = {
	.probe          = stm_virt_probe,
	.remove         = stm_virt_remove,
	.suspend	= stm_virt_suspend,
	.resume		= stm_virt_resume,
	.driver         = {
		.name   = "coresight-stm-virt",
		.owner	= THIS_MODULE,
		.of_match_table = stm_virt_match,
	},
};

static int __init stm_virt_init(void)
{
	return platform_driver_register(&stm_virt_driver);
}
module_init(stm_virt_init);

static void __exit stm_virt_exit(void)
{
	platform_driver_unregister(&stm_virt_driver);
}
module_exit(stm_virt_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight System Trace Macrocell(virt) driver");
