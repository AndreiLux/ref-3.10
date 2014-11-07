/*
 * HW Monitor support for Dialog DA906X PMIC series
 *
 * Copyright 2012 Dialog Semiconductor Ltd.
 *
 * Author: Krystian Garbaciak <krystian.garbaciak@diasemi.com>,
 *         Michal Hajduk <michal.hajduk@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/mfd/da906x/core.h>
#include <linux/odin_mailbox.h>
#include <linux/odin_pmic.h>

/* Manual and auto resolution defines */
#define DA906X_ADC_RES		\
		(1 << (DA906X_ADC_RES_L_BITS + DA906X_ADC_RES_M_BITS))
#define DA906X_ADC_MAX		(DA906X_ADC_RES - 1)
#define	DA906X_ADC_AUTO_RES	(1 << DA906X_ADC_RES_M_BITS)
#define	DA906X_ADC_AUTO_MAX	(DA906X_ADC_AUTO_RES - 1)

/* Define interpolation table to calculate ADC values  */
struct i_table {
	int x0;
	int a;
	int b;
};
#define ILINE(x1, x2, y1, y2)	{ \
		.x0 = (x1), \
		.a = ((y2) - (y1)) * DA906X_ADC_RES / ((x2) - (x1)), \
		.b = (y1) - ((y2) - (y1)) * (x1) / ((x2) - (x1)), \
	}

struct channel_info {
	const char *name;
	const struct i_table *tbl;
	int tbl_max;
	u16 auto_reg;
};

enum da906x_adc {
	DA906X_VSYS,
	DA906X_ADCIN1,
	DA906X_ADCIN2,
	DA906X_ADCIN3,
	DA906X_TJUNC,
	DA906X_VBBAT,
};

static const struct i_table vsys_tbl[] = {
	ILINE(0, DA906X_ADC_MAX, 2500, 5500)
};

static const struct i_table adcin_tbl[] = {
	ILINE(0, DA906X_ADC_MAX, 0, 2500)
};

static const struct i_table tjunc_tbl[] = {
	ILINE(0, DA906X_ADC_MAX, 287, -132)
};

static const struct i_table vbbat_tbl[] = {
	ILINE(0, DA906X_ADC_MAX, 0, 5000)
};

static const struct channel_info da906x_channels[] = {
	[DA906X_VSYS]	= { "VSYS",
			    vsys_tbl, ARRAY_SIZE(vsys_tbl) - 1,
			    DA906X_REG_VSYS_RES },
	[DA906X_ADCIN1]	= { "ADCIN1",
			    adcin_tbl,	ARRAY_SIZE(adcin_tbl) - 1,
			    DA906X_REG_ADCIN1_RES },
	[DA906X_ADCIN2]	= { "ADCIN2",
			    adcin_tbl,	ARRAY_SIZE(adcin_tbl) - 1,
			    DA906X_REG_ADCIN2_RES },
	[DA906X_ADCIN3]	= { "ADCIN3",
			    adcin_tbl,	ARRAY_SIZE(adcin_tbl) - 1,
			    DA906X_REG_ADCIN3_RES },
	[DA906X_TJUNC]	= { "TJUNC",
			    tjunc_tbl,	ARRAY_SIZE(tjunc_tbl) - 1 },
	[DA906X_VBBAT]	= { "VBBAT",
			    vbbat_tbl,	ARRAY_SIZE(vbbat_tbl) - 1}
};

struct da906x_hwmon {
	struct da906x *da906x;
	struct device *class_dev;
	struct mutex hwmon_mutex;
	struct completion man_adc_rdy;
	int irq;
	u8 adc_auto_en;
	s8 tjunc_offset;
};

static DECLARE_COMPLETION(adc_received);

int da906x_adc_convert(int channel, int x)
{
	const struct channel_info *info = &da906x_channels[channel];
	int i, ret;

	for (i = info->tbl_max; i > 0; i--)
		if (info->tbl[i].x0 <= x)
			break;

	ret = info->tbl[i].a * x;
	if (ret >= 0)
		ret += DA906X_ADC_RES / 2;
	else
		ret -= DA906X_ADC_RES / 2;
	ret = ret / DA906X_ADC_RES + info->tbl[i].b;
	return ret;
}

static int da906x_adc_manual_read(struct da906x_hwmon *hwmon, int channel)
{
	int ret;
	u8 data[2];

	mutex_lock(&hwmon->hwmon_mutex);

	init_completion(&hwmon->man_adc_rdy);

	/* Start measurment on selected channel */
	data[0] = (channel << DA906X_ADC_MUX_SHIFT) & DA906X_ADC_MUX_MASK;
	data[0] |= DA906X_ADC_MAN;
	ret = da906x_reg_update(hwmon->da906x, DA906X_REG_ADC_MAN,
				DA906X_ADC_MUX_MASK | DA906X_ADC_MAN, data[0]);
	if (ret < 0)
		goto out;

	/* Wait for interrupt from ADC */
	ret = wait_for_completion_timeout(&hwmon->man_adc_rdy,
					  msecs_to_jiffies(1000));
	if (ret == 0) {
		ret = -EBUSY;
		goto out;
	}

	/* Get results */
	ret = da906x_block_read(hwmon->da906x, DA906X_REG_ADC_RES_L, 2, data);
	if (ret < 0)
		goto out;
	ret = (data[0] & DA906X_ADC_RES_L_MASK) >> DA906X_ADC_RES_L_SHIFT;
	ret |= data[1] << DA906X_ADC_RES_L_BITS;
out:
	mutex_unlock(&hwmon->hwmon_mutex);
	return ret;
}

static int da906x_adc_auto_read(struct da906x *da906x, int channel)
{
	const struct channel_info *info = &da906x_channels[channel];
	int ret;

	ret = da906x_reg_read(da906x, info->auto_reg);
	if (ret < 0)
		return ret;

	return ret;
}

static irqreturn_t da906x_hwmon_irq_handler(int irq, void *irq_data)
{
	struct da906x_hwmon *hwmon = irq_data;

	complete(&hwmon->man_adc_rdy);

	return IRQ_HANDLED;
}

static irqreturn_t da906x_hwmon_mbirq_handler(int irq, void *data)
{
	unsigned int mbox_adc_data[7];
	unsigned int adc_value;
	int i;

	mbox_adc_data[2] = (*((unsigned int *)data + 2));

	/* This function will be replaced later */
	adc_value = mbox_adc_data[2];
	complete(&adc_received);

	return IRQ_HANDLED;
}

static ssize_t da906x_adc_get_en_state(struct device *dev,
				struct device_attribute *devattr, char *buf)
{
	struct da906x_hwmon *hwmon = dev_get_drvdata(dev);
	int chan = to_sensor_dev_attr(devattr)->index;

	return sprintf(buf, "%c\n",
		       (hwmon->adc_auto_en & (1 << chan)) ? '1' : '0');
}

/* Small delay is required before automatically read value is valid */
static ssize_t da906x_adc_set_en_state(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct da906x_hwmon *hwmon = dev_get_drvdata(dev);
	int channel = to_sensor_dev_attr(devattr)->index;
	unsigned long en_auto;
	int ret;

	BUG_ON(da906x_channels[channel].auto_reg == 0);

	ret = kstrtoul(buf, 0, &en_auto);
	if (ret < 0)
		return ret;

	if (en_auto) {
		ret = da906x_reg_set_bits(hwmon->da906x, DA906X_REG_ADC_CONT,
					  1 << channel);
		if (ret == 0)
			hwmon->adc_auto_en |= 1 << channel;
	} else {
		ret = da906x_reg_clear_bits(hwmon->da906x, DA906X_REG_ADC_CONT,
					    1 << channel);
		if (ret == 0)
			hwmon->adc_auto_en &= ~(1 << channel);
	}

	return count;
}

static ssize_t da906x_adc_read(struct device *dev,
			       struct device_attribute *devattr, char *buf)
{
	struct da906x_hwmon *hwmon = dev_get_drvdata(dev);
	int channel = to_sensor_dev_attr(devattr)->index;
	int val;

	if (hwmon->adc_auto_en & (1 << channel)) {
		val = da906x_adc_auto_read(hwmon->da906x, channel);
		if (val < 0)
			return val;

		val *= DA906X_ADC_RES / DA906X_ADC_AUTO_RES;
		val = da906x_adc_convert(channel, val);
	} else {
		val = da906x_adc_manual_read(hwmon, channel);
		if (val < 0)
			return val;

		if (channel == DA906X_TJUNC)
			val += hwmon->tjunc_offset;
		val = da906x_adc_convert(channel, val);
	}

	return sprintf(buf, "%d\n", val);
}

static ssize_t da906x_show_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, DA906X_DRVNAME_HWMON "\n");
}

static ssize_t da906x_show_label(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	const struct channel_info *info;

	info = &da906x_channels[to_sensor_dev_attr(devattr)->index];
	return sprintf(buf, "%s\n", info->name);
}

static SENSOR_DEVICE_ATTR(vsys_en_auto, S_IRUGO | S_IWUSR,
			  da906x_adc_get_en_state, da906x_adc_set_en_state,
			  DA906X_VSYS);
static SENSOR_DEVICE_ATTR(vsys_input, S_IRUGO,
			  da906x_adc_read, NULL, DA906X_VSYS);
static SENSOR_DEVICE_ATTR(vsys_label, S_IRUGO,
			  da906x_show_label, NULL, DA906X_VSYS);

static SENSOR_DEVICE_ATTR(in1_en_auto, S_IRUGO | S_IWUSR,
			  da906x_adc_get_en_state, da906x_adc_set_en_state,
			  DA906X_ADCIN1);
static SENSOR_DEVICE_ATTR(in1_input, S_IRUGO,
			  da906x_adc_read, NULL, DA906X_ADCIN1);
static SENSOR_DEVICE_ATTR(in1_label, S_IRUGO,
			  da906x_show_label, NULL, DA906X_ADCIN1);

static SENSOR_DEVICE_ATTR(in2_en_auto, S_IRUGO | S_IWUSR,
			  da906x_adc_get_en_state, da906x_adc_set_en_state,
			  DA906X_ADCIN2);
static SENSOR_DEVICE_ATTR(in2_input, S_IRUGO,
			  da906x_adc_read, NULL,
			  DA906X_ADCIN2);
static SENSOR_DEVICE_ATTR(in2_label, S_IRUGO,
			  da906x_show_label, NULL, DA906X_ADCIN2);

static SENSOR_DEVICE_ATTR(in3_en_auto, S_IRUGO | S_IWUSR,
			  da906x_adc_get_en_state, da906x_adc_set_en_state,
			  DA906X_ADCIN3);
static SENSOR_DEVICE_ATTR(in3_input, S_IRUGO,
			  da906x_adc_read, NULL, DA906X_ADCIN3);
static SENSOR_DEVICE_ATTR(in3_label, S_IRUGO,
			  da906x_show_label, NULL, DA906X_ADCIN3);

static SENSOR_DEVICE_ATTR(vbbat_input, S_IRUGO,
			  da906x_adc_read, NULL, DA906X_VBBAT);
static SENSOR_DEVICE_ATTR(vbbat_label, S_IRUGO,
			  da906x_show_label, NULL, DA906X_VBBAT);

static SENSOR_DEVICE_ATTR(tjunc_input, S_IRUGO,
			  da906x_adc_read, NULL, DA906X_TJUNC);
static SENSOR_DEVICE_ATTR(tjunc_label, S_IRUGO,
			  da906x_show_label, NULL, DA906X_TJUNC);

static DEVICE_ATTR(name, S_IRUGO, da906x_show_name, NULL);

static struct attribute *da906x_attributes[] = {
	&dev_attr_name.attr,
	&sensor_dev_attr_vsys_en_auto.dev_attr.attr,
	&sensor_dev_attr_vsys_input.dev_attr.attr,
	&sensor_dev_attr_vsys_label.dev_attr.attr,
	&sensor_dev_attr_in1_en_auto.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_label.dev_attr.attr,
	&sensor_dev_attr_in2_en_auto.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in2_label.dev_attr.attr,
	&sensor_dev_attr_in3_en_auto.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in3_label.dev_attr.attr,
	&sensor_dev_attr_vbbat_input.dev_attr.attr,
	&sensor_dev_attr_vbbat_label.dev_attr.attr,
	&sensor_dev_attr_tjunc_input.dev_attr.attr,
	&sensor_dev_attr_tjunc_label.dev_attr.attr,
	NULL
};

static const struct attribute_group da906x_attr_group = {
	.attrs = da906x_attributes,
};

static int da906x_hwmon_probe(struct platform_device *pdev)
{
	struct da906x *da906x = dev_get_drvdata(pdev->dev.parent);
	struct da906x_hwmon *hwmon;
	int ret;

	hwmon = devm_kzalloc(&pdev->dev, sizeof(struct da906x_hwmon),
			     GFP_KERNEL);
	if (!hwmon)
		return -ENOMEM;

	mutex_init(&hwmon->hwmon_mutex);
	init_completion(&hwmon->man_adc_rdy);
	hwmon->da906x = da906x;

	ret = sysfs_create_group(&pdev->dev.kobj, &da906x_attr_group);
	if (ret)
		return ret;

	hwmon->class_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(hwmon->class_dev)) {
		ret = PTR_ERR(hwmon->class_dev);
		goto err_sysfs;
	}

#ifndef CONFIG_MFD_DA906X_IRQ_DISABLE
	hwmon->irq = platform_get_irq_byname(pdev, DA906X_DRVNAME_HWMON);
	ret = devm_request_threaded_irq(&pdev->dev, hwmon->irq, NULL,
					da906x_hwmon_irq_handler,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					"HWMON", hwmon);
#else
	hwmon->irq = MB_ADC_HANDLER;
	ret = mailbox_request_irq(MB_ADC_HANDLER, da906x_hwmon_mbirq_handler, "MB_ADC_HANDLER");
#endif

	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ.\n");
		goto err_device;
	}
	platform_set_drvdata(pdev, hwmon);

	ret = da906x_reg_read(hwmon->da906x, DA906X_REG_T_OFFSET);
	if (ret < 0)
		dev_warn(&pdev->dev, "Could not read temperature offset.\n");
	else
		hwmon->tjunc_offset = (s8)ret;
#ifdef CONFIG_USB_ODIN_DRD
	g_hwmon = hwmon;
#endif
	return 0;

err_device:
	hwmon_device_unregister(hwmon->class_dev);
err_sysfs:
	sysfs_remove_group(&pdev->dev.kobj, &da906x_attr_group);
	return ret;
}

static int da906x_hwmon_remove(struct platform_device *pdev)
{
	struct da906x_hwmon *hwmon = platform_get_drvdata(pdev);

	hwmon_device_unregister(hwmon->class_dev);
	sysfs_remove_group(&pdev->dev.kobj, &da906x_attr_group);

	return 0;
}

static struct platform_driver da906x_hwmon_driver = {
	.probe = da906x_hwmon_probe,
	.remove = da906x_hwmon_remove,
	.driver = {
		.name = DA906X_DRVNAME_HWMON,
		.owner = THIS_MODULE,
	},
};

static int __init da906x_hwmon_init(void)
{
	return platform_driver_register(&da906x_hwmon_driver);
}
subsys_initcall(da906x_hwmon_init);

MODULE_DESCRIPTION("DA906X Hardware monitoring");
MODULE_AUTHOR("Krystian Garbaciak <krystian.garbaciak@diasemi.com>, Michal Hajduk <michal.hajduk@diasemi.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("paltform:" DA906X_DRVNAME_HWMON);
