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

#ifndef _VPPU_IO_H_
#define _VPPU_IO_H_

#define VPPU_IOCTL_MAGIC 		'P'	/* 0x50 */
#define VPPU_IO_OPEN 			_IO(VPPU_IOCTL_MAGIC, 0)
#define VPPU_IO_CLOSE 			_IO(VPPU_IOCTL_MAGIC, 1)
#define VPPU_IO_ROTATE		 	_IO(VPPU_IOCTL_MAGIC, 2)
#define VPPU_IO_MIRROR		 	_IO(VPPU_IOCTL_MAGIC, 3)

enum vppu_io_usage
{
	VPPU_IO_USE_ROTATE,
	VPPU_IO_USE_MIRROR,
};

struct vppu_io_open_arg
{
	enum vppu_io_usage usage;
};

struct vppu_io_rotate
{
	int src_fd;
	int dst_fd;

	int width;
	int height;
	int angle;

	enum {
		VPPU_IO_IMAGE_FORMAT_YUV420_2P,
		VPPU_IO_IMAGE_FORMAT_YUV420_3P,
	} format;
};

struct vppu_io_mirror
{
	struct ion_handle *src_handle;
	struct ion_handle *dst_handle;
	int width;
	int height;
};

#endif /* #ifndef _VPPU_IO_H_ */
