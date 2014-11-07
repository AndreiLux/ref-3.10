 /*
 * Copyright(c) 2012, LG Electronics Inc. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/mfd/max77819-charger.h>
#include <linux/wakelock.h>
#include <linux/mfd/d2260/pmic.h>
#include <linux/board_lge.h>
#include <linux/usb/odin_usb3.h>
#include <linux/mfd/max77807/max77807-charger.h>

struct batt_temp_pdata {
	int update_time;
	int temp_nums;
	int thr_mvolt;
	int i_decrease;
	int i_restore;
	int i_restore_usb;
	int i_restore_wlc;
};

enum {
	/* +60'C */TEMP_LEVEL_POWER_OFF = 0,
	/* +57'C */TEMP_LEVEL_WARNINGOVERHEAT,
	/* +55'C */TEMP_LEVEL_HOT_STOPCHARGING,
	/* +45'C */TEMP_LEVEL_DECREASING,
	/* +42'C */TEMP_LEVEL_HOT_RECHARGING,
	/* -5'C */TEMP_LEVEL_COLD_RECHARGING,
	/* -8'C */TEMP_LEVEL_WARNING_COLD,
	/* -10'C */TEMP_LEVEL_COLD_STOPCHARGING,
	TEMP_LEVEL_NORMAL
};

struct batt_temp_chip {
	struct power_supply *batt_psy;
	struct power_supply *ac_psy;
	struct workqueue_struct *bat_otp_internal_wq;
	struct delayed_work monitor_work;
	struct wake_lock bat_temp_wake_lock;
	struct wake_lock bat_temp_decrease_lock;
	struct batt_temp_pdata *pdata;
};

static struct batt_temp_chip *the_chip = NULL;

static int batt_temp_level[] = {
	600,
	570,
	550,
	450,
	420,
	-50,
	-80,
	-100,
};

static int temp_state = TEMP_LEVEL_NORMAL;
static int repeat_work = 0;
static int old_status = 0;
static int skip_flag = 0;
static int vbus_status = 0;
static int probe_done_check = 0;

extern unsigned int fake_battery_set;
extern void max77807_charger_set_charge_current_450mA(void);

void vbus_noty(int status)
{
	if(status){
		vbus_status = 1;
		printk("battery otp vbus_noty = 1\n");
	} else {
		vbus_status = 0;
		printk("battery otp vbus_noty = 0\n");
	}
}
EXPORT_SYMBOL(vbus_noty);

#if 1 /* This function will be changed to charging current setting &  Charging enable*/
static int power_supply_set_chg_enable(struct power_supply *psy, int enable)
{
	const union power_supply_propval ret = {enable,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_CHRG_ENABLE, &ret);

	return -ENXIO;
}

static int power_supply_set_current_limit(struct power_supply *psy, int limit)
{
	const union power_supply_propval ret = {limit,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &ret);
	return -ENXIO;
}

static int power_supply_set_health(struct power_supply *psy, int health)
{
	const union power_supply_propval ret = {health,};

	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_HEALTH, &ret);
	return -ENXIO;
}

static int power_supply_set_batt_temp_state(struct power_supply *psy, int temp_state)
{
	const union power_supply_propval ret = {temp_state,};
	if (psy->set_property)
		return psy->set_property(psy, POWER_SUPPLY_PROP_BATT_TEMP_STATE, &ret);
	return -ENXIO;
}

#endif

static struct batt_temp_pdata *batt_temp_parse_dt(struct device *dev)
{
	struct batt_temp_pdata *pdata;
	struct device_node *np = dev->of_node;
	u32 value;
	int ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	if (of_property_read_u32(np, "update_time", &value))
		value = 10000;
	pdata->update_time = value;

	if (of_property_read_u32(np, "thr_mvolt", &value))
		value = 4000;
	pdata->thr_mvolt = value;

	if (of_property_read_u32(np, "i_decrease", &value))
		value = 450;
	pdata->i_decrease = value;

	if (of_property_read_u32(np, "i_restore", &value))
		value = 1500;
	pdata->i_restore = value;

	if (of_property_read_u32(np, "i_restore_usb", &value))
		value = 500;
	pdata->i_restore_usb = value;

	if (of_property_read_u32(np, "i_restore_wlc", &value))
		value = 900;
	pdata->i_restore_wlc = value;

	pr_info("%s : pdata->thr_mvolt = (%d)\n",__func__,pdata->thr_mvolt);
	return pdata;
}

static int determin_batt_temp_state(struct batt_temp_pdata *pdata,
			int batt_temp, int batt_mvolt, int chg_present)
{
	if (chg_present) {
		pr_info("%s : chg_present = true\n", __func__);

		/* Over +60'C */
		if (batt_temp >= batt_temp_level[TEMP_LEVEL_POWER_OFF]) {
			temp_state = TEMP_LEVEL_POWER_OFF;

		/* Over +55'C */
		} else if (batt_temp >= batt_temp_level[TEMP_LEVEL_HOT_STOPCHARGING]) {
			temp_state = TEMP_LEVEL_HOT_STOPCHARGING;

		/* Over +45'C */
		} else if (batt_temp >= batt_temp_level[TEMP_LEVEL_DECREASING]) {
			if (batt_mvolt > pdata->thr_mvolt || temp_state == TEMP_LEVEL_HOT_STOPCHARGING) {
				temp_state = TEMP_LEVEL_HOT_STOPCHARGING;
			} else {
				temp_state = TEMP_LEVEL_DECREASING;
			}

		/* Over +42'C */
		} else if (batt_temp > batt_temp_level[TEMP_LEVEL_HOT_RECHARGING]) {
			if (temp_state == TEMP_LEVEL_HOT_STOPCHARGING) {
				temp_state = TEMP_LEVEL_HOT_STOPCHARGING;
			} else if (temp_state == TEMP_LEVEL_DECREASING) {
				temp_state = TEMP_LEVEL_DECREASING;
			} else {
				temp_state = TEMP_LEVEL_NORMAL;
			}

		/* Over -5'C */
		} else if (batt_temp >= batt_temp_level[TEMP_LEVEL_COLD_RECHARGING]) {
			if (temp_state == TEMP_LEVEL_HOT_STOPCHARGING || temp_state == TEMP_LEVEL_DECREASING){
				temp_state = TEMP_LEVEL_HOT_RECHARGING;
			}
			else if (temp_state == TEMP_LEVEL_COLD_STOPCHARGING){
				temp_state = TEMP_LEVEL_COLD_RECHARGING;
			}
			else{
				temp_state = TEMP_LEVEL_NORMAL;
			}

		/* Under -10'C */
		} else if (batt_temp <= batt_temp_level[TEMP_LEVEL_COLD_STOPCHARGING]) {
			temp_state = TEMP_LEVEL_COLD_STOPCHARGING;


		/* Under -5'C */
		} else if (batt_temp < batt_temp_level[TEMP_LEVEL_COLD_RECHARGING]) {
			if (temp_state == TEMP_LEVEL_COLD_STOPCHARGING) {
				temp_state = TEMP_LEVEL_COLD_STOPCHARGING;
			} else {
				temp_state = TEMP_LEVEL_NORMAL;
			}

		/* Under +42'C */
		} else if (batt_temp <= batt_temp_level[TEMP_LEVEL_HOT_RECHARGING]) {
			if (temp_state == TEMP_LEVEL_HOT_STOPCHARGING)
				temp_state = TEMP_LEVEL_HOT_RECHARGING;
			else if (temp_state == TEMP_LEVEL_COLD_STOPCHARGING)
				temp_state = TEMP_LEVEL_COLD_RECHARGING;
			else
				temp_state = TEMP_LEVEL_NORMAL;

		/* Default */
		} else {
			temp_state = TEMP_LEVEL_NORMAL;
		}
	} else {
		pr_info("%s : chg_present = false\n", __func__);

		if (batt_temp >= batt_temp_level[TEMP_LEVEL_POWER_OFF])
			temp_state = TEMP_LEVEL_POWER_OFF;
		else if(batt_temp >= batt_temp_level[TEMP_LEVEL_WARNINGOVERHEAT])
			temp_state = TEMP_LEVEL_WARNINGOVERHEAT;
		else
			temp_state = TEMP_LEVEL_NORMAL;
	}

	if(temp_state == TEMP_LEVEL_DECREASING){
		/* Hold Wakelock and Repeat Workque */
		pr_info("%s : Hold Battery Temperature Decrease Wakelock\n", __func__);
		wake_lock(&the_chip->bat_temp_decrease_lock);
		repeat_work = 1;
	} else {
		/* Release Wakelock and Stop Workque */
		if (wake_lock_active(&the_chip->bat_temp_decrease_lock)){
			wake_unlock(&the_chip->bat_temp_decrease_lock);
			pr_info("%s : Release Battery Temperature Decrease Wakelock\n", __func__);
		}
		repeat_work = 0;
	}

	/* Compare Decreasing repeat status */
	if(old_status == TEMP_LEVEL_DECREASING && temp_state == TEMP_LEVEL_DECREASING){
		skip_flag = 1;
		pr_info("%s : Skip Falg SET\n", __func__);
		/* Save Before Status for Skip Decreasing Sequence */
		old_status = temp_state;
		return temp_state;
	} else {
		skip_flag = 0;
		/* Save Before Status for Skip Decreasing Sequence */
		old_status = temp_state;
	}

	if(fake_battery_set == 0) {
		if(chg_present) {
			/* Set D2260 Battery LOW/HIGH Interrupt Threshold */
			if(temp_state == TEMP_LEVEL_POWER_OFF) {
				/* Set Temp Threshold Min(+41.5'C) MAX(+80'C) */
				d2260_hwmon_set_bat_threshold(415, 800);
			} else if(temp_state == TEMP_LEVEL_WARNINGOVERHEAT) {
				/* Set Temp Threshold Min(+41.5'C) MAX(+60'C) */
				d2260_hwmon_set_bat_threshold(415, 600);
			} else if(temp_state == TEMP_LEVEL_HOT_STOPCHARGING) {
				/* Set Temp Threshold Min(+41.5'C) MAX(+60'C) */
				d2260_hwmon_set_bat_threshold(415, 600);
			} else if(temp_state == TEMP_LEVEL_DECREASING) {
				/* Set Temp Threshold Min(+41.5'C) MAX(+55'C) */
				d2260_hwmon_set_bat_threshold(415, 555);
			} else if(temp_state == TEMP_LEVEL_HOT_RECHARGING) {
				/* Set Temp Threshold Min(-10'C) MAX(+45'C) */
				d2260_hwmon_set_bat_threshold(-105, 455);
			} else if(temp_state == TEMP_LEVEL_COLD_RECHARGING) {
				/* Set Temp Threshold Min(-10'C) MAX(+45'C) */
				d2260_hwmon_set_bat_threshold(-105, 455);
			} else if(temp_state == TEMP_LEVEL_WARNING_COLD) {
				/* Set Temp Threshold Min(-10'C) MAX(+45'C) */
				d2260_hwmon_set_bat_threshold(-105, 455);
			} else if(temp_state == TEMP_LEVEL_COLD_STOPCHARGING) {
				/* Set Temp Threshold Min(-30'C) MAX(-5'C) */
				d2260_hwmon_set_bat_threshold(-300, -45);
			} else if(temp_state == TEMP_LEVEL_NORMAL){
				/* Set Temp Threshold Min(-10'C) MAX(+45'C) */
				d2260_hwmon_set_bat_threshold(-105, 455);
			} else {
			}
		}else {
			/* No Cable */
			if(temp_state == TEMP_LEVEL_POWER_OFF) {
				/* Set Temp Threshold Min(+42'C) MAX(+80'C) */
				d2260_hwmon_set_bat_threshold(420, 800);
			} else if(temp_state == TEMP_LEVEL_WARNINGOVERHEAT) {
				/* Set Temp Threshold Min(+42'C) MAX(+60'C) */
				d2260_hwmon_set_bat_threshold(570, 600);
			} else if(temp_state == TEMP_LEVEL_NORMAL) {
				/* Set Temp Threshold Min(+42'C) MAX(+57'C) */
				d2260_hwmon_set_bat_threshold(420, 570);
			} else {
			}
		}
	}

	return temp_state;
}

extern unsigned int wlc_chrg_sts;
extern unsigned int wlc_noty_otp;
static void batt_temp_monitor_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct batt_temp_chip *chip = container_of(dwork,
					struct batt_temp_chip, monitor_work);
	struct batt_temp_pdata *pdata = chip->pdata;

	int batt_temp = 0;
	int batt_mvolt = 0;
	int chg_present = 0;
	int health_state = POWER_SUPPLY_HEALTH_GOOD;
	int batt_raw_temp = 0;
	int cable_type_val = 0;
	union power_supply_propval ret = {0,};
	int rc;

	pr_info("%s\n", __func__);

	/* Skip Battery Temperature Control without Battery */
	batt_raw_temp = d2260_get_battery_temp();
	if(batt_raw_temp < -300){
		pr_info("%s : No Battery Skip Battery Temperature Control\n", __func__);
		return;
	}

	/* Skip Battery Temperature Control with Facotry Cable */
	if((lge_get_boot_mode() >= LGE_BOOT_MODE_FACTORY) && (lge_get_boot_mode() <= LGE_BOOT_MODE_PIFBOOT3)){
		pr_info("%s : Factory Cable Skip Battery Temperature Control\n", __func__);
		return;
	}

	/* Skip OTP Scenario with Wireless charging */
	if(wlc_chrg_sts) {
		return;
	}

	/* Ensure Battery Temperature Control Function */
	wake_lock(&chip->bat_temp_wake_lock);

	/* Get Battery & Charger Status */
	rc = chip->batt_psy->get_property(chip->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &ret);
	if (rc) {
		pr_err("%s: failed to get voltage\n", __func__);
		return;
	}
	batt_mvolt = ret.intval/1000;

	rc = chip->batt_psy->get_property(chip->batt_psy, POWER_SUPPLY_PROP_TEMP, &ret);
	if (rc) {
		pr_err("%s: failed to get temperature\n", __func__);
		return;
	}
	batt_temp = ret.intval;

	chg_present = vbus_status;

	/* Choose Next Battery Temperature Action */
	temp_state = determin_batt_temp_state(pdata, batt_temp, batt_mvolt, chg_present);
	pr_info("%s: batt_temp = %d batt_mvolt = %d state = %d\n", __func__,
			batt_temp, batt_mvolt, temp_state);

	if(skip_flag){
		if(repeat_work){
			pr_info("%s : Decreasing Phase => Voltage & Battery Temperature Monitoring_<1>\n",__func__);

			/* Check Charging Current if(Charging Current > 450mA) => Set Charging Current 450mA */
			max77807_charger_set_charge_current_450mA();

			//schedule_delayed_work(&chip->monitor_work,round_jiffies_relative(msecs_to_jiffies(pdata->update_time)));
			queue_delayed_work(chip->bat_otp_internal_wq, &chip->monitor_work, round_jiffies_relative(msecs_to_jiffies(pdata->update_time)));
		}
		return;
	}

	power_supply_set_batt_temp_state(chip->ac_psy, temp_state) ;

	cable_type_val = max77807_cable_type_return();

	pr_info("%s: cable_type = %d\n", __func__,cable_type_val);

	switch (temp_state) {
	case TEMP_LEVEL_POWER_OFF:
		/* Over 60 degree Celcius need Power Off Alert Message */
	case TEMP_LEVEL_HOT_STOPCHARGING:
		/* over 55 degree celcius, hot stop charging->not indicate charging mark & LED light */
		/* Over 55 degree celcius need Alert Message */
		pr_info("%s: stop charging!! state = %d temp = %d mvolt = %d \n",
				__func__, temp_state, batt_temp, batt_mvolt);

		power_supply_set_chg_enable(chip->ac_psy, false);
		if (rc) {
			pr_err("%s: failed to set battery charging enable(false)\n", __func__);
		}
		pr_info("%s Battery Temp HOT Interrupt Set Charging Current & Enable \n",__func__);
		power_supply_set_current_limit(chip->ac_psy, 0);
		health_state = POWER_SUPPLY_HEALTH_OVERHEAT;
		break;
	case TEMP_LEVEL_COLD_STOPCHARGING:
		pr_info("%s: stop charging!! state = %d temp = %d mvolt = %d \n",
				__func__, temp_state, batt_temp, batt_mvolt);
		/* under -10 degree celcius, cold stop charging */

		power_supply_set_chg_enable(chip->ac_psy, false);
		if (rc) {
			pr_err("%s: failed to set battery charging enable(false)\n", __func__);
		}
		pr_info("%s Battery Temp COLD Interrupt Set Charging Current & Enable \n",__func__);
		power_supply_set_current_limit(chip->ac_psy, 0);
		health_state = POWER_SUPPLY_HEALTH_COLD;
		break;
	case TEMP_LEVEL_WARNINGOVERHEAT:
		health_state = POWER_SUPPLY_HEALTH_OVERHEAT;
		break;
	case TEMP_LEVEL_DECREASING:
		pr_info("%s: decrease current!! state = %d temp = %d mvolt = %d \n",
				__func__, temp_state, batt_temp, batt_mvolt);
		/* Over 45 degree celcius, decrease current to 450mA */
		/* over 42 under 55 degree celcius, charging current 450mA */
		/* over 45 under 55 degree celcius, check batt voltage->if over 4.0V ->stop charging->indicate charging mark & led (46 ~ -11 degree celcius, over 4.0V) */
		if(batt_mvolt > 4000) {
			power_supply_set_chg_enable(chip->ac_psy, false);
			if (rc) {
				pr_err("%s: failed to set battery charging enable(false)_decrease_phase\n", __func__);
			}
			pr_info("%s : TEMP_LEVEL_DECREASING => Over 4v Stop Charging\n", __func__);
		} else {
			power_supply_set_current_limit(chip->ac_psy, pdata->i_decrease* 1000);
				if (rc) {
					pr_err("%s: failed to set battery i_decrease_2\n", __func__);
				}
			pr_info("%s : TEMP_LEVEL_DECREASING => Under 4v set TA Current (%d)mA\n", __func__,pdata->i_decrease);
		}

		health_state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case TEMP_LEVEL_COLD_RECHARGING:
		/* over -5 degree celcius, start recharging */
	case TEMP_LEVEL_HOT_RECHARGING:
		pr_info("%s: restart charging!! state = %d temp = %d mvolt = %d \n",
				__func__, temp_state, batt_temp, batt_mvolt);
		/* under 42 degree celcius, start recharging */

		power_supply_set_chg_enable(chip->ac_psy, true);
		if (rc) {
			pr_err("%s: failed to set battery charging enable(true)\n", __func__);
		}
		if(cable_type_val == BC_CABLETYPE_SDP){
			power_supply_set_current_limit(chip->ac_psy, pdata->i_restore_usb * 1000);
			if (rc) {
				pr_err("%s: failed to set battery i_restore_usb\n", __func__);
			}
		} else {
			if(wlc_noty_otp) {
				power_supply_set_current_limit(chip->ac_psy, pdata->i_restore_wlc* 1000);
				if (rc) {
					pr_err("%s: failed to set battery i_restore_wlc\n", __func__);
				}
			} else {
				power_supply_set_current_limit(chip->ac_psy, pdata->i_restore * 1000);
				if (rc) {
					pr_err("%s: failed to set battery i_restore_TA\n", __func__);
				}
			}
		}
		pr_info("%s Battery Temp RECHRG Interrupt Set Charging Current & Enable \n",__func__);

		health_state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case TEMP_LEVEL_NORMAL:
		/* over -10 under 45 degree celcius, charging 1500mA */
		health_state = POWER_SUPPLY_HEALTH_GOOD;
	default:
		break;
	}

	power_supply_set_health(chip->batt_psy,health_state);
	if (rc) {
		pr_err("%s: failed to set battery health(%d)\n", __func__,rc);
	}

	if(repeat_work){
		pr_info("%s : Decreasing Phase => Voltage & Battery Temperature Monitoring_<2>\n",__func__);

		/* Check Charging Current if(Charging Current > 450mA) => Set Charging Current 450mA */
		max77807_charger_set_charge_current_450mA();

		//schedule_delayed_work(&chip->monitor_work,round_jiffies_relative(msecs_to_jiffies(pdata->update_time)));
		queue_delayed_work(chip->bat_otp_internal_wq, &chip->monitor_work, round_jiffies_relative(msecs_to_jiffies(pdata->update_time)));
	}

	wake_unlock(&chip->bat_temp_wake_lock);

}

void batt_temp_ctrl_start(int reset)
{
	if(the_chip == NULL){
		pr_info("%s = <<<the_chip is not initialized yet>>>\n",__func__);
		return;
	}

	if(reset){
		pr_info("%s: VBUS Changed Control Reset\n",__func__);
		temp_state = TEMP_LEVEL_NORMAL;
	} else {
		pr_info("%s: Temperature Changed Keep temp_state(%d)\n",__func__,temp_state);
	}

	if(probe_done_check != 1) {
		pr_info("%s: probe is not done yet => return\n",__func__);
		return;
	}
	queue_delayed_work(the_chip->bat_otp_internal_wq, &the_chip->monitor_work, round_jiffies_relative(msecs_to_jiffies(0)));
}
EXPORT_SYMBOL(batt_temp_ctrl_start);

static int batt_temp_ctrl_suspend(struct device *dev)
{
	struct batt_temp_chip *chip = dev_get_drvdata(dev);
#if 0 //Nothing To do
	cancel_delayed_work_sync(&chip->monitor_work);
#endif
	return 0;
}

static int batt_temp_ctrl_resume(struct device *dev)
{
	struct batt_temp_chip *chip = dev_get_drvdata(dev);
#if 0  //Nothing To do
	schedule_delayed_work(&chip->monitor_work,round_jiffies_relative(msecs_to_jiffies(30000)));
#endif
	return 0;
}

static int batt_temp_ctrl_probe(struct platform_device *pdev)
{
	struct batt_temp_chip *chip;
	struct device *dev = &pdev->dev;
	struct batt_temp_pdata *pdata;
	int rc = 0;

	pr_info("%s start\n", __func__);
	probe_done_check = 0;


	pdata = batt_temp_parse_dt(dev);
	if (!pdata) {
		pr_err("%s: no pdata\n", __func__);
		return -ENODEV;
	}

	chip = kzalloc(sizeof(struct batt_temp_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	chip->pdata = pdata;
	platform_set_drvdata(pdev, chip);

	if (!chip->batt_psy) {
	//	chip->batt_psy = power_supply_get_by_name("max17048");
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy) {
			pr_err("%s: failed to get power supply\n", __func__);
			return;
		}
	}

	if (!chip->ac_psy) {
		chip->ac_psy = power_supply_get_by_name("ac");
		if (!chip->ac_psy) {
			pr_err("%s: failed to get power supply\n", __func__);
			return;
		}
	}

	the_chip = chip;

	/* Make Battery OTP Driver own Workque */
	chip->bat_otp_internal_wq = create_workqueue("Battery_OTP_Driver_internal_workqueue");
	if (!chip->bat_otp_internal_wq) {
		pr_err("Can't create Battery_OTP_Driver_internal_workqueue\n");
		rc = -ENOMEM;
		return rc;
	}

	INIT_DELAYED_WORK(&chip->monitor_work, batt_temp_monitor_work);

	/* Prevent suspend until temperature control work finished */
	wake_lock_init(&chip->bat_temp_wake_lock, WAKE_LOCK_SUSPEND,
			"battery_temp_ctrl");

	/* Prevent suspend until temperature control work finished */
	wake_lock_init(&chip->bat_temp_decrease_lock, WAKE_LOCK_SUSPEND,
			"battery_temp_ctrl_decrease_lock");

	/* For Booting with Cable Inserted */
	queue_delayed_work(chip->bat_otp_internal_wq, &chip->monitor_work, round_jiffies_relative(msecs_to_jiffies(pdata->update_time)));

	probe_done_check = 1;

	pr_info("%s end\n", __func__);

	return 0;
}

static const struct dev_pm_ops batt_temp_ops = {
	.suspend = batt_temp_ctrl_suspend,
	.resume = batt_temp_ctrl_resume,
};

static struct of_device_id batt_temp_ctrl_of_match[] = {
	{ .compatible = "battery_temp_ctrl" },
	{},
};
MODULE_DEVICE_TABLE(of, batt_temp_ctrl_of_match);

static struct platform_driver this_driver = {
	.probe  = batt_temp_ctrl_probe,
	.driver = {
		.name  = "batt_temp_ctrl",
		.owner = THIS_MODULE,
		.pm = &batt_temp_ops,
		.of_match_table = batt_temp_ctrl_of_match,
	},
};

int __init batt_temp_ctrl_init(void)
{
	pr_info("batt_temp_ctrl_init \n");
	return platform_driver_register(&this_driver);
}

late_initcall(batt_temp_ctrl_init);

MODULE_DESCRIPTION("Battery Temperature Control Driver");
MODULE_AUTHOR("ChoongRyeol Lee <choongryeol.lee@lge.com>");
MODULE_LICENSE("GPL");
