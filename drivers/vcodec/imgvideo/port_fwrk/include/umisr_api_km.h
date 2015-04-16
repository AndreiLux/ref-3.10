/*!
 *****************************************************************************
 *
 * @file	   umisr_api_km.h
 *
 * The User Mode ISR kernel mode API.
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

#if !defined (__UMISR_API_KM_H__)
#define __UMISR_API_KM_H__

#include <img_defs.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*!
******************************************************************************

 @Function				UMISRKM_Initialise
 
 @Description 
 
 This function is used to initialises the user mode ISR component and should 
 be called from the device driver initialisation function.
 
 @Input     hProcHandle :	The process handle.

 @Output    phUmIsrHandle : A pointer used to return an UMISR handle.

 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT UMISRKM_Initialise(
    IMG_HANDLE			hProcHandle,
    IMG_HANDLE *        phUmIsrHandle
);

/*!
******************************************************************************

 @Function				UMISRKM_Deinitialise
 
 @Description 
 
 This function is used to deinitialises the user mode ISR component and should
 be called from the device driver de-initialisation function.
 
 @Input    hUmIsrHandle :   The UMISR handle.

 @Return	None.

******************************************************************************/
extern IMG_VOID UMISRKM_Deinitialise(
    IMG_HANDLE          hUmIsrHandle
);


/*!
******************************************************************************

 @Function				UMISRKM_SignalInterrupt
 
 @Description 
 
 This function is used to signal an interrupt from kernel to a user mode
 process.
 
 @Input    hUmIsrHandle :   The UMISR handle.

 @Input		ui32IntNo :		The interrupt number to be signalled.

 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT UMISRKM_SignalInterrupt(
    IMG_HANDLE          hUmIsrHandle,
	IMG_UINT32			ui32IntNo
);


#if defined(__cplusplus)
}
#endif
 
#endif /* __UMISR_API_KM_H__	*/

