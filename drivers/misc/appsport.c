/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>
#include <linux/switch.h>

struct appsport_platform_data {
		struct switch_dev appsport_sdev;
		int ext_det_gpio;
		int ext_det_irq;
		int appsport_state;
		struct delayed_work	ext_det_work;
};

struct appsport_platform_data *gpdata;

static void appsport_ext_det_work(struct work_struct *w)
{
	struct appsport_platform_data *pdata = container_of(w, struct appsport_platform_data,
						ext_det_work.work);

	if(!gpio_get_value(pdata->ext_det_gpio)) {
		pdata->appsport_state = 1;
	} else {
		pdata->appsport_state = 0;
	}
	switch_set_state(&pdata->appsport_sdev, pdata->appsport_state);

	return;
}

static irqreturn_t appsport_ext_det_irq(int irq, void *data)
{
	struct appsport_platform_data *pdata = data;

	queue_delayed_work(system_nrt_wq, &pdata->ext_det_work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}

static ssize_t
appsport_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;
	len = snprintf(buf, PAGE_SIZE, "%d\n", gpdata->appsport_state);
	return len;
}

static ssize_t
appsport_state_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	sscanf(buf, "%d\n", &gpdata->appsport_state);
	switch_set_state(&gpdata->appsport_sdev, gpdata->appsport_state);
	return count;
}

static DEVICE_ATTR(appsport_state, S_IRUGO | S_IWUSR,
		appsport_state_show, appsport_state_store);

static struct appsport_platform_data *appsport_dt_to_pdata(
	struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct appsport_platform_data *pdata;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "unable to allocate platform data\n");
		return NULL;
	}

	pdata->ext_det_gpio = of_get_named_gpio(node,
					"appsport,ext-det-gpio", 0);

	return pdata;
}

static int appsport_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct appsport_platform_data *pdata;

	pr_info("%s\n", __func__);

	if (pdev->dev.of_node) {
		dev_dbg(&pdev->dev, "device tree enabled\n");
		pdata = appsport_dt_to_pdata(pdev);
		if (!pdata)
			return -EINVAL;
		pdev->dev.platform_data = pdata;
	} else if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "missing platform_data\n");
		return -ENODEV;
	} else {
		pdata = pdev->dev.platform_data;
	}
	gpdata = pdata;

	pdata->appsport_sdev.name = "appsport";
	ret = switch_dev_register(&pdata->appsport_sdev);
	if (ret < 0)
		goto err1;

	INIT_DELAYED_WORK(&pdata->ext_det_work, appsport_ext_det_work);

	pdata->ext_det_irq = platform_get_irq_byname(pdev, "ext_det_irq");
	if (pdata->ext_det_irq  < 0) {
		dev_err(&pdev->dev, "platform_get_irq for ext_det_irq failed\n");
		pdata->ext_det_irq  = 0;
		goto err1;
	} else {
		ret = request_irq(pdata->ext_det_irq, appsport_ext_det_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "appsport_ext_det", pdata);
		if (ret) {
			dev_err(&pdev->dev, "request irq failed (EXT_DET)\n");
			goto err1;
		}
	}

	if (pdata && gpio_is_valid(pdata->ext_det_gpio)) {
		ret = devm_gpio_request(&pdev->dev, pdata->ext_det_gpio,
							"ext_det_gpio");
		if (ret) {
			dev_err(&pdev->dev,
				"ext_det_gpio gpio(%d) request failed:%d\n",
				pdata->ext_det_gpio, ret);
			goto err1;
		} else {
			gpio_direction_input(pdata->ext_det_gpio);
		}
	}

	ret = device_create_file(&pdev->dev, &dev_attr_appsport_state);
	if (ret)
		goto err1;

	queue_delayed_work(system_nrt_wq, &pdata->ext_det_work, msecs_to_jiffies(100));

	return 0;

err1:
	cancel_delayed_work_sync(&pdata->ext_det_work);

	return ret;
}

static int appsport_remove(struct platform_device *pdev)
{
	struct appsport_platform_data *pdata = pdev->dev.platform_data;

	cancel_delayed_work_sync(&pdata->ext_det_work);
	switch_dev_unregister(&pdata->appsport_sdev);
	device_remove_file(&pdev->dev, &dev_attr_appsport_state);

	return 0;
}

static const struct of_device_id appsport_dt_match[] = {
	{ .compatible = "lge,appsport",
	},
	{}
};
MODULE_DEVICE_TABLE(of, appsport_dt_match);

static struct platform_driver appsport_driver = {
	.probe = appsport_probe,
	.remove = appsport_remove,
	.driver		= {
		.name	= "appsport",
		.of_match_table = appsport_dt_match,
	},
};

static int __init appsport_init(void)
{
	return platform_driver_register(&appsport_driver);
}
module_init(appsport_init);

static void __exit appsport_exit(void)
{
	platform_driver_unregister(&appsport_driver);
}
module_exit(appsport_exit);

MODULE_DESCRIPTION("LGE APPSPORT DRIVER");
MODULE_LICENSE("GPL v2");
