/*!
 *****************************************************************************
 *
 * @file	   page_alloc_log.h
 *
 * This file contains the log file for the Page Allocation Component log events.
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

DECL_LOG_EVENT_START(PALLOC)
/*DECL_LOG_EVENT(<api>,<eventno>, <event>, HIGH|MEDIUM|LOW, <format string>) */
DECL_LOG_EVENT(PALLOC, PALLOC_INITIALISE,		HIGH,	"PALLOC_Initialise")
DECL_LOG_EVENT(PALLOC, PALLOC_DEINITIALISE,		HIGH,	"PALLOC_Deinitialise")
DECL_LOG_EVENT(PALLOC, PALLOC_COMPCONNECT,		HIGH,	"palloc_fnCompConnect")
DECL_LOG_EVENT(PALLOC, PALLOC_COMPDISCONNECT,	HIGH,	"palloc_fnCompDisconnect")
DECL_LOG_EVENT(PALLOC, PALLOC_COMPATTACH,		HIGH,	"palloc_fnCompAttach")
DECL_LOG_EVENT(PALLOC, PALLOC_ATTACH,			HIGH,	"PALLOC_AttachToConnection")
DECL_LOG_EVENT(PALLOC, PALLOC_FREE,				LOW,	"pallocbrg_fnFree pvParam: 0x%08X")
DECL_LOG_EVENT(PALLOC, PALLOC_ALLOC,			LOW,	"PALLOC_Alloc ui32AttachId: 0x%08X, ui32Size: 0x%08X")
DECL_LOG_EVENT(PALLOC, PALLOC_ALLOCID,			LOW,	"PALLOC_Alloc ui32AttachId: 0x%08X, *pui32AllocId: 0x%08X")
DECL_LOG_EVENT(PALLOC, PALLOC_FREEID,			LOW,	"PALLOC_Free ui32AllocId: 0x%08X")
DECL_LOG_EVENT(PALLOC, PALLOC_IMPORT,		    LOW,	"PALLOC_Import ui32AttachId: 0x%08X, fd: %d")
DECL_LOG_EVENT(PALLOC, PALLOC_IMPORTID,		    LOW,	"PALLOC_Import ui32AttachId: 0x%08X, *pui32AllocId: 0x%08X")
DECL_LOG_EVENT_END()

#ifdef __cplusplus
}
#endif

/* Second pass control */
#ifdef LOG_SECOND_PASS
#include __FILE__
#endif 
