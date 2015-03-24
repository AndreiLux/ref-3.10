#include <linux/delay.h>
#include "disp_ovl_engine_hw.h"
#include "ddp_reg.h"
#include "ddp_debug.h"

#ifdef DISP_OVL_ENGINE_HW_SUPPORT
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <mach/m4u.h>
#include "disp_ovl_engine_core.h"
#include "ddp_hal.h"


#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include <tz_cross/tz_ddp.h>
#include <mach/m4u_port.h>
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#endif


/* Parameter */
static DISP_OVL_ENGINE_INSTANCE disp_ovl_engine_params;
#define OVL_ENGINE_RDMA_TARGET_LINE  0x20
/* rdma0 update thread */
int gRdma0IrqChange = 0;
static unsigned int gRdma0IrqAddress;
static int gRdma0IrqSecure;
static struct task_struct *disp_rdma0_update_task;
static int disp_ovl_engine_rdma0_update_kthread(void *data);
static wait_queue_head_t disp_rdma0_update_wq;
static unsigned int gWakeupRdma0UpdateThread;
DEFINE_SEMAPHORE(disp_rdma0_update_semaphore);

/* Irq callback */
void (*disp_ovl_engine_hw_irq_callback) (unsigned int param) = NULL;
void disp_ovl_engine_hw_ovl_wdma_irq_handler(unsigned int param);
void disp_ovl_engine_hw_ovl_rdma_irq_handler(unsigned int param);

#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
/* these 2 APIs are used for accessing ddp_session / ddp_mem_session with TEE */
extern KREE_SESSION_HANDLE ddp_session_handle(void);
extern KREE_SESSION_HANDLE ddp_mem_session_handle(void);

void *disp_ovl_engine_hw_allocate_secure_memory(int size);
void disp_ovl_engine_hw_free_secure_memory(void *mem_handle);
#endif

void disp_ovl_engine_hw_init(void)
{
	memset(&disp_ovl_engine_params, 0, sizeof(DISP_OVL_ENGINE_INSTANCE));

	disp_path_register_ovl_wdma_callback(disp_ovl_engine_hw_ovl_wdma_irq_handler, 0);
	/* disp_path_register_ovl_rdma_callback(disp_ovl_engine_hw_ovl_rdma_irq_handler,0); */

	/* Init rdma0 update thread */
	gWakeupRdma0UpdateThread = 0;
	init_waitqueue_head(&disp_rdma0_update_wq);
	disp_rdma0_update_task = kthread_create(
		disp_ovl_engine_rdma0_update_kthread, NULL, "rdma0_update_kthread");
	wake_up_process(disp_rdma0_update_task);
	DISP_OVL_ENGINE_INFO("kthread_create rdma0_update_kthread\n");
	WDMASetSecure(0);
}


void disp_ovl_engine_hw_set_params(DISP_OVL_ENGINE_INSTANCE *params)
{
	memcpy(&disp_ovl_engine_params, params, sizeof(DISP_OVL_ENGINE_INSTANCE));
	atomic_set(&params->OverlaySettingDirtyFlag, 0);
	atomic_set(&params->OverlaySettingApplied, 1);
}


int g_ovl_wdma_irq_ignore = 0;
void disp_ovl_engine_hw_ovl_wdma_irq_handler(unsigned int param)
{
	DISP_OVL_ENGINE_DBG("disp_ovl_engine_hw_ovl_wdma_irq_handler\n");

	disp_module_clock_off(DISP_MODULE_WDMA1, "OVL");
	disp_module_clock_off(DISP_MODULE_OVL, "OVL");
	disp_module_clock_off(DISP_MODULE_SMI, "OVL");

	if (COUPLE_MODE == disp_ovl_engine_params.mode)
		disp_ovl_engine.OvlWrIdx =
			(disp_ovl_engine.OvlWrIdx + 1) % OVL_ENGINE_OVL_BUFFER_NUMBER;

	if (g_ovl_wdma_irq_ignore) {
		g_ovl_wdma_irq_ignore = 0;
		return;
	}

	if (disp_ovl_engine_hw_irq_callback != NULL)
		disp_ovl_engine_hw_irq_callback(param);
}

void disp_ovl_engine_hw_ovl_rdma_irq_handler(unsigned int param)
{
	DISP_OVL_ENGINE_DBG("disp_ovl_engine_hw_ovl_rdma_irq_handler\n");

	if (disp_ovl_engine_hw_irq_callback != NULL)
		disp_ovl_engine_hw_irq_callback(param);
}


static void _rdma0_irq_handler(unsigned int param)
{
/* unsigned int rdma_buffer_addr; */
	int lcm_width, lcm_height, lcm_bpp;	/* , tmp; */
/* struct disp_path_config_struct rConfig = {0}; */

	lcm_width = DISP_GetScreenWidth();
	lcm_height = DISP_GetScreenHeight();
	lcm_bpp = 3;		/* (DISP_GetScreenBpp() + 7) >> 3; */

	if (param & OVL_ENGINE_RDMA_TARGET_LINE) {	/* rdma0 target line */
#if 0
		tmp = disp_ovl_engine.RdmaRdIdx + 1;
		tmp %= OVL_ENGINE_OVL_BUFFER_NUMBER;
		if (tmp == disp_ovl_engine.OvlWrIdx) {
			DISP_OVL_ENGINE_DBG("OVL BuffCtl WDMA1 hang (%d), Show same buffer\n",
					    disp_ovl_engine.OvlWrIdx);
		} else {
			/* disp_path_get_mutex(); */
			rdma_buffer_addr =
			    disp_ovl_engine.Ovlmva +
			    lcm_width * lcm_height * lcm_bpp * disp_ovl_engine.RdmaRdIdx;
			DISP_REG_SET(DISP_REG_RDMA_MEM_START_ADDR, rdma_buffer_addr);
			DISP_OVL_ENGINE_DBG("OVL BuffCtl RdmaRdIdx: 0x%x Addr: 0x%x\n",
					    disp_ovl_engine.RdmaRdIdx, rdma_buffer_addr);
			disp_ovl_engine.RdmaRdIdx++;
			disp_ovl_engine.RdmaRdIdx %= OVL_ENGINE_OVL_BUFFER_NUMBER;
			/* disp_path_release_mutex(); */
		}
#else
		disp_ovl_engine_update_rdma0();
#endif
	}

	if (param & 0x2) {	/* rdma0 frame start */
		if (gRdma0IrqChange == 2)
			gRdma0IrqChange = 0;
		disp_ovl_engine_wake_up_rdma0_update_thread();
	}
}

extern void disp_register_intr(unsigned int irq, unsigned int secure);
unsigned int gOvlWdmaMutexID = 4;
static int OvlSecure;		/* Todo, this suggest that only one HW overlay. */
static int RdmaSecure;
void disp_ovl_engine_trigger_hw_overlay_decouple(void)
{
	int layer_id;
	int OvlSecureNew = 0;

	DISP_OVL_ENGINE_DBG("disp_ovl_engine_trigger_hw_overlay\n");
	disp_module_clock_on(DISP_MODULE_SMI, "OVL");
	disp_module_clock_on(DISP_MODULE_OVL, "OVL");
	disp_module_clock_on(DISP_MODULE_WDMA1, "OVL");

	OVLReset();
	WDMAReset(1);

	disp_path_config_OVL_WDMA_path(gOvlWdmaMutexID, TRUE, FALSE);

	disp_path_get_mutex_(gOvlWdmaMutexID);

	for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
		if ((disp_ovl_engine_params.cached_layer_config[layer_id].layer_en) &&
		    (disp_ovl_engine_params.cached_layer_config[layer_id].security ==
		     LAYER_SECURE_BUFFER))
			OvlSecureNew = 1;
	}

	if (OvlSecure != OvlSecureNew) {
		if (OvlSecureNew) {
			OvlSecure = OvlSecureNew;
			WDMASetSecure(OvlSecure);

			disp_register_intr(MT8135_DISP_OVL_IRQ_ID, OvlSecure);
			disp_register_intr(MT8135_DISP_WDMA1_IRQ_ID, OvlSecure);
		}
	}

	disp_path_config_OVL_WDMA(&(disp_ovl_engine_params.MemOutConfig), OvlSecure);

	for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
		disp_ovl_engine_params.cached_layer_config[layer_id].layer = layer_id;

		disp_path_config_layer_ovl_engine(&
						  (disp_ovl_engine_params.cached_layer_config
						   [layer_id]), OvlSecure);
	}

	/* disp_dump_reg(DISP_MODULE_WDMA1); */
	/* disp_dump_reg(DISP_MODULE_OVL); */

	disp_path_release_mutex_(gOvlWdmaMutexID);

	if (OvlSecure != OvlSecureNew) {
		if (!OvlSecureNew) {
			OvlSecure = OvlSecureNew;
			WDMASetSecure(OvlSecure);

			disp_register_intr(MT8135_DISP_OVL_IRQ_ID, OvlSecure);
			disp_register_intr(MT8135_DISP_WDMA1_IRQ_ID, OvlSecure);
		}
	}
}

void disp_ovl_engine_config_overlay(void)
{
	unsigned int i = 0;
	int dirty;
	int layer_id;
	int OvlSecureNew = 0;

	disp_path_config_layer_ovl_engine_control(true);

	for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
		if ((disp_ovl_engine_params.cached_layer_config[layer_id].layer_en) &&
		    (disp_ovl_engine_params.cached_layer_config[layer_id].security ==
		     LAYER_SECURE_BUFFER))
			OvlSecureNew = 1;
	}

	if (OvlSecure != OvlSecureNew) {
		OvlSecure = OvlSecureNew;
		WDMASetSecure(OvlSecure);

		disp_register_intr(MT8135_DISP_OVL_IRQ_ID, OvlSecure);
		disp_register_intr(MT8135_DISP_WDMA1_IRQ_ID, OvlSecure);
	}


	disp_path_get_mutex();
	for (i = 0; i < DDP_OVL_LAYER_MUN; i++) {
		if (disp_ovl_engine_params.cached_layer_config[i].isDirty) {
			dirty |= 1 << i;
			disp_path_config_layer_ovl_engine
			    (&disp_ovl_engine_params.cached_layer_config[i], OvlSecure);
			disp_ovl_engine_params.cached_layer_config[i].isDirty = false;
		}
	}
	disp_path_release_mutex();
}

void disp_ovl_engine_direct_link_overlay(void)
{
/* unsigned int i = 0; */
/* unsigned int dirty = 0; */
	int lcm_width = DISP_GetScreenWidth();
	int buffer_bpp = 3;
	static int first_boot = 1;

	/* Remove OVL and WDMA1 from ovl_wdma_mutex, because these two will get into mutex 0 */
	DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(gOvlWdmaMutexID), 0);

	/* setup the direct link of overlay - rdma */
	DISP_OVL_ENGINE_INFO("direct link overlay, addr=0x%x\n",
			     disp_ovl_engine.OvlBufAddr[disp_ovl_engine.RdmaRdIdx]);

	DISP_OVL_ENGINE_DBG
	    ("ovl addr: 0x%x srcModule: %d dstModule: %d inFormat: %d outFormat: %d\n",
	     disp_ovl_engine_params.path_info.ovl_config.addr,
	     disp_ovl_engine_params.path_info.srcModule, disp_ovl_engine_params.path_info.dstModule,
	     disp_ovl_engine_params.path_info.inFormat, disp_ovl_engine_params.path_info.outFormat);


	/* config m4u */
	/* if(lcm_params->dsi.mode != CMD_MODE) */
	{
		/* disp_path_get_mutex(); */
	}
#if 1
	if (1 == first_boot) {
		M4U_PORT_STRUCT portStruct;

		DISP_OVL_ENGINE_DBG("config m4u start\n\n");

		portStruct.ePortID = M4U_PORT_OVL_CH0;	/* hardware port ID, defined in M4U_PORT_ID_ENUM */
		portStruct.Virtuality = 1;
		portStruct.Security = 0;
		portStruct.domain = 3;	/* domain : 0 1 2 3 */
		portStruct.Distance = 1;
		portStruct.Direction = 0;
		m4u_config_port(&portStruct);
		first_boot = 0;
	}
#endif

#if 0
	if (disp_ovl_engine_params.fgNeedConfigM4U) {
		M4U_PORT_STRUCT portStruct;

		DISP_OVL_ENGINE_DBG("config m4u start\n\n");

		portStruct.ePortID = M4U_PORT_OVL_CH2;	/* hardware port ID, defined in M4U_PORT_ID_ENUM */
		portStruct.Virtuality = 1;
		portStruct.Security = 0;
		portStruct.domain = 3;	/* domain : 0 1 2 3 */
		portStruct.Distance = 1;
		portStruct.Direction = 0;
		m4u_config_port(&portStruct);


		portStruct.ePortID = M4U_PORT_OVL_CH3;	/* hardware port ID, defined in M4U_PORT_ID_ENUM */
		portStruct.Virtuality = 1;
		portStruct.Security = 0;
		portStruct.domain = 3;	/* domain : 0 1 2 3 */
		portStruct.Distance = 1;
		portStruct.Direction = 0;
		m4u_config_port(&portStruct);
		disp_ovl_engine_params.fgNeedConfigM4U = FALSE;
		disp_ovl_engine.Instance[disp_ovl_engine_params.index].fgNeedConfigM4U = FALSE;

		portStruct.ePortID = M4U_PORT_RDMA0;	/* hardware port ID, defined in M4U_PORT_ID_ENUM */
		portStruct.Virtuality = 0;
		portStruct.Security = 0;
		portStruct.domain = 1;	/* domain : 0 1 2 3 */
		portStruct.Distance = 1;
		portStruct.Direction = 0;
		m4u_config_port(&portStruct);

		DISP_OVL_ENGINE_DBG("config m4u done\n");
	}
#endif

	disp_path_register_ovl_rdma_callback(disp_ovl_engine_hw_ovl_rdma_irq_handler, 0);
	if (disp_is_cb_registered(DISP_MODULE_RDMA0, _rdma0_irq_handler))
		disp_unregister_irq(DISP_MODULE_RDMA0, _rdma0_irq_handler);

	disp_path_get_mutex();

	if (disp_ovl_engine.OvlBufAddr[disp_ovl_engine.RdmaRdIdx] != 0) {
		disp_ovl_engine_params.path_info.ovl_config.fmt = eRGB888;
		disp_ovl_engine_params.path_info.ovl_config.src_pitch = lcm_width * buffer_bpp;
		disp_ovl_engine_params.path_info.ovl_config.addr =
		    disp_ovl_engine.OvlBufAddr[disp_ovl_engine.RdmaRdIdx];
		disp_ovl_engine_params.path_info.addr =
					disp_ovl_engine.OvlBufAddr[disp_ovl_engine.RdmaRdIdx];
	}
	disp_path_config(&(disp_ovl_engine_params.path_info));
	disp_path_release_mutex();

	/* if(lcm_params->dsi.mode != CMD_MODE) */
	{
		/* disp_path_release_mutex(); */
	}
	if (disp_ovl_engine.OvlBufAddr[disp_ovl_engine.RdmaRdIdx] != 0) {
		disp_module_clock_off(DISP_MODULE_GAMMA, "OVL");
		/* disp_module_clock_off(DISP_MODULE_WDMA1, "OVL"); */
	}
    /*************************************************/
	/* Ultra config */

	DISP_REG_SET(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING, 0x40402020);
	DISP_REG_SET(DISP_REG_OVL_RDMA1_MEM_GMC_SETTING, 0x40402020);
	DISP_REG_SET(DISP_REG_OVL_RDMA2_MEM_GMC_SETTING, 0x40402020);
	DISP_REG_SET(DISP_REG_OVL_RDMA3_MEM_GMC_SETTING, 0x40402020);

	/* disp_wdma1 ultra */
	DISP_REG_SET(DISP_REG_WDMA_BUF_CON1 + 0x1000, 0x800800ff);
    /*************************************************/

	/* pr_info("DUMP register =============================================\n"); */
	/* disp_dump_reg(DISP_MODULE_OVL); */
	/* disp_dump_reg(DISP_MODULE_RDMA0); */
	/* pr_info("DUMP register end =============================================\n"); */

}

static void disp_ovl_engine_565_to_888(void *src_va, void *dst_va)
{
	unsigned int xres = DISP_GetScreenWidth();
	unsigned int yres = DISP_GetScreenHeight();

	unsigned short *s = (unsigned short *)(src_va);
	unsigned char *d = (unsigned char *)(dst_va);
	unsigned short src_rgb565 = 0;
	int j = 0;
	int k = 0;

	pr_info("disp ovl engine 555_to_888, s = 0x%x, d=0x%x\n", (unsigned int)s, (unsigned int)d);
	for (j = 0; j < yres; ++j) {
		for (k = 0; k < xres; ++k) {
			src_rgb565 = *s++;
			*d++ = ((src_rgb565 & 0x1F) << 3);
			*d++ = ((src_rgb565 & 0x7E0) >> 3);
			*d++ = ((src_rgb565 & 0xF800) >> 8);
		}
		/* s += (ALIGN_TO(xres, disphal_get_fb_alignment()) - xres); */
	}
	__cpuc_flush_dcache_area(dst_va, ((xres * yres * 3) + 63) & ~63);
}

static int g_ovl_wdma_mutex = 4;

int disp_ovl_engine_indirect_link_overlay(void *fb_va)
{
	/* Steps for reconfig display path
	   1. allocate internal buffer
	   2. set overlay output to buffer
	   3. config rdma read from memory and change mutex setting
	   4. config overlay to wdma, reconfig new mutex
	 */
	int lcm_width, lcm_height;
	int buffer_bpp;
	int tmpBufferSize;
	int temp_va = 0;
	static int internal_buffer_init;
/* struct disp_path_config_struct rConfig = {0}; */
	struct disp_path_config_struct config = { 0 };
	int i = 0;

	/* pr_info("DUMP register =============================================\n"); */
	/* disp_dump_reg(DISP_MODULE_OVL); */
	/* disp_dump_reg(DISP_MODULE_RDMA0); */
	/* pr_info("DUMP register end =============================================\n"); */

	DISP_OVL_ENGINE_ERR("indirect link overlay\n");

	DISP_OVL_ENGINE_DBG
	    ("ovl addr: 0x%x srcModule: %d dstModule: %d inFormat: %d outFormat: %d\n",
	     disp_ovl_engine_params.path_info.ovl_config.addr,
	     disp_ovl_engine_params.path_info.srcModule, disp_ovl_engine_params.path_info.dstModule,
	     disp_ovl_engine_params.path_info.inFormat, disp_ovl_engine_params.path_info.outFormat);

	/* step 1, alloc resource */
	lcm_width = DISP_GetScreenWidth();
	lcm_height = DISP_GetScreenHeight();
	buffer_bpp = 3;		/* (DISP_GetScreenBpp() + 7) >> 3; */
	tmpBufferSize = lcm_width * lcm_height * buffer_bpp * OVL_ENGINE_OVL_BUFFER_NUMBER;

	DISP_OVL_ENGINE_DBG("lcm_width: %d, lcm_height: %d, buffer_bpp: %d\n",
			    lcm_width, lcm_height, buffer_bpp);

	if (0 == internal_buffer_init) {
		internal_buffer_init = 1;

		DISP_OVL_ENGINE_DBG("indirect link alloc internal buffer\n");

		temp_va = (unsigned int)vmalloc(tmpBufferSize);

		if (((void *)temp_va) == NULL) {
			DISP_OVL_ENGINE_DBG("vmalloc %dbytes fail\n", tmpBufferSize);
			return OVL_ERROR;
		}

		if (m4u_alloc_mva(M4U_CLNTMOD_WDMA,
				  temp_va, tmpBufferSize, 0, 0, &disp_ovl_engine.Ovlmva)) {
			DISP_OVL_ENGINE_DBG("m4u_alloc_mva for disp_ovl_engine.Ovlmva fail\n");
			return OVL_ERROR;
		}

		m4u_dma_cache_maint(M4U_CLNTMOD_WDMA,
				    (void const *)temp_va, tmpBufferSize, DMA_BIDIRECTIONAL);

			for (i = 0; i < OVL_ENGINE_OVL_BUFFER_NUMBER; i++) {
				disp_ovl_engine.OvlBufAddr[i] =
				disp_ovl_engine.Ovlmva + lcm_width * lcm_height * buffer_bpp * i;

			disp_ovl_engine.OvlBufAddr_va[i] =
				temp_va + lcm_width * lcm_height * buffer_bpp * i;

			disp_ovl_engine_565_to_888(fb_va, (void *)disp_ovl_engine.OvlBufAddr_va[i]);

				disp_ovl_engine.OvlBufSecurity[i] = FALSE;
			}

		DISP_OVL_ENGINE_ERR("M4U alloc mva: 0x%x va: 0x%x size: 0x%x\n",
				    disp_ovl_engine.Ovlmva, temp_va, tmpBufferSize);
	}

	disp_module_clock_on(DISP_MODULE_GAMMA, "OVL");
	/* disp_module_clock_on(DISP_MODULE_WDMA1, "OVL"); */
	/* disp_module_clock_on(DISP_MODULE_RDMA1, "OVL"); */

	disp_ovl_engine.OvlWrIdx = 0;
	disp_ovl_engine.RdmaRdIdx = 0;
	/* temp_va = disp_ovl_engine.Ovlmva + lcm_width * lcm_height * buffer_bpp * disp_ovl_engine.OvlWrIdx; */
	/* step 2, config WDMA1 */
	{
		M4U_PORT_STRUCT portStruct;

		portStruct.ePortID = M4U_PORT_OVL_CH0;
		portStruct.Virtuality = 1;
		portStruct.Security = 0;
		portStruct.domain = 3;
		portStruct.Distance = 1;
		portStruct.Direction = 0;
		m4u_config_port(&portStruct);

		portStruct.ePortID = M4U_PORT_WDMA1;	/* hardware port ID, defined in M4U_PORT_ID_ENUM //M4U_PORT_WDMA1; */
		portStruct.Virtuality = 1;
		portStruct.Security = 0;
		portStruct.domain = 1;	/* domain : 0 1 2 3 */
		portStruct.Distance = 1;
		portStruct.Direction = 0;
		m4u_config_port(&portStruct);

			portStruct.ePortID = M4U_PORT_RDMA0;	/* hardware port ID, defined in M4U_PORT_ID_ENUM */
			portStruct.Virtuality = 1;
			portStruct.Security = 0;
			portStruct.domain = 1;	/* domain : 0 1 2 3 */
			portStruct.Distance = 1;
			portStruct.Direction = 0;
			m4u_config_port(&portStruct);

		}

		/* RDMAReset(0); */
	config.srcModule = DISP_MODULE_RDMA0;
	config.inFormat = RDMA_INPUT_FORMAT_RGB888;
	config.addr = disp_ovl_engine.OvlBufAddr[0]; /* *(volatile unsigned int *)(0xF40030A0); */
	config.pitch = lcm_width * buffer_bpp;
	config.srcHeight = lcm_height;
	config.srcWidth = lcm_width;

	if (LCM_TYPE_DSI == lcm_params->type) {
		if (lcm_params->dsi.mode == CMD_MODE)
			config.dstModule = DISP_MODULE_DSI_CMD;/* DISP_MODULE_WDMA1 */
		else
			config.dstModule = DISP_MODULE_DSI_VDO;/* DISP_MODULE_WDMA1 */

	} else if (LCM_TYPE_DPI == lcm_params->type) {
		config.dstModule = DISP_MODULE_DPI0;
	} else
		config.dstModule = DISP_MODULE_DBI;


	config.outFormat = RDMA_OUTPUT_FORMAT_ARGB;
	disp_register_irq(DISP_MODULE_RDMA0, _rdma0_irq_handler);
	DISP_WaitVSYNC();
	disp_update_mutex();
	disp_path_get_mutex();
	disp_path_config_internal_setting(&config);
	disp_path_release_mutex();
	disp_path_wait_reg_update();

	/* wait RDMA0 & WDMA1 ready */
	DISP_WaitVSYNC();
	/* prevent OVL stay in error state & unceasingly throw error IRQ */
	OVLStop();
	disp_path_config_internal_mutex(&config);
	disp_module_clock_off(DISP_MODULE_OVL, "OVL");

	/* disp_dump_reg(DISP_MODULE_MUTEX); */
	DISP_OVL_ENGINE_ERR("indirect link overlay leave\n");
	return 0;
}

void disp_ovl_engine_set_overlay_to_buffer(void)
{
	if (disp_ovl_engine.bCouple) {
		disp_path_config_mem_out(&disp_ovl_engine_params.MemOutConfig);
	} else {
		/* output to overlay buffer */
	}
}

int disp_ovl_engine_trigger_hw_overlay_couple(void)
{
	struct disp_path_config_mem_out_struct rMemOutConfig = { 0 };
	unsigned int temp_va = 0;
	unsigned int width, height, bpp;
	unsigned int size = 0, layer_id;
	int OvlSecureNew = 0;

	/* overlay output to internal buffer */
	if (atomic_read(&disp_ovl_engine_params.OverlaySettingDirtyFlag)) {
		/* output to buffer */
		disp_module_clock_on(DISP_MODULE_SMI, "OVL");
		disp_module_clock_on(DISP_MODULE_OVL, "OVL");
		disp_module_clock_on(DISP_MODULE_WDMA1, "OVL");

		if (down_interruptible(&disp_rdma0_update_semaphore)) {
			DISP_OVL_ENGINE_ERR(
				"disp_ovl_engine_trigger_hw_overlay_couple "
				"down_interruptible(disp_rdma0_update_semaphore) fail\n");
			return 0;
		}

		if (((disp_ovl_engine.OvlWrIdx + 1) % OVL_ENGINE_OVL_BUFFER_NUMBER) == disp_ovl_engine.RdmaRdIdx) {
			DISP_OVL_ENGINE_ERR
			    ("OVL BuffCtl RDMA hang (%d), stop write (ovlWrIdx: %d)\n",
			     disp_ovl_engine.RdmaRdIdx, disp_ovl_engine.OvlWrIdx);
			disp_ovl_engine.OvlWrIdx = (disp_ovl_engine.OvlWrIdx + OVL_ENGINE_OVL_BUFFER_NUMBER - 1)
				% OVL_ENGINE_OVL_BUFFER_NUMBER;
			DISP_OVL_ENGINE_ERR("OVL BuffCtl new WrIdx: %d\n", disp_ovl_engine.OvlWrIdx);
			/*return OVL_ERROR;*/
		}

		up(&disp_rdma0_update_semaphore);

		width = DISP_GetScreenWidth();
		height = DISP_GetScreenHeight();
		bpp = 3;	/* (DISP_GetScreenBpp() + 7) >> 3; */

		DISP_OVL_ENGINE_DBG("OVL BuffCtl disp_ovl_engine.OvlWrIdx: %d\n",
				    disp_ovl_engine.OvlWrIdx);
		/* temp_va = disp_ovl_engine.Ovlmva + (width * height * bpp) * disp_ovl_engine.OvlWrIdx; */

		OVLReset();
		WDMAReset(1);

		disp_path_config_OVL_WDMA_path(gOvlWdmaMutexID, TRUE, FALSE);
		disp_path_get_mutex_(g_ovl_wdma_mutex);

		for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
			if ((disp_ovl_engine_params.cached_layer_config[layer_id].layer_en) &&
			    (disp_ovl_engine_params.cached_layer_config[layer_id].security ==
			     LAYER_SECURE_BUFFER))
				OvlSecureNew = 1;
		}

		if (OvlSecure != OvlSecureNew) {
			if (OvlSecureNew) {
				OvlSecure = OvlSecureNew;
				WDMASetSecure(OvlSecure);

				DISP_OVL_ENGINE_ERR
				    ("disp_ovl_engine_trigger_hw_overlay_couple, OvlSecure=0x%x\n",
				     OvlSecure);

				disp_register_intr(MT8135_DISP_OVL_IRQ_ID, OvlSecure);
				disp_register_intr(MT8135_DISP_WDMA1_IRQ_ID, OvlSecure);
			}
		}
#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
		/* Allocate or free secure buffer */
		if (disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.OvlWrIdx] != OvlSecureNew) {

			if (OvlSecureNew) {
				/* Allocate secure buffer */
				disp_ovl_engine.OvlBufAddr[disp_ovl_engine.OvlWrIdx] =
				    (unsigned int)disp_ovl_engine_hw_allocate_secure_memory(width *
											    height *
											    bpp);
				disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.OvlWrIdx] = TRUE;
			} else {
				/* Free secure buffer */
				disp_ovl_engine_hw_free_secure_memory((void
								       *)(disp_ovl_engine.OvlBufAddr
									  [disp_ovl_engine.
									   OvlWrIdx]));
				disp_ovl_engine.OvlBufAddr[disp_ovl_engine.OvlWrIdx] =
				    disp_ovl_engine.Ovlmva +
				    (width * height * bpp) * disp_ovl_engine.OvlWrIdx;
				disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.OvlWrIdx] = FALSE;
			}

			DISP_OVL_ENGINE_ERR("OvlBufSecurity[%d] = %d\n", disp_ovl_engine.OvlWrIdx,
					    OvlSecureNew);
		}
#endif

		temp_va = disp_ovl_engine.OvlBufAddr[disp_ovl_engine.OvlWrIdx];
		rMemOutConfig.dirty = TRUE;
		rMemOutConfig.dstAddr = temp_va;
		rMemOutConfig.enable = TRUE;
		rMemOutConfig.outFormat = eRGB888;
		rMemOutConfig.srcROI.x = 0;
		rMemOutConfig.srcROI.y = 0;
		rMemOutConfig.srcROI.width = width;
		rMemOutConfig.srcROI.height = height;
		rMemOutConfig.security = OvlSecureNew;
		disp_path_config_OVL_WDMA(&rMemOutConfig, OvlSecure);

		for (layer_id = 0; layer_id < DDP_OVL_LAYER_MUN; layer_id++) {
			disp_ovl_engine_params.cached_layer_config[layer_id].layer = layer_id;
			disp_path_config_layer_ovl_engine(&
							  (disp_ovl_engine_params.
							   cached_layer_config[layer_id]),
							  OvlSecure);
		}
		disp_path_release_mutex_(g_ovl_wdma_mutex);

		if (OvlSecure != OvlSecureNew) {
			if (!OvlSecureNew) {
				OvlSecure = OvlSecureNew;
				WDMASetSecure(OvlSecure);

				DISP_OVL_ENGINE_ERR
				    ("disp_ovl_engine_trigger_hw_overlay_couple, OvlSecure=0x%x\n",
				     OvlSecure);

				disp_register_intr(MT8135_DISP_OVL_IRQ_ID, OvlSecure);
				disp_register_intr(MT8135_DISP_WDMA1_IRQ_ID, OvlSecure);
			}
		}

		/* disp_path_wait_mem_out_done(); */
		/* disp_ovl_engine.OvlWrIdx++; */
		/* disp_ovl_engine.OvlWrIdx %= OVL_ENGINE_OVL_BUFFER_NUMBER; */

	}
	if (disp_ovl_engine_params.MemOutConfig.dirty) {

		if (atomic_read(&disp_ovl_engine_params.OverlaySettingDirtyFlag)) {
			/* copy data from overlay buffer to capture buffer */
			/* wait wdma done */
			disp_path_wait_ovl_wdma_done();
			/* sw copy ,maybe need change to wdma write... */
			size =
			    disp_ovl_engine_params.MemOutConfig.srcROI.width *
			    disp_ovl_engine_params.MemOutConfig.srcROI.width * 3;
			memcpy(&disp_ovl_engine_params.MemOutConfig.dstAddr, &temp_va, size);
		} else {
		/*
			disp_path_get_mutex_(g_ovl_wdma_mutex);
			disp_path_config_OVL_WDMA(&disp_ovl_engine_params.MemOutConfig, OvlSecure);
			disp_path_release_mutex_(g_ovl_wdma_mutex);
		*/
		}
	}

	return 0;
}

int dump_all_info(void)
{
	int i;

	DISP_OVL_ENGINE_INFO
	    ("dump_all_info ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

	DISP_OVL_ENGINE_INFO
	    ("disp_ovl_engine:\nbInit %d bCouple %d bModeSwitch %d Ovlmva 0x%x OvlWrIdx %d RdmaRdIdx %d\n",
	     disp_ovl_engine.bInit, disp_ovl_engine.bCouple, disp_ovl_engine.bModeSwitch,
	     disp_ovl_engine.Ovlmva, disp_ovl_engine.OvlWrIdx, disp_ovl_engine.RdmaRdIdx);

	for (i = 0; i < 2; i++) {
		DISP_OVL_ENGINE_INFO
		    ("disp_ovl_engine.Instance[%d]:\nindex %d bUsed %d mode %d status %d "
		     "OverlaySettingDirtyFlag %d OverlaySettingApplied %d fgNeedConfigM4U %d\n", i,
		     disp_ovl_engine.Instance[i].index, disp_ovl_engine.Instance[i].bUsed,
		     disp_ovl_engine.Instance[i].mode, disp_ovl_engine.Instance[i].status,
		     *(unsigned int *)(&disp_ovl_engine.Instance[i].OverlaySettingDirtyFlag),
		     *(unsigned int *)(&disp_ovl_engine.Instance[i].OverlaySettingApplied),
		     (unsigned int)disp_ovl_engine.Instance[i].fgNeedConfigM4U);

		DISP_OVL_ENGINE_INFO("disp_ovl_engine.Instance[%d].cached_layer_config:\n "
				     "layer %d layer_en %d source %d addr 0x%x vaddr 0x%x fmt %d src_x %d"
				     "src_y %d src_w %d src_h %d src_pitch %d dst_x %d dst_y %d dst_w %d "
				     "dst_h %d isDirty %d security %d\n", i,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].layer,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].layer_en,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].source,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].addr,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].vaddr,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].fmt,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].src_x,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].src_y,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].src_w,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].src_h,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].src_pitch,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].dst_x,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].dst_y,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].dst_w,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].dst_h,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].isDirty,
				     disp_ovl_engine.Instance[i].cached_layer_config[3].security);
	}



	/* disp_dump_reg(DISP_MODULE_OVL); */
	/* disp_dump_reg(DISP_MODULE_MUTEX); */
	/* disp_dump_reg(DISP_MODULE_WDMA1); */
	/* disp_dump_reg(DISP_MODULE_RDMA0); */

	DISP_OVL_ENGINE_INFO
	    ("dump_all_info end --------------------------------------------------------\n");

	return 0;
}

int disp_ovl_disable_all_layer = true;
void disp_ovl_engine_trigger_hw_overlay(void)
{
	/*unsigned int layer_en_backup[DDP_OVL_LAYER_MUN] = { 0 }; */

	disp_path_config_layer_ovl_engine_control(true);

	/* bls occupied mutex3 in loader and change to mutex0 in kernel without release mutex3 */
	if (disp_ovl_engine.bPreventMutex3ReleaseTimeOut == 1) {
		disp_ovl_engine.bPreventMutex3ReleaseTimeOut = 0;

		/* DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(0), 0x0); */
		/* DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(2), 0x0); */

		{
			unsigned int mutex3_mod = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_MOD(3));
			DISP_MSG("[DDP] %s mutex3_mod = 0x%x\n",
			__func__ , mutex3_mod);

			if (mutex3_mod & (1<<9)) {
				unsigned int timeout = DISP_REG_GET(DISP_REG_CONFIG_REG_UPD_TIMEOUT);
				unsigned int commit = DISP_REG_GET(DISP_REG_CONFIG_REG_COMMIT);

				DISP_MSG("[DDP] %s reset mutex 3 MOD 0x%x timeout 0x%x commit 0x%x\n",
				__func__ , mutex3_mod, timeout, commit);

				if ((commit & (1<<9)) == 0) {
					DISP_MSG("[DDP] %s commit 0x%x of bls bit(9) is 0\n", __func__ , commit);

					/* set mutex3 mod*/
					DISP_REG_SET(DISP_REG_CONFIG_MUTEX_MOD(3), 0);

					/* reset mutex3 */
					DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(3), 1);
					DISP_REG_SET(DISP_REG_CONFIG_MUTEX_RST(3), 0);
				}
			}
		}
	}

	/* decouple mode */
		DISP_OVL_ENGINE_DBG(" decouple mode\n");
		if (COUPLE_MODE == disp_ovl_engine_params.mode) {	/* couple instance */
			DISP_OVL_ENGINE_DBG(" couple instance\n");
			disp_ovl_engine_trigger_hw_overlay_couple();
		} else {	/* de-couple instance */

			DISP_OVL_ENGINE_DBG(" decouple instance\n");
			disp_ovl_engine_trigger_hw_overlay_decouple();
		}

}

void disp_ovl_engine_hw_register_irq(void (*irq_callback) (unsigned int param))
{
	disp_ovl_engine_hw_irq_callback = irq_callback;
}


int disp_ovl_engine_hw_mva_map(struct disp_mva_map *mva_map_struct)
{
#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
	MTEEC_PARAM param[4];
	unsigned int paramTypes;
	TZ_RESULT ret;

	param[0].value.a = mva_map_struct->module;
	param[1].value.a = mva_map_struct->cache_coherent;
	param[2].value.a = mva_map_struct->addr;
	param[3].value.a = mva_map_struct->size;
	paramTypes =
	    TZ_ParamTypes4(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
	ret =
	    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_SECURE_MVA_MAP, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		DISP_OVL_ENGINE_ERR("KREE_TeeServiceCall(TZCMD_DDP_SECURE_MVA_MAP) fail, ret=%d\n",
				    ret);

		return -1;
	}
#endif
	return 0;
}



int disp_ovl_engine_hw_mva_unmap(struct disp_mva_map *mva_map_struct)
{
#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
	MTEEC_PARAM param[4];
	unsigned int paramTypes;
	TZ_RESULT ret;

	param[0].value.a = mva_map_struct->module;
	param[1].value.a = mva_map_struct->cache_coherent;
	param[2].value.a = mva_map_struct->addr;
	param[3].value.a = mva_map_struct->size;
	paramTypes =
	    TZ_ParamTypes4(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
	ret =
	    KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_SECURE_MVA_UNMAP, paramTypes,
				param);
	if (ret != TZ_RESULT_SUCCESS) {
		DISP_OVL_ENGINE_ERR
		    ("KREE_TeeServiceCall(TZCMD_DDP_SECURE_MVA_UNMAP) fail, ret=%d\n", ret);

		return -1;
	}
#endif
	return 0;
}

int disp_ovl_engine_hw_reset(void)
{
	if (disp_ovl_engine.bCouple) {
		OVLReset();
		RDMAReset(0);
	} else {
		OVLReset();
		WDMAReset(1);
	}
	return OVL_OK;
}

#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
static KREE_SESSION_HANDLE disp_ovl_engine_secure_memory_session;
KREE_SESSION_HANDLE disp_ovl_engine_secure_memory_session_handle(void)
{
	DISP_OVL_ENGINE_DBG("disp_ovl_engine_secure_memory_session_handle() acquire TEE session\n");
	/* TODO: the race condition here is not taken into consideration. */
	if (NULL == disp_ovl_engine_secure_memory_session) {
		TZ_RESULT ret;
		DISP_OVL_ENGINE_DBG
		    ("disp_ovl_engine_secure_memory_session_handle() create session\n");
		ret = KREE_CreateSession(TZ_TA_MEM_UUID, &disp_ovl_engine_secure_memory_session);
		if (ret != TZ_RESULT_SUCCESS) {
			DISP_OVL_ENGINE_ERR("KREE_CreateSession fail, ret=%d\n", ret);
			return NULL;
		}
	}

	DISP_OVL_ENGINE_DBG("disp_ovl_engine_secure_memory_session_handle() session=%x\n",
			    (unsigned int)disp_ovl_engine_secure_memory_session);
	return disp_ovl_engine_secure_memory_session;
}


void *disp_ovl_engine_hw_allocate_secure_memory(int size)
{
	KREE_SECUREMEM_HANDLE mem_handle;
	TZ_RESULT ret;
	struct disp_mva_map mva_map_struct;

	/* Allocate */
	ret = KREE_AllocSecurechunkmem(disp_ovl_engine_secure_memory_session_handle(),
				       &mem_handle, 0, size);
	if (ret != TZ_RESULT_SUCCESS) {
		DISP_OVL_ENGINE_ERR("KREE_AllocSecurechunkmem fail, ret=%d\n", ret);
		return NULL;
	}

	DISP_OVL_ENGINE_DBG("KREE_AllocSecurechunkmem handle=0x%x\n", mem_handle);

	/* Map mva */
	mva_map_struct.addr = (unsigned int)mem_handle;
	mva_map_struct.size = size;
	mva_map_struct.cache_coherent = 0;
	mva_map_struct.module = M4U_CLNTMOD_RDMA;
	disp_ovl_engine_hw_mva_map(&mva_map_struct);


	return (void *)mem_handle;
}


void disp_ovl_engine_hw_free_secure_memory(void *mem_handle)
{
	TZ_RESULT ret;
	struct disp_mva_map mva_map_struct;

	/* Unmap mva */
	mva_map_struct.addr = (unsigned int)mem_handle;
	mva_map_struct.size = 0;
	mva_map_struct.cache_coherent = 0;
	mva_map_struct.module = M4U_CLNTMOD_RDMA;
	disp_ovl_engine_hw_mva_unmap(&mva_map_struct);

	/* Free */
	ret = KREE_UnreferenceSecurechunkmem(disp_ovl_engine_secure_memory_session_handle(),
					     (KREE_SECUREMEM_HANDLE) mem_handle);

	DISP_OVL_ENGINE_DBG("KREE_UnreferenceSecurechunkmem handle=0x%0x\n",
			    (unsigned int)mem_handle);

	if (ret != TZ_RESULT_SUCCESS) {
		DISP_OVL_ENGINE_ERR("KREE_UnreferenceSecurechunkmem fail, ret=%d\n", ret);
	}

	return;
}
#endif


int disp_ovl_engine_update_rdma0(void)
{
	static unsigned int rdma_buffer_addr;
	static int rdma_cur_secure;
	static int jiffies_prev;
	int jiffies_cur = 0;

	jiffies_cur = jiffies;
	if ((jiffies_cur - jiffies_prev) > HZ)
		DISP_OVL_ENGINE_ERR("disp_ovl_engine_update_rdma0 delay %d tick\n", (jiffies_cur - jiffies_prev));

	jiffies_prev = jiffies_cur;

	if (gRdma0IrqChange == 1) {
		rdma_buffer_addr = gRdma0IrqAddress;
		rdma_cur_secure = gRdma0IrqSecure;
	}

	if (rdma_buffer_addr == 0)
		return 0;

	disp_path_get_mutex_ex(0, 0);

	#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
	if (RdmaSecure) {
		MTEEC_PARAM param[4];
		unsigned int paramTypes;
		TZ_RESULT ret;

		param[0].value.a = (uint32_t) rdma_buffer_addr;
		param[1].value.a = rdma_cur_secure;
		param[2].value.a = DISP_GetScreenWidth()*DISP_GetScreenHeight()*3;
		paramTypes = TZ_ParamTypes3(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
		DISP_OVL_ENGINE_DBG("Rdma config handle=0x%x\n", param[0].value.a);

		ret = KREE_TeeServiceCall(ddp_session_handle(), TZCMD_DDP_RDMA_ADDR_CONFIG, paramTypes, param);
		if (ret != TZ_RESULT_SUCCESS)
			DISP_OVL_ENGINE_ERR("TZCMD_DDP_RDMA_ADDR_CONFIG fail, ret=%d\n", ret);
	} else
	#endif
		DISP_REG_SET(DISP_REG_RDMA_MEM_START_ADDR, rdma_buffer_addr);

	disp_path_release_mutex_ex(0, 0);

	if (gRdma0IrqChange == 1)
		gRdma0IrqChange = 2;

	return 0;
}


void disp_ovl_engine_wake_up_rdma0_update_thread(void)
{
	gWakeupRdma0UpdateThread = 1;
	wake_up(&disp_rdma0_update_wq);
}

static int disp_ovl_engine_rdma0_update_kthread(void *data)
{
	int wait_ret = 0;
	struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
	unsigned int rdma_buffer_addr;
	int rdma_cur_secure;
	static unsigned int cur_rdma_idx = 0xFF;
	static unsigned int next_rdma_idx = OVL_ENGINE_OVL_BUFFER_NUMBER - 1;

	sched_setscheduler(current, SCHED_RR, &param);

	while (1) {
		wait_ret = wait_event_interruptible(disp_rdma0_update_wq, gWakeupRdma0UpdateThread);
		gWakeupRdma0UpdateThread = 0;
		if (gRdma0IrqChange != 0)
			continue;

		if (down_interruptible(&disp_rdma0_update_semaphore)) {
			DISP_OVL_ENGINE_ERR(
				"disp_ovl_engine_rdma0_update_kthread "
				"down_interruptible(disp_rdma0_update_semaphore) fail\n");
			continue;
		}

		if (((next_rdma_idx + 1) % OVL_ENGINE_OVL_BUFFER_NUMBER) != disp_ovl_engine.OvlWrIdx)
			next_rdma_idx = (next_rdma_idx + 1) % OVL_ENGINE_OVL_BUFFER_NUMBER;

		/* skip to config repeat buffer */
		if (disp_ovl_engine.RdmaRdIdx == next_rdma_idx) {
			up(&disp_rdma0_update_semaphore);
			continue;
		}

		disp_ovl_engine.RdmaRdIdx = next_rdma_idx;

		up(&disp_rdma0_update_semaphore);

		cur_rdma_idx = disp_ovl_engine.RdmaRdIdx;

		rdma_buffer_addr = disp_ovl_engine.OvlBufAddr[disp_ovl_engine.RdmaRdIdx];
		rdma_cur_secure = disp_ovl_engine.OvlBufSecurity[disp_ovl_engine.RdmaRdIdx];
#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
		/* Switch REE to TEE */
		if (rdma_cur_secure != RdmaSecure) {
			if (rdma_cur_secure) {
				RdmaSecure = rdma_cur_secure;

				disp_register_intr(MT8135_DISP_RDMA0_IRQ_ID, RdmaSecure);

				DISP_OVL_ENGINE_ERR("gRdma0IrqSecure = %d, RdmaRdIdx = %d, switch to TEE\n",
					RdmaSecure, disp_ovl_engine.RdmaRdIdx);
			}
		}
#endif

		/* disp_ovl_engine_update_rdma0(); */
		gRdma0IrqAddress = rdma_buffer_addr;
		gRdma0IrqSecure = rdma_cur_secure;
		gRdma0IrqChange = 1;
#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
		/* Switch TEE to REE */
		if (rdma_cur_secure != RdmaSecure) {
			if (!rdma_cur_secure) {
				disp_register_intr(MT8135_DISP_RDMA0_IRQ_ID, rdma_cur_secure);

				RdmaSecure = rdma_cur_secure;

				DISP_OVL_ENGINE_ERR("gRdma0IrqSecure = %d, RdmaRdIdx = %d, switch to REE\n",
					RdmaSecure, disp_ovl_engine.RdmaRdIdx);
			}
		}
#endif
		if (kthread_should_stop())
			break;
	}

	return OVL_OK;
}


#endif
