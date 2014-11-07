
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


#ifndef __ODIN_IOMMU_H__
#define __ODIN_IOMMU_H__


#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/genalloc.h>


struct mem_pool {
	struct mutex pool_mutex;
	struct gen_pool *gpool;
	unsigned long paddr;
	unsigned long size;
	unsigned long free;
	unsigned int id;
};

/* IOMMU page table base address and size */

#define ODIN_IOMMU_PERI_3RD_TBL_BASE	(0xDD800000)
#define ODIN_IOMMU_PERI_3RD_TBL_SIZE	(SZ_2M*4)

#ifdef	CONFIG_ODIN_ION_SMMU_4K
#define ODIN_IOMMU_GRAPHIC_3RD_TBL_BASE	(ODIN_IOMMU_PERI_3RD_TBL_BASE)
#define ODIN_IOMMU_GRAPHIC_3RD_TBL_SIZE	(ODIN_IOMMU_PERI_3RD_TBL_SIZE)
#endif

#define ODIN_IOMMU_DSS_2ND_TBL_BASE	(ODIN_ION_CARVEOUT_FB_BASE_PA	\
											+DSS_FB_SIZE)
#define ODIN_IOMMU_DSS_2ND_TBL_SIZE	(SZ_4K*SL_PAGE_NUM)

#define ODIN_IOMMU_VSP_2ND_TBL_BASE	(ODIN_IOMMU_DSS_2ND_TBL_BASE	\
											+ODIN_IOMMU_DSS_2ND_TBL_SIZE )
#define ODIN_IOMMU_VSP_2ND_TBL_SIZE	(SZ_4K*SL_PAGE_NUM)

#define ODIN_IOMMU_CSS_2ND_TBL_BASE	(ODIN_IOMMU_VSP_2ND_TBL_BASE	\
											+ODIN_IOMMU_VSP_2ND_TBL_SIZE)
#define ODIN_IOMMU_CSS_2ND_TBL_SIZE	(SZ_4K*SL_PAGE_NUM)

#define ODIN_IOMMU_PERI_2ND_TBL_BASE	(ODIN_IOMMU_CSS_2ND_TBL_BASE	\
											+ODIN_IOMMU_CSS_2ND_TBL_SIZE)
#define ODIN_IOMMU_PERI_2ND_TBL_SIZE	(SZ_4K*SL_PAGE_NUM)

/* SDMA zero padding */

#define	DMAGO(PC, CH, NS)			(0x00000000000000a0LL				\
									| ((unsigned long long)(PC) << 16)	\
									| ((CH) << 8) | ((NS) << 1))
#define	SDMA_NS_BASE				(0x0D010000)
#define	SMMU_WA_VAL				(0x0003FFFC)
#define	SMMU_WA_VAL_H				(0x00000003)
#define	SMMU_WA_VAL_L				(0xFFFC0000)
#define	SMMU_WA_DESC				(0x0003FFE0)

void odin_iommu_prepare_page_table(struct iommu_domain *domain,
										int domain_num);

phys_addr_t odin_iommu_get_pt_addr_from_irq(int irq);

/* Maximum number of Machine IDs that we are allowing to be mapped to the same
 * context bank. The number of MIDs mapped to the same CB does not affect
 * performance, but there is a practical limit on how many distinct MIDs may
 * be present. These mappings are typically determined at design time and are
 * not expected to change at run time.
 */
#define MAX_NUM_MIDS	8	/* Do I need this ? */

/**
 * struct odin_iommu_dev - a single IOMMU hardware instance
 * name		Human-readable name given to this IOMMU HW instance
 * ncb		Number of context banks present on this IOMMU HW instance
 */
struct odin_iommu_dev {
	const char *name;
	int ncb;
};

/**
 * struct odin_iommu_ctx_dev - an IOMMU context bank instance
 * name		Human-readable name given to this context bank
 * num		Index of this context bank within the hardware
 * mids		List of Machine IDs that are to be mapped into this context
 *		bank, terminated by -1. The MID is a set of signals on the
 *		AXI bus that identifies the function associated with a specific
 *		memory request. (See ARM spec).
 */
struct odin_iommu_ctx_dev {
	const char *name;
	int num;
};


/**
 * struct odin_iommu_drvdata - A single IOMMU hardware instance
 * @base:	IOMMU config port base address (VA)
 * @ncb		The number of contexts on this IOMMU
 * @irq:	Interrupt number
 * @clk:	The bus clock for this IOMMU hardware instance
 * @pclk:	The clock for the IOMMU bus interconnect
 *
 * A odin_iommu_drvdata holds the global driver data about a single piece
 * of an IOMMU hardware instance.
 */
struct odin_iommu_drvdata {
	void __iomem *base;
	u32 phys_base;
	int irq;
	int ncb;
	struct clk *clk;
	struct clk *pclk;
	const char *name;
#ifdef	CONFIG_ODIN_ION_SMMU_4K
	unsigned int iommu_id;
#endif
};

/**
 * struct odin_iommu_ctx_drvdata - an IOMMU context bank instance
 * @num:		Hardware context number of this context
 * @pdev:		Platform device associated wit this HW instance
 * @attached_elm:	List element for domains to track which devices are
 *			attached to them
 *
 * A odin_iommu_ctx_drvdata holds the driver data for a single context bank
 * within each IOMMU hardware instance
 */
struct odin_iommu_ctx_drvdata {
	int num;
	struct platform_device *pdev;
	struct list_head attached_elm;
};

/*
 * Interrupt handler for the IOMMU context fault interrupt. Hooking the
 * interrupt is not supported in the API yet, but this will print an error
 * message and dump useful IOMMU registers.
 */
irqreturn_t odin_iommu_fault_handler(int irq, void *dev_id);

#define ITER_IOMMU_UNIT	0
#define ITER_IOMMU_CTX		1

struct device * odin_iommu_get_unit_devs( const char * name,
													int unit_or_ctx );

#endif /* __ODIN_IOMMU_H__ */

