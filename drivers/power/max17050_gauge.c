/*
 * max17050_gauge.c
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
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/htc_battery_max17050.h>
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/iio/consumer.h>
#include <linux/iio/types.h>
#include <linux/iio/iio.h>

#define MAX17050_I2C_RETRY_TIMES (5)

/* Fuel Gauge Maxim MAX17050 Register Definition */
enum max17050_fg_register {
	MAX17050_FG_STATUS	= 0x00,
	MAX17050_FG_VALRT_Th	= 0x01,
	MAX17050_FG_TALRT_Th	= 0x02,
	MAX17050_FG_SALRT_Th	= 0x03,
	MAX17050_FG_AtRate	= 0x04,
	MAX17050_FG_RepCap	= 0x05,
	MAX17050_FG_RepSOC	= 0x06,
	MAX17050_FG_Age		= 0x07,
	MAX17050_FG_TEMP	= 0x08,
	MAX17050_FG_VCELL	= 0x09,
	MAX17050_FG_Current	= 0x0A,
	MAX17050_FG_AvgCurrent	= 0x0B,
	MAX17050_FG_Qresidual	= 0x0C,
	MAX17050_FG_SOC		= 0x0D,
	MAX17050_FG_AvSOC	= 0x0E,
	MAX17050_FG_RemCap	= 0x0F,
	MAX17050_FG_FullCAP	= 0x10,
	MAX17050_FG_TTE		= 0x11,
	MAX17050_FG_QRtable00	= 0x12,
	MAX17050_FG_FullSOCthr	= 0x13,
	MAX17050_FG_RSLOW	= 0x14,
	MAX17050_FG_RFAST	= 0x15,
	MAX17050_FG_AvgTA	= 0x16,
	MAX17050_FG_Cycles	= 0x17,
	MAX17050_FG_DesignCap	= 0x18,
	MAX17050_FG_AvgVCELL	= 0x19,
	MAX17050_FG_MinMaxTemp	= 0x1A,
	MAX17050_FG_MinMaxVolt	= 0x1B,
	MAX17050_FG_MinMaxCurr	= 0x1C,
	MAX17050_FG_CONFIG	= 0x1D,
	MAX17050_FG_ICHGTerm	= 0x1E,
	MAX17050_FG_AvCap	= 0x1F,
	MAX17050_FG_ManName	= 0x20,
	MAX17050_FG_DevName	= 0x21,
	MAX17050_FG_QRtable10	= 0x22,
	MAX17050_FG_FullCAPNom	= 0x23,
	MAX17050_FG_TempNom	= 0x24,
	MAX17050_FG_TempLim	= 0x25,
	MAX17050_FG_AvgTA0	= 0x26,
	MAX17050_FG_AIN		= 0x27,
	MAX17050_FG_LearnCFG	= 0x28,
	MAX17050_FG_SHFTCFG	= 0x29,
	MAX17050_FG_RelaxCFG	= 0x2A,
	MAX17050_FG_MiscCFG	= 0x2B,
	MAX17050_FG_TGAIN	= 0x2C,
	MAX17050_FG_TOFF	= 0x2D,
	MAX17050_FG_CGAIN	= 0x2E,
	MAX17050_FG_COFF	= 0x2F,

	MAX17050_FG_dV_acc	= 0x30,
	MAX17050_FG_I_acc	= 0x31,
	MAX17050_FG_QRtable20	= 0x32,
	MAX17050_FG_MaskSOC	= 0x33,
	MAX17050_FG_CHG_CNFG_10	= 0x34,
	MAX17050_FG_FullCAP0	= 0x35,
	MAX17050_FG_Iavg_empty	= 0x36,
	MAX17050_FG_FCTC	= 0x37,
	MAX17050_FG_RCOMP0	= 0x38,
	MAX17050_FG_TempCo	= 0x39,
	MAX17050_FG_V_empty	= 0x3A,
	MAX17050_FG_AvgCurrent0	= 0x3B,
	MAX17050_FG_TaskPeriod	= 0x3C,
	MAX17050_FG_FSTAT	= 0x3D,
	MAX17050_FG_TIMER       = 0x3E,
	MAX17050_FG_SHDNTIMER	= 0x3F,

	MAX17050_FG_AvgCurrentL	= 0x40,
	MAX17050_FG_AvgTAL	= 0x41,
	MAX17050_FG_QRtable30	= 0x42,
	MAX17050_FG_RepCapL	= 0x43,
	MAX17050_FG_AvgVCELL0	= 0x44,
	MAX17050_FG_dQacc	= 0x45,
	MAX17050_FG_dp_acc	= 0x46,
	MAX17050_FG_RlxSOC	= 0x47,
	MAX17050_FG_VFSOC0	= 0x48,
	MAX17050_FG_RemCapL	= 0x49,
	MAX17050_FG_VFRemCap	= 0x4A,
	MAX17050_FG_AvgVCELLL	= 0x4B,
	MAX17050_FG_QH0		= 0x4C,
	MAX17050_FG_QH		= 0x4D,
	MAX17050_FG_QL		= 0x4E,
	MAX17050_FG_RemCapL0	= 0x4F,
	MAX17050_FG_LOCK_I	= 0x62,
	MAX17050_FG_LOCK_II	= 0x63,
	MAX17050_FG_OCV		= 0x80,
	MAX17050_FG_VFOCV	= 0xFB,
	MAX17050_FG_VFSOC	= 0xFF,
};

int debugfs_regs[] = {
	MAX17050_FG_STATUS, MAX17050_FG_Age, MAX17050_FG_Cycles,
	MAX17050_FG_SHFTCFG, MAX17050_FG_VALRT_Th, MAX17050_FG_TALRT_Th,
	MAX17050_FG_SALRT_Th, MAX17050_FG_AvgCurrent, MAX17050_FG_Current,
	MAX17050_FG_MinMaxCurr, MAX17050_FG_VCELL, MAX17050_FG_AvgVCELL,
	MAX17050_FG_MinMaxVolt, MAX17050_FG_TEMP, MAX17050_FG_AvgTA,
	MAX17050_FG_MinMaxTemp, MAX17050_FG_AvSOC, MAX17050_FG_AvCap,
};

struct max17050_chip {
	struct i2c_client		*client;
	struct max17050_platform_data	*pdata;
	int				shutdown_complete;
	struct mutex			mutex;
	struct dentry			*dentry;
	struct device			*dev;
	struct power_supply		battery;
	bool				adjust_present;
	bool				is_low_temp;
	int				temp_normal2low_thr;
	int				temp_low2normal_thr;
	unsigned int	temp_normal_params[FLOUNDER_BATTERY_PARAMS_SIZE];
	unsigned int	temp_low_params[FLOUNDER_BATTERY_PARAMS_SIZE];
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
		msleep(20);
	}

	if (retry == MAX17050_I2C_RETRY_TIMES) {
		dev_err(&client->dev,
			"%s(): Failed in writing register 0x%02x after retry %d times\n"
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
		msleep(20);
	}

	if (retry == MAX17050_I2C_RETRY_TIMES) {
		dev_err(&client->dev,
			"%s(): Failed in reading register 0x%02x after retry %d times\n"
			, __func__, reg, MAX17050_I2C_RETRY_TIMES);
		mutex_unlock(&chip->mutex);
		return -EIO;
	}

	mutex_unlock(&chip->mutex);
	return (vals[1] << 8) | vals[0];
}

/* Return value in uV */
static int max17050_get_ocv(struct i2c_client *client, int *batt_ocv)
{
	int reg;

	reg = max17050_read_word(client, MAX17050_FG_VFOCV);
	if (reg < 0)
		return reg;
	*batt_ocv = (reg >> 4) * 1250;
	return 0;
}

static int max17050_get_vcell(struct i2c_client *client, int *vcell)
{
	int reg;
	int ret = 0;

	reg = max17050_read_word(client, MAX17050_FG_VCELL);
	if (reg < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, reg);
		ret = -EINVAL;
	} else
		*vcell = ((uint16_t)reg >> 3) * 625;

	return ret;
}

static int max17050_get_current(struct i2c_client *client, int *batt_curr)
{
	int curr;
	int ret = 0;

	/*
	 * TODO: Assumes current sense resistor is 10mohms.
	 */

	curr = max17050_read_word(client, MAX17050_FG_Current);
	if (curr < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, curr);
		ret = -EINVAL;
	} else
		*batt_curr = ((int16_t) curr) * 5000 / 32;

	return ret;
}

static int max17050_get_avgcurrent(struct i2c_client *client, int *batt_avg_curr)
{
	int avg_curr;
	int ret = 0;

	/*
	 * TODO: Assumes current sense resistor is 10mohms.
	 */

	avg_curr = max17050_read_word(client, MAX17050_FG_AvgCurrent);
	if (avg_curr < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, avg_curr);
		ret = -EINVAL;
	} else
		*batt_avg_curr = ((int16_t) avg_curr) * 5000 / 32;

	return ret;
}

static int max17050_get_charge(struct i2c_client *client, int *batt_charge)
{
	int charge;
	int ret = 0;

	/*
	 * TODO: Assumes current sense resistor is 10mohms.
	 */

	charge = max17050_read_word(client, MAX17050_FG_AvCap);
	if (charge < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, charge);
		ret = -EINVAL;
	} else
		*batt_charge = ((int16_t) charge) * 500;

	return ret;
}

static int max17050_get_charge_ext(struct i2c_client *client, int64_t *batt_charge_ext)
{
	int charge_msb, charge_lsb;
	int ret = 0;

	/*
	 * TODO: Assumes current sense resistor is 10mohms.
	 */

	charge_msb = max17050_read_word(client, MAX17050_FG_QH);
	charge_lsb = max17050_read_word(client, MAX17050_FG_QL);
	if (charge_msb < 0 || charge_lsb < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, charge_msb);
		ret = -EINVAL;
	} else
		*batt_charge_ext = (int64_t)((int16_t) charge_msb << 16 |
						(uint16_t) charge_lsb) * 8LL;

	return ret;
}

static int max17050_set_parameter_by_temp(struct i2c_client *client,
						bool is_low_temp)
{
	int ret = 0;
	int temp_params[FLOUNDER_BATTERY_PARAMS_SIZE];

	struct max17050_chip *chip = i2c_get_clientdata(client);

	if (is_low_temp)
		memcpy(temp_params, chip->temp_normal_params,
						sizeof(temp_params));
	else
		memcpy(temp_params, chip->temp_low_params,
						sizeof(temp_params));

	ret = max17050_write_word(client, MAX17050_FG_V_empty,
						temp_params[0]);
	if (ret < 0) {
		dev_err(&client->dev, "%s: write V_empty fail, err %d\n"
				, __func__, ret);
		return ret;
	}

	ret = max17050_write_word(client, MAX17050_FG_QRtable00,
						temp_params[1]);
	if (ret < 0) {
		dev_err(&client->dev, "%s: write QRtable00 fail, err %d\n"
				, __func__, ret);
		return ret;
	}

	ret = max17050_write_word(client, MAX17050_FG_QRtable10,
						temp_params[2]);
	if (ret < 0) {
		dev_err(&client->dev, "%s: write QRtable10 fail, err %d\n"
				, __func__, ret);
		return ret;
	}

	return 0;

}

static int __max17050_get_temperature(struct i2c_client *client, int *batt_temp)
{
	int temp;

	temp = max17050_read_word(client, MAX17050_FG_TEMP);
	if (temp < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, temp);
		return temp;
	}

	/* The value is signed. */
	if (temp & 0x8000) {
		temp = (0x7fff & ~temp) + 1;
		temp *= -1;
	}
	/* The value is converted into deci-centigrade scale */
	/* Units of LSB = 1 / 256 degree Celsius */

	*batt_temp = temp * 10 / (1 << 8);
	return 0;
}

static int max17050_get_temperature(struct i2c_client *client, int *batt_temp)
{
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

	*batt_temp = temp;
	if (max17050_data->adjust_present) {
		if (!max17050_data->is_low_temp &&
				temp <= max17050_data->temp_normal2low_thr) {
			max17050_set_parameter_by_temp(client, true);
			max17050_data->is_low_temp = true;
		} else if (max17050_data->is_low_temp &&
				temp >= max17050_data->temp_low2normal_thr) {
			max17050_set_parameter_by_temp(client, false);
			max17050_data->is_low_temp = false;
		}
	}

	return 0;
error:
	dev_err(&client->dev, "%s: temperature reading fail, err %d\n"
			, __func__, ret);
	return ret;
}

static int max17050_get_soc(struct i2c_client *client, int *soc_raw)
{
	int soc, soc_adjust;
	int ret = 0;

	soc = max17050_read_word(client, MAX17050_FG_RepSOC);
	if (soc < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, soc);
		ret = -EINVAL;
	} else {
		soc_adjust = (uint16_t)soc >> 8;
		if (!!(soc & 0xFF))
			soc_adjust += 1;
		*soc_raw = soc_adjust;
	}

	return ret;
}

static int htc_batt_max17050_get_vcell(int *batt_volt)
{
	int ret = 0;

	if (!batt_volt)
		return -EINVAL;

	if (!max17050_data)
		return -ENODEV;

	ret = max17050_get_vcell(max17050_data->client, batt_volt);

	return ret;
}

static int htc_batt_max17050_get_current(int *batt_curr)
{
	int ret = 0;

	if (!batt_curr)
		return -EINVAL;

	if (!max17050_data)
		return -ENODEV;

	ret = max17050_get_current(max17050_data->client, batt_curr);

	return ret;
}

static int htc_batt_max17050_get_avgcurrent(int *batt_avgcurr)
{
	int ret = 0;

	if (!batt_avgcurr)
		return -EINVAL;

	if (!max17050_data)
		return -ENODEV;

	ret = max17050_get_avgcurrent(max17050_data->client, batt_avgcurr);

	return ret;
}

static int htc_batt_max17050_get_charge(int *batt_charge)
{
	int ret = 0;

	if (!batt_charge)
		return -EINVAL;

	if (!max17050_data)
		return -ENODEV;

	ret = max17050_get_charge(max17050_data->client, batt_charge);

	return ret;
}

static int htc_batt_max17050_get_charge_ext(int64_t *batt_charge_ext)
{
	int ret = 0;

	if (!batt_charge_ext)
		return -EINVAL;

	if (!max17050_data)
		return -ENODEV;

	ret = max17050_get_charge_ext(max17050_data->client, batt_charge_ext);

	return ret;
}

static int htc_batt_max17050_get_temperature(int *batt_temp)
{
	int ret = 0;

	if (!batt_temp)
		return -EINVAL;

	if (!max17050_data)
		return -ENODEV;

	ret = max17050_get_temperature(max17050_data->client, batt_temp);

	return ret;
}

static int htc_batt_max17050_get_soc(int *batt_soc)
{
	int ret = 0;

	if (!batt_soc)
		return -EINVAL;

	if (!max17050_data)
		return -ENODEV;

	ret = max17050_get_soc(max17050_data->client, batt_soc);

	return ret;
}

static int htc_batt_max17050_get_ocv(int *batt_ocv)
{
	int ret = 0;

	if (!batt_ocv)
		return -EINVAL;

	if (!max17050_data)
		return -ENODEV;

	ret = max17050_get_ocv(max17050_data->client, batt_ocv);

	return ret;
}

static int max17050_debugfs_show(struct seq_file *s, void *unused)
{
	struct max17050_chip *chip = s->private;
	int index;
	u8 reg;
	unsigned int data;

	for (index = 0; index < ARRAY_SIZE(debugfs_regs); index++) {
		reg = debugfs_regs[index];
		data = max17050_read_word(chip->client, reg);
		if (data < 0)
			dev_err(&chip->client->dev, "%s: err %d\n", __func__,
									data);
		else
			seq_printf(s, "0x%02x:\t0x%04x\n", reg, data);
	}

	return 0;
}

static int max17050_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max17050_debugfs_show, inode->i_private);
}

struct htc_battery_max17050_ops htc_batt_max17050_ops = {
	.get_vcell = htc_batt_max17050_get_vcell,
	.get_battery_current = htc_batt_max17050_get_current,
	.get_battery_avgcurrent = htc_batt_max17050_get_avgcurrent,
	.get_temperature = htc_batt_max17050_get_temperature,
	.get_soc = htc_batt_max17050_get_soc,
	.get_ocv = htc_batt_max17050_get_ocv,
	.get_battery_charge = htc_batt_max17050_get_charge,
	.get_battery_charge_ext = htc_batt_max17050_get_charge_ext,
};

static const struct file_operations max17050_debugfs_fops = {
	.open		= max17050_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static enum power_supply_property max17050_prop[] = {
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_COUNTER_EXT,
};

static int max17050_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = htc_batt_max17050_get_vcell(&val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = htc_batt_max17050_get_current(&val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = htc_batt_max17050_get_avgcurrent(&val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = htc_batt_max17050_get_soc(&val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		htc_batt_max17050_get_ocv(&val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		htc_batt_max17050_get_temperature(&val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		htc_batt_max17050_get_charge(&val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER_EXT:
		htc_batt_max17050_get_charge_ext(&val->int64val);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static struct flounder_battery_platform_data
		*flounder_battery_dt_parse(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct flounder_battery_platform_data *pdata;
	struct device_node *map_node;
	struct device_node *child;
	int params_num = 0;
	int ret;
	u32 pval;
	u32 id_range[FLOUNDER_BATTERY_ID_RANGE_SIZE];
	u32 params[FLOUNDER_BATTERY_PARAMS_SIZE];
	struct flounder_battery_adjust_by_id *param;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->batt_id_channel_name =
		of_get_property(np, "battery-id-channel-name", NULL);

	map_node = of_get_child_by_name(np, "param_adjust_map_by_id");
	if (!map_node) {
		dev_warn(&client->dev,
				"parameter adjust map table not found\n");
		goto done;
	}

	for_each_child_of_node(map_node, child) {
		param = &pdata->batt_params[params_num];

		ret = of_property_read_u32(child, "id-number", &pval);
		if (!ret)
			param->id = pval;

		ret = of_property_read_u32_array(child, "id-range", id_range,
					FLOUNDER_BATTERY_ID_RANGE_SIZE);
		if (!ret)
			memcpy(param->id_range, id_range,
					sizeof(params) *
					FLOUNDER_BATTERY_ID_RANGE_SIZE);

		ret = of_property_read_u32(child,
					"temperature-normal-to-low-threshold",
					&pval);
		if (!ret)
			param->temp_normal2low_thr = pval;

		ret = of_property_read_u32(child,
					"temperature-low-to-normal-threshold",
					&pval);
		if (!ret)
			param->temp_low2normal_thr = pval;

		ret = of_property_read_u32_array(child,
					"temperature-normal-parameters",
					params,
					FLOUNDER_BATTERY_PARAMS_SIZE);
		if (!ret)
			memcpy(param->temp_normal_params, params,
					sizeof(params) *
					FLOUNDER_BATTERY_PARAMS_SIZE);

		ret = of_property_read_u32_array(child,
					"temperature-low-parameters",
					params,
					FLOUNDER_BATTERY_PARAMS_SIZE);
		if (!ret)
			memcpy(param->temp_low_params, params,
					sizeof(params) *
					FLOUNDER_BATTERY_PARAMS_SIZE);

		if (++params_num >= FLOUNDER_BATTERY_ID_MAX)
			break;
	}

done:
	pdata->batt_params_num = params_num;
	return pdata;
}

static int flounder_battery_id_check(
		struct flounder_battery_platform_data *pdata,
		struct max17050_chip *data)
{
	int batt_id = 0;
	struct iio_channel *batt_id_channel;
	int ret;
	int i;

	if (!pdata->batt_id_channel_name || pdata->batt_params_num == 0)
		return -EINVAL;

	batt_id_channel = iio_channel_get(NULL, pdata->batt_id_channel_name);
	if (IS_ERR(batt_id_channel)) {
		dev_err(data->dev,
				"Failed to get iio channel %s, %ld\n",
				pdata->batt_id_channel_name,
				PTR_ERR(batt_id_channel));
		return -EINVAL;
	}

	ret = iio_read_channel_processed(batt_id_channel, &batt_id);
	if (ret < 0)
		ret = iio_read_channel_raw(batt_id_channel, &batt_id);

	if (ret < 0) {
		dev_err(data->dev,
				"Failed to read batt id, ret=%d\n",
				ret);
		return -EFAULT;
	}

	dev_dbg(data->dev, "Battery id adc value is %d\n", batt_id);

	for (i = 0; i < pdata->batt_params_num; i++) {
		if (batt_id >= pdata->batt_params[i].id_range[0] &&
				batt_id <= pdata->batt_params[i].id_range[1]) {
			data->temp_normal2low_thr =
				pdata->batt_params[i].temp_normal2low_thr;
			data->temp_low2normal_thr =
				pdata->batt_params[i].temp_low2normal_thr;
			memcpy(data->temp_normal_params,
				pdata->batt_params[i].temp_normal_params,
				sizeof(data->temp_normal_params) *
				FLOUNDER_BATTERY_PARAMS_SIZE);
			memcpy(data->temp_low_params,
				pdata->batt_params[i].temp_low_params,
				sizeof(data->temp_low_params) *
				FLOUNDER_BATTERY_PARAMS_SIZE);

			data->adjust_present = true;
			max17050_set_parameter_by_temp(data->client, false);
			data->is_low_temp = false;
			return 0;
		}
	}

	return -ENODATA;
}

static int max17050_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct max17050_chip *chip;
	struct flounder_battery_platform_data *pdata = NULL;
	int ret;
	bool ignore_param_adjust = false;

	if (client->dev.platform_data)
		pdata = client->dev.platform_data;

	if (!pdata && client->dev.of_node) {
		pdata = flounder_battery_dt_parse(client);
		if (IS_ERR(pdata)) {
			ret = PTR_ERR(pdata);
			dev_err(&client->dev, "Parsing of node failed, %d\n",
				ret);
			ignore_param_adjust = true;
		}
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	mutex_init(&chip->mutex);
	chip->shutdown_complete = 0;
	i2c_set_clientdata(client, chip);

	if (!ignore_param_adjust)
		flounder_battery_id_check(pdata, chip);

	max17050_data = chip;

	chip->battery.name		= "battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17050_get_property;
	chip->battery.properties	= max17050_prop;
	chip->battery.num_properties	= ARRAY_SIZE(max17050_prop);
	chip->dev			= &client->dev;

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret)
		dev_err(&client->dev, "failed: power supply register\n");

	chip->dentry = debugfs_create_file("max17050-regs", S_IRUGO, NULL,
						chip, &max17050_debugfs_fops);

	return 0;
}

static int max17050_remove(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);

	debugfs_remove(chip->dentry);
	power_supply_unregister(&chip->battery);
	mutex_destroy(&chip->mutex);

	return 0;
}

static void max17050_shutdown(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->mutex);
	chip->shutdown_complete = 1;
	mutex_unlock(&chip->mutex);

}

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
device_initcall(max17050_init);

static void __exit max17050_exit(void)
{
	i2c_del_driver(&max17050_i2c_driver);
}
module_exit(max17050_exit);

MODULE_DESCRIPTION("MAX17050 Fuel Gauge");
MODULE_LICENSE("GPL");
