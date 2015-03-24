
#include <linux/of_platform.h>
#include <linux/bootmem.h>
#include <asm/mach/time.h>
#include <asm/mach/map.h>
#include <mach/mt_reg_base.h>
#include <mach/mtk_boot_share_page.h>
#include <mach/irqs.h>
#include <asm/page.h>
#include <mach/mt_mci.h>
#include <linux/bug.h>
#include <linux/memblock.h>
#include <linux/version.h>
#include <mach/mt_board.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#include <linux/of_irq.h>
#include <asm/hardware/gic.h>
#else
#include <linux/irqchip.h>
#endif


void __init mt_init_early(void)
{
	int ret;

	ret = reserve_bootmem(__pa(BOOT_SHARE_BASE), 0x1000, BOOTMEM_EXCLUSIVE);
	if (ret < 0) {
		pr_warn("reserve_bootmem BOOT_SHARE_BASE failed %d\n", ret);
	}
}

static struct map_desc mt_io_desc[] __initdata = {
	{
	 .virtual = VDEC_GCON_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(VDEC_GCON_BASE)),
	 .length = SZ_128K,
	 .type = MT_DEVICE},
	{
	 .virtual = VDEC_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(VDEC_BASE)),
	 .length = SZ_64K,
	 .type = MT_DEVICE},
	/*
	   {
	   .virtual = MCUSYS_CFGREG_BASE,
	   .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(MCUSYS_CFGREG_BASE)),
	   .length = SZ_64K,
	   .type = MT_DEVICE
	   },
	   {
	   .virtual = CORTEXA7MP_BASE,
	   .pfn = __phys_to_pfn(CORTEXA7MP_BASE),
	   .length = SZ_1M,
	   .type = MT_DEVICE
	   }, */
	{
	 .virtual = TOPRGU_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(TOPRGU_BASE)),
	 .length = SZ_4M,
	 .type = MT_DEVICE},
	/* {
	   .virtual = DEBUGTOP_BASE,
	   .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(DEBUGTOP_BASE)),
	   .length = SZ_1M,
	   .type = MT_DEVICE
	   }, */
	{
	 .virtual = AP_DMA_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(AP_DMA_BASE)),
	 .length = SZ_2M + SZ_1M,
	 .type = MT_DEVICE},
	{
	 .virtual = SYSRAM_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(SYSRAM_BASE)),
	 .length = SZ_256K,
	 .type = MT_DEVICE},
	{
	 .virtual = DISPSYS_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(DISPSYS_BASE)),
	 .length = SZ_16M,
	 .type = MT_DEVICE},
	{
	 .virtual = IMGSYS_CONFG_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(IMGSYS_CONFG_BASE)),
	 .length = SZ_16M,
	 .type = MT_DEVICE},
	{
	 .virtual = AUDIO_TOP_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(AUDIO_TOP_BASE)),
	 .length = SZ_64K,
	 .type = MT_DEVICE},
	{
	 .virtual = VENC_TOP_BASE,
	 .pfn = __phys_to_pfn(IO_VIRT_TO_PHYS(VENC_TOP_BASE)),
	 .length = SZ_64K,
	 .type = MT_DEVICE},
	{
	 .virtual = DEVINFO_BASE,
	 .pfn = __phys_to_pfn(0x08000000),
	 .length = SZ_64K,
	 .type = MT_DEVICE},
	{
	 .virtual = INTER_SRAM,
	 .pfn = __phys_to_pfn(0x00100000),
	 .length = SZ_64K,
	 .type = MT_DEVICE},


};

void __init mt_map_io(void)
{
	iotable_init(mt_io_desc, ARRAY_SIZE(mt_io_desc));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static struct of_device_id mt_irq_match[] __initdata = {
	{.compatible = "arm,cortex-a9-gic", .data = gic_of_init,},
	{}
};

#define irqchip_init()  \
	of_irq_init(mt_irq_match)
#endif				/* < kernel 3.10 */

void __init mt_dt_init_irq(void)
{
	irqchip_init();
	mt_init_irq();
}

void __init mt_reserve(void)
{
#if defined(CONFIG_MTK_RAM_CONSOLE_USING_DRAM)
	memblock_reserve(CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR, CONFIG_MTK_RAM_CONSOLE_DRAM_SIZE);
#endif

#ifdef CONFIG_MDUMP
	/* reserve lk memory & mdump buffer */
#ifdef BOOTLOADER_LK_ADDRESS
	memblock_reserve(BOOTLOADER_LK_ADDRESS, BOOTLOADER_LK_SIZE);
#endif
	memblock_reserve(CONFIG_MDUMP_BUFFER_ADDRESS, CONFIG_MDUMP_MESSAGE_SIZE * 2);
#endif
}
