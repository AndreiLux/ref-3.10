/**
* @file    mt_clkmgr.c
* @brief   Driver for Clock Manager
*
*/

#define __MT_CLKMGR_C__

/*=============================================================*/
/* Include files */
/*=============================================================*/

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/seq_file.h>

#include <asm/uaccess.h>

/* project includes */
#include <mach/memory.h>
#include <mach/mt_typedefs.h>
#include <mach/sync_write.h>
#include <mach/mt_dcm.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_freqhopping.h>
#include <mach/pmic_mt6320_sw.h>
#include <mach/mt_boot.h>

/* local includes */
#include <mach/mt_clkmgr.h>

/* forward references */


/*=============================================================*/
/* Macro definition                                            */
/*=============================================================*/

#ifndef BIT
#define BIT(_bit_)          (1UL << (_bit_))
#endif				/* BIT */

#define BITS(_bits_, _val_) ((BIT(((1)?_bits_)+1)-BIT(((0)?_bits_))) & (_val_<<((0)?_bits_)))
#define BITMASK(_bits_)     (BIT(((1)?_bits_)+1)-BIT(((0)?_bits_)))


/*=============================================================*/
/* Local type definition                                       */
/*=============================================================*/
#define PWR_DOWN    0
#define PWR_ON      1


/*=============================================================*/
/* Local variable definition                                   */
/*=============================================================*/


/*=============================================================*/
/* Local function definition                                   */
/*=============================================================*/


/*=============================================================*/
/* Gobal function definition                                   */
/*=============================================================*/

/* CONFIG */

/* #define CONFIG_CLKMGR_EMULATION */
/* #define CONFIG_CLKMGR_SHOWLOG */
/* #define STATE_CHECK_DEBUG */

#if CLKMGR_CTP
#define CLKMGR_BRINGUP              1
#else				/* !CLKMGR_CTP */
#define CLKMGR_BRINGUP              0
#endif				/* CLKMGR_CTP */

#define PLL_SETTLE_TIME (30)	/* us */

#define CLKMGR_WORKAROUND           1
#define CLKMGR_PLL_VCG              1
#define CLKMGR_DISP_ROT_WORKAROUND  1

#if CLKMGR_WORKAROUND

/* memory.h */
#define IO_PHYS_TO_VIRT(p) (0xf0000000 | ((p) & 0x0fffffff))

#endif				/* CLKMGR_WORKAROUND */

#if CLKMGR_CTP
#if defined(clkmgr_udelay)
#define udelay clkmgr_udelay
#endif
#endif				/* CLKMGR_CTP */


/* LOG */

#define USING_XLOG

#ifdef USING_XLOG
#include <linux/xlog.h>

#define TAG     "Power/clkmgr"

#if CLKMGR_CTP
#define HEX_FMT "0x%X"
#else				/* !CLKMGR_CTP */
#define HEX_FMT "0x%08x"
#endif				/* CLKMGR_CTP */

#define clk_err(fmt, args...)       \
	xlog_printk(ANDROID_LOG_ERROR, TAG, fmt, ##args)
#define clk_warn(fmt, args...)      \
	xlog_printk(ANDROID_LOG_WARN, TAG, fmt, ##args)
#define clk_info(fmt, args...)      \
	xlog_printk(ANDROID_LOG_INFO, TAG, fmt, ##args)
#define clk_dbg(fmt, args...)       \
	xlog_printk(ANDROID_LOG_DEBUG, TAG, fmt, ##args)
#define clk_ver(fmt, args...)       \
	xlog_printk(ANDROID_LOG_VERBOSE, TAG, fmt, ##args)

#else

#define TAG     "[Power/clkmgr] "

#define clk_err(fmt, args...)       \
	do { pr_err(TAG); pr_info(KERN_CONT fmt, ##args); } while (0)
#define clk_warn(fmt, args...)      \
	do { pr_warn(TAG); pr_info(KERN_CONT fmt, ##args); } while (0)
#define clk_info(fmt, args...)      \
	do { pr_notice(TAG); pr_info(KERN_CONT fmt, ##args); } while (0)
#define clk_dbg(fmt, args...)       \
	do { pr_info(TAG); pr_info(KERN_CONT fmt, ##args); } while (0)
#define clk_ver(fmt, args...)       \
	do { pr_debug(TAG); pr_info(KERN_CONT fmt, ##args); } while (0)

#endif

#define FUNC_LV_API         BIT(0)
#define FUNC_LV_LOCKED      BIT(1)
#define FUNC_LV_BODY        BIT(2)
#define FUNC_LV_OP          BIT(3)
#define FUNC_LV_REG_ACCESS  BIT(4)
#define FUNC_LV_DONT_CARE   BIT(5)

#define FUNC_LV_MASK \
	(FUNC_LV_API | FUNC_LV_LOCKED | FUNC_LV_BODY | FUNC_LV_OP | FUNC_LV_REG_ACCESS | FUNC_LV_DONT_CARE)

#if defined(CONFIG_CLKMGR_SHOWLOG)

#define ENTER_FUNC(lv) \
	do { if (lv & FUNC_LV_MASK) xlog_printk(ANDROID_LOG_WARN, TAG, ">> %s()\n", __func__); } while (0)
#define EXIT_FUNC(lv) \
	do { if (lv & FUNC_LV_MASK) xlog_printk(ANDROID_LOG_WARN, TAG, "<< %s():%d\n", __func__, __LINE__); } while (0)

#else

#define ENTER_FUNC(lv)
#define EXIT_FUNC(lv)

#endif				/* defined(CONFIG_CLKMGR_SHOWLOG) */


/* Register access function */

#if defined(CONFIG_CLKMGR_SHOWLOG)

#if defined(CONFIG_CLKMGR_EMULATION)	/* XXX: NOT ACCESS REGISTER */

#define clk_readl(addr) \
	((FUNC_LV_REG_ACCESS & FUNC_LV_MASK) ? \
		xlog_printk(ANDROID_LOG_WARN, TAG, "clk_readl("HEX_FMT") @ %s():%d\n", \
			(addr), __func__, __LINE__) : \
		0, 0)

#define clk_writel(addr, val)   \
	do { if (FUNC_LV_REG_ACCESS & FUNC_LV_MASK) \
		xlog_printk(ANDROID_LOG_WARN, TAG, "clk_writel("HEX_FMT", "HEX_FMT") @ %s():%d\n", \
			(addr), (val), __func__, __LINE__); \
	} while (0)

#define clk_setl(addr, val) \
	do { if (FUNC_LV_REG_ACCESS & FUNC_LV_MASK) \
		xlog_printk(ANDROID_LOG_WARN, TAG, "clk_setl("HEX_FMT", "HEX_FMT") @ %s():%d\n", \
			(addr), (val), __func__, __LINE__); \
	} while (0)

#define clk_clrl(addr, val) \
	do { if (FUNC_LV_REG_ACCESS & FUNC_LV_MASK) \
		xlog_printk(ANDROID_LOG_WARN, TAG, "clk_clrl("HEX_FMT", "HEX_FMT") @ %s():%d\n", \
			(addr), (val), __func__, __LINE__); \
	} while (0)

#else				/* XXX: ACCESS REGISTER */

#define clk_readl(addr) \
	((FUNC_LV_REG_ACCESS & FUNC_LV_MASK) ? \
		xlog_printk(ANDROID_LOG_WARN, TAG, "clk_readl("HEX_FMT") @ %s():%d\n", (addr), __func__, __LINE__) : \
		0, DRV_Reg32(addr))

#define clk_writel(addr, val)   \
	do { \
		unsigned int value; \
		if (FUNC_LV_REG_ACCESS & FUNC_LV_MASK) \
			xlog_printk(ANDROID_LOG_WARN, TAG, "clk_writel("HEX_FMT", "HEX_FMT") @ %s():%d\n", \
				(addr), (value = val), __func__, __LINE__); \
			mt65xx_reg_sync_writel((value), (addr)); \
	} while (0)

#define clk_setl(addr, val) \
	do { \
		if (FUNC_LV_REG_ACCESS & FUNC_LV_MASK) \
			xlog_printk(ANDROID_LOG_WARN, TAG, "clk_setl("HEX_FMT", "HEX_FMT") @ %s():%d\n", \
				(addr), (val), __func__, __LINE__); \
			mt65xx_reg_sync_writel(clk_readl(addr) | (val), (addr)); \
	} while (0)

#define clk_clrl(addr, val) \
	do { \
		if (FUNC_LV_REG_ACCESS & FUNC_LV_MASK) \
			xlog_printk(ANDROID_LOG_WARN, TAG, "clk_clrl("HEX_FMT", "HEX_FMT") @ %s():%d\n", \
				(addr), (val), __func__, __LINE__); \
			mt65xx_reg_sync_writel(clk_readl(addr) & ~(val), (addr)); \
	} while (0)

#endif				/* defined(CONFIG_CLKMGR_EMULATION) */

#else

#define clk_readl(addr)         DRV_Reg32(addr)
#define clk_writel(addr, val)   mt65xx_reg_sync_writel(val, addr)
#define clk_setl(addr, val)     mt65xx_reg_sync_writel(clk_readl(addr) | (val), addr)
#define clk_clrl(addr, val)     mt65xx_reg_sync_writel(clk_readl(addr) & ~(val), addr)

#endif				/* defined(CONFIG_CLKMGR_SHOWLOG) */


/* INIT */

static int initialized;

/* LOCK */

static DEFINE_SPINLOCK(clock_lock);

#define clkmgr_lock(flags)      spin_lock_irqsave(&clock_lock, flags)

#define clkmgr_unlock(flags)    spin_unlock_irqrestore(&clock_lock, flags)

#define clkmgr_locked()         spin_is_locked(&clock_lock)

int clkmgr_is_locked(void)
{
	return clkmgr_locked();
}
EXPORT_SYMBOL(clkmgr_is_locked);


/* PLL data structure */

struct pll;

struct pll_ops {
	int (*get_state) (struct pll *pll);
	void (*enable) (struct pll *pll);
	void (*disable) (struct pll *pll);
	void (*fsel) (struct pll *pll, unsigned int value);
	int (*dump_regs) (struct pll *pll, unsigned int *ptr);
	unsigned int (*vco_calc) (struct pll *pll);
	int (*hp_enable) (struct pll *pll);
	int (*hp_disable) (struct pll *pll);
	unsigned int (*get_postdiv) (struct pll *pll);
	void (*set_postdiv) (struct pll *pll, unsigned int postdiv);
	unsigned int (*get_pcw) (struct pll *pll);
	void (*set_pcw) (struct pll *pll, unsigned int pcw);
	int (*set_freq) (struct pll *pll, unsigned int freq);
};

struct pll {
	const char *name;
	const int type;		/* TODO: NOT USED (only SDM now) */
	const int mode;		/* TODO: NOT USED */
	const int feat;
	int state;
	unsigned int cnt;
	const unsigned int en_mask;
	const unsigned int base_addr;
	const unsigned int pwr_addr;
	const struct pll_ops *ops;
	const unsigned int hp_id;
	int hp_switch;
};


/* SUBSYS data structure */

struct subsys;

struct subsys_ops {
	int (*enable) (struct subsys *sys);
	int (*disable) (struct subsys *sys);
	int (*get_state) (struct subsys *sys);
	int (*dump_regs) (struct subsys *sys, unsigned int *ptr);
};

struct subsys {
	const char *name;
	const int type;
	const int force_on;
	unsigned int state;
	unsigned int default_sta;
	const unsigned int sta_mask;	/* mask in PWR_STATUS */
	const unsigned int ctl_addr;	/* SPM_XXX_PWR_CON */
	const unsigned int sram_pdn_bits;	/* SRAM_PDN @ SPM_XXX_PWR_CON */
	const unsigned int sram_pdn_ack_bits;	/* SRAM_PDN_ACK @ SPM_XXX_PWR_CON */
	unsigned int bus_prot_mask;	/* mask in INFRA_TOPAXI_PROTECTEN & INFRA_TOPAXI_PROTECTSTA1 */
	const unsigned int si0_way_en_mask;	/* mask in TOPAXI_SI0_CTL */
	struct cg_clk *clksrc;	/* engine clock of this subsys */
	const struct subsys_ops *ops;
	const struct cg_grp *start;	/* TODO: refine it */
	const unsigned int nr_grps;	/* TODO: refine it */
};


/* CLKMUX data structure */

struct clkmux;

struct clkmux_ops {
	int (*sel) (struct clkmux *mux, enum cg_clk_id clksrc);
	enum cg_clk_id (*get) (struct clkmux *mux);
};

struct clkmux_map {
	const unsigned int val;
	const enum cg_clk_id id;
	const unsigned int mask;
};

struct clkmux {
	const char *name;
	const unsigned int base_addr;
	const struct clkmux_ops *ops;

	const struct clkmux_map *map;
	const unsigned int nr_map;
	const enum cg_clk_id drain;
};


/* CG_GRP data structure */

struct cg_grp;

struct cg_grp_ops {
	unsigned int (*get_state) (struct cg_grp *grp);
	int (*dump_regs) (struct cg_grp *grp, unsigned int *ptr);
};

struct cg_grp {
	const char *name;
	const unsigned int set_addr;
	const unsigned int clr_addr;
	const unsigned int sta_addr;
	unsigned int mask;
	unsigned int state;
	const struct cg_grp_ops *ops;
	struct subsys *sys;
};


/* CG_CLK data structure */

struct cg_clk;

struct cg_clk_ops {
	int (*get_state) (struct cg_clk *clk);
	int (*check_validity) (struct cg_clk *clk);	/* 1: valid, 0: invalid */
	int (*enable) (struct cg_clk *clk);
	int (*disable) (struct cg_clk *clk);
};

struct cg_clk {
	const char *name;
	int cnt;
	unsigned int state;
	const unsigned int mask;
	const struct cg_clk_ops *ops;
	struct cg_grp *grp;
	struct pll *parent;

	enum cg_clk_id src;
};


/* CG_CLK variable & function */

static struct subsys syss[];	/* NR_SYSS */
static struct cg_grp grps[];	/* NR_GRPS */
static struct cg_clk clks[];	/* NR_CLKS */
static struct pll plls[];	/* NR_PLLS */

static struct cg_clk_ops general_gate_cg_clk_ops;	/* XXX: set/clr/sta addr are diff */
static struct cg_clk_ops general_en_cg_clk_ops;	/* XXX: set/clr/sta addr are diff */
static struct cg_clk_ops ao_cg_clk_ops;	/* XXX: always on */

#define                  always_on_cg_clk_ops       ao_cg_clk_ops
static struct cg_clk_ops always_off_cg_clk_ops;
#define                  wo_clr_en_cg_clk_ops       wo_clr_cg_clk_ops
static struct cg_clk_ops wo_clr_gate_cg_clk_ops;
static struct cg_clk_ops null_cg_clk_ops;
static struct cg_clk_ops virtual_cg_clk_ops;

static struct cg_clk clks[] =	/* NR_CLKS */
{
#if CLKMGR_PLL_VCG
	/* CG_MAINPLL */
	[MT_CG_MIX_MAINPLL_806M_EN] = {
		.name = __stringify(MT_CG_MIX_MAINPLL_806M_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_MAINPLL_CK,
		},
	[MT_CG_MIX_MAINPLL_537P3M_EN] = {
		.name = __stringify(MT_CG_MIX_MAINPLL_537P3M_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_MAINPLL_CK,
		},
	[MT_CG_MIX_MAINPLL_322P4M_EN] = {
		.name = __stringify(MT_CG_MIX_MAINPLL_322P4M_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_MAINPLL_CK,
		},
	[MT_CG_MIX_MAINPLL_230P3M_EN] = {
		.name = __stringify(MT_CG_MIX_MAINPLL_230P3M_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_MAINPLL_CK,
		},
	/* CG_UNIVPLL */
	[MT_CG_MIX_UNIVPLL_624M_EN] = {
		.name = __stringify(MT_CG_MIX_UNIVPLL_624M_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_UNIVPLL_CK,
		},
	[MT_CG_MIX_UNIVPLL_416M_EN] = {
		.name = __stringify(MT_CG_MIX_UNIVPLL_416M_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_UNIVPLL_CK,
		},
	[MT_CG_MIX_UNIVPLL_249P6M_EN] = {
		.name = __stringify(MT_CG_MIX_UNIVPLL_249P6M_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_UNIVPLL_CK,
		},
	[MT_CG_MIX_UNIVPLL_178P3M_EN] = {
		.name = __stringify(MT_CG_MIX_UNIVPLL_178P3M_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_UNIVPLL_CK,
		},
	[MT_CG_MIX_UNIVPLL_48M_EN] = {
		.name = __stringify(MT_CG_MIX_UNIVPLL_48M_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_UNIVPLL_CK,
		},
	[MT_CG_MIX_UNIVPLL_USB48M_EN] = {
		.name = __stringify(MT_CG_MIX_UNIVPLL_USB48M_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_UNIVPLL_CK,
		},
	/* CG_MMPLL */
	[MT_CG_MIX_MMPLL_DIV2_EN] = {
		.name = __stringify(MT_CG_MIX_MMPLL_DIV2_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_MMPLL_CK,
		},
	[MT_CG_MIX_MMPLL_DIV3_EN] = {
		.name = __stringify(MT_CG_MIX_MMPLL_DIV3_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_MMPLL_CK,
		},
	[MT_CG_MIX_MMPLL_DIV5_EN] = {
		.name = __stringify(MT_CG_MIX_MMPLL_DIV5_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_MMPLL_CK,
		},
	[MT_CG_MIX_MMPLL_DIV7_EN] = {
		.name = __stringify(MT_CG_MIX_MMPLL_DIV7_EN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_MMPLL_CK, },
#else				/* !CLKMGR_PLL_VCG */
	/* CG_MAINPLL */
	[MT_CG_MIX_MAINPLL_806M_EN] = {
		.name = __stringify(MT_CG_MIX_MAINPLL_806M_EN),
		.cnt = 0,
		.mask = MIX_MAINPLL_806M_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_MAINPLL],
		.src = MT_CG_V_AD_MAINPLL_CK,
		},
	[MT_CG_MIX_MAINPLL_537P3M_EN] = {
		.name = __stringify(MT_CG_MIX_MAINPLL_537P3M_EN),
		.cnt = 0,
		.mask = MIX_MAINPLL_537P3M_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_MAINPLL],
		.src = MT_CG_V_AD_MAINPLL_CK,
		},
	[MT_CG_MIX_MAINPLL_322P4M_EN] = {
		.name = __stringify(MT_CG_MIX_MAINPLL_322P4M_EN),
		.cnt = 0,
		.mask = MIX_MAINPLL_322P4M_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_MAINPLL],
		.src = MT_CG_V_AD_MAINPLL_CK,
		},
	[MT_CG_MIX_MAINPLL_230P3M_EN] = {
		.name = __stringify(MT_CG_MIX_MAINPLL_230P3M_EN),
		.cnt = 0,
		.mask = MIX_MAINPLL_230P3M_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_MAINPLL],
		.src = MT_CG_V_AD_MAINPLL_CK,
		},
	/* CG_UNIVPLL */
	[MT_CG_MIX_UNIVPLL_624M_EN] = {
		.name = __stringify(MT_CG_MIX_UNIVPLL_624M_EN),
		.cnt = 0,
		.mask = MIX_UNIVPLL_624M_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_UNIVPLL],
		.src = MT_CG_V_AD_UNIVPLL_CK,
		},
	[MT_CG_MIX_UNIVPLL_416M_EN] = {
		.name = __stringify(MT_CG_MIX_UNIVPLL_416M_EN),
		.cnt = 0,
		.mask = MIX_UNIVPLL_416M_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_UNIVPLL],
		.src = MT_CG_V_AD_UNIVPLL_CK,
		},
	[MT_CG_MIX_UNIVPLL_249P6M_EN] = {
		.name = __stringify(MT_CG_MIX_UNIVPLL_249P6M_EN),
		.cnt = 0,
		.mask = MIX_UNIVPLL_249P6M_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_UNIVPLL],
		.src = MT_CG_V_AD_UNIVPLL_CK,
		},
	[MT_CG_MIX_UNIVPLL_178P3M_EN] = {
		.name = __stringify(MT_CG_MIX_UNIVPLL_178P3M_EN),
		.cnt = 0,
		.mask = MIX_UNIVPLL_178P3M_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_UNIVPLL],
		.src = MT_CG_V_AD_UNIVPLL_CK,
		},
	[MT_CG_MIX_UNIVPLL_48M_EN] = {
		.name = __stringify(MT_CG_MIX_UNIVPLL_48M_EN),
		.cnt = 0,
		.mask = MIX_UNIVPLL_48M_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_UNIVPLL],
		.src = MT_CG_V_AD_UNIVPLL_CK,
		},
	[MT_CG_MIX_UNIVPLL_USB48M_EN] = {
		.name = __stringify(MT_CG_MIX_UNIVPLL_USB48M_EN),
		.cnt = 0,
		.mask = MIX_UNIVPLL_USB48M_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_UNIVPLL],
		.src = MT_CG_V_AD_UNIVPLL_CK,
		},
	/* CG_MMPLL */
	[MT_CG_MIX_MMPLL_DIV2_EN] = {
		.name = __stringify(MT_CG_MIX_MMPLL_DIV2_EN),
		.cnt = 0,
		.mask = MIX_MMPLL_DIV2_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_MMPLL],
		.src = MT_CG_V_AD_MMPLL_CK,
		},
	[MT_CG_MIX_MMPLL_DIV3_EN] = {
		.name = __stringify(MT_CG_MIX_MMPLL_DIV3_EN),
		.cnt = 0,
		.mask = MIX_MMPLL_DIV3_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_MMPLL],
		.src = MT_CG_V_AD_MMPLL_CK,
		},
	[MT_CG_MIX_MMPLL_DIV5_EN] = {
		.name = __stringify(MT_CG_MIX_MMPLL_DIV5_EN),
		.cnt = 0,
		.mask = MIX_MMPLL_DIV5_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_MMPLL],
		.src = MT_CG_V_AD_MMPLL_CK,
		},
	[MT_CG_MIX_MMPLL_DIV7_EN] = {
		.name = __stringify(MT_CG_MIX_MMPLL_DIV7_EN),
		.cnt = 0,
		.mask = MIX_MMPLL_DIV7_EN_BIT,
		.ops = &wo_clr_en_cg_clk_ops,
		.grp = &grps[CG_MMPLL],
		.src = MT_CG_V_AD_MMPLL_CK, },
#endif				/* CLKMGR_PLL_VCG */
	/* CG_TOP0 */
	[MT_CG_TOP_PDN_SMI] = {
		.name = __stringify(MT_CG_TOP_PDN_SMI),
		.cnt = 0,
		.mask = TOP_PDN_SMI_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP0],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_MFG] = {
		.name = __stringify(MT_CG_TOP_PDN_MFG),
		.cnt = 0,
		.mask = TOP_PDN_MFG_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP0],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_IRDA] = {
		.name = __stringify(MT_CG_TOP_PDN_IRDA),
		.cnt = 0,
		.mask = TOP_PDN_IRDA_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP0],
		.src = MT_CG_V_MUX,
		},
	/* CG_TOP1 */
	[MT_CG_TOP_PDN_CAM] = {
		.name = __stringify(MT_CG_TOP_PDN_CAM),
		.cnt = 0,
		.mask = TOP_PDN_CAM_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP1],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_AUD_INTBUS] = {
		.name = __stringify(MT_CG_TOP_PDN_AUD_INTBUS),
		.cnt = 0,
		.mask = TOP_PDN_AUD_INTBUS_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP1],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_JPG] = {
		.name = __stringify(MT_CG_TOP_PDN_JPG),
		.cnt = 0,
		.mask = TOP_PDN_JPG_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP1],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_DISP] = {
		.name = __stringify(MT_CG_TOP_PDN_DISP),
		.cnt = 0,
		.mask = TOP_PDN_DISP_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP1],
		.src = MT_CG_V_MUX,
		},
	/* CG_TOP2 */
	[MT_CG_TOP_PDN_MSDC30_1] = {
		.name = __stringify(MT_CG_TOP_PDN_MSDC30_1),
		.cnt = 0,
		.mask = TOP_PDN_MSDC30_1_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP2],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_MSDC30_2] = {
		.name = __stringify(MT_CG_TOP_PDN_MSDC30_2),
		.cnt = 0,
		.mask = TOP_PDN_MSDC30_2_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP2],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_MSDC30_3] = {
		.name = __stringify(MT_CG_TOP_PDN_MSDC30_3),
		.cnt = 0,
		.mask = TOP_PDN_MSDC30_3_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP2],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_MSDC30_4] = {
		.name = __stringify(MT_CG_TOP_PDN_MSDC30_4),
		.cnt = 0,
		.mask = TOP_PDN_MSDC30_4_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP2],
		.src = MT_CG_V_MUX,
		},
	/* CG_TOP3 */
	[MT_CG_TOP_PDN_USB20] = {
		.name = __stringify(MT_CG_TOP_PDN_USB20),
		.cnt = 0,
		.mask = TOP_PDN_USB20_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP3],
		.src = MT_CG_V_MUX,
		},
	/* CG_TOP4 */
	[MT_CG_TOP_PDN_VENC] = {
		.name = __stringify(MT_CG_TOP_PDN_VENC),
		.cnt = 0,
		.mask = TOP_PDN_VENC_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP4],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_SPI] = {
		.name = __stringify(MT_CG_TOP_PDN_SPI),
		.cnt = 0,
		.mask = TOP_PDN_SPI_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP4],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_UART] = {
		.name = __stringify(MT_CG_TOP_PDN_UART),
		.cnt = 0,
		.mask = TOP_PDN_UART_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP4],
		.src = MT_CG_V_MUX,
		},
	/* CG_TOP6 */
	[MT_CG_TOP_PDN_MEM] = {
		.name = __stringify(MT_CG_TOP_PDN_MEM),
		.cnt = 0,
		.mask = TOP_PDN_MEM_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP6],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_CAMTG] = {
		.name = __stringify(MT_CG_TOP_PDN_CAMTG),
		.cnt = 0,
		.mask = TOP_PDN_CAMTG_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP6],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_AUDIO] = {
		.name = __stringify(MT_CG_TOP_PDN_AUDIO),
		.cnt = 0,
		.mask = TOP_PDN_AUDIO_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP6],
		.src = MT_CG_V_MUX,
		},
	/* CG_TOP7 */
	[MT_CG_TOP_PDN_FIX] = {
		.name = __stringify(MT_CG_TOP_PDN_FIX),
		.cnt = 0,
		.mask = TOP_PDN_FIX_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP7],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_VDEC] = {
		.name = __stringify(MT_CG_TOP_PDN_VDEC),
		.cnt = 0,
		.mask = TOP_PDN_VDEC_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP7],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_DDRPHYCFG] = {
		.name = __stringify(MT_CG_TOP_PDN_DDRPHYCFG),
		.cnt = 0,
		.mask = TOP_PDN_DDRPHYCFG_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP7],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_DPILVDS] = {
		.name = __stringify(MT_CG_TOP_PDN_DPILVDS),
		.cnt = 0,
		.mask = TOP_PDN_DPILVDS_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP7],
		.src = MT_CG_V_MUX,
		},
	/* CG_TOP8 */
	[MT_CG_TOP_PDN_PMICSPI] = {
		.name = __stringify(MT_CG_TOP_PDN_PMICSPI),
		.cnt = 0,
		.mask = TOP_PDN_PMICSPI_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP8],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_MSDC30_0] = {
		.name = __stringify(MT_CG_TOP_PDN_MSDC30_0),
		.cnt = 0,
		.mask = TOP_PDN_MSDC30_0_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP8],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_SMI_MFG_AS] = {
		.name = __stringify(MT_CG_TOP_PDN_SMI_MFG_AS),
		.cnt = 0,
		.mask = TOP_PDN_SMI_MFG_AS_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP8],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_GCPU] = {
		.name = __stringify(MT_CG_TOP_PDN_GCPU),
		.cnt = 0,
		.mask = TOP_PDN_GCPU_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP8],
		.src = MT_CG_V_MUX,
		},
	/* CG_TOP9 */
	[MT_CG_TOP_PDN_DPI1] = {
		.name = __stringify(MT_CG_TOP_PDN_DPI1),
		.cnt = 0,
		.mask = TOP_PDN_DPI1_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP9],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_CCI] = {
		.name = __stringify(MT_CG_TOP_PDN_CCI),
		.cnt = 0,
		.mask = TOP_PDN_CCI_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP9],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_APLL] = {
		.name = __stringify(MT_CG_TOP_PDN_APLL),
		.cnt = 0,
		.mask = TOP_PDN_APLL_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP9],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_TOP_PDN_HDMIPLL] = {
		.name = __stringify(MT_CG_TOP_PDN_HDMIPLL),
		.cnt = 0,
		.mask = TOP_PDN_HDMIPLL_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_TOP9],
		.src = MT_CG_V_MUX,
		},
	/* CG_INFRA */
	[MT_CG_INFRA_PMIC_WRAP_PDN] = {
		.name = __stringify(MT_CG_INFRA_PMIC_WRAP_PDN),
		.cnt = 0,
		.mask = INFRA_PMIC_WRAP_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_INFRA_PMICSPI_PDN] = {
		.name = __stringify(MT_CG_INFRA_PMICSPI_PDN),
		.cnt = 0,
		.mask = INFRA_PMICSPI_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_TOP_PDN_PMICSPI,
		},
	[MT_CG_INFRA_CCIF1_AP_CTRL_PDN] = {
		.name = __stringify(MT_CG_INFRA_CCIF1_AP_CTRL_PDN),
		.cnt = 0,
		.mask = INFRA_CCIF1_AP_CTRL_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_INFRA_CCIF0_AP_CTRL_PDN] = {
		.name = __stringify(MT_CG_INFRA_CCIF0_AP_CTRL_PDN),
		.cnt = 0,
		.mask = INFRA_CCIF0_AP_CTRL_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_INFRA_KP_PDN] = {
		.name = __stringify(MT_CG_INFRA_KP_PDN),
		.cnt = 0,
		.mask = INFRA_KP_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_INFRA_CPUM_PDN] = {
		.name = __stringify(MT_CG_INFRA_CPUM_PDN),
		.cnt = 0,
		.mask = INFRA_CPUM_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_CPUM_TCK_IN,
		},
	[MT_CG_INFRA_MD2AHB_BUS_PDN] = {
		.name = __stringify(MT_CG_INFRA_MD2AHB_BUS_PDN),
		.cnt = 0,
		.mask = INFRA_MD2AHB_BUS_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_NULL,
		},
	[MT_CG_INFRA_MD2HWMIX_BUS_PDN] = {
		.name = __stringify(MT_CG_INFRA_MD2HWMIX_BUS_PDN),
		.cnt = 0,
		.mask = INFRA_MD2HWMIX_BUS_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_NULL,
		},
	[MT_CG_INFRA_MD2MCU_BUS_PDN] = {
		.name = __stringify(MT_CG_INFRA_MD2MCU_BUS_PDN),
		.cnt = 0,
		.mask = INFRA_MD2MCU_BUS_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_NULL,
		},
	[MT_CG_INFRA_MD1AHB_BUS_PDN] = {
		.name = __stringify(MT_CG_INFRA_MD1AHB_BUS_PDN),
		.cnt = 0,
		.mask = INFRA_MD1AHB_BUS_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_NULL,
		},
	[MT_CG_INFRA_MD1HWMIX_BUS_PDN] = {
		.name = __stringify(MT_CG_INFRA_MD1HWMIX_BUS_PDN),
		.cnt = 0,
		.mask = INFRA_MD1HWMIX_BUS_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_NULL,
		},
	[MT_CG_INFRA_MD1MCU_BUS_PDN] = {
		.name = __stringify(MT_CG_INFRA_MD1MCU_BUS_PDN),
		.cnt = 0,
		.mask = INFRA_MD1MCU_BUS_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_NULL,
		},
	[MT_CG_INFRA_M4U_PDN] = {
		.name = __stringify(MT_CG_INFRA_M4U_PDN),
		.cnt = 0,
		.mask = INFRA_M4U_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_TOP_PDN_MEM,
		},
	[MT_CG_INFRA_MFGAXI_PDN] = {
		.name = __stringify(MT_CG_INFRA_MFGAXI_PDN),
		.cnt = 0,
		.mask = INFRA_MFGAXI_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_INFRA_DEVAPC_PDN] = {
		.name = __stringify(MT_CG_INFRA_DEVAPC_PDN),
		.cnt = 0,
		.mask = INFRA_DEVAPC_PDN_BIT,
		.ops = &general_en_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_AXI_CK, },
#if AUDIO_WORKAROUND == 2
	[MT_CG_INFRA_AUDIO_PDN] = {
		.name = __stringify(MT_CG_INFRA_AUDIO_PDN),
		.cnt = 0,
		.mask = INFRA_AUDIO_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_TOP_PDN_AUDIO, },
#else				/* AUDIO_WORKAROUND != 2 */
	[MT_CG_INFRA_AUDIO_PDN] = {
		.name = __stringify(MT_CG_INFRA_AUDIO_PDN),
		.cnt = 0,
		.mask = INFRA_AUDIO_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_TOP_PDN_AUD_INTBUS, },
#endif				/* AUDIO_WORKAROUND == 2 */
	[MT_CG_INFRA_MFG_BUS_PDN] = {
		.name = __stringify(MT_CG_INFRA_MFG_BUS_PDN),
		.cnt = 0,
		.mask = INFRA_MFG_BUS_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_INFRA_SMI_PDN] = {
		.name = __stringify(MT_CG_INFRA_SMI_PDN),
		.cnt = 0,
		.mask = INFRA_SMI_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_TOP_PDN_SMI,
		},
	[MT_CG_INFRA_DBGCLK_PDN] = {
		.name = __stringify(MT_CG_INFRA_DBGCLK_PDN),
		.cnt = 0,
		.mask = INFRA_DBGCLK_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_INFRA],
		.src = MT_CG_V_AXI_CK,
		},
	/* CG_PERI0 */
	[MT_CG_PERI_I2C5_PDN] = {
		.name = __stringify(MT_CG_PERI_I2C5_PDN),
		.cnt = 0,
		.mask = PERI_I2C5_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_I2C4_PDN] = {
		.name = __stringify(MT_CG_PERI_I2C4_PDN),
		.cnt = 0,
		.mask = PERI_I2C4_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_I2C3_PDN] = {
		.name = __stringify(MT_CG_PERI_I2C3_PDN),
		.cnt = 0,
		.mask = PERI_I2C3_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_I2C2_PDN] = {
		.name = __stringify(MT_CG_PERI_I2C2_PDN),
		.cnt = 0,
		.mask = PERI_I2C2_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_I2C1_PDN] = {
		.name = __stringify(MT_CG_PERI_I2C1_PDN),
		.cnt = 0,
		.mask = PERI_I2C1_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_I2C0_PDN] = {
		.name = __stringify(MT_CG_PERI_I2C0_PDN),
		.cnt = 0,
		.mask = PERI_I2C0_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_UART3_PDN] = {
		.name = __stringify(MT_CG_PERI_UART3_PDN),
		.cnt = 0,
		.mask = PERI_UART3_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_UART2_PDN] = {
		.name = __stringify(MT_CG_PERI_UART2_PDN),
		.cnt = 0,
		.mask = PERI_UART2_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_UART1_PDN] = {
		.name = __stringify(MT_CG_PERI_UART1_PDN),
		.cnt = 0,
		.mask = PERI_UART1_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_UART0_PDN] = {
		.name = __stringify(MT_CG_PERI_UART0_PDN),
		.cnt = 0,
		.mask = PERI_UART0_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_IRDA_PDN] = {
		.name = __stringify(MT_CG_PERI_IRDA_PDN),
		.cnt = 0,
		.mask = PERI_IRDA_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_TOP_PDN_IRDA,
		},
	[MT_CG_PERI_NLI_PDN] = {
		.name = __stringify(MT_CG_PERI_NLI_PDN),
		.cnt = 0,
		.mask = PERI_NLI_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_MD_HIF_PDN] = {
		.name = __stringify(MT_CG_PERI_MD_HIF_PDN),
		.cnt = 0,
		.mask = PERI_MD_HIF_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_AP_HIF_PDN] = {
		.name = __stringify(MT_CG_PERI_AP_HIF_PDN),
		.cnt = 0,
		.mask = PERI_AP_HIF_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_MSDC30_3_PDN] = {
		.name = __stringify(MT_CG_PERI_MSDC30_3_PDN),
		.cnt = 0,
		.mask = PERI_MSDC30_3_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_TOP_PDN_MSDC30_4,
		},
	[MT_CG_PERI_MSDC30_2_PDN] = {
		.name = __stringify(MT_CG_PERI_MSDC30_2_PDN),
		.cnt = 0,
		.mask = PERI_MSDC30_2_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_TOP_PDN_MSDC30_3,
		},
	[MT_CG_PERI_MSDC30_1_PDN] = {
		.name = __stringify(MT_CG_PERI_MSDC30_1_PDN),
		.cnt = 0,
		.mask = PERI_MSDC30_1_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_TOP_PDN_MSDC30_2,
		},
	[MT_CG_PERI_MSDC20_2_PDN] = {
		.name = __stringify(MT_CG_PERI_MSDC20_2_PDN),
		.cnt = 0,
		.mask = PERI_MSDC20_2_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_TOP_PDN_MSDC30_1,
		},
	[MT_CG_PERI_MSDC20_1_PDN] = {
		.name = __stringify(MT_CG_PERI_MSDC20_1_PDN),
		.cnt = 0,
		.mask = PERI_MSDC20_1_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_TOP_PDN_MSDC30_0,
		},
	[MT_CG_PERI_AP_DMA_PDN] = {
		.name = __stringify(MT_CG_PERI_AP_DMA_PDN),
		.cnt = 0,
		.mask = PERI_AP_DMA_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_USB1_PDN] = {
		.name = __stringify(MT_CG_PERI_USB1_PDN),
		.cnt = 0,
		.mask = PERI_USB1_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_TOP_PDN_USB20,
		},
	[MT_CG_PERI_USB0_PDN] = {
		.name = __stringify(MT_CG_PERI_USB0_PDN),
		.cnt = 0,
		.mask = PERI_USB0_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_TOP_PDN_USB20,
		},
	[MT_CG_PERI_PWM_PDN] = {
		.name = __stringify(MT_CG_PERI_PWM_PDN),
		.cnt = 0,
		.mask = PERI_PWM_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_PWM7_PDN] = {
		.name = __stringify(MT_CG_PERI_PWM7_PDN),
		.cnt = 0,
		.mask = PERI_PWM7_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_PWM6_PDN] = {
		.name = __stringify(MT_CG_PERI_PWM6_PDN),
		.cnt = 0,
		.mask = PERI_PWM6_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_PWM5_PDN] = {
		.name = __stringify(MT_CG_PERI_PWM5_PDN),
		.cnt = 0,
		.mask = PERI_PWM5_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_PWM4_PDN] = {
		.name = __stringify(MT_CG_PERI_PWM4_PDN),
		.cnt = 0,
		.mask = PERI_PWM4_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_PWM3_PDN] = {
		.name = __stringify(MT_CG_PERI_PWM3_PDN),
		.cnt = 0,
		.mask = PERI_PWM3_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_PWM2_PDN] = {
		.name = __stringify(MT_CG_PERI_PWM2_PDN),
		.cnt = 0,
		.mask = PERI_PWM2_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_PWM1_PDN] = {
		.name = __stringify(MT_CG_PERI_PWM1_PDN),
		.cnt = 0,
		.mask = PERI_PWM1_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_THERM_PDN] = {
		.name = __stringify(MT_CG_PERI_THERM_PDN),
		.cnt = 0,
		.mask = PERI_THERM_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_NFI_PDN] = {
		.name = __stringify(MT_CG_PERI_NFI_PDN),
		.cnt = 0,
		.mask = PERI_NFI_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI0],
		.src = MT_CG_V_AXI_CK,
		},
	/* CG_PERI1 */
	[MT_CG_PERI_USBSLV_PDN] = {
		.name = __stringify(MT_CG_PERI_USBSLV_PDN),
		.cnt = 0,
		.mask = PERI_USBSLV_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI1],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_USB1_MCU_PDN] = {
		.name = __stringify(MT_CG_PERI_USB1_MCU_PDN),
		.cnt = 0,
		.mask = PERI_USB1_MCU_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI1],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_USB0_MCU_PDN] = {
		.name = __stringify(MT_CG_PERI_USB0_MCU_PDN),
		.cnt = 0,
		.mask = PERI_USB0_MCU_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI1],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_GCPU_PDN] = {
		.name = __stringify(MT_CG_PERI_GCPU_PDN),
		.cnt = 0,
		.mask = PERI_GCPU_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI1],
		.src = MT_CG_TOP_PDN_GCPU,
		},
	[MT_CG_PERI_FHCTL_PDN] = {
		.name = __stringify(MT_CG_PERI_FHCTL_PDN),
		.cnt = 0,
		.mask = PERI_FHCTL_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI1],
		.src = MT_CG_V_26M_CK,
		},
	[MT_CG_PERI_SPI1_PDN] = {
		.name = __stringify(MT_CG_PERI_SPI1_PDN),
		.cnt = 0,
		.mask = PERI_SPI1_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI1],
		.src = MT_CG_TOP_PDN_SPI,
		},
	[MT_CG_PERI_AUXADC_PDN] = {
		.name = __stringify(MT_CG_PERI_AUXADC_PDN),
		.cnt = 0,
		.mask = PERI_AUXADC_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI1],
		.src = MT_CG_V_26M_CK,
		},
	[MT_CG_PERI_PERI_PWRAP_PDN] = {
		.name = __stringify(MT_CG_PERI_PERI_PWRAP_PDN),
		.cnt = 0,
		.mask = PERI_PERI_PWRAP_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI1],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_PERI_I2C6_PDN] = {
		.name = __stringify(MT_CG_PERI_I2C6_PDN),
		.cnt = 0,
		.mask = PERI_I2C6_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_PERI1],
		.src = MT_CG_V_AXI_CK,
		},
	/* CG_MFG */
	[MT_CG_MFG_BAXI_PDN] = {
		.name = __stringify(MT_CG_MFG_BAXI_PDN),
		.cnt = 0,
		.mask = MFG_BAXI_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_MFG],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_MFG_BMEM_PDN] = {
		.name = __stringify(MT_CG_MFG_BMEM_PDN),
		.cnt = 0,
		.mask = MFG_BMEM_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_MFG],
		.src = MT_CG_TOP_PDN_SMI_MFG_AS,
		},
	[MT_CG_MFG_BG3D_PDN] = {
		.name = __stringify(MT_CG_MFG_BG3D_PDN),
		.cnt = 0,
		.mask = MFG_BG3D_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_MFG],
		.src = MT_CG_TOP_PDN_MFG,
		},
	[MT_CG_MFG_B26M_PDN] = {
		.name = __stringify(MT_CG_MFG_B26M_PDN),
		.cnt = 0,
		.mask = MFG_B26M_PDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_MFG],
		.src = MT_CG_V_26M_CK,
		},
	/* CG_ISP */
	[MT_CG_ISP_FPC_CKPDN] = {
		.name = __stringify(MT_CG_ISP_FPC_CKPDN),
		.cnt = 0,
		.mask = ISP_FPC_CKPDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_ISP],
		.src = MT_CG_V_SMI_DISP_CK,
		},
	[MT_CG_ISP_JPGENC_JPG_CKPDN] = {
		.name = __stringify(MT_CG_ISP_JPGENC_JPG_CKPDN),
		.cnt = 0,
		.mask = ISP_JPGENC_JPG_CKPDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_ISP],
		.src = MT_CG_V_JPEG_CK,
		},
	[MT_CG_ISP_JPGENC_SMI_CKPDN] = {
		.name = __stringify(MT_CG_ISP_JPGENC_SMI_CKPDN),
		.cnt = 0,
		.mask = ISP_JPGENC_SMI_CKPDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_ISP],
		.src = MT_CG_V_SMI_DISP_CK,
		},
	[MT_CG_ISP_JPGDEC_JPG_CKPDN] = {
		.name = __stringify(MT_CG_ISP_JPGDEC_JPG_CKPDN),
		.cnt = 0,
		.mask = ISP_JPGDEC_JPG_CKPDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_ISP],
		.src = MT_CG_V_JPEG_CK,
		},
	[MT_CG_ISP_JPGDEC_SMI_CKPDN] = {
		.name = __stringify(MT_CG_ISP_JPGDEC_SMI_CKPDN),
		.cnt = 0,
		.mask = ISP_JPGDEC_SMI_CKPDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_ISP],
		.src = MT_CG_V_SMI_DISP_CK,
		},
	[MT_CG_ISP_SEN_CAM_CKPDN] = {
		.name = __stringify(MT_CG_ISP_SEN_CAM_CKPDN),
		.cnt = 0,
		.mask = ISP_SEN_CAM_CKPDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_ISP],
		.src = MT_CG_TOP_PDN_CAM,
		},
	[MT_CG_ISP_SEN_TG_CKPDN] = {
		.name = __stringify(MT_CG_ISP_SEN_TG_CKPDN),
		.cnt = 0,
		.mask = ISP_SEN_TG_CKPDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_ISP],
		.src = MT_CG_TOP_PDN_CAMTG,
		},
	[MT_CG_ISP_CAM_CAM_CKPDN] = {
		.name = __stringify(MT_CG_ISP_CAM_CAM_CKPDN),
		.cnt = 0,
		.mask = ISP_CAM_CAM_CKPDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_ISP],
		.src = MT_CG_TOP_PDN_CAM,
		},
	[MT_CG_ISP_CAM_SMI_CKPDN] = {
		.name = __stringify(MT_CG_ISP_CAM_SMI_CKPDN),
		.cnt = 0,
		.mask = ISP_CAM_SMI_CKPDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_ISP],
		.src = MT_CG_V_SMI_DISP_CK,
		},
	[MT_CG_ISP_COMM_SMI_CKPDN] = {
		.name = __stringify(MT_CG_ISP_COMM_SMI_CKPDN),
		.cnt = 0,
		.mask = ISP_COMM_SMI_CKPDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_ISP],
		.src = MT_CG_V_SMI_DISP_CK,
		},
	[MT_CG_ISP_LARB4_SMI_CKPDN] = {
		.name = __stringify(MT_CG_ISP_LARB4_SMI_CKPDN),
		.cnt = 0,
		.mask = ISP_LARB4_SMI_CKPDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_ISP],
		.src = MT_CG_V_SMI_DISP_CK,
		},
	[MT_CG_ISP_LARB3_SMI_CKPDN] = {
		.name = __stringify(MT_CG_ISP_LARB3_SMI_CKPDN),
		.cnt = 0,
		.mask = ISP_LARB3_SMI_CKPDN_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_ISP],
		.src = MT_CG_V_SMI_DISP_CK,
		},
	/* CG_VENC */
	[MT_CG_VENC_CKE] = {
		.name = __stringify(MT_CG_VENC_CKE),
		.cnt = 0,
		.mask = VENC_CKE_BIT,
		.ops = &general_en_cg_clk_ops,
		.grp = &grps[CG_VENC],
		.src = MT_CG_TOP_PDN_VENC,
		},
	/* CG_VDEC0 */
	[MT_CG_VDEC_CKEN] = {
		.name = __stringify(MT_CG_VDEC_CKEN),
		.cnt = 0,
		.mask = VDEC_CKEN_BIT,
		.ops = &general_en_cg_clk_ops,
		.grp = &grps[CG_VDEC0],
		.src = MT_CG_TOP_PDN_VDEC,
		},
	/* CG_VDEC1 */
	[MT_CG_VDEC_LARB_CKEN] = {
		.name = __stringify(MT_CG_VDEC_LARB_CKEN),
		.cnt = 0,
		.mask = VDEC_LARB_CKEN_BIT,
		.ops = &general_en_cg_clk_ops,
		.grp = &grps[CG_VDEC1],
		.src = MT_CG_TOP_PDN_SMI,
		},
	/* CG_DISP0 */
	[MT_CG_DISP_SMI_LARB2] = {
		.name = __stringify(MT_CG_DISP_SMI_LARB2),
		.cnt = 0,
		.mask = DISP_SMI_LARB2_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_SMI,
		},
	[MT_CG_DISP_ROT_DISP] = {
		.name = __stringify(MT_CG_DISP_ROT_DISP),
		.cnt = 0,
		.mask = DISP_ROT_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_ROT_SMI] = {
		.name = __stringify(MT_CG_DISP_ROT_SMI),
		.cnt = 0,
		.mask = DISP_ROT_SMI_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_SMI,
		},
	[MT_CG_DISP_SCL_DISP] = {
		.name = __stringify(MT_CG_DISP_SCL_DISP),
		.cnt = 0,
		.mask = DISP_SCL_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_OVL_DISP] = {
		.name = __stringify(MT_CG_DISP_OVL_DISP),
		.cnt = 0,
		.mask = DISP_OVL_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_OVL_SMI] = {
		.name = __stringify(MT_CG_DISP_OVL_SMI),
		.cnt = 0,
		.mask = DISP_OVL_SMI_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_SMI,
		},
	[MT_CG_DISP_COLOR_DISP] = {
		.name = __stringify(MT_CG_DISP_COLOR_DISP),
		.cnt = 0,
		.mask = DISP_COLOR_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_TDSHP_DISP] = {
		.name = __stringify(MT_CG_DISP_TDSHP_DISP),
		.cnt = 0,
		.mask = DISP_TDSHP_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_BLS_DISP] = {
		.name = __stringify(MT_CG_DISP_BLS_DISP),
		.cnt = 0,
		.mask = DISP_BLS_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_WDMA0_DISP] = {
		.name = __stringify(MT_CG_DISP_WDMA0_DISP),
		.cnt = 0,
		.mask = DISP_WDMA0_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_WDMA0_SMI] = {
		.name = __stringify(MT_CG_DISP_WDMA0_SMI),
		.cnt = 0,
		.mask = DISP_WDMA0_SMI_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_SMI,
		},
	[MT_CG_DISP_WDMA1_DISP] = {
		.name = __stringify(MT_CG_DISP_WDMA1_DISP),
		.cnt = 0,
		.mask = DISP_WDMA1_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_WDMA1_SMI] = {
		.name = __stringify(MT_CG_DISP_WDMA1_SMI),
		.cnt = 0,
		.mask = DISP_WDMA1_SMI_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_SMI,
		},
	[MT_CG_DISP_RDMA0_DISP] = {
		.name = __stringify(MT_CG_DISP_RDMA0_DISP),
		.cnt = 0,
		.mask = DISP_RDMA0_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_RDMA0_SMI] = {
		.name = __stringify(MT_CG_DISP_RDMA0_SMI),
		.cnt = 0,
		.mask = DISP_RDMA0_SMI_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_DISP_RDMA0_OUTPUT] = {
		.name = __stringify(MT_CG_DISP_RDMA0_OUTPUT),
		.cnt = 0,
		.mask = DISP_RDMA0_OUTPUT_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_DISP_RDMA1_DISP] = {
		.name = __stringify(MT_CG_DISP_RDMA1_DISP),
		.cnt = 0,
		.mask = DISP_RDMA1_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_RDMA1_SMI] = {
		.name = __stringify(MT_CG_DISP_RDMA1_SMI),
		.cnt = 0,
		.mask = DISP_RDMA1_SMI_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_DISP_RDMA1_OUTPUT] = {
		.name = __stringify(MT_CG_DISP_RDMA1_OUTPUT),
		.cnt = 0,
		.mask = DISP_RDMA1_OUTPUT_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_DISP_GAMMA_DISP] = {
		.name = __stringify(MT_CG_DISP_GAMMA_DISP),
		.cnt = 0,
		.mask = DISP_GAMMA_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_GAMMA_PIXEL] = {
		.name = __stringify(MT_CG_DISP_GAMMA_PIXEL),
		.cnt = 0,
		.mask = DISP_GAMMA_PIXEL_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_DISP_CMDQ_DISP] = {
		.name = __stringify(MT_CG_DISP_CMDQ_DISP),
		.cnt = 0,
		.mask = DISP_CMDQ_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_CMDQ_SMI] = {
		.name = __stringify(MT_CG_DISP_CMDQ_SMI),
		.cnt = 0,
		.mask = DISP_CMDQ_SMI_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_SMI,
		},
	[MT_CG_DISP_G2D_DISP] = {
		.name = __stringify(MT_CG_DISP_G2D_DISP),
		.cnt = 0,
		.mask = DISP_G2D_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_G2D_SMI] = {
		.name = __stringify(MT_CG_DISP_G2D_SMI),
		.cnt = 0,
		.mask = DISP_G2D_SMI_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP0],
		.src = MT_CG_TOP_PDN_SMI,
		},
	/* CG_DISP1 */
	[MT_CG_DISP_DSI_DISP] = {
		.name = __stringify(MT_CG_DISP_DSI_DISP),
		.cnt = 0,
		.mask = DISP_DSI_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP1],
		.src = MT_CG_TOP_PDN_DISP,
		},
	[MT_CG_DISP_DSI_DSI] = {
		.name = __stringify(MT_CG_DISP_DSI_DSI),
		.cnt = 0,
		.mask = DISP_DSI_DSI_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP1],
		.src = MT_CG_V_AD_DSI0_LNTC_DSICLK,
		},
	[MT_CG_DISP_DSI_DIV2_DSI] = {
		.name = __stringify(MT_CG_DISP_DSI_DIV2_DSI),
		.cnt = 0,
		.mask = DISP_DSI_DIV2_DSI_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP1],
		.src = MT_CG_V_AD_DSI0_LNTC_DSICLK,
		},
	[MT_CG_DISP_DPI1] = {
		.name = __stringify(MT_CG_DISP_DPI1),
		.cnt = 0,
		.mask = DISP_DPI1_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP1],
		.src = MT_CG_TOP_PDN_DPI1,
		},
	[MT_CG_DISP_LVDS_DISP] = {
		.name = __stringify(MT_CG_DISP_LVDS_DISP),
		.cnt = 0,
		.mask = DISP_LVDS_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP1],
		.src = MT_CG_V_AD_VPLL_DPIX_CK,
		},
	[MT_CG_DISP_LVDS_CTS] = {
		.name = __stringify(MT_CG_DISP_LVDS_CTS),
		.cnt = 0,
		.mask = DISP_LVDS_CTS_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP1],
		.src = MT_CG_V_AD_LVDSTX_CLKDIG_CTS,
		},
	[MT_CG_DISP_HDMI_DISP] = {
		.name = __stringify(MT_CG_DISP_HDMI_DISP),
		.cnt = 0,
		.mask = DISP_HDMI_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP1],
		.src = MT_CG_TOP_PDN_DPI1,
		},
	[MT_CG_DISP_HDMI_PLL] = {
		.name = __stringify(MT_CG_DISP_HDMI_PLL),
		.cnt = 0,
		.mask = DISP_HDMI_PLL_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP1],
		.src = MT_CG_TOP_PDN_HDMIPLL,
		},
	[MT_CG_DISP_HDMI_AUDIO] = {
		.name = __stringify(MT_CG_DISP_HDMI_AUDIO),
		.cnt = 0,
		.mask = DISP_HDMI_AUDIO_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP1],
		.src = MT_CG_TOP_PDN_APLL,
		},
	[MT_CG_DISP_HDMI_SPDIF] = {
		.name = __stringify(MT_CG_DISP_HDMI_SPDIF),
		.cnt = 0,
		.mask = DISP_HDMI_SPDIF_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP1],
		.src = MT_CG_TOP_PDN_APLL,
		},
	[MT_CG_DISP_MUTEX_26M] = {
		.name = __stringify(MT_CG_DISP_MUTEX_26M),
		.cnt = 0,
		.mask = DISP_MUTEX_26M_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP1],
		.src = MT_CG_V_26M_CK,
		},
	[MT_CG_DISP_UFO_DISP] = {
		.name = __stringify(MT_CG_DISP_UFO_DISP),
		.cnt = 0,
		.mask = DISP_UFO_DISP_BIT,
		.ops = &general_gate_cg_clk_ops,
		.grp = &grps[CG_DISP1],
		.src = MT_CG_TOP_PDN_DISP, },

#if AUDIO_WORKAROUND == 1
	/* CG_AUDIO, workaround 1 */
	[MT_CG_AUDIO_PDN_AFE] = {
		.name = __stringify(MT_CG_AUDIO_PDN_AFE),
		.cnt = 0,
		.mask = AUDIO_PDN_AFE_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_AUDIO],
		.src = MT_CG_INFRA_AUDIO_PDN,
		},
	[MT_CG_AUDIO_PDN_I2S] = {
		.name = __stringify(MT_CG_AUDIO_PDN_I2S),
		.cnt = 0,
		.mask = AUDIO_PDN_I2S_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_AUDIO],
		.src = MT_CG_INFRA_AUDIO_PDN,
		},
	[MT_CG_AUDIO_PDN_APLL_TUNER] = {
		.name = __stringify(MT_CG_AUDIO_PDN_APLL_TUNER),
		.cnt = 0,
		.mask = AUDIO_PDN_APLL_TUNER_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_AUDIO],
		.src = MT_CG_TOP_PDN_APLL,
		},
	[MT_CG_AUDIO_PDN_HDMI_CK] = {
		.name = __stringify(MT_CG_AUDIO_PDN_HDMI_CK),
		.cnt = 0,
		.mask = AUDIO_PDN_HDMI_CK_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_AUDIO],
		.src = MT_CG_TOP_PDN_APLL,
		},
	[MT_CG_AUDIO_PDN_SPDF_CK] = {
		.name = __stringify(MT_CG_AUDIO_PDN_SPDF_CK),
		.cnt = 0,
		.mask = AUDIO_PDN_SPDF_CK_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_AUDIO],
		.src = MT_CG_TOP_PDN_APLL, },
#elif AUDIO_WORKAROUND == 2
	/* CG_AUDIO, workaround 1 */
	[MT_CG_AUDIO_PDN_AFE] = {
		.name = __stringify(MT_CG_AUDIO_PDN_AFE),
		.cnt = 0,
		.mask = AUDIO_PDN_AFE_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_AUDIO],
		.src = MT_CG_TOP_PDN_AUD_INTBUS,
		},
	[MT_CG_AUDIO_PDN_I2S] = {
		.name = __stringify(MT_CG_AUDIO_PDN_I2S),
		.cnt = 0,
		.mask = AUDIO_PDN_I2S_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_AUDIO],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_AUDIO_PDN_APLL_TUNER] = {
		.name = __stringify(MT_CG_AUDIO_PDN_APLL_TUNER),
		.cnt = 0,
		.mask = AUDIO_PDN_APLL_TUNER_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_AUDIO],
		.src = MT_CG_TOP_PDN_APLL,
		},
	[MT_CG_AUDIO_PDN_HDMI_CK] = {
		.name = __stringify(MT_CG_AUDIO_PDN_HDMI_CK),
		.cnt = 0,
		.mask = AUDIO_PDN_HDMI_CK_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_AUDIO],
		.src = MT_CG_TOP_PDN_APLL,
		},
	[MT_CG_AUDIO_PDN_SPDF_CK] = {
		.name = __stringify(MT_CG_AUDIO_PDN_SPDF_CK),
		.cnt = 0,
		.mask = AUDIO_PDN_SPDF_CK_BIT,
		.ops = &wo_clr_gate_cg_clk_ops,
		.grp = &grps[CG_AUDIO],
		.src = MT_CG_TOP_PDN_APLL, },
#else				/* AUDIO_WORKAROUND == ? */
#error
#endif				/* AUDIO_WORKAROUND */

	/* Virtual CGs */
	[MT_CG_V_AD_AUDPLL_CK] = {
		.name = __stringify(MT_CG_V_AD_AUDPLL_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_PLL,
		.parent = &plls[AUDPLL]},
	[MT_CG_V_AD_LVDSPLL_CK] = {
		.name = __stringify(MT_CG_V_AD_LVDSPLL_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_PLL,
		.parent = &plls[LVDSPLL]},
	[MT_CG_V_AD_MAINPLL_CK] = {
		.name = __stringify(MT_CG_V_AD_MAINPLL_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_PLL,
		.parent = &plls[MAINPLL]},
	[MT_CG_V_AD_MMPLL_CK] = {
		.name = __stringify(MT_CG_V_AD_MMPLL_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_PLL,
		.parent = &plls[MMPLL]},
	[MT_CG_V_AD_MSDCPLL_CK] = {
		.name = __stringify(MT_CG_V_AD_MSDCPLL_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_PLL,
		.parent = &plls[MSDCPLL]},
	[MT_CG_V_AD_TVDPLL_CK] = {
		.name = __stringify(MT_CG_V_AD_TVDPLL_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_PLL,
		.parent = &plls[TVDPLL]},
	[MT_CG_V_AD_UNIVPLL_CK] = {
		.name = __stringify(MT_CG_V_AD_UNIVPLL_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_PLL,
		.parent = &plls[UNIVPLL]},
	[MT_CG_V_AD_VDECPLL_CK] = {
		.name = __stringify(MT_CG_V_AD_VDECPLL_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_PLL,
		.parent = &plls[VDECPLL]},
	[MT_CG_V_AXI_CK] = {
		.name = __stringify(MT_CG_V_AXI_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_V_DPI0_CK] = {
		.name = __stringify(MT_CG_V_DPI0_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_V_MIPI26M_CK] = {
		.name = __stringify(MT_CG_V_MIPI26M_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_MUX,
		},
	[MT_CG_V_26M_CK] = {
		.name = __stringify(MT_CG_V_26M_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_V_AD_DSI0_LNTC_DSICLK] = {
		.name = __stringify(MT_CG_V_AD_DSI0_LNTC_DSICLK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_V_AD_HDMITX_CLKDIG_CTS] = {
		.name = __stringify(MT_CG_V_AD_HDMITX_CLKDIG_CTS),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_V_AD_MEM2MIPI_26M_CK] = {
		.name = __stringify(MT_CG_V_AD_MEM2MIPI_26M_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_V_AD_MIPI_26M_CK] = {
		.name = __stringify(MT_CG_V_AD_MIPI_26M_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_V_CLKPH_MCK] = {
		.name = __stringify(MT_CG_V_CLKPH_MCK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_V_CPUM_TCK_IN] = {
		.name = __stringify(MT_CG_V_CPUM_TCK_IN),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_V_PAD_RTC32K_CK] = {
		.name = __stringify(MT_CG_V_PAD_RTC32K_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_CLOCK_ON,
		},
	[MT_CG_V_AD_APLL_CK] = {
		.name = __stringify(MT_CG_V_AD_APLL_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_AUDPLL_CK,
		},
	[MT_CG_V_APLL_D16] = {
		.name = __stringify(MT_CG_V_APLL_D16),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_AUDPLL_CK,
		},
	[MT_CG_V_APLL_D24] = {
		.name = __stringify(MT_CG_V_APLL_D24),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_AUDPLL_CK,
		},
	[MT_CG_V_APLL_D4] = {
		.name = __stringify(MT_CG_V_APLL_D4),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_AUDPLL_CK,
		},
	[MT_CG_V_APLL_D8] = {
		.name = __stringify(MT_CG_V_APLL_D8),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_AUDPLL_CK,
		},
	[MT_CG_V_LVDSPLL_CLK] = {
		.name = __stringify(MT_CG_V_LVDSPLL_CLK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_LVDSPLL_CK,
		},
	[MT_CG_V_LVDSPLL_D2] = {
		.name = __stringify(MT_CG_V_LVDSPLL_D2),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_LVDSPLL_CK,
		},
	[MT_CG_V_LVDSPLL_D4] = {
		.name = __stringify(MT_CG_V_LVDSPLL_D4),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_LVDSPLL_CK,
		},
	[MT_CG_V_LVDSPLL_D8] = {
		.name = __stringify(MT_CG_V_LVDSPLL_D8),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_LVDSPLL_CK,
		},
	[MT_CG_V_AD_LVDSTX_CLKDIG_CTS] = {
		.name = __stringify(MT_CG_V_AD_LVDSTX_CLKDIG_CTS),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_LVDSPLL_CK,
		},
	[MT_CG_V_AD_VPLL_DPIX_CK] = {
		.name = __stringify(MT_CG_V_AD_VPLL_DPIX_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_LVDSPLL_CK,
		},
	[MT_CG_V_AD_TVHDMI_H_CK] = {
		.name = __stringify(MT_CG_V_AD_TVHDMI_H_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_TVDPLL_CK,
		},
	[MT_CG_V_HDMITX_CLKDIG_D2] = {
		.name = __stringify(MT_CG_V_HDMITX_CLKDIG_D2),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_HDMITX_CLKDIG_CTS,
		},
	[MT_CG_V_HDMITX_CLKDIG_D3] = {
		.name = __stringify(MT_CG_V_HDMITX_CLKDIG_D3),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_HDMITX_CLKDIG_CTS,
		},
	[MT_CG_V_TVHDMI_D2] = {
		.name = __stringify(MT_CG_V_TVHDMI_D2),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_TVHDMI_H_CK,
		},
	[MT_CG_V_TVHDMI_D4] = {
		.name = __stringify(MT_CG_V_TVHDMI_D4),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AD_TVHDMI_H_CK,
		},
	[MT_CG_V_CKSQ_MUX_CK] = {
		.name = __stringify(MT_CG_V_CKSQ_MUX_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_26M_CK,
		},
	[MT_CG_V_MEMPLL_MCK_D4] = {
		.name = __stringify(MT_CG_V_MEMPLL_MCK_D4),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_CLKPH_MCK,
		},
	[MT_CG_V_RTC_CK] = {
		.name = __stringify(MT_CG_V_RTC_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_PAD_RTC32K_CK,
		},
	[MT_CG_V_AXI_DDRPHYCFG_AS_CK] = {
		.name = __stringify(MT_CG_V_AXI_DDRPHYCFG_AS_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_AXI_CK,
		},
	[MT_CG_V_JPEG_CK] = {
		.name = __stringify(MT_CG_V_JPEG_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_TOP_PDN_JPG,
		},
	[MT_CG_V_MMPLL_D2] = {
		.name = __stringify(MT_CG_V_MMPLL_D2),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MMPLL_DIV2_EN,
		},
	[MT_CG_V_MMPLL_D3] = {
		.name = __stringify(MT_CG_V_MMPLL_D3),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MMPLL_DIV3_EN,
		},
	[MT_CG_V_MMPLL_D4] = {
		.name = __stringify(MT_CG_V_MMPLL_D4),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MMPLL_DIV2_EN,
		},
	[MT_CG_V_MMPLL_D5] = {
		.name = __stringify(MT_CG_V_MMPLL_D5),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MMPLL_DIV5_EN,
		},
	[MT_CG_V_MMPLL_D6] = {
		.name = __stringify(MT_CG_V_MMPLL_D6),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MMPLL_DIV3_EN,
		},
	[MT_CG_V_MMPLL_D7] = {
		.name = __stringify(MT_CG_V_MMPLL_D7),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MMPLL_DIV7_EN,
		},
	[MT_CG_V_SMI_DISP_CK] = {
		.name = __stringify(MT_CG_V_SMI_DISP_CK),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_TOP_PDN_SMI,
		},
	[MT_CG_V_SYSPLL_D10] = {
		.name = __stringify(MT_CG_V_SYSPLL_D10),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MAINPLL_806M_EN,
		},
	[MT_CG_V_SYSPLL_D12] = {
		.name = __stringify(MT_CG_V_SYSPLL_D12),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MAINPLL_806M_EN,
		},
	[MT_CG_V_SYSPLL_D16] = {
		.name = __stringify(MT_CG_V_SYSPLL_D16),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MAINPLL_806M_EN,
		},
	[MT_CG_V_SYSPLL_D2] = {
		.name = __stringify(MT_CG_V_SYSPLL_D2),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MAINPLL_806M_EN,
		},
	[MT_CG_V_SYSPLL_D24] = {
		.name = __stringify(MT_CG_V_SYSPLL_D24),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MAINPLL_806M_EN,
		},
	[MT_CG_V_SYSPLL_D2P5] = {
		.name = __stringify(MT_CG_V_SYSPLL_D2P5),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MAINPLL_322P4M_EN,
		},
	[MT_CG_V_SYSPLL_D3] = {
		.name = __stringify(MT_CG_V_SYSPLL_D3),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MAINPLL_537P3M_EN,
		},
	[MT_CG_V_SYSPLL_D3P5] = {
		.name = __stringify(MT_CG_V_SYSPLL_D3P5),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MAINPLL_230P3M_EN,
		},
	[MT_CG_V_SYSPLL_D4] = {
		.name = __stringify(MT_CG_V_SYSPLL_D4),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MAINPLL_806M_EN,
		},
	[MT_CG_V_SYSPLL_D5] = {
		.name = __stringify(MT_CG_V_SYSPLL_D5),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MAINPLL_322P4M_EN,
		},
	[MT_CG_V_SYSPLL_D6] = {
		.name = __stringify(MT_CG_V_SYSPLL_D6),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MAINPLL_806M_EN,
		},
	[MT_CG_V_SYSPLL_D8] = {
		.name = __stringify(MT_CG_V_SYSPLL_D8),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_MAINPLL_806M_EN,
		},
	[MT_CG_V_UNIVPLL1_D10] = {
		.name = __stringify(MT_CG_V_UNIVPLL1_D10),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_624M_EN,
		},
	[MT_CG_V_UNIVPLL1_D2] = {
		.name = __stringify(MT_CG_V_UNIVPLL1_D2),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_624M_EN,
		},
	[MT_CG_V_UNIVPLL1_D4] = {
		.name = __stringify(MT_CG_V_UNIVPLL1_D4),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_624M_EN,
		},
	[MT_CG_V_UNIVPLL1_D6] = {
		.name = __stringify(MT_CG_V_UNIVPLL1_D6),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_624M_EN,
		},
	[MT_CG_V_UNIVPLL1_D8] = {
		.name = __stringify(MT_CG_V_UNIVPLL1_D8),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_624M_EN,
		},
	[MT_CG_V_UNIVPLL2_D2] = {
		.name = __stringify(MT_CG_V_UNIVPLL2_D2),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_416M_EN,
		},
	[MT_CG_V_UNIVPLL2_D4] = {
		.name = __stringify(MT_CG_V_UNIVPLL2_D4),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_416M_EN,
		},
	[MT_CG_V_UNIVPLL2_D6] = {
		.name = __stringify(MT_CG_V_UNIVPLL2_D6),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_416M_EN,
		},
	[MT_CG_V_UNIVPLL2_D8] = {
		.name = __stringify(MT_CG_V_UNIVPLL2_D8),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_416M_EN,
		},
	[MT_CG_V_UNIVPLL_D10] = {
		.name = __stringify(MT_CG_V_UNIVPLL_D10),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_249P6M_EN,
		},
	[MT_CG_V_UNIVPLL_D26] = {
		.name = __stringify(MT_CG_V_UNIVPLL_D26),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_48M_EN,
		},
	[MT_CG_V_UNIVPLL_D3] = {
		.name = __stringify(MT_CG_V_UNIVPLL_D3),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_416M_EN,
		},
	[MT_CG_V_UNIVPLL_D5] = {
		.name = __stringify(MT_CG_V_UNIVPLL_D5),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_249P6M_EN,
		},
	[MT_CG_V_UNIVPLL_D7] = {
		.name = __stringify(MT_CG_V_UNIVPLL_D7),
		.cnt = 0,
		.mask = 0,
		.ops = &virtual_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_MIX_UNIVPLL_178P3M_EN, },

	/* Special CGs */
	[MT_CG_V_NULL] = {
		.name = __stringify(MT_CG_V_NULL),
		.cnt = 0,
		.mask = 0,
		.ops = &null_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_NULL,
		},
	[MT_CG_V_CLOCK_ON] = {
		.name = __stringify(MT_CG_V_CLOCK_ON),
		.cnt = 0,
		.mask = 0,
		.ops = &always_on_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_NULL,
		},
	[MT_CG_V_CLOCK_OFF] = {
		.name = __stringify(MT_CG_V_CLOCK_OFF),
		.cnt = 0,
		.mask = 0,
		.ops = &always_off_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_NULL,
		},
	[MT_CG_V_PLL] = {
		.name = __stringify(MT_CG_V_PLL),
		.cnt = 0,
		.mask = 0,
		.ops = &null_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_NULL,
		},
	[MT_CG_V_MUX] = {
		.name = __stringify(MT_CG_V_MUX),
		.cnt = 0,
		.mask = 0,
		.ops = &null_cg_clk_ops,
		.grp = &grps[CG_VIRTUAL],
		.src = MT_CG_V_NULL, },
};


static struct cg_clk *id_to_clk(enum cg_clk_id id)
{
	return id < NR_CLKS ? &clks[id] : NULL;
}

/* general_cg_clk_ops */

static int general_cg_clk_check_validity_op(struct cg_clk *clk)
{
	int valid = 0;

	ENTER_FUNC(FUNC_LV_OP);

	if (clk->mask & clk->grp->mask)
		valid = 1;

	EXIT_FUNC(FUNC_LV_OP);
	return valid;
}

/* general_gate_cg_clk_ops */

static int general_gate_cg_clk_get_state_op(struct cg_clk *clk)
{
	struct subsys *sys = clk->grp->sys;

	ENTER_FUNC(FUNC_LV_OP);

	EXIT_FUNC(FUNC_LV_OP);

	if (sys && !sys->state)
		return PWR_DOWN;
	else
		return (clk_readl(clk->grp->sta_addr) & (clk->mask)) ? PWR_DOWN : PWR_ON;	/* clock gate */
}

static int general_gate_cg_clk_enable_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);

	clk_writel(clk->grp->clr_addr, clk->mask);	/* clock gate */

	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}

static int general_gate_cg_clk_disable_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);

	clk_writel(clk->grp->set_addr, clk->mask);	/* clock gate */

	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}

static struct cg_clk_ops general_gate_cg_clk_ops = {
	.get_state = general_gate_cg_clk_get_state_op,
	.check_validity = general_cg_clk_check_validity_op,
	.enable = general_gate_cg_clk_enable_op,
	.disable = general_gate_cg_clk_disable_op,
};

/* general_en_cg_clk_ops */

static int general_en_cg_clk_get_state_op(struct cg_clk *clk)
{
	struct subsys *sys = clk->grp->sys;

	ENTER_FUNC(FUNC_LV_OP);

	EXIT_FUNC(FUNC_LV_OP);

	if (sys && !sys->state)
		return PWR_DOWN;
	else
		return (clk_readl(clk->grp->sta_addr) & (clk->mask)) ? PWR_ON : PWR_DOWN;	/* clock enable */
}

static int general_en_cg_clk_enable_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);

	clk_writel(clk->grp->set_addr, clk->mask);	/* clock enable */

	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}

static int general_en_cg_clk_disable_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);

	clk_writel(clk->grp->clr_addr, clk->mask);	/* clock enable */

	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}

static struct cg_clk_ops general_en_cg_clk_ops = {
	.get_state = general_en_cg_clk_get_state_op,
	.check_validity = general_cg_clk_check_validity_op,
	.enable = general_en_cg_clk_enable_op,
	.disable = general_en_cg_clk_disable_op,
};


/* ao_clk_ops */

static int ao_cg_clk_get_state_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return PWR_ON;
}

static int ao_cg_clk_check_validity_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return 1;
}

static int ao_cg_clk_enable_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}

static int ao_cg_clk_disable_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}

static struct cg_clk_ops ao_cg_clk_ops = {
	.get_state = ao_cg_clk_get_state_op,
	.check_validity = ao_cg_clk_check_validity_op,
	.enable = ao_cg_clk_enable_op,
	.disable = ao_cg_clk_disable_op,
};


/* always_off_cg_clk_ops */

static int always_off_cg_clk_get_state_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return PWR_DOWN;
}

static int valid_cg_clk_check_validity_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return 1;
}

static int noop_cg_clk_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}

static struct cg_clk_ops always_off_cg_clk_ops = {
	.get_state = always_off_cg_clk_get_state_op,
	.check_validity = valid_cg_clk_check_validity_op,
	.enable = noop_cg_clk_op,
	.disable = noop_cg_clk_op,
};

/* null_cg_clk_ops */

static int invalid_cg_clk_check_validity_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}

static struct cg_clk_ops null_cg_clk_ops = {
	.get_state = always_off_cg_clk_get_state_op,
	.check_validity = invalid_cg_clk_check_validity_op,
	.enable = noop_cg_clk_op,
	.disable = noop_cg_clk_op,
};

/* virtual_cg_clk_ops */

static int virtual_cg_clk_get_state_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return clk->state;
}

static struct cg_clk_ops virtual_cg_clk_ops = {
	.get_state = virtual_cg_clk_get_state_op,
	.check_validity = valid_cg_clk_check_validity_op,
	.enable = noop_cg_clk_op,
	.disable = noop_cg_clk_op,
};

/* wo_clr_gate_cg_clk_ops */

static int wo_clr_gate_cg_clk_enable_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);

	clk_clrl(clk->grp->set_addr, clk->mask);

	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}

static int wo_clr_gate_cg_clk_disable_op(struct cg_clk *clk)
{
	ENTER_FUNC(FUNC_LV_OP);

	clk_setl(clk->grp->set_addr, clk->mask);

	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}

static struct cg_clk_ops wo_clr_gate_cg_clk_ops = {
	.get_state = general_gate_cg_clk_get_state_op,
	.check_validity = general_cg_clk_check_validity_op,
	.enable = wo_clr_gate_cg_clk_enable_op,
	.disable = wo_clr_gate_cg_clk_disable_op,
};


/* CG_GRP variable & function */

static struct cg_grp_ops general_en_cg_grp_ops;
static struct cg_grp_ops general_gate_cg_grp_ops;

static struct cg_grp_ops noop_cg_grp_ops;

static struct cg_grp grps[] =	/* NR_GRPS */
{
	[CG_MAINPLL] = {
		.name = __stringify(CG_MAINPLL),
		.set_addr = MAINPLL_CON0,
		.clr_addr = MAINPLL_CON0,
		.sta_addr = MAINPLL_CON0,
		.mask = CG_MAINPLL_MASK,	/* TODO: set @ init is better */
		.ops = &general_en_cg_grp_ops,
		.sys = NULL,
		},
	[CG_UNIVPLL] = {
		.name = __stringify(CG_UNIVPLL),
		.set_addr = UNIVPLL_CON0,
		.clr_addr = UNIVPLL_CON0,
		.sta_addr = UNIVPLL_CON0,
		.mask = CG_UNIVPLL_MASK,	/* TODO: set @ init is better */
		.ops = &general_en_cg_grp_ops,
		.sys = NULL,
		},
	[CG_MMPLL] = {
		.name = __stringify(CG_MMPLL),
		.set_addr = MMPLL_CON0,
		.clr_addr = MMPLL_CON0,
		.sta_addr = MMPLL_CON0,
		.mask = CG_MMPLL_MASK,	/* TODO: set @ init is better */
		.ops = &general_en_cg_grp_ops,
		.sys = NULL,
		},
	[CG_TOP0] = {
		.name = __stringify(CG_TOP0),
		.set_addr = CLK_CFG_0,
		.clr_addr = CLK_CFG_0,
		.sta_addr = CLK_CFG_0,
		.mask = CG_TOP0_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = NULL,
		},
	[CG_TOP1] = {
		.name = __stringify(CG_TOP1),
		.set_addr = CLK_CFG_1,
		.clr_addr = CLK_CFG_1,
		.sta_addr = CLK_CFG_1,
		.mask = CG_TOP1_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = NULL,
		},
	[CG_TOP2] = {
		.name = __stringify(CG_TOP2),
		.set_addr = CLK_CFG_2,
		.clr_addr = CLK_CFG_2,
		.sta_addr = CLK_CFG_2,
		.mask = CG_TOP2_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = NULL,
		},
	[CG_TOP3] = {
		.name = __stringify(CG_TOP3),
		.set_addr = CLK_CFG_3,
		.clr_addr = CLK_CFG_3,
		.sta_addr = CLK_CFG_3,
		.mask = CG_TOP3_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = NULL,
		},
	[CG_TOP4] = {
		.name = __stringify(CG_TOP4),
		.set_addr = CLK_CFG_4,
		.clr_addr = CLK_CFG_4,
		.sta_addr = CLK_CFG_4,
		.mask = CG_TOP4_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = NULL,
		},
	[CG_TOP6] = {
		.name = __stringify(CG_TOP6),
		.set_addr = CLK_CFG_6,
		.clr_addr = CLK_CFG_6,
		.sta_addr = CLK_CFG_6,
		.mask = CG_TOP6_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = NULL,
		},
	[CG_TOP7] = {
		.name = __stringify(CG_TOP7),
		.set_addr = CLK_CFG_7,
		.clr_addr = CLK_CFG_7,
		.sta_addr = CLK_CFG_7,
		.mask = CG_TOP7_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = NULL,
		},
	[CG_TOP8] = {
		.name = __stringify(CG_TOP8),
		.set_addr = CLK_CFG_8,
		.clr_addr = CLK_CFG_8,
		.sta_addr = CLK_CFG_8,
		.mask = CG_TOP8_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = NULL,
		},
	[CG_TOP9] = {
		.name = __stringify(CG_TOP9),
		.set_addr = CLK_CFG_9,
		.clr_addr = CLK_CFG_9,
		.sta_addr = CLK_CFG_9,
		.mask = CG_TOP9_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = NULL,
		},
	[CG_INFRA] = {
		.name = __stringify(CG_INFRA),
		.set_addr = INFRA_PDN0,
		.clr_addr = INFRA_PDN1,
		.sta_addr = INFRA_PDN_STA,
		.mask = CG_INFRA_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = &syss[SYS_IFR],
		},
	[CG_PERI0] = {
		.name = __stringify(CG_PERI0),
		.set_addr = PERI_PDN0_SET,
		.clr_addr = PERI_PDN0_CLR,
		.sta_addr = PERI_PDN0_STA,
		.mask = CG_PERI0_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = &syss[SYS_IFR],
		},
	[CG_PERI1] = {
		.name = __stringify(CG_PERI1),
		.set_addr = PERI_PDN1_SET,
		.clr_addr = PERI_PDN1_CLR,
		.sta_addr = PERI_PDN1_STA,
		.mask = CG_PERI1_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = &syss[SYS_IFR],
		},
	[CG_MFG] = {
		.name = __stringify(CG_MFG),
		.set_addr = MFG_PD_SET,
		.clr_addr = MFG_PD_CLR,
		.sta_addr = MFG_PD_STATUS,
		.mask = CG_MFG_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = NULL,
		    },
	[CG_ISP] = {
		.name = __stringify(CG_ISP),
		.set_addr = IMG_CG_SET,
		.clr_addr = IMG_CG_CLR,
		.sta_addr = IMG_CG_CON,
		.mask = CG_ISP_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = &syss[SYS_ISP],
		    },
	[CG_VENC] = {
		.name = __stringify(CG_VENC),
		.set_addr = VENCSYS_CG_SET,
		.clr_addr = VENCSYS_CG_CLR,
		.sta_addr = VENCSYS_CG_CON,
		.mask = CG_VENC_MASK,	/* TODO: set @ init is better */
		.ops = &general_en_cg_grp_ops,
		.sys = &syss[SYS_VEN],
		},
	[CG_VDEC0] = {
		.name = __stringify(CG_VDEC0),
		.set_addr = VDEC_CKEN_SET,
		.clr_addr = VDEC_CKEN_CLR,
		.sta_addr = VDEC_CKEN_SET,
		.mask = CG_VDEC0_MASK,	/* TODO: set @ init is better */
		.ops = &general_en_cg_grp_ops,
		.sys = &syss[SYS_VDE],
		},
	[CG_VDEC1] = {
		.name = __stringify(CG_VDEC1),
		.set_addr = VDEC_LARB_CKEN_SET,
		.clr_addr = VDEC_LARB_CKEN_CLR,
		.sta_addr = VDEC_LARB_CKEN_SET,
		.mask = CG_VDEC1_MASK,	/* TODO: set @ init is better */
		.ops = &general_en_cg_grp_ops,
		.sys = &syss[SYS_VDE],
		},
	[CG_DISP0] = {
		.name = __stringify(CG_DISP0),
		.set_addr = DISP_CG_SET0,
		.clr_addr = DISP_CG_CLR0,
		.sta_addr = DISP_CG_CON0,
		.mask = CG_DISP0_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = &syss[SYS_DIS],
		},
	[CG_DISP1] = {
		.name = __stringify(CG_DISP1),
		.set_addr = DISP_CG_SET1,
		.clr_addr = DISP_CG_CLR1,
		.sta_addr = DISP_CG_CON1,
		.mask = CG_DISP1_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = &syss[SYS_DIS],
		},
	[CG_AUDIO] = {
		.name = __stringify(CG_AUDIO),
		.set_addr = AUDIO_TOP_CON0,
		.clr_addr = AUDIO_TOP_CON0,
		.sta_addr = AUDIO_TOP_CON0,
		.mask = CG_AUDIO_MASK,	/* TODO: set @ init is better */
		.ops = &general_gate_cg_grp_ops,
		.sys = NULL,
		},
	[CG_VIRTUAL] = {
		.name = __stringify(CG_VIRTUAL),
		.set_addr = 0,
		.clr_addr = 0,
		.sta_addr = 0,
		.mask = 0,
		.ops = &noop_cg_grp_ops,
		.sys = NULL,
		},
};


static struct cg_grp *id_to_grp(enum cg_grp_id id)
{
	return id < NR_GRPS ? &grps[id] : NULL;
}


/* noop_cg_grp_ops */

static int noop_cg_grp_dump_regs_op(struct cg_grp *grp, unsigned int *ptr)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return 0;		/* return size */
}

static unsigned int noop_cg_grp_get_state_op(struct cg_grp *grp)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}

static struct cg_grp_ops noop_cg_grp_ops = {
	.get_state = noop_cg_grp_get_state_op,
	.dump_regs = noop_cg_grp_dump_regs_op,
};


/* general_cg_grp */

static int general_cg_grp_dump_regs_op(struct cg_grp *grp, unsigned int *ptr)
{
	ENTER_FUNC(FUNC_LV_OP);

	*(ptr) = clk_readl(grp->sta_addr);

	EXIT_FUNC(FUNC_LV_OP);
	return 1;		/* return size */
}

/* general_gate_cg_grp */

static unsigned int general_gate_cg_grp_get_state_op(struct cg_grp *grp)
{
	unsigned int val;
	struct subsys *sys = grp->sys;

	ENTER_FUNC(FUNC_LV_OP);

	EXIT_FUNC(FUNC_LV_OP);

	if (sys && !sys->state) {
		return 0;
	} else {
		val = clk_readl(grp->sta_addr);
		val = (~val) & (grp->mask);	/* clock gate */

		return val;
	}
}

static struct cg_grp_ops general_gate_cg_grp_ops = {
	.get_state = general_gate_cg_grp_get_state_op,
	.dump_regs = general_cg_grp_dump_regs_op,
};

/* general_en_cg_grp */

static unsigned int general_en_cg_grp_get_state_op(struct cg_grp *grp)
{
	unsigned int val;
	struct subsys *sys = grp->sys;

	ENTER_FUNC(FUNC_LV_OP);

	EXIT_FUNC(FUNC_LV_OP);

	if (sys && !sys->state) {
		return 0;
	} else {
		val = clk_readl(grp->sta_addr);
		val &= (grp->mask);	/* clock enable */

		return val;
	}
}

static struct cg_grp_ops general_en_cg_grp_ops = {
	.get_state = general_en_cg_grp_get_state_op,
	.dump_regs = general_cg_grp_dump_regs_op,
};


/* enable_clock() / disable_clock() */

static int enable_pll_locked(struct pll *pll);
static int disable_pll_locked(struct pll *pll);

static int enable_subsys_locked(struct subsys *sys);
static int disable_subsys_locked(struct subsys *sys, int force_off);

static int power_prepare_locked(struct cg_grp *grp)
{
	int err = 0;

	ENTER_FUNC(FUNC_LV_BODY);

	if (grp->sys)
		err = enable_subsys_locked(grp->sys);

	EXIT_FUNC(FUNC_LV_BODY);
	return err;
}

static int power_finish_locked(struct cg_grp *grp)
{
	int err = 0;

	ENTER_FUNC(FUNC_LV_BODY);

	if (grp->sys)
		err = disable_subsys_locked(grp->sys, 0);	/* NOT force off */

	EXIT_FUNC(FUNC_LV_BODY);
	return err;
}

static int enable_clock_locked(struct cg_clk *clk)
{
	struct cg_grp *grp;
	unsigned int local_state;
#ifdef STATE_CHECK_DEBUG
	unsigned int reg_state;
#endif
	int err;

	ENTER_FUNC(FUNC_LV_LOCKED);

	if (NULL == clk) {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return -1;
	}

	clk->cnt++;

	/* clk_info("%s[%d]\n", __FUNCTION__, clk - &clks[0]); // <-XXX */

	if (clk->cnt > 1) {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return 0;	/* XXX: have enabled and return directly */
	}

	local_state = clk->state;
	grp = clk->grp;

#ifdef STATE_CHECK_DEBUG
	reg_state = clk->ops->get_state(clk);
	BUG_ON(local_state != reg_state);
#endif

	/* step 1: pll check */
	if (clk->parent)
		enable_pll_locked(clk->parent);

	/* step 2: subsys check */
	err = power_prepare_locked(grp);
	BUG_ON(err);

	/* step 3: source clock check */
	if (clk->src < NR_CLKS)
		enable_clock_locked(id_to_clk(clk->src));

	if (local_state == PWR_ON) {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return 0;	/* XXX: assume local_state & reg_state are the same */
	}
	/* step 4: local clock enable */
	clk->ops->enable(clk);

	clk->state = PWR_ON;
	grp->state |= clk->mask;

	EXIT_FUNC(FUNC_LV_LOCKED);
	return 0;
}

static int disable_clock_locked(struct cg_clk *clk)
{
	struct cg_grp *grp;
	unsigned int local_state;
#ifdef STATE_CHECK_DEBUG
	unsigned int reg_state;
#endif
	int err;

	ENTER_FUNC(FUNC_LV_LOCKED);

	if (NULL == clk) {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return -1;
	}

	WARN_ON(!clk->cnt);
	clk->cnt--;

	/* clk_info("%s[%d]\n", __FUNCTION__, clk - &clks[0]); // <-XXX */

	if (clk->cnt > 0) {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return 0;	/* XXX: not count down zero and return directly */
	} else if (clk->cnt < 0) {
		clk->cnt = 0;
		EXIT_FUNC(FUNC_LV_LOCKED);
		return 0;	/* Allow unbalanced disable */
	}

	local_state = clk->state;
	grp = clk->grp;

#ifdef STATE_CHECK_DEBUG
	reg_state = clk->ops->get_state(clk);
	BUG_ON(local_state != reg_state);
#endif

	if (local_state == PWR_DOWN) {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return 0;	/* XXX: assume local_state & reg_state are the same */
	}
	/* step 1: local clock disable */
	clk->ops->disable(clk);

	clk->state = PWR_DOWN;
	grp->state &= ~(clk->mask);

	/* step 2: source clock check */
	if (clk->src < NR_CLKS)
		disable_clock_locked(id_to_clk(clk->src));

	/* step 3: subsys check */
	err = power_finish_locked(grp);
	BUG_ON(err);

	/* step 4: pll check */
	if (clk->parent)
		disable_pll_locked(clk->parent);

	EXIT_FUNC(FUNC_LV_LOCKED);
	return 0;
}

static int get_clk_state_locked(struct cg_clk *clk)
{
	if (likely(initialized))
		return clk->state;
	else
		return clk->ops->get_state(clk);
}

int mt_enable_clock(enum cg_clk_id id, const char *name)
{
	int err;
	unsigned long flags;
	struct cg_clk *clk = id_to_clk(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!initialized);
	BUG_ON(!clk);
	BUG_ON(!clk->grp);
	BUG_ON(!clk->ops->check_validity(clk));

	clkmgr_lock(flags);
	err = enable_clock_locked(clk);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return err;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(mt_enable_clock);

int mt_disable_clock(enum cg_clk_id id, const char *name)
{
	int err;
	unsigned long flags;
	struct cg_clk *clk = id_to_clk(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!initialized);
	BUG_ON(!clk);
	BUG_ON(!clk->grp);
	BUG_ON(!clk->ops->check_validity(clk));

	clkmgr_lock(flags);
	err = disable_clock_locked(clk);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return err;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(mt_disable_clock);

int clock_is_on(enum cg_clk_id id)
{
	int state;
	unsigned long flags;
	struct cg_clk *clk = id_to_clk(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!clk);
	BUG_ON(!clk->grp);
	BUG_ON(!clk->ops->check_validity(clk));

	clkmgr_lock(flags);
	state = get_clk_state_locked(clk);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return state;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(clock_is_on);

int grp_dump_regs(enum cg_grp_id id, unsigned int *ptr)
{
	struct cg_grp *grp = id_to_grp(id);

	ENTER_FUNC(FUNC_LV_DONT_CARE);

	/* BUG_ON(!initialized); */
	BUG_ON(!grp);

	EXIT_FUNC(FUNC_LV_DONT_CARE);
	return grp->ops->dump_regs(grp, ptr);
}
EXPORT_SYMBOL(grp_dump_regs);

const char *grp_get_name(enum cg_grp_id id)
{
	struct cg_grp *grp = id_to_grp(id);

	ENTER_FUNC(FUNC_LV_DONT_CARE);

	/* BUG_ON(!initialized); */
	BUG_ON(!grp);

	EXIT_FUNC(FUNC_LV_DONT_CARE);
	return grp->name;
}

enum cg_grp_id clk_id_to_grp_id(enum cg_clk_id id)
{
	struct cg_clk *clk;

	clk = id_to_clk(id);

	return (NULL == clk) ? NR_GRPS : (clk->grp - &grps[0]);
}

unsigned int clk_id_to_mask(enum cg_clk_id id)
{
	struct cg_clk *clk;

	clk = id_to_clk(id);

	return (NULL == clk) ? 0 : clk->mask;
}


/* CLKMUX variable & function */

static struct clkmux_ops glitch_free_clkmux_ops;
static struct clkmux_ops glitch_free_wo_drain_cg_clkmux_ops;


static const struct clkmux_map _mt_clkmux_axi_sel_map[] = {
	{ .val = BITS(2 : 0, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 1), .id = MT_CG_V_SYSPLL_D3, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 2), .id = MT_CG_V_SYSPLL_D4, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 3), .id = MT_CG_V_SYSPLL_D6, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 4), .id = MT_CG_V_UNIVPLL_D5, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 5), .id = MT_CG_V_UNIVPLL2_D2, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 6), .id = MT_CG_V_SYSPLL_D3P5, .mask = BITMASK(2 : 0), },
};

static const struct clkmux_map _mt_clkmux_smi_sel_map[] = {
	{ .val = BITS(11 : 8, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 1), .id = MT_CG_V_CLKPH_MCK, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 2), .id = MT_CG_V_SYSPLL_D2P5, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 3), .id = MT_CG_V_SYSPLL_D3, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 4), .id = MT_CG_V_SYSPLL_D8, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 5), .id = MT_CG_V_UNIVPLL_D5, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 6), .id = MT_CG_V_UNIVPLL1_D2, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 7), .id = MT_CG_V_UNIVPLL1_D6, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 8), .id = MT_CG_V_MMPLL_D3, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 9), .id = MT_CG_V_MMPLL_D4, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 10), .id = MT_CG_V_MMPLL_D5, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 11), .id = MT_CG_V_MMPLL_D6, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 12), .id = MT_CG_V_MMPLL_D7, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 13), .id = MT_CG_V_AD_VDECPLL_CK, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 14), .id = MT_CG_V_LVDSPLL_CLK, .mask = BITMASK(11 : 8), },
};

static const struct clkmux_map _mt_clkmux_mfg_sel_map[] = {
	{ .val = BITS(19 : 16, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(19 : 16), },
	{ .val = BITS(19 : 16, 1), .id = MT_CG_V_UNIVPLL1_D4, .mask = BITMASK(19 : 16), },
	{ .val = BITS(19 : 16, 2), .id = MT_CG_V_SYSPLL_D2, .mask = BITMASK(19 : 16), },
	{ .val = BITS(19 : 16, 3), .id = MT_CG_V_SYSPLL_D2P5, .mask = BITMASK(19 : 16), },
	{ .val = BITS(19 : 16, 4), .id = MT_CG_V_SYSPLL_D3, .mask = BITMASK(19 : 16), },
	{ .val = BITS(19 : 16, 5), .id = MT_CG_V_UNIVPLL_D5, .mask = BITMASK(19 : 16), },
	{ .val = BITS(19 : 16, 6), .id = MT_CG_V_UNIVPLL1_D2, .mask = BITMASK(19 : 16), },
	{ .val = BITS(19 : 16, 7), .id = MT_CG_V_MMPLL_D2, .mask = BITMASK(19 : 16), },
	{ .val = BITS(19 : 16, 8), .id = MT_CG_V_MMPLL_D3, .mask = BITMASK(19 : 16), },
	{ .val = BITS(19 : 16, 9), .id = MT_CG_V_MMPLL_D4, .mask = BITMASK(19 : 16), },
	{ .val = BITS(19 : 16, 10), .id = MT_CG_V_MMPLL_D5, .mask = BITMASK(19 : 16), },
	{ .val = BITS(19 : 16, 11), .id = MT_CG_V_MMPLL_D6, .mask = BITMASK(19 : 16), },
	{ .val = BITS(19 : 16, 12), .id = MT_CG_V_MMPLL_D7, .mask = BITMASK(19 : 16), },
};

static const struct clkmux_map _mt_clkmux_irda_sel_map[] = {
	{ .val = BITS(25 : 24, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(25 : 24), },
	{ .val = BITS(25 : 24, 1), .id = MT_CG_V_UNIVPLL2_D8, .mask = BITMASK(25 : 24), },
	{ .val = BITS(25 : 24, 2), .id = MT_CG_V_UNIVPLL1_D6, .mask = BITMASK(25 : 24), },
};

static const struct clkmux_map _mt_clkmux_cam_sel_map[] = {
	{ .val = BITS(2 : 0, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 1), .id = MT_CG_V_SYSPLL_D3, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 2), .id = MT_CG_V_SYSPLL_D3P5, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 3), .id = MT_CG_V_SYSPLL_D4, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 4), .id = MT_CG_V_UNIVPLL_D5, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 5), .id = MT_CG_V_UNIVPLL2_D2, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 6), .id = MT_CG_V_UNIVPLL_D7, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 7), .id = MT_CG_V_UNIVPLL1_D4, .mask = BITMASK(2 : 0), },
};

static const struct clkmux_map _mt_clkmux_aud_intbus_sel_map[] = {
	{ .val = BITS(9 : 8, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(9 : 8), },
	{ .val = BITS(9 : 8, 1), .id = MT_CG_V_SYSPLL_D6, .mask = BITMASK(9 : 8), },
	{ .val = BITS(9 : 8, 2), .id = MT_CG_V_UNIVPLL_D10, .mask = BITMASK(9 : 8), },
};

static const struct clkmux_map _mt_clkmux_jpg_sel_map[] = {
	{ .val = BITS(18 : 16, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 1), .id = MT_CG_V_SYSPLL_D5, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 2), .id = MT_CG_V_SYSPLL_D4, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 3), .id = MT_CG_V_SYSPLL_D3, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 4), .id = MT_CG_V_UNIVPLL_D7, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 5), .id = MT_CG_V_UNIVPLL2_D2, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 6), .id = MT_CG_V_UNIVPLL_D5, .mask = BITMASK(18 : 16), },
};

static const struct clkmux_map _mt_clkmux_disp_sel_map[] = {
	{ .val = BITS(26 : 24, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 1), .id = MT_CG_V_SYSPLL_D3P5, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 2), .id = MT_CG_V_SYSPLL_D3, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 3), .id = MT_CG_V_UNIVPLL2_D2, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 4), .id = MT_CG_V_UNIVPLL_D5, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 5), .id = MT_CG_V_UNIVPLL1_D2, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 6), .id = MT_CG_V_LVDSPLL_CLK, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 7), .id = MT_CG_V_AD_VDECPLL_CK, .mask = BITMASK(26 : 24), },
};

static const struct clkmux_map _mt_clkmux_msdc30_1_sel_map[] = {
	{ .val = BITS(2 : 0, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 1), .id = MT_CG_V_SYSPLL_D6, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 2), .id = MT_CG_V_SYSPLL_D5, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 3), .id = MT_CG_V_UNIVPLL1_D4, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 4), .id = MT_CG_V_UNIVPLL2_D4, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 5), .id = MT_CG_V_AD_MSDCPLL_CK, .mask = BITMASK(2 : 0), },
};

static const struct clkmux_map _mt_clkmux_msdc30_2_sel_map[] = {
	{ .val = BITS(10 : 8, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 1), .id = MT_CG_V_SYSPLL_D6, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 2), .id = MT_CG_V_SYSPLL_D5, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 3), .id = MT_CG_V_UNIVPLL1_D4, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 4), .id = MT_CG_V_UNIVPLL2_D4, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 5), .id = MT_CG_V_AD_MSDCPLL_CK, .mask = BITMASK(10 : 8), },
};

static const struct clkmux_map _mt_clkmux_msdc30_3_sel_map[] = {
	{ .val = BITS(18 : 16, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 1), .id = MT_CG_V_SYSPLL_D6, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 2), .id = MT_CG_V_SYSPLL_D5, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 3), .id = MT_CG_V_UNIVPLL1_D4, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 4), .id = MT_CG_V_UNIVPLL2_D4, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 5), .id = MT_CG_V_AD_MSDCPLL_CK, .mask = BITMASK(18 : 16), },
};

static const struct clkmux_map _mt_clkmux_msdc30_4_sel_map[] = {
	{ .val = BITS(26 : 24, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 1), .id = MT_CG_V_SYSPLL_D6, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 2), .id = MT_CG_V_SYSPLL_D5, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 3), .id = MT_CG_V_UNIVPLL1_D4, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 4), .id = MT_CG_V_UNIVPLL2_D4, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 5), .id = MT_CG_V_AD_MSDCPLL_CK, .mask = BITMASK(26 : 24), },
};

static const struct clkmux_map _mt_clkmux_usb20_sel_map[] = {
	{ .val = BITS(1 : 0, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(1 : 0), },
	{ .val = BITS(1 : 0, 1), .id = MT_CG_V_UNIVPLL2_D6, .mask = BITMASK(1 : 0), },
	{ .val = BITS(1 : 0, 2), .id = MT_CG_V_UNIVPLL1_D10, .mask = BITMASK(1 : 0), },
};

static const struct clkmux_map _mt_clkmux_venc_sel_map[] = {
	{ .val = BITS(10 : 8, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 1), .id = MT_CG_V_SYSPLL_D3, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 2), .id = MT_CG_V_SYSPLL_D8, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 3), .id = MT_CG_V_UNIVPLL_D5, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 4), .id = MT_CG_V_UNIVPLL1_D6, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 5), .id = MT_CG_V_MMPLL_D4, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 6), .id = MT_CG_V_MMPLL_D5, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 7), .id = MT_CG_V_MMPLL_D6, .mask = BITMASK(10 : 8), },
};

static const struct clkmux_map _mt_clkmux_spi_sel_map[] = {
	{ .val = BITS(18 : 16, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 1), .id = MT_CG_V_SYSPLL_D6, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 2), .id = MT_CG_V_SYSPLL_D8, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 3), .id = MT_CG_V_SYSPLL_D10, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 4), .id = MT_CG_V_UNIVPLL1_D6, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 5), .id = MT_CG_V_UNIVPLL1_D8, .mask = BITMASK(18 : 16), },
};

static const struct clkmux_map _mt_clkmux_uart_sel_map[] = {
	{ .val = BITS(25 : 24, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(25 : 24), },
	{ .val = BITS(25 : 24, 1), .id = MT_CG_V_UNIVPLL2_D8, .mask = BITMASK(25 : 24), },
};

static const struct clkmux_map _mt_clkmux_mem_sel_map[] = {
	{ .val = BITS(1 : 0, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(1 : 0), },
	{ .val = BITS(1 : 0, 1), .id = MT_CG_V_CLKPH_MCK, .mask = BITMASK(1 : 0), },
};

static const struct clkmux_map _mt_clkmux_camtg_sel_map[] = {
	{ .val = BITS(10 : 8, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 1), .id = MT_CG_V_UNIVPLL_D26, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 2), .id = MT_CG_V_UNIVPLL1_D6, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 3), .id = MT_CG_V_SYSPLL_D16, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 4), .id = MT_CG_V_SYSPLL_D8, .mask = BITMASK(10 : 8), },
};

static const struct clkmux_map _mt_clkmux_audio_sel_map[] = {
	{ .val = BITS(25 : 24, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(25 : 24), },
	{ .val = BITS(25 : 24, 1), .id = MT_CG_V_SYSPLL_D24, .mask = BITMASK(25 : 24), },
};

static const struct clkmux_map _mt_clkmux_fix_sel_map[] = {
	{ .val = BITS(2 : 0, 0), .id = MT_CG_V_RTC_CK, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 1), .id = MT_CG_V_26M_CK, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 2), .id = MT_CG_V_UNIVPLL_D5, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 3), .id = MT_CG_V_UNIVPLL_D7, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 4), .id = MT_CG_V_UNIVPLL1_D2, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 5), .id = MT_CG_V_UNIVPLL1_D4, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 6), .id = MT_CG_V_UNIVPLL1_D6, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 7), .id = MT_CG_V_UNIVPLL1_D8, .mask = BITMASK(2 : 0), },
};

static const struct clkmux_map _mt_clkmux_vdec_sel_map[] = {
	{ .val = BITS(11 : 8, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 1), .id = MT_CG_V_AD_VDECPLL_CK, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 2), .id = MT_CG_V_CLKPH_MCK, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 3), .id = MT_CG_V_SYSPLL_D2P5, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 4), .id = MT_CG_V_SYSPLL_D3, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 5), .id = MT_CG_V_SYSPLL_D3P5, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 6), .id = MT_CG_V_SYSPLL_D4, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 7), .id = MT_CG_V_SYSPLL_D5, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 8), .id = MT_CG_V_SYSPLL_D6, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 9), .id = MT_CG_V_SYSPLL_D8, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 10), .id = MT_CG_V_UNIVPLL1_D2, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 11), .id = MT_CG_V_UNIVPLL2_D2, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 12), .id = MT_CG_V_UNIVPLL_D7, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 13), .id = MT_CG_V_UNIVPLL_D10, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 14), .id = MT_CG_V_UNIVPLL2_D4, .mask = BITMASK(11 : 8), },
	{ .val = BITS(11 : 8, 15), .id = MT_CG_V_AD_LVDSPLL_CK, .mask = BITMASK(11 : 8), },
};

static const struct clkmux_map _mt_clkmux_ddrphycfg_sel_map[] = {
	{ .val = BITS(17 : 16, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(17 : 16), },
	{ .val = BITS(17 : 16, 1), .id = MT_CG_V_AXI_DDRPHYCFG_AS_CK, .mask = BITMASK(17 : 16), },
	{ .val = BITS(17 : 16, 2), .id = MT_CG_V_SYSPLL_D12, .mask = BITMASK(17 : 16), },
};

static const struct clkmux_map _mt_clkmux_dpilvds_sel_map[] = {
	{ .val = BITS(26 : 24, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 1), .id = MT_CG_V_AD_LVDSPLL_CK, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 2), .id = MT_CG_V_LVDSPLL_D2, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 3), .id = MT_CG_V_LVDSPLL_D4, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 4), .id = MT_CG_V_LVDSPLL_D8, .mask = BITMASK(26 : 24), },
};

static const struct clkmux_map _mt_clkmux_pmicspi_sel_map[] = {
	{ .val = BITS(2 : 0, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 1), .id = MT_CG_V_UNIVPLL2_D6, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 2), .id = MT_CG_V_SYSPLL_D8, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 3), .id = MT_CG_V_SYSPLL_D10, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 4), .id = MT_CG_V_UNIVPLL1_D10, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 5), .id = MT_CG_V_MEMPLL_MCK_D4, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 6), .id = MT_CG_V_UNIVPLL_D26, .mask = BITMASK(2 : 0), },
	{ .val = BITS(2 : 0, 7), .id = MT_CG_V_SYSPLL_D24, .mask = BITMASK(2 : 0), },
};

static const struct clkmux_map _mt_clkmux_msdc30_0_sel_map[] = {
	{ .val = BITS(10 : 8, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 1), .id = MT_CG_V_SYSPLL_D6, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 2), .id = MT_CG_V_SYSPLL_D5, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 3), .id = MT_CG_V_UNIVPLL1_D4, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 4), .id = MT_CG_V_UNIVPLL2_D4, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 5), .id = MT_CG_V_AD_MSDCPLL_CK, .mask = BITMASK(10 : 8), },
};

static const struct clkmux_map _mt_clkmux_smi_mfg_as_sel_map[] = {
	{ .val = BITS(17 : 16, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(17 : 16), },
	{ .val = BITS(17 : 16, 1), .id = MT_CG_TOP_PDN_SMI, .mask = BITMASK(17 : 16), },
	{ .val = BITS(17 : 16, 2), .id = MT_CG_TOP_PDN_MFG, .mask = BITMASK(17 : 16), },
	{ .val = BITS(17 : 16, 3), .id = MT_CG_TOP_PDN_MEM, .mask = BITMASK(17 : 16), },
};

static const struct clkmux_map _mt_clkmux_gcpu_sel_map[] = {
	{ .val = BITS(26 : 24, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 1), .id = MT_CG_V_SYSPLL_D4, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 2), .id = MT_CG_V_UNIVPLL_D7, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 3), .id = MT_CG_V_SYSPLL_D5, .mask = BITMASK(26 : 24), },
	{ .val = BITS(26 : 24, 4), .id = MT_CG_V_SYSPLL_D6, .mask = BITMASK(26 : 24), },
};

static const struct clkmux_map _mt_clkmux_dpi1_sel_map[] = {
	{ .val = BITS(1 : 0, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(1 : 0), },
	{ .val = BITS(1 : 0, 1), .id = MT_CG_V_AD_TVHDMI_H_CK, .mask = BITMASK(1 : 0), },
	{ .val = BITS(1 : 0, 2), .id = MT_CG_V_TVHDMI_D2, .mask = BITMASK(1 : 0), },
	{ .val = BITS(1 : 0, 3), .id = MT_CG_V_TVHDMI_D4, .mask = BITMASK(1 : 0), },
};

static const struct clkmux_map _mt_clkmux_cci_sel_map[] = {
	{ .val = BITS(10 : 8, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 1), .id = MT_CG_MIX_MAINPLL_537P3M_EN, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 2), .id = MT_CG_V_UNIVPLL_D3, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 3), .id = MT_CG_V_SYSPLL_D2P5, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 4), .id = MT_CG_V_SYSPLL_D3, .mask = BITMASK(10 : 8), },
	{ .val = BITS(10 : 8, 5), .id = MT_CG_V_SYSPLL_D5, .mask = BITMASK(10 : 8), },
};

static const struct clkmux_map _mt_clkmux_apll_sel_map[] = {
	{ .val = BITS(18 : 16, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 1), .id = MT_CG_V_AD_APLL_CK, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 2), .id = MT_CG_V_APLL_D4, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 3), .id = MT_CG_V_APLL_D8, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 4), .id = MT_CG_V_APLL_D16, .mask = BITMASK(18 : 16), },
	{ .val = BITS(18 : 16, 5), .id = MT_CG_V_APLL_D24, .mask = BITMASK(18 : 16), },
};

static const struct clkmux_map _mt_clkmux_hdmipll_sel_map[] = {
	{ .val = BITS(25 : 24, 0), .id = MT_CG_V_CKSQ_MUX_CK, .mask = BITMASK(25 : 24), },
	{ .val = BITS(25 : 24, 1), .id = MT_CG_V_AD_HDMITX_CLKDIG_CTS, .mask = BITMASK(25 : 24), },
	{ .val = BITS(25 : 24, 2), .id = MT_CG_V_HDMITX_CLKDIG_D2, .mask = BITMASK(25 : 24), },
	{ .val = BITS(25 : 24, 3), .id = MT_CG_V_HDMITX_CLKDIG_D3, .mask = BITMASK(25 : 24), },
};

static struct clkmux muxs[] =	/* NR_CLKMUXS */
{
	[MT_CLKMUX_AXI_SEL] = {
		.name = __stringify(MT_CLKMUX_AXI_SEL),
		.base_addr = CLK_CFG_0,
		.ops = &glitch_free_wo_drain_cg_clkmux_ops,
		.map = _mt_clkmux_axi_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_axi_sel_map),
		.drain = MT_CG_V_AXI_CK,
		},
	[MT_CLKMUX_SMI_SEL] = {
		.name = __stringify(MT_CLKMUX_SMI_SEL),
		.base_addr = CLK_CFG_0,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_smi_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_smi_sel_map),
		.drain = MT_CG_TOP_PDN_SMI,
		},
	[MT_CLKMUX_MFG_SEL] = {
		.name = __stringify(MT_CLKMUX_MFG_SEL),
		.base_addr = CLK_CFG_0,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_mfg_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_mfg_sel_map),
		.drain = MT_CG_TOP_PDN_MFG,
		},
	[MT_CLKMUX_IRDA_SEL] = {
		.name = __stringify(MT_CLKMUX_IRDA_SEL),
		.base_addr = CLK_CFG_0,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_irda_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_irda_sel_map),
		.drain = MT_CG_TOP_PDN_IRDA,
		},
	[MT_CLKMUX_CAM_SEL] = {
		.name = __stringify(MT_CLKMUX_CAM_SEL),
		.base_addr = CLK_CFG_1,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_cam_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_cam_sel_map),
		.drain = MT_CG_TOP_PDN_CAM,
		},
	[MT_CLKMUX_AUD_INTBUS_SEL] = {
		.name = __stringify(MT_CLKMUX_AUD_INTBUS_SEL),
		.base_addr = CLK_CFG_1,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_aud_intbus_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_aud_intbus_sel_map),
		.drain = MT_CG_TOP_PDN_AUD_INTBUS,
		},
	[MT_CLKMUX_JPG_SEL] = {
		.name = __stringify(MT_CLKMUX_JPG_SEL),
		.base_addr = CLK_CFG_1,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_jpg_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_jpg_sel_map),
		.drain = MT_CG_TOP_PDN_JPG,
		},
	[MT_CLKMUX_DISP_SEL] = {
		.name = __stringify(MT_CLKMUX_DISP_SEL),
		.base_addr = CLK_CFG_1,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_disp_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_disp_sel_map),
		.drain = MT_CG_TOP_PDN_DISP,
		},
	[MT_CLKMUX_MSDC30_1_SEL] = {
		.name = __stringify(MT_CLKMUX_MSDC30_1_SEL),
		.base_addr = CLK_CFG_2,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_msdc30_1_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_msdc30_1_sel_map),
		.drain = MT_CG_TOP_PDN_MSDC30_1,
		},
	[MT_CLKMUX_MSDC30_2_SEL] = {
		.name = __stringify(MT_CLKMUX_MSDC30_2_SEL),
		.base_addr = CLK_CFG_2,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_msdc30_2_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_msdc30_2_sel_map),
		.drain = MT_CG_TOP_PDN_MSDC30_2,
		},
	[MT_CLKMUX_MSDC30_3_SEL] = {
		.name = __stringify(MT_CLKMUX_MSDC30_3_SEL),
		.base_addr = CLK_CFG_2,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_msdc30_3_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_msdc30_3_sel_map),
		.drain = MT_CG_TOP_PDN_MSDC30_3,
		},
	[MT_CLKMUX_MSDC30_4_SEL] = {
		.name = __stringify(MT_CLKMUX_MSDC30_4_SEL),
		.base_addr = CLK_CFG_2,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_msdc30_4_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_msdc30_4_sel_map),
		.drain = MT_CG_TOP_PDN_MSDC30_4,
		},
	[MT_CLKMUX_USB20_SEL] = {
		.name = __stringify(MT_CLKMUX_USB20_SEL),
		.base_addr = CLK_CFG_3,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_usb20_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_usb20_sel_map),
		.drain = MT_CG_TOP_PDN_USB20,
	    },
	[MT_CLKMUX_VENC_SEL] = {
		.name = __stringify(MT_CLKMUX_VENC_SEL),
		.base_addr = CLK_CFG_4,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_venc_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_venc_sel_map),
		.drain = MT_CG_TOP_PDN_VENC,
		},
	[MT_CLKMUX_SPI_SEL] = {
		.name = __stringify(MT_CLKMUX_SPI_SEL),
		.base_addr = CLK_CFG_4,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_spi_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_spi_sel_map),
		.drain = MT_CG_TOP_PDN_SPI,
		},
	[MT_CLKMUX_UART_SEL] = {
		.name = __stringify(MT_CLKMUX_UART_SEL),
		.base_addr = CLK_CFG_4,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_uart_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_uart_sel_map),
		.drain = MT_CG_TOP_PDN_UART,
		},
	[MT_CLKMUX_MEM_SEL] = {
		.name = __stringify(MT_CLKMUX_MEM_SEL),
		.base_addr = CLK_CFG_6,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_mem_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_mem_sel_map),
		.drain = MT_CG_TOP_PDN_MEM,
		},
	[MT_CLKMUX_CAMTG_SEL] = {
		.name = __stringify(MT_CLKMUX_CAMTG_SEL),
		.base_addr = CLK_CFG_6,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_camtg_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_camtg_sel_map),
		.drain = MT_CG_TOP_PDN_CAMTG,
		},
	[MT_CLKMUX_AUDIO_SEL] = {
		.name = __stringify(MT_CLKMUX_AUDIO_SEL),
		.base_addr = CLK_CFG_6,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_audio_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_audio_sel_map),
		.drain = MT_CG_TOP_PDN_AUDIO,
		},
	[MT_CLKMUX_FIX_SEL] = {
		.name = __stringify(MT_CLKMUX_FIX_SEL),
		.base_addr = CLK_CFG_7,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_fix_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_fix_sel_map),
		.drain = MT_CG_TOP_PDN_FIX,
		},
	[MT_CLKMUX_VDEC_SEL] = {
		.name = __stringify(MT_CLKMUX_VDEC_SEL),
		.base_addr = CLK_CFG_7,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_vdec_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_vdec_sel_map),
		.drain = MT_CG_TOP_PDN_VDEC,
		},
	[MT_CLKMUX_DDRPHYCFG_SEL] = {
		.name = __stringify(MT_CLKMUX_DDRPHYCFG_SEL),
		.base_addr = CLK_CFG_7,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_ddrphycfg_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_ddrphycfg_sel_map),
		.drain = MT_CG_TOP_PDN_DDRPHYCFG,
		},
	[MT_CLKMUX_DPILVDS_SEL] = {
		.name = __stringify(MT_CLKMUX_DPILVDS_SEL),
		.base_addr = CLK_CFG_7,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_dpilvds_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_dpilvds_sel_map),
		.drain = MT_CG_TOP_PDN_DPILVDS,
		},
	[MT_CLKMUX_PMICSPI_SEL] = {
		.name = __stringify(MT_CLKMUX_PMICSPI_SEL),
		.base_addr = CLK_CFG_8,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_pmicspi_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_pmicspi_sel_map),
		.drain = MT_CG_TOP_PDN_PMICSPI,
		},
	[MT_CLKMUX_MSDC30_0_SEL] = {
		.name = __stringify(MT_CLKMUX_MSDC30_0_SEL),
		.base_addr = CLK_CFG_8,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_msdc30_0_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_msdc30_0_sel_map),
		.drain = MT_CG_TOP_PDN_MSDC30_0,
		},
	[MT_CLKMUX_SMI_MFG_AS_SEL] = {
		.name = __stringify(MT_CLKMUX_SMI_MFG_AS_SEL),
		.base_addr = CLK_CFG_8,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_smi_mfg_as_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_smi_mfg_as_sel_map),
		.drain = MT_CG_TOP_PDN_SMI_MFG_AS,
		},
	[MT_CLKMUX_GCPU_SEL] = {
		.name = __stringify(MT_CLKMUX_GCPU_SEL),
		.base_addr = CLK_CFG_8,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_gcpu_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_gcpu_sel_map),
		.drain = MT_CG_TOP_PDN_GCPU,
		},
	[MT_CLKMUX_DPI1_SEL] = {
		.name = __stringify(MT_CLKMUX_DPI1_SEL),
		.base_addr = CLK_CFG_9,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_dpi1_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_dpi1_sel_map),
		.drain = MT_CG_TOP_PDN_DPI1,
		},
	[MT_CLKMUX_CCI_SEL] = {
		.name = __stringify(MT_CLKMUX_CCI_SEL),
		.base_addr = CLK_CFG_9,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_cci_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_cci_sel_map),
		.drain = MT_CG_TOP_PDN_CCI,
		},
	[MT_CLKMUX_APLL_SEL] = {
		.name = __stringify(MT_CLKMUX_APLL_SEL),
		.base_addr = CLK_CFG_9,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_apll_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_apll_sel_map),
		.drain = MT_CG_TOP_PDN_APLL,
		},
	[MT_CLKMUX_HDMIPLL_SEL] = {
		.name = __stringify(MT_CLKMUX_HDMIPLL_SEL),
		.base_addr = CLK_CFG_9,
		.ops = &glitch_free_clkmux_ops,
		.map = _mt_clkmux_hdmipll_sel_map,
		.nr_map = ARRAY_SIZE(_mt_clkmux_hdmipll_sel_map),
		.drain = MT_CG_TOP_PDN_HDMIPLL,
		},
};


/* general clkmux get */

static enum cg_clk_id general_clkmux_get_op(struct clkmux *mux)
{
	unsigned int reg_val;
	int i;

	ENTER_FUNC(FUNC_LV_OP);

	reg_val = clk_readl(mux->base_addr);

	for (i = 0; i < mux->nr_map; i++) {
		/* clk_dbg(">> "HEX_FMT", "HEX_FMT", "HEX_FMT"\n", mux->map[i].val, mux->map[i].mask, reg_val); */
		if ((mux->map[i].val & mux->map[i].mask) == (reg_val & mux->map[i].mask))
			break;
	}

#if 0				/* XXX: just for FPGA */
	if (i == mux->nr_map)
		return MT_CG_V_26M_CK;
#else
	BUG_ON(i == mux->nr_map);
#endif

	EXIT_FUNC(FUNC_LV_OP);

	return mux->map[i].id;
}


/* (half) glitch free clkmux sel (with drain cg) */

#define GFMUX_BIT_MASK_FOR_HALF_GF_OP BITMASK(31 : 30)	/* rg_mfg_gfmux_sel, rg_spinfi_gfmux_sel */

static enum cg_clk_id clkmux_get_locked(struct clkmux *mux);

static int glitch_free_clkmux_sel_op(struct clkmux *mux, enum cg_clk_id clksrc)
{
	struct cg_clk *drain_clk = id_to_clk(mux->drain);
	struct cg_clk *from_clk;
	int err = 0;
	int i;

	ENTER_FUNC(FUNC_LV_OP);

	BUG_ON(NULL == drain_clk);	/* map error @ muxs[]->ops & muxs[]->drain */
					/* (use glitch_free_clkmux_sel_op with "wrong drain cg") */

	if (clksrc == drain_clk->src) {	/* switch to the same source (i.e. don't need clk switch actually) */
		EXIT_FUNC(FUNC_LV_OP);
		return 0;
	}
	/* look up the clk map */
	for (i = 0; i < mux->nr_map; i++) {
		if (mux->map[i].id == clksrc)
			break;
	}

	/* clk sel match */
	if (i < mux->nr_map) {
		bool disable_src_cg_flag = FALSE;
		unsigned int reg_val;

		err = enable_clock_locked(drain_clk);	/* drain clock must be enabled before MUX switch */
		BUG_ON(err);

		/* (just) help to enable/disable src clock if necessary (i.e. drain cg is on original) */
		if (PWR_ON == drain_clk->ops->get_state(drain_clk)) {
			err = enable_clock_locked(id_to_clk(clksrc));	/* XXX: enable first for seemless transition */
			BUG_ON(err);
			disable_src_cg_flag = TRUE;
		} else {
			err = enable_clock_locked(id_to_clk(clksrc));
			BUG_ON(err);

			from_clk = id_to_clk(clkmux_get_locked(mux));

			err = enable_clock_locked(from_clk);
			BUG_ON(err);
		}

		/* set clkmux reg (inc. non glitch free / half glitch free / glitch free case) */
		reg_val = (clk_readl(mux->base_addr) & ~mux->map[i].mask) | mux->map[i].val;
		clk_writel(mux->base_addr, reg_val);

		/* (just) help to enable/disable src clock if necessary (i.e. drain cg is on original) */
		if (TRUE == disable_src_cg_flag) {
			err = disable_clock_locked(id_to_clk(drain_clk->src));
			BUG_ON(err);
		} else {
			err = disable_clock_locked(id_to_clk(clksrc));
			BUG_ON(err);
			err = disable_clock_locked(from_clk);
			BUG_ON(err);
		}

		drain_clk->src = clksrc;

		err = disable_clock_locked(drain_clk);	/* restore drain clock state */
		BUG_ON(err);
	} else {
		err = -1;	/* ERROR: clk sel not match */
	}
	BUG_ON(err);		/* clk sel not match */

	EXIT_FUNC(FUNC_LV_OP);
	return err;
}

static struct clkmux_ops glitch_free_clkmux_ops = {
	.sel = glitch_free_clkmux_sel_op,
	.get = general_clkmux_get_op,
};

/* glitch free clkmux without drain cg */

static int glitch_free_wo_drain_cg_clkmux_sel_op(struct clkmux *mux, enum cg_clk_id clksrc)
{
	unsigned int reg_val;
	struct cg_clk *src_clk;
	int err;
	int i;

	ENTER_FUNC(FUNC_LV_OP);

	/* search current src cg first */
	{
		reg_val = clk_readl(mux->base_addr);

		/* look up current src cg */
		for (i = 0; i < mux->nr_map; i++) {
			if ((mux->map[i].val & mux->map[i].mask) == (reg_val & mux->map[i].mask))
				break;
		}

		BUG_ON(i >= mux->nr_map);	/* map error @ muxs[]->map */

		src_clk = id_to_clk(mux->map[i].id);

		BUG_ON(NULL == src_clk);	/* map error @ muxs[]->map */

		if (clksrc == mux->map[i].id) {	/* switch to the same source (i.e. don't need clk switch actually) */
			EXIT_FUNC(FUNC_LV_OP);
			return 0;
		}
	}

	/* look up the clk map */
	for (i = 0; i < mux->nr_map; i++) {
		if (mux->map[i].id == clksrc)
			break;
	}

	/* clk sel match */
	if (i < mux->nr_map) {
		err = enable_clock_locked(id_to_clk(clksrc));	/* XXX: enable first for seemless transition */
		BUG_ON(err);

		/* set clkmux reg (inc. non glitch free / half glitch free / glitch free case) */
		reg_val = (clk_readl(mux->base_addr) & ~mux->map[i].mask) | mux->map[i].val;
		clk_writel(mux->base_addr, reg_val);

		err = disable_clock_locked(src_clk);
		BUG_ON(err);
	} else {
		err = -1;	/* ERROR: clk sel not match */
	}
	BUG_ON(err);		/* clk sel not match */

	EXIT_FUNC(FUNC_LV_OP);
	return err;
}

static struct clkmux_ops glitch_free_wo_drain_cg_clkmux_ops = {
	.sel = glitch_free_wo_drain_cg_clkmux_sel_op,
	.get = general_clkmux_get_op,
};


static struct clkmux *id_to_mux(enum clkmux_id id)
{
	return id < NR_CLKMUXS ? &muxs[id] : NULL;
}

static void clkmux_sel_locked(struct clkmux *mux, enum cg_clk_id clksrc)
{
	ENTER_FUNC(FUNC_LV_LOCKED);
	mux->ops->sel(mux, clksrc);
	EXIT_FUNC(FUNC_LV_LOCKED);
}

static enum cg_clk_id clkmux_get_locked(struct clkmux *mux)
{
	ENTER_FUNC(FUNC_LV_LOCKED);

	EXIT_FUNC(FUNC_LV_LOCKED);
	return mux->ops->get(mux);
}

int clkmux_sel(enum clkmux_id id, enum cg_clk_id clksrc, const char *name)
{
	unsigned long flags;
	struct clkmux *mux = id_to_mux(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!initialized);
	BUG_ON(!mux);
	/* BUG_ON(clksrc >= mux->nr_inputs); // XXX: clksrc match is @ clkmux_sel_op() */

	clkmgr_lock(flags);
	clkmux_sel_locked(mux, clksrc);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return 0;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(clkmux_sel);

int clkmux_get(enum clkmux_id id, const char *name)
{
	enum cg_clk_id clksrc;
	unsigned long flags;
	struct clkmux *mux = id_to_mux(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!initialized);
	BUG_ON(!mux);
	/* BUG_ON(clksrc >= mux->nr_inputs); // XXX: clksrc match is @ clkmux_sel_op() */

	clkmgr_lock(flags);
	clksrc = clkmux_get_locked(mux);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return clksrc;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(clkmux_get);


/* PLL variable & function */

#define PLL_TYPE_SDM        0
#define PLL_TYPE_LC         1

#define HAVE_RST_BAR        (0x1 << 0)
#define HAVE_PLL_HP         (0x1 << 1)
#define HAVE_FIX_FRQ        (0x1 << 2)

#define PLL_EN              BIT(0)	/* @ XPLL_CON0 */
#define PLL_SDM_FRA_EN      BIT(4)	/* @ XPLL_CON0 */
#define RST_BAR_MASK        BITMASK(27 : 27)	/* @ XPLL_CON0 */

#define SDM_PLL_N_INFO_MASK BITMASK(20 : 0)	/* @ XPLL_CON1 (XPLL_SDM_PCW) */
#define SDM_PLL_POSDIV_MASK BITMASK(26 : 24)	/* @ XPLL_CON1 (XPLL_POSDIV) */
#define SDM_PLL_N_INFO_CHG  BIT(31)	/* @ XPLL_CON1 (XPLL_SDM_PCW_CHG) */

#define ARMPLL_POSDIV_MASK  BITMASK(26 : 24)	/* @ XPLL_CON1 (XPLL_POSDIV) */
#define PLL_FBKDIV_MASK     0x00007F00

#define PLL_PWR_ON          BIT(0)	/* @ XPLL_PWR */
#define PLL_ISO_EN          BIT(1)	/* @ XPLL_PWR */

static struct pll_ops sdm_pll_ops;

static struct pll_ops arm_pll_ops;
static struct pll_ops lc_pll_ops;
static struct pll_ops aud_pll_ops;
static struct pll_ops tvd_pll_ops;


static struct pll plls[NR_PLLS] = {
	[ARMPLL1] = {
		.name = __stringify(ARMPLL1),
		.type = PLL_TYPE_SDM,
		.feat = HAVE_PLL_HP,
		.en_mask = 0x80000001,	/* TODO: 8135: use a readable constant to replace it */
		.base_addr = ARMPLL_CON0,
		.pwr_addr = ARMPLL_PWR_CON0,
		.ops = &arm_pll_ops,
		.hp_id = MT658X_FH_ARM_PLL,
		.hp_switch = 0,
		},
	[ARMPLL2] = {
		.name = __stringify(ARMPLL2),
		.type = PLL_TYPE_SDM,
		.feat = HAVE_PLL_HP,
		.en_mask = 0x80000001,	/* TODO: 8135: use a readable constant to replace it */
		.base_addr = ARMPLL2_CON0,
		.pwr_addr = ARMPLL2_PWR_CON0,
		.ops = &arm_pll_ops,
		.hp_id = MT658X_FH_ARM_PLL2,
		.hp_switch = 0,
		},
	[MAINPLL] = {
		.name = __stringify(MAINPLL),
		.type = PLL_TYPE_SDM,
		.feat = HAVE_PLL_HP | HAVE_RST_BAR,
		.en_mask = 0xF0000001,	/* TODO: 8135: use a readable constant to replace it */
		.base_addr = MAINPLL_CON0,
		.pwr_addr = MAINPLL_PWR_CON0,
		.ops = &sdm_pll_ops,
		.hp_id = MT658X_FH_MAIN_PLL,
		.hp_switch = 0,
		},
	[UNIVPLL] = {
		.name = __stringify(UNIVPLL),
		.type = PLL_TYPE_LC,
		.feat = HAVE_RST_BAR | HAVE_FIX_FRQ,
		.en_mask = 0xF3000001,	/* TODO: 8135: review en_mask & lc_pll_ops */
		.base_addr = UNIVPLL_CON0,
		.pwr_addr = UNIVPLL_PWR_CON0,
		.ops = &lc_pll_ops,
		},
	[MMPLL] = {
		.name = __stringify(MMPLL),
		.type = PLL_TYPE_SDM,
		.feat = HAVE_RST_BAR | HAVE_PLL_HP,
		.en_mask = 0xF0000001,	/* TODO: 8135: use a readable constant to replace it */
		.base_addr = MMPLL_CON0,
		.pwr_addr = MMPLL_PWR_CON0,
		.ops = &sdm_pll_ops,
		.hp_id = MT658X_FH_MM_PLL,
		.hp_switch = 0,
		   },
	[MSDCPLL] = {
		.name = __stringify(MSDCPLL),
		.type = PLL_TYPE_SDM,
		.feat = HAVE_PLL_HP,
		.en_mask = 0x80000001,	/* TODO: 8135: use a readable constant to replace it */
		.base_addr = MSDCPLL_CON0,
		.pwr_addr = MSDCPLL_PWR_CON0,
		.ops = &sdm_pll_ops,
		.hp_id = MT658X_FH_MSDC_PLL,
		.hp_switch = 0,
		},
	[TVDPLL] = {
		.name = __stringify(TVDPLL),
		.type = PLL_TYPE_SDM,
		.feat = HAVE_PLL_HP,
		.en_mask = 0x80000001,	/* TODO: 8135: use a readable constant to replace it */
		.base_addr = TVDPLL_CON0,
		.pwr_addr = TVDPLL_PWR_CON0,
		.ops = &tvd_pll_ops,
		.hp_id = MT658X_FH_TVD_PLL,
		.hp_switch = 0,
		    },
	[LVDSPLL] = {
		.name = __stringify(LVDSPLL),
		.type = PLL_TYPE_SDM,
		.feat = HAVE_PLL_HP,
		.en_mask = 0x80000001,	/* TODO: 8135: use a readable constant to replace it */
		.base_addr = LVDSPLL_CON0,
		.pwr_addr = LVDSPLL_PWR_CON0,
		.ops = &sdm_pll_ops,
		.hp_id = MT658X_FH_LVDS_PLL,
		.hp_switch = 0,
		},
	[AUDPLL] = {
		.name = __stringify(AUDPLL),
		.type = PLL_TYPE_SDM,
		.feat = 0,
		.en_mask = 0x80000001,	/* TODO: 8135: use a readable constant to replace it */
		.base_addr = AUDPLL_CON0,
		.pwr_addr = AUDPLL_PWR_CON0,
		.ops = &aud_pll_ops,
		.hp_id = 0,
		.hp_switch = 0,
		    },
	[VDECPLL] = {
		.name = __stringify(VDECPLL),
		.type = PLL_TYPE_SDM,
		.feat = HAVE_PLL_HP,
		.en_mask = 0x80000001,	/* TODO: 8135: use a readable constant to replace it */
		.base_addr = VDECPLL_CON0,
		.pwr_addr = VDECPLL_PWR_CON0,
		.ops = &sdm_pll_ops,
		.hp_id = MT658X_FH_VDEC_PLL,
		.hp_switch = 0,
		},
};


static struct pll *id_to_pll(unsigned int id)
{
	return id < NR_PLLS ? &plls[id] : NULL;
}

static int pll_get_state_op(struct pll *pll)
{
	ENTER_FUNC(FUNC_LV_OP);

	EXIT_FUNC(FUNC_LV_OP);
	return clk_readl(pll->base_addr) & PLL_EN;
}

static void sdm_pll_enable_op(struct pll *pll)
{
	/* PWRON:1 -> ISOEN:0 -> EN:1 -> RSTB:1 */

	ENTER_FUNC(FUNC_LV_OP);

	clk_setl(pll->pwr_addr, PLL_PWR_ON);
	udelay(1);		/* XXX: 30ns is enough (diff from 89) */
	clk_clrl(pll->pwr_addr, PLL_ISO_EN);
	udelay(1);		/* XXX: 30ns is enough (diff from 89) */

	clk_setl(pll->base_addr, pll->en_mask);
	udelay(PLL_SETTLE_TIME);

	if (pll->feat & HAVE_RST_BAR)
		clk_setl(pll->base_addr, RST_BAR_MASK);

	EXIT_FUNC(FUNC_LV_OP);
}

static void sdm_pll_disable_op(struct pll *pll)
{
	/* RSTB:0 -> EN:0 -> ISOEN:1 -> PWRON:0 */

	ENTER_FUNC(FUNC_LV_OP);

	if (pll->feat & HAVE_RST_BAR)
		clk_clrl(pll->base_addr, RST_BAR_MASK);

	clk_clrl(pll->base_addr, PLL_EN);

	clk_setl(pll->pwr_addr, PLL_ISO_EN);
	clk_clrl(pll->pwr_addr, PLL_PWR_ON);

	EXIT_FUNC(FUNC_LV_OP);
}


static unsigned int sdm_pll_get_postdiv_op(struct pll *pll)
{
	unsigned int con0 = clk_readl(pll->base_addr);	/* PLL_CON0 */
	unsigned int posdiv = (con0 & BITMASK(8 : 6)) >> 6;
	/* bit[8:6] */

	ENTER_FUNC(FUNC_LV_OP);

	EXIT_FUNC(FUNC_LV_OP);
	return posdiv;
}


static void sdm_pll_set_postdiv_op(struct pll *pll, unsigned int postdiv)
{
	unsigned con0_addr = pll->base_addr;
	unsigned int con0_value = clk_readl(con0_addr);	/* PLL_CON0 */

	ENTER_FUNC(FUNC_LV_OP);

	clk_writel(con0_addr, (con0_value & ~BITMASK(8 : 6)) | BITS(8 : 6, postdiv));
	/* bit[8:6] */

	EXIT_FUNC(FUNC_LV_OP);
}


static unsigned int sdm_pll_get_pcw_op(struct pll *pll)
{
	unsigned int con1_addr = pll->base_addr + 4;
	unsigned int reg_value;

	ENTER_FUNC(FUNC_LV_OP);

	reg_value = clk_readl(con1_addr);	/* PLL_CON1 */
	reg_value &= SDM_PLL_N_INFO_MASK;	/* bit[20:0] */

	EXIT_FUNC(FUNC_LV_OP);
	return reg_value;
}


static void sdm_pll_set_pcw_op(struct pll *pll, unsigned int pcw)
{
	unsigned int con1_addr = pll->base_addr + 4;
	unsigned int reg_value;
	unsigned int pll_en;

	ENTER_FUNC(FUNC_LV_OP);

	pll_en = clk_readl(pll->base_addr) & PLL_EN;	/* PLL_CON0[0] */

	reg_value = clk_readl(con1_addr);
	reg_value &= ~SDM_PLL_N_INFO_MASK;
	reg_value |= pcw & SDM_PLL_N_INFO_MASK;	/* PLL_CON1[20:0] = pcw */

	if (pll_en)
		reg_value |= SDM_PLL_N_INFO_CHG;	/* PLL_CON1[31] = 1 */

	clk_writel(con1_addr, reg_value);

	if (pll_en)
		udelay(PLL_SETTLE_TIME);

	EXIT_FUNC(FUNC_LV_OP);
}


static void sdm_pll_fsel_op(struct pll *pll, unsigned int value)
{
	ENTER_FUNC(FUNC_LV_OP);
	sdm_pll_set_pcw_op(pll, value);
	EXIT_FUNC(FUNC_LV_OP);
}


static int sdm_pll_dump_regs_op(struct pll *pll, unsigned int *ptr)
{
	ENTER_FUNC(FUNC_LV_OP);

	*(ptr) = clk_readl(pll->base_addr);	/* XPLL_CON0 */
	*(++ptr) = clk_readl(pll->base_addr + 4);	/* XPLL_CON1 */
	*(++ptr) = clk_readl(pll->pwr_addr);	/* XPLL_PWR */

	EXIT_FUNC(FUNC_LV_OP);
	return 3;		/* return size */
}

static const unsigned int pll_vcodivsel_map[2] = { 1, 2 };
static const unsigned int pll_prediv_map[4] = { 1, 2, 4, 4 };
static const unsigned int pll_posdiv_map[8] = { 1, 2, 4, 8, 16, 16, 16, 16 };
static const unsigned int pll_fbksel_map[4] = { 1, 2, 4, 4 };

static const unsigned int pll_n_info_map[14] =	/* assume fin = 26MHz */
{
	13000000,
	6500000,
	3250000,
	1625000,
	812500,
	406250,
	203125,
	101563,
	50782,
	25391,
	12696,
	6348,
	3174,
	1587,
};


static unsigned int calc_pll_vco_freq(
	unsigned int fin,
	unsigned int pcw,
	unsigned int vcodivsel,
	unsigned int prediv,
	unsigned int pcw_f_bits)
{
	/* vco = (fin * pcw * vcodivsel / prediv) >> pcw_f_bits; */
	uint64_t vco = fin;
	vco = vco * pcw * vcodivsel;
	do_div(vco, prediv);
	vco >>= pcw_f_bits;
	return (unsigned int)vco;
}


static unsigned int sdm_pll_vco_calc_op(struct pll *pll)
{
	unsigned int con0 = clk_readl(pll->base_addr);
	unsigned int con1 = clk_readl(pll->base_addr + 4);

	unsigned int vcodivsel = (con0 & BIT(19)) >> 19;	/* bit[19] */
	unsigned int prediv = (con0 & BITMASK(5 : 4)) >> 4;
	/* bit[5:4] */
	unsigned int pcw = con1 & BITMASK(20 : 0);
	/* bit[20:0] */

	ENTER_FUNC(FUNC_LV_OP);

	vcodivsel = pll_vcodivsel_map[vcodivsel];
	prediv = pll_prediv_map[prediv];

	EXIT_FUNC(FUNC_LV_OP);
	return calc_pll_vco_freq(26000, pcw, vcodivsel, prediv, 14);	/* 26M = 26000K Hz */
}


static int general_pll_calc_freq(
	unsigned int *pcw,
	int *postdiv_idx,
	unsigned int freq,
	int pcwfbits)
{
	const uint64_t freq_max = 2000 * 1000 * 1000;	/* 2000 MHz */
	const uint64_t freq_min = 1000 * 1000 * 1000;	/* 1000 MHz */
	const uint32_t fin = 26 * 1000 * 1000;	/* 26 MHz */
	static const uint64_t postdiv[] = { 1, 2, 4, 8, 16 };
	uint64_t n_info;
	int idx;

	ENTER_FUNC(FUNC_LV_OP);

	/* search suitable postdiv */
	for (idx = 0; idx < ARRAY_SIZE(postdiv) && postdiv[idx] * freq <= freq_min; idx++)
		;

	if (idx >= ARRAY_SIZE(postdiv)) {
		EXIT_FUNC(FUNC_LV_OP);
		return -2;	/* freq is out of range (too low) */
	} else if (postdiv[idx] * freq > freq_max) {
		EXIT_FUNC(FUNC_LV_OP);
		return -3;	/* freq is out of range (too high) */
	}

	n_info = (postdiv[idx] * freq) << pcwfbits;	/* n_info = freq * postdiv / 26MHz * 2^pcwfbits */
	do_div(n_info, fin);

	*postdiv_idx = idx;
	*pcw = (unsigned int)n_info;

	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}


static int general_pll_set_freq_op(struct pll *pll, unsigned int freq, int pcwfbits)
{
	int r;
	unsigned int pcw = 0;
	int postdiv_idx = 0;

	ENTER_FUNC(FUNC_LV_OP);

	r = general_pll_calc_freq(&pcw, &postdiv_idx, freq, pcwfbits);

	if (r == 0) {
		pll->ops->set_postdiv(pll, postdiv_idx);
		pll->ops->set_pcw(pll, pcw);
	}

	EXIT_FUNC(FUNC_LV_OP);
	return r;
}


static int sdm_pll_set_freq_op(struct pll *pll, unsigned int freq)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return general_pll_set_freq_op(pll, freq, 14);
}


static int sdm_pll_hp_enable_op(struct pll *pll)
{
	int err;
	unsigned int vco;

	ENTER_FUNC(FUNC_LV_OP);

	if (!pll->hp_switch || (pll->state == PWR_DOWN)) {
		EXIT_FUNC(FUNC_LV_OP);
		return 0;
	}

	vco = pll->ops->vco_calc(pll);
	err = freqhopping_config(pll->hp_id, vco, 1);

	EXIT_FUNC(FUNC_LV_OP);
	return err;
}

static int sdm_pll_hp_disable_op(struct pll *pll)
{
	int err;
	unsigned int vco;

	ENTER_FUNC(FUNC_LV_OP);

	if (!pll->hp_switch || (pll->state == PWR_ON)) {	/* TODO: why PWR_ON */
		EXIT_FUNC(FUNC_LV_OP);
		return 0;
	}

	vco = pll->ops->vco_calc(pll);
	err = freqhopping_config(pll->hp_id, vco, 0);

	EXIT_FUNC(FUNC_LV_OP);
	return err;
}

static struct pll_ops sdm_pll_ops = {
	.get_state = pll_get_state_op,
	.enable = sdm_pll_enable_op,
	.disable = sdm_pll_disable_op,
	.fsel = sdm_pll_fsel_op,
	.dump_regs = sdm_pll_dump_regs_op,
	.vco_calc = sdm_pll_vco_calc_op,
	.hp_enable = sdm_pll_hp_enable_op,
	.hp_disable = sdm_pll_hp_disable_op,
	.get_postdiv = sdm_pll_get_postdiv_op,
	.set_postdiv = sdm_pll_set_postdiv_op,
	.get_pcw = sdm_pll_get_pcw_op,
	.set_pcw = sdm_pll_set_pcw_op,
	.set_freq = sdm_pll_set_freq_op,
};


#define PLL_FBKDIV_OFFSET   0x8


static unsigned int arm_pll_get_postdiv_op(struct pll *pll)
{
	unsigned int con1 = clk_readl(pll->base_addr + 4);	/* PLL_CON1 */
	unsigned int posdiv = (con1 & BITMASK(26 : 24)) >> 24;
	/* bit[26:24] */

	ENTER_FUNC(FUNC_LV_OP);

	EXIT_FUNC(FUNC_LV_OP);
	return posdiv;
}


static void arm_pll_set_postdiv_op(struct pll *pll, unsigned int postdiv)
{
	unsigned con1_addr = pll->base_addr + 4;
	unsigned int con1_value = clk_readl(con1_addr);	/* PLL_CON1 */

	ENTER_FUNC(FUNC_LV_OP);

	clk_writel(con1_addr, (con1_value & ~BITMASK(26 : 24)) | BITS(26 : 24, postdiv));
	/* bit[26:24] */

	EXIT_FUNC(FUNC_LV_OP);
}


static void arm_pll_fsel_op(struct pll *pll, unsigned int value)
{
	unsigned int con1_addr = pll->base_addr + 4;
	unsigned int mask = BITMASK(26 : 24) | BITMASK(20 : 0);
	unsigned int con1_value;
	unsigned int pll_en;

	ENTER_FUNC(FUNC_LV_OP);

	con1_value = clk_readl(con1_addr);	/* PLL_CON1 */
	pll_en = clk_readl(pll->base_addr) & PLL_EN;	/* PLL_CON0[0] */
	con1_value = (con1_value & ~mask) | (value & mask);	/* bit[26:24], bit[20:0] = value */

	if (pll_en)
		con1_value |= SDM_PLL_N_INFO_CHG;	/* PLL_CON1[31] = 1 */

	clk_writel(con1_addr, con1_value);

	if (pll_en)
		udelay(PLL_SETTLE_TIME);

	EXIT_FUNC(FUNC_LV_OP);
}


static int arm_pll_set_freq_op(struct pll *pll, unsigned int freq)
{
	const int pcwfbits = 14;
	int r;
	unsigned int pcw = 0;
	int postdiv_idx = 0;

	ENTER_FUNC(FUNC_LV_OP);

	r = general_pll_calc_freq(&pcw, &postdiv_idx, freq, pcwfbits);

	if (r == 0) {
		unsigned int reg_val = BITS(26 : 24, postdiv_idx) | BITS(20 : 0, pcw);
		arm_pll_fsel_op(pll, reg_val);
	}

	EXIT_FUNC(FUNC_LV_OP);
	return r;
}


static struct pll_ops arm_pll_ops = {
	.get_state = pll_get_state_op,
	.enable = sdm_pll_enable_op,
	.disable = sdm_pll_disable_op,
	.fsel = arm_pll_fsel_op,
	.dump_regs = sdm_pll_dump_regs_op,
	.vco_calc = sdm_pll_vco_calc_op,
	.hp_enable = sdm_pll_hp_enable_op,
	.hp_disable = sdm_pll_hp_disable_op,
	.get_postdiv = arm_pll_get_postdiv_op,
	.set_postdiv = arm_pll_set_postdiv_op,
	.get_pcw = sdm_pll_get_pcw_op,
	.set_pcw = sdm_pll_set_pcw_op,
	.set_freq = arm_pll_set_freq_op,
};


static void lc_pll_enable_op(struct pll *pll)
{
	ENTER_FUNC(FUNC_LV_OP);

	clk_setl(pll->base_addr, pll->en_mask);
	udelay(PLL_SETTLE_TIME);

	if (pll->feat & HAVE_RST_BAR)
		clk_setl(pll->base_addr, RST_BAR_MASK);

	EXIT_FUNC(FUNC_LV_OP);
}


static void lc_pll_disable_op(struct pll *pll)
{
	ENTER_FUNC(FUNC_LV_OP);

	if (pll->feat & HAVE_RST_BAR)
		clk_clrl(pll->base_addr, RST_BAR_MASK);

	clk_clrl(pll->base_addr, 0x1);

	EXIT_FUNC(FUNC_LV_OP);
}


static unsigned int lc_pll_get_pcw_op(struct pll *pll)
{
	/* get FBKDIV */
	unsigned int con0 = clk_readl(pll->base_addr);	/* PLL_CON0 */
	unsigned int fbkdiv = (con0 & BITMASK(15 : 9)) >> 9;
	/* bit[15:9] */

	ENTER_FUNC(FUNC_LV_OP);

	EXIT_FUNC(FUNC_LV_OP);
	return fbkdiv;
}


static void lc_pll_set_pcw_op(struct pll *pll, unsigned int pcw)
{
	/* set FBKDIV */
	unsigned int con0 = clk_readl(pll->base_addr);

	ENTER_FUNC(FUNC_LV_OP);

	con0 = (con0 & ~BITMASK(15 : 9)) | BITS(15 : 9, pcw);
	/* PLL_CON0[15:9] = pcw (fbkdiv) */
	clk_writel(pll->base_addr, con0);

	EXIT_FUNC(FUNC_LV_OP);
}


static void lc_pll_fsel_op(struct pll *pll, unsigned int value)
{
	unsigned int fbkdiv = (value & BITMASK(15 : 9)) >> 9;

	ENTER_FUNC(FUNC_LV_OP);

	if (pll->feat & HAVE_FIX_FRQ) {
		EXIT_FUNC(FUNC_LV_OP);
		return;
	}

	lc_pll_set_pcw_op(pll, fbkdiv);

	EXIT_FUNC(FUNC_LV_OP);
}


static int lc_pll_dump_regs_op(struct pll *pll, unsigned int *ptr)
{
	ENTER_FUNC(FUNC_LV_OP);

	*(ptr) = clk_readl(pll->base_addr);

	EXIT_FUNC(FUNC_LV_OP);
	return 1;
}


static unsigned int lc_pll_vco_calc_op(struct pll *pll)
{
	unsigned int vco = 0;
	unsigned int con0 = clk_readl(pll->base_addr);

	unsigned int fbksel = (con0 & BITMASK(21 : 20)) >> 20;
	/* bit[21:20] */
	unsigned int vcodivsel = (con0 & BIT(19)) >> 19;	/* bit[19] */
	unsigned int fbkdiv = (con0 & BITMASK(15 : 9)) >> 9;
	/* bit[15:9] */
	unsigned int prediv = (con0 & BITMASK(5 : 4)) >> 4;
	/* bit[5:4] */

	ENTER_FUNC(FUNC_LV_OP);

	vcodivsel = pll_vcodivsel_map[vcodivsel];
	fbksel = pll_fbksel_map[fbksel];
	prediv = pll_prediv_map[prediv];

	vco = 26000 * fbkdiv * fbksel * vcodivsel / prediv;

#if 0
	clk_info("[%s]%s: [0x%08x] vco=%uKHz\n", __func__, pll->name, con0, vco);
#endif

	EXIT_FUNC(FUNC_LV_OP);
	return vco;
}


static int lc_pll_set_freq_op(struct pll *pll, unsigned int freq)
{
	const uint64_t freq_max = 2000 * 1000 * 1000;	/* 2000 MHz */
	const uint64_t freq_min = 1000 * 1000 * 1000;	/* 1000 MHz */
	const uint32_t fin = 26 * 1000 * 1000;	/* 26 MHz */
	static const uint64_t postdiv[] = { 1, 2, 4, 8, 16 };
	uint64_t fbkdiv;
	int idx;

	ENTER_FUNC(FUNC_LV_OP);

	/* search suitable postdiv */
	for (idx = 0; idx < ARRAY_SIZE(postdiv) && postdiv[idx] * freq <= freq_min; idx++)
		;

	if (idx >= ARRAY_SIZE(postdiv)) {
		EXIT_FUNC(FUNC_LV_OP);
		return -2;	/* freq is out of range (too low) */
	} else if (postdiv[idx] * freq > freq_max) {
		EXIT_FUNC(FUNC_LV_OP);
		return -3;	/* freq is out of range (too high) */
	}

	fbkdiv = (postdiv[idx] * freq);	/* fbkdiv = freq * postdiv / 26MHz */
	do_div(fbkdiv, fin);

	if (fbkdiv * fin != postdiv[idx] * freq) {
		EXIT_FUNC(FUNC_LV_OP);
		return -4;	/* invalid freq */
	}

	pll->ops->set_postdiv(pll, idx);
	pll->ops->set_pcw(pll, fbkdiv);

	EXIT_FUNC(FUNC_LV_OP);
	return 0;
}


static struct pll_ops lc_pll_ops = {
	.get_state = pll_get_state_op,
	.enable = lc_pll_enable_op,
	.disable = lc_pll_disable_op,
	.fsel = lc_pll_fsel_op,
	.dump_regs = lc_pll_dump_regs_op,
	.vco_calc = lc_pll_vco_calc_op,
	.get_postdiv = sdm_pll_get_postdiv_op,
	.set_postdiv = sdm_pll_set_postdiv_op,
	.get_pcw = lc_pll_get_pcw_op,
	.set_pcw = lc_pll_set_pcw_op,
	.set_freq = lc_pll_set_freq_op,
};


static void aud_pll_enable_op(struct pll *pll)
{
	/* PWRON:1 -> ISOEN:0 -> EN:1 -> RSTB:1 */

	ENTER_FUNC(FUNC_LV_OP);

	clk_setl(pll->pwr_addr, PLL_PWR_ON);
	udelay(1);		/* XXX: 30ns is enough (diff from 89) */
	clk_clrl(pll->pwr_addr, PLL_ISO_EN);
	udelay(1);		/* XXX: 30ns is enough (diff from 89) */

	clk_setl(pll->base_addr, pll->en_mask);
	clk_setl(pll->base_addr + 16, BIT(31));	/* AUDPLL_CON4[31] (AUDPLL_TUNER_EN) = 1 */
	udelay(PLL_SETTLE_TIME);

	if (pll->feat & HAVE_RST_BAR)
		clk_setl(pll->base_addr, RST_BAR_MASK);

	EXIT_FUNC(FUNC_LV_OP);
}


static void aud_pll_disable_op(struct pll *pll)
{
	/* RSTB:0 -> EN:0 -> ISOEN:1 -> PWRON:0 */

	ENTER_FUNC(FUNC_LV_OP);

	if (pll->feat & HAVE_RST_BAR)
		clk_clrl(pll->base_addr, RST_BAR_MASK);

	clk_clrl(pll->base_addr + 16, BIT(31));	/* AUDPLL_CON4[31] (AUDPLL_TUNER_EN) = 0 */
	clk_clrl(pll->base_addr, PLL_EN);

	clk_setl(pll->pwr_addr, PLL_ISO_EN);
	clk_clrl(pll->pwr_addr, PLL_PWR_ON);

	EXIT_FUNC(FUNC_LV_OP);
}


static void aud_pll_set_pcw_op(struct pll *pll, unsigned int pcw)
{
	unsigned int con1_addr = pll->base_addr + 4;
	unsigned int con4_addr = pll->base_addr + 16;
	unsigned int reg_value;
	unsigned int pll_en;

	ENTER_FUNC(FUNC_LV_OP);

	pll_en = clk_readl(pll->base_addr) & PLL_EN;
	reg_value = clk_readl(con1_addr);		/* AUDPLL_CON1[30:0] (PLL_SDM_PCW) = pcw */
	reg_value = (reg_value & ~BITMASK(30 : 0)) | BITS(30 : 0, pcw);

	if (pll_en)
		reg_value |= SDM_PLL_N_INFO_CHG;	/* AUDPLL_CON1[31] (AUDPLL_SDM_PCW_CHG) = 1 "and" */
							/* AUDPLL_CON4[31] (AUDPLL_TUNER_EN) = 1 */

	clk_writel(con4_addr, reg_value + 1);		/* AUDPLL_CON4[30:0] (AUDPLL_TUNER_N_INFO) = (pcw + 1) */
	clk_writel(con1_addr, reg_value);		/* AUDPLL_CON1 */

	if (pll_en)
		udelay(PLL_SETTLE_TIME);

	EXIT_FUNC(FUNC_LV_OP);
}


static void aud_pll_fsel_op(struct pll *pll, unsigned int value)
{
	ENTER_FUNC(FUNC_LV_OP);
	aud_pll_set_pcw_op(pll, value);
	EXIT_FUNC(FUNC_LV_OP);
}


static unsigned int tvd_pll_get_pcw_op(struct pll *pll)
{
	unsigned int con1_addr = pll->base_addr + 4;
	unsigned int reg_value;

	ENTER_FUNC(FUNC_LV_OP);

	reg_value = clk_readl(con1_addr);   /* PLL_CON1 */
	reg_value &= BITMASK(30 : 0);         /* bit[30:0] */

	EXIT_FUNC(FUNC_LV_OP);
	return reg_value;
}


static void tvd_pll_set_pcw_op(struct pll *pll, unsigned int pcw)
{
	unsigned int con1_addr = pll->base_addr + 4;
	unsigned int reg_value;
	unsigned int pll_en;

	ENTER_FUNC(FUNC_LV_OP);

	pll_en = clk_readl(pll->base_addr) & PLL_EN;    /* PLL_CON0[0] */

	reg_value = clk_readl(con1_addr);               /* PLL_CON1[30:0] = pcw */
	reg_value = (reg_value & ~BITMASK(30 : 0)) | BITS(30 : 0, pcw);

	if (pll_en)
		reg_value |= SDM_PLL_N_INFO_CHG;        /* PLL_CON1[31] = 1 */

	clk_writel(con1_addr, reg_value);

	if (pll_en)
		udelay(PLL_SETTLE_TIME);

	EXIT_FUNC(FUNC_LV_OP);
}


static void tvd_pll_fsel_op(struct pll *pll, unsigned int value)
{
	ENTER_FUNC(FUNC_LV_OP);
	tvd_pll_set_pcw_op(pll, value);
	EXIT_FUNC(FUNC_LV_OP);
}


static unsigned int tvd_pll_vco_calc_op(struct pll *pll)
{
	unsigned int con0 = clk_readl(pll->base_addr);
	unsigned int con1 = clk_readl(pll->base_addr + 4);

	unsigned int vcodivsel = (con0 & BIT(19)) >> 19;	/* bit[19] */
	unsigned int prediv = (con0 & BITMASK(5 : 4)) >> 4;
	/* bit[5:4] */
	unsigned int pcw = con1 & BITMASK(30 : 0);
	/* bit[30:0] */

	ENTER_FUNC(FUNC_LV_OP);

	vcodivsel = pll_vcodivsel_map[vcodivsel];
	prediv = pll_prediv_map[prediv];

	EXIT_FUNC(FUNC_LV_OP);
	return calc_pll_vco_freq(26000, pcw, vcodivsel, prediv, 24);	/* 26M = 26000K Hz */
}


static int tvd_pll_set_freq_op(struct pll *pll, unsigned int freq)
{
	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);
	return general_pll_set_freq_op(pll, freq, 24);
}


static struct pll_ops aud_pll_ops = {
	.get_state = pll_get_state_op,
	.enable = aud_pll_enable_op,
	.disable = aud_pll_disable_op,
	.fsel = aud_pll_fsel_op,
	.dump_regs = sdm_pll_dump_regs_op,
	.vco_calc = tvd_pll_vco_calc_op,
	.get_postdiv = sdm_pll_get_postdiv_op,
	.set_postdiv = sdm_pll_set_postdiv_op,
	.get_pcw = tvd_pll_get_pcw_op,
	.set_pcw = aud_pll_set_pcw_op,
	.set_freq = tvd_pll_set_freq_op,
};


static struct pll_ops tvd_pll_ops = {
	.get_state = pll_get_state_op,
	.enable = sdm_pll_enable_op,
	.disable = sdm_pll_disable_op,
	.fsel = tvd_pll_fsel_op,
	.dump_regs = sdm_pll_dump_regs_op,
	.vco_calc = tvd_pll_vco_calc_op,
	.get_postdiv = sdm_pll_get_postdiv_op,
	.set_postdiv = sdm_pll_set_postdiv_op,
	.get_pcw = tvd_pll_get_pcw_op,
	.set_pcw = tvd_pll_set_pcw_op,
	.set_freq = tvd_pll_set_freq_op,
};


static unsigned int pll_freq_calc_op(struct pll *pll)
{
	return pll->ops->vco_calc(pll) / pll_posdiv_map[pll->ops->get_postdiv(pll)];
}


unsigned int pll_get_freq(enum pll_id id)
{
	unsigned int r;
	unsigned long flags;
	struct pll *pll = id_to_pll(id);
	BUG_ON(!pll);

	ENTER_FUNC(FUNC_LV_BODY);

	clkmgr_lock(flags);
	r = pll_freq_calc_op(pll);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_BODY);
	return r;
}
EXPORT_SYMBOL(pll_get_freq);


unsigned int pll_set_freq(enum pll_id id, unsigned int freq_khz)
{
	unsigned int r;
	unsigned long flags;
	struct pll *pll = id_to_pll(id);
	BUG_ON(!pll);

	ENTER_FUNC(FUNC_LV_BODY);

	clkmgr_lock(flags);
	r = pll->ops->set_freq(pll, freq_khz * 1000);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_BODY);
	return r;
}
EXPORT_SYMBOL(pll_set_freq);


static int get_pll_state_locked(struct pll *pll)
{
	ENTER_FUNC(FUNC_LV_LOCKED);

	if (likely(initialized)) {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return pll->state;	/* after init, get from local_state */
	} else {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return pll->ops->get_state(pll);	/* before init, get from reg_val */
	}
}

static int enable_pll_locked(struct pll *pll)
{
	ENTER_FUNC(FUNC_LV_LOCKED);

	pll->cnt++;

	if (pll->cnt > 1) {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return 0;
	}

	pll->ops->enable(pll);
	pll->state = PWR_ON;

	if (pll->ops->hp_enable && pll->feat & HAVE_PLL_HP)	/* enable freqnency hopping automatically */
		pll->ops->hp_enable(pll);

	EXIT_FUNC(FUNC_LV_LOCKED);
	return 0;
}

static int disable_pll_locked(struct pll *pll)
{
	ENTER_FUNC(FUNC_LV_LOCKED);

	BUG_ON(!pll->cnt);
	pll->cnt--;

	if (pll->cnt > 0) {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return 0;
	}

	pll->ops->disable(pll);
	pll->state = PWR_DOWN;

	if (pll->ops->hp_disable && pll->feat & HAVE_PLL_HP)	/* disable freqnency hopping automatically */
		pll->ops->hp_disable(pll);

	EXIT_FUNC(FUNC_LV_LOCKED);
	return 0;
}

static int pll_fsel_locked(struct pll *pll, unsigned int value)
{
	ENTER_FUNC(FUNC_LV_LOCKED);

	pll->ops->fsel(pll, value);

	if (pll->ops->hp_enable && pll->feat & HAVE_PLL_HP)
		pll->ops->hp_enable(pll);

	EXIT_FUNC(FUNC_LV_LOCKED);
	return 0;
}

int pll_is_on(enum pll_id id)
{
	int state;
	unsigned long flags;
	struct pll *pll = id_to_pll(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!pll);

	clkmgr_lock(flags);
	state = get_pll_state_locked(pll);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return state;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(pll_is_on);

int enable_pll(enum pll_id id, const char *name)
{
	int err;
	unsigned long flags;
	struct pll *pll = id_to_pll(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!initialized);
	BUG_ON(!pll);

	clkmgr_lock(flags);
	err = enable_pll_locked(pll);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return err;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(enable_pll);

int disable_pll(enum pll_id id, const char *name)
{
	int err;
	unsigned long flags;
	struct pll *pll = id_to_pll(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!initialized);
	BUG_ON(!pll);

	clkmgr_lock(flags);
	err = disable_pll_locked(pll);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return err;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(disable_pll);

int pll_fsel(enum pll_id id, unsigned int value)
{
	int err;
	unsigned long flags;
	struct pll *pll = id_to_pll(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!initialized);
	BUG_ON(!pll);

	clkmgr_lock(flags);
	err = pll_fsel_locked(pll, value);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return err;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(pll_fsel);


int pll_hp_switch_on(enum pll_id id, int hp_on)
{
	int err = 0;
	unsigned long flags;
	int old_value;
	struct pll *pll = id_to_pll(id);

	BUG_ON(!initialized);
	BUG_ON(!pll);

	if (pll->type != PLL_TYPE_SDM) {
		err = -EINVAL;
		goto out;
	}

	clkmgr_lock(flags);
	old_value = pll->hp_switch;
	if (old_value == 0) {
		pll->hp_switch = 1;
		if (hp_on)
			err = pll->ops->hp_enable(pll);
	}
	clkmgr_unlock(flags);

#if 0
	clk_info("[%s]hp_switch(%d->%d), hp_on=%d\n", __func__, old_value, pll->hp_switch, hp_on);
#endif

out:
	return err;
}
EXPORT_SYMBOL(pll_hp_switch_on);


int pll_dump_regs(enum pll_id id, unsigned int *ptr)
{
	struct pll *pll = id_to_pll(id);

	ENTER_FUNC(FUNC_LV_DONT_CARE);

	BUG_ON(!initialized);
	BUG_ON(!pll);

	EXIT_FUNC(FUNC_LV_DONT_CARE);
	return pll->ops->dump_regs(pll, ptr);
}
EXPORT_SYMBOL(pll_dump_regs);

const char *pll_get_name(enum pll_id id)
{
	struct pll *pll = id_to_pll(id);

	ENTER_FUNC(FUNC_LV_DONT_CARE);

	BUG_ON(!initialized);
	BUG_ON(!pll);

	EXIT_FUNC(FUNC_LV_DONT_CARE);
	return pll->name;
}

#if 0				/* XXX: NOT USED @ MT6572 */
#define CLKSQ1_EN           BIT(0)
#define CLKSQ1_LPF_EN       BIT(1)
#define CLKSQ1_EN_SEL       BIT(0)
#define CLKSQ1_LPF_EN_SEL   BIT(1)

void enable_clksq1(void)
{
	unsigned long flags;

	clkmgr_lock(flags);
	clk_setl(AP_PLL_CON0, CLKSQ1_EN);
	udelay(200);
	clk_setl(AP_PLL_CON0, CLKSQ1_LPF_EN);
	clkmgr_unlock(flags);
}
EXPORT_SYMBOL(enable_clksq1);

void disable_clksq1(void)
{
	unsigned long flags;

	clkmgr_lock(flags);
	clk_clrl(AP_PLL_CON0, CLKSQ1_LPF_EN | CLKSQ1_EN);
	clkmgr_unlock(flags);
}
EXPORT_SYMBOL(disable_clksq1);

void clksq1_sw2hw(void)
{
	unsigned long flags;

	clkmgr_lock(flags);
	clk_clrl(AP_PLL_CON1, CLKSQ1_LPF_EN_SEL | CLKSQ1_EN_SEL);
	clkmgr_unlock(flags);
}
EXPORT_SYMBOL(clksq1_sw2hw);

void clksq2_hw2sw(void)
{
	unsigned long flags;

	clkmgr_lock(flags);
	clk_setl(AP_PLL_CON1, CLKSQ1_LPF_EN_SEL | CLKSQ1_EN_SEL);
	clkmgr_unlock(flags);
}
EXPORT_SYMBOL(clksq1_hw2sw);
#endif				/* XXX: NOT USED @ MT6572 */


/* LARB related */

static DEFINE_MUTEX(larb_monitor_lock);
static LIST_HEAD(larb_monitor_handlers);

void register_larb_monitor(struct larb_monitor *handler)
{
	struct list_head *pos;

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

#else				/* CONFIG_CLKMGR_EMULATION */

	mutex_lock(&larb_monitor_lock);
	list_for_each(pos, &larb_monitor_handlers) {
		struct larb_monitor *l;
		l = list_entry(pos, struct larb_monitor, link);

		if (l->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	mutex_unlock(&larb_monitor_lock);

	EXIT_FUNC(FUNC_LV_API);

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(register_larb_monitor);


void unregister_larb_monitor(struct larb_monitor *handler)
{
	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

#else				/* CONFIG_CLKMGR_EMULATION */

	mutex_lock(&larb_monitor_lock);
	list_del(&handler->link);
	mutex_unlock(&larb_monitor_lock);

	EXIT_FUNC(FUNC_LV_API);

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(unregister_larb_monitor);


/* TODO: 8135: review LARB_CKEN_SET */

#define LARB_CKEN_SET       (VDEC_GCON_BASE + 0x0008)
#define LARB_CKEN_CLR       (VDEC_GCON_BASE + 0x000C)

/* TODO: 8135: use cg_clk->ops->enable(cg_clk) to replace writing register directly */

static void larb_clk_prepare(int larb_idx)
{
	switch (larb_idx) {
	case MT_LARB0:
		/* ven */
		clk_writel(VENCSYS_CG_SET, 0x1);
		break;
	case MT_LARB1:
		/* vde */
		clk_writel(LARB_CKEN_SET, 0x1);
		break;
	case MT_LARB2:
		/* display */
		clk_writel(DISP_CG_CLR0, 0x1);
		break;
	case MT_LARB3:
		/* isp */
		clk_writel(IMG_CG_CLR, 0x1);
		break;
	case MT_LARB4:
		/* isp */
		clk_writel(IMG_CG_CLR, 0x4);
		break;
	default:
		BUG();
	}
}

static void larb_clk_finish(int larb_idx)
{
	switch (larb_idx) {
	case MT_LARB0:
		/* ven */
		clk_writel(VENCSYS_CG_CLR, 0x1);
		break;
	case MT_LARB1:
		/* vde */
		clk_writel(LARB_CKEN_CLR, 0x1);
		break;
	case MT_LARB2:
		/* display */
		clk_writel(DISP_CG_SET0, 0x1);
		break;
	case MT_LARB3:
		/* isp */
		clk_writel(IMG_CG_SET, 0x1);
		break;
	case MT_LARB4:
		/* isp */
		clk_writel(IMG_CG_SET, 0x4);
		break;
	default:
		BUG();
	}
}


static void larb_backup(int larb_idx)
{
	struct larb_monitor *pos;

	ENTER_FUNC(FUNC_LV_BODY);

	/* clk_dbg("[%s]: start to backup larb%d\n", __func__, larb_idx); */
	larb_clk_prepare(larb_idx);

	list_for_each_entry(pos, &larb_monitor_handlers, link) {
		if (pos->backup != NULL)
			pos->backup(pos, larb_idx);
	}

	larb_clk_finish(larb_idx);

	EXIT_FUNC(FUNC_LV_BODY);
}

static void larb_restore(int larb_idx)
{
	struct larb_monitor *pos;

	ENTER_FUNC(FUNC_LV_BODY);

	/* clk_dbg("[%s]: start to restore larb%d\n", __func__, larb_idx); */
	larb_clk_prepare(larb_idx);

	list_for_each_entry(pos, &larb_monitor_handlers, link) {
		if (pos->restore != NULL)
			pos->restore(pos, larb_idx);
	}

	larb_clk_finish(larb_idx);

	EXIT_FUNC(FUNC_LV_BODY);
}


/* SUBSYS variable & function */

#define SYS_TYPE_MODEM    0
#define SYS_TYPE_MEDIA    1
#define SYS_TYPE_OTHER    2

static struct subsys_ops general_sys_ops;
#if 0
static struct subsys_ops md1_sys_ops;
static struct subsys_ops con_sys_ops;
#endif
static struct subsys_ops dis_sys_ops;
static struct subsys_ops mfg_sys_ops;

#define MD1_PROT_MASK BITMASK(10 : 7)


static struct subsys_ops isp_sys_ops;
static struct subsys_ops ven_sys_ops;
static struct subsys_ops vde_sys_ops;

#define TOPAXI_SI0_CTL              (INFRACFG_BASE + 0x0200)
#define INFRA_TOPAXI_PROTECTEN      (INFRACFG_BASE + 0x0220)
#define INFRA_TOPAXI_PROTECTSTA1    (INFRACFG_BASE + 0x0228)

#define PWR_RST_B_BIT               BIT(0)	/* @ SPM_XXX_PWR_CON */
#define PWR_ISO_BIT                 BIT(1)	/* @ SPM_XXX_PWR_CON */
#define PWR_ON_BIT                  BIT(2)	/* @ SPM_XXX_PWR_CON */
#define PWR_ON_S_BIT                BIT(3)	/* @ SPM_XXX_PWR_CON */
#define PWR_CLK_DIS_BIT             BIT(4)	/* @ SPM_XXX_PWR_CON */
#define SRAM_CKISO_BIT              BIT(5)	/* @ SPM_FC0_PWR_CON or SPM_CPU_PWR_CON */
#define SRAM_ISOINT_B_BIT           BIT(6)	/* @ SPM_FC0_PWR_CON or SPM_CPU_PWR_CON */
#define SRAM_PDN_BITS               BITMASK(11 : 8)	/* @ SPM_XXX_PWR_CON */
#define SRAM_PDN_ACK_BITS           BITMASK(15 : 12)	/* @ SPM_XXX_PWR_CON */

#define DISP_PWR_STA_MASK           BIT(3)
#define MFG_3D_PWR_STA_MASK         BIT(4)
#define ISP_PWR_STA_MASK            BIT(5)
#define INFRA_PWR_STA_MASK          BIT(6)
#define VDEC_PWR_STA_MASK           BIT(7)
#define VEN_PWR_STA_MASK            BIT(14)
#define MFG_2D_PWR_STA_MASK         BIT(22)


static struct subsys syss[] =	/* NR_SYSS */
{
	[SYS_VDE] = {
		.name = __stringify(SYS_VDE),
		.type = SYS_TYPE_MEDIA,
		.default_sta = PWR_DOWN,
		.sta_mask = VDEC_PWR_STA_MASK,
		.ctl_addr = SPM_VDE_PWR_CON,
		.sram_pdn_bits = BITMASK(11 : 8),
		.sram_pdn_ack_bits = BITMASK(12 : 12),
		.bus_prot_mask = 0,
		.si0_way_en_mask = 0,
		.clksrc = &clks[MT_CG_TOP_PDN_VDEC],
		.ops = &vde_sys_ops,
		.start = &grps[CG_VDEC0],
		.nr_grps = 2,
		},
	[SYS_MFG_2D] = {
		.name = __stringify(SYS_MFG_2D),
		.type = SYS_TYPE_MEDIA,
		.default_sta = PWR_DOWN,
		.sta_mask = MFG_2D_PWR_STA_MASK,
		.ctl_addr = SPM_MFG_2D_PWR_CON,
		.sram_pdn_bits = BITMASK(9 : 8),
		.sram_pdn_ack_bits = BITMASK(21 : 20),
		.bus_prot_mask = BIT(5) | BIT(8) | BIT(9) | BIT(14),
		.si0_way_en_mask = BIT(10),
		.clksrc = NULL,
		.ops = &mfg_sys_ops,
		.start = &grps[CG_MFG],
		.nr_grps = 0,
		},
	[SYS_MFG] = {
		.name = __stringify(SYS_MFG),
		.type = SYS_TYPE_MEDIA,
		.default_sta = PWR_DOWN,
		.sta_mask = MFG_3D_PWR_STA_MASK,
		.ctl_addr = SPM_MFG_PWR_CON,
		.sram_pdn_bits = BITMASK(13 : 8),
		.sram_pdn_ack_bits = BITMASK(25 : 20),
		.bus_prot_mask = 0,	/* bus protection will be enabled when turn off MFG_2D */
		.si0_way_en_mask = 0,
		.clksrc = NULL,
		.ops = &mfg_sys_ops,
		.start = &grps[CG_MFG],
		.nr_grps = 0,
		},
	[SYS_VEN] = {
		.name = __stringify(SYS_VEN),
		.type = SYS_TYPE_MEDIA,
		.default_sta = PWR_DOWN,
		.sta_mask = VEN_PWR_STA_MASK,
		.ctl_addr = SPM_VEN_PWR_CON,
		.sram_pdn_bits = BITMASK(11 : 8),
		.sram_pdn_ack_bits = BITMASK(15 : 12),
		.bus_prot_mask = 0,
		.si0_way_en_mask = 0,
		.clksrc = &clks[MT_CG_TOP_PDN_VENC],
		.ops = &ven_sys_ops,
		.start = &grps[CG_VENC],
		.nr_grps = 1,
		},
	[SYS_ISP] = {
		.name = __stringify(SYS_ISP),
		.type = SYS_TYPE_MEDIA,
		.default_sta = PWR_ON,	/* TODO: 8135: 89 default off? */
		.sta_mask = ISP_PWR_STA_MASK,
		.ctl_addr = SPM_ISP_PWR_CON,
		.sram_pdn_bits = BITMASK(11 : 8),
		.sram_pdn_ack_bits = BITMASK(13 : 12),
		.bus_prot_mask = 0,
		.si0_way_en_mask = 0,
		.clksrc = NULL,
		.ops = &isp_sys_ops,
		.start = &grps[CG_ISP],
		.nr_grps = 1,
		},
	[SYS_DIS] = {
		.name = __stringify(SYS_DIS),
		.type = SYS_TYPE_MEDIA,
		.default_sta = PWR_ON,
		.sta_mask = DISP_PWR_STA_MASK,
		.ctl_addr = SPM_DIS_PWR_CON,
		.sram_pdn_bits = BITMASK(11 : 8),
		.sram_pdn_ack_bits = 0,
		.bus_prot_mask = 0,
		.si0_way_en_mask = 0,
		.clksrc = &clks[MT_CG_TOP_PDN_DISP],
		.ops = &dis_sys_ops,
		.start = &grps[CG_DISP0],
		.nr_grps = 2,
		},
	[SYS_IFR] = {
		.name = __stringify(SYS_IFR),
		.type = SYS_TYPE_OTHER,
		.default_sta = PWR_ON,
		.sta_mask = INFRA_PWR_STA_MASK,
		.ctl_addr = SPM_IFR_PWR_CON,
		.sram_pdn_bits = BITMASK(11 : 8),
		.sram_pdn_ack_bits = BITMASK(15 : 12),
		.bus_prot_mask = 0,
		.si0_way_en_mask = 0,
		.clksrc = NULL,
		.ops = &general_sys_ops,
		.start = &grps[CG_INFRA],
		.nr_grps = 3,
		},
};


static struct subsys *id_to_sys(unsigned int id)
{
	return id < NR_SYSS ? &syss[id] : NULL;
}


void ms_mtcmos(unsigned long long timestamp, unsigned char cnt, unsigned int *value)
{
	/* TODO: 8135: implement mtcoms info */
}

void print_mtcmos_trace_info_for_met(void)
{
	/* TODO: 8135: implement mtcoms info */
}


static int spm_mtcmos_power_off_general_locked(struct subsys *sys, int wait_power_ack, int ext_power_delay)
{
	int i;
	int err = 0;
	unsigned long expired = jiffies + HZ / 10;

	ENTER_FUNC(FUNC_LV_API);

	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	/* BUS_PROTECT */
	if (0 != sys->bus_prot_mask) {
		CHIP_SW_VER ver = mt_get_chip_sw_ver();

		if (ver == CHIP_SW_VER_01) {
			/* TODO: 8135: this is not a general flow (only can be used in MFG_2D), refine it! */
			unsigned int mfg_ctl = syss[SYS_MFG].ctl_addr;
			spm_write(mfg_ctl, spm_read(mfg_ctl) & ~PWR_CLK_DIS_BIT);	/* MFG (3D) PWR_CLK_DIS = 0 */
			spm_write(mfg_ctl, spm_read(mfg_ctl) & ~PWR_ISO_BIT);		/* MFG (3D) PWR_ISO = 0 */
			spm_write(mfg_ctl, spm_read(mfg_ctl) | PWR_RST_B_BIT);		/* MFG (3D) PWR_RST_B = 1 */
		}

		clk_setl(MFG_CONFIG_BASE + 0x0010, BIT(29));	/* enable BMEM_ASYNC_FREE_RUN  */

		spm_write(INFRA_TOPAXI_PROTECTEN, spm_read(INFRA_TOPAXI_PROTECTEN) | sys->bus_prot_mask);
		while ((spm_read(INFRA_TOPAXI_PROTECTSTA1) & sys->bus_prot_mask) !=
		       sys->bus_prot_mask) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;	/* TODO: 8135: err = ? */
			}
		}

		clk_clrl(MFG_CONFIG_BASE + 0x0010, BIT(29));	/* disable BMEM_ASYNC_FREE_RUN */

		if (ver == CHIP_SW_VER_01) {
			unsigned int mfg_ctl = syss[SYS_MFG].ctl_addr;
			spm_write(mfg_ctl, spm_read(mfg_ctl) | PWR_ISO_BIT);		/* MFG (3D) PWR_ISO = 1 */
			spm_write(mfg_ctl, spm_read(mfg_ctl) & ~PWR_RST_B_BIT);		/* MFG (3D) PWR_RST_B = 0 */
			spm_write(mfg_ctl, spm_read(mfg_ctl) | PWR_CLK_DIS_BIT);	/* MFG (3D) PWR_CLK_DIS = 1 */
		}
	}

	if (0 != sys->si0_way_en_mask)
		spm_write(TOPAXI_SI0_CTL, spm_read(TOPAXI_SI0_CTL) & ~sys->si0_way_en_mask);

	/* SRAM_PDN */
	for (i = BIT(8); i <= sys->sram_pdn_bits; i = (i << 1) + BIT(8)) {	/* set SRAM_PDN 1 one by one */
		spm_write(sys->ctl_addr, spm_read(sys->ctl_addr) | i);
	}

	/* wait until SRAM_PDN_ACK all 1 */
	while (sys->sram_pdn_ack_bits &&
	       ((spm_read(sys->ctl_addr) & sys->sram_pdn_ack_bits) != sys->sram_pdn_ack_bits)) {
		if (time_after(jiffies, expired)) {
			WARN_ON(1);
			break;	/* TODO: 8135: err = ? */
		}
	}

	spm_write(sys->ctl_addr, spm_read(sys->ctl_addr) | PWR_ISO_BIT);	/* PWR_ISO = 1 */
	spm_write(sys->ctl_addr, spm_read(sys->ctl_addr) & ~PWR_RST_B_BIT);	/* PWR_RST_B = 0 */
	spm_write(sys->ctl_addr, spm_read(sys->ctl_addr) | PWR_CLK_DIS_BIT);	/* PWR_CLK_DIS = 1 */

	spm_write(sys->ctl_addr, spm_read(sys->ctl_addr) & ~(PWR_ON_BIT));	/* PWR_ON = 0 */
	udelay(1);		/* delay 1 us */
	spm_write(sys->ctl_addr, spm_read(sys->ctl_addr) & ~(PWR_ON_S_BIT));	/* PWR_ON_S = 0 */
	udelay(1);		/* delay 1 us */

	if (ext_power_delay > 0) {	/* extra delay after power off */
		udelay(ext_power_delay);
	}

	if (wait_power_ack) {
		/* wait until PWR_ACK = 0 */
		while ((spm_read(SPM_PWR_STATUS) & sys->sta_mask)
		       || (spm_read(SPM_PWR_STATUS_S) & sys->sta_mask)) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;	/* TODO: 8135: err = ? */
			}
		}
	}

	EXIT_FUNC(FUNC_LV_API);
	return err;
}


static int spm_mtcmos_power_on_general_locked(struct subsys *sys, int wait_power_ack,
						int ext_power_delay)
{
	int i;
	int err = 0;
	unsigned long expired = jiffies + HZ / 10;

	ENTER_FUNC(FUNC_LV_API);

	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	spm_write(sys->ctl_addr, spm_read(sys->ctl_addr) | PWR_ON_BIT);	/* PWR_ON = 1 */
	udelay(1);		/* delay 1 us */
	spm_write(sys->ctl_addr, spm_read(sys->ctl_addr) | PWR_ON_S_BIT);	/* PWR_ON_S = 1 */
	udelay(3);		/* delay 3 us */

	if (ext_power_delay > 0) {	/* extra delay after power on */
		udelay(ext_power_delay);
	}

	if (wait_power_ack) {
		/* wait until PWR_ACK = 1 */
		while (!(spm_read(SPM_PWR_STATUS) & sys->sta_mask)
		       || !(spm_read(SPM_PWR_STATUS_S) & sys->sta_mask)) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;	/* TODO: 8135: err = ? */
			}
		}
	}

	spm_write(sys->ctl_addr, spm_read(sys->ctl_addr) & ~PWR_CLK_DIS_BIT);	/* PWR_CLK_DIS = 0 */
	spm_write(sys->ctl_addr, spm_read(sys->ctl_addr) & ~PWR_ISO_BIT);	/* PWR_ISO = 0 */
	spm_write(sys->ctl_addr, spm_read(sys->ctl_addr) | PWR_RST_B_BIT);	/* PWR_RST_B = 1 */

	/* SRAM_PDN */
	for (i = BIT(8); i <= sys->sram_pdn_bits; i = (i << 1) + BIT(8)) {	/* set SRAM_PDN 0 one by one */
		spm_write(sys->ctl_addr, spm_read(sys->ctl_addr) & ~i);
	}

	/* wait until SRAM_PDN_ACK all 0 */
	while (sys->sram_pdn_ack_bits && (spm_read(sys->ctl_addr) & sys->sram_pdn_ack_bits)) {
		if (time_after(jiffies, expired)) {
			WARN_ON(1);
			break;	/* TODO: 8135: err = ? */
		}
	}

	/* BUS_PROTECT */
	if (0 != sys->bus_prot_mask) {
		CHIP_SW_VER ver = mt_get_chip_sw_ver();

		if (ver == CHIP_SW_VER_01) {
			/* TODO: 8135: this is not a general flow (only can be used in MFG_2D), refine it! */
			unsigned int mfg_ctl = syss[SYS_MFG].ctl_addr;
			spm_write(mfg_ctl, spm_read(mfg_ctl) & ~PWR_CLK_DIS_BIT);	/* MFG (3D) PWR_CLK_DIS = 0 */
			spm_write(mfg_ctl, spm_read(mfg_ctl) & ~PWR_ISO_BIT);		/* MFG (3D) PWR_ISO = 0 */
			spm_write(mfg_ctl, spm_read(mfg_ctl) | PWR_RST_B_BIT);		/* MFG (3D) PWR_RST_B = 1 */
		}

		clk_setl(MFG_CONFIG_BASE + 0x0010, BIT(29));	/* enable BMEM_ASYNC_FREE_RUN */

		spm_write(INFRA_TOPAXI_PROTECTEN, spm_read(INFRA_TOPAXI_PROTECTEN) & ~sys->bus_prot_mask);
		while (spm_read(INFRA_TOPAXI_PROTECTSTA1) & sys->bus_prot_mask) {
			if (time_after(jiffies, expired)) {
				WARN_ON(1);
				break;	/* TODO: 8135: err = ? */
			}
		}

		clk_clrl(MFG_CONFIG_BASE + 0x0010, BIT(29));	/* disable BMEM_ASYNC_FREE_RUN */

		if (ver == CHIP_SW_VER_01) {
			unsigned int mfg_ctl = syss[SYS_MFG].ctl_addr;
			spm_write(mfg_ctl, spm_read(mfg_ctl) | PWR_ISO_BIT);		/* MFG (3D) PWR_ISO = 1 */
			spm_write(mfg_ctl, spm_read(mfg_ctl) & ~PWR_RST_B_BIT);		/* MFG (3D) PWR_RST_B = 0 */
			spm_write(mfg_ctl, spm_read(mfg_ctl) | PWR_CLK_DIS_BIT);	/* MFG (3D) PWR_CLK_DIS = 1 */
		}
	}

	if (0 != sys->si0_way_en_mask)
		spm_write(TOPAXI_SI0_CTL, spm_read(TOPAXI_SI0_CTL) | sys->si0_way_en_mask);

	EXIT_FUNC(FUNC_LV_API);
	return err;
}


static int spm_mtcmos_ctrl_general_locked(struct subsys *sys, int state)
{
	int err = 0;

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)
#else

	if (state == STA_POWER_DOWN) {
		err = spm_mtcmos_power_off_general_locked(sys, 1, 0);
	} else {		/* STA_POWER_ON */

		err = spm_mtcmos_power_on_general_locked(sys, 1, 0);
	}

	print_mtcmos_trace_info_for_met();	/* XXX: for MET */

	/* spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (0U << 0)); */

#endif				/* defined(CONFIG_CLKMGR_EMULATION) */

	EXIT_FUNC(FUNC_LV_API);
	return err;
}


static int spm_mtcmos_ctrl_mfg_locked(struct subsys *sys, int state)
{
	int err = 0;

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)
#else

	if (state == STA_POWER_DOWN) {
		err = spm_mtcmos_power_off_general_locked(sys, 1, 10);
	} else {		/* STA_POWER_ON */

		err = spm_mtcmos_power_on_general_locked(sys, 0, 10);
	}

	print_mtcmos_trace_info_for_met();	/* XXX: for MET */

	/* spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (0U << 0)); */

#endif				/* defined(CONFIG_CLKMGR_EMULATION) */

	EXIT_FUNC(FUNC_LV_API);
	return err;
}


static int general_sys_enable_op(struct subsys *sys)
{
	int err;

	ENTER_FUNC(FUNC_LV_OP);

	err = spm_mtcmos_ctrl_general_locked(sys, STA_POWER_ON);

	EXIT_FUNC(FUNC_LV_OP);
	return err;
}

static int general_sys_disable_op(struct subsys *sys)
{
	int err;

	ENTER_FUNC(FUNC_LV_OP);

	err = spm_mtcmos_ctrl_general_locked(sys, STA_POWER_DOWN);

	EXIT_FUNC(FUNC_LV_OP);
	return err;
}

#if 0
static int md1_sys_enable_op(struct subsys *sys)
{
	int err;

	ENTER_FUNC(FUNC_LV_OP);

	err = spm_mtcmos_ctrl_general_locked(sys, STA_POWER_ON);

	EXIT_FUNC(FUNC_LV_OP);
	return err;
}

static int md1_sys_disable_op(struct subsys *sys)
{
	int err;

	ENTER_FUNC(FUNC_LV_OP);

	err = spm_mtcmos_ctrl_general_locked(sys, STA_POWER_DOWN);

	EXIT_FUNC(FUNC_LV_OP);
	return err;
}

static int con_sys_enable_op(struct subsys *sys)
{
	int err;

	ENTER_FUNC(FUNC_LV_OP);

	err = spm_mtcmos_ctrl_general_locked(sys, STA_POWER_ON);

	EXIT_FUNC(FUNC_LV_OP);
	return err;
}

static int con_sys_disable_op(struct subsys *sys)
{
	int err;

	ENTER_FUNC(FUNC_LV_OP);

	err = spm_mtcmos_ctrl_general_locked(sys, STA_POWER_DOWN);

	EXIT_FUNC(FUNC_LV_OP);
	return err;
}
#endif


static int mfg_sys_enable_op(struct subsys *sys)
{
	int err;

	ENTER_FUNC(FUNC_LV_OP);

	err = spm_mtcmos_ctrl_mfg_locked(sys, STA_POWER_ON);

	EXIT_FUNC(FUNC_LV_OP);
	return err;
}


static int mfg_sys_disable_op(struct subsys *sys)
{
	int err;

	ENTER_FUNC(FUNC_LV_OP);

	err = spm_mtcmos_ctrl_mfg_locked(sys, STA_POWER_DOWN);

	EXIT_FUNC(FUNC_LV_OP);
	return err;
}


static int dis_sys_enable_op(struct subsys *sys)
{
	int err;
	err = general_sys_enable_op(sys);

#if CLKMGR_DISP_ROT_WORKAROUND
	/* disable ROT clock after MTCMOS power on */
	clk_writel(DISP_CG_SET0, DISP_ROT_DISP_BIT);
#endif				/* CLKMGR_DISP_ROT_WORKAROUND */

	larb_restore(MT_LARB2);
	return err;
}


static int dis_sys_disable_op(struct subsys *sys)
{
	int err;
	larb_backup(MT_LARB2);
	err = general_sys_disable_op(sys);
	return err;
}


static int isp_sys_enable_op(struct subsys *sys)
{
	int err;
	err = general_sys_enable_op(sys);
	larb_restore(MT_LARB3);
	larb_restore(MT_LARB4);
	return err;
}


static int isp_sys_disable_op(struct subsys *sys)
{
	int err;
	larb_backup(MT_LARB3);
	larb_backup(MT_LARB4);
	err = general_sys_disable_op(sys);
	return err;
}


static int ven_sys_enable_op(struct subsys *sys)
{
	int err;
	err = general_sys_enable_op(sys);
	larb_restore(MT_LARB0);
	return err;
}


static int ven_sys_disable_op(struct subsys *sys)
{
	int err;
	larb_backup(MT_LARB0);
	err = general_sys_disable_op(sys);
	return err;
}


static int vde_sys_enable_op(struct subsys *sys)
{
	int err;
	err = general_sys_enable_op(sys);
	larb_restore(MT_LARB1);
	return err;
}


static int vde_sys_disable_op(struct subsys *sys)
{
	int err;
	larb_backup(MT_LARB1);
	err = general_sys_disable_op(sys);
	return err;
}


static int sys_get_state_op(struct subsys *sys)
{
	unsigned int sta = clk_readl(SPM_PWR_STATUS);
	unsigned int sta_s = clk_readl(SPM_PWR_STATUS_S);

	ENTER_FUNC(FUNC_LV_OP);
	EXIT_FUNC(FUNC_LV_OP);

	return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);
}

static int sys_dump_regs_op(struct subsys *sys, unsigned int *ptr)
{
	ENTER_FUNC(FUNC_LV_OP);
	*(ptr) = clk_readl(sys->ctl_addr);

	EXIT_FUNC(FUNC_LV_OP);
	return 1;		/* return size */
}

static struct subsys_ops general_sys_ops = {
	.enable = general_sys_enable_op,
	.disable = general_sys_disable_op,
	.get_state = sys_get_state_op,
	.dump_regs = sys_dump_regs_op,
};

#if 0
static struct subsys_ops md1_sys_ops = {
	.enable = md1_sys_enable_op,
	.disable = md1_sys_disable_op,
	.get_state = sys_get_state_op,
	.dump_regs = sys_dump_regs_op,
};

static struct subsys_ops con_sys_ops = {
	.enable = con_sys_enable_op,
	.disable = con_sys_disable_op,
	.get_state = sys_get_state_op,
	.dump_regs = sys_dump_regs_op,
};
#endif


static struct subsys_ops mfg_sys_ops = {
	.enable = mfg_sys_enable_op,
	.disable = mfg_sys_disable_op,
	.get_state = sys_get_state_op,
	.dump_regs = sys_dump_regs_op,
};


static struct subsys_ops isp_sys_ops = {
	.enable = isp_sys_enable_op,
	.disable = isp_sys_disable_op,
	.get_state = sys_get_state_op,
	.dump_regs = sys_dump_regs_op,
};


static struct subsys_ops ven_sys_ops = {
	.enable = ven_sys_enable_op,
	.disable = ven_sys_disable_op,
	.get_state = sys_get_state_op,
	.dump_regs = sys_dump_regs_op,
};


static struct subsys_ops vde_sys_ops = {
	.enable = vde_sys_enable_op,
	.disable = vde_sys_disable_op,
	.get_state = sys_get_state_op,
	.dump_regs = sys_dump_regs_op,
};


static struct subsys_ops dis_sys_ops = {
	.enable = dis_sys_enable_op,
	.disable = dis_sys_disable_op,
	.get_state = sys_get_state_op,
	.dump_regs = sys_dump_regs_op,
};


static int get_sys_state_locked(struct subsys *sys)
{
	ENTER_FUNC(FUNC_LV_LOCKED);

	if (likely(initialized)) {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return sys->state;	/* after init, get from local_state */
	} else {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return sys->ops->get_state(sys);	/* before init, get from reg_val */
	}
}

int subsys_is_on(enum subsys_id id)
{
	int state;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!sys);

	clkmgr_lock(flags);
	state = get_sys_state_locked(sys);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return state;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(subsys_is_on);

static int enable_subsys_locked(struct subsys *sys)
{
	int err;
	int local_state = sys->state;	/* get_subsys_local_state(sys); */

	ENTER_FUNC(FUNC_LV_LOCKED);

#ifdef STATE_CHECK_DEBUG
	int reg_state = sys->ops->get_state(sys);	/* get_subsys_reg_state(sys); */
	BUG_ON(local_state != reg_state);
#endif

	if (local_state == PWR_ON) {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return 0;	/* XXX: ??? just check local_state ??? */
	}

	if (sys->clksrc)
		enable_clock_locked(sys->clksrc);	/* enable top clock before power on subsys */

	/* err = sys->pwr_ctrl(STA_POWER_ON); */
	err = sys->ops->enable(sys);
	WARN_ON(err);

	if (!err)
		sys->state = PWR_ON;
	else {
		if (sys->clksrc)
			disable_clock_locked(sys->clksrc);	/* release top clock if power on fail */
	}

	EXIT_FUNC(FUNC_LV_LOCKED);
	return err;
}

static int disable_subsys_locked(struct subsys *sys, int force_off)
{
	int err;
	int local_state = sys->state;	/* get_subsys_local_state(sys); */
	int i;
	const struct cg_grp *grp;

	ENTER_FUNC(FUNC_LV_LOCKED);

#ifdef STATE_CHECK_DEBUG
	int reg_state = sys->ops->get_state(sys);	/* get_subsys_reg_state(sys); */
	BUG_ON(local_state != reg_state);
#endif

	if (!force_off) {	/* XXX: check all clock gate groups related to this subsys are off */
		/* could be power off or not */
		for (i = 0; i < sys->nr_grps; i++) {
			grp = sys->start + i;

			if (grp->state) {
				EXIT_FUNC(FUNC_LV_LOCKED);
				return 0;
			}
		}
	}

	if (local_state == PWR_DOWN) {
		EXIT_FUNC(FUNC_LV_LOCKED);
		return 0;
	}
	/* err = sys->pwr_ctrl(STA_POWER_DOWN); */
	err = sys->ops->disable(sys);
	WARN_ON(err);

	if (!err)
		sys->state = PWR_DOWN;

	if (!err && sys->clksrc)
		disable_clock_locked(sys->clksrc);	/* release top clock after power down subsys */

	EXIT_FUNC(FUNC_LV_LOCKED);
	return err;
}

int enable_subsys(enum subsys_id id, const char *name)
{
	int err;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!initialized);
	BUG_ON(!sys);

	clkmgr_lock(flags);
	err = enable_subsys_locked(sys);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return err;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(enable_subsys);

int disable_subsys(enum subsys_id id, const char *name)
{
	int err;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!initialized);
	BUG_ON(!sys);

	clkmgr_lock(flags);
	err = disable_subsys_locked(sys, 0);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return err;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(disable_subsys);

int disable_subsys_force(enum subsys_id id, const char *name)
{
	int err;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);

	ENTER_FUNC(FUNC_LV_API);

#if defined(CONFIG_CLKMGR_EMULATION)

	return 0;

#else				/* CONFIG_CLKMGR_EMULATION */

	BUG_ON(!initialized);
	BUG_ON(!sys);

	clkmgr_lock(flags);
	err = disable_subsys_locked(sys, 1);
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_API);
	return err;

#endif				/* CONFIG_CLKMGR_EMULATION */
}
EXPORT_SYMBOL(disable_subsys_force);

int subsys_dump_regs(enum subsys_id id, unsigned int *ptr)
{
	struct subsys *sys = id_to_sys(id);

	ENTER_FUNC(FUNC_LV_DONT_CARE);

	BUG_ON(!initialized);
	BUG_ON(!sys);

	EXIT_FUNC(FUNC_LV_DONT_CARE);
	return sys->ops->dump_regs(sys, ptr);
}
EXPORT_SYMBOL(subsys_dump_regs);

const char *subsys_get_name(enum subsys_id id)
{
	struct subsys *sys = id_to_sys(id);

	ENTER_FUNC(FUNC_LV_DONT_CARE);

	BUG_ON(!initialized);
	BUG_ON(!sys);

	EXIT_FUNC(FUNC_LV_DONT_CARE);
	return sys->name;
}


#if 0				/* XXX: NOT USED @ MT6572 */
#define PMICSPI_CLKMUX_MASK 0x7
#define PMICSPI_MEMPLL_D4   0x5
#define PMICSPI_CLKSQ       0x0
void pmicspi_mempll2clksq(void)
{
	unsigned int val;
	val = clk_readl(CLK_CFG_8);
	/* BUG_ON((val & PMICSPI_CLKMUX_MASK) != PMICSPI_MEMPLL_D4); */

	val = (val & ~PMICSPI_CLKMUX_MASK) | PMICSPI_CLKSQ;
	clk_writel(CLK_CFG_8, val);
}
EXPORT_SYMBOL(pmicspi_mempll2clksq);

void pmicspi_clksq2mempll(void)
{
	unsigned int val;
	val = clk_readl(CLK_CFG_8);

	val = (val & ~PMICSPI_CLKMUX_MASK) | PMICSPI_MEMPLL_D4;
	clk_writel(CLK_CFG_8, val);
}
EXPORT_SYMBOL(pmicspi_clksq2mempll);
#endif				/* XXX: NOT USED @ MT6572 */


unsigned int mt_get_emi_freq(void)
{
	unsigned int reg_value;
	unsigned int output_freq = 0;

	ENTER_FUNC(FUNC_LV_API);

	reg_value = (clk_readl(DDRPHY_BASE + 0x0618) & BITMASK(8 : 2)) >> 2;
	if (reg_value == 0x5)
		output_freq = 266 * 1000;	/* Khz */
	else if (reg_value == 0xA)
		output_freq = 533 * 1000;	/* Khz */
	else if (reg_value == 0xC)
		output_freq = 666 * 1000;	/* Khz */
	else if (reg_value == 0xF)
		output_freq = 793 * 1000;	/* Khz */
	else
		output_freq = 0;

	output_freq /= 2;	/* emi = mempll / 2 */

	EXIT_FUNC(FUNC_LV_API);
	return output_freq;
}
EXPORT_SYMBOL(mt_get_emi_freq);


unsigned int mt_get_bus_freq(void)
{
	unsigned int output_freq = 0;
	enum cg_clk_id clksrc = clkmux_get(MT_CLKMUX_AXI_SEL, "clkmgr");

	switch (clksrc) {
	case MT_CG_V_CKSQ_MUX_CK:
		output_freq = 26000;
		break;

	case MT_CG_V_SYSPLL_D3:
		/* SYSPLL_D3 = SYSPLL / 3 = MAINPLL / 2 / 3 */
		output_freq = pll_freq_calc_op(&plls[MAINPLL]) / 6;
		break;

	case MT_CG_V_SYSPLL_D4:
		/* SYSPLL_D4 = SYSPLL / 4 = MAINPLL / 2 / 4 */
		output_freq = pll_freq_calc_op(&plls[MAINPLL]) / 8;
		break;

	case MT_CG_V_SYSPLL_D6:
		/* SYSPLL_D6 = SYSPLL / 6 = MAINPLL / 2 / 6 */
		output_freq = pll_freq_calc_op(&plls[MAINPLL]) / 12;
		break;

	case MT_CG_V_UNIVPLL_D5:
		/* UNIVPLL_D5 = UNIVPLL / 5 */
		output_freq = pll_freq_calc_op(&plls[UNIVPLL]) / 5;
		break;

	case MT_CG_V_UNIVPLL2_D2:
		/* UNIVPLL2_D2 = UNIVPLL2 / 2 = UNIVPLL / 3 / 2 */
		output_freq = pll_freq_calc_op(&plls[UNIVPLL]) / 6;
		break;

	case MT_CG_V_SYSPLL_D3P5:
		/* SYSPLL_D3P5 = SYSPLL / 3.5 = MAINPLL / 2 / 3.5 */
		output_freq = pll_freq_calc_op(&plls[MAINPLL]) / 7;
		break;

	default:
		BUG_ON(1);	/* unexpect clock source of AXI */
		break;

	}

	return output_freq;
}
EXPORT_SYMBOL(mt_get_bus_freq);


/* INIT related */

static void dump_clk_info(void);


#if CLKMGR_BRINGUP

static void cg_all_force_on(void)
{
	ENTER_FUNC(FUNC_LV_BODY);

	clk_setl(MAINPLL_CON0, CG_MAINPLL_MASK);
	clk_setl(UNIVPLL_CON0, CG_UNIVPLL_MASK);
	clk_setl(MMPLL_CON0, CG_MMPLL_MASK);
	clk_clrl(CLK_CFG_0, CG_TOP0_MASK);
	clk_clrl(CLK_CFG_1, CG_TOP1_MASK);
	clk_clrl(CLK_CFG_2, CG_TOP2_MASK);
	clk_clrl(CLK_CFG_3, CG_TOP3_MASK);
	clk_clrl(CLK_CFG_4, CG_TOP4_MASK);
	clk_clrl(CLK_CFG_6, CG_TOP6_MASK);
	clk_clrl(CLK_CFG_7, CG_TOP7_MASK);
	clk_clrl(CLK_CFG_8, CG_TOP8_MASK);
	clk_clrl(CLK_CFG_9, CG_TOP9_MASK);
	clk_writel(INFRA_PDN1, CG_INFRA_MASK);
	clk_writel(PERI_PDN0_CLR, CG_PERI0_MASK);
	clk_writel(PERI_PDN1_CLR, CG_PERI1_MASK);
	clk_writel(MFG_PD_CLR, CG_MFG_MASK);
	clk_writel(IMG_CG_CLR, CG_ISP_MASK);
	clk_writel(VENCSYS_CG_SET, CG_VENC_MASK);
	clk_writel(VDEC_CKEN_SET, CG_VDEC0_MASK);
	clk_writel(VDEC_LARB_CKEN_SET, CG_VDEC1_MASK);
	clk_writel(DISP_CG_CLR0, CG_DISP0_MASK);
	clk_writel(DISP_CG_CLR1, CG_DISP1_MASK);
	clk_setl(AUDIO_TOP_CON0, CG_AUDIO_MASK);

	EXIT_FUNC(FUNC_LV_BODY);
}

#endif				/* CLKMGR_BRINGUP */


#if CLKMGR_BRINGUP

static void pll_all_force_on(void)
{
	int i;
	struct pll *pll;

	ENTER_FUNC(FUNC_LV_API);

	for (i = 0; i < NR_PLLS; i++) {
		pll = &plls[i];
		if (!pll->ops->get_state(pll))
			enable_pll_locked(pll);
	}

	EXIT_FUNC(FUNC_LV_API);
}


static void subsys_all_default_on(void)
{
	int i;

	ENTER_FUNC(FUNC_LV_API);

	for (i = 0; i < NR_SYSS; i++) {
		if (i == SYS_MFG || i == SYS_MFG_2D)
			continue;

		syss[i].default_sta = PWR_ON;
	}

	EXIT_FUNC(FUNC_LV_API);
}


#endif				/* CLKMGR_BRINGUP */


unsigned int get_real_cg_state(enum cg_clk_id clkid)
{
	struct cg_clk *clk = id_to_clk(clkid);

	if (clkid > NR_CLKS)	/* special CGs */
		return get_clk_state_locked(&clks[clkid]);
	else if (!clk)
		return PWR_DOWN;
	else if (clk->grp != &grps[CG_VIRTUAL])
		return get_clk_state_locked(clk);
	else if (clk->src == MT_CG_V_PLL || clk->src == MT_CG_V_MUX)
		return get_clk_state_locked(clk);
	else
		return get_real_cg_state(clk->src);
}


static void disable_clock_range_locked(enum cg_clk_id from, enum cg_clk_id to)
{
	struct cg_clk *clk;
	enum cg_clk_id clkid;

	for (clkid = from; clkid <= to; ++clkid) {
		clk = &clks[clkid];

		if (clk->state == PWR_ON) {
			/* decrease ref_count. clk may be disabled if all its downstream clocks are disabled */
			disable_clock_locked(clk);
		}
	}
}


static void mt_subsys_init(void)
{
	int i;
	struct subsys *sys;

	ENTER_FUNC(FUNC_LV_API);

#if 0
	if (test_spm_gpu_power_on()) {	/* XXX: PRESET (mfg force on) */
		sys = id_to_sys(SYS_MFG);
		sys->default_sta = PWR_ON;
	} else
		clk_warn("[%s]: not force to turn on MFG\n", __func__);
#endif

	for (i = 0; i < NR_SYSS; i++) {
		sys = &syss[i];
		sys->state = sys->ops->get_state(sys);

		if (sys->state == sys->default_sta)
			continue;
		/* XXX: state not match and use default_state (sync with cg_clk[]?) */
		clk_info("[%s]%s, change state: (%u->%u)\n",
			__func__,
			sys->name,
			sys->state,
			sys->default_sta);

		if (sys->default_sta == PWR_DOWN)
			disable_subsys_locked(sys, 1);
		else
			enable_subsys_locked(sys);
	}

	EXIT_FUNC(FUNC_LV_API);
}

static void mt_plls_init(void)
{
	int i;
	struct pll *pll;

	ENTER_FUNC(FUNC_LV_API);

	for (i = 0; i < NR_PLLS; i++) {
		pll = &plls[i];
		pll->state = pll->ops->get_state(pll);
	}

	EXIT_FUNC(FUNC_LV_API);
}


#if 0				/* refer to mt_clkmgr_init() */

static void mt_plls_enable_hp(void)
{
	int i;
	struct pll *pll;

	ENTER_FUNC(FUNC_LV_API);

	for (i = 0; i < NR_PLLS; i++) {
		pll = &plls[i];

		if (pll->ops->hp_enable && pll->feat & HAVE_PLL_HP)
			pll->ops->hp_enable(pll);
	}

	EXIT_FUNC(FUNC_LV_API);
}

#endif				/* 0 // mt_clkmgr_init() */


static void mt_clks_init(void)
{
	int i;
	struct cg_grp *grp;
	struct cg_clk *clk;
	struct clkmux *clkmux;

	ENTER_FUNC(FUNC_LV_API);

	/* init CG_CLK */
	for (i = 0; i < NR_CLKS; i++) {
		clk = &clks[i];
		grp = clk->grp;

		if (NULL != grp) {	/* XXX: CG_CLK always map one of CL_GRP */
			grp->mask |= clk->mask;	/* XXX: init cg_grp mask by cg_clk mask */
			clk->state = clk->ops->get_state(clk);
			clk->cnt = (PWR_ON == clk->state) ? 1 : 0;
		} else
			BUG();	/* XXX: CG_CLK always map one of CL_GRP */
	}

	/* init virtual CGs in CG_CLK */
	for (i = FROM_CG_GRP_ID(CG_VIRTUAL); i <= TO_CG_GRP_ID(CG_VIRTUAL); i++) {
		clk = &clks[i];

		if (clk->src == MT_CG_V_MUX) {	/* a MUX drain VCG should be always on */
			clk->state = PWR_ON;
			clk->cnt = 2;	/* it will be decreased by 1 later */
		} else if (clk->src == MT_CG_V_PLL) {	/* sync PLL VCG state with PLL's state */
			if (clk->parent && get_pll_state_locked(clk->parent)) {
				clk->state = PWR_ON;
				clk->cnt = 1;
				enable_pll_locked(clk->parent);
			}
		}
	}

#if CLKMGR_PLL_VCG
	for (i = FROM_CG_GRP_ID(CG_MAINPLL); i <= TO_CG_GRP_ID(CG_MMPLL); i++) {
		clk = &clks[i];

		clk->state = get_real_cg_state(i);
		clk->cnt = (PWR_ON == clk->state) ? 1 : 0;
	}
#endif				/* CLKMGR_PLL_VCG */

	for (i = FROM_CG_GRP_ID(CG_VIRTUAL); i <= TO_CG_GRP_ID(CG_VIRTUAL); i++) {
		clk = &clks[i];

		clk->state = get_real_cg_state(i);
		clk->cnt = (PWR_ON == clk->state) ? 1 : 0;
	}

	/* init CG_GRP */
	for (i = 0; i < NR_GRPS; i++) {
		grp = &grps[i];
		grp->state = grp->ops->get_state(grp);
	}

	/* init CLKMUX (link clk src + clkmux + clk drain) */
	for (i = 0; i < NR_CLKMUXS; i++) {
		enum cg_clk_id src_id;

		clkmux = &muxs[i];

		src_id = clkmux->ops->get(clkmux);

		/* clk_info("clkmux[%d] %d -> %d\n", i, src_id, clkmux->drain); // <-XXX */

		clk = id_to_clk(clkmux->drain);	/* clk (drain) */

		if (NULL != clk) {
			clk->src = src_id;

			if (0) {
				clk = id_to_clk(src_id);	/* clk (source) */
				if (NULL != clk)
					enable_clock_locked(clk);
			}
		} else {	/* wo drain */
			clk = id_to_clk(src_id);	/* clk (source) */

			if (NULL != clk)	/* clk (source) */
				enable_clock_locked(clk);
		}
	}

	/* init CG_CLK again (construct clock tree dependency) */
	for (i = 0; i < NR_CLKS; i++) {
		clk = &clks[i];

		if (PWR_ON == clk->state) {
			BUG_ON((clk->src < NR_CLKS) && (NULL != clk->parent));

			if (clk->src < NR_CLKS) {
				clk = id_to_clk(clk->src);	/* clk (source) */
				if (NULL != clk)
					enable_clock_locked(clk);
			}
		}
	}

	/* default on clocks and PLLs */
	enable_pll_locked(&plls[MAINPLL]);
	enable_pll_locked(&plls[UNIVPLL]);

	if (get_pll_state_locked(&plls[ARMPLL1]))
		enable_pll_locked(&plls[ARMPLL1]);	/* hold ref_count if it is enabled */

	if (get_pll_state_locked(&plls[ARMPLL2]))
		enable_pll_locked(&plls[ARMPLL2]);	/* hold ref_count if it is enabled */

	enable_clock_locked(id_to_clk(MT_CG_TOP_PDN_MEM));
	enable_clock_locked(id_to_clk(MT_CG_TOP_PDN_CCI));

#if CLKMGR_WORKAROUND
	enable_clock_locked(id_to_clk(MT_CG_TOP_PDN_FIX));
	enable_clock_locked(id_to_clk(MT_CG_TOP_PDN_DDRPHYCFG));
#endif				/* CLKMGR_WORKAROUND */

#if AUDIO_WORKAROUND == 1
	enable_clock_locked(id_to_clk(MT_CG_TOP_PDN_AUDIO));
#endif				/* AUDIO_WORKAROUND == 1 */

#if CLKMGR_BRINGUP
	enable_clock_locked(id_to_clk(MT_CG_TOP_PDN_UART));
	enable_clock_locked(id_to_clk(MT_CG_TOP_PDN_DPILVDS));
#endif				/* CLKMGR_BRINGUP */

	/* decrease ref_count */
	disable_clock_range_locked(FROM_CG_GRP_ID(CG_TOP9), TO_CG_GRP_ID(CG_TOP9));
	disable_clock_range_locked(FROM_CG_GRP_ID(CG_TOP8), TO_CG_GRP_ID(CG_TOP8));
	disable_clock_range_locked(FROM_CG_GRP_ID(CG_TOP7), TO_CG_GRP_ID(CG_TOP7));
	disable_clock_range_locked(FROM_CG_GRP_ID(CG_TOP6), TO_CG_GRP_ID(CG_TOP6));
	disable_clock_range_locked(FROM_CG_GRP_ID(CG_TOP4), TO_CG_GRP_ID(CG_TOP4));
	disable_clock_range_locked(FROM_CG_GRP_ID(CG_TOP3), TO_CG_GRP_ID(CG_TOP3));
	disable_clock_range_locked(FROM_CG_GRP_ID(CG_TOP2), TO_CG_GRP_ID(CG_TOP2));
	disable_clock_range_locked(FROM_CG_GRP_ID(CG_TOP1), TO_CG_GRP_ID(CG_TOP1));
	disable_clock_range_locked(FROM_CG_GRP_ID(CG_TOP0), TO_CG_GRP_ID(CG_TOP0));

	disable_clock_range_locked(FROM_CG_GRP_ID(CG_VIRTUAL), TO_CG_GRP_ID(CG_VIRTUAL));

#if CLKMGR_PLL_VCG
	disable_clock_range_locked(FROM_CG_GRP_ID(CG_MAINPLL), TO_CG_GRP_ID(CG_MAINPLL));
	disable_clock_range_locked(FROM_CG_GRP_ID(CG_UNIVPLL), TO_CG_GRP_ID(CG_UNIVPLL));
	disable_clock_range_locked(FROM_CG_GRP_ID(CG_MMPLL), TO_CG_GRP_ID(CG_MMPLL));
#endif				/* CLKMGR_PLL_VCG */

	for (i = 0; i < NR_SYSS; i++) {
		struct subsys *sys = &syss[i];

		if (sys->state == PWR_ON && sys->clksrc)
			enable_clock_locked(sys->clksrc);	/* hold top clock if this subsys is on */
	}

	EXIT_FUNC(FUNC_LV_API);
}

#if 0
static void mt_md1_cg_init(void)
{
	clk_clrl(PERI_PDN_MD_MASK, 1U << 0);
	clk_writel(PERI_PDN0_MD1_SET, 0xFFFFFFFF);
}
#endif


int mt_clkmgr_init(void)
{
	ENTER_FUNC(FUNC_LV_API);

#if !defined(CONFIG_CLKMGR_EMULATION)

	BUG_ON(initialized);

#if CLKMGR_BRINGUP
	subsys_all_default_on();
#endif				/* CLKMGR_BRINGUP */

	mt_subsys_init();

#if CLKMGR_BRINGUP
	cg_all_force_on();	/* XXX: for bring up */
#endif				/* CLKMGR_BRINGUP */

	mt_clks_init();

#if CLKMGR_BRINGUP
	pll_all_force_on();	/* XXX: for bring up */
#endif				/* CLKMGR_BRINGUP */

	mt_plls_init();

#if defined(CONFIG_CLKMGR_SHOWLOG)
	dump_clk_info();
#endif

#endif				/* !defined(CONFIG_CLKMGR_EMULATION) */

	initialized = 1;

	mt_freqhopping_init();
#if 0				/* TODO: FIXME */
	mt_plls_enable_hp();
#endif				/* TODO: FIXME */

	EXIT_FUNC(FUNC_LV_API);
	return 0;
}


int mt_clkmgr_bringup_init(void)
{
	return mt_clkmgr_init();
}


#if !defined(CONFIG_MMC_MTK) && !defined(CONFIG_MTK_MMC_LEGACY)
void msdc_clk_status(int *status)
{
	*status = 0;
}
#endif


/* IDLE related */

bool clkmgr_idle_can_enter(unsigned int *condition_mask, unsigned int *block_mask)
{
	int i;
	unsigned int cg_mask = 0;
	bool cg_fail = 0;

	ENTER_FUNC(FUNC_LV_API);

	for (i = 0; i < NR_GRPS; i++) {
		cg_mask = grps[i].state & condition_mask[i];

		if (cg_mask) {
			block_mask[i] |= cg_mask;
			EXIT_FUNC(FUNC_LV_API);
			cg_fail = 1;
		}
	}

	if (cg_fail)
		return false;

	EXIT_FUNC(FUNC_LV_API);
	return true;
}


/* Golden setting */

enum print_mode {
	MODE_NORMAL,
	MODE_COMPARE,
	MODE_APPLY,
	MODE_COLOR,
	MODE_DIFF,
};

struct golden_setting {
	unsigned int addr;
	unsigned int mask;
	unsigned int golden_val;
};

struct snapshot {
	const char *func;
	unsigned int line;
	unsigned int reg_val[1];	/* XXX: actually variable length */
};

struct golden {
	unsigned int is_golden_log;

	enum print_mode mode;

	char func[64];		/* TODO: check the size is OK or not */
	unsigned int line;

	unsigned int *buf;
	unsigned int buf_size;

	struct golden_setting *buf_golden_setting;
	unsigned int nr_golden_setting;
	unsigned int max_nr_golden_setting;

	struct snapshot *buf_snapshot;
	unsigned int max_nr_snapshot;
	unsigned int snapshot_head;
	unsigned int snapshot_tail;
};

#define SIZEOF_SNAPSHOT(g) (sizeof(struct snapshot) + sizeof(unsigned int) * (g->nr_golden_setting - 1))

static struct golden _golden;

static void _golden_setting_enable(struct golden *g)
{
	if (NULL == g)
		return;

	g->buf_snapshot = (struct snapshot *)&(g->buf_golden_setting[g->nr_golden_setting]);
	g->max_nr_snapshot =
		(g->buf_size -
		 sizeof(struct golden_setting) * g->nr_golden_setting) / SIZEOF_SNAPSHOT(g);
	g->snapshot_head = 0;
	g->snapshot_tail = 0;	/* TODO: check it */

	g->is_golden_log = TRUE;
}

static void _golden_setting_disable(struct golden *g)
{
	if (NULL != g) {
		g->is_golden_log = FALSE;

		g->func[0] = '\0';

		g->buf_golden_setting = (struct golden_setting *)g->buf;
		g->nr_golden_setting = 0;
		g->max_nr_golden_setting = g->buf_size / 3 / sizeof(struct golden_setting);	/* TODO: refine it */
	}
}

static void _golden_setting_set_mode(struct golden *g, enum print_mode mode)
{
	g->mode = mode;
}

static void _golden_setting_init(struct golden *g, unsigned int *buf, unsigned int buf_size)
{
	if (NULL != g && NULL != buf) {
		g->mode = MODE_NORMAL;

		g->buf = buf;
		g->buf_size = buf_size;

		_golden_setting_disable(g);
	}
}

static void _golden_setting_add(
	struct golden *g,
	unsigned int addr,
	unsigned int mask,
	unsigned golden_val)
{
	if (NULL != g
	    && FALSE == g->is_golden_log && g->nr_golden_setting < g->max_nr_golden_setting) {
		g->buf_golden_setting[g->nr_golden_setting].addr = addr;
		g->buf_golden_setting[g->nr_golden_setting].mask = mask;
		g->buf_golden_setting[g->nr_golden_setting].golden_val = golden_val;

		g->nr_golden_setting++;
	}
}

static bool _is_pmic_addr(unsigned int addr)
{
	return (addr & 0xF0000000) ? FALSE : TRUE;
}

static void _golden_write_reg(unsigned int addr, unsigned int mask, unsigned int reg_val)
{
	if (_is_pmic_addr(addr))
		pmic_config_interface(addr, reg_val, mask, 0x0);
	else {
		*((unsigned int *)IO_PHYS_TO_VIRT(addr)) =
		    (*((unsigned int *)IO_PHYS_TO_VIRT(addr)) & ~mask) | (reg_val & mask);
	}
}

static unsigned int _golden_read_reg(unsigned int addr)
{
	unsigned int reg_val;

	if (_is_pmic_addr(addr))
		pmic_read_interface(addr, &reg_val, 0xFFFFFFFF, 0x0);
	else
		reg_val = *((unsigned int *)IO_PHYS_TO_VIRT(addr));

	return reg_val;
}

static int _is_snapshot_full(struct golden *g)
{
	if (g->snapshot_head + 1 == g->snapshot_tail
	    || g->snapshot_head + 1 == g->snapshot_tail + g->max_nr_snapshot)
		return 1;
	else
		return 0;
}

static int _is_snapshot_empty(struct golden *g)
{
	if (g->snapshot_head == g->snapshot_tail)
		return 1;
	else
		return 0;
}

static struct snapshot *_snapshot_produce(struct golden *g)
{
	if (NULL != g && !_is_snapshot_full(g)) {
		int idx = g->snapshot_head++;

		if (g->snapshot_head == g->max_nr_snapshot)
			g->snapshot_head = 0;

		return (struct snapshot *)((int)(g->buf_snapshot) + SIZEOF_SNAPSHOT(g) * idx);
	} else
		return NULL;
}

static struct snapshot *_snapshot_consume(struct golden *g)
{
	if (NULL != g && !_is_snapshot_empty(g)) {
		int idx = g->snapshot_tail++;

		if (g->snapshot_tail == g->max_nr_snapshot)
			g->snapshot_tail = 0;

		return (struct snapshot *)((int)(g->buf_snapshot) + SIZEOF_SNAPSHOT(g) * idx);
	} else
		return NULL;
}

static int _snapshot_golden_setting(struct golden *g, const char *func, const unsigned int line)
{
	struct snapshot *snapshot;
	int i;

	if (NULL != g
	    && TRUE == g->is_golden_log
	    && (g->func[0] == '\0'
	    || (!strcmp(g->func, func) && ((g->line == line) || (g->line == 0))))
	    ) {
		snapshot = _snapshot_produce(g);
		if (NULL == snapshot)
			return -1;

		snapshot->func = func;
		snapshot->line = line;

		for (i = 0; i < g->nr_golden_setting; i++) {
			if (MODE_APPLY == _golden.mode) {
				_golden_write_reg(
					g->buf_golden_setting[i].addr,
					g->buf_golden_setting[i].mask,
					g->buf_golden_setting[i].golden_val);
			}

			snapshot->reg_val[i] = _golden_read_reg(g->buf_golden_setting[i].addr);
		}

		return 0;
	} else {
		/* printf("[Err]: buffer full or not enabled\n"); */

		return -1;
	}
}

int snapshot_golden_setting(const char *func, const unsigned int line)
{
	return _snapshot_golden_setting(&_golden, func, line);
}

static int _parse_mask_val(char *buf, unsigned int *mask, unsigned int *golden_val)
{
	unsigned int i, bit_shift;
	unsigned int mask_result;
	unsigned int golden_val_result;

	for (i = 0, bit_shift = 1 << 31, mask_result = 0, golden_val_result = 0; bit_shift > 0;) {
		switch (buf[i]) {
		case '1':
			golden_val_result += bit_shift;
		case '0':
			mask_result += bit_shift;
		case 'x':
		case 'X':
			bit_shift >>= 1;
		case '_':
			break;
		default:
			return -1;
		}
		i++;
	}

	*mask = mask_result;
	*golden_val = golden_val_result;

	return 0;
}

static char *_gen_mask_str(
	const unsigned int mask,
	const unsigned int reg_val,
	char *_mask_str)
{
	unsigned int i, bit_shift;

	for (i = 2, bit_shift = 1 << 31; bit_shift > 0;) {
		switch (_mask_str[i]) {
		case '_':
			break;
		default:
			if (0 == (mask & bit_shift))
				_mask_str[i] = 'x';
			else if (0 == (reg_val & bit_shift))
				_mask_str[i] = '0';
			else
				_mask_str[i] = '1';
		case '\0':
			bit_shift >>= 1;
			break;
		}

		i++;
	}

	return _mask_str;
}

static char *_gen_diff_str(
	const unsigned int mask,
	const unsigned int golden_val,
	const unsigned int reg_val,
	char *_diff_str)
{
	unsigned int i, bit_shift;

	for (i = 2, bit_shift = 1 << 31; bit_shift > 0;) {
		switch (_diff_str[i]) {
		case '_':
			break;
		default:
			if (0 != ((golden_val ^ reg_val) & mask & bit_shift))
				_diff_str[i] = '^';
			else
				_diff_str[i] = ' ';
		case '\0':
			bit_shift >>= 1;
			break;
		}

		i++;
	}

	return _diff_str;
}

static char *_gen_color_str(
	const unsigned int mask,
	const unsigned int golden_val,
	const unsigned int reg_val)
{
#define FC "\e[41m"
#define EC "\e[m"
#define XXXX FC "x" EC FC "x" EC FC "x" EC FC "x" EC
	static char _clr_str[] =
	    "0b" XXXX "_" XXXX "_" XXXX "_" XXXX "_" XXXX "_" XXXX "_" XXXX "_" XXXX;
	unsigned int i, bit_shift;

	for (i = 2, bit_shift = 1 << 31; bit_shift > 0;) {
		switch (_clr_str[i]) {
		case '_':
			break;

		default:
			if (0 != ((golden_val ^ reg_val) & mask & bit_shift))
				_clr_str[i + 3] = '1';
			else
				_clr_str[i + 3] = '0';

			if (0 == (mask & bit_shift))
				_clr_str[i + 5] = 'x';
			else if (0 == (reg_val & bit_shift))
				_clr_str[i + 5] = '0';
			else
				_clr_str[i + 5] = '1';

			i += strlen(EC) + strlen(FC);	/* XXX: -1 is for '\0' (sizeof) */

		case '\0':
			bit_shift >>= 1;
			break;
		}

		i++;
	}

	return _clr_str;

#undef FC
#undef EC
#undef XXXX
}


/* DEBUG related */

static char *dump_clk_info1(char *page, int clkidx)
{
	char *p = page;

	if (clkidx < NR_CLKS) {
		struct cg_clk *clk = &clks[clkidx];

#if CLKMGR_CTP
		clk_info("[%d,\t%d,\t%s,\t" HEX_FMT ",\t%s,\t%s,\t%s]\n",
#else				/* !CLKMGR_CTP */
		p += sprintf(p, "[%3d, %3d, %3s, " HEX_FMT ", %-10s, %-30s, %-30s]\n",
#endif				/* CLKMGR_CTP */
			clkidx,
			clk->cnt,
			((PWR_ON == clk->state) ? "ON" : "off"),
			clk->mask,
			((clk->grp) ? clk->grp->name : "NULL"),
			clk->name,
			clks[clk->src].name);
	}

	return p;
}


void dump_clk_info_by_id(enum cg_clk_id id)
{
	char buf[256];

	if (id < NR_CLKS) {
		dump_clk_info1(buf, id);
#if !CLKMGR_CTP
		clk_info("%s", buf);
#endif				/* !CLKMGR_CTP */
	}
}


static void dump_clk_info(void)
{
	int i;

	for (i = 0; i < NR_CLKS; i++)
		dump_clk_info_by_id(i);
}


void clock_dump_info(void)
{
	unsigned long flags;

	ENTER_FUNC(FUNC_LV_BODY);

	clkmgr_lock(flags);
	dump_clk_info();
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_BODY);
}
EXPORT_SYMBOL(clock_dump_info);


static int clk_test_show(struct seq_file *s, void *v)
{
	loff_t *spos = (loff_t *) v;
	loff_t i = *spos;

	char buf[256];

	int cnt;
	unsigned int value[2];
	const char *name;

	ENTER_FUNC(FUNC_LV_BODY);

	if (i < NR_CLKS) {
		/* dump clk info */

		dump_clk_info1(buf, i);
		seq_printf(s, "%s", buf);
	} else {
		/* dump clock related registers */

		seq_puts(s, "********** clk register dump *********\n");

		for (i = 0; i < NR_GRPS; i++) {
			name = grp_get_name(i);
			cnt = grp_dump_regs(i, value);

			if (cnt == 1) {
				seq_printf(s, "[%02d][%-10s]=[" HEX_FMT "]\n", (int)i, name, value[0]);
			} else {
				seq_printf(s, "[%02d][%-10s]=[" HEX_FMT "][" HEX_FMT "]\n",
					(int)i, name, value[0], value[1]);
			}
		}

		seq_puts(s, "\n********** clk_test help *********\n");
		seq_puts(s, "clkmux *mux_id cg_id [mod_name] > /proc/clkmgr/clk_test\n");
		seq_puts(s, "enable  clk: echo enable  cg_id [mod_name] > /proc/clkmgr/clk_test\n");
		seq_puts(s, "disable clk: echo disable cg_id [mod_name] > /proc/clkmgr/clk_test\n");
	}

	EXIT_FUNC(FUNC_LV_BODY);
	return 0;
}


static int clk_test_write(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	char cmd[10];
	char mod_name[10];
	int mux_id;
	int cg_id;

	ENTER_FUNC(FUNC_LV_BODY);

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);

	if (copy_from_user(desc, buffer, len)) {
		EXIT_FUNC(FUNC_LV_BODY);
		return 0;
	}

	desc[len] = '\0';

	if (sscanf(desc, "%9s %d %d %9s", cmd, &mux_id, &cg_id, mod_name) == 4) {
		if (!strcmp(cmd, "clkmux"))
			clkmux_sel(mux_id, cg_id, mod_name);
	} else if (sscanf(desc, "%9s %d %9s", cmd, &cg_id, mod_name) == 3) {
		if (!strcmp(cmd, "enable"))
			enable_clock(cg_id, mod_name);
		else if (!strcmp(cmd, "disable"))
			disable_clock(cg_id, mod_name);
	} else if (sscanf(desc, "%9s %d", cmd, &cg_id) == 2) {
		if (!strcmp(cmd, "enable"))
			enable_clock(cg_id, "pll_test");
		else if (!strcmp(cmd, "disable"))
			disable_clock(cg_id, "pll_test");
	}

	EXIT_FUNC(FUNC_LV_BODY);
	return count;
}


static void *clk_test_start(struct seq_file *s, loff_t *pos)
{
	loff_t *spos;

	if (*pos >= NR_CLKS + 1)
		return NULL;

	spos = kmalloc(sizeof(loff_t), GFP_KERNEL);
	if (!spos)
		return NULL;

	*spos = *pos;
	return spos;
}


static void *clk_test_next(struct seq_file *s, void *v, loff_t *pos)
{
	loff_t *spos = (loff_t *) v;

	++(*spos);
	*pos = *spos;

	if (*pos >= NR_CLKS + 1)
		return NULL;

	return spos;
}


static void clk_test_stop(struct seq_file *s, void *v)
{
	kfree(v);
}


static const struct seq_operations clk_test_seq_ops = {
	.start = clk_test_start,
	.next = clk_test_next,
	.stop = clk_test_stop,
	.show = clk_test_show
};


static int clk_test_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &clk_test_seq_ops);
};


static const struct file_operations clk_test_fops = {
	.owner = THIS_MODULE,
	.write = clk_test_write,
	.open = clk_test_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};


static char *dump_pll_info1(char *page, int clkidx)
{
	char *p = page;

	if (clkidx < NR_PLLS) {
		struct pll *pll = &plls[clkidx];
		unsigned int freq = pll_freq_calc_op(pll);

#if CLKMGR_CTP
		clk_info("[%d] %s: %d.%d%d%d MHz: %s (%d)\n",
			clkidx,
			pll->name,
			freq / 1000,
			freq / 100 % 10,
			freq / 10 % 10,
			freq % 10,
			(pll->state ? "ON" : "off"),
			pll->cnt);
#else				/* !CLKMGR_CTP */
		p += sprintf(p, "[%d] %7s: %4d.%03d MHz: %3s (%2d)\n",
			clkidx,
			pll->name,
			freq / 1000,
			freq % 1000,
			(pll->state ? "ON" : "off"),
			pll->cnt);
#endif				/* CLKMGR_CTP */
	}

	return p;
}


static void dump_pll_info(void)
{
	char buf[128];
	int i;

	for (i = 0; i < NR_PLLS; i++) {
		dump_pll_info1(buf, i);
#if !CLKMGR_CTP
		clk_info("%s", buf);
#endif				/* !CLKMGR_CTP */
	}
}


void pll_dump_info(void)
{
	unsigned long flags;

	ENTER_FUNC(FUNC_LV_BODY);

	clkmgr_lock(flags);
	dump_pll_info();
	clkmgr_unlock(flags);

	EXIT_FUNC(FUNC_LV_BODY);
}
EXPORT_SYMBOL(pll_dump_info);


static int pll_test_show(struct seq_file *s, void *v)
{
	loff_t *spos = (loff_t *) v;
	loff_t i = *spos;

	char buf[128];

	int j;
	int cnt;
	unsigned int value[3];
	const char *name;

	ENTER_FUNC(FUNC_LV_BODY);

	if (i < NR_PLLS) {
		dump_pll_info1(buf, i);
		seq_printf(s, "%s", buf);
	} else {
		seq_puts(s, "********** pll register dump *********\n");

		for (i = 0; i < NR_PLLS; i++) {
			name = pll_get_name(i);
			cnt = pll_dump_regs(i, value);

			for (j = 0; j < cnt; j++)
				seq_printf(s, "[%d][%-7s reg%d]=[" HEX_FMT "]\n", (int)i, name, j, value[j]);
		}

		seq_puts(s, "\n********** pll_test help *********\n");
		seq_puts(s, "enable  pll: echo enable  id [mod_name] > /proc/clkmgr/pll_test\n");
		seq_puts(s, "disable pll: echo disable id [mod_name] > /proc/clkmgr/pll_test\n");
		seq_puts(s, "set freq   : echo setfreq id freq_khz   > /proc/clkmgr/pll_test\n");
	}

	EXIT_FUNC(FUNC_LV_BODY);
	return 0;
}


static int pll_test_write(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	char cmd[10];
	char mod_name[10];
	int id;
	unsigned int freq;

	ENTER_FUNC(FUNC_LV_BODY);

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);

	if (copy_from_user(desc, buffer, len)) {
		EXIT_FUNC(FUNC_LV_BODY);
		return 0;
	}

	desc[len] = '\0';

	if (sscanf(desc, "%9s %d %u", cmd, &id, &freq) == 3) {
		if (!strcmp(cmd, "setfreq")) {
			unsigned int r = pll_set_freq(id, freq);
			if (r != 0)
				clk_info("pll_set_freq(): " HEX_FMT ", id: %d, freq: %u\n", r, id, freq);
		}
	} else if (sscanf(desc, "%9s %d %9s", cmd, &id, mod_name) == 3) {
		if (!strcmp(cmd, "enable"))
			enable_pll(id, mod_name);
		else if (!strcmp(cmd, "disable"))
			disable_pll(id, mod_name);
	} else if (sscanf(desc, "%9s %d", cmd, &id) == 2) {
		if (!strcmp(cmd, "enable"))
			enable_pll(id, "pll_test");
		else if (!strcmp(cmd, "disable"))
			disable_pll(id, "pll_test");
	}

	EXIT_FUNC(FUNC_LV_BODY);
	return count;
}


static void *pll_test_start(struct seq_file *s, loff_t *pos)
{
	loff_t *spos;

	if (*pos >= NR_PLLS + 1)
		return NULL;

	spos = kmalloc(sizeof(loff_t), GFP_KERNEL);
	if (!spos)
		return NULL;

	*spos = *pos;
	return spos;
}


static void *pll_test_next(struct seq_file *s, void *v, loff_t *pos)
{
	loff_t *spos = (loff_t *) v;

	++(*spos);
	*pos = *spos;

	if (*pos >= NR_PLLS + 1)
		return NULL;

	return spos;
}


static void pll_test_stop(struct seq_file *s, void *v)
{
	kfree(v);
}


static const struct seq_operations pll_test_seq_ops = {
	.start = pll_test_start,
	.next = pll_test_next,
	.stop = pll_test_stop,
	.show = pll_test_show
};


static int pll_test_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &pll_test_seq_ops);
};


static const struct file_operations pll_test_fops = {
	.owner = THIS_MODULE,
	.write = pll_test_write,
	.open = pll_test_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};



static int pll_fsel_show(struct seq_file *s, void *v)
{
	int i;
	int cnt;
	unsigned int value[3];
	const char *name;

	ENTER_FUNC(FUNC_LV_BODY);

	for (i = 0; i < NR_PLLS; i++) {
		name = pll_get_name(i);

		if (pll_is_on(i)) {
			cnt = pll_dump_regs(i, value);

			if (cnt >= 2) {
				seq_printf(s, "[%d][%-7s]=[" HEX_FMT " " HEX_FMT "]\n",
					i, name, value[0], value[1]);
			} else {
				seq_printf(s, "[%d][%-7s]=[" HEX_FMT "]\n", i, name, value[0]);
			}
		} else {
			seq_printf(s, "[%d][%-7s]=[-1]\n", i, name);
		}
	}

	seq_puts(s, "\n********** pll_fsel help *********\n");
	seq_puts(s, "adjust pll frequency:  echo id freq > /proc/clkmgr/pll_fsel\n");

	EXIT_FUNC(FUNC_LV_BODY);
	return 0;
}

static int pll_fsel_write(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int id;
	unsigned int value;

	ENTER_FUNC(FUNC_LV_BODY);

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);

	if (copy_from_user(desc, buffer, len)) {
		EXIT_FUNC(FUNC_LV_BODY);
		return 0;
	}

	desc[len] = '\0';

	if (sscanf(desc, "%d %x", &id, &value) == 2)
		pll_fsel(id, value);

	EXIT_FUNC(FUNC_LV_BODY);
	return count;
}


static int pll_fsel_open(struct inode *inode, struct file *file)
{
	return single_open(file, pll_fsel_show, NULL);
}


static const struct file_operations pll_fsel_fops = {
	.owner = THIS_MODULE,
	.write = pll_fsel_write,
	.open = pll_fsel_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


void subsys_dump_info(void)
{
	int i;
	int state;
	struct subsys *sys;
	const char *name;
	unsigned long flags;

	ENTER_FUNC(FUNC_LV_BODY);
	clkmgr_lock(flags);

	for (i = 0; i < NR_SYSS; i++) {
		sys = id_to_sys(i);
		BUG_ON(!sys);
		state = get_sys_state_locked(sys);
		name = subsys_get_name(i);
#if CLKMGR_CTP
		clk_info("[%d][%s]: %s\n", i, name, state ? "ON" : "off");
#else				/* !CLKMGR_CTP */
		clk_info("[%d][%-10s]: %3s\n", i, name, state ? "ON" : "off");
#endif				/* CLKMGR_CTP */
	}

	clkmgr_unlock(flags);
	EXIT_FUNC(FUNC_LV_BODY);
}
EXPORT_SYMBOL(subsys_dump_info);


static int subsys_test_show(struct seq_file *s, void *v)
{
	int i;
	int state;
	unsigned int value, sta, sta_s;
	const char *name;

	ENTER_FUNC(FUNC_LV_BODY);

	sta = clk_readl(SPM_PWR_STATUS);
	sta_s = clk_readl(SPM_PWR_STATUS_S);

	seq_puts(s, "********** subsys register dump *********\n");

	for (i = 0; i < NR_SYSS; i++) {
		name = subsys_get_name(i);
		state = subsys_is_on(i);
		subsys_dump_regs(i, &value);
		seq_printf(s, "[%d][%-10s]=[" HEX_FMT "], state(%u)\n", i, name, value, state);
	}

	seq_printf(s, "SPM_PWR_STATUS=" HEX_FMT ", SPM_PWR_STATUS_S=" HEX_FMT "\n", sta, sta_s);

	seq_puts(s, "\n********** subsys_test help *********\n");
	seq_puts(s, "enable subsys:  echo enable id > /proc/clkmgr/subsys_test\n");
	seq_puts(s, "disable subsys: echo disable id [force_off] > /proc/clkmgr/subsys_test\n");

	EXIT_FUNC(FUNC_LV_BODY);
	return 0;
}

static int subsys_test_write(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	char cmd[10];
	enum subsys_id id;
	int force_off;
	int err = 0;

	ENTER_FUNC(FUNC_LV_BODY);

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);

	if (copy_from_user(desc, buffer, len)) {
		EXIT_FUNC(FUNC_LV_BODY);
		return 0;
	}

	desc[len] = '\0';

	if (sscanf(desc, "%9s %d %d", cmd, (int *)&id, &force_off) == 3) {
		if (!strcmp(cmd, "disable"))
			err = disable_subsys_force(id, "test");
	} else if (sscanf(desc, "%9s %d", cmd, (int *)&id) == 2) {
		if (!strcmp(cmd, "enable"))
			err = enable_subsys(id, "test");
		else if (!strcmp(cmd, "disable"))
			err = disable_subsys(id, "test");
	}

	clk_info("[%s]%s subsys %d: result is %d\n", __func__, cmd, id, err);

	EXIT_FUNC(FUNC_LV_BODY);
	return count;
}


static int subsys_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, subsys_test_show, NULL);
}


static const struct file_operations subsys_test_fops = {
	.owner = THIS_MODULE,
	.write = subsys_test_write,
	.open = subsys_test_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int udelay_test_show(struct seq_file *s, void *v)
{
	ENTER_FUNC(FUNC_LV_BODY);

	seq_puts(s, "\n********** udelay_test help *********\n");
	seq_puts(s, "test udelay:  echo delay > /proc/clkmgr/udelay_test\n");

	EXIT_FUNC(FUNC_LV_BODY);
	return 0;
}


static int udelay_test_write(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	unsigned int delay;
	unsigned int pre, pos;
	const unsigned long GTP3_COUNT = APMCU_GPTIMER_BASE + 0x38;

	ENTER_FUNC(FUNC_LV_BODY);

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);

	if (copy_from_user(desc, buffer, len)) {
		EXIT_FUNC(FUNC_LV_BODY);
		return 0;
	}

	desc[len] = '\0';

	if (sscanf(desc, "%u", &delay) == 1) {
		pre = clk_readl(GTP3_COUNT);	/* read GPT3 counter */
		udelay(delay);
		pos = clk_readl(GTP3_COUNT);	/* read GPT3 counter again */
		clk_info("udelay(%u) test: pre=" HEX_FMT ", pos=" HEX_FMT ", delta=%u\n",
			delay, pre, pos, pos - pre);
	}

	EXIT_FUNC(FUNC_LV_BODY);
	return count;
}


static int udelay_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, udelay_test_show, NULL);
}


static const struct file_operations udelay_test_fops = {
	.owner = THIS_MODULE,
	.write = udelay_test_write,
	.open = udelay_test_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int golden_test_show(struct seq_file *s, void *v)
{
	char _mask_str[] = "0bxxxx_xxxx_xxxx_xxxx_xxxx_xxxx_xxxx_xxxx";
	char _diff_str[] = "0b    _    _    _    _    _    _    _    ";
	int i = 0;

	ENTER_FUNC(FUNC_LV_BODY);

	if (FALSE == _golden.is_golden_log) {
		for (i = 0; i < _golden.nr_golden_setting; i++) {
			seq_printf(s, "" HEX_FMT " " HEX_FMT " " HEX_FMT "\n",
				_golden.buf_golden_setting[i].addr,
				_golden.buf_golden_setting[i].mask,
				_golden.buf_golden_setting[i].golden_val);
		}
	}

	if (0 == i) {
		seq_puts(s, "\n********** golden_test help *********\n");
		seq_puts(s,  "1.   disable snapshot:                  echo disable > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "2.   insert golden setting (tool mode): echo 0x10000000 (addr) 0bxxxx_xxxx_xxxx_xxxx_0001_0100_1001_0100 (mask & golden value) > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "(2.) insert golden setting (hex mode):  echo 0x10000000 (addr) 0xFFFF (mask) 0x1494 (golden value) > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "(2.) insert golden setting (dec mode):  echo 268435456 (addr) 65535 (mask) 5268 (golden value) > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "3.   set filter:                        echo filter func_name [line_num] > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "(3.) disable filter:                    echo filter > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "4.   enable snapshot:                   echo enable > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "5.   set compare mode:                  echo compare > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "(5.) set apply mode:                    echo apply > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "(5.) set color mode:                    echo color > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "(5.) set diff mode:                     echo color > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "(5.) disable compare/apply/color mode:  echo normal > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "6.   set register value (normal mode):  echo set 0x10000000 (addr) 0x13201494 (reg val) > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "(6.) set register value (mask mode):    echo set 0x10000000 (addr) 0xffff (mask) 0x13201494 (reg val) > /proc/clkmgr/golden_test\n");
		seq_puts(s,  "(6.) set register value (bit mode):     echo set 0x10000000 (addr) 0 (bit num) 1 (reg val) > /proc/clkmgr/golden_test\n");
	} else {
		struct snapshot *snapshot;

		if (!strcmp(_golden.func, __func__) && (_golden.line == 0))
			snapshot_golden_setting(__func__, 0);

		while (NULL != (snapshot = _snapshot_consume(&_golden))) {
			seq_printf(s, "@ %s():%d\n", snapshot->func, snapshot->line);

			for (i = 0; i < _golden.nr_golden_setting; i++) {
				if (MODE_NORMAL == _golden.mode ||
				    ((_golden.buf_golden_setting[i].mask & _golden.buf_golden_setting[i].golden_val) !=
				     (_golden.buf_golden_setting[i].mask & snapshot->reg_val[i]))) {
					if (MODE_COLOR == _golden.mode) {
						seq_printf(s, HEX_FMT "\t" HEX_FMT "\t" HEX_FMT "\t%s\n",
							_golden.buf_golden_setting[i].addr,
							_golden.buf_golden_setting[i].mask,
							snapshot->reg_val[i],
							_gen_color_str(_golden.buf_golden_setting[i].mask,
								_golden.buf_golden_setting[i].golden_val,
								snapshot->reg_val[i]));
					}
				} else if (MODE_DIFF == _golden.mode) {
					seq_printf(s, HEX_FMT "\t" HEX_FMT "\t" HEX_FMT "\t%s\n",
						_golden.buf_golden_setting[i].addr,
						_golden.buf_golden_setting[i].mask,
						snapshot->reg_val[i],
						_gen_mask_str(_golden.buf_golden_setting[i].mask,
							snapshot->reg_val[i], _mask_str));

					seq_printf(s, HEX_FMT "\t" HEX_FMT "\t" HEX_FMT "\t%s\n",
						_golden.buf_golden_setting[i].addr,
						_golden.buf_golden_setting[i].mask,
						_golden.buf_golden_setting[i].golden_val,
						_gen_diff_str(_golden.buf_golden_setting[i].mask,
							_golden.buf_golden_setting[i].golden_val,
							snapshot->reg_val[i], _diff_str));
				} else {
					seq_printf(s, HEX_FMT "\t" HEX_FMT "\n",
						_golden.buf_golden_setting[i].addr,
						snapshot->reg_val[i]);
				}
			}
		}
	}

	EXIT_FUNC(FUNC_LV_BODY);
	return 0;
}

static int golden_test_write(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	char desc[256];
	int len = 0;

	char cmd[64];
	unsigned int addr;
	unsigned int mask;
	unsigned int golden_val;

	ENTER_FUNC(FUNC_LV_BODY);

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);

	if (copy_from_user(desc, buffer, len)) {
		EXIT_FUNC(FUNC_LV_BODY);
		return 0;
	}

	desc[len] = '\0';

	/* set golden setting (hex mode) */
	if (sscanf(desc, "0x%x 0x%x 0x%x", &addr, &mask, &golden_val) == 3)
		_golden_setting_add(&_golden, addr, mask, golden_val);
	/* set golden setting (dec mode) */
	else if (sscanf(desc, "%d %d %d", &addr, &mask, &golden_val) == 3)
		_golden_setting_add(&_golden, addr, mask, golden_val);
	/* set filter (func + line) */
	else if (sscanf(desc, "filter %63s %d", _golden.func, &_golden.line) == 2)
		;
	/* set filter (func) */
	else if (sscanf(desc, "filter %63s", _golden.func) == 1)
		_golden.line = 0;
	/* set golden setting (mixed mode) */
	else if (sscanf(desc, "0x%x 0b%63s", &addr, cmd) == 2) {
		if (!_parse_mask_val(cmd, &mask, &golden_val))
			_golden_setting_add(&_golden, addr, mask, golden_val);
	}
	/* set reg value (mask mode) */
	else if (sscanf(desc, "set 0x%x 0x%x 0x%x", &addr, &mask, &golden_val) == 3)
		_golden_write_reg(addr, mask, golden_val);
	/* set reg value (bit mode) */
	else if (sscanf(desc, "set 0x%x %d %d", &addr, &mask, &golden_val) == 3) {
		if (0 <= mask && mask <= 31) {	/* XXX: mask is bit number (alias) */
			golden_val = (golden_val & BIT(0)) << mask;
			mask = BIT(0) << mask;
			_golden_write_reg(addr, mask, golden_val);
		}
	}
	/* set reg value (normal mode) */
	else if (sscanf(desc, "set 0x%x 0x%x", &addr, &golden_val) == 2) {
		_golden_write_reg(addr, 0xFFFFFFFF, golden_val);
	} else if (sscanf(desc, "%63s", cmd) == 1) {
		if (!strcmp(cmd, "enable"))
			_golden_setting_enable(&_golden);
		else if (!strcmp(cmd, "disable"))
			_golden_setting_disable(&_golden);
		else if (!strcmp(cmd, "normal"))
			_golden_setting_set_mode(&_golden, MODE_NORMAL);
		else if (!strcmp(cmd, "compare"))
			_golden_setting_set_mode(&_golden, MODE_COMPARE);
		else if (!strcmp(cmd, "apply"))
			_golden_setting_set_mode(&_golden, MODE_APPLY);
		else if (!strcmp(cmd, "color"))
			_golden_setting_set_mode(&_golden, MODE_COLOR);
		else if (!strcmp(cmd, "diff"))
			_golden_setting_set_mode(&_golden, MODE_DIFF);
		else if (!strcmp(cmd, "filter"))
			_golden.func[0] = '\0';
	}

	EXIT_FUNC(FUNC_LV_BODY);
	return count;
}


static int golden_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, golden_test_show, NULL);
}


static const struct file_operations golden_test_fops = {
	.owner = THIS_MODULE,
	.write = golden_test_write,
	.open = golden_test_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


void mt_clkmgr_debug_init(void)
{
	struct proc_dir_entry *entry;
	struct proc_dir_entry *clkmgr_dir;

	ENTER_FUNC(FUNC_LV_API);

	clkmgr_dir = proc_mkdir("clkmgr", NULL);

	if (!clkmgr_dir) {
		clk_err("[%s]: fail to mkdir /proc/clkmgr\n", __func__);
		EXIT_FUNC(FUNC_LV_API);
		return;
	}

	entry = proc_create("clk_test", 00640, clkmgr_dir, &clk_test_fops);
	entry = proc_create("pll_test", 00640, clkmgr_dir, &pll_test_fops);
	entry = proc_create("pll_fsel", 00640, clkmgr_dir, &pll_fsel_fops);
	entry = proc_create("subsys_test", 00640, clkmgr_dir, &subsys_test_fops);
	entry = proc_create("udelay_test", 00640, clkmgr_dir, &udelay_test_fops);

	{
#define GOLDEN_SETTING_BUF_SIZE (2 * PAGE_SIZE)

		unsigned int *buf;

		buf = kmalloc(GOLDEN_SETTING_BUF_SIZE, GFP_KERNEL);

		if (NULL != buf) {
			_golden_setting_init(&_golden, buf, GOLDEN_SETTING_BUF_SIZE);
			entry = proc_create("golden_test", 00640, clkmgr_dir, &golden_test_fops);
		}
	}

	EXIT_FUNC(FUNC_LV_API);
}

static int mt_clkmgr_debug_bringup_init(void)
{
	ENTER_FUNC(FUNC_LV_API);

#if 1				/* XXX: temp solution for init function not issued */
	if (0 == initialized)
		mt_clkmgr_init();
#endif				/* XXX: temp solution for init function not issued */

#if defined(CONFIG_MT_ENG_BUILD)
	mt_clkmgr_debug_init();
#endif				/* CONFIG_MT_ENG_BUILD */

	EXIT_FUNC(FUNC_LV_API);
	return 0;
}

module_init(mt_clkmgr_debug_bringup_init);
