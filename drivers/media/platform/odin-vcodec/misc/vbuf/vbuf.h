/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
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

#ifndef _VBUF_H_
#define _VBUF_H_

#include <linux/types.h>

struct vbuf
{
	unsigned long paddr;
	unsigned long *vaddr;
	size_t size;
};

int vbuf_init(void);
void vbuf_cleanup(void);

/**
 * vbuf_malloc()  - allocate video buffer
 * @size:	required buffer size
 *
 * allocate phycially continous memory via ion.
 * DO NOT USE THIS FUNCTINO IN ISR.
 */
struct vbuf *vbuf_malloc(const size_t size, bool vaddr_enable);

/**
 * vbuf_free() - free video buffer
 */
int vbuf_free(struct vbuf* vbuf);

/**
 * vbuf_validity - check validation of vbuf
 */
bool vbuf_validity(struct vbuf *vbuf);

struct ion_handle *vbuf_get_ion_handle(struct vbuf *vbuf);

#endif /* #ifndef _VBUF_H_ */
