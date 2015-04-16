/*
 *
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef _LINUX_HISI_ION_H
#define _LINUX_HISI_ION_H

#include <linux/ion.h>

/**
 * These are the only ids that should be used for Ion heap ids.
 * The ids listed are the order in which allocation will be attempted
 * if specified. Don't swap the order of heap ids unless you know what
 * you are doing!
 * Id's are spaced by purpose to allow new Id's to be inserted in-between (for
 * possible fallbacks)
 */

enum ion_heap_ids {
	INVALID_HEAP_ID = -1,
	ION_SYSTEM_HEAP_ID = 0,
	ION_SYSTEM_CONTIG_HEAP_ID = 1,
	ION_GRALLOC_HEAP_ID = 2,
	ION_DMA_HEAP_ID = 3,
	ION_DMA_POOL_HEAP_ID = 4,
	ION_CPU_DRAW_HEAP_ID = 5,

	ION_HEAP_ID_RESERVED = 31 /* Bit reserved */
};


/**
 * Macro should be used with ion_heap_ids defined above.
 */
#define ION_HEAP(bit) (1 << (bit))

#define ION_VMALLOC_HEAP_NAME	"vmalloc"
#define ION_KMALLOC_HEAP_NAME	"kmalloc"
#define ION_GRALLOC_HEAP_NAME   "gralloc"


#define ION_SET_CACHED(__cache)		(__cache | ION_FLAG_CACHED)
#define ION_SET_UNCACHED(__cache)	(__cache & ~ION_FLAG_CACHED)

#define ION_IS_CACHED(__flags)	((__flags) & ION_FLAG_CACHED)

//struct used for get phys addr of contig heap
struct ion_phys_data {
	int fd_buffer;
	unsigned long phys;
	size_t size;
};

//user command add for additional use
enum ION_HISI_CUSTOM_CMD {
	ION_HISI_CUSTOM_PHYS
};


/**
 * hisi_ion_client_create() - create iommu mapping for the given handle
 * @heap_mask:	ion heap type mask
 * @name:	the client name
 * @return:     the client handle
 *
 * This function should called by high-level user in kernel. Before users
 * can access a buffer, they should get a client via calling this function.
 */
struct ion_client *
hisi_ion_client_create(const char *name);

/*k3 add to calc free memory*/
void hisi_ionsysinfo(struct sysinfo *si);

#endif
