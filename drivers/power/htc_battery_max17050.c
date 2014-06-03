/*
 * htc_battery_max17050.c
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
#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/htc_battery_max17050.h>
#include <linux/power/battery-charger-gauge-comm.h>
#include <linux/pm.h>
#include <linux/jiffies.h>
#include <linux/wakelock.h>
#include <linux/of.h>
#ifdef CONFIG_BATTERY_SYSTEM_VOLTAGE_MONITOR
#include <linux/battery_system_voltage_monitor.h>
#endif

#define MAX17050_DELAY_S		(60)
#define MAX17050_CHARGING_DELAY_S	(30)
#define MAX17050_DELAY			(MAX17050_DELAY_S*HZ)
#define MAX17050_DELAY_FAST		(MAX17050_CHARGING_DELAY_S*HZ)
#define MAX17050_BATTERY_FULL		(100)
#define MAX17050_BATTERY_LOW		(15)
#define MAX17050_CHECK_TOLERANCE_MS	(MSEC_PER_SEC)
#define MAX17050_SOC_UPDATE_MS	\
	(MAX17050_DELAY_S*MSEC_PER_SEC - MAX17050_CHECK_TOLERANCE_MS)
#define MAX17050_SOC_UPDATE_LONG_MS	\
	(3600*MSEC_PER_SEC - MAX17050_CHECK_TOLERANCE_MS)

#define MAX17050_CHARGING_COMPLETE_CANCEL_SOC	(96)

#define MAX17050_BATTERY_CRITICAL_LOW_MV	(3450)
#define MAX17050_BATTERY_DEAD_MV		(3400)

#define MAX17050_NORMAL_MAX_SOC_DEC		(2)
#define MAX17050_CRITICAL_LOW_FORCE_SOC_DROP	(6)

#define MAX17050_VCELL_INIT_VALUE	(3800000)
#define MAX17050_TEMPERATURE_INIT_VALUE	(245)
#define MAX17050_CURRENT_INIT_VALUE	(500000)
#define	MAX17050_SOC_INIT_VALUE		(66)
#define	MAX17050_OCV_INIT_VALUE		MAX17050_VCELL_INIT_VALUE

struct htc_battery_max17050_data {
	struct delayed_work		work;
	struct power_supply		battery;
	struct device			*dev;
	struct battery_gauge_dev	*bg_dev;
	struct htc_battery_max17050_ops *ops;

	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	/* battery capacity unadjusted */
	int soc_raw;
	/* State Of Charge */
	int status;
	/* battery health */
	int health;
	/* battery capacity */
	int capacity_level;
	/* battery temperature */
	int batt_temp;
	/* battery current */
	int batt_curr;

	int lasttime_soc;
	int lasttime_status;
	int charger_status;
	int charge_complete;
	int present;
	unsigned long last_gauge_check_jiffies;
	unsigned long gauge_suspend_ms;
	unsigned long total_time_since_last_work_ms;
	unsigned long total_time_since_last_soc_update_ms;
	bool first_update_done;
	struct wake_lock update_wake_lock;
	struct mutex mutex;
#ifdef CONFIG_BATTERY_SYSTEM_VOLTAGE_MONITOR
	unsigned int monitor_voltage;
#endif
};
static struct htc_battery_max17050_data *htc_battery_data;

static int htc_battery_max17050_get_vcell(
			struct htc_battery_max17050_data *data, int *vcell)
{
	int ret = 0;

	if (!data->ops || !data->ops->get_vcell) {
		dev_warn(data->dev, "can not read battery vcell");
		return -ENODEV;
	}

	ret = data->ops->get_vcell(vcell);

	return ret;
}

static int htc_battery_max17050_get_current(
			struct htc_battery_max17050_data *data, int *batt_curr)
{
	int ret = 0;

	if (!data->ops || !data->ops->get_battery_current) {
		dev_warn(data->dev, "can not read battery current");
		return -ENODEV;
	}

	ret = data->ops->get_battery_current(batt_curr);

	return ret;
}

static int htc_battery_max17050_get_temperature(
			struct htc_battery_max17050_data *data, int *batt_temp)
{
	int ret = 0;

	if (!data->ops || !data->ops->get_temperature) {
		dev_warn(data->dev, "can not read battery temperature");
		return -ENODEV;
	}

	ret = data->ops->get_temperature(batt_temp);

	return ret;
}

static int htc_battery_max17050_get_soc(
			struct htc_battery_max17050_data *data, int *soc)
{
	int ret = 0;

	if (!data->ops || !data->ops->get_soc) {
		dev_warn(data->dev, "can not read battery soc");
		return -ENODEV;
	}

	ret = data->ops->get_soc(soc);

	return ret;
}

static int htc_battery_max17050_get_ocv(
			struct htc_battery_max17050_data *data, int *ocv)
{
	int ret = 0;

	if (!data->ops || !data->ops->get_ocv) {
		dev_warn(data->dev, "can not read battery ocv");
		return -ENODEV;
	}

	ret = data->ops->get_ocv(ocv);

	return ret;
}

static void htc_battery_max17050_status_check(
			struct htc_battery_max17050_data *data)
{
	if (data->charger_status == BATTERY_DISCHARGING ||
		data->charger_status == BATTERY_UNKNOWN)
		data->status = POWER_SUPPLY_STATUS_DISCHARGING;
	else {
		if (data->soc >= MAX17050_BATTERY_FULL
			&& data->charge_complete == 1)
			data->status = POWER_SUPPLY_STATUS_FULL;
		else
			data->status = POWER_SUPPLY_STATUS_CHARGING;
	}
}

static void htc_battery_max17050_health_check(
				struct htc_battery_max17050_data *data)
{
	if (data->soc >= MAX17050_BATTERY_FULL) {
		data->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		data->health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (data->soc < MAX17050_BATTERY_LOW) {
		data->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		data->health = POWER_SUPPLY_HEALTH_DEAD;
	} else {
		data->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
		data->health = POWER_SUPPLY_HEALTH_GOOD;
	}
}

static bool htc_battery_max17050_soc_adjust(
			struct htc_battery_max17050_data *data,
			unsigned long time_since_last_update)
{
	int soc_decrease;
	int soc, vcell_mv;

	if (!data->first_update_done) {
		if (data->soc_raw >= MAX17050_BATTERY_FULL) {
			data->soc = MAX17050_BATTERY_FULL - 1;
			data->lasttime_soc = MAX17050_BATTERY_FULL - 1;
		} else {
			data->soc = data->soc_raw;
			data->lasttime_soc = data->soc_raw;
		}
		data->first_update_done = true;
	}

	if (data->charge_complete == 1)
		soc = MAX17050_BATTERY_FULL;
	else if (data->soc_raw >= MAX17050_BATTERY_FULL
		&& data->lasttime_soc < MAX17050_BATTERY_FULL
		&& data->charge_complete == 0)
		soc = MAX17050_BATTERY_FULL - 1;
	else
		soc = data->soc_raw;

	if (soc > MAX17050_BATTERY_FULL)
		soc = MAX17050_BATTERY_FULL;
	else if (soc < 0)
		soc = 0;

	vcell_mv = data->vcell / 1000;
	if (data->charger_status == BATTERY_DISCHARGING ||
		data->charger_status == BATTERY_UNKNOWN) {
		if (vcell_mv >= MAX17050_BATTERY_CRITICAL_LOW_MV) {
			soc_decrease = data->lasttime_soc - soc;
			if (time_since_last_update >=
					MAX17050_SOC_UPDATE_LONG_MS) {
				if (soc_decrease < 0)
					soc = data->lasttime_soc;
				goto done;
			} else if (time_since_last_update <
					MAX17050_SOC_UPDATE_MS) {
				goto no_update;
			}

			if (soc_decrease < 0)
				soc_decrease = 0;
			else if (soc_decrease > MAX17050_NORMAL_MAX_SOC_DEC)
				soc_decrease = MAX17050_NORMAL_MAX_SOC_DEC;

			soc = data->lasttime_soc - soc_decrease;
		} else if (vcell_mv < MAX17050_BATTERY_DEAD_MV) {
			dev_info(data->dev,
				"Battery voltage < %dmV, focibly update level to 0\n",
				MAX17050_BATTERY_DEAD_MV);
			soc = 0;
		} else {
			dev_info(data->dev,
				"Battery voltage < %dmV, focibly decrease level with %d\n",
				MAX17050_BATTERY_CRITICAL_LOW_MV,
				MAX17050_CRITICAL_LOW_FORCE_SOC_DROP);
			soc_decrease = MAX17050_CRITICAL_LOW_FORCE_SOC_DROP;
			if (data->lasttime_soc <= soc_decrease)
				soc = 0;
			else
				soc = data->lasttime_soc - soc_decrease;
		}
	} else if (soc > data->lasttime_soc)
		soc = data->lasttime_soc + 1;
done:
	data->soc = soc;
	return true;

no_update:
	return false;
}

#ifdef CONFIG_BATTERY_SYSTEM_VOLTAGE_MONITOR
static void htc_battery_voltage_monitor_check(
			struct htc_battery_max17050_data *data,
			bool enable)
{
	unsigned int monitor_voltage = 0;
	int vcell_mv;

	if (enable) {
		vcell_mv = data->vcell / 1000;
		if (vcell_mv > MAX17050_BATTERY_CRITICAL_LOW_MV)
			monitor_voltage = MAX17050_BATTERY_CRITICAL_LOW_MV;
		else if (vcell_mv > MAX17050_BATTERY_DEAD_MV)
			monitor_voltage = MAX17050_BATTERY_DEAD_MV;
	}

	if (monitor_voltage != data->monitor_voltage) {
		if (monitor_voltage)
			battery_voltage_monitor_on_once(monitor_voltage);
		else
			battery_voltage_monitor_off();
		data->monitor_voltage = monitor_voltage;
	}
}
#endif

static void htc_battery_max17050_work(struct work_struct *work)
{
	struct htc_battery_max17050_data *data;
	unsigned long cur_jiffies;
	bool do_battery_update = false;
	bool soc_updated;
	int ret;
	int vcell, batt_curr, batt_temp, soc_raw;

	data = container_of(work, struct htc_battery_max17050_data, work.work);

	wake_lock(&data->update_wake_lock);

	mutex_lock(&data->mutex);
	if (!data->ops) {
		dev_warn(data->dev, "Max17050 is not ready for battery update");
#ifdef CONFIG_BATTERY_SYSTEM_VOLTAGE_MONITOR
		htc_battery_voltage_monitor_check(data, false);
#endif
		mutex_unlock(&data->mutex);
		wake_unlock(&data->update_wake_lock);
		return;
	}

	ret = htc_battery_max17050_get_vcell(data, &vcell);
	if (!ret)
		data->vcell = vcell;
	else
		vcell = data->vcell;
	ret = htc_battery_max17050_get_current(data, &batt_curr);
	if (!ret)
		data->batt_curr = batt_curr;
	else
		batt_curr = data->batt_curr;
	ret = htc_battery_max17050_get_temperature(data, &batt_temp);
	if (!ret)
		data->batt_temp = batt_temp;
	else
		batt_temp = data->batt_temp;
	ret = htc_battery_max17050_get_soc(data, &soc_raw);
	if (!ret)
		data->soc_raw = soc_raw;
	else
		soc_raw = data->soc_raw;

	cur_jiffies = jiffies;
	data->total_time_since_last_work_ms = 0;
	data->total_time_since_last_soc_update_ms +=
		((cur_jiffies - data->last_gauge_check_jiffies) *
							MSEC_PER_SEC / HZ);
	data->last_gauge_check_jiffies = cur_jiffies;

	soc_updated = htc_battery_max17050_soc_adjust(data,
				data->total_time_since_last_soc_update_ms);
	if (soc_updated)
		data->total_time_since_last_soc_update_ms = 0;

	dev_dbg(data->dev,
		"level=%d,level_raw=%d,vol=%d,temp=%d,current=%d,status=%d\n",
		data->soc,
		data->soc_raw,
		data->vcell,
		data->batt_temp,
		data->batt_curr,
		data->status);

	if (data->soc != data->lasttime_soc) {
		data->lasttime_soc = data->soc;
		if (data->soc >= MAX17050_BATTERY_FULL || data->soc <= 0)
			do_battery_update = true;
	}

	htc_battery_max17050_status_check(data);
	htc_battery_max17050_health_check(data);

	if (data->status != data->lasttime_status) {
		data->lasttime_status = data->status;
		do_battery_update = true;
	}

	if (do_battery_update)
		power_supply_changed(&data->battery);

#ifdef CONFIG_BATTERY_SYSTEM_VOLTAGE_MONITOR
	if (data->charger_status != BATTERY_CHARGING &&
			data->charger_status != BATTERY_CHARGING_DONE &&
			data->soc > 0)
		htc_battery_voltage_monitor_check(data, true);
	else
		htc_battery_voltage_monitor_check(data, false);
#endif

	if (data->status == POWER_SUPPLY_STATUS_DISCHARGING)
		schedule_delayed_work(&data->work, MAX17050_DELAY);
	else
		schedule_delayed_work(&data->work, MAX17050_DELAY_FAST);
	mutex_unlock(&data->mutex);

	battery_gauge_record_voltage_value(data->bg_dev, vcell);
	battery_gauge_record_current_value(data->bg_dev, batt_curr);
	battery_gauge_record_capacity_value(data->bg_dev, soc_raw);
	battery_gauge_update_record_to_charger(data->bg_dev);

	wake_unlock(&data->update_wake_lock);
}

static enum power_supply_property htc_battery_max17050_prop[] = {
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int htc_battery_max17050_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct htc_battery_max17050_data *data = container_of(psy,
				struct htc_battery_max17050_data, battery);
	int ocv;

	switch (psp) {
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = data->status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = data->vcell;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = data->batt_curr;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = data->soc;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = data->health;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = data->capacity_level;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		htc_battery_max17050_get_ocv(data, &ocv);
		val->intval = ocv;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = data->batt_temp;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->present;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int htc_battery_max17050_update_battery_status(
			struct battery_gauge_dev *bg_dev,
			enum battery_charger_status status)
{
	struct htc_battery_max17050_data *data =
				battery_gauge_get_drvdata(bg_dev);

	mutex_lock(&data->mutex);
	if (status == BATTERY_CHARGING)
		data->charger_status = BATTERY_CHARGING;
	else if (status == BATTERY_UNKNOWN)
		data->charger_status = BATTERY_UNKNOWN;
	else if (status == BATTERY_CHARGING_DONE)
		data->charger_status = BATTERY_CHARGING_DONE;
	else
		data->charger_status = BATTERY_DISCHARGING;

	if (status == BATTERY_CHARGING_DONE)
		data->charge_complete = 1;
	else if (status != BATTERY_CHARGING ||
		data->soc_raw < MAX17050_CHARGING_COMPLETE_CANCEL_SOC)
		data->charge_complete = 0;

	htc_battery_max17050_status_check(data);

	if (data->status != data->lasttime_status) {
		data->lasttime_status = data->status;
		power_supply_changed(&data->battery);
	}

#ifdef CONFIG_BATTERY_SYSTEM_VOLTAGE_MONITOR
	if (data->charger_status != BATTERY_CHARGING &&
			data->charger_status != BATTERY_CHARGING_DONE &&
			data->soc > 0)
		htc_battery_voltage_monitor_check(data, true);
	else
		htc_battery_voltage_monitor_check(data, false);
#endif
	mutex_unlock(&data->mutex);

	return 0;
}

static int htc_battery_max17050_get_battery_temp(void)
{
	if (htc_battery_data)
		return htc_battery_data->batt_temp;

	return 0;
}

int htc_battery_max17050_gauge_register(struct htc_battery_max17050_ops *ops)
{

	if (!htc_battery_data)
		return -ENODEV;

	if (!ops || !ops->get_vcell || !ops->get_battery_current ||
		!ops->get_temperature || !ops->get_soc) {
		dev_err(htc_battery_data->dev,
				"Gauge operation register fail!");
		return -EINVAL;
	}

	mutex_lock(&htc_battery_data->mutex);
	htc_battery_data->ops = ops;
	mutex_unlock(&htc_battery_data->mutex);
	schedule_delayed_work(&htc_battery_data->work, 0);

	return 0;
}
EXPORT_SYMBOL_GPL(htc_battery_max17050_gauge_register);

int htc_battery_max17050_gauge_unregister(void)
{
	if (!htc_battery_data)
		return -ENODEV;

	cancel_delayed_work_sync(&htc_battery_data->work);
	mutex_lock(&htc_battery_data->mutex);
	htc_battery_data->ops = NULL;
	mutex_unlock(&htc_battery_data->mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(htc_battery_max17050_gauge_unregister);

#ifdef CONFIG_BATTERY_SYSTEM_VOLTAGE_MONITOR
static int htc_battery_max17050_vbat_monitor_notification(unsigned int voltage)
{
	if (!htc_battery_data)
		return -ENODEV;

	dev_dbg(htc_battery_data->dev,
			"voltage alarm trigger, volt=%u!!!\n", voltage);

	mutex_lock(&htc_battery_data->mutex);
	htc_battery_data->monitor_voltage = 0;
	cancel_delayed_work_sync(&htc_battery_data->work);
	schedule_delayed_work(&htc_battery_data->work, 0);
	mutex_unlock(&htc_battery_data->mutex);

	return 0;
}
#endif

static struct battery_gauge_ops htc_battery_max17050_bg_ops = {
	.update_battery_status = htc_battery_max17050_update_battery_status,
	.get_battery_temp = htc_battery_max17050_get_battery_temp,
};

static struct battery_gauge_info htc_battery_max17050_bgi = {
	.cell_id = 0,
	.bg_ops = &htc_battery_max17050_bg_ops,
};

static int htc_battery_max17050_probe(struct platform_device *pdev)
{
	struct htc_battery_max17050_data *data;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	mutex_init(&data->mutex);
	dev_set_drvdata(&pdev->dev, data);

	data->battery.name		= "battery";
	data->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	data->battery.get_property	= htc_battery_max17050_get_property;
	data->battery.properties	= htc_battery_max17050_prop;
	data->battery.num_properties	= ARRAY_SIZE(htc_battery_max17050_prop);
	data->dev			= &pdev->dev;
	data->charge_complete		= 0;
	data->last_gauge_check_jiffies	= jiffies;
	data->total_time_since_last_work_ms		= 0;
	data->total_time_since_last_soc_update_ms	= 0;

	/* default value if gauge is not ready */
	data->vcell		= MAX17050_VCELL_INIT_VALUE;
	data->batt_temp		= MAX17050_TEMPERATURE_INIT_VALUE;
	data->batt_curr		= MAX17050_CURRENT_INIT_VALUE;
	data->soc_raw		= MAX17050_SOC_INIT_VALUE;
	data->status		= POWER_SUPPLY_STATUS_DISCHARGING;
	data->lasttime_status	= POWER_SUPPLY_STATUS_DISCHARGING;
	data->capacity_level	= POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	data->health		= POWER_SUPPLY_HEALTH_GOOD;

	wake_lock_init(&data->update_wake_lock, WAKE_LOCK_SUSPEND,
			"htc_battery_max17050_update");

	ret = power_supply_register(data->dev, &data->battery);
	if (ret) {
		dev_err(data->dev, "failed: power supply register\n");
		goto error;
	}

	data->bg_dev = battery_gauge_register(data->dev,
				&htc_battery_max17050_bgi, data);
	if (IS_ERR(data->bg_dev)) {
		ret = PTR_ERR(data->bg_dev);
		dev_err(data->dev, "battery gauge register failed: %d\n",
			ret);
		goto bg_err;
	}

#ifdef CONFIG_BATTERY_SYSTEM_VOLTAGE_MONITOR
	data->monitor_voltage = 0;
	battery_voltage_monitor_listener_register(
			htc_battery_max17050_vbat_monitor_notification);
#endif

	data->first_update_done = false;
	data->present = 1;
	INIT_DEFERRABLE_WORK(&data->work, htc_battery_max17050_work);

	htc_battery_data = data;

	return 0;
bg_err:
	power_supply_unregister(&data->battery);
error:
	mutex_destroy(&data->mutex);

	return ret;

}

static int htc_battery_max17050_remove(struct platform_device *pdev)
{
	struct htc_battery_max17050_data *data = dev_get_drvdata(&pdev->dev);

	data->present = 0;
	battery_gauge_unregister(data->bg_dev);
	power_supply_unregister(&data->battery);
	cancel_delayed_work_sync(&data->work);
	mutex_destroy(&data->mutex);

	return 0;
}

static void htc_battery_max17050_shutdown(struct platform_device *pdev)
{
	struct htc_battery_max17050_data *data = dev_get_drvdata(&pdev->dev);

	cancel_delayed_work_sync(&data->work);
}

#ifdef CONFIG_PM_SLEEP
static int htc_battery_max17050_prepare(struct device *dev)
{
	struct timespec xtime;
	unsigned long check_time = 0;
	unsigned long cur_jiffies;
	unsigned long time_since_last_system_ms;
	struct htc_battery_max17050_data *data = dev_get_drvdata(dev);

	xtime = CURRENT_TIME;
	data->gauge_suspend_ms = xtime.tv_sec * MSEC_PER_SEC +
					xtime.tv_nsec / NSEC_PER_MSEC;
	cur_jiffies = jiffies;
	time_since_last_system_ms =
		((cur_jiffies - data->last_gauge_check_jiffies) *
					MSEC_PER_SEC / HZ);
	data->total_time_since_last_work_ms += time_since_last_system_ms;
	data->total_time_since_last_soc_update_ms += time_since_last_system_ms;
	data->last_gauge_check_jiffies = cur_jiffies;

	if (data->status == POWER_SUPPLY_STATUS_CHARGING)
		check_time = MAX17050_CHARGING_DELAY_S * MSEC_PER_SEC;
	else
		check_time = MAX17050_DELAY_S * MSEC_PER_SEC;

	/* check if update is over time or in 1 second near future */
	if (check_time <= MAX17050_CHECK_TOLERANCE_MS ||
			data->total_time_since_last_work_ms >=
			check_time - MAX17050_CHECK_TOLERANCE_MS) {
		dev_info(dev,
			"%s: passing time:%lu ms, htc_battery_max17050_update immediately.",
			__func__, data->total_time_since_last_work_ms);
		cancel_delayed_work_sync(&data->work);
		schedule_delayed_work(&data->work, 0);
		return -EBUSY;
	}

	dev_info(dev, "%s: passing time:%lu ms.",
			__func__, data->total_time_since_last_work_ms);

	return 0;
}

static void htc_battery_max17050_complete(struct device *dev)
{
	unsigned long resume_ms;
	unsigned long sr_time_period_ms;
	unsigned long check_time;
	struct timespec xtime;
	struct htc_battery_max17050_data *data = dev_get_drvdata(dev);

	xtime = CURRENT_TIME;
	resume_ms = xtime.tv_sec * MSEC_PER_SEC +
					xtime.tv_nsec / NSEC_PER_MSEC;
	sr_time_period_ms = resume_ms - data->gauge_suspend_ms;
	data->total_time_since_last_work_ms += sr_time_period_ms;
	data->total_time_since_last_soc_update_ms += sr_time_period_ms;
	data->last_gauge_check_jiffies = jiffies;
	dev_info(dev, "%s: sr_time_period=%lu ms; total passing time=%lu ms.",
			__func__, sr_time_period_ms,
			data->total_time_since_last_work_ms);

	if (data->status == POWER_SUPPLY_STATUS_CHARGING)
		check_time = MAX17050_CHARGING_DELAY_S * MSEC_PER_SEC;
	else
		check_time = MAX17050_DELAY_S * MSEC_PER_SEC;

	/*
	 * When kernel resumes, gauge driver should check last work time
	 * to decide if do gauge work or just ignore.
	 */
	if (check_time <= MAX17050_CHECK_TOLERANCE_MS ||
			data->total_time_since_last_work_ms >=
			check_time - MAX17050_CHECK_TOLERANCE_MS) {
		dev_info(dev, "trigger htc_battery_max17050_work while resume.");
		cancel_delayed_work_sync(&data->work);
		schedule_delayed_work(&data->work, 0);
	}
}

static int htc_battery_max17050_suspend(struct device *dev)
{
	struct htc_battery_max17050_data *data = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&data->work);

	return 0;
}

static int htc_battery_max17050_resume(struct device *dev)
{
	unsigned long schedule_time;
	struct htc_battery_max17050_data *data = dev_get_drvdata(dev);

	if (data->status == POWER_SUPPLY_STATUS_CHARGING)
		schedule_time = MAX17050_DELAY_FAST;
	else
		schedule_time = MAX17050_DELAY;

	schedule_delayed_work(&data->work, schedule_time);

	return 0;
}

static const struct dev_pm_ops htc_battery_max17050_pm_ops = {
	.prepare = htc_battery_max17050_prepare,
	.complete = htc_battery_max17050_complete,
	.suspend = htc_battery_max17050_suspend,
	.resume = htc_battery_max17050_resume,
};
#endif /* CONFIG_PM */

static const struct of_device_id htc_battery_max17050_dt_match[] = {
	{ .compatible = "htc,max17050_battery" },
	{ },
};
MODULE_DEVICE_TABLE(of, htc_battery_max17050_dt_match);

static struct platform_driver htc_battery_max17050_driver = {
	.driver	= {
		.name	= "htc_battery_max17050",
		.of_match_table = of_match_ptr(htc_battery_max17050_dt_match),
		.owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm = &htc_battery_max17050_pm_ops,
#endif /* CONFIG_PM */
	},
	.probe		= htc_battery_max17050_probe,
	.remove		= htc_battery_max17050_remove,
	.shutdown	= htc_battery_max17050_shutdown,
};

static int __init htc_battery_max17050_init(void)
{
	return platform_driver_register(&htc_battery_max17050_driver);
}
fs_initcall_sync(htc_battery_max17050_init);

static void __exit htc_battery_max17050_exit(void)
{
	platform_driver_unregister(&htc_battery_max17050_driver);
}
module_exit(htc_battery_max17050_exit);

MODULE_DESCRIPTION("HTC battery max17050 module");
MODULE_LICENSE("GPL");
