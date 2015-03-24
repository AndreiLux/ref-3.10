
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_mfgsys.c
 *
 * Project:
 * --------
 *   MT8135
 *
 * Description:
 * ------------
 *   Implementation interface between RGX DDK and kernel GPU DVFS module
 *
 * Author:
 * -------
 *   Enzhu Wang
 *
 *============================================================================
 * History and modification
 *1. Initial @ April 27th 2013
 *2. Add  Enable/Disable MFG sytem API, Enable/Disable MFG clock API @May 20th 2013
 *3. Add MTKDevPrePowerState/MTKDevPostPowerState/MTKSystemPrePowerState/MTKSystemPostPowerState @ May 29th 2013
 *4. Move some interface to mtk_mfgdvfs.c @ July 10th 2013
 *5. E2 above IC version support HW APM feature @ Sep 17th 2013
 *============================================================================
 ****************************************************************************/

#include <linux/delay.h>

#include "mtk_mfgsys.h"
#include "mach/mt_gpufreq.h"
#include "mach/mt_clkmgr.h"
#include "mach/mt_typedefs.h"
/* #include "mach/upmu_common.h" */
#include "pvrsrv_error.h"
#include "mutex.h"
#include "mtk_mfgdvfs.h"
#include "mach/mt_boot.h"

/* eTblType declare for DVFS support or not and which level be defined */
static FREQ_TABLE_DVFS_TYPE eTblType = TBL_TYPE_DVFS_2_LEVEL;
/* static FREQ_TABLE_DVFS_TYPE eTblType = TBL_TYPE_DVFS_NOT_SUPPORT; */


int MtkEnableMfgClock(void)
{
	enable_clock(MT_CG_MFG_BAXI_PDN, "MFG");
	enable_clock(MT_CG_MFG_BMEM_PDN, "MFG");
	enable_clock(MT_CG_MFG_BG3D_PDN, "MFG");
	enable_clock(MT_CG_MFG_B26M_PDN, "MFG");
	mtk_mfg_debug("MtkEnableMfgClock  end...\n");
	return PVRSRV_OK;

}

int MtkDisableMfgClock(void)
{
	disable_clock(MT_CG_MFG_BAXI_PDN, "MFG");
	disable_clock(MT_CG_MFG_BMEM_PDN, "MFG");
	disable_clock(MT_CG_MFG_BG3D_PDN, "MFG");
	disable_clock(MT_CG_MFG_B26M_PDN, "MFG");

	mtk_mfg_debug("MtkDisableMfgClock  end...\n");
	return PVRSRV_OK;
}


static int MTKMFGClockPowerOn(void)
{
	#if MTK_PM_SUPPORT
	#else
	DRV_WriteReg32(MFG_PDN_RESET, 0x1);
	DRV_WriteReg32(MFG_PDN_CLR, 0xf);
	DRV_WriteReg32(MFG_PDN_SET, 0xf);
	DRV_WriteReg32(MFG_PDN_RESET, 0x0);
	DRV_WriteReg32(MFG_PDN_CLR, 0xf);
	#endif

	#if 0//RD_POWER_ISLAND
	//MtkEnableMfgClock();
	#endif

	mtk_mfg_debug("MFG_PDN_STATUS: 0x%X\n",DRV_Reg32(MFG_PDN_STATUS));

	//DRAM access path: need configuration for each power-up
	// 0x1 means G3D select Dram access through CCI-400
	DRV_WriteReg32(MFG_DRAM_ACCESS_PATH, 0x00000000);

	return PVRSRV_OK;

}


static int MtkEnableMfgSystem(void)
{
	enable_subsys(SYS_MFG_2D, "MFG_2D");
	enable_subsys(SYS_MFG, "MFG");
	mtk_mfg_debug("enable_subsys  end...\n");

	return PVRSRV_OK;
}

static int MtkDisableMfgSystem(void)
{

	//enable_clock(MT_CG_MFG_BMEM_PDN, "MFG");
	//enable MFG MEM, or disable subsys failed
	/* disable subsystem first before disable clock */
	disable_subsys(SYS_MFG, "MFG");
	disable_subsys(SYS_MFG_2D, "MFG_2D");
	mtk_mfg_debug("disable_subsys end...\n");
	return PVRSRV_OK;
}

static int MTKEnableHWAPM(void)
{
	mtk_mfg_debug("[MFG]MTKEnableHWAPM...\n");
	DRV_WriteReg32(0xf0206024, 0x8007050f);
	DRV_WriteReg32(0xf0206028, 0x0e0c0a09);
	return PVRSRV_OK;
}


static int MTKDisableHWAPM(void)
{
	mtk_mfg_debug("[MFG]MTKDisableHWAPM...\n");
	DRV_WriteReg32(0xf0206024, 0x0);
	DRV_WriteReg32(0xf0206028, 0x0);
	return PVRSRV_OK;
}

bool MTKMFGIsE2andAboveVersion()
{
	CHIP_SW_VER ver = 0;
	ver = mt_get_chip_sw_ver();
	if(CHIP_SW_VER_02 <= ver)
	{
		//PVR_LOG(("[MFG]MTKMFGIsE2andAboveVersion!"));
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


static PVRSRV_LINUX_MUTEX g_DevPreMutex;
static PVRSRV_LINUX_MUTEX g_DevPostMutex;

int MTKMFGSystemInit(void)
{
	MtkEnableMfgSystem();

	MTKMFGClockPowerOn();
	MTKSetFreqInfo(eTblType);

	LinuxInitMutex(&g_DevPreMutex);
	LinuxInitMutex(&g_DevPostMutex);

	if(MTKMFGIsE2andAboveVersion())
	{
		MTKEnableHWAPM();
	}

	//GPU mem clock freerun, HW control, power saving.
	DRV_WriteReg32(0xf0206010,DRV_Reg32(0xf0006610)|0x20000000);
	mtk_mfg_debug("[MFG]0xf0206010 is 0x%x\n", DRV_Reg32(0xf0006610));


	/*DVFS  feature entry */
	if(TBL_TYPE_DVFS_NOT_SUPPORT !=eTblType)
	{
		//MtkDVFSTimerSetup();
		MtkDVFSInit();
	}

	mtk_mfg_debug("[MFG]MTKMFGSystemInit End \n");


	return PVRSRV_OK;
}

int MTKMFGSystemDeInit(void)
{
	if(TBL_TYPE_DVFS_NOT_SUPPORT !=eTblType)
	{
		MtkDVFSDeInit();
	}
	return PVRSRV_OK;
}

extern void mt_gpu_disable_vgpu(void);
extern void mt_gpu_vgpu_sel_1_15(void);

static PVRSRV_DEV_POWER_STATE g_eCurrPowerState = PVRSRV_DEV_POWER_STATE_DEFAULT;
static PVRSRV_DEV_POWER_STATE g_eNewPowerState = PVRSRV_DEV_POWER_STATE_DEFAULT;

PVRSRV_ERROR MTKSysDevPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
										 PVRSRV_DEV_POWER_STATE eCurrentPowerState,
										 IMG_BOOL bForced)

{

	mtk_mfg_debug("MTKSysDevPrePowerState (%d->%d)\n",eCurrentPowerState,eNewPowerState);
	LinuxLockMutex(&g_DevPreMutex);

	usleep_range(400, 500);
	if(PVRSRV_DEV_POWER_STATE_DEFAULT == g_eCurrPowerState
	 &&PVRSRV_DEV_POWER_STATE_DEFAULT == g_eNewPowerState)
	{
		MtkDisableMfgClock();
	}

	if(PVRSRV_DEV_POWER_STATE_OFF == eNewPowerState
	 &&PVRSRV_DEV_POWER_STATE_ON == eCurrentPowerState)
	{
		if(MTKMFGIsE2andAboveVersion())
		{
			MTKDisableHWAPM();
		}

		MtkDisableMfgSystem();
		MtkDisableMfgClock();

	}
	else if(PVRSRV_DEV_POWER_STATE_ON == eNewPowerState
		  &&PVRSRV_DEV_POWER_STATE_OFF == eCurrentPowerState)
	{

		MtkEnableMfgClock();
		MtkEnableMfgSystem();
		mtk_mfg_debug("[MFG_ON]0xf0206000: 0x%X\n",DRV_Reg32(0xf0206000));

		if(MTKMFGIsE2andAboveVersion())
		{
			MTKEnableHWAPM();
		}

	}
	else
	{
		mtk_mfg_debug("MTKSysDevPrePowerState do nothing!\n");

	}

	g_eCurrPowerState = eCurrentPowerState;
	g_eNewPowerState = eNewPowerState;

	usleep_range(400, 500);
	LinuxUnLockMutex(&g_DevPreMutex);

	return PVRSRV_OK;

}

PVRSRV_ERROR MTKSysDevPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
										  PVRSRV_DEV_POWER_STATE eCurrentPowerState,
										  IMG_BOOL bForced)
{
	/* Post power sequence move to PrePowerState */
	return PVRSRV_OK;

	mtk_mfg_debug("MTKSysDevPostPowerState (%d->%d)\n",eCurrentPowerState,eNewPowerState);

	LinuxLockMutex(&g_DevPostMutex);


	if(PVRSRV_DEV_POWER_STATE_ON == eNewPowerState
	 &&PVRSRV_DEV_POWER_STATE_OFF == eCurrentPowerState)
	{
		MtkEnableMfgClock();

	}
	else if(PVRSRV_DEV_POWER_STATE_OFF == eNewPowerState
		  &&PVRSRV_DEV_POWER_STATE_ON == eCurrentPowerState)
	{
		MtkDisableMfgClock();
		//clock_dump_info();

	}
	else
	{
		mtk_mfg_debug("MTKSysDevPostPowerState do nothing!\n");

	}

	LinuxUnLockMutex(&g_DevPostMutex);

	return PVRSRV_OK;

}

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{

	mtk_mfg_debug("MTKSystemPrePowerState eNewPowerState %d\n",eNewPowerState);

	if(PVRSRV_SYS_POWER_STATE_OFF == eNewPowerState)
	{
		#if 0//!MTK_PM_SUPPORT
		MtkDisableMfgSystem();
		#endif
		mt_gpu_disable_vgpu();
	}
	else
	{
		mtk_mfg_debug("MTKSystemPrePowerState do nothing!\n");
	}

	return PVRSRV_OK;

}
PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{

	mtk_mfg_debug("MTKSystemPostPowerState eNewPowerState %d\n",eNewPowerState);

	if(PVRSRV_SYS_POWER_STATE_ON == eNewPowerState)
	{

		#if 0//!MTK_PM_SUPPORT
		MtkEnableMfgSystem();
		#endif
		mt_gpu_vgpu_sel_1_15();
		/*udelay(200);*/
		usleep_range(180, 220);
	}
	else
	{
		mtk_mfg_debug("MTKSystemPostPowerState do nothing!\n");

	}

	return PVRSRV_OK;
}
















