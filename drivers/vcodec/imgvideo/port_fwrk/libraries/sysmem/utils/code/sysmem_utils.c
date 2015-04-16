/*!
 *****************************************************************************
 *
 * @file	   sysmem_utils.c
 *
 * This file contains the System Memory Kernel Mode UtilitiesAPI.
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

#include "img_defs.h"
#ifdef IMG_MEM_ERROR_INJECTION
#undef IMG_MEM_ERROR_INJECTION
#include "sysmem_utils.h"
#define IMG_MEM_ERROR_INJECTION
#else
#include "sysmem_utils.h"
#endif
#include "lst.h"

#ifdef IMG_KERNEL_MODULE
#include <linux/module.h>
#endif

#define MAX_SYSMEM_HEAPS (5)
static IMG_INT32 gNoHeaps;
static LST_T gHeaps;

static IMG_BOOL	gSysMemInitialised = IMG_FALSE;		/*!< Indicates where the API has been initialised	*/

__inline static SYSMEM_Heap *findHeapById(SYS_eMemPool memId) {
	SYSMEM_Heap *heap = (SYSMEM_Heap *)LST_first(&gHeaps);
	while(heap != IMG_NULL)
	{
		if(memId == heap->memId)
			return heap;
		heap = LST_next(heap);
	}
	return IMG_NULL;
}

/*!
******************************************************************************

 @Function				SYSMEMU_Initialise

******************************************************************************/
IMG_RESULT SYSMEMU_Initialise(IMG_VOID)
{
	if (!gSysMemInitialised)
	{
		gSysMemInitialised = IMG_TRUE;
	}

	return IMG_SUCCESS;
}


IMG_RESULT SYSMEMU_AddMemoryHeap(
    SYSMEM_Ops *    ops,
    IMG_BOOL        contiguous,
    IMG_VOID *      priv,
    SYS_eMemPool *  peMemPool
)
{

	SYSMEM_Heap *heap = IMG_MALLOC(sizeof(SYSMEM_Heap));
	IMG_ASSERT(heap != IMG_NULL);
    if(IMG_NULL == heap)
    {
        return IMG_ERROR_OUT_OF_MEMORY;
    }
	heap->ops = ops;
	heap->contiguous = contiguous;
	heap->priv = priv;
	heap->memId = gNoHeaps;
	LST_add(&gHeaps, (void *)heap);

	if(heap->ops->Initialise)
		heap->ops->Initialise(heap);

	*peMemPool = gNoHeaps++;

    return IMG_SUCCESS;
}

IMGVIDEO_EXPORT_SYMBOL(SYSMEMU_AddMemoryHeap)

IMG_RESULT SYSMEMU_RemoveMemoryHeap(SYS_eMemPool memPool)
{
	SYSMEM_Heap *heap;
	heap = findHeapById(memPool);
	IMG_ASSERT(heap != IMG_NULL);
	if(!heap)
	{
		return IMG_ERROR_GENERIC_FAILURE;
	}
	heap = LST_remove(&gHeaps, (void *)heap);
    IMG_ASSERT(heap != IMG_NULL);
    if(heap == IMG_NULL)
    {
        return IMG_ERROR_FATAL;
    }

	if(heap->ops->Deinitialise)
		heap->ops->Deinitialise(heap);
	IMG_FREE(heap);
	return IMG_SUCCESS;
}
IMGVIDEO_EXPORT_SYMBOL(SYSMEMU_RemoveMemoryHeap)


/*!
******************************************************************************

 @Function				SYSMEMU_Deinitialise

******************************************************************************/
IMG_VOID SYSMEMU_Deinitialise(IMG_VOID)
{
	if (gSysMemInitialised)
	{
		gSysMemInitialised = IMG_FALSE;
	}
}

/*!
******************************************************************************

 @Function				SYSMEMU_AllocPages

******************************************************************************/
SYSMEMU_sPages * SYSMEMU_AllocPages(
	IMG_UINT32				ui32Size,
	SYS_eMemAttrib			eMemAttrib,
	SYS_eMemPool			eMemPool
)
{
	SYSMEMU_sPages *			psPages;

	IMG_ASSERT(gSysMemInitialised);

	/* Allocate connection context...*/
	psPages = IMG_MALLOC(sizeof(*psPages));
	IMG_ASSERT(psPages != IMG_NULL);
	if (psPages == IMG_NULL)
	{
		return IMG_NULL;
	}
	IMG_MEMSET(psPages, 0, sizeof(*psPages));

	/* Setup control block...*/
	psPages->ui32Size	= ui32Size;
	psPages->eMemAttrib = eMemAttrib;
	psPages->eMemPool	= eMemPool;

	/* Return pointer to stucture...*/
	return psPages;
}

/*!
******************************************************************************

 @Function				FreePages

******************************************************************************/
static IMG_VOID FreePages(
	SYSMEMU_sPages *			psPages
)
{
	IMG_ASSERT(gSysMemInitialised);

	/* Free the control block...*/
	IMG_FREE(psPages);
}

// allocation/free
IMG_RESULT SYSMEMU_AllocatePages(IMG_UINT32 ui32Size, SYS_eMemAttrib eMemAttrib,
		SYS_eMemPool eMemPool, IMG_HANDLE *phPagesHandle, IMG_UINT64 ** pPhyAddrs)
{
	IMG_RESULT result;
	SYSMEMU_sPages *psPages;
	SYSMEM_Heap *heap;
	heap = findHeapById(eMemPool);
	IMG_ASSERT(heap != IMG_NULL);
	IMG_ASSERT(phPagesHandle != IMG_NULL);
    IMG_ASSERT(pPhyAddrs != IMG_NULL);
    if((!heap) || (!phPagesHandle))
	{
		return IMG_ERROR_GENERIC_FAILURE;
	}

	psPages = SYSMEMU_AllocPages(ui32Size, eMemAttrib, eMemPool);
	IMG_ASSERT(psPages != IMG_NULL);
	if (psPages == IMG_NULL)
	{
		return IMG_ERROR_OUT_OF_MEMORY;
	}

	result = heap->ops->AllocatePages(heap, ui32Size, psPages, eMemAttrib);

	IMG_ASSERT(result == IMG_SUCCESS);
	if(result != IMG_SUCCESS)
	{
		FreePages(psPages);
	}

	*phPagesHandle = psPages;

    if(pPhyAddrs)
    {
        *pPhyAddrs = psPages->paPhysAddr;
    }


	return result;
}
IMGVIDEO_EXPORT_SYMBOL(SYSMEMU_AllocatePages)


IMG_VOID SYSMEMU_FreePages(IMG_HANDLE hPagesHandle)
{
	SYSMEMU_sPages *psPages = hPagesHandle;
	SYSMEM_Heap *heap;
	heap = findHeapById(psPages->eMemPool);
	IMG_ASSERT(heap != IMG_NULL);
	if(!heap)
	{
		return;
	}
	heap->ops->FreePages(heap, hPagesHandle);
	FreePages(psPages);
}
IMGVIDEO_EXPORT_SYMBOL(SYSMEMU_FreePages)


// import
IMG_RESULT SYSMEMU_ImportExternalPages(SYS_eMemPool eMemPool, IMG_UINT32 ui32Size, SYS_eMemAttrib eMemAttrib, IMG_HANDLE *phPagesHandle, IMG_VOID *pvCpuKmAddr, IMG_UINT64 * pPhyAddrs)
{
	IMG_RESULT result;
	SYSMEMU_sPages *psPages;
	SYSMEM_Heap *heap;
	heap = findHeapById(eMemPool);
	IMG_ASSERT(heap != IMG_NULL);
	if(!heap)
	{
		return IMG_ERROR_GENERIC_FAILURE;
	}

	psPages = SYSMEMU_AllocPages(ui32Size, eMemAttrib, eMemPool);
	IMG_ASSERT(psPages != IMG_NULL);
	if (psPages == IMG_NULL)
	{
		return IMG_ERROR_OUT_OF_MEMORY;
	}

	psPages->bImported = IMG_TRUE;

	result = heap->ops->ImportExternalPages(heap, ui32Size, psPages, eMemAttrib, pvCpuKmAddr, pPhyAddrs);

	IMG_ASSERT(result == IMG_SUCCESS);
	if(result != IMG_SUCCESS)
	{
		FreePages(psPages);
		return result;
	}

	*phPagesHandle = psPages;
	return result;

}

IMG_RESULT SYSMEMU_ImportPages(IMG_UINT32 ui32Size, IMG_HANDLE hExtImportHandle,
		IMG_HANDLE *phPagesHandle, IMG_VOID **ppvCpuKmAddr)
{
	IMG_RESULT result;
	SYSMEMU_sPages *psPages = hExtImportHandle;
	SYSMEM_Heap *heap;
	heap = findHeapById(psPages->eMemPool);
	IMG_ASSERT(heap != IMG_NULL);
	if(!heap)
		return IMG_ERROR_GENERIC_FAILURE;

	result = heap->ops->ImportPages(heap, ui32Size, hExtImportHandle, phPagesHandle);
	IMG_ASSERT(result == IMG_SUCCESS);
	if(result != IMG_SUCCESS)
	{
		return result;
	}

    if(ppvCpuKmAddr)
    {
        *ppvCpuKmAddr = (*((SYSMEMU_sPages **)phPagesHandle))->pvCpuKmAddr;
    }
	return result;
}

// translation
IMG_RESULT SYSMEMU_GetCpuKmAddr(IMG_VOID **ppvCpuKmAddr,IMG_HANDLE hPagesHandle)
{
	SYSMEMU_sPages *psPages = hPagesHandle;
	SYSMEM_Heap *heap;
	heap = findHeapById(psPages->eMemPool);
	IMG_ASSERT((heap != IMG_NULL) && (ppvCpuKmAddr != IMG_NULL));
	if((!heap) || (!ppvCpuKmAddr))
		return IMG_ERROR_GENERIC_FAILURE;
	return heap->ops->GetCpuKmAddr(heap, ppvCpuKmAddr, hPagesHandle);
}
IMGVIDEO_EXPORT_SYMBOL(SYSMEMU_GetCpuKmAddr)

IMG_UINT64 SYSMEMU_CpuKmAddrToCpuPAddr(SYS_eMemPool eMemPool, IMG_VOID *pvCpuKmAddr)
{
	SYSMEM_Heap *heap;
    IMG_UINT64 ui64Addr;
	heap = findHeapById(eMemPool);
	IMG_ASSERT(heap != IMG_NULL);
	if(!heap)
		return -1;

    ui64Addr = heap->ops->CpuKmAddrToCpuPAddr(heap, pvCpuKmAddr);
	return ui64Addr;
}
IMGVIDEO_EXPORT_SYMBOL(SYSMEMU_CpuKmAddrToCpuPAddr)


IMG_VOID *SYSMEMU_CpuPAddrToCpuKmAddr(SYS_eMemPool eMemPool, IMG_UINT64 pvCpuPAddr)
{
	SYSMEM_Heap *heap;
	heap = findHeapById(eMemPool);
	IMG_ASSERT(heap != IMG_NULL);
	if(!heap)
		return IMG_NULL;

	return heap->ops->CpuPAddrToCpuKmAddr(heap, pvCpuPAddr);
}


// maintenance
IMG_VOID SYSMEMU_UpdateMemory(IMG_HANDLE hPagesHandle, SYSMEM_UpdateDirection dir)
{
	switch(dir)
	{
	case CPU_TO_DEV:
#ifdef CONFIG_ARM
    /* ARM Cortex write buffer needs to be synchronised before device can access it */
    	dmb();
#endif
    	break;
	case DEV_TO_CPU:
		break;
	default:
		break;
	}
	return;
}
IMGVIDEO_EXPORT_SYMBOL(SYSMEMU_UpdateMemory)


IMG_BOOL SYSMEMKM_IsContiguous(IMG_HANDLE hPagesHandle) {
	SYSMEMU_sPages *psPages = hPagesHandle;
	SYSMEM_Heap *heap;
	heap = findHeapById(psPages->eMemPool);
	IMG_ASSERT(heap != IMG_NULL);
	if(!heap)
		return IMG_FALSE;

	return heap->contiguous;
}

