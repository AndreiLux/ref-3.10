/*!
 *****************************************************************************
 *
 * @file	   ump_api_km.c
 *
 * This file contains the User Mode Pipe Kernel Mode component.
 * User Mode Pipe Kernel Mode component.  The User Mode
 * Pipe  provides a means of piping information from kernel
 * mode to a user mode application.
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

#include "ump_api_km.h"
#include "sysdev_api_km.h"
#include "sysos_api_km.h"
#include "rman_api.h"
#include "lst.h"

/*!
******************************************************************************
 This structure contains the test device context.
******************************************************************************/
typedef struct
{
    LST_LINK;    /*!< List link (allows the structure to be part of a MeOS list).*/
    IMG_UINT32   ui32WriteIndex;  //!< Buffer write index - in bytes
    IMG_UINT32   ui32ReadIndex;   //!< Buffer read index - in bytes
    IMG_UINT32   ui32FreeSpace;   //!< The amount of "free" space in the buffer - in bytes
    IMG_UINT8 *  pui8Buffer;      //!< Pointer to buffer

} UMPKM_sBufferCb;

/*!
******************************************************************************
 This structure contains the device context.
******************************************************************************/
typedef struct
{
    IMG_HANDLE       hDevHandle;    //!< Device handle
    IMG_HANDLE       hEventHandle;  //!< Event handle
    IMG_BOOL         bPreempt;      //!< Pre-empt
    IMG_BOOL         bProcWaiting;  //!< Process is waiting
    IMG_BOOL         bDataLost;     //!< Data lost
    UMP_sConfigData  sConfigData;   //!< Config data
    LST_T            sBufferList;   //!< List of UMPKM_sBufferCb structures associated with this device

} UMPKM_sContext;

/*!
******************************************************************************

 @Function              umpkm_fnDevInit

 See definition of #DMANKM_pfnDevInit.

******************************************************************************/
static IMG_RESULT umpkm_fnDevInit (
    IMG_HANDLE   hDevHandle,
    IMG_HANDLE   hInitConnHandle,
    IMG_VOID **  ppvDevInstanceData
)
{
    UMPKM_sContext *  psContext;

    psContext = (UMPKM_sContext *)DMANKM_GetDevGlobalData(hDevHandle);
    IMG_ASSERT(psContext != IMG_NULL);

    *ppvDevInstanceData = psContext;

    /* Return success...*/
    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              umpkm_fnDevDeinit

 See definition of #DMANKM_pfnDevDeinit.

******************************************************************************/
static IMG_VOID umpkm_fnDevDeinit (
    IMG_HANDLE  hDevHandle,
    IMG_HANDLE  hInitConnHandle,
    IMG_VOID *  pvDevInstanceData
)
{
    //UMPKM_sContext *  psContext = (UMPKM_sContext *)pvDevInstanceData;
}

/*!
******************************************************************************

 @Function              umpkm_fnDevConnect

 See definition of #DMANKM_fnDevConnect.

******************************************************************************/
static IMG_RESULT umpkm_fnDevConnect (
    IMG_HANDLE   hConnHandle,
    IMG_VOID *   pvDevInstanceData,
    IMG_VOID **  ppvDevConnectionData
)
{
//    UMPKM_sContext *  psContext = (UMPKM_sContext *)pvDevInstanceData;

    /* Return success...*/
    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              umpkm_fnDevDisconnect

 See definition of #DMANKM_pfnDevDisconnect.

******************************************************************************/
static IMG_RESULT umpkm_fnDevDisconnect (
    IMG_HANDLE           hConnHandle,
    IMG_VOID *           pvDevInstanceData,
    IMG_VOID *           pvDevConnectionData,
    DMANKM_eDisconnType  eDisconnType
)
{
    IMG_UINT32        ui32Result = IMG_SUCCESS;
//    UMPKM_sContext *  psContext = (UMPKM_sContext *)pvDevInstanceData;

    ui32Result = DMANKM_DevDisconnectComplete(hConnHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);

    /* Return success...*/
    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function              umpkm_fnDevRegister

 See definition of #DMANKM_pfnDevRegister.

******************************************************************************/
static IMG_RESULT umpkm_fnDevRegister (
    DMANKM_sDevRegister *  psDevRegister
)
{
    psDevRegister->pfnDevInit       = umpkm_fnDevInit;
    psDevRegister->pfnDevDeinit     = umpkm_fnDevDeinit;
    psDevRegister->pfnDevConnect    = umpkm_fnDevConnect;
    psDevRegister->pfnDevDisconnect = umpkm_fnDevDisconnect;

    /* This is a pseudo-device */
    psDevRegister->ui32DevFlags |= DMAN_DFLAG_PSEUDO_DEVICE;

    /* Return success...*/
    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              umpkm_AddBuffer

******************************************************************************/
static IMG_RESULT umpkm_AddBuffer (
    UMPKM_sContext *  psContext
)
{
    UMPKM_sBufferCb *  psBufferCb;

    /* Allocate buffer CB...*/
    psBufferCb = IMG_MALLOC(sizeof(*psBufferCb));
    IMG_ASSERT(psBufferCb != IMG_NULL);
    if (psBufferCb == IMG_NULL)
    {
        return IMG_ERROR_OUT_OF_MEMORY;
    }
    IMG_MEMSET(psBufferCb, 0, sizeof(*psBufferCb));

    /* Allocate buffer...*/
    psBufferCb->pui8Buffer = IMG_MALLOC(psContext->sConfigData.ui32BufSize);
    IMG_ASSERT(psBufferCb->pui8Buffer != IMG_NULL);
    if (psBufferCb->pui8Buffer == IMG_NULL)
    {
        IMG_FREE(psBufferCb);
        return IMG_ERROR_OUT_OF_MEMORY;
    }
    psBufferCb->ui32FreeSpace = psContext->sConfigData.ui32BufSize;

    /* Add buffer to list..*/
    LST_add(&psContext->sBufferList, psBufferCb);

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              umpkm_Alloc

******************************************************************************/
static IMG_RESULT umpkm_Alloc (
    UMP_sConfigData *  psConfigData,
    UMPKM_sContext **  ppsContext
)
{
    UMPKM_sContext *  psContext;
    IMG_UINT32        ui32Result;

    /* Allocate context...*/
    psContext = IMG_MALLOC(sizeof(*psContext));
    IMG_ASSERT(psContext != IMG_NULL);
    if (psContext == IMG_NULL)
    {
        return IMG_ERROR_OUT_OF_MEMORY;
    }
    IMG_MEMSET(psContext, 0, sizeof(*psContext));

    *ppsContext = psContext;

    psContext->sConfigData = *psConfigData;
    LST_init(&psContext->sBufferList);

    /* Create a event object to wait on...*/
    ui32Result = SYSOSKM_CreateEventObject(&psContext->hEventHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_create_event_object;
    }

    /* Add a buffer to this context...*/
    ui32Result = umpkm_AddBuffer(psContext);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Return success...*/
    return IMG_SUCCESS;

    /* Error handling. */
error_add_buffer:
    SYSOSKM_DestroyEventObject(psContext->hEventHandle);
error_create_event_object:
    IMG_FREE(psContext);

    return ui32Result;
}

/*!
******************************************************************************

 @Function              umpkm_fnFree

******************************************************************************/
static IMG_VOID umpkm_fnFree (
    IMG_VOID *  pvParam
)
{
    UMPKM_sContext *   psContext = pvParam;
    UMPKM_sBufferCb *  psBufferCb;

    /* Free all buffers...*/
    psBufferCb = (UMPKM_sBufferCb *)LST_removeHead(&psContext->sBufferList);
    while (psBufferCb != IMG_NULL)
    {
        IMG_FREE(psBufferCb->pui8Buffer);
        IMG_FREE(psBufferCb);

        psBufferCb = (UMPKM_sBufferCb *)LST_removeHead(&psContext->sBufferList);
    }

    SYSOSKM_DestroyEventObject(psContext->hEventHandle);

    IMG_FREE(psContext);
}

/*!
******************************************************************************

 @Function                UMPKM_CreatePipe

******************************************************************************/
IMG_RESULT UMPKM_CreatePipe(
    IMG_CHAR *         pszPipeName,
    UMP_sConfigData *  psConfigData,
    IMG_HANDLE *       phPipeHandle
)
{
    IMG_UINT32        ui32Result;
    IMG_HANDLE        hResBHandle;
    IMG_VOID *        pvDevGlobalData = NULL;
    UMPKM_sContext *  psContext;

    /* Ensure it's a multiple of 32-bit words...*/
    IMG_ASSERT((psConfigData->ui32BufSize & 3) == 0);

    /* Register the device with the device manager...*/
    ui32Result = DMANKM_RegisterDevice(pszPipeName, umpkm_fnDevRegister);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Allocate context structure...*/
    ui32Result = umpkm_Alloc(psConfigData, &psContext);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_context_alloc;
    }

    /* Get access to the global resource bucket...*/
    ui32Result = RMAN_Initialise();
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_rman_init;
    }
    hResBHandle = RMAN_GetGlobalBucket();
    IMG_ASSERT(hResBHandle != IMG_NULL);
    if (hResBHandle == IMG_NULL)
    {
        ui32Result = IMG_ERROR_GENERIC_FAILURE;
        goto error_get_global_bucket;
    }

    /* Register this context with the resource manager...
       Other rman resources are dependent on this, so free it last */
    ui32Result = RMAN_RegisterResource(hResBHandle , RMAN_STICKY, umpkm_fnFree, psContext, IMG_NULL, IMG_NULL);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_register_resource;
    }

    /* Locate the device/pipe...*/
    ui32Result = DMANKM_LocateDevice(pszPipeName, &psContext->hDevHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error_locate_device;
    }

    /* Ensure that the global data for this device is only set once...*/
    pvDevGlobalData = DMANKM_GetDevGlobalData(psContext->hDevHandle);
    IMG_ASSERT(pvDevGlobalData == IMG_NULL);

    /* Set the global data...*/
    DMANKM_SetDevGlobalData(psContext->hDevHandle, psContext);

    /* Return handle to context...*/
    *phPipeHandle = psContext;

    /* Return success...*/
    return IMG_SUCCESS;

    /* Error handling. */
error_locate_device:
    /* Just unregister device... */
    goto error_context_alloc;
error_register_resource:
error_get_global_bucket:
error_rman_init:
    umpkm_fnFree(psContext);
error_context_alloc:
    DMANKM_UnRegisterDevice(pszPipeName);

    return ui32Result;
}


/*!
******************************************************************************

 @Function                UMPKM_WriteRecord

******************************************************************************/
IMG_RESULT UMPKM_WriteRecord(
    IMG_HANDLE  hPipeHandle,
    IMG_UINT32  ui32Size,
    IMG_VOID *  pvData
)
{
    UMPKM_sContext *   psContext = (UMPKM_sContext *)hPipeHandle;
    UMPKM_sBufferCb *  psBufferCb;
    IMG_UINT32         ui32WriteIndex;
    IMG_UINT32         ui32Result;
    IMG_UINT32         ui32RoundedSize;

    /* Get access to the context...*/
/*     IMG_ASSERT(psContext != IMG_NULL); */
/*     IMG_ASSERT(psContext->sConfigData.ui32BufSize >= (ui32Size + 4)); */

    /* Calculate the rounded size for this message...*/
    ui32RoundedSize = (ui32Size + 7) & ~0x3;

    /* Disable interrupts...*/
    SYSOSKM_DisableInt();

    /* Get access to the "current" buffer CB...*/
    psBufferCb = (UMPKM_sBufferCb *)LST_last(&psContext->sBufferList);
    if(!psBufferCb)
    {
        SYSOSKM_EnableInt();
        return IMG_SUCCESS;
    }

    /* Branch on buffer mode...*/
    switch (psContext->sConfigData.eBufferMode)
    {
    case UMP_MODE_CYCLIC:
        /* Ensure there is enough space in the buffer for the next "message"...*/
        while (psBufferCb->ui32FreeSpace < ui32RoundedSize)
        {
            IMG_UINT32        ui32NextSize;
            IMG_UINT32        ui32NextRoundedSize;

            /* Determine the total size of the message to be discarded...*/
            ui32NextSize= *(IMG_UINT32*)(&psBufferCb->pui8Buffer[psBufferCb->ui32ReadIndex]);
            ui32NextRoundedSize = (ui32NextSize + 7) & ~0x3;

            /* Update the free space...*/
            psBufferCb->ui32FreeSpace += ui32NextRoundedSize;

            /* Indicate data lost...*/
            psContext->bDataLost = IMG_TRUE;

            /* Update the read index...*/
            psBufferCb->ui32ReadIndex += ui32NextRoundedSize;
            if (psBufferCb->ui32ReadIndex >= psContext->sConfigData.ui32BufSize)
            {
                psBufferCb->ui32ReadIndex -= psContext->sConfigData.ui32BufSize;
            }
        }

        /* Copy size into buffer...*/
        ui32WriteIndex = psBufferCb->ui32WriteIndex;
        *(IMG_UINT32*)(&psBufferCb->pui8Buffer[ui32WriteIndex]) = ui32Size;
        ui32WriteIndex += 4;
        if (ui32WriteIndex >= psContext->sConfigData.ui32BufSize)
        {
            ui32WriteIndex = 0;
        }

        /* Copy this message into the buffer...*/
        if ((ui32WriteIndex + ui32Size) >= psContext->sConfigData.ui32BufSize)
        {
            IMG_UINT32  ui32Size2 = (ui32WriteIndex + ui32Size) - psContext->sConfigData.ui32BufSize;
            IMG_UINT32  ui32Size1 = ui32Size - ui32Size2;
            IMG_MEMCPY(&psBufferCb->pui8Buffer[ui32WriteIndex], pvData, ui32Size1);
            IMG_MEMCPY(&psBufferCb->pui8Buffer[0], &((IMG_UINT8 *)pvData)[ui32Size1], ui32Size2);
            psBufferCb->pui8Buffer[ui32Size2] = 0;
        }
        else
        {
            IMG_MEMCPY(&psBufferCb->pui8Buffer[ui32WriteIndex], pvData, ui32Size);
            psBufferCb->pui8Buffer[ui32WriteIndex + ui32Size] = 0;
        }
        break;

    case UMP_MODE_LOST_LESS:
        /* Ensure there is enough space in the buffer for the next "message"...*/
        if (psBufferCb->ui32FreeSpace < ui32RoundedSize)
        {
            /* Add a buffer to this context...*/
            ui32Result = umpkm_AddBuffer(psContext);
//            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                SYSOSKM_EnableInt();
                return ui32Result;
            }
            /* Get access to the "new" buffer CB...*/
            psBufferCb = (UMPKM_sBufferCb *)LST_last(&psContext->sBufferList);

//            IMG_ASSERT(psBufferCb->ui32FreeSpace >= ui32RoundedSize);
        }

        /* Copy size into buffer...*/
        ui32WriteIndex = psBufferCb->ui32WriteIndex;
        *(IMG_UINT32*)(&psBufferCb->pui8Buffer[ui32WriteIndex]) = ui32Size;
        ui32WriteIndex += 4;

        /* Copy this message into the buffer...*/
        IMG_MEMCPY(&psBufferCb->pui8Buffer[ui32WriteIndex], pvData, ui32Size);
        psBufferCb->pui8Buffer[ui32WriteIndex + ui32Size] = 0;
        break;

    default:
//        IMG_ASSERT(IMG_FALSE);
        break;
    }
    /* Update the buffer write index...*/
    psBufferCb->ui32WriteIndex += ui32RoundedSize;
    if (psBufferCb->ui32WriteIndex >= psContext->sConfigData.ui32BufSize)
    {
        psBufferCb->ui32WriteIndex -= psContext->sConfigData.ui32BufSize;
    }

    /* Update the free space...*/
//    IMG_ASSERT(psBufferCb->ui32FreeSpace >= ui32RoundedSize);
    psBufferCb->ui32FreeSpace -= ui32RoundedSize;

    /* If there is a process waiting then signal to it...*/
    if (psContext->bProcWaiting)
    {
        psContext->bProcWaiting = IMG_FALSE;
        SYSOSKM_SignalEventObject(psContext->hEventHandle);
    }

    /* Re enable interrupts...*/
    SYSOSKM_EnableInt();

    /* Return success...*/
    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function                UMP_GetConfigData1

******************************************************************************/
IMG_RESULT UMP_GetConfigData1(
    IMG_UINT32                ui32ConnId,
    UMP_sConfigData __user *  psConfigData
)
{
    IMG_HANDLE        hDevHandle;
    IMG_HANDLE        hConnHandle;
    UMPKM_sContext *  psContext;
    IMG_UINT32        ui32Result;

    /* Get the connection handle from it's ID...*/
    ui32Result = DMANKM_GetConnHandleFromId(ui32ConnId, &hConnHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Get the device handle...*/
    hDevHandle = DMANKM_GetDevHandleFromConn(hConnHandle);

    /* Get access to the context...*/
    psContext = (UMPKM_sContext *)DMANKM_GetDevGlobalData(hDevHandle);
    IMG_ASSERT(psContext != IMG_NULL);

    /* Return the config data...*/
    ui32Result = SYSOSKM_CopyToUser(psConfigData, &psContext->sConfigData, sizeof(psContext->sConfigData));

    if(ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Return success...*/
    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function                UMP_ReadBlock1

******************************************************************************/
IMG_RESULT UMP_ReadBlock1(
    IMG_UINT32           ui32ConnId,
    IMG_UINT32           ui32BufSize,
    IMG_VOID __user *    pvData,
    IMG_UINT32 __user *  pui32DataRead,
    IMG_BOOL __user *    pbDataLost
)
{
    IMG_HANDLE         hDevHandle;
    IMG_HANDLE         hConnHandle;
    UMPKM_sContext *   psContext;
    UMPKM_sBufferCb *  psBufferCb;
    UMPKM_sBufferCb *  psTmpBufferCb = NULL;
    IMG_UINT32         ui32ReadIndex;
    IMG_UINT32         ui32Size;
    IMG_UINT32         ui32Result;

    /* Get the connection handle from its ID...*/
    ui32Result = DMANKM_GetConnHandleFromId(ui32ConnId, &hConnHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Get the device handle...*/
    hDevHandle = DMANKM_GetDevHandleFromConn(hConnHandle);

    /* Get access to the context...*/
    psContext = (UMPKM_sContext *)DMANKM_GetDevGlobalData(hDevHandle);
    IMG_ASSERT(psContext != IMG_NULL);
    
    if(psContext == IMG_NULL)
    {
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        return ui32Result;
    }

    /* Buffer should be of the correct size...*/
    IMG_ASSERT(ui32BufSize >= psContext->sConfigData.ui32BufSize);

    /* Disable interrupts...*/
    SYSOSKM_DisableInt();

    /* Get access to the "oldest" buffer CB...*/
    psBufferCb = (UMPKM_sBufferCb *)LST_first(&psContext->sBufferList);

    /* While there is nothing to read....*/
    while (
           (psBufferCb->ui32FreeSpace == psContext->sConfigData.ui32BufSize) &&
           (!psContext->bPreempt)
          )
    {
        /* Signal process waiting...*/
        psContext->bProcWaiting = IMG_TRUE;

        /* Re enable interrupts...*/
        SYSOSKM_EnableInt();

        /* Wait for a signal...*/
        ui32Result = SYSOSKM_WaitEventObject(psContext->hEventHandle, IMG_FALSE);
        if (ui32Result == IMG_ERROR_INTERRUPTED)
        {
            /* Not really an error..*/
            return ui32Result;
        }
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            return ui32Result;
        }

        /* Disable interrupts...*/
        SYSOSKM_DisableInt();

        /* Get access to the "oldest" buffer CB...*/
        psBufferCb = (UMPKM_sBufferCb *)LST_first(&psContext->sBufferList);
    }

    /* If preempt...*/
    if (psContext->bPreempt)
    {
        IMG_UINT32 ui32DataRead = 0;
        IMG_BOOL   bDataLost    = IMG_FALSE;

        /* Flag no data read or lost...*/
        ui32Result = SYSOSKM_CopyToUser(pui32DataRead, &ui32DataRead, sizeof(IMG_UINT32));
        if(ui32Result != IMG_SUCCESS)
        {
            SYSOSKM_EnableInt();
            return ui32Result;
        }
        ui32Result = SYSOSKM_CopyToUser(pbDataLost, &bDataLost, sizeof(IMG_BOOL));
        if(ui32Result != IMG_SUCCESS)
        {
            SYSOSKM_EnableInt();
            return ui32Result;
        }
        psContext->bPreempt = IMG_FALSE;

        /* Re enable interrupts...*/
        SYSOSKM_EnableInt();

        /* Return success...*/
        return IMG_SUCCESS;
    }

    /* Return the amount of data read...*/
    ui32Size = psContext->sConfigData.ui32BufSize - psBufferCb->ui32FreeSpace;

    ui32Result = SYSOSKM_CopyToUser(pui32DataRead, &ui32Size, sizeof(IMG_UINT32));
    if(ui32Result != IMG_SUCCESS)
    {
        SYSOSKM_EnableInt();
        return ui32Result;
    }

    /* Branch on buffer mode...*/
    switch (psContext->sConfigData.eBufferMode)
    {
    case UMP_MODE_CYCLIC:
        /* Copy buffer...*/
        ui32ReadIndex = psBufferCb->ui32ReadIndex;
        if ((ui32ReadIndex + ui32Size) >= psContext->sConfigData.ui32BufSize)
        {
        IMG_UINT32  ui32Size2 = (ui32ReadIndex + ui32Size) - psContext->sConfigData.ui32BufSize;
        IMG_UINT32  ui32Size1 = ui32Size - ui32Size2;
            SYSOSKM_CopyToUser(pvData, &psBufferCb->pui8Buffer[ui32ReadIndex], ui32Size1);
            SYSOSKM_CopyToUser(((IMG_UINT8 __user *)pvData) + ui32Size1 , &psBufferCb->pui8Buffer[0], ui32Size2);
        }
        else
        {
            SYSOSKM_CopyToUser(pvData, &psBufferCb->pui8Buffer[ui32ReadIndex], ui32Size);
        }

        /* Reset the buffer controls...*/
        psBufferCb->ui32WriteIndex = 0;
        psBufferCb->ui32ReadIndex  = 0;
        psBufferCb->ui32FreeSpace  = psContext->sConfigData.ui32BufSize;
        break;

    case UMP_MODE_LOST_LESS:
        /* Copy buffer...*/
        ui32ReadIndex = psBufferCb->ui32ReadIndex;
        SYSOSKM_CopyToUser(pvData, &psBufferCb->pui8Buffer[ui32ReadIndex], ui32Size);

        /* Is there are more buffers...*/
        if (LST_next(psBufferCb) != IMG_NULL)
        {
            /* Free this buffer...*/
            psTmpBufferCb = (UMPKM_sBufferCb *)LST_removeHead(&psContext->sBufferList);
            IMG_ASSERT(psTmpBufferCb == psBufferCb);
            IMG_FREE(psBufferCb->pui8Buffer);
            IMG_FREE(psBufferCb);
        }
        else
        {
            /* Reset the buffer controls...*/
            psBufferCb->ui32WriteIndex = 0;
            psBufferCb->ui32ReadIndex  = 0;
            psBufferCb->ui32FreeSpace  = psContext->sConfigData.ui32BufSize;
        }
        break;

    default:
        IMG_ASSERT(IMG_FALSE);
    }

    /* Return and reset data lost flag...*/
    ui32Result = SYSOSKM_CopyToUser(pbDataLost, &psContext->bDataLost, sizeof(IMG_BOOL));
    if(ui32Result != IMG_SUCCESS)
    {
        SYSOSKM_EnableInt();
        return ui32Result;
    }
    psContext->bDataLost = IMG_FALSE;

    /* Re enable interrupts...*/
    SYSOSKM_EnableInt();

    /* Return success...*/
    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function                UMP_FlushPipe1

******************************************************************************/
IMG_RESULT UMP_FlushPipe1(
    IMG_UINT32  ui32ConnId
)
{
    IMG_HANDLE         hDevHandle;
    IMG_HANDLE         hConnHandle;
    UMPKM_sContext *   psContext;
    UMPKM_sBufferCb *  psBufferCb;
    UMPKM_sBufferCb *  psTmpBufferCb = NULL;
    IMG_UINT32         ui32Result;

    /* Get the connection handle from it's ID...*/
    ui32Result = DMANKM_GetConnHandleFromId(ui32ConnId, &hConnHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Get the device handle...*/
    hDevHandle = DMANKM_GetDevHandleFromConn(hConnHandle);

    /* Get access to the context...*/
    psContext = (UMPKM_sContext *)DMANKM_GetDevGlobalData(hDevHandle);
    IMG_ASSERT(psContext != IMG_NULL);

    if(psContext == IMG_NULL)    
    {
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        return ui32Result;
    }

    /* Disable interrupts...*/
    SYSOSKM_DisableInt();

    /* Get access to the "oldest" buffer CB...*/
    psBufferCb = (UMPKM_sBufferCb *)LST_first(&psContext->sBufferList);

    /* While there is something to read....*/
    while (psBufferCb->ui32FreeSpace != psContext->sConfigData.ui32BufSize)
    {

        /* Branch on buffer mode...*/
        switch (psContext->sConfigData.eBufferMode)
        {
        case UMP_MODE_LOST_LESS:
            /* Is there are more buffers...*/
            while (LST_next(psBufferCb) != IMG_NULL)
            {
                /* Free this buffer...*/
                psTmpBufferCb = (UMPKM_sBufferCb *)LST_removeHead(&psContext->sBufferList);
                IMG_ASSERT(psTmpBufferCb == psBufferCb);
                IMG_FREE(psBufferCb->pui8Buffer);
                IMG_FREE(psBufferCb);
            }
            //**** FALL THRU ***

        case UMP_MODE_CYCLIC:
            /* Reset the buffer controls...*/
            psBufferCb->ui32WriteIndex = 0;
            psBufferCb->ui32ReadIndex  = 0;
            psBufferCb->ui32FreeSpace  = psContext->sConfigData.ui32BufSize;
            break;

        default:
            IMG_ASSERT(IMG_FALSE);
        }
    }

    /* Re enable interrupts...*/
    SYSOSKM_EnableInt();

    /* Return success...*/
    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function                UMP_PreemptRead1

******************************************************************************/
IMG_RESULT UMP_PreemptRead1(
    IMG_UINT32  ui32ConnId
)
{
    IMG_HANDLE        hDevHandle;
    IMG_HANDLE        hConnHandle;
    UMPKM_sContext *  psContext;
    IMG_UINT32        ui32Result;

    /* Get the connection handle from it's ID...*/
    ui32Result = DMANKM_GetConnHandleFromId(ui32ConnId, &hConnHandle);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }

    /* Get the device handle...*/
    hDevHandle = DMANKM_GetDevHandleFromConn(hConnHandle);

    /* Get access to the context...*/
    psContext = (UMPKM_sContext *)DMANKM_GetDevGlobalData(hDevHandle);
    IMG_ASSERT(psContext != IMG_NULL);

    if(psContext == IMG_NULL)
    {
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        return ui32Result;
    }

    /* Disable interrupts...*/
    SYSOSKM_DisableInt();

    /* Flag preemption...*/
    psContext->bPreempt = IMG_TRUE;

    /* If there is a process waiting then signal to it...*/
    if (psContext->bProcWaiting)
    {
        psContext->bProcWaiting = IMG_FALSE;
        SYSOSKM_SignalEventObject(psContext->hEventHandle);
    }

    /* Re enable interrupts...*/
    SYSOSKM_EnableInt();

    /* Return success...*/
    return IMG_SUCCESS;
}
