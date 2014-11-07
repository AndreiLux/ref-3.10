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

#ifndef __ODIN_ISP_WRAP_H__
#define __ODIN_ISP_WRAP_H__


/******************************************************************************
	Local Literals & Definitions
******************************************************************************/
/*	AsyncFIFO Register Offsets	*/
#define OFFSET_ASYNCBUF_CTRL				0x00000000
#define OFFSET_ASYNCBUF_YUV_PATHCTRL		0x00000004
#define OFFSET_ASYNCBUF_BAYER_PRE_PATHCTRL	0x00000008
#define OFFSET_ASYNCBUF_BAYER_POST_PATHCTRL 0x0000000C
#define OFFSET_PRIMARY_SENSOR_IMG_SIZE		0x00000014	/* READ ONLY */
#define OFFSET_SECONDARY_SENSOR_IMG_SIZE	0x00000018	/* READ ONLY */
#define OFFSET_BAYER_PROC_IMAGE_SIZE		0x0000001C	/* READ ONLY */
#define OFFSET_VP_PROC_IMAGE_SIZE			0x00000020	/* READ ONLY */
#define OFFSET_DUMMY_PIX_GEN				0x00000024
#define OFFSET_VP_MASK_CTRL					0x00000028
#define OFFSET_VP_MASK_OFFSET				0x0000002C
#define OFFSET_VP_MASK_SIZE 				0x00000030
#define OFFSET_ASYNCBUF_BAYER_SCALER		0x00000038
#define OFFSET_BAYER_SCALER_ENABLE			0x00000400
#define OFFSET_BAYER_SCALER_OFFSET			0x00000404
#define OFFSET_BAYER_SCALER_MASK			0x00000408
#define OFFSET_BAYER_SCALER_RATIO			0x0000040C

/* AsyncFIFO Control Parameters */
#define LIVE_PATH_SET						0x3
#define RAW_BIT_MODE8						0x2
#define RAW_BIT_MODE10						0x1
#define RAW_BIT_MODE12						0x0

#define RAW8_DATA_CUT_7_0					4
#define RAW8_DATA_CUT_8_1					3
#define RAW8_DATA_CUT_9_2					2
#define RAW8_DATA_CUT_10_3					1
#define RAW8_DATA_CUT_11_4					0
#define RAW10_DATA_CUT_9_0					2
#define RAW10_DATA_CUT_10_1					1
#define RAW10_DATA_CUT_11_2					0

#define RAW8_DATA_CUT_ALIGN_MSB 			0x4
#define RAW8_DATA_CUT_ALIGN_1BIT			0x3
#define RAW8_DATA_CUT_ALIGN_2BIT			0x2
#define RAW8_DATA_CUT_ALIGN_3BIT			0x1
#define RAW8_DATA_CUT_ALIGN_LSB				0x0
#define RAW10_DATA_CUT_ALIGN_MSB			2
#define RAW10_DATA_CUT_ALIGN_1BIT			1
#define RAW10_DATA_CUT_ALIGN_LSB			0

/* Async buffer write clock delay	*/
#define ASYNC_WRITE_CLK_DELAY_DEFUALT		4

#define SENSOR_TYPE_BAYER	0
#define SENSOR_TYPE_YUV		1

typedef union {
	struct {
		volatile u32 sensor_type	: 2;
		volatile u32 fr_sensor	: 1;
		volatile u32 preupdate	: 1;
		volatile u32 readpath_en	: 1;
		volatile u32 senclksel	: 1;
		volatile u32 linememmode: 2;
		volatile u32 path1_mode	: 1;
		volatile u32 reserved3	: 3;
		volatile u32 yuvsensel	: 1;
		volatile u32 reserved4	: 19;
	} asbits;
	volatile u32 as32bits;
} CSS_ISP_WRAP_CTRL;

typedef union {
	struct {
		volatile u32 param_set	: 1;	/* [0]								*/
		volatile u32 reserved1	: 3;	/* [3:1]							*/
		volatile u32 yuv_en		: 1;	/* [4]								*/
		volatile u32 vsync_pol	: 1;	/* [5]								*/
		volatile u32 yc_endian	: 1;	/* [6]								*/
		volatile u32 jpeg_str_en : 1;	/* [7], JPEG Stream Path Enable Set */
		volatile u32 vsync_delay : 8;	/* [15:8], Vsync Delay Set			*/
		volatile u32 in_width	: 16;	/* [31:16], Image Width Size Set	*/
	} asbits;
	volatile u32 as32bits;
} CSS_ISP_WRAP_YUV_CTRL;

typedef union {
	struct {
		volatile u32 param_set	: 1;	/* [0]								*/
		volatile u32 raw_align	: 3;	/* [3:1]							*/
		volatile u32 bayer_en	: 1;	/* [4]								*/
		volatile u32 vsync_pol	: 1;	/* [5]								*/
		volatile u32 raw_mode_sel : 2;	/* [7:6], RAW8, RAW10, RAW12		*/
		volatile u32 raw_mask_sel : 3;	/* [10:8], RAW Data CUT Set 		*/
		volatile u32 dummy_pix_en : 1;	/* [11], Dummy Pixel Enable 		*/
		volatile u32 vsync_delay : 4;	/* [15:12], Vsync Delay Set 		*/
		volatile u32 in_width	: 16;	/* [31:16], Image Width Size Set	*/
	} asbits;
	volatile u32 as32bits;
} CSS_ISP_WRAP_BAYER_PRE_CTRL;

typedef union {
	struct {
		volatile u32 param_set	: 1;	/* [0]								*/
		volatile u32 reserved1	: 3;	/* [3:1]							*/
		volatile u32 bayer_en	: 1;	/* [4]								*/
		volatile u32 vsync_pol	: 1;	/* [5]								*/
		volatile u32 raw_bypass	: 1;	/* [6], Raw Data Bypass 			*/
		volatile u32 raw_bp_path : 1;	/* [7], Path Selection
											when Raw Data Bypass([6] == 1)	*/
		volatile u32 vsync_delay : 8;	/* [15:8], Vsync Delay Set			*/
		volatile u32 out_width	: 16;	/* [31:16], Image Width Size Set	*/
	} asbits;
	volatile u32 as32bits;
} CSS_ISP_WRAP_BAYER_POST_CTRL;

typedef union {
	struct {
		volatile u32 height		: 16;	/* Vertical Size for readPath	 	*/
		volatile u32 width		: 16;	/* Horizontal Size for readPath 	*/
	} asbits;
	volatile u32 as32bits;
} CSS_ISP_WRAP_VP_MASK_SIZE;

typedef union {
	struct {
		volatile u32 reserved1	: 4;
		volatile u32 enable		: 1;
		volatile u32 vsyncpol	: 1;
		volatile u32 reserved2	: 6;
		volatile u32 vsyncdly	: 4;
		volatile u32 outwidth	: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_ISP_WRAP_BAYER_SCALER;

typedef union {
	struct {
		volatile u32 enable		: 1;
		volatile u32 masken		: 1;
		volatile u32 sclen		: 1;
		volatile u32 reserved1	: 1;
		volatile u32 avgen		: 1;
		volatile u32 reserved2	: 29;
	} asbits;
	volatile u32 as32bits;
} CSS_ISP_WRAP_BAYER_SCALER_ENABLE;

typedef union {
	struct {
		volatile u32 offsety	: 16;
		volatile u32 offsetx	: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_ISP_WRAP_BAYER_SCALER_OFFSET;

typedef union {
	struct {
		volatile u32 masky		: 16;
		volatile u32 maskx		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_ISP_WRAP_BAYER_SCALER_MASK;

typedef union {
	struct {
		volatile u32 ratioy		: 16;
		volatile u32 ratiox		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_ISP_WRAP_BAYER_SCALER_RATIO;

typedef struct {
	CSS_ISP_WRAP_CTRL					ctrl;
	CSS_ISP_WRAP_YUV_CTRL				yuv_ctrl;
	CSS_ISP_WRAP_BAYER_PRE_CTRL 		pre_buf;
	CSS_ISP_WRAP_BAYER_POST_CTRL		post_buf;
	CSS_ISP_WRAP_BAYER_SCALER			param;
	CSS_ISP_WRAP_BAYER_SCALER_ENABLE	enable;
	CSS_ISP_WRAP_BAYER_SCALER_OFFSET	offset;
	CSS_ISP_WRAP_BAYER_SCALER_MASK		mask;
	CSS_ISP_WRAP_BAYER_SCALER_RATIO		ratio;
} CSS_ISP_WRAP_REG_DATA;

/******************************************************************************
	Externally defined global data
******************************************************************************/

/******************************************************************************
	Locally defined global data
******************************************************************************/
void odin_isp_warp_reg_dump(u32 idx);
u32 odin_isp_wrap_get_primary_sensor_size(void);
u32 odin_isp_wrap_get_secondary_sensor_size(void);
s32 odin_isp_wrap_control_param(struct css_afifo_ctrl *phandle);
s32 odin_isp_wrap_pre_param(struct css_afifo_pre *phandle);
s32 odin_isp_wrap_bayer_param(struct css_bayer_scaler *phandle);
s32 odin_isp_wrap_post_param(struct css_afifo_post *phandle);
s32 odin_isp_wrap_change_resolution(u32 path,
							struct css_bayer_scaler *bayer_scl);
s32 odin_isp_wrap_get_isp_idx(u32 scaler);
s32 odin_isp_wrap_get_read_path_status(u32 path);
s32 odin_isp_wrap_set_read_path_enable(u32 path);
s32 odin_isp_wrap_set_read_path_disable(u32 path);
s32 odin_isp_wrap_set_mem_ctrl(u32 mode);
s32 odin_isp_wrap_set_path_sel(u32 path);
s32 odin_isp_wrap_set_sensor_sel(u32 index);
void odin_isp_wrap_set_live_path(struct css_afifo_pre *phandle, u32 enable);
s32 odin_isp_wrap_get_status(void);
s32 odin_isp_wrap_get_mem_ctrl(void);
s32 odin_isp_wrap_get_path_sel(void);
s32 odin_isp_wrap_get_sensor_sel(void);
s32 odin_isp_wrap_get_resolution(u32 path, struct css_image_size* imgsize);

#endif /* __ODIN_ISP_WRAP_H__ */
