/*
 * linux/drivers/video/odin/s3d/drv_core.c
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
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*=========================================================
  INCLUDE
===========================================================*/
#include "drv_register_map.h"
#include "drv_core.h"
#include "svc_context.h"
#include "drv_core_disp.h"
#include "drv_core_color.h"


/*=========================================================
FUNCTION     drv_core_init_parm
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
===========================================================*/
int32 drv_core_init_parm (int32 parm, void* parm1, struct drv_core_info *info)
{
	int32 ret=RET_OK;
	/* struct svc_interface_info *info_ptr =
		(struct svc_interface_info  *)parm1;  */

	(void)drv_core_init_disp_parm (parm, parm1, info);
	(void)drv_core_init_fb_parm (parm, parm1, info);
	(void)drv_core_init_window_parm (parm, parm1, info);
	(void)drv_core_init_color_parm (parm, parm1, info);

	info->init=1;

	return ret;
}


/*=========================================================
FUNCTION     drv_core_init
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
===========================================================*/
int32 drv_core_init  (int32 parm, void* parm1, struct drv_core_info *info)
{
	int32 ret=RET_OK;
	struct svc_interface_info *info_ptr = (struct svc_interface_info *)parm1;

	LOGF("\n// ***** drv_core_init - start ***** \n");

#ifndef FEATURE_BUILD_RELEASE
	LOGF("\n// ***** Formatter enable ***** \n");
	set_reg16(DRV_NULL, BASE_ECT, 0x04); /* Formatter Enable */
#endif

	(void)drv_core_init_disp (parm, info_ptr, info);
	(void)drv_core_init_fb (parm, info_ptr, info);
	(void)drv_core_init_window(parm, info_ptr, info);
	(void)drv_core_init_color (parm, info_ptr, info);

	LOGF("// ***** drv_core_init - end ***** \n");
	LOGI("drv_core_init - end \n");

	return ret;
}


/*=========================================================
FUNCTION     drv_register_update
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
===========================================================*/
int32 drv_register_update   (int32 parm, void* parm1, void* parm2)
{
	int32 ret=RET_OK;
	struct drv_core_info *ptr = &get_context()->core;

	LOGI("register_update \n");

	ptr->shadow_update = 1;

	set_reg16(DRV_NULL, SHADOW_UP,
		( ((ptr->shadow_update & BIT_MASK1 ) << 0) ) );

  return ret;
}

/*=========================================================
FUNCTION     drv_screen_update
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
===========================================================*/
int32 drv_screen_update     (int32 parm, void* parm1, void* parm2)
{
  int32 ret=RET_OK;
  struct drv_core_info *ptr = &get_context()->core;

  LOGI("drv_screen_update \n");

  ptr->frame_start = 1;

  set_reg16(DRV_NULL, FRAME_ST,
      ( ((ptr->frame_start & BIT_MASK1 ) << 0) ) );

  return ret;
}
