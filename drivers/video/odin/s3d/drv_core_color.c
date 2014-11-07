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

/*========================================================
  INCLUDE
==========================================================*/
#include "drv_register_map.h"

#include "drv_core_color.h"
#include "svc_context.h"


/*========================================================
FUNCTION     drv_core_init_color_parm
DESCRIPTION

#define YUV_R(iY,iCb,iCr)	( 1.164*(iY - 16) + 1.596*(iCr - 128)  )
#define YUV_G(iY,iCb,iCr)
				( 1.164*(iY - 16) - 0.813*(iCr - 128) - 0.391*(iCb - 128))
#define YUV_B(iY,iCb,iCr)   ( 1.164*(iY - 16) + 2.018*(iCb - 128) )

#define RGB_Y(iR,iG,iB)     ( ( 0.257*iR) + (0.504*iG) + (0.098*iB) + 16  )
#define RGB_U(iR,iG,iB)     ( (-0.148*iR) - (0.291*iG) + (0.439*iB) + 128 )
#define RGB_V(iR,iG,iB)     ( ( 0.439*iR) - (0.368*iG) - (0.071*iB) + 128 )

DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
==========================================================*/
int32 drv_core_init_color_parm(int32 parm, void* parm1,
			struct drv_core_info *info)
{
	int32 ret=RET_OK;
	struct svc_interface_info *info_ptr = (struct svc_interface_info *)parm1;

	info_ptr->color.enable_y2r = 0;
	info_ptr->color.enable_r2y = 0;

	if (info_ptr->color.enable_y2r == 1)
	{
		info_ptr->color.r11_offset =(uint16)float2fixed((float) -16.000, 4 );
		info_ptr->color.r12_offset =(uint16)float2fixed((float) 0.000, 4 );
		info_ptr->color.r13_offset =(uint16)float2fixed((float)-128.000, 4 );
		info_ptr->color.r14_offset =(uint16)float2fixed((float) 0.000, 4 );
		info_ptr->color.g12_offset =(uint16)float2fixed((float)-128.000, 4 );
		info_ptr->color.g13_offset =(uint16)float2fixed((float)-128.000, 4 );
		info_ptr->color.g14_offset =(uint16)float2fixed((float) 0.000, 4 );
		info_ptr->color.b12_offset =(uint16)float2fixed((float)-128.000, 4 );
		info_ptr->color.b13_offset =(uint16)float2fixed((float) 0.000, 4 );
		info_ptr->color.b14_offset =(uint16)float2fixed((float) 0.000, 4 );
		info_ptr->color.r11coef =(uint16)float2fixed((float) 1.164, 10);
		info_ptr->color.r12coef =(uint16)float2fixed((float) 0.000, 10);
		info_ptr->color.r13coef =(uint16)float2fixed((float) 1.596, 10);
		info_ptr->color.g12coef =(uint16)float2fixed((float) -0.391, 10);
		info_ptr->color.g13coef =(uint16)float2fixed((float) -0.813, 10);
		info_ptr->color.b12coef =(uint16)float2fixed((float) 2.018, 10);
		info_ptr->color.b13coef =(uint16)float2fixed((float) 0.000, 10);
	}

	if (info_ptr->color.enable_r2y == 1)
	{
		info_ptr->color.y_rcoef =(uint16)float2fixed((float) 0.257, 10);
		info_ptr->color.y_gcoef =(uint16)float2fixed((float) 0.504, 10);
		info_ptr->color.y_bcoef =(uint16)float2fixed((float) 0.098, 10);
		info_ptr->color.cb_rcoef =(uint16)float2fixed((float)-0.148, 10);
		info_ptr->color.cb_gcoef =(uint16)float2fixed((float)-0.291, 10);
		info_ptr->color.cb_bcoef =(uint16)float2fixed((float) 0.439, 10);
		info_ptr->color.cr_rcoef =(uint16)float2fixed((float) 0.439, 10);
		info_ptr->color.cr_gcoef =(uint16)float2fixed((float)-0.368, 10);
		info_ptr->color.cr_bcoef =(uint16)float2fixed((float)-0.071, 10);

		info_ptr->color.y_coef_offset = 16;
		info_ptr->color.c_coef_offset = 128;
	}

	if (info_ptr->color.enable_y2r)
	{
		/*  y2r conversion control       */
		info->color.r12_offset = info_ptr->color.r12_offset ;
		info->color.r11_offset = info_ptr->color.r11_offset ;
		info->color.r14_offset = info_ptr->color.r14_offset ;
		info->color.r13_offset = info_ptr->color.r13_offset ;
		info->color.g13_offset = info_ptr->color.g13_offset ;
		info->color.g12_offset = info_ptr->color.g12_offset ;
		info->color.g14_offset = info_ptr->color.g14_offset ;
		info->color.b13_offset = info_ptr->color.b13_offset ;
		info->color.b12_offset = info_ptr->color.b12_offset ;
		info->color.b14_offset = info_ptr->color.b14_offset ;
		info->color.r12coef = info_ptr->color.r12coef ;
		info->color.r11coef = info_ptr->color.r11coef ;
		info->color.r13coef = info_ptr->color.r13coef ;
		info->color.g13coef = info_ptr->color.g13coef ;
		info->color.g12coef = info_ptr->color.g12coef ;
		info->color.b13coef = info_ptr->color.b13coef ;
		info->color.b12coef = info_ptr->color.b12coef ;
	}

	if (info_ptr->color.enable_r2y)
	{
		/*  r2y conversion control    */
		info->color.y_gcoef = info_ptr->color.y_gcoef;
		info->color.y_rcoef = info_ptr->color.y_rcoef;
		info->color.y_bcoef = info_ptr->color.y_bcoef ;
		info->color.cb_gcoef = info_ptr->color.cb_gcoef ;
		info->color.cb_rcoef = info_ptr->color.cb_rcoef ;
		info->color.cb_bcoef = info_ptr->color.cb_bcoef ;
		info->color.cr_gcoef = info_ptr->color.cr_gcoef ;
		info->color.cr_rcoef = info_ptr->color.cr_rcoef ;
		info->color.cr_bcoef = info_ptr->color.cr_bcoef ;
		info->color.c_coef_offset= info_ptr->color.c_coef_offset;
		info->color.y_coef_offset= info_ptr->color.y_coef_offset;
	}

	return ret;
}


/*========================================================
FUNCTION     drv_core_init_color
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
==========================================================*/
int32 drv_core_init_color  (int32 parm, void* parm1,
						struct drv_core_info *info)
{
	int32 ret=RET_OK;
	struct svc_interface_info *info_ptr = (struct svc_interface_info *)parm1;

	LOGF("\n ***** drv_core_init_color - start ***** \n");

	if (info_ptr->color.enable_y2r)
	{
		set_reg16(DRV_NULL, Y2R_R11_OFFSET,
		(info->color.r11_offset & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_R12_OFFSET,
		(info->color.r12_offset & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_R13_OFFSET,
		(info->color.r13_offset & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_R14_OFFSET,
		(info->color.r14_offset & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_G12_OFFSET,
		(info->color.g12_offset & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_G13_OFFSET,
		(info->color.g13_offset & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_G14_OFFSET,
		(info->color.g14_offset & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_B12_OFFSET,
		(info->color.b12_offset & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_B13_OFFSET,
		(info->color.b13_offset & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_B14_OFFSET,
		(info->color.b14_offset & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_R11_COEF,
		(info->color.r11coef & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_R12_COEF,
		(info->color.r12coef & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_R13_COEF,
		(info->color.r13coef & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_G12_COEF,
		(info->color.g12coef & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_G13_COEF,
		(info->color.g13coef & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_B12_COEF,
		(info->color.b12coef & BIT_MASK13) >> 0);
		set_reg16(DRV_NULL, Y2R_B13_COEF,
		(info->color.b13coef & BIT_MASK13) >> 0);
	}

	if (info_ptr->color.enable_r2y)
	{
		set_reg16(DRV_NULL, R2Y_Y_RCOEF ,
		(info->color.y_rcoef & BIT_MASK11) >> 0);
		set_reg16(DRV_NULL, R2Y_Y_GCOEF ,
		(info->color.y_gcoef & BIT_MASK11) >> 0);
		set_reg16(DRV_NULL, R2Y_Y_BCOEF ,
		(info->color.y_bcoef & BIT_MASK11) >> 0);
		set_reg16(DRV_NULL, R2Y_CB_RCOEF ,
		(info->color.cb_rcoef & BIT_MASK11) >> 0);
		set_reg16(DRV_NULL, R2Y_CB_GCOEF ,
		(info->color.cb_gcoef & BIT_MASK11) >> 0);
		set_reg16(DRV_NULL, R2Y_CB_BCOEF ,
		(info->color.cb_bcoef & BIT_MASK11) >> 0);
		set_reg16(DRV_NULL, R2Y_CR_RCOEF ,
		(info->color.cr_rcoef & BIT_MASK11) >> 0);
		set_reg16(DRV_NULL, R2Y_CR_GCOEF ,
		(info->color.cr_gcoef & BIT_MASK11) >> 0);
		set_reg16(DRV_NULL, R2Y_CR_BCOEF ,
		(info->color.cr_bcoef & BIT_MASK11) >> 0);
		set_reg16(DRV_NULL, R2Y_Y_COEF_OFFSET ,
		(info->color.y_coef_offset & BIT_MASK10) >> 0);
		set_reg16(DRV_NULL, R2Y_C_COEF_OFFSET ,
		(info->color.c_coef_offset & BIT_MASK10) >> 0);
	}

	LOGF("\n ***** drv_core_init_color - end ***** \n");

	return ret;
}
