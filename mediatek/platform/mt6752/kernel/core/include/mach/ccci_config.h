#ifndef ECCCI_INTERNAL_OPTION
#define ECCCI_INTERNAL_OPTION

#include <mach/mt_reg_base.h>

//================================================ 
// Bool option part
//================================================
//#define CCCI_STATISTIC
#define FEATURE_GET_MD_GPIO_NUM
#define FEATURE_GET_MD_GPIO_VAL
#define FEATURE_GET_MD_ADC_NUM
#define FEATURE_GET_MD_ADC_VAL
#define FEATURE_GET_MD_EINT_ATTR
#define FEATURE_GET_MD_BAT_VOL
#define FEATURE_PM_IPO_H
#define FEATURE_DFO_EN
#define FEATURE_SEQ_CHECK_EN
#define FEATURE_POLL_MD_EN
#if 0 // DEPRECATED
#define FEATURE_GET_TD_EINT_NUM
#define FEATURE_GET_DRAM_TYPE_CLK
#endif

#define ENABLE_EMI_PROTECTION
#define ENABLE_DRAM_API
#define ENABLE_MEM_REMAP_HW
//#define ENABLE_CHIP_VER_CHECK
//#define ENABLE_2G_3G_CHECK
#define ENABLE_MEM_SIZE_CHECK
//#define ENABLE_MD_WDT_DBG
#define FEATURE_CONN_MD_EXP_EN
#define FEATURE_USING_4G_MEMORY_API
#define FEATURE_VLTE_SUPPORT
//#define FEATURE_LOW_BATTERY_SUPPORT //disable for customer complaint
//================================================ 
// Configure value option part
//================================================
#define AP_PLATFORM_INFO    "MT6752E1"
#define CCCI_MTU            (3584-128)
#define CCMNI_MTU           (1500)
#define SKB_POOL_SIZE_4K    (256) // 2*MD
#define SKB_POOL_SIZE_1_5K  (256) // 2*MD
#define SKB_POOL_SIZE_16    (64)   // 2*MD
#define BM_POOL_SIZE        (SKB_POOL_SIZE_4K+SKB_POOL_SIZE_1_5K+SKB_POOL_SIZE_16)
#define RELOAD_TH            3  // reload pool if pool size dropped below 1/RELOAD_TH
#define MD_HEADER_VER_NO    (3)
#define MEM_LAY_OUT_VER     (1)

#define CCCI_MEM_ALIGN      (SZ_32M)
#define CCCI_SMEM_ALIGN_MD1 (0x200000) // 2M
#define CCCI_SMEM_ALIGN_MD2 (0x200000) // 2M

#define CURR_SEC_CCCI_SYNC_VER (1)	// Note: must sync with sec lib, if ccci and sec has dependency change
#define CCCI_DRIVER_VER     0x20110118

#endif
