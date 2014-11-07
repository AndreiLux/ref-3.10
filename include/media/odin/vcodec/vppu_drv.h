/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
 * Seokhoon.Kang <m4seokhoon.kang@lgepartner.com>
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

#ifndef _VPPU_DRV_H_
#define _VPPU_DRV_H_

#include "media/odin/vcodec/vcore/ppu.h"

enum vppu_image_format
{
	VPPU_IMAGE_FORMAT_YUV420_2P,
	VPPU_IMAGE_FORMAT_YUV420_3P,
};

enum vppu_report_info
{
	VPPU_OK,
	VPPU_FAIL,
};

struct vppu_report
{
	enum vppu_report_info hdr;
};

enum vppu_report_info vppu_rotate(void *ch, unsigned int src_paddr,
	unsigned int dst_paddr, int width, int height, int angle, enum vppu_image_format format);

void *vppu_open(bool without_fail, unsigned int width, unsigned int height,
	unsigned int fr_residual, unsigned int fr_divider,
		void *cb_arg, void (*cb_isr)(void *cb_arg, struct vppu_report *report));
void vppu_close(void *ch);

int vppu_init(void* (*get_vcore)(struct vcore_ppu_ops *ops,
							bool without_fail,
							unsigned int width, unsigned int height,
							unsigned int fr_residual, unsigned int fr_divider),
			void (*put_vcore)(void *select_id));
int vppu_cleanup(void);

#endif

