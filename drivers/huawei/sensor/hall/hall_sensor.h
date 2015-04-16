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


#ifndef HALL_SENSOR_H
#define HALL_SENSOR_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/hw_log.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <uapi/asm-generic/errno-base.h>
#include "../../inputhub/inputhub_route.h"
#include "../../inputhub/protocol.h"

#define HALL_DEV_NAME_LEN				(16)
#define HALL_MAX_SUPPORT_NUM			(2)
#define HALL_IRQ_NUM 				(2)
#define MAX_IRQ_NAME_LEN 			(50)
#define HALL_SENSOR_NAME 			"hall,sensor"
#define GPIO_HIGH_VOLTAGE  	 		(1)
#define GPIO_LOW_VOLTAGE    			(0)
#define NORTH_POLE_NAME 			"hall_north_pole"
#define SOUTH_POLE_NAME 			"hall_south_pole"
#define HALL_ID 				"hall_id"
#define HALL_TYPE 				"hall_type"
#define HALL_WAKE_UP 				"hall_wake_up_flag"
#define X_COR 					"x_coordinate"
#define Y_COR 					"y_coordinate"

typedef int packet_data;

typedef enum _hall_type {
	single_north_pole = 0,
	single_south_pole,
	double_poles,
} hall_type_t;

typedef enum _hall_status {
	hall_work_status = 1,
	hall_removed_status
} hall_status_t;

struct irq_info {
	int irq;
	int trigger_value;
	char name[MAX_IRQ_NAME_LEN];
};

struct hall_dev {
	int hall_type;
	hall_status_t hall_status;
	int wake_up;
	int hall_id;
	int x_coordinate;
	int y_coordinate;
	void *hall_device;
	struct platform_device *hall_dev;
	struct work_struct hall_work;
	struct wake_lock hall_lock;
	struct hall_ops *ops;
	struct irq_info irq_info[HALL_IRQ_NUM];
	char name[HALL_DEV_NAME_LEN];
};

struct hall_ops {
	int (*packet_event_data)(struct hall_dev *);
	int (*hall_event_report)(packet_data );
	int (*hall_device_irq_top_handler)(int , struct hall_dev *);
	int (*hall_release)(struct hall_dev *);
       int (*hall_device_init)(struct platform_device *pdev,\
		struct hall_dev *phall_dev);
};

extern struct hall_ops hall_device_ops;
int hall_first_report(bool enable);
#endif
