#ifndef __DISP_OVL_ENGINE_PRIV_H__
#define __DISP_OVL_ENGINE_PRIV_H__

#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include "disp_ovl_engine_dev.h"
#include "disp_ovl_engine_api.h"
#include "ddp_path.h"


extern int disp_ovl_engine_log_level;
#define DISP_OVL_ENGINE_ERR(string, args...) if (disp_ovl_engine_log_level >= 0) pr_err("[OVL]"string, ##args)
#define DISP_OVL_ENGINE_WARN(string, args...) if (disp_ovl_engine_log_level >= 1) pr_err("[OVL]"string, ##args)
#define DISP_OVL_ENGINE_INFO(string, args...) if (disp_ovl_engine_log_level >= 2) pr_err("[OVL]"string, ##args)
#define DISP_OVL_ENGINE_DBG(string, args...) if (disp_ovl_engine_log_level >= 3) pr_err("[OVL]"string, ##args)

/* #define DISP_OVL_ENGINE_REQUEST */
#define DISP_OVL_ENGINE_FENCE


#define OVL_ENGINE_INSTANCE_MAX_INDEX 5

#define OVL_ENGINE_OVL_BUFFER_NUMBER 4
typedef enum {
	OVERLAY_STATUS_IDLE,
	OVERLAY_STATUS_TRIGGER,
	OVERLAY_STATUS_BUSY,
	OVERLAY_STATUS_COMPLETE,
	OVERLAY_STATUS_MAX,
} OVERLAY_STATUS_T;


typedef struct {
	unsigned int index;
	bool bUsed;
	OVL_INSTANCE_MODE mode;
	OVERLAY_STATUS_T status;	/* current status */
	OVL_CONFIG_STRUCT cached_layer_config[DDP_OVL_LAYER_MUN];	/* record all layers config; */
	atomic_t OverlaySettingDirtyFlag;
	atomic_t OverlaySettingApplied;
	struct disp_path_config_mem_out_struct MemOutConfig;
	struct disp_path_config_struct path_info;
	bool fgNeedConfigM4U;
	bool fgCompleted;
#ifdef DISP_OVL_ENGINE_FENCE
	struct kthread_worker trigger_overlay_worker;
	struct task_struct *trigger_overlay_thread;
	struct kthread_work trigger_overlay_work;
	struct sw_sync_timeline *timeline;
	int timeline_max;
	int timeline_skip;
	struct sync_fence *fences[DDP_OVL_LAYER_MUN];
	struct workqueue_struct *wq;
	int outFence;
#endif
} DISP_OVL_ENGINE_INSTANCE;


typedef struct {
	bool bInit;

	DISP_OVL_ENGINE_INSTANCE Instance[OVL_ENGINE_INSTANCE_MAX_INDEX];
	DISP_OVL_ENGINE_INSTANCE *current_instance;

	OVL_CONFIG_STRUCT *captured_layer_config;
	OVL_CONFIG_STRUCT *realtime_layer_config;
	OVL_CONFIG_STRUCT layer_config[2][DDP_OVL_LAYER_MUN];
	unsigned int layer_config_index;

	struct mutex OverlaySettingMutex;
	bool bCouple;		/* overlay mode */
	bool bModeSwitch;	/* switch between couple mode & de-couple mode */
	unsigned int Ovlmva;
	unsigned int OvlWrIdx;
	unsigned int RdmaRdIdx;
	bool OvlBufSecurity[OVL_ENGINE_OVL_BUFFER_NUMBER];
	unsigned int OvlBufAddr[OVL_ENGINE_OVL_BUFFER_NUMBER];
	unsigned int OvlBufAddr_va[OVL_ENGINE_OVL_BUFFER_NUMBER];
	struct ion_client *ion_client;
	bool bPreventMutex3ReleaseTimeOut;
} DISP_OVL_ENGINE_PARAMS;

extern DISP_OVL_ENGINE_PARAMS disp_ovl_engine;
extern struct semaphore disp_ovl_engine_semaphore;
extern wait_queue_head_t disp_ovl_complete_wq;


void disp_ovl_engine_init(void);
void disp_ovl_engine_wake_up_ovl_engine_thread(void);
void disp_ovl_engine_interrupt_handler(unsigned int param);
void disp_ovl_engine_config_overlay(void);
#ifdef DISP_OVL_ENGINE_REQUEST
int Disp_Ovl_Engine_Set_Request(struct disp_ovl_engine_request_struct *overlayRequest, int timeout);
int Disp_Ovl_Engine_Get_Request(struct disp_ovl_engine_request_struct *overlayRequest);
int Disp_Ovl_Engine_Ack_Request(struct disp_ovl_engine_request_struct *overlayRequest);
#endif
void Disp_Ovl_Engine_clock_on(void);

void Disp_Ovl_Engine_clock_off(void);

void disp_ovl_engine_wake_up_rdma0_update_thread(void);

#endif
