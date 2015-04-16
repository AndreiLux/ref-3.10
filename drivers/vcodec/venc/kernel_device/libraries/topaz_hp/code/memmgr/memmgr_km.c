/*!
 *****************************************************************************
 *
 * @file	   memmgr_km.c
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

#include "tal.h"
#include "rman_api.h"
#include "img_types.h"
#include "img_errors.h"
#include "sysmem_utils.h"
#include "talmmu_api.h"
#include "sysmem_api_km.h"

#include "memmgr_km.h"
#include "topazsc_mmu.h"
#include "sysos_api_km.h"
#include "sysdev_api_km.h"
#include "sysdev_utils.h"

#define RMAN_SOCKETS_ID 0x101
#define RMAN_BUFFERS_ID 0x102

extern IMG_HANDLE g_SYSDevHandle;
extern IMG_BOOL g_bUseTiledMemory;
extern IMG_BOOL g_bUseInterleavedTiling;
extern IMG_BOOL g_bUseSecureFwUpload;
extern SYSDEVU_sInfo topaz_device;
extern IMG_HANDLE hMMUTemplate;

struct TopazScMMUContext {
	IMG_HANDLE topazsc_mmu_context;
	IMG_UINT32 ptd_phys_addr;
};

extern struct TopazScMMUContext hTopazScMMUContext;
extern IMG_UINT32 g_ui32MMUTileStride;
extern TALMMU_sHeapInfo	asMMU_HeapInfo[];
extern IMG_BOOL g_bUseExtendedAddressing;
extern TALMMU_sDevMemInfo sMMU_DeviceMemoryInfo;

IMG_BOOL allocateMemoryHelper(
	IMG_UINT32	ui32Size,
	IMG_UINT32	ui32Alignment,
	IMG_UINT32  ui32Heap,
	IMG_BOOL	bSaveRestore,
	MEMORY_INFO	*ppMemInfo,
	IMG_BOOL tileSensetive)
{
	IMG_HANDLE		hTemp;
	IMG_RESULT		result;
	IMG_UINT64		ui64Addr;

	/* If tileSensetive then change the alignment and the heapID, otherwise .. don't change the params. */
	if(tileSensetive && g_bUseTiledMemory)
	{
			ui32Alignment = 512 << asMMU_HeapInfo[0].ui32XTileStride;
			ui32Heap = 0;
	}

	/* Do an up update to target so that there is no uninitialise data in the test*/
	result = TALMMU_DevMemMalloc1(hTopazScMMUContext.topazsc_mmu_context,
									asMMU_HeapInfo[ui32Heap].ui32HeapId,
									ui32Size,
									ui32Alignment, &hTemp);
	IMG_ASSERT(result == IMG_SUCCESS);
	if(result != IMG_SUCCESS)
		return IMG_FALSE;

	result = TALMMU_CpuMemMap (hTemp, (IMG_VOID **)&(ppMemInfo->pvLinAddress));
	IMG_ASSERT(result == IMG_SUCCESS);
	ppMemInfo->hShadowMem = hTemp;
	ui64Addr = SYSMEMU_CpuKmAddrToCpuPAddr(topaz_device.sMemPool, (IMG_VOID*)(IMG_UINTPTR)ppMemInfo->pvLinAddress);
	ppMemInfo->ui32umTocken = ui64Addr & 0xffffffff;
	ui64Addr = SYSDEVKM_CpuPAddrToDevPAddr(g_SYSDevHandle, ui64Addr);
	if (result != IMG_SUCCESS)
	{
		return IMG_FALSE;
	}

	ppMemInfo->ui32DevPhysAddr = (IMG_UINT32)ui64Addr;
	ppMemInfo->ui32Size = ui32Size;
	ppMemInfo->bufferId = 0;

	return(IMG_TRUE);
}

IMG_BOOL freeMemoryHelper(MEMORY_INFO *pMemInfo)
{
	if (TALMMU_DevMemFree(hTopazScMMUContext.topazsc_mmu_context, pMemInfo->hShadowMem) == IMG_SUCCESS)
		return(IMG_TRUE);
	return IMG_FALSE;
}

static IMG_BOOL allocateMemoryCommon(
	TOPAZKM_DevContext *devContext,
	IMG_UINT32	ui32Size,
	IMG_UINT32	ui32Alignment,
	IMG_UINT32  ui32Heap,
	IMG_BOOL	bSaveRestore,
	MEMORY_INFO	**ppMemInfo, IMG_BOOL tileSensetive)
{
	IMG_BOOL result;

	MEMORY_INFO *pMemoryInfo = (MEMORY_INFO *)IMG_MALLOC(sizeof(MEMORY_INFO));
	IMG_ASSERT(pMemoryInfo != IMG_NULL);
	if(pMemoryInfo == IMG_NULL)
		return IMG_FALSE;
	IMG_MEMSET(pMemoryInfo, 0, sizeof(*pMemoryInfo));
	result = allocateMemoryHelper(ui32Size, ui32Alignment, ui32Heap, bSaveRestore, pMemoryInfo, tileSensetive);
	IMG_ASSERT(result == IMG_TRUE);
	if(result != IMG_TRUE)
	{
		IMG_FREE(pMemoryInfo);
		return IMG_FALSE;
	}

	pMemoryInfo->iu32MemoryRegionID = TAL_GetMemSpaceHandle("MEMSYSMEM");

	*ppMemInfo = pMemoryInfo;
	return IMG_TRUE;
}

IMG_BOOL allocMemory(
	TOPAZKM_DevContext *devContext,
	IMG_UINT32	ui32Size,
	IMG_UINT32	ui32Alignment,
	IMG_BOOL	bSaveRestore,
	MEMORY_INFO	**ppMemInfo)
{
	return allocateMemoryCommon(devContext, ui32Size, ui32Alignment, 1, bSaveRestore, ppMemInfo, IMG_FALSE);
}

IMG_BOOL allocGenericMemory(
	TOPAZKM_DevContext *devContext,
	IMG_UINT32	ui32Size,
	struct MEMORY_INFO_TAG	**ppMemoryInfo)
{
	IMG_PVOID	pData;

	if (allocateMemoryCommon(devContext, ui32Size, 64, 1, IMG_FALSE, ppMemoryInfo, IMG_FALSE))
	{
		pData = getKMAddress(*ppMemoryInfo);
		IMG_MEMSET(pData, 0, ui32Size);
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

IMG_BOOL allocNonMMUMemory(
	TOPAZKM_DevContext *devContext,
	IMG_UINT32	ui32Size,
	IMG_UINT32	ui32Alignment,
	IMG_BOOL	bSaveRestore,
	MEMORY_INFO	**ppMemInfo)
{
	IMG_UINT32 ui32Result;
	IMG_UINT64 ui64Addr;
	MEMORY_INFO	*pMemoryInfo;

	pMemoryInfo = (MEMORY_INFO*) IMG_MALLOC(sizeof(MEMORY_INFO));
	IMG_ASSERT(pMemoryInfo != IMG_NULL);
	if(pMemoryInfo == IMG_NULL)
	{
		*ppMemInfo = IMG_NULL;
		return(IMG_FALSE);
	}





	IMG_MEMSET(pMemoryInfo, 0, sizeof(*pMemoryInfo));
	ui32Result = SYSMEMU_AllocatePages(ui32Size, sMMU_DeviceMemoryInfo.eMemAttrib, sMMU_DeviceMemoryInfo.eMemPool, &pMemoryInfo->sysMemHandle, (IMG_UINT64 **)&(pMemoryInfo->pvLinAddress));
	IMG_ASSERT(ui32Result == IMG_SUCCESS);
	if (ui32Result != IMG_SUCCESS)
	{
		IMG_FREE(pMemoryInfo);
		*ppMemInfo = pMemoryInfo = NULL;
		return(IMG_FALSE);
	}


	ui64Addr = SYSMEMU_CpuKmAddrToCpuPAddr(topaz_device.sMemPool, (IMG_VOID*)(IMG_UINTPTR)pMemoryInfo->pvLinAddress);
	ui64Addr = SYSDEVKM_CpuPAddrToDevPAddr(g_SYSDevHandle, ui64Addr);

	pMemoryInfo->ui32DevPhysAddr = (IMG_UINT32)ui64Addr;
	pMemoryInfo->hShadowMem = pMemoryInfo;
	*ppMemInfo = pMemoryInfo;
	return(IMG_TRUE);
}

IMG_BOOL freeMemory(MEMORY_INFO **ppMemoryInfo)
{
	freeMemoryHelper(*ppMemoryInfo);
	IMG_FREE(*ppMemoryInfo);
	*ppMemoryInfo = IMG_NULL;
	return(IMG_TRUE);
}

IMG_BOOL freeMemoryNonMMU(MEMORY_INFO **ppMemoryInfo)
{
	MEMORY_INFO *pMemoryInfo = *ppMemoryInfo;
	#if defined (__TALMMU_USE_SYSAPIS__)
		SYSMEMU_FreePages(pMemoryInfo->sysMemHandle);
	#else
		IMG_ASSERT(0);
	#endif
	*ppMemoryInfo = IMG_NULL;
	return(IMG_TRUE);
}


IMG_PVOID getKMAddress(MEMORY_INFO *pMemoryInfo)
{
#if (IMG_KERNEL_MODULE)
	return(pMemoryInfo->pvLinAddress);
#else
	if (pMemoryInfo->pvUmAddress == NULL)
		/* Lazily map the memory to user space, this happens when the memory was allocated in the kernel and passed to the user space */
		SYSBRG_CreateCpuUmAddrMapping(pMemoryInfo->pvLinAddress, pMemoryInfo->ui32Size, &pMemoryInfo->pvUmAddress);
	IMG_ASSERT(pMemoryInfo->pvUmAddress);
#endif

	return pMemoryInfo->pvUmAddress;
}

/*****************************************************************************
 FUNCTION	: MMWriteDeviceMemRef

 PURPOSE	: Abstracts old TAL call 'TALREG_WriteDevMemRef32' so host isn't calling TAL directly.
			  Also allows behind the scenes substitution of TAL access with TALMMU access

 PARAMETERS	:

 RETURNS	:

 Notes		:
*****************************************************************************/
IMG_VOID writeMemoryRef(
	IMG_HANDLE                      ui32MemSpaceId,
    IMG_UINT32                      ui32Offset,
    IMG_HANDLE                      hRefDeviceMem,
    IMG_UINT32                      ui32RefOffset)
{
	MEMORY_INFO *srcMem;
	IMG_UINT32 devVirtAddress;
	IMG_UINT32 ui32Result;

	srcMem = (MEMORY_INFO *)hRefDeviceMem;
	ui32Result = TALMMU_GetDevVirtAddress(srcMem->hShadowMem, &devVirtAddress);
	IMG_ASSERT(ui32Result == IMG_SUCCESS);
	TALREG_WriteWord32(ui32MemSpaceId, ui32Offset, devVirtAddress + ui32RefOffset);
}

IMG_VOID writeMemoryRefNoMMU(
	IMG_HANDLE                      ui32MemSpaceId,
    IMG_UINT32                      ui32Offset,
    IMG_HANDLE                      hRefDeviceMem,
    IMG_UINT32                      ui32RefOffset)
{
	MEMORY_INFO *mem = (MEMORY_INFO *)hRefDeviceMem;
	TALREG_WriteWord32(ui32MemSpaceId, ui32Offset, mem->ui32DevPhysAddr + ui32RefOffset);
}

IMG_VOID writeMemRefToMemRef(
    IMG_HANDLE                      ui32MemSpaceId,
    IMG_HANDLE                      hDeviceMem,
    IMG_UINT32                      ui32Offset,
    IMG_UINT32                      ui32ManglerFuncIdExt,
    IMG_HANDLE                      hRefDeviceMem,
    IMG_UINT32                      ui32RefOffset)
{
	MEMORY_INFO *memDst = hDeviceMem;
	MEMORY_INFO *memSrc = hRefDeviceMem;
	IMG_UINT32 devVirtAddress;
	IMG_RESULT ui32Result;
	ui32Result = TALMMU_GetDevVirtAddress(memSrc->hShadowMem, &devVirtAddress);
	IMG_ASSERT(ui32Result == IMG_SUCCESS);
	*(IMG_UINT32 *)(memDst->pvLinAddress + ui32Offset) = devVirtAddress + ui32RefOffset;
}

/*
 * Bridge ..
 */

/*****************************************************************************
 FUNCTION	: MMFreeDeviceMemory

 PURPOSE	: Free device memory

 PARAMETERS	:	pvMemInfo				Location of allocation handle to TRACK_FREE

 RETURNS	: IMG_TRUE - successful, IMG_FALSE - failed

 Notes		:
*****************************************************************************/
IMG_BOOL TOPAZKM_MMUMFreeDeviceMemory(MEMORY_INFO *pMemInfo)
{
	IMG_RESULT result;
	IMG_HANDLE resHandle;
	MEMORY_INFO *pMemoryInfo = (MEMORY_INFO *)IMG_MALLOC(sizeof(MEMORY_INFO));
	IMG_ASSERT(pMemoryInfo != IMG_NULL);
	if(pMemoryInfo == IMG_NULL)
	{
		return(IMG_FALSE);
	}
	
	IMG_MEMSET(pMemoryInfo, 0, sizeof(*pMemoryInfo));
	result = SYSOSKM_CopyFromUser(pMemoryInfo, pMemInfo, sizeof(MEMORY_INFO));
	IMG_ASSERT(result == IMG_SUCCESS);

	if(pMemoryInfo->bufferId)
	{
		// Stream buffer
		result = RMAN_GetResource(pMemoryInfo->bufferId, RMAN_BUFFERS_ID, IMG_NULL, &resHandle);
		IMG_ASSERT(result == IMG_SUCCESS);
		if (result != IMG_SUCCESS)
		{
			IMG_FREE(pMemoryInfo);
			return IMG_FALSE;
		}

		RMAN_FreeResource(resHandle);
	}
	else
	{
		freeMemoryHelper(pMemoryInfo);
	}

	IMG_FREE(pMemoryInfo);

	return(IMG_TRUE);
}

IMG_BOOL MMUDeviceMemoryInitialise(IMG_UINT32 ui32MmuFlags, IMG_UINT32 ui32MMUTileStride)
{
	g_bUseTiledMemory = (ui32MmuFlags & MMU_TILED_FLAG);
	g_bUseInterleavedTiling = (ui32MmuFlags & MMU_TILED_INTERLEAVED);
	g_bUseSecureFwUpload = (ui32MmuFlags & MMU_SECURE_FW_UPLOAD);

	g_bUseExtendedAddressing = (ui32MmuFlags & MMU_EXTENDED_ADDR_FLAG);

	//TALMMU_Initialise(); Called in the following function instead
	// Let's try configuring our MMU space here for now..
	g_ui32MMUTileStride = 512;
	while (g_ui32MMUTileStride < ui32MMUTileStride) g_ui32MMUTileStride <<= 1;

	return TopazSc_MMU_Configure();
}

IMG_BOOL MMUDeviceMemoryHWSetup(IMG_HANDLE ui32TCoreReg)
{
	return TopazSc_MMU_HWSetup(ui32TCoreReg);
}

IMG_VOID MMDeviceMemoryDeInitialise(IMG_VOID)
{
	if(hMMUTemplate)	
		TALMMU_DevMemTemplateDestroy(hMMUTemplate);
}

IMG_BOOL TOPAZKM_MMUFlushMMUTableCache()
{
	TopazSc_MMU_FlushCache();

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: MMAllocateHeapDeviceMemory

 PURPOSE	: Allocate device memory using the TAL (from a specified heap, if using the MMU)

 PARAMETERS	:	ui32Size				Size of memory to allocate in bytes
				ui32Alignment			Alignment requirements of allocation
				ui32Heap				MMU heap from which to allocate
				bSaveRestore			Save surface on power transitions
				pvMemInfo				Location to return allocation handle

 RETURNS	: IMG_TRUE - successful, IMG_FALSE - failed

 Notes		:
*****************************************************************************/
IMG_BOOL TOPAZKM_MMUMAllocateHeapDeviceMemory(
	IMG_UINT32	ui32Size,
	IMG_UINT32	ui32Alignment,
	IMG_UINT32  ui32Heap,
	IMG_BOOL	bSaveRestore,
	MEMORY_INFO	*ppMemInfo,
	IMG_BOOL tileSensetive)
{
	MEMORY_INFO		*pMemoryInfo;
	IMG_BOOL		result;
	IMG_RESULT		out;

	/* Allocate memory handle */
	pMemoryInfo = (MEMORY_INFO *)IMG_MALLOC(sizeof(MEMORY_INFO));
	IMG_ASSERT(pMemoryInfo != IMG_NULL);
	if(pMemoryInfo == IMG_NULL)
		return(IMG_FALSE);
	IMG_MEMSET(pMemoryInfo, 0, sizeof(*pMemoryInfo));

	SYSOSKM_CopyFromUser(pMemoryInfo, ppMemInfo, sizeof(MEMORY_INFO));
	result = allocateMemoryHelper(ui32Size, ui32Alignment, ui32Heap, bSaveRestore, pMemoryInfo, tileSensetive);
	IMG_ASSERT(result == IMG_TRUE);
	if(result != IMG_TRUE)
	{
		IMG_FREE(pMemoryInfo);
		return IMG_FALSE;
	}

	out = SYSOSKM_CopyToUser(ppMemInfo, pMemoryInfo, sizeof(MEMORY_INFO));
	IMG_ASSERT(out == IMG_SUCCESS);
	IMG_FREE(pMemoryInfo);

	return result;
}

IMG_VOID _free_Memory(IMG_VOID *params)
{
	MEMORY_INFO *pMemInfo = (MEMORY_INFO *)params;
	freeMemoryHelper(pMemInfo);
	IMG_FREE(pMemInfo);
}


IMG_BOOL TOPAZKM_StreamMMUMAllocateHeapDeviceMemory(
	IMG_UINT32	ui32StreamId,
	IMG_UINT32	ui32Size,
	IMG_UINT32	ui32Alignment,
	IMG_UINT32  ui32Heap,
	IMG_BOOL	bSaveRestore,
	MEMORY_INFO	*ppMemInfo,
	IMG_BOOL tileSensetive)
{
	IMG_RESULT out;
	IMG_BOOL result;
	MEMORY_INFO *pMemoryInfo;
	IMG_COMM_SOCKET *pSocket;

	out = RMAN_GetResource(ui32StreamId, RMAN_SOCKETS_ID, (IMG_VOID **)&pSocket, IMG_NULL);
	IMG_ASSERT(out == IMG_SUCCESS);
	if (out != IMG_SUCCESS)
		return IMG_FALSE;

	/* Allocate memory handle */
	pMemoryInfo = (MEMORY_INFO *)IMG_MALLOC(sizeof(MEMORY_INFO));
	IMG_ASSERT(pMemoryInfo != IMG_NULL);
	if(pMemoryInfo == IMG_NULL)
		return(IMG_FALSE);
	IMG_MEMSET(pMemoryInfo, 0, sizeof(*pMemoryInfo));

	SYSOSKM_CopyFromUser(pMemoryInfo, ppMemInfo, sizeof(MEMORY_INFO));
	result = allocateMemoryHelper(ui32Size, ui32Alignment, ui32Heap, bSaveRestore, pMemoryInfo, tileSensetive);
	IMG_ASSERT(result == IMG_TRUE);
	if(result != IMG_TRUE) {
		IMG_FREE(pMemoryInfo);
		return IMG_FALSE;
	}

	out = RMAN_RegisterResource(pSocket->hResBHandle, RMAN_BUFFERS_ID, _free_Memory, (IMG_VOID *)pMemoryInfo, IMG_NULL, &pMemoryInfo->bufferId);
	IMG_ASSERT(out == IMG_SUCCESS);
	if(out != IMG_SUCCESS)
		return IMG_FALSE;

	out = SYSOSKM_CopyToUser(ppMemInfo, pMemoryInfo, sizeof(MEMORY_INFO));
	IMG_ASSERT(out == IMG_SUCCESS);

	return result;
}

/*****************************************************************************
 FUNCTION	: MMDeviceMemWriteDeviceMemRef

 PURPOSE	: Abstracts old TAL call 'TAL_DeviceMemWriteDeviceMemRefWithBitPattern' so host isn't calling TAL directly.
			  Also allows behind the scenes substitution of TAL access with TALMMU access

 PARAMETERS	:

 RETURNS	:

 Notes		:
*****************************************************************************/
IMG_VOID WriteMemRefToMemRef(
	IMG_HANDLE                      ui32MemSpaceId,
    IMG_HANDLE                      hDeviceMem,
    IMG_UINT32                      ui32Offset,
    IMG_UINT32                      ui32ManglerFuncIdExt,
    IMG_HANDLE                      hRefDeviceMem,
    IMG_UINT32                      ui32RefOffset)
{
	writeMemRefToMemRef(ui32MemSpaceId, hDeviceMem, ui32Offset, ui32ManglerFuncIdExt, hRefDeviceMem, ui32RefOffset);
}


/* Exported API through bridging */

IMG_BOOL TOPAZKM_MapExternal(IMG_UINT32 ui32BufLen, IMG_VOID *pvUM, IMG_HANDLE pallocHandle, IMG_UINT32 ui32Heap, IMG_UINT32 ui32Alignment, MEMORY_INFO *memInfo)
{
	IMG_RESULT		result;
	IMG_HANDLE		hTemp;
	MEMORY_INFO* pMemoryInfo;
	IMG_HANDLE hDevMemHeap;

	pMemoryInfo = (MEMORY_INFO *)IMG_MALLOC(sizeof(MEMORY_INFO));
	IMG_ASSERT(pMemoryInfo != IMG_NULL);
	if(pMemoryInfo == IMG_NULL)
		return IMG_FALSE;
	IMG_MEMSET(pMemoryInfo, 0, sizeof(*pMemoryInfo));

	result = TALMMU_GetHeapHandle(asMMU_HeapInfo[ui32Heap].ui32HeapId, hTopazScMMUContext.topazsc_mmu_context, &hDevMemHeap);
	IMG_ASSERT(result == IMG_SUCCESS);
	if(result != IMG_SUCCESS)
		goto map_failed;

	result = TALMMU_DevMemMapExtMem(hTopazScMMUContext.topazsc_mmu_context, hDevMemHeap, ui32BufLen, ui32Alignment, (void*)pvUM, pallocHandle, &hTemp);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
		goto map_failed;

	pMemoryInfo->pvLinAddress = (IMG_VOID *)pvUM;
	pMemoryInfo->hShadowMem = hTemp;
	pMemoryInfo->ui32DevPhysAddr = 0;

	result = SYSOSKM_CopyToUser(memInfo, pMemoryInfo, sizeof(MEMORY_INFO));
	IMG_ASSERT(result == IMG_SUCCESS);

	IMG_FREE(pMemoryInfo);  // Free up the temporary memory
	return IMG_TRUE;

map_failed:
	IMG_FREE(pMemoryInfo);
	return IMG_FALSE;
}

IMG_BOOL TOPAZKM_UnMapExternal(MEMORY_INFO *memInfo)
{
	IMG_RESULT result;
	MEMORY_INFO* pMemoryInfo;

	pMemoryInfo = (MEMORY_INFO *)IMG_MALLOC(sizeof(MEMORY_INFO));
	IMG_ASSERT(pMemoryInfo != IMG_NULL);
	if(pMemoryInfo == IMG_NULL)
		return IMG_FALSE;
	IMG_MEMSET(pMemoryInfo, 0, sizeof(*pMemoryInfo));

	result = SYSOSKM_CopyFromUser(pMemoryInfo, memInfo, sizeof(MEMORY_INFO));
	IMG_ASSERT(result == IMG_SUCCESS);
	if(result != IMG_SUCCESS)
		goto umap_failed;

	result = TALMMU_DevMemUnmap(pMemoryInfo->hShadowMem);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
		goto umap_failed;

	IMG_FREE(pMemoryInfo); // Free up the temporary memory
	return IMG_TRUE;

umap_failed:
	IMG_FREE(pMemoryInfo);
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: MMCopyTiledBuffer

 PURPOSE	: Perform tile or detile operation between host view of a buffer and user mode memory

 PARAMETERS	:	pvMemInfo		Allocation handle of tiled memory
				 ui32Offset		Byte offset from shadow
				 ui32Size		Number of bytes to transfer
				 pcBuffer		User mode buffer containing untiled data
				 bToMemory		Direction of transfer (IMG_TRUE for tiling, IMG_FALSE for detiling)

 RETURNS	: IMG_TRUE - successful, IMG_FALSE - failed

 Notes		:
*****************************************************************************/
IMG_BOOL TOPAZKM_MMCopyTiledBuffer(struct MEMORY_INFO_TAG *pMemoryInfo, IMG_CHAR *pcBuffer, IMG_UINT32 ui32Size, IMG_UINT32 ui32Offset, IMG_BOOL bToMemory)
{
	IMG_RESULT result;
	struct MEMORY_INFO_TAG *memInfo;
	IMG_CHAR *buffer;


	buffer = IMG_MALLOC(ui32Size);
	if(!buffer)
		return IMG_FALSE;

	memInfo = IMG_MALLOC(sizeof(*memInfo));
	if(!memInfo)
	{
		IMG_FREE(buffer);
		return IMG_FALSE;
	}

	result = SYSOSKM_CopyFromUser(buffer, pcBuffer, ui32Size);
	IMG_ASSERT(result == IMG_SUCCESS);
	if(result != IMG_SUCCESS)
		goto fail;

	result = SYSOSKM_CopyFromUser(memInfo, pMemoryInfo, sizeof(*memInfo));
	IMG_ASSERT(result == IMG_SUCCESS);
	if(result != IMG_SUCCESS)
		goto fail;

	if(TALMMU_CopyTileBuffer(pMemoryInfo->hShadowMem, ui32Offset, ui32Size, buffer, bToMemory) != IMG_SUCCESS)
		goto fail;

	IMG_FREE(buffer);
	IMG_FREE(memInfo);
	return IMG_TRUE;

fail:
	IMG_FREE(buffer);
	IMG_FREE(memInfo);
	return IMG_FALSE;
}

