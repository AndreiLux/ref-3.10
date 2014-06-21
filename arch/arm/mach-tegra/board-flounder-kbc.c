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

static struct keyreset_platform_data flounder_reset_keys_pdata = {
	.key_down_delay = 10000,
	.keys_down = {
		KEY_POWER,
		KEY_VOLUMEDOWN,
		0
	}
};

static struct platform_device flounder_reset_keys_device = {
	.name = KEYRESET_NAME,
	.dev.platform_data = &flounder_reset_keys_pdata,
	.id = PLATFORM_DEVID_AUTO,
};

#define PWR_MISTOUCH TEGRA_GPIO_PI3

static void clear_hw_reset(void)
{
	int ret = 0;
	pr_info("power key held down\n");
	ret = gpio_direction_output(PWR_MISTOUCH, 0);
	if (ret < 0)
		pr_err("[KEY] gpio_direction_output GPIO %d failed\n",
								PWR_MISTOUCH);
	msleep(100);
	ret = gpio_direction_output(PWR_MISTOUCH, 1);
	if (ret < 0)
		pr_err("[KEY] gpio_direction_output GPIO %d failed\n",
								PWR_MISTOUCH);
}

static void keep_hw_reset_clear(struct work_struct *);
DECLARE_DELAYED_WORK(clear_restart_work, &keep_hw_reset_clear);

static void keep_hw_reset_clear(struct work_struct *dummy)
{
	clear_hw_reset();
	schedule_delayed_work(&clear_restart_work, msecs_to_jiffies(2000));
}

static void start_reset_clear(void *dummy)
{
	schedule_delayed_work(&clear_restart_work, msecs_to_jiffies(2000));
}

static void stop_clearing(void *dummy)
{
	cancel_delayed_work_sync(&clear_restart_work);
}

static struct keycombo_platform_data flounder_clear_hw_reset_pdata = {
	.key_down_fn = &start_reset_clear,
	.key_up_fn = &stop_clearing,
	.keys_down = {
		KEY_POWER,
		0
	}
};

static struct platform_device flounder_clear_hw_reset_device = {
	.name = KEYCOMBO_NAME,
	.dev.platform_data = &flounder_clear_hw_reset_pdata,
	.id = PLATFORM_DEVID_AUTO,
};

int __init flounder_kbc_init(void)
{
	if (platform_device_register(&flounder_reset_keys_device))
		pr_warn(KERN_WARNING "%s: register key reset failure\n",
								__func__);

	gpio_request(PWR_MISTOUCH, "PWR_MISTOUCH");
	if (platform_device_register(&flounder_clear_hw_reset_device))
		pr_warn(KERN_WARNING "%s: register clear hw reset failure\n",
								__func__);
	return 0;
}
