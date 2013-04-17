#ifndef __LINUX_PKSM_H
#define __LINUX_PKSM_H
/*
 * Memory merging support.
 *
 * This code enables dynamic sharing of identical pages found in different
 * memory areas, even if they are not shared by fork().
 */

/* if !CONFIG_PKSM this file should not be compiled at all. */
#ifdef CONFIG_PKSM

extern unsigned long zero_pfn __read_mostly;
extern unsigned long pksm_zero_pfn __read_mostly;
extern struct page *empty_pksm_zero_page;

/* The number of page slots additionally sharing those nodes */
extern unsigned long ksm_pages_zero_sharing;

/* must be done before linked to mm */

struct rmap_item;

extern struct rmap_item *pksm_alloc_rmap_item(void);
extern void pksm_free_rmap_item(struct rmap_item *rmap_item);
extern int pksm_add_new_anon_page(struct page *page, struct rmap_item *rmap_item, struct anon_vma *anon_vma);
extern int pksm_del_anon_page(struct page *page);
extern void pksm_unmap_sharing_page(struct page *page, struct mm_struct *mm, unsigned long address);
extern void pksm_free_stable_anon(struct anon_vma *anon_vma);

#else

#endif /* !CONFIG_PKSM */
#endif /* __LINUX_PKSM_H */
