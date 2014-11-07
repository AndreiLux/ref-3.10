/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2014 by LG Electronics Inc.
 * Junglark Park <junglark.park@lgepartner.com>
 *
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

#ifndef __CABC_SYSFS_H__
#define __CABC_SYSFS_H__

struct device *cabc_device_register(struct device * dev, const char *dev_name);
void cabc_device_unregister(struct device *dev);

#endif /* __CABC_SYSFS_H__ */
