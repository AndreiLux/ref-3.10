/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2014 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __HW_SEMA_DRV_H__
#define __HW_SEMA_DRV_H__

#include <linux/errno.h>

struct hw_semaphore_ops {
	int (*hw_sema_lock)(unsigned int nr);
	int (*hw_sema_unlock)(unsigned int nr);
	int (*hw_sema_read)(unsigned int nr);
};

struct hw_sema_init_data {
	const char *hw_name;
	const char **hw_sema_list;
	unsigned int hw_sema_cnt;
	struct hw_semaphore_ops *ops;
};

#ifdef CONFIG_SND_SOC_ODIN_HW_SEMAPHORE_DRIVER
extern int hw_sema_register(struct device *dev, struct hw_sema_init_data *data);
extern int hw_sema_deregister(struct device *dev);
extern int hw_semaphore_interupt_callback(struct device *dev, unsigned int id);
#else
static inline int hw_sema_register(struct device *dev,
						struct hw_sema_init_data *data)
{
	return -ENOSYS;
}
static inline int hw_sema_deregister(struct device *dev)
{
	return -ENOSYS;
}
static inline int hw_semaphore_interupt_callback(struct device *dev,
							unsigned int id)
{
	return -ENOSYS;
}
#endif /* CONFIG_SND_SOC_ODIN_HW_SEMAPHORE_DRIVER */

#endif /* __HW_SEMA_DRV_H__ */


