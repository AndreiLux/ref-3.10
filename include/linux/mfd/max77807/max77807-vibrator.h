/*
 * linux/power/max77807-vibrator.h
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MAX77807_VIBRATOR_H
#define __LINUX_MAX77807_VIBRATOR_H

enum {
	MOTOR_ERM,
	MOTOR_LRA,
};

enum {
	INPUT_MPWM,
	INPUT_IPWM,
};

enum {
	PDIV_32,
	PDIV_64,
	PDIV_128,
	PDIV_256,
};

struct max77807_vib_platform_data {
	int mode;
	int htype;
	int pdiv;
};
#endif

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __MAX77807_VIBRATOR_H__
#define __MAX77807_VIBRATOR_H__



/* #define MAX77807_DEBUG */

#ifdef MAX77807_DEBUG
#define MAX77807_DBG_INFO(fmt, args...) \
	pr_info("[MAX77807-VIB]: %s: " fmt, __func__, ##args)
#define MAX77807_DBG(fmt, args...) \
	pr_info("[MAX77807-VIB]: %s: " fmt, __func__, ##args)
#define debug_printf(fmt, arg...) printk(fmt,##arg)
#define debug_puts(fmt) printk(fmt)
#define MAX77807_ERR(fmt, args...) \
	pr_err("[MAX77807-VIB]: %s: " fmt, __func__, ##args)
#else
#define MAX77807_DBG(fmt, args...) do { } while(0)
#define MAX77807_DBG_INFO(fmt, args...)  do { } while(0)
#define debug_printf(fmt, arg...) do { } while(0)
#define debug_puts(fmt) do { } while(0)
#define MAX77807_ERR(fmt, args...) \
	pr_err("[MAX77807-VIB]: %s: " fmt, __func__, ##args)
#endif


#endif /* _MAX77807_VIBRATOR_H__ */
