#ifndef __TRACER_CONFIG_H__
#define __TRACER_CONFIG_H__

#include <mach/mt_reg_base.h>
#include <mach/sync_write.h>

#define ETR_RAM_PHY_BASE  (0x00100000 + 0xF800)
#define ETR_RAM_VIR_BASE  (INTER_SRAM + 0xF800)
#define ETR_RAM_SIZE 0x200

#define ETM_NO (4)

#define ETM0_BASE	(IO_VIRT_TO_PHYS(DEBUGTOP_BASE) + 0x7C000)
#define ETM1_BASE       (IO_VIRT_TO_PHYS(DEBUGTOP_BASE) + 0x7D000)
#define ETM2_BASE       (IO_VIRT_TO_PHYS(DEBUGTOP_BASE) + 0x7E000)
#define ETM3_BASE       (IO_VIRT_TO_PHYS(DEBUGTOP_BASE) + 0x7F000)

/* #define ETB_BASE      (DEBUGTOP_BASE + 0x11000) */
#define ETB_BASE_PHY    (IO_VIRT_TO_PHYS(DEBUGTOP_BASE) + 0x13000)

/* ETR base */
#define ETR_BASE_PHY    (IO_VIRT_TO_PHYS(DEBUGTOP_BASE) + 0x13000)
#define FUNNEL_BASE_PHY	(IO_VIRT_TO_PHYS(DEBUGTOP_BASE) + 0x14000)


/* DEM Registers*/
#define DEM_BASE   DEBUGTOP_BASE + 0x1A000

#define DBGRST_ALL        (DEM_BASE + 0x028)
#define DBGBUSCLK_EN      (DEM_BASE + 0x02C)
#define DBGSYSCLK_EN      (DEM_BASE + 0x030)
#define AHBAP_EN          (DEM_BASE + 0x040)
#define DEM_UNLOCK        (DEM_BASE + 0xFB0)

#define AHB_EN            1 << 0
#define POWER_ON_RESET    0 << 0
#define SYSCLK_EN         1 << 0
#define BUSCLK_EN         1 << 0

#define DEM_UNLOCK_MAGIC  0xC5ACCE55


#define DBGAPB_BASE   DEBUGTOP_BASE
#define CPUDBG0_BASE  DBGAPB_BASE+0x00070000
#define CPUPMU0_BASE  DBGAPB_BASE+0x00071000
#define CPUDBG1_BASE  DBGAPB_BASE+0x00072000
#define CPUPMU1_BASE  DBGAPB_BASE+0x00073000
#define CPUDBG2_BASE  DBGAPB_BASE+0x00074000
#define CPUPMU2_BASE  DBGAPB_BASE+0x00075000
#define CPUDBG3_BASE  DBGAPB_BASE+0x00076000
#define CPUPMU3_BASE  DBGAPB_BASE+0x00077000
#define CTI0_BASE     DBGAPB_BASE+0x00078000
#define CTI1_BASE     DBGAPB_BASE+0x00079000
#define CTI2_BASE     DBGAPB_BASE+0x0007A000
#define CTI3_BASE     DBGAPB_BASE+0x0007B000
#define PTM0_BASE     DBGAPB_BASE+0x0007C000
#define PTM1_BASE     DBGAPB_BASE+0x0007D000
#define PTM2_BASE     DBGAPB_BASE+0x0007E000
#define PTM3_BASE     DBGAPB_BASE+0x0007F000

/* #define ETB_BASE      DBGAPB_BASE+0x00011000 */
/* ETR base */
#define ETB_BASE      DBGAPB_BASE+0x00013000

#define CTI_BASE      DBGAPB_BASE+0x00012000
#define TPIU_BASE     DBGAPB_BASE+0x00013000
#define FUNNEL_BASE   DBGAPB_BASE+0x00014000

#define STP_BASE      DBGAPB_BASE+0x00019000
#define DEM_BASE      DBGAPB_BASE+0x0001A000
#define SATATX_BASE   DBGAPB_BASE+0x0001B000


static inline void downgrade_cpu2etm_clk(void)
{
	unsigned int faxi_clk;
	/* downgrade CA7 to ETM clk */
	faxi_clk = readl(TOPRGU_BASE + 0x0140);
	faxi_clk &= 0xFFFFFFF0;
	faxi_clk |= 0x3;
	pr_info("[ETM] faxi_clk = %x\n", faxi_clk);
	mt65xx_reg_sync_writel(faxi_clk, TOPRGU_BASE + 0x0140);
}

#endif
