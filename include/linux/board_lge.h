#ifndef __ASM_ARCH_ODIN_BOARD_LGE_H
#define __ASM_ARCH_ODIN_BOARD_LGE_H

typedef enum {
	HW_REV_A = 0,
	HW_REV_B,
	HW_REV_C,
	HW_REV_D,
	HW_REV_E,
	HW_REV_F,
	HW_REV_G,
	HW_REV_H,
	HW_REV_I,
	HW_REV_J,
	HW_REV_1_0,
	HW_REV_1_1,
	HW_REV_1_2,
	HW_REV_MAX
} hw_rev_type;

extern hw_rev_type lge_get_board_revno(void);

typedef enum {
	CHG_CABLETYPE_UNDEFINED = 0,
	CHG_CABLETYPE_SDP,
	CHG_CABLETYPE_DCP,
	CHG_CABLETYPE_CDP,
	CHG_CABLETYPE_56k,
	CHG_CABLETYPE_130k,
	CHG_CABLETYPE_910k,
	CHG_CABLETYPE_MAX
} charger_cabletype;

extern charger_cabletype lge_get_usb_id(void);

/* [START] Test code for hiddenmenu */
struct pseudo_thermal_curr_ctrl_info_type {
    int current_mA;
};
/* [END] */

typedef enum {
	LGE_BOOT_MODE_NORMAL = 0,
	LGE_BOOT_MODE_CHARGER,
	LGE_BOOT_MODE_CHARGERLOGO,
	LGE_BOOT_MODE_FACTORY,		/* qem_130k */
	LGE_BOOT_MODE_FACTORY2,		/* qem_56k */
	LGE_BOOT_MODE_FACTORY3,		/* qem_910k */
	LGE_BOOT_MODE_PIFBOOT,		/* pif_130k */
	LGE_BOOT_MODE_PIFBOOT2,		/* pif_56k */
	LGE_BOOT_MODE_PIFBOOT3,		/* pif_910k */
}lge_boot_mode_type;

typedef enum {
	LGE_BOOT_HW_BOOT = 0,
	LGE_BOOT_HIDDEN_RESET,
	LGE_BOOT_SW_RESET,
	LGE_BOOT_HW_RESET,
	LGE_BOOT_SMPL_BOOT,
	LGE_BOOT_KEY_BOOT,
	LGE_BOOT_CABLE_BOOT,
}lge_boot_reason_type;

typedef enum {
	LGE_LCD_CONNECT = 0,
	LGE_LCD_DISCONNECT,
}lge_lcd_connect_check;


extern lge_boot_mode_type lge_get_boot_mode(void);
extern lge_boot_reason_type lge_get_boot_reason(void);
extern bool lge_get_factory_boot(void);
extern bool lge_get_factory_56k_130k_boot(void);

#define LT_CABLE_56K                6
#define LT_CABLE_130K               7
#define USB_CABLE_400MA             8
#define USB_CABLE_DTC_500MA         9
#define ABNORMAL_USB_CABLE_400MA    10
#define LT_CABLE_910K               11
#define NO_INIT_CABLE               12

extern unsigned int lge_pm_get_cable_info(void);

#define BATT_UNKNOWN    0
#define BATT_DS2704_N   17
#define BATT_DS2704_L   32
#define BATT_DS2704_C   48
#define BATT_ISL6296_N  73
#define BATT_ISL6296_L  94
#define BATT_ISL6296_C  105
#define BATT_NOT_PRESENT 200 /*in case of battery not presence */

extern unsigned int lge_get_batt_id(void);
extern char *lge_get_sidd(void);
extern unsigned int lge_get_fake_battery_mode(void);
extern unsigned int lge_get_lcd_connect(void);

enum lge_laf_mode_type {
	LGE_LAF_MODE_NORMAL = 0,
	LGE_LAF_MODE_LAF,
};

int lge_get_android_dlcomplete(void);
enum lge_laf_mode_type lge_get_laf_mode(void);

#if defined(CONFIG_PRE_SELF_DIAGNOSIS)
struct pre_selfd_platform_data {
	int (*set_values) (int r, int g, int b);
	int (*get_values) (int *r, int *g, int *b);
};
#endif

#if defined(CONFIG_PRE_SELF_DIAGNOSIS)
int lge_pre_self_diagnosis(char *drv_bus_code, int func_code, char *dev_code, char *drv_code, int errno);
#endif

#endif
