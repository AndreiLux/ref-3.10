/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
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

#include <asm/io.h>

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ion.h>
#include <linux/module.h>
#include <linux/odin_iommu.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>

#include <media/odin/vcodec/vlog.h>
#include "vbuf.h"

#define VBUF_MAGIC_NUM 0xdeadbeef

struct vbuf_node
{
	size_t size;
	struct vbuf addr;
	struct list_head list;
	struct ion_handle *ion_handle;
	unsigned int magic;
};

struct vbuf_desc
{
	struct ion_client *ion_client;

	spinlock_t lock_head;
	struct list_head head;
};
static struct vbuf_desc *d = NULL;

int vbuf_init(void)
{
	if (d) {
		vlog_print(VLOG_VBUF, "already initialized\n");
		return 0;
	}

	d = vzalloc(sizeof(struct vbuf_desc));
	if (d == NULL) {
		vlog_error("vzalloc failed\n");
		return -ENOMEM;
	}

	d->ion_client = odin_ion_client_create("vbuf");
	if (IS_ERR_OR_NULL(d->ion_client)) {
		vlog_error("odin_ion_client_create failed\n");
		return -EIO;
	}

	spin_lock_init(&d->lock_head);
	INIT_LIST_HEAD(&d->head);

	return 0;
}
EXPORT_SYMBOL(vbuf_init);

void vbuf_cleanup(void)
{
	if (!list_empty(&d->head)) {
		vlog_error("unfreed vbuf is exist.\n");
		vlog_error("memory leak will be occured\n");
	}

	odin_ion_client_destroy(d->ion_client);
	vfree(d);
}
EXPORT_SYMBOL(vbuf_cleanup);

static int _vbuf_alloc(struct vbuf_node *node, bool kmalloc, bool vaddr_enable)
{
	int errno = 0;

	if (kmalloc) {
		node->addr.vaddr = (unsigned long *)kzalloc(node->size, GFP_KERNEL);
		node->addr.paddr = (unsigned long)virt_to_phys(
											(volatile void*)node->addr.vaddr);
		node->addr.size = node->size;
	}
	else {
		node->ion_handle = ion_alloc(
								d->ion_client,
								node->size,
								SZ_2M,
								(1<<ODIN_ION_SYSTEM_HEAP1),
								0,
								ODIN_SUBSYS_VSP);
		if (IS_ERR_OR_NULL(node->ion_handle)) {
			vlog_error("ion_alloc failed\n");
			errno = -ENOMEM;
			return errno;
		}

		node->addr.paddr = (unsigned long)odin_ion_get_iova_of_buffer(
															node->ion_handle,
															ODIN_SUBSYS_VSP);
		if (node->addr.paddr == 0) {
			vlog_error("odin_ion_get_iova_of_buffer_failed\n");
			errno = -ENOMEM;
			goto err_odin_ion_get_iova_of_buffer_failed;
		}

		if (vaddr_enable) {
			node->addr.vaddr = (unsigned long *)ion_map_kernel(
														d->ion_client,
														node->ion_handle);
			if (node->addr.vaddr == 0) {
				vlog_error("ion_map_kernel failed.\n");
				errno = -ENOMEM;
				goto err_odin_ion_get_iova_of_buffer_failed;
			}
		}
		else
			node->addr.vaddr = NULL;

		node->addr.size = ALIGN(node->size, SZ_2M);
	}
	return 0;

err_odin_ion_get_iova_of_buffer_failed:
	ion_free(d->ion_client, node->ion_handle);
	return errno;
}

static void _vbuf_free(struct vbuf_node *node, bool kmalloc)
{
	if (kmalloc) {
		if (node->addr.vaddr)
			iounmap((void __iomem*)node->addr.vaddr);
		if (node->addr.paddr)
			kfree((void*)node->addr.paddr);
	}
	else {
		if (node->ion_handle) {
			if (node->addr.vaddr)
				ion_unmap_kernel(d->ion_client, node->ion_handle);
			ion_free(d->ion_client, node->ion_handle);
		}
	}
}

struct vbuf *vbuf_malloc(const size_t size, bool vaddr_enable)
{
	int errno = 0;
	struct vbuf_node *node;

	if (size <= 0) {
		vlog_error("size : %d\n", size);
		return ERR_PTR(-EINVAL);
	}

	node = vzalloc(sizeof(struct vbuf_node));
	if (node == NULL) {
		vlog_error("vzalloc failed\n");
		return ERR_PTR(-ENOMEM);
	}

	node->size = size;

	errno = _vbuf_alloc(node, false, vaddr_enable);
	if (errno < 0) {
		vlog_error("vbuf_alloc failed\n");
		goto err_vbuf_alloc;
	}

	node->magic = VBUF_MAGIC_NUM;

	spin_lock(&d->lock_head);
	list_add(&node->list, &d->head);
	spin_unlock(&d->lock_head);

	vlog_print(VLOG_VBUF, "alloc paddr:0x%08x size:0x%x\n",
				(unsigned int)node->addr.paddr, node->addr.size);

	return &node->addr;

err_vbuf_alloc:
	vfree(node);

	return ERR_PTR(errno);
}
EXPORT_SYMBOL(vbuf_malloc);

int vbuf_free(struct vbuf* vbuf)
{
	struct vbuf_node *node;

	if (IS_ERR_OR_NULL(vbuf)) {
		vlog_error("vbuf ptr 0x%08X is NULL\n", (unsigned int)vbuf);
		return -EINVAL;
	}

	node = container_of(vbuf, struct vbuf_node, addr);
	if (IS_ERR_OR_NULL(node)) {
		vlog_error("node ptr 0x%08X is NULL\n", (unsigned int)node);
		return -EINVAL;
	}
	if (node->magic != VBUF_MAGIC_NUM) {
		vlog_error("Invaild vbuf address\n");
		return -EINVAL;
	}

	spin_lock(&d->lock_head);
	list_del(&node->list);
	spin_unlock(&d->lock_head);

	vlog_print(VLOG_VBUF, "free paddr:0x%08x size:0x%x\n",
				(unsigned int)node->addr.paddr, node->addr.size);

	_vbuf_free(node, 0);

	vfree(node);

	return 0;
}
EXPORT_SYMBOL(vbuf_free);

bool vbuf_validity(struct vbuf *vbuf)
{
	struct vbuf_node *node;

	if (vbuf == NULL) {
		vlog_error("vbuf ptr is NULL\n");
		return false;
	}

	node = container_of(vbuf, struct vbuf_node, addr);
	if (node->magic != VBUF_MAGIC_NUM) {
		vlog_error("Invaild vbuf address\n");
		return false;
	}

	if (ALIGN(node->size, SZ_2M) != vbuf->size) {
		vlog_error("Invaild vbuf size\n");
		return false;
	}

	return true;
}
EXPORT_SYMBOL(vbuf_validity);

struct ion_handle* vbuf_get_ion_handle(struct vbuf *vbuf)
{
	struct vbuf_node *node;

	if (vbuf == NULL) {
		vlog_error("vbuf ptr is NULL\n");
		return ERR_PTR(-EINVAL);
	}

	node = container_of(vbuf, struct vbuf_node, addr);

	return node->ion_handle;
}

MODULE_AUTHOR("LGE");
