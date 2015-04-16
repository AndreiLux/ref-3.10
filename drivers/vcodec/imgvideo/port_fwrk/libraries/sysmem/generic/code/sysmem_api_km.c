/*!
 *****************************************************************************
 *
 * @file       sysmem_api_km.c
 *
 * This file contains the System Memory Kernel Mode API.
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

#include "img_defs.h"
#include "sysmem_api_km.h"
#include "sysmem_utils.h"


/*!
******************************************************************************

 @Function				SYSMEMKM_Initialise

******************************************************************************/
IMG_RESULT SYSMEMKM_Initialise(IMG_VOID)
{
	return SYSMEMU_Initialise();
}


/*!
******************************************************************************

 @Function				SYSMEMKM_Deinitialise

******************************************************************************/
IMG_VOID SYSMEMKM_Deinitialise(IMG_VOID)
{
	SYSMEMU_Deinitialise();
}


/*!
******************************************************************************

 @Function				SYSMEMKM_AllocPages

******************************************************************************/
IMG_RESULT SYSMEMKM_AllocPages(
	IMG_UINT32				ui32Size,
	SYS_eMemAttrib			eMemAttrib,
	SYS_eMemPool			eMemPool,
    IMG_UINT64 **   ppaPhysAddr,
	IMG_HANDLE *			phPagesHandle,
	IMG_VOID **				ppvCpuKmAddr
)
{
	SYSMEMU_sPages *			psPages;

	/* Allocate page structure...*/
	psPages = SYSMEMU_AllocPages(ui32Size, eMemAttrib, eMemPool);
	IMG_ASSERT(psPages != IMG_NULL);
	if (psPages == IMG_NULL)
	{
		return IMG_ERROR_OUT_OF_MEMORY;
	}

	/* Allocate kernel mode memory...*/
	psPages->pvCpuKmBaseAddr = IMG_MALLOC(ui32Size + SYS_PAGE_SIZE - 1);
	IMG_ASSERT(psPages->pvCpuKmBaseAddr != IMG_NULL);
	if (psPages->pvCpuKmBaseAddr == IMG_NULL)
	{
		return IMG_ERROR_OUT_OF_MEMORY;
	}
	psPages->pvCpuKmAddr = (IMG_VOID*)((((IMG_UINT64)psPages->pvCpuKmBaseAddr) + SYS_PAGE_SIZE - 1) & ~(SYS_PAGE_SIZE -1));

	psPages->bIsContiguous = IMG_TRUE;

	/* Return the address of the control block...*/
	*phPagesHandle = psPages;


	/* Return the kerel mode address of the allocation...*/
	*ppvCpuKmAddr = psPages->pvCpuKmAddr;

    *ppaPhysAddr = psPages->paPhysAddr; 
	return IMG_SUCCESS;
}




/*!
******************************************************************************

 @Function				SYSMEMKM_ImportPages

******************************************************************************/
IMG_RESULT SYSMEMKM_ImportPages(
	IMG_UINT32				ui32Size,
	IMG_HANDLE				hExtImportHandle,
	IMG_HANDLE *			phPagesHandle,
	IMG_VOID **				ppvCpuKmAddr			       
)
{
	SYSMEMU_sPages *		psPages = hExtImportHandle;
	SYSMEMU_sPages *		psNewPages;

	/**** In this implemantion we are importing buffers that we have allocated. */
	/* Allocate a new memory structure...*/
	psNewPages = IMG_MALLOC(sizeof(*psNewPages));
	IMG_ASSERT(psNewPages != IMG_NULL);
	if (psNewPages == IMG_NULL)
	{
		return IMG_ERROR_OUT_OF_MEMORY;
	}

	/* Take a copy of the orignal page allocation. */
	*psNewPages = *psPages;

	/* Set imported. */
	psNewPages->bImported = IMG_TRUE;

	/* Return the address of the control block...*/
	*phPagesHandle = psNewPages;

	/* Return the kernel mode address of the allocation...*/
	*ppvCpuKmAddr = psNewPages->pvCpuKmAddr;

	return IMG_SUCCESS;
}


/*!
******************************************************************************

 @Function              SYSMEMKM_ImportExternalPages

******************************************************************************/
IMG_RESULT SYSMEMKM_ImportExternalPages(
    IMG_UINT32      ui32Size,
    SYS_eMemAttrib  eMemAttrib,
    SYS_eMemPool    eMemPool,
    IMG_HANDLE *    phPagesHandle,
    IMG_VOID *      pvCpuKmAddr,
    IMG_UINT64      ui64PhyAddr
)
{
	IMG_ASSERT(IMG_FALSE);				/* Mapping is not supported via this implementation of the SYSMEM API. */

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				SYSMEMKM_GetCpuKmAddr

******************************************************************************/
static IMG_RESULT GetCpuKmAddr(
	SYSMEM_Heap *			heap,
	IMG_VOID **				ppvCpuKmAddr,
	IMG_HANDLE 				hPagesHandle
)
{
	IMG_ASSERT(IMG_FALSE);				/* Mapping is not supported via this implementation of the SYSMEM API. */

	/* Return success...*/
	return IMG_SUCCESS;
}



/*!
******************************************************************************

 @Function				SYSMEMKM_IsContiguous

 ******************************************************************************/
IMG_BOOL SYSMEMKM_IsContiguous(
	IMG_HANDLE		hPagesHandle
)
{
	SYSMEMU_sPages *	psPages = hPagesHandle;

	IMG_ASSERT(gSysMemInitialised);

	return psPages->bIsContiguous;
}


/*!
******************************************************************************

 @Function				SYSMEMKM_FreePages

******************************************************************************/
IMG_VOID SYSMEMKM_FreePages(
	IMG_HANDLE				hPagesHandle
)
{
	SYSMEMU_sPages *			psPages = hPagesHandle;

	IMG_ASSERT(gSysMemInitialised);

	/* If mapping then free on the copy of the page structure. */
	if (psPages->bImported)
	{
		IMG_FREE(psPages);
		return;
	}

	IMG_ASSERT((!psPages->bMappingOnly));			/* Mapping is not supported via this implementation of the SYSMEM API. */

	/* Free the kernel mode memory...*/
	IMG_FREE(psPages->pvCpuKmBaseAddr);

	/* Free the control block...*/
	SYSMEMU_FreePages(psPages);
}

/*!
******************************************************************************

 @Function				SYSMEMKM_MemoryCpuToDevice

******************************************************************************/
IMG_VOID SYSMEMKM_MemoryCpuToDevice(
	IMG_HANDLE		hPagesHandle
)
{
    return;
}
IMGVIDEO_EXPORT_SYMBOL(SYSMEMKM_MemoryCpuToDevice)
/*!
******************************************************************************

 @Function				SYSMEMKM_MemoryDeviceToCpu

******************************************************************************/
IMG_VOID SYSMEMKM_MemoryDeviceToCpu(
	IMG_HANDLE		hPagesHandle
)
{
    return;
}
IMGVIDEO_EXPORT_SYMBOL(SYSMEMKM_MemoryDeviceToCpu)
