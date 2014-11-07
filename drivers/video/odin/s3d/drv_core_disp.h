/*
 * linux/drivers/video/odin/s3d/drv_core_disp.h
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


#ifndef __DRV_CORE_DISP_H__
#define __DRV_CORE_DISP_H__

#include "drv_customer.h"
#include "drv_core.h"

/*  EXTERNAL FUNCTION	*/
int32 get_scaler_factor(void* base, int32 src_w, int32 src_h, int32 dst_w,
		int32 dst_h, int16 * scale_w, int16 * scale_h);

int32 drv_core_init_disp_parm   (int32 parm, void* parm1,
								struct drv_core_info *info);
int32 drv_core_init_disp (int32 parm, void* parm1,
								struct drv_core_info *info);

int32 drv_core_init_fb_parm (int32 parm, void* parm1,
								struct drv_core_info *info);
int32 drv_core_init_fb (int32 parm, void* parm1, struct drv_core_info *info);

int32 drv_core_init_window_parm (int32 parm, void* parm1,
								struct drv_core_info *info);
int32 drv_core_init_window   (int32 parm, void* parm1,
								struct drv_core_info *info);
#endif
