/*
 * linux/drivers/video/odin/s3d/svc_api.h
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

#ifndef __SVC_API_H__
#define __SVC_API_H__

#include "drv_customer.h"
#include "drv_mepsi.h"
#include "svc_context.h"


/*===========================================================================
  svc_cmd_type
===========================================================================*/
enum svc_cmd_type
{
	SVC_CMD_NULL			= 0x0000 , /* NULL	*/

	SVC_CMD_DRV_OPEN		= 1 ,	/* open		*/
	SVC_CMD_DRV_CLOSE		= 2 ,	/* close	*/
	SVC_CMD_REG_READ		= 3 ,	/* register read	*/
	SVC_CMD_REG_WRITE		= 4 ,	/* register write	*/
	SVC_CMD_MODE_CHANGE		= 5 ,
		/* 2D,MANUAL,PIXEL,SIDE_BY_SIDE,LINE,RED_BLUE,RED_GREEN,SUB_PIXEL,
		TOP_BOTTOM,SIDE_BY_SIDE_FULL,TOP_BOTTOM_FULL */
	SVC_CMD_SCREEN_UPDATE		= 6 ,	/* Screen Update */
	SVC_CMD_DISPLAY_COLORBAR     = 7 ,	/* Display Color Bar */

	SVC_CMD_MAX				= (uint16)0x7fff,
};


/*===========================================================================
  SVC_CMD_info
===========================================================================*/
struct svc_api_open_info{
	struct svc_interface_info  io;
};

struct drv_reg_info
{
	uint32 addr;
	uint32 data;
};


struct drv_mode_change_info
{
	enum disp_modetype{
		DISP_MODE2D_CAM0 = 0,	/* 2D CAM0 */
		DISP_MODE2D_CAM1 = 1,	/* 2D CAM1 */
		DISP_MODES3D_MANUAL = 2,
		DISP_MODES3D_PIXEL   = 3,
		DISP_MODES3D_SIDE_BY_SIDE   = 4,
		DISP_MODES3D_LINE = 5,
		DISP_MODES3D_RED_BLUE = 6,
		DISP_MODES3D_RED_GREEN = 7,
		DISP_MODES3D_SUB_PIXEL = 8,
		DISP_MODES3D_TOP_BOTTOM = 9,
		DISP_MODES3D_SIDE_BY_SIDE_FULL  = 10,
		DISP_MODES3D_TOP_BOTTOM_FULL    = 11,
	}type;
};

int32 svc_api_init      ( int32 cmd, void* parm1, void* parm2 );
int32 svc_api_exit      ( int32 cmd, void* parm1, void* parm2 );
int32 svc_api_set       ( int32 cmd, void* parm1, void* parm2 );

#endif
