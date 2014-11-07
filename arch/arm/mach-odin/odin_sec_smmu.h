/*
 * arch/arm/mach-odin/odin_sec_smmu.h
 *
 * Copyright:	(C) 2013 LG Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ODIN_SEC_SMMU_H__
#define __ODIN_SEC_SMMU_H__

#define	DMAGO(PC, CH, NS)	(0x00000000000000a0LL			\
				| ((unsigned long long)(PC) << 16)	\
				| ((CH) << 8) | ((NS) << 1))

#define	SECURE_SDMA		(0x0D000000)
#define	SMMU_WA_VAL		(0x0003FFFC)
#define	SMMU_WA_VAL_H		(0x00000003)
#define	SMMU_WA_VAL_L		(0xFFFC0000)
#define	SMMU_WA_DESC		(0x0003FFE0)

#define	ODIN_IOMMU_PERI_CTX_NUM	(4)

/* IOMMU Register Offsets */
#define GS0_CR0			(0x0000)
#define GS0_NSCR0		(0x0400)

#define GS0_SCR1		(0x0004)

#define GS0_ACR			(0x0010)
#define GS0_NSACR		(0x0410)

#define GS0_IDR0		(0x0020)
#define GS0_IDR1		(0x0024)
#define GS0_IDR2		(0x0028)
#define GS0_IDR7		(0x003C)
#define GS0_GFAR_L		(0x0040)
#define	GS0_GFAR_H		(0x0044)

#define GS0_GFSR		(0x0048)
#define GS0_NSGFSR		(0x0448)

#define GS0_GFSRRESTORE		(0x004C)
#define GS0_GFSYNR0		(0x0050)
#define GS0_NSGFSYNR0		(0x0450)

#define GS0_GFSYNR1		(0x0054)
#define	GS0_STLBIALL		(0x0060)
#define GS0_TLBIVMID		(0x0064)
#define GS0_TLBIALLNSNH		(0x0068)
#define GS0_TLBGSYNC		(0x0070)
#define GS0_TLBGSTATUS		(0x0074)

#define GS0_DBGRPTR		(0x0080)
#define GS0_DBGRDATA		(0x0084)
#define GS0_SMR_N		(0x0800)
#define GS0_S2CR_N		(0x0C00)

/*
 * IOMMU Bit masking macro
 */

/* --- GS0_CR0 --- */

#define	GS0_CR0_CLIENTPD_SHIFT	(0)
#define	GS0_CR0_GFRE_SHIFT	(1)
#define	GS0_CR0_GFIE_SHIFT	(2)
#define	GS0_CR0_GCFGFRE_SHIFT	(4)
#define	GS0_CR0_GCFGFIE_SHIFT	(5)
#define	GS0_CR0_STALLD_SHIFT	(8)
#define	GS0_CR0_GSE_SHIFT	(9)
#define	GS0_CR0_USFCFG_SHIFT	(10)
#define	GS0_CR0_VMIDPNE_SHIFT	(11)
#define	GS0_CR0_PTM_SHIFT	(12)
#define	GS0_CR0_FB_SHIFT	(13)
#define	GS0_CR0_BSU_SHIFT	(14)
#define	GS0_CR0_MEMATTR_SHIFT	(16)
#define	GS0_CR0_MTCFG_SHIFT	(20)
#define	GS0_CR0_SMCFCFG_SHIFT	(21)
#define	GS0_CR0_SHCFG_SHIFT	(22)
#define	GS0_CR0_RACFG_SHIFT	(24)
#define	GS0_CR0_WACFG_SHIFT	(26)
#define	GS0_CR0_NSCFG_SHIFT	(28)

#define	GS0_CR0_CLIENTPD_BIT(v)	((v)<<GS0_CR0_CLIENTPD_SHIFT)
#define GS0_CR0_GFRE_BIT(v)	((v)<<GS0_CR0_GFRE_SHIFT)
#define	GS0_CR0_GFIE_BIT(v)	((v)<<GS0_CR0_GFIE_SHIFT)
#define	GS0_CR0_GCFGFRE_BIT(v)	((v)<<GS0_CR0_GCFGFRE_SHIFT)
#define	GS0_CR0_GCFGFIE_BIT(v)	((v)<<GS0_CR0_GCFGFIE_SHIFT)
#define	GS0_CR0_STALLD_BIT(v)	((v)<<GS0_CR0_STALLD_SHIFT)
#define	GS0_CR0_GSE_BIT(v)	((v)<<GS0_CR0_GSE_SHIFT)
#define	GS0_CR0_USFCFG_BIT(v)	((v)<<GS0_CR0_USFCFG_SHIFT)
#define	GS0_CR0_VMIDPNE_BIT(v)	((v)<<GS0_CR0_VMIDPNE_SHIFT)
#define	GS0_CR0_PTM_BIT(v)	((v)<<GS0_CR0_PTM_SHIFT)
#define	GS0_CR0_FB_BIT(v)	((v)<<GS0_CR0_FB_SHIFT)
#define	GS0_CR0_BSU_BIT(v)	((v)<<GS0_CR0_BSU_SHIFT)
#define	GS0_CR0_MEMATTR_BIT(v)	((v)<<GS0_CR0_MEMATTR_SHIFT)
#define	GS0_CR0_MTCFG_BIT(v)	((v)<<GS0_CR0_MTCFG_SHIFT)
#define	GS0_CR0_SMCFCFG_BIT(v)	((v)<<GS0_CR0_SMCFCFG_SHIFT)
#define	GS0_CR0_SHCFG_BIT(v)	((v)<<GS0_CR0_SHCFG_SHIFT)
#define	GS0_CR0_RACFG_BIT(v)	((v)<<GS0_CR0_RACFG_SHIFT)
#define	GS0_CR0_WACFG_BIT(v)	((v)<<GS0_CR0_WACFG_SHIFT)
#define	GS0_CR0_NSCFG_BIT(v)	((v)<<GS0_CR0_NSCFG_SHIFT)

/* --- GS0_SCR1 --- */

#define	GS0_SCR1_SPMEM_SHIFT		(27)
#define	GS0_SCR1_SIF_SHIFT		(26)
#define	GS0_SCR1_GEFRO_SHIFT		(25)
#define	GS0_SCR1_GASRAE_SHIFT		(24)
#define	GS0_SCR1_NSNUMIRPTO_SHIFT	(16)
#define	GS0_SCR1_NSNUMSMRGO_SHIFT	(8)
#define	GS0_SCR1_NSNUMCBO_SHIFT		(0)

#define	GS0_SCR1_SPMEM_BIT(v)		((v)<<GS0_SCR1_SPMEM_SHIFT)
#define	GS0_SCR1_SIF_BIT(v)		((v)<<GS0_SCR1_SIF_SHIFT)
#define	GS0_SCR1_GEFRO_BIT(v)		((v)<<GS0_SCR1_GEFRO_SHIFT)
#define	GS0_SCR1_GASRAE_BIT(v)		((v)<<GS0_SCR1_GASRAE_SHIFT)
#define	GS0_SCR1_NSNUMIRPTO_BIT(v)	((v)<<GS0_SCR1_NSNUMIRPTO_SHIFT)
#define	GS0_SCR1_NSNUMSMRGO_BIT(v)	((v)<<GS0_SCR1_NSNUMSMRGO_SHIFT)
#define	GS0_SCR1_NSNUMCBO_BIT(v)	((v)<<GS0_SCR1_NSNUMCBO_SHIFT)

/* --- GS0_ACR --- */

#define	GS0_ACR_PREFETCHEN_SHIFT	(0)
#define	GS0_ACR_WC1EN_SHIFT		(1)
#define	GS0_ACR_WC2EN_SHIFT		(2)
#define	GS0_ACR_SMTNMB_TLBEN_SHIFT	(8)
#define	GS0_ACR_MMUDISB_TLBEN_SHIFT	(9)
#define	GS0_ACR_S2CRB_TLBEN_SHIFT	(10)

#define	GS0_ACR_PREFETCHEN_BIT(v)	((v)<<GS0_ACR_PREFETCHEN_SHIFT)
#define	GS0_ACR_WC1EN_BIT(v)		((v)<<GS0_ACR_WC1EN_SHIFT)
#define	GS0_ACR_WC2EN_BIT(v)		((v)<<GS0_ACR_WC2EN_SHIFT)
#define	GS0_ACR_SMTNMB_TLBEN_BIT(v)	((v)<<GS0_ACR_SMTNMB_TLBEN_SHIFT)
#define	GS0_ACR_MMUDISB_TLBEN_BIT(v)	((v)<<GS0_ACR_MMUDISB_TLBEN_SHIFT)
#define	GS0_ACR_S2CRB_TLBEN_BIT(v)	((v)<<GS0_ACR_S2CRB_TLBEN_SHIFT)

/* IOMMU IRQ number and mapping */
#define	IOMMU_PERI		39		/* 0x27	*/
#define	IOMMU_AUD		87		/* 0x57	*/
#define	IOMMU_SES		100		/* 0x64	*/
#define	IOMMU_ICS		112		/* 0x70	*/
#define	IOMMU_VSP_0		143		/* 0x8F	*/
#define	IOMMU_VSP_1		151		/* 0x97	*/
#define	IOMMU_DSS_0		153		/* 0x99	*/
#define	IOMMU_DSS_1		155		/* 0x9B	*/
#define	IOMMU_DSS_2		157		/* 0x9D	*/
#define	IOMMU_DSS_3		159		/* 0x9F	*/
#define	IOMMU_CSS_0		184		/* 0xB8	*/
#define	IOMMU_CSS_1		186		/* 0xBA	*/
#define	IOMMU_SDMA		236		/* 0xEC	*/
#define	IOMMU_GPU_0		243		/* 0xF3	*/
#define	IOMMU_GPU_1		269		/* 0x10D */

/* NIU wrapper define */

#define QOS_PRI_F		(0xF)
#define QOS_PRI_E		(0xE)
#define QOS_PRI_D		(0xD)
#define QOS_PRI_C		(0xC)
#define QOS_PRI_B		(0xB)
#define QOS_PRI_A		(0xA)
#define QOS_PRI_9		(0x9)
#define QOS_PRI_8		(0x8)
#define QOS_PRI_7		(0x7)
#define QOS_PRI_6		(0x6)
#define QOS_PRI_5		(0x5)
#define QOS_PRI_4		(0x4)
#define QOS_PRI_3		(0x3)
#define QOS_PRI_2		(0x2)
#define QOS_PRI_1		(0x1)
#define QOS_PRI_0		(0x0)

#define	ADD_SW_DM0		(0x0)
#define	ADD_SW_GM12		(0x1)
#define	ADD_SW_DM12		(0x2)
#define	ADD_SW_AM12		(0x3)

#define	NOC_URGENCY_3		(0x3)
#define	NOC_URGENCY_2		(0x2)
#define	NOC_URGENCY_1		(0x1)
#define	NOC_URGENCY_0		(0x0)

int odin_niu_tz_init(int irq);
int odin_iommu_tz_init(int irq);

#endif
