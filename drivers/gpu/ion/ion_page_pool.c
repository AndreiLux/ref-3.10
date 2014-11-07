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
#ifdef CONFIG_ODIN_ION_SMMU
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/rcupdate.h>
#include <linux/notifier.h>
#include <linux/odin_iommu.h>
#include <linux/platform_data/odin_drm.h>

#include "ion_priv.h"
#endif


struct ion_page_pool_item {
#ifdef CONFIG_ODIN_ION_SMMU
	int drm_label;
#endif
	struct page *page;
	struct list_head list;
};


#ifdef CONFIG_ODIN_ION_SMMU
struct odin_pa_ll {
	phys_addr_t pa;
	struct odin_pa_ll *prev;
	struct odin_pa_ll *next;
};

int odin_ion_get_page_cnt( size_t align )
{

	struct ion_page_pool_item *head, *item;
	struct ion_page_pool *pool = odin_ion_get_sys_heap_pool(align);
	int page_cnt=0;


	list_for_each_entry_safe( head, item, &pool->high_items, list ){
		page_cnt++;
	}

	list_for_each_entry_safe( head, item, &pool->low_items, list ){
		page_cnt++;
	}

	return page_cnt;

}

/* #define	PRINT_ODIN_PROTECT_BUFFER */

void odin_ion_prepare_pool_protection(struct ion_page_pool *pool)
{

#ifndef CONFIG_ARM_LPAE

	struct ion_page_pool_item *head, *item;
	struct odin_pa_ll *start, *end, *temp, *member;
	struct odin_pa_sz_ll *merge, *merge_temp;
	phys_addr_t pa_temp;
	unsigned long sz_temp;
	int should_continue=1;


	start = kzalloc( sizeof(struct odin_pa_ll), GFP_KERNEL );
	end = kzalloc( sizeof(struct odin_pa_ll), GFP_KERNEL );
	start->pa = 0;
	start->prev = start;
	start->next = end;
	end->pa = ODIN_MAX_PA_VALUE;
	end->prev = start;
	end->next = end;

	/* line-up DRM reserved pool : keep [ start ], [ end ] */

	list_for_each_entry_safe( head, item, &pool->high_items, list ){

		member = kzalloc( sizeof(struct odin_pa_ll), GFP_KERNEL );
		member->pa = (phys_addr_t)page_to_phys(head->page);

		temp = start;
		while ( temp->pa < member->pa ){
			temp = temp->next;
		}
		temp->prev->next = member;
		member->next = temp;
		member->prev = temp->prev;
		temp->prev = member;

	}

	/* merge the lined-up entries : keep [ merge ] */

	member = start->next;
	merge = kzalloc( sizeof(struct odin_pa_sz_ll), GFP_KERNEL );
	merge->pa = member->pa;
	merge->sz = SZ_2M;
	merge->next = merge;
	merge->prev = merge;
	while ( member->next != end ){

		if ( (merge->pa + merge->sz) == member->next->pa ){
			merge->sz += SZ_2M;
		}else{
			merge_temp = kzalloc( sizeof(struct odin_pa_sz_ll), GFP_KERNEL );
			merge_temp->pa = member->next->pa;
			merge_temp->sz = SZ_2M;
			merge->next = merge_temp;
			merge_temp->prev = merge;
			merge_temp->next = merge_temp;
			merge = merge_temp;
		}

		member = member->next;

	}

	/* line-up by merged chunk size : keep [ merge ] */

	if (merge->prev != merge->next){	/* except the case 1-big chunk */

		while (merge->prev != merge)
			merge = merge->prev;

		merge_temp = merge;
		while (1){

			if (should_continue==0)
				break;

			merge = merge_temp;
			while (1){

				if (merge->sz > merge->prev->sz){

					pa_temp = merge->pa;
					sz_temp = merge->sz;
					merge->pa = merge->prev->pa;
					merge->sz = merge->prev->sz;
					merge->prev->pa = pa_temp;
					merge->prev->sz = sz_temp;
					break;

				}else{

					if (merge->next==merge){
						should_continue = 0;
						break;
					}

				}

				merge = merge->next;

			}

		}

		while (merge->prev != merge)
			merge = merge->prev;

	}

	/* divide by each protection size */
	/* TODO ?? */

#ifdef PRINT_ODIN_PROTECT_BUFFER
	printk("[II] chunks ================================================\n");
	merge_temp = merge;
	while (merge_temp != merge_temp->next){
		printk("[II] addr : %lx ~ %lx, sz %lx\n",
			merge_temp->pa, (merge_temp->pa+merge_temp->sz), merge_temp->sz);
		merge_temp = merge_temp->next;
	}
	printk("[II] addr : %lx ~ %lx, sz %lx\n",
			merge_temp->pa, (merge_temp->pa+merge_temp->sz), merge_temp->sz);

	printk("[II] pretect areas =========================================\n");
	printk("[II] switch protected : addr %lx ~ %lx, size %lx\n",
				(merge->pa+merge->sz-ODIN_DRM_SWITCH_PROTECT_SZ),
				(merge->pa+merge->sz),
				ODIN_DRM_SWITCH_PROTECT_SZ );
	printk("[II] always protected : addr %lx ~ %lx, size %lx\n",
				(merge->pa+merge->sz										\
				-ODIN_DRM_SWITCH_PROTECT_SZ-ODIN_DRM_ALWAYS_PROTECT_SZ),
				(merge->pa+merge->sz-ODIN_DRM_SWITCH_PROTECT_SZ),
				ODIN_DRM_ALWAYS_PROTECT_SZ );
#endif

	/* prepare prootection */

	pool->drm_switch_protect_addr = merge->pa+merge->sz-ODIN_DRM_SWITCH_PROTECT_SZ;
	pool->drm_always_protect_addr =											\
		merge->pa+merge->sz-ODIN_DRM_SWITCH_PROTECT_SZ-ODIN_DRM_ALWAYS_PROTECT_SZ;

	/* attach label */

#ifdef PRINT_ODIN_PROTECT_BUFFER
	printk("[II] each 2M's label =======================================\n");
#endif

	list_for_each_entry_safe( head, item, &pool->high_items, list ){

		pa_temp = (phys_addr_t)page_to_phys(head->page);

		if ( (pa_temp >= pool->drm_switch_protect_addr) &&		\
			(pa_temp < (pool->drm_switch_protect_addr+ODIN_DRM_SWITCH_PROTECT_SZ)) ){
			head->drm_label = ODIN_DRM_SWITCH_PROTECT;
#ifdef PRINT_ODIN_PROTECT_BUFFER
			printk("[II] SWITCH %lx\n", pa_temp );
#endif
		}else if ( (pa_temp >= pool->drm_always_protect_addr) &&	\
			(pa_temp < (pool->drm_always_protect_addr+ODIN_DRM_ALWAYS_PROTECT_SZ)) ){
			head->drm_label = ODIN_DRM_ALWAYS_PROTECT;
#ifdef PRINT_ODIN_PROTECT_BUFFER
			printk("[II] ALWAYS %lx\n", pa_temp );
#endif
		}else{
			head->drm_label = ODIN_DRM_ALWAYS_NOT_PROTECT;
#ifdef PRINT_ODIN_PROTECT_BUFFER
			printk("[II] ALWAYS_NOT %lx\n", pa_temp );
#endif
		}
	}

#ifdef PRINT_ODIN_PROTECT_BUFFER
	printk("[II] =======================================================\n");
#endif

	/* clean up */

	while ( merge != merge->next ){
		merge_temp = merge->next;
		kfree( merge );
		merge = merge_temp;
	}
	kfree( merge );

	while ( start->next != end ){
		member = start->next;
		kfree( start );
		start = member;
	}
	kfree( start );
	kfree( end );

#else	/* CONFIG_ARM_LPAE */

	struct ion_page_pool_item *head, *item;

#ifdef PRINT_ODIN_PROTECT_BUFFER
	printk("[II] pretect areas =========================================\n");
	printk("[II] switch protected : addr 0 size 0\n");
	printk("[II] always protected : addr 0 size 0\n");
#endif

	/* attach label */

#ifdef PRINT_ODIN_PROTECT_BUFFER
	printk("[II] each 2M's label =======================================\n");
#endif

	list_for_each_entry_safe( head, item, &pool->high_items, list ){
		head->drm_label = ODIN_DRM_ALWAYS_NOT_PROTECT;
#ifdef PRINT_ODIN_PROTECT_BUFFER
		printk("[II] ALWAYS NOT %lx\n",
						(phys_addr_t)page_to_phys(head->page) );
#endif
	}

#ifdef PRINT_ODIN_PROTECT_BUFFER
	printk("[II] =======================================================\n");
#endif

#endif	/* CONFIG_ARM_LPAE */

}

void odin_print_sorted_and_merged_ion_pool(struct ion_page_pool *pool)
{

	struct ion_page_pool_item *head, *item;
	struct odin_pa_ll *start, *end, *temp, *member;
	int i=0;


	start = kzalloc( sizeof(struct odin_pa_ll), GFP_KERNEL );
	end = kzalloc( sizeof(struct odin_pa_ll), GFP_KERNEL );
	start->pa = 0;
	start->prev = start;
	start->next = end;
	end->pa = ODIN_MAX_PA_VALUE;
	end->prev = start;
	end->next = end;

	list_for_each_entry_safe( head, item, &pool->high_items, list ){

		member = kzalloc( sizeof(struct odin_pa_ll), GFP_KERNEL );
		member->pa = (phys_addr_t)page_to_phys(head->page);

		temp = start;
		while ( temp->pa < member->pa ){
			temp = temp->next;
		}
		temp->prev->next = member;
		member->next = temp;
		member->prev = temp->prev;
		temp->prev = member;

	}

	printk( "high item --------------------------------------------------------\n");
	while ( start->next != start ){
		member = start->next;

		if ( member != end ){
#ifdef CONFIG_ARM_LPAE
			printk( "%9llx\t", member->pa );
#else
			printk( "%9lx\t", (long unsigned int)(member->pa) );
#endif
			i++;
			if (i==4){
				printk( "\n" );
				i=0;
			}
		}

		kfree( start );
		start = member;
	}
	kfree( start );

	printk( "\n" );

	start = kzalloc( sizeof(struct odin_pa_ll), GFP_KERNEL );
	end = kzalloc( sizeof(struct odin_pa_ll), GFP_KERNEL );
	start->pa = 0;
	start->prev = start;
	start->next = end;
	end->pa = ODIN_MAX_PA_VALUE;
	end->prev = start;
	end->next = end;

	list_for_each_entry_safe( head, item, &pool->low_items, list ){

		member = kzalloc( sizeof(struct odin_pa_ll), GFP_KERNEL );
		member->pa = (phys_addr_t)page_to_phys(head->page);

		temp = start;
		while ( temp->pa < member->pa ){
			temp = temp->next;
		}
		temp->prev->next = member;
		member->next = temp;
		member->prev = temp->prev;
		temp->prev = member;

	}

	printk( "low item ---------------------------------------------------------\n");
	while ( start->next != start ){
		member = start->next;

		if ( member != end ){
#ifdef CONFIG_ARM_LPAE
			printk( "%9llx\t", member->pa );
#else
			printk( "%9lx\t", (long unsigned int)(member->pa) );
#endif
			i++;
			if (i==4){
				printk( "\n" );
				i=0;
			}
		}

		kfree( start );
		start = member;
	}
	kfree( start );

	printk( "\n" );


}

struct odin_ion_buffer_pid {
	pid_t pid;
	struct odin_ion_buffer_pid *prev;
	struct odin_ion_buffer_pid *next;
};

static unsigned long kill_grbuf_deathpending_timeout;

void odin_ion_kill_grbuf_owner( void )
{
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	int tasksize;
	short selected_oom_score_adj = 0;
	struct odin_ion_buffer_pid *head, *member;
	int match_found;


	/* find ION buffer's lastest users */

	head = odin_ion_sort_latest_buffer_user();

	/*
	 * member = head;
	 * while ( member->next != member ){
	 *	printk("pid : %d\n", member->pid);
	 *	member = member->next;
	 * }
	 * printk("pid : %d\n", member->pid);
	*/

	/* find process has the lowest oom score in ION buffer's lastest users */

	rcu_read_lock();

	for_each_process(tsk) {

		struct task_struct *p;
		short oom_score_adj;

		member = head->next->next;	/* skip -1, 0 */

		if (tsk->flags & PF_KTHREAD)
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		if (test_tsk_thread_flag(p, TIF_MEMDIE) &&
		    time_before_eq(jiffies, kill_grbuf_deathpending_timeout)) {
			task_unlock(p);
			rcu_read_unlock();
			return;
		}

		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < selected_oom_score_adj) {
			task_unlock(p);
			continue;
		}

		tasksize = get_mm_rss(p->mm);
		task_unlock(p);
		if (tasksize <= 0)
			continue;

		/* check if ION has the p */

		match_found = 0;
		while ( member->next != member ){
			if ( p->pid == member->pid )
				match_found = 1;
			member = member->next;
		}

		/* select process to be killed */

		if ( match_found == 1 ){
			selected = p;
			selected_oom_score_adj = oom_score_adj;
		}

	}

	if (selected) {

		kill_grbuf_deathpending_timeout = jiffies + HZ;
		send_sig(SIGKILL, selected, 0);
		set_tsk_thread_flag(selected, TIF_MEMDIE);
		printk(KERN_WARNING "[II] kill %s for %s\n",
			     selected->comm, current->comm );

	}

	rcu_read_unlock();

	/* cleaning resources */

	odin_ion_cleaning_sort_result(head);

	if (!selected)
		printk(KERN_WARNING "[II] no one\n" );

}
#endif

static void *ion_page_pool_alloc_pages(struct ion_page_pool *pool)
{
	struct page *page = alloc_pages(pool->gfp_mask, pool->order);

	if (!page)
		return NULL;
	/* this is only being used to flush the page for dma,
	   this api is not really suitable for calling from a driver
	   but no better way to flush a page for dma exist at this time */
#ifndef CONFIG_ODIN_ION_SMMU
	/* Why do we need this?												*/
	/* When we use the buffer, sync function will be called properly.	*/
	/* Block this for speed up											*/
	arm_dma_ops.sync_single_for_device(NULL,
					   pfn_to_dma(NULL, page_to_pfn(page)),
					   PAGE_SIZE << pool->order,
					   DMA_BIDIRECTIONAL);
#endif
	return page;
}

static void ion_page_pool_free_pages(struct ion_page_pool *pool,
				     struct page *page)
{
	__free_pages(page, pool->order);
}

#ifndef CONFIG_ODIN_ION_SMMU
static int ion_page_pool_add(struct ion_page_pool *pool, struct page *page)
{
	struct ion_page_pool_item *item;

	item = kzalloc(sizeof(struct ion_page_pool_item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;

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
	kfree(item);
	return page;
}
#else
static int odin_ion_get_area_item_belonged(
										struct ion_page_pool *pool,
										phys_addr_t addr)
{
	if ( (addr >= pool->drm_switch_protect_addr) &&		\
		(addr < (pool->drm_switch_protect_addr+ODIN_DRM_SWITCH_PROTECT_SZ)) )
		return ODIN_DRM_SWITCH_PROTECT;
	else if ( (addr >= pool->drm_always_protect_addr) &&	\
		(addr < (pool->drm_always_protect_addr+ODIN_DRM_ALWAYS_PROTECT_SZ)) )
		return ODIN_DRM_ALWAYS_PROTECT;
	else
		return ODIN_DRM_ALWAYS_NOT_PROTECT;
}

static int odin_ion_get_label_to_get_item(int secure,
										int drm_always_protect_cnt)
{

	if ( secure == 0 )
		return ODIN_DRM_ALWAYS_NOT_PROTECT;

	if ( drm_always_protect_cnt < ODIN_DRM_ALWAYS_PROTECT_CNT )
		return ODIN_DRM_ALWAYS_PROTECT;
	else
		return ODIN_DRM_SWITCH_PROTECT;

}

static int ion_page_pool_add(struct ion_page_pool *pool,
										struct page *page)
{
	struct ion_page_pool_item *item;
	int item_label;
	int unprotect_switch_buf=0;

	item = kzalloc(sizeof(struct ion_page_pool_item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;

	mutex_lock(&pool->mutex);

	if (pool->drm_always_protect_addr == 0){

		/* call from odin_ion_prepare_pool_protection */

		item->page = page;
		list_add_tail(&item->list, &pool->high_items);
		pool->high_count++;

	}else{

		item_label = odin_ion_get_area_item_belonged(	\
								pool, (phys_addr_t)page_to_phys(page));

		item->page = page;
		item->drm_label = item_label;
		list_add_tail(&item->list, &pool->high_items);
		pool->high_count++;

		if (item_label == ODIN_DRM_SWITCH_PROTECT){
			(pool->drm_switch_protect_cnt)--;
			if ( pool->drm_switch_protect_cnt==0 )
				unprotect_switch_buf = 1;
		}else if (item_label == ODIN_DRM_ALWAYS_PROTECT){
			(pool->drm_always_protect_cnt)--;
		}

#ifdef PRINT_ODIN_PROTECT_BUFFER
		printk("[II] returned item : %d\n", item->drm_label);
#endif

	}

	mutex_unlock(&pool->mutex);

	if (unprotect_switch_buf==1){

#ifdef PRINT_ODIN_PROTECT_BUFFER
		printk("[II] UNprotect SWITCH\n");
#endif

		tzmem_unmap_buffer(pool->drm_switch);
		tzmem_delete_list(pool->drm_switch);
	}

	return 0;
}

static struct page *ion_page_pool_remove(
							struct ion_page_pool *pool, bool high,
							int secure,
							int *protect_switch, int *protect_always)
{
	struct ion_page_pool_item *item;
	struct page *page;
	int item_label_to_get;
	int i;

	if (pool->high_count==0){
		printk("[II] pool's empty\n");
		return NULL;
	}

	*protect_switch = 0;
	*protect_always = 0;

	item_label_to_get = odin_ion_get_label_to_get_item(
								secure, pool->drm_always_protect_cnt);

#ifdef PRINT_ODIN_PROTECT_BUFFER
	printk("[II] item asked : %d\n", item_label_to_get);
#endif

	for (i=0; i<ODIN_DRM_POOL_ENTRY_CNT; i++){

		item = list_first_entry(&pool->high_items,
								struct ion_page_pool_item, list);

#ifdef PRINT_ODIN_PROTECT_BUFFER
		printk("[II] item(%d) : %p, %d, %lx\n", i, item,	\
				item->drm_label, (phys_addr_t)page_to_phys(item->page));
#endif

		if (item->drm_label == item_label_to_get){
			break;
		}else{
			list_del(&item->list);
			list_add_tail(&item->list, &pool->high_items);
		}

	}

	if (i<ODIN_DRM_POOL_ENTRY_CNT){

#ifdef PRINT_ODIN_PROTECT_BUFFER
		printk("[II] found : %d\n", item->drm_label);
#endif

		pool->high_count--;
		list_del(&item->list);
		page = item->page;
		kfree(item);

		if (item_label_to_get==ODIN_DRM_ALWAYS_PROTECT){
			(pool->drm_always_protect_cnt)++;
			if (pool->drm_always_protect_started==0){
				*protect_always = 1;
				pool->drm_always_protect_started = 1;
			}
		}else if (item_label_to_get==ODIN_DRM_SWITCH_PROTECT){
			(pool->drm_switch_protect_cnt)++;
			if (pool->drm_switch_protect_cnt==1)
				*protect_switch = 1;
		}

		return page;

	}

	if ( item_label_to_get == ODIN_DRM_ALWAYS_NOT_PROTECT )
		printk("[II] ALWAYS_NOT asked, but can't get\n");
	else if ( item_label_to_get == ODIN_DRM_ALWAYS_PROTECT )
		printk("[II] ALWAYS asked, but can't get\n");
	else /* item_label_to_get == ODIN_DRM_SWITCH_PROTECT */
		printk("[II] SWITCH asked, but can't get\n");

	return NULL;
}
#endif

#ifndef CONFIG_ODIN_ION_SMMU
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

	if (!page)
		page = ion_page_pool_alloc_pages(pool);

	return page;
}
#else
void *ion_page_pool_alloc(struct ion_page_pool *pool, int secure)
{
	struct page *page = NULL;
	int protect_switch=0;
	int protect_always=0;

	BUG_ON(!pool);

	mutex_lock(&pool->mutex);

	if (pool->high_count)
		page = ion_page_pool_remove(pool, true, secure,
									&protect_switch, &protect_always);
	else if (pool->low_count)
		page = ion_page_pool_remove(pool, false, secure,
									&protect_switch, &protect_always);

	mutex_unlock(&pool->mutex);

	if (protect_switch==1){

#ifdef PRINT_ODIN_PROTECT_BUFFER
		printk("[II] protect SWITCH\n");
#endif

		pool->drm_switch = tzmem_init_list();
		tzmem_add_entry(pool->drm_switch,
						pool->drm_switch_protect_addr,
						ODIN_DRM_SWITCH_PROTECT_SZ, DRM_DPB);
		tzmem_map_buffer(pool->drm_switch);
	}else if (protect_always==1){

#ifdef PRINT_ODIN_PROTECT_BUFFER
		printk("[II] protect ALWAYS\n");
#endif

		pool->drm_always = tzmem_init_list();
		tzmem_add_entry(pool->drm_always,
						pool->drm_always_protect_addr,
						ODIN_DRM_ALWAYS_PROTECT_SZ, DRM_DPB);
		tzmem_map_buffer(pool->drm_always);
	}

	if ( (!page) && (secure==0) )
		page = ion_page_pool_alloc_pages(pool);

	return page;
}
#endif

void ion_page_pool_free(struct ion_page_pool *pool, struct page* page)
{
	int ret;

	ret = ion_page_pool_add(pool, page);
	if (ret)
		ion_page_pool_free_pages(pool, page);
}

#ifdef CONFIG_ODIN_ION_SMMU
int ion_page_pool_total(struct ion_page_pool *pool, bool high)
#else
static int ion_page_pool_total(struct ion_page_pool *pool, bool high)
#endif
{
	int total = 0;

	total += high ? (pool->high_count + pool->low_count) *
		(1 << pool->order) :
			pool->low_count * (1 << pool->order);
	return total;
}

#ifndef CONFIG_ODIN_ION_SMMU
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
#else
#ifndef	CONFIG_ODIN_ION_SMMU_4K
int ion_page_pool_shrink(struct ion_page_pool *pool, gfp_t gfp_mask,
				int nr_to_scan)
{

	/* MAKE_ODIN_ION_POOL - check */

	int i, pages_in_pool, pages_to_report;
	bool high;
	unsigned long cache_sz;

	if (pool->order != 9)
		return 0;

	if ( odin_has_dram_3gbyte()==true )
		cache_sz = ODIN_3GDDR_ION_CACHE_SIZE;
	else
		cache_sz = ODIN_2GDDR_ION_CACHE_SIZE;

	high = gfp_mask & __GFP_HIGHMEM;

	pages_in_pool = ion_page_pool_total(pool, high);
	if( pages_in_pool > ((cache_sz/SZ_2M)*(1<<pool->order)) )
		pages_to_report = pages_in_pool - ((cache_sz/SZ_2M)*(1<<pool->order));
	else
		pages_to_report = 0;

	if (nr_to_scan == 0)
		return pages_to_report;

	for (i = 0; i < pages_to_report; ) {
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
		i += (1 << pool->order);
	}

	return i;
}
#else
int ion_page_pool_shrink(struct ion_page_pool *pool, gfp_t gfp_mask,
				int nr_to_scan)
{

	return 0;

#if 0
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
#endif
}
#endif
#endif

struct ion_page_pool *ion_page_pool_create(gfp_t gfp_mask, unsigned int order)
{
	struct ion_page_pool *pool = kmalloc(sizeof(struct ion_page_pool),
					     GFP_KERNEL);
	if (!pool)
		return NULL;
#ifdef CONFIG_ODIN_ION_SMMU
	pool->drm_switch_protect_addr = 0;
	pool->drm_always_protect_addr = 0;
	pool->drm_switch_protect_cnt = 0;
	pool->drm_always_protect_cnt = 0;
	pool->drm_always_protect_started = 0;
	pool->drm_switch = NULL;
	pool->drm_always = NULL;
#endif
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
	return 0;
}

static void __exit ion_page_pool_exit(void)
{
}

module_init(ion_page_pool_init);
module_exit(ion_page_pool_exit);
