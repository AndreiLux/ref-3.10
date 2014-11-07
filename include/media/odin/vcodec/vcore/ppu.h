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

#ifndef _VCORE_PPU_H_
#define _VCORE_PPU_H_

#define	VPPU_MAX_NUM_OF_DEVICE 		4

#define	VCORE_PPU_AU		0x20

#define	VCORE_TRUE			0
#define	VCORE_FALSE			(-1)

typedef int vcore_bool_t;

enum vcore_ppu_ret
{
	VCORE_PPU_FAIL = -1,
	VCORE_PPU_SUCCESS = 0,
	VCORE_PPU_RETRY,
};

enum vcore_ppu_image_format
{
	VCORE_PPU_IMAGE_FORMAT_YUV420_2P,
	VCORE_PPU_IMAGE_FORMAT_YUV420_3P,
};

struct vcore_ppu_report
{
	enum {
		VCORE_PPU_RESET,
		VCORE_PPU_DONE,
		VCORE_PPU_FEED,
	} hdr;

	union {
		enum {
			VCORE_PPU_REPORT_RESET_START,
			VCORE_PPU_REPORT_RESET_END,
		} reset;

		enum {
			VCORE_PPU_OK,
			VCORE_PPU_NO,
		} done;

		struct {
			/* none */
		} feed;

	} info;
};

struct vcore_ppu_ops
{
	enum vcore_ppu_ret (*open)(void **vcore_id,
				unsigned int workbuf_paddr, unsigned long *workbuf_vaddr,
				unsigned int workbuf_size,
				unsigned int width, unsigned int height,
				unsigned int fr_residual, unsigned int fr_divider,
				void *vppu_id,
				void (*vcore_ppu_report)(void *vppu_id,
									struct vcore_ppu_report *vcore_report));
	enum vcore_ppu_ret (*close)(void *vcore_id);
	enum vcore_ppu_ret (*rotate)(void *vcore_id,
								unsigned int src_addr, unsigned int dst_addr,
								unsigned int width, unsigned int height,
								int angle, enum vcore_ppu_image_format format);
	enum vcore_ppu_ret (*mirror)(void *vcore_id,
								unsigned int src_addr, unsigned int dst_addr,
								unsigned int width, unsigned int height,
								enum vcore_ppu_image_format format);

	void (*reset)(void *vcore_id);
};
#endif /* #ifndef _VCORE_PPU_H_ */

