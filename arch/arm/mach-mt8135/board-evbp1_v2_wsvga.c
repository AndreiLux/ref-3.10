
#include <linux/pm.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <asm/mach-types.h>
#include <mach/mt_board.h>
#include <mach/board.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpio.h>
#include <mach/board-common-audio.h>
#include <mt8135_evbp1_v2_wsvga/cust_gpio_usage.h>
#include <mt8135_evbp1_v2_wsvga/cust_eint.h>
#include <mt8135_evbp1_v2_wsvga/cust_kpd.h>

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
	.direction = 5,
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
	.direction = 5,
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
	.direction = 5,
	.power_id = MT65XX_POWER_NONE,	/*!< LDO is not used */
	.power_vol = VOL_DEFAULT,	/*!< LDO is not used */
};

struct mag_hw *get_cust_mag_hw(void)
{
	return &cust_mag_hw;
}

/************************* Leds Customization ********************************/
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

/********** ALSPS(Ambient Light Sensor) Customization ************************/
#include <cust_alsps.h>
#include <mach/mt_pm_ldo.h>
#include <mach/eint.h>
#include <mach/mt_gpio_def.h>

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
	/* .als_value  = {40, 40, 90,  90, 160, 160,  225,  320,  640,  1280,
	 *   1280,  2600,  2600, 2600,  10240, 10240}, */
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
	.kpd_pmic_lprst_td_value = 1,	/* timeout period. 0: 8sec; 1: 11sec; 2: 14sec; 3: 5sec */
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
#define PK_DBG_FUNC(fmt, arg...)    pr_debug(PFX "%s: " fmt, __func__ , ##arg)

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
		ISP_MCLK1_EN(TRUE);
		mdelay(1);

		pr_debug("Off camera %d GPIO...\n", pinSetIdx);
		pr_debug("RST pin: %d, PDN pin: %d\n", pinSet[pinSetIdx][IDX_PS_CMRST],
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

		pr_debug("Off camera %d GPIO...\n", 1 - pinSetIdx);
		pr_debug("RST pin: %d, PDN pin: %d\n", pinSet[1 - pinSetIdx][IDX_PS_CMRST],
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
			pr_debug("Power on main sensor...\n");
			pr_debug("RST pin: %d, PDN pin: %d\n", pinSet[pinSetIdx][IDX_PS_CMRST],
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
			pr_debug("Power on sub sensor...\n");
			pr_debug("RST pin: %d, PDN pin: %d\n", pinSet[pinSetIdx][IDX_PS_CMRST],
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
		ISP_MCLK1_EN(FALSE);
		mdelay(1);
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
		/* PLS add this ------------------------------------------------ */
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

#define CAM_CAL_I2C_BUSNUM 1
static struct i2c_board_info kd_cam_cal_dev __initdata = { I2C_BOARD_INFO("dummy_cam_cal", 0xAB >> 1) };

#define EEPROM_I2C_BUSNUM 1
static struct i2c_board_info kd_eeprom_dev __initdata = { I2C_BOARD_INFO("dummy_eeprom", 0xA0 >> 1) };

#define SUPPORT_I2C_BUS_NUM1        1
#define SUPPORT_I2C_BUS_NUM2        2

#define CAMERA_HW_DRVNAME1  "kd_camera_hw"
#define CAMERA_HW_DRVNAME2  "kd_camera_hw_bus2"

static struct i2c_board_info i2c_devs1 __initdata = { I2C_BOARD_INFO(CAMERA_HW_DRVNAME1, 0xfe >> 1) };
static struct i2c_board_info i2c_devs2 __initdata = { I2C_BOARD_INFO(CAMERA_HW_DRVNAME2, 0xfe >> 1) };
void mtkcam_imgsensor_i2c_bus_num(int *num1, int *num2)
{
	*num1 = SUPPORT_I2C_BUS_NUM1;
	*num2 = SUPPORT_I2C_BUS_NUM2;
}
/********************* GT8xx TouchScreen Customize ***************************/
#include <cust/cust_gt8xx.h>

static struct gt8xx_platform_data gt8xx_platform_data = {
	.gtp_rst_port = GPIO_CTP_RST_PIN,
	.gtp_int_port = GPIO_CTP_EINT_PIN,
	.gtp_rst_m_gpio = GPIO_CTP_RST_PIN_M_GPIO,
	.gtp_int_m_gpio = GPIO_CTP_EINT_PIN_M_GPIO,
	.gtp_int_m_eint = GPIO_CTP_EINT_PIN_M_EINT,
	.irq = CUST_EINT_TOUCH_PANEL_NUM,
};

#define TPD_I2C_NUMBER 0
static struct i2c_board_info i2c_tpd[] __initdata = {
	{
	 I2C_BOARD_INFO("mtk-tpd", (0xBA >> 1)),
	 .platform_data = &gt8xx_platform_data,
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

struct mt_pinctrl_cfg ariel_pin_cfg[] = {
	{},
};

static void __init mt_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);

	/* arch(3) phase:
	 * - initialize MTK pin manipulation API.
	 * - Create GPIO devices (gpio driver must have been registered at
	 *   post-core(2) phase)
	 * - Initialize and register EINTs.
	 * - configure SoC pins described in the table */

	mt_pinctrl_init(ariel_pin_cfg, NULL);

	pm_power_off = mt_power_off;
	mt_charger_init();

	i2c_register_board_info(TPD_I2C_NUMBER, i2c_tpd, ARRAY_SIZE(i2c_tpd));

	mtk_wcn_cmb_sdio_set_custom(&mtk_wifi_sdio_custom);

	mt_bls_init(NULL);
	mt_audio_init(&audio_custom_gpio_data, 0);

	mtk_hdmi_power_set_custom(&mtk_hdmi_pwr_custom);

	/* For PMIC */
	mt_pmic_init();

#ifdef CONFIG_MTK_KEYPAD
	mt_kpd_init();
#endif

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

	/* For Camera*/
	i2c_register_board_info(CAM_CAL_I2C_BUSNUM, &kd_cam_cal_dev, 1);
	i2c_register_board_info(EEPROM_I2C_BUSNUM, &kd_eeprom_dev, 1);
	i2c_register_board_info(SUPPORT_I2C_BUS_NUM1, &i2c_devs1, 1);
	i2c_register_board_info(SUPPORT_I2C_BUS_NUM2, &i2c_devs2, 1);
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

