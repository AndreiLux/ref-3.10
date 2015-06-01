#define MOD_NO_IN_1_DEVAPC                  16
//#define DEVAPC_MODULE_MAX_NUM               32  
#define DEVAPC_TAG                          "DEVAPC"
#define MAX_TIMEOUT                         100
 
 
// device apc attribute
/* typedef enum
 {
     E_L0=0,
     E_L1,
     E_L2,
     E_L3,
     E_MAX_APC_ATTR
 }APC_ATTR;*/
 
 // device apc index 
/*
 typedef enum
 {
     E_DEVAPC0=0,
     E_DEVAPC1,  
     E_DEVAPC2,
     E_DEVAPC3,
     E_DEVAPC4,
     E_MAX_DEVAPC
 }DEVAPC_NUM;
 */
 // domain index 
/* typedef enum
 {
     E_DOMAIN_0 = 0,
     E_DOMAIN_1 ,
     E_DOMAIN_2 , 
     E_DOMAIN_3 ,
     E_MAX
 }E_MASK_DOM;*/
 
 
 typedef struct {
     const char      *device_name;
     bool            forbidden;
 } DEVICE_INFO;
 
  
  static DEVICE_INFO D_APC0_Devices[] = {
    {"INFRA_AO_TOP_LEVEL_CLOCK_GENERATOR",     FALSE},//0
    {"INFRA_AO_INFRASYS_CONFIG_REGS",          FALSE},
    {"IO_Configuration",                       FALSE},
    {"INFRA_AO_PERISYS_CONFIG_REGS",           TRUE},
    {"INFRA_AO_DRAMC_CONTROLLER",              TRUE},
    {"INFRA_AO_GPIO_CONTROLLER",               FALSE},
    {"INFRA_AO_TOP_LEVEL_SLP_MANAGER",         TRUE},
    {"INFRA_AO_TOP_LEVEL_RESET_GENERATOR",     FALSE},
    {"INFRA_AO_GPT",                           TRUE},
    {"INFRA_AO_RSVD_1",                        TRUE},
    
    {"INFRA_AO_SEJ",                           TRUE},//10
    {"INFRA_AO_APMCU_EINT_CONTROLLER",         TRUE},
    {"INFRASYS_AP_MIXED_CONTROL_REG",          TRUE},
    {"INFRA_AO_PMIC_WRAP_CONTROL_REG",         FALSE},
    {"INFRA_AO_DEVICE_APC_AO",                 TRUE},
    {"INFRA_AO_DDRPHY_CONTROL_REG",            TRUE},
    {"INFRA_AO_KPAD_CONTROL_REG",              TRUE},
    {"INFRA_AO_DRAM_B_CONTROLLER",             TRUE},
    {"INFRA_AO_RSVD_2",                        TRUE},
    {"INFRA_AO_SPM_MD32",                      FALSE},
    
    {"INFRASYS_MCUSYS_CONFIG_REG",             TRUE},//20
    {"INFRASYS_CONTROL_REG",                   TRUE},
    {"INFRASYS_BOOTROM/SRAM",                  TRUE},
    {"INFRASYS_EMI_BUS_INTERFACE",             FALSE},
    {"INFRASYS_SYSTEM_CIRQ",                   TRUE},
    {"INFRASYS_MM_IOMMU_CONFIGURATION",        TRUE},
    {"INFRASYS_EFUSEC",                        FALSE},
    {"INFRASYS_DEVICE_APC_MONITOR",            TRUE},
    {"INFRASYS_MCU_BIU_CONFIGURATION",         TRUE},
    {"INFRASYS_MD1_AP_CCIF",                   TRUE},
    
    {"INFRASYS_MD1_MD_CCIF",                   FALSE},//30
    {"INFRASYS_MD2_AP_CCIF",                   TRUE},
    {"INFRASYS_MD2_MD_CCIF",                   FALSE},
    {"INFRASYS_MBIST_CONTROL_REG",             TRUE},
    {"INFRASYS_DRAMC_NAO_REGION_REG",          TRUE},
    {"INFRASYS_TRNG",                          TRUE},
    {"INFRASYS_GCPU",                          TRUE},
    {"INFRASYS_MD2MD_MD1_CCIF",                FALSE},
    {"INFRASYS_GCE",                           TRUE},
    {"INFRASYS_MD2MD_MD2_CCIF",                FALSE},
    
    {"INFRASYS_PERISYS_IOMMU",                 TRUE},//40
    {"INFRA_AO_ANA_MIPI_DSI0",                 TRUE},
    {"INFRASYS _RSVD_3",                       TRUE},
    {"INFRA_AO_ANA_MIPI_CSI0",                 TRUE},
    {"INFRA_AO_ANA_MIPI_CSI1",                 TRUE},
    {"INFRASYS_RSVD_4",                        TRUE},
    {"DEGBUGSYS",                              TRUE},
    {"DMA",                                    TRUE},
    {"AUXADC",                                 FALSE},
    {"UART0",                                  TRUE},
    
    {"UART1",                                  TRUE},//50
    {"UART2",                                  TRUE},
    {"UART3",                                  TRUE},
    {"PWM",                                    TRUE},
    {"I2C0",                                   TRUE},
    {"I2C1",                                   TRUE},
    {"I2C2",                                   TRUE},
    {"SPI0",                                   TRUE},
    {"PTP",                                    TRUE},
    {"BTIF",                                   TRUE},
    
    {"NFI",                                    TRUE},//60
    {"NFI_ECC",                                TRUE},
    {"PERISYS_RSVD_1",                         TRUE},
    {"PERISYS_RSVD_2",                         TRUE},
    {"USB2.0",                                 FALSE},
    {"USB2.0 SIF",                             FALSE},
    {"AUDIO",                                  FALSE},
    {"MSDC0",                                  TRUE},
    {"MSDC1",                                  TRUE},
    {"MSDC2",                                  TRUE},
      
    {"MSDC3",                                  TRUE},//70
    {"IC_USB",                                 TRUE},
    {"CONN_PERIPHERALS",                       FALSE},
    {"MD_PERIPHERALS",                         TRUE},
    {"MD2_PERIPHERALS",                        TRUE},
    {"MJC_CONFIG",                             TRUE},
    {"MJC_TOP",                                TRUE},
    {"SMI_LARB5",                              TRUE},
    {"MFG_CONFIG",                             TRUE},
    {"G3D_CONFIG",                             TRUE},

    {"G3D_UX_CMN",                             TRUE},//80
    {"G3D_UX_DW0",                             TRUE},
    {"G3D_MX",                                 TRUE},
    {"G3D_IOMMU",                              TRUE},
    {"MALI",                                   TRUE},
    {"MMSYS_CONFIG",                           TRUE},
    {"MDP_RDMA",                               TRUE},
    {"MDP_RSZ0",                               TRUE},
    {"MDP_RSZ1",                               TRUE},
    {"MDP_WDMA",                               TRUE},
    
    {"MDP_WROT",                               TRUE},//90
    {"MDP_TDSHP",                              TRUE},
    {"DISP_OVL0",                              TRUE},
    {"DISP_OVL1",                              TRUE},
    {"DISP_RDMA0",                             TRUE},
    {"DISP_RDMA1",                             TRUE},
    {"DISP_WDMA",                              TRUE},
    {"DISP_COLOR",                             TRUE},
    {"DISP_CCORR",                             TRUE},
    {"DISP_AAL",                               TRUE},
    
    {"DISP_GAMMA",                             TRUE},//100    
    {"DISP_DITHER",                            TRUE},
    {"DSI_UFOE",                               TRUE},
    {"DSI0",                                   TRUE},
    {"DPI0",                                   TRUE},
    {"DISP_PWM0",                              TRUE},
    {"MM_MUTEX",                               TRUE},
    {"SMI_LARB0",                              TRUE},
    {"SMI_COMMON",                             TRUE},
    {"DISP_WDMA1",                             TRUE},

    {"UFOD_RDMA0",                             TRUE},//110
    {"UFOD_RDMA1",                             TRUE},
    {"MM_RSVD1",                               TRUE},
    {"IMGSYS_CONFIG",                          TRUE},    
    {"IMGSYS_SMI_LARB2",                       TRUE},
    {"IMGSYS_SMI_LARB2",                       TRUE},
    {"IMGSYS_SMI_LARB2",                       TRUE},
    {"IMGSYS_CAM0",                            TRUE},
    {"IMGSYS_CAM1",                            TRUE},
    {"IMGSYS_CAM2",                            TRUE},
    
    {"IMGSYS_CAM3",                            TRUE},//120
    {"IMGSYS_SENINF",                          TRUE},
    {"IMGSYS_CAMSV",                           TRUE},
    {"IMGSYS_CAMSV",                           TRUE},
    {"IMGSYS_FD",                              TRUE}, 
    {"IMGSYS_MIPI_RX",                         TRUE},
    {"CAM0",                                   TRUE},
    {"CAM2",                                   TRUE},
    {"CAM3",                                   TRUE},
    {"VDECSYS_GLOBAL_CONFIGURATION",           TRUE},
    
    {"VDECSYS_SMI_LARB1",                      TRUE},//130
    {"VDEC_FULL_TOP",                          TRUE},
    {"VENC_GLOBAL_CON",                        TRUE},
    {"SMI_LARB3",                              TRUE},
    {"VENC",                                   TRUE},
    {"JPGENC",                                 TRUE},
    {"JPGDEC",                                 TRUE},
	{NULL,                                     TRUE},
  };

 
 
 
#define SET_SINGLE_MODULE(apcnum, domnum, index, module, permission_control)     \
 {                                                                               \
     mt_reg_sync_writel(readl(DEVAPC##apcnum##_D##domnum##_APC_##index) & ~(0x3 << (2 * module)), DEVAPC##apcnum##_D##domnum##_APC_##index); \
     mt_reg_sync_writel(readl(DEVAPC##apcnum##_D##domnum##_APC_##index) | (permission_control << (2 * module)),DEVAPC##apcnum##_D##domnum##_APC_##index); \
 }                                                                               \
 
#define UNMASK_SINGLE_MODULE_IRQ(apcnum, domnum, module_index)                  \
 {                                                                               \
     mt_reg_sync_writel(readl(DEVAPC##apcnum##_D##domnum##_VIO_MASK) & ~(module_index),      \
         DEVAPC##apcnum##_D##domnum##_VIO_MASK);                                 \
 }                                                                               \
 
#define CLEAR_SINGLE_VIO_STA(apcnum, domnum, module_index)                     \
 {                                                                               \
     mt_reg_sync_writel(readl(DEVAPC##apcnum##_D##domnum##_VIO_STA) | (module_index),        \
         DEVAPC##apcnum##_D##domnum##_VIO_STA);                                  \
 }                                                                               \

 
