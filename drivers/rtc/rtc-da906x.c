/*
 * Real Time Clock driver for DA906x PMIC family
 *
 * Copyright 2012 Dialog Semiconductors Ltd.
 *
 * Author: Krystian Garbaciak <krystian.garbaciak@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mfd/da906x/registers.h>
#include <linux/mfd/da906x/core.h>

#define YEARS_TO_DA906X(year)		((year) - 100)
#define MONTHS_TO_DA906X(month)		((month) + 1)
#define YEARS_FROM_DA906X(year)		((year) + 100)
#define MONTHS_FROM_DA906X(month)	((month) - 1)

#define CLOCK_DATA_LEN	(DA906X_REG_COUNT_Y - DA906X_REG_COUNT_S + 1)
#define ALARM_DATA_LEN	(DA906X_REG_ALARM_Y - DA906X_REG_ALARM_MI + 1)
enum {
	DATA_SEC = 0,
	DATA_MIN,
	DATA_HOUR,
	DATA_DAY,
	DATA_MONTH,
	DATA_YEAR,
};

struct da906x_rtc {
	struct rtc_device	*rtc_dev;
	struct da906x		*hw;
	int			irq_alarm;
	int			irq_tick;

	/* Config flag */
	int			tick_wake;

	/* Used to expand alarm precision onto seconds and to support ticks */
	unsigned int		alarm_secs;
	unsigned int		alarm_ticks;
};

static void da906x_data_to_tm(u8 *data, struct rtc_time *tm)
{
	tm->tm_sec = data[DATA_SEC] & DA906X_COUNT_SEC_MASK;
	tm->tm_min = data[DATA_MIN] & DA906X_COUNT_MIN_MASK;
	tm->tm_hour = data[DATA_HOUR] & DA906X_COUNT_HOUR_MASK;
	tm->tm_mday = data[DATA_DAY] & DA906X_COUNT_DAY_MASK;
	tm->tm_mon = MONTHS_FROM_DA906X(data[DATA_MONTH] &
					 DA906X_COUNT_MONTH_MASK);
	tm->tm_year = YEARS_FROM_DA906X(data[DATA_YEAR] &
					 DA906X_COUNT_YEAR_MASK);
} static void da906x_tm_to_data(struct rtc_time *tm, u8 *data)
{
	data[DATA_SEC] &= ~DA906X_COUNT_SEC_MASK;
	data[DATA_SEC] |= tm->tm_sec & DA906X_COUNT_SEC_MASK;
	data[DATA_MIN] &= ~DA906X_COUNT_MIN_MASK;
	data[DATA_MIN] |= tm->tm_min & DA906X_COUNT_MIN_MASK;
	data[DATA_HOUR] &= ~DA906X_COUNT_HOUR_MASK;
	data[DATA_HOUR] |= tm->tm_hour & DA906X_COUNT_HOUR_MASK;
	data[DATA_DAY] &= ~DA906X_COUNT_DAY_MASK;
	data[DATA_DAY] |= tm->tm_mday & DA906X_COUNT_DAY_MASK;
	data[DATA_MONTH] &= ~DA906X_COUNT_MONTH_MASK;
	data[DATA_MONTH] |= MONTHS_TO_DA906X(tm->tm_mon) &
			    DA906X_COUNT_MONTH_MASK;
	data[DATA_YEAR] &= ~DA906X_COUNT_YEAR_MASK;
	data[DATA_YEAR] |= YEARS_TO_DA906X(tm->tm_year) &
			   DA906X_COUNT_YEAR_MASK;
}

#define DA906X_ALARM_DELAY	INT_MAX
static int da906x_rtc_test_delay(struct rtc_time *alarm, struct rtc_time *cur)
{
	unsigned long a_time, c_time;

	rtc_tm_to_time(alarm, &a_time);
	rtc_tm_to_time(cur, &c_time);

	/* Alarm time has already passed */
	if (a_time < c_time)
		return -1;

	/* If alarm is set for current minute, return ticks to count down.
	   to set alarm first.
	   But when it is less than 2 seconds for the first one to become true,
	   return ticks, cause alarm may need too much time to synchronise. */
	if (a_time - c_time < alarm->tm_sec + 2)
		return a_time - c_time;
	else
		return DA906X_ALARM_DELAY;
}

static int da906x_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct da906x_rtc *rtc = dev_get_drvdata(dev);
	u8 data[CLOCK_DATA_LEN];
	int ret;

	data[DATA_SEC] = da906x_reg_read(rtc->hw, DA906X_REG_COUNT_S);
	data[DATA_MIN] = da906x_reg_read(rtc->hw, DA906X_REG_COUNT_MI);
	data[DATA_HOUR] = da906x_reg_read(rtc->hw, DA906X_REG_COUNT_H);
	data[DATA_DAY] = da906x_reg_read(rtc->hw, DA906X_REG_COUNT_D);
	data[DATA_MONTH] = da906x_reg_read(rtc->hw, DA906X_REG_COUNT_MO);
	data[DATA_YEAR] = da906x_reg_read(rtc->hw, DA906X_REG_COUNT_Y);

	/* Check, if RTC logic is initialised */
	if (!(data[DATA_SEC] & DA906X_RTC_READ))
		return -EBUSY;

	da906x_data_to_tm(data, tm);

	return rtc_valid_tm(tm);
}

static int da906x_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct da906x_rtc *rtc = dev_get_drvdata(dev);
	u8 data[CLOCK_DATA_LEN] = { [0 ... (CLOCK_DATA_LEN - 1)] = 0 };
	int ret;

	da906x_tm_to_data(tm, data);

	ret = da906x_block_write(rtc->hw,
				 DA906X_REG_COUNT_S, CLOCK_DATA_LEN, data);

	return ret;
}

static int da906x_rtc_set_time_temp(struct device *dev, struct rtc_time *tm)
{
	struct da906x_rtc *rtc = dev_get_drvdata(dev);
	u8 data[CLOCK_DATA_LEN] = { [0 ... (CLOCK_DATA_LEN - 1)] = 0 };

	da906x_tm_to_data(tm, data);

	/* set test time */
	da906x_reg_write(rtc->hw, DA906X_REG_COUNT_S, data[DATA_SEC]);
	da906x_reg_write(rtc->hw, DA906X_REG_COUNT_MI, data[DATA_MIN]);
	da906x_reg_write(rtc->hw, DA906X_REG_COUNT_H, data[DATA_HOUR]);
	da906x_reg_write(rtc->hw, DA906X_REG_COUNT_D, data[DATA_DAY]);
	da906x_reg_write(rtc->hw, DA906X_REG_COUNT_MO, data[DATA_MONTH]);
	da906x_reg_write(rtc->hw, DA906X_REG_COUNT_Y, data[DATA_YEAR]);

	return 0;
}

static int da906x_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct da906x_rtc *rtc = dev_get_drvdata(dev);
	u8 data[CLOCK_DATA_LEN];
	int ret;

	ret = da906x_block_read(rtc->hw, DA906X_REG_ALARM_MI, ALARM_DATA_LEN,
				&data[DATA_MIN]);
	if (ret < 0)
		return ret;

	da906x_data_to_tm(data, &alrm->time);
	alrm->time.tm_sec = rtc->alarm_secs;
	alrm->enabled = !!(data[DATA_YEAR] & DA906X_ALARM_ON);

	/* If there is no ticks left to count down and RTC event is
	   not processed yet, indicate pending */
	if (rtc->alarm_ticks == 0) {
		ret = da906x_reg_read(rtc->hw, DA906X_REG_EVENT_A);
		if (ret < 0)
			return ret;
		if (ret & (DA906X_E_ALARM | DA906X_E_TICK))
			alrm->pending = 1;
	} else {
		alrm->pending = 0;
	}

	return 0;
}

static int da906x_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct da906x_rtc *rtc = dev_get_drvdata(dev);
	u8 data[CLOCK_DATA_LEN] = { [0 ... (CLOCK_DATA_LEN - 1)] = 0 };
	struct rtc_time cur_tm;
	int cmp_val;
	int ret;

	data[DATA_MIN] = DA906X_ALARM_STATUS_ALARM;
	data[DATA_MONTH] = DA906X_TICK_TYPE_SEC;
	if (rtc->tick_wake)
		data[DATA_MONTH] |= DA906X_TICK_WAKE;

	ret = da906x_rtc_read_time(dev, &cur_tm);
	if (ret < 0)
		return ret;

	if (alrm->enabled) {
		/* Decide, whether to set alarm or enable ticks. */
		cmp_val = da906x_rtc_test_delay(&alrm->time, &cur_tm);
		if (cmp_val == 0) {
			rtc_update_irq(rtc->rtc_dev, 1, RTC_IRQF | RTC_AF);
		} else if (cmp_val > 0 && cmp_val < DA906X_ALARM_DELAY) {
			rtc->alarm_ticks = cmp_val - 1;
			data[DATA_YEAR] |= DA906X_TICK_ON;
		} else {
			data[DATA_YEAR] |= DA906X_ALARM_ON;
		}
	}

	da906x_tm_to_data(&alrm->time, data);
	rtc->alarm_secs = alrm->time.tm_sec;

	return da906x_block_write(rtc->hw, DA906X_REG_ALARM_MI, ALARM_DATA_LEN,
				 &data[DATA_MIN]);
}

static int da906x_rtc_set_alarm_temp(struct device *dev,
		struct rtc_wkalrm *alrm)
{
	struct da906x_rtc *rtc = dev_get_drvdata(dev);
	u8 data[CLOCK_DATA_LEN] = { [0 ... (CLOCK_DATA_LEN - 1)] = 0 };
	struct rtc_time cur_tm;
	int cmp_val;
	int ret;

	data[DATA_MIN] = DA906X_ALARM_STATUS_ALARM;
	data[DATA_MONTH] = DA906X_TICK_TYPE_SEC;
	if (rtc->tick_wake)
		data[DATA_MONTH] |= DA906X_TICK_WAKE;

	ret = da906x_rtc_read_time(dev, &cur_tm);
	if (ret < 0)
		return ret;

	if (alrm->enabled) {
		/* Decide, whether to set alarm or enable ticks. */
		cmp_val = da906x_rtc_test_delay(&alrm->time, &cur_tm);
		if (cmp_val == 0) {
			rtc_update_irq(rtc->rtc_dev, 1, RTC_IRQF | RTC_AF);
		} else if (cmp_val > 0 && cmp_val < DA906X_ALARM_DELAY) {
			rtc->alarm_ticks = cmp_val - 1;
			data[DATA_YEAR] |= DA906X_TICK_ON;
		} else {
			data[DATA_YEAR] |= DA906X_ALARM_ON;
		}
	}

	da906x_tm_to_data(&alrm->time, data);
	rtc->alarm_secs = alrm->time.tm_sec;

	da906x_reg_write(rtc->hw, DA906X_REG_ALARM_MI, data[DATA_MIN]);
	da906x_reg_write(rtc->hw, DA906X_REG_ALARM_H, data[DATA_HOUR]);
	da906x_reg_write(rtc->hw, DA906X_REG_ALARM_D, data[DATA_DAY]);
	da906x_reg_write(rtc->hw, DA906X_REG_ALARM_MO, data[DATA_MONTH]);
	da906x_reg_write(rtc->hw, DA906X_REG_ALARM_Y, data[DATA_YEAR]);

	return ret;
}

static int da906x_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct da906x_rtc *rtc = dev_get_drvdata(dev);
	struct rtc_wkalrm alrm;
	int ret;

	ret = da906x_reg_read(rtc->hw, DATA_YEAR);
	if (ret < 0)
		return ret;

	if (enabled) {
		/* Enable alarm, if it is not enabled already */
		if (!(ret & (DA906X_ALARM_ON | DA906X_TICK_ON))) {
			ret = da906x_rtc_read_alarm(dev, &alrm);
			if (ret < 0)
				return ret;

			alrm.enabled = 1;
			ret = da906x_rtc_set_alarm(dev, &alrm);
		}
	} else {
		ret = da906x_reg_clear_bits(rtc->hw, DA906X_REG_ALARM_Y,
					    DA906X_ALARM_ON);
	}

	return ret;
}

static irqreturn_t da906x_alarm_event(int irq, void *data)
{
	struct da906x_rtc *rtc = data;

	if (rtc->alarm_secs) {
		rtc->alarm_ticks = rtc->alarm_secs - 1;
		da906x_reg_update(rtc->hw, DA906X_REG_ALARM_Y,
				  DA906X_ALARM_ON | DA906X_TICK_ON,
				  DA906X_TICK_ON);
	} else {
		da906x_reg_clear_bits(rtc->hw, DA906X_REG_ALARM_Y,
				      DA906X_ALARM_ON);
		rtc_update_irq(rtc->rtc_dev, 1, RTC_IRQF | RTC_AF);
	}

	return IRQ_HANDLED;
}

static irqreturn_t da906x_tick_event(int irq, void *data)
{
	struct da906x_rtc *rtc = data;

	if (rtc->alarm_ticks-- == 0) {
		da906x_reg_clear_bits(rtc->hw,
				      DA906X_REG_ALARM_Y, DA906X_TICK_ON);
		rtc_update_irq(rtc->rtc_dev, 1, RTC_IRQF | RTC_UF);
	}

	return IRQ_HANDLED;
}

static const struct rtc_class_ops da906x_rtc_ops = {
	.read_time = da906x_rtc_read_time,
	.set_time = da906x_rtc_set_time_temp,
	.read_alarm = da906x_rtc_read_alarm,
	.set_alarm = da906x_rtc_set_alarm_temp,
	.alarm_irq_enable = da906x_rtc_alarm_irq_enable,
};

static __init int da906x_rtc_probe(struct platform_device *pdev)
{
	struct da906x *da906x = dev_get_drvdata(pdev->dev.parent);
	struct da906x_rtc *rtc;
	int ret;
	int alarm_mo;

	/* Start RTC in hardware */
	ret = da906x_reg_set_bits(da906x, DA906X_REG_CONTROL_E, DA906X_RTC_EN);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable RTC.\n");
		return ret;
	}

	ret = da906x_reg_set_bits(da906x, DA906X_REG_EN_32K, DA906X_CRYSTAL);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to run 32 KHz OSC.\n");
		return ret;
	}

	ret = da906x_reg_set_bits(da906x, DA906X_REG_IRQ_MASK_B, DA906X_M_WAKE);


	ret = da906x_reg_read(da906x, DA906X_REG_ALARM_MO);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to read RTC register.\n");
		return ret;
	}
	alarm_mo = ret;

	/* Register RTC device */
	rtc = devm_kzalloc(&pdev->dev, sizeof *rtc, GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	platform_set_drvdata(pdev, rtc);

	rtc->hw = da906x;
	rtc->rtc_dev = rtc_device_register(DA906X_DRVNAME_RTC, &pdev->dev,
					   &da906x_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc_dev)) {
		dev_err(&pdev->dev, "Failed to register RTC device: %ld\n",
			PTR_ERR(rtc->rtc_dev));
		return PTR_ERR(rtc->rtc_dev);
	}

	if (alarm_mo & DA906X_TICK_WAKE)
		rtc->tick_wake = 1;

#ifndef CONFIG_MFD_DA906X_IRQ_DISABLE
	/* Register interrupts. Complain on errors but let device
	   to be registered at least for date/time. */
	rtc->irq_alarm = platform_get_irq_byname(pdev, "ALARM");
	ret = request_threaded_irq(rtc->irq_alarm, NULL, da906x_alarm_event,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT, "ALARM", rtc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request ALARM IRQ.\n");
		rtc->irq_alarm = -ENXIO;
		return 0;
	}

	rtc->irq_tick = platform_get_irq_byname(pdev, "TICK");
	ret = request_threaded_irq(rtc->irq_tick, NULL, da906x_tick_event,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, "TICK", rtc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request TICK IRQ.\n");
		rtc->irq_tick = -ENXIO;
	}
#endif

	return 0;
}

static int __exit da906x_rtc_remove(struct platform_device *pdev)
{
	struct da906x_rtc *rtc = platform_get_drvdata(pdev);

	if (rtc->irq_alarm >= 0)
		free_irq(rtc->irq_alarm, rtc);

	if (rtc->irq_tick >= 0)
		free_irq(rtc->irq_tick, rtc);

	rtc_device_unregister(rtc->rtc_dev);
	return 0;
}

static struct platform_driver da906x_rtc_driver = {
	.probe		= da906x_rtc_probe,
	.remove		= da906x_rtc_remove,
	.driver		= {
		.name	= DA906X_DRVNAME_RTC,
		.owner	= THIS_MODULE,
	},
};

static int __init da906x_rtc_init(void)
{
	return platform_driver_register(&da906x_rtc_driver);
}
/*subsys_initcall(da906x_rtc_init);*/

/* Module information */
MODULE_AUTHOR("Krystian Garbaciak <krystian.garbaciak@diasemi.com>");
MODULE_DESCRIPTION("DA906x RTC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DA906X_DRVNAME_RTC);
