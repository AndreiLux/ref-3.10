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

#undef ENUM_BEGIN
#undef ENUM
#undef ENUM_END

#define ENUM_BEGIN(type)	static const char* type##_string[] = {
#define ENUM(name)		#name
#define ENUM_END(type)		};
#undef _VLOG_H_

#include <media/odin/vcodec/vlog.h>

#define ENUM_PREFIX_LEN strlen("VLOG_")

static void _vlog_sysfs_store_with_enum_string(const char *buf, size_t count)
{
	int i;
	char *on_off = 0;
	for (i=0; i<VLOG_MAX_NR; i++) {
		if (strncasecmp(buf, vlog_level_bit_string[i]+ENUM_PREFIX_LEN,
					strlen(vlog_level_bit_string[i])-ENUM_PREFIX_LEN) == 0) {
			on_off = (char *)buf + strlen(vlog_level_bit_string[i])-ENUM_PREFIX_LEN;

			while (*on_off == 0x20)
				on_off++;

			if (strncasecmp(on_off, "on", strlen("on")) == 0) {
				vlog_info("%s ON\n", vlog_level_bit_string[i]);
				vlog_enable_level(i);
			}
			else if (strncasecmp(on_off, "off", strlen("off")) == 0) {
				vlog_info("%s OFF\n", vlog_level_bit_string[i]);
				vlog_disable_level(i);
			}
			else
			{
				vlog_info("%s is unknown command\n", buf);
				break;
			}
		}
	}
}

static ssize_t _vlog_sysfs_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int user_level = 0;

	if (kstrtouint(buf, 16, &user_level) == 0)
		vlog_set_level(user_level);
	else
		_vlog_sysfs_store_with_enum_string(buf, count);

	return count;
}

static ssize_t _vlog_sysfs_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i;
	int len = 0;

	len += sprintf(buf, "0x%08x\n", vlog_level);
	for (i=0; i<VLOG_MAX_NR; i++) {
		if (vlog_level&(1<<i))
			len += sprintf(buf+len, "%s ON\n", vlog_level_bit_string[i]);
	}

	return len;
}

static DEVICE_ATTR(vlog, 0664, _vlog_sysfs_show, _vlog_sysfs_store);

void vlog_sysfs_init(struct device *device)
{
	int errno;

	vlog_info("vlog_sysfs_init start\n");
	if ((errno = device_create_file(device, &dev_attr_vlog)) < 0)
		vlog_error("device_create_file failed(%d)\n", errno);
}
