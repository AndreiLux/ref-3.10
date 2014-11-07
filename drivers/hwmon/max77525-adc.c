/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/hwmon.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/hwmon-sysfs.h>
#include <linux/mfd/max77525/max77525.h>
#include <linux/mfd/max77525/max77525-adc.h>
#include <linux/platform_device.h>

/* MAX77525 ADC register definition */
#define REG_ADCINT		0x2E
#define REG_ADCINTM		0x2F
#define DTRINT_MASK		BIT(1)
#define DTFINT_MASK		BIT(2)
#define ADCCONVINT_MASK	BIT(3)
#define ADCCONTINT_MASK	BIT(4)
#define ADCTRIGINT_MASK	BIT(5)

#define REG_ADCCNTL		0x30
#define ADCEN_MASK		BIT(0)
#define ADCEN_SHIFT		0
#define ADCREFEN_MASK	BIT(1)
#define ADCREFEN_SHIFT	1
#define ADCAVG_MASK		0xC0
#define ADCAVG_SHIFT	2
#define ADCCONV_MASK	BIT(4)
#define ADCCONV_SHIFT	4
#define ADCCONT_MASK	BIT(5)
#define ADCCONT_SHIFT	5

#define REG_ADCDLY		0x31
#define ADCDLY_MASK		0x1F

#define REG_ADCSEL0		0x32
#define CH1SEL_MASK		BIT(1)
#define CH2SEL_MASK		BIT(2)
#define CH3SEL_MASK		BIT(3)
#define CH6SEL_MASK		BIT(6)
#define CH7SEL_MASK		BIT(7)

#define REG_ADCSEL1		0x33
#define CH8SEL_MASK		BIT(0)
#define CH9SEL_MASK		BIT(1)
#define CH10SEL_MASK	BIT(2)
#define CH11SEL_MASK	BIT(3)
#define CH12SEL_MASK	BIT(4)
#define CH13SEL_MASK	BIT(5)

#define REG_ADCCHSEL	0x34
#define ADCCH_MASK		0x1F

#define REG_ADCDATAL	0x35
#define REG_ADCDATAH	0x36

#define REG_ADCICNFG	0x37
#define IADC_MASK		0x03
#define IADCMUX_MASK	0x0C

#define REG_DTRL		0x38
#define REG_DTRH		0x39
#define REG_DTFL		0x3A
#define REG_DTFH		0x3B
#define REG_DTSHDNL		0x3C
#define REG_DTSHDNH		0x3D

#define ADC_COMPLETION_TIMEOUT				HZ
#define ADC_HWMON_NAME_LENGTH				64


struct max77525_adc_scale_info {
	int multiplier;
	int divider;
	int offset;
};

static struct max77525_adc_scale_info adc_scale_infos[] = {
	/* V = 2.5V * CODE / 4096 * 1000 * 1000 (uV) */
	[SCALE_DEFAULT] = { (2.5*1000*1000), 4096, 0 },
	/* T = (CODE * 2.5 /4096 - 1.4914) / 0.005619 (1 degree) */
	[SCALE_TDIE] = { (2.5*1000*1000), (4096*5619), -(1.4914*1000*1000/5619)},
	/* V = 4.375V * CODE / 4096 * 1000 * 1000 (uV) */
	[SCALE_VBBATT] = { (4.375*1000*1000), 4096, 0 },
	/* V = 5.12V * CODE / 4096 * 1000 * 1000 (uV) */
	[SCALE_VSYS] = { (5.12*1000*1000), 4096, 0 },
	/* V = 1.8V * CODE / 4096 * 1000 * 1000 (uV) */
	[SCALE_VMBATDETB] = { (1.8*1000*1000), 4096, 0 },
};

int32_t max77525_adc_scale_default(int32_t adc_code, int32_t scale_type,
		struct max77525_adc_result *adc_chan_result)
{
	int32_t scale_voltage = 0;

	if (!adc_chan_result)
		return -EINVAL;

	scale_voltage = adc_code * adc_scale_infos[scale_type].multiplier
					/ adc_scale_infos[scale_type].divider
					+ adc_scale_infos[scale_type].offset;

	adc_chan_result->physical = scale_voltage;
	return 0;
}

static struct max77525_adc_scale_fn adc_scale_fn[] = {
	[SCALE_DEFAULT] = {max77525_adc_scale_default},
	[SCALE_TDIE] = {max77525_adc_scale_default},
	[SCALE_VBBATT] = {max77525_adc_scale_default},
	[SCALE_VSYS] = {max77525_adc_scale_default},
	[SCALE_VMBATDETB] = {max77525_adc_scale_default},
};

static int32_t max77525_adc_enable(struct max77525_adc *adc_dev, bool state)
{
	int rc = 0;
	unsigned int data = 0;

	rc = regmap_read(adc_dev->regmap, REG_ADCCNTL, &data);
	if (rc < 0) {
		return rc;
	}

	if (state) {
		data = data | ADCEN_MASK;
		rc = regmap_write(adc_dev->regmap, REG_ADCCNTL,	data);
		if (rc < 0) {
			pr_err("ADC enable setting failed\n");
			return rc;
		}

		/* Start conversion */
		data = data | ADCCONV_MASK;
		rc = regmap_write(adc_dev->regmap, REG_ADCCNTL,	data);
		if (rc < 0) {
			pr_err("ADC start conversion failed\n");
			return rc;
		}
	} else {
		data = (data & ~(ADCEN_MASK | ADCREFEN_MASK));
		rc = regmap_write(adc_dev->regmap, REG_ADCCNTL,	data);
		if (rc < 0) {
			pr_err("ADC disable setting failed\n");
			return rc;
		}
	}

	return 0;
}

static int32_t max77525_adc_configure(struct max77525_adc *adc,
			enum max77525_adc_channels channel)
{
	unsigned int chan_reg, chan_mask;
	int rc = 0;

	/* Channel selection */
	chan_reg = (channel < 8) ? REG_ADCSEL0 : REG_ADCSEL1;
	chan_mask = channel % 8;
	rc = regmap_write(adc->regmap, chan_reg, chan_mask);
	if (rc < 0) {
		pr_err("Channel configure error\n");
		return rc;
	}

	INIT_COMPLETION(adc->adc_rslt_completion);

	/* ADC enable and start conversion */
	rc = max77525_adc_enable(adc, true);

	return rc;
}

static int32_t max77525_adc_read_conversion_result(
		struct max77525_adc *adc_dev,
		enum max77525_adc_channels channel, int16_t *data)
{
	uint8_t rslt_lsb, rslt_msb;
	int rc = 0;

	/* Select channel */
	rc = regmap_write(adc_dev->regmap, REG_ADCCHSEL, (unsigned int) channel);
	if (rc < 0) {
		pr_err("max77525 adc channel select failed\n");
		return rc;
	}

	/* Read ADC value */
	rc = regmap_read(adc_dev->regmap, REG_ADCDATAL, (unsigned int *)&rslt_lsb);
	if (rc < 0) {
		pr_err("max77525 adc result read failed for data0\n");
		return rc;
	}

	rc = regmap_read(adc_dev->regmap, REG_ADCDATAH, (unsigned int *)&rslt_msb);
	if (rc < 0) {
		pr_err("max77525 adc result read failed for data1\n");
		return rc;
	}

	*data = (rslt_msb << 8) | rslt_lsb;

	rc = max77525_adc_enable(adc_dev, false);
	if (rc)
		return rc;

	return 0;
}

static irqreturn_t max77525_adc_isr(int irq, void *dev_id)
{
	struct max77525_adc *adc = dev_id;

	if (!adc->adc_initialized)
		return IRQ_HANDLED;

	printk("[%s] \n", __func__);
	complete(&adc->adc_rslt_completion);

	return IRQ_HANDLED;
}

int max77525_adc_conv_seq_request(struct max77525_adc *adc,
					enum max77525_adc_channels channel,
					struct max77525_adc_result *result)
{
	int rc = 0;

	if (!adc || !adc->adc_initialized)
		return -EPROBE_DEFER;

	mutex_lock(&adc->adc_lock);

	rc = max77525_adc_configure(adc, channel);
	if (rc) {
		pr_err("max77525 adc configure failed with %d\n", rc);
		goto fail_unlock;
	}

	rc = wait_for_completion_timeout(&adc->adc_rslt_completion,
					ADC_COMPLETION_TIMEOUT);
	if (!rc) {
		unsigned int int_reg = 0;
		rc = regmap_read(adc->regmap, REG_ADCINT, &int_reg);
		if (rc < 0)
			goto fail_unlock;

		int_reg &= ADCCONVINT_MASK;
		if (int_reg == ADCCONVINT_MASK)
			pr_debug("End of ADC conversion\n");
		else {
			pr_err("ADC disable failed\n");
			rc = -EINVAL;
			goto fail_unlock;
		}
	}

	rc = max77525_adc_read_conversion_result(adc, channel, &result->adc_code);
	if (rc) {
		pr_err("max77525 adc read adc code failed with %d\n", rc);
		goto fail_unlock;
	}

fail_unlock:
	mutex_unlock(&adc->adc_lock);

	return rc;
}
EXPORT_SYMBOL(max77525_adc_conv_seq_request);

int max77525_adc_read(struct max77525_adc *adc,
		enum max77525_adc_channels channel,
		struct max77525_adc_result *result)
{
	int rc = 0;
	int scale_type = 0;

	result->chan = channel;
	rc = max77525_adc_conv_seq_request(adc, channel, result);
	if (rc < 0) {
		pr_err("Error reading adc\n");
		return rc;
	}

	scale_type = adc->adc_channels[channel].adc_scale_fn;
	if (scale_type >= SCALE_NONE) {
		rc = -EBADF;
	}

	adc_scale_fn[scale_type].chan(result->adc_code, scale_type, result);


	return 0;
}
EXPORT_SYMBOL(max77525_adc_read);

static ssize_t max77525_adc_show(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct max77525_adc *adc_dev = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct max77525_adc_result result;
	int rc = -1;

	rc = max77525_adc_read(adc_dev, attr->index, &result);

	if (rc) {
		pr_err("ADC read error with %d\n", rc);
		return 0;
	}

	return snprintf(buf, ADC_HWMON_NAME_LENGTH,
		"Result:%d Raw:0x%x\n", result.physical, result.adc_code);
}

static struct sensor_device_attribute max77525_adc_attr =
	SENSOR_ATTR(NULL, S_IRUGO, max77525_adc_show, NULL, 0);

#ifdef CONFIG_USB_ODIN_DRD
struct max77525_adc *g_max77525_adc;
int odin_usb_get_adc_77525(void)
{
	struct max77525_adc *adc = g_max77525_adc;
	struct max77525_adc_result result;
	int rc = -1;

	rc = max77525_adc_read(adc, CH7_ADC1, &result);
	if (rc) {
		pr_err("ADC read error with %d\n", rc);
		return -1;
	}
	printk("[%s]CH7_ADC1 - Result:%d Raw:0x%x\n",
		result.physical, result.adc_code);

	rc = max77525_adc_read(adc, CH9_ADC2, &result);
	if (rc) {
		pr_err("ADC read error with %d\n", rc);
		return -1;
	}
	printk("[%s]CH9_ADC2 - Result:%d Raw:0x%x\n",
		result.physical, result.adc_code);

	return result.physical;
}

#endif /* ONFIG_USB_ODIN_DRD */

static int32_t max77525_adc_init_hwmon(struct platform_device *pdev)
{
	struct max77525_adc *adc = dev_get_drvdata(&pdev->dev);
	int rc = 0, i = 0, channel;

	if (!adc) {
		dev_err(&pdev->dev, "could not find adc device\n");
		return -ENODEV;
	}

	for (i=0; i < adc->num_channels; i++) {
		channel = adc->adc_channels[i].channel_num;
		max77525_adc_attr.index = adc->adc_channels[i].channel_num;
		max77525_adc_attr.dev_attr.attr.name =
						adc->adc_channels[i].name;
		memcpy(&adc->sens_attr[i], &max77525_adc_attr,
						sizeof(max77525_adc_attr));
		sysfs_attr_init(&adc->sens_attr[i].dev_attr.attr);
		rc = device_create_file(&pdev->dev,
				&adc->sens_attr[i].dev_attr);
		if (rc) {
			dev_err(&pdev->dev,
				"device_create_file failed for dev %s\n",
				adc->adc_channels[i].name);
			goto hwmon_err_sens;
		}
		i++;
	}

	return 0;
hwmon_err_sens:
	pr_err("Init HWMON failed for max77525_adc with %d\n", rc);
	return rc;
}

static int32_t max77525_adc_init_configure(struct max77525_adc *adc_dev)
{
	int rc = 0;
	unsigned int val;

	/* set average sample */
	val = adc_dev->adc_avg << ADCAVG_SHIFT;
	rc = regmap_write(adc_dev->regmap, REG_ADCCNTL, val);
	if (rc) {
		pr_err("ADC AVG error with %d\n", rc);
		return rc;
	}

	/* set adc delay time */
	val = adc_dev->adc_delay;
	rc = regmap_write(adc_dev->regmap, REG_ADCDLY, val);
	if (rc) {
		pr_err("ADC Delay error with %d\n", rc);
		return rc;
	}

	/* set interrupt mask */
	/* just enable ADCCONVINT_MASK */
	val = DTRINT_MASK | DTFINT_MASK | ADCCONTINT_MASK | ADCTRIGINT_MASK;
	rc = regmap_write(adc_dev->regmap, REG_ADCINTM, val);
	if (rc) {
		pr_err("ADC INTM error with %d\n", rc);
		return rc;
	}

	return 0;
}

int32_t max77525_adc_get_devicetree_data(struct platform_device *pdev,
			struct max77525_adc *adc_dev)
{
	struct device_node *pmic_node = pdev->dev.of_node;
	struct device_node *node, *child;
	struct max77525_adc_amux *adc_channel_list;
	int count_adc_channel_list = 0, rc = 0, i = 0;

	node = of_find_node_by_name(pmic_node, "adc");
	if (!node) {
		dev_err(&pdev->dev, "could not find adc sub-node\n");
		return -ENODEV;
	}

	for_each_child_of_node(node, child)
		count_adc_channel_list++;
	if (!count_adc_channel_list) {
		pr_err("No channel listing\n");
		return -EINVAL;
	}
	adc_channel_list = devm_kzalloc(&pdev->dev,
		((sizeof(struct max77525_adc_amux)) * count_adc_channel_list),
				GFP_KERNEL);
	if (!adc_channel_list) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	adc_dev->adc_channels = adc_channel_list;

	for_each_child_of_node(node, child) {
		int channel_num, post_scaling;
		int rc;
		const char *channel_name;

		channel_name = of_get_property(child,
				"label", NULL) ? : child->name;
		if (!channel_name) {
			pr_err("Invalid channel name\n");
			return -EINVAL;
		}

		rc = of_property_read_u32(child, "channel", &channel_num);
		if (rc) {
			pr_err("Invalid channel num\n");
			return -EINVAL;
		}
		rc = of_property_read_u32(child,
				"scale-function", &post_scaling);
		if (rc) {
			pr_err("Invalid channel post scaling property\n");
			return -EINVAL;
		}
		/* Individual channel properties */
		adc_channel_list[i].name = (char *)channel_name;
		adc_channel_list[i].channel_num = channel_num;
		adc_channel_list[i].adc_scale_fn = post_scaling;
		i++;
	}

	/* Get the ADC VDD reference voltage and ADC bit resolution */
	rc = of_property_read_u32(node, "maxim,adc-avg-sample",
			&adc_dev->adc_avg);
	if (rc) {
		pr_err("Invalid adc average sample property\n");
		return -EINVAL;
	}
	rc = of_property_read_u32(node, "maxim,adc-delay",
			&adc_dev->adc_delay);
	if (rc) {
		pr_err("Invalid adc delay property\n");
		return -EINVAL;
	}

	init_completion(&adc_dev->adc_rslt_completion);

	return 0;
}

static int max77525_adc_probe(struct platform_device *pdev)
{
	struct max77525_adc *adc;
	struct max77525 *max77525 = dev_get_drvdata(pdev->dev.parent);
	struct device_node *pmic_node = pdev->dev.parent->of_node;
	struct device_node *node, *child;
	int rc, count_adc_channel_list = 0;

	node = of_find_node_by_name(pmic_node, "adc");
	if (!node) {
		dev_err(&pdev->dev, "could not find adc sub-node\n");
		return -ENODEV;
	}

	for_each_child_of_node(node, child)
		count_adc_channel_list++;

	if (!count_adc_channel_list) {
		pr_err("No channel listing\n");
		return -EINVAL;
	}

	adc = devm_kzalloc(&pdev->dev, sizeof(struct max77525_adc) +
		(sizeof(struct sensor_device_attribute) *
				count_adc_channel_list), GFP_KERNEL);
	if (!adc) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	adc->num_channels = count_adc_channel_list;
	rc = max77525_adc_get_devicetree_data(pdev, adc);
	if (rc) {
		dev_err(&pdev->dev, "failed to read device tree\n");
		goto fail;
	}
	mutex_init(&adc->adc_lock);
	adc->regmap = max77525->regmap;

	dev_set_drvdata(&pdev->dev, adc);
	rc = max77525_adc_init_hwmon(pdev);
	if (rc) {
		dev_err(&pdev->dev, "failed to initialize max77525 hwmon adc\n");
		goto fail;
	}

	adc->adc_hwmon = hwmon_device_register(&pdev->dev);
	if (IS_ERR(adc->adc_hwmon)) {
		rc = PTR_ERR(adc->adc_hwmon);
		dev_err(&pdev->dev,
				"hwmon_device_register failed with %d.\n", rc);
		goto fail;
	}

	rc = max77525_adc_init_configure(adc);
	if (rc < 0) {
		pr_err("Setting init configuration failed %d\n", rc);
		return rc;
	}

	adc->adc_irq = regmap_irq_get_virq(max77525->irq_data, MAX77525_IRQ_ADCINT);
	if (adc->adc_irq < 0) {
		rc = adc->adc_irq;
		goto fail;
	}
#if 0 /* Disable irq */
	rc = devm_request_threaded_irq(&pdev->dev, adc->adc_irq, NULL,
			max77525_adc_isr, IRQF_TRIGGER_LOW | IRQF_SHARED,
			"max77525_adc_interrupt", adc);
	if (rc) {
		dev_err(&pdev->dev,
			"failed to request adc irq with error %d\n", rc);
		goto fail;
	} else {
		enable_irq_wake(adc->adc_irq);
	}
#endif
	adc->adc_initialized = true;
#ifdef CONFIG_USB_ODIN_DRD
	g_max77525_adc = adc;
#endif
	return 0;
fail:
	return rc;
}

static int max77525_adc_remove(struct platform_device *pdev)
{
	struct max77525_adc *adc = dev_get_drvdata(&pdev->dev);
	struct device_node *pmic_node = pdev->dev.parent->of_node;
	struct device_node *node, *child;
	int i = 0;

	node = of_find_node_by_name(pmic_node, "adc");

	for_each_child_of_node(node, child) {
		device_remove_file(&pdev->dev,
			&adc->sens_attr[i].dev_attr);
		i++;
	}
	adc->adc_initialized = false;
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static const struct of_device_id max77525_adc_match_table[] = {
	{.compatible = "maxim,max77525-adc", },
	{}
};

static struct platform_driver max77525_adc_driver = {
	.driver		= {
		.name	= "max77525-adc",
		.of_match_table = max77525_adc_match_table,
	},
	.probe		= max77525_adc_probe,
	.remove		= max77525_adc_remove,
};

static int __init max77525_adc_init(void)
{
	return platform_driver_register(&max77525_adc_driver);
}
module_init(max77525_adc_init);

static void __exit max77525_adc_exit(void)
{
	platform_driver_unregister(&max77525_adc_driver);
}
module_exit(max77525_adc_exit);

MODULE_DESCRIPTION("MAX77525 PMIC ADC driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Clark Kim <clark.kim@maximintegrated.com>");
MODULE_VERSION("1.0");
