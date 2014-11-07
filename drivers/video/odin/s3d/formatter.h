/*
 * linux/drivers/video/odin/s3d/formatter.h
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

#ifndef __DRV_DRIVER_H__
#define __DRV_DRIVER_H__

#include "drv_customer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEPSI_DRIVER_VERSION    "MEPSI F Driver ver.0.0004 "

#define NDRIVER_NAME		"mepsi_f"
#define NDRIVER_MAJOR		249
#define NDRIVER_MINOR		0


void fmt_write_reg(u32 addr, u32 val);
u32 fmt_read_reg(u32 addr);

#ifdef __cplusplus
}
#endif

#endif
