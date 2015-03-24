#ifdef BUILD_UBOOT
#define ENABLE_DSI_INTERRUPT 0

#include <asm/arch/disp_drv_platform.h>
#else

#define ENABLE_DSI_INTERRUPT 1

#include <linux/delay.h>
#include <disp_drv_log.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include "disp_drv_platform.h"
#include <linux/module.h>
#include <mach/mt_boot.h>

#include "lcd_reg.h"
#include "lcd_drv.h"

#include "dsi_reg.h"
#include "dsi_drv.h"
#endif

extern unsigned int EnableVSyncLog;

#if ENABLE_DSI_INTERRUPT
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <mach/irqs.h>
#include "mtkfb.h"
static wait_queue_head_t _dsi_wait_queue;
static wait_queue_head_t _dsi_dcs_read_wait_queue;
static wait_queue_head_t _dsi_wait_bta_te;
static wait_queue_head_t _dsi_wait_vm_done_queue;
#endif
/* static unsigned int _dsi_reg_update_wq_flag = 0; //for fixed warning */
static DECLARE_WAIT_QUEUE_HEAD(_dsi_reg_update_wq);

#include "debug.h"

/* #define ENABLE_DSI_ERROR_REPORT */

/*
#define PLL_BASE			(0xF0060000)
#define DSI_PHY_BASE		(0xF0060B00)
#define DSI_BASE		(0xF0140000)
*/

#if !(defined(CONFIG_MT8135_FPGA) || defined(BUILD_UBOOT))
/* #define DSI_MIPI_API */
#endif

#include <mach/sync_write.h>
#ifdef OUTREG32
#undef OUTREG32
#define OUTREG32(x, y) mt65xx_reg_sync_writel(y, x)
#endif

#ifndef OUTREGBIT
#define OUTREGBIT(TYPE, REG, bit, value)  \
		    do {    \
			TYPE r = *((TYPE *)&INREG32(&REG));    \
			r.bit = value;    \
			OUTREG32(&REG, AS_UINT32(&r));    \
		    } while (0)
#endif

static PDSI_REGS const DSI_REG = (PDSI_REGS) (DSI_BASE);
static PDSI_PHY_REGS const DSI_PHY_REG = (PDSI_PHY_REGS) (MIPI_CONFIG_BASE);
static PDSI_CMDQ_REGS const DSI_CMDQ_REG = (PDSI_CMDQ_REGS) (DSI_BASE + 0x180);
static PLCD_REGS const LCD_REG = (PLCD_REGS) (LCD_BASE);
static PDSI_VM_CMDQ_REGS const DSI_VM_CMD_REG = (PDSI_VM_CMDQ_REGS)(DSI_BASE + 0x134);

extern LCM_DRIVER *lcm_drv;
static bool dsi_log_on;

static const DISP_IF_DRIVER *disp_if_drv;
extern bool needStartDSI;

typedef struct {
	DSI_REGS regBackup;
	unsigned int cmdq_size;
	DSI_CMDQ_REGS cmdqBackup;
	unsigned int bit_time_ns;
	unsigned int vfp_period_us;
	unsigned int vsa_vs_period_us;
	unsigned int vsa_hs_period_us;
	unsigned int vsa_ve_period_us;
	unsigned int vbp_period_us;
	void (*pIntCallback) (DISP_INTERRUPT_EVENTS);
} DSI_CONTEXT;

static bool s_isDsiPowerOn = FALSE;
static DSI_CONTEXT _dsiContext;
extern LCM_PARAMS *lcm_params;

DSI_PLL_CONFIG pll_config[50] = { {1, 0, 0, 0x3E, 0, 2, 3, 8, 4, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_201_5  1 */
{1, 0, 0, 0x40, 0, 2, 2, 4, 1, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_208    2 */
{1, 0, 0, 0x42, 0, 2, 3, 9, 4, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_214_5  3 */
{1, 0, 0, 0x44, 0, 2, 2, 4, 1, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_221    4 */
{1, 0, 0, 0x46, 0, 2, 3, 9, 4, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_227_5  5 */
{1, 0, 0, 0x48, 0, 2, 2, 4, 1, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_234    6 */
{1, 0, 0, 0x4A, 0, 2, 3, 0xA, 5, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_240_5  7 */
{1, 0, 0, 0x4C, 0, 2, 2, 5, 2, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_247    8 */
{0, 0, 0, 0x27, 0, 2, 3, 0xA, 4, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_253_5  9 */
{0, 0, 0, 0x28, 0, 2, 2, 5, 2, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_260    10 */
{0, 0, 0, 0x29, 0, 2, 3, 0xA, 4, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_266_5  11 */
{0, 0, 0, 0x2A, 0, 2, 2, 5, 2, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_273    12 */
{0, 0, 0, 0x2B, 0, 2, 3, 0xB, 5, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_279_5  13 */
{0, 0, 0, 0x2C, 0, 2, 2, 6, 2, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_286    14 */
{0, 0, 0, 0x2D, 0, 2, 3, 0xB, 5, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_292_5  15 */
{0, 0, 0, 0x2E, 0, 2, 2, 6, 2, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_299    16 */
{0, 0, 0, 0x2F, 0, 2, 3, 0xC, 5, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_305_5  17 */
{0, 0, 0, 0x30, 0, 2, 2, 6, 2, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_312    18 */
{0, 0, 0, 0x31, 0, 2, 3, 0xD, 5, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_318_5  19 */
{0, 0, 0, 0x32, 0, 2, 2, 6, 2, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_325    20 */
{0, 0, 0, 0x33, 0, 2, 3, 0xE, 6, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_331_5  21 */
{0, 0, 0, 0x34, 0, 2, 2, 6, 2, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_338    22 */
{0, 0, 0, 0x35, 0, 1, 3, 0x7, 6, 0x6},	/* LCM_DSI_6589_PLL_CLOCK_344_5  23 */
{0, 0, 0, 0x36, 0, 2, 2, 7, 2, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_351    24 */
{0, 0, 0, 0x37, 0, 1, 3, 0x7, 7, 0x6},	/* LCM_DSI_6589_PLL_CLOCK_357_5  25 */
{0, 0, 0, 0x38, 0, 2, 2, 5, 2, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_364    26 */
{0, 0, 0, 0x39, 0, 2, 2, 0xA, 5, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_370_5  27 */
{0, 0, 0, 0x3A, 0, 2, 2, 0xA, 5, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_377    28 */
{0, 0, 0, 0x3B, 0, 1, 3, 5, 3, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_383_5  29 */
{0, 0, 0, 0x3C, 0, 2, 2, 5, 2, 0x9},	/* LCM_DSI_6589_PLL_CLOCK_390    30 */
{0, 0, 0, 0x3D, 0, 1, 3, 5, 3, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_396_5  31 */
{0, 0, 0, 0x3E, 0, 1, 2, 5, 5, 0x4},	/* LCM_DSI_6589_PLL_CLOCK_403    32 */
{0, 0, 0, 0x3F, 0, 1, 3, 5, 3, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_409_5  33 */
{0, 0, 0, 0x40, 0, 2, 2, 5, 2, 0xA},	/* LCM_DSI_6589_PLL_CLOCK_416    34 */
{0, 0, 0, 0x41, 0, 1, 3, 5, 3, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_422_5  35 */
{0, 0, 0, 0x42, 0, 1, 2, 6, 5, 0x4},	/* LCM_DSI_6589_PLL_CLOCK_429    36 */
{0, 0, 0, 0x43, 0, 1, 3, 6, 3, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_435_5  37 */
{0, 0, 0, 0x44, 0, 2, 2, 6, 2, 0xA},	/* LCM_DSI_6589_PLL_CLOCK_442    38 */
{0, 0, 0, 0x45, 0, 1, 3, 6, 3, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_448_5  39 */
{0, 0, 0, 0x46, 0, 1, 2, 6, 6, 0x4},	/* LCM_DSI_6589_PLL_CLOCK_455    40 */
{0, 0, 0, 0x47, 0, 1, 3, 6, 4, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_461_5  41 */
{0, 0, 0, 0x48, 0, 2, 2, 5, 2, 0xA},	/* LCM_DSI_6589_PLL_CLOCK_468    42 */
{0, 0, 0, 0x49, 0, 1, 3, 6, 4, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_474_5  43 */
{0, 0, 0, 0x4A, 0, 1, 2, 6, 6, 0x4},	/* LCM_DSI_6589_PLL_CLOCK_481    44 */
{0, 0, 0, 0x4B, 0, 1, 3, 7, 4, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_487_5  45 */
{0, 0, 0, 0x4C, 0, 2, 2, 5, 2, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_494    46 */
{0, 0, 0, 0x4D, 0, 1, 3, 7, 4, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_500_5  47 */
{0, 0, 0, 0x4E, 0, 1, 2, 7, 6, 0x4},	/* LCM_DSI_6589_PLL_CLOCK_507    48 */
{0, 0, 0, 0x4F, 0, 1, 3, 7, 4, 0x8},	/* LCM_DSI_6589_PLL_CLOCK_513_5  49 */
{0, 0, 0, 0x50, 0, 2, 2, 5, 2, 0xC},	/* LCM_DSI_6589_PLL_CLOCK_520    50 */
};

#ifndef BUILD_UBOOT

#ifndef MT65XX_NEW_DISP		/* for fixed warning */
static bool dsi_esd_recovery;
static bool dsi_noncont_clk_enabled;
static unsigned int dsi_noncont_clk_period = 1;
static bool dsi_int_te_enabled;
static unsigned int dsi_int_te_period = 1;
static unsigned int dsi_dpi_isr_count;
#endif				/* for fixed warning */
unsigned long g_handle_esd_flag;

static volatile bool lcdStartTransfer;
static volatile bool dsiTeEnable = true;

#endif

#ifdef BUILD_UBOOT
static long int get_current_time_us(void)
{
	return 0;		/* /TODO: fix me */
}
#else
static long int get_current_time_us(void)
{
	struct timeval t;
	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}
#endif
static void lcm_mdelay(UINT32 ms)
{
	msleep(ms);
}

void DSI_Enable_Log(bool enable)
{
	dsi_log_on = enable;
}

static wait_queue_head_t _vsync_wait_queue;
static bool dsi_vsync;
static bool wait_dsi_vsync;
static struct hrtimer hrtimer_vsync;
#define VSYNC_US_TO_NS(x) (x * 1000)
static unsigned int vsync_timer;

#if ENABLE_DSI_INTERRUPT
static irqreturn_t _DSI_InterruptHandler(int irq, void *dev_id)
{
	DSI_INT_STATUS_REG status = DSI_REG->DSI_INTSTA;
#ifdef ENABLE_DSI_ERROR_REPORT
	static unsigned int prev_error;
#endif

	MMProfileLogEx(MTKFB_MMP_Events.DSIIRQ, MMProfileFlagPulse, *(unsigned int *)&status,
		       lcdStartTransfer);
	if (dsi_log_on)
		pr_info("DSI IRQ, value = 0x%x!!\n", INREG32(0xF400D00C));

	if (status.RD_RDY) {
		/* /write clear RD_RDY interrupt */

		/* / write clear RD_RDY interrupt must be before DSI_RACK */
		/* / because CMD_DONE will raise after DSI_RACK, */
		/* / so write clear RD_RDY after that will clear CMD_DONE too */
#ifdef ENABLE_DSI_ERROR_REPORT
		{
			unsigned int read_data[4];
			OUTREG32(&read_data[0], AS_UINT32(&DSI_REG->DSI_RX_DATA0));
			OUTREG32(&read_data[1], AS_UINT32(&DSI_REG->DSI_RX_DATA1));
			OUTREG32(&read_data[2], AS_UINT32(&DSI_REG->DSI_RX_DATA2));
			OUTREG32(&read_data[3], AS_UINT32(&DSI_REG->DSI_TRIG_STA));
			if (dsi_log_on) {
				if ((read_data[0] & 0x3) == 0x02) {
					if (read_data[0] & (~prev_error))
						pr_info
						    ("[DSI] Detect DSI error. prev:0x%08X new:0x%08X\n",
						     prev_error, read_data[0]);
				} else if ((read_data[1] & 0x3) == 0x02) {
					if (read_data[1] & (~prev_error))
						pr_info
						    ("[DSI] Detect DSI error. prev:0x%08X new:0x%08X\n",
						     prev_error, read_data[1]);
				}
			}
			MMProfileLogEx(MTKFB_MMP_Events.DSIRead, MMProfileFlagStart, read_data[0],
				       read_data[1]);
			MMProfileLogEx(MTKFB_MMP_Events.DSIRead, MMProfileFlagEnd, read_data[2],
				       read_data[3]);
		}
#endif
		do {
			/* /send read ACK */
			/* DSI_REG->DSI_RACK.DSI_RACK = 1; */
			OUTREGBIT(DSI_RACK_REG, DSI_REG->DSI_RACK, DSI_RACK, 1);
		} while (DSI_REG->DSI_INTSTA.BUSY);

		MASKREG32(&DSI_REG->DSI_INTSTA, 0x1, 0x0);
		wake_up_interruptible(&_dsi_dcs_read_wait_queue);
		if (_dsiContext.pIntCallback)
			_dsiContext.pIntCallback(DISP_DSI_READ_RDY_INT);
	}

	if (status.CMD_DONE) {
		if (lcdStartTransfer) {
			/* The last screen update has finished. */
			if (_dsiContext.pIntCallback)
				_dsiContext.pIntCallback(DISP_DSI_CMD_DONE_INT);
			DBG_OnLcdDone();
		}
		/* clear flag & wait for next trigger */
		lcdStartTransfer = false;

		/* DSI_REG->DSI_INTSTA.CMD_DONE = 0; */
		OUTREGBIT(DSI_INT_STATUS_REG, DSI_REG->DSI_INTSTA, CMD_DONE, 0);

		wake_up_interruptible(&_dsi_wait_queue);
		/* if(_dsiContext.pIntCallback) */
		/* _dsiContext.pIntCallback(DISP_DSI_CMD_DONE_INT); */
		/* MASKREG32(&DSI_REG->DSI_INTSTA, 0x2, 0x0); */
	}

	if (status.TE_RDY) {
		DBG_OnTeDelayDone();

		/* Write clear RD_RDY */
		/* DSI_REG->DSI_INTSTA.TE_RDY = 0; */
		OUTREGBIT(DSI_INT_STATUS_REG, DSI_REG->DSI_INTSTA, TE_RDY, 0);

		/* Set DSI_RACK to let DSI idle */
		/* DSI_REG->DSI_RACK.DSI_RACK = 1; */
		OUTREGBIT(DSI_RACK_REG, DSI_REG->DSI_RACK, DSI_RACK, 1);

		wake_up_interruptible(&_dsi_wait_bta_te);


#ifndef BUILD_UBOOT
		if (wait_dsi_vsync)	/* judge if wait vsync */
		{
			if (EnableVSyncLog)
				pr_info("[DSI] VSync2\n");
			if (-1 != hrtimer_try_to_cancel(&hrtimer_vsync)) {
				dsi_vsync = true;
				/* hrtimer_try_to_cancel(&hrtimer_vsync); */
				if (EnableVSyncLog)
					pr_info("[DSI] VSync3\n");
				wake_up_interruptible(&_vsync_wait_queue);
			}
			/* pr_info("TE signal, and wake up\n"); */
		}
#endif
	}

	if (status.VM_DONE) {
/* DBG_OnTeDelayDone(); */
		OUTREGBIT(DSI_INT_STATUS_REG, DSI_REG->DSI_INTSTA, VM_DONE, 0);
		if (_dsiContext.pIntCallback)
			_dsiContext.pIntCallback(DISP_DSI_VMDONE_INT);
		if (dsi_log_on)
			pr_info("DSI VM done IRQ!!\n");
		/* Write clear VM_Done */
		/* DSI_REG->DSI_INTSTA.VM_DONE= 0; */
		wake_up_interruptible(&_dsi_wait_vm_done_queue);
	}

	return IRQ_HANDLED;

}
#endif
#ifndef BUILD_UBOOT
void DSI_GetVsyncCnt(void)
{
}

enum hrtimer_restart dsi_te_hrtimer_func(struct hrtimer *timer)
{
/* long long ret; */
	if (EnableVSyncLog)
		pr_info("[DSI] VSync0\n");
	if (wait_dsi_vsync) {
		dsi_vsync = true;

		if (EnableVSyncLog)
			pr_info("[DSI] VSync1\n");
		wake_up_interruptible(&_vsync_wait_queue);
	}
/* ret = hrtimer_forward_now(timer, ktime_set(0, VSYNC_US_TO_NS(vsync_timer))); */
/* pr_info("hrtimer callback\n"); */
	return HRTIMER_NORESTART;
}
#endif


/* static unsigned int vsync_wait_time = 0; //for fixed warning */
void DSI_WaitTE(void)
{
#ifndef BUILD_UBOOT
	wait_dsi_vsync = true;

	hrtimer_start(&hrtimer_vsync, ktime_set(0, VSYNC_US_TO_NS(vsync_timer)), HRTIMER_MODE_REL);
	if (EnableVSyncLog)
		pr_info("[DSI] +VSync\n");
	wait_event_interruptible(_vsync_wait_queue, dsi_vsync);
	if (EnableVSyncLog)
		pr_info("[DSI] -VSync\n");
	dsi_vsync = false;
	wait_dsi_vsync = false;
#endif
}

void DSI_InitVSYNC(unsigned int vsync_interval)
{
#ifndef BUILD_UBOOT
	ktime_t ktime;
	vsync_timer = vsync_interval;
	ktime = ktime_set(0, VSYNC_US_TO_NS(vsync_timer));
	hrtimer_init(&hrtimer_vsync, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_vsync.function = dsi_te_hrtimer_func;
/* hrtimer_start(&hrtimer_vsync, ktime, HRTIMER_MODE_REL); */
#endif
}

static BOOL _IsEngineBusy(void)
{
	DSI_INT_STATUS_REG status;

	status = DSI_REG->DSI_INTSTA;

	if (status.BUSY)
		return TRUE;
	return FALSE;
}

#if 0
static BOOL _IsCMDQBusy(void)
{
	DSI_INT_STATUS_REG INT_status;

	INT_status = DSI_REG->DSI_INTSTA;

	if (!INT_status.CMD_DONE) {
		DISP_LOG_PRINT(ANDROID_LOG_WARN, "DSI", " DSI CMDQ status BUSY !!\n");

		return TRUE;
	}

	return FALSE;
}
#endif

static void _WaitForEngineNotBusy(void)
{
	int timeOut;
#if ENABLE_DSI_INTERRUPT
	long int time;
	static const long WAIT_TIMEOUT = 2 * HZ;	/* 2 sec */
#endif

	if (DSI_REG->DSI_MODE_CTRL.MODE)
		return;

	timeOut = 200;

#if ENABLE_DSI_INTERRUPT
	time = get_current_time_us();

	if (in_interrupt()) {
		/* perform busy waiting if in interrupt context */
		while (_IsEngineBusy()) {
			msleep(1);
			if (--timeOut < 0) {

				DISP_LOG_PRINT(ANDROID_LOG_ERROR, "DSI",
					       " Wait for DSI engine not busy timeout!!!(Wait %d us)\n",
					       get_current_time_us() - time);
				DSI_DumpRegisters();
				DSI_Reset();

				break;
			}
		}
	} else {
		while (DSI_REG->DSI_INTSTA.BUSY || DSI_REG->DSI_INTSTA.CMD_DONE) {
			long ret = wait_event_interruptible_timeout(_dsi_wait_queue,
								    !_IsEngineBusy()
								    && !(DSI_REG->DSI_INTSTA.
									 CMD_DONE),
								    WAIT_TIMEOUT);
			if (0 == ret) {
				DISP_LOG_PRINT(ANDROID_LOG_WARN, "DSI",
					       " Wait for DSI engine not busy timeout!!!\n");
				DSI_DumpRegisters();
				DSI_Reset();
				dsiTeEnable = false;
			}
		}
	}
#else

	while (_IsEngineBusy()) {
		udelay(100);
		/*pr_info("xuecheng, dsi wait\n"); */
		if (--timeOut < 0) {
			DISP_LOG_PRINT(ANDROID_LOG_ERROR, "DSI",
				       " Wait for DSI engine not busy timeout!!!\n");
			DSI_DumpRegisters();
			DSI_Reset();
			dsiTeEnable = false;
			break;
		}
	}
	OUTREG32(&DSI_REG->DSI_INTSTA, 0x0);
#endif
}

void hdmi_dsi_waitnotbusy(void)
{
	_WaitForEngineNotBusy();
}

static void _BackupDSIRegisters(void)
{
	DSI_REGS *regs = &(_dsiContext.regBackup);

	/* memcpy((void*)&(_dsiContext.regBackup), (void*)DSI_BASE, sizeof(DSI_REGS)); */

	OUTREG32(&regs->DSI_INTEN, AS_UINT32(&DSI_REG->DSI_INTEN));
	OUTREG32(&regs->DSI_MODE_CTRL, AS_UINT32(&DSI_REG->DSI_MODE_CTRL));
	OUTREG32(&regs->DSI_TXRX_CTRL, AS_UINT32(&DSI_REG->DSI_TXRX_CTRL));
	OUTREG32(&regs->DSI_PSCTRL, AS_UINT32(&DSI_REG->DSI_PSCTRL));

	OUTREG32(&regs->DSI_VSA_NL, AS_UINT32(&DSI_REG->DSI_VSA_NL));
	OUTREG32(&regs->DSI_VBP_NL, AS_UINT32(&DSI_REG->DSI_VBP_NL));
	OUTREG32(&regs->DSI_VFP_NL, AS_UINT32(&DSI_REG->DSI_VFP_NL));
	OUTREG32(&regs->DSI_VACT_NL, AS_UINT32(&DSI_REG->DSI_VACT_NL));

	OUTREG32(&regs->DSI_HSA_WC, AS_UINT32(&DSI_REG->DSI_HSA_WC));
	OUTREG32(&regs->DSI_HBP_WC, AS_UINT32(&DSI_REG->DSI_HBP_WC));
	OUTREG32(&regs->DSI_HFP_WC, AS_UINT32(&DSI_REG->DSI_HFP_WC));
	OUTREG32(&regs->DSI_BLLP_WC, AS_UINT32(&DSI_REG->DSI_BLLP_WC));

	OUTREG32(&regs->DSI_HSTX_CKL_WC, AS_UINT32(&DSI_REG->DSI_HSTX_CKL_WC));
	OUTREG32(&regs->DSI_MEM_CONTI, AS_UINT32(&DSI_REG->DSI_MEM_CONTI));

	OUTREG32(&regs->DSI_PHY_TIMECON0, AS_UINT32(&DSI_REG->DSI_PHY_TIMECON0));
	OUTREG32(&regs->DSI_PHY_TIMECON1, AS_UINT32(&DSI_REG->DSI_PHY_TIMECON1));
	OUTREG32(&regs->DSI_PHY_TIMECON2, AS_UINT32(&DSI_REG->DSI_PHY_TIMECON2));
	OUTREG32(&regs->DSI_PHY_TIMECON3, AS_UINT32(&DSI_REG->DSI_PHY_TIMECON3));
	OUTREG32(&regs->DSI_VM_CMD_CON, AS_UINT32(&DSI_REG->DSI_VM_CMD_CON));

}

static void _RestoreDSIRegisters(void)
{
	DSI_REGS *regs = &(_dsiContext.regBackup);

	OUTREG32(&DSI_REG->DSI_INTEN, AS_UINT32(&regs->DSI_INTEN));
	OUTREG32(&DSI_REG->DSI_MODE_CTRL, AS_UINT32(&regs->DSI_MODE_CTRL));
	OUTREG32(&DSI_REG->DSI_TXRX_CTRL, AS_UINT32(&regs->DSI_TXRX_CTRL));
	OUTREG32(&DSI_REG->DSI_PSCTRL, AS_UINT32(&regs->DSI_PSCTRL));

	OUTREG32(&DSI_REG->DSI_VSA_NL, AS_UINT32(&regs->DSI_VSA_NL));
	OUTREG32(&DSI_REG->DSI_VBP_NL, AS_UINT32(&regs->DSI_VBP_NL));
	OUTREG32(&DSI_REG->DSI_VFP_NL, AS_UINT32(&regs->DSI_VFP_NL));
	OUTREG32(&DSI_REG->DSI_VACT_NL, AS_UINT32(&regs->DSI_VACT_NL));

	OUTREG32(&DSI_REG->DSI_HSA_WC, AS_UINT32(&regs->DSI_HSA_WC));
	OUTREG32(&DSI_REG->DSI_HBP_WC, AS_UINT32(&regs->DSI_HBP_WC));
	OUTREG32(&DSI_REG->DSI_HFP_WC, AS_UINT32(&regs->DSI_HFP_WC));
	OUTREG32(&DSI_REG->DSI_BLLP_WC, AS_UINT32(&regs->DSI_BLLP_WC));

	OUTREG32(&DSI_REG->DSI_HSTX_CKL_WC, AS_UINT32(&regs->DSI_HSTX_CKL_WC));
	OUTREG32(&DSI_REG->DSI_MEM_CONTI, AS_UINT32(&regs->DSI_MEM_CONTI));

	OUTREG32(&DSI_REG->DSI_PHY_TIMECON0, AS_UINT32(&regs->DSI_PHY_TIMECON0));
	OUTREG32(&DSI_REG->DSI_PHY_TIMECON1, AS_UINT32(&regs->DSI_PHY_TIMECON1));
	OUTREG32(&DSI_REG->DSI_PHY_TIMECON2, AS_UINT32(&regs->DSI_PHY_TIMECON2));
	OUTREG32(&DSI_REG->DSI_PHY_TIMECON3, AS_UINT32(&regs->DSI_PHY_TIMECON3));
	OUTREG32(&DSI_REG->DSI_VM_CMD_CON, AS_UINT32(&regs->DSI_VM_CMD_CON));
	}

static void _ResetBackupedDSIRegisterValues(void)
{
	DSI_REGS *regs = &_dsiContext.regBackup;
	memset((void *)regs, 0, sizeof(DSI_REGS));
}


static void DSI_BackUpCmdQ(void)
{
	unsigned int i;
	DSI_CMDQ_REGS *regs = &(_dsiContext.cmdqBackup);

	_dsiContext.cmdq_size = AS_UINT32(&DSI_REG->DSI_CMDQ_SIZE);

	for (i = 0; i < _dsiContext.cmdq_size; i++)
		OUTREG32(&regs->data[i], AS_UINT32(&DSI_CMDQ_REG->data[i]));
}


static void DSI_RestoreCmdQ(void)
{
	unsigned int i;
	DSI_CMDQ_REGS *regs = &(_dsiContext.cmdqBackup);

	OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, AS_UINT32(&_dsiContext.cmdq_size));

	for (i = 0; i < _dsiContext.cmdq_size; i++)
		OUTREG32(&DSI_CMDQ_REG->data[i], AS_UINT32(&regs->data[i]));
}

static void _DSI_RDMA0_IRQ_Handler(unsigned int param);
DSI_STATUS DSI_Init(BOOL isDsiPoweredOn)
{
	DSI_STATUS ret = DSI_STATUS_OK;

	memset(&_dsiContext, 0, sizeof(_dsiContext));

	if (isDsiPoweredOn) {
		_BackupDSIRegisters();
	} else {
		_ResetBackupedDSIRegisterValues();
	}
	ret = DSI_PowerOn();

	OUTREG32(&DSI_REG->DSI_MEM_CONTI, DSI_WMEM_CONTI);

	ASSERT(ret == DSI_STATUS_OK);
/*
    mipitx_con0=DSI_PHY_REG->MIPITX_CON0;
    mipitx_con1=DSI_PHY_REG->MIPITX_CON1;
    mipitx_con3=DSI_PHY_REG->MIPITX_CON3;
    mipitx_con6=DSI_PHY_REG->MIPITX_CON6;
    mipitx_con8=DSI_PHY_REG->MIPITX_CON8;
    mipitx_con9=DSI_PHY_REG->MIPITX_CON9;
*/
#if ENABLE_DSI_INTERRUPT
	init_waitqueue_head(&_dsi_wait_queue);
	init_waitqueue_head(&_dsi_dcs_read_wait_queue);
	init_waitqueue_head(&_dsi_wait_bta_te);
	init_waitqueue_head(&_dsi_wait_vm_done_queue);
	if (request_irq(MT8135_DISP_DSI_IRQ_ID,
			_DSI_InterruptHandler, IRQF_TRIGGER_LOW, MTKFB_DRIVER, NULL) < 0) {
		DISP_LOG_PRINT(ANDROID_LOG_ERROR, "DSI", "fail to request DSI irq\n");
		return DSI_STATUS_ERROR;
	}
	/* mt65xx_irq_unmask(MT6577_DSI_IRQ_ID); */
	/* DSI_REG->DSI_INTEN.CMD_DONE=1; */
	/* DSI_REG->DSI_INTEN.RD_RDY=1; */
	/* DSI_REG->DSI_INTEN.TE_RDY = 1; */
	OUTREGBIT(DSI_INT_ENABLE_REG, DSI_REG->DSI_INTEN, CMD_DONE, 1);
	OUTREGBIT(DSI_INT_ENABLE_REG, DSI_REG->DSI_INTEN, RD_RDY, 1);
	OUTREGBIT(DSI_INT_ENABLE_REG, DSI_REG->DSI_INTEN, TE_RDY, 1);

	init_waitqueue_head(&_vsync_wait_queue);
	/* DSI_REG->DSI_INTEN.VM_DONE = 1; */
	OUTREGBIT(DSI_INT_ENABLE_REG, DSI_REG->DSI_INTEN, VM_DONE, 1);
	init_waitqueue_head(&_vsync_wait_queue);


#endif
	/* if (lcm_params->dsi.mode == DSI_CMD_MODE) //fix warning */
	if (lcm_params->dsi.mode == CMD_MODE)
		disp_register_irq(DISP_MODULE_RDMA0, _DSI_RDMA0_IRQ_Handler);

	return DSI_STATUS_OK;
}


DSI_STATUS DSI_Deinit(void)
{
	DSI_STATUS ret = DSI_PowerOff();

	ASSERT(ret == DSI_STATUS_OK);

	return DSI_STATUS_OK;
}

#ifdef BUILD_UBOOT
DSI_STATUS DSI_PowerOn(void)
{

	if (!s_isDsiPowerOn) {
#if 0				/* FIXME */
		BOOL ret = hwEnableClock(MT_CG_DISP1_DSI_ENGINE, "DSI");
		BOOL ret = hwEnableClock(MT_CG_DISP1_DSI_DIGITAL, "DSI");
		BOOL ret = hwEnableClock(MT_CG_DISP1_DSI_LANE, "DSI");
		ASSERT(ret);
#else
		MASKREG32(0x14000110, 0x38, 0x0);
		printf("[DISP] - uboot - DSI_PowerOn. 0x%8x\n", INREG32(0x14000110));
#endif
		_RestoreDSIRegisters();
		/* _WaitForEngineNotBusy(); */
		s_isDsiPowerOn = TRUE;
	}

	return DSI_STATUS_OK;
}


DSI_STATUS DSI_PowerOff(void)
{

	if (s_isDsiPowerOn) {
		BOOL ret = TRUE;
		/* _WaitForEngineNotBusy(); */
		_BackupDSIRegisters();
#if 0				/* FIXME */
		BOOL ret = hwDisableClock(MT_CG_DISP1_DSI_ENGINE, "DSI");
		BOOL ret = hwDisableClock(MT_CG_DISP1_DSI_DIGITAL, "DSI");
		BOOL ret = hwDisableClock(MT_CG_DISP1_DSI_LANE, "DSI");
		ASSERT(ret);
#else
		/* *(unsigned int*)&DSI_REG->DSI_INTSTA = 0; */
		OUTREG32(&DSI_REG->DSI_INTSTA, 0);
		MASKREG32(0x14000110, 0x38, 0x38);
		printf("[DISP] - uboot - DSI_PowerOn. 0x%8x\n", INREG32(0x14000110));
#endif
		s_isDsiPowerOn = FALSE;
	}

	return DSI_STATUS_OK;
}

#else
DSI_STATUS DSI_PowerOn(void)
{
	int ret = 0;
	if (!s_isDsiPowerOn) {
#if 1				/* FIXME */
		if (!clock_is_on(MT_CG_DISP_DSI_DISP))
			ret += enable_clock(MT_CG_DISP_DSI_DISP, "DSI");
		if (!clock_is_on(MT_CG_DISP_DSI_DSI))
			ret += enable_clock(MT_CG_DISP_DSI_DSI, "DSI");
		if (!clock_is_on(MT_CG_DISP_DSI_DIV2_DSI))
			ret += enable_clock(MT_CG_DISP_DSI_DIV2_DSI, "DSI");
		ASSERT(!ret);
#endif
/* DSI_Reset(); */
		_RestoreDSIRegisters();
		s_isDsiPowerOn = TRUE;
	}
	return DSI_STATUS_OK;
}


DSI_STATUS DSI_PowerOff(void)
{
	int ret = 0;
	if (s_isDsiPowerOn) {
		/* _WaitForEngineNotBusy(); */
		_BackupDSIRegisters();
#if 1				/* FIXME */
		/* *(unsigned int*)&DSI_REG->DSI_INTSTA = 0; */
		OUTREG32(&DSI_REG->DSI_INTSTA, 0);
		ret += disable_clock(MT_CG_DISP_DSI_DISP, "DSI");
		ret += disable_clock(MT_CG_DISP_DSI_DSI, "DSI");
		ret += disable_clock(MT_CG_DISP_DSI_DIV2_DSI, "DSI");
		ASSERT(!ret);
#endif
		s_isDsiPowerOn = FALSE;
	}
	return DSI_STATUS_OK;
}
#endif

DSI_STATUS DSI_WaitForNotBusy(void)
{
	_WaitForEngineNotBusy();

	return DSI_STATUS_OK;
}


static void DSI_WaitBtaTE(void)
{
	DSI_T0_INS t0;
#if ENABLE_DSI_INTERRUPT
	long ret;
	static const long WAIT_TIMEOUT = 2 * HZ;	/* 2 sec */
#else
	long int dsi_current_time;
#endif

	if (DSI_REG->DSI_MODE_CTRL.MODE != CMD_MODE)
		return;		/* return DSI_STATUS_OK;  //fix warning */

	_WaitForEngineNotBusy();

	/* backup command queue setting. */
	DSI_BackUpCmdQ();

	t0.CONFG = 0x20;	/* /TE */
	t0.Data0 = 0;
	t0.Data_ID = 0;
	t0.Data1 = 0;

	OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(&t0));
	OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 1);

	/* DSI_REG->DSI_START.DSI_START=0; */
	/* DSI_REG->DSI_START.DSI_START=1; */
	OUTREGBIT(DSI_START_REG, DSI_REG->DSI_START, DSI_START, 0);
	OUTREGBIT(DSI_START_REG, DSI_REG->DSI_START, DSI_START, 1);

	/* wait BTA TE command complete. */
	_WaitForEngineNotBusy();

	/* restore command queue setting. */
	DSI_RestoreCmdQ();

#if ENABLE_DSI_INTERRUPT

	ret = wait_event_interruptible_timeout(_dsi_wait_bta_te, !_IsEngineBusy(), WAIT_TIMEOUT);

	if (0 == ret) {
		DISP_LOG_PRINT(ANDROID_LOG_WARN, "DSI",
			       "Wait for _dsi_wait_bta_te(DSI_INTSTA.TE_RDY) ready timeout!!!\n");

		/* Set DSI_RACK to let DSI idle */
		/* DSI_REG->DSI_RACK.DSI_RACK = 1; */
		OUTREGBIT(DSI_RACK_REG, DSI_REG->DSI_RACK, DSI_RACK, 1);

		DSI_DumpRegisters();
		/* /do necessary reset here */
		DSI_Reset();
		dsiTeEnable = false;	/* disable TE */
		return;		/* return ret; for fixed warning issue */
	}
	/* After setting DSI_RACK, it needs to wait for CMD_DONE interrupt. */
	_WaitForEngineNotBusy();

#else

	dsi_current_time = get_current_time_us();

	while (DSI_REG->DSI_INTSTA.TE_RDY == 0)	/* polling TE_RDY */
	{
		if (get_current_time_us() - dsi_current_time > 100 * 1000) {
			DISP_LOG_PRINT(ANDROID_LOG_WARN, "DSI", "Wait for TE_RDY timeout!!!\n");

			/* Set DSI_RACK to let DSI idle */
			/* DSI_REG->DSI_RACK.DSI_RACK = 1; */
			OUTREGBIT(DSI_RACK_REG, DSI_REG->DSI_RACK, DSI_RACK, 1);

			DSI_DumpRegisters();

			/* do necessary reset here */
			DSI_Reset();
			dsiTeEnable = false;	/* disable TE */
			break;
		}
	}

	/* Write clear RD_RDY */
	/* DSI_REG->DSI_INTSTA.TE_RDY = 0; */
	OUTREGBIT(DSI_INT_STATUS_REG, DSI_REG->DSI_INTSTA, TE_RDY, 0);

	/* Set DSI_RACK to let DSI idle */
	/* DSI_REG->DSI_RACK.DSI_RACK = 1; */
	OUTREGBIT(DSI_RACK_REG, DSI_REG->DSI_RACK, DSI_RACK, 1);
	if (!dsiTeEnable) {
		DSI_LP_Reset();
		return;
	}
	dsi_current_time = get_current_time_us();

	while (DSI_REG->DSI_INTSTA.CMD_DONE == 0)	/* polling CMD_DONE */
	{
		if (get_current_time_us() - dsi_current_time > 100 * 1000) {
			DISP_LOG_PRINT(ANDROID_LOG_WARN, "DSI", "Wait for CMD_DONE timeout!!!\n");

			/* Set DSI_RACK to let DSI idle */
			/* DSI_REG->DSI_RACK.DSI_RACK = 1; */
			OUTREGBIT(DSI_RACK_REG, DSI_REG->DSI_RACK, DSI_RACK, 1);

			DSI_DumpRegisters();

			/* /do necessary reset here */
			DSI_Reset();
			dsiTeEnable = false;	/* disable TE */
			break;
		}
	}

	/* Write clear CMD_DONE */
	/* DSI_REG->DSI_INTSTA.CMD_DONE = 0; */
	OUTREGBIT(DSI_INT_STATUS_REG, DSI_REG->DSI_INTSTA, CMD_DONE, 0);

#endif
	DSI_LP_Reset();
}


DSI_STATUS DSI_EnableClk(void)
{
	_WaitForEngineNotBusy();

	/* DSI_REG->DSI_START.DSI_START=0; */
	/* DSI_REG->DSI_START.DSI_START=1; */
	OUTREGBIT(DSI_START_REG, DSI_REG->DSI_START, DSI_START, 0);
	OUTREGBIT(DSI_START_REG, DSI_REG->DSI_START, DSI_START, 1);


	return DSI_STATUS_OK;
}


DSI_STATUS DSI_StartTransfer(bool isMutexLocked)
{
	/* needStartDSI = 1: For command mode or the first time of video mode. */
	/* After the first time of video mode. Configuration is applied in ConfigurationUpdateTask. */
	extern struct mutex OverlaySettingMutex;
	if (!isMutexLocked)
		disp_path_get_mutex();
	mutex_lock(&OverlaySettingMutex);
	/* isMutexLocked=false means it is the first time of DSI update after init or late resume. */
	/* Layer config was not applied in config update thread. */
	/* So need to config overlay here. */
	if (!isMutexLocked)
		LCD_ConfigOVL();
	/* Insert log for trigger point. */
	DBG_OnTriggerLcd();

	if (dsiTeEnable)
		DSI_WaitBtaTE();

	_WaitForEngineNotBusy();
	lcdStartTransfer = true;

	/* To trigger frame update. */
	DSI_EnableClk();
	mutex_unlock(&OverlaySettingMutex);
	if (!isMutexLocked)
		disp_path_release_mutex();
	return DSI_STATUS_OK;
}

DSI_STATUS DSI_DisableClk(void)
{
	/* DSI_REG->DSI_START.DSI_START=0; */
	OUTREGBIT(DSI_START_REG, DSI_REG->DSI_START, DSI_START, 0);

	return DSI_STATUS_OK;
}


DSI_STATUS DSI_Reset(void)
{
	/* DSI_REG->DSI_COM_CTRL.DSI_RESET = 1; */
	OUTREGBIT(DSI_COM_CTRL_REG, DSI_REG->DSI_COM_CTRL, DSI_RESET, 1);
	lcm_mdelay(5);
	/* DSI_REG->DSI_COM_CTRL.DSI_RESET = 0; */
	OUTREGBIT(DSI_COM_CTRL_REG, DSI_REG->DSI_COM_CTRL, DSI_RESET, 0);


	return DSI_STATUS_OK;
}

DSI_STATUS DSI_LP_Reset(void)
{
	_WaitForEngineNotBusy();
	OUTREGBIT(DSI_COM_CTRL_REG, DSI_REG->DSI_COM_CTRL, DSI_RESET, 1);
	OUTREGBIT(DSI_COM_CTRL_REG, DSI_REG->DSI_COM_CTRL, DSI_RESET, 0);
	return DSI_STATUS_OK;
}

DSI_STATUS DSI_SetMode(unsigned int mode)
{

	/* DSI_REG->DSI_MODE_CTRL.MODE = mode; */
	OUTREGBIT(DSI_MODE_CTRL_REG, DSI_REG->DSI_MODE_CTRL, MODE, mode);
	return DSI_STATUS_OK;
}

static void _DSI_RDMA0_IRQ_Handler(unsigned int param)
{
	if (_dsiContext.pIntCallback) {
		if (param & 4) {
			MMProfileLogEx(MTKFB_MMP_Events.ScreenUpdate, MMProfileFlagEnd, param, 0);
		}
		if (param & 8) {
			MMProfileLogEx(MTKFB_MMP_Events.ScreenUpdate, MMProfileFlagEnd, param, 0);
		}
		if (param & 2) {
			MMProfileLogEx(MTKFB_MMP_Events.ScreenUpdate, MMProfileFlagStart, param, 0);
			_dsiContext.pIntCallback(DISP_DSI_VSYNC_INT);
		}
		if (param & 0x20) {
			_dsiContext.pIntCallback(DISP_DSI_TARGET_LINE_INT);
		}
	}
}

static void _DSI_MUTEX_IRQ_Handler(unsigned int param)
{
	if (_dsiContext.pIntCallback) {
		if (param & 1) {
			_dsiContext.pIntCallback(DISP_DSI_REG_UPDATE_INT);
		}
	}
}

DSI_STATUS DSI_EnableInterrupt(DISP_INTERRUPT_EVENTS eventID)
{
#if ENABLE_DSI_INTERRUPT
	switch (eventID) {
	case DISP_DSI_READ_RDY_INT:
		/* DSI_REG->DSI_INTEN.RD_RDY = 1; */
		OUTREGBIT(DSI_INT_ENABLE_REG, DSI_REG->DSI_INTEN, RD_RDY, 1);
		break;
	case DISP_DSI_CMD_DONE_INT:
		/* DSI_REG->DSI_INTEN.CMD_DONE = 1; */
		OUTREGBIT(DSI_INT_ENABLE_REG, DSI_REG->DSI_INTEN, CMD_DONE, 1);
		break;
	case DISP_DSI_VMDONE_INT:
		OUTREGBIT(DSI_INT_ENABLE_REG, DSI_REG->DSI_INTEN, VM_DONE, 1);
		break;
	case DISP_DSI_VSYNC_INT:
		disp_register_irq(DISP_MODULE_RDMA0, _DSI_RDMA0_IRQ_Handler);
		break;
	case DISP_DSI_TARGET_LINE_INT:
		disp_register_irq(DISP_MODULE_RDMA0, _DSI_RDMA0_IRQ_Handler);
		break;
	case DISP_DSI_REG_UPDATE_INT:
		/* wake_up_interruptible(&_dsi_reg_update_wq); */
		disp_register_irq(DISP_MODULE_MUTEX, _DSI_MUTEX_IRQ_Handler);
		break;
	default:
		return DSI_STATUS_ERROR;
	}

	return DSI_STATUS_OK;
#else
	/* /TODO: warning log here */
	return DSI_STATUS_ERROR;
#endif
}


DSI_STATUS DSI_SetInterruptCallback(void (*pCB) (DISP_INTERRUPT_EVENTS))
{
	_dsiContext.pIntCallback = pCB;

	return DSI_STATUS_OK;
}

DSI_STATUS DSI_handle_TE(void)
{

	unsigned int data_array;

	/* data_array=0x00351504; */
	/* DSI_set_cmdq(&data_array, 1, 1); */

	/* lcm_mdelay(10); */

	/* RACT */
	/* data_array=1; */
	/* OUTREG32(&DSI_REG->DSI_RACK, data_array); */

	/* TE + BTA */
	data_array = 0x24;
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[DISP] DSI_handle_TE TE + BTA !!\n");
	OUTREG32(&DSI_CMDQ_REG->data, data_array);

	/* DSI_CMDQ_REG->data.byte0=0x24; */
	/* DSI_CMDQ_REG->data.byte1=0; */
	/* DSI_CMDQ_REG->data.byte2=0; */
	/* DSI_CMDQ_REG->data.byte3=0; */

	/* DSI_REG->DSI_CMDQ_SIZE.CMDQ_SIZE=1; */
	OUTREGBIT(DSI_CMDQ_CTRL_REG, DSI_REG->DSI_CMDQ_SIZE, CMDQ_SIZE, 1);

	/* DSI_REG->DSI_START.DSI_START=0; */
	/* DSI_REG->DSI_START.DSI_START=1; */
	OUTREGBIT(DSI_START_REG, DSI_REG->DSI_START, DSI_START, 0);
	OUTREGBIT(DSI_START_REG, DSI_REG->DSI_START, DSI_START, 1);


	/* wait TE Trigger status */
/* do */
/* { */
	lcm_mdelay(10);

	data_array = INREG32(&DSI_REG->DSI_INTSTA);
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[DISP] DSI INT state : %x !!\n", data_array);

	data_array = INREG32(&DSI_REG->DSI_TRIG_STA);
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[DISP] DSI TRIG TE status check : %x !!\n",
		       data_array);
/* } while(!(data_array&0x4)); */

	/* RACT */
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[DISP] DSI Set RACT !!\n");
	data_array = 1;
	OUTREG32(&DSI_REG->DSI_RACK, data_array);

	return DSI_STATUS_OK;

}

void DSI_PLL_Select(unsigned int pll_select)
{
	if (pll_select == 1) {
		/* ASSERT(0 == enable_pll_hp(LVDSPLL)); */
		ASSERT(0 == pll_hp_switch_on(LVDSPLL, 0));
		ASSERT(0 == enable_pll(LVDSPLL, "mtk_dsi"));
	}
}

#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))
unsigned int dsi_cycle_time;

void DSI_Config_VDO_Timing(LCM_PARAMS *lcm_params)
{
	unsigned int line_byte;
	unsigned int horizontal_sync_active_byte;
	unsigned int horizontal_backporch_byte;
	unsigned int horizontal_frontporch_byte;
	unsigned int horizontal_bllp_byte;
	unsigned int dsiTmpBufBpp;

#define LINE_PERIOD_US				(8 * line_byte * _dsiContext.bit_time_ns / 1000)

	if (lcm_params->dsi.data_format.format == LCM_DSI_FORMAT_RGB565)
		dsiTmpBufBpp = 2;
	else
		dsiTmpBufBpp = 3;

	OUTREG32(&DSI_REG->DSI_VSA_NL, lcm_params->dsi.vertical_sync_active);
	OUTREG32(&DSI_REG->DSI_VBP_NL, lcm_params->dsi.vertical_backporch);
	OUTREG32(&DSI_REG->DSI_VFP_NL, lcm_params->dsi.vertical_frontporch);
	OUTREG32(&DSI_REG->DSI_VACT_NL, lcm_params->dsi.vertical_active_line);

	line_byte = (lcm_params->dsi.horizontal_sync_active
		     + lcm_params->dsi.horizontal_backporch
		     + lcm_params->dsi.horizontal_frontporch
		     + lcm_params->dsi.horizontal_active_pixel) * dsiTmpBufBpp;

	if (lcm_params->dsi.mode == SYNC_EVENT_VDO_MODE || lcm_params->dsi.mode == BURST_VDO_MODE) {
		horizontal_sync_active_byte = (lcm_params->dsi.horizontal_sync_active * dsiTmpBufBpp - 10);	/* for fixed warning */

		ASSERT((lcm_params->dsi.horizontal_backporch +
			lcm_params->dsi.horizontal_sync_active) * dsiTmpBufBpp > 9);
		horizontal_backporch_byte =
		    ((lcm_params->dsi.horizontal_backporch +
		      lcm_params->dsi.horizontal_sync_active) * dsiTmpBufBpp - 10);
	} else {
		ASSERT(lcm_params->dsi.horizontal_sync_active * dsiTmpBufBpp > 9);
		horizontal_sync_active_byte =
		    (lcm_params->dsi.horizontal_sync_active * dsiTmpBufBpp - 10);

		ASSERT(lcm_params->dsi.horizontal_backporch * dsiTmpBufBpp > 9);
		horizontal_backporch_byte =
		    (lcm_params->dsi.horizontal_backporch * dsiTmpBufBpp - 10);
	}

	ASSERT(lcm_params->dsi.horizontal_frontporch * dsiTmpBufBpp > 11);
	horizontal_frontporch_byte = (lcm_params->dsi.horizontal_frontporch * dsiTmpBufBpp - 12);
	horizontal_bllp_byte = (lcm_params->dsi.horizontal_bllp * dsiTmpBufBpp);
/* ASSERT(lcm_params->dsi.horizontal_frontporch * dsiTmpBufBpp > ((300/dsi_cycle_time) * lcm_params->dsi.LANE_NUM)); */
/* horizontal_frontporch_byte -= ((300/dsi_cycle_time) * lcm_params->dsi.LANE_NUM); */

	OUTREG32(&DSI_REG->DSI_HSA_WC, ALIGN_TO((horizontal_sync_active_byte), 4));
	OUTREG32(&DSI_REG->DSI_HBP_WC, ALIGN_TO((horizontal_backporch_byte), 4));
	OUTREG32(&DSI_REG->DSI_HFP_WC, ALIGN_TO((horizontal_frontporch_byte), 4));
	OUTREG32(&DSI_REG->DSI_BLLP_WC, ALIGN_TO((horizontal_bllp_byte), 4));

	_dsiContext.vfp_period_us = LINE_PERIOD_US * lcm_params->dsi.vertical_frontporch / 1000;
	_dsiContext.vsa_vs_period_us = LINE_PERIOD_US * 1 / 1000;
	_dsiContext.vsa_hs_period_us =
	    LINE_PERIOD_US * (lcm_params->dsi.vertical_sync_active - 2) / 1000;
	_dsiContext.vsa_ve_period_us = LINE_PERIOD_US * 1 / 1000;
	_dsiContext.vbp_period_us = LINE_PERIOD_US * lcm_params->dsi.vertical_backporch / 1000;

	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[DISP] kernel - video timing, mode = %d\n",
		       lcm_params->dsi.mode);
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[DISP] kernel - VSA : %d %d(us)\n",
		       DSI_REG->DSI_VSA_NL,
		       (_dsiContext.vsa_vs_period_us + _dsiContext.vsa_hs_period_us +
			_dsiContext.vsa_ve_period_us));
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[DISP] kernel - VBP : %d %d(us)\n",
		       DSI_REG->DSI_VBP_NL, _dsiContext.vbp_period_us);
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[DISP] kernel - VFP : %d %d(us)\n",
		       DSI_REG->DSI_VFP_NL, _dsiContext.vfp_period_us);
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[DISP] kernel - VACT: %d\n",
		       DSI_REG->DSI_VACT_NL);
}


void DSI_PHY_clk_setting(LCM_PARAMS *lcm_params)
{				/* need re-write */
	unsigned int impendence_val = 0x08;

	DSI_PHY_REG->MIPITX_DSI_TOP_CON.RG_DSI_LNT_IMP_CAL_CODE = 8;
	DSI_PHY_REG->MIPITX_DSI_TOP_CON.RG_DSI_LNT_HS_BIAS_EN = 1;

	DSI_PHY_REG->MIPITX_DSI_BG_CON.RG_DSI_V032_SEL = 4;
	DSI_PHY_REG->MIPITX_DSI_BG_CON.RG_DSI_V04_SEL = 4;
	DSI_PHY_REG->MIPITX_DSI_BG_CON.RG_DSI_V072_SEL = 4;
	DSI_PHY_REG->MIPITX_DSI_BG_CON.RG_DSI_V10_SEL = 4;
	DSI_PHY_REG->MIPITX_DSI_BG_CON.RG_DSI_V12_SEL = 4;
	DSI_PHY_REG->MIPITX_DSI_BG_CON.RG_DSI_BG_CKEN = 1;
	DSI_PHY_REG->MIPITX_DSI_BG_CON.RG_DSI_BG_CORE_EN = 1;
	lcm_mdelay(10);

	DSI_PHY_REG->MIPITX_DSI0_CON.RG_DSI0_CKG_LDOOUT_EN = 1;
	DSI_PHY_REG->MIPITX_DSI0_CON.RG_DSI0_LDOCORE_EN = 1;

	DSI_PHY_REG->MIPITX_DSI0_MPPLL_SDM_CON1.RG_DSI0_MPPLL_PWR_ON = 1;
	DSI_PHY_REG->MIPITX_DSI0_MPPLL_SDM_CON1.RG_DSI0_MPPLL_ISO_EN = 1;
	mdelay(1);

	DSI_PHY_REG->MIPITX_DSI0_MPPLL_SDM_CON1.RG_DSI0_MPPLL_ISO_EN = 0;

	if (LCM_DSI_6589_PLL_CLOCK_NULL != lcm_params->dsi.PLL_CLOCK) {
		unsigned int i = lcm_params->dsi.PLL_CLOCK - 1;
		pr_info("LCM PLL Config = %d, %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", i,
			pll_config[i].TXDIV0, pll_config[i].TXDIV1, pll_config[i].FBK_SEL,
			pll_config[i].FBK_DIV, (pll_config[i].FBK_DIV << 24), pll_config[i].PRE_DIV,
			pll_config[i].RG_BR, pll_config[i].RG_BC, pll_config[i].RG_BIR,
			pll_config[i].RG_BIC, pll_config[i].RG_BP);
		DSI_PHY_REG->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_PREDIV = pll_config[i].PRE_DIV;	/* 0;     // preiv */
		DSI_PHY_REG->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_POSDIV = 0;	/* posdiv */
		DSI_PHY_REG->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_TXDIV1 = pll_config[i].TXDIV1;	/* 0;      // div1 */
		DSI_PHY_REG->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_TXDIV0 = pll_config[i].TXDIV0;	/* 1;      // div0 */
	} else {
		DSI_PHY_REG->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_PREDIV = 0;	/* preiv */
		DSI_PHY_REG->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_POSDIV = 0;	/* posdiv */
		DSI_PHY_REG->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_TXDIV1 = (lcm_params->dsi.pll_div2);	/* 0;        // div1 */
		DSI_PHY_REG->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_TXDIV0 = (lcm_params->dsi.pll_div1);	/* 1;        // div0 */
	}

	DSI_PHY_REG->MIPITX_DSI_PLL_CON1.RG_DSI0_MPPLL_SDM_SSC_EN = 0;
	DSI_PHY_REG->MIPITX_DSI_PLL_CON1.RG_DSI0_MPPLL_SDM_SSC_PH_INIT = 1;
	DSI_PHY_REG->MIPITX_DSI_PLL_CON1.RG_DSI0_MPPLL_SDM_FRA_EN = 0;

	/* 500MHz */
	DSI_PHY_REG->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_VOD_EN = 1;
	if (LCM_DSI_6589_PLL_CLOCK_NULL != lcm_params->dsi.PLL_CLOCK) {
		unsigned int i = lcm_params->dsi.PLL_CLOCK - 1;
		pr_info("LCM PLL Config = %x, %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n", i,
			pll_config[i].TXDIV0, pll_config[i].TXDIV1, pll_config[i].FBK_SEL,
			pll_config[i].FBK_DIV, (pll_config[i].FBK_DIV << 24), pll_config[i].PRE_DIV,
			pll_config[i].RG_BR, pll_config[i].RG_BC, pll_config[i].RG_BIR,
			pll_config[i].RG_BIC, pll_config[i].RG_BP);
		DSI_PHY_REG->MIPITX_DSI_PLL_CON2.RG_DSI0_MPPLL_SDM_PCW = (pll_config[i].FBK_DIV << 24);	/* 0x40000000; */
	} else {
		DSI_PHY_REG->MIPITX_DSI_PLL_CON2.RG_DSI0_MPPLL_SDM_PCW = (lcm_params->dsi.fbk_div << 24);	/* 0x40000000; */
	}
	if (mt_get_chip_sw_ver() != 0) {
		impendence_val = 0x08;
	} else {
		impendence_val = 0x0A;
	}
	DSI_PHY_REG->MIPITX_DSI0_CLOCK_LANE.RG_DSI0_LNTC_RT_CODE = impendence_val;
	DSI_PHY_REG->MIPITX_DSI0_CLOCK_LANE.RG_DSI0_LNTC_LDOOUT_EN = 1;

	/* ARIEL-3684: [Aston] Adjust MIPI data lane0 driving strength for eye diagram failed
	 * Due to long path on main PCB */
	if (lcm_params->dsi.cust_impendence_val) {
		pr_info("DSI Lane0 HS impedance = 0x%02x\n", lcm_params->dsi.cust_impendence_val);
		DSI_PHY_REG->MIPITX_DSI0_DATA_LANE0.RG_DSI0_LNT0_RT_CODE = lcm_params->dsi.cust_impendence_val;
				DSI_PHY_REG->MIPITX_DSI0_DATA_LANE0.RG_DSI0_LNT0_LDOOUT_EN = 1;
	} else {
		DSI_PHY_REG->MIPITX_DSI0_DATA_LANE0.RG_DSI0_LNT0_RT_CODE = impendence_val;
				DSI_PHY_REG->MIPITX_DSI0_DATA_LANE0.RG_DSI0_LNT0_LDOOUT_EN = 1;
	}

	DSI_PHY_REG->MIPITX_DSI0_DATA_LANE1.RG_DSI0_LNT1_RT_CODE = impendence_val;
	DSI_PHY_REG->MIPITX_DSI0_DATA_LANE1.RG_DSI0_LNT1_LDOOUT_EN = 1;

	DSI_PHY_REG->MIPITX_DSI0_DATA_LANE2.RG_DSI0_LNT2_RT_CODE = impendence_val;
	DSI_PHY_REG->MIPITX_DSI0_DATA_LANE2.RG_DSI0_LNT2_LDOOUT_EN = 1;

	DSI_PHY_REG->MIPITX_DSI0_DATA_LANE3.RG_DSI0_LNT3_RT_CODE = impendence_val;
	DSI_PHY_REG->MIPITX_DSI0_DATA_LANE3.RG_DSI0_LNT3_LDOOUT_EN = 1;

	DSI_PHY_REG->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_PLL_EN = 1;
	lcm_mdelay(20);

	/* default POSDIV by 4 */
	DSI_PHY_REG->MIPITX_DSI_PLL_TOP.RG_MPPLL_PRESERVE = 3;
}

#ifdef BUILD_UBOOT
void DSI_PHY_clk_switch(bool on)
{
	MIPITX_CFG0_REG mipitx_con0 = DSI_PHY_REG->MIPITX_CON0;

	if (on)
		mipitx_con0.PLL_EN = 1;
	else
		mipitx_con0.PLL_EN = 0;

	/* DSI_PHY_REG->MIPITX_CON0=mipitx_con0; */
	OUTREG32(&DSI_PHY_REG->MIPITX_CON0, AS_UINT32(&mipitx_con0));
}
#else
void DSI_PHY_clk_switch(bool on)
{
#if 1
	if (on) {		/* workaround: do nothing */
		/* switch to mipi tx dsi mode */
		OUTREGBIT(MIPITX_DSI_SW_CTRL_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL, SW_CTRL_EN, 0);

	} else {
		/* pre_oe/oe = 1 */
		OUTREGBIT(MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL_CON0,
			  SW_LNTC_LPTX_PRE_OE, 1);
		OUTREGBIT(MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL_CON0,
			  SW_LNTC_LPTX_OE, 1);
		OUTREGBIT(MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL_CON0,
			  SW_LNTC_HSTX_PRE_OE, 1);
		OUTREGBIT(MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL_CON0,
			  SW_LNTC_HSTX_OE, 1);
		OUTREGBIT(MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL_CON0,
			  SW_LNT0_LPTX_PRE_OE, 1);
		OUTREGBIT(MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL_CON0,
			  SW_LNT0_LPTX_OE, 1);
		OUTREGBIT(MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL_CON0,
			  SW_LNT1_LPTX_PRE_OE, 1);
		OUTREGBIT(MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL_CON0,
			  SW_LNT1_LPTX_OE, 1);
		OUTREGBIT(MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL_CON0,
			  SW_LNT2_LPTX_PRE_OE, 1);
		OUTREGBIT(MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL_CON0,
			  SW_LNT2_LPTX_OE, 1);
		OUTREGBIT(MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL_CON0,
			  SW_LNT2_HSTX_PRE_OE, 1);
		OUTREGBIT(MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL_CON0,
			  SW_LNT2_HSTX_OE, 1);

		/* switch to mipi tx sw mode */
		OUTREGBIT(MIPITX_DSI_SW_CTRL_REG, DSI_PHY_REG->MIPITX_DSI_SW_CTRL, SW_CTRL_EN, 1);

		/* disable mipi clock */
		OUTREGBIT(MIPITX_DSI_PLL_CON0_REG, DSI_PHY_REG->MIPITX_DSI_PLL_CON0,
			  RG_DSI0_MPPLL_PLL_EN, 0);
		mdelay(1);

		OUTREGBIT(MIPITX_DSI_TOP_CON_REG, DSI_PHY_REG->MIPITX_DSI_TOP_CON,
			  RG_DSI_LNT_HS_BIAS_EN, 0);
		OUTREGBIT(MIPITX_DSI_TOP_CON_REG, DSI_PHY_REG->MIPITX_DSI_TOP_CON,
			  RG_DSI_LNT_IMP_CAL_EN, 0);
		OUTREGBIT(MIPITX_DSI_TOP_CON_REG, DSI_PHY_REG->MIPITX_DSI_TOP_CON,
			  RG_DSI_LNT_TESTMODE_EN, 0);
		OUTREGBIT(MIPITX_DSI0_CLOCK_LANE_REG, DSI_PHY_REG->MIPITX_DSI0_CLOCK_LANE,
			  RG_DSI0_LNTC_LDOOUT_EN, 0);
		OUTREGBIT(MIPITX_DSI0_DATA_LANE0_REG, DSI_PHY_REG->MIPITX_DSI0_DATA_LANE0,
			  RG_DSI0_LNT0_LDOOUT_EN, 0);
		OUTREGBIT(MIPITX_DSI0_DATA_LANE1_REG, DSI_PHY_REG->MIPITX_DSI0_DATA_LANE1,
			  RG_DSI0_LNT1_LDOOUT_EN, 0);
		OUTREGBIT(MIPITX_DSI0_DATA_LANE2_REG, DSI_PHY_REG->MIPITX_DSI0_DATA_LANE2,
			  RG_DSI0_LNT2_LDOOUT_EN, 0);
		OUTREGBIT(MIPITX_DSI0_DATA_LANE3_REG, DSI_PHY_REG->MIPITX_DSI0_DATA_LANE3,
			  RG_DSI0_LNT3_LDOOUT_EN, 0);
		mdelay(1);

		OUTREGBIT(MIPITX_DSI0_CON_REG, DSI_PHY_REG->MIPITX_DSI0_CON, RG_DSI0_CKG_LDOOUT_EN,
			  0);
		OUTREGBIT(MIPITX_DSI0_CON_REG, DSI_PHY_REG->MIPITX_DSI0_CON, RG_DSI0_LDOCORE_EN, 0);

		OUTREGBIT(MIPITX_DSI_BG_CON_REG, DSI_PHY_REG->MIPITX_DSI_BG_CON, RG_DSI_BG_CKEN, 0);
		OUTREGBIT(MIPITX_DSI_BG_CON_REG, DSI_PHY_REG->MIPITX_DSI_BG_CON, RG_DSI_BG_CORE_EN,
			  0);
		mdelay(1);
	}
#endif
	if (on) {
		DSI_PHY_REG->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_PLL_EN = 1;
	} else {
		DSI_PHY_REG->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_PLL_EN = 0;
	}
}
#endif

void DSI_PHY_TIMCONFIG(LCM_PARAMS *lcm_params)
{
	DSI_PHY_TIMCON0_REG timcon0;
	DSI_PHY_TIMCON1_REG timcon1;
	DSI_PHY_TIMCON2_REG timcon2;
	DSI_PHY_TIMCON3_REG timcon3;
	unsigned int div1 = 0;
	unsigned int div2 = 0;
	unsigned int fbk_sel = 0;
	unsigned int pre_div = 0;
	unsigned int fbk_div = 0;
	unsigned int lane_no = lcm_params->dsi.LANE_NUM;

/* unsigned int div2_real; */
	unsigned int cycle_time;
	unsigned int ui;
	unsigned int hs_trail_m, hs_trail_n;

	if (LCM_DSI_6589_PLL_CLOCK_NULL != lcm_params->dsi.PLL_CLOCK) {
		div1 = pll_config[lcm_params->dsi.PLL_CLOCK - 1].TXDIV0;
		div2 = pll_config[lcm_params->dsi.PLL_CLOCK - 1].TXDIV1;
		fbk_sel = pll_config[lcm_params->dsi.PLL_CLOCK - 1].FBK_SEL;
		fbk_div = pll_config[lcm_params->dsi.PLL_CLOCK - 1].FBK_DIV;
		pre_div = pll_config[lcm_params->dsi.PLL_CLOCK - 1].PRE_DIV;
	} else {
		div1 = lcm_params->dsi.pll_div1;
		div2 = lcm_params->dsi.pll_div2;
		fbk_sel = lcm_params->dsi.fbk_sel;
		fbk_div = lcm_params->dsi.fbk_div;
	}

	switch (div1) {
	case 0:
		div1 = 1;
		break;
	case 1:
		div1 = 2;
		break;
	case 2:
	case 3:
		div1 = 4;
		break;
	default:
		pr_info("div1 should be less than 4!!\n");
		div1 = 4;
		break;
	}
	switch (div2) {
	case 0:
		div2 = 1;
		break;
	case 1:
		div2 = 2;
		break;
	case 2:
	case 3:
		div2 = 4;
		break;
	default:
		pr_info("div2 should be less than 4!!\n");
		div2 = 4;
		break;
	}
	switch (fbk_sel) {
	case 0:
		fbk_sel = 1;
		break;
	case 1:
		fbk_sel = 2;
		break;
	case 2:
	case 3:
		fbk_sel = 4;
		break;
	default:
		pr_info("fbk_sel should be less than 4!!\n");
		fbk_sel = 4;
		break;
	}

	switch (pre_div) {
	case 0:
		pre_div = 1;
		break;
	case 1:
		pre_div = 2;
		break;
	case 2:
	case 3:
		pre_div = 4;
		break;
	default:
		pr_info("pre_div should be less than 4!!\n");
		pre_div = 4;
		break;
	}

/* div2_real=div2 ? div2*0x02 : 0x1; */
	cycle_time = (4 * 8 * 1000 * div2 * div1 * pre_div * fbk_sel) / (26 * fbk_div * 2);
	ui = (4 * 1000 * div2 * div1 * pre_div * fbk_sel) / (26 * fbk_div * 2) + 1;

	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI",
		       "[DISP] - kernel - DSI_PHY_TIMCONFIG, Cycle Time = %d(ns), Unit Interval = %d(ns). div1 = %d, div2 = %d, fbk_div = %d, lane# = %d\n",
		       cycle_time, ui, div1, div2, fbk_div, lane_no);

#define NS_TO_CYCLE(n, c)	((n) / c + (((n) % c) ? 1 : 0))

	hs_trail_m = lane_no;
	hs_trail_n =
	    (lcm_params->dsi.HS_TRAIL == 0) ? NS_TO_CYCLE(((lane_no * 4 * ui) + 60),
							  cycle_time) : lcm_params->dsi.HS_TRAIL;

	/* +3 is recommended from designer becauase of HW latency */
	timcon0.HS_TRAIL = ((hs_trail_m > hs_trail_n) ? hs_trail_m : hs_trail_n) + 0x0a;

	timcon0.HS_PRPR =
	    (lcm_params->dsi.HS_PRPR == 0) ? NS_TO_CYCLE((60 + 5 * ui),
							 cycle_time) : lcm_params->dsi.HS_PRPR;
	/* HS_PRPR can't be 1. */
	if (timcon0.HS_PRPR == 0)
		timcon0.HS_PRPR = 1;

	timcon0.HS_ZERO =
	    (lcm_params->dsi.HS_ZERO ==
	     0) ? NS_TO_CYCLE((0xC8 + 0x0a * ui - timcon0.HS_PRPR * cycle_time),
			      cycle_time) : lcm_params->dsi.HS_ZERO;

	timcon0.LPX =
	    (lcm_params->dsi.LPX == 0) ? NS_TO_CYCLE(65, cycle_time) : lcm_params->dsi.LPX;
	if (timcon0.LPX == 0)
		timcon0.LPX = 1;
/* timcon1.TA_SACK         = (lcm_params->dsi.TA_SACK == 0) ? 1 : lcm_params->dsi.TA_SACK; */
	timcon1.TA_GET = (lcm_params->dsi.TA_GET == 0) ? (5 * timcon0.LPX) : lcm_params->dsi.TA_GET;
	timcon1.TA_SURE =
	    (lcm_params->dsi.TA_SURE == 0) ? (3 * timcon0.LPX / 2) : lcm_params->dsi.TA_SURE;
	timcon1.TA_GO = (lcm_params->dsi.TA_GO == 0) ? (4 * timcon0.LPX) : lcm_params->dsi.TA_GO;
	timcon1.DA_HS_EXIT =
	    (lcm_params->dsi.DA_HS_EXIT == 0) ? (2 * timcon0.LPX) : lcm_params->dsi.DA_HS_EXIT;

	timcon2.CLK_TRAIL =
	    ((lcm_params->dsi.CLK_TRAIL == 0) ? NS_TO_CYCLE(0x64,
							    cycle_time) : lcm_params->dsi.
	     CLK_TRAIL) + 0x0a;
	/* CLK_TRAIL can't be 1. */
	if (timcon2.CLK_TRAIL < 2)
		timcon2.CLK_TRAIL = 2;

/* timcon2.LPX_WAIT        = (lcm_params->dsi.LPX_WAIT == 0) ? 1 : lcm_params->dsi.LPX_WAIT; */
	timcon2.CONT_DET = lcm_params->dsi.CONT_DET;

	timcon3.CLK_HS_PRPR =
	    (lcm_params->dsi.CLK_HS_PRPR == 0) ? NS_TO_CYCLE(0x40,
							     cycle_time) : lcm_params->dsi.
	    CLK_HS_PRPR;
	if (timcon3.CLK_HS_PRPR == 0)
		timcon3.CLK_HS_PRPR = 1;

	timcon2.CLK_ZERO =
	    (lcm_params->dsi.CLK_ZERO == 0) ? NS_TO_CYCLE(0x190 - timcon3.CLK_HS_PRPR * cycle_time,
							  cycle_time) : lcm_params->dsi.CLK_ZERO;

	timcon3.CLK_HS_EXIT =
	    (lcm_params->dsi.CLK_HS_EXIT == 0) ? (2 * timcon0.LPX) : lcm_params->dsi.CLK_HS_EXIT;

	timcon3.CLK_HS_POST =
	    (lcm_params->dsi.CLK_HS_POST == 0) ? NS_TO_CYCLE((80 + 52 * ui),
							     cycle_time) : lcm_params->dsi.
	    CLK_HS_POST;

	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI",
		       "[DISP] - kernel - DSI_PHY_TIMCONFIG, HS_TRAIL = %d, HS_ZERO = %d, HS_PRPR = %d, LPX = %d, TA_GET = %d, TA_SURE = %d, TA_GO = %d, CLK_TRAIL = %d, CLK_ZERO = %d, CLK_HS_PRPR = %d\n",
		       timcon0.HS_TRAIL, timcon0.HS_ZERO, timcon0.HS_PRPR, timcon0.LPX,
		       timcon1.TA_GET, timcon1.TA_SURE, timcon1.TA_GO, timcon2.CLK_TRAIL,
		       timcon2.CLK_ZERO, timcon3.CLK_HS_PRPR);

	/* DSI_REG->DSI_PHY_TIMECON0=timcon0; */
	/* DSI_REG->DSI_PHY_TIMECON1=timcon1; */
	/* DSI_REG->DSI_PHY_TIMECON2=timcon2; */
	/* DSI_REG->DSI_PHY_TIMECON3=timcon3; */
	OUTREG32(&DSI_REG->DSI_PHY_TIMECON0, AS_UINT32(&timcon0));
	OUTREG32(&DSI_REG->DSI_PHY_TIMECON1, AS_UINT32(&timcon1));
	OUTREG32(&DSI_REG->DSI_PHY_TIMECON2, AS_UINT32(&timcon2));
	OUTREG32(&DSI_REG->DSI_PHY_TIMECON3, AS_UINT32(&timcon3));
	dsi_cycle_time = cycle_time;
}



void DSI_clk_ULP_mode(bool enter)
{
	DSI_PHY_LCCON_REG tmp_reg1;
	/* DSI_PHY_REG_ANACON0   tmp_reg2; */

	tmp_reg1 = DSI_REG->DSI_PHY_LCCON;
	/* tmp_reg2=DSI_PHY_REG->ANACON0; */

	if (enter) {

		tmp_reg1.LC_HS_TX_EN = 0;
		OUTREG32(&DSI_REG->DSI_PHY_LCCON, AS_UINT32(&tmp_reg1));
		mdelay(1);
		tmp_reg1.LC_ULPM_EN = 1;
		OUTREG32(&DSI_REG->DSI_PHY_LCCON, AS_UINT32(&tmp_reg1));
		mdelay(1);
		/* tmp_reg2.PLL_EN=0; */
		/* OUTREG32(&DSI_PHY_REG->ANACON0, AS_UINT32(&tmp_reg2)); */

	} else {

		/* tmp_reg2.PLL_EN=1; */
		/* OUTREG32(&DSI_PHY_REG->ANACON0, AS_UINT32(&tmp_reg2)); */
		mdelay(1);
		tmp_reg1.LC_ULPM_EN = 0;
		OUTREG32(&DSI_REG->DSI_PHY_LCCON, AS_UINT32(&tmp_reg1));
		mdelay(1);
		tmp_reg1.LC_WAKEUP_EN = 1;
		OUTREG32(&DSI_REG->DSI_PHY_LCCON, AS_UINT32(&tmp_reg1));
		mdelay(1);
		tmp_reg1.LC_WAKEUP_EN = 0;
		OUTREG32(&DSI_REG->DSI_PHY_LCCON, AS_UINT32(&tmp_reg1));
		mdelay(1);

	}
}


void DSI_clk_HS_mode(bool enter)
{
	DSI_PHY_LCCON_REG tmp_reg1 = DSI_REG->DSI_PHY_LCCON;

	if (enter && !DSI_clk_HS_state()) {
		tmp_reg1.LC_HS_TX_EN = 1;
		OUTREG32(&DSI_REG->DSI_PHY_LCCON, AS_UINT32(&tmp_reg1));
		/* lcm_mdelay(1); */
	} else if (!enter && DSI_clk_HS_state()) {
		tmp_reg1.LC_HS_TX_EN = 0;
		OUTREG32(&DSI_REG->DSI_PHY_LCCON, AS_UINT32(&tmp_reg1));
		/* lcm_mdelay(1); */

	}
}


DSI_STATUS DSI_EnableVM_CMD(void)
{

	OUTREGBIT(DSI_START_REG, DSI_REG->DSI_START, VM_CMD_START, 0);
	OUTREGBIT(DSI_START_REG, DSI_REG->DSI_START, VM_CMD_START, 1);
	return DSI_STATUS_OK;
}

void DSI_Set_VM_CMD(LCM_PARAMS *lcm_params)
{
	OUTREGBIT(DSI_VM_CMD_CON_REG, DSI_REG->DSI_VM_CMD_CON, TS_VFP_EN, 1);
	OUTREGBIT(DSI_VM_CMD_CON_REG, DSI_REG->DSI_VM_CMD_CON, VM_CMD_EN, 1);
	return;
}

bool DSI_clk_HS_state(void)
{
	return DSI_REG->DSI_PHY_LCCON.LC_HS_TX_EN ? TRUE : FALSE;
}


void DSI_lane0_ULP_mode(bool enter)
{
	DSI_PHY_LD0CON_REG tmp_reg1;

	tmp_reg1 = DSI_REG->DSI_PHY_LD0CON;

	if (enter) {
		/* suspend */
		tmp_reg1.L0_HS_TX_EN = 0;
		OUTREG32(&DSI_REG->DSI_PHY_LD0CON, AS_UINT32(&tmp_reg1));
		mdelay(1);
		tmp_reg1.L0_ULPM_EN = 1;
		OUTREG32(&DSI_REG->DSI_PHY_LD0CON, AS_UINT32(&tmp_reg1));
		mdelay(1);
	} else {
		/* resume */
		tmp_reg1.L0_ULPM_EN = 0;
		OUTREG32(&DSI_REG->DSI_PHY_LD0CON, AS_UINT32(&tmp_reg1));
		mdelay(1);
		tmp_reg1.L0_WAKEUP_EN = 1;
		OUTREG32(&DSI_REG->DSI_PHY_LD0CON, AS_UINT32(&tmp_reg1));
		mdelay(1);
		tmp_reg1.L0_WAKEUP_EN = 0;
		OUTREG32(&DSI_REG->DSI_PHY_LD0CON, AS_UINT32(&tmp_reg1));
		mdelay(1);
	}
}

#ifndef BUILD_UBOOT

/* called by DPI ISR */
void DSI_handle_esd_recovery(void)
{
}


/* called by "esd_recovery_kthread" */
bool DSI_esd_check(void)
{
#ifndef MT65XX_NEW_DISP
	bool result = false;

	if (dsi_esd_recovery)
		result = true;
	else
		result = false;

	dsi_esd_recovery = false;
#else
	DSI_MODE_CTRL_REG mode_ctl, mode_ctl_backup;
#if ENABLE_DSI_INTERRUPT	/* for fixed warning */
	long ret;
	static const long WAIT_TIMEOUT = 2 * HZ;
#endif
	bool result = false;
	/* backup video mode */
	OUTREG32(&mode_ctl_backup, AS_UINT32(&DSI_REG->DSI_MODE_CTRL));
	OUTREG32(&mode_ctl, AS_UINT32(&DSI_REG->DSI_MODE_CTRL));
	/* set to cmd mode */
	mode_ctl.MODE = 0;
	OUTREG32(&DSI_REG->DSI_MODE_CTRL, AS_UINT32(&mode_ctl));
#if ENABLE_DSI_INTERRUPT	/* wait video mode done */
	/* static const long WAIT_TIMEOUT = 2 * HZ;      // 2 sec   //for fixed warning */
	/* long ret; */
	/* static const long WAIT_TIMEOUT = 2 * HZ;      // 2 sec  //for fixed warning */

	ret = wait_event_interruptible_timeout(_dsi_wait_vm_done_queue,
					       !_IsEngineBusy(), WAIT_TIMEOUT);
	if (0 == ret) {
		xlog_printk(ANDROID_LOG_WARN, "DSI",
			    " Wait for DSI engine read ready timeout!!!\n");

		DSI_DumpRegisters();
		/* /do necessary reset here */
		DSI_Reset();
		return 0;
	}
#else
	unsigned int read_timeout_ms = 100;
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " Start polling VM done ready!!!\n");
#endif
	while (DSI_REG->DSI_INTSTA.VM_DONE == 0)	/* clear */
	{
		/* /keep polling */
		msleep(1);
		read_timeout_ms--;

		if (read_timeout_ms == 0) {
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI",
				       " Polling DSI VM done timeout!!!\n");
			DSI_DumpRegisters();

			DSI_Reset();
			return 0;
		}
	}
	/* DSI_REG->DSI_INTSTA.VM_DONE = 0; */
	OUTREGBIT(DSI_INT_STATUS_REG, DSI_REG->DSI_INTSTA, VM_DONE, 0);
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " End polling DSI VM done ready!!!\n");
#endif
#endif
	/* read DriverIC and check ESD */
	result = lcm_drv->esd_check();
	/* restore video mode */
	if (!result)
		OUTREG32(&DSI_REG->DSI_MODE_CTRL, AS_UINT32(&mode_ctl_backup));
#endif
	return result;
}


void DSI_set_int_TE(bool enable, unsigned int period)
{
#ifndef MT65XX_NEW_DISP
	dsi_int_te_enabled = enable;
	if (period < 1)
		period = 1;
	dsi_int_te_period = period;
	dsi_dpi_isr_count = 0;
#endif
}


/* called by DPI ISR. */
bool DSI_handle_int_TE(void)
{
#ifndef MT65XX_NEW_DISP
	DSI_T0_INS t0;
	long int dsi_current_time;

	if (!DSI_REG->DSI_MODE_CTRL.MODE)
		return false;

	dsi_current_time = get_current_time_us();

	if (DSI_REG->DSI_STATE_DBG3.TCON_STATE == DSI_VDO_VFP_STATE) {
		udelay(_dsiContext.vfp_period_us / 2);

		if ((DSI_REG->DSI_STATE_DBG3.TCON_STATE == DSI_VDO_VFP_STATE)
		    && DSI_REG->DSI_STATE_DBG0.CTL_STATE_0 == 0x1) {
			/* Can't do int. TE check while INUSE FB number is not 0 because later disable/enable DPI will set INUSE FB to number 0. */
			if (DPI_REG->STATUS.FB_INUSE != 0)
				return false;

			DSI_clk_HS_mode(0);

			/* DSI_REG->DSI_COM_CTRL.DSI_RESET = 1; */
			OUTREGBIT(DSI_COM_CTRL_REG, DSI_REG->DSI_COM_CTRL, DSI_RESET, 1);
			DPI_DisableClk();
			DSI_SetMode(CMD_MODE);
			/* DSI_REG->DSI_COM_CTRL.DSI_RESET = 0; */
			OUTREGBIT(DSI_COM_CTRL_REG, DSI_REG->DSI_COM_CTRL, DSI_RESET, 0);
			/* DSI_Reset(); */

			t0.CONFG = 0x20;	/* /TE */
			t0.Data0 = 0;
			t0.Data_ID = 0;
			t0.Data1 = 0;

			OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(&t0));
			OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 1);

			/* Enable RD_RDY INT for polling it's status later */
			/* DSI_REG->DSI_INTEN.RD_RDY =  1; */
			OUTREGBIT(DSI_INT_ENABLE_REG, DSI_REG->DSI_INTEN, RD_RDY, 1);

			DSI_EnableClk();

			while (DSI_REG->DSI_INTSTA.RD_RDY == 0)	/* polling RD_RDY */
			{
				if (get_current_time_us() - dsi_current_time >
				    _dsiContext.vfp_period_us) {
					xlog_printk(ANDROID_LOG_WARN, "DSI",
						    " Wait for internal TE time-out for %d (us)!!!\n",
						    _dsiContext.vfp_period_us);

					/* /do necessary reset here */
					/* DSI_REG->DSI_RACK.DSI_RACK = 1; */
					OUTREGBIT(DSI_RACK_REG, DSI_REG->DSI_RACK, DSI_RACK, 1);
					DSI_Reset();

					return true;
				}
			}

			/* Write clear RD_RDY */
			/* DSI_REG->DSI_INTSTA.RD_RDY = 1; */
			/* DSI_REG->DSI_RACK.DSI_RACK = 1; */
			OUTREGBIT(DSI_INT_STATUS_REG, DSI_REG->DSI_INTSTA, RD_RDY, 1);
			OUTREGBIT(DSI_RACK_REG, DSI_REG->DSI_RACK, DSI_RACK, 1);
			/* Write clear CMD_DONE */
			/* DSI_REG->DSI_INTSTA.CMD_DONE = 1; */
			OUTREGBIT(DSI_INT_STATUS_REG, DSI_REG->DSI_INTSTA, CMD_DONE, 1);

			/* Restart video mode. (with VSA ahead) */
			DSI_SetMode(SYNC_PULSE_VDO_MODE);
			DSI_clk_HS_mode(1);
			DPI_EnableClk();
			DSI_EnableClk();
		}

	}
#endif
	return false;

}


void DSI_set_noncont_clk(bool enable, unsigned int period)
{
#ifndef MT65XX_NEW_DISP
	dsi_noncont_clk_enabled = enable;
	if (period < 1)
		period = 1;
	dsi_noncont_clk_period = period;
	dsi_dpi_isr_count = 0;
#endif
}


/* called by DPI ISR. */
void DSI_handle_noncont_clk(void)
{
#ifndef MT65XX_NEW_DISP
	unsigned int state;
	long int dsi_current_time;

	if (!DSI_REG->DSI_MODE_CTRL.MODE)
		return;

	state = DSI_REG->DSI_STATE_DBG3.TCON_STATE;

	dsi_current_time = get_current_time_us();

	switch (state) {
	case DSI_VDO_VSA_VS_STATE:
		while (DSI_REG->DSI_STATE_DBG3.TCON_STATE != DSI_VDO_VSA_HS_STATE) {
			if (get_current_time_us() - dsi_current_time > _dsiContext.vsa_vs_period_us) {
				xlog_printk(ANDROID_LOG_WARN, "DSI",
					    " Wait for %x state timeout %d (us)!!!\n",
					    DSI_VDO_VSA_HS_STATE, _dsiContext.vsa_vs_period_us);
				return;
			}
		}
		break;

	case DSI_VDO_VSA_HS_STATE:
		while (DSI_REG->DSI_STATE_DBG3.TCON_STATE != DSI_VDO_VSA_VE_STATE) {
			if (get_current_time_us() - dsi_current_time > _dsiContext.vsa_hs_period_us) {
				xlog_printk(ANDROID_LOG_WARN, "DSI",
					    " Wait for %x state timeout %d (us)!!!\n",
					    DSI_VDO_VSA_VE_STATE, _dsiContext.vsa_hs_period_us);
				return;
			}
		}
		break;

	case DSI_VDO_VSA_VE_STATE:
		while (DSI_REG->DSI_STATE_DBG3.TCON_STATE != DSI_VDO_VBP_STATE) {
			if (get_current_time_us() - dsi_current_time > _dsiContext.vsa_ve_period_us) {
				xlog_printk(ANDROID_LOG_WARN, "DSI",
					    " Wait for %x state timeout %d (us)!!!\n",
					    DSI_VDO_VBP_STATE, _dsiContext.vsa_ve_period_us);
				return;
			}
		}
		break;

	case DSI_VDO_VBP_STATE:
		xlog_printk(ANDROID_LOG_WARN, "DSI",
			    "Can't do clock switch in DSI_VDO_VBP_STATE !!!\n");
		break;

	case DSI_VDO_VACT_STATE:
		while (DSI_REG->DSI_STATE_DBG3.TCON_STATE != DSI_VDO_VFP_STATE) {
			if (get_current_time_us() - dsi_current_time > _dsiContext.vfp_period_us) {
				xlog_printk(ANDROID_LOG_WARN, "DSI",
					    " Wait for %x state timeout %d (us)!!!\n",
					    DSI_VDO_VFP_STATE, _dsiContext.vfp_period_us);
				return;
			}
		}
		break;

	case DSI_VDO_VFP_STATE:
		while (DSI_REG->DSI_STATE_DBG3.TCON_STATE != DSI_VDO_VSA_VS_STATE) {
			if (get_current_time_us() - dsi_current_time > _dsiContext.vfp_period_us) {
				xlog_printk(ANDROID_LOG_WARN, "DSI",
					    " Wait for %x state timeout %d (us)!!!\n",
					    DSI_VDO_VSA_VS_STATE, _dsiContext.vfp_period_us);
				return;
			}
		}
		break;

	default:
		xlog_printk(ANDROID_LOG_ERROR, "DSI", "invalid state = %x\n", state);
		return;
	}

	/* Clock switch HS->LP->HS */
	DSI_clk_HS_mode(0);
	udelay(1);
	DSI_clk_HS_mode(1);
#endif
}
#endif

#ifdef ENABLE_DSI_ERROR_REPORT
static unsigned int _dsi_cmd_queue[32];
#endif
void DSI_set_cmdq_V2(unsigned cmd, unsigned char count, unsigned char *para_list,
		     unsigned char force_update)
{
	UINT32 i;		/* , layer, layer_state, lane_num;  //for fixed warning */
	UINT32 goto_addr, mask_para, set_para;
	/* UINT32 fbPhysAddr, fbVirAddr; //for fixed warning */
	DSI_T0_INS t0;
	/* DSI_T1_INS t1;          //for fixed warning */
	DSI_T2_INS t2;
	if (0 != DSI_REG->DSI_MODE_CTRL.MODE) {
		DSI_VM_CMD_CON_REG vm_cmdq;
		OUTREG32(&vm_cmdq, AS_UINT32(&DSI_REG->DSI_VM_CMD_CON));
		/*printk("set cmdq in VDO mode in set_cmdq_V2\n");*/
		if (cmd < 0xB0) {
			if (count > 1) {
				vm_cmdq.LONG_PKT = 1;
				vm_cmdq.CM_DATA_ID = DSI_DCS_LONG_PACKET_ID;
				vm_cmdq.CM_DATA_0 = count+1;
				OUTREG32(&DSI_REG->DSI_VM_CMD_CON, AS_UINT32(&vm_cmdq));
				goto_addr = (UINT32)(&DSI_VM_CMD_REG->data[0].byte0);
				mask_para = (0xFF<<((goto_addr&0x3)*8));
				set_para = (cmd<<((goto_addr&0x3)*8));
				MASKREG32(goto_addr&(~0x3), mask_para, set_para);
				for (i = 0; i < count; i++) {
					goto_addr = (UINT32)(&DSI_VM_CMD_REG->data[0].byte1) + i;
					mask_para = (0xFF<<((goto_addr&0x3)*8));
					set_para = (para_list[i]<<((goto_addr&0x3)*8));
					MASKREG32(goto_addr&(~0x3), mask_para, set_para);
									}
			} else {
				vm_cmdq.LONG_PKT = 0;
				vm_cmdq.CM_DATA_0 = cmd;
				if (count) {
					vm_cmdq.CM_DATA_ID = DSI_DCS_SHORT_PACKET_ID_1;
					vm_cmdq.CM_DATA_1 = para_list[0];
				} else {
					vm_cmdq.CM_DATA_ID = DSI_DCS_SHORT_PACKET_ID_0;
					vm_cmdq.CM_DATA_1 = 0;
				}
				OUTREG32(&DSI_REG->DSI_VM_CMD_CON, AS_UINT32(&vm_cmdq));
			}
		} else {
			if (count > 1) {
				vm_cmdq.LONG_PKT = 1;
				vm_cmdq.CM_DATA_ID = DSI_GERNERIC_LONG_PACKET_ID;
				vm_cmdq.CM_DATA_0 = count+1;
				OUTREG32(&DSI_REG->DSI_VM_CMD_CON, AS_UINT32(&vm_cmdq));
				goto_addr = (UINT32)(&DSI_VM_CMD_REG->data[0].byte0);
				mask_para = (0xFF<<((goto_addr&0x3)*8));
				set_para = (cmd<<((goto_addr&0x3)*8));
				MASKREG32(goto_addr&(~0x3), mask_para, set_para);
				for (i = 0; i < count; i++) {
					goto_addr = (UINT32)(&DSI_VM_CMD_REG->data[0].byte1) + i;
					mask_para = (0xFF<<((goto_addr&0x3)*8));
					set_para = (para_list[i]<<((goto_addr&0x3)*8));
					MASKREG32(goto_addr&(~0x3), mask_para, set_para);
				}
			} else {
				vm_cmdq.LONG_PKT = 0;
				vm_cmdq.CM_DATA_0 = cmd;
				if (count) {
					vm_cmdq.CM_DATA_ID = DSI_GERNERIC_SHORT_PACKET_ID_2;
					vm_cmdq.CM_DATA_1 = para_list[0];
				} else {
					vm_cmdq.CM_DATA_ID = DSI_GERNERIC_SHORT_PACKET_ID_1;
					vm_cmdq.CM_DATA_1 = 0;
				}
				OUTREG32(&DSI_REG->DSI_VM_CMD_CON, AS_UINT32(&vm_cmdq));
			}
		}
		/*start DSI VM CMDQ*/
	if (force_update) {
		MMProfileLogEx(MTKFB_MMP_Events.DSICmd, MMProfileFlagStart,
				*(unsigned int *)(&DSI_VM_CMD_REG->data[0]),
				*(unsigned int *)(&DSI_VM_CMD_REG->data[1]));
			DSI_EnableVM_CMD();
			/*must wait VM CMD done?*/
		MMProfileLogEx(MTKFB_MMP_Events.DSICmd, MMProfileFlagEnd,
				*(unsigned int *)(&DSI_VM_CMD_REG->data[2]),
				*(unsigned int *)(&DSI_VM_CMD_REG->data[3]));
		}
	} else {
#ifdef ENABLE_DSI_ERROR_REPORT
	if ((para_list[0] & 1)) {
		memset(_dsi_cmd_queue, 0, sizeof(_dsi_cmd_queue));
		memcpy(_dsi_cmd_queue, para_list, count);
		_dsi_cmd_queue[(count + 3) / 4 * 4] = 0x4;
		count = (count + 3) / 4 * 4 + 4;
		para_list = (unsigned char *)_dsi_cmd_queue;
	 } else {
		para_list[0] |= 4;
	 }
#endif
	 _WaitForEngineNotBusy();
	if (cmd < 0xB0) {
		if (count > 1) {
				t2.CONFG = 2;
				t2.Data_ID = DSI_DCS_LONG_PACKET_ID;
				t2.WC16 = count + 1;
				OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(&t2));
				goto_addr = (UINT32) (&DSI_CMDQ_REG->data[1].byte0);
				mask_para = (0xFF << ((goto_addr & 0x3) * 8));
				set_para = (cmd << ((goto_addr & 0x3) * 8));
				MASKREG32(goto_addr & (~0x3), mask_para, set_para);
				for (i = 0; i < count; i++) {
					goto_addr = (UINT32) (&DSI_CMDQ_REG->data[1].byte1) + i;
					mask_para = (0xFF << ((goto_addr & 0x3) * 8));
					set_para = (para_list[i] << ((goto_addr & 0x3) * 8));
					MASKREG32(goto_addr & (~0x3), mask_para, set_para);
				}

				OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 2 + (count) / 4);
			} else {
				t0.CONFG = 0;
				t0.Data0 = cmd;
				if (count) {
					t0.Data_ID = DSI_DCS_SHORT_PACKET_ID_1;
					t0.Data1 = para_list[0];
				} else {
					t0.Data_ID = DSI_DCS_SHORT_PACKET_ID_0;
					t0.Data1 = 0;
				}
				OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(&t0));
				OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 1);
			}
		} else {
			if (count > 1) {
				t2.CONFG = 2;
				t2.Data_ID = DSI_GERNERIC_LONG_PACKET_ID;
				t2.WC16 = count + 1;

				OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(&t2));

				goto_addr = (UINT32) (&DSI_CMDQ_REG->data[1].byte0);
				mask_para = (0xFF << ((goto_addr & 0x3) * 8));
				set_para = (cmd << ((goto_addr & 0x3) * 8));
				MASKREG32(goto_addr & (~0x3), mask_para, set_para);

				for (i = 0; i < count; i++) {
					goto_addr = (UINT32) (&DSI_CMDQ_REG->data[1].byte1) + i;
					mask_para = (0xFF << ((goto_addr & 0x3) * 8));
					set_para = (para_list[i] << ((goto_addr & 0x3) * 8));
					MASKREG32(goto_addr & (~0x3), mask_para, set_para);
				}

				OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 2 + (count) / 4);

			} else {
				t0.CONFG = 0;
				t0.Data0 = cmd;
				if (count) {
					t0.Data_ID = DSI_GERNERIC_SHORT_PACKET_ID_2;
					t0.Data1 = para_list[0];
				} else {
					t0.Data_ID = DSI_GERNERIC_SHORT_PACKET_ID_1;
					t0.Data1 = 0;
				}
				OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(&t0));
				OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 1);
			}
		}

/* for (i = 0; i < AS_UINT32(&DSI_REG->DSI_CMDQ_SIZE); i++) */
/* DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "DSI_set_cmdq_V2. DSI_CMDQ+%04x : 0x%08x\n", i*4, INREG32(DSI_BASE + 0x180 + i*4)); */

		if (force_update) {
			MMProfileLogEx(MTKFB_MMP_Events.DSICmd, MMProfileFlagStart,
				       *(unsigned int *)(&DSI_CMDQ_REG->data[0]),
				       *(unsigned int *)(&DSI_CMDQ_REG->data[1]));
			DSI_EnableClk();
			for (i = 0; i < 10; i++);
			_WaitForEngineNotBusy();
			MMProfileLogEx(MTKFB_MMP_Events.DSICmd, MMProfileFlagEnd,
				       *(unsigned int *)(&DSI_CMDQ_REG->data[2]),
				       *(unsigned int *)(&DSI_CMDQ_REG->data[3]));
		}
	}

}

void DSI_set_cmdq_V3(LCM_setting_table_V3 *para_tbl, unsigned int size, unsigned char force_update)
{
	UINT32 i;		/* , layer, layer_state, lane_num; //for fixed warning issue */
	UINT32 goto_addr, mask_para, set_para;
	/* UINT32 fbPhysAddr, fbVirAddr;   //for fixed warning issue. */
	DSI_T0_INS t0;
	/* DSI_T1_INS t1;        //for fixed warning issue */
	DSI_T2_INS t2;

	UINT32 index = 0;

	unsigned char data_id, cmd, count;
	unsigned char *para_list;

	do {
		data_id = para_tbl[index].id;
		cmd = para_tbl[index].cmd;
		count = para_tbl[index].count;
		para_list = para_tbl[index].para_list;

		if (data_id == REGFLAG_ESCAPE_ID && cmd == REGFLAG_DELAY_MS_V3) {
			udelay(1000 * count);
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI",
				       "DSI_set_cmdq_V3[%d]. Delay %d (ms)\n", index, count);
			continue;
		}
		if (0 != DSI_REG->DSI_MODE_CTRL.MODE) {
			DSI_VM_CMD_CON_REG vm_cmdq;
			OUTREG32(&vm_cmdq, AS_UINT32(&DSI_REG->DSI_VM_CMD_CON));
			/*printk("set cmdq in VDO mode\n");*/
			if (count > 1) {
				vm_cmdq.LONG_PKT = 1;
				vm_cmdq.CM_DATA_ID = data_id;
				vm_cmdq.CM_DATA_0 = count+1;
				OUTREG32(&DSI_REG->DSI_VM_CMD_CON, AS_UINT32(&vm_cmdq));
				goto_addr = (UINT32)(&DSI_VM_CMD_REG->data[0].byte0);
				mask_para = (0xFF<<((goto_addr&0x3)*8));
				set_para = (cmd<<((goto_addr&0x3)*8));
				MASKREG32(goto_addr&(~0x3), mask_para, set_para);
				for (i = 0; i < count; i++) {
					goto_addr = (UINT32)(&DSI_VM_CMD_REG->data[0].byte1) + i;
					mask_para = (0xFF<<((goto_addr&0x3)*8));
					set_para = (para_list[i]<<((goto_addr&0x3)*8));
					MASKREG32(goto_addr&(~0x3), mask_para, set_para);
				}
			} else {
				vm_cmdq.LONG_PKT = 0;
				vm_cmdq.CM_DATA_0 = cmd;
				if (count) {
					vm_cmdq.CM_DATA_ID = data_id;
					vm_cmdq.CM_DATA_1 = para_list[0];
				} else {
					vm_cmdq.CM_DATA_ID = data_id;
					vm_cmdq.CM_DATA_1 = 0;
				}
				OUTREG32(&DSI_REG->DSI_VM_CMD_CON, AS_UINT32(&vm_cmdq));
			}
		/*start DSI VM CMDQ*/
		if (force_update) {
			MMProfileLogEx(MTKFB_MMP_Events.DSICmd, MMProfileFlagStart,
				*(unsigned int *)(&DSI_VM_CMD_REG->data[0]),
				*(unsigned int *)(&DSI_VM_CMD_REG->data[1]));
				DSI_EnableVM_CMD();

			/*must wait VM CMD done?*/
		MMProfileLogEx(MTKFB_MMP_Events.DSICmd, MMProfileFlagEnd,
				*(unsigned int *)(&DSI_VM_CMD_REG->data[2]),
				*(unsigned int *)(&DSI_VM_CMD_REG->data[3]));
			}
		} else {
		_WaitForEngineNotBusy();
		{
			/* for(i = 0; i < sizeof(DSI_CMDQ_REG->data0) / sizeof(DSI_CMDQ); i++) */
			/* OUTREG32(&DSI_CMDQ_REG->data0[i], 0); */
			/* memset(&DSI_CMDQ_REG->data[0], 0, sizeof(DSI_CMDQ_REG->data[0])); */
			OUTREG32(&DSI_CMDQ_REG->data[0], 0);

			if (count > 1) {
				t2.CONFG = 2;
				t2.Data_ID = data_id;
				t2.WC16 = count + 1;

				OUTREG32(&DSI_CMDQ_REG->data[0].byte0, AS_UINT32(&t2));

				goto_addr = (UINT32) (&DSI_CMDQ_REG->data[1].byte0);
				mask_para = (0xFF << ((goto_addr & 0x3) * 8));
				set_para = (cmd << ((goto_addr & 0x3) * 8));
				MASKREG32(goto_addr & (~0x3), mask_para, set_para);

				for (i = 0; i < count; i++) {
					goto_addr = (UINT32) (&DSI_CMDQ_REG->data[1].byte1) + i;
					mask_para = (0xFF << ((goto_addr & 0x3) * 8));
					set_para = (para_list[i] << ((goto_addr & 0x3) * 8));
					MASKREG32(goto_addr & (~0x3), mask_para, set_para);
				}

				OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 2 + (count) / 4);
			} else {
				t0.CONFG = 0;
				t0.Data0 = cmd;
				if (count) {
					t0.Data_ID = data_id;
					t0.Data1 = para_list[0];
				} else {
					t0.Data_ID = data_id;
					t0.Data1 = 0;
				}
				OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(&t0));
				OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 1);
			}

			for (i = 0; i < AS_UINT32(&DSI_REG->DSI_CMDQ_SIZE); i++)
				DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI",
					       "DSI_set_cmdq_V3[%d]. DSI_CMDQ+%04x : 0x%08x\n",
					       index, i * 4, INREG32(DSI_BASE + 0x180 + i * 4));

			if (force_update) {
				MMProfileLog(MTKFB_MMP_Events.DSICmd, MMProfileFlagStart);
				DSI_EnableClk();
				for (i = 0; i < 10; i++);
				_WaitForEngineNotBusy();
				MMProfileLog(MTKFB_MMP_Events.DSICmd, MMProfileFlagEnd);
			}
		}
		}
	} while (++index < size);

}

void DSI_cabc_send(unsigned int *pdata, unsigned int queue_size, unsigned char force_update)
{
	DSI_MODE_CTRL_REG mode_ctl, mode_ctl_backup;
#if ENABLE_DSI_INTERRUPT	/* for fixed warning */
	long ret;
	static const long WAIT_TIMEOUT = 2 * HZ;
#endif
	/* backup video mode */
	OUTREG32(&mode_ctl_backup, AS_UINT32(&DSI_REG->DSI_MODE_CTRL));
	OUTREG32(&mode_ctl, AS_UINT32(&DSI_REG->DSI_MODE_CTRL));
	/* set to cmd mode */
	mode_ctl.MODE = 0;
	OUTREG32(&DSI_REG->DSI_MODE_CTRL, AS_UINT32(&mode_ctl));
#if ENABLE_DSI_INTERRUPT	/* wait video mode done */
	ret = wait_event_interruptible_timeout(_dsi_wait_vm_done_queue,
					       !_IsEngineBusy(), WAIT_TIMEOUT);
	if (0 == ret) {
		xlog_printk(ANDROID_LOG_WARN, "DSI",
			    " Wait for DSI engine read ready timeout!!!\n");

		DSI_DumpRegisters();
		/* /do necessary reset here */
		DSI_Reset();
		return;
	}
#endif
	/* to do: send CABC CMD here */
	DSI_set_cmdq(pdata, queue_size, force_update);
	/* end */

	/* restore video mode */
	OUTREG32(&DSI_REG->DSI_MODE_CTRL, AS_UINT32(&mode_ctl_backup));
	disp_if_drv = disphal_get_if_driver();
	needStartDSI = true;
	disp_if_drv->update_screen(false);
}


void DSI_set_cmdq(unsigned int *pdata, unsigned int queue_size, unsigned char force_update)
{
	UINT32 i;
	ASSERT(queue_size <= 16);
	if (0 != DSI_REG->DSI_MODE_CTRL.MODE) {
		DSI_VM_CMD_CON_REG vm_cmdq;
		OUTREG32(&vm_cmdq, AS_UINT32(&DSI_REG->DSI_VM_CMD_CON));
		/*printk("set cmdq in VDO mode\n");*/
		if (queue_size > 1) {
			unsigned int i = 0;
			vm_cmdq.LONG_PKT = 1;
			vm_cmdq.CM_DATA_ID = ((pdata[0] >> 8) & 0xFF);
			vm_cmdq.CM_DATA_0 = ((pdata[0] >> 16) & 0xFF);
			vm_cmdq.CM_DATA_1 = 0;
			OUTREG32(&DSI_REG->DSI_VM_CMD_CON, AS_UINT32(&vm_cmdq));
			for (i = 0; i < queue_size - 1; i++)
				OUTREG32(&DSI_VM_CMD_REG->data[i], AS_UINT32((pdata+i+1)));
		} else {
			vm_cmdq.LONG_PKT = 0;
			vm_cmdq.CM_DATA_ID = ((pdata[0] >> 8) & 0xFF);
			vm_cmdq.CM_DATA_0 = ((pdata[0] >> 16) & 0xFF);
			vm_cmdq.CM_DATA_1 = ((pdata[0] >> 24) & 0xFF);
			OUTREG32(&DSI_REG->DSI_VM_CMD_CON, AS_UINT32(&vm_cmdq));
		}
		/*start DSI VM CMDQ*/
	if (force_update) {
			MMProfileLogEx(MTKFB_MMP_Events.DSICmd, MMProfileFlagStart,
				*(unsigned int *)(&DSI_VM_CMD_REG->data[0]),
				*(unsigned int *)(&DSI_VM_CMD_REG->data[1]));
			DSI_EnableVM_CMD();
			/*must wait VM CMD done?*/
			MMProfileLogEx(MTKFB_MMP_Events.DSICmd, MMProfileFlagEnd,
				*(unsigned int *)(&DSI_VM_CMD_REG->data[2]),
				*(unsigned int *)(&DSI_VM_CMD_REG->data[3]));
		}
	} else {
	_WaitForEngineNotBusy();
#ifdef ENABLE_DSI_ERROR_REPORT
	if ((pdata[0] & 1)) {
		memcpy(_dsi_cmd_queue, pdata, queue_size * 4);
		_dsi_cmd_queue[queue_size++] = 0x4;
		pdata = (unsigned int *)_dsi_cmd_queue;
	} else {
		pdata[0] |= 4;
	}
#endif

	for (i = 0; i < queue_size; i++)
		OUTREG32(&DSI_CMDQ_REG->data[i], AS_UINT32((pdata + i)));

	OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, queue_size);

/* for (i = 0; i < queue_size; i++) */
/* pr_info("[DISP] - kernel - DSI_set_cmdq. DSI_CMDQ+%04x : 0x%08x\n", i*4, INREG32(DSI_BASE + 0x180 + i*4)); */

	if (force_update) {
		MMProfileLogEx(MTKFB_MMP_Events.DSICmd, MMProfileFlagStart,
			       *(unsigned int *)(&DSI_CMDQ_REG->data[0]),
			       *(unsigned int *)(&DSI_CMDQ_REG->data[1]));
		DSI_EnableClk();
		for (i = 0; i < 10; i++);
		_WaitForEngineNotBusy();
		MMProfileLogEx(MTKFB_MMP_Events.DSICmd, MMProfileFlagEnd,
			       *(unsigned int *)(&DSI_CMDQ_REG->data[2]),
			       *(unsigned int *)(&DSI_CMDQ_REG->data[3]));
		}
	}
}


DSI_STATUS DSI_Write_T0_INS(DSI_T0_INS *t0)
{
	OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(t0));

	OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 1);
	OUTREG32(&DSI_REG->DSI_START, 0);
	OUTREG32(&DSI_REG->DSI_START, 1);

	return DSI_STATUS_OK;
}


DSI_STATUS DSI_Write_T1_INS(DSI_T1_INS *t1)
{
	OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(t1));

	OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 1);
	OUTREG32(&DSI_REG->DSI_START, 0);
	OUTREG32(&DSI_REG->DSI_START, 1);

	return DSI_STATUS_OK;
}


DSI_STATUS DSI_Write_T2_INS(DSI_T2_INS *t2)
{
	unsigned int i;

	OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(t2));

	for (i = 0; i < ((t2->WC16 - 1) >> 2) + 1; i++)
		OUTREG32(&DSI_CMDQ_REG->data[1 + i], AS_UINT32((t2->pdata + i)));

	OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, (((t2->WC16 - 1) >> 2) + 2));
	OUTREG32(&DSI_REG->DSI_START, 0);
	OUTREG32(&DSI_REG->DSI_START, 1);

	return DSI_STATUS_OK;
}


DSI_STATUS DSI_Write_T3_INS(DSI_T3_INS *t3)
{
	OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(t3));

	OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 1);
	OUTREG32(&DSI_REG->DSI_START, 0);
	OUTREG32(&DSI_REG->DSI_START, 1);

	return DSI_STATUS_OK;
}

DSI_STATUS DSI_TXRX_Control(bool cksm_en,
			    bool ecc_en,
			    unsigned char lane_num,
			    unsigned char vc_num,
			    bool null_packet_en,
			    bool err_correction_en,
			    bool dis_eotp_en, bool hstx_cklp_en, unsigned int max_return_size)
{
	DSI_TXRX_CTRL_REG tmp_reg;

	tmp_reg = DSI_REG->DSI_TXRX_CTRL;

	/* /TODO: parameter checking */
/* tmp_reg.CKSM_EN=cksm_en; */
/* tmp_reg.ECC_EN=ecc_en; */
	switch (lane_num) {
	case LCM_ONE_LANE:
		tmp_reg.LANE_NUM = 1;
		break;
	case LCM_TWO_LANE:
		tmp_reg.LANE_NUM = 3;
		break;
	case LCM_THREE_LANE:
		tmp_reg.LANE_NUM = 0x7;
		break;
	case LCM_FOUR_LANE:
		tmp_reg.LANE_NUM = 0xF;
		break;
	}
	tmp_reg.VC_NUM = vc_num;
/* tmp_reg.CORR_EN = err_correction_en; */
	tmp_reg.DIS_EOT = dis_eotp_en;
	tmp_reg.NULL_EN = null_packet_en;
	tmp_reg.MAX_RTN_SIZE = max_return_size;
	tmp_reg.HSTX_CKLP_EN = hstx_cklp_en;
	OUTREG32(&DSI_REG->DSI_TXRX_CTRL, AS_UINT32(&tmp_reg));

	return DSI_STATUS_OK;
}


DSI_STATUS DSI_PS_Control(unsigned int ps_type, unsigned int vact_line, unsigned int ps_wc)
{
	DSI_PSCTRL_REG tmp_reg;
	UINT32 tmp_hstx_cklp_wc;
	tmp_reg = DSI_REG->DSI_PSCTRL;

	/* /TODO: parameter checking */
	ASSERT(ps_type <= PACKED_PS_18BIT_RGB666);
	if (ps_type > LOOSELY_PS_18BIT_RGB666)
		tmp_reg.DSI_PS_SEL = (5 - ps_type);
	else
		tmp_reg.DSI_PS_SEL = ps_type;
	tmp_reg.DSI_PS_WC = ps_wc;
	tmp_hstx_cklp_wc = ps_wc;

	OUTREG32(&DSI_REG->DSI_VACT_NL, AS_UINT32(&vact_line));
	OUTREG32(&DSI_REG->DSI_PSCTRL, AS_UINT32(&tmp_reg));
	OUTREG32(&DSI_REG->DSI_HSTX_CKL_WC, tmp_hstx_cklp_wc);
	return DSI_STATUS_OK;
}


DSI_STATUS DSI_ConfigVDOTiming(unsigned int dsi_vsa_nl,
			       unsigned int dsi_vbp_nl,
			       unsigned int dsi_vfp_nl,
			       unsigned int dsi_vact_nl,
			       unsigned int dsi_line_nb,
			       unsigned int dsi_hsa_nb,
			       unsigned int dsi_hbp_nb,
			       unsigned int dsi_hfp_nb,
			       unsigned int dsi_rgb_nb,
			       unsigned int dsi_hsa_wc,
			       unsigned int dsi_hbp_wc, unsigned int dsi_hfp_wc)
{
	/* /TODO: parameter checking */
	OUTREG32(&DSI_REG->DSI_VSA_NL, AS_UINT32(&dsi_vsa_nl));
	OUTREG32(&DSI_REG->DSI_VBP_NL, AS_UINT32(&dsi_vbp_nl));
	OUTREG32(&DSI_REG->DSI_VFP_NL, AS_UINT32(&dsi_vfp_nl));
	OUTREG32(&DSI_REG->DSI_VACT_NL, AS_UINT32(&dsi_vact_nl));
#if 0				/* 6583 not need these parameters */
	OUTREG32(&DSI_REG->DSI_LINE_NB, AS_UINT32(&dsi_line_nb));
	OUTREG32(&DSI_REG->DSI_HSA_NB, AS_UINT32(&dsi_hsa_nb));
	OUTREG32(&DSI_REG->DSI_HBP_NB, AS_UINT32(&dsi_hbp_nb));
	OUTREG32(&DSI_REG->DSI_HFP_NB, AS_UINT32(&dsi_hfp_nb));
	OUTREG32(&DSI_REG->DSI_RGB_NB, AS_UINT32(&dsi_rgb_nb));
#endif
	OUTREG32(&DSI_REG->DSI_HSA_WC, AS_UINT32(&dsi_hsa_wc));
	OUTREG32(&DSI_REG->DSI_HBP_WC, AS_UINT32(&dsi_hbp_wc));
	OUTREG32(&DSI_REG->DSI_HFP_WC, AS_UINT32(&dsi_hfp_wc));

	return DSI_STATUS_OK;
}

void DSI_write_lcm_cmd(unsigned int cmd)
{
	DSI_T0_INS t0_tmp;
	DSI_CMDQ_CONFG CONFG_tmp;

	CONFG_tmp.type = SHORT_PACKET_RW;
	CONFG_tmp.BTA = DISABLE_BTA;
	CONFG_tmp.HS = LOW_POWER;
	CONFG_tmp.CL = CL_8BITS;
	CONFG_tmp.TE = DISABLE_TE;
	CONFG_tmp.RPT = DISABLE_RPT;

	t0_tmp.CONFG = *((unsigned char *)(&CONFG_tmp));
	t0_tmp.Data_ID = (cmd & 0xFF);
	t0_tmp.Data0 = 0x0;
	t0_tmp.Data1 = 0x0;

	DSI_Write_T0_INS(&t0_tmp);
}


void DSI_write_lcm_regs(unsigned int addr, unsigned int *para, unsigned int nums)
{
	DSI_T2_INS t2_tmp;
	DSI_CMDQ_CONFG CONFG_tmp;

	CONFG_tmp.type = LONG_PACKET_W;
	CONFG_tmp.BTA = DISABLE_BTA;
	CONFG_tmp.HS = LOW_POWER;
	CONFG_tmp.CL = CL_8BITS;
	CONFG_tmp.TE = DISABLE_TE;
	CONFG_tmp.RPT = DISABLE_RPT;

	t2_tmp.CONFG = *((unsigned char *)(&CONFG_tmp));
	t2_tmp.Data_ID = (addr & 0xFF);
	t2_tmp.WC16 = nums;
	t2_tmp.pdata = para;

	DSI_Write_T2_INS(&t2_tmp);

}

UINT32 DSI_dcs_read_lcm_reg(UINT8 cmd)
{
	/* UINT32 max_try_count = 5; //for fixed warning */
	UINT32 recv_data = 0x0;	/* UINT32 recv_data; //for fixed warning */
	/* UINT32 recv_data_cnt;  //for fixed warning */
	/* unsigned int read_timeout_ms; //for fixed warning */
	/* unsigned char packet_type; //for fixed warning */
#if 0
	DSI_T0_INS t0;
#if ENABLE_DSI_INTERRUPT
	static const long WAIT_TIMEOUT = 2 * HZ;	/* 2 sec */
	long ret;
#endif

	if (DSI_REG->DSI_MODE_CTRL.MODE)
		return 0;

	do {
		if (max_try_count == 0)
			return 0;

		max_try_count--;
		recv_data = 0;
		recv_data_cnt = 0;
		read_timeout_ms = 20;

		_WaitForEngineNotBusy();

		t0.CONFG = 0x04;	/* /BTA */
		t0.Data0 = cmd;
		t0.Data_ID = DSI_DCS_READ_PACKET_ID;
		t0.Data1 = 0;

		OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(&t0));
		OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 1);

		/* /clear read ACK */
		DSI_REG->DSI_RACK.DSI_RACK = 1;
		DSI_REG->DSI_INTSTA.RD_RDY = 1;
		DSI_REG->DSI_INTSTA.CMD_DONE = 1;
		DSI_REG->DSI_INTEN.RD_RDY = 1;
		DSI_REG->DSI_INTEN.CMD_DONE = 1;

		OUTREG32(&DSI_REG->DSI_START, 0);
		OUTREG32(&DSI_REG->DSI_START, 1);

		/* / the following code is to */
		/* / 1: wait read ready */
		/* / 2: ack read ready */
		/* / 3: wait for CMDQ_DONE */
		/* / 3: read data */
#if ENABLE_DSI_INTERRUPT
		ret = wait_event_interruptible_timeout(_dsi_dcs_read_wait_queue,
						       !_IsEngineBusy(), WAIT_TIMEOUT);
		if (0 == ret) {
			DISP_LOG_PRINT(ANDROID_LOG_WARN, "DSI",
				       " Wait for DSI engine read ready timeout!!!\n");

			DSI_DumpRegisters();

			/* /do necessary reset here */
			DSI_REG->DSI_RACK.DSI_RACK = 1;
			DSI_Reset();

			return 0;
		}
#else
#ifdef DSI_DRV_DEBUG_LOG_ENABLE
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " Start polling DSI read ready!!!\n");
#endif
		while (DSI_REG->DSI_INTSTA.RD_RDY == 0)	/* /read clear */
		{
			/* /keep polling */
			msleep(1);
			read_timeout_ms--;

			if (read_timeout_ms == 0) {
				DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI",
					       " Polling DSI read ready timeout!!!\n");
				DSI_DumpRegisters();

				/* /do necessary reset here */
				DSI_REG->DSI_RACK.DSI_RACK = 1;
				DSI_Reset();
				return 0;
			}
		}
#ifdef DSI_DRV_DEBUG_LOG_ENABLE
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " End polling DSI read ready!!!\n");
#endif

		DSI_REG->DSI_RACK.DSI_RACK = 1;

		while (DSI_REG->DSI_STA.BUF_UNDERRUN || DSI_REG->DSI_STA.ESC_ENTRY_ERR
		       || DSI_REG->DSI_STA.LPDT_SYNC_ERR || DSI_REG->DSI_STA.CTRL_ERR
		       || DSI_REG->DSI_STA.CONTENT_ERR) {
			/* /DSI READ ACK HW bug workaround */
#ifdef DSI_DRV_DEBUG_LOG_ENABLE
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "DSI is busy: 0x%x !!!\n",
				       DSI_REG->DSI_STA.BUSY);
#endif
			DSI_REG->DSI_RACK.DSI_RACK = 1;
		}


		/* /clear interrupt status */
		DSI_REG->DSI_INTSTA.RD_RDY = 1;
		/* /STOP DSI */
		OUTREG32(&DSI_REG->DSI_START, 0);

#endif

		DSI_REG->DSI_INTEN.RD_RDY = 0;

#ifdef DSI_DRV_DEBUG_LOG_ENABLE
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_STA : 0x%x\n",
			       DSI_REG->DSI_RX_STA);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_CMDQ_SIZE : 0x%x\n",
			       DSI_REG->DSI_CMDQ_SIZE.CMDQ_SIZE);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_CMDQ_DATA0 : 0x%x\n",
			       DSI_CMDQ_REG->data[0].byte0);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_CMDQ_DATA1 : 0x%x\n",
			       DSI_CMDQ_REG->data[0].byte1);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_CMDQ_DATA2 : 0x%x\n",
			       DSI_CMDQ_REG->data[0].byte2);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_CMDQ_DATA3 : 0x%x\n",
			       DSI_CMDQ_REG->data[0].byte3);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_DATA.BYTE0 : 0x%x\n",
			       DSI_REG->DSI_RX_DATA.BYTE0);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_DATA.BYTE1 : 0x%x\n",
			       DSI_REG->DSI_RX_DATA.BYTE1);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_DATA.BYTE2 : 0x%x\n",
			       DSI_REG->DSI_RX_DATA.BYTE2);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_DATA.BYTE3 : 0x%x\n",
			       DSI_REG->DSI_RX_DATA.BYTE3);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_DATA.BYTE4 : 0x%x\n",
			       DSI_REG->DSI_RX_DATA.BYTE4);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_DATA.BYTE5 : 0x%x\n",
			       DSI_REG->DSI_RX_DATA.BYTE5);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_DATA.BYTE6 : 0x%x\n",
			       DSI_REG->DSI_RX_DATA.BYTE6);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_DATA.BYTE7 : 0x%x\n",
			       DSI_REG->DSI_RX_DATA.BYTE7);
#endif
		packet_type = DSI_REG->DSI_RX_DATA.BYTE0;

#ifdef DSI_DRV_DEBUG_LOG_ENABLE
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI read packet_type is 0x%x\n",
			       packet_type);
#endif
		if (DSI_REG->DSI_RX_STA.LONG == 1) {
			recv_data_cnt =
			    DSI_REG->DSI_RX_DATA.BYTE1 + DSI_REG->DSI_RX_DATA.BYTE2 * 16;
			if (recv_data_cnt > 4) {
#ifdef DSI_DRV_DEBUG_LOG_ENABLE
				DISP_LOG_PRINT(ANDROID_LOG_WARN, "DSI",
					       " DSI read long packet data  exceeds 4 bytes\n");
#endif
				recv_data_cnt = 4;
			}
			memcpy((void *)&recv_data, (void *)&DSI_REG->DSI_RX_DATA.BYTE4,
			       recv_data_cnt);
		} else {
			memcpy((void *)&recv_data, (void *)&DSI_REG->DSI_RX_DATA.BYTE1, 2);
		}

#ifdef DSI_DRV_DEBUG_LOG_ENABLE
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI read 0x%x data is 0x%x\n", cmd,
			       recv_data);
#endif
	} while (packet_type != 0x1C && packet_type != 0x21 && packet_type != 0x22);
	/* / here: we may receive a ACK packet which packet type is 0x02 (incdicates some error happened) */
	/* / therefore we try re-read again until no ACK packet */
	/* / But: if it is a good way to keep re-trying ??? */
#endif
	return recv_data;
}

/* / return value: the data length we got */
UINT32 DSI_dcs_read_lcm_reg_v2(UINT8 cmd, UINT8 *buffer, UINT8 buffer_size)
{
	UINT32 max_try_count = 5;
	UINT32 recv_data_cnt;
	unsigned int read_timeout_ms;
	unsigned char packet_type;
	DSI_RX_DATA_REG read_data0;
	DSI_RX_DATA_REG read_data1;
	DSI_RX_DATA_REG read_data2;
	DSI_RX_DATA_REG read_data3;
#if 1
	DSI_T0_INS t0;

#if ENABLE_DSI_INTERRUPT
	static const long WAIT_TIMEOUT = 2 * HZ;	/* 2 sec */
	long ret;
#endif
	if (DSI_REG->DSI_MODE_CTRL.MODE)
		return 0;

	if (buffer == NULL || buffer_size == 0)
		return 0;

	do {
		if (max_try_count == 0)
			return 0;
		max_try_count--;
		recv_data_cnt = 0;
		read_timeout_ms = 20;

		_WaitForEngineNotBusy();

		t0.CONFG = 0x04;	/* /BTA */
		t0.Data0 = cmd;
		if (buffer_size < 0x3)
			t0.Data_ID = DSI_DCS_READ_PACKET_ID;
		else
			t0.Data_ID = DSI_GERNERIC_READ_LONG_PACKET_ID;
		t0.Data1 = 0;

		OUTREG32(&DSI_CMDQ_REG->data[0], AS_UINT32(&t0));
		OUTREG32(&DSI_REG->DSI_CMDQ_SIZE, 1);

		/* /clear read ACK */
		/* DSI_REG->DSI_RACK.DSI_RACK = 1; */
		/* DSI_REG->DSI_INTSTA.RD_RDY = 1; */
		/* DSI_REG->DSI_INTSTA.CMD_DONE = 1; */
		/* DSI_REG->DSI_INTEN.RD_RDY =  1; */
		/* DSI_REG->DSI_INTEN.CMD_DONE=  1; */
		OUTREGBIT(DSI_RACK_REG, DSI_REG->DSI_RACK, DSI_RACK, 1);
		OUTREGBIT(DSI_INT_STATUS_REG, DSI_REG->DSI_INTSTA, RD_RDY, 1);
		OUTREGBIT(DSI_INT_STATUS_REG, DSI_REG->DSI_INTSTA, CMD_DONE, 1);
		OUTREGBIT(DSI_INT_ENABLE_REG, DSI_REG->DSI_INTEN, RD_RDY, 1);
		OUTREGBIT(DSI_INT_ENABLE_REG, DSI_REG->DSI_INTEN, CMD_DONE, 1);



		OUTREG32(&DSI_REG->DSI_START, 0);
		OUTREG32(&DSI_REG->DSI_START, 1);

		/* / the following code is to */
		/* / 1: wait read ready */
		/* / 2: ack read ready */
		/* / 3: wait for CMDQ_DONE */
		/* / 3: read data */
#if ENABLE_DSI_INTERRUPT
		ret = wait_event_interruptible_timeout(_dsi_dcs_read_wait_queue,
						       !_IsEngineBusy(), WAIT_TIMEOUT);
		if (0 == ret) {
			xlog_printk(ANDROID_LOG_WARN, "DSI",
				    " Wait for DSI engine read ready timeout!!!\n");

			DSI_DumpRegisters();

			/* /do necessary reset here */
			/* DSI_REG->DSI_RACK.DSI_RACK = 1; */
			OUTREGBIT(DSI_RACK_REG, DSI_REG->DSI_RACK, DSI_RACK, 1);
			DSI_Reset();

			return 0;
		}
#else
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
		if (dsi_log_on)
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI",
				       " Start polling DSI read ready!!!\n");
#endif
		while (DSI_REG->DSI_INTSTA.RD_RDY == 0)	/* /read clear */
		{
			/* /keep polling */
			msleep(1);
			read_timeout_ms--;

			if (read_timeout_ms == 0) {
				DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI",
					       " Polling DSI read ready timeout!!!\n");
				DSI_DumpRegisters();

				/* /do necessary reset here */
				/* DSI_REG->DSI_RACK.DSI_RACK = 1; */
				OUTREGBIT(DSI_RACK_REG, DSI_REG->DSI_RACK, DSI_RACK, 1);
				DSI_Reset();
				return 0;
			}
		}
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
		if (dsi_log_on)
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " End polling DSI read ready!!!\n");
#endif

		/* DSI_REG->DSI_RACK.DSI_RACK = 1; */
		OUTREGBIT(DSI_RACK_REG, DSI_REG->DSI_RACK, DSI_RACK, 1);

		/* /clear interrupt status */
		/* DSI_REG->DSI_INTSTA.RD_RDY = 1; */
		OUTREGBIT(DSI_INT_STATUS_REG, DSI_REG->DSI_INTSTA, RD_RDY, 1);
		/* /STOP DSI */
		OUTREG32(&DSI_REG->DSI_START, 0);

#endif

		/* DSI_REG->DSI_INTEN.RD_RDY =  0; */
		OUTREGBIT(DSI_INT_ENABLE_REG, DSI_REG->DSI_INTEN, RD_RDY, 1);

		OUTREG32(&read_data0, AS_UINT32(&DSI_REG->DSI_RX_DATA0));
		OUTREG32(&read_data1, AS_UINT32(&DSI_REG->DSI_RX_DATA1));
		OUTREG32(&read_data2, AS_UINT32(&DSI_REG->DSI_RX_DATA2));
		OUTREG32(&read_data3, AS_UINT32(&DSI_REG->DSI_RX_DATA3));
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
		if (dsi_log_on) {
			unsigned int i;
/* DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_STA : 0x%x\n", DSI_REG->DSI_RX_STA); */
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_CMDQ_SIZE : 0x%x\n",
				       DSI_REG->DSI_CMDQ_SIZE.CMDQ_SIZE);
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_CMDQ_DATA0 : 0x%x\n",
				       DSI_CMDQ_REG->data[0].byte0);
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_CMDQ_DATA1 : 0x%x\n",
				       DSI_CMDQ_REG->data[0].byte1);
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_CMDQ_DATA2 : 0x%x\n",
				       DSI_CMDQ_REG->data[0].byte2);
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_CMDQ_DATA3 : 0x%x\n",
				       DSI_CMDQ_REG->data[0].byte3);
#if 1
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_DATA0 : 0x%x\n",
				       DSI_REG->DSI_RX_DATA0);
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_DATA1 : 0x%x\n",
				       DSI_REG->DSI_RX_DATA1);
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_DATA2 : 0x%x\n",
				       DSI_REG->DSI_RX_DATA2);
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI_RX_DATA3 : 0x%x\n",
				       DSI_REG->DSI_RX_DATA3);

			pr_info("read_data0, %x,%x,%x,%x\n", read_data0.byte0, read_data0.byte1,
				read_data0.byte2, read_data0.byte3);
			pr_info("read_data1, %x,%x,%x,%x\n", read_data1.byte0, read_data1.byte1,
				read_data1.byte2, read_data1.byte3);
			pr_info("read_data2, %x,%x,%x,%x\n", read_data2.byte0, read_data2.byte1,
				read_data2.byte2, read_data2.byte3);
			pr_info("read_data3, %x,%x,%x,%x\n", read_data3.byte0, read_data3.byte1,
				read_data3.byte2, read_data3.byte3);
#endif
		}
#endif

#if 1
		packet_type = read_data0.byte0;

#ifdef DDI_DRV_DEBUG_LOG_ENABLE
		if (dsi_log_on)
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", " DSI read packet_type is 0x%x\n",
				       packet_type);
#endif



		if (packet_type == 0x1A || packet_type == 0x1C) {
			recv_data_cnt = read_data0.byte1 + read_data0.byte2 * 16;
			if (recv_data_cnt > 10) {
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
				if (dsi_log_on)
					DISP_LOG_PRINT(ANDROID_LOG_WARN, "DSI",
						       " DSI read long packet data  exceeds 4 bytes\n");
#endif
				recv_data_cnt = 10;
			}

			if (recv_data_cnt > buffer_size) {
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
				if (dsi_log_on)
					DISP_LOG_PRINT(ANDROID_LOG_WARN, "DSI",
						       " DSI read long packet data  exceeds buffer size: %d\n",
						       buffer_size);
#endif
				recv_data_cnt = buffer_size;
			}
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
			if (dsi_log_on)
				DISP_LOG_PRINT(ANDROID_LOG_WARN, "DSI",
					       " DSI read long packet size: %d\n", recv_data_cnt);
#endif
			if (recv_data_cnt <= 4)
				memcpy((void *)buffer, (void *)&read_data1, recv_data_cnt);
			else if (recv_data_cnt <= 8 && recv_data_cnt >= 5) {
				memcpy((void *)buffer, (void *)&read_data1, 4);
				memcpy((void *)(buffer + 4), (void *)&read_data2, recv_data_cnt-4);
					}
			else {
				memcpy((void *)buffer, (void *)&read_data1, 4);
				memcpy((void *)(buffer + 4), (void *)&read_data2, 4);
				memcpy((void *)(buffer + 8), (void *)&read_data3, recv_data_cnt-8);
					}
		} else {
			if (recv_data_cnt > buffer_size) {
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
				if (dsi_log_on)
					DISP_LOG_PRINT(ANDROID_LOG_WARN, "DSI",
						       " DSI read short packet data  exceeds buffer size: %d\n",
						       buffer_size);
#endif
				recv_data_cnt = buffer_size;
			}
			memcpy((void *)buffer, (void *)&read_data0.byte1, 2);
		}
#endif
	} while (packet_type != 0x1C && packet_type != 0x21 && packet_type != 0x22
		 && packet_type != 0x1A);
	/* / here: we may receive a ACK packet which packet type is 0x02 (incdicates some error happened) */
	/* / therefore we try re-read again until no ACK packet */
	/* / But: if it is a good way to keep re-trying ??? */
#endif
	return recv_data_cnt;
}

UINT32 DSI_read_lcm_reg(void)
{
	return 0;
}


DSI_STATUS DSI_write_lcm_fb(unsigned int addr, bool long_length)
{
	DSI_T1_INS t1_tmp;
	DSI_CMDQ_CONFG CONFG_tmp;

	CONFG_tmp.type = FB_WRITE;
	CONFG_tmp.BTA = DISABLE_BTA;
	CONFG_tmp.HS = HIGH_SPEED;

	if (long_length)
		CONFG_tmp.CL = CL_16BITS;
	else
		CONFG_tmp.CL = CL_8BITS;

	CONFG_tmp.TE = DISABLE_TE;
	CONFG_tmp.RPT = DISABLE_RPT;


	t1_tmp.CONFG = *((unsigned char *)(&CONFG_tmp));
	t1_tmp.Data_ID = 0x39;
	t1_tmp.mem_start0 = (addr & 0xFF);

	if (long_length)
		t1_tmp.mem_start1 = ((addr >> 8) & 0xFF);

	return DSI_Write_T1_INS(&t1_tmp);


}


DSI_STATUS DSI_read_lcm_fb(void)
{
	/* TBD */
	return DSI_STATUS_OK;
}

DSI_STATUS DSI_enable_MIPI_txio(bool en)
{
#if 0
	if (en) {
		*(volatile unsigned int *)(INFRACFG_BASE + 0x890) |= 0x00000100;	/* enable MIPI TX IO */
	} else {
		*(volatile unsigned int *)(INFRACFG_BASE + 0x890) &= ~0x00000100;	/* disable MIPI TX IO */
	}
#endif
	return DSI_STATUS_OK;
}


bool Need_Wait_ULPS(void)
{
#ifndef MT65XX_NEW_DISP
	if (((INREG32(DSI_BASE + 0x14C) >> 24) & 0xFF) != 0x04) {

#ifdef DDI_DRV_DEBUG_LOG_ENABLE
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[%s]:true\n", __func__);
#endif
		return TRUE;

	} else {

#ifdef DDI_DRV_DEBUG_LOG_ENABLE
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[%s]:false\n", __func__);
#endif
		return FALSE;

	}
#else				/* for fixed warning */
	return TRUE;		/* for fixed warning */
#endif
}


DSI_STATUS Wait_ULPS_Mode(void)
{
#ifndef MT65XX_NEW_DISP
	DSI_PHY_LCCON_REG lccon_reg = DSI_REG->DSI_PHY_LCCON;
	DSI_PHY_LD0CON_REG ld0con = DSI_REG->DSI_PHY_LD0CON;

	lccon_reg.LC_ULPM_EN = 1;
	ld0con.L0_ULPM_EN = 1;
	OUTREG32(&DSI_REG->DSI_PHY_LCCON, AS_UINT32(&lccon_reg));
	OUTREG32(&DSI_REG->DSI_PHY_LD0CON, AS_UINT32(&ld0con));

#ifdef DDI_DRV_DEBUG_LOG_ENABLE
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[%s]:enter\n", __func__);
#endif

	while (((INREG32(DSI_BASE + 0x14C) >> 24) & 0xFF) != 0x04) {
		lcm_mdelay(5);
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "DSI+%04x : 0x%08x\n", DSI_BASE,
			       INREG32(DSI_BASE + 0x14C));
#endif
	}

#ifdef DDI_DRV_DEBUG_LOG_ENABLE
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[%s]:exit\n", __func__);
#endif
#endif
	return DSI_STATUS_OK;

}


DSI_STATUS Wait_WakeUp(void)
{
#ifndef MT65XX_NEW_DISP
	DSI_PHY_LCCON_REG lccon_reg = DSI_REG->DSI_PHY_LCCON;
	DSI_PHY_LD0CON_REG ld0con = DSI_REG->DSI_PHY_LD0CON;

#ifdef DDI_DRV_DEBUG_LOG_ENABLE
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[%s]:enter\n", __func__);
#endif

	lccon_reg.LC_ULPM_EN = 0;
	ld0con.L0_ULPM_EN = 0;
	OUTREG32(&DSI_REG->DSI_PHY_LCCON, AS_UINT32(&lccon_reg));
	OUTREG32(&DSI_REG->DSI_PHY_LD0CON, AS_UINT32(&ld0con));

	mdelay(1);		/* Wait 1ms for LCM Spec */

	lccon_reg.LC_WAKEUP_EN = 1;
	ld0con.L0_WAKEUP_EN = 1;
	OUTREG32(&DSI_REG->DSI_PHY_LCCON, AS_UINT32(&lccon_reg));
	OUTREG32(&DSI_REG->DSI_PHY_LD0CON, AS_UINT32(&ld0con));

	while (((INREG32(DSI_BASE + 0x148) >> 8) & 0xFF) != 0x01) {
		lcm_mdelay(5);
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[soso]DSI+%04x : 0x%08x\n", DSI_BASE,
			       INREG32(DSI_BASE + 0x148));
#endif
	}

#ifdef DDI_DRV_DEBUG_LOG_ENABLE
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "[%s]:exit\n", __func__);
#endif

	lccon_reg.LC_WAKEUP_EN = 0;
	ld0con.L0_WAKEUP_EN = 0;
	OUTREG32(&DSI_REG->DSI_PHY_LCCON, AS_UINT32(&lccon_reg));
	OUTREG32(&DSI_REG->DSI_PHY_LD0CON, AS_UINT32(&ld0con));
#endif
	return DSI_STATUS_OK;

}

/* -------------------- Retrieve Information -------------------- */

DSI_STATUS DSI_DumpRegisters(void)
{
	UINT32 i;

	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "---------- Start dump DSI registers ----------\n");

	for (i = 0; i < sizeof(DSI_REGS); i += 4) {
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "DSI+%04x : 0x%08x\n", i,
			       INREG32(DSI_BASE + i));
	}

	for (i = 0; i < sizeof(DSI_CMDQ_REGS); i += 4) {
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "DSI_CMD+%04x(%p) : 0x%08x\n", i,
			       (UINT32 *) (DSI_BASE + 0x180 + i), INREG32((DSI_BASE + 0x180 + i)));
	}

	for (i = 0; i < sizeof(DSI_PHY_REGS); i += 4) {
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DSI", "DSI_PHY+%04x(%p) : 0x%08x\n", i,
			       (UINT32 *) (MIPI_CONFIG_BASE + 0x800 + i),
			       INREG32((MIPI_CONFIG_BASE + 0x800 + i)));
	}

	return DSI_STATUS_OK;
}


static LCM_PARAMS lcm_params_for_clk_setting;


DSI_STATUS DSI_FMDesense_Query(void)
{
	return DSI_STATUS_OK;
}

DSI_STATUS DSI_FM_Desense(unsigned long freq)
{
	/* /need check */
	DSI_Change_CLK(freq);
	return DSI_STATUS_OK;
}

DSI_STATUS DSI_Reset_CLK(void)
{
	extern LCM_PARAMS *lcm_params;

	_WaitForEngineNotBusy();
	DSI_PHY_TIMCONFIG(lcm_params);
	DSI_PHY_clk_setting(lcm_params);
	return DSI_STATUS_OK;
}

DSI_STATUS DSI_Get_Default_CLK(unsigned int *clk)
{
	extern LCM_PARAMS *lcm_params;
	unsigned int div2_real = lcm_params->dsi.pll_div2 ? lcm_params->dsi.pll_div2 : 0x1;

	*clk = 13 * (lcm_params->dsi.pll_div1 + 1) / div2_real;
	return DSI_STATUS_OK;
}

DSI_STATUS DSI_Get_Current_CLK(unsigned int *clk)
{

	return DSI_STATUS_OK;
}

DSI_STATUS DSI_Change_CLK(unsigned int clk)
{
	extern LCM_PARAMS *lcm_params;

	if (clk > 1000)
		return DSI_STATUS_ERROR;
	memcpy((void *)&lcm_params_for_clk_setting, (void *)lcm_params, sizeof(LCM_PARAMS));

	for (lcm_params_for_clk_setting.dsi.pll_div2 = 15;
	     lcm_params_for_clk_setting.dsi.pll_div2 > 0;
	     lcm_params_for_clk_setting.dsi.pll_div2--) {
		for (lcm_params_for_clk_setting.dsi.pll_div1 = 0;
		     lcm_params_for_clk_setting.dsi.pll_div1 < 39;
		     lcm_params_for_clk_setting.dsi.pll_div1++) {
			if ((13 * (lcm_params_for_clk_setting.dsi.pll_div1 + 1) /
			     lcm_params_for_clk_setting.dsi.pll_div2) >= clk)
				goto end;
		}
	}

	if (lcm_params_for_clk_setting.dsi.pll_div2 == 0) {
		for (lcm_params_for_clk_setting.dsi.pll_div1 = 0;
		     lcm_params_for_clk_setting.dsi.pll_div1 < 39;
		     lcm_params_for_clk_setting.dsi.pll_div1++) {
			if ((26 * (lcm_params_for_clk_setting.dsi.pll_div1 + 1)) >= clk)
				goto end;
		}
	}

 end:
	_WaitForEngineNotBusy();
	DSI_PHY_TIMCONFIG(&lcm_params_for_clk_setting);
	DSI_PHY_clk_setting(&lcm_params_for_clk_setting);

	return DSI_STATUS_OK;
}

DSI_STATUS DSI_Capture_Framebuffer(unsigned int pvbuf, unsigned int bpp, bool cmd_mode)
{
	unsigned int mva;
	unsigned int ret = 0;
	M4U_PORT_STRUCT portStruct;

	struct disp_path_config_mem_out_struct mem_out = { 0 };
	pr_info("enter DSI_Capture_FB!\n");

	if (bpp == 32)
		mem_out.outFormat = eARGB8888;
	else if (bpp == 16)
		mem_out.outFormat = eRGB565;
	else if (bpp == 24)
		mem_out.outFormat = eRGB888;
	else
		pr_info("DSI_Capture_FB, fb color format not support\n");

	pr_info("before alloc MVA: va = 0x%x, size = %d\n", pvbuf,
		lcm_params->height * lcm_params->width * bpp / 8);
	ret =
	    m4u_alloc_mva(M4U_CLNTMOD_WDMA, pvbuf, lcm_params->height * lcm_params->width * bpp / 8,
			  0, 0, &mva);
	if (ret != 0) {
		pr_info("m4u_alloc_mva() fail!\n");
		return DSI_STATUS_OK;
	}
	pr_info("addr=0x%x, format=%d \n", mva, mem_out.outFormat);	/* pr_info("addr=0x%x, format=d\n", mva, mem_out.outFormat); //for fixed warning */

	m4u_dma_cache_maint(M4U_CLNTMOD_WDMA, (const void *)pvbuf,	/* pvbuf,    //for fixed warning */
			    lcm_params->height * lcm_params->width * bpp / 8, DMA_BIDIRECTIONAL);

	portStruct.ePortID = M4U_PORT_WDMA1;	/* hardware port ID, defined in M4U_PORT_ID_ENUM */
	portStruct.Virtuality = 1;
	portStruct.Security = 0;
	portStruct.domain = 0;	/* domain : 0 1 2 3 */
	portStruct.Distance = 1;
	portStruct.Direction = 0;
	m4u_config_port(&portStruct);

	mem_out.enable = 1;
	mem_out.dstAddr = mva;
	mem_out.srcROI.x = 0;
	mem_out.srcROI.y = 0;
	mem_out.srcROI.height = lcm_params->height;
	mem_out.srcROI.width = lcm_params->width;

	_WaitForEngineNotBusy();
	disp_path_get_mutex();
	disp_path_config_mem_out(&mem_out);
	pr_info("Wait DSI idle\n");

	if (cmd_mode)
		DSI_EnableClk();

	disp_path_release_mutex();

	_WaitForEngineNotBusy();
/* msleep(20); */
	disp_path_get_mutex();
	mem_out.enable = 0;
	disp_path_config_mem_out(&mem_out);

	if (cmd_mode)
		DSI_EnableClk();

	disp_path_release_mutex();

	portStruct.ePortID = M4U_PORT_WDMA1;	/* hardware port ID, defined in M4U_PORT_ID_ENUM */
	portStruct.Virtuality = 0;
	portStruct.Security = 0;
	portStruct.domain = 0;	/* domain : 0 1 2 3 */
	portStruct.Distance = 1;
	portStruct.Direction = 0;
	m4u_config_port(&portStruct);

	m4u_dealloc_mva(M4U_CLNTMOD_WDMA,
			pvbuf, lcm_params->height * lcm_params->width * bpp / 8, mva);

	return DSI_STATUS_OK;
}


DSI_STATUS DSI_TE_Enable(BOOL enable)
{
	dsiTeEnable = enable;

	return DSI_STATUS_OK;
}

#if 1
unsigned char DSCRead(UINT8 cmd)
{
	unsigned char buffer[2], bVal;
	unsigned int array[16];


/* ///////// */
	DSI_MODE_CTRL_REG mode_ctl, mode_ctl_backup;
#if ENABLE_DSI_INTERRUPT	/* for fixed warning */
	long ret;
	static const long WAIT_TIMEOUT = 1 * HZ;
#endif
	/* bool result = false; */
	/* backup video mode */
	OUTREG32(&mode_ctl_backup, AS_UINT32(&DSI_REG->DSI_MODE_CTRL));
	OUTREG32(&mode_ctl, AS_UINT32(&DSI_REG->DSI_MODE_CTRL));
	/* set to cmd mode */
	mode_ctl.MODE = 0;
	OUTREG32(&DSI_REG->DSI_MODE_CTRL, AS_UINT32(&mode_ctl));
#if ENABLE_DSI_INTERRUPT	/* wait video mode done */
	/* static const long WAIT_TIMEOUT = 2 * HZ;      // 2 sec   //for fixed warning */
	/* long ret; */
	/* static const long WAIT_TIMEOUT = 2 * HZ;      // 2 sec  //for fixed warning */

	ret = wait_event_interruptible_timeout(_dsi_wait_vm_done_queue,
					       !_IsEngineBusy(), WAIT_TIMEOUT);
	if (0 == ret) {
		xlog_printk(ANDROID_LOG_WARN, "DSI",
			    " Wait for DSI engine read ready timeout!!!\n");

		DSI_DumpRegisters();
		/* /do necessary reset here */
		DSI_Reset();
		return 0;
	}
#else
#endif
/* //////////// */



	bVal = 0;
	array[0] = 0x00023700;	/* read id return two byte,version and id */
	DSI_set_cmdq(array, 1, 1);
	DSI_dcs_read_lcm_reg_v2(cmd, buffer, 2);

	/* ////////////////// */
	OUTREG32(&DSI_REG->DSI_MODE_CTRL, AS_UINT32(&mode_ctl_backup));
	/* //////////////// */

	DSI_LP_Reset();
	DSI_CHECK_RET(DSI_enable_MIPI_txio(TRUE));

	/* DSI_CHECK_RET(DSI_handle_TE()); */

	DSI_SetMode(lcm_params->dsi.mode);

	DSI_clk_HS_mode(1);
	DSI_CHECK_RET(DSI_StartTransfer(false));


	bVal = buffer[0];	/* we only need ID */


	return bVal;
}
EXPORT_SYMBOL(DSCRead);

void DSCWrite(UINT8 cmd, UINT8 bVal)
{
	unsigned int array[16];

/* ///////// */
	DSI_MODE_CTRL_REG mode_ctl, mode_ctl_backup;
#if ENABLE_DSI_INTERRUPT	/* for fixed warning */
	long ret;
	static const long WAIT_TIMEOUT = 1 * HZ;
#endif
	/* bool result = false; */
	/* backup video mode */
	OUTREG32(&mode_ctl_backup, AS_UINT32(&DSI_REG->DSI_MODE_CTRL));
	OUTREG32(&mode_ctl, AS_UINT32(&DSI_REG->DSI_MODE_CTRL));
	/* set to cmd mode */
	mode_ctl.MODE = 0;
	OUTREG32(&DSI_REG->DSI_MODE_CTRL, AS_UINT32(&mode_ctl));
#if ENABLE_DSI_INTERRUPT	/* wait video mode done */
	/* static const long WAIT_TIMEOUT = 2 * HZ;      // 2 sec   //for fixed warning */
	/* long ret; */
	/* static const long WAIT_TIMEOUT = 2 * HZ;      // 2 sec  //for fixed warning */

	ret = wait_event_interruptible_timeout(_dsi_wait_vm_done_queue,
					       !_IsEngineBusy(), WAIT_TIMEOUT);
	if (0 == ret) {
		xlog_printk(ANDROID_LOG_WARN, "DSI",
			    " Wait for DSI engine read ready timeout!!!\n");

		DSI_DumpRegisters();
		/* /do necessary reset here */
		DSI_Reset();
		return;
	}
#else
#endif
/* //////////// */

	array[0] = 0x00001500 | (cmd << 16) | (bVal << 24);	/* write 1 byte to cmd */
	DSI_set_cmdq(array, 1, 1);

	/* ////////////////// */
	OUTREG32(&DSI_REG->DSI_MODE_CTRL, AS_UINT32(&mode_ctl_backup));
	/* //////////////// */

	DSI_LP_Reset();
	DSI_CHECK_RET(DSI_enable_MIPI_txio(TRUE));

	/* DSI_CHECK_RET(DSI_handle_TE()); */

	DSI_SetMode(lcm_params->dsi.mode);

	DSI_clk_HS_mode(1);
	DSI_CHECK_RET(DSI_StartTransfer(false));


}
EXPORT_SYMBOL(DSCWrite);
#endif
