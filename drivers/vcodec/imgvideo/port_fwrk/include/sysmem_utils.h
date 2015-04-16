/*!
 *****************************************************************************
 *
 * @file	   sysmem_utils.h
 *
 * This file contains the header file information for the
 * System Memory Kernel Mode API.
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

#if !defined (__SYSMEM_UTILS_H__)
#define __SYSMEM_UTILS_H__

#include "img_errors.h"
#include "img_defs.h"
#include "system.h"
#include "lst.h"

#ifdef IMG_MEM_ERROR_INJECTION
#define SYSMEMU_AllocatePages  MEMERRINJ_DevAlloc
#define SYSMEMU_FreePages      MEMERRINJ_DevFree
#endif /* IMG_MEM_ERROR_INJECTION */

#if defined(__cplusplus)
extern "C" {
#endif

/*!
******************************************************************************
 This structure contains the pages information.
******************************************************************************/
typedef struct
{
    LST_LINK;                             //!< List link (allows the structure to be part of a MeOS list).
    IMG_BOOL        bMappingOnly;         //!< IMG_TRUE if this is a mapping rather than an allocation
    IMG_BOOL        bImported;            //!< IMG_TRUE if this is allocation was imported
    IMG_UINT32      ui32Size;             //!< Size of allocation
    SYS_eMemAttrib  eMemAttrib;           //!< Memory attributes
    SYS_eMemPool    eMemPool;             //!< Memory pool
    /* IMG_VOID *     pvCpuPhysBaseAddr;    //!< CPU physical base address */
    IMG_VOID *      pvCpuKmAddr;          //!< CPU kernel mode address
    IMG_HANDLE      hRegHandle;           //!< Handle to mapping
    IMG_VOID *      pvImplData;           //!< Pointer that holds data specific to sysmem implementation
    IMG_UINT64 *    paPhysAddr;            //!< Array with physical addresses of the pages
} SYSMEMU_sPages;


struct SYSMEM_Heap;

typedef enum {
    CPU_TO_DEV,
    DEV_TO_CPU
} SYSMEM_UpdateDirection;

typedef struct SYSMEM_Ops {
    IMG_RESULT (*Initialise)(struct SYSMEM_Heap *);
    IMG_VOID (*Deinitialise)(struct SYSMEM_Heap *);

    // allocation/free
    IMG_RESULT (*AllocatePages)(struct SYSMEM_Heap *, IMG_UINT32 ui32Size, SYSMEMU_sPages *psPages,
            SYS_eMemAttrib eMemAttrib);

    IMG_VOID (*FreePages)(struct SYSMEM_Heap *, IMG_HANDLE hPagesHandle);

    // import
    IMG_RESULT (*ImportExternalPages)(struct SYSMEM_Heap *, IMG_UINT32 ui32Size, SYSMEMU_sPages *psPages,
            SYS_eMemAttrib eMemAttrib, IMG_VOID *pvCpuKmAddr, IMG_UINT64 * pPhyAddrs);
    IMG_RESULT (*ImportPages)(struct SYSMEM_Heap *, IMG_UINT32 ui32Size, IMG_HANDLE hExtImportHandle,
            IMG_HANDLE *phPagesHandle);

    // translation
    IMG_RESULT    (*GetCpuKmAddr)(struct SYSMEM_Heap *, IMG_VOID **ppvCpuKmAddr,IMG_HANDLE hPagesHandle);

    IMG_UINT64 (*CpuKmAddrToCpuPAddr)(struct SYSMEM_Heap *heap, IMG_VOID *pvCpuKmAddr);
    IMG_VOID *(*CpuPAddrToCpuKmAddr)(struct SYSMEM_Heap *heap, IMG_UINT64 pvCpuPAddr);

    // maintenance
    IMG_VOID (*UpdateMemory)(struct SYSMEM_Heap *, IMG_HANDLE hPagesHandle, SYSMEM_UpdateDirection dir);

} SYSMEM_Ops;

typedef struct SYSMEM_Heap {
    LST_LINK;
    SYS_eMemPool memId;
    SYSMEM_Ops *ops;
    IMG_BOOL contiguous;
    IMG_VOID *priv;
} SYSMEM_Heap;

/*!
******************************************************************************

 @Function                SYSMEMU_Initialise

 @Description

 This function is used to initialise the system memory component and is called
 at start-up.

 @Input     None.

 @Return    IMG_RESULT : This function returns either IMG_SUCCESS or an
                         error code.

******************************************************************************/
extern IMG_RESULT SYSMEMU_Initialise(IMG_VOID);


/*!
******************************************************************************

 @Function                SYSMEMU_Deinitialise

 @Description

 This function is used to de-initialises the system memory component and would
 normally be called at shutdown.

 @Input     None.

 @Return    None.

******************************************************************************/
extern IMG_VOID SYSMEMU_Deinitialise(IMG_VOID);


/*!
******************************************************************************

 @Function                SYSMEMU_AllocPages

 @Description

 This function is used to allocate a number of memory pages which can be
 accessed by the devices within the system/SoC.

 @Input     ui32Size:   The size of the allocation (in bytes).

 @Input     eMemAttrib: The memory attributes.

 @Input     eMemPool:   The pool from which the memory is to be allocated.

 @Return    SYSMEMU_sPages * : A pointer to the pages control block, IMG_NULL
                              if the creation of the control block fails.

******************************************************************************/
extern SYSMEMU_sPages * SYSMEMU_AllocPages(
    IMG_UINT32      ui32Size,
    SYS_eMemAttrib  eMemAttrib,
    SYS_eMemPool    eMemPool
);

extern IMG_RESULT SYSMEMU_AddMemoryHeap(SYSMEM_Ops *ops, IMG_BOOL contiguous, IMG_VOID *priv, SYS_eMemPool *peMemPool);
extern IMG_RESULT SYSMEMU_RemoveMemoryHeap(SYS_eMemPool memPool);

/* Memory pool initialisation. */
extern IMG_RESULT SYSMEMKM_AddSystemMemory(
    SYS_eMemPool *  peMemPool
);
extern IMG_RESULT SYSMEMKM_AddDevIFMemory(
    IMG_UINTPTR     vstart,
    IMG_UINT64      pstart,
    IMG_UINT32      size,
    SYS_eMemPool *  peMemPool
);
extern IMG_RESULT SYSMEMKM_AddCarveoutMemory(
    IMG_UINTPTR     vstart,
    IMG_UINT64      pstart,
    IMG_UINT32      size,
    SYS_eMemPool *  peMemPool
);

// allocation/free
IMG_RESULT SYSMEMU_AllocatePages(IMG_UINT32 ui32Size, SYS_eMemAttrib eMemAttrib,
        SYS_eMemPool eMemPool, IMG_HANDLE *phPagesHandle, IMG_UINT64 ** pPhyAddrs);
IMG_VOID SYSMEMU_FreePages(IMG_HANDLE hPagesHandle);

// import
IMG_RESULT SYSMEMU_ImportExternalPages(SYS_eMemPool eMemPool, IMG_UINT32 ui32Size,
        SYS_eMemAttrib eMemAttrib, IMG_HANDLE *phPagesHandle, IMG_VOID *pvCpuKmAddr, IMG_UINT64 * pPhyAddrs);
IMG_RESULT SYSMEMU_ImportPages(IMG_UINT32 ui32Size, IMG_HANDLE hExtImportHandle,
        IMG_HANDLE *phPagesHandle, IMG_VOID **ppvCpuKmAddr);

// translation
IMG_RESULT SYSMEMU_GetCpuKmAddr(IMG_VOID **ppvCpuKmAddr,IMG_HANDLE hPagesHandle);
IMG_UINT64 SYSMEMU_CpuKmAddrToCpuPAddr(SYS_eMemPool eMemPool, IMG_VOID *pvCpuKmAddr);
IMG_VOID *SYSMEMU_CpuPAddrToCpuKmAddr(SYS_eMemPool eMemPool, IMG_UINT64 pvCpuPAddr);

// maintenance
IMG_VOID SYSMEMU_UpdateMemory(IMG_HANDLE hPagesHandle, SYSMEM_UpdateDirection dir);
IMG_BOOL SYSMEMKM_IsContiguous(IMG_HANDLE hPagesHandle);



#if defined(__cplusplus)
}
#endif

#endif /* __SYSMEM_UTILS_H__    */


