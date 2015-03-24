#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <asm/mach-types.h>
#include <mach/mt_board.h>
#include <mach/board.h>
#include <mach/devs.h>
#include <mach/board-common-audio.h>
#include <mt8135_stingray/cust_gpio_usage.h>
#include <mt8135_stingray/cust_eint.h>
#include <mt8135_stingray/cust_kpd.h>

#include <mach/mt_gpio.h>


#include <linux/gpio.h>
#include <linux/i2c.h>
#include <uapi/linux/input.h>
#include <linux/cyttsp4_bus.h>
#include <linux/cyttsp4_core.h>
#include <linux/cyttsp4_i2c.h>
#include <linux/cyttsp4_btn.h>
#include <linux/cyttsp4_mt.h>
#ifdef CONFIG_SND_SOC_MAX97236
#include <sound/max97236.h>
#endif

#include <linux/lp855x.h>
#include <linux/mpu.h>
#include <uapi/linux/input.h>
#include <linux/platform_data/anx3618.h>

#include <mach/mt_gpio.h>
#include <mach/mt_gpio_def.h>
#include <mach/mt_cam.h>
#include "mt_clk.h"

#ifdef CONFIG_MTK_SERIAL
#include <mach/board-common-serial.h>
#endif

#ifdef CONFIG_MTK_KEYPAD
#include <mach/mt_typedefs.h>
#include <mach/hal_pub_kpd.h>
#include <mach/mtk_kpd.h>
#endif

#ifdef CONFIG_INPUT_HALL_BU520
#include <linux/input/bu520.h>

#define HALL_1_GPIO_PIN		111
#define HALL_2_GPIO_PIN		119
#endif

#define USB_IDDIG 34
#define SLIMPORT_IRQ_GPIO 84
#define BACKLIGHT_EN_GPIO	76
#define TOUCH_RST_GPIO	103
#define MAX97236_IRQ_GPIO	102	/* GPIO102 -> EINT10 */
#define TOUCH_IRQ_GPIO	109	/* GPIO109 -> EINT5 */
#define GYRO_IRQ_GPIO	118 /* GPIO118 -> EINT2 */

#define MSDC3_CLK	200 /* WIFI SDIO CLK */
#define SPI0_CLK	46 /* SPI "WRAPPER PORT" CLK */

struct lab_i2c_board_info {
	struct i2c_board_info bi;
	int (*init)(struct lab_i2c_board_info *);
};

static int ariel_gpio_to_irq(struct lab_i2c_board_info *info);

#ifdef CONFIG_SND_SOC_MAX97236
static struct max97236_pdata max97236_platform_data __initdata = {
	.irq_gpio = -1,
};
static int ariel_max97236_init(struct lab_i2c_board_info *info)
{
	unsigned gpio_irq = info->bi.irq;
	int err = -ENODEV;

	err = mt_pin_set_mode_eint(gpio_irq);
	if (unlikely(!err))
		err = mt_pin_set_pull(gpio_irq, MT_PIN_PULL_ENABLE_UP);
	if (unlikely(!err))
		err = ariel_gpio_to_irq(info);

	return err;
}
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4
#include <mach/mt_pm_ldo.h>

extern U32 pmic_config_interface(U32, U32, U32, U32);

static void cyttsp4_config_ldo_ariel(void)
{
	/* Config VGP4 to 1800mV Vsel + 100mV Cal */
	pmic_config_interface(0x043C, 0x5B, 0x7F, 5);

	/* Config VGP6to 3000mV Vsel +100mV Cal */
	pmic_config_interface(0x045A, 0x5E, 0x7F, 5);
}

static int cyttsp4_xres_ariel(struct cyttsp4_core_platform_data *pdata, struct device *dev)
{
	const unsigned long rst_gpio = (unsigned long)(pdata->rst_gpio);

	gpio_direction_output(rst_gpio, 1);
	msleep(20);
	gpio_set_value(rst_gpio, 0);
	msleep(40);
	gpio_set_value(rst_gpio, 1);
	msleep(20);
	return 0;
}

static int cyttsp4_wakeup(struct cyttsp4_core_platform_data *pdata,
			  struct device *dev, atomic_t *ignore_irq)
{
	int rc = 0;
	/* TODO: Setup all GPIOs to normal mode here */
	if (ignore_irq)
		atomic_set(ignore_irq, 0);

	return rc;
}

static int cyttsp4_power_ariel(struct cyttsp4_core_platform_data *pdata,
			       int on, struct device *dev, atomic_t *ignore_irq)
{
	int rc = 0;
	const unsigned long rst_gpio = (unsigned long)pdata->rst_gpio;

	if (on) {
		rc = cyttsp4_wakeup(pdata, dev, ignore_irq);
		pmic_ldo_enable(MT65XX_POWER_LDO_VGP4, KAL_TRUE);
		pmic_ldo_enable(MT65XX_POWER_LDO_VGP6, KAL_TRUE);
		mt_set_gpio_out(rst_gpio, GPIO_OUT_ONE);
	} else {
		/* TODO: Place all GPIOs to HiZ mode here */
		atomic_set(ignore_irq, 1);
		mt_set_gpio_out(rst_gpio, GPIO_OUT_ZERO);
		pmic_ldo_enable(MT65XX_POWER_LDO_VGP4, KAL_FALSE);
		pmic_ldo_enable(MT65XX_POWER_LDO_VGP6, KAL_FALSE);
	}

	return rc;
}

static int cyttsp4_init_ariel(struct cyttsp4_core_platform_data *pdata, int on, struct device *dev)
{
	const unsigned long rst_gpio = (unsigned long)pdata->rst_gpio;

	if (likely(on)) {
		/* Keep the touch chip in reset, until AVDD is up */
		gpio_direction_output(rst_gpio, 0);
		gpio_direction_output(rst_gpio, 0);
		gpio_set_value(rst_gpio, 0);

		pmic_ldo_enable(MT65XX_POWER_LDO_VGP4, KAL_FALSE);
		pmic_ldo_enable(MT65XX_POWER_LDO_VGP6, KAL_FALSE);

		cyttsp4_config_ldo_ariel();

		pmic_ldo_enable(MT65XX_POWER_LDO_VGP4, KAL_TRUE);
		pmic_ldo_enable(MT65XX_POWER_LDO_VGP6, KAL_TRUE);

		msleep(20);

		gpio_set_value(rst_gpio, 1);
	} else {
		gpio_set_value(rst_gpio, 0);
		pmic_ldo_enable(MT65XX_POWER_LDO_VGP4, KAL_FALSE);
		pmic_ldo_enable(MT65XX_POWER_LDO_VGP6, KAL_FALSE);
	}

	return 0;
}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE
#include "cyttsp4_img.h"
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
};
#else
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = NULL,
	.size = 0,
	.ver = NULL,
	.vsize = 0,
};
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_TTCONFIG_UPGRADE
#include "cyttsp4_params.h"
static struct touch_settings cyttsp4_sett_param_regs = {
	.data = (uint8_t *) &cyttsp4_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_param_size = {
	.data = (uint8_t *) &cyttsp4_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_param_size),
	.tag = 0,
};

static struct cyttsp4_touch_config cyttsp4_ttconfig = {
	.param_regs = &cyttsp4_sett_param_regs,
	.param_size = &cyttsp4_sett_param_size,
	.fw_ver = ttconfig_fw_ver,
	.fw_vsize = ARRAY_SIZE(ttconfig_fw_ver),
};
#else
static struct cyttsp4_touch_config cyttsp4_ttconfig = {
	.param_regs = NULL,
	.param_size = NULL,
	.fw_ver = NULL,
	.fw_vsize = 0,
};
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_TTCONFIG_UPGRADE */

static struct cyttsp4_loader_platform_data _cyttsp4_loader_platform_data = {
	.fw = &cyttsp4_firmware,
	.ttconfig = &cyttsp4_ttconfig,
	.flags = CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE,
};

static struct cyttsp4_core_platform_data _cyttsp4_core_platform_data = {
	.irq_gpio = TOUCH_IRQ_GPIO,
	.rst_gpio = TOUCH_RST_GPIO,
	.xres = cyttsp4_xres_ariel,
	.init = cyttsp4_init_ariel,
	.power = cyttsp4_power_ariel,
	.detect = NULL,
	.flags = CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE,
	.easy_wakeup_gesture = CY_CORE_EWG_NONE,
	.loader_pdata = &_cyttsp4_loader_platform_data,
};

static struct cyttsp4_core_info cyttsp4_core_info = {
	.name = CYTTSP4_CORE_NAME,
	.id = "main_ttsp_core",
	.adap_id = CYTTSP4_I2C_NAME,
	.platform_data = &_cyttsp4_core_platform_data,
};

#define CY_MAXX 800
#define CY_MAXY 1280
#define CY_MINX 0
#define CY_MINY 0

#define CY_ABS_MIN_X CY_MINX
#define CY_ABS_MIN_Y CY_MINY
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MIN_P 0
#define CY_ABS_MIN_W 0
#define CY_ABS_MAX_P 255
#define CY_ABS_MAX_W 255

#define CY_ABS_MIN_T 0

#define CY_ABS_MAX_T 15

#define CY_IGNORE_VALUE 0xFFFF

static const uint16_t cyttsp4_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
};

struct touch_framework cyttsp4_framework = {
	.abs = (uint16_t *) &cyttsp4_abs[0],
	.size = ARRAY_SIZE(cyttsp4_abs),
	.enable_vkeys = 0,
};

static struct cyttsp4_mt_platform_data _cyttsp4_mt_platform_data = {
	.frmwrk = &cyttsp4_framework,
	.flags = 0,
	.inp_dev_name = CYTTSP4_MT_NAME,
};

struct cyttsp4_device_info cyttsp4_mt_device_info = {
	.name = CYTTSP4_MT_NAME,
	.core_id = "main_ttsp_core",
	.platform_data = &_cyttsp4_mt_platform_data,
};

#endif				/* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4 */
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
struct mt_camera_sensor_config stingray_camera_sensor_conf[] = {
	{
		.name = "ov2724mipiraw",
		.auxclk_name = "ISP_MCLK1",
		.pdn_pin = 184, /* CMPDN */
		.pdn_on = 1,
		.rst_pin = 183,	/*CM_RST */
		.rst_on = 0,
		.vcam_a = MT65XX_POWER_LDO_VCAMA,
		.vcam_d = MT65XX_POWER_LDO_VCAMIO,
		.vcam_io = -1,
	},
	{
		.name = "hi704yuv",
		.mclk_name = "CMMCLK",
		.auxclk_name = "ISP_MCLK1",
		.pdn_pin = 73, /* OK */
		.pdn_on = 1,
		.rst_pin = 74, /* OK; not connected */
		.rst_on = 1,
		.vcam_a = MT65XX_POWER_LDO_VCAMA,
		.vcam_d = MT65XX_POWER_LDO_VCAMIO,
		.vcam_io = -1,
	},
};

/********************* ATMEL TouchScreen Customize ***************************/
#ifdef CONFIG_TOUCHSCREEN_MTK_ATMEL_MXT
#include <drivers/input/touchscreen/atmel_mxt_ts/atmel_mxt_ts.h>

static struct mxt_platform_data atmel_pdata = {
	.reset_gpio = TOUCH_RST_GPIO,
	.irq_gpio = TOUCH_IRQ_GPIO,
};
#endif

/* WiFi SDIO platform data */
static const struct mtk_wifi_sdio_data stingray_wifi_sdio_data = {
	.irq = MTK_EINT_PIN(WIFI_EINT, EINTF_TRIGGER_LOW),
};

/* HDMI platform data */
static const struct mtk_hdmi_data stingray_hdmi_data = {
	.pwr = MTK_GPIO(HDMI_POWER_CONTROL),
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
	.irq = MTK_EINT_PIN(PMIC_EINT, EINTF_TRIGGER_HIGH),
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
		pr_info(
		"Power/PMIC****[pmic_mt6397_init] Unable to device register(%d)\n", ret);
	}
}

#if 0
static int ariel_gpio_to_eint(struct lab_i2c_board_info *info)
{
	u16 gpio_irq = info->bi.irq;
	struct mt_pin_def *pin = mt_get_pin_def(gpio_irq);
	int err = -ENODEV;
	if (pin && pin->eint) {
		err = mt_pin_set_mode_eint(gpio_irq);
		if (!err)
			err = gpio_request(gpio_irq, info->bi.type);
		if (!err)
			err = gpio_direction_input(gpio_irq);
		/* TODO: this is incorrect translation, but compatible
		 * with MTK mt_eint_* code; update, when mt_eint cleanup
		 * is done */
		if (!err)
			info->bi.irq = pin->eint->id;
		else
			info->bi.irq = -1;
	}
	return err;
}
#endif

static void ariel_gpio_init(int gpio, u32 flags, int pull_mode)
{
	if (gpio > 0) {
		int err;
		mt_pin_set_mode_gpio(gpio);
		mt_pin_set_pull(gpio, pull_mode);
		err = gpio_request_one(gpio, flags, "ariel");
		pr_err("GPIO%d: board init done; err=%d\n", gpio, err);
		gpio_free(gpio);
	}
}

static int ariel_gpio_to_irq(struct lab_i2c_board_info *info)
{
	u16 gpio_irq = info->bi.irq;
	int err = mt_pin_set_mode_eint(gpio_irq);
	if (!err)
		err = gpio_request(gpio_irq, info->bi.type);
	if (!err)
		err = gpio_direction_input(gpio_irq);
	if (!err)
		info->bi.irq = gpio_to_irq(gpio_irq);
	if (!err && info->bi.irq < 0)
		err = info->bi.irq;
	if (err)
		info->bi.irq = -1;
	return err;
}

static int ariel_touch_init(struct lab_i2c_board_info *info)
{
	int err;
	u16 gpio_irq = info->bi.irq;

	err = ariel_gpio_to_irq(info);
	if (!err)
		err = mt_pin_set_pull(gpio_irq, MT_PIN_PULL_ENABLE_UP);
	if (!err)
		err = mt_pin_set_mode_gpio(TOUCH_RST_GPIO);
	if (!err)
		err = gpio_request(TOUCH_RST_GPIO, "touch-reset");
	if (!err)
		err = gpio_direction_output(TOUCH_RST_GPIO, 1);
	return err;
}

static struct lp855x_rom_data ariel_lp8557_eeprom_arr[] = {
	{0x14, 0xCF}, /* 4V OV, 4 LED string enabled */
};

static struct lp855x_platform_data ariel_lp8557_pdata = {
	.mode = REGISTER_BASED,
	.device_control = (LP8557_I2C_CONFIG | LP8557_DISABLE_LEDS),
	.initial_brightness = 0x64,
	.max_brightness = 0xFF,
	.load_new_rom_data = 1,
	.size_program = ARRAY_SIZE(ariel_lp8557_eeprom_arr),
	.rom_data = ariel_lp8557_eeprom_arr,
	.gpio_en = BACKLIGHT_EN_GPIO,
};

static struct lab_i2c_board_info i2c_bus0_proto[] = {
#ifdef CONFIG_TOUCHSCREEN_MTK_ATMEL_MXT
	{
		.bi = {
			I2C_BOARD_INFO("atmel_mxt_ts", 0x4C),
			.irq = TOUCH_IRQ_GPIO,
			.platform_data = &atmel_pdata,
		},
		.init = ariel_touch_init,
	},
#endif
};

static struct lab_i2c_board_info i2c_bus0_evt1[] = {
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4
	{
		.bi = {
			I2C_BOARD_INFO(CYTTSP4_I2C_NAME, 0x24),
			.irq = TOUCH_IRQ_GPIO,
			.platform_data = CYTTSP4_I2C_NAME,
		},
		.init = ariel_touch_init,
	},
#endif
};

static struct lab_i2c_board_info i2c_bus1[] = {
#if 0
	{
		.bi = {
			I2C_BOARD_INFO("cm7981", 0x30),
		},
	},
	{
		.bi = {
			I2C_BOARD_INFO("ov2722", 0x36),
		},
	},
#endif
};

static struct lab_i2c_board_info i2c_bus2_proto[] = {
};

static struct lab_i2c_board_info ariel_i2c_bus2_backlight[] = {
	{
		.bi = {
			I2C_BOARD_INFO("lp8557_led", 0x2C),
			.platform_data = &ariel_lp8557_pdata,
		},
	},
};

static struct lab_i2c_board_info i2c_bus2_common[] = {
	{
		.bi = {
			I2C_BOARD_INFO("tmp103_temp_sensor", 0x70),
		},
	},
	{
		.bi = {
			I2C_BOARD_INFO("tmp103_temp_sensor", 0x71),
		},
	},
	{
		.bi = {
			I2C_BOARD_INFO("tmp103_temp_sensor", 0x72),
		},
	},
#ifdef CONFIG_SND_SOC_MAX97236
	{
		.bi = {
			I2C_BOARD_INFO("max97236", 0x40),
			.irq = MAX97236_IRQ_GPIO,
			.platform_data = &max97236_platform_data,
		},
		.init = ariel_max97236_init,
	},
#endif
#if 0
	{
		.bi = {
			I2C_BOARD_INFO("nt50357", 0x21),
		},
	},
#endif
};

static int ariel_mpu_init(struct lab_i2c_board_info *info)
{
	unsigned gpio_irq = info->bi.irq;
	int err;
	/* EINT is not used; use PD mode to save power */
	err = mt_pin_set_mode_gpio(gpio_irq);
	if (!err)
		err = mt_pin_set_pull(gpio_irq, MT_PIN_PULL_ENABLE_DOWN);
	if (!err)
		err = ariel_gpio_to_irq(info);
	return err;
}

static struct mpu_platform_data mpu_data = {
	.orientation = {
		1,  0,  0,
		0,  1,  0,
		0,  0,  1
	},
	.secondary_orientation = {
		0,  1,  0,
		1,  0,  0,
		0,  0, -1
	},
};

static struct lab_i2c_board_info i2c_bus3[] = {
	{
		.bi = {
			I2C_BOARD_INFO("mpu6515", 0x68),
			.irq = GYRO_IRQ_GPIO,
			.platform_data = &mpu_data,
		},
		.init = ariel_mpu_init,
	},
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

/*
 * FIXME: This has got to go away.
 *
 * GPIO87 can be configured to CLKMGR mode
 * Set 1 to bit 16 of the undocumented address 0xF0000220
 * enables 26MHz clock out put according to MTK
 */
static void ariel_slimport_clk_enable(bool enable)
{
	if (enable)
		writel(0x10000, (void __iomem *)0xF0000220);
	else
		writel(0x0, (void __iomem *)0xF0000220);
}

static struct anx3618_platform_data anx3618_data = {
	.gpio_hdmi      = 114,
	.gpio_vmch	= 115,
	.gpio_usb_plug  = 64,
	.gpio_pd        = 85,
	.gpio_rst       = 83,
	.gpio_cable_det = SLIMPORT_IRQ_GPIO,
	.gpio_intp      = 88,
	.gpio_usb_det   = 86,
	.gpio_1_0_v_on  = 82,
	.gpio_extclk	= 87,
	.vreg_3_3v      = MT65XX_POWER_LDO_VMCH,
	.vreg_gpio_1_0v = MT65XX_POWER_LDO_VMC,
	.vreg_gpio_1_0v_config = VOL_1800,
	.anx3618_clk_enable = ariel_slimport_clk_enable,
};

static struct anx3618_platform_data anx3618_evt1_data = {
	.gpio_hdmi      = 114,
	.gpio_vmch	= 115,
	.gpio_usb_plug  = 64,
	.gpio_pd        = 85,
	.gpio_rst       = 83,
	.gpio_cable_det = SLIMPORT_IRQ_GPIO,
	.gpio_intp      = 88,
	.gpio_usb_det   = 86,
	.gpio_sel_uart  = 87,
	.gpio_1_0_v_on  = 82,
	.vreg_3_3v      = MT65XX_POWER_LDO_VMCH,
	.vreg_gpio_1_0v = MT65XX_POWER_LDO_VMC,
	.vreg_gpio_1_0v_config = VOL_1800,
};

static int anx_init(struct lab_i2c_board_info *info)
{
	struct anx3618_platform_data *pdata = info->bi.platform_data;
	ariel_gpio_init(pdata->gpio_1_0_v_on,
		GPIOF_DIR_OUT,	MT_PIN_PULL_DISABLE);
	ariel_gpio_init(pdata->gpio_rst,
		GPIOF_DIR_OUT,	MT_PIN_PULL_DISABLE);
	ariel_gpio_init(pdata->gpio_hdmi,
		GPIOF_DIR_OUT,	MT_PIN_PULL_DISABLE);
	ariel_gpio_init(pdata->gpio_vmch,
		GPIOF_DIR_OUT,	MT_PIN_PULL_DISABLE);
	ariel_gpio_init(pdata->gpio_usb_plug,
		GPIOF_DIR_OUT,	MT_PIN_PULL_DISABLE);
	ariel_gpio_init(pdata->gpio_pd,
		GPIOF_DIR_OUT,	MT_PIN_PULL_DISABLE);
	ariel_gpio_init(pdata->gpio_sel_uart,
		GPIOF_DIR_IN,	MT_PIN_PULL_ENABLE_DOWN);
	ariel_gpio_init(pdata->gpio_cable_det,
		GPIOF_DIR_IN,	MT_PIN_PULL_ENABLE_UP);
	ariel_gpio_init(pdata->gpio_intp,
		GPIOF_DIR_IN,	MT_PIN_PULL_ENABLE_UP);
	ariel_gpio_init(pdata->gpio_usb_det,
		GPIOF_DIR_IN,	MT_PIN_PULL_DISABLE);
	ariel_gpio_to_irq(info);

	/* FIXME: Set GPIO87 to MODE5 for CLKMGR */
	if (pdata->gpio_extclk == 87)
		mt_pin_set_mode_by_name(pdata->gpio_extclk, "CLKM");
	mt_pin_set_mode_eint(pdata->gpio_intp);
	return 0;
}

static struct lab_i2c_board_info i2c_bus5[] = {
	{
		.bi = {
			I2C_BOARD_INFO("anx3618", 0x39),
			.irq = SLIMPORT_IRQ_GPIO,
			.platform_data = &anx3618_data,
		},
		.init = anx_init,
	},
};

static struct lab_i2c_board_info i2c_bus5_evt1[] = {
	{
		.bi = {
			I2C_BOARD_INFO("anx3618", 0x39),
			.irq = SLIMPORT_IRQ_GPIO,
			.platform_data = &anx3618_evt1_data,
		},
		.init = anx_init,
	},
};

struct i2c_board_cfg {
	struct lab_i2c_board_info *info;
	char *name;
	u16	speed;
	u8	bus_id;
	u8  n_dev;
	u16	min_rev;
	u16 max_rev;
	u16 *wrrd;
};

#define LAB_I2C_BUS_CFG(_bus_id, _bus_devs, _bus_speed) \
.name = #_bus_devs, \
.info = (_bus_devs), \
.bus_id = (_bus_id), \
.speed = (_bus_speed), \
.n_dev = ARRAY_SIZE(_bus_devs)

/* both aston and ariel */
static u16 i2c_bus2_wrrd[] = {
	0x40, /* MAX97236 needs WRRD (I2C repeated start) */
	0
};

static struct i2c_board_cfg ariel_i2c_config[] = {
	{
		LAB_I2C_BUS_CFG(0, i2c_bus0_proto, 100),
		.max_rev =	BOARD_REV_ARIEL_PROTO_2_0,
	},
	{
		LAB_I2C_BUS_CFG(0, i2c_bus0_evt1, 100),
		.min_rev =	BOARD_REV_ARIEL_PROTO_2_1,
		.max_rev =	(u16)-1,
	},
	{
		LAB_I2C_BUS_CFG(1, i2c_bus1, 300),
		.max_rev =	(u16)-1,
	},
	{
		LAB_I2C_BUS_CFG(2, i2c_bus2_proto, 100),
		.max_rev =	BOARD_REV_ARIEL_PROTO_2_0,
	},
	{
		LAB_I2C_BUS_CFG(2, i2c_bus2_common, 100),
		.max_rev =	(u16)-1,
		.wrrd    =  i2c_bus2_wrrd,
	},
	{
		LAB_I2C_BUS_CFG(2, ariel_i2c_bus2_backlight, 100),
		.max_rev =      (u16)-1,
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
		LAB_I2C_BUS_CFG(5, i2c_bus5, 300),
		.min_rev =	BOARD_REV_ARIEL_EVT_1_1,
		.max_rev =	(u16)-1,
	},
	{
		LAB_I2C_BUS_CFG(5, i2c_bus5_evt1, 300),
		.max_rev =	BOARD_REV_ARIEL_EVT_1_0,
	}
};

static int __init ariel_config_i2c(struct i2c_board_cfg *bus_cfg, int n_cfg, u16 brd_rev)
{
	int i, j, err;
	for (i = 0; i < n_cfg; ++i, ++bus_cfg) {
		if (!bus_cfg->info)
			break;
		if (bus_cfg->min_rev > brd_rev && bus_cfg->min_rev != (u16)-1)
			continue;
		if (bus_cfg->max_rev < brd_rev && bus_cfg->min_rev != (u16)-1)
			continue;
		pr_info("i2c#%d [%s]; %d kHz [%d devs]\n",
				bus_cfg->bus_id, bus_cfg->name,
				bus_cfg->speed, bus_cfg->n_dev);
		/* configure bus speed */
		mt_dev_i2c_bus_configure(
			bus_cfg->bus_id, bus_cfg->speed, bus_cfg->wrrd);
		/* do device-specific init (pinmuxing etc.) */
		for (j = 0; j < bus_cfg->n_dev; ++j) {
			struct lab_i2c_board_info *dev_cfg = &bus_cfg->info[j];
			if (dev_cfg->init) {
				err = dev_cfg->init(dev_cfg);
				if (err) {
					pr_err("Device '%s' init failed; err=%d\n",
							dev_cfg->bi.type, err);
					goto fail;
				}
			}
			err = i2c_register_board_info(
					bus_cfg->bus_id, &dev_cfg->bi, 1);
			if (err)
				goto fail;
			pr_debug("Device '%s' registered at i2c%d-%02X\n",
					dev_cfg->bi.type,
					bus_cfg->bus_id,
					dev_cfg->bi.addr);
		}
	}
	return 0;
fail:
	pr_err("Failed to register device #%d for bus i2c#%d [%s]; err=%d\n",
		j, bus_cfg->bus_id, bus_cfg->name, err);
	return err;
}

struct mt_pinctrl_cfg ariel_pin_cfg[] = {
	{ .gpio = USB_IDDIG, .pull = MT_PIN_PULL_DISABLE, .mode = MT_MODE_MAIN, },
	{ .gpio = MSDC3_CLK, .load_ma = 6, .mode = MT_MODE_MAIN, },
	{}
};

#ifdef CONFIG_INPUT_HALL_BU520
static struct hall_sensor_data hall_sensor_data[] = {
	{
	 .gpio_pin = HALL_1_GPIO_PIN,
	 .name = "hall_sensor_1",
	 .gpio_state = 1,
	},
	{
	 .gpio_pin = HALL_2_GPIO_PIN,
	 .name = "hall_sensor_2",
	 .gpio_state = 1,
	},
};
static struct hall_sensors hall_sensors = {
	.hall_sensor_data = hall_sensor_data,
	.hall_sensor_num = ARRAY_SIZE(hall_sensor_data),
};
static struct platform_device hall_bu520_device = {
	.name = "hall-bu520",
	.id = -1,
	.dev = {
		.platform_data = &hall_sensors,
		}
};

static void __init hall_bu520_init(void)
{
	int i;
	int gpio_pin;

	for (i = 0; i < hall_sensors.hall_sensor_num; i++) {
		gpio_pin = hall_sensors.hall_sensor_data[i].gpio_pin;
		mt_pin_set_mode_eint(gpio_pin);
		mt_pin_set_pull(gpio_pin, MT_PIN_PULL_ENABLE_UP);
		gpio_request(gpio_pin, hall_sensors.hall_sensor_data[i].name);
		gpio_direction_input(gpio_pin);
	}
	platform_device_register(&hall_bu520_device);
}
#endif

#ifdef CONFIG_MTK_SERIAL
/*********************** uart setup ******************************/
static struct mtk_uart_mode_data mt_uart1_data = {
	.uart = {
		.txd = MTK_GPIO_PIN(UART_UTXD1),
		.rxd = MTK_GPIO_PIN(UART_URXD1),
	},
};

static struct mtk_uart_mode_data mt_uart2_data = {
	.uart = {
		.txd = MTK_GPIO_PIN(UART_UTXD2),
		.rxd = MTK_GPIO_PIN(UART_URXD2),
	},
};

static struct mtk_uart_mode_data mt_uart3_data = {
	.uart = {
		.txd = MTK_GPIO_PIN(UART_UTXD3),
		.rxd = MTK_GPIO_PIN(UART_URXD3),
	},
};

static struct mtk_uart_mode_data mt_uart4_data = {
	.uart = {
	/*	.txd = MTK_GPIO_PIN(UART_UTXD4), */
		.rxd = MTK_GPIO_PIN(UART_URXD4),
	},
};

static void __init mt_ariel_serial_init(void)
{
	mt_register_serial(0, &mt_uart1_data);
	mt_register_serial(1, &mt_uart2_data);
	mt_register_serial(2, &mt_uart3_data);
	mt_register_serial(3, &mt_uart4_data);
}
#endif

/* AP: NOTE:
 *
 * this macro is a workaround for a problem, that detection method is using SDIO
 * interrupt pin, which could be used by SDIO driver at that same time.
 * It is safe to assume the we have the chip in a board file, that is defined
 * for a board with this chip.
 * */
#define COMBO_CHIP_ASSUME_DETECTED

static struct mtk_wcn_combo_gpio stingray_combo_gpio_data = {
	.conf = { .merge = true, .pcm = true },
	.pwr = {
		.pmu = MTK_GPIO_PIN(COMBO_PMU_EN),
		.rst = MTK_GPIO_PIN(COMBO_RST),
#ifndef COMBO_CHIP_ASSUME_DETECTED
		.det = MTK_GPIO_PIN(WIFI_EINT),
#endif
	},

	.eint.bgf = MTK_EINT_PIN(COMBO_BGF_EINT, EINTF_TRIGGER_LOW),

	.gps.lna = MTK_GPIO_PIN(GPS_LNA),

	.uart = {
		.rxd = MTK_GPIO_PIN(COMBO_URXD),
		.txd = MTK_GPIO_PIN(COMBO_UTXD),
	},

	.pcm = {
		.clk = MTK_GPIO_PIN(PCM_DAICLK),
		.out = MTK_GPIO_PIN(PCM_DAIPCMOUT),
		.in = MTK_GPIO_PIN(PCM_DAIPCMIN),
		.sync = MTK_GPIO_PIN(PCM_DAISYNC),
	},
};

static void __init stingray_init(void)
{
	pr_info("board_rev = 0x%x", BOARD_REV_STRINGRAY_1_0);

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

	mtk_wifi_sdio_set_data(&stingray_wifi_sdio_data, CONFIG_MTK_WCN_CMB_SDIO_SLOT);
	mtk_hdmi_set_data(&stingray_hdmi_data);

	mtk_combo_init(&stingray_combo_gpio_data);

	/* For PMIC */
	mt_pmic_init();

#ifdef CONFIG_MTK_KEYPAD
	mt_kpd_init();
#endif

	ariel_config_i2c(
		ariel_i2c_config, ARRAY_SIZE(ariel_i2c_config), BOARD_REV_STRINGRAY_1_0);

	mt_bls_init(NULL);
	mt_audio_init(&audio_custom_gpio_data, 0);

#ifdef CONFIG_INPUT_HALL_BU520
	hall_bu520_init();
#endif
#ifdef CONFIG_MTK_SERIAL
	mt_ariel_serial_init();
#endif
	mt_clk_init();
	mt_cam_init(stingray_camera_sensor_conf,
		ARRAY_SIZE(stingray_camera_sensor_conf));
}

static const char *const stingray_dt_match[] = { "mediatek,mt8135-stingray", NULL };

DT_MACHINE_START(MT8135, "MT8135")
	.smp            = smp_ops(mt65xx_smp_ops),
	.dt_compat      = stingray_dt_match,
	.map_io         = mt_map_io,
	.init_irq       = mt_dt_init_irq,
	.init_time      = mt8135_timer_init,
	.init_early     = mt_init_early,
	.init_machine   = stingray_init,
	.fixup          = mt_fixup,
	.restart        = arm_machine_restart,
	.reserve        = mt_reserve,
MACHINE_END
