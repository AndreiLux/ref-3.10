/*!
 *****************************************************************************
 *
 * @file	   sysmem_api_km.c
 *
 * This file contains DEVIF user mode implementation of the
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

#include "img_defs.h"
#include "sysmem_api_km.h"
#include "sysmem_utils.h"
#include "sysdev_utils1.h"
#include "sysbrg_utils.h"
#include "sysos_api_km.h"
#include "system.h"
#include <linux/genalloc.h>

//#define CLEAR_PAGES     (1)                         //!< Defined to clear memory pages (normally only for debug/test)


struct priv_params {
    IMG_UINTPTR  vstart;
    IMG_UINT64   pstart;
    IMG_UINT32   size;

    struct gen_pool *pool;
};

/*!
******************************************************************************

 @Function                SYSMEMKM_Initialise

******************************************************************************/
static IMG_RESULT Initialise(
    SYSMEM_Heap *  heap
)
{
    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function                SYSMEMKM_Deinitialise

******************************************************************************/
static IMG_VOID Deinitialise(
    SYSMEM_Heap *  heap
)
{
    struct priv_params *priv = (struct priv_params *)heap->priv;

    /* If we have a page allocation array - free it...*/
    gen_pool_destroy(priv->pool);
    IMG_FREE(priv);
    heap->priv = IMG_NULL;
}

static IMG_VOID *CpuPAddrToCpuKmAddr(
    SYSMEM_Heap *  heap,
    IMG_UINT64     pvCpuPAddr
)
{
    IMG_INT64 offset = 0LL;
    struct priv_params *prv = (struct priv_params *)heap->priv;

    IMG_ASSERT(pvCpuPAddr > prv->pstart && (pvCpuPAddr < (prv->pstart + prv->size)));
    offset = pvCpuPAddr - prv->pstart;
    return (IMG_VOID *)(offset + prv->vstart);
}


static IMG_UINT64 CpuKmAddrToCpuPAddr(
    SYSMEM_Heap *  heap,
    IMG_VOID *     pvCpuKmAddr
)
{
    IMG_INT64 offset = 0LL;
    struct priv_params *prv = (struct priv_params *)heap->priv;

    IMG_ASSERT(((IMG_UINTPTR)pvCpuKmAddr >= prv->vstart) && ((IMG_UINTPTR)pvCpuKmAddr < (prv->vstart + prv->size)));
    offset = (IMG_UINTPTR)pvCpuKmAddr - prv->vstart;
    return offset + prv->pstart;
}

/*!
******************************************************************************

 @Function                SYSMEMKM_AllocPages

******************************************************************************/
static IMG_RESULT AllocPages(
    SYSMEM_Heap *     heap,
    IMG_UINT32        ui32Size,
    SYSMEMU_sPages *  psPages,
    SYS_eMemAttrib    eMemAttrib
)
{
    struct priv_params *  prv = (struct priv_params *)heap->priv;
    IMG_UINT64            ui64CpuPhysAddr;
    IMG_RESULT            ui32Result;
    IMG_UINT64 *          pCpuPhysAddrs;
    size_t                numPages, pg_i, offset;
    size_t                physAddrArrSize;

    /* Calculate required no. of pages...*/
    psPages->pvImplData = (IMG_VOID *)gen_pool_alloc(prv->pool, ui32Size);
    if(psPages->pvImplData == IMG_NULL)
    {
        return IMG_ERROR_OUT_OF_MEMORY;
    }

    ui64CpuPhysAddr = CpuKmAddrToCpuPAddr(heap, psPages->pvImplData);
    IMG_ASSERT(ui64CpuPhysAddr != 0);
    if (ui64CpuPhysAddr == 0)
    {
        ui32Result = IMG_ERROR_GENERIC_FAILURE;
        goto error_map;
    }

    numPages = (ui32Size + SYS_MMU_PAGE_SIZE - 1)/SYS_MMU_PAGE_SIZE;
    physAddrArrSize = sizeof(*pCpuPhysAddrs) * numPages;
    pCpuPhysAddrs = IMG_BIGORSMALL_ALLOC(physAddrArrSize);
    if (!pCpuPhysAddrs)
    {
        ui32Result = IMG_ERROR_OUT_OF_MEMORY;
        goto error_array_alloc;
    }
    for (pg_i = 0, offset = 0; pg_i < numPages; offset += SYS_MMU_PAGE_SIZE, ++pg_i)
    {
        pCpuPhysAddrs[pg_i] = ui64CpuPhysAddr + offset;
    }
    psPages->paPhysAddr = pCpuPhysAddrs;

    /* Add this to the list of mappable regions...*/
    ui32Result = SYSBRGU_CreateMappableRegion(psPages->paPhysAddr[0], ui32Size, eMemAttrib, IMG_FALSE, psPages, &psPages->hRegHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_region_create;
    }

#if defined (CLEAR_PAGES)
    IMG_MEMSET( psPages->pvImplData, 0, ui32Size);
#endif

    return IMG_SUCCESS;

    /* Error handling. */
error_region_create:
    IMG_BIGORSMALL_FREE(physAddrArrSize, psPages->paPhysAddr);
error_array_alloc:
error_map:
    gen_pool_free(prv->pool, (unsigned long)psPages->pvImplData, ui32Size);

    psPages->paPhysAddr = IMG_NULL;
    psPages->hRegHandle = IMG_NULL;

    return ui32Result;
}


/*!
******************************************************************************

 @Function              SYSMEMKM_ImportExternalPages

******************************************************************************/
static IMG_RESULT ImportExternalPages(
    SYSMEM_Heap *     heap,
    IMG_UINT32        ui32Size,
    SYSMEMU_sPages *  psPages,
    SYS_eMemAttrib    eMemAttrib,
    IMG_VOID *        pvCpuKmAddr,
    IMG_UINT64 *      pPhyAddrs
)
{
//    IMG_RESULT ui32Result;
    size_t  numPages;
    size_t  physAddrArrSize;
    size_t  phy_i;

    IMG_ASSERT(pvCpuKmAddr != IMG_NULL);

    /* Allocate page structure */
    psPages->bMappingOnly = IMG_TRUE;
    psPages->bImported    = IMG_TRUE;
    psPages->pvCpuKmAddr  = pvCpuKmAddr;
    //psPages->pvImplData = psPages->pvCpuKmAddr;

    numPages = (ui32Size + SYS_MMU_PAGE_SIZE - 1)/SYS_MMU_PAGE_SIZE;
    physAddrArrSize = sizeof *(psPages->paPhysAddr) * numPages;

    psPages->paPhysAddr = IMG_BIGORSMALL_ALLOC(physAddrArrSize);
    IMG_ASSERT(IMG_NULL != psPages->paPhysAddr);
    if(IMG_NULL == psPages->paPhysAddr)
    {
        return IMG_ERROR_OUT_OF_MEMORY;
    }
    for (phy_i = 0; phy_i < numPages; ++phy_i)
        psPages->paPhysAddr[phy_i] = pPhyAddrs[phy_i];

    //ui32Result = SYSBRGU_CreateMappableRegion(pvCpuKmAddr, pPhyAddrs[0], ui32Size, eMemAttrib,
    //                                   IMG_TRUE, &psPages->hRegHandle);
    psPages->hRegHandle = IMG_NULL;
    //IMG_ASSERT(ui32Result == IMG_SUCCESS);
    //if (ui32Result != IMG_SUCCESS)
    //{
    //    return ui32Result;
    //}

    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function                SYSMEMKM_ImportPages

******************************************************************************/
static IMG_RESULT ImportPages(
    SYSMEM_Heap *  heap,
    IMG_UINT32     ui32Size,
    IMG_HANDLE     hExtImportHandle,
    IMG_HANDLE *   phPagesHandle
)
{
    SYSMEMU_sPages *  psPages = hExtImportHandle;
    SYSMEMU_sPages *  psNewPages;

    /**** In this implementation we are importing buffers that we have allocated. */
    /* Allocate a new memory structure...*/
    psNewPages = IMG_MALLOC(sizeof(*psNewPages));
    IMG_ASSERT(psNewPages != IMG_NULL);
    if (psNewPages == IMG_NULL)
    {
        return IMG_ERROR_OUT_OF_MEMORY;
    }

    /* Take a copy of the original page allocation. */
    *psNewPages = *psPages;

    /* Set imported. */
    psNewPages->bImported    = IMG_TRUE;
    psNewPages->bMappingOnly = IMG_FALSE;

    *phPagesHandle = psNewPages;

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function                SYSMEMKM_GetCpuKmAddr

******************************************************************************/
static IMG_RESULT GetCpuKmAddr(
    SYSMEM_Heap *  heap,
    IMG_VOID **    ppvCpuKmAddr,
    IMG_HANDLE     hPagesHandle
)
{
    SYSMEMU_sPages *  psPages = hPagesHandle;

    if(psPages->pvCpuKmAddr == IMG_NULL)
    {
        psPages->pvCpuKmAddr = psPages->pvImplData;
    }

    *ppvCpuKmAddr = psPages->pvCpuKmAddr;
    /* Return success...*/
    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function                SYSMEMKM_FreePages

******************************************************************************/
static IMG_VOID FreePages(
    SYSMEM_Heap *  heap,
    IMG_HANDLE     hPagesHandle
)
{
    struct priv_params *  prv = (struct priv_params *)heap->priv;
    SYSMEMU_sPages *      psPages = hPagesHandle;
    IMG_UINT32            ui32NoPages;
    IMG_UINT32            physAddrArrSize;

    /* Calculate required no. of pages...*/
    ui32NoPages = (psPages->ui32Size + (SYS_MMU_PAGE_SIZE-1)) / SYS_MMU_PAGE_SIZE;

    /* If mapping then free on the copy of the page structure. */
    /* This will happen after we call this function by sysmemutils */
    if (psPages->bImported)
    {
        if(psPages->bMappingOnly && (IMG_NULL != psPages->paPhysAddr))
        {
            IMG_BIGORSMALL_FREE(ui32NoPages * sizeof(psPages->paPhysAddr[0]), psPages->paPhysAddr);
            psPages->paPhysAddr = IMG_NULL;
        }
        return;
    }

    IMG_ASSERT((!psPages->bMappingOnly));            /* Mapping is not supported via this implementation of the SYSMEM API. */

    /* Removed this from the list of mappable regions...*/
    SYSBRGU_DestroyMappableRegion(psPages->hRegHandle);

    /* Calculate the size of page table...*/
    physAddrArrSize = sizeof(psPages->paPhysAddr[0]) * ui32NoPages;

    /* Free memory...*/
    gen_pool_free(prv->pool, (unsigned long)psPages->pvImplData, psPages->ui32Size);
    IMG_BIGORSMALL_FREE(physAddrArrSize, psPages->paPhysAddr);
    psPages->paPhysAddr = IMG_NULL;
    psPages->hRegHandle = IMG_NULL;
}


static IMG_VOID UpdateMemory(
    SYSMEM_Heap *           heap,
    IMG_HANDLE              hPagesHandle,
    SYSMEM_UpdateDirection  dir
)
{
    return;
}

static SYSMEM_Ops carveout_ops = {
        .Initialise = Initialise,
        .Deinitialise = Deinitialise,

        .AllocatePages = AllocPages,
        .FreePages = FreePages,

        .GetCpuKmAddr = GetCpuKmAddr,
        .CpuKmAddrToCpuPAddr = CpuKmAddrToCpuPAddr,
        .CpuPAddrToCpuKmAddr = CpuPAddrToCpuKmAddr,

        .ImportExternalPages = ImportExternalPages,
        .ImportPages = ImportPages,
        .UpdateMemory = UpdateMemory
};


IMG_RESULT SYSMEMKM_AddCarveoutMemory(
    IMG_UINTPTR     vstart,
    IMG_UINT64      pstart,
    IMG_UINT32      size,
    SYS_eMemPool *  peMemPool
)
{
    IMG_RESULT ui32Result;
    struct priv_params *prv;
    struct gen_pool *pool = gen_pool_create(12, -1);

    prv = (struct priv_params *)IMG_MALLOC(sizeof(*prv));
    IMG_ASSERT(prv != IMG_NULL);
    if(IMG_NULL == prv)
    {
        ui32Result = IMG_ERROR_OUT_OF_MEMORY;
        goto error_priv_alloc;
    }
    IMG_MEMSET((void *)prv, 0, sizeof(*prv));

    IMG_ASSERT(pool != IMG_NULL);
    IMG_ASSERT(size != 0);
    IMG_ASSERT((vstart & (SYS_MMU_PAGE_SIZE-1)) == 0);
    gen_pool_add_virt(pool, (unsigned long)vstart, (unsigned long)pstart, size, -1);

    prv->pool = pool;
    prv->pstart = pstart;
    prv->size = size;
    prv->vstart = vstart;

    ui32Result = SYSMEMU_AddMemoryHeap(&carveout_ops, IMG_TRUE, (IMG_VOID *)prv, peMemPool);
    IMG_ASSERT(IMG_SUCCESS == ui32Result);
    if(IMG_SUCCESS != ui32Result)
    {
        goto error_heap_add;
    }

    return IMG_SUCCESS;

error_heap_add:
    IMG_FREE(prv);
error_priv_alloc:
    gen_pool_destroy(pool);

    return ui32Result;
}
IMGVIDEO_EXPORT_SYMBOL(SYSMEMKM_AddCarveoutMemory)
