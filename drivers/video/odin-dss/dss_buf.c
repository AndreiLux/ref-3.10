/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Younghyun Jo <younghyun.jo@lge.com>
 * Youngki Lyu <youngki.lyu@lge.com>
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

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/ion.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/odin_iommu.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "dss_buf.h"

//#define DEBUG

struct dss_buf_desc
{
	struct ion_client *ion_client;

	struct list_head dss_buf_head;
	struct mutex dss_buf_head_lock;
};
static struct dss_buf_desc *d = NULL;

struct dss_buf
{
	int fd;
	unsigned long paddr; /* actually, it is iova */
	struct ion_handle *ion_handle;
	size_t size;

	struct list_head list; /* member of dss_buf_head */

	struct {
		long long int real_paddr; /* real physical address  */
		bool map;
	} debug;
};

void dss_buf_list_print(void)
{
	struct dss_buf *dss_buf = NULL, *tmp = NULL;
	int nr_unmapped = 0;

	pr_info("--- dss_buf list ---\n");

	mutex_lock(&d->dss_buf_head_lock);

	list_for_each_entry_safe(dss_buf, tmp, &d->dss_buf_head, list){
#ifdef DEBUG
		pr_info("\t\tfd:%d paddr:0x%08X size:%d %s\n", dss_buf->fd, dss_buf->paddr, dss_buf->size, dss_buf->debug.map ? "map" : "alloc");
#endif
		nr_unmapped++;
	}
	pr_info("dss_buf num of unmap:%d\n", nr_unmapped);

	mutex_unlock(&d->dss_buf_head_lock);
}

static int _dss_buf_free(struct dss_buf *dss_buf)
{
	int fd = -1;

	if (dss_buf == NULL) {
		pr_err("instance ptr is NULL\n");
		return -1;
	}

	fd = dss_buf->fd;

	mutex_lock(&d->dss_buf_head_lock);

	list_del(&dss_buf->list);

	mutex_unlock(&d->dss_buf_head_lock);

	ion_free(d->ion_client, dss_buf->ion_handle);
	vfree(dss_buf);

	return fd ;
}

unsigned long dss_buf_rot_paddr_get(const unsigned long paddr)
{
	struct dss_buf *dss_buf= NULL, *tmp = NULL;

	list_for_each_entry_safe(dss_buf, tmp, &d->dss_buf_head, list)
		if (dss_buf->paddr == paddr)
			 return odin_ion_get_iova_of_buffer(dss_buf->ion_handle, ODIN_SUBSYS_VSP);

	return 0;
}

unsigned long dss_buf_alloc(const size_t size)
{
	struct dss_buf *dss_buf = NULL;
	
	dss_buf = vzalloc(sizeof(struct dss_buf));
	if (dss_buf == NULL) {
		pr_err("vzalloc failed. size:%d\n", sizeof(struct dss_buf));
		goto err_vzalloc;
	}

	dss_buf->ion_handle = ion_alloc(d->ion_client, ALIGN(size, SZ_2M), SZ_2M,
			(1<<ODIN_ION_SYSTEM_HEAP1), 0, ODIN_SUBSYS_VSP | ODIN_SUBSYS_DSS);
	if (IS_ERR_OR_NULL(dss_buf->ion_handle))
	{
		pr_err("ion_alloc failed. errno:%d size:%d\n", (int)dss_buf->ion_handle, size);
		goto err_ion_alloc;
	}


	dss_buf->size = size;
	dss_buf->paddr = odin_ion_get_iova_of_buffer(dss_buf->ion_handle, ODIN_SUBSYS_DSS);
	dss_buf->fd = -1;

	dss_buf->debug.real_paddr = odin_ion_get_first_pa_of_buffer(dss_buf->ion_handle);
	dss_buf->debug.map = false;

	mutex_lock(&d->dss_buf_head_lock);

	list_add(&dss_buf->list, &d->dss_buf_head);

	mutex_unlock(&d->dss_buf_head_lock);

	//pr_info("alloc paddr:%08X\n", dss_buf->paddr);
	return dss_buf->paddr;

err_odin_ion_map_iommu:
	ion_free(d->ion_client, dss_buf->ion_handle);

err_ion_alloc:
	vfree(dss_buf);

err_vzalloc:
	pr_err("can`t alloc dss buf\n");
	return 0xffffffff;
}

int dss_buf_free(const void *fd_or_paddr)
{
	struct dss_buf *dss_buf = NULL, *tmp = NULL;

	list_for_each_entry_safe(dss_buf, tmp, &d->dss_buf_head, list)
		if (dss_buf->paddr == (unsigned long)(fd_or_paddr) || dss_buf->fd == (int)(fd_or_paddr))
			return _dss_buf_free(dss_buf);

	pr_err("can`t find dss_buf .fd_or_paddr:%d, 0x%08X\n", (int)fd_or_paddr, (int)fd_or_paddr);
	dss_buf_list_print();

	return -1;
}

unsigned long dss_buf_map(const int fd)
{
	struct dss_buf *dss_buf = NULL;
	
	dss_buf = vzalloc(sizeof(struct dss_buf));
	if (dss_buf == NULL) {
		pr_err("vzalloc failed. size:%d\n", sizeof(struct dss_buf));
		goto err_vzalloc;
	}

	dss_buf->ion_handle = ion_import_dma_buf(d->ion_client, fd);
	if (IS_ERR_OR_NULL(dss_buf->ion_handle)) {
		pr_err("ion_import_dma_buf failed. fd:%d\n", fd);
		goto err_ion_alloc;
	}

	if (odin_ion_map_iommu(dss_buf->ion_handle, ODIN_SUBSYS_VSP | ODIN_SUBSYS_DSS) < 0) {
		pr_err("odin_ion_map_iommu failed\n");
		goto err_odin_ion_map_iommu;
	}

	dss_buf->size = odin_ion_get_buffer_size(dss_buf->ion_handle);
	dss_buf->paddr = odin_ion_get_iova_of_buffer(dss_buf->ion_handle, ODIN_SUBSYS_DSS);
	dss_buf->fd = fd;

	dss_buf->debug.real_paddr = odin_ion_get_first_pa_of_buffer(dss_buf->ion_handle);
	dss_buf->debug.map = true;

	mutex_lock(&d->dss_buf_head_lock);
	list_add(&dss_buf->list, &d->dss_buf_head);
	mutex_unlock(&d->dss_buf_head_lock);

	//pr_info("map paddr:%08X\n", dss_buf->paddr);
	return dss_buf->paddr;

err_odin_ion_map_iommu:
	ion_free(d->ion_client, dss_buf->ion_handle);

err_ion_alloc:
	vfree(dss_buf);

err_vzalloc:
	pr_err("can`t map dss buf. fd:%d\n", fd);
	return 0xffffffff;
}

int dss_buf_unmap(const unsigned long paddr)
{
	struct dss_buf *dss_buf = NULL, *tmp = NULL;

	list_for_each_entry_safe(dss_buf, tmp, &d->dss_buf_head, list)
		if (dss_buf->paddr == paddr)
			return _dss_buf_free(dss_buf);

	pr_err("can`t find dss_buf .paddr:0x%08X\n", (int)paddr);
	dss_buf_list_print();

	return -1;
}

void dss_buf_init(void)
{
	if (d) {
		pr_warn("dss_buf is already initialized\n");
		return;
	}

	d = vzalloc(sizeof(struct dss_buf_desc));
	if (d == NULL) {
		pr_err("vzalloc failed. size:%d\n", sizeof(struct dss_buf_desc));
		goto err_vzalloc;
	}

	d->ion_client = odin_ion_client_create("dss_buf");
	if (IS_ERR_OR_NULL(d->ion_client)) {
		pr_err("odin_ion_client_create failed. errno:%d\n", (int)d->ion_client);
		goto err_odin_ion_client_create;
	}

	INIT_LIST_HEAD(&d->dss_buf_head);
	mutex_init(&d->dss_buf_head_lock);

	//_dss_buf_unit_test();

	return;

err_odin_ion_client_create:
	if (d) {
		vfree(d);
		d = NULL;
	}

err_vzalloc:
	pr_err("dss_buf init failed\n");

	return ;
}

void dss_buf_cleanup(void)
{
	if (d == NULL) {
		pr_warn("dss_buf is nothing to cleanup\n");
		return;
	}

	if (d->ion_client) {
		odin_ion_client_destroy(d->ion_client);
		d->ion_client = NULL;
	}

	if (d) {
		vfree(d);
		d = NULL;
	}

	return ;
}
