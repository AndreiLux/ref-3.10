/*
 * linux/drivers/video/odin/s3d/drv_mepsi.h
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

#ifndef __DRV_MEPSI_H__
#define __DRV_MEPSI_H__

#include "drv_customer.h"

/*-------------------------------------------------------
  Input Mode
--------------------------------------------------------*/
enum in_mode_type{
	IN_MODE_NULL                      = 0,
	IN_MODE_2D_CAM0                   = 1,
	IN_MODE_2D_CAM1                   = 2,
	IN_MODE_3D_SIDE_BY_SIDE           = 3,
	IN_MODE_3D_TOP_BOTTOM             = 4,

	IN_MODE_MAX                       = 0x7fff,
};


/*-------------------------------------------------------
  Output Mode
--------------------------------------------------------*/
enum out_mode_type{
	OUT_MODE_2D_NULL                 = 0,
	OUT_MODE_2D_CAM0                 = 1,
	OUT_MODE_2D_CAM1                 = 2,
	OUT_MODE_3D_PIXEL_BASE           = 3,
	OUT_MODE_3D_SUBPIXEL_BASE        = 4,
	OUT_MODE_3D_LINE_BASE            = 5,
	OUT_MODE_3D_TIME_DIV             = 6,
	OUT_MODE_3D_RED_BLUE             = 7,
	OUT_MODE_3D_RED_GREEN            = 8,
	OUT_MODE_3D_SIDE_BY_SIDE         = 9,
	OUT_MODE_3D_TOP_BOTTOM           = 10,
	OUT_MODE_TEST_SCREEN             = 11,

	OUT_MODE_MAX                     = 0x7fff,
};


/*-------------------------------------------------------
  IMAGE FORMAT
--------------------------------------------------------*/
enum mepsi_img_format{
	IMG_FORMAT_NULL                  =0,

	IMG_YUV444_1P_UNPACKED_VUYZ      =1,
	IMG_YUV444_1P_UNPACKED_ZVUY      =2,
	IMG_YUV444_1P_UNPACKED_UVYZ      =3,
	IMG_YUV444_1P_UNPACKED_ZUVY      =4,
	IMG_YUV444_1P_UNPACKED_ZYUV      =5, /* MEPSI.F ADDED*/
	IMG_YUV444_1P_UNPACKED_ZYVU      =6, /* MEPSI.F ADDED*/
	IMG_YUV444_1P_UNPACKED_YUVZ      =7, /* MEPSI.F ADDED*/
	IMG_YUV444_1P_UNPACKED_YVUZ      =8, /* MEPSI.F ADDED*/

	IMG_YUV444_1P_PACKED_VUY         =9,
	IMG_YUV444_1P_PACKED_UVY         =10,
	IMG_YUV444_1P_PACKED_YVU         =11, /* MEPSI.F ADDED*/
	IMG_YUV444_1P_PACKED_YUV         =12, /* MEPSI.F ADDED*/
	IMG_YUV422_1P_PACKED_VYUY        =13,
	IMG_YUV422_1P_PACKED_UYVY        =14,
	IMG_YUV422_1P_PACKED_YVYU        =15,
	IMG_YUV422_1P_PACKED_YUYV        =16,
	IMG_YUV422_2P_PLANER_VU          =17,
	IMG_YUV422_2P_PLANER_UV          =18,
	IMG_RGB8888_RGBZ                 =19,
	IMG_RGB8888_ZRGB                 =20,
	IMG_RGB8888_BGRZ                 =21,
	IMG_RGB8888_ZBGR                 =22,
	IMG_RGB888_RGB                   =23,
	IMG_RGB888_BGR                   =24,
	IMG_RGB565_RGB                   =25,
	IMG_RGB565_BGR                   =26,
	IMG_YUV420_IMC1                  =27,
	IMG_YUV420_IMC3                  =28,
	IMG_YUV420_IMC2                  =29,
	IMG_YUV420_IMC4                  =30,
	IMG_YUV420_YV12                  =31,
	IMG_YUV420_YV21                  =32,
	IMG_YUV420_NV12                  =33,
	IMG_YUV420_NV21                  =34,

	IMG_FORMAT_MAX                   =0x7fff,
};
#endif
