#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>	/* udelay */

#include <mach/mt_typedefs.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/hotplug.h>
#include <mach/mt_clkmgr.h>
#include <mach/pmic_mt6320_sw.h>
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>
#include <mach/upmu_hw.h>


/**************************************
 * for CPU MTCMOS
 **************************************/
/*
 * regiser bit definition
 */
/* SPM_FC1_PWR_CON */
#define SRAM_ISOINT_B   (1U << 6)
#define SRAM_CKISO      (1U << 5)
#define PWR_CLK_DIS     (1U << 4)
#define PWR_ON_S        (1U << 3)
#define PWR_ON          (1U << 2)
#define PWR_ISO         (1U << 1)
#define PWR_RST_B       (1U << 0)

/* SPM_CPU_FC1_L1_PDN */
#define L1_PDN_ACK      (1U << 8)
#define L1_PDN          (1U << 0)

/* SPM_CA15 */
#define CPU0_L1_PDN_ACK (1U << 8)
#define CPU0_L1_PDN     (1U << 0)
#define CPU0_L1_PDN_ISO (1U << 4)
#define CPU1_L1_PDN_ACK (1U << 9)
#define CPU1_L1_PDN     (1U << 1)
#define CPU1_L1_PDN_ISO (1U << 5)

/* SPM_CA15 L2*/
#define CA15_L2_PDN_ACK (1U << 8)
#define CA15_L2_PDN     (1U << 0)
#define CA15_L2_ISO     (1U << 4)


/* SPM_PWR_STATUS */
/* SPM_PWR_STATUS_S */
#define CA7_FC1          (1U << 11)
#define CA15_CX0         (1U << 17)
#define CA15_CX1         (1U << 18)
#define CA15_CPU0        (1U << 19)
#define CA15_CPU1        (1U << 20)
#define CA15_CPUTOP      (1U << 21)

/* SPM_SLEEP_TIMER_STA */
#define APMCU3_SLEEP    (1U << 18)
#define APMCU2_SLEEP    (1U << 17)
#define APMCU1_SLEEP    (1U << 16)
#define CA15_L2_WFI     (1U << 20)

#define CA15_L2RSTDISABLE (1U << 10)

#define EXT_CA15_OFF    (1U << 1)

/*****************************************
* CA15 cluster off w/ PLL and buck off
******************************************/
#define EXTERNAL_OFF

/*****************************************
* PMIC BUCK settle time, should not be changed
******************************************/
#define PMIC_BUCK_SETTLE_TIME (130)	/* us */


static DEFINE_SPINLOCK(spm_cpu_lock);

void spm_mtcmos_cpu_lock(unsigned long *flags)
{
	spin_lock_irqsave(&spm_cpu_lock, *flags);
}

void spm_mtcmos_cpu_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&spm_cpu_lock, *flags);
}

int spm_mtcmos_ctrl_cpu0(int state)
{
/*
	if (state == STA_POWER_DOWN) {

	} else {

	}
*/
	return 0;
}

int spm_mtcmos_ctrl_cpu1(int state)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	spm_mtcmos_cpu_lock(&flags);

	if (state == STA_POWER_DOWN) {
		while ((spm_read(SPM_SLEEP_TIMER_STA) & APMCU1_SLEEP) == 0)
			;

		spm_write(SPM_FC1_PWR_CON, spm_read(SPM_FC1_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_FC1_PWR_CON, spm_read(SPM_FC1_PWR_CON) & ~SRAM_ISOINT_B);

		spm_write(SPM_CPU_FC1_L1_PDN, spm_read(SPM_CPU_FC1_L1_PDN) | L1_PDN);
		while ((spm_read(SPM_CPU_FC1_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK)
			;

		spm_write(SPM_FC1_PWR_CON, spm_read(SPM_FC1_PWR_CON) | PWR_ISO);
		spm_write(SPM_FC1_PWR_CON, (spm_read(SPM_FC1_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);

		spm_write(SPM_FC1_PWR_CON, spm_read(SPM_FC1_PWR_CON) & ~PWR_ON);
		spm_write(SPM_FC1_PWR_CON, spm_read(SPM_FC1_PWR_CON) & ~PWR_ON_S);
		while (((spm_read(SPM_PWR_STATUS) & CA7_FC1) !=
			0) | ((spm_read(SPM_PWR_STATUS_S) & CA7_FC1) != 0))
			;

	} else {		/* STA_POWER_ON */

		spm_write(SPM_FC1_PWR_CON, spm_read(SPM_FC1_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_FC1_PWR_CON, spm_read(SPM_FC1_PWR_CON) | PWR_ON_S);
		while (((spm_read(SPM_PWR_STATUS) & CA7_FC1) !=
			CA7_FC1) | ((spm_read(SPM_PWR_STATUS_S) & CA7_FC1) != CA7_FC1))
			;

		spm_write(SPM_FC1_PWR_CON, spm_read(SPM_FC1_PWR_CON) & ~PWR_CLK_DIS);

		spm_write(SPM_CPU_FC1_L1_PDN, spm_read(SPM_CPU_FC1_L1_PDN) & ~L1_PDN);
		while ((spm_read(SPM_CPU_FC1_L1_PDN) & L1_PDN_ACK) != 0)
			;

		spm_write(SPM_FC1_PWR_CON, spm_read(SPM_FC1_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_FC1_PWR_CON, spm_read(SPM_FC1_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_FC1_PWR_CON, spm_read(SPM_FC1_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_FC1_PWR_CON, spm_read(SPM_FC1_PWR_CON) | PWR_RST_B);

	}

	spm_mtcmos_cpu_unlock(&flags);

	return 0;

}

/* CA15 core0 */
int spm_mtcmos_ctrl_cpu2(int state)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	spm_mtcmos_cpu_lock(&flags);

#ifdef EXTERNAL_OFF		/* VCA15 on */
	if ((spm_read(SPM_MD1_PWR_CON) & EXT_CA15_OFF) != EXT_CA15_OFF) {
		/* HOTPLUG_INFO("EXTERNAL DC/DC and PLL ON\n"); */
		upmu_set_vca15_en_nolock(1);
		upmu_set_vsrmca15_en_nolock(1);
		udelay(PMIC_BUCK_SETTLE_TIME);

		enable_pll(ARMPLL1, "CPU_HOTPLUG");

		spm_write(SPM_MD1_PWR_CON, spm_read(SPM_MD1_PWR_CON) | EXT_CA15_OFF);
/* spm_write(TOP_CKMUXSEL, spm_read(TOP_CKMUXSEL) & ~0x30 ); // Swithc to 26M */
	}
#endif

	if (state == STA_POWER_DOWN) {
		while ((spm_read(SPM_SLEEP_TIMER_STA) & APMCU2_SLEEP) == 0)
			;

		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU0_L1_PDN);
		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU0_L1_PDN_ACK) != CPU0_L1_PDN_ACK)
			;

		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU0_L1_PDN_ISO);

		spm_write(SPM_CA15_CX0_PWR_CON, spm_read(SPM_CA15_CX0_PWR_CON) | PWR_ISO);
		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA15_CX0_PWR_CON, spm_read(SPM_CA15_CX0_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CX0_PWR_CON, spm_read(SPM_CA15_CX0_PWR_CON) & ~PWR_ON_S);
		while (((spm_read(SPM_PWR_STATUS) & CA15_CX0) !=
			0) | ((spm_read(SPM_PWR_STATUS_S) & CA15_CX0) != 0))
			;
		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ON_S);
		while (((spm_read(SPM_PWR_STATUS) & CA15_CPU0) !=
			0) | ((spm_read(SPM_PWR_STATUS_S) & CA15_CPU0) != 0))
			;

		/* Turn off Cluster */
		if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU1 | CA15_CX1)) &&
		    !(spm_read(SPM_PWR_STATUS_S) & (CA15_CPU1 | CA15_CX1))) {
			spm_mtcmos_ctrl_cpusys(state);
		}

	} else {		/* STA_POWER_ON */
		/* check CA15 TOP */
		if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) && !(spm_read(SPM_PWR_STATUS_S) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys(state);

		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_RST_B);	/* RST */

		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ON);
		while ((spm_read(SPM_PWR_STATUS) & CA15_CPU0) != CA15_CPU0)
			;
		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ON_S);
		while ((spm_read(SPM_PWR_STATUS_S) & CA15_CPU0) != CA15_CPU0)
			;

		spm_write(SPM_CA15_CX0_PWR_CON, spm_read(SPM_CA15_CX0_PWR_CON) | PWR_ON);
		while ((spm_read(SPM_PWR_STATUS) & CA15_CX0) != CA15_CX0)
			;
		spm_write(SPM_CA15_CX0_PWR_CON, spm_read(SPM_CA15_CX0_PWR_CON) | PWR_ON_S);
		while ((spm_read(SPM_PWR_STATUS_S) & CA15_CX0) != CA15_CX0)
			;

		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU0_L1_PDN_ISO);
		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU0_L1_PDN);
		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU0_L1_PDN_ACK) != 0)
			;

		spm_write(SPM_CA15_CX0_PWR_CON, spm_read(SPM_CA15_CX0_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ISO);

		udelay(1);	/* >16 CPU CLK */
		spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_RST_B);

	}

	spm_mtcmos_cpu_unlock(&flags);

	return 0;

}

/* CA15 core1 */
int spm_mtcmos_ctrl_cpu3(int state)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	spm_mtcmos_cpu_lock(&flags);

#ifdef EXTERNAL_OFF		/* VCA15 on */
	if ((spm_read(SPM_MD1_PWR_CON) & EXT_CA15_OFF) != EXT_CA15_OFF) {
		upmu_set_vca15_en_nolock(1);
		upmu_set_vsrmca15_en_nolock(1);
		udelay(PMIC_BUCK_SETTLE_TIME);

		enable_pll(ARMPLL1, "CPU_HOTPLUG");

		spm_write(SPM_MD1_PWR_CON, spm_read(SPM_MD1_PWR_CON) | EXT_CA15_OFF);
/* spm_write(TOP_CKMUXSEL, spm_read(TOP_CKMUXSEL) & ~0x30 ); // Swithc to 26M */
	}
#endif

	if (state == STA_POWER_DOWN) {
		while ((spm_read(SPM_SLEEP_TIMER_STA) & APMCU3_SLEEP) == 0)
			;

		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU1_L1_PDN);
		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU1_L1_PDN_ACK) != CPU1_L1_PDN_ACK)
			;

		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU1_L1_PDN_ISO);
		spm_write(SPM_CA15_CX1_PWR_CON, spm_read(SPM_CA15_CX1_PWR_CON) | PWR_ISO);
		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA15_CX1_PWR_CON, spm_read(SPM_CA15_CX1_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CX1_PWR_CON, spm_read(SPM_CA15_CX1_PWR_CON) & ~PWR_ON_S);
		while (((spm_read(SPM_PWR_STATUS) & CA15_CX1) !=
			0) | ((spm_read(SPM_PWR_STATUS_S) & CA15_CX1) != 0))
			;
		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ON_S);
		while (((spm_read(SPM_PWR_STATUS) & CA15_CPU1) !=
			0) | ((spm_read(SPM_PWR_STATUS_S) & CA15_CPU1) != 0))
			;

		/* Turn off Cluster */
		if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU0 | CA15_CX0)) &&
		    !(spm_read(SPM_PWR_STATUS_S) & (CA15_CPU0 | CA15_CX0))) {
			spm_mtcmos_ctrl_cpusys(state);
		}

	} else {		/* STA_POWER_ON */
		/* check CA15 TOP */
		if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) && !(spm_read(SPM_PWR_STATUS_S) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys(state);

		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_RST_B);

		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ON);
		while ((spm_read(SPM_PWR_STATUS) & CA15_CPU1) != CA15_CPU1)
			;
		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ON_S);
		while ((spm_read(SPM_PWR_STATUS_S) & CA15_CPU1) != CA15_CPU1)
			;

		spm_write(SPM_CA15_CX1_PWR_CON, spm_read(SPM_CA15_CX1_PWR_CON) | PWR_ON);
		while ((spm_read(SPM_PWR_STATUS) & CA15_CX1) != CA15_CX1)
			;
		spm_write(SPM_CA15_CX1_PWR_CON, spm_read(SPM_CA15_CX1_PWR_CON) | PWR_ON_S);
		while ((spm_read(SPM_PWR_STATUS_S) & CA15_CX1) != CA15_CX1)
			;

		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU1_L1_PDN_ISO);
		spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU1_L1_PDN);
		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU1_L1_PDN_ACK) != 0)
			;

		spm_write(SPM_CA15_CX1_PWR_CON, spm_read(SPM_CA15_CX1_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ISO);

		udelay(1);	/* >16 CPU CLK */
		spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_RST_B);

	}

	spm_mtcmos_cpu_unlock(&flags);

	return 0;

}

int spm_mtcmos_ctrl_dbg(int state)
{
/*
	if (state == STA_POWER_DOWN) {

	} else {

	}
*/
	return 0;
}

int spm_mtcmos_ctrl_cpusys(int state)
{
	if (state == STA_POWER_DOWN) {

		while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_L2_WFI) == 0)
			;

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);

		spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) | CA15_L2_PDN);
		while ((spm_read(SPM_CA15_L2_PWR_CON) & CA15_L2_PDN_ACK) != CA15_L2_PDN_ACK)
			;
		spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) | CA15_L2_ISO);

		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			  (spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ISO);
		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | SRAM_CKISO);

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ON_S);
		while (((spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) !=
			0) | ((spm_read(SPM_PWR_STATUS_S) & CA15_CPUTOP) != 0))
			;

#ifdef EXTERNAL_OFF
		/* spm_write(0xF0001000, spm_read(0xF0001000) & ~0xC0); // 26MHz */
		spm_write(SPM_MD1_PWR_CON, spm_read(SPM_MD1_PWR_CON) & ~EXT_CA15_OFF);
		disable_pll(ARMPLL1, "CPU_HOTPLUG");
		upmu_set_vca15_en_nolock(0);
		upmu_set_vsrmca15_en_nolock(0);
#endif

	} else {		/* STA_POWER_ON */
		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_RST_B);

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ON);
		while ((spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) != CA15_CPUTOP)
			;
		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ON_S);
		while ((spm_read(SPM_PWR_STATUS_S) & CA15_CPUTOP) != CA15_CPUTOP)
			;

		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ISO);
		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_RST_B);

		spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) & ~CA15_L2_ISO);
		spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) & ~CA15_L2_PDN);
		while ((spm_read(SPM_CA15_L2_PWR_CON) & CA15_L2_PDN_ACK) != 0)
			;

		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			  spm_read(SPM_CA15_CPUTOP_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~SRAM_CKISO);

	}

	return 0;

}

bool spm_cpusys_can_power_down(void)
{
	return !(spm_read(SPM_PWR_STATUS) & 0x003E0800) &&
	    !(spm_read(SPM_PWR_STATUS_S) & 0x003E0800);
}

void spm_mtcmos_init(void)
{
	spm_write(SPM_MD1_PWR_CON, spm_read(SPM_MD1_PWR_CON) | EXT_CA15_OFF);
	spm_write(RGUCFG, spm_read(RGUCFG) & ~0x3);
	/* Set CONFIG_RES[5:0] = 6'b11_1011, to disable CKISO to trigger RGU1 */
	spm_write(CONFIG_RES, spm_read(CONFIG_RES) | 0x3B);
	/* dormant:1 shutdown:0 */
	spm_write(CA15_RST_CTL, spm_read(CA15_RST_CTL) & ~CA15_L2RSTDISABLE);

#if 0
	/* for debug monitor */
	spm_write(0xF0200264, spm_read(0xF0200264) & ~(1 << 9));	/* dfd down sample */
	spm_write(0xF020021c, 0xE);	/* Select ca15_mon_sel */
	spm_write(0xF0200010, 0x6);
#endif
}

MODULE_DESCRIPTION("MT8135 SPM-MTCMOS Driver v0.1");
