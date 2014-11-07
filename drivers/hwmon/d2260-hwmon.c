/*
 *  HWMON driver for Dialog D2260 PMIC
 *
 *  Copyright (C) 2013 Dialog Semiconductor Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/mfd/d2260/pmic.h>
#include <linux/mfd/d2260/registers.h>
#include <linux/mfd/d2260/core.h>

#ifdef CONFIG_USB_ODIN_DRD
#include <linux/usb/odin_usb3.h>
#endif

#ifdef CONFIG_INPUT_MAX14688
#include <linux/platform_data/hds_max14688.h>
#endif
#include <linux/mfd/max77819-charger.h>
#include <linux/mfd/max77807/max77807-charger.h>
#include <linux/board_lge.h>

struct d2260_hwmon {
	struct d2260 *d2260;
	struct device *classdev;
	struct platform_device  *pdev;
	struct completion man_adc_rdy;
	int	irq;
#ifdef CONFIG_USB_ODIN_DRD
	struct device *dev;
	struct delayed_work	adc_work;
	struct odin_usb3_otg 	*odin_otg;
	unsigned int usb_init : 1;
	struct wakeup_source	wake_lock;
	atomic_t status_adc;
	atomic_t status_bat;
#endif
};

struct vadc_map_bat_temp {
	int celsius;
	int voltage;
	int gradient;
	int intercept;
};

struct d2260_hwmon *g_hwmon = NULL;

static const struct vadc_map_bat_temp adcmap_btm_threshold[26] = {0};

static const struct vadc_map_bat_temp adcmap_btm_threshold_1_8[] = {
	{-400,	1630, -2, 15500}, /* index = 0 */
	{-350,	1620, -2, 15500},
	{-300,	1610, -6, 14300},
	{-250,	1580, -4, 14800},
	{-200,	1560, -8, 14000},
	{-150,	1520, -8, 14000},
	{-100,	1480, -10, 13800},
	{-50,	1430, -12, 13700},
	{0,		1370, -14, 13700},
	{50,	1300, -16, 13800},
	{100,	1220, -18, 14000},
	{150,	1130, -18, 14000},
	{200,	1040, -18, 14000},
	{250,	950, -20, 14500},
	{300,	850, -18, 13900},
	{350,	760, -16, 13200},
	{400,	680, -15, 12800},
	{420,	650, -20, 14900},
	{450,	590, -14, 12200},
	{500,	520, -14, 12200},
	{550,	450, -12, 11100},
	{600,	390, -10, 9900},
	{650,	340, -10, 9900},
	{700,	290, -8, 8500},
	{750,	250, -6, 7000},
	{800,	220, 1, 1}, /* index = 25 */
};

static const struct vadc_map_bat_temp adcmap_btm_threshold_2_5[] = {
	{-400,	2380, -4,  22200}, /* index = 0 */
	{-350,	2360, -4,  22200},
	{-300,	2340, -6,  21600},
	{-250,	2310, -6,  21600},
	{-200,	2280, -12, 20400},
	{-150,	2220, -16, 19800},
	{-100,	2140, -14, 20000},
	{-50,	2070, -18, 19800},
	{0,		1980, -16, 19800},
	{50,	1900, -26, 20300},
	{100,	1770, -20, 19700},
	{150,	1670, -26, 20600},
	{200,	1540, -26, 20600},
	{250,	1410, -26, 20600},
	{300,	1280, -24, 20000},
	{350,	1160, -24, 20000},
	{400,	1040, -30, 22400},
	{420,	980,  -30, 22400},
	{450,	890,  -14, 15200},
	{500,	820,  -20, 18200},
	{550,	720,  -18, 17100},
	{600,	630,  -18, 17100},
	{650,	540,  -14, 14500},
	{700,	470,  -14, 14500},
	{750,	400,  -10, 11500},
	{800,	350,  1,   1}, /* index = 25 */
};

#define temp_table_index 25

#define temp_table_max 800
#define temp_table_min -400

#define D2260_HWMON_STATUS_RESUME      0
#define D2260_HWMON_STATUS_SUSPEND     1
#define D2260_HWMON_STATUS_QUEUE_WORK  2

static int volt_table_max = 0;
static int volt_table_min = 0;

static int ADC_REF_VOLT = 0;

extern void call_sysfs_fuel(void);

int d2260_adc_auto_read(struct d2260_hwmon *hwmon, int channel);

static ssize_t show_name(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "d2260\n");
}
enum d2260_adc_channels {
	D2260_ADC_BAT_VOLTAGE = 0,
	D2260_ADC_TEMP1_VOLTAGE = 1,
	D2260_ADC_TEMP2_VOLTAGE = 2,
	D2260_ADC_VF_VOLTAGE = 3,
	D2260_ADC_TJUNC_VOLTAGE = 4,
	D2260_ADC_ADCIN1_VOLTAGE = 5,
	D2260_ADC_ADCIN2_VOLTAGE = 6,
	D2260_ADC_ADCIN3_VOLTAGE = 7,
	D2260_ADC_ADCIN4_VOLTAGE = 8,
};

static const char *input_names[] = {
	[D2260_ADC_BAT_VOLTAGE]   = "VBAT",
	[D2260_ADC_TEMP1_VOLTAGE] = "TEMP1",
	[D2260_ADC_TEMP2_VOLTAGE] = "TEMP2",
	[D2260_ADC_VF_VOLTAGE]    = "VF",
	[D2260_ADC_TJUNC_VOLTAGE] = "TJUNC",
	[D2260_ADC_ADCIN1_VOLTAGE] = "ADCIN1",
	[D2260_ADC_ADCIN2_VOLTAGE] = "ADCIN2",
	[D2260_ADC_ADCIN3_VOLTAGE] = "ADCIN3",
	[D2260_ADC_ADCIN4_VOLTAGE] = "ADCIN4",
};

int voltage_to_degree_temp1(int voltage)
{
	int count = 0;

	if(ADC_REF_VOLT == 18) {
		if(voltage >= volt_table_max) {return (10*voltage-15500)/-2;}
		if(voltage < volt_table_min) {return (10*voltage-7000)/-6;}
	} else {
		if(voltage >= volt_table_max) {return (10*voltage-22200)/-4;}
		if(voltage < volt_table_min) {return (10*voltage-11500)/-10;}
	}

	for(count = 0; count < temp_table_index; count++) {
		if((voltage < adcmap_btm_threshold[count].voltage) && (voltage >= adcmap_btm_threshold[count+1].voltage)) {
			return ((10*voltage)-(adcmap_btm_threshold[count].intercept))/(adcmap_btm_threshold[count].gradient);
		}
	}
}

int degree_to_voltage(int celsius)
{
	int count = 0;

	if(ADC_REF_VOLT == 18) {
		if(celsius <= temp_table_min) { return (-2*celsius+15500)/10; }
		if(celsius > temp_table_max) { return (-6*celsius+7000)/10; }
	} else {
		if(celsius <= temp_table_min) { return (-4*celsius+22200)/10; }
		if(celsius > temp_table_max) { return (-10*celsius+11500)/10; }
	}

	for(count = 0; count < temp_table_index; count++) {
		if((celsius > adcmap_btm_threshold[count].celsius) && (celsius <= adcmap_btm_threshold[count+1].celsius)) {
			return (((adcmap_btm_threshold[count].gradient)*celsius) + adcmap_btm_threshold[count].intercept)/10;
		}
	}
}

#ifdef CONFIG_USB_ODIN_DRD
#define D2260_ADC_VF_VOLTAGE_SHIFT 4

#define D2260_ADC_USB_OTG 0
#define D2260_ADC_USB_UNPLUGGED 0x6C

static int d2260_adc_read(struct d2260_hwmon *hwmon, bool state,
									unsigned int enable_adc, int channel)
{
	int ret;
	unsigned int adc_value;

	ret = d2260_set_bits(hwmon->d2260, D2260_ADC_CONT_REG, D2260_ADC_MODE_MASK);
	if (ret < 0)
		goto err;

	mdelay(30);
	ret = d2260_adc_auto_read(hwmon, channel);
	if (ret >= 0)
		adc_value = ret;
	else
		goto err;

	/* Clear ADC Enable Value */
	ret = d2260_clear_bits(hwmon->d2260, D2260_ADC_CONT_REG, 0x78);
	if (ret < 0)
		goto err;

	complete(&hwmon->man_adc_rdy);
	return (adc_value >> D2260_ADC_VF_VOLTAGE_SHIFT);

err:
	dev_err(hwmon->dev, "D2260 ADC read failed!(%d)\n", ret);
	return ret;
}

static void d2260_hwmon_usb_register(struct d2260_hwmon *hwmon)
{
	struct usb_phy *phy = NULL;
	struct odin_usb3_otg *usb3_otg;
	usb_dbg("[%s]\n", __func__);

	phy = usb_get_phy(USB_PHY_TYPE_USB3);
	if (IS_ERR_OR_NULL(phy)) {
		return;
	}

	usb3_otg = phy_to_usb3otg(phy);
	hwmon->odin_otg = usb3_otg;
	hwmon->odin_otg->adc_dev = hwmon->dev;
}

static void d2260_hwmon_irq_work(struct work_struct *work)
{
	struct d2260_hwmon *hwmon =
		container_of(work, struct d2260_hwmon, adc_work.work);
	int value, usb_id;

	value = d2260_adc_read(hwmon, true,
				D2260_AUTO_VF_EN_MASK, D2260_ADC_VF_VOLTAGE);
	if (value == -EBUSY) {
		schedule_delayed_work(&hwmon->adc_work, msecs_to_jiffies(300));
		return;
	}

	switch (value) {
	case D2260_ADC_USB_OTG:
#ifdef CONFIG_USB_G_LGE_ANDROID
		if (lge_get_factory_56k_130k_boot() &&
				odin_otg_get_vbus_state(hwmon->odin_otg)) {
			printk("otg off: lge_get_factory_56k_130k_boot and vbus on\n");
			usb_id = 1;
		}
		else {
			printk("otg on\n");
			usb_id = 0;
		}
#else
		usb_id = 0;
#endif
		break;
	case D2260_ADC_USB_UNPLUGGED:
		usb_id = 1;
		break;
	default:
		printk("[%s]Other cable \n", __func__);
		usb_id = 1;
		break;
	}
	odin_otg_id_event(hwmon->odin_otg, usb_id);
}

static irqreturn_t d2260_hwmon_irq_handler(int irq, void *irq_data)
{
	struct d2260_hwmon *hwmon = irq_data;

	if(atomic_read(&hwmon->status_adc) == D2260_HWMON_STATUS_SUSPEND) {
		atomic_set(&hwmon->status_adc, D2260_HWMON_STATUS_QUEUE_WORK);
	} else {
			if (hwmon->usb_init) {
#ifdef CONFIG_USB_G_LGE_ANDROID
			if (lge_get_factory_56k_130k_boot()) {
				schedule_delayed_work(&hwmon->adc_work, msecs_to_jiffies(300));
				__pm_wakeup_event(&hwmon->wake_lock, 800); /* 800ms Wake lock time */
			}
			else {
				schedule_delayed_work(&hwmon->adc_work, 0);
				__pm_wakeup_event(&hwmon->wake_lock, 500); /* 500ms Wake lock time */
			}
#else
			schedule_delayed_work(&hwmon->adc_work, 0);
			__pm_wakeup_event(&hwmon->wake_lock, 500); /* 500ms Wake lock time */
#endif
			}
	}

	return IRQ_HANDLED;
}

void d2260_drd_id_init_detect(struct odin_usb3_otg *odin_otg)
{
	struct d2260_hwmon *hwmon = dev_get_drvdata(odin_otg->adc_dev);

	hwmon->usb_init = 1;
#ifdef CONFIG_USB_G_LGE_ANDROID
	schedule_delayed_work(&hwmon->adc_work, HZ);
#else
	schedule_delayed_work(&hwmon->adc_work, 0);
#endif
}
#endif

unsigned char bat_temp_force_update = 0;
static irqreturn_t d2260_hwmon_irq_handler_ETBAT1 (int irq, void *data)
{
	struct d2260_hwmon *mon = data;
	struct d2260 *d2260 = mon->d2260;
	u8 reg_val;

	if(atomic_read(&mon->status_bat) == D2260_HWMON_STATUS_SUSPEND) {
		atomic_set(&mon->status_bat, D2260_HWMON_STATUS_QUEUE_WORK);
	} else {
		dlg_info("%s : <<<Battery Temperature Interrupt ON ISR>>>\n", __func__);

		/* Force the Battery information update */
		bat_temp_force_update = 1;

		/* Update Battery Information Before OTP Drv */
		call_sysfs_fuel();

		/* Call LG Battery Temperature Scenario  */
		batt_temp_ctrl_start(0);
	}

	return IRQ_HANDLED;
}

void d2260_hwmon_set_bat_threshold_max(void)
{
	if(g_hwmon == NULL){
		dlg_info("%s : g_hwmon is not initialized yet\n", __func__);
		return;
	}

	/* Set Battery LOW/HIGH InterruptTheroshold */
	d2260_reg_write(g_hwmon->d2260, D2260_TEMP1_HIGHP_REG, 0x00);
	d2260_reg_write(g_hwmon->d2260, D2260_TEMP1_HIGHN_REG, 0x00);

	d2260_reg_write(g_hwmon->d2260, D2260_TEMP1_LOW_REG, 0xFF);
}
EXPORT_SYMBOL(d2260_hwmon_set_bat_threshold_max);

void d2260_hwmon_set_bat_threshold(int min, int max)
{
	int min_to_volt = 0;
	int max_to_volt = 0;
	u8 min_reg = 0;
	u8 max_reg = 0;

	if(g_hwmon == NULL){
		dlg_info("%s : g_hwmon is not initialized yet\n", __func__);
		return;
	}

	/* Convert Temperature to Voltage(0mV~2500mV)*/
	min_to_volt = degree_to_voltage(min);
	max_to_volt = degree_to_voltage(max);

	min_reg = (min_to_volt*255)/2500;
	max_reg = (max_to_volt*255)/2500;

	dlg_info("%s min_temp(%d) max_temp(%d) min_volt(%d) max_volt(%d) min_reg(%x) max_reg(%x)\n", __func__,
				min, max, min_to_volt, max_to_volt, min_reg, max_reg);

	/* Set Battery LOW/HIGH InterruptTheroshold */
	d2260_reg_write(g_hwmon->d2260, D2260_TEMP1_HIGHP_REG, max_reg);
	d2260_reg_write(g_hwmon->d2260, D2260_TEMP1_HIGHN_REG, max_reg);

	d2260_reg_write(g_hwmon->d2260, D2260_TEMP1_LOW_REG, min_reg);

	/* Clear Battery Temperature EVENT Register*/
	d2260_set_bits(g_hwmon->d2260, D2260_EVENT_B_REG, D2260_E_TBAT1_MASK);

}
EXPORT_SYMBOL(d2260_hwmon_set_bat_threshold);

int d2260_adc_auto_read(struct d2260_hwmon *hwmon, int channel)
{
	int ret = -EINVAL;
	u8 msb_val,lsb_val;
	u16 adc_value;
	u8 adcin_sel = 0;
	u8 temp_reg = 0;

	mutex_lock(&hwmon->d2260->lock);

	switch (channel) {
	case D2260_ADC_BAT_VOLTAGE:
		ret = d2260_reg_read(hwmon->d2260, D2260_VBAT_RES_REG, &msb_val);
		if (ret < 0)
			goto err_adc;
		adc_value = msb_val << 4;
		ret = d2260_reg_read(hwmon->d2260, D2260_ADC_RES_AUTO1_REG, &lsb_val);
		if (ret < 0)
			goto err_adc;
		adc_value |= lsb_val & 0x0F;
		mutex_unlock(&hwmon->d2260->lock);
		return (2500 + adc_value * 2000 / 0xFFF);

	case D2260_ADC_TEMP1_VOLTAGE:
		ret = d2260_reg_read(hwmon->d2260, D2260_TEMP1_RES_REG, &msb_val);
		if (ret < 0)
			goto err_adc;
		adc_value = msb_val << 4;
		ret = d2260_reg_read(hwmon->d2260, D2260_ADC_RES_AUTO1_REG, &lsb_val);
		if (ret < 0)
			goto err_adc;
		adc_value |= lsb_val >> 4;
		/*dlg_info("%s : Battery Temp ADC : (%d) \n", __func__,adc_value);*/
		mutex_unlock(&hwmon->d2260->lock);
		return (adc_value * 2500 / 0xFFF);

	case D2260_ADC_TEMP2_VOLTAGE:
		/* dlg_info("\n   ###[%s], <TEMP_SENSE_1> \n", __func__); */
		ret = d2260_reg_read(hwmon->d2260, D2260_TEMP2_RES_REG, &msb_val);
		if (ret < 0)
			goto err_adc;
		adc_value = msb_val << 4;
		ret = d2260_reg_read(hwmon->d2260, D2260_ADC_RES_AUTO3_REG, &lsb_val);
		if (ret < 0)
			goto err_adc;
		adc_value |= lsb_val & 0x0F;
        mutex_unlock(&hwmon->d2260->lock);
		return (adc_value * 2500 / 0xFFF);

	case D2260_ADC_VF_VOLTAGE:
		/* dlg_info("\n   ###[%s], <USB_ID_ADC> \n", __func__); */
		ret = d2260_reg_read(hwmon->d2260, D2260_VF_RES_REG, &msb_val);
		if (ret < 0)
			goto err_adc;
		adc_value = msb_val << 4;
		ret = d2260_reg_read(hwmon->d2260, D2260_ADC_RES_AUTO2_REG, &lsb_val);
		if (ret < 0)
			goto err_adc;
		adc_value |= lsb_val & 0x0F;
		mutex_unlock(&hwmon->d2260->lock);
		return (adc_value * 2500 / 0xFFF);

	case D2260_ADC_TJUNC_VOLTAGE:
		ret = d2260_reg_read(hwmon->d2260, D2260_TJUNC_RES_REG, &msb_val);
		if (ret < 0)
			goto err_adc;
		adc_value = msb_val << 4;
		ret = d2260_reg_read(hwmon->d2260, D2260_ADC_RES_AUTO3_REG, &lsb_val);
		if (ret < 0)
			goto err_adc;
		adc_value |= lsb_val >> 4;
		mutex_unlock(&hwmon->d2260->lock);
		return (adc_value * 833 / 0xFFF);

	case D2260_ADC_ADCIN1_VOLTAGE:
		/* ADC auto mode disable */
		ret = d2260_clear_bits(hwmon->d2260, D2260_ADC_CONT_REG, D2260_ADC_AUTO_EN_MASK);
		if (ret < 0)
			goto err_adc;

		/* ADC Economy -> Normal */
		ret = d2260_set_bits(hwmon->d2260, D2260_ADC_CONT_REG, D2260_ADC_AUTO_EN_MASK|D2260_ADC_MODE_MASK);
		if (ret < 0)
			goto err_adc;

		adcin_sel = channel - D2260_ADC_ADCIN1_VOLTAGE;
		ret = d2260_reg_write(hwmon->d2260, D2260_GEN_CONF_0_REG, adcin_sel);
		if (ret < 0)
			goto err_adc;
		mdelay(2);	// msleep(2);
		ret = d2260_reg_read(hwmon->d2260, D2260_ADCIN5_RES_REG, &msb_val);
		if (ret < 0)
			return ret;
		adc_value = msb_val << 4;
		ret = d2260_reg_read(hwmon->d2260, D2260_ADC_RES_AUTO2_REG, &lsb_val);
		if (ret < 0)
			return ret;
		adc_value |= lsb_val >> 4;
		mutex_unlock(&hwmon->d2260->lock);
		return (adc_value * 2500 / 0xFFF);
	case D2260_ADC_ADCIN2_VOLTAGE:
		/*dlg_info("\n   ###[%s], <PCB_REVISION_ADC> \n", __func__);*/

		adcin_sel = channel - D2260_ADC_ADCIN1_VOLTAGE;
		ret = d2260_reg_write(hwmon->d2260, D2260_GEN_CONF_0_REG, adcin_sel);
		if (ret < 0)
			goto err_adc;
		msleep(2);
		ret = d2260_reg_read(hwmon->d2260, D2260_ADCIN5_RES_REG, &msb_val);
		if (ret < 0)
			goto err_adc;
		adc_value = msb_val << 4;
		ret = d2260_reg_read(hwmon->d2260, D2260_ADC_RES_AUTO2_REG, &lsb_val);
		if (ret < 0)
			goto err_adc;
		adc_value |= lsb_val >> 4;
		mutex_unlock(&hwmon->d2260->lock);
		return (adc_value * 2500 / 0xFFF);

	case D2260_ADC_ADCIN3_VOLTAGE:
		/*dlg_info("\n   ###[%s], <TOUCH_MAKER_ID> \n", __func__);*/

		adcin_sel = channel - D2260_ADC_ADCIN1_VOLTAGE;
		ret = d2260_reg_write(hwmon->d2260, D2260_GEN_CONF_0_REG, adcin_sel);
		if (ret < 0)
			goto err_adc;
		msleep(2);
		ret = d2260_reg_read(hwmon->d2260, D2260_ADCIN5_RES_REG, &msb_val);
		if (ret < 0)
			goto err_adc;
		adc_value = msb_val << 4;
		ret = d2260_reg_read(hwmon->d2260, D2260_ADC_RES_AUTO2_REG, &lsb_val);
		if (ret < 0)
			goto err_adc;
		adc_value |= lsb_val >> 4;
		mutex_unlock(&hwmon->d2260->lock);
		return (adc_value * 2500 / 0xFFF);

	case D2260_ADC_ADCIN4_VOLTAGE:
        /*dlg_info("\n   ###[%s], <VI_CHG> \n", __func__);*/ //For Debug
		adcin_sel = channel - D2260_ADC_ADCIN1_VOLTAGE;
		ret = d2260_reg_write(hwmon->d2260, D2260_GEN_CONF_0_REG, adcin_sel);
		if (ret < 0)
			goto err_adc;
		msleep(2);
		ret = d2260_reg_read(hwmon->d2260, D2260_ADCIN5_RES_REG, &msb_val);
		if (ret < 0)
			goto err_adc;
		adc_value = msb_val << 4;
		ret = d2260_reg_read(hwmon->d2260, D2260_ADC_RES_AUTO2_REG, &lsb_val);
		if (ret < 0)
			goto err_adc;
		adc_value |= lsb_val >> 4;
		mutex_unlock(&hwmon->d2260->lock);
		return (adc_value * 2500 / 0xFFF);

	default:
		break;
	};

err_adc:
	mutex_unlock(&hwmon->d2260->lock);
	return ret;
}

static ssize_t show_voltage(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct d2260_hwmon *hwmon = dev_get_drvdata(dev);
	int channel = to_sensor_dev_attr(attr)->index;
	int ret;

	ret = d2260_adc_auto_read(hwmon, channel);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", ret);
}

static ssize_t show_label(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int channel = to_sensor_dev_attr(attr)->index;

	return sprintf(buf, "%s\n", input_names[channel]);
}

static int d2260_adc_get(int ch)
{
	int val = -EINVAL;

	if (!g_hwmon) {
		dlg_warn("%s: hwmon is not ready\n", __func__);
		return -EFAULT;
	}

	if (ch >= D2260_ADC_BAT_VOLTAGE && ch <= D2260_ADC_ADCIN4_VOLTAGE) {
		val = d2260_adc_auto_read(g_hwmon, ch);
		/*dlg_info("%s: ch #%d val %d\n",__func__, ch, val);*/
	} else {
		dlg_warn("%s: invalid ch %d\n", __func__, ch);
	}

	return val;
}

#ifdef CONFIG_USB_ODIN_DRD
int odin_usb_get_adc(void)
{
	return d2260_adc_get(D2260_ADC_VF_VOLTAGE);
}
#endif

#ifdef CONFIG_INPUT_MAX14688
int odin_max14688_get_adc(void)
{
	return d2260_adc_get(D2260_ADC_ADCIN1_VOLTAGE);
}
#endif
int d2260_adc_get_usb_id(void)
{
	return d2260_adc_get(D2260_ADC_VF_VOLTAGE);
}

int d2260_adc_get_vbat_therm(void)
{
	return d2260_adc_get(D2260_ADC_TEMP1_VOLTAGE);
}
int d2260_adc_get_vichg(void)
{
	return d2260_adc_get(D2260_ADC_ADCIN4_VOLTAGE);
}

int d2260_get_battery_temp(void)
{
	return voltage_to_degree_temp1(d2260_adc_get(D2260_ADC_TEMP1_VOLTAGE));
}
EXPORT_SYMBOL(d2260_get_battery_temp);

int d2260_get_pcb_therm_top_voltage(void)
{
	return d2260_adc_get(D2260_ADC_ADCIN2_VOLTAGE);
}
EXPORT_SYMBOL(d2260_get_pcb_therm_top_voltage);

int d2260_get_pcb_therm_bot_voltage(void)
{
	return d2260_adc_get(D2260_ADC_ADCIN3_VOLTAGE);
}
EXPORT_SYMBOL(d2260_get_pcb_therm_bot_voltage);

#define D2260_NAMED_VOLTAGE(id,chan) \
	static SENSOR_DEVICE_ATTR(in##id##_input, S_IRUGO, show_voltage, NULL, chan); \
	static SENSOR_DEVICE_ATTR(in##id##_label, S_IRUGO, show_label, NULL, chan)

static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);

D2260_NAMED_VOLTAGE(0,D2260_ADC_BAT_VOLTAGE);
D2260_NAMED_VOLTAGE(1,D2260_ADC_TEMP1_VOLTAGE);
D2260_NAMED_VOLTAGE(2,D2260_ADC_TEMP2_VOLTAGE);
D2260_NAMED_VOLTAGE(3,D2260_ADC_VF_VOLTAGE);
D2260_NAMED_VOLTAGE(4,D2260_ADC_TJUNC_VOLTAGE);
D2260_NAMED_VOLTAGE(5,D2260_ADC_ADCIN1_VOLTAGE);
D2260_NAMED_VOLTAGE(6,D2260_ADC_ADCIN2_VOLTAGE);
D2260_NAMED_VOLTAGE(7,D2260_ADC_ADCIN3_VOLTAGE);
D2260_NAMED_VOLTAGE(8,D2260_ADC_ADCIN4_VOLTAGE);

static struct attribute *d2260_attributes[] = {
	&dev_attr_name.attr,

	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in0_label.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_label.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in2_label.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in3_label.dev_attr.attr,
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in4_label.dev_attr.attr,
	&sensor_dev_attr_in5_input.dev_attr.attr,
	&sensor_dev_attr_in5_label.dev_attr.attr,
	&sensor_dev_attr_in6_input.dev_attr.attr,
	&sensor_dev_attr_in6_label.dev_attr.attr,
	&sensor_dev_attr_in7_input.dev_attr.attr,
	&sensor_dev_attr_in7_label.dev_attr.attr,
	&sensor_dev_attr_in8_input.dev_attr.attr,
	&sensor_dev_attr_in8_label.dev_attr.attr,

	NULL
};

static const struct attribute_group d2260_attr_group = {
	.attrs	= d2260_attributes,
};

static int d2260_hwmon_hw_init(struct d2260 *d2260,
					u8 d2260_reg, u8 val)
{
	int ret;

	ret = d2260_reg_write(d2260, d2260_reg, val);
	if (ret < 0) {
		dev_err(d2260->dev,"hw init vale is failed \n");
		return ret;
	}
	return ret;
}

static int d2260_hwmon_probe(struct platform_device *pdev)
{
	struct d2260 *d2260 = dev_get_drvdata(pdev->dev.parent);
	struct d2260_hwmon *hwmon;
	int ret;
	unsigned int reg_val = 0;
	int temp = 0;
	int count = 0;

	dlg_info("%s : probe start\n", __func__);

	hwmon = devm_kzalloc(&pdev->dev, sizeof(struct d2260_hwmon), GFP_KERNEL);
	if (!hwmon) {
		ret = -ENOMEM;
		goto out;
	}

	hwmon->d2260 = d2260;
	hwmon->pdev = pdev;

	/* Initial ADC Set */
	d2260_hwmon_hw_init(d2260, D2260_GPADC_MCTL_REG, 0x55);

	/* ADC Active */
	d2260_hwmon_hw_init(d2260, D2260_ADC_CONT_REG, 0x87);

	/* VF Low Threshold Setting */
	d2260_hwmon_hw_init(d2260, D2260_VF_LOW_REG, 0x0F);

	/* VF High Threshold Setting */
	d2260_hwmon_hw_init(d2260, D2260_VF_HIGH_REG, 0x10);

	/* Get HW Revision */
	for(count = 0; count<26; count++){
		memcpy(&adcmap_btm_threshold[count], &adcmap_btm_threshold_2_5[count], sizeof(struct vadc_map_bat_temp));
	}
	volt_table_max = 2380;
	volt_table_min = 350;
	ADC_REF_VOLT = 25;
	printk("d2260_hwmon_probe : 2.5V ADC Reference Voltage Table\n");

	ret = sysfs_create_group(&pdev->dev.kobj, &d2260_attr_group);
	if (ret)
		goto err;

	hwmon->classdev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(hwmon->classdev)) {
		ret = PTR_ERR(hwmon->classdev);
		goto err_sysfs;
	}
#ifdef CONFIG_USB_ODIN_DRD
	hwmon->usb_init = 0;
	hwmon->dev = &pdev->dev;

	wakeup_source_init(&hwmon->wake_lock, "D2260_hwmon_lock");

	atomic_set(&hwmon->status_bat, D2260_HWMON_STATUS_RESUME);
	atomic_set(&hwmon->status_adc, D2260_HWMON_STATUS_RESUME);

	INIT_DELAYED_WORK(&hwmon->adc_work, d2260_hwmon_irq_work);

	d2260_hwmon_usb_register(hwmon);

	ret = d2260_request_irq(d2260, D2260_IRQ_EVF, "D2260-HWMON",
								d2260_hwmon_irq_handler, hwmon);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ.\n");
		goto err_sysfs;
	}
#endif

	/* Register Battery Temperature LOW/HIGH Threshold Interrupt */
	ret = d2260_request_irq(d2260, D2260_IRQ_ETBAT1, "ETBAT1_irq_INT",
								d2260_hwmon_irq_handler_ETBAT1, hwmon);
	if (ret < 0) {
		dev_err(d2260->dev,
				"Failed to register D2260_IRQ_ETBAT1 IRQ: %d\n", ret);
	}

	platform_set_drvdata(pdev, hwmon);

	g_hwmon = hwmon;

	init_completion(&hwmon->man_adc_rdy);

#if 0 /* Battery Temperature Table Check func */
	dlg_info("=================================================================================\n");
	for(temp = -450; temp <=800; temp+=50){
		dlg_info("Temperature Table test : (temp=%d, volt=%d)\n",temp,degree_to_voltage(temp));
		if(temp == 400){
			dlg_info("Temperature Table test : (temp=420, volt=%d)\n",degree_to_voltage(420));
		}
	}
	dlg_info("=================================================================================\n");

	for(temp=0; temp<=25; temp++){
		dlg_info("Temperature Table test : (volt=%d, temp=%d)\n",
			adcmap_btm_threshold[temp].voltage,voltage_to_degree_temp1(adcmap_btm_threshold[temp].voltage));
	}
	dlg_info("Temperature Table test : (volt=0, temp=%d)\n",voltage_to_degree_temp1(0));
	dlg_info("=================================================================================\n");
#endif

	dlg_info("%s : probe end\n", __func__);

	return ret;

err_sysfs:
#ifdef CONFIG_USB_ODIN_DRD
	d2260_free_irq(d2260, D2260_IRQ_EVF, hwmon);
	cancel_delayed_work_sync(&hwmon->adc_work);
	wakeup_source_trash(&hwmon->wake_lock);
#endif
	sysfs_remove_group(&pdev->dev.kobj, &d2260_attr_group);
err:
	kfree(hwmon);
out:
	DIALOG_DEBUG(d2260->dev, "%s(HWMON registered) ret=%d\n",__FUNCTION__,ret);
	return ret;
}

static int d2260_hwmon_suspend(struct platform_device *pdev)
{
	struct d2260_hwmon *hwmon = platform_get_drvdata(pdev);
	struct d2260 *d2260 = hwmon->d2260;

	atomic_set(&hwmon->status_bat, D2260_HWMON_STATUS_SUSPEND);
	atomic_set(&hwmon->status_adc, D2260_HWMON_STATUS_SUSPEND);
	return 0;
}

static int d2260_hwmon_resume(struct platform_device *pdev)
{
	struct d2260_hwmon *hwmon = platform_get_drvdata(pdev);
	struct d2260 *d2260 = hwmon->d2260;

	if (atomic_read(&hwmon->status_bat) == D2260_HWMON_STATUS_QUEUE_WORK) {
		dlg_info("%s : <<<Battery Temperature Interrupt ON RESUME>>>\n", __func__);

		/* Force the Battery information update */
		bat_temp_force_update = 1;

		/* Update Battery Information Before OTP Drv */
		call_sysfs_fuel();

		/* Call LG Battery Temperature Scenario  */
		batt_temp_ctrl_start(0);
	}

	if (atomic_read(&hwmon->status_adc) == D2260_HWMON_STATUS_QUEUE_WORK) {
		if (hwmon->usb_init) {
#ifdef CONFIG_USB_G_LGE_ANDROID
			if (lge_get_factory_56k_130k_boot()) {
				schedule_delayed_work(&hwmon->adc_work, msecs_to_jiffies(300));
				__pm_wakeup_event(&hwmon->wake_lock, 800); /* 800ms Wake lock time */
			}
			else {
				schedule_delayed_work(&hwmon->adc_work, 0);
				__pm_wakeup_event(&hwmon->wake_lock, 500); /* 500ms Wake lock time */
			}
#else
			schedule_delayed_work(&hwmon->adc_work, 0);
			__pm_wakeup_event(&hwmon->wake_lock, 500); /* 500ms Wake lock time */
#endif
		}

	}

	atomic_set(&hwmon->status_adc, D2260_HWMON_STATUS_RESUME);
	atomic_set(&hwmon->status_bat, D2260_HWMON_STATUS_RESUME);

	return 0;
}

static int d2260_hwmon_remove(struct platform_device *pdev)
{
	struct d2260_hwmon *hwmon = platform_get_drvdata(pdev);
	struct d2260 *d2260 = hwmon->d2260;

	hwmon_device_unregister(hwmon->classdev);
#ifdef CONFIG_USB_ODIN_DRD
	d2260_free_irq(d2260, D2260_IRQ_EVF, hwmon);
	cancel_delayed_work_sync(&hwmon->adc_work);
	wakeup_source_trash(&hwmon->wake_lock);
#endif
	sysfs_remove_group(&pdev->dev.kobj, &d2260_attr_group);
	platform_set_drvdata(pdev, NULL);
	kfree(hwmon);
	g_hwmon = NULL;

	return 0;
}

static struct platform_driver d2260_hwmon_driver = {
	.probe = d2260_hwmon_probe,
	.suspend = d2260_hwmon_suspend,
	.resume = d2260_hwmon_resume,
	.remove = d2260_hwmon_remove,
	.driver = {
		.name = "d2260-hwmon",
		.owner = THIS_MODULE,
	},
};

static int __init d2260_hwmon_init(void)
{
	return platform_driver_register(&d2260_hwmon_driver);
}
module_init(d2260_hwmon_init);

static void __exit d2260_hwmon_exit(void)
{
	platform_driver_unregister(&d2260_hwmon_driver);
}
module_exit(d2260_hwmon_exit);

MODULE_AUTHOR("In Hyuk Kang <hugh.kang@lge.com>");
MODULE_AUTHOR("Tony Olech <anthony.olech@diasemi.com>");
MODULE_DESCRIPTION("D2260 Hardware Monitoring");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:d2260-hwmon");

