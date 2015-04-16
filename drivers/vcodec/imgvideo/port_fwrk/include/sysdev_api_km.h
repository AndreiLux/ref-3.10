/*!
 *****************************************************************************
 *
 * @file	   sysdev_api_km.h
 *
 * The System Device kernel mode API.
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

#if !defined (__SYSDEV_API_KM_H__)
#define __SYSDEV_API_KM_H__	//!< Defined to prevent file being included more than once

#include <img_defs.h>
#include "target.h"
#include <dman_api_km.h>
#include <sysos_api_km.h>

#if defined(__cplusplus)
extern "C" {
#endif


/*!
******************************************************************************
 Macro used to initialise a #SYSDEVKM_sDevInfo structure.
******************************************************************************/
#define SYS_DEVICE(name,prefix,subdevice){ name, & prefix##KM_fnDevRegister, subdevice}

/*!
******************************************************************************
 This structure contains information for a device API.

 NOTE: This structure MUST be defined in static memory as it is retained and
 used by the SYSDEV component whilst the system is active.

 NOTE: The order of the items is important - see #SYS_DEVICE.

 @brief		This structure contains information for a SYSDEVKM API.

******************************************************************************/
typedef struct
{
	IMG_CHAR *				pszDeviceName;		//!< The device name
	DMANKM_pfnDevRegister	pfnDevRegister;		//!< Registration function
	IMG_BOOL				bSubDevice;			//!< Used to indicate devices which are a sub-component of some greater
											//!< device, and as such do not have their own PCI base address / socket ID.
} SYSDEVKM_sDevInfo;

/*!
******************************************************************************
 This type defines the device region(s).
******************************************************************************/
typedef enum
{
	SYSDEVKM_REGID_REGISTERS,			//!< Device registers
	SYSDEVKM_REGID_SLAVE_PORT,			//!< Device slave port

} SYSDEVKM_eRegionId;

/*!
******************************************************************************

 @Function				SYSDEVKM_RegisterDevices

 @Description

 This function is used called to register the devices with the Device Manager (DMAN).

 NOTE: This function is called from SYSDEVKM_Initialise() during start-up.  It
 is defined here to allow users to locate the devices that have been registered with
 the Device Manager.


 @Input		None.

 @Return    IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSDEVKM_RegisterDevices(IMG_VOID);


/*!
******************************************************************************

 @Function				SYSDEVKM_Initialise

 @Description

 This function is used to initialise the system device component and is
 called at start-up.

 NOTE: This function will call SYSDEVKM_RegisterDevices() to register the
 devices with the Device Manager.

 @Input		None.

 @Return    IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSDEVKM_Initialise(IMG_VOID);


/*!
******************************************************************************

 @Function				SYSDEVKM_Deinitialise

 @Description

 This function is used to deinitialises thesystem device component and would
 normally be called at shutdown.

 @Input		None.

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSDEVKM_Deinitialise(IMG_VOID);


/*!
******************************************************************************

 @Function				SYSDEVKM_OpenDevice

 @Description

 This function is used to open a device.

 This function may require enumeration and locating the device over PCI or
 some other form of device enumeration/location which is specific to the system/SoC.

 @Input		pszDeviceName :	The name of the device - should be the same
							as the name used when registering the device
							via DMANKM_RegisterDevice().

 @Output	phSysDevHandle :	A pointer used to return the device handle.

 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSDEVKM_OpenDevice(
	IMG_CHAR *				pszDeviceName,
	IMG_HANDLE *			phSysDevHandle
);


/*!
******************************************************************************

 @Function				SYSDEVKM_CloseDevice

 @Description

 This function is used to close a device.

 @Input		hSysDevHandle :	The device handle returned by SYSDEVKM_OpenDevice().

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSDEVKM_CloseDevice(
	IMG_HANDLE				hSysDevHandle
);


IMG_UINT32 SYSDEVKM_GetDeviceID(
	IMG_HANDLE				hSysDevHandle
);

/*!
******************************************************************************

 @Function				SYSDEVKM_GetCpuKmAddr

 @Description

 This function is used to obtain a kernel mode mapping of the device registers
 etc.

 @Input		hSysDevHandle :	The device handle returned by SYSDEVKM_OpenDevice().

 @Input		eRegionId :		The region of the device to be mapped.

 @Output	ppvCpuKmAddr :	A pointer used to return a kernel mode CPU linear
							address for the device region.

 @Output	pui32Size :		A pointer used to return the size of the region mapped.

 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSDEVKM_GetCpuKmAddr(
	IMG_HANDLE				hSysDevHandle,
	SYSDEVKM_eRegionId		eRegionId,
	IMG_VOID **				ppvCpuKmAddr,
	IMG_UINT32 *			pui32Size
);

/*!
******************************************************************************

 @Function				SYSDEVKM_CpuPAddrToDevPAddr

 @Description

 This function is used to convert a CPU physical address to a device physical
 address.

 NOTE: The device mapping of physical memory may be different from the CPU's
 mapping of memory, so a level of re-mapping may be required based on the
 device Id. For systems in which the CPU physical address is the same as the
 device physical address this function should just return the CPU physical
 address parameter as provided.

 @Input		hSysDevHandle :	The device handle returned by SYSDEVKM_OpenDevice().

 @Input		ui64CpuPAddr :	CPU physical address obtained via SYSDEV_GetCpuPAddrKM().

 @Return	IMG_UINT64 :	The device physical address.

******************************************************************************/
extern IMG_UINT64 SYSDEVKM_CpuPAddrToDevPAddr(
	IMG_HANDLE				hSysDevHandle,
	IMG_UINT64				ui64CpuPAddr
);


/*!
******************************************************************************

 @Function              SYSDEVKM_pfnDevKmLisr

 @Description

 This is the prototype for device Kernel Mode LISR callback functions.  This
 function is called when the device interrupt is suspected.

 NOTE: The Kernel Mode LISR should return IMG_FALSE if the device did not
 cause the interrupt. This allows for several devices to share an interrupt
 line.  The Kernel Mode LISR returns IMG_TRUE if the interrupt has been handled.

 NOTE: By convention, further device interrupts are disabled whilst the Kernel
 Mode LISR is active.  Device interrupts must be re-enabled by a synchronous,
 or asynchronous call to SYSDEVKN_EnableDeviceInt().

 @Input		pvParam :		Pointer parameter, defined when the
						    callback was registered.

							NOTE: This pointer must be valid in interrupt
							context.

 @Return	IMG_BOOL :		IMG_TRUE if the interrupt has been handles by the
							Kernel Mode LISR, otherwise IMG_FALSE.

******************************************************************************/
typedef IMG_BOOL ( * SYSDEVKM_pfnDevKmLisr) (
    IMG_VOID *                  pvParam
);


/*!
******************************************************************************

 @Function				SYSDEVKM_RegisterDevKmLisr

 @Description

 This function is used to register a Kernel Mode Low-level Interrupt Service
 Routine (LISR) callback function.  This function is called when the device
 interrupts.

 NOTE: On registering a Kernel Mode LISR for a device the device interrupt
 is enabled.

 NOTE: A Kernel Mode LISR many be registered multiple times.  However, the
 registration auguments must be the same for all calls to
 SYSBRGKM_RegisterDevKmLisr().

 @Input		hSysDevHandle :	The device handle returned by SYSDEVKM_OpenDevice().

 @Input		pfnDevKmLisr :	A pointer to the Kernel Mode LISR callback function.

 @Input		pvParam :		An IMG_VOID * value passed to the Kernel Mode LISR
					        function when a device interrupt occurs.

							NOTE: This pointer must be valid in interrupt
							context.

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSDEVKM_RegisterDevKmLisr(
	IMG_HANDLE					hSysDevHandle,
	SYSDEVKM_pfnDevKmLisr		pfnDevKmLisr,
    IMG_VOID *                  pvParam
);


/*!
******************************************************************************

 @Function				SYSDEVKM_RemoveDevKmLisr

 @Description

 This function is used to remove a Kernel Mode Low-level Interrupt Service
 Routine (LISR) callback function.

 NOTE: Removing the Kernel Mode Low-level Interrupt Service
 Routine disables interrupts for the device.

 @Input		hSysDevHandle :	The device handle returned by SYSDEVKM_OpenDevice().

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSDEVKM_RemoveDevKmLisr(
	IMG_HANDLE				hSysDevHandle
);

/*!
******************************************************************************

 @Function				SYSDEVKM_ActivateDevKmLisr

 @Description

 This function invokes a device's KM LISR. This can be used to manually invoke
 daisy chained interrupts from a top level ISR.

 @Input		hSysDevHandle :	The device handle returned by SYSDEVKM_OpenDevice().

 IMG_RESULT               :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSDEVKM_ActivateDevKmLisr(
	IMG_HANDLE				hSysDevHandle
);

/*!
******************************************************************************

 @Function				SYSDEVKM_SetPowerState

 @Description

 This function is used to set the power state for a device.

 NOTE: The power is state of the device is normally managed by the Device Manager.
 The power is turned on when the first connection is made to the device and off
 when the last connection has been closed and the device driver has signaled the
 disconnect has completed using DMANKM_DevDisconnectComplete().
 The power state may also be changed by the Device Manager in response to power
 transition events signaled via SYSOSKM_pfnKmPowerEvent - for devices that have
 registered power management callback functions such as DMANKM_fnDevPowerPreS5
 and DMANKM_fnDevPowerPostS0.

 @Input		hSysDevHandle :	The device handle returned by SYSDEVKM_OpenDevice().

 @Input     ePowerState   : Indicates the state to which the power is to be set.

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSDEVKM_SetPowerState(
	IMG_HANDLE				hSysDevHandle,
	SYSOSKM_ePowerState     ePowerState,
	IMG_BOOL				forAPM
);

/*!
******************************************************************************

 @Function				SYSDEVKM_SetDeviceInfo

 @Description

 This function is used to set the device info.

 NOTE: This function may not be used/required on all systems.

 @Input		pszDevName :	A pointer to the device name.

 @Input		psFullInfo :	A pointer to a structure containing the information.

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSDEVKM_SetDeviceInfo(
	IMG_CHAR *          pszDevName,
	TARGET_sTargetConfig *	psFullInfo
);

/*!
******************************************************************************

 @Function				SYSDEVKM_ApmPpmFlagsReset

 @Description

 This function is used to reset APM PPM flags in device manager.

 @Input		hDevHandle :	The device handle.

 @Return	None.
******************************************************************************/
IMG_VOID SYSDEVKM_ApmPpmFlagsReset(
	IMG_HANDLE				hDevHandle
);

/*!
******************************************************************************

 @Function				SYSDEVKM_ApmDeviceSuspend

 @Description

 This function is used to suspend the device by APM.

 @Input		hDevHandle :	The device handle.

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSDEVKM_ApmDeviceSuspend(
	IMG_HANDLE				hDevHandle
);

/*!
******************************************************************************

 @Function				SYSDEVKM_ApmDeviceResume

 @Description

 This function is used to resume the device after APM suspend.

 @Input		hDevHandle :	The device handle.

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSDEVKM_ApmDeviceResume(
	IMG_HANDLE				hDevHandle
);

#if defined(__cplusplus)
}
#endif

#endif /* __SYSDEV_API_KM_H__	*/


