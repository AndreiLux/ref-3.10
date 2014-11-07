/*
 * linux/drivers/video/odin/s3d/svc_context.c
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

#include "drv_register_map.h"
#include "svc_api.h"
#include "drv_core.h"
#include "svc_context.h"


static struct svc_context_info *g_context = DRV_NULL;


static int32 svc_context_set  ( int32 parm, void* parm1, void* parm2 );


/*===========================================================================
FUNCTION     get_context
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
===========================================================================*/
struct svc_context_info* get_context ( void )
{
  return g_context;
}

/*===========================================================================
FUNCTION     svc_context_set
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
===========================================================================*/
static int32 svc_context_set  ( int32 parm, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;
	struct svc_interface_info *info_ptr = (struct svc_interface_info *)parm1;

	if ( g_context == DRV_NULL )
	{
		g_context =
			(struct svc_context_info*) drv_malloc
			(sizeof(struct svc_context_info));
	}
	else
	{
		return RET_MEMORY_FAIL;
	}

	memcpy(&g_context->info, info_ptr, sizeof(struct svc_interface_info));

	return ret;
}

/*===========================================================================
FUNCTION     svc_context_exit
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
===========================================================================*/
int32 svc_context_exit       ( int32 cmd, void* parm1, void* parm2 )
{
  int32 ret=RET_OK;

  drv_free(g_context);

  g_context = DRV_NULL;

  return ret;
}

/*===========================================================================
FUNCTION     svc_context_init_parm
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
===========================================================================*/
int32 svc_context_init_parm( int32 cmd, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;
	struct svc_context_info *info;
	/*struct svc_interface_info *info_ptr =
		(struct svc_interface_info  *)parm1; */

	ret = svc_context_set ( cmd, parm1, parm2 );
	RET_CHECK(ret);

	info = get_context();

	ret = drv_core_init_parm ( cmd, parm1, (struct drv_core_info *)&info->core );
	RET_CHECK(ret);

	return RET_OK;
}



/*===========================================================================
FUNCTION     svc_context_init
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
===========================================================================*/
int32 svc_context_init  ( int32 cmd, void* parm1, void* parm2 )
{
	int32 ret=RET_OK;
	struct svc_context_info *ctxt = get_context();
	struct svc_interface_info *info_ptr = (struct svc_interface_info  *)parm1;

	LOGF("\n ***** svc_context_init start ***** \n");

	ret = drv_core_init (cmd, parm1, (struct drv_core_info *)&ctxt->core);
	RET_CHECK(ret);

	LOGF("\n ***** svc_context_init end ***** \n");

	return ret;
}


/*===========================================================================
  frame_buffer_setting

memory map(external ddr)

--------------------------------------        000MByte
  out page0
  out page1
  win0 page0 (max 3 plane)
  win0 page1 (max 3 plane)
  win1 page0 (max 3 plane)
  win1 page1 (max 3 plane)
  win2 page0 (max 3 plane)
  win2 page1 (max 3 plane)
  win3 page0 (max 3 plane)
  win3 page1 (max 3 plane)
--------------------------------------        MAX. 128MByte


===========================================================================*/
int32 frame_buffer_setting(int32 cmd, void* parm1, void* parm2)
{
	int32 ret=RET_OK;
	struct svc_interface_info *ptr = (struct svc_interface_info *)parm1;

	if ( ptr->out.pf_enabled == 0 )
	{
		ptr->out.addr_p0 = 0x000000;
		ptr->out.addr_p1 = ptr->out.addr_p0;
	}
	else
	{
		ptr->out.addr_p0 = 0x000000;
		ptr->out.addr_p1 = ptr->out.addr_p0 + (ptr->out.w*ptr->out.h*4);
	}

	if ( ptr->out.win[0].pf_enabled == 0 )
	{
		ptr->out.win[0].addr1_p0 = ptr->out.addr_p1 + (ptr->out.w*ptr->out.h*4);
		ptr->out.win[0].addr2_p0 = ptr->out.win[0].addr1_p0+
			(ptr->out.win[0].tw * ptr->out.win[0].th * 2);
		ptr->out.win[0].addr3_p0 = ptr->out.win[0].addr2_p0+
			(ptr->out.win[0].tw * ptr->out.win[0].th * 2);
		ptr->out.win[0].addr1_p1 = ptr->out.win[0].addr1_p0;
		ptr->out.win[0].addr2_p1 = ptr->out.win[0].addr2_p0;
		ptr->out.win[0].addr3_p1 = ptr->out.win[0].addr3_p0;
	}
	else
	{
		ptr->out.win[0].addr1_p0 = ptr->out.addr_p1 + (ptr->out.w*ptr->out.h*4);
		ptr->out.win[0].addr2_p0 = ptr->out.win[0].addr1_p0+
			(ptr->out.win[0].tw * ptr->out.win[0].th * 2);
		ptr->out.win[0].addr3_p0 = ptr->out.win[0].addr2_p0+
			(ptr->out.win[0].tw * ptr->out.win[0].th * 2);
		ptr->out.win[0].addr1_p1 = ptr->out.win[0].addr3_p0+
			(ptr->out.win[0].tw * ptr->out.win[0].th * 2);
		ptr->out.win[0].addr2_p1 = ptr->out.win[0].addr1_p1+
			(ptr->out.win[0].tw * ptr->out.win[0].th * 2);
		ptr->out.win[0].addr3_p1 = ptr->out.win[0].addr2_p1+
			(ptr->out.win[0].tw * ptr->out.win[0].th * 2);
	}

	if ( ptr->out.win[1].pf_enabled == 0 )
	{
		ptr->out.win[1].addr1_p0 = ptr->out.win[0].addr3_p1+
			(ptr->out.win[0].tw * ptr->out.win[0].th * 2);
		ptr->out.win[1].addr2_p0 = ptr->out.win[1].addr1_p0+
			(ptr->out.win[1].tw * ptr->out.win[1].th * 2);
		ptr->out.win[1].addr3_p0 = ptr->out.win[1].addr2_p0+
			(ptr->out.win[1].tw * ptr->out.win[1].th * 2);
		ptr->out.win[1].addr1_p1 = ptr->out.win[1].addr1_p0;
		ptr->out.win[1].addr2_p1 = ptr->out.win[1].addr2_p0;
		ptr->out.win[1].addr3_p1 = ptr->out.win[1].addr3_p0;
	}
	else
	{
		ptr->out.win[1].addr1_p0 = ptr->out.win[0].addr3_p1+
			(ptr->out.win[0].tw * ptr->out.win[0].th * 2);
		ptr->out.win[1].addr2_p0 = ptr->out.win[1].addr1_p0+
			(ptr->out.win[1].tw * ptr->out.win[1].th * 2);
		ptr->out.win[1].addr3_p0 = ptr->out.win[1].addr2_p0+
			(ptr->out.win[1].tw * ptr->out.win[1].th * 2);
		ptr->out.win[1].addr1_p1 = ptr->out.win[1].addr3_p0+
			(ptr->out.win[1].tw * ptr->out.win[1].th * 2);
		ptr->out.win[1].addr2_p1 = ptr->out.win[1].addr1_p1+
			(ptr->out.win[1].tw * ptr->out.win[1].th * 2);
		ptr->out.win[1].addr3_p1 = ptr->out.win[1].addr2_p1+
			(ptr->out.win[1].tw * ptr->out.win[1].th * 2);
	}

	if ( ptr->out.win[2].pf_enabled == 0 )
	{
		ptr->out.win[2].addr2_p1 = ptr->out.win[1].addr3_p1+
			(ptr->out.win[1].tw * ptr->out.win[1].th * 2);
		ptr->out.win[2].addr3_p1 = ptr->out.win[2].addr2_p1;
	}
	else
	{
		ptr->out.win[2].addr2_p1 = ptr->out.win[1].addr3_p1+
			(ptr->out.win[1].tw * ptr->out.win[1].th * 2);
		ptr->out.win[2].addr3_p1 = ptr->out.win[2].addr2_p1+
			(ptr->out.win[2].tw * ptr->out.win[2].th * 2);
	}

	if ( ptr->out.win[3].pf_enabled == 0 )
	{
		ptr->out.win[3].addr1_p0 = ptr->out.win[2].addr3_p1+
			(ptr->out.win[2].tw * ptr->out.win[2].th * 2);
		ptr->out.win[3].addr1_p1 = ptr->out.win[3].addr1_p0;
	}
	else
	{
		ptr->out.win[3].addr1_p0 = ptr->out.win[2].addr3_p1+
			(ptr->out.win[2].tw * ptr->out.win[2].th * 2);
		ptr->out.win[3].addr1_p1 = ptr->out.win[3].addr1_p0+
			(ptr->out.win[3].tw * ptr->out.win[3].th * 2);
	}

	return ret;
}

