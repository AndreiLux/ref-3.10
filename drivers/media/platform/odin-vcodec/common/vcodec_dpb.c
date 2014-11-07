/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
 * Seokhoon Kang <m4seokhoon.kang@lgepartner.com>
 * Inpyo Cho <inpyo.cho@lge.com>
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
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/ion.h>
#include <linux/list.h>
#include <linux/odin_iommu.h>
#include <linux/vmalloc.h>

#include <media/odin/vcodec/vlog.h>

#include "vcodec_dpb.h"

struct vcodec_dpb_node {
	unsigned int paddr;
	unsigned long *vaddr;
	unsigned int size;
	int fd;

	struct ion_handle *ion_handle;

	struct list_head list;
};

struct vcodec_dpb_inst {
	struct ion_client *ion_client;
	struct list_head dpb_list;

	struct list_head list;
};


struct vcodec_dpb_paddr_node {
	unsigned int masked_paddr;
	unsigned int bit_table;
	struct list_head list;
};

#ifdef CONFIG_ODIN_ION_SMMU_4K
static const unsigned int page_size = SZ_4K;
static const unsigned int sub_mask = 0xFFFF0000;
static const unsigned int page_size_bit = 15;
#else /* 2M */
static const unsigned int page_size = SZ_2M;
static const unsigned int sub_mask = 0xFC000000;
static const unsigned int page_size_bit = 21;
#endif
static struct list_head paddr_list;

static struct list_head inst_list;
struct mutex dpb_lock;


void ___vcodec_dpb_print_inst(void)
{
	struct vcodec_dpb_inst *inst_node, *inst_tmp;
	struct vcodec_dpb_node *node, *tmp;

	list_for_each_entry_safe(inst_node, inst_tmp, &inst_list, list) {
		list_for_each_entry_safe(node, tmp, &inst_node->dpb_list, list) {
			vlog_error("paddr 0x%08x, size 0x%08X\n", node->paddr, node->size);
		}
	}
}

void __vcodec_dpb_print_paddr(void)
{
	struct vcodec_dpb_paddr_node *node, *tmp;

	___vcodec_dpb_print_inst();

	list_for_each_entry_safe(node, tmp, &paddr_list, list) {
		vlog_error("node->masked_paddr : 0x%08X, bit_table : 0x%08X\n",
			node->masked_paddr,node->bit_table);
	}

	return;
}

static struct vcodec_dpb_paddr_node *__vcodec_dpb_find_paddr(unsigned int paddr)
{
	struct vcodec_dpb_paddr_node *node, *tmp;
	unsigned int masked_paddr = paddr & sub_mask;

	list_for_each_entry_safe(node, tmp, &paddr_list, list) {
		if (node->masked_paddr == masked_paddr) {
			return node;
		}
	}

	return NULL;
}

static unsigned int __vcodec_dpb_get_mask_bit(
								unsigned int paddr, unsigned int size)
{
	unsigned int aligned_size_unit;
	unsigned int masking_bit_num;
	unsigned int masking_bit;
	unsigned int loop_idx;

	masking_bit = 0x0;
	aligned_size_unit = ALIGN(size, page_size) >> page_size_bit;
	masking_bit_num = (ALIGN(paddr & ~(sub_mask), page_size)) >> page_size_bit;

	for (loop_idx=0; loop_idx < aligned_size_unit; loop_idx++) {
		masking_bit = masking_bit | (0x1 << (masking_bit_num + loop_idx));
	}

	if (masking_bit == 0) {
		vlog_error("masking_bit 0x%08X is wrong calculated\n",masking_bit);
	}

	return masking_bit;
}

static int _vcodec_dpb_add_paddr(
							unsigned int paddr, unsigned int size)
{
	struct vcodec_dpb_paddr_node *node;
	unsigned int masking_bit;

	node = __vcodec_dpb_find_paddr(paddr);
	if (node == NULL) {
		node = vzalloc(sizeof(struct vcodec_dpb_paddr_node));
		node->masked_paddr = paddr & sub_mask;
		node->bit_table =  0x0;
		list_add_tail(&node->list, &paddr_list);
	}

	masking_bit = __vcodec_dpb_get_mask_bit(paddr,size);

	if (node->bit_table & masking_bit) {
		vlog_error("paddr 0x%08X is already ON\n",paddr);
		vlog_error("masking_bit : 0x%08X\n",masking_bit);
		vlog_error("masked_paddr: 0x%08X, masking_bit_table : 0x%08X\n",
			node->masked_paddr,node->bit_table);
		__vcodec_dpb_print_paddr();
		return -1;
	}
	else {
		node->bit_table  = node->bit_table | masking_bit;
	}

	vlog_print(VLOG_VBUF,"masked_paddr: 0x%08X, masking_bit_table : 0x%08X\n",
		node->masked_paddr,node->bit_table);
	return 0;
}

static int _vcodec_dpb_del_paddr(
							unsigned int paddr, unsigned int size)
{
	struct vcodec_dpb_paddr_node *node;
	unsigned int masking_bit;

	node = __vcodec_dpb_find_paddr(paddr);
	if (node == NULL) {
		vlog_error("paddr 0x%08X is not found\n",paddr);
		return -1;
	}

	masking_bit = __vcodec_dpb_get_mask_bit(paddr,size);

	if ((node->bit_table & masking_bit) == 0x0) {
		vlog_error("paddr 0x%08X is already OFF\n", paddr);
		vlog_error("masking_bit : 0x%08X\n", masking_bit);
		vlog_error("masked_paddr: 0x%08X, masking_bit_table : 0x%08X\n",
			node->masked_paddr, node->bit_table);
		return -1;
	}
	else {
		node->bit_table  = node->bit_table & ~masking_bit;
	}

	vlog_print(VLOG_VBUF,"masked_paddr: 0x%08X, masking_bit_table : 0x%08X\n",
		node->masked_paddr,node->bit_table);

	if (node->bit_table == 0) {
		list_del(&node->list);
		vfree(node);
	}

	return 0;
}

static struct vcodec_dpb_node * _vcodec_dpb_node_find(
							struct vcodec_dpb_inst* inst, unsigned int paddr)
{
	struct vcodec_dpb_node *node, *tmp;

	list_for_each_entry_safe(node, tmp, &inst->dpb_list, list) {
		if (node->paddr == paddr) {
			return node;
		}
	}

	return NULL;
}

bool vcodec_dpb_add(void *id,
						int fd,
						unsigned int *paddr,
						unsigned long **vaddr,
						unsigned int size)
{
	struct vcodec_dpb_inst *inst;
	struct vcodec_dpb_node *node;

	*paddr = 0x0;
	if (vaddr != NULL)
		*vaddr = 0x0;

	if (id == NULL) {
		vlog_error("vcodec_dpb_inst is NULL\n");
		return false;
	}

	inst = (struct vcodec_dpb_inst *)id;
	node = vzalloc(sizeof(struct vcodec_dpb_node));
	if (node == NULL) {
		vlog_error("vzalloc failed\n");
		return false;
	}

	mutex_lock(&dpb_lock);

	if (fd < 0) {
		node->ion_handle = ion_alloc(inst->ion_client,
									size,
									0x1000,
									(1<<ODIN_ION_SYSTEM_HEAP1),
									0,
									ODIN_SUBSYS_VSP);
		if (IS_ERR_OR_NULL(node->ion_handle)) {
			vlog_error("ion_alloc failed\n");
			goto err_dpb_malloc;
		}
	}
	else {
		/* reference count ++ */
		node->ion_handle = ion_import_dma_buf(inst->ion_client, fd);
		if (IS_ERR_OR_NULL(node->ion_handle)) {
			vlog_error("ion_import_dma_buf failed. fd:%d\n", fd);
			goto err_dpb_malloc;
		}

		if (odin_ion_map_iommu(node->ion_handle, ODIN_SUBSYS_VSP) < 0) {
			vlog_error("odin_ion_map_iommu failed. fd:%d\n", fd);
			goto err_dpb_ion;
		}
	}

	node->fd = fd;
	node->size = size;

	node->paddr = odin_ion_get_iova_of_buffer(node->ion_handle, ODIN_SUBSYS_VSP);
	if (node->paddr == 0) {
		vlog_error("odin_ion_get_iova_of_buffer failed. fd:%d\n", fd);
		if (fd < 0)
			goto err_dpb_ion;
		else
			goto err_dpb_malloc;
	}

/*
	if (_vcodec_dpb_add_paddr(node->paddr,
			odin_ion_get_buffer_size(node->ion_handle)) < 0) {
		vlog_error("paddr 0x%08X is allocated already.\n", node->paddr);
		if (fd < 0)
			goto err_dpb_ion;
		else
			goto err_dpb_malloc;
	}
*/
	if (vaddr != NULL) {
		node->vaddr = (unsigned long *)ion_map_kernel(inst->ion_client,
													node->ion_handle);
		if (node->vaddr == 0) {
			vlog_error("ion_map_kernel failed.\n");
			if (fd < 0)
				goto err_dpb_ion;
			else
				goto err_dpb_malloc;
		}
	}
	else {
		node->vaddr = 0;
	}

	vlog_print(VLOG_VBUF, "paddr:0x%08X, fd:%d \n", node->paddr, node->fd);

	list_add_tail(&node->list, &inst->dpb_list);

	*paddr = node->paddr;
	if (vaddr != NULL)
		*vaddr = node->vaddr;

	mutex_unlock(&dpb_lock);

	return true;

err_dpb_ion:
	ion_free(inst->ion_client, node->ion_handle);

err_dpb_malloc:
	mutex_unlock(&dpb_lock);

	vfree(node);
	return false;
}

void vcodec_dpb_del(void *id, int fd, unsigned int paddr)
{
	struct vcodec_dpb_inst *inst;
	struct vcodec_dpb_node *node = NULL;

	if (id == NULL) {
		vlog_error("vcodec_dpb_inst is NULL\n");
		return ;
	}

	mutex_lock(&dpb_lock);

	inst = (struct vcodec_dpb_inst *)id;

	node = _vcodec_dpb_node_find(inst, paddr);
	if (node == NULL) {
		vlog_error("can`t find dpb node. fd:%d paddr:0x%08X\n", fd, paddr);
		mutex_unlock(&dpb_lock);
		return;
	}

	if (node->ion_handle == NULL) {
		vlog_error("ion_handle is NULL. fd:%d paddr:0x%08X\n", fd, paddr);
		mutex_unlock(&dpb_lock);
		return;
	}
/*
	_vcodec_dpb_del_paddr(node->paddr, odin_ion_get_buffer_size(node->ion_handle));
*/
	if (node->ion_handle) {
		if (node->vaddr)
			ion_unmap_kernel(inst->ion_client, node->ion_handle);
		/* reference count -- */
		ion_free(inst->ion_client, node->ion_handle);
	}

	list_del(&node->list);
	mutex_unlock(&dpb_lock);
	vlog_print(VLOG_VBUF, "paddr:0x%08X, fd:%d \n", node->paddr, node->fd);
	vfree(node);
}

bool vcodec_dpb_addr_get(
			void *id, int fd, unsigned int *paddr, unsigned long **vaddr)
{
	struct vcodec_dpb_inst *inst;
	struct vcodec_dpb_node *node, *tmp;
	bool ret = false;

	*paddr = 0x0;
	if (vaddr != NULL)
		*vaddr = 0x0;

	if (id == NULL) {
		vlog_error("id is NULL\n");
		return false;
	}

	mutex_lock(&dpb_lock);

	inst = (struct vcodec_dpb_inst *)id;
	list_for_each_entry_safe(node, tmp, &inst->dpb_list, list) {
		if (node->fd == fd) {
			*paddr = node->paddr;
			if (vaddr != NULL)
				*vaddr = node->vaddr;

			ret = true;
			break;
		}
	}

	mutex_unlock(&dpb_lock);
	return ret;
}

int vcodec_dpb_fd_get(void *id, unsigned int paddr)
{
	struct vcodec_dpb_inst *inst;
	struct vcodec_dpb_node *node, *tmp;
	int fd = 0;

	if (id == NULL) {
		vlog_error("vcodec_dpb_inst is NULL\n");
		return 0;
	}

	mutex_lock(&dpb_lock);

	inst = (struct vcodec_dpb_inst *)id;
	if (IS_ERR_OR_NULL(inst)) {
		vlog_error("inst id is NULL\n");
		mutex_unlock(&dpb_lock);
		return -1;
	}

	list_for_each_entry_safe(node, tmp, &inst->dpb_list, list) {
		if (node->paddr == paddr) {
			fd = node->fd;
			break;
		}
	}

	if (&node->list == &inst->dpb_list) {
		vlog_error("no dpb\n");
		fd = -1;
	}

	/*vlog_print(VLOG_DPB, "paddr(0x%08X) to fd(%d)\n", paddr, fd);*/

	mutex_unlock(&dpb_lock);

	return fd;
}

void vcodec_dpb_clear(void *id)
{
	struct vcodec_dpb_inst *inst;
	struct vcodec_dpb_node *node, *tmp;

	if (id == NULL) {
		vlog_error("vcodec_dpb_inst is null\n");
		return ;
	}

	inst = (struct vcodec_dpb_inst *)id;
	list_for_each_entry_safe(node, tmp, &inst->dpb_list, list)
		vcodec_dpb_del(id, node->fd, node->paddr);

	return;
}

void* vcodec_dpb_open(void)
{
	int errno = 0;
	struct vcodec_dpb_inst *inst;
	char ion_client_name[80];

	inst = vzalloc(sizeof(struct vcodec_dpb_inst));
	if (inst == NULL) {
		vlog_error("vzalloc failed\n");
		errno = -ENOMEM;
		goto err_valloc;
	}

	mutex_lock(&dpb_lock);

	sprintf(ion_client_name, "vcodec dpb 0x%x", (unsigned int)inst);
	inst->ion_client = odin_ion_client_create(ion_client_name);
	if (IS_ERR_OR_NULL(inst->ion_client)) {
		vlog_error("odin_ion_client_create failed\n");
		errno = -EIO;
		goto err_odin_ion_client_create;
	}

	INIT_LIST_HEAD(&inst->dpb_list);
	list_add_tail(&inst->list, &inst_list);

	mutex_unlock(&dpb_lock);

	return inst;

err_odin_ion_client_create:
	mutex_unlock(&dpb_lock);
	vfree(inst);

err_valloc:
	return ERR_PTR(errno);
}

void vcodec_dpb_close(void *id)
{
	struct vcodec_dpb_inst *inst;

	if (id == NULL) {
		vlog_error("vcodec_dpb_inst is null\n");
		return ;
	}

	inst = (struct vcodec_dpb_inst *)id;

	vcodec_dpb_clear(id);

	mutex_lock(&dpb_lock);

	list_del(&inst->list);

	odin_ion_client_destroy(inst->ion_client);
	mutex_unlock(&dpb_lock);

	vfree(inst);

	return;
}

void vcodec_dpb_init(void)
{
	INIT_LIST_HEAD(&inst_list);
	INIT_LIST_HEAD(&paddr_list);
	mutex_init(&dpb_lock);
}

void vcodec_dpb_cleanup(void)
{
	mutex_destroy(&dpb_lock);
}

