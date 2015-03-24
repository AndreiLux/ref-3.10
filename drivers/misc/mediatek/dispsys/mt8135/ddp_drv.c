#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/param.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kthread.h>

#include <linux/xlog.h>
#include <linux/proc_fs.h>	/* proc file use */

#include <asm/io.h>


#include <mach/irqs.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_irq.h>
#include <mach/irqs.h>
#include <mach/mt_clkmgr.h>	/* ???? */
#include <mach/mt_irq.h>
#include <mach/sync_write.h>

#include "ddp_drv.h"
#include "ddp_reg.h"
#include "ddp_path.h"
#include "ddp_debug.h"
#include "ddp_color.h"
#include "ddp_bls.h"
#include <ddp_dpfd.h>
#include "disp_drv_ddp.h"

#include "ddp_rot.h"
#include "ddp_wdma.h"

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include <tz_cross/tz_ddp.h>
#include <mach/m4u_port.h>
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#endif

/* #include <asm/tcm.h> */
unsigned int dbg_log = 0;
unsigned int irq_log = 0;	/* must disable irq level log by default, else will block uart output, open it only for debug use */

#define DISP_DEVNAME "mtk_disp"
/* device and driver */
static dev_t disp_devno;
static struct cdev *disp_cdev;
static struct class *disp_class;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
/* the session to communicate with TA */
/* code shall access these session handles via [ddp_session_handle()] API */
static KREE_SESSION_HANDLE ddp_session;
static KREE_SESSION_HANDLE ddp_mem_session;	/* use [ddp_mem_session_handle()] */

KREE_SESSION_HANDLE ddp_session_handle(void);
KREE_SESSION_HANDLE ddp_mem_session_handle(void);
#endif

unsigned int gOvlSecure = 0;
unsigned int gOvlLayerSecure[4] = { 0, 0, 0, 0 };
unsigned int gOvlLayerSecureLast[4] = { 0, 0, 0, 0 };

unsigned int gMemOutSecure = 0;
unsigned int gBitbltSecure = 0;

/* used by IRQ from TEE to REE
  * NOTICE: must be consistent with defined in tz_ddp_hal.c
  */
#define MT8135_DISP_ROT_IRQ_BIT    0
#define MT8135_DISP_OVL_IRQ_BIT    1
#define MT8135_DISP_WDMA0_IRQ_BIT  2
#define MT8135_DISP_WDMA1_IRQ_BIT  3
#define MT8135_DISP_RDMA0_IRQ_BIT  4
#define MT8135_DISP_RDMA1_IRQ_BIT  5

struct MTEE_DISP_IRQ_STATUS {
	unsigned int ROT_IRQ_STATUS;
	unsigned int OVL_IRQ_STATUS;
	unsigned int WDMA0_IRQ_STATUS;
	unsigned int WDMA1_IRQ_STATUS;
	unsigned int RDMA0_IRQ_STATUS;
	unsigned int RDMA1_IRQ_STATUS;
};


/* irq */
#define DISP_REGISTER_IRQ(irq_num) {\
    if (request_irq(irq_num , (irq_handler_t)disp_irq_handler, IRQF_TRIGGER_LOW, DISP_DEVNAME , NULL))\
    { DISP_ERR("ddp register irq failed! %d\n", irq_num); } }

/* TODO: confirm irq un-register method with morris */
#define DISP_UNREGISTER_IRQ(irq_num) free_irq(irq_num , 0)

/* -------------------------------------------------------------------------------// */
/* global variables */
typedef struct {
	spinlock_t irq_lock;
	unsigned int irq_src;	/* one bit represent one module */
} disp_irq_struct;

typedef struct {
	pid_t open_pid;
	pid_t open_tgid;
	struct list_head testList;
	unsigned int u4LockedMutex;
	unsigned int u4LockedResource;
	unsigned int u4Clock;
	spinlock_t node_lock;
} disp_node_struct;

#define DISP_MAX_IRQ_CALLBACK   10
static DDP_IRQ_CALLBACK g_disp_irq_table[DISP_MODULE_MAX][DISP_MAX_IRQ_CALLBACK];

disp_irq_struct g_disp_irq;
static DECLARE_WAIT_QUEUE_HEAD(g_disp_irq_done_queue);
static DECLARE_WAIT_QUEUE_HEAD(gMutexWaitQueue);

/* cmdq thread */
#define CMDQ_THREAD_NUM 7
unsigned char cmdq_thread[CMDQ_THREAD_NUM] = { 1, 1, 1, 1, 1, 1, 1 };

spinlock_t gCmdqLock;

static wait_queue_head_t cmq_wait_queue[CMDQ_THREAD_NUM];
unsigned char cmq_status[CMDQ_THREAD_NUM];

/* Hardware Mutex Variables */
#define ENGINE_MUTEX_NUM 4
spinlock_t gMutexLock;
int mutex_used[ENGINE_MUTEX_NUM] = { 1, 0, 1, 1 };	/* 0 for FB, 1 for Bitblt, 2 for HDMI, 3 for BLS */

/* G2d Variables */
spinlock_t gResourceLock;
unsigned int gLockedResource;	/* lock dpEngineType_6589 */
static DECLARE_WAIT_QUEUE_HEAD(gResourceWaitQueue);

/* Overlay Variables */
spinlock_t gOvlLock;
int disp_run_dp_framework = 0;
int disp_layer_enable = 0;
int disp_mutex_status = 0;

DISP_OVL_INFO disp_layer_info[DDP_OVL_LAYER_MUN];

/* AAL variables */
static unsigned long u4UpdateFlag;

/* Register update lock */
spinlock_t gRegisterUpdateLock;
spinlock_t gPowerOperateLock;
/* Clock gate management */
static unsigned long g_u4ClockOnTbl;

/* PQ variables */
extern UINT32 fb_width;
extern UINT32 fb_height;
extern unsigned char aal_debug_flag;

/* IRQ log print kthread */
static struct task_struct *disp_irq_log_task;
static wait_queue_head_t disp_irq_log_wq;
static int disp_irq_log_module;

extern void DpEngine_COLORonConfig(unsigned int srcWidth, unsigned int srcHeight);
extern void DpEngine_COLORonInit(void);


#if 0
struct disp_path_config_struct {
	DISP_MODULE_ENUM srcModule;
	unsigned int addr;
	unsigned int inFormat;
	unsigned int pitch;
	struct DISP_REGION srcROI;	/* ROI */

	unsigned int layer;
	bool layer_en;
	enum OVL_LAYER_SOURCE source;
	struct DISP_REGION bgROI;	/* background ROI */
	unsigned int bgColor;	/* background color */
	unsigned int key;	/* color key */
	bool aen;		/* alpha enable */
	unsigned char alpha;

	DISP_MODULE_ENUM dstModule;
	unsigned int outFormat;
	unsigned int dstAddr;	/* only take effect when dstModule=DISP_MODULE_WDMA1 */
};
int disp_path_enable(void);
int disp_path_config(struct disp_path_config_struct *pConfig);
#endif

unsigned int *pRegBackup = NULL;

/* -------------------------------------------------------------------------------// */
/* functions */

static int disp_irq_log_kthread_func(void *data)
{
	unsigned int i = 0;
	while (1) {
		wait_event_interruptible(disp_irq_log_wq, disp_irq_log_module);
		DISP_DBG("disp_irq_log_kthread_func dump intr register: disp_irq_log_module=%d\n",
			 disp_irq_log_module);
		for (i = 0; i < DISP_MODULE_MAX; i++) {
			if ((disp_irq_log_module & (1 << i)) != 0) {
				disp_dump_reg(i);
			}
		}
		/* reset wakeup flag */
		disp_irq_log_module = 0;
	}

	return 0;
}

unsigned int disp_ms2jiffies(unsigned long ms)
{
	return ((ms * HZ + 512) >> 10);
}

int disp_lock_mutex(void)
{
	unsigned long flags;
	int id = -1;
	int i;

	spin_lock_irqsave(&gMutexLock, flags);

	for (i = 0; i < ENGINE_MUTEX_NUM; i++)
		if (mutex_used[i] == 0) {
			id = i;
			mutex_used[i] = 1;
			DISP_REG_SET_FIELD(REG_FLD(1, i), DISP_REG_CONFIG_MUTEX_INTEN, 1);
			DISP_REG_SET_FIELD(REG_FLD(1, i + 6), DISP_REG_CONFIG_MUTEX_INTEN, 1);
			break;
		}
	spin_unlock_irqrestore(&gMutexLock, flags);

	return id;
}

int disp_unlock_mutex(int id)
{
	unsigned long flags;

	if (id < 0 && id >= ENGINE_MUTEX_NUM)
		return -1;

	spin_lock_irqsave(&gMutexLock, flags);

	mutex_used[id] = 0;
	DISP_REG_SET_FIELD(REG_FLD(1, id), DISP_REG_CONFIG_MUTEX_INTEN, 0);
	DISP_REG_SET_FIELD(REG_FLD(1, id + 6), DISP_REG_CONFIG_MUTEX_INTEN, 0);

	spin_unlock_irqrestore(&gMutexLock, flags);

	return 0;
}

bool disp_update_mutex(void)
{
	int i;
	unsigned long flags;
	u32 val = 0, rval;

	spin_lock_irqsave(&gMutexLock, flags);
	for (i = 0; i < ENGINE_MUTEX_NUM; i++)
		val |= mutex_used[i] ? (((1 << 6) | (1 << 0)) << i) : 0;
	rval = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTEN);
	if (val != rval)
		DISP_REG_SET(DISP_REG_CONFIG_MUTEX_INTEN, val);
	spin_unlock_irqrestore(&gMutexLock, flags);
	return val != rval;
}

int disp_lock_cmdq_thread(void)
{
	int i = 0;

	pr_info("disp_lock_cmdq_thread()called\n");

	spin_lock(&gCmdqLock);
	for (i = 0; i < CMDQ_THREAD_NUM; i++) {
		if (cmdq_thread[i] == 1) {
			cmdq_thread[i] = 0;
			break;
		}
	}
	spin_unlock(&gCmdqLock);

	pr_info("disp_lock_cmdq_thread(), i=%d\n", i);

	return (i >= CMDQ_THREAD_NUM) ? -1 : i;

}

int disp_unlock_cmdq_thread(unsigned int idx)
{
	if (idx >= CMDQ_THREAD_NUM)
		return -1;

	spin_lock(&gCmdqLock);
	cmdq_thread[idx] = 1;	/* free thread availbility */
	spin_unlock(&gCmdqLock);

	return 0;
}


/* if return is not 0, should wait again */
int disp_wait_intr(DISP_MODULE_ENUM module, unsigned int timeout_ms)
{
	int ret;
	unsigned long flags;

	unsigned long long end_time = 0;
	unsigned long long start_time = sched_clock();

	MMProfileLogEx(DDP_MMP_Events.WAIT_INTR, MMProfileFlagStart, 0, module);
	/* wait until irq done or timeout */
	ret = wait_event_interruptible_timeout(g_disp_irq_done_queue,
					       g_disp_irq.irq_src & (1 << module),
					       disp_ms2jiffies(timeout_ms));

	/*wake-up from sleep */
	if (ret == 0)		/* timeout */
	{
		MMProfileLogEx(DDP_MMP_Events.WAIT_INTR, MMProfileFlagPulse, 0, module);
		MMProfileLog(DDP_MMP_Events.WAIT_INTR, MMProfileFlagEnd);
		DISP_ERR("Wait Done Timeout! pid=%d, module=%d\n", current->pid, module);
		if (module == DISP_MODULE_WDMA0) {
			pr_info("======== WDMA0 timeout, dump all registers!\n");
			disp_dump_reg(DISP_MODULE_CONFIG);
			disp_dump_reg(DISP_MODULE_ROT);
			disp_dump_reg(DISP_MODULE_SCL);
			disp_dump_reg(DISP_MODULE_WDMA0);
			disp_dump_reg(DISP_MODULE_SMI);

			if (gBitbltSecure) {
				MTEEC_PARAM param[4];
				unsigned int paramTypes;
				TZ_RESULT ret;

				param[0].value.a = 0;
				paramTypes = TZ_ParamTypes1(TZPT_VALUE_INPUT);
				ret =
				KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_MDP_RESET,
						paramTypes, param);
				if (ret != TZ_RESULT_SUCCESS)
					DISP_ERR("TZCMD_DDP_MDP_RESET fail, ret=%d\n", ret);
				ROTStop();
				ROTReset();
				WDMAStop(0);
				WDMAReset(0);
			}
		} else {
			disp_dump_reg(module);
		}
		disp_dump_reg(DISP_MODULE_CONFIG);

		return -EAGAIN;
	} else if (ret < 0)	/* intr by a signal */
	{
		MMProfileLogEx(DDP_MMP_Events.WAIT_INTR, MMProfileFlagPulse, 1, module);
		MMProfileLog(DDP_MMP_Events.WAIT_INTR, MMProfileFlagEnd);
		DISP_ERR("Wait Done interrupted by a signal! pid=%d, module=%d\n", current->pid,
			 module);
		disp_dump_reg(module);
		return -EAGAIN;
	}

	MMProfileLogEx(DDP_MMP_Events.WAIT_INTR, MMProfileFlagEnd, 0, module);
	spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
	g_disp_irq.irq_src &= ~(1 << module);
	spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);

	end_time = sched_clock();
	/* DISP_MSG("**ROT_SCL_WDMA0 execute %d us\n", ((unsigned int)end_time-(unsigned int)start_time)/1000); */

	/* after enable new tile mode flow, this time can be very short */
	if (0)			/* module==DISP_MODULE_WDMA0 && (end_time-start_time)/1000<50) //less than 0.05ms */
	{
		DISP_ERR("ROT_SCL_WDMA0 path too fast! %d us\n",
			 ((unsigned int)end_time - (unsigned int)start_time) / 1000);
		DISP_MSG("DISP_REG_ROT_SRC_BASE_0: 0x%x\n", DISP_REG_GET(DISP_REG_ROT_SRC_BASE_0));
		DISP_MSG("DISP_REG_ROT_SRC_BASE_1: 0x%x\n", DISP_REG_GET(DISP_REG_ROT_SRC_BASE_1));
		DISP_MSG("DISP_REG_ROT_SRC_BASE_2: 0x%x\n", DISP_REG_GET(DISP_REG_ROT_SRC_BASE_2));
		DISP_MSG("DISP_REG_ROT_MF_SRC_SIZE: 0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MF_SRC_SIZE));
		DISP_MSG("DISP_REG_ROT_MF_CLIP_SIZE: 0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MF_CLIP_SIZE));
		DISP_MSG("DISP_REG_ROT_EN: 0x%x\n", DISP_REG_GET(DISP_REG_ROT_EN));
		DISP_MSG("DISP_REG_ROT_SRC_CON: 0x%x\n", DISP_REG_GET(DISP_REG_ROT_SRC_CON));
		DISP_MSG("DISP_REG_ROT_CON: 0x%x\n", DISP_REG_GET(DISP_REG_ROT_CON));

		DISP_MSG("DISP_REG_WDMA_DST_ADDR: 0x%x\n", DISP_REG_GET(DISP_REG_WDMA_DST_ADDR));
		DISP_MSG("DISP_REG_WDMA_DST_U_ADDR: 0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_DST_U_ADDR));
		DISP_MSG("DISP_REG_WDMA_DST_UV_PITCH: 0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_DST_UV_PITCH));
		DISP_MSG("DISP_REG_WDMA_SRC_SIZE: 0x%x\n", DISP_REG_GET(DISP_REG_WDMA_SRC_SIZE));
		DISP_MSG("DISP_REG_WDMA_CLIP_SIZE: 0x%x\n", DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE));
		DISP_MSG("DISP_REG_WDMA_EN: 0x%x\n", DISP_REG_GET(DISP_REG_WDMA_EN));
		DISP_MSG("DISP_REG_WDMA_CFG: 0x%x\n", DISP_REG_GET(DISP_REG_WDMA_CFG));
	}

	return 0;
}

int disp_register_irq(DISP_MODULE_ENUM module, DDP_IRQ_CALLBACK cb)
{
	int i;
	if (module >= DISP_MODULE_MAX) {
		DISP_ERR("Register IRQ with invalid module ID. module=%d\n", module);
		return -1;
	}
	if (cb == NULL) {
		DISP_ERR("Register IRQ with invalid cb.\n");
		return -1;
	}
	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (g_disp_irq_table[module][i] == cb)
			break;
	}
	if (i < DISP_MAX_IRQ_CALLBACK) {
		/* Already registered. */
		return 0;
	}
	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (g_disp_irq_table[module][i] == NULL)
			break;
	}
	if (i == DISP_MAX_IRQ_CALLBACK) {
		DISP_ERR("No enough callback entries for module %d.\n", module);
		return -1;
	}
	g_disp_irq_table[module][i] = cb;
	return 0;
}

int disp_unregister_irq(DISP_MODULE_ENUM module, DDP_IRQ_CALLBACK cb)
{
	int i;
	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (g_disp_irq_table[module][i] == cb) {
			g_disp_irq_table[module][i] = NULL;
			break;
		}
	}
	if (i == DISP_MAX_IRQ_CALLBACK) {
		DISP_ERR
		    ("Try to unregister callback function with was not registered. module=%d cb=0x%08X\n",
		     module, (unsigned int)cb);
		return -1;
	}
	return 0;
}


int disp_is_cb_registered(DISP_MODULE_ENUM module, DDP_IRQ_CALLBACK cb)
{
	int i;
	int ret = 0;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (g_disp_irq_table[module][i] == cb) {
			ret = 1;
			break;
		}
	}

	if (i == DISP_MAX_IRQ_CALLBACK) {
		ret = 0;
	}

	return ret;
}

void disp_invoke_irq_callbacks(DISP_MODULE_ENUM module, unsigned int param)
{
	int i;
	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (g_disp_irq_table[module][i]) {
			/* DISP_ERR("Invoke callback function. module=%d param=0x%X\n", module, param); */
			g_disp_irq_table[module][i] (param);
		}
	}
}

#if defined(CONFIG_MTK_HDMI_SUPPORT)
extern void hdmi_setorientation(int orientation);
void hdmi_power_on(void);
void hdmi_power_off(void);
extern void hdmi_update_buffer_switch(void);
extern bool is_hdmi_active(void);
extern void hdmi_update(void);
extern void hdmi_source_buffer_switch(void);
extern void hdmi_wdma1_done(void);
extern void hdmi_rdma1_done(void);
#endif

/*#if  defined(CONFIG_MTK_WFD_SUPPORT)
extern void wfd_setorientation(int orientation);
void hdmi_power_on(void);
void hdmi_power_off(void);
extern void wfd_update_buffer_switch(void);
extern bool is_wfd_active(void);
extern void wfd_update(void);
extern void wfd_source_buffer_switch(void);
#endif*/

/* extern void hdmi_test_switch_buffer(void); */

unsigned int cnt_rdma_underflow = 1;
unsigned int cnt_rdma_abnormal = 1;
unsigned int cnt_ovl_underflow = 1;
unsigned int cnt_ovl_eof = 1;
static /*__tcmfunc*/ irqreturn_t disp_irq_handler(int irq, void *dev_id)
{
	unsigned long reg_val;
	unsigned long flags;
	int i;
	DISP_IRQ("irq=%d, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		 irq,
		 DISP_REG_GET(DISP_REG_SCL_INTSTA),
		 DISP_REG_GET(DISP_REG_ROT_INTERRUPT_STATUS),
		 DISP_REG_GET(DISP_REG_OVL_INTSTA),
		 DISP_REG_GET(DISP_REG_WDMA_INTSTA),
		 /* DISP_REG_GET(DISP_REG_WDMA_INTSTA + 0x1000), */
		 DISP_REG_GET(DISP_REG_RDMA_INT_STATUS),
		 DISP_REG_GET(DISP_REG_RDMA_INT_STATUS + 0x1000));


	/*1. Process ISR */
	switch (irq) {
	case MT8135_DISP_SCL_IRQ_ID:
		reg_val = DISP_REG_GET(DISP_REG_SCL_INTSTA);
		if (reg_val & (1 << 0)) {
			DISP_IRQ("IRQ: SCL input frame done!\n");
		}
		if (reg_val & (1 << 1)) {
			DISP_IRQ("IRQ: SCL output frame done!\n");
			spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
			g_disp_irq.irq_src |= (1 << DISP_MODULE_SCL);
			spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
		}
		/* clear intr */
		DISP_REG_SET(DISP_REG_SCL_INTSTA, ~reg_val);
		MMProfileLogEx(DDP_MMP_Events.SCL_IRQ, MMProfileFlagPulse, reg_val, 0);
		disp_invoke_irq_callbacks(DISP_MODULE_SCL, reg_val);
		break;

	case MT8135_DISP_ROT_IRQ_ID:
		reg_val = DISP_REG_GET(DISP_REG_ROT_INTERRUPT_STATUS);
		if (reg_val & (1 << 0)) {
			DISP_IRQ("IRQ: ROT frame done!\n");
			spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
			g_disp_irq.irq_src |= (1 << DISP_MODULE_ROT);
			spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
		}
		if (reg_val & (1 << 1)) {
			DISP_IRQ("IRQ: ROT reg update done!\n");
		}
		if (reg_val & (1 << 2)) {
			DISP_ERR("IRQ: ROT underrun!\n");
		}
		/* clear intr */
		DISP_REG_SET(DISP_REG_ROT_INTERRUPT_STATUS, ~reg_val);
		MMProfileLogEx(DDP_MMP_Events.ROT_IRQ, MMProfileFlagPulse, reg_val, 0);
		disp_invoke_irq_callbacks(DISP_MODULE_ROT, reg_val);
		break;

	case MT8135_DISP_OVL_IRQ_ID:
		reg_val = DISP_REG_GET(DISP_REG_OVL_INTSTA);
		if (reg_val & (1 << 0)) {
			DISP_IRQ("IRQ: OVL reg update done!\n");
		}
		if (reg_val & (1 << 1)) {
			DISP_IRQ("IRQ: OVL frame done!\n");
			spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
			g_disp_irq.irq_src |= (1 << DISP_MODULE_OVL);
			spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
		}
		if (reg_val & (1 << 2)) {
			DISP_ERR("IRQ: OVL frame underrun! M0_mod= 0x%x, M0_SOF=0x%x,M4_mod=0x%x,M4_SOF=0x%x\n",
				DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(0)),
				DISP_REG_GET(DISP_REG_CONFIG_MUTEX_SOF(0)),
				DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(4)),
				DISP_REG_GET(DISP_REG_CONFIG_MUTEX_SOF(4)));
			DISP_ERR("CG0 = 0x%x,CG1=0x%x", DISP_REG_GET(DISP_REG_CONFIG_CG_CON0),
				DISP_REG_GET(DISP_REG_CONFIG_CG_CON1));
		}
		if (reg_val & (1 << 3)) {
			DISP_IRQ("IRQ: OVL SW reset done!\n");
		}
		if (reg_val & (1 << 4)) {
			DISP_IRQ("IRQ: OVL HW reset done!\n");
		}
		if (reg_val & (1 << 5)) {
			DISP_ERR("IRQ: OVL-RDMA0 not complete untill EOF!\n");
		}
		if (reg_val & (1 << 6)) {
			DISP_ERR("IRQ: OVL-RDMA1 not complete untill EOF!\n");
		}
		if (reg_val & (1 << 7)) {
			DISP_ERR("IRQ: OVL-RDMA2 not complete untill EOF!\n");
		}
		if (reg_val & (1 << 8)) {
			DISP_ERR("IRQ: OVL-RDMA3 not complete untill EOF!\n");
		}
		if (reg_val & (1 << 9)) {
			DISP_ERR("IRQ: OVL-RDMA0 fifo underflow!\n");
		}
		if (reg_val & (1 << 10)) {
			DISP_ERR("IRQ: OVL-RDMA1 fifo underflow!\n");
		}
		if (reg_val & (1 << 11)) {
			DISP_ERR("IRQ: OVL-RDMA2 fifo underflow!\n");
		}
		if (reg_val & (1 << 12)) {
			DISP_ERR("IRQ: OVL-RDMA3 fifo underflow!\n");
		}
		/* clear intr */
		DISP_REG_SET(DISP_REG_OVL_INTSTA, ~reg_val);
		MMProfileLogEx(DDP_MMP_Events.OVL_IRQ, MMProfileFlagPulse, reg_val, 0);
		disp_invoke_irq_callbacks(DISP_MODULE_OVL, reg_val);
		break;

	case MT8135_DISP_WDMA0_IRQ_ID:
		reg_val = DISP_REG_GET(DISP_REG_WDMA_INTSTA);
		if (reg_val & (1 << 0)) {
			DISP_IRQ("IRQ: WDMA0 frame done!\n");
			spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
			g_disp_irq.irq_src |= (1 << DISP_MODULE_WDMA0);
			spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
		}
		if (reg_val & (1 << 1)) {
			DISP_ERR("IRQ: WDMA0 underrun!\n");

		}
		/* clear intr */
		DISP_REG_SET(DISP_REG_WDMA_INTSTA, ~reg_val);
		MMProfileLogEx(DDP_MMP_Events.WDMA0_IRQ, MMProfileFlagPulse, reg_val,
			       DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE));
		disp_invoke_irq_callbacks(DISP_MODULE_WDMA0, reg_val);
		break;

	case MT8135_DISP_WDMA1_IRQ_ID:
		reg_val = DISP_REG_GET(DISP_REG_WDMA_INTSTA + 0x1000);
		if (reg_val & (1 << 0)) {
			DISP_IRQ("IRQ: WDMA1 frame done!\n");
			spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
			g_disp_irq.irq_src |= (1 << DISP_MODULE_WDMA1);
			spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
#if defined(CONFIG_MTK_HDMI_SUPPORT)
			if (!DISP_IsVideoMode())
				hdmi_source_buffer_switch();

			if (is_hdmi_active()) {
				hdmi_wdma1_done();
				hdmi_update();
			}
#endif
/*#if defined(CONFIG_MTK_WFD_SUPPORT)
					if(!DISP_IsVideoMode())
					    wfd_source_buffer_switch();

					if(is_wfd_active())
					{
					    wfd_update();
					}
#endif*/
			/* hdmi_test_switch_buffer(); */

		}
		if (reg_val & (1 << 1)) {
			DISP_ERR("IRQ: WDMA1 underrun!\n");

		}
		/* clear intr */
		DISP_REG_SET(DISP_REG_WDMA_INTSTA + 0x1000, ~reg_val);
		MMProfileLogEx(DDP_MMP_Events.WDMA1_IRQ, MMProfileFlagPulse, reg_val,
			       DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE + 0x1000));
		disp_invoke_irq_callbacks(DISP_MODULE_WDMA1, reg_val);
		break;

	case MT8135_DISP_RDMA0_IRQ_ID:
		reg_val = DISP_REG_GET(DISP_REG_RDMA_INT_STATUS);
		if (reg_val & (1 << 0)) {
			DISP_IRQ("IRQ: RDMA0 reg update done!\n");
		}
		if (reg_val & (1 << 1)) {
			DISP_IRQ("IRQ: RDMA0 frame start!\n");
			if (disp_needWakeUp()) {
				/*disp_update_hist();*/
				disp_wakeup_aal();
			}
			on_disp_aal_alarm_set();
		}
		if (reg_val & (1 << 2)) {
			DISP_IRQ("IRQ: RDMA0 frame done!\n");
			spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
			g_disp_irq.irq_src |= (1 << DISP_MODULE_RDMA0);
			spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
		}
		if (reg_val & (1 << 3)) {
			if (cnt_rdma_abnormal < 100) {
				DISP_ERR("IRQ: RDMA0 abnormal! %d times\n", cnt_rdma_abnormal);
				cnt_rdma_abnormal++;
			}
		}
		if (reg_val & (1 << 4)) {
			if (cnt_rdma_underflow < 100) {
				DISP_ERR("IRQ: RDMA0 underflow!%d times\n", cnt_rdma_underflow);
				cnt_rdma_underflow++;
			}
		}
		/* clear intr */
		DISP_REG_SET(DISP_REG_RDMA_INT_STATUS, ~reg_val);
		MMProfileLogEx(DDP_MMP_Events.RDMA0_IRQ, MMProfileFlagPulse, reg_val, 0);
		disp_invoke_irq_callbacks(DISP_MODULE_RDMA0, reg_val);
		break;

	case MT8135_DISP_RDMA1_IRQ_ID:
		reg_val = DISP_REG_GET(DISP_REG_RDMA_INT_STATUS + 0x1000);
		if (reg_val & (1 << 0)) {
			DISP_IRQ("IRQ: RDMA1 reg update done!\n");
		}
		if (reg_val & (1 << 1)) {
			DISP_IRQ("IRQ: RDMA1 frame start!\n");
		}
		if (reg_val & (1 << 2)) {
			DISP_IRQ("IRQ: RDMA1 frame done!\n");
			spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
			g_disp_irq.irq_src |= (1 << DISP_MODULE_RDMA1);
			spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);

#if defined(CONFIG_MTK_HDMI_SUPPORT)
			if (is_hdmi_active() && DISP_IsVideoMode()) {
				hdmi_rdma1_done();
			}
#endif

		}
		if (reg_val & (1 << 3)) {
			DISP_ERR("IRQ: RDMA1 abnormal!\n");
		}
		if (reg_val & (1 << 4)) {
			DISP_ERR("IRQ: RDM1A underflow!\n");
		}
		/* clear intr */
		DISP_REG_SET(DISP_REG_RDMA_INT_STATUS + 0x1000, ~reg_val);
		MMProfileLogEx(DDP_MMP_Events.RDMA1_IRQ, MMProfileFlagPulse, reg_val, 0);
		disp_invoke_irq_callbacks(DISP_MODULE_RDMA1, reg_val);
		break;

	case MT8135_DISP_COLOR_IRQ_ID:
		reg_val = DISP_REG_GET(DISPSYS_COLOR_BASE + 0x0F08);

		/* read LUMA histogram */
		if (reg_val & 0x2) {
/* TODO : might want to move to other IRQ~ -S */
			/* disp_update_hist(); */
			/* disp_wakeup_aal(); */
/* TODO : might want to move to other IRQ~ -E */
		}
		/* clear intr */
		DISP_REG_SET(DISPSYS_COLOR_BASE + 0x0F08, ~reg_val);
		MMProfileLogEx(DDP_MMP_Events.COLOR_IRQ, MMProfileFlagPulse, reg_val, 0);
/* disp_invoke_irq_callbacks(DISP_MODULE_COLOR, reg_val); */
		break;

	case MT8135_DISP_BLS_IRQ_ID:
		reg_val = DISP_REG_GET(DISP_REG_BLS_INTSTA);

		/* read LUMA & MAX(R,G,B) histogram */
		if (reg_val & 0x1) {
			/* disp_update_hist(); */
			/* disp_wakeup_aal(); */
		}
		/* clear intr */
		DISP_REG_SET(DISP_REG_BLS_INTSTA, ~reg_val);
		MMProfileLogEx(DDP_MMP_Events.BLS_IRQ, MMProfileFlagPulse, reg_val, 0);
		break;

	case MT8135_DISP_TDSHP_IRQ_ID:
		/* DISP_REG_SET(DISPSYS_TDSHP_BASE, 0); */
		DISP_IRQ("tdshp irq\n ");
		/* disp_invoke_irq_callbacks(DISP_MODULE_TDSHP, 0); */
		MMProfileLogEx(DDP_MMP_Events.TDSHP_IRQ, MMProfileFlagPulse, 0, 0);
		break;

	case MT8135_DISP_CMDQ_IRQ_ID:
		for (i = 0; i < CMDQ_THREAD_NUM; i++) {
			reg_val = DISP_REG_GET(DISP_REG_CMDQ_THRx_IRQ_FLAG(i));
			if (reg_val != 0) {
				DISP_IRQ("CMQ Thread %d IRQ %lu", i, reg_val);
				DISP_REG_SET(DISP_REG_CMDQ_THRx_IRQ_FLAG(i), ~reg_val);
				if (reg_val & (1 << 1))
					DISP_ERR("IRQ: CMQ %d Time out!\n", i);
				else if (reg_val & (1 << 4))
					DISP_ERR
					    ("IRQ: CMQ thread%d Invalid Command Instruction!\n",
					     i);
				disp_invoke_irq_callbacks(DISP_MODULE_CMDQ, reg_val);
				cmq_status[i] = 0;
				mb();
				wake_up_interruptible(&cmq_wait_queue[i]);
				MMProfileLogEx(DDP_MMP_Events.CMDQ_IRQ, MMProfileFlagPulse, reg_val,
					       i);
			}
		}
		return IRQ_HANDLED;
	case MT8135_DISP_MUTEX_IRQ_ID:	/* can not do reg update done status after release mutex(for ECO requirement), */
		/* so we have to check update timeout intr here */
		reg_val = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA);

		if (reg_val & 0x3cf)	/* udpate timeout intr triggered */
		{
			unsigned int reg = 0;
			unsigned int mutexID = 0;

			for (mutexID = 0; mutexID < 4; mutexID++) {
				if ((DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA) &
				     (1 << (mutexID + 6))) == (1 << (mutexID + 6))) {
					DISP_ERR("disp_path_release_mutex() timeout!\n");
					disp_irq_log_module |= (1 << DISP_MODULE_CONFIG);

					/* print error engine */
					reg = DISP_REG_GET(DISP_REG_CONFIG_REG_COMMIT);
					if (reg != 0) {
						if (reg & (1 << 0)) {
							DISP_ERR(" ROT update reg timeout!\n");
							disp_irq_log_module |=
							    (1 << DISP_MODULE_ROT);
						}
						if (reg & (1 << 1)) {
							DISP_ERR(" SCL update reg timeout!\n");
							disp_irq_log_module |=
							    (1 << DISP_MODULE_SCL);
						}
						if (reg & (1 << 2)) {
							DISP_ERR(" OVL update reg timeout!\n");
							disp_irq_log_module |=
							    (1 << DISP_MODULE_OVL);
						}
						if (reg & (1 << 3)) {
							DISP_ERR(" COLOR update reg timeout!\n");
							disp_irq_log_module |=
							    (1 << DISP_MODULE_COLOR);
						}
						if (reg & (1 << 4)) {
							DISP_ERR(" 2D_SHARP update reg timeout!\n");
							disp_irq_log_module |=
							    (1 << DISP_MODULE_TDSHP);
						}
						if (reg & (1 << 5)) {
							DISP_ERR(" WDMA0 update reg timeout!\n");
							disp_irq_log_module |=
							    (1 << DISP_MODULE_WDMA0);
						}
						if (reg & (1 << 6)) {
							DISP_ERR(" WDMA1 update reg timeout!\n");
							disp_irq_log_module |=
							    (1 << DISP_MODULE_WDMA1);
						}
						if (reg & (1 << 7)) {
							DISP_ERR(" RDMA0 update reg timeout!\n");
							disp_irq_log_module |=
							    (1 << DISP_MODULE_RDMA0);
						}
						if (reg & (1 << 8)) {
							DISP_ERR(" RDMA1 update reg timeout!\n");
							disp_irq_log_module |=
							    (1 << DISP_MODULE_RDMA1);
						}
						if (reg & (1 << 9)) {
							DISP_ERR(" BLS update reg timeout!\n");
							disp_irq_log_module |=
							    (1 << DISP_MODULE_BLS);
						}
						if (reg & (1 << 10)) {
							DISP_ERR(" GAMMA update reg timeout!\n");
							disp_irq_log_module |=
							    (1 << DISP_MODULE_GAMMA);
						}
					}
					/* prink smi related register */
					disp_irq_log_module |= (1 << DISP_MODULE_SMI);

					/* reset mutex */
					DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(mutexID), 1);
					DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(mutexID), 0);
					DISP_DBG("mutex reset done!\n");
				} else if ((DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA) & (1 << mutexID)) == (1 << mutexID))	/* mutex  update done */
				{
					spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
					g_disp_irq.irq_src |= (1 << (DISP_MODULE_MUTEX0 + mutexID));
					spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
					/* DISP_DBG("Mutex %d done!\n", mutexID); */
				}
			}
		}


		DISP_REG_SET(DISP_REG_CONFIG_MUTEX_INTSTA, ~reg_val);
		MMProfileLogEx(DDP_MMP_Events.Mutex_IRQ, MMProfileFlagPulse, reg_val, 0);
		disp_invoke_irq_callbacks(DISP_MODULE_MUTEX, reg_val);
		break;
	case MT_G2D_IRQ_ID:
		reg_val = DISP_REG_GET(DISP_REG_G2D_IRQ);
		if (reg_val & G2D_IRQ_STA_BIT) {
			unsigned long set_val = reg_val & ~(G2D_IRQ_STA_BIT);
			DISP_IRQ("IRQ: G2D done!\n");
			spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
			g_disp_irq.irq_src |= (1 << DISP_MODULE_G2D);
			spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
			/* clear intr */
			DISP_REG_SET(DISP_REG_G2D_IRQ, set_val);
		}

		disp_invoke_irq_callbacks(DISP_MODULE_G2D, reg_val);
		break;
	default:
		DISP_ERR("invalid irq=%d\n ", irq);
		break;
	}

	/* Wakeup event */
	mb();			/* Add memory barrier before the other CPU (may) wakeup */
	wake_up_interruptible(&g_disp_irq_done_queue);

	/* dump register if error happened */
	if (disp_irq_log_module != 0) {
		wake_up_interruptible(&disp_irq_log_wq);
	}
#if 0
	if (DISP_REG_GET(DISP_REG_SCL_INTSTA) != 0)
		DISP_REG_SET(DISP_REG_SCL_INTSTA, 0);

	if (DISP_REG_GET(DISP_REG_ROT_INTERRUPT_STATUS) != 0)
		DISP_REG_SET(DISP_REG_ROT_INTERRUPT_STATUS, 0);

	if (DISP_REG_GET(DISP_REG_OVL_INTSTA) != 0)
		DISP_REG_SET(DISP_REG_OVL_INTSTA, 0);

	if (DISP_REG_GET(DISP_REG_WDMA_INTSTA) != 0)
		DISP_REG_SET(DISP_REG_WDMA_INTSTA, 0);

	if (DISP_REG_GET(DISP_REG_WDMA_INTSTA + 0x1000) != 0)
		DISP_REG_SET(DISP_REG_WDMA_INTSTA + 0x1000, 0);

	if (DISP_REG_GET(DISP_REG_RDMA_INT_STATUS) != 0)
		DISP_REG_SET(DISP_REG_RDMA_INT_STATUS, 0);

	if (DISP_REG_GET(DISP_REG_RDMA_INT_STATUS + 0x1000) != 0)
		DISP_REG_SET(DISP_REG_RDMA_INT_STATUS + 0x1000, 0);
#endif


	return IRQ_HANDLED;
}


void disp_power_on(DISP_MODULE_ENUM eModule, unsigned int *pu4Record)
{
	unsigned long flag;
	unsigned int ret = 0;
	spin_lock_irqsave(&gPowerOperateLock, flag);


	if ((1 << eModule) & g_u4ClockOnTbl) {
		DISP_MSG("DDP power %lu is already enabled\n", (unsigned long)eModule);
	} else {
		switch (eModule) {
		case DISP_MODULE_ROT:
			enable_clock(MT_CG_DISP_ROT_DISP, "DDP_DRV");
			enable_clock(MT_CG_DISP_ROT_SMI, "DDP_DRV");
			break;
		case DISP_MODULE_SCL:
			enable_clock(MT_CG_DISP_SCL_DISP, "DDP_DRV");
			break;
		case DISP_MODULE_WDMA0:
			enable_clock(MT_CG_DISP_WDMA0_DISP, "DDP_DRV");
			enable_clock(MT_CG_DISP_WDMA0_SMI, "DDP_DRV");
			break;
		case DISP_MODULE_TDSHP:
			enable_clock(MT_CG_DISP_TDSHP_DISP, "DDP_DRV");
			break;
		case DISP_MODULE_G2D:
			enable_clock(MT_CG_DISP_G2D_DISP, "DDP_DRV");
			enable_clock(MT_CG_DISP_G2D_SMI, "DDP_DRV");
			break;
		default:
			DISP_ERR("disp_power_on:unknown module:%d\n", eModule);
			ret = -1;
			break;
		}

		if (0 == ret) {
			if (0 == g_u4ClockOnTbl) {
				enable_clock(MT_CG_DISP_SMI_LARB2, "DDP_DRV");
			}
			g_u4ClockOnTbl |= (1 << eModule);
			*pu4Record |= (1 << eModule);
		}
	}

	/* restore mutex intr_en reg, because this reg will be reset if power is off */
	/* but rot->scl>wdma path may still work in early suspend mode, need this reg value */
	disp_update_mutex();

	spin_unlock_irqrestore(&gPowerOperateLock, flag);
}

void disp_power_off(DISP_MODULE_ENUM eModule, unsigned int *pu4Record)
{
	unsigned long flag;
	unsigned int ret = 0;
	spin_lock_irqsave(&gPowerOperateLock, flag);

	DISP_DBG("power off : %d\n" , eModule);

	if ((1 << eModule) & g_u4ClockOnTbl) {
		switch (eModule) {
		case DISP_MODULE_ROT:
			ROTStop();
			ROTReset();
			disable_clock(MT_CG_DISP_ROT_SMI, "DDP_DRV");
			disable_clock(MT_CG_DISP_ROT_DISP, "DDP_DRV");
			break;
		case DISP_MODULE_SCL:
			disable_clock(MT_CG_DISP_SCL_DISP, "DDP_DRV");
			break;
		case DISP_MODULE_WDMA0:
			WDMAStop(0);
			WDMAReset(0);
			disable_clock(MT_CG_DISP_WDMA0_DISP, "DDP_DRV");
			disable_clock(MT_CG_DISP_WDMA0_SMI, "DDP_DRV");
			break;
		case DISP_MODULE_TDSHP:
			disable_clock(MT_CG_DISP_TDSHP_DISP, "DDP_DRV");
			break;
		case DISP_MODULE_G2D:
			disable_clock(MT_CG_DISP_G2D_DISP, "DDP_DRV");
			disable_clock(MT_CG_DISP_G2D_SMI, "DDP_DRV");
			break;
		default:
			DISP_ERR("disp_power_off:unsupported format:%d\n", eModule);
			ret = -1;
			break;
		}

		if (0 == ret) {
			g_u4ClockOnTbl &= (~(1 << eModule));
			*pu4Record &= (~(1 << eModule));

			if (0 == g_u4ClockOnTbl) {
				disable_clock(MT_CG_DISP_SMI_LARB2, "DDP_DRV");
			}

		}
	} else {
		DISP_ERR("DDP power %lu is already disabled\n", (unsigned long)eModule);
	}


	spin_unlock_irqrestore(&gPowerOperateLock, flag);
}

int disp_module_clock_on(DISP_MODULE_ENUM module, char *caller_name)
{
	switch (module) {
	case DISP_MODULE_OVL:
		enable_clock(MT_CG_DISP_OVL_DISP, caller_name);
		enable_clock(MT_CG_DISP_OVL_SMI, caller_name);
		break;
	case DISP_MODULE_WDMA1:
		enable_clock(MT_CG_DISP_WDMA1_DISP, caller_name);
		enable_clock(MT_CG_DISP_WDMA1_SMI, caller_name);
		break;
	case DISP_MODULE_RDMA1:
		enable_clock(MT_CG_DISP_RDMA1_DISP, caller_name);
		enable_clock(MT_CG_DISP_RDMA1_SMI, caller_name);
		enable_clock(MT_CG_DISP_RDMA1_OUTPUT, caller_name);
		break;
	case DISP_MODULE_GAMMA:
		enable_clock(MT_CG_DISP_GAMMA_DISP, caller_name);
		enable_clock(MT_CG_DISP_GAMMA_PIXEL, caller_name);
		break;
	case DISP_MODULE_SMI:
		enable_clock(MT_CG_DISP_SMI_LARB2, caller_name);
		break;
	default:
		DISP_ERR("disp_module_clock_on, unknow module=%d\n", module);
	}

	return 0;
}

int disp_module_clock_off(DISP_MODULE_ENUM module, char *caller_name)
{
	switch (module) {
	case DISP_MODULE_OVL:
		disable_clock(MT_CG_DISP_OVL_DISP, caller_name);
		disable_clock(MT_CG_DISP_OVL_SMI, caller_name);
		break;
	case DISP_MODULE_WDMA1:
		disable_clock(MT_CG_DISP_WDMA1_DISP, caller_name);
		disable_clock(MT_CG_DISP_WDMA1_SMI, caller_name);
		break;
	case DISP_MODULE_RDMA1:
		disable_clock(MT_CG_DISP_RDMA1_DISP, caller_name);
		disable_clock(MT_CG_DISP_RDMA1_SMI, caller_name);
		disable_clock(MT_CG_DISP_RDMA1_OUTPUT, caller_name);
		break;
	case DISP_MODULE_GAMMA:
		disable_clock(MT_CG_DISP_GAMMA_DISP, caller_name);
		disable_clock(MT_CG_DISP_GAMMA_PIXEL, caller_name);
		break;
	case DISP_MODULE_SMI:
		disable_clock(MT_CG_DISP_SMI_LARB2, caller_name);
		break;
	default:
		DISP_ERR("disp_module_clock_off, unknow module=%d\n", module);
	}

	return 0;
}


unsigned int inAddr = 0, outAddr = 0;

int disp_set_needupdate(DISP_MODULE_ENUM eModule, unsigned long u4En)
{
	unsigned long flag;
	spin_lock_irqsave(&gRegisterUpdateLock, flag);

	if (u4En) {
		u4UpdateFlag |= (1 << eModule);
	} else {
		u4UpdateFlag &= ~(1 << eModule);
	}

	spin_unlock_irqrestore(&gRegisterUpdateLock, flag);

	return 0;
}

void DISP_REG_SET_FIELD(unsigned long field, unsigned long reg32, unsigned long val)
{
	unsigned long flag;
	spin_lock_irqsave(&gRegisterUpdateLock, flag);
	/* *(volatile unsigned int*)(reg32) = ((*(volatile unsigned int*)(reg32) & ~(REG_FLD_MASK(field))) |  REG_FLD_VAL((field), (val))); */
	mt65xx_reg_sync_writel((*(volatile unsigned int *)(reg32) & ~(REG_FLD_MASK(field))) |
			       REG_FLD_VAL((field), (val)), reg32);
	spin_unlock_irqrestore(&gRegisterUpdateLock, flag);
}

int CheckAALUpdateFunc(int i4IsNewFrame)
{
	return (((1 << DISP_MODULE_BLS) & u4UpdateFlag) || i4IsNewFrame
		|| is_disp_aal_alarm_on()) ? 1 : 0;
}

int ConfAALFunc(int i4IsNewFrame)
{
	disp_onConfig_aal(i4IsNewFrame);
	disp_set_needupdate(DISP_MODULE_BLS, 0);
	return 0;
}

static int AAL_init;
void disp_aal_lock(void)
{
	if (0 == AAL_init) {
		/* pr_info("disp_aal_lock: register update func\n"); */
		DISP_RegisterExTriggerSource(CheckAALUpdateFunc, ConfAALFunc);
		AAL_init = 1;
	}
	GetUpdateMutex();
}

void disp_aal_unlock(void)
{
	ReleaseUpdateMutex();
	disp_set_needupdate(DISP_MODULE_BLS, 1);
}

int CheckColorUpdateFunc(int i4NotUsed)
{
	return ((1 << DISP_MODULE_COLOR) & u4UpdateFlag) ? 1 : 0;
}

int ConfColorFunc(int i4NotUsed)
{
	DpEngine_COLORonInit();
	DpEngine_COLORonConfig(fb_width, fb_height);

	disp_set_needupdate(DISP_MODULE_COLOR, 0);
	return 0;
}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
static struct task_struct *disp_secure_irq_task;
static int disp_secure_intr_callback(void *data)
{
	MTEEC_PARAM param[4];
	unsigned int paramTypes;
	TZ_RESULT ret;
	unsigned int irq;
	static unsigned int ree_wdma0_cnt;
	static unsigned int ree_rdma0_cnt;
	unsigned long flags;
	/* static unsigned int ree_ovl_cnt = 0; */

	struct MTEE_DISP_IRQ_STATUS IrqStatus;
	struct MTEE_DISP_IRQ_STATUS *pIrqStatus = &IrqStatus;

	KREE_SHAREDMEM_HANDLE p_IRQ_status_share_handle = 0;
	KREE_SHAREDMEM_PARAM  sharedParam;
	/* pr_err("[02420] in callback before TEE calls"); */
	/* Register shared memory */
	sharedParam.buffer = pIrqStatus;
	sharedParam.size = sizeof(struct MTEE_DISP_IRQ_STATUS);
	ret =
	    KREE_RegisterSharedmem(ddp_mem_session_handle(), &p_IRQ_status_share_handle,
				   &sharedParam);
	if (ret != TZ_RESULT_SUCCESS) {
		DISP_ERR("disp_register_share_memory Error: %d, line:%d, ddp_mem_session(%d)", ret,
			 __LINE__, (unsigned int)ddp_mem_session_handle());
		return 0;
	}

	param[0].value.a = 0;
	param[1].value.a = 0;	/* intr type */
	param[2].value.a = 0;
	param[3].value.a = 0;
	paramTypes =
	    TZ_ParamTypes4(TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT, TZPT_MEMREF_OUTPUT,
			   TZPT_VALUE_OUTPUT);

	/* set up param[2] data type */
	param[2].memref.handle = (uint32_t) p_IRQ_status_share_handle;
	param[2].memref.offset = 0;
	param[2].memref.size = sizeof(struct MTEE_DISP_IRQ_STATUS);
	DISP_MSG("config_layer handle=0x%x\n", param[2].memref.handle);

	DISP_DBG("disp_secure_intr_callback start run!\n");
	while (1) {
		ret =
		    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_INTR_CALLBACK, paramTypes,
					param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("KREE_TeeServiceCall(TZCMD_DDP_INTR_CALLBACK) fail, ret=%d\n",
				 ret);
		} else {
			DISP_DBG("disp_secure_intr_callback called irq=%d, statue_reg=0x%x\n",
				 param[1].value.a, param[2].value.a);
		}

#if 0
		irq = param[1].value.a;
		status_reg = param[2].value.a;
		switch (irq) {
		case MT8135_DISP_ROT_IRQ_ID:
			if (status_reg & (1 << 0)) {
				g_disp_irq.irq_src |= (1 << DISP_MODULE_ROT);
			}
			disp_invoke_irq_callbacks(DISP_MODULE_ROT, status_reg);
			break;
		case MT8135_DISP_WDMA0_IRQ_ID:
			if (status_reg & (1 << 0)) {
				g_disp_irq.irq_src |= (1 << DISP_MODULE_WDMA0);
				DISP_DBG("ree wdma0 intr done, cnt=%d\n", ree_wdma0_cnt++);
			}
			disp_invoke_irq_callbacks(DISP_MODULE_WDMA0, status_reg);
			break;
		case MT8135_DISP_WDMA1_IRQ_ID:
			if (status_reg & (1 << 0)) {
				g_disp_irq.irq_src |= (1 << DISP_MODULE_WDMA1);
			}
			disp_invoke_irq_callbacks(DISP_MODULE_WDMA1, status_reg);
			break;
		case MT8135_DISP_OVL_IRQ_ID:
			if (status_reg & (1 << 1)) {
				g_disp_irq.irq_src |= (1 << DISP_MODULE_OVL);
			}
			disp_invoke_irq_callbacks(DISP_MODULE_OVL, status_reg);
			break;
		}
#endif
		irq = param[1].value.a;
		DISP_DBG
		    ("[02420] irq=0x%08x ovl_status=0x%08x wdma1_status=0x%08x rdma0_status=0x%08x",
		     irq, pIrqStatus->OVL_IRQ_STATUS, pIrqStatus->WDMA1_IRQ_STATUS,
		     pIrqStatus->RDMA0_IRQ_STATUS);

		if (1 == ((irq >> MT8135_DISP_ROT_IRQ_BIT) & 0x00000001)) {
			if ((pIrqStatus->ROT_IRQ_STATUS) & (1 << 0)) {
				spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
				g_disp_irq.irq_src |= (1 << DISP_MODULE_ROT);
				spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
			}
			disp_invoke_irq_callbacks(DISP_MODULE_ROT, pIrqStatus->ROT_IRQ_STATUS);
		}
		if (1 == ((irq >> MT8135_DISP_OVL_IRQ_BIT) & 0x00000001)) {
			if ((pIrqStatus->OVL_IRQ_STATUS) & (1 << 1)) {
				spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
				g_disp_irq.irq_src |= (1 << DISP_MODULE_OVL);
				spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
			}
			disp_invoke_irq_callbacks(DISP_MODULE_OVL, pIrqStatus->OVL_IRQ_STATUS);
		}
		if (1 == ((irq >> MT8135_DISP_WDMA0_IRQ_BIT) & 0x00000001)) {
			if ((pIrqStatus->WDMA0_IRQ_STATUS) & (1 << 0)) {
				spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
				g_disp_irq.irq_src |= (1 << DISP_MODULE_WDMA0);
				DISP_DBG("[02420]ree wdma0 intr done, cnt=%d\n", ree_wdma0_cnt++);
				spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
			}
			disp_invoke_irq_callbacks(DISP_MODULE_WDMA0, pIrqStatus->WDMA0_IRQ_STATUS);
		}
		if (1 == ((irq >> MT8135_DISP_WDMA1_IRQ_BIT) & 0x00000001)) {
			if ((pIrqStatus->WDMA1_IRQ_STATUS) & (1 << 0)) {
				spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
				g_disp_irq.irq_src |= (1 << DISP_MODULE_WDMA1);
				/* DISP_DBG("[02420]ree wdma1/ovl intr done, cnt=%d\n", ree_ovl_cnt++); */
				spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
			}
			disp_invoke_irq_callbacks(DISP_MODULE_WDMA1, pIrqStatus->WDMA1_IRQ_STATUS);
		}
		if (1 == ((irq >> MT8135_DISP_RDMA0_IRQ_BIT) & 0x00000001)) {
			if ((pIrqStatus->RDMA0_IRQ_STATUS) & (1 << 2)) {
				spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
				g_disp_irq.irq_src |= (1 << DISP_MODULE_RDMA0);
				DISP_DBG("[02420]ree rdma0 intr done, cnt=%d\n", ree_rdma0_cnt++);
				spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
			}
			disp_invoke_irq_callbacks(DISP_MODULE_RDMA0, pIrqStatus->RDMA0_IRQ_STATUS);
		}
		if (1 == ((irq >> MT8135_DISP_RDMA1_IRQ_BIT) & 0x00000001)) {
			if ((pIrqStatus->RDMA1_IRQ_STATUS) & (1 << 2)) {
				spin_lock_irqsave(&g_disp_irq.irq_lock, flags);
				g_disp_irq.irq_src |= (1 << DISP_MODULE_RDMA1);
				spin_unlock_irqrestore(&g_disp_irq.irq_lock, flags);
			}
			disp_invoke_irq_callbacks(DISP_MODULE_RDMA1, pIrqStatus->RDMA1_IRQ_STATUS);
		}
		/* Wakeup event */
		mb();		/* Add memory barrier before the other CPU (may) wakeup */
		wake_up_interruptible(&g_disp_irq_done_queue);
	}
	return 0;
}
#endif

void disp_register_intr(unsigned int irq, unsigned int secure)
{
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	MTEEC_PARAM param[4];
	unsigned int paramTypes;
	TZ_RESULT ret;
	param[0].value.a = irq;
	param[1].value.a = secure;
	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);

	if (secure == 0) {
		/* unregister irq in TEE */
		ret =
		    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_REGISTER_INTR, paramTypes,
					param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("KREE_TeeServiceCall(TZCMD_DDP_REGISTER_INTR) fail, ret=%d\n",
				 ret);
		}
		/* register irq in REE */
		DISP_REGISTER_IRQ(irq);
		/* enable_irq(irq);        // request irq will also enable irq */
	} else {
		/* unregister irq in REE */
		disable_irq(irq);
		DISP_UNREGISTER_IRQ(irq);

		/* register irq in TEE */
		ret =
		    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_REGISTER_INTR, paramTypes,
					param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("KREE_TeeServiceCall(TZCMD_DDP_REGISTER_INTR) fail, ret=%d\n",
				 ret);
		}
	}
#endif
}

static long disp_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	DISP_WRITE_REG wParams;
	DISP_READ_REG rParams;
	unsigned int ret = 0;
	unsigned int value;
	DISP_MODULE_ENUM module;
	DISP_OVL_INFO ovl_info;
	DISP_PQ_PARAM *pq_param;
	DISP_PQ_PARAM *pq_cam_param;
	DISP_PQ_PARAM *pq_gal_param;
	DISPLAY_PQ_T *pq_index;
	DISPLAY_TDSHP_T *tdshp_index;
	DISPLAY_GAMMA_T *gamma_index;
	DISPLAY_PWM_T *pwm_lut;
	int layer, mutex_id;
	disp_wait_irq_struct wait_irq_struct;
	unsigned long lcmindex = 0;

#if defined(CONFIG_MTK_AAL_SUPPORT)
	DISP_AAL_PARAM *aal_param;
	int count;
#endif

#ifdef DDP_DBG_DDP_PATH_CONFIG
	struct disp_path_config_struct config;
#endif

	disp_node_struct *pNode = (disp_node_struct *) file->private_data;

#if 0
	if (inAddr == 0) {
		inAddr = kmalloc(800 * 480 * 4, GFP_KERNEL);
		memset((void *)inAddr, 0x55, 800 * 480 * 4);
		DISP_MSG("inAddr=0x%x\n", inAddr);
	}
	if (outAddr == 0) {
		outAddr = kmalloc(800 * 480 * 4, GFP_KERNEL);
		memset((void *)outAddr, 0xff, 800 * 480 * 4);
		DISP_MSG("outAddr=0x%x\n", outAddr);
	}
#endif
	DISP_DBG("cmd=0x%x, arg=0x%x\n", cmd, (unsigned int)arg);
	switch (cmd) {
	case DISP_IOCTL_WRITE_REG:

		if (copy_from_user(&wParams, (void *)arg, sizeof(DISP_WRITE_REG))) {
			DISP_ERR("DISP_IOCTL_WRITE_REG, copy_from_user failed\n");
			return -EFAULT;
		}

		DISP_DBG("write  0x%x = 0x%x (0x%x)\n", wParams.reg, wParams.val, wParams.mask);
		if (wParams.reg > DISPSYS_REG_ADDR_MAX || wParams.reg < DISPSYS_REG_ADDR_MIN) {
			DISP_ERR("reg write, addr invalid, addr min=0x%x, max=0x%x, addr=0x%x\n",
				 DISPSYS_REG_ADDR_MIN, DISPSYS_REG_ADDR_MAX, wParams.reg);
			return -EFAULT;
		}

		*(volatile unsigned int *)wParams.reg =
		    (*(volatile unsigned int *)wParams.reg & ~wParams.mask) | (wParams.
									       val & wParams.mask);
		/* mt65xx_reg_sync_writel(wParams.reg, value); */
		break;

	case DISP_IOCTL_READ_REG:
		if (copy_from_user(&rParams, (void *)arg, sizeof(DISP_READ_REG))) {
			DISP_ERR("DISP_IOCTL_READ_REG, copy_from_user failed\n");
			return -EFAULT;
		}
		if (0)		/* wParams.reg>DISPSYS_REG_ADDR_MAX || wParams.reg<DISPSYS_REG_ADDR_MIN) */
		{
			DISP_ERR("reg read, addr invalid, addr min=0x%x, max=0x%x, addr=0x%x\n",
				 DISPSYS_REG_ADDR_MIN, DISPSYS_REG_ADDR_MAX, wParams.reg);
			return -EFAULT;
		}

		value = (*(volatile unsigned int *)rParams.reg) & rParams.mask;

		DISP_DBG("read 0x%x = 0x%x (0x%x)\n", rParams.reg, value, rParams.mask);

		if (copy_to_user(rParams.val, &value, sizeof(unsigned int))) {
			DISP_ERR("DISP_IOCTL_READ_REG, copy_to_user failed\n");
			return -EFAULT;
		}
		break;

	case DISP_IOCTL_WAIT_IRQ:
		if (copy_from_user(&wait_irq_struct, (void *)arg, sizeof(wait_irq_struct))) {
			DISP_ERR("DISP_IOCTL_WAIT_IRQ, copy_from_user failed\n");
			return -EFAULT;
		}
		ret = disp_wait_intr(wait_irq_struct.module, wait_irq_struct.timeout_ms);
		break;

	case DISP_IOCTL_DUMP_REG:
		if (copy_from_user(&module, (void *)arg, sizeof(module))) {
			DISP_ERR("DISP_IOCTL_DUMP_REG, copy_from_user failed\n");
			return -EFAULT;
		}
		ret = disp_dump_reg(module);
		break;

	case DISP_IOCTL_LOCK_THREAD:
		pr_info("DISP_IOCTL_LOCK_THREAD!\n");
		value = disp_lock_cmdq_thread();
		if (copy_to_user((void *)arg, &value, sizeof(unsigned int))) {
			DISP_ERR("DISP_IOCTL_LOCK_THREAD, copy_to_user failed\n");
			return -EFAULT;
		}
		break;

	case DISP_IOCTL_UNLOCK_THREAD:
		if (copy_from_user(&value, (void *)arg, sizeof(value))) {
			DISP_ERR("DISP_IOCTL_UNLOCK_THREAD, copy_from_user failed\n");
			return -EFAULT;
		}
		ret = disp_unlock_cmdq_thread(value);
		break;

	case DISP_IOCTL_MARK_CMQ:
		if (copy_from_user(&value, (void *)arg, sizeof(value))) {
			DISP_ERR("DISP_IOCTL_MARK_CMQ, copy_from_user failed\n");
			return -EFAULT;
		}
		if (value >= CMDQ_THREAD_NUM)
			return -EFAULT;
		cmq_status[value] = 1;
		break;

	case DISP_IOCTL_WAIT_CMQ:
		if (copy_from_user(&value, (void *)arg, sizeof(value))) {
			DISP_ERR("DISP_IOCTL_WAIT_CMQ, copy_from_user failed\n");
			return -EFAULT;
		}
		if (value >= CMDQ_THREAD_NUM)
			return -EFAULT;

		wait_event_interruptible_timeout(cmq_wait_queue[value], cmq_status[value], 3 * HZ);
		if (cmq_status[value] != 0) {
			cmq_status[value] = 0;
			return -EFAULT;
		}
		break;

	case DISP_IOCTL_LOCK_RESOURCE:
		if (copy_from_user(&mutex_id, (void *)arg, sizeof(int))) {
			DISP_ERR("DISP_IOCTL_LOCK_RESOURCE, copy_from_user failed\n");
			return -EFAULT;
		}
		if ((-1) != mutex_id) {
			int ret = wait_event_interruptible_timeout(gResourceWaitQueue,
								   (gLockedResource &
								    (1 << mutex_id)) == 0,
								   disp_ms2jiffies(50));

			if (ret <= 0) {
				DISP_ERR("DISP_IOCTL_LOCK_RESOURCE, mutex_id 0x%x failed\n",
					 gLockedResource);
				return -EFAULT;
			}

			spin_lock(&gResourceLock);
			gLockedResource |= (1 << mutex_id);
			spin_unlock(&gResourceLock);

			spin_lock(&pNode->node_lock);
			pNode->u4LockedResource = gLockedResource;
			spin_unlock(&pNode->node_lock);
		} else {
			DISP_ERR("DISP_IOCTL_LOCK_RESOURCE, mutex_id = -1 failed\n");
			return -EFAULT;
		}
		break;


	case DISP_IOCTL_UNLOCK_RESOURCE:
		if (copy_from_user(&mutex_id, (void *)arg, sizeof(int))) {
			DISP_ERR("DISP_IOCTL_UNLOCK_RESOURCE, copy_from_user failed\n");
			return -EFAULT;
		}
		if ((-1) != mutex_id) {
			spin_lock(&gResourceLock);
			gLockedResource &= ~(1 << mutex_id);
			spin_unlock(&gResourceLock);

			spin_lock(&pNode->node_lock);
			pNode->u4LockedResource = gLockedResource;
			spin_unlock(&pNode->node_lock);

			wake_up_interruptible(&gResourceWaitQueue);
		} else {
			DISP_ERR("DISP_IOCTL_UNLOCK_RESOURCE, mutex_id = -1 failed\n");
			return -EFAULT;
		}
		break;

	case DISP_IOCTL_LOCK_MUTEX:
		{
			wait_event_interruptible_timeout(gMutexWaitQueue,
							 (mutex_id = disp_lock_mutex()) != -1,
							 disp_ms2jiffies(200));

			if ((-1) != mutex_id) {
				spin_lock(&pNode->node_lock);
				pNode->u4LockedMutex |= (1 << mutex_id);
				spin_unlock(&pNode->node_lock);
			}

			if (copy_to_user((void *)arg, &mutex_id, sizeof(int))) {
				DISP_ERR("disp driver : Copy to user error (mutex)\n");
				return -EFAULT;
			}
			break;
		}
	case DISP_IOCTL_UNLOCK_MUTEX:
		if (copy_from_user(&mutex_id, (void *)arg, sizeof(int))) {
			DISP_ERR("DISP_IOCTL_UNLOCK_MUTEX, copy_from_user failed\n");
			return -EFAULT;
		}
		disp_unlock_mutex(mutex_id);

		if ((-1) != mutex_id) {
			spin_lock(&pNode->node_lock);
			pNode->u4LockedMutex &= ~(1 << mutex_id);
			spin_unlock(&pNode->node_lock);
		}

		wake_up_interruptible(&gMutexWaitQueue);

		break;

	case DISP_IOCTL_SYNC_REG:
		mb();
		break;

	case DISP_IOCTL_SET_INTR:
		DISP_DBG("DISP_IOCTL_SET_INTR!\n");
		if (copy_from_user(&value, (void *)arg, sizeof(int))) {
			DISP_ERR("DISP_IOCTL_SET_INTR, copy_from_user failed\n");
			return -EFAULT;
		}
		/* enable intr */
		if ((value & 0xffff0000) != 0) {
			disable_irq(value & 0xff);
			pr_info("disable_irq %d\n", value & 0xff);
		} else {
			DISP_REGISTER_IRQ(value & 0xff);
			pr_info("enable irq: %d\n", value & 0xff);
		}
		break;

	case DISP_IOCTL_RUN_DPF:
		DISP_DBG("DISP_IOCTL_RUN_DPF!\n");
		if (copy_from_user(&value, (void *)arg, sizeof(int))) {
			DISP_ERR("DISP_IOCTL_SET_INTR, copy_from_user failed, %d\n", ret);
			return -EFAULT;
		}

		spin_lock(&gOvlLock);

		disp_run_dp_framework = value;

		spin_unlock(&gOvlLock);

		if (value == 1) {
			while (disp_get_mutex_status() != 0) {
				DISP_ERR("disp driver : wait fb release hw mutex\n");
				msleep(3);
			}
		}
		break;

	case DISP_IOCTL_CHECK_OVL:
		DISP_DBG("DISP_IOCTL_CHECK_OVL!\n");
		value = disp_layer_enable;

		if (copy_to_user((void *)arg, &value, sizeof(int))) {
			DISP_ERR("disp driver : Copy to user error (result)\n");
			return -EFAULT;
		}
		break;

	case DISP_IOCTL_GET_OVL:
		DISP_DBG("DISP_IOCTL_GET_OVL!\n");
		if (copy_from_user(&ovl_info, (void *)arg, sizeof(DISP_OVL_INFO))) {
			DISP_ERR("DISP_IOCTL_SET_INTR, copy_from_user failed, %d\n", ret);
			return -EFAULT;
		}

		layer = ovl_info.layer;

		spin_lock(&gOvlLock);
		ovl_info = disp_layer_info[layer];
		spin_unlock(&gOvlLock);

		if (copy_to_user((void *)arg, &ovl_info, sizeof(DISP_OVL_INFO))) {
			DISP_ERR("disp driver : Copy to user error (result)\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_AAL_EVENTCTL:
#if !defined(CONFIG_MTK_AAL_SUPPORT)
		pr_info
		    ("Invalid operation DISP_IOCTL_AAL_EVENTCTL since AAL is not turned on, in %s\n",
		     __func__);
		return -EFAULT;
#else
		if (copy_from_user(&value, (void *)arg, sizeof(int))) {
			pr_info("disp driver : DISP_IOCTL_AAL_EVENTCTL Copy from user failed\n");
			return -EFAULT;
		}
		disp_set_aal_alarm(value);
		disp_set_needupdate(DISP_MODULE_BLS, 1);
		ret = 0;
#endif
		break;

	case DISP_IOCTL_GET_AALSTATISTICS:
#if !defined(CONFIG_MTK_AAL_SUPPORT)
		pr_info
		    ("Invalid operation DISP_IOCTL_GET_AALSTATISTICS since AAL is not turned on, in %s\n",
		     __func__);
		return -EFAULT;
#else
		/* 1. Wait till new interrupt comes */
		if (disp_wait_hist_update(60)) {
			pr_info("disp driver : DISP_IOCTL_GET_AALSTATISTICS wait time out\n");
			return -EFAULT;
		}
		/*disp_update_hist();*/
		/*disp_wakeup_aal();*/
		/* 2. read out color engine histogram */
		disp_set_hist_readlock(1);
		if (copy_to_user
		    ((void *)arg, (void *)(disp_get_hist_ptr()), sizeof(DISP_AAL_STATISTICS))) {
			pr_info("disp driver : DISP_IOCTL_GET_AALSTATISTICS Copy to user failed\n");
			return -EFAULT;
		}
		disp_set_hist_readlock(0);
		ret = 0;
#endif
		break;

	case DISP_IOCTL_SET_AALPARAM:
#if !defined(CONFIG_MTK_AAL_SUPPORT)
		pr_info
		    ("Invalid operation : DISP_IOCTL_SET_AALPARAM since AAL is not turned on, in %s\n",
		     __func__);
		return -EFAULT;
#else
/* disp_set_needupdate(DISP_MODULE_BLS , 0); */

		disp_aal_lock();

		aal_param = get_aal_config();

		if (copy_from_user(aal_param, (void *)arg, sizeof(DISP_AAL_PARAM))) {
			pr_info("disp driver : DISP_IOCTL_SET_AALPARAM Copy from user failed\n");
			return -EFAULT;
		}

		disp_aal_unlock();
#endif
		break;

	case DISP_IOCTL_SET_PQPARAM:

		DISP_RegisterExTriggerSource(CheckColorUpdateFunc, ConfColorFunc);

		GetUpdateMutex();

		pq_param = get_Color_config();
		if (copy_from_user(pq_param, (void *)arg, sizeof(DISP_PQ_PARAM))) {
			pr_info("disp driver : DISP_IOCTL_SET_PQPARAM Copy from user failed\n");
			return -EFAULT;
		}

		ReleaseUpdateMutex();

		disp_set_needupdate(DISP_MODULE_COLOR, 1);

		break;

	case DISP_IOCTL_SET_PQINDEX:

		pq_index = get_Color_index();
		if (copy_from_user(pq_index, (void *)arg, sizeof(DISPLAY_PQ_T))) {
			pr_info("disp driver : DISP_IOCTL_SET_PQINDEX Copy from user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_GET_PQPARAM:

		pq_param = get_Color_config();
		if (copy_to_user((void *)arg, pq_param, sizeof(DISP_PQ_PARAM))) {
			pr_info("disp driver : DISP_IOCTL_GET_PQPARAM Copy to user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_SET_TDSHPINDEX:

		tdshp_index = get_TDSHP_index();
		if (copy_from_user(tdshp_index, (void *)arg, sizeof(DISPLAY_TDSHP_T))) {
			pr_info("disp driver : DISP_IOCTL_SET_TDSHPINDEX Copy from user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_GET_TDSHPINDEX:

		tdshp_index = get_TDSHP_index();
		if (copy_to_user((void *)arg, tdshp_index, sizeof(DISPLAY_TDSHP_T))) {
			pr_info("disp driver : DISP_IOCTL_GET_TDSHPINDEX Copy to user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_SET_GAMMALUT:

		gamma_index = get_gamma_index();
		if (copy_from_user(gamma_index, (void *)arg, sizeof(DISPLAY_GAMMA_T))) {
			pr_info("disp driver : DISP_IOCTL_SET_GAMMALUT Copy from user failed\n");
			return -EFAULT;
		}

		disp_bls_update_gamma_lut();

		break;

	case DISP_IOCTL_SET_PWMLUT:

		pwm_lut = get_pwm_lut();
		if (copy_from_user(pwm_lut, (void *)arg, sizeof(DISPLAY_PWM_T))) {
			pr_info("disp driver : DISP_IOCTL_SET_PWMLUT Copy from user failed\n");
			return -EFAULT;
		}

		disp_bls_update_pwm_lut();

		break;

	case DISP_IOCTL_SET_CLKON:
		if (copy_from_user(&module, (void *)arg, sizeof(DISP_MODULE_ENUM))) {
			pr_info("disp driver : DISP_IOCTL_SET_CLKON Copy from user failed\n");
			return -EFAULT;
		}

		disp_power_on(module, &(pNode->u4Clock));
		break;

	case DISP_IOCTL_SET_CLKOFF:
		if (copy_from_user(&module, (void *)arg, sizeof(DISP_MODULE_ENUM))) {
			pr_info("disp driver : DISP_IOCTL_SET_CLKOFF Copy from user failed\n");
			return -EFAULT;
		}

		disp_power_off(module, &(pNode->u4Clock));
		break;

	case DISP_IOCTL_MUTEX_CONTROL:
		if (copy_from_user(&value, (void *)arg, sizeof(int))) {
			pr_info("disp driver : DISP_IOCTL_MUTEX_CONTROL Copy from user failed\n");
			return -EFAULT;
		}

		if (value == 1) {
#if defined(CONFIG_MTK_AAL_SUPPORT)
			/* suspend AAL and disable BLS */
			disp_aal_lock();
			aal_debug_flag = 1;
			disp_aal_unlock();
			count = 0;
			while (DISP_REG_GET(DISP_REG_BLS_EN) != 0x80000000) {
				msleep(1);
				count++;
				if (count > 1000) {
					pr_info("disp driver : fail to disable BLS (0x%x)\n",
						DISP_REG_GET(DISP_REG_BLS_EN));
					break;
				}
			}
#endif
			GetUpdateMutex();
		} else if (value == 2) {
			ReleaseUpdateMutex();
#if defined(CONFIG_MTK_AAL_SUPPORT)
			/* resume AAL and enable BLS */
			disp_aal_lock();
			aal_debug_flag = 0;
			disp_aal_unlock();
			count = 0;
			while (DISP_REG_GET(DISP_REG_BLS_EN) != 0x80010001) {
				msleep(1);
				count++;
				if (count > 1000) {
					pr_info("disp driver : fail to enable BLS (0x%x)\n",
						DISP_REG_GET(DISP_REG_BLS_EN));
					break;
				}
			}
#endif
		} else {
			pr_info("disp driver : DISP_IOCTL_MUTEX_CONTROL invalid control\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_GET_LCMINDEX:

		lcmindex = DISP_GetLCMIndex();
		if (copy_to_user((void *)arg, &lcmindex, sizeof(unsigned long))) {
			pr_info("disp driver : DISP_IOCTL_GET_LCMINDEX Copy to user failed\n");
			return -EFAULT;
		}

		break;

		break;

	case DISP_IOCTL_SET_PQ_CAM_PARAM:

		pq_cam_param = get_Color_Cam_config();
		if (copy_from_user(pq_cam_param, (void *)arg, sizeof(DISP_PQ_PARAM))) {
			pr_info
			    ("disp driver : DISP_IOCTL_SET_PQ_CAM_PARAM Copy from user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_GET_PQ_CAM_PARAM:

		pq_cam_param = get_Color_Cam_config();
		if (copy_to_user((void *)arg, pq_cam_param, sizeof(DISP_PQ_PARAM))) {
			pr_info("disp driver : DISP_IOCTL_GET_PQ_CAM_PARAM Copy to user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_SET_PQ_GAL_PARAM:

		pq_gal_param = get_Color_Gal_config();
		if (copy_from_user(pq_gal_param, (void *)arg, sizeof(DISP_PQ_PARAM))) {
			pr_info
			    ("disp driver : DISP_IOCTL_SET_PQ_GAL_PARAM Copy from user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_GET_PQ_GAL_PARAM:

		pq_gal_param = get_Color_Gal_config();
		if (copy_to_user((void *)arg, pq_gal_param, sizeof(DISP_PQ_PARAM))) {
			pr_info("disp driver : DISP_IOCTL_GET_PQ_GAL_PARAM Copy to user failed\n");
			return -EFAULT;
		}

		break;

	case DISP_IOCTL_TEST_PATH:
#ifdef DDP_DBG_DDP_PATH_CONFIG
		if (copy_from_user(&value, (void *)arg, sizeof(value))) {
			DISP_ERR("DISP_IOCTL_MARK_CMQ, copy_from_user failed\n");
			return -EFAULT;
		}

		config.layer = 0;
		config.layer_en = 1;
		config.source = OVL_LAYER_SOURCE_MEM;
		config.addr = virt_to_phys(inAddr);
		config.inFormat = OVL_INPUT_FORMAT_RGB565;
		config.pitch = 480;
		config.srcROI.x = 0;	/* ROI */
		config.srcROI.y = 0;
		config.srcROI.width = 480;
		config.srcROI.height = 800;
		config.bgROI.x = config.srcROI.x;
		config.bgROI.y = config.srcROI.y;
		config.bgROI.width = config.srcROI.width;
		config.bgROI.height = config.srcROI.height;
		config.bgColor = 0xff;	/* background color */
		config.key = 0xff;	/* color key */
		config.aen = 0;	/* alpha enable */
		config.alpha = 0;
		DISP_DBG("value=%d\n", value);
		if (value == 0)	/* mem->ovl->rdma0->dpi0 */
		{
			config.srcModule = DISP_MODULE_OVL;
			config.outFormat = RDMA_OUTPUT_FORMAT_ARGB;
			config.dstModule = DISP_MODULE_DPI0;
			config.dstAddr = 0;
		} else if (value == 1)	/* mem->ovl-> wdma1->mem */
		{
			config.srcModule = DISP_MODULE_OVL;
			config.outFormat = WDMA_OUTPUT_FORMAT_RGB888;
			config.dstModule = DISP_MODULE_WDMA1;
			config.dstAddr = virt_to_phys(outAddr);
		} else if (value == 2)	/* mem->rdma0 -> dpi0 */
		{
			config.srcModule = DISP_MODULE_RDMA0;
			config.outFormat = RDMA_OUTPUT_FORMAT_ARGB;
			config.dstModule = DISP_MODULE_DPI0;
			config.dstAddr = 0;
		}
		disp_path_config(&config);
		disp_path_enable();
#endif
		break;
	case DISP_IOCTL_G_WAIT_REQUEST:
		ret = ddp_bitblt_ioctl_wait_reequest(arg);
		break;

	case DISP_IOCTL_T_INFORM_DONE:
		ret = ddp_bitblt_ioctl_inform_done(arg);
		break;

	case DISP_SECURE_MVA_MAP:
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		{
			struct disp_mva_map mva_map_struct;
			if (copy_from_user
			    (&mva_map_struct, (void *)arg, sizeof(struct disp_mva_map))) {
				DISP_ERR("DISP_SECURE_MVA_MAP, copy_from_user failed\n");
				return -EFAULT;
			}
			DISP_DBG("map_mva, module=%d, cache=%d, addr=0x%x, size=0x%x\n",
				 mva_map_struct.module,
				 mva_map_struct.cache_coherent,
				 mva_map_struct.addr, mva_map_struct.size);

			if (mva_map_struct.addr == 0) {
				DISP_ERR("invalid parameter for DISP_SECURE_MVA_MAP!\n");
			} else {
				MTEEC_PARAM param[4];
				unsigned int paramTypes;
				TZ_RESULT ret;

				param[0].value.a = mva_map_struct.module;
				param[1].value.a = mva_map_struct.cache_coherent;
				param[2].value.a = mva_map_struct.addr;
				param[3].value.a = mva_map_struct.size;
				paramTypes =
				    TZ_ParamTypes4(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT,
						   TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
				ret =
				    KREE_TeeServiceCall(ddp_session_handle(),
							TZCMD_DDP_SECURE_MVA_MAP, paramTypes,
							param);
				if (ret != TZ_RESULT_SUCCESS) {
					DISP_ERR
					    ("KREE_TeeServiceCall(TZCMD_DDP_SECURE_MVA_MAP) fail, ret=%d\n",
					     ret);
				}
			}
		}
#endif
		break;

	case DISP_SECURE_MVA_UNMAP:
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		{
			struct disp_mva_map mva_map_struct;
			if (copy_from_user
			    (&mva_map_struct, (void *)arg, sizeof(struct disp_mva_map))) {
				DISP_ERR("DISP_SECURE_MVA_UNMAP, copy_from_user failed\n");
				return -EFAULT;
			}
			DISP_DBG("unmap_mva, module=%d, cache=%d, addr=0x%x, size=0x%x\n",
				 mva_map_struct.module,
				 mva_map_struct.cache_coherent,
				 mva_map_struct.addr, mva_map_struct.size);

			if (mva_map_struct.addr == 0) {
				DISP_ERR("invalid parameter for DISP_SECURE_MVA_UNMAP!\n");
			} else {
				MTEEC_PARAM param[4];
				unsigned int paramTypes;
				TZ_RESULT ret;

				param[0].value.a = mva_map_struct.module;
				param[1].value.a = mva_map_struct.cache_coherent;
				param[2].value.a = mva_map_struct.addr;
				param[3].value.a = mva_map_struct.size;
				paramTypes =
				    TZ_ParamTypes4(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT,
						   TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
				ret =
				    KREE_TeeServiceCall(ddp_session_handle(),
							TZCMD_DDP_SECURE_MVA_UNMAP, paramTypes,
							param);
				if (ret != TZ_RESULT_SUCCESS) {
					DISP_ERR
					    ("KREE_TeeServiceCall(TZCMD_DDP_SECURE_MVA_UNMAP) fail, ret=%d\n",
					     ret);
				}
			}
		}
#endif
		break;

	case DISP_SECURE_SET_MODE_BITBLT:
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		{
			unsigned int secure;
			if (copy_from_user(&secure, (void *)arg, sizeof(unsigned int))) {
				pr_info
				    ("disp driver : DISP_SECURE_SET_MODE_BITBLT Copy from user failed\n");
				return -EFAULT;
			}

			if (secure != gBitbltSecure) {


				disp_register_intr(MT8135_DISP_ROT_IRQ_ID, secure);
				disp_register_intr(MT8135_DISP_WDMA0_IRQ_ID, secure);
				gBitbltSecure = secure;
			}
		}
#endif
		break;

	case DISP_SECURE_SET_MODE_OVL_MEM_OUT:
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		{
			unsigned int secure;
			if (copy_from_user(&secure, (void *)arg, sizeof(unsigned int))) {
				pr_info
				    ("disp driver : DISP_SECURE_SET_MODE_BITBLT Copy from user failed\n");
				return -EFAULT;
			}

			if (secure != gMemOutSecure) {
				if (disp_secure_irq_task == NULL) {
					disp_secure_irq_task =
					    kthread_create(disp_secure_intr_callback, NULL,
							   "disp_secure_irq_task");
					if (IS_ERR(disp_secure_irq_task)) {
						DISP_ERR
						    ("DISP_InitVSYNC(): Cannot create disp_irq_log_task kthread\n");
					}
					wake_up_process(disp_secure_irq_task);
				}

				disp_register_intr(MT8135_DISP_WDMA1_IRQ_ID, secure);
				gMemOutSecure = secure;
			}
		}
#endif
		break;

	default:
		DISP_ERR("Ddp drv dose not have such command : %d\n", cmd);
		break;
	}

	return ret;
}

static int disp_open(struct inode *inode, struct file *file)
{
	disp_node_struct *pNode = NULL;

	DISP_DBG("enter disp_open() process:%s\n", current->comm);

	/* Allocate and initialize private data */
	file->private_data = kmalloc(sizeof(disp_node_struct), GFP_ATOMIC);
	if (NULL == file->private_data) {
		DISP_ERR("Not enough entry for DDP open operation\n");
		return -ENOMEM;
	}

	pNode = (disp_node_struct *) file->private_data;
	pNode->open_pid = current->pid;
	pNode->open_tgid = current->tgid;
	INIT_LIST_HEAD(&(pNode->testList));
	pNode->u4LockedMutex = 0;
	pNode->u4LockedResource = 0;
	pNode->u4Clock = 0;
	spin_lock_init(&pNode->node_lock);

	return 0;

}

static ssize_t disp_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
	return 0;
}

static int disp_release(struct inode *inode, struct file *file)
{
	disp_node_struct *pNode = NULL;
	unsigned int index = 0;
	DISP_DBG("enter disp_release() process:%s\n", current->comm);

	pNode = (disp_node_struct *) file->private_data;

	spin_lock(&pNode->node_lock);

	if (pNode->u4LockedMutex) {
		DISP_ERR("Proccess terminated[Mutex] ! :%s , mutex:%u\n", current->comm,
			 pNode->u4LockedMutex);

		for (index = 0; index < ENGINE_MUTEX_NUM; index += 1) {
			if ((1 << index) & pNode->u4LockedMutex) {
				disp_unlock_mutex(index);
				DISP_DBG("unlock index = %d ,mutex_used[ %d %d %d %d ]\n", index,
					 mutex_used[0], mutex_used[1], mutex_used[2],
					 mutex_used[3]);
			}
		}

	}

	if (pNode->u4LockedResource) {
		DISP_ERR("Proccess terminated[REsource] ! :%s , resource:%d\n", current->comm,
			 pNode->u4LockedResource);
		spin_lock(&gResourceLock);
		gLockedResource = 0;
		spin_unlock(&gResourceLock);
	}

	if (pNode->u4Clock) {
		DISP_ERR("Process safely terminated [Clock] !:%s , clock:%u\n", current->comm,
			 pNode->u4Clock);

		for (index = 0; index < DISP_MODULE_MAX; index += 1) {
			if ((1 << index) & pNode->u4Clock) {
				disp_power_off((DISP_MODULE_ENUM) index, &pNode->u4Clock);
			}
		}
	}

	spin_unlock(&pNode->node_lock);

	if (NULL != file->private_data) {
		kfree(file->private_data);
		file->private_data = NULL;
	}

	return 0;
}

static int disp_flush(struct file *file, fl_owner_t a_id)
{
	return 0;
}

/* remap register to user space */
static int disp_mmap(struct file *file, struct vm_area_struct *a_pstVMArea)
{

	a_pstVMArea->vm_page_prot = pgprot_noncached(a_pstVMArea->vm_page_prot);
	if (remap_pfn_range(a_pstVMArea,
			    a_pstVMArea->vm_start,
			    a_pstVMArea->vm_pgoff,
			    (a_pstVMArea->vm_end - a_pstVMArea->vm_start),
			    a_pstVMArea->vm_page_prot)) {
		DISP_ERR("MMAP failed!!\n");
		return -1;
	}


	return 0;
}


/* Kernel interface */
static struct file_operations disp_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = disp_unlocked_ioctl,
	.open = disp_open,
	.release = disp_release,
	.flush = disp_flush,
	.read = disp_read,
	.mmap = disp_mmap
};

static int disp_probe(struct platform_device *pdev)
{
	struct class_device;

	/*int ret;*/
	int i;
	struct proc_dir_entry *disp_entry;
	/*struct class_device *class_dev = NULL;*/

	DISP_MSG("\ndisp driver probe...\n\n");
/*
	ret = alloc_chrdev_region(&disp_devno, 0, 1, DISP_DEVNAME);

	if (ret) {
		DISP_ERR("Error: Can't Get Major number for DISP Device\n");
	} else {
		DISP_MSG("Get DISP Device Major number (%d)\n", disp_devno);
	}

	disp_cdev = cdev_alloc();
	disp_cdev->owner = THIS_MODULE;
	disp_cdev->ops = &disp_fops;

	ret = cdev_add(disp_cdev, disp_devno, 1);

	disp_class = class_create(THIS_MODULE, DISP_DEVNAME);
	class_dev =
	    (struct class_device *)device_create(disp_class, NULL, disp_devno, NULL, DISP_DEVNAME);
    */

	disp_entry = proc_create(DISP_DEVNAME, 0, NULL, &disp_fops);
	if (NULL == disp_entry) {
		DISP_ERR("disp_probe(): Cannot create disp_entry\n");
	}

	/* initial wait queue */
	for (i = 0; i < CMDQ_THREAD_NUM; i++) {
		init_waitqueue_head(&cmq_wait_queue[i]);
		cmq_status[i] = 0;
	}

	spin_lock_init(&gMutexLock);
	spin_lock_init(&gCmdqLock);
	spin_lock_init(&gResourceLock);
	spin_lock_init(&gOvlLock);
	spin_lock_init(&gRegisterUpdateLock);
	spin_lock_init(&gPowerOperateLock);
	spin_lock_init(&g_disp_irq.irq_lock);

	gLockedResource = 0;

	init_waitqueue_head(&disp_irq_log_wq);

	/* Register IRQ */

	/* DISP_REGISTER_IRQ(MT8135_DISP_COLOR_IRQ_ID); */
	DISP_REGISTER_IRQ(MT8135_DISP_BLS_IRQ_ID);
	DISP_REGISTER_IRQ(MT8135_DISP_SCL_IRQ_ID);
	DISP_REGISTER_IRQ(MT8135_DISP_ROT_IRQ_ID);
	DISP_REGISTER_IRQ(MT8135_DISP_OVL_IRQ_ID);
	DISP_REGISTER_IRQ(MT8135_DISP_WDMA0_IRQ_ID);
	DISP_REGISTER_IRQ(MT8135_DISP_WDMA1_IRQ_ID);
	DISP_REGISTER_IRQ(MT8135_DISP_RDMA0_IRQ_ID);
	DISP_REGISTER_IRQ(MT8135_DISP_RDMA1_IRQ_ID);
	DISP_REGISTER_IRQ(MT8135_DISP_CMDQ_IRQ_ID);
	DISP_REGISTER_IRQ(MT8135_DISP_MUTEX_IRQ_ID);
	DISP_REGISTER_IRQ(MT_G2D_IRQ_ID);


	/* create kthread to print irq log, else if print too much log in irq context, EE or HW Reset may happen */
	disp_irq_log_task =
	    kthread_create(disp_irq_log_kthread_func, NULL, "disp_config_update_kthread");
	if (IS_ERR(disp_irq_log_task)) {
		DISP_ERR("DISP_InitVSYNC(): Cannot create disp_irq_log_task kthread\n");
	}
	wake_up_process(disp_irq_log_task);

	/* .TEE. create SVP DDP TEE service session */
	/* note that we do not create SVP DDP TEE session handle here, since the */
	/* initialization sequence of drivers are not fixed. */

	DISP_MSG("DISP Probe Done\n");

	return 0;
}

static int disp_remove(struct platform_device *pdev)
{
	disable_irq(MT8135_DISP_SCL_IRQ_ID);
	disable_irq(MT8135_DISP_ROT_IRQ_ID);
	disable_irq(MT8135_DISP_OVL_IRQ_ID);
	disable_irq(MT8135_DISP_WDMA0_IRQ_ID);
	disable_irq(MT8135_DISP_WDMA1_IRQ_ID);
	disable_irq(MT8135_DISP_RDMA0_IRQ_ID);
	disable_irq(MT8135_DISP_RDMA1_IRQ_ID);
	disable_irq(MT8135_DISP_CMDQ_IRQ_ID);

	/* disable_irq(MT8135_DISP_COLOR_IRQ_ID); */
	disable_irq(MT8135_DISP_BLS_IRQ_ID);
	disable_irq(MT_G2D_IRQ_ID);

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* .TEE. close SVP DDP TEE serivice session */
	/* the sessions are created and accessed using [ddp_session_handle()] / */
	/* [ddp_mem_session_handle()] API, and closed here. */
	{
		TZ_RESULT ret;

		ret = KREE_CloseSession(ddp_session);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("DDP close ddp_session fail ret=%d\n", ret);
			return -1;
		}

		ret = KREE_CloseSession(ddp_mem_session);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("DDP close ddp_mem_session fail ret=%d\n", ret);
			return -1;
		}
	}
#endif

	return 0;
}

static void disp_shutdown(struct platform_device *pdev)
{
	/* Nothing yet */
}

/* PM suspend */
static int disp_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

/* PM resume */
static int disp_resume(struct platform_device *pdev)
{
	return 0;
}


static struct platform_driver disp_driver = {
	.probe = disp_probe,
	.remove = disp_remove,
	.shutdown = disp_shutdown,
	.suspend = disp_suspend,
	.resume = disp_resume,
	.driver = {
		   .name = DISP_DEVNAME,
		   },
};

static void disp_device_release(struct device *dev)
{
	/* Nothing to release? */
}

static u64 disp_dmamask = ~(u32) 0;

static struct platform_device disp_device = {
	.name = DISP_DEVNAME,
	.id = 0,
	.dev = {
		.release = disp_device_release,
		.dma_mask = &disp_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = 0,
};

static int __init disp_init(void)
{
	int ret;

	DISP_MSG("Register disp device\n");
	if (platform_device_register(&disp_device)) {
		DISP_ERR("failed to register disp device\n");
		ret = -ENODEV;
		return ret;
	}

	DISP_MSG("Register the disp driver\n");
	if (platform_driver_register(&disp_driver)) {
		DISP_ERR("failed to register disp driver\n");
		platform_device_unregister(&disp_device);
		ret = -ENODEV;
		return ret;
	}

	ddp_debug_init();

	pRegBackup = kmalloc(DDP_BACKUP_REG_NUM * sizeof(int), GFP_KERNEL);
	ASSERT(pRegBackup != NULL);
	*pRegBackup = DDP_UNBACKED_REG_MEM;

	if (disp_secure_irq_task == NULL) {
		disp_secure_irq_task =
			kthread_create(disp_secure_intr_callback, NULL,
			"disp_secure_irq_task");
		if (IS_ERR(disp_secure_irq_task))
			DISP_ERR
			("DISP_InitVSYNC(): Cannot create disp_irq_log_task kthread\n");

		wake_up_process(disp_secure_irq_task);
	}
	return 0;
}

static void __exit disp_exit(void)
{
	cdev_del(disp_cdev);
	unregister_chrdev_region(disp_devno, 1);

	platform_driver_unregister(&disp_driver);
	platform_device_unregister(&disp_device);

	device_destroy(disp_class, disp_devno);
	class_destroy(disp_class);

	ddp_debug_exit();

	DISP_MSG("Done\n");
}

int disp_set_overlay_roi(int layer, int x, int y, int w, int h, int pitch)
{
	/* DISP_MSG(" disp_set_overlay_roi %d\n", layer ); */

	if (layer < 0 || layer >= DDP_OVL_LAYER_MUN)
		return -1;
	spin_lock(&gOvlLock);

	disp_layer_info[layer].x = x;
	disp_layer_info[layer].y = y;
	disp_layer_info[layer].w = w;
	disp_layer_info[layer].h = h;
	disp_layer_info[layer].pitch = pitch;

	spin_unlock(&gOvlLock);

	return 0;
}

int disp_set_overlay_addr(int layer, unsigned int addr, DDP_OVL_FORMAT fmt)
{
	/* DISP_MSG(" disp_set_overlay_addr %d\n", layer ); */
	if (layer < 0 || layer >= DDP_OVL_LAYER_MUN)
		return -1;

	spin_lock(&gOvlLock);

	disp_layer_info[layer].addr = addr;
	if (fmt != DDP_NONE_FMT)
		disp_layer_info[layer].fmt = fmt;

	spin_unlock(&gOvlLock);

	return 0;
}

int disp_set_overlay(int layer, int enable)
{
	/* DISP_MSG(" disp_set_overlay %d %d\n", layer, enable ); */
	if (layer < 0 || layer >= DDP_OVL_LAYER_MUN)
		return -1;

	spin_lock(&gOvlLock);

	if (enable == 0)
		disp_layer_enable = disp_layer_enable & ~(1 << layer);
	else
		disp_layer_enable = disp_layer_enable | (1 << layer);

	spin_unlock(&gOvlLock);

	return 0;
}

int disp_is_dp_framework_run(void)
{
	/* DISP_MSG(" disp_is_dp_framework_run " ); */
	return disp_run_dp_framework;
}

int disp_set_mutex_status(int enable)
{
	/* DISP_MSG(" disp_set_mutex_status %d\n", enable ); */
	spin_lock(&gOvlLock);

	disp_mutex_status = enable;

	spin_unlock(&gOvlLock);
	return 0;
}

int disp_get_mutex_status(void)
{
	return disp_mutex_status;
}

int disp_dump_reg(DISP_MODULE_ENUM module)
{
	unsigned int index;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	if ((gBitbltSecure == 1) &&
	    (module == DISP_MODULE_ROT || module == DISP_MODULE_WDMA0 || module == DISP_MODULE_WDMA1
	     || module == DISP_MODULE_OVL)) {
		MTEEC_PARAM param[4];
		unsigned int paramTypes;
		TZ_RESULT ret;

		param[0].value.a = module;
		paramTypes = TZ_ParamTypes1(TZPT_VALUE_INPUT);
		ret =
		    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_DUMP_REG, paramTypes,
					param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("KREE_TeeServiceCall(TZCMD_DDP_DUMP_REG) fail, ret=%d\n", ret);
		}

		return 0;
	}
#endif
	switch (module) {
	case DISP_MODULE_ROT:
		DISP_MSG("===== DISP ROT Reg Dump: ============\n");
		DISP_MSG("(+000)ROT_EN                   =0x%x\n", DISP_REG_GET(DISP_REG_ROT_EN));
		DISP_MSG("(+008)ROT_RESET                =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_RESET));
		DISP_MSG("(+010)ROT_INTERRUPT_ENABLE     =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_INTERRUPT_ENABLE));
		DISP_MSG("(+018)ROT_INTERRUPT_STATUS     =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_INTERRUPT_STATUS));
		DISP_MSG("(+020)ROT_CON                  =0x%x\n", DISP_REG_GET(DISP_REG_ROT_CON));
		DISP_MSG("(+028)ROT_GMCIF_CON            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_GMCIF_CON));
		DISP_MSG("(+030)ROT_SRC_CON              =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_SRC_CON));
		DISP_MSG("(+040)ROT_SRC_BASE_0           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_SRC_BASE_0));
		DISP_MSG("(+048)ROT_SRC_BASE_1           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_SRC_BASE_1));
		DISP_MSG("(+050)ROT_SRC_BASE_2           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_SRC_BASE_2));
		DISP_MSG("(+060)ROT_MF_BKGD_SIZE_IN_BYTE =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MF_BKGD_SIZE_IN_BYTE));
		DISP_MSG("(+070)ROT_MF_SRC_SIZE          =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MF_SRC_SIZE));
		DISP_MSG("(+078)ROT_MF_CLIP_SIZE         =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MF_CLIP_SIZE));
		DISP_MSG("(+080)ROT_MF_OFFSET_1          =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MF_OFFSET_1));
		DISP_MSG("(+088)ROT_MF_PAR               =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MF_PAR));
		DISP_MSG("(+090)ROT_SF_BKGD_SIZE_IN_BYTE =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_SF_BKGD_SIZE_IN_BYTE));
		DISP_MSG("(+0B8)ROT_SF_PAR               =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_SF_PAR));
		DISP_MSG("(+0C0)ROT_MB_DEPTH             =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MB_DEPTH));
		DISP_MSG("(+0C8)ROT_MB_BASE              =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MB_BASE));
		DISP_MSG("(+0D0)ROT_MB_CON               =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MB_CON));
		DISP_MSG("(+0D8)ROT_SB_DEPTH             =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_SB_DEPTH));
		DISP_MSG("(+0E0)ROT_SB_BASE              =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_SB_BASE));
		DISP_MSG("(+0E8)ROT_SB_CON               =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_SB_CON));
		DISP_MSG("(+0F0)ROT_VC1_RANGE            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_VC1_RANGE));
		DISP_MSG("(+200)ROT_TRANSFORM_0          =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_TRANSFORM_0));
		DISP_MSG("(+208)ROT_TRANSFORM_1          =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_TRANSFORM_1));
		DISP_MSG("(+210)ROT_TRANSFORM_2          =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_TRANSFORM_2));
		DISP_MSG("(+218)ROT_TRANSFORM_3          =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_TRANSFORM_3));
		DISP_MSG("(+220)ROT_TRANSFORM_4          =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_TRANSFORM_4));
		DISP_MSG("(+228)ROT_TRANSFORM_5          =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_TRANSFORM_5));
		DISP_MSG("(+230)ROT_TRANSFORM_6          =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_TRANSFORM_6));
		DISP_MSG("(+238)ROT_TRANSFORM_7          =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_TRANSFORM_7));
		DISP_MSG("(+240)ROT_RESV_DUMMY_0         =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_RESV_DUMMY_0));
		DISP_MSG("(+300)ROT_CHKS_EXTR            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_CHKS_EXTR));
		DISP_MSG("(+308)ROT_CHKS_INTW            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_CHKS_INTW));
		DISP_MSG("(+310)ROT_CHKS_INTR            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_CHKS_INTR));
		DISP_MSG("(+318)ROT_CHKS_ROTO            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_CHKS_ROTO));
		DISP_MSG("(+320)ROT_CHKS_SRIY            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_CHKS_SRIY));
		DISP_MSG("(+328)ROT_CHKS_SRIU            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_CHKS_SRIU));
		DISP_MSG("(+330)ROT_CHKS_SRIV            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_CHKS_SRIV));
		DISP_MSG("(+338)ROT_CHKS_SROY            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_CHKS_SROY));
		DISP_MSG("(+340)ROT_CHKS_SROU            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_CHKS_SROU));
		DISP_MSG("(+348)ROT_CHKS_SROV            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_CHKS_SROV));
		DISP_MSG("(+350)ROT_CHKS_VUPI            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_CHKS_VUPI));
		DISP_MSG("(+358)ROT_CHKS_VUPO            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_CHKS_VUPO));
		DISP_MSG("(+380)ROT_DEBUG_CON            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_DEBUG_CON));
		DISP_MSG("(+400)ROT_MON_STA_0            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_0));
		DISP_MSG("(+408)ROT_MON_STA_1            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_1));
		DISP_MSG("(+410)ROT_MON_STA_2            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_2));
		DISP_MSG("(+418)ROT_MON_STA_3            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_3));
		DISP_MSG("(+420)ROT_MON_STA_4            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_4));
		DISP_MSG("(+428)ROT_MON_STA_5            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_5));
		DISP_MSG("(+430)ROT_MON_STA_6            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_6));
		DISP_MSG("(+438)ROT_MON_STA_7            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_7));
		DISP_MSG("(+440)ROT_MON_STA_8            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_8));
		DISP_MSG("(+448)ROT_MON_STA_9            =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_9));
		DISP_MSG("(+450)ROT_MON_STA_10           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_10));
		DISP_MSG("(+458)ROT_MON_STA_11           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_11));
		DISP_MSG("(+460)ROT_MON_STA_12           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_12));
		DISP_MSG("(+468)ROT_MON_STA_13           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_13));
		DISP_MSG("(+470)ROT_MON_STA_14           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_14));
		DISP_MSG("(+478)ROT_MON_STA_15           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_15));
		DISP_MSG("(+480)ROT_MON_STA_16           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_16));
		DISP_MSG("(+488)ROT_MON_STA_17           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_17));
		DISP_MSG("(+490)ROT_MON_STA_18           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_18));
		DISP_MSG("(+498)ROT_MON_STA_19           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_19));
		DISP_MSG("(+4A0)ROT_MON_STA_20           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_20));
		DISP_MSG("(+4A8)ROT_MON_STA_21           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_21));
		DISP_MSG("(+4B0)ROT_MON_STA_22           =0x%x\n",
			 DISP_REG_GET(DISP_REG_ROT_MON_STA_22));
		break;

	case DISP_MODULE_SCL:
		DISP_MSG("===== DISP SCL Reg Dump: ============\n");
		DISP_MSG("(+000)SCL_CTRL       =0x%x\n", DISP_REG_GET(DISP_REG_SCL_CTRL));
		DISP_MSG("(+004)SCL_INTEN      =0x%x\n", DISP_REG_GET(DISP_REG_SCL_INTEN));
		DISP_MSG("(+008)SCL_INTSTA     =0x%x\n", DISP_REG_GET(DISP_REG_SCL_INTSTA));
		DISP_MSG("(+00C)SCL_STATUS     =0x%x\n", DISP_REG_GET(DISP_REG_SCL_STATUS));
		DISP_MSG("(+010)SCL_CFG        =0x%x\n", DISP_REG_GET(DISP_REG_SCL_CFG));
		DISP_MSG("(+018)SCL_INP_CHKSUM =0x%x\n", DISP_REG_GET(DISP_REG_SCL_INP_CHKSUM));
		DISP_MSG("(+01C)SCL_OUTP_CHKSUM=0x%x\n", DISP_REG_GET(DISP_REG_SCL_OUTP_CHKSUM));
		DISP_MSG("(+020)SCL_HRZ_CFG    =0x%x\n", DISP_REG_GET(DISP_REG_SCL_HRZ_CFG));
		DISP_MSG("(+024)SCL_HRZ_SIZE   =0x%x\n", DISP_REG_GET(DISP_REG_SCL_HRZ_SIZE));
		DISP_MSG("(+028)SCL_HRZ_FACTOR =0x%x\n", DISP_REG_GET(DISP_REG_SCL_HRZ_FACTOR));
		DISP_MSG("(+02C)SCL_HRZ_OFFSET =0x%x\n", DISP_REG_GET(DISP_REG_SCL_HRZ_OFFSET));
		DISP_MSG("(+040)SCL_VRZ_CFG    =0x%x\n", DISP_REG_GET(DISP_REG_SCL_VRZ_CFG));
		DISP_MSG("(+044)SCL_VRZ_SIZE   =0x%x\n", DISP_REG_GET(DISP_REG_SCL_VRZ_SIZE));
		DISP_MSG("(+048)SCL_VRZ_FACTOR =0x%x\n", DISP_REG_GET(DISP_REG_SCL_VRZ_FACTOR));
		DISP_MSG("(+04C)SCL_VRZ_OFFSET =0x%x\n", DISP_REG_GET(DISP_REG_SCL_VRZ_OFFSET));
		DISP_MSG("(+060)SCL_EXT_COEF   =0x%x\n", DISP_REG_GET(DISP_REG_SCL_EXT_COEF));
		DISP_MSG("(+064)SCL_PEAK_CFG   =0x%x\n", DISP_REG_GET(DISP_REG_SCL_PEAK_CFG));
		break;

	case DISP_MODULE_OVL:
		DISP_MSG("===== DISP OVL Reg Dump: ============\n");
		DISP_MSG("(000)OVL_STA                    =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_STA));
		DISP_MSG("(004)OVL_INTEN                  =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_INTEN));
		DISP_MSG("(008)OVL_INTSTA                 =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_INTSTA));
		DISP_MSG("(00C)OVL_EN                     =0x%x\n", DISP_REG_GET(DISP_REG_OVL_EN));
		DISP_MSG("(010)OVL_TRIG                   =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_TRIG));
		DISP_MSG("(014)OVL_RST                    =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RST));
		DISP_MSG("(020)OVL_ROI_SIZE               =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_ROI_SIZE));
		DISP_MSG("(024)OVL_DATAPATH_CON           =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_DATAPATH_CON));
		DISP_MSG("(028)OVL_ROI_BGCLR              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_ROI_BGCLR));
		DISP_MSG("(02C)OVL_SRC_CON                =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_SRC_CON));
		DISP_MSG("(030)OVL_L0_CON                 =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L0_CON));
		DISP_MSG("(034)OVL_L0_SRCKEY              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L0_SRCKEY));
		DISP_MSG("(038)OVL_L0_SRC_SIZE            =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L0_SRC_SIZE));
		DISP_MSG("(03C)OVL_L0_OFFSET              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L0_OFFSET));
		DISP_MSG("(040)OVL_L0_ADDR                =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L0_ADDR));
		DISP_MSG("(044)OVL_L0_PITCH               =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L0_PITCH));
		DISP_MSG("(0C0)OVL_RDMA0_CTRL             =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA0_CTRL));
		DISP_MSG("(0C4)OVL_RDMA0_MEM_START_TRIG   =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_START_TRIG));
		DISP_MSG("(0C8)OVL_RDMA0_MEM_GMC_SETTING  =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING));
		DISP_MSG("(0CC)OVL_RDMA0_MEM_SLOW_CON     =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_SLOW_CON));
		DISP_MSG("(0D0)OVL_RDMA0_FIFO_CTRL        =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA0_FIFO_CTRL));
		DISP_MSG("(050)OVL_L1_CON                 =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L1_CON));
		DISP_MSG("(054)OVL_L1_SRCKEY              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L1_SRCKEY));
		DISP_MSG("(058)OVL_L1_SRC_SIZE            =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L1_SRC_SIZE));
		DISP_MSG("(05C)OVL_L1_OFFSET              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L1_OFFSET));
		DISP_MSG("(060)OVL_L1_ADDR                =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L1_ADDR));
		DISP_MSG("(064)OVL_L1_PITCH               =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L1_PITCH));
		DISP_MSG("(0E0)OVL_RDMA1_CTRL             =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA1_CTRL));
		DISP_MSG("(0E4)OVL_RDMA1_MEM_START_TRIG   =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_START_TRIG));
		DISP_MSG("(0E8)OVL_RDMA1_MEM_GMC_SETTING  =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING));
		DISP_MSG("(0EC)OVL_RDMA1_MEM_SLOW_CON     =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_SLOW_CON));
		DISP_MSG("(0F0)OVL_RDMA1_FIFO_CTRL        =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA1_FIFO_CTRL));
		DISP_MSG("(070)OVL_L2_CON                 =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L2_CON));
		DISP_MSG("(074)OVL_L2_SRCKEY              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L2_SRCKEY));
		DISP_MSG("(078)OVL_L2_SRC_SIZE            =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L2_SRC_SIZE));
		DISP_MSG("(07C)OVL_L2_OFFSET              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L2_OFFSET));
		DISP_MSG("(080)OVL_L2_ADDR                =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L2_ADDR));
		DISP_MSG("(084)OVL_L2_PITCH               =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L2_PITCH));
		DISP_MSG("(100)OVL_RDMA2_CTRL             =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA2_CTRL));
		DISP_MSG("(104)OVL_RDMA2_MEM_START_TRIG   =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_START_TRIG));
		DISP_MSG("(108)OVL_RDMA2_MEM_GMC_SETTING  =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING));
		DISP_MSG("(10C)OVL_RDMA2_MEM_SLOW_CON     =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_SLOW_CON));
		DISP_MSG("(110)OVL_RDMA2_FIFO_CTRL        =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA2_FIFO_CTRL));
		DISP_MSG("(090)OVL_L3_CON                 =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L3_CON));
		DISP_MSG("(094)OVL_L3_SRCKEY              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L3_SRCKEY));
		DISP_MSG("(098)OVL_L3_SRC_SIZE            =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L3_SRC_SIZE));
		DISP_MSG("(09C)OVL_L3_OFFSET              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L3_OFFSET));
		DISP_MSG("(0A0)OVL_L3_ADDR                =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L3_ADDR));
		DISP_MSG("(0A4)OVL_L3_PITCH               =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_L3_PITCH));
		DISP_MSG("(120)OVL_RDMA3_CTRL             =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA3_CTRL));
		DISP_MSG("(124)OVL_RDMA3_MEM_START_TRIG   =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_START_TRIG));
		DISP_MSG("(128)OVL_RDMA3_MEM_GMC_SETTING  =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING));
		DISP_MSG("(12C)OVL_RDMA3_MEM_SLOW_CON     =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_SLOW_CON));
		DISP_MSG("(130)OVL_RDMA3_FIFO_CTRL        =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA3_FIFO_CTRL));
		DISP_MSG("(1C4)OVL_DEBUG_MON_SEL          =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_DEBUG_MON_SEL));
		DISP_MSG("(1C4)OVL_RDMA0_MEM_GMC_SETTING2 =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING2));
		DISP_MSG("(1C8)OVL_RDMA1_MEM_GMC_SETTING2 =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING2));
		DISP_MSG("(1CC)OVL_RDMA2_MEM_GMC_SETTING2 =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING2));
		DISP_MSG("(1D0)OVL_RDMA3_MEM_GMC_SETTING2 =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING2));
		DISP_MSG("(240)OVL_FLOW_CTRL_DBG          =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_FLOW_CTRL_DBG));
		DISP_MSG("(244)OVL_ADDCON_DBG             =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_ADDCON_DBG));
		DISP_MSG("(248)OVL_OUTMUX_DBG             =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_OUTMUX_DBG));
		DISP_MSG("(24C)OVL_RDMA0_DBG              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA0_DBG));
		DISP_MSG("(250)OVL_RDMA1_DBG              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA1_DBG));
		DISP_MSG("(254)OVL_RDMA2_DBG              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA2_DBG));
		DISP_MSG("(258)OVL_RDMA3_DBG              =0x%x\n",
			 DISP_REG_GET(DISP_REG_OVL_RDMA3_DBG));
		break;


	case DISP_MODULE_COLOR:
		DISP_MSG("===== DISP Color Reg Dump: ============\n");
		DISP_MSG("(CFG_MAIN)=0x%x\n", DISP_REG_GET(CFG_MAIN));	/* color all bypass */
		DISP_MSG("(0x420)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0x420)));
		DISP_MSG("(0xf00)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf00)));
		DISP_MSG("(0xf04)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf04)));
		DISP_MSG("(0xf64)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf64)));
		DISP_MSG("(0xf68)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf68)));
		DISP_MSG("(0xf6c)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf6c)));
		DISP_MSG("(0xf70)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf70)));
		DISP_MSG("(0xf74)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf74)));
		DISP_MSG("(0xf78)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf78)));
		DISP_MSG("(0xf7c)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf7c)));
		DISP_MSG("(0xf80)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf80)));
		DISP_MSG("(0xf84)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf84)));
		DISP_MSG("(0xf88)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf88)));
		DISP_MSG("(0xf8c)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf8c)));
		DISP_MSG("(0xf90)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf90)));
		DISP_MSG("(0xf94)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf94)));
		DISP_MSG("(0xf98)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf98)));
		DISP_MSG("(0xf9c)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf9c)));
		DISP_MSG("(0xfa0)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfa0)));
		DISP_MSG("(0xfa4)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfa4)));
		DISP_MSG("(0xfa8)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfa8)));
		DISP_MSG("(0xfac)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfac)));
		DISP_MSG("(0xfb0)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfb0)));
		DISP_MSG("(0xfb4)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfb4)));
		DISP_MSG("(0xfb8)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfb8)));
		DISP_MSG("(0xfbc)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfbc)));
		DISP_MSG("(0xfc0)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfc0)));
		DISP_MSG("(0xfc4)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfc4)));
		DISP_MSG("(0xfc8)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfc8)));
		DISP_MSG("(0xfcc)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfcc)));
		DISP_MSG("(0xfd0)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfd0)));
		DISP_MSG("(0xfd4)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfd4)));
		DISP_MSG("(0xfd8)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfd8)));
		DISP_MSG("(0xfdc)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfdc)));
		DISP_MSG("(0x428)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0x428)));
		DISP_MSG("(0x42c)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0x42c)));
		DISP_MSG("(0xf50)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf50)));
		DISP_MSG("(0xf54)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf54)));
		DISP_MSG("(0x40C)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0x40C)));
		DISP_MSG("(0xf60)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xf60)));
		DISP_MSG("(0xfa0)=0x%x\n", DISP_REG_GET((DISPSYS_COLOR_BASE + 0xfa0)));
		DISP_MSG("(G_PIC_ADJ_MAIN_2)=0x%x\n", DISP_REG_GET(G_PIC_ADJ_MAIN_2));
		for (index = 0; index < 8; index++) {
			DISP_MSG("(Y_SLOPE_1_0_MAIN)=0x%x\n",
				 DISP_REG_GET(Y_SLOPE_1_0_MAIN + 4 * index));
		}
		for (index = 0; index < 7; index++) {
			DISP_MSG("(LOCAL_HUE_CD_0)=0x%x\n",
				 DISP_REG_GET(LOCAL_HUE_CD_0 + 4 * index));
		}
		/* color window */
		DISP_MSG("(DISPSYS_COLOR_BASE)=0x%x\n",
			 DISP_REG_GET((DISPSYS_COLOR_BASE + 0x740)));



		break;

	case DISP_MODULE_TDSHP:
		break;
	case DISP_MODULE_BLS:
		DISP_MSG("===== DISP BLS Reg Dump: ============\n");
		DISP_MSG("(0x%08X)BLS_EN                =0x%x\n", DISP_REG_BLS_EN,
			 DISP_REG_GET(DISP_REG_BLS_EN));
		DISP_MSG("(0x%08X)BLS_SETTING           =0x%x\n", DISP_REG_BLS_BLS_SETTING,
			 DISP_REG_GET(DISP_REG_BLS_BLS_SETTING));
		DISP_MSG("(0x%08X)BLS_INTEN             =0x%x\n", DISP_REG_BLS_INTEN,
			 DISP_REG_GET(DISP_REG_BLS_INTEN));
		DISP_MSG("(0x%08X)BLS_INTSTA            =0x%x\n", DISP_REG_BLS_INTSTA,
			 DISP_REG_GET(DISP_REG_BLS_INTSTA));
		DISP_MSG("(0x%08X)BLS_SRC_SIZE          =0x%x\n", DISP_REG_BLS_SRC_SIZE,
			 DISP_REG_GET(DISP_REG_BLS_SRC_SIZE));
		DISP_MSG("(0x%08X)BLS_PWM_DUTY          =0x%x\n", DISP_REG_BLS_PWM_DUTY,
			 DISP_REG_GET(DISP_REG_BLS_PWM_DUTY));
		DISP_MSG("(0x%08X)BLS_PWM_DUTY_GAIN     =0x%x\n", DISP_REG_BLS_PWM_DUTY_GAIN,
			 DISP_REG_GET(DISP_REG_BLS_PWM_DUTY_GAIN));
		DISP_MSG("(0x%08X)BLS_PWM_CON           =0x%x\n", DISP_REG_BLS_PWM_CON,
			 DISP_REG_GET(DISP_REG_BLS_PWM_CON));
		DISP_MSG("(0x%08X)PWM_WAVE_NUM          =0x%x\n", DISP_REG_PWM_WAVE_NUM,
			 DISP_REG_GET(DISP_REG_PWM_WAVE_NUM));
		DISP_MSG("(0x%08X)PWM_SEND_WAVENUM      =0x%x\n", DISP_REG_PWM_SEND_WAVENUM,
			 DISP_REG_GET(DISP_REG_PWM_SEND_WAVENUM));
		break;

	case DISP_MODULE_WDMA0:
	case DISP_MODULE_WDMA1:
		if (DISP_MODULE_WDMA0 == module) {
			index = 0;
		} else if (DISP_MODULE_WDMA1 == module) {
			index = 1;
		}
		DISP_MSG("===== DISP WDMA%d Reg Dump: ============\n", index);
		DISP_MSG("(000)WDMA_INTEN         =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_INTEN + 0x1000 * index));
		DISP_MSG("(004)WDMA_INTSTA        =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_INTSTA + 0x1000 * index));
		DISP_MSG("(008)WDMA_EN            =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_EN + 0x1000 * index));
		DISP_MSG("(00C)WDMA_RST           =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_RST + 0x1000 * index));
		DISP_MSG("(010)WDMA_SMI_CON       =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_SMI_CON + 0x1000 * index));
		DISP_MSG("(014)WDMA_CFG           =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_CFG + 0x1000 * index));
		DISP_MSG("(018)WDMA_SRC_SIZE      =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_SRC_SIZE + 0x1000 * index));
		DISP_MSG("(01C)WDMA_CLIP_SIZE     =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE + 0x1000 * index));
		DISP_MSG("(020)WDMA_CLIP_COORD    =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_CLIP_COORD + 0x1000 * index));
		DISP_MSG("(024)WDMA_DST_ADDR      =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_DST_ADDR + 0x1000 * index));
		DISP_MSG("(028)WDMA_DST_W_IN_BYTE =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_DST_W_IN_BYTE + 0x1000 * index));
		DISP_MSG("(02C)WDMA_ALPHA         =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_ALPHA + 0x1000 * index));
		DISP_MSG("(030)WDMA_BUF_ADDR      =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_BUF_ADDR + 0x1000 * index));
		DISP_MSG("(034)WDMA_STA           =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_STA + 0x1000 * index));
		DISP_MSG("(038)WDMA_BUF_CON1      =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_BUF_CON1 + 0x1000 * index));
		DISP_MSG("(03C)WDMA_BUF_CON2      =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_BUF_CON2 + 0x1000 * index));
		DISP_MSG("(040)WDMA_C00           =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_C00 + 0x1000 * index));
		DISP_MSG("(044)WDMA_C02           =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_C02 + 0x1000 * index));
		DISP_MSG("(048)WDMA_C10           =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_C10 + 0x1000 * index));
		DISP_MSG("(04C)WDMA_C12           =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_C12 + 0x1000 * index));
		DISP_MSG("(050)WDMA_C20           =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_C20 + 0x1000 * index));
		DISP_MSG("(054)WDMA_C22           =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_C22 + 0x1000 * index));
		DISP_MSG("(058)WDMA_PRE_ADD0      =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_PRE_ADD0 + 0x1000 * index));
		DISP_MSG("(05C)WDMA_PRE_ADD2      =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_PRE_ADD2 + 0x1000 * index));
		DISP_MSG("(060)WDMA_POST_ADD0     =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_POST_ADD0 + 0x1000 * index));
		DISP_MSG("(064)WDMA_POST_ADD2     =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_POST_ADD2 + 0x1000 * index));
		DISP_MSG("(070)WDMA_DST_U_ADDR    =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_DST_U_ADDR + 0x1000 * index));
		DISP_MSG("(074)WDMA_DST_V_ADDR    =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_DST_V_ADDR + 0x1000 * index));
		DISP_MSG("(078)WDMA_DST_UV_PITCH  =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_DST_UV_PITCH + 0x1000 * index));
		DISP_MSG("(090)WDMA_DITHER_CON    =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_DITHER_CON + 0x1000 * index));
		DISP_MSG("(0A0)WDMA_FLOW_CTRL_DBG =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_FLOW_CTRL_DBG + 0x1000 * index));
		DISP_MSG("(0A4)WDMA_EXEC_DBG      =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_EXEC_DBG + 0x1000 * index));
		DISP_MSG("(0A8)WDMA_CLIP_DBG      =0x%x\n",
			 DISP_REG_GET(DISP_REG_WDMA_CLIP_DBG + 0x1000 * index));
		break;

	case DISP_MODULE_RDMA0:
	case DISP_MODULE_RDMA1:
		if (DISP_MODULE_RDMA0 == module) {
			index = 0;
		} else if (DISP_MODULE_RDMA1 == module) {
			index = 1;
		}

		break;

	case DISP_MODULE_CMDQ:
		DISP_MSG("===== DISP CMDQ Reg Dump: ============\n");
		DISP_MSG("(0x10)CMDQ_IRQ_FLAG          =0x%x\n",
			 DISP_REG_GET(DISP_REG_CMDQ_IRQ_FLAG));
		DISP_MSG("(0x20)CMDQ_LOADED_THR        =0x%x\n",
			 DISP_REG_GET(DISP_REG_CMDQ_LOADED_THR));
		DISP_MSG("(0x30)CMDQ_THR_SLOT_CYCLES   =0x%x\n",
			 DISP_REG_GET(DISP_REG_CMDQ_THR_SLOT_CYCLES));
		DISP_MSG("(0x40)CMDQ_BUS_CTRL          =0x%x\n",
			 DISP_REG_GET(DISP_REG_CMDQ_BUS_CTRL));
		DISP_MSG("(0x50)CMDQ_ABORT             =0x%x\n",
			 DISP_REG_GET(DISP_REG_CMDQ_ABORT));
		for (index = 0; index < CMDQ_THREAD_NUM; index++) {
			DISP_MSG("(0x%x)CMDQ_THRx_RESET%d                =0x%x\n",
				 (0x100 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_RESET(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_EN%d                   =0x%x\n",
				 (0x104 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_EN(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_SUSPEND%d              =0x%x\n",
				 (0x108 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_SUSPEND(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_STATUS%d               =0x%x\n",
				 (0x10c + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_STATUS(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_IRQ_FLAG%d             =0x%x\n",
				 (0x110 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_IRQ_FLAG(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_IRQ_FLAG_EN%d          =0x%x\n",
				 (0x114 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_IRQ_FLAG_EN(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_SECURITY%d             =0x%x\n",
				 (0x118 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_SECURITY(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_PC%d                   =0x%x\n",
				 (0x120 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_PC(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_END_ADDR%d             =0x%x\n",
				 (0x124 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_END_ADDR(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_EXEC_CMDS_CNT%d        =0x%x\n",
				 (0x128 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_EXEC_CMDS_CNT(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_WAIT_EVENTS0%d         =0x%x\n",
				 (0x130 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_WAIT_EVENTS0(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_WAIT_EVENTS1%d         =0x%x\n",
				 (0x134 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_WAIT_EVENTS1(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_OBSERVED_EVENTS0%d     =0x%x\n",
				 (0x140 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_OBSERVED_EVENTS0(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_OBSERVED_EVENTS1%d     =0x%x\n",
				 (0x144 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_OBSERVED_EVENTS1(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_OBSERVED_EVENTS0_CLR%d =0x%x\n",
				 (0x148 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_OBSERVED_EVENTS0_CLR(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_OBSERVED_EVENTS1_CLR%d =0x%x\n",
				 (0x14c + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_OBSERVED_EVENTS1_CLR(index)));
			DISP_MSG("(0x%x)CMDQ_THRx_INSTN_TIMEOUT_CYCLES%d =0x%x\n",
				 (0x150 + 0x80 * index), index,
				 DISP_REG_GET(DISP_REG_CMDQ_THRx_INSTN_TIMEOUT_CYCLES(index)));
		}
		break;


	case DISP_MODULE_GAMMA:
		break;
	case DISP_MODULE_DBI:
		break;
	case DISP_MODULE_DSI:
	case DISP_MODULE_DSI_VDO:
	case DISP_MODULE_DSI_CMD:
		/*
		   DISP_MSG("---------- Start dump DSI registers ----------\n");

		   for (index = 0; index < sizeof(DSI_REGS); index += 4)
		   {
		   DISP_MSG( "DSI+%04x : 0x%08x\n", index, DISP_REG_GET(DSI_BASE + index));
		   }

		   for (index = 0; index < sizeof(DSI_CMDQ_REGS); index += 4)
		   {
		   DISP_MSG("DSI_CMD+%04x(%p) : 0x%08x\n", index, (UINT32*)(DSI_BASE+0x180+index), DISP_REG_GET((DSI_BASE+0x180+index)));
		   }

		   for (index = 0; index < sizeof(DSI_PHY_REGS); index += 4)
		   {
		   DISP_MSG("DSI_PHY+%04x(%p) : 0x%08x\n", index, (UINT32*)(MIPI_CONFIG_BASE+0x800+index), DISP_REG_GET((MIPI_CONFIG_BASE+0x800+index)));
		   }

		 */
		break;
	case DISP_MODULE_DPI1:
		break;

	case DISP_MODULE_DPI0:
		/*
		   DISP_MSG("===== DISP DPI0 Reg Dump: ============\n");
		   DISP_MSG("(0x00)DPI0_EN_REG               = 0x%x\n", *DPI0_EN_REG               );
		   DISP_MSG("(0x04)DPI0_RST_REG              = 0x%x\n", *DPI0_RST_REG              );
		   DISP_MSG("(0x08)DPI0_INTEN_REG            = 0x%x\n", *DPI0_INTEN_REG            );
		   DISP_MSG("(0x0C)DPI0_INTSTA_REG           = 0x%x\n", *DPI0_INTSTA_REG           );
		   DISP_MSG("(0x10)DPI0_CON_REG              = 0x%x\n", *DPI0_CON_REG              );
		   DISP_MSG("(0x14)DPI0_CLKCON_REG           = 0x%x\n", *DPI0_CLKCON_REG           );
		   DISP_MSG("(0x18)DPI0_SIZE_REG             = 0x%x\n", *DPI0_SIZE_REG             );
		   DISP_MSG("(0x1C)DPI0_TGEN_HWIDTH_REG      = 0x%x\n", *DPI0_TGEN_HWIDTH_REG      );
		   DISP_MSG("(0x20)DPI0_TGEN_HPORCH_REG      = 0x%x\n", *DPI0_TGEN_HPORCH_REG      );
		   DISP_MSG("(0x24)DPI0_TGEN_VWIDTH_LODD_REG = 0x%x\n", *DPI0_TGEN_VWIDTH_LODD_REG );
		   DISP_MSG("(0x28)DPI0_TGEN_VPORCH_LODD_REG = 0x%x\n", *DPI0_TGEN_VPORCH_LODD_REG );
		   DISP_MSG("(0x2C)DPI0_TGEN_WWIDTH_LEVEN_REG= 0x%x\n", *DPI0_TGEN_WWIDTH_LEVEN_REG);
		   DISP_MSG("(0x30)DPI0_TGEN_VPORCH_LEVEN_REG= 0x%x\n", *DPI0_TGEN_VPORCH_LEVEN_REG);
		   DISP_MSG("(0x34)DPI0_TGEN_VWIDTH_RODD_REG = 0x%x\n", *DPI0_TGEN_VWIDTH_RODD_REG );
		   DISP_MSG("(0x38)DPI0_TGEN_VPORCH_RODD_REG = 0x%x\n", *DPI0_TGEN_VPORCH_RODD_REG );
		   DISP_MSG("(0x50)DPI0_BG_HCNTL_REG         = 0x%x\n", *DPI0_BG_HCNTL_REG         );
		   DISP_MSG("(0x54)DPI0_BG_VCNTL_REG         = 0x%x\n", *DPI0_BG_VCNTL_REG         );
		   DISP_MSG("(0x60)DPI0_STATUS_REG           = 0x%x\n", *DPI0_STATUS_REG           );
		   DISP_MSG("(0x64)DPI0_MATRIX_COEFF_SET0_REG= 0x%x\n", *DPI0_MATRIX_COEFF_SET0_REG);
		   DISP_MSG("(0x68)DPI0_MATRIX_COEFF_SET1_REG= 0x%x\n", *DPI0_MATRIX_COEFF_SET1_REG);
		   DISP_MSG("(0x6C)DPI0_MATRIX_COEFF_SET2_REG= 0x%x\n", *DPI0_MATRIX_COEFF_SET2_REG);
		   DISP_MSG("(0X8C)DPI0_Y_LIMIT_REG          = 0x%x\n", *DPI0_Y_LIMIT_REG          );
		   DISP_MSG("(0x90)DPI0_C_LIMIT_REG          = 0x%x\n", *DPI0_C_LIMIT_REG          );
		 */
		break;

	case DISP_MODULE_MUTEX0:
	case DISP_MODULE_MUTEX1:
	case DISP_MODULE_MUTEX2:
	case DISP_MODULE_MUTEX3:
		DISP_MSG("===== DISP Mutex%d Reg Dump: ============\n",
			 module - DISP_MODULE_MUTEX0);
		DISP_MSG("((0x0) CFG_MUTEX_INTEN    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTEN));
		DISP_MSG("((0x4) CFG_MUTEX_INTSTA   =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA));
		DISP_MSG("((0x8) CFG_REG_UPD_TIMEOUT=0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_REG_UPD_TIMEOUT));
		DISP_MSG("((0xC) CFG_REG_COMMIT     =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_REG_COMMIT));
		index = module - DISP_MODULE_MUTEX0;
		{
			DISP_MSG("(0x%x)CFG_MUTEX_EN(%d)  =0x%x\n", 0x20 + (0x20 * index), index,
				 DISP_REG_GET(DISP_REG_CONFIG_MUTEX_EN(index)));
			DISP_MSG("(0x%x)CFG_MUTEX(%d)     =0x%x\n", 0x24 + (0x20 * index), index,
				 DISP_REG_GET(DISP_REG_CONFIG_MUTEX(index)));
			DISP_MSG("(0x%x)CFG_MUTEX_RST(%d) =0x%x\n", 0x28 + (0x20 * index), index,
				 DISP_REG_GET(DISP_REG_CONFIG_MUTEX_RST(index)));
			DISP_MSG("(0x%x)CFG_MUTEX_MOD(%d) =0x%x\n", 0x2C + (0x20 * index), index,
				 DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(index)));
			DISP_MSG("(0x%x)CFG_MUTEX_SOF(%d) =0x%x\n", 0x30 + (0x20 * index), index,
				 DISP_REG_GET(DISP_REG_CONFIG_MUTEX_SOF(index)));
		}
		break;


	case DISP_MODULE_CONFIG:
	case DISP_MODULE_MUTEX:
		DISP_MSG("(0x020)CFG_SCL_MOUT_EN    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_SCL_MOUT_EN));
		DISP_MSG("(0x024)CFG_OVL_MOUT_EN    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_OVL_MOUT_EN));
		DISP_MSG("(0x028)CFG_COLOR_MOUT_EN  =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_COLOR_MOUT_EN));
		DISP_MSG("(0x02C)CFG_TDSHP_MOUT_EN  =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_TDSHP_MOUT_EN));
		DISP_MSG("(0x030)CFG_MOUT_RST       =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MOUT_RST));
		DISP_MSG("(0x034)CFG_RDMA0_OUT_SEL  =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_RDMA0_OUT_SEL));
		DISP_MSG("(0x038)CFG_RDMA1_OUT_SEL  =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_RDMA1_OUT_SEL));
		DISP_MSG("(0x03C)CFG_OVL_PQ_OUT_SEL =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_OVL_PQ_OUT_SEL));
		DISP_MSG("(0x040)CFG_WDMA0_SEL      =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_WDMA0_SEL));
		DISP_MSG("(0x044)CFG_OVL_SEL        =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_OVL_SEL));
		DISP_MSG("(0x048)CFG_OVL_PQ_IN_SEL  =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_OVL_PQ_IN_SEL));
		DISP_MSG("(0x04C)CFG_COLOR_SEL      =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_COLOR_SEL));
		DISP_MSG("(0x050)CFG_TDSHP_SEL      =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_TDSHP_SEL));
		DISP_MSG("(0x054)CFG_BLS_SEL        =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_BLS_SEL));
		DISP_MSG("(0x058)CFG_DBI_SEL        =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_DBI_SEL));
		DISP_MSG("(0x05C)CFG_DPI0_SEL       =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_DPI0_SEL));
		DISP_MSG("(0x60))CFG_MISC           =0x%x\n", DISP_REG_GET(DISP_REG_CONFIG_MISC));
		DISP_MSG("(0x70))CFG_PATH_DEBUG0    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_PATH_DEBUG0));
		DISP_MSG("(0x74))CFG_PATH_DEBUG1    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_PATH_DEBUG1));
		DISP_MSG("(0x78))CFG_PATH_DEBUG2    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_PATH_DEBUG2));
		DISP_MSG("(0x7C))CFG_PATH_DEBUG3    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_PATH_DEBUG3));
		DISP_MSG("(0x80))CFG_PATH_DEBUG4    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_PATH_DEBUG4));
		DISP_MSG("(0x100)CFG_CG_CON0        =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_CG_CON0));
		DISP_MSG("(0x104)CFG_CG_SET0        =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_CG_SET0));
		DISP_MSG("(0x108)CFG_CG_CLR0        =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_CG_CLR0));
		DISP_MSG("(0x110)CFG_CG_CON1        =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_CG_CON1));
		DISP_MSG("(0x114)CFG_CG_SET1        =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_CG_SET1));
		DISP_MSG("(0x118)CFG_CG_CLR1        =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_CG_CLR1));
		DISP_MSG("(0x120)CFG_HW_DCM_EN0     =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_HW_DCM_EN0));
		DISP_MSG("(0x124)CFG_HW_DCM_EN_SET0 =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_HW_DCM_EN_SET0));
		DISP_MSG("(0x128)CFG_HW_DCM_EN_CLR0 =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_HW_DCM_EN_CLR0));
		DISP_MSG("(0x130)CFG_HW_DCM_EN1     =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_HW_DCM_EN1));
		DISP_MSG("(0x134)CFG_HW_DCM_EN_SET1 =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_HW_DCM_EN_SET1));
		DISP_MSG("(0x138)CFG_HW_DCM_EN_CLR1 =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_HW_DCM_EN_CLR1));
		DISP_MSG("(0x800)CFG_MBIST_DONE0    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_DONE0));
		DISP_MSG("(0x804)CFG_MBIST_DONE1    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_DONE1));
		DISP_MSG("(0x808)CFG_MBIST_FAIL0    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_FAIL0));
		DISP_MSG("(0x80C)CFG_MBIST_FAIL1    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_FAIL1));
		DISP_MSG("(0x810)CFG_MBIST_FAIL2    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_FAIL2));
		DISP_MSG("(0x814)CFG_MBIST_HOLDB0   =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_HOLDB0));
		DISP_MSG("(0x818)CFG_MBIST_HOLDB1   =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_HOLDB1));
		DISP_MSG("(0x81C)CFG_MBIST_MODE0    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_MODE0));
		DISP_MSG("(0x820)CFG_MBIST_MODE1    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_MODE1));
		DISP_MSG("(0x824)CFG_MBIST_BSEL0    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_BSEL0));
		DISP_MSG("(0x828)CFG_MBIST_BSEL1    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_BSEL1));
		DISP_MSG("(0x82C)CFG_MBIST_BSEL2    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_BSEL2));
		DISP_MSG("(0x830)CFG_MBIST_BSEL3    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_BSEL3));
		DISP_MSG("(0x834)CFG_MBIST_CON      =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MBIST_CON));
		DISP_MSG("(0x870)CFG_DEBUG_OUT_SEL  =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_DEBUG_OUT_SEL));
		DISP_MSG("(0x874)CFG_TEST_CLK_SEL   =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_TEST_CLK_SEL));
		DISP_MSG("(0x880)CFG_DUMMY          =0x%x\n", DISP_REG_GET(DISP_REG_CONFIG_DUMMY));
		DISP_MSG("((0x0) CFG_MUTEX_INTEN    =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTEN));
		DISP_MSG("((0x4) CFG_MUTEX_INTSTA   =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA));
		DISP_MSG("((0x8) CFG_REG_UPD_TIMEOUT=0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_REG_UPD_TIMEOUT));
		DISP_MSG("((0xC) CFG_REG_COMMIT     =0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_REG_COMMIT));
		for (index = 0; index < 5; index++) {
			DISP_MSG("(0x%x)CFG_MUTEX_EN(%d)  =0x%x\n", 0x20 + (0x20 * index), index,
				 DISP_REG_GET(DISP_REG_CONFIG_MUTEX_EN(index)));
			DISP_MSG("(0x%x)CFG_MUTEX(%d)     =0x%x\n", 0x24 + (0x20 * index), index,
				 DISP_REG_GET(DISP_REG_CONFIG_MUTEX(index)));
			DISP_MSG("(0x%x)CFG_MUTEX_RST(%d) =0x%x\n", 0x28 + (0x20 * index), index,
				 DISP_REG_GET(DISP_REG_CONFIG_MUTEX_RST(index)));
			DISP_MSG("(0x%x)CFG_MUTEX_MOD(%d) =0x%x\n", 0x2C + (0x20 * index), index,
				 DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(index)));
			DISP_MSG("(0x%x)CFG_MUTEX_SOF(%d) =0x%x\n", 0x30 + (0x20 * index), index,
				 DISP_REG_GET(DISP_REG_CONFIG_MUTEX_SOF(index)));
		}
		break;

	case DISP_MODULE_G2D:
		DISP_MSG("(0x000)DISP_REG_G2D_START    =0x%x\n", DISP_REG_GET(DISP_REG_G2D_START));
		DISP_MSG("(0x004)DISP_REG_G2D_MODE_CON    =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_MODE_CON));
		DISP_MSG("(0x008)DISP_REG_G2D_RESET  =0x%x\n", DISP_REG_GET(DISP_REG_G2D_RESET));
		DISP_MSG("(0x00C)DISP_REG_G2D_STATUS  =0x%x\n", DISP_REG_GET(DISP_REG_G2D_STATUS));
		DISP_MSG("(0x010)DISP_REG_G2D_IRQ       =0x%x\n", DISP_REG_GET(DISP_REG_G2D_IRQ));
		DISP_MSG("(0x018)DISP_REG_G2D_ALP_CON  =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_ALP_CON));
		DISP_MSG("(0x040)DISP_REG_G2D_W2M_CON      =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_W2M_CON));
		DISP_MSG("(0x044)DISP_REG_G2D_W2M_ADDR        =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_W2M_ADDR));
		DISP_MSG("(0x048)DISP_REG_G2D_W2M_PITCH  =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_W2M_PITCH));
		DISP_MSG("(0x050)DISP_REG_G2D_W2M_SIZE      =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_W2M_SIZE));
		DISP_MSG("(0x080)DISP_REG_G2D_DST_CON        =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_DST_CON));
		DISP_MSG("(0x084)DISP_REG_G2D_DST_ADDR        =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_DST_ADDR));
		DISP_MSG("(0x088)DISP_REG_G2D_DST_PITCH       =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_DST_PITCH));
		DISP_MSG("(0x94))DISP_REG_G2D_DST_COLOR           =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_DST_COLOR));
		DISP_MSG("(0xC0))DISP_REG_G2D_SRC_CON    =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_SRC_CON));
		DISP_MSG("(0xC4))DISP_REG_G2D_SRC_ADDR    =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_SRC_ADDR));
		DISP_MSG("(0xC8))DISP_REG_G2D_SRC_PITCH    =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_SRC_PITCH));
		DISP_MSG("(0xD4))DISP_REG_G2D_SRC_COLOR    =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_SRC_COLOR));
		DISP_MSG("(0xD8))DISP_REG_G2D_DI_MAT_0    =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_DI_MAT_0));
		DISP_MSG("(0xDC)DISP_REG_G2D_DI_MAT_1        =0x%x\n",
			 DISP_REG_GET(DISP_REG_G2D_DI_MAT_1));
		DISP_MSG("(0xE0)DISP_REG_G2D_PMC        =0x%x\n", DISP_REG_GET(DISP_REG_G2D_PMC));

		break;

	case DISP_MODULE_SMI:
		DISP_MSG(" 0xf4010000= 0x%x\n", *(volatile unsigned int *)0xf4010000);
		DISP_MSG(" 0xf4010010= 0x%x\n", *(volatile unsigned int *)0xf4010010);
		DISP_MSG(" 0xF4010450= 0x%x\n", *(volatile unsigned int *)0xF4010450);
		DISP_MSG(" 0xF4010454= 0x%x\n", *(volatile unsigned int *)0xF4010454);
		DISP_MSG(" 0xF4010600= 0x%x\n", *(volatile unsigned int *)0xF4010600);
		DISP_MSG(" 0xF4010604= 0x%x\n", *(volatile unsigned int *)0xF4010604);
		DISP_MSG(" 0xF4010608= 0x%x\n", *(volatile unsigned int *)0xF4010608);
		DISP_MSG(" 0xF4010610= 0x%x\n", *(volatile unsigned int *)0xF4010610);
		DISP_MSG(" 0xF4010614= 0x%x\n", *(volatile unsigned int *)0xF4010614);
		DISP_MSG(" 0xF4010618= 0x%x\n", *(volatile unsigned int *)0xF4010618);
		DISP_MSG(" 0xf4000100= 0x%x\n", *(volatile unsigned int *)0xf4000100);
		DISP_MSG(" 0xf4000110= 0x%x\n", *(volatile unsigned int *)0xf4000110);
		DISP_MSG(" 0xf4000120= 0x%x\n", *(volatile unsigned int *)0xf4000120);
		DISP_MSG(" 0xf4000130= 0x%x\n", *(volatile unsigned int *)0xf4000130);
		DISP_MSG(" 0xf4000080= 0x%x\n", *(volatile unsigned int *)0xf4000080);
		DISP_MSG(" 0xf0202400= 0x%x\n", *(volatile unsigned int *)0xf0202400);
		DISP_MSG(" 0xf0202404= 0x%x\n", *(volatile unsigned int *)0xf0202404);
		DISP_MSG(" 0xf0202414= 0x%x\n", *(volatile unsigned int *)0xf0202414);
		DISP_MSG(" 0xF0202408= 0x%x\n", *(volatile unsigned int *)0xF0202408);
		DISP_MSG(" 0xF020240C= 0x%x\n", *(volatile unsigned int *)0xF020240C);
		DISP_MSG(" 0xF0202410= 0x%x\n", *(volatile unsigned int *)0xF0202410);
		break;

	default:
		DISP_MSG("disp_dump_reg() invalid module id=%d\n", module);

	}

	return 0;
}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
/* the API for other code to acquire ddp / ddp_mem session handle */
/* a NULL handle is returned when it fails */
KREE_SESSION_HANDLE ddp_session_handle(void)
{
	DISP_DBG("ddp_session_handle() acquire TEE session\n");
	/* TODO: the race condition here is not taken into consideration. */
	if (NULL == ddp_session) {
		TZ_RESULT ret;
		DISP_DBG("ddp_session_handle() create session\n");

		ret = KREE_CreateSession(TZ_TA_DDP_UUID, &ddp_session);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("ddp_session_handle() failed to create session, ret=%d\n", ret);
			return NULL;
		}
	}

	DISP_DBG("ddp_session_handle() session=%x\n", (unsigned int)ddp_session);
	return ddp_session;
}

KREE_SESSION_HANDLE ddp_mem_session_handle(void)
{
	DISP_DBG("ddp_mem_session_handle() acquires TEE memory session\n");
	/* TODO: the race condition here is not taken into consideration. */
	if (NULL == ddp_mem_session) {
		TZ_RESULT ret;
		DISP_DBG("ddp_mem_session_handle() create memory session\n");

		ret = KREE_CreateSession(TZ_TA_MEM_UUID, &ddp_mem_session);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("ddp_mem_session_handle() failed to create session: ret=%d\n",
				 ret);
			return NULL;
		}
	}

	DISP_DBG("ddp_mem_session_handle() session=%x\n", (unsigned int)ddp_mem_session);
	return ddp_mem_session;
}
#endif				/* CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT */
module_init(disp_init);
module_exit(disp_exit);
MODULE_AUTHOR("Tzu-Meng, Chung <Tzu-Meng.Chung@mediatek.com>");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");
