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

#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>

#include <media/odin/vcodec/vlog.h>

static ssize_t _vlog_libvlog_store(struct device *dev,
						struct device_attribute *attr, const char *buf, size_t count)
{
	vlog_print_libvlog_log(VLOG_LIBVLOG, buf);
	return count;
}

static ssize_t _vlog_libvlog_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR(vlog_libvlog, 0664, _vlog_libvlog_show, _vlog_libvlog_store);

void vlog_libvlog_init(struct device *device)
{
	int errno;
	vlog_info("vlog_vlog_init start\n");
	if ((errno = device_create_file(device, &dev_attr_vlog_libvlog)) < 0)
		vlog_error("device_create_file failed(%d)\n", errno);
}
