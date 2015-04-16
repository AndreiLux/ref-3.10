/*!
 *****************************************************************************
 *
 * @file	   sysdev_utils.h
 *
 * The System Device kernel mode utilities.
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

#if !defined (__SYSDEV_UTILS_H__)
#define __SYSDEV_UTILS_H__

#include <img_defs.h>
#include <sysdev_api_km.h>
#include <system.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined DOXYGEN_WILL_SEE_THIS

extern IMG_BOOL	gSysDevInitialised;		/*!< Indicates where the API has been initialised	*/

#endif

struct SYSDEVU_sInfo;

/*!
******************************************************************************
 This structure contains device information.

 #SYSDEVKM_sDevInfo MUST be the first element in this structure.
******************************************************************************/
typedef struct SYSDEVU_sInfo
{
	LST_LINK;
	SYSDEVKM_sDevInfo		sDevInfo;			//!< #SYS_DEVICE defined part of device info
	IMG_BOOL				bDevLocated;		//!< IMG_TRUE when the device has been located
	SYSDEVKM_pfnDevKmLisr	pfnDevKmLisr;		//!< Pointer to any registered Kernel Mode LISR callback


	IMG_VOID (*pfnFreeDevice)(struct SYSDEVU_sInfo *);

	IMG_VOID (*pfnResumeDevice)(struct SYSDEVU_sInfo *, IMG_BOOL forAPM);
	IMG_VOID (*pfnSuspendDevice)(struct SYSDEVU_sInfo *, IMG_BOOL forAPM);


	IMG_VOID *              pvParam;			//!< Pointer to be passed to the Kernel Mode LISR callback
	IMG_VOID *				pvLocParam;			//!< Pointer used to retains "located" information

	IMG_UINT32 *			pui32PhysRegBase;	//!< A pointer to the device registers physical address (or mappable to user mode) - IMG_NULL if not defined
	IMG_UINT32 *			pui32KmRegBase;		//!< A pointer to the device register base in kernel mode - IMG_NULL if not defined
	IMG_UINT32 				ui32RegSize;		//!< The size of the register block (0 if not known)

	IMG_UINT32 *			pui32PhysMemBase;
	IMG_UINT32 *			pui32KmMemBase;
	IMG_UINT32 				ui32MemSize;
	IMG_UINT64				pui64DevMemoryBase;

	IMG_UINT32				ui32DeviceId;

	SYS_eMemPool			sMemPool;
	SYS_eMemPool			secureMemPool;

	IMG_VOID *pPrivate;
} SYSDEVU_sInfo;

extern IMG_RESULT SYSDEVU_RegisterDriver(SYSDEVU_sInfo *);
extern IMG_RESULT SYSDEVU_UnRegisterDriver(SYSDEVU_sInfo *sysdev);

/*!
******************************************************************************

 @Function				SYSDEVU_RegisterDevices

 @Description 
 
 This function is used to register the devices with the Device Manager (DMAN).
 
 @Input		ui32NoDevices :	The number of devices. 

 @Input		psInfo :		A pointer to an array of devices info structures
							for the devices to be registered

 @Return    IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSDEVU_RegisterDevice(
	SYSDEVU_sInfo *			psInfo
);

extern IMG_RESULT SYSDEVU_UnRegisterDevice(
	SYSDEVU_sInfo *psInfo
);


/*!
******************************************************************************

 @Function				SYSDEVU_Initialise
 
 @Description 
 
 This function is used to initialise the system device component and is 
 called at start-up.  
 
 NOTE: This function will call SYSDEVKM_RegisterDevices() to register the
 devices with the Device Manager.
 
 @Input		None. 

 @Return    IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSDEVU_Initialise(IMG_VOID);


/*!
******************************************************************************

 @Function				SYSDEVU_Deinitialise
 
 @Description 
 
 This function is used to deinitialises thesystem device component and would 
 normally be called at shutdown.
 
 @Input		None.

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSDEVU_Deinitialise(IMG_VOID);
 
SYSDEVU_sInfo *SYSDEVU_GetDeviceById(
	IMG_UINT32 ui32DeviceId
);

/*!
******************************************************************************

 @Function				SYSDEVU_GetDeviceId
 
 @Description 
 
 This function is used to get the device id.

 @Input		pszDeviceName :	The name of the device - should be the same
							as the name used when registering the device
							via DMANKM_RegisterDevice().
 
 @Output	pui32DeviceId :	A pointer used to return the device Id.
 
 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSDEVU_GetDeviceId(
	IMG_CHAR *				pszDeviceName,
	IMG_UINT32 *			pui32DeviceId
);


/*!
******************************************************************************

 @Function				SYSDEVU1_GetMemoryInfo

 @Description

 This function is used to get the memory infomation.

 NOTE: This may not be required/used in all implementations of this API. It
 is used in the DEVIF implemenation from SYSMEM to obtain the memory mapping
 information.

 @Output	ppvMemory : A pointer used to return the kernel mode memory mapping.

 @Output	ppvPhysMemory : A pointer used to return the physical memory address.

 @Output	pui64MemSize : A pointer used to return the size of teh memory mapping.

 @Output	pui64DevMemoryBase : A pointer used to return the device address of the memory base

 @Return    None.

******************************************************************************/
extern IMG_VOID SYSDEVU_GetMemoryInfo(
	IMG_UINT32			ui32DeviceId,
	IMG_VOID **			ppvKmMemory,
	IMG_VOID **			ppvPhysMemory,
	IMG_UINT64 *		pui64MemSize,
	IMG_UINT64 *		pui64DevMemoryBase
);


/*!
******************************************************************************

 @Function				SYSDEVU_GetCpuAddrs
 
 @Description 
 
 This function is used to obtains a kernel mode mapping of the device registers
 etc.
 
 @Input		ui32DeviceId :	The device id returned by SYSDEVKM_GetDeviceId().
 
 @Input		eRegionId :	The region of the device to be mapped.

 @Output    ppvCpuKmAddr :  A pointer user to return the kernel mode address

 @Output    ppvCpuPhysAddr : A pointer user to return the physical address

 @Output    pui32Size :     A pointer user to return the region size

 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
IMG_UINT32 SYSDEVU_GetCpuAddrs(
	IMG_UINT32				ui32DeviceId,
	SYSDEVKM_eRegionId		eRegionId,
	IMG_VOID **             ppvCpuKmAddr,
	IMG_VOID **             ppvCpuPhysAddr,
	IMG_UINT32 *            pui32Size
);

extern IMG_VOID SYSDEVU_HandleSuspend(
	IMG_UINT32					ui32DeviceId,
	IMG_BOOL					forAPM
);

extern IMG_VOID SYSDEVU_HandleResume(
	IMG_UINT32					ui32DeviceId,
	IMG_BOOL					forAPM
);

/*!
******************************************************************************

 @Function				SYSDEVU_RegisterDevKmLisr
 
 @Description 
 
 This function is used to register a Kernel Mode Low-level Interrupt Service 
 Routine (LISR) callback function.  This function is called when the device 
 interrupts.

 NOTE: On registering a Kernel Mode LISR for a device the device interrupt 
 is enabled.

 NOTE: A Kernel Mode LISR many be registered multiple times.  However, the 
 registration auguments must be the same for all calls to 
 SYSBRGKM_RegisterDevKmLisr().

 NOTE: Interrupts are disabled when the device is closed using 
 SYSDEVKM_CloseDevice() and re-enabled on subsequent calls to
 SYSDEVKM_GetDeviceId().
 
 @Input		ui32DeviceId :	The device id returned by SYSDEVKM_GetDeviceId().

 @Input		pfnDevKmLisr :	A pointer to the Kernel Mode LISR callback function.

 @Input		pvParam :		An IMG_VOID * value passed to the Kernel Mode LISR
					        function when a device interrupt occurs.

							NOTE: This pointer must be valid in interrupt
							context.

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSDEVU_RegisterDevKmLisr(
	IMG_UINT32					ui32DeviceId,
	SYSDEVKM_pfnDevKmLisr		pfnDevKmLisr,
    IMG_VOID *                  pvParam
);
	
/*!
******************************************************************************

 @Function				SYSDEVU_InvokeDevKmLisr
 
 @Description 
 
 This function is used to invoke a Kernel Mode Low-level Interrupt Service 
 Routine (LISR) callback function.

 @Input		ui32DeviceId :	The device id returned by SYSDEVKM_GetDeviceId().

 IMG_RESULT              :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/

IMG_RESULT SYSDEVU_InvokeDevKmLisr(
	IMG_UINT32					ui32DeviceId
);

IMG_VOID SYSDEVU_SetDevMap(SYSDEVU_sInfo *sysdev, IMG_UINT32 *physreg_base, IMG_UINT32 *kmreg_base, IMG_UINT32 regsize, IMG_UINT32 *physmem_base,
		IMG_UINT32 *kmmem_base, IMG_UINT32 memsize, IMG_UINT64 devmem_base);

IMG_VOID SYSDEVU_SetDevFuncs(SYSDEVU_sInfo *sysdev,
		IMG_VOID (*pfnFreeDevice)(SYSDEVU_sInfo *),
		IMG_VOID (*pfnResumeDevice)(SYSDEVU_sInfo *, IMG_BOOL forAPM),
		IMG_VOID (*pfnSuspendDevice)(SYSDEVU_sInfo *, IMG_BOOL forAPM)
);
	
#if defined(__cplusplus)
}
#endif
 
#endif /* __SYSDEV_UTILS_H__	*/


