/*!
 *****************************************************************************
 *
 * @file	   hostutils_api_server.c
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
#include "hostutils_api.h"
#include "hostutils_api_rpc.h"


IMG_VOID HOSTUTILS_dispatch(SYSBRG_sPacket __user *psPacket)
{
	SYSBRG_sPacket sPacket;
	HOSTUTILS_sCmdMsg sCommandMsg;
	HOSTUTILS_sRespMsg sResponseMsg;

	if(SYSOSKM_CopyFromUser(&sPacket, psPacket, sizeof(sPacket)))
		IMG_ASSERT(!"failed to copy from user");
	if(SYSOSKM_CopyFromUser(&sCommandMsg, sPacket.pvCmdData, sizeof(sCommandMsg)))
		IMG_ASSERT(!"failed to copy from user");

	switch (sCommandMsg.eFuncId)
	{
	
      case COMMKM_Initialize_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sCOMMKM_InitializeCmd.apsWBDataInfo, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sCOMMKM_InitializeCmd.apsWBDataInfo)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sCOMMKM_InitializeCmd.pui32NumPipes, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sCOMMKM_InitializeCmd.pui32NumPipes)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sCOMMKM_InitializeResp.xCOMMKM_InitializeResp =
      		COMMKM_Initialize(
      
	  sCommandMsg.sCmd.sCOMMKM_InitializeCmd.ui32ConnId,
	  sCommandMsg.sCmd.sCOMMKM_InitializeCmd.apsWBDataInfo,
	  sCommandMsg.sCmd.sCOMMKM_InitializeCmd.pui32NumPipes,
	  sCommandMsg.sCmd.sCOMMKM_InitializeCmd.ui32MmuFlags,
	  sCommandMsg.sCmd.sCOMMKM_InitializeCmd.ui32MMUTileStride
      );
      break;
      
    
      case COMMKM_OpenSocket_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sCOMMKM_OpenSocketCmd.ui32SockId, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sCOMMKM_OpenSocketCmd.ui32SockId)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sCOMMKM_OpenSocketResp.xCOMMKM_OpenSocketResp =
      		COMMKM_OpenSocket(
      
	  sCommandMsg.sCmd.sCOMMKM_OpenSocketCmd.ui32ConnId,
	  sCommandMsg.sCmd.sCOMMKM_OpenSocketCmd.ui32SockId,
	  sCommandMsg.sCmd.sCOMMKM_OpenSocketCmd.eCodec
      );
      break;
      
    
      case COMMKM_SetupSocket_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.psBiasTables, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.psBiasTables)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.ui8CtxtNum, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.ui8CtxtNum)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.usedSocket, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.usedSocket)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sCOMMKM_SetupSocketResp.xCOMMKM_SetupSocketResp =
      		COMMKM_SetupSocket(
      
	  sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.ui32SockId,
	  sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.psBiasTables,
	  sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.ui16FrameHeight,
	  sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.ui16Width,
	  sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.bDoSerializedComm,
	  sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.ui8CtxtNum,
	  sCommandMsg.sCmd.sCOMMKM_SetupSocketCmd.usedSocket
      );
      break;
      
    
      case COMMKM_CloseSocket_ID:
      
      
	sResponseMsg.sResp.sCOMMKM_CloseSocketResp.xCOMMKM_CloseSocketResp =
      		COMMKM_CloseSocket(
      
	  sCommandMsg.sCmd.sCOMMKM_CloseSocketCmd.ui32SockId
      );
      break;
      
    
      case COMMKM_Send_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sCOMMKM_SendCmd.pMsg, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sCOMMKM_SendCmd.pMsg)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sCOMMKM_SendCmd.pui32WritebackVal, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sCOMMKM_SendCmd.pui32WritebackVal)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sCOMMKM_SendResp.xCOMMKM_SendResp =
      		COMMKM_Send(
      
	  sCommandMsg.sCmd.sCOMMKM_SendCmd.ui32SockId,
	  sCommandMsg.sCmd.sCOMMKM_SendCmd.pMsg,
	  sCommandMsg.sCmd.sCOMMKM_SendCmd.pui32WritebackVal
      );
      break;
      
    
      case COMMKM_Recv_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sCOMMKM_RecvCmd.pMsg, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sCOMMKM_RecvCmd.pMsg)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sCOMMKM_RecvResp.xCOMMKM_RecvResp =
      		COMMKM_Recv(
      
	  sCommandMsg.sCmd.sCOMMKM_RecvCmd.ui32SockId,
	  sCommandMsg.sCmd.sCOMMKM_RecvCmd.pMsg,
	  sCommandMsg.sCmd.sCOMMKM_RecvCmd.bBlocking
      );
      break;
      
    
      case COMMKM_LoadBias_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sCOMMKM_LoadBiasCmd.psBiasTables, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sCOMMKM_LoadBiasCmd.psBiasTables)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sCOMMKM_LoadBiasResp.xCOMMKM_LoadBiasResp =
      		COMMKM_LoadBias(
      
	  sCommandMsg.sCmd.sCOMMKM_LoadBiasCmd.psBiasTables,
	  sCommandMsg.sCmd.sCOMMKM_LoadBiasCmd.eStandard
      );
      break;
      
    
      case COMMKM_GetFwConfigInt_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sCOMMKM_GetFwConfigIntCmd.name, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sCOMMKM_GetFwConfigIntCmd.name)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sCOMMKM_GetFwConfigIntResp.xCOMMKM_GetFwConfigIntResp =
      		COMMKM_GetFwConfigInt(
      
	  sCommandMsg.sCmd.sCOMMKM_GetFwConfigIntCmd.ui32ConnId,
	  sCommandMsg.sCmd.sCOMMKM_GetFwConfigIntCmd.name
      );
      break;
      
    
      case COMM_GetPipeUsage_ID:
      
      
	sResponseMsg.sResp.sCOMM_GetPipeUsageResp.xCOMM_GetPipeUsageResp =
      		COMM_GetPipeUsage(
      
	  sCommandMsg.sCmd.sCOMM_GetPipeUsageCmd.ui8Pipe
      );
      break;
      
    
      case COMM_SetPipeUsage_ID:
      
      		COMM_SetPipeUsage(
      
	  sCommandMsg.sCmd.sCOMM_SetPipeUsageCmd.ui8Pipe,
	  sCommandMsg.sCmd.sCOMM_SetPipeUsageCmd.ui8Val
      );
      break;
      
    
      case COMMKM_IsIdle_ID:
      
      
	sResponseMsg.sResp.sCOMMKM_IsIdleResp.xCOMMKM_IsIdleResp =
      		COMMKM_IsIdle(
      
	  sCommandMsg.sCmd.sCOMMKM_IsIdleCmd.ui32SockId
      );
      break;
      
    
      case TOPAZKM_IsIdle_ID:
      
      
	sResponseMsg.sResp.sTOPAZKM_IsIdleResp.xTOPAZKM_IsIdleResp =
      		TOPAZKM_IsIdle(
      
	  sCommandMsg.sCmd.sTOPAZKM_IsIdleCmd.ui32SockId
      );
      break;
      
    
      case TOPAZKM_Suspend_ID:
      
      		TOPAZKM_Suspend(
      
	  sCommandMsg.sCmd.sTOPAZKM_SuspendCmd.ui32ConnId
      );
      break;
      
    
      case TOPAZKM_Resume_ID:
      
      		TOPAZKM_Resume(
      
	  sCommandMsg.sCmd.sTOPAZKM_ResumeCmd.ui32ConnId
      );
      break;
      
    
      case TOPAZKM_GetCoreDes1_ID:
      
      
	sResponseMsg.sResp.sTOPAZKM_GetCoreDes1Resp.xTOPAZKM_GetCoreDes1Resp =
      		TOPAZKM_GetCoreDes1(
      
      );
      break;
      
    
      case TOPAZKM_GetCoreRev_ID:
      
      
	sResponseMsg.sResp.sTOPAZKM_GetCoreRevResp.xTOPAZKM_GetCoreRevResp =
      		TOPAZKM_GetCoreRev(
      
      );
      break;
      
    
      case TOPAZKM_GetNumPipes_ID:
      
      
	sResponseMsg.sResp.sTOPAZKM_GetNumPipesResp.xTOPAZKM_GetNumPipesResp =
      		TOPAZKM_GetNumPipes(
      
      );
      break;
      
    
	}
	if(SYSOSKM_CopyToUser(sPacket.pvRespData, &sResponseMsg, sizeof(sResponseMsg)))
		IMG_ASSERT(!"failed to copy to user");
}
