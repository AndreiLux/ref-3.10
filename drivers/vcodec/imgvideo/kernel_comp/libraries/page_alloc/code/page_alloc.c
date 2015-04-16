/*!
 *****************************************************************************
 *
 * @file	   page_alloc.c
 *
 * This file contains the Page Allocator.
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
#include "img_types.h"
#include "img_errors.h"
#include "page_alloc_log.h"
#include "page_alloc.h"
#include "page_alloc_km.h"
#include "dman_api_km.h"
#include "rman_api.h"
#include "sysmem_utils.h"
#include "sysmem_api_km.h"
#include "sysdev_api_km.h"
#include "sysdev_utils.h"
#include "sysos_api_km.h"
#include "report_api.h"
#include "sysbrg_utils.h"
#include "sysmem_utils.h"
#include "sysmem_api_km.h"
#if defined ANDROID_ION_BUFFERS
# include "page_alloc_ion.h"
#endif


#define PALLOC_RES_TYPE_1    (0x09090001)    /*!< Resource type */


/*!
******************************************************************************
 This type defines the possible buffer types
 @brief  Buffer type
******************************************************************************/
typedef enum
{
    PALLOC_BUFTYPE_PALLOCATED,    /*!< Allocated by PALLOC    */
    PALLOC_BUFTYPE_USERALLOC,     /*!< Allocated in user mode */
    PALLOC_BUFTYPE_ANDROIDNATIVE, /*!< Android native buffer  */

} PALLOC_eBufType;

/*!
******************************************************************************
 This structure contains attachment information.
******************************************************************************/
typedef struct
{
    IMG_HANDLE  hDevHandle;     /*<! Device handle          */
    IMG_HANDLE  hResBHandle;    /*<! Resource bucket handle */
    IMG_HANDLE  hSysDevHandle;  /*<! SYSDEVKN device handle */

} PALLOC_sAttachContext;

/*!
******************************************************************************
 This structure contains allocation information.
******************************************************************************/
typedef struct
{
    IMG_HANDLE           hDevHandle;    /*!< DMAN device handle             */
    IMG_HANDLE           hPagesHandle;  /*!< SYSMEM handle                  */
    PALLOCKM_sAllocInfo  sAllocInfo;    /*!< Allocation information         */
    IMG_HANDLE           hBufHandle;    /*!< Handle to specific buffer data */
    PALLOC_eBufType      eBufType;      /*!< Buffer type                    */

} PALLOC_sKmAlloc;

/*!
******************************************************************************

 @Function                PALLOC_Initialise1

******************************************************************************/
IMG_RESULT PALLOC_Initialise1(IMG_VOID)
{
    LOG_EVENT(PALLOC, PALLOC_INITIALISE, (LOG_FLAG_START), 0, 0);

    LOG_EVENT(PALLOC, PALLOC_INITIALISE, (LOG_FLAG_END), 0, 0);

    /* Return success...*/
    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function                PALLOC_Deinitialise1

******************************************************************************/
IMG_VOID PALLOC_Deinitialise1(IMG_VOID)
{
    LOG_EVENT(PALLOC, PALLOC_DEINITIALISE, (LOG_FLAG_START), 0, 0);

    LOG_EVENT(PALLOC, PALLOC_DEINITIALISE, (LOG_FLAG_END), 0, 0);
}


/*!
******************************************************************************

 @Function              palloc_fnCompConnect

******************************************************************************/
static IMG_RESULT palloc_fnCompConnect (
    IMG_HANDLE   hAttachHandle,
    IMG_VOID **  ppvCompAttachmentData
)
{
    PALLOC_sAttachContext *  psAttachContext;
    IMG_UINT32               ui32Result;
    IMG_CHAR *               pszDeviceName;

    LOG_EVENT(PALLOC, PALLOC_COMPCONNECT, (LOG_FLAG_START), 0, 0);

    /* Allocate attachment context structure...*/
    psAttachContext = IMG_MALLOC(sizeof(*psAttachContext));
    IMG_ASSERT(psAttachContext != IMG_NULL);
    if (psAttachContext == IMG_NULL)
    {
        return IMG_ERROR_OUT_OF_MEMORY;
    }
    IMG_MEMSET(psAttachContext, 0, sizeof(*psAttachContext));

    /* Ensure the resource manager is initialised...*/
    ui32Result = RMAN_Initialise();
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_rman_init;
    }

    /* Create a bucket for the resources...*/
    ui32Result = RMAN_CreateBucket(&psAttachContext->hResBHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_rman_bucket;
    }

    /* Get device information...*/
    psAttachContext->hDevHandle = DMANKM_GetDevHandleFromAttach(hAttachHandle);
    pszDeviceName = DMANKM_GetDeviceName(psAttachContext->hDevHandle);
    ui32Result = SYSDEVKM_OpenDevice(pszDeviceName, &psAttachContext->hSysDevHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_sysdev_open;
    }

    /* Return attachment context...*/
    *ppvCompAttachmentData = psAttachContext;

    LOG_EVENT(PALLOC, PALLOC_COMPCONNECT, (LOG_FLAG_END), 0, 0);

    /* Return success...*/
    return IMG_SUCCESS;

    /* Error handling. */
error_sysdev_open:
    RMAN_DestroyBucket(psAttachContext->hResBHandle);
error_rman_bucket:
error_rman_init:
    IMG_FREE(psAttachContext);

    return ui32Result;
}

/*!
******************************************************************************

 @Function              palloc_fnCompDisconnect

******************************************************************************/
static IMG_RESULT palloc_fnCompDisconnect (
    IMG_HANDLE  hAttachHandle,
    IMG_VOID *  pvCompAttachmentData
)
{
    PALLOC_sAttachContext *  psAttachContext = pvCompAttachmentData;

    LOG_EVENT(PALLOC, PALLOC_COMPDISCONNECT, (LOG_FLAG_START), 0, 0);

    /* Destroy the bucket and it's resources...*/
    RMAN_DestroyBucket(psAttachContext->hResBHandle);

    /* If we opened a device...*/
    if (psAttachContext->hSysDevHandle != IMG_NULL)
    {
        SYSDEVKM_CloseDevice(psAttachContext->hSysDevHandle);
    }

    /* Free attachment context...*/
    IMG_FREE(psAttachContext);

    LOG_EVENT(PALLOC, PALLOC_COMPDISCONNECT, (LOG_FLAG_END), 0, 0);

    /* Return success...*/
    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function              palloc_fnCompAttach

******************************************************************************/
static IMG_RESULT palloc_fnCompAttach(
    IMG_HANDLE            hConnHandle,
    DMANKM_sCompAttach *  psCompAttach
)
{
    LOG_EVENT(PALLOC, PALLOC_COMPATTACH, (LOG_FLAG_START), 0, 0);

    psCompAttach->pfnCompConnect    = palloc_fnCompConnect;
    psCompAttach->pfnCompDisconnect = palloc_fnCompDisconnect;

    LOG_EVENT(PALLOC, PALLOC_COMPATTACH, (LOG_FLAG_END), 0, 0);

    /* Return success...*/
    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function                PALLOC_AttachToConnection

******************************************************************************/
IMG_RESULT PALLOC_AttachToConnection(
    IMG_UINT32           ui32ConnId,
    IMG_UINT32 __user *  pui32AttachId
)
{
    IMG_HANDLE  hDevHandle;
    IMG_UINT32  ui32Result;
    IMG_HANDLE  hConnHandle;
    IMG_UINT32  ui32AttachId;

    LOG_EVENT(PALLOC, PALLOC_ATTACH, (LOG_FLAG_START), 0, 0);

    /* Get the connection handle from it's ID...*/
    ui32Result = DMANKM_GetConnHandleFromId(ui32ConnId, &hConnHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Get the device handle from the connection...*/
    hDevHandle = DMANKM_GetDevHandleFromConn(hConnHandle);

    /* Lock the device...*/
    DMANKM_LockDeviceContext(hDevHandle);

    /* Call on to the kernel function...*/
    ui32Result = DMANKM_AttachComponent(hConnHandle, "PALLOCBRG", palloc_fnCompAttach, IMG_NULL, &ui32AttachId);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);

    /* Unlock the device...*/
    DMANKM_UnlockDeviceContext(hDevHandle);

    SYSOSKM_CopyToUser(pui32AttachId, &ui32AttachId, sizeof(ui32AttachId));
    LOG_EVENT(PALLOC, PALLOC_ATTACH, (LOG_FLAG_END), 0, 0);

    /* Return ui32Result...*/
    return ui32Result;
}


/*!
******************************************************************************

 @Function             palloc_fnFree

******************************************************************************/
static IMG_VOID palloc_fnFree(
    IMG_VOID *  pvParam
)
{
    PALLOC_sKmAlloc *psKmAlloc = (PALLOC_sKmAlloc *) pvParam;
    IMG_UINT numPages;

    LOG_EVENT(PALLOC, PALLOC_FREE, LOG_FLAG_START | LOG_FLAG_QUAL_ARG1,
              (IMG_UINT32) (IMG_UINTPTR) pvParam, 0);

    numPages = (psKmAlloc->sAllocInfo.ui32Size + SYS_MMU_PAGE_SIZE - 1)/SYS_MMU_PAGE_SIZE;
    switch (psKmAlloc->eBufType) {
    case PALLOC_BUFTYPE_PALLOCATED:
        /* If this is not a mapping only */
        if (!psKmAlloc->sAllocInfo.bMappingOnly)
        {
            /* Free pages */
            SYSMEMU_FreePages(psKmAlloc->hPagesHandle);
        }
        break;
    case PALLOC_BUFTYPE_USERALLOC:
        SYSOSKM_ReleaseCpuPAddrArray(((SYSMEMU_sPages *) psKmAlloc->hPagesHandle)->pvCpuKmAddr,
                                     psKmAlloc->hBufHandle,
                                     psKmAlloc->sAllocInfo.psSysPAddr, numPages);
        SYSMEMU_FreePages(psKmAlloc->hPagesHandle);
        break;
    case PALLOC_BUFTYPE_ANDROIDNATIVE:
        #if defined ANDROID_ION_BUFFERS
            palloc_ReleaseIONBuf(psKmAlloc->hBufHandle, NULL);
            SYSMEMU_FreePages(psKmAlloc->hPagesHandle);
        #elif defined ANDROID   // Default gralloc: ashmem
            SYSOSKM_ReleaseCpuPAddrArray(((SYSMEMU_sPages *) psKmAlloc->hPagesHandle)->pvCpuKmAddr,
                                         psKmAlloc->hBufHandle,
                                         psKmAlloc->sAllocInfo.psSysPAddr, numPages);
            SYSMEMU_FreePages(psKmAlloc->hPagesHandle);
        #else
            IMG_ASSERT(!"palloc_fnFree wrong buffer type");
        #endif
        break;
    default:
        IMG_ASSERT(!"palloc_fnFree wrong buffer type");
    }

    IMG_BIGORSMALL_FREE(numPages * sizeof(IMG_SYS_PHYADDR), psKmAlloc->sAllocInfo.psSysPAddr);

    /* Free this structure */
    IMG_FREE(psKmAlloc);

    LOG_EVENT(PALLOC, PALLOC_FREE, LOG_FLAG_END | LOG_FLAG_QUAL_ARG1,
              (IMG_UINT32) (IMG_UINTPTR) pvParam, 0);
}


/*!
******************************************************************************

 @Function                PALLOC_Alloc1

******************************************************************************/
IMG_RESULT PALLOC_Alloc1(
    IMG_UINT32                ui32AttachId,
    SYS_eMemAttrib            eMemAttrib,
    PALLOC_sUmAlloc __user *  psUmAlloc
)
{
    IMG_HANDLE               hDevHandle;
    IMG_UINT32               ui32Result;
    PALLOC_sKmAlloc *        psKmAlloc;
    IMG_HANDLE               hAttachHandle;
    PALLOC_sAttachContext *  psAttachContext;
    IMG_UINT32               ui32PageNo;
    PALLOC_sUmAlloc          sUmAllocCp;
    IMG_UINT32               ui32PageIdx;
    IMG_UINT64 *             pui64Phys;
    SYSMEMU_sPages *         psSysMem;
    SYS_eMemPool             eMemPool;
    SYSDEVU_sInfo *          psSysDev;
    /* the following code assumes that IMG_SYS_PHYADDR and IMG_UINT64 are the same size */

#ifndef SYSBRG_BRIDGING
    IMG_VOID *               pvKmAddr;
#endif

    if (SYSOSKM_CopyFromUser(&sUmAllocCp, psUmAlloc, sizeof(sUmAllocCp)) != IMG_SUCCESS)
    {
        return IMG_ERROR_FATAL;
    }

    LOG_EVENT(PALLOC, PALLOC_ALLOC, (LOG_FLAG_START | LOG_FLAG_QUAL_ARG1 |LOG_FLAG_QUAL_ARG2), ui32AttachId, sUmAllocCp.ui32Size);

    IMG_ASSERT(!sUmAllocCp.bMappingOnly);

    /* Get the attachment handle from its ID...*/
    ui32Result = DMANKM_GetAttachHandleFromId(ui32AttachId, &hAttachHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Get access to the attachment specific data...*/
    psAttachContext = DMANKM_GetCompAttachmentData(hAttachHandle);

    /* Get access to the device handle...*/
    hDevHandle = DMANKM_GetDevHandleFromAttach(hAttachHandle);

    /* Lock the device...*/
    DMANKM_LockDeviceContext(hDevHandle);

    psSysDev = SYSDEVU_GetDeviceById(SYSDEVKM_GetDeviceID(psAttachContext->hSysDevHandle));
    IMG_ASSERT(psSysDev != IMG_NULL); // I
    if (psSysDev == IMG_NULL)
    {
        return IMG_ERROR_DEVICE_NOT_FOUND;
    }

    eMemPool = (eMemAttrib & SYS_MEMATTRIB_SECURE) ? psSysDev->secureMemPool : psSysDev->sMemPool;

    /* Allocate allocation info...*/
    psKmAlloc = IMG_MALLOC(sizeof(*psKmAlloc));
    IMG_ASSERT(psKmAlloc != IMG_NULL);
    if (psKmAlloc == IMG_NULL)
    {
        ui32Result = IMG_ERROR_OUT_OF_MEMORY;
        goto error_alloc_info;
    }
    IMG_MEMSET(psKmAlloc, 0, sizeof(*psKmAlloc));

    /* Save device handle etc...*/
    psKmAlloc->hDevHandle          = hDevHandle;
    psKmAlloc->sAllocInfo.ui32Size = sUmAllocCp.ui32Size;
    psKmAlloc->hBufHandle          = NULL;
    psKmAlloc->eBufType            = PALLOC_BUFTYPE_PALLOCATED;

    /* Allocate pages...*/
    ui32Result = SYSMEMU_AllocatePages(sUmAllocCp.ui32Size, eMemAttrib, eMemPool,
                                       &psKmAlloc->hPagesHandle, &pui64Phys);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_alloc_pages;
    }

#ifndef SYSBRG_BRIDGING
    SYSMEMU_GetCpuKmAddr(&pvKmAddr, psKmAlloc->hPagesHandle);
    IMG_ASSERT(pvKmAddr != IMG_NULL);
    if(pvKmAddr == IMG_NULL)
    {
        ui32Result = IMG_ERROR_FATAL;
        goto error_cpu_km_addr;
    }
#endif

    /* Return addresses...*/
    psSysMem = psKmAlloc->hPagesHandle;


#ifdef PALLOC_EXPOSE_KM_HANDLE
    sUmAllocCp.hKmAllocHandle = psKmAlloc->hPagesHandle;
#endif

    /* Check if contiguous...*/
    psKmAlloc->sAllocInfo.bIsContiguous = SYSMEMKM_IsContiguous(psKmAlloc->hPagesHandle);

    /* Get the device id...*/
    ui32Result = DMANKM_GetDeviceId(hDevHandle, &sUmAllocCp.ui32DeviceId);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_get_dev_id;
    }

    sUmAllocCp.lOffset = 0;
    if (psSysMem->hRegHandle)
    {
        // Determine the offset to memory if it has been made mappable in UM.
        sUmAllocCp.lOffset = (long)pui64Phys[0];
    }

    /* Calculate the size of the allocation in pages...*/
    ui32PageNo = (sUmAllocCp.ui32Size + SYS_MMU_PAGE_SIZE - 1)/SYS_MMU_PAGE_SIZE;
    psKmAlloc->sAllocInfo.psSysPAddr = IMG_BIGORSMALL_ALLOC(sizeof(IMG_SYS_PHYADDR) * ui32PageNo);
    IMG_ASSERT(psKmAlloc->sAllocInfo.psSysPAddr);
    if (IMG_NULL == psKmAlloc->sAllocInfo.psSysPAddr)
    {
        ui32Result = IMG_ERROR_OUT_OF_MEMORY;
        goto error_page_array;
    }
    IMG_MEMSET(psKmAlloc->sAllocInfo.psSysPAddr, 0, sizeof(IMG_SYS_PHYADDR) * ui32PageNo);

    for (ui32PageIdx = 0; ui32PageIdx < ui32PageNo; ++ui32PageIdx)
    {
        psKmAlloc->sAllocInfo.psSysPAddr[ui32PageIdx] = pui64Phys[ui32PageIdx];
    }

    /* Register this with the resource manager...*/
    ui32Result = RMAN_RegisterResource(psAttachContext->hResBHandle, PALLOC_RES_TYPE_1,
                                       palloc_fnFree, psKmAlloc, IMG_NULL, &sUmAllocCp.ui32AllocId);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_resource_register;
    }

    LOG_EVENT(PALLOC, PALLOC_ALLOCID, (LOG_FLAG_END | LOG_FLAG_QUAL_ARG1 |LOG_FLAG_QUAL_ARG2),
              ui32AttachId, sUmAllocCp.ui32AllocId);

    /* Unlock the device...*/
    DMANKM_UnlockDeviceContext(hDevHandle);

    /* Copy to user changed PALLOC_sUmAlloc, including physical device addresses */
    if (SYSOSKM_CopyToUser(psUmAlloc, &sUmAllocCp, sizeof(sUmAllocCp)))
    {
        ui32Result = IMG_ERROR_FATAL;
        goto error_copy_to_user;
    }
    if (SYSOSKM_CopyToUser(psUmAlloc->aui64DevPAddr, psKmAlloc->sAllocInfo.psSysPAddr,
                           sizeof(psKmAlloc->sAllocInfo.psSysPAddr[0]) * ui32PageNo))
    {
        ui32Result = IMG_ERROR_FATAL;
        goto error_copy_to_user;
    }

    LOG_EVENT(PALLOC, PALLOC_ALLOC, (LOG_FLAG_END | LOG_FLAG_QUAL_ARG1 |LOG_FLAG_QUAL_ARG2),
              ui32AttachId, sUmAllocCp.ui32Size);

    /* Return. */
    return IMG_SUCCESS;

    /* Error handling. */
error_copy_to_user:
    /* Free everything. */
    PALLOC_Free1(sUmAllocCp.ui32AllocId);
    goto error_return;

error_resource_register:
    IMG_BIGORSMALL_FREE(sizeof(IMG_SYS_PHYADDR) * ui32PageNo,
                        psKmAlloc->sAllocInfo.psSysPAddr);
error_page_array:
error_get_dev_id:
#ifndef SYSBRG_BRIDGING
error_cpu_km_addr:
#endif /* SYSBRG_BRIDGING */
    SYSMEMU_FreePages(psKmAlloc->hPagesHandle);
error_alloc_pages:
    IMG_FREE(psKmAlloc);
error_alloc_info:
    /* Unlock the device. */
    DMANKM_UnlockDeviceContext(hDevHandle);

error_return:
    return ui32Result;
}


/*!
******************************************************************************

 @Function              PALLOC_Import1

******************************************************************************/
IMG_RESULT PALLOC_Import1(
    IMG_UINT32                ui32AttachId,
    SYS_eMemAttrib            eMemAttrib,
    int                       buff_fd,
    PALLOC_sUmAlloc __user *  psUmAlloc
)
{
    IMG_HANDLE               hDevHandle;
    IMG_UINT32               ui32Result;
    PALLOC_sKmAlloc *        psKmAlloc;
    IMG_HANDLE               hAttachHandle;
    PALLOC_sAttachContext *  psAttachContext;
    IMG_UINT32               ui32PageNo;
    IMG_UINT32               ui32PageIdx;
    IMG_UINT64               ui64CpuPAddr;
    PALLOC_sUmAlloc          sUmAllocCp;
    IMG_UINT64 *             paui64DevAddrs;
    SYSDEVU_sInfo *          psSysDev;
    SYS_eMemPool             eMemPool;
    IMG_PVOID                pvCpuKmAddr;

    LOG_EVENT(PALLOC, PALLOC_IMPORT, LOG_FLAG_START | LOG_FLAG_QUAL_ARG1 | LOG_FLAG_QUAL_ARG2,
              ui32AttachId, buff_fd);

    DEBUG_REPORT(REPORT_MODULE_PALLOC, "PALLOC_Import1 fd %d", buff_fd);

    if (SYSOSKM_CopyFromUser(&sUmAllocCp, psUmAlloc, sizeof sUmAllocCp) != IMG_SUCCESS)
    {
        return IMG_ERROR_FATAL;
    }

    IMG_ASSERT(sUmAllocCp.bMappingOnly);

    /* Get the attachment handle from its ID... */
    ui32Result = DMANKM_GetAttachHandleFromId(ui32AttachId, &hAttachHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Get access to the attachment specific data...*/
    psAttachContext = DMANKM_GetCompAttachmentData(hAttachHandle);

    /* Get access to the device handle...*/
    hDevHandle = DMANKM_GetDevHandleFromAttach(hAttachHandle);

    /* Lock the device...*/
    DMANKM_LockDeviceContext(hDevHandle);

    psSysDev = SYSDEVU_GetDeviceById(SYSDEVKM_GetDeviceID(psAttachContext->hSysDevHandle));
    IMG_ASSERT(psSysDev != IMG_NULL); // I
    if (psSysDev == IMG_NULL)
    {
        ui32Result = IMG_ERROR_DEVICE_NOT_FOUND;
        goto error_get_dev_by_id;
    }

    eMemPool = (eMemAttrib & SYS_MEMATTRIB_SECURE) ? psSysDev->secureMemPool : psSysDev->sMemPool;

    /* Allocate allocation info...*/
    psKmAlloc = IMG_MALLOC(sizeof *psKmAlloc);
    IMG_ASSERT(psKmAlloc != IMG_NULL);
    if (psKmAlloc == IMG_NULL)
    {
        ui32Result = IMG_ERROR_OUT_OF_MEMORY;
        goto error_alloc_info;
    }
    IMG_MEMSET(psKmAlloc, 0, sizeof *psKmAlloc);

    /* Save device handle etc... */
    psKmAlloc->hDevHandle = hDevHandle;
    psKmAlloc->sAllocInfo.ui32Size = sUmAllocCp.ui32Size;
    psKmAlloc->sAllocInfo.bIsContiguous = IMG_FALSE;

    /* Get the device id...*/
    ui32Result = DMANKM_GetDeviceId(hDevHandle, &sUmAllocCp.ui32DeviceId);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_get_dev_id;
    }

    psKmAlloc->sAllocInfo.bMappingOnly = IMG_TRUE;

    /* Calculate the size of the allocation in pages */
    ui32PageNo = (sUmAllocCp.ui32Size + SYS_MMU_PAGE_SIZE - 1)/SYS_MMU_PAGE_SIZE;

    psKmAlloc->sAllocInfo.psSysPAddr = IMG_BIGORSMALL_ALLOC(sizeof(IMG_SYS_PHYADDR) * ui32PageNo);
    IMG_ASSERT(psKmAlloc->sAllocInfo.psSysPAddr);
    if (IMG_NULL == psKmAlloc->sAllocInfo.psSysPAddr)
    {
        ui32Result = IMG_ERROR_OUT_OF_MEMORY;
        goto error_page_array;
    }
    paui64DevAddrs = IMG_BIGORSMALL_ALLOC((sizeof *paui64DevAddrs) * ui32PageNo);
    IMG_ASSERT(paui64DevAddrs);
    if (IMG_NULL == paui64DevAddrs)
    {
        ui32Result = IMG_ERROR_OUT_OF_MEMORY;
        goto error_addr_array;
    }

    if(buff_fd >= 0)
    {
        pvCpuKmAddr = NULL;
        /* ION buffer */
#if defined ANDROID_ION_BUFFERS
        psKmAlloc->eBufType = PALLOC_BUFTYPE_ANDROIDNATIVE;
    #if defined CONFIG_X86
        ui32Result = palloc_GetIONPages(eMemPool, buff_fd, sUmAllocCp.ui32Size,
                                        psKmAlloc->sAllocInfo.psSysPAddr,
                                        &pvCpuKmAddr,
                                        &psKmAlloc->hBufHandle);
    #else // if CONFIG_X86
        ui32Result = palloc_GetIONPages(eMemPool, buff_fd, sUmAllocCp.ui32Size,
                                        psKmAlloc->sAllocInfo.psSysPAddr,
                                        NULL,
                                        &psKmAlloc->hBufHandle);
    #endif // if CONFIG_X86
        if (ui32Result != IMG_SUCCESS)
        {
            IMG_ASSERT(!"palloc_GetIONPages");
            goto error_get_pages;
        }
#else   // if ANDROID_ION_BUFFERS
        IMG_ASSERT(!"NOT ANDROID: ION not supported");
        goto error_get_pages;
#endif  // if ANDROID_ION_BUFFERS
    }
    else
    {
        /* User space allocated buffer */
        IMG_VOID __user *  pvUmBuff = ( IMG_VOID __user * ) sUmAllocCp.pvCpuUmAddr;
        IMG_ASSERT(pvUmBuff);
        psKmAlloc->hBufHandle = (IMG_HANDLE)(sUmAllocCp.pvCpuUmAddr);
        psKmAlloc->eBufType = PALLOC_BUFTYPE_USERALLOC;
        /* Assign and lock physical addresses to the user space buffer.
           The mapping of the first page in the kernel is also returned */
        ui32Result = SYSOSKM_CpuUmAddrToCpuPAddrArray(pvUmBuff, psKmAlloc->sAllocInfo.psSysPAddr,
                                                      ui32PageNo, &pvCpuKmAddr);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            goto error_get_pages;
        }
    }

    /* Import pages */
    ui32Result = SYSMEMU_ImportExternalPages(eMemPool, sUmAllocCp.ui32Size, eMemAttrib,
                                             &psKmAlloc->hPagesHandle, pvCpuKmAddr,
                                             psKmAlloc->sAllocInfo.psSysPAddr);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_import_pages;
    }

    // Access from user space is not needed for the moment. Can be changed.
    sUmAllocCp.lOffset = 0;
#if PALLOC_EXPOSE_KM_HANDLE
    sUmAllocCp.hKmAllocHandle = psKmAlloc->hPagesHandle;
#endif /* PALLOC_EXPOSE_KM_HANDLE */

    for (ui32PageIdx = 0; ui32PageIdx < ui32PageNo; ++ui32PageIdx)
    {
        ui64CpuPAddr = psKmAlloc->sAllocInfo.psSysPAddr[ui32PageIdx];
        paui64DevAddrs[ui32PageIdx] =
            SYSDEVKM_CpuPAddrToDevPAddr(psAttachContext->hSysDevHandle, ui64CpuPAddr);
    }

    /* Register this with the resource manager */
    ui32Result = RMAN_RegisterResource(psAttachContext->hResBHandle, PALLOC_RES_TYPE_1,
                                       palloc_fnFree, psKmAlloc, IMG_NULL, &sUmAllocCp.ui32AllocId);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_resource_register;
    }

    LOG_EVENT(PALLOC, PALLOC_IMPORTID, LOG_FLAG_END | LOG_FLAG_QUAL_ARG1 | LOG_FLAG_QUAL_ARG2,
              ui32AttachId, sUmAllocCp.ui32AllocId);

    /* Unlock the device...*/
    DMANKM_UnlockDeviceContext(hDevHandle);

    /* Copy to user changed PALLOC_sUmAlloc, including physical device addresses */
    if (SYSOSKM_CopyToUser(psUmAlloc, &sUmAllocCp, sizeof sUmAllocCp))
    {
        ui32Result = IMG_ERROR_FATAL;
        goto error_copy_to_user;
    }
    if (SYSOSKM_CopyToUser(psUmAlloc->aui64DevPAddr, paui64DevAddrs,
                           (sizeof *paui64DevAddrs) * ui32PageNo))
    {
        ui32Result = IMG_ERROR_FATAL;
        goto error_copy_to_user;
    }
    /* Free the address array */
    IMG_BIGORSMALL_FREE((sizeof *paui64DevAddrs) * ui32PageNo, paui64DevAddrs);

    LOG_EVENT(PALLOC, PALLOC_IMPORT, LOG_FLAG_END | LOG_FLAG_QUAL_ARG1 | LOG_FLAG_QUAL_ARG2,
              ui32AttachId, buff_fd);

    /* Return. */
    return IMG_SUCCESS;

    /* Error handling. */
error_copy_to_user:
    /* Free everything. */
    PALLOC_Free1(sUmAllocCp.ui32AllocId);
    goto error_return;

error_resource_register:
    SYSMEMU_FreePages(psKmAlloc->hPagesHandle);
error_import_pages:
    if (buff_fd >= 0)
    {
#ifdef ANDROID_ION_BUFFERS
        palloc_ReleaseIONBuf(psKmAlloc->hBufHandle, NULL);
#endif /* ANDROID_ION_BUFFERS */
    }
    else
    {
        SYSOSKM_ReleaseCpuPAddrArray(pvCpuKmAddr, psKmAlloc->hBufHandle,
                                     psKmAlloc->sAllocInfo.psSysPAddr, ui32PageNo);
    }
error_get_pages:
    IMG_BIGORSMALL_FREE((sizeof *paui64DevAddrs) * ui32PageNo, paui64DevAddrs);
error_addr_array:
    IMG_BIGORSMALL_FREE(sizeof(IMG_SYS_PHYADDR) * ui32PageNo, psKmAlloc->sAllocInfo.psSysPAddr);
error_page_array:
error_get_dev_id:
    IMG_FREE(psKmAlloc);
error_alloc_info:
error_get_dev_by_id:
    /* Unlock the device. */
    DMANKM_UnlockDeviceContext(hDevHandle);

error_return:
    return ui32Result;
}


/*!
******************************************************************************

 @Function                PALLOC_Free1

******************************************************************************/
IMG_RESULT PALLOC_Free1(
    IMG_UINT32  ui32AllocId
)
{
    IMG_UINT32         ui32Result;
    PALLOC_sKmAlloc *  psKmAlloc;
    IMG_HANDLE         hResHandle;
    IMG_HANDLE         hDevHandle;

    LOG_EVENT(PALLOC, PALLOC_FREEID, (LOG_FLAG_START | LOG_FLAG_QUAL_ARG1), ui32AllocId, 0);

    /* Get the resource info from the id...*/
    ui32Result = RMAN_GetResource(ui32AllocId, PALLOC_RES_TYPE_1, (IMG_VOID **)&psKmAlloc, &hResHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    hDevHandle = psKmAlloc->hDevHandle;

    /* Lock the device...*/
    DMANKM_LockDeviceContext(hDevHandle);

    /* Free through the resource manager...*/
    RMAN_FreeResource(hResHandle);

    /* Unlock the device...*/
    DMANKM_UnlockDeviceContext(hDevHandle);

    LOG_EVENT(PALLOC, PALLOC_FREEID, (LOG_FLAG_END| LOG_FLAG_QUAL_ARG1), ui32AllocId, 0);

    /* Return IMG_SUCCESS...*/
    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function                PALLOCKM_GetAllocInfo

******************************************************************************/
IMG_RESULT PALLOCKM_GetAllocInfo(
    IMG_UINT32             ui32AllocId,
    PALLOCKM_sAllocInfo *  psAllocInfo
)
{
    IMG_UINT32         ui32Result;
    PALLOC_sKmAlloc *  psKmAlloc;
    IMG_HANDLE         hResHandle;

    /* Get the resource info from the id...*/
    ui32Result = RMAN_GetResource(ui32AllocId, PALLOC_RES_TYPE_1, (IMG_VOID **)&psKmAlloc, &hResHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Return the allocation info...*/
    *psAllocInfo = psKmAlloc->sAllocInfo;

    /* Return IMG_SUCCESS...*/
    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function                PALLOCKM_CloneAlloc

******************************************************************************/
IMG_RESULT PALLOCKM_CloneAlloc(
    IMG_UINT32    ui32AllocId,
    IMG_HANDLE    hResBHandle,
    IMG_HANDLE *  phResHandle,
    IMG_UINT32 *  pui32AllocId
)
{
    IMG_UINT32  ui32Result;
    IMG_HANDLE  hResHandle;

    /* Get the resource info from the id...*/
    ui32Result = RMAN_GetResource(ui32AllocId, PALLOC_RES_TYPE_1, IMG_NULL, &hResHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Create a cloned reference...*/
    ui32Result = RMAN_CloneResourceHandle(hResHandle, hResBHandle, phResHandle, pui32AllocId);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Return IMG_SUCCESS...*/
    return IMG_SUCCESS;
}
