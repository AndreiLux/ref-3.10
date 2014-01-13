/*
 * arch/arm/mach-tegra/board-flounder-kbc.c
 * Keys configuration for Nvidia tegra4 flounder platform.
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>

#include "iomap.h"
#include "wakeups-t12x.h"
#include "gpio-names.h"

#define GPIO_KEY(_id, _gpio, _iswake)           \
	{                                       \
		.code = _id,                    \
		.gpio = TEGRA_GPIO_##_gpio,     \
		.active_low = 1,                \
		.desc = #_id,                   \
		.type = EV_KEY,                 \
		.wakeup = _iswake,              \
		.debounce_interval = 20,        \
	}

#define PMC_WAKE2_STATUS         0x168
#define TEGRA_WAKE_PWR_INT      (1UL << 19)

static int flounder_wakeup_key(void)
{
	u32 status;
	int is_power_key;

	status = __raw_readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE2_STATUS);

	is_power_key = !!(status & TEGRA_WAKE_PWR_INT);
	if (is_power_key) {
		pr_info("%s: Power key pressed\n", __func__);
		return KEY_POWER;
	}
	return KEY_RESERVED;
}

static struct gpio_keys_button flounder_int_keys[] = {
	[0] = GPIO_KEY(KEY_POWER, PQ0, 1),
	[1] = GPIO_KEY(KEY_VOLUMEUP, PV2, 0),
	[2] = GPIO_KEY(KEY_VOLUMEDOWN, PQ5, 0),
};

static struct gpio_keys_platform_data flounder_int_keys_pdata = {
	.buttons	= flounder_int_keys,
	.nbuttons	= ARRAY_SIZE(flounder_int_keys),
	.wakeup_key	= flounder_wakeup_key,
};

static struct platform_device flounder_int_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data  = &flounder_int_keys_pdata,
	},
};

int __init flounder_kbc_init(void)
{
	return platform_device_register(&flounder_int_keys_device);
}
