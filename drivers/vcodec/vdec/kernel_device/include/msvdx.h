/*!
 *****************************************************************************
 *
 * @file       msvdx.h
 *
 * This file contains definition of MSVDX_sCoreProps structure.
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

#if !defined (__MSVDX_H__)
#define __MSVDX_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include <img_types.h>
#include <vdecdd_mmu_defs.h>

/*!
******************************************************************************
 This structure contains the core properties (inherent and internal).
 @brief  MSVDX Core Properties
******************************************************************************/
typedef struct MSVDX_sCoreProps_tag
{
    IMG_CHAR            acVersion[64];                  /*!< Deliberately a string since this should not be used outside
                                                             MSVDXIO to determine features.                                 */
    /* Video Standards. */
    IMG_BOOL            bMpeg2;                         /*!< MPEG2 supported                                                */
    IMG_BOOL            bMpeg4;                         /*!< MPEG4 supported                                                */
    IMG_BOOL            bH264;                          /*!< H.264 supported                                                */
    IMG_BOOL            bVc1;                           /*!< VC-1 supported                                                 */
    IMG_BOOL            bAvs;                           /*!< AVS supported                                                  */
    IMG_BOOL            bReal;                          /*!< Real Video supported                                           */
    IMG_BOOL            bJpeg;                          /*!< JPEG supported                                                 */
    IMG_BOOL            bVp6;                           /*!< On2 VP6 supported                                              */
    IMG_BOOL            bVp8;                           /*!< On2 VP8 supported                                              */

    /* Features (explicit) */
    IMG_BOOL            bRotationSupport;               /*!< Rotation (alternate out-of-loop output) supported              */
    IMG_BOOL            bScalingSupport;                /*!< Scaling (alternate out-of-loop output) supported               */
    IMG_BOOL            bHdSupport;                     /*!< High-definition video supported                                */
    IMG_UINT32          ui32NumStreams;                 /*!< Number of shift registers for concurrent parsing of streams    */
    IMG_UINT32          ui32NumCores;                   /*!< Number of cores for concurrent decoding (master only)          */

    /* Features (version-based) */
    IMG_BOOL            bExtendedStrides;               /*!< Extended strides (any multiple of 64-byte) supported by core.  */
    IMG_BOOL            b64ByteFixedStrides;            /*!< All fixed strides are 64-byte multiples                        */
    IMG_BOOL            b4kScalingCoeffs;               /*!<                                                                */
    IMG_BOOL            bChromaUpsample;                /*!<                                                                */
    IMG_BOOL            b8bitHighColour;                /*!< 8-bit 4:2:2 and 4:4:4 encoded material supported               */
    IMG_BOOL            bScalingWithOold;               /*!<                                                                */
    IMG_BOOL            bMultiCoreSupport;              /*!<                                                                */
    IMG_UINT32          ui32SrDmaBurstSize;             /*!< Burst size (in bytes)                                          */
    IMG_UINT32          ui32ScalingPrecision;           /*!<                                                                */
    IMG_UINT32          ui32CmdBufferSize;              /*!< Size (in words) of back-end command FIFO                       */
    IMG_UINT32          ui32MaxBitDepthLuma;            /*!<                                                                */
    IMG_UINT32          ui32MaxBitDepthChroma;          /*!<                                                                */
    IMG_BOOL            bAutoClockGatingSupport;        /*!< Auto clock-gating supported                                    */
    IMG_BOOL            bExtClockGating;                /*!< Extended clock-gating support                                  */
    IMG_BOOL            bNewCacheSettings;              /*!<                                                                */
    IMG_BOOL            bAuxLineBufSupported;           /*!<                                                                */
    IMG_BOOL            bLossless;                      /*!<                                                                */
    IMG_BOOL            bErrorHandling;                 /*!<                                                                */
    IMG_BOOL            bErrorConcealment;              /*!<                                                                */
    IMG_BOOL            bImprovedErrorConcealment;      /*!<                                                                */
    IMG_BOOL            bVdmcBurst4;                    /*!<                                                                */
    IMG_UINT32          ui32VdmcCacheSize;              /*!< Multiplier from original/default (e.g. x1, x2, x4).            */
    IMG_BOOL            ui32Latency;                    /*!<                                                                */
    IMG_BOOL            bNewTestRegSpace;               /*!< Indicates that test registers are located at REG_MSVDX_TEST_2
                                                             instead of REG_MSVDX_TEST.                                     */
    IMG_UINT32          ui32MinWidth;                   /*!< Minimum coded frame width (in pixels).                         */
    IMG_UINT32          ui32MinHeight;                  /*!< Minimum coded frame height (in pixels).                        */
    IMG_UINT32          aui32MaxWidth[VDEC_STD_MAX];    /*!< Maximum coded frame width (in pixels) for each standard.       */
    IMG_UINT32          aui32MaxHeight[VDEC_STD_MAX];   /*!< Maximum coded frame height (in pixels) for each standard.      */
    IMG_UINT32          ui32MaxMbs;                     /*!< Maximum number of MBs in coded frame (H.264 only).             */

    /* Features (derived) */
    MMU_eMmuType        eMmuType;                       /*!< MMU Type                                                       */

    /* BRN workarounds. */
    IMG_BOOL            BRN26832;                       /*!< BRN26832 workaround required                                   */
    IMG_BOOL            BRN28888;                       /*!< BRN28888 workaround required                                   */
    IMG_BOOL            BRN29688;                       /*!< BRN29688 workaround required                                   */
    IMG_BOOL            BRN29797;                       /*!< BRN29797 workaround required                                   */
    IMG_BOOL            BRN29870;                       /*!< BRN29870 workaround required                                   */
    IMG_BOOL            BRN29871;                       /*!< BRN29871 workaround required                                   */
    IMG_BOOL            BRN30095;                       /*!< BRN30095 workaround required                                   */
    IMG_BOOL            BRN30178;                       /*!< BRN30178 workaround required                                   */
    IMG_BOOL            BRN30306;                       /*!< BRN30306 workaround required                                   */
    IMG_BOOL            BRN31777;                       /*!< BRN31777 workaround required                                   */
    IMG_BOOL            BRN32651;                       /*!< BRN32651 workaround required                                   */
	IMG_BOOL			BRN40493;						/*!< BRN40493 workaround required									*/

} MSVDX_sCoreProps;


#if defined(__cplusplus)
}
#endif

#endif /* __MSVDX_H__ */
