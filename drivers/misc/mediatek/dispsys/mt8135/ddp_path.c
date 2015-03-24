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

#include <linux/xlog.h>
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
#include "ddp_bls.h"
#include "ddp_rdma.h"
#include "ddp_wdma.h"
#include "ddp_ovl.h"
#include "ddp_tdshp.h"
#include "ddp_color.h"

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#include <tz_cross/trustzone.h>
#include <tz_cross/tz_ddp.h>
#include <mach/m4u_port.h>
#include <tz_cross/ta_mem.h>
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#endif

unsigned int gMutexID = 0;
unsigned int gTdshpStatus[OVL_LAYER_NUM] = { 0 };

int gOvlSecureLast = 0;
int gOvlSecureTag = 0;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#ifndef MTK_OVERLAY_ENGINE_SUPPORT
static int updateSecureOvl(int secure)
{
	MTEEC_PARAM param[4];
	unsigned int paramTypes;
	TZ_RESULT ret;
	param[0].value.a = (secure == 0 ? 0 : 1);
	DISP_DBG("[DAPC] change secure protection to %d\n", param[0].value.a);
	param[1].value.a = 3;	/* OVL */
	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
	ret = KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_SET_DAPC_MODE, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS)
		DISP_ERR("KREE_TeeServiceCall(TZCMD_DDP_SET_DAPC_MODE) fail, ret=%d\n", ret);

	return 0;
}
#endif
#endif

static DEFINE_MUTEX(DpEngineMutexLock);

static DECLARE_WAIT_QUEUE_HEAD(g_disp_mutex_wq);
static unsigned int g_disp_mutex_reg_update;

/* 50us -> 1G needs 50000times */
/* #define USE_TIME_OUT */
#ifdef USE_TIME_OUT
static int disp_wait_timeout(bool flag, unsigned int timeout)
{
	unsigned int cnt = 0;

	while (cnt < timeout) {

		if (flag)
			return 0;
		cnt++;
	}
	return -1;
}
#endif

#if defined(MTK_OVERLAY_ENGINE_SUPPORT)

void (*gdisp_path_register_ovl_rdma_callback) (unsigned int param);


void _disp_path_ovl_rdma_callback(unsigned int param)
{
	if (gdisp_path_register_ovl_rdma_callback != NULL)
		gdisp_path_register_ovl_rdma_callback(param);
}


void disp_path_register_ovl_rdma_callback(void (*callback) (unsigned int param), unsigned int param)
{
	gdisp_path_register_ovl_rdma_callback = callback;
}


void disp_path_unregister_ovl_rdma_callback(void (*callback) (unsigned int param),
					    unsigned int param)
{
	gdisp_path_register_ovl_rdma_callback = NULL;
}

#endif


unsigned int g_ddp_timeout_flag = 0;
#define DDP_WAIT_TIMEOUT(flag, timeout) \
{ \
	unsigned int cnt = 0; \
	while (cnt < timeout) { \
		if (flag) { \
			g_ddp_timeout_flag = 0;\
			break; \
		} \
		cnt++;\
	} \
	g_ddp_timeout_flag = 1;\
}

static void _disp_path_mutex_reg_update_cb(unsigned int param)
{
	if (param & (1 << gMutexID)) {
		g_disp_mutex_reg_update = 1;
		wake_up_interruptible(&g_disp_mutex_wq);
#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
		if (gdisp_path_register_ovl_rdma_callback)
			gdisp_path_register_ovl_rdma_callback(param);
#endif
	}
}

unsigned int disp_mutex_lock_cnt = 0;
unsigned int disp_mutex_unlock_cnt = 0;
int disp_path_get_mutex(void)
{
	if (pq_debug_flag == 3) {
		return 0;
	} else {
		disp_register_irq(DISP_MODULE_MUTEX, _disp_path_mutex_reg_update_cb);
		return disp_path_get_mutex_(gMutexID);
	}
}

int disp_path_get_mutex_(int mutexID)
{
	unsigned int cnt = 0;

	DISP_DBG("disp_path_get_mutex %d\n", disp_mutex_lock_cnt++);
	mutex_lock(&DpEngineMutexLock);
	MMProfileLog(DDP_MMP_Events.Mutex[mutexID], MMProfileFlagStart);
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX(mutexID), 1);
	DISP_REG_SET_FIELD(REG_FLD(1, mutexID), DISP_REG_CONFIG_MUTEX_INTSTA, 0);

	while (((DISP_REG_GET(DISP_REG_CONFIG_MUTEX(mutexID)) & DISP_INT_MUTEX_BIT_MASK) !=
		DISP_INT_MUTEX_BIT_MASK)) {
		cnt++;
		if (cnt > 10000) {
			DISP_ERR("disp_path_get_mutex() timeout! mutexID=%d\n", mutexID);
			MMProfileLogEx(DDP_MMP_Events.Mutex[mutexID], MMProfileFlagPulse, 0, 0);
			disp_dump_reg(DISP_MODULE_MUTEX0 + mutexID);
			break;
		}
	}

	return 0;
}

int disp_path_release_mutex(void)
{
	if (pq_debug_flag == 3) {
		return 0;
	} else {
		g_disp_mutex_reg_update = 0;
		return disp_path_release_mutex_(gMutexID);
	}
}

int disp_path_get_mutex_ex(int mutexID, int sw_mutex)
{
	unsigned int cnt = 0;

	DISP_DBG("disp_path_get_mutex %d\n", disp_mutex_lock_cnt++);

	if (sw_mutex)
		mutex_lock(&DpEngineMutexLock);

	MMProfileLog(DDP_MMP_Events.Mutex[mutexID], MMProfileFlagStart);
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX(mutexID), 1);
	DISP_REG_SET_FIELD(REG_FLD(1, mutexID), DISP_REG_CONFIG_MUTEX_INTSTA, 0);

	while (((DISP_REG_GET(DISP_REG_CONFIG_MUTEX(mutexID)) & DISP_INT_MUTEX_BIT_MASK) !=
		DISP_INT_MUTEX_BIT_MASK)) {
		cnt++;
		if (cnt > 10000) {
			DISP_ERR("disp_path_get_mutex() timeout! mutexID=%d\n", mutexID);
			MMProfileLogEx(DDP_MMP_Events.Mutex[mutexID], MMProfileFlagPulse, 0, 0);
			disp_dump_reg(DISP_MODULE_MUTEX0 + mutexID);
			break;
		}
	}

	return 0;
}

/* check engines' clock bit and enable bit before unlock mutex */
#define DDP_SMI_LARB2_POWER_BIT     0x1
#define DDP_OVL_POWER_BIT     0x30
#define DDP_RDMA0_POWER_BIT   0xe000
#define DDP_WDMA1_POWER_BIT   0x1800
int disp_check_engine_status(int mutexID)
{
	int result = 0;
	u32 inten;
	unsigned int engine = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(mutexID));

	if ((DISP_REG_GET(DISP_REG_CONFIG_CG_CON0) & DDP_SMI_LARB2_POWER_BIT) != 0) {
		result = -1;
		DISP_ERR("smi clk if off before release mutex, clk=0x%x\n",
			 DISP_REG_GET(DISP_REG_CONFIG_CG_CON0));

		DISP_MSG("force on smi clock\n");
		DISP_REG_SET(DISP_REG_CONFIG_CG_CON0,
			     DISP_REG_GET(DISP_REG_CONFIG_CG_CON0) | DDP_SMI_LARB2_POWER_BIT);
	}

	inten = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTEN);
	if (disp_update_mutex())
		DISP_ERR("before release mutex %d inten=0x%08X\n", mutexID, inten);

	if (engine & (1 << 0)) {
		/* ROT */
		/* ROT */
	}

	if (engine & (1 << 1)) {
		/* SCL */
		/* SCL */
	}

	if (engine & (1 << 2)) {	/* OVL */
		/* [DAPC] when the engine is already in DAPC protected status, actually it cannot read the register */
		/* if(DISP_REG_GET(DISP_REG_OVL_EN)==0 || */
		/* (DISP_REG_GET(DISP_REG_CONFIG_CG_CON0)&DDP_OVL_POWER_BIT) != 0) */
		/* { */
		/* result = -1; */
		/* DISP_ERR("ovl abnormal, en=%d, clk=0x%x\n",
		   DISP_REG_GET(DISP_REG_OVL_EN), DISP_REG_GET(DISP_REG_CONFIG_CG_CON0)); */
		/* } */
	}

	if (engine & (1 << 3)) {
		/* COLOR */
		/* COLOR */
	}

	if (engine & (1 << 4)) {
		/* TDSHP */
		/* TDSHP */
	}

	if (engine & (1 << 5)) {
		/* WDMA0 */
		/* WDMA0 */
	}
	if (engine & (1 << 6)) {
		/* WDMA1 */
	}
	if (engine & (1 << 7)) {	/* RDMA0 */
		if ((DISP_REG_GET(DISP_REG_RDMA_GLOBAL_CON) & 0x1) == 0 ||
		    (DISP_REG_GET(DISP_REG_CONFIG_CG_CON0) & DDP_RDMA0_POWER_BIT) != 0) {
			result = -1;
			DISP_ERR("rdma0 abnormal, en=%d, clk=0x%x\n",
				 DISP_REG_GET(DISP_REG_RDMA_GLOBAL_CON),
				 DISP_REG_GET(DISP_REG_CONFIG_CG_CON0));
		}
	}
	if (engine & (1 << 8)) {
		/* RDMA1 */
		/* RDMA1 */

	}
	if (engine & (1 << 9)) {
		/* BLS */
		/* BLS */
	}
	if (engine & (1 << 10)) {
		/* GAMMA */
		/* GAMMA */
	}

	if (result != 0) {
		DISP_ERR("engine status error before release mutex, engine=0x%x, mutexID=%d\n",
			 engine, mutexID);
	}

	return result;

}

int disp_path_release_mutex_ex(int mutexID, int sw_mutex)
{
/* unsigned int reg = 0; */
/* unsigned int cnt = 0; */
	disp_check_engine_status(mutexID);
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX(mutexID), 0);

	MMProfileLog(DDP_MMP_Events.Mutex[mutexID], MMProfileFlagEnd);

	if (sw_mutex)
		mutex_unlock(&DpEngineMutexLock);

	DISP_DBG("disp_path_release_mutex %d\n", disp_mutex_unlock_cnt++);

	return 0;
}

int disp_path_release_mutex_(int mutexID)
{
/* unsigned int reg = 0; */
/* unsigned int cnt = 0; */
	disp_check_engine_status(mutexID);
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX(mutexID), 0);

#if 0
/* can not polling mutex update done status, */
/* because after ECO, polling will delay at lease 12ms */
	while (((DISP_REG_GET(DISP_REG_CONFIG_MUTEX(mutexID)) & DISP_INT_MUTEX_BIT_MASK) != 0)) {
		cnt++;
		if (cnt > 10000) {
			DISP_ERR("disp_path_release_mutex() timeout!\n");
			MMProfileLogEx(DDP_MMP_Events.Mutex[mutexID], MMProfileFlagPulse, 1, 0);
			break;
		}
	}
#endif

#if 0
	while (((DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA) & (1 << mutexID)) != (1 << mutexID))) {
		if ((DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA) & (1 << (mutexID + 6))) ==
		    (1 << (mutexID + 6))) {
			DISP_ERR("disp_path_release_mutex() timeout!\n");
			disp_dump_reg(DISP_MODULE_CONFIG);
			/* print error engine */
			reg = DISP_REG_GET(DISP_REG_CONFIG_REG_COMMIT);
			if (reg != 0) {
				if (reg & (1 << 0)) {
					DISP_MSG(" ROT update reg timeout!\n");
					disp_dump_reg(DISP_MODULE_ROT);
				}
				if (reg & (1 << 1)) {
					DISP_MSG(" SCL update reg timeout!\n");
					disp_dump_reg(DISP_MODULE_SCL);
				}
				if (reg & (1 << 2)) {
					DISP_MSG(" OVL update reg timeout!\n");
					disp_dump_reg(DISP_MODULE_OVL);
				}
				if (reg & (1 << 3)) {
					DISP_MSG(" COLOR update reg timeout!\n");
					disp_dump_reg(DISP_MODULE_COLOR);
				}
				if (reg & (1 << 4)) {
					DISP_MSG(" 2D_SHARP update reg timeout!\n");
					disp_dump_reg(DISP_MODULE_TDSHP);
				}
				if (reg & (1 << 5)) {
					DISP_MSG(" WDMA0 update reg timeout!\n");
					disp_dump_reg(DISP_MODULE_WDMA0);
				}
				if (reg & (1 << 6)) {
					DISP_MSG(" WDMA1 update reg timeout!\n");
					disp_dump_reg(DISP_MODULE_WDMA1);
				}
				if (reg & (1 << 7)) {
					DISP_MSG(" RDMA0 update reg timeout!\n");
					disp_dump_reg(DISP_MODULE_RDMA0);
				}
				if (reg & (1 << 8)) {
					DISP_MSG(" RDMA1 update reg timeout!\n");
					disp_dump_reg(DISP_MODULE_RDMA1);
				}
				if (reg & (1 << 9)) {
					DISP_MSG(" BLS update reg timeout!\n");
					disp_dump_reg(DISP_MODULE_BLS);
				}
				if (reg & (1 << 10)) {
					DISP_MSG(" GAMMA update reg timeout!\n");
					disp_dump_reg(DISP_MODULE_GAMMA);
				}
			}
			/* reset mutex */
			DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(mutexID), 1);
			DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(mutexID), 0);
			DISP_MSG("mutex reset done!\n");
			MMProfileLogEx(DDP_MMP_Events.Mutex0, MMProfileFlagPulse, mutexID, 1);
			break;
		}

		cnt++;
		if (cnt > 1000) {
			DISP_ERR("disp_path_release_mutex() timeout!\n");
			MMProfileLogEx(DDP_MMP_Events.Mutex0, MMProfileFlagPulse, mutexID, 2);
			break;
		}
	}

	/* clear status */
	reg = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA);
	reg &= ~(1 << mutexID);
	reg &= ~(1 << (mutexID + 6));
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_INTSTA, reg);
#endif
	MMProfileLog(DDP_MMP_Events.Mutex[mutexID], MMProfileFlagEnd);
	mutex_unlock(&DpEngineMutexLock);
	DISP_DBG("disp_path_release_mutex %d\n", disp_mutex_unlock_cnt++);

	return 0;
}

int disp_path_wait_reg_update(void)
{
	wait_event_interruptible_timeout(g_disp_mutex_wq, g_disp_mutex_reg_update, HZ / 10);
	return 0;
}

int disp_path_change_tdshp_status(unsigned int layer, unsigned int enable)
{
	ASSERT(layer < DDP_OVL_LAYER_MUN);
	DISP_MSG("disp_path_change_tdshp_status(), layer=%d, enable=%d", layer, enable);
	gTdshpStatus[layer] = enable;
	return 0;
}

#if 0
static unsigned long disp_register_share_memory(void *buffer, int size)
{
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	TZ_RESULT ret;
	KREE_SHAREDMEM_HANDLE sharedHandle = 0;
	KREE_SHAREDMEM_PARAM sharedParam;

	/* Register shared memory */
	sharedParam.buffer = buffer;
	sharedParam.size = size;
	ret = KREE_RegisterSharedmem(ddp_mem_session_handle(), &sharedHandle, &sharedParam);
	if (ret != TZ_RESULT_SUCCESS) {
		DISP_ERR("disp_register_share_memory Error: %d, line:%d, ddp_mem_session(%d)", ret,
			 __LINE__, ddp_mem_session_handle());
		return 0;
	}

	return sharedHandle;
#else
	return 0;
#endif
}

static int disp_unregister_share_memory(unsigned long sharedHandle)
{
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	TZ_RESULT ret;

	/* Unregister shared memory */
	ret = KREE_UnregisterSharedmem(ddp_mem_session_handle(), sharedHandle);
	if (ret != TZ_RESULT_SUCCESS) {
		DISP_ERR("disp_unregister_share_memory Error: %d, line:%d", ret, __LINE__);
		return -1;
	}
#endif

	return 0;
}
#endif


#ifdef MTK_OVERLAY_ENGINE_SUPPORT


int disp_path_config_layer_(OVL_CONFIG_STRUCT *pOvlConfig, int OvlSecure)
{
/* unsigned int reg_addr; */

	DISP_DBG("[DDP] config_layer(), layer=%d, en=%d, source=%d, fmt=%d", pOvlConfig->layer,	/* layer */
		 pOvlConfig->layer_en, pOvlConfig->source,	/* data source (0=memory) */
		 pOvlConfig->fmt);

	DISP_DBG("[DDP] config_layer(), addr=0x%x, src(%d, %d),pitch=%d, dst(%d, %d, %d, %d), keyEn=%d, key=%d",
		 pOvlConfig->addr,   /* addr */
		 pOvlConfig->src_x,	/* x */
		 pOvlConfig->src_y,	/* y */
		 pOvlConfig->src_pitch,	/* pitch, pixel number */
		 pOvlConfig->dst_x,	/* x */
		 pOvlConfig->dst_y,	/* y */
		 pOvlConfig->dst_w,	/* width */
		 pOvlConfig->dst_h,	/* height */
		 pOvlConfig->keyEn,	/* color key */
		 pOvlConfig->key);

	DISP_DBG("[DDP] config_layer(), aen=%d, alpha=%d, isTdshp=%d, idx=%d,sec=%d, ovl_sec = %d",
		 pOvlConfig->aen,    /* alpha enable */
		 pOvlConfig->alpha,
		 pOvlConfig->isTdshp, pOvlConfig->buff_idx, pOvlConfig->security, OvlSecure);

	/* config overlay */
	MMProfileLogEx(DDP_MMP_Events.Debug, MMProfileFlagPulse, pOvlConfig->layer,
		       pOvlConfig->layer_en);

	if (pOvlConfig->security == LAYER_SECURE_BUFFER)
		gOvlLayerSecure[pOvlConfig->layer] = 1;
	else
		gOvlLayerSecure[pOvlConfig->layer] = 0;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* if OVL already in secure state (DAPC protected) */
	if (OvlSecure == 1) {
		MTEEC_PARAM param[4];
		unsigned int paramTypes;
		TZ_RESULT ret;

		param[0].value.a = pOvlConfig->layer;
		param[1].value.a = pOvlConfig->layer_en;
		paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
		ret =
		    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_OVL_LAYER_SWITCH,
					paramTypes, param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR
			    ("[DAPC] KREE_TeeServiceCall(TZCMD_DDP_OVL_LAYER_SWITCH) fail, ret=%d\n",
			     ret);
		}
	} else
#endif
	{
		if (!gOvlLayerSecure[pOvlConfig->layer]) {
			/* OVLLayerSwitch */
			OVLLayerSwitch(pOvlConfig->layer, pOvlConfig->layer_en);
		} else {
			/* error */
			DISP_ERR("error, do not support security==LAYER_SECURE_BUFFER!\n");
		}
	}


	if (pOvlConfig->layer_en != 0) {
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		if (OvlSecure == 1) {
			MTEEC_PARAM param[4];
			unsigned int paramTypes;
			TZ_RESULT ret;
			KREE_SHAREDMEM_HANDLE p_ovl_config_layer_share_handle = 0;
			KREE_SHAREDMEM_PARAM sharedParam;
			/* p_ovl_config_layer_share_handle = */
			/* disp_register_share_memory(pOvlConfig, sizeof(OVL_CONFIG_STRUCT)); */
			/* Register shared memory */
			sharedParam.buffer = pOvlConfig;
			sharedParam.size = sizeof(OVL_CONFIG_STRUCT);
			ret =
			    KREE_RegisterSharedmem(ddp_mem_session_handle(),
						   &p_ovl_config_layer_share_handle, &sharedParam);
			if (ret != TZ_RESULT_SUCCESS) {
				DISP_ERR
				    ("disp_register_share_memory Error: %d, line:%d, ddp_mem_session(%d)",
				     ret, __LINE__, (unsigned int)ddp_mem_session_handle());
				return 0;
			}

			param[0].memref.handle = (uint32_t) p_ovl_config_layer_share_handle;
			param[0].memref.offset = 0;
			param[0].memref.size = sizeof(OVL_CONFIG_STRUCT);
			/* wether the display buffer is a secured one */
			param[1].value.a = gOvlLayerSecure[pOvlConfig->layer];
			paramTypes = TZ_ParamTypes2(TZPT_MEMREF_INPUT, TZPT_VALUE_INPUT);
			DISP_DBG("config_layer handle=0x%x\n", param[0].memref.handle);

			ret =
			    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_OVL_LAYER_CONFIG,
						paramTypes, param);

			if (ret != TZ_RESULT_SUCCESS) {
				/* error */
				DISP_ERR("TZCMD_DDP_OVL_LAYER_CONFIG fail, ret=%d\n", ret);
			}

			ret =
			    KREE_UnregisterSharedmem(ddp_mem_session_handle(),
						     p_ovl_config_layer_share_handle);

			if (ret) {
				DISP_ERR
				    ("UREE_UnregisterSharedmem %s share_handle Error: %d, line:%d, session(%d)",
				     __func__, ret, __LINE__,
				     (unsigned int)ddp_mem_session_handle());
			}
		} else
#endif
		{
			if (!gOvlLayerSecure[pOvlConfig->layer]) {
				OVLLayerConfig(pOvlConfig->layer,	/* layer */
					       pOvlConfig->source,	/* data source (0=memory) */
					       pOvlConfig->fmt, pOvlConfig->addr,	/* addr */
					       pOvlConfig->src_x,	/* x */
					       pOvlConfig->src_y,	/* y */
					       pOvlConfig->src_pitch,	/* pitch, pixel number */
					       pOvlConfig->dst_x,	/* x */
					       pOvlConfig->dst_y,	/* y */
					       pOvlConfig->dst_w,	/* width */
					       pOvlConfig->dst_h,	/* height */
					       pOvlConfig->keyEn,	/* color key */
					       pOvlConfig->key,	/* color key */
					       pOvlConfig->aen,	/* alpha enable */
					       pOvlConfig->alpha);	/* alpha */
			} else {
				DISP_ERR("error, do not support security==LAYER_SECURE_BUFFER!\n");
			}
		}
	}

	return 0;
}


int gOvlEngineControl = 0;
int disp_path_config_layer_ovl_engine_control(int enable)
{
	gOvlEngineControl = enable;
	return 0;
}


int disp_path_config_layer(OVL_CONFIG_STRUCT *pOvlConfig)
{
	if (gOvlEngineControl)
		return 0;

	return disp_path_config_layer_(pOvlConfig, 0);
}


int disp_path_config_layer_ovl_engine(OVL_CONFIG_STRUCT *pOvlConfig, int OvlSecure)
{
	if (!gOvlEngineControl)
		return 0;

	OVLStop();

	disp_path_config_layer_(pOvlConfig, OvlSecure);

	OVLSWReset();
	OVLStart();

	return 0;
}


/*add for directlink mode, no need to stop/start OVL. If do stop/start OVL, screen may blink*/
int disp_path_config_layer_ovl_engine_ex(OVL_CONFIG_STRUCT *pOvlConfig, int OvlSecure, bool fgCouple)
{
	if (!gOvlEngineControl)
		return 0;

	if (!fgCouple)
		OVLStop();

	disp_path_config_layer_(pOvlConfig, OvlSecure);

	if (!fgCouple) {
		OVLSWReset();
		OVLStart();
	}

	return 0;
}

#else


int disp_path_config_layer(OVL_CONFIG_STRUCT *pOvlConfig)
{
/* unsigned int reg_addr; */

	DISP_DBG("[DDP] config_layer(), layer=%d, en=%d, source=%d, fmt=%d, addr=0x%x",
		 pOvlConfig->layer,	/* layer */
		 pOvlConfig->layer_en, pOvlConfig->source,	/* data source (0=memory) */
		 pOvlConfig->fmt, pOvlConfig->addr);	/* addr */

	DISP_DBG("[DDP] config_layer(), src(%d, %d), pitch=%d, dst(%d, %d, %d, %d), keyEn=%d, key=%d, aen=%d",
		 pOvlConfig->src_x,  /* x */
		 pOvlConfig->src_y,	/* y */
		 pOvlConfig->src_pitch,	/* pitch, pixel number */
		 pOvlConfig->dst_x,	/* x */
		 pOvlConfig->dst_y,	/* y */
		 pOvlConfig->dst_w,	/* width */
		 pOvlConfig->dst_h,	/* height */
		 pOvlConfig->keyEn,	/* color key */
		 pOvlConfig->key,	/* color key */
		 pOvlConfig->aen);	/* alpha enable */

	DISP_DBG("[DDP] config_layer(), alpha=%d, isTdshp=%d, idx=%d, sec=%d\n ",
		 pOvlConfig->alpha,
		 pOvlConfig->isTdshp, pOvlConfig->buff_idx, pOvlConfig->security);

	/* config overlay */
	MMProfileLogEx(DDP_MMP_Events.Debug, MMProfileFlagPulse, pOvlConfig->layer,
		       pOvlConfig->layer_en);
	if (pOvlConfig->security != LAYER_SECURE_BUFFER) {
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		/* if OVL already in secure state (DAPC protected) */
		if (gOvlSecure == 1) {
			MTEEC_PARAM param[4];
			unsigned int paramTypes;
			TZ_RESULT ret;

			param[0].value.a = pOvlConfig->layer;
			param[1].value.a = pOvlConfig->layer_en;
			paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
			ret =
			    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_OVL_LAYER_SWITCH,
						paramTypes, param);
			if (ret != TZ_RESULT_SUCCESS) {
				DISP_ERR
				    ("[DAPC] KREE_TeeServiceCall(TZCMD_DDP_OVL_LAYER_SWITCH) fail, ret=%d\n",
				     ret);
			}
		} else {
#endif
			OVLLayerSwitch(pOvlConfig->layer, pOvlConfig->layer_en);
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		}
#endif
	} else {
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		MTEEC_PARAM param[4];
		unsigned int paramTypes;
		TZ_RESULT ret;

		param[0].value.a = pOvlConfig->layer;
		param[1].value.a = pOvlConfig->layer_en;
		paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
		ret =
		    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_OVL_LAYER_SWITCH,
					paramTypes, param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("KREE_TeeServiceCall(TZCMD_DDP_OVL_LAYER_SWITCH) fail, ret=%d\n",
				 ret);
		}
#else
		DISP_ERR("error, do not support security==LAYER_SECURE_BUFFER!\n");
#endif
	}

	if (pOvlConfig->layer_en != 0) {
		if (pOvlConfig->security != LAYER_SECURE_BUFFER) {
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
			if (gOvlSecure == 1) {
				MTEEC_PARAM param[4];
				unsigned int paramTypes;
				TZ_RESULT ret;
				KREE_SHAREDMEM_HANDLE p_ovl_config_layer_share_handle = 0;
				KREE_SHAREDMEM_PARAM sharedParam;
				/* p_ovl_config_layer_share_handle = */
				/* disp_register_share_memory(pOvlConfig, sizeof(OVL_CONFIG_STRUCT)); */
				/* Register shared memory */
				sharedParam.buffer = pOvlConfig;
				sharedParam.size = sizeof(OVL_CONFIG_STRUCT);
				ret =
				    KREE_RegisterSharedmem(ddp_mem_session_handle(),
							   &p_ovl_config_layer_share_handle,
							   &sharedParam);
				if (ret != TZ_RESULT_SUCCESS) {
					DISP_ERR
					    ("disp_register_share_memory Error: %d, line:%d, ddp_mem_session(%x)",
					     ret, __LINE__, (unsigned int)ddp_mem_session_handle());
					return 0;
				}

				param[0].memref.handle = (uint32_t) p_ovl_config_layer_share_handle;
				param[0].memref.offset = 0;
				param[0].memref.size = sizeof(OVL_CONFIG_STRUCT);
				param[1].value.a = pOvlConfig->security;
				/* wether the display buffer is a secured one */
				paramTypes = TZ_ParamTypes2(TZPT_MEMREF_INPUT, TZPT_VALUE_INPUT);
				DISP_DBG("config_layer handle=0x%x\n", param[0].memref.handle);

				ret =
				    KREE_TeeServiceCall(ddp_session_handle(),
							TZCMD_DDP_OVL_LAYER_CONFIG, paramTypes,
							param);
				if (ret != TZ_RESULT_SUCCESS) {
					/* error */
					DISP_ERR("TZCMD_DDP_OVL_LAYER_CONFIG fail, ret=%d\n", ret);
				}

				ret =
				    KREE_UnregisterSharedmem(ddp_mem_session_handle(),
							     p_ovl_config_layer_share_handle);
				if (ret) {
					DISP_ERR
					    ("UnregisterSharedmem %s share_handle Error: %d,line:%d, session(%x)",
					     __func__, ret, __LINE__,
					     (unsigned int)ddp_mem_session_handle());
				}
			} else {
#endif
				OVLLayerConfig(pOvlConfig->layer,	/* layer */
					       pOvlConfig->source,	/* data source (0=memory) */
					       pOvlConfig->fmt, pOvlConfig->addr,	/* addr */
					       pOvlConfig->src_x,	/* x */
					       pOvlConfig->src_y,	/* y */
					       pOvlConfig->src_pitch,	/* pitch, pixel number */
					       pOvlConfig->dst_x,	/* x */
					       pOvlConfig->dst_y,	/* y */
					       pOvlConfig->dst_w,	/* width */
					       pOvlConfig->dst_h,	/* height */
					       pOvlConfig->keyEn,	/* color key */
					       pOvlConfig->key,	/* color key */
					       pOvlConfig->aen,	/* alpha enable */
					       pOvlConfig->alpha);	/* alpha */

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
			}
#endif
			gOvlLayerSecure[pOvlConfig->layer] = 0;
		} else {
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
			MTEEC_PARAM param[4];
			unsigned int paramTypes;
			TZ_RESULT ret;
			KREE_SHAREDMEM_HANDLE p_ovl_config_layer_share_handle = 0;
			KREE_SHAREDMEM_PARAM sharedParam;
			/* p_ovl_config_layer_share_handle = */
			/* disp_register_share_memory(pOvlConfig, sizeof(OVL_CONFIG_STRUCT)); */
			/* Register shared memory */
			sharedParam.buffer = pOvlConfig;
			sharedParam.size = sizeof(OVL_CONFIG_STRUCT);
			ret =
			    KREE_RegisterSharedmem(ddp_mem_session_handle(),
						   &p_ovl_config_layer_share_handle, &sharedParam);
			if (ret != TZ_RESULT_SUCCESS) {
				DISP_ERR
				    ("disp_register_share_memory Error: %d, line:%d, ddp_mem_session(%x)",
				     ret, __LINE__, (unsigned int)ddp_mem_session_handle());
				return 0;
			}

			param[0].memref.handle = (uint32_t) p_ovl_config_layer_share_handle;
			param[0].memref.offset = 0;
			param[0].memref.size = sizeof(OVL_CONFIG_STRUCT);
			param[1].value.a = pOvlConfig->security;
			paramTypes = TZ_ParamTypes2(TZPT_MEMREF_INPUT, TZPT_VALUE_INPUT);
			DISP_DBG("config_layer handle=0x%x\n", param[0].memref.handle);

			ret =
			    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_OVL_LAYER_CONFIG,
						paramTypes, param);
			if (ret != TZ_RESULT_SUCCESS) {
				/* error */
				DISP_ERR("TZCMD_DDP_OVL_LAYER_CONFIG fail, ret=%d\n", ret);
			}

			ret =
			    KREE_UnregisterSharedmem(ddp_mem_session_handle(),
						     p_ovl_config_layer_share_handle);
			if (ret) {
				DISP_ERR
				    ("UnregisterSharedmem %s share_handle Error: %d, line:%d, session(%x)",
				     __func__, ret, __LINE__,
				     (unsigned int)ddp_mem_session_handle());
			}
#else
			DISP_ERR("error, do not support security==LAYER_SECURE_BUFFER!\n");
#endif
			gOvlLayerSecure[pOvlConfig->layer] = 1;
		}
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		/* then according the result above, we know if the OVL module should be in secure protection */
		/* every time the {gOvlSecure} determined by the status of 4 layers: */
		/* if one or more layer is set to be secure, then the OVL shall be in secure state. */
		/* if all layers are not secured, {gOvlSecure} is set to 0. */
		if (gOvlSecure !=
		    (gOvlLayerSecure[0] | gOvlLayerSecure[1] | gOvlLayerSecure[2] |
		     gOvlLayerSecure[3])) {
			gOvlSecure =
			    gOvlLayerSecure[0] | gOvlLayerSecure[1] | gOvlLayerSecure[2] |
			    gOvlLayerSecure[3];
			disp_register_intr(MT8135_DISP_OVL_IRQ_ID, gOvlSecure);

			if (pOvlConfig->security != LAYER_SECURE_BUFFER) {
				if (gOvlSecureTag == 1) {
					/* clear the tag */
					gOvlSecureTag = 0;
				} else {
					/* clear gOvlSecure */
					gOvlSecure = 0;
				}
			} else {
				if (gOvlSecureTag == 0)
					gOvlSecureTag = 1;	/* the tag shall be set */
			}

			/* we use {gOvlSecureLast} to record the last secure status of OVL */
			if ((int)gOvlSecure != gOvlSecureLast) {
				gOvlSecureLast = (int)gOvlSecure;
				updateSecureOvl(gOvlSecure);	/* and change DAPC protection level */
			}
			DISP_DBG("[DAPC] gOvlSecure=%d, gOvlSecureLast=%d, < %d, %d, %d, %d\n",
				 gOvlSecure, gOvlSecureLast, gOvlLayerSecure[0], gOvlLayerSecure[1],
				 gOvlLayerSecure[2], gOvlLayerSecure[3]);
		}
		/* config each layer to secure mode */
		{
			/* if the secure setting of each layer changed */
			if (gOvlLayerSecure[pOvlConfig->layer] !=
			    gOvlLayerSecureLast[pOvlConfig->layer]) {
				MTEEC_PARAM param[4];
				unsigned int paramTypes;
				TZ_RESULT ret;
				gOvlLayerSecureLast[pOvlConfig->layer] =
				    gOvlLayerSecure[pOvlConfig->layer];
				param[0].value.a = M4U_PORT_OVL_CH0 + pOvlConfig->layer;
				param[1].value.a = gOvlLayerSecure[pOvlConfig->layer];
				paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
				ret =
				    KREE_TeeServiceCall(ddp_session_handle(),
							TZCMD_DDP_SET_SECURE_MODE, paramTypes,
							param);
				if (ret != TZ_RESULT_SUCCESS) {
					DISP_ERR
					    ("KREE_TeeServiceCall(TZCMD_DDP_SET_SECURE_MODE) fail, ret=%d\n",
					     ret);
				}
			}
		}
#endif

	}
	if (pOvlConfig->isTdshp == 0) {
		gTdshpStatus[pOvlConfig->layer] = 0;
	} else {
		int i = 0;
		for (i = 0; i < OVL_LAYER_NUM; i++) {
			if (gTdshpStatus[i] == 1 && i != pOvlConfig->layer) {
				/* other layer has already enable tdshp */
				DISP_ERR
				    ("enable layer=%d tdshp, but layer=%d has already enable tdshp\n",
				     i, pOvlConfig->layer);
				return -1;
			}
			gTdshpStatus[pOvlConfig->layer] = 1;
		}
	}
	/* OVLLayerTdshpEn(pOvlConfig->layer, pOvlConfig->isTdshp); */
	if (pOvlConfig->security != LAYER_SECURE_BUFFER) {
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		/* if OVL already in secure state (DAPC protected) */
		if (gOvlSecure == 1) {
			MTEEC_PARAM param[4];
			unsigned int paramTypes;
			TZ_RESULT ret;

			/* DISP_DBG("[DAPC] enable two-d sharp when OVL ready in secure state\n"); */
			param[0].value.a = pOvlConfig->layer;
			param[1].value.a = 0;
			paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
			ret =
			    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_OVL_LAYER_TDSHP_EN,
						paramTypes, param);
			if (ret != TZ_RESULT_SUCCESS) {
				DISP_ERR
				    ("KREE_TeeServiceCall(TZCMD_DDP_OVL_LAYER_TDSHP_EN) fail, ret=%d\n",
				     ret);
			}
		} else {
#endif
			OVLLayerTdshpEn(pOvlConfig->layer, 0);	/* Cvs: de-couple */
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		}
#endif
	} else {
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		MTEEC_PARAM param[4];
		unsigned int paramTypes;
		TZ_RESULT ret;

		param[0].value.a = pOvlConfig->layer;
		param[1].value.a = 0;
		paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
		ret =
		    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_OVL_LAYER_TDSHP_EN,
					paramTypes, param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR
			    ("KREE_TeeServiceCall(TZCMD_DDP_OVL_LAYER_TDSHP_EN) fail, ret=%d\n",
			     ret);
		}
#else
		DISP_ERR("error, do not support security==LAYER_SECURE_BUFFER!\n");
#endif
	}

	return 0;
}

#endif

int disp_path_config_layer_addr(unsigned int layer, unsigned int addr)
{
	unsigned int reg_addr;

	DISP_DBG("[DDP]disp_path_config_layer_addr(), layer=%d, addr=0x%x\n ", layer, addr);

	if (gOvlSecure == 0) {	/* secure is 0 */
		switch (layer) {
		case 0:
			DISP_REG_SET(DISP_REG_OVL_L0_ADDR, addr);
			reg_addr = DISP_REG_OVL_L0_ADDR;
			break;
		case 1:
			DISP_REG_SET(DISP_REG_OVL_L1_ADDR, addr);
			reg_addr = DISP_REG_OVL_L1_ADDR;
			break;
		case 2:
			DISP_REG_SET(DISP_REG_OVL_L2_ADDR, addr);
			reg_addr = DISP_REG_OVL_L2_ADDR;
			break;
		case 3:
			DISP_REG_SET(DISP_REG_OVL_L3_ADDR, addr);
			reg_addr = DISP_REG_OVL_L3_ADDR;
			break;
		default:
			DISP_ERR("unknow layer=%d\n", layer);
		}
	} else {
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		MTEEC_PARAM param[4];
		unsigned int paramTypes;
		TZ_RESULT ret;

		param[0].value.a = layer;
		param[1].value.a = addr;
		paramTypes = TZ_ParamTypes3(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);
		ret =
		    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_OVL_CONFIG_LAYER_ADDR,
					paramTypes, param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR
			    ("KREE_TeeServiceCall(TZCMD_DDP_OVL_CONFIG_LAYER_ADDR) fail, ret=%d\n",
			     ret);
		} else {
			DISP_DBG("disp_path_config_layer_addr, reg=0x%x\n", param[2].value.a);
		}
#else
		DISP_ERR("error, do not support security==LAYER_SECURE_BUFFER!\n");
#endif
	}

	return 0;
}

DECLARE_WAIT_QUEUE_HEAD(mem_out_wq);
static unsigned int mem_out_done;
void _disp_path_wdma_callback(unsigned int param)
{
	mem_out_done = 1;
	wake_up_interruptible(&mem_out_wq);
}

void disp_path_wait_mem_out_done(void)
{
	int ret = 0;

	ret = wait_event_interruptible_timeout(mem_out_wq, mem_out_done, disp_ms2jiffies(1000));


	if (ret == 0) {		/* timeout */
		DISP_ERR("disp_path_wait_mem_out_done timeout\n");
		disp_dump_reg(DISP_MODULE_WDMA0);
	} else if (ret < 0) {	/* intr by a signal */
		DISP_ERR("disp_path_wait_mem_out_done intr by a signal ret=%d\n", ret);
	}

	mem_out_done = 0;
}


/* for video mode, if there are more than one frame between memory_out done and disable WDMA */
/* mem_out_done will be set to 1, next time user trigger disp_path_wait_mem_out_done() will return */
/* directly, in such case, screen capture will dose not work for one time. */
/* so we add this func to make sure disp_path_wait_mem_out_done() will be execute everytime. */
void disp_path_clear_mem_out_done_flag(void)
{
	mem_out_done = 0;
}

int disp_path_query(void)
{
	return 0;
}

/* add wdma1 into the path */
/* should call get_mutex() / release_mutex for this func */
int disp_path_config_mem_out(struct disp_path_config_mem_out_struct *pConfig)
{
	unsigned int reg;
	static unsigned int bMemOutEnabled;
#if 0
	DISP_DBG
	    (" disp_path_config_mem_out(), enable = %d, outFormat=%d, dstAddr=0x%x, ROI(%d,%d,%d,%d)\n",
	     pConfig->enable, pConfig->outFormat, pConfig->dstAddr, pConfig->srcROI.x,
	     pConfig->srcROI.y, pConfig->srcROI.width, pConfig->srcROI.height);
#endif
	if (pConfig->enable == 1 && pConfig->dstAddr == 0) {
		/* error */
		DISP_ERR("pConfig->dstAddr==0!\n");
	}

	if (pConfig->enable == 1) {
		mem_out_done = 0;
		disp_register_irq(DISP_MODULE_WDMA1, _disp_path_wdma_callback);

		/* config wdma1 */
		if (!bMemOutEnabled)
			WDMAReset(1);
		WDMAConfig(1,
			   WDMA_INPUT_FORMAT_ARGB,
			   pConfig->srcROI.width,
			   pConfig->srcROI.height,
			   0,
			   0,
			   pConfig->srcROI.width,
			   pConfig->srcROI.height,
			   pConfig->outFormat, pConfig->dstAddr, pConfig->srcROI.width, 1, 0);
		if (!bMemOutEnabled) {
			WDMAStart(1);
			/* mutex */
			reg = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(gMutexID));
			DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(gMutexID), reg | 0x40);	/* wdma1=6 */

			/* ovl mout */
			reg = DISP_REG_GET(DISP_REG_CONFIG_OVL_MOUT_EN);
			DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, reg | 0x1);	/* ovl_mout output to bls */
		}
		bMemOutEnabled = 1;
		/* disp_dump_reg(DISP_MODULE_WDMA1); */
	} else {
		/* mutex */
		reg = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(gMutexID));
		DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(gMutexID), reg & (~0x40));	/* wdma1=6 */

		/* ovl mout */
		reg = DISP_REG_GET(DISP_REG_CONFIG_OVL_MOUT_EN);
		DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, reg & (~0x1));	/* ovl_mout output to bls */

		/* config wdma1 */
		/* WDMAReset(1); */
		disp_unregister_irq(DISP_MODULE_WDMA1, _disp_path_wdma_callback);
		bMemOutEnabled = 0;
	}

	return 0;
}

/* just mem->ovl->wdma1->mem, used in suspend mode screen capture */
/* have to call this function set pConfig->enable=0 to reset configuration */
/* should call clock_on()/clock_off() if use this function in suspend mode */
int disp_path_config_mem_out_without_lcd(struct disp_path_config_mem_out_struct *pConfig)
{
	static unsigned int reg_mutex_mod;
	static unsigned int reg_mutex_sof;
	static unsigned int reg_mout;

	DISP_DBG
	    (" disp_path_config_mem_out(), enable = %d, outFormat=%d, dstAddr=0x%x, ROI(%d,%d,%d,%d)\n",
	     pConfig->enable, pConfig->outFormat, pConfig->dstAddr, pConfig->srcROI.x,
	     pConfig->srcROI.y, pConfig->srcROI.width, pConfig->srcROI.height);

	if (pConfig->enable == 1 && pConfig->dstAddr == 0) {
		/* error */
		DISP_ERR("pConfig->dstAddr==0!\n");
	}

	if (pConfig->enable == 1) {
		mem_out_done = 0;
		disp_register_irq(DISP_MODULE_WDMA1, _disp_path_wdma_callback);

		/* config wdma1 */
		WDMAReset(1);
		WDMAConfig(1,
			   WDMA_INPUT_FORMAT_ARGB,
			   pConfig->srcROI.width,
			   pConfig->srcROI.height,
			   0,
			   0,
			   pConfig->srcROI.width,
			   pConfig->srcROI.height,
			   pConfig->outFormat, pConfig->dstAddr, pConfig->srcROI.width, 1, 0);
		WDMAStart(1);

		/* mutex module */
		reg_mutex_mod = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(gMutexID));
		DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(gMutexID), 0x44);	/* ovl, wdma1 */

		/* mutex sof */
		reg_mutex_sof = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_SOF(gMutexID));
		DISP_REG_SET(DISP_REG_CONFIG_MUTEX_SOF(gMutexID), 0);	/* single mode */

		/* ovl mout */
		reg_mout = DISP_REG_GET(DISP_REG_CONFIG_OVL_MOUT_EN);
		DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, 1 << 0);	/* ovl_mout output to wdma1 */

		/* disp_dump_reg(DISP_MODULE_WDMA1); */
	} else {
		/* mutex */
		DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(gMutexID), reg_mutex_mod);
		DISP_REG_SET(DISP_REG_CONFIG_MUTEX_SOF(gMutexID), reg_mutex_sof);
		/* ovl mout */
		DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, reg_mout);

		disp_unregister_irq(DISP_MODULE_WDMA1, _disp_path_wdma_callback);
	}

	return 0;
}


#ifdef MTK_OVERLAY_ENGINE_SUPPORT
DECLARE_WAIT_QUEUE_HEAD(ovl_mem_out_wq);
static unsigned int ovl_mem_out_done;


void disp_path_wait_ovl_wdma_callbak(unsigned int param)
{
	ovl_mem_out_done = 1;
	wake_up_interruptible(&ovl_mem_out_wq);
}

void disp_path_wait_ovl_wdma_done(void)
{
	int ret = 0;

	ret = wait_event_interruptible_timeout(ovl_mem_out_wq,
					       ovl_mem_out_done, disp_ms2jiffies(1000));


	if (ret == 0) {		/* timeout */
		DISP_ERR("disp_path_wait_ovl_mem_out_done timeout\n");
		disp_dump_reg(DISP_MODULE_WDMA0);
	} else if (ret < 0) {	/* intr by a signal */
		DISP_ERR("disp_path_wait_ovl_mem_out_done intr by a signal ret=%d\n", ret);
	}

	ovl_mem_out_done = 0;
}

void (*gdisp_path_register_ovl_wdma_callback) (unsigned int param) = NULL;


void _disp_path_ovl_wdma_callback(unsigned int param)
{
	if (gdisp_path_register_ovl_wdma_callback != NULL)
		gdisp_path_register_ovl_wdma_callback(param);
}


void disp_path_register_ovl_wdma_callback(void (*callback) (unsigned int param), unsigned int param)
{
	gdisp_path_register_ovl_wdma_callback = callback;
}


void disp_path_unregister_ovl_wdma_callback(void (*callback) (unsigned int param),
					    unsigned int param)
{
	gdisp_path_register_ovl_wdma_callback = NULL;
}


int disp_path_config_OVL_WDMA_path(int mutex_id, int isolated, int connect)
{
#if 1
	if (isolated) {
	/* mutex module */
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(mutex_id), 0x44);	/* ovl, wdma1 */

	/* mutex sof */
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_SOF(mutex_id), 0);	/* single mode */

	/* ovl mout */
	DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, 1 << 0);	/* ovl_mout output to wdma1 */
	} else {
		unsigned int reg;

		if (connect) {
			/* mutex */
			reg = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(mutex_id));
			DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(mutex_id), reg | 0x40);	/* wdma1=6 */

			/* ovl mout */
			reg = DISP_REG_GET(DISP_REG_CONFIG_OVL_MOUT_EN);
			DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, reg | 0x1);	/* ovl_mout output to wdma1 */
		} else {
			/* mutex */
			reg = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(gMutexID));
			DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(gMutexID), reg & (~0x40));	/* wdma1=6 */

			/* ovl mout */
			reg = DISP_REG_GET(DISP_REG_CONFIG_OVL_MOUT_EN);
			DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, reg & (~0x1));	/* ovl_mout output to wdma1 */
		}
	}
#else
	/* mutex module */
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(mutex_id), 1 << 2 | 1 << 6 | 1 << 9);	/* ovl, wdma1 */

	/* mutex sof */
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_SOF(mutex_id), 0);	/* single mode */

	/* ovl mout */
	DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, 1 << 1);	/* ovl_mout output to BLS */

	DISP_REG_SET(DISP_REG_CONFIG_BLS_SEL, 0);	/* bls_sel from ovl */

	/* ///bypass BLS */
	DISP_REG_SET(DISP_REG_BLS_RST, 0x1);
	DISP_REG_SET(DISP_REG_BLS_RST, 0x0);
	DISP_REG_SET(DISP_REG_BLS_EN, 0x80000000);

	/* UFO to WDMA */
	/* *(volatile int *)(0xF4000880) = 0x3; */
#endif

	return 0;
}


int disp_path_config_OVL_WDMA(struct disp_path_config_mem_out_struct *pConfig, int OvlSecure)
{
	DISP_DBG
	    (" disp_path_config_OVL_WDMA(), enable = %d, outFormat=%d, dstAddr=0x%x, ROI(%d,%d,%d,%d)\n",
	     pConfig->enable, pConfig->outFormat, pConfig->dstAddr, pConfig->srcROI.x,
	     pConfig->srcROI.y, pConfig->srcROI.width, pConfig->srcROI.height);

	if (pConfig->enable == 1 && pConfig->dstAddr == 0) {
		/* error */
		DISP_ERR("pConfig->dstAddr==0!\n");
	}

	if (pConfig->enable == 1) {
		OVLROI(pConfig->srcROI.width,	/* width */
		       pConfig->srcROI.height,	/* height */
		       0);	/* background B */

		mem_out_done = 0;
		disp_register_irq(DISP_MODULE_WDMA1, _disp_path_ovl_wdma_callback);


#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		/* if OVL already in secure state (DAPC protected) */
		if (OvlSecure) {
			MTEEC_PARAM param[4];
			unsigned int paramTypes;
			TZ_RESULT ret;
			KREE_SHAREDMEM_HANDLE p_wdma_config_share_handle = 0;
			KREE_SHAREDMEM_PARAM sharedParam;

			/* Register shared memory */
			sharedParam.buffer = pConfig;
			sharedParam.size = sizeof(struct disp_path_config_mem_out_struct);
			ret =
			    KREE_RegisterSharedmem(ddp_mem_session_handle(),
						   &p_wdma_config_share_handle, &sharedParam);
			if (ret != TZ_RESULT_SUCCESS) {
				DISP_ERR
				    ("disp_register_share_memory Error: %d, line:%d, ddp_mem_session(%d)",
				     ret, __LINE__, (unsigned int)ddp_mem_session_handle());
				return 0;
			}

			param[0].memref.handle = (uint32_t) p_wdma_config_share_handle;
			param[0].memref.offset = 0;
			param[0].memref.size = sizeof(struct disp_path_config_mem_out_struct);
			param[1].value.a = pConfig->security;
			paramTypes = TZ_ParamTypes2(TZPT_MEMREF_INPUT, TZPT_VALUE_INPUT);
			DISP_DBG("wdma config handle=0x%x\n", param[0].memref.handle);

			ret =
			    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_OVL_WDMA_CONFIG,
						paramTypes, param);
			if (ret != TZ_RESULT_SUCCESS) {
				/* error */
				DISP_ERR("TZCMD_DDP_OVL_WDMA_CONFIG fail, ret=%d\n", ret);
			}

			ret =
			    KREE_UnregisterSharedmem(ddp_mem_session_handle(),
						     p_wdma_config_share_handle);
			if (ret) {
				DISP_ERR
				    ("UnregisterSharedmem %s share_handle Error:%d, line:%d, session(%d)",
				     __func__, ret, __LINE__,
				     (unsigned int)ddp_mem_session_handle());
			}
		} else
#endif
		{
			/* config wdma1 */
			WDMAReset(1);
			WDMAConfig(1,
				   WDMA_INPUT_FORMAT_ARGB,
				   pConfig->srcROI.width,
				   pConfig->srcROI.height,
				   0,
				   0,
				   pConfig->srcROI.width,
				   pConfig->srcROI.height,
				   pConfig->outFormat,
				   pConfig->dstAddr, pConfig->srcROI.width, 1, 0);
			WDMAStart(1);
		}

	} else {
		disp_unregister_irq(DISP_MODULE_WDMA1, _disp_path_ovl_wdma_callback);
	}

	return 0;
}
#endif


UINT32 fb_width = 0;
UINT32 fb_height = 0;
int disp_path_config(struct disp_path_config_struct *pConfig)
{
	fb_width = pConfig->srcROI.width;
	fb_height = pConfig->srcROI.height;
	return disp_path_config_(pConfig, gMutexID);
}

int disp_path_config_internal_setting(struct disp_path_config_struct *pConfig)
{
	fb_width = pConfig->srcROI.width;
	fb_height = pConfig->srcROI.height;
	return disp_path_config_setting(pConfig, gMutexID);
}

int disp_path_config_internal_mutex(struct disp_path_config_struct *pConfig)
{
	return disp_path_config_mutex(pConfig, gMutexID);
}


DISP_MODULE_ENUM g_dst_module;
int disp_path_config_(struct disp_path_config_struct *pConfig, int mutexId)
{

	disp_path_config_mutex(pConfig, mutexId);

	return disp_path_config_setting(pConfig, mutexId);
}

int disp_path_config_mutex(struct disp_path_config_struct *pConfig, int mutexId)
{
	unsigned int mutexMode;
	unsigned int mutexValue;

	DISP_DBG("[DDP] disp_path_config(), srcModule=%d, addr=0x%x, inFormat=%d",
		 pConfig->srcModule, pConfig->addr, pConfig->inFormat);

	DISP_DBG
	    ("[DDP] disp_path_config(),pitch=%d, bgROI(%d,%d,%d,%d), bgColor=%d, outFormat=%d, dstModule=%d",
	     pConfig->pitch, pConfig->bgROI.x, pConfig->bgROI.y, pConfig->bgROI.width,
	     pConfig->bgROI.height, pConfig->bgColor, pConfig->outFormat, pConfig->dstModule);

	DISP_DBG("[DDP] disp_path_config(),dstAddr=0x%x, dstPitch=%d, mutexId=%d ",
		 pConfig->dstAddr, pConfig->dstPitch, mutexId);

	g_dst_module = pConfig->dstModule;

	if (pConfig->srcModule == DISP_MODULE_RDMA0 && pConfig->dstModule == DISP_MODULE_WDMA1) {
		DISP_ERR("rdma0 wdma1 can not enable together!\n");
		return -1;
	}

	switch (pConfig->dstModule) {
	case DISP_MODULE_DSI_VDO:
		mutexMode = 1;
		break;

	case DISP_MODULE_DPI0:
		mutexMode = 2;
		break;

	case DISP_MODULE_DPI1:
	case DISP_MODULE_WDMA0:	/* FIXME: for hdmi temp */
		mutexMode = 3;
		if (pConfig->srcModule == DISP_MODULE_SCL) {
			/* mutexMode = 0 */
			mutexMode = 0;
		}
		break;

	case DISP_MODULE_DBI:
	case DISP_MODULE_DSI_CMD:
	case DISP_MODULE_WDMA1:
		mutexMode = 0;
		break;

	default:
		mutexMode = 0;
		DISP_ERR("unknown dstModule=%d\n", pConfig->dstModule);
		return -1;
	}


	if (pConfig->srcModule == DISP_MODULE_RDMA0) {
		/*ALPS01365979 [MT8135][KK.AOSPTC6][KKAOSP_SVP] Brightness adjusted not work when HDMI is connecting */
		mutexValue = 1 << 7 | 1 << 9;	/* rdma0=7, BLS=9(for backlight pwm control when hdmi connect) */
	} else if (pConfig->srcModule == DISP_MODULE_RDMA1) {
		mutexValue = 1 << 8;	/* rdma1=8 */
	} else if (pConfig->srcModule == DISP_MODULE_SCL && pConfig->dstModule == DISP_MODULE_WDMA0) {
		mutexValue = 1 | 1 << 1 | 1 << 5;	/* rot=0, scl=1, wdma0=5 */
	} else {
		if (pConfig->dstModule == DISP_MODULE_WDMA1) {
			mutexValue = 1 << 2 | 1 << 6;	/* ovl=2, wdma1=6 */
		} else {
#if defined(CONFIG_MTK_AAL_SUPPORT)
			/* mutexValue = 1<<2 | 1<<3 | 1<<4 | 1<<7 | 1<<9; //ovl=2, rdma0=7, Color=3, TDSHP=4, BLS=9 */
			/* Cvs: de-couple TDSHP from OVL stream */
			mutexValue = 1 << 2 | 1 << 3 | 1 << 7 | 1 << 9;	/* ovl=2, rdma0=7, Color=3, TDSHP=4, BLS=9 */
#else
			/* Elsa: de-couple BLS from OVL stream */
			mutexValue = 1 << 2 | 1 << 3 | 1 << 7;	/* ovl=2, rdma0=7, Color=3, TDSHP=4, */
#endif
		}
	}
	DISP_DBG("[DDP] %p mutex value : %x (mode : %d) (id : %d)\n",
		 (void *)DISP_REG_CONFIG_MUTEX_MOD(mutexId), mutexValue, mutexMode, mutexId);

	if (pConfig->dstModule != DISP_MODULE_DSI_VDO && pConfig->dstModule != DISP_MODULE_DPI0) {
		DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(mutexId), 1);
		DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(mutexId), 0);
	}

	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(mutexId), mutexValue);
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_SOF(mutexId), mutexMode);

	disp_update_mutex();

	return 0;
}

int disp_path_config_setting(struct disp_path_config_struct *pConfig, int mutexId)
{

#ifdef DDP_USE_CLOCK_API

#else
	/* TODO: clock manager sholud manager the clock ,not here */
	DISP_REG_SET(DISP_REG_CONFIG_CG_CLR0, 0xFFFFFFFF);
	DISP_REG_SET(DISP_REG_CONFIG_CG_CLR1, 0xFFFFFFFF);
#endif

	/* disp_path_get_mutex(); */

	/* /> config config reg */
	switch (pConfig->dstModule) {
	case DISP_MODULE_DSI:
	case DISP_MODULE_DSI_VDO:
	case DISP_MODULE_DSI_CMD:

		DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, 0x4);	/* ovl_mout output to COLOR */
		DISP_REG_SET(DISP_REG_CONFIG_COLOR_MOUT_EN, 0x8);	/* color_mout output to BLS */

		DISP_REG_SET(DISP_REG_CONFIG_COLOR_SEL, 1);	/* color_sel from ovl */
		DISP_REG_SET(DISP_REG_CONFIG_BLS_SEL, 1);	/* bls_sel from COLOR */

		/*
		   DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, 0x4);   // ovl_mout output to Color
		   DISP_REG_SET(DISP_REG_CONFIG_TDSHP_MOUT_EN, 0x10); // tdshp_mout output to OVL directlink
		   DISP_REG_SET(DISP_REG_CONFIG_COLOR_MOUT_EN, 0x8); // color_mout output to BLS

		   DISP_REG_SET(DISP_REG_CONFIG_TDSHP_SEL, 0);         // tdshp_sel from overlay before blending
		   DISP_REG_SET(DISP_REG_CONFIG_COLOR_SEL, 1);         // color_sel from overla after blending
		   DISP_REG_SET(DISP_REG_CONFIG_BLS_SEL, 1);         // bls_sel from COLOR
		 */
		DISP_REG_SET(DISP_REG_CONFIG_RDMA0_OUT_SEL, 0);	/* rdma0_mout to dsi0 */
		break;

	case DISP_MODULE_DPI0:
		if (pConfig->srcModule == DISP_MODULE_RDMA1) {
			DISP_REG_SET(DISP_REG_CONFIG_RDMA1_OUT_SEL, 0x1);	/* rdma1_mout to dpi0 */
			DISP_REG_SET(DISP_REG_CONFIG_DPI0_SEL, 1);	/* dpi0_sel from rdma1 */
			DISP_REG_SET(DISP_REG_CONFIG_MISC, 0x1);	/* set DPI IO for DPI usage */
		} else {
			DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, 0x4);	/* ovl_mout output to COLOR */
			DISP_REG_SET(DISP_REG_CONFIG_COLOR_MOUT_EN, 0x8);	/* color_mout output to BLS */

			DISP_REG_SET(DISP_REG_CONFIG_COLOR_SEL, 1);	/* color_sel from ovl */
			DISP_REG_SET(DISP_REG_CONFIG_BLS_SEL, 1);	/* bls_sel from COLOR */

			/*
			   DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, 0x4);   // ovl_mout output to Color
			   DISP_REG_SET(DISP_REG_CONFIG_TDSHP_MOUT_EN, 0x10); // tdshp_mout output to OVL directlink
			   DISP_REG_SET(DISP_REG_CONFIG_COLOR_MOUT_EN, 0x8); // color_mout output to BLS

			   DISP_REG_SET(DISP_REG_CONFIG_TDSHP_SEL, 0);         // tdshp_sel from overlay before blending
			   DISP_REG_SET(DISP_REG_CONFIG_COLOR_SEL, 1);         // color_sel from overla after blending
			   DISP_REG_SET(DISP_REG_CONFIG_BLS_SEL, 1);         // bls_sel from COLOR
			 */

			DISP_REG_SET(DISP_REG_CONFIG_RDMA0_OUT_SEL, 0x2);	/* rdma0_mout to dpi0 */
			DISP_REG_SET(DISP_REG_CONFIG_DPI0_SEL, 0);	/* dpi0_sel from rdma0 */
		}
		break;

	case DISP_MODULE_DPI1:
		DISP_REG_SET(DISP_REG_CONFIG_RDMA1_OUT_SEL, 0x2);	/* 2 for DPI1 */
		DISP_REG_SET(DISP_REG_CONFIG_DPI1_SEL, 0x1);	/* 1 for RDMA1 */
		break;

	case DISP_MODULE_DBI:
		DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, 0x4);	/* ovl_mout output to COLOR */
		DISP_REG_SET(DISP_REG_CONFIG_COLOR_MOUT_EN, 0x8);	/* color_mout output to BLS */

		DISP_REG_SET(DISP_REG_CONFIG_COLOR_SEL, 1);	/* color_sel from ovl */
		DISP_REG_SET(DISP_REG_CONFIG_BLS_SEL, 1);	/* bls_sel from COLOR */

		/*
		   DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, 0x4);   // ovl_mout output to Color
		   DISP_REG_SET(DISP_REG_CONFIG_TDSHP_MOUT_EN, 0x10); // tdshp_mout output to OVL directlink
		   DISP_REG_SET(DISP_REG_CONFIG_COLOR_MOUT_EN, 0x8); // color_mout output to BLS

		   DISP_REG_SET(DISP_REG_CONFIG_TDSHP_SEL, 0);         // tdshp_sel from overlay before blending
		   DISP_REG_SET(DISP_REG_CONFIG_COLOR_SEL, 1);         // color_sel from overla after blending
		   DISP_REG_SET(DISP_REG_CONFIG_BLS_SEL, 1);         // bls_sel from COLOR
		 */

		DISP_REG_SET(DISP_REG_CONFIG_RDMA0_OUT_SEL, 0x1);	/* rdma0_mout to dbi */
		DISP_REG_SET(DISP_REG_CONFIG_DBI_SEL, 0);	/* dbi_sel from rdma0 */

		break;
	case DISP_MODULE_WDMA0:
		if (pConfig->srcModule == DISP_MODULE_SCL) {
			DISP_REG_SET(DISP_REG_CONFIG_WDMA0_SEL, 0);	/* 0 for SCL */
			DISP_REG_SET(DISP_REG_CONFIG_SCL_MOUT_EN, 1 << 0);	/* 0 for WDMA0 */
		}
		break;
	case DISP_MODULE_WDMA1:
		DISP_REG_SET(DISP_REG_CONFIG_OVL_MOUT_EN, 0x1);	/* ovl_mout output to wdma1 */
		break;
	default:
		DISP_ERR("unknown dstModule=%d\n", pConfig->dstModule);
	}

	/* /> config engines */
	if (pConfig->srcModule == DISP_MODULE_OVL) {	/* config OVL */

		OVLROI(pConfig->bgROI.width,	/* width */
		       pConfig->bgROI.height,	/* height */
		       pConfig->bgColor);	/* background B */

		if (pConfig->dstModule != DISP_MODULE_DSI_VDO
		    && pConfig->dstModule != DISP_MODULE_DPI0) {
			OVLStop();
			/* OVLReset(); */
		}
		if (pConfig->ovl_config.layer < 4) {
				disp_path_config_layer_(&pConfig->ovl_config,
					(int)pConfig->ovl_config.security);
#if 0
			OVLLayerSwitch(pConfig->ovl_config.layer, pConfig->ovl_config.layer_en);
			if (pConfig->ovl_config.layer_en != 0) {
				if (pConfig->ovl_config.addr == 0 ||
				    pConfig->ovl_config.dst_w == 0 ||
				    pConfig->ovl_config.dst_h == 0) {
					DISP_ERR
					    ("ovl parameter invalidate, addr=0x%x, w=%d, h=%d\n",
					     pConfig->ovl_config.addr, pConfig->ovl_config.dst_w,
					     pConfig->ovl_config.dst_h);
					return -1;
				}

				OVLLayerConfig(pConfig->ovl_config.layer,	/* layer */
					       pConfig->ovl_config.source,	/* data source (0=memory) */
					       pConfig->ovl_config.fmt, pConfig->ovl_config.addr,	/* addr */
					       pConfig->ovl_config.src_x,	/* x */
					       pConfig->ovl_config.src_y,	/* y */
					       pConfig->ovl_config.src_pitch,	/* pitch, pixel number */
					       pConfig->ovl_config.dst_x,	/* x */
					       pConfig->ovl_config.dst_y,	/* y */
					       pConfig->ovl_config.dst_w,	/* width */
					       pConfig->ovl_config.dst_h,	/* height */
					       pConfig->ovl_config.keyEn,	/* color key */
					       pConfig->ovl_config.key,	/* color key */
					       pConfig->ovl_config.aen,	/* alpha enable */
					       pConfig->ovl_config.alpha);	/* alpha */
			}
#endif
		} else {
			DISP_ERR("layer ID undefined! %d\n", pConfig->ovl_config.layer);
		}
		if (disp_path_get_ovl_en() == 0)
			OVLSWReset();
		OVLStart();

		if (pConfig->dstModule == DISP_MODULE_WDMA1) {	/* 1. mem->ovl->wdma1->mem */
			WDMAReset(1);
			if (pConfig->dstAddr == 0 ||
			    pConfig->srcROI.width == 0 || pConfig->srcROI.height == 0) {
				DISP_ERR("wdma parameter invalidate, addr=0x%x, w=%d, h=%d\n",
					 pConfig->dstAddr,
					 pConfig->srcROI.width, pConfig->srcROI.height);
				return -1;
			}

			WDMAConfig(1,
				   WDMA_INPUT_FORMAT_ARGB,
				   pConfig->srcROI.width,
				   pConfig->srcROI.height,
				   0,
				   0,
				   pConfig->srcROI.width,
				   pConfig->srcROI.height,
				   pConfig->outFormat,
				   pConfig->dstAddr, pConfig->srcROI.width, 1, 0);
			WDMAStart(1);
		} else {	/* 2. ovl->bls->rdma0->lcd */

#if defined(CONFIG_MTK_AAL_SUPPORT)
			disp_bls_init(pConfig->srcROI.width, pConfig->srcROI.height);
#endif
			/* =============================config PQ start================================== */
#if 0
			DpEngine_SHARPonInit();
			DpEngine_SHARPonConfig(pConfig->srcROI.width,	/* width */
					       pConfig->srcROI.height);	/* height */
#endif

			DpEngine_COLORonInit();
			DpEngine_COLORonConfig(pConfig->srcROI.width,	/* width */
					       pConfig->srcROI.height);	/* height */


			/* =============================config PQ end================================== */
			/* /config RDMA */
			if (pConfig->dstModule != DISP_MODULE_DSI_VDO
			    && pConfig->dstModule != DISP_MODULE_DPI0) {
				RDMAStop(0);
				RDMAReset(0);
			}
			if (pConfig->srcROI.width == 0 || pConfig->srcROI.height == 0) {
				DISP_ERR("rdma parameter invalidate, w=%d, h=%d\n",
					 pConfig->srcROI.width, pConfig->srcROI.height);
				return -1;
			}
			RDMAConfig(0, RDMA_MODE_DIRECT_LINK,	/* /direct link mode */
				   RDMA_INPUT_FORMAT_RGB888,	/* inputFormat */
				   0,	/* address */
				   pConfig->outFormat,	/* output format */
				   pConfig->pitch,	/* pitch */
				   pConfig->srcROI.width,	/* width */
				   pConfig->srcROI.height,	/* height */
				   0,	/* byte swap */
				   0);	/* is RGB swap */

			RDMAStart(0);
		}
	} else if (pConfig->srcModule == DISP_MODULE_RDMA0
		   || pConfig->srcModule == DISP_MODULE_RDMA1) {
		int index = pConfig->srcModule == DISP_MODULE_RDMA0 ? 0 : 1;

		/* /config RDMA */
		if (pConfig->dstModule != DISP_MODULE_DSI_VDO
		    && pConfig->dstModule != DISP_MODULE_DPI0
		    && pConfig->srcModule != DISP_MODULE_RDMA1) {
			RDMAStop(index);
			RDMAReset(index);
		}
		if (pConfig->addr == 0 || pConfig->srcWidth == 0 || pConfig->srcHeight == 0) {
			DISP_ERR("rdma parameter invalidate, addr=0x%x, w=%d, h=%d\n",
				 pConfig->addr, pConfig->srcWidth, pConfig->srcHeight);
			return -1;
		}
		RDMAConfig(index, RDMA_MODE_MEMORY,	/* /direct link mode */
			   pConfig->inFormat,	/* inputFormat */
			   pConfig->addr,	/* address */
			   pConfig->outFormat,	/* output format */
			   pConfig->pitch,	/*  */
			   pConfig->srcWidth, pConfig->srcHeight, 0,	/* byte swap */
			   0);	/* is RGB swap */
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		/* RDMA1 will be started by HDMITX */
		if(index != 1)
			RDMAStart(index);
#else
		RDMAStart(index);
#endif
		disp_dump_reg(pConfig->srcModule);
	} else if (pConfig->srcModule == DISP_MODULE_SCL) {
		unsigned int memAddr[3] = { pConfig->addr, 0, 0 };
		DISP_COLOR_FORMAT outFormat;

		/* ROT */
		ROTStop();
		ROTReset();
		ROTConfig(0,
			  DISP_INTERLACE_FORMAT_NONE,
			  0,
			  pConfig->inFormat,
			  memAddr,
			  pConfig->srcWidth,
			  pConfig->srcHeight, pConfig->pitch, pConfig->srcROI, &outFormat);
		ROTStart();
		/* disp_dump_reg(DISP_MODULE_ROT); */

		/* SCL */
		SCLStop();
		SCLReset();
		SCLConfig(DISP_INTERLACE_FORMAT_NONE,
			  0,
			  pConfig->srcWidth,
			  pConfig->srcHeight, pConfig->dstWidth, pConfig->dstHeight, 0);
		SCLStart();

		/* disp_dump_reg(DISP_MODULE_SCL); */

		/* ROT->SCL->WDMA0 */
		if (pConfig->dstModule == DISP_MODULE_WDMA0) {
			WDMAReset(0);
			WDMAConfig(0, WDMA_INPUT_FORMAT_YUV444,	/* from SCL */
				   pConfig->dstWidth,
				   pConfig->dstHeight,
				   0,
				   0,
				   pConfig->dstWidth,
				   pConfig->dstHeight,
				   pConfig->outFormat, pConfig->dstAddr, pConfig->dstWidth, 1, 0);
			WDMAStart(0);
			/* disp_dump_reg(DISP_MODULE_WDMA0); */
		}


	}

/*************************************************/
/* Ultra config */
	/* ovl ultra 0x40402020 */
	if (gOvlSecure == 0) {
		/* DISP_REG_SET(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING, 0x40402020); */
		/* DISP_REG_SET(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING, 0x40402020); */
		/* DISP_REG_SET(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING, 0x40402020); */
		/* DISP_REG_SET(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING, 0x40402020); */
	} else {
		/* TODO: set ovl reg in TEE */
	}

#if 0
	/* disp_rdma0 ultra */
	DISP_REG_SET(DISP_REG_RDMA_MEM_GMC_SETTING_0, 0x00000000);
	/* disp_rdma1 ultra */
	DISP_REG_SET(DISP_REG_RDMA_MEM_GMC_SETTING_0 + 0x1000, 0xF0F0F0F0);
#else
	/* disp_rdma0 ultra */
	DISP_REG_SET(DISP_REG_RDMA_MEM_GMC_SETTING_0, 0x101010C0);
	/* disp_rdma1 ultra */
	DISP_REG_SET(DISP_REG_RDMA_MEM_GMC_SETTING_0 + 0x1000, 0x101010C0);
#endif
	/* disp_wdma0 ultra */
	/* DISP_REG_SET(DISP_REG_WDMA_BUF_CON1, 0x10000000); */
	/* DISP_REG_SET(DISP_REG_WDMA_BUF_CON2, 0x20402020); */
	/* disp_wdma1 ultra */
	/* DISP_REG_SET(DISP_REG_WDMA_BUF_CON1+0x1000, 0x800800ff); */

	/* only need to set wdma1 when overlay source */
	if (pConfig->srcModule == DISP_MODULE_OVL)
		DISP_REG_SET(DISP_REG_WDMA_BUF_CON2 + 0x1000, 0x20200808);

/*************************************************/
	/* TDOD: add debug cmd in display to dump register */
/* disp_dump_reg(DISP_MODULE_OVL); */
/* disp_dump_reg(DISP_MODULE_WDMA1); */
/* disp_dump_reg(DISP_MODULE_DPI0); */
/* disp_dump_reg(DISP_MODULE_RDMA0); */
/* disp_dump_reg(DISP_MODULE_CONFIG); */

/* disp_path_release_mutex(); */

	return 0;
}

#ifdef DDP_USE_CLOCK_API
unsigned int reg_offset = 0;
/* #define DDP_RECORD_REG_BACKUP_RESTORE  // print the reg value before backup and after restore */
void reg_backup(unsigned int reg_addr)
{
	*(pRegBackup + reg_offset) = DISP_REG_GET(reg_addr);
#ifdef DDP_RECORD_REG_BACKUP_RESTORE
	pr_info("0x%08x(0x%08x), ", reg_addr, *(pRegBackup + reg_offset));
	if ((reg_offset + 1) % 8 == 0)
		pr_info("\n");
#endif
	reg_offset++;
	if (reg_offset >= DDP_BACKUP_REG_NUM) {
		DISP_ERR("reg_backup fail, reg_offset=%d, regBackupSize=%d\n", reg_offset,
			 DDP_BACKUP_REG_NUM);
	}
}

void reg_restore(unsigned int reg_addr)
{
	DISP_REG_SET(reg_addr, *(pRegBackup + reg_offset));
#ifdef DDP_RECORD_REG_BACKUP_RESTORE
	pr_info("0x%08x(0x%08x), ", reg_addr, DISP_REG_GET(reg_addr));
	if ((reg_offset + 1) % 8 == 0)
		pr_info("\n");
#endif
	reg_offset++;

	if (reg_offset >= DDP_BACKUP_REG_NUM) {
		DISP_ERR("reg_backup fail, reg_offset=%d, regBackupSize=%d\n", reg_offset,
			 DDP_BACKUP_REG_NUM);
	}
}

static int disp_reg_backup(void)
{
	reg_offset = 0;
	DISP_DBG("disp_reg_backup() start, *pRegBackup=0x%x, reg_offset=%d ", *pRegBackup,
		 reg_offset);
	MMProfileLogEx(DDP_MMP_Events.BackupReg, MMProfileFlagStart, 0, 0);
	/* Config */
	/* reg_backup(DISP_REG_CONFIG_SCL_MOUT_EN      ); */
	reg_backup(DISP_REG_CONFIG_OVL_MOUT_EN);
	reg_backup(DISP_REG_CONFIG_COLOR_MOUT_EN);
	reg_backup(DISP_REG_CONFIG_TDSHP_MOUT_EN);
	reg_backup(DISP_REG_CONFIG_MOUT_RST);
	reg_backup(DISP_REG_CONFIG_RDMA0_OUT_SEL);
	reg_backup(DISP_REG_CONFIG_RDMA1_OUT_SEL);
	reg_backup(DISP_REG_CONFIG_OVL_PQ_OUT_SEL);
	/* reg_backup(DISP_REG_CONFIG_WDMA0_SEL        ); */
	reg_backup(DISP_REG_CONFIG_OVL_SEL);
	reg_backup(DISP_REG_CONFIG_OVL_PQ_IN_SEL);
	reg_backup(DISP_REG_CONFIG_COLOR_SEL);
	reg_backup(DISP_REG_CONFIG_TDSHP_SEL);
	reg_backup(DISP_REG_CONFIG_BLS_SEL);
	reg_backup(DISP_REG_CONFIG_DBI_SEL);
	reg_backup(DISP_REG_CONFIG_DPI0_SEL);
	reg_backup(DISP_REG_CONFIG_MISC);
	reg_backup(DISP_REG_CONFIG_PATH_DEBUG0);
	reg_backup(DISP_REG_CONFIG_PATH_DEBUG1);
	reg_backup(DISP_REG_CONFIG_PATH_DEBUG2);
	reg_backup(DISP_REG_CONFIG_PATH_DEBUG3);
	reg_backup(DISP_REG_CONFIG_PATH_DEBUG4);
/* reg_backup(DISP_REG_CONFIG_CG_CON0          ); */
/* reg_backup(DISP_REG_CONFIG_CG_SET0          ); */
/* reg_backup(DISP_REG_CONFIG_CG_CLR0          ); */
/* reg_backup(DISP_REG_CONFIG_CG_CON1          ); */
/* reg_backup(DISP_REG_CONFIG_CG_SET1          ); */
/* reg_backup(DISP_REG_CONFIG_CG_CLR1          ); */
	reg_backup(DISP_REG_CONFIG_HW_DCM_EN0);
	reg_backup(DISP_REG_CONFIG_HW_DCM_EN_SET0);
	reg_backup(DISP_REG_CONFIG_HW_DCM_EN_CLR0);
	reg_backup(DISP_REG_CONFIG_HW_DCM_EN1);
	reg_backup(DISP_REG_CONFIG_HW_DCM_EN_SET1);
	reg_backup(DISP_REG_CONFIG_HW_DCM_EN_CLR1);
	reg_backup(DISP_REG_CONFIG_MBIST_DONE0);
	reg_backup(DISP_REG_CONFIG_MBIST_DONE1);
	reg_backup(DISP_REG_CONFIG_MBIST_FAIL0);
	reg_backup(DISP_REG_CONFIG_MBIST_FAIL1);
	reg_backup(DISP_REG_CONFIG_MBIST_FAIL2);
	reg_backup(DISP_REG_CONFIG_MBIST_HOLDB0);
	reg_backup(DISP_REG_CONFIG_MBIST_HOLDB1);
	reg_backup(DISP_REG_CONFIG_MBIST_MODE0);
	reg_backup(DISP_REG_CONFIG_MBIST_MODE1);
	reg_backup(DISP_REG_CONFIG_MBIST_BSEL0);
	reg_backup(DISP_REG_CONFIG_MBIST_BSEL1);
	reg_backup(DISP_REG_CONFIG_MBIST_BSEL2);
	reg_backup(DISP_REG_CONFIG_MBIST_BSEL3);
	reg_backup(DISP_REG_CONFIG_MBIST_CON);
	reg_backup(DISP_REG_CONFIG_DEBUG_OUT_SEL);
	reg_backup(DISP_REG_CONFIG_TEST_CLK_SEL);
	reg_backup(DISP_REG_CONFIG_DUMMY);
	reg_backup(DISP_REG_CONFIG_MUTEX_INTEN);
	reg_backup(DISP_REG_CONFIG_MUTEX_INTSTA);
	reg_backup(DISP_REG_CONFIG_REG_UPD_TIMEOUT);
	reg_backup(DISP_REG_CONFIG_REG_COMMIT);
	reg_backup(DISP_REG_CONFIG_MUTEX0_EN);
	reg_backup(DISP_REG_CONFIG_MUTEX0);
	reg_backup(DISP_REG_CONFIG_MUTEX0_RST);
	reg_backup(DISP_REG_CONFIG_MUTEX0_MOD);
	reg_backup(DISP_REG_CONFIG_MUTEX0_SOF);
	reg_backup(DISP_REG_CONFIG_MUTEX1_EN);
	reg_backup(DISP_REG_CONFIG_MUTEX1);
	reg_backup(DISP_REG_CONFIG_MUTEX1_RST);
	reg_backup(DISP_REG_CONFIG_MUTEX1_MOD);
	reg_backup(DISP_REG_CONFIG_MUTEX1_SOF);
	reg_backup(DISP_REG_CONFIG_MUTEX2_EN);
	reg_backup(DISP_REG_CONFIG_MUTEX2);
	reg_backup(DISP_REG_CONFIG_MUTEX2_RST);
	reg_backup(DISP_REG_CONFIG_MUTEX2_MOD);
	reg_backup(DISP_REG_CONFIG_MUTEX2_SOF);
	reg_backup(DISP_REG_CONFIG_MUTEX3_EN);
	reg_backup(DISP_REG_CONFIG_MUTEX3);
	reg_backup(DISP_REG_CONFIG_MUTEX3_RST);
	reg_backup(DISP_REG_CONFIG_MUTEX3_MOD);
	reg_backup(DISP_REG_CONFIG_MUTEX3_SOF);
	reg_backup(DISP_REG_CONFIG_MUTEX4_EN);
	reg_backup(DISP_REG_CONFIG_MUTEX4);
	reg_backup(DISP_REG_CONFIG_MUTEX4_RST);
	reg_backup(DISP_REG_CONFIG_MUTEX4_MOD);
	reg_backup(DISP_REG_CONFIG_MUTEX4_SOF);
	reg_backup(DISP_REG_CONFIG_MUTEX5_EN);
	reg_backup(DISP_REG_CONFIG_MUTEX5);
	reg_backup(DISP_REG_CONFIG_MUTEX5_RST);
	reg_backup(DISP_REG_CONFIG_MUTEX5_MOD);
	reg_backup(DISP_REG_CONFIG_MUTEX5_SOF);
	reg_backup(DISP_REG_CONFIG_MUTEX_DEBUG_OUT_SEL);

	/* OVL */
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* When backup OVL registers, consider secure case */
	if (0 == gOvlSecure) {
#endif				/* CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT */
		reg_backup(DISP_REG_OVL_STA);
		reg_backup(DISP_REG_OVL_INTEN);
		reg_backup(DISP_REG_OVL_INTSTA);
		reg_backup(DISP_REG_OVL_EN);
		reg_backup(DISP_REG_OVL_TRIG);
		reg_backup(DISP_REG_OVL_RST);
		reg_backup(DISP_REG_OVL_ROI_SIZE);
		reg_backup(DISP_REG_OVL_DATAPATH_CON);
		reg_backup(DISP_REG_OVL_ROI_BGCLR);
		reg_backup(DISP_REG_OVL_SRC_CON);
		reg_backup(DISP_REG_OVL_L0_CON);
		reg_backup(DISP_REG_OVL_L0_SRCKEY);
		reg_backup(DISP_REG_OVL_L0_SRC_SIZE);
		reg_backup(DISP_REG_OVL_L0_OFFSET);
		reg_backup(DISP_REG_OVL_L0_ADDR);
		reg_backup(DISP_REG_OVL_L0_PITCH);
		reg_backup(DISP_REG_OVL_L1_CON);
		reg_backup(DISP_REG_OVL_L1_SRCKEY);
		reg_backup(DISP_REG_OVL_L1_SRC_SIZE);
		reg_backup(DISP_REG_OVL_L1_OFFSET);
		reg_backup(DISP_REG_OVL_L1_ADDR);
		reg_backup(DISP_REG_OVL_L1_PITCH);
		reg_backup(DISP_REG_OVL_L2_CON);
		reg_backup(DISP_REG_OVL_L2_SRCKEY);
		reg_backup(DISP_REG_OVL_L2_SRC_SIZE);
		reg_backup(DISP_REG_OVL_L2_OFFSET);
		reg_backup(DISP_REG_OVL_L2_ADDR);
		reg_backup(DISP_REG_OVL_L2_PITCH);
		reg_backup(DISP_REG_OVL_L3_CON);
		reg_backup(DISP_REG_OVL_L3_SRCKEY);
		reg_backup(DISP_REG_OVL_L3_SRC_SIZE);
		reg_backup(DISP_REG_OVL_L3_OFFSET);
		reg_backup(DISP_REG_OVL_L3_ADDR);
		reg_backup(DISP_REG_OVL_L3_PITCH);
		reg_backup(DISP_REG_OVL_RDMA0_CTRL);
		reg_backup(DISP_REG_OVL_RDMA0_MEM_START_TRIG);
		reg_backup(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING);
		reg_backup(DISP_REG_OVL_RDMA0_MEM_SLOW_CON);
		reg_backup(DISP_REG_OVL_RDMA0_FIFO_CTRL);
		reg_backup(DISP_REG_OVL_RDMA1_CTRL);
		reg_backup(DISP_REG_OVL_RDMA1_MEM_START_TRIG);
		reg_backup(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING);
		reg_backup(DISP_REG_OVL_RDMA1_MEM_SLOW_CON);
		reg_backup(DISP_REG_OVL_RDMA1_FIFO_CTRL);
		reg_backup(DISP_REG_OVL_RDMA2_CTRL);
		reg_backup(DISP_REG_OVL_RDMA2_MEM_START_TRIG);
		reg_backup(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING);
		reg_backup(DISP_REG_OVL_RDMA2_MEM_SLOW_CON);
		reg_backup(DISP_REG_OVL_RDMA2_FIFO_CTRL);
		reg_backup(DISP_REG_OVL_RDMA3_CTRL);
		reg_backup(DISP_REG_OVL_RDMA3_MEM_START_TRIG);
		reg_backup(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING);
		reg_backup(DISP_REG_OVL_RDMA3_MEM_SLOW_CON);
		reg_backup(DISP_REG_OVL_RDMA3_FIFO_CTRL);
		reg_backup(DISP_REG_OVL_L0_Y2R_PARA_R0);
		reg_backup(DISP_REG_OVL_L0_Y2R_PARA_R1);
		reg_backup(DISP_REG_OVL_L0_Y2R_PARA_G0);
		reg_backup(DISP_REG_OVL_L0_Y2R_PARA_G1);
		reg_backup(DISP_REG_OVL_L0_Y2R_PARA_B0);
		reg_backup(DISP_REG_OVL_L0_Y2R_PARA_B1);
		reg_backup(DISP_REG_OVL_L0_Y2R_PARA_YUV_A_0);
		reg_backup(DISP_REG_OVL_L0_Y2R_PARA_YUV_A_1);
		reg_backup(DISP_REG_OVL_L0_Y2R_PARA_RGB_A_0);
		reg_backup(DISP_REG_OVL_L0_Y2R_PARA_RGB_A_1);
		reg_backup(DISP_REG_OVL_L1_Y2R_PARA_R0);
		reg_backup(DISP_REG_OVL_L1_Y2R_PARA_R1);
		reg_backup(DISP_REG_OVL_L1_Y2R_PARA_G0);
		reg_backup(DISP_REG_OVL_L1_Y2R_PARA_G1);
		reg_backup(DISP_REG_OVL_L1_Y2R_PARA_B0);
		reg_backup(DISP_REG_OVL_L1_Y2R_PARA_B1);
		reg_backup(DISP_REG_OVL_L1_Y2R_PARA_YUV_A_0);
		reg_backup(DISP_REG_OVL_L1_Y2R_PARA_YUV_A_1);
		reg_backup(DISP_REG_OVL_L1_Y2R_PARA_RGB_A_0);
		reg_backup(DISP_REG_OVL_L1_Y2R_PARA_RGB_A_1);
		reg_backup(DISP_REG_OVL_L2_Y2R_PARA_R0);
		reg_backup(DISP_REG_OVL_L2_Y2R_PARA_R1);
		reg_backup(DISP_REG_OVL_L2_Y2R_PARA_G0);
		reg_backup(DISP_REG_OVL_L2_Y2R_PARA_G1);
		reg_backup(DISP_REG_OVL_L2_Y2R_PARA_B0);
		reg_backup(DISP_REG_OVL_L2_Y2R_PARA_B1);
		reg_backup(DISP_REG_OVL_L2_Y2R_PARA_YUV_A_0);
		reg_backup(DISP_REG_OVL_L2_Y2R_PARA_YUV_A_1);
		reg_backup(DISP_REG_OVL_L2_Y2R_PARA_RGB_A_0);
		reg_backup(DISP_REG_OVL_L2_Y2R_PARA_RGB_A_1);
		reg_backup(DISP_REG_OVL_L3_Y2R_PARA_R0);
		reg_backup(DISP_REG_OVL_L3_Y2R_PARA_R1);
		reg_backup(DISP_REG_OVL_L3_Y2R_PARA_G0);
		reg_backup(DISP_REG_OVL_L3_Y2R_PARA_G1);
		reg_backup(DISP_REG_OVL_L3_Y2R_PARA_B0);
		reg_backup(DISP_REG_OVL_L3_Y2R_PARA_B1);
		reg_backup(DISP_REG_OVL_L3_Y2R_PARA_YUV_A_0);
		reg_backup(DISP_REG_OVL_L3_Y2R_PARA_YUV_A_1);
		reg_backup(DISP_REG_OVL_L3_Y2R_PARA_RGB_A_0);
		reg_backup(DISP_REG_OVL_L3_Y2R_PARA_RGB_A_1);
		reg_backup(DISP_REG_OVL_DEBUG_MON_SEL);
		reg_backup(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING2);
		reg_backup(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING2);
		reg_backup(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING2);
		reg_backup(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING2);
		reg_backup(DISP_REG_OVL_FLOW_CTRL_DBG);
		reg_backup(DISP_REG_OVL_ADDCON_DBG);
		reg_backup(DISP_REG_OVL_OUTMUX_DBG);
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	} else {
		MTEEC_PARAM param[4];
		unsigned int paramTypes = 0;
		TZ_RESULT ret =
		    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_OVL_BACKUP_REG, paramTypes,
					param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("KREE_TeeServiceCall(TZCMD_DDP_OVL_BACKUP_REG) fail, ret=%d\n",
				 ret);
		}
	}
#endif				/* CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT */

	/* RDMA0 */
	reg_backup(DISP_REG_RDMA_INT_ENABLE);
	reg_backup(DISP_REG_RDMA_INT_STATUS);
	reg_backup(DISP_REG_RDMA_GLOBAL_CON);
	reg_backup(DISP_REG_RDMA_SIZE_CON_0);
	reg_backup(DISP_REG_RDMA_SIZE_CON_1);
	reg_backup(DISP_REG_RDMA_TARGET_LINE);
	reg_backup(DISP_REG_RDMA_MEM_CON);
	reg_backup(DISP_REG_RDMA_MEM_START_ADDR);
	reg_backup(DISP_REG_RDMA_MEM_SRC_PITCH);
	reg_backup(DISP_REG_RDMA_MEM_GMC_SETTING_0);
	reg_backup(DISP_REG_RDMA_MEM_SLOW_CON);
	reg_backup(DISP_REG_RDMA_MEM_GMC_SETTING_1);
	reg_backup(DISP_REG_RDMA_FIFO_CON);
	reg_backup(DISP_REG_RDMA_CF_00);
	reg_backup(DISP_REG_RDMA_CF_01);
	reg_backup(DISP_REG_RDMA_CF_02);
	reg_backup(DISP_REG_RDMA_CF_10);
	reg_backup(DISP_REG_RDMA_CF_11);
	reg_backup(DISP_REG_RDMA_CF_12);
	reg_backup(DISP_REG_RDMA_CF_20);
	reg_backup(DISP_REG_RDMA_CF_21);
	reg_backup(DISP_REG_RDMA_CF_22);
	reg_backup(DISP_REG_RDMA_CF_PRE_ADD0);
	reg_backup(DISP_REG_RDMA_CF_PRE_ADD1);
	reg_backup(DISP_REG_RDMA_CF_PRE_ADD2);
	reg_backup(DISP_REG_RDMA_CF_POST_ADD0);
	reg_backup(DISP_REG_RDMA_CF_POST_ADD1);
	reg_backup(DISP_REG_RDMA_CF_POST_ADD2);
	reg_backup(DISP_REG_RDMA_DUMMY);
	reg_backup(DISP_REG_RDMA_DEBUG_OUT_SEL);

	/* RDMA1 */
	/* reg_backup(DISP_REG_RDMA_INT_ENABLE + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_INT_STATUS + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_GLOBAL_CON + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_SIZE_CON_0 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_SIZE_CON_1 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_TARGET_LINE + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_MEM_CON + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_MEM_START_ADDR + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_MEM_SRC_PITCH + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_MEM_GMC_SETTING_0 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_MEM_SLOW_CON + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_MEM_GMC_SETTING_1 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_FIFO_CON + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_00 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_01 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_02 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_10 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_11 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_12 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_20 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_21 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_22 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_PRE_ADD0 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_PRE_ADD1 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_PRE_ADD2 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_POST_ADD0 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_POST_ADD1 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_CF_POST_ADD2 + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_DUMMY + 0x1000); */
	/* reg_backup(DISP_REG_RDMA_DEBUG_OUT_SEL + 0x1000); */
#if 0
	/* ROTDMA */
	reg_backup(DISP_REG_ROT_EN);
	reg_backup(DISP_REG_ROT_RESET);
	reg_backup(DISP_REG_ROT_INTERRUPT_ENABLE);
	reg_backup(DISP_REG_ROT_INTERRUPT_STATUS);
	reg_backup(DISP_REG_ROT_CON);
	reg_backup(DISP_REG_ROT_GMCIF_CON);
	reg_backup(DISP_REG_ROT_SRC_CON);
	reg_backup(DISP_REG_ROT_SRC_BASE_0);
	reg_backup(DISP_REG_ROT_SRC_BASE_1);
	reg_backup(DISP_REG_ROT_SRC_BASE_2);
	reg_backup(DISP_REG_ROT_MF_BKGD_SIZE_IN_BYTE);
	reg_backup(DISP_REG_ROT_MF_SRC_SIZE);
	reg_backup(DISP_REG_ROT_MF_CLIP_SIZE);
	reg_backup(DISP_REG_ROT_MF_OFFSET_1);
	reg_backup(DISP_REG_ROT_MF_PAR);
	reg_backup(DISP_REG_ROT_SF_BKGD_SIZE_IN_BYTE);
	reg_backup(DISP_REG_ROT_SF_PAR);
	reg_backup(DISP_REG_ROT_MB_DEPTH);
	reg_backup(DISP_REG_ROT_MB_BASE);
	reg_backup(DISP_REG_ROT_MB_CON);
	reg_backup(DISP_REG_ROT_SB_DEPTH);
	reg_backup(DISP_REG_ROT_SB_BASE);
	reg_backup(DISP_REG_ROT_SB_CON);
	reg_backup(DISP_REG_ROT_VC1_RANGE);
	reg_backup(DISP_REG_ROT_TRANSFORM_0);
	reg_backup(DISP_REG_ROT_TRANSFORM_1);
	reg_backup(DISP_REG_ROT_TRANSFORM_2);
	reg_backup(DISP_REG_ROT_TRANSFORM_3);
	reg_backup(DISP_REG_ROT_TRANSFORM_4);
	reg_backup(DISP_REG_ROT_TRANSFORM_5);
	reg_backup(DISP_REG_ROT_TRANSFORM_6);
	reg_backup(DISP_REG_ROT_TRANSFORM_7);
	reg_backup(DISP_REG_ROT_RESV_DUMMY_0);

	/* SCL */
	reg_backup(DISP_REG_SCL_CTRL);
	reg_backup(DISP_REG_SCL_INTEN);
	reg_backup(DISP_REG_SCL_INTSTA);
	reg_backup(DISP_REG_SCL_STATUS);
	reg_backup(DISP_REG_SCL_CFG);
	reg_backup(DISP_REG_SCL_INP_CHKSUM);
	reg_backup(DISP_REG_SCL_OUTP_CHKSUM);
	reg_backup(DISP_REG_SCL_HRZ_CFG);
	reg_backup(DISP_REG_SCL_HRZ_SIZE);
	reg_backup(DISP_REG_SCL_HRZ_FACTOR);
	reg_backup(DISP_REG_SCL_HRZ_OFFSET);
	reg_backup(DISP_REG_SCL_VRZ_CFG);
	reg_backup(DISP_REG_SCL_VRZ_SIZE);
	reg_backup(DISP_REG_SCL_VRZ_FACTOR);
	reg_backup(DISP_REG_SCL_VRZ_OFFSET);
	reg_backup(DISP_REG_SCL_EXT_COEF);
	reg_backup(DISP_REG_SCL_PEAK_CFG);

	/* WDMA 0 */
	reg_backup(DISP_REG_WDMA_INTEN);
	reg_backup(DISP_REG_WDMA_INTSTA);
	reg_backup(DISP_REG_WDMA_EN);
	reg_backup(DISP_REG_WDMA_RST);
	reg_backup(DISP_REG_WDMA_SMI_CON);
	reg_backup(DISP_REG_WDMA_CFG);
	reg_backup(DISP_REG_WDMA_SRC_SIZE);
	reg_backup(DISP_REG_WDMA_CLIP_SIZE);
	reg_backup(DISP_REG_WDMA_CLIP_COORD);
	reg_backup(DISP_REG_WDMA_DST_ADDR);
	reg_backup(DISP_REG_WDMA_DST_W_IN_BYTE);
	reg_backup(DISP_REG_WDMA_ALPHA);
	reg_backup(DISP_REG_WDMA_BUF_ADDR);
	reg_backup(DISP_REG_WDMA_STA);
	reg_backup(DISP_REG_WDMA_BUF_CON1);
	reg_backup(DISP_REG_WDMA_BUF_CON2);
	reg_backup(DISP_REG_WDMA_C00);
	reg_backup(DISP_REG_WDMA_C02);
	reg_backup(DISP_REG_WDMA_C10);
	reg_backup(DISP_REG_WDMA_C12);
	reg_backup(DISP_REG_WDMA_C20);
	reg_backup(DISP_REG_WDMA_C22);
	reg_backup(DISP_REG_WDMA_PRE_ADD0);
	reg_backup(DISP_REG_WDMA_PRE_ADD2);
	reg_backup(DISP_REG_WDMA_POST_ADD0);
	reg_backup(DISP_REG_WDMA_POST_ADD2);
	reg_backup(DISP_REG_WDMA_DST_U_ADDR);
	reg_backup(DISP_REG_WDMA_DST_V_ADDR);
	reg_backup(DISP_REG_WDMA_DST_UV_PITCH);
	reg_backup(DISP_REG_WDMA_DITHER_CON);
	reg_backup(DISP_REG_WDMA_FLOW_CTRL_DBG);
	reg_backup(DISP_REG_WDMA_EXEC_DBG);
	reg_backup(DISP_REG_WDMA_CLIP_DBG);
#endif
	/* WDMA1 */
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* when backup WDMA 1 registers, consider secure case */
	if (0 == gMemOutSecure) {
#endif				/* CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT */
		reg_backup(DISP_REG_WDMA_INTEN + 0x1000);
		reg_backup(DISP_REG_WDMA_INTSTA + 0x1000);
		reg_backup(DISP_REG_WDMA_EN + 0x1000);
		reg_backup(DISP_REG_WDMA_RST + 0x1000);
		reg_backup(DISP_REG_WDMA_SMI_CON + 0x1000);
		reg_backup(DISP_REG_WDMA_CFG + 0x1000);
		reg_backup(DISP_REG_WDMA_SRC_SIZE + 0x1000);
		reg_backup(DISP_REG_WDMA_CLIP_SIZE + 0x1000);
		reg_backup(DISP_REG_WDMA_CLIP_COORD + 0x1000);
		reg_backup(DISP_REG_WDMA_DST_ADDR + 0x1000);
		reg_backup(DISP_REG_WDMA_DST_W_IN_BYTE + 0x1000);
		reg_backup(DISP_REG_WDMA_ALPHA + 0x1000);
		reg_backup(DISP_REG_WDMA_BUF_ADDR + 0x1000);
		reg_backup(DISP_REG_WDMA_STA + 0x1000);
		reg_backup(DISP_REG_WDMA_BUF_CON1 + 0x1000);
		reg_backup(DISP_REG_WDMA_BUF_CON2 + 0x1000);
		reg_backup(DISP_REG_WDMA_C00 + 0x1000);
		reg_backup(DISP_REG_WDMA_C02 + 0x1000);
		reg_backup(DISP_REG_WDMA_C10 + 0x1000);
		reg_backup(DISP_REG_WDMA_C12 + 0x1000);
		reg_backup(DISP_REG_WDMA_C20 + 0x1000);
		reg_backup(DISP_REG_WDMA_C22 + 0x1000);
		reg_backup(DISP_REG_WDMA_PRE_ADD0 + 0x1000);
		reg_backup(DISP_REG_WDMA_PRE_ADD2 + 0x1000);
		reg_backup(DISP_REG_WDMA_POST_ADD0 + 0x1000);
		reg_backup(DISP_REG_WDMA_POST_ADD2 + 0x1000);
		reg_backup(DISP_REG_WDMA_DST_U_ADDR + 0x1000);
		reg_backup(DISP_REG_WDMA_DST_V_ADDR + 0x1000);
		reg_backup(DISP_REG_WDMA_DST_UV_PITCH + 0x1000);
		reg_backup(DISP_REG_WDMA_DITHER_CON + 0x1000);
		reg_backup(DISP_REG_WDMA_FLOW_CTRL_DBG + 0x1000);
		reg_backup(DISP_REG_WDMA_EXEC_DBG + 0x1000);
		reg_backup(DISP_REG_WDMA_CLIP_DBG + 0x1000);
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	} else {
		MTEEC_PARAM param[4];
		unsigned int paramTypes = 0;
		TZ_RESULT ret =
		    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_WDMA_BACKUP_REG, paramTypes,
					param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("KREE_TeeServiceCall(TZCMD_DDP_WDMA_BACKUP_REG) fail, ret=%d\n",
				 ret);
		}
	}
#endif				/* CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT */

	/* BLS */
	reg_backup(DISP_REG_BLS_EN);
	reg_backup(DISP_REG_BLS_RST);
	reg_backup(DISP_REG_BLS_INTEN);
	reg_backup(DISP_REG_BLS_INTSTA);
	reg_backup(DISP_REG_BLS_SRC_SIZE);
	reg_backup(DISP_REG_BLS_PWM_DUTY);
	reg_backup(DISP_REG_BLS_PWM_DUTY_GAIN);
	reg_backup(DISP_REG_BLS_PWM_CON);
	reg_backup(DISP_REG_PWM_H_DURATION);
	reg_backup(DISP_REG_PWM_L_DURATION);
	reg_backup(DISP_REG_PWM_G_DURATION);
	reg_backup(DISP_REG_PWM_SEND_DATA0);
	reg_backup(DISP_REG_PWM_SEND_DATA1);
	reg_backup(DISP_REG_PWM_WAVE_NUM);
	reg_backup(DISP_REG_PWM_DATA_WIDTH);
	reg_backup(DISP_REG_PWM_THRESH);
	reg_backup(DISP_REG_PWM_SEND_WAVENUM);

	DISP_DBG("disp_reg_backup() end, *pRegBackup=0x%x, reg_offset=%d\n", *pRegBackup,
		 reg_offset);
	MMProfileLogEx(DDP_MMP_Events.BackupReg, MMProfileFlagEnd, 0, 0);

	return 0;
}

static int disp_reg_restore(void)
{
	reg_offset = 0;
	DISP_DBG("disp_reg_restore(*) start, *pRegBackup=0x%x, reg_offset=%d ", *pRegBackup,
		 reg_offset);

	MMProfileLogEx(DDP_MMP_Events.BackupReg, MMProfileFlagStart, 1, 0);
	/* disp_path_get_mutex(); */
	/* Config */
	/* reg_restore(DISP_REG_CONFIG_SCL_MOUT_EN      ); */
	reg_restore(DISP_REG_CONFIG_OVL_MOUT_EN);
	reg_restore(DISP_REG_CONFIG_COLOR_MOUT_EN);
	reg_restore(DISP_REG_CONFIG_TDSHP_MOUT_EN);
	reg_restore(DISP_REG_CONFIG_MOUT_RST);
	reg_restore(DISP_REG_CONFIG_RDMA0_OUT_SEL);
	reg_restore(DISP_REG_CONFIG_RDMA1_OUT_SEL);
	reg_restore(DISP_REG_CONFIG_OVL_PQ_OUT_SEL);
	/* reg_restore(DISP_REG_CONFIG_WDMA0_SEL        ); */
	reg_restore(DISP_REG_CONFIG_OVL_SEL);
	reg_restore(DISP_REG_CONFIG_OVL_PQ_IN_SEL);
	reg_restore(DISP_REG_CONFIG_COLOR_SEL);
	reg_restore(DISP_REG_CONFIG_TDSHP_SEL);
	reg_restore(DISP_REG_CONFIG_BLS_SEL);
	reg_restore(DISP_REG_CONFIG_DBI_SEL);
	reg_restore(DISP_REG_CONFIG_DPI0_SEL);
	reg_restore(DISP_REG_CONFIG_MISC);
	reg_restore(DISP_REG_CONFIG_PATH_DEBUG0);
	reg_restore(DISP_REG_CONFIG_PATH_DEBUG1);
	reg_restore(DISP_REG_CONFIG_PATH_DEBUG2);
	reg_restore(DISP_REG_CONFIG_PATH_DEBUG3);
	reg_restore(DISP_REG_CONFIG_PATH_DEBUG4);
/* reg_restore(DISP_REG_CONFIG_CG_CON0          ); */
/* reg_restore(DISP_REG_CONFIG_CG_SET0          ); */
/* reg_restore(DISP_REG_CONFIG_CG_CLR0          ); */
/* reg_restore(DISP_REG_CONFIG_CG_CON1          ); */
/* reg_restore(DISP_REG_CONFIG_CG_SET1          ); */
/* reg_restore(DISP_REG_CONFIG_CG_CLR1          ); */
	reg_restore(DISP_REG_CONFIG_HW_DCM_EN0);
	reg_restore(DISP_REG_CONFIG_HW_DCM_EN_SET0);
	reg_restore(DISP_REG_CONFIG_HW_DCM_EN_CLR0);
	reg_restore(DISP_REG_CONFIG_HW_DCM_EN1);
	reg_restore(DISP_REG_CONFIG_HW_DCM_EN_SET1);
	reg_restore(DISP_REG_CONFIG_HW_DCM_EN_CLR1);
	reg_restore(DISP_REG_CONFIG_MBIST_DONE0);
	reg_restore(DISP_REG_CONFIG_MBIST_DONE1);
	reg_restore(DISP_REG_CONFIG_MBIST_FAIL0);
	reg_restore(DISP_REG_CONFIG_MBIST_FAIL1);
	reg_restore(DISP_REG_CONFIG_MBIST_FAIL2);
	reg_restore(DISP_REG_CONFIG_MBIST_HOLDB0);
	reg_restore(DISP_REG_CONFIG_MBIST_HOLDB1);
	reg_restore(DISP_REG_CONFIG_MBIST_MODE0);
	reg_restore(DISP_REG_CONFIG_MBIST_MODE1);
	reg_restore(DISP_REG_CONFIG_MBIST_BSEL0);
	reg_restore(DISP_REG_CONFIG_MBIST_BSEL1);
	reg_restore(DISP_REG_CONFIG_MBIST_BSEL2);
	reg_restore(DISP_REG_CONFIG_MBIST_BSEL3);
	reg_restore(DISP_REG_CONFIG_MBIST_CON);
	reg_restore(DISP_REG_CONFIG_DEBUG_OUT_SEL);
	reg_restore(DISP_REG_CONFIG_TEST_CLK_SEL);
	reg_restore(DISP_REG_CONFIG_DUMMY);
	reg_restore(DISP_REG_CONFIG_MUTEX_INTEN);
	reg_restore(DISP_REG_CONFIG_MUTEX_INTSTA);
	reg_restore(DISP_REG_CONFIG_REG_UPD_TIMEOUT);
	reg_restore(DISP_REG_CONFIG_REG_COMMIT);
	reg_restore(DISP_REG_CONFIG_MUTEX0_EN);
	reg_restore(DISP_REG_CONFIG_MUTEX0);
	reg_restore(DISP_REG_CONFIG_MUTEX0_RST);
	reg_restore(DISP_REG_CONFIG_MUTEX0_MOD);
	reg_restore(DISP_REG_CONFIG_MUTEX0_SOF);
	reg_restore(DISP_REG_CONFIG_MUTEX1_EN);
	reg_restore(DISP_REG_CONFIG_MUTEX1);
	reg_restore(DISP_REG_CONFIG_MUTEX1_RST);
	reg_restore(DISP_REG_CONFIG_MUTEX1_MOD);
	reg_restore(DISP_REG_CONFIG_MUTEX1_SOF);
	reg_restore(DISP_REG_CONFIG_MUTEX2_EN);
	reg_restore(DISP_REG_CONFIG_MUTEX2);
	reg_restore(DISP_REG_CONFIG_MUTEX2_RST);
	reg_restore(DISP_REG_CONFIG_MUTEX2_MOD);
	reg_restore(DISP_REG_CONFIG_MUTEX2_SOF);
	reg_restore(DISP_REG_CONFIG_MUTEX3_EN);
	reg_restore(DISP_REG_CONFIG_MUTEX3);
	reg_restore(DISP_REG_CONFIG_MUTEX3_RST);
	reg_restore(DISP_REG_CONFIG_MUTEX3_MOD);
	reg_restore(DISP_REG_CONFIG_MUTEX3_SOF);
	reg_restore(DISP_REG_CONFIG_MUTEX4_EN);
	reg_restore(DISP_REG_CONFIG_MUTEX4);
	reg_restore(DISP_REG_CONFIG_MUTEX4_RST);
	reg_restore(DISP_REG_CONFIG_MUTEX4_MOD);
	reg_restore(DISP_REG_CONFIG_MUTEX4_SOF);
	reg_restore(DISP_REG_CONFIG_MUTEX5_EN);
	reg_restore(DISP_REG_CONFIG_MUTEX5);
	reg_restore(DISP_REG_CONFIG_MUTEX5_RST);
	reg_restore(DISP_REG_CONFIG_MUTEX5_MOD);
	reg_restore(DISP_REG_CONFIG_MUTEX5_SOF);
	reg_restore(DISP_REG_CONFIG_MUTEX_DEBUG_OUT_SEL);

	/* OVL */
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* when restoring OVL registers, consider secure case */
	if (0 == gOvlSecure) {
#endif				/* CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT */
		reg_restore(DISP_REG_OVL_STA);
		reg_restore(DISP_REG_OVL_INTEN);
		reg_restore(DISP_REG_OVL_INTSTA);
		reg_restore(DISP_REG_OVL_EN);
		reg_restore(DISP_REG_OVL_TRIG);
		reg_restore(DISP_REG_OVL_RST);
		reg_restore(DISP_REG_OVL_ROI_SIZE);
		reg_restore(DISP_REG_OVL_DATAPATH_CON);
		reg_restore(DISP_REG_OVL_ROI_BGCLR);
		reg_restore(DISP_REG_OVL_SRC_CON);
		reg_restore(DISP_REG_OVL_L0_CON);
		reg_restore(DISP_REG_OVL_L0_SRCKEY);
		reg_restore(DISP_REG_OVL_L0_SRC_SIZE);
		reg_restore(DISP_REG_OVL_L0_OFFSET);
		reg_restore(DISP_REG_OVL_L0_ADDR);
		reg_restore(DISP_REG_OVL_L0_PITCH);
		reg_restore(DISP_REG_OVL_L1_CON);
		reg_restore(DISP_REG_OVL_L1_SRCKEY);
		reg_restore(DISP_REG_OVL_L1_SRC_SIZE);
		reg_restore(DISP_REG_OVL_L1_OFFSET);
		reg_restore(DISP_REG_OVL_L1_ADDR);
		reg_restore(DISP_REG_OVL_L1_PITCH);
		reg_restore(DISP_REG_OVL_L2_CON);
		reg_restore(DISP_REG_OVL_L2_SRCKEY);
		reg_restore(DISP_REG_OVL_L2_SRC_SIZE);
		reg_restore(DISP_REG_OVL_L2_OFFSET);
		reg_restore(DISP_REG_OVL_L2_ADDR);
		reg_restore(DISP_REG_OVL_L2_PITCH);
		reg_restore(DISP_REG_OVL_L3_CON);
		reg_restore(DISP_REG_OVL_L3_SRCKEY);
		reg_restore(DISP_REG_OVL_L3_SRC_SIZE);
		reg_restore(DISP_REG_OVL_L3_OFFSET);
		reg_restore(DISP_REG_OVL_L3_ADDR);
		reg_restore(DISP_REG_OVL_L3_PITCH);
		reg_restore(DISP_REG_OVL_RDMA0_CTRL);
		reg_restore(DISP_REG_OVL_RDMA0_MEM_START_TRIG);
		reg_restore(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING);
		reg_restore(DISP_REG_OVL_RDMA0_MEM_SLOW_CON);
		reg_restore(DISP_REG_OVL_RDMA0_FIFO_CTRL);
		reg_restore(DISP_REG_OVL_RDMA1_CTRL);
		reg_restore(DISP_REG_OVL_RDMA1_MEM_START_TRIG);
		reg_restore(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING);
		reg_restore(DISP_REG_OVL_RDMA1_MEM_SLOW_CON);
		reg_restore(DISP_REG_OVL_RDMA1_FIFO_CTRL);
		reg_restore(DISP_REG_OVL_RDMA2_CTRL);
		reg_restore(DISP_REG_OVL_RDMA2_MEM_START_TRIG);
		reg_restore(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING);
		reg_restore(DISP_REG_OVL_RDMA2_MEM_SLOW_CON);
		reg_restore(DISP_REG_OVL_RDMA2_FIFO_CTRL);
		reg_restore(DISP_REG_OVL_RDMA3_CTRL);
		reg_restore(DISP_REG_OVL_RDMA3_MEM_START_TRIG);
		reg_restore(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING);
		reg_restore(DISP_REG_OVL_RDMA3_MEM_SLOW_CON);
		reg_restore(DISP_REG_OVL_RDMA3_FIFO_CTRL);
		reg_restore(DISP_REG_OVL_L0_Y2R_PARA_R0);
		reg_restore(DISP_REG_OVL_L0_Y2R_PARA_R1);
		reg_restore(DISP_REG_OVL_L0_Y2R_PARA_G0);
		reg_restore(DISP_REG_OVL_L0_Y2R_PARA_G1);
		reg_restore(DISP_REG_OVL_L0_Y2R_PARA_B0);
		reg_restore(DISP_REG_OVL_L0_Y2R_PARA_B1);
		reg_restore(DISP_REG_OVL_L0_Y2R_PARA_YUV_A_0);
		reg_restore(DISP_REG_OVL_L0_Y2R_PARA_YUV_A_1);
		reg_restore(DISP_REG_OVL_L0_Y2R_PARA_RGB_A_0);
		reg_restore(DISP_REG_OVL_L0_Y2R_PARA_RGB_A_1);
		reg_restore(DISP_REG_OVL_L1_Y2R_PARA_R0);
		reg_restore(DISP_REG_OVL_L1_Y2R_PARA_R1);
		reg_restore(DISP_REG_OVL_L1_Y2R_PARA_G0);
		reg_restore(DISP_REG_OVL_L1_Y2R_PARA_G1);
		reg_restore(DISP_REG_OVL_L1_Y2R_PARA_B0);
		reg_restore(DISP_REG_OVL_L1_Y2R_PARA_B1);
		reg_restore(DISP_REG_OVL_L1_Y2R_PARA_YUV_A_0);
		reg_restore(DISP_REG_OVL_L1_Y2R_PARA_YUV_A_1);
		reg_restore(DISP_REG_OVL_L1_Y2R_PARA_RGB_A_0);
		reg_restore(DISP_REG_OVL_L1_Y2R_PARA_RGB_A_1);
		reg_restore(DISP_REG_OVL_L2_Y2R_PARA_R0);
		reg_restore(DISP_REG_OVL_L2_Y2R_PARA_R1);
		reg_restore(DISP_REG_OVL_L2_Y2R_PARA_G0);
		reg_restore(DISP_REG_OVL_L2_Y2R_PARA_G1);
		reg_restore(DISP_REG_OVL_L2_Y2R_PARA_B0);
		reg_restore(DISP_REG_OVL_L2_Y2R_PARA_B1);
		reg_restore(DISP_REG_OVL_L2_Y2R_PARA_YUV_A_0);
		reg_restore(DISP_REG_OVL_L2_Y2R_PARA_YUV_A_1);
		reg_restore(DISP_REG_OVL_L2_Y2R_PARA_RGB_A_0);
		reg_restore(DISP_REG_OVL_L2_Y2R_PARA_RGB_A_1);
		reg_restore(DISP_REG_OVL_L3_Y2R_PARA_R0);
		reg_restore(DISP_REG_OVL_L3_Y2R_PARA_R1);
		reg_restore(DISP_REG_OVL_L3_Y2R_PARA_G0);
		reg_restore(DISP_REG_OVL_L3_Y2R_PARA_G1);
		reg_restore(DISP_REG_OVL_L3_Y2R_PARA_B0);
		reg_restore(DISP_REG_OVL_L3_Y2R_PARA_B1);
		reg_restore(DISP_REG_OVL_L3_Y2R_PARA_YUV_A_0);
		reg_restore(DISP_REG_OVL_L3_Y2R_PARA_YUV_A_1);
		reg_restore(DISP_REG_OVL_L3_Y2R_PARA_RGB_A_0);
		reg_restore(DISP_REG_OVL_L3_Y2R_PARA_RGB_A_1);
		reg_restore(DISP_REG_OVL_DEBUG_MON_SEL);
		reg_restore(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING2);
		reg_restore(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING2);
		reg_restore(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING2);
		reg_restore(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING2);
		reg_restore(DISP_REG_OVL_FLOW_CTRL_DBG);
		reg_restore(DISP_REG_OVL_ADDCON_DBG);
		reg_restore(DISP_REG_OVL_OUTMUX_DBG);
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	} else {
		MTEEC_PARAM param[4];
		unsigned int paramTypes = 0;
		TZ_RESULT ret =
		    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_OVL_RESTORE_REG, paramTypes,
					param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("KREE_TeeServiceCall(TZCMD_DDP_OVL_RESTORE_REG) fail, ret=%d\n",
				 ret);
		}
	}
#endif				/* CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT */

	/* RDMA0 */
	reg_restore(DISP_REG_RDMA_INT_ENABLE);
	reg_restore(DISP_REG_RDMA_INT_STATUS);
	reg_restore(DISP_REG_RDMA_GLOBAL_CON);
	reg_restore(DISP_REG_RDMA_SIZE_CON_0);
	reg_restore(DISP_REG_RDMA_SIZE_CON_1);
	reg_restore(DISP_REG_RDMA_TARGET_LINE);
	reg_restore(DISP_REG_RDMA_MEM_CON);
	reg_restore(DISP_REG_RDMA_MEM_START_ADDR);
	reg_restore(DISP_REG_RDMA_MEM_SRC_PITCH);
	reg_restore(DISP_REG_RDMA_MEM_GMC_SETTING_0);
	reg_restore(DISP_REG_RDMA_MEM_SLOW_CON);
	reg_restore(DISP_REG_RDMA_MEM_GMC_SETTING_1);
	reg_restore(DISP_REG_RDMA_FIFO_CON);
	reg_restore(DISP_REG_RDMA_CF_00);
	reg_restore(DISP_REG_RDMA_CF_01);
	reg_restore(DISP_REG_RDMA_CF_02);
	reg_restore(DISP_REG_RDMA_CF_10);
	reg_restore(DISP_REG_RDMA_CF_11);
	reg_restore(DISP_REG_RDMA_CF_12);
	reg_restore(DISP_REG_RDMA_CF_20);
	reg_restore(DISP_REG_RDMA_CF_21);
	reg_restore(DISP_REG_RDMA_CF_22);
	reg_restore(DISP_REG_RDMA_CF_PRE_ADD0);
	reg_restore(DISP_REG_RDMA_CF_PRE_ADD1);
	reg_restore(DISP_REG_RDMA_CF_PRE_ADD2);
	reg_restore(DISP_REG_RDMA_CF_POST_ADD0);
	reg_restore(DISP_REG_RDMA_CF_POST_ADD1);
	reg_restore(DISP_REG_RDMA_CF_POST_ADD2);
	reg_restore(DISP_REG_RDMA_DUMMY);
	reg_restore(DISP_REG_RDMA_DEBUG_OUT_SEL);

	/* RDMA1 */
	/* reg_restore(DISP_REG_RDMA_INT_ENABLE + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_INT_STATUS + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_GLOBAL_CON + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_SIZE_CON_0 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_SIZE_CON_1 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_TARGET_LINE + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_MEM_CON + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_MEM_START_ADDR + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_MEM_SRC_PITCH + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_MEM_GMC_SETTING_0 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_MEM_SLOW_CON + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_MEM_GMC_SETTING_1 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_FIFO_CON + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_00 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_01 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_02 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_10 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_11 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_12 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_20 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_21 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_22 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_PRE_ADD0 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_PRE_ADD1 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_PRE_ADD2 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_POST_ADD0 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_POST_ADD1 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_CF_POST_ADD2 + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_DUMMY + 0x1000); */
	/* reg_restore(DISP_REG_RDMA_DEBUG_OUT_SEL + 0x1000); */
#if 0
	/* ROTDMA */
	reg_restore(DISP_REG_ROT_EN);
	reg_restore(DISP_REG_ROT_RESET);
	reg_restore(DISP_REG_ROT_INTERRUPT_ENABLE);
	reg_restore(DISP_REG_ROT_INTERRUPT_STATUS);
	reg_restore(DISP_REG_ROT_CON);
	reg_restore(DISP_REG_ROT_GMCIF_CON);
	reg_restore(DISP_REG_ROT_SRC_CON);
	reg_restore(DISP_REG_ROT_SRC_BASE_0);
	reg_restore(DISP_REG_ROT_SRC_BASE_1);
	reg_restore(DISP_REG_ROT_SRC_BASE_2);
	reg_restore(DISP_REG_ROT_MF_BKGD_SIZE_IN_BYTE);
	reg_restore(DISP_REG_ROT_MF_SRC_SIZE);
	reg_restore(DISP_REG_ROT_MF_CLIP_SIZE);
	reg_restore(DISP_REG_ROT_MF_OFFSET_1);
	reg_restore(DISP_REG_ROT_MF_PAR);
	reg_restore(DISP_REG_ROT_SF_BKGD_SIZE_IN_BYTE);
	reg_restore(DISP_REG_ROT_SF_PAR);
	reg_restore(DISP_REG_ROT_MB_DEPTH);
	reg_restore(DISP_REG_ROT_MB_BASE);
	reg_restore(DISP_REG_ROT_MB_CON);
	reg_restore(DISP_REG_ROT_SB_DEPTH);
	reg_restore(DISP_REG_ROT_SB_BASE);
	reg_restore(DISP_REG_ROT_SB_CON);
	reg_restore(DISP_REG_ROT_VC1_RANGE);
	reg_restore(DISP_REG_ROT_TRANSFORM_0);
	reg_restore(DISP_REG_ROT_TRANSFORM_1);
	reg_restore(DISP_REG_ROT_TRANSFORM_2);
	reg_restore(DISP_REG_ROT_TRANSFORM_3);
	reg_restore(DISP_REG_ROT_TRANSFORM_4);
	reg_restore(DISP_REG_ROT_TRANSFORM_5);
	reg_restore(DISP_REG_ROT_TRANSFORM_6);
	reg_restore(DISP_REG_ROT_TRANSFORM_7);
	reg_restore(DISP_REG_ROT_RESV_DUMMY_0);

	/* SCL */
	reg_restore(DISP_REG_SCL_CTRL);
	reg_restore(DISP_REG_SCL_INTEN);
	reg_restore(DISP_REG_SCL_INTSTA);
	reg_restore(DISP_REG_SCL_STATUS);
	reg_restore(DISP_REG_SCL_CFG);
	reg_restore(DISP_REG_SCL_INP_CHKSUM);
	reg_restore(DISP_REG_SCL_OUTP_CHKSUM);
	reg_restore(DISP_REG_SCL_HRZ_CFG);
	reg_restore(DISP_REG_SCL_HRZ_SIZE);
	reg_restore(DISP_REG_SCL_HRZ_FACTOR);
	reg_restore(DISP_REG_SCL_HRZ_OFFSET);
	reg_restore(DISP_REG_SCL_VRZ_CFG);
	reg_restore(DISP_REG_SCL_VRZ_SIZE);
	reg_restore(DISP_REG_SCL_VRZ_FACTOR);
	reg_restore(DISP_REG_SCL_VRZ_OFFSET);
	reg_restore(DISP_REG_SCL_EXT_COEF);
	reg_restore(DISP_REG_SCL_PEAK_CFG);

	/* WDMA 0 */
	reg_restore(DISP_REG_WDMA_INTEN);
	reg_restore(DISP_REG_WDMA_INTSTA);
	reg_restore(DISP_REG_WDMA_EN);
	reg_restore(DISP_REG_WDMA_RST);
	reg_restore(DISP_REG_WDMA_SMI_CON);
	reg_restore(DISP_REG_WDMA_CFG);
	reg_restore(DISP_REG_WDMA_SRC_SIZE);
	reg_restore(DISP_REG_WDMA_CLIP_SIZE);
	reg_restore(DISP_REG_WDMA_CLIP_COORD);
	reg_restore(DISP_REG_WDMA_DST_ADDR);
	reg_restore(DISP_REG_WDMA_DST_W_IN_BYTE);
	reg_restore(DISP_REG_WDMA_ALPHA);
	reg_restore(DISP_REG_WDMA_BUF_ADDR);
	reg_restore(DISP_REG_WDMA_STA);
	reg_restore(DISP_REG_WDMA_BUF_CON1);
	reg_restore(DISP_REG_WDMA_BUF_CON2);
	reg_restore(DISP_REG_WDMA_C00);
	reg_restore(DISP_REG_WDMA_C02);
	reg_restore(DISP_REG_WDMA_C10);
	reg_restore(DISP_REG_WDMA_C12);
	reg_restore(DISP_REG_WDMA_C20);
	reg_restore(DISP_REG_WDMA_C22);
	reg_restore(DISP_REG_WDMA_PRE_ADD0);
	reg_restore(DISP_REG_WDMA_PRE_ADD2);
	reg_restore(DISP_REG_WDMA_POST_ADD0);
	reg_restore(DISP_REG_WDMA_POST_ADD2);
	reg_restore(DISP_REG_WDMA_DST_U_ADDR);
	reg_restore(DISP_REG_WDMA_DST_V_ADDR);
	reg_restore(DISP_REG_WDMA_DST_UV_PITCH);
	reg_restore(DISP_REG_WDMA_DITHER_CON);
	reg_restore(DISP_REG_WDMA_FLOW_CTRL_DBG);
	reg_restore(DISP_REG_WDMA_EXEC_DBG);
	reg_restore(DISP_REG_WDMA_CLIP_DBG);
#endif
	/* WDMA1 */
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* when restoring WDMA 1 registers, consider secure case */
	if (0 == gMemOutSecure) {
#endif				/* CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT */
		reg_restore(DISP_REG_WDMA_INTEN + 0x1000);
		reg_restore(DISP_REG_WDMA_INTSTA + 0x1000);
		reg_restore(DISP_REG_WDMA_EN + 0x1000);
		reg_restore(DISP_REG_WDMA_RST + 0x1000);
		reg_restore(DISP_REG_WDMA_SMI_CON + 0x1000);
		reg_restore(DISP_REG_WDMA_CFG + 0x1000);
		reg_restore(DISP_REG_WDMA_SRC_SIZE + 0x1000);
		reg_restore(DISP_REG_WDMA_CLIP_SIZE + 0x1000);
		reg_restore(DISP_REG_WDMA_CLIP_COORD + 0x1000);
		reg_restore(DISP_REG_WDMA_DST_ADDR + 0x1000);
		reg_restore(DISP_REG_WDMA_DST_W_IN_BYTE + 0x1000);
		reg_restore(DISP_REG_WDMA_ALPHA + 0x1000);
		reg_restore(DISP_REG_WDMA_BUF_ADDR + 0x1000);
		reg_restore(DISP_REG_WDMA_STA + 0x1000);
		reg_restore(DISP_REG_WDMA_BUF_CON1 + 0x1000);
		reg_restore(DISP_REG_WDMA_BUF_CON2 + 0x1000);
		reg_restore(DISP_REG_WDMA_C00 + 0x1000);
		reg_restore(DISP_REG_WDMA_C02 + 0x1000);
		reg_restore(DISP_REG_WDMA_C10 + 0x1000);
		reg_restore(DISP_REG_WDMA_C12 + 0x1000);
		reg_restore(DISP_REG_WDMA_C20 + 0x1000);
		reg_restore(DISP_REG_WDMA_C22 + 0x1000);
		reg_restore(DISP_REG_WDMA_PRE_ADD0 + 0x1000);
		reg_restore(DISP_REG_WDMA_PRE_ADD2 + 0x1000);
		reg_restore(DISP_REG_WDMA_POST_ADD0 + 0x1000);
		reg_restore(DISP_REG_WDMA_POST_ADD2 + 0x1000);
		reg_restore(DISP_REG_WDMA_DST_U_ADDR + 0x1000);
		reg_restore(DISP_REG_WDMA_DST_V_ADDR + 0x1000);
		reg_restore(DISP_REG_WDMA_DST_UV_PITCH + 0x1000);
		reg_restore(DISP_REG_WDMA_DITHER_CON + 0x1000);
		reg_restore(DISP_REG_WDMA_FLOW_CTRL_DBG + 0x1000);
		reg_restore(DISP_REG_WDMA_EXEC_DBG + 0x1000);
		reg_restore(DISP_REG_WDMA_CLIP_DBG + 0x1000);
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	} else {
		MTEEC_PARAM param[4];
		unsigned int paramTypes = 0;
		TZ_RESULT ret =
		    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_WDMA_RESTORE_REG,
					paramTypes, param);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_ERR("KREE_TeeServiceCall(TZCMD_DDP_WDMA_RESTORE_REG) fail, ret=%d\n",
				 ret);
		}
	}
#endif				/* CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT */

#if 0
	/* BLS */
	reg_restore(DISP_REG_BLS_EN);
	reg_restore(DISP_REG_BLS_RST);
	reg_restore(DISP_REG_BLS_INTEN);
	reg_restore(DISP_REG_BLS_INTSTA);
	reg_restore(DISP_REG_BLS_SRC_SIZE);
	reg_restore(DISP_REG_BLS_PWM_DUTY);
	reg_restore(DISP_REG_BLS_PWM_DUTY_GAIN);
	reg_restore(DISP_REG_BLS_PWM_CON);
	reg_restore(DISP_REG_PWM_H_DURATION);
	reg_restore(DISP_REG_PWM_L_DURATION);
	reg_restore(DISP_REG_PWM_G_DURATION);
	reg_restore(DISP_REG_PWM_SEND_DATA0);
	reg_restore(DISP_REG_PWM_SEND_DATA1);
	reg_restore(DISP_REG_PWM_WAVE_NUM);
	reg_restore(DISP_REG_PWM_DATA_WIDTH);
	reg_restore(DISP_REG_PWM_THRESH);
	reg_restore(DISP_REG_PWM_SEND_WAVENUM);
#endif
	/* DISP_MSG("disp_reg_restore() release mutex\n"); */
	/* disp_path_release_mutex(); */
	DISP_DBG("disp_reg_restore() done\n");

	DpEngine_COLORonInit();
	DpEngine_COLORonConfig(fb_width, fb_height);

	/* backlight should be turn on last */
#if defined(CONFIG_MTK_AAL_SUPPORT)
	disp_bls_init(fb_width, fb_height);
#endif

	MMProfileLogEx(DDP_MMP_Events.BackupReg, MMProfileFlagEnd, 1, 0);
	return 0;
}

unsigned int disp_intr_status[DISP_MODULE_MAX] = { 0 };

int disp_intr_restore(void)
{
	/* restore intr enable reg */
	/* DISP_REG_SET(DISP_REG_ROT_INTERRUPT_ENABLE,   disp_intr_status[DISP_MODULE_ROT]  ); */
	/* DISP_REG_SET(DISP_REG_SCL_INTEN,              disp_intr_status[DISP_MODULE_SCL]  ); */
	DISP_REG_SET(DISP_REG_OVL_INTEN, disp_intr_status[DISP_MODULE_OVL]);
	/* DISP_REG_SET(DISP_REG_WDMA_INTEN,             disp_intr_status[DISP_MODULE_WDMA0]); */
	DISP_REG_SET(DISP_REG_WDMA_INTEN + 0x1000, disp_intr_status[DISP_MODULE_WDMA1]);
	DISP_REG_SET(DISP_REG_RDMA_INT_ENABLE, disp_intr_status[DISP_MODULE_RDMA0]);
	/* DISP_REG_SET(DISP_REG_RDMA_INT_ENABLE + 0x1000, disp_intr_status[DISP_MODULE_RDMA1]); */
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_INTEN, disp_intr_status[DISP_MODULE_MUTEX]);

	return 0;
}

/* TODO: color, tdshp, gamma, bls, cmdq intr management should add later */
int disp_intr_disable_and_clear(void)
{
	/* backup intr enable reg */
	/* disp_intr_status[DISP_MODULE_ROT] = DISP_REG_GET(DISP_REG_ROT_INTERRUPT_ENABLE); */
	/* disp_intr_status[DISP_MODULE_SCL] = DISP_REG_GET(DISP_REG_SCL_INTEN); */
	disp_intr_status[DISP_MODULE_OVL] = DISP_REG_GET(DISP_REG_OVL_INTEN);
	/* disp_intr_status[DISP_MODULE_WDMA0] = DISP_REG_GET(DISP_REG_WDMA_INTEN); */
	disp_intr_status[DISP_MODULE_WDMA1] = DISP_REG_GET(DISP_REG_WDMA_INTEN + 0x1000);
	disp_intr_status[DISP_MODULE_RDMA0] = DISP_REG_GET(DISP_REG_RDMA_INT_ENABLE);
	/* disp_intr_status[DISP_MODULE_RDMA1] = DISP_REG_GET(DISP_REG_RDMA_INT_ENABLE + 0x1000); */
	disp_intr_status[DISP_MODULE_MUTEX] = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTEN);

	/* disable intr */
	/* DISP_REG_SET(DISP_REG_ROT_INTERRUPT_ENABLE, 0); */
	/* DISP_REG_SET(DISP_REG_SCL_INTEN, 0); */
	DISP_REG_SET(DISP_REG_OVL_INTEN, 0);
	/* DISP_REG_SET(DISP_REG_WDMA_INTEN, 0); */
	DISP_REG_SET(DISP_REG_WDMA_INTEN + 0x1000, 0);
	DISP_REG_SET(DISP_REG_RDMA_INT_ENABLE, 0);
	/* DISP_REG_SET(DISP_REG_RDMA_INT_ENABLE + 0x1000, 0); */
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_INTEN, 0);

	/* clear intr status */
	/* DISP_REG_SET(DISP_REG_ROT_INTERRUPT_STATUS, 0); */
	/* DISP_REG_SET(DISP_REG_SCL_INTSTA, 0); */
	DISP_REG_SET(DISP_REG_OVL_INTSTA, 0);
	/* DISP_REG_SET(DISP_REG_WDMA_INTSTA, 0); */
	DISP_REG_SET(DISP_REG_WDMA_INTSTA + 0x1000, 0);
	DISP_REG_SET(DISP_REG_RDMA_INT_STATUS, 0);
	/* DISP_REG_SET(DISP_REG_RDMA_INT_STATUS + 0x1000, 0); */
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_INTSTA, 0);

	return 0;
}

static int gDispPathClockOnFirst = 1;
static int restore_reg_check = 0;

int disp_path_clock_on(char *name)
{
	bool bRDMAOff = 0;
	if (name != NULL) {
		/* error */
		DISP_MSG("disp_path_power_on, caller:%s\n", name);
	}

	if (gDispPathClockOnFirst) {
		gDispPathClockOnFirst = 0;
		if (!clock_is_on(MT_CG_DISP_SMI_LARB2))
			enable_clock(MT_CG_DISP_SMI_LARB2, "DDP");
	} else {
		enable_clock(MT_CG_DISP_SMI_LARB2, "DDP");
	}
/* enable_clock(MT_CG_DISP_CMDQ_DISP , "DDP"); */
/* enable_clock(MT_CG_DISP_CMDQ_SMI    , "DDP"); */

/* enable_clock(MT_CG_DISP_ROT_DISP  , "DDP"); */
/* enable_clock(MT_CG_DISP_ROT_SMI     , "DDP"); */
/* enable_clock(MT_CG_DISP_SCL_DISP         , "DDP"); */
/* enable_clock(MT_CG_DISP_WDMA0_DISP, "DDP"); */
/* enable_clock(MT_CG_DISP_WDMA0_SMI   , "DDP"); */
/*
	if (!clock_is_on(MT_CG_DISP_OVL_DISP))
		enable_clock(MT_CG_DISP_OVL_DISP, "DDP");
	if (!clock_is_on(MT_CG_DISP_OVL_SMI))
		enable_clock(MT_CG_DISP_OVL_SMI, "DDP");
*/
	if (!clock_is_on(MT_CG_DISP_COLOR_DISP))
		enable_clock(MT_CG_DISP_COLOR_DISP, "DDP");
/* enable_clock(MT_CG_DISP_TDSHP_DISP       , "DDP"); */
	if (!clock_is_on(MT_CG_DISP_BLS_DISP))
		enable_clock(MT_CG_DISP_BLS_DISP, "DDP");
	/* enable_clock(MT_CG_DISP_WDMA1_DISP, "DDP"); */
	/* enable_clock(MT_CG_DISP_WDMA1_SMI   , "DDP"); */
	if (!clock_is_on(MT_CG_DISP_RDMA0_DISP)) {
		enable_clock(MT_CG_DISP_RDMA0_DISP, "DDP");
		bRDMAOff = 1;
	}
	if (!clock_is_on(MT_CG_DISP_RDMA0_SMI)) {
		enable_clock(MT_CG_DISP_RDMA0_SMI, "DDP");
		bRDMAOff = 1;
	}
	if (!clock_is_on(MT_CG_DISP_RDMA0_OUTPUT)) {
		enable_clock(MT_CG_DISP_RDMA0_OUTPUT, "DDP");
		bRDMAOff = 1;
	}

	/* enable_clock(MT_CG_DISP_RDMA1_DISP, "DDP"); */
	/* enable_clock(MT_CG_DISP_RDMA1_SMI   , "DDP"); */
	/* enable_clock(MT_CG_DISP_RDMA1_OUTPUT, "DDP"); */
	/* enable_clock(MT_CG_DISP_GAMMA_DISP, "DDP"); */
	/* enable_clock(MT_CG_DISP_GAMMA_PIXEL , "DDP"); */

	/* enable_clock(MT_CG_DISP_G2D_DISP  , "DDP"); */
	/* enable_clock(MT_CG_DISP_G2D_SMI     , "DDP"); */

	/* DE request RDMA reset before engine enable */
	/* prevent reset RDMA interruptedly during RDMA working */
	if (bRDMAOff == 1)
		RDMAReset(0);

	/* restore ddp related registers */
	if (strncmp(name, "ipoh_mtkfb", 10)) {
		if (*pRegBackup != DDP_UNBACKED_REG_MEM) {
			disp_reg_restore();

			/* restore intr enable registers */
			disp_intr_restore();
		} else {
			if (restore_reg_check) {
				DISP_ERR
			    ("disp_path_clock_on(), dose not call disp_reg_restore, cause mem not inited!\n");
			} else {
				DISP_MSG
			    ("disp_path_clock_on(), dose not call disp_reg_restore, cause mem not inited!\n");
			}

			restore_reg_check = 1;

		}
	}


	return 0;
}


int disp_path_clock_off(char *name)
{
	if (name != NULL) {
		/* error */
		DISP_MSG("disp_path_power_off, caller:%s\n", name);
	}
	/* disable intr and clear intr status */
	disp_intr_disable_and_clear();

	/* DE request RDMA engine enable after DSI, so disable before backup register */
	RDMADisable(0);

	/* backup ddp related registers */
	disp_reg_backup();

/* disable_clock(MT_CG_DISP_CMDQ_DISP , "DDP"); */
/* disable_clock(MT_CG_DISP_CMDQ_SMI    , "DDP"); */

/* disable_clock(MT_CG_DISP_ROT_DISP  , "DDP"); */
/* disable_clock(MT_CG_DISP_ROT_SMI     , "DDP"); */
/* disable_clock(MT_CG_DISP_SCL_DISP         , "DDP"); */
/* disable_clock(MT_CG_DISP_WDMA0_DISP, "DDP"); */
/* disable_clock(MT_CG_DISP_WDMA0_SMI   , "DDP"); */

	/* Better to reset DMA engine before disable their clock */
	RDMAStop(0);
	RDMAReset(0);

/*
	WDMAStop(1);
	WDMAReset(1);

	OVLStop();
	OVLReset();

	disable_clock(MT_CG_DISP_OVL_DISP, "DDP");
	disable_clock(MT_CG_DISP_OVL_SMI, "DDP");
*/
	disable_clock(MT_CG_DISP_COLOR_DISP, "DDP");
/* disable_clock(MT_CG_DISP_TDSHP_DISP       , "DDP"); */
	disable_clock(MT_CG_DISP_BLS_DISP, "DDP");
	/* disable_clock(MT_CG_DISP_WDMA1_DISP, "DDP"); */
	/* disable_clock(MT_CG_DISP_WDMA1_SMI   , "DDP"); */
	disable_clock(MT_CG_DISP_RDMA0_DISP, "DDP");
	disable_clock(MT_CG_DISP_RDMA0_SMI, "DDP");
	disable_clock(MT_CG_DISP_RDMA0_OUTPUT, "DDP");

	/* disable_clock(MT_CG_DISP_RDMA1_DISP, "DDP"); */
	/* disable_clock(MT_CG_DISP_RDMA1_SMI   , "DDP"); */
	/* disable_clock(MT_CG_DISP_RDMA1_OUTPUT, "DDP"); */
	/* disable_clock(MT_CG_DISP_GAMMA_DISP, "DDP"); */
	/* disable_clock(MT_CG_DISP_GAMMA_PIXEL , "DDP"); */


	/* DPI can not suspend issue fixed, so remove this workaround */
	if (0) {
		/* if(g_dst_module==DISP_MODULE_DPI0 || g_dst_module==DISP_MODULE_DPI1) */
		DISP_DBG("warning: do not power off MT_CG_DISP_SMI_LARB2 for DPI resume issue\n");
	} else {
		disable_clock(MT_CG_DISP_SMI_LARB2, "DDP");
	}
	/* disable_clock(MT_CG_DISP_G2D_DISP  , "DDP"); */
	/* disable_clock(MT_CG_DISP_G2D_SMI     , "DDP"); */

	return 0;
}

int disp_path_rdma_start(unsigned idx)
{
	return RDMAStart(idx);
}
#endif


int disp_bls_set_max_backlight(unsigned int level)
{
	return disp_bls_set_max_backlight_(level);
}

#ifdef MTK_OVERLAY_ENGINE_SUPPORT
int disp_path_get_ovl_en(void)
{
	return DISP_REG_GET(DISP_REG_OVL_EN);
}
#endif
