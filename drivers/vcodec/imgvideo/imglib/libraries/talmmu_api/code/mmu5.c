/*!
 *****************************************************************************
 *
 * @file	   mmu5.c
 *
 * This header file contains virtual memory/address access functions
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

#include "mmu.h"
#include "mmu_definition.h"

#include <img_types.h>
#include <img_defs.h>
#include <img_errors.h>

/*! The number of tiled regions */
#define MMU_NUMBER_OF_TILED_REGIONS (10)

#ifdef MMU_DEBUG
#include "gzip_fileio.h"
#ifndef MMU_ERROR_FILE
#define MMU_ERROR_FILE "mmu_error.txt"
#endif
#endif

typedef enum
{
	MMU_BUS_WIDTH_32,
	MMU_BUS_WIDTH_36,
	MMU_BUS_WIDTH_40,
	MMU_BUS_WIDTH_40VID,

	// Placeholder
	MMU_NO_BUS_WIDTHS
}MMU_BusWidths;

typedef enum
{
	MMU_PAGE_SIZE_4K,
	MMU_PAGE_SIZE_16K,
	MMU_PAGE_SIZE_64K,
	MMU_PAGE_SIZE_256K,
	MMU_PAGE_SIZE_1M,
	MMU_PAGE_SIZE_4M,

	// Placeholder
	MMU_NO_PAGE_SIZES
}MMU_PAgeSizes;

/* Values used in MMU page table access... */
#define  MMU_PAGE_DIRECTORY_ENTRY_SHIFT        (22)
#define  MMU_PAGE_DIRECTORY_ENTRY_VALID_MASK   (0x00000001)
#define  MMU_PDE_PAGE_SIZE_MASK				   (0x0000000E)
#define  MMU_PDE_PAGE_SIZE_SHIFT			   (0x01)
#define  MMU_DEFAULT_PAGE_SIZE                 (MMU_4K_PAGE_SIZE)
#define  MMU_DEFAULT_PAGE_SIZE_ID              (0)
#define  MMU_PAGE_TABLE_ENTRY_VALID_MASK       (0x00000001)
#define  MMU_OFFSET_WITHIN_PAGE_MASK           (0x00000FFF)


/*!
******************************************************************************
 Defines used in address tiling...
******************************************************************************/
#define  MMU_TILE_STRIDE                    (256)
#define  MMU_36BIT_TILE_STRIDE              (512)
#define  MMU_TILED_BOTTOM_BITS_MASK         (0x000000FF)
#define  MMU_TILED_BOTTOM_BITS_SHIFT        (8)
#define  MMU_36BIT_TILED_BOTTOM_BITS_MASK	(0x000001FF)
#define  MMU_36BIT_TILED_BOTTOM_BITS_SHIFT  (9)
#define  MMU_TILED_YROW_TILE_MASK			(0x0000000F)
#define  MMU_TILED_YROW_TILE_SHIFT			(4)
#define  MMU_36BIT_TILED_YROW_TILE_MASK		(0x00000007)
#define  MMU_36BIT_TILED_YROW_TILE_SHIFT	(3)

/*!
******************************************************************************
  This structure contains information about a tiled region.
******************************************************************************/
typedef struct
{
	IMG_UINT64      ui64DevVirtAddr;    /*!< The device virtual address of the start of the tiled region  */
    IMG_UINT64      ui64Size;           /*!< The size of the region (in bytes)      */
    IMG_UINT32      ui32XTileStride;    /*!< The tile stride                        */

} MMU_sTiledRegionCB;


/*!
******************************************************************************
  This structure contains information about an MMU context.
******************************************************************************/
typedef struct{
	MMU_pfnReadMem			pfnReadPage;
	MMU_pfnReadMem			pfnReadLookup;
	IMG_UINT32				ui32MMUType;
	IMG_UINT32				ui32BusWidth;
    IMG_UINT64				ui64PCAddress;
	/*! IMG_TRUE if tiled regions defined for this memory space */
    IMG_BOOL                bTiledRegionDefined;
	/*! Tile Memory Regions associated with this memory space   */
	MMU_sTiledRegionCB      asTiledRegionCB[MMU_NUMBER_OF_TILED_REGIONS];
} MMU_sContextCB;

/* Structure containing MMU Page Size information */
typedef struct
{
	IMG_UINT32 ui32PageSize;
	IMG_UINT64 ui64VaddrPageMask;
	IMG_UINT32 ui32VaddrPTShift;
	/*IMG_UINT32 ui32PDirPTabMask;
	IMG_UINT32 ui32PagePhysMask;*/
} MMU_sMMUPageSizeInfo;

static MMU_sMMUPageSizeInfo gasMMUPageSizeLookup[MMU_NO_PAGE_SIZES] = 
{										//-3 is to allow for 64bit entry
	{MMU_PAGESIZE(4K), MMU_PAGEMASK(4K), MMU_PAGESHIFT(4K)},
	{MMU_PAGESIZE(16K), MMU_PAGEMASK(16K), MMU_PAGESHIFT(16K)},
	{MMU_PAGESIZE(64K), MMU_PAGEMASK(64K), MMU_PAGESHIFT(64K)},
	{MMU_PAGESIZE(256K), MMU_PAGEMASK(256K), MMU_PAGESHIFT(256K)},
	{MMU_PAGESIZE(1M), MMU_PAGEMASK(1M), MMU_PAGESHIFT(1M)},
	{MMU_PAGESIZE(4M), MMU_PAGEMASK(4M), MMU_PAGESHIFT(4M)}
};

typedef struct
{
	IMG_UINT32 ui32BusWidth;
} MMU_sMMUTypeInfo;

static MMU_sMMUTypeInfo gasMMUTypesLookup[MMU_NO_TYPES] = 
{
	{MMU_BUS_WIDTH_32},
	{MMU_BUS_WIDTH_32},
	{MMU_BUS_WIDTH_32},
	{MMU_BUS_WIDTH_36},
	{MMU_BUS_WIDTH_40VID},
    {MMU_BUS_WIDTH_40VID},
};

typedef struct
{
	IMG_UINT32 ui32BusWidth;				//<! The bus width in bits
	IMG_UINT64 ui64PageSizeIdMask;			//<! Mask for the Page Size Id from the PD
	IMG_UINT32 ui32PageSizeIdShift;			//<! Shift for the Page Size Id from the PD
	IMG_UINT64 ui64Vaddr_PCMask;			//<! Mask for the PC in the Virtual Address
	IMG_UINT32 ui32Vaddr_PCShift;			//<! Shift for the PC in the Virtual Address
	IMG_UINT64 ui64Vaddr_PDMask;			//<! Mask for the PD in the Virtual Address
	IMG_UINT32 ui32Vaddr_PDShift;			//<! Mask for the PD in the Virtual Address
	IMG_UINT64 ui64Vaddr_PTandPageMask;		//<! Mask for the PT and Page in the Virtual Address
	IMG_UINT32 ui32Vaddr_PTShift;			//<! The shift required for a PTE

	IMG_UINT64 ui64PDE_PTMask;				//!< Mask for the PT in the Page Directory Entry
	IMG_UINT32 ui32PDE_PTShift;				//!< Shift for the PT in the Page Directory Entry
	IMG_UINT64 ui64PTE_PageMask;			//!< Mask for the Page in the Page Table Entry
	IMG_UINT32 ui32PTE_PageShift;			//!< Shift for the Page in the Page Table Entry

	IMG_UINT32 ui32PCEntrySize;				//!< Size in bytes of the PC Entry
	IMG_UINT32 ui32PDEntrySize;				//!< Size in bytes of the PD Entry
	IMG_UINT32 ui32PTEntrySize;				//!< Size in bytes of the PT Entry
	

} MMU_sMMUBusWidthInfo;

static MMU_sMMUBusWidthInfo gasMMUBusWidthsLookup[MMU_NO_BUS_WIDTHS] =
{
	{32, MMU_PD_PAGESIZEID_MASK, MMU_PD_PAGESIZEID_SHIFT(32), MMU_VA_PC_MASK(32), MMU_VA_PC_SHIFT(32), MMU_VA_PD_MASK(32), MMU_VA_PD_SHIFT(32), MMU_VA_PT_PAGE_MASK(32), MMU_VA_PT_SHIFT(32), MMU_PD_PT_MASK(32), MMU_PD_PT_SHIFT(32), MMU_PTE_PAGE_MASK(32), MMU_PTE_PAGE_SHIFT(32), MMU_PCE_SIZE(32), MMU_PDE_SIZE(32), MMU_PTE_SIZE(32)},
	{36, MMU_PD_PAGESIZEID_MASK, MMU_PD_PAGESIZEID_SHIFT(36), MMU_VA_PC_MASK(36), MMU_VA_PC_SHIFT(36), MMU_VA_PD_MASK(36), MMU_VA_PD_SHIFT(36), MMU_VA_PT_PAGE_MASK(36), MMU_VA_PT_SHIFT(36), MMU_PD_PT_MASK(36), MMU_PD_PT_SHIFT(36), MMU_PTE_PAGE_MASK(36), MMU_PTE_PAGE_SHIFT(36), MMU_PCE_SIZE(36), MMU_PDE_SIZE(36), MMU_PTE_SIZE(36)},
	{40, MMU_PD_PAGESIZEID_MASK, MMU_PD_PAGESIZEID_SHIFT(40), MMU_VA_PC_MASK(40), MMU_VA_PC_SHIFT(40), MMU_VA_PD_MASK(40), MMU_VA_PD_SHIFT(40), MMU_VA_PT_PAGE_MASK(40), MMU_VA_PT_SHIFT(40), MMU_PD_PT_MASK(40), MMU_PD_PT_SHIFT(40), MMU_PTE_PAGE_MASK(40), MMU_PTE_PAGE_SHIFT(40), MMU_PCE_SIZE(40), MMU_PDE_SIZE(40), MMU_PTE_SIZE(40)},
	{40, MMU_PD_PAGESIZEID_MASK, MMU_PD_PAGESIZEID_SHIFT(40VID), MMU_VA_PC_MASK(40VID), MMU_VA_PC_SHIFT(40VID), MMU_VA_PD_MASK(40VID), MMU_VA_PD_SHIFT(40VID), MMU_VA_PT_PAGE_MASK(40VID), MMU_VA_PT_SHIFT(40VID), MMU_PD_PT_MASK(40VID), MMU_PD_PT_SHIFT(40VID), MMU_PTE_PAGE_MASK(40VID), MMU_PTE_PAGE_SHIFT(40VID), MMU_PCE_SIZE(40VID), MMU_PDE_SIZE(40VID), MMU_PTE_SIZE(40VID)}
};
#define THIS_BUS_WIDTH gasMMUBusWidthsLookup[psContextCB->ui32BusWidth]


typedef struct MMU_sTilingParam_tag
{
	IMG_UINT32 ui32TileStride;
	IMG_UINT32 ui32BottomBitsMask;
	IMG_UINT32 ui32BottomBitsShift;
	IMG_UINT32 ui32YRowMask;
	IMG_UINT32 ui32YRowShift;
}MMU_sTilingParam;


/* Lookup table for Tiling Parameters */
static MMU_sTilingParam gasTilingParamLookup[MMU_NO_TYPES] = 
{
	{0,0,0,0,0},
	{MMU_TILE_STRIDE, MMU_TILED_BOTTOM_BITS_MASK, MMU_TILED_BOTTOM_BITS_SHIFT, MMU_TILED_YROW_TILE_MASK, MMU_TILED_YROW_TILE_SHIFT},
	{MMU_TILE_STRIDE, MMU_TILED_BOTTOM_BITS_MASK, MMU_TILED_BOTTOM_BITS_SHIFT, MMU_TILED_YROW_TILE_MASK, MMU_TILED_YROW_TILE_SHIFT},
	{MMU_36BIT_TILE_STRIDE, MMU_36BIT_TILED_BOTTOM_BITS_MASK, MMU_36BIT_TILED_BOTTOM_BITS_SHIFT, MMU_36BIT_TILED_YROW_TILE_MASK, MMU_36BIT_TILED_YROW_TILE_SHIFT},
	{MMU_36BIT_TILE_STRIDE, MMU_36BIT_TILED_BOTTOM_BITS_MASK, MMU_36BIT_TILED_BOTTOM_BITS_SHIFT, MMU_36BIT_TILED_YROW_TILE_MASK, MMU_36BIT_TILED_YROW_TILE_SHIFT},
	{MMU_TILE_STRIDE, MMU_TILED_BOTTOM_BITS_MASK, MMU_TILED_BOTTOM_BITS_SHIFT, MMU_TILED_YROW_TILE_MASK, MMU_TILED_YROW_TILE_SHIFT}
};

static IMG_BOOL			gbMMUInitialised = IMG_FALSE;
static MMU_sContextCB	gasContextCBs[MMU_NUMBER_OF_CONTEXTS];
static IMG_UINT32		gui32ContextsInUse = 0;
							

/*!
******************************************************************************

 @Function				MMU_Initialise

******************************************************************************/
extern IMG_VOID MMU_Initialise ()
{
	if (gbMMUInitialised == IMG_FALSE)
	{
		memset(gasContextCBs, 0, MMU_NUMBER_OF_CONTEXTS * sizeof(MMU_sContextCB));
		gbMMUInitialised = IMG_TRUE;
	}
}

/*!
******************************************************************************

 @Function				mmu_GetContext

******************************************************************************/
MMU_sContextCB* mmu_GetContext(IMG_UINT32 ui32MMUId)
{
	MMU_sContextCB *psContextCB;
	/* Check MMU Id is Valid */
	IMG_ASSERT(ui32MMUId < MMU_NUMBER_OF_CONTEXTS);
	IMG_ASSERT(gbMMUInitialised);

	psContextCB = &gasContextCBs[ui32MMUId];
	if(psContextCB->ui32MMUType == MMU_TYPE_NONE)
		return NULL;
	return psContextCB;
}


/*!
******************************************************************************

 @Function				mmu_GetPCEntry

******************************************************************************/
IMG_UINT32 mmu_GetPCEntry(IMG_UINT32 ui32MMUId, IMG_UINT64 ui64VirtAddr, IMG_PUINT64 pui64PCEntry)
{

	MMU_sContextCB *psContextCB = mmu_GetContext(ui32MMUId);
	IMG_UINT64 ui64PCAddr;

	// Currently only the 40bit MMU has PC
	switch (psContextCB->ui32MMUType)
	{ 
	case MMU_TYPE_VP_40BIT_ADDR:
		break;
	default:
		*pui64PCEntry = 0;
		return IMG_SUCCESS;
	}

	/* Get PDT Entry and Validate */
	ui64PCAddr = MMU_GetPCEntryAddress(ui32MMUId, ui64VirtAddr);

	if (psContextCB->pfnReadLookup(ui64PCAddr, THIS_BUS_WIDTH.ui32PCEntrySize, (IMG_PUINT8)pui64PCEntry) != IMG_SUCCESS)
	{
		//printf("WARNING: Cannot read from page directory, address 0x%" IMG_INT64PRFX "X\n", ui64PCAddr);
		return IMG_ERROR_INVALID_PARAMETERS;
	}

	if ((*pui64PCEntry & MMU_VALID_MASK) == 0)
	{
		return IMG_ERROR_MMU_PAGE_CATALOGUE_FAULT;
	}

	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				mmu_GetPageDirEntry

******************************************************************************/
IMG_UINT32 mmu_GetPageDirEntry(IMG_UINT32 ui32MMUId, IMG_UINT64 ui64VirtAddr, IMG_UINT64 ui64PCEntry, IMG_PUINT64 pui64PageDirEntry)
{

	MMU_sContextCB *psContextCB = mmu_GetContext(ui32MMUId);
	IMG_UINT64 ui64PageDirAddr;
	

	/* Get PDT Entry and Validate */
	ui64PageDirAddr = MMU_GetPageDirEntryAddress(ui32MMUId, ui64VirtAddr, ui64PCEntry);

	if (psContextCB->pfnReadLookup(ui64PageDirAddr, THIS_BUS_WIDTH.ui32PDEntrySize, (IMG_PUINT8)pui64PageDirEntry) != IMG_SUCCESS)
	{
		printf("WARNING: Cannot read from page directory, address 0x%" IMG_INT64PRFX "X\n", ui64PageDirAddr);
		return IMG_ERROR_INVALID_PARAMETERS;
	}
	
	if (((*pui64PageDirEntry) & MMU_VALID_MASK) == 0)
	{
		return IMG_ERROR_MMU_PAGE_DIRECTORY_FAULT;
	}

	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				mmu_GetPageTableEntry

******************************************************************************/
IMG_UINT32 mmu_GetPageTableEntry(IMG_UINT32 ui32MMUId, IMG_UINT64 ui64VirtAddr, IMG_UINT64 ui64PDEntry, IMG_PUINT64 pui64PageTableEntry)
{

	MMU_sContextCB *psContextCB = mmu_GetContext(ui32MMUId);
	IMG_UINT64 ui64PageTabAddr;
	

	/* Get PDT Entry and Validate */
	ui64PageTabAddr = MMU_GetPageTableEntryAddress(ui32MMUId, ui64VirtAddr, ui64PDEntry);

	if (psContextCB->pfnReadLookup(ui64PageTabAddr, THIS_BUS_WIDTH.ui32PTEntrySize, (IMG_PUINT8)pui64PageTableEntry) != IMG_SUCCESS)
	{
		printf("WARNING: Cannot read from page table, address 0x%" IMG_INT64PRFX "X\n", ui64PageTabAddr);
		return IMG_ERROR_INVALID_PARAMETERS;
	}
	
	if ((*pui64PageTableEntry & MMU_VALID_MASK) == 0)
	{
		printf("WARNING: Invalid Page Table Entry at address 0x%" IMG_INT64PRFX "X, 0x%" IMG_INT64PRFX "X\n", ui64PageTabAddr, *pui64PageTableEntry);
		return IMG_ERROR_MMU_PAGE_TABLE_FAULT;
	}

	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				mmu_GetTileRegion

******************************************************************************/
IMG_INT32 mmu_GetTileRegion(
    IMG_UINT32				ui32MMUId,
    IMG_UINT64              ui64DevVirtAddr
)
{
    IMG_UINT32              i;
	MMU_sContextCB			*psContextCB = mmu_GetContext(ui32MMUId);

	IMG_ASSERT(gbMMUInitialised);
	IMG_ASSERT (psContextCB);

    /* If the context has tile regions...*/
    if (psContextCB->bTiledRegionDefined)
    {
        /* Loop over regions...*/
        for (i=0; i<MMU_NUMBER_OF_TILED_REGIONS; i++)
        {
            /* If region defined...*/
            if (psContextCB->asTiledRegionCB[i].ui64Size != 0)
            {
                /* If device virtual address is within a tiled region...*/
                if (
                        (ui64DevVirtAddr >= psContextCB->asTiledRegionCB[i].ui64DevVirtAddr) &&
                        (ui64DevVirtAddr < (psContextCB->asTiledRegionCB[i].ui64DevVirtAddr+psContextCB->asTiledRegionCB[i].ui64Size))
                        )
                {
                    return i;
                }
            }
        }
    }
    return -1;
}

/*!
******************************************************************************

 @Function				MMU_QuickTileAddress

******************************************************************************/
IMG_UINT64  MMU_QuickTileAddress(
    IMG_UINT32				ui32XTileStride,
	IMG_UINT32				ui32MMUType,
    IMG_UINT64              ui64DevVirtAddr
    )
{
    IMG_UINT64              ui64TiledDevVirtAddr = ui64DevVirtAddr;
    IMG_UINT64              ui64AddrBits = ui64DevVirtAddr;
    IMG_UINT32              ui32BottomBits;
    IMG_UINT32              ui32TileInX;
    IMG_UINT32              ui32YRowWithinTile;

	IMG_ASSERT(ui32MMUType < MMU_NO_TYPES);

    /* Calculate the tiled address...*/

	/* Take the bottom bits (below tiled bits) */
	ui32BottomBits = (IMG_UINT32)ui64AddrBits & gasTilingParamLookup[ui32MMUType].ui32BottomBitsMask;
	ui64AddrBits >>= gasTilingParamLookup[ui32MMUType].ui32BottomBitsShift;

	/* Take the tile in X bits (bits to move up) */
	ui32TileInX = (IMG_UINT32)ui64AddrBits & ((1 << (ui32XTileStride + 1)) - 1);
	ui64AddrBits >>= (ui32XTileStride + 1);

	/* Take the tile in Y bits (bits to move down) */
	ui32YRowWithinTile = (IMG_UINT32)ui64AddrBits & gasTilingParamLookup[ui32MMUType].ui32YRowMask;
	ui64AddrBits >>= gasTilingParamLookup[ui32MMUType].ui32YRowShift;

	/* Put it all back together */
	ui64TiledDevVirtAddr = ui64AddrBits;
	ui64TiledDevVirtAddr <<= (ui32XTileStride + 1);
	ui64TiledDevVirtAddr |= (IMG_UINT64)ui32TileInX;
	ui64TiledDevVirtAddr <<= gasTilingParamLookup[ui32MMUType].ui32YRowShift;
	ui64TiledDevVirtAddr |= (IMG_UINT64)ui32YRowWithinTile;
	ui64TiledDevVirtAddr <<= gasTilingParamLookup[ui32MMUType].ui32BottomBitsShift;
	ui64TiledDevVirtAddr |= (IMG_UINT64)ui32BottomBits;                                  

    return ui64TiledDevVirtAddr;
}

/*!
******************************************************************************

 @Function				MMU_TileAddress

******************************************************************************/
IMG_UINT64  MMU_TileAddress(
    IMG_UINT32				ui32MMUId,
	IMG_UINT64				ui64DevVirtAddr
    )
{
	MMU_sContextCB			*psContextCB = mmu_GetContext(ui32MMUId);
    IMG_UINT64              ui64TiledDevVirtAddr = ui64DevVirtAddr;
    IMG_INT32				i32TileRegion;

	IMG_ASSERT(gbMMUInitialised);
	IMG_ASSERT(psContextCB);

	i32TileRegion = mmu_GetTileRegion(ui32MMUId, ui64DevVirtAddr);
    if ( i32TileRegion >= 0 )
	{
        /* Calculate the tiled address...*/
		ui64TiledDevVirtAddr = MMU_QuickTileAddress(psContextCB->asTiledRegionCB[i32TileRegion].ui32XTileStride, psContextCB->ui32MMUType, ui64DevVirtAddr);
    }
    return ui64TiledDevVirtAddr;
}

/*!
******************************************************************************

 @Function				MMU_TileSize

******************************************************************************/
IMG_UINT32  MMU_TileSize(
    IMG_UINT32				ui32MMUId,
    IMG_UINT64              ui64DevVirtAddr
)
{
	MMU_sContextCB	*psContextCB = mmu_GetContext(ui32MMUId);
	
	IMG_ASSERT(gbMMUInitialised);
	if (psContextCB == IMG_NULL)
		return 0;

	if (MMU_IsTiled(ui32MMUId, ui64DevVirtAddr))
	{
		return gasTilingParamLookup[psContextCB->ui32MMUType].ui32TileStride;
	}
	else
	{
		return 0;
	}

}

/*!
******************************************************************************

 @Function				mmu_GetPageSizeId

******************************************************************************/
IMG_INT32 mmu_GetPageSizeId (
	IMG_UINT32						ui32MMUId,	
	IMG_UINT64						ui64VirtualAddress
)
{	
	IMG_UINT64 ui64PageDirEntry, ui64PCEntry;
	MMU_sContextCB	*psContextCB = mmu_GetContext(ui32MMUId);

	if ( mmu_GetPCEntry(ui32MMUId, ui64VirtualAddress, &ui64PCEntry)!= IMG_SUCCESS )return 0;
	if ( mmu_GetPageDirEntry(ui32MMUId, ui64VirtualAddress, ui64PCEntry, &ui64PageDirEntry)!= IMG_SUCCESS )return 0;

	// If we are using the 40bit MMU the page size is in the PD otherwise it is in the PT
	switch ( psContextCB->ui32MMUType )
	{
	case MMU_TYPE_VP_40BIT_ADDR:
	case MMU_TYPE_VARIABLE_PAGE_32BIT_ADDR:
		return (IMG_UINT32)MMU_MASK_AND_SHIFTR(ui64PageDirEntry, THIS_BUS_WIDTH.ui64PageSizeIdMask, THIS_BUS_WIDTH.ui32PageSizeIdShift);

	case MMU_TYPE_4K_PAGES_32BIT_ADDR:
	case MMU_TYPE_4K_PAGES_36BIT_ADDR:
	case MMU_TYPE_4K_PAGES_40BIT_ADDR:
		return MMU_DEFAULT_PAGE_SIZE_ID;

	default:
		IMG_ASSERT(IMG_FALSE);
	}
	return 0;
}

/*!
******************************************************************************

 @Function				mmu_GetPageSize

******************************************************************************/
IMG_UINT32 mmu_GetPageSize (
	IMG_UINT32						ui32MMUId,	
	IMG_UINT64						ui64VirtualAddress
)
{	
	IMG_UINT32 ui32PageSizeId;
	ui32PageSizeId = mmu_GetPageSizeId(ui32MMUId, ui64VirtualAddress);
	return gasMMUPageSizeLookup[ui32PageSizeId].ui32PageSize;
}

#ifdef MMU_DEBUG
#define MMU_ROW_LENGTH 32

/*!
******************************************************************************

 @Function				mmu_PrintTable

******************************************************************************/
IMG_VOID mmu_PrintTable(IMG_UINT32 ui32MMUId, IMG_UINT64 ui64PhysAddr, IMG_UINT32 ui32PageSize, IMG_UINT32 ui32EntrySize, FILE* hErrorFile)
{
	IMG_UINT32 ui32PagePos = 0, ui32Buffer;
	MMU_sContextCB *psContextCB = mmu_GetContext(ui32MMUId);

	IMG_ASSERT(gbMMUInitialised);
	if (psContextCB == IMG_NULL)
		return;

	while (ui32PagePos < ui32PageSize)
	{
		do
		{
			do
			{
				psContextCB->pfnReadLookup(ui64PhysAddr + ui32PagePos, 4, (IMG_PUINT8)&ui32Buffer);
				fprintf(hErrorFile, "%08X", ui32Buffer);
				ui32PagePos+= 4;
			}while(ui32PagePos % ui32EntrySize != 0);
			fprintf(hErrorFile, " ");
		} while (ui32PagePos % MMU_ROW_LENGTH != 0);
		fprintf(hErrorFile, "\n");
	}
}
/*!
******************************************************************************

 @Function				mmu_ErrorOutput

******************************************************************************/
IMG_VOID mmu_ErrorOutput(IMG_UINT32 ui32MMUId, IMG_UINT64 ui64VirtAddr)
{
	IMG_UINT64 ui64PCEntry = 0, ui64PageDirEntry = 0, ui64PageTabEntry = 0, ui64TiledDevVirtAddr, ui64PageDirAddress, ui64PageTabAddr, ui64PageAddr;
	MMU_sContextCB *psContextCB = mmu_GetContext(ui32MMUId);
	FILE * hErrorFile;
	IMG_RESULT rResult;

	IMG_ASSERT(gbMMUInitialised);
	if (psContextCB == IMG_NULL)
		return;

	hErrorFile = fopen(MMU_ERROR_FILE, "w");
	IMG_ASSERT(hErrorFile);
	
	fprintf(hErrorFile, "Virtual Address: 0x%" IMG_INT64PRFX "X\n", ui64VirtAddr);
	// Tile the address (if required)...
    ui64TiledDevVirtAddr = MMU_TileAddress(ui32MMUId, ui64VirtAddr);
	fprintf(hErrorFile, "Tiled Virtual Address: 0x%" IMG_INT64PRFX "X\n", ui64TiledDevVirtAddr);

	// Get the PC Entry if required
	rResult = mmu_GetPCEntry(ui32MMUId, ui64TiledDevVirtAddr, &ui64PCEntry);
	if (psContextCB->ui32MMUType == MMU_TYPE_VP_40BIT)
	{
		fprintf(hErrorFile, "Page Catalogue Address: 0x%" IMG_INT64PRFX "X\n", psContextCB->ui64PCAddress);
		if (rResult != IMG_SUCCESS )
		{
			fprintf(hErrorFile, "Cannot read from page Catalogue\n");
			fclose(hErrorFile);
			return;
		}
		else
		{
			fprintf(hErrorFile, "Page Catalogue Entry: 0x%X\n", ui64PCEntry);
			fprintf(hErrorFile, "Page Catalogue:\n");
			mmu_PrintTable(ui32MMUId, psContextCB->ui64PCAddress, mmu_GetPageSize(ui32MMUId, ui64VirtAddr), THIS_BUS_WIDTH.ui32PCEntrySize, hErrorFile);
			fprintf(hErrorFile, "\n\n");
		}

	}
	
	ui64PageDirAddress = MMU_GetPageDirEntryAddress(ui32MMUId, ui64TiledDevVirtAddr, ui64PCEntry);
	fprintf(hErrorFile, "Page Directory Address: 0x%" IMG_INT64PRFX "X\n", ui64PageDirAddress);
	// Get the Page Directory Entry
	if (psContextCB->pfnReadLookup(ui64PageDirAddress, THIS_BUS_WIDTH.ui32PDEntrySize, (IMG_PUINT8)&ui64PageDirEntry) != IMG_SUCCESS)
	{
		fprintf(hErrorFile, "Cannot read from page Directory\n");
		fclose(hErrorFile);
		return;
	}
	else
	{
		fprintf(hErrorFile, "Page Directory Entry: 0x%X\n", ui64PageDirEntry);
		fprintf(hErrorFile, "Page Directory:\n");
		mmu_PrintTable(ui32MMUId, ui64PageDirAddress & ~0xFFF, mmu_GetPageSize(ui32MMUId, ui64VirtAddr), THIS_BUS_WIDTH.ui32PDEntrySize, hErrorFile);
		fprintf(hErrorFile, "\n\n");
	}

	ui64PageTabAddr = MMU_GetPageTableEntryAddress(ui32MMUId, ui64VirtAddr, ui64PageDirEntry);
	fprintf(hErrorFile, "Page Table Address: 0x%" IMG_INT64PRFX "X\n", ui64PageTabAddr);

	// Get Page Table Entry
	if (psContextCB->pfnReadLookup(ui64PageTabAddr, THIS_BUS_WIDTH.ui32PTEntrySize, (IMG_PUINT8)&ui64PageTabEntry) != IMG_SUCCESS)
	{
		fprintf(hErrorFile, "Cannot read from page Table\n");
		fclose(hErrorFile);
		return;
	}
	else
	{
		fprintf(hErrorFile, "Page Table Entry: 0x%X\n", ui64PageTabEntry);
		fprintf(hErrorFile, "Page Table:\n");
		mmu_PrintTable(ui32MMUId, ui64PageTabAddr & ~0xFFF, mmu_GetPageSize(ui32MMUId, ui64VirtAddr), THIS_BUS_WIDTH.ui32PTEntrySize, hErrorFile);
		fprintf(hErrorFile, "\n\n");
	}

	ui64PageAddr = MMU_GetPageAddress(ui32MMUId, ui64TiledDevVirtAddr, ui64PageTabEntry);

	fprintf(hErrorFile, "Page Address: 0x%X\n", ui64PageAddr);
	fclose(hErrorFile);
}
#endif //MMU_DEBUG

/*!
******************************************************************************

 @Function				MMU_ReadVmem

******************************************************************************/
IMG_RESULT MMU_ReadVmem (
	IMG_UINT64						ui64VirtAddr,
	IMG_UINT64						ui64Size,
	IMG_UINT32 *					pui32Values,
	IMG_UINT32						ui32MMUId
)
{
	IMG_RESULT		rResult, rFinalResult = IMG_SUCCESS;
	MMU_sContextCB	*psContextCB = mmu_GetContext(ui32MMUId);
	IMG_UINT64		ui64Address;
    IMG_UINT64		ui64SizeRemaining = ui64Size;
	IMG_UINT64		ui64CurDevVirtAddr;
	IMG_PUINT8		pui8Buffer = (IMG_PUINT8)pui32Values;
	IMG_BOOL		bTiled;
	IMG_UINT32		ui32TileStride, ui32ReadSize, ui32Offset, ui32BytesToProcess;

	IMG_ASSERT(gbMMUInitialised);
	if (psContextCB == IMG_NULL)
		return IMG_ERROR_INVALID_ID;

	ui32TileStride = MMU_TileSize(ui32MMUId, ui64VirtAddr);
	// Set readsize to size of page/tile
    if (ui32TileStride > 0)
    {
        bTiled = IMG_TRUE;
        IMG_ASSERT(MMU_IsTiled(ui32MMUId, (ui64VirtAddr+ui64Size-1)));
		ui32ReadSize = ui32TileStride;
    }
    else
	{
        bTiled = IMG_FALSE;
		ui32ReadSize = mmu_GetPageSize(ui32MMUId, ui64VirtAddr);
    }

	ui64CurDevVirtAddr = ui64VirtAddr;

	 /* Loop over virtual address space...*/
	while (ui64SizeRemaining > 0)
    {
		// With the variable page size MMU, if we move to a new page table, the page size may change so the size is checked at this boundry
		if ((bTiled == IMG_FALSE) && (psContextCB->ui32MMUType ==  MMU_TYPE_VARIABLE_PAGE_32BIT_ADDR) && ((ui64CurDevVirtAddr & THIS_BUS_WIDTH.ui64Vaddr_PTandPageMask) == 0))
			ui32ReadSize = mmu_GetPageSize(ui32MMUId, ui64VirtAddr);
		
        IMG_ASSERT(ui32ReadSize != 0);

		// Calculate offset from start of page/tile
		ui32Offset = (IMG_UINT32)(ui64CurDevVirtAddr % ui32ReadSize);

		/* Calculate size to end of page/tile...*/
		ui32BytesToProcess = ui32ReadSize - ui32Offset;

        /* check size remaining at least as big as the size scheduled...*/
        if (ui64SizeRemaining < (IMG_UINT64)ui32BytesToProcess)
        {
            /* The size is what is left to process...*/
            ui32BytesToProcess = (IMG_UINT32)ui64SizeRemaining;
        }
        
        /* Get device memory handle and offset from device virtual address and page table directory ...*/
		rResult = MMU_VmemToPhysMem(ui64CurDevVirtAddr, &ui64Address, ui32MMUId);
		if (rResult != IMG_SUCCESS)
		{
#ifdef MMU_DEBUG
			mmu_ErrorOutput(ui32MMUId, ui64CurDevVirtAddr);
#endif
			rFinalResult = rResult;
			if (psContextCB->pfnReadPage(ui64Address, ui32BytesToProcess, 0) != IMG_SUCCESS)
				return IMG_ERROR_INVALID_PARAMETERS;
		}
		else
		{
			/* Copy to host memory representing the page...*/
        	if (psContextCB->pfnReadPage(ui64Address, ui32BytesToProcess, pui8Buffer) != IMG_SUCCESS)
				return IMG_ERROR_INVALID_PARAMETERS;
		}

		pui8Buffer += ui32BytesToProcess;

        /* Update loop variables...*/
        ui64SizeRemaining  -= ui32BytesToProcess;
        ui64CurDevVirtAddr += ui32BytesToProcess;
    }
	return rFinalResult;
}

/*!
******************************************************************************

 @Function				MMU_VmemToPhysMem

******************************************************************************/
IMG_RESULT MMU_VmemToPhysMem (
	IMG_UINT64						ui64VirtAddr,
	IMG_UINT64 *					pui64Value,
	IMG_UINT32						ui32MMUId
	)
{
	IMG_UINT64 ui64PCEntry = 0, ui64PageDirEntry = 0, ui64PageTabEntry = 0, ui64TiledDevVirtAddr;
	MMU_sContextCB *psContextCB = mmu_GetContext(ui32MMUId);
	IMG_RESULT rResult;

	IMG_ASSERT(gbMMUInitialised);
	if (psContextCB == IMG_NULL)
		return IMG_ERROR_INVALID_ID;

	/* Tile the address (if required)...*/
    ui64TiledDevVirtAddr = MMU_TileAddress(ui32MMUId, ui64VirtAddr);

	// Get the PC Entry if required
	rResult = mmu_GetPCEntry(ui32MMUId, ui64TiledDevVirtAddr, &ui64PCEntry);
	if (rResult != IMG_SUCCESS ) return rResult;

	// Get the Page Directory Entry
	rResult = mmu_GetPageDirEntry(ui32MMUId, ui64TiledDevVirtAddr, ui64PCEntry, &ui64PageDirEntry);
	if (rResult != IMG_SUCCESS ) return rResult;

	// Get Page Table Entry
	rResult = mmu_GetPageTableEntry(ui32MMUId, ui64TiledDevVirtAddr, ui64PageDirEntry, &ui64PageTabEntry);
	if (rResult != IMG_SUCCESS ) return rResult;
	
	*pui64Value = MMU_GetPageAddress(ui32MMUId, ui64TiledDevVirtAddr, ui64PageTabEntry);

	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				MMU_GetPCEntryAddress

******************************************************************************/
IMG_UINT64 MMU_GetPCEntryAddress (
	IMG_UINT32				ui32MMUId,
	IMG_UINT64              ui64DevVirtAddr
)
{
	MMU_sContextCB			*psContextCB = mmu_GetContext(ui32MMUId);
	IMG_UINT64				ui64PCEntryAddr;

	IMG_ASSERT(gbMMUInitialised);
	IMG_ASSERT (psContextCB);

	// Currently only the 40bit MMU has PC
	switch (psContextCB->ui32MMUType)
	{ 
	case MMU_TYPE_VP_40BIT_ADDR:
		break;
	default:
		return 0;
	}

	ui64PCEntryAddr = MMU_MASK_AND_SHIFTR(ui64DevVirtAddr, THIS_BUS_WIDTH.ui64Vaddr_PCMask, THIS_BUS_WIDTH.ui32Vaddr_PCShift);
	ui64PCEntryAddr |= psContextCB->ui64PCAddress;
	return ui64PCEntryAddr;
}

/*!
******************************************************************************

 @Function				MMU_GetPageDirEntryAddress

******************************************************************************/
IMG_UINT64 MMU_GetPageDirEntryAddress (
	IMG_UINT32				ui32MMUId,
	IMG_UINT64              ui64DevVirtAddr,
	IMG_UINT64				ui64PCEntry
)
{
	MMU_sContextCB			*psContextCB = mmu_GetContext(ui32MMUId);
	IMG_UINT64				ui64PageDirAddr, ui64PageDirEntryAddr;

	IMG_ASSERT(gbMMUInitialised);
	IMG_ASSERT (psContextCB);
	
	switch (psContextCB->ui32MMUType)
	{
	case MMU_TYPE_VP_40BIT_ADDR:
		ui64PageDirAddr = MMU_MASK_AND_SHIFTL(ui64PCEntry, MMU_PC_PDP_MASK, MMU_PC_PDP_SHIFT);
		break;
	case MMU_TYPE_NONE:
	case MMU_TYPE_4K_PAGES_32BIT_ADDR:
	case MMU_TYPE_VARIABLE_PAGE_32BIT_ADDR:
	case MMU_TYPE_4K_PAGES_36BIT_ADDR:
	case MMU_TYPE_4K_PAGES_40BIT_ADDR:
		ui64PageDirAddr = psContextCB->ui64PCAddress;
		break;
	default:
		IMG_ASSERT(IMG_FALSE);
	}

	ui64PageDirEntryAddr = ui64PageDirAddr;
	ui64PageDirEntryAddr |= MMU_MASK_AND_SHIFTR(ui64DevVirtAddr, THIS_BUS_WIDTH.ui64Vaddr_PDMask, THIS_BUS_WIDTH.ui32Vaddr_PDShift );
	return ui64PageDirEntryAddr;
}

/*!
******************************************************************************

 @Function				MMU_GetPageTableEntryAddress

******************************************************************************/
IMG_UINT64 MMU_GetPageTableEntryAddress (
	IMG_UINT32				ui32MMUId,
	IMG_UINT64              ui64DevVirtAddr,
	IMG_UINT64				ui64PageDirEntry
)
{
	MMU_sContextCB			*psContextCB = mmu_GetContext(ui32MMUId);
	IMG_UINT64				ui64PageTabEntAddr, ui64PageTable;
	IMG_UINT32				ui32PageSizeId;

	IMG_ASSERT(gbMMUInitialised);
	IMG_ASSERT (psContextCB);

	// Store the page size in the settings structure 
	ui32PageSizeId = (IMG_UINT32)MMU_MASK_AND_SHIFTR(ui64PageDirEntry, THIS_BUS_WIDTH.ui64PageSizeIdMask, THIS_BUS_WIDTH.ui32PageSizeIdShift);
	
	// Get physical address of page table...
	ui64PageTable = MMU_MASK_AND_SHIFTL(ui64PageDirEntry, THIS_BUS_WIDTH.ui64PDE_PTMask, THIS_BUS_WIDTH.ui32PDE_PTShift);
	
	ui64PageTabEntAddr = ui64PageTable | MMU_MASK_AND_SHIFTR(ui64DevVirtAddr, gasMMUPageSizeLookup[ui32PageSizeId].ui64VaddrPageMask ^ THIS_BUS_WIDTH.ui64Vaddr_PTandPageMask, (gasMMUPageSizeLookup[ui32PageSizeId].ui32VaddrPTShift - THIS_BUS_WIDTH.ui32Vaddr_PTShift));
	return ui64PageTabEntAddr;
}

/*!
******************************************************************************

 @Function				MMU_GetPageAddress

******************************************************************************/
IMG_UINT64 MMU_GetPageAddress (
	IMG_UINT32				ui32MMUId,
	IMG_UINT64              ui64DevVirtAddr,
	IMG_UINT64				ui64PageTabEntry
)
{
	MMU_sContextCB			*psContextCB = mmu_GetContext(ui32MMUId);
	IMG_UINT64				ui64Page;
	IMG_UINT32				ui32PageSizeId = mmu_GetPageSizeId(ui32MMUId, ui64DevVirtAddr);

	IMG_ASSERT(gbMMUInitialised);
	IMG_ASSERT (psContextCB);

	// Get physical address of page
	ui64Page = MMU_MASK_AND_SHIFTL(ui64PageTabEntry, THIS_BUS_WIDTH.ui64PTE_PageMask, THIS_BUS_WIDTH.ui32PTE_PageShift) & (~gasMMUPageSizeLookup[ui32PageSizeId].ui64VaddrPageMask);
	// Add Offset Within Page
	ui64Page |= MMU_MASK_AND_SHIFTL(ui64DevVirtAddr, gasMMUPageSizeLookup[ui32PageSizeId].ui64VaddrPageMask, 0);
	return ui64Page;
}
/*!
******************************************************************************

 @Function				MMU_IsTiled

******************************************************************************/
IMG_BOOL MMU_IsTiled(
    IMG_UINT32				ui32MMUId,
    IMG_UINT64              ui64DevVirtAddr
)
{
    if ( mmu_GetTileRegion(ui32MMUId, ui64DevVirtAddr) >= 0 )
		return IMG_TRUE;
	return IMG_FALSE;
}


/*!
******************************************************************************

 @Function				MMU_SetTiledRegion

******************************************************************************/

IMG_VOID MMU_SetTiledRegion(
	IMG_UINT32				ui32MMUId,
    IMG_UINT32              ui32TiledRegionNo,
	IMG_UINT64				ui64DevVirtAddr,
    IMG_UINT64              ui64Size,
    IMG_UINT32              ui32XTileStride
)
{
	MMU_sContextCB *psContextCB;
	IMG_UINT32		i;

	IMG_ASSERT(gbMMUInitialised);
	/* Check MMU Id and Tiled Id are Valid */
	IMG_ASSERT(ui32MMUId < MMU_NUMBER_OF_CONTEXTS);
	IMG_ASSERT(ui32TiledRegionNo < MMU_NUMBER_OF_TILED_REGIONS);

	psContextCB = &gasContextCBs[ui32MMUId];
	IMG_ASSERT(psContextCB->ui32MMUType < MMU_NO_TYPES);
	IMG_ASSERT(psContextCB->ui32MMUType != MMU_TYPE_NONE);
	
    /* Set the tiled region parameters...*/
    psContextCB->asTiledRegionCB[ui32TiledRegionNo].ui64DevVirtAddr    = ui64DevVirtAddr;
    psContextCB->asTiledRegionCB[ui32TiledRegionNo].ui64Size           = ui64Size;
    psContextCB->asTiledRegionCB[ui32TiledRegionNo].ui32XTileStride    = ui32XTileStride;

    /* See if tiled region defined...*/
    psContextCB->bTiledRegionDefined = IMG_FALSE;
    for (i=0; i<MMU_NUMBER_OF_TILED_REGIONS; i++)
    {
        if (psContextCB->asTiledRegionCB[i].ui64Size != 0)
        {
            psContextCB->bTiledRegionDefined = IMG_TRUE;
        }
    }
}

/*!
******************************************************************************

 @Function				MMU_ContextsDefined

******************************************************************************/
IMG_UINT32 MMU_ContextsDefined ()
{
	IMG_ASSERT(gbMMUInitialised);
	return gui32ContextsInUse;
}


/*!
******************************************************************************

 @Function				MMU_Setup

******************************************************************************/
IMG_RESULT MMU_Setup (
	IMG_UINT32		ui32MMUId,
	IMG_UINT64		ui64PCAddress,
	IMG_UINT32		ui32MMUType,
	MMU_pfnReadMem	pfnReadMem
	)
{
	MMU_sContextCB *psContextCB;

	IMG_ASSERT(gbMMUInitialised);
	IMG_ASSERT(ui32MMUId < MMU_NUMBER_OF_CONTEXTS);
	
	psContextCB = &gasContextCBs[ui32MMUId];
	if (psContextCB->ui32MMUType == MMU_TYPE_NONE)
		gui32ContextsInUse++;
	memset(psContextCB, 0, sizeof(MMU_sContextCB));

	psContextCB->ui64PCAddress = ui64PCAddress;
	psContextCB->ui32MMUType = ui32MMUType;
	psContextCB->ui32BusWidth = gasMMUTypesLookup[ui32MMUType].ui32BusWidth;
	psContextCB->pfnReadLookup = pfnReadMem;
	psContextCB->pfnReadPage = pfnReadMem;
	
	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				MMU_Clear

******************************************************************************/
IMG_VOID MMU_Clear (
	IMG_UINT32		ui32MMUId
)
{
	MMU_sContextCB *psContextCB;
	IMG_ASSERT(gbMMUInitialised);
	IMG_ASSERT(ui32MMUId < MMU_NUMBER_OF_CONTEXTS);
		
	psContextCB = &gasContextCBs[ui32MMUId];
	if (psContextCB->ui32MMUType != MMU_TYPE_NONE)
		gui32ContextsInUse--;
	psContextCB->ui32MMUType = MMU_TYPE_NONE;
}

/*!
******************************************************************************

 @Function				MMU_PageReadCallback

******************************************************************************/
extern IMG_RESULT MMU_PageReadCallback (
	IMG_UINT32		ui32MMUId,
	MMU_pfnReadMem	pfnReadMem
)
{
	MMU_sContextCB *psContextCB;
	IMG_ASSERT(gbMMUInitialised);
	IMG_ASSERT(ui32MMUId < MMU_NUMBER_OF_CONTEXTS);
		
	psContextCB = &gasContextCBs[ui32MMUId];
	psContextCB->pfnReadPage = pfnReadMem;

	return IMG_SUCCESS;
}

