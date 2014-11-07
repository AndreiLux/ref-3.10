/*
 *  Real Time Clock driver for Dialog D2260 PMIC
 *
 *  Copyright (C) 2013 Dialog Semiconductor Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <linux/mfd/d2260/pmic.h>
#include <linux/mfd/d2260/registers.h>
#include <linux/mfd/d2260/core.h>

struct setSrAlarmData_t {
	bool            is_enabled;
	unsigned long   real_alarm;
};

#if defined(CONFIG_MACH_ODIN_LIGER)
#if 1
#define D2260_YEAR_BASE		100	/* 2000 - 1900 */
#define SEC_YEAR_BASE 		14  /* 2014 */
#else
#define D2260_YEAR_BASE		70	/* 1970 - 1900 */
#define SEC_YEAR_BASE 		44  /* 2014 */
#endif
#else
#define D2260_YEAR_BASE		100	/*2000 - 1900 */
#define SEC_YEAR_BASE 		13  /* 2013 */
#endif

static struct rtc_time init_rtc_time =
{
	.tm_sec   = 0xca,
	.tm_min   = 0xfe,
	.tm_hour  = 0xca,
	.tm_mday  = 0xfe,
	.tm_mon   = 0xca,
	.tm_year  = 0xfe,
	.tm_wday  = 0xca,
	.tm_yday  = 0xfe,
	.tm_isdst = 0xca
};

struct d2260_rtc {
	struct d2260 *d2260;
	struct platform_device  *pdev;
	struct rtc_device       *rtc;
	int irq;
	int alarm_enabled;      /* used over suspend/resume */
};

static int d2260_rtc_check_param(struct rtc_time *rtc_tm)
{
	if ((rtc_tm->tm_sec > D2260_RTC_SECONDS_LIMIT) || (rtc_tm->tm_sec < 0))
		return -D2260_RTC_INVALID_SECONDS;

	if ((rtc_tm->tm_min > D2260_RTC_MINUTES_LIMIT) || (rtc_tm->tm_min < 0))
		return -D2260_RTC_INVALID_MINUTES;

	if ((rtc_tm->tm_hour > D2260_RTC_HOURS_LIMIT) || (rtc_tm->tm_hour < 0))
		return -D2260_RTC_INVALID_HOURS;

	if (rtc_tm->tm_mday == 0)
		return -D2260_RTC_INVALID_DAYS;

	if ((rtc_tm->tm_mon > D2260_RTC_MONTHS_LIMIT) || (rtc_tm->tm_mon <= 0))
		return -D2260_RTC_INVALID_MONTHS;

	if ((rtc_tm->tm_year > D2260_RTC_YEARS_LIMIT) || (rtc_tm->tm_year < 0))
		return -D2260_RTC_INVALID_YEARS;

	return 0;
}

static int d2260_rtc_tm_read(struct d2260 *d2260, struct rtc_time *tm,
		u16 reg_base)
{
	const int NUM_REGS = 6;
	int ret = 0;
	int i;

	if (d2260_single_set() == 0) {
		unsigned int d2260_rtc_time[2];

		ret = d2260_group_read(d2260, reg_base, NUM_REGS,
			d2260_rtc_time);
		if (ret < 0)
			return ret;

		tm->tm_sec = d2260_rtc_time[0];
		tm->tm_min = d2260_rtc_time[0] >> D2260_SHIFT_8;
		tm->tm_hour = d2260_rtc_time[0] >> D2260_SHIFT_16;
		tm->tm_mday = d2260_rtc_time[0] >> D2260_SHIFT_24;
		tm->tm_mon = d2260_rtc_time[1];
		tm->tm_year = d2260_rtc_time[1] >> D2260_SHIFT_8;
	}
	else if (d2260_single_set() == 1) {
		u16 d2260_rtc_time[NUM_REGS];

		for (i = 0; i < NUM_REGS; i++) {
			ret = d2260_reg_read(d2260, (reg_base + i),
				&d2260_rtc_time[i]);
			if (ret < 0)
				return ret;
		}

		tm->tm_sec = d2260_rtc_time[0];
		tm->tm_min = d2260_rtc_time[1];
		tm->tm_hour = d2260_rtc_time[2];
		tm->tm_mday = d2260_rtc_time[3];
		tm->tm_mon = d2260_rtc_time[4];
		tm->tm_year = d2260_rtc_time[5];
	}

	tm->tm_sec &= D2260_RTC_SECS_MASK;
	tm->tm_min &= D2260_RTC_MINS_MASK;
	tm->tm_hour &= D2260_RTC_HRS_MASK;
	tm->tm_mday &= D2260_RTC_DAY_MASK;
	tm->tm_mon &= D2260_RTC_MTH_MASK;
	tm->tm_year &= D2260_RTC_YRS_MASK;

	return ret;
}

static int d2260_rtc_tm_write(struct d2260 *d2260, u8 *time, u16 reg_base)
{
	const int NUM_REGS = 6;
	int ret = 0;
	int i;

	if (d2260_single_set() == 0) {
		unsigned int d2260_rtc_time[2];

		d2260_rtc_time[0] = time[0] |
				time[1] << D2260_SHIFT_8 |
				time[2] << D2260_SHIFT_16 |
				time[3] << D2260_SHIFT_24;
		d2260_rtc_time[1] = time[4] |
				time[5] << D2260_SHIFT_8;

		ret = d2260_group_write(d2260, reg_base, NUM_REGS,
			d2260_rtc_time);
		if (ret < 0)
			return ret;
	}
	else if (d2260_single_set() == 1) {
		for (i = 0; i < NUM_REGS; i++) {
			ret = d2260_reg_write(d2260, (reg_base + i), time[i]);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

/*
 * Read current time and date in RTC
 */
static int d2260_rtc_readtime(struct device *dev, struct rtc_time *tm)
{
	struct d2260_rtc *rtc = dev_get_drvdata(dev);
	struct d2260 *d2260 = rtc->d2260;
	int ret = 0;

	ret = d2260_rtc_tm_read(d2260, tm, D2260_COUNT_S_REG);
	if (ret < 0) {
		dev_err(d2260->dev, "%s: read error %d\n", __func__, ret);
		return ret;
	}

	/* sanity checking */
	ret = d2260_rtc_check_param(tm);
	if (ret) {
		dev_err(d2260->dev, "%s : check error %d\n", __func__, ret);
		return ret;
	}

	tm->tm_year += D2260_YEAR_BASE;
	tm->tm_mon -= 1;
	tm->tm_yday = rtc_year_days(tm->tm_mday, tm->tm_mon, tm->tm_year);

	printk("d2260_rtc_readtime RTC reg. %02x-%02x-%02x %02x:%02x:%02x\n",
		tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	return ret;
}

#if defined(CONFIG_RTC_INTF_SECCLK)
extern int secclk_rtc_changed(int (*fp_read_rtc)(struct device *, struct rtc_time *), struct device *dev, struct rtc_time *tm);
#endif

/*
 * Set current time and date in RTC
 */
static int d2260_rtc_settime(struct device *dev, struct rtc_time *tm)
{
	struct d2260_rtc *rtc = dev_get_drvdata(dev);
	struct d2260 *d2260 = rtc->d2260;
	u8 rtc_time[6];
	int ret = 0;

#if defined(CONFIG_RTC_INTF_SECCLK)
	dev_dbg(d2260->dev, "adjust secclk in d2260_rtc_settime\n");
	secclk_rtc_changed(d2260_rtc_readtime, dev, tm);
#endif

	rtc_time[0] = tm->tm_sec;
	rtc_time[1] = tm->tm_min;
	rtc_time[2] = tm->tm_hour;
	rtc_time[3] = tm->tm_mday;
	rtc_time[4] = tm->tm_mon + 1;
#if defined(CONFIG_MACH_ODIN_LIGER)
	rtc_time[5] = tm->tm_year > D2260_YEAR_BASE ? (tm->tm_year - D2260_YEAR_BASE) : 0;
#else
	rtc_time[5] = tm->tm_year - D2260_YEAR_BASE;
#endif
	rtc_time[5] |= D2260_MONITOR_MASK;

	ret = d2260_rtc_tm_write(d2260, rtc_time, D2260_COUNT_S_REG);
	if (ret < 0) {
		dev_err(d2260->dev, "%s: write error %d\n", __func__, ret);
	}

	printk("d2260_rtc_settime RTC reg. %02x-%02x-%02x %02x:%02x:%02x\n",
		tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	return 0;
}

static int d2260_rtc_stop_alarm(struct d2260 *d2260)
{
	int ret;

	/* Set RTC_SET to stop the clock */
	ret = d2260_clear_bits(d2260, D2260_ALARM_Y_REG, D2260_ALARM_ON_MASK);
	if (ret < 0)
		return ret;

	return 0;
}

static int d2260_rtc_start_alarm(struct d2260 *d2260)
{
	int ret;

	ret = d2260_set_bits(d2260, D2260_ALARM_Y_REG, D2260_ALARM_ON_MASK);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * Read alarm time and date in RTC
 */
static int d2260_rtc_readalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct d2260_rtc *rtc = dev_get_drvdata(dev);
	struct d2260 *d2260 = rtc->d2260;
	struct rtc_time *tm = &alrm->time;
	int ret = 0;

	d2260_rtc_tm_read(d2260, tm, D2260_ALARM_S_REG);

	ret = d2260_rtc_check_param(tm);
	if (ret < 0) {
		dev_err(d2260->dev, "%s : check error %d\n", __func__, ret);
		return ret;
	}

	tm->tm_year += D2260_YEAR_BASE;
	tm->tm_mon -= 1;

	printk("d2260_rtc_readalarm RTC reg. %02x-%02x-%02x %02x:%02x:%02x\n",
		tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	return 0;
}

static int d2260_rtc_setalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct d2260_rtc *rtc = dev_get_drvdata(dev);
	struct d2260 *d2260 = rtc->d2260;
	struct rtc_time alarm_tm;
	u8 rtc_ctrl, reg_val;
	u8 time[6];
	int ret = 0;

	/* Set RTC_SET to stop the clock */
	ret = d2260_clear_bits(d2260, D2260_ALARM_Y_REG, D2260_ALARM_ON_MASK);
	if (ret < 0) {
		dev_err(d2260->dev, "%s: clear bits error %d\n", __func__, ret);
		return ret;
	}

	memcpy(&alarm_tm, &alrm->time, sizeof(struct rtc_time));

#if defined(CONFIG_MACH_ODIN_LIGER)
	alarm_tm.tm_year = alarm_tm.tm_year > D2260_YEAR_BASE ? (alarm_tm.tm_year - D2260_YEAR_BASE) : 0;
#else
	alarm_tm.tm_year -= D2260_YEAR_BASE;
#endif
	alarm_tm.tm_mon += 1;

	ret = d2260_rtc_check_param(&alarm_tm);
	if (ret < 0) {
		dev_err(d2260->dev, "%s : check error %d\n", __func__, ret);
		return ret;
	}

	memset(time, 0, sizeof(time));

	time[0] = alarm_tm.tm_sec;
	ret = d2260_reg_read(d2260, D2260_ALARM_MI_REG, &reg_val);
	if (ret < 0) {
		dev_err(d2260->dev, "%s : check error %d\n", __func__, ret);
		return ret;
	}

	rtc_ctrl = reg_val & (~D2260_RTC_ALMMINS_MASK);
	time[1] = rtc_ctrl | alarm_tm.tm_min;
	time[2] |= alarm_tm.tm_hour;
	time[3] |= alarm_tm.tm_mday;
	time[4] |= alarm_tm.tm_mon;
	ret = d2260_reg_read(d2260, D2260_ALARM_Y_REG, &reg_val);
	if (ret < 0) {
		dev_err(d2260->dev, "%s : check error %d\n", __func__, ret);
		return ret;
	}

	rtc_ctrl = reg_val & (~D2260_RTC_ALMYRS_MASK);
	time[5] = rtc_ctrl | alarm_tm.tm_year;

	ret = d2260_rtc_tm_write(d2260, time, D2260_ALARM_S_REG);
	if (ret < 0) {
		dev_err(d2260->dev, "%s: write error %d\n", __func__, ret);
		return ret;
	}

	if (alrm->enabled)
		ret = d2260_rtc_start_alarm(d2260);

	printk("%s : RTC reg. %02x-%02x-%02x %02x:%02x:%02x\n",
		__func__, time[5], time[4], time[3], time[2], time[1], time[0]);

	return ret;
}

static int d2260_rtc_alarm_irq_enable(struct device *dev,
			unsigned int enabled)
{
	struct d2260_rtc *rtc = dev_get_drvdata(dev);
	struct d2260 *d2260 = rtc->d2260;

	printk("%s \n", __func__);

	if (enabled)
		return d2260_rtc_start_alarm(d2260);
	else
		return d2260_rtc_stop_alarm(d2260);
}

static irqreturn_t d2260_rtc_timer_alarm_handler(int irq, void *data)
{
	struct d2260_rtc *rtc = data;
	struct d2260 *d2260 = rtc->d2260;

	rtc_update_irq(rtc->rtc, 1, RTC_AF | RTC_IRQF);
	printk("\nRTC: TIMER ALARM\n");
	return IRQ_HANDLED;
}

static const struct rtc_class_ops d2260_rtc_ops = {
	.read_time = d2260_rtc_readtime,
	.set_time = d2260_rtc_settime,
	.read_alarm = d2260_rtc_readalarm,
	.set_alarm = d2260_rtc_setalarm,
	.alarm_irq_enable = d2260_rtc_alarm_irq_enable,
#if 0	/* block */
	.update_irq_enable = d2260_rtc_update_irq_enable,
#endif
};

#ifdef CONFIG_PM
static int d2260_rtc_suspend(struct device *dev)
{
	return 0;
}

static int d2260_rtc_resume(struct device *dev)
{
#if defined(CONFIG_RTC_ANDROID_ALARM_WORKAROUND)
	/* This option selects temporary fix for alarm handling in 'Android'
	 * environment. This option enables code to disable alarm in the
	 * 'resume' handler of RTC driver. In the normal mode,
	 * android handles all alarms in software without using the RTC chip.
	 * Android sets the alarm in the rtc only in the suspend path (by
	 * calling .set_alarm with struct rtc_wkalrm->enabled set to 1).
	 * In the resume path, android tries to disable alarm by calling
	 * .set_alarm with struct rtc_wkalrm->enabled' field set to 0.
	 * But unfortunately, it memsets the rtc_wkalrm struct to 0, which
	 * causes the rtc lib to flag error and control does not reach this
	 * driver. Hence this workaround.
	 */
	d2260_rtc_alarm_irq_enable(dev, 0);
#endif
	return 0;
}

static struct dev_pm_ops d2260_rtc_pm_ops = {
	.suspend = d2260_rtc_suspend,
	.resume = d2260_rtc_resume,
	.thaw = d2260_rtc_resume,
	.restore = d2260_rtc_resume,
	.poweroff = d2260_rtc_suspend,
};
#endif

static void d2260_rtc_time_fixup(struct device *dev)
{
	struct rtc_time current_rtc_time;
	memset(&current_rtc_time, 0 , sizeof(struct rtc_time));

	d2260_rtc_readtime(dev, &current_rtc_time);
	current_rtc_time.tm_year = SEC_YEAR_BASE;
	memcpy(&init_rtc_time, &current_rtc_time, sizeof(struct rtc_time));
	d2260_rtc_settime(dev, &current_rtc_time);
}

static int d2260_rtc_probe(struct platform_device *pdev)
{
	struct d2260 *d2260 = dev_get_drvdata(pdev->dev.parent);
	struct d2260_rtc *rtc;
	int ret;
	u8 reg_val;

	printk("%s(Starting RTC)\n",__FUNCTION__);
	device_init_wakeup(&pdev->dev, 1);

	rtc = devm_kzalloc(&pdev->dev, sizeof(struct d2260_rtc), GFP_KERNEL);
	if (!rtc) {
		return -ENOMEM;
	}

	rtc->d2260 = d2260;
	platform_set_drvdata(pdev, rtc);

#if defined(CONFIG_MACH_ODIN_LIGER)
#if 0 /* Move Date & Time Fixup Func to MSBL */
	ret = d2260_reg_read(d2260, D2260_COUNT_Y_REG,&reg_val);
	if (ret < 0)
		reg_val = 0;

	if (!(reg_val & D2260_MONITOR_MASK)) {
		printk("Fixup RTC\n");
		d2260_set_bits(d2260, D2260_COUNT_Y_REG, D2260_MONITOR_MASK);
		d2260_rtc_time_fixup(&pdev->dev);
	}
#endif
#endif

	/* Factory RTC Test W/A - Enable RTC monitor bit*/
	ret = d2260_reg_read(d2260, D2260_COUNT_S_REG, &reg_val);
	ret = d2260_reg_read(d2260, D2260_COUNT_Y_REG, &reg_val);
	if (ret < 0) {
		printk("%s : D2260_COUNT_Y_REG read error %d\n", __func__, ret);
	} else {
		printk("%s : D2260_COUNT_Y_REG : 0x%02X\n", __func__, reg_val);
		if((reg_val & 0x40)==0) {
			printk("%s : D2260_RTC Monitor Bit is not set => set Monitor Bit\n", __func__);
			d2260_set_bits(d2260, D2260_COUNT_Y_REG, D2260_MONITOR_MASK);
			d2260_rtc_time_fixup(&pdev->dev);
		}
	}

	/* Check Monitor Bit */
	ret = d2260_reg_read(d2260, D2260_COUNT_S_REG, &reg_val);
	ret = d2260_reg_read(d2260, D2260_COUNT_Y_REG, &reg_val);
	if (ret < 0) {
		printk("%s : D2260_COUNT_Y_REG_2ND read error %d\n", __func__, ret);
	} else {
		printk("%s : D2260_COUNT_Y_REG_2ND : 0x%02X\n", __func__, reg_val);
	}

	rtc->irq =  D2260_IRQ_EALARM;
	ret = d2260_request_irq(rtc->d2260, rtc->irq, "ALM",
			d2260_rtc_timer_alarm_handler, rtc);
	if (ret < 0) {
		dev_err(d2260->dev,
		"Failed to register RTC Timer Alarm IRQ: %d\n", ret);
		goto err;
	}

	rtc->rtc = rtc_device_register(pdev->name, &pdev->dev,
		&d2260_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc)) {
		ret = PTR_ERR(rtc->rtc);
		goto err;
	}

	return ret;

err:
	kfree(rtc);
	printk("%s(RTC registered) ret=%d\n",__FUNCTION__,ret);
	return ret;
}

static int d2260_rtc_remove(struct platform_device *pdev)
{
	struct d2260_rtc *rtc = platform_get_drvdata(pdev);
	struct d2260 *d2260 = rtc->d2260;

	printk("%s(RTC removed)\n",__FUNCTION__);
	rtc_device_unregister(rtc->rtc);

	platform_set_drvdata(pdev, NULL);

	return 0;
}


static struct platform_driver d2260_rtc_driver = {
	.probe = d2260_rtc_probe,
	.remove = d2260_rtc_remove,
	.driver = {
		.name = "d2260-rtc",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &d2260_rtc_pm_ops,
#endif
	},
};

static int __init d2260_rtc_init(void)
{
	return platform_driver_register(&d2260_rtc_driver);
}
subsys_initcall(d2260_rtc_init);

static void __exit d2260_rtc_exit(void)
{
	platform_driver_unregister(&d2260_rtc_driver);
}
module_exit(d2260_rtc_exit);

MODULE_AUTHOR("William Seo <william.seo@diasemi.com>");
MODULE_AUTHOR("Tony Olech <anthony.olech@diasemi.com>");
MODULE_DESCRIPTION("RTC driver for the Dialog d2260 PMIC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:d2260-rtc");

