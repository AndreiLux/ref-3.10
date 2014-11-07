
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


#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/iommu.h>
#include <linux/highmem.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/odin_iommu.h>

#include <asm/mach/map.h>


#define PAGE_SIZE_2M_ALIGN(addr)		ALIGN(addr, SZ_2M)


extern unsigned long odin_smmu_alloc_sz[ALLOC_PG_SZ_BIGGEST];


struct ion_odin_carveout_smmu_heap {
	struct ion_heap		heap;
	struct gen_pool		*pool;
	unsigned long	 	base;	/* ion_phys_addr_t 	base */
};

static int ion_carveout_smmu_heap_allocate(struct ion_heap *heap,
					struct ion_buffer *buffer,
					unsigned long size, unsigned long align,
					unsigned long flags,
					unsigned int map_subsys)
{

	unsigned long aligned_size;
	struct ion_odin_carveout_smmu_heap *odin_smmu_heap;
	unsigned long offset;
	int ret;


	INIT_LIST_HEAD( &(buffer->subsys_iova_list) );

	aligned_size = PAGE_SIZE_2M_ALIGN( size );
	odin_smmu_heap = container_of( heap,
									struct ion_odin_carveout_smmu_heap, heap );

	offset = gen_pool_alloc( odin_smmu_heap->pool, aligned_size );
	if ( likely(offset!=0) ){
		if ( unlikely(get_odin_debug_v_message() == true) ){
			pr_info("[II] alloc PA carve_smmu heap : %lx \t %lx\n",
													offset, aligned_size);
		}
	} else {
		return -ENOMEM;
	}
	buffer->priv_phys = offset;

	if ( map_subsys != 0 ){

		ret = odin_subsystem_map_buffer( map_subsys, CARVEOUT_POOL,
											aligned_size, buffer, 0 );

		if ( unlikely(ret != 0) ){
			goto map_buffer_fail;
		}else{
			return 0;
		}

	}else{
		return 0;
	}

map_buffer_fail :

	/* Carveout_smmu heap is only for hardwares. */
	/* So, if mapping a buffer failed, the buffer is useless. */
	gen_pool_free( odin_smmu_heap->pool, offset, aligned_size );

	return -ENOMEM;

}

static void ion_carveout_smmu_heap_free(struct ion_buffer *buffer)
{
	struct ion_odin_carveout_smmu_heap *odin_smmu_heap;

	odin_smmu_heap = container_of( buffer->heap,
										struct ion_odin_carveout_smmu_heap, heap );
	if ( odin_subsystem_unmap_buffer( odin_smmu_heap->pool,
										&(buffer->subsys_iova_list), CARVEOUT_POOL ) < 0 ){
		/* Although error occured, I have nothing to do.	*/
		/* Should I reboot system?? 					*/
	}

	gen_pool_free( odin_smmu_heap->pool, buffer->priv_phys, buffer->size );
}

static struct sg_table * ion_carveout_smmu_heap_map_dma(
										struct ion_heap *heap,
										struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}
	sg_set_page(table->sgl, phys_to_page(buffer->priv_phys), buffer->size, 0);

	/* moved from ion_buffer_create( ) */
	sg_dma_address(table->sgl) = sg_phys(table->sgl);

	return table;
}

static void ion_carveout_smmu_heap_unmap_dma(struct ion_heap *heap,
										struct ion_buffer *buffer)
{
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
	buffer->sg_table = NULL;
}

static int ion_carveout_smmu_heap_map_iommu(struct ion_buffer *buffer,
			     						unsigned int subsys)
{
	int ret = 0;

	ret = odin_subsystem_map_buffer( subsys, CARVEOUT_POOL,
										buffer->size, buffer, 0 );

	return ret;
}

static int ion_carveout_smmu_heap_map_user( struct ion_heap *mapper,
											struct ion_buffer *buffer,
											struct vm_area_struct *vma )
{
	/* This function serves FrameBuffer only */

	return remap_pfn_range(vma, vma->vm_start,
				__phys_to_pfn(buffer->priv_phys) + vma->vm_pgoff,
				vma->vm_end - vma->vm_start,
				pgprot_noncached(vma->vm_page_prot));

}

static void *ion_carveout_smmu_heap_map_kernel(struct ion_heap *heap,
				   struct ion_buffer *buffer)
{

	int mtype = MT_MEMORY_NONCACHED;
	unsigned long pa = buffer->priv_phys;
	size_t size;


	if (pa==ODIN_ION_CARVEOUT_FB_BASE_PA)
		size = DSS_FB_SIZE;
	else
		size = buffer->size;

	if (buffer->flags & ION_FLAG_CACHED)
		mtype = MT_MEMORY;

	buffer->vaddr = __arm_ioremap( pa, size, mtype );

	return buffer->vaddr;

}

static void ion_carveout_smmu_heap_unmap_kernel(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	__arm_iounmap(buffer->vaddr);
	buffer->vaddr = NULL;
	return;
}

static int ion_carveout_smmu_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  unsigned long *addr, size_t *len)	/* ion_phys_addr_t *addr */
{
	*addr = buffer->priv_phys;
	*len = buffer->size;
	return 0;
}

static struct ion_heap_ops carveout_smmu_heap_ops = {
	.allocate		= ion_carveout_smmu_heap_allocate,
	.free			= ion_carveout_smmu_heap_free,
	.map_dma		= ion_carveout_smmu_heap_map_dma,
	.unmap_dma		= ion_carveout_smmu_heap_unmap_dma,
	.map_iommu 		= ion_carveout_smmu_heap_map_iommu,
	.map_user		= ion_carveout_smmu_heap_map_user,
	.map_kernel		= ion_carveout_smmu_heap_map_kernel,
	.unmap_kernel	= ion_carveout_smmu_heap_unmap_kernel,
	.phys			= ion_carveout_smmu_heap_phys,
};

struct ion_heap *ion_carveout_smmu_heap_create(
											struct ion_platform_heap *heap_data)
{

	struct ion_odin_carveout_smmu_heap		*odin_smmu_heap;

	odin_smmu_heap = kzalloc(sizeof(struct ion_odin_carveout_smmu_heap),
											GFP_KERNEL);
	if (!odin_smmu_heap)
		return ERR_PTR(-ENOMEM);

	odin_smmu_heap->pool = gen_pool_create(21, -1);
	if (!odin_smmu_heap->pool) {
		kfree(odin_smmu_heap);
		return ERR_PTR(-ENOMEM);
	}
	odin_smmu_heap->base = heap_data->base;
	gen_pool_add(odin_smmu_heap->pool, odin_smmu_heap->base, heap_data->size, -1);

	odin_smmu_heap->heap.ops = &carveout_smmu_heap_ops;
	odin_smmu_heap->heap.type = ION_HEAP_TYPE_CARVEOUT_SMMU;

	return &odin_smmu_heap->heap;

}

void ion_carveout_smmu_heap_destroy(struct ion_heap *heap)
{

	struct ion_odin_carveout_smmu_heap *odin_smmu_heap =
		container_of(heap, struct  ion_odin_carveout_smmu_heap, heap);

	gen_pool_destroy(odin_smmu_heap->pool);
	kfree(odin_smmu_heap);
	odin_smmu_heap = NULL;
}

