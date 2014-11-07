/*
 * ISP interface driver
 *
 * Copyright (C) 2010 - 2013 Mtekvision
 * Author: DongHyung Ko <kodh@mtekvision.com>
 *         Jinyoung Park <parkjyb@mtekvision.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ODIN_ISPGENERAL_H__
#define __ODIN_ISPGENERAL_H__

/**
@brief
  RESOLUTION TYPE DEFINITION
*/
struct res_size {
	int width;
	int height;
	int cmp;
};

enum {
	OFF = 0,
	ON
};

enum {
	SUCCESS = 0,
	FAIL = -1
};


enum {
	R_CH = 0,
	G_CH,
	B_CH
};

#define RGB_CH_CNT	  (B_CH + 1)


/* Sensor Resolution Related Registers */
/*
Megapixel			Image				Print Size,	Screen Size
					Resolution			240 dpi
----------------------------------------------------------------------
0.3 				640 x 480			2x3 			VGA
0.8 				1024 x 768			3x4				XVGA
1					1366 x 768			3x5				WXVGA
1.3 				1280 x 1024 		4x5 			SXVGA
2					1600 x 1200			5x6				UXVGA
3.2 				2048 x 1536			6x8 			WQXGA
4					2560 x 1600			6x10			QSXGA
5					2560 x 1920			8x10			QSXGA
6					3000 x 2000			8x12			-
7					3000 x 2300			10x12			-
8					3264 x 2448			10x13			-
9					3456 x 2592			11x14			WQUXGA
10					3648 x 2736			11x15			-
12					4000 x 3000			12x17			-
*/
enum {
	MODE_CHG_INIT = 0,
	MODE_QQVGA,
	MODE_QCIF,
	MODE_QVGA,
	MODE_CIF,
	MODE_VGA,
	MODE_SVGA,
	MODE_XGA,
	MODE_XGA_PLUS,
	MODE_HD,
	MODE_QUADVGA,
	MODE_SXGA,
	MODE_UXGA,
	MODE_QXGA,
	MODE_4M,
	MODE_QSXGA,
	MODE_8MEGA,
	MODE_CNT
};

enum {
	BAYER_RAW8	= 0,
	BAYER_RAW10,
	BAYER_RAW12,
	BAYER_DPCM,
};

#define CAMERA_FORMAT_BAYER_RAW8			0x00
#define CAMERA_FORMAT_BAYER_RAW10			0x01
#define CAMERA_FORMAT_BAYER_RAW12			0x02
#define CAMERA_FORMAT_BAYER_DPCM10			0x03
#define CAMERA_FORMAT_YUV422				0x10
#define CAMERA_FORMAT_YUV420				0x11
#define CAMERA_FORMAT_RGB565				0x20
#define CAMERA_FORMAT_RGB888				0x21
#define CAMERA_FORMAT_CCIR					0x30
#define CAMERA_FORMAT_JPEG					0x40

/**
@brief
  MTEK ISP RELATED DEFINITIONs
*/

#define CAM_PIXORDER_BAYER_RG		0
#define CAM_PIXORDER_BAYER_GR		1
#define CAM_PIXORDER_BAYER_GB		2
#define CAM_PIXORDER_BAYER_BG		3

#define CAM_PIXORDER_YC_CBYCRY		0
#define CAM_PIXORDER_YC_YCBYCR		1
#define CAM_PIXORDER_YC_CRYCBY		2
#define CAM_PIXORDER_YC_YCRYCB		3

#define LINUX_V4L_FILENAME			"/dev/video"

struct camera_input_desc {
	unsigned int marginx, marginy;
	unsigned int sizex, sizey;
	unsigned int format;	/* BAYER, YUV, CCIR */
	unsigned int align;
};

struct camera_output_desc {
	unsigned int offsetx, offsety;
	unsigned int maskx, masky;
	unsigned int format;	/* YUV422, YUV420, BAYER, JPEG(with JPEG Block) */
	unsigned int align;
};


#define DIFFERENCE(x,y) ((x>y)? (x-y):(y-x))


#endif /* __ODIN_ISPGENERAL_H__ */
