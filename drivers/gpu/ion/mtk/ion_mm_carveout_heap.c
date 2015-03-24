/*
 * drivers/gpu/ion/ion_carveout_heap.c
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
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/mtk_ion.h>
#include <linux/mm.h>
#include <mach/m4u.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include "ion_priv.h"

#include <asm/mach/map.h>

#if ION_SUPPORT_TEE

#include <trustzone/kree/system.h>
#include <trustzone/kree/mem.h>
#include <trustzone/tz_cross/ta_mem.h>


bool tee_mem_can_be_used = false;
ion_phys_addr_t mm_carveout_heap_base;
size_t mm_carveout_heap_size;
#endif

struct ion_mm_carveout_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
	size_t size;
};

struct ion_mm_buffer_info {
	struct mutex lock;
	int eModuleID;
	unsigned int security;
	unsigned int coherent;
	void *pVA;
	ion_phys_addr_t priv_phys;
	unsigned int MVA;
	ion_mm_buf_debug_info_t dbg_info;
};

static int ion_mm_carveout_heap_debug_show(struct ion_heap *heap, struct seq_file *s, void *unused);

ion_phys_addr_t ion_mm_carveout_allocate(struct ion_heap *heap,
				      unsigned long size,
				      unsigned long align)
{
	struct ion_mm_carveout_heap *mm_carveout_heap =
		container_of(heap, struct ion_mm_carveout_heap, heap);
	unsigned long offset = gen_pool_alloc(mm_carveout_heap->pool, size);

	IONMSG("ion_mm_carveout_allocate heap = %x, size = 0x%x, offset = 0x%x.\n", heap, size, offset);

	if (!offset) {
		IONMSG("ion_mm_carveout alloc fail! size=0x%x, free=0x%x\n", size,
				gen_pool_avail(mm_carveout_heap->pool));
		return ION_CARVEOUT_ALLOCATE_FAIL;
	}

	return offset;
}

void ion_mm_carveout_free(struct ion_heap *heap, ion_phys_addr_t addr,
		       unsigned long size)
{
	struct ion_mm_carveout_heap *mm_carveout_heap =
		container_of(heap, struct ion_mm_carveout_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;

	gen_pool_free(mm_carveout_heap->pool, addr, size);
}

static int ion_mm_carveout_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  ion_phys_addr_t *addr, size_t *len)
{
	struct ion_mm_buffer_info *pBufferInfo = (struct ion_mm_buffer_info *) buffer->priv_virt;
	if (!pBufferInfo) {
		IONMSG("[ion_mm_carveout_heap_phys]: Error. Invalid buffer.\n");
		return -EFAULT;	/* Invalid buffer */
	}
	if (pBufferInfo->eModuleID == -1) {
		IONMSG("[ion_mm_carveout_heap_phys]: Error. Buffer not configured.\n");
		return -EFAULT;	/* Buffer not configured. */
	}
	IONMSG("ion_mm_carveout_heap_phys eModuleID = 0x%x, len = 0x%x, pa = 0x%x.\n",
			pBufferInfo->eModuleID, buffer->size, pBufferInfo->priv_phys);

	/* Allocate MVA */

	mutex_lock(&(pBufferInfo->lock));
	if (pBufferInfo->MVA == 0) {
		int ret =
		    m4u_alloc_mva_fix(pBufferInfo->eModuleID, buffer->priv_phys, buffer->size,
				     pBufferInfo->security, pBufferInfo->coherent,
				     &pBufferInfo->MVA);
		/*int ret = m4u_fill_linear_pagetable(pBufferInfo->priv_phys, buffer->size);

		pBufferInfo->MVA = pBufferInfo->priv_phys; */

		if (ret < 0) {
			mutex_unlock(&(pBufferInfo->lock));
			pBufferInfo->MVA = 0;
			IONMSG("[ion_mm_carveout_heap_phys]: Error. Allocate MVA failed.\n");
			return -EFAULT;
		}
	}
	*addr = (ion_phys_addr_t) pBufferInfo->MVA;	/* MVA address */
	mutex_unlock(&(pBufferInfo->lock));
	*len = buffer->size;

	IONMSG("ion_mm_carveout_heap_phys MVA = 0x%x, len = 0x%x.\n", pBufferInfo->MVA, buffer->size);
	return 0;
}

#if ION_SUPPORT_TEE

int ion_stop_secure_chunk_mem(void)
{
	KREE_SESSION_HANDLE mem_session;
	TZ_RESULT result;

	KREE_CreateSession(TZ_TA_MEM_UUID, &mem_session);
	result = KREE_StopSecurechunkmemSvc(mem_session, (void **)&mm_carveout_heap_base, &mm_carveout_heap_size);
	IONMSG("ion_stop_secure_chunk_mem result = 0x%x, base = 0x%x, size = 0x%x.\n",
		result, mm_carveout_heap_base, mm_carveout_heap_size);
	KREE_CloseSession(mem_session);

	if (result == TZ_RESULT_SUCCESS)
		tee_mem_can_be_used = true;
	else
		tee_mem_can_be_used = false;

	return result;
}

int ion_start_secure_chunk_mem(void)
{
	KREE_SESSION_HANDLE mem_session;
	TZ_RESULT result;

	struct rb_node *n;

	struct ion_device *dev = g_ion_device;

	/*query mm_carveout_heap buffer exists or not?*/
	mutex_lock(&dev->buffer_lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buffer = rb_entry(n, struct ion_buffer, node);
		if (buffer->heap->id == ION_HEAP_TYPE_MM_CARVEOUT) {
			mutex_unlock(&dev->buffer_lock);
			return -1;
		}
	}
	mutex_unlock(&dev->buffer_lock);

	KREE_CreateSession(TZ_TA_MEM_UUID, &mem_session);
	result = KREE_StartSecurechunkmemSvc(mem_session, (uint32_t)mm_carveout_heap_base, mm_carveout_heap_size);
	IONMSG("ion_start_secure_chunk_mem result = 0x%x, base = 0x%x, size = 0x%x.\n",
		(unsigned int)result, (unsigned int)mm_carveout_heap_base, (unsigned int)mm_carveout_heap_size);
	KREE_CloseSession(mem_session);

	if (result == TZ_RESULT_SUCCESS)
		tee_mem_can_be_used = false;
	else
		tee_mem_can_be_used = true;

	return result;
}

static TZ_RESULT ion_query_secure_chunk_mem(void **base, uint32_t *size)
{
	KREE_SESSION_HANDLE mem_session;
	TZ_RESULT result;

	KREE_CreateSession(TZ_TA_MEM_UUID, &mem_session);
	result = KREE_QuerySecurechunkmem(mem_session, base, size);
	IONMSG("ion_query_secure_chunk_mem result = 0x%x, base = 0x%x, size = 0x%x.\n",
		result, base, size);
	KREE_CloseSession(mem_session);

	return result;
}

#endif



static int ion_mm_carveout_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	ion_phys_addr_t tmp_priv_phys;
	struct ion_mm_buffer_info *pBufferInfo = NULL;

#if ION_SUPPORT_TEE
	if (!tee_mem_can_be_used)
		return -EFAULT;
#endif

	tmp_priv_phys = ion_mm_carveout_allocate(heap, size, align);

	/* create MM buffer info for it */
	pBufferInfo = kzalloc(sizeof(struct ion_mm_buffer_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pBufferInfo)) {
		IONMSG("[ion_mm_carveout_heap_allocate]: Error. Allocate ion_buffer failed.\n");
		return -EFAULT;
	}

	pBufferInfo->priv_phys = tmp_priv_phys;
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

	IONMSG("ion_mm_carveout_heap_allocate heap = 0x%x, buffer->priv_phys = 0x%x.\n", heap, pBufferInfo->priv_phys);

	return pBufferInfo->priv_phys == ION_CARVEOUT_ALLOCATE_FAIL ? -ENOMEM : 0;
}

static void ion_mm_carveout_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_mm_buffer_info *pBufferInfo = (struct ion_mm_buffer_info *) buffer->priv_virt;
	ion_mm_carveout_free(heap, pBufferInfo->priv_phys, buffer->size);
	pBufferInfo->priv_phys = ION_CARVEOUT_ALLOCATE_FAIL;

	IONMSG("ion_mm_carveout_heap_free size = 0x%x.\n", sizeof(struct ion_mm_buffer_info));

	kfree(pBufferInfo);
}

struct sg_table *ion_mm_carveout_heap_map_dma(struct ion_heap *heap,
					      struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;
	struct ion_mm_buffer_info *pBufferInfo = (struct ion_mm_buffer_info *) buffer->priv_virt;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}
	sg_set_page(table->sgl, phys_to_page(pBufferInfo->priv_phys), buffer->size,
		    0);
	return table;
}

void ion_mm_carveout_heap_unmap_dma(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	sg_free_table(buffer->sg_table);
}

void *ion_mm_carveout_heap_map_kernel(struct ion_heap *heap,
				   struct ion_buffer *buffer)
{
	void *ret;
	int mtype = MT_MEMORY_NONCACHED;
	struct ion_mm_buffer_info *pBufferInfo = (struct ion_mm_buffer_info *) buffer->priv_virt;

	if (buffer->flags & ION_FLAG_CACHED)
		mtype = MT_MEMORY;

	IONMSG("ion_mm_carveout_heap_map_kernel pBufferInfo->priv_phys = 0x%x, buffer->size = 0x%x.\n",
			pBufferInfo->priv_phys, buffer->size);

	ret = __arm_ioremap(pBufferInfo->priv_phys, buffer->size,
			      mtype);
	if (ret == NULL)
		return ERR_PTR(-ENOMEM);

	return ret;
}

void ion_mm_carveout_heap_unmap_kernel(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	__arm_iounmap(buffer->vaddr);
	buffer->vaddr = NULL;
	return;
}

int ion_mm_carveout_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			       struct vm_area_struct *vma)
{
	struct ion_mm_buffer_info *pBufferInfo = (struct ion_mm_buffer_info *) buffer->priv_virt;
	return remap_pfn_range(vma, vma->vm_start,
			       __phys_to_pfn(pBufferInfo->priv_phys) + vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       (vma->vm_page_prot));
			       /*pgprot_noncached(vma->vm_page_prot));*/
}

void ion_mm_carveout_heap_free_bufferInfo(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	struct ion_mm_buffer_info *pBufferInfo = (struct ion_mm_buffer_info *) buffer->priv_virt;

	IONMSG("ion_mm_carveout_heap_free_bufferInfo size = 0x%x.\n", sizeof(struct ion_mm_buffer_info));

	if (pBufferInfo) {
		mutex_lock(&(pBufferInfo->lock));
		if ((pBufferInfo->eModuleID != -1) && (pBufferInfo->MVA)) {
			m4u_dealloc_mva_sg(pBufferInfo->eModuleID, table, buffer->size,
					   pBufferInfo->MVA);
		}
		mutex_unlock(&(pBufferInfo->lock));
	}
}

void ion_mm_carveout_heap_add_freelist(struct ion_buffer *buffer)
{
	ion_mm_carveout_heap_free_bufferInfo(buffer);
}

static struct ion_heap_ops mm_carveout_heap_ops = {
	.allocate = ion_mm_carveout_heap_allocate,
	.free = ion_mm_carveout_heap_free,
	.phys = ion_mm_carveout_heap_phys,
	.map_dma = ion_mm_carveout_heap_map_dma,
	.unmap_dma = ion_mm_carveout_heap_unmap_dma,
	.map_user = ion_heap_map_user,
	/*.map_kernel = ion_carveout_heap_map_kernel,
	.unmap_kernel = ion_carveout_heap_unmap_kernel,*/
	.map_kernel = ion_mm_carveout_heap_map_kernel,
	.unmap_kernel = ion_mm_carveout_heap_unmap_kernel,
	/*.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,*/
	.add_freelist = ion_mm_carveout_heap_add_freelist,
};

#define ION_PRINT_LOG_OR_SEQ(seq_file, fmt, args...) \
		do {\
			if (seq_file)\
				seq_printf(seq_file, fmt, ##args);\
			else\
				printk(fmt, ##args);\
		} while (0)

static void ion_mm_carveout_chunk_show(struct gen_pool *pool,
				struct gen_pool_chunk *chunk,
				void *data)
{
	int order, nlongs, nbits, i;

	order = pool->min_alloc_order;
	nbits = (chunk->end_addr - chunk->start_addr) >> order;
	nlongs = BITS_TO_LONGS(nbits);

	ION_PRINT_LOG_OR_SEQ(NULL, "phys_addr=0x%x nlongs = 0x%x, order = 0x%x, start_addr = 0x%x, end_addr = 0x%x.\n",
	(unsigned int)chunk->phys_addr, nlongs, order, (unsigned int)chunk->start_addr, (unsigned int)chunk->end_addr);

	for (i = 0; i < nlongs; i++)
		ION_PRINT_LOG_OR_SEQ(NULL, "0x%x ", (unsigned int)chunk->bits[i]);

	ION_PRINT_LOG_OR_SEQ(NULL, "\n");
}

static int ion_mm_carveout_heap_debug_show(struct ion_heap *heap, struct seq_file *s, void *unused)
{
	struct ion_mm_carveout_heap *mm_carveout_heap =
			container_of(heap, struct ion_mm_carveout_heap, heap);
	size_t size_avail, total_size;

	total_size = gen_pool_size(mm_carveout_heap->pool);
	size_avail = gen_pool_avail(mm_carveout_heap->pool);

	ION_PRINT_LOG_OR_SEQ(NULL, "total_size=0x%x, free=0x%x, base=0x%x\n",
			total_size, size_avail, (unsigned int)mm_carveout_heap->base);

	gen_pool_for_each_chunk(mm_carveout_heap->pool, ion_mm_carveout_chunk_show, s);

	return 0;
}

struct ion_heap *ion_mm_carveout_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_mm_carveout_heap *mm_carveout_heap;

#if ION_SUPPORT_TEE
	if (TZ_RESULT_SUCCESS == ion_query_secure_chunk_mem((void **)&mm_carveout_heap_base, &mm_carveout_heap_size)) {

		IONMSG("ion_query_secure_chunk_mem success! base = 0x%x, size = 0x%x.\n",
			mm_carveout_heap_base, mm_carveout_heap_size);
		heap_data->base = (ion_phys_addr_t)mm_carveout_heap_base;
		heap_data->size = mm_carveout_heap_size;
	} else {
		IONMSG("ion_query_secure_chunk_mem fail!");
		heap_data->base = 0;
		heap_data->size = 0;

		mm_carveout_heap_base = 0;
		mm_carveout_heap_size = 0;
	}
#endif

	IONMSG("ion_mm_carveout_heap_create base = 0x%x, size = 0x%x.\n", heap_data->base, heap_data->size);

	mm_carveout_heap = kzalloc(sizeof(struct ion_mm_carveout_heap), GFP_KERNEL);
	if (!mm_carveout_heap)
		return ERR_PTR(-ENOMEM);

	mm_carveout_heap->pool = gen_pool_create(12, -1);
	if (!mm_carveout_heap->pool) {
		kfree(mm_carveout_heap);
		return ERR_PTR(-ENOMEM);
	}
	mm_carveout_heap->base = heap_data->base;
	mm_carveout_heap->size = heap_data->size;
	gen_pool_add(mm_carveout_heap->pool, mm_carveout_heap->base, heap_data->size,
		     -1);
	mm_carveout_heap->heap.ops = &mm_carveout_heap_ops;
	mm_carveout_heap->heap.type = ION_HEAP_TYPE_MM_CARVEOUT;
	mm_carveout_heap->heap.flags = 0;
	mm_carveout_heap->heap.debug_show = ion_mm_carveout_heap_debug_show;

	return &mm_carveout_heap->heap;
}

void ion_mm_carveout_heap_destroy(struct ion_heap *heap)
{
	struct ion_mm_carveout_heap *mm_carveout_heap =
	     container_of(heap, struct  ion_mm_carveout_heap, heap);

	gen_pool_destroy(mm_carveout_heap->pool);
	kfree(mm_carveout_heap);
	mm_carveout_heap = NULL;
}

int ion_mm_carveout_copy_dbg_info(ion_mm_buf_debug_info_t *src, ion_mm_buf_debug_info_t *dst)
{
	int i;
	dst->handle = src->handle;
	for (i = 0; i < ION_MM_DBG_NAME_LEN; i++)
		dst->dbg_name[i] = src->dbg_name[i];

	dst->dbg_name[ION_MM_DBG_NAME_LEN - 1] = '\0';
	dst->value1 = src->value1;
	dst->value2 = src->value2;
	dst->value3 = src->value3;
	dst->value4 = src->value4;

	return 0;

}

long ion_mm_carveout_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg, int from_kernel)
{
	ion_mm_data_t Param;
	long ret = 0;
	/* char dbgstr[256]; */
	unsigned long ret_copy;
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
			if ((int)buffer->heap->type == ION_HEAP_TYPE_MM_CARVEOUT) {
				struct ion_mm_buffer_info *pBufferInfo = buffer->priv_virt;
				mutex_lock(&(pBufferInfo->lock));
				if (pBufferInfo->MVA == 0) {
					pBufferInfo->eModuleID =
					    Param.config_buffer_param.eModuleID;
					pBufferInfo->security = Param.config_buffer_param.security;
					pBufferInfo->coherent = Param.config_buffer_param.coherent;
					IONMSG("[in_mm_carveout_heap] config buffer eModuleID = %d.\n",
						pBufferInfo->eModuleID);
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
				char *errMsg = "Cannot set dbg buffer that is not from mm carveout heap heap.";
				IONMSG("[ion_mm_heap]: Error. %s.\n", errMsg);
				ret = 0;
			}
		} else {
			IONMSG("[ion_mm_carveout_heap]: Error config buf with invalid handle.\n");
			ret = -EFAULT;
		}
		break;

#if ION_SUPPORT_TEE
	case ION_MM_CARVEOUT_START_SECURE_CHUNK_MEM:
	{
		int result;
		result = ion_start_secure_chunk_mem();
		if (!result) {
			IONMSG("[ion_mm_carveout_heap]: Info start secure chunk mem success.\n");
			ret = 0;
		} else {
			IONMSG("[ion_mm_carveout_heap]: Error start secure chunk mem fail.\n");
			ret = -EFAULT;
		}
	}
	break;
	case ION_MM_CARVEOUT_STOP_SECURE_CHUNK_MEM:
	{
		int result;
		result = ion_stop_secure_chunk_mem();
		if (!result) {
			IONMSG("[ion_mm_carveout_heap]: Info. stop secure chunk mem success.\n");
			ret = 0;
		} else {
			IONMSG("[ion_mm_carveout_heap]: Error stop secure chunk mem fail.\n");
			ret = -EFAULT;
		}
	}
	break;
#endif
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
				if ((int)buffer->heap->type == ION_HEAP_TYPE_MM_CARVEOUT) {
					struct ion_mm_buffer_info *pBufferInfo = buffer->priv_virt;
					mutex_lock(&(pBufferInfo->lock));
					ion_mm_carveout_copy_dbg_info(&(Param.buf_debug_info_param),
							     &(pBufferInfo->dbg_info));
					mutex_unlock(&(pBufferInfo->lock));
				} else {
					char *errMsg = "Cannot set dbg buffer that is not from mm carveout heap heap.";
					IONMSG("[ion_mm_heap]: Error. %s.\n", errMsg);
					ret = -EFAULT;
				}
			} else {
				IONMSG
				("[ion_mm_carveout_heap]: Error. set dbg buffer with invalid handle.\n");
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
				if ((int)buffer->heap->type == ION_HEAP_TYPE_MM_CARVEOUT) {
					struct ion_mm_buffer_info *pBufferInfo = buffer->priv_virt;
					mutex_lock(&(pBufferInfo->lock));
					ion_mm_carveout_copy_dbg_info(&(pBufferInfo->dbg_info),
							     &(Param.buf_debug_info_param));
					mutex_unlock(&(pBufferInfo->lock));
				} else {
					char *errMsg = "Cannot set dbg buffer that is not from mm carveout heap heap.";
					IONMSG("[ion_mm_carveout_heap]: Error. %s.\n", errMsg);
					ret = -EFAULT;
				}
			} else {
				IONMSG
				("[ion_mm_carveout_heap]: Error. set dbg buffer with invalid handle.\n");
				ret = -EFAULT;
			}
		}
		break;

	default:
		IONMSG("[ion_mm_carveout_heap]: Error. Invalid command.\n");
		ret = -EFAULT;
	}
	if (from_kernel)
		*(ion_mm_data_t *) arg = Param;
	else
		ret_copy = copy_to_user((void __user *)arg, &Param, sizeof(ion_mm_data_t));
	return ret;
}
