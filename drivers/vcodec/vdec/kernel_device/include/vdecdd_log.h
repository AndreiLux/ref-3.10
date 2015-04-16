/*!
 *****************************************************************************
 *
 * @file       vdecdd_log.h
 *
 * This file contains the log file for the VDEC Device log events.
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

/* NOTE : THIS FILE MUST --NOT-- INCLUDE ANTI RECURSION PROTECTION - IT INCLUDES ITSELF */

#include "log_api.h"

#ifdef __cplusplus
extern "C" {
#endif

DECL_LOG_EVENT_START(VDECDD)
/*DECL_LOG_EVENT(<api>,<eventno>, <event>, HIGH|MEDIUM|LOW, <format string>) */
DECL_LOG_EVENT(VDECDD, VDECDD_REGISTER,                   HIGH, "VDECKM_fnDevRegister")
DECL_LOG_EVENT(VDECDD, VDECDD_DEVINIT,                    HIGH, "vdeckm_fnDevInit")
DECL_LOG_EVENT(VDECDD, VDECDD_DEVDEINIT,                  HIGH, "vdeckm_fnDevDeinit")
DECL_LOG_EVENT(VDECDD, VDECDD_DEVCONNECT,                 HIGH, "vdeckm_fnDevConnect")
DECL_LOG_EVENT(VDECDD, VDECDD_DEVDISCONNECT,              HIGH, "vdeckm_fnDevDisconnect")
DECL_LOG_EVENT(VDECDD, VDECDD_DEVHISR,                    HIGH, "vdeckm_fnDevKmHisr")
DECL_LOG_EVENT(VDECDD, VDECDD_DEVLISR,                    HIGH, "vdeckm_fnDevKmLisr")
DECL_LOG_EVENT(VDECDD, VDECDD_DEVPOWERPRES5,              HIGH, "vdeckm_fnDevPowerPreS5")
DECL_LOG_EVENT(VDECDD, VDECDD_DEVPOWERPOSTS0,             HIGH, "vdeckm_fnDevPowerPostS0")

DECL_LOG_EVENT(VDECDD, VDECDD_SUPPORTEDFEATURES,          HIGH, "VDECDD_SupportedFeatures")
DECL_LOG_EVENT(VDECDD, VDECDD_ISSUPPORTEDFEATURE,         HIGH, "VDECDD_IsSupportedFeature      eFeature: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMCREATE,               HIGH, "VDECDD_StreamCreate            ui32StrId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMDESTROY,              HIGH, "VDECDD_StreamDestroy           ui32StrId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_GETCALLBACKEVENT,           HIGH, "VDECDD_GetCallbackEvent        ui32StrId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_PREEMPTCALLBACKEVENT,       HIGH, "VDECDD_PreemptCallbackEvent    ui32StrId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMPLAY,                 HIGH, "VDECDD_StreamPlay              ui32StrId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMSTOP,                 HIGH, "VDECDD_StreamStop              ui32StrId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMGETSTOPFLAGS,         HIGH, "VDECDD_StreamGetStopFlags      ui32StrId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMGETSEQUHDRINFO,       HIGH, "VDECDD_StreamGetSequHdrInfo    ui32StrId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMMAPBUFF,              HIGH, "VDECDD_StreamMapBuf            ui32StrId: 0x%08X, ui32BufMapId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMUNMAPBUFF,            HIGH, "VDECDD_StreamUnmapBuf          ui32BufMapId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMSUBMITUNIT,           HIGH, "VDECDD_StreamSubmitUnit        ui32StrId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMSETOUTPUTCONFIG,      HIGH,	"VDECDD_StreamSetOutputConfig	ui32StrId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMFILLPICTBUFF,         HIGH, "VDECDD_StreamFillPictBuf       ui32BufMapId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMFLUSH,                HIGH, "VDECDD_StreamFlush             ui32StrId: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMRELEASEBUFS,          HIGH, "VDECDD_StreamReleaseBufs       ui32StrId: 0x%08X, eBufType: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMSETBEHAVIOURONERRORS, HIGH, "VDECDD_SetBehaviourOnErrors    ui32StrId: 0x%08X, eErrorHandling: 0x%08X")
DECL_LOG_EVENT(VDECDD, VDECDD_STREAMGETSTATUS,            HIGH, "VDECDD_StreamGetStatus         ui32StrId: 0x%08X")
DECL_LOG_EVENT_END()

#ifdef __cplusplus
}
#endif

/* Second pass control */
#ifdef LOG_SECOND_PASS
#include __FILE__
#endif
