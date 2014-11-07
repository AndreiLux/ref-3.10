/*
 * linux/drivers/video/odin/s3d/svc_api.c
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


/*===========================================================================
  INCLUDE
===========================================================================*/

#include "svc_api.h"
#include "drv_mepsi.h"

/*===========================================================================
  DEFINITIONS
===========================================================================*/

/*===========================================================================
  DATA
===========================================================================*/

static int32 svc_api_null ( int32 cmd, void* parm1, void* parm2 );
static int32 svc_api_reg_read ( int32 cmd, void* parm1, void* parm2 );
static int32 svc_api_reg_write ( int32 cmd, void* parm1, void* parm2 );
static int32 svc_api_open ( int32 cmd, void* parm1, void* parm2 );
static int32 svc_api_close ( int32 cmd, void* parm1, void* parm2 );
static int32 svc_api_mode_change ( int32 cmd, void* parm1, void* parm2 );
static int32 svc_api_disp_update ( int32 cmd, void* parm1, void* parm2 );
static int32 svc_api_disp_colorbar ( int32 cmd, void* parm1, void* parm2 );

struct svc_api_info
{
  enum svc_cmd_type cmd;
  int32 (*func1) (int32 cmd, void *parm1, void *parm2);
  const char *name;
};

const static struct svc_api_info svc_info[] =
{
	{SVC_CMD_NULL , svc_api_null, "SVC_CMD_NULL"},
	{SVC_CMD_DRV_OPEN, svc_api_open, "SVC_CMD_DRV_OPEN"},
	{SVC_CMD_DRV_CLOSE, svc_api_close, "SVC_CMD_DRV_CLOSE"},
	{SVC_CMD_REG_READ, svc_api_reg_read, "SVC_CMD_REG_READ"},
	{SVC_CMD_REG_WRITE , svc_api_reg_write, "SVC_CMD_REG_WRITE"},
	{SVC_CMD_MODE_CHANGE  , svc_api_mode_change, "SVC_CMD_MODE_CHANGE"},
	{SVC_CMD_SCREEN_UPDATE, svc_api_disp_update, "SVC_CMD_SCREEN_UPDATE "},
	{SVC_CMD_DISPLAY_COLORBAR, svc_api_disp_colorbar, "SVC_CMD_DISLAY_COLOR_BAR"},
	{SVC_CMD_MAX  ,svc_api_null, "SVC_CMD_MAX"},
};


/*===========================================================================
  svc_api_init
===========================================================================*/
int32 svc_api_init      ( int32 cmd, void* parm1, void* parm2 )
{
	static int8 init=0;
	int32 ret=RET_OK;

	if (init == 1)
	{
		LOGI("already svc_api_init - error! \n");

		return RET_ERROR;
	}

	/* init=1;	*/

	return ret;
}

/*===========================================================================
  svc_api_exit
===========================================================================*/
int32 svc_api_exit      ( int32 cmd, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;

	/* init=0;	*/

	return ret;
}

/*===========================================================================
  svc_api_set
===========================================================================*/
int32 svc_api_set       ( int32 cmd, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;
	struct svc_api_info *info_ptr=(struct svc_api_info *)&svc_info[0];

	for ( ; info_ptr->cmd != SVC_CMD_MAX ; info_ptr++ )
	{
		if ( info_ptr->cmd == cmd )
		{
			LOGI("api : %s \n", info_ptr->name);

			if ( info_ptr->func1 != NULL )
			return(info_ptr->func1( cmd, parm1, parm2 ));
		}
	}

	LOGE("svc_api_set error ! (%d)\n",cmd);

	return ret;
}



/*
 * MEPSI API FUNCTION
*/

/*===========================================================================
  svc_api_null

===========================================================================*/
static int32 svc_api_null ( int32 cmd, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;

	LOGI("svc_api_null(%d)\n",cmd);

	return ret;
}

/*===========================================================================
  svc_api_reg_read

===========================================================================*/
static int32 svc_api_reg_read ( int32 cmd, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;
	struct drv_reg_info *ptr = (struct drv_reg_info *)parm1;
	/* struct SVC_CMD_info *info = (struct SVC_CMD_info *)parm1; */
	/* struct drv_reg_info *ptr  = &info->u.reg; */

	ptr->data = drv_io_read(parm2, ptr->addr);

	ret = ptr->addr;

	LOGI("svc_api_read (0x%x, 0x%x)\n", ptr->addr, ptr->data);

	return ret;
}

/*===========================================================================
  svc_api_reg_write

===========================================================================*/
static int32 svc_api_reg_write ( int32 cmd, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;
	struct drv_reg_info *ptr = (struct drv_reg_info *)parm1;

	LOGI("[Before] svc_api_write (0x%x, 0x%x)\n", ptr->addr, ptr->data);

	ret = drv_io_write(parm2, ptr->addr, ptr->data);

	LOGI("[After] svc_api_write (0x%x, 0x%x)\n", ptr->addr, ptr->data);

	return ret;
}

/*===========================================================================
  svc_api_open

===========================================================================*/
static int32 svc_api_open ( int32 cmd, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;

	struct svc_api_open_info *open_ptr = (struct svc_api_open_info *)parm1;
	struct svc_interface_info *ptr = (struct svc_interface_info *)&open_ptr->io;

	ret = svc_context_init_parm ( cmd, ptr, parm2 );
	ret = svc_context_init      ( cmd, ptr, parm2 );

	return ret;
}

/*===========================================================================
  svc_api_close

===========================================================================*/
static int32 svc_api_close  ( int32 cmd, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;

	ret = svc_context_exit ( cmd, parm1, parm2 );

	return ret;
}

/*===========================================================================
  svc_api_mode_change

===========================================================================*/
static int32 svc_api_mode_change   ( int32 cmd, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;
	struct drv_mode_change_info *ptr = (struct drv_mode_change_info *)parm1;

	switch (ptr->type)
	{
		case OUT_MODE_2D_CAM0                 :  break;
		case OUT_MODE_2D_CAM1                 :  break;
		case OUT_MODE_3D_PIXEL_BASE           :  break;
		case OUT_MODE_3D_SUBPIXEL_BASE        :  break;
		case OUT_MODE_3D_LINE_BASE            :  break;
		case OUT_MODE_3D_TIME_DIV             :  break;
		case OUT_MODE_3D_RED_BLUE             :  break;
		case OUT_MODE_3D_RED_GREEN            :  break;
		case OUT_MODE_3D_SIDE_BY_SIDE         :  break;
		case OUT_MODE_3D_TOP_BOTTOM           :  break;

		default :
		  ret = RET_ERROR;
	}
	return ret;
}


/*===========================================================================
  svc_api_disp_update
===========================================================================*/
static int32 svc_api_disp_update   ( int32 cmd, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;

	ret = drv_register_update (cmd, parm1, parm2);
	RET_CHECK(ret);

	ret = drv_screen_update (cmd, parm1, parm2);
	RET_CHECK(ret);

	return ret;
}

/*===========================================================================
  svc_api_disp_colorbar
===========================================================================*/
static int32 svc_api_disp_colorbar   ( int32 cmd, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;

	/* reserved */

	return ret;
}
