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

#include <linux/device.h>
#include <media/odin/vcodec/vlog.h>

#include "vlog_sysfs.h"
#include "vlog_libvlog.h"

 unsigned int vlog_level =      (1 << VLOG_ERROR) | \
						(1 << VLOG_INFO) | \
						(1 << VLOG_VDEC_MONITOR) | \
						(1 << VLOG_VENC_MONITOR) | \
						(1 << VLOG_VCORE_MONITOR);

unsigned int vlog_get_level(void)
{
	return vlog_level;
}

void vlog_set_level(unsigned int level)
{
	vlog_level = level;
}

void vlog_enable_level(enum vlog_level_bit vlog_level_bit)
{
	vlog_level |= 1<<vlog_level_bit;
}

void vlog_disable_level(enum vlog_level_bit vlog_level_bit)
{
	vlog_level &= ~(1<<vlog_level_bit);
}

void vlog_init(struct device *device)
{
	vlog_libvlog_init(device);
	vlog_sysfs_init(device);
}

EXPORT_SYMBOL(vlog_level);
