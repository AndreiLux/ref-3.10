/*
 *  watchdog driver for Dialog D2260 PMIC
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
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/watchdog.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

#include <linux/mfd/d2260/registers.h>
#include <linux/mfd/d2260/core.h>

#define D2260_DEF_TIMEOUT	4
#define D2260_TWDMIN		256

struct d2260_wdt_data {
	struct watchdog_device wdt;
	struct d2260 *d2260;
	struct kref kref;
	unsigned long jpast;
};

static const struct {
	u8 reg_val;
	int time;  /* Seconds */
} d2260_wdt_maps[] = {
	{ 1, 2 },
	{ 2, 4 },
	{ 3, 8 },
	{ 4, 16 },
	{ 5, 32 },
	{ 5, 33 },  /* Actual time  32.768s so included both 32s and 33s */
	{ 6, 65 },
	{ 6, 66 },  /* Actual time 65.536s so include both, 65s and 66s */
	{ 7, 131 },
};

static void d2260_wdt_release_resources(struct kref *r)
{
	struct d2260_wdt_data *driver_data =
		container_of(r, struct d2260_wdt_data, kref);

	kfree(driver_data);
}

static int d2260_wdt_set_timeout(struct watchdog_device *wdt_dev,
		unsigned int timeout)
{
	struct d2260_wdt_data *driver_data = watchdog_get_drvdata(wdt_dev);
	struct d2260 *d2260 = driver_data->d2260;
	int ret, i;

	DIALOG_DEBUG(d2260->dev,"%s(WDT) timeout=%d\n",__FUNCTION__,timeout);

	/*
	* Disable the Watchdog timer before setting
	* new time out.
	*/
	ret = d2260_clear_bits(d2260, D2260_CONTROL_C_REG,
		D2260_TWDSCALE_MASK);
	if (ret < 0) {
		dev_err(d2260->dev, "Failed to disable watchdog bit, %d\n",
		ret);
		return ret;
	}

	if (timeout) {
		/*
		* To change the timeout, d2260 needs to
		* be disabled for at least 150 us.
		*/
		udelay(150);

		/* Set the desired timeout */
		for (i = 0; i < ARRAY_SIZE(d2260_wdt_maps); i++)
			if (d2260_wdt_maps[i].time == timeout)
				break;

		if (i == ARRAY_SIZE(d2260_wdt_maps))
			ret = -EINVAL;
		else {
			int value = D2260_TWDSCALE_MASK & (d2260_wdt_maps[i].reg_val << D2260_TWDSCALE_BIT);
			ret = d2260_set_bits(d2260, D2260_CONTROL_C_REG, value);
		}
		if (ret < 0) {
			dev_err(d2260->dev,
			"Failed to update timescale bit, %d\n", ret);
			return ret;
		}

		wdt_dev->timeout = timeout;
		driver_data->jpast = jiffies;
	}

	return 0;
}

static int d2260_wdt_start(struct watchdog_device *wdt_dev)
{
	struct d2260_wdt_data *driver_data = watchdog_get_drvdata(wdt_dev);
	struct d2260 *d2260 = driver_data->d2260;

	DIALOG_DEBUG(d2260->dev,"%s(WDT START) timeout=%d\n",__FUNCTION__,wdt_dev->timeout);
	return d2260_wdt_set_timeout(wdt_dev, wdt_dev->timeout);
}

static int d2260_wdt_stop(struct watchdog_device *wdt_dev)
{
	struct d2260_wdt_data *driver_data = watchdog_get_drvdata(wdt_dev);
	struct d2260 *d2260 = driver_data->d2260;

	DIALOG_DEBUG(d2260->dev,"%s(WDT STOP)\n",__FUNCTION__);
	return d2260_wdt_set_timeout(wdt_dev, 0);
}

static int d2260_wdt_ping(struct watchdog_device *wdt_dev)
{
	struct d2260_wdt_data *driver_data = watchdog_get_drvdata(wdt_dev);
	struct d2260 *d2260 = driver_data->d2260;
	unsigned long msec, jnow = jiffies;
	int ret;

	DIALOG_DEBUG(d2260->dev,"%s(start PING)\n",__FUNCTION__);
	/*
	* We have a minimum time for watchdog window called TWDMIN. A write
	* to the watchdog before this elapsed time should cause an error.
	*/
	msec = (jnow - driver_data->jpast) * 1000/HZ;
	if (msec < D2260_TWDMIN)
		mdelay(msec);

	ret = d2260_set_bits(d2260, D2260_CONTROL_C_REG, D2260_WATCHDOG_MASK); /* Reset the watchdog timer */
	DIALOG_DEBUG(d2260->dev,"%s(completed PING) ret=%d\n",__FUNCTION__,ret);

	return ret;
}

static struct watchdog_info d2260_wdt_info = {
	.options	= WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
	.identity	= "D2260 Watchdog",
};

static const struct watchdog_ops d2260_wdt_ops = {
	.owner = THIS_MODULE,
	.start = d2260_wdt_start,
	.stop = d2260_wdt_stop,
	.ping = d2260_wdt_ping,
	.set_timeout = d2260_wdt_set_timeout,
};

static int d2260_wdt_probe(struct platform_device *pdev)
{
	struct d2260 *d2260 = dev_get_drvdata(pdev->dev.parent);
	struct d2260_wdt_data *driver_data;
	struct watchdog_device *d2260_wdt;
	int ret;

	DIALOG_DEBUG(d2260->dev,"%s(Starting WDT)\n",__FUNCTION__);

	driver_data = devm_kzalloc(&pdev->dev, sizeof(*driver_data),
					GFP_KERNEL);
	if (!driver_data) {
		dev_err(d2260->dev, "Unable to alloacate watchdog device\n");
		ret = -ENOMEM;
		goto out;
	}
	driver_data->d2260 = d2260;

	d2260_wdt = &driver_data->wdt;

	d2260_wdt->timeout = D2260_DEF_TIMEOUT;
	d2260_wdt->info = &d2260_wdt_info;
	d2260_wdt->ops = &d2260_wdt_ops;
	watchdog_set_drvdata(d2260_wdt, driver_data);

	kref_init(&driver_data->kref);

	ret = d2260_clear_bits(d2260, D2260_CONTROL_C_REG, D2260_TWDSCALE_MASK);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to disable watchdog bits, %d\n",
				ret);
		goto err;
	}

	ret = watchdog_register_device(&driver_data->wdt);
	if (ret != 0) {
		dev_err(d2260->dev, "watchdog_register_device() failed: %d\n",
				ret);
		goto err;
	}

	dev_set_drvdata(&pdev->dev, driver_data);

	return ret;

err:
	kfree(driver_data);
out:
	DIALOG_DEBUG(d2260->dev, "%s(WDT registered) ret=%d\n",__FUNCTION__,ret);
	return ret;
}

static int d2260_wdt_remove(struct platform_device *pdev)
{
	struct d2260_wdt_data *driver_data = dev_get_drvdata(&pdev->dev);

	DIALOG_DEBUG(&pdev->dev, "%s(WDT removed)\n",__FUNCTION__);
	watchdog_unregister_device(&driver_data->wdt);
	kref_put(&driver_data->kref, d2260_wdt_release_resources);

	return 0;
}

static struct platform_driver d2260_wdt_driver = {
	.probe = d2260_wdt_probe,
	.remove = d2260_wdt_remove,
	.driver = {
		.name	= "d2260-watchdog",
	},
};

module_platform_driver(d2260_wdt_driver);

MODULE_AUTHOR("Tony Olech <anthony.olech@diasemi.com>");
MODULE_DESCRIPTION("D2260 Watchdog Device Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:d2260-watchdog");
