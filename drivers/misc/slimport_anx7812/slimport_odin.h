/*
 * Copyright(c) 2014, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef SLIMPORT_ODIN
#define SLIMPORT_ODIN

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_data/slimport_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/async.h>
#include <linux/slimport.h>
#include <linux/platform_data/gpio-odin.h>

#include "slimport_tx_drv.h"
#include "slimport_tx_reg.h"

#include <linux/of_gpio.h>
#include <linux/of_platform.h>

#include <linux/board_lge.h>
#include <linux/mfd/d2260/pmic.h>

#include "../../video/odin/display/hdmi-panel.h"

/* Enable IRQ After Boot-Up*/
#define SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
/* Set ANX_PDWN_CTL GPIO to Input Mode When Device Suspended */
#define ANX_PDWN_CTL_GPIO_MODE
/* Handle Slimport Cable Detection ISR After I2C Resumed */
#define ANX_I2C_SYNC_IRQ

#ifdef ANX_I2C_SYNC_IRQ
enum {
 I2C_RESUME = 0,
 I2C_SUSPEND,
 I2C_SUSPEND_IRQ,
};
#endif

#endif
