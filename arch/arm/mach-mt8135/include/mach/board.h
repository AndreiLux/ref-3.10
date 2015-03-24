#ifndef __ARCH_ARM_MACH_BOARD_H
#define __ARCH_ARM_MACH_BOARD_H

#include <linux/pm.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <cust/board-custom.h>
#include <mach/mt_gpio_def.h>
#include <mach/battery_custom_data.h>

#include <mach/mtk_wcn_cmb_stub.h>

/* WIFI SDIO platform data */
struct mtk_wifi_sdio_data {
	struct mt_pin_info irq;
};

void mtk_wifi_sdio_set_data(const struct mtk_wifi_sdio_data *pdata, int port);

/* HDMI platform data */
struct mtk_hdmi_data {
	struct mt_pin_info pwr;
};
void mtk_hdmi_set_data(const struct mtk_hdmi_data *pdata);

struct mtk_wcn_combo_gpio;
int mtk_combo_init(struct mtk_wcn_combo_gpio *pin_info);

/* MSDC/SDIO */

typedef void (*msdc_sdio_irq_handler_t) (void *);	/* external irq handler */
typedef void (*msdc_pm_callback_t) (void *data, bool power_enable);

#define MSDC_CD_PIN_EN      (1 << 0)	/* card detection pin is wired   */
#define MSDC_WP_PIN_EN      (1 << 1)	/* write protection pin is wired */
#define MSDC_RST_PIN_EN     (1 << 2)	/* emmc reset pin is wired       */
#define MSDC_SDIO_IRQ       (1 << 3)	/* use internal sdio irq (bus)   */
#define MSDC_EXT_SDIO_IRQ   (1 << 4)	/* use external sdio irq         */
#define MSDC_REMOVABLE      (1 << 5)	/* removable slot                */
#define MSDC_SYS_SUSPEND    (1 << 6)	/* suspended by system           */
#define MSDC_HIGHSPEED      (1 << 7)	/* high-speed mode support       */
#define MSDC_UHS1           (1 << 8)	/* uhs-1 mode support            */
#define MSDC_DDR            (1 << 9)	/* ddr mode support              */
#define MSDC_INTERNAL_CLK   (1 << 11)	/* Force Internal clock */

#define MSDC_SMPL_RISING    (0)
#define MSDC_SMPL_FALLING   (1)

#define MSDC_CMD_PIN        (0)
#define MSDC_DAT_PIN        (1)
#define MSDC_CD_PIN         (2)
#define MSDC_WP_PIN         (3)
#define MSDC_RST_PIN        (4)

enum {
	MSDC_CLKSRC_200MHZ = 0
};
#define MSDC_BOOT_EN (1)
#define MSDC_CD_HIGH (1)
#define MSDC_CD_LOW  (0)
enum {
	MSDC_EMMC = 0,
	MSDC_SD = 1,
	MSDC_SDIO = 2
};

enum {
	SDIO_IRQ_FLAG,
	SDIO_WAKE_LOCK_FLAG,
};

struct msdc_pinctrl {
	/* filter */
	u16 timing;
	u16 voltage;
	/* settings */
	u8 clk_drv;
	u8 cmd_drv;
	u8 dat_drv;
};

struct mmcdev_info {
	struct {
		u8 id; /* Manufacturer ID */
		char name[7]; /* Product name */
		union {
			struct {
				u16 timing;
				u16 voltage;
			};
			u32 tag;
		};
	} filter;

	u8 r_smpl;
	u8 d_smpl;
	u8 wd_smpl;
	u8 cmd_rxdly;
	u8 cmd_rrxdly;
	u8 rd_rxdly;
	u8 wr_rxdly;
	u8 dly_sel;
};

struct msdc_hw {
	unsigned char clk_src;	/* host clock source */

	u32 max_pio;
	u32 max_dma_polling;
	u32 max_busy_time;
	u32 max_poll_time;
	u32 poll_sleep_base;

	const struct mmcdev_info *dev_info;
	u32 dev_info_num;
	struct msdc_pinctrl pin_ctl[4]; /* support up to 4 different sets of parameters */

	unsigned long flags;	/* hardware capability flags */
	unsigned long data_pins;	/* data pins */
	unsigned long data_offset;	/* data address offset */
	int shutdown_delay_ms;
	int power_state;

	struct sdio_config {
		struct sdio_iocon {
			u32 dspl;
			u32 w_dspl;
			u32 rspl;
		} iocon;
		struct sdio_pad_tune {
			u32 rrdly;
			u32 rdly;
			u32 wrdly;
		} pad_tune;
		struct sdio_dat_rd {
			u8 dly0[4];
		} dat_rd;
		msdc_pm_callback_t power_enable;
		void *power_data;
		bool enable_tune:1;
		struct mt_pin_info irq;
	} sdio;

	unsigned long host_function;	/* define host function */
	bool boot:1;		/* define boot host */
	bool cd_level:1;	/* card detection level */
	/* config gpio pull mode */
	void (*config_gpio_pin) (int type, int pull);

	/* external power control for card */
	void (*ext_power_on) (void);
	void (*ext_power_off) (void);

	/* external cd irq operations */
	void (*request_cd_eirq) (msdc_sdio_irq_handler_t cd_irq_handler, void *data);
	void (*enable_cd_eirq) (void);
	void (*disable_cd_eirq) (void);
	int (*get_cd_status) (void);
};

extern struct msdc_hw msdc0_hw;
extern struct msdc_hw msdc1_hw;
extern struct msdc_hw msdc2_hw;
extern struct msdc_hw msdc3_hw;

/*GPS driver*/
#define GPS_FLAG_FORCE_OFF  0x0001
struct mt3326_gps_hardware {
	int (*ext_power_on) (int);
	int (*ext_power_off) (int);
};
extern struct mt3326_gps_hardware mt3326_gps_hw;

/* NAND driver */
struct mtk_nand_host_hw {
	unsigned int nfi_bus_width;	/* NFI_BUS_WIDTH */
	unsigned int nfi_access_timing;	/* NFI_ACCESS_TIMING */
	unsigned int nfi_cs_num;	/* NFI_CS_NUM */
	unsigned int nand_sec_size;	/* NAND_SECTOR_SIZE */
	unsigned int nand_sec_shift;	/* NAND_SECTOR_SHIFT */
	unsigned int nand_ecc_size;
	unsigned int nand_ecc_bytes;
	unsigned int nand_ecc_mode;
};
extern struct mtk_nand_host_hw mtk_nand_hw;

/* Keypad driver */
#define KCOL_KROW_MAX  8
struct mtk_kpd_hardware {
	void *kpd_init_keymap;
	u16 kpd_pwrkey_map;
	u16 kpd_key_debounce;
	bool onekey_reboot_normal_mode;	/* ONEKEY_REBOOT_NORMAL_MODE */
	bool twokey_reboot_normal_mode;	/* TWOKEY_REBOOT_NORMAL_MODE */
	bool onekey_reboot_other_mode;	/* ONEKEY_REBOOT_OTHER_MODE */
	bool twokey_reboot_other_mode;	/* TWOKEY_REBOOT_OTHER_MODE */
	bool kpd_pmic_rstkey_map_en;	/* KPD_PMIC_RSTKEY_MAP */
	u16 kpd_pmic_rstkey_map_value;
	bool kpd_pmic_lprst_td_en;	/* KPD_PMIC_LPRST_TD */
	unsigned char kpd_pmic_lprst_td_value;	/* timeout period. 0: 8sec; 1: 11sec; 2: 14sec; 3: 5sec */
	unsigned int kcol[KCOL_KROW_MAX];
	unsigned int krow[KCOL_KROW_MAX];
};

/* Battery */
void mt_battery_init(void);
void mt_custom_battery_init(void);
void mt_charger_init(void);

extern mt_battery_charging_custom_data mt_bat_charging_data;
extern mt_battery_meter_custom_data mt_bat_meter_data;

/* PMIC driver */
struct mtk_pmic_eint {
	struct mt_pin_info irq;
};

#endif				/* __ARCH_ARM_MACH_BOARD_H */
