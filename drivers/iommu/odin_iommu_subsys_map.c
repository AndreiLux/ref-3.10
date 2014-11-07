
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


#include <linux/io.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/genalloc.h>
#include <linux/odin_iommu.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include "odin_iommu.h"
#include "odin_iommu_domains.h"
#include "odin_iommu_subsys_map.h"
#include "odin_iommu_hw.h"


struct odin_buffer_node {
	struct rb_node rb_node_all_buffer;
	struct list_head *subsys_iova_list; /* struct mapped_subsys_iova_list */
	unsigned long length;
	unsigned long subsystems;
	unsigned int nsubsys;
	struct ion_buffer *buffer;
	unsigned long align;
};

static struct rb_root buffer_root;

static DEFINE_SPINLOCK(odin_buffer_lock);

struct odin_ion_buffer_pid {
	pid_t pid;
	struct odin_ion_buffer_pid *prev;
	struct odin_ion_buffer_pid *next;
};

bool odin_ion_buffer_from_big_page_pool(
				unsigned int map_subsys, size_t size, unsigned int flags )
{
#if 0
	if ( (flags&ION_FLAG_SECURE) != 0 ){
		return true;
	}else if ( (map_subsys&(ODIN_SUBSYS_VSP|ODIN_SUBSYS_CSS)) != 0 ){
		if ( size < SZ_1M )
			return false;
		else
			return true;
	}else{
		return false;
	}
#else
	if ( (flags&(ION_FLAG_SECURE|ION_FLAG_OUT_BUFFER)) != 0 ){
		return true;
	}else{
		return false;
	}
#endif
}
EXPORT_SYMBOL( odin_ion_buffer_from_big_page_pool );

struct odin_ion_buffer_pid * odin_ion_sort_latest_buffer_user(void)
{

	struct rb_node *rb;
	struct odin_ion_buffer_pid *head, *tail, *temp, *member;


	/* Insert code for the case kzalloc fail */

	head = kzalloc( sizeof(struct odin_ion_buffer_pid), GFP_KERNEL );
	tail = kzalloc( sizeof(struct odin_ion_buffer_pid), GFP_KERNEL );
	head->pid = -1;
	head->prev = head;
	head->next = tail;
	tail->pid = 0x7FFFFFFF;
	tail->prev = head;
	tail->next = tail;

	for ( rb=rb_first(&buffer_root); rb; rb=rb_next(rb) ){

		struct odin_buffer_node *node;
		node = rb_entry( rb, struct odin_buffer_node, rb_node_all_buffer );

		member = kzalloc( sizeof(struct odin_ion_buffer_pid), GFP_KERNEL );
		member->pid = node->buffer->pid;

		temp = head;
		while ( temp->pid < member->pid ){
			temp = temp->next;
			if ( temp==temp->next )
				break;
		}

		if ( temp->pid == member->pid ){
			kfree( member );
			continue;
		}

		temp->prev->next = member;
		member->next = temp;
		member->prev = temp->prev;
		temp->prev = member;

	}

	return head;

}

void odin_ion_cleaning_sort_result(struct odin_ion_buffer_pid *head)
{

	struct odin_ion_buffer_pid *member;


	while ( head->next != head ){
		member = head->next;
		kfree( head );
		head = member;
	}
	kfree( head );

}

static char * get_domain_name_from_subsys_num( unsigned int subsys_num )
{

	switch ( subsys_num ){

		case ODIN_SUBSYS_CSS :
			return "css";

		case ODIN_SUBSYS_VSP :
			return "vsp";

		case ODIN_SUBSYS_DSS :
			return "dss";

		default :
			return "";

	}

}

void odin_print_ion_unused_buffer( void )
{

	int total_size;


	printk( "==================================================================\n");

	total_size = ion_page_pool_total(odin_ion_get_sys_heap_pool(SZ_2M), true)<<12;
	if (total_size!=0)
		odin_print_sorted_and_merged_ion_pool(odin_ion_get_sys_heap_pool(SZ_2M));

	printk( "total size of 2MB cached buffers (not used)\n");
	printk( "\t0x%x [bytes]\t%d [bytes]\n", total_size, total_size );

	printk( "==================================================================\n");

	total_size = ion_page_pool_total(odin_ion_get_sys_heap_pool(SZ_4K), true)<<12;

	printk( "total size of 4KB cached buffers (not used)\n");
	printk( "\t0x%x [bytes]\t%d [bytes]\n", total_size, total_size );

	printk( "==================================================================\n");

}

void odin_print_ion_used_buffer( void )
{

	struct rb_node *rb;
	struct mapped_subsys_iova_list *subsys_iova_list_elem, *va_tmp;
	int buf_cnt_2m=0, buf_cnt_4k=0, i, j=0;
	struct scatterlist *sg;


	printk( "==================================================================\n");

	for ( rb=rb_first(&buffer_root); rb; rb=rb_next(rb) ){

		struct odin_buffer_node *node;

		node = rb_entry( rb, struct odin_buffer_node, rb_node_all_buffer );

		list_for_each_entry_safe( subsys_iova_list_elem, va_tmp,
									node->subsys_iova_list, list ){
			printk( "\t%s : %lx",
				get_domain_name_from_subsys_num(subsys_iova_list_elem->subsys),
				subsys_iova_list_elem->iova );
		}
		printk( "\t%s %d (sz:0x%lx)\n", node->buffer->task_comm, node->buffer->pid,	\
										node->buffer->size);

		printk( "\tk : %lx\tu : %d %lx\t%s\n",
				(unsigned long)(node->buffer->vaddr),
				node->buffer->mapped_pid, node->buffer->vm_start,
				node->buffer->mapped_task_comm);

		i=0;
		for_each_sg(node->buffer->sg_table->sgl, sg,
						node->buffer->sg_table->nents, j){

			if ( node->align == SZ_2M ){
#ifdef CONFIG_ARM_LPAE
				printk( "\t%llx", sg_dma_address(sg) );
#else
				printk( "\t%lx", sg_dma_address(sg) );
#endif
				i++;
				if (i==4){
					printk("\n");
					i=0;
				}
			}

			if ( node->align == SZ_2M )
				buf_cnt_2m++;
			else if ( node->align == SZ_4K )
				buf_cnt_4k++;
		}
		printk( "\n\n" );

	}

	printk( "total size of 2MB cached buffers (used)\n");
	printk( "\t0x%x [bytes]\t%d [bytes]\n", buf_cnt_2m*SZ_2M, buf_cnt_2m*SZ_2M );

	printk( "total size of 4KB cached buffers (used)\n");
	printk( "\t0x%x [bytes]\t%d [bytes]\n", buf_cnt_4k*SZ_4K, buf_cnt_4k*SZ_4K );

	printk( "==================================================================\n");

}

void odin_print_ion_used_buffer_sorted( void )
{

	struct rb_node *rb;
	struct odin_pa_sz_ll *head, *tail, *temp, *member;
	int buf_cnt_2m=0, buf_cnt_4k=0, i=0;
	struct scatterlist *sg;


	printk( "==================================================================\n");

	head = kzalloc( sizeof(struct odin_pa_sz_ll), GFP_KERNEL );
	tail = kzalloc( sizeof(struct odin_pa_sz_ll), GFP_KERNEL );
	head->pa = 0;
	head->prev = head;
	head->next = tail;
	tail->pa = ODIN_MAX_PA_VALUE;
	tail->prev = head;
	tail->next = tail;

	for ( rb=rb_first(&buffer_root); rb; rb=rb_next(rb) ){

		struct odin_buffer_node *node;
		node = rb_entry( rb, struct odin_buffer_node, rb_node_all_buffer );

		for_each_sg(node->buffer->sg_table->sgl, sg,
						node->buffer->sg_table->nents, i){

			member = kzalloc( sizeof(struct odin_pa_sz_ll), GFP_KERNEL );
			member->pa = sg_dma_address(sg);
			member->sz = node->align;

			temp = head;
			while ( temp->pa < member->pa ){
				temp = temp->next;
				if ( temp==temp->next )
					break;
			}

			temp->prev->next = member;
			member->next = temp;
			member->prev = temp->prev;
			temp->prev = member;

			if ( node->align == SZ_2M )
				buf_cnt_2m++;
			else if ( node->align == SZ_4K )
				buf_cnt_4k++;

		}

	}

	i=0;
	while ( head->next != head ){
		member = head->next;

		if ( member->sz == SZ_2M ){
			if ( member != tail ){
#ifdef CONFIG_ARM_LPAE
				printk( "%9llx\t", member->pa );
#else
				printk( "%9lx\t", member->pa );
#endif
				i++;
				if (i==4){
					printk( "\n" );
					i=0;
				}
			}
		}

		kfree( head );
		head = member;
	}
	kfree( head );

	printk( "\n");
	printk( "total size of 2MB cached buffers (used)\n");
	printk( "\t0x%x [bytes]\t%d [bytes]\n", buf_cnt_2m*SZ_2M, buf_cnt_2m*SZ_2M );

	printk( "total size of 4KB cached buffers (used)\n");
	printk( "\t0x%x [bytes]\t%d [bytes]\n", buf_cnt_4k*SZ_4K, buf_cnt_4k*SZ_4K );

	printk( "==================================================================\n");

}

/* This is not very beautiful.										*/
/* But without this, Skia has to be changed too much.					*/
/* This is only for limited scenario.									*/
/* Don't use this!!												*/
/* You should keep your offset-less-user_process_va & call get-put pair.	*/
struct ion_buffer * odin_get_buffer_by_usr_addr(
								struct ion_client *client, void *usr_addr,
								unsigned long *offset)
{

	struct rb_node *rb;
	struct ion_buffer *buffer = NULL;
	unsigned long vm_start = 0;


	for ( rb=rb_first(&buffer_root); rb; rb=rb_next(rb) ){

		struct odin_buffer_node *node;

		node = rb_entry( rb, struct odin_buffer_node, rb_node_all_buffer );

		if ( node->buffer->vm_start != 0 ){

			if ( node->buffer->vm_start <= (unsigned long)usr_addr ){

				if ( vm_start <= node->buffer->vm_start ){

					buffer = node->buffer;
					vm_start = node->buffer->vm_start;

				}

			}

		}

	}
	*offset = (unsigned long)usr_addr - vm_start;

	return buffer;

}

#if 0
static char * get_domain_name_from_irq_num( unsigned int irq_num )
{

	switch ( irq_num ){

		case IOMMU_NS_IRQ_CSS_0	:
		case IOMMU_NS_IRQ_CSS_1	:
			return "css";

		case IOMMU_NS_IRQ_VSP_0	:
		case IOMMU_NS_IRQ_VSP_1	:
			return "vsp";

		case IOMMU_NS_IRQ_DSS_0	:
		case IOMMU_NS_IRQ_DSS_1	:
		case IOMMU_NS_IRQ_DSS_2	:
		case IOMMU_NS_IRQ_DSS_3	:
			return "dss";

		case IOMMU_NS_IRQ_ICS	:
			return "ics";

		case IOMMU_NS_IRQ_AUD	:
			return "aud";

		case IOMMU_NS_IRQ_PERI	:
			return "peri";

		case IOMMU_NS_IRQ_SDMA	:
			return "sdma";

		case IOMMU_NS_IRQ_SES	:
			return "ses";

		default					:
			return "";

	}

}
#endif

static struct odin_buffer_node *find_buffer(void *key)
{

	struct rb_root *root = &buffer_root;
	struct rb_node *temp = root->rb_node;
	struct rb_node *p = NULL;

	spin_lock(&odin_buffer_lock);
	p = root->rb_node;

	if (temp != p)
		printk(KERN_INFO" buffer root is changed to %x from %x \n", temp, p);

	while (p) {
		struct odin_buffer_node *node;

		node = rb_entry(p, struct odin_buffer_node, rb_node_all_buffer);

		if (key < (void *)node->subsys_iova_list)
			p = p->rb_left;
		else if (key > (void *)node->subsys_iova_list)
			p = p->rb_right;
		else {
			spin_unlock(&odin_buffer_lock);
			return node;
		}
	}

	spin_unlock(&odin_buffer_lock);
	return NULL;

}

static int add_buffer( struct odin_buffer_node * node )
{

	struct rb_root *root = &buffer_root;
	struct rb_node **p = NULL;
	struct rb_node *parent = NULL;
	void *key;

	spin_lock(&odin_buffer_lock);

	key = node->subsys_iova_list;
	p = &root->rb_node;

	while ( *p ){
		struct odin_buffer_node *tmp;
		parent = *p;
		tmp = rb_entry(parent, struct odin_buffer_node, rb_node_all_buffer);

		if (key < (void *)tmp->subsys_iova_list)
			p = &(*p)->rb_left;
		else if (key > (void *)tmp->subsys_iova_list)
			p = &(*p)->rb_right;
		else {
			/* If this warning situation is occured, it is tried to add buffer twice! */
			spin_unlock(&odin_buffer_lock);
			printk(KERN_WARNING "[II] %s error\n", __func__);
			return -EINVAL;
		}
	}
	rb_link_node(&node->rb_node_all_buffer, parent, p);
	rb_insert_color(&node->rb_node_all_buffer, root);

	spin_unlock(&odin_buffer_lock);
	return 0;

}

static int remove_buffer(struct odin_buffer_node *victim_node)
{

	struct rb_root *root = &buffer_root;


	if (!victim_node)
		return -EINVAL;

	spin_lock(&odin_buffer_lock);
	rb_erase(&victim_node->rb_node_all_buffer, root);
	spin_unlock(&odin_buffer_lock);

	return 0;

}

static unsigned int calc_subsys_bitmap( unsigned int subsys_flags,
						unsigned int * subsys_num, unsigned long already_mapped_subsys )
{

	unsigned int subsys_to_map = 0;


	if ( (subsys_flags & ODIN_SUBSYS_CSS) &&
		((already_mapped_subsys & ODIN_SUBSYS_CSS) == 0) ){
		subsys_to_map = subsys_to_map | ODIN_SUBSYS_CSS;
		*subsys_num = *subsys_num + 1;
	}
	if ( (subsys_flags & ODIN_SUBSYS_VSP) &&
		((already_mapped_subsys & ODIN_SUBSYS_VSP) == 0) ){
		subsys_to_map = subsys_to_map | ODIN_SUBSYS_VSP;
		*subsys_num = *subsys_num + 1;
	}
	if ( (subsys_flags & ODIN_SUBSYS_DSS) &&
		((already_mapped_subsys & ODIN_SUBSYS_DSS) == 0) ){
		subsys_to_map = subsys_to_map | ODIN_SUBSYS_DSS;
		*subsys_num = *subsys_num + 1;
	}

	return subsys_to_map;

}

int odin_subsystem_map_buffer( unsigned int subsys, int pool_num,
								unsigned long size, struct ion_buffer *buffer,
								unsigned long align )
{

	int ret=0, i, j = 0, alloc_or_map;
	struct odin_buffer_node *node;
	unsigned int subsys_num = 0;
	unsigned int subsys_to_map, subsys_flags, mapped_subsys_num;
	unsigned int domain_num;
	struct iommu_domain *domain = NULL;
	unsigned long iova_start = 0;
	unsigned long iova_to_map = 0;
	struct mapped_subsys_iova_list *subsys_iova_elem;
	struct mapped_subsys_iova_list *tmp;
	struct scatterlist *sg;
	struct sg_table *sg_tbl;


	node = find_buffer( &(buffer->subsys_iova_list) );

	if ( likely(node == NULL) ){
		alloc_or_map = ODIN_ALLOC_BUFFER;
		node = kzalloc( sizeof(*node), GFP_ATOMIC );
		if ( unlikely(!node) ){
			printk("[II] fail kzalloc for node : %s\n", __func__);
			ret = (-ENOMEM);
			goto out_func;
		}
	}else{
		alloc_or_map = ODIN_MAP_BUFFER;
	}

	subsys_to_map = calc_subsys_bitmap( subsys, &subsys_num,
											node->subsystems );

	subsys_flags = 1;
	mapped_subsys_num = 0;
	for ( i=0; i<subsys_num; i++ ){

		while ( (subsys_to_map & subsys_flags) == 0 ){
				subsys_flags = subsys_flags << 1;
		}

		domain_num = odin_get_domain_num_from_subsys( subsys_flags );
		domain = odin_get_iommu_domain( domain_num );

#ifdef	CONFIG_ODIN_ION_SMMU_4K
		if ( i==0 ) {
#endif
			iova_start = odin_allocate_iova_address(
								domain_num, pool_num, size, 0);
#ifdef	CONFIG_ODIN_ION_SMMU_4K
		}

		if ( unlikely(get_odin_debug_v_message() == true) ){
			pr_info("[II] alloc IOVA\t%9lx\t%lx\t%s\t(p %llx)\n",
							iova_start, size,
							odin_get_domain_name_from_subsys(subsys_flags),
							0);
		}
#endif

		if ( unlikely(iova_start == 0) ){

			printk("[II] fail alloc iova : %s\n", __func__);
			subsys_to_map = subsys_to_map & (~subsys_flags);
			subsys_flags = subsys_flags << 1;
			continue;

		}

#ifdef	CONFIG_ODIN_ION_SMMU_4K
		if ( i==0 ){
#endif
			ret = 0;
			iova_to_map = iova_start;

			if ( buffer->heap->id == ODIN_ION_SYSTEM_HEAP1 ){

				sg_tbl = (struct sg_table *)(buffer->priv_virt);
				for_each_sg(sg_tbl->sgl, sg, sg_tbl->nents, j){

					/* ret = iommu_map( domain, iova_to_map,				*/
					/*				sg_dma_address(sg), (sg->length), 0 );	*/
					ret = domain->ops->map(domain, iova_to_map,
									sg_dma_address(sg), (sg->length), 0);
					if ( unlikely(ret) ){
						break;
					}
					iova_to_map += sg->length;

				}

			}else{

				ret = iommu_map( domain, iova_to_map,
								(phys_addr_t)(buffer->priv_phys), buffer->size, 0 );
				iova_to_map += buffer->size;

			}

#ifdef	CONFIG_ODIN_ION_SMMU_4K
		}
#endif

		if ( unlikely(ret) ){

			iommu_unmap( domain, iova_start, (iova_to_map-iova_start) );
			odin_free_iova_address( iova_start, domain_num, pool_num,
										size, 0 );

			subsys_to_map = subsys_to_map & (~subsys_flags);
			subsys_flags = subsys_flags << 1;
			continue;

		}else{

			subsys_iova_elem =
						kzalloc( sizeof(struct mapped_subsys_iova_list), GFP_KERNEL );
			if ( unlikely(!subsys_iova_elem) ){

				printk("[II] fail kzalloc for iova_list : %s\n", __func__);

#ifdef	CONFIG_ODIN_ION_SMMU_4K
				if ( i==0 ){
#endif
					iommu_unmap( domain, iova_start, (iova_to_map-iova_start) );
					odin_free_iova_address( iova_start, domain_num, pool_num,
											size, 0 );
#ifdef	CONFIG_ODIN_ION_SMMU_4K
				}
#endif

				subsys_to_map = subsys_to_map & (~subsys_flags);
				subsys_flags = subsys_flags << 1;
				continue;

			}

			subsys_iova_elem->subsys = subsys_flags;
			subsys_iova_elem->iova = iova_start;
			list_add_tail( &(subsys_iova_elem->list), &(buffer->subsys_iova_list) );
			mapped_subsys_num++;

		}

		subsys_flags = subsys_flags << 1;

	}

	if ( likely(mapped_subsys_num != 0) ){

		node->subsys_iova_list = &(buffer->subsys_iova_list);
		node->length = size;
		node->buffer = buffer;
		node->align = align;
		if ( alloc_or_map == ODIN_MAP_BUFFER ){

			node->subsystems |= subsys_to_map;
			node->nsubsys += mapped_subsys_num;

		}else{

			node->subsystems = subsys_to_map;
			node->nsubsys = mapped_subsys_num;

			if ( unlikely(add_buffer(node) != 0) ) {
				printk("[II] fail add_buffer : %s\n", __func__);
				ret = -EINVAL;
				goto add_buffer_fail;
			}

		}
	}

	/* User can call this function without subsys. */
	/* So, if mapped_subsys_num is 0, it can be no-error */
	return 0;

add_buffer_fail:

	subsys_flags = 1;
	for ( i=0; i<mapped_subsys_num; i++ ){

		while ( (subsys_to_map & subsys_flags) == 0 ){
			subsys_flags = subsys_flags << 1;
		}

		domain_num = odin_get_domain_num_from_subsys( subsys_flags );
		domain = odin_get_iommu_domain( domain_num );

		list_for_each_entry_safe( subsys_iova_elem, tmp,
										&(buffer->subsys_iova_list), list ){
			if ( subsys_iova_elem->subsys == subsys_flags ){
#ifdef	CONFIG_ODIN_ION_SMMU_4K
				if ( i==0 ){
#endif
					iommu_unmap( domain, (subsys_iova_elem->iova), size );
					odin_free_iova_address( (subsys_iova_elem->iova),
											domain_num, pool_num, size, 0 );
#ifdef	CONFIG_ODIN_ION_SMMU_4K
				}
#endif
				list_del( &subsys_iova_elem->list );
				kfree( subsys_iova_elem );
				break;
			}
		}

		subsys_flags = subsys_flags << 1;

	}

	kfree(node);

out_func :

	pr_err("[II] %s : map_fail\n", __func__ );
	return ret;

}

int odin_subsystem_unmap_buffer( struct gen_pool * smmu_heap_pool,
										struct list_head * subsys_iova_list,
										int pool_num )
{

	struct odin_buffer_node *node;
	struct mapped_subsys_iova_list *iova_entry;
	struct mapped_subsys_iova_list *iova_tmp;
	unsigned int domain_num;
	struct iommu_domain *subsys_domain;
#ifdef	CONFIG_ODIN_ION_SMMU_4K
	int i;
#endif


	node = find_buffer( subsys_iova_list );
	if ( unlikely(!node) )
		return -EINVAL;

#ifdef	CONFIG_ODIN_ION_SMMU_4K
	i = 0;
#endif
	list_for_each_entry_safe( iova_entry, iova_tmp, subsys_iova_list, list ){

#ifdef	CONFIG_ODIN_ION_SMMU_4K
		if ( i==0 ){
#endif
			domain_num = odin_get_domain_num_from_subsys( iova_entry->subsys );
			subsys_domain = odin_get_iommu_domain( domain_num );

			iommu_unmap( subsys_domain, iova_entry->iova, node->length );
			odin_free_iova_address( iova_entry->iova, domain_num,
										pool_num, node->length, 0);
#ifdef	CONFIG_ODIN_ION_SMMU_4K
		}else{
			if ( unlikely(get_odin_debug_v_message() == true) ){
				pr_info("[II] free  IOVA\t%9lx\t%lx\t%s\t(p %llx)\n",
						iova_entry->iova, node->length,
						odin_get_domain_name_from_subsys(iova_entry->subsys),
						0);
			}
		}
#endif

		list_del( &iova_entry->list );
		kfree( iova_entry );

#ifdef	CONFIG_ODIN_ION_SMMU_4K
		i++;
#endif

	}

	if ( unlikely(pool_num == CARVEOUT_POOL) ){

		if ( unlikely(get_odin_debug_v_message() == true) ){
#ifdef CONFIG_ARM_LPAE
			pr_info("[II] free PA carve_smmu heap : %llx \t %lx\n",
#else
			pr_info("[II] free PA carve_smmu heap : %lx \t %lx\n",
#endif
								(phys_addr_t)(node->buffer->priv_phys),
								node->buffer->size);
		}
		gen_pool_free( smmu_heap_pool,
								node->buffer->priv_phys,
								node->buffer->size);
	}

	remove_buffer( node );
	kfree( node );

	return 0;

}
EXPORT_SYMBOL( odin_subsystem_unmap_buffer );

