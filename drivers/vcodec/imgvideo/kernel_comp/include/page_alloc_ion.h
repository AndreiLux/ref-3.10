/*!
 *****************************************************************************
 *
 * @file	   page_alloc_ion.h
 *
 * The Page Allocator ION util API.
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

#if !defined (PAGE_ALLOC_ION_H)
#define PAGE_ALLOC_ION_H

#include "img_defs.h"
#include "img_types.h"
#include "servicesext.h"    // Needed for IMG_SYS_PHYADDR


#if defined(__cplusplus)
extern "C" {
#endif


/*!
******************************************************************************

 @Function              palloc_GetIONPages
 
 @Description 
 
 Get addresses for an ion-allocated buffer
 
 @Input		buff_fd :       Process file descriptor for ION buffer

 @Input		uiSize :        Buffer size

 @Output	pCpuPhysAddrs : Pointer to array where we will store the physical addresses
                            of the pages that compose the buffer
							 
 @Output	ppvKernAddr :   Pointer to location where the kernel linear address pointing
                            to the buffer will be stored

 @Output	phBuf :         Pointer to buffer handle

 @Return	IMG_RESULT :    This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT palloc_GetIONPages(SYS_eMemPool eMemPool, int buff_fd, IMG_UINT uiSize,
                                     IMG_SYS_PHYADDR* pCpuPhysAddrs, IMG_PVOID *ppvKernAddr,
                                     IMG_HANDLE *phBuf);


/*!
******************************************************************************

 @Function              palloc_ReleaseIONBuf
 
 @Description 
 
 Get addresses for an ion-allocated buffer
 
 @Input		hBuf :       Handle to the ION buffer

 @Return	IMG_RESULT : This function returns either IMG_SUCCESS or an
                         error code.

******************************************************************************/
extern IMG_RESULT palloc_ReleaseIONBuf(IMG_HANDLE hBuf, IMG_PVOID pvKernAddr);

#if defined(__cplusplus)
}
#endif
 
#endif /* PAGE_ALLOC_ION_H	*/
