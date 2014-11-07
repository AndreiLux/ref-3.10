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

#ifndef _VLOG_H_
#define _VLOG_H_

#ifdef __KERNEL__
#include <linux/device.h>
#endif /* #ifdef __KERNEL__ */

#ifndef ENUM_BEGIN
#define ENUM_BEGIN(type)	enum type {
#endif

#ifndef ENUM
#define ENUM(name)		name
#endif

#ifndef ENUM_END
#define ENUM_END(type)		};
#endif

ENUM_BEGIN(vlog_level_bit)
	ENUM(VLOG_ERROR),
	ENUM(VLOG_WARNING),
	ENUM(VLOG_INFO),
	ENUM(VLOG_TRACE),

	ENUM(VLOG_VPPU_0),
	ENUM(VLOG_VPPU_1),

	ENUM(VLOG_VCORE_INFO),
	ENUM(VLOG_VCORE_MONITOR),
	ENUM(VLOG_VCORE_2),

	ENUM(VLOG_VCORE_DEC),
	ENUM(VLOG_VCORE_ENC),
	ENUM(VLOG_VCORE_JPG),
	ENUM(VLOG_VCORE_PNG),
	ENUM(VLOG_VCORE_CHUNK),
	ENUM(VLOG_VCORE_ISR),
	ENUM(VLOG_VCORE_VDI),

	ENUM(VLOG_VDEC_BUF),
	ENUM(VLOG_VDEC_VES),
	ENUM(VLOG_VDEC_VDC),
	ENUM(VLOG_VDEC_MONITOR),
	ENUM(VLOG_VDEC_FLUSH),
	ENUM(VLOG_VDEC_CB),

	ENUM(VLOG_VENC_0),
	ENUM(VLOG_VENC_VEC),
	ENUM(VLOG_VENC_EPB),
	ENUM(VLOG_VENC_MONITOR),

	ENUM(VLOG_VEVENT),
	ENUM(VLOG_VBUF),
	ENUM(VLOG_LIBVLOG),
	ENUM(VLOG_DPB),
	ENUM(VLOG_MULTI_INSTANCE),

	ENUM(VLOG_RESERVE_1),

	ENUM(VLOG_MAX_NR),
ENUM_END(vlog_level_bit)

#ifdef __KERNEL__
extern unsigned int vlog_level;

#define vlog_error(text, args...) 	vlog_print(VLOG_ERROR, text, ##args)
#define vlog_warning(text, args...) 	vlog_print(VLOG_WARNING, text, ##args)
#define vlog_info(text, args...) 	vlog_print(VLOG_INFO, text, ##args)
#define vlog_trace(text, args...) 	vlog_print(VLOG_TRACE, text, ##args)

#define vlog_print(mylevel, text, args...)					\
	do {									\
		if((1<<mylevel) & vlog_level)					\
			printk("["#mylevel"]@%s()\t"text, __func__, ##args);	\
	} while(0)

#define vlog_print_libvlog_log(mylevel, str)					\
	do {									\
		if((1<<mylevel) & vlog_level)					\
			printk("["#mylevel"]%s", str);				\
	} while(0)

#define vlog_print_level(mylevel)							\
	do {									\
		if((1<<mylevel) & vlog_level)					\
			printk("["#mylevel"_TRACE]"" %s:%d\n", __func__, __LINE__);\
	} while(0)

extern unsigned int vlog_get_level(void);
extern void vlog_set_level(unsigned int level);

extern void vlog_enable_level(enum vlog_level_bit vlog_level_bit);
extern void vlog_disable_level(enum vlog_level_bit vlog_level_bit);

extern void vlog_init(struct device *device);
#endif /* #ifdef __KERNEL__ */

#endif /* #ifndef _VLOG_H_ */
