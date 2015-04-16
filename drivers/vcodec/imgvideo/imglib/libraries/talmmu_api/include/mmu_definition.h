/*!
 *****************************************************************************
 *
 * @file	   mmu_definition.h
 *
 * : parameter defines for each MMU
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

// General Parameters
#define MMU_VALID_MASK										(0x1)

/*!
******************************************************************************

	Page Details

******************************************************************************/

// Page Sizes
#define MMU_PAGESHIFT_4K									(12)
#define MMU_PAGESHIFT_16K									(MMU_PAGESHIFT_4K + 2)
#define MMU_PAGESHIFT_64K									(MMU_PAGESHIFT_16K + 2)
#define MMU_PAGESHIFT_256K									(MMU_PAGESHIFT_64K + 2)
#define MMU_PAGESHIFT_1M									(MMU_PAGESHIFT_256K + 2)
#define MMU_PAGESHIFT_4M									(MMU_PAGESHIFT_1M + 2)

#define MMU_PAGESHIFT(pagesize)								MMU_PAGESHIFT_##pagesize
#define MMU_PAGESIZE(pagesize)								(1ULL << (MMU_PAGESHIFT(pagesize)))
#define MMU_PAGEMASK(pagesize)								((MMU_PAGESIZE(pagesize)) - 1)


#define MMU_MASK_AND_SHIFTR(value, mask, shift)			( ((value) & (mask)) >> shift )
#define MMU_MASK_AND_SHIFTL(value, mask, shift)			( ((value) & (mask)) << shift )

//Number of Address bits used in Table entries
#define MMU_ADDRBITS_PCE_40VID								(0)
#define MMU_ADDRBITS_PCE_40									(2)
#define MMU_ADDRBITS_PCE_36									(0)
#define MMU_ADDRBITS_PCE_32									(0)

#define MMU_ADDRBITS_PDE_40VID								(2)
#define MMU_ADDRBITS_PDE_40									(3)
#define MMU_ADDRBITS_PDE_36									(2)
#define MMU_ADDRBITS_PDE_32									(2)

#define MMU_ADDRBITS_PTE_40VID								(2)
#define MMU_ADDRBITS_PTE_40									(3)
#define MMU_ADDRBITS_PTE_36									(2)
#define MMU_ADDRBITS_PTE_32									(2)


/*!
******************************************************************************

	Virtual Address Details

******************************************************************************/

// The top bit of the Page Table bits in the virtual address
#define MMU_VA_PT_TOPBIT_32									(21)
#define MMU_VA_PT_TOPBIT_36									(21)
#define MMU_VA_PT_TOPBIT_40									(20)
#define MMU_VA_PT_TOPBIT_40VID								(21)

// The top bit of the Page Directory bits in the virtual address
#define MMU_VA_PD_TOPBIT_32									(31)
#define MMU_VA_PD_TOPBIT_36									(31)
#define MMU_VA_PD_TOPBIT_40									(29)
#define MMU_VA_PD_TOPBIT_40VID								(31)

// The top bit of the Page Catalogue in the virtual address
#define MMU_VA_PC_TOPBIT_32									(31)
#define MMU_VA_PC_TOPBIT_36									(31)
#define MMU_VA_PC_TOPBIT_40									(39)
#define MMU_VA_PC_TOPBIT_40VID								(31)

// Masks for each section of the virtual address
#define MMU_VA_PT_PAGE_MASK(buswidth)						( (2ULL << MMU_VA_PT_TOPBIT_##buswidth) - 1 )
#define MMU_VA_PT_MASK(pagesizeid, ptandpagemask)			( MMU_PAGEMASK(pagesizeid) ^ ptandpagemask )
#define MMU_VA_PD_MASK(buswidth)							( ( (2ULL << MMU_VA_PD_TOPBIT_##buswidth) - 1 ) ^ MMU_VA_PT_PAGE_MASK(buswidth) )
#define MMU_VA_PC_MASK(buswidth)							( ( ( (2ULL << MMU_VA_PC_TOPBIT_##buswidth) - 1 ) ^ MMU_VA_PD_MASK(buswidth) ) ^ MMU_VA_PT_PAGE_MASK(buswidth) )

#define MMU_VA_PC_SHIFT(buswidth)							( MMU_VA_PD_TOPBIT_##buswidth +1 - MMU_ADDRBITS_PCE_##buswidth )		
#define MMU_VA_PD_SHIFT(buswidth)							( MMU_VA_PT_TOPBIT_##buswidth +1 - MMU_ADDRBITS_PDE_##buswidth)		
#define MMU_VA_PT_SHIFT(buswidth)							( MMU_ADDRBITS_PTE_##buswidth )

/*!
******************************************************************************

	Page Catalogue Details

******************************************************************************/
#define MMU_PC_ATTRIBUTES_TOPBIT							(3)					//!< The top bit of the attributes within the PC entry
#define	MMU_PC_PDP_TOPBIT									(31)				//!< The top bit of the PD Address within the PC entry
#define MMU_PC_PDP_SHIFT									(8)					//!< The number of bits the PD address entry has to be shifted left to get the real address

#define MMU_PCE_SIZE_32										(0)					//!< Size in bytes of each PC entry
#define MMU_PCE_SIZE_36										(0)					//!< Size in bytes of each PC entry
#define MMU_PCE_SIZE_40										(4)					//!< Size in bytes of each PC entry
#define MMU_PCE_SIZE_40VID									(0)					//!< Size in bytes of each PC entry

#define MMU_PCE_SIZE(buswidth)								MMU_PCE_SIZE_##buswidth

// This section only works for 40bit MMU at present and will need expension for new MMU types
#define MMU_PC_PDP_MASK										( ((2ULL << MMU_PC_PDP_TOPBIT)-1) ^ ((2ULL << MMU_PC_ATTRIBUTES_TOPBIT)-1) )	//!< PDP Entry Mask


/*!
******************************************************************************

	Page Directory Details

******************************************************************************/
#define MMU_PD_ATTRIBUTES_TOPBIT_32							(3)
#define MMU_PD_ATTRIBUTES_TOPBIT_36							(3)
#define MMU_PD_ATTRIBUTES_TOPBIT_40							(3)
#define MMU_PD_ATTRIBUTES_TOPBIT_40VID						(3)

#define MMU_PD_PAGESIZEID_SIZE								(3)

#define MMU_PD_PAGESIZEID_SHIFT(buswidth)					((MMU_PD_ATTRIBUTES_TOPBIT_##buswidth) - MMU_PD_PAGESIZEID_SIZE + 1)
#define MMU_PD_PAGESIZEID_MASK								(0xE)

#define MMU_PDE_TOPBIT_32									(31)
#define MMU_PDE_TOPBIT_36									(31)
#define MMU_PDE_TOPBIT_40									(39)
#define MMU_PDE_TOPBIT_40VID								(31)

#define MMU_PDE_PT_SHIFT_32									(0)
#define MMU_PDE_PT_SHIFT_36									(4)
#define MMU_PDE_PT_SHIFT_40									(0)
#define MMU_PDE_PT_SHIFT_40VID								(8)

#define MMU_PD_PT_SHIFT(buswidth)							MMU_PDE_PT_SHIFT_##buswidth
#define MMU_PD_PT_MASK(buswidth)							( (( 2ULL << MMU_PD_ATTRIBUTES_TOPBIT_##buswidth )-1) ^ (( 2ULL << MMU_PDE_TOPBIT_##buswidth)-1) )
#define MMU_PDE_SIZE(buswidth)								(1 << MMU_ADDRBITS_PDE_##buswidth)

/*!
******************************************************************************

	Page Table Details

******************************************************************************/
#define MMU_PT_ATTRIBUTES_TOPBIT_32									(3)
#define MMU_PT_ATTRIBUTES_TOPBIT_36									(3)
#define MMU_PT_ATTRIBUTES_TOPBIT_40									(11)
#define MMU_PT_ATTRIBUTES_TOPBIT_40VID								(3)
	
#define MMU_PTE_TOPBIT_32											(31)
#define MMU_PTE_TOPBIT_36											(31)
#define MMU_PTE_TOPBIT_40											(39)
#define MMU_PTE_TOPBIT_40VID										(31)

#define MMU_PTE_SHIFT_32											(0)
#define MMU_PTE_SHIFT_36											(4)
#define MMU_PTE_SHIFT_40											(0)
#define MMU_PTE_SHIFT_40VID											(8)

#define MMU_PTE_PAGE_SHIFT(buswidth)								MMU_PTE_SHIFT_##buswidth	
#define MMU_PTE_SIZE(buswidth)										(1 << MMU_ADDRBITS_PTE_##buswidth)

#define MMU_PTE_PAGE_MASK(buswidth)									( ( (2ULL << MMU_PT_ATTRIBUTES_TOPBIT_##buswidth)-1 ) ^ ( (2ULL << MMU_PTE_TOPBIT_##buswidth)-1 ) )


