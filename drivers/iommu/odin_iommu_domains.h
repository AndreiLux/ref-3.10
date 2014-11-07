
/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef __ODIN_IOMMU_DOMAINS_H__
#define __ODIN_IOMMU_DOMAINS_H__


/* ==== pool for FrameBuffer - Ion driver's carveout_smmu heap === */

/* DSS */

#ifndef CONFIG_ODIN_ION_I_TUNER
#define	DSS_CARVEOUT_POOL_START			(SZ_32M)
#define	DSS_CARVEOUT_POOL_SIZE			(SZ_64M)

#define	DSS_SYSTEM_POOL_START				(SZ_128M)
#define	DSS_SYSTEM_POOL_SIZE				(0xFFE00000-DSS_SYSTEM_POOL_START)
#else
#define	DSS_CARVEOUT_POOL_START			(SZ_32M)
#define	DSS_CARVEOUT_POOL_SIZE			(SZ_16M+0x07000000-SZ_2M)

#define	DSS_SYSTEM_POOL_START				(SZ_16M+0x07000000+SZ_32M)
#define	DSS_SYSTEM_POOL_SIZE				(0xFFE00000-DSS_SYSTEM_POOL_START)
#endif

/* ==== pool for graphic h/w - Ion driver's system heap ========== */

/* CSS, VSP */

#define	SYSTEM_POOL_START					(SZ_2M)
#define	SYSTEM_POOL_SIZE					(0xFFE00000-SYSTEM_POOL_START)

#ifdef CONFIG_ODIN_ION_I_TUNER
#define	CSS_CARVEOUT_POOL_START			(SZ_2M)
#define	CSS_CARVEOUT_POOL_SIZE			(0x07000000-CSS_CARVEOUT_POOL_START)

#define	CSS_SYSTEM_POOL_START				(0x07000000)
#define	CSS_SYSTEM_POOL_SIZE				(0xFFE00000-CSS_SYSTEM_POOL_START)
#endif

/* ICS, PERI, SDMA, SES */

#define	EXCLUSIVE_POOL_START				(SZ_2M)
#define	EXCLUSIVE_POOL_SIZE				(0xFFE00000-EXCLUSIVE_POOL_START)

/* AUD */

#define	AUD_EXCLUSIVE_POOL_START			(SZ_32M)
#define	AUD_EXCLUSIVE_POOL_SIZE			(0xFFE00000-AUD_EXCLUSIVE_POOL_START)

/* PERI(eMMC)'s ARM DMA-IOMMU */

#define	EMMC_ARM_DMA_IOMMU_POOL_START	(SZ_1G)
#define	EMMC_ARM_DMA_IOMMU_POOL_SIZE		(SZ_2G)


struct odin_priv {
	u64 *pgtable;
	u64 *pgtable_3rd_level;
	struct list_head list_attached;
};

extern unsigned long odin_allocate_iova_address(unsigned int iommu_domain,
					unsigned int partition_no,
					unsigned long size,
					phys_addr_t first_pa);

extern void odin_free_iova_address(unsigned long iova,
			unsigned int iommu_domain,
			unsigned int partition_no,
			unsigned long size,
			phys_addr_t first_pa);

extern unsigned long odin_get_domain_num_from_subsys(
			unsigned int ui_subsys_id);

#endif	/* __ODIN_IOMMU_DOMAINS_H__ */
