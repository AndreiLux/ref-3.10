/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __DRV2603_VIBRATOR_H__
#define __DRV2603_VIBRATOR_H__



#define DRV2603_DEBUG

#ifdef DRV2603_DEBUG
#define DRV2603_DBG_INFO(fmt, args...) \
	pr_info("[DRV2603]: %s: " fmt, __func__, ##args)
#define DRV2603_DBG(fmt, args...) \
	pr_info("[DRV2603]: %s: " fmt, __func__, ##args)
#define debug_printf(fmt, arg...) printk(fmt,##arg)
#define debug_puts(fmt) printk(fmt)
#define DRV2603_ERR(fmt, args...) \
	pr_err("[DRV2603]: %s: " fmt, __func__, ##args)
#else
#define DRV2603_DBG_INFO(fmt, args...)  do { } while(0)
#define debug_printf(fmt, arg...) do { } while(0)
#define debug_puts(fmt) do { } while(0)
#define DRV2603_ERR(fmt, args...) \
	pr_err("[DRV2603]: %s: " fmt, __func__, ##args)
#endif


#endif /* _DRV2603_VIBRATOR_H__ */









