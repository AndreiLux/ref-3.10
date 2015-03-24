/*****************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_mfgdvfs.c
 *
 * Project:
 * --------
 *   MT8135
 *
 * Description:
 * ------------
 *   Implementation power policy and loading detection for DVFS
 *
 * Author:
 * -------
 *   Enzhu Wang
 *
 *============================================================================
 * History and modification
 *1. Initial @ July 10th 2013, to verify V & F switch,not according to loading currently
 *2. Added GPU DVFS mtk flow and timer proc implemetation
 *============================================================================
 ****************************************************************************/

#include "mtk_mfgsys.h"
#include "mtk_mfgdvfs.h"
#include "mach/mt_gpufreq.h"
#include "mach/mt_clkmgr.h"
#include "mach/mt_typedefs.h"
/* #include "mach/upmu_common.h" */
#include "pvrsrv_error.h"
#include "allocmem.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "power.h"



/**************************
* GPU DVFS define
***************************/
#define MTK_RGX_DEVICE_INDEX_INVALID   0xFFFF
#define MTK_RGX_DVFS_TBL_INDEX_INVALID 0xFFFF
#define MTK_RGX_DVFS_LOAD_INVALID      0xFFFF

static MTK_DVFS_T *gpsMTKDVFS = IMG_NULL;

static IMG_UINT32 g_ui32DevIdx = MTK_RGX_DEVICE_INDEX_INVALID;
static IMG_UINT32 g_ui32TblIdx = MTK_RGX_DVFS_TBL_INDEX_INVALID;


static struct mt_gpufreq_info ar_freqs_vgpu_table[3][4] = {
{
	{GPU_DVFS_F1, 50, 100, GPU_POWER_VGPU_1_15V_VOL, 100},
	{GPU_DVFS_F3, 0,  50,  GPU_POWER_VGPU_1_05V_VOL, 100},
},

{
	{GPU_DVFS_F1, 60, 100, GPU_POWER_VGPU_1_15V_VOL, 100},
	{GPU_DVFS_F3, 20, 60,  GPU_POWER_VGPU_1_05V_VOL, 100},
	{GPU_DVFS_F6, 0,  20,  GPU_POWER_VGPU_1_00V_VOL, 100},
},

{
	{GPU_DVFS_F1, 70, 100, GPU_POWER_VGPU_1_15V_VOL, 100},
	{GPU_DVFS_F2, 50, 70,  GPU_POWER_VGPU_1_10V_VOL, 100},
	{GPU_DVFS_F3, 30, 50,  GPU_POWER_VGPU_1_05V_VOL, 100},
	{GPU_DVFS_F5, 0,  30,  GPU_POWER_VGPU_1_05V_VOL, 100},
},

};

static IMG_VOID MtkInitSetFreqTbl(FREQ_TABLE_DVFS_TYPE TblType)
{
	switch (TblType)
	{
		case TBL_TYPE_DVFS_2_LEVEL:
			mtk_mfg_debug("[GPU DVFS] freqs_vgpu_table_1 two level ...\n");
			mt_gpufreq_register(ar_freqs_vgpu_table[0], 2);
			g_ui32TblIdx = 0;
			break;
		case TBL_TYPE_DVFS_3_LEVEL:
			mtk_mfg_debug("[GPU DVFS] freqs_vgpu_table_2 three level ...\n");
			mt_gpufreq_register(ar_freqs_vgpu_table[1], 3);
			g_ui32TblIdx = 1;
			break;
		case TBL_TYPE_DVFS_4_LEVEL:
			mtk_mfg_debug("[GPU DVFS] freqs_vgpu_table_3 four level ...\n");
			mt_gpufreq_register(ar_freqs_vgpu_table[2], 4);
			g_ui32TblIdx = 2;
			break;
		case TBL_TYPE_DVFS_NOT_SUPPORT:
		default:
			mtk_mfg_debug("[GPU DVFS] mt_gpufreq_non_register ...\n");
			mt_gpufreq_non_register();
			break;
	}
	return;
}


IMG_INT32 MTKSetFreqInfo(FREQ_TABLE_DVFS_TYPE TblType)
{
	mt_gpufreq_set_initial();
	MtkInitSetFreqTbl(TblType);
	return PVRSRV_OK;
}

static IMG_INT32 MtkDVFSGetDevIdx(IMG_VOID)
{
	IMG_UINT32 ui32DeviceIndex = MTK_RGX_DEVICE_INDEX_INVALID;
	IMG_UINT32 i;

	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++)
	{
		PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[i];
		//PVR_LOG(("[MFG_DVFS] eDeviceType %d", psDeviceNode->psDevConfig->eDeviceType));
		if (psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
		{
			ui32DeviceIndex = i;
		}
	}
	return ui32DeviceIndex;

}

static IMG_VOID MtkDVFSWriteBackFreqToRGX(IMG_UINT32 ui32NewFreq)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[g_ui32DevIdx];
	RGX_DATA			*psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;

	/* kHz to Hz write to RGX as the same unit */
	psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32NewFreq*1000;

	//PVR_LOG(("[MFG_DVFS]ui32CoreClockSpeed %d", psRGXData->psRGXTimingInfo->ui32CoreClockSpeed));

	return;

}

static IMG_VOID MtkCheckSwitchDVFS(IMG_UINT32 ui32Load)
{
	IMG_UINT32 ui32CurFreq = 0;
	IMG_UINT32 ui32NewFreq = 0;
	IMG_UINT32 ui32Idx = 0;
	IMG_UINT32 ui32DevIdx = MTK_RGX_DEVICE_INDEX_INVALID;
	PVRSRV_ERROR	eError = PVRSRV_OK;

	if( MTK_RGX_DVFS_TBL_INDEX_INVALID == g_ui32TblIdx )
	{
		PVR_LOG(("[MFG_DVFS]Invalid GPU DVFS table!"));
		return;
	}

	if(mt_gpufreq_dvfs_disabled())
	{
		PVR_LOG(("[MFG_DVFS]Disable switch GPU DVFS!"));
		return;
	}

	/* ensure g_ui32DevIdx is initialized already */
	ui32DevIdx = g_ui32DevIdx;


	for(ui32Idx = 0; ui32Idx <= (g_ui32TblIdx+1); ui32Idx++)
	{
		if( ui32Load <= ar_freqs_vgpu_table[g_ui32TblIdx][ui32Idx].gpufreq_upper_bound
		   && ui32Load >= ar_freqs_vgpu_table[g_ui32TblIdx][ui32Idx].gpufreq_lower_bound	)
		{
			ui32NewFreq = ar_freqs_vgpu_table[g_ui32TblIdx][ui32Idx].gpufreq_khz;
			break;
		}
	}

	ui32CurFreq = mt_gpufreq_get_cur_freq();

	if(ui32CurFreq!=ui32NewFreq)
	{
		#if MTK_DVFS_IDLE_DEVICE
		eError = PVRSRVDevicePreClockSpeedChange(ui32DevIdx,TRUE,(IMG_VOID*)NULL);
		#else
		eError = PVRSRVDevicePreClockSpeedChange(ui32DevIdx,FALSE,(IMG_VOID*)NULL);
		#endif
		//PVR_LOG(("[MFG_DVFS]ui32CurFreq(%d),ui32NewFreq(%d)",ui32CurFreq,ui32NewFreq));

		if(PVRSRV_OK == eError)
		{
			mt_gpufreq_switch_handler(ui32Load);
			#if MTK_DVFS_IDLE_DEVICE
			PVRSRVDevicePostClockSpeedChange(ui32DevIdx,TRUE,(IMG_VOID*)NULL);
			#else
			PVRSRVDevicePostClockSpeedChange(ui32DevIdx,FALSE,(IMG_VOID*)NULL);
			#endif

			/* need to check cur freq to avoid freq changed not sync by themal protetion
				 and is switch ready before write back to RGX */
			ui32CurFreq = mt_gpufreq_get_cur_freq();

			MtkDVFSWriteBackFreqToRGX(ui32CurFreq);
		}
		else
		{
			PVR_LOG(("[MFG_DVFS]PVRSRVDevicePreClockSpeedChange failed!"));
		}
	}

	return;

}


static IMG_UINT32  MtkDVFSLoadingEstimated(IMG_VOID)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_DEVICE_NODE* psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[g_ui32DevIdx];
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_GPU_UTIL_STATS sGpuUtilStats = {0};

	if(NULL == psDevInfo->pfnGetGpuUtilStats)
	{
		PVR_LOG(("[MFG_DVFS]pfnGetGpuUtilStats is NULL"));
		return MTK_RGX_DVFS_LOAD_INVALID;
	}

	sGpuUtilStats = psDevInfo->pfnGetGpuUtilStats(psDeviceNode);

	if(sGpuUtilStats.bPoweredOn)
	{
		//PVR_LOG(("[MFG_DVFS]ui32GpuStatActive %d",sGpuUtilStats.ui32GpuStatActive ));
		return  (sGpuUtilStats.ui32GpuStatActive)/100;
	}
	else
	{
		//PVR_LOG(("[MFG_DVFS]sGpuUtilStats.bValid %d",sGpuUtilStats.bValid ));
		return MTK_RGX_DVFS_LOAD_INVALID;
	}

}


static IMG_BOOL MtkDVFSCheckDevIsPowerDown(IMG_VOID)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	if( PVRSRV_SYS_POWER_STATE_OFF == psPVRSRVData->eCurrentPowerState)
	{
		PVR_LOG(("[MFG_DVFS]psPVRSRVData->eCurrentPowerState OFF"));
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}

}

static IMG_VOID MtkDVFSTimerFuncCB(IMG_VOID *pDVFS)
{

	IMG_UINT32 ui32Load = 0;

	if(MTK_RGX_DEVICE_INDEX_INVALID == g_ui32DevIdx)
	{
		g_ui32DevIdx = MtkDVFSGetDevIdx();
	}

	if(MtkDVFSCheckDevIsPowerDown())
	{
		PVR_LOG(("[MFG_DVFS]MtkDVFSCheckDevIsPowerDown"));
		return;
	}

	ui32Load = MtkDVFSLoadingEstimated();

	if(MTK_RGX_DVFS_LOAD_INVALID == ui32Load)
	{
		return;
	}
	else
	{
		mt_gpufreq_set_cur_load(ui32Load);
		MtkCheckSwitchDVFS(ui32Load);
	}

	return;

}


#if MTK_DVFS_CREATE_THREAD
static IMG_VOID MTKDVFSThread(IMG_PVOID pvData)
{
	MTK_DVFS_T *psMTKDVFS = pvData;

	PVRSRV_ERROR	eError = PVRSRV_OK;

	IMG_INT32 *pDVFS = OSAllocZMem(sizeof(IMG_INT32));
	if (!pDVFS)
	{
		PVR_LOG(("[MFG_DVFS]OSAllocZMem failed"));
	}

	gpsMTKDVFS->hMtkDVFSTimer = OSAddTimer(MtkDVFSTimerFuncCB, (IMG_VOID *)pDVFS, MTK_DVFS_SWITCH_INTERVAL);
	if (!gpsMTKDVFS->hMtkDVFSTimer)
	{
		PVR_LOG(("[MFG_DVFS]OSAddTimer failed"));
	}

	eError = OSEnableTimer(gpsMTKDVFS->hMtkDVFSTimer);

	PVR_DPF_RETURN_RC(eError);


	return;


}
#else
static IMG_VOID MtkDVFSTimerSetup(IMG_VOID)
{
	PVRSRV_ERROR	eError = PVRSRV_OK;

	IMG_INT32 *pDVFS = OSAllocZMem(sizeof(IMG_INT32));
	if (!pDVFS)
	{
		PVR_LOG(("[MFG_DVFS]OSAllocZMem failed"));
	}

	gpsMTKDVFS->hMtkDVFSTimer = OSAddTimer(MtkDVFSTimerFuncCB, (IMG_VOID *)pDVFS, MTK_DVFS_SWITCH_INTERVAL);
	if (!gpsMTKDVFS->hMtkDVFSTimer)
	{
		PVR_LOG(("[MFG_DVFS]OSAddTimer failed"));
	}

	eError = OSEnableTimer(gpsMTKDVFS->hMtkDVFSTimer);

	return;


}

#endif

PVRSRV_ERROR MtkDVFSInit(IMG_VOID)
{
	PVRSRV_ERROR	eError = PVRSRV_OK;

//	PVR_LOG(("[MFG_DVFS]MtkDVFSInit..."));

	gpsMTKDVFS = OSAllocMem(sizeof(*gpsMTKDVFS));
	if (gpsMTKDVFS == IMG_NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
	OSMemSet(gpsMTKDVFS, 0, sizeof(*gpsMTKDVFS));

	#if MTK_DVFS_CREATE_THREAD
	/* Create a thread which is used to do DVFS switch */
	eError = OSThreadCreate(&gpsMTKDVFS->hMtkDVFSThread,
							"MTK_DVFS",
							MTKDVFSThread,
							gpsMTKDVFS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"MtkDVFSInit: Failed to create MTKDVFSThread"));
	}
	#else
	MtkDVFSTimerSetup();
	#endif

	return eError;
}


PVRSRV_ERROR MtkDVFSDeInit(IMG_VOID)
{
	PVRSRV_ERROR	eError = PVRSRV_OK;
//	PVR_LOG(("[MFG_DVFS]MtkDVFSDeInit..."));


	#if MTK_DVFS_CREATE_THREAD
	/* Stop and cleanup MTK DVFS thread */
	if (gpsMTKDVFS->hMtkDVFSThread)
	{
		OSThreadDestroy(gpsMTKDVFS->hMtkDVFSThread);
		gpsMTKDVFS->hMtkDVFSThread = IMG_NULL;
	}
	#endif

	if(gpsMTKDVFS->hMtkDVFSTimer)
	{
		/* Stop and clean up timer */
		eError = OSDisableTimer(gpsMTKDVFS->hMtkDVFSTimer);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF_RETURN_RC(PVRSRV_ERROR_UNABLE_TO_DISABLE_TIMER);
		}

		eError = OSRemoveTimer(gpsMTKDVFS->hMtkDVFSTimer);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF_RETURN_RC(PVRSRV_ERROR_UNABLE_TO_REMOVE_TIMER);
		}
		gpsMTKDVFS->hMtkDVFSTimer = IMG_NULL;

	}
	return eError;
}














