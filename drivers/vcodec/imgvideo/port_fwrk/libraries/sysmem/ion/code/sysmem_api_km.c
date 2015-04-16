/*!
 *****************************************************************************
 *
 * @file       sysmem_api_km.c
 *
 * This file contains kernel mode implementation of the
 * System Memory Kernel Mode API, for devices with ION heap allocator.
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
#include "sysmem_api_km.h"
#define IMG_MEM_ERROR_INJECTION
#else
#include "sysmem_api_km.h"
#endif
#include "sysmem_utils.h"
#include "sysdev_utils1.h"
#include "sysbrg_utils.h"
#include "sysos_api_km.h"
#include <linux/mm.h>
#include <linux/version.h>

#include <linux/ion.h>
#include <linux/scatterlist.h>

#include "img_errors.h"
#include "report_api.h"


/* For unified memory, we will use the system heap. In case we have device memory,
   we will use the carveout heap. */
#ifdef IMG_MEMALLOC_UNIFIED_VMALLOC
#define USE_ION_SYSTEM_HEAP
#endif

#ifdef USE_ION_SYSTEM_HEAP
#define ION_HEAP_MASK ION_HEAP_SYSTEM_MASK
#define IS_KERNEL_ALLOCED IMG_TRUE
#else
#define ION_HEAP_MASK ION_HEAP_CARVEOUT_MASK
#define IS_KERNEL_ALLOCED IMG_FALSE
#endif


extern struct ion_device *ion_device_create(long (*custom_ioctl)
                                            (struct ion_client *client,
                                             unsigned int cmd,
                                             unsigned long arg));

/*!
******************************************************************************
 @Function                sysmemkm_GetIONClient
******************************************************************************/
static struct ion_client *sysmemkm_GetIONClient(void)
{
    // Works like a singleton
    static struct ion_client *psIONClient = NULL;

    struct ion_device *psIONDev = NULL;

    if (psIONClient)
        return psIONClient;

    // Returns the only device in the system (ION module patch)
    psIONDev = ion_device_create(NULL);
    IMG_ASSERT(psIONDev);
    if (psIONDev) {
        // ion_client_create has a cache to store a client already allocated for each task
        psIONClient = ion_client_create(psIONDev, "sysmem_ion_client");
        IMG_ASSERT(psIONClient);
    }

    return psIONClient;
}

/*!
******************************************************************************
 @Function                SYSMEMKM_Initialise
******************************************************************************/
static IMG_RESULT Initialise(IMG_VOID)
{
	return IMG_SUCCESS;
}

/*!
******************************************************************************
 @Function                SYSMEMKM_Deinitialise
******************************************************************************/
static IMG_VOID Deinitialise(IMG_VOID)
{
}

/*****************************************************************************
 @Function                SYSMEMKM_AllocPages
******************************************************************************/
static IMG_RESULT AllocPages(
	SYSMEM_Heap *		heap,
	IMG_UINT32			ui32Size,
	SYSMEMU_sPages *	psPages,
	SYS_eMemAttrib		eMemAttrib
)
{
    IMG_UINT32           Res;
    struct ion_handle *  ionHandle;
    unsigned             allocFlags;
    struct ion_client *  pIONcl;
    IMG_UINT64 *         pCpuPhysAddrs;
    size_t               numPages;
    size_t               physAddrArrSize;


    pIONcl = sysmemkm_GetIONClient();
    if (!pIONcl) {
        REPORT(REPORT_MODULE_SYSMEM, REPORT_ERR, "Error retrieving ion client");
        Res = IMG_ERROR_FATAL;
        goto errGetClient;
    }
    if (   (eMemAttrib & SYS_MEMATTRIB_WRITECOMBINE)
        || (eMemAttrib & SYS_MEMATTRIB_UNCACHED))
    {
        allocFlags = 0;
    } else {
        allocFlags = ION_FLAG_CACHED;
    }

    if (eMemAttrib == SYS_MEMATTRIB_UNCACHED)
        REPORT(REPORT_MODULE_SYSMEM, REPORT_WARNING,
               "Purely uncached memory is not supported by ION");

    // 4kb aligment, heap depends on platform
    ionHandle = ion_alloc(pIONcl, ui32Size, PAGE_SIZE,
                          ION_HEAP_MASK,
                          allocFlags);
    if (!ionHandle) {
        REPORT(REPORT_MODULE_SYSMEM, REPORT_ERR,
               "Error allocating %u bytes from ion", ui32Size);
        Res = IMG_ERROR_OUT_OF_MEMORY;
        goto errAlloc;
    }

    /* Find out physical addresses in the mappable region */
    numPages = (ui32Size + SYS_MMU_PAGE_SIZE - 1)/SYS_MMU_PAGE_SIZE;

    physAddrArrSize = sizeof *pCpuPhysAddrs * numPages;
    pCpuPhysAddrs = IMG_BIGORSMALL_ALLOC(physAddrArrSize);
    if (!pCpuPhysAddrs) {
        Res = IMG_ERROR_OUT_OF_MEMORY;
        goto errPhysArrAlloc;
    }
#ifdef USE_ION_SYSTEM_HEAP
    {
        struct scatterlist *psScattLs, *psScattLsAux;
        struct sg_table *psSgTable;
        size_t pg_i = 0;

        psSgTable = ion_sg_table(pIONcl, ionHandle);
        if (psSgTable == NULL)
        {
            REPORT(REPORT_MODULE_SYSMEM, REPORT_ERR, "Error obtaining sg table");
            Res = IMG_ERROR_FATAL;
            goto errGetPhys;
        }
        psScattLs = psSgTable->sgl;

        if (psScattLs == NULL)
        {
            REPORT(REPORT_MODULE_SYSMEM, REPORT_ERR, "Error obtaining scatter list");
            Res = IMG_ERROR_FATAL;
            goto errGetPhys;
        }

        // Get physical addresses from scatter list
        for (psScattLsAux = psScattLs; psScattLsAux; psScattLsAux = sg_next(psScattLsAux))
        {
            int offset;
            dma_addr_t chunkBase = sg_phys(psScattLsAux);

            for (offset = 0; offset < psScattLsAux->length; offset += PAGE_SIZE, ++pg_i)
            {
                if (pg_i >= numPages)
                    break;
                pCpuPhysAddrs[pg_i] = chunkBase + offset;
            }
            if (pg_i >= numPages)
                break;
        }
    }
#else
    {
        ion_phys_addr_t physaddr;
        size_t len = 0;
        size_t pg_i, offset;

        Res = ion_phys(pIONcl, ionHandle, &physaddr, &len);
        if (Res) {
            REPORT(REPORT_MODULE_SYSMEM, REPORT_ERR,
                   "Error %u from ion_phys", Res);
            Res = IMG_ERROR_FATAL;
            goto errGetPhys;
        }
        IMG_ASSERT(ui32Size == len);

        for (pg_i = 0, offset = 0; pg_i < numPages; offset += PAGE_SIZE, ++pg_i)
        {
            pCpuPhysAddrs[pg_i] = physaddr + offset;
        }
    }
#endif
    // Set pointer to physical address in structure
    psPages->paPhysAddr = pCpuPhysAddrs;

    DEBUG_REPORT(REPORT_MODULE_SYSMEM, "%s region of size %u phys 0x%llx",
                 __FUNCTION__, ui32Size, psPages->paPhysAddr[0]);

    Res = SYSBRGU_CreateMappableRegion(psPages->paPhysAddr[0], ui32Size, eMemAttrib,
                                       IS_KERNEL_ALLOCED, psPages, &psPages->hRegHandle);
    if (Res != IMG_SUCCESS) {
        REPORT(REPORT_MODULE_SYSMEM, REPORT_ERR,
               "Error %u in SYSBRGU_CreateMappableRegion", Res);
        goto errCreateMapRegion;
    }

    psPages->bIsContiguous = IMG_FALSE;
    psPages->pvImplData = ionHandle;

    return IMG_SUCCESS;

errCreateMapRegion:
errGetPhys:
    IMG_BIGORSMALL_FREE(numPages*sizeof(*pCpuPhysAddrs), pCpuPhysAddrs);
errPhysArrAlloc:
#ifdef USE_ION_SYSTEM_HEAP
    ion_unmap_kernel(pIONcl, ionHandle);
#endif
errMapKernel:
    ion_free(pIONcl, ionHandle);
errAlloc:
errGetClient:
errHandleAlloc:
    return Res;
}

/*!
******************************************************************************
 @Function              SYSMEMKM_ImportExternalPages
******************************************************************************/
IMG_RESULT ImportExternalPages(
	SYSMEM_Heap *	heap,
	IMG_UINT32      ui32Size,
	SYSMEMU_sPages *psPages,
	SYS_eMemAttrib  eMemAttrib,
	IMG_VOID *      pvCpuKmAddr,
    IMG_UINT64 *    pPhyAddrs
)
{
    SYSMEMU_sPages *  psPages;
    size_t            numPages = (ui32Size + SYS_MMU_PAGE_SIZE - 1)/SYS_MMU_PAGE_SIZE;
    size_t            physAddrArrSize = sizeof *(psPages->paPhysAddr) * numPages;
    size_t            phy_i;


    psPages->bMappingOnly = IMG_TRUE;
    psPages->bImported = IMG_TRUE;
    psPages->bIsContiguous = IMG_FALSE;
    psPages->pvCpuKmAddr = pvCpuKmAddr;

    psPages->paPhysAddr = IMG_BIGORSMALL_ALLOC(physAddrArrSize);
    for (phy_i = 0; phy_i < numPages; ++phy_i)
        psPages->paPhysAddr[phy_i] = pPhyAddrs[phy_i];

    /* No SYSBRGU_sMappableReg - SYSBRGU_CreateMappableRegion not called. */
    psPages->hRegHandle = NULL;


    return IMG_SUCCESS;
}

/*!
******************************************************************************
 @Function                SYSMEMKM_ImportPages
******************************************************************************/
static IMG_RESULT ImportPages(
	SYSMEM_Heap *			heap,
	IMG_UINT32				ui32Size,
	IMG_HANDLE				hExtImportHandle,
	IMG_HANDLE *			phPagesHandle
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
    psNewPages->bImported = IMG_TRUE;

    /* Return the address of the control block...*/
    *phPagesHandle = psNewPages;


    return IMG_SUCCESS;
}


/*!
******************************************************************************
 @Function                SYSMEMKM_GetCpuKmAddr
******************************************************************************/
static IMG_RESULT GetCpuKmAddr(
	SYSMEM_Heap *			heap,
	IMG_VOID **				ppvCpuKmAddr,
	IMG_HANDLE 				hPagesHandle
)
{
    SYSMEMU_sPages *  psPages = hPagesHandle;
    struct ion_client *  pIONcl;

    if(psPages->pvCpuKmAddr == IMG_NULL)
    {
        pIONcl = sysmemkm_GetIONClient();

        if (!pIONcl) {
            REPORT(REPORT_MODULE_SYSMEM, REPORT_ERR, "Error retrieving ion client");
            return;
        }

        #ifdef USE_ION_SYSTEM_HEAP
        psPages->pvCpuKmAddr = ion_map_kernel(pIONcl, psPages->pvImplData);
        #else
        psPages->pvCpuKmAddr = SYSOSKM_CpuPAddrToCpuKmAddr(pCpuPhysAddrs[0]);
        #endif
        if (!psPages->pvCpuKmAddr) {
            REPORT(REPORT_MODULE_SYSMEM, REPORT_ERR,
                   "Error mapping buffer to kernel address space");
            Res = IMG_ERROR_FATAL;
            goto errMapKernel;
        }
        
    }
    *ppvCpuKmAddr = psPages->pvCpuKmAddr;
    /* Return success...*/
    return IMG_SUCCESS;
}


/*****************************************************************************
 @Function                SYSMEMKM_FreePages
******************************************************************************/
static IMG_VOID FreePages(
	SYSMEM_Heap *			heap,
	IMG_HANDLE				hPagesHandle
)
{
    SYSMEMU_sPages *     psPages = hPagesHandle;
    struct ion_client *  pIONcl;
    struct ion_handle *  ionHandle;
    size_t               numPages;


    /* If mapping then free on the copy of the page structure. */
    /* This will happen by sysmemutils function */
    if (psPages->bImported)
    {
        return;
    }

    /* Mapping is not supported via this implementation of the SYSMEM API */
    IMG_ASSERT((!psPages->bMappingOnly));

    /* Remove from the list of mappable regions */
    SYSBRGU_DestroyMappableRegion(psPages->hRegHandle);

    /* Free array with physical addresses */
    numPages = (psPages->ui32Size + SYS_MMU_PAGE_SIZE - 1)/SYS_MMU_PAGE_SIZE;
    IMG_BIGORSMALL_FREE(numPages * sizeof(*psPages->paPhysAddr), psPages->paPhysAddr);

    /* Free memory */
    pIONcl = sysmemkm_GetIONClient();
    if (!pIONcl) {
        REPORT(REPORT_MODULE_SYSMEM, REPORT_ERR, "Error retrieving ion client");
        return;
    }
    ionHandle = psPages->pvImplData;
#ifdef USE_ION_SYSTEM_HEAP
    if (psPages->pvCpuKmAddr)
        ion_unmap_kernel(pIONcl, ionHandle);
#endif
    ion_free(pIONcl, ionHandle);

}


static SYSMEM_Ops ion_ops = {
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

SYS_eMemPool SYSMEMKM_AddIONMemory() {
	static SYS_eMemPool memPool;
	static IMG_BOOL initialized = IMG_FALSE;

	/* Only one system heap is allowed to be inserted - obviously - */
	if(initialized)
		return memPool;

	initialized = IMG_TRUE;
	memPool = SYSMEMU_AddMemoryHeap(&ion_ops, IMG_TRUE, IMG_NULL);
	return memPool;
}
