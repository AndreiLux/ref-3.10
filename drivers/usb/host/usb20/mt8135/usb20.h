#ifndef __USB20_H__
#define __USB20_H__

#include <mach/charging.h>

#define DEVICE_INTTERRUPT 1
#define EINT_CHR_DET_NUM 23


#define U2PHYDTM1  (USB_SIF_BASE+0x800 + 0x6c)
#define ID_PULL_UP 0x0101
#define ID_PHY_RESET 0x3d11

#if defined(CONFIG_MTK_FAN5405_SUPPORT) \
    || defined(CONFIG_MTK_BQ24158_SUPPORT) \
    || defined(CONFIG_MTK_NCP1851_SUPPORT) \
    || defined(CONFIG_MTK_BQ24196_SUPPORT) \
    || defined(CONFIG_MTK_BQ24297_SUPPORT)
#define SWITCH_CHARGER 1
#endif

#if defined(CONFIG_MT8135_FPGA)
#define FPGA_PLATFORM 1
#endif

struct mt_usb_glue {
	struct device *dev;
	struct platform_device *musb;
};

extern bool upmu_is_chr_det(void);

/* specific USB fuctnion */
typedef enum {
	CABLE_MODE_CHRG_ONLY = 0,
	CABLE_MODE_NORMAL,
	CABLE_MODE_HOST_ONLY,
	CABLE_MODE_MAX
} CABLE_MODE;

/* switch charger API*/
#ifdef CONFIG_MTK_FAN5405_SUPPORT
extern void fan5405_set_opa_mode(kal_uint32 val);
extern void fan5405_set_otg_pl(kal_uint32 val);
extern void fan5405_set_otg_en(kal_uint32 val);
extern kal_uint32 fan5405_config_interface_liao(kal_uint8 RegNum, kal_uint8 val);
#elif defined(CONFIG_MTK_NCP1851_SUPPORT) || defined(CONFIG_MTK_BQ24196_SUPPORT) || defined(CONFIG_MTK_BQ24158_SUPPORT) || defined(CONFIG_MTK_BQ24297_SUPPORT)
extern void tbl_charger_otg_vbus(kal_uint32 mode);
#endif

#endif
