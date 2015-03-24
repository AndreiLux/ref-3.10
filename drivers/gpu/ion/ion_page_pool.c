/*
 * drivers/gpu/ion/ion_mem_pool.c
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

#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "ion_priv.h"

static struct kmem_cache *item_cache;
static DEFINE_MUTEX(item_mutex);
static struct list_head items = LIST_HEAD_INIT(items);
#define MAX_ITEM_COUNT ((2 * PAGE_SIZE) / sizeof(struct ion_page_pool_item) - 1)

struct ion_page_pool_item {
        struct page *page;
        struct list_head list;
};

static void *ion_page_pool_alloc_pages(struct ion_page_pool *pool)
{
        struct page *page = alloc_pages(pool->gfp_mask, pool->order);

        if (!page)
                return NULL;
        /* this is only being used to flush the page for dma,
           this api is not really suitable for calling from a driver
           but no better way to flush a page for dma exist at this time */
        arm_dma_ops.sync_single_for_device(NULL, pfn_to_dma(NULL, page_to_pfn(page)), PAGE_SIZE << pool->order, DMA_BIDIRECTIONAL);
        //__dma_page_cpu_to_dev(page, 0, PAGE_SIZE << pool->order,
        //                      DMA_BIDIRECTIONAL);
        //we split_page because gpu driver uses vm_insert_page() to map pa to user va
        //but vm_insert_page() can only insert indivitual page
        //split_page(page, pool->order);
        return page;
}

static void ion_page_pool_free_pages(struct ion_page_pool *pool,
                                     struct page *page)
{
        //int i;
        __free_pages(page, pool->order);
        //we split_pages in allocate (see ion_page_pool_alloc_pages)
        //for (i = 0; i < (1 << pool->order); i++)
        //    __free_page(page + i);
}

static int ion_page_pool_add(struct ion_page_pool *pool, struct page *page)
{
        struct ion_page_pool_item *item;

	item = kmem_cache_alloc(item_cache, GFP_KERNEL);
        if (!item)
                return -ENOMEM;

/* ACOS_MOD_BEGIN {internal_membo} */
#ifdef CONFIG_TRAPZ_PVA
	if (page) {
		page->detail.allocation_pid = 0;
		page->detail.last_mapper_pid = 0;
	}
#endif
/* ACOS_MOD_END {internal_membo} */

        mutex_lock(&pool->mutex);
        item->page = page;
        if (PageHighMem(page)) {
                list_add_tail(&item->list, &pool->high_items);
                pool->high_count++;
        } else {
                list_add_tail(&item->list, &pool->low_items);
                pool->low_count++;
        }
        mutex_unlock(&pool->mutex);
        return 0;
}

static struct page *ion_page_pool_remove(struct ion_page_pool *pool, bool high)
{
        struct ion_page_pool_item *item;
        struct page *page;

        if (high) {
                BUG_ON(!pool->high_count);
                item = list_first_entry(&pool->high_items,
                                        struct ion_page_pool_item, list);
                pool->high_count--;
        } else {
                BUG_ON(!pool->low_count);
                item = list_first_entry(&pool->low_items,
                                        struct ion_page_pool_item, list);
                pool->low_count--;
        }

        list_del(&item->list);
        page = item->page;
	kmem_cache_free(item_cache, item);
        return page;
}

void *ion_page_pool_alloc(struct ion_page_pool *pool)
{
        struct page *page = NULL;

        BUG_ON(!pool);

        mutex_lock(&pool->mutex);
        if (pool->high_count)
                page = ion_page_pool_remove(pool, true);
        else if (pool->low_count)
                page = ion_page_pool_remove(pool, false);
        mutex_unlock(&pool->mutex);

/* ACOS_MOD_BEGIN {internal_membo} */
#ifdef CONFIG_TRAPZ_PVA
	if (page) {
		struct task_struct *cur_task;
		cur_task = current;
		if (cur_task)
			page->detail.allocation_pid = cur_task->tgid;
	}
#endif
/* ACOS_MOD_END {internal_membo} */
	if (!page)
		page = ion_page_pool_alloc_pages(pool);

        return page;
}

void ion_page_pool_free(struct ion_page_pool *pool, struct page* page)
{
        int ret;

	IONMSG("ion_page_pool_free pool = 0x%x, page = 0x%x.\n", pool, page);
        ret = ion_page_pool_add(pool, page);
        if (ret)
                ion_page_pool_free_pages(pool, page);
}

static int ion_page_pool_total(struct ion_page_pool *pool, bool high)
{
        int total = 0;

        total += high ? (pool->high_count + pool->low_count) *
                (1 << pool->order) :
                        pool->low_count * (1 << pool->order);
        return total;
}

int ion_page_pool_shrink(struct ion_page_pool *pool, gfp_t gfp_mask,
                                int nr_to_scan)
{
        int nr_freed = 0;
        int i;
        bool high;

        high = gfp_mask & __GFP_HIGHMEM;

        if (nr_to_scan == 0)
                return ion_page_pool_total(pool, high);

        for (i = 0; i < nr_to_scan; i++) {
                struct page *page;

                mutex_lock(&pool->mutex);
                if (high && pool->high_count) {
                        page = ion_page_pool_remove(pool, true);
                } else if (pool->low_count) {
                        page = ion_page_pool_remove(pool, false);
                } else {
                        mutex_unlock(&pool->mutex);
                        break;
                }
                mutex_unlock(&pool->mutex);
                ion_page_pool_free_pages(pool, page);
                nr_freed += (1 << pool->order);
        }

        return nr_freed;
}

struct ion_page_pool *ion_page_pool_create(gfp_t gfp_mask, unsigned int order)
{
        struct ion_page_pool *pool = kmalloc(sizeof(struct ion_page_pool),
                                             GFP_KERNEL);
        if (!pool)
                return NULL;
        pool->high_count = 0;
        pool->low_count = 0;
        INIT_LIST_HEAD(&pool->low_items);
        INIT_LIST_HEAD(&pool->high_items);
        pool->gfp_mask = gfp_mask;
        pool->order = order;
        mutex_init(&pool->mutex);
        plist_node_init(&pool->list, order);

        return pool;
}

void ion_page_pool_destroy(struct ion_page_pool *pool)
{
        kfree(pool);
}

static int __init ion_page_pool_init(void)
{
	item_cache = kmem_cache_create("ion_page_pool_items",
					sizeof(struct ion_page_pool_item),
					0, SLAB_HWCACHE_ALIGN, NULL);
	if (!item_cache)
		return -ENOMEM;
        return 0;
}

static void __exit ion_page_pool_exit(void)
{
	kmem_cache_destroy(item_cache);
}

module_init(ion_page_pool_init);
module_exit(ion_page_pool_exit);
