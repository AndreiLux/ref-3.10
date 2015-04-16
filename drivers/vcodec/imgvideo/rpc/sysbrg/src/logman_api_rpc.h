/*!
 *****************************************************************************
 *
 * @file	   logman_api_rpc.h
 *
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

#ifndef __LOGMAN_RPC_H__
#define __LOGMAN_RPC_H__

#include "img_defs.h"
#include "sysbrg_api.h"
#include "logman_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	LOGMAN_EnableLogging_ID,
	LOGMAN_DisableLogging_ID,
	LOGMAN_GetApiSize_ID,
	LOGMAN_GetApiList_ID,
	LOGMAN_GetApiEvents_ID,
	LOGMAN_GetFormatStrings_ID,
	LOGMAN_TestLogEnabled_ID,

} LOGMAN_eFuncId;

typedef struct
{
	LOGMAN_eFuncId	eFuncId;
    union
	{
	
		struct
		{
			 IMG_INT32 i32ApiId;
			 IMG_INT32 i32LogLevel;
	
		} sLOGMAN_EnableLoggingCmd;
	
		struct
		{
			 IMG_INT32 i32ApiId;
	
		} sLOGMAN_DisableLoggingCmd;
	
		struct
		{
			 IMG_INT32 __user * pi32ApiSize;
	
		} sLOGMAN_GetApiSizeCmd;
	
		struct
		{
			 LOG_sApiInfo __user * psApiInfo;
	
		} sLOGMAN_GetApiListCmd;
	
		struct
		{
			 IMG_INT32 __user * pi32ApiEvents;
	
		} sLOGMAN_GetApiEventsCmd;
	
		struct
		{
			 IMG_CHAR __user * pszString;
			 IMG_INT32 i32ApiIndex;
			 IMG_INT32 i32Event;
	
		} sLOGMAN_GetFormatStringsCmd;
	
		struct
		{
			 IMG_BOOL __user * pbLogEnabled;
	
		} sLOGMAN_TestLogEnabledCmd;
	
	} sCmd;
} LOGMAN_sCmdMsg;

typedef struct
{
    union
	{
	
		struct
		{
			IMG_RESULT		xLOGMAN_EnableLoggingResp;
		} sLOGMAN_EnableLoggingResp;
	
		struct
		{
			IMG_RESULT		xLOGMAN_DisableLoggingResp;
		} sLOGMAN_DisableLoggingResp;
	
		struct
		{
			IMG_RESULT		xLOGMAN_GetApiSizeResp;
		} sLOGMAN_GetApiSizeResp;
	
		struct
		{
			IMG_RESULT		xLOGMAN_GetApiListResp;
		} sLOGMAN_GetApiListResp;
	
		struct
		{
			IMG_RESULT		xLOGMAN_GetApiEventsResp;
		} sLOGMAN_GetApiEventsResp;
	
		struct
		{
			IMG_RESULT		xLOGMAN_GetFormatStringsResp;
		} sLOGMAN_GetFormatStringsResp;
	
		struct
		{
			IMG_RESULT		xLOGMAN_TestLogEnabledResp;
		} sLOGMAN_TestLogEnabledResp;
	
	} sResp;
} LOGMAN_sRespMsg;



extern IMG_VOID LOGMAN_dispatch(SYSBRG_sPacket __user *psPacket);

#ifdef __cplusplus
}
#endif

#endif
