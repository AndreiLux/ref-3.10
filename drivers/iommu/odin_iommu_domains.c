
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


#include <linux/iommu.h>
#include <asm/sizes.h>
#include <asm/page.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/odin_iommu.h>

#include "odin_iommu.h"
#include "odin_iommu_domains.h"
#include "odin_iommu_subsys_map.h"


struct odin_iommu_domain {
	/* iommu domain to map in */
	struct iommu_domain *domain;
	/* number of iova pools */
	int npools;
	/*
	 * array of gen_pools for allocating iovas.
	 * behavior is undefined if these overlap
	 */
	struct mem_pool *iova_pools;
};

struct {
	char *name;
	int domain;
	int irq;
} odin_iommu_ctx_names[] = {
	/* CSS */
	{
		.name = "css_0_ns",	/* read */
#ifndef	CONFIG_ODIN_ION_SMMU_4K
		.domain = CSS_DOMAIN,
#else
		.domain = GRAPHIC_DOMAIN,
#endif
		.irq = IOMMU_NS_IRQ_CSS_0,
	},
	/* CSS */
	{
		.name = "css_1_ns",	/* write */
#ifndef	CONFIG_ODIN_ION_SMMU_4K
		.domain = CSS_DOMAIN,
#else
		.domain = GRAPHIC_DOMAIN,
#endif
		.irq = IOMMU_NS_IRQ_CSS_1,
	},
	/* VSP */
	{
		.name = "vsp_0_ns",
#ifndef	CONFIG_ODIN_ION_SMMU_4K
		.domain = VSP_DOMAIN,
#else
		.domain = GRAPHIC_DOMAIN,
#endif
		.irq = IOMMU_NS_IRQ_VSP_0,
	},
	/* VSP */
	{
		.name = "vsp_1_ns",
#ifndef	CONFIG_ODIN_ION_SMMU_4K
		.domain = VSP_DOMAIN,
#else
		.domain = GRAPHIC_DOMAIN,
#endif
		.irq = IOMMU_NS_IRQ_VSP_1,
	},
	/* DSS */
	{
		.name =	"dss_0_ns",
#ifndef	CONFIG_ODIN_ION_SMMU_4K
		.domain = DSS_DOMAIN,
#else
		.domain = GRAPHIC_DOMAIN,
#endif
		.irq = IOMMU_NS_IRQ_DSS_0,
	},
	/* DSS */
	{
		.name =	"dss_1_ns",
#ifndef	CONFIG_ODIN_ION_SMMU_4K
		.domain = DSS_DOMAIN,
#else
		.domain = GRAPHIC_DOMAIN,
#endif
		.irq = IOMMU_NS_IRQ_DSS_1,
	},
	/* DSS */
	{
		.name = "dss_2_ns",	/* overlay */
#ifndef	CONFIG_ODIN_ION_SMMU_4K
		.domain = DSS_DOMAIN,
#else
		.domain = GRAPHIC_DOMAIN,
#endif
		.irq = IOMMU_NS_IRQ_DSS_2,
	},
	/* DSS */
	{
		.name = "dss_3_ns",	/* gfx */
#ifndef	CONFIG_ODIN_ION_SMMU_4K
		.domain = DSS_DOMAIN,
#else
		.domain = GRAPHIC_DOMAIN,
#endif
		.irq = IOMMU_NS_IRQ_DSS_3,
	},
	/* ICS */
	{
		.name = "ics_ns",
		.domain = ICS_DOMAIN,
		.irq = IOMMU_NS_IRQ_ICS,
	},
	/* AUD */
	{
		.name = "aud_ns",
		.domain = AUD_DOMAIN,
		.irq = IOMMU_NS_IRQ_AUD,
	},
	/* PERI */
	{
		.name = "peri_ns",
		.domain = PERI_DOMAIN,
		.irq = IOMMU_NS_IRQ_PERI,
	},
	/* SDMA */
	{
		.name = "sdma_ns",
		.domain = SDMA_DOMAIN,
		.irq = IOMMU_NS_IRQ_SDMA,
	},
	/* SES */
	{
		.name = "ses_ns",
		.domain = SES_DOMAIN,
		.irq = IOMMU_NS_IRQ_SES,
	},
};

static struct mem_pool global_pools[] =  {
	[EXCLUSIVE_POOL] =
		{
			.paddr	= EXCLUSIVE_POOL_START,
			.size	= EXCLUSIVE_POOL_SIZE,
		},
};

#ifndef	CONFIG_ODIN_ION_SMMU_4K
static struct mem_pool css_pools[] =  {
#ifndef CONFIG_ODIN_ION_I_TUNER
	[SYSTEM_POOL] =
		{
			.paddr	= SYSTEM_POOL_START,
			.size	= SYSTEM_POOL_SIZE,
		},
#else
	[CARVEOUT_POOL] =
		{
			.paddr	= CSS_CARVEOUT_POOL_START,
			.size	= CSS_CARVEOUT_POOL_SIZE,
		},
	[SYSTEM_POOL] =
		{
			.paddr	= CSS_SYSTEM_POOL_START,
			.size	= CSS_SYSTEM_POOL_SIZE,
		},
#endif
};

static struct mem_pool vsp_pools[] =  {
	[SYSTEM_POOL] =
		{
			.paddr	= SYSTEM_POOL_START,
			.size	= SYSTEM_POOL_SIZE,
		},
};
#endif

static struct mem_pool dss_pools[] =  {
#ifndef	CONFIG_ODIN_ION_SMMU_4K
	[CARVEOUT_POOL] =
		{
			.paddr	= DSS_CARVEOUT_POOL_START,
			.size	= DSS_CARVEOUT_POOL_SIZE,
		},
	[SYSTEM_POOL] =
		{
			.paddr	= DSS_SYSTEM_POOL_START,
			.size	= DSS_SYSTEM_POOL_SIZE,
		},
#else
	[CARVEOUT_POOL] =
		{
			.paddr	= DSS_CARVEOUT_POOL_START,
			.size	= DSS_CARVEOUT_POOL_SIZE,
		},
	[SYSTEM_POOL] =
		{
			.paddr	= DSS_SYSTEM_POOL_START,
			.size	= DSS_SYSTEM_POOL_SIZE,
		},
#endif
};

static struct mem_pool ics_pools[] =  {
	[EXCLUSIVE_POOL] =
		{
			.paddr	= EXCLUSIVE_POOL_START,
			.size	= EXCLUSIVE_POOL_SIZE,
		},
};

static struct mem_pool aud_pools[] =  {
	[EXCLUSIVE_POOL] =
		{
			.paddr	= AUD_EXCLUSIVE_POOL_START,
			.size	= AUD_EXCLUSIVE_POOL_SIZE,
		},
};

static struct mem_pool peri_pools[] =  {
	[EXCLUSIVE_POOL] =
		{
			.paddr	= EXCLUSIVE_POOL_START,
			.size	= EXCLUSIVE_POOL_SIZE,
		},
};

static struct mem_pool sdma_pools[] =  {
	[EXCLUSIVE_POOL] =
		{
			.paddr	= EXCLUSIVE_POOL_START,
			.size	= EXCLUSIVE_POOL_SIZE,
		},
};

static struct mem_pool ses_pools[] =  {
	[EXCLUSIVE_POOL] =
		{
			.paddr	= EXCLUSIVE_POOL_START,
			.size	= EXCLUSIVE_POOL_SIZE,
		},
};


static struct odin_iommu_domain odin_iommu_domains[] = {
	[GLOBAL_DOMAIN] = {
		.iova_pools = global_pools,
		.npools = ARRAY_SIZE(global_pools),
	},
#ifndef	CONFIG_ODIN_ION_SMMU_4K
	[CSS_DOMAIN] = {
		.iova_pools = css_pools,
		.npools = ARRAY_SIZE(css_pools),
	},
	[VSP_DOMAIN] = {
		.iova_pools = vsp_pools,
		.npools = ARRAY_SIZE(vsp_pools),
	},
	[DSS_DOMAIN] = {
		.iova_pools = dss_pools,
		.npools = ARRAY_SIZE(dss_pools),
	},
#else
	[GRAPHIC_DOMAIN] = {
		.iova_pools = dss_pools,
		.npools = ARRAY_SIZE(dss_pools),
	},
#endif
	[ICS_DOMAIN] = {
		.iova_pools = ics_pools,
		.npools = ARRAY_SIZE(ics_pools),
	},
	[AUD_DOMAIN] = {
		.iova_pools = aud_pools,
		.npools = ARRAY_SIZE(aud_pools),
	},
	[PERI_DOMAIN] = {
		.iova_pools = peri_pools,
		.npools = ARRAY_SIZE(peri_pools),
	},
	[SDMA_DOMAIN] = {
		.iova_pools = sdma_pools,
		.npools = ARRAY_SIZE(sdma_pools),
	},
	[SES_DOMAIN] = {
		.iova_pools = ses_pools,
		.npools = ARRAY_SIZE(ses_pools),
	},
};


bool odin_iommu_check_non_graphic_domain(struct iommu_domain *domain)
{
	struct odin_priv *priv = domain->priv;
	if (!priv->pgtable)
		return true;
	else
		return false;
}


/* function for debugging */
char * odin_get_domain_name_from_subsys( unsigned int ui_subsys_id )
{

	if ( ui_subsys_id & ODIN_SUBSYS_CSS ){
		return "css";
	}else if ( ui_subsys_id & ODIN_SUBSYS_VSP ){
		return "vsp";
	}else if ( ui_subsys_id & ODIN_SUBSYS_DSS ){
		return "dss";
	}

	return "";

}
EXPORT_SYMBOL(odin_get_domain_name_from_subsys);


#ifndef	CONFIG_ODIN_ION_SMMU_4K
/* function for debugging */
char * odin_get_domain_name_from_domain_num( int domain_no )
{

	switch ( domain_no ){

		case DSS_DOMAIN	:
			return "dss";

		case CSS_DOMAIN	:
			return "css";

		case VSP_DOMAIN	:
			return "vsp";

		case ICS_DOMAIN	:
			return "ics";

		case AUD_DOMAIN	:
			return "aud";

		case PERI_DOMAIN :
			return "peri";

		case SDMA_DOMAIN :
			return "sdma";

		case SES_DOMAIN	:
			return "ses";

		default					:
			return "";

	}

}
#endif


int odin_get_domain_num_from_irq( int irq_num )
{

	switch ( irq_num ){

#ifndef	CONFIG_ODIN_ION_SMMU_4K
		case IOMMU_NS_IRQ_CSS_0	:
		case IOMMU_NS_IRQ_CSS_1	:
			return CSS_DOMAIN;

		case IOMMU_NS_IRQ_VSP_0	:
		case IOMMU_NS_IRQ_VSP_1	:
			return VSP_DOMAIN;

		case IOMMU_NS_IRQ_DSS_0	:
		case IOMMU_NS_IRQ_DSS_1	:
		case IOMMU_NS_IRQ_DSS_2	:
		case IOMMU_NS_IRQ_DSS_3	:
			return DSS_DOMAIN;
#else
		case IOMMU_NS_IRQ_CSS_0	:
		case IOMMU_NS_IRQ_CSS_1	:
		case IOMMU_NS_IRQ_VSP_0	:
		case IOMMU_NS_IRQ_VSP_1	:
		case IOMMU_NS_IRQ_DSS_0	:
		case IOMMU_NS_IRQ_DSS_1	:
		case IOMMU_NS_IRQ_DSS_2	:
		case IOMMU_NS_IRQ_DSS_3	:
			return GRAPHIC_DOMAIN;
#endif

		case IOMMU_NS_IRQ_ICS	:
			return ICS_DOMAIN;

		case IOMMU_NS_IRQ_AUD	:
			return AUD_DOMAIN;

		case IOMMU_NS_IRQ_PERI	:
			return PERI_DOMAIN;

		case IOMMU_NS_IRQ_SDMA	:
			return SDMA_DOMAIN;

		case IOMMU_NS_IRQ_SES	:
			return SES_DOMAIN;

		default					:
			return 0;

	}

}

#ifdef	CONFIG_ODIN_ION_SMMU_4K
int odin_get_power_domain_num_from_irq( int irq_num )
{

	switch ( irq_num ){

		case IOMMU_NS_IRQ_CSS_0	:
		case IOMMU_NS_IRQ_CSS_1	:
			return CSS_POWER_DOMAIN;

		case IOMMU_NS_IRQ_VSP_0	:
		case IOMMU_NS_IRQ_VSP_1	:
			return VSP_POWER_DOMAIN;

		case IOMMU_NS_IRQ_DSS_0	:
		case IOMMU_NS_IRQ_DSS_1	:
		case IOMMU_NS_IRQ_DSS_2	:
		case IOMMU_NS_IRQ_DSS_3	:
			return DSS_POWER_DOMAIN;

		case IOMMU_NS_IRQ_ICS	:
			return ICS_POWER_DOMAIN;

		case IOMMU_NS_IRQ_AUD	:
			return AUD_POWER_DOMAIN;

		case IOMMU_NS_IRQ_PERI	:
			return PERI_POWER_DOMAIN;

		case IOMMU_NS_IRQ_SDMA	:
			return SDMA_POWER_DOMAIN;

		case IOMMU_NS_IRQ_SES	:
			return SES_POWER_DOMAIN;

		default					:
			return 0;

	}

}
#endif

struct iommu_domain *odin_get_iommu_domain(int domain_num)
{
	if ( likely(domain_num >= 0 && domain_num < MAX_DOMAINS) )
		return odin_iommu_domains[domain_num].domain;
	else
		return NULL;
}

unsigned long odin_get_domain_num_from_subsys(
					unsigned int ui_subsys_id)
{

#ifndef	CONFIG_ODIN_ION_SMMU_4K
	if ( ui_subsys_id & ODIN_SUBSYS_CSS ){
		return CSS_DOMAIN;
	}else if ( ui_subsys_id & ODIN_SUBSYS_VSP ){
		return VSP_DOMAIN;
	}else if ( ui_subsys_id & ODIN_SUBSYS_DSS ){
		return DSS_DOMAIN;
	}

	return GLOBAL_DOMAIN;
#else
	return GRAPHIC_DOMAIN;
#endif


}

unsigned long odin_allocate_iova_address(unsigned int iommu_domain,
					unsigned int partition_no,
					unsigned long size,
					phys_addr_t first_pa)
{

	struct mem_pool		*pool;
	unsigned long 		iova;


	if ( unlikely(iommu_domain >= MAX_DOMAINS) )
		return 0;

	if ( unlikely(partition_no >= odin_iommu_domains[iommu_domain].npools) )
		return 0;

	pool = &odin_iommu_domains[iommu_domain].iova_pools[partition_no];

	if ( unlikely(!pool->gpool) )
		return 0;

	iova = gen_pool_alloc( pool->gpool, size );

#ifndef	CONFIG_ODIN_ION_SMMU_4K
	if ( unlikely(get_odin_debug_v_message() == true) ){
#ifdef CONFIG_ARM_LPAE
		pr_info("[II] alloc IOVA\t%9lx\t%lx\t%s\t(p %llx)\n",
#else
		pr_info("[II] alloc IOVA\t%9lx\t%lx\t%s\t(p %lx)\n",
#endif
							iova, size,
							odin_get_domain_name_from_domain_num(iommu_domain),
							first_pa);
	}
#endif

	if ( likely(iova) )
		pool->free -= size;

	return iova;

}

void odin_free_iova_address(unsigned long iova,
			   unsigned int iommu_domain,
			   unsigned int partition_no,
			   unsigned long size,
			   phys_addr_t first_pa)
{

	struct mem_pool *pool;


	if ( unlikely(iommu_domain >= MAX_DOMAINS) ) {
		WARN(1, "[II] Invalid domain %d\n", iommu_domain);
		return;
	}

	if ( unlikely(partition_no >= odin_iommu_domains[iommu_domain].npools) ) {
		WARN(1, "[II] Invalid partition %d for domain %d\n",
			partition_no, iommu_domain);
		return;
	}

	pool = &odin_iommu_domains[iommu_domain].iova_pools[partition_no];

#ifndef	CONFIG_ODIN_ION_SMMU_4K
	if ( unlikely(get_odin_debug_v_message() == true) ){
#ifdef CONFIG_ARM_LPAE
		pr_info("[II] free  IOVA\t%9lx\t%lx\t%s\t(p %llx)\n",
#else
		pr_info("[II] free  IOVA\t%9lx\t%lx\t%s\t(p %lx)\n",
#endif
							iova, size,
							odin_get_domain_name_from_domain_num(iommu_domain),
							first_pa);
	}
#endif

	pool->free += size;
	gen_pool_free(pool->gpool, iova, size);

}

static int __init odin_subsystem_iommu_init( void )
{

	int i, j;

	for ( i=0; i<ARRAY_SIZE(odin_iommu_domains); i++ ){

		odin_iommu_domains[i].domain = iommu_domain_alloc( &platform_bus_type );

		if ( !odin_iommu_domains[i].domain )
			continue;

		odin_iommu_prepare_page_table(odin_iommu_domains[i].domain, i);

		for ( j=0; j<odin_iommu_domains[i].npools; j++ ){
			struct mem_pool *pool = &odin_iommu_domains[i].iova_pools[j];
			mutex_init( &pool->pool_mutex );
			pool->gpool = gen_pool_create( 21, -1 );

			if ( !pool->gpool ){
				printk( KERN_ERR "[II] odin_subsystem_iommu_init:	\
											gen_pool_create fail\n" );
				continue;
			}

			if ( gen_pool_add( pool->gpool, pool->paddr, pool->size, -1 ) ){
				printk( KERN_ERR "[II] odin_subsystem_iommu_init:	\
											gen_pool_add fail\n" );
				gen_pool_destroy(pool->gpool);
				pool->gpool = NULL;
				continue;
			}
		}

	}

	for ( i=0; i<ARRAY_SIZE(odin_iommu_ctx_names); i++ ){

		int domain_idx;
		struct device *ctx;


		if ( odin_check_iommu_enabled(odin_iommu_ctx_names[i].irq)==0 )
			continue;

		ctx = odin_iommu_get_unit_devs(
							odin_iommu_ctx_names[i].name, ITER_IOMMU_CTX );
		if ( !ctx )
			continue;

		domain_idx = odin_iommu_ctx_names[i].domain;

		if ( !odin_iommu_domains[domain_idx].domain )
			continue;

		if ( iommu_attach_device(odin_iommu_domains[domain_idx].domain, ctx) ){
			printk( KERN_ERR "[II] odin_subsystem_iommu_init:	\
										gen_pool_add fail\n" );
			continue;
		}
	}

	return 0;

	/* When should I call exitcall?? */

}
subsys_initcall(odin_subsystem_iommu_init);
