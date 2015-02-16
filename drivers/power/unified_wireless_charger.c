/*
 * IDPT9025A / BQ51020 Wireless Charging(WLC) control driver
 *
 * Copyright (C) 2012 LG Electronics, Inc
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */
#define DEBUG
#define pr_fmt(fmt)	"Wireless: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>

#include <linux/leds-pm8xxx.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/power/unified_wireless_charger.h>
#include <linux/delay.h>

#include <soc/qcom/lge/board_lge.h>
#include <linux/wakelock.h>

#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT
#include <linux/power/unified_wireless_charger_alignment.h>
#endif
#define MAX_CHECKING_COUNT	10
#define MAX_USB_CHECKING_COUNT	4
#define MAX_DISCONNECT_COUNT	4

#define _WIRELESS_ "wireless"
#define _BATTERY_     "battery"
#define _USB_		"usb"

struct unified_wlc_chip {
	struct device        *dev;
	struct power_supply  *psy_ext;
	struct power_supply	*psy_batt;
	struct power_supply	*dc_psy;
	struct power_supply   wireless_psy;
	struct delayed_work   wireless_interrupt_work;
	struct delayed_work   wireless_set_online_work;
	struct delayed_work   wireless_set_offline_work;
	struct delayed_work   wireless_eoc_work;
#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT
	struct delayed_work wireless_align_work;
#endif
	struct wake_lock      wireless_chip_wake_lock;
	struct wake_lock      wireless_eoc_wake_lock;

	unsigned int		wlc_full_chg;
	int					enabled;
	bool				wlc_state;
#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT
	struct mutex align_lock;
	unsigned int align_values;
#endif
};

static const struct platform_device_id unified_id[] = {
	{UNIFIED_WLC_DEV_NAME, 0},
	{},
};

static struct of_device_id unified_match[] = {
	{ .compatible = "lge,unified_wlc", },
	{}
};

static struct unified_wlc_chip *the_chip;
static bool wireless_charging;

static enum power_supply_property pm_power_props_wireless[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_WIRELESS_CHARGER_SWITCH,
#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT
	POWER_SUPPLY_PROP_ALIGNMENT,
#endif
};

static char *pm_power_supplied_to[] = {
	"battery",
};
int online_work_cnt;
int offline_work_cnt;
int disconnection_cnt;

static void wireless_inserted(struct unified_wlc_chip *chip);
static void wireless_removed(struct unified_wlc_chip *chip);
#if 0
static int wlc_thermal_mitigation = -1;

static int set_wlc_thermal_mitigation(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	if (!the_chip) {
		pr_err("called before init\n");
		return ret;
	}

	return 0;
}
module_param_call(wlc_thermal_mitigation, set_wlc_thermal_mitigation,
	param_get_int, &wlc_thermal_mitigation, 0644);
#endif
int is_wireless_charger_plugged_internal(struct unified_wlc_chip *chip)
{
	bool ret = chip->wlc_state;
	pr_info("[WLC] is_wireless_charger_plugged_internal, ret = %d\n", ret);

	return ret;

}

#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT
static int wireless_align_start(struct unified_wlc_chip *chip)
{
	int align = 0;

	chip->align_values = 0;
#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT_IDT9025A
	align = idtp9025_align_start();
#endif
	if (align > 0)
		chip->align_values = align;

	return align;
}

static int wireless_align_stop(struct unified_wlc_chip *chip)
{
	int ret = 0;
#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT_IDT9025A
	idtp9025_align_stop();
#endif
	chip->align_values = 0;
	return ret;
}

static int wireless_align_get_value(struct unified_wlc_chip *chip)
{
	int align = 0;
	int align_changed = 0;

#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT_IDT9025A
	align = idtp9025_align_get_value();
#endif

	if ((align > 0) && (chip->align_values != align)) {
		pr_info("\n alignment has been changed!! [%d][%d]\n\n", chip->align_values, align);
		chip->align_values = align;
		align_changed = true;
	}

	return align_changed;
}

static void wireless_align_proc(struct unified_wlc_chip *chip,
									bool attached)
{
	if (likely(attached)) {
		/* start work queue for alignment */
		wireless_align_start(chip);
		if (likely(delayed_work_pending(&chip->wireless_align_work)))
			flush_delayed_work(&chip->wireless_align_work);
		schedule_delayed_work(&chip->wireless_align_work,
					msecs_to_jiffies(WLC_ALIGN_INTERVAL));
	} else {
		cancel_delayed_work_sync(&chip->wireless_align_work);
		wireless_align_stop(chip);
	}
}
static void wireless_align_work(struct work_struct *work)
{
	struct unified_wlc_chip *chip = container_of(work,
				struct unified_wlc_chip, wireless_align_work.work);
	union power_supply_propval ret = {0,};
	int battery_capacity = 0;
	int align_changed = 0;
	int wlc_state = 0;

	if (!chip)
		return;
	wlc_state = chip->wlc_state;

	if (!wlc_state) {
		if (disconnection_cnt > MAX_DISCONNECT_COUNT) {
			wireless_removed(chip);
		} else {
			disconnection_cnt++;
			goto check_status;
		}
	}

	chip->psy_batt = power_supply_get_by_name("battery");

	if (!chip->psy_batt)
		return;

	chip->psy_batt->get_property(chip->psy_batt, POWER_SUPPLY_PROP_CAPACITY, &ret);
	battery_capacity = ret.intval;

	if ((!wireless_charging) || (battery_capacity == 100))
		goto check_status;

	mutex_lock(&chip->align_lock);
	align_changed = wireless_align_get_value(chip);
	mutex_unlock(&chip->align_lock);

	if (align_changed)
		power_supply_changed(&chip->wireless_psy);

check_status:
	schedule_delayed_work(&chip->wireless_align_work,
					msecs_to_jiffies(WLC_ALIGN_INTERVAL));
}
#endif

static int pm_power_set_property_wireless(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct unified_wlc_chip *chip =
			container_of(psy, struct unified_wlc_chip, wireless_psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_WIRELESS_CHARGER_SWITCH:
		chip->enabled = val->intval;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static int pm_power_get_property_wireless(struct power_supply *psy,
					 enum power_supply_property psp,
					 union power_supply_propval *val)
{
	struct unified_wlc_chip *chip =
			container_of(psy, struct unified_wlc_chip, wireless_psy);

	/* Check if called before init */
	/* todo workaround for below kmsg
	power_supply wireless: driver failed to report `present' property: 4294967274
	if (!the_chip)
		return -EINVAL;
	*/
	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = wireless_charging;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = wireless_charging;
		break;
	case POWER_SUPPLY_PROP_WIRELESS_CHARGER_SWITCH:
		val->intval = chip->enabled;
		break;
#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT
	case POWER_SUPPLY_PROP_ALIGNMENT:
		if (unlikely(!wireless_charging)) {
			val->intval = 0;
		} else {
			if (chip->align_values == 0) {
				mutex_lock(&chip->align_lock);
				wireless_align_get_value(chip);
				mutex_unlock(&chip->align_lock);
			}
			val->intval = chip->align_values;
		}
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int pm_power_set_event_property_wireless(struct power_supply *psy,
					enum power_supply_event_type psp,
					const union power_supply_propval *val)
{
	struct unified_wlc_chip *chip =
		container_of(psy, struct unified_wlc_chip, wireless_psy);

	switch(psp){
	case POWER_SUPPLY_PROP_WIRELESS_CHARGE_COMPLETED:
		pr_info("%s : ask POWER_SUPPLY_PROP_WIRELESS_CHARGE_COMPLETED", __func__);
		schedule_delayed_work(&chip->wireless_eoc_work,
			round_jiffies_relative(msecs_to_jiffies(2000)));
		break;
	default:
		return -EINVAL;
	}
	return 0;

}

static int pm_power_get_event_property_wireless(struct power_supply *psy,
					enum power_supply_event_type psp,
					union power_supply_propval *val)
{
	struct unified_wlc_chip *chip =
		container_of(psy, struct unified_wlc_chip, wireless_psy);

	switch(psp){
	case POWER_SUPPLY_PROP_WIRELESS_ONLINE:
		if(likely(chip)){
			val->intval = is_wireless_charger_plugged_internal(chip);
			pr_info("%s : POWER_SUPPLY_PROP_WIRELESS_ONLINE :%d\n", __func__, val->intval);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}


static void wireless_set_online_work(struct work_struct *work)
{
	int wlc = 0;
	struct unified_wlc_chip *chip = container_of(work,
				struct unified_wlc_chip, wireless_set_online_work.work);

	wlc = is_wireless_charger_plugged_internal(chip);
	online_work_cnt++;

	if (wlc) {
		pr_info("[WLC] %s : Wireless psy is updated to ONLINE !!!!!\n",
				__func__);
		wireless_charging = true;
		power_supply_changed(&chip->wireless_psy);
		return;
	}

	if (!wlc && (online_work_cnt < MAX_CHECKING_COUNT)) {
		pr_info("[WLC] %s:It was Ghost WLC (wlc(%d))..... Retry count : %d\n"
			, __func__, wlc, online_work_cnt);
		schedule_delayed_work(&chip->wireless_set_online_work,
			round_jiffies_relative(msecs_to_jiffies(100)));
	} else {
		pr_info("[WLC] %s:Cancel online work.\n", __func__);
		return;
	}
}

static void wireless_set_offline_work(struct work_struct *work)
{
	int wlc = 0;
	int usb_present = 0;
	struct power_supply *usb_psy;
	union power_supply_propval ret = {0,};
	struct unified_wlc_chip *chip = container_of(work,
		struct unified_wlc_chip, wireless_set_offline_work.work);

	wlc = is_wireless_charger_plugged_internal(chip);

	chip->psy_batt = power_supply_get_by_name("battery");

	if (!chip->psy_batt)
		return;
	usb_psy = power_supply_get_by_name("usb");
	usb_psy->get_property(usb_psy, POWER_SUPPLY_PROP_ONLINE, &ret);
	usb_present = ret.intval;

	offline_work_cnt++;

	if (!wlc) {
		if (usb_present || (offline_work_cnt > MAX_USB_CHECKING_COUNT)) {
			pr_info("[WLC] %s : USB Already updated or W/O USB (usb(%d) cnt(%d))\n"
						, __func__, usb_present, offline_work_cnt);
			if (wake_lock_active(&chip->wireless_chip_wake_lock)) {
				pr_debug("[WLC] %s : RELEASE wakelock\n", __func__);
				wake_unlock(&chip->wireless_chip_wake_lock);
			}
			return;
		} else {
			pr_info("[WLC] %s : Checking USB ........ Retry count : %d\n"
						, __func__, offline_work_cnt);
			schedule_delayed_work(&chip->wireless_set_offline_work,
					round_jiffies_relative(msecs_to_jiffies(1000)));
			return;
		}
	}
}

static void wireless_inserted(struct unified_wlc_chip *chip)
{
	pr_info("[WLC] %s\n", __func__);

	if (!wake_lock_active(&chip->wireless_chip_wake_lock)) {
		pr_info("[WLC] %s : ACQAURE wakelock\n", __func__);
		wake_lock(&chip->wireless_chip_wake_lock);
	}

#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT
		wireless_align_proc(chip, true);
#endif
	online_work_cnt = 0;
	disconnection_cnt = 0;

	schedule_delayed_work(&chip->wireless_set_online_work,
			round_jiffies_relative(msecs_to_jiffies(200)));
}

static void wireless_removed(struct unified_wlc_chip *chip)
{
	pr_info("[WLC] %s\n", __func__);

#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT
		wireless_align_proc(chip, false);
#endif

	offline_work_cnt = 0;
	pr_info("[WLC] %s : Wireless psy is updated to OFFLINE !!!\n", __func__);
	wireless_charging = false;
	chip->wlc_state = false;
	power_supply_changed(&chip->wireless_psy);
	schedule_delayed_work(&chip->wireless_set_offline_work,
		round_jiffies_relative(msecs_to_jiffies(500)));
}

static void wireless_interrupt_worker(struct work_struct *work)
{
	struct unified_wlc_chip *chip =
		container_of(work, struct unified_wlc_chip,
			 wireless_interrupt_work.work);

	pr_info("[WLC] %s\n", __func__);

	if (is_wireless_charger_plugged_internal(chip))
		wireless_inserted(chip);
	else
		wireless_removed(chip);
}

void wireless_interrupt_handler(bool dc_present)
{
	int chg_state;
	struct unified_wlc_chip *chip = the_chip;

	chip->wlc_state = dc_present;
	chg_state = is_wireless_charger_plugged_internal(chip);
	if (chg_state)

		pr_info("[WLC] %s : I'm on the WLC PAD\n", __func__);
	else
		pr_info("[WLC] %s : I'm NOT on the WLC PAD\n", __func__);

	schedule_delayed_work(&chip->wireless_interrupt_work, round_jiffies_relative(msecs_to_jiffies(100)));

	return;
}

void wireless_chg_term_handler(void)
{
	int chg_status = 0, chg_type = 0;

	struct unified_wlc_chip *chip = the_chip;
	struct power_supply *dc_psy;
	union power_supply_propval wlc_ret = {0,};

	chip->wlc_state = false;
	dc_psy = power_supply_get_by_name("dc");
	dc_psy->get_property(dc_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &wlc_ret);
	chg_type = wlc_ret.intval;

	dc_psy->get_property(dc_psy, POWER_SUPPLY_PROP_STATUS, &wlc_ret);
	chg_status = wlc_ret.intval;

	pr_info("[WLC] %s : POWER_SUPPLY_PROP_CHARGE_TYPE = %d\n", __func__, chg_type);
	pr_info("[WLC] %s : POWER_SUPPLY_PROP_STATUS = %d\n", __func__, chg_status);

	if(chg_type == POWER_SUPPLY_CHARGE_TYPE_NONE &&
			chg_status == POWER_SUPPLY_STATUS_FULL){
		wireless_charging = false;
		chip->wireless_psy.set_event_property(&(chip->wireless_psy), POWER_SUPPLY_PROP_WIRELESS_CHARGE_COMPLETED, &wlc_ret);
	}

	return;
}

static void wireless_eoc_work(struct work_struct *work)
{
	struct unified_wlc_chip *chip = container_of(work,
		struct unified_wlc_chip, wireless_eoc_work.work);

	wake_lock(&chip->wireless_eoc_wake_lock);

	gpio_set_value(chip->wlc_full_chg, 1);
	pr_info("[WLC] %s : Send EPT signal!!\n", __func__);
	msleep(3500);
	gpio_set_value(chip->wlc_full_chg, 0);
	pr_info("[WLC] %s : Re-enable RX!!\n", __func__);

	wake_unlock(&chip->wireless_eoc_wake_lock);
}

static int unified_wlc_hw_init(struct unified_wlc_chip *chip)
{
	int ret = 0;
	pr_info("[WLC] %s\n", __func__);

	/* wlc_enable DIR_IN and register IRQ */

	/* wlc_full_chg DIR_OUT and Low */
	ret = gpio_request_one(chip->wlc_full_chg, GPIOF_OUT_INIT_LOW,
			"wlc_full_chg");
	if (ret < 0) {
		pr_err("[WLC] %s : failed to request gpio wlc_full_chg\n", __func__);
		goto err_request_gpio1_failed;
	}

	return 0;
/*
err_request_gpio2_failed:
	gpio_free(chip->wlc_enable);
*/
err_request_gpio1_failed:
	return ret;
}

static int unified_wlc_resume(struct device *dev)
{
	struct unified_wlc_chip *chip = dev_get_drvdata(dev);

	if (!chip) {
		pr_err("called before init\n");
		return -ENODEV;
	}

	return 0;
}

static int unified_wlc_suspend(struct device *dev)
{
	struct unified_wlc_chip *chip = dev_get_drvdata(dev);

	if (!chip) {
		pr_err("called before init\n");
		return -ENODEV;
	}

	return 0;
}

static void unified_parse_dt(struct device *dev,
		struct unified_wlc_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	pdata->wlc_full_chg = of_get_named_gpio(np, "lge,wlc_full_chg", 0);
	
}

static int wlc_charger_property_is_writeable(struct power_supply *psy,
	enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_WIRELESS_CHARGER_SWITCH:
		return 1;

	default:
		break;
	}

	return -EINVAL;
}

static int unified_wlc_probe(struct platform_device *pdev)
{
	int rc = 0;

	struct unified_wlc_chip *chip;
	struct unified_wlc_platform_data *pdata;
	struct power_supply *dc_psy;

	pr_info("[WLC] %s : probe start\n", __func__);

	dc_psy = power_supply_get_by_name("dc");
	if (!dc_psy) {
		pr_err("DC supply not found, deferring probe\n");
		return -EPROBE_DEFER;
	}

	/*Read platform data from dts file*/

	pdata = devm_kzalloc(&pdev->dev,
					sizeof(struct unified_wlc_platform_data),
					GFP_KERNEL);
	if (!pdata) {
		pr_err("[WLC] %s : missing platform data\n", __func__);
		return -ENODEV;
	}

	if (pdev->dev.of_node) {
		pdev->dev.platform_data = pdata;
		unified_parse_dt(&pdev->dev, pdata);
	} else {
		pdata = pdev->dev.platform_data;
	}

	chip = kzalloc(sizeof(struct unified_wlc_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("[WLC] %s : Cannot allocate unified_wlc_chip\n", __func__);
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;

	chip->wlc_state = false;
	/*Set Power Supply type for wlc*/
	chip->wireless_psy.name = "wireless";
	chip->wireless_psy.type = POWER_SUPPLY_TYPE_WIRELESS;
	chip->wireless_psy.supplied_to = pm_power_supplied_to;
	chip->wireless_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to);
	chip->wireless_psy.properties = pm_power_props_wireless;
	chip->wireless_psy.num_properties = ARRAY_SIZE(pm_power_props_wireless);
	chip->wireless_psy.get_property = pm_power_get_property_wireless;
	chip->wireless_psy.set_property = pm_power_set_property_wireless;
	chip->wireless_psy.get_event_property = pm_power_get_event_property_wireless;
	chip->wireless_psy.set_event_property = pm_power_set_event_property_wireless;
	chip->wireless_psy.property_is_writeable =	wlc_charger_property_is_writeable;
	chip->dc_psy = dc_psy;
	rc = power_supply_register(chip->dev, &chip->wireless_psy);
	if (rc < 0) {
		pr_err("[WLC] %s : power_supply_register wireless failed rx = %d\n",
			__func__, rc);
		goto free_chip;
	}

	chip->dc_psy = power_supply_get_by_name("dc");
	if (!chip->dc_psy) {
		pr_err("dc supply not found.\n");
		return -ENODEV;
	}

	INIT_DELAYED_WORK(&chip->wireless_interrupt_work, wireless_interrupt_worker);
	INIT_DELAYED_WORK(&chip->wireless_set_online_work, wireless_set_online_work);
	INIT_DELAYED_WORK(&chip->wireless_set_offline_work, wireless_set_offline_work);
	INIT_DELAYED_WORK(&chip->wireless_eoc_work, wireless_eoc_work);
#ifdef CONFIG_LGE_PM_UNIFIED_WLC_ALIGNMENT
	INIT_DELAYED_WORK(&chip->wireless_align_work, wireless_align_work);
	mutex_init(&chip->align_lock);
	chip->align_values = 0;
#endif
	/*Set  Wake lock for wlc*/
	wake_lock_init(&chip->wireless_chip_wake_lock, WAKE_LOCK_SUSPEND,
				"unified_wireless_chip");
	wake_lock_init(&chip->wireless_eoc_wake_lock, WAKE_LOCK_SUSPEND,
				"unified_wireless_eoc");

	/*Set GPIO & Enable GPIO IRQ for wlc*/
	chip->wlc_full_chg = pdata->wlc_full_chg;

	rc = unified_wlc_hw_init(chip);
	if (rc) {
		pr_err("[WLC] %s : couldn't init hardware rc = %d\n", __func__, rc);
		goto free_chip;
	}

	platform_set_drvdata(pdev, chip);

	the_chip = chip;

	/* For Booting Wireless_charging and For Power Charging Logo
	 * In Wireless Charging
	 */
	if (is_wireless_charger_plugged_internal(chip)) {
		pr_info("[WLC] %s : I'm on WLC PAD during booting\n ", __func__);
		wireless_inserted(chip);
	}
	/*else
		wireless_removed(chip);*/
	chip->enabled = 0;
	pr_info("[WLC] %s : probe done\n", __func__);
	return 0;
free_chip:
	kfree(chip);
	return rc;
}

static int unified_wlc_remove(struct platform_device *pdev)
{
	struct unified_wlc_chip *chip = platform_get_drvdata(pdev);

	pr_info("[WLC] %s :remove\n", __func__);
	wake_lock_destroy(&chip->wireless_chip_wake_lock);
	wake_lock_destroy(&chip->wireless_eoc_wake_lock);
	the_chip = NULL;
	platform_set_drvdata(pdev, NULL);
	power_supply_unregister(&chip->wireless_psy);
	gpio_free(chip->wlc_full_chg);
	kfree(chip);
	return 0;
}

static const struct dev_pm_ops unified_pm_ops = {
	.suspend = unified_wlc_suspend,
	.resume = unified_wlc_resume,
};

static struct platform_driver unified_wlc_driver = {
	.probe = unified_wlc_probe,
	.remove = unified_wlc_remove,
	.id_table = unified_id,
	.driver = {
		.name = UNIFIED_WLC_DEV_NAME,
		.owner = THIS_MODULE,
		.pm = &unified_pm_ops,
		.of_match_table = unified_match,
	},
};

static int __init unified_wlc_init(void)
{
	return platform_driver_register(&unified_wlc_driver);
}

static void __exit unified_wlc_exit(void)
{
	platform_driver_unregister(&unified_wlc_driver);
}

late_initcall(unified_wlc_init);
module_exit(unified_wlc_exit);

MODULE_AUTHOR("Kyungtae Oh <Kyungtae.oh@lge.com>");
MODULE_DESCRIPTION("unified Wireless Charger Control Driver");
MODULE_LICENSE("GPL v2");
