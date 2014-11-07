/*
 * max17048.c
 *
 * Copyright 2013 Maxim Integrated Products, Inc.
 * Gyungoh Yoo <jack.yoo@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/regmap.h>
#include <linux/regmap-ipc.h>
#include <linux/version.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/power/max17048.h>
#include <linux/mfd/max77819-charger.h>
#include <linux/mfd/d2260/pmic.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/debugfs.h>
#include <linux/platform_data/gpio-odin.h>
#include <linux/power/max17048.h>
#include <linux/board_lge.h>
#include <linux/reboot.h>
#include <linux/wakelock.h>
#include <linux/platform_data/gpio-odin.h>
#include <linux/kernel.h>

#include <linux/mfd/max77807/max77807-charger.h>
#define MAX17048_NAME	"max17048"

/*
               
                                                                               
                                
 */
#ifdef CONFIG_MACH_ODIN_LIGER
#define MAX17048_FUEL_I2C_SLOT      2
#else
#define MAX17048_FUEL_I2C_SLOT      5
#endif
/*              */
#define MAX17048_FUEL_SLAVE_ADDR    0x6C

/* registers */
#define MAX17048_VCELL		0x02
#define MAX17048_SOC		0x04
#define MAX17048_MODE		0x06
#define MAX17048_VERSION	0x08
#define MAX17048_HIBRT		0x0A
#define MAX17048_CONFIG		0x0C
#define MAX17048_VALRT		0x14
#define MAX17048_CRATE		0x16
#define MAX17048_VRESET		0x18
#define MAX17048_STATUS		0x1A
#define MAX17048_TABLE		0x40
#define MAX17048_CMD		0xFE

/* MAX17048_MODE */
#define MAX17048_QUICK_START	0x4000
#define MAX17048_ENSLEEP	0x2000
#define MAX17048_HIBSTAT	0x1000

/* MAX17048_CONFIG */
#define MAX17048_RCOMP		0xFF00
#define MAX17048_SLEEP		0x0080
#define MAX17048_ALMD		0x0040
#define MAX17048_ALRT		0x0020
#define MAX17048_ATHD		0x001F

/* MAX17048_VALRT */
#define MAX17048_MAX		0xFF00
#define MAX17048_MIN		0x00FF

/* MAX17048_STATUS */
#define MAX17048_ENVR		0x4000
#define MAX17048_MD		0x2000
#define MAX17048_HD		0x1000
#define MAX17048_VR		0x0800
#define MAX17048_VL		0x0400
#define MAX17048_VH		0x0200
#define MAX17048_RI		0x0100

/* MAX17048_MASKS */
#define CFG_ALRT_MASK    0x0020
#define CFG_ATHD_MASK    0x001F
#define CFG_ALSC_MASK    0x0040
#define CFG_RCOMP_MASK    0xFF00
#define CFG_RCOMP_SHIFT    8
#define CFG_ALSC_SHIFT   6
#define STAT_RI_MASK     0x0100
#define STAT_CLEAR_MASK  0xFF00

#define M2SH(m) ((m) & 0x0F ? ((m) & 0x03 ? ((m) & 0x01 ? 0 : 1) : ((m) & 0x04 ? 2 : 3)) : \
		((m) & 0x30 ? ((m) & 0x10 ? 4 : 5) : ((m) & 0x40 ? 6 : 7)))

/*
               
                                
                                
 */
#define MAX17048_WORK_DELAY	0
#define MAX17048_WORK_DELAY_WITH_CHARGER	3000
#define MAX17048_WORK_DELAY_WITHOUT_CHARGER	60000

#define DEFAULT_TEMP	200
/*              */

#define FG_STATUS_RESUME      0
#define FG_STATUS_SUSPEND     1
#define FG_STATUS_QUEUE_WORK  2

int f_data_voltage;
int f_data_soc;
int f_data_cap;
int f_data_temper;
static int f_inited = 0;
static int update_soc = 0;
static int update_cable_sts = 0;
static int fake_battery_soc = 0;
static int resume_flag = 0;

static struct max17048 *g_fg = NULL;
/*for board revision*/
static int batt_present = 1;
static int off_mode_status = 0;
static int off_mode_low_count = 0;
static int off_mode_low_flag = 0;
static int bat_id_vendor = 0;
static int i2c_error_count = 0;

static int real_temp = 200;
extern int d2260_adc_get_vichg(void);
extern void call_run_led_pattern(void);
extern int wcharger_conn_sts(void);

int get_voltage(void)
{
    return f_data_voltage;
}
int get_soc()
{
    return f_data_soc;
}
int get_cap()
{
    return f_data_cap;

}
int get_temper()
{
    return f_data_temper;
}

struct max17048
{
	struct device *dev;
	struct power_supply psy;
	struct power_supply	psy_batt_id;
	struct power_supply *ac;
	struct power_supply *usb;
	struct power_supply *wireless;
	struct regmap *regmap;
	struct delayed_work check_work;
	struct delayed_work isr_handle_work;
	struct workqueue_struct *fg_internal_wq;
	struct dentry *dent;
	struct wake_lock fg_worker_wakelock;
	unsigned int batt_id_info;
	unsigned int valid_batt_id_check;
	struct delayed_work fuel_int;
	unsigned short model_data[32];
	int soc;
	int irq;
/*
               
                                     
                                
 */
	u8 rcomp;
	int rcomp_co_hot;
	int rcomp_co_cold;
/*              */
	int health;
	atomic_t status;
};


static enum power_supply_property max17048_props[] =
{
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,	// healthd
	POWER_SUPPLY_PROP_CHARGE_COUNTER,	// healthd
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VALID_BATTERY_ID,
};

static enum power_supply_property lge_battery_id_battery_props[] = {
	POWER_SUPPLY_PROP_BATTERY_ID_CHECKER,
	POWER_SUPPLY_PROP_VALID_BATTERY_ID,
};

struct debug_reg {
	char  *name;
	u8  reg;
};

#define MAX77819_FG_DEBUG_REG(x) {#x, x##_REG}

static struct debug_reg max77819_fg_debug_regs[] = {
	/* 0x02 */MAX77819_FG_DEBUG_REG(MAX17048_VCELL),
	/* 0x04 */MAX77819_FG_DEBUG_REG(MAX17048_SOC),
	/* 0x06 */MAX77819_FG_DEBUG_REG(MAX17048_MODE),
	/* 0x08 */MAX77819_FG_DEBUG_REG(MAX17048_VERSION),
	/* 0x0A */MAX77819_FG_DEBUG_REG(MAX17048_HIBRT),
	/* 0x0C */MAX77819_FG_DEBUG_REG(MAX17048_CONFIG),
	/* 0x14 */MAX77819_FG_DEBUG_REG(MAX17048_VALRT),
	/* 0x16 */MAX77819_FG_DEBUG_REG(MAX17048_CRATE),
	/* 0x18 */MAX77819_FG_DEBUG_REG(MAX17048_VRESET),
	/* 0x1A */MAX77819_FG_DEBUG_REG(MAX17048_STATUS),
	/* 0x40 */MAX77819_FG_DEBUG_REG(MAX17048_TABLE),
	/* 0xFE */MAX77819_FG_DEBUG_REG(MAX17048_CMD),
};

/*
               
                                           
                                
 */
#ifdef CONFIG_MACH_ODIN_LIGER
static int bound_check(int max, int min, int val)
{
	val = max(min, val);
	val = min(max, val);
	return val;
}
#endif
/*              */

void call_sysfs_fuel()
{
	if(g_fg == NULL || f_inited == 0){
		printk("call_sysfs_fuel => g_fg is not initialized yet or driver probed not yet\n");
		return;
	}
	printk("call sysfs fuel \n");
	update_cable_sts = 1;
	cancel_delayed_work(&g_fg->check_work);
	queue_delayed_work(g_fg->fg_internal_wq, &g_fg->check_work, msecs_to_jiffies(MAX17048_WORK_DELAY));
}

void set_off_mode_flag(bool off_flag)
{
	if(off_flag == 1){
		printk("max77819-fg set off mode  as 1\n");
		off_mode_status = 1;
	}else{
		printk("max77819-fg set off mode  as 0\n");
		off_mode_status = 0;
	}

}
#define INIT_VAL 1000
static int fake_temp = INIT_VAL;
static ssize_t batt_temp_fake_temp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	ret = fake_temp;

	pr_info("%s: fake_temp = %d\n", __func__, fake_temp);
	return 0;
}

unsigned int fake_battery_set = 0;
static ssize_t batt_temp_fake_temp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int level;

	if (!count)
		return -EINVAL;

	level = simple_strtoul(buf, NULL, 10);

	if (level > 1000) {
		/* Fake Battery ON -> Set D2260 Hwmon Threshold MAX */
		fake_temp = 1000-level;
		d2260_hwmon_set_bat_threshold_max();
		set_fake_battery_flag(true);
		fake_battery_set = 1;
		batt_temp_ctrl_start(0);
	} else if(level == 1000) {
		/* Fake Battery OFF -> Start Battery OTP Work */
		fake_temp = level;
		set_fake_battery_flag(false);
		fake_battery_set = 0;
		batt_temp_ctrl_start(0);
	} else {
		/* Fake Battery ON -> Set D2260 Hwmon Threshold MAX */
		fake_temp = level;
		d2260_hwmon_set_bat_threshold_max();
		set_fake_battery_flag(true);
		fake_battery_set = 1;
		batt_temp_ctrl_start(0);
	}

	cancel_delayed_work(&g_fg->check_work);
	queue_delayed_work(g_fg->fg_internal_wq, &g_fg->check_work, msecs_to_jiffies(MAX17048_WORK_DELAY));

	pr_info("%s: fake_temp = %d\n", __func__, fake_temp);
	return count;
}

static ssize_t at_chg_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int r;
	union power_supply_propval ret = {0,};
	struct max17048 *me = dev_get_drvdata(dev);

	me->psy.get_property(&me->psy, POWER_SUPPLY_PROP_STATUS, &ret);

	pr_info("%s: charging status = %d\n", __func__, ret.intval);
	if (ret.intval == 1) {	// POWER_SUPPLY_STATUS_CHARGING
		r = snprintf(buf, 3, "%d\n", 1);
	} else {
		r = snprintf(buf, 3, "%d\n", 0);
	}

	return r;
}

static ssize_t at_chg_status_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct max17048 *me = dev_get_drvdata(dev);
	union power_supply_propval value = {0,};

	if (strncmp(buf, "0", 1) == 0) {
		/* stop charging */
		pr_info("%s: AT_CHARGE => itop charging\n", __func__);
		value.intval = 0;
	} else if (strncmp(buf, "1", 1) == 0) {
		/* start charging */
		pr_info("%s: AT_CHARGE => start charging\n", __func__);
		value.intval = 1;
	}
	me->ac->set_property(me->ac, POWER_SUPPLY_PROP_CHARGING_ENABLED, &value);

	return count;
}

static ssize_t at_chg_complete_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int r;
	int ret_val = 0;
	int chcomp_voltage = 0;
	unsigned int volt_val = 0;

	struct max17048 *me = dev_get_drvdata(dev);
	union power_supply_propval ret = {0,};
	me->psy.get_property(&me->psy, POWER_SUPPLY_PROP_CAPACITY, &ret);

	if(g_fg == NULL){
		printk("at_chg_complete_show g_fg is not initialized yet\n");
		return 0;
	}

	ret_val = regmap_read(g_fg->regmap, MAX17048_VCELL, &volt_val);
	if (likely(ret_val >= 0))
	{
		/* unit = 0.625mV */
		chcomp_voltage = (int)(volt_val >> 3) * 625;
	}

	printk("at_chg_complete_show Battery Voltage = %d\n",chcomp_voltage);

	if (chcomp_voltage >= 4250000) {
		r = snprintf(buf, 3, "%d\n", 1);
		pr_info("%s: guage = 100, buf = %s\n", __func__, buf);
	} else {
		r = snprintf(buf, 3, "%d\n", 0);
		pr_info("%s: guage < 100, buf = %s\n", __func__, buf);
	}

	return r;
}

static ssize_t at_chg_complete_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct max17048 *me = dev_get_drvdata(dev);
	union power_supply_propval value = {0,};

	if (strncmp(buf, "1", 1) == 0) {
		/* stop charging */
		pr_info("%s: AT_CHCOMP => stop charging\n", __func__);
		value.intval = 0;
	} else if (strncmp(buf, "0", 1) == 0) {
		/* start charging */
		pr_info("%s: AT_CHCOMP => start charging\n", __func__);
		value.intval = 1;
	}
	me->ac->set_property(me->ac, POWER_SUPPLY_PROP_CHARGING_ENABLED, &value);

	return count;
}

static ssize_t at_pmic_reset_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int r = 0;
	bool pm_reset = true;

	msleep(3000); /* for waiting return values of testmode */

	//machine_restart(NULL);
	kernel_restart(NULL);

	r = snprintf(buf, 3, "%d\n", pm_reset);

	return r;
}

static int max17048_set_reset(struct max17048 *chip)
{
	int r = 0;
	struct regmap *regmap = chip->regmap;

	r = regmap_write(regmap, MAX17048_MODE, MAX17048_QUICK_START);
	if (IS_ERR_VALUE(r)) {
		pr_err("%s: reg writing failed, r = %d\n", __func__, r);
		return r;
	}

	pr_info("%s: Reset (Quickstart)\n", __func__);
	return 0;
}

static int fuel_rst_status = 0;
static ssize_t at_fuel_reset_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	if (g_fg == NULL) {
		return -ENODEV;
	}

	if (strncmp(buf, "reset", 5) == 0) {
		fuel_rst_status = 1;
		cancel_delayed_work(&g_fg->check_work);
		max17048_set_reset(g_fg);
		//schedule_delayed_work(&g_fg->check_work, HZ);
		queue_delayed_work(g_fg->fg_internal_wq, &g_fg->check_work, HZ);
	} else {
		fuel_rst_status = 0;
		return -EINVAL;
	}

	return count;
}

extern int d2260_adc_get_usb_id(void);

static ssize_t at_usbid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int r = 0;
	int adc_value;

	adc_value = d2260_adc_get_usb_id();
	if (adc_value == -ETIMEDOUT) {
		/* reason: adc read timeout, assume it is open cable */
		pr_err("%s: Unable to read adc, adc_value = %d\n", __func__, adc_value);
		return r;
	}

	pr_info("%s: adc_value = %d\n", __func__, adc_value);
	r = sprintf(buf, "%d\n", adc_value);

	return r;
}

static ssize_t at_chargingmodeoff_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct max17048 *me = dev_get_drvdata(dev);
	union power_supply_propval value = {0,};

	/* charging mode off */
	pr_info("%s: charging mode off\n", __func__);
	value.intval = 0;

	me->ac->set_property(me->ac, POWER_SUPPLY_PROP_CHARGING_ENABLED, &value);

	return count;
}

static ssize_t at_battexist_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int r = 0;
	int batt_value = batt_present;
	r = sprintf(buf, "%d\n", batt_value);
	pr_info("%s: batt_value = %d\n", __func__, batt_value);

	return r;
}
static ssize_t offmode_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct max17048 *me = dev_get_drvdata(dev);
	union power_supply_propval value = {0,};

	/* charging mode off */
	if (strncmp(buf, "1", 1) == 0) {
		pr_info("%s: offmode \n", __func__);
		value.intval = 1;
	} else if (strncmp(buf, "0", 1) == 0) {
		pr_info("%s: nomal mode\n", __func__);
		value.intval = 0;
	}

	me->ac->set_property(me->ac, POWER_SUPPLY_PROP_OFF_MODE, &value);

	return count;
}

/* This function is only for SIDD Conversion START */
static int sidd_hexval(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0;
}

static long sidd_atol(const char *num)
{
	long value = 0;
	int neg = 0;

	if (num[0] == '0' && num[1] == 'x') {
		// hex
		num += 2;
		while (*num && isxdigit(*num))
			value = value * 16 + sidd_hexval(*num++);
	} else {
		// decimal
		if (num[0] == '-') {
			neg = 1;
			num++;
		}
		while (*num && isdigit(*num))
			value = value * 10 + *num++  - '0';
	}

	if (neg)
		value = -value;

	return value;
}
/* This function is only for SIDD Conversion END */

static ssize_t sidd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int r;
	r = snprintf(buf, 12, "%s\n", lge_get_sidd());
	return r;
}

static ssize_t sidd_ca15_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int r;
	char *temp;
	char ca15[6] = {0};
	unsigned int ca15_int = 0;

	temp = lge_get_sidd();
	//printk("sidd_ca15_show temp= %s\n",temp);

	ca15[0] = '0';
	ca15[1] = 'x';
	ca15[2] = temp[1];
	ca15[3] = temp[2];
	ca15[4] = temp[3];
	ca15[5] = '\0';

	//printk("sidd_ca15_show = %s\n",ca15);
	ca15_int = sidd_atol(ca15);
	//printk("sidd_ca15_show dec= %d\n",ca15_int);
	r = snprintf(buf, 6, "%d\n", ca15_int);
	return r;
}

static ssize_t sidd_gpu_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int r;
	char *temp;
	char gpu[6] = {0};
	unsigned int gpu_int = 0;

	temp = lge_get_sidd();
	//printk("sidd_gpu_show temp= %s\n",temp);

	gpu[0] = '0';
	gpu[1] = 'x';
	gpu[2] = temp[5];
	gpu[3] = temp[6];
	gpu[4] = temp[7];
	gpu[5] = '\0';

	//printk("sidd_gpu_show = %s\n",gpu);
	gpu_int = sidd_atol(gpu);
	//printk("sidd_gpu_show dec = %d\n",gpu_int);
	r = snprintf(buf, 6, "%d\n", gpu_int);
	return r;
}

static ssize_t volt_base_soc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int r = 0;
	int ret_val = 0;
	int soc_value = 0;
	int voltage_raw = 0;
	int voltage_now = 0;

	if(g_fg == NULL){
		printk("volt_base_soc_show g_fg is not initialized yet\n");
		return 0;
	}

	ret_val = regmap_read(g_fg->regmap, MAX17048_VCELL, &voltage_raw);
	if (likely(ret_val >= 0))
	{
		/* unit = 0.625mV */
		voltage_now = (int)(voltage_raw >> 3) * 625;
	}

	printk("volt_base_soc_show Battery Voltage = %d\n",voltage_now);

	if(voltage_now >= 4250000) {
		soc_value = 100;
	} else if((voltage_now >= 4200000) && (voltage_now < 4250000)) {
		soc_value = 96;
	} else if((voltage_now >= 4000000) && (voltage_now < 4200000)) {
		soc_value = 85;
	} else if((voltage_now >= 3950000) && (voltage_now < 4000000)) {
		soc_value = 78;
	} else if((voltage_now >= 3880000) && (voltage_now < 3950000)) {
		soc_value = 66;
	} else if((voltage_now >= 3820000) && (voltage_now < 3880000)) {
		soc_value = 54;
	} else if((voltage_now >= 3790000) && (voltage_now < 3820000)) {
		soc_value = 45;
	} else if((voltage_now >= 3750000) && (voltage_now < 3790000)) {
		soc_value = 34;
	} else if((voltage_now >= 3730000) && (voltage_now < 3750000)) {
		soc_value = 23;
	} else if((voltage_now >= 3660000) && (voltage_now < 3730000)) {
		soc_value = 11;
	} else {
		soc_value = 0;
	}

	pr_info("%s: soc_value = %d\n", __func__, soc_value);
	r = sprintf(buf, "%d\n", soc_value);

	return r;
}

static ssize_t real_temp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", d2260_get_battery_temp());
}

DEVICE_ATTR(at_charge, 0644, at_chg_status_show, at_chg_status_store);
DEVICE_ATTR(at_chcomp, 0644, at_chg_complete_show, at_chg_complete_store);
DEVICE_ATTR(at_pmrst, 0440, at_pmic_reset_show, NULL);
DEVICE_ATTR(at_fuelrst, 0200, NULL, at_fuel_reset_store);
DEVICE_ATTR(at_usbid, 0444, at_usbid_show, NULL);
DEVICE_ATTR(at_chargingmodeoff, 0200, NULL, at_chargingmodeoff_store);
DEVICE_ATTR(at_battexist, 0444, at_battexist_show, NULL);
DEVICE_ATTR(offmode, 0200, NULL,offmode_store);
DEVICE_ATTR(fake_temp, 0664, batt_temp_fake_temp_show, batt_temp_fake_temp_store);
DEVICE_ATTR(sidd, 0444, sidd_show, NULL);
DEVICE_ATTR(sidd_ca15, 0444, sidd_ca15_show, NULL);
DEVICE_ATTR(sidd_gpu, 0444, sidd_gpu_show, NULL);
DEVICE_ATTR(volt_base_soc, 0444, volt_base_soc_show, NULL);
DEVICE_ATTR(real_temp, 0444, real_temp_show, NULL);

static int max17048_masked_write_reg(struct max17048 *max17048, int reg, u16 mask, u16 val)
{
	s32 rc;
	u32 temp;
	int ret;

	ret = regmap_read(max17048->regmap, reg, &temp);
	if(ret < 0) {
		pr_err("max17048_read_reg failed: reg=%03X, rc=%d\n", reg, temp);
		return ret;
	}

	if(((u16)temp & mask) == (val & mask))
		return 0;

	temp &= ~mask;
	temp |= val & mask;
	rc = regmap_write(max17048->regmap, reg, temp);
	if(rc) {
		pr_err("max17048_write_reg failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}

	return 0;
}

static int max17048_get_status(struct max17048 *max17048)
{
	union power_supply_propval ret = {0,};

	if (max17048->ac == NULL || max17048->usb == NULL || max17048->wireless == NULL) {
		pr_err("max17048: ac/usb/wireless is not registered\n");
		return -1;
	}

	/* MAX77807_Charger Drv VBUS Status */
	if (max77807_odin_charger_enable == 1) {
		ret.intval = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		ret.intval = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	return ret.intval;
}

#define ERROR_Tolerance 10
static int old_soc = 0;
static int probe_set = 0;
static int old_soc_for_wlc = 0;
static int max17048_get_soc(struct max17048 *max17048)
{
	u8 buf[2];
	int batt_soc = 0;
	int diff_val= 0;
	int ret = -EINVAL;
	int chrg_sts = 0;
	unsigned int empty_soc = 0;

	buf[0] = (max17048->soc & 0x0000FF00) >> 8;
	buf[1] = (max17048->soc & 0x000000FF);

	pr_debug("%s: SOC raw = 0x%x%x\n", __func__, buf[0], buf[1]);

	batt_soc = (((int)buf[0]*256)+buf[1])*19531;

	if(off_mode_status == 1)
		batt_soc = (batt_soc - (empty_soc * 1000000))	/((980 - empty_soc) * 10000);
	else
		batt_soc = (batt_soc - (empty_soc * 1000000))	/((970 - empty_soc) * 10000);

	/* Wireless Charging (99%<->100%) Swing Issue Debugging */
	if((batt_soc >= 100) && (old_soc_for_wlc == 99) && (wcharger_conn_sts() == 1) && (f_data_voltage < 4330000)) {
		printk("max17048_get_soc <wlc swing issue occured> => adust soc\n");
		batt_soc = 99;
	}

	/* 1% Power OFF voltage adjust */
	chrg_sts = max17048_get_status(max17048);
	if((batt_soc == 1) && (f_data_voltage >= 3450000) && (chrg_sts != POWER_SUPPLY_STATUS_CHARGING)){
		printk("Low_batt_soc= (%d) chip->voltage = (%d)\n",batt_soc,(f_data_voltage/1000));
		batt_soc = 2;
		//schedule_delayed_work(&max17048->check_work, msecs_to_jiffies(10000));
		queue_delayed_work(g_fg->fg_internal_wq, &g_fg->check_work, msecs_to_jiffies(10000));
	}

	batt_soc = bound_check(100, 0, batt_soc);

	/* Display Battery Capacity(100)% without Battery for MILKY Way*/
	if(!batt_present) {
		batt_soc = 100;
	}

	/* Hidden Menu Fake Battery SOC set 80% */
	if(fake_battery_soc) {
		printk("max17048_get_soc -> fake soc set to 80 percent, original_soc:%d, original_temp:%d\n",batt_soc,real_temp);
		batt_soc = 80;
	}

	old_soc_for_wlc = batt_soc;
	return batt_soc;
}

static int max17048_get_capacity(struct max17048 *chip, int *soc)
{
	int raw_soc;
	int ret = -EINVAL;

	ret = regmap_read(chip->regmap, MAX17048_SOC, &raw_soc);
	if (likely(ret >= 0)) {
		if(raw_soc < 0) {
			printk("max17048_get_capacity <<< Negative SOC error >>>\n");
			return raw_soc;
		} else {
			chip->soc = raw_soc;
			*soc = max17048_get_soc(chip);
		}
	} else {
		printk("max17048_get_capacity <<< regmap_read_error >>>\n");
		return ret;
	}

	return 0;
}

static int Old_Bat_temp = 200;
static int max17048_get_temperature(struct max17048 *max17048, int *temp)
{
	/* Default Battery Temperature = 20'C Room Temperature*/
	int bat_temp = 200;

	/* Fake Battery Mode for Factory Test */
	if (fake_temp != INIT_VAL) {
		*temp = fake_temp;
		//printk("max17048_get_temperature : Fake Battery Mode is ON\n");
		fake_battery_soc = 1; /* set soc 80% */
		real_temp = d2260_get_battery_temp();
	} else {
		fake_battery_soc = 0; /* set real soc(%) */

		bat_temp = d2260_get_battery_temp();

		/* Battery Present Check */
		if(bat_temp < -300) {
			printk("max17048_get_temperature : under -300'C , read one more time to be sure \n");
			bat_temp = d2260_get_battery_temp();

			if(bat_temp < -300) {
				batt_present = 0;
			}
		} else {
			batt_present = 1;
		}

		/* D2260 HWMON ADC Exception Handling */
		if(bat_temp > 1000) {
			printk("max17048_get_temperature : Over 100'C due to ADC ERROR -> Keep Bat Temp\n");
			*temp = Old_Bat_temp;
		} else {
			Old_Bat_temp = bat_temp;
			*temp = bat_temp;
		}
	}
	return 0;
}

static int max17048_set_rcomp(struct max17048 *max17048, int temp)
{
	int ret;
	int val;
	int scale_coeff;
	unsigned int ttemp;
	unsigned int rcomp;

	ttemp = temp / 10;

	if(bat_id_vendor==1) { //LG CHEM
		if (ttemp > 20)
			scale_coeff = 750;	/*HOT*/
		else if (ttemp < 20)
			scale_coeff = 5225; /* COLD */
		else
			scale_coeff = 0;

		rcomp = (102) * 1000 - (ttemp-20) * scale_coeff; /* 102 = RCOMP Value */
	} else { // TECHNOHILL
		if (ttemp > 20)
			scale_coeff = 1700;	/*HOT*/
		else if (ttemp < 20)
			scale_coeff = 3425; /* COLD */
		else
			scale_coeff = 0;

		rcomp = (117) * 1000 - (ttemp-20) * scale_coeff; /* 117 = RCOMP Value */
	}

	rcomp = bound_check(255, 0 , rcomp / 1000);

	rcomp = rcomp << 8;

	ret = regmap_update_bits(max17048->regmap,
			MAX17048_CONFIG, MAX17048_RCOMP, rcomp);

	if (ret < 0)
		dev_err(max17048->dev, "cannot write RCOMP. (%d)\n", ret);

	return ret;
}

static int max17048_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct max17048 *max17048 = container_of(psy, struct max17048, psy);
	struct regmap *regmap = max17048->regmap;
	unsigned int value;
	unsigned short tmp;
	int ret=0;

	switch (psp)
	{
	case POWER_SUPPLY_PROP_STATUS:
		if(f_data_cap == 100) {
			if (max77807_odin_charger_enable == 0) {
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			} else {
				val->intval = POWER_SUPPLY_STATUS_FULL;
			}
		} else {
			val->intval = max17048_get_status(max17048);
		}
		break;

	case POWER_SUPPLY_PROP_HEALTH:

		if (g_fg) {
			val->intval = g_fg->health;
		}
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = regmap_read(max17048->regmap, MAX17048_VCELL, &value);
		if (likely(ret >= 0))
			/* 78.125uV/cell */
			val->intval = value / 8 * 625;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:	// dummy
		val->intval = 500;
		break;

	case POWER_SUPPLY_PROP_CHARGE_COUNTER:	// dummy
		val->intval = 1;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
#if 0 /* Setting Battery SOC(%) and Incicator Battery SOC(%) Sync Issue */
		ret = max17048_get_capacity(max17048, &value);
		if (likely(ret >= 0))
			val->intval = value;
#else
		val->intval = f_data_cap;
#endif
		break;

	case POWER_SUPPLY_PROP_TEMP:

	if(lge_get_boot_mode()== LGE_BOOT_MODE_FACTORY){
			val->intval = 250;

	}else{
		ret = max17048_get_temperature(max17048, &value);
		if (likely(ret >= 0))
			val->intval = value;
	}
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = batt_present;
		break;

	case POWER_SUPPLY_PROP_VALID_BATTERY_ID:
		if(lge_get_factory_boot()){
			val->intval = 1;
		} else {
			val->intval = max17048->valid_batt_id_check;
		}
		break;

	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int lge_battery_id_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct max17048 *max17048 = container_of(psy, struct max17048, psy_batt_id);

	switch (psp) {
		case POWER_SUPPLY_PROP_BATTERY_ID_CHECKER:
			val->intval = max17048->batt_id_info;
			break;
		case POWER_SUPPLY_PROP_VALID_BATTERY_ID:
			val->intval = max17048->valid_batt_id_check;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}
static int max17048_set_property (struct power_supply *psy,	enum power_supply_property psp, const union power_supply_propval *val)
{

	struct max17048 *max17048 = container_of(psy, struct max17048, psy);
	switch (psp) {
		case POWER_SUPPLY_PROP_HEALTH:
			if (g_fg) {
				g_fg->health = val->intval ;
			}
			break;

		default:
			return -EINVAL;
	}
	return 0;
}

static int max17048_write_model_data(struct max17048 *max17048)
{
	struct device *dev = max17048->psy.dev;
	struct regmap *regmap = max17048->regmap;
	int ret;

	/* Unlock Model Data */
	ret = regmap_write(regmap, 0x3E, 0x4A57);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "Unlock Model Data failed. (%d)\n", ret);
		return ret;
	}

	/* Write Model Data */
	ret = regmap_bulk_write(regmap, MAX17048_TABLE, max17048->model_data,
			ARRAY_SIZE(max17048->model_data));
	if (IS_ERR_VALUE(ret))
		dev_err(dev, "Can't write Model Data (%d)\n", ret);

	/* Lock Model Data */
	ret = regmap_write(regmap, 0x3E, 0x0000);
	if (IS_ERR_VALUE(ret))
		dev_err(dev, "Lock Model Data failed. (%d)\n", ret);

	return ret;
}

static int max17048_clear_interrupt(struct max17048 *chip)
{
	int ret;

	//pr_info("%s.\n", __func__);

	ret = max17048_masked_write_reg(g_fg,
			MAX17048_CONFIG, CFG_ALRT_MASK, 0);
	if (ret < 0) {
		pr_err("%s: failed to clear alert status bit\n", __func__);
		return ret;
	}

	ret = max17048_masked_write_reg(g_fg,
			MAX17048_STATUS, STAT_CLEAR_MASK, 0);
	if (ret < 0) {
		pr_err("%s: failed to clear status reg\n", __func__);
		return ret;
	}

	return 0;
}

static int max17048_next_alert_level(int level)
{
	int next_level;
	printk(KERN_DEBUG "max17048_next_alert_level : Current SOC_level = %d\n",level);

	if(level >= 15){
		next_level = 30; //15
	}else if(level <15 && level >=5){
		next_level = 10; //5
	}else if(level <5 && level >=0){
		next_level = 0;  //0
	}else{}

	printk(KERN_DEBUG "max17048_next_alert_level : Next_alert_level = %d\n",next_level/2);
	return next_level;
}

static int max17048_set_athd_alert(struct max17048 *chip, int level)
{
	int ret;

	pr_info("%s.\n", __func__);

	level = bound_check(32, 1, level);
	level = 32 - level;

	ret = max17048_masked_write_reg(g_fg,
			MAX17048_CONFIG, CFG_ATHD_MASK, level);
	if (ret < 0)
		pr_err("%s: failed to set athd alert\n", __func__);

	return ret;
}

static int max17048_set_alsc_alert(struct max17048 *chip, bool enable)
{
	int ret;
	u16 val;

	pr_info("%s. with %d\n", __func__, enable);

	val = (u16)(!!enable << CFG_ALSC_SHIFT);

	ret = max17048_masked_write_reg(g_fg,
			MAX17048_CONFIG, CFG_ALSC_MASK, val);
	if (ret < 0)
		pr_err("%s: failed to set alsc alert\n", __func__);

	return ret;
}

static irqreturn_t max17048_isr(int irq, void *data)
{
	struct max17048 *max17048 = data;
	int ret;

	if(atomic_read(&max17048->status) == FG_STATUS_SUSPEND) {
		atomic_set(&max17048->status, FG_STATUS_QUEUE_WORK);
	} else {
		queue_work(max17048->fg_internal_wq, &max17048->isr_handle_work);
	}

	return IRQ_HANDLED;
}

static int max17048_hw_init(struct max17048 *max17048, struct max17048_platform_data *pdata)
{
	struct regmap *regmap = max17048->regmap;
	int ret;
	int init_config;
	int init_status;
	int init_valrt;

/*
               
                                                   
                                
 */
#ifdef CONFIG_MACH_ODIN_LIGER
	ret = regmap_update_bits(regmap, MAX17048_CONFIG, MAX17048_ATHD,
			((pdata->empty_alert_threshold / 20000) << M2SH(MAX17048_ATHD)));
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max17048->dev, "cannot write ATHD. (%d)\n", ret);
		return ret;
	}

	ret = regmap_update_bits(regmap, MAX17048_CONFIG,MAX17048_ALMD, 1);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max17048->dev, "cannot write ALSC. (%d)\n", ret);
		return ret;
	}
#else
	ret = regmap_update_bits(regmap, MAX17048_CONFIG, MAX17048_ATHD,
			((pdata->empty_alert_threshold / 20000) << M2SH(MAX17048_ATHD)));
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max17048->dev, "cannot write ATHD. (%d)\n", ret);
		return ret;
	}

	ret = regmap_update_bits(regmap, MAX17048_CONFIG,MAX17048_ALMD, 1);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max17048->dev, "cannot write ALSC. (%d)\n", ret);
		return ret;
	}
#endif
/*              */

	ret = regmap_read(max17048->regmap, MAX17048_CONFIG, &init_config);
	ret = regmap_read(max17048->regmap, MAX17048_STATUS, &init_status);
	ret = regmap_read(max17048->regmap, MAX17048_VALRT, &init_valrt);
	pr_info("%s config=(%X) status=(%X) valrt=(%X)\n", __func__,init_config,init_status,init_valrt);

	return ret;
}

extern unsigned char bat_temp_force_update;
extern unsigned char VBUS_STATUS;
static void max17048_fuelgauge_worker(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct max17048 *max17048 = container_of(dwork, struct max17048, check_work);
	int ret;
	unsigned int val;

	int voltage = -1;
	int capacity = -1;
	int soc = -1;
	int temp = -1;
	int vichg =0;

	wake_lock(&g_fg->fg_worker_wakelock);

	if (!max17048->ac) {
		max17048->ac = power_supply_get_by_name("ac");
		if (!max17048->ac) {
			pr_err("max17048 : ac supply not found\n");
		}
	}

	if (!max17048->usb) {
		max17048->usb = power_supply_get_by_name("usb");
		if (!max17048->usb) {
			pr_err("max17048 : usb supply not found\n");
		}
	}

	if (!max17048->wireless) {
		max17048->wireless= power_supply_get_by_name("wireless");
		if (!max17048->wireless) {
			pr_err("max17048 : wireless supply not found\n");
		}
	}

	/* Battery Voltage Update */
	ret = regmap_read(max17048->regmap, MAX17048_VCELL, &val);
	if (likely(ret >= 0))
	{
		/* unit = 0.625mV */
		voltage = (int)(val >> 3) * 625;
		f_data_voltage = voltage;
	}

	/* Battery SOC Update */
	ret = max17048_get_capacity(max17048, &val);
	if (likely(ret >= 0))
	{
		i2c_error_count = 0;
		if((val == 0) && (f_data_cap > 10)) {
			capacity = f_data_cap;
			printk("max17048_fuelgauge_worker =>  << SOC READ ERROR OCCURED (Abnormal Zero Value) >>\n");
		} else {
			capacity = val;
			f_data_cap = capacity;
		}
	} else {
		i2c_error_count++;
		printk("max17048_fuelgauge_worker =>  << SOC READ ERROR OCCURED ret = (%d)(%d)>>\n",ret, i2c_error_count);

		if(i2c_error_count == 3) {
			/* Hidden Reset for I2C Error Restore */
			printk("max17048_fuelgauge_worker =>  << Hidden Reset for I2C Restore >>\n");
			BUG();
		}
	}

	/* Battery Temperature Update */
	if(lge_get_boot_mode()== LGE_BOOT_MODE_FACTORY){
			temp = 250;
			f_data_temper =  temp;
	}else{
		ret = max17048_get_temperature(max17048, &val);
		if (likely(ret >= 0))
		{
			temp = val;
			f_data_temper =  temp;
		}
	}

	/* Battery Temperature Compensation */
	ret = max17048_set_rcomp(max17048, temp);
	if(ret)
		pr_err("%s: failed to set rcomp\n", __func__);

#if 0
	vichg =( d2260_adc_get_vichg() * 100) / 141 ;
#endif

	/* Update Sysfs only when SOC(%) Changed */
	if((update_cable_sts) || (bat_temp_force_update) || (update_soc != capacity)) {
		update_soc = capacity;
		power_supply_changed(&max17048->psy);
		pr_err("%s: (%d)mV (%d.%d)'C soc(%d) -> FW_SYNC(chg = %d)(temp = %d) \n", __func__, voltage, temp/10, temp%10, capacity,update_cable_sts,bat_temp_force_update);
		bat_temp_force_update = 0;
		update_cable_sts = 0;
	} else {
		pr_err("%s: (%d)mV (%d.%d)'C soc(%d) \n", __func__, voltage, temp/10, temp%10, capacity);
	}

	/* Run work every 2min */
	if(lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO){
		queue_delayed_work(g_fg->fg_internal_wq, &g_fg->check_work, msecs_to_jiffies(MAX17048_WORK_DELAY_WITH_CHARGER));
	}else
	queue_delayed_work(g_fg->fg_internal_wq, &g_fg->check_work, msecs_to_jiffies(MAX17048_WORK_DELAY_WITHOUT_CHARGER));

	if(off_mode_status == 1 && capacity == 100 ){
		printk("max17048 led update for chargerlogo\n");
		call_run_led_pattern();
		off_mode_status = 2;

	}
	wake_unlock(&g_fg->fg_worker_wakelock);
}

static void max17048_isr_handle_worker(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct max17048 *max17048 = container_of(dwork, struct max17048, isr_handle_work);

	unsigned int config, status;
	int ret = 0;

	ret = regmap_read(max17048->regmap, MAX17048_CONFIG, &config);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(max17048->psy.dev, "cannot read CONFIG. (%d)\n", ret);
		return IRQ_HANDLED;
	}

	ret = regmap_read(max17048->regmap, MAX17048_STATUS, &status);
	if (IS_ERR_VALUE(ret))
		dev_err(max17048->psy.dev, "cannot read STATUS. (%d)\n", ret);

	pr_info("%s config=(%X) status=(%X)\n", __func__,config,status);

	/* Update Battery Information */
	queue_work(g_fg->fg_internal_wq, &g_fg->check_work);

	/* Clear Fuelgauge Interrupt */
	max17048_clear_interrupt(max17048);
}

#ifdef CONFIG_OF
static struct max17048_platform_data *max17048_parse_dt(struct device *dev)
{
	struct max17048_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

/*
               
                                     
                                
 */
#ifndef CONFIG_MACH_ODIN_LIGER
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
	if (of_property_read_u32(np, "rcomp", &value))
		value = 102;
	pdata->rcomp = value;

#else
	if (of_property_read_u8(np, "rcomp", &pdata->rcomp))
		pdata->rcomp = 102;
#endif

#else
	if (of_property_read_u32(np, "rcomp", &pdata->rcomp))
		pdata->rcomp = 102;

	if (of_property_read_u32(np, "rcomp-co-hot", &pdata->rcomp_co_hot))
		pdata->rcomp_co_hot = 750;

	if (of_property_read_u32(np, "rcomp-co-cold", &pdata->rcomp_co_cold))
		pdata->rcomp_co_cold = 5225;

#endif

	if (of_property_read_u32(np, "empty-alert-threshold", &pdata->empty_alert_threshold))
		pdata->empty_alert_threshold = 4;
	if (unlikely(pdata->empty_alert_threshold > 31))
	{
		dev_err(dev, "Invalid empty-alert-threshold : %d\n", pdata->empty_alert_threshold);
		return ERR_PTR(-EINVAL);
	}

	if (of_property_read_u32(np, "voltage-alert-threshold", &pdata->voltage_alert_threshold))
		pdata->empty_alert_threshold = 0;
	if (unlikely(pdata->empty_alert_threshold > 5120000))
	{
		dev_err(dev, "Invalid voltage-alert-threshold : %d\n", pdata->voltage_alert_threshold);
		return ERR_PTR(-EINVAL);
	}
/*              */

	pdata->irq_gpio = of_get_named_gpio(np, "max77819_fg,num_gpio", 0);

	if (of_property_read_u32(np, "num_gpio", &pdata->irq_gpio))
		pdata->irq_gpio = -1;

	if (pdata->irq_gpio < 0) {
		pdata->irq = irq_of_parse_and_map(np, 0);
		pr_info("%s: irq_of_parse_and_map ret with %d forced set 202", __func__, pdata->irq);
		pdata->irq = 202;
	} else {
		ret = gpio_request(pdata->irq_gpio, MAX17048_NAME"-irq");
		if (unlikely(IS_ERR_VALUE(ret))) {
			pr_err("%s: failed to request gpio %u [%d]\n", __func__, pdata->irq_gpio, ret);
			pdata = ERR_PTR(ret);
			goto out;
		}

		gpio_direction_input(pdata->irq_gpio);
		pr_info("%s: INT_GPIO %u assigned\n", __func__, pdata->irq_gpio);

		/* override pdata irq */
		pdata->irq = gpio_to_irq(pdata->irq_gpio);
	}

	pr_info("%s: property:INTGPIO %d\n", __func__, pdata->irq_gpio);
	pr_info("%s: property:IRQ     %d\n", __func__, pdata->irq);

out:
	return pdata;
}
#endif

const struct regmap_config max17048_regmap_config =
{
	.reg_bits = 8,
	.val_bits = 16,
	.val_format_endian = REGMAP_ENDIAN_BIG,
};

static int set_reg(void *data, u64 val)
{
	struct max17048 *me;
	u32 addr = (u32) data;
	int ret;

	if (g_fg) {
		me = g_fg;
	}
	else{
		printk("==g_fg set_reg is not initialized yet==\n");
		return;
	}

	ret = regmap_write(me->regmap, addr, (u32) val);

	return ret;
}

static int get_reg(void *data, u64 *val)
{
	struct max17048 *me;
	u32 addr = (u32) data;
	u32 temp;
	int ret;

	if (g_fg) {
		me = g_fg;
	}
	else{
		printk("==g_fg get_reg is not initialized yet==\n");
		return;
	}

	ret = regmap_read(me->regmap, addr, &temp);

	if (ret < 0)
		return ret;

	*val = temp;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");


static int max77819_fg_create_debugfs_entries(struct max17048 *chip)
{
	int i;

	chip->dent = debugfs_create_dir(MAX17048_NAME, NULL);
	if (IS_ERR(chip->dent)) {
		pr_err("max77819_fg driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(max77819_fg_debug_regs) ; i++) {
		char *name = max77819_fg_debug_regs[i].name;
		u32 reg = max77819_fg_debug_regs[i].reg;
		struct dentry *file;

		file = debugfs_create_file(name, 0644, chip->dent,
					(void *) reg, &reg_fops);
		if (IS_ERR(file)) {
			pr_err("debugfs_create_file %s failed.\n", name);
			return -EFAULT;
		}
	}
	return 0;
}


static int __init max17048_probe(struct platform_device *pdev)
{
	struct ipc_client *regmap_ipc;
	struct device *dev = &pdev->dev;
	struct max17048 *max17048;
	struct max17048_platform_data *pdata;
	int ret = 0;

	max17048 = devm_kzalloc(dev, sizeof(*max17048), GFP_KERNEL);
	if (unlikely(max17048 == NULL))
		return -ENOMEM;

	printk("%s: start!!\n",__func__);

#ifdef CONFIG_OF
	pdata = max17048_parse_dt(dev);
	if (unlikely(IS_ERR(pdata)))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev);
#endif

	max17048->irq = pdata->irq;
	memcpy(max17048->model_data, pdata->model_data,
		ARRAY_SIZE(pdata->model_data) * sizeof(pdata->model_data[0]));
	max17048->dev = dev;
	max17048->irq = gpio_to_irq(116);
	if(max17048->irq < 0){
		dev_err(dev, "failed to get gpio irq\n");
		return -ENOMEM;
	}

/*
               
                                       
                                
 */
#ifdef CONFIG_MACH_ODIN_LIGER
	max17048->rcomp = pdata->rcomp;
	max17048->rcomp_co_hot = pdata->rcomp_co_hot;
	max17048->rcomp_co_cold = pdata->rcomp_co_cold;
#endif
/*              */

	//max17048->psy.name		= MAX17048_NAME;
	max17048->psy.name		= "battery";
	max17048->psy.type		= POWER_SUPPLY_TYPE_BATTERY;
	max17048->psy.get_property	= max17048_get_property;
	max17048->psy.set_property	= max17048_set_property;
	max17048->psy.properties	= max17048_props;
	max17048->psy.num_properties	= ARRAY_SIZE(max17048_props);


	max17048->psy_batt_id.name		= "battery_id";
	max17048->psy_batt_id.type		= POWER_SUPPLY_TYPE_BATTERY;
	max17048->psy_batt_id.get_property	= lge_battery_id_get_property;
	max17048->psy_batt_id.properties	= lge_battery_id_battery_props;
	max17048->psy_batt_id.num_properties= ARRAY_SIZE(lge_battery_id_battery_props);

    regmap_ipc = kzalloc(sizeof(struct ipc_client), GFP_KERNEL);
    if (regmap_ipc == NULL) {
        pr_err("%s : Platform data not specified.\n", __func__);
        return -ENOMEM;
    }

	regmap_ipc->slot = MAX17048_FUEL_I2C_SLOT;
	regmap_ipc->addr = MAX17048_FUEL_SLAVE_ADDR;
	regmap_ipc->dev  = dev;

	max17048->regmap = devm_regmap_init_ipc(regmap_ipc, &max17048_regmap_config);
	if (IS_ERR(max17048->regmap))
		return PTR_ERR(max17048->regmap);

	dev_set_drvdata(dev, max17048);

	probe_set = 0;
	ret = max17048_get_capacity(max17048, &old_soc);
	if (likely(ret >= 0)) {
		printk("max17048 first SOC setting = %d\n",old_soc);
	}
	probe_set = 1;

	g_fg = max17048;

	ret = max17048_hw_init(max17048, pdata);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "H/W init failed. (%d)\n", ret);
		return ret;
	}

	printk("max17048 get charger driver POWER_SUPPLY\n");
	max17048->ac = power_supply_get_by_name("ac");
	if (!max17048->ac) {
			pr_err("max17048 : ac supply not found\n");
	}

	max17048->usb = power_supply_get_by_name("usb");
	if (!max17048->usb) {
			pr_err("max17048 : usb supply not found\n");
	}

	max17048->wireless= power_supply_get_by_name("wireless");
	if (!max17048->wireless) {
			pr_err("max17048 : wireless supply not found\n");
	}

	printk("max17048 POWER_SUPPLY Register\n");
	ret = power_supply_register(dev, &max17048->psy);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "failed: power supply register (%d)\n", ret);
		return ret;
	}

	ret = power_supply_register(dev, &max17048->psy_batt_id);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, "failed: power supply register (%d)\n", ret);
		return ret;
	}

	max17048->fg_internal_wq = create_workqueue("fuelgauge_internal_workqueue");

	atomic_set(&max17048->status, FG_STATUS_RESUME);

	INIT_DELAYED_WORK(&max17048->check_work, max17048_fuelgauge_worker);
	INIT_DELAYED_WORK(&max17048->isr_handle_work,max17048_isr_handle_worker);

	/* Odin Code */
	ret = odin_gpio_sms_request_threaded_irq(max17048->irq, NULL, max17048_isr,
			IRQF_ONESHOT | IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND, "max17048", max17048);
	if (IS_ERR_VALUE(ret))
	{
		power_supply_unregister(&max17048->psy);
		dev_info(dev, "%s <<<request_irq_failed>>>\n",__func__);
		return ret;
	}

	/* Fake Battery */
	device_create_file(&pdev->dev, &dev_attr_fake_temp);

	/* SIDD */
	device_create_file(&pdev->dev, &dev_attr_sidd);
	device_create_file(&pdev->dev, &dev_attr_sidd_ca15);
	device_create_file(&pdev->dev, &dev_attr_sidd_gpu);

	/* ATcmd */
	ret = device_create_file(&pdev->dev, &dev_attr_at_charge);
	if (ret < 0) {
		pr_err("%s:File dev_attr_at_charge creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_charge;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_at_chcomp);
	if (ret < 0) {
		pr_err("%s:File dev_attr_at_chcomp creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_chcomp;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_at_pmrst);
	if (ret < 0) {
		pr_err("%s:File dev_attr_at_pmrst creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_pmrst;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_at_fuelrst);
	if (ret < 0) {
		pr_err("%s:File dev_attr_at_fuelrst creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_fuelrst;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_at_usbid);
	if (ret < 0) {
		pr_err("%s:File dev_attr_at_usbid creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_usbid;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_at_chargingmodeoff);
	if (ret < 0) {
		pr_err("%s:File dev_attr_at_chargingmodeoff creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_chargingmodeoff;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_at_battexist);
	if (ret < 0) {
		pr_err("%s:File dev_attr_at_battexist creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_battexist;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_offmode);
	if (ret < 0) {
		pr_err("%s:File dev_attr_at_battexist creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_offmode;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_volt_base_soc);
	if (ret < 0) {
		pr_err("%s:File dev_attr_at_volt_base_soc creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_volt_base_soc;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_real_temp);
	if (ret < 0) {
		pr_err("%s:File dev_attr_real_temp creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_at_real_temp;
	}

	ret = max77819_fg_create_debugfs_entries(max17048);
	if (ret) {
		dev_err(dev, "max77819_fg_create_debugfs_entries failed\n");
	}

	/* Prevent suspend until Fuelgauge Updeate work finished */
	wake_lock_init(&g_fg->fg_worker_wakelock, WAKE_LOCK_SUSPEND,"Fuelgauge_Worker_Lock");

	max17048_set_athd_alert(max17048,32);
	max17048_set_alsc_alert(max17048,true);

	ret = max17048_clear_interrupt(max17048);
	if(ret)
		pr_err("%s: error clear alert irq register\n", __func__);

	/* Fake Battery W/A */
	if(lge_get_fake_battery_mode()){ // Fake Battery ON
		printk("lge_get_fake_battery_mode ON\n");
		fake_temp = 250;
		d2260_hwmon_set_bat_threshold_max();
		set_fake_battery_flag(true);
		fake_battery_set = 1;
		batt_temp_ctrl_start(0);
	} else {
		fake_temp = 1000;
		set_fake_battery_flag(false);
		fake_battery_set = 0;
	}

	queue_delayed_work(g_fg->fg_internal_wq, &g_fg->check_work, msecs_to_jiffies(1000));

	/* Update Battery ID Info */
	max17048->batt_id_info = lge_get_batt_id();

	if(max17048->batt_id_info == BATT_DS2704_N ||
		max17048->batt_id_info == BATT_DS2704_L ||
		max17048->batt_id_info == BATT_DS2704_C ||
		max17048->batt_id_info == BATT_ISL6296_N ||
		max17048->batt_id_info == BATT_ISL6296_L ||
		max17048->batt_id_info == BATT_ISL6296_C) {
		max17048->valid_batt_id_check = 1; /* Valid Battery CELL */
	} else {
		max17048->valid_batt_id_check = 0; /* Invalid Battery CELL */
	}

	if((max17048->batt_id_info == BATT_DS2704_C) || (max17048->batt_id_info == BATT_ISL6296_L)) {
		bat_id_vendor = 2;
		printk("max17048->batt_id_info TECHNOHILL Battery\n");
	} else {
		bat_id_vendor = 1;
		printk("max17048->batt_id_info LG CHEM Battery\n");
	}

	printk("max17048->batt_id_info = (%d)\n",max17048->batt_id_info);
	f_inited = 1;

	if(lge_get_boot_mode()==LGE_BOOT_MODE_CHARGERLOGO){
		dev_info(dev, "%s chargerlogo \n",__func__);
		call_sysfs_fuel();
	}

	dev_info(dev, "%s done\n",__func__);
	return 0;

err_at_battexist:
	device_remove_file(&pdev->dev, &dev_attr_offmode);
err_offmode:
	device_remove_file(&pdev->dev, &dev_attr_at_chargingmodeoff);
err_at_chargingmodeoff:
	device_remove_file(&pdev->dev, &dev_attr_at_usbid);
err_at_usbid:
	device_remove_file(&pdev->dev, &dev_attr_at_fuelrst);
err_at_fuelrst:
	device_remove_file(&pdev->dev, &dev_attr_at_pmrst);
err_at_pmrst:
	device_remove_file(&pdev->dev, &dev_attr_at_chcomp);
err_at_chcomp:
	device_remove_file(&pdev->dev, &dev_attr_at_charge);
err_at_volt_base_soc:
	device_remove_file(&pdev->dev, &dev_attr_volt_base_soc);
err_at_real_temp:
	device_remove_file(&pdev->dev, &dev_attr_real_temp);
err_at_charge:
	devm_kfree(dev, pdata);
	return ret;
}

static int __exit max17048_remove(struct platform_device *pdev)
{
	struct max17048 *max17048 = platform_get_drvdata(pdev);

	devm_free_irq(&pdev->dev, max17048->irq, max17048);
	power_supply_unregister(&max17048->psy);

	if (max17048->dent)
		debugfs_remove_recursive(max17048->dent);

	device_remove_file(&pdev->dev, &dev_attr_at_charge);
	device_remove_file(&pdev->dev, &dev_attr_at_chcomp);
	device_remove_file(&pdev->dev, &dev_attr_at_pmrst);
	device_remove_file(&pdev->dev, &dev_attr_at_fuelrst);
	device_remove_file(&pdev->dev, &dev_attr_at_usbid);
	device_remove_file(&pdev->dev, &dev_attr_at_chargingmodeoff);
	device_remove_file(&pdev->dev, &dev_attr_at_battexist);
	device_remove_file(&pdev->dev, &dev_attr_offmode);
	device_remove_file(&pdev->dev, &dev_attr_volt_base_soc);
	device_remove_file(&pdev->dev, &dev_attr_real_temp);

	return 0;
}

static int max17048_suspend(struct platform_device *pdev)
{
	struct max17048 *max17048 = platform_get_drvdata(pdev);

	if (g_fg == NULL) {
		printk("max17048_suspend : global g_fg not initialized yet\n");
		return 0;
	}

	max17048_set_athd_alert(max17048,max17048_next_alert_level(get_cap()));
	cancel_delayed_work(&max17048->check_work);
	if((off_mode_status == 1) && (f_data_cap !=100)){
		printk("max17048 chargerlogo mode , set alse as true\n");
		max17048_set_alsc_alert(max17048, true);
	}
	else
	max17048_set_alsc_alert(max17048, false);

	atomic_set(&max17048->status, FG_STATUS_SUSPEND);
	return 0;
}

static int max17048_resume(struct platform_device *pdev)
{
	struct max17048 *max17048 = platform_get_drvdata(pdev);

	if (g_fg == NULL) {
		printk("max17048_resume : global g_fg not initialized yet\n");
		return 0;
	}

	max17048_set_alsc_alert(max17048, true);

	if (atomic_read(&max17048->status) == FG_STATUS_QUEUE_WORK) {
		queue_work(max17048->fg_internal_wq, &max17048->isr_handle_work);
	} else {
		queue_delayed_work(max17048->fg_internal_wq, &max17048->check_work, msecs_to_jiffies(100));
	}

	atomic_set(&max17048->status, FG_STATUS_RESUME);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id max17048_of_match[] = {
        { .compatible = "maxim,max77819_fg_ipc" },
        { },
};
MODULE_DEVICE_TABLE(of, max17048_of_match);
#endif

static const struct platform_device_id max17048_ids[] = {
	{ MAX17048_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, max17048_ids);

static struct platform_driver max17048_driver = {
	.driver	= {
		.name	= MAX17048_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = max17048_of_match,
#endif
	},
	.probe		= max17048_probe,
	.suspend	= max17048_suspend,
	.resume		= max17048_resume,
	.remove		= max17048_remove,
	.id_table	= max17048_ids,
};

static int __init max17048_init(void)
{
	return platform_driver_register(&max17048_driver);
}
late_initcall(max17048_init);

static void __exit max17048_exit(void)
{
	platform_driver_unregister(&max17048_driver);
}
module_exit(max17048_exit);

MODULE_ALIAS("i2c:max17048");
MODULE_AUTHOR("Gyungoh Yoo <jack.yoo@maximintegrated.com>");
MODULE_DESCRIPTION("MAX17048 Fuel Gauge");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
