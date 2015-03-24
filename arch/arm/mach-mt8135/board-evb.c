
#include <linux/pm.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <asm/mach-types.h>
#include <mach/mt_board.h>
#include <mach/board.h>
#include <mach/devs.h>
#include <mach/mt_pm_ldo.h>
#include <mach/board-common-audio.h>
#include <mach/mt_gpio.h>
#include <mt8135_evbp1_v2/cust_gpio_usage.h>
#include <mt8135_evbp1_v2/cust_eint.h>
#include <mt8135_evbp1_v2/cust_kpd.h>

#ifdef CONFIG_MTK_KEYPAD
#include <mach/mt_typedefs.h>
#include <mach/hal_pub_kpd.h>
#include <mach/mtk_kpd.h>
#endif

/**************** Accelerometer Customization ********************************/
#include <cust/cust_acc.h>
/*---------------------------------------------------------------------------*/
#define MC_I2C_DEV_BUS		3
#define MC_I2C_SLAVE_ADDR		0x4C
static struct i2c_board_info i2c_mc3210 __initdata = {
	I2C_BOARD_INFO("MC3210", MC_I2C_SLAVE_ADDR) };

int cust_acc_power(struct acc_hw *hw, unsigned int on, char *devname)
{
	if (hw->power_id == MT65XX_POWER_NONE)
		return 0;
	if (on)
		return hwPowerOn(hw->power_id, hw->power_vol, devname);
	else
		return hwPowerDown(hw->power_id, devname);
}

/*---------------------------------------------------------------------------*/
static struct acc_hw cust_acc_hw = {
	.i2c_num = MC_I2C_DEV_BUS,
	.direction = 6,
	.power_id = MT65XX_POWER_NONE,	/*!< LDO is not used */
	.power_vol = VOL_DEFAULT,	/*!< LDO is not used */
	.firlen = 0,		/*!< don't enable low pass fileter */
	.power = cust_acc_power,
};

/*---------------------------------------------------------------------------*/
struct acc_hw *get_cust_acc_hw(void)
{
	return &cust_acc_hw;
}

/****************** Gyroscope Customization **********************************/
#include <cust/cust_gyro.h>
/*---------------------------------------------------------------------------*/
#define MPU_I2C_DEV_BUS		3
#define MPU_I2C_SLAVE_ADDR		0xD0
static struct i2c_board_info i2c_mpu3000 __initdata = {
	I2C_BOARD_INFO("MPU3000", (MPU_I2C_SLAVE_ADDR >> 1)) };

static struct gyro_hw cust_gyro_hw = {
	.addr = MPU_I2C_SLAVE_ADDR,
	.i2c_num = MPU_I2C_DEV_BUS,
	.direction = 6,
	.power_id = MT65XX_POWER_NONE,	/*!< LDO is not used */
	.power_vol = VOL_DEFAULT,	/*!< LDO is not used */
	.firlen = 16,		/*!< don't enable low pass fileter */
	/* .power = cust_gyro_power, */
};

/*---------------------------------------------------------------------------*/
struct gyro_hw *get_cust_gyro_hw(void)
{
	return &cust_gyro_hw;
}

/*---------------------------------------------------------------------------*/
static int mpu_gpio_config(void)
{
	/* because we donot use EINT ,to support low power */
	/* config to GPIO input mode + PD */
	/* set   GPIO_MSE_EINT_PIN */
	mt_set_gpio_mode(GPIO_GYRO_EINT_PIN, GPIO_GYRO_EINT_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_GYRO_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_GYRO_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_GYRO_EINT_PIN, GPIO_PULL_DOWN);
	return 0;
}

/**************** Magnetometer Customization ********************************/
#include <cust/cust_mag.h>
#define AKM_I2C_DEV_BUS		3
#define AKM_I2C_SLAVE_ADDR		0x18
static struct i2c_board_info i2c_akm8963 __initdata = {
	I2C_BOARD_INFO("akm8963", (AKM_I2C_SLAVE_ADDR >> 1)) };

static struct mag_hw cust_mag_hw = {
	.i2c_num = AKM_I2C_DEV_BUS,
	.direction = 6,
	.power_id = MT65XX_POWER_NONE,	/*!< LDO is not used */
	.power_vol = VOL_DEFAULT,	/*!< LDO is not used */
};

struct mag_hw *get_cust_mag_hw(void)
{
	return &cust_mag_hw;
}

/************************* Accdet Customization ********************************/
#include <accdet_custom_def.h>
#include <accdet_custom.h>

/* key press customization: long press time */
struct headset_key_custom headset_key_custom_setting = {
	2000
};

struct headset_key_custom *get_headset_key_custom_setting(void)
{
	return &headset_key_custom_setting;
}

#ifdef ACCDET_MULTI_KEY_FEATURE
static struct headset_mode_settings cust_headset_settings = {
	0x900, 0x400, 1, 0x3f0, 0x800, 0x800, 0x20
};
#else
/* headset mode register settings(for MT6575) */
static struct headset_mode_settings cust_headset_settings = {
	0x900, 0x400, 1, 0x3f0, 0x3000, 0x3000, 0x20
};
#endif

struct headset_mode_settings *get_cust_headset_settings(void)
{
	return &cust_headset_settings;
}

static struct headset_mode_data cust_headset_data = {
	.cust_eint_accdet_num = CUST_EINT_ACCDET_NUM,
	.cust_eint_accdet_debounce_cn = CUST_EINT_ACCDET_DEBOUNCE_CN,
	.cust_eint_accdet_type = CUST_EINT_ACCDET_TYPE,
	.cust_eint_accdet_debounce_en = CUST_EINT_ACCDET_DEBOUNCE_EN,
	.gpio_accdet_eint_pin = GPIO_ACCDET_EINT_PIN,
	.gpio_accdet_eint_pin_m_eint = GPIO_ACCDET_EINT_PIN_M_EINT,
};

struct headset_mode_data *get_cust_headset_data(void)
{
	return &cust_headset_data;
}

/************************* Leds Customization ********************************/
#include <cust_leds.h>
#include <cust_leds_def.h>
#include <linux/mtkfb.h>
#include <drivers/misc/mediatek/dispsys/mt8135/ddp_bls.h>

#define CUST_BLS_PWM		/* need to be compatible with dct GPIO setting */

/*
#define ERROR_BL_LEVEL 0xFFFFFFFF

unsigned int brightness_mapping(unsigned int level)
{
	return ERROR_BL_LEVEL;
}
*/
unsigned int brightness_mapping(unsigned int level)
{
#if !defined(CUST_BLS_PWM)

	return (level << 6) >> 8;
#else
	unsigned int mapped_level;

	mapped_level = level;

	return mapped_level;
#endif
}

unsigned int Cust_SetBacklight(int level, int div)
{
#if !defined(CUST_BLS_PWM)
	mtkfb_set_backlight_pwm(div);
	mtkfb_set_backlight_level(brightness_mapping(level));
#else
	disp_bls_set_backlight(brightness_mapping(level));
#endif
	return 0;
}


static struct cust_mt65xx_led cust_led_list[MT65XX_LED_TYPE_TOTAL] = {
	{"red", MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_NLED_ISINK0, {0} },	/* have no hw, add as blue for android app need */
	{"green", MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_NLED_ISINK1, {0} },
	{"blue", MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_NLED_ISINK0, {0} },
	{"jogball-backlight", MT65XX_LED_MODE_NONE, -1, {0} },
	{"keyboard-backlight", MT65XX_LED_MODE_NONE, -1, {0} },
	{"button-backlight", MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_BUTTON, {0} },
#if !defined(CUST_BLS_PWM)
	{"lcd-backlight", MT65XX_LED_MODE_PWM, PWM1, {1, 3, 1, 1, 0} },
#else
	{"lcd-backlight", MT65XX_LED_MODE_CUST_BLS_PWM, (int)disp_bls_set_backlight,
	 {0, 0x11, 0, 0, 0} },
#endif
};

struct cust_mt65xx_led *get_cust_led_list(void)
{
	return cust_led_list;
}

/********** ALSPS(Ambient Light Sensor) Customization ************************/
#include <cust_alsps.h>
#include <mach/mt_pm_ldo.h>

#ifdef CONFIG_ALSPS_MTK_STK2203
#define STK_I2C_DEV_BUS		3
#define STK_I2C_SLAVE_ADDR		0x20
static struct i2c_board_info i2c_stk2203 __initdata = {
	I2C_BOARD_INFO("STK2203", (STK_I2C_SLAVE_ADDR >> 1)) };

static struct alsps_hw cust_alsps_hw = {
	.i2c_num = STK_I2C_DEV_BUS,
	.irq_gpio = GPIO_ALS_EINT_PIN,
	.polling_mode = 1,
	.power_id = MT65XX_POWER_NONE,	/*LDO is not used */
	.power_vol = VOL_DEFAULT,	/*LDO is not used */
	.i2c_addr = {0x20, 0x00, 0x00, 0x00},
	.als_level = {0, 1, 1, 5, 11, 11, 70, 700, 1400, 2100, 4200, 7000, 9800, 12600, 14000},
	.als_value = {0, 1, 1, 5, 11, 11, 70, 700, 1400, 2100, 4200, 7000, 9800, 12600, 14000, 14000},
	/* .als_value  = {40, 40, 90,  90, 160, 160,  225,  320,  640,  1280,  1280,  2600,  2600, 2600,  10240, 10240}, */
	.ps_threshold = 0xFE,
};

struct alsps_hw *get_cust_alsps_hw(void)
{
	return &cust_alsps_hw;
}

static int stk_gpio_config(void)
{
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);
	return 0;
}

#endif				/* CONFIG_ALSPS_MTK_STK2203 */

/********** keypad Customization ************************/
#ifdef CONFIG_MTK_KEYPAD

static u16 kpd_keymap[KPD_NUM_KEYS] = KPD_INIT_KEYMAP();

struct mtk_kpd_hardware mtk_kpd_hw = {
	.kpd_init_keymap = kpd_keymap,
	.kpd_pwrkey_map = KEY_POWER,
	.kpd_key_debounce = KPD_KEY_DEBOUNCE,
	.onekey_reboot_normal_mode = TRUE,	/* ONEKEY_REBOOT_NORMAL_MODE */
	.twokey_reboot_normal_mode = FALSE,	/* TWOKEY_REBOOT_NORMAL_MODE */
	.onekey_reboot_other_mode = TRUE,	/* ONEKEY_REBOOT_OTHER_MODE */
	.twokey_reboot_other_mode = FALSE,	/* TWOKEY_REBOOT_OTHER_MODE */
	.kpd_pmic_rstkey_map_en = FALSE,	/* KPD_PMIC_RSTKEY_MAP */
	.kpd_pmic_rstkey_map_value = KEY_VOLUMEDOWN,
	.kpd_pmic_lprst_td_en = TRUE,	/* KPD_PMIC_LPRST_TD */
	.kpd_pmic_lprst_td_value = 3,	/* timeout period. 0: 8sec; 1: 11sec; 2: 14sec; 3: 5sec */
	.kcol = {
		[0] = GPIO_KPD_KCOL0_PIN & ~0x80000000,
		[1] = GPIO_KPD_KCOL1_PIN & ~0x80000000,
	},
};

static struct platform_device kpd_pdev = {
	.name = "mtk-kpd",
	.id = -1,
	.dev = {
		.platform_data = &mtk_kpd_hw,
	},
};

static int __init mt_kpd_init(void)
{
	return platform_device_register(&kpd_pdev);
}

#endif				/* CONFIG_MTK_KEYPAD */

/************************* Camera Customization ******************************/

#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include "kd_camera_hw.h"

#include "linux/camera/kd_imgsensor.h"
#include "linux/camera/kd_imgsensor_define.h"
#include "linux/camera/kd_camera_feature.h"

#define CAMERA_POWER_VCAM_A MT65XX_POWER_LDO_VCAMA
#define CAMERA_POWER_VCAM_D MT65XX_POWER_LDO_VCAMD	/* VCAMD */
#define CAMERA_POWER_VCAM_A2 MT65XX_POWER_LDO_VCAMD	/* VCAMAF */
#define CAMERA_POWER_VCAM_D2 MT65XX_POWER_LDO_VCAMIO	/* VCAMIO */

/******************************************************************************
 * Debug configuration
******************************************************************************/
#define PFX "[kd_camera_hw]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    pr_info(PFX "%s: " fmt, __func__ , ##arg)

#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG PK_DBG_FUNC
#define PK_ERR(fmt, arg...)         pr_err(PFX "%s: " fmt, __func__ , ##arg)
#else
#define PK_DBG(a, ...)
#define PK_ERR(a, ...)
#endif

int kdCISModulePowerOn(CAMERA_DUAL_CAMERA_SENSOR_ENUM SensorIdx, char *currSensorName, BOOL On,
		       char *mode_name)
{
	u32 pinSetIdx = 0;	/* default main sensor */

#define IDX_PS_CMRST 0
#define IDX_PS_CMPDN 4

#define IDX_PS_MODE 1
#define IDX_PS_ON   2
#define IDX_PS_OFF  3

	u32 pinSet[2][8] = {
		/* for test ov2659 in main sensor slot */
		{
		 GPIO_CAMERA_CMRST_PIN,
		 GPIO_CAMERA_CMRST_PIN_M_GPIO,	/* mode */
		 GPIO_OUT_ONE,	/* ON state */
		 GPIO_OUT_ZERO,	/* OFF state */
		 GPIO_CAMERA_CMPDN_PIN,
		 GPIO_CAMERA_CMPDN_PIN_M_GPIO,
		 GPIO_OUT_ZERO,
		 GPIO_OUT_ONE,
		 },
		/* for sub sensor */
		{
		 GPIO_CAMERA_CMRST1_PIN,
		 GPIO_CAMERA_CMRST1_PIN_M_GPIO,
		 GPIO_OUT_ZERO,
		 GPIO_OUT_ONE,
		 GPIO_CAMERA_CMPDN1_PIN,
		 GPIO_CAMERA_CMRST1_PIN_M_GPIO,
		 GPIO_OUT_ZERO,
		 GPIO_OUT_ONE,
		 }
	};

	if (DUAL_CAMERA_MAIN_SENSOR == SensorIdx) {
		pinSetIdx = 0;
	} else if (DUAL_CAMERA_SUB_SENSOR == SensorIdx) {
		pinSetIdx = 1;
	} else if (DUAL_CAMERA_MAIN_2_SENSOR == SensorIdx) {
		return 0;
	}
	/* power ON */
	if (On) {
		PK_DBG("Off camera %d GPIO...\n", pinSetIdx);
		PK_DBG("RST pin: %d, PDN pin: %d\n", pinSet[pinSetIdx][IDX_PS_CMRST],
		       pinSet[pinSetIdx][IDX_PS_CMPDN]);
		if (mt_set_gpio_mode
		    (pinSet[pinSetIdx][IDX_PS_CMRST],
		     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_MODE])) {
			PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
		}
		if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)) {
			PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
		}
		if (mt_set_gpio_out
		    (pinSet[pinSetIdx][IDX_PS_CMRST],
		     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF])) {
			PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
		}
		if (mt_set_gpio_mode
		    (pinSet[pinSetIdx][IDX_PS_CMPDN],
		     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
			PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
		}
		if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
			PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
		}
		if (mt_set_gpio_out
		    (pinSet[pinSetIdx][IDX_PS_CMPDN],
		     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF])) {
			PK_DBG("[CAMERA LENS] set gpio failed!!\n");
		}

		PK_DBG("Off camera %d GPIO...\n", 1 - pinSetIdx);
		PK_DBG("RST pin: %d, PDN pin: %d\n", pinSet[1 - pinSetIdx][IDX_PS_CMRST],
		       pinSet[1 - pinSetIdx][IDX_PS_CMPDN]);
		if (mt_set_gpio_mode
		    (pinSet[1 - pinSetIdx][IDX_PS_CMRST],
		     pinSet[1 - pinSetIdx][IDX_PS_CMRST + IDX_PS_MODE])) {
			PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
		}
		if (mt_set_gpio_dir(pinSet[1 - pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)) {
			PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
		}
		if (mt_set_gpio_out
		    (pinSet[1 - pinSetIdx][IDX_PS_CMRST],
		     pinSet[1 - pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF])) {
			PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
		}
		if (mt_set_gpio_mode
		    (pinSet[1 - pinSetIdx][IDX_PS_CMPDN],
		     pinSet[1 - pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
			PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
		}
		if (mt_set_gpio_dir(pinSet[1 - pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
			PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
		}
		if (mt_set_gpio_out
		    (pinSet[1 - pinSetIdx][IDX_PS_CMPDN],
		     pinSet[1 - pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF])) {
			PK_DBG("[CAMERA LENS] set gpio failed!!\n");
		}

		if (pinSetIdx == 0) {
			PK_DBG("Power on main sensor...\n");
			PK_DBG("RST pin: %d, PDN pin: %d\n", pinSet[pinSetIdx][IDX_PS_CMRST],
			       pinSet[pinSetIdx][IDX_PS_CMPDN]);

			PK_DBG("On PDN...\n");
			if (mt_set_gpio_mode
			    (pinSet[pinSetIdx][IDX_PS_CMPDN],
			     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
				PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
			}
			if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
				PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
			}
			if (mt_set_gpio_out
			    (pinSet[pinSetIdx][IDX_PS_CMPDN],
			     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_ON])) {
				PK_DBG("[CAMERA LENS] set gpio failed!!\n");
			}
			/* AVDD */
			if (TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800, mode_name)) {
			}
			mdelay(1);

			/* DOVDD */
			if (TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800, mode_name)) {
			}
			/* DVDD */
			if (TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800, mode_name)) {
			}


			PK_DBG("Off PDN...\n");
			if (mt_set_gpio_mode
			    (pinSet[pinSetIdx][IDX_PS_CMPDN],
			     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
				PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
			}
			if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
				PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
			}
			if (mt_set_gpio_out
			    (pinSet[pinSetIdx][IDX_PS_CMPDN],
			     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF])) {
				PK_DBG("[CAMERA LENS] set gpio failed!!\n");
			}
			mdelay(6);

			PK_DBG("On PDN...\n");
			if (mt_set_gpio_mode
			    (pinSet[pinSetIdx][IDX_PS_CMPDN],
			     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
				PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
			}
			if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
				PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
			}
			if (mt_set_gpio_out
			    (pinSet[pinSetIdx][IDX_PS_CMPDN],
			     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_ON])) {
				PK_DBG("[CAMERA LENS] set gpio failed!!\n");
			}
			mdelay(11);

			PK_DBG("On RST...\n");
			if (mt_set_gpio_mode
			    (pinSet[pinSetIdx][IDX_PS_CMRST],
			     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_MODE])) {
				PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
			}
			if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)) {
				PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
			}
			if (mt_set_gpio_out
			    (pinSet[pinSetIdx][IDX_PS_CMRST],
			     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_ON])) {
				PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
			}

		} else {
			PK_DBG("Power on sub sensor...\n");
			PK_DBG("RST pin: %d, PDN pin: %d\n", pinSet[pinSetIdx][IDX_PS_CMRST],
			       pinSet[pinSetIdx][IDX_PS_CMPDN]);

			mdelay(5);

			/* DOVDD */
			if (TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800, mode_name)) {
			}
			/* AVDD */
			if (TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800, mode_name)) {
			}
			mdelay(1);

			/* DVDD */
			if (TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800, mode_name)) {
			}

			mdelay(6);
			if (mt_set_gpio_mode
			    (pinSet[pinSetIdx][IDX_PS_CMPDN],
			     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
				PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
			}
			if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
				PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
			}
			if (mt_set_gpio_out
			    (pinSet[pinSetIdx][IDX_PS_CMPDN],
			     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_ON])) {
				PK_DBG("[CAMERA LENS] set gpio failed!!\n");
			}
			mdelay(11);
			if (mt_set_gpio_mode
			    (pinSet[pinSetIdx][IDX_PS_CMRST],
			     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_MODE])) {
				PK_DBG("[CAMERA SENSOR] set gpio mode failed!!\n");
			}
			if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)) {
				PK_DBG("[CAMERA SENSOR] set gpio dir failed!!\n");
			}
			if (mt_set_gpio_out
			    (pinSet[pinSetIdx][IDX_PS_CMRST],
			     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_ON])) {
				PK_DBG("[CAMERA SENSOR] set gpio failed!!\n");
			}
		}
	} else {
		if (mt_set_gpio_mode
		    (pinSet[pinSetIdx][IDX_PS_CMPDN],
		     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_MODE])) {
			PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
		}
		if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_DIR_OUT)) {
			PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
		}
		if (mt_set_gpio_out
		    (pinSet[pinSetIdx][IDX_PS_CMPDN],
		     pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF])) {
			PK_DBG("[CAMERA LENS] set gpio failed!!\n");
		}
		if (mt_set_gpio_mode
		    (pinSet[pinSetIdx][IDX_PS_CMRST],
		     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_MODE])) {
			PK_DBG("[CAMERA LENS] set gpio mode failed!!\n");
		}
		if (mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_DIR_OUT)) {
			PK_DBG("[CAMERA LENS] set gpio dir failed!!\n");
		}
		if (mt_set_gpio_out
		    (pinSet[pinSetIdx][IDX_PS_CMRST],
		     pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF])) {
			PK_DBG("[CAMERA LENS] set gpio failed!!\n");
		}

		if (TRUE != hwPowerDown(CAMERA_POWER_VCAM_D, mode_name)) {
		}
		if (TRUE != hwPowerDown(CAMERA_POWER_VCAM_A, mode_name)) {
		}
		if (TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2, mode_name)) {
		}
		/* For Power Saving */

		if (pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF] != GPIO_OUT_ZERO) {
			mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_OUT_ZERO);
		}
		if (pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF] != GPIO_OUT_ZERO) {
			mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO);
		}
		/* PLS add this ------------------------------------------------------------------------------------------------------- */
		if (pinSet[1 - pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF] != GPIO_OUT_ZERO) {
			mt_set_gpio_out(pinSet[1 - pinSetIdx][IDX_PS_CMPDN], GPIO_OUT_ZERO);
		}
		if (pinSet[1 - pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF] != GPIO_OUT_ZERO) {
			mt_set_gpio_out(pinSet[1 - pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO);
		}
		/* ~For Power Saving */
	}			/*  */

	return 0;
}
EXPORT_SYMBOL(kdCISModulePowerOn);

/************************* Touch Customization ******************************/
#include <mach/mt_gpio.h>
#include <cust/cust_gt9xx.h>

u8 cust_ctp_cfg_group1[] = { 0x41, 0x00, 0x05, 0x20, 0x03, 0x0A, 0x3D, 0x00,
	0x01, 0x8C, 0x23, 0x09, 0x50, 0x3C, 0x03, 0x03,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,
	0x1A, 0x1E, 0x14, 0x8E, 0x2E, 0x88, 0x2E, 0x30,
	0xA0, 0x0F, 0x00, 0x00, 0x00, 0x03, 0x02, 0x1D,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x50, 0xC8, 0x94, 0x85, 0x03,
	0x08, 0x00, 0x00, 0xCD, 0x0D, 0x31, 0x89, 0x10,
	0x31, 0x45, 0x13, 0x33, 0x2D, 0x17, 0x33, 0x61,
	0x1F, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x57, 0x50, 0x30, 0xFF, 0xFF, 0x06, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x09, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
	0x1D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x02,
	0x04, 0x06, 0x07, 0x08, 0x0A, 0x0C, 0x0D, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x19, 0x1B, 0x1C, 0x1E,
	0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
	0x27, 0x28, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xF5, 0x01,
};

signed int cust_tpd_calibration_matrix_rotation_normal[] = {
	0, -4096, 3272704, -4096, 0, 5238784, 0, 0 };
signed int cust_tpd_calibration_matrix_rotation_factory[] = {
	0, -4096, 3272704, -4096, 0, 5238784, 0, 0 };

static struct gt9xx_platform_data gt9xx_platform_data = {
	.gtp_debug_on = 0,	/* #define GTP_DEBUG_ON          0 */
	.gtp_debug_array_on = 0,	/* #define GTP_DEBUG_ARRAY_ON    0 */
	.gtp_debug_func_on = 0,	/* #define GTP_DEBUG_FUNC_ON     0 */

	.ctp_cfg_group1 = cust_ctp_cfg_group1,	/* CTP_CFG_GROUP1 */

	.gtp_rst_port = GPIO_CTP_RST_PIN,	/* #define GTP_RST_PORT    GPIO_CTP_RST_PIN */
	.gtp_int_port = GPIO_CTP_EINT_PIN,	/* #define GTP_INT_PORT    GPIO_CTP_EINT_PIN */
	.gtp_rst_m_gpio = GPIO_CTP_RST_PIN_M_GPIO,
	.gtp_int_m_gpio = GPIO_CTP_EINT_PIN_M_GPIO,
	.gtp_int_m_eint = GPIO_CTP_EINT_PIN_M_EINT,

	.gtp_max_height = 1280,	/* #define GTP_MAX_HEIGHT   1280 */
	.gtp_max_width = 800,	/* #define GTP_MAX_WIDTH    800 */
	.gtp_int_trigger = 1,	/* #define GTP_INT_TRIGGER  1 */

	.gtp_esd_check_circle = 2000,	/* #define GTP_ESD_CHECK_CIRCLE  2000 */
	.tpd_power_source_custom = MT65XX_POWER_LDO_VGP6,	/* #define TPD_POWER_SOURCE_CUSTOM  MT65XX_POWER_LDO_VGP6 */

	.tpd_velocity_custom_x = 20,	/* #define TPD_VELOCITY_CUSTOM_X 20 */
	.tpd_velocity_custom_y = 20,	/* #define TPD_VELOCITY_CUSTOM_Y 20 */

	.i2c_master_clock = 300,	/* #define I2C_MASTER_CLOCK              300 */

	.tpd_calibration_matrix_rotation_normal = cust_tpd_calibration_matrix_rotation_normal,	/* #define TPD_CALIBRATION_MATRIX_ROTATION_NORMAL  {0, -4096, 3272704, -4096, 0, 5238784, 0, 0}; */
	.tpd_calibration_matrix_rotation_factory = cust_tpd_calibration_matrix_rotation_factory,	/* #define TPD_CALIBRATION_MATRIX_ROTATION_FACTORY {0, -4096, 3272704, -4096, 0, 5238784, 0, 0}; */
};

#define TPD_I2C_NUMBER 0
static struct i2c_board_info i2c_tpd[] __initdata = {
	{
	 I2C_BOARD_INFO("gt9xx", (0xBA >> 1)),
	 .irq = CUST_EINT_TOUCH_PANEL_NUM,
	 .platform_data = &gt9xx_platform_data,
	 },
};
/*********************** uart setup ******************************/
#include <cust/uart_custom.h>
#define UART_NOT_DEFINED -1
static struct uart_mode_data cust_uart_data = {
#ifdef GPIO_UART_UTXD1_PIN
	.cust_gpio_uart_utxd1 = GPIO_UART_UTXD1_PIN,
#else
	.cust_gpio_uart_utxd1 = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_UTXD2_PIN
	.cust_gpio_uart_utxd2 = GPIO_UART_UTXD2_PIN,
#else
	.cust_gpio_uart_utxd2 = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_UTXD3_PIN
	.cust_gpio_uart_utxd3 = GPIO_UART_UTXD3_PIN,
#else
	.cust_gpio_uart_utxd3 = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_UTXD4_PIN
	/*.cust_gpio_uart_utxd4 = GPIO_UART_UTXD4_PIN,*/
#else
	/*.cust_gpio_uart_utxd4 = UART_NOT_DEFINED,*/
#endif
#ifdef GPIO_UART_URXD1_PIN
	.cust_gpio_uart_urxd1 = GPIO_UART_URXD1_PIN,
#else
	.cust_gpio_uart_urxd1 = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_URXD2_PIN
	.cust_gpio_uart_urxd2 = GPIO_UART_URXD2_PIN,
#else
	.cust_gpio_uart_urxd2 = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_URXD3_PIN
	.cust_gpio_uart_urxd3 = GPIO_UART_URXD3_PIN,
#else
	.cust_gpio_uart_urxd3 = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_URXD4_PIN
	.cust_gpio_uart_urxd4 = GPIO_UART_URXD4_PIN,
#else
	.cust_gpio_uart_urxd4 = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_UTXD1_PIN_M_GPIO
	.cust_gpio_utxd1_mode_gpio = GPIO_UART_UTXD1_PIN_M_GPIO,
#else
	.cust_gpio_utxd1_mode_gpio = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_UTXD2_PIN_M_GPIO
	.cust_gpio_utxd2_mode_gpio = GPIO_UART_UTXD2_PIN_M_GPIO,
#else
	.cust_gpio_utxd2_mode_gpio = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_UTXD3_PIN_M_GPIO
	.cust_gpio_utxd3_mode_gpio = GPIO_UART_UTXD3_PIN_M_GPIO,
#else
	.cust_gpio_utxd3_mode_gpio = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_UTXD4_PIN_M_GPIO
	/*.cust_gpio_utxd4_mode = GPIO_UART_UTXD1_PIN_M_GPIO,*/
#else
	/*.cust_gpio_utxd4_mode_gpio = UART_NOT_DEFINED,*/
#endif
#ifdef GPIO_UART_URXD1_PIN_M_GPIO
	.cust_gpio_urxd1_mode_gpio = GPIO_UART_URXD1_PIN_M_GPIO,
#else
	.cust_gpio_urxd1_mode_gpio = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_URXD2_PIN_M_GPIO
	.cust_gpio_urxd2_mode_gpio = GPIO_UART_URXD2_PIN_M_GPIO,
#else
	.cust_gpio_urxd2_mode_gpio = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_URXD3_PIN_M_GPIO
	.cust_gpio_urxd3_mode_gpio = GPIO_UART_URXD3_PIN_M_GPIO,
#else
	.cust_gpio_urxd3_mode_gpio = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_URXD4_PIN_M_GPIO
	.cust_gpio_urxd4_mode_gpio = GPIO_UART_URXD4_PIN_M_GPIO,
#else
	.cust_gpio_urxd4_mode_gpio = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_UTXD1_PIN_M_UTXD
	.cust_gpio_utxd1_mode_uart = GPIO_UART_UTXD1_PIN_M_UTXD,
#else
	.cust_gpio_uart_utxd1 = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_UTXD2_PIN_M_UTXD
	.cust_gpio_utxd2_mode_uart = GPIO_UART_UTXD2_PIN_M_UTXD,
#else
	.cust_gpio_uart_utxd1 = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_UTXD3_PIN_M_UTXD
	.cust_gpio_utxd3_mode_uart = GPIO_UART_UTXD3_PIN_M_UTXD,
#else
	.cust_gpio_uart_utxd1 = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_URXD1_PIN_M_URXD
	.cust_gpio_urxd1_mode_uart = GPIO_UART_URXD1_PIN_M_URXD,
#else
	.cust_gpio_uart_utxd1 = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_URXD2_PIN_M_URXD
	.cust_gpio_urxd2_mode_uart = GPIO_UART_URXD2_PIN_M_URXD,
#else
	.cust_gpio_uart_utxd1 = UART_NOT_DEFINED,
#endif
#ifdef GPIO_UART_URXD3_PIN_M_URXD
	.cust_gpio_urxd3_mode_uart = GPIO_UART_URXD3_PIN_M_URXD,
#else
	.cust_gpio_uart_utxd1 = UART_NOT_DEFINED,
#endif
};
struct uart_mode_data *get_cust_uart_data(void)
{
	return &cust_uart_data;
}
/*********************** WIFI SDIO Custom setup ******************************/
static const struct mtk_wifi_sdio_custom mtk_wifi_sdio_custom = {
	.pin = GPIO_WIFI_EINT_PIN,
	.num = CUST_EINT_WIFI_NUM,
	.debounce_en = CUST_EINT_WIFI_DEBOUNCE_EN,
	.debounce_cn = CUST_EINT_WIFI_DEBOUNCE_CN,
	.type = CUST_EINT_WIFI_TYPE,
};
/*********************** HDMI Power Custom setup ******************************/
static const struct mtk_hdmi_power_custom mtk_hdmi_pwr_custom = {
	.mtk_hdmi_power_pin = GPIO_HDMI_POWER_CONTROL,
};

/************************* Audio Customization ******************************/

static struct mt_audio_custom_gpio_data audio_custom_gpio_data = {
	.combo_i2s_ck = GPIO_COMBO_I2S_CK_PIN,
	.combo_i2s_ws = GPIO_COMBO_I2S_WS_PIN,
	.combo_i2s_dat = GPIO_COMBO_I2S_DAT_PIN,
	.pcm_daiout = GPIO_PCM_DAIPCMOUT_PIN,
};

/*****************************************************************************/
/*****************************************************************************/
static const struct of_device_id mt_dt_bus_match[] __initconst = {
	{.compatible = "simple-bus",},
	{.compatible = "arm,amba-bus",},
	{}
};

struct mtk_pmic_eint cust_pmic_eint = {
	.gpio_pin = GPIO_PMIC_EINT_PIN,
	.eint_num = CUST_EINT_MT6397_PMIC_NUM,
	.debounce_cn = CUST_EINT_MT6397_PMIC_DEBOUNCE_CN,
	.debounce_en = CUST_EINT_MT6397_PMIC_DEBOUNCE_EN,
	.type = CUST_EINT_MT6397_PMIC_TYPE,
	.is_auto_umask = 0,
};

struct platform_device pmic_mt6397_device = {
	.name = "pmic_mt6397",
	.id = -1,
	.dev = {
		.platform_data = &cust_pmic_eint,
		}
};

void __init mt_pmic_init(void)
{
	int ret;

	ret = platform_device_register(&pmic_mt6397_device);
	if (ret) {
		pr_info("Power/PMIC****[pmic_mt6397_init] Unable to device register(%d)\n", ret);
	}
}

struct lab_i2c_board_info {
	struct i2c_board_info bi;
	int (*init)(struct lab_i2c_board_info *);
};

static struct lab_i2c_board_info i2c_bus0[] = {

};
static struct lab_i2c_board_info i2c_bus1[] = {

};




static struct lab_i2c_board_info i2c_bus3[] = {
#if 0
	{
		.bi = {
			I2C_BOARD_INFO("mpu6515", 0x68),
			.irq = INV_MPU_IRQ_GPIO,
			.platform_data = &mpu_data,
		},
		.init = ariel_mpu_init,
	},
#endif
};

static struct lab_i2c_board_info i2c_bus4[] = {
#if 0
	{
		.bi = {
			I2C_BOARD_INFO("bq24297", 0x6B),
		},
	},
#endif
};
static struct lab_i2c_board_info i2c_bus5[] = {
#if 0
	{
		.bi = {
			I2C_BOARD_INFO("anx7818", 0x38),
		},
	},
#endif
};

struct i2c_board_cfg {
	struct lab_i2c_board_info *info;
	char *name;
	u16	speed;
	u8	bus_id;
	u8  n_dev;
	u16	min_rev;
	u16 max_rev;
};

#define LAB_I2C_BUS_CFG(_bus_id, _bus_devs, _bus_speed) \
.name = #_bus_devs, \
.info = (_bus_devs), \
.bus_id = (_bus_id), \
.speed = (_bus_speed), \
.n_dev = ARRAY_SIZE(_bus_devs)

static struct i2c_board_cfg evb_i2c_config[] = {
	{
		LAB_I2C_BUS_CFG(0, i2c_bus0, 300),
		.max_rev =	(u16)-1,
	},
	{
		LAB_I2C_BUS_CFG(1, i2c_bus1, 300),
		.max_rev =	(u16)-1,
	},
	{
		LAB_I2C_BUS_CFG(3, i2c_bus3, 100),
		.max_rev =	(u16)-1,
	},
	{
		LAB_I2C_BUS_CFG(4, i2c_bus4, 100),
		.max_rev =	(u16)-1,
	},
	{
		LAB_I2C_BUS_CFG(5, i2c_bus5, 100),
		.max_rev =	(u16)-1,
	}
};

int evb_config_i2c(struct i2c_board_cfg *cfg, int n_cfg, u16 brd_rev)
{
	int i, j, err;
	for (i = 0; i < n_cfg; ++i, ++cfg) {
		if (!cfg->info)
			break;
		if (cfg->min_rev > brd_rev && cfg->min_rev != (u16)-1)
			continue;
		if (cfg->max_rev < brd_rev && cfg->min_rev != (u16)-1)
			continue;
		pr_info("i2c#%d [%s]; %d kHz [%d devs]\n",
			cfg->bus_id, cfg->name, cfg->speed, cfg->n_dev);
		/* configure bus speed */
		mt_dev_set_i2c_bus_speed(cfg->bus_id, cfg->speed);
		/* do device-specific init (pinmuxing etc.) */
		for (j = 0; j < cfg->n_dev; ++j) {
			if (cfg->info[j].init) {
				err = cfg->info[j].init(cfg->info);
				if (err) {
					pr_err("Device '%s' init failed; err=%d\n",
						cfg->info[j].bi.type, err);
					goto fail;
				}
			}
			err = i2c_register_board_info(
				cfg->bus_id, &cfg->info[j].bi, 1);
			if (err)
				goto fail;
			pr_debug("Device '%s' registered at i2c%d-%02X\n",
					cfg->info[j].bi.type,
					cfg->bus_id,
					cfg->info[j].bi.addr);
		}
	}
	return 0;
fail:
	pr_err("Failed to register device #%d for bus i2c#%d [%s]; err=%d\n",
		j, cfg->bus_id, cfg->name, err);
	return err;
}

static void __init mt_init(void)
{
	of_platform_populate(NULL, mt_dt_bus_match, NULL, NULL);
	pm_power_off = mt_power_off;

	mt_charger_init();

	mtk_wcn_cmb_sdio_set_custom(&mtk_wifi_sdio_custom);

	mtk_hdmi_power_set_custom(&mtk_hdmi_pwr_custom);

	/* For PMIC */
	mt_pmic_init();

#ifdef CONFIG_MTK_KEYPAD
	mt_kpd_init();
#endif

	i2c_register_board_info(TPD_I2C_NUMBER, i2c_tpd, ARRAY_SIZE(i2c_tpd));
	mt_audio_init(&audio_custom_gpio_data, 0);
	mt_bls_init(NULL);
	evb_config_i2c(
		evb_i2c_config, ARRAY_SIZE(evb_i2c_config), 2);
	/* For Accelerometer */
	i2c_register_board_info(MC_I2C_DEV_BUS, &i2c_mc3210, 1);
	/* For Gyroscope gpio config */
	mpu_gpio_config();
	i2c_register_board_info(MPU_I2C_DEV_BUS, &i2c_mpu3000, 1);
	/* For Magnetometer */
	i2c_register_board_info(AKM_I2C_DEV_BUS, &i2c_akm8963, 1);
	/* For ALSPS */
	#ifdef CONFIG_ALSPS_MTK_STK2203
	stk_gpio_config();
	i2c_register_board_info(STK_I2C_DEV_BUS, &i2c_stk2203, 1);
	#endif
}

static const char * const mt_dt_match[] = { "mediatek,mt8135", NULL };

DT_MACHINE_START(MT8135, "MT8135")
	.smp            = smp_ops(mt65xx_smp_ops),
	.dt_compat      = mt_dt_match,
	.map_io         = mt_map_io,
	.init_irq       = mt_dt_init_irq,
	.init_time      = mt8135_timer_init,
	.init_early     = mt_init_early,
	.init_machine   = mt_init,
	.fixup          = mt_fixup,
	.restart        = arm_machine_restart,
	.reserve        = mt_reserve,
MACHINE_END

/*consys combo chip gpio set start*/
const struct mtk_wcn_combo_gpio combo_gpio_pdata = {
	.ldo = {
#ifdef GPIO_COMBO_6620_LDO_EN_PIN
		.pin_num = GPIO_COMBO_6620_LDO_EN_PIN,
		.mode1 = GPIO_COMBO_6620_LDO_EN_PIN_M_GPIO,
		.mode2 = -1,
		.mode3 = -1,
#else
		.pin_num = -1,
#endif
	},
	.pmu = {
		.pin_num = GPIO_COMBO_PMU_EN_PIN,
		.mode1 = GPIO_COMBO_PMU_EN_PIN_M_GPIO,
		.mode2 = GPIO_COMBO_PMU_EN_PIN_M_EINT,
		.mode3 = -1,
	},

	.pmuv28 = {
#ifdef	GPIO_COMBO_PMUV28_EN_PIN
		.pin_num = GPIO_COMBO_PMUV28_EN_PIN,
		.mode1 = GPIO_COMBO_PMUV28_EN_PIN_M_GPIO,
		.mode2 = -1,
		.mode3 = -1,
#else
		.pin_num = -1,
#endif
	},

	.rst = {
		.pin_num = GPIO_COMBO_RST_PIN,
		.mode1 = GPIO_COMBO_RST_PIN_M_GPIO,
		.mode2 = -1,
		.mode3 = -1,
	},

	.bgf = {
		.eint = {
#ifdef GPIO_COMBO_BGF_EINT_PIN
			.pin_num = GPIO_COMBO_BGF_EINT_PIN,
			.mode1 = GPIO_COMBO_BGF_EINT_PIN_M_GPIO,
			.mode2 = GPIO_COMBO_BGF_EINT_PIN_M_EINT,
			.mode3 = -1,
#else
			.pin_num = -1,
#endif
		},

#ifdef CUST_EINT_COMBO_BGF_NUM
		.num = CUST_EINT_COMBO_BGF_NUM,
		.debounce_cn = CUST_EINT_COMBO_BGF_DEBOUNCE_CN,
		.type = CUST_EINT_COMBO_BGF_TYPE,
		.debounce_en = CUST_EINT_COMBO_BGF_DEBOUNCE_EN,
#else
		.num = -1,
#endif
	},

	.wifi = {
		.eint = {
#ifdef GPIO_WIFI_EINT_PIN
			.pin_num = GPIO_WIFI_EINT_PIN,
			.mode1 = GPIO_WIFI_EINT_PIN_M_GPIO,
			.mode2 = GPIO_WIFI_EINT_PIN_M_EINT,
			.mode3 = -1,
#else
			.pin_num = -1,
#endif
		},
#ifdef CUST_EINT_WIFI_NUM
		.num = CUST_EINT_WIFI_NUM,
		.debounce_cn = CUST_EINT_WIFI_DEBOUNCE_CN,
		.type = CUST_EINT_WIFI_TYPE,
		.debounce_en = CUST_EINT_WIFI_DEBOUNCE_EN,
#else
		.num = -1,
#endif
	},

	.all = {
		.eint = {
#ifdef GPIO_COMBO_ALL_EINT_PIN
			.pin_num = GPIO_COMBO_ALL_EINT_PIN,
			.mode1 = GPIO_COMBO_ALL_EINT_PIN_M_GPIO,
			.mode2 = GPIO_COMBO_ALL_EINT_PIN_M_EINT,
			.mode3 = -1,
#else
			.pin_num = -1,
#endif
		},

#ifdef CUST_EINT_COMBO_ALL_NUM
		.num = CUST_EINT_COMBO_ALL_NUM,
#else
		.num = -1,
#endif
	},

	.uart_rxd = {
#ifdef GPIO_COMBO_URXD_PIN
		.pin_num = GPIO_COMBO_URXD_PIN,
		.mode1 = GPIO_COMBO_URXD_PIN_M_GPIO,
		.mode2 = GPIO_COMBO_URXD_PIN_M_URXD,
		.mode3 = -1,
#else
		.pin_num = -1,
#endif
	},

	.uart_txd = {
#ifdef GPIO_COMBO_UTXD_PIN
		.pin_num = GPIO_COMBO_UTXD_PIN,
		.mode1 = GPIO_COMBO_UTXD_PIN_M_GPIO,
		.mode2 = GPIO_COMBO_UTXD_PIN_M_UTXD,
		.mode3 = -1,
#else
		.pin_num = -1,
#endif
	},

	.pcm_clk = {
#ifdef GPIO_PCM_DAICLK_PIN
		.pin_num = GPIO_PCM_DAICLK_PIN,
		.mode1 = GPIO_PCM_DAICLK_PIN_M_CLK,
		.mode2 = GPIO_PCM_DAICLK_PIN_M_PCM0_CK,
		.mode3 = -1,
#else
		.pin_num = -1,
#endif
	},

	.pcm_out = {
#ifdef GPIO_PCM_DAIPCMOUT_PIN
		.pin_num = GPIO_PCM_DAIPCMOUT_PIN,
		.mode1 = GPIO_PCM_DAIPCMOUT_PIN_M_MRG_I2S_PCM_TX,
		.mode2 = GPIO_PCM_DAIPCMOUT_PIN_M_PCM0_DO,
/*		.mode3 = GPIO_PCM_DAIPCMOUT_PIN_M_DAIPCMOUT,*/
#else
		.pin_num = -1,
#endif
	},

	.pcm_in = {
#ifdef GPIO_PCM_DAIPCMIN_PIN
		.pin_num = GPIO_PCM_DAIPCMIN_PIN,
		.mode1 = GPIO_PCM_DAIPCMIN_PIN_M_MRG_I2S_PCM_RX,
		.mode2 = GPIO_PCM_DAIPCMIN_PIN_M_PCM0_DI,
/*		.mode3 = GPIO_PCM_DAIPCMIN_PIN_M_DAIPCMIN,*/
#else
		.pin_num = -1,
#endif
	},

	.pcm_sync = {
#ifdef GPIO_PCM_DAISYNC_PIN
		.pin_num = GPIO_PCM_DAISYNC_PIN,
		.mode1 = GPIO_PCM_DAISYNC_PIN_M_MRG_I2S_PCM_SYNC,
		.mode2 = GPIO_PCM_DAISYNC_PIN_M_PCM0_WS,
/*		.mode3 = GPIO_PCM_DAISYNC_PIN_M_BTSYNC,*/
#else
		.pin_num = -1,
#endif
	},

	.i2s_ck = {
#ifdef GPIO_COMBO_I2S_CK_PIN
		.pin_num = GPIO_COMBO_I2S_CK_PIN,
		.mode1 = GPIO_COMBO_I2S_CK_PIN_M_GPIO,
		.mode2 = GPIO_COMBO_I2S_CK_PIN_M_CLK,
		.mode3 = GPIO_COMBO_I2S_CK_PIN_M_I2SIN_CK,
/*		.mode4 = GPIO_COMBO_I2S_CK_PIN_M_I2S0_CK,*/
#else
		.pin_num = -1,
#endif
	},

	.i2s_ws = {
#ifdef GPIO_COMBO_I2S_WS_PIN
		.pin_num = GPIO_COMBO_I2S_WS_PIN,
		.mode1 = GPIO_COMBO_I2S_WS_PIN_M_GPIO,
		.mode2 = GPIO_COMBO_I2S_WS_PIN_M_MRG_I2S_PCM_SYNC,
		.mode3 = GPIO_COMBO_I2S_WS_PIN_M_I2SIN_WS,
/*		.mode4 = GPIO_COMBO_I2S_WS_PIN_M_I2S0_WS,*/
#else
		.pin_num = -1,
#endif
	},

	.i2s_dat = {
#ifdef GPIO_COMBO_I2S_DAT_PIN
		.pin_num = GPIO_COMBO_I2S_DAT_PIN,
		.mode1 = GPIO_COMBO_I2S_DAT_PIN_M_GPIO,
		.mode2 = GPIO_COMBO_I2S_DAT_PIN_M_MRG_I2S_PCM_RX,
		.mode3 = GPIO_COMBO_I2S_DAT_PIN_M_I2SIN_DAT,
/*		.mode4 = GPIO_COMBO_I2S_DAT_PIN_M_I2S0_DAT,*/
#else
		.pin_num = -1,
#endif
	},

	.gps_sync = {
#ifdef GPIO_GPS_SYNC_PIN
		.pin_num = GPIO_GPS_SYNC_PIN,
		.mode1 = GPIO_GPS_SYNC_PIN_M_GPIO,
		.mode2 = -1,
		.mode3 = -1,
#else
		.pin_num = -1,
#endif
	},

	.gps_lan = {
#ifdef GPIO_GPS_LNA_PIN
		.pin_num = GPIO_GPS_LNA_PIN,
		.mode1 = GPIO_GPS_LNA_PIN_M_GPIO,
		.mode2 = -1,
		.mode3 = -1,
#else
		.pin_num = -1,
#endif
	},

	.gps_sync_m = {
#ifndef GPIO_GPS_SYNC_PIN_M_GPS_SYNC
		.m = -1,
#else
		.m = GPIO_GPS_SYNC_PIN_M_GPS_SYNC,
#endif

#ifdef GPIO_GPS_SYNC_PIN_M_MD1_GPS_SYNC
		.m_md1 = GPIO_GPS_SYNC_PIN_M_MD1_GPS_SYNC,
#else
		.m_md1 = -1,
#endif

#ifdef GPIO_GPS_SYNC_PIN_M_MD2_GPS_SYNC
		.m_md2 = GPIO_GPS_SYNC_PIN_M_MD2_GPS_SYNC,
#else
		.m_md2 = -1,
#endif
	},
};
EXPORT_SYMBOL(combo_gpio_pdata);

