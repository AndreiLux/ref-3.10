
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


#ifndef __LINUX_ODIN_IOMMU_H__
#define __LINUX_ODIN_IOMMU_H__

#include <linux/genalloc.h>
#include <linux/ion.h>

#include <../drivers/gpu/ion/ion_priv.h>	/* not good but can't help */

/* for DMC */

#define	SMMU_WA_DESC					(0x0003FFE0)
#define	ODIN_DMC_BASE					(0x35000000)
#define	ODIN_DMC_MRR					(0x00000068)

/* 448MB	: 0x1C00_0000 */
/* 256MB	: 0x1000_0000 */

#define    ODIN_2GDDR_ION_CACHE_SIZE   (0xCE00000) /* 206 MB */
#ifndef	CONFIG_ODIN_ION_SMMU_4K
#define	ODIN_3GDDR_ION_CACHE_SIZE	(SZ_1G)
#else
#define	ODIN_3GDDR_ION_CACHE_SIZE	(ODIN_2GDDR_ION_CACHE_SIZE)
#endif


/* pool's protect area */

#define	ODIN_DRM_ALWAYS_PROTECT_SZ		\
							(ODIN_2GDDR_ION_CACHE_SIZE-(SZ_8M*3*3))
#define	ODIN_DRM_SWITCH_PROTECT_SZ		(SZ_8M*3)
#define	ODIN_DRM_ALWAYS_NOT_PROTECT_SZ	(SZ_8M*3*2)

#define	ODIN_DRM_ALWAYS_PROTECT_CNT		(ODIN_DRM_ALWAYS_PROTECT_SZ/SZ_2M)
#define	ODIN_DRM_POOL_ENTRY_CNT			(ODIN_2GDDR_ION_CACHE_SIZE/SZ_2M)

/* struct ion_page_pool_item's drm_label */

#define	ODIN_DRM_ALWAYS_PROTECT			(1)
#define	ODIN_DRM_SWITCH_PROTECT			(2)
#define	ODIN_DRM_ALWAYS_NOT_PROTECT		(4)


/* for sMMU */

#define	DSS_FB_BASE_IOVA				(0x02000000)
#define	DSS_FB_SIZE					(0x00FD4000)

#define	DSS_FB0_BASE_PA				ODIN_ION_CARVEOUT_FB_BASE_PA

#define	DSS_CABC_DESCRIPTOR_SIZE		(0x800)
#define	DSS_CABC_DESCRIPTOR_PA		(ODIN_ION_CARVEOUT_FB_BASE_PA	\
										+ DSS_FB_SIZE - DSS_CABC_DESCRIPTOR_SIZE)
#define	DSS_CABC_DESCRIPTOR_IOVA		(DSS_FB_BASE_IOVA	\
										+ DSS_FB_SIZE - DSS_CABC_DESCRIPTOR_SIZE)

#define	ODIN_IOMMU_FIRST_BANK_BASE			(0x80000000)
#define	ODIN_IOMMU_FIRST_BANK_SIZE			(SZ_2M+SZ_512K+SZ_256K+SZ_64K+SZ_32K)
#define	ODIN_IOMMU_FIRST_BANK_OFFSET			(0x40000000)
#define	ODIN_IOMMU_SECOND_BANK_BASE			(0x100000000)
#define	ODIN_IOMMU_SECOND_BANK_SIZE			(SZ_2M+SZ_1M)
#define	ODIN_IOMMU_SECOND_BANK_OFFSET		(0x60000000)
#define	ODIN_IOMMU_SECOND_BANK_IOVA_BASE	(0xA0000000)

#ifdef CONFIG_ARM_LPAE
#define	ODIN_MAX_PA_VALUE			(0xFFFFFFFFFFFFFFFF)
#else
#define	ODIN_MAX_PA_VALUE			(0xFFFFFFFF)
#endif

/* This is used for odin_iommu_ops */
#define ODIN_IOMMU_PGSIZES	( SZ_4K | /*SZ_64K |*/ SZ_2M /*| SZ_32M*/ )


#ifndef	CONFIG_ODIN_ION_SMMU_4K
enum {
	GLOBAL_DOMAIN,
	CSS_DOMAIN,
	VSP_DOMAIN,
	DSS_DOMAIN,
	ICS_DOMAIN,
	AUD_DOMAIN,
	PERI_DOMAIN,
	SDMA_DOMAIN,
	SES_DOMAIN,
	MAX_DOMAINS
};
#else
enum {
	GLOBAL_DOMAIN,
	GRAPHIC_DOMAIN,
	ICS_DOMAIN,
	AUD_DOMAIN,
	PERI_DOMAIN,
	SDMA_DOMAIN,
	SES_DOMAIN,
	MAX_DOMAINS
};

enum {
	GLOBAL_POWER_DOMAIN,
	CSS_POWER_DOMAIN,
	VSP_POWER_DOMAIN,
	DSS_POWER_DOMAIN,
	ICS_POWER_DOMAIN,
	AUD_POWER_DOMAIN,
	PERI_POWER_DOMAIN,
	SDMA_POWER_DOMAIN,
	SES_POWER_DOMAIN,
	MAX_POWER_DOMAINS
};
#endif


enum odin_smmu_alloc_sz_idx {
	ALLOC_PG_SZ_ZERO		= 0,
	ALLOC_PG_SZ_2M			= 1,
	ALLOC_PG_SZ_BIGGEST	= 2
};


enum {
	ODIN_ALLOC_BUFFER		= 0,
	ODIN_MAP_BUFFER		= 1,
};


enum {
	CARVEOUT_POOL		= 0,
	SYSTEM_POOL		= 1,
	EXCLUSIVE_POOL		= 2,
};


#define	IOMMU_NS_IRQ_CSS_0	184		/* 0xB8	*/
#define	IOMMU_NS_IRQ_CSS_1	186		/* 0xBA	*/
#define	IOMMU_NS_IRQ_VSP_0	143		/* 0x8F	*/
#define	IOMMU_NS_IRQ_VSP_1	151		/* 0x97	*/
#define	IOMMU_NS_IRQ_DSS_0	153		/* 0x99	*/
#define	IOMMU_NS_IRQ_DSS_1	155		/* 0x9B	*/
#define	IOMMU_NS_IRQ_DSS_2	157		/* 0x9D	*/
#define	IOMMU_NS_IRQ_DSS_3	159		/* 0x9F	*/
#define	IOMMU_NS_IRQ_ICS		112		/* 0x70	*/
#define	IOMMU_NS_IRQ_AUD		87		/* 0x57	*/
#define	IOMMU_NS_IRQ_PERI		39		/* 0x27	*/
#define	IOMMU_NS_IRQ_SDMA		236		/* 0xEC	*/
#define	IOMMU_NS_IRQ_GPU_0	243		/* 0xF3	*/
#define	IOMMU_NS_IRQ_GPU_1	269		/* 0x10D	*/
#define	IOMMU_NS_IRQ_SES		100		/* 0x64	*/

enum odin_subsystem_id {
	ODIN_SUBSYS_INVALID				= -1,
	ODIN_SUBSYS_CSS					= 0x00000001,
	ODIN_SUBSYS_VSP					= 0x00000002,
	ODIN_SUBSYS_DSS					= 0x00000004,
	ODIN_SUBSYS_MAX 					= 0x00000200
};

struct phys_addr_list {
	phys_addr_t 		pa;
	unsigned long		size;
	struct list_head	list;
};

struct mapped_subsys_iova_list {
	unsigned int		subsys;
	unsigned long		iova;
	struct list_head 	list;
};

/* This is not very beautiful.										*/
/* But without this, Skia has to be changed too much.					*/
/* This is only for limited scenario.									*/
/* Don't use this!!												*/
/* You should keep your offset-less-user_process_va & call get-put pair.	*/
extern struct ion_buffer * odin_get_buffer_by_usr_addr(
								struct ion_client *client, void *usr_addr,
								unsigned long *offset);

extern void odin_iommu_fill_buffer_zero(
				unsigned long iova, unsigned long size );

extern void odin_flush_iotlb_sdma( void );

extern bool odin_has_dram_3gbyte( void );

extern int odin_check_iommu_enabled( int subsys_irq_num );

extern char * odin_get_domain_name_from_subsys( unsigned int ui_subsys_id );

extern struct odin_ion_buffer_pid * odin_ion_sort_latest_buffer_user( void );

extern void odin_ion_cleaning_sort_result( struct odin_ion_buffer_pid *head );

extern void odin_ion_kill_grbuf_owner( void );

extern void odin_print_ion_unused_buffer( void );

extern void odin_print_ion_used_buffer_sorted( void );

extern void odin_print_ion_used_buffer( void );

extern void odin_iommu_prohibit_tlb_flush( int domain_num );

extern void odin_iommu_permit_tlb_flush( int irq_num );

extern void odin_iommu_wakeup_from_idle( int irq );

extern bool get_odin_debug_nv_message( void );

extern void set_odin_debug_nv_message( bool val );

extern bool get_odin_debug_v_message( void );

extern void set_odin_debug_v_message( bool val );

extern bool get_odin_debug_bh_message( void );

extern void set_odin_debug_bh_message( bool val );

extern int odin_ion_map_iommu( struct ion_handle *handle,
				unsigned int subsys );

extern int odin_subsystem_map_buffer( unsigned int subsys, int pool_num,
				unsigned long size, struct ion_buffer *buffer, unsigned long align );

extern int odin_subsystem_unmap_buffer( struct gen_pool * smmu_heap_pool,
				struct list_head * subsys_iova_list, int pool_num );

extern void odin_iommu_program_context(void __iomem *base, int ctx, int ncb,
				phys_addr_t pgtable, int irq, int redirect);

#ifndef	CONFIG_ODIN_ION_SMMU_4K
extern char * odin_get_domain_name_from_domain_num( int domain_no );
#else
extern int odin_get_power_domain_num_from_irq( int irq_num );
#endif

extern int odin_get_domain_num_from_irq( int irq_num );

extern struct iommu_domain *odin_get_iommu_domain(int domain_num);

extern int odin_iommu_add_device(struct device *dev);

extern bool odin_ion_buffer_from_big_page_pool(
				unsigned int map_subsys, size_t size, unsigned int flags );

extern unsigned int odin_iommu_get_id_from_irq( int irq_num );

#endif /* __LINUX_ODIN_IOMMU_H__ */

