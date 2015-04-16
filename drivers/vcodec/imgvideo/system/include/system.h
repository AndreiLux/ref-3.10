/*!
 *****************************************************************************
 *
 * @file	   system.h
 *
 * This file contains the System Defintions.
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

#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <img_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(IMG_KERNEL_MODULE)
#include <linux/module.h>
#endif
/*! Size of the system log to be used */
#define SYS_LOG_BUFFER_SIZE (4096)


/*! name for the linux kernel system bridging module */
#define IMGSYSBRG_MODULE_NAME "imgsysbrg_vdec"
/*! name for the linux kernel acpi module */
#define IMGSYS_MODULE_NAME "imgsys_topaz_hp"


#define     SYS_MMU_PAGE_ALIGNMENT  (4096)  //!< Page alignment of the MMU
#define     SYS_LOG_BUFFER_SIZE     (4096)  //!< Logging buffer size
#define 	MMU_PAGE_SIZE 			(0x1000)
#define 	SYS_MMU_PAGE_SIZE 		(MMU_PAGE_SIZE)				//!< Page size of the MMU


#define SYS_BUILD_FLAGS (0x0)


/*!
******************************************************************************
 This type defines the memory attributes.
******************************************************************************/
typedef enum
{
	SYS_MEMATTRIB_CACHED		= 0x00000001,	//!< Memory to be allocated as cached
	SYS_MEMATTRIB_UNCACHED		= 0x00000002,	//!< Memory to be allocated as uncached
	SYS_MEMATTRIB_WRITECOMBINE	= 0x00000004,   /*!< Memory to be allocated as write-combined
													 (or equivalent buffered/burst writes mechanism)	*/
	SYS_MEMATTRIB_SECURE		= 0x00000008
} SYS_eMemAttrib;


typedef IMG_UINT32 SYS_eMemPool;

#if defined(IMG_KERNEL_MODULE)
#define IMGVIDEO_EXPORT_SYMBOL(symbol) EXPORT_SYMBOL(symbol);
#else
#define IMGVIDEO_EXPORT_SYMBOL(symbol)
#endif

#ifdef __cplusplus
}
#endif


#endif /* __SYSTEM_H__   */

