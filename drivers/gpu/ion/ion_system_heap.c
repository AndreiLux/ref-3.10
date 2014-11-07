/*
 * drivers/gpu/ion/ion_system_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/odin_iommu.h>
#include <linux/iommu.h>
#include <linux/kernel.h>
#include "ion_priv.h"


#ifdef CONFIG_ODIN_ION_SMMU
extern unsigned long odin_smmu_alloc_sz[ALLOC_PG_SZ_BIGGEST];
#endif

#ifdef CONFIG_ODIN_ION_SMMU
static unsigned int high_order_gfp_flags = (GFP_HIGHUSER |
					    __GFP_NOWARN | __GFP_NORETRY) &
					   ~__GFP_WAIT;
static unsigned int low_order_gfp_flags  = (GFP_HIGHUSER |
					 __GFP_NOWARN);
#else
static unsigned int high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO |
					    __GFP_NOWARN | __GFP_NORETRY) &
					   ~__GFP_WAIT;
static unsigned int low_order_gfp_flags  = (GFP_HIGHUSER | __GFP_ZERO |
					 __GFP_NOWARN);
#endif

#ifdef CONFIG_ODIN_ION_SMMU
#ifndef CONFIG_ODIN_GRAPHIC_SMMU_OFF

#define	ION_SZ_2M_IDX	0
#ifndef	CONFIG_ODIN_ION_SMMU_4K
static const unsigned int orders[] = {9};
#else
#define	ION_SZ_4K_IDX	1
static const unsigned int orders[] = {9, 0};
#endif

#else
static const unsigned int orders[] = {12, 11, 10, 9};
#endif
#else
static const unsigned int orders[] = {8, 4, 0};
#endif
static const int num_orders = ARRAY_SIZE(orders);
static int order_to_index(unsigned int order)
{
	int i;
	for (i = 0; i < num_orders; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

static unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

struct ion_system_heap {
	struct ion_heap heap;
	struct ion_page_pool **pools;
};

struct page_info {
	struct page *page;
	unsigned int order;
	struct list_head list;
};

#ifdef CONFIG_ODIN_ION_SMMU
#ifdef CONFIG_ODIN_ION_SMMU_4K
static int align_to_index( unsigned long align )
{
	if ( align==SZ_4K )
		return ION_SZ_4K_IDX;
	else
		return ION_SZ_2M_IDX;
}
#endif

/* only for sysfs */
struct ion_page_pool * odin_ion_get_sys_heap_pool( size_t align )
{

	struct ion_system_heap *sys_heap = container_of(
												odin_ion_get_sys_heap(),
												struct ion_system_heap,
												heap);
#ifndef	CONFIG_ODIN_ION_SMMU_4K
	return sys_heap->pools[ION_SZ_2M_IDX];
#else
	if ( align == SZ_4K )
		return sys_heap->pools[ION_SZ_4K_IDX];
	else if ( align == SZ_2M )
		return sys_heap->pools[ION_SZ_2M_IDX];
	else
		return NULL;
#endif

}
#endif

#ifndef CONFIG_ODIN_ION_SMMU
static struct page *alloc_buffer_page(struct ion_system_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order)
{
	bool cached = ion_buffer_cached(buffer);
	bool split_pages = ion_buffer_fault_user_mappings(buffer);
	struct ion_page_pool *pool = heap->pools[order_to_index(order)];
	struct page *page;

	if (!cached) {
		page = ion_page_pool_alloc(pool);
	} else {
		gfp_t gfp_flags = low_order_gfp_flags;

		if (order > 4)
			gfp_flags = high_order_gfp_flags;
		page = alloc_pages(gfp_flags, order);
		if (!page)
			return 0;
		arm_dma_ops.sync_single_for_device(NULL,
			pfn_to_dma(NULL, page_to_pfn(page)),
			PAGE_SIZE << order, DMA_BIDIRECTIONAL);
	}
	if (!page)
		return 0;

	if (split_pages)
		split_page(page, order);
	return page;
}
#else
static struct page *alloc_buffer_page(struct ion_system_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order)
{
	bool split_pages = ion_buffer_fault_user_mappings(buffer);
	struct ion_page_pool *pool = heap->pools[order_to_index(order)];
	struct page *page;


	/* MAKE_ODIN_ION_POOL - check */

#ifndef CONFIG_ODIN_ION_SMMU_4K
	page = ion_page_pool_alloc(pool, (buffer->flags&ION_FLAG_SECURE));
#else
	if ( likely(order == orders[ION_SZ_4K_IDX]) )
		page = alloc_pages(pool->gfp_mask, pool->order);
	else
		page = ion_page_pool_alloc(pool, (buffer->flags&ION_FLAG_SECURE));
#endif

	if (!page)
		return 0;

	if (split_pages)
		split_page(page, order);
	return page;
}
#endif

static void free_buffer_page(struct ion_system_heap *heap,
			     struct ion_buffer *buffer, struct page *page,
			     unsigned int order)
{
#ifndef CONFIG_ODIN_ION_SMMU
	bool cached = ion_buffer_cached(buffer);
#endif
	bool split_pages = ion_buffer_fault_user_mappings(buffer);
	int i;

	/* MAKE_ODIN_ION_POOL - start */

#ifndef CONFIG_ODIN_ION_SMMU
	if (!cached) {
		struct ion_page_pool *pool = heap->pools[order_to_index(order)];
		ion_page_pool_free(pool, page);
	} else if (split_pages) {
		for (i = 0; i < (1 << order); i++)
			__free_page(page + i);
	} else {
		__free_pages(page, order);
	}
#else
	if ( unlikely(split_pages) ) {
		for (i = 0; i < (1 << order); i++)
			__free_page(page + i);
	} else {
		struct ion_page_pool *pool = heap->pools[order_to_index(order)];
#ifndef CONFIG_ODIN_ION_SMMU_4K
		ion_page_pool_free(pool, page);
#else
		if ( order == orders[ION_SZ_4K_IDX] )
			__free_pages(page, pool->order);
		else
			ion_page_pool_free(pool, page);
#endif
	}
#endif
}

#ifndef	CONFIG_ODIN_ION_SMMU_4K
static struct page_info *alloc_largest_available(struct ion_system_heap *heap,
						 struct ion_buffer *buffer,
						 unsigned long size,
						 unsigned int max_order)
{
	struct page *page;
	struct page_info *info;
	int i;

	for (i = 0; i < num_orders; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_buffer_page(heap, buffer, orders[i]);
		if (!page)
			continue;

		info = kmalloc(sizeof(struct page_info), GFP_KERNEL);
		info->page = page;
		info->order = orders[i];
		return info;
	}
	return NULL;
}
#else
static struct page *alloc_largest_available(struct ion_system_heap *heap,
						 struct ion_buffer *buffer,
						 unsigned long size,
						 unsigned int max_order)
{
	struct page *page;

	page = alloc_buffer_page(heap, buffer, max_order);

	return page;
}
#endif

#ifdef CONFIG_ODIN_ION_SMMU
static int ion_system_heap_allocate(struct ion_heap *heap,
				     struct ion_buffer *buffer,
				     unsigned long size, unsigned long align,
				     unsigned long flags, unsigned int map_subsys)
#else
static int ion_system_heap_allocate(struct ion_heap *heap,
				     struct ion_buffer *buffer,
				     unsigned long size, unsigned long align,
				     unsigned long flags)
#endif
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table;
	struct scatterlist *sg;
	int ret;
	struct list_head pages;
#ifndef	CONFIG_ODIN_ION_SMMU_4K
	struct page_info *info, *tmp_info;
	int i = 0;
#else
	int i = size/align, j = 0;
#endif
	long size_remaining = PAGE_ALIGN(size);
#ifndef	CONFIG_ODIN_ION_SMMU_4K
	unsigned int max_order = orders[0];
#else
	unsigned int max_order = orders[align_to_index(align)];
#endif
	bool split_pages = ion_buffer_fault_user_mappings(buffer);


#ifdef CONFIG_ODIN_ION_SMMU
	INIT_LIST_HEAD( &(buffer->subsys_iova_list) );
#endif

	INIT_LIST_HEAD(&pages);
#ifndef	CONFIG_ODIN_ION_SMMU_4K
	while (size_remaining > 0) {
		info = alloc_largest_available(sys_heap, buffer, size_remaining, max_order);
		if (!info)
			goto err;
		list_add_tail(&info->list, &pages);
		size_remaining -= (1 << info->order) * PAGE_SIZE;
		max_order = info->order;
		i++;
	}

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		goto err;

	if (split_pages)
		ret = sg_alloc_table(table, PAGE_ALIGN(size) / PAGE_SIZE,
				     GFP_KERNEL);
	else
		ret = sg_alloc_table(table, i, GFP_KERNEL);

	if (ret)
		goto err1;

	sg = table->sgl;
	list_for_each_entry_safe(info, tmp_info, &pages, list) {
		struct page *page = info->page;
		if (split_pages) {
			for (i = 0; i < (1 << info->order); i++) {
				sg_set_page(sg, page + i, PAGE_SIZE, 0);
				sg = sg_next(sg);
			}
		} else {
			sg_set_page(sg, page, (1 << info->order) * PAGE_SIZE,
				    0);
			sg = sg_next(sg);
		}
		list_del(&info->list);
		kfree(info);
	}
#else
	/* remove all about "split pages" option */

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		goto err;

	ret = sg_alloc_table(table, i, GFP_KERNEL);
	if (ret)
		goto err1;

	sg = table->sgl;

	while (size_remaining > 0) {
		struct page *page;

		page = alloc_largest_available(sys_heap, buffer, size_remaining, max_order);
		if (!page)
			goto err2;
		j++;
		size_remaining -= (1 << max_order) * PAGE_SIZE;

		sg_set_page(sg, page, (1 << max_order) * PAGE_SIZE, 0);

		/* moved from ion_buffer_create( ) */
		sg_dma_address(sg) = sg_phys(sg);

		sg = sg_next(sg);
	}
#endif

	buffer->priv_virt = table;

#ifdef CONFIG_ODIN_ION_SMMU

	if ( unlikely(get_odin_debug_v_message() == true) ){
		for_each_sg( table->sgl, sg, table->nents, i ){
			struct page *page = sg_page(sg);
#ifdef CONFIG_ARM_LPAE
			pr_info("[II] alloc PA\t%llx\tSZ_2M\n", page_to_phys(page));
#else
			pr_info("[II] alloc PA\t%lx\tSZ_2M\n", (long unsigned int)page_to_phys(page));
#endif
		}
	}

	if ( likely(map_subsys != 0) ){
		odin_subsystem_map_buffer( map_subsys, SYSTEM_POOL, size, buffer, align );
		/* Although error occured, I have nothing to do.	*/
		/* User just can't get the iova.					*/
	}

#endif

	return 0;
err2:
#ifdef	CONFIG_ODIN_ION_SMMU_4K
	for_each_sg( table->sgl, sg, table->nents, i ){
		free_buffer_page(sys_heap, buffer, sg_page(sg), max_order);
		if ((--j) == 0)
			break;
	}
#endif
err1:
	kfree(table);
err:
#ifndef CONFIG_ODIN_ION_SMMU
	list_for_each_entry(info, &pages, list) {
		free_buffer_page(sys_heap, buffer, info->page, info->order);
		kfree(info);
	}
#else
#ifndef	CONFIG_ODIN_ION_SMMU_4K
	list_for_each_entry_safe(info, tmp_info, &pages, list) {
		free_buffer_page(sys_heap, buffer, info->page, info->order);
		list_del(&info->list);
		kfree(info);
	}
#endif
#endif
	return -ENOMEM;
}

void ion_system_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
#ifndef CONFIG_ODIN_ION_SMMU
	bool cached = ion_buffer_cached(buffer);
#endif
	struct scatterlist *sg;
	LIST_HEAD(pages);
	int i;

	/* uncached pages come from the page pools, zero them before returning
	   for security purposes (other allocations are zerod at alloc time */
#ifndef CONFIG_ODIN_ION_SMMU
	/* block for speed up				*/
	if (!cached)
		ion_heap_buffer_zero(buffer);
#endif

#ifdef CONFIG_ODIN_ION_SMMU

	if ( odin_subsystem_unmap_buffer( NULL,
		&(buffer->subsys_iova_list), SYSTEM_POOL ) < 0 ){
		/* Although error occured, I have nothing to do.	*/
		/* Should I reboot system?? 					*/
	}

	for_each_sg(table->sgl, sg, table->nents, i){

		if ( unlikely(get_odin_debug_v_message() == true) ){
#ifdef CONFIG_ARM_LPAE
			pr_info("[II] free  PA\t%llx\tSZ_2M\n",
											page_to_phys(sg_page(sg)));
#else
			pr_info("[II] free  PA\t%lx\tSZ_2M\n",
											(long unsigned int)page_to_phys(sg_page(sg)));
#endif
		}
		free_buffer_page(sys_heap, buffer, sg_page(sg),
				get_order(sg->length));

	}
#else
	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg),
				get_order(sg_dma_len(sg)));
#endif

	sg_free_table(table);
	kfree(table);
}

struct sg_table *ion_system_heap_map_dma(struct ion_heap *heap,
					 struct ion_buffer *buffer)
{
	return buffer->priv_virt;
}

void ion_system_heap_unmap_dma(struct ion_heap *heap,
			       struct ion_buffer *buffer)
{
	return;
}


#ifdef CONFIG_ODIN_ION_SMMU
static int ion_system_heap_map_iommu(struct ion_buffer *buffer,
			     							unsigned int subsys)
{
	int i_ret = 0;

	i_ret = odin_subsystem_map_buffer( subsys, SYSTEM_POOL, buffer->size, buffer, 0 );

	return i_ret;
}

static void * ion_system_heap_map_kernel_with_condition(
											struct ion_heap *heap,
											struct ion_buffer *buffer,
											unsigned long offset,
											unsigned long size)
{
	struct scatterlist *sg;
	int i, j;
	void *vaddr;
	pgprot_t pgprot;
	struct sg_table *table = buffer->sg_table;
	int npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *)*npages);
	struct page **tmp = pages;
	struct page **vmap_start = pages;
	unsigned long offset_page, offset_remainder;


	if ( buffer->vaddr_with_condition ){
		if (pages)
			vfree(pages);
		return (buffer->vaddr_with_condition);
	}

	if ( (offset+size) > buffer->size ){
		if (pages)
			vfree(pages);
		return 0;
	}

	if (!pages)
		return 0;

	if (buffer->flags & ION_FLAG_CACHED)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sg(table->sgl, sg, table->nents, i) {
		/* ifndef CONFIG_ODIN_ION_SMMU								*/
		/* int npages_this_entry = PAGE_ALIGN(sg_dma_len(sg)) / PAGE_SIZE;	*/
		int npages_this_entry = PAGE_ALIGN(sg->length) / PAGE_SIZE;

		struct page *page = sg_page(sg);
		BUG_ON(i >= npages);
		for (j = 0; j < npages_this_entry; j++) {
			*(tmp++) = page++;
		}
	}

	offset_page = offset / PAGE_SIZE;
	for (i=0; i<offset_page; i++)
		vmap_start++;
	offset_remainder = offset - (offset_page<<PAGE_SHIFT);
	vaddr = vmap(vmap_start,											\
					(PAGE_ALIGN(offset_remainder+size)>>PAGE_SHIFT),	\
					VM_MAP, pgprot);

	vfree(pages);

	/* ifdef CONFIG_ODIN_ION_SMMU */
	buffer->vaddr_with_condition = vaddr;

	return (vaddr+offset_remainder);

}

static void ion_system_heap_unmap_kernel_with_condition(
											struct ion_heap *heap,
											struct ion_buffer *buffer)
{
	if (buffer->vaddr_with_condition){
		vunmap(buffer->vaddr_with_condition);
		buffer->vaddr_with_condition = NULL;
	}
}
#endif

static struct ion_heap_ops system_heap_ops = {
	.allocate = ion_system_heap_allocate,
	.free = ion_system_heap_free,
	.map_dma = ion_system_heap_map_dma,
	.unmap_dma = ion_system_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
#ifdef CONFIG_ODIN_ION_SMMU
	.map_iommu = ion_system_heap_map_iommu,
	.map_kernel_with_condition = ion_system_heap_map_kernel_with_condition,
	.unmap_kernel_with_condition = ion_system_heap_unmap_kernel_with_condition,
#endif
};

static int ion_system_heap_shrink(struct shrinker *shrinker,
				  struct shrink_control *sc) {

	struct ion_heap *heap = container_of(shrinker, struct ion_heap,
					     shrinker);
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int nr_total = 0;
	int nr_freed = 0;
	int i;

	if (sc->nr_to_scan == 0)
		goto end;

	/* shrink the free list first, no point in zeroing the memory if
	   we're just going to reclaim it */
#ifndef CONFIG_ODIN_ION_SMMU
	nr_freed += ion_heap_freelist_drain(heap, sc->nr_to_scan * PAGE_SIZE) /
		PAGE_SIZE;
#else
	if ( (heap->flags&ION_HEAP_FLAG_DEFER_FREE) != 0 )
		nr_freed += ion_heap_freelist_drain(heap, sc->nr_to_scan * PAGE_SIZE) /
					PAGE_SIZE;
#endif

	if (nr_freed >= sc->nr_to_scan)
		goto end;

#ifndef CONFIG_ODIN_ION_SMMU
	for (i = 0; i < num_orders; i++) {
#else
#ifndef	CONFIG_ODIN_ION_SMMU_4K
	for (i = 0; i < num_orders; i++) {
#else
	for (i = ION_SZ_4K_IDX; i < num_orders; i++) {
#endif
#endif
		struct ion_page_pool *pool = sys_heap->pools[i];

		nr_freed += ion_page_pool_shrink(pool, sc->gfp_mask,
						 sc->nr_to_scan);
		if (nr_freed >= sc->nr_to_scan)
			break;
	}

end:
	/* total number of items is whatever the page pools are holding
	   plus whatever's in the freelist */
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];
		nr_total += ion_page_pool_shrink(pool, sc->gfp_mask, 0);
	}
#ifndef CONFIG_ODIN_ION_SMMU
	nr_total += ion_heap_freelist_size(heap) / PAGE_SIZE;
#else
	if ( (heap->flags&ION_HEAP_FLAG_DEFER_FREE) != 0 )
		nr_total += ion_heap_freelist_size(heap) / PAGE_SIZE;
#endif
	return nr_total;

}

static int ion_system_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{

	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];
		seq_printf(s, "%d order %u highmem pages in pool = %lu total\n",
			   pool->high_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in pool = %lu total\n",
			   pool->low_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->low_count);
	}
	return 0;
}

struct ion_heap *ion_system_heap_create(struct ion_platform_heap *unused)
{
	struct ion_system_heap *heap;
	int i;

	heap = kzalloc(sizeof(struct ion_system_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &system_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_SYSTEM;
#ifndef CONFIG_ODIN_ION_SMMU
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
#else
	heap->heap.flags = 0;
#endif
	heap->pools = kzalloc(sizeof(struct ion_page_pool *) * num_orders,
			      GFP_KERNEL);
	if (!heap->pools)
		goto err_alloc_pools;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i] > 4)
			gfp_flags = high_order_gfp_flags;
		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_pool;
		heap->pools[i] = pool;
	}

	heap->heap.shrinker.shrink = ion_system_heap_shrink;
	heap->heap.shrinker.seeks = DEFAULT_SEEKS;
	heap->heap.shrinker.batch = 0;
	register_shrinker(&heap->heap.shrinker);
	heap->heap.debug_show = ion_system_heap_debug_show;
	return &heap->heap;
err_create_pool:
	for (i = 0; i < num_orders; i++)
		if (heap->pools[i])
			ion_page_pool_destroy(heap->pools[i]);
	kfree(heap->pools);
err_alloc_pools:
	kfree(heap);
	return ERR_PTR(-ENOMEM);
}

void ion_system_heap_destroy(struct ion_heap *heap)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;

	for (i = 0; i < num_orders; i++)
		ion_page_pool_destroy(sys_heap->pools[i]);
	kfree(sys_heap->pools);
	kfree(sys_heap);
}

#ifdef CONFIG_ODIN_ION_SMMU
static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags,
					   unsigned int map_subsys)
#else
static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
#endif
{
	buffer->priv_virt = kzalloc(len, GFP_KERNEL);
	if (!buffer->priv_virt)
		return -ENOMEM;
	return 0;
}

void ion_system_contig_heap_free(struct ion_buffer *buffer)
{
	kfree(buffer->priv_virt);
}

static int ion_system_contig_heap_phys(struct ion_heap *heap,
				       struct ion_buffer *buffer,
				       unsigned long *addr, size_t *len)	/* ion_phys_addr_t *addr */
{
	*addr = virt_to_phys(buffer->priv_virt);
	*len = buffer->size;
	return 0;
}

struct sg_table *ion_system_contig_heap_map_dma(struct ion_heap *heap,
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
	sg_set_page(table->sgl, virt_to_page(buffer->priv_virt), buffer->size,
		    0);
	return table;
}

void ion_system_contig_heap_unmap_dma(struct ion_heap *heap,
				      struct ion_buffer *buffer)
{
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
}

int ion_system_contig_heap_map_user(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    struct vm_area_struct *vma)
{
	unsigned long pfn = __phys_to_pfn(virt_to_phys(buffer->priv_virt));
	return remap_pfn_range(vma, vma->vm_start, pfn + vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);

}

static struct ion_heap_ops kmalloc_ops = {
	.allocate = ion_system_contig_heap_allocate,
	.free = ion_system_contig_heap_free,
	.phys = ion_system_contig_heap_phys,
	.map_dma = ion_system_contig_heap_map_dma,
	.unmap_dma = ion_system_contig_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_system_contig_heap_map_user,
};

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &kmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM_CONTIG;
	return heap;
}

void ion_system_contig_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

