/*
 *  max17047.c
 *
 *  Copyright (C) 2012 Maxim Integrated Product
 *  Gyungoh Yoo <jack.yoo@maxim-ic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/power/max17047.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/mailbox.h>
#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/of_irq.h>
#include <linux/regmap-ipc.h>
#include <linux/gpio.h>
#ifdef CONFIG_MAX17047_JEITA
#include <linux/max77665_power.h>
#endif

#define MAX17047_NAME					"max17047"
#define MAX17047_I2C_SLOT				4
#define MAX17047_SLAVE_ADDRESS			0x6C
#define MAX17047_REG_STATUS				0x00
#define MAX17047_REG_VALRT_TH			0x01
#define MAX17047_REG_TALRT_TH			0x02
#define MAX17047_REG_SOCALRT_TH			0x03
#define MAX17047_REG_ATRATE				0x04
#define MAX17047_REG_REMCAPREP			0x05
#define MAX17047_REG_SOCREP				0x06
#define MAX17047_REG_AGE				0x07
#define MAX17047_REG_TEMPERATURE		0x08
#define MAX17047_REG_VCELL				0x09
#define MAX17047_REG_CURRENT			0x0A
#define MAX17047_REG_AVERAGECURRENT		0x0B
#define MAX17047_REG_SOCMIX				0x0D
#define MAX17047_REG_SOCAV				0x0E
#define MAX17047_REG_REMCAPMIX			0x0F
#define MAX17047_REG_FULLCAP			0x10
#define MAX17047_REG_TTE				0x11
#define MAX17047_REG_QRESIDUAL00		0x12
#define MAX17047_REG_FULLSOCTHR			0x13
#define MAX17047_REG_AERAGETEMPERATURE	0x16
#define MAX17047_REG_CYCLES				0x17
#define MAX17047_REG_DESIGNCAP			0x18
#define MAX17047_REG_AVERAGEVCELL		0x19
#define MAX17047_REG_MAXMINTEMPERATURE	0x1A
#define MAX17047_REG_MAXMINVCELL		0x1B
#define MAX17047_REG_MAXMINCURRENT		0x1C
#define MAX17047_REG_CONFIG				0x1D
#define MAX17047_REG_ICHGTERM			0x1E
#define MAX17047_REG_REMCAPAV			0x1F
#define MAX17047_REG_VERSION			0x21
#define MAX17047_REG_QRESIDUAL10		0x22
#define MAX17047_REG_FULLCAPNOM			0x23
#define MAX17047_REG_TEMPNOM			0x24
#define MAX17047_REG_TEMPLIM			0x25
#define MAX17047_REG_AIN				0x27
#define MAX17047_REG_LEARNCFG			0x28
#define MAX17047_REG_FILTERCFG			0x29
#define MAX17047_REG_RELAXCFG			0x2A
#define MAX17047_REG_MISCCFG			0x2B
#define MAX17047_REG_TGAIN				0x2C
#define MAX17047_REG_TOFF				0x2D
#define MAX17047_REG_CGAIN				0x2E
#define MAX17047_REG_COFF				0x2F
#define MAX17047_REG_QRESIDUAL20		0x32
#define MAX17047_REG_IAVG_EMPTY			0x36
#define MAX17047_REG_FCTC				0x37
#define MAX17047_REG_RCOMP0				0x38
#define MAX17047_REG_TEMPCO				0x39
#define MAX17047_REG_V_EMPTY			0x3A
#define MAX17047_REG_FSTAT				0x3D
#define MAX17047_REG_TIMER				0x3E
#define MAX17047_REG_SHDNTIMER			0x3F
#define MAX17047_REG_QRESIDUAL30		0x42
#define MAX17047_REG_DQACC				0x45
#define MAX17047_REG_DPACC				0x46
#define MAX17047_REG_QH					0x4D
#define MAX17047_REG_CHAR_TBL			0x80
#define MAX17047_REG_VFOCV				0xFB
#define MAX17047_REG_SOCVF				0xFF

/* MAX17047_REG_STATUS */
#define MAX17047_R_POR					0x0002
#define MAX17047_R_BST					0x0008
#define MAX17047_R_VMN					0x0100
#define MAX17047_R_TMN					0x0200
#define MAX17047_R_SMN					0x0400
#define MAX17047_R_BI					0x0800
#define MAX17047_R_VMX					0x1000
#define MAX17047_R_TMX					0x2000
#define MAX17047_R_SMX					0x4000
#define MAX17047_R_BR					0x8000

/* MAX17047_REG_CONFIG */
#define MAX17047_R_BER					0x0001
#define MAX17047_R_BEI					0x0002
#define MAX17047_R_AEN					0x0004
#define MAX17047_R_FTHRM				0x0008
#define MAX17047_R_ETHRM				0x0010
#define MAX17047_R_ALSH					0x0020
#define MAX17047_R_I2CSH				0x0040
#define MAX17047_R_SHDN					0x0080
#define MAX17047_R_TEX					0x0100
#define MAX17047_R_TEN					0x0200
#define MAX17047_R_AINSH				0x0400
#define MAX17047_R_ALRTP				0x0800
#define MAX17047_R_VS					0x1000
#define MAX17047_R_TS					0x2000
#define MAX17047_R_SS					0x4000

#define MAX17047_WORK_DELAY				10000
#define RETRY_CNT						3
#define LEARNED_PARAM_LEN				50

#ifdef CONFIG_MAX17047_JEITA
enum max17047_jeita_status
{
	MAX17047_COLD,
	MAX17047_COOL,
	MAX17047_NORMAL,
	MAX17047_WARM,
	MAX17047_HOT,
};
#endif

struct max17047
{
	struct device *dev;
	struct regmap *regmap;
	struct max17047_platform_data *pdata;
	struct power_supply psy;
	struct delayed_work check_work;
	int irq;
	int r_sns;
	char learned_parameter[LEARNED_PARAM_LEN];
	bool current_sensing;
#ifdef CONFIG_MAX17047_JEITA
	enum max17047_jeita_status jeita_status;
	struct power_supply *charger;
	struct max77665_charger_pdata *charger_pdata;
#endif
};

/*             
                                                                                                
                                                                                              
                                
 */
#define CONFIG_ODIN_HDK_WA_FG_MAILBOX_ERR	/* workaround to avoid gauge display error */

#ifdef CONFIG_ODIN_HDK_WA_FG_MAILBOX_ERR
static int workaround_soc = -1;
static int workaround_vol = -1;
static int workaround_on = -1;
#endif
/*              */

static inline int max17047_write_verify_reg(struct max17047 *max17047, u8 reg, u16 val)
{
	int i;
	int ret;
	unsigned int tmp;

	for (i = 0; i < RETRY_CNT; i++)
	{
		ret = regmap_write(max17047->regmap, reg, val);
		if (IS_ERR_VALUE(ret))
			return ret;

		ret = regmap_read(max17047->regmap, reg, &tmp);
		if (IS_ERR_VALUE(ret))
			return ret;

		if (likely(val == tmp))
			return 0;
	}

	return -EIO;
}

static int max17047_get_capacity(struct max17047 *max17047, int *soc)
{
	unsigned int value;
	int ret = -EINVAL;

	ret = regmap_read(max17047->regmap,
			max17047->current_sensing ? MAX17047_REG_SOCREP : MAX17047_REG_SOCVF,
			&value);
	if (likely(ret >= 0))
	{
#ifdef CONFIG_MACH_ODIN_HDK
		value = (((value + 0x0080) >> 8) * 8695) / 10000;
		if (value > 100)
			value = 100;
		else if (value < 0)
			value = 0;
#else
		/* unit = (1 / 256) % */
		value = (value + 0x0080) >> 8;
#endif

		*soc = value;
	}

	return ret;
}

static int max17047_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17047 *max17047 = container_of(psy, struct max17047, psy);
	unsigned int value;
	int ret = -EINVAL;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = regmap_read(max17047->regmap, MAX17047_REG_STATUS, &value);
		if (likely(ret >= 0))
		{
			/* Bst is 0 when a battery is present */
			val->intval = value & MAX17047_R_BST ? 0 : 1;
#ifdef CONFIG_ODIN_HDK_WA_FG_MAILBOX_ERR
			workaround_on = val->intval;
#endif
		}
#ifdef CONFIG_ODIN_HDK_WA_FG_MAILBOX_ERR
		else {
			dev_err(max17047->dev, "read status err in %s... return with prev value (%d)\n",
					__func__, workaround_on);
			val->intval = workaround_on;
			ret = 0;
		}
#endif
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = regmap_read(max17047->regmap, MAX17047_REG_CYCLES, &value);
		if (likely(ret >= 0))
		{
			val->intval = value;
		}
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = regmap_read(max17047->regmap, MAX17047_REG_MAXMINVCELL, &value);
		if (likely(ret >= 0))
		{
			/* unit = 20mV */
			val->intval = (int)(value >> 8) * 20000;
		}
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		ret = regmap_read(max17047->regmap, MAX17047_REG_MAXMINVCELL, &value);
		if (likely(ret >= 0))
		{
			/* unit = 20mV */
			val->intval = (int)(value & 0xFF) * 20000;
		}
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = regmap_read(max17047->regmap, MAX17047_REG_VCELL, &value);
		if (likely(ret >= 0))
		{
			/* unit = 0.625mV */
			val->intval = (int)(value >> 3) * 625;
#ifdef CONFIG_ODIN_HDK_WA_FG_MAILBOX_ERR
			workaround_vol = val->intval;
#endif
		}
#ifdef CONFIG_ODIN_HDK_WA_FG_MAILBOX_ERR
		else {
			dev_err(max17047->dev, "read voltage err in %s... return with prev value (%d)\n",
					__func__, workaround_vol);
			val->intval = workaround_vol;
			ret = 0;
		}
#endif
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = regmap_read(max17047->regmap, MAX17047_REG_AVERAGEVCELL, &value);
		if (likely(ret >= 0))
		{
			/* unit = 0.625mV */
			val->intval = (int)(value >> 3) * 625;
		}
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		/* Assume r_sns >= 0 */
		ret = regmap_read(max17047->regmap, MAX17047_REG_MAXMINCURRENT, &value);
		if (likely(ret >= 0))
		{
			/* unit = 0.4mV / RSENSE */
			val->intval = (int)(value >> 8) * 400 * 1000 / max17047->r_sns;
		}
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* Assume r_sns >= 0 */
		ret = regmap_read(max17047->regmap, MAX17047_REG_CURRENT, &value);
		if (likely(ret >= 0))
		{
			/* unit = 1.5625uV / RSENSE */
			val->intval = (int)value * 15625 / max17047->r_sns / 1000;
		}
		break;

	case POWER_SUPPLY_PROP_CURRENT_AVG:
		/* Assume r_sns >= 0 */
		ret = regmap_read(max17047->regmap, MAX17047_REG_AVERAGECURRENT, &value);
		if (likely(ret >= 0))
		{
			/* unit = 1.5625uV / RSENSE */
			val->intval = (int)value * 15625 / max17047->r_sns / 1000;
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		/* Assume r_sns >= 0 */
		ret = regmap_read(max17047->regmap, MAX17047_REG_DESIGNCAP, &value);
		if (likely(ret >= 0))
		{
			/* unit = 5.0uVh / RSENSE */
			val->intval = (int)value * 5 * 1000 / max17047->r_sns;
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
		/* Assume r_sns >= 0 */
		ret = regmap_read(max17047->regmap, MAX17047_REG_FULLCAPNOM, &value);
		if (likely(ret >= 0))
		{
			/* unit = 5.0uVh / RSENSE */
			val->intval = (int)value * 5 * 1000 / max17047->r_sns;
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		/* Assume r_sns >= 0 */
		ret = regmap_read(max17047->regmap, MAX17047_REG_REMCAPREP, &value);
		if (likely(ret >= 0))
		{
			/* unit = 5.0uVh / RSENSE */
			val->intval = (int)value * 5 * 1000 / max17047->r_sns;
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_AVG:
		/* Assume r_sns >= 0 */
		ret = regmap_read(max17047->regmap, MAX17047_REG_REMCAPAV, &value);
		if (likely(ret >= 0))
		{
			/* unit = 5.0uVh / RSENSE */
			val->intval = (int)value * 5 * 1000 / max17047->r_sns;
		}
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		ret = max17047_get_capacity(max17047, &value);
		if (likely(ret >= 0))
		{
			val->intval = value;
#ifdef CONFIG_ODIN_HDK_WA_FG_MAILBOX_ERR
			workaround_soc = val->intval;
#endif
		}
#ifdef CONFIG_ODIN_HDK_WA_FG_MAILBOX_ERR
		else  {
			dev_err(max17047->dev, "read soc err in %s... return with prev value (%d)\n",
					__func__, workaround_soc);
			val->intval = workaround_soc;
			ret = 0;
		}
#endif
		break;

	case POWER_SUPPLY_PROP_TEMP:
		ret = regmap_read(max17047->regmap, MAX17047_REG_TEMPERATURE, &value);
		if (likely(ret >= 0))
		{
			/* unit = tenths of degree Celsius */
			val->intval = ((short)value * 10) / 256;
		}
		break;

	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		ret = regmap_read(max17047->regmap, MAX17047_REG_TTE, &value);
		if (likely(ret >= 0))
		{
			/* unit = 3min */
			val->intval = value * 180;
		}
		break;

	default:
		ret = -EINVAL;
	}
	return ret;
}

static void max17047_set_charged(struct power_supply *psy)
{
	struct max17047 *max17047 = container_of(psy, struct max17047, psy);
	int ret;
	unsigned int val;

	/* Make battery full. To make RepSoc = 100%, write FullCap = RepCap */
	ret = regmap_read(max17047->regmap, MAX17047_REG_SOCREP, &val);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max17047->psy.dev, "max17047_read_reg() returned %d in ", ret);
		return;
	}

	ret = regmap_write(max17047->regmap, MAX17047_REG_FULLCAP, val);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max17047->psy.dev, "max17047_write_reg() returned %d in ", ret);
		return;
	}

	power_supply_changed(&max17047->psy);
}

#ifdef CONFIG_MAX17047_JEITA
static void max17047_change_jeita_status(struct max17047 *max17047, u8 temp)
{
	struct max77665_charger_pdata *charger_pdata = max17047->charger_pdata;
	u8 min, max;
	unsigned int val;
	int ret;

	switch(max17047->jeita_status)
	{
		case MAX17047_COLD:
			if (temp > charger_pdata->t1)
				max17047->jeita_status = MAX17047_COOL;
			else if (temp > charger_pdata->t2 + 3)
				max17047->jeita_status = MAX17047_NORMAL;
			break;

		case MAX17047_COOL:
			if (temp < charger_pdata->t1)
				max17047->jeita_status = MAX17047_COLD;
			else if (temp > charger_pdata->t2 + 3)
				max17047->jeita_status = MAX17047_NORMAL;
			break;

		case MAX17047_NORMAL:
			if (temp < charger_pdata->t2)
				max17047->jeita_status = MAX17047_COOL;
			else if (temp > charger_pdata->t3)
				max17047->jeita_status = MAX17047_WARM;
			break;

		case MAX17047_WARM:
			if (temp < charger_pdata->t3 - 3)
				max17047->jeita_status = MAX17047_NORMAL;
			else if (temp > charger_pdata->t4)
				max17047->jeita_status = MAX17047_HOT;
			break;

		case MAX17047_HOT:
			if (temp < charger_pdata->t3 - 3)
				max17047->jeita_status = MAX17047_NORMAL;
			else if (temp < charger_pdata->t4)
				max17047->jeita_status = MAX17047_WARM;
			break;
	}

	switch(max17047->jeita_status)
	{
		case MAX17047_COLD:
			min = 0x80;
			max = charger_pdata->t1 + 3;
			max77665_charger_enable(&max77665_charger->charger, FALSE);
			max77665_change_jeita_status(max17047->charger, FALSE, TRUE);
			break;

		case MAX17047_COOL:
			min = charger_pdata->t1;
			max = charger_pdata->t2 + 3;
			max77665_change_jeita_status(max17047->charger, FALSE, TRUE);
			max77665_charger_enable(&max77665_charger->charger, TRUE);
			break;

		case MAX17047_NORMAL:
			min = charger_pdata->t2;
			max = charger_pdata->t3;
			max77665_change_jeita_status(max17047->charger, FALSE, FALSE);
			max77665_charger_enable(&max77665_charger->charger, TRUE);
			break;

		case MAX17047_WARM:
			min = charger_pdata->t3 - 3;
			max = charger_pdata->t4;
			max77665_change_jeita_status(max17047->charger, TRUE, FALSE);
			max77665_charger_enable(&max77665_charger->charger, TRUE);
			break;

		case MAX17047_HOT:
			min = charger_pdata->t4 - 3;
			max = 0x7F;
			max77665_charger_enable(&max77665_charger->charger, FALSE);
			max77665_change_jeita_status(max17047->charger, TRUE, FALSE);
			break;
	}

	val = ((unsigned int)max << 8) | min;
	ret = regmap_write(max17047->regmap, MAX17047_REG_TALRT_TH, val);
	if (IS_ERR_VALUE(ret))
		dev_err(&max17047->psy.dev, "max17047_write_reg() errors in %s\n", __FUNC__);
}
#endif

static enum power_supply_property max17047_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
};

static enum power_supply_property max17047_props_no_rsns[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
};

static int max17047_save_learned_parameter(struct max17047 *max17047)
{
	unsigned int rcomp0, tempco, fullcap, cycles, fullcapnom,
			iavg_empty, qrtable0, qrtable1, qrtable2, qrtable3;
	int ret;

	/* Step 20. Save Learned Parameters
	 * It is recommended to save the learned capacity parameters every time
	 * bit 6 of the Cycles register toggles (so that it is saved every 64% change in the battery)
	 * so that if power is lost the values can easily be restored.
	 */
	ret = regmap_read(max17047->regmap, MAX17047_REG_RCOMP0, &rcomp0);
	ret |= regmap_read(max17047->regmap, MAX17047_REG_TEMPCO, &tempco);
	ret |= regmap_read(max17047->regmap, MAX17047_REG_FULLCAP, &fullcap);
	ret |= regmap_read(max17047->regmap, MAX17047_REG_CYCLES, &cycles);
	ret |= regmap_read(max17047->regmap, MAX17047_REG_FULLCAPNOM, &fullcapnom);
	ret |= regmap_read(max17047->regmap, MAX17047_REG_IAVG_EMPTY, &iavg_empty);
	ret |= regmap_read(max17047->regmap, MAX17047_REG_QRESIDUAL00, &qrtable0);
	ret |= regmap_read(max17047->regmap, MAX17047_REG_QRESIDUAL10, &qrtable1);
	ret |= regmap_read(max17047->regmap, MAX17047_REG_QRESIDUAL20, &qrtable2);
	ret |= regmap_read(max17047->regmap, MAX17047_REG_QRESIDUAL30, &qrtable3);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max17047->psy.dev, "cannot read learned parameter\n");
		return -EIO;
	}

	snprintf(max17047->learned_parameter, LEARNED_PARAM_LEN,
		"%04X %04X %04X %04X %04X %04X %04X %04X %04X %04X",
		rcomp0, tempco, fullcap, cycles, fullcapnom, iavg_empty,
		qrtable0, qrtable1, qrtable2, qrtable3);

	return 0;
}

static int max17047_restore_learned_parameter(struct max17047 *max17047)
{
	unsigned int rcomp0, tempco, fullcap, cycles, fullcapnom,
			iavg_empty, qrtable0, qrtable1, qrtable2, qrtable3;
	unsigned int fullcap0, remcap, val;
	int ret;

	/* Step 21. Restoring Capacity Parameters
	 */
	sscanf(max17047->learned_parameter, "%x %x %x %x %x %x %x %x %x %x",
		&rcomp0, &tempco, &fullcap, &cycles, &fullcapnom, &iavg_empty,
		&qrtable0, &qrtable1, &qrtable2, &qrtable3);

	/* If power is lost, then the Capacity information can be easily restored with the following procedure.
	 */
	ret = regmap_write(max17047->regmap, MAX17047_REG_RCOMP0, rcomp0);
	ret |= regmap_write(max17047->regmap, MAX17047_REG_TEMPCO, tempco);
	ret |= regmap_write(max17047->regmap, MAX17047_REG_IAVG_EMPTY, iavg_empty);
	ret |= regmap_write(max17047->regmap, MAX17047_REG_FULLCAPNOM, fullcapnom);
	ret |= regmap_write(max17047->regmap, MAX17047_REG_QRESIDUAL00, qrtable0);
	ret |= regmap_write(max17047->regmap, MAX17047_REG_QRESIDUAL10, qrtable1);
	ret |= regmap_write(max17047->regmap, MAX17047_REG_QRESIDUAL20, qrtable2);
	ret |= regmap_write(max17047->regmap, MAX17047_REG_QRESIDUAL30, qrtable3);
	if (IS_ERR_VALUE(ret))
		return -EIO;

	/* Step 22. Wait 350ms
	 */
	mdelay(350);

	/* Step 23. Restore FullCap
	 */
	ret = regmap_read(max17047->regmap, 0x35, &fullcap0);
	ret |= regmap_read(max17047->regmap, MAX17047_REG_SOCMIX, &val);
	if (IS_ERR_VALUE(ret))
		return -EIO;

	remcap = val * fullcap0 / 25600;
	ret = regmap_write(max17047->regmap, MAX17047_REG_REMCAPMIX, remcap);
	ret |= regmap_write(max17047->regmap, MAX17047_REG_FULLCAP, fullcap);
	ret |= regmap_write(max17047->regmap, MAX17047_REG_DQACC, fullcapnom / 4);
	ret |= regmap_write(max17047->regmap, MAX17047_REG_DPACC, 0x3200);
	if (unlikely(ret < 0))
		return -EIO;

	/* Step 22. Wait 350ms
	 */
	mdelay(350);

	/* Step 25. Restore Cycles Register
	 */
	ret = regmap_write(max17047->regmap, MAX17047_REG_CYCLES, cycles);
	if (cycles > 0xFF)
		ret |= regmap_write(max17047->regmap, MAX17047_REG_LEARNCFG, 0x0676);

	if (IS_ERR_VALUE(ret))
	{
		dev_err(max17047->psy.dev, "cannot write CYCLES\n");
		return -EIO;
	}

	return 0;
}

static int max17047_hw_init(struct max17047 *max17047)
{
	struct max17047_platform_data *pdata = max17047->pdata;
	unsigned int vfsoc, remcap, repcap;
	unsigned int val;
#ifdef CONFIG_BATTERY_MAX17047_CUSTOM_MODEL
	u16 tmp[16 * 3];
	int i, n;
	u32 sum;
	u32 temp;
#endif
	int ret;

	/*
	* INITIALIZE REGISTERS TO RECOMMENDED CONFIGURATION
	* -------------------------------------------------
	* The MAX77696 Fuel Gauge should be initialized prior to being used.
	* The following three registers should be written to these values in order
	* for the MAX77696 Fuel Gauge to perform at its best. These values are
	* written to RAM, so they must be written to the device any time that power
	* is applied or restored to the device. Some registers are updated
	* internally, so it is necessary to verify that the register was written
	* correctly to prevent data collisions.
	*/

	/* Step 0. Check for POR
	 */
	ret = regmap_read(max17047->regmap, MAX17047_REG_STATUS, &val);
	if (unlikely(ret < 0))
	{
		dev_err(max17047->psy.dev, "cannot read POR\n");
		return ret;
	}

	if (!(val & MAX17047_R_POR))
	{
		dev_info(max17047->psy.dev, "POR is not set\n");
		return 0;
	}

	/* Delay at least 500ms
	 */
	msleep(500);

	/* Initialize Configuration
	 */
	/* register setting model gauge m1 */
	regmap_write(max17047->regmap, MAX17047_REG_CONFIG, 0x2104);
	regmap_write(max17047->regmap, MAX17047_REG_CGAIN, 0x0000);
	regmap_write(max17047->regmap, MAX17047_REG_MISCCFG, 0x0003);
	regmap_write(max17047->regmap, MAX17047_REG_LEARNCFG, 0x0007);
	regmap_write(max17047->regmap, MAX17047_REG_SOCALRT_TH, 0x7810);

	/*
	* LOAD CUSTOM MODEL AND PARAMETERS
	* --------------------------------
	* The custom model that is stored in the MAX77696 Fuel Gauge is also
	* written to RAM and so it must be written to the device any time that
	* power is applied or restored to the device. When the device is powered
	* on, the host software must first unlock write access to the model, write
	* the model, verify the model was written properly, and then lock access to
	* the model. After the model is loaded correctly, simply write a few
	* registers with customized parameters that will be provided by Maxim.
	*/

#ifdef CONFIG_BATTERY_MAX17047_CUSTOM_MODEL
	for (i = 0; i < RETRY_CNT; i++)
	{
		/* Step 4. Unlock Model Access
		 */
		regmap_write(max17047->regmap, 0x62, 0x0059);
		regmap_write(max17047->regmap, 0x63, 0x00C4);

		/* Step 5. Write/Read/Verify the Custom Model
		 * Once the model is unlocked, the host software must write the 48 word model to the MAX17042/7.
		 * The model islocated between memory locations 0x80h and 0xAFh.
		 */
		regmap_bulk_write(max17047->regmap, 0x80, &pdata->custome_model[0], 16);
		regmap_bulk_write(max17047->regmap, 0x90, &pdata->custome_model[16], 16);
		regmap_bulk_write(max17047->regmap, 0xA0, &pdata->custome_model[32], 16);
		/* The model can be read directly back from the MAX17047.
		 * So simply read the 48 words of the model back from the device to verify if it was written correctly.
		 */
		regmap_bulk_read(max17047->regmap, 0x80, &tmp[0], 16);
		regmap_bulk_read(max17047->regmap, 0x90, &tmp[16], 16);
		regmap_bulk_read(max17047->regmap, 0xA0, &tmp[32], 16);
		/* If any of the values do not matc, return to step 4.
		 */
		ret = memcmp(&pdata->custome_model[0], &tmp[0], sizeof(tmp) * sizeof(u16));
		if (likely(ret == 0))
			break;
	}
	if (unlikely(i >= RETRY_CNT))
	{
		dev_err(max17047->psy.dev, "cannot write model data\n");
		return -EIO;
	}

	/* Steps 6 & 7 are deleted, numbering continues at step 8 for legacy reasons
	 */

	for (i = 0; i < RETRY_CNT; i++)
	{
		/* Step 8. Lock Model Access
		 */
		regmap_write(max17047->regmap, 0x62, 0x0000);
		regmap_write(max17047->regmap, 0x63, 0x0000);

		/* Step 9. Verify that Model Access is locked
		 * If the model remains unlocked, the MAX17042/7 will not be able to monitor the capacity of the battery.
		 * Therefore it is very critical that the Model Access is locked.
		 * To verify it is locked, simply read back the model as in Step 5.
		 */
		regmap_bulk_read(max17047->regmap, 0x80, &tmp[0], 16);
		regmap_bulk_read(max17047->regmap, 0x90, &tmp[16], 16);
		regmap_bulk_read(max17047->regmap, 0xA0, &tmp[32], 16);
		/* However, this time, all values should be read as 0x00h.
		 * If any values are non-zero, repeat Step 8 to make sure the Model Access is locked.
		 */
		for (sum = 0, n = 0; n < 16 * 3; n++)
			sum += tmp[n];
		if (sum == 0)
			break;
	}
	if (unlikely(i >= RETRY_CNT))
	{
		dev_err(max17047->psy.dev, "cannot unlock model data\n");
		return -EIO;
	}

	/* Step 10. Write Custom Parameters
	 */
	max17047_write_verify_reg(max17047, MAX17047_REG_RCOMP0, pdata->rcomp0);
	max17047_write_verify_reg(max17047, MAX17047_REG_TEMPCO, pdata->tempco);
	regmap_write(max17047->regmap, MAX17047_REG_ICHGTERM, pdata->termcurr);
	regmap_write(max17047->regmap, MAX17047_REG_TGAIN, pdata->tgain);
	regmap_write(max17047->regmap, MAX17047_REG_TOFF, pdata->toff);
	max17047_write_verify_reg(max17047, MAX17047_REG_V_EMPTY, pdata->vempty);
	max17047_write_verify_reg(max17047, MAX17047_REG_QRESIDUAL00, pdata->qrtable00);
	max17047_write_verify_reg(max17047, MAX17047_REG_QRESIDUAL10, pdata->qrtable10);
	max17047_write_verify_reg(max17047, MAX17047_REG_QRESIDUAL20, pdata->qrtable20);
	max17047_write_verify_reg(max17047, MAX17047_REG_QRESIDUAL30, pdata->qrtable30);
#endif

	/* Step 11. Update Full Capacity Parameters
	 */
	max17047_write_verify_reg(max17047, MAX17047_REG_FULLCAP, pdata->capacity);
	regmap_write(max17047->regmap, MAX17047_REG_DESIGNCAP, pdata->vf_fullcap);
	max17047_write_verify_reg(max17047, MAX17047_REG_FULLCAPNOM, pdata->vf_fullcap);

	/* Step 13. Delay at least 350ms
	 */
	msleep(350);

	/* Step 14. Write VFSOC and QH values to VFSOC0 and QH0
	 */
	/* Read VFSOC */
	regmap_read(max17047->regmap, MAX17047_REG_SOCVF, &vfsoc);
	/* Enable Write Access to VFSOC0 */
	regmap_write(max17047->regmap, 0x60, 0x0080);
	/* Write and Verify VFSOC0 */
	regmap_write(max17047->regmap, 0x48, vfsoc);
	/* Disable Write Access to VFSOC0 */
	regmap_write(max17047->regmap, 0x60,0x0000);
	/* Read Qh register */
	regmap_read(max17047->regmap, MAX17047_REG_QH, &val);
	/* Write Qh to Qh0 */
	regmap_write(max17047->regmap, 0x4C, val);

	/* Step 15.5 Advance to Coulomb-Counter Mode
	 * Advancing the cycles register to a higher values makes the fuelgauge behave more like a coulomb counter.
	 * MAX17047 supports quicker insertion error healing by supporting starting from a lower learn stage.
	 * To Advance to Coulomb-Counter Mode,
	 * simply write the Cycles register to a value of 160% for MAX17042 and 96% for MAX17047.
	 */
	max17047_write_verify_reg(max17047, MAX17047_REG_CYCLES, 0x0060);

	/* Step 16. Load New Capacity Parameters */
	remcap = vfsoc * pdata->vf_fullcap / 25600;
	max17047_write_verify_reg(max17047, MAX17047_REG_REMCAPMIX, remcap);
	/* ModelScaling is used if your FullPoint VFSOC is scaled down.
	 * This was used as a fix for the MAX17042 early charge termination. MAX17047/50 will use ModelScaling = 1
	 */
	repcap = remcap * (pdata->capacity / pdata->vf_fullcap) / 1;
	max17047_write_verify_reg(max17047, MAX17047_REG_REMCAPREP, repcap);
	/* Write dQ_acc to 200% of Capacity and dP_acc to 200% */
	val = pdata->vf_fullcap / 4;
	max17047_write_verify_reg(max17047, MAX17047_REG_DQACC, val);
	max17047_write_verify_reg(max17047, MAX17047_REG_DPACC, 0x3200);
	max17047_write_verify_reg(max17047, MAX17047_REG_FULLCAP, pdata->capacity);
	regmap_write(max17047->regmap, MAX17047_REG_DESIGNCAP, pdata->vf_fullcap);
	max17047_write_verify_reg(max17047, MAX17047_REG_FULLCAPNOM, pdata->vf_fullcap);
	regmap_write(max17047->regmap, MAX17047_REG_SOCREP, vfsoc);

	/* Step 17. Initialization Complete
	 * Clear the POR bit to indicate that the custommodel and parameters were successfully loaded.
	 */
	regmap_read(max17047->regmap, MAX17047_REG_STATUS, &val);
	max17047_write_verify_reg(max17047, MAX17047_REG_STATUS, val & ~MAX17047_R_POR);

	/* write CONFIG register
	 */
	val =	MAX17047_R_BER |
		MAX17047_R_BEI |
		MAX17047_R_AEN |
		MAX17047_R_ETHRM |
		MAX17047_R_TEN;
	ret = regmap_write(max17047->regmap, MAX17047_REG_CONFIG, val);

	return ret;
}

#ifdef CONFIG_BATTERY_MAX17047_INTR
static irqreturn_t max17047_isr(int irq, void *irq_data)
{
	struct max17047 *max17047 = irq_data;
	unsigned int val;
	int ret;

	ret = regmap_read(max17047->regmap, MAX17047_REG_STATUS, &val);
	if (IS_ERR_VALUE(ret))
		dev_err(max17047->psy.dev, "cannot read STATUS\n");

	ret = regmap_write(max17047->regmap, MAX17047_REG_STATUS, 0x00);
	if (IS_ERR_VALUE(ret))
		dev_err(max17047->psy.dev, "cannot clear STATUS\n");

	if (val & MAX17047_R_POR)
	{
		dev_info(max17047->psy.dev, "detected POR\n");
		max17047_hw_init(max17047);
	}

	power_supply_changed(&max17047->psy);

#ifdef CONFIG_MAX17047_JEITA
	if (max17047->charger_pdata->enable_jeita)
	{
		ret = regmap_read(max17047->regmap, MAX17047_REG_TEMPERATURE, &val);
		if (likely(ret >= 0))
		{
			max17047_change_jeita_status(max17047, val >> 8);
		}
	}
#endif

	return IRQ_HANDLED;
}
#endif

static ssize_t max17047_show_learned_parameter(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max17047 *max17047 = dev_get_drvdata(dev);
	int ret;

	ret = max17047_save_learned_parameter(max17047);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "cannot read learned_parameter\n");
		return ret;
	}

	return sprintf(buf, "%s\n", max17047->learned_parameter);
}

static ssize_t max17047_set_learned_parameter(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct max17047 *max17047 = dev_get_drvdata(dev);
	int ret;

	strncpy(max17047->learned_parameter, buf, LEARNED_PARAM_LEN);
	ret = max17047_restore_learned_parameter(max17047);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "cannot write learned_parameter\n");
		return ret;
	}

	return count;
}

const static DEVICE_ATTR(learned_parameter, S_IRUGO | S_IWUSR,
		max17047_show_learned_parameter, max17047_set_learned_parameter);

const struct regmap_config max17047_regmap_config =
{
	.reg_bits = 8,
	.val_bits = 16,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static void max17047_fuelgauge_worker(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct max17047 *max17047 = container_of(dwork, struct max17047, check_work);
	int ret;
	unsigned int val;

	int voltage = -1;
	int average = -1;
	int capacity = -1;

	/* save learned parameters in driver data */
	ret = max17047_save_learned_parameter(max17047);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max17047->dev, "cannot read learned_parameter\n");
		return ret;
	}

	ret = regmap_read(max17047->regmap, MAX17047_REG_VCELL, &val);
	if (likely(ret >= 0))
	{
		/* unit = 0.625mV */
		voltage = (int)(val >> 3) * 625;
	}

	ret = regmap_read(max17047->regmap, MAX17047_REG_AVERAGEVCELL, &val);
	if (likely(ret >= 0))
	{
		/* unit = 0.625mV */
		average = (int)(val >> 3) * 625;
	}

	ret = max17047_get_capacity(max17047, &val);
	if (likely(ret >= 0))
	{
		capacity = val;
	}

	dev_info(max17047->dev, "check fuel-gauge %d mV, %d %%\n", voltage, capacity);
	power_supply_changed(&max17047->psy);
	schedule_delayed_work(&max17047->check_work, msecs_to_jiffies(MAX17047_WORK_DELAY));
}

#ifdef CONFIG_OF
static struct max17047_platform_data *max17047_parse_dt(struct device *dev)
{
	struct max17047_platform_data *pdata;
	struct device_node *np = dev->of_node;
	u32 val, arr[ARRAY_SIZE(pdata->custome_model)];
#ifdef CONFIG_MAX17047_JEITA
	char *tmp;
#endif
	int i;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	if (!of_property_read_u32(np, "capacity", &val)){
		pdata->capacity = (u16)val;
	}

	if (!of_property_read_u32(np, "vf-fullcap", &val)){
	pdata->vf_fullcap = (u16)val;
	}

	if (!of_property_read_u32(np, "qrtable00", &val)){
	pdata->qrtable00 = (u16)val;
	}

	if (!of_property_read_u32(np, "qrtable10", &val)){
	pdata->qrtable10 = (u16)val;
	}

	if (!of_property_read_u32(np, "qrtable20", &val)){
	pdata->qrtable20 = (u16)val;
	}

	if (!of_property_read_u32(np, "qrtable30", &val)){
	pdata->qrtable30 = (u16)val;
	}

	if (!of_property_read_u32(np, "vempty", &val)){
	pdata->vempty = (u16)val;
	}

	if (!of_property_read_u32(np, "fullsocthr", &val)){
	pdata->fullsocthr = (u16)val;
	}

	if (!of_property_read_u32(np, "rcomp0", &val)){
	pdata->rcomp0 = (u16)val;
	}

	if (!of_property_read_u32(np, "tempco", &val)){
	pdata->tempco = (u16)val;
	}

	if (!of_property_read_u32(np, "termcurr", &val)){
	pdata->termcurr = (u16)val;
	}

	if (!of_property_read_u32(np, "tempnom", &val)){
	pdata->tempnom = (u16)val;
	}

	if (!of_property_read_u32(np, "filtercfg", &val)){
	pdata->filtercfg = (u16)val;
	}

	if (!of_property_read_u32(np, "relaxcfg", &val)){
	pdata->relaxcfg = (u16)val;
	}

	if (!of_property_read_u32(np, "tgain", &val)){
	pdata->tgain = (u16)val;
	}

	if (!of_property_read_u32(np, "toff", &val)){
	pdata->toff = (u16)val;
	}

	if (!of_property_read_u32_array(np, "custome-model", arr, ARRAY_SIZE(arr)))
		return ERR_PTR(-EINVAL);
	for (i = 0; i < ARRAY_SIZE(arr); i++)
		pdata->custome_model[i] = (u16)arr[i];

#ifdef CONFIG_MAX17047_JEITA
	if (!of_property_read_string(np, "charger-name", &tmp))
		return ERR_PTR(-EINVAL);
	pdata->charger_name = devm_kzalloc(dev, sizeof(strlen(tmp)), GFP_KERNEL)
		strcpy(pdata->charger_name, tmp);
#endif
	if (!of_property_read_u32(np, "r-sns", &val)){
		pdata->r_sns = (u16)val;
		pdata->current_sensing = of_property_read_bool(np, "current-sensing");
	}
	return pdata;
}
#endif

static int max17047_suspend(struct device *dev)
{
	struct max17047 *max17047 = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "max17047 driver goes into suspend state...\n");
#if 0
	/* save learned parameters in driver data */
	ret = max17047_save_learned_parameter(max17047);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "cannot read learned_parameter\n");
		return ret;
	}
#endif
	return 0;
}

static int max17047_resume(struct device *dev)
{
	struct max17047 *max17047 = dev_get_drvdata(dev);
    unsigned int val;
	int ret;

	/* read status register to check PowerOnReset */
	ret = regmap_read(max17047->regmap, MAX17047_REG_STATUS, &val);
	if (IS_ERR_VALUE(ret))
		dev_err(max17047->psy.dev, "cannot read STATUS\n");

	ret = regmap_write(max17047->regmap, MAX17047_REG_STATUS, 0x00);
	if (IS_ERR_VALUE(ret))
		dev_err(max17047->psy.dev, "cannot clear STATUS\n");

	if (val & MAX17047_R_POR)
	{
		dev_info(max17047->psy.dev, "detected POR\n");
		max17047_hw_init(max17047);

		/* restore learned parameter from drvdata to register */
		ret = max17047_restore_learned_parameter(max17047);
		if (IS_ERR_VALUE(ret))
		{
			dev_err(dev, "cannot write learned_parameter, ret is %d\n", ret);
		}
	}

	dev_dbg(dev, "max17047 driver is resumed successful...\n");

	power_supply_changed(&max17047->psy);
	return 0;
}

static int __devinit max17047_ipc_probe(struct platform_device *pdev)
{
	struct ipc_client *regmap_ipc;
	struct device *dev = &pdev->dev;
	struct max17047_platform_data *pdata;
	struct max17047 *max17047;
	int ret;

	max17047 = devm_kzalloc(dev, sizeof(*max17047), GFP_KERNEL);
	if (!max17047)
		return -ENOMEM;

#ifdef CONFIG_OF
	pdata = max17047_parse_dt(dev);
	if (unlikely(IS_ERR(pdata)))
		return PTR_ERR(pdata);
#if !defined(CONFIG_MACH_ODIN_HDK)
	pdev->irq = pdata->irq;
#endif
#else
	pdata = dev_get_platdata(dev);
#endif

	max17047->dev = dev;
	max17047->irq = gpio_to_irq(116);

	max17047->r_sns = pdata->r_sns;
	max17047->current_sensing = pdata->current_sensing;
	max17047->pdata = pdata;

	max17047->psy.name		= MAX17047_NAME;
	max17047->psy.type		= POWER_SUPPLY_TYPE_BATTERY;
	max17047->psy.get_property	= max17047_get_property;
	max17047->psy.set_charged	= max17047_set_charged;

	if (max17047->r_sns > 0)
	{
		max17047->psy.properties	= max17047_props;
		max17047->psy.num_properties	= ARRAY_SIZE(max17047_props);
	}
	else
	{
		max17047->psy.properties	= max17047_props_no_rsns;
		max17047->psy.num_properties	= ARRAY_SIZE(max17047_props_no_rsns);
	}

#ifdef CONFIG_MAX17047_JEITA
	max17047->jeita_status = MAX17047_NORMAL;
	max17047->charger = power_supply_get_by_name(pdata->charger_name);
	max17047->charger_pdata = dev_get_drvdata(max17047->charger->dev->parent);
#endif

	regmap_ipc = kzalloc(sizeof(struct ipc_client), GFP_KERNEL);
	if(regmap_ipc == NULL){
		dev_err(dev, "Platform data not specified.\n");
		return -ENOMEM;
	}

	regmap_ipc->slot = MAX17047_I2C_SLOT;
	regmap_ipc->addr = MAX17047_SLAVE_ADDRESS;
	regmap_ipc->dev = pdev->dev;

	max17047->regmap = devm_regmap_init_ipc(regmap_ipc, &max17047_regmap_config);
	if (IS_ERR(max17047->regmap))
		return PTR_ERR(max17047->regmap);

	dev_set_drvdata(dev, max17047);

	ret = max17047_hw_init(max17047);
	if (IS_ERR_VALUE(ret))
		return ret;

	(void)max17047_save_learned_parameter(max17047);

	/* This driver exports 'learned_parameter' attribute.
	 * User mode deam is responsible for writing this value to nonvolatile memory every time
	 * and restoring it on power-up events.
	 */
	ret = device_create_file(max17047->psy.dev, &dev_attr_learned_parameter);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max17047->psy.dev, "failed: cannot create device attribute.\n");
		goto err_unregister;
	}

	ret = power_supply_register(dev, &max17047->psy);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "failed: power supply register\n");
		return ret;
	}

#ifdef CONFIG_BATTERY_MAX17047_INTR
	ret = devm_request_threaded_irq(dev, max17047->irq, NULL,
			max17047_isr, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			MAX17047_NAME, max17047);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "failed to reqeust IRQ\n");
		return ret;
	}
#endif

#ifdef CONFIG_MAX17047_JEITA
	if (max17047->charger_pdata->enable_jeita)
		max17047_change_jeita_status(max17047,
				(max17047->charger_pdata->t2 +
				max17047->charger_pdata->t3) / 2);
#endif

#ifdef CONFIG_OF
	devm_kfree(dev, pdata);
#endif

	INIT_DELAYED_WORK(&max17047->check_work, max17047_fuelgauge_worker);
	schedule_delayed_work(&max17047->check_work, msecs_to_jiffies(MAX17047_WORK_DELAY));
	dev_info(dev, "%s done\n", __func__);
	return 0;

err_unregister:
	power_supply_unregister(&max17047->psy);
	return ret;
}

static int __devexit max17047_remove(struct platform_device *pdev)
{
	struct max17047 *max17047 = platform_get_drvdata(pdev);

#ifdef CONFIG_BATTERY_MAX17047_INTR
	devm_free_irq(&pdev->dev, max17047->irq, max17047);
#endif
	device_remove_file(max17047->psy.dev, &dev_attr_learned_parameter);
	power_supply_unregister(&max17047->psy);
	return 0;
}

static const struct dev_pm_ops max17047_pm_ops = {
	.suspend	= max17047_suspend,
	.resume		= max17047_resume,
};

#ifdef CONFIG_OF
static struct of_device_id max17047_of_match[] = {
        { .compatible = "maxim,max77665_fg_ipc" },
        {},
};
MODULE_DEVICE_TABLE(of, max17047_of_match);
#endif

static struct platform_driver max17047_driver = {
	.driver	= {
		.name	= MAX17047_NAME,
		.owner = THIS_MODULE,
		.pm	= &max17047_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = max17047_of_match,
#endif
	},
	.probe		= max17047_ipc_probe,
	.remove		= __devexit_p(max17047_remove),
};

static int __init max17047_ipc_init(void)
{
	return platform_driver_register(&max17047_driver);
}
module_init(max17047_ipc_init);

static void __exit max17047_ipc_exit(void)
{
	platform_driver_unregister(&max17047_driver);
}
module_exit(max17047_ipc_exit);

MODULE_AUTHOR("Gyungoh Yoo<jack.yoo@maxim-ic.com>");
MODULE_DESCRIPTION("MAX17047 Fuel Gauge");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.2");
