
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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <linux/clk.h>
#include <linux/scatterlist.h>
#include <linux/odin_iommu.h>
#include <linux/delay.h>

#include <asm/dma-iommu.h>
#include <asm/cacheflush.h>
#include <asm/sizes.h>

#include "odin_iommu_hw.h"
#include "odin_iommu.h"
#include "odin_iommu_domains.h"


unsigned long odin_smmu_alloc_sz[ALLOC_PG_SZ_BIGGEST] = { 0, SZ_2M };

#define	ODIN_TLB_FLUSH_DELAY	(0)


static DEFINE_SPINLOCK(odin_iommu_lock);
/* static DEFINE_SPINLOCK(odin_iommu_map_lock);	*/
static DEFINE_SPINLOCK(odin_iommu_fault_lock);
static DEFINE_SPINLOCK(odin_iommu_sdma_lock);


#ifndef	CONFIG_ODIN_ION_SMMU_4K
static atomic_t is_css_0_idle;
static atomic_t is_css_1_idle;
static atomic_t is_vsp_0_idle;
static atomic_t is_vsp_1_idle;
static atomic_t is_dss_0_idle;
static atomic_t is_dss_1_idle;
static atomic_t is_dss_2_idle;
static atomic_t is_dss_3_idle;
static atomic_t is_ics_idle;
static atomic_t is_aud_idle;
static atomic_t is_peri_idle;
static atomic_t is_sdma_idle;
static atomic_t is_ses_idle;
#else
static atomic_t is_iommu_idle[13];
enum {
	ODIN_IOMMU_CSS_0_ID,
	ODIN_IOMMU_CSS_1_ID,
	ODIN_IOMMU_VSP_0_ID,
	ODIN_IOMMU_VSP_1_ID,
	ODIN_IOMMU_DSS_0_ID,
	ODIN_IOMMU_DSS_1_ID,
	ODIN_IOMMU_DSS_2_ID,
	ODIN_IOMMU_DSS_3_ID,
	ODIN_IOMMU_ICS_ID,
	ODIN_IOMMU_AUD_ID,
	ODIN_IOMMU_PERI_ID,
	ODIN_IOMMU_SDMA_ID,
	ODIN_IOMMU_SES_ID,
};
unsigned int odin_iommu_get_id_from_irq( int irq_num )
{

	switch ( irq_num ){

		case IOMMU_NS_IRQ_DSS_0 :
				return ODIN_IOMMU_DSS_0_ID;

		case IOMMU_NS_IRQ_DSS_1 :
				return ODIN_IOMMU_DSS_1_ID;

		case IOMMU_NS_IRQ_DSS_2 :
				return ODIN_IOMMU_DSS_2_ID;

		case IOMMU_NS_IRQ_DSS_3 :
				return ODIN_IOMMU_DSS_3_ID;

		case IOMMU_NS_IRQ_CSS_0 :
				return ODIN_IOMMU_CSS_0_ID;

		case IOMMU_NS_IRQ_CSS_1 :
				return ODIN_IOMMU_CSS_1_ID;

		case IOMMU_NS_IRQ_VSP_0 :
				return ODIN_IOMMU_VSP_0_ID;

		case IOMMU_NS_IRQ_VSP_1 :
				return ODIN_IOMMU_VSP_1_ID;

		case IOMMU_NS_IRQ_ICS :
				return ODIN_IOMMU_ICS_ID;

		case IOMMU_NS_IRQ_AUD :
				return ODIN_IOMMU_AUD_ID;

		case IOMMU_NS_IRQ_PERI :
				return ODIN_IOMMU_PERI_ID;

		case IOMMU_NS_IRQ_SDMA :
				return ODIN_IOMMU_SDMA_ID;

		case IOMMU_NS_IRQ_SES :
				return ODIN_IOMMU_SES_ID;

		default :
				return 0;	/* impossible */

	}

	return 0;	/* impossible */

}
#endif


#ifndef	CONFIG_ODIN_ION_SMMU_4K
static int odin_iommu_subsys_idle_state( int irq_num )
{

	switch ( irq_num ){

		case IOMMU_NS_IRQ_DSS_0 :
				return atomic_read( &is_dss_0_idle );

		case IOMMU_NS_IRQ_DSS_1 :
				return atomic_read( &is_dss_1_idle );

		case IOMMU_NS_IRQ_DSS_2 :
				return atomic_read( &is_dss_2_idle );

		case IOMMU_NS_IRQ_DSS_3 :
				return atomic_read( &is_dss_3_idle );

		case IOMMU_NS_IRQ_CSS_0 :
				return atomic_read( &is_css_0_idle );

		case IOMMU_NS_IRQ_CSS_1 :
				return atomic_read( &is_css_1_idle );

		case IOMMU_NS_IRQ_VSP_0 :
				return atomic_read( &is_vsp_0_idle );

		case IOMMU_NS_IRQ_VSP_1 :
				return atomic_read( &is_vsp_1_idle );

		case IOMMU_NS_IRQ_ICS :
				return atomic_read( &is_ics_idle );

		case IOMMU_NS_IRQ_AUD :
				return atomic_read( &is_aud_idle );

		case IOMMU_NS_IRQ_PERI :
				return atomic_read( &is_peri_idle );

		case IOMMU_NS_IRQ_SDMA :
				return atomic_read( &is_sdma_idle );

		case IOMMU_NS_IRQ_SES :
				return atomic_read( &is_ses_idle );

		default :
			return 0;

	}

	return 0;

}
#else
static unsigned int odin_iommu_subsys_idle_state( unsigned int iommu_id )
{
	return atomic_read( &is_iommu_idle[iommu_id] );
}
#endif


/* call from pm driver */
#ifndef	CONFIG_ODIN_ION_SMMU_4K
void odin_iommu_prohibit_tlb_flush( int domain_num )
{

	if ( domain_num == CSS_DOMAIN ){

		atomic_set(&is_css_0_idle, 1);
		atomic_set(&is_css_1_idle, 1);

	}else if (  domain_num == VSP_DOMAIN ){

		atomic_set(&is_vsp_0_idle, 1);
		atomic_set(&is_vsp_1_idle, 1);

	}else if (  domain_num == DSS_DOMAIN ){

		atomic_set(&is_dss_0_idle, 1);
		atomic_set(&is_dss_1_idle, 1);
		atomic_set(&is_dss_2_idle, 1);
		atomic_set(&is_dss_3_idle, 1);

	}else if (  domain_num == ICS_DOMAIN ){

		atomic_set(&is_ics_idle, 1);

	}else if (  domain_num == AUD_DOMAIN ){

		atomic_set(&is_aud_idle, 1);

	}else if (  domain_num == PERI_DOMAIN ){

		atomic_set(&is_peri_idle, 1);

	}else if (  domain_num == SDMA_DOMAIN ){

		atomic_set(&is_sdma_idle, 1);

	}else if (  domain_num == SES_DOMAIN ){

		atomic_set(&is_ses_idle, 1);

	}

}
#else
void odin_iommu_prohibit_tlb_flush( int domain_num )
{

	if ( domain_num == CSS_POWER_DOMAIN ){

		atomic_set( &is_iommu_idle[ODIN_IOMMU_CSS_0_ID], 1 );
		atomic_set( &is_iommu_idle[ODIN_IOMMU_CSS_1_ID], 1 );

	}else if (  domain_num == VSP_POWER_DOMAIN ){

		atomic_set( &is_iommu_idle[ODIN_IOMMU_VSP_0_ID], 1 );
		atomic_set( &is_iommu_idle[ODIN_IOMMU_VSP_1_ID], 1 );

	}else if (  domain_num == DSS_POWER_DOMAIN ){

		atomic_set( &is_iommu_idle[ODIN_IOMMU_DSS_0_ID], 1 );
		atomic_set( &is_iommu_idle[ODIN_IOMMU_DSS_1_ID], 1 );
		atomic_set( &is_iommu_idle[ODIN_IOMMU_DSS_2_ID], 1 );
		atomic_set( &is_iommu_idle[ODIN_IOMMU_DSS_3_ID], 1 );

	}else if (  domain_num == ICS_POWER_DOMAIN ){

		atomic_set( &is_iommu_idle[ODIN_IOMMU_ICS_ID], 1 );

	}else if (  domain_num == AUD_POWER_DOMAIN ){

		atomic_set( &is_iommu_idle[ODIN_IOMMU_AUD_ID], 1 );

	}else if (  domain_num == PERI_POWER_DOMAIN ){

		atomic_set( &is_iommu_idle[ODIN_IOMMU_PERI_ID], 1 );

	}else if (  domain_num == SDMA_POWER_DOMAIN ){

		atomic_set( &is_iommu_idle[ODIN_IOMMU_SDMA_ID], 1 );

	}else if (  domain_num == SES_POWER_DOMAIN ){

		atomic_set( &is_iommu_idle[ODIN_IOMMU_SES_ID], 1 );

	}

}
#endif
EXPORT_SYMBOL(odin_iommu_prohibit_tlb_flush);


/* call from pm driver */
#ifndef	CONFIG_ODIN_ION_SMMU_4K
void odin_iommu_permit_tlb_flush( int irq_num )
{

	switch ( irq_num ){

		case IOMMU_NS_IRQ_DSS_0 :
				atomic_set( &is_dss_0_idle, 0 );
				break;

		case IOMMU_NS_IRQ_DSS_1 :
				atomic_set( &is_dss_1_idle, 0 );
				break;

		case IOMMU_NS_IRQ_DSS_2 :
				atomic_set( &is_dss_2_idle, 0 );
				break;

		case IOMMU_NS_IRQ_DSS_3 :
				atomic_set( &is_dss_3_idle, 0 );
				break;

		case IOMMU_NS_IRQ_CSS_0 :
				atomic_set( &is_css_0_idle, 0 );
				break;

		case IOMMU_NS_IRQ_CSS_1 :
				atomic_set( &is_css_1_idle, 0 );
				break;

		case IOMMU_NS_IRQ_VSP_0 :
				atomic_set( &is_vsp_0_idle, 0 );
				break;

		case IOMMU_NS_IRQ_VSP_1 :
				atomic_set( &is_vsp_1_idle, 0 );
				break;

		case IOMMU_NS_IRQ_ICS :
				atomic_set( &is_ics_idle, 0 );
				break;

		case IOMMU_NS_IRQ_AUD :
				atomic_set( &is_aud_idle, 0 );
				break;

		case IOMMU_NS_IRQ_PERI :
				atomic_set( &is_peri_idle, 0 );
				break;

		case IOMMU_NS_IRQ_SDMA :
				atomic_set( &is_sdma_idle, 0 );
				break;

		case IOMMU_NS_IRQ_SES :
				atomic_set( &is_ses_idle, 0 );
				break;

		default :
			break;

	}

}
#else
void odin_iommu_permit_tlb_flush( int irq_num )
{
	atomic_set( &is_iommu_idle[(odin_iommu_get_id_from_irq(irq_num))], 0 );
}
#endif
EXPORT_SYMBOL(odin_iommu_permit_tlb_flush);


void odin_iommu_fill_buffer_zero( unsigned long iova, unsigned long size )
{

#if 0
	unsigned int tmp=0;
	unsigned long long cmd=0;
#endif


	spin_lock( &odin_iommu_sdma_lock );
#if 0
	writel( 0x000000BC|SMMU_WA_VAL_L, 	smmu_wa_desc	);
	writel( 0x02BC0000|SMMU_WA_VAL_H, 	smmu_wa_desc+4	);
	writel( dest_addr,					smmu_wa_desc+8	);
	writel( 0x410501BC, 				smmu_wa_desc+12 );
	writel( 0x00200E41, 				smmu_wa_desc+16 );
	writel( 0x38130804,				 	smmu_wa_desc+20 );
	writel( 0x00803403, 				smmu_wa_desc+24 );

	tmp = readl( sdma_base + 0x1000 ) & ~0xd;
	writel( tmp, sdma_base + 0x1000 );
	writel( 0x0, sdma_base + 0x1004 );
	writel( 0x0000ffff, sdma_base + 0x1008 );
	writel( (tmp|0x8), sdma_base + 0x1000) ;
	writel( 0xff000000, sdma_base + 0x20 );
	while ( readl(sdma_base + 0xd00) & 0x1 ); /* add time out */

	cmd = DMAGO( SMMU_WA_DESC, 0, 0 );

	writel( (((unsigned int)cmd << 16) | (0 << 8) | 0), sdma_base + 0xd08 );
	writel( (unsigned int)(cmd >> 16), sdma_base + 0xd0c );
	writel( 0x0, sdma_base + 0xd04 );
	while ( !((readl(sdma_base + 0x024) >> 16) & 0x1) ){
		udelay( 1 ); /* add time out */
	}
	writel( 0x1<<16, sdma_base + 0x02c );
#endif
	spin_unlock( &odin_iommu_sdma_lock );

}


static int __flush_iotlb_va( struct iommu_domain *domain, unsigned int va )
{

	struct odin_priv				*priv = domain->priv;
	struct odin_iommu_drvdata		*iommu_drvdata = NULL;
	struct odin_iommu_ctx_drvdata	*ctx_drvdata;


	list_for_each_entry( ctx_drvdata, &priv->list_attached, attached_elm ){

#ifndef	CONFIG_ODIN_ION_SMMU_4K
		if ( unlikely(!ctx_drvdata->pdev || !ctx_drvdata->pdev->dev.parent) )
			return -EINVAL;
#endif

		iommu_drvdata = dev_get_drvdata( ctx_drvdata->pdev->dev.parent );
#ifndef	CONFIG_ODIN_ION_SMMU_4K
		if ( unlikely(!iommu_drvdata) )
			return -EINVAL;
#endif

#ifndef	CONFIG_ODIN_ION_SMMU_4K
		if ( unlikely(odin_iommu_subsys_idle_state(iommu_drvdata->irq) == 1) )
#else
		if ( odin_iommu_subsys_idle_state(iommu_drvdata->iommu_id) != 0 )
#endif
			continue;

		/* Invalidate all TLB associated with the VMID = 0 */
		SET_GS0_TLBIALLNSNH( iommu_drvdata->base, 0 );

	}

#ifndef	CONFIG_ODIN_ION_SMMU_4K
	if (iommu_drvdata)
		if (iommu_drvdata->irq == IOMMU_NS_IRQ_DSS_0)
			if ( likely(odin_iommu_subsys_idle_state(IOMMU_NS_IRQ_SDMA) == 0) )
				odin_flush_iotlb_sdma();

	udelay(ODIN_TLB_FLUSH_DELAY);
#endif

	return 0;

}


void odin_iommu_program_context(void __iomem *base, int ctx, int ncb,
								phys_addr_t pgtable, int irq, int redirect)
{

	/* TODO : add every context bank independently in device tree */

	/* ========== context 0 definition ========== */

	SET_CB_TTBR0_L( base, ctx, (u32)pgtable );
#ifdef CONFIG_ARM_LPAE
	SET_CB_TTBR0_H( base, ctx, (u32)(pgtable>>32) );
#else
	SET_CB_TTBR0_H( base, ctx, 0 );
#endif

	if ( irq == IOMMU_NS_IRQ_PERI ){
		SET_CB_TTBR0_L( base, (ctx+1), (u32)pgtable );
#ifdef CONFIG_ARM_LPAE
		SET_CB_TTBR0_H( base, (ctx+1), (u32)(pgtable>>32) );
#else
		SET_CB_TTBR0_H( base, (ctx+1), 0 );
#endif
	}

	SET_CB_FSR( base, (ctx),
				0xFFFFFFFF );

	if ( irq == IOMMU_NS_IRQ_PERI ){
		SET_CB_FSR( base, (ctx+1),
				0xFFFFFFFF );
	}

	if ( irq == IOMMU_NS_IRQ_PERI ){
		SET_GS1_CBAR_N( base,
					GS1_CBAR_N_IRPTNDX_BIT(0x0) 		| GS1_CBAR_N_TYPE_BIT(0x0) 				|
					GS1_CBAR_N_VMID_BIT(0x0),
					0x0 );
		SET_GS1_CBAR_N( base,
					GS1_CBAR_N_IRPTNDX_BIT(0x0) 		| GS1_CBAR_N_TYPE_BIT(0x0) 				|
					GS1_CBAR_N_VMID_BIT(0x0),
					0x1 );
		SET_GS1_CBAR_N( base,
					GS1_CBAR_N_IRPTNDX_BIT(0x0) 		| GS1_CBAR_N_TYPE_BIT(0x1) 				|
					GS1_CBAR_N_VMID_BIT(0x0),
					0x2 );
		SET_GS1_CBAR_N( base,
					GS1_CBAR_N_IRPTNDX_BIT(0x0) 		| GS1_CBAR_N_TYPE_BIT(0x1) 				|
					GS1_CBAR_N_VMID_BIT(0x0),
					0x3 );
	}else{
		SET_GS1_CBAR_N( base,
					GS1_CBAR_N_IRPTNDX_BIT(0x0) 		| GS1_CBAR_N_TYPE_BIT(0x0) 				|
					GS1_CBAR_N_VMID_BIT(0x0),
					0x0 );
	}

	SET_CB_SCTLR( base, ctx,
					CB_SCTLR_WACFG_BIT(0x0) 			| CB_SCTLR_RACFG_BIT(0x0) 				|
					CB_SCTLR_SHCFG_BIT(0x0) 			| CB_SCTLR_FB_BIT(0x0) 					|
					CB_SCTLR_MEMATTR_BIT(0xF) 		| CB_SCTLR_BSU_BIT(0x0) 					|
					CB_SCTLR_HUPCF_BIT(0x1) 			| CB_SCTLR_CFCFG_BIT(0x0) 				|
					CB_SCTLR_CFIE_BIT(0x1) 			| CB_SCTLR_CFRE_BIT(0x1) 					|
					CB_SCTLR_E_BIT(0x0) 				| CB_SCTLR_AFFD_BIT(0x1) 					|
					CB_SCTLR_AFE_BIT(0x1) 			| CB_SCTLR_TRE_BIT(0x1) 					|
					CB_SCTLR_M_BIT(0x1) );				/* Enable MMU translation !! */

	if ( irq == IOMMU_NS_IRQ_PERI ){
		SET_CB_SCTLR( base, (ctx+1),
					CB_SCTLR_WACFG_BIT(0x0) 			| CB_SCTLR_RACFG_BIT(0x0) 				|
					CB_SCTLR_SHCFG_BIT(0x0) 			| CB_SCTLR_FB_BIT(0x0) 					|
					CB_SCTLR_MEMATTR_BIT(0xF) 		| CB_SCTLR_BSU_BIT(0x0) 					|
					CB_SCTLR_HUPCF_BIT(0x1) 			| CB_SCTLR_CFCFG_BIT(0x0) 				|
					CB_SCTLR_CFIE_BIT(0x1) 			| CB_SCTLR_CFRE_BIT(0x1) 					|
					CB_SCTLR_E_BIT(0x0) 				| CB_SCTLR_AFFD_BIT(0x1) 					|
					CB_SCTLR_AFE_BIT(0x1) 			| CB_SCTLR_TRE_BIT(0x1) 					|
					CB_SCTLR_M_BIT(0x1) );				/* Enable MMU translation !! */
	}

	/* ========== for normal translation ========== */

	/* When MMU translation is enabled,					*/
	/* AMBA signals are generated with page table & S2CR. 	*/

	if ( irq == IOMMU_NS_IRQ_PERI ){
		SET_GS0_S2CR_N( base,
					GS0_S2CR_N_00_MTCFG_BIT(0x1)		| GS0_S2CR_N_00_WACFG_BIT(0x0) 			|
					GS0_S2CR_N_00_RACFG_BIT(0x0)		| GS0_S2CR_N_00_SHCFG_BIT(0x0) 			|
					GS0_S2CR_N_00_MEMATTR_BIT(0x0F)	| GS0_S2CR_N_00_TRANSIENTCFG_BIT(0x0) 	|
					GS0_S2CR_N_00_INSTCFG_BIT(0x0) 	| GS0_S2CR_N_00_PRIVCFG_BIT(0x0) 		|
					GS0_S2CR_N_00_TYPE_BIT(0x0) 		| GS0_S2CR_N_00_NSCFG_BIT(0x0) 			|
					GS0_S2CR_N_00_CBNDX_BIT(0x0),
					0x0	);
		SET_GS0_S2CR_N( base,
					GS0_S2CR_N_00_MTCFG_BIT(0x1)		| GS0_S2CR_N_00_WACFG_BIT(0x0) 			|
					GS0_S2CR_N_00_RACFG_BIT(0x0)		| GS0_S2CR_N_00_SHCFG_BIT(0x0) 			|
					GS0_S2CR_N_00_MEMATTR_BIT(0x0F)	| GS0_S2CR_N_00_TRANSIENTCFG_BIT(0x0) 	|
					GS0_S2CR_N_00_INSTCFG_BIT(0x0) 	| GS0_S2CR_N_00_PRIVCFG_BIT(0x0) 		|
					GS0_S2CR_N_00_TYPE_BIT(0x0) 		| GS0_S2CR_N_00_NSCFG_BIT(0x0) 			|
					GS0_S2CR_N_00_CBNDX_BIT(0x0),
					0x1	);
		SET_GS0_S2CR_N( base,
					GS0_S2CR_N_00_MTCFG_BIT(0x1)		| GS0_S2CR_N_00_WACFG_BIT(0x0) 			|
					GS0_S2CR_N_00_RACFG_BIT(0x0)		| GS0_S2CR_N_00_SHCFG_BIT(0x0) 			|
					GS0_S2CR_N_00_MEMATTR_BIT(0x0F)	| GS0_S2CR_N_00_TRANSIENTCFG_BIT(0x0) 	|
					GS0_S2CR_N_00_INSTCFG_BIT(0x0) 	| GS0_S2CR_N_00_PRIVCFG_BIT(0x0) 		|
					GS0_S2CR_N_00_TYPE_BIT(0x1) 		| GS0_S2CR_N_00_NSCFG_BIT(0x0) 			|
					GS0_S2CR_N_00_CBNDX_BIT(0x0),
					0x2	);
		SET_GS0_S2CR_N( base,
					GS0_S2CR_N_00_MTCFG_BIT(0x1)		| GS0_S2CR_N_00_WACFG_BIT(0x0) 			|
					GS0_S2CR_N_00_RACFG_BIT(0x0)		| GS0_S2CR_N_00_SHCFG_BIT(0x0) 			|
					GS0_S2CR_N_00_MEMATTR_BIT(0x0F)	| GS0_S2CR_N_00_TRANSIENTCFG_BIT(0x0) 	|
					GS0_S2CR_N_00_INSTCFG_BIT(0x0) 	| GS0_S2CR_N_00_PRIVCFG_BIT(0x0) 		|
					GS0_S2CR_N_00_TYPE_BIT(0x1) 		| GS0_S2CR_N_00_NSCFG_BIT(0x0) 			|
					GS0_S2CR_N_00_CBNDX_BIT(0x0),
					0x3	);
	}else{
		SET_GS0_S2CR_N( base,
					GS0_S2CR_N_00_MTCFG_BIT(0x1)		| GS0_S2CR_N_00_WACFG_BIT(0x0) 			|
					GS0_S2CR_N_00_RACFG_BIT(0x0)		| GS0_S2CR_N_00_SHCFG_BIT(0x0) 			|
					GS0_S2CR_N_00_MEMATTR_BIT(0x0F)	| GS0_S2CR_N_00_TRANSIENTCFG_BIT(0x0) 	|
					GS0_S2CR_N_00_INSTCFG_BIT(0x0) 	| GS0_S2CR_N_00_PRIVCFG_BIT(0x0) 		|
					GS0_S2CR_N_00_TYPE_BIT(0x0) 		| GS0_S2CR_N_00_NSCFG_BIT(0x0) 			|
					GS0_S2CR_N_00_CBNDX_BIT(0x0),
					0x0	);
	}

	SET_CB_TTBCR( base, ctx,
					CB_TTBCR_SH0_BIT(PTW_NON_SHAREABLE)											|
					CB_TTBCR_ORGN0_BIT(0x0)			| CB_TTBCR_IRGN0_BIT(0x0) 				|
					CB_TTBCR_SL0_BIT(0x0) 			| CB_TTBCR_T0SZ_BIT(0x0) );

	if ( irq == IOMMU_NS_IRQ_PERI ){
		SET_CB_TTBCR( base, (ctx+1),
					CB_TTBCR_SH0_BIT(PTW_NON_SHAREABLE)											|
					CB_TTBCR_ORGN0_BIT(0x0)			| CB_TTBCR_IRGN0_BIT(0x0) 				|
					CB_TTBCR_SL0_BIT(0x0) 			| CB_TTBCR_T0SZ_BIT(0x0) );
	}

	mb();

}


static int odin_iommu_domain_init(struct iommu_domain *domain)
{

	struct odin_priv *priv = kzalloc( sizeof(*priv), GFP_KERNEL );


	if (!priv)
		goto fail_nomem;

	INIT_LIST_HEAD(&priv->list_attached);
	domain->priv = priv;
	return 0;

fail_nomem:
	return -ENOMEM;

}


static void odin_iommu_domain_destroy(struct iommu_domain *domain)
{

	struct odin_priv	*priv;
	unsigned long 		flags;


	spin_lock_irqsave( &odin_iommu_lock, flags );

	priv = domain->priv;
	domain->priv = NULL;
	kfree( priv );

	spin_unlock_irqrestore( &odin_iommu_lock, flags );

}


void odin_iommu_prepare_page_table(struct iommu_domain *domain,
												int domain_num)
{

	/* TODO : add error check code */

	struct odin_priv *priv = domain->priv;
#if 0
	u64 *sl_pte;
	u64 *tl_pte;
	u64 pa;
	int i;
#endif


	switch ( domain_num ){

#ifndef	CONFIG_ODIN_ION_SMMU_4K
		case CSS_DOMAIN :

				priv->pgtable = (u64 *)ioremap(
									ODIN_IOMMU_CSS_2ND_TBL_BASE,
									ODIN_IOMMU_CSS_2ND_TBL_SIZE);
				memset(priv->pgtable, 0, ODIN_IOMMU_CSS_2ND_TBL_SIZE);
				dmac_flush_range(priv->pgtable,
					( priv->pgtable + (LONG_DESC_ENTRY_NUM*SL_PAGE_NUM) ));

				break;

		case VSP_DOMAIN :

				priv->pgtable = (u64 *)ioremap(
									ODIN_IOMMU_VSP_2ND_TBL_BASE,
									ODIN_IOMMU_VSP_2ND_TBL_SIZE);
				memset(priv->pgtable, 0, ODIN_IOMMU_VSP_2ND_TBL_SIZE);
				dmac_flush_range(priv->pgtable,
					( priv->pgtable + (LONG_DESC_ENTRY_NUM*SL_PAGE_NUM) ));

				break;

		case DSS_DOMAIN :

				priv->pgtable = (u64 *)ioremap(
									ODIN_IOMMU_DSS_2ND_TBL_BASE,
									ODIN_IOMMU_DSS_2ND_TBL_SIZE);

				break;
#else
		case GRAPHIC_DOMAIN :

				priv->pgtable = (u64 *)ioremap(
									ODIN_IOMMU_DSS_2ND_TBL_BASE,
									ODIN_IOMMU_DSS_2ND_TBL_SIZE);

				priv->pgtable_3rd_level = (u64 *)ioremap(
									ODIN_IOMMU_GRAPHIC_3RD_TBL_BASE,
									ODIN_IOMMU_GRAPHIC_3RD_TBL_SIZE);

				memset(priv->pgtable_3rd_level, 0, ODIN_IOMMU_GRAPHIC_3RD_TBL_SIZE);
				dmac_flush_range(priv->pgtable_3rd_level,
					( priv->pgtable_3rd_level +	\
					(LONG_DESC_ENTRY_NUM*LONG_DESC_ENTRY_NUM*SL_PAGE_NUM) ));

				break;
#endif

		case SDMA_DOMAIN :

				/* If priv->pgtable==NULL, it is non-graphic subsystem				*/
				/* Because SDMA is sharing DSS's page table, it should not be mapped	*/
				priv->pgtable = NULL;
				priv->pgtable_3rd_level = NULL;

				break;

		case PERI_DOMAIN :
#if 0
				/* 2nd level - I */

				priv->pgtable = (u64 *)ioremap(
									ODIN_IOMMU_PERI_2ND_TBL_BASE,
									SZ_4K);
				memset(priv->pgtable, 0, SZ_4K);
				dmac_flush_range(priv->pgtable,
					( priv->pgtable + LONG_DESC_ENTRY_NUM ));
				iounmap(priv->pgtable);

				/* 2nd level - II */

				priv->pgtable = (u64 *)ioremap(
									(ODIN_IOMMU_PERI_2ND_TBL_BASE+SZ_4K),
									(ODIN_IOMMU_PERI_2ND_TBL_SIZE-SZ_4K));
				sl_pte = priv->pgtable;
				pa = ODIN_IOMMU_PERI_3RD_TBL_BASE+SZ_2M;
				for ( i=0; i<((ODIN_IOMMU_PERI_2ND_TBL_SIZE-SZ_4K)/8); i++ ){
					*(sl_pte+i) = ( pa&NEXT_LEVEL_PAGE_ADDR_MASK ) | LDS2_PGT;
					pa += SZ_4K;
				}
				dmac_flush_range( sl_pte,
					(sl_pte+(ODIN_IOMMU_PERI_2ND_TBL_SIZE-SZ_4K)/8) );
				iounmap(priv->pgtable);

				/* 3rd level - I */

				priv->pgtable_3rd_level = (u64 *)ioremap(
									ODIN_IOMMU_PERI_3RD_TBL_BASE+SZ_2M,
									(ODIN_IOMMU_PERI_3RD_TBL_SIZE-SZ_2M));

				tl_pte = priv->pgtable_3rd_level;
				pa = ODIN_IOMMU_FIRST_BANK_BASE;
				for ( i=0; i<(ODIN_IOMMU_FIRST_BANK_SIZE/8); i++ ){
					*(tl_pte+i) = (pa&OUTPUT_PHYS_ADDR_MASK) |	\
									LDS2_MEM | LDS2_CONT(0x0) | LDS2_TYPE(0x1);
					pa += SZ_4K;
				}
				dmac_flush_range( tl_pte, (tl_pte+ODIN_IOMMU_FIRST_BANK_SIZE/8) );

				/* 3rd level - II */

				tl_pte = priv->pgtable_3rd_level + ODIN_IOMMU_SECOND_BANK_SIZE/8;
				pa = (u64)ODIN_IOMMU_SECOND_BANK_BASE;
				for ( i=0; i<(ODIN_IOMMU_SECOND_BANK_SIZE/8); i++ ){
					*(tl_pte+i) = (pa&OUTPUT_PHYS_ADDR_MASK) |	\
									LDS2_MEM | LDS2_CONT(0x0) | LDS2_TYPE(0x1);
					pa += SZ_4K;
				}
				dmac_flush_range( tl_pte, (tl_pte+ODIN_IOMMU_SECOND_BANK_SIZE/8) );

				iounmap(priv->pgtable_3rd_level);
#endif
				/* If priv->pgtable==NULL, it is non-graphic subsystem */
				priv->pgtable = NULL;
				priv->pgtable_3rd_level = NULL;

				break;

		/* case ICS / AUD / SES / SDMA */

		default :

				break;

	}

}


phys_addr_t odin_iommu_get_pt_addr_from_irq(int irq)
{

	switch ( irq ){

		case IOMMU_NS_IRQ_CSS_0 :
		case IOMMU_NS_IRQ_CSS_1 :

#ifndef	CONFIG_ODIN_ION_SMMU_4K
				return (phys_addr_t)ODIN_IOMMU_CSS_2ND_TBL_BASE;
#else
				return (phys_addr_t)ODIN_IOMMU_DSS_2ND_TBL_BASE;
#endif

		case IOMMU_NS_IRQ_VSP_0 :
		case IOMMU_NS_IRQ_VSP_1 :

#ifndef	CONFIG_ODIN_ION_SMMU_4K
				return (phys_addr_t)ODIN_IOMMU_VSP_2ND_TBL_BASE;
#else
				return (phys_addr_t)ODIN_IOMMU_DSS_2ND_TBL_BASE;
#endif

		case IOMMU_NS_IRQ_DSS_0 :
		case IOMMU_NS_IRQ_DSS_1 :
		case IOMMU_NS_IRQ_DSS_2 :
		case IOMMU_NS_IRQ_DSS_3 :

				return (phys_addr_t)ODIN_IOMMU_DSS_2ND_TBL_BASE;

		case IOMMU_NS_IRQ_PERI :

				return (phys_addr_t)ODIN_IOMMU_PERI_2ND_TBL_BASE;

		case IOMMU_NS_IRQ_SDMA :

				return (phys_addr_t)ODIN_IOMMU_DSS_2ND_TBL_BASE;

		/* case ICS / AUD / SES */

		default :

				return 0;

	}

}


static int odin_iommu_map(struct iommu_domain *domain, unsigned long va,
			 					phys_addr_t pa, size_t size, int prot)
{

	/* Avoid every calculating & error checking. We're so busy!! */

	struct odin_priv *priv;
	/* unsigned long flags;	*/

	u32 l2start_offset;
	u32 sl_offset;
	u64 *sl_table;
	u64 *sl_pte;

#ifdef	CONFIG_ODIN_ION_SMMU_4K
	u32 l3start_offset;
	u32 tl_offset;
	u64 *tl_table;
	u64 *tl_pte;
	u64 tl_pte_pa;
#endif

	int ret=0;


	/* spin_lock_irqsave( &odin_iommu_map_lock, flags );	*/

	priv = domain->priv;

	if ( likely(size == SZ_4K) ){

		sl_table = priv->pgtable;
		l2start_offset = ( va >> FL_ADDR_BIT_SHIFT ) << 9;
		sl_offset = SL_ADDR_OFFSET(va);
		sl_pte = sl_table + l2start_offset + sl_offset;

		tl_table = priv->pgtable_3rd_level;
		l3start_offset = ( va >> SL_ADDR_BIT_SHIFT ) << 9;
		tl_offset = TL_ADDR_OFFSET(va);
		tl_pte = tl_table + l3start_offset + tl_offset;

		tl_pte_pa = ODIN_IOMMU_GRAPHIC_3RD_TBL_BASE +	\
						(l3start_offset<<3) + (tl_offset<<3);
		*sl_pte = ( tl_pte_pa & NEXT_LEVEL_PAGE_ADDR_MASK ) | LDS2_PGT;

		/* dmac_flush_range(sl_pte, sl_pte + 1); */

		*tl_pte =
			(pa&OUTPUT_PHYS_ADDR_MASK) | LDS2_MEM | LDS2_CONT(0x0) | LDS2_TYPE(0x1);
		/* dmac_flush_range(tl_pte, tl_pte + 1); */

	}else{	/* SZ_2M */

		sl_table = priv->pgtable;
		l2start_offset = ( va >> FL_ADDR_BIT_SHIFT ) << 9;
		sl_offset = SL_ADDR_OFFSET(va);
		sl_pte = sl_table + l2start_offset + sl_offset;

		*sl_pte =
			(pa&OUTPUT_PHYS_ADDR_MASK) | LDS2_MEM | LDS2_CONT(0x0) | LDS2_TYPE(0x0);
		/* dmac_flush_range(sl_pte, sl_pte + 1); */

	}

	/* spin_unlock_irqrestore( &odin_iommu_map_lock, flags );	*/
	return ret;

}


static size_t odin_iommu_unmap(struct iommu_domain *domain, unsigned long va,
		    							size_t size)
{

	/* Avoid every calculating & error checking. We're so busy!! */

	struct odin_priv *priv;
	/* unsigned long flags;	*/

	u32 l2start_offset;
	u64 *sl_table;
	u64 *sl_pte;
	u32 sl_offset;

#ifdef	CONFIG_ODIN_ION_SMMU_4K
	u32 l3start_offset;
	u64 *tl_table;
	u32 tl_offset;
	u64 *tl_pte;
#endif

	int ret=0;


	/* spin_lock_irqsave( &odin_iommu_map_lock, flags );	*/

	priv = domain->priv;

	if ( likely(size == SZ_4K) ){

		sl_table = priv->pgtable;
		l2start_offset = ( va >> FL_ADDR_BIT_SHIFT ) << 9;
		sl_offset = SL_ADDR_OFFSET(va);
		sl_pte = sl_table + l2start_offset + sl_offset;

		tl_table = priv->pgtable_3rd_level;
		l3start_offset = ( va >> SL_ADDR_BIT_SHIFT ) << 9;
		tl_offset = TL_ADDR_OFFSET(va);
		tl_pte = tl_table + l3start_offset + tl_offset;

		*sl_pte = 0;
		/* dmac_flush_range(sl_pte, sl_pte + 1); */

		*tl_pte = 0;
		/* dmac_flush_range(tl_pte, tl_pte + 1); */

	}else{	/* SZ_2M */

		sl_table = priv->pgtable;
		l2start_offset = ( va >> FL_ADDR_BIT_SHIFT ) << 9;
		sl_offset = SL_ADDR_OFFSET(va);
		sl_pte = sl_table + sl_offset + l2start_offset;

		if ( unlikely((*sl_pte) == 0) ){
			/* When non-error status, unmapped size should be returned. */
			goto fail;
		}

		*sl_pte = 0;
		/* dmac_flush_range(sl_pte, sl_pte + 1); */

	}

	ret = __flush_iotlb_va( domain, 0 );
	if ( unlikely(ret != 0) )
		ret = 0;	/* When non-error status, unmapped size should be returned. */
	else
		ret = size;

fail:
	/* spin_unlock_irqrestore( &odin_iommu_map_lock, flags );	*/

	return (size_t)ret;

}


static int odin_iommu_attach_dev(struct iommu_domain *domain,
										struct device *dev)
{

	struct odin_priv *priv;
	struct odin_iommu_ctx_dev *ctx_dev;
	struct odin_iommu_drvdata *iommu_drvdata;
	struct odin_iommu_ctx_drvdata *ctx_drvdata;
	struct odin_iommu_ctx_drvdata *tmp_drvdata;
	int ret = 0;
	unsigned long flags;


	spin_lock_irqsave(&odin_iommu_lock, flags);

	priv = domain->priv;

	if (!priv || !dev) {
		ret = -EINVAL;
		goto fail;
	}

	iommu_drvdata = dev_get_drvdata(dev->parent);
	ctx_drvdata = dev_get_drvdata(dev);
	ctx_dev = dev->platform_data;

	if (!iommu_drvdata || !ctx_drvdata || !ctx_dev) {
		ret = -EINVAL;
		goto fail;
	}

	if (!list_empty(&ctx_drvdata->attached_elm)) {
		ret = -EBUSY;
		goto fail;
	}

	if ( unlikely(get_odin_debug_v_message() == true) ){
		pr_info("[II] odin_iommu_attach_dev() called : ctx_name %s\n",	\
														ctx_dev->name);
	}

	list_for_each_entry(tmp_drvdata, &priv->list_attached, attached_elm)
		if (tmp_drvdata == ctx_drvdata) {
			ret = -EBUSY;
			goto fail;
		}

	/* static memory mapping ======================================== */

	if ( (iommu_drvdata->irq!=IOMMU_NS_IRQ_DSS_0) &&
		(iommu_drvdata->irq!=IOMMU_NS_IRQ_DSS_1) &&
		(iommu_drvdata->irq!=IOMMU_NS_IRQ_DSS_2) &&
		(iommu_drvdata->irq!=IOMMU_NS_IRQ_DSS_3) ){

		odin_iommu_program_context(iommu_drvdata->base, ctx_dev->num,
						iommu_drvdata->ncb,
						odin_iommu_get_pt_addr_from_irq(iommu_drvdata->irq),
						iommu_drvdata->irq, 0);

	}

	odin_iommu_permit_tlb_flush(iommu_drvdata->irq);

	list_add(&(ctx_drvdata->attached_elm), &priv->list_attached);

	__flush_iotlb_va( domain, 0 );

fail:
	spin_unlock_irqrestore(&odin_iommu_lock, flags);
	return ret;

}


static void odin_iommu_detach_dev(struct iommu_domain *domain,
				 struct device *dev)
{

	struct odin_priv *priv;
	struct odin_iommu_ctx_dev *ctx_dev;
	struct odin_iommu_drvdata *iommu_drvdata;
	struct odin_iommu_ctx_drvdata *ctx_drvdata;
	unsigned long flags;


	spin_lock_irqsave(&odin_iommu_lock, flags);

	priv = domain->priv;
	if ( !priv || !dev )
		goto fail;

	iommu_drvdata = dev_get_drvdata(dev->parent);
	ctx_drvdata = dev_get_drvdata(dev);
	ctx_dev = dev->platform_data;

	if ( !iommu_drvdata || !ctx_drvdata || !ctx_dev )
		goto fail;

	__flush_iotlb_va( domain, 0 );

	list_del_init(&ctx_drvdata->attached_elm);

fail:
	spin_unlock_irqrestore(&odin_iommu_lock, flags);

}


static void __get_pte( unsigned long va, u64 * sl, u64 * tl, int irq )
{

	struct iommu_domain *d =
				odin_get_iommu_domain(odin_get_domain_num_from_irq(irq));
	struct odin_priv *priv = d->priv;
	u64 *sl_table = priv->pgtable;
	u64 *tl_table = priv->pgtable_3rd_level;
	u32 l2start_offset;
	u64 *sl_pte;
	u64 sl_offset;
	u32 l3start_offset;
	u64 *tl_pte;
	u64 tl_offset;


	l2start_offset = ( va >> FL_ADDR_BIT_SHIFT ) << 9;
	sl_offset = SL_ADDR_OFFSET(va);
	sl_pte = sl_table + sl_offset + (u64)l2start_offset;
	*sl = *sl_pte;

	l3start_offset = ( va >> SL_ADDR_BIT_SHIFT ) << 9;
	tl_offset = TL_ADDR_OFFSET(va);
	tl_pte = tl_table + tl_offset + (u64)l3start_offset;
	*tl = *tl_pte;

}


#ifdef CONFIG_ODIN_SMMU_DMA
static phys_addr_t odin_iommu_iova_to_phys(
									struct iommu_domain *domain,
							       dma_addr_t iova);
#endif

irqreturn_t odin_iommu_fault_handler(int irq, void *dev_id)
{

	struct odin_iommu_drvdata		*drvdata = dev_id;
	void __iomem					*base;
	u32  							flt_iova_l=0, flt_iova_h=0;
	unsigned long					flags;
	u32								cb_fsr=0, cb_fsrrestore=0, cb_fsynr=0;
	u32								gs1_cbfrsynra=0;
	u64								sl_pte=0, tl_pte=0;


	spin_lock_irqsave(&odin_iommu_fault_lock, flags);

	if ( unlikely(!drvdata) ){
		printk( KERN_ERR "[II] Invalid device ID	\
							in context interrupt handler\n" );
		spin_unlock_irqrestore(&odin_iommu_fault_lock, flags);
		return 0;
	}
	base = drvdata->base;

	flt_iova_l = GET_CB_FAR_L( base, 0 );
	flt_iova_h = GET_CB_FAR_H( base, 0 );

	cb_fsr = GET_CB_FSR( base, 0 );
	cb_fsrrestore = GET_CB_FSRRESTORE( base, 0 );
	cb_fsynr = GET_CB_FSYNR0( base, 0 );
	gs1_cbfrsynra = GET_GS1_CBFRSYNRA_N( base, 0 );

	if (irq==IOMMU_NS_IRQ_PERI)
		tl_pte = odin_iommu_iova_to_phys(
									odin_get_iommu_domain(PERI_DOMAIN),
									(u64)flt_iova_l);
	else
		__get_pte( flt_iova_l&0xFFFFF000, &sl_pte, &tl_pte, irq );

	SET_CB_FSR( base, 0, 0xFFFFFFFF );

	spin_unlock_irqrestore(&odin_iommu_fault_lock, flags);

	printk( KERN_ERR "[II] ptw fault (%s) : %x\t%x ======================\n",			\
							drvdata->name, flt_iova_h, flt_iova_l );
	printk( KERN_ERR "cb_fsr %x\tcb_fsrrestore %x\tcb_fsynr %x\tgs1_cbfrsynra %x\n",	\
							cb_fsr, cb_fsrrestore, cb_fsynr, gs1_cbfrsynra );
	printk( KERN_ERR "%llx\t%llx\n", sl_pte, tl_pte );

	return 0;

}


#ifdef CONFIG_ODIN_SMMU_DMA
static phys_addr_t odin_iommu_iova_to_phys(
									struct iommu_domain *domain,
							       dma_addr_t iova)
{
	if (iova < (phys_addr_t)ODIN_IOMMU_SECOND_BANK_IOVA_BASE)
		return (dma_addr_t)(iova+ODIN_IOMMU_FIRST_BANK_OFFSET);
	else
		return (dma_addr_t)(iova+ODIN_IOMMU_SECOND_BANK_OFFSET);
}


struct odin_iommu_archdata {
	struct list_head attached_list;
	struct dma_iommu_mapping *iommu_mapping;
	spinlock_t attach_lock;
	struct shmobile_iommu_domain *attached;
	int num_attached_devices;
};
static struct odin_iommu_archdata *odin_archdata;


int odin_iommu_add_device(struct device *dev)
{
	struct odin_iommu_archdata *archdata = odin_archdata;
	struct dma_iommu_mapping *mapping;


	mapping = archdata->iommu_mapping;
	if (!mapping) {
		mapping = arm_iommu_create_mapping(
						&platform_bus_type, EMMC_ARM_DMA_IOMMU_POOL_START,
						EMMC_ARM_DMA_IOMMU_POOL_SIZE, 0);

		if (IS_ERR(mapping)){
			printk("[II] PERI create mapping error");
			return PTR_ERR(mapping);
		}
		archdata->iommu_mapping = mapping;
	}

	dev->archdata.iommu = archdata;
	if (arm_iommu_attach_device(dev, mapping))
		pr_err("[II] PERI arm_iommu_attach_device failed\n");

	return 0;
}
#endif


static struct iommu_ops odin_iommu_ops = {
	.domain_init = odin_iommu_domain_init,
	.domain_destroy = odin_iommu_domain_destroy,
	.attach_dev = odin_iommu_attach_dev,
	.detach_dev = odin_iommu_detach_dev,
	.map = odin_iommu_map,
	.unmap = odin_iommu_unmap,
#ifdef CONFIG_ODIN_SMMU_DMA
	.iova_to_phys = odin_iommu_iova_to_phys,
#endif
	.pgsize_bitmap = ODIN_IOMMU_PGSIZES,
};


static int __init odin_iommu_init(void)
{

#ifdef CONFIG_ODIN_SMMU_DMA
	static struct odin_iommu_archdata *archdata;

	/* TODO : insert error check code */
	archdata = kmalloc(sizeof(*archdata), GFP_KERNEL);
	spin_lock_init(&archdata->attach_lock);
	archdata->attached = NULL;
	archdata->iommu_mapping = NULL;
	odin_archdata = archdata;
#endif

	bus_set_iommu(&platform_bus_type, &odin_iommu_ops);
	return 0;

}
subsys_initcall(odin_iommu_init);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hyunsun Hong <hyunsun.hong@lge.com>");
