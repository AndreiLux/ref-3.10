/*!
 *****************************************************************************
 *
 * @file       hwctrl_api.c
 *
 * VDECDD Hardware control API.
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

#include "hwctrl_api.h"
#include "core_api.h"
#include "report_api.h"
#include "rman_api.h"
#include "sysos_api_km.h"

#ifndef SYSBRG_BRIDGING
    #include "talmmu_api.h"
#else
    //#include "msvdx_ext.h"
    #include "sysmem_utils.h"
#endif

#include "vdecfw_msg_mem_io.h"

#ifdef __FAKE_MTX_INTERFACE__
    #include "vdecfw_fakemtx.h"
#endif

#ifdef USE_REAL_FW
    #include "vdecfw_bin.h"
#endif

#ifndef VDEC_MSVDX_HARDWARE
    //#define HWCTRL_REPLACEMENT_HW_THREAD
#endif

#ifdef HWCTRL_REPLACEMENT_HW_THREAD

/*!
******************************************************************************
 Contains task control information
******************************************************************************/
typedef struct
{
    IMG_BOOL    bExit;              /*!< Used to signal the task to exit.  */
    LST_T       sMsgList;           /*!< List of messages.                 */
    IMG_HANDLE  hMsgListMutex;      /*!< Sync list of messages.            */
    IMG_HANDLE  hNewMsgSent;        /*!< New message sent event.           */

    // book-keeping info
    IMG_HANDLE  hTask;              /*!< Task handle.                      */
    IMG_HANDLE  hActivatedEvent;    /*!< Indicates task was activated.     */
    IMG_HANDLE  hDeactivatedEvent;  /*!< Indicates task was deactivated.   */
} HWCTRL_sTaskInfo;

#endif


/*!
******************************************************************************
 This structure contains the hwctrl context.
 @brief  Hardware Control Context
******************************************************************************/
typedef struct
{
    IMG_BOOL                bInitialised;           /*!< Indicates whether the core has been enumerated.                            */
    volatile IMG_BOOL       bPower;                 /*!< Indicating whether the core is powered up.                                 */

    IMG_UINT32              ui32CoreNum;
    VDECDD_sDdDevConfig     sDevConfig;             /*!< Device configuration.                                                      */

    IMG_HANDLE              hMsvdx;                 /*!< Handle to MSVDX Context.                                                   */

    IMG_VOID *              pvDecCore;
    IMG_VOID *              pvCompInitUserData;

    MSVDXIO_sDdBufInfo      aui32RendecBufInfo[VDEC_NUM_RENDEC_BUFFERS];  /*!< Device virtual address of rendec buffers.            */
    MSVDXIO_sDdBufInfo      sDevPtdBufInfo;                               /*!< Buffer to hold device Page Table directory address.  */
    MSVDXIO_sDdBufInfo      sPsrModInfo;

    // State
    LST_T                   sPendPictList;          /*!< List of pictures that are being decoded on this core.                      */
    IMG_UINT32              ui32NumFreeSlots;       /*!< Number of front-end decoding slots (in pictures).                          */
    IMG_UINT32              ui32Load;               /*!< Metric to indicate the fullness of the core.                               */
    HWCTRL_sMsgStatus       sHostMsgStatus;         /*!< MSVDX message status.                                                      */

    IMG_HANDLE              hTimerHandle;
    IMG_BOOL                bMonitorBusy;

    HWCTRL_sState           sState;                 /*!< Current HWCTRL state. In core context because it is too big for stack.     */
    HWCTRL_sState           sPrevState;
    IMG_BOOL                bPrevHwStateSet;

    IMG_HANDLE              hFWPrintTimerHandle;
    IMG_BOOL                bPrintTaskBusy;
    IMG_BOOL                bPrintTaskRequestedActive;

    // Firmware message.
    MSVDX_sMsgQueue         sMsgQueue;              /*!< The queue of firmware messages.                                            */
    MSVDX_sIntStatus        sIntStatus;
    MSVDXIO_sHISRMsg        asHISRMsg[MSVDX_MAX_LISR_HISR_MSG]; /*!< Messages passed between LISR and HISR.                         */

#ifdef HWCTRL_REPLACEMENT_HW_THREAD
    HWCTRL_sTaskInfo      * psTaskInfo;             /*!< Pointer to task data.                                                      */
#endif

} HWCTRL_sHwCtx;


#ifdef HWCTRL_REPLACEMENT_HW_THREAD

/*!
******************************************************************************
 This type defines the HWCTRL task events.
 @brief  HWCTRL Internal Events
******************************************************************************/
typedef enum
{
    HWCTRL_EVENT_DECODE_PICTURE,  /*!< Decode a picture.     */

    HWCTRL_EVENT_MAX,             /*!< Max. "normal" event.  */

} HWCTRL_eEvent;


/*!
******************************************************************************
 Contains event information posted to HWCTRL task
******************************************************************************/
typedef struct
{
    LST_LINK;        /*!< List link (allows the structure to be part of a MeOS list).*/

    HWCTRL_eEvent    eEvent;   /*!< HWCTRL event.    */
    HWCTRL_sHwCtx *  psHwCtx;  /*!< HWCTRL context.  */

} HWCTRL_sEventInfo;

/*!
******************************************************************************

 @Function              hwctrl_SendEvent

******************************************************************************/
static IMG_VOID hwctrl_SendEvent(
    HWCTRL_sTaskInfo *   psTaskInfo,
    HWCTRL_eEvent        eEvent,
    HWCTRL_sEventInfo *  psEventInfo
)
{
    psEventInfo->eEvent = eEvent;
    SYSOSKM_LockMutex(psTaskInfo->hMsgListMutex);
    LST_add(&psTaskInfo->sMsgList, psEventInfo);
    SYSOSKM_UnlockMutex(psTaskInfo->hMsgListMutex);

    /* Kick the task...*/
    SYSOSKM_SignalEventObject(psTaskInfo->hNewMsgSent);
}


/*!
******************************************************************************

 @Function              hwctrl_Task

 @Description

 This task mimics the hardware operation when none is present.

 @Input     pvTaskParam : Pointer to task parameter.

******************************************************************************/
static IMG_BOOL
hwctrl_Task(
    IMG_VOID *pvParams
)
{
    HWCTRL_sTaskInfo *   psTaskInfo;
    HWCTRL_sEventInfo *  psEventInfo;
    IMG_RESULT           ui32Result;

    psTaskInfo = (HWCTRL_sTaskInfo *)pvParams;
    IMG_ASSERT(psTaskInfo);

    SYSOSKM_SignalEventObject(hActivatedEvent);

    while (!psTaskInfo->bExit)
    {
        SYSOSKM_WaitEventObject(psTaskInfo->hNewMsgSent);

        if (!psTaskInfo->bExit)
        {
            /* Get the event...*/
            SYSOSKM_LockMutex(psTaskInfo->hMsgListMutex);
            psEventInfo = (HWCTRL_sEventInfo *)LST_removeHead(&psTaskInfo->sMsgList);
            SYSOSKM_UnlockMutex(psTaskInfo->hMsgListMutex);

            while (psEventInfo != IMG_NULL)
            {
                switch (psEventInfo->eEvent)
                {
                case HWCTRL_EVENT_DECODE_PICTURE:

                    // Fake hardware processing.
                    KRN_hibernate(&hibernateQ, 1);

                    /* Generate a Dev HW Interrupt and not submit the picture */
                    ui32Result = CORE_DevHwInterrupt(psEventInfo->psHwCtx->pvCompInitUserData, psEventInfo->psHwCtx->pvDecCore);
                    IMG_ASSERT(ui32Result == IMG_SUCCESS);
                    if (ui32Result != IMG_SUCCESS)
                    {
                        //return ui32Result;
                    }
                    break;

                default:
                    IMG_ASSERT(IMG_FALSE);
                    break;
                }

                IMG_FREE(psEventInfo);

                /* Get next event */
                SYSOSKM_LockMutex(psTaskInfo->hMsgListMutex);
                psEventInfo = (HWCTRL_sEventInfo *)LST_removeHead(&psTaskInfo->sMsgList);
                SYSOSKM_UnlockMutex(psTaskInfo->hMsgListMutex);
            }
        }
    }

    SYSOSKM_SignalEventObject(hDeactivatedEvent);

    return IMG_FALSE;
}

#endif

static IMG_VOID
hwctrl_SetupDWR(
    HWCTRL_sHwCtx * psHwCtx,
    SYSOSKM_pfnTimer pfnTimer
)
{
    if (psHwCtx->hTimerHandle == IMG_NULL)
    {
        if (psHwCtx->sDevConfig.ui32DwrPeriod)
        {
            SYSOSKM_CreateTimer(pfnTimer, psHwCtx, psHwCtx->sDevConfig.ui32DwrPeriod, &psHwCtx->hTimerHandle);
        }
    }

    psHwCtx->bMonitorBusy = IMG_FALSE;
    psHwCtx->bPrevHwStateSet = IMG_FALSE;
    psHwCtx->sPrevState.ui32DWRRetry = 0;
}


#ifdef FW_PRINT

// Important, verify the base address of the debug words in VDECFW_sFirmwareState
#define READ_REQUEST(psHwCtx, pui32Register)                                \
MSVDX_ReadVLR     (                                                         \
        psHwCtx->hMsvdx,                                                    \
        VLR_FIRMWARE_STATE_AREA_BASE_ADDR+sizeof(VDECFW_sFirmwareState)-12, \
        (IMG_UINT32*)pui32Register,                                         \
        1);


#define WRITE_ACKNOWLEDGE(psHwCtx, pui32Register)                           \
MSVDX_WriteVLR(                                                             \
        psHwCtx->hMsvdx,                                                    \
        VLR_FIRMWARE_STATE_AREA_BASE_ADDR+sizeof(VDECFW_sFirmwareState)-8,  \
       (IMG_UINT32*)pui32Register,                                          \
        1);


#define READ_DATA32(psHwCtx, pui32Register)                                 \
MSVDX_ReadVLR(                                                              \
        psHwCtx->hMsvdx,                                                    \
        VLR_FIRMWARE_STATE_AREA_BASE_ADDR+sizeof(VDECFW_sFirmwareState)-4 , \
        (IMG_UINT32*)pui32Register,                                         \
        1);

#ifdef SYSBRG_BRIDGING
#define MODE_PRINT printk
#else
#define MODE_PRINT printf
#endif

#define MAX_STATIC_STRING_SIZE  1024

static IMG_BOOL
hwctrl_MTXPrintTask(
    IMG_VOID * pvParam
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *)pvParam;
    IMG_UINT32  ui32Word, ui32SavedWord;
    IMG_UINT32  ui32StringSize;
    IMG_BOOL    bContainsValue;
    IMG_UINT8   pui8String[MAX_STATIC_STRING_SIZE];
    IMG_UINT32  i;

    if (psHwCtx->bPrintTaskRequestedActive == IMG_FALSE)
    {
        return IMG_FALSE;   // we don't want the task to be kicked again by the timer
    }
    if (psHwCtx->bPrintTaskBusy == IMG_TRUE)
    {
        return IMG_TRUE;   // we want the task to be kicked again by the timer
    }

    psHwCtx->bPrintTaskBusy = IMG_TRUE;

    ui32Word = 0;
    WRITE_ACKNOWLEDGE(psHwCtx, &ui32Word); // Always zero Acknowledge at start to eliminate any possible miscommunications

    READ_REQUEST(psHwCtx, &ui32Word);   // Check if FW has requested any transfer
    ui32SavedWord = ui32Word;

    if (ui32Word == 0)
    {
        psHwCtx->bPrintTaskBusy = IMG_FALSE;
        return IMG_TRUE;    // we want the task to be kicked again by the timer
    }

    MODE_PRINT("FIRMWARE MESSAGE: ");   // Acknowledge the request

    WRITE_ACKNOWLEDGE(psHwCtx, &ui32Word);

    ui32StringSize = (ui32Word & 0x7FFF);   // 1024 bytes is the maximum string size allowed dispite this value
    bContainsValue = (ui32Word > 0x7fff);   // Contains also a numerical value

    // if string length is >0
    if (ui32StringSize > 0)
    {
        if (ui32StringSize > MAX_STATIC_STRING_SIZE)
        {
            MODE_PRINT("Not enough memory for firmware printing, something may be wrong with the printing function\n");
            ui32Word = 0;
            WRITE_ACKNOWLEDGE(psHwCtx, &ui32Word);
            psHwCtx->bPrintTaskBusy = IMG_FALSE;
            return IMG_TRUE;    // we want the task to be kicked again by the timer
        }

        for (i=4; i<=ui32StringSize; i+=4)        // read in 4-byte words
        {
            do
            {
                READ_REQUEST(psHwCtx, &ui32Word);
            }while (ui32Word == ui32SavedWord);    // wait for request value change
            ui32SavedWord = ui32Word;

            if (i != ui32Word)    // error, send 0 and reinitialize
            {
                MODE_PRINT("FW print lost synchronisation while string reading, reinitialize\n");
                ui32Word = 0;
                WRITE_ACKNOWLEDGE(psHwCtx, &ui32Word);
                psHwCtx->bPrintTaskBusy = IMG_FALSE;
                return IMG_TRUE;    // we want the task to be kicked again by the timer
            }

            READ_DATA32(psHwCtx, &ui32Word);
            pui8String[i-4] =  ui32Word      & 0xFF;
            pui8String[i-3] = (ui32Word>>8)  & 0xFF;
            pui8String[i-2] = (ui32Word>>16) & 0xFF;
            pui8String[i-1] = (ui32Word>>24) & 0xFF;

            WRITE_ACKNOWLEDGE(psHwCtx, &i);
        }

        if (i-4 < ui32StringSize)    // read the last 1, 2 or 3 bytes if they remain (cannot be 4+ and for 0 do nothing)
        {
            do
            {
                READ_REQUEST(psHwCtx, &ui32Word);
            }while (ui32Word == ui32SavedWord);    // wait for request value change
            ui32SavedWord = ui32Word;

            if (ui32StringSize != ui32Word)    // error, send 0 and reinitialize
            {
                MODE_PRINT("FW print lost synchronisation while string ending reading, reinitialize\n");
                ui32Word = 0;
                WRITE_ACKNOWLEDGE(psHwCtx, &ui32Word);
                psHwCtx->bPrintTaskBusy = IMG_FALSE;
                return IMG_TRUE;    // we want the task to be kicked again by the timer
            }

            READ_DATA32(psHwCtx, &ui32Word);
            switch (ui32StringSize-(i-4))
            {
            case 3:     pui8String[i-2] = (ui32Word>>16) & 0xFF;
            case 2:     pui8String[i-3] = (ui32Word>>8)  & 0xFF;
            case 1:     pui8String[i-4] =  ui32Word      & 0xFF;
            default:    break;
            }

            WRITE_ACKNOWLEDGE(psHwCtx, &ui32StringSize);
        }

        MODE_PRINT("%s", pui8String);
    }

    if (bContainsValue)
    {
        do
        {
            READ_REQUEST(psHwCtx, &ui32Word);
        }while (ui32Word == ui32SavedWord);    // wait for request value change
        ui32SavedWord = ui32Word;

        if (0x8001 != ui32Word)    // error, send 0 and reinitialize
        {
            MODE_PRINT("FW print lost synchronisation while value reading, reinitialize\n");
            ui32Word = 0;
            WRITE_ACKNOWLEDGE(psHwCtx, &ui32Word);
            psHwCtx->bPrintTaskBusy = IMG_FALSE;
            return IMG_TRUE;    // we want the task to be kicked again by the timer
        }

        READ_DATA32(psHwCtx, &ui32Word);

        MODE_PRINT("\t--> 0x%08lx", ui32Word);

        WRITE_ACKNOWLEDGE(psHwCtx, &ui32SavedWord);
    }

    MODE_PRINT("\n");

    do
    {
        READ_REQUEST(psHwCtx, &ui32Word);
    }while (ui32Word == ui32SavedWord);    // wait for request value change

    if (0 != ui32Word)    // error, send 0 and reinitialize
    {
        MODE_PRINT("FW print lost synchronisation while finishing, reinitialize\n");
        ui32Word = 0;
        WRITE_ACKNOWLEDGE(psHwCtx, &ui32Word);
        psHwCtx->bPrintTaskBusy = IMG_FALSE;
        return IMG_TRUE;    // we want the task to be kicked again by the timer
    }

    WRITE_ACKNOWLEDGE(psHwCtx, &ui32Word); // Always zero Acknowledge at start to eliminate any possible miscommunications

    psHwCtx->bPrintTaskBusy = IMG_FALSE;
    return IMG_TRUE;    // we want the task to be kicked again by the timer
}
#endif //FW_PRINT


static IMG_BOOL
hwctrl_HwStateIsLockUp(
    HWCTRL_sHwCtx * psHwCtx
)
{
    IMG_BOOL bReturnVal = IMG_FALSE;

    // Check if the HW State indicates a LockUp
    if (!LST_empty(&psHwCtx->sPendPictList))
    {
        IMG_UINT32      ui32FreeSlots;
        IMG_UINT32      ui32Load;

        HWCTRL_GetCoreStatus(psHwCtx, &ui32FreeSlots, &ui32Load, &psHwCtx->sState);

        // Make sure we actually have set the Previous State once at least before checking if it has been remained unmodified
        // Without this we may compare the first state to uninitialised state
        if (psHwCtx->bPrevHwStateSet)
        {
            MSVDXIO_sState * psCurState = &psHwCtx->sState.sMsvdx;
            MSVDXIO_sState * psPrevState = &psHwCtx->sPrevState.sMsvdx;

            // If HW state has not been modified for this core since the last time and there
            // is no progress of data transfer to SR
            if ( (psPrevState->sFwState.aui32CheckPoint[VDECFW_CHECKPOINT_PICTURE_STARTED] ==
                  psCurState->sFwState.aui32CheckPoint[VDECFW_CHECKPOINT_PICTURE_STARTED]) &&
                 (psPrevState->sFwState.aui32CheckPoint[VDECFW_CHECKPOINT_BE_PICTURE_COMPLETE] ==
                  psCurState->sFwState.aui32CheckPoint[VDECFW_CHECKPOINT_BE_PICTURE_COMPLETE]) &&
                 (psCurState->ui32DMACStatus == psPrevState->ui32DMACStatus)
               )
            {
                VDECFW_eCodecType eCodec = psCurState->sFwState.ui8CurCodec;
                /* We support large JPEGs, which can be encoded in one slice. There is no way to determine if
                   HW is locked up in this case (latest MBs are not updated in ENTDEC_INFORMATION register
                   for JPEG), so retry timer <psHwCtx->sDevConfig.ui32DwrJPEGRetry> times. */
                if (eCodec == VDECFW_CODEC_JPEG)
                {
                    if (psHwCtx->sPrevState.ui32DWRRetry < psHwCtx->sDevConfig.ui32DwrJPEGRetry)
                    {
                        psHwCtx->sState.ui32DWRRetry++;
                    }
                    else
                    {
                        bReturnVal = IMG_TRUE;
                    }
                }
                // Now test for backend macroblock number
                else if ( (psCurState->ui32BeMbX == psPrevState->ui32BeMbX) &&
                          (psCurState->ui32BeMbY == psPrevState->ui32BeMbY) &&
                          (psCurState->ui32FeMbX == psPrevState->ui32FeMbX) &&
                          (psCurState->ui32FeMbY == psPrevState->ui32FeMbY)
                        )
                {
                    MSVDX_sIntStatus    sIntStatus;
                    IMG_UINT32          ui32Result;

                    IMG_MEMSET(&sIntStatus, 0, sizeof(sIntStatus));

                    bReturnVal = IMG_TRUE;
                    {
                        // Check that we haven't received any messages since last time
                        IMG_UINT32 i;
                        for (i = 0; i < VDECFW_MSGID_COMPLETION_TYPES; i++)
                        {
                            if ((psHwCtx->sState.sFwMsgStatus.aui8CompletionFenceID[i] !=
                                psHwCtx->sPrevState.sFwMsgStatus.aui8CompletionFenceID[i]) ||
                                (psHwCtx->sState.sHostMsgStatus.aui8CompletionFenceID[i] !=
                                psHwCtx->sPrevState.sHostMsgStatus.aui8CompletionFenceID[i]))
                            {
                                bReturnVal = IMG_FALSE;
                                break;
                            }
                        }
                        if ((psPrevState->sFwState.ui32BESlices != psCurState->sFwState.ui32BESlices) ||
                            (psPrevState->sFwState.ui32FESlices != psCurState->sFwState.ui32FESlices))
                        {
                            bReturnVal = IMG_FALSE;
                        }
                    }
                    if (bReturnVal)
                    {
                         /* Lets dump some useful debug information from the Hardware */
                        ui32Result = MSVDX_HandleInterrupts(psHwCtx->hMsvdx, &sIntStatus);
                        IMG_ASSERT(ui32Result == IMG_SUCCESS);

                        REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "====================================================");
                        if (psCurState->ui32MSVDXIntStatus & MSVDX_CORE_CR_MSVDX_INTERRUPT_STATUS_CR_MTX_COMMAND_TIMEOUT_IRQ_MASK)
                        {
                            REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "There might be a core lock-up (possibly due to MTX");
                            REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "command timeout). Dumping debug info:");
                        }
                        else
                        {
                            REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "There might be a core lock-up. Dumping debug info:");
                        }
                        REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "Back-End MbX                          [% 10d]", psPrevState->ui32BeMbX);
                        REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "Back-End MbY                          [% 10d]", psPrevState->ui32BeMbY);
                        REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "Front-End MbX                         [% 10d]", psPrevState->ui32FeMbX);
                        REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "Front-End MbY                         [% 10d]", psPrevState->ui32FeMbY);

                        REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "VDECFW_CHECKPOINT_BE_PICTURE_COMPLETE [0x%08X]", psCurState->sFwState.aui32CheckPoint[VDECFW_CHECKPOINT_BE_PICTURE_COMPLETE]);
                        REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "VDECFW_CHECKPOINT_PICTURE_STARTED     [0x%08X]", psCurState->sFwState.aui32CheckPoint[VDECFW_CHECKPOINT_PICTURE_STARTED]);
                        REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "DMACStatus                            [0x%08X]", psPrevState->ui32DMACStatus);
                        REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "Interrupt Status                      [0x%08X]", sIntStatus.ui32Pending);
                        REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE, "====================================================");
                    }

                    /* Now lets dump The complete Register spaces for CORE, MTX, VEC, VDMC, VDEB, DMAC and VLR. Also Dump RTM register for HW Debug. */
                    if (psHwCtx->sDevConfig.bCoreDump)
                    {
                        MSVDX_DumpRegisters(psHwCtx->hMsvdx);
                    }
                }
            }
        }
        else
        {
            psHwCtx->bPrevHwStateSet = IMG_TRUE;
        }

        IMG_MEMCPY( &psHwCtx->sPrevState, &psHwCtx->sState, sizeof(HWCTRL_sState) );
    }

    return bReturnVal;
}


IMG_BOOL HWCTRL_HwStateIsLockUp(IMG_VOID * pvParam)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *)pvParam;

    if(hwctrl_HwStateIsLockUp(psHwCtx))
    {
        return IMG_TRUE;
    }
    else
    {
        psHwCtx->bMonitorBusy = IMG_FALSE;
        return IMG_FALSE;
    }
}

static IS_NOT_USED IMG_BOOL
hwctrl_MonitorHwState(
    IMG_VOID * pvParam
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *)pvParam;
    IMG_UINT32 ui32Result;

    // bMonitorBusy is used to detect if we are already in a process of resetting the core
    if (psHwCtx->bMonitorBusy)
    {
        // ALWAYS return true if you want the timer to keep running
        return IMG_TRUE;
    }

    // Check for state and reset if needed
    if (hwctrl_HwStateIsLockUp(psHwCtx))
    {
        psHwCtx->bMonitorBusy = IMG_TRUE;

        // Do the Device Reset
        // ...
        ui32Result = CORE_DevServiceTimeExpiry(psHwCtx->pvCompInitUserData, psHwCtx->ui32CoreNum);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            //return ui32Result;
        }
    }

    // ALWAYS return true if you want the timer to keep running
    return IMG_TRUE;
}


#ifdef VDEC_MSVDX_HARDWARE

/*!
******************************************************************************

 @Function              hwctrl_CalcMbLoad

******************************************************************************/
static IMG_UINT32
hwctrl_CalcMbLoad(
    IMG_UINT32          ui32NumMbs,
    IMG_BOOL            bIntraCoded
)
{
    IMG_UINT32 ui32Load = 0;

    ui32Load = ui32NumMbs;
    if (bIntraCoded)
    {
        //ui32Load = (ui32Load*3)/2;
    }

    return ui32Load;
}


/*!
******************************************************************************

 @Function              hwctrl_CalculateLoad

******************************************************************************/
static IMG_UINT32
hwctrl_CalculateLoad(
    BSPP_sPictHdrInfo *  psPictHdrInfo
)
{
    IMG_UINT32 ui32NumMbs;

    ui32NumMbs = ((psPictHdrInfo->sCodedFrameSize.ui32Width+15)/16) * ((psPictHdrInfo->sCodedFrameSize.ui32Height+15)/16);

    return hwctrl_CalcMbLoad(ui32NumMbs, psPictHdrInfo->bIntraCoded);
}


/*!
******************************************************************************

 @Function              hwctrl_SendInitMessage

 @Description

 This function sends the init message to the firmware.

******************************************************************************/
static IMG_RESULT hwctrl_SendInitMessage(
    HWCTRL_sHwCtx                 * psHwCtx,
    const VDECFW_sCoreInitData    * psCoreInitData,
    const MSVDXIO_sDdBufInfo      * psDevPtdBufInfo
)
{
    IMG_UINT32  i;

    /* Create a control picture message here from the config. */
    IMG_UINT8 aui8Msg[V2_INITMSG_SIZE];
    IMG_UINT8 * pui8Msg = &aui8Msg[0];

    VDEC_BZERO(&aui8Msg);
    MEMIO_WRITE_FIELD(pui8Msg,V2_INITMSG_SIZE, sizeof(aui8Msg));
    MEMIO_WRITE_FIELD(pui8Msg,V2_INITMSG_MID, VDECFW_MSGID_FIRMWARE_INIT);

    MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_COREID, psHwCtx->ui32CoreNum);

    if (psHwCtx->sDevConfig.eDecodeLevel == VDECDD_DECODELEVEL_FWHDRPARSING)
    {
        MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_PARSEONLY, 1);
    }
    else if (psHwCtx->sDevConfig.eDecodeLevel == VDECDD_DECODELEVEL_FEHWDECODING)
    {
        MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_FAKEBE, 1);
    }

    if (psHwCtx->sDevConfig.bLockStepDecode)
    {
        MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_LOCKSTEP, 1);
    }

    if (psHwCtx->sDevConfig.bPerformanceLog)
    {
        MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_PEFORMANCELOG, 1);
    }

    if (!psHwCtx->sDevConfig.bOptimisedPerformance)
    {
        MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_DISABLEOPT, 1);
    }

    //Updating the host pointer because the value was temporary stored in the device memory
    UPDATE_HOST(psDevPtdBufInfo, IMG_TRUE);

    // Populate MMU configuration.
    MEMIO_WRITE_TABLE_FIELD(pui8Msg, V2_INITMSG_MMU_DIR_LIST_BASE, 0, *((IMG_UINT32*)psDevPtdBufInfo->pvCpuVirt));

    MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_MMU_CONTROL2, psCoreInitData->sMmuConfig.ui32MmuControl2);
    MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_MMU_36_BIT_TWIDDLE, psCoreInitData->sMmuConfig.b36bitMemTwiddle);

    // Populate Rendec configuration.
    MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_RENDEC_CONTROL0, psCoreInitData->sRendecConfig.ui32RegVecRendecControl0);
    MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_RENDEC_CONTROL1, psCoreInitData->sRendecConfig.ui32RegVecRendecControl1);
    MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_RENDEC_BUFFER_ADDR0, psHwCtx->aui32RendecBufInfo[0].ui32DevVirt);
    MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_RENDEC_BUFFER_ADDR1, psHwCtx->aui32RendecBufInfo[1].ui32DevVirt);
    MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_RENDEC_BUFFER_SIZE, psCoreInitData->sRendecConfig.ui32RegVecRendecBufferSize);
    for (i = 0; i < 6; i++)
    {
        MEMIO_WRITE_TABLE_FIELD(pui8Msg, V2_INITMSG_RENDEC_INITIAL_CONTEXT, i, psCoreInitData->sRendecConfig.aui32RendecInitialContext[i]);
    }

    MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_VEC_CONTROL_2, psCoreInitData->ui32RegVecControl2);

    /* Send the message to the firmware */
    MEMIO_WRITE_FIELD(pui8Msg, V2_INITMSG_FENCE_ID, ((++(psHwCtx->sHostMsgStatus.aui8ControlFenceID[VDECFW_MSGID_FIRMWARE_INIT-VDECFW_MSGID_BASE_PADDING]))&0xFF));

    DEBUG_REPORT(REPORT_MODULE_HWCTRL, "[MID=0x%02X] [CONTROL] [0x%02X]",
        psHwCtx->sHostMsgStatus.aui8ControlFenceID[VDECFW_MSGID_FIRMWARE_INIT-VDECFW_MSGID_BASE_PADDING], VDECFW_MSGID_FIRMWARE_INIT);

    MSVDX_SendFirmwareMessage(psHwCtx->hMsvdx, MTXIO_AREA_CONTROL, aui8Msg);

    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function              hwctrl_SendDecodeMessage

 @Description

 This function sends the decode message to the firmware.

******************************************************************************/
static IMG_RESULT
hwctrl_SendDecodeMessage(
    HWCTRL_sHwCtx         * psHwCtx,
    MSVDXIO_sDdBufInfo    * psTransactionBufInfo,
    MSVDXIO_sDdBufInfo    * psStrPtdBufInfo
)
{
    /* Create a decode picture message here from the transaction. */
    IMG_UINT8 aui8Msg[V2_DECODEMSG_SIZE];
    IMG_UINT8 * pui8Msg = &aui8Msg[0];

    VDEC_BZERO(&aui8Msg);
    MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_SIZE, sizeof(aui8Msg));
    MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_MID, VDECFW_MSGID_DECODE_PICTURE);

    // Must write the Ptd value directly to message so that it goes into Pdump.

    //Updating the host pointer because the value was temporary stored in the device memory
    UPDATE_HOST(psStrPtdBufInfo, IMG_TRUE);

    MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_STREAM_PTD, *((IMG_UINT32*)psStrPtdBufInfo->pvCpuVirt));

    MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_TRANSACTION_ADDR, GET_HOST_ADDR(psTransactionBufInfo, !psHwCtx->sDevConfig.bFakeMtx));
    MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_TRANSACTION_SIZE, sizeof(VDECFW_sTransaction));
    UPDATE_DEVICE(psTransactionBufInfo, IMG_TRUE);

    if (psHwCtx->sPsrModInfo.hMemory)
    {
        MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_PSRMODINFO_ADDR, GET_HOST_ADDR(&psHwCtx->sPsrModInfo, !psHwCtx->sDevConfig.bFakeMtx));
        MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_PSRMODINFO_SIZE, sizeof(VDECFW_sPsrModInfo) * VDECFW_CODEC_MAX);
        UPDATE_DEVICE((&psHwCtx->sPsrModInfo), IMG_TRUE);
    }

    /* Send the message to the firmware */
    MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_FENCE_ID, ((++(psHwCtx->sHostMsgStatus.aui8DecodeFenceID[VDECFW_MSGID_DECODE_PICTURE-VDECFW_MSGID_PSR_PADDING]))&0xFF));

    DEBUG_REPORT(REPORT_MODULE_HWCTRL, "[MID=0x%02X] [DECODE] [0x%02X]",
        psHwCtx->sHostMsgStatus.aui8DecodeFenceID[VDECFW_MSGID_DECODE_PICTURE-VDECFW_MSGID_PSR_PADDING], VDECFW_MSGID_DECODE_PICTURE);

    MSVDX_SendFirmwareMessage(psHwCtx->hMsvdx, MTXIO_AREA_DECODE, aui8Msg);

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              hwctrl_SendBatchMessage

 @Description

 This function sends a batch message that contains a decode message,
 a number of bitstream buffer messages and the end bytes.

******************************************************************************/
static IMG_RESULT
hwctrl_SendBatchMessage(
    HWCTRL_sHwCtx         * psHwCtx,
    MSVDXIO_sDdBufInfo    * psTransactionBufInfo,
    MSVDXIO_sDdBufInfo    * psStrPtdBufInfo,
    MSVDXIO_sDdBufInfo    * psBatchMsgBufInfo,
    LST_T                 * psDecPictSegList,
    MSVDXIO_sDdBufInfo    * psStartCodeBufInfo,
    MSVDXIO_sDdBufInfo    * psEndBytesBufInfo
)
{
    IMG_UINT8 aui8Msg[V2_BATCHMSG_SIZE];
    IMG_UINT8 * pui8Msg = psBatchMsgBufInfo->pvCpuVirt;
    IMG_UINT32 ui32Count = 0;
    IMG_UINT8 ui8BatchMessageID = 0;

    DECODER_sDecPictSeg * psDecPictSeg = IMG_NULL;

    //Calculate the size the messages need
    psDecPictSeg = (DECODER_sDecPictSeg *) LST_first(psDecPictSegList);
    while(psDecPictSeg != IMG_NULL)
    {
        //Don't send to the FW the VDECDD_BSSEG_SKIP segments
        if((psDecPictSeg->psBitStrSeg && psDecPictSeg->psBitStrSeg->ui32BitStrSegFlag&VDECDD_BSSEG_SKIP) == 0)
        {
            if (psDecPictSeg->psBitStrSeg->ui32BitStrSegFlag &VDECDD_BSSEG_INSERTSCP)
            {
                ui32Count++;
            }
            ui32Count++;
        }
        psDecPictSeg = LST_next(psDecPictSeg);
    }
    //If it is bigger than the buffer fail.
    IMG_ASSERT(V2_DECODEMSG_SIZE + ui32Count*V2_BUFFERMSG_SIZE <= BATCH_MSG_BUFFER_SIZE);
    if(V2_DECODEMSG_SIZE + ui32Count*V2_BUFFERMSG_SIZE > BATCH_MSG_BUFFER_SIZE)
    {
        //This is temporary and the size of the buffer will be arranged to
        //never fail.
        return ~IMG_SUCCESS;
    }

    IMG_MEMSET(pui8Msg, 0, psBatchMsgBufInfo->ui32BufSize);

    /* Create Decode Message */
    MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_SIZE, V2_DECODEMSG_SIZE);
    MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_MID, VDECFW_MSGID_DECODE_PICTURE);

    // Must write the Ptd value directly to message so that it goes into Pdump.

    //Updating the host pointer because the value was temporary stored in the device memory
    UPDATE_HOST(psStrPtdBufInfo, IMG_TRUE);

    MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_STREAM_PTD, *((IMG_UINT32*)psStrPtdBufInfo->pvCpuVirt));

    MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_TRANSACTION_ADDR, GET_HOST_ADDR(psTransactionBufInfo, !psHwCtx->sDevConfig.bFakeMtx));
    MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_TRANSACTION_SIZE, sizeof(VDECFW_sTransaction));
    UPDATE_DEVICE(psTransactionBufInfo, IMG_TRUE);

    if (psHwCtx->sPsrModInfo.hMemory)
    {
        MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_PSRMODINFO_ADDR, GET_HOST_ADDR(&psHwCtx->sPsrModInfo, !psHwCtx->sDevConfig.bFakeMtx));
        MEMIO_WRITE_FIELD(pui8Msg, V2_DECODEMSG_PSRMODINFO_SIZE, sizeof(VDECFW_sPsrModInfo) * VDECFW_CODEC_MAX);
        UPDATE_DEVICE((&psHwCtx->sPsrModInfo), IMG_TRUE);
    }

    pui8Msg += V2_DECODEMSG_SIZE;

    ui8BatchMessageID = ((++(psHwCtx->sHostMsgStatus.aui8DecodeFenceID[VDECFW_MSGID_BATCH-VDECFW_MSGID_PSR_PADDING]))&0xFF);

    DEBUG_REPORT(REPORT_MODULE_HWCTRL, "[MID=0x%02X] [DECODE] [0x%02X]",
        ui8BatchMessageID, VDECFW_MSGID_BATCH);

    /* Create Bitstream messages */
    psDecPictSeg = (DECODER_sDecPictSeg *) LST_first(psDecPictSegList);
    while(psDecPictSeg != IMG_NULL)
    {
        //Don't send to the FW the VDECDD_BSSEG_SKIP segments
        if((psDecPictSeg->psBitStrSeg && psDecPictSeg->psBitStrSeg->ui32BitStrSegFlag&VDECDD_BSSEG_SKIP) == 0)
        {
            VDECDD_sDdBufMapInfo *  psDdBufMapInfo;
            IMG_UINT32 ui32Result;
            /* Get access to map info context...*/
            ui32Result = RMAN_GetResource(psDecPictSeg->psBitStrSeg->ui32BufMapId, VDECDD_BUFMAP_TYPE_ID, (IMG_VOID **)&psDdBufMapInfo, IMG_NULL);
            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                return ui32Result;
            }

            if (psDecPictSeg->psBitStrSeg->ui32BitStrSegFlag & VDECDD_BSSEG_INSERTSCP)
            {
                IMG_UINT32 ui32StartCodeLength = psStartCodeBufInfo->ui32BufSize;

                if (psDecPictSeg->psBitStrSeg->ui32BitStrSegFlag & VDECDD_BSSEG_INSERT_STARTCODE)
                {
                    IMG_UINT8 * pui8StartCode = psStartCodeBufInfo->pvCpuVirt;

                    pui8StartCode[ui32StartCodeLength - 1] = psDecPictSeg->psBitStrSeg->ui8StartCodeSuffix;

                    UPDATE_DEVICE(psStartCodeBufInfo, IMG_TRUE);
                }
                else
                {
                    ui32StartCodeLength -= 1;
                }

                MEMIO_WRITE_FIELD(pui8Msg,V2_BUFFERMSG_SIZE,V2_BUFFERMSG_SIZE);
                MEMIO_WRITE_FIELD(pui8Msg,V2_BUFFERMSG_MID,VDECFW_MSGID_BITSTREAM_BUFFER);
                MEMIO_WRITE_FIELD(pui8Msg,V2_BUFFERMSG_FENCE_ID, ((++(psHwCtx->sHostMsgStatus.aui8DecodeFenceID[VDECFW_MSGID_BITSTREAM_BUFFER-VDECFW_MSGID_PSR_PADDING]))&0xFF));

                MEMIO_WRITE_FIELD(pui8Msg, V2_BUFFERMSG_DATAADDR, psStartCodeBufInfo->ui32DevVirt);
                MEMIO_WRITE_FIELD(pui8Msg, V2_BUFFERMSG_DATASIZE, ui32StartCodeLength);

                pui8Msg += V2_BUFFERMSG_SIZE;

                DEBUG_REPORT(REPORT_MODULE_HWCTRL, "[MID=0x%02X] [DECODE] [0x%02X]",
                    psHwCtx->sHostMsgStatus.aui8DecodeFenceID[VDECFW_MSGID_BITSTREAM_BUFFER-VDECFW_MSGID_PSR_PADDING], VDECFW_MSGID_BITSTREAM_BUFFER);
            }

            MEMIO_WRITE_FIELD(pui8Msg,V2_BUFFERMSG_SIZE,V2_BUFFERMSG_SIZE);
            MEMIO_WRITE_FIELD(pui8Msg,V2_BUFFERMSG_MID,VDECFW_MSGID_BITSTREAM_BUFFER);
            MEMIO_WRITE_FIELD(pui8Msg,V2_BUFFERMSG_FENCE_ID, ((++(psHwCtx->sHostMsgStatus.aui8DecodeFenceID[VDECFW_MSGID_BITSTREAM_BUFFER-VDECFW_MSGID_PSR_PADDING]))&0xFF));

            MEMIO_WRITE_FIELD(pui8Msg, V2_BUFFERMSG_DATAADDR, psDdBufMapInfo->sDdBufInfo.ui32DevVirt + psDecPictSeg->psBitStrSeg->ui32DataByteOffset);
            MEMIO_WRITE_FIELD(pui8Msg, V2_BUFFERMSG_DATASIZE, psDecPictSeg->psBitStrSeg->ui32DataSize);

            IMG_ASSERT(psDecPictSeg->psBitStrSeg->ui32DataSize <= psDdBufMapInfo->sDdBufInfo.ui32BufSize);

            UPDATE_DEVICE((&psDdBufMapInfo->sDdBufInfo), IMG_TRUE);

            pui8Msg += V2_BUFFERMSG_SIZE;

            DEBUG_REPORT(REPORT_MODULE_HWCTRL, "[MID=0x%02X] [DECODE] [0x%02X]",
                psHwCtx->sHostMsgStatus.aui8DecodeFenceID[VDECFW_MSGID_BITSTREAM_BUFFER-VDECFW_MSGID_PSR_PADDING], VDECFW_MSGID_BITSTREAM_BUFFER);
        }
        psDecPictSeg = LST_next(psDecPictSeg);
    }

    /* Finally create the Batch message */
    VDEC_BZERO(&aui8Msg);
    pui8Msg = &aui8Msg[0];

    MEMIO_WRITE_FIELD(pui8Msg, V2_BATCHMSG_SIZE, V2_BATCHMSG_SIZE);
    MEMIO_WRITE_FIELD(pui8Msg, V2_BATCHMSG_MID, VDECFW_MSGID_BATCH);
    MEMIO_WRITE_FIELD(pui8Msg, V2_BATCHMSG_FLAGS, 0);
    MEMIO_WRITE_FIELD(pui8Msg, V2_BATCHMSG_STREAM_PTD, *((IMG_UINT32*)psStrPtdBufInfo->pvCpuVirt));
    MEMIO_WRITE_FIELD(pui8Msg, V2_BATCHMSG_BATCHADDR, GET_HOST_ADDR(psBatchMsgBufInfo, !psHwCtx->sDevConfig.bFakeMtx));
    MEMIO_WRITE_FIELD(pui8Msg, V2_BATCHMSG_BATCHSIZE, V2_DECODEMSG_SIZE + ui32Count*V2_BUFFERMSG_SIZE);

    UPDATE_DEVICE(psBatchMsgBufInfo, IMG_TRUE);

    MEMIO_WRITE_FIELD(pui8Msg, V2_BATCHMSG_FENCE_ID, ui8BatchMessageID);

    MSVDX_SendFirmwareMessage(psHwCtx->hMsvdx, MTXIO_AREA_DECODE, aui8Msg);

    return IMG_SUCCESS;

}

/*
******************************************************************************

 @Function              hwctrl_ProcessFWMsg

 @Description

 This function reads out firmware message into the picture in the pending list

 @Input     pui32Msg        : Pointer to the message.

 @Input        psPendPictList    : Pointer to the hwctrl pending list.

 @Output    pbDecodedMsg    : Pointer to boolean to return whether messages was decoded message.

 @Return    IMG_RESULT      : This function returns either IMG_SUCCESS or an
                              error code.

******************************************************************************/
static IMG_RESULT
hwctrl_ProcessFWMsg(
    HWCTRL_sHwCtx * psHwCtx,
    IMG_UINT32 *  pui32Msg,
    LST_T *       psPendPictList,
#ifndef IMG_KERNEL_MODULE
    IMG_BOOL      bPdumpAndRes,
#endif
    IMG_BOOL *    pbDecodedMsg
)
{
    IMG_UINT32  ui32MsgId;
    IMG_UINT32 ui32Tid = 0;
    DECODER_sDecPict * psDecPict;
    VDEC_sPictDecoded * psPictDecoded = IMG_NULL;
    VDECFW_sPerformanceData * psPerformanceData = IMG_NULL;
    VDEC_sPictHwCrc * psPictHwCrc = IMG_NULL;

    ui32Tid = MEMIO_READ_FIELD(pui32Msg, V2_DECODEDMSG_TID);
    psDecPict = LST_first(psPendPictList);
    while(psDecPict)
    {
        if (psDecPict->ui32TransactionId == ui32Tid)
        {
            break;
        }
        psDecPict = LST_next(psDecPict);
    }

    // We must have a picture in the list that matches the tid
    if(psDecPict == IMG_NULL)
    {
        IMG_ASSERT(IMG_FALSE);

        REPORT(REPORT_MODULE_HWCTRL, REPORT_ERR,
            "Firmware decoded message [TID: 0x%08X] received with no pending picture", ui32Tid);
        return IMG_ERROR_FATAL;
    }

    ui32MsgId = MEMIO_READ_FIELD(pui32Msg, V2_DECODEMSG_MID);

    switch ( ui32MsgId )
    {
    case VDECFW_MSGID_PIC_DECODED:
        *pbDecodedMsg = IMG_TRUE;

        if(psDecPict->psFirstFieldFwMsg->sPictDecoded.bFirstFieldReceived)
        {
            psPictDecoded = &psDecPict->psSecondFieldFwMsg->sPictDecoded;
        }
        else
        {
            psPictDecoded = &psDecPict->psFirstFieldFwMsg->sPictDecoded;
            psDecPict->psFirstFieldFwMsg->sPictDecoded.bFirstFieldReceived = IMG_TRUE;
        }

        psPictDecoded->ui32FEError = MEMIO_READ_FIELD(pui32Msg, V2_DECODEDMSG_FE_ERR);
        psPictDecoded->ui32NoBEDWT = MEMIO_READ_FIELD(pui32Msg, V2_DECODEDMSG_BEWDT);

        //if(FLAG_IS_SET(MEMIO_READ_FIELD(pui32Msg,V2_DECODEDMSG_FLAGS),VDECFW_MSGFLAG_DECODED_SKIP_PICTURE))
        //{
        //    psPictDecoded->bSkipPict = IMG_TRUE;
        //    psPictDecoded->ui32TransIdToFillSkipPict = MEMIO_READ_FIELD(pui32Msg, V2_DECODEDMSG_FILLPICID);
        //}

        psHwCtx->sHostMsgStatus.aui8CompletionFenceID[VDECFW_MSGID_PIC_DECODED-VDECFW_MSGID_BE_PADDING] = MEMIO_READ_FIELD(pui32Msg, V2_DECODEDMSG_FENCE_ID);
        DEBUG_REPORT(REPORT_MODULE_HWCTRL, "[MID=0x%02X] [COMPLETION_DECODED]", MEMIO_READ_FIELD(pui32Msg, V2_DECODEDMSG_FENCE_ID));

#ifndef IMG_KERNEL_MODULE
        if(bPdumpAndRes)
        {
            if(psDecPict->psAltPict != IMG_NULL)
            {
                UPDATE_HOST((&psDecPict->psAltPict->psPictBuf->sDdBufInfo), IMG_TRUE);
            }
            else
            {
                UPDATE_HOST((&psDecPict->psReconPict->psPictBuf->sDdBufInfo), IMG_TRUE);
            }
        }
#endif
        break;

    case VDECFW_MSGID_PIC_CRCS:

        if(psDecPict->psFirstFieldFwMsg->sPictHwCrc.bFirstFieldReceived)
        {
            psPictHwCrc = &psDecPict->psSecondFieldFwMsg->sPictHwCrc;
        }
        else
        {
            psPictHwCrc = &psDecPict->psFirstFieldFwMsg->sPictHwCrc;
            psDecPict->psFirstFieldFwMsg->sPictHwCrc.bFirstFieldReceived = IMG_TRUE;
        }

        // Fill up the CRC data
        psPictHwCrc->ui32CrcShiftRegFE        = /*MEMIO_READ_FIELD(pui32Msg, V2_CRCMSG_CRC_SR_FE)*/1;
        psPictHwCrc->ui32CrcVecCommand        = MEMIO_READ_FIELD(pui32Msg, V2_CRCMSG_CRC_VEC_CMD);
        psPictHwCrc->ui32CrcVecIxform         = MEMIO_READ_FIELD(pui32Msg, V2_CRCMSG_CRC_VEC_IXFORM);
        psPictHwCrc->ui32CrcVdmcPixRecon      = MEMIO_READ_FIELD(pui32Msg, V2_CRCMSG_CRC_VDMC_PIXRECON);
        psPictHwCrc->ui32VdebScaleWriteData   = MEMIO_READ_FIELD(pui32Msg, V2_CRCMSG_CRC_VDEB_SCALE_WDATA);
        psPictHwCrc->ui32VdebScaleAddr        = MEMIO_READ_FIELD(pui32Msg, V2_CRCMSG_CRC_VDEB_BURSTB_ADDR);
        psPictHwCrc->ui32VdebBBWriteData      = MEMIO_READ_FIELD(pui32Msg, V2_CRCMSG_CRC_VDEB_BURSTB_WDATA);
        psPictHwCrc->ui32VdebBBAddr           = MEMIO_READ_FIELD(pui32Msg, V2_CRCMSG_CRC_VDEB_BURSTB_ADDR);
        psPictHwCrc->ui32VdebSysMemAddr       = MEMIO_READ_FIELD(pui32Msg, V2_CRCMSG_CRC_VDEB_SYSMEM_ADDR);
        psPictHwCrc->ui32VdebSysMemWriteData  = MEMIO_READ_FIELD(pui32Msg, V2_CRCMSG_CRC_VDEB_SYSMEM_WDATA);

        psHwCtx->sHostMsgStatus.aui8CompletionFenceID[VDECFW_MSGID_PIC_CRCS-VDECFW_MSGID_BE_PADDING] = MEMIO_READ_FIELD(pui32Msg, V2_CRCMSG_FENCE_ID);
        DEBUG_REPORT(REPORT_MODULE_HWCTRL, "[MID=0x%02X] [COMPLETION_CRC]", MEMIO_READ_FIELD(pui32Msg, V2_CRCMSG_FENCE_ID));
        break;

    case VDECFW_MSGID_PIC_PERFORMANCE:

        if(psDecPict->psFirstFieldFwMsg->sPerformanceData.bFirstFieldReceived)
        {
            psPerformanceData = &psDecPict->psSecondFieldFwMsg->sPerformanceData;
        }
        else
        {
            psPerformanceData = &psDecPict->psFirstFieldFwMsg->sPerformanceData;
            psDecPict->psFirstFieldFwMsg->sPerformanceData.bFirstFieldReceived = IMG_TRUE;
        }

        // Fill up the performance data
        //psPerformanceData->ui16HeightMBS          = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_WIDTH_MBS);
        //psPerformanceData->ui16HeightMBS          = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_HEIGHT_MBS);
        psPerformanceData->ui16NumSlices          = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_NUM_SLICES);
        //psPerformanceData->ui161SliceSize         = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_1SLICE_SIZE);
        psPerformanceData->ui32PictureStarted     = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_PICTURE_STARTED);
        psPerformanceData->ui32FirmwareReady      = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_FIRMWARE_READY);
        psPerformanceData->ui32PicmanComplete     = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_PICMAN_COMPLETE);
        psPerformanceData->ui32FirmwareSaved      = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_FIRMWARE_SAVED);
        psPerformanceData->ui32EntdecStarted      = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_ENTDEC_STARTED);
        psPerformanceData->ui32Fe1sliceDone       = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_FE_1SLICE_DONE);
        psPerformanceData->ui32FePictureComplete  = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_FE_PICTURE_COMPLETE);
        psPerformanceData->ui32BePictureStarted   = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_BE_PICTURE_STARTED);
        psPerformanceData->ui32Be1sliceDone       = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_BE_1SLICE_DONE);
        psPerformanceData->ui32BePictureComplete  = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_BE_PICTURE_COMPLETE);
        psPerformanceData->ui32SyncStart          = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_SYNC_START);
        psPerformanceData->ui32SyncComplete       = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_SYNC_COMPLETE);

        psPerformanceData->ui32TotParseSliceHeader = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_T1);
        psPerformanceData->ui32TotSetupRegisters   = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_T2);
        psPerformanceData->ui32TotVLC              = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_T4);
        psPerformanceData->ui32TotParserLoad       = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_T3);
        psPerformanceData->ui32TotParseAndSetupReg = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_PARSE_AND_SETUP_REG);
        psPerformanceData->ui32TotHwFEDecode       = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_TOT_HW_FE_DECODE);
        psPerformanceData->ui32TotHwBEDecode       = MEMIO_READ_FIELD(pui32Msg, V2_PERFMSG_TOT_HW_BE_DECODE);

        psHwCtx->sHostMsgStatus.aui8CompletionFenceID[VDECFW_MSGID_PIC_PERFORMANCE-VDECFW_MSGID_BE_PADDING] = MEMIO_READ_FIELD(pui32Msg, V2_DECODEDMSG_FENCE_ID);
        DEBUG_REPORT(REPORT_MODULE_HWCTRL, "[MID=0x%02X] [COMPLETION_PERFORMANCE]", MEMIO_READ_FIELD(pui32Msg, V2_DECODEDMSG_FENCE_ID));
        break;

    case VDECFW_MSGID_BE_PADDING:
        psHwCtx->sHostMsgStatus.aui8CompletionFenceID[VDECFW_MSGID_BE_PADDING-VDECFW_MSGID_BE_PADDING] = MEMIO_READ_FIELD(pui32Msg, V2_PADMSG_FENCE_ID);
        DEBUG_REPORT(REPORT_MODULE_HWCTRL, "[MID=0x%02X] [COMPLETION_BE_PADDING]", MEMIO_READ_FIELD(pui32Msg, V2_PADMSG_FENCE_ID));
        break;

    default:
        IMG_ASSERT(IMG_FALSE);
        break;
    }

    return IMG_SUCCESS;
}



/*
******************************************************************************

 @Function              decoder_ProcessMTXMsgs

 @Description

 This function is used for now to read through firmware messages and send Dev HW Interrupt.

 @Input     psDdDevContext  : Pointer to hwctrl context.

 @Input     pvUserData      : Pointer to component wide user data.

 @Return    IMG_RESULT      : This function returns either IMG_SUCCESS or an
                              error code.

******************************************************************************/
static IMG_RESULT
hwctrl_ProcessMTXMsgs(
    HWCTRL_sHwCtx     * psHwCtx,
    MSVDX_sMsgQueue   * psMsgQueue,
    IMG_VOID          * pvUserData
)
{
    IMG_RESULT eResult;

    MSVDXIO_sHISRMsg *  psHISRMsg;
    MSVDXIO_sHISRMsg    sHISRMsg;

    IMG_BOOL  bDecodedMsg;

    /* Process all pending HISR messages. */
    psHISRMsg = LST_first(&psMsgQueue->sNewMsgList);
    while (psHISRMsg != IMG_NULL)
    {
        // Make a local copy of the message.
        sHISRMsg = *psHISRMsg;

        /* Remove the message from the queue...*/
        psHISRMsg = LST_removeHead(&psMsgQueue->sNewMsgList);
        if (psHISRMsg != IMG_NULL)
        {
            /* Process the fw message and populate the Pic */
            bDecodedMsg = IMG_FALSE;
            eResult = hwctrl_ProcessFWMsg(psHwCtx,
                                          &psMsgQueue->aui32MsgBuf[sHISRMsg.ui32MsgIndex],
                                          &psHwCtx->sPendPictList,
#ifndef IMG_KERNEL_MODULE
                                           psHwCtx->sDevConfig.bPdumpAndRes,
#endif
                                          &bDecodedMsg);
            IMG_ASSERT(eResult == IMG_SUCCESS);


            if (sHISRMsg.ui32MsgIndex < psMsgQueue->ui32ReadIdx)
            {
                // We assume here that the buffer wrapped around and this is why the ui32MsgIndex < ui32ReadIdx
                // so the message should start at the beggining of the buffer. Otherwise the ReadIdx should be set
                // at the value of ui32MsgIndex;
                IMG_ASSERT(sHISRMsg.ui32MsgIndex == 0);
                // When the message didn't fit in the last portion of message buffer
                // move the read index to the start of the message.
                psMsgQueue->ui32ReadIdx = 0;
            }

            psMsgQueue->ui32ReadIdx += sHISRMsg.ui32MsgSize;

            /* Free the message */
            LST_add(&psMsgQueue->sFreeMsgList, psHISRMsg);

            if (bDecodedMsg)
            {
#ifndef IMG_KERNEL_MODULE
                eResult = MSVDX_PDUMPUnLock(psHwCtx->hMsvdx);
                IMG_ASSERT(eResult == IMG_SUCCESS);
#endif
                CORE_DevHwInterrupt(pvUserData, psHwCtx->pvDecCore);
            }
        }

        /* Get the next message...*/
        psHISRMsg = LST_first(&psMsgQueue->sNewMsgList);
    }

    return IMG_SUCCESS;
}

#endif // VDEC_MSVDX_HARDWARE

#ifndef IMG_KERNEL_MODULE
/*!
******************************************************************************

 @Function              HWCTRL_SyncPDumpContexts

 @Description

 Sync pdump contexts

******************************************************************************/
IMG_RESULT
HWCTRL_SyncPDumpContexts(
    IMG_HANDLE          hHwCtx
)
{
    HWCTRL_sHwCtx         * psHwCtx;

    IMG_ASSERT(hHwCtx != IMG_NULL);
    if(hHwCtx == IMG_NULL)
    {
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    psHwCtx = (HWCTRL_sHwCtx *)hHwCtx;

    return MSVDX_PDUMPSync(psHwCtx->hMsvdx);
}
#endif

/*!
******************************************************************************

 @Function              HWCTRL_GetCoreStatus

******************************************************************************/
IMG_UINT32
HWCTRL_GetCoreStatus(
    IMG_HANDLE          hHwCtx,
    IMG_UINT32        * pui32FreeSlots,
    IMG_UINT32        * pui32Load,
    HWCTRL_sState     * psState
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;
    IMG_UINT32      ui32Result = IMG_SUCCESS;

    *pui32FreeSlots = psHwCtx->ui32NumFreeSlots;
    *pui32Load = 0;

#ifdef VDEC_MSVDX_HARDWARE
    if (psHwCtx->sDevConfig.eDecodeLevel > VDECDD_DECODELEVEL_DECODER &&
        psHwCtx->bPower)
    {
        DECODER_sDecPict  *  psDecPict;
        IMG_UINT32           ui32PictLoadDone = 0;
        MSVDXIO_sState       sMsvdxIOState;
        MSVDXIO_sState *     psMsvdxIOState = IMG_NULL;

        if (IMG_NULL == psState)
        {
            psMsvdxIOState = &sMsvdxIOState;
        }
        else
        {
            psMsvdxIOState  = &psState->sMsvdx;
        }

        ui32Result = MSVDX_GetCoreState(psHwCtx->hMsvdx, psMsvdxIOState);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            goto error;
        }

        if (IMG_NULL != psState)
        {
            IMG_MEMCPY(psState->sFwMsgStatus.aui8ControlFenceID,
                       psState->sMsvdx.sFwState.aui8ControlFenceID,
                       sizeof(psState->sFwMsgStatus.aui8ControlFenceID));
            IMG_MEMCPY(psState->sFwMsgStatus.aui8DecodeFenceID,
                       psState->sMsvdx.sFwState.aui8DecodeFenceID,
                       sizeof(psState->sFwMsgStatus.aui8DecodeFenceID));
            IMG_MEMCPY(psState->sFwMsgStatus.aui8CompletionFenceID,
                       psState->sMsvdx.sFwState.aui8CompletionFenceID,
                       sizeof(psState->sFwMsgStatus.aui8CompletionFenceID));

            IMG_MEMCPY(psState->sHostMsgStatus.aui8ControlFenceID,
                       psHwCtx->sHostMsgStatus.aui8ControlFenceID,
                       sizeof(psState->sHostMsgStatus.aui8ControlFenceID));
            IMG_MEMCPY(psState->sHostMsgStatus.aui8DecodeFenceID,
                       psHwCtx->sHostMsgStatus.aui8DecodeFenceID,
                       sizeof(psState->sHostMsgStatus.aui8DecodeFenceID));
            IMG_MEMCPY(psState->sHostMsgStatus.aui8CompletionFenceID,
                       psHwCtx->sHostMsgStatus.aui8CompletionFenceID,
                       sizeof(psState->sHostMsgStatus.aui8CompletionFenceID));

            psState->ui32DWRRetry = psHwCtx->sPrevState.ui32DWRRetry;
        }

        // Obtain up-to-date load based upon the progress of the current picture on the back-end.
        psDecPict = LST_first(&psHwCtx->sPendPictList);
        if (psDecPict)
        {
            IMG_UINT32 ui32NumMbProcessed;
            IMG_UINT32 ui32WidthInMb = (psDecPict->psPictHdrInfo->sCodedFrameSize.ui32Width + (VDEC_MB_DIMENSION - 1)) / VDEC_MB_DIMENSION;

            ui32NumMbProcessed = (psMsvdxIOState->ui32BeMbY * ui32WidthInMb) + psMsvdxIOState->ui32BeMbX;
            ui32PictLoadDone = hwctrl_CalcMbLoad(ui32NumMbProcessed, psDecPict->psPictHdrInfo->bIntraCoded);
        }
        else
        {
            IMG_ASSERT(psHwCtx->ui32Load == 0);
        }

        *pui32Load = psHwCtx->ui32Load - ui32PictLoadDone;
    }

error:

#endif
    return ui32Result;
}




/*!
******************************************************************************

 @Function              HWCTRL_CoreFlushMmuCache

******************************************************************************/
IMG_RESULT
HWCTRL_CoreFlushMmuCache(
    IMG_HANDLE  hHwCtx
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;
    IMG_UINT32      ui32Result;

    if (psHwCtx->bInitialised)
    {
        if (psHwCtx->sDevConfig.eDecodeLevel > VDECDD_DECODELEVEL_DECODER && psHwCtx->bPower)
        {
            ui32Result = MSVDX_FlushMmuCache(psHwCtx->hMsvdx);
            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                goto error;
            }
        }
    }

    return IMG_SUCCESS;

error:
    return ui32Result;
}



/*!
******************************************************************************

 @Function              HWCTRL_HandleInterrupt

 @Description

 This function handles the device interrupts and makes a request for the decoder
 to later service the core interrupt.

******************************************************************************/
IMG_RESULT
HWCTRL_HandleInterrupt(
    IMG_HANDLE  hHwCtx,
    IMG_VOID *  pvUserData
)
{
#ifdef VDEC_MSVDX_HARDWARE
    HWCTRL_sHwCtx     * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;
    IMG_UINT32          ui32Result;

    IMG_ASSERT(hHwCtx);

    // We want to make sure Power has not been turned off and this interrupt needs to be ignored.
    // Also need to make sure we are configured to handle this.
    if (psHwCtx->bPower)
    {
        MSVDX_sIntStatus  * psIntStatus = &psHwCtx->sIntStatus;

        ui32Result = MSVDX_HandleInterrupts(psHwCtx->hMsvdx, psIntStatus);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);

        if (psIntStatus->ui32Pending & MSVDX_CORE_CR_MSVDX_INTERRUPT_STATUS_CR_MMU_FAULT_IRQ_MASK)
        {
            // Report the fault.
            REPORT(REPORT_MODULE_HWCTRL, REPORT_NOTICE,
                   "MMU %s fault from %s %s @ 0x%08X",
                   (psIntStatus->MMU_PF_N_RW) ? "PAGE" : "PROTECTION",
                       (psIntStatus->ui32Requestor & (1<<0)) ? "DMAC" :
                       (psIntStatus->ui32Requestor & (1<<1)) ? "VEC"  :
                       (psIntStatus->ui32Requestor & (1<<2)) ? "VDMC" :
                       (psIntStatus->ui32Requestor & (1<<3)) ? "VDEB" :
                                                               "INVALID",
                   (psIntStatus->MMU_FAULT_RNW) ? "READING" : "WRITING",
                   psIntStatus->MMU_FAULT_ADDR);

            // Do the Device Reset
            // ...
            ui32Result = CORE_DevServiceTimeExpiry(psHwCtx->pvCompInitUserData, psHwCtx->ui32CoreNum);
            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                return ui32Result;
            }
        }

        if ((psIntStatus->ui32Pending & MSVDX_CORE_CR_MSVDX_INTERRUPT_STATUS_CR_MTX_IRQ_MASK) &&
            (IMG_NULL != psIntStatus->psMsgQueue))
        {
#ifdef IMG_KERNEL_MODULE
            // This checks that we don't get any spurious interrupts.
            // When run in no-bridging because of the interrupt latency and
            // the way we process messages (we process all the messages we find
            // in the VLR buffer not only the ones that each interrupt is 'asociated')
            // we may end up here with an interrupt but no new messages and this will
            // be valid.

            IMG_ASSERT(psIntStatus->psMsgQueue->bQueued &&
                       !LST_empty(&psIntStatus->psMsgQueue->sNewMsgList));
#else
            // In no-bridging if there are no messages just continue. We must have
            // processed them
            if(psIntStatus->psMsgQueue->bQueued)
            {
#endif
                do
                {
                    /* Deal with MTX messages. */
                    hwctrl_ProcessMTXMsgs(psHwCtx, psIntStatus->psMsgQueue, pvUserData);

                    if (!psIntStatus->psMsgQueue->bEmpty)
                    {
                        REPORT(REPORT_MODULE_HWCTRL, REPORT_WARNING, "Must service message buffer again...");

                        ui32Result = MSVDX_HandleInterrupts(psHwCtx->hMsvdx, psIntStatus);
                        IMG_ASSERT(ui32Result == IMG_SUCCESS);
                    }

                }
                while (!psIntStatus->psMsgQueue->bEmpty && psIntStatus->psMsgQueue->bQueued);
#ifndef IMG_KERNEL_MODULE
            }
#endif
        }
    }
#endif

    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function              HWCTRL_PowerOff

 @Description

 This function handles when the device is going off.

******************************************************************************/
IMG_RESULT
HWCTRL_PowerOff(
    IMG_HANDLE  hHwCtx
)
{
    IMG_UINT32  ui32Result = IMG_SUCCESS;

#ifdef VDEC_MSVDX_HARDWARE
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;

    if (psHwCtx->bPower && psHwCtx->sDevConfig.eDecodeLevel > VDECDD_DECODELEVEL_DECODER)
    {
        // Turn the power flag off. Any subsequent interrupts from the hardware will be ignored.
        // We need to disable interrupts. There is a Service Time Expiry task running, which when
        // check the status of the hardware in interrupt context, that might read from hardware while
        // we have disabled the clocks. This will lockup the hardware.
        SYSOSKM_DisableInt();
        psHwCtx->bPower = IMG_FALSE;
        SYSOSKM_EnableInt();

#ifdef FW_PRINT
        psHwCtx->bPrintTaskRequestedActive = IMG_FALSE;
        if(psHwCtx->hFWPrintTimerHandle)
        {
            SYSOSKM_DestroyTimer( psHwCtx->hFWPrintTimerHandle );
            psHwCtx->hFWPrintTimerHandle = IMG_NULL;
        }
#endif //FW_PRINT

        // Destroy the service timer expiry off.
        if(psHwCtx->hTimerHandle)
        {
            SYSOSKM_DestroyTimer(psHwCtx->hTimerHandle);
            psHwCtx->hTimerHandle = IMG_NULL;
        }

#ifndef IMG_KERNEL_MODULE
        // Sync all pdump scripts before deinitializing. This makes sure
        // that teardown will not happen while the backend context is waiting
        // for pictures to come up
        ui32Result = MSVDX_PDUMPSync(psHwCtx->hMsvdx);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            goto error;
        }
#endif

        ui32Result = MSVDX_DeInitialise(psHwCtx->hMsvdx);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            goto error;
        }

        DEBUG_REPORT(REPORT_MODULE_HWCTRL,
            "HWCTRL has performed Power off operations on Core %d",
            psHwCtx->ui32CoreNum);
    }

error:

#endif // VDEC_MSVDX_HARDWARE

    return ui32Result;
}



/*!
******************************************************************************

 @Function              hwctrl_PowerOn

 @Description

 This function handles when the device is turning on.

******************************************************************************/
static IMG_RESULT
hwctrl_PowerOn(
    IMG_HANDLE                  hHwCtx
)
{
    IMG_UINT32  ui32Result = IMG_SUCCESS;

#ifdef VDEC_MSVDX_HARDWARE
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;
    IMG_UINT32  i = 0;

    if (!psHwCtx->bPower && psHwCtx->sDevConfig.eDecodeLevel > VDECDD_DECODELEVEL_DECODER)
    {
        VDECFW_sCoreInitData    sInitConfig;

        ui32Result = MSVDX_Initialise(psHwCtx->hMsvdx, &sInitConfig);
        if (ui32Result != IMG_SUCCESS)
        {
            REPORT(REPORT_MODULE_HWCTRL, REPORT_ERR,
                    "ERROR: Failed initialising core %d!",
                    psHwCtx->ui32CoreNum);
            goto error;
        }

        /* Send init message to the firmware. */
        ui32Result = hwctrl_SendInitMessage(psHwCtx,
                                            &sInitConfig,
                                            &psHwCtx->sDevPtdBufInfo);
        if (ui32Result != IMG_SUCCESS)
        {
            REPORT(REPORT_MODULE_HWCTRL, REPORT_ERR,
                   "ERROR: Failed sending init message to firmware for core %d!",
                   psHwCtx->ui32CoreNum);
        }

        hwctrl_SetupDWR(psHwCtx, (SYSOSKM_pfnTimer)hwctrl_MonitorHwState);

#ifdef FW_PRINT
        psHwCtx->bPrintTaskRequestedActive = IMG_TRUE;
        SYSOSKM_CreateTimer( (SYSOSKM_pfnTimer)hwctrl_MTXPrintTask, psHwCtx, 1, &psHwCtx->hFWPrintTimerHandle );
#endif //FW_PRINT

        SYSOSKM_DisableInt();
        psHwCtx->bPower = IMG_TRUE;
        SYSOSKM_EnableInt();

#ifndef IMG_KERNEL_MODULE
        // Unlock pdump context that many times as the decode slots per core we have minus 1
        // (we lock after we submit a picture).
        // This will allow that many pictures in the pipeline
        for(i = 0; i < psHwCtx->sDevConfig.ui32NumSlotsPerCore - 1; i++)
        {
            ui32Result = MSVDX_PDUMPUnLock(psHwCtx->hMsvdx);
            if(ui32Result != IMG_SUCCESS)
            {
                REPORT(REPORT_MODULE_HWCTRL, REPORT_ERR,
                   "ERROR: Failed unlock pdump context");
                goto error;
            }
        }

        // Add a sync across all pdump scripts. This will prevent backend pdump
        // from trying to poll for pictures before driver submitted any.
        ui32Result = MSVDX_PDUMPSync(psHwCtx->hMsvdx);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if(ui32Result != IMG_SUCCESS)
        {
            REPORT(REPORT_MODULE_HWCTRL, REPORT_ERR,
               "ERROR: Failed unlock pdump context");
            goto error;
        }
#endif

        DEBUG_REPORT(REPORT_MODULE_HWCTRL,
            "HWCTRL has performed Power on operations on Core %d",
            psHwCtx->ui32CoreNum);
    }

error:

#endif // VDEC_MSVDX_HARDWARE

    return ui32Result;
}





/*!
******************************************************************************

 @Function              HWCTRL_PictureSubmit

******************************************************************************/
IMG_RESULT
HWCTRL_PictureSubmit(
    IMG_HANDLE          hHwCtx,
    DECODER_sDecPict *  psDecPict
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;

    if (psHwCtx->bInitialised)
    {
#ifdef VDEC_MSVDX_HARDWARE
        if (psHwCtx->sDevConfig.eDecodeLevel > VDECDD_DECODELEVEL_DECODER)
        {
            IMG_RESULT ui32Result;

            ui32Result = hwctrl_PowerOn(psHwCtx);
            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                return ui32Result;
            }

            // Add picture to core decode list
            LST_add(&psHwCtx->sPendPictList, psDecPict);

            ui32Result = hwctrl_SendDecodeMessage(psHwCtx,
                                                  psDecPict->psTransactionInfo->psDdBufInfo,
                                                  psDecPict->psStrPtdBufInfo);
            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                return ui32Result;
            }

            psHwCtx->ui32Load += hwctrl_CalculateLoad(psDecPict->psPictHdrInfo);
        }
        else
#endif // VDEC_MSVDX_HARDWARE
        {
            // Add picture to core decode list
            LST_add(&psHwCtx->sPendPictList, psDecPict);
        }

        psHwCtx->ui32NumFreeSlots--;
    }

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              HWCTRL_DiscardHeadPicList

******************************************************************************/
IMG_RESULT
HWCTRL_DiscardHeadPicList(
    IMG_HANDLE          hHwCtx
)
{
    DECODER_sDecPict    *   psDecPict;
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;
    IMG_UINT32  ui32Result;

    if(psHwCtx->bInitialised)
    {
        // Remove the head of the list
        psDecPict = LST_first(&psHwCtx->sPendPictList);
        if(psDecPict)
        {
            // Mark the picture
            psDecPict->psFirstFieldFwMsg->sPictDecoded.bDWRFired = IMG_TRUE;
            // Send async message for this discarded picture
            ui32Result = CORE_DevSwInterruptPicDiscarded(psHwCtx->pvCompInitUserData, psHwCtx->pvDecCore);
            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                return ui32Result;

            }
            DEBUG_REPORT(REPORT_MODULE_HWCTRL,
                "HWCTRL has discarded the picture at the head on Core %d",
                psHwCtx->ui32CoreNum);
        }
    }

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              HWCTRL_PostCoreReplay

******************************************************************************/
IMG_RESULT
HWCTRL_PostCoreReplay(
    IMG_HANDLE          hHwCtx
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;
    IMG_UINT32  ui32Result = IMG_SUCCESS;

    if (psHwCtx->bInitialised)
    {
#ifdef VDEC_MSVDX_HARDWARE
        ui32Result = CORE_DevReplay(psHwCtx->pvCompInitUserData, psHwCtx->ui32CoreNum);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            goto error;
        }
        DEBUG_REPORT(REPORT_MODULE_HWCTRL,
                     "HWCTRL has posted a AsynMessage for Replay for core %d",
                     psHwCtx->ui32CoreNum);
#endif
    }

#ifdef VDEC_MSVDX_HARDWARE
error:
#endif
    return ui32Result;
}

/*!
******************************************************************************

@Function              HWCTRL_CoreReplay

******************************************************************************/
IMG_RESULT
HWCTRL_CoreReplay(
    IMG_HANDLE          hHwCtx
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;
    IMG_UINT32  ui32Result = IMG_SUCCESS;

    if (psHwCtx->bInitialised)
    {
#ifdef VDEC_MSVDX_HARDWARE
        DECODER_sDecPict    *   psDecPict;
        psDecPict = LST_first(&psHwCtx->sPendPictList);

        while(psDecPict)
        {
            // Configure the core if not configured.
            ui32Result = hwctrl_PowerOn(psHwCtx);
            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                goto error;
            }

            // Send the Decode Message for the picture
            ui32Result = hwctrl_SendDecodeMessage(psHwCtx,
                psDecPict->psTransactionInfo->psDdBufInfo,
                psDecPict->psStrPtdBufInfo);

            // Send all the bitstream data
            ui32Result = HWCTRL_PictureSubmitFragment(hHwCtx,&psDecPict->sDecPictSegList);
            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                goto error;
            }

            // Send the End Bytes
            ui32Result = HWCTRL_CoreSendEndBytes(hHwCtx,
                psDecPict->psEndBytesBufInfo);
            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                goto error;
            }

            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            psDecPict = LST_next(psDecPict);
        }

        DEBUG_REPORT(REPORT_MODULE_HWCTRL,
                     "HWCTRL has performed Replay operation on Core %d",
                     psHwCtx->ui32CoreNum);
#endif
    }

#ifdef VDEC_MSVDX_HARDWARE
error:
#endif
    return ui32Result;
}

/*!
******************************************************************************

 @Function              HWCTRL_PictureSubmitBatch

******************************************************************************/
IMG_RESULT
HWCTRL_PictureSubmitBatch(
    IMG_HANDLE          hHwCtx,
    DECODER_sDecPict *  psDecPict,
    IMG_HANDLE          hResources,
    MSVDXIO_sPtdInfo  *  psPtdInfo
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;

    if (psHwCtx->bInitialised)
    {
#ifdef VDEC_MSVDX_HARDWARE
        if (psHwCtx->sDevConfig.eDecodeLevel > VDECDD_DECODELEVEL_DECODER)
        {
            IMG_RESULT ui32Result;

            ui32Result = hwctrl_PowerOn(psHwCtx);
            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                return ui32Result;
            }

            // Add picture to core decode list
            LST_add(&psHwCtx->sPendPictList, psDecPict);

            ui32Result = hwctrl_SendBatchMessage(psHwCtx,
                                                 psDecPict->psTransactionInfo->psDdBufInfo,
                                                 psDecPict->psStrPtdBufInfo,
                                                 psDecPict->psBatchMsgInfo->psDdBufInfo,
                                                 &psDecPict->sDecPictSegList,
                                                 psDecPict->psStartCodeBufInfo,
                                                 psDecPict->psEndBytesBufInfo);

            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                return ui32Result;
            }

            psHwCtx->ui32Load += hwctrl_CalculateLoad(psDecPict->psPictHdrInfo);
        }
        else
#endif
        {
            // Add picture to core decode list
            LST_add(&psHwCtx->sPendPictList, psDecPict);
        }

        psHwCtx->ui32NumFreeSlots--;
    }

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              HWCTRL_PictureSubmitFragment

******************************************************************************/
IMG_RESULT
HWCTRL_PictureSubmitFragment(
    IMG_HANDLE  hHwCtx,
    LST_T *     psDecPictSegList
)
{
#ifdef VDEC_MSVDX_HARDWARE
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;

    if (psHwCtx->bInitialised &&
        (psHwCtx->sDevConfig.eDecodeLevel > VDECDD_DECODELEVEL_DECODER))
    {
        /* Load the Base component if needed */

        /* Create a message here for bitstream dma buffer for the fragment */
        VDECDD_sDdBufMapInfo *  psDdBufMapInfo;
        IMG_UINT8 aui8Msg[V2_BUFFERMSG_SIZE];
        IMG_UINT8 * pui8Msg = &aui8Msg[0];
        DECODER_sDecPictSeg * psDecPictSeg = IMG_NULL;
        IMG_UINT32 ui32Result;

        psDecPictSeg = (DECODER_sDecPictSeg *) LST_first(psDecPictSegList);
        while(psDecPictSeg != IMG_NULL)
        {

            /* Get access to map info context...*/
            ui32Result = RMAN_GetResource(psDecPictSeg->psBitStrSeg->ui32BufMapId, VDECDD_BUFMAP_TYPE_ID, (IMG_VOID **)&psDdBufMapInfo, IMG_NULL);
            IMG_ASSERT(ui32Result == IMG_SUCCESS);
            if (ui32Result != IMG_SUCCESS)
            {
                return ui32Result;
            }
            VDEC_BZERO(&aui8Msg);
            MEMIO_WRITE_FIELD(pui8Msg,V2_BUFFERMSG_SIZE,sizeof(aui8Msg));
            MEMIO_WRITE_FIELD(pui8Msg,V2_BUFFERMSG_MID,VDECFW_MSGID_BITSTREAM_BUFFER);

            MEMIO_WRITE_FIELD(pui8Msg, V2_BUFFERMSG_DATAADDR,
                psDdBufMapInfo->sDdBufInfo.ui32DevVirt + psDecPictSeg->psBitStrSeg->ui32DataByteOffset);
            MEMIO_WRITE_FIELD(pui8Msg, V2_BUFFERMSG_DATASIZE,
                psDecPictSeg->psBitStrSeg->ui32DataSize);

            IMG_ASSERT(psDecPictSeg->psBitStrSeg->ui32DataSize <= psDdBufMapInfo->sDdBufInfo.ui32BufSize);

            UPDATE_DEVICE((&psDdBufMapInfo->sDdBufInfo), IMG_TRUE);

            /* Send the message to the firmware */
            MEMIO_WRITE_FIELD(pui8Msg, V2_BUFFERMSG_FENCE_ID, ((++(psHwCtx->sHostMsgStatus.aui8DecodeFenceID[VDECFW_MSGID_BITSTREAM_BUFFER-VDECFW_MSGID_PSR_PADDING]))&0xFF));

            DEBUG_REPORT(REPORT_MODULE_HWCTRL, "[MID=0x%02X] [DECODE] [0x%02X]",
                psHwCtx->sHostMsgStatus.aui8DecodeFenceID[VDECFW_MSGID_BITSTREAM_BUFFER-VDECFW_MSGID_PSR_PADDING], VDECFW_MSGID_BITSTREAM_BUFFER);

            MSVDX_SendFirmwareMessage(psHwCtx->hMsvdx, MTXIO_AREA_DECODE, aui8Msg);

            psDecPictSeg = LST_next(psDecPictSeg);
        }
    }
#endif

    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function              HWCTRL_KickSwInterrupt

******************************************************************************/
IMG_RESULT
HWCTRL_KickSwInterrupt(
    IMG_HANDLE           hHwCtx,
    IMG_HANDLE           hDecServiceInt
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *)     hHwCtx;

    {
        IMG_RESULT ui32Result;

        /* Generate a Dev SW Interrupt */
        ui32Result = CORE_DevSwInterrupt(psHwCtx->pvCompInitUserData, hDecServiceInt);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            //return ui32Result;
        }
    }

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              HWCTRL_CoreSendEndBytes

******************************************************************************/
IMG_RESULT
HWCTRL_CoreSendEndBytes(
    IMG_HANDLE              hHwCtx,
    MSVDXIO_sDdBufInfo    * psEndBytesBufInfo
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *)     hHwCtx;

#ifdef VDEC_MSVDX_HARDWARE
    if (psHwCtx->sDevConfig.eDecodeLevel > VDECDD_DECODELEVEL_DECODER)
    {
        if (psHwCtx->bInitialised)
        {
            IMG_UINT8 aui8Msg[V2_BUFFERMSG_SIZE];
            IMG_UINT8 * pui8Msg = &aui8Msg[0];

            VDEC_BZERO(&aui8Msg);
            MEMIO_WRITE_FIELD(pui8Msg,V2_BUFFERMSG_SIZE,sizeof(aui8Msg));
            MEMIO_WRITE_FIELD(pui8Msg,V2_BUFFERMSG_MID,VDECFW_MSGID_BITSTREAM_BUFFER);
            MEMIO_WRITE_FIELD(pui8Msg,V2_BUFFERMSG_FLAGS,1<<VDECFW_MSGFLAG_BUFFER_LAST_SHIFT);

            MEMIO_WRITE_FIELD(pui8Msg, V2_BUFFERMSG_DATAADDR, psEndBytesBufInfo->ui32DevVirt);
            MEMIO_WRITE_FIELD(pui8Msg, V2_BUFFERMSG_DATASIZE, psEndBytesBufInfo->ui32BufSize);

            /* Send the message to the firmware */
            MEMIO_WRITE_FIELD(pui8Msg, V2_BUFFERMSG_FENCE_ID, (((++psHwCtx->sHostMsgStatus.aui8DecodeFenceID[VDECFW_MSGID_BITSTREAM_BUFFER-VDECFW_MSGID_PSR_PADDING]))&0xFF));

            DEBUG_REPORT(REPORT_MODULE_HWCTRL, "[MID=0x%02X] [DECODE] [0x%02X]",
                psHwCtx->sHostMsgStatus.aui8DecodeFenceID[VDECFW_MSGID_BITSTREAM_BUFFER-VDECFW_MSGID_PSR_PADDING], VDECFW_MSGID_BITSTREAM_BUFFER);

            MSVDX_SendFirmwareMessage(psHwCtx->hMsvdx, MTXIO_AREA_DECODE, aui8Msg);

#ifndef IMG_KERNEL_MODULE
            {
                IMG_UINT32 ui32Result;

                ui32Result = MSVDX_PDUMPLock(psHwCtx->hMsvdx);
                if (ui32Result != IMG_SUCCESS)
                {
                    REPORT(REPORT_MODULE_HWCTRL, REPORT_ERR,
                           "Failed to get PDUMP Lock");
                    return ui32Result;
                }
            }
#endif

        }
    }
    else
#endif
    {
#ifdef HWCTRL_REPLACEMENT_HW_THREAD
        HWCTRL_sEventInfo   * psEventInfo;

        VDEC_MALLOC(psEventInfo);
        IMG_ASSERT(psEventInfo != IMG_NULL);
        VDEC_BZERO(psEventInfo);

        psEventInfo->psHwCtx = psHwCtx;

        // Fake the hardware processing.
        hwctrl_SendEvent(psHwCtx->psTaskInfo,
                         HWCTRL_EVENT_DECODE_PICTURE,
                         psEventInfo);
#else
        IMG_RESULT ui32Result;

        /* Generate a Dev HW Interrupt and not submit the picture */
        ui32Result = CORE_DevHwInterrupt(psHwCtx->pvCompInitUserData, psHwCtx->pvDecCore);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            //return ui32Result;
        }
    }
#endif

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              HWCTRL_GetPicPendPictList

******************************************************************************/
IMG_RESULT
HWCTRL_GetPicPendPictList(
    IMG_HANDLE           hHwCtx,
    IMG_UINT32           ui32TransactionId,
    DECODER_sDecPict **  ppsDecPict
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;
    DECODER_sDecPict    *   psDecPic;

    psDecPic = LST_first(&psHwCtx->sPendPictList);
    while(psDecPic)
    {
        if(psDecPic->ui32TransactionId == ui32TransactionId)
        {
            *ppsDecPict = psDecPic;
            break;
        }
        psDecPic = LST_next(psDecPic);
    }

    if(psDecPic == IMG_NULL)
    {
        REPORT(REPORT_MODULE_HWCTRL, REPORT_ERR,
            "Failed to find pending picture from transaction ID");
        return IMG_ERROR_INVALID_ID;
    }

    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function              HWCTRL_PeekHeadPicList

******************************************************************************/
IMG_RESULT
HWCTRL_PeekHeadPicList(
    IMG_HANDLE           hHwCtx,
    DECODER_sDecPict **  ppsDecPict
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;

    if (psHwCtx)
    {
        *ppsDecPict = LST_first(&psHwCtx->sPendPictList);
    }

    if (*ppsDecPict)
    {
        return IMG_SUCCESS;
    }
    else
    {
        return IMG_ERROR_GENERIC_FAILURE;
    }
}


/*!
******************************************************************************

 @Function              HWCTRL_RemoveHeadPicList

******************************************************************************/
IMG_RESULT
HWCTRL_RemoveHeadPicList(
    IMG_HANDLE           hHwCtx,
    DECODER_sDecPict **  ppsDecPict
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;

    if(psHwCtx)
    {
        *ppsDecPict = LST_removeHead(&psHwCtx->sPendPictList);

        if (*ppsDecPict)
        {
            // Indicate that a decode slot is now free.
            psHwCtx->ui32NumFreeSlots++;

#ifdef VDEC_MSVDX_HARDWARE
            if (psHwCtx->sDevConfig.eDecodeLevel > VDECDD_DECODELEVEL_DECODER)
            {
                psHwCtx->ui32Load -= hwctrl_CalculateLoad((*ppsDecPict)->psPictHdrInfo);
            }
#endif
        }
    }
    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              HWCTRL_RemoveFromPicList

******************************************************************************/
IMG_RESULT
HWCTRL_RemoveFromPicList(
    IMG_HANDLE          hHwCtx,
    DECODER_sDecPict *  psDecPict
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;

    if (psHwCtx)
    {
        DECODER_sDecPict *  psCurDecPict;

        // Ensure that this picture is in the list.
        psCurDecPict = LST_first(&psHwCtx->sPendPictList);
        while (psCurDecPict)
        {
            if (psCurDecPict == psDecPict)
            {
                LST_remove(&psHwCtx->sPendPictList, psDecPict);

                // Indicate that a decode slot is now free.
                psHwCtx->ui32NumFreeSlots++;

                break;
            }

            psCurDecPict = LST_next(psCurDecPict);
        }
    }

    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function              HWCTRL_Configure

******************************************************************************/
IMG_RESULT
HWCTRL_Configure(
    IMG_HANDLE                  hHwCtx,
    const MSVDXIO_sFw         * psFw,
    const MSVDXIO_sDdBufInfo    aui32RendecBufinfo[],
    const MSVDXIO_sDdBufInfo  * psDevPtdBufInfo,
    const MSVDXIO_sPtdInfo    * psPtdInfo
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;
    IMG_UINT32  ui32Result;

#ifdef VDEC_MSVDX_HARDWARE
    if (psHwCtx->sDevConfig.eDecodeLevel > VDECDD_DECODELEVEL_DECODER)
    {
        ui32Result = MSVDX_PrepareFirmware(psHwCtx->hMsvdx, psFw, psPtdInfo);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            goto error;
        }

        psHwCtx->aui32RendecBufInfo[0] = aui32RendecBufinfo[0];
        psHwCtx->aui32RendecBufInfo[1] = aui32RendecBufinfo[1];
        psHwCtx->sDevPtdBufInfo = *psDevPtdBufInfo;
        psHwCtx->sPsrModInfo = psFw->sPsrModInfo;
    }
#endif

    return IMG_SUCCESS;

error:
    return ui32Result;
}


/*!
******************************************************************************

 @Function              HWCTRL_Initialise

******************************************************************************/
IMG_RESULT
HWCTRL_Initialise(
    IMG_VOID                  * pvDecCore,
    IMG_VOID                  * pvCompInitUserData,
    IMG_UINT32                  ui32CoreNum,
    const VDECDD_sDdDevConfig * psDdDevConfig,
    MSVDX_sCoreProps          * p,
    MSVDX_sRendec             * psRendec,
    IMG_HANDLE                * phHwCtx
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) *phHwCtx;
    IMG_RESULT ui32Result;

    if (psHwCtx == IMG_NULL)
    {
        VDEC_MALLOC(psHwCtx);
        IMG_ASSERT(psHwCtx);
        if(psHwCtx == IMG_NULL)
        {
            REPORT(REPORT_MODULE_HWCTRL, REPORT_ERR,
                   "Failed to allocate memory for HWCTRL context");
            return IMG_ERROR_OUT_OF_MEMORY;
        }
        VDEC_BZERO(psHwCtx);
        *phHwCtx = psHwCtx;
    }

    if (!psHwCtx->bInitialised)
    {
        IMG_UINT32  ui32Msg;

        psHwCtx->sIntStatus.psMsgQueue = &psHwCtx->sMsgQueue;

        ui32Result = MSVDX_Create(psDdDevConfig,
                                  ui32CoreNum,
                                  p,
                                  psRendec,
                                  &psHwCtx->hMsvdx);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            goto error;
        }

        IMG_ASSERT(psHwCtx->hMsvdx);

        /* Initialise message info...*/
        LST_init(&psHwCtx->sMsgQueue.sFreeMsgList);
        LST_init(&psHwCtx->sMsgQueue.sNewMsgList);
        for (ui32Msg = 0; ui32Msg < MSVDX_MAX_LISR_HISR_MSG; ui32Msg++)
        {
            LST_add(&psHwCtx->sMsgQueue.sFreeMsgList, &psHwCtx->asHISRMsg[ui32Msg]);
        }

#ifdef HWCTRL_REPLACEMENT_HW_THREAD
        /* Start HW task...*/
        VDEC_MALLOC(psHwCtx->psTaskInfo);
        IMG_ASSERT(psHwCtx->psTaskInfo != IMG_NULL);
        VDEC_BZERO(psHwCtx->psTaskInfo);
        LST_init(&psHwCtx->psTaskInfo->sMsgList);

        DQ_init(&psHwCtx->psTaskInfo->hibernateQ);

        SYSOSKM_CreateMutex(&psHwCtx->psTaskInfo->hMsgListMutex);
        SYSOSKM_CreateEventObject(&psHwCtx->psTaskInfo->hNewMsgSent);

        SYSOSKM_CreateEventObject(&hActivatedEvent);
        SYSOSKM_CreateEventObject(&hDeactivatedEvent);

        SYSOSKM_CreateTimer(hwctrl_Task, psHwCtx->psTaskInfo, 0, &psHwCtx->psTaskInfo->hTask);

        SYSOSKM_WaitEventObject(hActivatedEvent);
#endif /* #ifdef HWCTRL_REPLACEMENT_HW_THREAD */

        LST_init(&psHwCtx->sPendPictList);

        psHwCtx->sDevConfig         = *psDdDevConfig;
        psHwCtx->ui32NumFreeSlots   = psDdDevConfig->ui32NumSlotsPerCore;
        psHwCtx->ui32CoreNum        = ui32CoreNum;
        psHwCtx->pvCompInitUserData = pvCompInitUserData;
        psHwCtx->pvDecCore          = pvDecCore;
        psHwCtx->bInitialised       = IMG_TRUE;
        psHwCtx->hTimerHandle       = IMG_NULL;
    }

    return IMG_SUCCESS;

error:
    if (psHwCtx->bInitialised)
    {
        if (IMG_SUCCESS != HWCTRL_Deinitialise(*phHwCtx))
        {
            REPORT(REPORT_MODULE_HWCTRL, REPORT_ERR,
                   "HWCTRL_Deinitialise() failed to tidy-up after error");
        }
    }
    else
    {
        IMG_FREE(*phHwCtx);
    }

    return ui32Result;
}


/*!
******************************************************************************

 @Function              HWCTRL_Deinitialise

******************************************************************************/
IMG_RESULT
HWCTRL_Deinitialise(
    IMG_HANDLE  hHwCtx
)
{
    HWCTRL_sHwCtx * psHwCtx = (HWCTRL_sHwCtx *) hHwCtx;
    IMG_UINT32      ui32Result;

    if (psHwCtx->bInitialised)
    {
#ifdef FW_PRINT
        psHwCtx->bPrintTaskRequestedActive = IMG_FALSE;
        if(psHwCtx->hFWPrintTimerHandle)
        {
            SYSOSKM_DestroyTimer( psHwCtx->hFWPrintTimerHandle );
            psHwCtx->hFWPrintTimerHandle = IMG_NULL;
        }
#endif //FW_PRINT
        if(psHwCtx->hTimerHandle)
        {
            SYSOSKM_DestroyTimer( psHwCtx->hTimerHandle );
            psHwCtx->hTimerHandle = IMG_NULL;
        }

        ui32Result = MSVDX_Destroy(psHwCtx->hMsvdx);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            return ui32Result;
        }

#ifdef HWCTRL_REPLACEMENT_HW_THREAD
        /* Stop the task...*/
        psHwCtx->psTaskInfo->bExit = IMG_TRUE;
        SYSOSKM_SignalEventObject(psHwCtx->psTaskInfo->hNewMsgSent);

        SYSOSKM_WaitEventObject(hDeactivatedEvent);

        SYSOSKM_DestroyTimer(psHwCtx->psTaskInfo->hTask);

        SYSOSKM_DestroyEventObject(hActivatedEvent);
        SYSOSKM_DestroyEventObject(hDeactivatedEvent);

        SYSOSKM_DestroyEventObject(psHwCtx->psTaskInfo->hNewMsgSent);
        SYSOSKM_DestroyMutex(psHwCtx->psTaskInfo->hMsgListMutex);

        if (psHwCtx->psTaskInfo)
        {
            IMG_FREE(psHwCtx->psTaskInfo);
            psHwCtx->psTaskInfo = IMG_NULL;
        }
#endif

        IMG_FREE(psHwCtx);
        psHwCtx = IMG_NULL;
    }

    return IMG_SUCCESS;
}


