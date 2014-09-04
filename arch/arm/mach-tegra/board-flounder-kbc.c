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
#include <linux/delay.h>
#include <linux/keycombo.h>
#include <linux/keyreset.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>

#include "iomap.h"
#include "wakeups-t12x.h"
#include "gpio-names.h"

#define RESTART_DELAY_MS 7000
static int print_reset_warning(void)
{
	pr_info("Power held for %ld seconds. Restarting soon.\n",
				RESTART_DELAY_MS / MSEC_PER_SEC);
	return 0;
}

static struct keyreset_platform_data flounder_reset_keys_pdata = {
	.reset_fn = &print_reset_warning,
	.key_down_delay = RESTART_DELAY_MS,
	.keys_down = {
		KEY_POWER,
		0
	}
};

static struct platform_device flounder_reset_keys_device = {
	.name = KEYRESET_NAME,
	.dev.platform_data = &flounder_reset_keys_pdata,
	.id = PLATFORM_DEVID_AUTO,
};

int __init flounder_kbc_init(void)
{
	if (platform_device_register(&flounder_reset_keys_device))
		pr_warn(KERN_WARNING "%s: register key reset failure\n",
								__func__);
	return 0;
}
