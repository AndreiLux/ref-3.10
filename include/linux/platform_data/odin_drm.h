/*
 * include/linux/platform_data/odin_drm.h
 *
 * Copyright:	(C) 2014 LG Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ODIN_DRM_H__
#define __ODIN_DRM_H__

#include <linux/list.h>

typedef enum {
	DRM_CPB         = 0x1,
	DRM_DPB         = 0x2,
	DRM_ROTATOR_BUF = 0x4,
	WFD_CSC_BUF     = 0x8,
	WFD_EPB         = 0x10,
	WFD_RTP_BUF     = 0x20,
	HDMI_EXT_FB     = 0x40,
	HDMI_PRIM_FB    = 0x80,
	RESERVED,
} buffer_type_t;

struct odin_drm_msg {
	unsigned int buffer;
	unsigned int len;
	unsigned int type;
};

struct buf_info {
	unsigned int pa;
	unsigned int offset;
};

struct neon_rotator_secure_arg_t {
	struct buf_info src[2];
	struct buf_info dst[2];
	unsigned int rot_degree;
	unsigned int width;
	unsigned int height;
	unsigned int whole_height;
};

#define DRM_IOC_MAGIC		'R'

#define DRM_BUFFER_ALLOC	_IOWR(DRM_IOC_MAGIC, 0, struct odin_drm_msg)
#define DRM_BUFFER_FREE		_IOWR(DRM_IOC_MAGIC, 1, struct odin_drm_msg)
#define DRM_BUFFER_MAP		_IOWR(DRM_IOC_MAGIC, 2, struct odin_drm_msg)
#define DRM_BUFFER_UNMAP	_IOWR(DRM_IOC_MAGIC, 3, struct odin_drm_msg)

#define DRM_PLAY_START		_IO(DRM_IOC_MAGIC, 5)
#define DRM_PLAY_TERMINATE	_IO(DRM_IOC_MAGIC, 6)

#define DRM_SECURE_ROTATE	_IOWR(DRM_IOC_MAGIC, 11, struct neon_rotator_secure_arg_t)
#define DRM_ROTATOR_MAP		_IOWR(DRM_IOC_MAGIC, 12, struct neon_rotator_secure_arg_t)
#define DRM_ROTATOR_UNMAP	_IOWR(DRM_IOC_MAGIC, 13, struct neon_rotator_secure_arg_t)

struct odin_tz_ion {
	unsigned int paddr;
	unsigned int length;
	unsigned int type;
};

struct tz_ion_list {
	struct list_head list;
	struct odin_tz_ion tzion;
};

struct odin_drm_ion {
	struct tz_ion_list tzlist;
	unsigned ion_cnt;
};

struct odin_drm_ion* tzmem_init_list(void);
void tzmem_add_entry(struct odin_drm_ion *ion,
		     phys_addr_t paddr,
		     unsigned int length,
		     unsigned int type);

void tzmem_delete_list(struct odin_drm_ion *ion);
int tzmem_map_buffer(struct odin_drm_ion *pion);
int tzmem_unmap_buffer(struct odin_drm_ion *pion);

#endif
