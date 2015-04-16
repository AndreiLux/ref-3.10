/*!
 *****************************************************************************
 *
 * @file       msvdx_ext.h
 *
 * Low-level MSVDX interface component
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

#if !defined (__MSVDX_EXT_H__)
#define __MSVDX_EXT_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include "img_include.h"
#include "vdec.h"

#include "vdecfw.h"
#include "vdecfw_bin.h"

#include "mem_io.h"
#include "reg_io2.h"
#include "msvdx_ext2.h"

/*! Maxmium number of DMA linked list elements:
 *  - max 3 elements of 64kB parts of firmware,
 *  - 1 potential element to fill gap between text and data section,
 *  - 2 elements for MTX enable,
 *  - 1 element to purge DMA state,
 *  - 1 element for storing MTX enable register values.
 *  See msvdxio_DMAMTX() */
#define VDEC_DMAC_LL_BUFS_COUNT 8

/*!
******************************************************************************
 @brief  MSVDXIO Poll function modes
******************************************************************************/
typedef enum
{
    MSVDXIO_POLL_EQUAL,
    MSVDXIO_POLL_NOT_EQUAL,
    MSVDXIO_POLL_MAX

} MSVDXIO_ePollMode;

typedef struct
{
#ifdef SYSBRG_BRIDGING
    IMG_UINT32  ui32PtdPhysAddr;  /*!< Page table directory's physical address                  */
#else /* not SYSBRG_BRIDGING */
    IMG_HANDLE  hPtdMemSpace;     /*!< Handle to page table directory's memory space.           */
    IMG_UINT32  ui32IntRegNum;    /*!< Internal register number used to store the PTD address.  */
#endif /* not SYSBRG_BRIDGING */
} MSVDXIO_sPtdInfo;


/*!
******************************************************************************
 Message between LISR and HISR
******************************************************************************/
typedef struct
{
    LST_LINK;

    IMG_UINT32  ui32MsgIndex;    /*!< Message index into aui32MsgBuffer.       */
    IMG_UINT32  ui32MsgSize;     /*!< Size of the message.                     */
    IMG_UINT32  ui32NewRdIndex;  /*!< New read index after message processed.  */

} MSVDXIO_sHISRMsg;


/*!
******************************************************************************
 This structure contains MTX software information.
 @brief  MTX Software Information
******************************************************************************/
typedef struct
{
    IMG_UINT32    ui32TextOrigin;
    IMG_UINT32    ui32TextAddr;
    IMG_UINT32 *  pui32Text;
    IMG_UINT32    ui32TextSize;
    IMG_UINT32    ui32DataOrigin;
    IMG_UINT32    ui32DataAddr;
    IMG_UINT32 *  pui32Data;
    IMG_UINT32    ui32DataSize;
    IMG_UINT32 *  pui32TextReloc;
    IMG_UINT8 *   pui8TextRelocType;
    IMG_UINT32 *  pui32TextRelocFullAddr;
    IMG_UINT32    ui32TextRelocSize;
    IMG_UINT32 *  pui32DataReloc;
    IMG_UINT32    ui32DataRelocSize;

} MSVDXIO_sMTXSwInfo;


typedef struct
{
    VDECFW_sFirmwareBinInfo  sFwBaseBinInfo;                 /*!< Firmware base text, data and relocation sections info.    */
    VDECFW_sFirmwareBinInfo  asFwBinModInfo[VDEC_STD_MAX];   /*!< Firmware module text, data and relocation sections info.  */

    MSVDXIO_sDdBufInfo       sFwBaseBufInfo;                 /*!< Firmware base text and data buffer info.                  */

    MSVDXIO_sDdBufInfo       asFwTextBufInfo[VDEC_STD_MAX];  /*!< Firmware modules text buffer info.                        */
    MSVDXIO_sDdBufInfo       asFwDataBufInfo[VDEC_STD_MAX];  /*!< Firmware modules data buffer info.                        */

    MSVDXIO_sDdBufInfo       sDmaLLBufInfo;                  /*!< DMA linked list buffer info.                              */

    MSVDXIO_sDdBufInfo       sPsrModInfo;                    /*!< Parser Module buffer info.                                */

} MSVDXIO_sFw;




typedef struct
{
    IMG_BOOL    bEmpty;        /*!< Indicates whether all the MTX messages were processed.
                                    This should only be IMG_FALSE if there were not
                                    enough messages in sFreeMsgList list.
                                    The remaining messages can be processed when more
                                    sFreeMsgList have been made available.                                    */
    IMG_BOOL    bQueued;       /*!< Indicates whether messages have been queued in
                                    the MTXIO message buffer "sNewMsgList".                                   */

    LST_T       sFreeMsgList;  /*!< Free HISR messages of type MSVDXIO_sHISRMsg.                              */
    LST_T       sNewMsgList;   /*!< New HISR messages of type MSVDXIO_sHISRMsg.                               */

    IMG_UINT32  ui32ReadIdx;   /*!< Read index into host copy message buffer.                                 */
    IMG_UINT32  ui32WriteIdx;  /*!< Write index into host copy message buffer.                                */

    IMG_UINT32  aui32MsgBuf[MSVDX_SIZE_MSG_BUFFER]; /*!< Message buffer used for messages received from MTX.  */

} MSVDX_sMsgQueue;


/*!
******************************************************************************
 This structure contains MSVDX interrupt Status information.
 @brief  MSVDX Interrupt Status information
******************************************************************************/
typedef struct
{
    IMG_UINT32         ui32Pending;

    // MMU fault.
    IMG_UINT32         ui32Requestor;
    IMG_UINT32         MMU_FAULT_ADDR;
    IMG_BOOL           MMU_FAULT_RNW;
    IMG_BOOL           MMU_PF_N_RW;

    // Firmware messages.
    MSVDX_sMsgQueue *  psMsgQueue;

} MSVDX_sIntStatus;


/*!
******************************************************************************
 This structure contains MSVDX Runtime information.
 @brief  MSVDX runtime Information
******************************************************************************/
typedef struct
{
    IMG_UINT32  ui32MTX_PC;
    IMG_UINT32  ui32MTX_PCX;
    IMG_UINT32  ui32MTX_ENABLE;
    IMG_UINT32  ui32MTX_STATUS_BITS;
    IMG_UINT32  ui32MTX_FAULT0;
    IMG_UINT32  ui32MTX_A0StP;
    IMG_UINT32  ui32MTX_A0FrP;

    IMG_UINT32  ui32DMA_SETUP[3];
    IMG_UINT32  ui32DMA_COUNT[3];
    IMG_UINT32  ui32DMA_PERIPHERAL_ADDR[3];

} MSVDX_sRuntimeStatus;


/*!
******************************************************************************
 This structure contains the HW core state.
 @brief  HWCTRL Core State
******************************************************************************/
typedef struct
{
    VDECFW_sFirmwareState  sFwState;            /*!< Firmware state.                                   */
    MSVDX_sRuntimeStatus   sRT;                 /*!< MTX Runtime Status.                               */

    IMG_UINT32             ui32DMACStatus;      /*!< DMAC CNT on channel DMA_CHANNEL_SR1 (channel 2).  */
    IMG_UINT32             ui32FeMbX;           /*!< Last processed MB X in front-end hardware.        */
    IMG_UINT32             ui32FeMbY;           /*!< Last processed MB Y in front-end hardware.        */
    IMG_UINT32             ui32BeMbX;           /*!< Last processed MB X in back-end hardware.         */
    IMG_UINT32             ui32BeMbY;           /*!< Last processed MB Y in back-end hardware.         */
    IMG_UINT32             ui32MSVDXIntStatus;  /*!< MSVDX interrupt status.                           */

} MSVDXIO_sState;


/*!
******************************************************************************
 Comms areas
******************************************************************************/
typedef enum
{
    MTXIO_AREA_CONTROL = 0,     /*!< The control comms area     */
    MTXIO_AREA_DECODE = 1,      /*!< The decode comms area      */
    MTXIO_AREA_COMPLETION = 2,  /*!< The completion comms area  */

    MTXIO_AREA_MAX,             /*!< The end marker             */

} MSVDX_eCommsArea;


#if defined(__cplusplus)
}
#endif

#endif /* __MSVDXIO_H__ */



