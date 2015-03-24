#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/android_pmem.h>
/* FIX-ME: marked for early porting */
/* #include <linux/android_vmem.h> */
#include <asm/setup.h>
#include <asm/mach/arch.h>
#include <linux/sysfs.h>
#include <asm/io.h>
#include <linux/spi/spi.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/musb/musb.h>
#include <linux/musbfsh.h>
#include "mach/memory.h"
#include "mach/irqs.h"
#include <mach/mt_reg_base.h>
#include <mach/devs.h>
#include <mach/i2c.h>
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>
#include <mach/mt_boot.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_ddp_bls.h>
#include <linux/version.h>
#include <linux/of_fdt.h>

extern u32 get_devinfo_with_index(u32 index);
extern u32 g_devinfo_data[];
extern u32 g_devinfo_data_size;
extern void adjust_kernel_cmd_line_setting_for_console(char *, char *);
unsigned int mtk_get_max_DRAM_size(void);

struct {
	u32 base;
	u32 size;
} bl_fb = {
0, 0};

static int use_bl_fb;

/*=======================================================================*/
/* MT8135 USB GADGET                                                     */
/*=======================================================================*/
#include <mach/mt_usb.h>
static struct mt_usb_board_data usb_board_data = {
	.vbus_gpio = 89,
};
static u64 usb_dmamask = DMA_BIT_MASK(32);
static struct musb_hdrc_config musb_config_mt65xx = {
	.multipoint = true,
	.dyn_fifo = true,
	.soft_con = true,
	.dma = true,
	.num_eps = 16,
	.dma_channels = 8,
};

static struct musb_hdrc_platform_data usb_data = {
#ifdef CONFIG_USB_MTK_OTG
	.mode = MUSB_OTG,
#else
	.mode = MUSB_PERIPHERAL,
#endif
	.config = &musb_config_mt65xx,
	.board_data = &usb_board_data,
};

struct platform_device mt_device_usb = {
	.name = "mt_usb",
	.id = -1,		/* only one such device */
	.dev = {
		.platform_data = &usb_data,
		.dma_mask = &usb_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		/* .release=musbfsh_hcd_release, */
		},
};

void set_usb_vbus_gpio(int gpio)
{
	usb_board_data.vbus_gpio = gpio;
}
/*=======================================================================*/
/* MT8135 USB11 Host                      */
/*=======================================================================*/
#if defined(CONFIG_MTK_USBFSH)
static u64 usb11_dmamask = DMA_BIT_MASK(32);

static struct musbfsh_hdrc_config musbfsh_config_mt65xx = {
	.multipoint = false,
	.dyn_fifo = true,
	.soft_con = true,
	.dma = true,
	.num_eps = 16,
	.dma_channels = 8,
};

static struct musbfsh_hdrc_platform_data usb_data_mt65xx = {
	.mode = 1,
	.config = &musbfsh_config_mt65xx,
};

static struct platform_device mt_usb11_dev = {
	.name = "mt_usb11",
	.id = -1,
	.dev = {
		.platform_data = &usb_data_mt65xx,
		.dma_mask = &usb11_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.release = musbfsh_hcd_release,
		},
};
#endif

#ifdef __USING_DUMMY_CCCI_API__
unsigned int modem_size_list[1] = { 0 };

static unsigned int get_nr_modem(void)
{
	return 0;
}

static unsigned int *get_modem_size_list(void)
{
	return modem_size_list;
}
#endif

#define MAX_NR_MODEM 2
unsigned long modem_start_addr_list[MAX_NR_MODEM] = { 0x0, 0x0, };

unsigned int get_modem_size(void)
{
	int i, nr_modem;
	unsigned int size = 0, *modem_size_list;
	modem_size_list = get_modem_size_list();
	nr_modem = get_nr_modem();
	if (modem_size_list) {
		for (i = 0; i < nr_modem; i++)
			size += modem_size_list[i];
		return size;
	} else {
		return 0;
	}
}

unsigned long *get_modem_start_addr_list(void)
{
	return modem_start_addr_list;
}
EXPORT_SYMBOL(get_modem_start_addr_list);

#define RESERVED_MEM_MODEM  (0x0)	/* do not reserve memory in advance, do it in mt_fixup */
#ifndef CONFIG_RESERVED_MEM_SIZE_FOR_PMEM
#define CONFIG_RESERVED_MEM_SIZE_FOR_PMEM	1
#endif

#if defined(CONFIG_MTK_FB)
char temp_command_line[1024] = { 0 };
#endif

/*
 * The memory size reserved for PMEM
 *
 * The size could be varied in different solutions.
 * The size is set in mt65xx_fixup function.
 * - MT8135 in develop should be 0x3600000
 * - MT8135 SQC should be 0x0
 */
unsigned int RESERVED_MEM_SIZE_FOR_PMEM = 0x1700000;
unsigned long pmem_start = 0x9C000000;	/* pmem_start is inited in mt_fixup */
unsigned long kernel_mem_sz = 0x0;	/* kernel_mem_sz is inited in mt_fixup */

#define TOTAL_RESERVED_MEM_SIZE (RESERVED_MEM_SIZE_FOR_PMEM + \
				 RESERVED_MEM_SIZE_FOR_FB)

#define MAX_PFN        ((max_pfn << PAGE_SHIFT) + PHYS_OFFSET)

#define PMEM_MM_START  (pmem_start)
#define PMEM_MM_SIZE   (RESERVED_MEM_SIZE_FOR_PMEM)

#define FB_START       (PMEM_MM_START + PMEM_MM_SIZE)
#define FB_SIZE        (RESERVED_MEM_SIZE_FOR_FB)


struct platform_device fh_dev = {
	.name = "mt-freqhopping",
	.id = -1,
};

struct platform_device pmic_wrap_dev = {
	.name = "mt8135-pwrap",
	.id = -1,
};

#if defined(CONFIG_SERIAL_AMBA_PL011)
static struct amba_device uart1_device = {
	.dev = {
		.coherent_dma_mask = ~0,
		.init_name = "dev:f1",
		.platform_data = NULL,
		},
	.res = {
		.start = 0xE01F1000,
		.end = 0xE01F1000 + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
		},
	.dma_mask = ~0,
	.irq = MT_UART1_IRQ_ID,
};
#endif

/*=======================================================================*/
/* MT8135 MSDC Hosts                                                       */
/*=======================================================================*/
#if defined(CFG_DEV_MSDC0)
static struct resource mt_resource_msdc0[] = {
	{
#ifdef CONFIG_MMC_MTK_EMMC
	 .start = MSDC_0_BASE,
	 .end = MSDC_0_BASE + 0x108,
#else
	 .start = MSDC_1_BASE,
	 .end = MSDC_1_BASE + 0x108,
#endif
	 .flags = IORESOURCE_MEM,
	 },
	{
#ifdef CONFIG_MMC_MTK_EMMC
	 .start = MT_MSDC0_IRQ_ID,
#else
	 .start = MT_MSDC1_IRQ_ID,
#endif
	 .flags = IORESOURCE_IRQ,
	 },
};
#endif

#if defined(CFG_DEV_MSDC1)
static struct resource mt_resource_msdc1[] = {
	{
#ifdef CONFIG_MMC_MTK_EMMC
	 .start = MSDC_1_BASE,
	 .end = MSDC_1_BASE + 0x108,
#else
	 .start = MSDC_0_BASE,
	 .end = MSDC_0_BASE + 0x108,
#endif
	 .flags = IORESOURCE_MEM,
	 },
	{
#ifdef CONFIG_MMC_MTK_EMMC
	 .start = MT_MSDC1_IRQ_ID,
#else
	 .start = MT_MSDC0_IRQ_ID,
#endif
	 .flags = IORESOURCE_IRQ,
	 },
};
#endif

#if defined(CFG_DEV_MSDC2)
static struct resource mt_resource_msdc2[] = {
	{
	 .start = MSDC_2_BASE,
	 .end = MSDC_2_BASE + 0x108,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_MSDC2_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};
#endif

#if defined(CFG_DEV_MSDC3)
static struct resource mt_resource_msdc3[] = {
	{
	 .start = MSDC_3_BASE,
	 .end = MSDC_3_BASE + 0x108,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_MSDC3_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};
#endif
#if defined(CFG_DEV_MSDC4)
static struct resource mt_resource_msdc4[] = {
	{
	 .start = MSDC_4_BASE,
	 .end = MSDC_4_BASE + 0x108,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_MSDC4_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};
#endif

#if defined(CONFIG_MTK_FB)
static u64 mtkfb_dmamask = ~(u32) 0;

static struct resource resource_fb[] = {
	{
	 .start = 0,		/* Will be redefined later */
	 .end = 0,
	 .flags = IORESOURCE_MEM}
};

static struct platform_device mt6575_device_fb = {
	.name = "mtkfb",
	.id = 0,
	.num_resources = ARRAY_SIZE(resource_fb),
	.resource = resource_fb,
	.dev = {
		.dma_mask = &mtkfb_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
};
#endif

#ifdef CONFIG_MTK_MULTIBRIDGE_SUPPORT
static struct platform_device mtk_multibridge_dev = {
	.name = "multibridge",
	.id = 0,
};
#endif
#ifdef CONFIG_MTK_HDMI_SUPPORT
static struct platform_device mtk_hdmi_dev = {
	.name = "hdmitx",
	.id = 0,
};
#endif


#ifdef CONFIG_MTK_MT8193_SUPPORT
static struct platform_device mtk_ckgen_dev = {
	.name = "mt8193-ckgen",
	.id = 0,
};
#endif


#if defined(CONFIG_MTK_SPI)
static struct resource mt_spi_resources[] = {
	[0] = {
	       .start = SPI1_BASE,
	       .end = SPI1_BASE + 0x0028,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = MT8135_SPI1_IRQ_ID,
	       .flags = IORESOURCE_IRQ,
	       },
};

static struct platform_device mt_spi_device = {
	.name = "mt-spi",
	.num_resources = ARRAY_SIZE(mt_spi_resources),
	.resource = mt_spi_resources
};

#endif


#if defined(CONFIG_USB_MTK_ACM_TEMP)
struct platform_device usbacm_temp_device = {
	.name = "USB_ACM_Temp_Driver",
	.id = -1,
};
#endif

#if defined(CONFIG_MTK_ACCDET)
struct platform_device accdet_device = {
	.name = "Accdet_Driver",
	.id = -1,
	/* .dev    ={ */
	/* .release = accdet_dumy_release, */
	/* } */
};
#endif

#if defined(CONFIG_MTK_TVOUT_SUPPORT)

static struct resource mt6575_TVOUT_resource[] = {
	[0] = {			/* TVC */
	       .start = TVC_BASE,
	       .end = TVC_BASE + 0x78,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {			/* TVR */
	       .start = TV_ROT_BASE,
	       .end = TV_ROT_BASE + 0x378,
	       .flags = IORESOURCE_MEM,
	       },
	[2] = {			/* TVE */
	       .start = TVE_BASE,
	       .end = TVE_BASE + 0x84,
	       .flags = IORESOURCE_MEM,
	       },
};

static u64 mt6575_TVOUT_dmamask = ~(u32) 0;

static struct platform_device mt6575_TVOUT_dev = {
	.name = "TV-out",
	.id = 0,
	.num_resources = ARRAY_SIZE(mt6575_TVOUT_resource),
	.resource = mt6575_TVOUT_resource,
	.dev = {
		.dma_mask = &mt6575_TVOUT_dmamask,
		.coherent_dma_mask = 0xffffffffUL}
};
#endif
static struct platform_device mt_device_msdc[] = {
#if defined(CFG_DEV_MSDC0)
	{
	 .name = "mtk-msdc",
	 .id = 0,
	 .num_resources = ARRAY_SIZE(mt_resource_msdc0),
	 .resource = mt_resource_msdc0,
	 .dev = {
		 .platform_data = &msdc0_hw,
		 },
	 },
#endif
#if defined(CFG_DEV_MSDC1)
	{
	 .name = "mtk-msdc",
	 .id = 1,
	 .num_resources = ARRAY_SIZE(mt_resource_msdc1),
	 .resource = mt_resource_msdc1,
	 .dev = {
		 .platform_data = &msdc1_hw,
		 },
	 },
#endif
#if defined(CFG_DEV_MSDC2)
	{
	 .name = "mtk-msdc",
	 .id = 2,
	 .num_resources = ARRAY_SIZE(mt_resource_msdc2),
	 .resource = mt_resource_msdc2,
	 .dev = {
		 .platform_data = &msdc2_hw,
		 },
	 },
#endif
#if defined(CFG_DEV_MSDC3)
	{
	 .name = "mtk-msdc",
	 .id = 3,
	 .num_resources = ARRAY_SIZE(mt_resource_msdc3),
	 .resource = mt_resource_msdc3,
	 .dev = {
		 .platform_data = &msdc3_hw,
		 },
	 },
#endif
#if defined(CFG_DEV_MSDC4)
	{
	 .name = "mtk-msdc",
	 .id = 4,
	 .num_resources = ARRAY_SIZE(mt_resource_msdc4),
	 .resource = mt_resource_msdc4,
	 .dev = {
		 .platform_data = &msdc4_hw,
		 },
	 },
#endif
};

#ifdef CONFIG_RFKILL
/*=======================================================================*/
/* MT8135 RFKill module (BT and WLAN)                                             */
/*=======================================================================*/
/* MT66xx RFKill BT */
struct platform_device mt_rfkill_device = {
	.name = "mt-rfkill",
	.id = -1,
};
#endif

/*=======================================================================*/
/* HID Keyboard  add by zhangsg                                                 */
/*=======================================================================*/

#if defined(CONFIG_KEYBOARD_HID)
static struct platform_device mt_hid_dev = {
	.name = "hid-keyboard",
	.id = -1,
};
#endif

/*=======================================================================*/
/* MT6575 Touch Panel                                                    */
/*=======================================================================*/
static struct platform_device mtk_tpd_dev = {
	.name = "mtk-tpd",
	.id = -1,
};

/*=======================================================================*/
/* MT6575 ofn                                                           */
/*=======================================================================*/
#if defined(CONFIG_CUSTOM_KERNEL_OFN)
static struct platform_device ofn_driver = {
	.name = "mtofn",
	.id = -1,
};
#endif

/*=======================================================================*/
/* CPUFreq                                                               */
/*=======================================================================*/
#ifdef CONFIG_CPU_FREQ
static struct platform_device cpufreq_pdev = {
	.name = "mt-cpufreq",
	.id = -1,
};
#endif

/*=======================================================================*/
/* MT6575 Thermal Controller module                                      */
/*=======================================================================*/
struct platform_device thermal_pdev = {
	.name = "mtk-thermal",
	.id = -1,
};

/*=======================================================================*/
/* MT8135 PTP module                                      */
/*=======================================================================*/
struct platform_device ptp_pdev = {
	.name = "mtk-ptp",
	.id = -1,
};

/*=======================================================================*/
/* MT8135 SPM-MCDI module                                      */
/*=======================================================================*/
struct platform_device spm_mcdi_pdev = {
	.name = "mtk-spm-mcdi",
	.id = -1,
};


/*=======================================================================*/
/* MT8135 USIF-DUMCHAR                                                          */
/*=======================================================================*/

#if defined(CONFIG_MTK_MMC_LEGACY)
static struct platform_device dummychar_device = {
	.name = "dummy_char",
	.id = 0,
};
#endif

/*=======================================================================*/
/* MT8135 NAND                                                           */
/*=======================================================================*/
#if defined(CONFIG_MTK_MTD_NAND)
#define NFI_base    NFI_BASE	/* 0x80032000 */
#define NFIECC_base NFIECC_BASE	/* 0x80038000 */
static struct resource mtk_resource_nand[] = {
	{
	 .start = NFI_base,
	 .end = NFI_base + 0x1A0,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = NFIECC_base,
	 .end = NFIECC_base + 0x150,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_NFI_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
	{
	 .start = MT_NFIECC_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct platform_device mtk_nand_dev = {
	.name = "mtk-nand",
	.id = 0,
	.num_resources = ARRAY_SIZE(mtk_resource_nand),
	.resource = mtk_resource_nand,
	.dev = {
		.platform_data = &mtk_nand_hw,
		},
};
#endif

/*=======================================================================*/
/* MTK I2C                                                            */
/*=======================================================================*/

static struct resource mt_resource_i2c0[] = {
	{
	 .start = I2C0_BASE,
	 .end = I2C0_BASE + 0x70,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_I2C0_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct resource mt_resource_i2c1[] = {
	{
	 .start = I2C1_BASE,
	 .end = I2C1_BASE + 0x70,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_I2C1_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct resource mt_resource_i2c2[] = {
	{
	 .start = I2C2_BASE,
	 .end = I2C2_BASE + 0x70,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_I2C2_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct resource mt_resource_i2c3[] = {
	{
	 .start = I2C3_BASE,
	 .end = I2C3_BASE + 0x70,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_I2C3_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct resource mt_resource_i2c4[] = {
	{
	 .start = I2C4_BASE,
	 .end = I2C4_BASE + 0x70,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_I2C4_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct resource mt_resource_i2c5[] = {
	{
	 .start = I2C5_BASE,
	 .end = I2C5_BASE + 0x70,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_I2C5_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct resource mt_resource_i2c6[] = {
	{
	 .start = I2C6_BASE,
	 .end = I2C6_BASE + 0x70,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MT_I2C6_IRQ_ID,
	 .flags = IORESOURCE_IRQ,
	 },
};

static int mt_i2c_enable_dma_clk(struct mt_i2c_data *i2c, bool enable)
{
	return enable ?
		enable_clock(MT_CG_PERI_AP_DMA_PDN, "i2c") :
		disable_clock(MT_CG_PERI_AP_DMA_PDN, "i2c");
}

static int mt_i2c_enable_clk(struct mt_i2c_data *i2c, bool enable)
{
	return enable ?
		enable_clock(i2c->pdn, "i2c") :
		disable_clock(i2c->pdn, "i2c");
}

static int mt_i2c_enable_clk_pmic(struct mt_i2c_data *i2c, bool enable)
{
	u32 val = enable ? 0 : 1;
	mt_i2c_enable_clk(i2c, enable);
	return upmu_set_rg_wrp_i2c_pdn(val, i2c->pmic_ch);
}

static u32 mt_i2c_get_func_clk(struct mt_i2c_data *i2c)
{
	return (i2c->flags & MT_WRAPPER_BUS) ?
		(u32)(I2C_CLK_RATE) :
		(mt_get_bus_freq() / 16);
}

struct mt_i2c_data mt_i2c_data[] = {
	{
		.pdn	= MT_CG_PERI_I2C0_PDN,
		.speed	= 100,
		.delay_len = 2,
		.enable_dma_clk = mt_i2c_enable_dma_clk,
		.enable_clk = mt_i2c_enable_clk,
		.get_func_clk = mt_i2c_get_func_clk,
		.flags  = 0,
	},
	{
		.pdn	= MT_CG_PERI_I2C1_PDN,
		.speed	= 100,
		.delay_len = 2,
		.enable_dma_clk = mt_i2c_enable_dma_clk,
		.enable_clk = mt_i2c_enable_clk,
		.get_func_clk = mt_i2c_get_func_clk,
		.flags  = 0,
	},
	{
		.pdn	= MT_CG_PERI_I2C2_PDN,
		.speed	= 100,
		.delay_len = 2,
		.enable_dma_clk = mt_i2c_enable_dma_clk,
		.enable_clk = mt_i2c_enable_clk,
		.get_func_clk = mt_i2c_get_func_clk,
		.flags  = 0,
	},
	{
		.pdn	= MT_CG_PERI_I2C3_PDN,
		.speed	= 100,
		.delay_len = 2,
		.enable_dma_clk = mt_i2c_enable_dma_clk,
		.enable_clk = mt_i2c_enable_clk,
		.get_func_clk = mt_i2c_get_func_clk,
		.flags  = 0,
	},
	{
		.pdn	= MT_CG_PERI_I2C4_PDN,
		.speed	= 100,
		.delay_len = 2,
		.pmic_ch = 0,
		.enable_dma_clk = mt_i2c_enable_dma_clk,
		.enable_clk = mt_i2c_enable_clk_pmic,
		.get_func_clk = mt_i2c_get_func_clk,
		.flags  = MT_WRAPPER_BUS,
	},
	{
		.pdn	= MT_CG_PERI_I2C5_PDN,
		.speed	= 100,
		.delay_len = 2,
		.pmic_ch = 1,
		.enable_dma_clk = mt_i2c_enable_dma_clk,
		.enable_clk = mt_i2c_enable_clk_pmic,
		.get_func_clk = mt_i2c_get_func_clk,
		.flags  = MT_WRAPPER_BUS,
	},
	{
		.pdn	= MT_CG_PERI_I2C6_PDN,
		.speed	= 100,
		.pmic_ch = 2,
		.delay_len = 2,
		.enable_dma_clk = mt_i2c_enable_dma_clk,
		.enable_clk = mt_i2c_enable_clk_pmic,
		.get_func_clk = mt_i2c_get_func_clk,
		.flags  = MT_WRAPPER_BUS,
	},
};

/* called from board code to adjust speed, and list of
 * devices that need WRRD command to generate repeated start */
int mt_dev_i2c_bus_configure(u16 n_i2c, u16 speed, u16 wrrd[])
{
	if (n_i2c >= ARRAY_SIZE(mt_i2c_data))
		return -ENODEV;
	mt_i2c_data[n_i2c].speed = speed;
	if (wrrd)
		mt_i2c_data[n_i2c].need_wrrd = wrrd;
	return 0;
}

static struct platform_device mt_device_i2c[] = {
	{
		.name = "mt-i2c",
		.id = 0,
		.num_resources = ARRAY_SIZE(mt_resource_i2c0),
		.resource = mt_resource_i2c0,
		.dev = {
			.platform_data = &mt_i2c_data[0],
		},
	},
	{
		.name = "mt-i2c",
		.id = 1,
		.num_resources = ARRAY_SIZE(mt_resource_i2c1),
		.resource = mt_resource_i2c1,
		.dev = {
			.platform_data = &mt_i2c_data[1],
		},
	},
	{
		.name = "mt-i2c",
		.id = 2,
		.num_resources = ARRAY_SIZE(mt_resource_i2c2),
		.resource = mt_resource_i2c2,
		.dev = {
			.platform_data = &mt_i2c_data[2],
		},
	},
	{
		.name = "mt-i2c",
		.id = 3,
		.num_resources = ARRAY_SIZE(mt_resource_i2c3),
		.resource = mt_resource_i2c3,
		.dev = {
			.platform_data = &mt_i2c_data[3],
		},
	},
	{
		.name = "mt-i2c",
		.id = 4,
		.num_resources = ARRAY_SIZE(mt_resource_i2c4),
		.resource = mt_resource_i2c4,
		.dev = {
			.platform_data = &mt_i2c_data[4],
		},
	},
	{
		.name = "mt-i2c",
		.id = 5,
		.num_resources = ARRAY_SIZE(mt_resource_i2c5),
		.resource = mt_resource_i2c5,
		.dev = {
			.platform_data = &mt_i2c_data[5],
		},
	},
	{
		.name = "mt-i2c",
		.id = 6,
		.num_resources = ARRAY_SIZE(mt_resource_i2c6),
		.resource = mt_resource_i2c6,
		.dev = {
			.platform_data = &mt_i2c_data[6],
		},
	},
};

static u64 mtk_smi_dmamask = ~(u32) 0;

static struct platform_device mtk_smi_dev = {
	.name = "MTK_SMI",
	.id = 0,
	.dev = {
		.dma_mask = &mtk_smi_dmamask,
		.coherent_dma_mask = 0xffffffffUL}
};


static u64 mtk_m4u_dmamask = ~(u32) 0;

static struct platform_device mtk_m4u_dev = {
	.name = "M4U_device",
	.id = 0,
	.dev = {
		.dma_mask = &mtk_m4u_dmamask,
		.coherent_dma_mask = 0xffffffffUL}
};


/*=======================================================================*/
/* MT6573 GPS module                                                    */
/*=======================================================================*/
/* MT3326 GPS */
#ifdef CONFIG_MTK_GPS
struct platform_device mt3326_device_gps = {
	.name = "mt3326-gps",
	.id = -1,
	.dev = {
		.platform_data = &mt3326_gps_hw,
		},
};
#endif

/*=======================================================================*/
/* MT6573 PMEM                                                           */
/*=======================================================================*/
#if defined(CONFIG_ANDROID_PMEM)
static struct android_pmem_platform_data pdata_multimedia = {
	.name = "pmem_multimedia",
	.no_allocator = 0,
	.cached = 1,
	.buffered = 1
};

static struct platform_device pmem_multimedia_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = {.platform_data = &pdata_multimedia}
};
#endif

#if defined(CONFIG_ANDROID_VMEM)
static struct android_vmem_platform_data pdata_vmultimedia = {
	.name = "vmem_multimedia",
	.no_allocator = 0,
	.cached = 1,
	.buffered = 1
};

static struct platform_device vmem_multimedia_device = {
	.name = "android_vmem",
	.id = -1,
	.dev = {.platform_data = &pdata_vmultimedia}
};
#endif

/*=======================================================================*/
/* MT6575 SYSRAM                                                         */
/*=======================================================================*/
static struct platform_device camera_sysram_dev = {
	.name = "camera-sysram",	/* FIXME. Sync to driver, init.rc, MHAL */
	.id = 0,
};

/*=======================================================================*/
/*=======================================================================*/
/* Commandline fitlter to choose the supported commands                  */
/*=======================================================================*/
/*=======================================================================*/
/* Commandline filter                                                    */
/* This function is used to filter undesired command passed from LK      */
/*=======================================================================*/
static void cmdline_filter(struct tag *cmdline_tag, char *default_cmdline)
{
	const char *undesired_cmds[] = {
		"console=",
		"root=",
	};

	int i;
	int ck_f = 0;
	char *cs, *ce;

	cs = cmdline_tag->u.cmdline.cmdline;
	ce = cs;
	while ((__u32) ce < (__u32) tag_next(cmdline_tag)) {

		while (*cs == ' ' || *cs == '\0') {
			cs++;
			ce = cs;
		}

		if (*ce == ' ' || *ce == '\0') {
			for (i = 0; i < sizeof(undesired_cmds) / sizeof(char *); i++) {
				if (memcmp(cs, undesired_cmds[i], strlen(undesired_cmds[i])) == 0) {
					ck_f = 1;
					break;
				}
			}

			if (ck_f == 0) {
				*ce = '\0';
				/* Append to the default command line */
				strcat(default_cmdline, " ");
				strcat(default_cmdline, cs);
			}
			ck_f = 0;
			cs = ce + 1;
		}
		ce++;
	}
	if (strlen(default_cmdline) >= COMMAND_LINE_SIZE)
		panic("Command line length is too long.\n\r");
}

/*=======================================================================*/
/* Parse the framebuffer info						 */
/*=======================================================================*/
static int __init parse_tag_videofb_fixup(const struct tag *tags)
{
	bl_fb.base = tags->u.videolfb.lfb_base;
	bl_fb.size = tags->u.videolfb.lfb_size;
	use_bl_fb++;
	return 0;
}

static int __init parse_tag_devinfo_data_fixup(const struct tag *tags)
{
	int i = 0;
	int size = tags->u.devinfo_data.devinfo_data_size;

	for (i = 0; i < size; i++)
		g_devinfo_data[i] = tags->u.devinfo_data.devinfo_data[i];

	/* print chip id for debugging purpose */
	pr_info("tag_devinfo_data_rid, indx[%d]:0x%x\n", 12, g_devinfo_data[12]);
	/* KSY: Add for 8135 debug purpose */
	pr_info("tag_devinfo_data_hw2_res0, indx[%d]:0x%x\n", 21, g_devinfo_data[21]);
	pr_info("tag_devinfo_data_hw2_res1, indx[%d]:0x%x\n", 22, g_devinfo_data[22]);
	pr_info("tag_devinfo_data_hw2_res2, indx[%d]:0x%x\n", 23, g_devinfo_data[23]);
	pr_info("tag_devinfo_data_hw2_res3, indx[%d]:0x%x\n", 24, g_devinfo_data[24]);
	pr_info("tag_devinfo_data_hw2_res4, indx[%d]:0x%x\n", 25, g_devinfo_data[25]);
	pr_info("tag_devinfo_data size:%d\n", size);
	g_devinfo_data_size = size;
	return 0;
}

static int __init early_init_dt_get_chosen(unsigned long node, const char *uname, int depth,
					   void *data)
{
	if (depth != 1 || (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	return node;
}

void __init mt_fixup(struct tag *tags, char **cmdline, struct meminfo *mi)
{
/* FIXME: need porting */
#if 1
	struct tag *cmdline_tag = NULL;
	struct tag *reserved_mem_bank_tag = NULL;
	unsigned long node = (unsigned long)tags;
	unsigned long max_limit_size = CONFIG_MAX_DRAM_SIZE_SUPPORT - RESERVED_MEM_MODEM;
	unsigned long avail_dram = 0;
	unsigned long bl_mem_sz = 0;
	/* for modem fixup */
	unsigned int nr_modem = 0, i = 0;
	unsigned int max_avail_addr = 0;
	unsigned int modem_start_addr = 0;
	unsigned int hole_start_addr = 0;
	unsigned int hole_size = 0;
	unsigned int *modem_size_list = 0;

	node = of_scan_flat_dt(early_init_dt_get_chosen, NULL);

	cmdline_tag = of_get_flat_dt_prop(node, "atag,cmdline", NULL);
#if defined(CONFIG_MTK_FB)
	if (cmdline_tag)
		cmdline_filter(cmdline_tag, (char *)&temp_command_line);
#endif

	tags = of_get_flat_dt_prop(node, "atag,boot", NULL);
	if (tags)
		g_boot_mode = tags->u.boot.bootmode;

	tags = of_get_flat_dt_prop(node, "atag,meta", NULL);
	if (tags) {
		g_meta_com_type = tags->u.meta_com.meta_com_type;
		g_meta_com_id = tags->u.meta_com.meta_com_id;
	}

	tags = of_get_flat_dt_prop(node, "atag,videolfb", NULL);
	if (tags)
		parse_tag_videofb_fixup(tags);

	tags = of_get_flat_dt_prop(node, "atag,devinfo", NULL);
	if (tags)
		parse_tag_devinfo_data_fixup(tags);

	tags = of_get_flat_dt_prop(node, "atag,mem", NULL);
	if (tags) {
		for (; tags->hdr.tag == ATAG_MEM; tags = tag_next(tags)) {
			bl_mem_sz += tags->u.mem.size;
			/*
			 * Modify the memory tag to limit available memory to
			 * CONFIG_MAX_DRAM_SIZE_SUPPORT
			 */
			if (max_limit_size > 0) {
				if (max_limit_size >= tags->u.mem.size) {
					max_limit_size -= tags->u.mem.size;
					avail_dram += tags->u.mem.size;
				} else {
					tags->u.mem.size = max_limit_size;
					avail_dram += max_limit_size;
					max_limit_size = 0;
				}
				/* By Keene: */
				/* remove this check to avoid calcuate pmem size before we know all dram size */
				/* Assuming the minimum size of memory bank is 256MB */
				/* if (tags->u.mem.size >= (TOTAL_RESERVED_MEM_SIZE)) { */
				reserved_mem_bank_tag = tags;
				/* } */
			} else {
				tags->u.mem.size = 0;
			}
		}
	}

	kernel_mem_sz = avail_dram;	/* keep the DRAM size (limited by CONFIG_MAX_DRAM_SIZE_SUPPORT) */
	/*
	 * If the maximum memory size configured in kernel
	 * is smaller than the actual size (passed from BL)
	 * Still limit the maximum memory size but use the FB
	 * initialized by BL
	 */
	if (bl_mem_sz >= (CONFIG_MAX_DRAM_SIZE_SUPPORT - RESERVED_MEM_MODEM))
		use_bl_fb++;

	/*
	 * Setup PMEM size
	 */
	/*
	   if (avail_dram < 0x10000000)
	   RESERVED_MEM_SIZE_FOR_PMEM = 0x1700000;
	   else */
	RESERVED_MEM_SIZE_FOR_PMEM = 0;

	/* Reserve memory in the last bank */
	if (reserved_mem_bank_tag) {
		reserved_mem_bank_tag->u.mem.size -= ((__u32) TOTAL_RESERVED_MEM_SIZE);
		mi->bank[mi->nr_banks - 1].size -= ((__u32) TOTAL_RESERVED_MEM_SIZE);
		pmem_start = reserved_mem_bank_tag->u.mem.start + reserved_mem_bank_tag->u.mem.size;
	} else			/* we should always have reserved memory */
		BUG();

	pr_alert("[PHY layout]avaiable DRAM size (lk) = 0x%08lx\n[PHY layout]avaiable DRAM size = 0x%08lx\n[PHY layout]FB       :   0x%08lx - 0x%08lx  (0x%08x)\n",
		 bl_mem_sz, kernel_mem_sz, FB_START, FB_START + FB_SIZE, FB_SIZE);
	if (PMEM_MM_SIZE) {
		pr_alert("[PHY layout]PMEM     :   0x%08lx - 0x%08lx  (0x%08x)\n",
			 PMEM_MM_START, PMEM_MM_START + PMEM_MM_SIZE, PMEM_MM_SIZE);
	}
	/*
	 * fixup memory tags for dual modem model
	 * assumptions:
	 * 1) modem start addresses should be 32MiB aligned
	 */
	nr_modem = get_nr_modem();
	modem_size_list = get_modem_size_list();

	for (i = 0; i < nr_modem; i++) {
		/* sanity test */
		if (modem_size_list[i]) {
			pr_alert("fixup for modem [%d], size = 0x%08x\n", i, modem_size_list[i]);
		} else {
			pr_alert("[Error]skip empty modem [%d]\n", i);
			continue;
		}
		pr_alert("reserved_mem_bank_tag start = 0x%08x, reserved_mem_bank_tag size = 0x%08x, TOTAL_RESERVED_MEM_SIZE = 0x%08x\n",
			 reserved_mem_bank_tag->u.mem.start,
			 reserved_mem_bank_tag->u.mem.size, TOTAL_RESERVED_MEM_SIZE);
		/* find out start address for modem */
		max_avail_addr = reserved_mem_bank_tag->u.mem.start +
		    reserved_mem_bank_tag->u.mem.size;
		modem_start_addr = round_down((max_avail_addr - modem_size_list[i]), 0x2000000);
		/* sanity test */
		if (modem_size_list[i] > reserved_mem_bank_tag->u.mem.size) {
			pr_alert("[Error]skip modem [%d] fixup: size too large: 0x%08x, reserved_mem_bank_tag->u.mem.size: 0x%08x\n",
				i, modem_size_list[i], reserved_mem_bank_tag->u.mem.size);
			continue;
		}
		if (modem_start_addr < reserved_mem_bank_tag->u.mem.start) {
			pr_alert("[Error]skip modem [%d] fixup: modem crosses memory bank boundary: 0x%08x, reserved_mem_bank_tag->u.mem.start: 0x%08x\n",
				 i, modem_start_addr, reserved_mem_bank_tag->u.mem.start);
			continue;
		}
		pr_alert("modem fixup sanity test pass\n");
		modem_start_addr_list[i] = modem_start_addr;
		hole_start_addr = modem_start_addr + modem_size_list[i];
		hole_size = max_avail_addr - hole_start_addr;
		pr_alert("max_avail_addr = 0x%08x, modem_start_addr_list[%d] = 0x%08x, hole_start_addr = 0x%08x, hole_size = 0x%08x\n",
			 max_avail_addr, i, modem_start_addr, hole_start_addr, hole_size);
		pr_alert("[PHY layout]MD       :   0x%08x - 0x%08x  (0x%08x)\n",
			 modem_start_addr, modem_start_addr + modem_size_list[i],
			 modem_size_list[i]);
		/* shrink reserved_mem_bank */
		reserved_mem_bank_tag->u.mem.size -= (max_avail_addr - modem_start_addr);
		pr_alert("reserved_mem_bank: start = 0x%08x, size = 0x%08x\n",
			 reserved_mem_bank_tag->u.mem.start, reserved_mem_bank_tag->u.mem.size);
#if 0
		/* setup a new memory tag */
		tags->hdr.tag = ATAG_MEM;
		tags->hdr.size = tag_size(tag_mem32);
		tags->u.mem.start = hole_start_addr;
		tags->u.mem.size = hole_size;
		/* do next tag */
		tags = tag_next(tags);
#endif
	}
/* tags->hdr.tag = ATAG_NONE; // mark the end of the tag list */
/* tags->hdr.size = 0; */


	if (cmdline_tag != NULL) {
#ifdef CONFIG_FIQ_DEBUGGER
		char *console_ptr;
		int uart_port;
#endif
		char *br_ptr;
		/* This function may modify ttyMT3 to ttyMT0 if needed */
		adjust_kernel_cmd_line_setting_for_console(cmdline_tag->u.cmdline.cmdline,
							   *cmdline);
#ifdef CONFIG_FIQ_DEBUGGER
		console_ptr = strstr(*cmdline, "ttyMT");
		if ((console_ptr) != 0) {
			uart_port = console_ptr[5] - '0';
			if (uart_port > 3)
				uart_port = -1;

			fiq_uart_fixup(uart_port);
		}
#endif

		/*FIXME mark for porting */
		cmdline_filter(cmdline_tag, *cmdline);
		br_ptr = strstr(*cmdline, "boot_reason=");
		if ((br_ptr) != 0) {
			/* get boot reason */
			g_boot_reason = br_ptr[12] - '0';
		}

		/* Use the default cmdline */
		/* memcpy((void*)cmdline_tag, */
		/* (void*)tag_next(cmdline_tag), */
		/* ATAG_NONE actual size */
		/* (uint32_t)(none_tag) - (uint32_t)(tag_next(cmdline_tag)) + 8); */
	}
#endif
}

struct platform_device auxadc_device = {
	.name = "mt-auxadc",
	.id = -1,
};

/*=======================================================================*/
/* MT6575 sensor module                                                  */
/*=======================================================================*/
struct platform_device sensor_gsensor = {
	.name = "gsensor",
	.id = -1,
};

struct platform_device sensor_msensor = {
	.name = "msensor",
	.id = -1,
};

struct platform_device sensor_orientation = {
	.name = "orientation",
	.id = -1,
};

struct platform_device sensor_alsps = {
	.name = "als_ps",
	.id = -1,
};

struct platform_device sensor_gyroscope = {
	.name = "gyroscope",
	.id = -1,
};

struct platform_device sensor_barometer = {
	.name = "barometer",
	.id = -1,
};

/* hwmon sensor */
struct platform_device hwmon_sensor = {
	.name = "hwmsensor",
	.id = -1,
};

/*=======================================================================*/
/* Camera ISP                                                            */
/*=======================================================================*/
static struct resource mt_resource_isp[] = {
	{			/* ISP configuration */
	 .start = CAMINF_BASE,
	 .end = CAMINF_BASE + 0xE000,
	 .flags = IORESOURCE_MEM,
	 },
	{			/* ISP IRQ */
	 .start = CAMERA_ISP_IRQ0_ID,
	 .flags = IORESOURCE_IRQ,
	 }
};

static u64 mt_isp_dmamask = ~(u32) 0;
/*  */
static struct platform_device mt_isp_dev = {
	.name = "camera-isp",
	.id = 0,
	.num_resources = ARRAY_SIZE(mt_resource_isp),
	.resource = mt_resource_isp,
	.dev = {
		.dma_mask = &mt_isp_dmamask,
		.coherent_dma_mask = 0xffffffffUL}
};

#if 0
/*=======================================================================*/
/* MT6575 EIS                                                            */
/*=======================================================================*/
static struct resource mt_resource_eis[] = {
	[0] = {			/* EIS configuration */
	       .start = EIS_BASE,
	       .end = EIS_BASE + 0x2C,
	       .flags = IORESOURCE_MEM,
	       }
};

static u64 mt_eis_dmamask = ~(u32) 0;
/*  */
static struct platform_device mt_eis_dev = {
	.name = "camera-eis",
	.id = 0,
	.num_resources = ARRAY_SIZE(mt_resource_eis),
	.resource = mt_resource_eis,
	.dev = {
		.dma_mask = &mt_eis_dmamask,
		.coherent_dma_mask = 0xffffffffUL}
};

#endif
/*  */
/*=======================================================================*/
/* Image sensor                                                        */
/*=======================================================================*/
static struct platform_device sensor_dev = {
	.name = "image_sensor",
	.id = -1,
};

static struct platform_device sensor_dev_bus2 = {
	.name = "image_sensor_bus2",
	.id = -1,
};

/*  */
/*=======================================================================*/
/* Lens actuator                                                        */
/*=======================================================================*/
static struct platform_device actuator_dev = {
	.name = "lens_actuator",
	.id = -1,
};

/*=======================================================================*/
/* MT6575 jogball                                                        */
/*=======================================================================*/
#ifdef CONFIG_MOUSE_PANASONIC_EVQWJN
static struct platform_device jbd_pdev = {
	.name = "mt6575-jb",
	.id = -1,
};
#endif

/*=======================================================================*/
/* MT8135 Pipe Manager                                                         */
/*=======================================================================*/
static struct platform_device camera_pipemgr_dev = {
	.name = "camera-pipemgr",
	.id = -1,
};

#ifdef CONFIG_MTK_LEDS
static struct platform_device mt65xx_leds_device = {
	.name = "leds-mt65xx",
	.id = -1
};
#endif

/*=======================================================================*/
/* NFC                                                                          */
/*=======================================================================*/
#if defined(CONFIG_MTK_NFC)
static struct platform_device mtk_nfc_6605_dev = {
	.name = "mt6605",
	.id = -1,
};
#endif

/* BLS device */
static struct mt_bls_data mt_bls_data = {
	.pwm_config = {
		.div = 0x11,
	},
};

static struct platform_device mt_bls_device = {
	.name  = "mt-ddp-bls",
	.id = -1,
	.dev = {
		.platform_data = &mt_bls_data,
	},
};

int __init mt_pwrap_init(void)
{
	return platform_device_register(&pmic_wrap_dev);
}

void __init mt_bls_init(struct mt_bls_data *bls_data)
{
	if (bls_data)
		mt_bls_device.dev.platform_data = bls_data;
	platform_device_register(&mt_bls_device);
}

/*=======================================================================*/
/* MT8135 Board Device Initialization                                    */
/*=======================================================================*/
int __init mt_board_init(void)
{
	int i = 0, retval = 0;

#ifdef CONFIG_FIQ_DEBUGGER
	retval = platform_device_register(&mt_fiq_debugger);
	if (retval != 0)
		return retval;
#endif

#if defined(CONFIG_MTK_MTD_NAND)
	retval = platform_device_register(&mtk_nand_dev);
	if (retval != 0) {
		pr_err("register nand device fail\n");
		return retval;
	}
#endif

	retval = platform_device_register(&fh_dev);
	if (retval != 0)
		return retval;

#ifdef CONFIG_MOUSE_PANASONIC_EVQWJN
	retval = platform_device_register(&jbd_pdev);
	if (retval != 0)
		return retval;
#endif

#if defined(CONFIG_KEYBOARD_HID)
	retval = platform_device_register(&mt_hid_dev);
	if (retval != 0)
		return retval;
#endif

#if defined(CONFIG_MTK_I2C)
	/* i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0)); */
	/* i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1)); */
	/* i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2)); */
	for (i = 0; i < ARRAY_SIZE(mt_device_i2c); i++) {
		retval = platform_device_register(&mt_device_i2c[i]);
		if (retval != 0)
			return retval;
	}
#endif
#if defined(CONFIG_MMC_MTK)
	for (i = 0; i < ARRAY_SIZE(mt_device_msdc); i++) {
		retval = platform_device_register(&mt_device_msdc[i]);
		if (retval != 0)
			return retval;
	}
#endif

#ifdef CONFIG_MTK_MULTIBRIDGE_SUPPORT
	retval = platform_device_register(&mtk_multibridge_dev);
	pr_debug("multibridge_driver_device\n!");
	if (retval != 0)
		return retval;
#endif


/* =====SMI/M4U devices=========== */
	pr_debug("register MTK_SMI device\n");
	retval = platform_device_register(&mtk_smi_dev);
	if (retval != 0)
		return retval;

	pr_debug("register M4U device: %d\n", retval);
	retval = platform_device_register(&mtk_m4u_dev);
	if (retval != 0)
		return retval;
/* =========================== */

#ifdef CONFIG_MTK_MT8193_SUPPORT
	pr_info("register 8193_CKGEN device\n");
	retval = platform_device_register(&mtk_ckgen_dev);
	if (retval != 0) {

		pr_info("register 8193_CKGEN device FAILS!\n");
		return retval;
	}
#endif


#if defined(CONFIG_MTK_FB)
	/*
	 * Bypass matching the frame buffer info. between boot loader and kernel
	 * if the limited memory size of the kernel is smaller than the
	 * memory size from bootloader
	 */
	if (((bl_fb.base == FB_START) && (bl_fb.size == FB_SIZE)) || (use_bl_fb == 2)) {
		pr_debug("FB is initialized by BL(%d)\n", use_bl_fb);
		mtkfb_set_lcm_inited(1);
	} else if ((bl_fb.base == 0) && (bl_fb.size == 0)) {
		pr_info("FB is not initialized(%d)\n", use_bl_fb);
		mtkfb_set_lcm_inited(0);
	} else {
		pr_info
		    ("******************************************************************************\n"
		     "   WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING\n"
		     "******************************************************************************\n"
		     "\n"
		     "  The default FB base & size values are not matched between BL and kernel\n"
		     "    - BOOTLD: start 0x%08x, size %d\n"
		     "    - KERNEL: start 0x%08lx, size %d\n" "\n"
		     "  If you see this warning message, please update your uboot.\n" "\n"
		     "******************************************************************************\n"
		     "   WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING\n"
		     "******************************************************************************\n"
		     "\n", bl_fb.base, bl_fb.size, FB_START, FB_SIZE);

		{
			int delay_sec = 5;

			while (delay_sec >= 0) {
				pr_info("\rcontinue after %d seconds ...", delay_sec--);
				mdelay(1000);
			}
			pr_info("\n");
		}
#if 0
		panic("The default base & size values are not matched " "between BL and kernel\n");
#endif
	}

	resource_fb[0].start = FB_START;
	resource_fb[0].end = FB_START + FB_SIZE - 1;

	pr_debug("FB start: 0x%x end: 0x%x\n", resource_fb[0].start, resource_fb[0].end);

	retval = platform_device_register(&mt6575_device_fb);
	if (retval != 0)
		return retval;
#endif

#if defined(CONFIG_MTK_LEDS)
	retval = platform_device_register(&mt65xx_leds_device);
	if (retval != 0)
		return retval;
#endif

#ifdef CONFIG_MTK_HDMI_SUPPORT
	retval = platform_device_register(&mtk_hdmi_dev);
	if (retval != 0)
		return retval;
#endif


#if defined(CONFIG_MTK_SPI)
/* spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs)); */
	platform_device_register(&mt_spi_device);
#endif


#if defined(CONFIG_MTK_TVOUT_SUPPORT)
	retval = platform_device_register(&mt6575_TVOUT_dev);
	pr_info("register TV-out device\n");
	if (retval != 0)
		return retval;
#endif

#if 1
	retval = platform_device_register(&auxadc_device);
	if (retval != 0) {
		pr_info("****[auxadc_driver] Unable to device register(%d)\n", retval);
		return retval;
	}
#endif

#if defined(CONFIG_MTK_ACCDET)
	retval = platform_device_register(&accdet_device);
	pr_debug("register accdet device\n");

	if (retval != 0) {
		pr_info("platform_device_accdet_register error:(%d)\n", retval);
		return retval;
	} else {
		pr_debug("platform_device_accdet_register done!\n");
	}
#endif

#if defined(CONFIG_USB_MTK_ACM_TEMP)
	retval = platform_device_register(&usbacm_temp_device);
	pr_debug("register usbacm temp device\n");

	if (retval != 0) {
		pr_info("platform_device_usbacm_register error:(%d)\n", retval);
		return retval;
	} else {
		pr_debug("platform_device_usbacm_register done!\n");
	}
#endif


#if 0				/* defined(CONFIG_MDP_MT6575) */
	/* pr_info("[MDP]platform_device_register\n\r"); */
	retval = platform_device_register(&mt6575_MDP_dev);
	if (retval != 0)
		return retval;
#endif

#if defined(CONFIG_MTK_SENSOR_SUPPORT)

	retval = platform_device_register(&hwmon_sensor);
	pr_info("hwmon_sensor device!");
	if (retval != 0)
		return retval;

#if defined(CONFIG_CUSTOM_KERNEL_ACCELEROMETER)
	retval = platform_device_register(&sensor_gsensor);
	pr_info("sensor_gsensor device!");
	if (retval != 0)
		return retval;
#endif

#if defined(CONFIG_CUSTOM_KERNEL_MAGNETOMETER)
	retval = platform_device_register(&sensor_msensor);
	pr_info("sensor_msensor device!");
	if (retval != 0)
		return retval;

	retval = platform_device_register(&sensor_orientation);
	pr_info("sensor_osensor device!");
	if (retval != 0)
		return retval;

#endif

#if defined(CONFIG_CUSTOM_KERNEL_GYROSCOPE)
	retval = platform_device_register(&sensor_gyroscope);
	pr_info("sensor_gyroscope device!");
	if (retval != 0)
		return retval;
#endif

#if defined(CUSTOM_KERNEL_BAROMETER)
	retval = platform_device_register(&sensor_barometer);
	pr_info("sensor_barometer device!");
	if (retval != 0)
		return retval;
#endif

#if defined(CONFIG_CUSTOM_KERNEL_ALSPS)
	retval = platform_device_register(&sensor_alsps);
	pr_info("sensor_alsps device!");
	if (retval != 0)
		return retval;
#endif
#endif

#if defined(CONFIG_MTK_USBFSH)
	pr_info("register musbfsh device\n");
	retval = platform_device_register(&mt_usb11_dev);
	if (retval != 0) {
		pr_info("register musbfsh device fail!\n");
		return retval;
	}
#endif

#if defined(CONFIG_USB_MTK_HDRC)
	pr_debug("mt_device_usb register\n");
	retval = platform_device_register(&mt_device_usb);
	if (retval != 0) {
		pr_info("mt_device_usb register fail\n");
		return retval;
	}
#endif

	/* init battery driver after USB due to charger type detection */
	mt_battery_init();

#if defined(CONFIG_MTK_TOUCHPANEL)
	retval = platform_device_register(&mtk_tpd_dev);
	if (retval != 0)
		return retval;
#endif
#if defined(CONFIG_CUSTOM_KERNEL_OFN)
	retval = platform_device_register(&ofn_driver);
	if (retval != 0)
		return retval;
#endif

#if (defined(CONFIG_MTK_MTD_NAND) || defined(CONFIG_MTK_MMC_LEGACY))
	retval = platform_device_register(&dummychar_device);
	if (retval != 0)
		return retval;
#endif


#if defined(CONFIG_ANDROID_PMEM)
	pdata_multimedia.start = PMEM_MM_START;
	pdata_multimedia.size = PMEM_MM_SIZE;
	pr_info("PMEM start: 0x%lx size: 0x%lx\n", pdata_multimedia.start, pdata_multimedia.size);

	retval = platform_device_register(&pmem_multimedia_device);
	if (retval != 0)
		return retval;
#endif

#if defined(CONFIG_ANDROID_VMEM)
	pdata_vmultimedia.start = PMEM_MM_START;
	pdata_vmultimedia.size = PMEM_MM_SIZE;
	pr_info("VMEM start: 0x%lx size: 0x%lx\n", pdata_vmultimedia.start, pdata_vmultimedia.size);

	retval = platform_device_register(&vmem_multimedia_device);
	if (retval != 0) {
		pr_info("vmem platform register failed\n");
		return retval;
	}
#endif

#ifdef CONFIG_CPU_FREQ
	retval = platform_device_register(&cpufreq_pdev);
	if (retval != 0)
		return retval;
#endif

	retval = platform_device_register(&thermal_pdev);
	if (retval != 0)
		return retval;

	retval = platform_device_register(&ptp_pdev);
	if (retval != 0)
		return retval;

	retval = platform_device_register(&spm_mcdi_pdev);
	if (retval != 0)
		return retval;
/*  */
/* ======================================================================= */
/* Image sensor */
/* ======================================================================= */
#if 1				/* /defined(CONFIG_VIDEO_CAPTURE_DRIVERS) */
	retval = platform_device_register(&sensor_dev);
	if (retval != 0)
		return retval;
#endif
#if 1				/* /defined(CONFIG_VIDEO_CAPTURE_DRIVERS) */
	retval = platform_device_register(&sensor_dev_bus2);
	if (retval != 0)
		return retval;
#endif

/*  */
/* ======================================================================= */
/* Lens motor */
/* ======================================================================= */
#if 1				/* defined(CONFIG_ACTUATOR) */
	retval = platform_device_register(&actuator_dev);
	if (retval != 0)
		return retval;
#endif
/*  */
/* ======================================================================= */
/* Camera ISP */
/* ======================================================================= */

#if !defined(CONFIG_MT8135_FPGA)	/* If not FPGA. */
	pr_debug("ISP platform device registering...\n");
	retval = platform_device_register(&mt_isp_dev);
	if (retval != 0) {
		pr_info("ISP platform device register failed.\n");
		return retval;
	}
#endif

#if 0
	retval = platform_device_register(&mt_eis_dev);
	if (retval != 0)
		return retval;
#endif

#ifdef CONFIG_RFKILL
	retval = platform_device_register(&mt_rfkill_device);
	if (retval != 0)
		return retval;
#endif

#if 1
	retval = platform_device_register(&camera_sysram_dev);
	if (retval != 0)
		return retval;
#endif

#if defined(CONFIG_MTK_GPS)
	retval = platform_device_register(&mt3326_device_gps);
	if (retval != 0)
		return retval;
#endif

	retval = platform_device_register(&camera_pipemgr_dev);
	if (retval != 0)
		return retval;

#if defined(CONFIG_MTK_NFC)
	retval = platform_device_register(&mtk_nfc_6605_dev);
	pr_debug("mtk_nfc_6605_dev register ret %d", retval);
	if (retval != 0)
		return retval;
#endif

	return 0;
}

/*
 * is_pmem_range
 * Input
 *   base: buffer base physical address
 *   size: buffer len in byte
 * Return
 *   1: buffer is located in pmem address range
 *   0: buffer is out of pmem address range
 */
int is_pmem_range(unsigned long *base, unsigned long size)
{
	unsigned long start = (unsigned long)base;
	unsigned long end = start + size;

	/* pr_info("[PMEM] start=0x%p,end=0x%p,size=%d\n", start, end, size); */
	/* pr_info("[PMEM] PMEM_MM_START=0x%p,PMEM_MM_SIZE=%d\n", PMEM_MM_START, PMEM_MM_SIZE); */

	if (start < PMEM_MM_START)
		return 0;
	if (end >= PMEM_MM_START + PMEM_MM_SIZE)
		return 0;

	return 1;
}
EXPORT_SYMBOL(is_pmem_range);

/* return the actual physical DRAM size */
unsigned int mtk_get_max_DRAM_size(void)
{
	return kernel_mem_sz + RESERVED_MEM_MODEM;
}

unsigned int get_phys_offset(void)
{
	return PHYS_OFFSET;
}
EXPORT_SYMBOL(get_phys_offset);


#include <asm/sections.h>
void get_text_region(unsigned int *s, unsigned int *e)
{
	*s = (unsigned int)_text, *e = (unsigned int)_etext;
}
EXPORT_SYMBOL(get_text_region);
