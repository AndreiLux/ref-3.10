#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include <asm/system.h>
#include <mach/mt_reg_base.h>
/* #include <asm/hardware/cache-l2x0.h> */
#include <mach/mt_clkmgr.h>
#if 0

#define DRAMC_PHYCTL1 (DRAM_CTR_BASE + 0x00F0)
#define DRAMC_GDDR3CTL1 (DRAM_CTR_BASE + 0x00F4)
#define LPDDR_ACTIM_VAL_MASK 0x0000F000
/* #define LPDDR_ACTIM_VAL 0x660302C7  // tRCD=7T, tRP=7T, tFAW=don't care, tWR=4T, CL=3, tWTR=1T, tRC=18T, tRAS=11T */
/* #define LPDDR2_ACTIM_VAL 0x44D905A8  // tRCD=5T, tRP=5T, tFAW=14T, tWR=9T, CL=3, tWTR=5T, tRC=16T, tRAS=12T */
#define LPDDR_CONF1_VAL_MASK 0xFFFFFF3F
/* #define LPDDR_CONF1_VAL 0x000000C0  // tRRD=4T */
/* #define LPDDR2_CONF1_VAL 0x00000080  // tRRD=3T */
#define LPDDR_TEST2_3_VAL_MASK 0xFFE0FFFF
/* #define LPDDR_TEST2_3_VAL 0x00000000    // tRFC=18T */
/* #define LPDDR2_TEST2_3_VAL 0x00040000    // tRFC=25T */
#define LPDDR_PD_CTR_VAL_MASK 0xFFFF00FF
/* #define LPDDR_PD_CTR_VAL 0x00002C00 // tXSR=44T */
/* #define LPDDR2_PD_CTR_VAL 0x00001E00 // tXSR=30T */
#endif
/* LPDDR clk 239.2 */
#define LPDDR_ACTIM_VAL    0x44030296
#define LPDDR_CONF1_VAL    0x00000080
#define LPDDR_TEST2_3_VAL  0x000D0000
#define LPDDR_PD_CTR_VAL   0x00003000

/* LPDDR2 clk 266 */
#define LPDDR2_ACTIM_VAL   0x67D8054F
#define LPDDR2_CONF1_VAL   0x00000080
#define LPDDR2_TEST2_3_VAL 0x000E0000
#define LPDDR2_PD_CTR_VAL  0x00002600

static int dram_clk;
static spinlock_t lock;

/*
 * get_ddr_type: check ddr type.
 * Return 2 for LPDDR2, 1 for LPDDR.
 */
static int get_ddr_type(void)
{
#if 0
	return (*(volatile unsigned int *)(DRAM_CTR_BASE + 0x00D8) & 0xC0000000)
	    ? ((*(volatile unsigned int *)(APMIXED_BASE + 0x0204) & 0x00000020) ? 1 : 3) : 2;
#endif
}

/*
 * lpddr_overclock: overclock the LPDDR.
 * @clk: target clock rate.
 * Return error code.
 */
static int lpddr_overclock(int clk)
{
	char str[KSYM_SYMBOL_LEN];
	unsigned long size;
	unsigned long flags;
	unsigned int addr, end, data;
	register int delay, reg_pll_con0;

	switch (clk) {
	case 192...197:
		reg_pll_con0 = 0x2460;
		dram_clk = 192;
		break;
	case 198...202:
		reg_pll_con0 = 0x2560;
		dram_clk = 198;
		break;
	case 203...207:
		reg_pll_con0 = 0x2660;
		dram_clk = 203;
		break;
	case 208...212:
		reg_pll_con0 = 0x2760;
		dram_clk = 208;
		break;
	case 213...217:
		reg_pll_con0 = 0x2860;
		dram_clk = 213;
		break;
	case 218...223:
		reg_pll_con0 = 0x2960;
		dram_clk = 218;
		break;
	case 224...228:
		reg_pll_con0 = 0x2A60;
		dram_clk = 224;
		break;
	case 229...233:
		reg_pll_con0 = 0x2B60;
		dram_clk = 229;
		break;
	case 234...238:
		reg_pll_con0 = 0x2C60;
		dram_clk = 234;
		break;
	case 239:
		reg_pll_con0 = 0x2D60;
		dram_clk = 239;
		break;
	default:
		return -1;

	}

	kallsyms_lookup((unsigned long)lpddr_overclock, &size, NULL, NULL, str);
	addr = (unsigned int)lpddr_overclock;
	/* preload addtional 256 bytes to cover CPU/cache prefetch */
	end = addr + size + 256;

	/*
	 * NoteXXX: The DRAM controller is not allowed to change the DRAM clock rate directly. Need to follow below procedures:
	 *          1. Flush cache.
	 *          2. Run code internally to enforce not to access DRAM.
	 *             (Lockdown code and data in the cache.)
	 *          3. Memory barrier.
	 *          4. DRAM enters the self refresh state.
	 *          5. Change the clock rate.
	 *          6. Set new AC timing parameters.
	 *          7. DRAM leaves the self refresh state.
	 */

	spin_lock_irqsave(&lock, flags);

	flush_cache_all();
	/* outer_flush_all(); */

	for (; addr < end; addr += 4) {
		data = *(volatile unsigned int *)addr;
	}

#ifdef PL310
	*(volatile unsigned int *)(PL310_BASE + L2X0_LOCKDOWN_WAY_D) = 0x0000FFFF;
	*(volatile unsigned int *)(PL310_BASE + L2X0_LOCKDOWN_WAY_I) = 0x0000FFFF;

	dsb();
	*(volatile unsigned int *)(PL310_BASE + L2X0_CACHE_SYNC) = 0;
#endif

#if 0				/* there is no need to disable EMI and DRAMC */
	/* disable EMI */
	*(volatile unsigned int *)(EMI_BASE + 0x0060) &= ~0x00000400;

	/* turn off CTO protocol */
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x01E0) &= ~0x20000000;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x01E0) |= 0x80000000;
#endif

#if 0
	/* enter DRAM self-refresh */
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0004) |= 0x04000000;
#endif
	/* delay for at least 10us */
#if 0
	for (delay = 32768; delay != 0; delay--)
		;
#endif
	asm volatile ("mov %0, #2\n"
		      "mov %0, %0, LSL #15\n"
		      "1:\n"
		      "subs %0, %0, #1\n"
		      "bne 1b\n"
		      : "=r" (delay)
		      : : "cc");

	/* overclock */
#if 0
	/*
	 * NoteXXX: Cannot change the EMI's and DRAMC's clock rate directly.
	 *          Need to set the PLL to change the clock rate first.
	 *          Then use the jitter-free MUX to switch the clock source.
	 */
	*(volatile unsigned int *)MAINPLL_CON0 = reg_pll_con0;
#if 0
	for (delay = 0; delay < 120; delay++)
		;
#endif
	asm volatile ("mov %0, #2\n"
		      "mov %0, %0, LSL #7\n"
		      "1:\n"
		      "subs %0, %0, #1\n"
		      "bne 1b\n"
		      : "=r" (delay)
		      : : "cc");
	*(volatile unsigned int *)TOP_CKDIV0 = 0x10;
#endif

#if 0
	/* follow the JEDEC spec, independent on the DRAM part */
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0000) &= LPDDR_ACTIM_VAL_MASK;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0000) |= LPDDR_ACTIM_VAL;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0004) &= LPDDR_CONF1_VAL_MASK;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0004) |= LPDDR_CONF1_VAL;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0044) &= LPDDR_TEST2_3_VAL_MASK;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0044) |= LPDDR_TEST2_3_VAL;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x01DC) &= LPDDR_PD_CTR_VAL_MASK;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x01DC) |= LPDDR_PD_CTR_VAL;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0008) &= ~0x000000FF;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0008) |= 0x00000060;
#endif

	/* PHY RESET */
	*(volatile unsigned int *)DRAMC_PHYCTL1 |= (1 << 28);
	*(volatile unsigned int *)DRAMC_GDDR3CTL1 |= (1 << 25);
#if 0
	for (delay = 1024; delay != 0; delay--)
		;
#endif
	asm volatile ("mov %0, #2\n"
		      "mov %0, %0, LSL #10\n"
		      "1:\n"
		      "subs %0, %0, #1\n"
		      "bne 1b\n"
		      : "=r" (delay)
		      : : "cc");
	*(volatile unsigned int *)DRAMC_PHYCTL1 &= ~(1 << 28);
	*(volatile unsigned int *)DRAMC_GDDR3CTL1 &= ~(1 << 25);

	/* exit DRAM self-refresh */
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0004) &= ~0x04000000;

#if 0
	/* turn on CTO protocol */
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x01E0) |= 0x20000000;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x01E0) &= ~0x80000000;

	/* enable EMI */
	*(volatile unsigned int *)(EMI_BASE + 0x0060) |= 0x00000400;
#endif

#ifdef PL310
	*(volatile unsigned int *)(PL310_BASE + L2X0_LOCKDOWN_WAY_D) = 0x00000000;
	*(volatile unsigned int *)(PL310_BASE + L2X0_LOCKDOWN_WAY_I) = 0x00000000;
#endif
	spin_unlock_irqrestore(&lock, flags);

	mt6577_uart_update_sysclk();

	return 0;
}

/*
 * lpddr2_overclock: overclock the LPDDR2.
 * @clk: target clock rate.
 * Return error code.
 */
static int lpddr2_overclock(int clk)
{
	char str[KSYM_SYMBOL_LEN];
	unsigned long size;
	unsigned long flags;
	unsigned int addr, end, data;
	register int delay, reg_pll_con0;

	switch (clk) {
	case 260...266:
		reg_pll_con0 = 0x2760;
		dram_clk = 260;
		break;
	case 267...272:
		reg_pll_con0 = 0x2860;
		dram_clk = 267;
		break;
	case 273...279:
		reg_pll_con0 = 0x2960;
		dram_clk = 273;
		break;
	case 280...285:
		reg_pll_con0 = 0x2A60;
		dram_clk = 280;
		break;
	case 286...292:
		reg_pll_con0 = 0x2B60;
		dram_clk = 286;
		break;
	case 293...298:
		reg_pll_con0 = 0x2C60;
		dram_clk = 293;
		break;
	case 299:
		reg_pll_con0 = 0x2D60;
		dram_clk = 299;
		break;
	default:
		return -1;
	}

	kallsyms_lookup((unsigned long)lpddr2_overclock, &size, NULL, NULL, str);
	addr = (unsigned int)lpddr2_overclock;
	/* preload addtional 256 bytes to cover CPU/cache prefetch */
	end = addr + size + 256;

	/*
	 * NoteXXX: The DRAM controller is not allowed to change the DRAM clock rate directly. Need to follow below procedures:
	 *          1. Flush cache.
	 *          2. Run code internally to enforce not to access DRAM.
	 *             (Lockdown code and data in the cache.)
	 *          3. Memory barrier.
	 *          4. DRAM enters the self refresh state.
	 *          5. Change the clock rate.
	 *          6. Set new AC timing parameters.
	 *          7. DRAM leaves the self refresh state.
	 */

	spin_lock_irqsave(&lock, flags);

	flush_cache_all();
	outer_flush_all();

	for (; addr < end; addr += 4) {
		data = *(volatile unsigned int *)addr;
	}

#ifdef PL310
	*(volatile unsigned int *)(PL310_BASE + L2X0_LOCKDOWN_WAY_D) = 0x0000FFFF;
	*(volatile unsigned int *)(PL310_BASE + L2X0_LOCKDOWN_WAY_I) = 0x0000FFFF;

	dsb();
	*(volatile unsigned int *)(PL310_BASE + L2X0_CACHE_SYNC) = 0;
#endif
	/* enter DRAM self-refresh */
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0004) |= 0x04000000;

	/* delay for at least 10us */
#if 0
	for (delay = 32768; delay != 0; delay--)
		;
#endif
	asm volatile ("mov %0, #2\n"
		      "mov %0, %0, LSL #15\n"
		      "1:\n"
		      "subs %0, %0, #1\n"
		      "bne 1b\n"
		      : "=r" (delay)
		      : : "cc");

	/* overclock */
	*(volatile unsigned int *)0xF000706C = 0x0001;
	*(volatile unsigned int *)MAINPLL_CON0 = reg_pll_con0;
#if 0
	for (delay = 0; delay < 120; delay++)
		;
#endif
	asm volatile ("mov %0, #2\n"
		      "mov %0, %0, LSL #7\n"
		      "1:\n"
		      "subs %0, %0, #1\n"
		      "bne 1b\n"
		      : "=r" (delay)
		      : : "cc");
	*(volatile unsigned int *)(TOP_CKDIV0) = 0x8;

	/* follow the JEDEC spec, independent on the DRAM part */
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0000) &= LPDDR_ACTIM_VAL_MASK;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0000) |= LPDDR2_ACTIM_VAL;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0004) &= LPDDR_CONF1_VAL_MASK;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0004) |= LPDDR2_CONF1_VAL;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0044) &= LPDDR_TEST2_3_VAL_MASK;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0044) |= LPDDR2_TEST2_3_VAL;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x01DC) &= LPDDR_PD_CTR_VAL_MASK;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x01DC) |= LPDDR2_PD_CTR_VAL;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0008) &= ~0x000000FF;
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0008) |= 0x00000060;

	/* PHY RESET */
	*(volatile unsigned int *)DRAMC_PHYCTL1 |= (1 << 28);
	*(volatile unsigned int *)DRAMC_GDDR3CTL1 |= (1 << 25);
#if 0
	for (delay = 1024; delay != 0; delay--)
		;
#endif
	asm volatile ("mov %0, #2\n"
		      "mov %0, %0, LSL #10\n"
		      "1:\n"
		      "subs %0, %0, #1\n"
		      "bne 1b\n"
		      : "=r" (delay)
		      :  : "cc");
	*(volatile unsigned int *)DRAMC_PHYCTL1 &= ~(1 << 28);
	*(volatile unsigned int *)DRAMC_GDDR3CTL1 &= ~(1 << 25);

	/* exit DRAM self-refresh */
	*(volatile unsigned int *)(DRAM_CTR_BASE + 0x0004) &= ~0x04000000;

#ifdef PL310
	*(volatile unsigned int *)(PL310_BASE + L2X0_LOCKDOWN_WAY_D) = 0x00000000;
	*(volatile unsigned int *)(PL310_BASE + L2X0_LOCKDOWN_WAY_I) = 0x00000000;
#endif
	spin_unlock_irqrestore(&lock, flags);

	mt6577_uart_update_sysclk();

	return 0;
}

static ssize_t dram_overclock_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d,%d\n", get_ddr_type(), dram_clk);
}

static ssize_t dram_overclock_store(struct device_driver *driver, const char *buf, size_t count)
{
	int clk;

	clk = simple_strtol(buf, 0, 10);

	if (get_ddr_type() == 2) {
		if (clk >= 260 && clk <= 272) {
			lpddr2_overclock(clk);
		}
	} else if (get_ddr_type() == 1) {
		if (clk >= 192 && clk <= 239) {
			lpddr_overclock(clk);
		}
	}

	return count;
}

DRIVER_ATTR(emi_clk_test, 0664, dram_overclock_show, dram_overclock_store);

static struct platform_driver dram_overclock_drv = {
	.driver = {
		   .name = "emi_clk_test",
		   .bus = &platform_bus_type,
		   .owner = THIS_MODULE,
		   },
};

/* This function is used by Modem side to get the ddr type and running clock */
int get_dram_info(int *clk, int *type)
{

	int clock = 0;
	int pll_con0 = *(volatile unsigned int *)MAINPLL_CON0;

	*type = get_ddr_type();


	if (*type == 2) {
		switch (pll_con0) {
		case 0x2760:
			clock = 260;
			break;
		case 0x2860:
			clock = 267;
			break;
		case 0x2960:
			clock = 273;
		case 0x2A60:
			clock = 280;
			break;
		case 0x2B60:
			clock = 286;
			break;
		case 0x2C60:
			clock = 293;
			break;
		case 0x2D60:
			clock = 299;
			break;
		default:
			return -1;
		}
	} else if (*type == 1) {
		switch (pll_con0) {
		case 0x2460:
			clock = 192;
			break;
		case 0x2560:
			clock = 198;
			break;
		case 0x2660:
			clock = 203;
			break;
		case 0x2760:
			clock = 208;
			break;
		case 0x2860:
			clock = 213;
			break;
		case 0x2960:
			clock = 218;
			break;
		case 0x2A60:
			clock = 224;
			break;
		case 0x2B60:
			clock = 229;
			break;
		case 0x2C60:
			clock = 234;
			break;
		case 0x2D60:
			clock = 239;
			break;
		default:
			return -1;

		}

	} else if (*type == 3) {

		switch (pll_con0) {
		case 0x4CA0:
			clock = 333;
			break;
		case 0x4DA0:
			clock = 338;
			break;
		case 0x4EA0:
			clock = 342;
			break;
		case 0x4FA0:
			clock = 347;
			break;
		case 0x50A0:
			clock = 351;
			break;
		case 0x51A0:
			clock = 355;
			break;
		case 0x52A0:
			clock = 360;
			break;
		case 0x53A0:
			clock = 364;
			break;
		case 0x54A0:
			clock = 368;
			break;
		case 0x55A0:
			clock = 373;
			break;
		default:
			return -1;
		}
	}
	*clk = clock;

	return 0;
}
EXPORT_SYMBOL(get_dram_info);
int __init dram_overclock_init(void)
{
#if 0
	int ret;

	if (get_ddr_type() == 2) {
		dram_clk = 260;
	} else {
		dram_clk = 192;
	}

	ret = driver_register(&dram_overclock_drv);
	if (ret) {
		pr_info("fail to create the dram_overclock driver\n");
		return ret;
	}
	ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_clk_test);
	if (ret) {
		pr_info("fail to create the dram_overclock sysfs files\n");
		return ret;
	}
#endif
	spin_lock_init(&lock);

	return 0;
}
arch_initcall(dram_overclock_init);
