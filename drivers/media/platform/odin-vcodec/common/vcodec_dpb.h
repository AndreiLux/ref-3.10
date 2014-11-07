/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
 * Seokhoon Kang <m4seokhoon.kang@lgepartner.com>
 * Inpyo Cho <inpyo.cho@lge.com>
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

#ifndef _VCODEC_DPB_H_
#define _VCODEC_DPB_H_

bool vcodec_dpb_add(void *id,
					int fd,
					unsigned int *paddr,
					unsigned long **vaddr,
					unsigned int size);
void vcodec_dpb_del(
			void *id, int fd, unsigned int paddr); /* pass fd or paddr */

bool vcodec_dpb_addr_get(
			void *id, int fd, unsigned int *paddr, unsigned long **vaddr);
int vcodec_dpb_fd_get(void *id, unsigned int paddr);
void vcodec_dpb_clear(void *id);
void* vcodec_dpb_open(void);
void vcodec_dpb_close(void *id);
void vcodec_dpb_init(void);
void vcodec_dpb_cleanup(void);

#endif /* #ifndef _VCODEC_DPB_H_ */
