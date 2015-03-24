#ifndef __PMIC_WRAP_REGS_H__
#define __PMIC_WRAP_REGS_H__
#include <mach/mt_reg_base.h>
#include <mach/mt_irq.h>
#include <linux/cdev.h>

#include "mach/sync_write.h"

#define PMIC_WRAP_DEBUG

#define PWRAPTAG                "[PWRAP] "
#ifdef PMIC_WRAP_DEBUG
#define PWRAPDEB(fmt, arg...)     pr_info(PWRAPTAG "cpuid=%d," fmt, raw_smp_processor_id(), ##arg)
  /* #define PWRAPLOG(fmt, arg...)   pr_info(PWRAPTAG fmt,##arg) */
#define PWRAPFUC(fmt, arg...)     pr_info(PWRAPTAG "cpuid=%d,%s\n", raw_smp_processor_id(), __func__)
  /* #define PWRAPFUC(fmt, arg...)   pr_info(PWRAPTAG "%s\n", __FUNCTION__) */
#endif
/* typedef unsigned int        U32; */
/* typedef signed int          S32; */
#define PWRAPLOG(fmt, arg...)   pr_info(PWRAPTAG fmt, ##arg)
#define PWRAPERR(fmt, arg...)   pr_err(PWRAPTAG "ERROR,line=%d " fmt, __LINE__, ##arg)
#define PWRAPREG(fmt, arg...)   pr_err(PWRAPTAG fmt, ##arg)

/* #define SLV_6323 */
#define SLV_6320		/* for re-use 6320 related code */
#define SLV_6397

#ifdef SLV_6323
#define PMIC_WRAP_BASE (PWRAP_BASE)	/* 0x1000D000 */
#else				/* SLV_6397(8135) */
/* PMIC_WRAP_BASE already defined in mt_reg_base.h (0xF000F000) */
#endif

#ifdef SLV_6323
#define MT_PMIC_WRAP_IRQ_ID MT6582_PMIC_WRAP_IRQ_ID
#elif defined(SLV_6397)
#define MT_PMIC_WRAP_IRQ_ID MT8135_PMIC_WRAP_IRQ_ID
#endif

#define CKSYS_BASE              (INFRA_BASE)

#define PMIC_WRAP_REG_RANGE   85.5
#define PMIC_WRAP_REG_MAX     0xF000F154

/* -------macro for timeout setting-------------------------------- */
/******************************************************************************
global variable and  sys interface
******************************************************************************/
#define TIMEOUT_RESET	    0xFF	/* us */
#define TIMEOUT_READ	    0xFF	/* us */
#define TIMEOUT_WAIT_IDLE	0xFF	/* us */
/* -------macro for clock gating-------------------------------- */
/* INFRACFG_BASE = 0xF0001000; */
#define INFRA_GLOBALCON_PDN0	(INFRACFG_BASE+0x40)
#define INFRA_GLOBALCON_PDN1    (INFRACFG_BASE+0X44)
/* PERICFG_BASE = 0xF0003000; */
#define PERI_GLOBALCON_PDN1_SET (PERICFG_BASE + 0x0c)
#define PERI_GLOBALCON_PDN1_CLR (PERICFG_BASE + 0x14)
#define PERI_GLOBALCON_PDN1_STA (PERICFG_BASE + 0x1c)
/* TOPRGU_BASE = 0xF0000000; */
#define CLK_PDN_SET            (TOPRGU_BASE + 0x170)
#define CLK_PDN_CLR            (TOPRGU_BASE + 0x174)
#define CLK_PDN_STA            (TOPRGU_BASE + 0x178)

/* -------macro for spi clock config-------------------------------- */
#define CLK_CFG_4_CLR                       (CKSYS_BASE+0x088)	/* 6582 */
#define CLK_SPI_CK_26M	0xF

/* -------macro for spi clock config-------------------------------- */
#define CLK_CFG_8                       (TOPRGU_BASE+0x164)	/* 6589 */
#define WDT_SWSYSRST                    (TOPRGU_BASE+0x18)	/* 6589 */


#define PMIC_WRAP_MUX_SEL               (PMIC_WRAP_BASE+0x0)
#define PMIC_WRAP_WRAP_EN               (PMIC_WRAP_BASE+0x4)
#define PMIC_WRAP_DIO_EN                (PMIC_WRAP_BASE+0x8)
#define PMIC_WRAP_SIDLY                 (PMIC_WRAP_BASE+0xC)
#define PMIC_WRAP_CSHEXT                (PMIC_WRAP_BASE+0x10)
#define PMIC_WRAP_CSHEXT_WRITE          (PMIC_WRAP_BASE+0x14)
#define PMIC_WRAP_CSHEXT_READ           (PMIC_WRAP_BASE+0x18)
#define PMIC_WRAP_CSLEXT_START          (PMIC_WRAP_BASE+0x1C)
#define PMIC_WRAP_CSLEXT_END            (PMIC_WRAP_BASE+0x20)
#define PMIC_WRAP_STAUPD_PRD            (PMIC_WRAP_BASE+0x24)
#define PMIC_WRAP_STAUPD_GRPEN          (PMIC_WRAP_BASE+0x28)
#define PMIC_WRAP_STAUPD_MAN_TRIG       (PMIC_WRAP_BASE+0x2C)
#define PMIC_WRAP_STAUPD_STA            (PMIC_WRAP_BASE+0x30)
#define PMIC_WRAP_EVENT_IN_EN           (PMIC_WRAP_BASE+0x34)
#define PMIC_WRAP_EVENT_DST_EN          (PMIC_WRAP_BASE+0x38)
#define PMIC_WRAP_WRAP_STA              (PMIC_WRAP_BASE+0x3C)
#define PMIC_WRAP_RRARB_INIT            (PMIC_WRAP_BASE+0x40)
#define PMIC_WRAP_RRARB_EN              (PMIC_WRAP_BASE+0x44)
#define PMIC_WRAP_RRARB_STA0            (PMIC_WRAP_BASE+0x48)
#define PMIC_WRAP_RRARB_STA1            (PMIC_WRAP_BASE+0x4C)
#define PMIC_WRAP_HARB_INIT             (PMIC_WRAP_BASE+0x50)
#define PMIC_WRAP_HARB_HPRIO            (PMIC_WRAP_BASE+0x54)
#define PMIC_WRAP_HIPRIO_ARB_EN         (PMIC_WRAP_BASE+0x58)
#define PMIC_WRAP_HARB_STA0             (PMIC_WRAP_BASE+0x5C)
#define PMIC_WRAP_HARB_STA1             (PMIC_WRAP_BASE+0x60)
#define PMIC_WRAP_MAN_EN                (PMIC_WRAP_BASE+0x64)
#define PMIC_WRAP_MAN_CMD               (PMIC_WRAP_BASE+0x68)
#define PMIC_WRAP_MAN_RDATA             (PMIC_WRAP_BASE+0x6C)
#define PMIC_WRAP_MAN_VLDCLR            (PMIC_WRAP_BASE+0x70)
#define PMIC_WRAP_WACS0_EN              (PMIC_WRAP_BASE+0x74)
#define PMIC_WRAP_INIT_DONE0            (PMIC_WRAP_BASE+0x78)
#define PMIC_WRAP_WACS0_CMD             (PMIC_WRAP_BASE+0x7C)
#define PMIC_WRAP_WACS0_RDATA           (PMIC_WRAP_BASE+0x80)
#define PMIC_WRAP_WACS0_VLDCLR          (PMIC_WRAP_BASE+0x84)
#define PMIC_WRAP_WACS1_EN              (PMIC_WRAP_BASE+0x88)
#define PMIC_WRAP_INIT_DONE1            (PMIC_WRAP_BASE+0x8C)
#define PMIC_WRAP_WACS1_CMD             (PMIC_WRAP_BASE+0x90)
#define PMIC_WRAP_WACS1_RDATA           (PMIC_WRAP_BASE+0x94)
#define PMIC_WRAP_WACS1_VLDCLR          (PMIC_WRAP_BASE+0x98)
#define PMIC_WRAP_WACS2_EN              (PMIC_WRAP_BASE+0x9C)
#define PMIC_WRAP_INIT_DONE2            (PMIC_WRAP_BASE+0xA0)
#define PMIC_WRAP_WACS2_CMD             (PMIC_WRAP_BASE+0xA4)
#define PMIC_WRAP_WACS2_RDATA           (PMIC_WRAP_BASE+0xA8)
#define PMIC_WRAP_WACS2_VLDCLR          (PMIC_WRAP_BASE+0xAC)
#define PMIC_WRAP_INT_EN                (PMIC_WRAP_BASE+0xB0)
#define PMIC_WRAP_INT_FLG_RAW           (PMIC_WRAP_BASE+0xB4)
#define PMIC_WRAP_INT_FLG               (PMIC_WRAP_BASE+0xB8)
#define PMIC_WRAP_INT_CLR               (PMIC_WRAP_BASE+0xBC)
#define PMIC_WRAP_SIG_ADR               (PMIC_WRAP_BASE+0xC0)
#define PMIC_WRAP_SIG_MODE              (PMIC_WRAP_BASE+0xC4)
#define PMIC_WRAP_SIG_VALUE             (PMIC_WRAP_BASE+0xC8)
#define PMIC_WRAP_SIG_ERRVAL            (PMIC_WRAP_BASE+0xCC)
#define PMIC_WRAP_CRC_EN                (PMIC_WRAP_BASE+0xD0)
#define PMIC_WRAP_EVENT_STA             (PMIC_WRAP_BASE+0xD4)
#define PMIC_WRAP_EVENT_STACLR          (PMIC_WRAP_BASE+0xD8)
#define PMIC_WRAP_TIMER_EN              (PMIC_WRAP_BASE+0xDC)
#define PMIC_WRAP_TIMER_STA             (PMIC_WRAP_BASE+0xE0)
#define PMIC_WRAP_WDT_UNIT              (PMIC_WRAP_BASE+0xE4)
#define PMIC_WRAP_WDT_SRC_EN            (PMIC_WRAP_BASE+0xE8)
#define PMIC_WRAP_WDT_FLG               (PMIC_WRAP_BASE+0xEC)
#define PMIC_WRAP_DEBUG_INT_SEL         (PMIC_WRAP_BASE+0xF0)
#define PMIC_WRAP_DVFS_ADR0             (PMIC_WRAP_BASE+0xF4)
#define PMIC_WRAP_DVFS_WDATA0           (PMIC_WRAP_BASE+0xF8)
#define PMIC_WRAP_DVFS_ADR1             (PMIC_WRAP_BASE+0xFC)
#define PMIC_WRAP_DVFS_WDATA1           (PMIC_WRAP_BASE+0x100)
#define PMIC_WRAP_DVFS_ADR2             (PMIC_WRAP_BASE+0x104)
#define PMIC_WRAP_DVFS_WDATA2           (PMIC_WRAP_BASE+0x108)
#define PMIC_WRAP_DVFS_ADR3             (PMIC_WRAP_BASE+0x10C)
#define PMIC_WRAP_DVFS_WDATA3           (PMIC_WRAP_BASE+0x110)
#define PMIC_WRAP_DVFS_ADR4             (PMIC_WRAP_BASE+0x114)
#define PMIC_WRAP_DVFS_WDATA4           (PMIC_WRAP_BASE+0x118)
#define PMIC_WRAP_DVFS_ADR5             (PMIC_WRAP_BASE+0x11C)
#define PMIC_WRAP_DVFS_WDATA5           (PMIC_WRAP_BASE+0x120)
#define PMIC_WRAP_DVFS_ADR6             (PMIC_WRAP_BASE+0x124)
#define PMIC_WRAP_DVFS_WDATA6           (PMIC_WRAP_BASE+0x128)
#define PMIC_WRAP_DVFS_ADR7             (PMIC_WRAP_BASE+0x12C)
#define PMIC_WRAP_DVFS_WDATA7           (PMIC_WRAP_BASE+0x130)
#define PMIC_WRAP_CIPHER_KEY_SEL        (PMIC_WRAP_BASE+0x134)
#define PMIC_WRAP_CIPHER_IV_SEL         (PMIC_WRAP_BASE+0x138)
#define PMIC_WRAP_CIPHER_LOAD           (PMIC_WRAP_BASE+0x13C)
#define PMIC_WRAP_CIPHER_START          (PMIC_WRAP_BASE+0x140)
#define PMIC_WRAP_CIPHER_RDY            (PMIC_WRAP_BASE+0x144)
#define PMIC_WRAP_CIPHER_MODE           (PMIC_WRAP_BASE+0x148)
#define PMIC_WRAP_CIPHER_SWRST          (PMIC_WRAP_BASE+0x14C)
#define PMIC_WRAP_CIPHER_IV0            (PMIC_WRAP_BASE+0x150)
#define PMIC_WRAP_CIPHER_IV1            (PMIC_WRAP_BASE+0x154)
#define PMIC_WRAP_CIPHER_IV2            (PMIC_WRAP_BASE+0x158)
#define PMIC_WRAP_DCM_EN                (PMIC_WRAP_BASE+0x15C)
#define PMIC_WRAP_DCM_DBC_PRD           (PMIC_WRAP_BASE+0x160)


/* -----macro for wrapper  regsister-------------------------------------------------------- */
#define GET_STAUPD_DLE_CNT(x)        ((x>>0)  & 0x00000007)
#define GET_STAUPD_ALE_CNT(x)        ((x>>3)  & 0x00000007)
#define GET_STAUPD_FSM(x)            ((x>>6)  & 0x00000007)
#define GET_PWRAP_GPS_ACK(x)         ((x>>0)  & 0x00000001)
#define GET_GPS_PWRAP_REQ(x)         ((x>>1)  & 0x00000001)
#define GET_GPSINF_DLE_CNT(x)        ((x>>4)  & 0x00000003)
#define GET_GPSINF_ALE_CNT(x)        ((x>>6)  & 0x00000003)
#define GET_GPS_INF_FSM(x)           ((x>>8)  & 0x00000007)
#define GET_PWRAP_GPS_WDATA(x)       ((x>>15) & 0x0001ffff)
#define GET_WRAP_CH_DLE_RESTCNT(x)   ((x>>0)  & 0x00000007)
#define GET_WRAP_CH_ALE_RESTCNT(x)   ((x>>3)  & 0x00000003)
#define GET_WRAP_AG_DLE_RESTCNT(x)   ((x>>5)  & 0x00000003)
#define GET_WRAP_CH_W(x)             ((x>>7)  & 0x00000001)
#define GET_WRAP_CH_REQ(x)           ((x>>8)  & 0x00000001)
#define GET_AG_WRAP_W(x)             ((x>>9)  & 0x00000001)
#define GET_AG_WRAP_REQ(x)           ((x>>10) & 0x00000001)
#define GET_WRAP_FSM(x)              ((x>>11) & 0x0000000f)
#define GET_HARB_WRAP_WDATA(x)       ((x>>0)  & 0x0000ffff)
#define GET_HARB_WRAP_ADR(x)         ((x>>16) & 0x00007fff)
#define GET_HARB_WRAP_REQ(x)         ((x>>31) & 0x00000001)
#define GET_HARB_DLE_EMPTY(x)        ((x>>0)  & 0x00000001)
#define GET_HARB_DLE_FULL(x)         ((x>>1)  & 0x00000001)
#define GET_HARB_VLD(x)              ((x>>2)  & 0x00000001)
#define GET_HARB_DLE_OWN(x)          ((x>>3)  & 0x0000000f)
#define GET_HARB_OWN(x)              ((x>>7)  & 0x0000000f)
#define GET_HARB_DLE_RESTCNT(x)      ((x>>11) & 0x0000000f)
#define GET_AG_HARB_REQ(x)           ((x>>15) & 0x000001ff)
#define GET_HARB_WRAP_W(x)           ((x>>24) & 0x00000001)
#define GET_HARB_WRAP_REQ0(x)        ((x>>25) & 0x00000001)
#define GET_SPI_WDATA(x)             ((x>>0)  & 0x000000ff)
#define GET_SPI_OP(x)                ((x>>8)  & 0x0000001f)
#define GET_SPI_W(x)                 ((x>>13) & 0x00000001)
#define GET_MAN_RDATA(x)             ((x>>0)  & 0x000000ff)
#define GET_MAN_FSM(x)               ((x>>8)  & 0x00000007)
#define GET_MAN_REQ(x)               ((x>>11) & 0x00000001)
#define GET_WACS0_WDATA(x)           ((x>>0)  & 0x0000ffff)
#define GET_WACS0_ADR(x)             ((x>>16) & 0x00007fff)
#define GET_WACS0_WRITE(x)           ((x>>31) & 0x00000001)
#define GET_WACS0_RDATA(x)           ((x>>0)  & 0x0000ffff)
#define GET_WACS0_FSM(x)             ((x>>16) & 0x00000007)
#define GET_WACS0_REQ(x)             ((x>>19) & 0x00000001)
#define GET_SYNC_IDLE0(x)            ((x>>20) & 0x00000001)
#define GET_INIT_DONE0(x)            ((x>>21) & 0x00000001)
#define GET_WACS1_WDATA(x)           ((x>>0)  & 0x0000ffff)
#define GET_WACS1_ADR(x)             ((x>>16) & 0x00007fff)
#define GET_WACS1_WRITE(x)           ((x>>31) & 0x00000001)
#define GET_WACS1_RDATA(x)           ((x>>0)  & 0x0000ffff)
#define GET_WACS1_FSM(x)             ((x>>16) & 0x00000007)
#define GET_WACS1_REQ(x)             ((x>>19) & 0x00000001)
#define GET_SYNC_IDLE1(x)            ((x>>20) & 0x00000001)
#define GET_INIT_DONE1(x)            ((x>>21) & 0x00000001)
#define GET_WACS2_WDATA(x)           ((x>>0)  & 0x0000ffff)
#define GET_WACS2_ADR(x)             ((x>>16) & 0x00007fff)
#define GET_WACS2_WRITE(x)           ((x>>31) & 0x00000001)
#define GET_WACS2_RDATA(x)           ((x>>0)  & 0x0000ffff)
#define GET_WACS2_FSM(x)             ((x>>16) & 0x00000007)
#define GET_WACS2_REQ(x)             ((x>>19) & 0x00000001)
#define GET_SYNC_IDLE2(x)            ((x>>20) & 0x00000001)
#define GET_INIT_DONE2(x)            ((x>>21) & 0x00000001)

/* -----macro for wrapper bridge regsister-------------------------------------------------------- */
/* APB Module peri_pwrap_bridge */

#define PERI_PWRAP_BRIDGE_IARB_INIT         (PERI_PWRAP_BRIDGE_BASE+0x0)
#define PERI_PWRAP_BRIDGE_IORD_ARB_EN       (PERI_PWRAP_BRIDGE_BASE+0x4)
#define PERI_PWRAP_BRIDGE_IARB_STA0         (PERI_PWRAP_BRIDGE_BASE+0x8)
#define PERI_PWRAP_BRIDGE_IARB_STA1         (PERI_PWRAP_BRIDGE_BASE+0xC)
#define PERI_PWRAP_BRIDGE_WACS3_EN          (PERI_PWRAP_BRIDGE_BASE+0x10)
#define PERI_PWRAP_BRIDGE_INIT_DONE3        (PERI_PWRAP_BRIDGE_BASE+0x14)
#define PERI_PWRAP_BRIDGE_WACS3_CMD         (PERI_PWRAP_BRIDGE_BASE+0x18)
#define PERI_PWRAP_BRIDGE_WACS3_RDATA       (PERI_PWRAP_BRIDGE_BASE+0x1C)
#define PERI_PWRAP_BRIDGE_WACS3_VLDCLR      (PERI_PWRAP_BRIDGE_BASE+0x20)
#define PERI_PWRAP_BRIDGE_WACS4_EN          (PERI_PWRAP_BRIDGE_BASE+0x24)
#define PERI_PWRAP_BRIDGE_INIT_DONE4        (PERI_PWRAP_BRIDGE_BASE+0x28)
#define PERI_PWRAP_BRIDGE_WACS4_CMD         (PERI_PWRAP_BRIDGE_BASE+0x2C)
#define PERI_PWRAP_BRIDGE_WACS4_RDATA       (PERI_PWRAP_BRIDGE_BASE+0x30)
#define PERI_PWRAP_BRIDGE_WACS4_VLDCLR      (PERI_PWRAP_BRIDGE_BASE+0x34)
#define PERI_PWRAP_BRIDGE_INT_EN            (PERI_PWRAP_BRIDGE_BASE+0x38)
#define PERI_PWRAP_BRIDGE_INT_FLG_RAW       (PERI_PWRAP_BRIDGE_BASE+0x3C)
#define PERI_PWRAP_BRIDGE_INT_FLG           (PERI_PWRAP_BRIDGE_BASE+0x40)
#define PERI_PWRAP_BRIDGE_INT_CLR           (PERI_PWRAP_BRIDGE_BASE+0x44)
#define PERI_PWRAP_BRIDGE_TIMER_EN          (PERI_PWRAP_BRIDGE_BASE+0x48)
#define PERI_PWRAP_BRIDGE_TIMER_STA         (PERI_PWRAP_BRIDGE_BASE+0x4C)
#define PERI_PWRAP_BRIDGE_WDT_UNIT          (PERI_PWRAP_BRIDGE_BASE+0x50)
#define PERI_PWRAP_BRIDGE_WDT_SRC_EN        (PERI_PWRAP_BRIDGE_BASE+0x54)
#define PERI_PWRAP_BRIDGE_WDT_FLG           (PERI_PWRAP_BRIDGE_BASE+0x58)
#define PERI_PWRAP_BRIDGE_DEBUG_INT_SEL     (PERI_PWRAP_BRIDGE_BASE+0x5C)

/* -----macro for wrapper bridge regsister-------------------------------------------------------- */

/* Macros */
#define ENABLE	1
#define DISABLE 0
#define DISABLE_ALL 0

/* #define PWRAP_ENABLE_DCM              WRAP_WR32(PMIC_WRAP_DCM_EN, ENABLE) */
/* #define PWRAP_DISABLE_DCM             WRAP_WR32(PMIC_WRAP_DCM_EN, DISABLE) */
/* #define PWRAP_DISABLE_DCM_DBC_PRD     WRAP_WR32(PMIC_WRAP_DCM_DBC_PRD, DISABLE) */
/* #define PWRAP_SET_DCM_DBC_PRD(x)      WRAP_WR32(PMIC_WRAP_DCM_DBC_PRD, x) */
/*  */
/* #define PWRAP_ENABLE                  WRAP_WR32(PMIC_WRAP_WRAP_EN,ENABLE) //enable wrap */
/* #define PWRAP_DISABLE                 WRAP_WR32(PMIC_WRAP_WRAP_EN,DISABLE) //disable wrap */

/* HIPRIS_ARB */
#define MDINF		(1 << 0)
#define WACS0		(1 << 1)
#define WACS1		(1 << 2)
#define WACS2		(1 << 3)
#define DVFSINF		(1 << 4)
#define STAUPD		(1 << 5)
#define GPSINF		(1 << 6)
/* #define PWRAP_HIPRIO_ARB_EN(x)        WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,x)//need read back reg */
/* #define PWRAP_ENABLE_WACS2            WRAP_WR32(PMIC_WRAP_WACS2_EN,ENABLE) */

/* MUX SEL */
#define	WRAPPER_MODE	0
#define	MANUAL_MODE		1

/* OP TYPE */
#define OP_TYPE_CK	0	/* for MT6323 */
#define OP_TYPE_CSL	1	/* for MT6320 */
#define MSB		1	/* for MT6323 */
#define LSB		0	/* for MT6320 */

/* SIG mode */
#define CHECK_CRC	0
#define CHECK_SIG	1



/* macro for staupd sta fsm */
#define STAUPD_FSM_IDLE               (0x00)
#define STAUPD_FSM_REQ                (0x02)
#define STAUPD_FSM_WFDLE              (0x04)	/* wait for dle,wait for read data done, */

/* macro for WRAP_STA  FSM */
/* #define WRAP_STA_FSM_IDLE               (0x00) */
/* #define WRAP_STA_IDLE               (0x00) */

/* macro for MAN_RDATA  FSM */
#define MAN_FSM_NO_REQ             (0x00)
#define MAN_FSM_IDLE               (0x00)
#define MAN_FSM_REQ                (0x02)
#define MAN_FSM_WFDLE              (0x04)	/* wait for dle,wait for read data done, */
#define MAN_FSM_WFVLDCLR           (0x06)

/* macro for WACS_FSM */
#define WACS_FSM_IDLE               (0x00)
#define WACS_FSM_REQ                (0x02)
#define WACS_FSM_WFDLE              (0x04)	/* wait for dle,wait for read data done, */
#define WACS_FSM_WFVLDCLR           (0x06)	/* finish read data , wait for valid flag clearing */
#define WACS_INIT_DONE              (0x01)
#define WACS_SYNC_IDLE              (0x01)
#define WACS_SYNC_BUSY              (0x00)



/* -----macro for  regsister@PMIC ------------------------------------------------- */
#define PMIC_BASE                 (0x0000)
#define PMIC_WRP_CKPDN            (PMIC_BASE+0x011A)	/* 0x0056 */
#define PMIC_WRP_RST_CON          (PMIC_BASE+0x0120)	/* 0x005C */
#define PMIC_TOP_CKCON2           (PMIC_BASE+0x012A)
#define PMIC_TOP_CKCON3           (PMIC_BASE+0x01D4)

/* -----macro for dewrapper regsister-------------------------------------------------------- */
#define DEW_BASE                  (0xBC00)
#define DEW_EVENT_OUT_EN   (DEW_BASE+0x0)
#define DEW_DIO_EN         (DEW_BASE+0x2)
#define DEW_EVENT_SRC_EN   (DEW_BASE+0x4)
#define DEW_EVENT_SRC      (DEW_BASE+0x6)
#define DEW_EVENT_FLAG     (DEW_BASE+0x8)
#define DEW_READ_TEST      (DEW_BASE+0xA)
#define DEW_WRITE_TEST     (DEW_BASE+0xC)
#define DEW_CRC_EN         (DEW_BASE+0xE)
#define DEW_CRC_VAL        (DEW_BASE+0x10)
#define DEW_MON_GRP_SEL    (DEW_BASE+0x12)
#define DEW_MON_FLAG_SEL   (DEW_BASE+0x14)
#define DEW_EVENT_TEST     (DEW_BASE+0x16)
#define DEW_CIPHER_KEY_SEL (DEW_BASE+0x18)
#define DEW_CIPHER_IV_SEL  (DEW_BASE+0x1A)
#define DEW_CIPHER_LOAD    (DEW_BASE+0x1C)
#define DEW_CIPHER_START   (DEW_BASE+0x1E)
#define DEW_CIPHER_RDY     (DEW_BASE+0x20)
#define DEW_CIPHER_MODE    (DEW_BASE+0x22)
#define DEW_CIPHER_SWRST   (DEW_BASE+0x24)
#define DEW_CIPHER_IV0     (DEW_BASE+0x26)
#define DEW_CIPHER_IV1     (DEW_BASE+0x28)
#define DEW_CIPHER_IV2     (DEW_BASE+0x2A)
#define DEW_CIPHER_IV3     (DEW_BASE+0x2C)
#define DEW_CIPHER_IV4     (DEW_BASE+0x2E)
#define DEW_CIPHER_IV5     (DEW_BASE+0x30)

#define PMIC_REG_BASE      (0x0000)
#ifdef SLV_6323
#define AUXADC_CON21       (PMIC_REG_BASE+0x076C)
#define AUXADC_ADC12       (PMIC_REG_BASE+0x072C)
#define AUXADC_ADC13       (PMIC_REG_BASE+0x072E)
#define AUXADC_ADC14       (PMIC_REG_BASE+0x0730)
#define AUXADC_CON2        (PMIC_REG_BASE+0x0746)
#define AUXADC_CON3        (PMIC_REG_BASE+0x0748)
#endif

#ifdef SLV_6320
  /* #define PMIC_TOP_CKCON2    (PMIC_REG_BASE+0x012A) */
#elif defined SLV_6323
#define TOP_CKCON1_CLR     (PMIC_REG_BASE+0x012A)
#define TOP_CKCON1         (PMIC_REG_BASE+0x0126)
#endif

#if 0				/* no use in hal */
/* #define EFUSE_VAL_0_15     (PMIC_REG_BASE+0x060E) */
/* #define EFUSE_VAL_16_31    (PMIC_REG_BASE+0x0610) */
/* #define EFUSE_VAL_32_47    (PMIC_REG_BASE+0x0612) */
/* #define EFUSE_VAL_48_63    (PMIC_REG_BASE+0x0614) */
/* #define EFUSE_VAL_64_79    (PMIC_REG_BASE+0x0616) */
#endif

/* -----macro for dewrapper defaule value------------------------------------------------------- */
#define DEFAULT_VALUE_READ_TEST      0x5aa5
#define WRITE_TEST_VALUE             0xa55a

/* -----macro for manual commnd -------------------------------------------------------- */
#define OP_WR    (0x1)
#define OP_RD    (0x0)

#define OP_CSH   (0x0)
#define OP_CSL   (0x1)
#define OP_CK    (0x2)

#define OP_OUTS  (0x8)
#define OP_OUTD  (0x9)
#define OP_OUTQ  (0xA)

#define OP_INS   (0xC)
#define OP_INS0  (0xD)
#define OP_IND   (0xE)
#define OP_INQ   (0xF)

#define OP_OS2IS (0x10)
#define OP_OS2ID (0x11)
#define OP_OS2IQ (0x12)
#define OP_OD2IS (0x13)
#define OP_OD2ID (0x14)
#define OP_OD2IQ (0x15)
#define OP_OQ2IS (0x16)
#define OP_OQ2ID (0x17)
#define OP_OQ2IQ (0x18)

#define OP_OSNIS (0x19)
#define OP_ODNID (0x1A)

#define E_PWR_INVALID_ARG               1
#define E_PWR_INVALID_RW                2
#define E_PWR_INVALID_ADDR              3
#define E_PWR_INVALID_WDAT              4
#define E_PWR_INVALID_OP_MANUAL         5
#define E_PWR_NOT_IDLE_STATE            6
#define E_PWR_NOT_INIT_DONE             7
#define E_PWR_NOT_INIT_DONE_READ        8
#define E_PWR_WAIT_IDLE_TIMEOUT         9
#define E_PWR_WAIT_IDLE_TIMEOUT_READ    10
#define E_PWR_INIT_SIDLY_FAIL           11
#define E_PWR_RESET_TIMEOUT             12
#define E_PWR_TIMEOUT                   13

#define E_PWR_INIT_RESET_SPI            20
#define E_PWR_INIT_SIDLY                21
#define E_PWR_INIT_REG_CLOCK            22
#define E_PWR_INIT_ENABLE_PMIC          23
#define E_PWR_INIT_DIO                  24
#define E_PWR_INIT_CIPHER               25
#define E_PWR_INIT_WRITE_TEST           26
#define E_PWR_INIT_ENABLE_CRC           27
#define E_PWR_INIT_ENABLE_DEWRAP        28
#define E_PWR_INIT_ENABLE_EVENT         29

#define E_PWR_READ_TEST_FAIL            30
#define E_PWR_WRITE_TEST_FAIL           31
#define E_PWR_SWITCH_DIO                32

#define PWRAP_INT_DEBUG              0x80000000
#define PWRAP_INT_RRARB_DLE_FIFO_UNF 0x08000000
#define PWRAP_INT_RRARB_DLE_FIFO_OVF 0x04000000
#define PWRAP_INT_RRARB_REQ_FIFO_UNF 0x02000000
#define PWRAP_INT_RRARB_REQ_FIFO_OVF 0x01000000
#define PWRAP_INT_STAUPD_DLE_CNT_OVF 0x00800000
#define PWRAP_INT_STAUPD_ALE_CNT_OVF 0x00400000
#define PWRAP_INT_PERI_BRIDGE        0x00200000
#define PWRAP_INT_WACS2_CMD_MISS     0x00100000
#define PWRAP_INT_WACS2_UNEXP_DLE    0x00080000
#define PWRAP_INT_WACS2_UNEXP_VLDCLR 0x00040000
#define PWRAP_INT_WACS1_CMD_MISS     0x00020000
#define PWRAP_INT_WACS1_UNEXP_DLE    0x00010000
#define PWRAP_INT_WACS1_UNEXP_VLDCLR 0x00008000
#define PWRAP_INT_WACS0_CMD_MISS     0x00004000
#define PWRAP_INT_WACS0_UNEXP_DLE    0x00002000
#define PWRAP_INT_WACS0_UNEXP_VLDCLR 0x00001000
#define PWRAP_INT_HARBDLE_OVF        0x00000800
#define PWRAP_INT_HARBDLE_UNF        0x00000400
#define PWRAP_INT_AGDLE_OVF          0x00000200
#define PWRAP_INT_AGDLE_UNF          0x00000100
#define PWRAP_INT_CHDLE_OVF          0x00000080
#define PWRAP_INT_CHDLE_UNF          0x00000040
#define PWRAP_INT_MAN_CMD_MISS       0x00000020
#define PWRAP_INT_MAN_UNEXP_DLE      0x00000010
#define PWRAP_INT_MAN_UNEXP_VLDCLR   0x00000008
#define PWRAP_INT_EVENT_EMPTY        0x00000004
#define PWRAP_INT_SIG_ERR            0x00000002
#define PWRAP_INT_WDT                0x00000001

/* -----macro for read/write register -------------------------------------------------------- */

/* #define WRAP_RD32(addr)            (*(volatile U32 *)(addr)) */
/* #define WRAP_WR32(addr,data)       ((*(volatile U32 *)(addr)) = (U32)data) */

/* #define WRAP_SET_BIT(BS,REG)       ((*(volatile U32*)(REG)) |= (U32)(BS)) */
/* #define WRAP_CLR_BIT(BS,REG)       ((*(volatile U32*)(REG)) &= ~((U32)(BS))) */

#define WRAP_RD32(addr)            __raw_readl((void *)addr)
#define WRAP_WR32(addr, val)        mt65xx_reg_sync_writel((val), ((void *)addr))

#define WRAP_SET_BIT(BS, REG)       mt65xx_reg_sync_writel((__raw_readl((void *)REG) | (u32)(BS)), ((void *)REG))
#define WRAP_CLR_BIT(BS, REG)       mt65xx_reg_sync_writel((__raw_readl((void *)REG) & (~(u32)(BS))), ((void *)REG))

/* -----------------soft reset -------------------------------------------------------- */
#define INFRA_GLOBALCON_RST0               (INFRACFG_BASE+0x030)

#define PWRAP_SOFT_RESET                   WRAP_SET_BIT(1<<7, INFRA_GLOBALCON_RST0)
#define PWRAP_CLEAR_SOFT_RESET_BIT         WRAP_CLR_BIT(1<<7, INFRA_GLOBALCON_RST0)
#define PERI_GLOBALCON_RST1                (PERICFG_BASE+0x004)

#endif				/* __PMIC_WRAP_REGS_H__ */
