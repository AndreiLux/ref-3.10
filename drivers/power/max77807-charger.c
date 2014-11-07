/*
 * Maxim MAX77807 Charger Driver
 *
 * Copyright (C) 2014 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* #define DEBUG */
/* #define VERBOSE_DEBUG */
#define log_level  0
#define log_worker 0

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/wakelock.h>
#include <linux/debugfs.h>
#include <linux/reboot.h>

/* for Regmap */
#include <linux/regmap.h>

/* for Device Tree */
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>

//#include <linux/power_supply.h>
#include <linux/mfd/max77807/max77807.h>
#include <linux/mfd/max77807/max77807-charger.h>
#include <linux/delay.h>
#include <linux/mfd/d2260/pmic.h>

#include <linux/power/max14670.h>
#include <linux/board_lge.h>
#include <linux/platform_data/gpio-odin.h>
#include <linux/workqueue.h>

#include <linux/slimport.h>
/* For DebugFS */
#include <linux/debugfs.h>

#ifdef CONFIG_USB_ODIN_DRD
#include <linux/usb/odin_usb3.h>
#endif

/* For wireless charging alignment */
#include <linux/power/unified_wireless_charger_alignment.h>

#define DRIVER_DESC    "MAX77807 Charger Driver"
#define DRIVER_NAME    MAX77807_CHARGER_NAME
#define DRIVER_VERSION "1.0"
#define DRIVER_AUTHOR  "TaiEup Kim <clark.kim@maximintegrated.com>"

#define IRQ_WORK_DELAY              0
#define IRQ_WORK_INTERVAL           msecs_to_jiffies(5000)
#define LOG_WORK_INTERVAL           msecs_to_jiffies(10000)
#define CC_WORK_INTERVAL           	msecs_to_jiffies(2000)
#define OFF_MODE_INTERVAL           msecs_to_jiffies(1500)
#define AICL_WORK_INTERVAL			msecs_to_jiffies(200)
#define VERIFICATION_UNLOCK         0

/* Register map */
#define CHG_SRC_INT_MASK			0x23
#define CHG_INT 					0xB0
#define CHG_INT_MASK                0xB1
#define CHG_INT_CHGIN				BIT (6)
#define CHG_INT_WCIN              	BIT (5)
#define CHG_INT_CHG                 BIT (4)
#define CHG_INT_BAT                 BIT (3)
#define CHG_INT_BATP                BIT (2)
#define CHG_INT_BYP					BIT (0)

#define CHG_INT_OK					0xB2
#define CHG_INT_CHGIN_OK			BIT (6)
#define CHG_INT_WCIN_OK				BIT (5)
#define CHG_INT_CHG_OK				BIT (4)
#define CHG_INT_BAT_OK				BIT (3)
#define CHG_INT_BATP_OK				BIT (2)
#define CHG_INT_BYP_OK				BIT (0)

#define CHG_DETAILS_00				0xB3
#define CHG_DETAILS_CHGIN_DTLS		BITS(6,5)
#define CHG_DETAILS_WCIN_DTLS       BITS(4,3)
#define CHG_DETAILS_BATP_DTLS       BIT (0)

#define CHG_DETAILS_01				0xB4
#define CHG_DETAILS_TREG        	BIT (7)
#define CHG_DETAILS_BAT_DTLS        BITS(6,4)
#define CHG_DETAILS_CHG_DTLS        BITS(3,0)

#define CHG_DETAILS_02				0xB5
#define CHG_DETAILS_BYP_DTLS		BITS(3,0)

#define CHG_DETAILS_03				0xB6

#define CHG_CNFG_00					0xB7
#define CHG_CNFG_OTG_CTRL			BIT (7)
#define CHG_CNFG_DISIBS				BIT (6)
#define CHG_CNFG_DIS_MUIC_CNTRL		BIT (5)
#define CHG_CNFG_WDTEN				BIT (4)
#define CHG_CNFG_MODE				BITS(3,0)
#define CHG_CNFG_MODE_BOOST			BIT (3)
#define CHG_CNFG_MODE_BUCK			BIT (2)
#define CHG_CNFG_MODE_OTG			BIT (1)
#define CHG_CNFG_MODE_CHARGER		BIT (0)

#define CHG_CNFG_01					0xB8
#define CHG_CNFG_PQEN				BIT (7)
#define CHG_CNFG_CHG_RSTRT			BITS(5,4)
#define CHG_CNFG_FCHGTIME			BITS(2,0)

#define CHG_CNFG_02					0xB9
#define CHG_CNFG_OTG_ILIM			BIT (7)
#define CHG_CNFG_CHG_CC				BITS(5,0)

#define CHG_CNFG_03					0xBA
#define CHG_CNFG_TO_TIME			BITS(5,3)
#define CHG_CNFG_TO_ITH				BITS(2,0)

#define CHG_CNFG_04					0xBB
#define CHG_CNFG_MINVSYS			BITS(7,5)
#define CHG_CNFG_CHG_CV_PRM			BITS(4,0)

#define CHG_CNFG_05					0xBC

#define CHG_CNFG_06					0xBD
#define CHG_CNFG_CHGPROT			BITS(3,2)
#define CHG_CNFG_WDTCLR				BITS(1,0)

#define CHG_CNFG_07					0xBE
#define CHG_CNFG_REGTEMP			BITS(6,5)

#define CHG_CNFG_08					0xBF

#define CHG_CNFG_09					0xC0
#define CHG_CNFG_CHGIN_ILIM			BITS(6,0)

#define CHG_CNFG_10					0xC1
#define CHG_CNFG_WCIN_ILIM			BITS(5,0)

#define CHG_CNFG_11					0xC2
#define CHG_CNFG_VBYPSET			BITS(6,0)

#define CHG_CNFG_12					0xC3
#define CHG_CNFG_CHG_LPM			BIT (7)
#define CHG_CNFG_WCINSEL			BIT (6)
#define CHG_CNFG_CHGINSEL			BIT (5)
#define CHG_CNFG_VCHGIN_REG			BITS(4,3)
#define CHG_CNFG_B2SOVRC			BITS(2,0)

#define CHG_CNFG_13					0xC4
#define CHG_CNFG_14					0xC5

#define SAFEOUTCTRL					0xC6
#define SAFEOUTCTRL_ACTDISSAFEO1	BIT (4)
#define SAFEOUTCTRL_ENSAFEOUT2		BIT(7)
#define SAFEOUTCTRL_SAFEOUT2		BITS(3,2)
#define SAFEOUTCTRL_SAFEOUT1		BITS(1,0)
/* define interrupt register */
#define PMIC_INTSRC					0x22
#define PMIC_INTSRC_MASK			0x23
#define INTSRC_CHGR_INT				BIT (0)
#define INTSRC_TOP_INT				BIT (1)
#define INTSRC_USBSW_INT			BIT (3)

#define MAX77807_DISABLE_IRQ	1

#define WLC_ALIGN_START_DELAY      (1000)
#define WLC_ALIGN_INTERVAL         (2000)

struct max77807_charger *global_charger = NULL;
int max77807_odin_charger_enable = 0;
unsigned int wlc_chrg_sts = 0;
static int wlc_connect_sts = 0;
static int wlc_otp_vbus_flag = 0;
static int offmode_flag= 0;
static int vbus_flag= 0;
static int at_chargingoff_flag= 0;

/* [START] Test code for Thermal Engine */
struct pseudo_thermal_curr_ctrl_info_type pseudo_thermal_curr_ctrl_info = {
        .current_mA = 0,
};
/* [END] */

struct debug_reg {
	char  *name;
	u8  reg;
};

#define MAX77807_CHRG_DEBUG_REG(x) {#x, x##_REG}

static struct debug_reg max77807_chrg_debug_regs[] = {
	MAX77807_CHRG_DEBUG_REG(CHG_SRC_INT_MASK),
	MAX77807_CHRG_DEBUG_REG(CHG_INT),
	MAX77807_CHRG_DEBUG_REG(CHG_INT_MASK),
	MAX77807_CHRG_DEBUG_REG(CHG_INT_OK),
	MAX77807_CHRG_DEBUG_REG(CHG_DETAILS_00),
	MAX77807_CHRG_DEBUG_REG(CHG_DETAILS_01),
	MAX77807_CHRG_DEBUG_REG(CHG_DETAILS_02),
	MAX77807_CHRG_DEBUG_REG(CHG_DETAILS_03),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_00),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_01),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_02),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_03),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_04),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_05),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_06),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_07),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_08),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_09),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_10),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_11),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_12),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_13),
	MAX77807_CHRG_DEBUG_REG(CHG_CNFG_14),
	MAX77807_CHRG_DEBUG_REG(SAFEOUTCTRL),
};

enum {
	WCIN_DTLS_INVALID_00,
	WCIN_DTLS_INVALID_01,
	WCIN_DTLS_INVALID_02,
	WCIN_DTLS_VALID,
};

enum {
	CHGIN_DTLS_INVALID_00,
	CHGIN_DTLS_INVALID_01,
	CHGIN_DTLS_INVALID_02,
	CHGIN_DTLS_VALID,
};

enum {
	CHG_DTLS_PREQUAL,
	CHG_DTLS_FASTCHARGE_CC,
	CHG_DTLS_FASTCHARGE_CV,
	CHG_DTLS_TOPOFF,
	CHG_DTLS_DONE,
	CHG_DTLS_HIGH_TEMP_CHARGING,
	CHG_DTLS_OFF_TIMER_FAULT,
	CHG_DTLS_OFF_SUSPEND,
	CHG_DTLS_OFF_INPUT_INVALID,
	CHG_DTLS_OFF_JUCTION_TEMP,
	CHG_DTLS_OFF_WDT_EXPIRED,
};

enum {
	BAT_DTLS_NO_BATTERY,
	BAT_DTLS_DEAD,
	BAT_DTLS_TIMER_FAULT,
	BAT_DTLS_OKAY,
	BAT_DTLS_OKAY_LOW,
	BAT_DTLS_OVERVOLTAGE,
	BAT_DTLS_OVERCURRENT,
};


#define BYP_DTLS_VCHGINLIM	BIT (3)
#define BYP_DTLS_BCKNEGILIM	BIT (2)
#define BYP_DTLS_BSTILIM	BIT (1)
#define BYP_DTLS_OTGILIM	BIT (0)

#define __lock(_me)    mutex_lock(&(_me)->lock)
#define __unlock(_me)  mutex_unlock(&(_me)->lock)

#define max77807_charger_read_irq_status(_me, _irq_reg) \
		({\
			u8 __irq_current = 0;\
			int __rc = max77807_read((_me)->io, _irq_reg, &__irq_current);\
			if (unlikely(IS_ERR_VALUE(__rc))) {\
				log_err(#_irq_reg" read error [%d]\n", __rc);\
				__irq_current = 0;\
			}\
			__irq_current;\
		})

#define CHGPROT_UNLOCK  3
#define CHGPROT_LOCK    0

#define CHGIN_OK_INPUT_VALID	1
#define CHGIN_OK_INPUT_INVALID	0
static struct wakeup_source max77807_chg_lock;
static int get_curr_thermal = 0;
static int fake_battery_status = 0;
static int wlc_align_value_sts_2s_wait = 0;
static int max77807_charger_set_charge_current (struct max77807_charger *me, int limit_uA, int charge_uA);
extern int d2260_adc_get_vichg(void);
extern int d2260_adc_get_usb_id(void);
extern void call_sysfs_fuel(void);
extern void idtp9025_align_set_wlc_full_gpio(void);
extern void idtp9025_align_set_wlc_full_gpio_high(void);
extern void idtp9025_align_set_wlc_full_gpio_low(void);
extern int idtp9025_align_freq_value(void);

void set_fake_battery_flag(bool fake_flag)
{
	if(fake_flag == 1){
		printk("max77807-charger set fake battery as 1\n");
		fake_battery_status = 1;
	}else{
		printk("max77807-charger set fake battery as 0\n");
		fake_battery_status = 0;
	}

}

/* Charger wake lock */
static void max77807_charger_wake_lock (struct max77807_charger *me, bool enable)
{
	/* TODO: EOC unlock chg wake lock needed */
	if (!me)
		log_dbg("called before init\n");

	if (enable) {
		if (!wake_lock_active(&me->chg_wake_lock)) {
			log_err("charging wake lock enable\n");
			wake_lock(&me->chg_wake_lock);
		}
	} else {
		if (wake_lock_active(&me->chg_wake_lock)) {
			log_err("charging wake lock disable\n");
			wake_unlock(&me->chg_wake_lock);
		}
	}
}

static int wireless_align_start(struct max77807_charger *chip)
{
	int align = 0;

	chip->align_values = 0;

	align = idtp9025_align_start();
	if(align > 0)
		chip->align_values = align;

	return align;
}

static int wireless_align_stop(struct max77807_charger *chip)
{
	int ret = 0;

#if 0
	chip->align_values = 0;
#else
	queue_delayed_work(chip->chg_internal_wq, &chip->wlc_reset_work, msecs_to_jiffies(2000));
#endif
	return ret;
}

static int align_value_sts = 0;
static int wireless_align_get_value(struct max77807_charger *chip)
{
	int align = 0;
	int align_changed = 0;

	align = idtp9025_align_get_value();

	if((idtp9025_align_freq_value() < 145) && (chip->align_values == 0) && (align == 1)) {
		pr_err("wireless_align_get_value => Under 145Hz => Retry Onemore Time\n");
		align = idtp9025_align_get_value();
	}

	if((align > 0) && (chip->align_values != align)) {
		pr_info("\n alignment has been changed!! [%d][%d] \n\n", chip->align_values, align);
		chip->align_values = align;
		align_value_sts = align;
		align_changed = true;
	}

	return align_changed;
}
static void wireless_align_proc(struct max77807_charger *chip, bool attached)
{
	int start_status = 0;
	int rc = 0;

	/* Wakelock for WLC */
	wake_lock(&chip->wlc_align_wakelock);

	if (likely(attached)) {
		printk("wireless_align_proc [ATTACHED]\n");
		/* start work queue for alignment */

		start_status = idtp9025_align_init_check();
		if(start_status < 0) {
			printk("wireless_align_proc_error <start_status> = [%d] \n",start_status);
			return;
		}
#if 0
		if (likely(delayed_work_pending(&chip->wireless_align_work))) {
			flush_delayed_work_sync(&chip->wireless_align_work);
		}
#endif
		queue_delayed_work(chip->chg_internal_wq, &chip->wireless_align_work, msecs_to_jiffies(WLC_ALIGN_START_DELAY));
	} else {
		printk("wireless_align_proc [DETACHED]\n");
		queue_delayed_work(chip->chg_internal_wq, &chip->wireless_align_work,msecs_to_jiffies(100));
	}
}

static __always_inline int max77807_charger_unlock (struct max77807_charger *me)
{
	int rc = 0;
	if(me->is_factory_cable)
		goto out;

	rc = max77807_masked_write(me->io, CHG_CNFG_06, CHG_CNFG_CHGPROT,
								FFS(CHG_CNFG_CHGPROT), CHGPROT_UNLOCK);
	if (unlikely(IS_ERR_VALUE(rc))) {
		log_err("failed to unlock [%d]\n", rc);
		goto out;
	}

out:
	return rc;
}

static __always_inline int max77807_charger_lock (struct max77807_charger *me)
{
	int rc = 0;

	rc = max77807_masked_write(me->io, CHG_CNFG_06, CHG_CNFG_CHGPROT,
								FFS(CHG_CNFG_CHGPROT), CHGPROT_LOCK);
	if (unlikely(IS_ERR_VALUE(rc))) {
		log_err("failed to lock [%d]\n", rc);
	}

	return rc;
}

#ifdef CONFIG_MAX77807_USBSW
static bool max77807_charger_present_input (struct max77807_charger *me)
{
	return (me->usbsw_status & BIT_STATUS2_VBVOLT ? true : false);
}
static bool max77807_charger_present_input_slimport (struct max77807_charger *me)
{
	u8 chgin_ok = 0;
	u8 details[3];
	int rc;

	rc = max77807_masked_read(me->io, CHG_INT_OK, CHG_INT_CHGIN_OK,
									FFS(CHG_INT_CHGIN_OK), &chgin_ok);
    if (unlikely(IS_ERR_VALUE(rc))) {
        return false;
    }

	if (chgin_ok == CHGIN_OK_INPUT_VALID) {
		return true;
	} else {
		max77807_bulk_read(me->io, CHG_DETAILS_00, details, 3);
		if (((details[0] & CHG_DETAILS_CHGIN_DTLS) == 0) &&
				((details[1] & CHG_DETAILS_CHG_DTLS) == CHG_DTLS_OFF_INPUT_INVALID)) {
					return false;
		} else {
					return true;
		}
	}
}
#else
static bool max77807_charger_present_input (struct max77807_charger *me)
{
	u8 chgin_ok = 0;
	u8 details[3];
    int rc = 0;

	rc = max77807_masked_read(me->io, CHG_INT_OK, CHG_INT_CHGIN_OK,
								FFS(CHG_INT_CHGIN_OK), &chgin_ok);
	if (unlikely(IS_ERR_VALUE(rc))) {
		return false;
	}

	if (chgin_ok == CHGIN_OK_INPUT_VALID) {
		return true;
	} else {
		/* read CHG_DETAILS */
		rc = max77807_bulk_read(me->io, CHG_DETAILS_00, details, 3);
		if (unlikely(IS_ERR_VALUE(rc))) {
			return false;
		}
		if (((details[0] & CHG_DETAILS_CHGIN_DTLS) == 0) &&
				(details[2] & BYP_DTLS_VCHGINLIM) == BYP_DTLS_VCHGINLIM) {
			/* input voltage regulation loop is active */
			return true;
		} else {
			return false;
		}
	}
}
#endif

static bool max77807_wcharger_present_input (struct max77807_charger *me)
{
    u8 wcin_ok = 0;
	u8 details[3];
    int rc = 0;

	rc = max77807_masked_read(me->io, CHG_INT_OK, CHG_INT_WCIN_OK,
								FFS(CHG_INT_WCIN_OK), &wcin_ok);
	if (unlikely(IS_ERR_VALUE(rc))) {
		return false;
	}

	if (wcin_ok == CHGIN_OK_INPUT_VALID) {
		wlc_otp_vbus_flag = 1;
		//printk("max77807_wcharger_present_input is OK\n");
		return true;
	} else {
		/* read CHG_DETAILS */
		rc = max77807_read(me->io, CHG_DETAILS_00, &details[0]);
		rc = max77807_read(me->io, CHG_DETAILS_01, &details[1]);
		rc = max77807_read(me->io, CHG_DETAILS_02, &details[2]);
		//printk("max77807_wcharger_present_input 00=[0x%02X] 01=[0x%02X] 02=[0x%02X]\n",details[0],details[1],details[2]);
		if (((details[0] & CHG_DETAILS_CHGIN_DTLS) == 0) &&
			(details[2] & BYP_DTLS_VCHGINLIM) == BYP_DTLS_VCHGINLIM) {
			/* input voltage regulation loop is active */
			wlc_otp_vbus_flag = 2;
			//printk("max77807_wcharger_present_input is Condition OK\n");
			return true;
		} else {
			//printk("max77807_wcharger_present_input is FALSE\n");
			return false;
		}
	}
}

unsigned int wlc_conn_sts = 0;
int wcharger_conn_sts(void)
{
	if(global_charger == NULL) {
		printk("wcharger_conn_sts <Global Charger> is NULL\n");
		return 0;
	}

	if(max77807_wcharger_present_input(global_charger) == true) {
		return 1;
	} else {
		return 0;
	}
}

unsigned int wlc_noty_otp = 0;
int old_curr_sts = 0;
int chrg_curr_sts = 0;
int chrg_curr_thr_H = 0;
int chrg_curr_thr_L = 0;
static void wireless_align_work(struct work_struct *work)
{
	struct max77807_charger *chip = container_of(work, struct max77807_charger, wireless_align_work.work);
	struct power_supply *psy=NULL;
	union power_supply_propval ret = {0,};
	int battery_capacity = 0;
	int align_changed = 0;
	int batt_volt_align = 0;
	int rc = 0;
	u8 val = 0;
	bool wlc_conn_status = false;

	if(!chip) return;

	if(global_charger == NULL) {
		printk("wireless_align_work global charger is NULL\n");
		return;
	}

	/* Set Charging Done Timer 60min*/
	rc = max77807_read(chip->io, CHG_CNFG_03, &val);
	if((val & 0x30) != 0x30) {
		printk("wireless_align_work_set wlc_charging_done_timer 60min\n");
		rc = max77807_write(chip->io, CHG_CNFG_03, 0x30);
		if (unlikely(IS_ERR_VALUE(rc))) {
			printk("wireless_align_work_1 set wlc_charging_done_timer fail\n");
		}
	}

	/* Battery Present Check */
	psy = power_supply_get_by_name("battery");
	if (!psy) {
		pr_err("%s: failed to get <battery> power supply\n", __func__);
		return;
	}
	psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &ret);
	if(ret.intval==0) {
		printk("wireless_align_work => No Battery Skip\n");
			if(wake_lock_active(&chip->wlc_align_wakelock)) {
				wake_unlock(&chip->wlc_align_wakelock);
				pr_info("wireless_align_proc_<no_bat> [wake_unlock]\n");
			}
			return;
		}

	/* Wireless Charging Current Control */

	batt_volt_align = get_voltage();

	if(chrg_curr_sts == 1) {
		if(batt_volt_align < chrg_curr_thr_L) { chrg_curr_sts =2; }
	} else if(chrg_curr_sts == 2) {
		if(batt_volt_align > chrg_curr_thr_H) { chrg_curr_sts =1; }
		else if(batt_volt_align < chrg_curr_thr_L) { chrg_curr_sts =3; }
		else { }
	} else if(chrg_curr_sts == 3) {
		if(batt_volt_align > chrg_curr_thr_H) { chrg_curr_sts = 2; }
	} else {
		printk("wireless_align_work First chrg_curr_sts Setting\n");
		if(batt_volt_align >= 4300000) {
			chrg_curr_sts = 1;
		} else if((batt_volt_align >= 4200000) && (batt_volt_align < 4300000)) {
			chrg_curr_sts = 2;
		} else {
			chrg_curr_sts = 3;
		}
	}

	/* Adjust Threshold */
	if(chrg_curr_sts == 1) {
		chrg_curr_thr_H = 5000000;
		chrg_curr_thr_L = 4230000;
	} else if(chrg_curr_sts == 2) {
		chrg_curr_thr_H = 4300000;
		chrg_curr_thr_L = 4050000;
	} else if(chrg_curr_sts == 3) {
		chrg_curr_thr_H = 4200000;
		chrg_curr_thr_L = 0;
	} else {
		//printk("wireless_align_work set wlc charging voltage threshold\n");
	}

	//printk("wireless_align_work < chrg_curr_sts = [%d] old_curr_sts = [%d] >\n", chrg_curr_sts, old_curr_sts);

	wlc_conn_status = max77807_wcharger_present_input(chip);

	if((wlc_connect_sts == 0) && (wlc_conn_status == true)) {
		printk("wireless_align_work <<ISR is not WLC>> set false\n");
		wlc_conn_status = false;
	}

	if(wlc_conn_status == true && wlc_otp_vbus_flag == 1 ) {
		if(chrg_curr_sts != old_curr_sts) {
			if(chrg_curr_sts == 1) {
				rc = max77807_charger_set_charge_current(chip, 433000, 500000);
				old_curr_sts = 1;
			} else if(chrg_curr_sts == 2) {
				rc = max77807_charger_set_charge_current(chip, 433000, 500000);
				old_curr_sts = 2;
			} else if(chrg_curr_sts == 3) {
				rc = max77807_charger_set_charge_current(chip, 740000, 900000);
				old_curr_sts = 3;
			} else {
				printk("wireless_align_work chrg_curr_sts is 0\n");
			}
		} else {
		//printk("wireless_align_work chrg_curr [new state == old state]\n");
		}
	}

	/* WLC Attach Status Check */
	if(wlc_conn_status == false) {
		printk("wireless_align_work => WLC removed => Re Check After 2Sec\n");

		/* Reset Alignment Value */
		wireless_align_stop(chip);

		/* wireless charger remove */
		queue_delayed_work(chip->chg_internal_wq, &chip->check_cable_work,msecs_to_jiffies(2000));

		/* WLC Stop Chrg Worker */
		queue_delayed_work(chip->chg_internal_wq, &chip->wlc_chrg_stop_work,msecs_to_jiffies(2000));

		/* UnMask Worker */
		queue_delayed_work(chip->chg_internal_wq, &chip->wlc_unmask_work,msecs_to_jiffies(2000));

		return;
	}

	psy->get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &ret);
	battery_capacity = ret.intval;

	/* Full Charging Enter Suspend */
	if((wlc_conn_status == true) && (battery_capacity == 100) && (get_voltage() > 4325000)) {
		printk("wireless_align_work wlc_full_chrg go to suspend\n");

		/* UnMask WLC Interrupt */
		rc = max77807_read(chip->io, CHG_INT_MASK, &val);
		if (unlikely(IS_ERR_VALUE(rc))) {
			log_err("wireless_align_work CHG_INT_MASK read error [%d]\n", rc);
		} else {
			rc = max77807_write(chip->io, CHG_INT_MASK, (val&0xDF));
			if (unlikely(IS_ERR_VALUE(rc))) {
				log_err("wireless_align_work CHG_INT_MASK write error [%d]\n", rc);
			} else {
				log_err("wireless_align_work UnMASK WLC Interrupt\n");
			}
		}

		/* Wake Unlock */
		if(wake_lock_active(&chip->wlc_align_wakelock)) {
			wake_unlock(&chip->wlc_align_wakelock);
			pr_info("wireless_align_work_<wlc_full_chrg> [wake_unlock]\n");
		}

		return;
	}

	if(battery_capacity == 100) {
		goto check_status;
	}

	mutex_lock(&chip->align_lock);
	align_changed = wireless_align_get_value(chip);
	mutex_unlock(&chip->align_lock);

	if(align_changed) {
		printk("wireless_align_work update align value = %d\n",align_value_sts);
		power_supply_changed(&chip->wireless);
	}

check_status:
	//schedule_delayed_work(&chip->wireless_align_work, msecs_to_jiffies(WLC_ALIGN_INTERVAL));
	queue_delayed_work(chip->chg_internal_wq, &chip->wireless_align_work,msecs_to_jiffies(WLC_ALIGN_INTERVAL));
}

static __inline int max77807_charger_get_dcilmt (struct max77807_charger *me,
															int *uA)
{
    int rc = 0;
    u8 dcilmt = 0;

	rc = max77807_masked_read(me->io, CHG_CNFG_09, CHG_CNFG_CHGIN_ILIM,
								FFS(CHG_CNFG_CHGIN_ILIM), &dcilmt);
	if (unlikely(IS_ERR_VALUE(rc))) {
		goto out;
	}

	*uA = dcilmt < 0x03 ? 60000 : (int)(dcilmt - 0x03) * 20000 + 60000;

	log_vdbg("<get_dcilmt> %Xh -> %duA\n", dcilmt, *uA);

out:
	return rc;
}

static int max77807_charger_set_dcilmt (struct max77807_charger *me, int uA)
{
	int rc = 0;
    u8 dcilmt;
	bool chg_input, wc_input;

	chg_input = max77807_charger_present_input(me);
	wc_input = max77807_wcharger_present_input(me);

	dcilmt = uA <  60000 ? 0x00 : ((uA - 60000)/20000 + 0x03);

	log_vdbg("<set_dcilmt> %duA -> %Xh\n", uA, dcilmt);

	if (wc_input && !chg_input) {
		/* max input limit is 1264mA */
		rc = max77807_write(me->io, CHG_CNFG_10, dcilmt);
		if (unlikely(IS_ERR_VALUE(rc))) {
			goto out;
		}
	} else {
		rc = max77807_write(me->io, CHG_CNFG_09, dcilmt);
	}
out:
	return rc;
}

static __inline int max77807_charger_get_enable (struct max77807_charger *me,
															int *en)
{
    int rc = 0;
    u8 mode = 0;

	rc = max77807_masked_read(me->io, CHG_CNFG_00, CHG_CNFG_MODE,
								FFS(CHG_CNFG_MODE), &mode);
	if (unlikely(IS_ERR_VALUE(rc))) {
		goto out;
	}

	*en = !!(mode & CHG_CNFG_MODE_CHARGER);
	log_dbg("<get_enable> %s\n", en ? "enabled" : "disabled");

out:
	return rc;
}

static int max77807_charger_set_enable (struct max77807_charger *me, int en)
{
	if (en == 1){
		max77807_odin_charger_enable = 1;
		me->chg_enabled =1;
	}
	else{
		max77807_odin_charger_enable = 0;
		me->chg_enabled =0;
	}

	log_dbg("<set_enable> %s\n", en ? "enabling" : "disabling");
	return max77807_masked_write(me->io, CHG_CNFG_00, CHG_CNFG_MODE_CHARGER,
									FFS(CHG_CNFG_MODE_CHARGER), !!en);
}

static __inline int max77807_charger_get_chgcc (struct max77807_charger *me,
															int *uA)
{
    int rc = 0;
    u8 chgcc = 0;

	rc = max77807_masked_read(me->io, CHG_CNFG_02, CHG_CNFG_CHG_CC,
								FFS(CHG_CNFG_CHG_CC), &chgcc);
	if (unlikely(IS_ERR_VALUE(rc))) {
		goto out;
	}

	*uA = (int) chgcc * 100000 / 3;

	printk("max77807_charger_get_chgcc <get_chgcc> %Xh -> %duA\n", chgcc, *uA);

out:
	return rc;
}

static int max77807_charger_get_chgcc_for_OTP (struct max77807_charger *me)
{
    int rc = 0;
	int uA = 0;
    u8 chgcc = 0;

	rc = max77807_masked_read(me->io, CHG_CNFG_02, CHG_CNFG_CHG_CC,
								FFS(CHG_CNFG_CHG_CC), &chgcc);
	if (unlikely(IS_ERR_VALUE(rc))) {
		printk("max77807_charger_get_chgcc_get_value_FAIL\n");
		goto out;
	}

	uA = (int) chgcc * 100000 / 3;

	printk("max77807_charger_get_chgcc <get_chgcc> %Xh -> %duA\n", chgcc, uA);
	return uA;

out:
	return rc;
}

static int max77807_charger_set_chgcc (struct max77807_charger *me, int uA)
{
	u8 chgcc;

	chgcc = uA * 3 / 100000;

	log_vdbg("<set_chgcc> %duA -> %Xh\n", uA, chgcc);

	return max77807_masked_write(me->io, CHG_CNFG_02, CHG_CNFG_CHG_CC,
									FFS(CHG_CNFG_CHG_CC), chgcc);
}

static int max77807_charger_set_charge_current (struct max77807_charger *me,
																int limit_uA, int charge_uA)
{
	int rc = 0;

	if(me->is_factory_cable)
		goto out;
	if(me->vbus_check == false)
	{
		printk("max77807_charger_set_charge_current : vbus is false \n");
		goto out;
	}
	if((at_chargingoff_flag== 1)&& (limit_uA >60000))
	{
		printk("max77807_charger_set_charge_current : at chargingoff is true \n");
		goto out;
	}

	printk("max77807_charger_set_charge_current : limit(%d) chrg_currednt(%d)\n",limit_uA/1000,charge_uA/1000);

	rc = max77807_charger_set_chgcc(me, charge_uA);

	rc = max77807_charger_set_dcilmt(me, limit_uA);

out:
	return rc;
}

void max77807_charger_set_charge_current_450mA(void)
{
	int *chg_current;
	int rc = 0;

	if(global_charger == NULL) {
		printk("max77807_charger_set_charge_current_450mA <global charger is NULL>\n");
		return;
	}

	chg_current = max77807_charger_get_chgcc_for_OTP(global_charger);
	//printk("KKK_TEST chg_current = %d\n",chg_current);

	if(chg_current >= 500000) {
		max77807_charger_set_chgcc(global_charger,450000);
		printk("max77807_charger_set_charge_current_450mA for OTP Driver\n");
	}
}

static int max77807_charger_exit_dev (struct max77807_charger *me)
{
	struct max77807_charger_platform_data *pdata = me->pdata;

	max77807_charger_set_enable(me, false);

	me->current_limit_volatile  = me->current_limit_permanent;
	me->charge_current_volatile = me->charge_current_permanent;

	max77807_charger_set_charge_current(me, me->current_limit_permanent,
	me->charge_current_permanent);

	me->dev_enabled     = (!pdata->enable_coop || pdata->coop_psy_name);
	me->dev_initialized = false;
	return 0;
}

static int max77807_charger_init_dev (struct max77807_charger *me)
{
	struct max77807_charger_platform_data *pdata = me->pdata;
	int rc;
	u8 val;

	val  = 0;
	// val |= CHG_INT_CHGIN;
	if(wlc_connect_sts == 0) {
		val |= CHG_INT_WCIN;
	}
	// val |= CHG_INT_CHG;
	// val |= CHG_INT_BAT;
	// val |= CHG_INT_BATP;
	// val |= CHG_INT_BYP;
/*
	rc = max77807_write(me->io, CHG_INT_MASK, ~val);
	if (unlikely(IS_ERR_VALUE(rc))) {
		log_err("CHG_INT_MASK write error [%d]\n", rc);
	goto out;
	}
*/
	/* unlock charger register */
	max77807_charger_unlock(me);

	/* charge current */
	rc = max77807_charger_set_charge_current(me, me->current_limit_volatile,
												me->charge_current_volatile);
	if (unlikely(IS_ERR_VALUE(rc))) {
		goto out;
	}

	/* topoff timer */
	val = pdata->topoff_timer <=  0 ? 0x00 :
	pdata->topoff_timer <= 70 ?
	(int)DIV_ROUND_UP(pdata->topoff_timer, 10) : 0x07;
	rc = max77807_masked_write(me->io, CHG_CNFG_03, CHG_CNFG_TO_TIME,
								FFS(CHG_CNFG_TO_TIME), val);
	if (unlikely(IS_ERR_VALUE(rc))) {
		goto out;
	}

	/* topoff current */
	val = pdata->topoff_current <= 100000 ? 0x00 :
	pdata->topoff_current <= 200000 ? (int)DIV_ROUND_UP(pdata->topoff_current - 100000, 25000) :
	pdata->topoff_current <= 350000 ? (int)DIV_ROUND_UP(pdata->topoff_current - 200000, 50000) : 0x07;
	rc = max77807_masked_write(me->io, CHG_CNFG_03, CHG_CNFG_TO_ITH,
								FFS(CHG_CNFG_TO_ITH), val);
	if (unlikely(IS_ERR_VALUE(rc))) {
		goto out;
	}

	/* charge restart threshold */
	val = pdata->charge_restart_threshold <= 200000 ?
	(int)(pdata->charge_restart_threshold - 100000)/50 : 0x03;
	rc = max77807_masked_write(me->io, CHG_CNFG_01, CHG_CNFG_CHG_RSTRT,
								FFS(CHG_CNFG_CHG_RSTRT), val);
	if (unlikely(IS_ERR_VALUE(rc))) {
		goto out;
	}

	/* fast-charge timer */
	val = pdata->fast_charge_timer ==  0 ? 0x00 :
	pdata->fast_charge_timer <=  4 ? 0x01 :
	pdata->fast_charge_timer <= 16 ?
	(int)DIV_ROUND_UP(pdata->fast_charge_timer - 4, 2) + 1 : 0x00;
	rc = max77807_masked_write(me->io, CHG_CNFG_01, CHG_CNFG_FCHGTIME,
								FFS(CHG_CNFG_FCHGTIME), val);
	if (unlikely(IS_ERR_VALUE(rc))) {
		goto out;
	}

	/* charge termination voltage */
	val = pdata->charge_termination_voltage < 3650000 ? 0x00 :
	pdata->charge_termination_voltage <= 4325000 ?
	(int)DIV_ROUND_UP(pdata->charge_termination_voltage - 3650000, 25000) :
	pdata->charge_termination_voltage <= 4340000 ? 0x1C :
	pdata->charge_termination_voltage <= 4400000 ?
	(int)DIV_ROUND_UP(pdata->charge_termination_voltage - 3650000, 25000) + 1 : 0x1F;
	rc = max77807_masked_write(me->io, CHG_CNFG_04, CHG_CNFG_CHG_CV_PRM,
								FFS(CHG_CNFG_CHG_CV_PRM), val);
	if (unlikely(IS_ERR_VALUE(rc))) {
		goto out;
	}

	/* charger enable */
	rc = max77807_charger_set_enable(me, me->dev_enabled);
	if (unlikely(IS_ERR_VALUE(rc))) {
		goto out;
	}

	me->dev_initialized = true;
	log_dbg("device initialized\n");

out:
	return rc;
}

#ifdef CONFIG_USB_ODIN_DRD

bool max77807_charger_vbus_event(struct max77807_charger *me,
									int vbus, int delay_ms)
{
	if(!me->otg_vbus) {
		odin_otg_vbus_event(me->odin_otg, vbus, delay_ms);
		return true;
	}
	else {
		printk("No %s because otg_vbus is on\n", __func__);
		return false;
	}
}

static void max77807_charger_usb_event(struct notifier_block *nb,
													unsigned long event, void *data)
{
	struct max77807_charger *me =
		container_of(nb, struct max77807_charger, nb);
	switch (event) {
		case ODIN_USB_CABLE_EVENT:
			queue_delayed_work(me->chg_internal_wq,
								&me->check_cable_work, IRQ_WORK_DELAY);
			break;
		default:
			break;
	}
}
static void max77807_charger_usb_register(struct max77807_charger *me)
{
	struct usb_phy *phy = NULL;
	struct odin_usb3_otg *usb3_otg;
	int ret = 0;

	usb_dbg("[%s] \n", __func__);

	phy = usb_get_phy(USB_PHY_TYPE_USB3);
	if (IS_ERR_OR_NULL(phy)) {
		return;
	}

	usb3_otg = phy_to_usb3otg(phy);
	me->odin_otg = usb3_otg;
	me->odin_otg->charger_dev = me->dev;
	me->odin_otg->factory_cable_no_battery = me->is_factory_cable;

	if(!me->is_factory_cable) {
		me->nb.notifier_call = max77807_charger_usb_event;
		ret = usb_register_notifier(phy, &me->nb);
		if (ret)
			log_err("%s: fail to register usb notifier \n", __func__);
	}
}

bool max77807_drd_charger_init_detect(struct odin_usb3_otg *odin_otg)
{
	struct max77807_charger *me =
					dev_get_drvdata(odin_otg->charger_dev);
	u8 irq_current;
	bool input = false;
	bool wlc_input = false;

	usb_dbg("[%s] \n", __func__);

	if(me->is_factory_cable) {
		max77807_charger_vbus_event(me, 1, 450);
		me->usb_init = 1;
		input = true;
	}
	else {
#ifdef CONFIG_MAX77807_USBSW
		u8 val;

		max77807_usbsw_read(REG_USBSW_INT2, &val);
		me->usbsw_irq = val;
		max77807_usbsw_read(REG_USBSW_STATUS2, &val);
		me->usbsw_status = val;
#endif
		wlc_input = max77807_wcharger_present_input(me);
		input = max77807_charger_present_input(me);
		if(input == false) {
			if(wlc_input) {
				max77807_odin_charger_enable = 1;
				printk("max77807-charger intit detect: wireless charger\n");
				me->dcin_type				 = POWER_SUPPLY_TYPE_WIRELESS;
				me->status				 = POWER_SUPPLY_STATUS_CHARGING;
				me->charge_type 			 = POWER_SUPPLY_CHARGE_TYPE_FAST;
			} else {
				max77807_odin_charger_enable = 0;
				printk("max77807-charger intit detect: no cable\n");
				me->dcin_type				 = POWER_SUPPLY_TYPE_BATTERY;
				me->status				 = POWER_SUPPLY_STATUS_DISCHARGING;
				me->charge_type 			 = POWER_SUPPLY_CHARGE_TYPE_NONE;
			}
		}

		log_info("DC input %s\n", input ? "inserted" : "removed");
		max77807_charger_vbus_event(me, input, 450);
		me->usb_init = 1;

		if (likely(input)) {
			log_info("start charging\n");
			max77807_charger_init_dev(me);
		} else if(wlc_input) {
			log_info("wlc start charging => max77807_drd_charger_init_detect\n");
			max77807_charger_init_dev(me);
			wireless_align_proc(me, true);
			wlc_chrg_sts = 1;
		} else {
			log_info("No Charging Input Source\n");
		}
	}

	return input;
}

#define RBOOST_CTL1_RBOOSTEN BIT(0)
#define RBOOST_CTL1_RBFORCEPWM BIT(4)
#define RBOOST_CTL1_BSTSLEWRATE BIT(7,5)
#define BATTOSOC_CTL_OTG_EN BIT(5)

void max77807_otg_vbus_onoff(struct odin_usb3_otg *odin_otg, int onoff)
{
	struct max77807_charger *me =
				dev_get_drvdata(odin_otg->charger_dev);
	u8 cnfg_00;

	log_info("[%s] ==> %s \n", __func__, onoff? "ON":"OFF");

	if (onoff) {
		me->otg_vbus = true;
		max77807_read(me->io,CHG_CNFG_00, &cnfg_00); // OTG register read and write bit to enable
		max77807_write(me->io,CHG_CNFG_00,0x2A);
		max77807_read(me->io,CHG_CNFG_00, &cnfg_00); // OTG register read and write bit to enable

		max77807_write(me->io, CHG_INT_MASK, 0xDF); // interrupt unmask for WLC
		max77807_write(me->io,CHG_CNFG_02,0x80); // set current limit as 900mA

	} else {
		max77807_write(me->io,CHG_CNFG_00,0x05);
		//max77807_write(me->io, CHG_INT_MASK, 0xDF); // interrupt unmask for WLC
		max77807_write(me->io, CHG_INT_MASK, 0x9F); // interrupt unmask for WLC
		max77807_read(me->io,CHG_CNFG_00, &cnfg_00); // OTG register read and write bit to enable
		me->otg_vbus = false;
	}
}
#endif /* CONFIG_USB_ODIN_DRD */

#define max77807_charger_psy_setprop(_me, _psy, _psp, _val) \
		({\
			struct power_supply *__psy = _me->_psy;\
			union power_supply_propval __propval = { .intval = _val };\
			int __rc = -ENXIO;\
			if (likely(__psy && __psy->set_property)) {\
				__rc = __psy->set_property(__psy, POWER_SUPPLY_PROP_##_psp,\
									&__propval);\
			}\
			__rc;\
		})

#define max77807_charger_psy_getprop(_me, _psy, _psp, _val)	\
		({\
			struct power_supply *__psy = _me->_psy;\
			int __rc = -ENXIO;\
			if (likely(__psy && __psy->get_property)) {\
				__rc = __psy->get_property(__psy, POWER_SUPPLY_PROP_##_psp,\
							_val);\
			}\
			__rc;\
		})


#define PSY_PROP(psy, prop, val) (psy->get_property(psy, \
						POWER_SUPPLY_PROP_##prop, val))


static void max77807_charger_psy_init (struct max77807_charger *me)
{
	if (unlikely(!me->psy_ext && me->pdata->ext_psy_name)) {
		me->psy_ext = power_supply_get_by_name(me->pdata->ext_psy_name);
		if (likely(me->psy_ext)) {
			log_dbg("psy %s found\n", me->pdata->ext_psy_name);
			max77807_charger_psy_setprop(me, psy_ext, PRESENT, false);
		}
	}

	if (unlikely(!me->psy_coop && me->pdata->coop_psy_name)) {
		me->psy_coop = power_supply_get_by_name(me->pdata->coop_psy_name);
		if (likely(me->psy_coop)) {
			log_dbg("psy %s found\n", me->pdata->coop_psy_name);
		}
	}
}

static void max77807_wlc_reset_worker (struct work_struct *work)
{
	struct max77807_charger *me =
			container_of(work, struct max77807_charger, wlc_reset_work.work);

	printk("max77807_wlc_reset_worker\n");

	wlc_align_value_sts_2s_wait = 0;

	if(max77807_wcharger_present_input(me) == false) {
		idtp9025_align_stop();

		printk("max77807_wlc_reset_worker => Reset WLC Alignment Value Zero\n");
		me->align_values = 0;
		align_value_sts = 0;
		power_supply_changed(&me->wireless);
	} else {
		printk("max77807_wlc_reset_worker => Keep WLC Alignment Value\n");
	}
}

static void max77807_wlc_int_unmask_worker (struct work_struct *work)
{
	struct max77807_charger *me =
			container_of(work, struct max77807_charger, wlc_unmask_work.work);
	int rc = 0;
	u8 val = 0;

	printk("max77807_wlc_int_unmask_worker\n");

	if(max77807_wcharger_present_input(me) == true) {
		printk("max77807_wlc_int_unmask_worker <WLC OK> return => Restart Align Worker\n");
		//schedule_delayed_work(&me->wireless_align_work, msecs_to_jiffies(500));
		queue_delayed_work(me->chg_internal_wq, &me->wireless_align_work,msecs_to_jiffies(500));
		return;
	}

	/* UnMask WLC Interrupt */
	rc = max77807_read(me->io, CHG_INT_MASK, &val);
	if (unlikely(IS_ERR_VALUE(rc))) {
		log_err("wireless_align_work CHG_INT_MASK read error [%d]\n", rc);
	} else {
		rc = max77807_write(me->io, CHG_INT_MASK, (val&0xDF));
		if (unlikely(IS_ERR_VALUE(rc))) {
			log_err("wireless_align_work CHG_INT_MASK write error [%d]\n", rc);
		} else {
			log_err("wireless_align_work UnMASK WLC Interrupt\n");
		}
	}

	/* Wake Unlock */
	if(wake_lock_active(&me->wlc_align_wakelock)) {
		wake_unlock(&me->wlc_align_wakelock);
		pr_info("wireless_align_work_<wlc_remove> [wake_unlock]\n");
	}
}

static void max77807_wlc_chrg_stop_worker (struct work_struct *work)
{
	struct max77807_charger *me =
			container_of(work, struct max77807_charger, wlc_chrg_stop_work.work);
	int rc = 0;
	u8 val = 0;

	printk("max77807_wlc_chrg_stop_worker\n");

	if(max77807_wcharger_present_input(me) == true) {
		printk("max77807_wlc_chrg_stop_worker WLC OK Return\n");
		return;
	}

	/* Set Charging Done Timer 0min*/
	rc = max77807_write(me->io, CHG_CNFG_03, 0x00);
	if (unlikely(IS_ERR_VALUE(rc))) {
		printk("wireless_align_work_2 set wlc_charging_done_timer fail\n");
	}

	chrg_curr_sts = 0;
	old_curr_sts = 0;

	me->vbus_check = false;
	vbus_noty(0);
	wlc_noty_otp = 0;

	wlc_chrg_sts = 0;

}

static void max77807_charger_check_chgin_worker (struct work_struct *work)
{
	struct max77807_charger *me =
		container_of(work, struct max77807_charger, check_chgin_work.work);
	u8 chgin_ok = 0;
	int rc;

	if(me->wireless_old_info)
	{
		rc = max77807_masked_read(me->io, CHG_INT_OK, CHG_INT_CHGIN_OK,
									FFS(CHG_INT_CHGIN_OK), &chgin_ok);
		if (unlikely(IS_ERR_VALUE(rc))) {
			return false;
		}
		if (chgin_ok == CHGIN_OK_INPUT_VALID) {

			printk("max77807 chgin worker chg_input OK\n");
			me->vbus_check = true;
			vbus_noty(1);

#ifdef CONFIG_USB_ODIN_DRD
			if (me->odin_otg && me->usb_init) {
				printk("max77807-charger enable vbus event \n");
				max77807_charger_vbus_event(me, 1, 450);
			}
#endif
			me->wireless_old_info = false;
		}


	}
			printk("max77807 worker end\n");

}
static void max77807_charger_check_cable_worker (struct work_struct *work)
{
	int usb_id = 0;
	u32 cabletype = BC_CABLETYPE_UNDEFINED;
	struct max77807_charger *me =
		container_of(work, struct max77807_charger, check_cable_work.work);
	u8 val = 0;
	int rc = 0;
	bool wlc_sts = 0;

	struct max77807_charger_platform_data *pdata = me->pdata;
	if (me->odin_otg) {
		cabletype = me->odin_otg->cabletype;
	} else {
		log_err("%s: odin_otg is not initialized\n", __func__);
		cabletype = BC_CABLETYPE_SDP;
	}

	usb_id = d2260_adc_get_usb_id();

	printk("%s: bc_cabletype %d, usb id %d\n", __func__, cabletype, usb_id);

	wlc_sts = max77807_wcharger_present_input(me);
	if(me->vbus_check == false && wlc_sts) {
		me->vbus_check = true;
		printk("%s: vbus_check is false but wlc is connected => Run Align Worker After 2 Second\n", __func__);
		queue_delayed_work(me->chg_internal_wq, &me->wireless_align_work,msecs_to_jiffies(WLC_ALIGN_INTERVAL));
	}

    /* Cable Type for battery temperature control driver */
	switch (cabletype) {
			case BC_CABLETYPE_UNDEFINED:
				if(me->vbus_check == false) {
					wlc_align_value_sts_2s_wait = 1;
					printk("max77807-charger setting for stop charging 1\n");
					me->dcin_type                = POWER_SUPPLY_TYPE_BATTERY;
					me->status      			 = POWER_SUPPLY_STATUS_DISCHARGING;
					me->charge_type              = POWER_SUPPLY_CHARGE_TYPE_NONE;
					max77807_charger_exit_dev(me);
					queue_delayed_work(me->chg_internal_wq, &me->check_chgin_work, msecs_to_jiffies(1000));
					me->slimport_old_info = false;
					max77807_write(me->io,CHG_SRC_INT_MASK, 0xF6);
					max77807_write(me->io,CHG_INT_MASK, 0x0F);
				} else { /* Cable Undefined & VBUS Connected */
					rc = max77807_read(me->io, CHG_DETAILS_00, &val);
					if (unlikely(IS_ERR_VALUE(rc))) {
						log_err("<<wireless vbus is not enough>> [%d]\n", rc);
					}
					//printk("%s: detail_01 test value = %X\n", __func__,val);
					if((val&0x18) == 0x18) {
						printk("%s: wireless charging current setting start\n", __func__);
						me->wireless_old_info = true;
						me->dcin_type                = POWER_SUPPLY_TYPE_WIRELESS;
						if(old_curr_sts == 1) {
							me->current_limit_volatile	 = 433000; /* 0.433A */
							me->charge_current_volatile  = 500000;
						} else if(old_curr_sts == 2) {
							me->current_limit_volatile	 = 433000; /* 0.433A */
							me->charge_current_volatile  = 500000;
						} else if(old_curr_sts == 3) {
							me->current_limit_volatile	 = 740000; /* 0.740A */
							me->charge_current_volatile  = 900000;
						} else {
							me->current_limit_volatile	 = 433000; /* 0.433A */
							me->charge_current_volatile  = 500000;
						}
						printk("max77807_charger_check_cable_worker = Wireless Charger set %d mA\n",(me->current_limit_volatile/1000));
						me->charge_type              = POWER_SUPPLY_CHARGE_TYPE_FAST;
						max77807_charger_psy_init(me);
						max77807_charger_init_dev(me);
					}else{

						printk("max77807-charger setting for stop charging 2\n");
						me->dcin_type                = POWER_SUPPLY_TYPE_BATTERY;
						me->status      			 = POWER_SUPPLY_STATUS_DISCHARGING;
						me->charge_type              = POWER_SUPPLY_CHARGE_TYPE_NONE;
						max77807_charger_exit_dev(me);
						me->slimport_old_info = false;
						max77807_write(me->io,CHG_SRC_INT_MASK, 0xF6);
						max77807_write(me->io,CHG_INT_MASK, 0x0F);
					}
				}
				break;

			case BC_CABLETYPE_SDP:
#if defined (CONFIG_SLIMPORT_ANX7808) || defined (CONFIG_SLIMPORT_ANX7812)
			case BC_CABLETYPE_SLIMPORT:
#endif
				me->dcin_type                = POWER_SUPPLY_TYPE_USB;
				//me->current_limit_volatile   = pdata->current_limit_usb; /* 0.5A */
				if(fake_battery_status == 1){
					me->current_limit_volatile   = 800000; /* 0.5A */
					me->charge_current_volatile  = 800000; /* 0.5A */
				}else{
					me->current_limit_volatile   = 500000; /* 0.5A */
					me->charge_current_volatile  = pdata->current_limit_usb; /* 0.5A */
				}
				me->charge_type              = POWER_SUPPLY_CHARGE_TYPE_FAST;
				max77807_charger_psy_init(me);
				max77807_charger_init_dev(me);
				max77807_write(me->io,CHG_SRC_INT_MASK, 0xF6);
				max77807_write(me->io, CHG_INT_MASK, 0xDF);
				if(slimport_is_connected()) {
					me->slimport_old_info = true;
					max77807_write(me->io,CHG_SRC_INT_MASK, 0xF6);
					max77807_write(me->io,CHG_INT_MASK, 0x0F);
					printk("max77807 slimport is inserted\n");
				}
				break;

			case BC_CABLETYPE_DCP: //#2 , TA
				me->dcin_type                = POWER_SUPPLY_TYPE_MAINS;
				//me->current_limit_volatile   = MAX77807_CHARGER_CURRENT_UNLIMIT;  /* UnLimit */
				//me->current_limit_volatile   = pdata->current_limit_ac;  /* UnLimit */
				me->current_limit_volatile   = 1800000;  /* UnLimit */
				me->charge_current_volatile  = pdata->current_limit_ac;           /* 1.5A */
				me->charge_type              = POWER_SUPPLY_CHARGE_TYPE_FAST;
				max77807_charger_psy_init(me);
				max77807_charger_init_dev(me);
				max77807_write(me->io,CHG_SRC_INT_MASK, 0xF6);
				max77807_write(me->io, CHG_INT_MASK, 0xDF);
				break;

			case BC_CABLETYPE_CDP: // #3 , 1.5A PC HOST
				me->dcin_type                = POWER_SUPPLY_TYPE_MAINS;
				//me->current_limit_volatile   = pdata->current_limit_ac;
				me->current_limit_volatile   = 1500000;  /* UnLimit */
				/* UnLimit */
				me->charge_current_volatile  = pdata->current_limit_ac;  /* 1.5A */
				me->charge_type              = POWER_SUPPLY_CHARGE_TYPE_FAST;
				max77807_charger_psy_init(me);
				max77807_charger_init_dev(me);
				max77807_write(me->io,CHG_SRC_INT_MASK, 0xF6);
				max77807_write(me->io, CHG_INT_MASK, 0xDF);
				break;

			default:
				me->dcin_type                = POWER_SUPPLY_TYPE_UNKNOWN;
				//me->current_limit_volatile   = pdata->current_limit_usb;          /* 0.5A */
				me->current_limit_volatile   = 500000; /* 0.5A */
				// me->current_limit_volatile   = MAX77807_CHARGER_CURRENT_MINIMUM;  /* 0.1A */
				me->charge_current_volatile  = pdata->current_limit_usb;          /* 0.5A */
				me->charge_type              = POWER_SUPPLY_CHARGE_TYPE_FAST;
				max77807_charger_psy_init(me);
				max77807_charger_init_dev(me);
				max77807_write(me->io,CHG_SRC_INT_MASK, 0xF6);
				max77807_write(me->io, CHG_INT_MASK, 0xDF);
				break;
		}

		call_sysfs_fuel();
		me->thrmgr_chrg_current = me->charge_current_volatile;
		me->thrmgr_input_current = me->current_limit_volatile;
		me->thrmgr_up_trig = 0 ;
		me->thrmgr_down_trig = 0;


	log_info("%s: type %d, limit %dmA, cc %dmA , boot_reason: %d\n", __func__, me->dcin_type,
	me->current_limit_volatile, me->charge_current_volatile,lge_get_boot_reason());

	batt_temp_ctrl_start(1);

	if(me->vbus_check && me->dcin_type == POWER_SUPPLY_TYPE_UNKNOWN) {
		cancel_delayed_work(&me->check_cable_work);
		queue_delayed_work(me->chg_internal_wq, &me->check_cable_work,
										msecs_to_jiffies(2000));
	}
}


int max77807_cable_type_return(void)
{
	if(global_charger == NULL) {
		printk("global charger is NULL\n");
		return 0;
	}

	return global_charger->odin_otg->cabletype;
}

static void max77807_charger_psy_changed (struct max77807_charger *me)
{
	max77807_charger_psy_init(me);

	if (me->dcin_type == POWER_SUPPLY_TYPE_USB) {
		if (likely(&me->usb)) {
			power_supply_changed(&me->usb);
		}
	} else if (me->dcin_type == POWER_SUPPLY_TYPE_MAINS) {
		if (likely(&me->ac)) {
			power_supply_changed(&me->ac);
		}
	} else if ((me->dcin_type == POWER_SUPPLY_TYPE_WIRELESS)&&((wlc_align_value_sts_2s_wait == 0))) {
		if (likely(&me->wireless)) {
			power_supply_changed(&me->wireless);
			//printk("max77807_charger_psy_changed_wireless_1\n");
		}
	} else {
		if (likely(&me->usb)) {
			power_supply_changed(&me->usb);
		}
		if (likely(&me->ac)) {
			power_supply_changed(&me->ac);
        }
        if ((likely(&me->wireless))&&(wlc_align_value_sts_2s_wait == 0)) {
            power_supply_changed(&me->wireless);
			//printk("max77807_charger_psy_changed_wireless_2\n");
		}
	}

	if (likely(me->psy_ext)) {
		power_supply_changed(me->psy_ext);
		//pr_err("max77807_charger_psy_changed_wireless_3\n");
	}

	if (likely(me->psy_coop)) {
		power_supply_changed(me->psy_coop);
		//pr_err("max77807_charger_psy_changed_wireless_4\n");
	}
}

struct max77807_charger_status_map {
int health, status, charge_type;
};

static struct max77807_charger_status_map max77807_charger_status_map[] = {
#define STATUS_MAP(_chg_dtls, _health, _status, _charge_type) \
		[CHG_DTLS_##_chg_dtls] = {\
		.health = POWER_SUPPLY_HEALTH_##_health,\
		.status = POWER_SUPPLY_STATUS_##_status,\
		.charge_type = POWER_SUPPLY_CHARGE_TYPE_##_charge_type,\
}
//                 chg_details_xx          	health               			status        		charge_type
STATUS_MAP(PREQUAL,     		GOOD,					CHARGING, 		TRICKLE),
STATUS_MAP(FASTCHARGE_CC,    	GOOD,       			CHARGING,     	FAST),
STATUS_MAP(FASTCHARGE_CV,    	GOOD,					CHARGING,     	FAST),
STATUS_MAP(TOPOFF,           	GOOD,       			CHARGING,     	FAST),
STATUS_MAP(DONE,             	GOOD,       			FULL,         	NONE),
STATUS_MAP(HIGH_TEMP_CHARGING, 	UNKNOWN,				CHARGING,		FAST),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
STATUS_MAP(OFF_TIMER_FAULT,		SAFETY_TIMER_EXPIRE,	NOT_CHARGING, 	NONE),
#else /* LINUX_VERSION_CODE ... */
STATUS_MAP(OFF_TIMER_FAULT,     UNKNOWN,             	NOT_CHARGING, 	NONE),
#endif /* LINUX_VERSION_CODE ... */
STATUS_MAP(OFF_SUSPEND,     	UNKNOWN,             	NOT_CHARGING, 	NONE),
STATUS_MAP(OFF_INPUT_INVALID,   UNKNOWN,             	NOT_CHARGING,   NONE),
STATUS_MAP(OFF_JUCTION_TEMP,    UNKNOWN,             	NOT_CHARGING, 	UNKNOWN),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
STATUS_MAP(OFF_WDT_EXPIRED, 	WATCHDOG_TIMER_EXPIRE,  NOT_CHARGING, 	UNKNOWN),
#else /* LINUX_VERSION_CODE ... */
STATUS_MAP(OFF_WDT_EXPIRED, 	UNKNOWN,				NOT_CHARGING,	UNKNOWN),
#endif /* LINUX_VERSION_CODE ... */
};

static int max77807_charger_update (struct max77807_charger *me , enum power_supply_type psy_type)
{
    int rc = 0;
	u8 val;
	u8 bat_dtls, chg_dtls, treg;
	u8 batp_dtls, wcin_dtls, chgin_dtls, chg_details_00;
	u8 byp_dtls;

	me->health      = POWER_SUPPLY_HEALTH_UNKNOWN;
	me->status      = POWER_SUPPLY_STATUS_UNKNOWN;
	me->charge_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	rc = max77807_read(me->io, CHG_DETAILS_00, &val);
	if (unlikely(IS_ERR_VALUE(rc))) {
		log_err("CHG_DETAILS_00 read error [%d]\n", rc);
		goto out;
	}
	batp_dtls = (val & CHG_DETAILS_BATP_DTLS)>>FFS(CHG_DETAILS_BATP_DTLS);
	wcin_dtls = (val & CHG_DETAILS_WCIN_DTLS)>>FFS(CHG_DETAILS_WCIN_DTLS);
	chgin_dtls = (val & CHG_DETAILS_CHGIN_DTLS)>>FFS(CHG_DETAILS_CHGIN_DTLS);

	log_vdbg("BATP_DTLS %Xh WCIN_DTLS %Xh CHGIN_DTLS %Xh\n", batp_dtls, wcin_dtls, chgin_dtls);

	rc = max77807_read(me->io, CHG_DETAILS_01, &val);
	if (unlikely(IS_ERR_VALUE(rc))) {
		log_err("CHG_DETAILS_01 read error [%d]\n", rc);
		goto out;
	}
	bat_dtls = (val & CHG_DETAILS_BAT_DTLS)>>FFS(CHG_DETAILS_BAT_DTLS);
	chg_dtls = (val & CHG_DETAILS_CHG_DTLS)>>FFS(CHG_DETAILS_CHG_DTLS);
	treg = (val & CHG_DETAILS_TREG)>>FFS(CHG_DETAILS_TREG);

	log_vdbg("BAT_DTLS %Xh CHG_DTLS %Xh TREG %Xh\n", bat_dtls, chg_dtls, treg);

	rc = max77807_read(me->io, CHG_DETAILS_02, &val);
	if (unlikely(IS_ERR_VALUE(rc))) {
		log_err("CHG_DETAILS_02 read error [%d]\n", rc);
		goto out;
	}
	byp_dtls = (val & CHG_DETAILS_BYP_DTLS)>>FFS(CHG_DETAILS_BYP_DTLS);

	log_vdbg("BYP_DTLS %Xh\n", byp_dtls);

	me->present = ((chgin_dtls == CHGIN_DTLS_VALID) || (wcin_dtls == WCIN_DTLS_VALID));
	if (unlikely(!me->present)) {
		/* no charger present */
		me->health      = POWER_SUPPLY_HEALTH_UNKNOWN;
		me->status      = POWER_SUPPLY_STATUS_DISCHARGING;
		me->charge_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		goto out;
	}

	me->health      = max77807_charger_status_map[chg_dtls].health;
	me->status      = max77807_charger_status_map[chg_dtls].status;
	me->charge_type = max77807_charger_status_map[chg_dtls].charge_type;

	if (likely(me->health != POWER_SUPPLY_HEALTH_UNKNOWN)) {
		goto out;
	}

	/* override health by TREG */
	if (treg != 0)
	me->health = POWER_SUPPLY_HEALTH_OVERHEAT;

out:
	log_vdbg("PRESENT %d HEALTH %d STATUS %d CHARGE_TYPE %d\n",
	me->present, me->health, me->status, me->charge_type);
	return rc;
}

#ifdef CONFIG_MAX77807_USBSW
static void max77807_detect_charger(struct max77807_charger *me)
{
	bool chg_input,slim_input, wc_input;
	bool vbus_ret = true;

	slim_input = max77807_charger_present_input_slimport(me);
	printk("max77807_detect_charger: 4.1V chg input %s\n", slim_input ? "inserted" : "removed");
	chg_input = max77807_charger_present_input(me);
	wc_input = max77807_wcharger_present_input(me);

	if (chg_input && slim_input) {
#if 0
		/* charger insert */
		me->vbus_check = true;
		vbus_flag= 1;
		vbus_noty(1);
#endif

#ifdef CONFIG_USB_ODIN_DRD
		if (me->odin_otg && me->usb_init) {
				printk("max77807-charger enable vbus event \n");
				vbus_ret = max77807_charger_vbus_event(me, 1, 450);
		}
#endif

		if(vbus_ret) {
			/* charger insert */
			me->vbus_check = true;
			vbus_flag= 1;
			vbus_noty(1);
		}
		else { /* OTG Vbus */
			if(!wc_input) {
				/* charger remove */
				me->vbus_check = false;
				vbus_flag= 0;
				vbus_noty(0);

				/* Check Cable */
				queue_delayed_work(me->chg_internal_wq, &me->check_cable_work, IRQ_WORK_DELAY);
			}
		}
	} else {
#if 0
			/* charger remove */
			me->vbus_check = false;
			vbus_flag= 0;
			vbus_noty(0);
#endif
			at_chargingoff_flag= 0;
			if(offmode_flag == 1){
					printk("max77807-charger wake lock timeout 1msec\n");
					wake_lock_timeout(&me->off_mode_lock,msecs_to_jiffies(3000));
					gpio_set_value(125, 0);
					me->dcin_type                = POWER_SUPPLY_TYPE_BATTERY;
					me->status      			 = POWER_SUPPLY_STATUS_DISCHARGING;
					me->charge_type              = POWER_SUPPLY_CHARGE_TYPE_NONE;
					max77807_charger_exit_dev(me);
					call_sysfs_fuel();
			}

#ifdef CONFIG_USB_ODIN_DRD
		if (me->odin_otg && me->usb_init) {
			printk("max77807-charger disable vbus event \n");
			/* Should set delay 600 ms to get disconnect event
			if the disconnect event with charger safeout is used */
			vbus_ret = max77807_charger_vbus_event(me, 0, 0);
		}
#endif

		if(vbus_ret) {
			/* charger remove */
			me->vbus_check = false;
			vbus_flag= 0;
			vbus_noty(0);
		}
		else { /* OTG Vbus */
			if(!wc_input) {
				/* charger remove */
				me->vbus_check = false;
				vbus_flag= 0;
				vbus_noty(0);

				/* Check Cable */
				queue_delayed_work(me->chg_internal_wq, &me->check_cable_work, IRQ_WORK_DELAY);
			}
		}
	}
	printk("max77807_detect_charger: Charger input %s\n", chg_input ? "inserted" : "removed");

	if(vbus_ret) {
		/* notify psy changed */
		max77807_charger_psy_changed(me);
	}

    return;

}
#endif

static void max77807_do_irq(struct max77807_charger *me, int irq)
{

	u8 val, chg_details[3];
	bool chg_input, wc_input;
	bool vbus_ret = true;
	int rc = 0;

	chg_details[0] = me->details_0;
	chg_details[1] = me->details_1;
	chg_details[2] = me->details_2;

	switch (1<<irq) {
#ifdef CONFIG_MAX77807_USBSW
		case CHG_INT_CHGIN:
			if(me->slimport_old_info == true){
				chg_input = max77807_charger_present_input_slimport(me);
			}else{
				chg_input = max77807_charger_present_input(me);
				wc_input = max77807_wcharger_present_input(me);
			}

			if (chg_input) {

				/* charger insert */
				me->vbus_check = true;
				vbus_noty(1);

				if(lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO){
					max77807_usbsw_read(0x05, &val);
					if(val & 0x01)
						me->dcin_type = POWER_SUPPLY_TYPE_USB;
					else if(val & 0x5)
						me->dcin_type = POWER_SUPPLY_TYPE_MAINS;
					max77807_charger_psy_init(me);
					call_sysfs_fuel();
				}

#ifdef CONFIG_USB_ODIN_DRD
				if (me->odin_otg && me->usb_init){
					printk("max77807-charger enable vbus event \n");
					vbus_ret = max77807_charger_vbus_event(me, 1, 450);
				}

				if(vbus_ret) {
					/* charger insert */
					me->vbus_check = true;
					vbus_noty(1);
				}
				else { /* OTG Vbus */
					if(!wc_input) {
						me->vbus_check = false;
						vbus_noty(0);

						/* Check Cable */
						queue_delayed_work(me->chg_internal_wq, &me->check_cable_work, IRQ_WORK_DELAY);
					}
				}
#endif
			} else {
				if (!wc_input) {
					me->vbus_check = false;
					vbus_noty(0);
					at_chargingoff_flag= 0;

					/* charger remove */
#ifdef CONFIG_USB_ODIN_DRD
					if (me->odin_otg && me->usb_init){
						printk("max77807-charger disable vbus event \n");
						/* Should set delay 600 ms to get disconnect event
						if the disconnect event with charger safeout is used */
						vbus_ret = max77807_charger_vbus_event(me, 0, 0);
					}
#endif
					call_sysfs_fuel();

					if(!vbus_ret) { /* OTG Vbus */
						/* Check Cable */
						queue_delayed_work(me->chg_internal_wq, &me->check_cable_work, IRQ_WORK_DELAY);
					}
				}
			}
			printk("max77807-charger CHG_INT_CHGIN: Charger input %s\n", chg_input ? "inserted" : "removed");
			break;
#endif
		case CHG_INT_WCIN:
			chg_input = max77807_charger_present_input(me);
			wc_input = max77807_wcharger_present_input(me);

			if((wc_input == true) && (wlc_connect_sts == 0)) {
				pr_err("max77807_do_irq <ISR is not WLC>\n");
			}

			if (wc_input && (wlc_connect_sts == 1)) {
				if (!chg_input) {
					me->vbus_check = true;
					vbus_noty(1);
					wlc_noty_otp = 1;

					/* wireless charger only */
					printk("wlc start charging check \n");

					/* Mask WLC Interrupt */
					rc = max77807_read(me->io, CHG_INT_MASK, &val);
					if (unlikely(IS_ERR_VALUE(rc))) {
						log_err("max77807_do_irq CHG_INT_MASK read error [%d]\n", rc);
					} else {
						rc = max77807_write(me->io, CHG_INT_MASK, (val|0x20));
						if (unlikely(IS_ERR_VALUE(rc))) {
							log_err("max77807_do_irq CHG_INT_MASK write error [%d]\n", rc);
						} else {
							log_err("max77807_do_irq MASK WLC Interrupt\n");
						}
					}

					/* Check Cable */
					queue_delayed_work(me->chg_internal_wq, &me->check_cable_work,IRQ_WORK_DELAY );
					/* wireless charger alignment start */
					wireless_align_proc(me, true);
					wlc_chrg_sts = 1;
				}
			} else {
				if (!chg_input) {
					me->vbus_check = false;
					vbus_noty(0);
					wlc_noty_otp = 0;
					wlc_align_value_sts_2s_wait = 1;

					/* wireless charger remove */
					printk("wlc stop charging\n");

#if 0
					/* UnMask WLC Interrupt */
					rc = max77807_read(me->io, CHG_INT_MASK, &val);
					if (unlikely(IS_ERR_VALUE(rc))) {
						log_err("max77807_do_irq CHG_INT_MASK read error [%d]\n", rc);
					} else {
						rc = max77807_write(me->io, CHG_INT_MASK, (val&0xDF));
						if (unlikely(IS_ERR_VALUE(rc))) {
							log_err("max77807_do_irq CHG_INT_MASK write error [%d]\n", rc);
						} else {
							log_err("max77807_do_irq UnMASK WLC Interrupt\n");
						}
					}
#endif
					queue_delayed_work(me->chg_internal_wq, &me->check_cable_work,IRQ_WORK_DELAY );
					wireless_align_proc(me, false);
					wlc_chrg_sts = 0;
				}
			}
			log_dbg("CHG_INT_WCIN: Wireless charger input %s\n", wc_input ? "inserted" : "removed");
			break;

		case CHG_INT_CHG:
			/* do insert code here */
			val = (chg_details[1] & CHG_DETAILS_CHG_DTLS)>>FFS(CHG_DETAILS_CHG_DTLS);
			log_dbg("CHG_INT_CHG: chg_dtls = %02Xh\n", val);
			break;

		case CHG_INT_BAT:
			/* do insert code here */
			val = (chg_details[1] & CHG_DETAILS_BAT_DTLS)>>FFS(CHG_DETAILS_BAT_DTLS);
			log_dbg("CHG_INT_BAT: bat_dtls = %02Xh\n", val);
			break;

		case CHG_INT_BATP:
			/* do insert code here */
			val = (chg_details[0] & CHG_DETAILS_BATP_DTLS)>>FFS(CHG_DETAILS_BATP_DTLS);
			log_dbg("CHG_INT_BATP: battery %s\n",
					val ? "no presence" : "presence");
			break;

		case CHG_INT_BYP:
			/* do insert code here */
			val = (chg_details[2] & CHG_DETAILS_BYP_DTLS)>>FFS(CHG_DETAILS_BYP_DTLS);
			log_dbg("CHG_INT_BYP: byp_dtls = %02Xh\n", val);
			if((me->dcin_type == POWER_SUPPLY_TYPE_USB) && (val & 0x08)){
				printk("max77807-charger input current limit as 100mA\n");

				me->current_limit_volatile = 100000;
				me->charge_current_volatile = 100000;
				rc = max77807_charger_set_charge_current(me, me->current_limit_volatile,
									me->charge_current_volatile);
				if (unlikely(IS_ERR_VALUE(rc))) {
					goto out;
				}
			}


			break;

		default:
			break;
	}

	if(vbus_ret) {
		/* notify psy changed */
		max77807_charger_psy_changed(me);
	}

out:
	return;
}

static void max77807_charger_irq_work (struct work_struct *work)
{
	struct max77807_charger *me =
		container_of(work, struct max77807_charger, irq_work.work);
	u8 irq, irq_mask = 0;
	int i;

	__lock(me);

	irq = me->irq;
	irq_mask = me->irq_mask;

#ifdef CONFIG_MAX77807_USBSW
	if (me->usbsw_irq & BIT_INT2_VBVOLT)
		max77807_detect_charger(me);
#endif

	for (i=0; i<8; i++) {
		if (((irq>>i) & 0x01) & (~(irq_mask>>i) & 0x01)) {
			max77807_do_irq(me, i);
		}
	}

	__unlock(me);
	return;
}

static irqreturn_t max77807_charger_isr (int irq, void *data)
{
	struct max77807_charger *me = data;
	u8 reg_irq, reg_irq_mask, reg_intsrc = 0;
	u8 reg_details[3];
#ifdef CONFIG_MAX77807_USBSW
	u8 val;
#endif
	int rc = 0;

	max77807_read(me->io, PMIC_INTSRC, &reg_intsrc);
	printk("<ISR> INTSRC %02Xh\n", reg_intsrc);
	if ((reg_intsrc & (INTSRC_CHGR_INT | INTSRC_USBSW_INT)) == 0)
		return IRQ_HANDLED;
	reg_irq = max77807_charger_read_irq_status(me, CHG_INT);
	reg_irq_mask = max77807_charger_read_irq_status(me, CHG_INT_MASK);
	max77807_bulk_read(me->io, CHG_DETAILS_00, reg_details, 3);
	printk("<ISR> CHG_INT %02Xh CHG_DETAILS %02Xh %02Xh %02Xh\n",
				reg_irq, reg_details[0], reg_details[1], reg_details[2]);
	printk("max77807-charger chg_int_mask :0x%x\n", reg_irq_mask);

	if(reg_details[0] == 0x18) {
		log_err("max77807_charger_isr <wlc_source_detected> MASK WLC_INT\n");
		rc = max77807_write(me->io, CHG_INT_MASK, 0xFF);
		if (unlikely(IS_ERR_VALUE(rc))) {
			log_err("max77807_charger_isr CHG_INT_MASK write error [%d]\n", rc);
		}
		wlc_connect_sts = 1;
	} else {
		wlc_connect_sts = 0;
	}

	me->irq = reg_irq;
	me->details_0 = reg_details[0];
	me->details_1 = reg_details[1];
	me->details_2 = reg_details[2];
	me->irq_mask = reg_irq_mask;

#ifdef CONFIG_MAX77807_USBSW
	max77807_usbsw_read(REG_USBSW_INT2, &val);
	me->usbsw_irq = val;
	max77807_usbsw_read(REG_USBSW_STATUS2, &val);
	me->usbsw_status = val;
#endif
	schedule_delayed_work(&me->irq_work, IRQ_WORK_DELAY);

#ifdef CONFIG_USB_ODIN_DRD
	if (me->usb_init)
		 __pm_wakeup_event(&me->ws, 1000);
#endif
	return IRQ_HANDLED;
}

static int max77807_charger_get_property (struct power_supply *psy,
														enum power_supply_property psp, union power_supply_propval *val)
{
	struct max77807_charger *me = NULL;
	int rc = 0;

#if 0
	me = (psy->type == POWER_SUPPLY_TYPE_MAINS) ?
	container_of(psy, struct max77807_charger, ac) :
	container_of(psy, struct max77807_charger, usb);
#else
	if(psy->type == POWER_SUPPLY_TYPE_MAINS){
		me = container_of(psy, struct max77807_charger, ac);
	} else if(psy->type == POWER_SUPPLY_TYPE_USB) {
		me = container_of(psy, struct max77807_charger, usb);
	} else if(psy->type == POWER_SUPPLY_TYPE_WIRELESS) {
		me = container_of(psy, struct max77807_charger, wireless);
	} else {
		me = container_of(psy, struct max77807_charger, usb);
	}
#endif

	__lock(me);

#if 0
	rc = max77807_charger_update(me, psy->type);
	if (unlikely(IS_ERR_VALUE(rc))) {
		goto out;
	}
#endif
	switch (psp) {
		case POWER_SUPPLY_PROP_PRESENT:
			/*      val->intval = me->present;*/ /* Original code is modified*/
			val->intval = me->present && (psy->type == me->dcin_type);
			break;

#ifndef POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED
		case POWER_SUPPLY_PROP_ONLINE:
#endif /* !POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED */
		case POWER_SUPPLY_PROP_CHRG_ENABLE:
		case POWER_SUPPLY_PROP_CHARGING_ENABLED:
			/*		val->intval = me->dev_enabled; */ /* Original code is modified*/
			val->intval = me->chg_enabled && (psy->type == me->dcin_type);
			break;

		case POWER_SUPPLY_PROP_CHARGING_ENABLE_USBSUSPEND:
			val->intval = !(me->chg_enabled && (psy->type == me->dcin_type));
	        break;

		case POWER_SUPPLY_PROP_STATUS:
		if(psy->type == POWER_SUPPLY_TYPE_WIRELESS) {
			val->intval = max77807_wcharger_present_input(me);
		}  else {
			val->intval = max77807_charger_present_input(me);
		}
			break;

		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			val->intval = me->charge_type;
			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
			val->intval = me->charge_current_volatile;
			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			val->intval = me->current_limit_volatile;
			break;

		case POWER_SUPPLY_PROP_ALIGNMENT:
			val->intval = align_value_sts;
			printk("wireless charging alignment set value < %d >\n",align_value_sts);
			break;

		case POWER_SUPPLY_PROP_THERMAL_MITIGATION:
			val->intval = me->thrmgr_chrg_current ;
			break ;

		case POWER_SUPPLY_PROP_BATT_TEMP_STATE:
			val->intval = me->thrmgr_batt_temp_state ;
			break ;

		case POWER_SUPPLY_PROP_OFF_MODE:
			val->intval = offmode_flag ;
			break ;

		default:
			rc = -EINVAL;
			goto out;
	}

out:
	log_vdbg("<get_property> psp %d val %d [%d]\n", psp, val->intval, rc);
	__unlock(me);
	return rc;
}

/* [START] Test code for Thermal Engine */
int pseudo_thermal_curr_ctrl_set(struct pseudo_thermal_curr_ctrl_info_type *info)
{
        struct max77807_charger *me =  NULL;
        int rc = 0;

        if(!global_charger)
                return -1;

        me =  global_charger;

        pseudo_thermal_curr_ctrl_info.current_mA = info->current_mA;

        printk("pseudo_thermal_curr_ctrl_set : (%d)mA\n",pseudo_thermal_curr_ctrl_info.current_mA);

        /* Set POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT */
        rc = max77807_charger_set_charge_current
                (me, MAX77807_CHARGER_CURRENT_MAXIMUM,((pseudo_thermal_curr_ctrl_info.current_mA)*1000));
        if (unlikely(IS_ERR_VALUE(rc))) {
                printk("pseudo_thermal_curr_ctrl_set : Current Setting Fail\n");
        }
        return 0;
}
EXPORT_SYMBOL(pseudo_thermal_curr_ctrl_set);
/* [END] */


static int max77807_charger_set_property (struct power_supply *psy,
enum power_supply_property psp, const union power_supply_propval *val)
{
	struct max77807_charger *me = NULL;
	int uA, rc = 0;
	int bat_temp ;

#if 0
	me = (psy->type == POWER_SUPPLY_TYPE_MAINS) ?
			container_of(psy, struct max77807_charger, ac) :
			container_of(psy, struct max77807_charger, usb);
#else
	if(psy->type == POWER_SUPPLY_TYPE_MAINS){
		me = container_of(psy, struct max77807_charger, ac);
	} else if(psy->type == POWER_SUPPLY_TYPE_USB) {
		me = container_of(psy, struct max77807_charger, usb);
	} else if(psy->type == POWER_SUPPLY_TYPE_WIRELESS) {
		me = container_of(psy, struct max77807_charger, wireless);
	} else {
		me = container_of(psy, struct max77807_charger, usb);
	}
#endif
	__lock(me);

	switch (psp) {
		case POWER_SUPPLY_PROP_CHRG_ENABLE: /* Except Charging Current Setting for OTP Drv */
			if(val->intval == 0) {
				rc = max77807_charger_set_charge_current(me, me->current_limit_volatile, 0);
			} else {
				rc = max77807_charger_set_charge_current(me, me->current_limit_volatile, me->charge_current_volatile);
			}
			break;

		case POWER_SUPPLY_PROP_CHARGING_ENABLED:
			rc = max77807_charger_set_enable(me, val->intval);
			if (unlikely(IS_ERR_VALUE(rc))) {
				goto out;
			}

			me->dev_enabled = val->intval;

			if(val->intval == 0){
				at_chargingoff_flag= 1;
				printk("max77807 ATD charging off as 60mA\n");
				me->current_limit_volatile = 60000;
				me->charge_current_volatile = 60000;
			}else{
				at_chargingoff_flag= 0;
				me->current_limit_volatile  = 1000000;
				me->charge_current_volatile = 1000000;
			}
			/* apply charge current */
			rc = max77807_charger_set_charge_current(me, me->current_limit_volatile,
					me->charge_current_volatile);
			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
			uA = abs(val->intval);

			me->charge_current_volatile  = uA;
			me->charge_current_permanent =
			val->intval > 0 ? uA : me->charge_current_permanent;

			rc = max77807_charger_set_charge_current(me, me->current_limit_volatile,
					uA);
			if (unlikely(IS_ERR_VALUE(rc))) {
				goto out;
			}

			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			uA = abs(val->intval);
			me->current_limit_volatile  = uA;
			me->current_limit_permanent =
			val->intval > 0 ? uA : me->current_limit_permanent;

			rc = max77807_charger_set_charge_current(me, uA,
					me->charge_current_volatile);
			if (unlikely(IS_ERR_VALUE(rc))) {
				goto out;
			}

			break;
		case POWER_SUPPLY_PROP_OFF_MODE:
			printk("max77807 off mode :%d\n", val->intval);
			if(val->intval == 0) {
				offmode_flag = 0;
				set_off_mode_flag(false);
//				max77807_charger_wake_lock(me,0);
			}
			else if(val->intval == 1) {
				offmode_flag = 1;
				set_off_mode_flag(true);
//				max77807_charger_wake_lock(me,1);
			}
			break;

		case POWER_SUPPLY_PROP_THERMAL_MITIGATION:
			printk("thermalmgr set charging current:%d type %d %d\n", val->intval, psy->type, me->charge_current_volatile);
			uA = abs(val->intval);
			if(uA < me->thrmgr_chrg_current){ // temp goes up
				me->thrmgr_down_trig = 0 ;
				me->thrmgr_up_trig = 1 ;

				if(uA < me->charge_current_volatile){ // check temp state
					me->charge_current_volatile  = uA;
					me->charge_current_permanent =
					val->intval > 0 ? uA : me->charge_current_permanent;

					rc = max77807_charger_set_charge_current(me, me->current_limit_volatile, uA);
					if (unlikely(IS_ERR_VALUE(rc))) {
						goto out;
					}
				}
			}

			if(uA > me->thrmgr_chrg_current){ // temp goes down
				me->thrmgr_down_trig = 1 ;
				me->thrmgr_up_trig = 0 ;
				if(me->thrmgr_batt_temp_state >= 4 ){ // lower than TEMP_LEVEL_HOT_RECHARGING
					me->charge_current_volatile  = uA;
					me->charge_current_permanent =
					val->intval > 0 ? uA : me->charge_current_permanent;
					rc = max77807_charger_set_charge_current(me, me->current_limit_volatile, uA);
					if (unlikely(IS_ERR_VALUE(rc))) {
						goto out;
					}
				}
			}
			me->thrmgr_chrg_current = uA ;
			break ;

		case POWER_SUPPLY_PROP_BATT_TEMP_STATE:
			printk("battery temp state:%d\n", val->intval);
			me->thrmgr_batt_temp_state = val->intval ;
			// thermal_mitigation_batt_temp_state = val->intval ;
			break ;
		default:
			rc = -EINVAL;
			goto out;
	}

out:
	log_vdbg("<set_property> psp %d val %d [%d]\n", psp, val->intval, rc);
	__unlock(me);
	return rc;
}

static int max77807_charger_property_is_writeable (struct power_supply *psy,
enum power_supply_property psp)
{
	switch (psp) {
		case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		case POWER_SUPPLY_PROP_CHRG_ENABLE:
		case POWER_SUPPLY_PROP_THERMAL_MITIGATION:
		return 1;

		default:
		break;
	}

	return -EINVAL;
}

static void max77807_charger_external_power_changed (struct power_supply *psy)
{
	struct max77807_charger *me = NULL;
	struct power_supply *supplicant;
	int i;

#if 0
	me = (psy->type == POWER_SUPPLY_TYPE_MAINS) ?
		container_of(psy, struct max77807_charger, ac) :
		container_of(psy, struct max77807_charger, usb);
#else
	if(psy->type == POWER_SUPPLY_TYPE_MAINS){
		me = container_of(psy, struct max77807_charger, ac);
	} else if(psy->type == POWER_SUPPLY_TYPE_USB) {
		me = container_of(psy, struct max77807_charger, usb);
	} else if(psy->type == POWER_SUPPLY_TYPE_WIRELESS) {
		me = container_of(psy, struct max77807_charger, wireless);
	} else {
		me = container_of(psy, struct max77807_charger, usb);
	}
#endif
	__lock(me);

	for (i = 0; i < me->pdata->num_supplicants; i++) {
		supplicant = power_supply_get_by_name(me->pdata->supplied_to[i]);
		if (likely(supplicant)) {
			power_supply_changed(supplicant);
			//pr_err("max77807_charger_psy_changed_wireless_5\n");
		}
	}

	__unlock(me);
}

static enum power_supply_property max77807_charger_psy_props[] = {
		POWER_SUPPLY_PROP_STATUS,
		POWER_SUPPLY_PROP_CHARGE_TYPE,
		POWER_SUPPLY_PROP_PRESENT,
#ifndef POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED
		POWER_SUPPLY_PROP_ONLINE,
#endif /* !POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED */
		POWER_SUPPLY_PROP_CHARGING_ENABLED,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,     /* charging current */
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, /* input current limit */
		POWER_SUPPLY_PROP_CHRG_ENABLE, 	   			   /* For OTP Drv */
		POWER_SUPPLY_PROP_OFF_MODE,
		POWER_SUPPLY_PROP_BATT_TEMP_STATE,
		POWER_SUPPLY_PROP_THERMAL_MITIGATION,
};

static enum power_supply_property max77807_wlc_charger_psy_props[] = {
		POWER_SUPPLY_PROP_STATUS,
		POWER_SUPPLY_PROP_CHARGE_TYPE,
		POWER_SUPPLY_PROP_PRESENT,
#ifndef POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED
		POWER_SUPPLY_PROP_ONLINE,
#endif /* !POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED */
		POWER_SUPPLY_PROP_CHARGING_ENABLED,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,     /* charging current */
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, /* input current limit */
		POWER_SUPPLY_PROP_CHRG_ENABLE, 	   			   /* For OTP Drv */
		POWER_SUPPLY_PROP_ALIGNMENT,
		POWER_SUPPLY_PROP_BATT_TEMP_STATE,
		POWER_SUPPLY_PROP_THERMAL_MITIGATION,
};

static void *max77807_charger_get_platdata (struct max77807_charger *me)
{
#ifdef CONFIG_OF
	struct device *dev = me->dev;
	struct device_node *np = dev->of_node;
	struct max77807_charger_platform_data *pdata;
	size_t sz;
	int num_supplicants, i;

	num_supplicants = of_property_count_strings(np, "supplied_to");
	num_supplicants = max(0, num_supplicants);

	sz = sizeof(*pdata) + num_supplicants * sizeof(char*);
	pdata = devm_kzalloc(dev, sz, GFP_KERNEL);
	if (unlikely(!pdata)) {
		log_err("out of memory (%uB requested)\n", sz);
		pdata = ERR_PTR(-ENOMEM);
		goto out;
	}

	of_property_read_u32(np, "irq_gpio",
	&pdata->irq);

	log_dbg("property:IRQ %d\n", pdata->irq);

	pdata->psy_name = "ac";
	of_property_read_string(np, "psy_name", (char const **)&pdata->psy_name);
	log_dbg("property:PSY NAME        %s\n", pdata->psy_name);

	of_property_read_string(np, "ext_psy_name",
	(char const **)&pdata->ext_psy_name);
	log_dbg("property:EXT PSY         %s\n",
	pdata->ext_psy_name ? pdata->ext_psy_name : "null");

	if (unlikely(num_supplicants <= 0)) {
		pdata->supplied_to     = NULL;
		pdata->num_supplicants = 0;
		log_dbg("property:SUPPLICANTS     null\n");
	} else {
		pdata->num_supplicants = (size_t)num_supplicants;
		log_dbg("property:SUPPLICANTS     %d\n", num_supplicants);
		pdata->supplied_to = (char**)(pdata + 1);
		for (i = 0; i < num_supplicants; i++) {
			of_property_read_string_index(np, "supplied_to", i,
			(char const **)&pdata->supplied_to[i]);
			log_dbg("property:SUPPLICANTS     %s\n", pdata->supplied_to[i]);
		}
	}

	pdata->fast_charge_timer = 0;	// disable
	of_property_read_u32(np, "fast_charge_timer",
	&pdata->fast_charge_timer);
	log_dbg("property:FCHGTIME         %uhour\n", pdata->fast_charge_timer);

	pdata->current_limit_usb = 500000;
	of_property_read_u32(np, "current_limit_usb",
	&pdata->current_limit_usb);
	log_dbg("property:DCILMT_USB      %uuA\n", pdata->current_limit_usb);

	pdata->current_limit_ac = 1500000;
	of_property_read_u32(np, "current_limit_ac",
	&pdata->current_limit_ac);
	log_dbg("property:DCILMT_AC       %uuA\n", pdata->current_limit_ac);

	pdata->fast_charge_current = 500000;
	of_property_read_u32(np, "fast_charge_current",
	&pdata->fast_charge_current);
	log_dbg("property:CHG_CC           %uuA\n", pdata->fast_charge_current);

	pdata->charge_termination_voltage = 4350000;
	of_property_read_u32(np, "charge_termination_voltage",
	&pdata->charge_termination_voltage);
	log_dbg("property:CHG_CV_PRM         %uuV\n",
	pdata->charge_termination_voltage);

	pdata->topoff_timer = 0;
	of_property_read_u32(np, "topoff_timer", &pdata->topoff_timer);
	log_dbg("property:TOPOFF_TIME      %umin\n", pdata->topoff_timer);

	pdata->topoff_current = 200000;
	of_property_read_u32(np, "topoff_current", &pdata->topoff_current);
	log_dbg("property:TOPOFF_ITH         %uuA\n", pdata->topoff_current);

	pdata->charge_restart_threshold = 150000;
	of_property_read_u32(np, "charge_restart_threshold",
	&pdata->charge_restart_threshold);
	log_dbg("property:CHG_RSTRT        %uuV\n", pdata->charge_restart_threshold);

	pdata->enable_coop = of_property_read_bool(np, "enable_coop");
	log_dbg("property:COOP CHG        %s\n",
	pdata->enable_coop ? "enabled" : "disabled");

	if (likely(pdata->enable_coop)) {
		of_property_read_string(np, "coop_psy_name",
		(char const **)&pdata->coop_psy_name);
		log_dbg("property:COOP CHG        %s\n",
		pdata->coop_psy_name ? pdata->coop_psy_name : "null");
	}

out:
	return pdata;
#else /* CONFIG_OF */
	return dev_get_platdata(me->dev) ?
	dev_get_platdata(me->dev) : ERR_PTR(-EINVAL);
#endif /* CONFIG_OF */
}

static __always_inline
void max77807_charger_destroy (struct max77807_charger *me)
{
	struct device *dev = me->dev;

	if (likely(me->irq > 0)) {
		devm_free_irq(dev, me->irq, me);
	}

	cancel_delayed_work_sync(&me->irq_work);

	/* Delete the work queue */
	destroy_workqueue(me->chg_internal_wq);
	if (likely(me->attr_grp)) {
		sysfs_remove_group(me->kobj, me->attr_grp);
	}

	if (likely(&me->ac)) {
		power_supply_unregister(&me->ac);
	}

	if (likely(&me->usb)) {
		power_supply_unregister(&me->usb);
	}

#ifdef CONFIG_OF
	if (likely(me->pdata)) {
		devm_kfree(dev, me->pdata);
	}
#endif /* CONFIG_OF */

	mutex_destroy(&me->lock);
	// spin_lock_destroy(&me->irq_lock);

	devm_kfree(dev, me);
}

#ifdef CONFIG_OF
static struct of_device_id max77807_charger_of_ids[] = {
		{ .compatible = "maxim,"MAX77807_CHARGER_NAME },
		{ },
};
MODULE_DEVICE_TABLE(of, max77807_charger_of_ids);
#endif /* CONFIG_OF */

bool max77807_check_factory_cable(struct max77807_charger *me)
{
	int temp=0;
	int usb_id = 0;

	usb_id = d2260_adc_get_usb_id();
	temp = d2260_get_battery_temp();

	log_info("is_factory_cable , usb_id :%d ,  get-factory-boot:%d , temper:%d \n",
						usb_id, lge_get_factory_boot(),temp);
	if(((usb_id > 90 && usb_id < 1200) || lge_get_factory_boot()) && (temp < -300))
		me->is_factory_cable = true;

	log_info("is_factory_cable :%d\n", me->is_factory_cable);

	if(me->is_factory_cable)
		max77807_charger_lock(me) ;

	return me->is_factory_cable;
}

static int set_reg(void *data, u64 val)
{
	struct max77807_charger *me;
	u32 addr = (u32) data;
	int ret;

	if (global_charger) {
		me = global_charger;
	}
	else{
		printk("==global_charger set_reg is not initialized yet==\n");
		return;
	}

	ret = max77807_write(me->io, addr, (u8) val);

	return ret;
}

static int get_reg(void *data, u64 *val)
{
	struct max77807_charger *me;
	u32 addr = (u32) data;
	u8 temp;
	int ret;

	if (global_charger) {
		me = global_charger;
	}
	else{
		printk("==global_charger get_reg is not initialized yet==\n");
		return;
	}

	ret = max77807_read(me->io, addr, &temp);
	if (ret < 0)
		return ret;

	*val = temp;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

static int max77807_chrg_create_debugfs_entries(struct max77807_charger *chip)
{
	int i;

	chip->dent = debugfs_create_dir(MAX77807_CHARGER_NAME, NULL);
	if (IS_ERR(chip->dent)) {
		pr_err("max77807_chrg driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(max77807_chrg_debug_regs) ; i++) {
		char *name = max77807_chrg_debug_regs[i].name;
		u32 reg = max77807_chrg_debug_regs[i].reg;
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

static int max77807_charger_probe (struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77807_dev *chip = dev_get_drvdata(dev->parent);
	struct max77807_charger *me = NULL;
	int rc = 0;
	int ret = 0;
	u8 val;
	u8 chg_int_mask, src_int_mask,cnfg_00;

	printk("max77807_charger_probe_start\n");

	me = devm_kzalloc(dev, sizeof(*me), GFP_KERNEL);
	if (unlikely(!me)) {
		log_err("out of memory (%uB requested)\n", (unsigned int)sizeof(*me));
		return -ENOMEM;
	}

	dev_set_drvdata(dev, me);

	spin_lock_init(&me->irq_lock);
	mutex_init(&me->lock);
	mutex_init(&me->align_lock);
	me->io   = max77807_get_io(chip);
	me->dev  = dev;
	me->kobj = &dev->kobj;
	me->irq  = -1;

	me->vbus_check = false;
	vbus_noty(0);
	me->is_factory_cable = false;

#ifdef CONFIG_USB_ODIN_DRD
	me->usb_init = 0;
	me->otg_vbus = false;
#endif

	me->chg_internal_wq = create_workqueue("max77087_internal_workqueue");
	if (!me->chg_internal_wq) {
		log_err("Can't create max77087_internal_workqueue\n");
		rc = -ENOMEM;
		goto abort;
	}

	INIT_DELAYED_WORK(&me->irq_work, max77807_charger_irq_work);
	INIT_DELAYED_WORK(&me->check_cable_work, max77807_charger_check_cable_worker);
	INIT_DELAYED_WORK(&me->check_chgin_work, max77807_charger_check_chgin_worker);
	INIT_DELAYED_WORK(&me->wlc_reset_work, max77807_wlc_reset_worker);
	INIT_DELAYED_WORK(&me->wlc_unmask_work, max77807_wlc_int_unmask_worker);
	INIT_DELAYED_WORK(&me->wlc_chrg_stop_work, max77807_wlc_chrg_stop_worker);

	me->pdata = max77807_charger_get_platdata(me);
	if (unlikely(IS_ERR(me->pdata))) {
		rc = PTR_ERR(me->pdata);
		me->pdata = NULL;

		log_err("failed to get platform data [%d]\n", rc);
		goto abort;
	}

	/* disable all IRQ */
	//max77807_write(me->io, CHGINTM1,   0xC6);
	// max77807_write(me->io, CHGINTMSK2, 0xFF);
	me->dev_enabled               =
	(!me->pdata->enable_coop || me->pdata->coop_psy_name);
	me->current_limit_permanent   = 100000;	// 100mA
	me->current_limit_volatile    = me->current_limit_permanent;
	me->charge_current_permanent  = me->pdata->fast_charge_current;
	me->charge_current_volatile   = me->charge_current_permanent;
	me->charge_type               = POWER_SUPPLY_CHARGE_TYPE_NONE;

	me->ac.name                     = "ac";
	me->ac.type                     = POWER_SUPPLY_TYPE_MAINS;
	me->ac.supplied_to              = me->pdata->supplied_to;
	me->ac.num_supplicants          = me->pdata->num_supplicants;
	me->ac.properties               = max77807_charger_psy_props;
	me->ac.num_properties           = ARRAY_SIZE(max77807_charger_psy_props);
	me->ac.get_property             = max77807_charger_get_property;
	me->ac.set_property             = max77807_charger_set_property;
	me->ac.property_is_writeable	= max77807_charger_property_is_writeable;
	me->ac.external_power_changed	= max77807_charger_external_power_changed;


	rc = power_supply_register(dev, &me->ac);
	if (unlikely(IS_ERR_VALUE(rc))) {
		log_err("failed to register ac_power_supply class [%d]\n", rc);
		goto abort;
	}
	me->usb.name                    = "usb";
	me->usb.type                    = POWER_SUPPLY_TYPE_USB;
	me->usb.properties              = max77807_charger_psy_props;
	me->usb.num_properties          = ARRAY_SIZE(max77807_charger_psy_props);
	me->usb.get_property            = max77807_charger_get_property;
	me->usb.set_property            = max77807_charger_set_property;
	me->usb.property_is_writeable   = max77807_charger_property_is_writeable;

	rc = power_supply_register(dev, &me->usb);
	if (unlikely(IS_ERR_VALUE(rc))) {
		log_err("failed to register usb_power_supply class [%d]\n", rc);
		goto abort;
	}

	me->wireless.name               	 = "wireless";
	me->wireless.type               	 = POWER_SUPPLY_TYPE_WIRELESS;
	me->wireless.properties              = max77807_wlc_charger_psy_props;
	me->wireless.num_properties          = ARRAY_SIZE(max77807_wlc_charger_psy_props);
	me->wireless.get_property            = max77807_charger_get_property;
	me->wireless.set_property            = max77807_charger_set_property;
	me->wireless.property_is_writeable   = max77807_charger_property_is_writeable;

	rc = power_supply_register(dev, &me->wireless);
	if (unlikely(IS_ERR_VALUE(rc))) {
		log_err("failed to register wireless_power_supply class [%d]\n", rc);
		goto abort;
	}

	/* wireless charging alignment */
	me->align_values = 0;
	INIT_DELAYED_WORK(&me->wireless_align_work, wireless_align_work);

#ifdef CONFIG_MAX77807_USBSW
	rc = max77807_usbsw_read(REG_USBSW_ID, &val);
	if (unlikely(IS_ERR_VALUE(rc))) {
		log_err("failed to read REG_USBSW_ID register[%d]\n", rc);
		goto abort;
	}
	max77807_usbsw_read(REG_USBSW_INT2, &val);
	me->usbsw_irq = val;
	max77807_usbsw_read(REG_USBSW_STATUS2, &val);
	me->usbsw_status = val;

#endif

	gpio_request(136, "max77807-irq");
	gpio_direction_input(136);
	me->irq = gpio_to_irq(136);
	printk("max77807 charger irq :%d \n",me->irq);

	if (unlikely(me->irq <= 0)) {
		log_warn("interrupt disabled\n");
		schedule_delayed_work(&me->irq_work, IRQ_WORK_INTERVAL);
	} else {
		rc = odin_gpio_sms_request_threaded_irq(me->irq, NULL, max77807_charger_isr,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_NO_SUSPEND, DRIVER_NAME, me);
		if (unlikely(IS_ERR_VALUE(rc))) {
			log_err("failed to request IRQ %d [%d]\n", me->irq, rc);
			me->irq = -1;
			goto abort;
		}
	}

	global_charger = me;

	if(max77807_check_factory_cable(me)) {
		printk("max77807-charger, goto out due to factory cable was inserted\n");
		goto fatory_cable;
	}

	wake_lock_init(&me->chg_wake_lock, WAKE_LOCK_SUSPEND,"Chg_Worker_Lock");
	wake_lock_init(&me->wlc_align_wakelock, WAKE_LOCK_SUSPEND,"wirelss_charging_alignment_Lock");
	wake_lock_init(&me->off_mode_lock, WAKE_LOCK_SUSPEND, "chg_off_mode_lock");

	max77807_charger_psy_init(me);
	max77807_charger_unlock(me);

	if (likely(max77807_charger_present_input(me))) {
		rc = max77807_charger_init_dev(me);
		if (unlikely(IS_ERR_VALUE(rc))) {
			goto abort;
		}
	}

	/* Reset charger IC*/
	rc = max77807_write(me->io,CHG_CNFG_00, 0x00);
	rc = max77807_write(me->io,CHG_CNFG_00, 0x05);

	//rc = max77807_write(me->io, CHG_INT_MASK, 0xDF);
	rc = max77807_write(me->io, CHG_INT_MASK, 0x9F);
	rc = max77807_write(me->io,CHG_SRC_INT_MASK, 0xF6);
	/* SAFEOUT charger IC*/
	rc = max77807_write(me->io,0xc6, 0x45);

	if (unlikely(IS_ERR_VALUE(rc))) {
		log_err("CHG_INT_MASK write error [%d]\n", rc);
	}

#ifdef CONFIG_USB_ODIN_DRD
	max77807_charger_usb_register(me);
	wakeup_source_init(&me->ws, "charger_lock");
#endif
	if(lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO){
		schedule_delayed_work(&me->irq_work, OFF_MODE_INTERVAL);
	}else
		schedule_delayed_work(&me->irq_work, IRQ_WORK_DELAY);
	log_info("driver "DRIVER_VERSION" installed\n");

	ret = max77807_chrg_create_debugfs_entries(me);
	if (ret) {
		pr_err("max77807_chrg_create_debugfs_entries failed\n");
	}
    printk("max77807_charger_probe_end\n");
	return 0;

fatory_cable:
#ifdef CONFIG_USB_ODIN_DRD
	max77807_charger_usb_register(me);
	wakeup_source_init(&me->ws, "charger_lock");
#endif
    return 0;

abort:
	if (me->dent)
		debugfs_remove_recursive(me->dent);
	dev_set_drvdata(dev, NULL);
	max77807_charger_destroy(me);
	return rc;
}

static int max77807_charger_remove (struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77807_charger *me = dev_get_drvdata(dev);
#ifdef CONFIG_USB_ODIN_DRD
	struct usb_phy *phy = NULL;
	phy = usb_get_phy(USB_PHY_TYPE_USB3);
#endif

	if (me->dent)
		debugfs_remove_recursive(me->dent);

	dev_set_drvdata(dev, NULL);
#ifdef CONFIG_USB_ODIN_DRD
	if (phy)
		usb_unregister_notifier(phy, &me->nb);
	wakeup_source_trash(&me->ws);
#endif
	max77807_charger_destroy(me);

	global_charger = NULL;

	return 0;
}

static int max77807_charger_shutdown (struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77807_charger *me = dev_get_drvdata(dev);
	printk("max77807-charger shutdown \n");
	max77807_charger_exit_dev(me);
	global_charger = NULL;
	return 0;
}


#ifdef CONFIG_PM_SLEEP
static int max77807_charger_suspend (struct device *dev)
{
	struct max77807_charger *me = dev_get_drvdata(dev);

	cancel_delayed_work(&me->irq_work);
	cancel_delayed_work(&me->wireless_align_work);

	return 0;
}

static int max77807_charger_resume (struct device *dev)
{
	struct max77807_charger *me = dev_get_drvdata(dev);

	log_vdbg("resuming\n");

	//schedule_delayed_work(&me->irq_work, IRQ_WORK_DELAY);
	if((max77807_wcharger_present_input(me)==true) && vbus_flag !=1){
		printk("max77807 resume wirelse align worker \n");
		//schedule_delayed_work(&me->wireless_align_work, 0);
		queue_delayed_work(me->chg_internal_wq, &me->wireless_align_work,msecs_to_jiffies(0));
	}
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(max77807_charger_pm, max77807_charger_suspend,
										max77807_charger_resume);
static struct platform_driver max77807_charger_driver =
{
	.driver.name            = DRIVER_NAME,
	.driver.owner           = THIS_MODULE,
	.driver.pm              = &max77807_charger_pm,
#ifdef CONFIG_OF
	.driver.of_match_table  = max77807_charger_of_ids,
#endif /* CONFIG_OF */
	.probe                  = max77807_charger_probe,
	.remove                 = max77807_charger_remove,
	.shutdown               = max77807_charger_shutdown,
};

static __init int max77807_charger_init (void)
{
	return platform_driver_register(&max77807_charger_driver);
}
late_initcall(max77807_charger_init);

static __exit void max77807_charger_exit (void)
{
	platform_driver_unregister(&max77807_charger_driver);
}
module_exit(max77807_charger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_VERSION(DRIVER_VERSION);


