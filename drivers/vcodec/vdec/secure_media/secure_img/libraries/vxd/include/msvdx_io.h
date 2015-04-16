/*!
 *****************************************************************************
 *
 * @file       msvdx_io.h
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

#if !defined (__MSVDXIO_H__)
#define __MSVDXIO_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include "img_include.h"
#include "vdec.h"

#include "secure_defs.h"

#include "vdecfw.h"
#include "vdecfw_bin.h"
#include "msvdx_ext.h"
#include "msvdx_ext2.h"

#include "mem_io.h"
#include "reg_io2.h"



/*!
******************************************************************************

 @Function              MSVDXIO_ReadRegister

 @Description

 Read value from register.

 @Input     hCoreContext    : Handle to MSVDX IO context.

 @Input     eMemRegion      : Memory region of register.

 @Input     ui32Offset      : Register offset within register/memory space.

 @Return    IMG_RESULT      : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_UINT32
MSVDXIO_ReadRegister(
    const IMG_HANDLE    hCoreContext,
    MEM_REGIONS         eMemRegion,
    IMG_UINT32          ui32Offset
);


/*!
******************************************************************************

 @Function              MSVDXIO_WriteRegister

 @Description

 Write value to register.

 @Input     hCoreContext    : Handle to MSVDX IO context.

 @Input     eMemRegion      : Memory region of register.

 @Input     ui32Offset      : Register offset within register/memory space.

 @Input     ui32Value       : Value to write to register.

 @Input     ui32Mask        : Mask of bits to modify to register.

 @Return    IMG_RESULT      : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_RESULT
MSVDXIO_WriteRegister(
    const IMG_HANDLE    hCoreContext,
    MEM_REGIONS         eMemRegion,
    IMG_UINT32          ui32Offset,
    IMG_UINT32          ui32Value,
    IMG_UINT32          ui32Mask
);


/*!
******************************************************************************

 @Function              MSVDXIO_VLRWriteWords

 @Description

 This function writes an array of words to VEC local RAM (VLR).

 @Input     hCoreContext : Handle to MSVDXIO context.

 @Input     eMemRegion   : VLR memory space.

 @Input     ui32Addr     : Address (byte) in VLR.

 @Input     ui32NumWords : Number of 32-bits words to write.

 @Input     pui32Values  : Array of words to write.

 @Return    IMG_RESULT   : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_RESULT 
MSVDXIO_VLRWriteWords(
    const IMG_HANDLE    hCoreContext,
    MEM_REGIONS         eMemRegion,
    IMG_UINT32          ui32Addr,
    IMG_UINT32          ui32NumWords,
    const IMG_UINT32  * pui32Values
);


/*!
******************************************************************************

 @Function              MSVDXIO_VLRReadWords

 @Description

 This function reads an array of words from VEC local RAM (VLR).

 @Input     hCoreContext : Handle to MSVDXIO context.

 @Input     eMemRegion   : VLR memory space.

 @Input     ui32Addr     : Address (byte) in VLR.

 @Input     ui32NumWords : Number of 32-bits words to read.

 @Input     pui32Values  : Array of words to read.

 @Input     bValidate    : POL values read from VLR

 @Return    IMG_RESULT   : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_RESULT
MSVDXIO_VLRReadWords(
    const IMG_HANDLE    hCoreContext,
    MEM_REGIONS         eMemRegion,
    IMG_UINT32          ui32Addr,
    IMG_UINT32          ui32NumWords,
    IMG_UINT32        * pui32Values,
    IMG_BOOL            bValidate
);


/*!
******************************************************************************

 @Function              MSVDXIO_Poll

 @Description

 Poll for a specific register value.

 @Input     hCoreContext    : Handle to MSVDX IO context.

 @Input     eMemRegion      : Memory region of register.

 @Input     ui32Offset      : Register offset within register/memory space.

 @Input     ui32RequValue   : Required register value.

 @Input     ui32Enable      : Mask of live bits to apply to register.

 @Input     ePollMode       : Check function to be used (equals, !equals ect)

 @Return    IMG_RESULT      : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_RESULT
MSVDXIO_Poll(
    const IMG_HANDLE    hCoreContext,
    MEM_REGIONS         eMemRegion,
    IMG_UINT32          ui32Offset,
    IMG_UINT32          ui32RequValue,
    IMG_UINT32          ui32Enable,
    MSVDXIO_ePollMode   ePollMode
);

#if !defined(SECURE_MEDIA_SUPPORT) && !defined(VXD_BRIDGING)

/*!
******************************************************************************

 @Function              MSVDXIO_PDUMPVerifPoll

 @Description

 Poll only for pdump purposes (to force the pdump script to verify this value.

 @Input     hCoreContext    : Handle to video device IO context.

 @Input     ui32MemRegion   : Memory region of register.

 @Input     ui32Offset      : Register offset within register/memory space.

 @Input     ui32RequValue   : Required register value.

 @Input     ui32Enable      : Mask of live bits to apply to register.

******************************************************************************/
extern IMG_RESULT
MSVDXIO_PDUMPVerifPoll(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32MemRegion,
    IMG_UINT32          ui32Offset,
    IMG_UINT32          ui32RequValue,
    IMG_UINT32          ui32Enable
);

/*
******************************************************************************

 @Function              MSVDXIO_PDUMPSync

 @Description

 Sync pdump contexts

 @Input     hCoreContext    : Handle to MSVDX IO context.

******************************************************************************/
extern IMG_RESULT
MSVDXIO_PDUMPSync(
    const IMG_HANDLE  hCoreContext
);


/*
******************************************************************************

 @Function              MSVDXIO_PDUMPLock

 @Description

 Lock pdump contexts

 @Input     hCoreContext    : Handle to MSVDX IO context.

******************************************************************************/
extern IMG_RESULT
MSVDXIO_PDUMPLock(
    const IMG_HANDLE  hCoreContext
);


/*
******************************************************************************

 @Function              MSVDXIO_PDUMPUnLock

 @Description

 Unlock pdump contexts

 @Input     hCoreContext    : Handle to MSVDX IO context.

******************************************************************************/
extern IMG_RESULT
MSVDXIO_PDUMPUnLock(
    const IMG_HANDLE  hCoreContext
);

/*!
******************************************************************************

 @Function              MSVDXIO_PDUMPComment

******************************************************************************/
extern IMG_RESULT
MSVDXIO_PDUMPComment(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32MemRegion,
    const IMG_CHAR *    pszComment
);

/*!
******************************************************************************

 @Function              MSVDXIO_PDUMPPollCircBuff

 @Description

 Poll only for pdump purposes (to force the pdump script to verify this value.

 @Input     hCoreContext         : Handle to video device IO context.

 @Input     ui32MemRegion        : Memory region of register.

 @Input     ui32Offset           : Register offset within register/memory space.

 @Input     ui32WriteOffsetVal   : Required register value.

 @Input     ui32PacketSize       : Required register value.

 @Input     ui32BufferSize       : Required register value.

******************************************************************************/
IMG_RESULT
MSVDXIO_PDUMPPollCircBuff(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32MemRegion,
    IMG_UINT32          ui32Offset,
    IMG_UINT32          ui32WriteOffsetVal,
    IMG_UINT32          ui32PacketSize,
    IMG_UINT32          ui32BufferSize
);

#endif

/*!
******************************************************************************

 @Function              MSVDXIO_GetCoreState

 @Description

 Obtain the MSVDX core state.

 @Input     hCoreContext    : Handle to MSVDX IO context.

 @Output    psState         : Pointer to state information.

 @Return    IMG_RESULT      : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_UINT32
MSVDXIO_GetCoreState(
    const IMG_HANDLE    hCoreContext,
    MSVDXIO_sState    * psState
);


/*!
******************************************************************************

 @Function              MSVDXIO_HandleInterrupts

 @Description

 Handle any pending MSVDX interrupts and clear.

 @Input     hCoreContext    : Handle to MSVDX IO context.

 @Output    psIntStatus     : Pointer to interrupt status information.

 @Return    IMG_RESULT      : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_UINT32
MSVDXIO_HandleInterrupts(
    const IMG_HANDLE    hCoreContext,
    MSVDX_sIntStatus  * psIntStatus
);


/*!
******************************************************************************

 @Function              MSVDXIO_SendFirmwareMessage
 
 @Description

 Send a message to firmware.

 @Input     hCoreContext    : Handle to MSVDX IO context.

 @Input     eArea           : Comms area into which the message should be written.

 @Input     psMsgHdr        : Pointer to message to send.

 @Return    IMG_RESULT      : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_UINT32
MSVDXIO_SendFirmwareMessage(
    const IMG_HANDLE    hCoreContext,
    MSVDX_eCommsArea    eArea,
    const IMG_VOID    * psMsgHdr
);


/*!
******************************************************************************

 @Function              MSVDXIO_LoadBaseFirmware

 @Description

 Load the firmware base component into MTX RAM.

 @Input     hCoreContext    : Handle to MSVDX IO context.

 @Return    IMG_RESULT      : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_RESULT 
MSVDXIO_LoadBaseFirmware(
    const IMG_HANDLE        hCoreContext
);


/*!
******************************************************************************

 @Function              MSVDXIO_PrepareFirmware

 @Description

 Prepares the firmware for loading to MTX.

 @Input     hCoreContext    : Handle to MSVDX IO context.

 @Input     psFw            : Pointer to FW layout information and DMA LL buffer.

 @Return    IMG_RESULT      : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_RESULT
MSVDXIO_PrepareFirmware(
    const IMG_HANDLE            hCoreContext,
    const MSVDXIO_sFw         * psFw
);


/*!
******************************************************************************

 @Function              MSVDXIO_DisableClocks

 @Description

 Disables MSVDX clocks.

 @Input     hCoreContext    : Handle to MSVDX IO context.

 @Return    IMG_RESULT      : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_RESULT 
MSVDXIO_DisableClocks(
    const IMG_HANDLE    hCoreContext
);


/*!
******************************************************************************

 @Function              MSVDXIO_EnableClocks

 @Description

 Enables MSVDX clocks.

 @Input     hCoreContext            : Handle to MSVDX IO context.

 @Input     bAutoClockGatingSupport : Auto clock-gating is supported by the core.

 @Input     bExtClockGating         : Extended clock-gating register are present on core.

 @Input     bForceManual            : Force manual clock-gating.

 @Return    IMG_RESULT              : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_UINT32
MSVDXIO_EnableClocks(
    const IMG_HANDLE    hCoreContext,
    IMG_BOOL            bAutoClockGatingSupport,
    IMG_BOOL            bExtClockGating,
    IMG_BOOL            bForceManual
);


/*!
******************************************************************************

 @Function              MSVDXIO_Initialise

 @Description

 Initialises an MSVDXIO context.

 @Input     bFakeMtx        : Flag to indicate whether fake mtx is being used.

 @Input     ui32CoreNum     : Core number.

 @Output    phCoreContext   : A pointer to handle to the MSVDX IO context.

 @Return    IMG_RESULT      : Returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_RESULT
MSVDXIO_Initialise(
    IMG_BOOL                    bFakeMtx,
    IMG_UINT32                  ui32CoreNum,
    IMG_HANDLE                * phCoreContext
);
/*
******************************************************************************

 @Function              MSVDXIO_Initialise

******************************************************************************/
IMG_RESULT 
MSVDXIO_DeInitialise(
    IMG_UINT32          ui32CoreNum,
    IMG_HANDLE          hCoreContext
);


#if defined(__cplusplus)
}
#endif

#endif /* __MSVDXIO_H__ */



