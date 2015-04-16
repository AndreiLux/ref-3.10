/*!
 *****************************************************************************
 *
 * @file	   sysdev_utils1.h
 *
 * This file contains the header file information for the
 * System Device Kernel Mode Utilities Level 1 API.
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

#if !defined (__SYSDEV_UTILS1_H__)
#define __SYSDEV_UTILS1_H__

#include <img_defs.h>
#include <sysdev_utils.h>

#if defined(__cplusplus)
extern "C" {
#endif



/*!
******************************************************************************

 @Function				SYSDEVU1_Initialise

 @Description

 This function is used to initialise the system device component and is
 called at start-up.

 NOTE: This function will call SYSDEVKM_RegisterDevices() to register the
 devices with the Device Manager.

 @Input		None.

 @Return    IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSDEVU1_Initialise(IMG_VOID);


/*!
******************************************************************************

 @Function				SYSDEVU1_Deinitialise

 @Description

 This function is used to deinitialises the system device component and would
 normally be called at shutdown.

 @Input		None.

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSDEVU1_Deinitialise(IMG_VOID);

/*!
******************************************************************************

 @Function				SYSDEVU1_LocateDevice

 @Description

 This function is used called to locate a device.

 @Input		psInfo :		A pointer to the device info structure for the
							device to be located.

 @Return    IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSDEVU1_LocateDevice(
	SYSDEVU_sInfo *			psInfo
);

/*!
******************************************************************************

 @Function				SYSDEVU1_CheckDevicePresent

 @Description

 This function is used called to check if the device is present in the system.

 @Input		None.

 @Return    IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
IMG_RESULT SYSDEVU1_CheckDevicePresent(IMG_VOID);

/*!
******************************************************************************

 @Function				SYSDEVU1_FreeDevice

 @Description

 This function is used called to free a device.

 @Input		psInfo :		A pointer to the device info structure for the
							device to be freed.

 @Return    None.

******************************************************************************/
extern IMG_VOID SYSDEVU1_FreeDevice(
	SYSDEVU_sInfo *			psInfo
);


/*!
******************************************************************************

 @Function				SYSDEVU1_PreDeinitialise

 @Description

 This function is used to perform any pre-deinitialisation.

 @Return    None.

******************************************************************************/
extern IMG_VOID SYSDEVU1_PreDeinitialise( IMG_VOID );

/*!
******************************************************************************

 @Function				SYSDEVU1_CpuKmAddrToCpuPAddr

 @Description

 This function is used to convert a CPU kernel mode address to a CPU physical
 address.

 NOTE: This is a generic utility function that does not specifically involve
 a device.

 @Input		pvCpuKmAddr :	The CPU kernel mode address.

 @Return	IMG_UINT64 :	The CPU physical address.

******************************************************************************/
IMG_UINT64 SYSDEVU1_CpuKmAddrToCpuPAddr(
	IMG_VOID *				pvCpuKmAddr
);

/*!
******************************************************************************

 @Function				SYSDEVU1_CpuPAddrToCpuKmAddr

 @Description

 This function is used to convert a CPU physical address to a CPU
 kernel mode address

 NOTE: This is a generic utility function that does not specifically involve
 a device.

 @Input		pvCpuPAddr :	The CPU physical address.

 @Return	IMG_UINT64 :	The CPU kernel mode address.

******************************************************************************/
IMG_VOID *SYSDEVU1_CpuPAddrToCpuKmAddr(
		IMG_UINT64 pvCpuPAddr
);

/*!
******************************************************************************

 @Function				SYSDEVU1_SetDeviceInfo

 @Description

 This function is used to set the device info.

 NOTE: This function may not be used/required on all systems.

 @Input		pszDevName :	A pointer to the device name.

 @Input		psFullInfo :	A pointer to a structure containing the information.

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSDEVU1_SetDeviceInfo(
	IMG_CHAR *				pszDevName,
	TARGET_sTargetConfig *		psFullInfo
);

/*!
******************************************************************************

 @Function				SYSDEVU1_RemapReources

 @Description

 This function is used to map device resources.

 NOTE: This function may not be used/required on all systems.

 @Input		dev :	A pointer to the PCI device.

 @Output	None

 @Return	None.

******************************************************************************/
IMG_VOID *SYSDEVU1_RemapResources(
	IMG_VOID *dev,
	IMG_UINT32 ui32Bar
	);

/*!
******************************************************************************

 @Function				SYSDEVU1_UnmapResources

 @Description

 This function is used to unmap device resources.

 NOTE: This function may not be used/required on all systems.

 @Input		dev :	A pointer to the PCI device.

 @Output	None

 @Return	None.

******************************************************************************/
IMG_VOID SYSDEVU1_UnmapResources(
	IMG_UINT32 ui32Bar
	);

/*!
******************************************************************************

 @Function				SYSDEVU1_HandleSuspend

 @Description

 This function handles a suspend event.

 NOTE: This function may not be used/required on all systems.

 @Input		None
 @Output	None

 @Return	None.

******************************************************************************/
IMG_VOID SYSDEVU1_HandleSuspend(IMG_VOID);

/*!
******************************************************************************

 @Function				SYSDEVU1_HandleResume

 @Description

 This function handles a suspend event.

 NOTE: This function may not be used/required on all systems.

 @Input		None
 @Output	None

 @Return	None.

******************************************************************************/
IMG_VOID SYSDEVU1_HandleResume(IMG_VOID);

#if defined(__cplusplus)
}
#endif

#endif /* __SYSDEV_UTILS1_H__	*/


