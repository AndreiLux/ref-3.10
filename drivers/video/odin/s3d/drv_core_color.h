/*
 * linux/drivers/video/odin/s3d/drv_core_color.h
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


#ifndef __DRV_CORE_COLOR_H__
#define __DRV_CORE_COLOR_H__

#include "drv_customer.h"
#include "drv_core.h"


/*  EXTERNAL FUNCTION	*/
int32 drv_core_init_color_parm  (int32 parm,
				void* parm1, struct drv_core_info *info);
int32 drv_core_init_color (int32 parm, void* parm1,
				struct drv_core_info *info);

#endif
