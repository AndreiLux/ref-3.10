/*!
 *****************************************************************************
 *
 * @file       sysdev_utils1.c
 *
 * This file contains DEVIF user mode implementation of the
 * System Device Kernel Mode Utilities Level 1 API.
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

#include <sysdev_utils1.h>
#ifdef IMG_MEM_ERROR_INJECTION
#undef IMG_MEM_ERROR_INJECTION
#include "sysmem_utils.h"
#define IMG_MEM_ERROR_INJECTION
#else
#include "sysmem_utils.h"
#endif
#include <devif_api.h>
#include <tal.h>
#include <system.h>
#include <osa.h>

#ifndef IMG_KERNEL_MODULE
    #include "tal_setup.h"
#endif

static IMG_VOID *       gpvMemory;                     //!< A pointer to the device memory - IMG_NULL if not defined
static IMG_UINT64       gui64MemSize       = 0;        //!< The size of the device memory (0 if not known)
static IMG_UINT64       gui64DevMemoryBase = 0;        //!< The base address of the device's view of memory

#ifndef DEVIF_DEFINE_DEVIF1_FUNCS
static IMG_VOID *       gpvMemoryBase = IMG_NULL;      //!< A pointer to the device memory base
#endif

static SYSDEVU_sInfo *  gpsInfo = IMG_NULL;            //!< Pointer to global device info (only one device supported)

static IMG_HANDLE       ghIsrTaskCb;                   //!< ISR task control block
static IMG_HANDLE       ghIsrTaskActiveEvent;          //!< ISR task active

static IMG_BOOL         gbShutdown = IMG_FALSE;        //!< IMG_TRUE when shutting down

static IMG_BOOL         gbIsrTaskStarted = IMG_FALSE;  //!< IMG_TRUE when ISR task has been started

/*!
******************************************************************************

 @Function    sysdev_IsrTask

******************************************************************************/
static void sysdev_IsrTask(void * pvParams)
{
    /* Indicate that the ISR task is active...*/
    OSA_ThreadSyncSignal(ghIsrTaskActiveEvent);

    /* We loop until the end of time. Or the system is killed */
    while (!gbShutdown)
    {
        if (TALSETUP_IsInitialised())
    {
        IMG_HANDLE  hMemSpace, hItr;

        hMemSpace = TALITERATOR_GetFirst(TALITR_TYP_INTERRUPT, &hItr);
        while(hMemSpace)
        {
            IMG_BOOL  bHandled;
            IMG_ASSERT(gpsInfo != IMG_NULL);

            /* If there is a LISR registered...*/
            if ( (gpsInfo != IMG_NULL) && (gpsInfo->pfnDevKmLisr != IMG_NULL) )
            {
                /* Call it...*/
                SYSOSKM_DisableInt();
                bHandled = gpsInfo->pfnDevKmLisr(gpsInfo->pvParam);
                SYSOSKM_EnableInt();

                /* Interrupt should have been handled...*/
                IMG_ASSERT(bHandled);
            }
            hMemSpace = TALITERATOR_GetNext(hItr);
        }
        TALITERATOR_Destroy(hItr);
        }

        /* Sleep for a 10 millisecond */
        OSA_ThreadSleep(10);
    }
}

/*!
******************************************************************************

 @Function                SYSDEVU1_FreeDevice

******************************************************************************/
static IMG_VOID freeDevice(
    SYSDEVU_sInfo *  psInfo
)
{
    DEVIF_sDeviceCB *  psDeviceCB = psInfo->pvLocParam;

#ifndef DEVIF_DEFINE_DEVIF1_FUNCS
    if (gpvMemoryBase != IMG_NULL)
    {
        IMG_FREE(gpvMemoryBase);
        gpvMemoryBase = IMG_NULL;
    }
#endif

    if (psDeviceCB != IMG_NULL)
    {
        if (psDeviceCB->pfnDeinitailise    != IMG_NULL)
        {
            psDeviceCB->pfnDeinitailise(psDeviceCB);
        }
        OSA_ThreadSyncDestroy(ghIsrTaskActiveEvent);

        IMG_FREE(psDeviceCB);
    }

    if (gbIsrTaskStarted)
    {
        /* Indicate shutting down...*/
        gbShutdown = IMG_TRUE;

        /* Wait for the task to inactivate...*/
        OSA_ThreadWaitExitAndDestroy(ghIsrTaskCb);
    }
}


/*!
******************************************************************************

 @Function                SYSDEVU1_LocateDevice

******************************************************************************/
IMG_RESULT SYSDEVU_RegisterDriver(
    SYSDEVU_sInfo *  psInfo
)
{
    IMG_RESULT         ui32Result;
    DEVIF_sDeviceCB *  psDeviceCB;
    DEVIF_sDevMapping  sDevMapping;

    /* Set pointer to device info...*/
    gpsInfo = psInfo;
#ifndef IMG_KERNEL_MODULE
    /* Initialise the TAL */
    ui32Result = TALSETUP_Initialise();
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
#endif
    /* Initialise semaphores...*/
    OSA_ThreadSyncCreate(&ghIsrTaskActiveEvent);

    /* Start the ISR task...*/
    OSA_ThreadCreateAndStart(sysdev_IsrTask,
                             NULL,
                             "KMISR Task",
                             OSA_THREAD_PRIORITY_HIGHEST,
                             &ghIsrTaskCb);

    gbIsrTaskStarted = IMG_TRUE;

    /* Wait for the task to activate...*/
    OSA_ThreadSyncWait(ghIsrTaskActiveEvent, OSA_NO_TIMEOUT);

    /* Allocate device control block....*/
    psDeviceCB = IMG_MALLOC(sizeof(*psDeviceCB));
    IMG_ASSERT(psDeviceCB != IMG_NULL);
    if (psDeviceCB == IMG_NULL)
    {
        return IMG_ERROR_OUT_OF_MEMORY;
    }
    IMG_MEMSET(psDeviceCB, 0, sizeof(*psDeviceCB));

    /* Save in device info control block...*/
    psInfo->pvLocParam = psDeviceCB;

    /* Configure the device...*/
#ifdef DEVIF_DEFINE_DEVIF1_FUNCS
    #ifndef FAKE_DEVIF
    DEVIF1_ConfigureDevice(psInfo->sDevInfo.pszDeviceName, psDeviceCB);

    /* Call the init function...*/
    IMG_ASSERT(psDeviceCB->pfnInitailise);
    psDeviceCB->pfnInitailise(psDeviceCB);

    /* Call the init function...*/
    IMG_ASSERT(psDeviceCB->pfnGetDeviceMapping);
    psDeviceCB->pfnGetDeviceMapping(psDeviceCB, &sDevMapping);

    /* Save register pointer etc....*/
    psInfo->pui32PhysRegBase = sDevMapping.pui32Registers;
    psInfo->pui32KmRegBase   = sDevMapping.pui32Registers;
    psInfo->ui32RegSize      = sDevMapping.ui32RegSize;
    #else
    // Test code - allows "host" memory to be used for device memory
    IMG_MEMSET(&sDevMapping, 0, sizeof(sDevMapping));
#define MEM_SIZE    (0x1000000)
    sDevMapping.pui32Memory            = IMG_MALLOC(MEM_SIZE+SYS_MMU_PAGE_SIZE);
    IMG_ASSERT(sDevMapping.pui32Memory != 0);
    gpvMemoryBase = sDevMapping.pui32Memory;
    sDevMapping.pui32Memory            = (IMG_VOID *)(((IMG_UINTPTR)(sDevMapping.pui32Memory)+SYS_MMU_PAGE_SIZE) & ~(SYS_MMU_PAGE_SIZE-1));
    sDevMapping.ui32MemSize            = MEM_SIZE;
    sDevMapping.ui32DevMemoryBase      = 0;
    #endif
#else

    IMG_MEMSET(&sDevMapping, 0, sizeof(sDevMapping));
#define MEM_SIZE    (0x10000000)
    if (gpvMemory == IMG_NULL)
    {
        gpvMemoryBase = IMG_MALLOC(MEM_SIZE+SYS_MMU_PAGE_SIZE);
    }
    IMG_ASSERT(gpvMemoryBase != IMG_NULL);
    sDevMapping.pui32Memory       = (IMG_VOID *)(((IMG_UINTPTR)(gpvMemoryBase)+SYS_MMU_PAGE_SIZE) & ~(SYS_MMU_PAGE_SIZE-1));
    sDevMapping.ui32MemSize       = MEM_SIZE;
    sDevMapping.ui32DevMemoryBase = 0;
#endif

    /**** Only support one device/memory (at the moment)***/
//    IMG_ASSERT(gui64MemSize == 0);
    gpvMemory          = sDevMapping.pui32Memory;
    gui64MemSize       = sDevMapping.ui32MemSize;
    gui64DevMemoryBase = sDevMapping.ui32DevMemoryBase;


    SYSDEVU_SetDevMap(psInfo, psInfo->pui32PhysRegBase, psInfo->pui32KmRegBase, psInfo->ui32RegSize, gpvMemory, gpvMemory, IMG_UINT64_TO_UINT32(gui64MemSize), IMG_UINT64_TO_UINT32(gui64DevMemoryBase));
    SYSDEVU_SetDevFuncs(psInfo, freeDevice, IMG_NULL, IMG_NULL);

    ui32Result = SYSMEMKM_AddDevIFMemory((IMG_UINTPTR)gpvMemory, (IMG_UINT64)gpvMemory, IMG_UINT64_TO_UINT32(gui64MemSize), &psInfo->sMemPool);
    IMG_ASSERT(IMG_SUCCESS == ui32Result);
    if (IMG_SUCCESS != ui32Result)
    {
        return ui32Result;
    }

    /* Return success...*/
    return IMG_SUCCESS;
}

IMG_RESULT SYSDEVU_UnRegisterDriver(SYSDEVU_sInfo *sysdev) {
    SYSMEMU_RemoveMemoryHeap(sysdev->sMemPool);
    sysdev->pfnFreeDevice(sysdev);
    return IMG_SUCCESS;
}
