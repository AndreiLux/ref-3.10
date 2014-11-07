/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
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
#include <linux/kernel.h>
#include <linux/interrupt.h>    /**< For isr */
#include <linux/irq.h>			/**< For isr */
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/odin_mailbox.h>
#include <linux/odin_pd.h>
#include <asm/io.h>
#include "dsp_drv.h"

#include "mailbox/mailbox_hal.h"
#include "dsp_lpa_drv.h"
#include "dsp_pcm_drv.h"
#include "dsp_omx_drv.h"
#include "dsp_alsa_drv.h"

#if defined(CONFIG_WODEN_DSP)
#include "woden/dsp_coredrv_woden.h"
#elif defined(CONFIG_ODIN_DSP)
#include "odin/dsp_coredrv_odin.h"
#endif

/* For Debug */
#define	CHECK_DSP_DEBUG	0
#define FOR_DEBUG 1

atomic_t status = ATOMIC_INIT(0);

DSP_MEM_CFG *pst_dsp_mem_cfg = &dsp_mem_cfg[0];

static struct platform_driver dsp_driver;

static int dsp_init_function(void);

struct class *dsp_dev_class;

static DEFINE_MUTEX(mutex);

#ifdef CONFIG_PM_WAKELOCKS
	struct wake_lock odin_wake_lock;
	char odin_wake_lock_name[32];
#endif

int (*dsp_register_device)(struct platform_device *pdev);
void (*dsp_unregister_device)(void);
#if !defined(WITH_BINARY)
int (*dsp_download_dram)(void *firmwareBuffer);
int (*dsp_download_iram)(void *firmwareBuffer);
#else
int (*dsp_download_dram)(void);
int (*dsp_download_iram)(void);
#endif
int (*dsp_power_on)(void);
void (*dsp_power_off)(void);
irqreturn_t (*dsp_interrupt)(int irq, void *data);

static ssize_t dsp_get_status(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	int status_dsp;
	status_dsp = atomic_read(&lpa_status);
	return sprintf(buf, "%d\n", status_dsp);
}

static CLASS_ATTR(dsp_status, S_IWUSR|S_IRUGO, dsp_get_status, NULL);

static int
dsp_init_function(void)
{

#if defined(CONFIG_WODEN_DSP)
	dsp_register_device = dsp_woden_register_device;
	dsp_download_dram = dsp_woden_download_dram;
	dsp_download_iram = dsp_woden_download_iram;
	dsp_power_on = dsp_woden_power_on;
	dsp_power_off = dsp_woden_power_off;
	dsp_unregister_device = dsp_woden_unregister_device;
	dsp_interrupt = dsp_woden_interrupt;

#elif defined(CONFIG_ODIN_DSP)
	dsp_register_device = dsp_odin_register_device;
	dsp_download_dram = dsp_odin_download_dram;
	dsp_download_iram = dsp_odin_download_iram;
	dsp_power_on = dsp_odin_power_on;
	dsp_power_off = dsp_odin_power_off;
	dsp_unregister_device = dsp_odin_unregister_device;
	dsp_interrupt = dsp_odin_interrupt;
#endif

	return 0;
}


static int
dsp_open(struct inode *inode, struct file *filp)
{
	switch (MINOR(inode->i_rdev))
	{
		case DSP_OMX_MINOR	: filp->f_op = &gst_dsp_omx_fops; break;
		case DSP_LPA_MINOR	: filp->f_op = &gst_dsp_lpa_fops; break;
		case DSP_PCM_MINOR	: filp->f_op = &gst_dsp_pcm_fops; break;
		case DSP_ALSA_MINOR	: filp->f_op = &gst_dsp_alsa_fops; break;
	
		default	: return -ENXIO;
	}

	if (filp->f_op && filp->f_op->open)
	{
		return filp->f_op->open(inode,filp);
	}
	
	return 0;
}

static int
dsp_close(struct inode *inode, struct file *filp)
{
	switch (MINOR(inode->i_rdev))
	{
		case DSP_OMX_MINOR	: filp->f_op = &gst_dsp_omx_fops; break;
		case DSP_LPA_MINOR	: filp->f_op = &gst_dsp_lpa_fops; break;
		case DSP_PCM_MINOR	: filp->f_op = &gst_dsp_pcm_fops; break;
		case DSP_ALSA_MINOR	: filp->f_op = &gst_dsp_alsa_fops; break;

		default	: return -ENXIO;
	}

	if (filp->f_op && filp->f_op->release)
	{
		return filp->f_op->release(inode,filp);
	}
	
	return 0;
}

static struct
file_operations gst_dsp_fops =
{
	.owner = THIS_MODULE,
	.open 	= dsp_open,
	.release= dsp_close,
};

/**
 *
 * probing module.
 *
 * @param	struct platform_device *pdev
 * @return	int 0 : OK , -1 : NOT OK
 *
 */
static int
dsp_probe(struct platform_device *pdev)
{
	int ret = 0;

#if defined(WITH_BINARY)
	void __iomem	*aud_crg_base;
#endif
	ret = dsp_init_function();

	ret = dsp_register_device(pdev);	
	if (ret<0)
	{
		printk("Can't register device\n");
	}

	/* Mailbox Init */
	hal_mailbox_init();

	memset(&workqueue_flag, 0x0, sizeof(WORKQUEUE_FLAG));
	spin_lock_init(&workqueue_flag.flag_lock);

#if defined(WITH_BINARY)
	aud_crg_base = ioremap(0x34670000, 0x100);
	if (!aud_crg_base) {
		printk("audio_crg ioremap failed\n");
		return -ENOMEM;
	}
	__raw_writel(0xffffff80, aud_crg_base + 0x100);

	odin_pd_on(PD_AUD, 0);

	__raw_writel(0xffffffff, aud_crg_base + 0x100);

	iounmap(aud_crg_base);

	ret = dsp_download_dram();		
	
	ret = dsp_download_iram();

	ret = dsp_power_on();
#endif


#ifdef CONFIG_PM_WAKELOCKS
	snprintf(odin_wake_lock_name, sizeof(odin_wake_lock_name),
		"odin-lpa");
	wake_lock_init(&odin_wake_lock, WAKE_LOCK_SUSPEND,
		odin_wake_lock_name);
#endif

	return 0;
}

static int
dsp_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct clk *dsp_clk;
#if 0	
	if ((atomic_read(&lpa_status)!=LPA_PLAY)||(atomic_read(&lpa_status)!=LPA_OFF))
	{
		dsp_clk = clk_get(NULL, "p_cclk_dsp");

		clk_disable_unprepare(dsp_clk);
	}
#endif
	return 0;
}

#if 0
static int
dsp_remove(struct platform_device *pdev)
{

	dsp_unregister_device();
	
	class_remove_file(dsp_dev_class, &class_attr_dsp_status);
	
	platform_driver_unregister(&dsp_driver);

	unregister_chrdev(DSP_MAJOR,DSP_MODULE);
	
	return 0;
}
#endif
static int
dsp_resume(struct platform_device *pdev)
{
	struct clk *dsp_clk;
	int ret;

	if (atomic_read(&lpa_status)==LPA_PLAY)
	{

#ifdef CONFIG_PM_WAKELOCKS
		wake_lock(&odin_wake_lock);
#if 1
		printk("lpa_wake_lock\n");
#endif
#endif
	}
	else
	{
#if 0
		dsp_clk = clk_get(NULL, "p_cclk_dsp");

		ret = clk_prepare_enable(dsp_clk);
		if (ret)
		{
			printk("cclk_dsp enable failed : %d\n", ret);
		}
#endif
	}
	
	return 0;
}


/*
 *	module platform driver structure
 */

static struct of_device_id dsp_match[] = {
#if defined(CONFIG_WODEN_DSP)
	{ .compatible = "tensilica,woden-dsp", },
#elif defined(CONFIG_ODIN_DSP)
	{ .compatible = "tensilica,odin-dsp", },
#endif
	{ }
};

static struct
platform_driver dsp_driver =
{
	.probe          = dsp_probe,
	.suspend        = dsp_suspend,
#if 0
	.remove         = dsp_remove,
#endif
	.resume         = dsp_resume,
	.driver         =
	{
		.name   = DSP_MODULE,
		.of_match_table = dsp_match,
	},
};

static int __init
dsp_init(void)
{
	int result;
	struct device *dev;
	struct clk *dsp_clk;
	int ret;

#if defined(CONFIG_WODEN_DSP)
	pst_dsp_mem_cfg = &dsp_mem_cfg[0];
#elif defined(CONFIG_ODIN_DSP)
	pst_dsp_mem_cfg = &dsp_mem_cfg[1];
#endif

	result = register_chrdev(DSP_MAJOR,DSP_MODULE, &gst_dsp_fops);

	if (result < 0)
	{
		return result;
	}

#if defined(CONFIG_WODEN_DSP)
	dsp_dev_class = class_create(THIS_MODULE, "woden_dsp");
	if (IS_ERR(dsp_dev_class)) {
		pr_err("%s: Failed to create class(woden_dsp)\n", __func__);
	}
#elif defined(CONFIG_ODIN_DSP)
	dsp_dev_class = class_create(THIS_MODULE, "odin_dsp");
	if (IS_ERR(dsp_dev_class)) {
		pr_err("%s: Failed to create class(odin_dsp)\n", __func__);
	}
#endif

	ret = class_create_file(dsp_dev_class, &class_attr_dsp_status);
	if (ret)
	{
		printk("failed to create file for dsp_status\n");
	}

	dev = device_create(dsp_dev_class,
			NULL,
			MKDEV(DSP_MAJOR, DSP_OMX_MINOR),
			NULL,
			"dsp_omx");

	if (IS_ERR(dev))
	{
		printk("[%s] Failed to device_create\n",__func__);
	}

	dev = device_create(dsp_dev_class,
			NULL,
			MKDEV(DSP_MAJOR, DSP_LPA_MINOR),
			NULL,
			"dsp_lpa");

	if (IS_ERR(dev))
	{
		printk("[%s] Failed to device_create\n",__func__);
	}

	dev = device_create(dsp_dev_class,
			NULL,
			MKDEV(DSP_MAJOR, DSP_PCM_MINOR),
			NULL,
			"dsp_pcm");

	if (IS_ERR(dev))
	{
		printk("[%s] Failed to device_create\n",__func__);
	}

	dev = device_create(dsp_dev_class,
			NULL,
			MKDEV(DSP_MAJOR, DSP_ALSA_MINOR),
			NULL,
			"dsp_alsa");

	if (IS_ERR(dev))
	{
		printk("[%s] Failed to device_create\n",__func__);
	}

	if (platform_driver_register(&dsp_driver) < 0)
	{
		printk("[%s] platform driver register failed\n", DSP_MODULE);
	}
	else
	{
		printk("[%s] platform driver register done\n", DSP_MODULE);
	}

#if 0
	dsp_clk = clk_get(NULL, "cclk_dsp");

	ret = clk_prepare_enable(dsp_clk);
	if (ret)
	{
		dev_err(dev, "clk_enable failed : %d\n", ret);
	}
#endif

	printk("[%s] Audio DSP driver init done\n",DSP_MODULE);

	return 0;
}

static void __exit
dsp_exit(void)
{
#if defined(WITH_BINARY)
	void __iomem	*aud_crg_base;
	
	dsp_power_off();
#endif

#ifdef CONFIG_PM_WAKELOCKS
	wake_lock_destroy(&odin_wake_lock);
#endif

	hal_mailbox_exit();
	
	dsp_unregister_device();
	
	class_remove_file(dsp_dev_class, &class_attr_dsp_status);
	
	platform_driver_unregister(&dsp_driver);

	unregister_chrdev(DSP_MAJOR,DSP_MODULE);

#if defined(WITH_BINARY)
	aud_crg_base = ioremap(0x34670000, 0x100);
	if (!aud_crg_base) {
		printk("audio_crg ioremap failed\n");
		return -ENOMEM;
	}

	odin_pd_off(PD_AUD, 0);
	
	__raw_writel(0xffffff80, aud_crg_base + 0x100);
	
	iounmap(aud_crg_base);
#endif

	return;
}

module_init(dsp_init);
module_exit(dsp_exit);

MODULE_AUTHOR("Heewan Park <heewan.park@lge.com>");
MODULE_DESCRIPTION("Audio DSP driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DSP_MODULE);

