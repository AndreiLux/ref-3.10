/*
 * This software is used for bluetooth HW enable/disable control.
 *
 * Copyright (C) 2013 LGE Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>

#define BTRFKILL_DBG

#ifdef BTRFKILL_DBG
#define BTRFKILLDBG(fmt, arg...) \
	 pr_info("[BTRFKILL:  %s (%d)] " fmt "\n" , __func__, __LINE__, ## arg)
#else
#define BTRFKILLDBG(fmt, arg...) do { } while (0)
#endif

#define BT_SET_HOST_WAKE_GPIOMUX 1	/* This is a test feature for bring up without LPM. */

struct bluetooth_rfkill_platform_data {
	int gpio_reset;
};

struct bluetooth_rfkill_device {
	struct rfkill *rfk;
	int gpio_bt_reset;
	struct pinctrl *bt_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};

static int bluetooth_set_power(void *data, bool blocked)
{
	struct bluetooth_rfkill_device *bdev = data;
	struct pinctrl_state *pins_state;

	BTRFKILLDBG("set blocked %d", blocked);

	if (!IS_ERR_OR_NULL(bdev->bt_pinctrl)) {
		pins_state = blocked ? bdev->gpio_state_active
			: bdev->gpio_state_suspend;
		if (pinctrl_select_state(bdev->bt_pinctrl, pins_state)) {
			pr_err("%s: error on pinctrl_select_state for bt enable - %s\n", __func__,
					blocked ? "bt_enable_active" : "bt_enable_suspend");
		} else {
			BTRFKILLDBG("%s: success to set pinctrl for bt enable - %s\n", __func__,
					blocked ? "bt_enable_active" : "bt_enable_suspend");
		}
	}

	if (!blocked) {
		gpio_direction_output(bdev->gpio_bt_reset, 0);
		msleep(30);
		gpio_direction_output(bdev->gpio_bt_reset, 1);
		BTRFKILLDBG("Bluetooth RESET HIGH!!");
	} else {
		gpio_direction_output(bdev->gpio_bt_reset, 0);
		BTRFKILLDBG("Bluetooth RESET LOW!!");
	}
	return 0;
}

static int bluetooth_rfkill_parse_dt(struct device *dev,
		struct bluetooth_rfkill_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

#if (defined BT_SET_HOST_WAKE_GPIOMUX) && (BT_SET_HOST_WAKE_GPIOMUX == 1)
	pdata->gpio_reset = of_get_named_gpio_flags(np,
			"gpio-bt-host-wake", 0, NULL);
	if (pdata->gpio_reset >= 0) {
		pr_err("%s: gpio-bt-host-wake\n", __func__);
		return 1;
	}
#endif

	pdata->gpio_reset = of_get_named_gpio_flags(np,
			"gpio-bt-reset", 0, NULL);
	if (pdata->gpio_reset < 0) {
		pr_err("%s: failed to get gpio-bt-reset\n", __func__);
		return pdata->gpio_reset;
	}

	return 0;
}

int bt_enable_pinctrl_init(struct device *dev,
		struct bluetooth_rfkill_device *bdev)
{
	int retval = 0;

	bdev->gpio_state_suspend = 0;
	bdev->gpio_state_active = 0;
	bdev->bt_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(bdev->bt_pinctrl)) {
		pr_err("%s: target does not use pinctrl for bt enable\n", __func__);
		return -ENODEV;
	}

	bdev->gpio_state_active = pinctrl_lookup_state(bdev->bt_pinctrl, "bt_enable_active");
	if (IS_ERR_OR_NULL(bdev->gpio_state_active)) {
		pr_err("%s: can't get gpio_state_active for bt enable\n", __func__);
		retval = -ENODEV;
		goto err_active_state;
	}

	bdev->gpio_state_suspend = pinctrl_lookup_state(bdev->bt_pinctrl, "bt_enable_suspend");
	if (IS_ERR_OR_NULL(bdev->gpio_state_suspend)) {
		pr_err("%s: can't get gpio_state_suspend for bt enable\n", __func__);
		retval = -ENODEV;
		goto err_suspend_state;
	}

	if (pinctrl_select_state(bdev->bt_pinctrl, bdev->gpio_state_suspend)) {
		pr_err("%s: error on pinctrl_select_state for bt enable\n", __func__);
		retval = -ENODEV;
		goto err_suspend_state;
	} else
		pr_info("%s: success to set pinctrl_select_state for bt enable\n", __func__);

	return retval;

err_suspend_state:
	bdev->gpio_state_suspend = 0;

err_active_state:
	bdev->gpio_state_active = 0;
	devm_pinctrl_put(bdev->bt_pinctrl);
	bdev->bt_pinctrl = 0;

	return retval;
}

static struct rfkill_ops bluetooth_rfkill_ops = {
	.set_block = bluetooth_set_power,
};

static int bluetooth_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;
	bool default_state = true;  /* off */
	struct bluetooth_rfkill_platform_data *pdata;
	struct bluetooth_rfkill_device *bdev;

#if (defined BT_SET_HOST_WAKE_GPIOMUX) && (BT_SET_HOST_WAKE_GPIOMUX == 1)
	struct pinctrl *bt_pinctrl;
	struct pinctrl_state *gpio_state_active;
#endif

	BTRFKILLDBG("entry");

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct bluetooth_rfkill_platform_data),
				GFP_KERNEL);
		if (pdata == NULL) {
			pr_err("%s: no pdata\n", __func__);
			return -ENOMEM;
		}
		pdev->dev.platform_data = pdata;
		rc = bluetooth_rfkill_parse_dt(&pdev->dev, pdata);
		if (rc < 0) {
			pr_err("%s: failed to parse device tree\n", __func__);
			return rc;
		}
	} else {
		pdata = pdev->dev.platform_data;
	}

	if (!pdata) {
		pr_err("%s: no pdata\n", __func__);
		return -ENODEV;
	}

	bdev = kzalloc(sizeof(struct bluetooth_rfkill_device), GFP_KERNEL);
	if (!bdev) {
		pr_err("%s: no memory\n", __func__);
		return -ENOMEM;
	}

	bdev->gpio_bt_reset = pdata->gpio_reset;
	platform_set_drvdata(pdev, bdev);

#if (defined BT_SET_HOST_WAKE_GPIOMUX) && (BT_SET_HOST_WAKE_GPIOMUX == 1)
	if (rc == 1) {
		gpio_request_one(bdev->gpio_bt_reset, GPIOF_IN, "bt_host_wake");

		bt_pinctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR_OR_NULL(bt_pinctrl)) {
			pr_err("%s: target does not use pinctrl for host wake\n", __func__);
			return -ENODEV;
		}
		gpio_state_active = pinctrl_lookup_state(bt_pinctrl, "bt_active");
		if (IS_ERR_OR_NULL(gpio_state_active)) {
			pr_err("%s: can't get gpio_state_active for host wake\n", __func__);
			return -ENODEV;
		}

		if (pinctrl_select_state(bt_pinctrl, gpio_state_active))
			pr_err("%s: error on pinctrl_select_state for host wake\n", __func__);
		else
			pr_err("%s: success to set pinctrl_select_state for host wake\n", __func__);

		return -ENODEV;
	}
#endif

	rc = gpio_request_one(bdev->gpio_bt_reset, GPIOF_OUT_INIT_LOW, "bt_reset");

	if (rc) {
		pr_err("%s: failed to request gpio(%d)\n", __func__,
				bdev->gpio_bt_reset);
		goto err_gpio_reset;
	}

	bluetooth_set_power(bdev, default_state);

	bdev->rfk = rfkill_alloc("brcm_Bluetooth_rfkill", &pdev->dev,
			RFKILL_TYPE_BLUETOOTH,
			&bluetooth_rfkill_ops, bdev);
	if (!bdev->rfk) {
		pr_err("%s: failed to alloc rfkill\n", __func__);
		rc = -ENOMEM;
		goto err_rfkill_alloc;
	}

	rfkill_set_states(bdev->rfk, default_state, false);

	/* userspace cannot take exclusive control */
	rc = rfkill_register(bdev->rfk);
	if (rc) {
		pr_err("%s: failed to register rfkill\n", __func__);
		goto err_rfkill_reg;
	}

	rc = bt_enable_pinctrl_init(&pdev->dev, bdev);
	if (rc) {
		pr_err("%s: failed to init pinctrl\n", __func__);
		goto err_rfkill_reg;
	}

	return 0;


err_rfkill_reg:
	rfkill_destroy(bdev->rfk);
err_rfkill_alloc:
	gpio_free(bdev->gpio_bt_reset);
err_gpio_reset:
	kfree(bdev);
	return rc;
}

static int bluetooth_rfkill_remove(struct platform_device *pdev)
{
	struct bluetooth_rfkill_device *bdev = platform_get_drvdata(pdev);

	if (!IS_ERR_OR_NULL(bdev->bt_pinctrl)) {
		bdev->gpio_state_active = 0;
		bdev->gpio_state_suspend = 0;
		devm_pinctrl_put(bdev->bt_pinctrl);
		bdev->bt_pinctrl = 0;
	}

	rfkill_unregister(bdev->rfk);
	rfkill_destroy(bdev->rfk);
	gpio_free(bdev->gpio_bt_reset);
	platform_set_drvdata(pdev, NULL);
	kfree(bdev);
	return 0;
}

static struct of_device_id bluetooth_rfkil_match_table[] = {
#if (defined BT_SET_HOST_WAKE_GPIOMUX) && (BT_SET_HOST_WAKE_GPIOMUX == 1)
	{ .compatible = "lge,bluetooth_host_wake", },
#endif
	{ .compatible = "lge,bluetooth_rfkill", },
	{ },
};

static struct platform_driver bluetooth_rfkill_driver = {
	.probe = bluetooth_rfkill_probe,
	.remove = bluetooth_rfkill_remove,
	.driver = {
		.name = "bluetooth_rfkill",
		.owner = THIS_MODULE,
		.of_match_table = bluetooth_rfkil_match_table,
	},
};

static int __init bluetooth_rfkill_init(void)
{
	return platform_driver_register(&bluetooth_rfkill_driver);
}

static void __exit bluetooth_rfkill_exit(void)
{
	platform_driver_unregister(&bluetooth_rfkill_driver);
}

module_init(bluetooth_rfkill_init);
module_exit(bluetooth_rfkill_exit);

MODULE_DESCRIPTION("bluetooth rfkill");
MODULE_AUTHOR("Devin Kim <dojip.kim@lge.com>");
MODULE_LICENSE("GPL");
