/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * Younghyun Jo <younghyun.jo@lge.com>
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

#include <asm/bug.h>
#include <asm/io.h>

#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/seq_file.h>
#include <linux/sizes.h>
#include <linux/vmalloc.h>

#include <sound/ama.h>

#include "ama_debugfs.h"

#define AMA_ALIGN SZ_4K

struct ama_pool
{
	unsigned long vaddr;
	unsigned long paddr;
	size_t size;

	struct list_head list;
};

struct ama_desc
{
	unsigned long paddr;
	unsigned long vaddr;
	size_t size;

	struct resource *resource;

	struct mutex lock;

	struct list_head free_pool;
	struct list_head alloc_pool;
};
struct ama_desc *ama_desc = NULL;

static int _ama_status_show(struct seq_file *seq, void *arg)
{
	struct ama_pool *pool = NULL, *tmp = NULL;
	size_t allocated_size = 0;
	size_t free_size = 0;

	mutex_lock(&ama_desc->lock);

	seq_printf(seq, "====AMA STATUS====\n");
	seq_printf(seq, "\tbase:0x%08X(paddr0x%08X) size:%d 0x%08X\n",
			(unsigned int)ama_desc->vaddr, (unsigned int)ama_desc->paddr,
			ama_desc->size, ama_desc->size);
	seq_printf(seq, "\n");
	seq_printf(seq, "\t---- ALLOCATED POOL ----\n");
	list_for_each_entry_safe(pool, tmp, &ama_desc->alloc_pool, list) {
		seq_printf(seq, "\t\t 0x%08X++0x%08X\n",
				(unsigned int)pool->vaddr, pool->size);
		allocated_size += pool->size;
	}
	seq_printf(seq, "\n");

	seq_printf(seq, "\t---- FREE POOL ----\n");
	list_for_each_entry_safe(pool, tmp, &ama_desc->free_pool, list) {
		seq_printf(seq, "\t\t 0x%08X++0x%08X\n",
				(unsigned int)pool->vaddr, pool->size);
		free_size += pool->size;
	}
	seq_printf(seq, "\n");
	seq_printf(seq, "\t---- free:%d 0x%08X alloc:%d 0x%08X ----\n",
			free_size, free_size, allocated_size, allocated_size);

	mutex_unlock(&ama_desc->lock);

	return 0;
}

static void* _ama_alloc_pool_get(void *addr)
{
	struct ama_pool *pool = NULL, *tmp = NULL;

	list_for_each_entry_safe(pool, tmp, &ama_desc->alloc_pool, list) {
		if (pool->vaddr == (unsigned long)addr) {
			return pool;
		}
	}

	return NULL;
}

static void _ama_free_pool_add(struct ama_pool *free)
{
	struct ama_pool *pool = NULL, *tmp = NULL;
	list_for_each_entry_safe(pool, tmp, &ama_desc->free_pool, list) {
		if (free->vaddr <  pool->vaddr)
			break;
	}

	list_add_tail(&free->list, &pool->list);
}

static void _ama_defragmentation(struct ama_pool *fragmented)
{
	struct ama_pool *prev = NULL, *next = NULL;

	if (fragmented->list.prev != &ama_desc->free_pool) {
		prev = list_entry(fragmented->list.prev, struct ama_pool, list);
		if (prev->vaddr + prev->size == fragmented->vaddr) {
			prev->size += fragmented->size;
			list_del(&fragmented->list);
			vfree(fragmented);
			fragmented = prev;
		}
	}

	if (!list_is_last(&fragmented->list, &ama_desc->free_pool)) {
		next = list_entry(fragmented->list.next, struct ama_pool, list);
		if (fragmented->vaddr + fragmented->size == next->vaddr) {
			next->size += fragmented->size;
			next->vaddr -= fragmented->size;
			list_del(&fragmented->list);
			vfree(fragmented);
		}
	}
}

void* ama_virt_to_phys(void *addr)
{
	struct ama_pool *pool = NULL;

	mutex_lock(&ama_desc->lock);

	pool = _ama_alloc_pool_get(addr);
	if (pool == NULL) {
		pr_err("[AMA] _ama_alloc_pool_get failed addr:0x%08X\n",
				(unsigned int)addr);
		mutex_unlock(&ama_desc->lock);
		return NULL;
	}

	mutex_unlock(&ama_desc->lock);

	return pool->paddr;
}

void* ama_alloc(size_t size)
{
	struct ama_pool *pool = NULL, *tmp = NULL, *new = NULL;
	size_t aligned_size = ALIGN(size, AMA_ALIGN);
	size_t alloc_size = 0;
	unsigned long addr = 0;

	if (ama_desc == NULL) {
		pr_err("[AMA] ama is not initialized\n");
		return NULL;
	}

	mutex_lock(&ama_desc->lock);

	list_for_each_entry_safe(pool, tmp, &ama_desc->free_pool, list) {
		if (aligned_size <= pool->size) {
			new = vzalloc(sizeof(struct ama_pool));
			if (new == NULL) {
				pr_err("[AMA] vzalloc failed. size:%d\n",
						sizeof(struct ama_pool));
				break;
			}

			new->vaddr = pool->vaddr;
			new->paddr = pool->paddr;
			new->size = aligned_size;
			list_add(&new->list, &ama_desc->alloc_pool);

			addr = new->vaddr;

			pool->vaddr += aligned_size;
			pool->paddr += aligned_size;
			pool->size -= aligned_size;

			if (pool->size == 0) {
				list_del(&pool->list);
				vfree(pool);
			}
			break;
		}
		else
			alloc_size += pool->size;
	}

	if (addr == 0)
		pr_err("[AMA] ama_alloc failed. allocated:%d, req%d\n", alloc_size, size);

	mutex_unlock(&ama_desc->lock);

	return (void*)addr;
}

void ama_free(void *addr)
{
	struct ama_pool *pool = NULL;

	if (ama_desc == NULL) {
		pr_err("[AMA] ama is not initialized\n");
		return;
	}

	if(addr == NULL || (unsigned long)addr < ama_desc->vaddr) {
		pr_err("[AMA] Invalid addr 0x%08X\n", (unsigned int)addr);
		return;
	}

	mutex_lock(&ama_desc->lock);

	pool = _ama_alloc_pool_get(addr);
	if (pool == NULL) {
		pr_err("[AMA] _ama_alloc_pool_get failed. addr:0x%08X\n", (unsigned int)addr);
		mutex_unlock(&ama_desc->lock);
		return;
	}

	list_del(&pool->list);

	_ama_free_pool_add(pool);

	_ama_defragmentation(pool);

	mutex_unlock(&ama_desc->lock);
}

bool ama_init(unsigned long base, size_t size)
{
	struct ama_pool *pool;
	if (ama_desc) {
		pr_warn("[AMA] ama is initialized\n");
		return true;
	}

	ama_desc = vzalloc(sizeof(struct ama_desc));
	if (ama_desc == NULL) {
		pr_err("[AMA] vzalloc failed. size:%d\n", sizeof(struct ama_desc));
		goto err_vzalloc;
	}

	ama_desc->paddr = base;
	ama_desc->size = size;

	ama_desc->resource = request_mem_region(ama_desc->paddr, ama_desc->size, "ama");
	if (ama_desc->resource == NULL) {
		pr_err("[AMA] request_mem_region failed. 0x%08X++0x%08X\n",
				(unsigned int)ama_desc->paddr, ama_desc->size);
		goto err_request_mem_region;
	}

	ama_desc->vaddr = (unsigned long)ioremap_nocache(ama_desc->paddr, ama_desc->size);
	if (ama_desc->vaddr == 0) {
		pr_err("[AMA] ioremap_nocache failed. 0x%08X++0x%08X\n",
				(unsigned int)ama_desc->paddr, ama_desc->size);
		goto err_ioremap_nocache;
	}

	INIT_LIST_HEAD(&ama_desc->free_pool);
	INIT_LIST_HEAD(&ama_desc->alloc_pool);

	mutex_init(&ama_desc->lock);

	pool = vzalloc(sizeof(struct ama_pool));
	if (pool == NULL) {
		pr_err("[AMA] vzalloc pool failed size:%d\n", sizeof(struct ama_pool));
		goto err_vzalloc_apool;
	}

	pool->vaddr = ama_desc->vaddr;
	pool->paddr = ama_desc->paddr;
	pool->size = ama_desc->size;

	list_add(&pool->list, &ama_desc->free_pool);

	ama_debugfs_init(_ama_status_show, NULL);

	return true;

err_vzalloc_apool:
	mutex_destroy(&ama_desc->lock);
	iounmap((void*)ama_desc->vaddr);

err_ioremap_nocache:
	release_mem_region(ama_desc->paddr, ama_desc->size);

err_request_mem_region:
	if (ama_desc)
		vfree(ama_desc);
err_vzalloc:
	ama_desc = NULL;

	return false;
}

bool ama_cleanup(void)
{
	struct ama_pool *pool = NULL;

	if (ama_desc == NULL) {
		pr_warn("[AMA] ama is not initialized\n");
		return true;
	}

	mutex_lock(&ama_desc->lock);

	if (!list_empty(&ama_desc->alloc_pool) ||
			list_is_singular(&ama_desc->alloc_pool)) {
		pr_err("[AMA] allocated memory pool exist.\n");
		mutex_unlock(&ama_desc->lock);
		return false;
	}

	pool = list_first_entry(&ama_desc->free_pool, struct ama_pool, list);
	list_del(&pool->list);
	vfree(pool);

	ama_debugfs_cleanup();

	iounmap((void*)ama_desc->vaddr);

	release_mem_region(ama_desc->paddr, ama_desc->size);

	mutex_unlock(&ama_desc->lock);
	mutex_destroy(&ama_desc->lock);

	vfree(ama_desc);
	ama_desc = NULL;

	return true;
}
