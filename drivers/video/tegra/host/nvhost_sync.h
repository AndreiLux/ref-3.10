/*
 * drivers/video/tegra/host/nvhost_sync.h
 *
 * Tegra Graphics Host Syncpoint Integration to linux/sync Framework
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_SYNC_H
#define __NVHOST_SYNC_H

#include <linux/types.h>

#ifdef __KERNEL__

#include "../../../staging/android/sync.h"

struct nvhost_syncpt;
struct nvhost_sync_timeline;
struct nvhost_sync_pt;

struct nvhost_sync_timeline *nvhost_sync_timeline_create(
		struct nvhost_syncpt *sp,
		int id);

void nvhost_sync_pt_signal(struct nvhost_sync_pt *pt);

struct sync_fence *nvhost_sync_create_fence(
		struct nvhost_syncpt *sp,
		u32 *ids, u32 *vals,
		u32 num_pts,
		const char *name);

#endif /* __KERNEL __ */

#endif /* __NVHOST_SYNC_H */
