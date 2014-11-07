/*
 * Battery driver for Maxim MAX77665
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/mfd/max77665.h>
#include <linux/odin_pmic.h>
#include <linux/power/max77665-charger.h>

#include <asm/io.h>

/* Register */
#define MAX77665_CHG_INT		0xB0
#define MAX77665_CHG_INT_MASK		0xB1
#define MAX77665_CHG_INT_OK		0xB2
#define MAX77665_CHG_DETAILS_00		0xB3
#define MAX77665_CHG_DETAILS_01		0xB4
#define MAX77665_CHG_DETAILS_02		0xB5
#define MAX77665_CHG_DETAILS_03		0xB6
#define MAX77665_CHG_CNFG_00		0xB7
#define MAX77665_CHG_CNFG_01		0xB8
#define MAX77665_CHG_CNFG_02		0xB9
#define MAX77665_CHG_CNFG_03		0xBA
#define MAX77665_CHG_CNFG_04		0xBB
#define MAX77665_CHG_CNFG_05		0xBC
#define MAX77665_CHG_CNFG_06		0xBD
#define MAX77665_CHG_CNFG_07		0xBE
#define MAX77665_CHG_CNFG_08		0xBF
#define MAX77665_CHG_CNFG_09		0xC0
#define MAX77665_CHG_CNFG_10		0xC1
#define MAX77665_CHG_CNFG_11		0xC2
#define MAX77665_CHG_CNFG_12		0xC3
#define MAX77665_CHG_CNFG_13		0xC4
#define MAX77665_CHG_CNFG_14		0xC5
#define MAX77665_SAFEOUT_LDO		0xC6

/* MAX77665_CHG_INT */
#define MAX77665_BYP_I			(1 << 0)
#define MAX77665_BAT_I			(1 << 3)
#define MAX77665_CHG_I			(1 << 4)
#define MAX77665_CHGIN_I		(1 << 6)

/* MAX77665_CHG_INT_MASK */
#define MAX77665_BYP_IM			(1 << 0)
#define MAX77665_BAT_IM			(1 << 3)
#define MAX77665_CHG_IM			(1 << 4)
#define MAX77665_CHGIN_IM		(1 << 6)

/* MAX77665_CHG_INT_OK */
#define MAX77665_BYP_OK			(1 << 0)
#define MAX77665_BAT_OK			(1 << 3)
#define MAX77665_CHG_OK			(1 << 4)
#define MAX77665_CHGIN_OK		(1 << 6)

/* MAX77665_CHG_DETAILS_00 */
#define MAX77665_CHGIN_DTLS		0x60
#define MAX77665_THM_DTLS		0x07

/* MAX77665_CHG_DETAILS_01 */
#define MAX77665_CHG_DTLS		0x0F
#define MAX77665_BAT_DTLS		0x70

/* MAX77665_CHG_CNFG_00 */
#define MAX77665_MODE			(0x0F << 0)
#define MAX77665_WDTEN			(1 << 4)
#define MAX77665_DIS_MUIC_CTRL		(1 << 5)
#define MAX77665_DISIBS			(1 << 6)

/* MAX77665_CHG_CNFG_01 */
#define MAX77665_FCHGTIME		(7 << 0)
#define MAX77665_CHG_RSTRT		(3 << 4)
#define MAX77665_PQEN			(1 << 7)

/* MAX77665_CHG_CNFG_02 */
#define MAX77665_CHG_CC			(0x3F << 0)
#define MAX77665_OTG_ILIM		(1 << 7)

/* MAX77665_CHG_CNFG_06 */
#define MAX77665_WDTCLR			(3 << 0)
#define MAX77665_CHGPROT		(3 << 2)

/* MAX77665_CHG_CNFG_09 */
#define MAX77665_CHGIN_ILIM		(0x7F << 0)

#define MAX77665_DELAY 1000

struct max77665_charger
{
	struct power_supply ac;
	struct power_supply usb;
	struct max77665 *max77665;
	enum power_supply_type type;
	int fast_charge_current_ac;		/* 0mA ~ 2100mA */
	int fast_charge_current_usb;		/* 0mA ~ 2100mA */
	bool online;
	int irq;
#ifdef CONFIG_MAX77665_JEITA
	struct max77665_charger_pdata *pdata;
#endif
};

static enum power_supply_property max77665_charger_props[] =
{
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

#ifdef CONFIG_USB_ODIN_DRD
static struct max77665_charger *g_charger;
static u8 save_int;

/* MAX77803 CHG_CNFG_00 register */
#define CHG_CNFG_00_MODE_SHIFT		0
#define CHG_CNFG_00_CHG_SHIFT		0
#define CHG_CNFG_00_OTG_SHIFT		1
#define CHG_CNFG_00_BUCK_SHIFT		2
#define CHG_CNFG_00_BOOST_SHIFT		3
#define CHG_CNFG_00_DIS_MUIC_CTRL_SHIFT	5
#define CHG_CNFG_00_MODE_MASK		(0xf << CHG_CNFG_00_MODE_SHIFT)
#define CHG_CNFG_00_CHG_MASK		(1 << CHG_CNFG_00_CHG_SHIFT)
#define CHG_CNFG_00_OTG_MASK		(1 << CHG_CNFG_00_OTG_SHIFT)
#define CHG_CNFG_00_BUCK_MASK		(1 << CHG_CNFG_00_BUCK_SHIFT)
#define CHG_CNFG_00_BOOST_MASK		(1 << CHG_CNFG_00_BOOST_SHIFT)
#define CHG_CNFG_00_DIS_MUIC_CTRL_MASK	(1 << CHG_CNFG_00_DIS_MUIC_CTRL_SHIFT)

void max77665_charger_interrupt_onoff(int onoff)
{
	struct max77665_charger *max77665_charger = g_charger;
	struct regmap *regmap = max77665_charger->max77665->regmap;
	int ret;
	unsigned int int_mask;

	ret = regmap_read(regmap, MAX77665_CHG_INT_MASK, &int_mask);
	if (IS_ERR_VALUE(ret))
		dev_err(max77665_charger->ac.dev ,
					"Failed to read MAX77665_CHG_INT_MASK: %d\n", ret);

	if (onoff) {
		save_int = int_mask;

		int_mask |= (1 << 4);	/* disable chgin intr */
		int_mask |= (1 << 6);	/* disable chg */
		int_mask &= ~(1 << 0);	/* enable byp intr */
	} else {
		int_mask = save_int;
	}

	ret = regmap_write(regmap, MAX77665_CHG_INT_MASK, int_mask);
	if (IS_ERR_VALUE(ret))
		dev_err(max77665_charger->ac.dev ,
					"Failed to set MAX77665_CHG_INT_MASK: %d\n", ret);

}

void max77665_charger_otg_onoff(int onoff)
{
	struct max77665_charger *max77665_charger = g_charger;
	struct regmap *regmap = max77665_charger->max77665->regmap;
	int ret;
	unsigned int cnfg00;

	ret = regmap_read(regmap, MAX77665_CHG_CNFG_00, &cnfg00);
	if (IS_ERR_VALUE(ret))
		dev_err(max77665_charger->ac.dev ,
					"Failed to read MAX77665_CHG_CNFG_00: %d\n", ret);

	if (onoff) {
		cnfg00 &= ~(CHG_CNFG_00_CHG_MASK
				| CHG_CNFG_00_OTG_MASK
				| CHG_CNFG_00_BUCK_MASK
				| CHG_CNFG_00_BOOST_MASK
				| CHG_CNFG_00_DIS_MUIC_CTRL_MASK);
		cnfg00 |= (CHG_CNFG_00_OTG_MASK
				| CHG_CNFG_00_BOOST_MASK
				| CHG_CNFG_00_DIS_MUIC_CTRL_MASK);
	} else {
		cnfg00 &= ~(CHG_CNFG_00_OTG_MASK
				| CHG_CNFG_00_BOOST_MASK
				| CHG_CNFG_00_DIS_MUIC_CTRL_MASK);
		cnfg00 |= CHG_CNFG_00_BUCK_MASK;
	}

	ret = regmap_write(regmap, MAX77665_CHG_CNFG_00, cnfg00);
	if (IS_ERR_VALUE(ret))
		dev_err(max77665_charger->ac.dev ,
					"Failed to set MAX77665_CHG_CNFG_00: %d\n", ret);
}

void max77665_charger_vbypset(u8 voltage)
{
	struct max77665_charger *max77665_charger = g_charger;
	struct regmap *regmap = max77665_charger->max77665->regmap;
	int ret;

	ret = regmap_write(regmap, MAX77665_CHG_CNFG_11, voltage);
	if (IS_ERR_VALUE(ret))
		dev_err(max77665_charger->ac.dev ,
					"Failed to set MAX77665_CHG_CNFG_11: %d\n", ret);
}
#endif

static int max77665_charger_set_current_limit(struct max77665_charger
									*max77665_charger, int mA)
{
	struct regmap *regmap = max77665_charger->max77665->regmap;
	unsigned int val = ((mA / 20000) << M2SH(MAX77665_CHGIN_ILIM));

	return regmap_update_bits(regmap, MAX77665_CHG_CNFG_09,
										MAX77665_CHGIN_ILIM, val);
}

static int max77665_charger_enable(struct max77665_charger *max77665_charger,
														bool on)
{
	unsigned int val;
	int ret;

	if (on) /* charger=on, OTG=off, buck=on, boost=off */
		val = 0x05 << M2SH(MAX77665_MODE);
	else  /* charger=off, OTG=off, buck=on, boost=off */
		val = 0x04 << M2SH(MAX77665_MODE);

	ret = regmap_update_bits(max77665_charger->max77665->regmap,
			MAX77665_CHG_CNFG_00, MAX77665_MODE, 0x05);
	if (IS_ERR_VALUE(ret))
		dev_err(max77665_charger->ac.dev ,
					"Failed to set MAX77665_CHG_CNFG_00: %d\n", ret);

	return ret;
}

static irqreturn_t max77665_charger_isr(int irq, void *dev_id)
{
	struct max77665_charger *max77665_charger = (struct max77665_charger *)dev_id;
	struct regmap *regmap = max77665_charger->max77665->regmap;
	unsigned int val, val2;
	int ret;

	ret = regmap_read(regmap, MAX77665_CHG_INT, &val);
	if (IS_ERR_VALUE(ret))
		dev_err(max77665_charger->ac.dev ,
					"Failed to read MAX77665_CHG_INT: %d\n", ret);

	if (likely(val & (MAX77665_CHGIN_I | MAX77665_CHG_I)))
	{
		ret = regmap_read(regmap, MAX77665_CHG_INT_OK, &val);
		if (IS_ERR_VALUE(ret))
			dev_err(max77665_charger->ac.dev ,
						"Failed to read MAX77665_CHG_INT: %d\n", ret);

		ret = regmap_read(regmap, MAX77665_CHG_DETAILS_01, &val2);
		if (IS_ERR_VALUE(ret))
			dev_err(max77665_charger->ac.dev ,
						"Failed to read MAX77665_CHG_INT: %d\n", ret);

		if (((val & (MAX77665_CHGIN_OK | MAX77665_CHG_OK))
				== (MAX77665_CHGIN_OK | MAX77665_CHG_OK))
				&& ((val2 & MAX77665_CHG_DTLS) != 0x08))
		{
			max77665_charger->online = true;
			max77665_charger->type = POWER_SUPPLY_TYPE_USB;
			power_supply_changed(&max77665_charger->usb);
		}
		else
		{
			max77665_charger->online = false;

			if (max77665_charger->type == POWER_SUPPLY_TYPE_MAINS)
				power_supply_changed(&max77665_charger->ac);
			else
				power_supply_changed(&max77665_charger->usb);
		}
#ifdef CONFIG_MAX77665_JEITA
		max77665_change_jeita_status(&max77665_charger->charger, false, false);
		max77665_charger_enable(&max77665_charger->charger, true);
#endif
	}

	return IRQ_HANDLED;
}

static int max77665_charger_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct max77665_charger *max77665_charger;
	struct regmap *regmap;
	int ret = -EINVAL;
	unsigned int value;

	max77665_charger = (psy->type == POWER_SUPPLY_TYPE_MAINS) ?
			container_of(psy, struct max77665_charger, ac) :
			container_of(psy, struct max77665_charger, usb);
	regmap = max77665_charger->max77665->regmap;

	switch (psp)
	{
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == max77665_charger->type && max77665_charger->online)
			val->intval = 1;
		else
			val->intval = 0;
		ret = 0;
		break;

	case POWER_SUPPLY_PROP_STATUS:
		if (psy->type == max77665_charger->type) {
			ret = regmap_read(regmap, MAX77665_CHG_DETAILS_01, &value);
			if (IS_ERR_VALUE(ret))
				dev_err(max77665_charger->ac.dev ,
								"MAX77665 charger cannot read CHG_DETAILS_01\n");
			else {
				switch ((value & MAX77665_CHG_DTLS)) {
					case 0x0:
					case 0x1:
					case 0x2:
					case 0x3:
					case 0x5:
						val->intval = POWER_SUPPLY_STATUS_CHARGING;
						break;
					case 0x4:
						val->intval = POWER_SUPPLY_STATUS_FULL;
						break;
					case 0x6:
					case 0x7:
					case 0x8:
					case 0xA:
					case 0xB:
						val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
						break;
					default:
						val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
				}
			}
		}
		else
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (psy->type == max77665_charger->type)
		{
			/* Get charger status from CHG_DETAILS_01 */
			ret = regmap_read(regmap, MAX77665_CHG_DETAILS_01, &value);
			if (IS_ERR_VALUE(ret))
				dev_err(max77665_charger->ac.dev ,
								"MAX77665 charger cannot read CHG_DETAILS_01\n");
			else
			{
				switch ((value & MAX77665_CHG_DTLS))
				{
				case 0x0:
					val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
					break;
				case 0x1:
				case 0x2:
				case 0x3:
					val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
					break;
				case 0x4:
				case 0x8:
				case 0xA:
				case 0xB:
					val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
					break;
				default:
					val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
				}
			}
		}
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;

	default:
		val->intval = 0;
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int max77665_charger_set_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct max77665_charger *max77665_charger;
	struct regmap *regmap;
	int ret = -EINVAL;

	max77665_charger = (psy->type == POWER_SUPPLY_TYPE_MAINS) ?
			container_of(psy, struct max77665_charger, ac) :
			container_of(psy, struct max77665_charger, usb);
	regmap = max77665_charger->max77665->regmap;

	switch (psp)
	{
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == max77665_charger->type)
		{
			if (val->intval != (max77665_charger->online ? 1 : 0))
			{
				max77665_charger->online = val->intval ? true : false;

				ret = max77665_charger_enable(max77665_charger, max77665_charger->online);
				if (IS_ERR_VALUE(ret))
					dev_err(psy->dev, "Failed to call max77665_charger_enable: %d\n", ret);

				power_supply_changed(psy);
			}
		}
		else
		{
			if (val->intval == 1)
			{
				ret = max77665_charger_set_current_limit(max77665_charger,
						(psy->type == POWER_SUPPLY_TYPE_MAINS) ?
							max77665_charger->fast_charge_current_ac :
							max77665_charger->fast_charge_current_usb);
				if (IS_ERR_VALUE(ret))
					dev_err(psy->dev ,
							"Failed to call max77665_charger_set_current_limit: %d\n", ret);

				if (max77665_charger->online == false)
				{
					max77665_charger->online = true;
					ret = max77665_charger_enable(max77665_charger, true);
					if (IS_ERR_VALUE(ret))
						dev_err(psy->dev, "Failed to call max77665_charger_enable: %d\n", ret);
				}

				max77665_charger->type = psy->type;
				power_supply_changed(&max77665_charger->usb);
				power_supply_changed(&max77665_charger->ac);
			}
		}
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (psy->type == max77665_charger->type)
		{
			if (val->intval == 0)
				ret = max77665_charger_set_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
			else if (val->intval >= 60000 && val->intval <= 2540000)
			{
				ret = max77665_charger_set_current_limit(max77665_charger, val->intval);
				if (likely(ret >= 0))
				{
					if (psy->type == POWER_SUPPLY_TYPE_MAINS)
						max77665_charger->fast_charge_current_ac = val->intval;
					else
						max77665_charger->fast_charge_current_usb = val->intval;
				}
			}
		}
		break;

	default:
		val->intval = 0;
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int max77665_charger_property_is_writeable(struct power_supply *psy,
									enum power_supply_property psp)
{
	switch (psp) {
		case POWER_SUPPLY_PROP_CURRENT_MAX:
		case POWER_SUPPLY_PROP_ONLINE:
			return 1;

		default:
			return -EINVAL;
		}
}

#ifdef CONFIG_MAX77665_JEITA
void max77665_change_jeita_status(struct power_supply *charger,
								bool reduce_voltage, bool half_current)
{
	struct max77665_charger *max77665_charger = container_of(charger,
								struct max77665_charger, charger);
	struct max77665_charger_pdata *pdata = max77665_charger->pdata;
	int fast_charge_current;
	int ret;
	u8 val;

	val = ((max77665_charger ? pdata->charger_termination_voltage_jeita :
				pdata->charger_termination_voltage_jeita) << 0);
	ret = regmap_write(regmap, MAX77665_CHG_CNFG_04, val);
	if (IS_ERR_VALUE(ret))
		dev_err(max77665_charger->charger->dev,
						"Failed to set MAX77665_CHG_CNFG_04: %d\n", ret);

	fast_charge_current = pdata->fast_charge_current;
	if (half_current)
		fast_charge_current /= 2;
	val = ((fast_charge_current * 3 / 100) << 0);
	ret = regmap_write(regmap, MAX77665_CHG_CNFG_02, val);
	if (IS_ERR_VALUE(ret))
		dev_err(max77665_charger->charger->dev,
						"Failed to set MAX77665_CHG_CNFG_02: %d\n", ret);
}
EXPORT_SYMBOL(max77665_change_jeita_status);
#endif

static int max77665_charger_hw_init(struct max77665_charger *max77665_charger,
		struct device *dev, struct max77665_charger_pdata *pdata)
{
	struct regmap *regmap = max77665_charger->max77665->regmap;
	unsigned int val;
	int ret;
	int revision_data = odin_pmic_revision_get();

	val = MAX77665_BYP_IM | MAX77665_BAT_IM;
	ret = regmap_write(regmap, MAX77665_CHG_INT_MASK, val);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "Failed to set CHG_INT_MASK: %d\n", ret);
		goto out;
	}

	/* Changed USB Out voltage from 4.9V to 3.3V if revision is less than D */
	if (revision_data == MADK_V1_1_1) {
		ret = regmap_read(regmap, MAX77665_SAFEOUT_LDO, &val);
		val = val | 0x03;
		ret = regmap_write(regmap, MAX77665_SAFEOUT_LDO, val);
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "Failed to set LDO1 Out to 3.3V: %d\n", ret);
	}

	/* Unlock protected registers */
	ret = regmap_write(regmap, MAX77665_CHG_CNFG_06, MAX77665_CHGPROT);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "Failed to set MAX8957_REG_CHG_CNFG_06: %d\n", ret);
		goto out;
	}

	val = (pdata->fast_charge_timer << M2SH(MAX77665_FCHGTIME)) |
		(pdata->charging_restart_thresold << M2SH(MAX77665_CHG_RSTRT)) |
		MAX77665_PQEN;
	ret = regmap_write(regmap, MAX77665_CHG_CNFG_01, val);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "Failed to set MAX77665_CHG_CNFG_01: %d\n", ret);
		goto out;
	}

	if (unlikely(pdata->fast_charge_current_usb < 0 ||
				pdata->fast_charge_current_usb > 2100 ||
				pdata->fast_charge_current_ac < 0 ||
				pdata->fast_charge_current_ac > 2100)) {
		dev_err(dev, "The fast charging current is invalid: %dmA\n",
												pdata->fast_charge_current_usb);
		goto out;
	}
	val = ((pdata->fast_charge_current_usb * 3 / 100) << M2SH(MAX77665_CHG_CC));
	ret = regmap_write(regmap, MAX77665_CHG_CNFG_02, val);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "Failed to set MAX77665_CHG_CNFG_02: %d\n", ret);
		goto out;
	}

	val = (pdata->top_off_current_thresold << 0) | (pdata->top_off_timer << 3);
	ret = regmap_write(regmap, MAX77665_CHG_CNFG_03, val);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "Failed to set MAX77665_CHG_CNFG_03: %d\n", ret);
		goto out;
	}

	val = (pdata->charger_termination_voltage << 0);
	ret = regmap_write(regmap, MAX77665_CHG_CNFG_04, val);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "Failed to set MAX77665_CHG_CNFG_04: %d\n", ret);
		goto out;
	}

out:
	return ret;
}

#ifdef CONFIG_OF
static struct max77665_charger_pdata *max77665_charger_parse_dt(
										struct device *dev)
{
	struct device_node *nproot = dev->parent->of_node;
	struct max77665_charger_pdata *pdata;
	struct device_node *regulators_np, *np;
	u32 val;

	if (unlikely(nproot == NULL))
		return ERR_PTR(-EINVAL);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	np = of_find_node_by_name(nproot, "charger");
	if (unlikely(np == NULL)) {
		dev_err(dev, "charger node not found\n");
		return ERR_PTR(-EINVAL);
	}

#if !defined(CONFIG_MACH_ODIN_HDK)
	pdata->irq = irq_of_parse_and_map(dev->of_node, 0);
	if (pdata->irq == 0)
		return ERR_PTR(-EINVAL);
#endif

	if (!of_property_read_u32(np, "fast-charging-current-ac",
												&pdata->fast_charge_current_ac))
		pdata->fast_charge_current_ac = 1000000;

	if (!of_property_read_u32(np, "fast-charging-current-usb",
												&pdata->fast_charge_current_ac))
		pdata->fast_charge_current_ac = 500000;

	if (!of_property_read_u32(np, "fast-charging-time", &val))
		pdata->fast_charge_timer = 4;

	switch (val) {
		case 0:
			pdata->fast_charge_timer = MAX77665_FCHGTIME_DISABLE;
			break;
		case 4:
		case 6:
		case 8:
		case 10:
		case 12:
		case 14:
		case 16:
			pdata->fast_charge_timer = MAX77665_FCHGTIME_4H + (val - 4) / 2;
			break;
		default:
			return ERR_PTR(-EINVAL);
		}

	if (!of_property_read_u32(np, "charging-restart-thresold", &val))
		pdata->fast_charge_timer = 150;

	switch (val) {
		case 0:
			pdata->charging_restart_thresold = MAX77665_CHG_RSTRT_DISABLE;
			break;
		case 100:
		case 150:
		case 200:
			pdata->charging_restart_thresold =
							MAX77665_CHG_RSTRT_100MV + (val - 100) / 50;
			break;
		default:
			return ERR_PTR(-EINVAL);
		}

	if (!of_property_read_u32(np, "topoff-time", &val))
		pdata->top_off_timer = MAX77665_CHG_TO_TIME_30MIN;
	else
		pdata->top_off_timer = MAX77665_CHG_TO_TIME_0MIN + val / 10;

	if (!of_property_read_u32(np, "topoff-current-threshold", &val))
		pdata->top_off_current_thresold = MAX77665_CHG_TO_ITH_150MA;
	else
		switch (val) {
			case 100:
			case 125:
			case 150:
			case 175:
			case 200:
				pdata->top_off_current_thresold =
						MAX77665_CHG_TO_ITH_100MA + (val - 100) / 25;
				break;
			case 250:
			case 300:
			case 350:
				pdata->top_off_current_thresold =
						MAX77665_CHG_TO_ITH_250MA + (val - 250) / 50;
				break;
			default:
				return ERR_PTR(-EINVAL);
			}


	if (!of_property_read_u32(np, "charging-termination-voltage", &val))
		pdata->charger_termination_voltage = MAX77665_CHG_CV_PRM_4200MV;
	else
		pdata->charger_termination_voltage =
					MAX77665_CHG_CV_PRM_3650MV + (val - 3650) / 25;

	return pdata;
}
#endif

static __init int max77665_charger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77665_charger_pdata *pdata;
	struct max77665_charger *max77665_charger;
	struct max77665 *max77665 = dev_get_drvdata(dev->parent);
	int ret;

	max77665_charger = devm_kzalloc(dev,
							sizeof(struct max77665_charger), GFP_KERNEL);
	if (unlikely(!max77665_charger))
		return -ENOMEM;

#ifdef CONFIG_OF
	pdata = max77665_charger_parse_dt(dev);
	if (unlikely(IS_ERR(pdata)))
		return PTR_ERR(pdata);
#if defined(CONFIG_MACH_ODIN_HDK)
	max77665_charger->irq =
			regmap_irq_chip_get_base(max77665->irq_data) + MAX77665_INT_CHGR;
#endif
#else
	pdata = dev_get_platdata(dev);
	max77665_charger->irq = platform_get_irq(pdev, 0);
#endif

	platform_set_drvdata(pdev, max77665_charger);
	max77665_charger->max77665 = max77665;
	max77665_charger->type = POWER_SUPPLY_TYPE_USB;
	max77665_charger->fast_charge_current_ac = pdata->fast_charge_current_ac;
	max77665_charger->fast_charge_current_usb = pdata->fast_charge_current_usb;
	max77665_charger->online = 0;
#ifdef CONFIG_MAX77665_JEITA
	max77665_charger->pdata = pdata;
#endif

	ret = max77665_charger_hw_init(max77665_charger, dev, pdata);
	if (IS_ERR_VALUE(ret))
		return ret;

	max77665_charger->ac.name = "ac";
	max77665_charger->ac.type = POWER_SUPPLY_TYPE_MAINS;
	max77665_charger->ac.properties = max77665_charger_props;
	max77665_charger->ac.num_properties = ARRAY_SIZE(max77665_charger_props);
	max77665_charger->ac.get_property = max77665_charger_get_property;
	max77665_charger->ac.set_property = max77665_charger_set_property;
	max77665_charger->ac.property_is_writeable =
							max77665_charger_property_is_writeable;
	ret = power_supply_register(&pdev->dev, &max77665_charger->ac);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "Failed to register max77665_charger driver: %d\n", ret);
		goto out;
	}

	max77665_charger->usb.name = "usb";
	max77665_charger->usb.type = POWER_SUPPLY_TYPE_USB;
	max77665_charger->usb.properties = max77665_charger_props;
	max77665_charger->usb.num_properties = ARRAY_SIZE(max77665_charger_props);
	max77665_charger->usb.get_property = max77665_charger_get_property;
	max77665_charger->usb.set_property = max77665_charger_set_property;
	max77665_charger->usb.property_is_writeable =
							max77665_charger_property_is_writeable;
	ret = power_supply_register(&pdev->dev, &max77665_charger->usb);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "Failed to register max77665_charger driver: %d\n", ret);
		goto out1;
	}

	ret = devm_request_threaded_irq(dev, max77665_charger->irq, NULL,
				max77665_charger_isr, IRQF_ONESHOT,
				"max77665-charger", max77665_charger);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "max77665: failed to request IRQ	%X\n", ret);
		goto out2;
	}
#ifdef CONFIG_USB_ODIN_DRD
	g_charger = max77665_charger;
#endif

#ifdef CONFIG_OF
	devm_kfree(dev, pdata);
#endif

	return 0;
out2:
	power_supply_unregister(&max77665_charger->usb);
out1:
	power_supply_unregister(&max77665_charger->ac);
out:
	return ret;
}

static int max77665_charger_remove(struct platform_device *pdev)
{
	struct device *dev= &pdev->dev;
	struct max77665_charger *max77665_charger = platform_get_drvdata(pdev);
	int ret;
	unsigned int val;

	val = MAX77665_BYP_IM | MAX77665_BAT_IM | MAX77665_CHG_IM | MAX77665_CHGIN_IM;
	ret = regmap_write(max77665_charger->max77665->regmap,
							MAX77665_CHG_INT_MASK, val);
	if (unlikely(ret !=0)) {
		dev_err(dev, "Failed to set IRQ1_MASK: %d\n", ret);
		goto out;
	}

	devm_free_irq(dev, max77665_charger->irq, pdev);
	power_supply_unregister(&max77665_charger->usb);
	power_supply_unregister(&max77665_charger->ac);
out:
	return 0;
}

static struct platform_driver max77665_charger_driver =
{
	.driver.name = "max77665-charger",
	.probe	= max77665_charger_probe,
	.remove	= max77665_charger_remove,
};

static int __init max77665_charger_init(void)
{
	return platform_driver_register(&max77665_charger_driver);
}
module_init(max77665_charger_init);

static void __exit max77665_charger_exit(void)
{
	platform_driver_unregister(&max77665_charger_driver);
}
module_exit(max77665_charger_exit);

MODULE_DESCRIPTION("Charger driver for MAX77665");
MODULE_AUTHOR("Gyungoh Yoo<jack.yoo@maxim-ic.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.2");
