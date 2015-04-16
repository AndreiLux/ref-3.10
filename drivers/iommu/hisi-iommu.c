/*
 * Copyright (C) 20013-2013 hisilicon. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>

#include <linux/hisi-iommu.h>

#define ENABLE_CACHE_PGTBL 0
#define ENABLE_WC_PGTBL 0

#ifdef DEBUG
#define dbg(format, arg...)    \
	do {                 \
		printk(KERN_INFO "[iommu]"format, ##arg); \
	} while (0)
#else
#define dbg(format, arg...)
#endif

DEFINE_MUTEX(hisi_iommu_mutex);

struct __hisi_iommu_domain {
	struct iommu_domain *domain;
	unsigned long *pgtbl_addr; /* page table virtual address */
	size_t pgtbl_size;
	unsigned long iova_start;
	unsigned long iova_end;
};

static void dump_domain(struct iommu_domain *domain)
{
#ifdef DEBUG
	struct __hisi_iommu_domain *hisi_domain = domain->priv;

	dbg("domain: %p, pgtbl_addr: %p, pgtbl_size: 0x%x,"
		"iova_start: 0x%lx, iova_end: 0x%lx\n",
		hisi_domain->domain, hisi_domain->pgtbl_addr,
		hisi_domain->pgtbl_size, hisi_domain->iova_start,
		hisi_domain->iova_end);
#endif
}
static void __iomem * map_pgtbl(unsigned long pgtbl_addr, size_t pgtbl_size)
{
	dbg("map_pgtbl: pgtbl_addr: 0x%lx, pgtbl_size: 0x%x\n",
			pgtbl_addr, pgtbl_size);
#if ENABLE_CACHE_PGTBL
	dbg("page table cache\n");
	return ioremap_cached(pgtbl_addr, pgtbl_size);
#elif ENABLE_WC_PGTBL
	dbg("page table wc\n");
	return ioremap_wc(pgtbl_addr, pgtbl_size);
#else
	dbg("page table nocache\n");
	return ioremap_nocache(pgtbl_addr, pgtbl_size);
#endif
}

static void unmap_pgtbl(void __iomem *pgtbl_virt_addr)
{
	iounmap(pgtbl_virt_addr);
}

static inline void flush_pgtbl(struct __hisi_iommu_domain *hisi_domain,
	unsigned long iova, size_t size)
{
#if ENABLE_CACHE_PGTBL
	unsigned long *start = ((unsigned long *)hisi_domain->pgtbl_addr) +
			((iova - hisi_domain->iova_start) >> PAGE_SHIFT);
	unsigned long *end = start + (size >> PAGE_SHIFT);

	dbg("flush pftbl range: %p -- %p\n", start, end);
	dmac_flush_range((void *)start, (void *)end);

	outer_flush_range(hisi_reserved_smmu_phymem,
		  hisi_reserved_smmu_phymem + HISI_IOMMU_PGTABLE_SIZE);
#elif ENABLE_WC_PGTBL
	dbg("flush page table: dmb()\n");
	smp_wmb();
#endif
}

static int hisi_domain_init(struct iommu_domain *domain)
{
	struct __hisi_iommu_domain *hisi_domain;

	hisi_domain = kzalloc(sizeof(*hisi_domain), GFP_KERNEL);
	if (!hisi_domain) {
		printk(KERN_ERR "[%s]no mem!\n", __func__);
		return -ENOMEM;
	}

	/* set iova range */
	hisi_domain->iova_start = HISI_IOMMU_IOVA_START;
	hisi_domain->iova_end = HISI_IOMMU_IOVA_END;

	/* init the page table */
	hisi_domain->pgtbl_addr = map_pgtbl(hisi_reserved_smmu_phymem,
			HISI_IOMMU_PGTABLE_SIZE);
	if (!hisi_domain->pgtbl_addr) {
		kfree(hisi_domain);
		return -EINVAL;
	}

	hisi_domain->pgtbl_size = HISI_IOMMU_PGTABLE_SIZE;

	memset(hisi_domain->pgtbl_addr, 0, hisi_domain->pgtbl_size);
	flush_pgtbl(hisi_domain, hisi_domain->iova_start,
		(hisi_domain->iova_end - hisi_domain->iova_start));

	domain->priv = hisi_domain;
	hisi_domain->domain = domain;

	/* just for debug */
	dump_domain(domain);

	return 0;
}

static void hisi_domain_destroy(struct iommu_domain *domain)
{
	struct __hisi_iommu_domain *hisi_domain;

	mutex_lock(&hisi_iommu_mutex);

	hisi_domain = domain->priv;

	/* free the page table */
	unmap_pgtbl(hisi_domain->pgtbl_addr);

	/* free the hisi_domain */
	kfree(domain->priv);
	domain->priv = NULL;

	mutex_unlock(&hisi_iommu_mutex);
}

int get_attach_value(struct iommu_domain *domain,
		unsigned long iova_start, unsigned long *ptb_base,
		unsigned long *iova_base)
{
	phys_addr_t ptb_addr;
	struct __hisi_iommu_domain *hisi_domain = domain->priv;

	dbg("iova_start: 0x%lx, hisi_domain->iova_start: 0x%lx\n",
		iova_start, hisi_domain->iova_start);

	/* calculate page entry phys address */
	ptb_addr = hisi_reserved_smmu_phymem;
	ptb_addr += (((iova_start - hisi_domain->iova_start) >> PAGE_SHIFT) * 4);

	/* attach addr should align to 128Byte */
	if (ptb_addr & (SZ_128 - 1)) {
		iova_start -= (((ptb_addr & (SZ_128 - 1)) >> 2) * SZ_4K);
		ptb_addr &= ~(SZ_128 - 1);
	}

	/* output the result */
	*ptb_base = ptb_addr;
	*iova_base = iova_start;

	return 0;
}
EXPORT_SYMBOL_GPL(get_attach_value);


int get_iova_range(struct iommu_domain *domain, unsigned long *iova_start,
		unsigned long *iova_end)
{
	struct __hisi_iommu_domain *hisi_domain = domain->priv;
	*iova_start = hisi_domain->iova_start;
	*iova_end = hisi_domain->iova_end;
	return 0;
}
EXPORT_SYMBOL_GPL(get_iova_range);

static inline void set_pg_entry(volatile unsigned long *pe_addr,
		phys_addr_t phys_addr)
{
	*pe_addr = (phys_addr & HISI_IOMMU_PE_PA_MASK) |
			(HISI_IOMMU_PE_V_MASK | HISI_IOMMU_PE_P_MASK);
}

static inline void clear_pg_entry(volatile unsigned long *pe_addr)
{
	*pe_addr = 0;
}

static int __hisi_map(struct __hisi_iommu_domain *hisi_domain,
	   unsigned long iova, phys_addr_t paddr, size_t size)
{
	unsigned long *pgtable;
	unsigned long offset = 0;
	size_t maped_size = 0;

	/* get the pgtable address */
	pgtable = (unsigned long *)hisi_domain->pgtbl_addr;
	if (!pgtable) {
		printk(KERN_ERR "[%s]error: page table not ready!\n", __func__);
		return -EINVAL;
	}

	/* get the offset of first page entry */
	offset = (iova - hisi_domain->iova_start) >> order_base_2(SZ_4K);

	/* write page address into the table */
	for (maped_size = 0; maped_size < size; maped_size += SZ_4K, offset++) {
		set_pg_entry((pgtable + offset), (paddr + maped_size));
	}

	dbg("[%s]iova: 0x%lx, paddr: 0x%x, size: 0x%x\n", __func__,
			iova, paddr, size);

	return 0;
}

static int hisi_map(struct iommu_domain *domain, unsigned long iova,
	   phys_addr_t paddr, size_t size, int prot)
{
	struct __hisi_iommu_domain *hisi_domain;
	int ret = 0;

	mutex_lock(&hisi_iommu_mutex);

	/* check domain */
	hisi_domain = domain->priv;
	if (!hisi_domain) {
		printk(KERN_ERR "hisi_domain NULL!\n");
		ret = -EINVAL;
		goto error;
	}

	if ((iova < hisi_domain->iova_start) || (size > hisi_domain->iova_end)) {
		printk(KERN_ERR "iova out of range!\n");
		ret = -EINVAL;
		goto error;
	}


	if (!IS_ALIGNED(paddr, SZ_4K) || !IS_ALIGNED(iova, SZ_4K)) {
		printk(KERN_ERR "addr is not aligend to SZ_4K!\n");
		ret = -EINVAL;
		goto error;
	}

	ret = __hisi_map(hisi_domain, iova, paddr, size);
	if (ret) {
		goto error;
	}

	flush_pgtbl(hisi_domain, iova, size);

error:
	mutex_unlock(&hisi_iommu_mutex);
	return ret;
}

static int __hisi_unmap(struct __hisi_iommu_domain *hisi_domain,
	   unsigned long iova, size_t size)
{
	unsigned long *pgtable;
	unsigned long offset = 0;
	size_t maped_size = 0;

	/* get the pgtable address */
	pgtable = (unsigned long *)hisi_domain->pgtbl_addr;
	if (!pgtable) {
		printk(KERN_ERR "[%s]error: page table not ready!\n", __func__);
		return -EINVAL;
	}

	/* get the offset of first page entry */
	offset = (iova - hisi_domain->iova_start) >> order_base_2(SZ_4K);

	/* write page address into the table */
	for (maped_size = 0; maped_size < size; maped_size += SZ_4K, offset++) {
		clear_pg_entry(pgtable + offset);
	}
	dbg("[%s]iova: 0x%lx, size: 0x%x\n", __func__, iova, size);

	return 0;
}

static size_t hisi_unmap(struct iommu_domain *domain, unsigned long iova,
	     size_t size)
{
	struct __hisi_iommu_domain *hisi_domain;

	mutex_lock(&hisi_iommu_mutex);

	hisi_domain = domain->priv;
	if (!hisi_domain) {
		printk(KERN_ERR "[%s]domain is not ready!\n", __func__);
		goto error;
	}

	if (!IS_ALIGNED(iova, SZ_4K)) {
		printk(KERN_ERR "iova is not aligned to SZ_4K!\n");
		goto error;
	}

	if (__hisi_unmap(hisi_domain, iova, size)) {
		goto error;
	}

	flush_pgtbl(hisi_domain, iova, size);

	mutex_unlock(&hisi_iommu_mutex);
	return size;

error:
	mutex_unlock(&hisi_iommu_mutex);
	return 0;
}

static dma_addr_t get_phys_addr(struct scatterlist *sg)
{
	/*
	 * Try sg_dma_address first so that we can
	 * map carveout regions that do not have a
	 * struct page associated with them.
	 */
	dma_addr_t dma_addr = sg_dma_address(sg);
	if (dma_addr == 0)
		dma_addr = sg_phys(sg);
	return dma_addr;
}

static int hisi_map_range(struct iommu_domain *domain, unsigned long iova,
	    struct scatterlist *sg, size_t size, int prot)
{
	struct __hisi_iommu_domain *hisi_domain;
	phys_addr_t phys_addr;
	size_t maped_size = 0;
	int ret = 0;

	dbg("[%s]+\n", __func__);

	dbg("domain: %p, iova: 0x%lx, sg: %p, size: 0x%x\n",
		domain, iova, sg, size);

	mutex_lock(&hisi_iommu_mutex);

	hisi_domain = domain->priv;
	if (!hisi_domain) {
		ret = -EINVAL;
		goto error;
	}

	if (!IS_ALIGNED(iova, SZ_4K)) {
		printk(KERN_ERR "iova is not aligned to SZ_4K!\n");
		ret = -EINVAL;
		goto error;
	}

	while (sg) {
		dbg("to map a sg: iova: 0x%lx, sg: %p, maped size: 0x%x\n",
			iova, sg, maped_size);

		phys_addr = get_phys_addr(sg);
		dbg("phys_addr: 0x%x\n", phys_addr);

		ret = __hisi_map(hisi_domain, iova, phys_addr, sg->length);
		if (ret) {
			printk(KERN_ERR "__hisi_map failed!\n");
			break;
		}

		iova += ALIGN(sg->length, SZ_4K);
		maped_size += ALIGN(sg->length, SZ_4K);
		if (maped_size >= size) {
			break;
		}

		sg = sg_next(sg);
	};

	flush_pgtbl(hisi_domain, iova, size);

error:
	mutex_unlock(&hisi_iommu_mutex);

	dbg("[%s]-\n", __func__);

	return ret;
}
static size_t hisi_unmap_range(struct iommu_domain *domain, unsigned long iova,
	      size_t size)
{
	struct __hisi_iommu_domain *hisi_domain;
	int ret = 0;

	dbg("[%s]+\n", __func__);
	dbg("domain: %p, iova: 0x%lx, size: 0x%x\n", domain, iova, size);

	mutex_lock(&hisi_iommu_mutex);

	hisi_domain = domain->priv;
	if (!hisi_domain) {
		printk(KERN_ERR "hisi_domain NULL!\n");
		goto error;
	}

	if (!IS_ALIGNED(iova, SZ_4K)) {
		printk(KERN_ERR "iova is not aligned to 4K!\n");
		goto error;
	}

	ret = __hisi_unmap(hisi_domain, iova, size);
	if (ret) {
		printk(KERN_ERR "__hisi_unmap failed!\n");
		goto error;
	}

	flush_pgtbl(hisi_domain, iova, size);

	mutex_unlock(&hisi_iommu_mutex);
	dbg("[%s]-\n", __func__);
	return size;
error:
	mutex_unlock(&hisi_iommu_mutex);
	return 0;
}

static size_t hisi_map_tile_row(struct iommu_domain *domain, unsigned long iova,
	   size_t size, struct scatterlist *sg, size_t sg_offset)
{
	struct __hisi_iommu_domain *hisi_domain = domain->priv;
	unsigned long map_size;
	unsigned long phys_addr;
	unsigned long mapped_size = 0;
	int ret;

	map_size = ((sg->length - sg_offset) < size) ?
			(sg->length - sg_offset) : size;

	phys_addr = (unsigned long)get_phys_addr(sg) + sg_offset;

	while (mapped_size < size) {

		map_size = size - mapped_size;
		if (map_size > (sg->length - sg_offset)) {
			map_size = (sg->length - sg_offset);
		}

		phys_addr = (unsigned long)get_phys_addr(sg) + sg_offset;

		ret = __hisi_map(hisi_domain, iova + mapped_size,
				phys_addr, map_size);
		if (ret) {
			printk(KERN_ERR "[%s]__hisi_map failed!\n", __func__);
			break;
		}

		mapped_size += map_size;
		sg_offset = 0;
		sg = sg_next(sg);
		if (!sg) {
			break;
		}
	}

	return mapped_size;
}

/**
 *
 */
int hisi_map_tile(struct iommu_domain *domain, unsigned long iova,
	   struct scatterlist *sg, size_t size, int prot,
	   struct tile_format *format)
{
	struct __hisi_iommu_domain *hisi_domain;
	struct scatterlist *sg_node;
	unsigned int rows, row;
	unsigned int sg_mapped_size;
	unsigned int mapped_size;
	unsigned int size_virt, size_phys; /* virt len and phys len in one row */
	unsigned int sg_offset; /* mapped len of the first sg node */
	unsigned int phys_length;
	int ret = 0;

	dbg("[%s]+\n", __func__);

	mutex_lock(&hisi_iommu_mutex);

	hisi_domain = domain->priv;
	if (!hisi_domain) {
		ret = -EINVAL;
		goto error;
	}

	/* calculate the whole len of phys mem */
	for (phys_length = 0, sg_node = sg; sg_node; sg_node= sg_next(sg_node)) {
		phys_length += ALIGN(sg_node->length, SZ_4K);
	}
	dbg("phys_length: 0x%x\n", phys_length);

	/* calculate rows number */
	rows = (phys_length / SZ_4K) / format->phys_page_line;
	dbg("rows: 0x%x\n", rows);

	sg_mapped_size = 0;
	sg_offset = 0;
	sg_node = sg;
	size_phys = (format->phys_page_line * SZ_4K);
	size_virt = (format->virt_page_line * SZ_4K);

	/* map row by row */
	for (row = 0; row < rows; row++) {

		/* populate the phys addr */
		while (sg_node) {
			if (sg_mapped_size >= sg_node->length) {
				sg_mapped_size -= sg_node->length;
				sg_node = sg_next(sg_node);
			} else {
				sg_offset = sg_mapped_size;
				break;
			}
		}
		dbg("sg_offset: 0x%x\n", sg_offset);

		if (!sg_node) {
			printk("phys mem is not enough!\n");
			ret = -EINVAL;
			break;
		}

		/* excute map operation */
		mapped_size = hisi_map_tile_row(domain, iova + (size_virt * row),
			    size_phys, sg_node, sg_offset);
		if (mapped_size != size_phys) {
			WARN(1, "hisi_map_tile_row failed!\n");
			ret = -EINVAL;
			break;
		}

		dbg("mapped_size: 0x%x\n", mapped_size);

		sg_mapped_size = mapped_size + sg_offset;
		sg_offset = 0;
	};

	flush_pgtbl(hisi_domain, iova, size);

error:
	mutex_unlock(&hisi_iommu_mutex);

	dbg("[%s]-\n", __func__);

	return ret;
}

size_t hisi_unmap_tile(struct iommu_domain *domain, unsigned long iova,
	     size_t size)
{
	return hisi_unmap_range(domain, iova, size);
}


static phys_addr_t hisi_iova_to_phys(struct iommu_domain *domain,
			    dma_addr_t iova)
{
	struct __hisi_iommu_domain *hisi_domain;
	phys_addr_t phys_addr = 0;
	unsigned long *pgtable;
	unsigned long pg_index;
	int ret;

	mutex_lock(&hisi_iommu_mutex);

	hisi_domain = domain->priv;
	if (!hisi_domain) {
		ret = -EINVAL;
		goto error;
	}

	pgtable = hisi_domain->pgtbl_addr;
	if (!pgtable) {
		ret = -EINVAL;
		goto error;
	}

	if (iova < hisi_domain->iova_start || iova > hisi_domain->iova_end) {
		printk(KERN_ERR "iova 0x%x is not belongs to this domain!\n", iova);
		ret = -EINVAL;
		goto error;
	}

	pg_index = (iova - hisi_domain->iova_start) >> order_base_2(SZ_4K);

	phys_addr = *(pgtable + pg_index) + (iova & (SZ_4K - 1));

error:
	mutex_unlock(&hisi_iommu_mutex);

	dbg("[%s]iova: 0x%x, phys: 0x%x\n", __func__, iova, phys_addr);
	return phys_addr;
}

static int hisi_domain_has_cap(struct iommu_domain *domain,
		      unsigned long cap)
{
	return 0;
}

static struct iommu_ops hisi_iommu_ops = {
	.domain_init = hisi_domain_init,
	.domain_destroy = hisi_domain_destroy,

	.map = hisi_map,
	.unmap = hisi_unmap,

	.map_range = hisi_map_range,
	.unmap_range = hisi_unmap_range,

	.map_tile = hisi_map_tile,
	.unmap_tile = hisi_unmap_tile,

	.iova_to_phys = hisi_iova_to_phys,
	.domain_has_cap = hisi_domain_has_cap,

	.pgsize_bitmap = SZ_4K,
};

static int __init hisi_iommu_init(void)
{
	int ret;

	ret = bus_set_iommu(&platform_bus_type, &hisi_iommu_ops);
	if (ret) {
		printk(KERN_ERR "[%s]bus_set_iommu failed!\n", __func__);
		return ret;
	}

	dbg("[%s]hisi iommu init done!\n", __func__);

	return 0;
}

subsys_initcall(hisi_iommu_init);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("l00196665");
