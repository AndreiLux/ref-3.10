/*!
 *****************************************************************************
 *
 * @file	   sysdev_api_km.c
 *
 * This file contains the System Device Kernel Mode API.
 * ---------------------------------------------------------------------------
 *
 * Copyright (c) Imagination Technologies Ltd.
 * 
 * The contents of this file are subject to the MIT license as set out below.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE.
 * 
 * Alternatively, the contents of this file may be used under the terms of the 
 * GNU General Public License Version 2 ("GPL")in which case the provisions of
 * GPL are applicable instead of those above. 
 * 
 * If you wish to allow use of your version of this file only under the terms 
 * of GPL, and not to allow others to use your version of this file under the 
 * terms of the MIT license, indicate your decision by deleting the provisions 
 * above and replace them with the notice and other provisions required by GPL 
 * as set out in the file called “GPLHEADER” included in this distribution. If 
 * you do not delete the provisions above, a recipient may use your version of 
 * this file under the terms of either the MIT license or GPL.
 * 
 * This License is also included in this distribution in the file called 
 * "MIT_COPYING".
 *
 *****************************************************************************/

#include "system.h"

#include "sysdev_api_km.h"
#include "sysdev_utils.h"
#include "sysdev_utils1.h"
#include "sysos_api_km.h"
#include "sysbrg_utils.h"

#include <linux/module.h>
#include "target.h"

static atomic_t  gsActiveOpenCnt  = {.counter = 0 };    //!< Count of active open

/*!
******************************************************************************
 This structure contains device information.
******************************************************************************/
typedef struct
{
	IMG_UINT32			ui32DeviceId;			//!< The device ID.
	IMG_VOID *			pvCpuKmAddrMemBase;		//!< Kernel mode CPU address of device memory
	IMG_VOID *			pvCpuPhysAddrMemBase;	//!< CPU physical address of device memory
	IMG_UINT64			ui64MemSize;			//!< Size of device memory				
	IMG_UINT64			ui64DevMemBase;			//!< Base of device memory

} SYSDEVKM_sDevice;


/*!
******************************************************************************

 @Function				SYSDEVKM_Initialise

******************************************************************************/
IMG_RESULT SYSDEVKM_Initialise(IMG_VOID)
{
	IMG_UINT32				ui32Result;

	/* Initialise...*/
	ui32Result = SYSDEVU_Initialise();
	IMG_ASSERT(ui32Result == IMG_SUCCESS);
	if (ui32Result != IMG_SUCCESS)
	{
		return ui32Result;
	}

	return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function				SYSDEVKM_Deinitialise

******************************************************************************/
IMG_VOID SYSDEVKM_Deinitialise(IMG_VOID)
{
	/* Ensure that all of the object have been destroyed...*/
	IMG_ASSERT(gsActiveOpenCnt.counter == 0);

	SYSDEVU_Deinitialise();
}


/*!
******************************************************************************

 @Function				SYSDEVKM_OpenDevice

******************************************************************************/
IMG_RESULT SYSDEVKM_OpenDevice(
	IMG_CHAR *				pszDeviceName,
	IMG_HANDLE *			phSysDevHandle
)
{
	IMG_UINT32			ui32Result;
	SYSDEVKM_sDevice *	psDevice;

	/* Allocate a device structure...*/
	psDevice = IMG_MALLOC(sizeof(*psDevice));
	IMG_ASSERT(psDevice != IMG_NULL);
	if (psDevice == IMG_NULL)
	{
		return IMG_ERROR_OUT_OF_MEMORY;
	}
	IMG_MEMSET(psDevice, 0, sizeof(*psDevice));

	/* Get the device id...*/
#ifndef __FAKE_HW__
	ui32Result = SYSDEVU_GetDeviceId(pszDeviceName, &psDevice->ui32DeviceId);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        IMG_FREE(psDevice);
        return ui32Result;
    }

    SYSDEVU_GetMemoryInfo(psDevice->ui32DeviceId,
                          &psDevice->pvCpuKmAddrMemBase,
                          &psDevice->pvCpuPhysAddrMemBase,
                          &psDevice->ui64MemSize,
                          &psDevice->ui64DevMemBase);
#else
#define MEM_SIZE	(770048)
	psDevice->ui32DeviceId = 0;

	/* no such thing as memory base, as memory is allocated arbitrarily by the kernel */
    psDevice->pvCpuKmAddrMemBase    = IMG_NULL;
    psDevice->pvCpuPhysAddrMemBase	= IMG_NULL;
    psDevice->ui64MemSize			= MEM_SIZE;
	psDevice->ui64DevMemBase		= 0;

	ui32Result = IMG_SUCCESS;
#endif 

	/* Return the device handle...*/
	*phSysDevHandle = psDevice;
	
	/* Update count...*/
	atomic_inc(&gsActiveOpenCnt);

	return IMG_SUCCESS;
}
IMGVIDEO_EXPORT_SYMBOL(SYSDEVKM_OpenDevice)


/*!
******************************************************************************

 @Function				SYSDEVKM_CloseDevice

******************************************************************************/
IMG_VOID SYSDEVKM_CloseDevice(
	IMG_HANDLE			hSysDevHandle
)
{
	SYSDEVKM_sDevice *	psDevice = (SYSDEVKM_sDevice *)hSysDevHandle;
	
	IMG_ASSERT(hSysDevHandle != IMG_NULL);
	if (hSysDevHandle == IMG_NULL)
	{
		return;
	}
	IMG_FREE(psDevice);
	
	/* Update count...*/
	atomic_dec(&gsActiveOpenCnt);
}
IMGVIDEO_EXPORT_SYMBOL(SYSDEVKM_CloseDevice)

/*!
******************************************************************************

 @Function                SYSDEVKM_GetDeviceID

******************************************************************************/
IMG_UINT32 SYSDEVKM_GetDeviceID(
	IMG_HANDLE				hSysDevHandle
)
{
	SYSDEVKM_sDevice *psDevice = (SYSDEVKM_sDevice *)hSysDevHandle;
	return psDevice->ui32DeviceId;
}
IMGVIDEO_EXPORT_SYMBOL(SYSDEVKM_GetDeviceID)

/*!
******************************************************************************

 @Function                SYSDEVKM_GetCpuKmAddr

******************************************************************************/
IMG_RESULT SYSDEVKM_GetCpuKmAddr(
	IMG_HANDLE				hSysDevHandle,
	SYSDEVKM_eRegionId		eRegionId,
	IMG_VOID **				ppvCpuKmAddr,
	IMG_UINT32 *            pui32Size
)
{
	SYSDEVKM_sDevice *		psDevice = (SYSDEVKM_sDevice *)hSysDevHandle;
	IMG_VOID *              pvCpuPhysAddr;
	IMG_UINT32              ui32Result;
	IMG_HANDLE              hRegHandle;
	
	IMG_ASSERT(hSysDevHandle != IMG_NULL);
	if (hSysDevHandle == IMG_NULL)
	{
		return IMG_ERROR_GENERIC_FAILURE;
	}

	ui32Result = SYSDEVU_GetCpuAddrs(psDevice->ui32DeviceId, eRegionId, ppvCpuKmAddr, &pvCpuPhysAddr, pui32Size);
	IMG_ASSERT(ui32Result == IMG_SUCCESS);
	if (ui32Result != IMG_SUCCESS) 
	{
		return ui32Result;
	}

	/* Add this to the list of mappable regions...*/
	if (!SYSBRGU_GetMappableRegion(pvCpuPhysAddr))
	{
		ui32Result = SYSBRGU_CreateMappableRegion((IMG_UINTPTR) pvCpuPhysAddr, *pui32Size,
		                                          SYS_MEMATTRIB_UNCACHED, IMG_FALSE, NULL, &hRegHandle);
		IMG_ASSERT(ui32Result == IMG_SUCCESS);
		if (ui32Result != IMG_SUCCESS) 
		{
			return ui32Result;
		}
	}

	return IMG_SUCCESS;
}

#include "sysmem_utils.h"
/*!
******************************************************************************

 @Function				SYSDEVKM_CpuPAddrToDevPAddr

******************************************************************************/
IMG_UINT64 SYSDEVKM_CpuPAddrToDevPAddr(
	IMG_HANDLE				hSysDevHandle,
	IMG_UINT64				ui64CpuPAddr
)
{
	IMG_UINT64	ui64DevPAddr = 0;
#ifndef IMG_MEMALLOC_UNIFIED_VMALLOC
    SYSDEVU_sInfo *     psSysDev;
    SYSDEVKM_sDevice *  psDevice = (SYSDEVKM_sDevice *)hSysDevHandle;
	IMG_UINT64	ui64CpuPAddrMemBase;
#endif /* ndef IMG_MEMALLOC_UNIFIED_VMALLOC */

	IMG_ASSERT(gSysDevInitialised);
	
	IMG_ASSERT(hSysDevHandle != IMG_NULL);
	if (hSysDevHandle == IMG_NULL)
	{
		return 0;
	}
	
#ifdef IMG_MEMALLOC_UNIFIED_VMALLOC
    /* on unified memory systems, MemBase is 0, because memory is arbitrarily allocated */
    ui64DevPAddr = ui64CpuPAddr;
#else /* ndef IMG_MEMALLOC_UNIFIED_VMALLOC */
    psSysDev = SYSDEVU_GetDeviceById(psDevice->ui32DeviceId);
    /* Convert address to physical address */
    ui64CpuPAddrMemBase = SYSMEMU_CpuKmAddrToCpuPAddr(psSysDev->sMemPool, psDevice->pvCpuKmAddrMemBase);

    if (
            (ui64CpuPAddr >= ui64CpuPAddrMemBase) &&
            (ui64CpuPAddr < ui64CpuPAddrMemBase+psDevice->ui64MemSize)
        )
    {
        ui64DevPAddr = ui64CpuPAddr - ui64CpuPAddrMemBase + psDevice->ui64DevMemBase;
    }
    else
    {
        IMG_ASSERT(IMG_FALSE);
    }
#endif /* ndef IMG_MEMALLOC_UNIFIED_VMALLOC */

	/* Return the device physical address...*/
	return ui64DevPAddr;
}
IMGVIDEO_EXPORT_SYMBOL(SYSDEVKM_CpuPAddrToDevPAddr)

/*!
******************************************************************************

 @Function				SYSDEVKM_RegisterDevKmLisr

******************************************************************************/
IMG_VOID SYSDEVKM_RegisterDevKmLisr(
				    IMG_HANDLE			hSysDevHandle,
				    SYSDEVKM_pfnDevKmLisr	pfnDevKmLisr,
				    IMG_VOID *                  pvParam
)
{
	SYSDEVKM_sDevice *	psDevice = (SYSDEVKM_sDevice *)hSysDevHandle;
	
	IMG_ASSERT(gSysDevInitialised);
	IMG_ASSERT(hSysDevHandle != IMG_NULL);
	if (hSysDevHandle == IMG_NULL)
	{
		return;
	}

	SYSDEVU_RegisterDevKmLisr(psDevice->ui32DeviceId, pfnDevKmLisr, pvParam);
}


/*!
******************************************************************************

 @Function				SYSDEVKM_RemoveDevKmLisr

******************************************************************************/
IMG_VOID SYSDEVKM_RemoveDevKmLisr(
	IMG_HANDLE				hSysDevHandle
)
{
	SYSDEVKM_sDevice *	psDevice = (SYSDEVKM_sDevice *)hSysDevHandle;
	
	IMG_ASSERT(gSysDevInitialised);
	IMG_ASSERT(hSysDevHandle != IMG_NULL);
	if (hSysDevHandle == IMG_NULL)
	{
		return;
	}

	SYSDEVU_RegisterDevKmLisr(psDevice->ui32DeviceId, IMG_NULL, IMG_NULL);
}

/*!
******************************************************************************

 @Function				SYSDEVKM_SetPowerState

******************************************************************************/
IMG_VOID SYSDEVKM_SetPowerState(
	IMG_HANDLE				hSysDevHandle,
	SYSOSKM_ePowerState     ePowerState,
	IMG_BOOL				forAPM
)
{
	SYSDEVKM_sDevice *	psDevice = (SYSDEVKM_sDevice *)hSysDevHandle;
	IMG_ASSERT(gSysDevInitialised);
	IMG_ASSERT(hSysDevHandle != IMG_NULL);

	if (hSysDevHandle == IMG_NULL)
	{
		return;
	}

	switch(ePowerState) {
	case SYSOSKM_POWERSTATE_S5:		// suspend
		SYSDEVU_HandleSuspend(psDevice->ui32DeviceId, forAPM);
		break;
	case SYSOSKM_POWERSTATE_S0:		// resume
		SYSDEVU_HandleResume(psDevice->ui32DeviceId, forAPM);
		break;
	}

}

/*!
******************************************************************************

 @Function				SYSDEVKM_SetDeviceInfo

******************************************************************************/
IMG_VOID SYSDEVKM_SetDeviceInfo(
	IMG_CHAR *          pszDevName,
	TARGET_sTargetConfig *	psFullInfo
)
{
	IMG_ASSERT(gSysDevInitialised);

#if 0
	SYSDEVU1_SetDeviceInfo(pszDevName, psFullInfo);
#endif
}

/*!
******************************************************************************

 @Function				SYSDEVKM_ActivateDevKmLisr

******************************************************************************/
IMG_RESULT SYSDEVKM_ActivateDevKmLisr(
	IMG_HANDLE				hSysDevHandle
)
{
    IMG_UINT32          ui32Result;
	SYSDEVKM_sDevice *	psDevice = (SYSDEVKM_sDevice *)hSysDevHandle;
	
	IMG_ASSERT(gSysDevInitialised);
	IMG_ASSERT(hSysDevHandle != IMG_NULL);
	if (hSysDevHandle == IMG_NULL)
	{
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        return ui32Result;
    }
    ui32Result = SYSDEVU_InvokeDevKmLisr(psDevice->ui32DeviceId);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
	}

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				SYSDEVKM_ApmPpmFlagsReset

******************************************************************************/
IMG_VOID SYSDEVKM_ApmPpmFlagsReset(
	IMG_HANDLE				hDevHandle
)
{
	IMG_ASSERT(gSysDevInitialised);
	IMG_ASSERT(hDevHandle != IMG_NULL);
	if (hDevHandle == IMG_NULL)
	{
		return;
	}

    DMANKM_ResetPowerManagementFlag(hDevHandle);
}


/*!
******************************************************************************

 @Function				SYSDEVKM_ApmDeviceSuspend

******************************************************************************/
IMG_VOID SYSDEVKM_ApmDeviceSuspend(
	IMG_HANDLE				hDevHandle
)
{
	IMG_ASSERT(gSysDevInitialised);
	IMG_ASSERT(hDevHandle != IMG_NULL);
	if (hDevHandle == IMG_NULL)
	{
		return;
	}

    DMANKM_SuspendDevice(hDevHandle);
}


/*!
******************************************************************************

 @Function				SYSDEVKM_ApmDeviceResume

******************************************************************************/
IMG_VOID SYSDEVKM_ApmDeviceResume(
	IMG_HANDLE				hDevHandle
)
{
	IMG_ASSERT(gSysDevInitialised);
	IMG_ASSERT(hDevHandle != IMG_NULL);
	if (hDevHandle == IMG_NULL)
	{
		return;
	}
	
    DMANKM_ResumeDevice(hDevHandle);
}
