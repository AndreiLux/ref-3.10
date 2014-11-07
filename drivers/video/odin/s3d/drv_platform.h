/*
 * linux/drivers/video/odin/s3d/drv_platform.h
 *
 * Copyright (C) 2012 LG Electronics
 * Author:
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DRV_PLATFORM_H__
#define __DRV_PLATFORM_H__

#include "drv_customer.h"
#include "drv_register_map.h"
#include "drv_port.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
/*#include <linux/gpio.h>*/
/*#include <linux/of_gpio.h>*/
#include <linux/idr.h>
#include <linux/slab.h>

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
/*#include <media/videobuf2-core.h>*/
/*#include <media/v4l2-ctrls.h>*/
/*#include <media/v4l2-device.h>*/
/*#include <media/v4l2-mediabus.h>*/
/*#include <media/exynos_flite.h>*/
/*#include <media/v4l2-ioctl.h>*/

/*---------------------------------------------------------------------------
  DATA TYPE & STRUCT
---------------------------------------------------------------------------*/

#define DRV_PRINT	printk
#define DRV_SLEEP	ndelay
#define DRV_SPRINT	sprintf
#define DRV_FPRINT	fprintf

#define PI              3.14159265

enum drv_boolean{
  DRV_FALSE    = 0,
  DRV_TRUE     = 1,
};


/*---------------------------------------------------------------------------
  EXTERNAL FUNCTION
---------------------------------------------------------------------------*/

void* drv_malloc(int32 size);
void  drv_free(void* ptr);
void* drv_memcpy(void* dst,const void* src, int count);
void* drv_memset(void* s, int c, int n);

int32 drv_delay(int32 parm, uint32 ms);

#endif
