#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpu.h>

#include <linux/types.h>
#include <linux/string.h>
#include <mach/mt_cirq.h>
#include <asm/system_misc.h>

#include <mach/mt_typedefs.h>
#include <mach/sync_write.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_dcm.h>
#include <mach/mt_gpt.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_spm_sleep.h>
#include <mach/hotplug.h>

#include <mach/mt_idle_trace.h>

DEFINE_TRACE(Cx_to_Cy_event);

#define IDLE_CCI_WORKAROUND     1

#if defined(SPM_MCDI_FUNC)
	#undef SPM_MCDI_FUNC
#endif /* defined(SPM_MCDI_FUNC) */

#define USING_XLOG

#ifdef USING_XLOG
#include <linux/xlog.h>

#define TAG     "Power/swap"

#define idle_err(fmt, args...)       \
	xlog_printk(ANDROID_LOG_ERROR, TAG, fmt, ##args)
#define idle_warn(fmt, args...)      \
	xlog_printk(ANDROID_LOG_WARN, TAG, fmt, ##args)
#define idle_info(fmt, args...)      \
	xlog_printk(ANDROID_LOG_INFO, TAG, fmt, ##args)
#define idle_dbg(fmt, args...)       \
	xlog_printk(ANDROID_LOG_DEBUG, TAG, fmt, ##args)
#define idle_ver(fmt, args...)       \
	xlog_printk(ANDROID_LOG_VERBOSE, TAG, fmt, ##args)

#else /* !USING_XLOG */

#define TAG     "[Power/swap] "

#define idle_err(fmt, args...)       \
do {	\
	pr_err(TAG);           \
	pr_cont(fmt, ##args);    \
} while (0)

#define idle_warn(fmt, args...)      \
do {	\
	pr_warn(TAG);       \
	pr_cont(fmt, ##args);   \
} while (0)

#define idle_info(fmt, args...)      \
do {	\
	pr_notice(TAG);        \
	pr_cont(fmt, ##args);   \
} while (0)

#define idle_dbg(fmt, args...)       \
do {	\
	pr_info(TAG);          \
	pr_cont(fmt, ##args);   \
} while (0)

#define idle_ver(fmt, args...)       \
do {	\
	pr_debug(TAG);         \
	pr_cont(fmt, ##args);    \
} while (0)


#endif


#define idle_readl(addr) \
	DRV_Reg32(addr)

#define idle_writel(addr, val)   \
	mt65xx_reg_sync_writel(val, addr)

#define idle_setl(addr, val) \
	mt65xx_reg_sync_writel(idle_readl(addr) | (val), addr)

#define idle_clrl(addr, val) \
	mt65xx_reg_sync_writel(idle_readl(addr) & ~(val), addr)

#define INVALID_GRP_ID(grp) (grp < 0 || grp >= NR_GRPS)
bool __attribute__((weak))
clkmgr_idle_can_enter(unsigned int *condition_mask, unsigned int *block_mask)
{
	return false;
}

enum {
	IDLE_TYPE_MC = 0,
	IDLE_TYPE_DP = 1,
	IDLE_TYPE_SL = 2,
	IDLE_TYPE_RG = 3,
	NR_TYPES = 4,
};

enum {
	BY_CPU = 0,
	BY_CLK = 1,
	BY_TMR = 2,
	BY_OTH = 3,
	NR_REASONS = 4,
};

static const char *idle_name[NR_TYPES] = {
	"mcidle",
	"dpidle",
	"slidle",
	"rgidle",
};

static const char *reason_name[NR_REASONS] = {
	"by_cpu",
	"by_clk",
	"by_tmr",
	"by_oth",
};

static int idle_switch[NR_TYPES] = {
	1,  /* mcidle switch  */
	1,  /* dpidle switch */
	1,  /* slidle switch */
	1,  /* rgidle switch */
};


#if IDLE_CCI_WORKAROUND
static int cci_mux_before_idle_wfi;
#endif /* IDLE_CCI_WORKAROUND */


#define MT_IDLE_DEBUG

#ifdef MT_IDLE_DEBUG
struct idle_info {
	int type;
	int in;
	unsigned int timestamp;
};

static struct idle_info save_data[NR_CPUS];

static void save_idle_info(int cpu, int type, int in)
{
	save_data[cpu].type = type;
	save_data[cpu].in = in;
	save_data[cpu].timestamp = idle_readl(0xF0008028);
}

int dump_idle_info(char *buffer, int size)
{
	int i;
	int len = 0;
#define LOCAL_LEN   128
	char fmt[LOCAL_LEN];

	if (!buffer || size <= 1)
		return 0;

	len += snprintf(fmt + len, LOCAL_LEN - len
	, "[idle]cpu type in stamp\n");

	for (i = 0; i < nr_cpu_ids; i++) {
		len += snprintf(fmt + len, LOCAL_LEN - len,
		"%d %d %d %x\n", i, save_data[i].type,
		save_data[i].in, save_data[i].timestamp);
	}

	len = min(len, size - 1);
	memcpy(buffer, fmt, len);
	*(buffer + len) = '\0';

	return len;
}

#else

static inline void save_idle_info(int cpu, int type, int in)    { }
int dump_idle_info(char *buffer, int size)  { return 0; }

#endif

/************************************************
 * multi-core idle part
 ************************************************/
#ifdef SPM_MCDI_FUNC

static unsigned int mcidle_gpt_percpu[NR_CPUS] = {
	NR_GPTS,
	GPT1,
	GPT4,
	GPT5,
};

static unsigned int mcidle_condition_mask[NR_GRPS] = {
	[CG_MAINPLL]    = 0x00000000, /* PLL */
	[CG_UNIVPLL]    = 0x00000000,
	[CG_MMPLL]      = 0x00000000,
	[CG_TOP0]       = 0x00000000, /* TOPCK */
	[CG_TOP1]       = 0x00000000,
	[CG_TOP2]       = 0x00000000,
	[CG_TOP3]       = 0x00000000,
	[CG_TOP4]       = 0x00000000,
	[CG_TOP6]       = 0x00000000,
	[CG_TOP7]       = 0x00000000,
	[CG_TOP8]       = 0x00000000,
	[CG_TOP9]       = 0x00000000,
	[CG_INFRA]      = 0x00000000, /* INFRA: */
	/* PERI0: i2c5~0, uart2~0, aphif, usb1~0, pwm7~5, nfi */
	[CG_PERI0]      = 0xFDC40DC1,
	[CG_PERI1]      = 0x00000009, /* PERI1: spi1, i2c6 */
	[CG_MFG]        = 0x00000000, /* MFG: */
	[CG_ISP]        = 0x00003FF5, /* IMAGE: all */
	[CG_VENC]       = 0x00000001, /* VENC:  all */
	[CG_VDEC0]      = 0x00000001, /* VDEC0: all */
	[CG_VDEC1]      = 0x00000001, /* VDEC1: all */
	/* DISP0: g2d,cmdq,gamma,rdma1,wdma1,wdma0,2DSHP,SCL,ROT */
	[CG_DISP0]      = 0x01FF1E8E,
	[CG_DISP1]      = 0x00000000, /* DISP1: */
	[CG_AUDIO]      = 0x00000040, /* AUDIO: i2s */
};

static unsigned int mcidle_block_mask[NR_GRPS] = {0x0};

static unsigned int mcidle_timer_left[NR_CPUS];
static unsigned int mcidle_timer_left2[NR_CPUS];
static unsigned int mcidle_time_critera = 26000;

static unsigned long mcidle_cnt[NR_CPUS] = {0};
static unsigned long mcidle_block_cnt[NR_CPUS][NR_REASONS] = {{0 } };

static DEFINE_MUTEX(mcidle_locked);

static void enable_mcidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&mcidle_locked);
	mcidle_condition_mask[grp] &= ~mask;
	mutex_unlock(&mcidle_locked);
}

static void disable_mcidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&mcidle_locked);
	mcidle_condition_mask[grp] |= mask;
	mutex_unlock(&mcidle_locked);
}

void enable_mcidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);
	BUG_ON(INVALID_GRP_ID(grp));
	enable_mcidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_mcidle_by_bit);

void disable_mcidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);
	BUG_ON(INVALID_GRP_ID(grp));
	disable_mcidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_mcidle_by_bit);


bool mcidle_can_enter(int cpu)
{
	int reason = NR_REASONS;

	if (En_SPM_MCDI != 1) {
		reason = BY_OTH;
		goto out;
	}

#if 1
	/* only SODI */
	if (cpu != 0) {
		reason = BY_OTH;
		goto out;
	}
#endif

	if (atomic_read(&is_in_hotplug) == 1) {
		reason = BY_CPU;
		goto out;
	}

	if (cpu == 0) {
		memset(mcidle_block_mask, 0, NR_GRPS * sizeof(unsigned int));
		if (!clkmgr_idle_can_enter(mcidle_condition_mask
			, mcidle_block_mask)) {
			reason = BY_CLK;
			goto out;
		}
	}

	mcidle_timer_left[cpu] = localtimer_get_counter();
	if (mcidle_timer_left[cpu] < mcidle_time_critera ||
		((int)mcidle_timer_left[cpu]) < 0) {
		reason = BY_TMR;
		goto out;
	}

out:
	if (reason < NR_REASONS) {
		mcidle_block_cnt[cpu][reason]++;
		return false;
	} else {
		return true;
	}
}

static void mcidle_before_wfi(int cpu)
{
	unsigned int id = mcidle_gpt_percpu[cpu];
	if (cpu != 0) {
		mcidle_timer_left2[cpu] = localtimer_get_counter();
		gpt_set_cmp(id, mcidle_timer_left2[cpu]);
		start_gpt(id);
	}
}

static void mcidle_after_wfi(int cpu)
{
	unsigned int id = mcidle_gpt_percpu[cpu];
	if (cpu != 0) {
		if (gpt_check_and_ack_irq(id)) {
			localtimer_set_next_event(1);
		} else {
			/* waked up by other wakeup source */
			unsigned int cnt, cmp;
			gpt_get_cnt(id, &cnt);
			gpt_get_cmp(id, &cmp);
			if (unlikely(cmp < cnt)) {
				idle_err("[%s]GPT%d: counter = %10u, compare = %10u\n"
				, __func__, id + 1, cnt, cmp);
				BUG();
			}

			localtimer_set_next_event(cmp-cnt);
			stop_gpt(id);
		}
	}

	mcidle_cnt[cpu]++;
}

static void go_to_mcidle(int cpu)
{
	mcidle_before_wfi(cpu);

	spm_mcdi_wfi();

	mcidle_after_wfi(cpu);
}

#else /* !defined(SPM_MCDI_FUNC) */

void enable_mcidle_by_bit(int id)
{
	idle_dbg("%s(): MCDI is not enabled.\n", __func__);
}
EXPORT_SYMBOL(enable_mcidle_by_bit);

void disable_mcidle_by_bit(int id)
{
	idle_dbg("%s(): MCDI is not enabled.\n", __func__);
}
EXPORT_SYMBOL(disable_mcidle_by_bit);

#endif /* defined(SPM_MCDI_FUNC) */

/************************************************
 * deep idle part
 ************************************************/
static unsigned int dpidle_condition_mask[NR_GRPS] = {
	[CG_MAINPLL]    = 0x00000000, /* PLL */
	[CG_UNIVPLL]    = 0x00000000,
	[CG_MMPLL]      = 0x00000000,
	[CG_TOP0]       = 0x00000000, /* TOPCK */
	[CG_TOP1]       = 0x00000000,
	[CG_TOP2]       = 0x00000000,
	[CG_TOP3]       = 0x00000000,
	[CG_TOP4]       = 0x00000000,
	[CG_TOP6]       = 0x00000000,
	[CG_TOP7]       = 0x00000000,
	[CG_TOP8]       = 0x00000000,
	[CG_TOP9]       = 0x00000000,
	[CG_INFRA]      = 0x00000000, /* INFRA: */
	/* PERI0: i2c5~0, uart2~0, aphif, usb1~0, pwm7~5, nfi */
	[CG_PERI0]      = 0xFDC40DC1,
	[CG_PERI1]      = 0x00000029, /* PERI1: spi1, i2c6, GCPU */
	[CG_MFG]        = 0x0000000F, /* MFG:   all */
	[CG_ISP]        = 0x00003FF5, /* IMAGE: all */
	[CG_VENC]       = 0x00000001, /* VENC:  all */
	[CG_VDEC0]      = 0x00000001, /* VDEC0: all */
	[CG_VDEC1]      = 0x00000001, /* VDEC1: all */
	[CG_DISP0]      = 0x01FFFFFF, /* DISP0: all */
	[CG_DISP1]      = 0x0000FFFF, /* DISP1: all, LVDS, HDMI */
	[CG_AUDIO]      = 0x00300040, /* AUDIO: i2s, HDMI */
};

static unsigned int dpidle_block_mask[NR_GRPS] = {0x0};

static unsigned int dpidle_timer_left;
static unsigned int dpidle_timer_left2;
static unsigned int dpidle_time_critera = 26000;

static unsigned long dpidle_cnt[NR_CPUS] = {0};
static unsigned long dpidle_block_cnt[NR_REASONS] = {0};

static DEFINE_MUTEX(dpidle_locked);

static void enable_dpidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&dpidle_locked);
	dpidle_condition_mask[grp] &= ~mask;
	mutex_unlock(&dpidle_locked);
}

static void disable_dpidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&dpidle_locked);
	dpidle_condition_mask[grp] |= mask;
	mutex_unlock(&dpidle_locked);
}

void enable_dpidle_by_bit(int id)
{
	unsigned int grp = clk_id_to_grp_id(id);
	unsigned int mask = clk_id_to_mask(id);

	if ((grp >= NR_GRPS) || (mask == 0))
		idle_info("[%s]wrong clock id\n", __func__);
	else
		enable_dpidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_dpidle_by_bit);

void disable_dpidle_by_bit(int id)
{
	unsigned int grp = clk_id_to_grp_id(id);
	unsigned int mask = clk_id_to_mask(id);

	if ((grp >= NR_GRPS) || (mask == 0))
		idle_info("[%s]wrong clock id\n", __func__);
	else
		disable_dpidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_dpidle_by_bit);


static bool dpidle_can_enter(void)
{
	int reason = NR_REASONS;

#ifdef SPM_MCDI_FUNC
	if (En_SPM_MCDI != 0) {
		reason = BY_OTH;
		goto out;
	}
#endif

	/* if ((smp_processor_id() != 0) || (num_online_cpus() != 1)) { */
	if (atomic_read(&hotplug_cpu_count) != 1) {
		reason = BY_CPU;
		goto out;
	}

	memset(dpidle_block_mask, 0, NR_GRPS * sizeof(unsigned int));
	if (!clkmgr_idle_can_enter(dpidle_condition_mask, dpidle_block_mask)) {
		reason = BY_CLK;
		goto out;
	}

	dpidle_timer_left = localtimer_get_counter();
	if (dpidle_timer_left < dpidle_time_critera ||
		((int)dpidle_timer_left) < 0) {
		reason = BY_TMR;
		goto out;
	}

out:
	if (reason < NR_REASONS) {
		dpidle_block_cnt[reason]++;
		return false;
	} else {
		return true;
	}
}

bool spm_dpidle_can_enter(void)
{
	return true;
}

static unsigned int clk_cfg_1;

#define faudintbus_pll2sq() \
do {    \
	clk_cfg_1 = idle_readl(CLK_CFG_1);\
	idle_writel(CLK_CFG_1, clk_cfg_1 & 0xFFFFFCFF);  \
} while (0)

#define faudintbus_sq2pll() \
	idle_writel(CLK_CFG_1, clk_cfg_1);  \


void spm_dpidle_before_wfi(void)
{
	int cpu = smp_processor_id();
	trace_Cx_to_Cy_event(cpu, 0, 3);
	faudintbus_pll2sq();

#if 0
	dpidle_timer_left = localtimer_get_counter();
	gpt_set_cmp(GPT4, dpidle_timer_left);
#else
	dpidle_timer_left2 = localtimer_get_counter();
	gpt_set_cmp(GPT4, dpidle_timer_left2);
#endif

	start_gpt(GPT4);
}

void spm_dpidle_after_wfi(void)
{
	int cpu = smp_processor_id();
	trace_Cx_to_Cy_event(cpu, 3, 0);

#if 0
	idle_info("[%s]timer_left=%u, timer_left2=%u, delta=%u\n",
	dpidle_timer_left, dpidle_timer_left2
	, dpidle_timer_left-dpidle_timer_left2);
#endif

	/* if (gpt_check_irq(GPT4)) { */
	if (gpt_check_and_ack_irq(GPT4)) {
		/* waked up by WAKEUP_GPT */
		localtimer_set_next_event(1);
	} else {
		/* waked up by other wakeup source */
		unsigned int cnt, cmp;
		gpt_get_cnt(GPT4, &cnt);
		gpt_get_cmp(GPT4, &cmp);
		if (unlikely(cmp < cnt)) {
			idle_err("[%s]GPT%d: counter = %10u, compare = %10u\n"
			, __func__,	GPT4 + 1, cnt, cmp);
			BUG();
		}

		localtimer_set_next_event(cmp-cnt);
		stop_gpt(GPT4);
		/*GPT_ClearCount(WAKEUP_GPT); */
	}

	faudintbus_sq2pll();

	dpidle_cnt[0]++;
}


/************************************************
 * slow idle part
 ************************************************/
static unsigned int slidle_condition_mask[NR_GRPS] = {
	[CG_MAINPLL] = 0x00000000,	/* PLL */
	[CG_UNIVPLL] = 0x00000000,
	[CG_MMPLL] = 0x00000000,
	[CG_TOP0] = 0x00000000,	/* TOPCK */
	[CG_TOP1] = 0x00000000,
	[CG_TOP2] = 0x00000000,
	[CG_TOP3] = 0x00000000,
	[CG_TOP4] = 0x00000000,
	[CG_TOP6] = 0x00000000,
	[CG_TOP7] = 0x00000000,
	[CG_TOP8] = 0x00000000,
	[CG_TOP9] = 0x00000000,
	[CG_INFRA] = 0x00000000,	/* INFRA: */
	[CG_PERI0] = 0xFC001000,	/* PERI0: i2c5~0, dma */
	[CG_PERI1] = 0x00000009,	/* PERI1: spi1, i2c6 */
	[CG_MFG] = 0x00000000,	/* MFG: */
	[CG_ISP] = 0x00000000,	/* IMAGE: */
	[CG_VENC] = 0x00000000,	/* VENC: */
	[CG_VDEC0] = 0x00000000,	/* VDEC0: */
	[CG_VDEC1] = 0x00000000,	/* VDEC1: */
	[CG_DISP0] = 0x00000000,	/* DISP0: */
	[CG_DISP1] = 0x00000000,	/* DISP1: */
	[CG_AUDIO] = 0x00000000,	/* AUDIO: */
};

static unsigned int slidle_block_mask[NR_GRPS] = {0x0};

static unsigned long slidle_cnt[NR_CPUS] = {0};
static unsigned long slidle_block_cnt[NR_REASONS] = {0};

static DEFINE_MUTEX(slidle_locked);


static void enable_slidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&slidle_locked);
	slidle_condition_mask[grp] &= ~mask;
	mutex_unlock(&slidle_locked);
}

static void disable_slidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&slidle_locked);
	slidle_condition_mask[grp] |= mask;
	mutex_unlock(&slidle_locked);
}

void enable_slidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	if (INVALID_GRP_ID(grp))
		return;

	enable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_slidle_by_bit);

void disable_slidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	if (INVALID_GRP_ID(grp))
		return;

	disable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_slidle_by_bit);

static bool slidle_can_enter(void)
{
	int reason = NR_REASONS;
	/* if ((smp_processor_id() != 0) || (num_online_cpus() != 1)) { */
	if (atomic_read(&hotplug_cpu_count) != 1) {
		reason = BY_CPU;
		goto out;
	}

	memset(slidle_block_mask, 0, NR_GRPS * sizeof(unsigned int));
	if (!clkmgr_idle_can_enter(slidle_condition_mask, slidle_block_mask)) {
		reason = BY_CLK;
		goto out;
	}

	if ((ptp_data[0] != 0) || (ptp_data[3] != 0)) {
		reason = BY_OTH;
		goto out;
	}

out:
	if (reason < NR_REASONS) {
		slidle_block_cnt[reason]++;
		return false;
	} else {
		return true;
	}
}

static void slidle_before_wfi(int cpu)
{
	trace_Cx_to_Cy_event(cpu, 0, 2);
	if (cpu == 0) {
		mt_cirq_wfi_func();
		mt_cirq_enable();
	}

#if IDLE_CCI_WORKAROUND
	cci_mux_before_idle_wfi = clkmux_get(MT_CLKMUX_CCI_SEL, "slidle");
	clkmux_sel(MT_CLKMUX_CCI_SEL, MT_CG_V_CKSQ_MUX_CK, "slidle");
#endif /* IDLE_CCI_WORKAROUND */

	bus_dcm_enable();
}

static void slidle_after_wfi(int cpu)
{
	trace_Cx_to_Cy_event(cpu, 2, 0);

	bus_dcm_disable();

#if IDLE_CCI_WORKAROUND
	clkmux_sel(MT_CLKMUX_CCI_SEL, cci_mux_before_idle_wfi, "slidle");
#endif /* IDLE_CCI_WORKAROUND */

	if (cpu == 0) {
		mt_cirq_flush();
		mt_cirq_disable();
	}
	slidle_cnt[cpu]++;
}

static void go_to_slidle(int cpu)
{
	slidle_before_wfi(cpu);

	dsb();
	__asm__ __volatile__("wfi" : : : "memory");

	slidle_after_wfi(cpu);
}


/************************************************
 * regular idle part
 ************************************************/
static unsigned long rgidle_cnt[NR_CPUS] = {0};

static void rgidle_before_wfi(int cpu)
{
	trace_Cx_to_Cy_event(cpu, 0, 1);
	if (cpu == 0) {
		mt_cirq_wfi_func();
		mt_cirq_enable();
	}
}

static void rgidle_after_wfi(int cpu)
{

	trace_Cx_to_Cy_event(cpu, 1, 0);

	if (cpu == 0) {
		mt_cirq_flush();
		mt_cirq_disable();
	}
	rgidle_cnt[cpu]++;
}

static noinline void go_to_rgidle(int cpu)
{
	rgidle_before_wfi(cpu);

	dsb();
	__asm__ __volatile__("wfi" : : : "memory");

	rgidle_after_wfi(cpu);
}

/************************************************
 * idle task flow part
 ************************************************/

/*
 * xxidle_handler return 1 if enter and exit the low power state
 */
#ifdef SPM_MCDI_FUNC
static inline int mcidle_handler(int cpu)
{
	if (idle_switch[IDLE_TYPE_MC]) {
		if (mcidle_can_enter(cpu)) {
			save_idle_info(cpu, IDLE_TYPE_MC, 1);
			go_to_mcidle(cpu);
			save_idle_info(cpu, IDLE_TYPE_MC, 0);
			return 1;
		}
	}

	return 0;
}
#else
static inline int mcidle_handler(int cpu)
{
	return 0;
}
#endif

static int dpidle_cpu_pdn = 1;

static inline int dpidle_handler(int cpu)
{
	int ret = 0;
	if (idle_switch[IDLE_TYPE_DP]) {
		if (dpidle_can_enter()) {
			save_idle_info(cpu, IDLE_TYPE_DP, 1);
			spm_go_to_dpidle(dpidle_cpu_pdn, 0);
			save_idle_info(cpu, IDLE_TYPE_DP, 0);
			ret = 1;
		}
	}

	return ret;
}

static inline int slidle_handler(int cpu)
{
	int ret = 0;
	if (idle_switch[IDLE_TYPE_SL]) {
		if (slidle_can_enter()) {
			save_idle_info(cpu, IDLE_TYPE_SL, 1);
			go_to_slidle(cpu);
			save_idle_info(cpu, IDLE_TYPE_SL, 0);
			ret = 1;
		}
	}

	return ret;
}

static inline int rgidle_handler(int cpu)
{
	int ret = 0;
	if (idle_switch[IDLE_TYPE_RG]) {
		save_idle_info(cpu, IDLE_TYPE_RG, 1);
		go_to_rgidle(cpu);
		save_idle_info(cpu, IDLE_TYPE_RG, 0);
		ret = 1;
	}

	return ret;
}

static int (*idle_handlers[NR_TYPES])(int) = {
	mcidle_handler,
	dpidle_handler,
	slidle_handler,
	rgidle_handler,
};


void arch_idle(void)
{
	int cpu = smp_processor_id();
	int i;

	for (i = 0; i < NR_TYPES; i++) {
		if (idle_handlers[i](cpu))
			break;
	}
}

#define idle_attr(_name)                         \
static struct kobj_attribute _name##_attr = {   \
	.attr = {                                   \
		.name = __stringify(_name),             \
		.mode = 0644,                           \
	},                                          \
	.show = _name##_show,                       \
	.store = _name##_store,                     \
}

#ifdef SPM_MCDI_FUNC
static ssize_t mcidle_state_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	int cpus, reason, i;

	p += sprintf(p, "*********** multi-core idle state ************\n");
	p += sprintf(p, "mcidle_time_critera=%u\n", mcidle_time_critera);

	for (cpus = 0; cpus < nr_cpu_ids; cpus++) {
		p += sprintf(p, "cpu:%d\n", cpus);
		for (reason = 0; reason < NR_REASONS; reason++) {
			p += sprintf(p, "[%d]mcidle_block_cnt[%s]=%lu\n"
			, reason, reason_name[reason]
			, mcidle_block_cnt[cpus][reason]);
		}
		p += sprintf(p, "\n");
	}

	for (i = 0; i < NR_GRPS; i++) {
		p += sprintf(p, "[%02d]mcidle_condition_mask[%-10s]=0x%08x\t\t"
		, i, grp_get_name(i), mcidle_condition_mask[i]);

		p += sprintf(p, "mcidle_block_mask[%08x]=0x%08x\n"
		, grp_get_name(i), mcidle_block_mask[i]);
	}

	p += sprintf(p, "\n********** mcidle command help **********\n");
	p += sprintf(p, "mcidle help:   cat /sys/power/mcidle_state\n");
	p += sprintf(p, "switch on/off: echo [mcidle] 1/0 ");
	p += sprintf(p, "> /sys/power/mcidle_state\n");
	p += sprintf(p, "en_mc_by_bit:  echo enable id ");
	p += sprintf(p, "> /sys/power/mcidle_state\n");
	p += sprintf(p, "dis_mc_by_bit: echo disable id ");
	p += sprintf(p, "> /sys/power/mcidle_state\n");
	p += sprintf(p, "modify tm_cri: echo time value(dec) ");
	p += sprintf(p, "> /sys/power/mcidle_state\n");

	len = p - buf;
	return len;
}

static ssize_t mcidle_state_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t n)
{
	char cmd[32];
	int param;

	if (sscanf(buf, "%31s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "mcdle"))
			idle_switch[IDLE_TYPE_MC] = param;
		else if (!strcmp(cmd, "enable"))
			enable_mcidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_mcidle_by_bit(param);
		else if (!strcmp(cmd, "time"))
			mcidle_time_critera = param;

		return n;
	} else if (sscanf(buf, "%d", &param) == 1) {
		idle_switch[IDLE_TYPE_MC] = param;
		return n;
	}

	return -EINVAL;
}
idle_attr(mcidle_state);
#endif

static ssize_t dpidle_state_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	int i;

	p += sprintf(p, "*********** deep idle state ************\n");
	p += sprintf(p, "dpidle_cpu_pdn = %d\n", dpidle_cpu_pdn);
	p += sprintf(p, "dpidle_time_critera=%u\n", dpidle_time_critera);

	for (i = 0; i < NR_REASONS; i++) {
		p += sprintf(p, "[%d]dpidle_block_cnt[%s]=%lu\n"
		, i, reason_name[i], dpidle_block_cnt[i]);
	}

	p += sprintf(p, "\n");

	for (i = 0; i < NR_GRPS; i++) {
		p += sprintf(p, "[%02d]dpidle_condition_mask[%-10s]=0x%08x\t\t"
		, i, grp_get_name(i), dpidle_condition_mask[i]);

		p += sprintf(p, "dpidle_block_mask[%-10s]=0x%08x\n",
		grp_get_name(i), dpidle_block_mask[i]);
	}

	p += sprintf(p, "\n*********** dpidle command help  ************\n");
	p += sprintf(p, "dpidle help:   cat /sys/power/dpidle_state\n");
	p += sprintf(p, "switch on/off: echo [dpidle] 1/0 ");
	p += sprintf(p, "> /sys/power/dpidle_state\n");
	p += sprintf(p, "cpupdn on/off: echo cpupdn 1/0 ");
	p += sprintf(p, "> /sys/power/dpidle_state\n");
	p += sprintf(p, "en_dp_by_bit:  echo enable id ");
	p += sprintf(p, "> /sys/power/dpidle_state\n");
	p += sprintf(p, "dis_dp_by_bit: echo disable id ");
	p += sprintf(p, "> /sys/power/dpidle_state\n");
	p += sprintf(p, "modify tm_cri: echo time value(dec) ");
	p += sprintf(p, "> /sys/power/dpidle_state\n");

	len = p - buf;
	return len;
}

static ssize_t dpidle_state_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t n)
{
	char cmd[32];
	int param;

	if (sscanf(buf, "%31s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "dpidle"))
			idle_switch[IDLE_TYPE_DP] = param;
		else if (!strcmp(cmd, "enable"))
			enable_dpidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_dpidle_by_bit(param);
		else if (!strcmp(cmd, "cpupdn"))
			dpidle_cpu_pdn = !!param;
		else if (!strcmp(cmd, "time"))
			dpidle_time_critera = param;

		return n;
	} else if (sscanf(buf, "%d", &param) == 1) {
		idle_switch[IDLE_TYPE_DP] = param;
		return n;
	}

	return -EINVAL;
}
idle_attr(dpidle_state);

static ssize_t slidle_state_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	int i;

	p += sprintf(p, "*********** slow idle state ************\n");
	for (i = 0; i < NR_REASONS; i++) {
		p += sprintf(p, "[%d]slidle_block_cnt[%s]=%lu\n",
		i, reason_name[i], slidle_block_cnt[i]);
	}

	p += sprintf(p, "\n");

	for (i = 0; i < NR_GRPS; i++) {
		p += sprintf(p, "[%02d]slidle_condition_mask[%-10s]=0x%08x\t\t"
		, i, grp_get_name(i), slidle_condition_mask[i]);

		p += sprintf(p, "slidle_block_mask[%-10s]=0x%08x\n",
		grp_get_name(i), slidle_block_mask[i]);
	}

	p += sprintf(p, "\n********** slidle command help **********\n");
	p += sprintf(p, "slidle help:   cat /sys/power/slidle_state\n");
	p += sprintf(p, "switch on/off: echo [slidle] 1/0 ");
	p += sprintf(p, "> /sys/power/slidle_state\n");

	len = p - buf;
	return len;
}

static ssize_t slidle_state_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t n)
{
	char cmd[32];
	int param;

	if (sscanf(buf, "%31s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "slidle"))
			idle_switch[IDLE_TYPE_SL] = param;
		else if (!strcmp(cmd, "enable"))
			enable_slidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_slidle_by_bit(param);

		return n;
	} else if (sscanf(buf, "%d", &param) == 1) {
		idle_switch[IDLE_TYPE_SL] = param;
		return n;
	}

	return -EINVAL;
}
idle_attr(slidle_state);

static ssize_t rgidle_state_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	p += sprintf(p, "*********** regular idle state ************\n");
	p += sprintf(p, "\n********** rgidle command help **********\n");
	p += sprintf(p, "rgidle help:   cat /sys/power/rgidle_state\n");
	p += sprintf(p,
	"switch on/off: echo [rgidle] 1/0 > /sys/power/rgidle_state\n");

	len = p - buf;
	return len;
}

static ssize_t rgidle_state_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t n)
{
	char cmd[32];
	int param;

	if (sscanf(buf, "%31s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "rgidle"))
			idle_switch[IDLE_TYPE_RG] = param;

		return n;
	} else if (sscanf(buf, "%d", &param) == 1) {
		idle_switch[IDLE_TYPE_RG] = param;
		return n;
	}

	return -EINVAL;
}
idle_attr(rgidle_state);

static ssize_t idle_state_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	char *p = buf;

	int i;

	p += sprintf(p, "********** idle state dump **********\n");
#ifdef SPM_MCDI_FUNC
	for (i = 0; i < nr_cpu_ids; i++) {
		p += sprintf(p, "mcidle_cnt[%d]=%lu, dpidle_cnt[%d]=%lu, "
		i, mcidle_cnt[i], i, dpidle_cnt[i]);

		p += sprintf(p, "slidle_cnt[%d]=%lu, rgidle_cnt[%d]=%lu\n",
		i, slidle_cnt[i], i, rgidle_cnt[i]);
	}
#else
	for (i = 0; i < nr_cpu_ids; i++) {
		p += sprintf(p,
	"dpidle_cnt[%d]=%lu, slidle_cnt[%d]=%lu, rgidle_cnt[%d]=%lu\n",
	i, dpidle_cnt[i], i, slidle_cnt[i], i, rgidle_cnt[i]);
	}
#endif

	p += sprintf(p, "\n********** variables dump **********\n");
	for (i = 0; i < NR_TYPES; i++)
		p += sprintf(p, "%s_switch=%d, ", idle_name[i], idle_switch[i]);

	p += sprintf(p, "\n");

	p += sprintf(p, "\n********** idle command help **********\n");
	p += sprintf(p, "status help:   cat /sys/power/idle_state\n");
	p += sprintf(p,
	"switch on/off: echo switch mask > /sys/power/idle_state\n");

#ifdef SPM_MCDI_FUNC
	p += sprintf(p, "mcidle help:   cat /sys/power/mcidle_state\n");
#else
	p += sprintf(p, "mcidle help:   mcidle is unavailable\n");
#endif
	p += sprintf(p, "dpidle help:   cat /sys/power/dpidle_state\n");
	p += sprintf(p, "slidle help:   cat /sys/power/slidle_state\n");
	p += sprintf(p, "rgidle help:   cat /sys/power/rgidle_state\n");

	len = p - buf;
	return len;
}

static ssize_t idle_state_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t n)
{
	char cmd[32];
	int idx;
	int param;

	if (sscanf(buf, "%31s %x", cmd, &param) == 2) {
		if (!strcmp(cmd, "switch")) {
			for (idx = 0; idx < NR_TYPES; idx++) {
#ifndef SPM_MCDI_FUNC
				if (idx == IDLE_TYPE_MC)
					continue;
#endif
				idle_switch[idx]
				= (param & (1U << idx)) ? 1 : 0;
			}
		}
		return n;
	}

	return -EINVAL;
}
idle_attr(idle_state);


void mt_idle_init(void)
{
	int err = 0;

	idle_info("[%s]entry!!\n", __func__);
	arm_pm_idle = arch_idle;

#ifndef SPM_MCDI_FUNC
	idle_switch[IDLE_TYPE_MC] = 0;
#endif

	err = free_gpt(GPT1);
	if (err)
		idle_info("[%s]fail to free GPT1\n", __func__);

	err = request_gpt(GPT1, GPT_ONE_SHOT, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1,
		0, NULL, GPT_NOAUTOEN);
	if (err)
		idle_info("[%s]fail to request GPT1\n", __func__);

	err = request_gpt(GPT4, GPT_ONE_SHOT, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1,
		0, NULL, GPT_NOAUTOEN);
	if (err)
		idle_info("[%s]fail to request GPT4\n", __func__);

	err = request_gpt(GPT5, GPT_ONE_SHOT, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1,
		0, NULL, GPT_NOAUTOEN);
	if (err)
		idle_info("[%s]fail to request GPT5\n", __func__);

	err = sysfs_create_file(power_kobj, &idle_state_attr.attr);
#ifdef SPM_MCDI_FUNC
	err |= sysfs_create_file(power_kobj, &mcidle_state_attr.attr);
#endif
	err |= sysfs_create_file(power_kobj, &dpidle_state_attr.attr);
	err |= sysfs_create_file(power_kobj, &slidle_state_attr.attr);
	err |= sysfs_create_file(power_kobj, &rgidle_state_attr.attr);

	if (err)
		idle_err("[%s]: fail to create sysfs\n", __func__);
}
