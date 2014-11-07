/*
 * Flash-led driver for Maxim MAX77819
 *
 * Copyright (C) 2013 Maxim Integrated Product
 * Gyungoh Yoo <jack.yoo@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LEDS_MAX77819_FLASH_H__
#define __LEDS_MAX77819_FLASH_H__

struct max77819_flash_platform_data
{
	bool flash_en_control;
	bool torch_en_control;
	bool enable_maxflash;
	int maxflash_threshold;		/* 2400000uV to 3400000uV in 33333uV steps */
	int maxflash_hysteresis;	/* One of 100000uV, 200000uV, 300000uV or -1 */
	int maxflash_falling_timer;	/* 256us to 2048us in 256us steps */
	int maxflash_rising_timer;	/* 256us to 2048us in 256us steps */
};

#endif /* __LEDS_MAX77819_FLASH_H__ */
