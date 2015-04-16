/*!
 *****************************************************************************
 *
 * @file	   sysmem_api_km.h
 *
 * The System Memory kernel mode API.
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

#if !defined (__SYSMEM_API_KM_H__)
#define __SYSMEM_API_KM_H__

#include "img_errors.h"
#include "img_types.h"
#include "img_defs.h"
#include "system.h"

#if defined(__cplusplus)
extern "C" {
#endif


/*!
******************************************************************************

 @Function				SYSMEMKM_Initialise
 
 @Description 
 
 This function is used to initialise the system memory component and is called 
 at start-up.  
  
 @Input		None. 

 @Return    IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSMEMKM_Initialise(IMG_VOID);


/*!
******************************************************************************

 @Function				SYSMEMKM_Deinitialise
 
 @Description 
 
 This function is used to deinitialises the system memory component and would 
 normally be called at shutdown.
 
 @Input		None.

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSMEMKM_Deinitialise(IMG_VOID);


/*!
******************************************************************************

 @Function				SYSMEMKM_AllocPages
 
 @Description 
 
 This function is used to allocate a number of memory pages which can be 
 accessed by the devices within the system/SoC.

 NOTE: The returned pointer is for a CPU linear mapping in kernel mode which
 is page aligned.

 NOTE: The pool (eMemPool) can be used when devices share memory, to ensure
 the that allocation is made from memory accessible by the devices sharing the
 allocation.

 NOTE: It is assumed that the memory allocated is locked and will not cause
 a page fault when the device accesses it.
  
 @Input		ui32Size :		The size of the allocation (in bytes).

 @Input		eMemAttrib :	The memory attributes.

 @Input		eMemPool :		The pool from which the memory is to be allocated.

 @Output	phPagesHandle :	A pointer used to return a handle to the block.

 @Output	ppvCpuKmAddr :	A pointer used to return kernel mode pointer for 
							the allocation.

 @Return    IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSMEMKM_AllocPages(
	IMG_UINT32		ui32Size,
	SYS_eMemAttrib	eMemAttrib,
	SYS_eMemPool	eMemPool,
    IMG_UINT64 **   ppaPhysAddr,
	IMG_HANDLE *	phPagesHandle,
	IMG_VOID **		ppvCpuKmAddr
);


/*!
******************************************************************************

 @Function              SYSMEMKM_ImportExternalPages
 
 @Description 
 
 This function is used to import a number of memory pages which can be
 accessed by the devices within the system/SoC.

 NOTE: The returned pointer is for a CPU linear mapping in kernel mode which
 is page aligned.

 NOTE: The pool (eMemPool) can be used when devices share memory, to ensure
 the that allocation is made from memory accessible by the devices sharing the
 allocation.

 NOTE: It is assumed that the memory allocated is locked and will not cause
 a page fault when the device accesses it.

 @Input     ui32Size :      The size of the allocation (in bytes).

 @Input     eMemAttrib :    The memory attributes.

 @Input     eMemPool :      The pool from which the memory is to be allocated.

 @Output    phPagesHandle : A pointer used to return a handle to the block.

 @Input     pvCpuKmAddr :   KM address to beginning of buffer

 @Input     ui64PhyAddr :   Physical address (only makes sense for contiguous memory)

 @Return    IMG_RESULT :    This function returns either IMG_SUCCESS or an
                            error code.

******************************************************************************/
extern IMG_RESULT SYSMEMKM_ImportExternalPages(
    IMG_UINT32      ui32Size,
    SYS_eMemAttrib  eMemAttrib,
    SYS_eMemPool    eMemPool,
    IMG_HANDLE *    phPagesHandle,
    IMG_VOID *      pvCpuKmAddr,
    IMG_SYS_PHYADDR *pPhyAddrs
);


/*!
******************************************************************************

 @Function				SYSMEMKM_ImportPages
 
 @Description 
 
 This function is used to create Portability Framework structures for 
 memory regions, which have been allocated externally.
  
 @Input		ui32Size :		The size of the allocation (in bytes).

 @Input     hExtImportHandle	: The buffer "external import" handle.

 @Output	phPagesHandle :	A pointer used to return a handle to the block.

 @Output	ppvCpuKmAddr :	A pointer used to return kernel mode pointer for 
							the allocation.

 @Return    IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT SYSMEMKM_ImportPages(
	IMG_UINT32				ui32Size,
	IMG_HANDLE				hExtImportHandle,
	IMG_HANDLE *			phPagesHandle,
	IMG_VOID **				ppvCpuKmAddr			       
);


/*!
******************************************************************************

 @Function				SYSMEMKM_GetCpuKmAddr
 
 @Description 
 
 This function is used to obtain the CPU KM address.
 
 NOTE: It is assumed that the memory to be mapped is locked so that the
 device is able to access the memory.
 
 @Output	pvCpuKmAddr :	The kernel mode address of the memory to be mapped.

 @Input		hPagesHandle :	A pointer used to return a handle to the block.

 @Return    IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT	SYSMEMKM_GetCpuKmAddr(
	IMG_VOID **				ppvCpuKmAddr,
	IMG_HANDLE  			hPagesHandle
);


/*!
******************************************************************************

 @Function				SYSMEMKM_IsContiguous
 
 @Description 
 
 This function is used to determine if an allocation is contiguous.

 @Input		hPagesHandle :		A handle to the block returned by 
								SYSMEMKM_AllocPages() or SYSMEMKM_MapPages()

 @Return	IMG_BOOL :			IMG_TRUE if the allocation is contigous, otherwise
								IMG_FALSE.

******************************************************************************/
extern IMG_BOOL SYSMEMKM_IsContiguous(
	IMG_HANDLE		hPagesHandle
);


/*!
******************************************************************************

 @Function				SYSMEMKM_FreePages
 
 @Description 
 
 This function is used to free memory pages.

 NOTE: If the pages where allocated using SYSMEMKM_AllocPages() then this
 function frees of the underlying memory allocation.  However, when used
 with pages mapped through SYSMEMKM_MapPages() then only the mapping
 is removed.  The underlying memory is not freed.

 @Input		hPagesHandle :		A handle to the block returned by 
								SYSMEMKM_AllocPages() or SYSMEMKM_MapPages()

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSMEMKM_FreePages(
	IMG_HANDLE		hPagesHandle
);

/*!
******************************************************************************

 @Function				SYSMEMKM_MemoryCpuToDevice
 
 @Description 
 
 This function is used to synchronise memory so that it can be accessed by the device. 
 This function must be called after being modified by the host, and 
 before a buffer is accessed by the device, so that any
 caching (eg write-buffers or writecombine buffers) are flushed/cleaned/sync'd.
 On ARM, this uses a memory barrier to ensure synchronisation of write-buffer. 
 On X86, this does nothing. 

 @Input		hPagesHandle :		A handle to the block returned by 
								SYSMEMKM_AllocPages() or SYSMEMKM_MapPages()

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSMEMKM_MemoryCpuToDevice(
	IMG_HANDLE		hPagesHandle
);


/*!
******************************************************************************

 @Function				SYSMEMKM_MemoryDeviceToCpu
 
 @Description 
 
 This function is used to synchronise memory after it has been modified by the device. 
 This function must be called before a buffer is accessed by the host.
 On ARM, this does nothing. 

 @Input		hPagesHandle :		A handle to the block returned by 
								SYSMEMKM_AllocPages() or SYSMEMKM_MapPages()

 @Return	None.

******************************************************************************/
extern IMG_VOID SYSMEMKM_MemoryDeviceToCpu(
	IMG_HANDLE		hPagesHandle
);

/*!
******************************************************************************
 
 @Function IMG_BIGORSMALL_ALLOC

 @Description

 Allocate and free using either kmalloc or vmalloc, depending on the size

 @Input ui32Size   : number of bytes to be allocated
 @Return pointer to allocated memory
 
******************************************************************************/
static size_t max_kmalloc_size = 8192;	// arbitrary size boundary: 2 pages
static IMG_INLINE void * IMG_BIGORSMALL_ALLOC(IMG_UINT32 ui32Size)
{
    void * ptr;
    if(ui32Size <= max_kmalloc_size)
        ptr = IMG_MALLOC(ui32Size);
    else
        ptr = IMG_BIG_MALLOC(ui32Size);
    return ptr;
}
/*!
******************************************************************************
 
 @Function IMG_BIGORSMALL_FREE

 @Description

 Free using either kfree or vfree, depending on the size

 @Input ui32Size   : number of bytes to be allocated
 @Input ptr :        memory to be freed
 @Return none
 
******************************************************************************/
static IMG_INLINE void IMG_BIGORSMALL_FREE(IMG_UINT32 ui32Size, void* ptr)
{
    if(ui32Size <= max_kmalloc_size)
        IMG_FREE(ptr);
    else
        IMG_BIG_FREE(ptr);
}


#if defined(__cplusplus)
}
#endif
 
#endif /* __SYSMEM_API_KM_H__	*/


