/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2014 by LG Electronics Inc.
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

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/bitops.h>

#include "hw_semaphore_drv.h"

#define DRIVER_NAME "odin_aud_semaphore"
#define DT_SEMPROP  "semaphore-list"

#define ODIN_SEMAPHORE00       0x0
#define ODIN_SEMAPHORE01       0x4
#define ODIN_SEMAPHORE02       0x8
#define ODIN_SEMAPHORE03       0xc
#define ODIN_SEMAPHORE04       0x10
#define ODIN_SEMAPHORE05       0x14
#define ODIN_SEMAPHORE06       0x18
#define ODIN_SEMAPHORE07       0x1c
#define ODIN_SEMAPHORE08       0x20
#define ODIN_SEMAPHORE09       0x24
#define ODIN_SEMAPHORE10       0x28
#define ODIN_SEMAPHORE11       0x2c
#define ODIN_SEMAPHORE12       0x30
#define ODIN_SEMAPHORE13       0x34
#define ODIN_SEMAPHORE14       0x38
#define ODIN_SEMAPHORE15       0x3c
#define ODIN_SEMAPHORE16       0x40
#define ODIN_SEMAPHORE17       0x44
#define ODIN_SEMAPHORE18       0x48
#define ODIN_SEMAPHORE19       0x4c
#define ODIN_SEMAPHORE20       0x50
#define ODIN_SEMAPHORE21       0x54
#define ODIN_SEMAPHORE22       0x58
#define ODIN_SEMAPHORE23       0x5c
#define ODIN_SEMAPHORE24       0x60
#define ODIN_SEMAPHORE25       0x64
#define ODIN_SEMAPHORE26       0x68
#define ODIN_SEMAPHORE27       0x6c
#define ODIN_SEMAPHORE28       0x70
#define ODIN_SEMAPHORE29       0x74
#define ODIN_SEMAPHORE30       0x78
#define ODIN_SEMAPHORE31       0x7c
#define ODIN_INTERRUPT_STATUS  0x100
#define ODIN_INTERRUPT_CLEAR   0x104
#define ODIN_INTERRUPT_MASK    0x108

#define ODIN_AUD_SEMA_INT_CLEAR  1
#define ODIN_AUD_SEMA_INT_MASK   1
#define ODIN_AUD_SEMA_INT_UNMASK 0

#define TOREG(nr) (ODIN_SEMAPHORE00 + (nr * 0x4))

static int odin_aud_sema_lock(unsigned int nr);
static int odin_aud_sema_unlock(unsigned int nr);
static int odin_aud_sema_lock_read(unsigned int nr);

const char **of_odin_aud_sema_list = NULL;
static void __iomem *aud_sema_base = NULL;
static int aud_sema_irq = 0;
static int aud_sema_cnt = 0;

struct hw_sema_init_data odin_aud_sema_init;

struct hw_semaphore_ops odin_aud_sema_ops = {
	.hw_sema_lock = odin_aud_sema_lock,
	.hw_sema_unlock = odin_aud_sema_unlock,
	.hw_sema_read = odin_aud_sema_lock_read
};

#define check_valid_number(nr)                \
do {                                          \
        if ((nr) >= aud_sema_cnt)             \
                return -EINVAL;               \
} while(0)

static unsigned int _aud_sema_read(void __iomem *reg)
{
	return readl(reg);
}

static void _aud_sema_write(void __iomem *reg, unsigned int val)
{
	writel(val, reg);
}

static void _aud_sema_mask_write(void __iomem *reg, unsigned int mask,
				 unsigned int val)
{
	u32 rwval = 0;
	if (!mask)
		return;
	rwval = readl(reg);
	rwval = (rwval & ~mask) | (mask & val);
	writel(rwval, reg);
}

static ssize_t odin_aud_sema_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	char *wbf = NULL;
	size_t bufsz= PAGE_SIZE;
	ssize_t cn = 0;
	unsigned int val = 0;
	int i;

	wbf = devm_kzalloc(dev, bufsz, GFP_KERNEL);
	if (!wbf) {
		dev_err(dev, "fialed to memory for show\n");
		return 0;
	}

	for (i = 0; i < 32; i++) {
		val = odin_aud_sema_lock_read(i);
		if (cn < bufsz) {
			cn += snprintf(wbf + cn, bufsz - cn,
				       "hw_semaphore[%d] is %s\n", i,
				       val ? "lock":"unlock");
		}
	}

	val = _aud_sema_read(aud_sema_base + ODIN_INTERRUPT_STATUS);
	if (cn < bufsz) {
		cn += snprintf(wbf + cn, bufsz - cn,
			       "odin_aud_sema interrupt status 0x%x\n", val);
	}

	val = _aud_sema_read(aud_sema_base + ODIN_INTERRUPT_MASK);
	if (cn < bufsz) {
		cn += snprintf(wbf + cn, bufsz - cn,
			       "odin_aud_sema interrupt mask 0x%x\n", val);
	}

	memcpy(buf, wbf, cn);
	devm_kfree(dev, wbf);
	return cn;
}

static ssize_t odin_aud_sema_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	unsigned long val;
	int ret;

	ret = strict_strtoul(buf, 0, &val);
	if (ret) {
		dev_err(dev, "%s is not in hex or decimal form\n", buf);
		goto store_out;
	}

	if (val < 0 || val >= aud_sema_cnt) {
		dev_err(dev, "%ld is wrong 0 <= val < %d\n", val, aud_sema_cnt);
		goto store_out;
	}

	_aud_sema_read(aud_sema_base + TOREG(val));
	_aud_sema_write(aud_sema_base + TOREG(val), 0x0);
store_out:
	return strnlen(buf, count);
}

static DEVICE_ATTR(sem_st, S_IWUSR | S_IRUGO,
		   odin_aud_sema_show, odin_aud_sema_store);

static int odin_aud_sema_int_disable(unsigned int nr)
{
	check_valid_number(nr);
	_aud_sema_mask_write(aud_sema_base + ODIN_INTERRUPT_MASK,
			     BIT(nr), ODIN_AUD_SEMA_INT_MASK << nr);
	return 0;
}

static int odin_aud_sema_int_enable(unsigned int nr)
{
	check_valid_number(nr);
	_aud_sema_mask_write(aud_sema_base + ODIN_INTERRUPT_MASK,
			     BIT(nr), ODIN_AUD_SEMA_INT_UNMASK << nr);
	return 0;
}

static int odin_aud_sema_int_clear(unsigned int nr)
{
	check_valid_number(nr);
	_aud_sema_mask_write(aud_sema_base + ODIN_INTERRUPT_CLEAR,
			     BIT(nr), ODIN_AUD_SEMA_INT_CLEAR << nr);
	return 0;
}

static int odin_aud_sema_lock(unsigned int nr)
{
	check_valid_number(nr);
	return _aud_sema_read(aud_sema_base + TOREG(nr));
}

static int odin_aud_sema_unlock(unsigned int nr)
{
	check_valid_number(nr);
	_aud_sema_write(aud_sema_base + TOREG(nr), 0x0);
	return 0;
}

static int odin_aud_sema_lock_read(unsigned int nr)
{
	unsigned int value;
	check_valid_number(nr);
	value = _aud_sema_read(aud_sema_base + TOREG(nr));
	if (!value) { /* value is automatically changed to 1 from 0 at read  */
		odin_aud_sema_int_disable(nr);
		_aud_sema_write(aud_sema_base + TOREG(nr), 0x0);
		odin_aud_sema_int_clear(nr);
		odin_aud_sema_int_enable(nr);
	}
	return value;
}

static irqreturn_t odin_aud_sema_irq_handler(int irq, void *data)
{
	struct device *dev = (struct device *)data;
	unsigned int val = 0;
	unsigned int nr = 0;
	int cnt = 0;
	int i;

	val = _aud_sema_read(aud_sema_base + ODIN_INTERRUPT_STATUS);
	cnt = __sw_hweight32(val);
	for (i = 0; i < cnt ; i++) {
		/* nr = fls(val); */
		nr = ffs(val);
		odin_aud_sema_int_clear(nr-1);
		hw_semaphore_interupt_callback(dev, nr-1);
		val &= ~(BIT(nr-1));
	}

	return IRQ_HANDLED;
}

static int construct_sema_list(struct device *dev, struct device_node *np)
{
	int i;
	int ret;
	aud_sema_cnt = of_property_count_strings(np, DT_SEMPROP);
	if (aud_sema_cnt <= 0) {
		dev_err(dev, "not found %s property in dt\n", DT_SEMPROP);
		return -ENOSYS;
	}

	of_odin_aud_sema_list = devm_kzalloc(dev,
				(sizeof(char *) * aud_sema_cnt), GFP_KERNEL);
	if (!of_odin_aud_sema_list) {
		dev_err(dev, "fialed to alloc memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < aud_sema_cnt; i++) {
		ret = of_property_read_string_index
		      (np, DT_SEMPROP, i, (of_odin_aud_sema_list+i));
		if (ret) {
			dev_err(dev, "failed to get str idx %d, %d", i, ret);
			break;
		}
		dev_dbg(dev, "%d : %s", i, of_odin_aud_sema_list[i]);
	}

	return ret;
}

static int odin_aud_sema_plat_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0;

	if (!pdev->dev.of_node) {
		dev_err(dev, "need to device tree\n");
		return -ENOSYS;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	aud_sema_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(aud_sema_base))
		return PTR_ERR(aud_sema_base);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res) {
		aud_sema_irq = res->start;
		ret = devm_request_irq(dev, aud_sema_irq,
				       odin_aud_sema_irq_handler,
				       IRQF_ONESHOT, DRIVER_NAME, dev);
		if (ret) {
			dev_err(dev, "Failed to request irq(%d) %d\n",
				aud_sema_irq, ret);
			return ret;
		}
	} else {
		dev_err(dev, "Failed to get irq number\n");
		return -ENOSYS;
	}

	ret = construct_sema_list(dev, pdev->dev.of_node);
	if (ret)
		return ret;

	odin_aud_sema_init.hw_name = dev_name(dev);
	odin_aud_sema_init.hw_sema_list = of_odin_aud_sema_list;
	odin_aud_sema_init.hw_sema_cnt = aud_sema_cnt;
	odin_aud_sema_init.ops = &odin_aud_sema_ops;

	ret = hw_sema_register(dev, &odin_aud_sema_init);
	if (ret) {
		dev_err(dev, "Failed to register hw sema driver %d\n", ret);
		return ret;
	}

	device_create_file(&pdev->dev, &dev_attr_sem_st);
	return ret;
}

static int odin_aud_sema_plat_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_sem_st);
	hw_sema_deregister(&pdev->dev);
	return 0;
}

static const struct of_device_id odin_aud_sema_of_match[] = {
	{.compatible = "lge,odin-audio-semaphore",},
	{},
};

MODULE_DEVICE_TABLE(of, odin_aud_sema_of_match);

#ifdef CONFIG_PM
static int odin_aud_sema_suspend(struct device *dev)
{
	return 0;
}

static int odin_aud_sema_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops odin_aud_sema_pm = {
	.suspend = odin_aud_sema_suspend,
	.resume  = odin_aud_sema_resume,
};

#define odin_aud_sema_pm_ops (&odin_aud_sema_pm)
#else
#define odin_aud_sema_pm_ops NULL
#endif

static struct platform_driver odin_aud_sema_plat_driver = {
	.driver = {
		   .name	= DRIVER_NAME,
		   .owner	= THIS_MODULE,
		   .pm		= odin_aud_sema_pm_ops,
		   .of_match_table = of_match_ptr(odin_aud_sema_of_match),
		   },
	.probe = odin_aud_sema_plat_probe,
	.remove = odin_aud_sema_plat_remove,
};

module_platform_driver(odin_aud_sema_plat_driver);

MODULE_DESCRIPTION("Odin Audio Semaphore Driver");
MODULE_LICENSE("GPL");
