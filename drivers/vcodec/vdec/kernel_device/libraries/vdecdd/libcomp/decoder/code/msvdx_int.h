/*!
 *****************************************************************************
 *
 * @file       msvdx_int.h
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

#if !defined (__MSVDXINT_H__)
#define __MSVDXINT_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include "img_defs.h"
#include "vdecdd_int.h"
#include "scaler_setup.h"
#include "msvdx_ext.h"

#define VDEC_NUM_RENDEC_BUFFERS 2

#define INTRA_BUF_SIZE                  (1024 * 32)
#define AUX_LINE_BUFFER_SIZE            (512 * 1024)

#define MAX_PICTURE_WIDTH               (2560)
#define MAX_PICTURE_HEIGHT              (1600)
#define MAX_MB_WIDTH                    (VDEC_ALIGN_SIZE(MAX_PICTURE_WIDTH, VDEC_MB_DIMENSION) / VDEC_MB_DIMENSION)
#define MAX_MB_HEIGHT                   (VDEC_ALIGN_SIZE(MAX_PICTURE_HEIGHT,VDEC_MB_DIMENSION) / VDEC_MB_DIMENSION)
#define MAX_MB_SIZE                     ((MAX_MB_WIDTH+4)*MAX_MB_HEIGHT)

#ifdef HAS_H264
#define H264_FE_BUFFER_SIZE             (0)
#define H264_BE_BUFFER_SIZE             (0)
#else
#define H264_FE_BUFFER_SIZE             (0)
#define H264_BE_BUFFER_SIZE             (0)
#endif /* HAS_H264 */

#ifdef HAS_MPEG4
#define MPEG4_FE_BUFFER_SIZE            (0)
#define MPEG4_BE_BUFFER_SIZE            (0)
#else
#define MPEG4_FE_BUFFER_SIZE            (0)
#define MPEG4_BE_BUFFER_SIZE            (0)
#endif /* HAS_MPEG4 */

#ifdef HAS_AVS
#define AVS_FE_BUFFER_SIZE              (0)
#define AVS_BE_BUFFER_SIZE              (0)
#else
#define AVS_FE_BUFFER_SIZE              (0)
#define AVS_BE_BUFFER_SIZE              (0)
#endif /* HAS_AVS */

#ifdef HAS_VP8
//Define the buffer size for the max image size possible
#define MB_FLAGS_BUFFER_SIZE            (VDEC_MB_DIMENSION*1024)
#define FIRST_PARTITION_BUFFER_SIZE     (MAX_MB_SIZE*64)
#define CURRENT_PICTURE_BUFFER_SIZE     (MAX_MB_SIZE*64)
#define SEGMENTID_BUFFER_SIZE           (MAX_MB_SIZE/4)

#define MB_FLAGS_BUFFER_OFFSET          (0)
#define FIRST_PARTITION_BUFFER_OFFSET   (((MB_FLAGS_BUFFER_OFFSET+MB_FLAGS_BUFFER_SIZE) + 3) & 0xFFFFFFFC)
#define CURRENT_PICTURE_BUFFER_OFFSET   (((FIRST_PARTITION_BUFFER_OFFSET+FIRST_PARTITION_BUFFER_SIZE) + 3) & 0xFFFFFFFC)
#define SEGMENTID_BUFFER_OFFSET         (((CURRENT_PICTURE_BUFFER_OFFSET+CURRENT_PICTURE_BUFFER_SIZE) + 3) & 0xFFFFFFFC)

#define VP8_FE_BUFFER_SIZE              (0)
#define VP8_BE_BUFFER_SIZE              (((SEGMENTID_BUFFER_OFFSET+SEGMENTID_BUFFER_SIZE) + 3) & 0xFFFFFFFC)
#else
#define VP8_FE_BUFFER_SIZE              (0)
#define VP8_BE_BUFFER_SIZE              (0)
#endif /* HAS_VP8 */

#ifdef HAS_VP6
//Define the buffer size for the max image size possible
#define VP6_PIC_BUFFER_SIZE             (MAX_MB_SIZE*64)
#define VP6_PIC_BUFFER_OFFSET           (0)

#define VP6_FE_BUFFER_SIZE              (0)
#define VP6_BE_BUFFER_SIZE              (((VP6_PIC_BUFFER_SIZE) + 3) & 0xFFFFFFFC)
#else
#define VP6_FE_BUFFER_SIZE              (0)
#define VP6_BE_BUFFER_SIZE              (0)
#endif /* HAS_VP6 */

#ifdef HAS_VC1
#define VC1_MSB_BUFFER_SIZE             (MAX_MB_SIZE/4)
#define VC1_MSB_BUFFER_OFFSET           (0)
#define VC1_BE_BUFFER_SIZE              (((VC1_MSB_BUFFER_SIZE) + 3) & 0xFFFFFFFC)

#define VC1_BITPLANE_BUFFER_SIZE        (((MAX_MB_WIDTH+16)* MAX_MB_HEIGHT))
#define VC1_BITPLANE0_BUFFER_OFFSET     (0)
#define VC1_BITPLANE1_BUFFER_OFFSET     (((VC1_BITPLANE0_BUFFER_OFFSET+VC1_BITPLANE_BUFFER_SIZE) + 3) & 0xFFFFFFFC)
#define VC1_BITPLANE2_BUFFER_OFFSET     (((VC1_BITPLANE1_BUFFER_OFFSET+VC1_BITPLANE_BUFFER_SIZE) + 3) & 0xFFFFFFFC)
#define VC1_FE_BUFFER_SIZE              (((VC1_BITPLANE2_BUFFER_OFFSET+VC1_BITPLANE_BUFFER_SIZE) + 3) & 0xFFFFFFFC)
#else
#define VC1_FE_BUFFER_SIZE              (0)
#define VC1_BE_BUFFER_SIZE              (0)
#endif /* HAS_VC1 */


/*!
******************************************************************************
 This macro returns the host address of device buffer.
******************************************************************************/
#define GET_HOST_ADDR(buf, device)                                           \
(                                                                            \
    (device) ? ((buf)->ui32DevVirt) : (IMG_UINTPTR)((buf)->pvCpuVirt)  \
)

#define GET_HOST_ADDR_OFFSET(buf, device, offset)                      \
(                                                                      \
    (device) ? (((buf)->ui32DevVirt)+(offset)) : (IMG_UINTPTR)((((IMG_UINT32)((buf)->pvCpuVirt))+(offset)))  \
)

#ifdef SYSBRG_BRIDGING
// Ensure memory is synchronised between host cpu and device
#define UPDATE_DEVICE(buf, device) SYSMEMU_UpdateMemory((buf)->hMemoryAlloc, CPU_TO_DEV)
#define UPDATE_HOST(buf, device)   SYSMEMU_UpdateMemory((buf)->hMemoryAlloc, DEV_TO_CPU)
#else
#include "talmmu_api.h"
// Update the device/host memory.
#define UPDATE_DEVICE(buf, device) { if(device) { TALMMU_CopyHostToDevMem((buf)->hMemory); } }
#define UPDATE_HOST(buf, device)   { if(device) { TALMMU_CopyDevMemToHost((buf)->hMemory); } }
#endif


typedef struct
{
    VDECDD_sDdPictBuf     * psReconPict;
    VDECDD_sDdPictBuf     * psAltPict;

    MSVDXIO_sDdBufInfo    * psIntraBufInfo;
    MSVDXIO_sDdBufInfo    * psAuxLineBufInfo;
    MSVDXIO_sDdBufInfo    * psFEBufInfo;
    MSVDXIO_sDdBufInfo    * psBEBufInfo;

    IMG_BOOL                bTwoPass;

} MSVDX_sBuffers;


/*!
******************************************************************************
 This structure contains rendec properties.
 @brief  MSVDX Rendec Properties
******************************************************************************/
typedef struct
{
    IMG_BOOL    bInUse;                                        /*!< Enable/Disable use of external memory.              */
    IMG_UINT32  aui32BufferSize[VDEC_NUM_RENDEC_BUFFERS];      /*!< Size of Rendec buffer 0 in multiples of 4KB.        */
    IMG_UINT32  ui32DecodeStartSize;                           /*!< Threshold in bytes before Rendec starts processing. */

    IMG_UINT8   ui8BurstSizeWrite;                             /*!< Burst size of Rendec write: 0--2.                   */
    IMG_UINT8   ui8BurstSizeRead;                              /*!< Burst size of Rendec read: 0--3.                    */

    IMG_UINT32  aui32InitialContext[6];                        /*!< Initial context for Rendec.                         */

} MSVDX_sRendec;


/*!
******************************************************************************

 @Function              MSVDXIO_fnTimer

 @Description

 This callback function is used on the expiry of a timer.

 @Input     pvParam : Parameter registered with timer.

 @Return    IMG_BOOL :

******************************************************************************/
extern IMG_BOOL
MSVDXIO_fnTimer(
    IMG_VOID * pvParam
);


/*!
******************************************************************************

 @Function              MSVDX_DeInitialise

******************************************************************************/
extern IMG_RESULT
MSVDX_DeInitialise(
    const IMG_HANDLE    hMsvdxContext
);


/*
******************************************************************************

 @Function              MSVDX_GetScalerCoeffCmds

******************************************************************************/
extern IMG_RESULT
MSVDX_GetScalerCoeffCmds(
    const SCALER_sCoeffs  * psScalerCoeffs,
    IMG_UINT32            * pui32PictCmds
);


/*
******************************************************************************

 @Function              MSVDX_GetScalerCmds

******************************************************************************/
extern IMG_RESULT
MSVDX_GetScalerCmds(
    const SCALER_sConfig    * psScalerConfig,
    const SCALER_sPitch     * psPitch,
    const SCALER_sFilter    * psFilter,
    const PIXEL_sPixelInfo  * psOutLoopPixelInfo,
    SCALER_sParams          * psParams,
    IMG_UINT32              * pui32PictCmds
);


/*!
******************************************************************************

 @Function              MSVDX_SetAltPictCmds

 @Description

 This has all that it needs to translate a Stream Unit for a picture into a transaction.

 @Input             psStrUnit : Pointer to stream unit.

 @Input             psStrConfigData : Pointer to stream configuration.

 @Input             psOutputConfig : Pointer to output configuration.

 @Input             p : Pointer to decoder core properties.

 @Input             psDecPict : Pointer to decoder picture.

 @Return            IMG_RESULT :

******************************************************************************/
extern IMG_RESULT 
MSVDX_SetAltPictCmds(
    const VDECDD_sStrUnit       * psStrUnit,
    const VDEC_sStrConfigData   * psStrConfigData,
    const VDEC_sStrOutputConfig * psOutputConfig,
    const MSVDX_sCoreProps      * p,
    const MSVDX_sBuffers        * psBuffers,
    IMG_UINT32                  * pui32PictCmds
);


/*!
******************************************************************************

 @Function              MSVDX_SetReconPictCmds

 @Description

 This has all that it needs to translate a Stream Unit for a picture into a transaction.

 @Input             psStrUnit : Pointer to stream unit.

 @Input             psStrConfigData : Pointer to stream configuration.

 @Input             psOutputConfig : Pointer to output configuration.

 @Input             psCoreProps : Pointer to decoder core properties.

 @Input             psDecPict : Pointer to decoder picture.

 @Return            IMG_RESULT : This function returns either IMG_SUCCESS or an
                                 error code.

******************************************************************************/
extern IMG_RESULT 
MSVDX_SetReconPictCmds(
    const VDECDD_sStrUnit         * psStrUnit,
    const VDEC_sStrConfigData     * psStrConfigData,
    const VDEC_sStrOutputConfig   * psOutputConfig,
    const MSVDX_sCoreProps        * p,
    const MSVDX_sBuffers          * psBuffers,
    IMG_UINT32                    * pui32PictCmds
);


/*!
******************************************************************************

 @Function              MSVDX_GetMmuTileConfig

 This function returns the MMU Tile config for the core.

 @Input     eTileScheme         : Tiling scheme.
 
 @Input     b128ByteInterleaved : IMG_TRUE if twidling.

 @Output    psMmuTileConfig	    : A pointer to strcuture for Tile Config.

 @Return   IMG_RESULT        : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
extern IMG_RESULT 
MSVDX_GetMmuTileConfig(
    VDEC_eTileScheme        eTileScheme,
    IMG_BOOL                b128ByteInterleaved,
    VDECFW_sMmuTileConfig * psMmuTileConfig
);


/*
******************************************************************************

 @Function              MSVDX_DumpRegisters

 @Description

 Dump the Core state.

 @Input     hMsvdxContext   : A handle to the MSVDX context.

******************************************************************************/
extern IMG_RESULT MSVDX_DumpRegisters(
    const IMG_HANDLE    hMsvdxContext
);


/*!
******************************************************************************

 @Function              MSVDX_GetCoreState

******************************************************************************/
extern IMG_RESULT
MSVDX_GetCoreState(
    const IMG_HANDLE    hMsvdxContext,
    MSVDXIO_sState    * psState
);


/*
******************************************************************************

 @Function              MSVDX_CoreReset

 @Description

 Reset MSVDX core.

 @Input     hMsvdx   : A handle to the MSVDX context.

******************************************************************************/
extern IMG_RESULT 
MSVDX_CoreReset(
    const IMG_HANDLE    hMsvdxContext
);


/*!
******************************************************************************

 @Function              MSVDX_FlushMmuCache

******************************************************************************/
extern IMG_RESULT
MSVDX_FlushMmuCache(
    const IMG_HANDLE    hMsvdxContext
);


/*!
******************************************************************************

 @Function              MSVDX_HandleInterrupts

******************************************************************************/
extern IMG_RESULT
MSVDX_HandleInterrupts(
    const IMG_HANDLE    hMsvdxContext,
    MSVDX_sIntStatus  * psIntStatus
);


/*!
******************************************************************************

 @Function              MSVDX_SendFirmwareMessage

******************************************************************************/
extern IMG_RESULT
MSVDX_SendFirmwareMessage(
    const IMG_HANDLE        hMsvdxContext,
    MSVDX_eCommsArea        eArea,
    const IMG_VOID        * psMsgHdr
);


/*!
******************************************************************************

 @Function              MSVDX_Initialise

******************************************************************************/
extern IMG_RESULT
MSVDX_Initialise(
    const IMG_HANDLE        hMsvdxContext,
    VDECFW_sCoreInitData  * psInitConfig
);


/*!
******************************************************************************

 @Function              MSVDX_PrepareFirmware

******************************************************************************/
extern IMG_RESULT
MSVDX_PrepareFirmware(
    const IMG_HANDLE            hMsvdxContext,
    const MSVDXIO_sFw         * psFw,
    const MSVDXIO_sPtdInfo     * psPtdInfo
);


/*!
******************************************************************************

 @Function              MSVDX_Create

******************************************************************************/
extern IMG_RESULT
MSVDX_Create(
    const VDECDD_sDdDevConfig * psDdDevConfig,
    IMG_UINT32                  ui32CoreNum,
    MSVDX_sCoreProps          * psCoreProps,
    MSVDX_sRendec             * psRendec,
    IMG_HANDLE                * phMsvdxContext
);
/*!
******************************************************************************

 @Function              MSVDX_ReadVLR

******************************************************************************/
extern IMG_RESULT MSVDX_ReadVLR(
	IMG_HANDLE m,
    IMG_UINT32 ui32Offset,
	IMG_UINT32 * pui32,
    IMG_UINT32 ui32Words
);
/*!
******************************************************************************

 @Function              MSVDX_WriteVLR

******************************************************************************/
extern IMG_RESULT MSVDX_WriteVLR(
	IMG_HANDLE m,
    IMG_UINT32 ui32Offset,
	IMG_UINT32 * pui32,
    IMG_UINT32 ui32Words
);
/*!
******************************************************************************

 @Function              MSVDX_Destroy

******************************************************************************/
IMG_RESULT
MSVDX_Destroy(
    IMG_HANDLE                  hMsvdxContext
);

#ifndef IMG_KERNEL_MODULE
/*!
******************************************************************************

 @Function              MSVDX_PDUMPSync

 This syncs pdump contexts

 @Input     hMsvdxContext       : Handle to #MSVDX_sContext

******************************************************************************/
extern IMG_UINT32 
MSVDX_PDUMPSync(
     const IMG_HANDLE  hMsvdxContext
);

/*!
******************************************************************************

 @Function              MSVDX_PDUMPLock

 This locks pdump contexts

 @Input     hMsvdxContext       : Handle to #MSVDX_sContext

******************************************************************************/
extern IMG_UINT32 
MSVDX_PDUMPLock(
    const IMG_HANDLE  hMsvdxContext
);

/*!
******************************************************************************

 @Function              MSVDX_PDUMPUnLock

 This unlocks pdump contexts

 @Input     hMsvdxContext       : Handle to #MSVDX_sContext

******************************************************************************/
extern IMG_UINT32 
MSVDX_PDUMPUnLock(
    const IMG_HANDLE  hMsvdxContext
);

#endif

#if defined(__cplusplus)
}
#endif

#endif /* __MSVDXINT_H__ */



