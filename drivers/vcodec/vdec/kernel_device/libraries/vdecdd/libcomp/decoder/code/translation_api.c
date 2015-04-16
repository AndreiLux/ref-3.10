/*!
 *****************************************************************************
 *
 * @file       translation_api.c
 *
 * VDECDD Translation API.
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

#include "translation_api.h"
#include "img_errors.h"
#include "rman_api.h"

#ifndef SYSBRG_BRIDGING
    #include "talmmu_api.h"
#else
    #include "msvdx_ext.h"
    #include "sysmem_utils.h"
#endif
#ifdef SECURE_MEDIA_SUPPORT
    #include "secureapi.h"
#endif

#include "vdecfw.h"
#ifdef HAS_AVS
#include "avsfw_data.h"
#endif /* HAS_AVS */
#ifdef HAS_H264
#include "h264fw_data.h"
#endif /* HAS_H264 */
#ifdef HAS_MPEG4
#include "mpeg4fw_data.h"
#endif /* HAS_MPEG4 */
#ifdef HAS_VP6
#include "vp6fw_data.h"
#endif /* HAS_VP6 */
#ifdef HAS_VP8
#include "vp8fw_data.h"
#endif /* HAS_VP8 */
#ifdef HAS_VC1
#include "msvdx_vec_vc1_reg_io2.h"
#include "vc1fw_data.h"
#endif /* HAS_VC1 */
#ifdef HAS_JPEG
#include "msvdx_vec_jpeg_reg_io2.h"
#include "jpegfw_data.h"
#endif /* HAS_JPEG */
#ifdef HAS_MPEG2
#include "mpeg2fw_data.h"
#endif /* HAS_MPEG2 */
#ifdef HAS_REAL
#include "realfw_data.h"
#endif /* HAS_REAL */

#define NEED_TO_FIX

#define NO_VALUE    0


/*!
******************************************************************************
 This enum defines values of ENTDEC_BE_MODE field of VEC_ENTDEC_BE_CONTROL
 register and ENTDEC_FE_MODE field of VEC_ENTDEC_FE_CONTROL register.
*****************************************************************************/
typedef enum
{
    VDEC_ENTDEC_MODE_JPEG        = 0x0,    //!< JPEG
    VDEC_ENTDEC_MODE_H264        = 0x1,    //!< H264 (MPEG4/AVC)
    VDEC_ENTDEC_MODE_VC1         = 0x2,    //!< VC1
    VDEC_ENTDEC_MODE_MPEG2       = 0x3,    //!< MPEG2
    VDEC_ENTDEC_MODE_MPEG4       = 0x4,    //!< MPEG4
    VDEC_ENTDEC_MODE_AVS         = 0x5,    //!< AVS
    VDEC_ENTDEC_MODE_WMV9        = 0x6,    //!< WMV9
    VDEC_ENTDEC_MODE_MPEG1       = 0x7,    //!< MPEG1
    VDEC_ENTDEC_MODE_EXT_REAL8   = 0x0,    //!< RealVideo8, with ENTDEC_[BE|FE]_EXTENDED_MODE bit set
    VDEC_ENTDEC_MODE_EXT_REAL9   = 0x1,    //!< RealVideo9, with ENTDEC_[BE|FE]_EXTENDED_MODE bit set
    VDEC_ENTDEC_MODE_EXT_VP6     = 0x2,    //!< VP6, with ENTDEC_[BE|FE]_EXTENDED_MODE bit set
    VDEC_ENTDEC_MODE_EXT_VP8     = 0x3,    //!< VP8, with ENTDEC_[BE|FE]_EXTENDED_MODE bit set
    VDEC_ENTDEC_MODE_EXT_SVC     = 0x4,    //!< SVC, with ENTDEC_[BE|FE]_EXTENDED_MODE bit set
} VDEC_eMsvdxEntDecMode;



/*!
******************************************************************************

 @Function              translation_SetBuffer

 @Description

 This has all that it needs to translate a Stream Unit for a picture into a transaction.

 @Return            IMG_RESULT :

******************************************************************************/
static IMG_RESULT translation_SetBuffer(
    VDECDD_sDdPictBuf      * psPictBuf,
    VDECFW_sImageBuffer    * psBuffer
)
{
    IMG_UINT32 i;

    for (i = 0; i < VDEC_PLANE_MAX; i++)
    {
        psBuffer->aui32ByteOffset[i] = (IMG_UINT32)GET_HOST_ADDR(&psPictBuf->psPictBuf->sDdBufInfo, IMG_TRUE) +
                                            psPictBuf->sRendInfo.asPlaneInfo[i].ui32Offset;
    }

    return IMG_SUCCESS;
}

#ifdef HAS_AVS
/*!
******************************************************************************

 @Function              translation_AVSHeader

******************************************************************************/
static IMG_RESULT translation_AVSHeader(
    const BSPP_sSequHdrInfo  * psSeq,
    VDECDD_sPicture          * psPicture,
    DECODER_sDecPict         * psDecPict,
    AVSFW_sHeaderData        * psHeaderData
)
{
    const VDEC_sAVSSequHdrInfo * psAvsSequHdrInfo = &psSeq->sAVSSequHdrInfo;

    /* Set current sequence id */
    psHeaderData->ui16HorizontalSize = psSeq->sComSequHdrInfo.sOrigDisplayRegion.ui32Width;
    psHeaderData->ui16VerticalSize   = psSeq->sComSequHdrInfo.sOrigDisplayRegion.ui32Height;
    psHeaderData->bLowDelay          = psAvsSequHdrInfo->bLowDelay;

    psHeaderData->ui32MbParamsBaseAddress = psPicture->psPictResInt->psMBParamBuf == IMG_NULL ?
                0 : (IMG_UINT32)GET_HOST_ADDR(&psPicture->psPictResInt->psMBParamBuf->sDdBufInfo, IMG_TRUE);

    /* Set up buffer config */
    translation_SetBuffer(psDecPict->psReconPict, &psHeaderData->sPrimary);

    if (psDecPict->psAltPict)
    {
        translation_SetBuffer(psDecPict->psAltPict, &psHeaderData->sAlternate);
    }

    return IMG_SUCCESS;
}
#endif /* HAS_AVS */

#ifdef HAS_MPEG4
/*!
******************************************************************************

 @Function              translation_MPEG4Header

******************************************************************************/
static IMG_RESULT translation_MPEG4Header(
    const BSPP_sSequHdrInfo  * psSeq,
    VDECDD_sPicture          * psPicture,
    DECODER_sDecPict         * psDecPict,
    MPEG4FW_sHeaderData      * psHeaderData,
    VDEC_sStrConfigData      * psStrConfigData
)
{
    IMG_UINT32 ui32MaxHeldPicNum, ui32Result;

    psHeaderData->ui32Profile = psSeq->sComSequHdrInfo.ui32CodecProfile;

    ui32Result = VDECDDUTILS_RefPictGetMaxNum(psStrConfigData, &psSeq->sComSequHdrInfo, &ui32MaxHeldPicNum);
    IMG_ASSERT(IMG_SUCCESS == ui32Result);
    if (IMG_SUCCESS != ui32Result)
    {
        return ui32Result;
    }
    psHeaderData->bBPictures = (ui32MaxHeldPicNum == 1) ? IMG_FALSE : IMG_TRUE;

    // Obtain the MB parameter address from the stream unit.
    psHeaderData->ui32MbParamsBaseAddress = psPicture->psPictResInt->psMBParamBuf == IMG_NULL ?
            0 : (IMG_UINT32)GET_HOST_ADDR(&psPicture->psPictResInt->psMBParamBuf->sDdBufInfo, IMG_TRUE);

    // Obtain the DP address from the stream unit.
    psHeaderData->ui32DataPartition0BaseAddress = psPicture->psPictResInt->psDP0Buf == IMG_NULL ?
            0 : (IMG_UINT32)GET_HOST_ADDR(&psPicture->psPictResInt->psDP0Buf->sDdBufInfo, IMG_TRUE);

    psHeaderData->ui32DataPartition1BaseAddress = psPicture->psPictResInt->psDP1Buf == IMG_NULL ?
            0 : (IMG_UINT32)GET_HOST_ADDR(&psPicture->psPictResInt->psDP1Buf->sDdBufInfo, IMG_TRUE);

    if (psDecPict->psPrevPictDecRes)
    {
        psHeaderData->ui32FEParserVLRBufferLoad = GET_HOST_ADDR(&psDecPict->psPrevPictDecRes->sMPEG4FEVLRBackup, IMG_TRUE);
    }
    else
    {
        // If no previous context exists, use the current context which should be all zeros.
        psHeaderData->ui32FEParserVLRBufferLoad = GET_HOST_ADDR(&psDecPict->psCurPictDecRes->sMPEG4FEVLRBackup, IMG_TRUE);
    }
//    UPDATE_DEVICE(psHeaderData->ui32FEParserVLRBufferLoad, IMG_TRUE);

    psHeaderData->ui32FEParserVLRBufferSave = GET_HOST_ADDR(&psDecPict->psCurPictDecRes->sMPEG4FEVLRBackup, IMG_TRUE);

    translation_SetBuffer(psDecPict->psReconPict, &psHeaderData->sPrimary);

    if (psDecPict->psAltPict)
    {
        translation_SetBuffer(psDecPict->psAltPict, &psHeaderData->sAlternate);
    }

    return IMG_SUCCESS;
}
#endif /* HAS_MPEG4 */


#ifdef HAS_VP6
/*!
******************************************************************************

 @Function              translation_VP6Header

******************************************************************************/
static IMG_RESULT translation_VP6Header(
    const BSPP_sSequHdrInfo  * psSeq,
    VDECDD_sPicture          * psPicture,
    DECODER_sDecPict         * psDecPict,
    VP6FW_sHeaderData        * psHeaderData,
    VDEC_sStrConfigData      * psStrConfigData
)
{
    psHeaderData->ui32FrameSize = psDecPict->psPictHdrInfo->ui32PicDataSize;

    // Obtain the First Partition Buffer address from the stream unit.
    psHeaderData->ui32FirstPartitionBaseAddress = (psDecPict->psBEBufInfo == IMG_NULL) ?
        0 : ((IMG_UINT32)((IMG_UINT32)GET_HOST_ADDR(psDecPict->psBEBufInfo, IMG_TRUE))+VP6_PIC_BUFFER_OFFSET);

    translation_SetBuffer(psDecPict->psReconPict, &psHeaderData->sPrimary);

    if (psDecPict->psAltPict)
    {
        translation_SetBuffer(psDecPict->psAltPict, &psHeaderData->sAlternate);
    }
    return IMG_SUCCESS;
}
#endif /* HAS_VP6 */

#ifdef HAS_VP8
/*!
******************************************************************************

 @Function              translation_VP8Header

******************************************************************************/
static IMG_RESULT translation_VP8Header(
    const BSPP_sSequHdrInfo  * psSeq,
    VDECDD_sPicture          * psPicture,
    DECODER_sDecPict         * psDecPict,
    VP8FW_sHeaderData        * psHeaderData,
    VDEC_sStrConfigData      * psStrConfigData
)
{
    DECODER_sDecPictSeg * psDecPictSeg;
    BSPP_sBitStrSeg * psBitStrSeg;

    //Calculate Frame Length
    psHeaderData->ui32FrameLength = psDecPict->psPictHdrInfo->ui32PicDataSize;

    psDecPictSeg = LST_last(&psDecPict->sDecPictSegList);
    psBitStrSeg = psDecPictSeg->psBitStrSeg;

    //Padding the buffer, assuming only 1 buffer in the list for VP8
    //Buffers should be less than 64Kb (1 DMA only allowed) and already
    //padded to 512bytes.
    //Multiple segments (but only if consecutive in memory) should be
    //possible but it's not supported here yet.
    if(psHeaderData->ui32FrameLength < 0x200)
    {
        psBitStrSeg->ui32DataSize = 0x200;
    }

    //Buffers point inside the BE buffers as the following
    //MB_FLAGS_BUFFER_OFFSET          (0)
    //FIRST_PARTITION_BUFFER_OFFSET   (MB_FLAGS_BUFFER_OFFSET_OFFSET+MB_FLAGS_BUFFER_SIZE)
    //CURRENT_PICTURE_BUFFER_OFFSET   (FIRST_PARTITION_BUFFER_OFFSET+FIRST_PARTITION_BUFFER_SIZE)
    //SEGMENTID_BUFFER_OFFSET         (CURRENT_PICTURE_BUFFER_OFFSET+CURRENT_PICTURE_BUFFER_SIZE)

    // Obtain the MB Flags Buffer address from the stream unit
    psHeaderData->ui32MbFlagsBaseAddress = (psDecPict->psBEBufInfo == IMG_NULL) ?
        0 : ((IMG_UINT32)((IMG_UINT32)GET_HOST_ADDR(psDecPict->psBEBufInfo, IMG_TRUE))+MB_FLAGS_BUFFER_OFFSET);

    // Obtain the First Partition Buffer address from the stream unit.
    psHeaderData->ui32FirstPartitionBaseAddress = (psDecPict->psBEBufInfo == IMG_NULL) ?
        0 : ((IMG_UINT32)((IMG_UINT32)GET_HOST_ADDR(psDecPict->psBEBufInfo, IMG_TRUE))+FIRST_PARTITION_BUFFER_OFFSET);

    // Obtain the Current Picture Buffer address from the stream unit.
    psHeaderData->ui32CurrentPictureBaseAddress = (psDecPict->psBEBufInfo == IMG_NULL) ?
        0 : ((IMG_UINT32)((IMG_UINT32)GET_HOST_ADDR(psDecPict->psBEBufInfo, IMG_TRUE))+CURRENT_PICTURE_BUFFER_OFFSET);

    // Obtain the SegmentID Buffer address from the stream unit.
    psHeaderData->ui32SegmentIDBaseAddress = (psDecPict->psBEBufInfo == IMG_NULL) ?
        0 : ((IMG_UINT32)((IMG_UINT32)GET_HOST_ADDR(psDecPict->psBEBufInfo, IMG_TRUE))+SEGMENTID_BUFFER_OFFSET);

    translation_SetBuffer(psDecPict->psReconPict, &psHeaderData->sPrimary);

    if (psDecPict->psAltPict)
    {
        translation_SetBuffer(psDecPict->psAltPict, &psHeaderData->sAlternate);
    }
    return IMG_SUCCESS;
}
#endif /* HAS_VP8 */

#ifdef HAS_VC1

#include "msvdx_vec_reg_io2.h"

/*!
******************************************************************************

 @Function              translation_VC1Header

******************************************************************************/
static IMG_RESULT translation_VC1Header(
    const BSPP_sSequHdrInfo  * psSeq,
    VDECDD_sPicture          * psPicture,
    DECODER_sDecPict         * psDecPict,
    VC1FW_sHeaderData        * psHeaderData,
    VDEC_sStrConfigData      * psStrConfigData
)
{
    IMG_UINT32 ui32MsvdxProfile = 0;
    VC1FW_sDdSequenceSPS * psDdSPS = &psHeaderData->sDdSequenceSPS;

    psHeaderData->ui16CodedWidth = psDecPict->psPictHdrInfo->sCodedFrameSize.ui32Width;
    psHeaderData->ui16CodedHeight = psDecPict->psPictHdrInfo->sCodedFrameSize.ui32Height;

    psHeaderData->ui32FrameSize = psDecPict->psPictHdrInfo->ui32PicDataSize;
    psHeaderData->bEmulationPrevention = psDecPict->psPictHdrInfo->bEmulationPrevention;

    // Derive SPS parameters
    if (psSeq->sComSequHdrInfo.ui32CodecProfile == VC1_PROFILE_SIMPLE)
    {
        ui32MsvdxProfile = 0;
    }
    else if (psSeq->sComSequHdrInfo.ui32CodecProfile == VC1_PROFILE_MAIN)
    {
        ui32MsvdxProfile = 1;
    }
    else if (psSeq->sComSequHdrInfo.ui32CodecProfile == VC1_PROFILE_ADVANCED)
    {
        ui32MsvdxProfile = 2;
    }

    psDdSPS->bAdvancedProfile = (psSeq->sComSequHdrInfo.ui32CodecProfile == VC1_PROFILE_ADVANCED);

    /* CR_VEC_ENTDEC_FE_CONTROL */
    psDdSPS->ui32RegEntdecFEControl = 0;
    REGIO_WRITE_FIELD_LITE(psDdSPS->ui32RegEntdecFEControl, MSVDX_VEC, CR_VEC_ENTDEC_FE_CONTROL, ENTDEC_FE_PROFILE, ui32MsvdxProfile);
    //REGIO_WRITE_FIELD_LITE(psDdSPS->ui32RegEntdecFEControl, MSVDX_VEC, CR_VEC_ENTDEC_FE_CONTROL, ENTDEC_FE_CONTROL_ENTDEC_BITPLANE_INTERRUPT_ENABLE, 1); //Enable Interrupt for Bitplane Decoding
    REGIO_WRITE_FIELD_LITE(psDdSPS->ui32RegEntdecFEControl, MSVDX_VEC, CR_VEC_ENTDEC_FE_CONTROL, ENTDEC_FE_MODE, VDEC_ENTDEC_MODE_VC1);

    /* CR_VEC_ENTDEC_BE_CONTROL */
    psDdSPS->ui32RegEntdecBEControl = 0;
    REGIO_WRITE_FIELD(psDdSPS->ui32RegEntdecBEControl, MSVDX_VEC, CR_VEC_ENTDEC_BE_CONTROL, ENTDEC_BE_PROFILE, ui32MsvdxProfile);
    REGIO_WRITE_FIELD(psDdSPS->ui32RegEntdecBEControl, MSVDX_VEC, CR_VEC_ENTDEC_BE_CONTROL, ENTDEC_BE_MODE, VDEC_ENTDEC_MODE_VC1);

    //Buffers point inside the BE buffers as the following
    //MB_FLAGS_BUFFER_OFFSET          (0)

    // Obtain VLC Index Table Info
    psHeaderData->ui32VlcIndexSize = (psDecPict->psVlcIdxTableBufInfo->ui32BufSize / (sizeof(IMG_UINT16)*3));

    // Obtain the MB parameter address from the stream unit.
    if (psPicture->psPictResInt && psPicture->psPictResInt->psMBParamBuf)
    {
        psHeaderData->ui32MbParamsSize = psPicture->psPictResInt->psMBParamBuf->sDdBufInfo.ui32BufSize;
        psHeaderData->ui32MbParamsBaseAddress = (IMG_UINT32)GET_HOST_ADDR(&psPicture->psPictResInt->psMBParamBuf->sDdBufInfo, IMG_TRUE);
    }
    else
    {
        psHeaderData->ui32MbParamsSize = 0;
        psHeaderData->ui32MbParamsBaseAddress = 0;
    }

    //AuxMSB is directly set in translation_SetReconPictCmds. It would be useless to pass it in the header and set it in the parser
    //because anyway it would be written in the common commands.
    // Obtain the AuxMSB buffer address from the stream unit.
    //psHeaderData->ui32AuxMSBParamsBaseAddress = psPicture->psPictResInt->psMBParamBuf == IMG_NULL ?
    //    0 : ((IMG_UINT32)(GET_HOST_ADDR(psDecPict->psBEBufInfo, IMG_TRUE))+VC1_MSB_BUFFER_OFFSET);

    //Obtain Bitplane 0 address address from the stream unit.
    psHeaderData->aui32BitPlanesBufferBaseAddress[0] = (IMG_NULL == psDecPict->psFEBufInfo) ?
        0 : ((IMG_UINT32)(GET_HOST_ADDR(psDecPict->psFEBufInfo, IMG_TRUE))+VC1_BITPLANE0_BUFFER_OFFSET);

    //Obtain Bitplane 1 address address from the stream unit.
    psHeaderData->aui32BitPlanesBufferBaseAddress[1] = (IMG_NULL == psDecPict->psFEBufInfo) ?
        0 : ((IMG_UINT32)(GET_HOST_ADDR(psDecPict->psFEBufInfo, IMG_TRUE))+VC1_BITPLANE1_BUFFER_OFFSET);

    //Obtain Bitplane 2 address address from the stream unit.
    psHeaderData->aui32BitPlanesBufferBaseAddress[2] = (IMG_NULL == psDecPict->psFEBufInfo) ?
        0 : ((IMG_UINT32)(GET_HOST_ADDR(psDecPict->psFEBufInfo, IMG_TRUE))+VC1_BITPLANE2_BUFFER_OFFSET);

    translation_SetBuffer(psDecPict->psReconPict, &psHeaderData->sPrimary);

    if (psDecPict->psAltPict)
    {
        translation_SetBuffer(psDecPict->psAltPict, &psHeaderData->sAlternate);
    }

    return IMG_SUCCESS;
}
#endif /* HAS_VC1 */

#ifdef HAS_H264
/*!
******************************************************************************

 @Function              translation_H264Header

******************************************************************************/
static IMG_RESULT translation_H264Header(
    VDECDD_sPicture          * psPicture,
    DECODER_sDecPict         * psDecPict,
    H264FW_sHeaderData       * psHeaderData,
    VDEC_sStrConfigData      * psStrConfigData
)
{
    psHeaderData->bTwoPassFlag = psDecPict->psPictHdrInfo->bDiscontinuousMbs;
    psHeaderData->bDisableMvc = psStrConfigData->bDisableMvc;

    // Obtain the MB parameter address from the stream unit.
    psHeaderData->ui32MbParamsBaseAddress = psPicture->psPictResInt->psMBParamBuf == IMG_NULL ?
            0 : (IMG_UINT32)GET_HOST_ADDR(&psPicture->psPictResInt->psMBParamBuf->sDdBufInfo, IMG_TRUE);

    UPDATE_DEVICE((&psDecPict->psCurPictDecRes->sH264SgmBuf), IMG_TRUE);

    psHeaderData->ui32SliceGroupMapBaseAddress = (IMG_UINT32)GET_HOST_ADDR(&psDecPict->psCurPictDecRes->sH264SgmBuf, IMG_TRUE);

#ifdef __FAKE_MTX_INTERFACE__
    /* Just to get Second Pass working for Fake Mtx                         */
    /* In case of fake mtx, we pass buff info structure (below) defined in  */
    /* vdecdd_int.h (VDECDD_sDdBufInfo). We need handle for memory to update*/
    /* from device and read MB data                                         */
    psHeaderData->ui32MBBufInfo = (IMG_UINT32)(&psPicture->psPictResInt->psMBParamBuf->sDdBufInfo);
#endif

    translation_SetBuffer(psDecPict->psReconPict, &psHeaderData->sPrimary);

    if (psDecPict->psAltPict)
    {
        translation_SetBuffer(psDecPict->psAltPict, &psHeaderData->sAlternate);
    }

    // Signal whether we have PPS for the second field. 
    if(psPicture->sDecPictAuxInfo.ui32SecondPPSId == BSPP_INVALID)
    {
        psHeaderData->bSecondPPS = IMG_FALSE;
    }
    else
    {
        psHeaderData->bSecondPPS = IMG_TRUE;
    }

    return IMG_SUCCESS;
}
#endif /* HAS_H264 */

#ifdef HAS_JPEG

#include "msvdx_vec_reg_io2.h"

/*!
******************************************************************************

 @Function              translation_JPEGHeader

******************************************************************************/
static IMG_RESULT translation_JPEGHeader(
    const BSPP_sSequHdrInfo              * psSeq,
    const DECODER_sDecPict               * psDecPict,
    const BSPP_sPictHdrInfo              * psPictHdrInfo,
    JPEGFW_sHeaderData                   * psHeaderData
)
{
    IMG_UINT32 ui32i;

    /* Output picture planes addresses */
    for (ui32i = 0; ui32i < psSeq->sComSequHdrInfo.sPixelInfo.ui32NoPlanes; ui32i++)
    {
        psHeaderData->aui32PlaneOffsets[ui32i] = (IMG_UINT32)GET_HOST_ADDR(&psDecPict->psReconPict->psPictBuf->sDdBufInfo, IMG_TRUE) +
            psDecPict->psReconPict->sRendInfo.asPlaneInfo[ui32i].ui32Offset;
    }

    translation_SetBuffer(psDecPict->psReconPict, &psHeaderData->sPrimary);

    return IMG_SUCCESS;
}
#endif /* HAS_JPEG */

#ifdef HAS_MPEG2

#include "msvdx_cmds_io2.h"

/*!
******************************************************************************

 @Function              translation_MPEG2Header

******************************************************************************/
static IMG_RESULT translation_MPEG2Header(
    const BSPP_sSequHdrInfo  * psSeq,
    VDECDD_sPicture          * psPicture,
    DECODER_sDecPict         * psDecPict,
    MPEG2FW_sHeaderData      * psHeaderData,
    VDEC_sStrConfigData      * psStrConfigData,
    IMG_UINT32               * pui32PictCmds
)
{
    /* Set up buffer config */
    translation_SetBuffer(psDecPict->psReconPict, &psHeaderData->sPrimary);

    if (psDecPict->psAltPict)
    {
        translation_SetBuffer(psDecPict->psAltPict, &psHeaderData->sAlternate);
    }

#if 1
    {
        IMG_UINT32 ui32CodedPicHeight;
        IMG_UINT32 ui32HeightMBs;

        /* Set up width and height in macroblock units  */
        if (psSeq->sComSequHdrInfo.bInterlacedFrames)
        {
            ui32HeightMBs = ((psSeq->sComSequHdrInfo.sMaxFrameSize.ui32Height + 31) >> 5) * 2;
        }
        else
        {
            ui32HeightMBs = (psSeq->sComSequHdrInfo.sMaxFrameSize.ui32Height + 15) >> 4;
        }

        /* Update coded picture size in array of common commands */
        ui32CodedPicHeight = ui32HeightMBs * 16 - 1;
        REGIO_WRITE_FIELD_LITE(pui32PictCmds[VDECFW_CMD_CODED_PICTURE],
                MSVDX_CMDS, CODED_PICTURE_SIZE, CODED_PICTURE_HEIGHT,
                ui32CodedPicHeight);
    }
#endif

    return IMG_SUCCESS;
}
#endif /* HAS_MPEG2 */

#ifdef HAS_REAL
/*!
******************************************************************************

 @Function              translation_RealHeader

******************************************************************************/
static IMG_RESULT translation_RealHeader(
    const BSPP_sSequHdrInfo  * psSeqHdr,
    const BSPP_sPictHdrInfo  * psPictHdr,
    VDECDD_sPicture          * psPicture,
    DECODER_sDecPict         * psDecPict,
    REALFW_sHeaderData       * psHeaderData
)
{
    IMG_MEMCPY (psHeaderData->sPicHdrInfo.aui32FragmentSize, psPictHdr->sRealExtraPictHdrInfo.aui32FragmentSize, sizeof (psHeaderData->sPicHdrInfo.aui32FragmentSize));
    psHeaderData->sPicHdrInfo.ui32NumFragments  =   psPictHdr->sRealExtraPictHdrInfo.ui32NumOfFragments;

    // Obtain the MB parameter address from the stream unit.
    psHeaderData->ui32MbParamsBaseAddress = psPicture->psPictResInt->psMBParamBuf == IMG_NULL ?
            0 : (IMG_UINT32)GET_HOST_ADDR(&psPicture->psPictResInt->psMBParamBuf->sDdBufInfo, IMG_TRUE);

    translation_SetBuffer(psDecPict->psReconPict, &psHeaderData->sPrimary);

    if (psDecPict->psAltPict)
    {
        translation_SetBuffer(psDecPict->psAltPict, &psHeaderData->sAlternate);
    }

    return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function              translation_RealFragStartSegmentsCreate

******************************************************************************/
static IMG_RESULT translation_RealFragStartSegmentsCreate(
    const BSPP_sPictHdrInfo   * psPictHdrInfo,
    LST_T                     * psBitStrSegList,
    LST_T                     * psDecPictSegList
)
{
    IMG_INT32                   i = 0;
    IMG_UINT32                  ui32CurrOffsetInBuffer = 0;
    IMG_UINT32                  ui32PrevDataRemainingInBuffer = 0;
    IMG_UINT32                  ui32CurrDataRemainingInBuffer = 0;
    IMG_UINT32                  ui32SizeOfPrefixRequired = 0;
    IMG_UINT32                  ui32AddedSegmentsCounter = 0;

    DECODER_sDecPictSeg       * apsFragDecPictSeg[REALFW_MAX_FRAGMENTS+MAX_PICS_IN_SYSTEM]; // Array of wrapper elements
    BSPP_sBitStrSeg         * apsFragBitStrSeg[REALFW_MAX_FRAGMENTS+MAX_PICS_IN_SYSTEM];  // Array of bitstream segment elements

    BSPP_sBitStrSeg         * psBitStrSeg = (BSPP_sBitStrSeg *)LST_first(psBitStrSegList);

    IMG_ASSERT(psBitStrSeg != IMG_NULL);
    if (psBitStrSeg == IMG_NULL)
    {
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    ui32PrevDataRemainingInBuffer = psBitStrSeg->ui32DataSize;  // Before counting the Fragment Indeces in actual buffer
    if (psBitStrSeg->ui32DataSize > (1 + psPictHdrInfo->sRealExtraPictHdrInfo.ui32NumOfFragments * 8))  // Fragment Indeces are fully in the buffer with extra data too
    {
        ui32CurrOffsetInBuffer = psBitStrSeg->ui32DataByteOffset + (1 + psPictHdrInfo->sRealExtraPictHdrInfo.ui32NumOfFragments * 8);   // Offset after Fragment Indeces in actual buffer
        ui32CurrDataRemainingInBuffer = psBitStrSeg->ui32DataSize - (1 + psPictHdrInfo->sRealExtraPictHdrInfo.ui32NumOfFragments * 8);  // After counting the Fragment Indeces in actual buffer
    }
    else    // Skip to next buffer because there not fragment data in this
    {
        psBitStrSeg = LST_next(psBitStrSeg);
        IMG_ASSERT(psBitStrSeg != IMG_NULL);
        if (psBitStrSeg == IMG_NULL)
        {
            return IMG_ERROR_INVALID_PARAMETERS;
        }
        ui32CurrOffsetInBuffer = psBitStrSeg->ui32DataByteOffset + ((1 + psPictHdrInfo->sRealExtraPictHdrInfo.ui32NumOfFragments * 8) - ui32PrevDataRemainingInBuffer);   // Offset after Fragment Indeces in actual buffer (the PART OF THEM)
        ui32CurrDataRemainingInBuffer = psBitStrSeg->ui32DataSize - ((1 + psPictHdrInfo->sRealExtraPictHdrInfo.ui32NumOfFragments * 8) - ui32PrevDataRemainingInBuffer);  // After counting the Fragment Indeces in actual buffer (the PART OF THEM)
    }

    // Now we have skipped the Fragment Indeces
    for (i=0; i<(IMG_INT32)psPictHdrInfo->sRealExtraPictHdrInfo.ui32NumOfFragments; i++)
    {
        // Calculate how much data we need to send from the fragment
        ui32SizeOfPrefixRequired =
            (psPictHdrInfo->sRealExtraPictHdrInfo.aui32FragmentSize[i] > REALFW_NUM_START_BYTES_COPIED) ?
            REALFW_NUM_START_BYTES_COPIED :
            psPictHdrInfo->sRealExtraPictHdrInfo.aui32FragmentSize[i];

        IMG_ASSERT(ui32AddedSegmentsCounter < REALFW_MAX_FRAGMENTS+MAX_PICS_IN_SYSTEM);
        if (ui32AddedSegmentsCounter >= REALFW_MAX_FRAGMENTS+MAX_PICS_IN_SYSTEM)
        {
            return IMG_ERROR_INVALID_PARAMETERS;
        }
        apsFragBitStrSeg[ui32AddedSegmentsCounter] = (BSPP_sBitStrSeg *)IMG_MALLOC(sizeof(BSPP_sBitStrSeg));
        IMG_ASSERT(apsFragBitStrSeg[ui32AddedSegmentsCounter] != IMG_NULL);
        if (apsFragBitStrSeg[ui32AddedSegmentsCounter] == IMG_NULL)
        {
            return IMG_ERROR_MALLOC_FAILED;
        }
        IMG_MEMCPY(apsFragBitStrSeg[ui32AddedSegmentsCounter], psBitStrSeg, sizeof(BSPP_sBitStrSeg));

        apsFragBitStrSeg[ui32AddedSegmentsCounter]->ui32DataByteOffset = ui32CurrOffsetInBuffer;
        if (ui32SizeOfPrefixRequired <= ui32CurrDataRemainingInBuffer)
        {
            apsFragBitStrSeg[ui32AddedSegmentsCounter]->ui32DataSize = ui32SizeOfPrefixRequired;
            ui32CurrOffsetInBuffer += ui32SizeOfPrefixRequired;
            ui32CurrDataRemainingInBuffer -= ui32SizeOfPrefixRequired;
        }
        else
        {
            ui32PrevDataRemainingInBuffer = ui32CurrDataRemainingInBuffer;
            apsFragBitStrSeg[ui32AddedSegmentsCounter]->ui32DataSize = ui32PrevDataRemainingInBuffer;
            // We need an extra segment
            ui32AddedSegmentsCounter++;
            psBitStrSeg = LST_next(psBitStrSeg);
            IMG_ASSERT(psBitStrSeg != IMG_NULL);
            if (psBitStrSeg == IMG_NULL)
            {
                return IMG_ERROR_INVALID_PARAMETERS;
            }

            IMG_ASSERT(ui32AddedSegmentsCounter < REALFW_MAX_FRAGMENTS+MAX_PICS_IN_SYSTEM);
            if (ui32AddedSegmentsCounter >= REALFW_MAX_FRAGMENTS+MAX_PICS_IN_SYSTEM)
            {
                return IMG_ERROR_INVALID_PARAMETERS;
            }
            apsFragBitStrSeg[ui32AddedSegmentsCounter] = (BSPP_sBitStrSeg *)IMG_MALLOC(sizeof(BSPP_sBitStrSeg));
            IMG_ASSERT(apsFragBitStrSeg[ui32AddedSegmentsCounter] != IMG_NULL);
            if (apsFragBitStrSeg[ui32AddedSegmentsCounter] == IMG_NULL)
            {
                return IMG_ERROR_MALLOC_FAILED;
            }
            IMG_MEMCPY(apsFragBitStrSeg[ui32AddedSegmentsCounter], psBitStrSeg, sizeof(BSPP_sBitStrSeg));

            apsFragBitStrSeg[ui32AddedSegmentsCounter]->ui32DataByteOffset = psBitStrSeg->ui32DataByteOffset + (ui32SizeOfPrefixRequired - ui32PrevDataRemainingInBuffer);
            apsFragBitStrSeg[ui32AddedSegmentsCounter]->ui32DataSize = ui32SizeOfPrefixRequired - ui32PrevDataRemainingInBuffer;
            ui32CurrOffsetInBuffer = psBitStrSeg->ui32DataByteOffset + (ui32SizeOfPrefixRequired - ui32PrevDataRemainingInBuffer);
            ui32CurrDataRemainingInBuffer = psBitStrSeg->ui32DataSize - (ui32SizeOfPrefixRequired - ui32PrevDataRemainingInBuffer);
        }
        ui32AddedSegmentsCounter++;

        if ((psPictHdrInfo->sRealExtraPictHdrInfo.aui32FragmentSize[i] - ui32SizeOfPrefixRequired) < ui32CurrDataRemainingInBuffer)
        {
            ui32CurrOffsetInBuffer += (psPictHdrInfo->sRealExtraPictHdrInfo.aui32FragmentSize[i] - ui32SizeOfPrefixRequired);
            ui32CurrDataRemainingInBuffer -= (psPictHdrInfo->sRealExtraPictHdrInfo.aui32FragmentSize[i] - ui32SizeOfPrefixRequired);
        }
        else
        {
            IMG_UINT32 ui32RemFragmentData = (psPictHdrInfo->sRealExtraPictHdrInfo.aui32FragmentSize[i] - ui32SizeOfPrefixRequired) - ui32CurrDataRemainingInBuffer;
            while(i < (IMG_INT32)psPictHdrInfo->sRealExtraPictHdrInfo.ui32NumOfFragments - 1) // Do not do anything else if this is the last segment
            {
                psBitStrSeg = LST_next(psBitStrSeg);
                IMG_ASSERT(psBitStrSeg != IMG_NULL);
                if (psBitStrSeg == IMG_NULL)
                {
                    return IMG_ERROR_INVALID_PARAMETERS;
                }
                if (ui32RemFragmentData < psBitStrSeg->ui32DataSize)
                {
                    break;
                }
                else
                {
                    ui32RemFragmentData -= psBitStrSeg->ui32DataSize;
                }
            }
            ui32CurrOffsetInBuffer = psBitStrSeg->ui32DataByteOffset + ui32RemFragmentData;
            ui32CurrDataRemainingInBuffer = psBitStrSeg->ui32DataSize - ui32RemFragmentData;
        }
    }

    // Now "apsFragBitStrSeg[]" contains all the segments we need to send, we need to push them in the list in the correct order:
    // 1) before the actual bitstream segment 2) with correct order (first at the head)
    // so, we push to the head starting from the end of the array with the segments
    // The number of them is "ui32AddedSegmentsCounter"
    for (i = ui32AddedSegmentsCounter - 1; i >= 0; i--)
    {
        //LST_addHead(psBitStrSegList, apsFragBitStrSeg[i]);  // This SHOULD NOT be added into the bitstream segment list, it will create problem with buffer recycling
        apsFragDecPictSeg[i] = (DECODER_sDecPictSeg *)IMG_MALLOC(sizeof(DECODER_sDecPictSeg));
        IMG_ASSERT(apsFragDecPictSeg[i] != IMG_NULL);
        if (apsFragDecPictSeg[i] == IMG_NULL)
        {
            return IMG_ERROR_MALLOC_FAILED;
        }
        apsFragDecPictSeg[i]->psBitStrSeg = apsFragBitStrSeg[i];    // connect the two lists
        LST_addHead(psDecPictSegList, apsFragDecPictSeg[i]);        // Add the wrapper element into the wrapper list
    }

    return IMG_SUCCESS;
}
#endif

/*!
******************************************************************************

 @Function          translation_GetCodec

******************************************************************************/
static IMG_RESULT translation_GetCodec(
    VDEC_eVidStd        eVidStd,
    VDECFW_eCodecType * peCodec
)
{
    VDECFW_eCodecType eCodec = VDEC_CODEC_NONE;
    IMG_RESULT ui32Result = IMG_ERROR_NOT_SUPPORTED;

    // Translate from video standard to firmware codec.
    switch (eVidStd)
    {
#ifdef HAS_MPEG2
    case VDEC_STD_MPEG2:
        eCodec = VDECFW_CODEC_MPEG2;
        ui32Result = IMG_SUCCESS;
        break;
#endif /* HAS_MPEG2 */
#ifdef HAS_MPEG4
    case VDEC_STD_MPEG4:
    case VDEC_STD_H263:
    case VDEC_STD_SORENSON:
        eCodec = VDECFW_CODEC_MPEG4;
        ui32Result = IMG_SUCCESS;
        break;
#endif /* HAS_MPEG4 */
#ifdef HAS_H264
    case VDEC_STD_H264:
        eCodec = VDECFW_CODEC_H264;
        ui32Result = IMG_SUCCESS;
        break;
#endif /* HAS_H264 */
#ifdef HAS_VC1
    case VDEC_STD_VC1:
        eCodec = VDECFW_CODEC_VC1;
        ui32Result = IMG_SUCCESS;
        break;
#endif /* HAS_VC1 */
#ifdef HAS_AVS
    case VDEC_STD_AVS:
        eCodec = VDECFW_CODEC_AVS;
        ui32Result = IMG_SUCCESS;
        break;
#endif /* HAS_AVS */
#ifdef HAS_REAL
    case VDEC_STD_REAL:
        eCodec = VDECFW_CODEC_RV;
        ui32Result = IMG_SUCCESS;
        break;
#endif /* HAS_REAL */
#ifdef HAS_JPEG
    case VDEC_STD_JPEG:
        eCodec = VDECFW_CODEC_JPEG;
        ui32Result = IMG_SUCCESS;
        break;
#endif /* HAS_JPEG */
#ifdef HAS_VP6
    case VDEC_STD_VP6:
        eCodec = VDECFW_CODEC_VP6;
        ui32Result = IMG_SUCCESS;
        break;
#endif /* HAS_VP6 */
#ifdef HAS_VP8
    case VDEC_STD_VP8:
        eCodec = VDECFW_CODEC_VP8;
        ui32Result = IMG_SUCCESS;
        break;
#endif /* HAS_VP8 */
    default:
        IMG_ASSERT(IMG_FALSE);
        ui32Result = IMG_ERROR_NOT_SUPPORTED;
        break;
    }

    *peCodec = eCodec;

    return ui32Result;
}


/*!
******************************************************************************

 @Function              TRANSLATION_PicturePrepare

******************************************************************************/
IMG_RESULT TRANSLATION_PicturePrepare(
    IMG_UINT32                  ui32StrId,
    VDEC_sStrConfigData       * psStrConfigData,
    VDECDD_sStrUnit           * psStrUnit,
    DECODER_sDecPict          * psDecPict,
    const MSVDX_sCoreProps    * psCoreProps,
    IMG_BOOL                    bFakeMTX
)
{
    VDECFW_sTransaction   * psTransaction = (VDECFW_sTransaction *)psDecPict->psTransactionInfo->psDdBufInfo->pvCpuVirt;
    VDECDD_sPicture       * psPicture;
    MSVDX_sBuffers          sBuffers;

    //IMG_BOOL               bForceExtendedAsBaseline;
    IMG_RESULT              ui32Result;
#ifndef USE_FW_RELOC_INFO_PACKING
    IMG_UINT32 copyOffset = 0;
    IMG_UINT32 copySize = 0;
#endif /* USE_FW_RELOC_INFO_PACKING */

    /* Reset transaction data. */
    VDEC_BZERO(psTransaction);

    /* Construct transaction based on new picture. */
    IMG_ASSERT(psStrUnit->eStrUnitType == VDECDD_STRUNIT_PICTURE_START);

    /* Obtain picture data. */
    psPicture = (VDECDD_sPicture*)psStrUnit->pvDdPictData;
    if (psPicture->psPictResInt && psPicture->psPictResInt->psDdPictBuf)
    {
        psDecPict->psReconPict = psPicture->psPictResInt->psDdPictBuf;
        psDecPict->psAltPict = &psPicture->sDisplayPictBuf;
    }
    else
    {
        psDecPict->psReconPict = &psPicture->sDisplayPictBuf;
    }

    /* Setup top-level parameters. */
    psTransaction->ui32StreamId = ui32StrId;
    psTransaction->ui32TransactionId = psDecPict->ui32TransactionId;

    //Propagating the Secure Stream Flag
    psTransaction->bSecureStream = psStrConfigData->bSecureStream;

    ui32Result = translation_GetCodec(psStrConfigData->eVidStd,
                                      &psTransaction->eCodec);

    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        return ui32Result;
    }
    psTransaction->eParserMode = VDECFW_SCP_ONLY;

    // Get the MMUTileConfig from the buffer config in the stream unit
    ui32Result = MSVDX_GetMmuTileConfig(psPicture->sDisplayPictBuf.sBufConfig.eTileScheme,
                                        psPicture->sDisplayPictBuf.sBufConfig.b128ByteInterleave,
                                        &psTransaction->sMmuTileConfig);
    if (ui32Result != IMG_SUCCESS)
    {
        REPORT(REPORT_MODULE_DECODER, REPORT_ERR,
                "Failed to obtain the MMU tile configuration");
        return ui32Result;
    }

    // Determine any dependencies for this picture.
    psTransaction->ui32FeSyncTransactionId = psDecPict->ui32FeSyncTransactionId;
    psTransaction->ui32BeSyncTransactionId = psDecPict->ui32BeSyncTransactionId;

    psTransaction->ui32FeWdt = psDecPict->ui32FeWdtPeriod;
    psTransaction->ui32BeWdt = psDecPict->ui32BeWdtPeriod;
	psTransaction->ui32PSRWdt = psDecPict->ui32PSRWdtPeriod;

    if (psDecPict->psPrevPictDecRes)
    {
        psTransaction->uispCtxLoadAddr = GET_HOST_ADDR(&psDecPict->psPrevPictDecRes->sFwCtxBuf, !bFakeMTX);
    }
    else
{
        // If no previous context exists, use the current context which should be all zeros.
        psTransaction->uispCtxLoadAddr = GET_HOST_ADDR(&psDecPict->psCurPictDecRes->sFwCtxBuf, !bFakeMTX);
    }

    psTransaction->uispCtxSaveAddr = GET_HOST_ADDR(&psDecPict->psCurPictDecRes->sFwCtxBuf, !bFakeMTX);
    switch(psStrConfigData->eVidStd)
    {
#ifdef HAS_AVS
        case VDEC_STD_AVS:
            psTransaction->ui32CtxSize = sizeof(AVSFW_sContextData);
            break;
#endif /* HAS_AVS */
#ifdef HAS_H264
        case VDEC_STD_H264:
            psTransaction->ui32CtxSize = sizeof(H264FW_sContextData);
            break;
#endif /* HAS_H264 */
#ifdef HAS_MPEG4
        case VDEC_STD_MPEG4:
        case VDEC_STD_H263:
        case VDEC_STD_SORENSON:
            psTransaction->ui32CtxSize = sizeof(MPEG4FW_sContextData);
            break;
#endif /* HAS_MPEG4 */
#ifdef HAS_VP6
        case VDEC_STD_VP6:
            psTransaction->ui32CtxSize = sizeof(VP6FW_sContextData);
            break;
#endif /* HAS_VP6 */
#ifdef HAS_VP8
        case VDEC_STD_VP8:
            psTransaction->ui32CtxSize = sizeof(VP8FW_sContextData);
            break;
#endif /* HAS_VP8 */
#ifdef HAS_JPEG
        case VDEC_STD_JPEG:
            psTransaction->ui32CtxSize = sizeof(JPEGFW_sContextData);
            break;
#endif /* HAS_JPEG */
#ifdef HAS_MPEG2
        case VDEC_STD_MPEG2:
            psTransaction->ui32CtxSize = sizeof(MPEG2FW_sContextData);
            break;
#endif /* HAS_MPEG2 */
#ifdef HAS_REAL
        case VDEC_STD_REAL:
            psTransaction->ui32CtxSize = sizeof(REALFW_sContextData);
            break;
#endif /* HAS_REAL */
#ifdef HAS_VC1
        case VDEC_STD_VC1:
            psTransaction->ui32CtxSize = sizeof(VC1FW_sContextData);
            break;
#endif /* HAS_VC1 */
        default:
            psTransaction->ui32CtxSize = 0;
            break;
}

    psTransaction->uispCtrlSaveAddr = GET_HOST_ADDR(&psDecPict->psPictRefRes->sFwCtrlBuf, !bFakeMTX);
    psTransaction->ui32CtrlSize = psDecPict->psPictRefRes->sFwCtrlBuf.ui32BufSize;

    IMG_ASSERT(psStrUnit);
    IMG_ASSERT(psStrUnit->psSequHdrInfo);
    IMG_ASSERT(psStrUnit->psPictHdrInfo);
    
    // Sending Sequence info only if its a First Pic of Sequence, or a Start of Closed GOP
    if (psStrUnit->psPictHdrInfo->bFirstPicOfSequence || psStrUnit->bClosedGOP)
    {

        VDECDD_sDdBufMapInfo *  psDdBufMapInfo;

        ui32Result = RMAN_GetResource(psStrUnit->psSequHdrInfo->ui32BufMapId, VDECDD_BUFMAP_TYPE_ID, (IMG_VOID **)&psDdBufMapInfo, IMG_NULL);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            return ui32Result;
        }

        UPDATE_DEVICE((&psDdBufMapInfo->sDdBufInfo), !bFakeMTX);
        //psTransaction->uipSequenceInfoSource = GET_HOST_ADDR(&psDdBufMapInfo->sDdBufInfo, !bFakeMTX);
        psTransaction->uipSequenceInfoSource = GET_HOST_ADDR_OFFSET(&psDdBufMapInfo->sDdBufInfo, !bFakeMTX, psStrUnit->psSequHdrInfo->ui32BufOffset);
    }
    else
    {
        psTransaction->uipSequenceInfoSource = 0ULL;
    }

    if (psStrUnit->psPictHdrInfo->sPictAuxData.ui32Id != BSPP_INVALID)
    {
        VDECDD_sDdBufMapInfo *  psDdBufMapInfo;

        IMG_ASSERT(psStrUnit->psPictHdrInfo->sPictAuxData.pvData);

        ui32Result = RMAN_GetResource(psStrUnit->psPictHdrInfo->sPictAuxData.ui32BufMapId, VDECDD_BUFMAP_TYPE_ID, (IMG_VOID **)&psDdBufMapInfo, IMG_NULL);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            return ui32Result;
        }

        UPDATE_DEVICE((&psDdBufMapInfo->sDdBufInfo), !bFakeMTX);
        //psTransaction->uipPPSInfoSource = GET_HOST_ADDR(&psDdBufMapInfo->sDdBufInfo, !bFakeMTX);
        psTransaction->uipPPSInfoSource = GET_HOST_ADDR_OFFSET(&psDdBufMapInfo->sDdBufInfo, !bFakeMTX, psStrUnit->psPictHdrInfo->sPictAuxData.ui32BufOffset);
    }
    else
    {
        psTransaction->uipPPSInfoSource = 0ULL;
    }

    if (psStrUnit->psPictHdrInfo->sSecondPictAuxData.ui32Id != BSPP_INVALID)
    {
        VDECDD_sDdBufMapInfo *  psDdBufMapInfo;

        IMG_ASSERT(psStrUnit->psPictHdrInfo->sSecondPictAuxData.pvData);

        ui32Result = RMAN_GetResource(psStrUnit->psPictHdrInfo->sSecondPictAuxData.ui32BufMapId, VDECDD_BUFMAP_TYPE_ID, (IMG_VOID **)&psDdBufMapInfo, IMG_NULL);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);
        if (ui32Result != IMG_SUCCESS)
        {
            return ui32Result;
        }

        UPDATE_DEVICE((&psDdBufMapInfo->sDdBufInfo), !bFakeMTX);
        //psTransaction->uipSecondPPSInfoSource = GET_HOST_ADDR(&psDdBufMapInfo->sDdBufInfo, !bFakeMTX);
        psTransaction->uipSecondPPSInfoSource = GET_HOST_ADDR_OFFSET(&psDdBufMapInfo->sDdBufInfo, !bFakeMTX, psStrUnit->psPictHdrInfo->sSecondPictAuxData.ui32BufOffset);
    }
    else
    {
        psTransaction->uipSecondPPSInfoSource = 0ULL;
    }

    switch(psStrConfigData->eVidStd)
    {
#ifdef HAS_AVS
        case VDEC_STD_AVS:
        {
            AVSFW_sHeaderData * psHeaderData =  (AVSFW_sHeaderData *)psDecPict->psHdrInfo->psDdBufInfo->pvCpuVirt;
            const BSPP_sSequHdrInfo * psSeq = psStrUnit->psSequHdrInfo;

            /* Reset header data. */
            VDEC_BZERO(psHeaderData);

            /* Prepare active parameter sets. */
            translation_AVSHeader(psSeq, psPicture, psDecPict, psHeaderData);

            /* Setup header size in the transaction. */
            psTransaction->ui32PsrHdrSize =  sizeof(AVSFW_sHeaderData);
            psTransaction->uispVlcIdxTableAddr = 0;
            psTransaction->ui32VlcIdxTableSize = 0;
            psTransaction->ui32VlcTablesSize = 0;
            break;
        }
#endif /* HAS_AVS */
#ifdef HAS_H264
        case VDEC_STD_H264:
        {
            H264FW_sHeaderData       * psHeaderData = (H264FW_sHeaderData *)psDecPict->psHdrInfo->psDdBufInfo->pvCpuVirt;
            psTransaction->eParserMode = psStrUnit->psPictHdrInfo->eParserMode;

            /* Reset header data. */
            VDEC_BZERO(psHeaderData);

            /* Prepare active parameter sets. */
            translation_H264Header(psPicture, psDecPict, psHeaderData, psStrConfigData);

            /* Setup header size in the transaction. */
            psTransaction->ui32PsrHdrSize = sizeof(H264FW_sHeaderData);
            break;
        }
#endif /* HAS_H264 */
#ifdef HAS_MPEG4
        case VDEC_STD_MPEG4:
        case VDEC_STD_H263:
        case VDEC_STD_SORENSON:
        {
            MPEG4FW_sHeaderData      * psHeaderData =  (MPEG4FW_sHeaderData *)psDecPict->psHdrInfo->psDdBufInfo->pvCpuVirt;
            const BSPP_sSequHdrInfo  * psSeq = psStrUnit->psSequHdrInfo;

            /* Reset header data. */
            VDEC_BZERO(psHeaderData);

            /* Prepare active parameter sets. */
            translation_MPEG4Header(psSeq, psPicture, psDecPict, psHeaderData, psStrConfigData);

            /* Setup header size in the transaction. */
            psTransaction->ui32PsrHdrSize =  sizeof(MPEG4FW_sHeaderData);
            break;
        }
#endif /* HAS_MPEG4 */
#ifdef HAS_VP6
    case VDEC_STD_VP6:
        {
            VP6FW_sHeaderData   * psHeaderData = (VP6FW_sHeaderData *)psDecPict->psHdrInfo->psDdBufInfo->pvCpuVirt;
            const BSPP_sSequHdrInfo   * psSeq = psStrUnit->psSequHdrInfo;


            /* Reset header data. */
            VDEC_BZERO(psHeaderData);

            /* Prepare Active parameter sets */
            translation_VP6Header(psSeq, psPicture, psDecPict, psHeaderData, psStrConfigData);

            /* Setup header data address and size in the transaction. */
            UPDATE_DEVICE(psDecPict->psHdrInfo->psDdBufInfo, !bFakeMTX);
            psTransaction->uispPsrHdrAddr = GET_HOST_ADDR(psDecPict->psHdrInfo->psDdBufInfo, !bFakeMTX);
            psTransaction->ui32PsrHdrSize = sizeof(VP6FW_sHeaderData);

        }
        break;
#endif /* HAS_VP6 */
#ifdef HAS_VP8
    case VDEC_STD_VP8:
        {
            VP8FW_sHeaderData   * psHeaderData = (VP8FW_sHeaderData *)psDecPict->psHdrInfo->psDdBufInfo->pvCpuVirt;
            const BSPP_sSequHdrInfo   * psSeq = psStrUnit->psSequHdrInfo;
            BSPP_sBitStrSeg *psBitStrSeg = (BSPP_sBitStrSeg *)LST_first(&psStrUnit->sBitStrSegList);

            /* Reset header data. */
            VDEC_BZERO(psHeaderData);

            /* Prepare Active parameter sets */
            translation_VP8Header(psSeq, psPicture, psDecPict, psHeaderData, psStrConfigData);

            while(psBitStrSeg && (psBitStrSeg->ui32BitStrSegFlag & VDECDD_BSSEG_SKIP)!=0)
            {
                psBitStrSeg = LST_next(psBitStrSeg);
            }

            IMG_ASSERT(psBitStrSeg != IMG_NULL);
            if(psBitStrSeg != IMG_NULL)
            {
                IMG_UINT32 ui32Result;
                VDECDD_sDdBufMapInfo *  psDdBufMapInfo;
                /* Get access to map info context...*/
                ui32Result = RMAN_GetResource(psBitStrSeg->ui32BufMapId, VDECDD_BUFMAP_TYPE_ID, (IMG_VOID **)&psDdBufMapInfo, IMG_NULL);
                IMG_ASSERT(ui32Result == IMG_SUCCESS);
                if (ui32Result != IMG_SUCCESS)
                {
                    return ui32Result;
                }
                psHeaderData->ui32DCTBaseAddress = ((psDdBufMapInfo->sDdBufInfo.ui32DevVirt) + psBitStrSeg->ui32DataByteOffset);
            }

            /* Setup header data address and size in the transaction. */
            UPDATE_DEVICE(psDecPict->psHdrInfo->psDdBufInfo, !bFakeMTX);
            psTransaction->uispPsrHdrAddr = GET_HOST_ADDR(psDecPict->psHdrInfo->psDdBufInfo, !bFakeMTX);
            psTransaction->ui32PsrHdrSize = sizeof(VP8FW_sHeaderData);

        }
        break;
#endif /* HAS_VP8 */
#ifdef HAS_JPEG
        case VDEC_STD_JPEG:
        {
            JPEGFW_sHeaderData            * psHeaderData = (JPEGFW_sHeaderData *)psDecPict->psHdrInfo->psDdBufInfo->pvCpuVirt;
            const BSPP_sSequHdrInfo       * psSeq = psStrUnit->psSequHdrInfo;
            const BSPP_sPictHdrInfo       * psPictHdrInfo = psStrUnit->psPictHdrInfo;

            /* Reset header data. */
            VDEC_BZERO(psHeaderData);

            /* Prepare active parameter sets. */
            translation_JPEGHeader(psSeq, psDecPict, psPictHdrInfo, psHeaderData);

            /* Setup header size in the transaction. */
            psTransaction->ui32PsrHdrSize = sizeof(JPEGFW_sHeaderData);

            break;
        }
#endif /* HAS_JPEG */
#ifdef HAS_MPEG2
        case VDEC_STD_MPEG2:
        {
            MPEG2FW_sHeaderData      * psHeaderData =  (MPEG2FW_sHeaderData *)psDecPict->psHdrInfo->psDdBufInfo->pvCpuVirt;
            const BSPP_sSequHdrInfo  * psSeq = psStrUnit->psSequHdrInfo;

            /* Reset header data. */
            VDEC_BZERO(psHeaderData);

            /* Prepare active parameter sets. */
            translation_MPEG2Header(psSeq, psPicture, psDecPict, psHeaderData, psStrConfigData, psTransaction->aui32PictCmds);

            /* Setup header size in the transaction. */
            psTransaction->ui32PsrHdrSize =  sizeof(MPEG2FW_sHeaderData);
            break;
        }
#endif /* HAS_MPEG2 */
#ifdef HAS_REAL
        case VDEC_STD_REAL:
        {
            REALFW_sHeaderData        * psHeaderData = (REALFW_sHeaderData *)psDecPict->psHdrInfo->psDdBufInfo->pvCpuVirt;
            const BSPP_sSequHdrInfo   * psSeq = psStrUnit->psSequHdrInfo;
            const BSPP_sPictHdrInfo   * psPictHdrInfo = psStrUnit->psPictHdrInfo;

            /* Reset header data. */
            VDEC_BZERO(psHeaderData);

            translation_RealHeader(psSeq, psPictHdrInfo, psPicture, psDecPict, psHeaderData);

            translation_RealFragStartSegmentsCreate(psPictHdrInfo, &psStrUnit->sBitStrSegList, &psDecPict->sDecPictSegList);

            /* Setup header size in the transaction. */
            psTransaction->ui32PsrHdrSize = sizeof(REALFW_sHeaderData);
            break;
        }
#endif /* HAS_REAL*/
#ifdef HAS_VC1
        case VDEC_STD_VC1:
        {
            VC1FW_sHeaderData      * psHeaderData =  (VC1FW_sHeaderData *)psDecPict->psHdrInfo->psDdBufInfo->pvCpuVirt;
            const BSPP_sSequHdrInfo  * psSeq = psStrUnit->psSequHdrInfo;

            psTransaction->eParserMode = psStrUnit->psPictHdrInfo->eParserMode;

            /* Reset header data. */
            VDEC_BZERO(psHeaderData);

            /* Prepare active parameter sets. */
            translation_VC1Header(psSeq, psPicture, psDecPict, psHeaderData, psStrConfigData);

            /* Setup header size in the transaction. */
            psTransaction->ui32PsrHdrSize =  sizeof(VC1FW_sHeaderData);
            break;
        }
#endif /* HAS_VC1 */
        default:
            psTransaction->ui32PsrHdrSize = 0;
            break;
    }
    /* Setup header data address in the transaction. */
    UPDATE_DEVICE(psDecPict->psHdrInfo->psDdBufInfo, !bFakeMTX);
    psTransaction->uispPsrHdrAddr = GET_HOST_ADDR(psDecPict->psHdrInfo->psDdBufInfo, !bFakeMTX);

    if (psDecPict->psVlcTablesBufInfo->hMemory)
    {
        UPDATE_DEVICE(psDecPict->psVlcTablesBufInfo, IMG_TRUE);
        psTransaction->uispVlcTablesAddr = GET_HOST_ADDR(psDecPict->psVlcTablesBufInfo, IMG_TRUE);
        psTransaction->ui32VlcTablesSize = psDecPict->psVlcTablesBufInfo->ui32BufSize;
    }

    if (psDecPict->psVlcIdxTableBufInfo->hMemory)
    {
        UPDATE_DEVICE(psDecPict->psVlcIdxTableBufInfo, !bFakeMTX);
        psTransaction->uispVlcIdxTableAddr = GET_HOST_ADDR(psDecPict->psVlcIdxTableBufInfo, !bFakeMTX);
        psTransaction->ui32VlcIdxTableSize = psDecPict->psVlcIdxTableBufInfo->ui32BufSize;
    }


    /* We need to make sure there is minimum translation between Hdr Data populated */
    /* by the bspp and Picture Hdr data needed by the firmware. */

    /* Prepare BE Commands */

    sBuffers.psReconPict = psDecPict->psReconPict;
    sBuffers.psAltPict = psDecPict->psAltPict;
    sBuffers.psIntraBufInfo = psDecPict->psIntraBufInfo;
    sBuffers.psAuxLineBufInfo = psDecPict->psAuxLineBufInfo;
    sBuffers.psBEBufInfo = psDecPict->psBEBufInfo;

    /* Reconstructed Picture Configuration */
    ui32Result = MSVDX_SetReconPictCmds(psStrUnit,
                                        psStrConfigData,
                                        &psPicture->sOutputConfig,
                                        psCoreProps,
                                        &sBuffers,
                                        psTransaction->aui32PictCmds);

    /* Alternative Picture Configuration */
    if (psDecPict->psAltPict)
    {
        psDecPict->bTwoPass = psPicture->sOutputConfig.bForceOold;

#ifdef HAS_H264
        if (psStrConfigData->eVidStd == VDEC_STD_H264)
        {
            psDecPict->bTwoPass = (psDecPict->bTwoPass || psStrUnit->psPictHdrInfo->bDiscontinuousMbs);
        }
#endif /* HAS_H264 */

        sBuffers.bTwoPass = psDecPict->bTwoPass;

        /* Alternative Picture Configuration */
        /* Configure second buffer for out-of-loop processing (e.g. scaling etc.). */
        ui32Result = MSVDX_SetAltPictCmds(psStrUnit,
                                          psStrConfigData,
                                          &psPicture->sOutputConfig,
                                          psCoreProps,
                                          &sBuffers,
                                          psTransaction->aui32PictCmds);
    }


    /* Prepare Parser configuration parameters */

    return IMG_SUCCESS;
}



