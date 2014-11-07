/*
 * linux/drivers/video/odin/s3d/drv_core_disp.c
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


/*=======================================================
  INCLUDE
=========================================================*/
#include "drv_register_map.h"
#include "drv_core_disp.h"
#include "svc_context.h"

/*#define __set_reg16(a,b,c)
set_reg16(a,((b & 0xf000) | ((b & 0xfff) >> 2)),c) */


/*--------------------------------------------------------------
  out format
----------------------------------------------------------------*/

const static struct drv_out_format_info out_format_tbl[] =
{
  {IMG_FORMAT_NULL,},         /*,d_order ,z_position  ,d_format */
  {IMG_YUV444_1P_UNPACKED_VUYZ  ,2       ,1       ,4},/* YUV444_UNPACKED */
  {IMG_YUV444_1P_UNPACKED_ZVUY  ,2       ,0       ,4},
  {IMG_YUV444_1P_UNPACKED_UVYZ  ,3       ,1       ,4},
  {IMG_YUV444_1P_UNPACKED_ZUVY  ,3       ,0       ,4},
  {IMG_YUV444_1P_UNPACKED_ZYUV  ,0       ,0       ,4},
  {IMG_YUV444_1P_UNPACKED_ZYVU  ,1       ,0       ,4},
  {IMG_YUV444_1P_UNPACKED_YUVZ  ,0       ,1       ,4},
  {IMG_YUV444_1P_UNPACKED_YVUZ  ,1       ,1       ,4},
  {IMG_YUV444_1P_PACKED_VUY     ,2       ,0       ,5},/* YUV444_PACKED */
  {IMG_YUV444_1P_PACKED_UVY     ,3       ,0       ,5},
  {IMG_YUV444_1P_PACKED_YVU     ,1       ,0       ,5},
  {IMG_YUV444_1P_PACKED_YUV     ,0       ,0       ,5},
  {IMG_RGB8888_RGBZ             ,1       ,1       ,0},/* RGB888 pack */
  {IMG_RGB8888_ZRGB             ,1       ,0       ,0},
  {IMG_RGB8888_BGRZ             ,0       ,1       ,0},
  {IMG_RGB8888_ZBGR             ,0       ,0       ,0},
  {IMG_RGB888_RGB               ,1       ,0       ,1},/* RGB888 unpack */
  {IMG_RGB888_BGR               ,0       ,0       ,1},
  {IMG_RGB565_RGB               ,1       ,0       ,2},
  {IMG_RGB565_BGR               ,0       ,0       ,2},

  {IMG_FORMAT_MAX,},
};


/*-------------------------------------------------------------
  in format
---------------------------------------------------------------*/

const static struct drv_in_format_info in_format_tbl[] =
{
  {IMG_FORMAT_NULL,},
  /* pack_en,z_position,d_order,rgb16_en,y420_mode,y422_mode,y_format,y_en} */
  {IMG_YUV444_1P_UNPACKED_VUYZ  ,0 ,0 ,0 ,0 ,0        ,0       ,0       ,1},
  {IMG_YUV444_1P_UNPACKED_ZVUY  ,0 ,1 ,0 ,0 ,0        ,0       ,0       ,1},
  {IMG_YUV444_1P_UNPACKED_UVYZ  ,0 ,0 ,1 ,0 ,0        ,0       ,0       ,1},
  {IMG_YUV444_1P_UNPACKED_ZUVY  ,0 ,1 ,1 ,0 ,0        ,0       ,0       ,1},
  {IMG_YUV444_1P_UNPACKED_ZYUV  ,0 ,1 ,1 ,0 ,0        ,0       ,0       ,1},
  {IMG_YUV444_1P_UNPACKED_ZYVU  ,0 ,1 ,1 ,0 ,0        ,0       ,0       ,1},
  {IMG_YUV444_1P_UNPACKED_YUVZ  ,0 ,1 ,1 ,0 ,0        ,0       ,0       ,1},
  {IMG_YUV444_1P_UNPACKED_YVUZ  ,0 ,1 ,1 ,0 ,0        ,0       ,0       ,1},
  {IMG_YUV444_1P_PACKED_VUY     ,1 ,0 ,1 ,0 ,0        ,0       ,0       ,1},
  {IMG_YUV444_1P_PACKED_UVY     ,1 ,0 ,1 ,0 ,0        ,0       ,0       ,1},
  {IMG_YUV444_1P_PACKED_YVU     ,1 ,0 ,1 ,0 ,0        ,0       ,0       ,1},
  {IMG_YUV444_1P_PACKED_YUV     ,1 ,0 ,1 ,0 ,0        ,0       ,0       ,1},
  {IMG_YUV422_1P_PACKED_VYUY    ,0 ,0 ,1 ,0 ,0        ,0       ,1       ,1},
  {IMG_YUV422_1P_PACKED_UYVY    ,0 ,0 ,0 ,0 ,0        ,0       ,1       ,1},
  {IMG_YUV422_1P_PACKED_YVYU    ,0 ,0 ,1 ,0 ,0        ,1       ,1       ,1},
  {IMG_YUV422_1P_PACKED_YUYV    ,0 ,0 ,0 ,0 ,0        ,1       ,1       ,1},
  {IMG_YUV422_2P_PLANER_VU      ,0 ,0 ,0 ,0 ,0        ,0       ,2       ,1},
  {IMG_YUV422_2P_PLANER_UV      ,0 ,0 ,1 ,0 ,0        ,0       ,2       ,1},
  {IMG_RGB8888_RGBZ             ,0 ,0 ,0 ,0 ,0        ,0       ,0       ,0},
  {IMG_RGB8888_ZRGB             ,0 ,1 ,0 ,0 ,0        ,0       ,0       ,0},
  {IMG_RGB8888_BGRZ             ,0 ,0 ,1 ,0 ,0        ,0       ,0       ,0},
  {IMG_RGB8888_ZBGR             ,0 ,1 ,1 ,0 ,0        ,0       ,0       ,0},
  {IMG_RGB888_RGB               ,1 ,0 ,0 ,0 ,0        ,0       ,0       ,0},
  {IMG_RGB888_BGR               ,1 ,0 ,1 ,0 ,0        ,0       ,0       ,0},
  {IMG_RGB565_RGB               ,0 ,0 ,0 ,1 ,0        ,0       ,0       ,0},
  {IMG_RGB565_BGR               ,0 ,0 ,1 ,1 ,0        ,0       ,0       ,0},
  {IMG_YUV420_IMC1              ,0 ,0 ,1 ,0 ,0        ,0       ,3       ,1},
  {IMG_YUV420_IMC3              ,0 ,0 ,0 ,0 ,0        ,0       ,3       ,1},
  {IMG_YUV420_IMC2              ,0 ,0 ,1 ,0 ,1        ,0       ,3       ,1},
  {IMG_YUV420_IMC4              ,0 ,0 ,0 ,0 ,1        ,0       ,3       ,1},
  {IMG_YUV420_YV12              ,0 ,0 ,0 ,0 ,2        ,0       ,3       ,1},
  {IMG_YUV420_YV21              ,0 ,0 ,1 ,0 ,2        ,0       ,3       ,1},
  {IMG_YUV420_NV12              ,0 ,0 ,1 ,0 ,3        ,0       ,3       ,1},
  {IMG_YUV420_NV21              ,0 ,0 ,0 ,0 ,3        ,0       ,3       ,1},

  {IMG_FORMAT_MAX,},
};


/*============================================================
FUNCTION     drv_core_init_disp_parm
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
==============================================================*/
int32 drv_core_init_disp_parm  (int32 parm, void* parm1,
						struct drv_core_info *info)
{
	int32 ret=RET_OK;
	struct svc_interface_info *info_ptr = (struct svc_interface_info  *)parm1;
	const struct drv_out_format_info *out_format_ptr = &out_format_tbl[0];
	int8 bypass = 0;

	LOGI("\n ***** drv_core_init_disp_parm - start ***** \n");

	if ( info_ptr->out.color_enabled == 1 )
		info->test_en1  =1;/*color bar output enable (for test) */
	else
		info->test_en1  =0;/*color bar output enable (for test) */

	info->test_en0      =0;/*single color output enable (for test) */
	info->start_vpol    =0;
			/*vsync start polarity ( 0 : vsync rising edge,  1 : vsync fallin */
	info->start_mode2   =0;
			/*vsync start mode enable (0 : disable,  1 : enable)   */

	for ( ; out_format_ptr->format != IMG_FORMAT_MAX ; out_format_ptr++)
	{
		if ( out_format_ptr->format == info_ptr->out.format )
		{
			LOGI("OUT FORMAT : %d \n", out_format_ptr->format);

			info->dma_od_order      =out_format_ptr->d_order;
			info->dma_oz_position   =out_format_ptr->z_position;
			info->dma_od_format     =out_format_ptr->d_format;
			break;
		}
	}

	info->fifo_od_format =0;
		/*fifo interface data format ( 0 : rgb24,  1 : yuv444)  */
	info->start_mode        =1;
		/*frame start mode ( 0 : single,  1 : periodic)  */
	info->dma_if_en         =1;
		/*dma (write) interface enable (0 : disable,  1 : enable)  */
	info->fifo_if_en        =1;
		/*fifo interface enable (0 : disable,  1 : enable)  */
	info->frame_start       =1;
		/*frame start (auto cleared)    */
	info->shadow_update     =1;
		/*shadow regsiter update enable (auto cleared)   */
	info->fb3_rpage         =0;
		/*designate frame buffer3 page to read    */
	info->fb2_rpage         =0;
		/*designate frame buffer2 page to read    */
	info->fb1_rpage         =0;
		/*designate frame buffer1 page to read    */
	info->fb0_rpage         =0;
		/*designate frame buffer0 page to read    */
	info->df_dst_type       =0;
		/*deformatting (0:side-by-side,1:top-bottom,
			2:pixel-base,3:line-base,4:anaglyph) */
	info->df_src_type       =0;/*wj@@,temp deformatting source type
							(0 : pixel base image, 1 : line base image) */
	info->deformat_en =0;/*de-formatting enable ( 0 : disable, 1 : enable) */

	switch (info_ptr->out.mode)
	{
	case OUT_MODE_2D_CAM0 :
	case OUT_MODE_2D_CAM1 :
		bypass = 1;
		break;

	case OUT_MODE_3D_PIXEL_BASE :
		info->s3d_type = 0;
		break;
	case OUT_MODE_3D_SUBPIXEL_BASE :
		info->s3d_type = 1;
		break;
	case OUT_MODE_3D_LINE_BASE :
		info->s3d_type = 2;
		break;
	case OUT_MODE_3D_TIME_DIV :
		info->s3d_type = 3;
		break;
	case OUT_MODE_3D_RED_BLUE :
		info->b_mask = 1;
		/*blue color mask for left image in anagyph output mode o */
		info->g_mask = 0;
		/*green color mask for left image in anaglyph output mode o */
		info->r_mask = 0;
		/*red color mask for left image in anagyph output mode o */
		info->s3d_type = 4;
		break;
	case OUT_MODE_3D_RED_GREEN :
		info->b_mask = 0;
		/*blue color mask for left image in anagyph output mode o */
		info->g_mask = 0;
		/*green color mask for left image in anaglyph output mode o */
		info->r_mask = 0;
		/*red color mask for left image in anagyph output mode o */
		info->s3d_type = 4;
		break;
	case OUT_MODE_3D_SIDE_BY_SIDE :		break;
	case OUT_MODE_3D_TOP_BOTTOM :		break;
	default :
		ret = RET_ERROR;
	}


	switch (info_ptr->in.mode)
	{
	case IN_MODE_3D_SIDE_BY_SIDE :
		info->s3d_src_type = 0;
		break;
	case IN_MODE_3D_TOP_BOTTOM           :
		info->s3d_src_type = 1;
		break;
	default :
		ret = RET_ERROR;
		break;
	}

	if (bypass == 1)
		info->s3d_mode_en =0;/* s3d mode enable( 0 : disable,  1 : enable) */
	else
		info->s3d_mode_en =1;/* s3d mode enable( 0 : disable,  1 : enable) */

	info->bg_color =0x00ffff;/* background color (dummmy+green+blue+red) */
	info->fb2_bl_sel =2;
		/*frame buffer2 read burst length  2'h(0 : 8,1 : 16,2 : 32) */
	info->fb1_bl_sel =2;
		/*frame buffer1 read burst length  2'h(0 : 8,1 : 16,2 : 32) */
	info->fb0_bl_sel =2;
		/*frame buffer0 read burst length  2'h(0 : 8,1 : 16,2 : 32) */
	info->frame_width =info_ptr->out.w;
		/*frame width (total output size)  o  32 ~ 4808 */
	info->frame_height =info_ptr->out.h;
		/*frame height (total output size)  o  32 ~ 4808 */
	info->dma_dst0_addr =info_ptr->out.addr_p0;
		/*dma write destination0 address */
	info->dma_dst1_addr =info_ptr->out.addr_p1;
		/*dma write destination1 address */
	info->frate_con =info_ptr->out.frame_rate;
		/* 1 frame period / op. clock per */

	LOGI("\n ***** drv_core_init_disp_parm - end ***** \n");

	return ret;
}

/*===========================================================
FUNCTION     drv_core_init_fb_parm
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
=============================================================*/
int32 drv_core_init_fb_parm (int32 parm, void* parm1,
	struct drv_core_info *info)
{
  int32 ret=RET_OK;
  struct svc_interface_info *info_ptr = (struct svc_interface_info  *)parm1;

  LOGI("\n ***** drv_core_init_fb_parm - start ***** \n");

  info->fb0_pl0_addr0    =info_ptr->out.win[0].addr1_p0;
  info->fb0_pl1_addr0    =info_ptr->out.win[0].addr2_p0;
  info->fb0_pl2_addr0    =info_ptr->out.win[0].addr3_p0;
  info->fb0_pl0_addr1    =info_ptr->out.win[0].addr1_p1;
  info->fb0_pl1_addr1    =info_ptr->out.win[0].addr2_p1;
  info->fb0_pl2_addr1    =info_ptr->out.win[0].addr3_p1;
  info->fb0_hsize        =info_ptr->out.win[0].tw;
  info->fb0_vsize        =info_ptr->out.win[0].th;
  info->fb0_dcnt         =(info_ptr->out.win[0].tw * info_ptr->out.win[0].th);
  info->fb1_pl0_addr0    =info_ptr->out.win[1].addr1_p0;
  info->fb1_pl1_addr0    =info_ptr->out.win[1].addr2_p0;
  info->fb1_pl2_addr0    =info_ptr->out.win[1].addr3_p0;
  info->fb1_pl0_addr1    =info_ptr->out.win[1].addr1_p1;
  info->fb1_pl1_addr1    =info_ptr->out.win[1].addr2_p1;
  info->fb1_pl2_addr1    =info_ptr->out.win[1].addr3_p1;
  info->fb1_hsize        =info_ptr->out.win[1].tw;
  info->fb1_vsize        =info_ptr->out.win[1].th;
  info->fb1_dcnt         =(info_ptr->out.win[1].tw * info_ptr->out.win[1].th);
  info->fb2_pl0_addr0    =info_ptr->out.win[2].addr1_p0;
  info->fb2_pl0_addr1    =info_ptr->out.win[2].addr1_p1;
  info->fb2_hsize        =info_ptr->out.win[2].tw;
  info->fb2_vsize        =info_ptr->out.win[2].th;
  info->fb2_dcnt         =(info_ptr->out.win[2].tw * info_ptr->out.win[2].th);
  info->fb3_pl0_addr0    =info_ptr->out.win[3].addr1_p0;
  info->fb3_pl0_addr1    =info_ptr->out.win[3].addr1_p1;
  info->fb3_hsize        =info_ptr->out.win[3].tw;
  info->fb3_vsize        =info_ptr->out.win[3].th;
  info->fb3_dcnt         =(info_ptr->out.win[3].tw * info_ptr->out.win[3].th);

  LOGI("\n ***** drv_core_init_fb_parm - end ***** \n");

  return ret;
}



/*==========================================================
FUNCTION     drv_core_init_window_parm
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
============================================================*/
int32 drv_core_init_window_parm (int32 parm, void* parm1,
			struct drv_core_info *info)
{
	int32 ret=RET_OK;
	struct svc_interface_info *info_ptr = (struct svc_interface_info  *)parm1;
	const struct drv_in_format_info *out_format_ptr = &in_format_tbl[0];

	LOGI("\n ***** drv_core_init_window_parm - start ***** \n");

	/* window  setting ****************************************/
	info->win3_en          =0;
	info->win2_en          =0;
	info->win1_en          =0;
	info->win0_en          =1;

	info->win3_pri         =0;
	info->win2_pri         =0;
	info->win1_pri         =0;
	info->win0_pri         =1;


  /* window0 setting *****************************************/

	for ( ; out_format_ptr->format != IMG_FORMAT_MAX ; out_format_ptr++)
	{
		if ( out_format_ptr->format == info_ptr->out.win[0].format )
		{
			LOGI("WIN[0] FORMAT : %d \n", out_format_ptr->format);

			info->win0_packen      =out_format_ptr->pack_en ;
			info->win0_z_position  =out_format_ptr->z_position ;
			info->win0_d_order     =out_format_ptr->d_order ;
			info->win0_y420_mode   =out_format_ptr->y420_mode ;
			info->win0_y422_mode   =out_format_ptr->y422_mode ;
			info->win0_y_format    =out_format_ptr->y_format ;
			info->win0_rgb_16b     =out_format_ptr->rgb16_en ;
			info->win0_iy_en       =out_format_ptr->y_en ;

			break;
		}
	}

	info->win0_ab_sel = 0;
	info->win0_bg_ab_en = 0;
	info->win0_tp_mode = 0;
	info->win0_tp_en = 0;
	info->win0_ab_en = 0;

#if 1 /* color bar display */
	if ( info_ptr->out.color_enabled == 1 )
	{
		info->win0_xst = 0;/* info_ptr->out.win[0].sx ; */
		info->win0_yst = 0;/* info_ptr->out.win[0].sy ; */
		info->win0_hsize = 0;/* info_ptr->out.win[0].tw ; */
		info->win0_vsize = 0;/* info_ptr->out.win[0].th ; */
		info->win0_hcut = 0;/* info_ptr->out.win[0].w ; */
		info->win0_vcut = 0;/* info_ptr->out.win[0].h ; */
	}
	else
#endif
	{
		info->win0_xst = info_ptr->out.win[0].sx ;
		info->win0_yst = info_ptr->out.win[0].sy ;
		info->win0_hsize = info_ptr->out.win[0].tw ;
		info->win0_vsize = info_ptr->out.win[0].th ;
		info->win0_hcut = 0;/* wj@@ info_ptr->out.win[0].w; */
		info->win0_vcut = 0;/* wj@@ info_ptr->out.win[0].h; */
	}

	info->win0_color_key = 0;
	info->win0_ab_val = 0;
	info->win0_fil_more = 0;
	info->win0_fil_en_v = 1;
	info->win0_fil_en_h = 1;

#if 0 /* scaler */
	if ((info_ptr->out.win[0].tw == info_ptr->out.w) &&
		(info_ptr->out.win[0].th == info_ptr->out.h))
	{
		info->win0_scl_en     = 0;
		info->win0_ws_wd      = 0x100;
		info->win0_ws_wd      = 0x100;
	}
	else
	{
		LOGI("scaler enable win[0] \n");

		info->win0_scl_en        = 1;
		ret = get_scaler_factor(DRV_NULL, info_ptr->out.win[0].tw,
		info_ptr->out.win[0].th, info_ptr->out.w, info_ptr->out.h,
		&info->win0_ws_wd, &info->win0_ws_wd);
	}
#endif

	info->win0_scl_en = 0;
	info->win0_tws = info_ptr->out.win[0].tw ;
	info->win0_ths = info_ptr->out.win[0].th ;
	info->win0_ws_wd = 0x100;
	info->win0_hs_hd = 0x100;
	info->win0_x_off = 0;
	info->win0_y_off = 0;

  /* window1 setting ***************************************/

	for ( ; out_format_ptr->format != IMG_FORMAT_MAX ; out_format_ptr++)
	{
		if ( out_format_ptr->format == info_ptr->out.win[1].format )
		{
			LOGI("WIN[1] FORMAT : %d \n", out_format_ptr->format);

			info->win1_packen      =out_format_ptr->pack_en ;
			info->win1_z_position  =out_format_ptr->z_position ;
			info->win1_d_order     =out_format_ptr->d_order ;
			info->win1_y420_mode   =out_format_ptr->y420_mode ;
			info->win1_y422_mode   =out_format_ptr->y422_mode ;
			info->win1_y_format    =out_format_ptr->y_format ;
			info->win1_rgb_16b     =out_format_ptr->rgb16_en ;
			info->win1_iy_en       =out_format_ptr->y_en ;

			break;
		}
	}

	info->win1_ab_sel      =0;
	info->win1_bg_ab_en    =0;
	info->win1_tp_mode     =0;
	info->win1_tp_en       =0;
	info->win1_ab_en       =0;

	info->win1_xst         =info_ptr->out.win[1].sx ;
	info->win1_yst         =info_ptr->out.win[1].sy ;
	info->win1_hsize       =info_ptr->out.win[1].tw ;
	info->win1_vsize       =info_ptr->out.win[1].th ;
	info->win1_hcut        =info_ptr->out.win[1].w ;
	info->win1_vcut        =info_ptr->out.win[1].h ;

	info->win1_color_key   =0;

	info->win1_ab_val      =0;

	info->win1_fil_more    =0;
	info->win1_fil_en_v    =0;
	info->win1_fil_en_h    =0;
	info->win1_scl_en      =0;

	info->win1_tws         =info_ptr->out.win[1].tw ;
	info->win1_ths         =info_ptr->out.win[1].th ;
	info->win1_ws_wd       =0x100;
	info->win1_hs_hd       =0x100;
	info->win1_x_off       =0;
	info->win1_y_off       =0;


  /* window2 setting ******************************************/

	for ( ; out_format_ptr->format != IMG_FORMAT_MAX ; out_format_ptr++)
	{
		if ( out_format_ptr->format == info_ptr->out.win[2].format )
		{
			LOGI("WIN[2] FORMAT : %d \n", out_format_ptr->format);

			info->win2_packen      =out_format_ptr->pack_en ;
			info->win2_z_position  =out_format_ptr->z_position;
			info->win2_d_order     =out_format_ptr->d_order ;
			info->win2_rgb_16b     =out_format_ptr->rgb16_en ;

			break;
		}
	}

	info->win2_ab_sel      =0;
	info->win2_bg_ab_en    =0;
	info->win2_tp_mode     =0;
	info->win2_tp_en       =0;
	info->win2_ab_en       =0;

	info->win2_xst         =info_ptr->out.win[2].sx ;
	info->win2_yst         =info_ptr->out.win[2].sy ;
	info->win2_hsize       =info_ptr->out.win[2].tw ;
	info->win2_vsize       =info_ptr->out.win[2].th ;
	info->win2_hcut        =info_ptr->out.win[2].w ;
	info->win2_vcut        =info_ptr->out.win[2].h ;

	info->win2_color_key   =0;

	info->win2_ab_val      =0;

	info->win2_fil_more    =0;
	info->win2_fil_en_v    =0;
	info->win2_fil_en_h    =0;
	info->win2_scl_en      =0;

	info->win2_tws         =info_ptr->out.win[2].tw ;
	info->win2_ths         =info_ptr->out.win[2].th ;
	info->win2_ws_wd       =0x100;
	info->win2_hs_hd       =0x100;
	info->win2_x_off       =0;
	info->win2_y_off       =0;


  /* window3 setting *****************************************/

	for ( ; out_format_ptr->format != IMG_FORMAT_MAX ; out_format_ptr++)
	{
		if ( out_format_ptr->format == info_ptr->out.win[3].format )
		{
			LOGI("WIN[3] FORMAT : %d \n", out_format_ptr->format);

			info->win3_packen      =out_format_ptr->pack_en ;
			info->win3_z_position  =out_format_ptr->z_position;
			info->win3_d_order     =out_format_ptr->d_order ;
			info->win3_rgb_16b     =out_format_ptr->rgb16_en ;

			break;
		}
	}

	info->win3_ab_sel      =0;
	info->win3_bg_ab_en    =0;
	info->win3_tp_mode     =0;
	info->win3_tp_en       =0;
	info->win3_ab_en       =0;

	info->win3_xst         =info_ptr->out.win[3].sx ;
	info->win3_yst         =info_ptr->out.win[3].sy ;
	info->win3_hsize       =info_ptr->out.win[3].tw ;
	info->win3_vsize       =info_ptr->out.win[3].th ;
	info->win3_hcut        =info_ptr->out.win[3].w ;
	info->win3_vcut        =info_ptr->out.win[3].h ;

	info->win3_color_key   =0;

	info->win3_ab_val      =0;

	info->win3_fil_more    =0;
	info->win3_fil_en_v    =0;
	info->win3_fil_en_h    =0;
	info->win3_scl_en      =0;

	info->win3_tws         =info_ptr->out.win[3].tw ;
	info->win3_ths         =info_ptr->out.win[3].th ;
	info->win3_ws_wd       =0x100;
	info->win3_hs_hd       =0x100;
	info->win3_x_off       =0;
	info->win3_y_off       =0;

	LOGI("\n ***** drv_core_init_window_parm - end ***** \n");

	return ret;
}



/*===================================================
FUNCTION     get_scaler_factor
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
=====================================================*/
int32 get_scaler_factor(void* base, int32 src_w, int32 src_h, int32 dst_w,
					int32 dst_h, int16 * scale_w, int16 * scale_h)
{
	int32 ret=RET_OK;
	int32 ratio_w, ratio_h;
/*	float wx, hx;	*/

#if 1
	ratio_w = (src_w * 256) / (dst_w);
	ratio_h = (src_h * 256) / (dst_h);
#else /* round up the number  */
	ratio_w = ((src_w * 256 *2) / (dst_w) + 1)/2;
	ratio_h = ((src_h * 256 *2) / (dst_h) + 1)/2;
#endif

#if 0
#ifdef FEATURE_BUILD_WIN32 /*for debugging */
	if (src_w > dst_w)
		wx = ((float)src_w / (float)dst_w);
	else
		wx = ((float)dst_w / (float)src_w);

	if (src_h > dst_h)
		hx = ((float)src_h / (float)dst_h);
	else
		hx = ((float)dst_h / (float)src_h);

	LOGI("SCALE(W:%.2f,H:%.2f):(%dx%d)=>(%dx%d),{ws_wd:0x%x, hs_hd:0x%x} \n",
	wx,hx,src_w,src_h,dst_w,dst_h,ratio_w,ratio_h);
#endif
#endif

#if 1 /* 抗寇贸府 - 坷瞒 贸府	*/
	if (ratio_w == 0x100)
	{
		if (src_w > dst_w)
			ratio_w++;
		else
			if (src_w < dst_w)
			ratio_w--;
	}

	if (ratio_h == 0x100)
	{
		if (src_h > dst_h)
			ratio_h++;
		else
			if (src_h < dst_h)
			ratio_h--;
	}
#endif

	*scale_w           = (int16)ratio_w;
	*scale_h           = (int16)ratio_h;

#if 0
#ifdef FEATURE_BUILD_WIN32
	if (wx>8 || hx>8)
	ret = RET_ERROR;
#endif
#endif

  return ret;
}


/*==========================================================
FUNCTION     drv_core_init_disp
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
============================================================*/
int32 drv_core_init_disp (int32 parm, void* parm1, struct drv_core_info *info)
{
	int32 ret=RET_OK;
/*	struct svc_interface_info *info_ptr =
	(struct svc_interface_info *)parm1; */

	LOGF("\n ***** drv_core_init_disp - start ***** \n");


	/* oooo0  setting ********************************************/
  set_reg16(DRV_NULL, OUTPUT_CON,
      (
          ((info->test_en1         & BIT_MASK1 ) << 13)
      |   ((info->test_en0         & BIT_MASK1 ) << 12)
      |   ((info->start_vpol       & BIT_MASK1 ) << 11)
      |   ((info->start_mode2      & BIT_MASK1 ) << 10)
      |   ((info->dma_od_order     & BIT_MASK3 ) << 8)
      |   ((info->dma_oz_position  & BIT_MASK1 ) << 7)
      |   ((info->dma_od_format    & BIT_MASK3 ) << 4)
      |   ((info->fifo_od_format   & BIT_MASK1 ) << 3)
      |   ((info->start_mode       & BIT_MASK1 ) << 2)
      |   ((info->dma_if_en        & BIT_MASK1 ) << 1)
      |   ((info->fifo_if_en       & BIT_MASK1 ) << 0)
      )
      );

#if 0/* reserved */
  set_reg16(DRV_NULL, FRAME_ST,
      (
          ((info->frame_start   & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, SHADOW_UP,
      (
          ((info->shadow_update & BIT_MASK1 ) << 0)
      )
      );
#endif

  set_reg16(DRV_NULL, FB_RD_PAGE,
      (
          ((info->fb3_rpage     & BIT_MASK1 ) << 3)
      |   ((info->fb2_rpage     & BIT_MASK1 ) << 2)
      |   ((info->fb1_rpage     & BIT_MASK1 ) << 1)
      |   ((info->fb0_rpage     & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, DE_FORMAT_CON,
      (
          ((info->df_dst_type   & BIT_MASK3 ) << 2)
      |   ((info->df_src_type   & BIT_MASK1 ) << 1)
      |   ((info->deformat_en   & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, ANAG_CMASK,
      (
          ((info->b_mask        & BIT_MASK1 ) << 2)
      |   ((info->g_mask        & BIT_MASK1 ) << 1)
      |   ((info->r_mask        & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, S3D_CON,
      (
          ((info->s3d_src_type  & BIT_MASK1 ) << 4)
      |   ((info->s3d_type      & BIT_MASK3 ) << 1)
      |   ((info->s3d_mode_en   & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, BG_COLOR_H, (info->bg_color & BIT_MASK16)   >>  0 );
  set_reg16(DRV_NULL, BG_COLOR_L, (info->bg_color & BIT_MASK_H8)  >>  16);
  set_reg16(DRV_NULL, BL_SEL,
      (
          ((info->fb2_bl_sel    & BIT_MASK2 ) << 8)
      |   ((info->fb1_bl_sel    & BIT_MASK2 ) << 4)
      |   ((info->fb0_bl_sel    & BIT_MASK2 ) << 0)
      )
      );

	set_reg16(DRV_NULL, FRAME_WIDTH, (info->frame_width & BIT_MASK16) >>  0 );
	set_reg16(DRV_NULL, FRAME_HEIGHT, (info->frame_height & BIT_MASK16) >>  0 );

	set_reg16(DRV_NULL, DMA_DST0_ADDR_L,
		(info->dma_dst0_addr & BIT_MASK16) >> 0 );
	set_reg16(DRV_NULL, DMA_DST0_ADDR_H,
		(info->dma_dst0_addr & BIT_MASK_H16) >> 16);

	set_reg16(DRV_NULL, DMA_DST1_ADDR_L,
		(info->dma_dst1_addr & BIT_MASK16) >> 0 );
	set_reg16(DRV_NULL, DMA_DST1_ADDR_H,
		(info->dma_dst1_addr & BIT_MASK_H16) >> 16);

	set_reg16(DRV_NULL, FRATE_CON_L, (info->frate_con & BIT_MASK16) >>  0 );
	set_reg16(DRV_NULL, FRATE_CON_H, (info->frate_con & BIT_MASK_H16) >>  16);

	LOGF("\n ***** drv_core_init_disp - end ***** \n");

  return ret;
}



/*===========================================================
FUNCTION     drv_core_init_window
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
=============================================================*/
int32 drv_core_init_window  (int32 parm, void* parm1,
	struct drv_core_info *info)
{
  int32 ret=RET_OK;
/*  struct svc_interface_info *info_ptr =
	(struct svc_interface_info  *)parm1; */

	LOGF("\n ***** drv_core_init_window - start ***** \n");


  /* window  setting **********************************/
  set_reg16(DRV_NULL, WIN_EN,
      (
          ((info->win3_en     & BIT_MASK1 ) << 3)
      |   ((info->win2_en     & BIT_MASK1 ) << 2)
      |   ((info->win1_en     & BIT_MASK1 ) << 1)
      |   ((info->win0_en     & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, WIN_PRI,
      (
          ((info->win3_pri    & BIT_MASK2 ) << 6)
      |   ((info->win2_pri    & BIT_MASK2 ) << 4)
      |   ((info->win1_pri    & BIT_MASK2 ) << 2)
      |   ((info->win0_pri    & BIT_MASK2 ) << 0)
      )
      );

  /* window0 setting **********************************/
  set_reg16(DRV_NULL, WIN0_CON,
      (
          ((info->win0_packen      & BIT_MASK1 ) << 13)
      |   ((info->win0_z_position  & BIT_MASK1 ) << 12)
      |   ((info->win0_d_order     & BIT_MASK1 ) << 11)
      |   ((info->win0_y420_mode   & BIT_MASK2 ) << 9)
      |   ((info->win0_y422_mode   & BIT_MASK1 ) << 8)
      |   ((info->win0_y_format    & BIT_MASK2 ) << 6)
      |   ((info->win0_rgb_16b     & BIT_MASK1 ) << 5)
      |   ((info->win0_iy_en       & BIT_MASK1 ) << 4)
      )
      );

  set_reg16(DRV_NULL, WIN0_AB_CON,
      (
          ((info->win0_ab_sel       & BIT_MASK1 ) << 4)
      |   ((info->win0_bg_ab_en     & BIT_MASK1 ) << 3)
      |   ((info->win0_tp_mode      & BIT_MASK1 ) << 2)
      |   ((info->win0_tp_en        & BIT_MASK1 ) << 1)
      |   ((info->win0_ab_en        & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, WIN0_XST , (info->win0_xst & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN0_YST , (info->win0_yst & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN0_HSIZE, (info->win0_hsize & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN0_VSIZE, (info->win0_vsize & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN0_HCUT , (info->win0_hcut  & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN0_VCUT , (info->win0_vcut  & BIT_MASK13) >> 0);


	set_reg16(DRV_NULL, WIN0_CKEY_L , (info->win0_color_key & BIT_MASK16)  >> 0);
	set_reg16(DRV_NULL, WIN0_CKEY_H , (info->win0_color_key & BIT_MASK_H8) >> 16);
	set_reg16(DRV_NULL, WIN0_AB_VAL , (info->win0_ab_val  & BIT_MASK8)   >> 0);

	set_reg16(DRV_NULL, WIN0_SCL_CON,
	(	((info->win0_fil_more     & BIT_MASK1 ) << 3)
	|   ((info->win0_fil_en_v     & BIT_MASK1 ) << 2)
	|   ((info->win0_fil_en_h     & BIT_MASK1 ) << 1)
	|   ((info->win0_scl_en       & BIT_MASK1 ) << 0)
	));

	set_reg16(DRV_NULL, WIN0_TWS , (info->win0_tws  & BIT_MASK13)   >> 0);
	set_reg16(DRV_NULL, WIN0_THS , (info->win0_ths  & BIT_MASK13)   >> 0);
	set_reg16(DRV_NULL, WIN0_WS_WD , (info->win0_ws_wd & BIT_MASK16)   >> 0);
	set_reg16(DRV_NULL, WIN0_HS_HD , (info->win0_hs_hd & BIT_MASK16)   >> 0);
	set_reg16(DRV_NULL, WIN0_X_OFF , (info->win0_x_off & BIT_MASK13)   >> 0);
	set_reg16(DRV_NULL, WIN0_Y_OFF , (info->win0_y_off & BIT_MASK13)   >> 0);

#if 0
  /* window1 setting ******************************************/
  set_reg16(DRV_NULL, WIN1_CON,
      (
          ((info->win1_packen      & BIT_MASK1 ) << 13)
      |   ((info->win1_z_position  & BIT_MASK1 ) << 12)
      |   ((info->win1_d_order     & BIT_MASK1 ) << 11)
      |   ((info->win1_y420_mode   & BIT_MASK2 ) << 9)
      |   ((info->win1_y422_mode   & BIT_MASK1 ) << 8)
      |   ((info->win1_y_format    & BIT_MASK2 ) << 6)
      |   ((info->win1_rgb_16b     & BIT_MASK1 ) << 5)
      |   ((info->win1_iy_en       & BIT_MASK1 ) << 4)
      )
      );

  set_reg16(DRV_NULL, WIN1_AB_CON,
      (
          ((info->win1_ab_sel       & BIT_MASK1 ) << 4)
      |   ((info->win1_bg_ab_en     & BIT_MASK1 ) << 3)
      |   ((info->win1_tp_mode      & BIT_MASK1 ) << 2)
      |   ((info->win1_tp_en        & BIT_MASK1 ) << 1)
      |   ((info->win1_ab_en        & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, WIN1_XST  , (info->win1_xst & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN1_YST  , (info->win1_yst & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN1_HSIZE, (info->win1_hsize & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN1_VSIZE, (info->win1_vsize & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN1_HCUT , (info->win1_hcut  & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN1_VCUT , (info->win1_vcut  & BIT_MASK13) >> 0);

  set_reg16(DRV_NULL, WIN1_CKEY_L ,
  	(info->win1_color_key & BIT_MASK16)  >> 0);
  set_reg16(DRV_NULL, WIN1_CKEY_H , (info->win1_color_key & BIT_MASK_H8) >> 16);

  set_reg16(DRV_NULL, WIN1_AB_VAL , (info->win1_ab_val   & BIT_MASK8)   >> 0);

  set_reg16(DRV_NULL, WIN1_SCL_CON,
      (
          ((info->win1_fil_more     & BIT_MASK1 ) << 3)
      |   ((info->win1_fil_en_v     & BIT_MASK1 ) << 2)
      |   ((info->win1_fil_en_h     & BIT_MASK1 ) << 1)
      |   ((info->win1_scl_en       & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, WIN1_TWS , (info->win1_tws  & BIT_MASK13)   >> 0);
  set_reg16(DRV_NULL, WIN1_THS , (info->win1_ths  & BIT_MASK13)   >> 0);
  set_reg16(DRV_NULL, WIN1_WS_WD , (info->win1_ws_wd & BIT_MASK16) >> 0);
  set_reg16(DRV_NULL, WIN1_HS_HD , (info->win1_hs_hd & BIT_MASK16)   >> 0);
  set_reg16(DRV_NULL, WIN1_X_OFF , (info->win1_x_off & BIT_MASK13)   >> 0);
  set_reg16(DRV_NULL, WIN1_Y_OFF , (info->win1_y_off & BIT_MASK13)   >> 0);

  /* window2 setting **********************************************/
  set_reg16(DRV_NULL, WIN2_CON,
      (
          ((info->win2_packen      & BIT_MASK1 ) << 13)
      |   ((info->win2_z_position  & BIT_MASK1 ) << 12)
      |   ((info->win2_d_order     & BIT_MASK1 ) << 11)
      |   ((info->win2_rgb_16b     & BIT_MASK1 ) << 5)
      )
      );

  set_reg16(DRV_NULL, WIN2_AB_CON,
      (
          ((info->win2_ab_sel       & BIT_MASK1 ) << 4)
      |   ((info->win2_bg_ab_en     & BIT_MASK1 ) << 3)
      |   ((info->win2_tp_mode      & BIT_MASK1 ) << 2)
      |   ((info->win2_tp_en        & BIT_MASK1 ) << 1)
      |   ((info->win2_ab_en        & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, WIN2_XST  , (info->win2_xst   & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN2_YST  , (info->win2_yst   & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN2_HSIZE, (info->win2_hsize & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN2_VSIZE, (info->win2_vsize & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN2_HCUT , (info->win2_hcut  & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN2_VCUT , (info->win2_vcut  & BIT_MASK13) >> 0);

  set_reg16(DRV_NULL, WIN2_CKEY_L ,
	(info->win2_color_key & BIT_MASK16)  >> 0);
  set_reg16(DRV_NULL, WIN2_CKEY_H ,
  	(info->win2_color_key & BIT_MASK_H8) >> 16);

  set_reg16(DRV_NULL, WIN2_AB_VAL , (info->win2_ab_val  & BIT_MASK8)   >> 0);

  set_reg16(DRV_NULL, WIN2_SCL_CON,
      (
          ((info->win2_fil_more     & BIT_MASK1 ) << 3)
      |   ((info->win2_fil_en_v     & BIT_MASK1 ) << 2)
      |   ((info->win2_fil_en_h     & BIT_MASK1 ) << 1)
      |   ((info->win2_scl_en       & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, WIN2_TWS  , (info->win2_tws & BIT_MASK13)   >> 0);
  set_reg16(DRV_NULL, WIN2_THS  , (info->win2_ths & BIT_MASK13)   >> 0);
  set_reg16(DRV_NULL, WIN2_WS_WD , (info->win2_ws_wd  & BIT_MASK16)   >> 0);
  set_reg16(DRV_NULL, WIN2_HS_HD , (info->win2_hs_hd   & BIT_MASK16)   >> 0);
  set_reg16(DRV_NULL, WIN2_X_OFF , (info->win2_x_off  & BIT_MASK13)   >> 0);
  set_reg16(DRV_NULL, WIN2_Y_OFF , (info->win2_y_off  & BIT_MASK13)   >> 0);

  /* window3 setting **************************************************/
  set_reg16(DRV_NULL, WIN3_CON,
      (
          ((info->win3_packen      & BIT_MASK1 ) << 13)
      |   ((info->win3_z_position  & BIT_MASK1 ) << 12)
      |   ((info->win3_d_order     & BIT_MASK1 ) << 11)
      |   ((info->win3_rgb_16b     & BIT_MASK1 ) << 5)
      )
      );

  set_reg16(DRV_NULL, WIN3_AB_CON,
      (
          ((info->win3_ab_sel       & BIT_MASK1 ) << 4)
      |   ((info->win3_bg_ab_en     & BIT_MASK1 ) << 3)
      |   ((info->win3_tp_mode      & BIT_MASK1 ) << 2)
      |   ((info->win3_tp_en        & BIT_MASK1 ) << 1)
      |   ((info->win3_ab_en        & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, WIN3_XST  , (info->win3_xst & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN3_YST  , (info->win3_yst & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN3_HSIZE, (info->win3_hsize & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN3_VSIZE, (info->win3_vsize & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN3_HCUT , (info->win3_hcut  & BIT_MASK13) >> 0);
  set_reg16(DRV_NULL, WIN3_VCUT , (info->win3_vcut  & BIT_MASK13) >> 0);

  set_reg16(DRV_NULL, WIN3_CKEY_L ,
  	(info->win3_color_key & BIT_MASK16) >> 0);
  set_reg16(DRV_NULL, WIN3_CKEY_H ,
  	(info->win3_color_key & BIT_MASK_H8) >> 16);

  set_reg16(DRV_NULL, WIN3_AB_VAL , (info->win3_ab_val  & BIT_MASK8)   >> 0);

  set_reg16(DRV_NULL, WIN3_SCL_CON,
      (
          ((info->win3_fil_more     & BIT_MASK1 ) << 3)
      |   ((info->win3_fil_en_v     & BIT_MASK1 ) << 2)
      |   ((info->win3_fil_en_h     & BIT_MASK1 ) << 1)
      |   ((info->win3_scl_en       & BIT_MASK1 ) << 0)
      )
      );

  set_reg16(DRV_NULL, WIN3_TWS  , (info->win3_tws  & BIT_MASK13)   >> 0);
  set_reg16(DRV_NULL, WIN3_THS  , (info->win3_ths  & BIT_MASK13)   >> 0);
  set_reg16(DRV_NULL, WIN3_WS_WD , (info->win3_ws_wd & BIT_MASK16)   >> 0);
  set_reg16(DRV_NULL, WIN3_HS_HD , (info->win3_hs_hd & BIT_MASK16)   >> 0);
  set_reg16(DRV_NULL, WIN3_X_OFF , (info->win3_x_off & BIT_MASK13)   >> 0);
  set_reg16(DRV_NULL, WIN3_Y_OFF , (info->win3_y_off & BIT_MASK13)   >> 0);
#endif

	LOGF("\n ***** drv_core_init_window - end ***** \n");

	return ret;
}




/*============================================================
FUNCTION     drv_core_init_fb
DESCRIPTION
DEPENDENCIES
RETURN VALUE
SIDE EFFECTS
=============================================================*/
int32 drv_core_init_fb (int32 parm, void* parm1, struct drv_core_info *info)
{
	int32 ret=RET_OK;
/*	struct svc_interface_info *info_ptr =
	(struct svc_interface_info  *)parm1; */

	LOGF("\n ***** drv_core_init_fb - start ***** \n");

	set_reg16(DRV_NULL, FB0_PL0_ADDR0_L,
		(info->fb0_pl0_addr0     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB0_PL0_ADDR0_H,
		(info->fb0_pl0_addr0     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB0_PL1_ADDR0_L,
		(info->fb0_pl1_addr0     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB0_PL1_ADDR0_H,
		(info->fb0_pl1_addr0     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB0_PL2_ADDR0_L,
		(info->fb0_pl2_addr0     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB0_PL2_ADDR0_H,
		(info->fb0_pl2_addr0     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB0_PL0_ADDR1_L,
		(info->fb0_pl0_addr1     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB0_PL0_ADDR1_H,
		(info->fb0_pl0_addr1     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB0_PL1_ADDR1_L,
		(info->fb0_pl1_addr1     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB0_PL1_ADDR1_H,
		(info->fb0_pl1_addr1     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB0_PL2_ADDR1_L,
		(info->fb0_pl2_addr1     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB0_PL2_ADDR1_H,
		(info->fb0_pl2_addr1     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB0_HSIZE      ,
		(info->fb0_hsize         & BIT_MASK13)   >>  0 );
	set_reg16(DRV_NULL, FB0_VSIZE      ,
		(info->fb0_vsize         & BIT_MASK13)   >>  0 );
	set_reg16(DRV_NULL, FB0_DCNT_L     ,
		(info->fb0_dcnt          & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB0_DCNT_H     ,
		(info->fb0_dcnt          & BIT_MASK_H9)  >>  16);
	set_reg16(DRV_NULL, FB1_PL0_ADDR0_L,
		(info->fb1_pl0_addr0     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB1_PL0_ADDR0_H,
		(info->fb1_pl0_addr0     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB1_PL1_ADDR0_L,
		(info->fb1_pl1_addr0     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB1_PL1_ADDR0_H,
		(info->fb1_pl1_addr0     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB1_PL2_ADDR0_L,
		(info->fb1_pl2_addr0     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB1_PL2_ADDR0_H,
		(info->fb1_pl2_addr0     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB1_PL0_ADDR1_L,
		(info->fb1_pl0_addr1     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB1_PL0_ADDR1_H,
		(info->fb1_pl0_addr1     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB1_PL1_ADDR1_L,
		(info->fb1_pl1_addr1     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB1_PL1_ADDR1_H,
		(info->fb1_pl1_addr1     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB1_PL2_ADDR1_L,
		(info->fb1_pl2_addr1     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB1_PL2_ADDR1_H,
		(info->fb1_pl2_addr1     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB1_HSIZE      ,
		(info->fb1_hsize         & BIT_MASK13)   >>  0 );
	set_reg16(DRV_NULL, FB1_VSIZE      ,
		(info->fb1_vsize         & BIT_MASK13)   >>  0 );
	set_reg16(DRV_NULL, FB1_DCNT_L     ,
		(info->fb1_dcnt          & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB1_DCNT_H     ,
		(info->fb1_dcnt          & BIT_MASK_H9)  >>  16);
	set_reg16(DRV_NULL, FB2_PL0_ADDR0_L,
		(info->fb2_pl0_addr0     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB2_PL0_ADDR0_H,
		(info->fb2_pl0_addr0     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB2_PL0_ADDR1_L,
		(info->fb2_pl0_addr1     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB2_PL0_ADDR1_H,
		(info->fb2_pl0_addr1     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB2_HSIZE      ,
		(info->fb2_hsize         & BIT_MASK13)   >>  0 );
	set_reg16(DRV_NULL, FB2_VSIZE      ,
		(info->fb2_vsize         & BIT_MASK13)   >>  0 );
	set_reg16(DRV_NULL, FB2_DCNT_L     ,
		(info->fb2_dcnt          & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB2_DCNT_H     ,
		(info->fb2_dcnt          & BIT_MASK_H9)  >>  16);
	set_reg16(DRV_NULL, FB3_PL0_ADDR0_L,
		(info->fb3_pl0_addr0     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB3_PL0_ADDR0_H,
		(info->fb3_pl0_addr0     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB3_PL0_ADDR1_L,
		(info->fb3_pl0_addr1     & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB3_PL0_ADDR1_H,
		(info->fb3_pl0_addr1     & BIT_MASK_H16) >>  16);
	set_reg16(DRV_NULL, FB3_HSIZE      ,
		(info->fb3_hsize         & BIT_MASK13)   >>  0 );
	set_reg16(DRV_NULL, FB3_VSIZE      ,
		(info->fb3_vsize         & BIT_MASK13)   >>  0 );
	set_reg16(DRV_NULL, FB3_DCNT_L     ,
		(info->fb3_dcnt          & BIT_MASK16)   >>  0 );
	set_reg16(DRV_NULL, FB3_DCNT_H     ,
		(info->fb3_dcnt          & BIT_MASK_H9)  >>  16);

	LOGF("\n ***** drv_core_init_fb - end ***** \n");

  return ret;
}
