
#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/ion.h>
#include <linux/mtk_ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion_priv.h"
#include <linux/slab.h>
#include <mach/m4u.h>
#include <linux/ion_drv.h>
#include <linux/mutex.h>
#include <linux/mmprofile.h>
#include "ion_profile.h"

typedef struct {
	struct mutex lock;
	int eModuleID;
	unsigned int security;
	unsigned int coherent;
	void *pVA;
	unsigned int MVA;
	ion_mm_buf_debug_info_t dbg_info;
} ion_mm_buffer_info;


#define ION_FUNC_ENTER		/* MMProfileLogMetaString(MMP_ION_DEBUG, MMProfileFlagStart, __func__); */
#define ION_FUNC_LEAVE		/* MMProfileLogMetaString(MMP_ION_DEBUG, MMProfileFlagEnd, __func__); */

static unsigned int high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO |
					    __GFP_NOWARN | __GFP_NORETRY |
					    __GFP_NO_KSWAPD) & ~__GFP_WAIT;
static unsigned int low_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN);
static const unsigned int orders[] = { 8, 4, 2, 1, 0 };

/* static const unsigned int orders[] = {8, 4, 0}; */
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
	struct ion_page_pool **noncachable_pools;
	struct kmem_cache *info_cache;
	struct ion_page_pool **cachable_pools;
};

struct page_info {
	struct page *page;
	unsigned int order;
	struct list_head list;
};

/* static unsigned int high_mem = 0; */
static struct page *alloc_buffer_page(struct ion_system_heap *heap,
				      struct ion_buffer *buffer, unsigned long order)
{
	bool cached = ion_buffer_cached(buffer);
	bool split_pages = ion_buffer_fault_user_mappings(buffer);
	struct ion_page_pool *non_cached_pool = heap->noncachable_pools[order_to_index(order)];
	struct ion_page_pool *cachable_pool = heap->cachable_pools[order_to_index(order)];
	struct page *page;

/*	if (!cached) {
		page = ion_page_pool_alloc(pool);
	} else {
		gfp_t gfp_flags = low_order_gfp_flags;

		if (order > 0)
			gfp_flags = high_order_gfp_flags;
		page = alloc_pages(gfp_flags, order);
		if (!page)
			return 0;
		 __dma_page_cpu_to_dev(page, 0, PAGE_SIZE << order,
		 DMA_BIDIRECTIONAL);
		 we split_page because gpu driver uses vm_insert_page() to map pa to user va
		 but vm_insert_page() can only insert indivitual page
		 split_page(page, order);
	}*/

	page = ion_page_pool_alloc(cached ? cachable_pool : non_cached_pool);
	if (!page) {
		IONMSG("error: alloc_pages fail order=%d cache=%d\n", order, cached);
		return 0;
	}

	if (split_pages)
		split_page(page, order);
	return page;
}

static void free_buffer_page(struct ion_system_heap *heap,
			     struct ion_buffer *buffer, struct page *page, unsigned int order)
{
	bool cached = ion_buffer_cached(buffer);
/*	bool split_pages = ion_buffer_fault_user_mappings(buffer);
	int i;

	if (!cached) {
		struct ion_page_pool *pool = heap->pools[order_to_index(order)];
		ion_page_pool_free(pool, page);
	} else if (split_pages) {
		for (i = 0; i < (1 << order); i++)
			__free_page(page + i);
	} else {
		 we split_pages in allocate (see alloc_buffer_page)
		for (i = 0; i < (1 << order); i++)
			__free_page(page + i);
		 __free_pages(page, order);

	}*/

	struct ion_page_pool *noncachable_pool = heap->noncachable_pools[order_to_index(order)];
	struct ion_page_pool *cachable_pool = heap->cachable_pools[order_to_index(order)];
    ion_page_pool_free(cached ? cachable_pool : noncachable_pool, page);
}


static struct page_info *alloc_largest_available(struct ion_system_heap *heap,
						 struct ion_buffer *buffer,
						 unsigned long size, unsigned int max_order)
{
	struct page *page;
	struct page_info *info;
	int i;

	info = kmem_cache_alloc(heap->info_cache, GFP_KERNEL);
	if (!info)
		return NULL;


	for (i = 0; i < num_orders; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_buffer_page(heap, buffer, orders[i]);
		if (!page)
			continue;

		info->page = page;
		info->order = orders[i];
		return info;
	}

	kfree(info);

	return NULL;
}

static int ion_mm_heap_allocate(struct ion_heap *heap,
				struct ion_buffer *buffer,
				unsigned long size, unsigned long align, unsigned long flags)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table;
	struct scatterlist *sg;
	int ret;
	struct list_head pages;
	struct page_info *info, *tmp_info;
	int i = 0;
	long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];
	bool split_pages = ion_buffer_fault_user_mappings(buffer);
	ion_mm_buffer_info *pBufferInfo = NULL;

	INIT_LIST_HEAD(&pages);
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
		ret = sg_alloc_table(table, PAGE_ALIGN(size) / PAGE_SIZE, GFP_KERNEL);
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
			sg_set_page(sg, page, (1 << info->order) * PAGE_SIZE, 0);
			sg = sg_next(sg);
		}
		list_del(&info->list);
		kmem_cache_free(sys_heap->info_cache, info);
	}


	/* create MM buffer info for it */
	pBufferInfo = (ion_mm_buffer_info *) kzalloc(sizeof(ion_mm_buffer_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pBufferInfo)) {
		IONMSG("[ion_mm_heap_allocate]: Error. Allocate ion_buffer failed.\n");
		goto err1;
	}

	buffer->sg_table = table;
	pBufferInfo->pVA = 0;
	pBufferInfo->MVA = 0;
	pBufferInfo->eModuleID = -1;
	pBufferInfo->dbg_info.value1 = 0;
	pBufferInfo->dbg_info.value2 = 0;
	pBufferInfo->dbg_info.value3 = 0;
	pBufferInfo->dbg_info.value4 = 0;
	strncpy((pBufferInfo->dbg_info.dbg_name), "nothing", ION_MM_DBG_NAME_LEN);
	mutex_init(&(pBufferInfo->lock));

	buffer->priv_virt = pBufferInfo;


	return 0;
 err1:
	kfree(table);
	IONMSG("error: alloc for sg_table fail\n");
 err:
	list_for_each_entry_safe(info, tmp_info, &pages, list) {
		free_buffer_page(sys_heap, buffer, info->page, info->order);
		kmem_cache_free(sys_heap->info_cache, info);
	}
	IONMSG("error: mm_alloc fail: size=%d, flag=%d\n", size, flags);
	return -ENOMEM;
}


void ion_mm_heap_free_bufferInfo(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	ion_mm_buffer_info *pBufferInfo = (ion_mm_buffer_info *) buffer->priv_virt;
	buffer->priv_virt = NULL;

	if (pBufferInfo) {
		mutex_lock(&(pBufferInfo->lock));
		if ((pBufferInfo->eModuleID != -1) && (pBufferInfo->MVA)) {
			m4u_dealloc_mva_sg(pBufferInfo->eModuleID, table, buffer->size,
					   pBufferInfo->MVA);
		}
		mutex_unlock(&(pBufferInfo->lock));
		kfree(pBufferInfo);
	}
}


void ion_mm_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
/*	bool cached = ion_buffer_cached(buffer); */
	struct scatterlist *sg;
	LIST_HEAD(pages);
	int i;

	/* uncached pages come from the page pools, zero them before returning
	   for security purposes (other allocations are zerod at alloc time */
	//if (!cached)
		ion_heap_buffer_zero(buffer);

	ion_mm_heap_free_bufferInfo(buffer);

	for_each_sg(table->sgl, sg, table->nents, i)
	    free_buffer_page(sys_heap, buffer, sg_page(sg), get_order(sg_dma_len(sg)));
	sg_free_table(table);
	kfree(table);


}

struct sg_table *ion_mm_heap_map_dma(struct ion_heap *heap, struct ion_buffer *buffer)
{
	return buffer->sg_table;
}

void ion_mm_heap_unmap_dma(struct ion_heap *heap, struct ion_buffer *buffer)
{
	return;
}

static int ion_mm_heap_phys(struct ion_heap *heap,
			    struct ion_buffer *buffer, ion_phys_addr_t *addr, size_t *len)
{
	ion_mm_buffer_info *pBufferInfo = (ion_mm_buffer_info *) buffer->priv_virt;
	if (!pBufferInfo) {
		IONMSG("[ion_mm_heap_phys]: Error. Invalid buffer.\n");
		return -EFAULT;	/* Invalid buffer */
	}
	if (pBufferInfo->eModuleID == -1) {
		IONMSG("[ion_mm_heap_phys]: Error. Buffer not configured.\n");
		return -EFAULT;	/* Buffer not configured. */
	}
	/* Allocate MVA */

	mutex_lock(&(pBufferInfo->lock));
	if (pBufferInfo->MVA == 0) {
		int ret =
		    m4u_alloc_mva_sg(pBufferInfo->eModuleID, buffer->sg_table, buffer->size,
				     pBufferInfo->security, pBufferInfo->coherent,
				     &pBufferInfo->MVA);
		if (ret < 0) {
			mutex_unlock(&(pBufferInfo->lock));
			pBufferInfo->MVA = 0;
			IONMSG("[ion_mm_heap_phys]: Error. Allocate MVA failed.\n");
			return -EFAULT;
		}
	}
	*addr = (ion_phys_addr_t) pBufferInfo->MVA;	/* MVA address */
	mutex_unlock(&(pBufferInfo->lock));
	*len = buffer->size;

	return 0;
}


void ion_mm_heap_add_freelist(struct ion_buffer *buffer)
{
	ion_mm_heap_free_bufferInfo(buffer);
}


static struct ion_heap_ops system_heap_ops = {
	.allocate = ion_mm_heap_allocate,
	.free = ion_mm_heap_free,
	.map_dma = ion_mm_heap_map_dma,
	.unmap_dma = ion_mm_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
	.phys = ion_mm_heap_phys,
	.add_freelist = ion_mm_heap_add_freelist,
};

static int ion_system_heap_shrink(struct shrinker *shrinker, struct shrink_control *sc)
{

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
	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE) {
		nr_freed += ion_heap_freelist_drain(heap, sc->nr_to_scan * PAGE_SIZE) / PAGE_SIZE;
		if (nr_freed >= sc->nr_to_scan)
			goto end;
	}

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->noncachable_pools[i];

		nr_freed += ion_page_pool_shrink(pool, sc->gfp_mask, sc->nr_to_scan);
		if (nr_freed >= sc->nr_to_scan)
			break;
	}

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->cachable_pools[i];

		nr_freed += ion_page_pool_shrink(pool, sc->gfp_mask, sc->nr_to_scan);
		if (nr_freed >= sc->nr_to_scan)
			break;
	}


 end:
	/* total number of items is whatever the page pools are holding
	   plus whatever's in the freelist */
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->noncachable_pools[i];
		nr_total += ion_page_pool_shrink(pool, sc->gfp_mask, 0);
	}

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->cachable_pools[i];
		nr_total += ion_page_pool_shrink(pool, sc->gfp_mask, 0);
	}

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE) {
		nr_total += ion_heap_freelist_size(heap) / PAGE_SIZE;
	}
	return nr_total;

}

static int ion_mm_heap_debug_show(struct ion_heap *heap, struct seq_file *s, void *unused)
{

	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
#if 0
	struct ion_device *dev = heap->dev;
	struct rb_node *n;
#endif
	int i;

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->noncachable_pools[i];
		seq_printf(s, "%d order %u highmem pages in noncachable pool = %lu total\n",
			   pool->high_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in noncachable pool = %lu total\n",
			   pool->low_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->low_count);

		pool = sys_heap->cachable_pools[i];
		seq_printf(s, "%d order %u highmem pages in cachable pool = %lu total\n",
			   pool->high_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in cachable pool = %lu total\n",
			   pool->low_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->low_count);
	}
	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE) {
		seq_printf(s, "mm_heap_freelist total_size=0x%x\n", ion_heap_freelist_size(heap));
	} else {
		seq_puts(s, "mm_heap defer free disabled\n");
	}
#if 0
	{
		ion_mm_buffer_info *pBufInfo;
		ion_mm_buf_debug_info_t *pDbg;

		seq_puts(s, "----------------------------------------------------\n");

		seq_printf(s, "%8.s %8.s %4.s %3.s %3.s %3.s %8.s %3.s %3.s %3.s %8.s "
			   "%4.s %4.s %4.s %4.s %s\n",
			   "buffer", "size", "kmap", "ref", "hdl", "mod", "mva", "sec", "coh",
			   "pid", "comm", "v1", "v2", "v3", "v4", "dbg_name");


		mutex_lock(&dev->buffer_lock);
		for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
			struct ion_buffer *buffer = rb_entry(n, struct ion_buffer,
							     node);
			if (buffer->heap->type != heap->type)
				continue;
			pBufInfo = (ion_mm_buffer_info *) buffer->priv_virt;
			pDbg = &(pBufInfo->dbg_info);

			seq_printf(s, "0x%x %8u %3d %3d %3d %3d %8x %3d %3d %3d %s "
				   "0x%x 0x%x 0x%x 0x%x %s\n", (unsigned int)buffer,
				   buffer->size, buffer->kmap_cnt,
				   atomic_read(&buffer->ref.refcount),
				   buffer->handle_count,
				   pBufInfo->eModuleID, pBufInfo->MVA,
				   pBufInfo->security, pBufInfo->coherent,
				   buffer->pid, buffer->task_comm,
				   pDbg->value1, pDbg->value2, pDbg->value3, pDbg->value4,
				   pDbg->dbg_name);
		}
		mutex_unlock(&dev->buffer_lock);

		seq_puts(s, "----------------------------------------------------\n");

	}

#if ION_RUNTIME_DEBUGGER
	/* dump all handle's backtrace */
	for (n = rb_first(&dev->clients); n; n = rb_next(n)) {
		struct ion_client *client = rb_entry(n, struct ion_client, node);

		if (client->task) {
			char task_comm[TASK_COMM_LEN];
			get_task_comm(task_comm, client->task);
			seq_printf(s, "%16.s %16u(pid)================>\n", task_comm, client->pid);
		} else {
			seq_printf(s, "%16.s %16u(pid)================>\n", client->name,
				   client->pid);
		}

		{
			struct rb_node *m;

			mutex_lock(&client->lock);
			for (m = rb_first(&client->handles); m; m = rb_next(m)) {
				struct ion_handle *handle = rb_entry(m,
								     struct ion_handle,
								     node);
				int i;

				seq_printf(s, "handle=0x%x, buffer=0x%x, heap=%d, backtrace is:\n",
					   (unsigned int)handle, (unsigned int)handle->buffer,
					   handle->buffer->heap->id);

				for (i = 0; i < handle->dbg.backtrace_num; i++) {
					seq_printf(s, "\t\tbt: 0x%x\n", handle->dbg.backtrace[i]);
				}

			}
			mutex_unlock(&client->lock);
		}

	}
#endif

#endif

	return 0;
}

struct ion_heap *ion_mm_heap_create(struct ion_platform_heap *unused)
{
	struct ion_system_heap *heap;
	int i;

	heap = kzalloc(sizeof(struct ion_system_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &system_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_MULTIMEDIA;
	heap->heap.flags = 0;
	heap->noncachable_pools = kzalloc(sizeof(struct ion_page_pool *) * num_orders, GFP_KERNEL);
	if (!heap->noncachable_pools)
		goto err_alloc_pools;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i] > 0)
			gfp_flags = high_order_gfp_flags;
		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_noncachable_pool;
		heap->noncachable_pools[i] = pool;
	}

	heap->cachable_pools = kzalloc(sizeof(struct ion_page_pool *) * num_orders, GFP_KERNEL);
	if (!heap->cachable_pools)
		goto err_alloc_pools;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i] > 0)
			gfp_flags = high_order_gfp_flags;
		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_cachable_pool;
		heap->cachable_pools[i] = pool;
	}
	heap->heap.shrinker.shrink = ion_system_heap_shrink;
	heap->heap.shrinker.seeks = DEFAULT_SEEKS;
	heap->heap.shrinker.batch = 1024;
	register_shrinker(&heap->heap.shrinker);
	heap->heap.debug_show = ion_mm_heap_debug_show;
	heap->info_cache = kmem_cache_create("ion_page_infos",
						sizeof(struct page_info),
						0, SLAB_HWCACHE_ALIGN, NULL);
	if (!heap->info_cache) {
		IONMSG("[ion_mm_heap]: error creating info_cache\n");
            goto err_create_noncachable_pool;
        }
	return &heap->heap;
err_create_noncachable_pool:
	IONMSG("[ion_mm_heap]: error to create noncachable pool\n");
	for (i = 0; i < num_orders; i++)
		if (heap->noncachable_pools[i])
			ion_page_pool_destroy(heap->noncachable_pools[i]);
	kfree(heap->noncachable_pools);
err_create_cachable_pool:
		IONMSG("[ion_mm_heap]: error to create cachable pool\n");
		for (i = 0; i < num_orders; i++)
			if (heap->cachable_pools[i])
				ion_page_pool_destroy(heap->cachable_pools[i]);
		kfree(heap->cachable_pools);
 err_alloc_pools:
	IONMSG("[ion_mm_heap]: error to allocate pool\n");
	kfree(heap);
	return ERR_PTR(-ENOMEM);
}

void ion_mm_heap_destroy(struct ion_heap *heap)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;

	for (i = 0; i < num_orders; i++) {
		ion_page_pool_destroy(sys_heap->noncachable_pools[i]);
	}

	for(i = 0; i< num_orders; i++) {
		ion_page_pool_destroy(sys_heap->cachable_pools[i]);
	}

	kfree(sys_heap->noncachable_pools);
	kfree(sys_heap->cachable_pools);
	kfree(sys_heap);
}

int ion_mm_copy_dbg_info(ion_mm_buf_debug_info_t *src, ion_mm_buf_debug_info_t *dst)
{
	int i;
	dst->handle = src->handle;
	for (i = 0; i < ION_MM_DBG_NAME_LEN; i++) {
		dst->dbg_name[i] = src->dbg_name[i];
	}
	dst->dbg_name[ION_MM_DBG_NAME_LEN - 1] = '\0';
	dst->value1 = src->value1;
	dst->value2 = src->value2;
	dst->value3 = src->value3;
	dst->value4 = src->value4;

	return 0;

}



long ion_mm_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg, int from_kernel)
{
	ion_mm_data_t Param;
	long ret = 0;
	/* char dbgstr[256]; */
	unsigned long ret_copy;
	ION_FUNC_ENTER;
	if (from_kernel)
		Param = *(ion_mm_data_t *) arg;
	else
		ret_copy = copy_from_user(&Param, (void __user *)arg, sizeof(ion_mm_data_t));

	switch (Param.mm_cmd) {
	case ION_MM_CONFIG_BUFFER:
		if (Param.config_buffer_param.handle) {
			struct ion_buffer *buffer;
			struct ion_handle *kernel_handle;
			kernel_handle = ion_drv_get_kernel_handle(client,
								  Param.config_buffer_param.handle,
								  from_kernel);
			if (IS_ERR(kernel_handle)) {
				IONMSG("ion config buffer fail! port=%d\n",
				       Param.config_buffer_param.eModuleID);
				ret = -EINVAL;
				break;
			}

			buffer = ion_handle_buffer(kernel_handle);
			if ((int)buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA) {
				ion_mm_buffer_info *pBufferInfo = buffer->priv_virt;
				mutex_lock(&(pBufferInfo->lock));
				if (pBufferInfo->MVA == 0) {
					pBufferInfo->eModuleID =
					    Param.config_buffer_param.eModuleID;
					pBufferInfo->security = Param.config_buffer_param.security;
					pBufferInfo->coherent = Param.config_buffer_param.coherent;
				} else {
					if (pBufferInfo->security !=
					    Param.config_buffer_param.security
					    || pBufferInfo->coherent !=
					    Param.config_buffer_param.coherent) {
						IONMSG
						    ("[ion_mm_heap]: Warning. config buffer param error:.\n");
						IONMSG("sec:%d(%d), coherent: %d(%d)\n",
						       pBufferInfo->security,
						       Param.config_buffer_param.security,
						       pBufferInfo->coherent,
						       Param.config_buffer_param.coherent);
						ret = -ION_ERROR_CONFIG_LOCKED;
					}
				}
				mutex_unlock(&(pBufferInfo->lock));
			} else {
				IONMSG
				    ("[ion_mm_heap]: Error. Cannot configure buffer that is not from multimedia heap.\n");
				ret = 0;
			}
		} else {
			IONMSG("[ion_mm_heap]: Error config buf with invalid handle.\n");
			ret = -EFAULT;
		}
		break;

	case ION_MM_SET_DEBUG_INFO:
		{
			struct ion_buffer *buffer;
			if (Param.buf_debug_info_param.handle) {
				struct ion_handle *kernel_handle;
				kernel_handle = ion_drv_get_kernel_handle(client,
									  Param.
									  buf_debug_info_param.
									  handle, from_kernel);
				if (IS_ERR(kernel_handle)) {
					IONMSG("ion config buffer fail! port=%d\n",
					       Param.config_buffer_param.eModuleID);
					ret = -EINVAL;
					break;
				}

				buffer = ion_handle_buffer(kernel_handle);
				if ((int)buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA) {
					ion_mm_buffer_info *pBufferInfo = buffer->priv_virt;
					mutex_lock(&(pBufferInfo->lock));
					ion_mm_copy_dbg_info(&(Param.buf_debug_info_param),
							     &(pBufferInfo->dbg_info));
					mutex_unlock(&(pBufferInfo->lock));
				} else {
					IONMSG
					    ("[ion_mm_heap]: Error. Cannot set dbg buffer that is not from multimedia heap.\n");
					ret = -EFAULT;
				}
			} else {
				IONMSG
				    ("[ion_mm_heap]: Error. set dbg buffer with invalid handle.\n");
				ret = -EFAULT;
			}
		}
		break;

	case ION_MM_GET_DEBUG_INFO:
		{
			struct ion_buffer *buffer;
			if (Param.buf_debug_info_param.handle) {
				struct ion_handle *kernel_handle;
				kernel_handle = ion_drv_get_kernel_handle(client,
									  Param.
									  buf_debug_info_param.
									  handle, from_kernel);
				if (IS_ERR(kernel_handle)) {
					IONMSG("ion config buffer fail! port=%d\n",
					       Param.config_buffer_param.eModuleID);
					ret = -EINVAL;
					break;
				}
				buffer = ion_handle_buffer(kernel_handle);
				if ((int)buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA) {
					ion_mm_buffer_info *pBufferInfo = buffer->priv_virt;
					mutex_lock(&(pBufferInfo->lock));
					ion_mm_copy_dbg_info(&(pBufferInfo->dbg_info),
							     &(Param.buf_debug_info_param));
					mutex_unlock(&(pBufferInfo->lock));
				} else {
					IONMSG
					    ("[ion_mm_heap]: Error. Cannot set dbg buffer that is not from multimedia heap.\n");
					ret = -EFAULT;
				}
			} else {
				IONMSG
				    ("[ion_mm_heap]: Error. set dbg buffer with invalid handle.\n");
				ret = -EFAULT;
			}
		}
		break;

	default:
		IONMSG("[ion_mm_heap]: Error. Invalid command.\n");
		ret = -EFAULT;
	}
	if (from_kernel)
		*(ion_mm_data_t *) arg = Param;
	else
		ret_copy = copy_to_user((void __user *)arg, &Param, sizeof(ion_mm_data_t));
	ION_FUNC_LEAVE;
	return ret;
}
