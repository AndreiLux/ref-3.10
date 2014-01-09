/*
 * max17050_battery.c
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
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/max17050_battery.h>
#include <linux/power/battery-charger-gauge-comm.h>
#include <linux/pm.h>
#include <linux/jiffies.h>

#define MAX17050_DELAY		(60*HZ)
#define MAX17050_BATTERY_FULL	(100)
#define MAX17050_BATTERY_LOW	(15)

#define MAX17050_I2C_RETRY_TIMES (5)
#define MAX17050_TEMPERATURE_RE_READ_MS (1400)

/* Fuel Gauge Maxim MAX17050 Register Definition */
enum max17050_fg_register {
	MAX17050_FG_STATUS   	= 0x00,
	MAX17050_FG_VALRT_Th	= 0x01,
	MAX17050_FG_TALRT_Th	= 0x02,
	MAX17050_FG_SALRT_Th	= 0x03,
	MAX17050_FG_AtRate		= 0x04,
	MAX17050_FG_RepCap		= 0x05,
	MAX17050_FG_RepSOC		= 0x06,
	MAX17050_FG_Age			= 0x07,
	MAX17050_FG_TEMP		= 0x08,
	MAX17050_FG_VCELL		= 0x09,
	MAX17050_FG_Current		= 0x0A,
	MAX17050_FG_AvgCurrent	= 0x0B,
	MAX17050_FG_Qresidual	= 0x0C,
	MAX17050_FG_SOC			= 0x0D,
	MAX17050_FG_AvSOC		= 0x0E,
	MAX17050_FG_RemCap		= 0x0F,
	MAX17050_FG_FullCAP		= 0x10,
	MAX17050_FG_TTE			= 0x11,
	MAX17050_FG_QRtable00	= 0x12,
	MAX17050_FG_FullSOCthr	= 0x13,
	MAX17050_FG_RSLOW		= 0x14,
	MAX17050_FG_RFAST		= 0x15,
	MAX17050_FG_AvgTA		= 0x16,
	MAX17050_FG_Cycles		= 0x17,
	MAX17050_FG_DesignCap	= 0x18,
	MAX17050_FG_AvgVCELL	= 0x19,
	MAX17050_FG_MinMaxTemp	= 0x1A,
	MAX17050_FG_MinMaxVolt	= 0x1B,
	MAX17050_FG_MinMaxCurr	= 0x1C,
	MAX17050_FG_CONFIG		= 0x1D,
	MAX17050_FG_ICHGTerm	= 0x1E,
	MAX17050_FG_AvCap		= 0x1F,
	MAX17050_FG_ManName		= 0x20,
	MAX17050_FG_DevName		= 0x21,
	MAX17050_FG_QRtable10	= 0x22,
	MAX17050_FG_FullCAPNom	= 0x23,
	MAX17050_FG_TempNom		= 0x24,
	MAX17050_FG_TempLim		= 0x25,
	MAX17050_FG_AvgTA0		= 0x26,
	MAX17050_FG_AIN			= 0x27,
	MAX17050_FG_LearnCFG	= 0x28,
	MAX17050_FG_SHFTCFG		= 0x29,
	MAX17050_FG_RelaxCFG	= 0x2A,
	MAX17050_FG_MiscCFG		= 0x2B,
	MAX17050_FG_TGAIN		= 0x2C,
	MAX17050_FG_TOFF		= 0x2D,
	MAX17050_FG_CGAIN		= 0x2E,
	MAX17050_FG_COFF		= 0x2F,

	MAX17050_FG_dV_acc		= 0x30,
	MAX17050_FG_I_acc		= 0x31,
	MAX17050_FG_QRtable20	= 0x32,
	MAX17050_FG_MaskSOC		= 0x33,
	MAX17050_FG_CHG_CNFG_10	= 0x34,
	MAX17050_FG_FullCAP0	= 0x35,
	MAX17050_FG_Iavg_empty	= 0x36,
	MAX17050_FG_FCTC		= 0x37,
	MAX17050_FG_RCOMP0		= 0x38,
	MAX17050_FG_TempCo		= 0x39,
	MAX17050_FG_V_empty		= 0x3A,
	MAX17050_FG_AvgCurrent0	= 0x3B,
	MAX17050_FG_TaskPeriod	= 0x3C,
	MAX17050_FG_FSTAT		= 0x3D,
	MAX17050_FG_TIMER       = 0x3E,
	MAX17050_FG_SHDNTIMER	= 0x3F,

	MAX17050_FG_AvgCurrentL	= 0x40,
	MAX17050_FG_AvgTAL		= 0x41,
	MAX17050_FG_QRtable30	= 0x42,
	MAX17050_FG_RepCapL		= 0x43,
	MAX17050_FG_AvgVCELL0	= 0x44,
	MAX17050_FG_dQacc		= 0x45,
	MAX17050_FG_dp_acc		= 0x46,
	MAX17050_FG_RlxSOC		= 0x47,
	MAX17050_FG_VFSOC0		= 0x48,
	MAX17050_FG_RemCapL		= 0x49,
	MAX17050_FG_VFRemCap	= 0x4A,
	MAX17050_FG_AvgVCELLL	= 0x4B,
	MAX17050_FG_QH0			= 0x4C,
	MAX17050_FG_QH			= 0x4D,
	MAX17050_FG_QL			= 0x4E,
	MAX17050_FG_RemCapL0	= 0x4F,
	MAX17050_FG_LOCK_I		= 0x62,
	MAX17050_FG_LOCK_II		= 0x63,
	MAX17050_FG_OCV 		= 0x80,
	MAX17050_FG_VFOCV 		= 0xFB,
	MAX17050_FG_VFSOC 		= 0xFF,
};

struct max17050_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply		battery;
	struct max17050_platform_data *pdata;
	struct battery_gauge_dev	*bg_dev;

	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
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
	int shutdown_complete;
	int charge_complete;
	int present;
	struct mutex mutex;
};
static struct max17050_chip *max17050_data;

#define MAX17050_T_GAIN_OFF_NUM (5)
struct max17050_t_gain_off_table {
	int t[MAX17050_T_GAIN_OFF_NUM];
	unsigned int tgain[MAX17050_T_GAIN_OFF_NUM];
	unsigned int toff[MAX17050_T_GAIN_OFF_NUM];
};

static struct max17050_t_gain_off_table t_gain_off_lut = {
	.t     = {   -200,   -160,      1,    401,    501},
	.tgain = { 0xDFB0, 0xDF90, 0xEAC0, 0xDD50, 0xDD30},
	.toff  = { 0x32A5, 0x3370, 0x21E2, 0x2A30, 0x2A5A},
};

static int max17050_write_word(struct i2c_client *client, int reg, u16 value)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);
	int retry;
	uint8_t buf[3];

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 3,
			.buf = buf,
		}
	};

	mutex_lock(&chip->mutex);
	if (chip && chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -ENODEV;
	}

	buf[0] = reg & 0xFF;
	buf[1] = value & 0xFF;
	buf[2] = (value >> 8) & 0xFF;

	for (retry = 0; retry < MAX17050_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(10);
	}

	if (retry == MAX17050_I2C_RETRY_TIMES) {
		dev_err(&client->dev, "%s(): Failed in writing register"
				" 0x%02x after retry %d times\n"
				, __func__, reg, MAX17050_I2C_RETRY_TIMES);
		mutex_unlock(&chip->mutex);
		return -EIO;
	}
	mutex_unlock(&chip->mutex);

	return 0;
}

static int max17050_read_word(struct i2c_client *client, int reg)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);
	int retry;
	uint8_t vals[2], buf[1];

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 2,
			.buf = vals,
		}
	};

	mutex_lock(&chip->mutex);
	if (chip && chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -ENODEV;
	}

	buf[0] = reg & 0xFF;

	for (retry = 0; retry < MAX17050_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(client->adapter, msg, 2) == 2)
			break;
		msleep(10);
	}

	if (retry == MAX17050_I2C_RETRY_TIMES) {
		dev_err(&client->dev, "%s(): Failed in reading register"
				" 0x%02x after retry %d times\n"
				, __func__, reg, MAX17050_I2C_RETRY_TIMES);
		mutex_unlock(&chip->mutex);
		return -EIO;
	}

	mutex_unlock(&chip->mutex);
	return (vals[1] << 8) | vals[0];
}

/* Return value in uV */
static int max17050_get_ocv(struct max17050_chip *chip)
{
	int reg;
	int ocv;

	reg = max17050_read_word(chip->client, MAX17050_FG_VFOCV);
	if (reg < 0)
		return reg;
	ocv = (reg >> 4) * 1250;
	return ocv;
}

static int max17050_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17050_chip *chip = container_of(psy,
				struct max17050_chip, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->vcell;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = chip->batt_curr;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->soc;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->health;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = chip->capacity_level;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = max17050_get_ocv(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = chip->batt_temp;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->present;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void max17050_get_vcell(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);
	int vcell;

	vcell = max17050_read_word(client, MAX17050_FG_VCELL);
	if (vcell < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, vcell);
	else
		chip->vcell = ((uint16_t)vcell >> 3) * 625;
}

static void max17050_get_current(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);
	int curr;

	/*
	 * TODO: Assumes current sense resistor is 10mohms.
	 */

	curr = max17050_read_word(client, MAX17050_FG_Current);
	if (curr < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, curr);
	else
		chip->batt_curr = ((int16_t) curr) * 5000 / 32;
}

static int __max17050_get_temperature(struct i2c_client *client, int *batt_temp)
{
	int temp;

	temp = max17050_read_word(client, MAX17050_FG_TEMP);
	if (temp < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, temp);
		return temp;
	}

	*batt_temp = ((int16_t) temp) * 10 / (1 << 8);

	return 0;
}

static void max17050_get_temperature(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);
	int temp, ret, i;
	uint16_t tgain, toff;

	ret  = __max17050_get_temperature(client, &temp);
	if (ret < 0)
		goto error;

	if (temp <= t_gain_off_lut.t[0]) {
		tgain = t_gain_off_lut.tgain[0];
		toff = t_gain_off_lut.toff[0];
	} else {
		tgain = t_gain_off_lut.tgain[MAX17050_T_GAIN_OFF_NUM - 1];
		toff = t_gain_off_lut.toff[MAX17050_T_GAIN_OFF_NUM - 1];
		/* adjust TGAIN and TOFF for battery temperature accuracy*/
		for (i = 0; i < MAX17050_T_GAIN_OFF_NUM - 1; i++) {
			if (temp >= t_gain_off_lut.t[i] &&
				temp < t_gain_off_lut.t[i + 1]) {
				tgain = t_gain_off_lut.tgain[i];
				toff = t_gain_off_lut.toff[i];
				break;
			}
		}
	}

	ret = max17050_write_word(client, MAX17050_FG_TGAIN, tgain);
	if (ret < 0)
		goto error;

	ret = max17050_write_word(client, MAX17050_FG_TOFF, toff);
	if (ret < 0)
		goto error;

	msleep(MAX17050_TEMPERATURE_RE_READ_MS);

	ret  = __max17050_get_temperature(client, &temp);
	if (ret < 0)
		goto error;

	chip->batt_temp = temp;
	return;
error:
	dev_err(&client->dev, "%s: temperature reading fail, err %d\n"
			, __func__, ret);
}

static void max17050_get_soc(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);
	int soc;

	soc = max17050_read_word(client, MAX17050_FG_RepSOC);
	if (soc < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, soc);
	else
		chip->soc = (uint16_t)soc >> 8;

	if (chip->soc >= MAX17050_BATTERY_FULL && chip->charge_complete != 1)
		chip->soc = MAX17050_BATTERY_FULL-1;

	if (chip->status == POWER_SUPPLY_STATUS_FULL && chip->charge_complete) {
		chip->soc = MAX17050_BATTERY_FULL;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (chip->soc < MAX17050_BATTERY_LOW) {
		chip->status = chip->lasttime_status;
		chip->health = POWER_SUPPLY_HEALTH_DEAD;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	} else {
		chip->status = chip->lasttime_status;
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	}
}

static void max17050_work(struct work_struct *work)
{
	struct max17050_chip *chip;

	chip = container_of(work, struct max17050_chip, work.work);

	max17050_get_vcell(chip->client);
	max17050_get_current(chip->client);
	max17050_get_temperature(chip->client);
	max17050_get_soc(chip->client);

	dev_info(&chip->client->dev,
		"level=%d,vol=%d,temp=%d,current=%d,status=%d\n",
		chip->soc,
		chip->vcell,
		chip->batt_temp,
		chip->batt_curr,
		chip->status);

	if (chip->soc != chip->lasttime_soc ||
		chip->status != chip->lasttime_status) {
		chip->lasttime_soc = chip->soc;
		power_supply_changed(&chip->battery);
	}

	schedule_delayed_work(&chip->work, MAX17050_DELAY);
}

static enum power_supply_property max17050_battery_props[] = {
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

#ifdef CONFIG_OF
static struct max17050_platform_data *max17050_parse_dt(struct device *dev)
{
	/* TODO: load dt data if ready */
	return NULL;
}
#else
static struct max17050_platform_data *max17050_parse_dt(struct device *dev)
{
	return NULL;
}
#endif /* CONFIG_OF */

static int max17050_update_battery_status(struct battery_gauge_dev *bg_dev,
		enum battery_charger_status status)
{
	struct max17050_chip *chip = battery_gauge_get_drvdata(bg_dev);

	if (status == BATTERY_CHARGING)
		chip->status = POWER_SUPPLY_STATUS_CHARGING;
	else if (status == BATTERY_CHARGING_DONE) {
		chip->charge_complete = 1;
		chip->soc = MAX17050_BATTERY_FULL;
		chip->status = POWER_SUPPLY_STATUS_FULL;
		power_supply_changed(&chip->battery);
		return 0;
	} else {
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
		chip->charge_complete = 0;
	}
	chip->lasttime_status = chip->status;
	power_supply_changed(&chip->battery);
	return 0;
}

static struct battery_gauge_ops max17050_bg_ops = {
	.update_battery_status = max17050_update_battery_status,
};

static struct battery_gauge_info max17050_bgi = {
	.cell_id = 0,
	.bg_ops = &max17050_bg_ops,
};

static int max17050_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct max17050_chip *chip;
	int ret;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;

	if (client->dev.of_node) {
		chip->pdata = max17050_parse_dt(&client->dev);
		if (IS_ERR(chip->pdata))
			return PTR_ERR(chip->pdata);
	} else {
		chip->pdata = client->dev.platform_data;
		if (!chip->pdata)
			return -ENODATA;
	}

	max17050_data = chip;
	mutex_init(&chip->mutex);
	chip->shutdown_complete = 0;
	i2c_set_clientdata(client, chip);

	chip->battery.name		= "battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17050_get_property;
	chip->battery.properties	= max17050_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17050_battery_props);
	chip->status			= POWER_SUPPLY_STATUS_DISCHARGING;
	chip->lasttime_status		= POWER_SUPPLY_STATUS_DISCHARGING;
	chip->charge_complete		= 0;

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto error;
	}
	chip->bg_dev = battery_gauge_register(&client->dev, &max17050_bgi,
				chip);
	if (IS_ERR(chip->bg_dev)) {
		ret = PTR_ERR(chip->bg_dev);
		dev_err(&client->dev, "battery gauge register failed: %d\n",
			ret);
		goto bg_err;
	}

	chip->present = 1;
	INIT_DEFERRABLE_WORK(&chip->work, max17050_work);
	schedule_delayed_work(&chip->work, 0);

	return 0;
bg_err:
	power_supply_unregister(&chip->battery);
error:
	mutex_destroy(&chip->mutex);

	return ret;
}

static int max17050_remove(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);

	chip->present = 0;
	battery_gauge_unregister(chip->bg_dev);
	power_supply_unregister(&chip->battery);
	cancel_delayed_work_sync(&chip->work);
	mutex_destroy(&chip->mutex);

	return 0;
}

static void max17050_shutdown(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&chip->work);
	mutex_lock(&chip->mutex);
	chip->shutdown_complete = 1;
	mutex_unlock(&chip->mutex);

}

#ifdef CONFIG_PM_SLEEP
static int max17050_suspend(struct device *dev)
{
	struct max17050_chip *chip = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&chip->work);

	return 0;
}

static int max17050_resume(struct device *dev)
{
	struct max17050_chip *chip = dev_get_drvdata(dev);

	schedule_delayed_work(&chip->work, MAX17050_DELAY);

	return 0;
}
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(max17050_pm_ops, max17050_suspend, max17050_resume);

#ifdef CONFIG_OF
static const struct of_device_id max17050_dt_match[] = {
	{ .compatible = "maxim,max17050" },
	{ },
};
MODULE_DEVICE_TABLE(of, max17050_dt_match);
#endif

static const struct i2c_device_id max17050_id[] = {
	{ "max17050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17050_id);

static struct i2c_driver max17050_i2c_driver = {
	.driver	= {
		.name	= "max17050",
		.of_match_table = of_match_ptr(max17050_dt_match),
		.pm = &max17050_pm_ops,
	},
	.probe		= max17050_probe,
	.remove		= max17050_remove,
	.id_table	= max17050_id,
	.shutdown	= max17050_shutdown,
};

static int __init max17050_init(void)
{
	return i2c_add_driver(&max17050_i2c_driver);
}
fs_initcall_sync(max17050_init);

static void __exit max17050_exit(void)
{
	i2c_del_driver(&max17050_i2c_driver);
}
module_exit(max17050_exit);

MODULE_DESCRIPTION("MAX17050 Fuel Gauge");
MODULE_LICENSE("GPL");
