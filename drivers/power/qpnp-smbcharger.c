/* Copyright (c) 2014 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "SMBCHG: %s: " fmt, __func__

#include <linux/spmi.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/bitops.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/spmi.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/debugfs.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/batterydata-lib.h>

#if defined(CONFIG_SLIMPORT_ANX7812) || defined(CONFIG_SLIMPORT_ANX7816)
#include <linux/slimport.h>
#endif

#ifdef CONFIG_LGE_PM
#include <soc/qcom/lge/board_lge.h>
#include <linux/qpnp/qpnp-adc.h>
#include <soc/qcom/smem.h>
#include <linux/wakelock.h>
#include <linux/reboot.h>
#include <linux/max17048_battery.h>
#define LGE_DEFAULT_TEMP  250
#endif

#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
#include <soc/qcom/lge/lge_charging_scenario.h>
#define MONITOR_BATTEMP_POLLING_PERIOD    (60*HZ)
#endif

#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
#include <linux/power/lge_battery_id.h>
#endif

#ifdef CONFIG_LGE_PM_UNIFIED_WLC
#include <linux/power/unified_wireless_charger.h>
#endif

/* Mask/Bit helpers */
#define _SMB_MASK(BITS, POS) \
	((unsigned char)(((1 << (BITS)) - 1) << (POS)))
#define SMB_MASK(LEFT_BIT_POS, RIGHT_BIT_POS) \
		_SMB_MASK((LEFT_BIT_POS) - (RIGHT_BIT_POS) + 1, \
				(RIGHT_BIT_POS))
#ifdef CONFIG_LGE_PM_CHARGE_INFO
#define CHARGING_INFORM_NORMAL_TIME     60000
#endif

#ifdef CONFIG_LGE_PM
#define LT_CABLE_56K		6
#define LT_CABLE_130K		7
#define LT_CABLE_910K		11

#define NULL_CHECK_VOID(p)  \
	if (!(p)) { \
		pr_err("FATAL (%s)\n", __func__); \
		return ; \
	}
#define NULL_CHECK(p, err)  \
	if (!(p)) { \
		pr_err("FATAL (%s)\n", __func__); \
		return err; \
	}
#endif

#ifdef CONFIG_LGE_PM
#define DCP_ICL_LIMITED		1800
#define DCP_FCC_LIMITED		1400
#define DCP_ICL_DEFAULT		1800
#define DCP_FCC_DEFAULT		1800

#define EVP_ICL_LIMITED		1400
#define EVP_FCC_LIMITED		1400
#define EVP_ICL_DEFAULT		1800
#define EVP_FCC_DEFAULT		2000

#define QC20_ICL_LIMITED	1400
#define QC20_FCC_LIMITED	1400
#define QC20_ICL_DEFAULT	1800
#define QC20_FCC_DEFAULT	2000

#define FACTORY_ICL_DEFAULT	1500
#define FACTORY_ICL_130K	500
#define FACTORY_FCC_DEFAULT	500
#define FACTORY_ICL_1200	1200
#define FACTORY_ICL_1000	1000

#define DC_ICL_DEFAULT		900
#define DC_DEC_LIMITED		500
#endif

#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
#define PSEUDO_BATT_USB_ICL	900
extern struct pseudo_batt_info_type pseudo_batt_info;
extern int safety_timer;
#endif

/* Config registers */
struct smbchg_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
};

struct parallel_usb_cfg {
	struct power_supply		*psy;
	int				min_current_thr_ma;
	int				min_9v_current_thr_ma;
	int				allowed_lowering_ma;
	int				current_max_ma;
	bool				avail;
	struct mutex			lock;
	int				initial_aicl_ma;
};

struct ilim_entry {
	int vmin_uv;
	int vmax_uv;
	int icl_pt_ma;
	int icl_lv_ma;
	int icl_hv_ma;
};

struct ilim_map {
	int			num;
	struct ilim_entry	*entries;
};

struct smbchg_chip {
	struct device			*dev;
	struct spmi_device		*spmi;

	/* peripheral register address bases */
	u16				chgr_base;
	u16				bat_if_base;
	u16				usb_chgpth_base;
	u16				dc_chgpth_base;
	u16				otg_base;
	u16				misc_base;

	int				fake_battery_soc;
	u8				revision[4];

	/* configuration parameters */
	int				iterm_ma;
	int				usb_max_current_ma;
	int				dc_max_current_ma;
	int				usb_target_current_ma;
	int				usb_tl_current_ma;
	int				dc_target_current_ma;
	int				target_fastchg_current_ma;
	int				fastchg_current_ma;
	int				vfloat_mv;
	int				resume_delta_mv;
	int				safety_time;
	int				prechg_safety_time;
	int				bmd_pin_src;
	bool				use_vfloat_adjustments;
	bool				iterm_disabled;
	bool				bmd_algo_disabled;
	bool				soft_vfloat_comp_disabled;
#ifdef CONFIG_LGE_PM
	bool				jeita_disabled;
	int				aicl_enabled;
	int				target_vfloat_mv;
#endif
	bool				chg_enabled;
	bool				low_icl_wa_on;
	bool				battery_unknown;
	bool				charge_unknown_battery;
	bool				chg_inhibit_en;
	bool				chg_inhibit_source_fg;
	u8				original_usbin_allowance;
	struct parallel_usb_cfg		parallel;
	struct delayed_work		parallel_en_work;
	struct dentry			*debug_root;

	/* wipower params */
	struct ilim_map			wipower_default;
	struct ilim_map			wipower_pt;
	struct ilim_map			wipower_div2;
	struct qpnp_vadc_chip		*vadc_dev;
	bool				wipower_dyn_icl_avail;
	struct ilim_entry		current_ilim;
	struct mutex			wipower_config;
	bool				wipower_configured;
	struct qpnp_adc_tm_btm_param	param;

	/* flash current prediction */
	int				rpara_uohm;
	int				rslow_uohm;

	/* vfloat adjustment */
	int				max_vbat_sample;
	int				n_vbat_samples;

	/* status variables */
	int				usb_suspended;
	int				dc_suspended;
	int				wake_reasons;
	bool				usb_online;
	bool				dc_present;
	bool				usb_present;
	bool				batt_present;
	int				otg_retries;
	ktime_t				otg_enable_time;
	bool				aicl_deglitch_on;

	/* jeita and temperature */
	bool				batt_hot;
	bool				batt_cold;
	bool				batt_warm;
	bool				batt_cool;
	unsigned int			thermal_levels;
	unsigned int			therm_lvl_sel;
	unsigned int			*thermal_mitigation;

	/* irqs */
	int				batt_hot_irq;
	int				batt_warm_irq;
	int				batt_cool_irq;
	int				batt_cold_irq;
	int				batt_missing_irq;
#ifdef CONFIG_LGE_PM
	int				batt_term_missing_irq;
#endif
	int				vbat_low_irq;
	int				chg_hot_irq;
	int				chg_term_irq;
	int				taper_irq;
	bool				taper_irq_enabled;
	struct mutex			taper_irq_lock;
	int				recharge_irq;
	int				fastchg_irq;
	int				safety_timeout_irq;
	int				power_ok_irq;
	int				dcin_uv_irq;
	int				usbin_uv_irq;
#ifdef CONFIG_LGE_PM
	int				usbin_ov_irq;
#endif
	int				src_detect_irq;
	int				otg_fail_irq;
	int				otg_oc_irq;
	int				aicl_done_irq;
	int				usbid_change_irq;
	int				chg_error_irq;
	bool				enable_aicl_wake;

	/* psy */
	struct power_supply		*usb_psy;
	struct power_supply		batt_psy;
	struct power_supply		dc_psy;
	struct power_supply		*bms_psy;
	int				dc_psy_type;
	const char			*bms_psy_name;
	const char			*battery_psy_name;
	bool				psy_registered;

	struct smbchg_regulator		otg_vreg;
	struct smbchg_regulator		ext_otg_vreg;
	struct work_struct		usb_set_online_work;
	struct work_struct		batt_soc_work;
	struct delayed_work		vfloat_adjust_work;
	spinlock_t			sec_access_lock;
	struct mutex			current_change_lock;
	struct mutex			usb_set_online_lock;
	struct mutex			usb_en_lock;
	struct mutex			dc_en_lock;
#ifdef CONFIG_LGE_PM_COMMON
	struct qpnp_vadc_chip           *vadc_usbin_dev;
	int				batfet_en;
	struct delayed_work		usb_insert_work;
#endif
#ifdef CONFIG_LGE_PM
	struct wake_lock                chg_wake_lock;
	struct wake_lock		uevent_wake_lock;
#endif
#ifdef CONFIG_LGE_PM_CHARGE_INFO
	struct delayed_work             charging_inform_work;
#endif
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	struct delayed_work             battemp_work;
	struct wake_lock                lcs_wake_lock;
	enum lge_btm_states             btm_state;
	int                             pseudo_ui_chg;
	int                             otp_ibat_current;
	int				chg_current_te;
	int 				is_parallel_chg_dis_by_otp;
	int				is_parallel_chg_changed_by_taper;
#endif
#if defined(CONFIG_LGE_PM_BATTERY_ID_CHECKER)
	int 				batt_id_smem;
	int 				batt_id;
#endif
#ifdef CONFIG_LGE_PM_MAXIM_EVP_CONTROL
	int				set_evp_vol;
	int				is_enable_evp_chg;
	int				retry_cnt_evp_chg;
	struct delayed_work		enable_evp_chg_work;
#endif
#ifdef CONFIG_LGE_PM_DETECT_QC20
	struct delayed_work		detect_QC20_work;
	struct delayed_work		enable_QC20_chg_work;
	int				is_enable_QC20_chg ;
	int				retry_cnt_QC20_chg;
	int				init_QC20;
#endif
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	struct qpnp_vadc_chip		*vadc_smb_dev;
	struct delayed_work		iusb_change_work;
	bool				fb_state; // 1: ON, 0: OFF
	bool				chg_state; // 1: limited, 0: default
	bool				enable_polling;
	int				polling_cnt;
	struct delayed_work		usb_remove_work;
	bool				check_aicl_complete;
	int 				is_parallel_chg_dis_by_taper;
#endif
	struct mutex			pm_lock;
};

#ifdef CONFIG_LGE_PM
static struct smbchg_chip *the_chip;

static unsigned int cable_type;
static unsigned int cable_smem_size;
static unsigned int factory_mode;
#ifdef CONFIG_MACH_MSM8994_Z2
static unsigned int batt_smem_present;
#endif
static bool is_factory_cable(void)
{
#ifdef CONFIG_LGE_PM_USB_ID
	unsigned int cable_info;
	cable_info = lge_pm_get_cable_type();

	if ((cable_info == CABLE_56K ||
		cable_info == CABLE_130K ||
		cable_info == CABLE_910K) ||
		(cable_type == LT_CABLE_56K ||
		cable_type == LT_CABLE_130K ||
		cable_type == LT_CABLE_910K))
		return true;
#else
	if (cable_type == LT_CABLE_56K ||
		cable_type == LT_CABLE_130K ||
		cable_type == LT_CABLE_910K)
		return true;
#endif
	else
		return false;
}

static bool is_factory_cable_130k(void)
{
	unsigned int cable_info;
	cable_info = lge_pm_get_cable_type();

	if (cable_info == CABLE_130K ||
		cable_type == LT_CABLE_130K)
		return true;
	else
		return false;
}
#ifdef CONFIG_MACH_MSM8994_Z2
static bool is_factory_cable_910k(void)
{
	unsigned int cable_info;
	cable_info = lge_pm_get_cable_type();

	if (cable_info == CABLE_910K ||
		cable_type == LT_CABLE_910K)
		return true;
	else
		return false;
}
#endif
#endif

enum print_reason {
	PR_REGISTER	= BIT(0),
	PR_INTERRUPT	= BIT(1),
	PR_STATUS	= BIT(2),
	PR_DUMP		= BIT(3),
	PR_PM		= BIT(4),
	PR_MISC		= BIT(5),
	PR_WIPOWER	= BIT(6),
#ifdef CONFIG_LGE_PM
	PR_LGE = BIT(7),
#endif
};

enum wake_reason {
	PM_PARALLEL_CHECK = BIT(0),
	PM_REASON_VFLOAT_ADJUST = BIT(1),
};

#ifdef CONFIG_LGE_PM
static int smbchg_debug_mask = PR_INTERRUPT|PR_LGE|PR_PM;
#else
static int smbchg_debug_mask;
#endif

module_param_named(
	debug_mask, smbchg_debug_mask, int, S_IRUSR | S_IWUSR
);

static int smbchg_parallel_en;
module_param_named(
        parallel_en, smbchg_parallel_en, int, S_IRUSR | S_IWUSR
);

#ifdef CONFIG_LGE_PM_USE_FG
static int smbchg_use_fg = 0;
module_param_named(
        use_fg, smbchg_use_fg, int, S_IRUSR | S_IWUSR
);
#endif

#ifdef CONFIG_LGE_PM_COMMON
#define pr_smb(reason, fmt, ...)                                \
        do {                                                    \
                if (smbchg_debug_mask & (reason))               \
                        pr_err(fmt, ##__VA_ARGS__);            \
                else                                            \
                        pr_debug(fmt, ##__VA_ARGS__);           \
        } while (0)
#else
#define pr_smb(reason, fmt, ...)				\
	do {							\
		if (smbchg_debug_mask & (reason))		\
			pr_info(fmt, ##__VA_ARGS__);		\
		else						\
			pr_debug(fmt, ##__VA_ARGS__);		\
	} while (0)
#endif

static int wipower_dyn_icl_en;
module_param_named(
	dynamic_icl_wipower_en, wipower_dyn_icl_en,
	int, S_IRUSR | S_IWUSR
);

static int wipower_dcin_interval = ADC_MEAS1_INTERVAL_2P0MS;
module_param_named(
	wipower_dcin_interval, wipower_dcin_interval,
	int, S_IRUSR | S_IWUSR
);

#define WIPOWER_DEFAULT_HYSTERISIS_UV	250000
static int wipower_dcin_hyst_uv = WIPOWER_DEFAULT_HYSTERISIS_UV;
module_param_named(
	wipower_dcin_hyst_uv, wipower_dcin_hyst_uv,
	int, S_IRUSR | S_IWUSR
);


#define pr_smb_rt(reason, fmt, ...)					\
	do {								\
		if (smbchg_debug_mask & (reason))			\
			pr_info_ratelimited(fmt, ##__VA_ARGS__);	\
		else							\
			pr_debug_ratelimited(fmt, ##__VA_ARGS__);	\
	} while (0)

static int smbchg_read(struct smbchg_chip *chip, u8 *val,
			u16 addr, int count)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;

	if (addr == 0) {
		dev_err(chip->dev, "addr cannot be zero addr=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return -EINVAL;
	}

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, addr, val, count);
	if (rc) {
		dev_err(chip->dev, "spmi read failed addr=0x%02x sid=0x%02x rc=%d\n",
				addr, spmi->sid, rc);
		return rc;
	}
	return 0;
}

/*
 * Writes an arbitrary number of bytes to a specified register
 *
 * Do not use this function for register writes if possible. Instead use the
 * smbchg_masked_write function.
 *
 * The sec_access_lock must be held for all register writes and this function
 * does not do that. If this function is used, please hold the spinlock or
 * random secure access writes may fail.
 */
static int smbchg_write(struct smbchg_chip *chip, u8 *val,
			u16 addr, int count)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;

	if (addr == 0) {
		dev_err(chip->dev, "addr cannot be zero addr=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return -EINVAL;
	}

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, addr, val, count);
	if (rc) {
		dev_err(chip->dev, "write failed addr=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return rc;
	}

	return 0;
}

/*
 * Writes a register to the specified by the base and limited by the bit mask
 *
 * Do not use this function for register writes if possible. Instead use the
 * smbchg_masked_write function.
 *
 * The sec_access_lock must be held for all register writes and this function
 * does not do that. If this function is used, please hold the spinlock or
 * random secure access writes may fail.
 */
static int smbchg_masked_write_raw(struct smbchg_chip *chip, u16 base, u8 mask,
									u8 val)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, base, 1);
	if (rc) {
		dev_err(chip->dev, "spmi read failed: addr=%03X, rc=%d\n",
				base, rc);
		return rc;
	}

	reg &= ~mask;
	reg |= val & mask;

	pr_smb(PR_REGISTER, "addr = 0x%x writing 0x%x\n", base, reg);

	rc = smbchg_write(chip, &reg, base, 1);
	if (rc) {
		dev_err(chip->dev, "spmi write failed: addr=%03X, rc=%d\n",
				base, rc);
		return rc;
	}

	return 0;
}

/*
 * Writes a register to the specified by the base and limited by the bit mask
 *
 * This function holds a spin lock to ensure secure access register writes goes
 * through. If the secure access unlock register is armed, any old register
 * write can unarm the secure access unlock, causing the next write to fail.
 *
 * Note: do not use this for sec_access registers. Instead use the function
 * below: smbchg_sec_masked_write
 */
static int smbchg_masked_write(struct smbchg_chip *chip, u16 base, u8 mask,
								u8 val)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&chip->sec_access_lock, flags);
	rc = smbchg_masked_write_raw(chip, base, mask, val);
	spin_unlock_irqrestore(&chip->sec_access_lock, flags);

	return rc;
}

/*
 * Unlocks sec access and writes to the register specified.
 *
 * This function holds a spin lock to exclude other register writes while
 * the two writes are taking place.
 */
#define SEC_ACCESS_OFFSET	0xD0
#define SEC_ACCESS_VALUE	0xA5
#define PERIPHERAL_MASK		0xFF
static int smbchg_sec_masked_write(struct smbchg_chip *chip, u16 base, u8 mask,
									u8 val)
{
	unsigned long flags;
	int rc;
	u16 peripheral_base = base & (~PERIPHERAL_MASK);

	spin_lock_irqsave(&chip->sec_access_lock, flags);

	rc = smbchg_masked_write_raw(chip, peripheral_base + SEC_ACCESS_OFFSET,
				SEC_ACCESS_VALUE, SEC_ACCESS_VALUE);
	if (rc) {
		dev_err(chip->dev, "Unable to unlock sec_access: %d", rc);
		goto out;
	}

	rc = smbchg_masked_write_raw(chip, base, mask, val);

out:
	spin_unlock_irqrestore(&chip->sec_access_lock, flags);
	return rc;
}

static void smbchg_stay_awake(struct smbchg_chip *chip, int reason)
{
	int reasons;

	mutex_lock(&chip->pm_lock);
	reasons = chip->wake_reasons | reason;
	if (reasons != 0 && chip->wake_reasons == 0) {
		pr_smb(PR_PM, "staying awake: 0x%02x (bit %d)\n",
				reasons, reason);
		pm_stay_awake(chip->dev);
	}
	chip->wake_reasons = reasons;
	mutex_unlock(&chip->pm_lock);
}

static void smbchg_relax(struct smbchg_chip *chip, int reason)
{
	int reasons;

	mutex_lock(&chip->pm_lock);
	reasons = chip->wake_reasons & (~reason);
	if (reasons == 0 && chip->wake_reasons != 0) {
		pr_smb(PR_PM, "relaxing: 0x%02x (bit %d)\n",
				reasons, reason);
		pm_relax(chip->dev);
	}
	chip->wake_reasons = reasons;
	mutex_unlock(&chip->pm_lock);
};

enum pwr_path_type {
	UNKNOWN = 0,
	PWR_PATH_BATTERY = 1,
	PWR_PATH_USB = 2,
	PWR_PATH_DC = 3,
};

#define PWR_PATH		0x08
#define PWR_PATH_MASK		0x03
static enum pwr_path_type smbchg_get_pwr_path(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + PWR_PATH, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read PWR_PATH rc = %d\n", rc);
		return PWR_PATH_BATTERY;
	}

	return reg & PWR_PATH_MASK;
}

#define RID_STS				0xB
#define RID_MASK			0xF
#define IDEV_STS			0x8
#define RT_STS				0x10
#define USBID_MSB			0xE
#define USBIN_UV_BIT			0x0
#define USBIN_OV_BIT			0x1
#define FMB_STS_MASK			SMB_MASK(3, 0)
#define USBID_GND_THRESHOLD		0x495
static bool is_otg_present(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;
	u8 usbid_reg[2];
	u16 usbid_val;

	/*
	 * There is a problem with USBID conversions on PMI8994 revisions
	 * 2.0.0. As a workaround, check that the cable is not
	 * detected as factory test before enabling OTG.
	 */
	rc = smbchg_read(chip, &reg, chip->misc_base + IDEV_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read IDEV_STS rc = %d\n", rc);
		return false;
	}

	if ((reg & FMB_STS_MASK) != 0) {
		pr_smb(PR_STATUS, "IDEV_STS = %02x, not ground\n", reg);
		return false;
	}

	rc = smbchg_read(chip, usbid_reg, chip->usb_chgpth_base + USBID_MSB, 2);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read USBID rc = %d\n", rc);
		return false;
	}
	usbid_val = (usbid_reg[0] << 8) | usbid_reg[1];

	if (usbid_val > USBID_GND_THRESHOLD) {
		pr_smb(PR_STATUS, "USBID = 0x%04x, too high to be ground\n",
				usbid_val);
		return false;
	}

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + RID_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev,
				"Couldn't read usb rid status rc = %d\n", rc);
		return false;
	}

#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "RID_STS = %02x\n", reg);
#else
	pr_smb(PR_STATUS, "RID_STS = %02x\n", reg);
#endif
	return (reg & RID_MASK) == 0;
}

#define USBIN_9V			BIT(5)
#define USBIN_UNREG			BIT(4)
#define USBIN_LV			BIT(3)
#define DCIN_9V				BIT(2)
#define DCIN_UNREG			BIT(1)
#define DCIN_LV				BIT(0)
#define INPUT_STS			0x0D
#define DCIN_UV_BIT			0x0
#define DCIN_OV_BIT			0x1
static bool is_dc_present(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->dc_chgpth_base + RT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read dc status rc = %d\n", rc);
		return false;
	}

	if ((reg & DCIN_UV_BIT) || (reg & DCIN_OV_BIT))
		return false;

	return true;
}

static bool is_usb_present(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + RT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb rt status rc = %d\n", rc);
		return false;
	}
	if ((reg & USBIN_UV_BIT) || (reg & USBIN_OV_BIT))
		return false;

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + INPUT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb status rc = %d\n", rc);
		return false;
	}

	return !!(reg & (USBIN_9V | USBIN_UNREG | USBIN_LV));
}

static char *usb_type_str[] = {
	"SDP",		/* bit 0 */
	"OTHER",	/* bit 1 */
	"DCP",		/* bit 2 */
	"CDP",		/* bit 3 */
	"NONE",		/* bit 4 error case */
};

#define N_TYPE_BITS		4
#define TYPE_BITS_OFFSET	4

static int get_type(u8 type_reg)
{
	unsigned long type = type_reg;
	type >>= TYPE_BITS_OFFSET;
	return find_first_bit(&type, N_TYPE_BITS);
}

/* helper to return the string of USB type */
static inline char *get_usb_type_name(int type)
{
	return usb_type_str[type];
}

static enum power_supply_type usb_type_enum[] = {
	POWER_SUPPLY_TYPE_USB,		/* bit 0 */
	POWER_SUPPLY_TYPE_UNKNOWN,	/* bit 1 */
	POWER_SUPPLY_TYPE_USB_DCP,	/* bit 2 */
	POWER_SUPPLY_TYPE_USB_CDP,	/* bit 3 */
	POWER_SUPPLY_TYPE_USB,		/* bit 4 error case, report SDP */
};

/* helper to return enum power_supply_type of USB type */
static inline enum power_supply_type get_usb_supply_type(int type)
{
	return usb_type_enum[type];
}

static enum power_supply_property smbchg_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
	POWER_SUPPLY_PROP_FLASH_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
#ifdef CONFIG_LGE_PM_COMMON
        POWER_SUPPLY_PROP_TEMP,
#endif
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
        POWER_SUPPLY_PROP_PSEUDO_BATT,
#endif
#ifdef CONFIG_LGE_PM_MAXIM_EVP_CONTROL
	POWER_SUPPLY_PROP_ENABLE_EVP_CHG,
#endif
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	POWER_SUPPLY_PROP_SAFTETY_CHARGER_TIMER,
#endif
#ifdef CONFIG_LGE_PM_COMMON
	POWER_SUPPLY_PROP_BATTERY_ID_CHECKER,
	POWER_SUPPLY_PROP_EXT_PWR_CHECK,
	POWER_SUPPLY_PROP_BATFET_EN,
#endif
#ifdef CONFIG_LGE_PM_EVP_TESTCMD_SUPPORT
	POWER_SUPPLY_PROP_EVP_TESTCMD_SUPPORT,
#endif
};

#define CHGR_STS			0x0E
#define BATT_LESS_THAN_2V		BIT(4)
#define CHG_HOLD_OFF_BIT		BIT(3)
#define CHG_TYPE_MASK			SMB_MASK(2, 1)
#define CHG_TYPE_SHIFT			1
#define BATT_NOT_CHG_VAL		0x0
#define BATT_PRE_CHG_VAL		0x1
#define BATT_FAST_CHG_VAL		0x2
#define BATT_TAPER_CHG_VAL		0x3
#define CHG_EN_BIT			BIT(0)
#define CHG_INHIBIT_BIT		BIT(1)
#define BAT_TCC_REACHED_BIT		BIT(7)
#ifdef CONFIG_LGE_PM
static int get_prop_batt_present(struct smbchg_chip *chip);
static int get_prop_batt_capacity(struct smbchg_chip *chip);
#endif
static int get_prop_batt_status(struct smbchg_chip *chip)
{
	int rc, status = POWER_SUPPLY_STATUS_DISCHARGING;
	u8 reg = 0, chg_type;

#ifdef CONFIG_LGE_PM
	int batt_present = get_prop_batt_present(chip);
	int capacity = get_prop_batt_capacity(chip);
	bool charger_present = is_usb_present(chip);
#ifdef CONFIG_LGE_PM_UNIFIED_WLC
	bool wireless_present = is_dc_present(chip);

	if(capacity >= 100 && batt_present && (charger_present || wireless_present))
#else
	if(capacity >= 100 && batt_present && charger_present)
#endif
		return POWER_SUPPLY_STATUS_FULL;
#else
	bool charger_present, chg_inhibit;

	rc = smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read RT_STS rc = %d\n", rc);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	if (reg & BAT_TCC_REACHED_BIT)
		return POWER_SUPPLY_STATUS_FULL;

	charger_present = is_usb_present(chip) | is_dc_present(chip);
	if (!charger_present)
		return POWER_SUPPLY_STATUS_DISCHARGING;

	chg_inhibit = reg & CHG_INHIBIT_BIT;
	if (chg_inhibit)
		return POWER_SUPPLY_STATUS_FULL;

#endif
	rc = smbchg_read(chip, &reg, chip->chgr_base + CHGR_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
#ifdef CONFIG_LGE_PM_UNIFIED_WLC
	if ((chip->usb_present || chip->dc_present )&& chip->pseudo_ui_chg)
#else
	if (chip->usb_present && chip->pseudo_ui_chg)
#endif
		return POWER_SUPPLY_STATUS_CHARGING;
#endif
	if (reg & CHG_HOLD_OFF_BIT) {
		/*
		 * when chg hold off happens the battery is
		 * not charging
		 */
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		goto out;
	}

	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;

	if (chg_type == BATT_NOT_CHG_VAL)
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	else
		status = POWER_SUPPLY_STATUS_CHARGING;
out:
	pr_smb_rt(PR_MISC, "CHGR_STS = 0x%02x\n", reg);
	return status;
}

#define BAT_PRES_STATUS			0x08
#define BAT_PRES_BIT			BIT(7)
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
#define LGE_FAKE_BATT_PRES		1
#endif
static int get_prop_batt_present(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	if (lge_get_fake_battery())
		return LGE_FAKE_BATT_PRES;
#endif

	rc = smbchg_read(chip, &reg, chip->bat_if_base + BAT_PRES_STATUS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
		return 0;
	}

#ifdef CONFIG_LGE_PM
	return !!(reg & BAT_PRES_BIT) || chip->batt_present;
#else
	return !!(reg & BAT_PRES_BIT);
#endif
}

static int get_prop_charge_type(struct smbchg_chip *chip)
{
	int rc;
	u8 reg, chg_type;

	rc = smbchg_read(chip, &reg, chip->chgr_base + CHGR_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
		return 0;
	}

	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;

#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "chg_type = %d\n", chg_type);
	if (chg_type != BATT_NOT_CHG_VAL) {
		if (!wake_lock_active(&chip->chg_wake_lock)) {
			pr_err("chg_wake_locked\n");
			wake_lock(&chip->chg_wake_lock);
		}
	} else {
		if (wake_lock_active(&chip->chg_wake_lock)) {
			pr_err("chg_wake_unlocked\n");
			wake_unlock(&chip->chg_wake_lock);
		}
	}
#endif
	if (chg_type == BATT_NOT_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	else if (chg_type == BATT_TAPER_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_TAPER;
	else if (chg_type == BATT_FAST_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (chg_type == BATT_PRE_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}


static int set_property_on_fg(struct smbchg_chip *chip,
		enum power_supply_property prop, int val)
{
	int rc;
	union power_supply_propval ret = {0, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);
	if (!chip->bms_psy) {
		pr_smb(PR_STATUS, "no bms psy found\n");
		return -EINVAL;
	}

	ret.intval = val;
	rc = chip->bms_psy->set_property(chip->bms_psy, prop, &ret);
	if (rc)
		pr_smb(PR_STATUS,
			"bms psy does not allow updating prop %d rc = %d\n",
			prop, rc);

	return rc;
}

static void smbchg_check_and_notify_fg_soc(struct smbchg_chip *chip)
{
	bool charger_present, charger_suspended;

	charger_present = is_usb_present(chip) | is_dc_present(chip);
	charger_suspended = chip->usb_suspended & chip->dc_suspended;

	if (charger_present && !charger_suspended
		&& get_prop_batt_status(chip)
			== POWER_SUPPLY_STATUS_CHARGING) {
		pr_smb(PR_STATUS, "Adjusting battery soc in FG\n");
		set_property_on_fg(chip, POWER_SUPPLY_PROP_CHARGE_FULL, 1);
	}
}

static int get_property_from_fg(struct smbchg_chip *chip,
		enum power_supply_property prop, int *val)
{
	int rc;
	union power_supply_propval ret = {0, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);
	if (!chip->bms_psy) {
		pr_smb(PR_STATUS, "no bms psy found\n");
		return -EINVAL;
	}

	rc = chip->bms_psy->get_property(chip->bms_psy, prop, &ret);
	if (rc) {
		pr_smb(PR_STATUS,
			"bms psy doesn't support reading prop %d rc = %d\n",
			prop, rc);
		return rc;
	}

	*val = ret.intval;
	return rc;
}
#define DEFAULT_BATT_CAPACITY	50
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
#define LGE_FAKE_BATT_CAPACITY		50
#endif
static int get_prop_batt_capacity(struct smbchg_chip *chip)
{
#if defined(CONFIG_BATTERY_MAX17048)
#if defined(CONFIG_LGE_PM_USE_FG)
	int capacity, rc;
#endif
#else
	int capacity, rc;
#endif

#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	if (lge_get_fake_battery())
		return LGE_FAKE_BATT_CAPACITY;
#endif
	if (chip->fake_battery_soc >= 0)
		return chip->fake_battery_soc;
#ifdef CONFIG_BATTERY_MAX17048
#ifdef CONFIG_LGE_PM_USE_FG
	if (smbchg_use_fg) {
		rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_CAPACITY, &capacity);
		if (rc) {
			pr_smb(PR_STATUS, "Couldn't get capacity rc = %d\n", rc);
			capacity = DEFAULT_BATT_CAPACITY;
		}
		if ((capacity == 100) && (get_prop_charge_type(chip)
						== POWER_SUPPLY_CHARGE_TYPE_TAPER
					|| get_prop_batt_status(chip)
						== POWER_SUPPLY_STATUS_FULL))
			smbchg_check_and_notify_fg_soc(chip);
		return capacity;
	}
#endif
	return max17048_get_capacity();
#else
	rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_CAPACITY, &capacity);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get capacity rc = %d\n", rc);
		capacity = DEFAULT_BATT_CAPACITY;
	}
	if ((capacity == 100) && (get_prop_charge_type(chip)
					== POWER_SUPPLY_CHARGE_TYPE_TAPER
				|| get_prop_batt_status(chip)
					== POWER_SUPPLY_STATUS_FULL))
		smbchg_check_and_notify_fg_soc(chip);
	return capacity;
#endif
}

#define DEFAULT_BATT_TEMP		200
static int get_prop_batt_temp(struct smbchg_chip *chip)
{
	int temp, rc;

	rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_TEMP, &temp);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get temperature rc = %d\n", rc);
		temp = DEFAULT_BATT_TEMP;
	}
	return temp;
}

#define DEFAULT_BATT_CURRENT_NOW	0
static int get_prop_batt_current_now(struct smbchg_chip *chip)
{
	int ua, rc;

	rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_CURRENT_NOW, &ua);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get current rc = %d\n", rc);
		ua = DEFAULT_BATT_CURRENT_NOW;
	}
	return ua;
}

#define DEFAULT_BATT_VOLTAGE_NOW	0
static int get_prop_batt_voltage_now(struct smbchg_chip *chip)
{
	int uv, rc;

	rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_VOLTAGE_NOW, &uv);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get voltage rc = %d\n", rc);
		uv = DEFAULT_BATT_VOLTAGE_NOW;
	}
	return uv;
}

#define DEFAULT_BATT_VOLTAGE_MAX_DESIGN	4200000
static int get_prop_batt_voltage_max_design(struct smbchg_chip *chip)
{
	int uv, rc;

	rc = get_property_from_fg(chip,
			POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN, &uv);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get voltage rc = %d\n", rc);
		uv = DEFAULT_BATT_VOLTAGE_MAX_DESIGN;
	}
	return uv;
}

static int get_prop_batt_health(struct smbchg_chip *chip)
{
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	if (chip->btm_state == BTM_HEALTH_OVERHEAT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	if (chip->btm_state == BTM_HEALTH_COLD)
		return POWER_SUPPLY_HEALTH_COLD;
	else
		return POWER_SUPPLY_HEALTH_GOOD;
#else
	if (chip->batt_hot)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (chip->batt_cold)
		return POWER_SUPPLY_HEALTH_COLD;
	else if (chip->batt_warm)
		return POWER_SUPPLY_HEALTH_WARM;
	else if (chip->batt_cool)
		return POWER_SUPPLY_HEALTH_COOL;
	else
		return POWER_SUPPLY_HEALTH_GOOD;
#endif
}

#ifdef CONFIG_LGE_PM_COMMON
int fcc_usb_current_table[] = {
        300,
        400,
        450,
        475,
        500,
        550,
        600,
        650,
        700,
        900,
        950,
        1000,
        1100,
        1200,
        1400,
        1450,
        1500,
        1600,
        1800,
        1850,
        1880,
        1910,
        1930,
        1950,
        1970,
        2000,
        2050,
        2100,
        2300,
        2400,
        2500,
        3000
};
#endif

int usb_current_table[] = {
	300,
	400,
	450,
	475,
	500,
	550,
	600,
	650,
	700,
	900,
	950,
	1000,
	1100,
	1200,
	1400,
	1450,
	1500,
	1600,
	1800,
	1850,
	1880,
	1910,
	1930,
	1950,
	1970,
	2000,
	2050,
	2100,
	2300,
	2400,
	2500,
	3000
};

int dc_current_table[] = {
	300,
	400,
	450,
	475,
	500,
	550,
	600,
	650,
	700,
	900,
	950,
	1000,
	1100,
	1200,
	1400,
	1450,
	1500,
	1600,
	1800,
	1850,
	1880,
	1910,
	1930,
	1950,
	1970,
	2000,
};

static int calc_thermal_limited_current(struct smbchg_chip *chip,
						int current_ma)
{
#ifdef CONFIG_LGE_PM_COMMON
	return current_ma;
#else
	int therm_ma;

	if (chip->therm_lvl_sel > 0
			&& chip->therm_lvl_sel < (chip->thermal_levels - 1)) {
		/*
		 * consider thermal limit only when it is active and not at
		 * the highest level
		 */
		therm_ma = (int)chip->thermal_mitigation[chip->therm_lvl_sel];
		if (therm_ma < current_ma) {
			pr_smb(PR_STATUS,
				"Limiting current due to thermal: %d mA",
				therm_ma);
			return therm_ma;
		}
	}

	return current_ma;
#endif
}

#define CMD_IL			0x40
#define USBIN_SUSPEND_BIT	BIT(4)
#define CURRENT_100_MA		100
#define CURRENT_150_MA		150
#define CURRENT_500_MA		500
#define CURRENT_900_MA		900
#define SUSPEND_CURRENT_MA	2
static int smbchg_usb_suspend(struct smbchg_chip *chip, bool suspend)
{
	int rc;

	rc = smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
			USBIN_SUSPEND_BIT, suspend ? USBIN_SUSPEND_BIT : 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set usb suspend rc = %d\n", rc);
	return rc;
}

#define DCIN_SUSPEND_BIT	BIT(3)
static int smbchg_dc_suspend(struct smbchg_chip *chip, bool suspend)
{
	int rc = 0;

	rc = smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
			DCIN_SUSPEND_BIT, suspend ? DCIN_SUSPEND_BIT : 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set dc suspend rc = %d\n", rc);
	return rc;
}

#define IL_CFG			0xF2
#define DCIN_INPUT_MASK	SMB_MASK(4, 0)
static int smbchg_set_dc_current_max(struct smbchg_chip *chip, int current_ma)
{
	int i;
	u8 dc_cur_val;

	for (i = ARRAY_SIZE(dc_current_table) - 1; i >= 0; i--) {
		if (current_ma >= dc_current_table[i])
			break;
	}

	if (i < 0) {
		dev_err(chip->dev, "Cannot find %dma current_table\n",
				current_ma);
		return -EINVAL;
	}

	chip->dc_max_current_ma = dc_current_table[i];
	dc_cur_val = i & DCIN_INPUT_MASK;

#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "dc current set to %d mA\n",
			chip->dc_max_current_ma);
#else
	pr_smb(PR_STATUS, "dc current set to %d mA\n",
			chip->dc_max_current_ma);
#endif
	return smbchg_sec_masked_write(chip, chip->dc_chgpth_base + IL_CFG,
				DCIN_INPUT_MASK, dc_cur_val);
}

enum enable_reason {
	/* userspace has suspended charging altogether */
	REASON_USER = BIT(0),
	/*
	 * this specific path has been suspended through the power supply
	 * framework
	 */
	REASON_POWER_SUPPLY = BIT(1),
	/*
	 * the usb driver has suspended this path by setting a current limit
	 * of < 2MA
	 */
	REASON_USB = BIT(2),
	/*
	 * when a wireless charger comes online,
	 * the dc path is suspended for a second
	 */
	REASON_WIRELESS = BIT(3),
	/*
	 * the thermal daemon can suspend a charge path when the system
	 * temperature levels rise
	 */
	REASON_THERMAL = BIT(4),
	/*
	 * an external OTG supply is being used, suspend charge path so the
	 * charger does not accidentally try to charge from the external supply.
	 */
	REASON_OTG = BIT(5),
};

static struct power_supply *get_parallel_psy(struct smbchg_chip *chip)
{
	if (!chip->parallel.avail)
		return NULL;
	if (chip->parallel.psy)
		return chip->parallel.psy;
	chip->parallel.psy = power_supply_get_by_name("usb-parallel");
	if (!chip->parallel.psy)
		pr_smb(PR_STATUS, "parallel charger not found\n");
	return chip->parallel.psy;
}

static void smbchg_usb_update_online_work(struct work_struct *work)
{
	struct smbchg_chip *chip = container_of(work,
				struct smbchg_chip,
				usb_set_online_work);
	bool user_enabled = (chip->usb_suspended & REASON_USER) == 0;
	int online = user_enabled && chip->usb_present;

	mutex_lock(&chip->usb_set_online_lock);
	if (chip->usb_online != online) {
		power_supply_set_online(chip->usb_psy, online);
		chip->usb_online = online;
#ifdef CONFIG_LGE_PM
		pr_smb(PR_LGE, "online = %d\n", online);
#endif
	}
	mutex_unlock(&chip->usb_set_online_lock);
}

static bool smbchg_primary_usb_is_en(struct smbchg_chip *chip,
		enum enable_reason reason)
{
	bool enabled;

	mutex_lock(&chip->usb_en_lock);
	enabled = (chip->usb_suspended & reason) == 0;
	mutex_unlock(&chip->usb_en_lock);

	return enabled;
}

static int smbchg_primary_usb_en(struct smbchg_chip *chip, bool enable,
		enum enable_reason reason, bool *changed)
{
	int rc = 0, suspended;

#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "usb %s, susp = %02x, en? = %d, reason = %02x\n",
                        chip->usb_suspended == 0 ? "enabled"
                        : "suspended", chip->usb_suspended, enable, reason);
#else
	pr_smb(PR_STATUS, "usb %s, susp = %02x, en? = %d, reason = %02x\n",
			chip->usb_suspended == 0 ? "enabled"
			: "suspended", chip->usb_suspended, enable, reason);
#endif
	mutex_lock(&chip->usb_en_lock);
	if (!enable)
		suspended = chip->usb_suspended | reason;
	else
		suspended = chip->usb_suspended & (~reason);

	/* avoid unnecessary spmi interactions if nothing changed */
	if (!!suspended == !!chip->usb_suspended) {
#ifdef CONFIG_LGE_PM
		pr_smb(PR_LGE, "usbin suspend state is not changed. keep current state."
				 " suspended = %02x, chip->usb_suspended = %02x\n", suspended, chip->usb_suspended);
#endif
		*changed = false;
		goto out;
	}

	*changed = true;
	rc = smbchg_usb_suspend(chip, suspended != 0);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't set usb suspend: %d rc = %d\n",
			suspended, rc);
		goto out;
	}

#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "usb charging %s, suspended = %02x\n",
			suspended == 0 ? "enabled"
			: "suspended", suspended);
#else
	pr_smb(PR_STATUS, "usb charging %s, suspended = %02x\n",
			suspended == 0 ? "enabled"
			: "suspended", suspended);
#endif
out:
	chip->usb_suspended = suspended;
	mutex_unlock(&chip->usb_en_lock);
	return rc;
}

static int smbchg_dc_en(struct smbchg_chip *chip, bool enable,
		enum enable_reason reason)
{
	int rc = 0, suspended;

	pr_smb(PR_LGE, "dc %s, susp = %02x, en? = %d, reason = %02x\n",
			chip->dc_suspended == 0 ? "enabled"
			: "suspended", chip->dc_suspended, enable, reason);
	mutex_lock(&chip->dc_en_lock);
	if (!enable)
		suspended = chip->dc_suspended | reason;
	else
		suspended = chip->dc_suspended & ~reason;

#ifdef CONFIG_LGE_PM
#else
	/* avoid unnecessary spmi interactions if nothing changed */
	if (!!suspended == !!chip->dc_suspended)
		goto out;
#endif

	rc = smbchg_dc_suspend(chip, suspended != 0);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't set dc suspend: %d rc = %d\n",
			suspended, rc);
		goto out;
	}

	if (chip->psy_registered)
		power_supply_changed(&chip->dc_psy);
#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "dc charging %s, suspended = %02x\n",
			suspended == 0 ? "enabled"
			: "suspended", suspended);
#else
	pr_smb(PR_STATUS, "dc charging %s, suspended = %02x\n",
			suspended == 0 ? "enabled"
			: "suspended", suspended);
#endif
out:
	chip->dc_suspended = suspended;
	mutex_unlock(&chip->dc_en_lock);
	return rc;
}

#define CHGPTH_CFG		0xF4
#define CFG_USB_2_3_SEL_BIT	BIT(7)
#define CFG_USB_2		0
#define CFG_USB_3		BIT(7)
#define USBIN_INPUT_MASK	SMB_MASK(4, 0)
#define USBIN_MODE_CHG_BIT	BIT(0)
#define USBIN_LIMITED_MODE	0
#define USBIN_HC_MODE		BIT(0)
#define USB51_MODE_BIT		BIT(1)
#define USB51_100MA		0
#define USB51_500MA		BIT(1)

#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
#define USB_AICL_CFG		0xF3
#define AICL_EN_BIT		BIT(2)
#define ICL_STS_1_REG		0x7
#define AICL_STS_BIT		BIT(5)
static void handle_usb_removal(struct smbchg_chip *chip);
static void smbchg_usb_remove_work(struct work_struct *work) {
	struct smbchg_chip *chip =
		container_of(work, struct smbchg_chip, usb_remove_work.work);
	bool usb_present = is_usb_present(chip);

	chip->check_aicl_complete = false;
	pr_smb(PR_LGE, "delayed usb present check\n");
	if (chip->usb_present && !usb_present) {
		chip->usb_present = usb_present;
		handle_usb_removal(chip);
	}
}
#endif

static int smbchg_set_high_usb_chg_current(struct smbchg_chip *chip,
							int current_ma)
{
	int i, rc;
	u8 usb_cur_val;
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	u8 reg;
#endif
	for (i = ARRAY_SIZE(usb_current_table) - 1; i >= 0; i--) {
		if (current_ma >= usb_current_table[i])
			break;
	}
	if (i < 0) {
		dev_err(chip->dev,
			"Cannot find %dma current_table using %d\n",
			current_ma, CURRENT_150_MA);

		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_2);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_100MA);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set %dmA rc=%d\n",
					CURRENT_150_MA, rc);
		else
			chip->usb_max_current_ma = 150;
		return rc;
	}

	usb_cur_val = i & USBIN_INPUT_MASK;
	rc = smbchg_sec_masked_write(chip, chip->usb_chgpth_base + IL_CFG,
				USBIN_INPUT_MASK, usb_cur_val);
	if (rc < 0) {
		dev_err(chip->dev, "cannot write to config c rc = %d\n", rc);
		return rc;
	}

	rc = smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
				USBIN_MODE_CHG_BIT, USBIN_HC_MODE);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't write cfg 5 rc = %d\n", rc);

#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	rc |= smbchg_read(chip, &reg, chip->usb_chgpth_base + USB_AICL_CFG, 1);
	if (reg & AICL_EN_BIT) {
		bool aicl_complete;
		u8 aicl_state;
		int cnt;

		chip->check_aicl_complete = true;
		msleep(200);
		while (!aicl_complete && cnt++ < 2) {
			rc |= smbchg_read(chip, &aicl_state,
				chip->usb_chgpth_base + ICL_STS_1_REG, 1);
			if (aicl_state & AICL_STS_BIT)
				aicl_complete = true;
			else
				msleep(50);
		}
		cancel_delayed_work(&chip->usb_remove_work);
		schedule_delayed_work(&chip->usb_remove_work, msecs_to_jiffies(400));
	}
#endif
	chip->usb_max_current_ma = usb_current_table[i];
	return rc;
}

/* if APSD results are used
 *	if SDP is detected it will look at 500mA setting
 *		if set it will draw 500mA
 *		if unset it will draw 100mA
 *	if CDP/DCP it will look at 0x0C setting
 *		i.e. values in 0x41[1, 0] does not matter
 */
static int smbchg_set_usb_current_max(struct smbchg_chip *chip,
							int current_ma)
{
	int rc;
	bool changed;

#ifdef CONFIG_LGE_PM_COMMON
	//                                                                                       
#else
	if (!chip->batt_present) {
		pr_info_ratelimited("Ignoring usb current->%d, battery is absent\n",
				current_ma);
		return 0;
	}
#endif
	pr_smb(PR_STATUS, "USB current_ma = %d\n", current_ma);

	/* to do: verify entering suspend state */
	if (current_ma == SUSPEND_CURRENT_MA) {
		/* suspend the usb if current set to 2mA */
		rc = smbchg_primary_usb_en(chip, false, REASON_USB, &changed);
		chip->usb_max_current_ma = 0;
		goto out;
	} else {
		rc = smbchg_primary_usb_en(chip, true, REASON_USB, &changed);
	}

	if (chip->low_icl_wa_on) {
		chip->usb_max_current_ma = current_ma;
#ifdef CONFIG_LGE_PM
		pr_smb(PR_LGE,
			"low_icl_wa on, ignoring the usb current setting\n");
#else
		pr_smb(PR_STATUS,
			"low_icl_wa on, ignoring the usb current setting\n");
#endif
		goto out;
	}
	if (current_ma < CURRENT_150_MA) {
		/* force 100mA */
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_2);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_100MA);
		chip->usb_max_current_ma = 100;
		goto out;
	}
	/* specific current values */
	if (current_ma == CURRENT_150_MA) {
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_3);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_100MA);
		chip->usb_max_current_ma = 150;
		goto out;
	}
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	if (chip->parallel.current_max_ma != 0)
		goto hc_mode;
#endif
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	if ( (pseudo_batt_info.mode == 1 || safety_timer == 0) &&
			(current_ma == PSEUDO_BATT_USB_ICL) )
		goto hc_mode;
#endif
	if (current_ma == CURRENT_500_MA) {
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_2);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_500MA);
		chip->usb_max_current_ma = 500;
		goto out;
	}
	if (current_ma == CURRENT_900_MA) {
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_3);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_500MA);
		chip->usb_max_current_ma = 900;
		goto out;
	}
#if defined(CONFIG_LGE_PM_PARALLEL_CHARGING) || defined(CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY)
hc_mode:
#endif
	rc = smbchg_set_high_usb_chg_current(chip, current_ma);
out:
#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "usb current set to %d mA\n",
			chip->usb_max_current_ma);
#else
	pr_smb(PR_STATUS, "usb current set to %d mA\n",
			chip->usb_max_current_ma);
#endif
	if (rc < 0)
		dev_err(chip->dev,
			"Couldn't set %dmA rc = %d\n", current_ma, rc);
	return rc;
}

#define USBIN_HVDCP_STS			0x0C
#define USBIN_HVDCP_SEL_BIT		BIT(4)
#define USBIN_HVDCP_SEL_9V_BIT		BIT(1)
static int smbchg_get_min_parallel_current_ma(struct smbchg_chip *chip)
{
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	return chip->parallel.min_current_thr_ma;
#else
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + USBIN_HVDCP_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb status rc = %d\n", rc);
		return 0;
	}
	if ((reg & USBIN_HVDCP_SEL_BIT) && (reg & USBIN_HVDCP_SEL_9V_BIT))
		return chip->parallel.min_9v_current_thr_ma;
	return chip->parallel.min_current_thr_ma;
#endif
}

#define ICL_STS_1_REG			0x7
#define ICL_STS_2_REG			0x9
#define ICL_STS_MASK			0x1F
#define AICL_STS_BIT			BIT(5)
#define USBIN_SUSPEND_STS_BIT		BIT(3)
#define USBIN_ACTIVE_PWR_SRC_BIT	BIT(1)
#define DCIN_ACTIVE_PWR_SRC_BIT		BIT(0)
static bool smbchg_is_parallel_usb_ok(struct smbchg_chip *chip)
{
	int min_current_thr_ma, rc, type;
	u8 reg;
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	u8 chg_type;

#if defined(CONFIG_SLIMPORT_ANX7812) || defined(CONFIG_SLIMPORT_ANX7816)
	union power_supply_propval prop = {0,};
#endif

	/* execute parallel charging only if the charger is version 2.x */
	if (chip->revision[1] < 2) {
		pr_smb(PR_LGE, "Parallel charging not enabled\n");
		return false;
	}

	if (is_factory_cable())
		return false;

	rc = smbchg_read(chip, &reg, chip->chgr_base + CHGR_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
		return false;
	}
	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;
	if (chg_type != BATT_FAST_CHG_VAL) {
		pr_smb(PR_LGE, "Not in fast charge, skipping\n");
		return false;
	}

	if (chip->is_parallel_chg_dis_by_taper) {
		pr_smb(PR_LGE, "Diabled by taper handler, skipping\n");
		return false;
	}

	if (chip->is_parallel_chg_dis_by_otp) {
		pr_smb(PR_LGE, "Diabled by LG OTP scenario, skipping\n");
		return false;
	}

#if defined(CONFIG_SLIMPORT_ANX7812) || defined(CONFIG_SLIMPORT_ANX7816)
	if (slimport_is_connected())
	{
		pr_smb(PR_LGE, "slimport_is_connected. get msm usb phy BC 1.2 result\n");
		rc = chip->usb_psy->get_property(chip->usb_psy,
					POWER_SUPPLY_PROP_TYPE, &prop);
		if (prop.intval == POWER_SUPPLY_TYPE_USB_CDP)
		{
			pr_smb(PR_LGE, "CDP adapter, skipping\n");
			return false;
		}

		if (prop.intval == POWER_SUPPLY_TYPE_USB)
		{
			pr_smb(PR_LGE, "SDP adapter, skipping\n");
			return false;
		}
	}
	else
	{
		rc = smbchg_read(chip, &reg, chip->misc_base + IDEV_STS, 1);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
			return false;
		}

		type = get_type(reg);
		if (get_usb_supply_type(type) == POWER_SUPPLY_TYPE_USB_CDP) {
			pr_smb(PR_LGE, "CDP adapter, skipping\n");
			return false;
		}

		if (get_usb_supply_type(type) == POWER_SUPPLY_TYPE_USB) {
			pr_smb(PR_LGE, "SDP adapter, skipping\n");
			return false;
		}

	}
#else
	rc = smbchg_read(chip, &reg, chip->misc_base + IDEV_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
		return false;
	}

	type = get_type(reg);
	if (get_usb_supply_type(type) == POWER_SUPPLY_TYPE_USB_CDP) {
		pr_smb(PR_LGE, "CDP adapter, skipping\n");
		return false;
	}

	if (get_usb_supply_type(type) == POWER_SUPPLY_TYPE_USB) {
		pr_smb(PR_LGE, "SDP adapter, skipping\n");
		return false;
	}
#endif
	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + ICL_STS_2_REG, 1);
	if (rc) {
		dev_err(chip->dev, "Could not read usb icl sts 2: %d\n", rc);
		return false;
	}
	/*
	 * If USBIN is suspended or not the active power source, do not enable
	 * parallel charging. The device may be charging off of DCIN.
	 */
	if (!!(reg & USBIN_SUSPEND_STS_BIT) ||
				!(reg & USBIN_ACTIVE_PWR_SRC_BIT)) {
		pr_smb(PR_LGE, "USB not active power source: %02x\n", reg);
		return false;
	}

	min_current_thr_ma = smbchg_get_min_parallel_current_ma(chip);
	if (min_current_thr_ma <= 0) {
		pr_smb(PR_LGE, "parallel charger unavailable for thr: %d\n",
				min_current_thr_ma);
		return false;
	}
	if (chip->usb_tl_current_ma < min_current_thr_ma) {
		pr_smb(PR_LGE, "Weak USB chg skip enable: %d < %d\n",
			chip->usb_tl_current_ma, min_current_thr_ma);
		return false;
	}
#else // QCT_Origin
	if (!smbchg_parallel_en) {
		pr_smb(PR_STATUS, "Parallel charging not enabled\n");
		return false;
	}

	if (get_prop_charge_type(chip) != POWER_SUPPLY_CHARGE_TYPE_FAST) {
		pr_smb(PR_STATUS, "Not in fast charge, skipping\n");
		return false;
	}

	if (get_prop_batt_health(chip) != POWER_SUPPLY_HEALTH_GOOD) {
		pr_smb(PR_STATUS, "JEITA active, skipping\n");
		return false;
	}

	rc = smbchg_read(chip, &reg, chip->misc_base + IDEV_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
		return false;
	}

	type = get_type(reg);
	if (get_usb_supply_type(type) == POWER_SUPPLY_TYPE_USB_CDP) {
		pr_smb(PR_STATUS, "CDP adapter, skipping\n");
		return false;
	}

	if (get_usb_supply_type(type) == POWER_SUPPLY_TYPE_USB) {
		pr_smb(PR_STATUS, "SDP adapter, skipping\n");
		return false;
	}

	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + ICL_STS_2_REG, 1);
	if (rc) {
		dev_err(chip->dev, "Could not read usb icl sts 2: %d\n", rc);
		return false;
	}

	/*
	 * If USBIN is suspended or not the active power source, do not enable
	 * parallel charging. The device may be charging off of DCIN.
	 */
	if (!!(reg & USBIN_SUSPEND_STS_BIT) ||
				!(reg & USBIN_ACTIVE_PWR_SRC_BIT)) {
		pr_smb(PR_STATUS, "USB not active power source: %02x\n", reg);
		return false;
	}

	min_current_thr_ma = smbchg_get_min_parallel_current_ma(chip);
	if (min_current_thr_ma <= 0) {
		pr_smb(PR_STATUS, "parallel charger unavailable for thr: %d\n",
				min_current_thr_ma);
		return false;
	}
	if (chip->usb_tl_current_ma < min_current_thr_ma) {
		pr_smb(PR_STATUS, "Weak USB chg skip enable: %d < %d\n",
			chip->usb_tl_current_ma, min_current_thr_ma);
		return false;
	}
#endif
	return true;
}

#define FCC_CFG			0xF2
#define FCC_500MA_VAL		0x4
#define FCC_MASK		SMB_MASK(4, 0)
static int smbchg_set_fastchg_current(struct smbchg_chip *chip,
							int current_ma)
{
	int i, rc;
	u8 cur_val;

	/* the fcc enumerations are the same as the usb currents */
	for (i = ARRAY_SIZE(usb_current_table) - 1; i >= 0; i--) {
		if (current_ma >= usb_current_table[i])
			break;
	}
	if (i < 0) {
		dev_err(chip->dev,
			"Cannot find %dma current_table using %d\n",
			current_ma, CURRENT_500_MA);

		rc = smbchg_sec_masked_write(chip, chip->chgr_base + FCC_CFG,
					FCC_MASK,
					FCC_500MA_VAL);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set %dmA rc=%d\n",
					CURRENT_500_MA, rc);
		else
			chip->fastchg_current_ma = 500;
		return rc;
	}

	cur_val = i & FCC_MASK;
	rc = smbchg_sec_masked_write(chip, chip->chgr_base + FCC_CFG,
				FCC_MASK, cur_val);
	if (rc < 0) {
		dev_err(chip->dev, "cannot write to fcc cfg rc = %d\n", rc);
		return rc;
	}

	chip->fastchg_current_ma = usb_current_table[i];
#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "fastcharge current set to %d\n",
			chip->fastchg_current_ma);
#else
	pr_smb(PR_STATUS, "fastcharge current set to %d\n",
			chip->fastchg_current_ma);
#endif
	return rc;
}

#ifdef CONFIG_LGE_PM
#define DC_AICL_CFG2				0xF4
#define AICL_THRESHOLD_5V_4P4			BIT(0)
#define AICL_THRESHOLD_5V_TO_9V_4P4		BIT(1)
#endif

#define USB_AICL_CFG				0xF3
#define AICL_EN_BIT				BIT(2)
static void smbchg_rerun_aicl(struct smbchg_chip *chip)
{
#ifdef CONFIG_LGE_PM
	if (is_factory_cable())
		return;
#endif
	smbchg_sec_masked_write(chip, chip->usb_chgpth_base + USB_AICL_CFG,
			AICL_EN_BIT, 0);
	/* Add a delay so that AICL successfully clears */
	msleep(50);
	smbchg_sec_masked_write(chip, chip->usb_chgpth_base + USB_AICL_CFG,
			AICL_EN_BIT, AICL_EN_BIT);
}

static void taper_irq_en(struct smbchg_chip *chip, bool en)
{
	mutex_lock(&chip->taper_irq_lock);
	if (en != chip->taper_irq_enabled) {
		if (en) {
			enable_irq(chip->taper_irq);
			enable_irq_wake(chip->taper_irq);
		} else {
			disable_irq_wake(chip->taper_irq);
			disable_irq_nosync(chip->taper_irq);
		}
		chip->taper_irq_enabled = en;
	}
	mutex_unlock(&chip->taper_irq_lock);
}

static void smbchg_parallel_usb_disable(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	union power_supply_propval pval = {0, };
#endif
	if (!parallel_psy)
		return;
#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "disabling parallel charger\n");
#else
	pr_smb(PR_STATUS, "disabling parallel charger\n");
#endif
	taper_irq_en(chip, false);
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	parallel_psy->get_property(parallel_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	if (pval.intval == 1 && chip->is_parallel_chg_dis_by_taper == 1) {
		pval.intval = chip->vfloat_mv;
		parallel_psy->set_property(parallel_psy,
			POWER_SUPPLY_PROP_FLOAT_VOLTAGE, &pval);
	}
#endif
	chip->parallel.initial_aicl_ma = 0;
	chip->parallel.current_max_ma = 0;
	power_supply_set_current_limit(parallel_psy,
				SUSPEND_CURRENT_MA * 1000);
	power_supply_set_present(parallel_psy, false);
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
	smbchg_set_usb_current_max(chip, pval.intval / 1000);
	smbchg_rerun_aicl(chip);
#else
	smbchg_set_fastchg_current(chip, chip->target_fastchg_current_ma);
	chip->usb_tl_current_ma =
		calc_thermal_limited_current(chip, chip->usb_target_current_ma);
	smbchg_set_usb_current_max(chip, chip->usb_tl_current_ma);
	smbchg_rerun_aicl(chip);
#endif
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	chip->is_parallel_chg_changed_by_taper = 0;
#endif
}

#define PARALLEL_TAPER_MAX_TRIES		3
#define PARALLEL_FCC_PERCENT_REDUCTION		75
#define MINIMUM_PARALLEL_FCC_MA			500
#define CHG_ERROR_BIT		BIT(0)
#define BAT_TAPER_MODE_BIT	BIT(6)
static void smbchg_parallel_usb_taper(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);
	union power_supply_propval pval = {0, };
	int parallel_fcc_ma, tries = 0;
	u8 reg = 0;

	if (!parallel_psy)
		return;

#ifdef CONFIG_LGE_PM
	if (is_factory_cable())
		return;
#endif

try_again:
	mutex_lock(&chip->parallel.lock);
	if (chip->parallel.current_max_ma == 0) {
		pr_smb(PR_STATUS, "Not parallel charging, skipping\n");
		goto done;
	}
	parallel_psy->get_property(parallel_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
	tries += 1;
	parallel_fcc_ma = pval.intval / 1000;
#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "try #%d parallel charger fcc = %d\n",
			tries, parallel_fcc_ma);
#else
	pr_smb(PR_STATUS, "try #%d parallel charger fcc = %d\n",
			tries, parallel_fcc_ma);
#endif
	if (parallel_fcc_ma < MINIMUM_PARALLEL_FCC_MA
				|| tries > PARALLEL_TAPER_MAX_TRIES) {
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
		chip->is_parallel_chg_dis_by_taper = 1;
#endif
		smbchg_parallel_usb_disable(chip);
		goto done;
	}
	pval.intval = 1000 * ((parallel_fcc_ma
			* PARALLEL_FCC_PERCENT_REDUCTION) / 100);
	parallel_psy->set_property(parallel_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	chip->is_parallel_chg_changed_by_taper = 1;
#endif
	/*
	 * sleep here for 100 ms in order to make sure the charger has a chance
	 * to go back into constant current charging
	 */
	mutex_unlock(&chip->parallel.lock);
	msleep(100);

	mutex_lock(&chip->parallel.lock);
	if (chip->parallel.current_max_ma == 0) {
		pr_smb(PR_STATUS, "Not parallel charging, skipping\n");
		goto done;
	}
	smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	if (reg & BAT_TAPER_MODE_BIT) {
		mutex_unlock(&chip->parallel.lock);
		goto try_again;
	}
	taper_irq_en(chip, true);
done:
	mutex_unlock(&chip->parallel.lock);
}

static int smbchg_get_aicl_level_ma(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + ICL_STS_1_REG, 1);
	if (rc) {
		dev_err(chip->dev, "Could not read usb icl sts 1: %d\n", rc);
		return 0;
	}
	reg &= ICL_STS_MASK;
	if (reg >= ARRAY_SIZE(usb_current_table)) {
		pr_warn("invalid AICL value: %02x\n", reg);
		return 0;
	}
	return usb_current_table[reg];
}

static void smbchg_parallel_usb_enable(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);
	union power_supply_propval pval = {0, };
	int current_limit_ma, parallel_cl_ma, total_current_ma;
	int new_parallel_cl_ma, min_current_thr_ma;

	if (!parallel_psy)
		return;

	pr_smb(PR_STATUS, "Attempting to enable parallel charger\n");
	min_current_thr_ma = smbchg_get_min_parallel_current_ma(chip);
	if (min_current_thr_ma <= 0) {
		pr_smb(PR_STATUS, "parallel charger unavailable for thr: %d\n",
				min_current_thr_ma);
		goto disable_parallel;
	}

	current_limit_ma = smbchg_get_aicl_level_ma(chip);
	if (current_limit_ma <= 0)
		goto disable_parallel;

	if (chip->parallel.initial_aicl_ma == 0) {
		if (current_limit_ma < min_current_thr_ma) {
			pr_smb(PR_STATUS, "Initial AICL very low: %d < %d\n",
				current_limit_ma, min_current_thr_ma);
			goto disable_parallel;
		}
		chip->parallel.initial_aicl_ma = current_limit_ma;
	}
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	else {
		pr_smb(PR_LGE, "already parallel charging %d/%d, skipping\n",
			chip->parallel.initial_aicl_ma, current_limit_ma);
		return;
	}
#endif

	/*
	 * Use the previous set current from the parallel charger.
	 * Treat 2mA as 0 because that is the suspend current setting
	 */
	parallel_cl_ma = chip->parallel.current_max_ma;
	if (parallel_cl_ma <= SUSPEND_CURRENT_MA)
		parallel_cl_ma = 0;

	/*
	 * Set the parallel charge path's input current limit (ICL)
	 * to the total current / 2
	 */
	total_current_ma = current_limit_ma + parallel_cl_ma;
	if (total_current_ma < chip->parallel.initial_aicl_ma
			- chip->parallel.allowed_lowering_ma) {
		pr_smb(PR_STATUS,
			"Too little total current : %d (%d + %d) < %d - %d\n",
			total_current_ma,
			current_limit_ma, parallel_cl_ma,
			chip->parallel.initial_aicl_ma,
			chip->parallel.allowed_lowering_ma);
		goto disable_parallel;
	}

	new_parallel_cl_ma = total_current_ma / 2;

	if (new_parallel_cl_ma == parallel_cl_ma) {
		pr_smb(PR_STATUS,
			"AICL at %d, old ICL: %d new ICL: %d, skipping\n",
			current_limit_ma, parallel_cl_ma, new_parallel_cl_ma);
		return;
	} else {
		pr_smb(PR_STATUS, "AICL at %d, old ICL: %d new ICL: %d\n",
			current_limit_ma, parallel_cl_ma, new_parallel_cl_ma);
	}

	taper_irq_en(chip, true);
	chip->parallel.current_max_ma = new_parallel_cl_ma;
	power_supply_set_present(parallel_psy, true);
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	/* setting order: input current > charging current */
	smbchg_set_usb_current_max(chip, chip->parallel.current_max_ma);
	power_supply_set_current_limit(parallel_psy,
				chip->parallel.current_max_ma * 1000);
	smbchg_set_fastchg_current(chip, chip->target_fastchg_current_ma / 2);
	pval.intval = chip->target_fastchg_current_ma * 1000 / 2;
	parallel_psy->set_property(parallel_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
	chip->enable_polling = true;
	chip->polling_cnt = 0;
#else
	smbchg_set_fastchg_current(chip, chip->target_fastchg_current_ma / 2);
	pval.intval = chip->target_fastchg_current_ma * 1000 / 2;
	parallel_psy->set_property(parallel_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);

	smbchg_set_usb_current_max(chip, chip->parallel.current_max_ma);
	power_supply_set_current_limit(parallel_psy,
				chip->parallel.current_max_ma * 1000);
#endif

	return;

disable_parallel:
#ifdef CONFIG_LGE_PM
	/* print parallel-disable reason */
	pr_smb(PR_LGE, "disable parallel: min_current_thr_ma: %d, current_limit_ma: %d\n",
			min_current_thr_ma, current_limit_ma);
	pr_smb(PR_LGE, "Too little total current : %d (%d + %d) < %d - %d\n",
		total_current_ma, current_limit_ma, parallel_cl_ma,
		chip->parallel.initial_aicl_ma,
		chip->parallel.allowed_lowering_ma);
#endif
	if (chip->parallel.current_max_ma != 0) {
		pr_smb(PR_STATUS, "disabling parallel charger\n");
		smbchg_parallel_usb_disable(chip);
	}
}

static void smbchg_parallel_usb_en_work(struct work_struct *work)
{
	struct smbchg_chip *chip = container_of(work,
				struct smbchg_chip,
				parallel_en_work.work);

	smbchg_relax(chip, PM_PARALLEL_CHECK);
	mutex_lock(&chip->parallel.lock);
	if (smbchg_is_parallel_usb_ok(chip))
		smbchg_parallel_usb_enable(chip);
	mutex_unlock(&chip->parallel.lock);
}

#define PARALLEL_CHARGER_EN_DELAY_MS	3500
static void smbchg_parallel_usb_check_ok(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);

	if (!parallel_psy)
		return;
	mutex_lock(&chip->parallel.lock);
	if (smbchg_is_parallel_usb_ok(chip)) {
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
		smbchg_parallel_usb_enable(chip);
#else
		smbchg_stay_awake(chip, PM_PARALLEL_CHECK);
		schedule_delayed_work(
			&chip->parallel_en_work,
			msecs_to_jiffies(PARALLEL_CHARGER_EN_DELAY_MS));
#endif
	} else if (chip->parallel.current_max_ma != 0) {
		pr_smb(PR_STATUS, "parallel charging unavailable\n");
		smbchg_parallel_usb_disable(chip);
	}
	mutex_unlock(&chip->parallel.lock);
}

static int smbchg_usb_en(struct smbchg_chip *chip, bool enable,
		enum enable_reason reason)
{
	bool changed = false;
	int rc = smbchg_primary_usb_en(chip, enable, reason, &changed);

	if (changed)
		smbchg_parallel_usb_check_ok(chip);
	return rc;
}

static struct ilim_entry *smbchg_wipower_find_entry(struct smbchg_chip *chip,
				struct ilim_map *map, int uv)
{
	int i;
	struct ilim_entry *ret = &(chip->wipower_default.entries[0]);

	for (i = 0; i < map->num; i++) {
		if (is_between(uv,
			map->entries[i].vmin_uv, map->entries[i].vmax_uv))
			ret = &map->entries[i];
	}
	return ret;
}

static int ilim_ma_table[] = {
	300,
	400,
	450,
	475,
	500,
	550,
	600,
	650,
	700,
	900,
	950,
	1000,
	1100,
	1200,
	1400,
	1450,
	1500,
	1600,
	1800,
	1850,
	1880,
	1910,
	1930,
	1950,
	1970,
	2000,
};

#define ZIN_ICL_PT	0xFC
#define ZIN_ICL_LV	0xFD
#define ZIN_ICL_HV	0xFE
#define ZIN_ICL_MASK	SMB_MASK(4, 0)
static int smbchg_dcin_ilim_config(struct smbchg_chip *chip, int offset, int ma)
{
	int i, rc;

	for (i = ARRAY_SIZE(ilim_ma_table); i >= 0; i--) {
		if (ma >= ilim_ma_table[i])
			break;
	}

	if (i < 0)
		i = 0;

	rc = smbchg_masked_write(chip, chip->bat_if_base + offset,
			ZIN_ICL_MASK, i);
	if (rc)
		dev_err(chip->dev, "Couldn't write bat if offset %d value = %d rc = %d\n",
				offset, i, rc);
	return rc;
}

static int smbchg_wipower_ilim_config(struct smbchg_chip *chip,
						struct ilim_entry *ilim)
{
	int rc = 0;

	if (chip->current_ilim.icl_pt_ma != ilim->icl_pt_ma) {
		rc = smbchg_dcin_ilim_config(chip, ZIN_ICL_PT, ilim->icl_pt_ma);
		if (rc)
			dev_err(chip->dev, "failed to write batif offset %d %dma rc = %d\n",
					ZIN_ICL_PT, ilim->icl_pt_ma, rc);
		else
			chip->current_ilim.icl_pt_ma =  ilim->icl_pt_ma;
	}

	if (chip->current_ilim.icl_lv_ma !=  ilim->icl_lv_ma) {
		rc = smbchg_dcin_ilim_config(chip, ZIN_ICL_LV, ilim->icl_lv_ma);
		if (rc)
			dev_err(chip->dev, "failed to write batif offset %d %dma rc = %d\n",
					ZIN_ICL_LV, ilim->icl_lv_ma, rc);
		else
			chip->current_ilim.icl_lv_ma =  ilim->icl_lv_ma;
	}

	if (chip->current_ilim.icl_hv_ma !=  ilim->icl_hv_ma) {
		rc = smbchg_dcin_ilim_config(chip, ZIN_ICL_HV, ilim->icl_hv_ma);
		if (rc)
			dev_err(chip->dev, "failed to write batif offset %d %dma rc = %d\n",
					ZIN_ICL_HV, ilim->icl_hv_ma, rc);
		else
			chip->current_ilim.icl_hv_ma =  ilim->icl_hv_ma;
	}
	return rc;
}

static void btm_notify_dcin(enum qpnp_tm_state state, void *ctx);
static int smbchg_wipower_dcin_btm_configure(struct smbchg_chip *chip,
		struct ilim_entry *ilim)
{
	int rc;

	if (ilim->vmin_uv == chip->current_ilim.vmin_uv
			&& ilim->vmax_uv == chip->current_ilim.vmax_uv)
		return 0;

	chip->param.channel = DCIN;
	chip->param.btm_ctx = chip;
	if (wipower_dcin_interval < ADC_MEAS1_INTERVAL_0MS)
		wipower_dcin_interval = ADC_MEAS1_INTERVAL_0MS;

	if (wipower_dcin_interval > ADC_MEAS1_INTERVAL_16S)
		wipower_dcin_interval = ADC_MEAS1_INTERVAL_16S;

	chip->param.timer_interval = wipower_dcin_interval;
	chip->param.threshold_notification = &btm_notify_dcin;
	chip->param.high_thr = ilim->vmax_uv + wipower_dcin_hyst_uv;
	chip->param.low_thr = ilim->vmin_uv - wipower_dcin_hyst_uv;
	chip->param.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
	rc = qpnp_vadc_channel_monitor(chip->vadc_dev, &chip->param);
	if (rc) {
		dev_err(chip->dev, "Couldn't configure btm for dcin rc = %d\n",
				rc);
	} else {
		chip->current_ilim.vmin_uv = ilim->vmin_uv;
		chip->current_ilim.vmax_uv = ilim->vmax_uv;
		pr_smb(PR_STATUS, "btm ilim = (%duV %duV %dmA %dmA %dmA)\n",
			ilim->vmin_uv, ilim->vmax_uv,
			ilim->icl_pt_ma, ilim->icl_lv_ma, ilim->icl_hv_ma);
	}
	return rc;
}

static int smbchg_wipower_icl_configure(struct smbchg_chip *chip,
						int dcin_uv, bool div2)
{
	int rc = 0;
	struct ilim_map *map = div2 ? &chip->wipower_div2 : &chip->wipower_pt;
	struct ilim_entry *ilim = smbchg_wipower_find_entry(chip, map, dcin_uv);

	rc = smbchg_wipower_ilim_config(chip, ilim);
	if (rc) {
		dev_err(chip->dev, "failed to config ilim rc = %d, dcin_uv = %d , div2 = %d, ilim = (%duV %duV %dmA %dmA %dmA)\n",
			rc, dcin_uv, div2,
			ilim->vmin_uv, ilim->vmax_uv,
			ilim->icl_pt_ma, ilim->icl_lv_ma, ilim->icl_hv_ma);
		return rc;
	}

	rc = smbchg_wipower_dcin_btm_configure(chip, ilim);
	if (rc) {
		dev_err(chip->dev, "failed to config btm rc = %d, dcin_uv = %d , div2 = %d, ilim = (%duV %duV %dmA %dmA %dmA)\n",
			rc, dcin_uv, div2,
			ilim->vmin_uv, ilim->vmax_uv,
			ilim->icl_pt_ma, ilim->icl_lv_ma, ilim->icl_hv_ma);
		return rc;
	}
	chip->wipower_configured = true;
	return 0;
}

static void smbchg_wipower_icl_deconfigure(struct smbchg_chip *chip)
{
	int rc;
	struct ilim_entry *ilim = &(chip->wipower_default.entries[0]);

	if (!chip->wipower_configured)
		return;

	rc = smbchg_wipower_ilim_config(chip, ilim);
	if (rc)
		dev_err(chip->dev, "Couldn't config default ilim rc = %d\n",
				rc);

	rc = qpnp_vadc_end_channel_monitor(chip->vadc_dev);
	if (rc)
		dev_err(chip->dev, "Couldn't de configure btm for dcin rc = %d\n",
				rc);

	chip->wipower_configured = false;
	chip->current_ilim.vmin_uv = 0;
	chip->current_ilim.vmax_uv = 0;
	chip->current_ilim.icl_pt_ma = ilim->icl_pt_ma;
	chip->current_ilim.icl_lv_ma = ilim->icl_lv_ma;
	chip->current_ilim.icl_hv_ma = ilim->icl_hv_ma;
	pr_smb(PR_WIPOWER, "De config btm\n");
}

#define FV_STS		0x0C
#define DIV2_ACTIVE	BIT(7)
static void __smbchg_wipower_check(struct smbchg_chip *chip)
{
	int chg_type;
	bool usb_present, dc_present;
	int rc;
	int dcin_uv;
	bool div2;
	struct qpnp_vadc_result adc_result;
	u8 reg;

	if (!wipower_dyn_icl_en) {
		smbchg_wipower_icl_deconfigure(chip);
		return;
	}

	chg_type = get_prop_charge_type(chip);
	usb_present = is_usb_present(chip);
	dc_present = is_dc_present(chip);
	if (chg_type != POWER_SUPPLY_CHARGE_TYPE_NONE
			 && !usb_present
			&& dc_present
			&& chip->dc_psy_type == POWER_SUPPLY_TYPE_WIPOWER) {
		rc = qpnp_vadc_read(chip->vadc_dev, DCIN, &adc_result);
		if (rc) {
			pr_smb(PR_STATUS, "error DCIN read rc = %d\n", rc);
			return;
		}
		dcin_uv = adc_result.physical;

		/* check div_by_2 */
		rc = smbchg_read(chip, &reg, chip->chgr_base + FV_STS, 1);
		if (rc) {
			pr_smb(PR_STATUS, "error DCIN read rc = %d\n", rc);
			return;
		}
		div2 = !!(reg & DIV2_ACTIVE);

		pr_smb(PR_WIPOWER,
			"config ICL chg_type = %d usb = %d dc = %d dcin_uv(adc_code) = %d (0x%x) div2 = %d\n",
			chg_type, usb_present, dc_present, dcin_uv,
			adc_result.adc_code, div2);
		smbchg_wipower_icl_configure(chip, dcin_uv, div2);
	} else {
		pr_smb(PR_WIPOWER,
			"deconfig ICL chg_type = %d usb = %d dc = %d\n",
			chg_type, usb_present, dc_present);
		smbchg_wipower_icl_deconfigure(chip);
	}
}

static void smbchg_wipower_check(struct smbchg_chip *chip)
{
	if (!chip->wipower_dyn_icl_avail)
		return;

	mutex_lock(&chip->wipower_config);
	__smbchg_wipower_check(chip);
	mutex_unlock(&chip->wipower_config);
}

static void btm_notify_dcin(enum qpnp_tm_state state, void *ctx)
{
	struct smbchg_chip *chip = ctx;

	mutex_lock(&chip->wipower_config);
	pr_smb(PR_WIPOWER, "%s state\n",
			state  == ADC_TM_LOW_STATE ? "low" : "high");
	chip->current_ilim.vmin_uv = 0;
	chip->current_ilim.vmax_uv = 0;
	__smbchg_wipower_check(chip);
	mutex_unlock(&chip->wipower_config);
}

static int force_dcin_icl_write(void *data, u64 val)
{
	struct smbchg_chip *chip = data;

	smbchg_wipower_check(chip);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_dcin_icl_ops, NULL,
		force_dcin_icl_write, "0x%02llx\n");

/*
 * set the dc charge path's maximum allowed current draw
 * that may be limited by the system's thermal level
 */
static int smbchg_set_thermal_limited_dc_current_max(struct smbchg_chip *chip,
							int current_ma)
{
	current_ma = calc_thermal_limited_current(chip, current_ma);
	return smbchg_set_dc_current_max(chip, current_ma);
}

/*
 * set the usb charge path's maximum allowed current draw
 * that may be limited by the system's thermal level
 */
static int smbchg_set_thermal_limited_usb_current_max(struct smbchg_chip *chip,
							int current_ma)
{
	int rc;

	chip->usb_tl_current_ma =
		calc_thermal_limited_current(chip, current_ma);
	rc = smbchg_set_usb_current_max(chip, chip->usb_tl_current_ma);

#ifdef CONFIG_LGE_PM_COMMON
	smbchg_rerun_aicl(chip);
#else
	smbchg_rerun_aicl(chip);
	smbchg_parallel_usb_check_ok(chip);
#endif
	return rc;
}

static int smbchg_system_temp_level_set(struct smbchg_chip *chip,
								int lvl_sel)
{
	int rc = 0;
	int prev_therm_lvl;

	if (!chip->thermal_mitigation) {
		dev_err(chip->dev, "Thermal mitigation not supported\n");
		return -EINVAL;
	}

	if (lvl_sel < 0) {
		dev_err(chip->dev, "Unsupported level selected %d\n", lvl_sel);
		return -EINVAL;
	}

	if (lvl_sel >= chip->thermal_levels) {
		dev_err(chip->dev, "Unsupported level selected %d forcing %d\n",
				lvl_sel, chip->thermal_levels - 1);
		lvl_sel = chip->thermal_levels - 1;
	}

	if (lvl_sel == chip->therm_lvl_sel)
		return 0;

	mutex_lock(&chip->current_change_lock);
	prev_therm_lvl = chip->therm_lvl_sel;
	chip->therm_lvl_sel = lvl_sel;
	if (chip->therm_lvl_sel == (chip->thermal_levels - 1)) {
		/*
		 * Disable charging if highest value selected by
		 * setting the DC and USB path in suspend
		 */
#ifdef CONFIG_LGE_PM_COMMON
#else
		rc = smbchg_dc_en(chip, false, REASON_THERMAL);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set dc suspend rc %d\n", rc);
			goto out;
		}
		rc = smbchg_usb_en(chip, false, REASON_THERMAL);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set usb suspend rc %d\n", rc);
			goto out;
		}
#endif
		goto out;
	}

	rc = smbchg_set_thermal_limited_usb_current_max(chip,
					chip->usb_target_current_ma);
#ifdef CONFIG_LGE_PM_COMMON
#else
	rc = smbchg_set_thermal_limited_dc_current_max(chip,
					chip->dc_target_current_ma);
#endif
	if (prev_therm_lvl == chip->thermal_levels - 1) {
		/*
		 * If previously highest value was selected charging must have
		 * been disabed. Enable charging by taking the DC and USB path
		 * out of suspend.
		 */
#ifdef CONFIG_LGE_PM_COMMON
#else
		rc = smbchg_dc_en(chip, true, REASON_THERMAL);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set dc suspend rc %d\n", rc);
			goto out;
		}
		rc = smbchg_usb_en(chip, true, REASON_THERMAL);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set usb suspend rc %d\n", rc);
			goto out;
		}
#endif
	}
out:
	mutex_unlock(&chip->current_change_lock);
	return rc;
}

#define UCONV			1000000LL
#define VIN_FLASH_UV		5500000LL
#define FLASH_V_THRESHOLD	3000000LL
#define BUCK_EFFICIENCY		800LL
static int smbchg_calc_max_flash_current(struct smbchg_chip *chip)
{
	int ocv_uv, ibat_ua, esr_uohm, rbatt_uohm, rc;
	int64_t ibat_flash_ua, total_flash_ua, total_flash_power_fw;

	rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_VOLTAGE_OCV, &ocv_uv);
	if (rc) {
		pr_smb(PR_STATUS, "bms psy does not support OCV\n");
		return 0;
	}

	rc = get_property_from_fg(chip,
			POWER_SUPPLY_PROP_CURRENT_NOW, &ibat_ua);
	if (rc) {
		pr_smb(PR_STATUS, "bms psy does not support current_now\n");
		return 0;
	}

	rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_RESISTANCE,
			&esr_uohm);
	if (rc) {
		pr_smb(PR_STATUS, "bms psy does not support resistance\n");
		return 0;
	}

	rbatt_uohm = esr_uohm + chip->rpara_uohm + chip->rslow_uohm;
	ibat_flash_ua = (div_s64((ocv_uv - FLASH_V_THRESHOLD) * UCONV,
			rbatt_uohm)) - ibat_ua;
	total_flash_power_fw = FLASH_V_THRESHOLD * ibat_flash_ua
			* BUCK_EFFICIENCY;
	total_flash_ua = div64_s64(total_flash_power_fw, VIN_FLASH_UV * 1000LL);
	pr_smb(PR_MISC,
		"ibat_flash=%lld\n, ocv=%d, ibat=%d, rbatt=%d t_flash=%lld\n",
		ibat_flash_ua, ocv_uv, ibat_ua, rbatt_uohm, total_flash_ua);
	return (int)total_flash_ua;
}

#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
#define SFT	0xfd
#define SFT_EN	SMB_MASK(5,4)
static void smbchg_enable_safety_timer(struct smbchg_chip *chip, bool enable)
{
	int rc = 0;
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	struct power_supply *parallel_psy;
	union power_supply_propval ret = {0,};
#endif

	rc = smbchg_sec_masked_write(chip,
                                chip->chgr_base + SFT,
                                SFT_EN, enable ? 0 : SFT_EN);
	if (rc < 0)
		pr_err("failed to set safety timer enable = %d\n", enable);

	pr_smb(PR_LGE,"set safety timer enable = %d\n", enable);

#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	parallel_psy = get_parallel_psy(chip);
	if (!parallel_psy)
		return;

	ret.intval = enable;
	parallel_psy->set_property(parallel_psy,
		POWER_SUPPLY_PROP_SAFTETY_CHARGER_TIMER, &ret);
#endif
}
#endif

#ifdef CONFIG_LGE_PM_COMMON
#define DEFAULT_VBUS_UV 5000
static int smbchg_get_usb_adc(struct smbchg_chip *chip)
{
	struct qpnp_vadc_result results;
	int rc, usbin_vol;

	if (!chip->usb_present)
		return DEFAULT_VBUS_UV;

	if (IS_ERR_OR_NULL(chip->vadc_usbin_dev)) {
		chip->vadc_usbin_dev = qpnp_get_vadc(chip->dev, "chg");
		if (IS_ERR_OR_NULL(chip->vadc_usbin_dev)) {
			pr_err("vadc is not init yet");
			return DEFAULT_VBUS_UV;
		}
	}

	rc = qpnp_vadc_read(chip->vadc_usbin_dev, USBIN, &results);
	if (rc < 0) {
		dev_err(chip->dev, "failed to read usb in voltage. rc = %d\n", rc);
		return DEFAULT_VBUS_UV;
	}

	usbin_vol = results.physical/1000;
	pr_smb(PR_STATUS, "usb in voltage = %dmV\n", usbin_vol);

	return usbin_vol;
}
#endif

#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
static void smbchg_monitor_batt_temp_queue(struct smbchg_chip *chip)
{
	cancel_delayed_work_sync(&chip->battemp_work);
	schedule_delayed_work(&chip->battemp_work, HZ*1);
	if (!chip->usb_present &&
			wake_lock_active(&chip->lcs_wake_lock))
		wake_unlock(&chip->lcs_wake_lock);
}
#endif

#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
#define UNLIMITED_CHG_SOC	6
#define POLLING_CNT_MAX		2
/* control input current limit by OLED panel state */
static void smbchg_set_lcd_limited_current_max(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);
	union power_supply_propval pval = {0,};
	int capacity, icl_ma, fcc_ma;

	if (!parallel_psy || chip->parallel.current_max_ma == 0)
		goto OUT;

#if defined(CONFIG_MACH_MSM8994_Z2_SPR_US) || defined(CONFIG_MACH_MSM8994_Z2_USC_US)
	if (lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO)
		goto OUT;
#endif

	/* under 1800mA TA case*/
	if (chip->parallel.initial_aicl_ma < DCP_ICL_DEFAULT) {
		pr_smb(PR_LGE, "icl %d < %d .. skipping\n",
			chip->parallel.initial_aicl_ma, DCP_ICL_DEFAULT);
		goto OUT;
	}

#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	/* ibat decreased case by taper handler */
	if (chip->is_parallel_chg_changed_by_taper) {
		pr_smb(PR_LGE, "is_parallel_chg_changed_by_taper .. skipping\n");
		goto OUT;
	}

	/* ibat limited case by thermal-engine*/
	if (chip->chg_current_te != EVP_FCC_DEFAULT) {
		pr_smb(PR_LGE, "ibat limited by thermal-engine .. skipping\n");
		goto OUT;
	}
#endif

	/* low battery case */
	capacity = get_prop_batt_capacity(chip);
	if (capacity < UNLIMITED_CHG_SOC) {
		pr_smb(PR_LGE, "soc %d < %d\n", capacity, UNLIMITED_CHG_SOC);
		chip->enable_polling = true;
		return;
	}

	/* single charging case */
	if (!smbchg_is_parallel_usb_ok(chip))
		goto OUT;

	pr_smb(PR_LGE,"%d: %d -> %d\n", chip->enable_polling,
		chip->chg_state, chip->fb_state);

	if (chip->fb_state) {
		if (chip->set_evp_vol && chip->is_enable_evp_chg) {
			icl_ma = EVP_ICL_LIMITED;
			fcc_ma = EVP_FCC_LIMITED;
		}
		else if (chip->init_QC20 && chip->is_enable_QC20_chg) {
			icl_ma = QC20_ICL_LIMITED;
			fcc_ma = QC20_FCC_LIMITED;
		}
		else {
			icl_ma = DCP_ICL_LIMITED;
			fcc_ma = DCP_FCC_LIMITED;
		}
	} else {
		if (chip->set_evp_vol && chip->is_enable_evp_chg) {
			icl_ma = EVP_ICL_DEFAULT;
			fcc_ma = EVP_FCC_DEFAULT;
		}
		else if (chip->init_QC20 && chip->is_enable_QC20_chg) {
			icl_ma = QC20_ICL_DEFAULT;
			fcc_ma = QC20_FCC_DEFAULT;
		}
		else {
			icl_ma = DCP_ICL_DEFAULT;
			fcc_ma = DCP_FCC_DEFAULT;
		}
	}
	chip->chg_state = chip->fb_state;
	chip->parallel.current_max_ma = icl_ma/2;
	chip->target_fastchg_current_ma = fcc_ma;
	smbchg_set_usb_current_max(chip, chip->parallel.current_max_ma);
	power_supply_set_current_limit(parallel_psy,
				chip->parallel.current_max_ma * 1000);
	smbchg_set_fastchg_current(chip, chip->target_fastchg_current_ma / 2);
	pval.intval = chip->target_fastchg_current_ma * 1000 / 2;
	parallel_psy->set_property(parallel_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);

OUT:
	chip->polling_cnt++;
	if (chip->polling_cnt >= POLLING_CNT_MAX) {
		chip->enable_polling = false;
		chip->polling_cnt = 0;
	}
	return;
}
static void smbchg_iusb_change_worker(struct work_struct *work)
{
	struct smbchg_chip *chip =
	                container_of(work, struct smbchg_chip, iusb_change_work.work);

	smbchg_set_lcd_limited_current_max(chip);
}

void smbchg_fb_notify_update_cb(bool is_on)
{
	NULL_CHECK_VOID(the_chip);

	/* update panel power state */
	the_chip->fb_state = is_on;

	if (!the_chip->usb_present)
		return;

	pr_smb(PR_LGE,"%d: %d -> %d\n", the_chip->enable_polling,
		the_chip->chg_state, the_chip->fb_state);
	if (!the_chip->enable_polling) {
		if (the_chip->chg_state != the_chip->fb_state) {
			cancel_delayed_work_sync(&the_chip->iusb_change_work);
			schedule_delayed_work(&the_chip->iusb_change_work,
				msecs_to_jiffies(500));
		}
	}
}
EXPORT_SYMBOL(smbchg_fb_notify_update_cb);
#endif

#ifdef CONFIG_LGE_PM_EVP_TESTCMD_SUPPORT
static int store_is_enable_evp_chg = 1;
#endif

#ifdef CONFIG_LGE_PM_DETECT_QC20
#define MONITOR_USBIN_QC20_CHG		4*HZ //this time delay should be reducecd. because this delay makes EVP detection time longer.
#define QC20_RETRY_COUNT		5
#define QC20_USBIN_VOL_THRSHD		7500
static void disable_QC20_chg(struct smbchg_chip *chip)
{
	cancel_delayed_work(&chip->enable_QC20_chg_work);
	cancel_delayed_work(&chip->detect_QC20_work);
	chip->is_enable_QC20_chg = 0;
	chip->retry_cnt_QC20_chg = 0;
	chip->init_QC20 = 0;
}

static void smbchg_usb_enable_QC20_chg_work(struct work_struct *work)
{
	// mostly copy from func. check_usbin_evp_chg()
	int usbin_vol = 0;
	union power_supply_propval pval;
	struct power_supply *usb_psy;
	struct power_supply *parallel_psy;
	struct smbchg_chip *chip =
			container_of(work, struct smbchg_chip, enable_QC20_chg_work.work);

	parallel_psy = get_parallel_psy(chip);
	if (!parallel_psy)
		return;

	if (!chip->usb_psy)
		usb_psy = power_supply_get_by_name("usb");
	else
		usb_psy = chip->usb_psy;

	if (!smbchg_is_parallel_usb_ok(chip)) {
		pr_smb(PR_LGE, "QC2.0 ta plugged, but charger setting not changed\n");
		chip->target_fastchg_current_ma = QC20_FCC_DEFAULT;
		chip->is_enable_QC20_chg = 1;
		chip->retry_cnt_QC20_chg = 0;
		chip->init_QC20 = 1;
		return;
	}

	if (chip->retry_cnt_QC20_chg >= QC20_RETRY_COUNT) {
		pr_smb(PR_LGE, "failed to set QC2.0 high voltage, input voltage is 5V\n");
		/* this case is plugged evp ta but usb input voltage is under high vol.
		   so will be change only charging current setting to 5V settings*/
		chip->target_fastchg_current_ma = DCP_FCC_DEFAULT;
		smbchg_set_fastchg_current(chip, chip->target_fastchg_current_ma / 2);
		pval.intval = chip->target_fastchg_current_ma * 1000 / 2;
		parallel_psy->set_property(parallel_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
		/* QC20 ta plugged */
		chip->is_enable_QC20_chg = 0;
		chip->retry_cnt_QC20_chg = 0;
		return;
	}

	if (!chip->init_QC20) {
		pr_smb(PR_LGE, "init QC2.0 ICL, FCC. for QC2.0 9V\n");

		/* set evp input current, charging current */
		chip->parallel.current_max_ma = QC20_ICL_DEFAULT/2;
		smbchg_set_usb_current_max(chip, chip->parallel.current_max_ma);
		power_supply_set_current_limit(parallel_psy,
					chip->parallel.current_max_ma * 1000);
		chip->target_fastchg_current_ma = QC20_FCC_DEFAULT;
		smbchg_set_fastchg_current(chip, chip->target_fastchg_current_ma / 2);
		pval.intval = chip->target_fastchg_current_ma * 1000 / 2;
		parallel_psy->set_property(parallel_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
		chip->init_QC20 = 1;
		/* usb input voltage monitor */
		chip->retry_cnt_QC20_chg++;
		schedule_delayed_work(&chip->enable_QC20_chg_work,
			MONITOR_USBIN_QC20_CHG);
	} else {
		usbin_vol = smbchg_get_usb_adc(chip);
		if (usbin_vol > QC20_USBIN_VOL_THRSHD) {
			pr_smb(PR_LGE, "usbin_vol %d is over %d\n",
				usbin_vol, QC20_USBIN_VOL_THRSHD);
			chip->retry_cnt_QC20_chg = 0;
			chip->is_enable_QC20_chg = 1;
		}
		else  {
			pr_smb(PR_LGE, "usbin_vol %d is under %d\n",
				usbin_vol, QC20_USBIN_VOL_THRSHD);
			chip->retry_cnt_QC20_chg++;
			schedule_delayed_work(&chip->enable_QC20_chg_work,
				MONITOR_USBIN_QC20_CHG);
		}
	}
}

static void smbchg_usb_detect_QC20_work(struct work_struct *work)
{
	int rc = 0, type = 0;
	u8 reg = 0;
	union power_supply_propval pval;
	enum power_supply_type usb_type;
	struct power_supply *usb_psy;
	struct smbchg_chip *chip =
			container_of(work, struct smbchg_chip, detect_QC20_work.work);

#if defined(CONFIG_SLIMPORT_ANX7812) || defined(CONFIG_SLIMPORT_ANX7816)
	if (slimport_is_connected())
	{
		pr_smb(PR_LGE, "slimport_is_connected. "
				"Dont excute QC2.0 or EVP detection.\n");
		return;
	}
#endif

	/* check usb type is DCP */
	rc = smbchg_read(chip, &reg, chip->misc_base + IDEV_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
		return;
	}

	type = get_type(reg);
	usb_type = get_usb_supply_type(type);

	if ( usb_type != POWER_SUPPLY_TYPE_USB_DCP )
	{
		pr_smb(PR_LGE, "usb type is not DCP. "
				"Dont excute QC2.0 or EVP detection. usb_type = %d\n", usb_type);
		return;
	}

	if (!chip->usb_psy)
		usb_psy = power_supply_get_by_name("usb");
	else
		usb_psy = chip->usb_psy;

	/* check inserted DCP is QC2.0 */
	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + USBIN_HVDCP_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb status rc = %d\n", rc);
		return;
	}

	pr_smb(PR_LGE, "get HVDCP reg=0x%x\n", reg);

	if ( ((reg & USBIN_HVDCP_SEL_BIT) == USBIN_HVDCP_SEL_BIT))
	{
		pval.intval = 0;
		schedule_delayed_work(&chip->enable_QC20_chg_work,
			msecs_to_jiffies(500));
		pr_smb(PR_LGE, "TA is QC2.0.\n");
	}
	else
	{
		pval.intval = 1;
		pr_smb(PR_LGE, "TA is not QC2.0. Do EVP detection\n");
	}

	usb_psy->set_property(usb_psy, POWER_SUPPLY_PROP_EVP_DETECT_START, &pval);
}
#endif

#ifdef CONFIG_LGE_PM_MAXIM_EVP_CONTROL
#define EVP_USBIN_VOLTAGE_MAX		9000
#define EVP_USBIN_VOLTAGE_MIN		5000
#define EVP_USBIN_VOLTAGE_NEW		8300
#define EVP_USBIN_VOL_THRSHD		7500
#define EVP_RETRY_COUNT			10
#define MONITOR_USBIN_EVP_CHG		(5*HZ)
static void disable_evp_chg(struct smbchg_chip *chip)
{
	cancel_delayed_work(&chip->enable_evp_chg_work);
	chip->set_evp_vol = 0;
	chip->is_enable_evp_chg = 0;
	chip->retry_cnt_evp_chg = 0;
}

static void check_usbin_evp_chg(struct work_struct *work)
{
	int usbin_vol = 0;
	union power_supply_propval pval;
	struct power_supply *usb_psy;
	struct power_supply *parallel_psy;
	struct smbchg_chip *chip =
                        container_of(work, struct smbchg_chip, enable_evp_chg_work.work);

	parallel_psy = get_parallel_psy(chip);
	if (!parallel_psy)
		return;

	if (!chip->usb_psy)
		usb_psy = power_supply_get_by_name("usb");
	else
		usb_psy = chip->usb_psy;

	if (!smbchg_is_parallel_usb_ok(chip)) {
		pr_smb(PR_LGE, "evp ta plugged, but charger setting not changed\n");
		pval.intval = EVP_USBIN_VOLTAGE_NEW;
		usb_psy->set_property(usb_psy, POWER_SUPPLY_PROP_EVP_VOL, &pval);
		chip->target_fastchg_current_ma = EVP_FCC_DEFAULT;
		chip->set_evp_vol = 1;
		chip->is_enable_evp_chg = 1;
		chip->retry_cnt_evp_chg = 0;
		return;
	}

	if (chip->retry_cnt_evp_chg >= EVP_RETRY_COUNT) {
		pr_smb(PR_LGE, "failed to set high voltage, input voltage is 5V\n");
		/* this case is plugged evp ta but usb input voltage is under high vol.
		   so will be change only charging current setting to 5V settings*/
		chip->target_fastchg_current_ma = DCP_FCC_DEFAULT;
		smbchg_set_fastchg_current(chip, chip->target_fastchg_current_ma / 2);
		pval.intval = chip->target_fastchg_current_ma * 1000 / 2;
		parallel_psy->set_property(parallel_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
		/* evp ta plugged */
		chip->set_evp_vol = 1;
		chip->is_enable_evp_chg = 0;
		chip->retry_cnt_evp_chg = 0;
		return;
	}

	if (!chip->set_evp_vol) {
		/* set evp voltage */
		pr_smb(PR_LGE, "setting evp voltage to %dmV\n",EVP_USBIN_VOLTAGE_NEW);
		pval.intval = EVP_USBIN_VOLTAGE_NEW;
		usb_psy->set_property(usb_psy, POWER_SUPPLY_PROP_EVP_VOL, &pval);

		/* set evp input current, charging current */
		chip->parallel.current_max_ma = EVP_ICL_DEFAULT/2;
		smbchg_set_usb_current_max(chip, chip->parallel.current_max_ma);
		power_supply_set_current_limit(parallel_psy,
					chip->parallel.current_max_ma * 1000);
		chip->target_fastchg_current_ma = EVP_FCC_DEFAULT;
		smbchg_set_fastchg_current(chip, chip->target_fastchg_current_ma / 2);
		pval.intval = chip->target_fastchg_current_ma * 1000 / 2;
		parallel_psy->set_property(parallel_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
		chip->set_evp_vol = 1;
		/* usb input voltage monitor */
		chip->retry_cnt_evp_chg++;
		schedule_delayed_work(&chip->enable_evp_chg_work,
			MONITOR_USBIN_EVP_CHG);
	} else {
		usbin_vol = smbchg_get_usb_adc(chip);
		if (usbin_vol > EVP_USBIN_VOL_THRSHD) {
			pr_smb(PR_LGE, "usbin_vol %d is over %d\n",
				usbin_vol, EVP_USBIN_VOL_THRSHD);
			chip->retry_cnt_evp_chg = 0;
			chip->is_enable_evp_chg = 1;
		}
		else  {
			pr_smb(PR_LGE, "usbin_vol %d is under %d\n",
				usbin_vol, EVP_USBIN_VOL_THRSHD);
			chip->retry_cnt_evp_chg++;
			schedule_delayed_work(&chip->enable_evp_chg_work,
				MONITOR_USBIN_EVP_CHG);
		}
	}
}

static int enable_evp_chg(struct smbchg_chip *chip, int enable)
{
	int rc = 0;

	pr_smb(PR_LGE, "enable = %d.\n", enable);

	if (enable == 1) {
#ifdef CONFIG_LGE_PM_EVP_TESTCMD_SUPPORT
		store_is_enable_evp_chg = 0;
#endif
		cancel_delayed_work(&chip->enable_evp_chg_work);
		schedule_delayed_work(&chip->enable_evp_chg_work,
				msecs_to_jiffies(500));
	}
	else if (enable == 0) {
		disable_evp_chg(chip);
	}
	else {
		dev_err(chip->dev, "failed to set evp chg. Enable value is incorrect.\n");
	}

	return rc;
}
#endif

#ifdef CONFIG_LGE_PM_COMMON
#define USBIN_SUSPEND_SRC_BIT_LGE		BIT(6)
static void smbchg_batfet_en(struct smbchg_chip *chip, bool en)
{
	int rc;

	if(en == 0)
		chip->batfet_en = 0; /* batfet disable*/
	else if ( en == 1 )
		chip->batfet_en = 1; /* batfet enable */

	rc = smbchg_sec_masked_write(chip,
			chip->usb_chgpth_base + CHGPTH_CFG,
			USBIN_SUSPEND_SRC_BIT_LGE, en ? 0 : USBIN_SUSPEND_SRC_BIT_LGE);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set usb_chgpth cfg rc=%d\n", rc);
		return;
	}
}
#endif
#define VFLOAT_CFG_REG			0xF4
#define MIN_FLOAT_MV			3600
#define MAX_FLOAT_MV			4500
#define VFLOAT_MASK			SMB_MASK(5, 0)

#define MID_RANGE_FLOAT_MV_MIN		3600
#define MID_RANGE_FLOAT_MIN_VAL		0x05
#define MID_RANGE_FLOAT_STEP_MV		20

#define HIGH_RANGE_FLOAT_MIN_MV		4340
#define HIGH_RANGE_FLOAT_MIN_VAL	0x2A
#define HIGH_RANGE_FLOAT_STEP_MV	10

#define VHIGH_RANGE_FLOAT_MIN_MV	4360
#define VHIGH_RANGE_FLOAT_MIN_VAL	0x2C
#define VHIGH_RANGE_FLOAT_STEP_MV	20
static int smbchg_float_voltage_set(struct smbchg_chip *chip, int vfloat_mv)
{
	int rc, delta;
	u8 temp;

	if ((vfloat_mv < MIN_FLOAT_MV) || (vfloat_mv > MAX_FLOAT_MV)) {
		dev_err(chip->dev, "bad float voltage mv =%d asked to set\n",
					vfloat_mv);
		return -EINVAL;
	}

	if (vfloat_mv <= HIGH_RANGE_FLOAT_MIN_MV) {
		/* mid range */
		delta = vfloat_mv - MID_RANGE_FLOAT_MV_MIN;
		temp = MID_RANGE_FLOAT_MIN_VAL + delta
				/ MID_RANGE_FLOAT_STEP_MV;
		vfloat_mv -= delta % MID_RANGE_FLOAT_STEP_MV;
	} else if (vfloat_mv <= VHIGH_RANGE_FLOAT_MIN_MV) {
		/* high range */
		delta = vfloat_mv - HIGH_RANGE_FLOAT_MIN_MV;
		temp = HIGH_RANGE_FLOAT_MIN_VAL + delta
				/ HIGH_RANGE_FLOAT_STEP_MV;
		vfloat_mv -= delta % HIGH_RANGE_FLOAT_STEP_MV;
	} else {
		/* very high range */
		delta = vfloat_mv - VHIGH_RANGE_FLOAT_MIN_MV;
		temp = VHIGH_RANGE_FLOAT_MIN_VAL + delta
				/ VHIGH_RANGE_FLOAT_STEP_MV;
		vfloat_mv -= delta % VHIGH_RANGE_FLOAT_STEP_MV;
	}

	rc = smbchg_sec_masked_write(chip, chip->chgr_base + VFLOAT_CFG_REG,
			VFLOAT_MASK, temp);

	if (rc)
		dev_err(chip->dev, "Couldn't set float voltage rc = %d\n", rc);
	else
		chip->vfloat_mv = vfloat_mv;

	return rc;
}

static int smbchg_float_voltage_get(struct smbchg_chip *chip)
{
	return chip->vfloat_mv;
}

static int smbchg_battery_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	int rc = 0;
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		smbchg_usb_en(chip, val->intval, REASON_USER);
#ifdef CONFIG_LGE_PM_COMMON
		if (chip->dc_psy_type != -EINVAL)
			smbchg_dc_en(chip, val->intval, REASON_USER);
#else
		smbchg_dc_en(chip, val->intval, REASON_USER);
#endif
		chip->chg_enabled = val->intval;
		schedule_work(&chip->usb_set_online_work);
		break;
#ifdef CONFIG_LGE_PM_COMMON
	case POWER_SUPPLY_PROP_BATFET_EN:
		smbchg_batfet_en(chip, val->intval);
		break;
#endif
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->fake_battery_soc = val->intval;
		power_supply_changed(&chip->batt_psy);
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		smbchg_system_temp_level_set(chip, val->intval);
		break;
#ifdef CONFIG_LGE_PM_MAXIM_EVP_CONTROL
	case POWER_SUPPLY_PROP_ENABLE_EVP_CHG:
		enable_evp_chg(chip, val->intval);
		break;
#endif
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	case POWER_SUPPLY_PROP_SAFTETY_CHARGER_TIMER:
		smbchg_enable_safety_timer(chip, val->intval);
		safety_timer = val->intval;
		break;
#endif
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smbchg_set_fastchg_current(chip, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smbchg_float_voltage_set(chip, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return rc;
}

static int smbchg_battery_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
#ifdef CONFIG_LGE_PM_COMMON
	case POWER_SUPPLY_PROP_BATFET_EN:
#endif
#ifdef CONFIG_LGE_PM_MAXIM_EVP_CONTROL
	case POWER_SUPPLY_PROP_ENABLE_EVP_CHG:
#endif
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	case POWER_SUPPLY_PROP_SAFTETY_CHARGER_TIMER:
#endif
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int smbchg_battery_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = get_prop_batt_status(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = chip->chg_enabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = smbchg_float_voltage_get(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = get_prop_batt_health(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_FLASH_CURRENT_MAX:
		val->intval = smbchg_calc_max_flash_current(chip);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = chip->fastchg_current_ma * 1000;
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		val->intval = chip->therm_lvl_sel;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
		if (pseudo_batt_info.mode) {
			val->intval = pseudo_batt_info.capacity;
			break;
		}
#endif
		val->intval = get_prop_batt_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_prop_batt_current_now(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
		if (pseudo_batt_info.mode) {
			val->intval = pseudo_batt_info.volt * 1000;
			break;
		}
#endif
		val->intval = get_prop_batt_voltage_now(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
		if (pseudo_batt_info.mode) {
			val->intval = pseudo_batt_info.temp;
			break;
		}
#endif
		val->intval = get_prop_batt_temp(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = get_prop_batt_voltage_max_design(chip);
		break;
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	case POWER_SUPPLY_PROP_SAFTETY_CHARGER_TIMER:
		val->intval = safety_timer;
		break;
#endif
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	case POWER_SUPPLY_PROP_PSEUDO_BATT:
		val->intval = pseudo_batt_info.mode;
		break;
#endif
#ifdef CONFIG_LGE_PM_MAXIM_EVP_CONTROL
	case POWER_SUPPLY_PROP_ENABLE_EVP_CHG:
		val->intval = chip->set_evp_vol;
		break;
#endif
#ifdef CONFIG_LGE_PM_COMMON
	case POWER_SUPPLY_PROP_BATTERY_ID_CHECKER:
#ifdef CONFIG_MACH_MSM8994_Z2
		val->intval = 1;  //Z2 has embedded battery. always return 1.
#elif defined(CONFIG_LGE_PM_BATTERY_ID_CHECKER)
		if (is_factory_cable() && is_usb_present(chip))
			val->intval = 1;
		else
			//val->intval = chip->batt_id_smem;
			val->intval = 1;	//To do. fix battery id for Lion prj.
		break;
#else
		val->intval = 1;
#endif
		break;
	case POWER_SUPPLY_PROP_EXT_PWR_CHECK:
		val->intval = lge_pm_get_cable_type();
		break;
	case POWER_SUPPLY_PROP_BATFET_EN:
		val->intval = chip->batfet_en;
		break;
#endif
#ifdef CONFIG_LGE_PM_EVP_TESTCMD_SUPPORT
	case POWER_SUPPLY_PROP_EVP_TESTCMD_SUPPORT:
		val->intval = store_is_enable_evp_chg;
		break;
#endif
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property smbchg_dc_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
};

static int smbchg_dc_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, dc_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		return smbchg_dc_en(chip, val->intval, REASON_POWER_SUPPLY);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smbchg_dc_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, dc_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = get_prop_batt_status(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = is_dc_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = chip->dc_suspended == 0;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* return if dc is charging the battery */
		val->intval = (smbchg_get_pwr_path(chip) == PWR_PATH_DC)
				&& (get_prop_batt_status(chip)
					== POWER_SUPPLY_STATUS_CHARGING);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smbchg_dc_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

#define USBIN_SUSPEND_SRC_BIT		BIT(6)
static void smbchg_unknown_battery_en(struct smbchg_chip *chip, bool en)
{
	int rc;

	if (en == chip->battery_unknown || chip->charge_unknown_battery)
		return;

	chip->battery_unknown = en;
	rc = smbchg_sec_masked_write(chip,
		chip->usb_chgpth_base + CHGPTH_CFG,
		USBIN_SUSPEND_SRC_BIT, en ? 0 : USBIN_SUSPEND_SRC_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set usb_chgpth cfg rc=%d\n", rc);
		return;
	}
}

#define CMD_CHG_REG	0x42
#define EN_BAT_CHG_BIT		BIT(1)
static int smbchg_charging_en(struct smbchg_chip *chip, bool en)
{
#ifdef CONFIG_LGE_PM
#ifdef CONFIG_MACH_MSM8994_Z2
	if (is_factory_cable_910k() && !batt_smem_present) {
		pr_smb(PR_LGE, "smem battery is absent, charging disable\n");
		en = false;
	}
#endif
	pr_smb(PR_LGE, "en = %d\n", en);
#endif
	return smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
			EN_BAT_CHG_BIT, en ? 0 : EN_BAT_CHG_BIT);
}

static void smbchg_vfloat_adjust_check(struct smbchg_chip *chip)
{
	if (!chip->use_vfloat_adjustments)
		return;

	smbchg_stay_awake(chip, PM_REASON_VFLOAT_ADJUST);
	pr_smb(PR_STATUS, "Starting vfloat adjustments\n");
	schedule_delayed_work(&chip->vfloat_adjust_work, 0);
}

#ifdef CONFIG_LGE_PM
#define VBUS_SDP_UV	4700
#endif
#define UNKNOWN_BATT_TYPE	"Unknown Battery"
#define LOADING_BATT_TYPE	"Loading Battery Data"
static void smbchg_external_power_changed(struct power_supply *psy)
{
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, batt_psy);
	union power_supply_propval prop = {0,};
	int rc, current_limit = 0;
	bool en;

	if (chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	if (chip->bms_psy) {
		chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_BATTERY_TYPE, &prop);
		en = strcmp(prop.strval, UNKNOWN_BATT_TYPE) != 0;
		smbchg_unknown_battery_en(chip, en);
#ifdef CONFIG_LGE_PM
#else
		en = strcmp(prop.strval, LOADING_BATT_TYPE) != 0;
		smbchg_charging_en(chip, en);
#endif
	}

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CHARGING_ENABLED, &prop);
	if (rc < 0)
		pr_smb(PR_MISC, "could not read USB charge_en, rc=%d\n",
				rc);
	else
		smbchg_usb_en(chip, prop.intval, REASON_POWER_SUPPLY);

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc < 0)
		dev_err(chip->dev,
			"could not read USB current_max property, rc=%d\n", rc);
	else
		current_limit = prop.intval / 1000;

#ifdef CONFIG_LGE_PM
	rc = chip->usb_psy->get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_TYPE, &prop);
	if (rc < 0)
		dev_err(chip->dev,
			"could not read USB Charge Type property, rc=%d\n", rc);
	if (prop.intval == POWER_SUPPLY_TYPE_USB) {
		/* when usb cable is inserted to usb hub wo/external pwr,
		 * input src disconnect and connect repeatedly
		 * apply w/a code in order to allow usb to be recognized as a valid input. */
		if (current_limit == CURRENT_500_MA &&
			smbchg_get_usb_adc(chip) < VBUS_SDP_UV) {
			current_limit = CURRENT_100_MA;
		}
#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
		if (pseudo_batt_info.mode == 1 || safety_timer == 0) {
			current_limit = PSEUDO_BATT_USB_ICL;
			pr_smb(PR_LGE, "pseudo_batt, current_limit = %d", current_limit);
		}
#endif
	}
	if (is_factory_cable()) {
		if (is_factory_cable_130k())
			current_limit = FACTORY_ICL_130K;
		else {
#ifdef CONFIG_MACH_MSM8994_Z2
			/* MITS/MID 56K port disconnect issue
			 * in case of embedded battery model
			 * - battery present: ICL 1000mA, Not present: ICL 1500mA
			 */
			if (batt_smem_present)
				current_limit = FACTORY_ICL_1000;
			else
				current_limit = FACTORY_ICL_DEFAULT;
#else
			current_limit = FACTORY_ICL_DEFAULT;
#endif
		}
		chip->target_fastchg_current_ma = FACTORY_FCC_DEFAULT;
	}
#endif
	pr_smb(PR_MISC, "current_limit = %d\n", current_limit);
	mutex_lock(&chip->current_change_lock);
	if (current_limit != chip->usb_target_current_ma) {
#ifdef CONFIG_LGE_PM
		pr_smb(PR_LGE, "changed current_limit = %d, usb_target_current_ma = %d\n",
			current_limit, chip->usb_target_current_ma);
#else
		pr_smb(PR_STATUS, "changed current_limit = %d\n",
						current_limit);
#endif
		chip->usb_target_current_ma = current_limit;
		rc = smbchg_set_thermal_limited_usb_current_max(chip,
				current_limit);
		if (rc < 0)
			dev_err(chip->dev,
				"Couldn't set usb current rc = %d\n", rc);
	}
	mutex_unlock(&chip->current_change_lock);

	smbchg_vfloat_adjust_check(chip);

	power_supply_changed(&chip->batt_psy);
}

#define OTG_EN		BIT(0)
static int smbchg_otg_regulator_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	chip->otg_retries = 0;
	rc = smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
			OTG_EN, OTG_EN);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't enable OTG mode rc=%d\n", rc);
	else
		chip->otg_enable_time = ktime_get();

#ifdef CONFIG_LGE_PM
        pr_smb(PR_LGE, "Enabling OTG Boost\n");
#else

	pr_smb(PR_STATUS, "Enabling OTG Boost\n");
#endif
	return rc;
}

static int smbchg_otg_regulator_disable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	rc = smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
			OTG_EN, 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't disable OTG mode rc=%d\n", rc);
#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "Disabling OTG Boost\n");
#else
	pr_smb(PR_STATUS, "Disabling OTG Boost\n");
#endif
	return rc;
}

static int smbchg_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	u8 reg = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	rc = smbchg_read(chip, &reg, chip->bat_if_base + CMD_CHG_REG, 1);
	if (rc < 0) {
		dev_err(chip->dev,
				"Couldn't read OTG enable bit rc=%d\n", rc);
		return rc;
	}

	return (reg & OTG_EN) ? 1 : 0;
}

struct regulator_ops smbchg_otg_reg_ops = {
	.enable		= smbchg_otg_regulator_enable,
	.disable	= smbchg_otg_regulator_disable,
	.is_enabled	= smbchg_otg_regulator_is_enable,
};

#ifdef CONFIG_LGE_PM
void usbin_suspend_for_flash_led(bool suspend)
{
	bool changed;
	int rc = 0;
	NULL_CHECK_VOID(the_chip);

	if (is_factory_cable())
		return;

	if (!suspend)
		msleep(100);

	pr_smb(PR_LGE, "usbin_suspend_for_flash_led suspend = %d\n", suspend);

	rc = smbchg_primary_usb_en(the_chip, suspend ? false:true, REASON_USER, &changed);
	if (rc < 0) {
		pr_smb(PR_LGE, "failed to set usb suspend for flash led\n");
	}
}
EXPORT_SYMBOL(usbin_suspend_for_flash_led);
#endif

#define USBIN_CHGR_CFG			0xF1
#define USBIN_ADAPTER_9V		0x3
#define HVDCP_EN_BIT			BIT(3)
static int smbchg_external_otg_regulator_enable(struct regulator_dev *rdev)
{
	bool changed;
	int rc = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	rc = smbchg_primary_usb_en(chip, false, REASON_OTG, &changed);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't suspend charger rc=%d\n", rc);
		return rc;
	}

	rc = smbchg_read(chip, &chip->original_usbin_allowance,
			chip->usb_chgpth_base + USBIN_CHGR_CFG, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb allowance rc=%d\n", rc);
		return rc;
	}

	/*
	 * To disallow source detect and usbin_uv interrupts, set the adapter
	 * allowance to 9V, so that the audio boost operating in reverse never
	 * gets detected as a valid input
	 */
	rc = smbchg_sec_masked_write(chip,
				chip->usb_chgpth_base + CHGPTH_CFG,
				HVDCP_EN_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't disable HVDCP rc=%d\n", rc);
		return rc;
	}

	rc = smbchg_sec_masked_write(chip,
				chip->usb_chgpth_base + USBIN_CHGR_CFG,
				0xFF, USBIN_ADAPTER_9V);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't write usb allowance rc=%d\n", rc);
		return rc;
	}

#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "Enabling OTG Boost\n");
#else
	pr_smb(PR_STATUS, "Enabling OTG Boost\n");
#endif
	return rc;
}

static int smbchg_external_otg_regulator_disable(struct regulator_dev *rdev)
{
	bool changed;
	int rc = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	rc = smbchg_primary_usb_en(chip, true, REASON_OTG, &changed);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't unsuspend charger rc=%d\n", rc);
		return rc;
	}

	/*
	 * Reenable HVDCP and set the adapter allowance back to the original
	 * value in order to allow normal USBs to be recognized as a valid
	 * input.
	 */
	rc = smbchg_sec_masked_write(chip,
				chip->usb_chgpth_base + CHGPTH_CFG,
				HVDCP_EN_BIT, HVDCP_EN_BIT);

	if (rc < 0) {
		dev_err(chip->dev, "Couldn't enable HVDCP rc=%d\n", rc);
		return rc;
	}

	rc = smbchg_sec_masked_write(chip,
				chip->usb_chgpth_base + USBIN_CHGR_CFG,
				0xFF, chip->original_usbin_allowance);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't write usb allowance rc=%d\n", rc);
		return rc;
	}

#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "Disabling OTG Boost\n");
#else
	pr_smb(PR_STATUS, "Disabling OTG Boost\n");
#endif
	return rc;
}

static int smbchg_external_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	return !smbchg_primary_usb_is_en(chip, REASON_OTG);
}

struct regulator_ops smbchg_external_otg_reg_ops = {
	.enable		= smbchg_external_otg_regulator_enable,
	.disable	= smbchg_external_otg_regulator_disable,
	.is_enabled	= smbchg_external_otg_regulator_is_enable,
};

static int smbchg_regulator_init(struct smbchg_chip *chip)
{
	int rc = 0;
	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};
	struct device_node *regulator_node;

	regulator_node = of_get_child_by_name(chip->dev->of_node,
			"qcom,smbcharger-boost-otg");

	init_data = of_get_regulator_init_data(chip->dev, regulator_node);
	if (!init_data) {
		dev_err(chip->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	if (init_data->constraints.name) {
		chip->otg_vreg.rdesc.owner = THIS_MODULE;
		chip->otg_vreg.rdesc.type = REGULATOR_VOLTAGE;
		chip->otg_vreg.rdesc.ops = &smbchg_otg_reg_ops;
		chip->otg_vreg.rdesc.name = init_data->constraints.name;

		cfg.dev = chip->dev;
		cfg.init_data = init_data;
		cfg.driver_data = chip;
		cfg.of_node = regulator_node;

		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_STATUS;

		chip->otg_vreg.rdev = regulator_register(
						&chip->otg_vreg.rdesc, &cfg);
		if (IS_ERR(chip->otg_vreg.rdev)) {
			rc = PTR_ERR(chip->otg_vreg.rdev);
			chip->otg_vreg.rdev = NULL;
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev,
					"OTG reg failed, rc=%d\n", rc);
		}
	}

	if (rc)
		return rc;

	regulator_node = of_get_child_by_name(chip->dev->of_node,
			"qcom,smbcharger-external-otg");
	init_data = of_get_regulator_init_data(chip->dev, regulator_node);
	if (!init_data) {
		dev_err(chip->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	if (init_data->constraints.name) {
		if (of_get_property(chip->dev->of_node,
					"otg-parent-supply", NULL))
			init_data->supply_regulator = "otg-parent";
		chip->ext_otg_vreg.rdesc.owner = THIS_MODULE;
		chip->ext_otg_vreg.rdesc.type = REGULATOR_VOLTAGE;
		chip->ext_otg_vreg.rdesc.ops = &smbchg_external_otg_reg_ops;
		chip->ext_otg_vreg.rdesc.name = init_data->constraints.name;

		cfg.dev = chip->dev;
		cfg.init_data = init_data;
		cfg.driver_data = chip;
		cfg.of_node = regulator_node;

		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_STATUS;

		chip->ext_otg_vreg.rdev = regulator_register(
					&chip->ext_otg_vreg.rdesc, &cfg);
		if (IS_ERR(chip->ext_otg_vreg.rdev)) {
			rc = PTR_ERR(chip->ext_otg_vreg.rdev);
			chip->ext_otg_vreg.rdev = NULL;
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev,
					"external OTG reg failed, rc=%d\n", rc);
		}
	}

	return rc;
}

static void smbchg_regulator_deinit(struct smbchg_chip *chip)
{
	if (chip->otg_vreg.rdev)
		regulator_unregister(chip->otg_vreg.rdev);
	if (chip->ext_otg_vreg.rdev)
		regulator_unregister(chip->ext_otg_vreg.rdev);
}

#define REVISION1_REG			0x0
#define DIG_MINOR			0
#define DIG_MAJOR			1
#define ANA_MINOR			2
#define ANA_MAJOR			3
static int smbchg_low_icl_wa_check(struct smbchg_chip *chip)
{
	int rc = 0;
#ifdef CONFIG_LGE_PM_COMMON
	bool enable = 0;
	u8 reg = 0, chg_type = 0;
#else
	bool enable = (get_prop_batt_status(chip)
		!= POWER_SUPPLY_STATUS_CHARGING);
#endif
	/* only execute workaround if the charger is version 1.x */
	if (chip->revision[DIG_MAJOR] > 1)
		return 0;

#ifdef CONFIG_LGE_PM_COMMON
	rc = smbchg_read(chip, &reg, chip->chgr_base + CHGR_STS, 1);
        if (rc < 0) {
                dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
		return 0;
        }

        chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;

        if (chg_type == BATT_NOT_CHG_VAL)
		enable = 1;
#endif

	mutex_lock(&chip->current_change_lock);
#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "low icl %s -> %s\n",
			chip->low_icl_wa_on ? "on" : "off",
			enable ? "on" : "off");
#else
	pr_smb(PR_STATUS, "low icl %s -> %s\n",
			chip->low_icl_wa_on ? "on" : "off",
			enable ? "on" : "off");
#endif
	if (enable == chip->low_icl_wa_on)
		goto out;

	chip->low_icl_wa_on = enable;
	if (enable) {
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_2);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_100MA);
		if (rc)
			dev_err(chip->dev,
				"could not set low current limit: %d\n", rc);
	} else {

		rc = smbchg_set_thermal_limited_usb_current_max(chip,
						chip->usb_target_current_ma);
		if (rc)
			dev_err(chip->dev,
				"could not set current limit: %d\n", rc);
	}
out:
	mutex_unlock(&chip->current_change_lock);
	return rc;
}

static int vf_adjust_low_threshold = 5;
module_param(vf_adjust_low_threshold, int, 0644);

static int vf_adjust_high_threshold = 7;
module_param(vf_adjust_high_threshold, int, 0644);

static int vf_adjust_n_samples = 10;
module_param(vf_adjust_n_samples, int, 0644);

static int vf_adjust_max_delta_mv = 40;
module_param(vf_adjust_max_delta_mv, int, 0644);

static int vf_adjust_trim_steps_per_adjust = 1;
module_param(vf_adjust_trim_steps_per_adjust, int, 0644);

#define CENTER_TRIM_CODE		7
#define MAX_LIN_CODE			14
#define MAX_TRIM_CODE			15
#define SCALE_SHIFT			4
#define VF_TRIM_OFFSET_MASK		SMB_MASK(3, 0)
#define VF_STEP_SIZE_MV			10
#define SCALE_LSB_MV			17
static int smbchg_trim_add_steps(int prev_trim, int delta_steps)
{
	int scale_steps;
	int linear_offset, linear_scale;
	int offset_code = prev_trim & VF_TRIM_OFFSET_MASK;
	int scale_code = (prev_trim & ~VF_TRIM_OFFSET_MASK) >> SCALE_SHIFT;

	if (abs(delta_steps) > 1) {
		pr_smb(PR_STATUS,
			"Cant trim multiple steps delta_steps = %d\n",
			delta_steps);
		return prev_trim;
	}
	if (offset_code <= CENTER_TRIM_CODE)
		linear_offset = offset_code + CENTER_TRIM_CODE;
	else if (offset_code > CENTER_TRIM_CODE)
		linear_offset = MAX_TRIM_CODE - offset_code;

	if (scale_code <= CENTER_TRIM_CODE)
		linear_scale = scale_code + CENTER_TRIM_CODE;
	else if (scale_code > CENTER_TRIM_CODE)
		linear_scale = scale_code - (CENTER_TRIM_CODE + 1);

	/* check if we can accomodate delta steps with just the offset */
	if (linear_offset + delta_steps >= 0
			&& linear_offset + delta_steps <= MAX_LIN_CODE) {
		linear_offset += delta_steps;

		if (linear_offset > CENTER_TRIM_CODE)
			offset_code = linear_offset - CENTER_TRIM_CODE;
		else
			offset_code = MAX_TRIM_CODE - linear_offset;

		return (prev_trim & ~VF_TRIM_OFFSET_MASK) | offset_code;
	}

	/* changing offset cannot satisfy delta steps, change the scale bits */
	scale_steps = delta_steps > 0 ? 1 : -1;

	if (linear_scale + scale_steps < 0
			|| linear_scale + scale_steps > MAX_LIN_CODE) {
		pr_smb(PR_STATUS,
			"Cant trim scale_steps = %d delta_steps = %d\n",
			scale_steps, delta_steps);
		return prev_trim;
	}

	linear_scale += scale_steps;

	if (linear_scale > CENTER_TRIM_CODE)
		scale_code = linear_scale - CENTER_TRIM_CODE;
	else
		scale_code = linear_scale + (CENTER_TRIM_CODE + 1);
	prev_trim = (prev_trim & VF_TRIM_OFFSET_MASK)
		| scale_code << SCALE_SHIFT;

	/*
	 * now that we have changed scale which is a 17mV jump, change the
	 * offset bits (10mV) too so the effective change is just 7mV
	 */
	delta_steps = -1 * delta_steps;

	linear_offset = clamp(linear_offset + delta_steps, 0, MAX_LIN_CODE);
	if (linear_offset > CENTER_TRIM_CODE)
		offset_code = linear_offset - CENTER_TRIM_CODE;
	else
		offset_code = MAX_TRIM_CODE - linear_offset;

	return (prev_trim & ~VF_TRIM_OFFSET_MASK) | offset_code;
}

#define TRIM_14		0xFE
#define VF_TRIM_MASK	0xFF
static int smbchg_adjust_vfloat_mv_trim(struct smbchg_chip *chip,
						int delta_mv)
{
	int sign, delta_steps, rc = 0;
	u8 prev_trim, new_trim;
	int i;

	sign = delta_mv > 0 ? 1 : -1;
	delta_steps = (delta_mv + sign * VF_STEP_SIZE_MV / 2)
			/ VF_STEP_SIZE_MV;

	rc = smbchg_read(chip, &prev_trim, chip->misc_base + TRIM_14, 1);
	if (rc) {
		dev_err(chip->dev, "Unable to read trim 14: %d\n", rc);
		return rc;
	}

	for (i = 1; i <= abs(delta_steps)
			&& i <= vf_adjust_trim_steps_per_adjust; i++) {
		new_trim = (u8)smbchg_trim_add_steps(prev_trim,
				delta_steps > 0 ? 1 : -1);
		if (new_trim == prev_trim) {
			pr_smb(PR_STATUS,
				"VFloat trim unchanged from %02x\n", prev_trim);
			/* treat no trim change as an error */
			return -EINVAL;
		}

		rc = smbchg_sec_masked_write(chip, chip->misc_base + TRIM_14,
				VF_TRIM_MASK, new_trim);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't change vfloat trim rc=%d\n", rc);
		}
		pr_smb(PR_STATUS,
			"VFlt trim %02x to %02x, delta steps: %d\n",
			prev_trim, new_trim, delta_steps);
		prev_trim = new_trim;
	}

	return rc;
}

static void smbchg_vfloat_adjust_work(struct work_struct *work)
{
	struct smbchg_chip *chip = container_of(work,
				struct smbchg_chip,
				vfloat_adjust_work.work);
	int vbat_uv, vbat_mv, ibat_ua, rc, delta_vfloat_mv;
	bool taper, enable;

start:
	taper = (get_prop_charge_type(chip)
		== POWER_SUPPLY_CHARGE_TYPE_TAPER);
	enable = taper && (chip->parallel.current_max_ma == 0);

	if (!enable) {
		pr_smb(PR_MISC,
			"Stopping vfloat adj taper=%d parallel_ma = %d\n",
			taper, chip->parallel.current_max_ma);
		goto stop;
	}

	set_property_on_fg(chip, POWER_SUPPLY_PROP_UPDATE_NOW, 1);
	rc = get_property_from_fg(chip,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &vbat_uv);
	if (rc) {
		pr_smb(PR_STATUS,
			"bms psy does not support voltage rc = %d\n", rc);
		goto stop;
	}
	vbat_mv = vbat_uv / 1000;

	if ((vbat_mv - chip->vfloat_mv) < -1 * vf_adjust_max_delta_mv) {
		pr_smb(PR_STATUS, "Skip vbat out of range: %d\n", vbat_mv);
		goto start;
	}

	rc = get_property_from_fg(chip,
			POWER_SUPPLY_PROP_CURRENT_NOW, &ibat_ua);
	if (rc) {
		pr_smb(PR_STATUS,
			"bms psy does not support current_now rc = %d\n", rc);
		goto stop;
	}

	if (ibat_ua / 1000 > -chip->iterm_ma) {
		pr_smb(PR_STATUS, "Skip ibat too high: %d\n", ibat_ua);
		goto start;
	}

	pr_smb(PR_STATUS, "sample number = %d vbat_mv = %d ibat_ua = %d\n",
		chip->n_vbat_samples,
		vbat_mv,
		ibat_ua);

	chip->max_vbat_sample = max(chip->max_vbat_sample, vbat_mv);
	chip->n_vbat_samples += 1;
	if (chip->n_vbat_samples < vf_adjust_n_samples) {
		pr_smb(PR_STATUS, "Skip %d samples; max = %d\n",
			chip->n_vbat_samples, chip->max_vbat_sample);
		goto start;
	}
	/* if max vbat > target vfloat, delta_vfloat_mv could be negative */
	delta_vfloat_mv = chip->vfloat_mv - chip->max_vbat_sample;
	pr_smb(PR_STATUS, "delta_vfloat_mv = %d, samples = %d, mvbat = %d\n",
		delta_vfloat_mv, chip->n_vbat_samples, chip->max_vbat_sample);
	/*
	 * enough valid samples has been collected, adjust trim codes
	 * based on maximum of collected vbat samples if necessary
	 */
	if (delta_vfloat_mv > vf_adjust_high_threshold
			|| delta_vfloat_mv < -1 * vf_adjust_low_threshold) {
		rc = smbchg_adjust_vfloat_mv_trim(chip, delta_vfloat_mv);
		if (rc) {
			pr_smb(PR_STATUS,
				"Stopping vfloat adj after trim adj rc = %d\n",
				 rc);
			goto stop;
		}
		chip->max_vbat_sample = 0;
		chip->n_vbat_samples = 0;
		goto start;
	}

stop:
	chip->max_vbat_sample = 0;
	chip->n_vbat_samples = 0;
	smbchg_relax(chip, PM_REASON_VFLOAT_ADJUST);
	return;
}

static int smbchg_charging_status_change(struct smbchg_chip *chip)
{
	smbchg_low_icl_wa_check(chip);
	smbchg_vfloat_adjust_check(chip);
	return 0;
}

static void smbchg_adjust_batt_soc_work(struct work_struct *work)
{
	struct smbchg_chip *chip = container_of(work,
				struct smbchg_chip,
				batt_soc_work);

	if (get_prop_batt_capacity(chip) == 100)
		smbchg_check_and_notify_fg_soc(chip);
}

#define HOT_BAT_HARD_BIT	BIT(0)
#define HOT_BAT_SOFT_BIT	BIT(1)
#define COLD_BAT_HARD_BIT	BIT(2)
#define COLD_BAT_SOFT_BIT	BIT(3)
#define BAT_OV_BIT		BIT(4)
#define BAT_LOW_BIT		BIT(5)
#define BAT_MISSING_BIT		BIT(6)
#define BAT_TERM_MISSING_BIT	BIT(7)
static irqreturn_t batt_hot_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_hot = !!(reg & HOT_BAT_HARD_BIT);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	smbchg_low_icl_wa_check(chip);
#ifdef CONFIG_LGE_PM
#else
	smbchg_charging_status_change(chip);
	smbchg_parallel_usb_check_ok(chip);
#endif
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	smbchg_wipower_check(chip);
	return IRQ_HANDLED;
}

static irqreturn_t batt_cold_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_cold = !!(reg & COLD_BAT_HARD_BIT);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	smbchg_low_icl_wa_check(chip);
#ifdef CONFIG_LGE_PM
#else
	smbchg_charging_status_change(chip);
	smbchg_parallel_usb_check_ok(chip);
#endif
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	smbchg_wipower_check(chip);
	return IRQ_HANDLED;
}

static irqreturn_t batt_warm_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_warm = !!(reg & HOT_BAT_SOFT_BIT);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
#ifdef CONFIG_LGE_PM
#else
	smbchg_parallel_usb_check_ok(chip);
#endif
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t batt_cool_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_cool = !!(reg & COLD_BAT_SOFT_BIT);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
#ifdef CONFIG_LGE_PM
#else
	smbchg_parallel_usb_check_ok(chip);
#endif
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t batt_pres_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_present = !(reg & BAT_MISSING_BIT);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

#ifdef CONFIG_LGE_PM
static irqreturn_t batt_term_missing_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_present = !(reg & BAT_TERM_MISSING_BIT);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	smbchg_charging_en(chip, chip->batt_present);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}
#endif
static irqreturn_t vbat_low_handler(int irq, void *_chip)
{
	pr_warn_ratelimited("vbat low\n");
	return IRQ_HANDLED;
}

static irqreturn_t chg_error_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;

	pr_smb(PR_INTERRUPT, "chg-error triggered\n");
	smbchg_charging_status_change(chip);
	smbchg_parallel_usb_check_ok(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);

	smbchg_wipower_check(chip);
	return IRQ_HANDLED;
}

static irqreturn_t fastchg_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;

	pr_smb(PR_INTERRUPT, "p2f triggered\n");
	smbchg_charging_status_change(chip);
	smbchg_parallel_usb_check_ok(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);

	smbchg_wipower_check(chip);
	return IRQ_HANDLED;
}

static irqreturn_t chg_hot_handler(int irq, void *_chip)
{
	pr_warn_ratelimited("chg hot\n");
	smbchg_wipower_check(_chip);
	return IRQ_HANDLED;
}

#define USB_AICL_DEGLITCH_MASK		(BIT(5) | BIT(4) | BIT(3))
#define USB_AICL_DEGLITCH_SHORT		(BIT(5) | BIT(4) | BIT(3))
#define USB_AICL_DEGLITCH_LONG		0
#define MISC_TRIM_OPTIONS_15_8		0xF5
#define AICL_RERUN_MASK			(BIT(5) | BIT(4))
#define AICL_RERUN_ON			(BIT(5) | BIT(4))
#define AICL_RERUN_OFF			0
static void smbchg_aicl_deglitch_wa_en(struct smbchg_chip *chip, bool en)
{
	int rc;

	if (en && !chip->aicl_deglitch_on) {
		rc = smbchg_sec_masked_write(chip,
			chip->usb_chgpth_base + USB_AICL_CFG,
			USB_AICL_DEGLITCH_MASK, USB_AICL_DEGLITCH_SHORT);
		if (rc)
			return;
		rc = smbchg_sec_masked_write(chip,
			chip->misc_base + MISC_TRIM_OPTIONS_15_8,
			AICL_RERUN_MASK, AICL_RERUN_ON);
		if (rc)
			return;
		pr_smb(PR_STATUS, "AICL deglitch set to short\n");
	} else if (!en && chip->aicl_deglitch_on) {
		rc = smbchg_sec_masked_write(chip,
			chip->usb_chgpth_base + USB_AICL_CFG,
			USB_AICL_DEGLITCH_MASK, USB_AICL_DEGLITCH_LONG);
		if (rc)
			return;
		rc = smbchg_sec_masked_write(chip,
			chip->misc_base + MISC_TRIM_OPTIONS_15_8,
			AICL_RERUN_MASK, AICL_RERUN_OFF);
		if (rc)
			return;
		pr_smb(PR_STATUS, "AICL deglitch set to normal\n");
	}
	chip->aicl_deglitch_on = en;
}

static void smbchg_aicl_deglitch_wa_check(struct smbchg_chip *chip)
{
#ifdef CONFIG_LGE_PM
	u8 reg, chg_type;
	int rc;
	rc = smbchg_read(chip, &reg, chip->chgr_base + CHGR_STS, 1);
	if (rc < 0)
		dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
	else
		chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;
	smbchg_aicl_deglitch_wa_en(chip,
		chg_type == BATT_TAPER_CHG_VAL);
#else
	smbchg_aicl_deglitch_wa_en(chip,
		get_prop_charge_type(chip) == POWER_SUPPLY_CHARGE_TYPE_TAPER);
#endif
}

#ifdef CONFIG_LGE_PM_UNIFIED_WLC
#define WLC_EOC_ACK 0x80
#endif
static irqreturn_t chg_term_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;
#ifdef CONFIG_LGE_PM_UNIFIED_WLC
	bool eoc_dc_present = is_dc_present(chip);
#endif
	smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);

	smbchg_aicl_deglitch_wa_check(chip);
	smbchg_parallel_usb_check_ok(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
#ifdef CONFIG_BATTERY_MAX17048
#else
	if (reg & BAT_TCC_REACHED_BIT)
		schedule_work(&chip->batt_soc_work);
#endif
#ifdef CONFIG_LGE_PM
	msleep(10);
	if (reg == 0x80)
	{
		smbchg_float_voltage_set(chip, chip->target_vfloat_mv + 50);
	}
	else
	{
		if (chip->target_vfloat_mv != chip->vfloat_mv)
			smbchg_float_voltage_set(chip, chip->target_vfloat_mv);
	}
	pr_smb(PR_LGE, "current vfloat_mv = %d, target vfloat_mv = %d \n",
		chip->vfloat_mv, chip->target_vfloat_mv);
#endif

#ifdef CONFIG_LGE_PM_UNIFIED_WLC
	if((reg == WLC_EOC_ACK) && eoc_dc_present){
		wireless_chg_term_handler();
	}
#endif
	return IRQ_HANDLED;
}

static irqreturn_t taper_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	taper_irq_en(chip, false);
	smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	smbchg_aicl_deglitch_wa_check(chip);
	smbchg_parallel_usb_taper(chip);
	smbchg_wipower_check(chip);
	return IRQ_HANDLED;
}

static irqreturn_t recharge_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	smbchg_aicl_deglitch_wa_check(chip);
	smbchg_parallel_usb_check_ok(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t safety_timeout_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->misc_base + RT_STS, 1);
	pr_warn_ratelimited("safety timeout rt_stat = 0x%02x\n", reg);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

#ifdef CONFIG_LGE_PM
#else
/**
 * power_ok_handler() - called when the switcher turns on or turns off
 * @chip: pointer to smbchg_chip
 * @rt_stat: the status bit indicating switcher turning on or off
 */
static irqreturn_t power_ok_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->misc_base + RT_STS, 1);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	return IRQ_HANDLED;
}
#endif

/**
 * dcin_uv_handler() - called when the dc voltage crosses the uv threshold
 * @chip: pointer to smbchg_chip
 * @rt_stat: the status bit indicating whether dc voltage is uv
 */
#define DCIN_UNSUSPEND_DELAY_MS		1000
static irqreturn_t dcin_uv_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool dc_present = is_dc_present(chip);
#ifdef CONFIG_LGE_PM_UNIFIED_WLC
	bool usb_present = is_usb_present(chip);
#endif
	pr_smb(PR_LGE, "chip->dc_present = %d dc_present = %d\n",
			chip->dc_present, dc_present);

	if (chip->dc_present != dc_present) {
		/* dc changed */
		chip->dc_present = dc_present;
		if (chip->psy_registered)
			power_supply_changed(&chip->dc_psy);
#ifdef CONFIG_LGE_PM_UNIFIED_WLC
		if(!usb_present)
			wireless_interrupt_handler(dc_present);

		/* dc path refresh for power source detect */
		if (dc_present)
			smbchg_dc_en(chip, true, REASON_USER);
		else
			smbchg_dc_en(chip, false, REASON_USER);
#endif

	}

	smbchg_wipower_check(chip);
	return IRQ_HANDLED;
}

static void handle_usb_removal(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);

#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "setting usb psy type = unknown, present = %d\n",
			chip->usb_present);

	if (chip->target_vfloat_mv != chip->vfloat_mv)
		smbchg_float_voltage_set(chip, chip->target_vfloat_mv);
	pr_smb(PR_LGE, "current vfloat_mv = %d, target vfloat_mv = %d \n",
		chip->vfloat_mv, chip->target_vfloat_mv);
#endif

	if (chip->usb_psy) {
		pr_smb(PR_MISC, "setting usb psy type = %d\n",
				POWER_SUPPLY_TYPE_UNKNOWN);
		pr_smb(PR_MISC, "setting usb psy present = %d\n",
				chip->usb_present);
		power_supply_set_supply_type(chip->usb_psy,
				POWER_SUPPLY_TYPE_UNKNOWN);
		power_supply_set_present(chip->usb_psy, chip->usb_present);
		schedule_work(&chip->usb_set_online_work);
	}
	if (parallel_psy)
		power_supply_set_present(parallel_psy, false);
	if (chip->parallel.avail && chip->enable_aicl_wake) {
		disable_irq_wake(chip->aicl_done_irq);
		chip->enable_aicl_wake = false;
		smbchg_aicl_deglitch_wa_check(chip);
	}
#ifdef CONFIG_LGE_PM_CHARGE_INFO
	cancel_delayed_work(&chip->charging_inform_work);
#endif
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	chip->is_parallel_chg_dis_by_taper = 0;
	if (!is_factory_cable())
		chip->target_fastchg_current_ma = DCP_FCC_DEFAULT;
#endif
#ifdef CONFIG_LGE_PM_MAXIM_EVP_CONTROL
	disable_evp_chg(chip);
#endif
#ifdef CONFIG_LGE_PM_DETECT_QC20
	disable_QC20_chg(chip);
#endif
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	chip->is_parallel_chg_dis_by_otp = 0;
	chip->is_parallel_chg_changed_by_taper = 0;
#endif
}

static void handle_usb_insertion(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);
	enum power_supply_type usb_supply_type;
	int rc, type;
	char *usb_type_name = "null";
	u8 reg = 0;

	/* usb inserted */
	rc = smbchg_read(chip, &reg, chip->misc_base + IDEV_STS, 1);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
	type = get_type(reg);
	usb_type_name = get_usb_type_name(type);
	usb_supply_type = get_usb_supply_type(type);
#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "inserted %s, usb psy type = %d stat_5 = 0x%02x\n",
			usb_type_name, usb_supply_type, reg);

	if (chip->target_vfloat_mv != chip->vfloat_mv)
		smbchg_float_voltage_set(chip, chip->target_vfloat_mv);
	pr_smb(PR_LGE, "current vfloat_mv = %d, target vfloat_mv = %d \n",
		chip->vfloat_mv, chip->target_vfloat_mv);

	smbchg_charging_en(chip, chip->batt_present);
#else
	pr_smb(PR_STATUS, "inserted %s, usb psy type = %d stat_5 = 0x%02x\n",
			usb_type_name, usb_supply_type, reg);
#endif
	if (chip->usb_psy) {
		pr_smb(PR_MISC, "setting usb psy type = %d\n",
				usb_supply_type);
#if defined(CONFIG_SLIMPORT_ANX7812) || defined(CONFIG_SLIMPORT_ANX7816)
		if (slimport_is_connected())
		{
			pr_smb(PR_LGE, "slimport_is_connected. ignore PMI charger type detection result.\n");
			// do not set usb_type. do msm usb phy charger type detection.
		}
		else
		{
			power_supply_set_supply_type(chip->usb_psy, usb_supply_type);
		}
#else
		power_supply_set_supply_type(chip->usb_psy, usb_supply_type);
#endif
		pr_smb(PR_MISC, "setting usb psy present = %d\n",
				chip->usb_present);
		power_supply_set_present(chip->usb_psy, chip->usb_present);
		schedule_work(&chip->usb_set_online_work);
	}
	if (parallel_psy)
		power_supply_set_present(parallel_psy, true);
	if (chip->parallel.avail && !chip->enable_aicl_wake) {
		enable_irq_wake(chip->aicl_done_irq);
		chip->enable_aicl_wake = true;
	}
#ifdef CONFIG_LGE_PM_CHARGE_INFO
	schedule_delayed_work(&chip->charging_inform_work,
		round_jiffies_relative(msecs_to_jiffies(CHARGING_INFORM_NORMAL_TIME)));
#endif
#ifdef CONFIG_LGE_PM_DETECT_QC20
	schedule_delayed_work(&chip->detect_QC20_work, MONITOR_USBIN_QC20_CHG);
#endif
}

#ifdef CONFIG_LGE_PM_COMMON
static void smbchg_usb_insert_work(struct work_struct *work) {
	struct smbchg_chip *chip =
		container_of(work, struct smbchg_chip, usb_insert_work.work);
	handle_usb_insertion(chip);
}
#endif

/**
 * usbin_uv_handler() - this is called when USB charger is removed
 * @chip: pointer to smbchg_chip chip
 * @rt_stat: the status bit indicating chg insertion/removal
 */
static irqreturn_t usbin_uv_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool usb_present = is_usb_present(chip);

#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);

	if ( (chip->uevent_wake_lock.ws.name) != NULL )
		wake_lock_timeout(&chip->uevent_wake_lock, HZ*3);
#else
	pr_smb(PR_STATUS, "chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);
#endif

#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	if (chip->usb_present && chip->check_aicl_complete) {
		pr_smb(PR_LGE, "aicl done is not triggered. skipping...\n");
		return IRQ_HANDLED;
	}
#endif
#ifdef CONFIG_LGE_PM_COMMON
	cancel_delayed_work(&chip->usb_insert_work);
#endif
	if (chip->usb_present && !usb_present) {
		/* USB removed */
		chip->usb_present = usb_present;
		handle_usb_removal(chip);
	}
#ifdef CONFIG_LGE_PM_COMMON
	else if (!chip->usb_present && usb_present) {
		/* USB inserted */
		chip->usb_present = usb_present;
		schedule_delayed_work(&chip->usb_insert_work,
			msecs_to_jiffies(1000));
	}
#endif
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	smbchg_monitor_batt_temp_queue(chip);
#endif
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	if (chip->usb_present) {
		chip->enable_polling = true;
	} else {
		chip->enable_polling = false;
		chip->polling_cnt = 0;
	}
#endif
	smbchg_wipower_check(chip);
	return IRQ_HANDLED;
}

#ifdef CONFIG_LGE_PM
static irqreturn_t usbin_ov_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool usb_present = is_usb_present(chip);

	pr_smb(PR_LGE, "chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);

	return IRQ_HANDLED;
}
#endif

/**
 * src_detect_handler() - this is called when USB charger type is detected, use
 *			it for handling USB charger insertion
 * @chip: pointer to smbchg_chip
 * @rt_stat: the status bit indicating chg insertion/removal
 */
static irqreturn_t src_detect_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool usb_present = is_usb_present(chip);

#ifdef CONFIG_LGE_PM_COMMON
	pr_smb(PR_LGE, "usb_present = %d\n", usb_present);
#else
	pr_smb(PR_STATUS, "chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);
#endif

#ifdef CONFIG_LGE_PM_COMMON
#else
	if (!chip->usb_present && usb_present) {
		/* USB inserted */
		chip->usb_present = usb_present;
		handle_usb_insertion(chip);
	}
#endif
	return IRQ_HANDLED;
}

/**
 * otg_oc_handler() - called when the usb otg goes over current
 */
#define NUM_OTG_RETRIES			5
#define OTG_OC_RETRY_DELAY_US		50000
static irqreturn_t otg_oc_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
    int retries = chip->otg_retries;
	s64 elapsed_us = ktime_us_delta(ktime_get(), chip->otg_enable_time);

	if (elapsed_us > OTG_OC_RETRY_DELAY_US)
		chip->otg_retries = 0;

	pr_smb(PR_INTERRUPT, "triggered otg_retries=%d\n",retries);
	/*
	 * Due to a HW bug in the PMI8994 charger, the current inrush that
	 * occurs when connecting certain OTG devices can cause the OTG
	 * overcurrent protection to trip.
	 *
	 * The work around is to try reenabling the OTG when getting an
	 * overcurrent interrupt once.
	 */
	if (retries < NUM_OTG_RETRIES) {
		retries++;
#ifdef CONFIG_LGE_PM
		pr_smb(PR_LGE, "Retrying OTG enable. Try #%d, elapsed_us %lld\n",
#else
		pr_smb(PR_STATUS, "Retrying OTG enable. Try #%d, elapsed_us %lld\n",
#endif

							retries, elapsed_us);

		chip->otg_retries = retries;
		smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
							OTG_EN, 0);
		msleep(20);
		smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
							OTG_EN, OTG_EN);
		chip->otg_enable_time = ktime_get();
	}
	return IRQ_HANDLED;
}

/**
 * otg_fail_handler() - called when the usb otg fails
 * (when vbat < OTG UVLO threshold)
 */
static irqreturn_t otg_fail_handler(int irq, void *_chip)
{
	pr_smb(PR_INTERRUPT, "triggered\n");
	return IRQ_HANDLED;
}

/**
 * aicl_done_handler() - called when the usb AICL algorithm is finished
 *			and a current is set.
 */
static irqreturn_t aicl_done_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool usb_present = is_usb_present(chip);

#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	pr_smb(PR_INTERRUPT, "aicl_done triggered: %d\n",
		smbchg_get_aicl_level_ma(chip));
#else
	pr_smb(PR_INTERRUPT, "aicl_done triggered\n");
#endif
	if (usb_present)
		smbchg_parallel_usb_check_ok(chip);
	return IRQ_HANDLED;
}

/**
 * usbid_change_handler() - called when the usb RID changes.
 * This is used mostly for detecting OTG
 */
static irqreturn_t usbid_change_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool otg_present;

	pr_smb(PR_INTERRUPT, "triggered\n");

	/*
	 * After the falling edge of the usbid change interrupt occurs,
	 * there may still be some time before the ADC conversion for USB RID
	 * finishes in the fuel gauge. In the worst case, this could be up to
	 * 15 ms.
	 *
	 * Sleep for 20 ms (minimum msleep time) to wait for the conversion to
	 * finish and the USB RID status register to be updated before trying
	 * to detect OTG insertions.
	 */
	msleep(20);
	otg_present = is_otg_present(chip);
	if (chip->usb_psy)
		power_supply_set_usb_otg(chip->usb_psy, otg_present ? 1 : 0);
#ifdef CONFIG_LGE_PM
	if (otg_present)
		pr_smb(PR_LGE, "OTG detected\n");
#else
	if (otg_present)
		pr_smb(PR_STATUS, "OTG detected\n");
#endif

	return IRQ_HANDLED;
}

#ifdef CONFIG_LGE_PM
#define CHGR_STS			0x0E
#define CHGR_CFG2			0xFC
#define CHG_EN_COMMAND_BIT		BIT(6)
#define CHARGER_INHIBIT_BIT		BIT(0)
#define ICL_STS_2_REG			0x9
#define USBIN_SUSPEND_STS_BIT		BIT(3)
#ifdef __DUMP_REG__
static inline void dump_reg_lge(struct smbchg_chip *chip, u16 addr,
		const char *name)
{
	u8 reg;

	smbchg_read(chip, &reg, addr, 1);
	pr_smb(PR_LGE, "%s - %04X = %02X\n", name, addr, reg);
}
static void dump_regs_lge(struct smbchg_chip *chip)
{
	dump_reg_lge(chip, chip->chgr_base + 0x0E, "CHGR_STS");
	dump_reg_lge(chip, chip->chgr_base + 0x10, "CHGR_INT_RT");
	dump_reg_lge(chip, chip->bat_if_base + 0x10, "BAT_IF_INT_RT");
	dump_reg_lge(chip, chip->bat_if_base + 0x42, "BAT_IF_CMD_CHG");
	dump_reg_lge(chip, chip->usb_chgpth_base + 0x09, "USB ICL_STS_2");
	dump_reg_lge(chip, chip->usb_chgpth_base + 0x0B, "USB RID_STS");
	dump_reg_lge(chip, chip->usb_chgpth_base + 0x10, "USB RT_STS");
	dump_reg_lge(chip, chip->usb_chgpth_base + 0x40, "USB CMD_IL");
	dump_reg_lge(chip, chip->misc_base + 0x10, "MISC RT_STS");
	dump_reg_lge(chip, chip->misc_base + 0x10, "MISC RT_STS");
}
#endif
static void smbchg_restart_charging_check(struct smbchg_chip *chip)
{
	int rc;
	u8 reg, chg_type, reg2;

	if (!chip->usb_present || is_factory_cable())
		return;

	pr_smb(PR_LGE, "check restart charge condition\n");

	rc = smbchg_read(chip, &reg, chip->chgr_base + CHGR_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
		return;
	}

	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;
	if (chg_type == BATT_NOT_CHG_VAL && get_prop_batt_capacity(chip) <= 99) {
		rc = smbchg_read(chip, &reg2,
				chip->usb_chgpth_base + ICL_STS_2_REG, 1);
		if (rc < 0) {
			dev_err(chip->dev, "Could not read usb icl sts 2: %d\n", rc);
			return;
		}

		if (!!(reg2 & USBIN_SUSPEND_STS_BIT)) {
			pr_smb(PR_LGE, "input voltage collapsed! usbin suspended..\n");
#ifdef __DUMP_REG__
			dump_regs_lge(chip);
#endif
			return;
		}

		if (lge_get_boot_mode() != LGE_BOOT_MODE_CHARGERLOGO) {
			if (chip->vfloat_mv == chip->target_vfloat_mv)
				return;
		}

		pr_smb(PR_LGE, "trying restart charge\n");
		rc = smbchg_sec_masked_write(chip, chip->chgr_base + CHGR_CFG2,
				CHARGER_INHIBIT_BIT | CHG_EN_COMMAND_BIT, 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set chgr_cfg2 rc=%d\n", rc);
			return;
		}

		/* Sleep for 100 ms in order to let charge inhibit fall */
		msleep(100);

		rc = smbchg_sec_masked_write(chip, chip->chgr_base + CHGR_CFG2,
				CHG_EN_COMMAND_BIT, CHG_EN_COMMAND_BIT);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set chgr_cfg2 rc=%d\n", rc);
			return;
		}

		/* Sleep for 500 ms in order to give charging a chance to restart */
		msleep(500);
	}
}
#endif

#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
static int smbchg_thermal_fastchg_current_ma;
static int smbchg_set_thermal_limited_chg_current(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	NULL_CHECK(the_chip, -EINVAL);

	if (is_factory_cable())
		return 0;

	pr_smb(PR_LGE, "request - %d\n", smbchg_thermal_fastchg_current_ma);

	the_chip->chg_current_te = smbchg_thermal_fastchg_current_ma;

	smbchg_monitor_batt_temp_queue(the_chip);

	return 0;
}
module_param_call(smbchg_thermal_fastchg_current_ma, smbchg_set_thermal_limited_chg_current,
	param_get_uint, &smbchg_thermal_fastchg_current_ma, 0644);

#define OTP_MIN_FCC_MA	600
static void smbchg_adjust_fcc_for_otp(struct smbchg_chip *chip, struct charging_info req)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);
	union power_supply_propval pval = {0, };
	int req_target_fcc, req_te_fcc, req_final_fcc;
	int now_total_fcc, now_pmi_fcc, now_smb_fcc;

	req_target_fcc = req.chg_current_ma;
	req_te_fcc = req.chg_current_te;
	req_final_fcc = chip->otp_ibat_current;
	now_pmi_fcc = chip->fastchg_current_ma;

	if (chip->usb_suspended == 1) {
		req_final_fcc = 0;
		now_pmi_fcc = 0;
	}

	/* for single charging */
	if (!parallel_psy) {
		now_total_fcc = now_pmi_fcc;

		if (req_final_fcc != now_total_fcc)
			smbchg_set_fastchg_current(chip, req_final_fcc);
		goto OUT;
	}

	/*  for parallel charging */
	parallel_psy->get_property(parallel_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
	now_smb_fcc = pval.intval/1000;

	if (chip->parallel.current_max_ma == 0)
		now_smb_fcc = 0;

	now_total_fcc = now_pmi_fcc + now_smb_fcc;

	if (req_final_fcc == now_total_fcc)
		goto OUT;

	if (chip->is_parallel_chg_changed_by_taper) {
		pr_smb(PR_LGE, " is_parallel_chg_changed_by_taper = %d \n",
			chip->is_parallel_chg_changed_by_taper);
		if (req_final_fcc >= now_total_fcc)
			goto OUT;
	}

	 /*
	  * parallel charging is disabled, when requested fcc < OTP_MIN_FCC_MA.
	  * re-enable pararelle charging if possible, when requested fcc >= OTP_MIN_FCC_MA.
	  */
	pr_smb(PR_LGE, "is_parallel_chg_dis_by_otp = %d, parallel.current_max_ma = %dmA \n",
				 chip->is_parallel_chg_dis_by_otp, chip->parallel.current_max_ma);

	if (req_final_fcc >= OTP_MIN_FCC_MA && chip->is_parallel_chg_dis_by_otp) {
		chip->is_parallel_chg_dis_by_otp = 0;
		smbchg_parallel_usb_check_ok(chip);
	}

	if (chip->parallel.current_max_ma != 0) {
	/* parallel charging available */
		if (req_final_fcc >= OTP_MIN_FCC_MA) {
			if (now_pmi_fcc > now_smb_fcc) {
				smbchg_set_fastchg_current(chip, req_final_fcc / 2);
			} else {
				smbchg_set_fastchg_current(chip, req_final_fcc / 2);
				pval.intval = req_final_fcc * 1000 / 2;
				parallel_psy->set_property(parallel_psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
			}
		} else {
			chip->is_parallel_chg_dis_by_otp = 1;
			smbchg_parallel_usb_check_ok(chip);
			smbchg_set_fastchg_current(chip, req_final_fcc);
		}
	/* parallel charging unavailable */
	} else {
		if (chip->is_parallel_chg_changed_by_taper)
			smbchg_set_fastchg_current(chip, req_final_fcc / 2);
		else
			smbchg_set_fastchg_current(chip, req_final_fcc);
	}

OUT:
	pr_smb(PR_LGE, "req_target_fcc = %dmA req_te_fcc = %dmA"\
		" req_final_fcc = %dmA  now_total_fcc = %dmA [%dmA(pmi) + %dmA(smb)] \n",
		req_target_fcc, req_te_fcc, req_final_fcc, now_total_fcc, now_pmi_fcc, now_smb_fcc);
}

static int temp_before;
static void smbchg_monitor_batt_temp(struct work_struct *work)
{
	struct smbchg_chip *chip =
	                container_of(work, struct smbchg_chip, battemp_work.work);
	union power_supply_propval ret = {0,};
	struct charging_info req;
	struct charging_rsp res;
	bool is_changed = false;
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	struct qpnp_vadc_result results;
	int rc = 0;
#endif
	int64_t smb_current = 0;

	if (IS_ERR_OR_NULL(chip)) {
		pr_err("smbchg_chip is null\n");
		goto OUT;
	}

#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	if (IS_ERR_OR_NULL(chip->vadc_smb_dev)) {
		chip->vadc_smb_dev = qpnp_get_vadc(chip->dev, "smb");
		if (IS_ERR_OR_NULL(chip->vadc_smb_dev)) {
			pr_err("vadc_smb_dev is not init yet");
			goto OUT;
		}
	}
#endif

	if (IS_ERR_OR_NULL(&chip->batt_psy)) {
		pr_err("battery psy is not init yet\n");
		req.batt_temp = LGE_DEFAULT_TEMP / 10;
	} else {
		chip->batt_psy.get_property(&(chip->batt_psy),
				POWER_SUPPLY_PROP_TEMP, &ret);
		req.batt_temp = ret.intval / 10;
		pr_debug("temp = %d\n", req.batt_temp);
	}

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	if (chip->bms_psy) {
#ifndef CONFIG_BATTERY_MAX17048
		chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &ret);
		req.batt_volt = ret.intval;
		pr_debug("volt = %d\n", req.batt_volt);
#endif
		chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CURRENT_NOW, &ret);
		req.current_now = ret.intval;
		pr_debug("current = %d\n", req.current_now);

	} else {
		pr_err("No BMS supply registered, use dummy data\n");
#ifndef CONFIG_BATTERY_MAX17048
		req.batt_volt = 4111;
#endif
		req.current_now = 222;
	}

#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	if(chip->chg_enabled) {
		rc = qpnp_vadc_read(chip->vadc_smb_dev, P_MUX5_1_1, &results);
		if (rc) {
			pr_err("Unable to read smb135x vchg rc=%d\n", rc);
		}
		smb_current = (results.physical * 125) / 100;
		/* control charging param */
		if ( chip->usb_present && chip->enable_polling) {
			smbchg_set_lcd_limited_current_max(chip);
		}
	}
#endif

#ifdef CONFIG_BATTERY_MAX17048
	chip->batt_psy.get_property(&(chip->batt_psy),
			  POWER_SUPPLY_PROP_VOLTAGE_NOW, &ret);
	req.batt_volt = ret.intval;
#endif

	pr_smb(PR_LGE, "voltage = %d current = %d (smb = %lld)\n",
		req.batt_volt, req.current_now, smb_current);

	req.chg_current_ma = chip->target_fastchg_current_ma;
	req.chg_current_te = chip->chg_current_te;

#ifdef CONFIG_LGE_PM_FACTORY_PSEUDO_BATTERY
	chip->usb_psy->get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_TYPE, &ret);
	if ((ret.intval == POWER_SUPPLY_TYPE_USB) &&
		(pseudo_batt_info.mode == 1 || safety_timer == 0)) {
		req.chg_current_te = chip->target_fastchg_current_ma;
	}
#endif
	req.is_charger = chip->usb_present;

	lge_monitor_batt_temp(req, &res);

	if (((res.change_lvl != STS_CHE_NONE) && req.is_charger) ||
		(res.force_update == true)) {
		if (res.change_lvl == STS_CHE_NORMAL_TO_DECCUR ||
			(res.force_update == true && res.state == CHG_BATT_DECCUR_STATE &&
			res.dc_current != DC_CURRENT_DEF && res.change_lvl != STS_CHE_STPCHG_TO_DECCUR
			)) {
			chip->otp_ibat_current = res.dc_current;
		} else if (res.change_lvl == STS_CHE_NORMAL_TO_STPCHG ||
			(res.force_update == true &&
			res.state == CHG_BATT_STPCHG_STATE)) {
			wake_lock(&chip->lcs_wake_lock);
			smbchg_charging_en(chip, false);
		} else if (res.change_lvl == STS_CHE_DECCUR_TO_NORAML) {
			chip->otp_ibat_current = res.dc_current;
		} else if (res.change_lvl == STS_CHE_DECCUR_TO_STPCHG) {
			wake_lock(&chip->lcs_wake_lock);
			smbchg_charging_en(chip, false);
		} else if (res.change_lvl == STS_CHE_STPCHG_TO_NORMAL) {
			chip->otp_ibat_current = res.dc_current;
			smbchg_charging_en(chip, true);
			wake_unlock(&chip->lcs_wake_lock);
		}
		else if (res.change_lvl == STS_CHE_STPCHG_TO_DECCUR) {
			chip->otp_ibat_current = res.dc_current;
			smbchg_charging_en(chip, true);
			wake_unlock(&chip->lcs_wake_lock);
		}
		else if (res.force_update == true && res.state == CHG_BATT_NORMAL_STATE &&
			res.dc_current != DC_CURRENT_DEF) {
			chip->otp_ibat_current = res.dc_current;
		}
	}

	if (!is_factory_cable())
		smbchg_adjust_fcc_for_otp(chip, req);

#ifdef CONFIG_LGE_PM
	if (res.state != CHG_BATT_STPCHG_STATE) {
		smbchg_restart_charging_check(chip);
	}
#endif

	if (chip->pseudo_ui_chg ^ res.pseudo_chg_ui) {
		is_changed = true;
		chip->pseudo_ui_chg = res.pseudo_chg_ui;
	}

	if (chip->btm_state ^ res.btm_state) {
		is_changed = true;
		chip->btm_state = res.btm_state;
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
		if (res.btm_state == BTM_HEALTH_GOOD) {
			cancel_delayed_work_sync(&chip->iusb_change_work);
			schedule_delayed_work(&chip->iusb_change_work,0);
		}
#endif
	}

	if (temp_before != req.batt_temp) {
		is_changed = true;
		temp_before = req.batt_temp;
	}

	if (is_changed == true)
		power_supply_changed(&chip->batt_psy);

OUT:
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	if (chip->usb_present && chip->enable_polling)
		schedule_delayed_work(&chip->battemp_work, (20*HZ));
	else
#endif
	schedule_delayed_work(&chip->battemp_work,
			MONITOR_BATTEMP_POLLING_PERIOD);
}
#endif

#ifdef CONFIG_LGE_PM_CHARGE_INFO
static char *cable_type_str[] = {
		"NOT INIT", "MHL 1K", "U_28P7K", "28P7K", "56K",
		"100K", "130K", "180K", "200K", "220K",
		"270K", "330K", "620K", "910K", "OPEN"
	};

static char *power_supply_type_str[] = {
		"Unknown", "Battery", "UPS", "Mains", "USB",
		"USB_DCP", "USB_CDP", "USB_ACA", "Wireless", "BMS",
		"USB_Parallel", "fuelgauge", "Wipower"
	};

static char *batt_fake_str[] = {
		"FAKE_OFF", "FAKE_ON"
	};

#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
static char *dual_chg_str[] = {
		"DUAL_OFF", "DUAL_ON"
	};
#endif

static char* get_usb_type(struct smbchg_chip *chip)
{
	union power_supply_propval ret = {0,};

	if (!chip->usb_psy)
	{
		pr_smb(PR_LGE, "usb power supply is not registerd\n");
		return "null";
	}

	chip->usb_psy->get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_TYPE, &ret);

	return power_supply_type_str[ret.intval];
}

#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
static int get_smb_ibat_set(struct smbchg_chip* chip)
{
	union power_supply_propval ret = {0, };

	if (!chip->parallel.psy)
	{
		pr_smb(PR_LGE, "parallel power supply is not registerd\n");
		return 0;
	}

	chip->parallel.psy->get_property(chip->parallel.psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &ret);

	return ret.intval/1000;
}

static int get_smb_ibat_now(struct smbchg_chip* chip)
{
	int rc =0;
	struct qpnp_vadc_result results;

	if (IS_ERR_OR_NULL(chip->vadc_smb_dev)) {
		chip->vadc_smb_dev = qpnp_get_vadc(chip->dev, "smb");
		if (IS_ERR_OR_NULL(chip->vadc_smb_dev)) {
			pr_err("vadc_smb_dev is not init yet");
			return 0;
		}
	}

	rc = qpnp_vadc_read(chip->vadc_smb_dev, P_MUX5_1_1, &results);
	if (rc) {
		pr_err("Unable to read smb135x vchg rc=%d\n", rc);
		return 0;
	}
	return (results.physical * 125) / 100;
}
#endif

static void charging_information(struct work_struct *work)
{
	struct smbchg_chip *chip =
				container_of(work, struct smbchg_chip, charging_inform_work.work);
	int usb_present = chip->usb_present;
	char *usb_type_name = get_usb_type(chip);
	char *cable_type_name = cable_type_str[lge_pm_get_cable_type()];
	int usbin_vol = smbchg_get_usb_adc(chip);
	char *batt_fake = batt_fake_str[pseudo_batt_info.mode];
	int batt_temp = get_prop_batt_temp(chip)/10;
	int batt_soc = max17048_get_capacity();
	int batt_vol = max17048_get_voltage();
	int pmi_iusb_set = chip->usb_max_current_ma;
	int pmi_ibat_set = chip->fastchg_current_ma;
	int total_ibat_now = get_prop_batt_current_now(chip)/1000;
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	int smb_iusb_set = chip->parallel.current_max_ma;
	int smb_ibat_set = get_smb_ibat_set(chip);
	int smb_ibat_now = get_smb_ibat_now(chip)/1000;
	char *dual_chg_en = dual_chg_str[(smb_iusb_set > 2)];
#endif

#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	pr_smb(PR_LGE, "[C], USB_PRESENT, USB_TYPE, CABLE_INFO, USBIN_VOL, "
			"BATT_FAKE, BATT_TEMP, BATT_SOC, BATT_VOL, "
			"PMI_IUSB_SET, SMB_IUSB_SET, PMI_IBAT_SET, SMB_IBAT_SET, "
			"TOTAL_IBAT_NOW, SMB_IBAT_NOW, DUAL_CHG_EN\n");
	pr_smb(PR_LGE, "[I], %d, %s, %s, %d, "
			"%s, %d, %d, %d, "
			"%d, %d, %d, %d, "
			"%d, %d, %s\n",
			usb_present, usb_type_name, cable_type_name, usbin_vol,
			batt_fake, batt_temp, batt_soc, batt_vol,
			pmi_iusb_set, smb_iusb_set, pmi_ibat_set, smb_ibat_set,
			total_ibat_now, smb_ibat_now, dual_chg_en);
#else
	pr_smb(PR_LGE, "[C], USB_PRESENT, USB_TYPE, CABLE_INFO, USBIN_VOL, "
			"BATT_FAKE, BATT_TEMP, BATT_SOC, BATT_VOL, "
			"PMI_IUSB_SET, PMI_IBAT_SET, TOTAL_IBAT_NOW\n");
	pr_smb(PR_LGE, "[I], %d, %s, %s, %d; "
			"%s, %d, %d, %d; "
			"%d, %d, %d\n",
			usb_present, usb_type_name, cable_type_name, usbin_vol,
			batt_fake, batt_temp, batt_soc, batt_vol,
			pmi_iusb_set, pmi_ibat_set, total_ibat_now);
#endif
	schedule_delayed_work(&chip->charging_inform_work,
		round_jiffies_relative(msecs_to_jiffies(CHARGING_INFORM_NORMAL_TIME)));
}
#endif

#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
static ssize_t at_chg_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;
	bool b_chg_ok = false;
	int chg_type;
	struct smbchg_chip *chip = dev_get_drvdata(dev);

	NULL_CHECK(chip, -EINVAL);

	chg_type = get_prop_charge_type(chip);
	if (chg_type != POWER_SUPPLY_CHARGE_TYPE_NONE) {
		b_chg_ok = true;
		r = snprintf(buf, 3, "%d\n", b_chg_ok);
		pr_info("[Diag] true  ! buf = %s, charging = 1\n", buf);
	} else {
		b_chg_ok = false;
		r = snprintf(buf, 3, "%d\n", b_chg_ok);
		pr_info("[Diag] false ! buf = %s, charging = 0\n", buf);
	}

	return r;
}

static ssize_t at_chg_status_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int enable = 0;
	struct smbchg_chip *chip = dev_get_drvdata(dev);

	NULL_CHECK(chip, -EINVAL);
	NULL_CHECK(count, -EINVAL);

	if (strncmp(buf, "0", 1) == 0)
		enable = false;
	else if (strncmp(buf, "1", 1) == 0)
		enable = true;
	else {
		pr_err("[Diag] abnormal input value\n");
		return -EINVAL;
	}

	pr_info("[Diag] %s charging start\n",
			enable ? "start" : "stop");

	smbchg_usb_en(chip, enable ? 1 : 0, REASON_USER);
	if (chip->dc_psy_type != -EINVAL)
		smbchg_dc_en(chip, enable ? 1 : 0, REASON_USER);
	chip->chg_enabled = enable ? 1 : 0;
	schedule_work(&chip->usb_set_online_work);

	return 1;
}

static ssize_t at_chg_complete_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int gauge_level = 0;
	int r = 0;
	struct smbchg_chip *chip = dev_get_drvdata(dev);

	NULL_CHECK(chip, -EINVAL);

	gauge_level = get_prop_batt_capacity(chip);

	if (gauge_level == 100)
		r = snprintf(buf, 3, "%d\n", 0);
	else
		r = snprintf(buf, 3, "%d\n", 1);

	pr_err("[Diag] buf : %s, gauge : %d\n", buf, gauge_level);

	return r;
}

static ssize_t at_chg_complete_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	bool enable = 0;
	struct smbchg_chip *chip = dev_get_drvdata(dev);

	NULL_CHECK(chip, -EINVAL);
	NULL_CHECK(count, -EINVAL);

	if (strncmp(buf, "0", 1) == 0)
		enable = true;
	else if (strncmp(buf, "1", 1) == 0)
		enable = false;
	else {
		pr_err("[Diag] abnormal input value\n");
		return -EINVAL;
	}

	pr_info("[Diag] charging %s complete start\n",
			enable ? "not" : "");

	smbchg_usb_en(chip, enable ? 1 : 0, REASON_USER);
	if (chip->dc_psy_type != -EINVAL)
		smbchg_dc_en(chip, enable ? 1 : 0, REASON_USER);
	chip->chg_enabled = enable ? 1 : 0;
	schedule_work(&chip->usb_set_online_work);

	return 1;
}

static ssize_t at_pmic_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r = 0;
	bool pm_reset = true;
	struct smbchg_chip *chip = dev_get_drvdata(dev);

	NULL_CHECK(chip, -EINVAL);

	/* for waiting return values of testmode */
	msleep(3000);

	machine_restart(NULL);

	r = snprintf(buf, 3, "%d\n", pm_reset);

	return r;
}
DEVICE_ATTR(at_charge, 0644, at_chg_status_show, at_chg_status_store);
DEVICE_ATTR(at_chcomp, 0644, at_chg_complete_show, at_chg_complete_store);
DEVICE_ATTR(at_pmrst, 0440, at_pmic_reset_show, NULL);
#endif

static int determine_initial_status(struct smbchg_chip *chip)
{
#ifdef CONFIG_LGE_PM
	int rc, type;
	u8 reg = 0;
#endif
	/*
	 * It is okay to read the interrupt status here since
	 * interrupts aren't requested. reading interrupt status
	 * clears the interrupt so be careful to read interrupt
	 * status only in interrupt handling code
	 */
	batt_pres_handler(0, chip);
	batt_hot_handler(0, chip);
	batt_warm_handler(0, chip);
	batt_cool_handler(0, chip);
	batt_cold_handler(0, chip);
#ifdef CONFIG_LGE_PM
	batt_term_missing_handler(0, chip);
#endif
	chg_term_handler(0, chip);
	usbid_change_handler(0, chip);
	src_detect_handler(0, chip);

	chip->usb_present = is_usb_present(chip);
	chip->dc_present = is_dc_present(chip);

#ifdef CONFIG_LGE_PM
	if (chip->usb_present)
	{
		rc = smbchg_read(chip, &reg, chip->misc_base + IDEV_STS, 1);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
		type = get_type(reg);

		/* if apsd is not finished, do insertion func after 1s */
		if (type == 4)
		{
			pr_smb(PR_LGE, "APSD is not finished. Do insertion func after 1s");
			schedule_delayed_work(&chip->usb_insert_work,
				msecs_to_jiffies(1000));
		} else {
			handle_usb_insertion(chip);
		}
	}
#else
	if (chip->usb_present)
		handle_usb_insertion(chip);
#endif
	else
		handle_usb_removal(chip);

#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	if (lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO)
		smbchg_monitor_batt_temp_queue(chip);
#endif

#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	if (chip->usb_present)
		chip->enable_polling = true;
#endif
	return 0;
}

static int prechg_time[] = {
	24,
	48,
	96,
	192,
};
static int chg_time[] = {
	192,
	384,
	768,
	1536,
};

enum bpd_type {
	BPD_TYPE_BAT_NONE,
	BPD_TYPE_BAT_ID,
	BPD_TYPE_BAT_THM,
	BPD_TYPE_BAT_THM_BAT_ID,
	BPD_TYPE_DEFAULT,
};

static const char * const bpd_label[] = {
	[BPD_TYPE_BAT_NONE]		= "bpd_none",
	[BPD_TYPE_BAT_ID]		= "bpd_id",
	[BPD_TYPE_BAT_THM]		= "bpd_thm",
	[BPD_TYPE_BAT_THM_BAT_ID]	= "bpd_thm_id",
};

static inline int get_bpd(const char *name)
{
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(bpd_label); i++) {
		if (strcmp(bpd_label[i], name) == 0)
			return i;
	}
	return -EINVAL;
}

#define CHGR_CFG1			0xFB
#define RECHG_THRESHOLD_SRC_BIT		BIT(1)
#define TERM_I_SRC_BIT			BIT(2)
#define TERM_SRC_FG			BIT(2)
#define CHGR_CFG2			0xFC
#define CHG_INHIB_CFG_REG		0xF7
#define CHG_INHIBIT_50MV_VAL		0x00
#define CHG_INHIBIT_100MV_VAL		0x01
#define CHG_INHIBIT_200MV_VAL		0x02
#define CHG_INHIBIT_300MV_VAL		0x03
#define CHG_INHIBIT_MASK		0x03
#define USE_REGISTER_FOR_CURRENT	BIT(2)
#define CHG_EN_SRC_BIT			BIT(7)
#define CHG_EN_COMMAND_BIT		BIT(6)
#define P2F_CHG_TRAN			BIT(5)
#define I_TERM_BIT			BIT(3)
#define AUTO_RECHG_BIT			BIT(2)
#define CHARGER_INHIBIT_BIT		BIT(0)
#define CFG_TCC_REG			0xF9
#define CHG_ITERM_50MA			0x1
#define CHG_ITERM_100MA			0x2
#define CHG_ITERM_150MA			0x3
#define CHG_ITERM_200MA			0x4
#define CHG_ITERM_250MA			0x5
#define CHG_ITERM_300MA			0x0
#define CHG_ITERM_500MA			0x6
#define CHG_ITERM_600MA			0x7
#define CHG_ITERM_MASK			SMB_MASK(2, 0)
#define USB51_COMMAND_POL		BIT(2)
#define USB51AC_CTRL			BIT(1)
#define SFT_CFG				0xFD
#define TR_8OR32B			0xFE
#define BUCK_8_16_FREQ_BIT		BIT(0)
#define SFT_EN_MASK			SMB_MASK(5, 4)
#define SFT_TO_MASK			SMB_MASK(3, 2)
#define PRECHG_SFT_TO_MASK		SMB_MASK(1, 0)
#define SFT_TIMER_DISABLE_BIT		BIT(5)
#define PRECHG_SFT_TIMER_DISABLE_BIT	BIT(4)
#define SAFETY_TIME_MINUTES_SHIFT	2
#define BM_CFG				0xF3
#define BATT_MISSING_ALGO_BIT		BIT(2)
#define BMD_PIN_SRC_MASK		SMB_MASK(1, 0)
#define PIN_SRC_SHIFT			0
#define CHGR_CFG			0xFF
#define RCHG_LVL_BIT			BIT(0)
#ifdef CONFIG_LGE_PM_COMMON
#define CFG_AFVC			0xF6 //0xF5 correct config address
#else
#define CFG_AFVC			0xF5
#endif
#define VFLOAT_COMP_ENABLE_MASK		SMB_MASK(2, 0)
#define TR_RID_REG			0xFA
#define FG_INPUT_FET_DELAY_BIT		BIT(3)
#define TRIM_OPTIONS_7_0		0xF6
#define INPUT_MISSING_POLLER_EN_BIT	BIT(3)

#ifdef CONFIG_LGE_PM_COMMON
#define CFG_JEITA			0xFA
#define JEITA_ENABLE_MASK		SMB_MASK(5,0)
#define BATT_MISSING_POLLER		BIT(3)
#define STANDBY_OSC_TRIM		BIT(7)
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
#define HVDCP_EN_BIT			BIT(3)
#endif
#endif
#define AICL_WL_SEL_CFG			0xF5
#define AICL_WL_SEL_MASK		SMB_MASK(1, 0)
#define AICL_WL_SEL_45S		0
static int smbchg_hw_init(struct smbchg_chip *chip)
{
	int rc, i;
	u8 reg, mask;

	rc = smbchg_read(chip, chip->revision,
			chip->misc_base + REVISION1_REG, 4);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read revision rc=%d\n",
				rc);
		return rc;
	}
#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "Charger Revision DIG: %d.%d; ANA: %d.%d\n",
			chip->revision[DIG_MAJOR], chip->revision[DIG_MINOR],
			chip->revision[ANA_MAJOR], chip->revision[ANA_MINOR]);
#else
	pr_smb(PR_STATUS, "Charger Revision DIG: %d.%d; ANA: %d.%d\n",
			chip->revision[DIG_MAJOR], chip->revision[DIG_MINOR],
			chip->revision[ANA_MAJOR], chip->revision[ANA_MINOR]);
#endif

	rc = smbchg_sec_masked_write(chip,
			chip->dc_chgpth_base + AICL_WL_SEL_CFG,
			AICL_WL_SEL_MASK, AICL_WL_SEL_45S);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set AICL rerun timer rc=%d\n",
				rc);
		return rc;
	}

	rc = smbchg_sec_masked_write(chip, chip->usb_chgpth_base + TR_RID_REG,
			FG_INPUT_FET_DELAY_BIT, FG_INPUT_FET_DELAY_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't disable fg input fet delay rc=%d\n",
				rc);
		return rc;
	}

	rc = smbchg_sec_masked_write(chip, chip->misc_base + TRIM_OPTIONS_7_0,
			INPUT_MISSING_POLLER_EN_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't disable input missing poller rc=%d\n",
				rc);
		return rc;
	}

	/*
	 * force using current from the register i.e. ignore auto
	 * power source detect (APSD) mA ratings
	 */
	reg = USE_REGISTER_FOR_CURRENT;

	rc = smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
			USE_REGISTER_FOR_CURRENT, USE_REGISTER_FOR_CURRENT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set input limit cmd rc=%d\n", rc);
		return rc;
	}

	/*
	 * set chg en by cmd register, set chg en by writing bit 1,
	 * enable auto pre to fast, enable auto recharge by default.
	 * enable current termination and charge inhibition based on
	 * the device tree configuration.
	 */

#ifdef CONFIG_LGE_PM
	//CHARGER_INHIBIT_BIT Disable for battery missing detection
	rc = smbchg_sec_masked_write(chip, chip->chgr_base + CHGR_CFG2,
			CHG_EN_SRC_BIT | CHG_EN_COMMAND_BIT | P2F_CHG_TRAN
			| I_TERM_BIT | AUTO_RECHG_BIT | CHARGER_INHIBIT_BIT,
			CHG_EN_COMMAND_BIT
			| (chip->iterm_disabled ? I_TERM_BIT : 0));
#else
	rc = smbchg_sec_masked_write(chip, chip->chgr_base + CHGR_CFG2,
			CHG_EN_SRC_BIT | CHG_EN_COMMAND_BIT | P2F_CHG_TRAN
			| I_TERM_BIT | AUTO_RECHG_BIT | CHARGER_INHIBIT_BIT,
			CHG_EN_COMMAND_BIT
			| (chip->chg_inhibit_en ? CHARGER_INHIBIT_BIT : 0)
			| (chip->iterm_disabled ? I_TERM_BIT : 0));
#endif
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set chgr_cfg2 rc=%d\n", rc);
		return rc;
	}

#ifdef CONFIG_BATTERY_MAX17048
	rc = smbchg_sec_masked_write(chip, chip->misc_base + TRIM_OPTIONS_7_0,
			STANDBY_OSC_TRIM, STANDBY_OSC_TRIM);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't disable ESR adjustment during Taper rc=%d\n",
				rc);
		return rc;
	}
#endif

	/*
	 * Based on the configuration, use the analog sensors or the fuelgauge
	 * adc for recharge threshold source.
	 */

	if (chip->chg_inhibit_source_fg)
		rc = smbchg_sec_masked_write(chip, chip->chgr_base + CHGR_CFG1,
			TERM_I_SRC_BIT | RECHG_THRESHOLD_SRC_BIT,
			TERM_SRC_FG | RECHG_THRESHOLD_SRC_BIT);
	else
		rc = smbchg_sec_masked_write(chip, chip->chgr_base + CHGR_CFG1,
			TERM_I_SRC_BIT | RECHG_THRESHOLD_SRC_BIT, 0);

	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set chgr_cfg2 rc=%d\n", rc);
		return rc;
	}

	/*
	 * control USB suspend via command bits and set correct 100/500mA
	 * polarity on the usb current
	 */
	rc = smbchg_sec_masked_write(chip, chip->usb_chgpth_base + CHGPTH_CFG,
		USBIN_SUSPEND_SRC_BIT | USB51_COMMAND_POL | USB51AC_CTRL,
		(chip->charge_unknown_battery ? 0 : USBIN_SUSPEND_SRC_BIT));

	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set usb_chgpth cfg rc=%d\n", rc);
		return rc;
	}

	/* set the float voltage */
	if (chip->vfloat_mv != -EINVAL) {
		rc = smbchg_float_voltage_set(chip, chip->vfloat_mv);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set float voltage rc = %d\n", rc);
			return rc;
		}
		pr_smb(PR_STATUS, "set vfloat to %d\n", chip->vfloat_mv);
	}

	/* set iterm */
	if (chip->iterm_ma != -EINVAL) {
		if (chip->iterm_disabled) {
			dev_err(chip->dev, "Error: Both iterm_disabled and iterm_ma set\n");
			return -EINVAL;
		} else {
			if (chip->iterm_ma <= 50)
				reg = CHG_ITERM_50MA;
			else if (chip->iterm_ma <= 100)
				reg = CHG_ITERM_100MA;
			else if (chip->iterm_ma <= 150)
				reg = CHG_ITERM_150MA;
			else if (chip->iterm_ma <= 200)
				reg = CHG_ITERM_200MA;
			else if (chip->iterm_ma <= 250)
				reg = CHG_ITERM_250MA;
			else if (chip->iterm_ma <= 300)
				reg = CHG_ITERM_300MA;
			else if (chip->iterm_ma <= 500)
				reg = CHG_ITERM_500MA;
			else
				reg = CHG_ITERM_600MA;

			rc = smbchg_sec_masked_write(chip,
					chip->chgr_base + CFG_TCC_REG,
					CHG_ITERM_MASK, reg);
			if (rc) {
				dev_err(chip->dev,
					"Couldn't set iterm rc = %d\n", rc);
				return rc;
			}
			pr_smb(PR_STATUS, "set tcc (%d) to 0x%02x\n",
					chip->iterm_ma, reg);
		}
	}

	/* set the safety time voltage */
	if (chip->safety_time != -EINVAL) {
		reg = (chip->safety_time > 0 ? 0 : SFT_TIMER_DISABLE_BIT) |
			(chip->prechg_safety_time > 0
			? 0 : PRECHG_SFT_TIMER_DISABLE_BIT);

		for (i = 0; i < ARRAY_SIZE(chg_time); i++) {
			if (chip->safety_time <= chg_time[i]) {
				reg |= i << SAFETY_TIME_MINUTES_SHIFT;
				break;
			}
		}
		for (i = 0; i < ARRAY_SIZE(prechg_time); i++) {
			if (chip->prechg_safety_time <= prechg_time[i]) {
				reg |= i;
				break;
			}
		}

		rc = smbchg_sec_masked_write(chip,
				chip->chgr_base + SFT_CFG,
				SFT_EN_MASK | SFT_TO_MASK |
				(chip->prechg_safety_time > 0
				? PRECHG_SFT_TO_MASK : 0), reg);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set safety timer rc = %d\n",
				rc);
			return rc;
		}
	}

	/* make the buck switch faster to prevent some vbus oscillation */
	rc = smbchg_sec_masked_write(chip,
			chip->usb_chgpth_base + TR_8OR32B,
			BUCK_8_16_FREQ_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set buck frequency rc = %d\n", rc);
		return rc;
	}

	/* battery missing detection */
	mask =  BATT_MISSING_ALGO_BIT;
#ifdef CONFIG_LGE_PM_COMMON
	reg = chip->bmd_algo_disabled ? 0 : BATT_MISSING_ALGO_BIT;
	mask |=  BATT_MISSING_POLLER;
	reg |= BATT_MISSING_POLLER;
#else
	reg = chip->bmd_algo_disabled ? BATT_MISSING_ALGO_BIT : 0;
#endif
	if (chip->bmd_pin_src < BPD_TYPE_DEFAULT) {
		mask |= BMD_PIN_SRC_MASK;
		reg |= chip->bmd_pin_src << PIN_SRC_SHIFT;
	}
	rc = smbchg_sec_masked_write(chip,
			chip->bat_if_base + BM_CFG, mask, reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set batt_missing config = %d\n",
									rc);
		return rc;
	}

	smbchg_charging_status_change(chip);

	/*
	 * The charger needs 20 milliseconds to go into battery supplementary
	 * mode. Sleep here until we are sure it takes into effect.
	 */
	msleep(20);
	smbchg_usb_en(chip, chip->chg_enabled, REASON_USER);
#ifdef CONFIG_LGE_PM_COMMON
	if (chip->dc_psy_type != -EINVAL)
		smbchg_dc_en(chip, chip->chg_enabled, REASON_USER);
	else
		smbchg_dc_en(chip, false, REASON_USER);
#else
	smbchg_dc_en(chip, chip->chg_enabled, REASON_USER);
#endif
	/* resume threshold */
	if (chip->resume_delta_mv != -EINVAL) {
#ifdef CONFIG_LGE_PM
	/* CHARGER_INHIBIT_BIT Disable for battery missing detection */
#else
		/*
		 * Configure only if the recharge threshold source is not
		 * fuel gauge ADC.
		 */
		if (!chip->chg_inhibit_source_fg) {
			if (chip->resume_delta_mv < 100)
				reg = CHG_INHIBIT_50MV_VAL;
			else if (chip->resume_delta_mv < 200)
				reg = CHG_INHIBIT_100MV_VAL;
			else if (chip->resume_delta_mv < 300)
				reg = CHG_INHIBIT_200MV_VAL;
			else
				reg = CHG_INHIBIT_300MV_VAL;

			rc = smbchg_sec_masked_write(chip,
					chip->chgr_base + CHG_INHIB_CFG_REG,
					CHG_INHIBIT_MASK, reg);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't set inhibit val rc = %d\n",
						rc);
				return rc;
			}
		}
#endif
		rc = smbchg_sec_masked_write(chip,
				chip->chgr_base + CHGR_CFG,
				RCHG_LVL_BIT, (chip->resume_delta_mv < 200)
				? 0 : RCHG_LVL_BIT);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set recharge rc = %d\n",
					rc);
			return rc;
		}
	}

#ifdef CONFIG_LGE_PM
	if (chip->jeita_disabled) {
		rc = smbchg_sec_masked_write(chip, chip->chgr_base + CFG_JEITA,
				JEITA_ENABLE_MASK, 0x20);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't disable jeita func. rc = %d\n",rc);
		}
	}
	if (chip->aicl_enabled != -EINVAL) {
		if (is_factory_cable()) {
			/* set aicl disable in factory env. */
			chip->aicl_enabled = 0;
		}
		rc = smbchg_sec_masked_write(chip, chip->usb_chgpth_base + USB_AICL_CFG,
			AICL_EN_BIT, chip->aicl_enabled ? AICL_EN_BIT : 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't write aicl reg rc = %d\n",rc);
		}
		rc = smbchg_sec_masked_write(chip, chip->dc_chgpth_base + DC_AICL_CFG2,
			AICL_THRESHOLD_5V_4P4, chip->aicl_enabled ? AICL_THRESHOLD_5V_4P4 : 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't write reg for aicl threshold 5V. rc = %d\n",rc);
		}
		rc = smbchg_sec_masked_write(chip, chip->dc_chgpth_base + DC_AICL_CFG2,
			AICL_THRESHOLD_5V_TO_9V_4P4, chip->aicl_enabled ? AICL_THRESHOLD_5V_TO_9V_4P4 : 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't write reg for aicl threshold 5V to 9V. rc = %d\n",rc);
		}
	}
#endif

	/* DC path current settings */
	if (chip->dc_psy_type != -EINVAL) {
		rc = smbchg_set_thermal_limited_dc_current_max(chip,
						chip->dc_target_current_ma);
		if (rc < 0) {
			dev_err(chip->dev, "can't set dc current: %d\n", rc);
			return rc;
		}
	}


	/*
	 * on some devices the battery is powered via external sources which
	 * could raise its voltage above the float voltage. smbchargers go
	 * in to reverse boost in such a situation and the workaround is to
	 * disable float voltage compensation (note that the battery will appear
	 * hot/cold when powered via external source).
	 */
	if (chip->soft_vfloat_comp_disabled) {
		rc = smbchg_sec_masked_write(chip, chip->chgr_base + CFG_AFVC,
				VFLOAT_COMP_ENABLE_MASK, 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't disable soft vfloat rc = %d\n",
					rc);
			return rc;
		}
	}

#ifdef CONFIG_LGE_PM
	if (is_factory_cable()) {
		/* set charging current to 500mA */
		chip->target_fastchg_current_ma = FACTORY_FCC_DEFAULT;
	}
#endif
	rc = smbchg_set_fastchg_current(chip, chip->target_fastchg_current_ma);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set fastchg current = %d\n", rc);
		return rc;
	}

	rc = smbchg_read(chip, &chip->original_usbin_allowance,
			chip->usb_chgpth_base + USBIN_CHGR_CFG, 1);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't read usb allowance rc=%d\n", rc);

	if (chip->wipower_dyn_icl_avail) {
		rc = smbchg_wipower_ilim_config(chip,
				&(chip->wipower_default.entries[0]));
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set default wipower ilim = %d\n",
				rc);
			return rc;
		}
	}

	return rc;
}

static struct of_device_id smbchg_match_table[] = {
	{
		.compatible	= "qcom,qpnp-smbcharger",
		.data		= (void *)ARRAY_SIZE(usb_current_table),
	},
	{ },
};

#define DC_MA_MIN 300
#define DC_MA_MAX 1000
#define OF_PROP_READ(chip, prop, dt_property, retval, optional)		\
do {									\
	if (retval)							\
		break;							\
	if (optional)							\
		prop = -EINVAL;						\
									\
	retval = of_property_read_u32(chip->spmi->dev.of_node,		\
					"qcom," dt_property	,	\
					&prop);				\
									\
	if ((retval == -EINVAL) && optional)				\
		retval = 0;						\
	else if (retval)						\
		dev_err(chip->dev, "Error reading " #dt_property	\
				" property rc = %d\n", rc);		\
} while (0)

#define ILIM_ENTRIES		3
#define VOLTAGE_RANGE_ENTRIES	2
#define RANGE_ENTRY		(ILIM_ENTRIES + VOLTAGE_RANGE_ENTRIES)
static int smb_parse_wipower_map_dt(struct smbchg_chip *chip,
		struct ilim_map *map, char *property)
{
	struct device_node *node = chip->dev->of_node;
	int total_elements, size;
	struct property *prop;
	const __be32 *data;
	int num, i;

	prop = of_find_property(node, property, &size);
	if (!prop) {
		dev_err(chip->dev, "%s missing\n", property);
		return -EINVAL;
	}

	total_elements = size / sizeof(int);
	if (total_elements % RANGE_ENTRY) {
		dev_err(chip->dev, "%s table not in multiple of %d, total elements = %d\n",
				property, RANGE_ENTRY, total_elements);
		return -EINVAL;
	}

	data = prop->value;
	num = total_elements / RANGE_ENTRY;
	map->entries = devm_kzalloc(chip->dev,
			num * sizeof(struct ilim_entry), GFP_KERNEL);
	if (!map->entries) {
		dev_err(chip->dev, "kzalloc failed for default ilim\n");
		return -ENOMEM;
	}
	for (i = 0; i < num; i++) {
		map->entries[i].vmin_uv =  be32_to_cpup(data++);
		map->entries[i].vmax_uv =  be32_to_cpup(data++);
		map->entries[i].icl_pt_ma =  be32_to_cpup(data++);
		map->entries[i].icl_lv_ma =  be32_to_cpup(data++);
		map->entries[i].icl_hv_ma =  be32_to_cpup(data++);
	}
	map->num = num;
	return 0;
}

static int smb_parse_wipower_dt(struct smbchg_chip *chip)
{
	int rc = 0;

	chip->wipower_dyn_icl_avail = false;

	if (!chip->vadc_dev)
		goto err;

	rc = smb_parse_wipower_map_dt(chip, &chip->wipower_default,
					"qcom,wipower-default-ilim-map");
	if (rc) {
		dev_err(chip->dev, "failed to parse wipower-pt-ilim-map rc = %d\n",
				rc);
		goto err;
	}

	rc = smb_parse_wipower_map_dt(chip, &chip->wipower_pt,
					"qcom,wipower-pt-ilim-map");
	if (rc) {
		dev_err(chip->dev, "failed to parse wipower-pt-ilim-map rc = %d\n",
				rc);
		goto err;
	}

	rc = smb_parse_wipower_map_dt(chip, &chip->wipower_div2,
					"qcom,wipower-div2-ilim-map");
	if (rc) {
		dev_err(chip->dev, "failed to parse wipower-div2-ilim-map rc = %d\n",
				rc);
		goto err;
	}
	chip->wipower_dyn_icl_avail = true;
	return 0;
err:
	chip->wipower_default.num = 0;
	chip->wipower_pt.num = 0;
	chip->wipower_default.num = 0;
	if (chip->wipower_default.entries)
		devm_kfree(chip->dev, chip->wipower_default.entries);
	if (chip->wipower_pt.entries)
		devm_kfree(chip->dev, chip->wipower_pt.entries);
	if (chip->wipower_div2.entries)
		devm_kfree(chip->dev, chip->wipower_div2.entries);
	chip->wipower_default.entries = NULL;
	chip->wipower_pt.entries = NULL;
	chip->wipower_div2.entries = NULL;
	chip->vadc_dev = NULL;
	return rc;
}

static int smb_parse_dt(struct smbchg_chip *chip)
{
	int rc = 0;
	struct device_node *node = chip->dev->of_node;
	const char *dc_psy_type, *bpd;

	if (!node) {
		dev_err(chip->dev, "device tree info. missing\n");
		return -EINVAL;
	}

	/* read optional u32 properties */
	OF_PROP_READ(chip, chip->iterm_ma, "iterm-ma", rc, 1);
	OF_PROP_READ(chip, chip->target_fastchg_current_ma,
			"fastchg-current-ma", rc, 1);

#if defined(CONFIG_LGE_PM_BATTERY_ID_CHECKER)
	if ( chip->batt_id == BATT_ID_DS2704_L  ||
		chip->batt_id == BATT_ID_DS2704_C ||
		chip->batt_id == BATT_ID_ISL6296_L ||
		chip->batt_id == BATT_ID_ISL6296_C )
		OF_PROP_READ(chip, chip->vfloat_mv, "float-voltage-mv-normal-batt", rc, 1);
	else if (chip->batt_id == BATT_ID_RA4301_VC0 ||
		chip->batt_id == BATT_ID_RA4301_VC1 ||
		chip->batt_id == BATT_ID_SW3800_VC0 ||
		chip->batt_id == BATT_ID_SW3800_VC1)
		OF_PROP_READ(chip, chip->vfloat_mv, "float-voltage-mv-high-batt", rc, 1);
	else
		OF_PROP_READ(chip, chip->vfloat_mv, "float-voltage-mv", rc, 1);
	pr_err("vfloat_mv %d .\n", chip->vfloat_mv);

#else
	OF_PROP_READ(chip, chip->vfloat_mv, "float-voltage-mv", rc, 1);
#endif
	OF_PROP_READ(chip, chip->safety_time, "charging-timeout-mins", rc, 1);
	OF_PROP_READ(chip, chip->rpara_uohm, "rparasitic-uohm", rc, 1);
	OF_PROP_READ(chip, chip->prechg_safety_time, "precharging-timeout-mins",
			rc, 1);
#ifdef CONFIG_LGE_PM
	chip->jeita_disabled = of_property_read_bool(node, "lge,jeita-disabled");
	rc = of_property_read_u32(node, "lge,aicl-en", &(chip->aicl_enabled));
	chip->target_vfloat_mv = chip->vfloat_mv;
#endif

	if (chip->safety_time != -EINVAL &&
		(chip->safety_time > chg_time[ARRAY_SIZE(chg_time) - 1])) {
		dev_err(chip->dev, "Bad charging-timeout-mins %d\n",
						chip->safety_time);
		return -EINVAL;
	}
	if (chip->prechg_safety_time != -EINVAL &&
		(chip->prechg_safety_time >
		 prechg_time[ARRAY_SIZE(prechg_time) - 1])) {
		dev_err(chip->dev, "Bad precharging-timeout-mins %d\n",
						chip->prechg_safety_time);
		return -EINVAL;
	}
	OF_PROP_READ(chip, chip->resume_delta_mv, "resume-delta-mv", rc, 1);
	OF_PROP_READ(chip, chip->parallel.min_current_thr_ma,
			"parallel-usb-min-current-ma", rc, 1);
	OF_PROP_READ(chip, chip->parallel.min_9v_current_thr_ma,
			"parallel-usb-9v-min-current-ma", rc, 1);
	OF_PROP_READ(chip, chip->parallel.allowed_lowering_ma,
			"parallel-allowed-lowering-ma", rc, 1);
	if (chip->parallel.min_current_thr_ma != -EINVAL
			&& chip->parallel.min_9v_current_thr_ma != -EINVAL)
		chip->parallel.avail = true;
	pr_smb(PR_STATUS, "parallel usb thr: %d, 9v thr: %d\n",
			chip->parallel.min_current_thr_ma,
			chip->parallel.min_9v_current_thr_ma);

	/* read boolean configuration properties */
	chip->use_vfloat_adjustments = of_property_read_bool(node,
						"qcom,autoadjust-vfloat");
	chip->bmd_algo_disabled = of_property_read_bool(node,
						"qcom,bmd-algo-disabled");
	chip->iterm_disabled = of_property_read_bool(node,
						"qcom,iterm-disabled");
	chip->soft_vfloat_comp_disabled = of_property_read_bool(node,
					"qcom,soft-vfloat-comp-disabled");
	chip->chg_enabled = !(of_property_read_bool(node,
						"qcom,charging-disabled"));
	chip->charge_unknown_battery = of_property_read_bool(node,
						"qcom,charge-unknown-battery");
	chip->chg_inhibit_en = of_property_read_bool(node,
					"qcom,chg-inhibit-en");
#ifdef CONFIG_LGE_PM
        //CHARGER_INHIBIT_BIT Disable for battery missing detection
        chip->chg_inhibit_source_fg = 0;
#else
	chip->chg_inhibit_source_fg = of_property_read_bool(node,
						"qcom,chg-inhibit-fg");
#endif
	/* parse the battery missing detection pin source */
	rc = of_property_read_string(chip->spmi->dev.of_node,
		"qcom,bmd-pin-src", &bpd);
	if (rc) {
		/* Select BAT_THM as default BPD scheme */
		chip->bmd_pin_src = BPD_TYPE_DEFAULT;
		rc = 0;
	} else {
		chip->bmd_pin_src = get_bpd(bpd);
		if (chip->bmd_pin_src < 0) {
			dev_err(chip->dev,
				"failed to determine bpd schema %d\n", rc);
			return rc;
		}
	}

	/* parse the dc power supply configuration */
	rc = of_property_read_string(node, "qcom,dc-psy-type", &dc_psy_type);
	if (rc) {
		chip->dc_psy_type = -EINVAL;
		rc = 0;
	} else {
		if (strcmp(dc_psy_type, "Mains") == 0)
			chip->dc_psy_type = POWER_SUPPLY_TYPE_MAINS;
		else if (strcmp(dc_psy_type, "Wireless") == 0)
			chip->dc_psy_type = POWER_SUPPLY_TYPE_WIRELESS;
		else if (strcmp(dc_psy_type, "Wipower") == 0)
			chip->dc_psy_type = POWER_SUPPLY_TYPE_WIPOWER;
	}
	if (chip->dc_psy_type != -EINVAL) {
		OF_PROP_READ(chip, chip->dc_target_current_ma,
				"dc-psy-ma", rc, 0);
		if (rc)
			return rc;
		if (chip->dc_target_current_ma < DC_MA_MIN
				|| chip->dc_target_current_ma > DC_MA_MAX) {
			dev_err(chip->dev, "Bad dc mA %d\n",
					chip->dc_target_current_ma);
			return -EINVAL;
		}
	}

	if (chip->dc_psy_type == POWER_SUPPLY_TYPE_WIPOWER)
		smb_parse_wipower_dt(chip);

	/* read the bms power supply name */
	rc = of_property_read_string(node, "qcom,bms-psy-name",
						&chip->bms_psy_name);
	if (rc)
		chip->bms_psy_name = NULL;

	/* read the bms power supply name */
	rc = of_property_read_string(node, "qcom,battery-psy-name",
						&chip->battery_psy_name);
	if (rc)
		chip->battery_psy_name = "battery";

	if (of_find_property(node, "qcom,thermal-mitigation",
					&chip->thermal_levels)) {
		chip->thermal_mitigation = devm_kzalloc(chip->dev,
			chip->thermal_levels,
			GFP_KERNEL);

		if (chip->thermal_mitigation == NULL) {
			dev_err(chip->dev, "thermal mitigation kzalloc() failed.\n");
			return -ENOMEM;
		}

		chip->thermal_levels /= sizeof(int);
		rc = of_property_read_u32_array(node,
				"qcom,thermal-mitigation",
				chip->thermal_mitigation, chip->thermal_levels);
		if (rc) {
			dev_err(chip->dev,
				"Couldn't read threm limits rc = %d\n", rc);
			return rc;
		}
	}

	return 0;
}

#define SUBTYPE_REG			0x5
#define SMBCHG_CHGR_SUBTYPE		0x1
#define SMBCHG_OTG_SUBTYPE		0x8
#define SMBCHG_BAT_IF_SUBTYPE		0x3
#define SMBCHG_USB_CHGPTH_SUBTYPE	0x4
#define SMBCHG_DC_CHGPTH_SUBTYPE	0x5
#define SMBCHG_MISC_SUBTYPE		0x7
#define REQUEST_IRQ(chip, resource, irq_num, irq_name, irq_handler, flags, rc)\
do {									\
	irq_num = spmi_get_irq_byname(chip->spmi,			\
					resource, irq_name);		\
	if (irq_num < 0) {						\
		dev_err(chip->dev, "Unable to get " irq_name " irq\n");	\
		return -ENXIO;						\
	}								\
	rc = devm_request_threaded_irq(chip->dev,			\
			irq_num, NULL, irq_handler, flags, irq_name,	\
			chip);						\
	if (rc < 0) {							\
		dev_err(chip->dev, "Unable to request " irq_name " irq: %d\n",\
				rc);					\
		return -ENXIO;						\
	}								\
} while (0)

static int smbchg_request_irqs(struct smbchg_chip *chip)
{
	int rc = 0;
	struct resource *resource;
	struct spmi_resource *spmi_resource;
	u8 subtype;
	struct spmi_device *spmi = chip->spmi;
	unsigned long flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
							| IRQF_ONESHOT;

	spmi_for_each_container_dev(spmi_resource, chip->spmi) {
		if (!spmi_resource) {
				dev_err(chip->dev, "spmi resource absent\n");
			return rc;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
						IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			dev_err(chip->dev, "node %s IO resource absent!\n",
				spmi->dev.of_node->full_name);
			return rc;
		}

		rc = smbchg_read(chip, &subtype,
				resource->start + SUBTYPE_REG, 1);
		if (rc) {
			dev_err(chip->dev, "Peripheral subtype read failed rc=%d\n",
					rc);
			return rc;
		}

		switch (subtype) {
		case SMBCHG_CHGR_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->chg_error_irq,
				"chg-error", chg_error_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->taper_irq,
				"chg-taper-thr", taper_handler,
				(IRQF_TRIGGER_RISING | IRQF_ONESHOT), rc);
			disable_irq_nosync(chip->taper_irq);
			REQUEST_IRQ(chip, spmi_resource, chip->chg_term_irq,
				"chg-tcc-thr", chg_term_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->recharge_irq,
				"chg-rechg-thr", recharge_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->fastchg_irq,
				"chg-p2f-thr", fastchg_handler, flags, rc);
			enable_irq_wake(chip->chg_term_irq);
			enable_irq_wake(chip->chg_error_irq);
			enable_irq_wake(chip->fastchg_irq);
			break;
		case SMBCHG_BAT_IF_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->batt_hot_irq,
				"batt-hot", batt_hot_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->batt_warm_irq,
				"batt-warm", batt_warm_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->batt_cool_irq,
				"batt-cool", batt_cool_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->batt_cold_irq,
				"batt-cold", batt_cold_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->batt_missing_irq,
				"batt-missing", batt_pres_handler, flags, rc);
#ifdef CONFIG_LGE_PM
			REQUEST_IRQ(chip, spmi_resource, chip->batt_term_missing_irq,
				"batt-term-missing", batt_term_missing_handler, flags, rc);
#endif
			REQUEST_IRQ(chip, spmi_resource, chip->vbat_low_irq,
				"batt-low", vbat_low_handler, flags, rc);
#ifdef CONFIG_LGE_PM
#else
			enable_irq_wake(chip->batt_hot_irq);
			enable_irq_wake(chip->batt_warm_irq);
			enable_irq_wake(chip->batt_cool_irq);
			enable_irq_wake(chip->batt_cold_irq);
			enable_irq_wake(chip->batt_missing_irq);
#endif
#ifdef CONFIG_LGE_PM
			enable_irq_wake(chip->batt_term_missing_irq);
#endif
			enable_irq_wake(chip->vbat_low_irq);
			break;
		case SMBCHG_USB_CHGPTH_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->usbin_uv_irq,
				"usbin-uv", usbin_uv_handler, flags, rc);
#ifdef CONFIG_LGE_PM
			REQUEST_IRQ(chip, spmi_resource, chip->usbin_ov_irq,
				"usbin-ov", usbin_ov_handler, flags, rc);
#endif
			REQUEST_IRQ(chip, spmi_resource, chip->src_detect_irq,
				"usbin-src-det",
				src_detect_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->otg_fail_irq,
				"otg-fail", otg_fail_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->otg_oc_irq,
				"otg-oc", otg_oc_handler,
				(IRQF_TRIGGER_RISING | IRQF_ONESHOT), rc);
			REQUEST_IRQ(chip, spmi_resource, chip->aicl_done_irq,
				"aicl-done",
#ifdef CONFIG_LGE_PM
				aicl_done_handler, (IRQF_TRIGGER_RISING|IRQF_ONESHOT), rc);
#else
				aicl_done_handler, flags, rc);
#endif
			REQUEST_IRQ(chip, spmi_resource,
				chip->usbid_change_irq, "usbid-change",
				usbid_change_handler,
				(IRQF_TRIGGER_FALLING | IRQF_ONESHOT), rc);
			enable_irq_wake(chip->usbin_uv_irq);
#ifdef CONFIG_LGE_PM
			enable_irq_wake(chip->usbin_ov_irq);
#endif
			enable_irq_wake(chip->src_detect_irq);
			enable_irq_wake(chip->otg_fail_irq);
			enable_irq_wake(chip->otg_oc_irq);
			enable_irq_wake(chip->usbid_change_irq);
			break;
		case SMBCHG_DC_CHGPTH_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->dcin_uv_irq,
				"dcin-uv", dcin_uv_handler, flags, rc);
			enable_irq_wake(chip->dcin_uv_irq);
			break;
		case SMBCHG_MISC_SUBTYPE:
#ifdef CONFIG_LGE_PM
#else
			REQUEST_IRQ(chip, spmi_resource, chip->power_ok_irq,
				"power-ok", power_ok_handler, flags, rc);
#endif
			REQUEST_IRQ(chip, spmi_resource, chip->chg_hot_irq,
				"temp-shutdown", chg_hot_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource,
				chip->safety_timeout_irq,
				"safety-timeout",
				safety_timeout_handler, flags, rc);
			enable_irq_wake(chip->chg_hot_irq);
			enable_irq_wake(chip->safety_timeout_irq);
			break;
		case SMBCHG_OTG_SUBTYPE:
			break;
		}
	}

	return rc;
}

#define REQUIRE_BASE(chip, base, rc)					\
do {									\
	if (!rc && !chip->base) {					\
		dev_err(chip->dev, "Missing " #base "\n");		\
		rc = -EINVAL;						\
	}								\
} while (0)

static int smbchg_parse_peripherals(struct smbchg_chip *chip)
{
	int rc = 0;
	struct resource *resource;
	struct spmi_resource *spmi_resource;
	u8 subtype;
	struct spmi_device *spmi = chip->spmi;

	spmi_for_each_container_dev(spmi_resource, chip->spmi) {
		if (!spmi_resource) {
				dev_err(chip->dev, "spmi resource absent\n");
			return rc;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
						IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			dev_err(chip->dev, "node %s IO resource absent!\n",
				spmi->dev.of_node->full_name);
			return rc;
		}

		rc = smbchg_read(chip, &subtype,
				resource->start + SUBTYPE_REG, 1);
		if (rc) {
			dev_err(chip->dev, "Peripheral subtype read failed rc=%d\n",
					rc);
			return rc;
		}

		switch (subtype) {
		case SMBCHG_CHGR_SUBTYPE:
			chip->chgr_base = resource->start;
			break;
		case SMBCHG_BAT_IF_SUBTYPE:
			chip->bat_if_base = resource->start;
			break;
		case SMBCHG_USB_CHGPTH_SUBTYPE:
			chip->usb_chgpth_base = resource->start;
			break;
		case SMBCHG_DC_CHGPTH_SUBTYPE:
			chip->dc_chgpth_base = resource->start;
			break;
		case SMBCHG_MISC_SUBTYPE:
			chip->misc_base = resource->start;
			break;
		case SMBCHG_OTG_SUBTYPE:
			chip->otg_base = resource->start;
			break;
		}
	}

	REQUIRE_BASE(chip, chgr_base, rc);
	REQUIRE_BASE(chip, bat_if_base, rc);
	REQUIRE_BASE(chip, usb_chgpth_base, rc);
	REQUIRE_BASE(chip, dc_chgpth_base, rc);
	REQUIRE_BASE(chip, misc_base, rc);

	return rc;
}

static inline void dump_reg(struct smbchg_chip *chip, u16 addr,
		const char *name)
{
	u8 reg;

	smbchg_read(chip, &reg, addr, 1);
	pr_smb(PR_DUMP, "%s - %04X = %02X\n", name, addr, reg);
}

/* dumps useful registers for debug */
static void dump_regs(struct smbchg_chip *chip)
{
	u16 addr;

	/* charger peripheral */
	for (addr = 0xB; addr <= 0x10; addr++)
		dump_reg(chip, chip->chgr_base + addr, "CHGR Status");
	for (addr = 0xF0; addr <= 0xFF; addr++)
		dump_reg(chip, chip->chgr_base + addr, "CHGR Config");
	/* battery interface peripheral */
	dump_reg(chip, chip->bat_if_base + RT_STS, "BAT_IF Status");
	dump_reg(chip, chip->bat_if_base + CMD_CHG_REG, "BAT_IF Command");
	for (addr = 0xF0; addr <= 0xFB; addr++)
		dump_reg(chip, chip->bat_if_base + addr, "BAT_IF Config");
	/* usb charge path peripheral */
	for (addr = 0x7; addr <= 0x10; addr++)
		dump_reg(chip, chip->usb_chgpth_base + addr, "USB Status");
	dump_reg(chip, chip->usb_chgpth_base + CMD_IL, "USB Command");
	for (addr = 0xF0; addr <= 0xF5; addr++)
		dump_reg(chip, chip->usb_chgpth_base + addr, "USB Config");
	/* dc charge path peripheral */
	dump_reg(chip, chip->dc_chgpth_base + RT_STS, "DC Status");
	for (addr = 0xF0; addr <= 0xF6; addr++)
		dump_reg(chip, chip->dc_chgpth_base + addr, "DC Config");
	/* misc peripheral */
	dump_reg(chip, chip->misc_base + IDEV_STS, "MISC Status");
	dump_reg(chip, chip->misc_base + RT_STS, "MISC Status");
	for (addr = 0xF0; addr <= 0xF3; addr++)
		dump_reg(chip, chip->misc_base + addr, "MISC CFG");
}

#ifdef CONFIG_LGE_PM_COMMON
static ssize_t reg_dump_sys_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        int r = 0;
        struct smbchg_chip *chip = dev_get_drvdata(dev);

        NULL_CHECK(chip, -EINVAL);

	dump_regs(chip);

        return r;
}
DEVICE_ATTR(reg_dump_sys, 0444, reg_dump_sys_show, NULL);
#endif

#ifdef CONFIG_LGE_PM
int lge_get_sbl_cable_type(void)
{
	int ret_cable_type = 0;
	unsigned int *p_cable_type = (unsigned int *)
		(smem_get_entry(SMEM_ID_VENDOR1, &cable_smem_size, 0, 0));

	if (p_cable_type)
		ret_cable_type = *p_cable_type;
	else
		ret_cable_type = 0;

	return ret_cable_type;
}
EXPORT_SYMBOL(lge_get_sbl_cable_type);
#endif

static int create_debugfs_entries(struct smbchg_chip *chip)
{
	struct dentry *ent;

	chip->debug_root = debugfs_create_dir("qpnp-smbcharger", NULL);
	if (!chip->debug_root) {
		dev_err(chip->dev, "Couldn't create debug dir\n");
		return -EINVAL;
	}

	ent = debugfs_create_file("force_dcin_icl_check",
				  S_IFREG | S_IWUSR | S_IRUGO,
				  chip->debug_root, chip,
				  &force_dcin_icl_ops);
	if (!ent) {
		dev_err(chip->dev,
			"Couldn't create force dcin icl check file\n");
		return -EINVAL;
	}
	return 0;
}

#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
void lge_get_battery_id(struct smbchg_chip *chip) {
	struct power_supply *psy;
	union power_supply_propval prop = {0,};
	int battery_id;

	psy= power_supply_get_by_name("battery_id");
	if (psy) {
		psy->get_property(psy, POWER_SUPPLY_PROP_BATTERY_ID_CHECKER, &prop);
		battery_id = prop.intval;
	} else {
		pr_info("battery_id not found. use default battey \n");
		battery_id = BATT_ID_DEFAULT;
	}

	if (battery_id == BATT_ID_DS2704_L ||
		battery_id == BATT_ID_DS2704_C ||
		battery_id == BATT_ID_ISL6296_L ||
		battery_id == BATT_ID_ISL6296_C||
		battery_id == BATT_ID_RA4301_VC0 ||
		battery_id == BATT_ID_RA4301_VC1 ||
		battery_id == BATT_ID_SW3800_VC0 ||
		battery_id == BATT_ID_SW3800_VC1)
		chip->batt_id_smem = 1;
	else
		chip->batt_id_smem = 0;

	chip->batt_id = battery_id;
	return;
}
#endif

#ifdef CONFIG_MACH_MSM8994_Z2
#define BATT_NOT_PRESENT 200 /*in case of battery not presence */
#endif
static int smbchg_probe(struct spmi_device *spmi)
{
	int rc;
	struct smbchg_chip *chip;
	struct power_supply *usb_psy;
	struct qpnp_vadc_chip *vadc_dev;


#ifdef CONFIG_LGE_PM
	unsigned int *p_cable_type = (unsigned int *)
		(smem_get_entry(SMEM_ID_VENDOR1, &cable_smem_size, 0, 0));
#ifdef CONFIG_MACH_MSM8994_Z2
	unsigned int smem_size = 0;
	unsigned int *batt_id = (unsigned int*)
		(smem_get_entry(SMEM_BATT_INFO, &smem_size, 0, 0));
#endif
	if (p_cable_type)
		cable_type = *p_cable_type;
	else
		cable_type = 0;

	if (cable_type == LT_CABLE_56K || cable_type == LT_CABLE_130K ||
					cable_type == LT_CABLE_910K)
		factory_mode = 1;
	 else
		factory_mode = 0;

#ifdef CONFIG_MACH_MSM8994_Z2
	if (smem_size != 0 && batt_id) {
		unsigned int _batt_id;
#ifdef CONFIG_LGE_PM_LOW_BATT_LIMIT
		_batt_id = (*batt_id>>8) & 0x00ff;
#else
		_batt_id = *batt_id;
#endif
		if (_batt_id == BATT_NOT_PRESENT)
			batt_smem_present = 0;
		else
			batt_smem_present = 1;
	}
	pr_smb(PR_LGE, "probe start! cable_type = %d factory_mode = %d batt_smem_present = %d\n",
		cable_type, factory_mode, batt_smem_present);
#else
	pr_smb(PR_LGE, "probe start! cable_type = %d factory_mode = %d\n",
		cable_type, factory_mode);
#endif
#endif

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_smb(PR_STATUS, "USB supply not found, deferring probe\n");
		return -EPROBE_DEFER;
	}

	if (of_find_property(spmi->dev.of_node, "qcom,dcin-vadc", NULL)) {
		vadc_dev = qpnp_get_vadc(&spmi->dev, "dcin");
		if (IS_ERR(vadc_dev)) {
			rc = PTR_ERR(vadc_dev);
			if (rc != -EPROBE_DEFER)
				dev_err(&spmi->dev, "Couldn't get vadc rc=%d\n",
						rc);
			return rc;
		}
	}

	chip = devm_kzalloc(&spmi->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	INIT_WORK(&chip->usb_set_online_work, smbchg_usb_update_online_work);
	INIT_WORK(&chip->batt_soc_work, smbchg_adjust_batt_soc_work);
#ifdef CONFIG_LGE_PM_COMMON
	INIT_DELAYED_WORK(&chip->usb_insert_work, smbchg_usb_insert_work);
#endif
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	wake_lock_init(&chip->lcs_wake_lock,
			WAKE_LOCK_SUSPEND, "LGE charging scenario");
	INIT_DELAYED_WORK(&chip->battemp_work, smbchg_monitor_batt_temp);
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	chip->chg_current_te = EVP_FCC_DEFAULT;
	chip->otp_ibat_current = EVP_FCC_DEFAULT;
	smbchg_thermal_fastchg_current_ma = EVP_FCC_DEFAULT;
#else
	chip->chg_current_te = chip->target_fastchg_current_ma;
	chip->otp_ibat_current = chip->target_fastchg_current_ma;
	smbchg_thermal_fastchg_current_ma = chip->target_fastchg_current_ma;
#endif
#endif
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	INIT_DELAYED_WORK(&chip->usb_remove_work, smbchg_usb_remove_work);
	INIT_DELAYED_WORK(&chip->iusb_change_work, smbchg_iusb_change_worker);
	chip->chg_state = false;
	chip->fb_state = true;
#endif
#ifdef CONFIG_LGE_PM_MAXIM_EVP_CONTROL
	INIT_DELAYED_WORK(&chip->enable_evp_chg_work, check_usbin_evp_chg);
#endif
#ifdef CONFIG_LGE_PM_DETECT_QC20
	INIT_DELAYED_WORK(&chip->detect_QC20_work, smbchg_usb_detect_QC20_work);
	INIT_DELAYED_WORK(&chip->enable_QC20_chg_work, smbchg_usb_enable_QC20_chg_work);
#endif
	INIT_DELAYED_WORK(&chip->parallel_en_work,
			smbchg_parallel_usb_en_work);
	INIT_DELAYED_WORK(&chip->vfloat_adjust_work, smbchg_vfloat_adjust_work);
#ifdef CONFIG_LGE_PM_CHARGE_INFO
	INIT_DELAYED_WORK(&chip->charging_inform_work, charging_information);
#endif
	chip->vadc_dev = vadc_dev;
	chip->spmi = spmi;
	chip->dev = &spmi->dev;
	chip->usb_psy = usb_psy;
	chip->fake_battery_soc = -EINVAL;
	chip->usb_online = -EINVAL;
#ifdef CONFIG_LGE_PM_COMMON
	chip->batfet_en = 1;
#endif
	dev_set_drvdata(&spmi->dev, chip);

	spin_lock_init(&chip->sec_access_lock);
	mutex_init(&chip->current_change_lock);
	mutex_init(&chip->usb_set_online_lock);
	mutex_init(&chip->usb_en_lock);
	mutex_init(&chip->dc_en_lock);
	mutex_init(&chip->parallel.lock);
	mutex_init(&chip->taper_irq_lock);
	mutex_init(&chip->pm_lock);
	mutex_init(&chip->wipower_config);

#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	lge_get_battery_id(chip);
#endif

	rc = smbchg_parse_peripherals(chip);
	if (rc) {
		dev_err(chip->dev, "Error parsing DT peripherals: %d\n", rc);
		return rc;
	}
	rc = smb_parse_dt(chip);
	if (rc < 0) {
		dev_err(&spmi->dev, "Unable to parse DT nodes: %d\n", rc);
		return rc;
	}

	rc = smbchg_regulator_init(chip);
	if (rc) {
		dev_err(&spmi->dev,
			"Couldn't initialize regulator rc=%d\n", rc);
		return rc;
	}

	rc = smbchg_hw_init(chip);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Unable to intialize hardware rc = %d\n", rc);
		goto free_regulator;
	}

	rc = determine_initial_status(chip);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Unable to determine init status rc = %d\n", rc);
		goto free_regulator;
	}

#ifdef CONFIG_LGE_PM_USB_ID
	get_cable_data_from_dt(spmi->dev.of_node);
#endif
#ifdef CONFIG_LGE_PM
	the_chip = chip;
        wake_lock_init(&chip->chg_wake_lock,
			WAKE_LOCK_SUSPEND, "charging_wake_lock");
#endif

#ifdef CONFIG_LGE_PM
	wake_lock_init(&chip->uevent_wake_lock, WAKE_LOCK_SUSPEND, "qpnp_chg_uevent");
#endif

	chip->batt_psy.name		= chip->battery_psy_name;
	chip->batt_psy.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.get_property	= smbchg_battery_get_property;
	chip->batt_psy.set_property	= smbchg_battery_set_property;
	chip->batt_psy.properties	= smbchg_battery_properties;
	chip->batt_psy.num_properties	= ARRAY_SIZE(smbchg_battery_properties);
	chip->batt_psy.external_power_changed = smbchg_external_power_changed;
	chip->batt_psy.property_is_writeable = smbchg_battery_is_writeable;

	rc = power_supply_register(chip->dev, &chip->batt_psy);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Unable to register batt_psy rc = %d\n", rc);
		goto free_regulator;
	}

	if (chip->dc_psy_type != -EINVAL) {
		chip->dc_psy.name		= "dc";
		chip->dc_psy.type		= chip->dc_psy_type;
		chip->dc_psy.get_property	= smbchg_dc_get_property;
		chip->dc_psy.set_property	= smbchg_dc_set_property;
		chip->dc_psy.property_is_writeable = smbchg_dc_is_writeable;
		chip->dc_psy.properties		= smbchg_dc_properties;
		chip->dc_psy.num_properties = ARRAY_SIZE(smbchg_dc_properties);
		rc = power_supply_register(chip->dev, &chip->dc_psy);
		if (rc < 0) {
			dev_err(&spmi->dev,
				"Unable to register dc_psy rc = %d\n", rc);
			goto unregister_batt_psy;
		}
	}
	chip->psy_registered = true;

	rc = smbchg_request_irqs(chip);
	if (rc < 0) {
		dev_err(&spmi->dev, "Unable to request irqs rc = %d\n", rc);
		goto unregister_dc_psy;
	}

	power_supply_set_present(chip->usb_psy, chip->usb_present);

#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
	rc = device_create_file(&spmi->dev, &dev_attr_at_charge);
	if (rc < 0) {
		pr_err("%s:File dev_attr_at_charge creation failed: %d\n",
				__func__, rc);
		rc = -ENODEV;
		goto err_at_charge;
	}

	rc = device_create_file(&spmi->dev, &dev_attr_at_chcomp);
	if (rc < 0) {
		pr_err("%s:File dev_attr_at_chcomp creation failed: %d\n",
				__func__, rc);
		rc = -ENODEV;
		goto err_at_chcomp;
	}

	rc = device_create_file(&spmi->dev, &dev_attr_at_pmrst);
	if (rc < 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, rc);
		rc = -ENODEV;
		goto err_at_pmrst;
	}
#endif

#ifdef CONFIG_LGE_PM_COMMON
	rc = device_create_file(&spmi->dev, &dev_attr_reg_dump_sys);
        if (rc < 0) {
                pr_err("%s:File dev_attr_reg_dump_sys creation failed: %d\n",
                                __func__, rc);

		device_remove_file(&spmi->dev, &dev_attr_reg_dump_sys);
        }
#endif
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	schedule_delayed_work(&chip->battemp_work, (60*HZ) / 2);
#endif
	dump_regs(chip);
	create_debugfs_entries(chip);
#ifdef CONFIG_LGE_PM
	pr_smb(PR_LGE, "successfully probed dc = %d usb = %d\n",
			chip->dc_present, chip->usb_present);
#else
	dev_info(chip->dev, "SMBCHG successfully probed batt=%d dc = %d usb = %d\n",
			get_prop_batt_present(chip),
			chip->dc_present, chip->usb_present);
#endif
	return 0;

#ifdef CONFIG_LGE_PM_FACTORY_TESTMODE
err_at_pmrst:
	device_remove_file(&spmi->dev, &dev_attr_at_chcomp);
err_at_chcomp:
	device_remove_file(&spmi->dev, &dev_attr_at_charge);
err_at_charge:
#endif
unregister_dc_psy:
	power_supply_unregister(&chip->dc_psy);
unregister_batt_psy:
	power_supply_unregister(&chip->batt_psy);
free_regulator:
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	wake_lock_destroy(&chip->lcs_wake_lock);
#endif
#ifdef CONFIG_LGE_PM
	wake_lock_destroy(&chip->chg_wake_lock);
#endif
	smbchg_regulator_deinit(chip);
	handle_usb_removal(chip);
#ifdef CONFIG_LGE_PM
	the_chip = NULL;
#endif
	return rc;
}

static int smbchg_remove(struct spmi_device *spmi)
{
	struct smbchg_chip *chip = dev_get_drvdata(&spmi->dev);

#ifdef CONFIG_LGE_PM
	wake_lock_destroy(&chip->chg_wake_lock);
#endif
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	wake_lock_destroy(&chip->lcs_wake_lock);
#endif
	debugfs_remove_recursive(chip->debug_root);

	if (chip->dc_psy_type != -EINVAL)
		power_supply_unregister(&chip->dc_psy);

	power_supply_unregister(&chip->batt_psy);
	smbchg_regulator_deinit(chip);
#ifdef CONFIG_LGE_PM
	the_chip = NULL;
#endif
	return 0;
}

#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
static int smbchg_resume(struct device *dev)
{
	struct smbchg_chip *chip = dev_get_drvdata(dev);
	NULL_CHECK(chip, -EINVAL);
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	cancel_delayed_work_sync(&chip->battemp_work);
	schedule_delayed_work(&chip->battemp_work, HZ*1);
#endif
	return 0;
}

static int smbchg_suspend(struct device *dev)
{
	struct smbchg_chip *chip = dev_get_drvdata(dev);
	NULL_CHECK(chip, -EINVAL);
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	cancel_delayed_work_sync(&chip->battemp_work);
#endif
#ifdef CONFIG_LGE_PM_PARALLEL_CHARGING
	cancel_delayed_work_sync(&chip->iusb_change_work);
#endif
	return 0;
}
#endif

static const struct dev_pm_ops smbchg_pm_ops = {
#ifdef CONFIG_LGE_PM_CHARGING_TEMP_SCENARIO
	.resume 	= smbchg_resume,
	.suspend	= smbchg_suspend,
#endif
};

MODULE_DEVICE_TABLE(spmi, smbchg_id);

static struct spmi_driver smbchg_driver = {
	.driver		= {
		.name		= "qpnp-smbcharger",
		.owner		= THIS_MODULE,
		.of_match_table	= smbchg_match_table,
		.pm		= &smbchg_pm_ops,
	},
	.probe		= smbchg_probe,
	.remove		= smbchg_remove,
};

static int __init smbchg_init(void)
{
	return spmi_driver_register(&smbchg_driver);
}

static void __exit smbchg_exit(void)
{
	return spmi_driver_unregister(&smbchg_driver);
}

module_init(smbchg_init);
module_exit(smbchg_exit);

MODULE_DESCRIPTION("QPNP SMB Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:qpnp-smbcharger");
