#ifndef _K3V3_IOMMU_H_
#define _K3V3_IOMMU_H_

#include <linux/list.h>
#include <linux/iommu.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#define HISI_IOMMU_IOVA_START 0x40000000
#define HISI_IOMMU_IOVA_END 0xC0000000
#define HISI_IOVA_POOL_ALIGN SZ_256K

extern unsigned long hisi_reserved_smmu_phymem;
#define HISI_IOMMU_PGTABLE_SIZE 0x200000

#define HISI_IOMMU_PE_PA_MASK 0xFFFFF000
#define HISI_IOMMU_PE_P_MASK (1 << 11)
#define HISI_IOMMU_PE_V_MASK (1 << 10)


#ifdef CONFIG_K3V300_IOMMU

/**
 * hisi iommu interface
 */
int get_attach_value(struct iommu_domain *domain,
		unsigned long iova_start, unsigned long *ptb_base,
		unsigned long *iova_base);
int get_iova_range(struct iommu_domain *domain, unsigned long *iova_start,
		unsigned long *iova_end);

/**
 * hisi iommu domain interface
 */
size_t hisi_iommu_iova_size(void);
size_t hisi_iommu_iova_available(void);
void hisi_iommu_free_iova(unsigned long iova, size_t size);
unsigned long hisi_iommu_alloc_iova(size_t size, unsigned long align);
int hisi_iommu_map_range(unsigned long iova_start, struct scatterlist *sgl,
		unsigned long iova_size);
int hisi_iommu_unmap_range(unsigned long iova_start,
		unsigned long iova_size);

int hisi_iommu_map_domain(struct scatterlist *sg,
		struct iommu_map_format *format);
int hisi_iommu_unmap_domain(struct iommu_map_format *format);

phys_addr_t hisi_iommu_domain_iova_to_phys(unsigned long iova);

#ifdef CONFIG_K3V300_IOMMU_TEST
int init_iommt_test(void);
#else
static inline int init_iommt_test(void)
{
		    return 0;
}
#endif

#else

/**
 * hisi iommu interface
 */
static inline int get_attach_value(struct iommu_domain *domain,
		unsigned long iova_start, unsigned long *ptb_base,
		unsigned long *iova_base)
{
	return 0;
}

static inline int get_iova_range(struct iommu_domain *domain, unsigned long *iova_start,
		unsigned long *iova_end)
{
	return 0;
}

/**
 * hisi iommu domain interface
 */
static inline int hisi_iommu_map_domain(struct scatterlist *sg,
		struct iommu_map_format *format)
{
	return 0;
}

static inline int hisi_iommu_unmap_domain(struct iommu_map_format *format)
{
	return 0;
}

static inline phys_addr_t hisi_iommu_domain_iova_to_phys(unsigned long iova)
{
	return 0;
}

#endif /* CONFIG_K3V300_IOMMU */

#endif /* _K3V3_IOMMU_H_ */
