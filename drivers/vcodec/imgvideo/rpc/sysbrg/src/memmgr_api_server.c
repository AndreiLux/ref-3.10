/*!
 *****************************************************************************
 *
 * @file	   memmgr_api_server.c
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
#include "memmgr_api.h"
#include "memmgr_api_rpc.h"


IMG_VOID MEMMGR_dispatch(SYSBRG_sPacket __user *psPacket)
{
	SYSBRG_sPacket sPacket;
	MEMMGR_sCmdMsg sCommandMsg;
	MEMMGR_sRespMsg sResponseMsg;

	if(SYSOSKM_CopyFromUser(&sPacket, psPacket, sizeof(sPacket)))
		IMG_ASSERT(!"failed to copy from user");
	if(SYSOSKM_CopyFromUser(&sCommandMsg, sPacket.pvCmdData, sizeof(sCommandMsg)))
		IMG_ASSERT(!"failed to copy from user");

	switch (sCommandMsg.eFuncId)
	{
	
      case WriteMemRefToMemRef_ID:
      
      		WriteMemRefToMemRef(
      
	  sCommandMsg.sCmd.sWriteMemRefToMemRefCmd.ui32MemSpaceId,
	  sCommandMsg.sCmd.sWriteMemRefToMemRefCmd.hDeviceMem,
	  sCommandMsg.sCmd.sWriteMemRefToMemRefCmd.ui32Offset,
	  sCommandMsg.sCmd.sWriteMemRefToMemRefCmd.ui32ManglerFuncIdExt,
	  sCommandMsg.sCmd.sWriteMemRefToMemRefCmd.hRefDeviceMem,
	  sCommandMsg.sCmd.sWriteMemRefToMemRefCmd.ui32RefOffset
      );
      break;
      
    
      case TOPAZKM_MMUMAllocateHeapDeviceMemory_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sTOPAZKM_MMUMAllocateHeapDeviceMemoryCmd.ppMemInfo, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sTOPAZKM_MMUMAllocateHeapDeviceMemoryCmd.ppMemInfo)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sTOPAZKM_MMUMAllocateHeapDeviceMemoryResp.xTOPAZKM_MMUMAllocateHeapDeviceMemoryResp =
      		TOPAZKM_MMUMAllocateHeapDeviceMemory(
      
	  sCommandMsg.sCmd.sTOPAZKM_MMUMAllocateHeapDeviceMemoryCmd.ui32Size,
	  sCommandMsg.sCmd.sTOPAZKM_MMUMAllocateHeapDeviceMemoryCmd.ui32Alignment,
	  sCommandMsg.sCmd.sTOPAZKM_MMUMAllocateHeapDeviceMemoryCmd.ui32Heap,
	  sCommandMsg.sCmd.sTOPAZKM_MMUMAllocateHeapDeviceMemoryCmd.bSaveRestore,
	  sCommandMsg.sCmd.sTOPAZKM_MMUMAllocateHeapDeviceMemoryCmd.ppMemInfo,
	  sCommandMsg.sCmd.sTOPAZKM_MMUMAllocateHeapDeviceMemoryCmd.tileSensetive
      );
      break;
      
    
      case TOPAZKM_StreamMMUMAllocateHeapDeviceMemory_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sTOPAZKM_StreamMMUMAllocateHeapDeviceMemoryCmd.ppMemInfo, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sTOPAZKM_StreamMMUMAllocateHeapDeviceMemoryCmd.ppMemInfo)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sTOPAZKM_StreamMMUMAllocateHeapDeviceMemoryResp.xTOPAZKM_StreamMMUMAllocateHeapDeviceMemoryResp =
      		TOPAZKM_StreamMMUMAllocateHeapDeviceMemory(
      
	  sCommandMsg.sCmd.sTOPAZKM_StreamMMUMAllocateHeapDeviceMemoryCmd.ui32StreamId,
	  sCommandMsg.sCmd.sTOPAZKM_StreamMMUMAllocateHeapDeviceMemoryCmd.ui32Size,
	  sCommandMsg.sCmd.sTOPAZKM_StreamMMUMAllocateHeapDeviceMemoryCmd.ui32Alignment,
	  sCommandMsg.sCmd.sTOPAZKM_StreamMMUMAllocateHeapDeviceMemoryCmd.ui32Heap,
	  sCommandMsg.sCmd.sTOPAZKM_StreamMMUMAllocateHeapDeviceMemoryCmd.bSaveRestore,
	  sCommandMsg.sCmd.sTOPAZKM_StreamMMUMAllocateHeapDeviceMemoryCmd.ppMemInfo,
	  sCommandMsg.sCmd.sTOPAZKM_StreamMMUMAllocateHeapDeviceMemoryCmd.tileSensetive
      );
      break;
      
    
      case TOPAZKM_MMUMFreeDeviceMemory_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sTOPAZKM_MMUMFreeDeviceMemoryCmd.pMemInfo, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sTOPAZKM_MMUMFreeDeviceMemoryCmd.pMemInfo)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sTOPAZKM_MMUMFreeDeviceMemoryResp.xTOPAZKM_MMUMFreeDeviceMemoryResp =
      		TOPAZKM_MMUMFreeDeviceMemory(
      
	  sCommandMsg.sCmd.sTOPAZKM_MMUMFreeDeviceMemoryCmd.pMemInfo
      );
      break;
      
    
      case TOPAZKM_MMUFlushMMUTableCache_ID:
      
      
	sResponseMsg.sResp.sTOPAZKM_MMUFlushMMUTableCacheResp.xTOPAZKM_MMUFlushMMUTableCacheResp =
      		TOPAZKM_MMUFlushMMUTableCache(
      
      );
      break;
      
    
      case TOPAZKM_MapExternal_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sTOPAZKM_MapExternalCmd.pvUM, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sTOPAZKM_MapExternalCmd.pvUM)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sTOPAZKM_MapExternalCmd.memInfo, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sTOPAZKM_MapExternalCmd.memInfo)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sTOPAZKM_MapExternalResp.xTOPAZKM_MapExternalResp =
      		TOPAZKM_MapExternal(
      
	  sCommandMsg.sCmd.sTOPAZKM_MapExternalCmd.ui32BufLen,
	  sCommandMsg.sCmd.sTOPAZKM_MapExternalCmd.pvUM,
	  sCommandMsg.sCmd.sTOPAZKM_MapExternalCmd.pallocHandle,
	  sCommandMsg.sCmd.sTOPAZKM_MapExternalCmd.ui32Heap,
	  sCommandMsg.sCmd.sTOPAZKM_MapExternalCmd.ui32Alignment,
	  sCommandMsg.sCmd.sTOPAZKM_MapExternalCmd.memInfo
      );
      break;
      
    
      case TOPAZKM_UnMapExternal_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sTOPAZKM_UnMapExternalCmd.memInfo, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sTOPAZKM_UnMapExternalCmd.memInfo)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sTOPAZKM_UnMapExternalResp.xTOPAZKM_UnMapExternalResp =
      		TOPAZKM_UnMapExternal(
      
	  sCommandMsg.sCmd.sTOPAZKM_UnMapExternalCmd.memInfo
      );
      break;
      
    
      case TOPAZKM_MMCopyTiledBuffer_ID:
      
      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sTOPAZKM_MMCopyTiledBufferCmd.pMemoryInfo, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sTOPAZKM_MMCopyTiledBufferCmd.pMemoryInfo)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
		#ifdef WIN32
		if(!SYSBRGKM_CheckParams((IMG_VOID __user *)&sCommandMsg.sCmd.sTOPAZKM_MMCopyTiledBufferCmd.pcBuffer, sizeof((SYSBRG_sPacket *)&sCommandMsg.sCmd.sTOPAZKM_MMCopyTiledBufferCmd.pcBuffer)))
		    IMG_ASSERT(IMG_FALSE);
		#else
        // not required to check pointer access under linux, as SYSOSKM_Copy{To,From}User has to be used
		#endif
	      
	sResponseMsg.sResp.sTOPAZKM_MMCopyTiledBufferResp.xTOPAZKM_MMCopyTiledBufferResp =
      		TOPAZKM_MMCopyTiledBuffer(
      
	  sCommandMsg.sCmd.sTOPAZKM_MMCopyTiledBufferCmd.pMemoryInfo,
	  sCommandMsg.sCmd.sTOPAZKM_MMCopyTiledBufferCmd.pcBuffer,
	  sCommandMsg.sCmd.sTOPAZKM_MMCopyTiledBufferCmd.ui32Size,
	  sCommandMsg.sCmd.sTOPAZKM_MMCopyTiledBufferCmd.ui32Offset,
	  sCommandMsg.sCmd.sTOPAZKM_MMCopyTiledBufferCmd.bToMemory
      );
      break;
      
    
	}
	if(SYSOSKM_CopyToUser(sPacket.pvRespData, &sResponseMsg, sizeof(sResponseMsg)))
		IMG_ASSERT(!"failed to copy to user");
}
