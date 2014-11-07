/*
 * BQ51013 Wireless Charging(WLC) interface(MAX14670) driver
 *
 * Copyright (C) 2014 LG Electronics, Inc
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/power_supply.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/delay.h>

#include <linux/power/max14670.h>

static const struct platform_device_id max14670_id[] = {
	{MAX14670_WLC_DEV_NAME, 0},
	{},
};

static struct max14670_wlc_chip *the_chip = NULL;
static bool wireless_charging;
static bool wireless_charge_done;

#ifdef CONFIG_OF
static int max14670_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;

	if(of_property_read_u32(np, "wl_chg_int",
				&the_chip->wl_chg_int) != 0)
		return -EINVAL;
	WLC_DBG_INFO("wl_chg_int gpio = %d\n", the_chip->wl_chg_int);

	if(of_property_read_u32(np, "wl_chg_full",
				&the_chip->wl_chg_full) != 0)
		return -EINVAL;
	WLC_DBG_INFO("wl_chg_full gpio= %d\n", the_chip->wl_chg_full);
}
#endif

static int max14670_init_gpio(struct max14670_wlc_chip *chip)
{
	int ret = 0;

	ret = gpio_request_one(chip->wl_chg_int, GPIOF_DIR_IN, "wl_chg_int");
	if (ret < 0) {
		pr_err("failed to request gpio %d\n", chip->wl_chg_int);
		goto gpio_free1;
	}

	/* Rev.F (sms-gpio 37) --> Rev.G (sms-gpio 21)
	ret = gpio_request_one(chip->wl_chg_full, GPIOF_OUT_INIT_LOW, "wl_chg_full");
	if (ret < 0) {
		pr_err("failed to request gpio %d\n", chip->wl_chg_full);
		goto gpio_free2;
	}
	*/

	return ret;

gpio_free1:
	gpio_free(chip->wl_chg_int);
gpio_free2:
	gpio_free(chip->wl_chg_full);

	return ret;
}

int wlc_is_plugged(void)
{
	static bool initialized = false;
	unsigned int wlc_chg_int = 0;
	int ret = 0;

	if(the_chip == NULL){
		WLC_DBG_INFO("wlc_is_plugged <<the_chip is not initialized yet>>\n");
		return 0;
	}

	wlc_chg_int = the_chip->wl_chg_int;
	if (!wlc_chg_int) {
		pr_warn("wlc : active_n gpio is not defined yet");
		return 0;
	}

	ret = !gpio_get_value(the_chip->wl_chg_int);
	WLC_DBG_INFO("wlc_is_plugged = (%d)\n",ret);

	return ret;
}
EXPORT_SYMBOL(wlc_is_plugged);

static enum power_supply_property pm_power_props_wireless[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static char *pm_power_supplied_to[] = {
	"battery",
};

static int pm_power_get_property_wireless(struct power_supply *psy,
					  enum power_supply_property psp,
					  union power_supply_propval *val)
{
	/* Check if called before init */
	if (!the_chip)
		return -EINVAL;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = wireless_charging;	/* always battery_on */
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = wireless_charging;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void wireless_set(struct max14670_wlc_chip *chip)
{
	WLC_DBG_INFO("wireless_set\n");

#ifdef LGE_USED_WAKELOCK_MAX14670
	wake_lock(&chip->wireless_chip_wake_lock);
#endif

	wireless_charging = true;
	wireless_charge_done = false;

	power_supply_changed(&chip->wireless_psy);
	/* TODO : charger notification */
	/* set_wireless_power_supply_control(wireless_charging); */
}

static void wireless_reset(struct max14670_wlc_chip *chip)
{
	WLC_DBG_INFO("wireless_reset\n");

	wireless_charging = false;
	wireless_charge_done = false;

	power_supply_changed(&chip->wireless_psy);
	/* TODO : charger notification */
	/* set_wireless_power_supply_control(wireless_charging); */

#ifdef LGE_USED_WAKELOCK_MAX14670
	wake_unlock(&chip->wireless_chip_wake_lock);
#endif
}

static void wireless_interrupt_worker(struct work_struct *work)
{
	struct max14670_wlc_chip *chip =
	    container_of(work, struct max14670_wlc_chip,
			 wireless_interrupt_work);

	WLC_DBG_INFO("wireless_interrupt_worker\n");
	if (wireless_charging){
		wireless_set(chip);
	} else {
		wireless_reset(chip);
	}
}

int set_wireless_charger_status(int value)
{
	if(the_chip == NULL){
		WLC_DBG_INFO("set_wireless_charger_status <<the_chip is not initialized yet>>\n");
		return 0;
	}
	wireless_charging = value;
	schedule_work(&the_chip->wireless_interrupt_work);
}
EXPORT_SYMBOL(set_wireless_charger_status);

static irqreturn_t wireless_interrupt_handler(int irq, void *data)
{
	int chg_state;
	struct max14670_wlc_chip *chip = data;

	//chg_state = wlc_is_plugged();
	//WLC_DBG_INFO("\nwireless is plugged state = %d\n\n", chg_state);
	//schedule_work(&chip->wireless_interrupt_work);

	return IRQ_HANDLED;
}

static void wireless_eoc_work(struct work_struct *work)
{
	struct max14670_wlc_chip *chip = container_of(work,
			struct max14670_wlc_chip, wireless_eoc_work.work);

#ifdef LGE_USED_WAKELOCK_MAX14670
	wake_lock(&chip->wireless_eoc_wake_lock);
#endif

	/* Codes that have to be changed
	gpio_set_value(chip->wl_chg_full, 1);
	pr_err("%s Send EPT signal\n", __func__);
	msleep(3500);
	gpio_set_value(chip->wl_chg_full, 0);
	pr_err("%s Re-enable Rx\n", __func__);
	*/

#ifdef LGE_USED_WAKELOCK_MAX14670
	wake_unlock(&chip->wireless_eoc_wake_lock);
#endif
}

int wireless_charging_completed()
{
	WLC_DBG_INFO("%s\n", __func__);
	schedule_delayed_work(&the_chip->wireless_eoc_work,
			round_jiffies_relative(msecs_to_jiffies(2000)));
	return 1;
}
EXPORT_SYMBOL(wireless_charging_completed);

static int __init max14670_wlc_hw_init(struct max14670_wlc_chip *chip)
{
	int ret;
	WLC_DBG_INFO("hw_init\n");

	/* active_n pin must be monitoring the max14670 status */
	ret = request_threaded_irq(gpio_to_irq(chip->wl_chg_int), NULL,
			wireless_interrupt_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
			"wireless_charger", chip);

	if (ret < 0) {
		pr_err("wlc: wireless_charger request irq failed\n");
		goto free_gpio;
	}

	/* failed code
	ret = enable_irq_wake(gpio_to_irq(chip->wl_chg_int));
	if (ret < 0) {
		pr_err("wlc: enable_irq_wake failed\n");
		goto free_irq;
	}
	*/
	return 0;

free_irq:
	free_irq(gpio_to_irq(chip->wl_chg_int), NULL);
free_gpio:
	gpio_free(chip->wl_chg_int);

	return 0;
}

static int max14670_wlc_resume(struct device *dev)
{
	return 0;
}

static int max14670_wlc_suspend(struct device *dev)
{
	return 0;
}

static int __init max14670_wlc_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct max14670_wlc_chip *chip;

	WLC_DBG_INFO("probe\n");

	chip = kzalloc(sizeof(struct max14670_wlc_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("wlc: Cannot allocate max14670_wlc_chip\n");
		return -ENOMEM;
	}

	the_chip = chip;

	if (pdev->dev.of_node) {
		printk("wlc: probe: pdev->dev.of-node\n");
		max14670_parse_dt(&pdev->dev);
	}

	chip->dev = &pdev->dev;
	chip->wl_chg_int = the_chip->wl_chg_int;
	chip->wl_chg_full = the_chip->wl_chg_full;

	rc = max14670_init_gpio(chip);
	if (rc) {
		pr_err("wlc: couldn't init gpio rc = %d\n", rc);
		goto free_chip;
	}

	rc = max14670_wlc_hw_init(chip);
	if (rc) {
		pr_err("wlc: couldn't init hardware rc = %d\n", rc);
		goto free_chip;
	}

	chip->wireless_psy.name = "wireless";
	chip->wireless_psy.type = POWER_SUPPLY_TYPE_WIRELESS;
	chip->wireless_psy.supplied_to = pm_power_supplied_to;
	chip->wireless_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to);
	chip->wireless_psy.properties = pm_power_props_wireless;
	chip->wireless_psy.num_properties = ARRAY_SIZE(pm_power_props_wireless);
	chip->wireless_psy.get_property = pm_power_get_property_wireless;

	rc = power_supply_register(chip->dev, &chip->wireless_psy);
	if (rc < 0) {
		pr_err("wlc: power_supply_register wireless failed rx = %d\n",
			      rc);
		goto free_chip;
	}

	the_chip = chip;

	INIT_WORK(&chip->wireless_interrupt_work, wireless_interrupt_worker);
	INIT_DELAYED_WORK(&chip->wireless_eoc_work, wireless_eoc_work);

#ifdef LGE_USED_WAKELOCK_MAX14670
	wake_lock_init(&chip->wireless_chip_wake_lock, WAKE_LOCK_SUSPEND,
		       "max14670_wireless_chip");
	wake_lock_init(&chip->wireless_eoc_wake_lock, WAKE_LOCK_SUSPEND,
			"max14670_wireless_eoc");
#endif

	WLC_DBG_INFO("max14670_wlc_probe_finished\n");
	return 0;

free_chip:
	kfree(chip);

	return rc;
}

static int max14670_wlc_remove(struct platform_device *pdev)
{
	struct max14670_wlc_chip *chip = platform_get_drvdata(pdev);

	WLC_DBG_INFO("remove\n");
#ifdef LGE_USED_WAKELOCK_MAX14670
	wake_lock_destroy(&chip->wireless_chip_wake_lock);
	wake_lock_destroy(&chip->wireless_eoc_wake_lock);
#endif
	the_chip = NULL;
	platform_set_drvdata(pdev, NULL);
	power_supply_unregister(&chip->wireless_psy);
	free_irq(gpio_to_irq(chip->wl_chg_int), chip);
	gpio_free(chip->wl_chg_int);
	kfree(chip);
	return 0;
}

static const struct dev_pm_ops max14670_pm_ops = {
	.suspend = max14670_wlc_suspend,
	.resume = max14670_wlc_resume,
};
#ifdef CONFIG_OF
static struct of_device_id max14670_match_table[] = {
	{ .compatible = "maxim,max14670_wlc"},
	{},
};
#endif

static struct platform_driver max14670_wlc_driver = {
	.probe = max14670_wlc_probe,
	.remove = max14670_wlc_remove,
	.id_table = max14670_id,
	.driver = {
		.name = MAX14670_WLC_DEV_NAME,
		.owner = THIS_MODULE,
		.pm = &max14670_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = max14670_match_table,
#endif
	},
};

static int __init max14670_wlc_init(void)
{
	return platform_driver_register(&max14670_wlc_driver);
}

static void __exit max14670_wlc_exit(void)
{
	platform_driver_unregister(&max14670_wlc_driver);
}

late_initcall(max14670_wlc_init);
module_exit(max14670_wlc_exit);

MODULE_AUTHOR("Kyungtae Oh <kyungtae.oh@lge.com>");
MODULE_DESCRIPTION("BQ51013 Wireless Charger Interface Driver");
MODULE_LICENSE("GPL v2");
