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
#include <linux/wakelock.h>

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

#define MAX17050_I2C_RETRY_TIMES (5)
#define MAX17050_TEMPERATURE_RE_READ_MS (1400)

#define MAX17050_CHARGING_COMPLETE_CANCEL_SOC	(96)

#define MAX17050_BATTERY_CRITICAL_LOW_MV	(3450)
#define MAX17050_BATTERY_DEAD_MV		(3400)

#define MAX17050_NORMAL_MAX_SOC_DEC		(2)
#define MAX17050_CRITICAL_LOW_FORCE_SOC_DROP	(6)

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
	int shutdown_complete;
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
	struct mutex soc_mutex;
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

static bool max17050_soc_adjust(struct max17050_chip *chip,
			unsigned long time_since_last_update)
{
	int soc_decrease;
	int soc, vcell_mv;

	if (!chip->first_update_done) {
		if (chip->soc_raw >= MAX17050_BATTERY_FULL) {
			chip->soc = MAX17050_BATTERY_FULL - 1;
			chip->lasttime_soc = MAX17050_BATTERY_FULL - 1;
		} else {
			chip->soc = chip->soc_raw;
			chip->lasttime_soc = chip->soc_raw;
		}
		chip->first_update_done = true;
	}

	if (chip->charge_complete == 1)
		soc = MAX17050_BATTERY_FULL;
	else if (chip->soc_raw >= MAX17050_BATTERY_FULL
		&& chip->lasttime_soc < MAX17050_BATTERY_FULL
		&& chip->charge_complete == 0)
		soc = MAX17050_BATTERY_FULL - 1;
	else
		soc = chip->soc_raw;

	if (soc > MAX17050_BATTERY_FULL)
		soc = MAX17050_BATTERY_FULL;
	else if (soc < 0)
		soc = 0;

	vcell_mv = chip->vcell / 1000;
	if (chip->charger_status == BATTERY_DISCHARGING ||
		chip->charger_status == BATTERY_UNKNOWN) {
		if (vcell_mv >= MAX17050_BATTERY_CRITICAL_LOW_MV) {
			soc_decrease = chip->lasttime_soc - soc;
			if (time_since_last_update >=
					MAX17050_SOC_UPDATE_LONG_MS) {
				if (soc_decrease < 0)
					soc = chip->lasttime_soc;
				goto done;
			} else if (time_since_last_update <
					MAX17050_SOC_UPDATE_MS) {
				goto no_update;
			}

			if (soc_decrease < 0)
				soc_decrease = 0;
			else if (soc_decrease > MAX17050_NORMAL_MAX_SOC_DEC)
				soc_decrease = MAX17050_NORMAL_MAX_SOC_DEC;

			soc = chip->lasttime_soc - soc_decrease;
		} else if (vcell_mv < MAX17050_BATTERY_DEAD_MV) {
			dev_info(&chip->client->dev,
				"Battery voltage < %dmV, focibly update level to 0\n",
				MAX17050_BATTERY_DEAD_MV);
			soc = 0;
		} else {
			dev_info(&chip->client->dev,
				"Battery voltage < %dmV, focibly decrease level with %d\n",
				MAX17050_BATTERY_CRITICAL_LOW_MV,
				MAX17050_CRITICAL_LOW_FORCE_SOC_DROP);
			soc_decrease = MAX17050_CRITICAL_LOW_FORCE_SOC_DROP;
			if (chip->lasttime_soc <= soc_decrease)
				soc = 0;
			else
				soc = chip->lasttime_soc - soc_decrease;
		}
	} else if (soc > chip->lasttime_soc)
		soc = chip->lasttime_soc + 1;
done:
	chip->soc = soc;
	return true;

no_update:
	return false;
}

static void max17050_battery_status_check(struct max17050_chip *chip)
{
	if (chip->charger_status == BATTERY_DISCHARGING ||
		chip->charger_status == BATTERY_UNKNOWN)
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	else {
		if (chip->soc >= MAX17050_BATTERY_FULL
			&& chip->charge_complete == 1)
			chip->status = POWER_SUPPLY_STATUS_FULL;
		else
			chip->status = POWER_SUPPLY_STATUS_CHARGING;
	}
}

static void max17050_battery_health_check(struct max17050_chip *chip)
{
	if (chip->soc >= MAX17050_BATTERY_FULL) {
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (chip->soc < MAX17050_BATTERY_LOW) {
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		chip->health = POWER_SUPPLY_HEALTH_DEAD;
	} else {
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
	}
}

static void max17050_get_soc(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);
	int soc, soc_adjust;

	soc = max17050_read_word(client, MAX17050_FG_RepSOC);
	if (soc < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, soc);
	else {
		soc_adjust = (uint16_t)soc >> 8;
		if (!!(soc & 0xFF))
			soc_adjust += 1;
		chip->soc_raw = soc_adjust;
	}
}

static void max17050_work(struct work_struct *work)
{
	struct max17050_chip *chip;
	unsigned long cur_jiffies;
	bool do_battery_update = false;
	bool soc_updated;
	int vcell, batt_curr, soc_raw;

	chip = container_of(work, struct max17050_chip, work.work);

	wake_lock(&chip->update_wake_lock);

	mutex_lock(&chip->soc_mutex);
	max17050_get_vcell(chip->client);
	max17050_get_current(chip->client);
	max17050_get_temperature(chip->client);
	max17050_get_soc(chip->client);

	vcell = chip->vcell;
	batt_curr = chip->batt_curr;
	soc_raw = chip->soc_raw;

	cur_jiffies = jiffies;
	chip->total_time_since_last_work_ms = 0;
	chip->total_time_since_last_soc_update_ms +=
		((cur_jiffies - chip->last_gauge_check_jiffies) *
							MSEC_PER_SEC / HZ);
	chip->last_gauge_check_jiffies = cur_jiffies;

	soc_updated = max17050_soc_adjust(chip,
				chip->total_time_since_last_soc_update_ms);
	if (soc_updated)
		chip->total_time_since_last_soc_update_ms = 0;

	dev_dbg(&chip->client->dev,
		"level=%d,level_raw=%d,vol=%d,temp=%d,current=%d,status=%d\n",
		chip->soc,
		chip->soc_raw,
		chip->vcell,
		chip->batt_temp,
		chip->batt_curr,
		chip->status);

	if (chip->soc != chip->lasttime_soc) {
		chip->lasttime_soc = chip->soc;
		if (chip->soc >= MAX17050_BATTERY_FULL || chip->soc <= 0)
			do_battery_update = true;
	}

	max17050_battery_status_check(chip);
	max17050_battery_health_check(chip);

	if (chip->status != chip->lasttime_status) {
		chip->lasttime_status = chip->status;
		do_battery_update = true;
	}

	if (do_battery_update)
		power_supply_changed(&chip->battery);

	if (chip->status == POWER_SUPPLY_STATUS_DISCHARGING)
		schedule_delayed_work(&chip->work, MAX17050_DELAY);
	else
		schedule_delayed_work(&chip->work, MAX17050_DELAY_FAST);
	mutex_unlock(&chip->soc_mutex);

	battery_gauge_record_voltage_value(chip->bg_dev, vcell);
	battery_gauge_record_current_value(chip->bg_dev, batt_curr);
	battery_gauge_record_capacity_value(chip->bg_dev, soc_raw);
	battery_gauge_update_record_to_charger(chip->bg_dev);

	wake_unlock(&chip->update_wake_lock);
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

	mutex_lock(&chip->soc_mutex);
	if (status == BATTERY_CHARGING)
		chip->charger_status = BATTERY_CHARGING;
	else if (status == BATTERY_UNKNOWN)
		chip->charger_status = BATTERY_UNKNOWN;
	else if (status == BATTERY_CHARGING_DONE)
		chip->charger_status = BATTERY_CHARGING_DONE;
	else
		chip->charger_status = BATTERY_DISCHARGING;

	if (status == BATTERY_CHARGING_DONE)
		chip->charge_complete = 1;
	else if (status != BATTERY_CHARGING ||
		chip->soc_raw < MAX17050_CHARGING_COMPLETE_CANCEL_SOC)
		chip->charge_complete = 0;

	max17050_battery_status_check(chip);

	if (chip->status != chip->lasttime_status) {
		chip->lasttime_status = chip->status;
		power_supply_changed(&chip->battery);
	}
	mutex_unlock(&chip->soc_mutex);

	return 0;
}

static int max17050_get_battery_temp(void)
{
	if (max17050_data)
		return max17050_data->batt_temp;

	return 0;
}

static struct battery_gauge_ops max17050_bg_ops = {
	.update_battery_status = max17050_update_battery_status,
	.get_battery_temp = max17050_get_battery_temp,
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
	mutex_init(&chip->soc_mutex);
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
	chip->last_gauge_check_jiffies	= jiffies;
	chip->total_time_since_last_work_ms		= 0;
	chip->total_time_since_last_soc_update_ms	= 0;

	wake_lock_init(&chip->update_wake_lock, WAKE_LOCK_SUSPEND,
			"max17050_update");

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

	chip->first_update_done = false;
	chip->present = 1;
	INIT_DEFERRABLE_WORK(&chip->work, max17050_work);
	schedule_delayed_work(&chip->work, 0);

	return 0;
bg_err:
	power_supply_unregister(&chip->battery);
error:
	mutex_destroy(&chip->mutex);
	mutex_destroy(&chip->soc_mutex);

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
	mutex_destroy(&chip->soc_mutex);

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
static int max17050_prepare(struct device *dev)
{
	struct timespec xtime;
	unsigned long check_time = 0;
	unsigned long cur_jiffies;
	unsigned long time_since_last_system_ms;
	struct max17050_chip *chip = dev_get_drvdata(dev);

	xtime = CURRENT_TIME;
	chip->gauge_suspend_ms = xtime.tv_sec * MSEC_PER_SEC +
					xtime.tv_nsec / NSEC_PER_MSEC;
	cur_jiffies = jiffies;
	time_since_last_system_ms =
		((cur_jiffies - chip->last_gauge_check_jiffies) *
					MSEC_PER_SEC / HZ);
	chip->total_time_since_last_work_ms += time_since_last_system_ms;
	chip->total_time_since_last_soc_update_ms += time_since_last_system_ms;
	chip->last_gauge_check_jiffies = cur_jiffies;

	if (chip->status == POWER_SUPPLY_STATUS_CHARGING)
		check_time = MAX17050_CHARGING_DELAY_S * MSEC_PER_SEC;
	else
		check_time = MAX17050_DELAY_S * MSEC_PER_SEC;

	/* check if update is over time or in 1 second near future */
	if (check_time <= MAX17050_CHECK_TOLERANCE_MS ||
			chip->total_time_since_last_work_ms >=
			check_time - MAX17050_CHECK_TOLERANCE_MS) {
		dev_info(dev,
			"%s: passing time:%lu ms, max17050_update immediately.",
			__func__, chip->total_time_since_last_work_ms);
		cancel_delayed_work(&chip->work);
		schedule_delayed_work(&chip->work, 0);
		return -EBUSY;
	}

	dev_info(dev, "%s: passing time:%lu ms.",
			__func__, chip->total_time_since_last_work_ms);

	return 0;
}

static void max17050_complete(struct device *dev)
{
	unsigned long resume_ms;
	unsigned long sr_time_period_ms;
	unsigned long check_time;
	struct timespec xtime;
	struct max17050_chip *chip = dev_get_drvdata(dev);

	xtime = CURRENT_TIME;
	resume_ms = xtime.tv_sec * MSEC_PER_SEC +
					xtime.tv_nsec / NSEC_PER_MSEC;
	sr_time_period_ms = resume_ms - chip->gauge_suspend_ms;
	chip->total_time_since_last_work_ms += sr_time_period_ms;
	chip->total_time_since_last_soc_update_ms += sr_time_period_ms;
	chip->last_gauge_check_jiffies = jiffies;
	dev_info(dev, "%s: sr_time_period=%lu ms; total passing time=%lu ms.",
			__func__, sr_time_period_ms,
			chip->total_time_since_last_work_ms);

	if (chip->status == POWER_SUPPLY_STATUS_CHARGING)
		check_time = MAX17050_CHARGING_DELAY_S * MSEC_PER_SEC;
	else
		check_time = MAX17050_DELAY_S * MSEC_PER_SEC;

	/*
	 * When kernel resumes, gauge driver should check last work time
	 * to decide if do gauge work or just ignore.
	 */
	if (check_time <= MAX17050_CHECK_TOLERANCE_MS ||
			chip->total_time_since_last_work_ms >=
			check_time - MAX17050_CHECK_TOLERANCE_MS) {
		dev_info(dev, "trigger max17050_work while resume.");
		cancel_delayed_work_sync(&chip->work);
		schedule_delayed_work(&chip->work, 0);
	}
}

static int max17050_suspend(struct device *dev)
{
	struct max17050_chip *chip = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&chip->work);

	return 0;
}

static int max17050_resume(struct device *dev)
{
	unsigned long schedule_time;
	struct max17050_chip *chip = dev_get_drvdata(dev);

	if (chip->status == POWER_SUPPLY_STATUS_CHARGING)
		schedule_time = MAX17050_DELAY_FAST;
	else
		schedule_time = MAX17050_DELAY;

	schedule_delayed_work(&chip->work, schedule_time);

	return 0;
}
#endif /* CONFIG_PM */

static const struct dev_pm_ops max17050_pm_ops = {
	.prepare = max17050_prepare,
	.complete = max17050_complete,
	.suspend = max17050_suspend,
	.resume = max17050_resume,
};

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
