/*
 * Maxim MAX77819 Charger Driver
 *
 * Copyright (C) 2013 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define LOG_LEVEL  0
#define LOG_WORKER 0

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

/* for Regmap */
#include <linux/regmap.h>

/* for Device Tree */
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include <linux/power_supply.h>
#include <linux/mfd/max77819.h>
#include <linux/mfd/max77819-charger.h>
#include <linux/delay.h>

#include <linux/power/max14670.h>
#include <linux/board_lge.h>
#include <linux/mfd/d2260/pmic.h>

#define DRIVER_DESC    "MAX77819 Charger Driver"
#define DRIVER_NAME    MAX77819_CHARGER_NAME
#define DRIVER_VERSION MAX77819_DRIVER_VERSION".3-rc"
#define DRIVER_AUTHOR  "Gyungoh Yoo <jack.yoo@maximintegrated.com>"

#define IRQ_WORK_DELAY              0
#define IRQ_WORK_INTERVAL           msecs_to_jiffies(5000)
#define LOG_WORK_INTERVAL           msecs_to_jiffies(10000)

#define VERIFICATION_UNLOCK         0

/* Register map */
#define FLASH_EN					0x05
#define CHGINT                      0x30
#define CHGINTM1                    0x31
#define CHGINT1_AICLOTG             BIT (7)
#define CHGINT1_TOPOFF              BIT (6)
#define CHGINT1_OVP                 BIT (5)
#define CHGINT1_DC_UVP              BIT (4)
#define CHGINT1_CHG                 BIT (3)
#define CHGINT1_BAT                 BIT (2)
#define CHGINT1_THM                 BIT (1)
#define CHG_STAT                    0x32
#define DC_BATT_DTLS                0x33
#define DC_BATT_DTLS_DC_AICL        BIT (7)
#define DC_BATT_DTLS_DC_I           BIT (6)
#define DC_BATT_DTLS_DC_OVP         BIT (5)
#define DC_BATT_DTLS_DC_UVP         BIT (4)
#define DC_BATT_DTLS_BAT_DTLS       BITS(3,2)
#define DC_BATT_DTLS_BATDET_DTLS    BITS(1,0)
#define CHG_DTLS                    0x34
#define CHG_DTLS_THM_DTLS           BITS(7,5)
#define CHG_DTLS_TOPOFF_DTLS        BIT (4)
#define CHG_DTLS_CHG_DTLS           BITS(3,0)
#define BAT2SYS_DTLS                0x35
#define BAT2SOC_CTL                 0x36
#define CHGCTL1                     0x37
#define CHGCTL1_SFO_DEBOUNCE_TMR    BITS(7,6)
#define CHGCTL1_SFO_DEBOUNCE_EN     BIT (5)
#define CHGCTL1_THM_DIS             BIT (4)
#define CHGCTL1_JEITA_EN            BIT (3)
#define CHGCTL1_BUCK_EN             BIT (2)
#define CHGCTL1_CHGPROT             BITS(1,0)
#define FCHGCRNT                    0x38
#define FCHGCRNT_FCHGTIME           BITS(7,5)
#define FCHGCRNT_CHGCC              BITS(4,0)
#define TOPOFF                      0x39
#define TOPOFF_TOPOFFTIME           BITS(7,5)
#define TOPOFF_IFST2P8              BIT (4)
#define TOPOFF_ITOPOFF              BITS(2,0)
#define BATREG                      0x3A
#define BATREG_REGTEMP              BITS(7,6)
#define BATREG_CHGRSTRT             BIT (5)
#define BATREG_MBATREG              BITS(4,1)
#define BATREG_VICHG_GAIN           BIT (0)
#define DCCRNT                      0x3B
#define DCCRNT_DCILMT               BITS(5,0)
#define AICLCNTL                    0x3C
#define AICLCNTL_AICL_RESET         BIT (5)
#define AICLCNTL_AICL               BITS(4,1)
#define AICLCNTL_DCMON_DIS          BIT (0)
#define RBOOST_CTL1                 0x3D
#define CHGCTL2                     0x3E
#define CHGCTL2_DCILIM_EN           BIT (7)
#define CHGCTL2_PREQCUR             BITS(6,5)
#define CHGCTL2_CEN                 BIT (4)
#define CHGCTL2_QBATEN              BIT (3)
#define CHGCTL2_VSYSREG             BITS(2,0)
#define BATDET                      0x3F
#define USBCHGCTL                   0x40
#define USBCHGCTL_USB_SUSPEND       BIT (2)
#define MBATREGMAX                  0x41
#define CHGCCMAX                    0x42
#define RBOOST_CTL2                 0x43
#define CHGINT2                     0x44
#define CHGINTMSK2                  0x45
#define CHGINT2_DC_V                BIT (7)
#define CHGINT2_CHG_WDT             BIT (4)
#define CHGINT2_CHG_WDT_WRN         BIT (0)
#define CHG_WDTC                    0x46
#define CHG_WDT_CTL                 0x47
#define CHG_WDT_DTLS                0x48

#ifdef CONFIG_USB_ODIN_DRD
#include <linux/usb/odin_usb3.h>
#endif

static int usb_id = 0;
static int is_factory_cable = 0;

struct max77819_charger {
    struct mutex                           lock;
    struct max77819_dev                   *chip;
    struct max77819_io                    *io;
    struct device                         *dev;
    struct kobject                        *kobj;
    struct attribute_group                *attr_grp;
    struct max77819_charger_platform_data *pdata;
    int                                    irq;
    u8                                     irq1_saved;
    u8                                     irq2_saved;
    spinlock_t                             irq_lock;
    struct delayed_work                    irq_work;
    struct delayed_work                    log_work;
	struct dentry						  *dent;
    struct power_supply                   *psy_ext;
    struct power_supply                   *psy_coop; /* cooperating charger */
	struct power_supply                    ac;
	struct power_supply                    usb;
	struct power_supply                    wireless;
	bool                                   chg_enabled;
	enum power_supply_type                 dcin_type;
    bool                                   dev_enabled;
    bool                                   dev_initialized;
    int                                    current_limit_volatile;
    int                                    current_limit_permanent;
    int                                    charge_current_volatile;
    int                                    charge_current_permanent;
    int                                    present;
    int                                    health;
    int                                    status;
    int                                    charge_type;
    int                                    vichg;
#ifdef CONFIG_USB_ODIN_DRD
    int                                   usbphy_ready;
	unsigned int usb_init : 1;
	struct odin_usb3_otg *odin_otg;
#endif
	struct wake_lock                       chg_wake_lock;
};

/* [START] Test code for Thermal Engine */
struct pseudo_thermal_curr_ctrl_info_type pseudo_thermal_curr_ctrl_info = {
	.current_mA = 0,
};
/* [END] */

struct debug_reg {
	char  *name;
	u8  reg;
};

#define MAX77819_CHRG_DEBUG_REG(x) {#x, x##_REG}

static struct debug_reg max77819_chrg_debug_regs[] = {
	/* 0x30 */MAX77819_CHRG_DEBUG_REG(CHGINT),
	/* 0x31 */MAX77819_CHRG_DEBUG_REG(CHGINTM1),
	/* 0x32 */MAX77819_CHRG_DEBUG_REG(CHG_STAT),
	/* 0x33 */MAX77819_CHRG_DEBUG_REG(DC_BATT_DTLS),
	/* 0x34 */MAX77819_CHRG_DEBUG_REG(CHG_DTLS),
	/* 0x35 */MAX77819_CHRG_DEBUG_REG(BAT2SYS_DTLS),
	/* 0x36 */MAX77819_CHRG_DEBUG_REG(BAT2SOC_CTL),
	/* 0x37 */MAX77819_CHRG_DEBUG_REG(CHGCTL1),
	/* 0x38 */MAX77819_CHRG_DEBUG_REG(FCHGCRNT),
	/* 0x39 */MAX77819_CHRG_DEBUG_REG(TOPOFF),
	/* 0x3A */MAX77819_CHRG_DEBUG_REG(BATREG),
	/* 0x3B */MAX77819_CHRG_DEBUG_REG(DCCRNT),
	/* 0x3C */MAX77819_CHRG_DEBUG_REG(AICLCNTL),
	/* 0x3D */MAX77819_CHRG_DEBUG_REG(RBOOST_CTL1),
	/* 0x3E */MAX77819_CHRG_DEBUG_REG(CHGCTL2),
	/* 0x3F */MAX77819_CHRG_DEBUG_REG(BATDET),
	/* 0x40 */MAX77819_CHRG_DEBUG_REG(USBCHGCTL),
	/* 0x41 */MAX77819_CHRG_DEBUG_REG(MBATREGMAX),
	/* 0x42 */MAX77819_CHRG_DEBUG_REG(CHGCCMAX),
	/* 0x43 */MAX77819_CHRG_DEBUG_REG(RBOOST_CTL2),
	/* 0x44 */MAX77819_CHRG_DEBUG_REG(CHGINT2),
	/* 0x45 */MAX77819_CHRG_DEBUG_REG(CHGINTMSK2),
	/* 0x46 */MAX77819_CHRG_DEBUG_REG(CHG_WDTC),
	/* 0x47 */MAX77819_CHRG_DEBUG_REG(CHG_WDT_CTL),
	/* 0x48 */MAX77819_CHRG_DEBUG_REG(CHG_WDT_DTLS),
};


#define __lock(_me)    mutex_lock(&(_me)->lock)
#define __unlock(_me)  mutex_unlock(&(_me)->lock)

enum {
    BATDET_DTLS_CONTACT_BREAK       = 0b00,
    BATDET_DTLS_BATTERY_DETECTED_01 = 0b01,
    BATDET_DTLS_BATTERY_DETECTED_10 = 0b10,
    BATDET_DTLS_BATTERY_REMOVED     = 0b11,
};

static char *max77819_charger_batdet_details[] = {
    [BATDET_DTLS_CONTACT_BREAK]       = "contact break",
    [BATDET_DTLS_BATTERY_DETECTED_01] = "battery detected (01)",
    [BATDET_DTLS_BATTERY_DETECTED_10] = "battery detected (10)",
    [BATDET_DTLS_BATTERY_REMOVED]     = "battery removed",
};

enum {
    DC_UVP_INVALID = 0,
    DC_UVP_VALID   = 1,
};

static char *max77819_charger_dcuvp_details[] = {
    [DC_UVP_INVALID] = "VDC is invalid; VDC < VDC_UVLO",
    [DC_UVP_VALID]   = "VDC is valid; VDC > VDC_UVLO",
};

enum {
    DC_OVP_VALID   = 0,
    DC_OVP_INVALID = 1,
};

static char *max77819_charger_dcovp_details[] = {
    [DC_OVP_VALID]   = "VDC is valid; VDC < VDC_OVLO",
    [DC_OVP_INVALID] = "VDC is invalid; VDC > VDC_OVLO",
};

enum {
    DC_I_VALID   = 0,
    DC_I_INVALID = 1,
};

static char *max77819_charger_dci_details[] = {
    [DC_I_VALID]   = "IDC is valid; IDC < DCILMT",
    [DC_I_INVALID] = "IDC is invalid; IDC > DCILMT",
};

enum {
    DC_AICL_OK  = 0,
    DC_AICL_NOK = 1,
};

static char *max77819_charger_aicl_details[] = {
    [DC_AICL_OK]  = "VDC > AICL threshold",
    [DC_AICL_NOK] = "VDC < AICL threshold",
};

enum {
    BAT_DTLS_UVP     = 0b00,
    BAT_DTLS_TIMEOUT = 0b01,
    BAT_DTLS_OK      = 0b10,
    BAT_DTLS_OVP     = 0b11,
};

static char *max77819_charger_bat_details[] = {
    [BAT_DTLS_UVP]     = "battery voltage < 2.1V",
    [BAT_DTLS_TIMEOUT] = "timer fault",
    [BAT_DTLS_OK]      = "battery okay",
    [BAT_DTLS_OVP]     = "battery overvoltage",
};

enum {
    CHG_DTLS_DEAD_BATTERY     = 0b0000,
    CHG_DTLS_PRECHARGE        = 0b0001,
    CHG_DTLS_FASTCHARGE_CC    = 0b0010,
    CHG_DTLS_FASTCHARGE_CV    = 0b0011,
    CHG_DTLS_TOPOFF           = 0b0100,
    CHG_DTLS_DONE             = 0b0101,
    CHG_DTLS_TIMER_FAULT      = 0b0110,
    CHG_DTLS_TEMP_SUSPEND     = 0b0111,
    CHG_DTLS_OFF              = 0b1000,
    CHG_DTLS_THM_LOOP         = 0b1001,
    CHG_DTLS_TEMP_SHUTDOWN    = 0b1010,
    CHG_DTLS_BUCK             = 0b1011,
    CHG_DTLS_OTG_OVER_CURRENT = 0b1100,
    CHG_DTLS_USB_SUSPEND      = 0b1101,
};

static char *max77819_charger_chg_details[] = {
    [CHG_DTLS_DEAD_BATTERY] =
        "charger is in dead-battery region",
    [CHG_DTLS_PRECHARGE] =
        "charger is in precharge mode",
    [CHG_DTLS_FASTCHARGE_CC] =
        "charger is in fast-charge constant current mode",
    [CHG_DTLS_FASTCHARGE_CV] =
        "charger is in fast-charge constant voltage mode",
    [CHG_DTLS_TOPOFF] =
        "charger is in top-off mode",
    [CHG_DTLS_DONE] =
        "charger is in done mode",
    [CHG_DTLS_TIMER_FAULT] =
        "charger is in timer fault mode",
    [CHG_DTLS_TEMP_SUSPEND] =
        "charger is in temperature suspend mode",
    [CHG_DTLS_OFF] =
        "buck off, charger off",
    [CHG_DTLS_THM_LOOP] =
        "charger is operating with its thermal loop active",
    [CHG_DTLS_TEMP_SHUTDOWN] =
        "charger is off and junction temperature is > TSHDN",
    [CHG_DTLS_BUCK] =
        "buck on, charger off",
    [CHG_DTLS_OTG_OVER_CURRENT] =
        "charger OTG current limit is exceeded longer than debounce time",
    [CHG_DTLS_USB_SUSPEND] =
        "USB suspend",
};

enum {
    TOPOFF_NOT_REACHED = 0,
    TOPOFF_REACHED     = 1,
};

static char *max77819_charger_topoff_details[] = {
    [TOPOFF_NOT_REACHED] = "topoff is not reached",
    [TOPOFF_REACHED]     = "topoff is reached",
};

enum {
    THM_DTLS_LOW_TEMP_SUSPEND   = 0b001,
    THM_DTLS_LOW_TEMP_CHARGING  = 0b010,
    THM_DTLS_STD_TEMP_CHARGING  = 0b011,
    THM_DTLS_HIGH_TEMP_CHARGING = 0b100,
    THM_DTLS_HIGH_TEMP_SUSPEND  = 0b101,
};

static char *max77819_charger_thm_details[] = {
    [THM_DTLS_LOW_TEMP_SUSPEND]   = "cold; T < T1",
    [THM_DTLS_LOW_TEMP_CHARGING]  = "cool; T1 < T < T2",
    [THM_DTLS_STD_TEMP_CHARGING]  = "normal; T2 < T < T3",
    [THM_DTLS_HIGH_TEMP_CHARGING] = "warm; T3 < T < T4",
    [THM_DTLS_HIGH_TEMP_SUSPEND]  = "hot; T4 < T",
};

#define CHGINT1 CHGINT
#define max77819_charger_read_irq_status(_me, _irq_reg) \
        ({\
            u8 __irq_current = 0;\
            int __rc = max77819_read((_me)->io, _irq_reg, &__irq_current);\
            if (unlikely(IS_ERR_VALUE(__rc))) {\
                log_err(#_irq_reg" read error [%d]\n", __rc);\
                __irq_current = 0;\
            }\
            __irq_current;\
        })

enum {
    CFG_CHGPROT = 0,
    CFG_SFO_DEBOUNCE_TMR,
    CFG_SFO_DEBOUNCE_EN,
    CFG_THM_DIS,
    CFG_JEITA_EN,
    CFG_BUCK_EN,
    CFG_DCILIM_EN,
    CFG_PREQCUR,
    CFG_CEN,
    CFG_QBATEN,
    CFG_VSYSREG,
    CFG_DCILMT,
    CFG_FCHGTIME,
    CFG_CHGCC,
    CFG_AICL_RESET,
    CFG_AICL,
    CFG_DCMON_DIS,
    CFG_MBATREG,
    CFG_CHGRSTRT,
    CFG_TOPOFFTIME,
    CFG_ITOPOFF,
    CFG_USB_SUSPEND,
};

static struct max77819_bitdesc max77819_charger_cfg_bitdesc[] = {
    #define CFG_BITDESC(_cfg_bit, _cfg_reg) \
            [CFG_##_cfg_bit] = MAX77819_BITDESC(_cfg_reg, _cfg_reg##_##_cfg_bit)

    CFG_BITDESC(CHGPROT         , CHGCTL1 ),
    CFG_BITDESC(SFO_DEBOUNCE_TMR, CHGCTL1 ),
    CFG_BITDESC(SFO_DEBOUNCE_EN , CHGCTL1 ),
    CFG_BITDESC(THM_DIS         , CHGCTL1 ),
    CFG_BITDESC(JEITA_EN        , CHGCTL1 ),
    CFG_BITDESC(BUCK_EN         , CHGCTL1 ),
    CFG_BITDESC(DCILIM_EN       , CHGCTL2 ),
    CFG_BITDESC(PREQCUR         , CHGCTL2 ),
    CFG_BITDESC(CEN             , CHGCTL2 ),
    CFG_BITDESC(QBATEN          , CHGCTL2 ),
    CFG_BITDESC(VSYSREG         , CHGCTL2 ),
    CFG_BITDESC(DCILMT          , DCCRNT  ),
    CFG_BITDESC(FCHGTIME        , FCHGCRNT),
    CFG_BITDESC(CHGCC           , FCHGCRNT),
    CFG_BITDESC(AICL_RESET      , AICLCNTL),
    CFG_BITDESC(AICL            , AICLCNTL),
    CFG_BITDESC(DCMON_DIS       , AICLCNTL),
    CFG_BITDESC(MBATREG         , BATREG  ),
    CFG_BITDESC(CHGRSTRT        , BATREG  ),
    CFG_BITDESC(TOPOFFTIME      , TOPOFF  ),
    CFG_BITDESC(ITOPOFF         , TOPOFF  ),
    CFG_BITDESC(USB_SUSPEND     , USBCHGCTL),
};
#define __cfg_bitdesc(_cfg) (&max77819_charger_cfg_bitdesc[CFG_##_cfg])

#define PROTCMD_UNLOCK  3
#define PROTCMD_LOCK    0

static struct wakeup_source max77819_chg_lock;
static struct max77819_charger *global_charger = NULL;
static int get_curr_thermal = 0;
int odin_charger_enable = 0;

extern int d2260_adc_get_vichg(void);
extern int d2260_adc_get_usb_id(void);
extern void call_sysfs_fuel(void);

#ifdef CONFIG_USB_ODIN_DRD
static int max77819_get_usbphy_ready(void);
static void max77819_set_usbphy_ready(int value);
#endif

static __always_inline int max77819_charger_unlock (struct max77819_charger *me)
{
    int rc;

	if(is_factory_cable == 1)
		goto out;

    rc = max77819_write_bitdesc(me->io, __cfg_bitdesc(CHGPROT), PROTCMD_UNLOCK);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("failed to unlock [%d]\n", rc);
        goto out;
    }

#if VERIFICATION_UNLOCK
    do {
        u8 chgprot = 0;

        rc = max77819_read_bitdesc(me->io, __cfg_bitdesc(CHGPROT), &chgprot);
        if (unlikely(IS_ERR_VALUE(rc) || chgprot != PROTCMD_UNLOCK)) {
            log_err("access denied - CHGPROT %X [%d]\n", chgprot, rc);
            rc = -EACCES;
            goto out;
        }
    } while (0);
#endif /* VERIFICATION_UNLOCK */

out:
    return rc;
}

/* Lock Charger property */
static __always_inline int max77819_charger_lock (struct max77819_charger *me)
{
    int rc;

    rc = max77819_write_bitdesc(me->io, __cfg_bitdesc(CHGPROT), PROTCMD_LOCK);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("failed to lock [%d]\n", rc);
    }

    return rc;
}

#define max77819_charger_read_config(_me, _cfg, _val_ptr) \
        ({\
            int __rc = max77819_read_bitdesc((_me)->io, __cfg_bitdesc(_cfg),\
                _val_ptr);\
            if (unlikely(IS_ERR_VALUE(__rc))) {\
                log_err("read config "#_cfg" error [%d]\n", __rc);\
            } else {\
                log_vdbg("read config "#_cfg": %Xh\n", *(_val_ptr));\
            }\
            __rc;\
        })
#define max77819_charger_write_config(_me, _cfg, _val) \
        ({\
            int __rc = max77819_charger_unlock(_me);\
            if (likely(!IS_ERR_VALUE(__rc))) {\
                __rc = max77819_write_bitdesc((_me)->io, __cfg_bitdesc(_cfg),\
                    _val);\
                if (unlikely(IS_ERR_VALUE(__rc))) {\
                    log_err("write config "#_cfg" error [%d]\n", __rc);\
                } else {\
                    log_vdbg("write config "#_cfg": %Xh\n", _val);\
                }\
                max77819_charger_lock(_me);\
            }\
            __rc;\
        })

/* Set DC input current limit */
static __inline int max77819_charger_get_dcilmt (struct max77819_charger *me,
    int *uA)
{
    int rc;
    u8 dcilmt = 0;

    rc = max77819_charger_read_config(me, DCILMT, &dcilmt);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    if (unlikely(dcilmt >= 0x3F)) {
        *uA = MAX77819_CHARGER_CURRENT_UNLIMIT;
        log_vdbg("<get_dcilmt> no limit\n");
        goto out;
    }

    *uA = dcilmt < 0x03 ? 100000 :
          dcilmt < 0x35 ? (int)(dcilmt - 0x03) * 25000 +  275000 :
                          (int)(dcilmt - 0x35) * 37500 + 1537500;
    log_vdbg("<get_dcilmt> %Xh -> %duA\n", dcilmt, *uA);

out:
    return rc;
}

/* Set DC input current limit */
static int max77819_charger_set_dcilmt (struct max77819_charger *me, int uA)
{
    u8 dcilmt;

    if (unlikely(uA == MAX77819_CHARGER_CURRENT_UNLIMIT)) {
        dcilmt = 0x3F;
        log_vdbg("<set_dcilmt> no limit\n");
        goto out;
    }

    dcilmt = uA <  275000 ? 0x00 :
             uA < 1537500 ? DIV_ROUND_UP(uA -  275000, 25000) + 0x03 :
             uA < 1875000 ? DIV_ROUND_UP(uA - 1537500, 37500) + 0x35 : 0x3F;
    log_vdbg("<set_dcilmt> %duA -> %Xh\n", uA, dcilmt);

out:
    return max77819_charger_write_config(me, DCILMT, dcilmt);
}

static __inline int max77819_charger_get_enable (struct max77819_charger *me,
    int *en)
{
    int rc;
    u8 cen = 0;

    rc = max77819_charger_read_config(me, CEN, &cen);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    *en = !!cen;
    log_dbg("<get_enable> %s\n", cen ? "enabled" : "disabled");

out:
    return rc;
}

static int max77819_charger_set_enable (struct max77819_charger *me, int en)
{
	int rc = 0;
	u8 test = 0;

	log_info("%s : batt_temp_set_enable[%d]\n", __func__,en);

	log_dbg("Please activating this code when the real battery is inserted \n");

    log_dbg("<set_enable> %s\n", en ? "enabling" : "disabling");
    me->chg_enabled = !!en;
	if (en == 1)
		odin_charger_enable = 1;
	else
		odin_charger_enable = 0;
    rc =  max77819_charger_write_config(me, CEN, !!en);
	return rc;
}

int max77819_charger_set_enable_extern (int en)
{
	return max77819_charger_set_enable(global_charger, en);
}
EXPORT_SYMBOL(max77819_charger_set_enable_extern);

static int max77819_charger_set_usb_suspend (struct max77819_charger *me, int en)
{
	int rc = 0;
	u8 test = 0;

	printk("%s : batt_temp_set_enable[%d]\n", __func__,en);
    printk("<set_enable> %s\n", en ? "enabling" : "disabling");

    me->chg_enabled = !!en;
	if (en == 1)
		odin_charger_enable = 1;
	else
		odin_charger_enable = 0;

	rc = max77819_charger_write_config(me, CEN, !!en);
	rc = max77819_charger_write_config(me, USB_SUSPEND, !en);
	return rc;
}

static __inline int max77819_charger_get_chgcc (struct max77819_charger *me,
    int *uA)
{
    int rc;
    u8 dcilmt = 0;

    rc = max77819_charger_read_config(me, CHGCC, &dcilmt);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    *uA = dcilmt < 0x01 ? 0 :
          dcilmt < 0x1C ? (int)(dcilmt - 0x01) * 50000 +  250000 :
          dcilmt < 0x1F ? (int)(dcilmt - 0x1C) * 62500 + 1620000 : 1800000;
    log_vdbg("<get_chgcc> %Xh -> %duA\n", dcilmt, *uA);

out:
    return rc;
}

/* Get Fast-charging current limit */
static int max77819_charger_set_chgcc (struct max77819_charger *me, int uA)
{
    u8 chgcc;

    chgcc = uA <  250000 ? 0x00 :
            uA < 1620000 ? DIV_ROUND_UP(uA -  250000, 50000) + 0x01 :
            uA < 1800000 ? DIV_ROUND_UP(uA - 1620000, 62500) + 0x1C : 0x1F;
    log_vdbg("<set_chgcc> %duA -> %Xh\n", uA, chgcc);

    return max77819_charger_write_config(me, CHGCC, chgcc);
}

/* Set Fast-charging current limit */
static int max77819_charger_set_charge_current (struct max77819_charger *me,
    int limit_uA, int charge_uA)
{
    int rc;
    u8 charging_current ;

    #define DCILMT_MIN  100000
    #define DCILMT_MAX  1875000
    #define CHGCC_MIN   0
    #define CHGCC_MAX   1800000

    log_info("%s : batt_temp control [%d]uA\n", __func__,charge_uA);
    get_curr_thermal = charge_uA/1000;

    max77819_read(me->io,TOPOFF , &charging_current);
	log_info("charging current :0x%x\n",charging_current );
	if(charging_current == 0x13)
	{
        printk(" charging as 2.8A due to factory cable is inserted \n");
        max77819_write(me->io, TOPOFF,0x13);
        max77819_write(me->io, DCCRNT,0x3F);
		goto out;
    }
/* Skip set CC */
/*  max77819_charger_set_chgcc(me, CHGCC_MIN); */

    if (limit_uA == MAX77819_CHARGER_CURRENT_MAXIMUM) {
        limit_uA = DCILMT_MAX;
    } else if (limit_uA == MAX77819_CHARGER_CURRENT_MINIMUM) {
        limit_uA = DCILMT_MIN;
    } else if (limit_uA != MAX77819_CHARGER_CURRENT_UNLIMIT) {
        limit_uA  = max(DCILMT_MIN, limit_uA );
    }

    if (charge_uA == MAX77819_CHARGER_CURRENT_UNLIMIT ||
        charge_uA == MAX77819_CHARGER_CURRENT_MAXIMUM) {
        charge_uA = CHGCC_MAX;
    } else if (limit_uA == MAX77819_CHARGER_CURRENT_MINIMUM) {
        charge_uA = CHGCC_MIN;
    } else {
        charge_uA = max(CHGCC_MIN , charge_uA);
    }

    if (likely(limit_uA == MAX77819_CHARGER_CURRENT_UNLIMIT ||
               limit_uA >= charge_uA)) {
        rc = max77819_charger_set_dcilmt(me, limit_uA);
        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }

        if (likely(me->dev_enabled)) {
            rc = max77819_charger_set_chgcc(me, charge_uA);
        }

        goto out;
    }

    if (likely(me->dev_enabled)) {
        log_dbg("setting current %duA but limited up to %duA\n", charge_uA,
            limit_uA);
        if (likely(limit_uA >= CHGCC_MIN)) {
            rc = max77819_charger_set_chgcc(me, limit_uA);
        } else {
            log_warn("disabling charger ; charging current < %uA\n", CHGCC_MIN);
            rc = max77819_charger_set_enable(me, false);
        }

        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }
    }

    rc = max77819_charger_set_dcilmt(me, limit_uA);

out:
    return rc;
}

static bool max77819_charger_present_input (struct max77819_charger *me)
{
    u8 dc_uvp = 0;
    int rc;

    rc = max77819_read_reg_bit(me->io, DC_BATT_DTLS, DC_UVP, &dc_uvp);
    if (unlikely(IS_ERR_VALUE(rc))) {
        return false;
    }

    return (dc_uvp == DC_UVP_VALID);
}

unsigned int cable_type_return_val = 0;
static void max77819_charger_check_cable (struct max77819_charger *me)
{
    struct max77819_charger_platform_data *pdata = me->pdata;
#ifdef CONFIG_USB_ODIN_DRD
    u32 cabletype = BC_CABLETYPE_UNDEFINED;

    if (me->odin_otg) {
        cabletype = me->odin_otg->cabletype;
    } else {
        log_err("%s: odin_otg is not initialized\n", __func__);
        cabletype = BC_CABLETYPE_SDP;
    }

    usb_id = d2260_adc_get_usb_id();

    log_info("%s: bc_cabletype %d, usb id %d\n", __func__, cabletype, usb_id);
	if(me->usbphy_ready == 0 || cabletype != BC_CABLETYPE_UNDEFINED ) {
	/* Cable Type for battery temperature control driver */
	cable_type_return_val = cabletype;

	switch (cabletype) {
        case BC_CABLETYPE_SDP:
#if defined (CONFIG_SLIMPORT_ANX7808) || defined (CONFIG_SLIMPORT_ANX7812)
		case BC_CABLETYPE_SLIMPORT:
#endif
            me->dcin_type                = POWER_SUPPLY_TYPE_USB;
            me->current_limit_volatile   = pdata->current_limit_usb; /* 0.5A */
            me->charge_current_volatile  = pdata->current_limit_usb; /* 0.5A */
            break;
        case BC_CABLETYPE_DCP: //#2 , TA
			me->dcin_type                = POWER_SUPPLY_TYPE_MAINS;
            me->current_limit_volatile   = MAX77819_CHARGER_CURRENT_UNLIMIT;  /* UnLimit */
            me->charge_current_volatile  = pdata->current_limit_ac;           /* 1.5A */
            break;
        case BC_CABLETYPE_CDP: // #3 , 1.5A PC HOST
            me->dcin_type                = POWER_SUPPLY_TYPE_MAINS;
            me->current_limit_volatile   = MAX77819_CHARGER_CURRENT_UNLIMIT;
											/* UnLimit */
            me->charge_current_volatile  = pdata->current_limit_ac;  /* 1.5A */
            break;
        default:
            me->dcin_type                = POWER_SUPPLY_TYPE_UNKNOWN;
            me->current_limit_volatile   = pdata->current_limit_usb;          /* 0.5A */
           // me->current_limit_volatile   = MAX77819_CHARGER_CURRENT_MINIMUM;  /* 0.1A */
            me->charge_current_volatile  = pdata->current_limit_usb;          /* 0.5A */
            break;
		}

		me->usbphy_ready =  max77819_get_usbphy_ready();
		log_info("max77819 get usbphy ready :%d \n",me->usbphy_ready);

		/* Call Battery Temperature Control with Reset Status when plugged */
		if (me->usbphy_ready) {

		}
		call_sysfs_fuel();
	}
#endif

	if(wlc_is_plugged()){
		log_info("<<wireless charger connected>>\n");
		me->dcin_type                 = POWER_SUPPLY_TYPE_WIRELESS;
		me->current_limit_volatile    = pdata->current_limit_usb;
		me->charge_current_volatile  = pdata->current_limit_usb;
		set_wireless_charger_status(1);
	}

    log_info("%s: type %d, limit %dmA, cc %dmA\n", __func__, me->dcin_type,
            me->current_limit_volatile, me->charge_current_volatile);
}

int max77819_cable_type_return(void)
{
	return cable_type_return_val;
}
EXPORT_SYMBOL(max77819_cable_type_return);

static int max77819_charger_exit_dev (struct max77819_charger *me)
{
    struct max77819_charger_platform_data *pdata = me->pdata;

    max77819_charger_set_charge_current(me, me->current_limit_permanent, 0);
    max77819_charger_set_enable(me, false);

    me->current_limit_volatile  = me->current_limit_permanent;
    me->charge_current_volatile = me->charge_current_permanent;

    me->dcin_type       = POWER_SUPPLY_TYPE_UNKNOWN;
    me->dev_enabled     = (!pdata->enable_coop || pdata->coop_psy_name);
    me->dev_initialized = false;
    return 0;
}

static int max77819_charger_init_dev (struct max77819_charger *me)
{
    struct max77819_charger_platform_data *pdata = me->pdata;
    unsigned long irq_flags;
    int rc;
    u8 irq1_current, irq2_current, val;

    val  = 0;
//    val |= CHGINT1_AICLOTG;
    val |= CHGINT1_TOPOFF;
    val |= CHGINT1_DC_UVP;
    val |= CHGINT1_CHG;
/*	val |= CHGINT1_OVP;
*	val |= CHGINT1_BAT;
*	val |= CHGINT1_THM;
*/
    rc = max77819_write(me->io, CHGINTM1, ~val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("CHGINTM1 write error [%d]\n", rc);
        goto out;
    }

    val  = 0;
/*	val |= CHGINT2_DC_V;
*	val |= CHGINT2_CHG_WDT;
*	val |= CHGINT2_CHG_WDT_WRN;
*/
    rc = max77819_write(me->io, CHGINTMSK2, ~val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("CHGINTMSK2 write error [%d]\n", rc);
        goto out;
    }


    irq1_current = max77819_charger_read_irq_status(me, CHGINT1);
    irq2_current = max77819_charger_read_irq_status(me, CHGINT2);

    spin_lock_irqsave(&me->irq_lock, irq_flags);
    me->irq1_saved |= irq1_current;
    me->irq2_saved |= irq2_current;
    spin_unlock_irqrestore(&me->irq_lock, irq_flags);

    log_dbg("CHGINT1 CURR %02Xh SAVED %02Xh\n", irq1_current, me->irq1_saved);
    log_dbg("CHGINT2 CURR %02Xh SAVED %02Xh\n", irq2_current, me->irq2_saved);

    /* charger enable */
    rc = max77819_charger_set_enable(me, me->dev_enabled);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* DCILMT enable */
    rc = max77819_charger_write_config(me, DCILIM_EN, true);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* check charger type & current limit */
    max77819_charger_check_cable(me);

    /* charge current */
    rc = max77819_charger_set_charge_current(me, me->current_limit_volatile,
        me->charge_current_volatile);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }
	/* JEITA DISABLE */
	rc = max77819_charger_write_config(me , JEITA_EN , true);

    /* topoff timer */
    val = pdata->topoff_timer <=  0 ? 0x00 :
          pdata->topoff_timer <= 60 ?
            (int)DIV_ROUND_UP(pdata->topoff_timer, 10) : 0x07;
    rc = max77819_charger_write_config(me, TOPOFFTIME, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* topoff current */
    val = pdata->topoff_current <  50000 ? 0x00 :
          pdata->topoff_current < 400000 ?
            (int)DIV_ROUND_UP(pdata->topoff_current - 50000, 50000) : 0x07;
    rc = max77819_charger_write_config(me, ITOPOFF, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* charge restart threshold */
    val = (pdata->charge_restart_threshold > 150000);
    rc = max77819_charger_write_config(me, CHGRSTRT, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* charge termination voltage */
    val = pdata->charge_termination_voltage < 3700000 ? 0x00 :
        (int)DIV_ROUND_UP(pdata->charge_termination_voltage - 3700000, 50000)
        + 0x01;
    rc = max77819_charger_write_config(me, MBATREG, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* thermistor control */
    val = (pdata->enable_thermistor == false);
    rc = max77819_charger_write_config(me, THM_DIS, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    /* AICL control */

    val = (pdata->enable_aicl == false);
    rc = max77819_charger_write_config(me, DCMON_DIS, val);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    if (likely(pdata->enable_aicl)) {
        int uv;

        /* AICL detection voltage selection */

        uv = pdata->aicl_detection_voltage;
        val = uv < 3900000 ? 0x00 :
              uv < 4800000 ? (int)DIV_ROUND_UP(uv - 3900000, 100000) : 0x09;
        log_dbg("AICL detection voltage %uV (%Xh)\n", uv, val);

        rc = max77819_charger_write_config(me, AICL, val);
        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }

        /* AICL reset threshold */

        uv = (int)pdata->aicl_reset_threshold;
        val = (uv > 100000);
        log_dbg("AICL reset threshold %uV (%Xh)\n", uv, val);

        rc = max77819_charger_write_config(me, AICL_RESET, val);
        if (unlikely(IS_ERR_VALUE(rc))) {
            goto out;
        }
    }

    me->dev_initialized = true;
    log_dbg("device initialized\n");

out:
    return rc;
}

#ifdef CONFIG_USB_ODIN_DRD
static int usbphy_ready = 0;

int max77819_get_usbphy_ready(void)
{
	return usbphy_ready;
}

void max77819_set_usbphy_ready(int value)
{
	usb_dbg("[%s] value: %d\n", __func__, value);

	usbphy_ready = value;
}

void max77819_usb3phy_ready_set(enum bc_cabletype_odin_otg cabletype)
{
	switch(cabletype) {
		case BC_CABLETYPE_SDP:
		case BC_CABLETYPE_CDP:
		case BC_CABLETYPE_DCP:
			max77819_set_usbphy_ready(1);
			break;
		default:
			break;
	}
}

static void max77819_charger_usb_register(struct max77819_charger *me)
{
	struct usb_phy *phy = NULL;
	struct odin_usb3_otg *usb3_otg;

	usb_dbg("[%s] \n", __func__);

	phy = usb_get_phy(USB_PHY_TYPE_USB3);
	if (IS_ERR_OR_NULL(phy)) {
		return;
	}

	usb3_otg = phy_to_usb3otg(phy);
	me->odin_otg = usb3_otg;
	me->odin_otg->charger_dev = me->dev;
}

void max77819_drd_charger_init_detect(struct odin_usb3_otg *odin_otg)
{
	struct max77819_charger *me =
				dev_get_drvdata(odin_otg->charger_dev);
	u8 irq_current;
	int input = 0;

	irq_current = max77819_charger_read_irq_status(me, CHGINT1);
	log_dbg("<ISR> CHGINT1 CURR %02Xh SAVED %02Xh\n",
					irq_current, me->irq1_saved);
	me->irq1_saved |= irq_current;

	irq_current = max77819_charger_read_irq_status(me, CHGINT2);
	log_dbg("<ISR> CHGINT2 CURR %02Xh SAVED %02Xh\n",
					irq_current, me->irq2_saved);
	me->irq2_saved |= irq_current;

	input = max77819_charger_present_input(me);

	log_info("DC input %s\n", input ? "inserted" : "removed");
	odin_otg_vbus_event(me->odin_otg, input, 350);
	me->usb_init = 1;

	if (likely(input)) {
		max77819_charger_check_cable(me);
		log_info("start charging\n");
		max77819_charger_init_dev(me);
	}
}

#define RBOOST_CTL1_RBOOSTEN BIT(0)
#define RBOOST_CTL1_RBFORCEPWM BIT(4)
#define RBOOST_CTL1_BSTSLEWRATE BIT(7,5)
#define BATTOSOC_CTL_OTG_EN BIT(5)

void max77819_otg_vbus_onoff(struct odin_usb3_otg *odin_otg, int onoff)
{
	struct max77819_charger *me =
				dev_get_drvdata(odin_otg->charger_dev);
	u8 rboost_ctl1, rboost_ctl2, bat2soc_ctl;

	if (onoff)
	{
		max77819_write(me->io, CHGINTM1, 0xFF);

		max77819_read(me->io, BAT2SOC_CTL, &bat2soc_ctl);
		bat2soc_ctl |= BATTOSOC_CTL_OTG_EN;
		max77819_write(me->io, BAT2SOC_CTL, bat2soc_ctl);

		max77819_read(me->io, RBOOST_CTL1, &rboost_ctl1);
		rboost_ctl1 = RBOOST_CTL1_RBOOSTEN;
		max77819_write(me->io, RBOOST_CTL1, rboost_ctl1);

		max77819_write(me->io, RBOOST_CTL2, 0x50);
	} else {
		max77819_write(me->io, RBOOST_CTL2, 0x00);

		max77819_read(me->io, RBOOST_CTL1, &rboost_ctl1);
		rboost_ctl1 = ~RBOOST_CTL1_RBOOSTEN;
		max77819_write(me->io, RBOOST_CTL1, rboost_ctl1);

		max77819_read(me->io, BAT2SOC_CTL, &bat2soc_ctl);
		bat2soc_ctl &= ~BATTOSOC_CTL_OTG_EN;
		max77819_write(me->io, BAT2SOC_CTL, bat2soc_ctl);

		max77819_write(me->io, CHGINTM1, (u8)~CHGINT1_DC_UVP);
	}
}
#endif	/* CONFIG_USB_ODIN_DRD */

#define max77819_charger_psy_setprop(_me, _psy, _psp, _val) \
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


/* Charger wake lock */
static void max77819_charger_wake_lock (struct max77819_charger *me, bool enable)
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

static void max77819_charger_psy_init (struct max77819_charger *me)
{
    if (unlikely(!me->psy_ext && me->pdata->ext_psy_name)) {
        me->psy_ext = power_supply_get_by_name(me->pdata->ext_psy_name);
        if (likely(me->psy_ext)) {
            log_dbg("psy %s found\n", me->pdata->ext_psy_name);
            max77819_charger_psy_setprop(me, psy_ext, PRESENT, false);
        }
    }

    if (unlikely(!me->psy_coop && me->pdata->coop_psy_name)) {
        me->psy_coop = power_supply_get_by_name(me->pdata->coop_psy_name);
        if (likely(me->psy_coop)) {
            log_dbg("psy %s found\n", me->pdata->coop_psy_name);
        }
    }
}

static void max77819_charger_psy_changed (struct max77819_charger *me)
{
    max77819_charger_psy_init(me);

    if (me->dcin_type == POWER_SUPPLY_TYPE_USB) {
        if (likely(&me->usb)) {
            power_supply_changed(&me->usb);
        }
    } else if (me->dcin_type == POWER_SUPPLY_TYPE_MAINS) {
        if (likely(&me->ac)) {
            power_supply_changed(&me->ac);
        }
    } else {
        if (likely(&me->usb)) {
            power_supply_changed(&me->usb);
        }
        if (likely(&me->ac)) {
            power_supply_changed(&me->ac);
        }
    }

    if (likely(me->psy_ext)) {
        power_supply_changed(me->psy_ext);
    }

    if (likely(me->psy_coop)) {
        power_supply_changed(me->psy_coop);
    }
}

struct max77819_charger_status_map {
    int health, status, charge_type;
};

static struct max77819_charger_status_map max77819_charger_status_map[] = {
    #define STATUS_MAP(_chg_dtls, _health, _status, _charge_type) \
            [CHG_DTLS_##_chg_dtls] = {\
                .health = POWER_SUPPLY_HEALTH_##_health,\
                .status = POWER_SUPPLY_STATUS_##_status,\
                .charge_type = POWER_SUPPLY_CHARGE_TYPE_##_charge_type,\
            }
    /*                           health               status   charge_type */
    STATUS_MAP(DEAD_BATTERY,     DEAD,                NOT_CHARGING, NONE),
    STATUS_MAP(PRECHARGE,        UNKNOWN,             CHARGING,     TRICKLE),
    STATUS_MAP(FASTCHARGE_CC,    UNKNOWN,             CHARGING,     FAST),
    STATUS_MAP(FASTCHARGE_CV,    UNKNOWN,             CHARGING,     FAST),
    STATUS_MAP(TOPOFF,           UNKNOWN,             CHARGING,     FAST),
    STATUS_MAP(DONE,             UNKNOWN,             FULL,         NONE),
    STATUS_MAP(TIMER_FAULT,      SAFETY_TIMER_EXPIRE, NOT_CHARGING, NONE),
    STATUS_MAP(TEMP_SUSPEND,     UNKNOWN,             NOT_CHARGING, NONE),
    STATUS_MAP(OFF,              UNKNOWN,             NOT_CHARGING, NONE),
    STATUS_MAP(THM_LOOP,         UNKNOWN,             CHARGING,     NONE),
    STATUS_MAP(TEMP_SHUTDOWN,    OVERHEAT,            NOT_CHARGING, NONE),
    STATUS_MAP(BUCK,             UNKNOWN,             NOT_CHARGING, UNKNOWN),
    STATUS_MAP(OTG_OVER_CURRENT, UNKNOWN,             NOT_CHARGING, UNKNOWN),
    STATUS_MAP(USB_SUSPEND,      UNKNOWN,             NOT_CHARGING, NONE),
};

static int max77819_charger_update (struct max77819_charger *me,
							enum power_supply_type psy_type)
{
    int rc;
    u8 dc_batt_dtls, chg_dtls;
    u8 batdet, bat, dcuvp, dcovp, dci, aicl;
    u8 chg, topoff, thm;

    me->health      = POWER_SUPPLY_HEALTH_UNKNOWN;
    me->status      = POWER_SUPPLY_STATUS_UNKNOWN;
    me->charge_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

    rc = max77819_read(me->io, DC_BATT_DTLS, &dc_batt_dtls);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("DC_BATT_DTLS read error [%d]\n", rc);
        goto out;
    }

    rc = max77819_read(me->io, CHG_DTLS, &chg_dtls);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("CHG_DTLS read error [%d]\n", rc);
        goto out;
    }

    log_vdbg("DC_BATT_DTLS %Xh CHG_DTLS %Xh\n", dc_batt_dtls, chg_dtls);

    batdet = BITS_GET(dc_batt_dtls, DC_BATT_DTLS_BATDET_DTLS);
    log_vdbg("*** BATDET %s\n", max77819_charger_batdet_details[batdet]);

    bat = BITS_GET(dc_batt_dtls, DC_BATT_DTLS_BAT_DTLS);
    log_vdbg("*** BAT    %s\n", max77819_charger_bat_details[bat]);

    dcuvp = BITS_GET(dc_batt_dtls, DC_BATT_DTLS_DC_UVP);
    log_vdbg("*** DC_UVP %s\n", max77819_charger_dcuvp_details[dcuvp]);

    dcovp = BITS_GET(dc_batt_dtls, DC_BATT_DTLS_DC_OVP);
    log_vdbg("*** DC_OVP %s\n", max77819_charger_dcovp_details[dcovp]);

    dci = BITS_GET(dc_batt_dtls, DC_BATT_DTLS_DC_I);
    log_vdbg("*** DC_I   %s\n", max77819_charger_dci_details[dci]);

    aicl = BITS_GET(dc_batt_dtls, DC_BATT_DTLS_DC_AICL);
    log_vdbg("*** AICL   %s\n", max77819_charger_aicl_details[aicl]);

    chg = BITS_GET(chg_dtls, CHG_DTLS_CHG_DTLS);
    log_vdbg("*** CHG    %s\n", max77819_charger_chg_details[chg]);

    topoff = BITS_GET(chg_dtls, CHG_DTLS_TOPOFF_DTLS);
    log_vdbg("*** TOPOFF %s\n", max77819_charger_topoff_details[topoff]);

    thm = BITS_GET(chg_dtls, CHG_DTLS_THM_DTLS);
    log_vdbg("*** THM    %s\n", max77819_charger_thm_details[thm]);

    me->present = (dcuvp == DC_UVP_VALID);

    if ((!me->present) || (!psy_type) || (me->dcin_type != psy_type)) {
        /* no charger present */
        me->health      = POWER_SUPPLY_HEALTH_UNKNOWN;
        me->status      = POWER_SUPPLY_STATUS_DISCHARGING;
        me->charge_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
        goto out;
    }

    if (unlikely(dcovp != DC_OVP_VALID)) {
        me->status      = POWER_SUPPLY_STATUS_NOT_CHARGING;
        me->health      = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
        me->charge_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
        goto out;
    }

    me->health      = max77819_charger_status_map[chg].health;
    me->status      = max77819_charger_status_map[chg].status;
    me->charge_type = max77819_charger_status_map[chg].charge_type;

    if (likely(me->health != POWER_SUPPLY_HEALTH_UNKNOWN)) {
        goto out;
    }

    /* override health by THM_DTLS */
    switch (thm) {
    case THM_DTLS_LOW_TEMP_SUSPEND:
        me->health = POWER_SUPPLY_HEALTH_COLD;
        break;
    case THM_DTLS_HIGH_TEMP_SUSPEND:
        me->health = POWER_SUPPLY_HEALTH_OVERHEAT;
        break;
    default:
        me->health = POWER_SUPPLY_HEALTH_GOOD;
        break;
    }

out:
    log_vdbg("PSY%d, DcIn%d, Bat%Xh, Chg%Xh, P%d H%d S%d T%d\n",
        psy_type, me->dcin_type, dc_batt_dtls, chg_dtls,
        me->present, me->health, me->status, me->charge_type);
    return rc;
}

#define max77819_charger_resume_log_work(_me) \
        do {\
            if (likely(LOG_WORKER)) {\
                if (likely(!delayed_work_pending(&(_me)->log_work))) {\
                    schedule_delayed_work(&(_me)->log_work, LOG_WORK_INTERVAL);\
                }\
            }\
        } while (0)

#define max77819_charger_suspend_log_work(_me) \
        cancel_delayed_work_sync(&(_me)->log_work)

static void max77819_charger_log_work (struct work_struct *work)
{
    struct max77819_charger *me =
        container_of(work, struct max77819_charger, log_work.work);
    int val = 0;
    u8 regval = 0;
#if defined(CONFIG_MACH_ODIN_LIGER)
	u8 onoff = 0;
	u32 current_limit = 0;
	u32 chgcc = 0;
	u8 topoff = 0, batreg = 0;
    u32 vichg = 0;
#endif

    __lock(me);

    max77819_charger_update(me, me->dcin_type);

#if defined(CONFIG_MACH_ODIN_LIGER)
    max77819_charger_get_enable(me, &val);
	onoff = val;

    max77819_charger_get_dcilmt(me, &val);
	current_limit = val;

    max77819_charger_get_chgcc(me, &val);
	chgcc = val;
    max77819_read(me->io, TOPOFF, &regval);
	topoff = regval;
    max77819_read(me->io, BATREG, &regval);
	batreg = regval;
    me->vichg = vichg = (d2260_adc_get_vichg() * 100) / 141;

    log_info("%s,DCIN%d,P%d,H%d,S%d,T%d,cl%dmA,cc%dmA,ci%dmA,0x%02Xh,0x%02Xh\n",
        (onoff ? "On" : "Off"), me->dcin_type,
        me->present, me->health, me->status, me->charge_type,
        ((current_limit == MAX77819_CHARGER_CURRENT_UNLIMIT) ?
		-1 : (current_limit / 1000)),
        (chgcc / 1000), vichg, topoff, batreg
    );
#else
    max77819_charger_get_enable(me, &val);
    log_info("charger = %s\n", val ? "on" : "off");

    max77819_charger_get_dcilmt(me, &val);

    if (me->current_limit_volatile == MAX77819_CHARGER_CURRENT_UNLIMIT) {
        log_info("current limit expected = unlimited\n");
    } else {
        log_info("current limit expected = %duA\n", me->current_limit_volatile);
    }

    if (val == MAX77819_CHARGER_CURRENT_UNLIMIT) {
        log_info("current limit set      = unlimited\n");
    } else {
        log_info("current limit set      = %duA\n", val);
    }

    max77819_charger_get_chgcc(me, &val);

    log_info("charge current expected = %duA\n", me->charge_current_volatile);
    log_info("charge current set      = %duA\n", val);

    max77819_read(me->io, TOPOFF, &regval);
    log_info("TOPOFF register %02Xh\n", regval);

    max77819_read(me->io, BATREG, &regval);
    log_info("BATREG register %02Xh\n", regval);
#endif

    max77819_charger_resume_log_work(me);

    __unlock(me);
}

unsigned char VBUS_STATUS = 0; /* VBUS Status for Battery Temperature Driver */
static void max77819_charger_irq_work (struct work_struct *work)
{
    struct max77819_charger *me =
        container_of(work, struct max77819_charger, irq_work.work);
    unsigned long irq_flags;
    u8 irq1_current, irq2_current, chgcc, chgctl2, dccrnt;
    bool present_input;
    int rc;

    __lock(me);

    irq1_current = max77819_charger_read_irq_status(me, CHGINT1);
    irq2_current = max77819_charger_read_irq_status(me, CHGINT2);

    spin_lock_irqsave(&me->irq_lock, irq_flags);
    irq1_current |= me->irq1_saved;
    me->irq1_saved = 0;
    irq2_current |= me->irq2_saved;
    me->irq2_saved = 0;
    spin_unlock_irqrestore(&me->irq_lock, irq_flags);

    log_info("<IRQ_WORK> CHGINT1 %02Xh CHGINT2 %02Xh\n", irq1_current,
        irq2_current);
#if 0 /* Not necessary original code */
    if (unlikely(!irq1_current && !irq2_current)) {
        goto done;
    }
#endif
    present_input = max77819_charger_present_input(me);
    log_info("<IRQ_WORK> present_input %d, dev_initialized %d\n",
				present_input, me->dev_initialized);
    if (present_input ^ me->dev_initialized) {
#ifdef CONFIG_USB_ODIN_DRD
	usb_dbg("DC input %s\n", present_input ? "inserted" : "removed");
	if (me->odin_otg && me->usb_init) {
		if (present_input)
			odin_otg_vbus_event(me->odin_otg, 1, 350);
		else
			odin_otg_vbus_event(me->odin_otg, 0, 0);
	}
#endif
        max77819_charger_psy_init(me);

        if (likely(present_input)) {
            log_info("start charging\n");

			VBUS_STATUS = 1;

//			max77819_charger_wake_lock(me, true);
            max77819_charger_init_dev(me);
			max77819_read(me->io,FCHGCRNT, &chgcc);
			max77819_read(me->io,CHGCTL2, &chgctl2);
			max77819_read(me->io,DCCRNT, &dccrnt);
			log_info("chgcc: 0x%x, chgctl2: 0x%x, dccrnt: 0x%x\n",chgcc,chgctl2, dccrnt);
        } else {
            log_info("stop charging\n");

			/* Call Battery Temperature Control with Reset Status  when un-plugged */
			VBUS_STATUS = 0;



            max77819_charger_exit_dev(me);
#ifdef CONFIG_USB_ODIN_DRD
			me->usbphy_ready = 0;
			max77819_set_usbphy_ready(0);
#endif
			set_wireless_charger_status(0);
			call_sysfs_fuel();
        }

#if 0 /* If system is using extra charger, please enable this function */
        max77819_charger_psy_setprop(me, psy_coop, CHARGING_ENABLED,
                present_input);
        max77819_charger_psy_setprop(me, psy_ext, PRESENT, present_input);
#endif
        schedule_delayed_work(&me->irq_work, IRQ_WORK_DELAY);
        goto out;
    }

    /* notify psy changed */
    max77819_charger_psy_changed(me);
    if (present_input && me->dev_initialized) {
        /* check charger type & current limit */
        max77819_charger_check_cable(me);

        if (me->dcin_type != POWER_SUPPLY_TYPE_UNKNOWN) {
            /* charge current */
            rc = max77819_charger_set_charge_current(me, me->current_limit_volatile,
                    me->charge_current_volatile);
            if (unlikely(IS_ERR_VALUE(rc))) {
                log_err("can't set charge current\n");
                me->dcin_type = POWER_SUPPLY_TYPE_UNKNOWN;
            }
        }

        if (me->dcin_type == POWER_SUPPLY_TYPE_UNKNOWN)
            schedule_delayed_work(&me->irq_work, msecs_to_jiffies(500));
    }

done:
    if (unlikely(me->irq <= 0)) {
        if (likely(!delayed_work_pending(&me->irq_work))) {
            schedule_delayed_work(&me->irq_work, IRQ_WORK_INTERVAL);
        }
    }

out:
    __unlock(me);
    return;
}

static irqreturn_t max77819_charger_isr (int irq, void *data)
{
    struct max77819_charger *me = data;
    u8 irq_current;
	u8 irq_data;
#ifdef CONFIG_USB_ODIN_DRD
    usb_dbg("[%s] \n", __func__);
    if (me->usb_init)
#endif
    {
        irq_current = max77819_charger_read_irq_status(me, CHGINT1);
        log_vdbg("<ISR> CHGINT1 CURR %02Xh SAVED %02Xh\n",
                irq_current, me->irq1_saved);
        log_info("<ISR> CHGINT1 CURR %02Xh SAVED %02Xh\n",
                irq_current, me->irq1_saved);


		irq_data =  max77819_charger_read_irq_status(me, 0x3C); // AICL control
        log_info("<ISR> AICL control 0x%x \n",irq_data);

        me->irq1_saved |= irq_current;

        irq_current = max77819_charger_read_irq_status(me, CHGINT2);
        log_vdbg("<ISR> CHGINT2 CURR %02Xh SAVED %02Xh\n",
                irq_current, me->irq2_saved);
        log_info("<ISR> CHGINT2 CURR %02Xh SAVED %02Xh\n",
                irq_current, me->irq2_saved);
        me->irq2_saved |= irq_current;

        __pm_wakeup_event(&max77819_chg_lock, 100); /* 100ms Wake lock time */

        schedule_delayed_work(&me->irq_work, IRQ_WORK_DELAY);
    }
    return IRQ_HANDLED;
}

static int max77819_charger_get_property (struct power_supply *psy,
        enum power_supply_property psp, union power_supply_propval *val)
{
    struct max77819_charger *me = NULL;
    int rc = 0;

    me = (psy->type == POWER_SUPPLY_TYPE_MAINS) ?
        container_of(psy, struct max77819_charger, ac) :
        container_of(psy, struct max77819_charger, usb);

    __lock(me);

    rc = max77819_charger_update(me, psy->type);
    if (unlikely(IS_ERR_VALUE(rc))) {
        goto out;
    }

    switch (psp) {
        case POWER_SUPPLY_PROP_PRESENT:
            /*      val->intval = me->present;*/ /* Original code is modified*/
            val->intval = me->present && (psy->type == me->dcin_type);
            break;

#ifndef POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED
        case POWER_SUPPLY_PROP_ONLINE:
#endif /* !POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED */
        case POWER_SUPPLY_PROP_CHARGING_ENABLED:
            /*		val->intval = me->dev_enabled; */ /* Original code is modified*/
            /*      val->intval = odin_charger_enable; */ /* SIC code is modifed*/
            val->intval = me->chg_enabled && (psy->type == me->dcin_type);
            break;

		case POWER_SUPPLY_PROP_CHARGING_ENABLE_USBSUSPEND:
			val->intval = !(me->chg_enabled && (psy->type == me->dcin_type));
			break;

        case POWER_SUPPLY_PROP_HEALTH:
            val->intval = me->health;
            break;

        case POWER_SUPPLY_PROP_STATUS:
            val->intval = max77819_charger_present_input(me);
            break;

        case POWER_SUPPLY_PROP_CHARGE_TYPE:
            val->intval = me->charge_type;
            break;

        case POWER_SUPPLY_PROP_THERMAL_CURRENT_CTRL:
            val->intval = get_curr_thermal;
            break;

        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
            val->intval = me->charge_current_volatile;
            break;

        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
            val->intval = me->current_limit_volatile;
            break;

        default:
            rc = -EINVAL;
            goto out;
    }

    __unlock(me);
    return 0;
out:
    log_vdbg("<get_property> psp %d val %d [%d]\n", psp, val->intval, rc);
    __unlock(me);
    return rc;
}

/* [START] Test code for Thermal Engine */
int pseudo_thermal_curr_ctrl_set(struct pseudo_thermal_curr_ctrl_info_type *info)
{
	struct max77819_charger *me =  NULL;
	int rc = 0;

	if(!global_charger)
		return -1;

	me =  global_charger;

	pseudo_thermal_curr_ctrl_info.current_mA = info->current_mA;

	printk("pseudo_thermal_curr_ctrl_set : (%d)mA\n",pseudo_thermal_curr_ctrl_info.current_mA);

	/* Set POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT */
	rc = max77819_charger_set_charge_current
		(me, MAX77819_CHARGER_CURRENT_MAXIMUM,((pseudo_thermal_curr_ctrl_info.current_mA)*1000));
	if (unlikely(IS_ERR_VALUE(rc))) {
		printk("pseudo_thermal_curr_ctrl_set : Current Setting Fail\n");
	}
	return 0;
}
EXPORT_SYMBOL(pseudo_thermal_curr_ctrl_set);
/* [END] */

static int max77819_charger_set_property (struct power_supply *psy,
        enum power_supply_property psp, const union power_supply_propval *val)
{
    struct max77819_charger *me = NULL;
    int ua, rc = 0;

    me = (psy->type == POWER_SUPPLY_TYPE_MAINS) ?
        container_of(psy, struct max77819_charger, ac) :
        container_of(psy, struct max77819_charger, usb);
    __lock(me);

    switch (psp) {
        case POWER_SUPPLY_PROP_CHARGING_ENABLED:
            rc = max77819_charger_set_enable(me, val->intval);
            if (unlikely(IS_ERR_VALUE(rc))) {
                goto out;
            }

            me->dev_enabled = val->intval;

            /* apply charge current */
            rc = max77819_charger_set_charge_current(me,
				me->current_limit_volatile, me->charge_current_volatile);
            break;

		case POWER_SUPPLY_PROP_CHARGING_ENABLE_USBSUSPEND:
            rc = max77819_charger_set_usb_suspend(me, val->intval);
            if (unlikely(IS_ERR_VALUE(rc))) {
                goto out;
            }
			break;

        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
            ua = abs(val->intval);
            rc = max77819_charger_set_charge_current(me,
				me->current_limit_volatile, ua);
            if (unlikely(IS_ERR_VALUE(rc))) {
                goto out;
            }

            me->charge_current_volatile  = ua;
            me->charge_current_permanent =
                val->intval > 0 ? ua : me->charge_current_permanent;
            break;

        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
            ua = abs(val->intval);
            rc = max77819_charger_set_charge_current(me, ua,
                    me->charge_current_volatile);
            if (unlikely(IS_ERR_VALUE(rc))) {
                goto out;
            }

            me->current_limit_volatile  = ua;
            me->current_limit_permanent =
                val->intval > 0 ? ua : me->current_limit_permanent;
            break;

        default:
            rc = -EINVAL;
            goto out;
    }

out:
    log_vdbg("<set_property> psp %d val %d [%d]\n", psp, val->intval, rc);
    __unlock(me);
    return rc;
}

static int max77819_charger_property_is_writeable (struct power_supply *psy,
    enum power_supply_property psp)
{
    switch (psp) {
    case POWER_SUPPLY_PROP_CHARGING_ENABLED:
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CHARGING_ENABLE_USBSUSPEND:
        return 1;

    default:
        break;
    }

    return -EINVAL;
}

static void max77819_charger_external_power_changed (struct power_supply *psy)
{
    struct max77819_charger *me = NULL;
    struct power_supply *supplicant;
    int i;

    me = (psy->type == POWER_SUPPLY_TYPE_MAINS) ?
        container_of(psy, struct max77819_charger, ac) :
        container_of(psy, struct max77819_charger, usb);
    __lock(me);

    for (i = 0; i < me->pdata->num_supplicants; i++) {
        supplicant = power_supply_get_by_name(me->pdata->supplied_to[i]);
        if (likely(supplicant)) {
            power_supply_changed(supplicant);
        }
    }

    __unlock(me);
}

static enum power_supply_property max77819_charger_psy_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_CHARGE_TYPE,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_PRESENT,
#ifndef POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED
    POWER_SUPPLY_PROP_ONLINE,
#endif /* !POWER_SUPPLY_PROP_CHARGING_ENABLED_REPLACED */
    POWER_SUPPLY_PROP_CHARGING_ENABLED,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,     /* charging current */
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, /* input current limit */
    POWER_SUPPLY_PROP_THERMAL_CURRENT_CTRL,
    POWER_SUPPLY_PROP_CHARGING_ENABLE_USBSUSPEND,
};

static void *max77819_charger_get_platdata (struct max77819_charger *me)
{
#ifdef CONFIG_OF
    struct device *dev = me->dev;
    struct device_node *np = dev->of_node;
    struct max77819_charger_platform_data *pdata;
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

    pdata->irq = irq_of_parse_and_map(np, 0);
    log_dbg("property:IRQ             %d\n", pdata->irq);

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

    pdata->current_limit_usb = 500000;
    of_property_read_u32(np, "current_limit_usb",
            &pdata->current_limit_usb);
    log_dbg("property:DCILMT_USB      %uuA\n", pdata->current_limit_usb);

    pdata->current_limit_ac = 1700000;
    of_property_read_u32(np, "current_limit_ac",
            &pdata->current_limit_ac);
    log_dbg("property:DCILMT_AC       %uuA\n", pdata->current_limit_ac);
    pdata->fast_charge_current = 500000;
    of_property_read_u32(np, "fast_charge_current",
        &pdata->fast_charge_current);
    log_dbg("property:CHGCC           %uuA\n", pdata->fast_charge_current);

    pdata->charge_termination_voltage = 43500000;
    of_property_read_u32(np, "charge_termination_voltage",
        &pdata->charge_termination_voltage);
    log_dbg("property:MBATREG         %uuV\n",
        pdata->charge_termination_voltage);

    pdata->topoff_timer = 0;
    of_property_read_u32(np, "topoff_timer", &pdata->topoff_timer);
    log_dbg("property:TOPOFFTIME      %umin\n", pdata->topoff_timer);

    pdata->topoff_current = 200000;
    of_property_read_u32(np, "topoff_current", &pdata->topoff_current);
    log_dbg("property:ITOPOFF         %uuA\n", pdata->topoff_current);

    pdata->charge_restart_threshold = 150000;
    of_property_read_u32(np, "charge_restart_threshold",
        &pdata->charge_restart_threshold);
    log_dbg("property:CHGRSTRT        %uuV\n", pdata->charge_restart_threshold);

    pdata->enable_coop = of_property_read_bool(np, "enable_coop");
    log_dbg("property:COOP CHG        %s\n",
        pdata->enable_coop ? "enabled" : "disabled");

    if (likely(pdata->enable_coop)) {
        of_property_read_string(np, "coop_psy_name",
            (char const **)&pdata->coop_psy_name);
        log_dbg("property:COOP CHG        %s\n",
            pdata->coop_psy_name ? pdata->coop_psy_name : "null");
    }

    pdata->enable_thermistor = of_property_read_bool(np, "enable_thermistor");
    log_dbg("property:THERMISTOR      %s\n",
        pdata->enable_thermistor ? "enabled" : "disabled");

    pdata->enable_aicl = of_property_read_bool(np, "enable_aicl");
    log_dbg("property:AICL            %s\n",
        pdata->enable_aicl ? "enabled" : "disabled");

    pdata->aicl_detection_voltage = 4500000;
    of_property_read_u32(np, "aicl_detection_voltage",
        &pdata->aicl_detection_voltage);
    log_dbg("property:AICL DETECTION  %uuV\n", pdata->aicl_detection_voltage);

    pdata->aicl_reset_threshold = 100000;
    of_property_read_u32(np, "aicl_reset_threshold",
        &pdata->aicl_reset_threshold);
    log_dbg("property:AICL RESET      %uuV\n", pdata->aicl_reset_threshold);

out:
    return pdata;
#else /* CONFIG_OF */
    return dev_get_platdata(me->dev) ?
        dev_get_platdata(me->dev) : ERR_PTR(-EINVAL);
#endif /* CONFIG_OF */
}

static __always_inline
void max77819_charger_destroy (struct max77819_charger *me)
{
    struct device *dev = me->dev;

    cancel_delayed_work_sync(&me->log_work);

    if (likely(me->irq > 0)) {
        devm_free_irq(dev, me->irq, me);
    }

    cancel_delayed_work_sync(&me->irq_work);

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
/*  spin_lock_destroy(&me->irq_lock);*/

    devm_kfree(dev, me);
}

#ifdef CONFIG_OF
static struct of_device_id max77819_charger_of_ids[] = {
    { .compatible = "maxim,"MAX77819_CHARGER_NAME },
    { },
};
MODULE_DEVICE_TABLE(of, max77819_charger_of_ids);
#endif /* CONFIG_OF */

static int set_reg(void *data, u64 val)
{
	struct max77819_charger *me;
	u32 addr = (u32) data;
	int ret;

	if (global_charger) {
		me = global_charger;
	}
	else{
		printk("==global_charger set_reg is not initialized yet==\n");
		return;
	}

	//struct i2c_client *client = the_chip->client;
	//ret = bq24192_write_reg(client, addr, (u8) val);

	ret = max77819_write(me->io, addr, (u8) val);

	return ret;
}
static int get_reg(void *data, u64 *val)
{
	struct max77819_charger *me;
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

	ret = max77819_read(me->io, addr, &temp);
	if (ret < 0)
		return ret;

	*val = temp;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");


static int max77819_chrg_create_debugfs_entries(struct max77819_charger *chip)
{
	int i;

	chip->dent = debugfs_create_dir(MAX77819_CHARGER_NAME, NULL);
	if (IS_ERR(chip->dent)) {
		pr_err("max77819_chrg driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(max77819_chrg_debug_regs) ; i++) {
		char *name = max77819_chrg_debug_regs[i].name;
		u32 reg = max77819_chrg_debug_regs[i].reg;
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

void check_factory_cable(struct max77819_charger *me)
{
	int temp=0;
	usb_id = d2260_adc_get_usb_id();
	temp = d2260_get_battery_temp();

	log_info("is_factory_cable , usb_id :%d ,  get-factory-boot:%d , temper:%d \n",usb_id, lge_get_factory_boot(),temp);
	if(((usb_id > 90 && usb_id < 1200) || lge_get_factory_boot()) && (temp < -300))
		is_factory_cable = 1;


	log_info("is_factory_cable :%d\n", is_factory_cable);

	if(is_factory_cable == 1)
		max77819_charger_lock(me) ;

}

static __init int max77819_charger_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct max77819_dev *chip = dev_get_drvdata(dev->parent);
    struct max77819_charger *me = NULL;
    int rc = 0;
	int ret = 0;

    log_dbg("attached\n");

    me = devm_kzalloc(dev, sizeof(*me), GFP_KERNEL);
    if (unlikely(!me)) {
        log_err("out of memory (%uB requested)\n", sizeof(*me));
        return -ENOMEM;
    }

    dev_set_drvdata(dev, me);

    spin_lock_init(&me->irq_lock);
    mutex_init(&me->lock);
    me->io   = max77819_get_io(chip);
    me->dev  = dev;
    me->kobj = &dev->kobj;
    me->irq  = -1;

    INIT_DELAYED_WORK(&me->irq_work, max77819_charger_irq_work);
    INIT_DELAYED_WORK(&me->log_work, max77819_charger_log_work);

    me->pdata = max77819_charger_get_platdata(me);
    if (unlikely(IS_ERR(me->pdata))) {
        rc = PTR_ERR(me->pdata);
        me->pdata = NULL;

        log_err("failed to get platform data [%d]\n", rc);
        goto abort;
    }

    /* disable all IRQ */
    max77819_write(me->io, CHGINTM1,   0xC6);
    max77819_write(me->io, CHGINTMSK2, 0xFF);

    me->dev_enabled               =
        (!me->pdata->enable_coop || me->pdata->coop_psy_name);
    me->current_limit_permanent   = MAX77819_CHARGER_CURRENT_UNLIMIT;
    me->current_limit_volatile    = me->current_limit_permanent;
    me->charge_current_permanent  = me->pdata->fast_charge_current;
    me->charge_current_volatile   = me->charge_current_permanent;

    me->ac.name                     = "ac";
    me->ac.type                     = POWER_SUPPLY_TYPE_MAINS;
    me->ac.supplied_to              = me->pdata->supplied_to;
    me->ac.num_supplicants          = me->pdata->num_supplicants;
    me->ac.properties               = max77819_charger_psy_props;
    me->ac.num_properties           = ARRAY_SIZE(max77819_charger_psy_props);
    me->ac.get_property             = max77819_charger_get_property;
    me->ac.set_property             = max77819_charger_set_property;
    me->ac.property_is_writeable	= max77819_charger_property_is_writeable;
    me->ac.external_power_changed	= max77819_charger_external_power_changed;

    rc = power_supply_register(dev, &me->ac);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("failed to register ac_power_supply class [%d]\n", rc);
        goto abort;
    }

    me->usb.name                    = "usb";
    me->usb.type                    = POWER_SUPPLY_TYPE_USB;
    me->usb.properties              = max77819_charger_psy_props;
    me->usb.num_properties          = ARRAY_SIZE(max77819_charger_psy_props);
    me->usb.get_property            = max77819_charger_get_property;
    me->usb.set_property            = max77819_charger_set_property;
    me->usb.property_is_writeable   = max77819_charger_property_is_writeable;

    rc = power_supply_register(dev, &me->usb);
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("failed to register usb_power_supply class [%d]\n", rc);
        goto abort;
    }

	check_factory_cable(me);

    max77819_charger_psy_init(me);

#ifdef CONFIG_USB_ODIN_DRD
	me->usb_init = 0;
	me->usbphy_ready = false;
#endif

	me->irq = regmap_irq_get_virq(chip->irq_data, MAX77819_IRQ_CHGR);
	if (me->irq < 0) {
		rc = me->irq;
		goto abort;
	}
	log_dbg("77819 Charger IRQ = %d \n", me->irq);

    if (unlikely(me->irq <= 0)) {
        log_warn("interrupt disabled\n");
        schedule_delayed_work(&me->irq_work, IRQ_WORK_INTERVAL);
    } else {
        rc = devm_request_threaded_irq(dev, me->irq, NULL, max77819_charger_isr,
            IRQF_ONESHOT, DRIVER_NAME, me);
        if (unlikely(IS_ERR_VALUE(rc))) {
            log_err("failed to request IRQ %d [%d]\n", me->irq, rc);
            me->irq = -1;
            goto abort;
        }
    }

   global_charger = me;

#ifdef CONFIG_USB_ODIN_DRD
	max77819_write(me->io, CHGINTM1, (u8)~CHGINT1_DC_UVP);
	max77819_charger_usb_register(me);
#else
    /* enable IRQ we want */
    max77819_write(me->io, CHGINTM1, (u8)~CHGINT1_DC_UVP);
#endif
    log_info("driver "DRIVER_VERSION" installed\n");

	ret = max77819_chrg_create_debugfs_entries(me);
	if (ret) {
		pr_err("max77819_chrg_create_debugfs_entries failed\n");
	}

    schedule_delayed_work(&me->irq_work, IRQ_WORK_DELAY);
    max77819_charger_resume_log_work(me);

	wakeup_source_init(&max77819_chg_lock, "charger_lock");

    return 0;

abort:
    dev_set_drvdata(dev, NULL);
    max77819_charger_destroy(me);
	if (me->dent)
		debugfs_remove_recursive(me->dent);
    return rc;
}

static int max77819_charger_remove (struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct max77819_charger *me = dev_get_drvdata(dev);

	global_charger = NULL;

    dev_set_drvdata(dev, NULL);
    max77819_charger_destroy(me);

	if (me->dent)
		debugfs_remove_recursive(me->dent);

    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max77819_charger_suspend (struct device *dev)
{
    struct max77819_charger *me = dev_get_drvdata(dev);

    __lock(me);

    log_vdbg("suspending\n");

    max77819_charger_suspend_log_work(me);
    //flush_delayed_work(&me->irq_work);
    cancel_delayed_work_sync(&me->irq_work);

    __unlock(me);
    return 0;
}

static int max77819_charger_resume (struct device *dev)
{
    struct max77819_charger *me = dev_get_drvdata(dev);

    __lock(me);

    log_vdbg("resuming\n");

    schedule_delayed_work(&me->irq_work, IRQ_WORK_DELAY);
    max77819_charger_resume_log_work(me);

    __unlock(me);
    return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(max77819_charger_pm, max77819_charger_suspend,
    max77819_charger_resume);

static struct platform_driver max77819_charger_driver =
{
    .driver.name            = DRIVER_NAME,
    .driver.owner           = THIS_MODULE,
    .driver.pm              = &max77819_charger_pm,
#ifdef CONFIG_OF
    .driver.of_match_table  = max77819_charger_of_ids,
#endif /* CONFIG_OF */
    .probe                  = max77819_charger_probe,
    .remove                 = max77819_charger_remove,
};

static __init int max77819_charger_init (void)
{
    return platform_driver_register(&max77819_charger_driver);
}
module_init(max77819_charger_init);

static __exit void max77819_charger_exit (void)
{
    platform_driver_unregister(&max77819_charger_driver);
}
module_exit(max77819_charger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_VERSION(DRIVER_VERSION);
