
/*
 * Copyright(c) 2013 by LG Electronics Inc.
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef __ODIN_IOMMU_HW_H__
#define __ODIN_IOMMU_HW_H__


#include <linux/compiler.h>


#define	SDMA_SMMU_BASE									(0x0D020000)

/* =========================================================== */
/* =========================================================== */
/* ========= for ARMv7, Stage 2 Translation Table Long-Descriptor ========= */
/* =========================================================== */
/* =========================================================== */


/* memory attribute */

#define	DATA_INNER_SHAREABLE							(0x3)
#define	DATA_NON_SAREABLE								(0x0)

#define	DATA_OUTER_INNER_WB_CACHEABLE				(0xF)
#define	DATA_OUTER_INNER_NON_CACHEABLE				(0x5)

#define	PTW_INNER_SHAREABLE							(0x2)
#define	PTW_NON_SHAREABLE								(0x0)


/* descriptor */

#define	LONG_DESC_ENTRY_NUM							(512)

#define LDS2_NLTADDR_SHIFT							(12)
#define LDS2_XN_SHIFT									(54)
#define LDS2_CH_SHIFT									(52)
#define LDS2_AF_SHIFT									(10)
#define LDS2_SH_SHIFT									(8)
#define LDS2_HAP_SHIFT									(6)
#define LDS2_MEMATTR_SHIFT							(2)
#define LDS2_TYPE_SHIFT								(1)
#define LDS2_VALID_SHIFT								(0)

#define LDS2_NLTADDR(x)								((u64)((u64)(x) << LDS2_NLTADDR_SHIFT))
#define LDS2_XN(x)										((u64)((u64)(x) << LDS2_XN_SHIFT))
#define LDS2_CONT(x)									((u64)((u64)(x) << LDS2_CH_SHIFT))
#define LDS2_AF(x)										((u64)((u64)(x) << LDS2_AF_SHIFT))
#define LDS2_SH(x)										((u64)((u64)(x) << LDS2_SH_SHIFT))
#define LDS2_HAP(x)									((u64)((u64)(x) << LDS2_HAP_SHIFT))
#define LDS2_MEMATTR(x)								((u64)((u64)(x) << LDS2_MEMATTR_SHIFT))
#define LDS2_TYPE(x)									((u64)((u64)(x) << LDS2_TYPE_SHIFT))
#define LDS2_VALID(x)									((u64)((u64)(x) << LDS2_VALID_SHIFT))

#define LDS2_PGT										((u64)(LDS2_TYPE(0x1) | LDS2_VALID(0x1)))

#define LDS2_MEM_USE_CCI								((u64)(LDS2_XN(0x0) | LDS2_AF(0x1)				\
														| LDS2_SH(DATA_INNER_SHAREABLE)					\
														| LDS2_HAP(0x3)								\
														| LDS2_MEMATTR(DATA_OUTER_INNER_WB_CACHEABLE)	\
														| LDS2_VALID(0x1)))

#define LDS2_MEM_USE_NOC								((u64)(LDS2_XN(0x0) | LDS2_AF(0x1)				\
														| LDS2_SH(DATA_NON_SAREABLE)					\
														| LDS2_HAP(0x3)								\
														| LDS2_MEMATTR(DATA_OUTER_INNER_NON_CACHEABLE)	\
														| LDS2_VALID(0x1)))

#define LDS2_MEM										LDS2_MEM_USE_NOC


/* for computing address */

#define FL_ADDR_BIT_SHIFT								(30)
#define SL_ADDR_BIT_SHIFT								(21)
#define TL_ADDR_BIT_SHIFT								(12)

#define FL_BASE_ADDR_MASK								((u64)(0x000000FFC0000000))
/* #define SL_BASE_ADDR_MASK								((u64)(0x000000003FE00000))	*/
/* #define TL_BASE_ADDR_MASK								((u64)(0x00000000001FF000))	*/
#define SL_BASE_ADDR_MASK								(0x3FE00000)
#define TL_BASE_ADDR_MASK								(0x001FF000)

#define FL_ADDR_OFFSET(va)							\
					((u64)( (((u64)(va))&FL_BASE_ADDR_MASK) >> (FL_ADDR_BIT_SHIFT) ))
#define SL_ADDR_OFFSET(va)		( ((va)&SL_BASE_ADDR_MASK) >> SL_ADDR_BIT_SHIFT )
#define TL_ADDR_OFFSET(va)		( ((va)&TL_BASE_ADDR_MASK) >> TL_ADDR_BIT_SHIFT )

#define NEXT_LEVEL_PAGE_ADDR_MASK					((u64)(0x000000FFFFFFF000))

#define OUTPUT_PHYS_ADDR_MASK							((u64)(0x000000FFFFFFF000))

#define	SL_PAGE_NUM									4	/* IOVA : 4G */


/* for overlapped mapping */

#define	OVERLAP_MAPPING_MASK							((u64)(0x0780000000000000))
#define	OVERLAP_MAPPING_FIRST							((u64)(0x0080000000000000))
#define	OVERLAP_START_BIT_POSITION					(55)


/* =========================================================== */
/* =========================================================== */
/* ============= setter / getter macros ============================ */
/* =========================================================== */
/* =========================================================== */


#define CTX_SHIFT (12)

/* === global & ctx register setter / getter  ============================ */

#define GET_GLOBAL_REG(reg, base) 		( readl( ((base)+(reg)) ) )
#define SET_GLOBAL_REG(reg, base, val)	( writel( (val), ((base)+(reg)) ) )

#define GET_GLOBAL_REG_N(r, n, b)    		\
								GET_GLOBAL_REG( ((r)+(((u32)n)<<(2))), (b) )
#define SET_GLOBAL_REG_N(r, b, v, n) 		\
								SET_GLOBAL_REG( ((r)+(((u32)n)<<(2))), (b), (v) )

#define GET_CTX_REG(reg, base, ctx) 		\
								( readl( (base)+(reg)+(((u32)ctx)<<CTX_SHIFT)) )
#define SET_CTX_REG(reg, base, ctx, val)	\
								writel( (val), ((base)+(((u32)ctx)<<CTX_SHIFT)+(reg)) )

/* === global & ctx register field setter / getter  ==================== */

#define GET_FIELD(addr, mask, shift)					( (readl(addr)>>(shift)) & (mask) )
#define SET_FIELD(addr, mask, shift, v) \
do { \
	int t = readl(addr); \
	writel( (t & ~((mask)<<(shift)) ) + ( ((v)&(mask))<<(shift) ), (addr) );\
} while (0)

#define GET_GLOBAL_FIELD(b, r, F)    					\
											GET_FIELD( ((b)+(r)), (F##_MASK), (F##_SHIFT) )
#define SET_GLOBAL_FIELD(b, r, F, v)					\
											SET_FIELD( ((b)+(r)), (F##_MASK), (F##_SHIFT), (v) )

#define GET_CONTEXT_FIELD(b, c, r, F)					\
				GET_FIELD( ((b)+(r)+(((u32)c)<<CTX_SHIFT)), (F##_MASK), (F##_SHIFT) )
#define SET_CONTEXT_FIELD(b, c, r, F, v)				\
				SET_FIELD( ((b)+(r)+(((u32)c)<<CTX_SHIFT)), (F##_MASK), (F##_SHIFT), (v) )


/* =========================================================== */
/* =========================================================== */
/* ================ register setter / getter ========================= */
/* =========================================================== */
/* =========================================================== */


/* === Global space 0 register ====================================== */

/* === offsets === */

#define GS0_CR0											(0x0000)
#define GS0_NSCR0										(0x0400)

#define GS0_SCR1										(0x0004)

#define GS0_ACR											(0x0010)
#define GS0_NSACR										(0x0410)

#define GS0_IDR0										(0x0020)
#define GS0_IDR1										(0x0024)
#define GS0_IDR2										(0x0028)
#define GS0_IDR7										(0x003C)
#define GS0_GFAR_L										(0x0040)
#define	GS0_GFAR_H										(0x0044)

#define GS0_GFSR										(0x0048)
#define GS0_NSGFSR										(0x0448)

#define GS0_GFSRRESTORE								(0x004C)
#define GS0_GFSYNR0									(0x0050)
#define GS0_NSGFSYNR0									(0x0450)

#define GS0_GFSYNR1									(0x0054)
#define	GS0_STLBIALL									(0x0060)
#define GS0_TLBIVMID									(0x0064)
#define GS0_TLBIALLNSNH								(0x0068)
#define GS0_TLBGSYNC									(0x0070)
#define GS0_TLBGSTATUS									(0x0074)

#define GS0_DBGRPTR									(0x0080)
#define GS0_DBGRDATA									(0x0084)

#define GS0_SMR_N										(0x0800)
#define GS0_S2CR_N										(0x0C00)

/* === setter === */

/* use this code when kernel is switched to NS */
#define SET_GS0_CR0(b, v)			 				SET_GLOBAL_REG(GS0_CR0, (b), (v))
#define SET_GS0_SCR1(b, v)			 				SET_GLOBAL_REG(GS0_SCR1, (b), (v))
#define SET_GS0_ACR(b, v)	 						SET_GLOBAL_REG(GS0_ACR, (b), (v))
#define SET_GS0_GFSR(b, v)							SET_GLOBAL_REG(GS0_GFSR, (b), (v))

#define SET_GS0_NSCR0(b, v)			 			SET_GLOBAL_REG(GS0_NSCR0, (b), (v))
#define SET_GS0_NSACR(b, v)	 					SET_GLOBAL_REG(GS0_NSACR, (b), (v))
#define SET_GS0_NSGFSR(b, v)						SET_GLOBAL_REG(GS0_NSGFSR, (b), (v))

#define SET_GS0_GFAR_L(b, v)						SET_GLOBAL_REG(GS0_GFAR_L, (b), (v))
#define SET_GS0_GFAR_H(b, v)						SET_GLOBAL_REG(GS0_GFAR_H, (b), (v))
#define SET_GS0_GFSRRESTORE(b, v)					SET_GLOBAL_REG(GS0_GFSRRESTORE, (b), (v))
#define SET_GS0_GFSYNR0(b, v)						SET_GLOBAL_REG(GS0_GFSYNR0, (b), (v))
#define SET_GS0_GFSYNR1(b, v)						SET_GLOBAL_REG(GS0_GFSYNR1, (b), (v))
#define SET_GS0_STLBIALL(b, v)					SET_GLOBAL_REG(GS0_STLBIALL, (b), (v))
#define SET_GS0_TLBIVMID(b, v)					SET_GLOBAL_REG(GS0_TLBIVMID, (b), (v))
#define SET_GS0_TLBIALLNSNH(b, v)					SET_GLOBAL_REG(GS0_TLBIALLNSNH, (b), (v))
#define SET_GS0_TLBGSYNC(b, v)					SET_GLOBAL_REG(GS0_TLBGSYNC, (b), (v))

#define SET_GS0_DBGRPTR(b, v)						SET_GLOBAL_REG(GS0_DBGRPTR, (b), (v))

#define SET_GS0_SMR_N(b, v, n)					SET_GLOBAL_REG_N(GS0_SMR_N, (b), (v), (n))
#define SET_GS0_S2CR_N(b, v, n)					SET_GLOBAL_REG_N(GS0_S2CR_N, (b), (v), (n))

/* === getter === */

#define GET_GS0_CR0(b)									GET_GLOBAL_REG(GS0_CR0, (b))
#define GET_GS0_ACR(b)									GET_GLOBAL_REG(GS0_ACR, (b))
#define GET_GS0_IDR0(b)								GET_GLOBAL_REG(GS0_IDR0, (b))
#define GET_GS0_IDR1(b)								GET_GLOBAL_REG(GS0_IDR1, (b))
#define GET_GS0_IDR2(b)								GET_GLOBAL_REG(GS0_IDR2, (b))
#define GET_GS0_IDR7(b)								GET_GLOBAL_REG(GS0_IDR7, (b))
#define GET_GS0_GFAR_L(b)								GET_GLOBAL_REG(GS0_GFAR_L, (b))
#define	GET_GS0_GFAR_H(b)								GET_GLOBAL_REG(GS0_GFAR_H, (b))
#define GET_GS0_GFSR(b)								GET_GLOBAL_REG(GS0_GFSR, (b))

#define GET_GS0_NSGFSR(b)								GET_GLOBAL_REG(GS0_NSGFSR, (b))

#define GET_GS0_GFSRRESTORE(b)						GET_GLOBAL_REG(GS0_GFSRRESTORE, (b))
#define GET_GS0_GFSYNR0(b)							GET_GLOBAL_REG(GS0_GFSYNR0, (b))

#define GET_GS0_NSGFSYNR0(b)							GET_GLOBAL_REG(GS0_NSGFSYNR0, (b))

#define GET_GS0_GFSYNR1(b)							GET_GLOBAL_REG(GS0_GFSYNR1, (b))
#define GET_GS0_TLBIVMID(b)							GET_GLOBAL_REG(GS0_TLBIVMID, (b))
#define GET_GS0_TLBIALLNSNH(b)						GET_GLOBAL_REG(GS0_TLBIALLNSNH, (b))
#define GET_GS0_TLBGSYNC(b)							GET_GLOBAL_REG(GS0_TLBGSYNC, (b))
#define GET_GS0_TLBGSTATUS(b)							GET_GLOBAL_REG(GS0_TLBGSTATUS, (b))

#define GET_GS0_DBGRDATA(b)							GET_GLOBAL_REG(GS0_DBGRDATA, (b))

#define GET_GS0_SMR_N(b, n)	 						GET_GLOBAL_REG_N(GS0_SMR_N, (n), (b))
#define GET_GS0_S2CR_N(b, n)	 						GET_GLOBAL_REG_N(GS0_S2CR_N, (n), (b))

/* === Global space 1 register ====================================== */

/* === offsets === */

#define GS1_CBAR_N										(0x1000)
#define GS1_CBFRSYNRA_N								(0x1400)

/* === setter === */

#define SET_GS1_CBAR_N(b, v, n)						\
								SET_GLOBAL_REG_N(GS1_CBAR_N, (b), (v), (n))
#define SET_GS1_CBFRSYNRA_N(b, v, n)					\
								SET_GLOBAL_REG_N(GS1_CBFRSYNRA_N, (b), (v), (n))

/* === getter === */

#define GET_GS1_CBAR_N(b, n)							\
								GET_GLOBAL_REG_N(GS1_CBAR_N, (n), (b))
#define GET_GS1_CBFRSYNRA_N(b, n)	 					\
								GET_GLOBAL_REG_N(GS1_CBFRSYNRA_N, (n), (b))

/* === Integration register ========================================= */

/* === offsets === */

#define ITCTRL											(0x2000)
#define ITOP											(0x2004)
#define ITIP											(0x2008)

/* === setter === */

#define SET_ITCTRL(b, v)	 							SET_GLOBAL_REG(ITCTRL, (b), (v))
#define SET_ITOP(b, v)	 								SET_GLOBAL_REG(ITOP, (b), (v))

/* === getter === */

#define GET_ITCTRL(b)									GET_GLOBAL_REG(ITCTRL, (b))
#define GET_ITOP(b)									GET_GLOBAL_REG(ITOP, (b))
#define GET_ITIP(b)									GET_GLOBAL_REG(ITIP, (b))

/* === Performance monitoring register ================================ */

/* === offsets === */

#define PMEVCNTR_N										(0x3000)
#define PMEVTYPE_N										(0x3400)
#define PMCGCR_N										(0x3800)
#define PMCGSMR_N										(0x3A00)
#define PMCNTENSET_N									(0x3C00)
#define PMCNTENCLR_N									(0x3C20)
#define PMINTENSET_N									(0x3C40)
#define PMINTENCLR_N									(0x3C60)
#define PMOVSCLR_N										(0x3C80)
#define PMOVSSET_N										(0x3CC0)
#define PMCFGR											(0x3E00)
#define PMCR											(0x3E04)
#define PMCEID_N										(0x3E20)
#define PMAUTHSTATUS									(0x3FB8)
#define PMDEVTYPE										(0x3FCC)

/* === setter === */

#define SET_PMEVCNTR_N(b, v, n)						\
								SET_GLOBAL_REG_N(PMEVCNTR_N, (b), (v), (n))
#define SET_PMEVTYPE_N(b, v, n)						\
								SET_GLOBAL_REG_N(PMEVTYPE_N, (b), (v), (n))
#define SET_PMCGCR_N(b, v, n)							\
								SET_GLOBAL_REG_N(PMCGCR_N, (b), (v), (n))
#define SET_PMCGSMR_N(b, v, n)						\
								SET_GLOBAL_REG_N(PMCGSMR_N, (b), (v), (n))
#define SET_PMCNTENSET_N(b, v, n)						\
								SET_GLOBAL_REG_N(PMCNTENSET_N, (b), (v), (n))
#define SET_PMCNTENCLR_N(b, v, n)						\
								SET_GLOBAL_REG_N(PMCNTENCLR_N, (b), (v), (n))
#define SET_PMINTENSET_N(b, v, n)						\
								SET_GLOBAL_REG_N(PMINTENSET_N, (b), (v), (n))
#define SET_PMINTENCLR_N(b, v, n)						\
								SET_GLOBAL_REG_N(PMINTENCLR_N, (b), (v), (n))
#define SET_PMOVSCLR_N(b, v, n)						\
								SET_GLOBAL_REG_N(PMOVSCLR_N, (b), (v), (n))
#define SET_PMOVSSET_N(b, v, n)						\
								SET_GLOBAL_REG_N(PMOVSSET_N, (b), (v), (n))
#define SET_PMCR(b, v)									\
								SET_GLOBAL_REG(PMCR, (b), (v))

/* === getter === */

#define GET_PMEVCNTR_N(b, n)	 						GET_GLOBAL_REG_N(PMEVCNTR_N, (n), (b))
#define GET_PMEVTYPE_N(b, n)	 						GET_GLOBAL_REG_N(PMEVTYPE_N, (n), (b))
#define GET_PMCGCR_N(b, n)	 							GET_GLOBAL_REG_N(PMCGCR_N, (n), (b))
#define GET_PMCGSMR_N(b, n)	 						GET_GLOBAL_REG_N(PMCGSMR_N, (n), (b))
#define GET_PMCNTENSET_N(b, n)	 					GET_GLOBAL_REG_N(PMCNTENSET_N, (n), (b))
#define GET_PMCNTENCLR_N(b, n)	 					GET_GLOBAL_REG_N(PMCNTENCLR_N, (n), (b))
#define GET_PMINTENSET_N(b, n)	 					GET_GLOBAL_REG_N(PMINTENSET_N, (n), (b))
#define GET_PMINTENCLR_N(b, n)	 					GET_GLOBAL_REG_N(PMINTENCLR_N, (n), (b))
#define GET_PMOVSCLR_N(b, n)	 						GET_GLOBAL_REG_N(PMOVSCLR_N, (n), (b))
#define GET_PMOVSSET_N(b, n)	 						GET_GLOBAL_REG_N(PMOVSSET_N, (n), (b))
#define GET_PMCFGR(b)									GET_GLOBAL_REG(PMCFGR, (b))
#define GET_PMCR(b)									GET_GLOBAL_REG(PMCR, (b))
#define GET_PMCEID_N(b, n)	 							GET_GLOBAL_REG_N(PMCR, (n), (b))
#define GET_PMAUTHSTATUS(b)							GET_GLOBAL_REG(PMAUTHSTATUS, (b))
#define GET_PMDEVTYPE(b)								GET_GLOBAL_REG(PMDEVTYPE, (b))

/* === Translation context bank register ============================== */

#define	CB												(0x8000)

/* === offsets === */

#define CB_SCTLR										(CB+0x0000)
#define CB_TTBR0_L										(CB+0x0020)
#define CB_TTBR0_H										(CB+0x0024)
#define CB_TTBCR										(CB+0x0030)
#define CB_FSR											(CB+0x0058)
#define CB_FSRRESTORE									(CB+0x005C)
#define CB_FAR_L										(CB+0x0060)
#define CB_FAR_H										(CB+0x0064)
#define CB_FSYNR0										(CB+0x0068)
#define CB_PMXEVCNTR									(CB+0x0E00)
#define CB_PMXEVTYPER									(CB+0x0E80)
#define CB_PMCFGR										(CB+0x0F00)
#define CB_PMCR											(CB+0x0F04)
#define CB_PMCEID_N									(CB+0x0F20)
#define CB_PMCNTENSET									(CB+0x0F40)
#define CB_PMCNTENCLR									(CB+0x0F44)
#define CB_PMINTENSET									(CB+0x0F48)
#define CB_PMINTENCLR									(CB+0x0F4C)
#define CB_PMOVSRCLR									(CB+0x0F54)
#define CB_PMOVSRSET									(CB+0x0F50)
#define CB_PMAUTHSTATUS								(CB+0x0FB8)

/* === setter === */

#define SET_CB_SCTLR(b, c, v)						SET_CTX_REG(CB_SCTLR, (b), (c), (v))
#define SET_CB_TTBR0_L(b, c, v)					SET_CTX_REG(CB_TTBR0_L, (b), (c), (v))
#define SET_CB_TTBR0_H(b, c, v)					SET_CTX_REG(CB_TTBR0_H, (b), (c), (v))
#define SET_CB_TTBCR(b, c, v)						SET_CTX_REG(CB_TTBCR, (b), (c), (v))
#define SET_CB_FSR(b, c, v)						SET_CTX_REG(CB_FSR, (b), (c), (v))
#define SET_CB_FSRRESTORE(b, c, v)				SET_CTX_REG(CB_FSRRESTORE, (b), (c), (v))
#define SET_CB_FAR_L(b, c, v)						SET_CTX_REG(CB_FAR_L, (b), (c), (v))
#define SET_CB_FAR_H(b, c, v)						SET_CTX_REG(CB_FAR_H, (b), (c), (v))
#define SET_CB_FSYNR0(b, c, v)					SET_CTX_REG(CB_FSYNR0, (b), (c), (v))
#define SET_CB_PMXEVCNTR(b, c, v)					SET_CTX_REG(CB_PMXEVCNTR, (b), (c), (v))
#define SET_CB_PMXEVTYPER(b, c, v)				SET_CTX_REG(CB_PMXEVTYPER, (b), (c), (v))
#define SET_CB_PMCR(b, c, v)						SET_CTX_REG(CB_PMCR, (b), (c), (v))
#define SET_CB_PMCNTENSET(b, c, v)				SET_CTX_REG(CB_PMCNTENSET, (b), (c), (v))
#define SET_CB_PMCNTENCLR(b, c, v)				SET_CTX_REG(CB_PMCNTENCLR, (b), (c), (v))
#define SET_CB_PMINTENSET(b, c, v)				SET_CTX_REG(CB_PMINTENSET, (b), (c), (v))
#define SET_CB_PMINTENCLR(b, c, v)				SET_CTX_REG(CB_PMINTENCLR, (b), (c), (v))
#define SET_CB_PMOVSRCLR(b, c, v)					SET_CTX_REG(CB_PMOVSRCLR, (b), (c), (v))
#define SET_CB_PMOVSRSET(b, c, v)					SET_CTX_REG(CB_PMOVSRSET, (b), (c), (v))

/* === getter === */

#define GET_CB_SCTLR(b, c)								GET_CTX_REG(CB_SCTLR, (b), (c))
#define GET_CB_TTBCR(b, c)								GET_CTX_REG(CB_TTBCR, (b), (c))
#define GET_CB_FSR(b, c)								GET_CTX_REG(CB_FSR, (b), (c))
#define GET_CB_FSRRESTORE(b, c)						GET_CTX_REG(CB_FSRRESTORE, (b), (c))
#define GET_CB_FAR_L(b, c)								GET_CTX_REG(CB_FAR_L, (b), (c))
#define GET_CB_FAR_H(b, c)								GET_CTX_REG(CB_FAR_H, (b), (c))
#define GET_CB_FSYNR0(b, c)							GET_CTX_REG(CB_FSYNR0, (b), (c))
#define GET_CB_PMXEVCNTR(b, c)						GET_CTX_REG(CB_PMXEVCNTR, (b), (c))
#define GET_CB_PMXEVTYPER(b, c)						GET_CTX_REG(CB_PMXEVTYPER, (b), (c))
#define GET_CB_PMCFGR(b, c)							GET_CTX_REG(CB_PMCFGR, (b), (c))
#define GET_CB_PMCR(b, c)								GET_CTX_REG(CB_PMCR, (b), (c))
#define GET_CB_PMCNTENSET(b, c)						GET_CTX_REG(CB_PMCNTENSET, (b), (c))
#define GET_CB_PMCNTENCLR(b, c)						GET_CTX_REG(CB_PMCNTENCLR, (b), (c))
#define GET_CB_PMINTENSET(b, c)						GET_CTX_REG(CB_PMINTENSET, (b), (c))
#define GET_CB_PMINTENCLR(b, c)						GET_CTX_REG(CB_PMINTENCLR, (b), (c))
#define GET_CB_PMOVSRCLR(b, c)						GET_CTX_REG(CB_PMOVSRCLR, (b), (c))
#define GET_CB_PMOVSRSET(b, c)						GET_CTX_REG(CB_PMOVSRSET, (b), (c))
#define GET_CB_PMAUTHSTATUS(b, c)						GET_CTX_REG(CB_PMAUTHSTATUS, (b), (c))

/* =========================================================== */
/* =========================================================== */
/* ====================== bit field setter / getter =================== */
/* =========================================================== */
/* =========================================================== */

/* === Global space 0 register ====================================== */

/* === setter === */

/* --- GS0_CR0 --- */

#define SET_GS0_CR0_CLIENTPD(b, v)					\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_CLIENTPD, v)
#define SET_GS0_CR0_GFRE(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_GFRE, v)
#define	SET_GS0_CR0_GFIE(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_GFIE, v)
#define	SET_GS0_CR0_GCFGFRE(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_GCFGFRE, v)
#define	SET_GS0_CR0_GCFGFIE(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_GCFGFIE, v)
#define	SET_GS0_CR0_STALLD(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_STALLD, v)
#define	SET_GS0_CR0_GSE(b, v)							\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_GSE, v)
#define	SET_GS0_CR0_USFCFG(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_USFCFG, v)
#define	SET_GS0_CR0_VMIDPNE(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_VMIDPNE, v)
#define	SET_GS0_CR0_PTM(b, v)							\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_PTM, v)
#define	SET_GS0_CR0_FB(b, v)							\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_FB, v)
#define	SET_GS0_CR0_BSU(b, v)							\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_BSU, v)
#define	SET_GS0_CR0_MEMATTR(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_MEMATTR, v)
#define	SET_GS0_CR0_MTCFG(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_MTCFG, v)
#define	SET_GS0_CR0_SMCFCFG(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_SMCFCFG, v)
#define	SET_GS0_CR0_SHCFG(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_SHCFG, v)
#define	SET_GS0_CR0_RACFG(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_RACFG, v)
#define	SET_GS0_CR0_WACFG(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_WACFG, v)
#define	SET_GS0_CR0_NSCFG(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_NSCFG, v)

/* --- GS0_ACR --- */

#define	SET_GS0_ACR_PREFETCHEN(b, v)					\
						SET_GLOBAL_FIELD(b, GS0_ACR, GS0_ACR_PREFETCHEN, v)
#define	SET_GS0_ACR_WC1EN(b, v)						\
						SET_GLOBAL_FIELD(b, GS0_ACR, GS0_ACR_WC1EN, v)
#define	SET_GS0_ACR_WC2EN(b, v)						\
						SET_GLOBAL_FIELD(b, GS0_ACR, GS0_ACR_WC2EN, v)
#define	SET_GS0_ACR_SMTNMB_TLBEN(b, v)				\
						SET_GLOBAL_FIELD(b, GS0_ACR, GS0_ACR_SMTNMB_TLBEN, v)
#define	SET_GS0_ACR_MMUDISB_TLBEN(b, v)				\
						SET_GLOBAL_FIELD(b, GS0_ACR, GS0_ACR_MMUDISB_TLBEN, v)
#define	SET_GS0_ACR_S2CRB_TLBEN(b, v)				\
						SET_GLOBAL_FIELD(b, GS0_ACR, GS0_ACR_S2CRB_TLBEN, v)

/* --- GS0_GFSR / GS0_GFSRRESTORE --- */

#define SET_GS0_GFSR_ICF(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_ICF, v)
#define	SET_GS0_GFSR_USF(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_USF, v)
#define	SET_GS0_GFSR_SMCF(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_SMCF, v)
#define	SET_GS0_GFSR_UCBF(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_UCBF, v)
#define	SET_GS0_GFSR_UCIF(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_UCIF, v)
#define	SET_GS0_GFSR_CAF(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_CAF, v)
#define	SET_GS0_GFSR_EF(b, v)							\
								SET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_EF, v)
#define	SET_GS0_GFSR_PF(b, v)							\
								SET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_PF, v)
#define	SET_GS0_GFSR_MULTI(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_MULTI, v)

#define	SET_GS0_GFSRRESTORE_ICF(b, v)				\
								SET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_ICF, v)
#define	SET_GS0_GFSRRESTORE_USF(b, v)				\
								SET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_USF, v)
#define	SET_GS0_GFSRRESTORE_SMCF(b, v)				\
								SET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_SMCF, v)
#define	SET_GS0_GFSRRESTORE_UCBF(b, v)				\
								SET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_UCBF, v)
#define	SET_GS0_GFSRRESTORE_UCIF(b, v)				\
								SET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_UCIF, v)
#define	SET_GS0_GFSRRESTORE_CAF(b, v)				\
								SET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_CAF, v)
#define	SET_GS0_GFSRRESTORE_EF(b, v)					\
								SET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_EF, v)
#define	SET_GS0_GFSRRESTORE_PF(b, v)					\
								SET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_PF, v)
#define	SET_GS0_GFSRRESTORE_MULTI(b, v)				\
								SET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_MULTI, v)

/* --- GS0_GFSYNR0 --- */

#define	SET_GS0_GFSYNR0_WNR(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_GFSYNR0, GS0_GFSYNR0_WNR, v)
#define	SET_GS0_GFSYNR0_PNU(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_GFSYNR0, GS0_GFSYNR0_PNU, v)
#define	SET_GS0_GFSYNR0_IND(b, v)						\
								SET_GLOBAL_FIELD(b, GS0_GFSYNR0, GS0_GFSYNR0_IND, v)
#define	SET_GS0_GFSYNR0_NSSTATE(b, v)				\
								SET_GLOBAL_FIELD(b, GS0_GFSYNR0, GS0_GFSYNR0_NSSTATE, v)
#define	SET_GS0_GFSYNR0_NSATTR(b, v)					\
								SET_GLOBAL_FIELD(b, GS0_GFSYNR0, GS0_GFSYNR0_NSATTR, v)
#define	SET_GS0_GFSYNR0_NESTED(b, v)					\
								SET_GLOBAL_FIELD(b, GS0_GFSYNR0, GS0_GFSYNR0_NESTED, v)

/* --- GS0_GFSYNR1 --- */

#define	SET_GS0_GFSYNR1_STREAMID(b, v)				\
								SET_GLOBAL_FIELD(b, GS0_GFSYNR1, GS0_GFSYNR1_STREAMID, v)
#define	SET_GS0_GFSYNR1_SSDINDEX(b, v)				\
								SET_GLOBAL_FIELD(b, GS0_GFSYNR1, GS0_GFSYNR1_SSDINDEX, v)

/* --- GS0_SMR_N --- */

#define	SET_GS0_SMR_N_ID(b, n, v)						\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_SMR_N), GS0_SMR_N_ID, v)
#define	SET_GS0_SMR_N_MASK(b, n, v)					\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_SMR_N), GS0_SMR_N_MASK, v)
#define	SET_GS0_SMR_N_VALID(b, n, v)					\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_SMR_N), GS0_SMR_N_VALID, v)

/* --- GS0_S2CR_N --- */

/* type 00 */

#define	SET_GS0_S2CR_N_00_CBNDX(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_CBNDX, v)
#define	SET_GS0_S2CR_N_00_SHCFG(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_SHCFG, v)
#define	SET_GS0_S2CR_N_00_MTCFG(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_MTCFG, v)
#define	SET_GS0_S2CR_N_00_MEMATTR(b, n, v)			\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_MEMATTR, v)
#define	SET_GS0_S2CR_N_00_TYPE(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_TYPE, v)
#define	SET_GS0_S2CR_N_00_NSCFG(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_NSCFG, v)
#define	SET_GS0_S2CR_N_00_RACFG(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_RACFG, v)
#define	SET_GS0_S2CR_N_00_WACFG(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_WACFG, v)
#define	SET_GS0_S2CR_N_00_PRIVCFG(b, n, v)			\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_PRIVCFG, v)
#define	SET_GS0_S2CR_N_00_INSTCFG(b, n, v)			\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_INSTCFG, v)
#define	SET_GS0_S2CR_N_00_TRANSIENTCFG(b, n, v)		\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_TRANSIENTCFG, v)

/* type 01 */

#define	SET_GS0_S2CR_N_01_VMID(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_VMID, v)
#define	SET_GS0_S2CR_N_01_SHCFG(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_SHCFG, v)
#define	SET_GS0_S2CR_N_01_MTCFG(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_MTCFG, v)
#define	SET_GS0_S2CR_N_01_MEMATTR(b, n, v)			\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_MEMATTR, v)
#define	SET_GS0_S2CR_N_01_TYPE(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_TYPE, v)
#define	SET_GS0_S2CR_N_01_NSCFG(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_NSCFG, v)
#define	SET_GS0_S2CR_N_01_RACFG(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_RACFG, v)
#define	SET_GS0_S2CR_N_01_WACFG(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_WACFG, v)
#define	SET_GS0_S2CR_N_01_BSU(b, n, v)				\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_BSU, v)
#define	SET_GS0_S2CR_N_01_FB(b, n, v)					\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_FB, v)
#define	SET_GS0_S2CR_N_01_TRANSIENTCFG(b, n, v)		\
								SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_TRANSIENTCFG, v)

/* type 10 */

#define	SET_GS0_S2CR_N_10_TYPE(b, n, v)				\
										SET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_10_TYPE, v)

/* === getter === */

/* --- GS0_CR0 --- */

#define GET_GS0_CR0_CLIENTPD(b)		GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_CLIENTPD)
#define GET_GS0_CR0_GFRE(b)			GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_GFRE)
#define	GET_GS0_CR0_GFIE(b)			GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_GFIE)
#define	GET_GS0_CR0_GCFGFRE(b)						\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_GCFGFRE)
#define	GET_GS0_CR0_GCFGFIE(b)						\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_GCFGFIE)
#define	GET_GS0_CR0_STALLD(b)							\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_STALLD)
#define	GET_GS0_CR0_GSE(b)							\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_GSE)
#define	GET_GS0_CR0_USFCFG(b)							\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_USFCFG)
#define	GET_GS0_CR0_VMIDPNE(b)						\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_VMIDPNE)
#define	GET_GS0_CR0_PTM(b)							\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_PTM)
#define	GET_GS0_CR0_FB(b)								\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_FB)
#define	GET_GS0_CR0_BSU(b)							\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_BSU)
#define	GET_GS0_CR0_MEMATTR(b)						\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_MEMATTR)
#define	GET_GS0_CR0_MTCFG(b)							\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_MTCFG)
#define	GET_GS0_CR0_SMCFCFG(b)						\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_SMCFCFG)
#define	GET_GS0_CR0_SHCFG(b)							\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_SHCFG)
#define	GET_GS0_CR0_RACFG(b)							\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_RACFG)
#define	GET_GS0_CR0_WACFG(b)							\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_WACFG)
#define	GET_GS0_CR0_NSCFG(b)							\
										GET_GLOBAL_FIELD(b, GS0_CR0, GS0_CR0_NSCFG)

/* --- GS0_ACR --- */

#define	GET_GS0_ACR_PREFETCHEN(b)		\
									GET_GLOBAL_FIELD(b, GS0_ACR, GS0_ACR_PREFETCHEN)
#define	GET_GS0_ACR_WC1EN(b)			\
									GET_GLOBAL_FIELD(b, GS0_ACR, GS0_ACR_WC1EN)
#define	GET_GS0_ACR_WC2EN(b)			\
									GET_GLOBAL_FIELD(b, GS0_ACR, GS0_ACR_WC2EN)
#define	GET_GS0_ACR_SMTNMB_TLBEN(b)	\
									GET_GLOBAL_FIELD(b, GS0_ACR, GS0_ACR_SMTNMB_TLBEN)
#define	GET_GS0_ACR_MMUDISB_TLBEN(b)	\
									GET_GLOBAL_FIELD(b, GS0_ACR, GS0_ACR_MMUDISB_TLBEN)
#define	GET_GS0_ACR_S2CRB_TLBEN(b)	\
									GET_GLOBAL_FIELD(b, GS0_ACR, GS0_ACR_S2CRB_TLBEN)

/* --- GS0_IDR0 --- */

#define GET_GS0_IDR0_NUMSMRG(b)		\
									GET_GLOBAL_FIELD(b, GS0_IDR0, GS0_IDR0_NUMSMRG)
#define GET_GS0_IDR0_NUMSIDB(b)		\
									GET_GLOBAL_FIELD(b, GS0_IDR0, GS0_IDR0_NUMSIDB)
#define GET_GS0_IDR0_BTM(b)			\
									GET_GLOBAL_FIELD(b, GS0_IDR0, GS0_IDR0_BTM)
#define GET_GS0_IDR0_CTTW(b)			\
									GET_GLOBAL_FIELD(b, GS0_IDR0, GS0_IDR0_CTTW)
#define GET_GS0_IDR0_NUMRPT(b)		\
									GET_GLOBAL_FIELD(b, GS0_IDR0, GS0_IDR0_NUMRPT)
#define GET_GS0_IDR0_PTFS(b)			\
									GET_GLOBAL_FIELD(b, GS0_IDR0, GS0_IDR0_PTFS)
#define GET_GS0_IDR0_SMS(b)			\
									GET_GLOBAL_FIELD(b, GS0_IDR0, GS0_IDR0_SMS)
#define GET_GS0_IDR0_NTS(b)			\
									GET_GLOBAL_FIELD(b, GS0_IDR0, GS0_IDR0_NTS)
#define GET_GS0_IDR0_S2TS(b)			\
									GET_GLOBAL_FIELD(b, GS0_IDR0, GS0_IDR0_S2TS)
#define GET_GS0_IDR0_S1TS(b)			\
									GET_GLOBAL_FIELD(b, GS0_IDR0, GS0_IDR0_S1TS)
#define GET_GS0_IDR0_SES(b)			\
									GET_GLOBAL_FIELD(b, GS0_IDR0, GS0_IDR0_SES)

/* --- GS0_IDR1 --- */

#define	GET_GS0_IDR1_NUMCB(b)			\
									GET_GLOBAL_FIELD(b, GS0_IDR1, GS0_IDR1_NUMCB)
#define	GET_GS0_IDR1_NUMSSDNDXB(b)	\
									GET_GLOBAL_FIELD(b, GS0_IDR1, GS0_IDR1_NUMSSDNDXB)
#define	GET_GS0_IDR1_SSDTP(b)			\
									GET_GLOBAL_FIELD(b, GS0_IDR1, GS0_IDR1_SSDTP)
#define	GET_GS0_IDR1_SMCD(b)			\
									GET_GLOBAL_FIELD(b, GS0_IDR1, GS0_IDR1_SMCD)
#define	GET_GS0_IDR1_NUMS2CB(b)		\
									GET_GLOBAL_FIELD(b, GS0_IDR1, GS0_IDR1_NUMS2CB)
#define	GET_GS0_IDR1_NUMPAGENDXB(b)	\
									GET_GLOBAL_FIELD(b, GS0_IDR1, GS0_IDR1_NUMPAGENDXB)
#define	GET_GS0_IDR1_PAGESIZE(b)		\
									GET_GLOBAL_FIELD(b, GS0_IDR1, GS0_IDR1_PAGESIZE)

/* --- GS0_GFSR / GS0_GFSRRESTORE --- */

#define GET_GS0_GFSR_ICF(b)			GET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_ICF)
#define	GET_GS0_GFSR_USF(b)			GET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_USF)
#define	GET_GS0_GFSR_SMCF(b)			GET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_SMCF)
#define	GET_GS0_GFSR_UCBF(b)			GET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_UCBF)
#define	GET_GS0_GFSR_UCIF(b)			GET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_UCIF)
#define	GET_GS0_GFSR_CAF(b)			GET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_CAF)
#define	GET_GS0_GFSR_EF(b)			GET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_EF)
#define	GET_GS0_GFSR_PF(b)			GET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_PF)
#define	GET_GS0_GFSR_MULTI(b)			GET_GLOBAL_FIELD(b, GS0_GFSR, GS0_GFSR_MULTI)

#define	GET_GS0_GFSRRESTORE_ICF(b)	\
					GET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_ICF)
#define	GET_GS0_GFSRRESTORE_USF(b)	\
					GET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_USF)
#define	GET_GS0_GFSRRESTORE_SMCF(b)	\
					GET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_SMCF)
#define	GET_GS0_GFSRRESTORE_UCBF(b)	\
					GET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_UCBF)
#define	GET_GS0_GFSRRESTORE_UCIF(b)	\
					GET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_UCIF)
#define	GET_GS0_GFSRRESTORE_CAF(b)	\
					GET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_CAF)
#define	GET_GS0_GFSRRESTORE_EF(b)		\
					GET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_EF)
#define	GET_GS0_GFSRRESTORE_PF(b)		\
					GET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_PF)
#define	GET_GS0_GFSRRESTORE_MULTI(b)	\
					GET_GLOBAL_FIELD(b, GS0_GFSRRESTORE, GS0_GFSRRESTORE_MULTI)

/* --- GS0_GFSYNR0 --- */

#define	GET_GS0_GFSYNR0_WNR(b)		\
					GET_GLOBAL_FIELD(b, GS0_GFSYNR0, GS0_GFSYNR0_WNR)
#define	GET_GS0_GFSYNR0_PNU(b)		\
					GET_GLOBAL_FIELD(b, GS0_GFSYNR0, GS0_GFSYNR0_PNU)
#define	GET_GS0_GFSYNR0_IND(b)		\
					GET_GLOBAL_FIELD(b, GS0_GFSYNR0, GS0_GFSYNR0_IND)
#define	GET_GS0_GFSYNR0_NSSTATE(b)	\
					GET_GLOBAL_FIELD(b, GS0_GFSYNR0, GS0_GFSYNR0_NSSTATE)
#define	GET_GS0_GFSYNR0_NSATTR(b)		\
					GET_GLOBAL_FIELD(b, GS0_GFSYNR0, GS0_GFSYNR0_NSATTR)
#define	GET_GS0_GFSYNR0_NESTED(b)		\
					GET_GLOBAL_FIELD(b, GS0_GFSYNR0, GS0_GFSYNR0_NESTED)

/* --- GS0_GFSYNR1 --- */

#define	GET_GS0_GFSYNR1_STREAMID(b)	\
					GET_GLOBAL_FIELD(b, GS0_GFSYNR1, GS0_GFSYNR1_STREAMID)
#define	GET_GS0_GFSYNR1_SSDINDEX(b)	\
					GET_GLOBAL_FIELD(b, GS0_GFSYNR1, GS0_GFSYNR1_SSDINDEX)

/* --- GS0_TLBGSTATUS --- */

#define	GET_GS0_TLBGSTATUS_GSACTIVE(b)	\
					GET_GLOBAL_FIELD(b, GS0_TLBGSTATUS, GS0_TLBGSTATUS_GSACTIVE)

/* --- GS0_SMR_N --- */

#define	GET_GS0_SMR_N_ID(b, n)			\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_SMR_N), GS0_SMR_N_ID)
#define	GET_GS0_SMR_N_MASK(b, n)			\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_SMR_N), GS0_SMR_N_MASK)
#define	GET_GS0_SMR_N_VALID(b, n)			\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_SMR_N), GS0_SMR_N_VALID)

/* --- GS0_S2CR_N --- */

/* type 00 */

#define	GET_GS0_S2CR_N_00_CBNDX(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_CBNDX)
#define	GET_GS0_S2CR_N_00_SHCFG(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_SHCFG)
#define	GET_GS0_S2CR_N_00_MTCFG(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_MTCFG)
#define	GET_GS0_S2CR_N_00_MEMATTR(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_MEMATTR)
#define	GET_GS0_S2CR_N_00_TYPE(b, n)					\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_TYPE)
#define	GET_GS0_S2CR_N_00_NSCFG(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_NSCFG)
#define	GET_GS0_S2CR_N_00_RACFG(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_RACFG)
#define	GET_GS0_S2CR_N_00_WACFG(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_WACFG)
#define	GET_GS0_S2CR_N_00_PRIVCFG(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_PRIVCFG)
#define	GET_GS0_S2CR_N_00_INSTCFG(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_INSTCFG)
#define	GET_GS0_S2CR_N_00_TRANSIENTCFG(b, n)		\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_00_TRANSIENTCFG)

/* type 01 */

#define	GET_GS0_S2CR_N_01_VMID(b, n)					\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_VMID)
#define	GET_GS0_S2CR_N_01_SHCFG(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_SHCFG)
#define	GET_GS0_S2CR_N_01_MTCFG(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_MTCFG)
#define	GET_GS0_S2CR_N_01_MEMATTR(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_MEMATTR)
#define	GET_GS0_S2CR_N_01_TYPE(b, n)					\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_TYPE)
#define	GET_GS0_S2CR_N_01_NSCFG(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_NSCFG)
#define	GET_GS0_S2CR_N_01_RACFG(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_RACFG)
#define	GET_GS0_S2CR_N_01_WACFG(b, n)				\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_WACFG)
#define	GET_GS0_S2CR_N_01_BSU(b, n)					\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_BSU)
#define	GET_GS0_S2CR_N_01_FB(b, n)					\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_FB)
#define	GET_GS0_S2CR_N_01_TRANSIENTCFG(b, n)		\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_01_TRANSIENTCFG)

/* type 10 */

#define	GET_GS0_S2CR_N_10_TYPE(b, n)					\
					GET_GLOBAL_FIELD(b, (n<<2)|(GS0_S2CR_N), GS0_S2CR_N_10_TYPE)

/* === Global space 1 register ====================================== */

/* === setter === */

/* --- GS1_CBAR_N --- */

#define	SET_GS1_CBAR_N_VMID(b, n, v)					\
					SET_GLOBAL_FIELD(b, (n<<2)|(GS1_CBAR_N), GS1_CBAR_N_VMID, v)
#define SET_GS1_CBAR_N_TYPE(b, n, v)					\
					SET_GLOBAL_FIELD(b, (n<<2)|(GS1_CBAR_N), GS1_CBAR_N_TYPE, v)
#define SET_GS1_CBAR_N_IRPTNDX(b, n, v)				\
					SET_GLOBAL_FIELD(b, (n<<2)|(GS1_CBAR_N), GS1_CBAR_N_IRPTNDX, v)

/* --- GS1_CBFRSYNRA_N --- */

#define	SET_GS1_CBFRSYNRA_N_STREAMID(b, n, v)		\
					SET_GLOBAL_FIELD(b, (n<<2)|(GS1_CBFRSYNRA_N), GS1_CBFRSYNRA_N_STREAMID, v)
#define	SET_GS1_CBFRSYNRA_N_SSDINDEX(b, n, v)		\
					SET_GLOBAL_FIELD(b, (n<<2)|(GS1_CBFRSYNRA_N), GS1_CBFRSYNRA_N_SSDINDEX, v)

/* === Integration register ========================================= */

/* === setter === */

/* --- ITCTRL --- */

#define	SET_ITCTRL_MODE(b, v)							SET_GLOBAL_FIELD(b, ITCTRL, ITCTRL_MODE, v)

/* --- ITOP --- */

#define	SET_ITOP_SPINDEN(b, v)						SET_GLOBAL_FIELD(b, ITOP, ITOP_SPINDEN, v)

/* === Translation context bank register =============================== */

/* === setter === */

/* --- CB_SCTLR --- */

#define SET_CB_SCTLR_M(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_M, v)
#define SET_CB_SCTLR_TRE(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_TRE, v)
#define SET_CB_SCTLR_AFE(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_AFE, v)
#define SET_CB_SCTLR_AFFD(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_AFFD, v)
#define SET_CB_SCTLR_E(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_E, v)
#define SET_CB_SCTLR_CFRE(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_CFRE, v)
#define SET_CB_SCTLR_CFIE(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_CFIE, v)
#define SET_CB_SCTLR_CFCFG(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_CFCFG, v)
#define SET_CB_SCTLR_HUPCF(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_HUPCF, v)
#define SET_CB_SCTLR_PTW(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_PTW, v)
#define SET_CB_SCTLR_BSU(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_BSU, v)
#define SET_CB_SCTLR_MEMATTR(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_MEMATTR, v)
#define SET_CB_SCTLR_FB(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_FB, v)
#define SET_CB_SCTLR_SHCFG(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_SHCFG, v)
#define SET_CB_SCTLR_RACFG(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_RACFG, v)
#define SET_CB_SCTLR_WACFG(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_SCTLR, CB_SCTLR_WACFG, v)

/* --- CB_TTBCR --- */

#define SET_CB_TTBCR_T0SZ(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_TTBCR, CB_TTBCR_T0SZ, v)
#define SET_CB_TTBCR_S(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_TTBCR, CB_TTBCR_S, v)
#define SET_CB_TTBCR_SL0(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_TTBCR, CB_TTBCR_SL0, v)
#define SET_CB_TTBCR_IRGN0(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_TTBCR, CB_TTBCR_IRGN0, v)
#define SET_CB_TTBCR_ORGN0(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_TTBCR, CB_TTBCR_ORGN0, v)
#define SET_CB_TTBCR_SH0(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_TTBCR, CB_TTBCR_SH0, v)
#define SET_CB_TTBCR_EAE(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_TTBCR, CB_TTBCR_EAE, v)

/* --- CB_FSR / CB_FSRRESTORE --- */

#define SET_CB_FSR_TF(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSR, CB_FSR_TF, v)
#define SET_CB_FSR_AFF(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSR, CB_FSR_AFF, v)
#define SET_CB_FSR_PF(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSR, CB_FSR_PF, v)
#define SET_CB_FSR_EF(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSR, CB_FSR_EF, v)
#define SET_CB_FSR_TLBMCF(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_FSR, CB_FSR_TLBMCF, v)
#define SET_CB_FSR_TLBLKF(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_FSR, CB_FSR_TLBLKF, v)
#define SET_CB_FSR_SS(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSR, CB_FSR_SS, v)
#define SET_CB_FSR_MULTI(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSR, CB_FSR_MULTI, v)

#define SET_CB_FSRRESTORE_TF(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_FSRRESTORE, CB_FSRRESTORE_TF, v)
#define SET_CB_FSRRESTORE_AFF(b, c, v)	\
					SET_CONTEXT_FIELD(b, c, CB_FSRRESTORE, CB_FSRRESTORE_AFF, v)
#define SET_CB_FSRRESTORE_PF(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_FSRRESTORE, CB_FSRRESTORE_PF, v)
#define SET_CB_FSRRESTORE_EF(b, c, v)	 	\
					SET_CONTEXT_FIELD(b, c, CB_FSRRESTORE, CB_FSRRESTORE_EF, v)
#define SET_CB_FSRRESTORE_TLBMCF(b, c, v)	\
					SET_CONTEXT_FIELD(b, c, CB_FSRRESTORE, CB_FSRRESTORE_TLBMCF, v)
#define SET_CB_FSRRESTORE_TLBLKF(b, c, v)	\
					SET_CONTEXT_FIELD(b, c, CB_FSRRESTORE, CB_FSRRESTORE_TLBLKF, v)
#define SET_CB_FSRRESTORE_SS(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSRRESTORE, CB_FSRRESTORE_SS, v)
#define SET_CB_FSRRESTORE_MULTI(b, c, v)		\
					SET_CONTEXT_FIELD(b, c, CB_FSRRESTORE, CB_FSRRESTORE_MULTI, v)

/* --- CB_FSYNR0 --- */

#define SET_CB_FSYNR0_PLVL(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSYNR0, CB_FSYNR0_PLVL, v)
#define SET_CB_FSYNR0_WNR(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSYNR0, CB_FSYNR0_WNR, v)
#define SET_CB_FSYNR0_PNU(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSYNR0, CB_FSYNR0_PNU, v)
#define SET_CB_FSYNR0_IND(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSYNR0, CB_FSYNR0_IND, v)
#define SET_CB_FSYNR0_NSSTATE(b, c, v)		\
					SET_CONTEXT_FIELD(b, c, CB_FSYNR0, CB_FSYNR0_NSSTATE, v)
#define SET_CB_FSYNR0_NSATTR(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSYNR0, CB_FSYNR0_NSATTR, v)
#define SET_CB_FSYNR0_PTWF(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSYNR0, CB_FSYNR0_PTWF, v)
#define SET_CB_FSYNR0_AFR(b, c, v)	 		\
					SET_CONTEXT_FIELD(b, c, CB_FSYNR0, CB_FSYNR0_AFR, v)
#define SET_CB_FSYNR0_S1CBNDX(b, c, v)		\
					SET_CONTEXT_FIELD(b, c, CB_FSYNR0, CB_FSYNR0_S1CBNDX, v)

/* =========================================================== */
/* =========================================================== */
/* ================= bit field mask / shift ========================== */
/* =========================================================== */
/* =========================================================== */


/* === Global space 0 register ====================================== */

/* --- GS0_CR0 --- */

#define	GS0_CR0_CLIENTPD_MASK							(0x0001)
#define	GS0_CR0_GFRE_MASK								(0x0001)
#define	GS0_CR0_GFIE_MASK								(0x0001)
#define	GS0_CR0_GCFGFRE_MASK							(0x0001)
#define	GS0_CR0_GCFGFIE_MASK							(0x0001)
#define	GS0_CR0_STALLD_MASK							(0x0001)
#define	GS0_CR0_GSE_MASK								(0x0001)
#define	GS0_CR0_USFCFG_MASK							(0x0001)
#define	GS0_CR0_VMIDPNE_MASK							(0x0001)
#define	GS0_CR0_PTM_MASK								(0x0001)
#define	GS0_CR0_FB_MASK								(0x0001)
#define	GS0_CR0_BSU_MASK								(0x0003)
#define	GS0_CR0_MEMATTR_MASK							(0x000F)
#define	GS0_CR0_MTCFG_MASK							(0x0001)
#define	GS0_CR0_SMCFCFG_MASK							(0x0001)
#define	GS0_CR0_SHCFG_MASK							(0x0003)
#define	GS0_CR0_RACFG_MASK							(0x0003)
#define	GS0_CR0_WACFG_MASK							(0x0003)
#define	GS0_CR0_NSCFG_MASK							(0x0003)

#define	GS0_CR0_CLIENTPD_SHIFT						(0)
#define	GS0_CR0_GFRE_SHIFT							(1)
#define	GS0_CR0_GFIE_SHIFT							(2)
#define	GS0_CR0_GCFGFRE_SHIFT							(4)
#define	GS0_CR0_GCFGFIE_SHIFT							(5)
#define	GS0_CR0_STALLD_SHIFT							(8)
#define	GS0_CR0_GSE_SHIFT								(9)
#define	GS0_CR0_USFCFG_SHIFT							(10)
#define	GS0_CR0_VMIDPNE_SHIFT							(11)
#define	GS0_CR0_PTM_SHIFT								(12)
#define	GS0_CR0_FB_SHIFT								(13)
#define	GS0_CR0_BSU_SHIFT								(14)
#define	GS0_CR0_MEMATTR_SHIFT							(16)
#define	GS0_CR0_MTCFG_SHIFT							(20)
#define	GS0_CR0_SMCFCFG_SHIFT							(21)
#define	GS0_CR0_SHCFG_SHIFT							(22)
#define	GS0_CR0_RACFG_SHIFT							(24)
#define	GS0_CR0_WACFG_SHIFT							(26)
#define	GS0_CR0_NSCFG_SHIFT							(28)

#define GS0_CR0_CLIENTPD								(GS0_CR0_CLIENTPD_MASK<<GS0_CR0_CLIENTPD_SHIFT)
#define GS0_CR0_GFRE									(GS0_CR0_GFRE_MASK<<GS0_CR0_GFRE_SHIFT)
#define	GS0_CR0_GFIE									(GS0_CR0_GFIE_MASK<<GS0_CR0_GFIE_SHIFT)
#define	GS0_CR0_GCFGFRE								(GS0_CR0_GCFGFRE_MASK<<GS0_CR0_GCFGFRE_SHIFT)
#define	GS0_CR0_GCFGFIE								(GS0_CR0_GCFGFIE_MASK<<GS0_CR0_GCFGFIE_SHIFT)
#define	GS0_CR0_STALLD									(GS0_CR0_STALLD_MASK<<GS0_CR0_STALLD_SHIFT)
#define	GS0_CR0_GSE									(GS0_CR0_GSE_MASK<<GS0_CR0_GSE_SHIFT)
#define	GS0_CR0_USFCFG									(GS0_CR0_USFCFG_MASK<<GS0_CR0_USFCFG_SHIFT)
#define	GS0_CR0_VMIDPNE								(GS0_CR0_VMIDPNE_MASK<<GS0_CR0_VMIDPNE_SHIFT)
#define	GS0_CR0_PTM									(GS0_CR0_PTM_MASK<<GS0_CR0_PTM_SHIFT)
#define	GS0_CR0_FB										(GS0_CR0_FB_MASK<<GS0_CR0_FB_SHIFT)
#define	GS0_CR0_BSU									(GS0_CR0_BSU_MASK<<GS0_CR0_BSU_SHIFT)
#define	GS0_CR0_MEMATTR								(GS0_CR0_MEMATTR_MASK<<GS0_CR0_MEMATTR_SHIFT)
#define	GS0_CR0_MTCFG									(GS0_CR0_MTCFG_MASK<<GS0_CR0_MTCFG_SHIFT)
#define	GS0_CR0_SMCFCFG								(GS0_CR0_SMCFCFG_MASK<<GS0_CR0_SMCFCFG_SHIFT)
#define	GS0_CR0_SHCFG									(GS0_CR0_SHCFG_MASK<<GS0_CR0_SHCFG_SHIFT)
#define	GS0_CR0_RACFG									(GS0_CR0_RACFG_MASK<<GS0_CR0_RACFG_SHIFT)
#define	GS0_CR0_WACFG									(GS0_CR0_WACFG_MASK<<GS0_CR0_WACFG_SHIFT)
#define	GS0_CR0_NSCFG									(GS0_CR0_NSCFG_MASK<<GS0_CR0_NSCFG_SHIFT)

#define	GS0_CR0_CLIENTPD_BIT(v)						((v)<<GS0_CR0_CLIENTPD_SHIFT)
#define GS0_CR0_GFRE_BIT(v)							((v)<<GS0_CR0_GFRE_SHIFT)
#define	GS0_CR0_GFIE_BIT(v)							((v)<<GS0_CR0_GFIE_SHIFT)
#define	GS0_CR0_GCFGFRE_BIT(v)						((v)<<GS0_CR0_GCFGFRE_SHIFT)
#define	GS0_CR0_GCFGFIE_BIT(v)						((v)<<GS0_CR0_GCFGFIE_SHIFT)
#define	GS0_CR0_STALLD_BIT(v)							((v)<<GS0_CR0_STALLD_SHIFT)
#define	GS0_CR0_GSE_BIT(v)							((v)<<GS0_CR0_GSE_SHIFT)
#define	GS0_CR0_USFCFG_BIT(v)							((v)<<GS0_CR0_USFCFG_SHIFT)
#define	GS0_CR0_VMIDPNE_BIT(v)						((v)<<GS0_CR0_VMIDPNE_SHIFT)
#define	GS0_CR0_PTM_BIT(v)							((v)<<GS0_CR0_PTM_SHIFT)
#define	GS0_CR0_FB_BIT(v)								((v)<<GS0_CR0_FB_SHIFT)
#define	GS0_CR0_BSU_BIT(v)							((v)<<GS0_CR0_BSU_SHIFT)
#define	GS0_CR0_MEMATTR_BIT(v)						((v)<<GS0_CR0_MEMATTR_SHIFT)
#define	GS0_CR0_MTCFG_BIT(v)							((v)<<GS0_CR0_MTCFG_SHIFT)
#define	GS0_CR0_SMCFCFG_BIT(v)						((v)<<GS0_CR0_SMCFCFG_SHIFT)
#define	GS0_CR0_SHCFG_BIT(v)							((v)<<GS0_CR0_SHCFG_SHIFT)
#define	GS0_CR0_RACFG_BIT(v)							((v)<<GS0_CR0_RACFG_SHIFT)
#define	GS0_CR0_WACFG_BIT(v)							((v)<<GS0_CR0_WACFG_SHIFT)
#define	GS0_CR0_NSCFG_BIT(v)							((v)<<GS0_CR0_NSCFG_SHIFT)

/* --- GS0_SCR1 --- */

#define	GS0_SCR1_SPMEM_MASK				(0x0001)
#define	GS0_SCR1_SIF_MASK					(0x0001)
#define	GS0_SCR1_GEFRO_MASK				(0x0001)
#define	GS0_SCR1_GASRAE_MASK				(0x0001)
#define	GS0_SCR1_NSNUMIRPTO_MASK			(0x00FF)
#define	GS0_SCR1_NSNUMSMRGO_MASK			(0x00FF)
#define	GS0_SCR1_NSNUMCBO_MASK			(0x00FF)

#define	GS0_SCR1_SPMEM_SHIFT				(27)
#define	GS0_SCR1_SIF_SHIFT				(26)
#define	GS0_SCR1_GEFRO_SHIFT				(25)
#define	GS0_SCR1_GASRAE_SHIFT				(24)
#define	GS0_SCR1_NSNUMIRPTO_SHIFT		(16)
#define	GS0_SCR1_NSNUMSMRGO_SHIFT		(8)
#define	GS0_SCR1_NSNUMCBO_SHIFT			(0)

#define GS0_SCR1_SPMEM						\
						(GS0_SCR1_SPMEM_MASK<<GS0_SCR1_SPMEM_SHIFT)
#define GS0_SCR1_SIF						\
						(GS0_SCR1_SIF_MASK<<GS0_SCR1_SIF_SHIFT)
#define GS0_SCR1_GEFRO						\
						(GS0_SCR1_GEFRO_MASK<<GS0_SCR1_GEFRO_SHIFT)
#define GS0_SCR1_GASRAE					\
						(GS0_SCR1_GASRAE_MASK<<GS0_SCR1_GASRAE_SHIFT)
#define GS0_SCR1_NSNUMIRPTO				\
						(GS0_SCR1_NSNUMIRPTO_MASK<<GS0_SCR1_NSNUMIRPTO_SHIFT)
#define GS0_SCR1_NSNUMSMRGO				\
						(GS0_SCR1_NSNUMSMRGO_MASK<<GS0_SCR1_NSNUMSMRGO_SHIFT)
#define GS0_SCR1_NSNUMCBO					\
						(GS0_SCR1_NSNUMCBO_MASK<<GS0_SCR1_NSNUMCBO_SHIFT)

#define	GS0_SCR1_SPMEM_BIT(v)				((v)<<GS0_SCR1_SPMEM_SHIFT)
#define	GS0_SCR1_SIF_BIT(v)				((v)<<GS0_SCR1_SIF_SHIFT)
#define	GS0_SCR1_GEFRO_BIT(v)				((v)<<GS0_SCR1_GEFRO_SHIFT)
#define	GS0_SCR1_GASRAE_BIT(v)			((v)<<GS0_SCR1_GASRAE_SHIFT)
#define	GS0_SCR1_NSNUMIRPTO_BIT(v)		((v)<<GS0_SCR1_NSNUMIRPTO_SHIFT)
#define	GS0_SCR1_NSNUMSMRGO_BIT(v)		((v)<<GS0_SCR1_NSNUMSMRGO_SHIFT)
#define	GS0_SCR1_NSNUMCBO_BIT(v)			((v)<<GS0_SCR1_NSNUMCBO_SHIFT)

/* --- GS0_ACR --- */

#define	GS0_ACR_PREFETCHEN_MASK						(0x0001)
#define	GS0_ACR_WC1EN_MASK							(0x0001)
#define	GS0_ACR_WC2EN_MASK							(0x0001)
#define	GS0_ACR_SMTNMB_TLBEN_MASK					(0x0001)
#define	GS0_ACR_MMUDISB_TLBEN_MASK					(0x0001)
#define	GS0_ACR_S2CRB_TLBEN_MASK						(0x0001)

#define	GS0_ACR_PREFETCHEN_SHIFT						(0)
#define	GS0_ACR_WC1EN_SHIFT							(1)
#define	GS0_ACR_WC2EN_SHIFT							(2)
#define	GS0_ACR_SMTNMB_TLBEN_SHIFT					(8)
#define	GS0_ACR_MMUDISB_TLBEN_SHIFT					(9)
#define	GS0_ACR_S2CRB_TLBEN_SHIFT					(10)

#define	GS0_ACR_PREFETCHEN							\
					(GS0_ACR_PREFETCHEN_MASK<<GS0_ACR_PREFETCHEN_SHIFT)
#define	GS0_ACR_WC1EN									\
					(GS0_ACR_WC1EN_MASK<<GS0_ACR_WC1EN_SHIFT)
#define	GS0_ACR_WC2EN									\
					(GS0_ACR_WC2EN_MASK<<GS0_ACR_WC2EN_SHIFT)
#define	GS0_ACR_SMTNMB_TLBEN							\
					(GS0_ACR_SMTNMB_TLBEN_MASK<<GS0_ACR_SMTNMB_TLBEN_SHIFT)
#define	GS0_ACR_MMUDISB_TLBEN							\
					(GS0_ACR_MMUDISB_TLBEN_MASK<<GS0_ACR_MMUDISB_TLBEN_SHIFT)
#define	GS0_ACR_S2CRB_TLBEN							\
					(GS0_ACR_S2CRB_TLBEN_MASK<<GS0_ACR_S2CRB_TLBEN_SHIFT)

#define	GS0_ACR_PREFETCHEN_BIT(v)						((v)<<GS0_ACR_PREFETCHEN_SHIFT)
#define	GS0_ACR_WC1EN_BIT(v)							((v)<<GS0_ACR_WC1EN_SHIFT)
#define	GS0_ACR_WC2EN_BIT(v)							((v)<<GS0_ACR_WC2EN_SHIFT)
#define	GS0_ACR_SMTNMB_TLBEN_BIT(v)					((v)<<GS0_ACR_SMTNMB_TLBEN_SHIFT)
#define	GS0_ACR_MMUDISB_TLBEN_BIT(v)					((v)<<GS0_ACR_MMUDISB_TLBEN_SHIFT)
#define	GS0_ACR_S2CRB_TLBEN_BIT(v)					((v)<<GS0_ACR_S2CRB_TLBEN_SHIFT)

/* --- GS0_IDR0 --- */

#define GS0_IDR0_NUMSMRG_MASK							(0x00FF)
#define GS0_IDR0_NUMSIDB_MASK							(0x000F)
#define GS0_IDR0_BTM_MASK								(0x0001)
#define GS0_IDR0_CTTW_MASK							(0x0001)
#define GS0_IDR0_NUMRPT_MASK							(0x00FF)
#define GS0_IDR0_PTFS_MASK							(0x0001)
#define GS0_IDR0_SMS_MASK								(0x0001)
#define GS0_IDR0_NTS_MASK								(0x0001)
#define GS0_IDR0_S2TS_MASK							(0x0001)
#define GS0_IDR0_S1TS_MASK							(0x0001)
#define GS0_IDR0_SES_MASK								(0x0001)

#define GS0_IDR0_NUMSMRG_SHIFT						(0)
#define GS0_IDR0_NUMSIDB_SHIFT						(9)
#define GS0_IDR0_BTM_SHIFT							(13)
#define GS0_IDR0_CTTW_SHIFT							(14)
#define GS0_IDR0_NUMRPT_SHIFT							(16)
#define GS0_IDR0_PTFS_SHIFT							(24)
#define GS0_IDR0_SMS_SHIFT							(27)
#define GS0_IDR0_NTS_SHIFT							(28)
#define GS0_IDR0_S2TS_SHIFT							(29)
#define GS0_IDR0_S1TS_SHIFT							(30)
#define GS0_IDR0_SES_SHIFT							(31)

#define GS0_IDR0_NUMSMRG								(GS0_IDR0_NUMSMRG_MASK<<GS0_IDR0_NUMSMRG_SHIFT)
#define GS0_IDR0_NUMSIDB								(GS0_IDR0_NUMSIDB_MASK<<GS0_IDR0_NUMSIDB_SHIFT)
#define GS0_IDR0_BTM									(GS0_IDR0_BTM_MASK<<GS0_IDR0_BTM_SHIFT)
#define GS0_IDR0_CTTW									(GS0_IDR0_CTTW_MASK<<GS0_IDR0_CTTW_SHIFT)
#define GS0_IDR0_NUMRPT								(GS0_IDR0_NUMRPT_MASK<<GS0_IDR0_NUMRPT_SHIFT)
#define GS0_IDR0_PTFS									(GS0_IDR0_PTFS_MASK<<GS0_IDR0_PTFS_SHIFT)
#define GS0_IDR0_SMS									(GS0_IDR0_SMS_MASK<<GS0_IDR0_SMS_SHIFT)
#define GS0_IDR0_NTS									(GS0_IDR0_NTS_MASK<<GS0_IDR0_NTS_SHIFT)
#define GS0_IDR0_S2TS									(GS0_IDR0_S2TS_MASK<<GS0_IDR0_S2TS_SHIFT)
#define GS0_IDR0_S1TS									(GS0_IDR0_S1TS_MASK<<GS0_IDR0_S1TS_SHIFT)
#define GS0_IDR0_SES									(GS0_IDR0_SES_MASK<<GS0_IDR0_SES_SHIFT)

/* --- GS0_IDR1 --- */

#define	GS0_IDR1_NUMCB_MASK							(0x00FF)
#define	GS0_IDR1_NUMSSDNDXB_MASK						(0x000F)
#define	GS0_IDR1_SSDTP_MASK							(0x0001)
#define	GS0_IDR1_SMCD_MASK							(0x0001)
#define	GS0_IDR1_NUMS2CB_MASK							(0x00FF)
#define	GS0_IDR1_NUMPAGENDXB_MASK					(0x0007)
#define	GS0_IDR1_PAGESIZE_MASK						(0x0001)

#define	GS0_IDR1_NUMCB_SHIFT							(0)
#define	GS0_IDR1_NUMSSDNDXB_SHIFT					(8)
#define	GS0_IDR1_SSDTP_SHIFT							(12)
#define	GS0_IDR1_SMCD_SHIFT							(15)
#define	GS0_IDR1_NUMS2CB_SHIFT						(16)
#define	GS0_IDR1_NUMPAGENDXB_SHIFT					(28)
#define	GS0_IDR1_PAGESIZE_SHIFT						(31)

#define	GS0_IDR1_NUMCB									\
						(GS0_IDR1_NUMCB_MASK<<GS0_IDR1_NUMCB_SHIFT)
#define	GS0_IDR1_NUMSSDNDXB							\
						(GS0_IDR1_NUMSSDNDXB_MASK<<GS0_IDR1_NUMSSDNDXB_SHIFT)
#define	GS0_IDR1_SSDTP									\
						(GS0_IDR1_SSDTP_MASK<<GS0_IDR1_SSDTP_SHIFT)
#define	GS0_IDR1_SMCD									\
						(GS0_IDR1_SMCD_MASK<<GS0_IDR1_SMCD)
#define	GS0_IDR1_NUMS2CB								\
						(GS0_IDR1_NUMS2CB_MASK<<GS0_IDR1_NUMS2CB_SHIFT)
#define	GS0_IDR1_NUMPAGENDXB							\
						(GS0_IDR1_NUMPAGENDXB_MASK<<GS0_IDR1_NUMPAGENDXB_SHIFT)
#define	GS0_IDR1_PAGESIZE								\
						(GS0_IDR1_PAGESIZE_MASK<<GS0_IDR1_PAGESIZE_SHIFT)

/* --- GS0_GFSR / GS0_GFSRRESTORE --- */

#define	GS0_GFSR_ICF_MASK								(0x0001)
#define	GS0_GFSR_USF_MASK								(0x0001)
#define	GS0_GFSR_SMCF_MASK							(0x0001)
#define	GS0_GFSR_UCBF_MASK							(0x0001)
#define	GS0_GFSR_UCIF_MASK							(0x0001)
#define	GS0_GFSR_CAF_MASK								(0x0001)
#define	GS0_GFSR_EF_MASK								(0x0001)
#define	GS0_GFSR_PF_MASK								(0x0001)
#define	GS0_GFSR_MULTI_MASK							(0x0001)

#define	GS0_GFSR_ICF_SHIFT							(0)
#define	GS0_GFSR_USF_SHIFT							(1)
#define	GS0_GFSR_SMCF_SHIFT							(2)
#define	GS0_GFSR_UCBF_SHIFT							(3)
#define	GS0_GFSR_UCIF_SHIFT							(4)
#define	GS0_GFSR_CAF_SHIFT							(5)
#define	GS0_GFSR_EF_SHIFT								(6)
#define	GS0_GFSR_PF_SHIFT								(7)
#define	GS0_GFSR_MULTI_SHIFT							(31)

#define	GS0_GFSR_ICF									(GS0_GFSR_ICF_MASK<<GS0_GFSR_ICF_SHIFT)
#define	GS0_GFSR_USF									(GS0_GFSR_USF_MASK<<GS0_GFSR_USF_SHIFT)
#define	GS0_GFSR_SMCF									(GS0_GFSR_SMCF_MASK<<GS0_GFSR_SMCF_SHIFT)
#define	GS0_GFSR_UCBF									(GS0_GFSR_UCBF_MASK<<GS0_GFSR_UCBF_SHIFT)
#define	GS0_GFSR_UCIF									(GS0_GFSR_UCIF_MASK<<GS0_GFSR_UCIF_SHIFT)
#define	GS0_GFSR_CAF									(GS0_GFSR_CAF_MASK<<GS0_GFSR_CAF_SHIFT)
#define	GS0_GFSR_EF									(GS0_GFSR_EF_MASK<<GS0_GFSR_EF_SHIFT)
#define	GS0_GFSR_PF									(GS0_GFSR_PF_MASK<<GS0_GFSR_PF_SHIFT)
#define	GS0_GFSR_MULTI									(GS0_GFSR_MULTI_MASK<<GS0_GFSR_MULTI_SHIFT)

#define	GS0_GFSRRESTORE_ICF							(GS0_GFSR_ICF_MASK<<GS0_GFSR_ICF_SHIFT)
#define	GS0_GFSRRESTORE_USF							(GS0_GFSR_USF_MASK<<GS0_GFSR_USF_SHIFT)
#define	GS0_GFSRRESTORE_SMCF							(GS0_GFSR_SMCF_MASK<<GS0_GFSR_SMCF_SHIFT)
#define	GS0_GFSRRESTORE_UCBF							(GS0_GFSR_UCBF_MASK<<GS0_GFSR_UCBF_SHIFT)
#define	GS0_GFSRRESTORE_UCIF							(GS0_GFSR_UCIF_MASK<<GS0_GFSR_UCIF_SHIFT)
#define	GS0_GFSRRESTORE_CAF							(GS0_GFSR_CAF_MASK<<GS0_GFSR_CAF_SHIFT)
#define	GS0_GFSRRESTORE_EF							(GS0_GFSR_EF_MASK<<GS0_GFSR_EF_SHIFT)
#define	GS0_GFSRRESTORE_PF							(GS0_GFSR_PF_MASK<<GS0_GFSR_PF_SHIFT)
#define	GS0_GFSRRESTORE_MULTI							(GS0_GFSR_MULTI_MASK<<GS0_GFSR_MULTI_SHIFT)

/* --- GS0_GFSYNR0 --- */

#define	GS0_GFSYNR0_WNR_MASK							(0x0001)
#define	GS0_GFSYNR0_PNU_MASK							(0x0001)
#define	GS0_GFSYNR0_IND_MASK							(0x0001)
#define	GS0_GFSYNR0_NSSTATE_MASK						(0x0001)
#define	GS0_GFSYNR0_NSATTR_MASK						(0x0001)
#define	GS0_GFSYNR0_NESTED_MASK						(0x0001)

#define	GS0_GFSYNR0_WNR_SHIFT							(0)
#define	GS0_GFSYNR0_PNU_SHIFT							(1)
#define	GS0_GFSYNR0_IND_SHIFT							(2)
#define	GS0_GFSYNR0_NSSTATE_SHIFT					(3)
#define	GS0_GFSYNR0_NSATTR_SHIFT						(4)
#define	GS0_GFSYNR0_NESTED_SHIFT						(5)

#define	GS0_GFSYNR0_WNR		(GS0_GFSYNR0_WNR_MASK<<GS0_GFSYNR0_WNR_SHIFT)
#define	GS0_GFSYNR0_PNU		(GS0_GFSYNR0_PNU_MASK<<GS0_GFSYNR0_PNU_SHIFT)
#define	GS0_GFSYNR0_IND		(GS0_GFSYNR0_IND_MASK<<GS0_GFSYNR0_IND_SHIFT)
#define	GS0_GFSYNR0_NSSTATE	(GS0_GFSYNR0_NSSTATE_MASK<<GS0_GFSYNR0_NSSTATE)
#define	GS0_GFSYNR0_NSATTR	(GS0_GFSYNR0_NSATTR_MASK<<GS0_GFSYNR0_NSATTR_SHIFT)
#define	GS0_GFSYNR0_NESTED	(GS0_GFSYNR0_NESTED_MASK<<GS0_GFSYNR0_NESTED_SHIFT)

/* --- GS0_GFSYNR1 --- */

#define	GS0_GFSYNR1_STREAMID_MASK					(0x7FFF)
#define	GS0_GFSYNR1_SSDINDEX_MASK					(0x7FFF)

#define	GS0_GFSYNR1_STREAMID_SHIFT					(0)
#define	GS0_GFSYNR1_SSDINDEX_SHIFT					(16)

#define	GS0_GFSYNR1_STREAMID							\
						(GS0_GFSYNR1_STREAMID_MASK<<GS0_GFSYNR1_STREAMID_SHIFT)
#define	GS0_GFSYNR1_SSDINDEX							\
						(GS0_GFSYNR1_SSDINDEX_MASK<<GS0_GFSYNR1_SSDINDEX_SHIFT)

/* --- GS0_TLBGSTATUS --- */

#define	GS0_TLBGSTATUS_GSACTIVE_MASK					(0x0001)

#define	GS0_TLBGSTATUS_GSACTIVE_SHIFT				(0)

#define	GS0_TLBGSTATUS_GSACTIVE						\
						(GS0_TLBGSTATUS_GSACTIVE_MASK<<GS0_TLBGSTATUS_GSACTIVE_SHIFT)

/* --- GS0_SMR_N --- */

#define	GS0_SMR_N_ID_MASK								(0x7FFF)
#define	GS0_SMR_N_MASK_MASK							(0x7FFF)
#define	GS0_SMR_N_VALID_MASK							(0x0001)

#define	GS0_SMR_N_ID_SHIFT							(0)
#define	GS0_SMR_N_MASK_SHIFT							(16)
#define	GS0_SMR_N_VALID_SHIFT							(31)

#define	GS0_SMR_N_ID									(GS0_SMR_N_ID_MASK<<GS0_SMR_N_ID_SHIFT)
#define	GS0_SMR_N_MASK									(GS0_SMR_N_MASK_MASK<<GS0_SMR_N_MASK_SHIFT)
#define	GS0_SMR_N_VALID								(GS0_SMR_N_VALID_MASK<<GS0_SMR_N_VALID_SHIFT)

#define	GS0_SMR_N_ID_BIT(v)							((v)<<GS0_SMR_N_ID_SHIFT)
#define	GS0_SMR_N_MASK_BIT(v)							((v)<<GS0_SMR_N_MASK_SHIFT)
#define	GS0_SMR_N_VALID_BIT(v)						((v)<<GS0_SMR_N_VALID_SHIFT)

/* --- GS0_S2CR_N --- */

/* type 00 */

#define	GS0_S2CR_N_00_CBNDX_MASK						(0x00FF)
#define	GS0_S2CR_N_00_SHCFG_MASK						(0x0003)
#define	GS0_S2CR_N_00_MTCFG_MASK						(0x0001)
#define	GS0_S2CR_N_00_MEMATTR_MASK					(0x0001)
#define	GS0_S2CR_N_00_TYPE_MASK						(0x000F)
#define	GS0_S2CR_N_00_NSCFG_MASK						(0x0003)
#define	GS0_S2CR_N_00_RACFG_MASK						(0x0003)
#define	GS0_S2CR_N_00_WACFG_MASK						(0x0003)
#define	GS0_S2CR_N_00_PRIVCFG_MASK					(0x0003)
#define	GS0_S2CR_N_00_INSTCFG_MASK					(0x0003)
#define	GS0_S2CR_N_00_TRANSIENTCFG_MASK				(0x0003)

#define	GS0_S2CR_N_00_CBNDX_SHIFT					(0)
#define	GS0_S2CR_N_00_SHCFG_SHIFT					(8)
#define	GS0_S2CR_N_00_MTCFG_SHIFT					(11)
#define	GS0_S2CR_N_00_MEMATTR_SHIFT					(12)
#define	GS0_S2CR_N_00_TYPE_SHIFT						(16)
#define	GS0_S2CR_N_00_NSCFG_SHIFT					(18)
#define	GS0_S2CR_N_00_RACFG_SHIFT					(20)
#define	GS0_S2CR_N_00_WACFG_SHIFT					(22)
#define	GS0_S2CR_N_00_PRIVCFG_SHIFT					(24)
#define	GS0_S2CR_N_00_INSTCFG_SHIFT					(26)
#define	GS0_S2CR_N_00_TRANSIENTCFG_SHIFT			(28)

#define	GS0_S2CR_N_00_CBNDX							\
					(GS0_S2CR_N_00_CBNDX_MASK<<GS0_S2CR_N_00_CBNDX_SHIFT)
#define	GS0_S2CR_N_00_SHCFG							\
					(GS0_S2CR_N_00_SHCFG_MASK<<GS0_S2CR_N_00_SHCFG_SHIFT)
#define	GS0_S2CR_N_00_MTCFG							\
					(GS0_S2CR_N_00_MTCFG_MASK<<GS0_S2CR_N_00_MTCFG_SHIFT)
#define	GS0_S2CR_N_00_MEMATTR							\
					(GS0_S2CR_N_00_MEMATTR_MASK<<GS0_S2CR_N_00_MEMATTR_SHIFT)
#define	GS0_S2CR_N_00_TYPE							\
					(GS0_S2CR_N_00_TYPE_MASK<<GS0_S2CR_N_00_TYPE_SHIFT)
#define	GS0_S2CR_N_00_NSCFG							\
					(GS0_S2CR_N_00_NSCFG_MASK<<GS0_S2CR_N_00_NSCFG_SHIFT)
#define	GS0_S2CR_N_00_RACFG							\
					(GS0_S2CR_N_00_RACFG_MASK<<GS0_S2CR_N_00_RACFG_SHIFT)
#define	GS0_S2CR_N_00_WACFG							\
					(GS0_S2CR_N_00_WACFG_MASK<<GS0_S2CR_N_00_WACFG_SHIFT)
#define	GS0_S2CR_N_00_PRIVCFG							\
					(GS0_S2CR_N_00_PRIVCFG_MASK<<GS0_S2CR_N_00_PRIVCFG_SHIFT)
#define	GS0_S2CR_N_00_INSTCFG							\
					(GS0_S2CR_N_00_INSTCFG_MASK<<GS0_S2CR_N_00_INSTCFG_SHIFT)
#define	GS0_S2CR_N_00_TRANSIENTCFG					\
					(GS0_S2CR_N_00_TRANSIENTCFG_MASK<<GS0_S2CR_N_00_TRANSIENTCFG_SHIFT)

#define	GS0_S2CR_N_00_CBNDX_BIT(v)					\
					((v)<<GS0_S2CR_N_00_CBNDX_SHIFT)
#define	GS0_S2CR_N_00_SHCFG_BIT(v)					\
					((v)<<GS0_S2CR_N_00_SHCFG_SHIFT)
#define	GS0_S2CR_N_00_MTCFG_BIT(v)					\
					((v)<<GS0_S2CR_N_00_MTCFG_SHIFT)
#define	GS0_S2CR_N_00_MEMATTR_BIT(v)					\
					((v)<<GS0_S2CR_N_00_MEMATTR_SHIFT)
#define	GS0_S2CR_N_00_TYPE_BIT(v)						\
					((v)<<GS0_S2CR_N_00_TYPE_SHIFT)
#define	GS0_S2CR_N_00_NSCFG_BIT(v)					\
					((v)<<GS0_S2CR_N_00_NSCFG_SHIFT)
#define	GS0_S2CR_N_00_RACFG_BIT(v)					\
					((v)<<GS0_S2CR_N_00_RACFG_SHIFT)
#define	GS0_S2CR_N_00_WACFG_BIT(v)					\
					((v)<<GS0_S2CR_N_00_WACFG_SHIFT)
#define	GS0_S2CR_N_00_PRIVCFG_BIT(v)					\
					((v)<<GS0_S2CR_N_00_PRIVCFG_SHIFT)
#define	GS0_S2CR_N_00_INSTCFG_BIT(v)					\
					((v)<<GS0_S2CR_N_00_INSTCFG_SHIFT)
#define	GS0_S2CR_N_00_TRANSIENTCFG_BIT(v)			\
					((v)<<GS0_S2CR_N_00_TRANSIENTCFG_SHIFT)

/* type 01 */

#define	GS0_S2CR_N_01_VMID_MASK						(0x00FF)
#define	GS0_S2CR_N_01_SHCFG_MASK						(0x0003)
#define	GS0_S2CR_N_01_MTCFG_MASK						(0x0001)
#define	GS0_S2CR_N_01_MEMATTR_MASK					(0x000F)
#define	GS0_S2CR_N_01_TYPE_MASK						(0x0003)
#define	GS0_S2CR_N_01_NSCFG_MASK						(0x0003)
#define	GS0_S2CR_N_01_RACFG_MASK						(0x0003)
#define	GS0_S2CR_N_01_WACFG_MASK						(0x0003)
#define	GS0_S2CR_N_01_BSU_MASK						(0x0003)
#define	GS0_S2CR_N_01_FB_MASK							(0x0001)
#define	GS0_S2CR_N_01_TRANSIENTCFG_MASK				(0x0002)

#define	GS0_S2CR_N_01_VMID_SHIFT						(0)
#define	GS0_S2CR_N_01_SHCFG_SHIFT					(8)
#define	GS0_S2CR_N_01_MTCFG_SHIFT					(11)
#define	GS0_S2CR_N_01_MEMATTR_SHIFT					(12)
#define	GS0_S2CR_N_01_TYPE_SHIFT						(16)
#define	GS0_S2CR_N_01_NSCFG_SHIFT					(18)
#define	GS0_S2CR_N_01_RACFG_SHIFT					(20)
#define	GS0_S2CR_N_01_WACFG_SHIFT					(22)
#define	GS0_S2CR_N_01_BSU_SHIFT						(24)
#define	GS0_S2CR_N_01_FB_SHIFT						(26)
#define	GS0_S2CR_N_01_TRANSIENTCFG_SHIFT			(28)

#define	GS0_S2CR_N_01_VMID							\
					(GS0_S2CR_N_01_VMID_MASK<<GS0_S2CR_N_01_VMID_SHIFT)
#define	GS0_S2CR_N_01_SHCFG							\
					(GS0_S2CR_N_01_SHCFG_MASK<<GS0_S2CR_N_01_SHCFG_SHIFT)
#define	GS0_S2CR_N_01_MTCFG							\
					(GS0_S2CR_N_01_MTCFG_MASK<<GS0_S2CR_N_01_MTCFG_SHIFT)
#define	GS0_S2CR_N_01_MEMATTR							\
					(GS0_S2CR_N_01_MEMATTR_MASK<<GS0_S2CR_N_01_MEMATTR_SHIFT)
#define	GS0_S2CR_N_01_TYPE							\
					(GS0_S2CR_N_01_TYPE_MASK<<GS0_S2CR_N_01_TYPE_SHIFT)
#define	GS0_S2CR_N_01_NSCFG							\
					(GS0_S2CR_N_01_NSCFG_MASK<<GS0_S2CR_N_01_NSCFG_SHIFT)
#define	GS0_S2CR_N_01_RACFG							\
					(GS0_S2CR_N_01_RACFG_MASK<<GS0_S2CR_N_01_RACFG_SHIFT)
#define	GS0_S2CR_N_01_WACFG							\
					(GS0_S2CR_N_01_WACFG_MASK<<GS0_S2CR_N_01_WACFG_SHIFT)
#define	GS0_S2CR_N_01_BSU								\
					(GS0_S2CR_N_01_BSU_MASK<<GS0_S2CR_N_01_BSU_SHIFT)
#define	GS0_S2CR_N_01_FB								\
					(GS0_S2CR_N_01_FB_MASK<<GS0_S2CR_N_01_FB_SHIFT)
#define	GS0_S2CR_N_01_TRANSIENTCFG					\
					(GS0_S2CR_N_01_TRANSIENTCFG_MASK<<GS0_S2CR_N_01_TRANSIENTCFG_SHIFT)

/* type 10 */

#define	GS0_S2CR_N_10_TYPE_MASK						\
					(0x0003)

#define	GS0_S2CR_N_10_TYPE_SHIF						\
					(16)

#define	GS0_S2CR_N_10_TYPE							\
					(GS0_S2CR_N_10_TYPE_MASK<<GS0_S2CR_N_10_TYPE_SHIF)

#define	GS0_S2CR_N_10_TYPE_BIT(v)						\
					((v)<<GS0_S2CR_N_10_TYPE_SHIF)

/* === Global space 1 register ====================================== */

/* --- GS1_CBAR_N --- */

#define GS1_CBAR_N_VMID_MASK							(0x00FF)
#define GS1_CBAR_N_TYPE_MASK							(0x0003)
#define GS1_CBAR_N_IRPTNDX_MASK						(0x00FF)

#define GS1_CBAR_N_VMID_SHIFT							(0)
#define GS1_CBAR_N_TYPE_SHIFT							(16)
#define GS1_CBAR_N_IRPTNDX_SHIFT						(24)

#define GS1_CBAR_N_VMID								\
					(GS1_CBAR_N_VMID_MASK<<GS1_CBAR_N_VMID_SHIFT)
#define GS1_CBAR_N_TYPE								\
					(GS1_CBAR_N_TYPE_MASK<<GS1_CBAR_N_TYPE_SHIFT)
#define GS1_CBAR_N_IRPTNDX							\
					(GS1_CBAR_N_IRPTNDX_MASK<<GS1_CBAR_N_IRPTNDX_SHIFT)

#define GS1_CBAR_N_VMID_BIT(v)						\
					((v)<<GS1_CBAR_N_VMID_SHIFT)
#define GS1_CBAR_N_TYPE_BIT(v)						\
					((v)<<GS1_CBAR_N_TYPE_SHIFT)
#define GS1_CBAR_N_IRPTNDX_BIT(v)						\
					((v)<<GS1_CBAR_N_IRPTNDX_SHIFT)

/* --- GS1_CBFRSYNRA_N --- */

#define	GS1_CBFRSYNRA_N_STREAMID_MASK				(0x7FFF)
#define	GS1_CBFRSYNRA_N_SSDINDEX_MASK				(0x7FFF)

#define	GS1_CBFRSYNRA_N_STREAMID_SHIFT				(0)
#define	GS1_CBFRSYNRA_N_SSDINDEX_SHIFT				(16)

#define	GS1_CBFRSYNRA_N_STREAMID						\
					(GS1_CBFRSYNRA_N_STREAMID_MASK<<GS1_CBFRSYNRA_N_STREAMID_SHIFT)
#define	GS1_CBFRSYNRA_N_SSDINDEX						\
					(GS1_CBFRSYNRA_N_SSDINDEX_MASK<<GS1_CBFRSYNRA_N_SSDINDEX_SHIFT)

/* === Integration register ========================================= */

/* --- ITCTRL --- */

#define ITCTRL_MODE_MASK								(0x0001)

#define ITCTRL_MODE_SHIFT								(0)

#define ITCTRL_MODE									(ITCTRL_MODE_MASK<<ITCTRL_MODE_SHIFT)

/* --- ITOP --- */

#define ITOP_SPINDEN_MASK								(0x0001)

#define ITOP_SPINDEN_SHIFT							(0)

#define ITOP_SPINDEN									(ITOP_SPINDEN_MASK<<ITOP_SPINDEN_SHIFT)

/* === Translation context bank register ============================== */

#define	ODIN_IOMMU_PERI_CTX_NUM						(4)

/* --- CB_SCTLR --- */

#define CB_SCTLR_M_MASK								(0x0001)
#define CB_SCTLR_TRE_MASK								(0x0001)
#define CB_SCTLR_AFE_MASK								(0x0001)
#define CB_SCTLR_AFFD_MASK							(0x0001)
#define CB_SCTLR_E_MASK								(0x0001)
#define CB_SCTLR_CFRE_MASK							(0x0001)
#define CB_SCTLR_CFIE_MASK							(0x0001)
#define CB_SCTLR_CFCFG_MASK							(0x0001)
#define CB_SCTLR_HUPCF_MASK							(0x0001)
#define CB_SCTLR_PTW_MASK								(0x0001)
#define CB_SCTLR_BSU_MASK								(0x0003)
#define CB_SCTLR_MEMATTR_MASK							(0x000F)
#define CB_SCTLR_FB_MASK								(0x0001)
#define CB_SCTLR_SHCFG_MASK							(0x0003)
#define CB_SCTLR_RACFG_MASK							(0x0003)
#define CB_SCTLR_WACFG_MASK							(0x0003)

#define CB_SCTLR_M_SHIFT								(0)
#define CB_SCTLR_TRE_SHIFT							(1)
#define CB_SCTLR_AFE_SHIFT							(2)
#define CB_SCTLR_AFFD_SHIFT							(3)
#define CB_SCTLR_E_SHIFT								(4)
#define CB_SCTLR_CFRE_SHIFT							(5)
#define CB_SCTLR_CFIE_SHIFT							(6)
#define CB_SCTLR_CFCFG_SHIFT							(7)
#define CB_SCTLR_HUPCF_SHIFT							(8)
#define CB_SCTLR_PTW_SHIFT							(13)
#define CB_SCTLR_BSU_SHIFT							(14)
#define CB_SCTLR_MEMATTR_SHIFT						(16)
#define CB_SCTLR_FB_SHIFT								(21)
#define CB_SCTLR_SHCFG_SHIFT							(22)
#define CB_SCTLR_RACFG_SHIFT							(24)
#define CB_SCTLR_WACFG_SHIFT							(26)

#define CB_SCTLR_M										(CB_SCTLR_M_MASK<<CB_SCTLR_M_SHIFT)
#define CB_SCTLR_TRE									(CB_SCTLR_TRE_MASK<<CB_SCTLR_TRE_SHIFT)
#define CB_SCTLR_AFE									(CB_SCTLR_AFE_MASK<<CB_SCTLR_AFE_SHIFT)
#define CB_SCTLR_AFFD									(CB_SCTLR_AFFD_MASK<<CB_SCTLR_AFFD_SHIFT)
#define CB_SCTLR_E										(CB_SCTLR_E_MASK<<CB_SCTLR_E_SHIFT)
#define CB_SCTLR_CFRE									(CB_SCTLR_CFRE_MASK<<CB_SCTLR_CFRE_SHIFT)
#define CB_SCTLR_CFIE									(CB_SCTLR_CFIE_MASK<<CB_SCTLR_CFIE_SHIFT)
#define CB_SCTLR_CFCFG									(CB_SCTLR_CFCFG_MASK<<CB_SCTLR_CFCFG_SHIFT)
#define CB_SCTLR_HUPCF									(CB_SCTLR_HUPCF_MASK<<CB_SCTLR_HUPCF_SHIFT)
#define CB_SCTLR_PTW									(CB_SCTLR_PTW_MASK<<CB_SCTLR_PTW_SHIFT)
#define CB_SCTLR_BSU									(CB_SCTLR_BSU_MASK<<CB_SCTLR_BSU_SHIFT)
#define CB_SCTLR_MEMATTR								(CB_SCTLR_MEMATTR_MASK<<CB_SCTLR_MEMATTR_SHIFT)
#define CB_SCTLR_FB									(CB_SCTLR_FB_MASK<<CB_SCTLR_FB_SHIFT)
#define CB_SCTLR_SHCFG									(CB_SCTLR_SHCFG_MASK<<CB_SCTLR_SHCFG_SHIFT)
#define CB_SCTLR_RACFG									(CB_SCTLR_RACFG_MASK<<CB_SCTLR_RACFG_SHIFT)
#define CB_SCTLR_WACFG									(CB_SCTLR_WACFG_MASK<<CB_SCTLR_WACFG_SHIFT)

#define CB_SCTLR_M_BIT(v)								((v)<<CB_SCTLR_M_SHIFT)
#define CB_SCTLR_TRE_BIT(v)							((v)<<CB_SCTLR_TRE_SHIFT)
#define CB_SCTLR_AFE_BIT(v)							((v)<<CB_SCTLR_AFE_SHIFT)
#define CB_SCTLR_AFFD_BIT(v)							((v)<<CB_SCTLR_AFFD_SHIFT)
#define CB_SCTLR_E_BIT(v)								((v)<<CB_SCTLR_E_SHIFT)
#define CB_SCTLR_CFRE_BIT(v)							((v)<<CB_SCTLR_CFRE_SHIFT)
#define CB_SCTLR_CFIE_BIT(v)							((v)<<CB_SCTLR_CFIE_SHIFT)
#define CB_SCTLR_CFCFG_BIT(v)							((v)<<CB_SCTLR_CFCFG_SHIFT)
#define CB_SCTLR_HUPCF_BIT(v)							((v)<<CB_SCTLR_HUPCF_SHIFT)
#define CB_SCTLR_PTW_BIT(v)							((v)<<CB_SCTLR_PTW_SHIFT)
#define CB_SCTLR_BSU_BIT(v)							((v)<<CB_SCTLR_BSU_SHIFT)
#define CB_SCTLR_MEMATTR_BIT(v)						((v)<<CB_SCTLR_MEMATTR_SHIFT)
#define CB_SCTLR_FB_BIT(v)							((v)<<CB_SCTLR_FB_SHIFT)
#define CB_SCTLR_SHCFG_BIT(v)							((v)<<CB_SCTLR_SHCFG_SHIFT)
#define CB_SCTLR_RACFG_BIT(v)							((v)<<CB_SCTLR_RACFG_SHIFT)
#define CB_SCTLR_WACFG_BIT(v)							((v)<<CB_SCTLR_WACFG_SHIFT)

/* --- CB_TTBCR --- */

#define CB_TTBCR_T0SZ_MASK							(0x000F)
#define CB_TTBCR_S_MASK								(0x0001)
#define CB_TTBCR_SL0_MASK								(0x0003)
#define CB_TTBCR_IRGN0_MASK							(0x0003)
#define CB_TTBCR_ORGN0_MASK							(0x0003)
#define CB_TTBCR_SH0_MASK								(0x0003)
#define CB_TTBCR_EAE_MASK								(0x0001)

#define CB_TTBCR_T0SZ_SHIFT							(0)
#define CB_TTBCR_S_SHIFT								(4)
#define CB_TTBCR_SL0_SHIFT							(6)
#define CB_TTBCR_IRGN0_SHIFT							(8)
#define CB_TTBCR_ORGN0_SHIFT							(10)
#define CB_TTBCR_SH0_SHIFT							(12)
#define CB_TTBCR_EAE_SHIFT							(31)

#define CB_TTBCR_T0SZ									(CB_TTBCR_T0SZ_MASK<<CB_TTBCR_T0SZ_SHIFT)
#define CB_TTBCR_S										(CB_TTBCR_S_MASK<<CB_TTBCR_S_SHIFT)
#define CB_TTBCR_SL0									(CB_TTBCR_SL0_MASK<<CB_TTBCR_SL0_SHIFT)
#define CB_TTBCR_IRGN0									(CB_TTBCR_IRGN0_MASK<<CB_TTBCR_IRGN0_SHIFT)
#define CB_TTBCR_ORGN0									(CB_TTBCR_ORGN0_MASK<<CB_TTBCR_ORGN0_SHIFT)
#define CB_TTBCR_SH0									(CB_TTBCR_SH0_MASK<<CB_TTBCR_SH0_SHIFT)
#define CB_TTBCR_EAE									(CB_TTBCR_EAE_MASK<<CB_TTBCR_EAE_SHIFT)

#define CB_TTBCR_T0SZ_BIT(v)							((v)<<CB_TTBCR_T0SZ_SHIFT)
#define CB_TTBCR_S_BIT(v)								((v)<<CB_TTBCR_S_SHIFT)
#define CB_TTBCR_SL0_BIT(v)							((v)<<CB_TTBCR_SL0_SHIFT)
#define CB_TTBCR_IRGN0_BIT(v)							((v)<<CB_TTBCR_IRGN0_SHIFT)
#define CB_TTBCR_ORGN0_BIT(v)							((v)<<CB_TTBCR_ORGN0_SHIFT)
#define CB_TTBCR_SH0_BIT(v)							((v)<<CB_TTBCR_SH0_SHIFT)
#define CB_TTBCR_EAE_BIT(v)							((v)<<CB_TTBCR_EAE_SHIFT)

/* --- CB_FSR / CB_FSRRESTORE --- */

#define CB_FSR_TF_MASK									(0x0001)
#define CB_FSR_AFF_MASK								(0x0001)
#define CB_FSR_PF_MASK									(0x0001)
#define CB_FSR_EF_MASK									(0x0001)
#define CB_FSR_TLBMCF_MASK							(0x0001)
#define CB_FSR_TLBLKF_MASK							(0x0001)
#define CB_FSR_SS_MASK									(0x0001)
#define CB_FSR_MULTI_MASK								(0x0001)

#define CB_FSR_TF_SHIFT								(1)
#define CB_FSR_AFF_SHIFT								(2)
#define CB_FSR_PF_SHIFT								(3)
#define CB_FSR_EF_SHIFT								(4)
#define CB_FSR_TLBMCF_SHIFT							(5)
#define CB_FSR_TLBLKF_SHIFT							(6)
#define CB_FSR_SS_SHIFT								(30)
#define CB_FSR_MULTI_SHIFT							(31)

#define CB_FSR_TF										(CB_FSR_TF_MASK<<CB_FSR_TF_SHIFT)
#define CB_FSR_AFF										(CB_FSR_AFF_MASK<<CB_FSR_AFF_SHIFT)
#define CB_FSR_PF										(CB_FSR_PF_MASK<<CB_FSR_PF_SHIFT)
#define CB_FSR_EF										(CB_FSR_EF_MASK<<CB_FSR_EF_SHIFT)
#define CB_FSR_TLBMCF									(CB_FSR_TLBMCF_MASK<<CB_FSR_TLBMCF_SHIFT)
#define CB_FSR_TLBLKF									(CB_FSR_TLBLKF_MASK<<CB_FSR_TLBLKF_SHIFT)
#define CB_FSR_SS										(CB_FSR_SS_MASK<<CB_FSR_SS_SHIFT)
#define CB_FSR_MULTI									(CB_FSR_MULTI_MASK<<CB_FSR_MULTI_SHIFT)

#define CB_FSRRESTORE_TF								(CB_FSR_TF_MASK<<CB_FSR_TF_SHIFT)
#define CB_FSRRESTORE_AFF								(CB_FSR_AFF_MASK<<CB_FSR_AFF_SHIFT)
#define CB_FSRRESTORE_PF								(CB_FSR_PF_MASK<<CB_FSR_PF_SHIFT)
#define CB_FSRRESTORE_EF								(CB_FSR_EF_MASK<<CB_FSR_EF_SHIFT)
#define CB_FSRRESTORE_TLBMCF							(CB_FSR_TLBMCF_MASK<<CB_FSR_TLBMCF_SHIFT)
#define CB_FSRRESTORE_TLBLKF							(CB_FSR_TLBLKF_MASK<<CB_FSR_TLBLKF_SHIFT)
#define CB_FSRRESTORE_SS								(CB_FSR_SS_MASK<<CB_FSR_SS_SHIFT)
#define CB_FSRRESTORE_MULTI							(CB_FSR_MULTI_MASK<<CB_FSR_MULTI_SHIFT)

/* --- CB_FSYNR0 --- */

#define CB_FSYNR0_PLVL_MASK							(0x0003)
#define CB_FSYNR0_WNR_MASK							(0x0001)
#define CB_FSYNR0_PNU_MASK							(0x0001)
#define CB_FSYNR0_IND_MASK							(0x0001)
#define CB_FSYNR0_NSSTATE_MASK						(0x0001)
#define CB_FSYNR0_NSATTR_MASK							(0x0001)
#define CB_FSYNR0_PTWF_MASK							(0x0001)
#define CB_FSYNR0_AFR_MASK							(0x0001)
#define CB_FSYNR0_S1CBNDX_MASK						(0x00FF)

#define CB_FSYNR0_PLVL_SHIFT							(0)
#define CB_FSYNR0_WNR_SHIFT							(4)
#define CB_FSYNR0_PNU_SHIFT							(5)
#define CB_FSYNR0_IND_SHIFT							(6)
#define CB_FSYNR0_NSSTATE_SHIFT						(7)
#define CB_FSYNR0_NSATTR_SHIFT						(8)
#define CB_FSYNR0_PTWF_SHIFT							(10)
#define CB_FSYNR0_AFR_SHIFT							(11)
#define CB_FSYNR0_S1CBNDX_SHIFT						(16)

#define CB_FSYNR0_PLVL				(CB_FSYNR0_PLVL_MASK<<CB_FSYNR0_PLVL_SHIFT)
#define CB_FSYNR0_WNR				(CB_FSYNR0_WNR_MASK<<CB_FSYNR0_WNR_SHIFT)
#define CB_FSYNR0_PNU				(CB_FSYNR0_PNU_MASK<<CB_FSYNR0_PNU_SHIFT)
#define CB_FSYNR0_IND				(CB_FSYNR0_IND_MASK<<CB_FSYNR0_IND_SHIFT)
#define CB_FSYNR0_NSSTATE			(CB_FSYNR0_NSSTATE_MASK<<CB_FSYNR0_NSSTATE_SHIFT)
#define CB_FSYNR0_NSATTR			(CB_FSYNR0_NSATTR_MASK<<CB_FSYNR0_NSATTR_SHIFT)
#define CB_FSYNR0_PTWF				(CB_FSYNR0_PTWF_MASK<<CB_FSYNR0_PTWF_SHIFT)
#define CB_FSYNR0_AFR				(CB_FSYNR0_AFR_MASK<<CB_FSYNR0_AFR_SHIFT)
#define CB_FSYNR0_S1CBNDX			(CB_FSYNR0_S1CBNDX_MASK<<CB_FSYNR0_S1CBNDX_SHIFT)


/* =========================================================== */
/* =========================================================== */
/* ======================== NiU wrapper ========================= */
/* =========================================================== */
/* =========================================================== */


#define QOS_PRI_F			(0xF)
#define QOS_PRI_E			(0xE)
#define QOS_PRI_D			(0xD)
#define QOS_PRI_C			(0xC)
#define QOS_PRI_B			(0xB)
#define QOS_PRI_A			(0xA)
#define QOS_PRI_9			(0x9)
#define QOS_PRI_8			(0x8)
#define QOS_PRI_7			(0x7)
#define QOS_PRI_6			(0x6)
#define QOS_PRI_5			(0x5)
#define QOS_PRI_4			(0x4)
#define QOS_PRI_3			(0x3)
#define QOS_PRI_2			(0x2)
#define QOS_PRI_1			(0x1)
#define QOS_PRI_0			(0x0)

#define	ADD_SW_DM0			(0x0)
#define	ADD_SW_GM12		(0x1)
#define	ADD_SW_DM12		(0x2)
#define	ADD_SW_AM12		(0x3)

#define	NOC_URGENCY_3		(0x3)
#define	NOC_URGENCY_2		(0x2)
#define	NOC_URGENCY_1		(0x1)
#define	NOC_URGENCY_0		(0x0)


#endif /* __ODIN_IOMMU_HW_H__ */

