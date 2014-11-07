/*
 * max77665-muic.c - MUIC driver for the Maxim 77665
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/input.h>
#include <linux/switch.h>
#include <linux/power_supply.h>
#include <linux/mfd/max77665.h>
#include <linux/max77665-muic.h>
#include <asm/gpio.h>
#include <linux/io.h>

/* Registers */
#define MAX77665_ID			0x00
#define MAX77665_INT1			0X01
#define MAX77665_INT2			0x02
#define MAX77665_INT3			0x03
#define MAX77665_STATUS1		0x04
#define MAX77665_STATUS2		0x05
#define MAX77665_STATUS3		0x06
#define MAX77665_INTMASK1		0x07
#define MAX77665_INTMASK2		0x08
#define MAX77665_INTMASK3		0x09
#define MAX77665_CDETCTRL1		0x0A
#define MAX77665_CDETCTRL2		0x0B
#define MAX77665_CONTROL1		0x0C
#define MAX77665_CONTROL2		0x0D
#define MAX77665_CONTROL3		0x0E

/* MAX77665_INT1 */
#define MAX77665_INT_ADC		0x01
#define MAX77665_INT_ADCLOW		0x02
#define MAX77665_INT_ADCERROR		0x04
#define MAX77665_INT_ADC1K		0x08

/* MAX77665_INT2 */
#define MAX77665_INT_CHGYPE		0x01
#define MAX77665_INT_CHGDETRUN		0x02
#define MAX77665_INT_DCDTMR		0x04
#define MAX77665_INT_DXOVP		0x08
#define MAX77665_INT_VBVOLT		0x10
#define MAX77665_INT_VIDRM		0x20

/* MAX77665_INT3 */
#define MAX77665_INT_EOC		0x01
#define MAX77665_INT_CGMBC		0x02
#define MAX77665_INT_OVP		0x04
#define MAX77665_INT_MBCCHGERR		0x08
#define MAX77665_INT_CHGENBLD		0x10
#define MAX77665_INT_BATDET		0x20

/* MAX77665_STATUS1 */
#define MAX77665_STATUS_ADC		0x1F
#define MAX77665_STATUS_ADCLOW		0x20
#define MAX77665_STATUS_ADCERROR	0x40
#define MAX77665_STATUS_ADC1K		0x80

/* MAX77665_STATUS2 */
#define MAX77665_STATUS_CHGTYP		0x07
#define MAX77665_STATUS_CHGDETRUN	0x08
#define MAX77665_STATUS_DCDTMR		0x10
#define MAX77665_STATUS_DXOVP		0x20
#define MAX77665_STATUS_VBVOLT		0x40
#define MAX77665_STATUS_VIDRM		0x80

/* MAX77665_STATUS3 */
#define MAX77665_STATUS_EOC		0x01
#define MAX77665_STATUS_CGMBC		0x02
#define MAX77665_STATUS_OVP		0x04
#define MAX77665_STATUS_MBCCHGERR	0x08
#define MAX77665_STATUS_CHGENBLD	0x10
#define MAX77665_STATUS_BATDET		0x60

/* MAX77665_INTMASK1 */
#define MAX77665_INT_ADCM		0x01
#define MAX77665_INT_ADCLOWM		0x02
#define MAX77665_INT_ADCERRORM		0x04
#define MAX77665_INT_ADC1KM		0x08

/* MAX77665_INTMASK2 */
#define MAX77665_INT_CHGYPEM		0x01
#define MAX77665_INT_CHGDETRUNM		0x02
#define MAX77665_INT_DCDTMRM		0x04
#define MAX77665_INT_DXOVPM		0x08
#define MAX77665_INT_VBVOLTM		0x10
#define MAX77665_INT_VIDRMM		0x20

/* MAX77665_INTMASK3 */
#define MAX77665_INT_EOCM		0x01
#define MAX77665_INT_CGMBCM		0x02
#define MAX77665_INT_OVPM		0x04
#define MAX77665_INT_MBCCHGERRM		0x08
#define MAX77665_INT_CHGENBLDM		0x10
#define MAX77665_INT_BATDETM		0x20


/* MAX77665_STATUS_ADC */
#define MAX77665_ADC_OTG		0x00
#define MAX77665_ADC_SENDEND		0x01
#define MAX77665_ADC_S1			0x02
#define MAX77665_ADC_S2			0x03
#define MAX77665_ADC_S3			0x04
#define MAX77665_ADC_S4			0x05
#define MAX77665_ADC_S5			0x06
#define MAX77665_ADC_S6			0x07
#define MAX77665_ADC_S7			0x08
#define MAX77665_ADC_S8			0x09
#define MAX77665_ADC_S9			0x0A
#define MAX77665_ADC_S10		0x0B
#define MAX77665_ADC_S11		0x0C
#define MAX77665_ADC_S12		0x0D
#define MAX77665_ADC_ACCESSORY1		0x0E
#define MAX77665_ADC_ACCESSORY2		0x0F
#define MAX77665_ADC_ACCESSORY3		0x10
#define MAX77665_ADC_ACCESSORY4		0x11
#define MAX77665_ADC_ACCESSORY5		0x12
#define MAX77665_ADC_CEA936		0x13
#define MAX77665_ADC_DEVICE		0x14
#define MAX77665_ADC_TTY		0x15
#define MAX77665_ADC_UART		0x16
#define MAX77665_ADC_TYPE1		0x17
#define MAX77665_ADC_USB_OFF		0x18
#define MAX77665_ADC_USB_ON		0x19
#define MAX77665_ADC_AV			0x1A
#define MAX77665_ADC_TYPE2		0x1B
#define MAX77665_ADC_UART_OFF		0x1C
#define MAX77665_ADC_UART_ON		0x1D
#define MAX77665_ADC_AUDIO		0x1E
#define MAX77665_ADC_OPEN		0x1F

enum max77665_status
{
	MAX77665_EV_OTG,
	MAX77665_EV_USB,
	MAX77665_EV_CHG,
	MAX77665_EV_MHL,
	MAX77665_EV_UART,
	MAX77665_EV_HEADPHONE,
	MAX77665_EV_SENDEND,
	MAX77665_EV_KEY1,
	MAX77665_EV_KEY2,
	MAX77665_EV_KEY3,
	MAX77665_EV_KEY4,
	MAX77665_EV_KEY5,
	MAX77665_EV_KEY6,
	MAX77665_EV_KEY7,
	MAX77665_EV_KEY8,
	MAX77665_EV_KEY9,
	MAX77665_EV_KEY10,
	MAX77665_EV_KEY11,
	MAX77665_EV_KEY12,
};

enum max77665_muic_chg_type
{
	MAX77665_MUIC_CHG_NOTHING,
	MAX77665_MUIC_CHG_USB,
	MAX77665_MUIC_CHG_USB_DOWNSTREAN,
	MAX77665_MUIC_CHG_DEDICATED,
	MAX77665_MUIC_CHG_APPLE500MA,
	MAX77665_MUIC_CHG_APPLE1MA,
	MAX77665_MUIC_CHG_SPECIAL,
	MAX77665_MUIC_CHG_RESERVED,
};

struct max77665_muic
{
	struct device		*dev;
	struct regmap		*regmap;
	struct input_dev	*input;
	struct switch_dev	sdev;
	enum max77665_status	status;
	unsigned int keys[MAX77665_REMOTE_KEYS];
#ifdef CONFIG_USB_ODIN_DRD
	struct delayed_work usb_init;
	int init;
	struct odin_usb3_otg 	*odin_otg;
#endif
};

#define CONFIG_USB_ODIN_WAKEUP

#ifdef CONFIG_USB_ODIN_WAKEUP
/* Charger detect temp code */
#define CHARGER_CHECK_ADDR 0x200F4030
static void __iomem *max77665_charger_check;
static struct wakeup_source max77665_muic_lock;
#endif

#ifdef CONFIG_USB_ODIN_DRD
#include <linux/usb/odin_usb3.h>
#include <linux/delay.h>
#include <linux/usb/otg.h>

static struct max77665_muic *g_muic;
void odin_usb_otg_vbus_onoff_77665(int onoff)
{
	struct max77665_muic *max77665_muic = g_muic;
	u8 cdetctrl1_data;
	int ret;
	printk(KERN_INFO "[%s] ------> %s \n", __func__, onoff?"ON":"OFF");

	if (onoff){
		max77665_charger_interrupt_onoff(onoff);

		max77665_charger_otg_onoff(onoff);

		max77665_charger_vbypset(0x50);
	} else {
		max77665_charger_otg_onoff(onoff);

		max77665_charger_vbypset(0x00);

		mdelay(50);

		max77665_charger_interrupt_onoff(onoff);
	}
}

static void max77665_vbus_status(
		struct max77665_muic *max77665_muic, u8 adc, u8 chg)
{
	switch (adc & MAX77665_STATUS_ADC)
	{
		case MAX77665_ADC_OPEN:
			switch (chg & MAX77665_STATUS_CHGTYP)
			{
				case 0: /* No Voltage */
					odin_otg_vbus_event(max77665_muic->odin_otg, 0, 0);
					break;
				case 1: /* USB */
					odin_otg_vbus_event(max77665_muic->odin_otg, 1, 0);
					break;
				default:
					printk("[%s]CHG Other case = %x \n", __func__, chg);
					break;
			}
			break;
		default:
			printk("[%s]ADC Other case = %x \n", __func__, adc);
			break;
	}
}

static int max77665_muic_adcdbset(struct max77665_muic *max77665_muic,
							int value)
{
	int ret;
	u8 control3_data;

	ret = regmap_read(max77665_muic->regmap, MAX77665_CONTROL3, &control3_data);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max77665_muic->dev, "%s: max77665_read_regs() errors -%d\n"
								, __func__, ret);
		return ret;
	}

	control3_data = value << 4;
	ret = regmap_write(max77665_muic->regmap, MAX77665_CONTROL3, &control3_data);
	if (IS_ERR_VALUE(ret))
	{
		dev_info(max77665_muic->dev, "%s: max77665_write_regs() errors -%d\n"
								, __func__, ret);
		return ret;
	}

	return 0;
}

static void max77665_muic_usb_init(struct work_struct *work)
{
	struct max77665_muic *max77665_muic =
	    container_of(work, struct max77665_muic, usb_init.work);
	u8 int_data[3];
	u8 status_data[3];
	u8 adc, chg;
	int ret;
	struct usb_phy *phy = NULL;
	struct odin_usb3_otg *usb3_otg;
	printk("[%s] \n", __func__);

	phy = usb_get_phy(USB_PHY_TYPE_USB3);
	if (IS_ERR_OR_NULL(phy))
		return;

	usb3_otg = phy_to_usb3otg(phy);
	max77665_muic->odin_otg = usb3_otg;

	ret = regmap_bulk_read(max77665_muic->regmap, MAX77665_INT1,
				int_data, ARRAY_SIZE(int_data));
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max77665_muic->dev, "%s: max77665_read_regs() errors -%d\n"
								, __func__, ret);
		return IRQ_HANDLED;
	}

	ret = regmap_bulk_read(max77665_muic->regmap, MAX77665_STATUS1,
				status_data, ARRAY_SIZE(status_data));
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max77665_muic->dev, "%s: max77665_read_regs() errors -%d\n"
								, __func__, ret);
		return IRQ_HANDLED;
	}

	adc = status_data[0] & (MAX77665_STATUS_ADC | MAX77665_STATUS_ADC1K);
	chg = status_data[1] & MAX77665_STATUS_CHGTYP;

	max77665_vbus_status(max77665_muic, adc, chg);

	max77665_muic->init = 1;
}


#endif

static enum max77665_status max77665_muic_analyze_status(
		struct max77665_muic *max77665_muic, u8 adc, u8 chg)
{
	enum max77665_status status;

	if (adc & MAX77665_STATUS_ADC1K)
		status = MAX77665_EV_MHL;
	else
		switch (adc & MAX77665_STATUS_ADC)
		{
		case MAX77665_ADC_OTG:
			status = MAX77665_EV_OTG;
			break;

		case MAX77665_ADC_SENDEND:
			status = MAX77665_EV_SENDEND;
			break;

		case MAX77665_ADC_S1 ... MAX77665_ADC_S12:
			status = MAX77665_EV_KEY1 + adc - MAX77665_ADC_S1;
			break;

		case MAX77665_ADC_CEA936:
		case MAX77665_ADC_AUDIO:
			status = MAX77665_EV_HEADPHONE;
			break;

		case MAX77665_ADC_USB_OFF:
		case MAX77665_ADC_USB_ON:
			status = MAX77665_EV_USB;
			break;

		case MAX77665_ADC_UART:
		case MAX77665_ADC_UART_OFF:
		case MAX77665_ADC_UART_ON:
			status = MAX77665_EV_UART;
			break;

		case MAX77665_ADC_OPEN:
			switch (chg & MAX77665_STATUS_CHGTYP)
			{
				case 0x01:
					status = MAX77665_EV_USB;
					break;

				case 0x02 ... 0x06:
					status = MAX77665_EV_CHG;
					break;
			}
			break;
		}

	return status;
}

static void max77665_muic_process_status(struct max77665_muic *max77665_muic,
		enum max77665_status status, int inserted, u8 chg_type)
{
#if defined(CONFIG_MACH_ODIN_HDK)
	struct power_supply *psy = NULL;
	union power_supply_propval val;
#endif
	switch (status)
	{
	case MAX77665_EV_OTG:
		/* It does not need to do anything. AP can detect OTG */
		break;

	case MAX77665_EV_MHL:
		input_report_switch(max77665_muic->input, SW_VIDEOOUT_INSERT, inserted);
		input_sync(max77665_muic->input);
		break;

	case MAX77665_EV_SENDEND:
		input_report_key(max77665_muic->input, KEY_MEDIA, inserted);
		input_sync(max77665_muic->input);
		break;

	case MAX77665_EV_HEADPHONE:
		switch_set_state(&max77665_muic->sdev, inserted);

		input_report_switch(max77665_muic->input, SW_HEADPHONE_INSERT, inserted);
		input_sync(max77665_muic->input);
		break;

	case MAX77665_EV_UART:
		break;

	case MAX77665_EV_CHG:
	case MAX77665_EV_USB:
		switch (chg_type)
		{
		case MAX77665_MUIC_CHG_USB:
		case MAX77665_MUIC_CHG_USB_DOWNSTREAN:
		case MAX77665_MUIC_CHG_APPLE500MA:
#if defined(CONFIG_MACH_ODIN_HDK)
			psy = power_supply_get_by_name("usb");
			val.intval = 1;
			psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
#else
			power_supply_set_charging_by(power_supply_get_by_name("usb"), TRUE);
#endif
			break;

		case MAX77665_MUIC_CHG_DEDICATED:
		case MAX77665_MUIC_CHG_APPLE1MA:
		case MAX77665_MUIC_CHG_SPECIAL:
#if defined(CONFIG_MACH_ODIN_HDK)
			psy = power_supply_get_by_name("ac");
			val.intval = 1;
			psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
#else
			power_supply_set_charging_by(power_supply_get_by_name("ac"), TRUE);
#endif
			break;

		default:
			break;
		}
		break;

	case MAX77665_EV_KEY1 ... MAX77665_EV_KEY12:
		input_report_key(max77665_muic->input,
			max77665_muic->keys[status - MAX77665_EV_KEY1], inserted);
		input_sync(max77665_muic->input);
		break;

	default:
		break;
	}
}

#ifdef CONFIG_USB_ODIN_DRD
static int max77665_odin_irq_handler(struct max77665_muic *max77665_muic,
								int irq)
{
	u8 int_data[3];
	u8 status_data[3];
	u8 adc, chg;
	int ret;

	ret = regmap_bulk_read(max77665_muic->regmap, MAX77665_INT1,
					int_data, ARRAY_SIZE(int_data));
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max77665_muic->dev, "%s: max77665_read_regs() errors -%d\n"
								, __func__, ret);
		return IRQ_HANDLED;
	}

	ret = regmap_bulk_read(max77665_muic->regmap, MAX77665_STATUS1,
					status_data, ARRAY_SIZE(status_data));
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max77665_muic->dev, "%s: max77665_read_regs() errors -%d\n"
								, __func__, ret);
		return IRQ_HANDLED;
	}

	adc = status_data[0] & (MAX77665_STATUS_ADC | MAX77665_STATUS_ADC1K);
	chg = status_data[1] & MAX77665_STATUS_CHGTYP;

	if (max77665_muic->init) {

		switch (adc & MAX77665_STATUS_ADC)
		{
#if 0 /*ADC OTG moved to DA906X HWMON */
		case MAX77665_ADC_OTG:
			break;
#endif
		case MAX77665_ADC_OPEN:
			switch (chg & MAX77665_STATUS_CHGTYP)
			{
				case 0x00:
					if (odin_drd_usb_id_state(max77665_muic->odin_otg.phy)) {
						odin_otg_vbus_event(max77665_muic->odin_otg, 0, 0);
					}
					break;

				case 0x01:
					if (odin_drd_usb_id_state(max77665_muic->odin_otg.phy)) {
				#ifdef CONFIG_USB_ODIN_WAKEUP
						u8 charger_int;
						/* Charger wake up workaround code is added */
						charger_int = readw(max77665_charger_check + 0x0);
						if ((charger_int & 0x2) == 0x2) {
							writew(0, max77665_charger_check);
							__pm_wakeup_event(&max77665_muic_lock, 5000);
							odin_otg_vbus_event(max77665_muic->odin_otg, 1, 2000);
						} else
				#endif
						{
							odin_otg_vbus_event(max77665_muic->odin_otg, 1, 0);
						}
					}
					break;

				default:
					printk("[%s] CASE :: CHG - Other \n", __func__);
					break;
			}
			break;

		default:
			printk("[%s] CASE :: ACD - Other \n", __func__);
			break;
		}
	}
	return 0;
}
#endif

static irqreturn_t max77665_muic_isr(int irq, void *data)
{
	struct max77665_muic *max77665_muic = (struct max77665_muic *)data;

#ifdef CONFIG_USB_ODIN_DRD
	max77665_odin_irq_handler(max77665_muic, irq);
#else
	u8 int_data[3];
	u8 status_data[3];
	u8 adc, chg;
	int ret;
	enum max77665_status status;

	ret = regmap_bulk_read(max77665_muic->regmap, MAX77665_INT1,
					int_data, ARRAY_SIZE(int_data));
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max77665_muic->dev, "%s: max77665_read_regs() errors -%d\n"
					, __func__, ret);
		return IRQ_HANDLED;
	}

	if (int_data[0] & MAX77665_INT_ADC)
	{
		ret = regmap_bulk_read(max77665_muic->regmap, MAX77665_STATUS1,
					status_data, ARRAY_SIZE(status_data));
		if (IS_ERR_VALUE(ret))
		{
			dev_err(max77665_muic->dev, "%s: max77665_read_regs() errors -%d\n"
					, __func__, ret);
			return IRQ_HANDLED;
		}
		adc = status_data[0] & (MAX77665_STATUS_ADC | MAX77665_STATUS_ADC1K);
		chg = status_data[1] & MAX77665_STATUS_CHGTYP;

		status = max77665_muic_analyze_status(max77665_muic, adc, chg);

		if (unlikely(status == max77665_muic->status))
			return IRQ_HANDLED;

#ifndef CONFIG_MACH_ODIN_HDK
		max77665_muic_process_status(max77665_muic, max77665_muic->status, 0,
					MAX77665_MUIC_CHG_NOTHING);
		max77665_muic_process_status(max77665_muic, status, 1, chg);
#endif
		max77665_muic->status = status;
	}
#endif /*CONFIG_USB_ODIN_DRD*/
	return IRQ_HANDLED;
}

static int max77665_muic_irq_init(struct i2c_client *client,
					struct max77665_muic *max77665_muic)
{
#ifdef CONFIG_USB_ODIN_DRD
	int ret;

	regmap_write(max77665_muic->regmap, MAX77665_INTMASK1, 0x09);
	regmap_write(max77665_muic->regmap, MAX77665_INTMASK2, 0x11);
	regmap_write(max77665_muic->regmap, MAX77665_INTMASK3, 0x00);
#else
	const u8 masks[3] =
	{
		MAX77665_INT_ADCLOWM	|
		MAX77665_INT_ADCERRORM	|
		MAX77665_INT_ADC1KM,
		MAX77665_INT_CHGYPEM	|
		MAX77665_INT_CHGDETRUNM	|
		MAX77665_INT_DCDTMRM	|
		MAX77665_INT_DXOVPM	|
		MAX77665_INT_VBVOLTM	|
		MAX77665_INT_VIDRMM,
		MAX77665_INT_EOCM	|
		MAX77665_INT_CGMBCM	|
		MAX77665_INT_OVPM	|
		MAX77665_INT_MBCCHGERRM	|
		MAX77665_INT_CHGENBLDM	|
		MAX77665_INT_BATDETM,
	};
	int ret;

	ret = regmap_bulk_write(max77665_muic->regmap, MAX77665_INTMASK1,
						masks, ARRAY_SIZE(masks));
	if (IS_ERR_VALUE(ret))
	{
		dev_info(max77665_muic->dev, "%s: max77665_write_regs() errors -%d\n"
						, __func__, ret);
		return ret;
	}
#endif
	ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
			max77665_muic_isr, 0, "max77665-muic", max77665_muic);
	if (IS_ERR_VALUE(ret))
		dev_err(&client->dev, "Failed to request IRQ #%d: %d\n"
						, client->irq, ret);

	return ret;
}

#ifdef CONFIG_OF
static struct max77665_muic_platform_data *max77665_muic_parse_dt
				(struct device *dev)
{
	struct max77665_muic_platform_data *pdata;
	struct device_node *np = dev->of_node;
	u32 val[ARRAY_SIZE(pdata->keys)];
#if defined(CONFIG_MACH_ODIN_HDK)
	struct device_node *max77665_np = NULL;
	int ret, i, irq_base;
#endif

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

#if defined(CONFIG_MACH_ODIN_HDK)
	max77665_np = of_find_node_by_name(of_aliases, "max77665");
	if (max77665_np) {
		if (of_property_read_u32(max77665_np, "irq-base", &irq_base) != 0)
			return ERR_PTR(-EINVAL);
		pdata->irq = irq_base + MAX77665_INT_MUIC;
		printk(KERN_CRIT "%s: get irq_base %d -> muic irq %d\n", __func__, irq_base, pdata->irq);
	} else {
		printk(KERN_CRIT "%s: can't find max77665\n", __func__);
		return ERR_PTR(-EINVAL);
	}
#else
	pdata->irq = irq_of_parse_and_map(dev->of_node, 0);
	if (pdata->irq == 0)
		return ERR_PTR(-EINVAL);
#endif

	ret = of_property_read_u32_array(np, "keys", val, ARRAY_SIZE(val));
	if (IS_ERR_VALUE(ret))
		return ERR_PTR(-EINVAL);

	for (i = 0; i < ARRAY_SIZE(val); i++)
		pdata->keys[i] = val[i];

	return pdata;
}
#endif


const struct regmap_config max77665_muic_regmap_config =
{
	.reg_bits = 8,
	.val_bits = 8,
};

static int max77665_muic_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct max77665_muic_platform_data *pdata = NULL;
	struct max77665_muic *max77665_muic;
	struct input_dev *input;
	int i;
	int ret;

#ifdef CONFIG_OF
	pdata = max77665_muic_parse_dt(dev);
	if (unlikely(IS_ERR(pdata)))
		return PTR_ERR(pdata);
	client->irq = pdata->irq;
#else
	pdata = dev_get_platdata(dev);
#endif

	max77665_muic = devm_kzalloc(dev, sizeof(*max77665_muic), GFP_KERNEL);
	if (unlikely(!max77665_muic))
	{
		dev_err(dev, "%s: failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	max77665_muic->regmap = devm_regmap_init_i2c(client,
				&max77665_muic_regmap_config);
	if (IS_ERR(max77665_muic->regmap))
		return PTR_ERR(max77665_muic->regmap);

	input = input_allocate_device();
	if (unlikely(!input))
	{
		dev_err(dev, "%s: failed to allocate state\n", __func__);
		return -ENOMEM;
	}

	max77665_muic->input = input;
	i2c_set_clientdata(client, max77665_muic);

	input->name = "max77665-muic";
	input->dev.parent = dev;

	input->id.bustype = BUS_I2C;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0001;

	/* Enable auto repeat feature of Linux input subsystem */
	__set_bit(EV_REP, input->evbit);

	for (i = 0; i < MAX77665_REMOTE_KEYS; i++)
	{
		max77665_muic->keys[i] = pdata->keys[i];
		if (pdata->keys[i] != KEY_RESERVED)
			input_set_capability(input, EV_KEY, pdata->keys[i]);
	}

	ret = input_register_device(input);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "%s: Unable to register input device, %d\n", __func__, ret);
		goto err_input;
	}

	max77665_muic->sdev.name = "max77665-muic";
	ret = switch_dev_register(&max77665_muic->sdev);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "%s: Unable to register switch device, %d\n", __func__, ret);
		goto err_input;
	}
#ifdef CONFIG_USB_ODIN_DRD
	max77665_muic->init = 0;
#endif

	ret = max77665_muic_irq_init(client, max77665_muic);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "%s: Unable to register IRQ, %d\n", __func__, ret);
		goto err_switch;
	}

#ifdef CONFIG_USB_ODIN_DRD
	g_muic = max77665_muic;
	max77665_muic_adcdbset(max77665_muic, 2);
	INIT_DELAYED_WORK(&max77665_muic->usb_init, max77665_muic_usb_init);
	schedule_delayed_work(&max77665_muic->usb_init, msecs_to_jiffies(20000));
#endif

#ifdef CONFIG_USB_ODIN_WAKEUP
	max77665_charger_check = ioremap(CHARGER_CHECK_ADDR, 0x8);
	wakeup_source_init(&max77665_muic_lock, "max77665_muic");
#endif

#ifdef CONFIG_OF
	devm_kfree(dev, pdata);
#endif
	return 0;

err_switch:
	switch_dev_unregister(&max77665_muic->sdev);
err_input:
	input_free_device(input);
	return ret;
}

static int max77665_muic_remove(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct max77665_muic *max77665_muic = i2c_get_clientdata(client);

	devm_free_irq(dev, client->irq, max77665_muic);
	input_unregister_device(max77665_muic->input);
	switch_dev_unregister(&max77665_muic->sdev);
	input_free_device(max77665_muic->input);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id max77665_muic_of_match[] = {
        { .compatible = "maxim,max77665-muic" },
        { },
};
MODULE_DEVICE_TABLE(of, max77665_muic_of_match);
#endif

static const struct i2c_device_id max77665_muic_id[] =
{
	{ "max77665-muic", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max77665_id);

static struct i2c_driver max77665_muic_driver =
{
	.driver =
	{
		.name = "max77665-muic",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = max77665_muic_of_match,
#endif
	},
	.id_table		= max77665_muic_id,
	.probe			= max77665_muic_probe,
	.remove			= max77665_muic_remove,
};

static int __init max77665_muic_init(void)
{
	return i2c_add_driver(&max77665_muic_driver);
}
module_init(max77665_muic_init);

static void __exit max77665_muic_exit(void)
{
	i2c_del_driver(&max77665_muic_driver);
}
module_exit(max77665_muic_exit);

MODULE_DESCRIPTION("Maxim MAX77665 MUIC driver");
MODULE_AUTHOR("Gyungoh Yoo<jack.yoo@maximintegrated.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.2");
