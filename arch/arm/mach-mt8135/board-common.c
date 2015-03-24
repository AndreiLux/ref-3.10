/* system header files */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/mtd/nand.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/mmc/host.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/setup.h>
#include <asm/atomic.h>

#include <mach/system.h>
#include <mach/board.h>
#include <mach/hardware.h>
#include <mach/mt_gpio_def.h>
#include <mach/mt_bt.h>
#include <mach/eint.h>
#include <mach/rtc.h>
#include <mach/mt_typedefs.h>
#include "board-custom.h"
#include <mach/upmu_hw.h>
#include <mach/pmic_sw.h>
#include <mach/battery_common.h>
#include <mach/mt_spm_sleep.h>

static struct msdc_hw *msdc_wifi_sdio_hw;

static struct msdc_hw *msdc_hw[5] = {
#if defined(CFG_DEV_MSDC0)
	[0] = &msdc0_hw,
#endif
#if defined(CFG_DEV_MSDC1)
	[1] = &msdc1_hw,
#endif
#if defined(CFG_DEV_MSDC2)
	[2] = &msdc2_hw,
#endif
#if defined(CFG_DEV_MSDC3)
	[3] = &msdc3_hw,
#endif
#if defined(CFG_DEV_MSDC4)
	[4] = &msdc4_hw,
#endif
};

static unsigned long mt_power_off_flag;

void mt_power_off_prepare(void)
{
	mt_power_off();
}

/*=======================================================================*/
/* Board Specific Devices Power Management                               */
/*=======================================================================*/
void mt_power_off(void)
{
	int ret_val = 0;
	int reg_val = 0;
	int bbpu_down = 0;

	if (test_and_set_bit(0, &mt_power_off_flag))
		return;

	pr_notice("mt_power_off\n");

	/* Enable CA15 by default for different PMIC behavior */
	pmic_config_interface(VCA15_CON7, 0x1, PMIC_VCA15_EN_MASK, PMIC_VCA15_EN_SHIFT);
	pmic_config_interface(VSRMCA15_CON7, 0x1, PMIC_VSRMCA15_EN_MASK, PMIC_VSRMCA15_EN_SHIFT);
	udelay(200);

	ret_val = pmic_read_interface(VCA15_CON7, &reg_val, 0xFFFF, 0);
	pr_info("Reg[0x%x]=0x%x\n", VCA15_CON7, reg_val);
	ret_val = pmic_read_interface(VSRMCA15_CON7, &reg_val, 0xFFFF, 0);
	pr_info("Reg[0x%x]=0x%x\n", VSRMCA15_CON7, reg_val);

#ifdef CONFIG_MTK_AUTO_POWER_ON_WITH_CHARGER
	if (pmic_chrdet_status() == KAL_TRUE)
		rtc_mark_enter_kpoc();
#endif

	/* pull PWRBB low */
	if (pmic_chrdet_status() == KAL_FALSE) {
		bbpu_down = 1;
		rtc_bbpu_power_down();
	}

	while (1) {
		pr_info("check charger\n");
		if (pmic_chrdet_status() == KAL_TRUE) {
#ifdef CONFIG_MTK_AUTO_POWER_ON_WITH_CHARGER
			arm_pm_restart(0, "enter_kpoc");
#else
			arm_pm_restart(0, "charger");
#endif
		} else if (bbpu_down == 0) {
			bbpu_down = 1;
			rtc_bbpu_power_down();
		}
	}
}

/*=======================================================================*/
/* Board Specific Devices                                                */
/*=======================================================================*/
/*GPS driver*/
/*FIXME: remove mt3326 notation */
struct mt3326_gps_hardware mt3326_gps_hw = {
	.ext_power_on = NULL,
	.ext_power_off = NULL,
};

/*=======================================================================*/
/* Board Specific Devices Init                                           */
/*=======================================================================*/

#ifdef CONFIG_MTK_BT_SUPPORT
void mt_bt_power_on(void)
{
	pr_info("+mt_bt_power_on\n");

#if defined(CONFIG_MTK_COMBO) || defined(CONFIG_MTK_COMBO_MODULE)
	/* combo chip product */
	/*
	 * Ignore rfkill0/state call. Controll BT power on/off through device /dev/stpbt.
	 */
#else
	/* standalone product */
#endif

	pr_info("-mt_bt_power_on\n");
}
EXPORT_SYMBOL(mt_bt_power_on);

void mt_bt_power_off(void)
{
	pr_info("+mt_bt_power_off\n");

#if defined(CONFIG_MTK_COMBO) || defined(CONFIG_MTK_COMBO_MODULE)
	/* combo chip product */
	/*
	 * Ignore rfkill0/state call. Controll BT power on/off through device /dev/stpbt.
	 */
#else
	/* standalone product */
#endif

	pr_info("-mt_bt_power_off\n");
}
EXPORT_SYMBOL(mt_bt_power_off);

int mt_bt_suspend(pm_message_t state)
{
	pr_info("+mt_bt_suspend\n");
	pr_info("-mt_bt_suspend\n");
	return MT_BT_OK;
}

int mt_bt_resume(pm_message_t state)
{
	pr_info("+mt_bt_resume\n");
	pr_info("-mt_bt_resume\n");
	return MT_BT_OK;
}
#endif

#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
void mtk_wifi_sdio_set_data(const struct mtk_wifi_sdio_data *pdata, int port)
{
	if (port < 0 || port >= ARRAY_SIZE(msdc_hw))
		return;

	msdc_wifi_sdio_hw  = msdc_hw[port];

	if (pdata && msdc_wifi_sdio_hw) {
		int err;
		msdc_wifi_sdio_hw->sdio.irq = pdata->irq;
		err = gpio_request(pdata->irq.pin, "WIFI-SDIO-IRQ");
		if (err)
			pr_err("%s: filed to request WiFi SDIO IRQ GPIO%d: err=%d\n",
				__func__, pdata->irq.pin, err);
	}
}

int board_sdio_ctrl(unsigned int sdio_port_num, unsigned int on)
{
	struct msdc_hw *hw = msdc_wifi_sdio_hw;

	/* ignore sdio_port_num, and always use the predefined sdio_hw,
	 * because caller is not passing correct number */

	if (sdio_port_num >= ARRAY_SIZE(msdc_hw) ||
		!hw || !hw->sdio.power_enable || !hw->sdio.power_data)
		return -1;

	if (hw->power_state == on)
		return 0;

	if (on) {
		hw->sdio.power_enable(hw->sdio.power_data, false);
		hw->sdio.power_enable(hw->sdio.power_data, true);
	} else
		hw->sdio.power_enable(hw->sdio.power_data, false);

	return 0;
}
EXPORT_SYMBOL(board_sdio_ctrl);
#endif				/* end of defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) */

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
static const struct mtk_hdmi_data *mtk_hdmi_data;

void mtk_hdmi_set_data(const struct mtk_hdmi_data *pdata)
{
	if (pdata->pwr.valid)
		gpio_request(pdata->pwr.pin, "HDMI-PWR");
	mtk_hdmi_data = pdata;
}
void mt_hdmi_power_ctrl(bool fgen)
{
	const struct mtk_hdmi_data *pdata = mtk_hdmi_data;

	if (!pdata || !pdata->pwr.valid)
		return;

	mt_pin_set_mode_gpio(pdata->pwr.pin);
	gpio_direction_output(pdata->pwr.pin, (fgen) ? 1 : 0);
}
EXPORT_SYMBOL(mt_hdmi_power_ctrl);
#endif
/* Board Specific Devices                                                */
/*=======================================================================*/

/*=======================================================================*/
/* Board Specific Devices Init                                           */
/*=======================================================================*/

/*=======================================================================*/
/* Board Devices Capability                                              */
/*=======================================================================*/
#define MSDC_SDCARD_FLAG  (MSDC_SYS_SUSPEND | MSDC_REMOVABLE | MSDC_HIGHSPEED | MSDC_UHS1 | MSDC_DDR)
/* Please enable/disable SD card MSDC_CD_PIN_EN for customer request */
#define MSDC_SDIO_FLAG    (MSDC_EXT_SDIO_IRQ | MSDC_HIGHSPEED|MSDC_REMOVABLE)
#define MSDC_EMMC_FLAG	  (MSDC_SYS_SUSPEND | MSDC_HIGHSPEED | MSDC_UHS1 | MSDC_DDR)

#if defined(CFG_DEV_MSDC0)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0)
struct msdc_hw msdc0_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 0,
	.cmd_drv = 0,
	.dat_drv = 0,
	.data_pins = 4,
	.data_offset = 0,
	/* MT6620 use External IRQ, wifi uses high speed. here wifi manage his own suspend and resume,
	does not support hot plug*/
	/*MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN | MSDC_REMOVABLE,(this flag is for SD card) */
	.flags = MSDC_SDIO_FLAG,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SDIO,
	.boot = 0,
};
#else
/* different record is chosen each time error happens: listed are "known good" combinations.
 * any device shall be able to find a good fit out of this set */
static const struct mmcdev_info msdc0_devinfo[] = {
	/* high-frequency rules */
	{
		.filter.timing = BIT(MMC_TIMING_MMC_HS200) | BIT(MMC_TIMING_UHS_DDR50) | BIT(MMC_TIMING_UHS_SDR104),
		.dly_sel = 1,
		.r_smpl = MSDC_SMPL_RISING, .wd_smpl = MSDC_SMPL_RISING, .d_smpl = MSDC_SMPL_RISING,
	},
	{
		.filter.timing = BIT(MMC_TIMING_MMC_HS200) | BIT(MMC_TIMING_UHS_DDR50) | BIT(MMC_TIMING_UHS_SDR104),
		.dly_sel = 1,
		.r_smpl = MSDC_SMPL_FALLING, .wd_smpl = MSDC_SMPL_FALLING, .d_smpl = MSDC_SMPL_FALLING,
	},
	/* generic 'catch-all' rules */
	{
		.r_smpl = MSDC_SMPL_RISING, .wd_smpl = MSDC_SMPL_RISING, .d_smpl = MSDC_SMPL_RISING,
	},
	{
		.r_smpl = MSDC_SMPL_FALLING, .wd_smpl = MSDC_SMPL_FALLING, .d_smpl = MSDC_SMPL_FALLING,
	},
};

struct msdc_hw msdc0_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	/* time in usec */
	.max_poll_time = 1000,
	.max_busy_time = 10,

	.dev_info = msdc0_devinfo,
	.dev_info_num = ARRAY_SIZE(msdc0_devinfo),
	.pin_ctl = {
		{
			.timing = BIT(MMC_TIMING_MMC_HS200) | BIT(MMC_TIMING_UHS_DDR50) | BIT(MMC_TIMING_UHS_SDR104),
			.clk_drv = 2,
			.cmd_drv = 2,
			.dat_drv = 1,
		},
		{
			.clk_drv = 4,
			.cmd_drv = 4,
			.dat_drv = 4,
		},
	},
	.data_pins = 8,
	.data_offset = 0,
	.flags = MSDC_EMMC_FLAG,
	.host_function = MSDC_EMMC,
	.boot = MSDC_BOOT_EN,
	.shutdown_delay_ms = 1600,
};
#endif
#endif
#if defined(CFG_DEV_MSDC1)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
struct msdc_hw msdc1_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 0,
	.cmd_drv = 0,
	.dat_drv = 0,
	.data_pins = 4,
	.data_offset = 0,
	/* MT6620 use External IRQ, wifi uses high speed. here wifi manage his own suspend and resume,
	does not support hot plug */
	/* MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN | MSDC_REMOVABLE, */
	.flags = MSDC_SDIO_FLAG,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SDIO,
	.boot = 0,
};
#else

struct msdc_hw msdc1_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 5,
	.cmd_drv = 3,
	.dat_drv = 3,
	.clk_drv_sd_18 = 3,
	.cmd_drv_sd_18 = 3,
	.dat_drv_sd_18 = 3,
	.data_pins = 4,
	.data_offset = 0,
	.flags = MSDC_SDCARD_FLAG | MSDC_CD_PIN_EN | MSDC_REMOVABLE,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SD,
	.boot = 0,
	.cd_level = MSDC_CD_LOW,
};
#endif
#endif
#if defined(CFG_DEV_MSDC2)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
    /* MSDC2 settings for MT66xx combo connectivity chip */
struct msdc_hw msdc2_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 0,
	.cmd_drv = 0,
	.dat_drv = 0,
	.data_pins = 4,
	.data_offset = 0,
	/* MT6620 use External IRQ, wifi uses high speed. here wifi manage his own suspend and resume,
	does not support hot plug */
	.flags = MSDC_SDIO_FLAG,/* MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN | MSDC_REMOVABLE, */
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SDIO,
	.boot = 0,
};
#else

struct msdc_hw msdc2_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 5,
	.cmd_drv = 3,
	.dat_drv = 3,
	.clk_drv_sd_18 = 3,
	.cmd_drv_sd_18 = 3,
	.dat_drv_sd_18 = 3,
	.data_pins = 4,
	.data_offset = 0,
	.flags = MSDC_SDCARD_FLAG,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SD,
	.boot = 0,
	.cd_level = MSDC_CD_LOW,
};
#endif
#endif
#if defined(CFG_DEV_MSDC3)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 3)
    /* MSDC3 settings for MT66xx combo connectivity chip */
static const struct mmcdev_info msdc3_devinfo[] = {
	{
		.r_smpl = MSDC_SMPL_RISING, .wd_smpl = MSDC_SMPL_RISING, .d_smpl = MSDC_SMPL_RISING,
	},
};

struct msdc_hw msdc3_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,

	/* values in bytes */
	.max_pio = 128,
	.max_dma_polling = 32768,
	/* time in usec */
	.max_poll_time = 1000000,
	.max_busy_time = 100,
	.poll_sleep_base = 10,

	.dev_info = msdc3_devinfo,
	.dev_info_num = ARRAY_SIZE(msdc3_devinfo),

	.pin_ctl = {
		{
			.clk_drv = 3,
			.cmd_drv = 3,
			.dat_drv = 3,
		},
	},
	.data_pins = 4,
	.data_offset = 0,
	/* MT6620 use External IRQ, wifi uses high speed. here wifi manage his own suspend and resume,
	does not support hot plug */
	.flags = MSDC_SDIO_FLAG,/* MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN | MSDC_REMOVABLE, */
	.host_function = MSDC_SDIO,
	.boot = 0,
};

#endif
#endif
#if defined(CFG_DEV_MSDC4)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0)
struct msdc_hw msdc4_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 0,
	.cmd_drv = 0,
	.dat_drv = 0,
	.data_pins = 4,
	.data_offset = 0,
	/* MT6620 use External IRQ, wifi uses high speed. here wifi manage his own suspend and resume,
	does not support hot plug */
	/* MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN | MSDC_REMOVABLE,(this flag is for SD card) */
	.flags = MSDC_SDIO_FLAG,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_SDIO,
	.boot = 0,
};
#else
struct msdc_hw msdc4_hw = {
	.clk_src = MSDC_CLKSRC_200MHZ,
	.cmd_edge = MSDC_SMPL_FALLING,
	.rdata_edge = MSDC_SMPL_FALLING,
	.wdata_edge = MSDC_SMPL_FALLING,
	.clk_drv = 2,
	.cmd_drv = 2,
	.dat_drv = 2,
	.data_pins = 8,
	.data_offset = 0,
	.flags = MSDC_EMMC_FLAG,
	.dat0rddly = 0,
	.dat1rddly = 0,
	.dat2rddly = 0,
	.dat3rddly = 0,
	.dat4rddly = 0,
	.dat5rddly = 0,
	.dat6rddly = 0,
	.dat7rddly = 0,
	.datwrddly = 0,
	.cmdrrddly = 0,
	.cmdrddly = 0,
	.host_function = MSDC_EMMC,
	.boot = MSDC_BOOT_EN,
};
#endif
#endif

/* MT6575 NAND Driver */
#if defined(CONFIG_MTK_MTD_NAND)
struct mt6575_nand_host_hw mt6575_nand_hw = {
	.nfi_bus_width = 8,
	.nfi_access_timing = NFI_DEFAULT_ACCESS_TIMING,
	.nfi_cs_num = NFI_CS_NUM,
	.nand_sec_size = 512,
	.nand_sec_shift = 9,
	.nand_ecc_size = 2048,
	.nand_ecc_bytes = 32,
	.nand_ecc_mode = NAND_ECC_HW,
};
#endif

/************************* Vibrator Customization ****************************/
#include <cust_vibrator.h>

static struct vibrator_pdata vibrator_pdata = {
	.vib_timer = 30,
};

static struct platform_device vibrator_device = {
	.name = "mtk_vibrator",
	.id = -1,
	.dev = {
		.platform_data = &vibrator_pdata,
	},
};

static int __init vibr_init(void)
{
	int ret;

	ret = platform_device_register(&vibrator_device);
	if (ret) {
		pr_info("Unable to register vibrator device (%d)\n", ret);
		return ret;
	}

	return 0;
}

static int vibr_exit(void)
{
	platform_device_unregister(&vibrator_device);

	return 0;
}
/*****************************************************************************/

static struct mtk_combo_data wmt_data;

static struct platform_device mtk_wmt_detect_device = {
	.name = "mtk-wmt-detect",
};

static struct platform_device mtk_wmt_device = {
	.name = "mtk-wmt",
};

int __init mtk_combo_init(struct mtk_wcn_combo_gpio *pin_info)
{
	if (!pin_info)
		return -EINVAL;
	wmt_data.pin_info = pin_info;
	mtk_wmt_detect_device.dev.platform_data = &pin_info->pwr;
	mtk_wmt_device.dev.platform_data = &wmt_data;
	platform_device_register(&mtk_wmt_detect_device);
	platform_device_register(&mtk_wmt_device);
	return 0;
}

static int __init board_common_init(void)
{
	vibr_init();

	return 0;
}

static void board_common_exit(void)
{
	vibr_exit();
}

module_init(board_common_init);
module_exit(board_common_exit);

