/* Copyright (c) 2012-13, The Linux Foundation. All rights reserved.
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

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/mfd/max77525/max77525.h>
#include <linux/mfd/max77525/rtc-max77525.h>
#if defined(CONFIG_RTC_ALARM_BOOT)
#include <linux/reboot.h>
#endif
#include <linux/module.h>
#include <linux/of.h>


/* RTC/ALARM Register offsets */
#define REG_RTCINT		0x40
#define REG_RTCINTM		0x41
#define REG_RTCCNTLM	0x42
#define REG_RTCCNTL		0x43
#define REG_RTCUPDATE	0x44
#define REG_RTCSMPL		0x46
#define REG_RTCSEC		0x47
#define REG_RTCMIN		0x48
#define REG_RTCHOUR		0x49
#define REG_RTCDOW		0x4A
#define REG_RTCMONTH	0x4B
#define REG_RTCYEAR		0x4C
#define REG_RTCDOM		0x4D
#define REG_RTCAE1		0x4E
#define REG_RTCSECA1	0x4F
#define REG_RTCMINA1	0x50
#define REG_RTCHOURA1	0x51
#define REG_RTCDOWA1	0x52
#define REG_RTCMONTHA1	0x53
#define REG_RTCYEARA1	0x54
#define REG_RTCDOMA1	0x55
#define REG_RTCAE2		0x56
#define REG_RTCSECA2	0x57
#define REG_RTCMINA2	0x58
#define REG_RTCHOURA2	0x59
#define REG_RTCDOWA2	0x5A
#define REG_RTCMONTHA2	0x5B
#define REG_RTCYEARA2	0x5C
#define REG_RTCDOMA2	0x5D

enum {
	RTC_SEC,
	RTC_MIN,
	RTC_HOUR,
	RTC_DOW,
	RTC_MONTH,
	RTC_YEAR,
	RTC_DOM,
	RTC_NR_TIME
};

/* RTCINT register bit fields */
#define BIT_RTC1S		BIT(4)
#define BIT_SMPL		BIT(3)
#define BIT_RTCA2		BIT(2)
#define BIT_RTCA1		BIT(1)
#define BIT_RTC60S		BIT(0)

/* RTCCNTL register bit fields */
#define BIT_HRMODE		BIT(1)
#define BIT_BCD			BIT(0)

/* RTCUPDATE register bit fields*/
#define BIT_RBUDR		BIT(4)
#define BIT_RTCWAKE		BIT(3)
#define BIT_FREEZ_SEC	BIT(2)
#define BIT_RSVD		BIT(1)
#define BIT_UDR			BIT(0)

/* RTCSMPL register bit fields */
#define SMPL_EN_MASK	0x80
#define SMPPLT_MASK		0x0C

/* RTC register bit field */
#define SEC_MASK		0x7F
#define MIN_MASK		0x7F
#define HOUR_MASK		0x3F
#define AMPM_MASK		0x40
#define DOW_MASK		0x7F
#define BIT_SAT			BIT(6)
#define BIT_FRI			BIT(5)
#define BIT_THU			BIT(4)
#define BIT_WED			BIT(3)
#define BIT_TUH			BIT(2)
#define BIT_MON			BIT(1)
#define BIT_SUN			BIT(0)

#define MONTH_MASK		0x1F
#define YEAR_MASK		0xFF
#define DAY_MASK		0x3F

/* RTC ALARM ENABLE register bit fields */
#define BIT_AEDOM		BIT(6)
#define BIT_AEYEAR		BIT(5)
#define BIT_AEMONTH		BIT(4)
#define BIT_AEDOW		BIT(3)
#define BIT_AEHOUR		BIT(2)
#define BIT_AEMIN		BIT(1)
#define BIT_AESEC		BIT(0)

#define BINARY_MODE		0
#define BCD_MODE		BIT_BCD

#define HRMODE_12		0
#define HRMODE_24		BIT_HRMODE

#define TO_SECS(arr)		(arr[0] | (arr[1] << 8) | (arr[2] << 16) | \
							(arr[3] << 24))

/* argument 'm' should not be zero. */
#define MASK2SHIFT(m) ((m) & 0x0F ? ((m) & 0x03 ? ((m) & 0x01 ? 0 : 1) : \
		((m) & 0x04 ? 2 : 3)) : ((m) & 0x30 ? ((m) & 0x10 ? 4 : 5) : \
		((m) & 0x40 ? 6 : 7)))

enum max77525_rtc_update_op
{
	RTC_WRITE_TO_REG,
	RTC_READ_FROM_REG,
};

#define RTC_UPDATE_DELAY	200
#define RTC_YEAR_BASE		100

/* rtc driver internal structure */
struct max77525_rtc {
	struct regmap	*regmap;
	struct rtc_device *rtc_dev;
	u8 	cache_update;
	int rtc_alarm_irq;
	bool rtc_alarm_powerup;
	spinlock_t alarm_ctrl_lock;
};

static void max77525_rtc_data_to_tm(u8 *data, struct rtc_time *tm)
{
	u8 dow = data[RTC_DOW] & DOW_MASK;

	tm->tm_sec = data[RTC_SEC] & SEC_MASK;
	tm->tm_min = data[RTC_MIN] & MIN_MASK;
	tm->tm_hour = data[RTC_HOUR] & HOUR_MASK;

	tm->tm_wday = MASK2SHIFT(dow);
	tm->tm_mday = data[RTC_DOM] & DAY_MASK;
	tm->tm_mon = (data[RTC_MONTH] & MONTH_MASK) - 1;
	tm->tm_year = (data[RTC_YEAR] & YEAR_MASK) + RTC_YEAR_BASE;
	tm->tm_yday = 0;
	tm->tm_isdst = 0;
}

static int max77525_rtc_tm_to_data(struct rtc_time *tm, u8 *data)
{
	data[RTC_SEC] = tm->tm_sec;
	data[RTC_MIN] = tm->tm_min;
	data[RTC_HOUR] = tm->tm_hour;
	data[RTC_DOW] = 1 << tm->tm_wday;
	data[RTC_DOM] = tm->tm_mday;
	data[RTC_MONTH] = tm->tm_mon + 1;
	data[RTC_YEAR] = tm->tm_year > RTC_YEAR_BASE ?
			(tm->tm_year - RTC_YEAR_BASE) : 0;

	if (tm->tm_hour >= 12)
		data[RTC_HOUR] |= AMPM_MASK;

	if (unlikely(tm->tm_year < 100))
	{
		pr_warn("%s: MAX77235 RTC cannot handle the year %d."
			"Assume it's 2000.\n", __func__, 1900 + tm->tm_year);
		return -EINVAL;
	}
	return 0;
}

static int max77525_rtc_update(struct max77525_rtc *rtc_dd,
		enum max77525_rtc_update_op op)
{
	int ret;
	u8 val = 0;

	switch (op)
	{
	case RTC_WRITE_TO_REG:
		val = BIT_UDR | rtc_dd->cache_update;
		break;
	case RTC_READ_FROM_REG:
		val = BIT_RBUDR | rtc_dd->cache_update;
		break;
	}

	ret = regmap_write(rtc_dd->regmap, REG_RTCUPDATE, val);
	if (IS_ERR_VALUE(ret))
		return ret;

	udelay(RTC_UPDATE_DELAY);
	return ret;
}

static int max77525_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct max77525_rtc *rtc_dd = dev_get_drvdata(dev);
	u8 data[RTC_NR_TIME];
	int ret;

	int i;

	ret = max77525_rtc_tm_to_data(tm, data);

	if (IS_ERR_VALUE(ret))
		return ret;

	regmap_write(rtc_dd->regmap, REG_RTCSEC,  data[RTC_SEC]);
	regmap_write(rtc_dd->regmap, REG_RTCMIN,  data[RTC_MIN]);
	regmap_write(rtc_dd->regmap, REG_RTCHOUR, data[RTC_HOUR]);
	regmap_write(rtc_dd->regmap, REG_RTCDOW,  data[RTC_DOW]);
	regmap_write(rtc_dd->regmap, REG_RTCMONTH,data[RTC_MONTH]);
	regmap_write(rtc_dd->regmap, REG_RTCYEAR, data[RTC_YEAR]);
	regmap_write(rtc_dd->regmap, REG_RTCDOM,  data[RTC_DOM]);

	if (IS_ERR_VALUE(ret))
		return ret;

	ret = max77525_rtc_update(rtc_dd, RTC_WRITE_TO_REG);

	return ret;
}

static int max77525_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct max77525_rtc *rtc_dd = dev_get_drvdata(dev);
	u8 data[RTC_NR_TIME];
	int ret;

	ret = max77525_rtc_update(rtc_dd, RTC_READ_FROM_REG);
	if (IS_ERR_VALUE(ret))
		return ret;

	regmap_read(rtc_dd->regmap, REG_RTCSEC,  &data[RTC_SEC]);
	regmap_read(rtc_dd->regmap, REG_RTCMIN,  &data[RTC_MIN]);
	regmap_read(rtc_dd->regmap, REG_RTCHOUR, &data[RTC_HOUR]);
	regmap_read(rtc_dd->regmap, REG_RTCDOW,  &data[RTC_DOW]);
	regmap_read(rtc_dd->regmap, REG_RTCMONTH,&data[RTC_MONTH]);
	regmap_read(rtc_dd->regmap, REG_RTCYEAR, &data[RTC_YEAR]);
	regmap_read(rtc_dd->regmap, REG_RTCDOM,  &data[RTC_DOM]);
	if (likely(ret >= 0))
	{
		max77525_rtc_data_to_tm(data, tm);
		ret = rtc_valid_tm(tm);
		if (ret) {
			dev_err(dev, "Invalid time read from RTC\n");
			return ret;
		}
	}

	dev_dbg(dev, "h:m:s == %d:%d:%d, d/m/y = %d/%d/%d\n",
			tm->tm_hour, tm->tm_min, tm->tm_sec,
			tm->tm_mday, tm->tm_mon, tm->tm_year);

	return 0;
}

static int max77525_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct max77525_rtc *rtc_dd = dev_get_drvdata(dev);
	u8 data[RTC_NR_TIME];
	u8 val;
	int ret;

	ret = max77525_rtc_tm_to_data(&alarm->time, data);
	if (IS_ERR_VALUE(ret))
		return ret;

	if (alarm->enabled)
		val = BIT_AEDOM | BIT_AEYEAR | BIT_AEMONTH |
			BIT_AEDOW | BIT_AEHOUR | BIT_AEMIN | BIT_AESEC;
	else
		val = 0;

	regmap_write(rtc_dd->regmap, REG_RTCSECA1,  data[RTC_SEC]);
	regmap_write(rtc_dd->regmap, REG_RTCMINA1,  data[RTC_MIN]);
	regmap_write(rtc_dd->regmap, REG_RTCHOURA1, data[RTC_HOUR]);
	regmap_write(rtc_dd->regmap, REG_RTCDOWA1,  data[RTC_DOW]);
	regmap_write(rtc_dd->regmap, REG_RTCMONTHA1,data[RTC_MONTH]);
	regmap_write(rtc_dd->regmap, REG_RTCYEARA1, data[RTC_YEAR]);
	regmap_write(rtc_dd->regmap, REG_RTCDOMA1,  data[RTC_DOM]);
	if (IS_ERR_VALUE(ret))
		return ret;

	ret = regmap_write(rtc_dd->regmap, REG_RTCAE1, val);
	if (IS_ERR_VALUE(ret))
		return ret;

	return ret;
}

static int max77525_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct max77525_rtc *rtc_dd = dev_get_drvdata(dev);
	u8 data[RTC_NR_TIME];
	unsigned int val;
	int ret = 0;

	regmap_read(rtc_dd->regmap, REG_RTCSECA1,  &data[RTC_SEC]);
	regmap_read(rtc_dd->regmap, REG_RTCMINA1,  &data[RTC_MIN]);
	regmap_read(rtc_dd->regmap, REG_RTCHOURA1, &data[RTC_HOUR]);
	regmap_read(rtc_dd->regmap, REG_RTCDOWA1,  &data[RTC_DOW]);
	regmap_read(rtc_dd->regmap, REG_RTCMONTHA1,&data[RTC_MONTH]);
	regmap_read(rtc_dd->regmap, REG_RTCYEARA1, &data[RTC_YEAR]);
	regmap_read(rtc_dd->regmap, REG_RTCDOMA1,  &data[RTC_DOM]);

	max77525_rtc_data_to_tm(data, &alarm->time);
	ret = rtc_valid_tm(&alarm->time);
	if (ret) {
		dev_err(dev, "Invalid time read from RTC\n");
		return ret;
	}

	dev_dbg(dev, "Alarm set for - h:r:s=%d:%d:%d, d/m/y=%d/%d/%d\n",
		alarm->time.tm_hour, alarm->time.tm_min,
				alarm->time.tm_sec, alarm->time.tm_mday,
				alarm->time.tm_mon, alarm->time.tm_year);

	ret = regmap_read(rtc_dd->regmap, REG_RTCAE1, &val);
	if (IS_ERR_VALUE(ret))
		return ret;

	alarm->enabled = (val == 0) ? 0 : 1;
	return ret;
}

static int max77525_rtc_alarm_irq_enable(struct device *dev,
		unsigned int enabled)
{
	struct max77525_rtc *rtc_dd = dev_get_drvdata(dev);
	unsigned int val;
	unsigned long irq_flags;
	int ret;

	if (enabled)
		val = BIT_AESEC | BIT_AEMIN |
			BIT_AEHOUR | BIT_AEMONTH |
			BIT_AEYEAR | BIT_AEDOM;
	else
		val = 0;

	spin_lock_irqsave(&rtc_dd->alarm_ctrl_lock, irq_flags);

	ret = regmap_write(rtc_dd->regmap, REG_RTCAE1, val);

	spin_unlock_irqrestore(&rtc_dd->alarm_ctrl_lock, irq_flags);

	return ret;
}

static struct rtc_class_ops max77525_rtc_ops = {
	.read_time = max77525_rtc_read_time,
	.set_alarm = max77525_rtc_set_alarm,
	.read_alarm = max77525_rtc_read_alarm,
	.alarm_irq_enable = max77525_rtc_alarm_irq_enable,
};

static irqreturn_t max77525_rtc_alarm_irq(int irq, void *dev_id)
{
	struct max77525_rtc *rtc_dd = dev_id;
	int ret;
	unsigned int val;

	ret = regmap_read(rtc_dd->regmap, REG_RTCINT, &val);
	if (IS_ERR_VALUE(ret))
		return IRQ_NONE;

	if (val & BIT_RTCA1)
		rtc_update_irq(rtc_dd->rtc_dev, 1, RTC_IRQF | RTC_AF);
	else
		return IRQ_NONE;

	return IRQ_HANDLED;
}

static int max77525_rtc_hw_init(struct max77525_rtc *rtc_dd,
									struct max77525_rtc_platform_data *pdata)
{
	unsigned int val;
	int ret = 0;

	ret = regmap_read(rtc_dd->regmap, REG_RTCCNTL, &val);
	if (IS_ERR_VALUE(ret))
		return ret;

	if (unlikely((val != (HRMODE_24 | BINARY_MODE))))
	{
		val = BIT_BCD | BIT_HRMODE;
		ret = regmap_write(rtc_dd->regmap, REG_RTCCNTLM, val);
		if (IS_ERR_VALUE(ret))
			return ret;

		val = HRMODE_24 | BINARY_MODE;
		ret = regmap_write(rtc_dd->regmap, REG_RTCCNTL, val);
		if (IS_ERR_VALUE(ret))
			return ret;

		val = 0;
		ret = regmap_write(rtc_dd->regmap, REG_RTCCNTLM, val);
		if (IS_ERR_VALUE(ret))
			return ret;
	}

	val = BIT_RTC1S | BIT_SMPL | BIT_RTCA2 | BIT_RTC60S;
	ret = regmap_write(rtc_dd->regmap, REG_RTCINTM, val);
	if (IS_ERR_VALUE(ret))
		return ret;

	val = 0;
	if (pdata->smpl_en == true)
		val = SMPL_EN_MASK | pdata->smpl_timer;

	ret = regmap_write(rtc_dd->regmap, REG_RTCSMPL, val);
	if (IS_ERR_VALUE(ret))
		return ret;

	return ret;
}

static int max77525_rtc_probe(struct platform_device *pdev)
{
	int rc;
	struct max77525 *max77525 = dev_get_drvdata(pdev->dev.parent);
	struct device_node *pmic_node = pdev->dev.parent->of_node;
	struct device_node *node;
	struct max77525_rtc_platform_data *rtc_pdata;
	struct max77525_rtc *rtc_dd;

	rtc_dd = devm_kzalloc(&pdev->dev, sizeof(*rtc_dd), GFP_KERNEL);
	if (rtc_dd == NULL) {
		dev_err(&pdev->dev, "Unable to allocate memory!\n");
		return -ENOMEM;
	}

	rtc_pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct max77525_rtc_platform_data), GFP_KERNEL);
	if (rtc_pdata == NULL) {
		dev_err(&pdev->dev, "Unable to allocate memory!\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, rtc_dd);

	/* rtc alarm interrupt wakeup */
	device_init_wakeup(&pdev->dev, 1);

	rtc_dd->cache_update = BIT_RTCWAKE | BIT_RSVD;
	rtc_dd->regmap = max77525->regmap;

	node = of_find_node_by_name(pmic_node, "rtc");
	/* Get the rtc write property */
	rtc_pdata->rtc_write_enable = of_property_read_bool(node,
									"maxim,max77525-rtc-write");
	if (rtc_pdata->rtc_write_enable == true)
		max77525_rtc_ops.set_time = max77525_rtc_set_time;
	rtc_pdata->rtc_alarm_powerup = of_property_read_bool(node,
									"maxim,max77525-rtc-alarm-pwrup");
	rtc_dd->rtc_alarm_powerup = rtc_pdata->rtc_alarm_powerup;

	rtc_pdata->smpl_en = of_property_read_bool(node,
									"maxim,max77525-rtc-enable-smpl");
	rc = of_property_read_u32(node,
						"maxim,max77525-rtc-smpl-timer",
						&rtc_pdata->smpl_timer);
	if (rc && rc != -EINVAL) {
		dev_err(&pdev->dev,
			"Error reading smpl timer property %d\n", rc);
		return rc;
	}

	rc = max77525_rtc_hw_init(rtc_dd, rtc_pdata);
	if (IS_ERR_VALUE(rc))
		return rc;

	/* Initialise spinlock to protect RTC control register */
	spin_lock_init(&rtc_dd->alarm_ctrl_lock);

	/* Register the RTC device */
	rtc_dd->rtc_dev = rtc_device_register("max77525-rtc", &pdev->dev,
			&max77525_rtc_ops, THIS_MODULE);
	if (unlikely(IS_ERR(rtc_dd->rtc_dev)))
		return PTR_ERR(rtc_dd->rtc_dev);

	if (rtc_pdata->rtc_alarm_powerup == true) {
		rtc_dd->rtc_alarm_irq = regmap_irq_get_virq(max77525->irq_data,
				MAX77525_IRQ_RTCINT);
		if (rtc_dd->rtc_alarm_irq < 0) {
			rc = rtc_dd->rtc_alarm_irq;
			goto err_unregister;
		}

		rc = devm_request_threaded_irq(&pdev->dev, rtc_dd->rtc_alarm_irq,
				NULL, max77525_rtc_alarm_irq, IRQF_TRIGGER_LOW | IRQF_SHARED,
				"rtc-alarm0", rtc_dd);
		if (IS_ERR_VALUE(rc))
			goto err_unregister;
	}
return 0;

err_unregister:
	rtc_device_unregister(rtc_dd->rtc_dev);
	return rc;
}

static int max77525_rtc_remove(struct platform_device *pdev)
{
	struct max77525_rtc *rtc_dd = platform_get_drvdata(pdev);

	device_init_wakeup(&pdev->dev, 0);
	free_irq(rtc_dd->rtc_alarm_irq, rtc_dd);
	rtc_device_unregister(rtc_dd->rtc_dev);
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static void max77525_rtc_shutdown(struct platform_device *pdev)
{
	u8 val;
	int rc;
	unsigned long irq_flags;
	struct max77525_rtc *rtc_dd = platform_get_drvdata(pdev);
	bool rtc_alarm_powerup = rtc_dd->rtc_alarm_powerup;

	if (!rtc_alarm_powerup) {
		spin_lock_irqsave(&rtc_dd->alarm_ctrl_lock, irq_flags);
		dev_dbg(&pdev->dev, "Disabling alarm interrupts\n");

		/* Disable alarms wake up */
		val = BIT_RSVD;
		rc = regmap_write(rtc_dd->regmap,REG_RTCUPDATE, val);
		if (rc) {
			dev_err(&pdev->dev, "I2C write failed\n");
			goto fail_alarm_disable;
		}

		/* Disable SMPL */
		val = 0;
		rc = regmap_write(rtc_dd->regmap,REG_RTCSMPL, val);
		if (rc)
			dev_err(&pdev->dev, "I2C write failed\n");

fail_alarm_disable:
		spin_unlock_irqrestore(&rtc_dd->alarm_ctrl_lock, irq_flags);
	}
}

static struct of_device_id max77525_rtc_match_table[] = {
	{
		.compatible = "maxim,max77525-rtc",
	},
	{}
};

const static struct platform_device_id rtc_id[] =
{
	{ "max77525-rtc", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, rtc_id);

static struct platform_driver max77525_rtc_driver =
{
	.driver		=
	{
		.name	= "max77525-rtc",
		.owner	= THIS_MODULE,
		.of_match_table = max77525_rtc_match_table,
	},
	.probe		= max77525_rtc_probe,
	.remove		= max77525_rtc_remove,
	.shutdown	= max77525_rtc_shutdown,
	.id_table	= rtc_id,
};

static int __init max77525_rtc_init(void)
{
	return platform_driver_register(&max77525_rtc_driver);
}
module_init(max77525_rtc_init);

static void __exit max77525_rtc_exit(void)
{
	platform_driver_unregister(&max77525_rtc_driver);
}
module_exit(max77525_rtc_exit);

MODULE_DESCRIPTION("Maxim MAX77525 RTC driver");
MODULE_AUTHOR("Clark Kim <clark.kim@maximintegrated.com>");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:max77525-rtc");
