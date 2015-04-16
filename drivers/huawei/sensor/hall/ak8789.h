/*
 * copyright (C) 2013 HUAWEI, Inc.
 * Author: tuhaibo <tuhaibo@huawei.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef HALL_AK8789_H
#define HALL_AK8789_H

#include <linux/string.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/hw_log.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include "hall_sensor.h"
#include "../../inputhub/inputhub_route.h"
#include "../../inputhub/protocol.h"

#define AK8789_GPIO_NUM				(2)
#define HALL_EVENT_START_SHIFT 			(2)
#define MAX_GPIO_NAME_LEN 			(50)

struct hall_ak8789_dev {
	hall_type_t hall_ak8789_type;
	int gpio_poles[AK8789_GPIO_NUM];
};

#endif
