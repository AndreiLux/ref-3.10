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

#ifndef __HW_SEMAPHORE_H__
#define __HW_SEMAPHORE_H__

#include <linux/errno.h>
#include <linux/list.h>

enum {
	HW_SEMA_TRY_MODE,    /* if semaphore locked, will not wait for release*/
	HW_SEMA_WAITING_MODE, /* if semaphore locked, will wait */
};

struct hw_sema_element;
struct hw_sema_uelement {
	struct hw_sema_element *entry;
	struct list_head ulist;
	unsigned int waiting;
};

#ifdef CONFIG_SND_SOC_ODIN_HW_SEMAPHORE_DRIVER
extern int hw_sema_put_sema(struct device *dev, struct hw_sema_uelement *uelem);
extern struct hw_sema_uelement *hw_sema_get_sema_name(struct device *dev,
						      const char *name);
extern int hw_sema_set_sema_type(struct device *dev,
				 struct hw_sema_uelement *uelem, int type);
extern int hw_sema_get_sema_type(struct hw_sema_uelement *uelem);
extern int hw_sema_lock_status(struct hw_sema_uelement *uelem);
extern int hw_sema_up(struct hw_sema_uelement *uelem);
extern int hw_sema_down(struct hw_sema_uelement *uelem);
extern int hw_sema_down_interruptible(struct hw_sema_uelement *uelem);
extern int hw_sema_down_killable(struct hw_sema_uelement *uelem);
extern unsigned long hw_sema_down_timeout(struct hw_sema_uelement *uelem,
					  unsigned long timeout);

#else
static inline int hw_sema_put_sema(struct device *dev,
				   struct hw_sema_uelement *uelem)
{
	return -ENOSYS;
}
static inline struct hw_sema_uelement *hw_sema_get_sema_name(struct device *dev,
							     const char *name)
{
	return NULL;
}
static inline int hw_sema_set_sema_type(struct device *dev,
					struct hw_sema_uelement *uelem,
					int type)
{
	return -ENOSYS;
}
static inline int hw_sema_get_sema_type(struct hw_sema_uelement *uelem)
{
	return -ENOSYS;
}
static inline int hw_sema_lock_status(struct hw_sema_uelement *uelem)
{
	return -ENOSYS;
}
static inline int hw_sema_up(struct hw_sema_uelement *uelem)
{
	return -ENOSYS;
}
static inline int hw_sema_down(struct hw_sema_uelement *uelem)
{
	return -ENOSYS;
}
static inline int hw_sema_down_interruptible(struct hw_sema_uelement *uelem)
{
	return -ENOSYS;
}
static inline int hw_sema_down_killable(struct hw_sema_uelement *uelem)
{
	return -ENOSYS;
}
static inline unsigned long hw_sema_down_timeout(struct hw_sema_uelement *uelem,
						 unsigned long timeout)
{
	return 0;
}
#endif /* CONFIG_SND_SOC_ODIN_HW_SEMAPHORE_DRIVER */
#endif /* __HW_SEMAPHORE_H__ */
