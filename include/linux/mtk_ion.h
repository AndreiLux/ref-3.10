/*
 * include/linux/ion.h
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

#ifndef _MTK_LINUX_ION_H
#define _MTK_LINUX_ION_H

/* #include <linux/ion.h> */

enum mtk_ion_heap_type {
	/* ION_HEAP_TYPE_MULTIMEDIA = ION_HEAP_TYPE_CUSTOM + 1, */
	ION_HEAP_TYPE_MULTIMEDIA = 10,
	ION_HEAP_TYPE_MM_CARVEOUT = 11,
};

#define ION_HEAP_MULTIMEDIA_MASK    (1 << ION_HEAP_TYPE_MULTIMEDIA)
#define ION_HEAP_MM_CARVEOUT_MASK    (1 << ION_HEAP_TYPE_MM_CARVEOUT)

#define ION_NUM_HEAP_IDS		sizeof(unsigned int) * 8

#endif				/* _MTK_LINUX_ION_H */
