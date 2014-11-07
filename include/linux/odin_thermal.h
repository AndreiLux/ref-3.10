/* include/linux/odin_thermal.c
 *
 * ODIN thermal sensor driver
 *
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
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

#ifndef __MACH_ODIN_THERMAL_H
#define __MACH_ODIN_THERMAL_H

#ifdef CONFIG_ODIN_THERMAL
extern int __init odin_tsens_driver_arch_init(void);
#else
static inline int __init odin_tsens_driver_arch_init(void)
{
	return -ENOSYS;
}
#endif

#ifdef CONFIG_ODIN_THERMAL
extern int odin_tsens_get_temp(u32 id, unsigned long *temp);
#else
static inline int odin_tsens_get_temp(u32 id, unsigned long *temp)
{
	return -ENOSYS;
}
#endif

#endif /* __MACH_ODIN_THERMAL_H */