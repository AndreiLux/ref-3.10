
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
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/iommu.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/types.h>
#include <linux/odin_iommu.h>

#include "odin_iommu_hw.h"
#include "odin_iommu.h"
#include "odin_iommu_domains.h"
#include <linux/platform_data/odin_tz.h>


static void __iomem *odin_sdma_iommu_base;

static bool odin_dram_3gbyte;

static struct platform_device *odin_iommu_root_dev;

static LIST_HEAD(odin_iommu_units);
static LIST_HEAD(odin_iommu_ctxs);

struct odin_iommus {
	const char			*name;
	struct device		*dev;
    struct list_head	list;
};


void odin_flush_iotlb_sdma( void )
{
	SET_GS0_TLBIALLNSNH( odin_sdma_iommu_base, 0 );
}
EXPORT_SYMBOL(odin_flush_iotlb_sdma);


bool odin_has_dram_3gbyte( void )
{
	return odin_dram_3gbyte;
}
EXPORT_SYMBOL(odin_has_dram_3gbyte);


struct device * odin_iommu_get_unit_devs( const char * name, int unit_or_ctx )
{

	struct list_head	*unit_head;
	struct list_head	*head;
	struct odin_iommus	*unit;


	if ( unit_or_ctx == ITER_IOMMU_UNIT ){
		head = &odin_iommu_units;
	}else{
		head = &odin_iommu_ctxs;
	}

	list_for_each( unit_head, head )
	{

		unit = list_entry( unit_head, struct odin_iommus, list );

		if ( !strcmp( unit->name, name ) ){
			return unit->dev;
		}

	}

	return NULL;

}
EXPORT_SYMBOL(odin_iommu_get_unit_devs);


/* change this function to set resiter with secure API */
#ifndef CONFIG_ODIN_TEE
static void odin_iommu_set_secure_register( void __iomem * regs_base,
											int irq )
{

	/* if ( irq == IOMMU_NS_IRQ_DSS_3 )						*/
	/*	read_dss3_one_byte( sdma_base, smmu_wa_desc );		*/

	SET_GS0_CR0( regs_base,
					GS0_CR0_NSCFG_BIT(0x0)			|
					GS0_CR0_WACFG_BIT(0x0)			| GS0_CR0_RACFG_BIT(0x0)			|
					GS0_CR0_SHCFG_BIT(0x0)			| GS0_CR0_MTCFG_BIT(0x0)			|
					GS0_CR0_MEMATTR_BIT(0xF)			| GS0_CR0_BSU_BIT(0x0) 			|
					GS0_CR0_FB_BIT(0x0)				| GS0_CR0_PTM_BIT(0x1)				|
					GS0_CR0_VMIDPNE_BIT(0x0) 			| GS0_CR0_USFCFG_BIT(0x1)			|
					GS0_CR0_GCFGFIE_BIT(0x1) 			| GS0_CR0_GCFGFRE_BIT(0x1) 		|
					GS0_CR0_GFIE_BIT(0x1)				| GS0_CR0_GFRE_BIT(0x1) 			|
					GS0_CR0_CLIENTPD_BIT(0x1) );		/* Bypassing */

	if ( irq == IOMMU_NS_IRQ_PERI ){
		SET_GS0_SCR1( regs_base,
					GS0_SCR1_SPMEM_BIT(0x1)			| GS0_SCR1_SIF_BIT(0x1)			|
					GS0_SCR1_GEFRO_BIT(0x1)			| GS0_SCR1_GASRAE_BIT(0x0)		|
					GS0_SCR1_NSNUMIRPTO_BIT(0x1)											|
					GS0_SCR1_NSNUMSMRGO_BIT(ODIN_IOMMU_PERI_CTX_NUM)						|
					GS0_SCR1_NSNUMCBO_BIT(ODIN_IOMMU_PERI_CTX_NUM) );
	}else{
		SET_GS0_SCR1( regs_base,
					GS0_SCR1_SPMEM_BIT(0x1)			| GS0_SCR1_SIF_BIT(0x1)			|
					GS0_SCR1_GEFRO_BIT(0x1)			| GS0_SCR1_GASRAE_BIT(0x0)		|
					GS0_SCR1_NSNUMIRPTO_BIT(0x1)		| GS0_SCR1_NSNUMSMRGO_BIT(0x1)	|
					GS0_SCR1_NSNUMCBO_BIT(0x1) );
	}

	SET_GS0_ACR( regs_base,
					GS0_ACR_S2CRB_TLBEN_BIT(0x1)		| GS0_ACR_MMUDISB_TLBEN_BIT(0x1)	|
					GS0_ACR_SMTNMB_TLBEN_BIT(0x1) );

	/* initialize fault status registers */
	SET_GS0_GFSR( regs_base, 0xFFFFFFFF );

	/* invalidate secure TLBs */
	SET_GS0_STLBIALL( regs_base, 0xFFFFFFFF );

}
#endif

static void odin_iommu_set_global_register( void __iomem * regs_base,
											int irq )
{

	u32 smrg, cb;
	unsigned int cnt;


	smrg = GET_GS0_IDR0_NUMSMRG( regs_base );
	cb = GET_GS0_IDR1_NUMCB( regs_base );

#ifdef CONFIG_ODIN_TEE
	/* change NSCR0 -> CR0 when non-secure */
	SET_GS0_CR0( regs_base,
#else
	SET_GS0_NSCR0( regs_base,
#endif
					GS0_CR0_WACFG_BIT(0x0)				| GS0_CR0_RACFG_BIT(0x0)			|
					GS0_CR0_SHCFG_BIT(0x0)				| GS0_CR0_MTCFG_BIT(0x0)			|
					GS0_CR0_MEMATTR_BIT(0xF)				| GS0_CR0_BSU_BIT(0x0) 			|
					GS0_CR0_FB_BIT(0x0)					| GS0_CR0_PTM_BIT(0x1)				|
					GS0_CR0_VMIDPNE_BIT(0x0) 				| GS0_CR0_USFCFG_BIT(0x1)			|
					GS0_CR0_GCFGFIE_BIT(0x1) 				| GS0_CR0_GCFGFRE_BIT(0x1) 		|
					GS0_CR0_GFIE_BIT(0x1)					| GS0_CR0_GFRE_BIT(0x1) 			|
					GS0_CR0_CLIENTPD_BIT(0x0) );
#ifdef CONFIG_ODIN_TEE
	/* change NSACR -> ACR when non-secure */
	SET_GS0_ACR( regs_base,
#else
	SET_GS0_NSACR( regs_base,
#endif
					GS0_ACR_S2CRB_TLBEN_BIT(0x1)			| GS0_ACR_MMUDISB_TLBEN_BIT(0x1)	|
					GS0_ACR_SMTNMB_TLBEN_BIT(0x1)		| GS0_ACR_WC2EN_BIT(0x1)			|
					GS0_ACR_WC1EN_BIT(0x1)				| GS0_ACR_PREFETCHEN_BIT(0x1) );
#ifdef CONFIG_ODIN_TEE
	/* change NSGFSR -> GFSR when non-secure */
	SET_GS0_GFSR( regs_base,
#else
	SET_GS0_NSGFSR( regs_base,
#endif
					0xFFFFFFFF );
	SET_GS0_TLBIALLNSNH( regs_base,
					0xFFFFFFFF );

	if ( irq == IOMMU_NS_IRQ_PERI ){

		/* eMMC		:	0x63		*/
		/* SDIO0		:	0xB4		*/
		/* SDIO1		:	0xF5		*/
		/* SD card	:	0x36		*/
		/* PDMA0	:	0x77		*/
		/* PDMA1	:	0xC8	*/

		SET_GS0_SMR_N(	regs_base,
					GS0_SMR_N_VALID_BIT(0x1)				| GS0_SMR_N_MASK_BIT(0x7FF0)		|
					GS0_SMR_N_ID_BIT(0x3),
					0x0 );
		SET_GS0_SMR_N(	regs_base,
					GS0_SMR_N_VALID_BIT(0x1)				| GS0_SMR_N_MASK_BIT(0x7FF0)		|
					GS0_SMR_N_ID_BIT(0x6),
					0x1 );
		SET_GS0_SMR_N(	regs_base,
					GS0_SMR_N_VALID_BIT(0x1)				| GS0_SMR_N_MASK_BIT(0x7F7F)		|
					GS0_SMR_N_ID_BIT(0x80),
					0x2 );
		SET_GS0_SMR_N(	regs_base,
					GS0_SMR_N_VALID_BIT(0x1)				| GS0_SMR_N_MASK_BIT(0x7F00)		|
					GS0_SMR_N_ID_BIT(0x77),
					0x3 );
	}else{
		SET_GS0_SMR_N(	regs_base,
					GS0_SMR_N_VALID_BIT(0x1)				| GS0_SMR_N_MASK_BIT(0x7FFF)		|
					GS0_SMR_N_ID_BIT(0x0),
					0x0 );
	}

	/* Odin use context bank 0 only */
	/* other context banks (1~7) are set when global register setting */
	if ( irq == IOMMU_NS_IRQ_PERI ){
		for ( cnt=ODIN_IOMMU_PERI_CTX_NUM; cnt<smrg; cnt++ ){
			SET_GS0_SMR_N( regs_base, 0x0, cnt );
			SET_GS0_S2CR_N( regs_base, GS0_S2CR_N_10_TYPE_BIT(0x2), cnt );
		}

		/* initialize Context Bank Attribute registers */
		for ( cnt=ODIN_IOMMU_PERI_CTX_NUM; cnt<cb; cnt++ ){
			SET_GS1_CBAR_N( regs_base, 0x0, cnt );
		}
	}else{
		for ( cnt=1; cnt<smrg; cnt++ ){
			SET_GS0_SMR_N( regs_base, 0x0, cnt );
			SET_GS0_S2CR_N( regs_base, GS0_S2CR_N_10_TYPE_BIT(0x2), cnt );
		}

		/* initialize Context Bank Attribute registers */
		for ( cnt=1; cnt<cb; cnt++ ){
			SET_GS1_CBAR_N( regs_base, 0x0, cnt );
		}
	}

}

static void odin_iommu_set_entire_register(
										struct odin_iommu_drvdata *drvdata )
{

#ifdef CONFIG_ODIN_TEE
	tz_ops(ODIN_IOMMU_INIT, (void*)drvdata->irq);
#else
	odin_iommu_set_secure_register( drvdata->base, drvdata->irq );
#endif
	odin_iommu_set_global_register( drvdata->base, drvdata->irq );

	odin_iommu_program_context( drvdata->base, 0, drvdata->ncb,
						odin_iommu_get_pt_addr_from_irq(drvdata->irq),
										drvdata->irq, 0 );

}

static int odin_iommu_probe( struct platform_device * pdev )
{

	struct resource res;
	struct odin_iommu_drvdata *drvdata;
	struct odin_iommu_dev *iommu_dev;
	void __iomem *regs_base;
	int ret, irq;
	struct odin_iommu_dev iommu_platform_d;
	struct odin_iommus *odin_iommu_unit;


	of_property_read_string( pdev->dev.of_node, "name", &(pdev->name) );
	of_property_read_u32( pdev->dev.of_node, "id", &(pdev->id) );
	of_property_read_string( pdev->dev.of_node, "dev_platform_data_name",	\
								&(iommu_platform_d.name) );
	of_property_read_u32( pdev->dev.of_node, "dev_platform_data_ncb",	\
								&(iommu_platform_d.ncb) );

	if ( (unlikely(get_odin_debug_v_message()==true)) ||
		(unlikely(get_odin_debug_nv_message()==true)) ){
		pr_info("[II] odin_iommu_probe() called : pdev->id %d\n", pdev->id);
	}

	if (pdev->id == -1) {
		odin_iommu_root_dev = pdev;
		return 0;
	}
	pdev->dev.parent = &(odin_iommu_root_dev->dev);

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);

	if (!drvdata) {
		ret = -ENOMEM;
		goto fail;
	}

	platform_device_add_data( pdev, &iommu_platform_d,
								sizeof(struct odin_iommu_dev));
	iommu_dev = pdev->dev.platform_data;

	if (!iommu_dev) {
		ret = -ENODEV;
		goto fail;
	}

	/* ========== get register region ========== */

	ret = of_address_to_resource(pdev->dev.of_node, 0, &res);
	if (ret!=0)
		goto fail;

	regs_base = ioremap(res.start, resource_size(&res));

	if (!regs_base) {
		printk( KERN_ERR "[II] Could not ioremap: %s\n", __func__ );
		ret = -EBUSY;
		goto fail_mem;
	}

	/* ========== get irq ========== */

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);

	if (irq < 0) {
		ret = -ENODEV;
		goto fail_io;
	}

	ret = request_irq(irq, odin_iommu_fault_handler, 0,
			"odin_iommu_nonsecure_irq", drvdata);
	if (ret) {
		printk( KERN_ERR "[II] Request IRQ %d failed with ret=%d\n",	\
													irq, ret );
		goto fail_io;
	}

	/* ========== prepare sdma tlb flush ========== */

	if (irq==IOMMU_NS_IRQ_SDMA)
		odin_sdma_iommu_base = regs_base;

	/* ========== save ========== */

	drvdata->pclk = NULL;
	drvdata->clk = NULL;
	drvdata->phys_base = res.start;
	drvdata->base = regs_base;
	drvdata->irq = irq;
	drvdata->ncb = iommu_dev->ncb;
	drvdata->name = iommu_dev->name;
#ifdef	CONFIG_ODIN_ION_SMMU_4K
	drvdata->iommu_id = odin_iommu_get_id_from_irq(irq);
#endif

	platform_set_drvdata(pdev, drvdata);

	if ( ( irq != IOMMU_NS_IRQ_DSS_0 ) && \
			( irq != IOMMU_NS_IRQ_DSS_1 ) && \
			( irq != IOMMU_NS_IRQ_DSS_2 ) && \
			( irq != IOMMU_NS_IRQ_DSS_3 ) ){

		odin_iommu_set_global_register( regs_base, irq );

	}

	mb();

	odin_iommu_unit = kzalloc( sizeof(struct odin_iommus), GFP_KERNEL );
	odin_iommu_unit->dev = &(pdev->dev);
	odin_iommu_unit->name = drvdata->name;
	list_add( &odin_iommu_unit->list , &odin_iommu_units );

	return 0;

fail_io:
	iounmap(regs_base);

fail_mem:

fail:
	kfree(drvdata);
	return ret;

}

static int odin_iommu_remove(struct platform_device *pdev)
{

	struct odin_iommu_drvdata *drv = NULL;


	drv = platform_get_drvdata(pdev);
	if (drv) {
		memset(drv, 0, sizeof(*drv));
		kfree(drv);
		platform_set_drvdata(pdev, NULL);
	}
	return 0;

}

static int odin_iommu_suspend( struct platform_device * pdev,
										pm_message_t state )
{

	struct odin_iommu_drvdata *drvdata = platform_get_drvdata(pdev);


	if ( unlikely(!drvdata) )
		return 0;

#ifndef	CONFIG_ODIN_ION_SMMU_4K
	odin_iommu_prohibit_tlb_flush(
						odin_get_domain_num_from_irq(drvdata->irq) );
#else
	odin_iommu_prohibit_tlb_flush(
						odin_get_power_domain_num_from_irq(drvdata->irq) );
#endif

	return 0;

}

static int odin_iommu_resume( struct platform_device * pdev )
{

	struct odin_iommu_drvdata *drvdata = platform_get_drvdata(pdev);


	if ( unlikely(!drvdata) )
		return 0;

	odin_iommu_wakeup_from_idle(drvdata->irq);

	return 0;

}

static const char * odin_iommu_get_name_from_irq( int irq )
{

	switch ( irq ) {
		case IOMMU_NS_IRQ_DSS_0 :
			return "dss_0";
		case IOMMU_NS_IRQ_DSS_1 :
			return "dss_1";
		case IOMMU_NS_IRQ_DSS_2 :
			return "dss_2";
		case IOMMU_NS_IRQ_DSS_3 :
			return "dss_3";
		case IOMMU_NS_IRQ_CSS_0 :
			return "css_0";
		case IOMMU_NS_IRQ_CSS_1 :
			return "css_1";
		case IOMMU_NS_IRQ_VSP_0 :
			return "vsp_0";
		case IOMMU_NS_IRQ_VSP_1 :
			return "vsp_1";
		case IOMMU_NS_IRQ_ICS :
			return "ics";
		case IOMMU_NS_IRQ_AUD :
			return "aud";
		case IOMMU_NS_IRQ_PERI :
			return "peri";
		case IOMMU_NS_IRQ_SES :
			return "ses";
		case IOMMU_NS_IRQ_SDMA :
			return "sdma";
		default:
			return NULL;
	}

}

#ifndef CONFIG_ODIN_TEE
static void odin_set_niu_from_irq_num( int irq_num )
{

	void __iomem *regs_d_qos;
	void __iomem *regs_pt_qos;


	switch ( irq_num ){

		case IOMMU_NS_IRQ_DSS_0 :
				regs_d_qos = ioremap( 0x31C10000, SZ_8 );
				regs_pt_qos = ioremap( 0x31C10040, SZ_4 );
				*(unsigned long *)regs_d_qos = 0x00000000;
				*(((unsigned long *)regs_d_qos)+1) =	\
					( 0xFFFFF700 + (ADD_SW_DM12<<6) + (QOS_PRI_0<<2) + NOC_URGENCY_0 );
				*(unsigned long *)regs_pt_qos = ( (QOS_PRI_0<<2) + ADD_SW_DM12 );
				break;

		case IOMMU_NS_IRQ_DSS_1 :
				regs_d_qos = ioremap( 0x31D10000, SZ_8 );
				regs_pt_qos = ioremap( 0x31D10040, SZ_4 );
				*(unsigned long *)regs_d_qos = 0x00000000;
				*(((unsigned long *)regs_d_qos)+1) =	\
					( 0xFFFFF700 + (ADD_SW_DM12<<6) + (QOS_PRI_0<<2) + NOC_URGENCY_0 );
				*(unsigned long *)regs_pt_qos = ( (QOS_PRI_0<<2) + ADD_SW_DM12 );
				break;

		case IOMMU_NS_IRQ_DSS_2 :
				regs_d_qos = ioremap( 0x31E10000, SZ_8 );
				regs_pt_qos = ioremap( 0x31E10040, SZ_4 );
				*(unsigned long *)regs_d_qos = 0x00000000;
				*(((unsigned long *)regs_d_qos)+1) =	\
					( 0xFFFFF700 + (ADD_SW_DM12<<6) + (QOS_PRI_0<<2) + NOC_URGENCY_0 );
				*(unsigned long *)regs_pt_qos = ( (QOS_PRI_0<<2) + ADD_SW_DM12 );
				break;

		case IOMMU_NS_IRQ_DSS_3 :
				regs_d_qos = ioremap( 0x31F10000, SZ_8 );
				regs_pt_qos = ioremap( 0x31F10040, SZ_4 );
				*(unsigned long *)regs_d_qos = 0x00000000;
				*(((unsigned long *)regs_d_qos)+1) =	\
					( 0xFFFFF700 + (ADD_SW_DM12<<6) + (QOS_PRI_0<<2) + NOC_URGENCY_0 );
				*(unsigned long *)regs_pt_qos = ( (QOS_PRI_0<<2) + ADD_SW_DM12 );
				break;

		case IOMMU_NS_IRQ_CSS_0 :
				regs_d_qos = ioremap( 0x33810000, SZ_8 );
				regs_pt_qos = ioremap( 0x33810040, SZ_4 );
				*(unsigned long *)regs_d_qos = 0x00000000;
				*(((unsigned long *)regs_d_qos)+1) =	\
					( 0xFFFFF700 + (ADD_SW_GM12<<6) + (QOS_PRI_F<<2) + NOC_URGENCY_3 );
				*(unsigned long *)regs_pt_qos = ( (QOS_PRI_F<<2) + ADD_SW_GM12 );
				break;

		case IOMMU_NS_IRQ_CSS_1 :
				regs_d_qos = ioremap( 0x33910000, SZ_8 );
				regs_pt_qos = ioremap( 0x33910040, SZ_4 );
				*(unsigned long *)regs_d_qos = 0x00000000;
				*(((unsigned long *)regs_d_qos)+1) =	\
					( 0xFFFFF700 + (ADD_SW_GM12<<6) + (QOS_PRI_F<<2) + NOC_URGENCY_3 );
				*(unsigned long *)regs_pt_qos = ( (QOS_PRI_F<<2) + ADD_SW_GM12 );
				break;

		case IOMMU_NS_IRQ_VSP_0 :
				regs_d_qos = ioremap( 0x320F0000, SZ_8 );
				regs_pt_qos = ioremap( 0x320F0040, SZ_4 );
				*(unsigned long *)regs_d_qos = 0x00000000;
				*(((unsigned long *)regs_d_qos)+1) =	\
					( 0xFFFFF700 + (ADD_SW_DM12<<6) + (QOS_PRI_0<<2) + NOC_URGENCY_0 );
				*(unsigned long *)regs_pt_qos = ( (QOS_PRI_0<<2) + ADD_SW_DM12 );
				break;

		case IOMMU_NS_IRQ_VSP_1 :
				regs_d_qos = ioremap( 0x321F0000, SZ_8 );
				regs_pt_qos = ioremap( 0x321F0040, SZ_4 );
				*(unsigned long *)regs_d_qos = 0x00000000;
				*(((unsigned long *)regs_d_qos)+1) =	\
					( 0xFFFFF700 + (ADD_SW_DM12<<6) + (QOS_PRI_0<<2) + NOC_URGENCY_0 );
				*(unsigned long *)regs_pt_qos = ( (QOS_PRI_0<<2) + ADD_SW_DM12 );
				break;

		case IOMMU_NS_IRQ_ICS :
				regs_d_qos = ioremap( 0x3FF50000, SZ_8 );
				regs_pt_qos = ioremap( 0x3FF50040, SZ_4 );
				*(unsigned long *)regs_d_qos = 0x00000000;
				*(((unsigned long *)regs_d_qos)+1) =	\
					( 0xFFFFF700 + (ADD_SW_AM12<<6) + (QOS_PRI_0<<2) + NOC_URGENCY_0 );
				*(unsigned long *)regs_pt_qos = ( (QOS_PRI_0<<2) + ADD_SW_AM12 );
				break;

		case IOMMU_NS_IRQ_AUD :
				regs_d_qos = ioremap( 0x34810000, SZ_8 );
				regs_pt_qos = ioremap( 0x34810040, SZ_4 );
				*(unsigned long *)regs_d_qos = 0x00000000;
				*(((unsigned long *)regs_d_qos)+1) =	\
					( 0xFFFFF700 + (ADD_SW_AM12<<6) + (QOS_PRI_F<<2) + NOC_URGENCY_3 );
				*(unsigned long *)regs_pt_qos = ( (QOS_PRI_F<<2) + ADD_SW_AM12 );
				break;

		case IOMMU_NS_IRQ_PERI :
				regs_d_qos = ioremap( 0x20050000, SZ_8 );
				regs_pt_qos = ioremap( 0x20050080, SZ_4 );
				*(unsigned long *)regs_d_qos = 0x00000000;
				*(((unsigned long *)regs_d_qos)+1) =	\
					( 0xFFFFF700 + (ADD_SW_AM12<<6) + (QOS_PRI_0<<2) + NOC_URGENCY_0 );
				*(unsigned long *)regs_pt_qos = ( (QOS_PRI_0<<2) + ADD_SW_AM12 );
				break;

		case IOMMU_NS_IRQ_SES :
				regs_d_qos = ioremap( 0x360F0000, SZ_8 );
				regs_pt_qos = ioremap( 0x360F0040, SZ_4 );
				*(unsigned long *)regs_d_qos = 0x00000000;
				*(((unsigned long *)regs_d_qos)+1) =	\
					( 0xFFFFF700 + (ADD_SW_DM0<<6 ) + (QOS_PRI_0<<2) + NOC_URGENCY_0 );
				*(unsigned long *)regs_pt_qos = ( (QOS_PRI_0<<2) + ADD_SW_DM0  );
				break;

		/* Do I need SDMA ?? */

		default :
			return;	/* can't be reached here */

	}

	iounmap( regs_pt_qos );
	iounmap( regs_d_qos );

}
#endif


int odin_check_iommu_enabled( int subsys_irq_num )
{

	/* add case ICS, AUD, SES */

#ifdef CONFIG_ODIN_ION_SMMU
#ifndef CONFIG_ODIN_GRAPHIC_SMMU_OFF
	if ( (subsys_irq_num==IOMMU_NS_IRQ_DSS_0) ||	\
		(subsys_irq_num==IOMMU_NS_IRQ_DSS_1) ||	\
		(subsys_irq_num==IOMMU_NS_IRQ_DSS_2) ||	\
		(subsys_irq_num==IOMMU_NS_IRQ_DSS_3) ||	\
		(subsys_irq_num==IOMMU_NS_IRQ_VSP_0) ||	\
		(subsys_irq_num==IOMMU_NS_IRQ_VSP_1) ||	\
		(subsys_irq_num==IOMMU_NS_IRQ_CSS_0) ||	\
		(subsys_irq_num==IOMMU_NS_IRQ_CSS_1) ||	\
		(subsys_irq_num==IOMMU_NS_IRQ_SDMA) ){
		return 1;
	}
#endif
#endif

#if 0
	if ( subsys_irq_num==IOMMU_NS_IRQ_PERI )
		if ( odin_has_dram_3gbyte()==true )
			return 1;
#endif

	return 0;

}
EXPORT_SYMBOL(odin_check_iommu_enabled);


void odin_iommu_wakeup_from_idle( int irq )
{

	struct device *dev;
	struct odin_iommu_drvdata *drvdata;


#ifdef CONFIG_ODIN_TEE
	tz_ops(ODIN_NIU_INIT, (void*)irq);
#else
	odin_set_niu_from_irq_num( irq );
#endif

	if ( odin_check_iommu_enabled( irq )==1 ){

		dev = odin_iommu_get_unit_devs(
					odin_iommu_get_name_from_irq(irq), ITER_IOMMU_UNIT );
		drvdata = dev_get_drvdata( dev );

		odin_iommu_set_entire_register( drvdata );

		odin_iommu_permit_tlb_flush( irq );

	}

}
EXPORT_SYMBOL(odin_iommu_wakeup_from_idle);

static int odin_iommu_ctx_probe(struct platform_device *pdev)
{

	struct odin_iommu_ctx_dev *c;
	struct odin_iommu_drvdata *drvdata;
	struct odin_iommu_ctx_drvdata *ctx_drvdata = NULL;
	int ret;
	struct odin_iommu_ctx_dev iommu_platform_d;
	struct odin_iommus *odin_iommu_ctx;
	const char *dt_string_val;


	of_property_read_string( pdev->dev.of_node, "name", &(pdev->name) );
	of_property_read_u32( pdev->dev.of_node, "id", &(pdev->id) );
	of_property_read_string( pdev->dev.of_node, "dev_platform_data_name",	\
												&(iommu_platform_d.name) );
	of_property_read_u32( pdev->dev.of_node, "dev_platform_data_num",		\
												&(iommu_platform_d.num) );
	of_property_read_string( pdev->dev.of_node, "parent_name", &dt_string_val );

	if ( (unlikely(get_odin_debug_v_message()==true)) ||
		(unlikely(get_odin_debug_nv_message()==true)) ){
		pr_info("[II] odin_iommu_ctx_probe() called : pdev->id %d\n",		\
												pdev->id);
	}

	pdev->dev.parent = odin_iommu_get_unit_devs( dt_string_val, ITER_IOMMU_UNIT );
	platform_device_add_data( pdev, &iommu_platform_d,
								sizeof(struct odin_iommu_ctx_dev) );
	c = pdev->dev.platform_data;

	if (!c || !pdev->dev.parent) {
		ret = -EINVAL;
		goto fail;
	}

	drvdata = dev_get_drvdata(pdev->dev.parent);

	if (!drvdata) {
		ret = -ENODEV;
		goto fail;
	}

	ctx_drvdata = kzalloc(sizeof(*ctx_drvdata), GFP_KERNEL);
	if (!ctx_drvdata) {
		ret = -ENOMEM;
		goto fail;
	}
	ctx_drvdata->num = c->num;
	ctx_drvdata->pdev = pdev;

	INIT_LIST_HEAD(&ctx_drvdata->attached_elm);
	platform_set_drvdata(pdev, ctx_drvdata);


	odin_iommu_ctx = kzalloc( sizeof(struct odin_iommus), GFP_KERNEL );
	odin_iommu_ctx->dev = &(pdev->dev);
	odin_iommu_ctx->name = iommu_platform_d.name;
	list_add( &odin_iommu_ctx->list , &odin_iommu_ctxs );

	return 0;
fail:
	kfree(ctx_drvdata);
	return ret;

}

static int odin_iommu_ctx_remove(struct platform_device *pdev)
{

	struct odin_iommu_ctx_drvdata *drv = NULL;


	drv = platform_get_drvdata(pdev);
	if (drv) {
		memset(drv, 0, sizeof(struct odin_iommu_ctx_drvdata));
		kfree(drv);
		platform_set_drvdata(pdev, NULL);
	}
	return 0;

}

static struct of_device_id odin_iommu_match[] = {
	{ .compatible = "arm,odin_iommu" },
	{},
};

static struct platform_driver odin_iommu_driver = {
	.driver = {
		.name	= "odin_iommu",
		.of_match_table = of_match_ptr( odin_iommu_match ),
	},
	.probe		= odin_iommu_probe,
	.remove		= odin_iommu_remove,
	.suspend	= odin_iommu_suspend,
	.resume		= odin_iommu_resume,
};

static struct of_device_id odin_iommu_ctx_match[] = {
	{ .compatible = "arm,odin_iommu_ctx" },
	{},
};

static struct platform_driver odin_iommu_ctx_driver = {
	.driver = {
		.name	= "odin_iommu_ctx",
		.of_match_table = of_match_ptr( odin_iommu_ctx_match ),
	},
	.probe		= odin_iommu_ctx_probe,
	.remove		= odin_iommu_ctx_remove,
};

static int __init odin_iommu_driver_init(void)
{

	int ret;
	u32 dmc_size;


#ifdef CONFIG_ODIN_TEE
	dmc_size = tz_read(ODIN_DMC_BASE+ODIN_DMC_MRR);
#else
	void __iomem *dmc_size_reg;
	dmc_size_reg = ioremap(ODIN_DMC_BASE+ODIN_DMC_MRR, SZ_4);
	dmc_size = *(u32 *)dmc_size_reg;
	iounmap(dmc_size_reg);
#endif

	if ( ((dmc_size&0xFF)==0x1B) ||	((dmc_size&0xFF)==0x1F) )
		odin_dram_3gbyte = false;
	else
		odin_dram_3gbyte = true;

	ret = platform_driver_register(&odin_iommu_driver);
	if (ret != 0) {
		printk( KERN_ERR "[II] Failed to register IOMMU driver\n" );
		goto error;
	}

	ret = platform_driver_register(&odin_iommu_ctx_driver);
	if (ret != 0) {
		printk( KERN_ERR "[II] Failed to register IOMMU context driver\n" );
		goto iommu_driver_err;
	}

	return ret;

iommu_driver_err:
	platform_driver_unregister(&odin_iommu_driver);

error:
	return ret;

}

static void __exit odin_iommu_driver_exit(void)
{

	platform_driver_unregister(&odin_iommu_ctx_driver);
	platform_driver_unregister(&odin_iommu_driver);

}

arch_initcall(odin_iommu_driver_init);
module_exit(odin_iommu_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hyunsun Hong <hyunsun.hong@lge.com>");
