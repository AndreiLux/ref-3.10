#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <mach/irqs.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_dormant.h>
#include <mach/wd_api.h>

/* =================================== */

#define mcdi_linux_test_mode 0

#if mcdi_linux_test_mode

#define SPM_MCDI_BYPASS_SYSPWREQ 1

/* 1: enable MCDI + Thermal Protect */
/* 0: Thermal Protect only */
u32 En_SPM_MCDI = 1;

#else

#define SPM_MCDI_BYPASS_SYSPWREQ 0

/* 1: enable MCDI + Thermal Protect */
/* 0: Thermal Protect only */
u32 En_SPM_MCDI = 0;

#endif
/* =================================== */

static struct task_struct *mcdi_task_0;
static struct task_struct *mcdi_task_1;
static struct task_struct *mcdi_task_2;
static struct task_struct *mcdi_task_3;

#define WFI_OP        4
#define WFI_L2C      5
#define WFI_SCU      6

#define mcdi_wfi_with_sync()                         \
do {                                            \
    isb();                                      \
    dsb();                                      \
    __asm__ __volatile__("wfi" : : : "memory"); \
} while (0)

#define read_cntp_cval(cntp_cval_lo, cntp_cval_hi) \
do {    \
    __asm__ __volatile__(   \
    "MRRC p15, 2, %0, %1, c14\n"    \
 : "=r"(cntp_cval_lo), "=r"(cntp_cval_hi) \
    :   \
 : "memory"); \
} while (0)

#define write_cntp_cval(cntp_cval_lo, cntp_cval_hi) \
do {    \
    __asm__ __volatile__(   \
    "MCRR p15, 2, %0, %1, c14\n"    \
    :   \
 : "r"(cntp_cval_lo), "r"(cntp_cval_hi));    \
} while (0)

#define write_cntp_ctl(cntp_ctl)  \
do {    \
    __asm__ __volatile__(   \
    "MCR p15, 0, %0, c14, c2, 1\n"  \
    :   \
 : "r"(cntp_ctl)); \
} while (0)

/* ========================================== */
/* PCM code for MCDI (Multi Core Deep Idle)  v 2.6  2012/11/19 */
/* ========================================== */
static u32 spm_pcm_mcdi[] = {

	0x10007c1f, 0x19c0001f, 0x00004900, 0x19c0001f, 0x00004800, 0x1880001f,
	0x100041dc, 0x18c0001f, 0x10004044, 0x1a10001f, 0x100041dc, 0x1a50001f,
	0x10004044, 0xba008008, 0xff00ffff, 0x00670000, 0xba408009, 0x00ffffff,
	0x9f000000, 0xe0800008, 0xe0c00009, 0xa1d80407, 0x1b00001f, 0xbffff7ff,
	0xf0000000, 0x17c07c1f, 0x1b00001f, 0x3fffe7ff, 0x1b80001f, 0x20000004,
	0x8090840d, 0xb092044d, 0x02400409, 0xd8000a0c, 0x80c7a401, 0x1b00001f,
	0xbffff7ff, 0xd8000a02, 0x17c07c1f, 0x1240041f, 0x1880001f, 0x100041dc,
	0x18c0001f, 0x10004044, 0x1a10001f, 0x100041dc, 0x1a50001f, 0x10004044,
	0xba008008, 0xff00ffff, 0x000a0000, 0xba408009, 0x00ffffff, 0x81000000,
	0xe0800008, 0xe0c00009, 0x1a50001f, 0x10004044, 0x19c0001f, 0x00014920,
	0x19c0001f, 0x00004922, 0x1b80001f, 0x2000000a, 0x18c0001f, 0x10006320,
	0xe0c0001f, 0x809a840d, 0xd8000982, 0x17c07c1f, 0xe0c0000f, 0x18c0001f,
	0x10006814, 0xe0c00001, 0xd8200a02, 0x17c07c1f, 0xa8000000, 0x00000004,
	0x1b00001f, 0x7fffefff, 0xf0000000, 0x17c07c1f, 0x1a50001f, 0x10006400,
	0x82570409, 0xd8000c29, 0x17c07c1f, 0xd8000b8a, 0x17c07c1f, 0xe2e00036,
	0xe2e0003e, 0xe2e0002e, 0xd8200c4a, 0x17c07c1f, 0xe2e0006e, 0xe2e0004e,
	0xe2e0004c, 0xe2e0004d, 0xf0000000, 0x17c07c1f, 0x1a50001f, 0x10006400,
	0x82570409, 0xd8000e49, 0x17c07c1f, 0xd8000daa, 0x17c07c1f, 0xe2e0006d,
	0xe2e0002d, 0xd8200e4a, 0x17c07c1f, 0xe2e0002f, 0xe2e0003e, 0xe2e00032,
	0xd8200ea9, 0x17c07c1f, 0xe2e0005d, 0xf0000000, 0x17c07c1f, 0x1a00001f,
	0x00000001, 0x1a40001f, 0x00000000, 0xa250a408, 0xe2c00009, 0xd8000f6a,
	0x02a0040a, 0xf0000000, 0x17c07c1f, 0x1a40001f, 0xffffffff, 0x1250a41f,
	0xe2c00009, 0xd800106a, 0x02a0040a, 0xf0000000, 0x17c07c1f, 0x1a10001f,
	0x10006608, 0x8ac00008, 0x0000000f, 0xf0000000, 0x17c07c1f, 0x8a900001,
	0x1000620c, 0x8a100001, 0x10006224, 0xa290a00a, 0x8a100001, 0x10006228,
	0xa291200a, 0x8a100001, 0x1000622c, 0xa291a00a, 0x124e341f, 0xb28024aa,
	0x1a10001f, 0x10006400, 0x8206a001, 0x6a60000a, 0x0000000f, 0x82e02009,
	0x82c0040b, 0xf0000000, 0x17c07c1f, 0x1880001f, 0x100041dc, 0x18c0001f,
	0x10004044, 0x1a10001f, 0x100041dc, 0x1a50001f, 0x10004044, 0xd80016ab,
	0x17c07c1f, 0xba008008, 0xff00ffff, 0x000a0000, 0xba408009, 0x00ffffff,
	0x81000000, 0xd82017ab, 0x17c07c1f, 0xba008008, 0xff00ffff, 0x00670000,
	0xba408009, 0x00ffffff, 0x9f000000, 0xe0800008, 0xe0c00009, 0xf0000000,
	0x17c07c1f, 0x820cb401, 0xd82018a8, 0x17c07c1f, 0xa1d78407, 0xf0000000,
	0x17c07c1f, 0x1a00001f, 0x10006604, 0xd80019cb, 0xe2200004, 0x1b80001f,
	0x20000020, 0xd8201a2b, 0xe2200000, 0x1b80001f, 0x20000020, 0xf0000000,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001,
	0x11407c1f, 0xc1001120, 0x11007c1f, 0x109d101f, 0xa8800002, 0x01600001,
	0x1300081f, 0x1b80001f, 0xd0010000, 0x61a07c05, 0x88900006, 0x10006814,
	0xd8003f62, 0x808ab001, 0xc8801822, 0x17c07c1f, 0x8098840d, 0x810e3404,
	0xd8002a42, 0x80e01404, 0x81000403, 0xd8202344, 0x17c07c1f, 0xa1400405,
	0x81108403, 0xd8202584, 0x17c07c1f, 0x1900001f, 0x10006218, 0xc1000c80,
	0x12807c1f, 0x1900001f, 0x10006264, 0x1a80001f, 0x00000010, 0xc1000ee0,
	0x17c07c1f, 0x1900001f, 0x10006218, 0xc1000c80, 0x1280041f, 0xa1508405,
	0x81110403, 0xd82027c4, 0x17c07c1f, 0x1900001f, 0x1000621c, 0xc1000c80,
	0x12807c1f, 0x1900001f, 0x1000626c, 0x1a80001f, 0x00000010, 0xc1000ee0,
	0x17c07c1f, 0x1900001f, 0x1000621c, 0xc1000c80, 0x1280041f, 0xa1510405,
	0x81118403, 0xd8202a04, 0x17c07c1f, 0x1900001f, 0x10006220, 0xc1000c80,
	0x12807c1f, 0x1900001f, 0x10006274, 0x1a80001f, 0x00000010, 0xc1000ee0,
	0x17c07c1f, 0x1900001f, 0x10006220, 0xc1000c80, 0x1280041f, 0xa1518405,
	0xd82037e2, 0x17c07c1f, 0xc1001120, 0x11007c1f, 0x80c01404, 0x82000c01,
	0x818d3001, 0xb18030c1, 0xb18b30c1, 0xb18c30c1, 0xd8002c28, 0x17c07c1f,
	0x82001006, 0xd8002d08, 0x17c07c1f, 0xd0002dc0, 0x17c07c1f, 0xd8202dc6,
	0x17c07c1f, 0x89400005, 0xfffffffe, 0xe8208000, 0x10006f00, 0x00000000,
	0xa1d20407, 0xa1d18407, 0x89c00007, 0xfffffff7, 0x89c00007, 0xfffffbcf,
	0x81008c01, 0xd8203124, 0x810db001, 0xb1003081, 0xd8203124, 0x17c07c1f,
	0x1900001f, 0x10006218, 0xc1000a40, 0x12807c1f, 0x1900001f, 0x10006264,
	0x1a80001f, 0x00000010, 0xc1001020, 0x17c07c1f, 0x1b80001f, 0x20000080,
	0x1900001f, 0x10006218, 0xc1000a40, 0x1280041f, 0x89400005, 0xfffffffd,
	0xe8208000, 0x10006f04, 0x00000000, 0x81010c01, 0xd8203484, 0x810e3001,
	0xb1003081, 0xd8203484, 0x17c07c1f, 0x1900001f, 0x1000621c, 0xc1000a40,
	0x12807c1f, 0x1900001f, 0x1000626c, 0x1a80001f, 0x00000010, 0xc1001020,
	0x17c07c1f, 0x1b80001f, 0x20000080, 0x1900001f, 0x1000621c, 0xc1000a40,
	0x1280041f, 0x89400005, 0xfffffffb, 0xe8208000, 0x10006f08, 0x00000000,
	0x81018c01, 0xd82037e4, 0x810eb001, 0xb1003081, 0xd82037e4, 0x17c07c1f,
	0x1900001f, 0x10006220, 0xc1000a40, 0x12807c1f, 0x1900001f, 0x10006274,
	0x1a80001f, 0x00000010, 0xc1001020, 0x17c07c1f, 0x1b80001f, 0x20000080,
	0x1900001f, 0x10006220, 0xc1000a40, 0x1280041f, 0x89400005, 0xfffffff7,
	0xe8208000, 0x10006f0c, 0x00000000, 0xc08011e0, 0x10807c1f, 0xd8202062,
	0x17c07c1f, 0x1b00001f, 0x7fffefff, 0x1b80001f, 0xd0010000, 0x8098840d,
	0x80d0840d, 0xb0d2046d, 0xa0800c02, 0xd8203a62, 0x17c07c1f, 0x8880000c,
	0x3d200001, 0xd8002062, 0x17c07c1f, 0xd0003860, 0x17c07c1f, 0xe8208000,
	0x10006310, 0x0b1600f8, 0xc10014a0, 0x11007c1f, 0x19c0001f, 0x00014b20,
	0x19c0001f, 0x00004b22, 0x18c0001f, 0x10006320, 0xe0c0001f, 0x1b80001f,
	0x20000014, 0x809a840d, 0xd8003d22, 0x17c07c1f, 0xe0c0000f, 0x18c0001f,
	0x10006814, 0xe0c00001, 0xd8202062, 0xa8000000, 0x00000004, 0x1b00001f,
	0x7fffefff, 0x1b80001f, 0x90100000, 0x80810001, 0xd8202062, 0x17c07c1f,
	0x10007c1f, 0x19c0001f, 0x00004900, 0x19c0001f, 0x00004800, 0xc10014a0,
	0x1100041f, 0xa1d80407, 0xd0002060, 0x19c0001f, 0x00015800, 0x10007c1f,
	0xf0000000
};

#define PCM_MCDI_BASE            __pa(spm_pcm_mcdi)
#define PCM_MCDI_LEN              (511 - 1)
#define MCDI_pcm_pc_0      0
#define MCDI_pcm_pc_1      26
#define MCDI_pcm_pc_2      MCDI_pcm_pc_0
#define MCDI_pcm_pc_3      MCDI_pcm_pc_1

/* extern void mtk_wdt_suspend(void); */
/* extern void mtk_wdt_resume(void); */
extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);
extern int mt_SPI_mask_all(struct mtk_irq_mask *mask);
extern int mt_SPI_mask_restore(struct mtk_irq_mask *mask);
extern void mt_irq_mask_for_sleep(unsigned int irq);
extern void mt_irq_unmask_for_sleep(unsigned int irq);
extern void mt_cirq_enable(void);
extern void mt_cirq_disable(void);
extern void mt_cirq_clone_gic(void);
extern void mt_cirq_flush(void);

extern spinlock_t spm_lock;

static struct mtk_irq_mask MCDI_cpu_irq_mask;

static void spm_mcdi_dump_regs(void)
{
#if 0
	/* PLL register */
	clc_notice("AP_PLL_CON0     0x%x = 0x%x\n", AP_PLL_CON0, spm_read(AP_PLL_CON0));
	clc_notice("AP_PLL_CON1     0x%x = 0x%x\n", AP_PLL_CON1, spm_read(AP_PLL_CON1));
	clc_notice("AP_PLL_CON2     0x%x = 0x%x\n", AP_PLL_CON2, spm_read(AP_PLL_CON2));
	clc_notice("MMPLL_CON0      0x%x = 0x%x\n", MMPLL_CON0, spm_read(MMPLL_CON0));
	clc_notice("ISPPLL_CON0     0x%x = 0x%x\n", ISPPLL_CON0, spm_read(ISPPLL_CON0));
	clc_notice("MSDCPLL_CON0    0x%x = 0x%x\n", MSDCPLL_CON0, spm_read(MSDCPLL_CON0));
	clc_notice("MSDCPLL_PWR_CON 0x%x = 0x%x\n", MSDCPLL_PWR_CON0, spm_read(MSDCPLL_PWR_CON0));
	clc_notice("TVDPLL_CON0     0x%x = 0x%x\n", TVDPLL_CON0, spm_read(TVDPLL_CON0));
	clc_notice("TVDPLL_PWR_CON  0x%x = 0x%x\n", TVDPLL_PWR_CON0, spm_read(TVDPLL_PWR_CON0));
	clc_notice("LVDSPLL_CON0    0x%x = 0x%x\n", LVDSPLL_CON0, spm_read(LVDSPLL_CON0));
	clc_notice("LVDSPLL_PWR_CON 0x%x = 0x%x\n", LVDSPLL_PWR_CON0, spm_read(LVDSPLL_PWR_CON0));

	/* INFRA/PERICFG register */
	clc_notice("INFRA_PDN_STA   0x%x = 0x%x\n", INFRA_PDN_STA, spm_read(INFRA_PDN_STA));
	clc_notice("PERI_PDN0_STA   0x%x = 0x%x\n", PERI_PDN0_STA, spm_read(PERI_PDN0_STA));
	clc_notice("PERI_PDN1_STA   0x%x = 0x%x\n", PERI_PDN1_STA, spm_read(PERI_PDN1_STA));
#endif

	/* SPM register */
	clc_notice("POWER_ON_VAL0   0x%x = 0x%x\n", SPM_POWER_ON_VAL0, spm_read(SPM_POWER_ON_VAL0));
	clc_notice("POWER_ON_VAL1   0x%x = 0x%x\n", SPM_POWER_ON_VAL1, spm_read(SPM_POWER_ON_VAL1));
	clc_notice("PCM_PWR_IO_EN   0x%x = 0x%x\n", SPM_PCM_PWR_IO_EN, spm_read(SPM_PCM_PWR_IO_EN));
	clc_notice("PCM_REG0_DATA   0x%x = 0x%x\n", SPM_PCM_REG0_DATA, spm_read(SPM_PCM_REG0_DATA));
	clc_notice("PCM_REG7_DATA   0x%x = 0x%x\n", SPM_PCM_REG7_DATA, spm_read(SPM_PCM_REG7_DATA));
	clc_notice("PCM_REG13_DATA  0x%x = 0x%x\n", SPM_PCM_REG13_DATA,
		   spm_read(SPM_PCM_REG13_DATA));
	clc_notice("CLK_CON         0x%x = 0x%x\n", SPM_CLK_CON, spm_read(SPM_CLK_CON));
	clc_notice("AP_DVFS_CON     0x%x = 0x%x\n", SPM_AP_DVFS_CON_SET,
		   spm_read(SPM_AP_DVFS_CON_SET));
	clc_notice("PWR_STATUS      0x%x = 0x%x\n", SPM_PWR_STATUS, spm_read(SPM_PWR_STATUS));
	clc_notice("PWR_STATUS_S    0x%x = 0x%x\n", SPM_PWR_STATUS_S, spm_read(SPM_PWR_STATUS_S));
	clc_notice("SLEEP_TIMER_STA 0x%x = 0x%x\n", SPM_SLEEP_TIMER_STA,
		   spm_read(SPM_SLEEP_TIMER_STA));
	clc_notice("WAKE_EVENT_MASK 0x%x = 0x%x\n", SPM_SLEEP_WAKEUP_EVENT_MASK,
		   spm_read(SPM_SLEEP_WAKEUP_EVENT_MASK));

	/* PCM register */
	clc_notice("PCM_REG0_DATA   0x%x = 0x%x\n", SPM_PCM_REG0_DATA, spm_read(SPM_PCM_REG0_DATA));
	clc_notice("PCM_REG1_DATA   0x%x = 0x%x\n", SPM_PCM_REG1_DATA, spm_read(SPM_PCM_REG1_DATA));
	clc_notice("PCM_REG2_DATA   0x%x = 0x%x\n", SPM_PCM_REG2_DATA, spm_read(SPM_PCM_REG2_DATA));
	clc_notice("PCM_REG3_DATA   0x%x = 0x%x\n", SPM_PCM_REG3_DATA, spm_read(SPM_PCM_REG3_DATA));
	clc_notice("PCM_REG4_DATA   0x%x = 0x%x\n", SPM_PCM_REG4_DATA, spm_read(SPM_PCM_REG4_DATA));
	clc_notice("PCM_REG5_DATA   0x%x = 0x%x\n", SPM_PCM_REG5_DATA, spm_read(SPM_PCM_REG5_DATA));
	clc_notice("PCM_REG6_DATA   0x%x = 0x%x\n", SPM_PCM_REG6_DATA, spm_read(SPM_PCM_REG6_DATA));
	clc_notice("PCM_REG7_DATA   0x%x = 0x%x\n", SPM_PCM_REG7_DATA, spm_read(SPM_PCM_REG7_DATA));
	clc_notice("PCM_REG8_DATA   0x%x = 0x%x\n", SPM_PCM_REG8_DATA, spm_read(SPM_PCM_REG8_DATA));
	clc_notice("PCM_REG9_DATA   0x%x = 0x%x\n", SPM_PCM_REG9_DATA, spm_read(SPM_PCM_REG9_DATA));
	clc_notice("PCM_REG10_DATA   0x%x = 0x%x\n", SPM_PCM_REG10_DATA,
		   spm_read(SPM_PCM_REG10_DATA));
	clc_notice("PCM_REG11_DATA   0x%x = 0x%x\n", SPM_PCM_REG11_DATA,
		   spm_read(SPM_PCM_REG11_DATA));
	clc_notice("PCM_REG12_DATA   0x%x = 0x%x\n", SPM_PCM_REG12_DATA,
		   spm_read(SPM_PCM_REG12_DATA));
	clc_notice("PCM_REG13_DATA   0x%x = 0x%x\n", SPM_PCM_REG13_DATA,
		   spm_read(SPM_PCM_REG13_DATA));
	clc_notice("PCM_REG14_DATA   0x%x = 0x%x\n", SPM_PCM_REG14_DATA,
		   spm_read(SPM_PCM_REG14_DATA));
	clc_notice("PCM_REG15_DATA   0x%x = 0x%x\n", SPM_PCM_REG15_DATA,
		   spm_read(SPM_PCM_REG15_DATA));
}

static void SPM_SW_Reset(void)
{
	/* Enable register access key */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	/* re-init power control register (select PCM clock to qaxi) */
	spm_write(SPM_POWER_ON_VAL0, 0);
	spm_write(SPM_POWER_ON_VAL1, 0x00015800);
	spm_write(SPM_PCM_PWR_IO_EN, 0);

	/* Software reset */
	spm_write(SPM_PCM_CON0, (CON0_CFG_KEY | CON0_PCM_SW_RESET | CON0_IM_SLEEP_DVS));
	spm_write(SPM_PCM_CON0, (CON0_CFG_KEY | CON0_IM_SLEEP_DVS));
}

static void SET_PCM_EVENT_VEC(u32 n, u32 event, u32 resume, u32 imme, u32 pc)
{
	switch (n) {
	case 0:
		spm_write(SPM_PCM_EVENT_VECTOR0, EVENT_VEC(event, resume, imme, pc));
		break;
	case 1:
		spm_write(SPM_PCM_EVENT_VECTOR1, EVENT_VEC(event, resume, imme, pc));
		break;
	case 2:
		spm_write(SPM_PCM_EVENT_VECTOR2, EVENT_VEC(event, resume, imme, pc));
		break;
	case 3:
		spm_write(SPM_PCM_EVENT_VECTOR3, EVENT_VEC(event, resume, imme, pc));
		break;
	default:
		break;
	}
}

static void PCM_Init(void)
{
	/* Init PCM r0 */
	spm_write(SPM_PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL0));
	spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R0);
	spm_write(SPM_PCM_PWR_IO_EN, 0);

	/* Init PCM r7 */
	spm_write(SPM_PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL1));
	spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R7);
	spm_write(SPM_PCM_PWR_IO_EN, 0);

	/* set SPM_PCM_REG_DATA_INI */
	spm_write(SPM_PCM_REG_DATA_INI, 0);

	/* set AP STANBY CONTROL */
	spm_write(SPM_AP_STANBY_CON, ((0x0 << WFI_OP) | (0x1 << WFI_L2C) | (0x1 << WFI_SCU)));	/* operand or, mask l2c, mask scu */
	spm_write(SPM_CORE0_CON, 0x0);	/* core_0 wfi_sel */
	spm_write(SPM_CORE1_CON, 0x0);	/* core_1 wfi_sel */
	spm_write(SPM_CORE2_CON, 0x0);	/* core_2 wfi_sel */
	spm_write(SPM_CORE3_CON, 0x0);	/* core_3 wfi_sel */

	/* SET_PCM_MASK_MASTER_REQ */
	spm_write(SPM_PCM_MAS_PAUSE_MASK, 0xFFFFFFFF);	/* bus protect mask */

	/* clean ISR status */
	spm_write(SPM_SLEEP_ISR_MASK, 0x0008);
	spm_write(SPM_SLEEP_ISR_STATUS, 0x0018);

	/* SET_PCM_WAKE_SOURCE */
#if SPM_MCDI_BYPASS_SYSPWREQ
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, (~(0x3c600001)));	/* PCM_TIMER/Thermal/CIRQ/CSYSBDG/CPU[n]123/csyspwreq_b */
#else
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, (~(0x3d600001)));	/* PCM_TIMER/Thermal/CIRQ/CSYSBDG/CPU[n]123 */
#endif

	/* unmask SPM ISR ( 1 : Mask and will not issue the ISR ) */
	spm_write(SPM_SLEEP_ISR_MASK, 0);
}

static void KICK_IM_PCM(u32 pcm_sa, u32 len)
{
	u32 clc_temp;

	spm_write(SPM_PCM_IM_PTR, pcm_sa);
	spm_write(SPM_PCM_IM_LEN, len);

	/* set non-replace mde */
	spm_write(SPM_PCM_CON1, (CON1_CFG_KEY | CON1_MIF_APBEN | CON1_IM_NONRP_EN));

	/* sync register and enable IO output for regiser 0 and 7 */
	spm_write(SPM_PCM_PWR_IO_EN, (PCM_PWRIO_EN_R7 | PCM_PWRIO_EN_R0));

	/* Kick PCM & IM */
	clc_temp = spm_read(SPM_PCM_CON0);
	spm_write(SPM_PCM_CON0, (clc_temp | CON0_CFG_KEY | CON0_PCM_KICK | CON0_IM_KICK));
	spm_write(SPM_PCM_CON0, (clc_temp | CON0_CFG_KEY));
}

static void spm_go_to_MCDI(void)
{
	unsigned long flags;
	spin_lock_irqsave(&spm_lock, flags);

	/* check dram controller setting ============================== */
	spm_check_dramc_for_pcm();

	/* mask SPM IRQ ======================================= */
	mt_irq_mask_for_sleep(MT_SPM_IRQ_ID);	/* mask spm */

	/* SPM SW reset ======================================== */
	SPM_SW_Reset();

	/* Set PCM VSR ========================================= */
	SET_PCM_EVENT_VEC(0, 11, 1, 0, MCDI_pcm_pc_0);	/* MD wake */
	SET_PCM_EVENT_VEC(1, 12, 1, 0, MCDI_pcm_pc_1);	/* MD sleep */
	SET_PCM_EVENT_VEC(2, 30, 1, 0, MCDI_pcm_pc_2);	/* MM wake */
	SET_PCM_EVENT_VEC(3, 31, 1, 0, MCDI_pcm_pc_3);	/* MM sleep */

	/* init PCM ============================================ */
	PCM_Init();

	/* print spm debug log ===================================== */
	spm_mcdi_dump_regs();

	/* Kick PCM and IM ======================================= */
	BUG_ON(PCM_MCDI_BASE & 0x00000003);	/* check 4-byte alignment */
	KICK_IM_PCM(PCM_MCDI_BASE, PCM_MCDI_LEN);
	clc_notice("Kick PCM and IM OK.\r\n");

	spin_unlock_irqrestore(&spm_lock, flags);
}

static u32 spm_leave_MCDI(void)
{
	u32 spm_counter;

	/* Mask ARM i bit */
	asm volatile ("cpsid i @ arch_local_irq_disable" :  :  : "memory", "cc");

	/* trigger cpu wake up event */
	spm_write(SPM_SLEEP_CPU_WAKEUP_EVENT, 0x1);

	/* polling SPM_SLEEP_ISR_STATUS =========================== */
	spm_counter = 0;

	while (((spm_read(SPM_SLEEP_ISR_STATUS) >> 3) & 0x1) == 0x0) {
		if (spm_counter >= 10000) {
			/* set cpu wake up event = 0 */
			spm_write(SPM_SLEEP_CPU_WAKEUP_EVENT, 0x0);

			clc_notice("spm_leave_MCDI : failed.\r\n");

			/* Un-Mask ARM i bit */
			asm volatile ("cpsie i @ arch_local_irq_enable" :  :  : "memory", "cc");
			return 1;
		}
		spm_counter++;
	}

	/* set cpu wake up event = 0 */
	spm_write(SPM_SLEEP_CPU_WAKEUP_EVENT, 0x0);

	/* clean SPM_SLEEP_ISR_STATUS ============================ */
	spm_write(SPM_SLEEP_ISR_MASK, 0x0008);
	spm_write(SPM_SLEEP_ISR_STATUS, 0x0018);

	/* disable IO output for regiser 0 and 7 */
	spm_write(SPM_PCM_PWR_IO_EN, 0x0);

	/* print spm debug log =================================== */
	spm_mcdi_dump_regs();

	/* clean wakeup event raw status */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, 0xffffffff);

	/* check dram controller setting ============================== */
	spm_check_dramc_for_pcm();

	clc_notice("spm_leave_MCDI : OK.\r\n");

	/* Un-Mask ARM i bit */
	asm volatile ("cpsie i @ arch_local_irq_enable" :  :  : "memory", "cc");
	return 0;
}


static void spm_core_wfi_sel(u32 spm_core_no, u32 spm_core_sel)
{
	unsigned long flags;

	if (En_SPM_MCDI == 0) {
		spm_write(SPM_CORE0_CON, 0x0);
		spm_write(SPM_CORE1_CON, 0x0);
		spm_write(SPM_CORE2_CON, 0x0);
		spm_write(SPM_CORE3_CON, 0x0);

		return;
	}

	if (spm_core_no >= NR_CPUS) {
		return;
	}

	switch (spm_core_no) {
	case 0:
		spm_write(SPM_CORE0_CON, spm_core_sel);
		break;
	case 1:
		spm_write(SPM_CORE1_CON, spm_core_sel);
		break;
	case 2:
		spm_write(SPM_CORE2_CON, spm_core_sel);
		break;
	case 3:
		spm_write(SPM_CORE3_CON, spm_core_sel);
		break;
	default:
		break;
	}
}

void spm_clean_ISR_status(void)
{
	/* clean wakeup event raw status */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, 0xffffffff);

	/* clean ISR status */
	spm_write(SPM_SLEEP_ISR_MASK, 0x0008);
	spm_write(SPM_SLEEP_ISR_STATUS, 0x0018);
}


void spm_mcdi_wfi(void)
{
	volatile u32 core_id;
	/* u32 temp_address; */

	preempt_disable();

	core_id = (u32) smp_processor_id();

	/* Mask ARM i bit */
	asm volatile ("cpsid i @ arch_local_irq_disable" :  :  : "memory", "cc");

	if (core_id == 0) {
		/* enable & init CIRQ */
		mt_cirq_clone_gic();
		mt_cirq_enable();

		/* CPU0 enable CIRQ for SPM, Mask all IRQ keep only SPM IRQ ========== */
		mt_SPI_mask_all(&MCDI_cpu_irq_mask);	/* mask core_0 SPI */
		mt_irq_unmask_for_sleep(MT_SPM_IRQ_ID);	/* unmask spm */

		spm_core_wfi_sel(core_id, 0x1);

		/* clc_notice("core_%d enter wfi.\n", core_id); */

		mcdi_wfi_with_sync();	/* enter wfi */

		/* clc_notice("core_%d exit wfi.\n", core_id); */

		/* restore irq mask */
		mt_SPI_mask_restore(&MCDI_cpu_irq_mask);

		/* flush & disable CIRQ */
		/* mt_cirq_flush(); */
		mt_cirq_disable();
	} else			/* Core 1,2,3  Keep original IRQ */
	{
		spm_core_wfi_sel(core_id, 0x1);

		/* clc_notice("core_%d enter wfi.\n", core_id); */

		if (En_SPM_MCDI == 0) {
			mcdi_wfi_with_sync();
		} else {
			if (!cpu_power_down(DORMANT_MODE)) {
				/* do not add code here */
				mcdi_wfi_with_sync();
			}
			cpu_check_dormant_abort();
		}

		/* clc_notice("core_%d exit wfi.\n", core_id); */
	}

	/* Un-Mask ARM i bit */
	asm volatile ("cpsie i @ arch_local_irq_enable" :  :  : "memory", "cc");

	preempt_enable();
}

int spm_wfi_for_sodi_test(void *sodi_data)
{
	volatile u32 do_not_change_it = 1;

	while (do_not_change_it) {
		volatile u32 core_id;
		/* u32 temp_address; */

		preempt_disable();

		core_id = (u32) smp_processor_id();

		/* Mask ARM i bit */
		asm volatile ("cpsid i @ arch_local_irq_disable" :  :  : "memory", "cc");

		if (core_id == 0) {
			/* enable & init CIRQ */
			mt_cirq_clone_gic();
			mt_cirq_enable();

			/* CPU0 enable CIRQ for SPM, Mask all IRQ keep only SPM IRQ ========== */
			mt_SPI_mask_all(&MCDI_cpu_irq_mask);	/* mask core_0 SPI */
			mt_irq_unmask_for_sleep(MT_SPM_IRQ_ID);	/* unmask spm */

			spm_core_wfi_sel(core_id, 0x1);

			clc_notice("core_%d enter wfi.\n", core_id);

			mcdi_wfi_with_sync();	/* enter wfi */

			clc_notice("core_%d exit wfi.\n", core_id);

			/* restore irq mask */
			mt_SPI_mask_restore(&MCDI_cpu_irq_mask);

			/* flush & disable CIRQ */
			/* mt_cirq_flush(); */
			mt_cirq_disable();

			/* delay 10 ms */
			mdelay(10);
		} else		/* Core 1,2,3  Keep original IRQ */
		{
			spm_core_wfi_sel(core_id, 0x1);

			clc_notice("core_%d enter wfi.\n", core_id);

			if (En_SPM_MCDI == 0) {
				mcdi_wfi_with_sync();
			} else {
				if (!cpu_power_down(DORMANT_MODE)) {
					/* do not add code here */
					mcdi_wfi_with_sync();
				}
				cpu_check_dormant_abort();
			}

			clc_notice("core_%d exit wfi.\n", core_id);

			/* delay 10 ms */
			mdelay(10);
		}

		/* Un-Mask ARM i bit */
		asm volatile ("cpsie i @ arch_local_irq_enable" :  :  : "memory", "cc");

		preempt_enable();
	}

	return 0;
}

int spm_wfi_for_mcdi_test(void *mcdi_data)
{
	volatile u32 do_not_change_it = 1;

	while (do_not_change_it) {
		volatile u32 lo, hi, core_id;
		/* u32 temp_address; */

		preempt_disable();

		core_id = (u32) smp_processor_id();

#if 0				/* set local timer ============================================= */
		read_cntp_cval(lo, hi);

		switch (core_id) {
		case 0:
			lo += 5070000;	/* (1*13*1000*1000);  390 ms */
			break;

		case 1:
			lo += 2470000;	/* (2*13*1000*1000); 190ms */
			break;

		case 2:
			lo += 1170000;	/* (3*13*1000*1000); 90ms */
			break;

		case 3:
			lo += 520000;	/* (4*13*1000*1000); 40ms */
			break;

		default:
			break;
		}

		write_cntp_cval(lo, hi);
		write_cntp_ctl(0x1);	/* CNTP_CTL_ENABLE */
#else				/* set timer ============================================== */
		switch (core_id) {
		case 0:
			read_cntp_cval(lo, hi);
			lo += 5070000;	/* 390 ms, 13MHz */
			write_cntp_cval(lo, hi);
			write_cntp_ctl(0x1);	/* CNTP_CTL_ENABLE */
			break;

		case 1:
			lo += 2470000;	/* 190ms */
			break;

		case 2:
			lo += 1170000;	/* 90ms */
			break;

		case 3:
			lo += 520000;	/* 40ms */
			break;

		default:
			break;
		}
#endif
		/* ======================================================= */

		if (core_id == 0) {
			/* Mask ARM i bit */
			asm volatile ("cpsid i @ arch_local_irq_disable" :  :  : "memory", "cc");

			/* enable & init CIRQ */
			mt_cirq_clone_gic();
			mt_cirq_enable();

			/* CPU0 enable CIRQ for SPM, Mask all IRQ keep only SPM IRQ ========== */
			mt_SPI_mask_all(&MCDI_cpu_irq_mask);	/* mask core_0 SPI */
			mt_irq_unmask_for_sleep(MT_SPM_IRQ_ID);	/* unmask spm */

			spm_core_wfi_sel(core_id, 0x1);

			clc_notice("core_%d enter wfi.\n", core_id);

			mcdi_wfi_with_sync();	/* enter wfi */

			clc_notice("core_%d exit wfi.\n", core_id);

			/* restore irq mask */
			mt_SPI_mask_restore(&MCDI_cpu_irq_mask);

			/* flush & disable CIRQ */
			/* mt_cirq_flush(); */
			mt_cirq_disable();

			/* delay 10 ms */
			mdelay(10);

			/* Un-Mask ARM i bit */
			asm volatile ("cpsie i @ arch_local_irq_enable" :  :  : "memory", "cc");

			/* spm_leave_MCDI(); */
		} else		/* Core 1,2,3  Keep original IRQ */
		{
			/* Mask ARM i bit */
			asm volatile ("cpsid i @ arch_local_irq_disable" :  :  : "memory", "cc");

			spm_core_wfi_sel(core_id, 0x1);

			clc_notice("core_%d enter wfi.\n", core_id);

			if (En_SPM_MCDI == 0) {
				mcdi_wfi_with_sync();
			} else {
				if (!cpu_power_down(DORMANT_MODE)) {
					/* do not add code here */
					mcdi_wfi_with_sync();
				}
				cpu_check_dormant_abort();
			}

			clc_notice("core_%d exit wfi.\n", core_id);

			/* delay 10 ms */
			mdelay(10);

			/* Un-Mask ARM i bit */
			asm volatile ("cpsie i @ arch_local_irq_enable" :  :  : "memory", "cc");
		}

		preempt_enable();
	}

	return 0;
}


void spm_check_core_status(u32 target_core)
{
	u32 clc_temp_00, target_core_temp, spm_core_wfi, spm_core_pws;
	u32 spm_core_sel_0 = 0, spm_core_sel_1 = 0, spm_core_sel_2 = 0, spm_core_sel_3 = 0;

	if (En_SPM_MCDI == 0) {
		return;
	}

	target_core_temp = target_core & 0xf;

	/* wfi_sel */
	if ((target_core_temp & 0x1) == 0x1) {
		spm_core_sel_0 = (spm_read(SPM_CORE0_CON)) & 0x1;
	}

	if ((target_core_temp & 0x2) == 0x2) {
		spm_core_sel_1 = (spm_read(SPM_CORE1_CON)) & 0x1;
	}

	if ((target_core_temp & 0x4) == 0x4) {
		spm_core_sel_2 = (spm_read(SPM_CORE2_CON)) & 0x1;
	}

	if ((target_core_temp & 0x8) == 0x8) {
		spm_core_sel_3 = (spm_read(SPM_CORE3_CON)) & 0x1;
	}

	if ((spm_core_sel_0 | spm_core_sel_1 | spm_core_sel_2 | spm_core_sel_3) == 0x0) {
		return;
	}
	/* ========================================================= */
	while (1) {
		/* wfi => 1:wfi */
		clc_temp_00 = spm_read(SPM_PCM_REG13_DATA);
		spm_core_wfi = (clc_temp_00 >> 28) & 0xf;

		/* power_state => 1: power down */
		clc_temp_00 = spm_read(SPM_PCM_REG5_DATA);
		spm_core_pws = (clc_temp_00 >> 0) & 0xf;

		if ((target_core_temp == spm_core_wfi) && (target_core_temp == spm_core_pws)) {
			break;
		}
	}

}

void spm_hot_plug_in_before(u32 target_core)
{
	u32 target_core_temp;

	if (En_SPM_MCDI == 0) {
		return;
	}

	target_core_temp = target_core & 0xf;

	if ((target_core_temp & 0x1) == 0x1) {
		spm_write(SPM_MP_CORE0_AUX, 0x0);
	}

	if ((target_core_temp & 0x2) == 0x2) {
		spm_write(SPM_MP_CORE1_AUX, 0x0);
	}

	if ((target_core_temp & 0x4) == 0x4) {
		spm_write(SPM_MP_CORE2_AUX, 0x0);
	}

	if ((target_core_temp & 0x8) == 0x8) {
		spm_write(SPM_MP_CORE3_AUX, 0x0);
	}
}

void spm_hot_plug_out_after(u32 target_core)
{
	u32 target_core_temp;

	if (En_SPM_MCDI == 0) {
		return;
	}

	target_core_temp = target_core & 0xf;

	if ((target_core_temp & 0x1) == 0x1) {
		spm_write(SPM_MP_CORE0_AUX, 0x1);
	}

	if ((target_core_temp & 0x2) == 0x2) {
		spm_write(SPM_MP_CORE1_AUX, 0x1);
	}

	if ((target_core_temp & 0x4) == 0x4) {
		spm_write(SPM_MP_CORE2_AUX, 0x1);
	}

	if ((target_core_temp & 0x8) == 0x8) {
		spm_write(SPM_MP_CORE3_AUX, 0x1);
	}
}

void spm_disable_sodi(void)
{
	u32 clc_temp;

	clc_temp = spm_read(SPM_CLK_CON);
	clc_temp |= (0x1 << 13);

	spm_write(SPM_CLK_CON, clc_temp);
}

void spm_enable_sodi(void)
{
	u32 clc_temp;

	clc_temp = spm_read(SPM_CLK_CON);
	clc_temp &= 0xffffdfff;	/* ~(0x1<<13); */

	spm_write(SPM_CLK_CON, clc_temp);
}

/* ============================================================================== */

static int spm_mcdi_probe(struct platform_device *pdev)
{
	clc_notice("spm_mcdi_probe start.\n");
	spm_go_to_MCDI();
	return 0;
}

static void spm_mcdi_early_suspend(struct early_suspend *h)
{
	clc_notice("spm_mcdi_early_suspend start.\n");
	spm_leave_MCDI();
}

static void spm_mcdi_late_resume(struct early_suspend *h)
{
	clc_notice("spm_mcdi_late_resume start.\n");
	spm_go_to_MCDI();
}

static struct platform_driver mtk_spm_mcdi_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = spm_mcdi_probe,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		   .name = "mtk-spm-mcdi",
		   },
};

static struct early_suspend mtk_spm_mcdi_early_suspend_driver = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 251,
	.suspend = spm_mcdi_early_suspend,
	.resume = spm_mcdi_late_resume,
};

void spm_mcdi_LDVT_sodi(void)
{
	clc_notice("spm_mcdi_LDVT_sodi() start.\n");
	int res = 0;
	struct wd_api *wd_api = NULL;
	res = get_wd_api(&wd_api);

	if (res) {
		pr_info("spm_mcdi_LDVT_sodi, get wd api error\n");
	} else {
		wd_api->wd_suspend_notify();
	}
	/* mtk_wdt_suspend(); */

	/* show image on screen ============================ */

	/* spm_enable_sodi ============================ */
	spm_enable_sodi();

	mcdi_task_0 = kthread_create(spm_wfi_for_sodi_test, NULL, "mcdi_task_0");
	mcdi_task_1 = kthread_create(spm_wfi_for_sodi_test, NULL, "mcdi_task_1");
	mcdi_task_2 = kthread_create(spm_wfi_for_sodi_test, NULL, "mcdi_task_2");
	mcdi_task_3 = kthread_create(spm_wfi_for_sodi_test, NULL, "mcdi_task_3");

	if (IS_ERR(mcdi_task_0) || IS_ERR(mcdi_task_1) || IS_ERR(mcdi_task_2)
	    || IS_ERR(mcdi_task_3)) {
		clc_notice("Unable to start kernel thread(0x%x, 0x%x, 0x%x, 0x%x)./n",
			   IS_ERR(mcdi_task_0), IS_ERR(mcdi_task_1), IS_ERR(mcdi_task_2),
			   IS_ERR(mcdi_task_3));
		mcdi_task_0 = NULL;
		mcdi_task_1 = NULL;
		mcdi_task_2 = NULL;
		mcdi_task_3 = NULL;
	}

	kthread_bind(mcdi_task_0, 0);
	kthread_bind(mcdi_task_1, 1);
	kthread_bind(mcdi_task_2, 2);
	kthread_bind(mcdi_task_3, 3);

	wake_up_process(mcdi_task_0);
	wake_up_process(mcdi_task_1);
	wake_up_process(mcdi_task_2);
	wake_up_process(mcdi_task_3);

	clc_notice("spm_mcdi_LDVT_01() end.\n");
}

void spm_mcdi_LDVT_mcdi(void)
{
	clc_notice("spm_mcdi_LDVT_mcdi() start.\n");
	/* mtk_wdt_suspend(); */
	int res = 0;
	struct wd_api *wd_api = NULL;
	res = get_wd_api(&wd_api);

	if (res) {
		pr_info("spm_mcdi_LDVT_sodi, get wd api error\n");
	} else {
		wd_api->wd_suspend_notify();
	}

	/* spm_disable_sodi ============================ */
	spm_disable_sodi();

#if 0
	smp_call_function(spm_mcdi_wfi_for_test, NULL, 0);
	spm_mcdi_wfi_for_test();
#else
	mcdi_task_0 = kthread_create(spm_wfi_for_mcdi_test, NULL, "mcdi_task_0");
	mcdi_task_1 = kthread_create(spm_wfi_for_mcdi_test, NULL, "mcdi_task_1");
	mcdi_task_2 = kthread_create(spm_wfi_for_mcdi_test, NULL, "mcdi_task_2");
	mcdi_task_3 = kthread_create(spm_wfi_for_mcdi_test, NULL, "mcdi_task_3");

	if (IS_ERR(mcdi_task_0) || IS_ERR(mcdi_task_1) || IS_ERR(mcdi_task_2)
	    || IS_ERR(mcdi_task_3)) {
		clc_notice("Unable to start kernel thread(0x%x, 0x%x, 0x%x, 0x%x)./n",
			   IS_ERR(mcdi_task_0), IS_ERR(mcdi_task_1), IS_ERR(mcdi_task_2),
			   IS_ERR(mcdi_task_3));
		mcdi_task_0 = NULL;
		mcdi_task_1 = NULL;
		mcdi_task_2 = NULL;
		mcdi_task_3 = NULL;
	}

	kthread_bind(mcdi_task_0, 0);
	kthread_bind(mcdi_task_1, 1);
	kthread_bind(mcdi_task_2, 2);
	kthread_bind(mcdi_task_3, 3);

	wake_up_process(mcdi_task_0);
	wake_up_process(mcdi_task_1);
	wake_up_process(mcdi_task_2);
	wake_up_process(mcdi_task_3);
#endif
}

/***************************
* show current SPM-MCDI stauts
****************************/
static int spm_mcdi_debug_read(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	char *p = buf;

	if (En_SPM_MCDI)
		p += sprintf(p, "SPM MCDI+Thermal Protect enabled.\n");
	else
		p += sprintf(p, "SPM MCDI disabled, Thermal Protect only.\n");

	len = p - buf;
	return len;
}

/************************************
* set SPM-MCDI stauts by sysfs interface
*************************************/
static ssize_t spm_mcdi_debug_write(struct file *file, const char *buffer, unsigned long count,
				    void *data)
{
	int enabled = 0;

	if (sscanf(buffer, "%d", &enabled) == 1) {
		if (enabled == 0) {
			spm_leave_MCDI();
			En_SPM_MCDI = 0;
		} else if (enabled == 1) {
			En_SPM_MCDI = 1;
			spm_go_to_MCDI();
		} else if (enabled == 2) {
			clc_notice("spm_mcdi_LDVT_sodi() (argument_0 = %d)\n", enabled);
			spm_mcdi_LDVT_sodi();
		} else if (enabled == 3) {
			clc_notice("spm_mcdi_LDVT_mcdi() (argument_0 = %d)\n", enabled);
			spm_mcdi_LDVT_mcdi();
		} else {
			clc_notice("bad argument_0!! (argument_0 = %d)\n", enabled);
		}
	} else {
		clc_notice("bad argument_1!!\n");
	}

	return count;
}

static int __init spm_mcdi_init(void)
{
	struct proc_dir_entry *mt_entry = NULL;
	struct proc_dir_entry *mt_mcdi_dir = NULL;
	int mcdi_err = 0;

	mt_mcdi_dir = proc_mkdir("mcdi", NULL);
	if (!mt_mcdi_dir) {
		clc_notice("[%s]: mkdir /proc/mcdi failed\n", __func__);
	} else {
		mt_entry =
		    create_proc_entry("mcdi_debug", S_IRUGO | S_IWUSR | S_IWGRP, mt_mcdi_dir);
		if (mt_entry) {
			mt_entry->read_proc = spm_mcdi_debug_read;
			mt_entry->write_proc = spm_mcdi_debug_write;
		}
	}

	mcdi_err = platform_driver_register(&mtk_spm_mcdi_driver);

	if (mcdi_err) {
		clc_notice("spm mcdi driver callback register failed..\n");
		return mcdi_err;
	}

	clc_notice("spm mcdi driver callback register OK..\n");

	register_early_suspend(&mtk_spm_mcdi_early_suspend_driver);

	clc_notice("spm mcdi driver early suspend callback register OK..\n");

	return 0;
}

static void __exit spm_mcdi_exit(void)
{
	clc_notice("Exit SPM-MCDI\n\r");
}
late_initcall(spm_mcdi_init);

MODULE_DESCRIPTION("MT6589 SPM-Idle Driver v0.1");
