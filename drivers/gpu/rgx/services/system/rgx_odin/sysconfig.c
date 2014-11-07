/*************************************************************************/ /*!
@File
@Title          System Configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System Configuration functions
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include "pvrsrv_device.h"
#include "syscommon.h"
#include "sysconfig.h"
#include "physheap.h"
#if defined(SUPPORT_ION)
#include "ion_support.h"
#endif

#include "pvrsrv.h"
#include <linux/odin_pd.h>
#include <linux/odin_pmic.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/odin_gpufreq.h>

#define RGX_HW_SYSTEM_NAME "RGX_ODIN"
#define RGX_BYPASS_DIFF 4500
#define RGX_BYPASS_UTIL 9900
#define RGX_BUMPUP_MIN_SCALE 2

static RGX_TIMING_INFORMATION	gsRGXTimingInfo;
static RGX_DATA			gsRGXData;
static PVRSRV_DEVICE_CONFIG 	gsDevices[1];
static PVRSRV_SYSTEM_CONFIG 	gsSysConfig;

static PHYS_HEAP_FUNCTIONS	gsPhysHeapFuncs;
static PHYS_HEAP_CONFIG		gsPhysHeapConfig[1];

#if defined (RGX_DVFS)
extern u64 odin_current_gpu_bandwidth(void); // get GPU memory usage in bytes per 20milisecond

IMG_BOOL gbClimbHill = IMG_FALSE;
IMG_UINT32 gui32ClimbCount = 0;
IMG_BOOL gbDelayedChange = IMG_FALSE;;
IMG_UINT32 gui32DelayedCoreclk = 0;
IMG_UINT32 gui32DelayedMemclk = 0;
IMG_UINT32 gui32DelayedIdx = 0;
#endif


#if defined (RGX_DVFS)
enum ID_MEMPORT {
	MPID_0 = 0,
	MPID_1,
	MPID_MAX
};

enum ID_CORE {
	CRID_0 = 0,
	CRID_1,
	CRID_2,
	CRID_3,
	CRID_4,
	CRID_5,
	CRID_MAX
};
#endif

/*
	CPU to Device physcial address translation
*/
static
IMG_VOID UMAPhysHeapCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
									   IMG_DEV_PHYADDR *psDevPAddr,
									   IMG_CPU_PHYADDR *psCpuPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	psDevPAddr->uiAddr = psCpuPAddr->uiAddr;
}

/*
	Device to CPU physcial address translation
*/
static
IMG_VOID UMAPhysHeapDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
									   IMG_CPU_PHYADDR *psCpuPAddr,
									   IMG_DEV_PHYADDR *psDevPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	psCpuPAddr->uiAddr = psDevPAddr->uiAddr;
}

/*
	SysCreateConfigData
*/
PVRSRV_ERROR SysCreateConfigData(IMG_UINT32 baseAddr, IMG_UINT32 baseSize, IMG_UINT32 irqNum, PVRSRV_SYSTEM_CONFIG **ppsSysConfig)
{
	IMG_UINT32 i = 0;

#if defined (RGX_DVFS)
	IMG_UINT32 ui32Remainder = 0;
	IMG_UINT32 ui32TempScore;

	IMG_UINT32 aui32MemportClk[RGX_MEMPORT_LEVEL] = {
		140000,
		200000,
		400000,
		900000
	};

	IMG_UINT32 aui32CoreClk[RGX_CORE_LEVEL] = {
		120000,
		180000,
		240000,
		300000,
		360000,
		420000,
		460000,
		900000
	};

	IMG_UINT32 aui32BWBound[RGX_CORE_LEVEL] = {
		1499,
		2063,
		2318,
		2632,
		2705,
		2898,
		2954,
		900000
	};

/*[Up_threshold], [Down_threshold]*/
	IMG_UINT32 sRgx_conservative_param[RGX_CORE_LEVEL][2] = {
		{7300,  0},
		{8500,  7000},
		{8700,  9200},
		{8700,  9200},
		{8700,  9200},
		{8700,  9200},
		{10000, 8000},
		{90000, 90000},
	};
#endif
	/*
	 * Setup information about physical memory heap(s) we have
	 */
	gsPhysHeapFuncs.pfnCpuPAddrToDevPAddr = UMAPhysHeapCpuPAddrToDevPAddr;
	gsPhysHeapFuncs.pfnDevPAddrToCpuPAddr = UMAPhysHeapDevPAddrToCpuPAddr;

	gsPhysHeapConfig[0].ui32PhysHeapID = 0;
	gsPhysHeapConfig[0].pszPDumpMemspaceName = "SYSMEM";
	gsPhysHeapConfig[0].eType = PHYS_HEAP_TYPE_UMA;
	gsPhysHeapConfig[0].psMemFuncs = &gsPhysHeapFuncs;
	gsPhysHeapConfig[0].hPrivData = IMG_NULL;

#if 0
	gsPhysHeapConfig[1].ui32PhysHeapID = 1;
	gsPhysHeapConfig[1].pszPDumpMemspaceName = "ION carveout";
	gsPhysHeapConfig[1].eType = PHYS_HEAP_TYPE_LMA;
	gsPhysHeapConfig[1].psMemFuncs = &gsPhysHeapFuncs;
	gsPhysHeapConfig[1].hPrivData = IMG_NULL;

#if defined(SUPPORT_ODIN_SMMU)
	gsPhysHeapConfig[1].sStartAddr = 0xBC000000;
	gsPhysHeapConfig[1].uiSize = 0x04000000;
#else
	gsPhysHeapConfig[1].sStartAddr = 0xC0000000;
	gsPhysHeapConfig[1].uiSize = 0x3B000000;
#endif
#endif

	gsSysConfig.pasPhysHeaps = &(gsPhysHeapConfig[0]);
	gsSysConfig.ui32PhysHeapCount = IMG_ARR_NUM_ELEMS(gsPhysHeapConfig);

	gsSysConfig.pui32BIFTilingHeapConfigs = gauiBIFTilingHeapXStrides;
	gsSysConfig.ui32BIFTilingHeapCount = IMG_ARR_NUM_ELEMS(gauiBIFTilingHeapXStrides);

	/*
	 * Setup RGX specific timing data
	 */
	gsRGXTimingInfo.ui32CoreClockSpeed        = RGX_HW_CORE_CLOCK_SPEED;
#if defined (RGX_DVFS)
	gsRGXTimingInfo.ui32MemClockSpeed         = RGX_HW_CORE_CLOCK_SPEED;
#endif
#if defined (RGX_APM_TLA)
	gsRGXTimingInfo.bEnableActivePM           = IMG_TRUE;
	gsRGXTimingInfo.ui32ActivePMLatencyms     = SYS_RGX_ACTIVE_POWER_LATENCY_MS;
#else
	gsRGXTimingInfo.bEnableActivePM           = IMG_FALSE;
	gsRGXTimingInfo.ui32ActivePMLatencyms     = 0;
#endif
#if defined (RGX_APM_RASCALDUST)
	gsRGXTimingInfo.bEnableRDPowIsland        = IMG_TRUE;
#else
	gsRGXTimingInfo.bEnableRDPowIsland        = IMG_FALSE;
#endif
#if defined (RGX_DVFS)
	gsRGXTimingInfo.bEnableDVFS			   = IMG_TRUE;
    gsRGXTimingInfo.ui32UtilTriggerTickms  = SYS_RGX_DVFS_SAMPLE_COUNT;
    gsRGXTimingInfo.ui32MinStayms = SYS_RGX_DVFS_STAY_MIN;
    gsRGXTimingInfo.ui32GpuMaxFreq = RGX_HW_CORE_MAX_CLOCK_SPEED;
    gsRGXTimingInfo.ui32GpuMinFreq = RGX_HW_CORE_MIN_CLOCK_SPEED;
    gsRGXTimingInfo.ui32GpuUtilHistory = 0;
    gsRGXTimingInfo.ui32GpuScaleIDHistory = 1; /**IMPORTANT* DVFS table index should same as real start clock*/

	/* Make DVFS table*/

	ui32TempScore = OSDivide64((IMG_UINT64)RGXFWIF_GPU_STATS_MAX_VALUE_OF_STATE * 10, RGX_CORE_LEVEL - 1, &ui32Remainder);
	ui32TempScore = OSDivide64((IMG_UINT64)ui32TempScore + 5, 10, &ui32Remainder);

	for (i=0;i < RGX_CORE_LEVEL;i++) {
		gsRGXTimingInfo.sRgx_conservative_table[i].ui32Score = ui32TempScore * i;

		gsRGXTimingInfo.sRgx_conservative_table[i].ui32CoreClk = aui32CoreClk[i];
		gsRGXTimingInfo.sRgx_conservative_table[i].ui32UpThreshold = sRgx_conservative_param[i][0];
		gsRGXTimingInfo.sRgx_conservative_table[i].ui32DownThreshold = sRgx_conservative_param[i][1];
		gsRGXTimingInfo.sRgx_conservative_table[i].ui32BWBound = aui32BWBound[i];
		gsRGXTimingInfo.ui32AvailableClockSpeed[i] = gsRGXTimingInfo.sRgx_conservative_table[i].ui32CoreClk * 1000;
	}

	/* Make compensation*/
	for (i=1;i < CRID_MAX;i++) {
		IMG_UINT ui32ClkStdGap, ui32ClkActualGap, ui32ClkWeight;
		IMG_UINT32 ui32MaxFreq, ui32MinFreq;

		ui32MaxFreq = OSDivide64((IMG_UINT64)gsRGXTimingInfo.ui32GpuMaxFreq, 10000, &ui32Remainder);
		ui32MinFreq = OSDivide64((IMG_UINT64)gsRGXTimingInfo.ui32GpuMinFreq, 10000, &ui32Remainder);

		ui32ClkStdGap = OSDivide64((IMG_UINT64)(ui32MaxFreq - ui32MinFreq), CRID_MAX, &ui32Remainder);
		ui32ClkActualGap = aui32CoreClk[i] - aui32CoreClk[i-1];
		ui32ClkWeight = OSDivide64((IMG_UINT64)(ui32ClkActualGap * 10), ui32ClkStdGap, &ui32Remainder);

		gsRGXTimingInfo.sRgx_conservative_table[i].ui32Score = (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[i - 1].ui32Score
																+ OSDivide64((IMG_UINT64)(ui32TempScore * ui32ClkWeight), 100, &ui32Remainder);
	}


	for (i=0;i < RGX_MEMPORT_LEVEL;i++) {
		gsRGXTimingInfo.sRgx_memport_table[i] = aui32MemportClk[i];
	}

	gbDelayedChange = IMG_FALSE;
	gui32DelayedCoreclk = RGX_HW_CORE_CLOCK_SPEED;
	gui32DelayedMemclk = RGX_HW_CORE_CLOCK_SPEED;
	gui32DelayedIdx = gsRGXTimingInfo.ui32GpuScaleIDHistory;
#else
	gsRGXTimingInfo.bEnableDVFS			   = IMG_FALSE;
	gsRGXTimingInfo.ui32UtilTriggerTickms  = 0;
#endif

	/*
	 *Setup RGX specific data
	 */
	gsRGXData.psRGXTimingInfo = &gsRGXTimingInfo;

	/*
	 * Setup RGX device
	 */
	gsDevices[0].eDeviceType            = PVRSRV_DEVICE_TYPE_RGX;
	gsDevices[0].pszName                = RGX_HW_SYSTEM_NAME;

	/* Device setup information */
	//                                            
#if 1//defined(CONFIG_OF)
	gsDevices[0].sRegsCpuPBase.uiAddr = baseAddr;
	gsDevices[0].ui32RegsSize = baseSize;
	gsDevices[0].ui32IRQ = irqNum;
#else
	gsDevices[0].sRegsCpuPBase.uiAddr   = 0x3A000000;
	gsDevices[0].ui32RegsSize           = 0x7FFFFF;
	gsDevices[0].ui32IRQ                = 220;
#endif
	gsDevices[0].bIRQIsShared           = IMG_FALSE;

	/* Device's physical heap IDs */
	gsDevices[0].aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL] = 0;
	gsDevices[0].aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL] = 0;

	/* No power management on no HW system */
	gsDevices[0].pfnPrePowerState       = SysPowerStateOff;
	gsDevices[0].pfnPostPowerState      = SysPowerStateOn;

#if defined (RGX_DVFS)
		/*                  */
	gsDevices[0].pfnClockSpeedTarget	= SysClockSpeedGovernor;
	gsDevices[0].pfnClockSpeedChange 	= SysClockSpeedChanger;
#endif

	/* No clock frequency either */
	gsDevices[0].pfnClockFreqGet        = HwRGXClockFreq;

	/* No interrupt handled either */
	gsDevices[0].pfnInterruptHandled    = IMG_NULL;

	gsDevices[0].pfnCheckMemAllocSize   = SysCheckMemAllocSize;

	gsDevices[0].hDevData               = &gsRGXData;

	/*
	 * Setup system config
	 */
	gsSysConfig.pszSystemName = RGX_HW_SYSTEM_NAME;
	gsSysConfig.uiDeviceCount = sizeof(gsDevices)/sizeof(gsDevices[0]);
	gsSysConfig.pasDevices = &gsDevices[0];

	/* No power management on no HW system */
	gsSysConfig.pfnSysPrePowerState = IMG_NULL;
	gsSysConfig.pfnSysPostPowerState = IMG_NULL;

	/* no cache snooping */
	gsSysConfig.eCacheSnoopingMode = PVRSRV_SYSTEM_SNOOP_NONE;

	/* Setup other system specific stuff */
#if defined(SUPPORT_ION) && !defined(SUPPORT_ODIN_ION)
	IonInit(NULL);
#endif

	*ppsSysConfig = &gsSysConfig;

	return PVRSRV_OK;
}

/*
	SysDestroyConfigData
*/
IMG_VOID SysDestroyConfigData(PVRSRV_SYSTEM_CONFIG *psSysConfig)
{
	PVR_UNREFERENCED_PARAMETER(psSysConfig);

#if defined(SUPPORT_ION) && !defined(SUPPORT_ODIN_ION)
	IonDeinit();
#endif
}

static IMG_UINT32 HwRGXClockFreq(IMG_HANDLE hSysData)
{
	PVRSRV_DEVICE_CONFIG *psDevConfig = &gsSysConfig.pasDevices[0];
	RGX_DATA *psRGXData = (RGX_DATA *)psDevConfig->hDevData;

	return psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;

}
PVRSRV_ERROR SysDebugInfo(PVRSRV_SYSTEM_CONFIG *psSysConfig)
{
	PVR_UNREFERENCED_PARAMETER(psSysConfig);



	return PVRSRV_OK;
}

/*
	SysClockSpeedTarget
*/
#if defined (RGX_DVFS)
static
PVRSRV_ERROR IMG_CALLCONV SysClockSpeedGovernor(IMG_UINT32 ui32GpuUtilActive, IMG_UINT32 ui32GpuUtilIdle, IMG_UINT32 *ui32TargetClockSpeed)
{

	IMG_UINT32 ui32Score = 0;
	IMG_UINT32 ui32HistoryDnDiff = 0;
	IMG_UINT32 ui32HistoryUpDiff = 0;
	PVRSRV_DATA		*psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_DEVICE_NODE  *psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[0];
	PVRSRV_RGXDEV_INFO	 *psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_DEVICE_CONFIG *psDevConfig = &gsSysConfig.pasDevices[0];
	RGX_DATA *psRGXData = (RGX_DATA *)psDevConfig->hDevData;
	IMG_UINT32 ui32Remainder = 0;
	IMG_UINT32 ui32CoreSpeed = 0;
	int i = 0;
	IMG_BOOL bClimbHill = IMG_FALSE;
	IMG_BOOL bDownHill = IMG_FALSE;

	/*Normalize Scale*/
	IMG_UINT32 ui32ActNormalize = 0;
	IMG_UINT32 ui32IdleNormalize = 0;

	/*BandWidth relative Scale*/
	IMG_UINT32 ui32CCIPortBW = 0;
	IMG_UINT32 ui32CCIPortBWdiff = 0;
	IMG_UINT32 ui32CCIPortBWHistory = 0;
	IMG_UINT32 ui32CCIPortActiveBWHistory = 0;
	IMG_UINT32 ui32BWGap, ui32BWGapMin;
	IMG_BOOL bAdaptive = IMG_FALSE;
	IMG_BOOL bReset = IMG_FALSE;

    /*Differential Scale*/
	IMG_UINT32 ui32ActDiff = 0;
	IMG_UINT32 ui32IdleDiff = 0;
	IMG_UINT32 ui32ActHistory = 0;
	IMG_UINT32 ui32ScaleID = psRGXData->psRGXTimingInfo->ui32GpuScaleIDHistory;

	/*Min Max Freq Throttling*/
	IMG_UINT32 ui32MaxFreq = OSDivide64((IMG_UINT64)(IMG_UINT32)psRGXData->psRGXTimingInfo->ui32GpuMaxFreq, 1000, &ui32Remainder);
	IMG_UINT32 ui32MinFreq = OSDivide64((IMG_UINT64)(IMG_UINT32)psRGXData->psRGXTimingInfo->ui32GpuMinFreq, 1000, &ui32Remainder);


	PVR_DPF((PVR_DBG_MESSAGE, "ENTER : SysClockSpeedGovernor"));

	/*Normalize Scale*/
	ui32ActNormalize = ui32GpuUtilActive;
	ui32IdleNormalize = ui32GpuUtilIdle;

	ui32CCIPortBW = odin_current_gpu_bandwidth() >> 20;

	/*For Bump up situation
	* Caculate utilization difference with past
	*/
	if (psRGXData->psRGXTimingInfo->ui32GpuUtilHistory < ui32GpuUtilActive)
	{
		ui32HistoryUpDiff = ui32GpuUtilActive - psRGXData->psRGXTimingInfo->ui32GpuUtilHistory;
		ui32HistoryUpDiff = ui32HistoryUpDiff * (gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID + 1].ui32Score - gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32Score);
		ui32HistoryUpDiff = OSDivide64((IMG_UINT64)ui32HistoryUpDiff, 1000, &ui32Remainder);

	}else if (psRGXData->psRGXTimingInfo->ui32GpuUtilHistory > ui32GpuUtilActive){

		ui32HistoryDnDiff = psRGXData->psRGXTimingInfo->ui32GpuUtilHistory - ui32GpuUtilActive;
		ui32HistoryDnDiff = ui32HistoryDnDiff * (gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32Score - gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID - 1].ui32Score);
		ui32HistoryDnDiff = OSDivide64((IMG_UINT64)ui32HistoryDnDiff, 1000, &ui32Remainder);

	}

	/*Store current utilization*/
	psRGXData->psRGXTimingInfo->ui32GpuUtilHistory = ui32ActNormalize;

Conclusion :
	/*Get past scaled score*/
	ui32Score = (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32Score;

	/*Caculate GPU utilization difference with threshold*/
	if (ui32ActNormalize > (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32UpThreshold)
	{
		ui32ActDiff = ui32ActNormalize - (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32UpThreshold;
		ui32ActDiff += OSDivide64((IMG_UINT64)ui32ActDiff, 2, &ui32Remainder);
		bClimbHill = IMG_TRUE;

	}else if (ui32ActNormalize < (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32DownThreshold)
	{
		if (ui32IdleNormalize > (RGXFWIF_GPU_STATS_MAX_VALUE_OF_STATE - (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32DownThreshold))
			ui32IdleDiff = ui32IdleNormalize - (RGXFWIF_GPU_STATS_MAX_VALUE_OF_STATE - (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32DownThreshold);
		else
			ui32IdleDiff = (RGXFWIF_GPU_STATS_MAX_VALUE_OF_STATE - (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32DownThreshold) - ui32IdleNormalize;
		ui32IdleDiff += OSDivide64((IMG_UINT64)ui32IdleDiff, 2, &ui32Remainder);
		bDownHill = IMG_TRUE;

	}else{
		goto Next_Phase_BW;
	}

Calculation :
	/*Caculate score*/
	if (bClimbHill)
	{
		if ((gui32ClimbCount * SYS_RGX_DVFS_SAMPLE_COUNT) < (SYS_RGX_DVFS_STAY_MIN * ui32ScaleID))
		{
			if ((ui32HistoryUpDiff < RGX_BYPASS_DIFF) && (ui32ActNormalize < RGX_BYPASS_UTIL))
			{
				gui32ClimbCount++;
				bClimbHill = IMG_FALSE;
				goto Next_Phase_BW;
			}
		}

		/*clock up */
		if (ui32ActDiff < ui32HistoryUpDiff)
		{
			ui32Score += ui32HistoryUpDiff;

		}else{
			/*Check Bump-up*/
			if (ui32ActNormalize > RGX_BYPASS_UTIL)
				ui32Score += ui32ActDiff + (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[RGX_BUMPUP_MIN_SCALE].ui32Score;
			else
				ui32Score += ui32ActDiff;
		}

		/*Prevent exceed maximum value*/
		if (ui32Score > (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[RGX_CORE_LEVEL - 1].ui32Score)
			ui32Score = (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[RGX_CORE_LEVEL - 1].ui32Score - 1;

		gui32ClimbCount = 0;

	}else if (bDownHill)
	{
		/*clock down*/
		if (ui32Score > ui32IdleDiff)
		{
			if (ui32IdleDiff > ui32HistoryDnDiff)
				ui32Score -= ui32IdleDiff;
			else
				ui32Score -= ui32HistoryDnDiff;

		}else{

			if (ui32Score > (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[RGX_BUMPUP_MIN_SCALE].ui32Score)
			{
				ui32Score = (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID - RGX_BUMPUP_MIN_SCALE].ui32Score;
			}else{

				ui32Score = 0;
			}
		}
		gui32ClimbCount--;
	}

Phase_BW :
	/*Memory stall detecting*/
	ui32CCIPortBWHistory = psRGXData->psRGXTimingInfo->ui32GpuCCIBWHistory;
	ui32CCIPortActiveBWHistory = psRGXData->psRGXTimingInfo->ui32GpuActiveCCIBWHistory;
#if CCIBW_MONITORING
	if (gbClimbHill)
	{
		gbClimbHill = IMG_FALSE;

		if (ui32CCIPortBW < OSDivide64((IMG_UINT64)(gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32BWBound * 39), 100, &ui32Remainder)
			&& (ui32ActNormalize > 9200))
		{

			bAdaptive = IMG_TRUE;

			if (ui32CCIPortBW > ui32CCIPortActiveBWHistory)
			{

				ui32BWGap = (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32BWBound
						- (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID - 1].ui32BWBound;
				ui32BWGapMin = OSDivide64((IMG_UINT64)(ui32BWGap * 20), 100, &ui32Remainder);

				ui32CCIPortBWdiff = ui32CCIPortBW - ui32CCIPortActiveBWHistory;

				if (ui32CCIPortBWdiff >= ui32BWGapMin)
					bAdaptive = IMG_FALSE;

			}
		}
	}
#if 1 /*Need re-think*/
	/*Check difference from previous BW for Pull up frequency*/
    if (bClimbHill)
    {
		bClimbHill = IMG_FALSE;

		if((ui32CCIPortBW < OSDivide64((IMG_UINT64)(gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32BWBound * 39), 100, &ui32Remainder))
			&& (ui32Score > (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID + 1].ui32Score)
			&& (ui32ActNormalize > 9200))
	    {
			bReset = IMG_TRUE;

			if(ui32CCIPortBW > ui32CCIPortBWHistory)
			{

				ui32BWGap = (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID + 1].ui32BWBound
						- (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32BWBound;
				ui32BWGapMin = OSDivide64((IMG_UINT64)(ui32BWGap * 20), 100, &ui32Remainder);

				ui32CCIPortBWdiff = ui32CCIPortBW - ui32CCIPortBWHistory;

				if(ui32CCIPortBWdiff >= ui32BWGapMin)
					bReset = IMG_FALSE;

			}
	    }
    }

#endif
#endif
Next_Phase_BW :
	/*Save CCI GPU port Bandwidth */
	psRGXData->psRGXTimingInfo->ui32GpuCCIBWHistory = ui32CCIPortBW;
#if CCIBW_MONITORING
	/*Reset capacity change*/
	if (bReset)
	{
		ui32Score = (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID].ui32Score;

		bReset = IMG_FALSE;
	}

	/*Pull down frequency*/
	if (bAdaptive)
	{
		 if (ui32ScaleID != 0)
		 {
			IMG_UINT ui32TempScore = (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[ui32ScaleID - 1].ui32Score;

			if(ui32Score > ui32TempScore)
			{
				ui32Score = ui32TempScore;
			}
		 }

		 bAdaptive = IMG_FALSE;
	}
#endif
Follow_thru :
	/*Find Frequency for measured capacity*/
	for(i = CRID_0; i <= CRID_MAX; i++)
	{
		if((ui32Score >= (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[i].ui32Score) &&
			(ui32Score < (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[i+1].ui32Score))
		{
			ui32CoreSpeed = (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[i].ui32CoreClk;
			break;
		}
	}

	/*Check throttling*/
	if (ui32MaxFreq < ui32CoreSpeed)
	{
		ui32CoreSpeed = ui32MaxFreq;
	}else if (ui32MinFreq > ui32CoreSpeed)
	{
		ui32CoreSpeed = ui32MinFreq;
	}

	/*Find ID for determined frequency*/
	for(i=CRID_0; i <= CRID_MAX; i++)
	{
		if((ui32CoreSpeed >= (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[i].ui32CoreClk) &&
			(ui32CoreSpeed < (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[i+1].ui32CoreClk))
		{
			ui32CoreSpeed = (IMG_UINT32)gsRGXTimingInfo.sRgx_conservative_table[i].ui32CoreClk;
			psRGXData->psRGXTimingInfo->ui32GpuScaleIDHistory = i;
			break;
		}
	}

	if (ui32CoreSpeed * 1000 == psRGXData->psRGXTimingInfo->ui32CoreClockSpeed)
		goto Exit_out;

	/*Save CCI GPU port Bandwidth when only frequency changed */
	psRGXData->psRGXTimingInfo->ui32GpuActiveCCIBWHistory = ui32CCIPortBW;

	/*Send back normalized DVFS frequency*/
	*ui32TargetClockSpeed = ui32CoreSpeed;


	return PVRSRV_OK;

Exit_out:

	return IMG_TRUE;

}
#endif

static BLOCKING_NOTIFIER_HEAD(odin_gpufreq_notifier);

int odin_gpufreq_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&odin_gpufreq_notifier, nb);
}

EXPORT_SYMBOL(odin_gpufreq_register_notifier);

int odin_gpufreq_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&odin_gpufreq_notifier, nb);
}

EXPORT_SYMBOL(odin_gpufreq_unregister_notifier);


/*
	SysClockSpeedChange
*/
#if defined (RGX_DVFS)
static
PVRSRV_ERROR IMG_CALLCONV SysClockSpeedChanger(IMG_UINT32 ui32TargetClockSpeed, IMG_BOOL bIdleDevice)
{
	struct clk *pCoreClk = clk_get(IMG_NULL, "gpu_core_clk");
	struct clk *pMemClk = clk_get(IMG_NULL, "gpu_mem_clk");

	PVRSRV_DEVICE_CONFIG *psDevConfig = &gsSysConfig.pasDevices[0];
	RGX_DATA *psRGXData = (RGX_DATA *)psDevConfig->hDevData;

	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_DEVICE_NODE*  psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[0];
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;

	IMG_UINT32 ui32CoreFreq = (IMG_UINT32)ui32TargetClockSpeed;
	IMG_UINT32 ui32MemFreq = 0;
	int i;

	for(i=MPID_0; i < RGX_MEMPORT_LEVEL; i++)
	{
		if((ui32CoreFreq <= (IMG_UINT32)gsRGXTimingInfo.sRgx_memport_table[i]))
		{
			if (i <= MPID_MAX)
			{
				ui32MemFreq = (IMG_UINT32)gsRGXTimingInfo.sRgx_memport_table[i];
				break;
			}else{
				ui32MemFreq = (IMG_UINT32)gsRGXTimingInfo.sRgx_memport_table[i-1];
			}

		}
	}

	/*Check GPU PD0 alive and already reached same freq level before*/
	if ((psDevInfo->psRGXFWIfTraceBuf->ePowState != RGXFWIF_POW_OFF)
		&& (psRGXData->psRGXTimingInfo->ui32CoreClockSpeed != ui32CoreFreq * 1000))
	{

		struct odin_gpufreq_freqs freqs;

		freqs.max = (IMG_UINT32)psRGXData->psRGXTimingInfo->ui32GpuMaxFreq;
		freqs.old = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;
		freqs.new = ui32CoreFreq;
		freqs.util = psRGXData->psRGXTimingInfo->ui32GpuUtilHistory;

		if(psRGXData->psRGXTimingInfo->ui32CoreClockSpeed < ui32CoreFreq * 1000)
			gbClimbHill = IMG_TRUE;
		else
			gbClimbHill = IMG_FALSE;

		/*Store current DVFS clock speed*/
		psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32CoreFreq * 1000;
		psRGXData->psRGXTimingInfo->ui32MemClockSpeed = ui32MemFreq * 1000;

		blocking_notifier_call_chain(&odin_gpufreq_notifier, ODIN_GPUFREQ_PRECHANGE, &freqs);


		/* DO call Linux Kernel common clock framework API */
		clk_set_rate(pMemClk, ui32MemFreq*1000);
		clk_set_rate(pCoreClk, ui32CoreFreq*1000);


		blocking_notifier_call_chain(&odin_gpufreq_notifier, ODIN_GPUFREQ_POSTCHANGE, &freqs);

	}else
	{
			gbDelayedChange = IMG_FALSE;
			gui32DelayedCoreclk = ui32CoreFreq;
			gui32DelayedMemclk = ui32MemFreq;
			gui32DelayedIdx = psRGXData->psRGXTimingInfo->ui32GpuScaleIDHistory;
			gbDelayedChange = IMG_TRUE;
	}

	if (bIdleDevice)
	{
		/* TO-DO : Add HW control code if necessary */
	}

	return PVRSRV_OK;
}
#endif
/*
	SysPrePowerState
*/
static
PVRSRV_ERROR IMG_CALLCONV SysPowerStateOff(PVRSRV_DEV_POWER_STATE eNewPowerState, PVRSRV_DEV_POWER_STATE eCurrentPowerState, IMG_BOOL bForced)
{

	struct odin_gpufreq_freqs freqs;
	PVRSRV_DEVICE_CONFIG *psDevConfig = &gsSysConfig.pasDevices[0];
	RGX_DATA *psRGXData = (RGX_DATA *)psDevConfig->hDevData;
	IMG_UINT eError;

	if ((eNewPowerState != eCurrentPowerState) &&
		(eNewPowerState != PVRSRV_DEV_POWER_STATE_ON))
	{
#if defined (RGX_DVFS)
		freqs.max = psRGXData->psRGXTimingInfo->ui32GpuMaxFreq;
		freqs.old = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;
		freqs.new = 0;

		blocking_notifier_call_chain(&odin_gpufreq_notifier, ODIN_GPUFREQ_PRECHANGE, &freqs);
#endif
		/*Send Mailbox API for PD control*/
		eError = odin_pd_off(PD_GPU, 0);
#if defined (RGX_DVFS)
		blocking_notifier_call_chain(&odin_gpufreq_notifier, ODIN_GPUFREQ_POSTCHANGE, &freqs);
#endif
		if (eError < PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"Mailbox error returned in Power Off"));
			return IMG_TRUE;
		}
	}

	return PVRSRV_OK;
}

/*
	SysPostPowerState
*/
static
PVRSRV_ERROR IMG_CALLCONV SysPowerStateOn(PVRSRV_DEV_POWER_STATE eNewPowerState, PVRSRV_DEV_POWER_STATE eCurrentPowerState, IMG_BOOL bForced)
{
	struct odin_gpufreq_freqs freqs;
	PVRSRV_DEVICE_CONFIG *psDevConfig = &gsSysConfig.pasDevices[0];
	RGX_DATA *psRGXData = (RGX_DATA *)psDevConfig->hDevData;
	IMG_UINT eError;

	if ((eNewPowerState != eCurrentPowerState) &&
		(eCurrentPowerState != PVRSRV_DEV_POWER_STATE_ON))
	{
#if defined (RGX_DVFS)
		freqs.max = psRGXData->psRGXTimingInfo->ui32GpuMaxFreq;
		freqs.old = 0;
		if (gbDelayedChange)
			freqs.new = gui32DelayedCoreclk * 1000;
		else
			freqs.new = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;

		blocking_notifier_call_chain(&odin_gpufreq_notifier, ODIN_GPUFREQ_PRECHANGE, &freqs);
#endif
		/*Send Mailbox API for PD control*/
		PVR_DPF((PVR_DBG_MESSAGE,"A O"));
		eError = odin_pd_on(PD_GPU, 0);

		if (eError < PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"Mailbox error returned in Power On"));
			return IMG_TRUE;
		}
#if defined (RGX_DVFS)
		if (gbDelayedChange)
		{
			struct clk *pCoreClk = clk_get(IMG_NULL, "gpu_core_clk");
			struct clk *pMemClk = clk_get(IMG_NULL, "gpu_mem_clk");

			if(psRGXData->psRGXTimingInfo->ui32CoreClockSpeed < gui32DelayedCoreclk * 1000)
				gbClimbHill = IMG_TRUE;
			else
				gbClimbHill = IMG_FALSE;


			/* DO call Linux Kernel common clock framework API */
			clk_set_rate(pMemClk, gui32DelayedMemclk*1000);
			clk_set_rate(pCoreClk, gui32DelayedCoreclk*1000);

			/*Store current DVFS clock speed*/
			psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = gui32DelayedCoreclk * 1000;
			psRGXData->psRGXTimingInfo->ui32MemClockSpeed = gui32DelayedMemclk * 1000;
			psRGXData->psRGXTimingInfo->ui32GpuScaleIDHistory = gui32DelayedIdx;

			gbDelayedChange = IMG_FALSE;
		}

		blocking_notifier_call_chain(&odin_gpufreq_notifier, ODIN_GPUFREQ_POSTCHANGE, &freqs);
#endif
	}

	return PVRSRV_OK;
}




/******************************************************************************
 End of file (sysconfig.c)
******************************************************************************/
