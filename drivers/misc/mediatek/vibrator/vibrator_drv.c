/******************************************************************************
 * mt6575_vibrator.c - MT6575 Android Linux Vibrator Device Driver
 *
 * Copyright 2009-2010 MediaTek Co.,Ltd.
 *
 * DESCRIPTION:
 *     This file provid the other drivers vibrator relative functions
 *
 ******************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_pm_ldo.h>
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>
#include <cust_vibrator.h>

#include "../../../staging/android/timed_output.h"

#define VERSION			"v 0.1"
#define VIB_DEVICE		"mtk_vibrator"

#define VIBR_FLG_ENABLE		1
#define VIBR_FLG_INACTIVE	2

struct vibr_ctx {
	struct work_struct work;
	struct timed_output_dev todev;
	struct hrtimer timeout;
	long flags;
	int vib_timer;
};

/* extern void dct_pmic_VIBR_enable(kal_bool dctEnable); */
static void vibr_hw_enable(void)
{
	/* [bit 5]: VIBR_SW_MODE 0=HW, 1=SW */
	upmu_set_rg_vibr_sw_mode(1);

	/* [bit 4-3]: VIBR_FR_ORI, 00=float, 01=forward, 10=braking, 11=backward */
	upmu_set_rg_vibr_fr_ori(1);

	/* enable PMU_VIBR */
	hwPowerOn(MT65XX_POWER_LDO_VIBR, VOL_3300, "VIBR");

	/* [bit 6]: VIBR_PWDB, 1=enable */
	upmu_set_rg_vibr_pwdb(1);
	return;
}

static void vibr_hw_disable(void)
{
	/* disable PMU_VIBR */
	hwPowerDown(MT65XX_POWER_LDO_VIBR, "VIBR");

	/* [bit 6]: VIBR_PWDB, 1=enable */
	upmu_set_rg_vibr_pwdb(0);
	return;
}

static void vibr_enable(struct vibr_ctx *ctx, int value)
{
	ktime_t time;

	if (test_and_set_bit(VIBR_FLG_ENABLE, &ctx->flags))
		return;

#ifdef CUST_VIBR_LIMIT
	if (value > ctx->vib_limit && value < ctx->vib_timer)
#else
	if (value >= 10 && value < ctx->vib_timer)
#endif
		value = ctx->vib_timer;

	value = (value > 15000 ? 15000 : value);
	time = ktime_set(value / 1000, (value % 1000) * 1000000);
	hrtimer_start(&ctx->timeout, time, HRTIMER_MODE_REL);

	vibr_hw_enable();
}

static void vibr_disable(struct vibr_ctx *ctx)
{
	if (!test_and_clear_bit(VIBR_FLG_ENABLE, &ctx->flags))
		return;

	vibr_hw_disable();
}

static ssize_t store_vibr_on(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct vibr_ctx *ctx = dev_get_drvdata(dev);
	int value;

	if (!buf || !size)
		return -EINVAL;

	dev_info(dev, "Buf is %s and size is %d\n", buf, size);

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;

	(value) ? vibr_enable(ctx, value) : vibr_disable(ctx);

	return size;
}
static DEVICE_ATTR(vibr_on, 0220, NULL, store_vibr_on);

static void vibr_timeout(struct work_struct *work)
{
	struct vibr_ctx *ctx = container_of(work, struct vibr_ctx, work);

	vibr_disable(ctx);
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct vibr_ctx *ctx = container_of(dev, struct vibr_ctx, todev);

	if (hrtimer_active(&ctx->timeout)) {
		ktime_t r = hrtimer_get_remaining(&ctx->timeout);
		return ktime_to_ms(r);
	} else
		return 0;
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	struct vibr_ctx *ctx = container_of(dev, struct vibr_ctx, todev);
	int ret;

	ret = hrtimer_cancel(&ctx->timeout);
	if (ret)
		dev_info(dev->dev, "vibrator_enable: try to cancel hrtimer\n");

	if (test_bit(VIBR_FLG_INACTIVE, &ctx->flags) || !value)
		return;

	vibr_enable(ctx, value);
	dev_info(dev->dev, "vibrator_enable: vibrator enable: %d\n", value);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct vibr_ctx *ctx = container_of(timer, struct vibr_ctx, timeout);

	schedule_work(&ctx->work);

	return HRTIMER_NORESTART;
}

static int vib_probe(struct platform_device *pdev)
{
	struct vibrator_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct vibr_ctx *ctx;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->vib_timer = pdata->vib_timer;

	hrtimer_init(&ctx->timeout, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	ctx->timeout.function = vibrator_timer_func;
	ctx->todev.name = "vibrator";
	ctx->todev.get_time = vibrator_get_time;
	ctx->todev.enable = vibrator_enable;
	INIT_WORK(&ctx->work, vibr_timeout);

	dev_set_drvdata(&pdev->dev, ctx);

	ret = timed_output_dev_register(&ctx->todev);
	if (ret)
		goto exit1;

	ret = device_create_file(ctx->todev.dev, &dev_attr_vibr_on);
	if (ret) {
		dev_info(&pdev->dev, "device_create_file vibr_on fail!\n");
		goto exit2;
	}

	return 0;

exit2:
	timed_output_dev_unregister(&ctx->todev);

exit1:
	kfree(ctx);

	return -EFAULT;
}

static int vib_remove(struct platform_device *pdev)
{
	struct vibr_ctx *ctx = dev_get_drvdata(&pdev->dev);

	hrtimer_cancel(&ctx->timeout);

	timed_output_dev_unregister(&ctx->todev);

	return 0;
}

static void vib_shutdown(struct platform_device *pdev)
{
	struct vibr_ctx *ctx = dev_get_drvdata(&pdev->dev);

	set_bit(VIBR_FLG_INACTIVE, &ctx->flags);

	vibr_disable(ctx);
}

static int vib_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct vibr_ctx *ctx = dev_get_drvdata(&pdev->dev);

	set_bit(VIBR_FLG_INACTIVE, &ctx->flags);

	vibr_disable(ctx);

	return 0;
}

static int vib_resume(struct platform_device *pdev)
{
	struct vibr_ctx *ctx = dev_get_drvdata(&pdev->dev);

	clear_bit(VIBR_FLG_INACTIVE, &ctx->flags);

	return 0;
}

static struct platform_driver vibrator_driver = {
	.probe = vib_probe,
	.remove = vib_remove,
	.shutdown = vib_shutdown,
	.suspend = vib_suspend,
	.resume = vib_resume,
	.driver = {
		   .name = VIB_DEVICE,
		   .owner = THIS_MODULE,
		   },
};

module_platform_driver(vibrator_driver);
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MTK Vibrator Driver (VIB)");
MODULE_LICENSE("GPL");
