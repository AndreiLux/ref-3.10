/*!
 *****************************************************************************
 *
 * @file	   page_alloc_ion.c
 *
 * This file contains auxiliary function to use ION as memory allocator for page_alloc
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

#include <linux/fdtable.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
/* Modify by g00260377 for DTS2014032707330 2014.03.27 begin*/
#include <linux/hisi_ion.h>
/* Modify by g00260377 for DTS2014032707330 2014.03.27 end*/
#include <linux/scatterlist.h>
#include <system.h>

#include <sysmem_utils.h>

/* Modify by g00260377 for DTS2014032707330 2014.03.27 begin*/
extern struct ion_client *hisi_ion_client_create(const char *name);
/* Modify by g00260377 for DTS2014032707330 2014.03.27 end*/

#include "img_errors.h"
#include "page_alloc_ion.h"
#include "report_api.h"


#define PALLOC_ION "palloc_ion"

/* For unified memory, we will use the system heap. In case we have device memory,
   we will use the carveout heap. */
#ifdef IMG_MEMALLOC_UNIFIED_VMALLOC
#define ION_SYSTEM_HEAP
#endif


/*!
******************************************************************************

 @Function              palloc_GetIONClient

******************************************************************************/
#ifdef ANDROID_ION_BUFFERS
static struct ion_client *palloc_GetIONClient(void)
{
    // Multi-process? Per-process info in kernel?
    struct ion_device *psIONDev = NULL;
    static struct ion_client *psIONClient = NULL;

/* Modify by g00260377 for DTS2014032707330 2014.03.27 begin*/
    if(!psIONClient)
        psIONClient = hisi_ion_client_create("vcodec_ion_client");
/* Modify by g00260377 for DTS2014032707330 2014.03.27 end*/
    return psIONClient;
}
#endif  // ANDROID_ION_BUFFERS


/*!
******************************************************************************
 @Function              palloc_GetIONPages
******************************************************************************/
#ifdef ANDROID_ION_BUFFERS
IMG_RESULT palloc_GetIONPages(SYS_eMemPool eMemPool, int buff_fd, IMG_UINT uiSize,
                              IMG_SYS_PHYADDR* pCpuPhysAddrs, IMG_PVOID *ppvKernAddr,
                              IMG_HANDLE *phBuf)
{
    struct ion_handle *ionHandle;
    IMG_RESULT result = IMG_ERROR_FATAL;
    unsigned numPages, pg_i = 0;
    struct ion_client *pIONcl;

    /* Modify by g00260377 for DTS2014032707330 2014.03.27 begin*/
    //REPORT(PALLOC_ION, REPORT_INFO, "Importing buff_fd %d of size %u", buff_fd, uiSize);
    /* Modify by g00260377 for DTS2014032707330 2014.03.27 end*/

    pIONcl = palloc_GetIONClient();
    if (!pIONcl)
        goto exitFailGetClient;

    ionHandle = ion_import_dma_buf(pIONcl, buff_fd);
    if (ionHandle == NULL) {
        REPORT(PALLOC_ION, REPORT_ERR, "Error obtaining handle from fd %d", buff_fd);
        result = IMG_ERROR_FATAL;
        goto exitFailImportFD;
    }
    *phBuf = ionHandle;

    numPages = (uiSize + PAGE_SIZE - 1)/PAGE_SIZE;

#if defined(ION_SYSTEM_HEAP)
    {
        struct scatterlist *psScattLs, *psScattLsAux;
        struct sg_table *psSgTable;
        psSgTable = ion_sg_table(pIONcl, ionHandle);
        if (psSgTable == NULL)
        {
            REPORT(PALLOC_ION, REPORT_ERR, "Error obtaining sg table");
            result = IMG_ERROR_FATAL;
            goto exitFailMap;
        }
        psScattLs = psSgTable->sgl;

    if (psScattLs == NULL)
    {
        REPORT(PALLOC_ION, REPORT_ERR, "Error obtaining scatter list");
        result = IMG_ERROR_FATAL;
        goto exitFailMap;
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
        if (ppvKernAddr)
            *ppvKernAddr = ion_map_kernel(pIONcl, ionHandle);
    }
#else
    {
		int offset;
		ion_phys_addr_t physaddr;
		size_t len = 0;

		result = ion_phys(pIONcl, ionHandle, &physaddr, &len);
		if(result)
		{
			IMG_ASSERT(!"ion_phys failed");
			result = IMG_ERROR_FATAL;
			goto exitFailMap;
		}

		for (offset = 0; pg_i < numPages; offset += PAGE_SIZE, ++pg_i)
		{
			if (pg_i >= numPages)
				break;
            pCpuPhysAddrs[pg_i] = physaddr + offset;
		}
        if (ppvKernAddr)
            *ppvKernAddr = SYSMEMU_CpuPAddrToCpuKmAddr(eMemPool, physaddr);
    }
#endif

    if (ppvKernAddr && *ppvKernAddr == NULL) {
		REPORT(PALLOC_ION, REPORT_ERR, "Error mapping to kernel address");
		result = IMG_ERROR_FATAL;
		goto exitFailMapKernel;
	}

    result = IMG_SUCCESS;


exitFailMapKernel:
exitFailMap:
exitFailImportFD:
exitFailGetClient:
    return result;
}
#else   // ANDROID_ION_BUFFERS
IMG_RESULT palloc_GetIONPages(SYS_eMemPool eMemPool, int buff_fd, IMG_UINT uiSize,
                              IMG_SYS_PHYADDR* pCpuPhysAddrs, IMG_PVOID *ppvKernAddr,
                              IMG_HANDLE *phBuf)
{
    IMG_ASSERT(!"palloc_GetIONPages Not compiled with support for ION");
    return IMG_ERROR_FATAL;
}
#endif  // ANDROID_ION_BUFFERS


/*!
******************************************************************************
 @Function              palloc_ReleaseIONBuf
******************************************************************************/
#ifdef ANDROID_ION_BUFFERS
IMG_RESULT palloc_ReleaseIONBuf(IMG_HANDLE hBuf, IMG_PVOID pvKernAddr)
{
    IMG_RESULT result = IMG_ERROR_FATAL;
    struct ion_client *pIONcl;
    struct ion_handle *ionHandle;

    /* Modify by g00260377 for DTS2014032707330 2014.03.27 begin*/
    //REPORT(PALLOC_ION, REPORT_INFO, "Releasing ion_handle 0x%p", hBuf);
    /* Modify by g00260377 for DTS2014032707330 2014.03.27 end*/

    IMG_ASSERT(hBuf);
    ionHandle = (struct ion_handle *) hBuf;

    pIONcl = palloc_GetIONClient();
    if (pIONcl) {
#if defined(ION_SYSTEM_HEAP)
        if (pvKernAddr)
        ion_unmap_kernel(pIONcl, ionHandle);
#endif
        ion_free(pIONcl, ionHandle);
        result = IMG_SUCCESS;
    } else {
        REPORT(PALLOC_ION, REPORT_ERR, "Releasing cannot find ION client");
    }

    return result;
}
#else   // ANDROID_ION_BUFFERS
IMG_RESULT palloc_ReleaseIONBuf(IMG_HANDLE hBuf, IMG_PVOID pvKernAddr)
{
    IMG_ASSERT(!"palloc_ReleaseIONBuf Not compiled with support for ION");
    return IMG_ERROR_FATAL;    
}
#endif  // ANDROID_ION_BUFFERS
