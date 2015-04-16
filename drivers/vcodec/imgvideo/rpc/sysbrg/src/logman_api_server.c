/*!
 *****************************************************************************
 *
 * @file	   logman_api_server.c
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

#include "sysbrg_api.h"
#include "sysbrg_api_km.h"
#include "sysos_api_km.h"
#include "logman_api.h"
#include "logman_api_rpc.h"


IMG_VOID LOGMAN_dispatch(SYSBRG_sPacket __user *psPacket)
{
	SYSBRG_sPacket sPacket;
	LOGMAN_sCmdMsg sCommandMsg;
	LOGMAN_sRespMsg sResponseMsg;

	if(SYSOSKM_CopyFromUser(&sPacket, psPacket, sizeof(sPacket)))
		IMG_ASSERT(!"failed to copy from user");
	if(SYSOSKM_CopyFromUser(&sCommandMsg, sPacket.pvCmdData, sizeof(sCommandMsg)))
		IMG_ASSERT(!"failed to copy from user");

	switch (sCommandMsg.eFuncId)
	{
	
      case LOGMAN_EnableLogging_ID:
      
      
	sResponseMsg.sResp.sLOGMAN_EnableLoggingResp.xLOGMAN_EnableLoggingResp =
      		LOGMAN_EnableLogging(
      
	  sCommandMsg.sCmd.sLOGMAN_EnableLoggingCmd.i32ApiId,
	  sCommandMsg.sCmd.sLOGMAN_EnableLoggingCmd.i32LogLevel
      );
      break;
      
    
      case LOGMAN_DisableLogging_ID:
      
      
	sResponseMsg.sResp.sLOGMAN_DisableLoggingResp.xLOGMAN_DisableLoggingResp =
      		LOGMAN_DisableLogging(
      
	  sCommandMsg.sCmd.sLOGMAN_DisableLoggingCmd.i32ApiId
      );
      break;
      
    
      case LOGMAN_GetApiSize_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sLOGMAN_GetApiSizeCmd.pi32ApiSize, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sLOGMAN_GetApiSizeCmd.pi32ApiSize)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sLOGMAN_GetApiSizeResp.xLOGMAN_GetApiSizeResp =
      		LOGMAN_GetApiSize(
      
	  sCommandMsg.sCmd.sLOGMAN_GetApiSizeCmd.pi32ApiSize
      );
      break;
      
    
      case LOGMAN_GetApiList_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sLOGMAN_GetApiListCmd.psApiInfo, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sLOGMAN_GetApiListCmd.psApiInfo)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sLOGMAN_GetApiListResp.xLOGMAN_GetApiListResp =
      		LOGMAN_GetApiList(
      
	  sCommandMsg.sCmd.sLOGMAN_GetApiListCmd.psApiInfo
      );
      break;
      
    
      case LOGMAN_GetApiEvents_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sLOGMAN_GetApiEventsCmd.pi32ApiEvents, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sLOGMAN_GetApiEventsCmd.pi32ApiEvents)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sLOGMAN_GetApiEventsResp.xLOGMAN_GetApiEventsResp =
      		LOGMAN_GetApiEvents(
      
	  sCommandMsg.sCmd.sLOGMAN_GetApiEventsCmd.pi32ApiEvents
      );
      break;
      
    
      case LOGMAN_GetFormatStrings_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sLOGMAN_GetFormatStringsCmd.pszString, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sLOGMAN_GetFormatStringsCmd.pszString)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sLOGMAN_GetFormatStringsResp.xLOGMAN_GetFormatStringsResp =
      		LOGMAN_GetFormatStrings(
      
	  sCommandMsg.sCmd.sLOGMAN_GetFormatStringsCmd.pszString,
	  sCommandMsg.sCmd.sLOGMAN_GetFormatStringsCmd.i32ApiIndex,
	  sCommandMsg.sCmd.sLOGMAN_GetFormatStringsCmd.i32Event
      );
      break;
      
    
      case LOGMAN_TestLogEnabled_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sLOGMAN_TestLogEnabledCmd.pbLogEnabled, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sLOGMAN_TestLogEnabledCmd.pbLogEnabled)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sLOGMAN_TestLogEnabledResp.xLOGMAN_TestLogEnabledResp =
      		LOGMAN_TestLogEnabled(
      
	  sCommandMsg.sCmd.sLOGMAN_TestLogEnabledCmd.pbLogEnabled
      );
      break;
      
    
	}
	if(SYSOSKM_CopyToUser(sPacket.pvRespData, &sResponseMsg, sizeof(sResponseMsg)))
		IMG_ASSERT(!"failed to copy to user");
}
