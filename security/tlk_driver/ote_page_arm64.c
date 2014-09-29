/*
 * Copyright (C) 2014 Google, Inc.
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

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/pagemap.h>

#include "ote_protocol.h"

static pte_t *_get_pte(unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;

	struct mm_struct *mm = current->mm;

	pgd = pgd_offset(mm, addr);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return NULL;

	pud = pud_offset(pgd, addr);
	if (pud_none(*pud) || pud_bad(*pud))
		return NULL;

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return NULL;

	return pte_offset_map(pmd, addr);
}

int te_fill_page_info(struct te_oper_param_page_info *pg_inf,
		      unsigned long start, struct page *page)
{
	pte_t *ppte;
	uint64_t val;
	uint64_t pte;
	uint attr_shift;

	if (!page)
		return -1;

	ppte = _get_pte(start);
	if (!ppte)
		return -1;

	pte = pte_val(*ppte);

	if ((pte & 0x0000FFFFFFFFF000) !=
	    (page_to_phys(page) & 0x0000FFFFFFFFF000))
		return -1;

	asm (" mrs %0, mair_el1\n" : "=&r" (val));
	attr_shift = ((pte >> 2) & 0x7) << 3;
	pg_inf->attr = (pte & 0x0000FFFFFFFFFFFF)
		     | (((val >> attr_shift) & 0xFF) << 48);
	return 0;
}

