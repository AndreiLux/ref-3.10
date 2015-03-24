#ifdef CONFIG_MTK_HIBERNATION

#include "mach/mtk_hibernate_dpm.h"

extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);

#endif

#define MOD_NO_IN_1_DEVAPC                  16
#define DEVAPC_MODULE_MAX_NUM               32
#define DEVAPC_TAG                          "DEVAPC"
#define ABORT_EMI                           0x08000008


/* device apc attribute */
typedef enum {
	E_L0 = 0,
	E_L1,
	E_L2,
	E_L3,
	E_MAX_APC_ATTR
} APC_ATTR;

 /* device apc index */
typedef enum {
	E_DEVAPC0 = 0,
	E_DEVAPC1,
	E_DEVAPC2,
	E_DEVAPC3,
	E_DEVAPC4,
	E_MAX_DEVAPC
} DEVAPC_NUM;

 /* domain index */
typedef enum {
	E_AP_MCU = 0,
	E_MD1_MCU,
	E_MD2_MCU,
	E_MM_MCU,
	E_MAX
} E_MASK_DOM;


typedef struct {
	const char *device_name;
	bool protect;
} DEVICE_INFO;


static DEVICE_INFO D_APC0_Devices[] = {
	{"AP DMA", FALSE},
	{"NFI", FALSE},
	{"NFI ECC", FALSE},
	{"AUXADC", FALSE},
	{"IRDA", FALSE},
	{"FHCTL", FALSE},
	{"UART0", FALSE},
	{"UART1", FALSE},
	{"UART2", FALSE},
	{"UART3", FALSE},
	{"NLI ARB", FALSE},
	{"PWM", FALSE},
	{"PTP_THERM_CTRL", FALSE},
	{"I2C0", FALSE},
	{"I2C1", FALSE},
	{"I2C2", FALSE},
	{"I2C3", FALSE},
	{"I2C4", FALSE},
	{"I2C5", FALSE},
	{"I2C6", FALSE},
	{"AP HIF", FALSE},
	{"MD HIF", FALSE},
	{"SPI1", FALSE},
	{"PERI PWRAP bridge", FALSE},
	{"GCPU", FALSE},
	{"GCPU", FALSE},
	{"CA15_PTP_THERM_CTRL", FALSE},
	{"AP_DEVICE_APC_0", FALSE},
	{NULL, FALSE},
};

static DEVICE_INFO D_APC1_Devices[] = {
	{"MCUSYS Configuration Registers", FALSE},
	{"INFRA Configuration Registers", FALSE},
	{"SMI Common", FALSE},
	{"EMI Bus Interface", FALSE},
	{"SMI Local Arbitor", FALSE},
	{"M4U", FALSE},
	{"MFG Configuration Register", FALSE},
	{"Device APC", FALSE},
	{"(Reserved)", FALSE},
	{"APMIXED", FALSE},
	{"(Reserved)", FALSE},
	{"(Reserved)", FALSE},
	{"(Reserved)", FALSE},
	{"Infra_Perisys MBIST Controller", FALSE},
	{"(Reserved)", FALSE},
	{"(Reserved)", FALSE},
	{"Debug System", FALSE},
	{"AP_DEVICE_APC_1", FALSE},
	{"SRAMROM", FALSE},
	{"AP_DMA", FALSE},
	{"AP_AO_DEVICE_APC_0", FALSE},
	{"AP_AO_DEVICE_APC_1", FALSE},
	{"AP_AO_DEVICE_APC_2", FALSE},
	{"AP_AO_DEVICE_APC_3", FALSE},
	{"AP_AO_DEVICE_APC_4", FALSE},
	{"M4U", FALSE},
	{"DISP_CMDQ", FALSE},
	{"EMI", FALSE},
	{NULL, FALSE},
};


static DEVICE_INFO D_APC2_Devices[] = {
	{"Top-level Reset Generator", FALSE},
	{"INFRA AO Configuration Registers", FALSE},
	{"BootROM/SRAM", FALSE},
	{"Perisys Configuration Register", FALSE},
	{"DRAM Controller 0", FALSE},
	{"GPIO Controller", FALSE},
	{"Top-level Sleep Manager", FALSE},
	{"(Reserved)", FALSE},
	{"GPT", FALSE},
	{"EFuse Controller", FALSE},
	{"SEJ", FALSE},
	{"APMCU External Interrupt Controller", FALSE},
	{"(Reserved)", FALSE},
	{"(Reserved)", FALSE},
	{"SMI Control Register", FALSE},
	{"PMIC Wrapper", FALSE},
	{"Device APC AO", FALSE},
	{"DDR PHY Control Register", FALSE},
	{"MIPI Configuration Register", FALSE},
	{"(Reserved)", FALSE},
	{"(Reserved)", FALSE},
	{"KP", FALSE},
	{"USB20 0", FALSE},
	{"USB20 1", FALSE},
	{"MSDC0", FALSE},
	{"MSDC1", FALSE},
	{"MSDC2", FALSE},
	{"MSDC3", FALSE},
	{"MSDC4", FALSE},
	{"USB SIF", FALSE},
	{"AP_DEVICE_APC_2", FALSE},
	{NULL, FALSE},
};


static DEVICE_INFO D_APC3_Devices[] = {
	{"DISP Config", FALSE},
	{"DISP ROT", FALSE},
	{"DISP SCL", FALSE},
	{"DISP OVL", FALSE},
	{"DISP WDMA0", FALSE},
	{"DISP WDMA1", FALSE},
	{"DISP RDMA0", FALSE},
	{"DISP RDMA1", FALSE},
	{"DISP BLS", FALSE},
	{"DISP GAMMA", FALSE},
	{"DISP COLOR", FALSE},
	{"DISP TDSHP", FALSE},
	{"DISP DBI", FALSE},
	{"DISP DSI", FALSE},
	{"DISP DPI0", FALSE},
	{"DISP DPI1", FALSE},
	{"DISP SMI LARB2", FALSE},
	{"DISP MUTEX", FALSE},
	{"DISP CMDQ", FALSE},
	{"DISP G2D", FALSE},
	{"DISP I2C", FALSE},
	{"AP_DEVICE_APC_3", FALSE},
	{NULL, FALSE},
};


static DEVICE_INFO D_APC4_Devices[] = {
	{"IMGSYS CONFIG", FALSE},
	{"IMGSYS SMI LARB3", FALSE},
	{"IMGSYS SMI LARB4", FALSE},
	{"IMGSYS ISPSYS", FALSE},
	{"IMGSYS CAM 0", FALSE},
	{"IMGSYS CAM 1", FALSE},
	{"(Reserved)", FALSE},
	{"(Reserved)", FALSE},
	{"IMGSYS SENINF", FALSE},
	{"IMGSYS JPG DEC", FALSE},
	{"IMGSYS JPG ENC", FALSE},
	{"(Reserved)", FALSE},
	{"MIPI_RX_CONFIG", FALSE},
	{"(Reserved)", FALSE},
	{"(Reserved)", FALSE},
	{"(Reserved)", FALSE},
	{"VDEC Global Control", FALSE},
	{"VDEC SMI LARB1", FALSE},
	{"VDEC", FALSE},
	{"VENC Global Control", FALSE},
	{"VENC SMI LARB0", FALSE},
	{"VENC", FALSE},
	{"Audio", FALSE},
	{"MFG", FALSE},
	{"AP_DEVICE_APC_4", FALSE},
	{NULL, FALSE},
};


#define SET_SINGLE_MODULE(apcnum, domnum, index, module, permission_control)     \
 {                                                                               \
     mt65xx_reg_sync_writel(readl(DEVAPC##apcnum##_D##domnum##_APC_##index) & ~(0x3 << (2 * module)), DEVAPC##apcnum##_D##domnum##_APC_##index); \
     mt65xx_reg_sync_writel(readl(DEVAPC##apcnum##_D##domnum##_APC_##index) | (permission_control << (2 * module)), DEVAPC##apcnum##_D##domnum##_APC_##index); \
 }                                                                               \

#define UNMASK_SINGLE_MODULE_IRQ(apcnum, domnum, module_index)                  \
 {                                                                               \
     mt65xx_reg_sync_writel(readl(DEVAPC##apcnum##_D##domnum##_VIO_MASK) & ~(module_index),      \
	 DEVAPC##apcnum##_D##domnum##_VIO_MASK);                                 \
 }                                                                               \

#define CLEAR_SINGLE_VIO_STA(apcnum, domnum, module_index)                     \
 {                                                                               \
     mt65xx_reg_sync_writel(readl(DEVAPC##apcnum##_D##domnum##_VIO_STA) | (module_index),        \
	 DEVAPC##apcnum##_D##domnum##_VIO_STA);                                  \
 }
