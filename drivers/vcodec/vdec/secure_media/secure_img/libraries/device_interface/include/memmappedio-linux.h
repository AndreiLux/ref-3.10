/*!
 *****************************************************************************
 *
 * @file       memmappedio-linux.h
 *
 * This file declares the memory-mapped device interface.
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

#ifndef __MEMMAPPEDIO_H__
#define __MEMMAPPEDIO_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include "img_include.h"
#include "img_defs.h"


/*!
******************************************************************************

 @Function              MEMMAPPEDIO_pfnDevKmLisr

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
typedef IMG_BOOL ( * MEMMAPPEDIO_pfnDevKmLisr) (
    IMG_VOID *                  pvParam
);

/*!
******************************************************************************
 This enum describes the device region to map.
******************************************************************************/
typedef enum 
{
	MEMMAPPEDIO_MAPAREA_REGISTER,
	MEMMAPPEDIO_MAPAREA_MEMORY,
	MEMMAPPEDIO_MAPAREA_REG_MEM,

	MEMMAPPEDIO_MAPAREA_MAX

} MEMMAPPEDIO_eMapArea;

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
	IMG_BOOL				bSubDevice;			//!< Used to indicate devices which are a sub-component of some greater
											//!< device, and as such do not have their own PCI base address / socket ID.
} MEMMAPPEDIO_sDevInfo;

/*!
******************************************************************************
 This structure contains device information.

 #SYSDEVKM_sDevInfo MUST be the first element in this structure.
******************************************************************************/
typedef struct
{
	MEMMAPPEDIO_sDevInfo   		sDevInfo;			//!< #SYS_DEVICE defined part of device info
	IMG_BOOL				    bDevLocated;		//!< IMG_TRUE when the device has been located
	MEMMAPPEDIO_pfnDevKmLisr   	pfnDevKmLisr;		//!< Pointer to any registered Kernel Mode LISR callback
	IMG_VOID *                  pvParam;			//!< Pointer to be passed to the Kernel Mode LISR callback
	IMG_VOID *				    pvLocParam;			//!< Pointer used to retains "located" information
	IMG_UINTPTR 		        uipPhysRegBase;	    //!< A pointer to the device registers physical address (or mappable to user mode) - IMG_NULL if not defined
	IMG_VOID *  		        pvKmRegBase;		//!< A pointer to the device register base in kernel mode - IMG_NULL if not defined
	IMG_UINT32 				    ui32RegSize;		//!< The size of the register block (0 if not known)

} MEMMAPPEDIO_sInfo;


#ifndef IMG_KERNEL_MODULE

#pragma message("Device Interface (Memory-mapped) not implemented for this host platform")
#define MEMMAPPEDIO_Initialise()                                  IMG_ERROR_NOT_SUPPORTED
#define MEMMAPPEDIO_DeInitialise(eArea)                           IMG_ERROR_NOT_SUPPORTED
#define MEMMAPPEDIO_LocateDevice(psInfo, eArea)                   IMG_ERROR_NOT_SUPPORTED
#define MEMMAPPEDIO_GetMemoryInfo(ppui32KmMemory, pui64MemSize)   IMG_ERROR_NOT_SUPPORTED
#define MEMMAPPEDIO_CpuVirtAddrToCpuPAddr(pvCpuKmAddr)            0

#else

/*!
******************************************************************************

 @Function				MEMMAPPEDIO_Initialise

******************************************************************************/
extern IMG_RESULT 
MEMMAPPEDIO_Initialise(
     IMG_VOID
);
/*!
******************************************************************************

 @Function				MEMMAPPEDIO_DeInitialise

******************************************************************************/
extern IMG_VOID 
MEMMAPPEDIO_DeInitialise(
    MEMMAPPEDIO_eMapArea    eArea
);

/*!
******************************************************************************

 @Function				MEMMAPPEDIO_LocateDevice

 @Description

******************************************************************************/
extern IMG_RESULT 
MEMMAPPEDIO_LocateDevice(
	MEMMAPPEDIO_sInfo *			psInfo,
	MEMMAPPEDIO_eMapArea        eArea
);

/*!
******************************************************************************

 @Function				MEMMAPPEDIO_GetMemoryInfo

******************************************************************************/
IMG_VOID MEMMAPPEDIO_GetMemoryInfo(
	IMG_VOID **         ppui32KmMemory,
	IMG_UINT64 *        pui64MemSize
);
/*!
******************************************************************************

 @Function				MEMMAPPEDIO_CpuVirtAddrToCpuPAddr

******************************************************************************/
IMG_UINT64 MEMMAPPEDIO_CpuVirtAddrToCpuPAddr(
				       IMG_VOID *pvCpuKmAddr
);

#endif

#if defined(__cplusplus)
}
#endif

#endif /* __MEMMAPPEDIO_H__ */
