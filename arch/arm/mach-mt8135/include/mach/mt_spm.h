#ifndef _MT_SPM_
#define _MT_SPM_

#include <linux/kernel.h>
#include <mach/mt_reg_base.h>
#include <mach/sync_write.h>

/*
 * for SPM register control
 */
#define SPM_POWERON_CONFIG_SET      (SPM_BASE + 0x0000)
#define SPM_POWER_ON_VAL0           (SPM_BASE + 0x0010)
#define SPM_POWER_ON_VAL1           (SPM_BASE + 0x0014)
#define SPM_CLK_SETTLE              (SPM_BASE + 0x0100)
#define SPM_FC0_PWR_CON             (SPM_BASE + 0x0200)
#define SPM_DBG_PWR_CON             (SPM_BASE + 0x0204)
#define SPM_CPU_PWR_CON             (SPM_BASE + 0x0208)
/* #define SPM_MP_CORE0_AUX            (SPM_BASE + 0x020c) */
#define SPM_VDE_PWR_CON             (SPM_BASE + 0x0210)
#define SPM_MFG_PWR_CON             (SPM_BASE + 0x0214)
#define SPM_FC1_PWR_CON             (SPM_BASE + 0x0218)
/* #define SPM_FC2_PWR_CON             (SPM_BASE + 0x021c) */
/* #define SPM_FC3_PWR_CON             (SPM_BASE + 0x0220) */
/* #define SPM_MP_CORE1_AUX            (SPM_BASE + 0x0224) */
/* #define SPM_MP_CORE2_AUX            (SPM_BASE + 0x0228) */
/* #define SPM_MP_CORE3_AUX            (SPM_BASE + 0x022c) */
#define SPM_VEN_PWR_CON             (SPM_BASE + 0x0230)
#define SPM_IFR_PWR_CON             (SPM_BASE + 0x0234)
#define SPM_ISP_PWR_CON             (SPM_BASE + 0x0238)
#define SPM_DIS_PWR_CON             (SPM_BASE + 0x023c)
#define SPM_DPY_PWR_CON             (SPM_BASE + 0x0240)
#define SPM_CPU_L2_DAT_PDN          (SPM_BASE + 0x0244)
#define SPM_CPU_L2_DAT_SLEEP_B      (SPM_BASE + 0x0248)
/* #define SPM_CPU_L2_TAG_PDN          (SPM_BASE + 0x024c) */
/* #define SPM_CPU_L2_TAG_SLEEP_B      (SPM_BASE + 0x0250) */
/* #define SPM_CPU_SCU_PDN             (SPM_BASE + 0x0254) */
/* #define SPM_CPU_SCU_SLEEP_B         (SPM_BASE + 0x0258) */
#define SPM_MP_CORE0_AUX            (SPM_BASE + 0x024c)
#define SPM_MP_CORE1_AUX            (SPM_BASE + 0x0250)
#define SPM_MP_CORE2_AUX            (SPM_BASE + 0x0254)
#define SPM_MP_CORE3_AUX            (SPM_BASE + 0x0258)
#define SPM_CPU_FC0_L1_PDN          (SPM_BASE + 0x025c)
#define SPM_CPU_FC0_L1_SLEEP_B      (SPM_BASE + 0x0260)
#define SPM_CPU_FC1_L1_PDN          (SPM_BASE + 0x0264)
#define SPM_CPU_FC1_L1_SLEEP_B      (SPM_BASE + 0x0268)
#define SPM_CPU_FC2_L1_PDN          (SPM_BASE + 0x026c)
#define SPM_CPU_FC2_L1_SLEEP_B      (SPM_BASE + 0x0270)
#define SPM_CPU_FC3_L1_PDN          (SPM_BASE + 0x0274)
#define SPM_CPU_FC3_L1_SLEEP_B      (SPM_BASE + 0x0278)
#define SPM_IFR_FH_SRAM_CTRL        (SPM_BASE + 0x027c)
/* #define SPM_MD2_PWR_CON             (SPM_BASE + 0x0280) */
#define SPM_MD1_PWR_CON             (SPM_BASE + 0x0284)
#define SPM_GSTA_PWR_CON            (SPM_BASE + 0x0288)
#define SPM_MCU_PWR_CON	        (SPM_BASE + 0x0290)
#define SPM_CA7_CFG_PWR_CON         (SPM_BASE + 0x0294)
#define SPM_MCU_CFG_PWR_CON         (SPM_BASE + 0x0298)
#define SPM_CA15_CX0_PWR_CON        (SPM_BASE + 0x029c)
#define SPM_CA15_CX1_PWR_CON        (SPM_BASE + 0x02a0)
#define SPM_CA15_CPU0_PWR_CON       (SPM_BASE + 0x02a4)
#define SPM_CA15_CPU1_PWR_CON       (SPM_BASE + 0x02a8)
#define SPM_CA15_CPUTOP_PWR_CON     (SPM_BASE + 0x02ac)
#define SPM_CA15_L1_PWR_CON         (SPM_BASE + 0x02b0)
#define SPM_CA15_L2_PWR_CON         (SPM_BASE + 0x02b4)
#define SPM_MFG_2D_PWR_CON          (SPM_BASE + 0x02b8)
#define SPM_PCM_CON0                (SPM_BASE + 0x0310)
#define SPM_PCM_CON1                (SPM_BASE + 0x0314)
#define SPM_PCM_IM_PTR              (SPM_BASE + 0x0318)
#define SPM_PCM_IM_LEN              (SPM_BASE + 0x031c)
#define SPM_PCM_REG_DATA_INI        (SPM_BASE + 0x0320)
#define SPM_PCM_EVENT_VECTOR0       (SPM_BASE + 0x0340)
#define SPM_PCM_EVENT_VECTOR1       (SPM_BASE + 0x0344)
#define SPM_PCM_EVENT_VECTOR2       (SPM_BASE + 0x0348)
#define SPM_PCM_EVENT_VECTOR3       (SPM_BASE + 0x034c)
#define SPM_PCM_MAS_PAUSE_MASK      (SPM_BASE + 0x0354)
#define SPM_PCM_PWR_IO_EN           (SPM_BASE + 0x0358)
#define SPM_PCM_TIMER_VAL           (SPM_BASE + 0x035c)
#define SPM_PCM_TIMER_OUT           (SPM_BASE + 0x0360)
#define SPM_PCM_REG0_DATA           (SPM_BASE + 0x0380)
#define SPM_PCM_REG1_DATA           (SPM_BASE + 0x0384)
#define SPM_PCM_REG2_DATA           (SPM_BASE + 0x0388)
#define SPM_PCM_REG3_DATA           (SPM_BASE + 0x038c)
#define SPM_PCM_REG4_DATA           (SPM_BASE + 0x0390)
#define SPM_PCM_REG5_DATA           (SPM_BASE + 0x0394)
#define SPM_PCM_REG6_DATA           (SPM_BASE + 0x0398)
#define SPM_PCM_REG7_DATA           (SPM_BASE + 0x039c)
#define SPM_PCM_REG8_DATA           (SPM_BASE + 0x03a0)
#define SPM_PCM_REG9_DATA           (SPM_BASE + 0x03a4)
#define SPM_PCM_REG10_DATA          (SPM_BASE + 0x03a8)
#define SPM_PCM_REG11_DATA          (SPM_BASE + 0x03ac)
#define SPM_PCM_REG12_DATA          (SPM_BASE + 0x03b0)
#define SPM_PCM_REG13_DATA          (SPM_BASE + 0x03b4)
#define SPM_PCM_REG14_DATA          (SPM_BASE + 0x03b8)
#define SPM_PCM_REG15_DATA          (SPM_BASE + 0x03bc)
#define SPM_PCM_EVENT_REG_STA       (SPM_BASE + 0x03c0)
#define SPM_PCM_FSM_STA             (SPM_BASE + 0x03c4)
#define SPM_PCM_IM_HOST_RW_PTR      (SPM_BASE + 0x03c8)
#define SPM_PCM_IM_HOST_RW_DAT      (SPM_BASE + 0x03cc)
#define SPM_PCM_EVENT_VECTOR4       (SPM_BASE + 0x03d0)
#define SPM_PCM_EVENT_VECTOR5       (SPM_BASE + 0x03d4)
#define SPM_PCM_EVENT_VECTOR6       (SPM_BASE + 0x03d8)
#define SPM_PCM_EVENT_VECTOR7       (SPM_BASE + 0x03dc)
#define SPM_PCM_SW_INT_SET          (SPM_BASE + 0x03e0)
#define SPM_PCM_SW_INT_CLEAR        (SPM_BASE + 0x03e4)
#define SPM_CLK_CON                 (SPM_BASE + 0x0400)
#define SPM_APMCU_PWRCTL            (SPM_BASE + 0x0600)
#define SPM_AP_DVFS_CON_SET         (SPM_BASE + 0x0604)
#define SPM_AP_STANBY_CON           (SPM_BASE + 0x0608)
#define SPM_PWR_STATUS              (SPM_BASE + 0x060c)
#define SPM_PWR_STATUS_S            (SPM_BASE + 0x0610)
#define SPM_SLEEP_TIMER_STA         (SPM_BASE + 0x0720)
#define SPM_SLEEP_TWAM_CON          (SPM_BASE + 0x0760)
#define SPM_SLEEP_TWAM_STATUS       (SPM_BASE + 0x0764)
#define SPM_SLEEP_WAKEUP_EVENT_MASK (SPM_BASE + 0x0810)
#define SPM_SLEEP_CPU_WAKEUP_EVENT  (SPM_BASE + 0x0814)
#define SPM_PCM_WDT_TIMER_VAL       (SPM_BASE + 0x0824)
#define SPM_SLEEP_ISR_MASK          (SPM_BASE + 0x0900)
#define SPM_SLEEP_ISR_STATUS        (SPM_BASE + 0x0904)
#define SPM_SLEEP_WAKEUP_DBG_EDGE   (SPM_BASE + 0x0908)
#define SPM_ATB_LMU_CON0            (SPM_BASE + 0x090c)
#define SPM_SLEEP_ISR_RAW_STA       (SPM_BASE + 0x0910)
#define SPM_PCM_SRC_REQ             (SPM_BASE + 0x0b04)
#define SPM_SLEEP_CPU_IRQ_MASK      (SPM_BASE + 0x0b10)
#define SPM_CORE0_WFI_SEL               (SPM_BASE + 0x0f00)
#define SPM_CORE1_WFI_SEL               (SPM_BASE + 0x0f04)
#define SPM_CORE2_WFI_SEL               (SPM_BASE + 0x0f08)
#define SPM_CORE3_WFI_SEL               (SPM_BASE + 0x0f0c)

#define SPM_PROJECT_CODE            0xb16

#define CON0_PCM_KICK               (1U << 0)
#define CON0_IM_KICK                (1U << 1)
#define CON0_IM_SLEEP_DVS           (1U << 3)
#define CON0_EVENT_VEC0_EN          (1U << 4)
#define CON0_EVENT_VEC1_EN          (1U << 5)
#define CON0_EVENT_VEC2_EN          (1U << 6)
#define CON0_EVENT_VEC3_EN          (1U << 7)
#define CON0_EVENT_VEC4_EN          (1U << 8)
#define CON0_EVENT_VEC5_EN          (1U << 9)
#define CON0_EVENT_VEC6_EN          (1U << 10)
#define CON0_EVENT_VEC7_EN          (1U << 11)
#define CON0_PCM_SW_RESET           (1U << 15)
#define CON0_CFG_KEY                (SPM_PROJECT_CODE << 16)

#define CON1_IM_SLAVE               (1U << 0)
#define CON1_MIF_APBEN              (1U << 3)
#define CON1_PCM_TIMER_EN           (1U << 5)
#define CON1_IM_NONRP_EN            (1U << 6)
#define CON1_CFG_KEY                (SPM_PROJECT_CODE << 16)

#define PCM_PWRIO_EN_R0             (1U << 0)
#define PCM_PWRIO_EN_R1             (1U << 1)
#define PCM_PWRIO_EN_R2             (1U << 2)
#define PCM_PWRIO_EN_R7             (1U << 7)
#define PCM_RF_SYNC_R0              (1U << 16)
#define PCM_RF_SYNC_R7              (1U << 23)

#define R7_UART_CLK_OFF_REQ         (1U << 0)

#define R13_MD2_SRCCLKENI           (1U << 1)
#define R13_MD1_SRCCLKENI           (1U << 4)
#define R13_UART_CLK_OFF_ACK        (1U << 20)

#define CC_SYSCLK0_EN_0             (1U << 0)
#define CC_SYSCLK0_EN_1             (1U << 1)
#define CC_SYSCLK1_EN_0             (1U << 2)
#define CC_SYSCLK1_EN_1             (1U << 3)
#define CC_SYSSETTLE_SEL            (1U << 4)
#define CC_LOCK_INFRA_DCM           (1U << 5)
#define CC_SRCLKEN_PERI_MASK        (1U << 6)
#define CC_SRCLKEN_MD_MASK          (1U << 7)
#define CC_SRCVOLTEN_MASK           (1U << 8)
#define CC_CXO32K_RM_EN_MD1         (1U << 9)
#define CC_CXO32K_RM_EN_MD2         (1U << 10)
#define CC_CLKSQ0_SEL               (1U << 11)
#define CC_CLKSQ1_SEL               (1U << 12)
#define CC_DISABLE_SODI             (1U << 13)
#define CC_DISABLE_DORM_PWR         (1U << 14)
#define CC_DISABLE_INFRA_PWR        (1U << 15)
#define CC_SRCLKEN0_EN              (1U << 16)
#define CC_SRCLKEN1_EN              (1U << 17)

#define SRC_PCM_APSRC_REQ           (1U << 0)
#define SRC_PCM_F26M_REQ            (1U << 1)

#define WAKE_SRC_TS                 (1U << 1)
#define WAKE_SRC_KP                 (1U << 2)
#define WAKE_SRC_GPT                (1U << 4)
#define WAKE_SRC_EINT               (1U << 5)
/* #define WAKE_SRC_CCIF_MD2           (1U << 7) */
/* #define WAKE_SRC_CCIF_MD1           (1U << 8) */
#define WAKE_SRC_LOW_BAT            (1U << 9)
#define WAKE_SRC_USB0_CD            (1U << 14)
#define WAKE_SRC_USB1_CD            (1U << 15)
#define WAKE_SRC_USB1_PDN           (1U << 16)
#define WAKE_SRC_USB0_PDN           (1U << 17)
#define WAKE_SRC_DBGSYS             (1U << 18)
#define WAKE_SRC_UART0              (1U << 19)
#define WAKE_SRC_AFE                (1U << 20)
#define WAKE_SRC_THERM              (1U << 21)
#define WAKE_SRC_CIRQ               (1U << 22)
#define WAKE_SRC_PWRAP              (1U << 23)
#define WAKE_SRC_SYSPWREQ           (1U << 24)
/* #define WAKE_SRC_MD_WDT             (1U << 25) */
#define WAKE_SRC_THERM2             (1U << 25)
#define WAKE_SRC_CPU0_IRQ           (1U << 26)
#define WAKE_SRC_CPU1_IRQ           (1U << 27)
#define WAKE_SRC_CPU2_IRQ           (1U << 28)
#define WAKE_SRC_CPU3_IRQ           (1U << 29)

#define EVENT_VEC(event, resume, imme, pc)  \
    (((event) << 0) | ((resume) << 5) | ((imme) << 6) | ((pc) << 16))

#define spm_read(addr)              (*(volatile u32 *)(addr))
#define spm_write(addr, val)        mt65xx_reg_sync_writel(val, addr)


#define spm_is_wakesrc_invalid(wakesrc)     (!!((u32)(wakesrc) & 0xc0083dc9))

#define spm_check_dramc_for_pcm()                               \
    BUG_ON((spm_read(0xf00041dc) & 0x00ff0000) != 0x00660000 || \
	   (spm_read(0xf0004044) & 0xff000000) != 0x9f000000)

#define spm_emerg(fmt, args...)     pr_emerg("[SPM] " fmt, ##args)
#define spm_alert(fmt, args...)     pr_alert("[SPM] " fmt, ##args)
#define spm_crit(fmt, args...)      pr_crit("[SPM] " fmt, ##args)
#define spm_error(fmt, args...)     pr_err("[SPM] " fmt, ##args)
#define spm_warning(fmt, args...)   pr_warn("[SPM] " fmt, ##args)
#define spm_notice(fmt, args...)    pr_notice("[SPM] " fmt, ##args)
#define spm_info(fmt, args...)      pr_info("[SPM] " fmt, ##args)
#define spm_debug(fmt, args...)     pr_debug("[SPM] " fmt, ##args)

extern void spm_go_to_normal(void);
extern void spm_module_init(void);
extern void spm_fs_init(void);

extern u32 spm_sleep_wakesrc;
extern bool slp_ck26m_on;
extern bool slp_cpu_pdn;
extern bool slp_infra_pdn;

extern bool slp_chk_golden;
extern bool slp_dump_gpio;
extern bool slp_dump_regs;
extern int slp_pcm_timer;

#endif
