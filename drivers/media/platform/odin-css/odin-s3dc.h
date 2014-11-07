/*
 * Stereoscopic 3D Creator driver
 *
 * Copyright (C) 2013 Mtekvision
 * Author: Daeheon Kim <kimdh@mtekvision.com>
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

#ifndef __ODIN_S3DC_H__
#define __ODIN_S3DC_H__


#define VER_MAJOR 1
#define VER_MINOR 1

#define NXSC_DRV_VER (VER_MAJOR << 4) | (VER_MINOR)
#define PI 3.14159265f

#define TOP_EN_RESET							0x1000
#define TOP_STATUS_MASK							0x1004
#define TOP_STATUS_CLR							0x1008
#define TOP_STATUS_INT							0x100C
#define TOP_INTERRUPT							0x1010
#define BLT_TEST_MODE0							0x3000
#define BLT_CMD_FIXED_COLOR_FOR_FILLRECT_L		0x3008
#define BLT_CMD_FIXED_COLOR_FOR_FILLRECT_H		0x300C
#define BLT_CMD_WIDTH_L							0x3024
#define BLT_CMD_WIDTH_H							0x3028
#define BLT_CMD_HEIGHT							0x302C
#define BLT_CMD_SRC_SM_WIDTH_LOW				0x3040
#define BLT_CMD_SRC_SM_WIDTH_HIGH				0x3044
#define BLT_CMD_DST_X_START 					0x3048
#define BLT_CMD_DST_Y_START 					0x304C
#define BLT_CMD_DST_SM_ADDR_LOW					0x3050
#define BLT_CMD_DST_SM_ADDR_HIGH				0x3054
#define BLT_CMD_DST_SM_WIDTH_LOW				0x3058
#define BLT_CMD_DST_SM_WIDTH_HIGH				0x305C
#define BLT_CMD_DST_SM_HEIGHT					0x3060
#define BLT_CMD_SRC2_SM_WIDTH_LOW				0x3074
#define BLT_CMD_SRC2_SM_WIDTH_HIGH				0x3078
#define BLT_CMD_D_FORMAT						0x3084
#define BLT_MODE								0x30A0
#define BLT_SBL									0x30BC
#define BLT_FULL_EN								0x30C0
#define BLT_FILTER_SCALER_EN					0x30E4
#define BLT_CMD_DST_SM_ADDR2_LOW				0x30E8
#define BLT_CMD_DST_SM_ADDR2_HIGH				0x30EC
#define BLT_INPUT_TYPE							0x3140
#define BLT_OUT_TYPE							0x3144
#define BLT_OUT_YUV_TYPE						0x3148
#define PIXEL_FORMAT_CF_EN						0x5000
#define S3DTYPE_D2_SEL							0x5004
#define DECI_MODE_MANUAL_S3D_EN					0x5008
#define LINE_START_0							0x500C
#define LINE_COUNT_0							0x5010
#define PIXEL_START_0							0x5014
#define PIXEL_COUNT_0							0x5018
#define OFF0_ADDRESS0_L							0x501C
#define OFF0_ADDRESS0_H							0x5020
#define OFF1_ADDRESS0_L							0x5024
#define OFF1_ADDRESS0_H							0x5028
#define BLT_SLOW_FAST_FIX_AUTO_SYNC				0x502c
#define LINE_START_1							0x5030
#define LINE_COUNT_1							0x5034
#define PIXEL_START_1							0x5038
#define PIXEL_COUNT_1							0x503C
#define OFF0_ADDRESS1_L							0x5040
#define OFF0_ADDRESS1_H							0x5044
#define OFF1_ADDRESS1_L							0x5048
#define OFF1_ADDRESS1_H							0x504C
#define LR_CHANGE								0x5054
#define WS_WD									0x5058
#define HS_HD									0x505C
#define CAM0_TOTAL_PIXEL_COUNT					0x5060
#define DP_STEP									0x5064
#define TILT_BYPASS								0x5068
#define TANGENT									0x506C
#define RECI_PWIDTH								0x5070
#define WS_WD1									0x5074
#define HS_HD1									0x5078
#define PSTART_OFFSET0							0x5080
#define PSTART_OFFSET1							0x5084
#define CAM1_TOTAL_PIXEL_COUNT					0x5088
#define CAM0_TOTAL_LINE_COUNT					0x508C
#define AC_DP_TH_STEP							0x5090
#define CAM1_TOTAL_LINE_COUNT					0x5094
#define CTW0_COMP								0x50C4
#define CTW1_COMP								0x50C8
#define ROT_BUF_SIZE							0x50CC
#define ROT_ENABLE								0x50D0
#define ROT_DEGREE_SIGN 						0x50D4
#define ROT_SIN_L								0x50D8
#define ROT_SIN_H								0x50DC
#define ROT_COS_L								0x50E0
#define ROT_COS_H								0x50E4
#define ROT_CSC_L								0x50E8
#define ROT_CSC_M								0x50EC
#define ROT_CSC_H								0x50F0
#define ROT_COT_L								0x50F4
#define ROT_COT_M								0x50F8
#define ROT_COT_H								0x50FC
#define ROT_DECI_BLOCK							0x5118
#define CAM0_IF_SEL								0x5140
#define	Y_BASE_ADDR0_L							0x5144
#define	Y_BASE_ADDR0_H							0x5148
#define	C0_BASE_ADDR0_L							0x514C
#define	C0_BASE_ADDR0_H							0x5150
#define VS_BLANK0								0x515C
#define HS_BLANK0								0x5160
#define VS2HS_DEL0								0x5164
#define HS2VS_DEL0								0x5168
#define DMA_T_WIDTH0							0x516C
#define DMA_T_HEIGHT0							0x5170
#define DMA_X_OFFSET0							0x5174
#define DMA_Y_OFFSET0							0x5178
#define DMA_P_WIDTH0							0x517C
#define DMA_P_HEIGHT0							0x5180
#define DMA_START_MODE0							0x5184
#define DMA_START0								0x5188
#define D_FORMAT0								0x518C
#define D_ORDER0								0x5190
#define DMA_BUSY0								0x519C
#define CAM1_IF_SEL								0x51C0
#define	Y_BASE_ADDR1_L							0x51C4
#define	Y_BASE_ADDR1_H							0x51C8
#define	C0_BASE_ADDR1_L							0x51CC
#define	C0_BASE_ADDR1_H							0x51D0
#define VS_BLANK1								0x51DC
#define HS_BLANK1								0x51E0
#define VS2HS_DEL1								0x51E4
#define HS2VS_DEL1								0x51E8
#define DMA_T_WIDTH1							0x51EC
#define DMA_T_HEIGHT1							0x51F0
#define DMA_X_OFFSET1							0x51F4
#define DMA_Y_OFFSET1							0x51F8
#define DMA_P_WIDTH1							0x51FC
#define DMA_P_HEIGHT1							0x5200
#define DMA_START_MODE1							0x5204
#define DMA_START1								0x5208
#define D_FORMAT1								0x520C
#define D_ORDER1								0x5210
#define DMA_BUSY1								0x521C
#define CPU_TRIG_EN								0x5240
#define CPU_TRIG_ST								0x5244
#define ONE_SHOT_ENABLE							0x5248
#define WB_ENABLES								0x5400
#define WB_CURR_AVG_UP_CNT_HWB_VAL				0x5404
#define WB_FSTABLE_CNT							0x5410
#define COLOR_PATTERN_EN_CON_EN					0x5800
#define COLOR_HUE_DEGREE_CAM0					0x5804
#define CGAIN_YGAIN_CAM0						0x5808
#define HCOLOR_LCOLOR_TH_CAM0					0x580C
#define LCGAIN_HCGAIN_CAM0						0x5810
#define LBRIGHT_HBRIGHT_TH_CAM0					0x5814
#define LYGAIN_HYGAIN_CAM0						0x5818
#define OGAIN_BRIGHT_CAM0						0x581C
#define COLOR_HUE_DEGREE_CAM1					0x5840
#define CGAIN_YGAIN_CAM1						0x5844
#define HCOLOR_LCOLOR_TH_CAM1					0x5848
#define LCGAIN_HCGAIN_CAM1						0x584C
#define LBRIGHT_HBRIGHT_TH_CAM1					0x5850
#define LYGAIN_HYGAIN_CAM1						0x5854
#define OGAIN_BRIGHT_CAM1						0x5858
#define COLOR_R11_OFFSET						0x59C0
#define COLOR_R11_COEFF							0x59F0
#define AC_Y_START								0x5C04
#define AC_X_START								0x5C08
#define AC_HEIGHT								0x5C0C
#define AC_WIDTH								0x5C10
#define AC_TAL									0x5C14
#define AC_P_START0_D							0x5C54
#define AC_P_START1_D							0x5C58
#define AC_TOTAL_HEIGHT							0x5C64
#define AC_TOTAL_WIDTH							0x5C68
#define AC_DP_REDUN								0x5C90
#define AC_AUTO_ENABLE							0x5C94
#define AC_EN_ONE								0x5CAC

/* 0: no page flip, 1:page flip on */
#define USE_PAGE_FLIPPING			1

/* sync 관련 Tuning Points */
#define BLT_SLOW					0
#define BLT_FAST					0
#define FIX_SYNC					0
#define AUTO_SYNC					1

#define IO_INDIRECT_ADDR		(6)
#define IO_INDIRECT_DATA		(7)
#define IO_LCD_INDIRECT_ADDR	(0)
#define IO_LCD_INDIRECT_DATA	(1)

/* BLT out data order define */
/* order for YUV444_1PLANE_PACKED */
#define	BLT_OUT_ORDER_YUV	0
#define BLT_OUT_ORDER_YVU	1
/* order for YUV422_1PLANE_PACKED */
#define BLT_OUT_ORDER_YUYV	0
#define BLT_OUT_ORDER_UYVY	1
#define BLT_OUT_ORDER_YVYU	2
#define BLT_OUT_ORDER_VYUY	3
/* order for YUV420_2PLANE, YUV422_PLANAR */
#define BLT_OUT_ORDER_UVUV	0
#define BLT_OUT_ORDER_VUVU	1
/* order for 422 planar */
#define BLT_OUT_ORDER_U_V	0
#define BLT_OUT_ORDER_V_U	1
/* order for yuv444 unpacked format */
#define BLT_OUT_ORDER_ZYUV	0
#define BLT_OUT_ORDER_YUVZ	1
#define BLT_OUT_ORDER_ZYVU	2
#define BLT_OUT_ORDER_YVUZ	3
/* order for RGB8888 */
#define BLT_OUT_ORDER_RGBZ	0
#define BLT_OUT_ORDER_ZRGB	1
#define BLT_OUT_ORDER_BGRZ	2
#define BLT_OUT_ORDER_ZBGR	3
/* order for RGB888,RGB565 */
#define BLT_OUT_ORDER_RGB	0
#define BLT_OUT_ORDER_BGR	1

/* data order define (Do not change) */
#define	ORDER_NONE	0xFF
#define	ORDER_YUV	0
#define ORDER_YVU	1
#define ORDER_YUYV	0
#define ORDER_UYVY	1
#define ORDER_YVYU	2
#define ORDER_VYUY	3
#define ORDER_UVUV	0
#define ORDER_VUVU	1
/* data order define for Direct input */
#define ORDER_UV_FIRST			0
#define ORDER_U_FIRST_V_SECOND	1
#define ORDER_V_FIRST_U_SECOND	2
#define ORDER_UV_SECOND			3

/* BLT Input data format(= CF write) */
#define BLT_INPUT_RGB888	1
#define BLT_INPUT_RGB8888	2

/* BLT register bit defines */
#define BLT_CMD_MERGE_PIXEL_3000	0x000C	/* 0000_00_00_0_000_11_00 */
#define BLT_CMD_SEND_PIXEL_3000		0x0008	/* 0000_00_00_0_000_10_00 */
#define BLT_CMD_GET_PIXEL_3000		0x0004	/* 0000_00_00_0_000_01_00 */
#define BLT_CMD_FILL_RECT_3000		0x0000	/* 0000_00_00_0_000_00_00 */

#define BLT_COLOR_FMT_RGB565		(0x4)
#define BLT_COLOR_FMT_RGBA8888		(0x8)
/* FIXED COLOR -> STACK MEM 에 채운다. */
#define BLT_OP_FILLRECT_FC2S		(0x0040)

/* Display Size */
/* #define DISPLAY_WIDTH			3264 */
/* #define DISPLAY_HEIGHT			2448 */
#define DISPLAY_WIDTH				640
#define DISPLAY_HEIGHT				480

/* TOP interrupt defines */
#define INT_S3DC_CF_LOSS_FRAME		(0x1 << 0)
#define INT_S3DC_CF_CANCEL_FRAME 	(0x1 << 1)
#define INT_S3DC_CF_CORRUPT_FRAME	(0x1 << 2)
#define INT_S3DC_CF_OVERFLOW 		(0x1 << 3)
#define INT_S3DC_CF_L_START			(0x1 << 4)
#define INT_S3DC_CF_R_START			(0x1 << 5)
#define INT_S3DC_CF_IDLE 			(0x1 << 6)
#define INT_S3DC_LOSS_FRAME			(0x1 << 7)
#define INT_S3DC_FRAME_END			(0x1 << 8)
#define INT_S3DC_BLT_IDLE			(0x1 << 9)
#define INT_S3DC_ALL_IDLE			(0x1 << 10)

typedef union {
	struct {
		volatile u32 trans_stop_req	: 1;	/* SW RESET (Auto recovery) */
		volatile u32 reserved1		: 1;	/* Reserved */
		volatile u32 nxs3dc_on		: 1;	/* NXS3DC_ON */
		volatile u32 reserved2		: 1;	/* Reserved */
		volatile u32 cf_reg_update	: 1;	/* CF Shadow register update O */
		volatile u32 blt_reg_update : 1;	/* BLT Shadow register update On */
		volatile u32 reserved3		: 26;	/* Reserved */
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_TOP_EN_RESET;	/* 0x1000 TOP_EN_RESET */

typedef union {
	struct {
		volatile u32 ready			: 1;	/* Blt ready signal */
		volatile u32 rd_ready		: 1;	/* Get pixel read valid signal */
		volatile u32 blt_cmd		: 2;	/* 2'b00: Fill Rect */
											/* 2'b11: Merge Pixel */
											/* 2'b10: Send Pixel */
											/* 2'b01: Get Pixel */
		volatile u32 blt_mode		: 2;	/* 2'b00: S --> D */
											/* 2'b01: S,S' --> D */
											/* 2'b10: S,S' --> DD */
											/* 2'b11: S,F --> D */
		volatile u32 reserved1		: 1;	/* Reserved */
		volatile u32 wr_ready		: 1;	/* Send Pixel ready valid signal */
		volatile u32 reserved2		: 24;	/* Reserved */
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_TEST_MODE0;

typedef union {
	struct {
		volatile u32 fix_color_l	: 16;	/* used at Fill Rect */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_FR_COLOR_L;

typedef union {
	struct {
		volatile u32 fix_color_h	: 16;	/* used at Fill Rect */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_FR_COLOR_H;

typedef union {
	struct {
		volatile u32 width_l		: 16;	/* set region */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_WIDTH_L;

typedef union {
	struct {
		volatile u32 width_h		: 10;	/* set region */
		volatile u32 reserved		: 22;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_WIDTH_H;

typedef union {
	struct {
		volatile u32 height 		: 16;	/* set region */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_HEIGHT;

typedef union {
	struct {
		volatile u32 src_sm_width_l	: 16;	/* set region */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_SRCSM_WIDTH_L;

typedef union {
	struct {
		volatile u32 src_sm_width_h	: 10;	/* set region */
		volatile u32 reserved		: 22;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_SRCSM_WIDTH_H;

typedef union {
	struct {
		volatile u32 dst_xst		: 16;	/* dst x offset */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_DST_XST;

typedef union {
	struct {
		volatile u32 dst_yst		: 16;	/* dst y offset */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_DST_YST;

typedef union {
	struct {
		volatile u32 dst_sm_addr_l	: 16;	/* set region */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_DSTSM_ADDR_L;

typedef union {
	struct {
		volatile u32 dst_sm_addr_h	: 15;	/* set region */
		volatile u32 reserved		: 17;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_DSTSM_ADDR_H;

typedef union {
	struct {
		volatile u32 dst_sm_width_l	: 16;	/* set region */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_DSTSM_WIDTH_L;

typedef union {
	struct {
		volatile u32 dst_sm_width_h	: 10;	/* set region */
		volatile u32 reserved		: 22;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_DSTSM_WIDTH_H;

typedef union {
	struct {
		volatile u32 dst_sm_height	: 16;	/* set region */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_DSTSM_HEIGHT;

typedef union {
	struct {
		volatile u32 src2_sm_width_l	: 16;	/* set region */
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_SRC2SM_WIDTH_L;

typedef union {
	struct {
		volatile u32 src2_sm_width_h	: 10;	/* set region */
		volatile u32 reserved			: 22;	/* Reserved */
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST0_SRC2SM_WIDTH_H;

typedef union {
	struct {
		volatile u32 host_data		: 16;	/* data write or read */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_TEST_MODE1;

typedef union {
	struct {
		volatile u32 src_d_fmt		: 4;	/* src data format */
		volatile u32 dst_d_fmt		: 4;	/* dst data format */
		volatile u32 src2_d_fmt 	: 4;	/* src2 data format */
		volatile u32 reserved		: 20;	/* Reserved */
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_FMT;

typedef union {
	struct {
		volatile u32 sbt_mode		: 1;	/* 1:driven by CF_ENG */
		volatile u32 reserved1		: 6;	/* Reserved */
		volatile u32 sbt_mode_d 	: 1;	/* SBT_MODE Confirm */
		volatile u32 reserved2		: 24;	/* Reserved */
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_MODE_R;

typedef union {
	struct {
		volatile u32 sbl			: 7;	/* Block transfer 단위 */
		volatile u32 reserved		: 25;	/* Reserved */
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_TEST_MODE2;

typedef union {
	struct {
		volatile u32 reserved1		: 3;	/* Reserved */
		volatile u32 full_en		: 1;	/* Full size SBS or TB enable */
		volatile u32 reserved2		: 28;	/* Reserved */
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_FULL_EN;

typedef union {
	struct {
		volatile u32 scl_en 		: 1;	/* scale enable */
		volatile u32 fil_en 		: 2;	/* filter enable */
		volatile u32 reserved		: 29;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_SCALE_SET0;

typedef union {
	struct {
		volatile u32 dst_sm_addr2_l	: 16;	/* flipping */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST1_DSTSM_ADDR_L;

typedef union {
	struct {
		volatile u32 dst_sm_addr2_h	: 15;	/* flipping */
		volatile u32 reserved		: 17;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_RGNST1_DSTSM_ADDR_H;

typedef union {
	struct {
		/* 0	: RGB, 1: BGR for srource data */
		volatile u32 ld_order		: 1;
		/* 0	: RGBz, 1 ; zRGB	for srource data */
		volatile u32 lz_position	: 1;
		/* 0 ; RGBA8888, 1	: RGB888	for srource data */
		volatile u32 lpack_en		: 1;
		volatile u32 reserved		: 29;	/* Reserved */
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_IN_FMT;

typedef union {
	struct {
		/* 0	: RGB, 1: BGR for destination data */
		volatile u32 od_order		: 1;
		/* 0	: RGBz, 1 ; zRGB	for destination data */
		volatile u32 oz_position	: 1;
		/* 0 ; RGBA8888, 1	: RGB888	for destination data */
		volatile u32 opack_en		: 1;
		volatile u32 reserved		: 29;	/* Reserved */
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_OUT_FMT;

typedef union {
	struct {
		/* YUV enable for destination data */
		volatile u32 oy_en			: 1;
		volatile u32 y_fmt			: 2;	/* YUV444 */
		/* 0 : YUYV or YVYU (along to Od_order), 1 ; UYVY or VYUY */
		volatile u32 y422_mode		: 1;
		/* 00 : IMC1 or IMC3 along to Od_order */
		volatile u32 y420_mode		: 2;
		volatile u32 reserved		: 26;	/* Reserved */
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_BLT_YUV_SET;

typedef union {
	struct {
		volatile u32 reserved1		: 5;	/* Reserved */
		volatile u32 wr_16b0		: 1;	/* 16b write enable */
		volatile u32 reserved2		: 4;	/* Reserved */
		/* 32 bits input 16bits output, CAM 1 */
		volatile u32 wr_16b1		: 1;
		volatile u32 reserved3		: 2;	/* Reserved */
		/* cam0 write pack enable (0	: RGBA8888, 1 */
		volatile u32 cam0_pack_en	: 1;
		/* cam1 write pack enable (0	: RGBA8888, 1 */
		volatile u32 cam1_pack_en	: 1;
		volatile u32 reserved4		: 17;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_WR_CONDITION;

typedef union {
	struct {
		volatile u32 reserved1		: 1;	/* Reserved */
		/* S3D_EN =0로 2D mode에서 출력하려는 camera 선택 */
		volatile u32 d2_sel 		: 1;
		volatile u32 s3d_type		: 3;	/* 3'b000	: pixel barrier */
		volatile u32 reserved2		: 27;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_S3D_FMT;

typedef union {
	struct {
		volatile u32 s3d_en 		: 1;	/* 3D mode enable */
		/* manual convergence시 L, R blend enable */
		volatile u32 blend_en		: 1;
		volatile u32 deci_mode		: 2;	/* 2'b00 */
		/* Filter enable in pixel skip mode (DECI_MODE = 1 or 3) */
		volatile u32 deci_fil_en	: 1;
		volatile u32 reserved		: 27;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ENABLE;

typedef union {
	struct {
		volatile u32 l_start0		: 14;	/* cam0의 시작 라인 설정 */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CROP_REGION0_L_START0;

typedef union {
	struct {
		volatile u32 l_width0		: 14;	/* cam0의 총 라인 개수 설정 */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CROP_REGION0_L_WIDTH0;

typedef union {
	struct {
		volatile u32 p_start0		: 14;	/* cam0의 시작 픽셀 설정 */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CROP_REGION0_P_START0;

typedef union {
	struct {
		volatile u32 p_width0		: 14;	/* cam0의 총 픽셀 개수 설정 */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CROP_REGION0_P_WIDTH0;

typedef union {
	struct {
		volatile u32 off0_addr0_l	: 16;	/* OFF0_ADDR0 하위 16bits */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_WADDR0_OFF0_ADDR0_L;

typedef union {
	struct {
		volatile u32 off0_addr0_h	: 14;	/* OFF0_ADDR0 상위 14bits */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_WADDR0_OFF0_ADDR0_H;

typedef union {
	struct {
		volatile u32 off1_addr0_l	: 16;	/* OFF1_ADDR0 하위 16bits */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_WADDR0_OFF1_ADDR0_L;

typedef union {
	struct {
		volatile u32 off1_addr0_h	: 14;	/* OFF1_ADDR0 상위 14bits */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_WADDR0_OFF1_ADDR0_H;

typedef union {
	struct {
		volatile u32 sync_en		: 1;	/* auto sync enable */
		/* auto sync disalbe 시 cam0 또는 cam1에 동기화 */
		volatile u32 fix_sync		: 1;
		volatile u32 blt_fast		: 1;
		volatile u32 blt_slow		: 1;
		volatile u32 reserved		: 28;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_SYNC;

typedef union {
	struct {
		volatile u32 l_start1		: 14;	/* cam1의 시작 라인 설정 */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CROP_REGION1_L_START1;

typedef union {
	struct {
		volatile u32 l_width1		: 14;	/* cam1의 총 라인 개수 설정 */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CROP_REGION1_L_WIDTH1;

typedef union {
	struct {
		volatile u32 p_start1		: 14;	/* cam1의 시작 픽셀 설정 */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CROP_REGION1_P_START1;

typedef union {
	struct {
		volatile u32 p_width1		: 14;	/* cam1의 총 픽셀 개수 설정 */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CROP_REGION1_P_WIDTH1;

typedef union {
	struct {
		volatile u32 off0_addr1_l	: 16;	/* OFF0_ADDR1 하위 16bits */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_WADDR1_OFF0_ADDR1_L;

typedef union {
	struct {
		volatile u32 off0_addr1_h	: 14;	/* OFF0_ADDR1 하위 16bits */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_WADDR1_OFF0_ADDR1_H;

typedef union {
	struct {
		volatile u32 off1_addr1_l	: 16;	/* OFF1_ADDR1 하위 16bits */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_WADDR1_OFF1_ADDR1_L;

typedef union {
	struct {
		volatile u32 off1_addr1_h	: 14;	/* OFF1_ADDR1 상위 14bits */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_WADDR1_OFF1_ADDR1_H;

typedef union {
	struct {
		volatile u32 cam_dec		: 1;	/* reverse cam 1 <=> 0 */
		volatile u32 reserved		: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CAM_SELECT;

typedef union {
	struct {
		volatile u32 ws_wd0 		: 16;	/* horizontal scale factor0 */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_SCALE_FACTOR0_WS_WD0;

typedef union {
	struct {
		volatile u32 hs_hd0 		: 16;	/* vertical scale factor0 */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_SCALE_FACTOR0_HS_HD0;

typedef union {
	struct {
		volatile u32 ctw0			: 14;	/* Camera0 입력 total width */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CAM_INFO0;

typedef union {
	struct {
		/* 100% or 50% of disparity apply to next frame */
		volatile u32 dp_step		: 8;
		volatile u32 reserved		: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_DP_STEP;

typedef union {
	struct {
		volatile u32 reserved1		: 2;	/* Reserved */
		/* tilt bypass enable (width에 대해 non crop 시
		 * tilt disable 시키고자 할 경우) */
		volatile u32 tilt_bypass	: 1;
		volatile u32 reserved2		: 29;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_TILT_BYPASS;

typedef union {
	struct {
		/* tangent value for tilt compensation */
		volatile u32 tangent		: 16;
		volatile u32 reserved2		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_TILT_TANGENT;

typedef union {
	struct {
		/* reciprocalof pwidth (1/P_WIDTH1*65536) */
		volatile u32 reci_pwid		: 16;
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_TILT_RECI_PWID;

typedef union {
	struct {
		volatile u32 ws_wd1 		: 16;	/* horizontal scale factor1 */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_SCALE_FACTOR1_WS_WD1;

typedef union {
	struct {
		volatile u32 hs_hd1 		: 16;	/* vertical scale factor1 */
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_SCALE_FACTOR1_HS_HD1;

typedef union {
	struct {
		volatile u32 p_start_offset0	: 14;	/* signed 11b -8191 ~ +8191 */
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_P_OFFSET0;

typedef union {
	struct {
		volatile u32 p_start_offset1	: 14;	/* signed 11b -8191 ~ +8191 */
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_P_OFFSET1;

typedef union {
	struct {
		volatile u32 ctw1			: 14;	/* Camera1 입력 total width */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CAM_INFO1;

typedef union {
	struct {
		volatile u32 cth0			: 14;	/* Camera0 입력 total height */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CAM_INFO1_CTH0;

typedef union {
	struct {
		volatile u32 ac_dp_th_step	: 32;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_DP_TH_STEP;

typedef union {
	struct {
		volatile u32 cth1			: 14;	/* Camera1 입력 total height */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_CAM_INFO1_CTH1;

typedef union {
	struct {
		volatile u32 warn0			: 1;	/* warning0 */
		volatile u32 warn1			: 1;	/* warning1 */
		volatile u32 reserved		: 30;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_WARNNING;

typedef union {
	struct {
		volatile u32 ctw0_comp		: 14;	/* CTW0 보정값 */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_COMPENSATION_CTW0;

typedef union {
	struct {
		volatile u32 ctw1_comp		: 14;	/* CTW1 보정값 */
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_COMPENSATION_CTW1;

typedef union {
	struct {
		/* buffer의 end address, 부착된 buffer의 size에 따라 다름 */
		volatile u32 br_bufsize		: 11;
		volatile u32 reserved		: 21;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_BUFSIZE;

typedef union {
	struct {
		/* rotation의 enable, 0 degree에서 0으로 설정 */
		volatile u32 br_rot_enable	: 1;
		volatile u32 reserved		: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_ROT_ENABLE;

typedef union {
	struct {
		volatile u32 br_degree_sign	: 1;	/* rotation degree의 sign */
		volatile u32 reserved		: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_DEGREE_SIGN;

typedef union {
	struct {
		volatile u32 br_sin_l			: 16;	/* sin(degree)의 low bits */
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_SIN_L;

typedef union {
	struct {
		volatile u32 br_sin_h			: 10;	/* sin(degree)의 high bits */
		volatile u32 reserved			: 22;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_SIN_H;

typedef union {
	struct {
		volatile u32 br_cos_l			: 16;	/* cos(degree)의 low bits */
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_COS_L;

typedef union {
	struct {
		volatile u32 br_cos_h			: 10;	/* cos(degree)의 high bits */
		volatile u32 reserved			: 22;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_COS_H;

typedef union {
	struct {
		volatile u32 br_csc_l			: 16;	/* csc(degree)의 low bits */
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_CSC_L;

typedef union {
	struct {
		volatile u32 br_csc_m			: 16;	/* csc(degree)의 middle bits */
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_CSC_M;

typedef union {
	struct {
		volatile u32 br_csc_h			: 10;	/* csc(degree)의 high bits */
		volatile u32 reserved			: 22;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_CSC_H;

typedef union {
	struct {
		volatile u32 br_cot_l			: 16;	/* cot(degree)의 low bits */
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_COT_L;

typedef union {
	struct {
		volatile u32 br_cot_m			: 16;	/* cot(degree)의 middle bits */
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_COT_M;

typedef union {
	struct {
		volatile u32 br_cot_h			: 10;	/* cot(degree)의 high bits */
		volatile u32 reserved			: 22;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_COT_H;

typedef union {
	struct {
		volatile u32 br_min_limit		: 14;	/* p_start min value */
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT_BR_MIN_LIMIT;

typedef union {
	struct {
		/* block deci_mode=0x1 or deci_mode=0x2 when rot is enable.
		 * In these case deci_mode set to 0x0. need to scale factor for cam 0
		 */
		volatile u32 rot_deci_block		: 1;
		volatile u32 reserved			: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_ROT;

typedef union {
	struct {
		/* Camera0 interface selection */
		volatile u32 cam0_if_sel		: 1;
		volatile u32 reserved			: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_IF_SEL0;

typedef union {
	struct {
		/* Y component base address0 [15:0] */
		volatile u32 y_base_addr0_l		: 16;
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_RADDR0_Y_BASE_ADDR0_L;

typedef union {
	struct {
		/* Y component base address0 [31:16] */
		volatile u32 y_base_addr0_h		: 16;
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_RADDR0_Y_BASE_ADDR0_H;

typedef union {
	struct {
		/* C0 component base address0 [15:0] */
		volatile u32 c0_base_addr0_l	: 16;
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_RADDR0_C0_BASE_ADDR0_L;

typedef union {
	struct {
		/* C0 component base address0 [31:16] */
		volatile u32 c0_base_addr0_h	: 16;
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_RADDR0_C0_BASE_ADDR0_H;

typedef union {
	struct {
		/* Vertical Sync Width (only for DMA interface mode only) */
		volatile u32 vs_blank0			: 16;
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INT_TIMING0_VSBLANK0;

typedef union {
	struct {
		/* Horizontal Sync Width (for DMA interface mode only) */
		volatile u32 hs_blank0			: 16;
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INT_TIMING0_HSBLANK0;

typedef union {
	struct {
		/* Interval between Vertical Sync and Horizontal Sync
			(for DMA interface mode only) */
		volatile u32 vs2hs_del0 		: 16;
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INT_TIMING0_VS2HS_DEL0;

typedef union {
	struct {
		/* Interval between Horizontal Sync and Vertical Sync
			(for DMA interface mode only) */
		volatile u32 hs2vs_del0 		: 16;
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INT_TIMING0_HS2VS_DEL0;

typedef union {
	struct {
		/* Source image total width 0 */
		volatile u32 dma_t_width0		: 14;
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INPUT_SIZE0_DMA_T_WIDTH0;

typedef union {
	struct {
		/* Source image total height 0 */
		volatile u32 dma_t_height0		: 14;
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INPUT_SIZE0_DMA_T_HEIGHT0;

typedef union {
	struct {
		/* Source image crop x offset 0 from origin */
		volatile u32 dma_x_offset0		: 14;
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INPUT_SIZE0_DMA_X_OFFSET0;

typedef union {
	struct {
		/* Source image crop y offset 0 from origin */
		volatile u32 dma_y_offset0		: 14;
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INPUT_SIZE0_DMA_Y_OFFSET0;

typedef union {
	struct {
		volatile u32 dma_p_width0		: 14;	/* Source image crop width 0 */
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INPUT_SIZE0_DMA_P_WIDTH0;

typedef union {
	struct {
		volatile u32 dma_p_height0		: 14;	/* Source image crop height 0 */
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INPUT_SIZE0_DMA_P_HEIGHT0;

typedef union {
	struct {
		/* DMA start mode0 */
		/* 0 : DMA start by host command */
		/* 1 : DMA start by CAM1 (direct interface) VSYNC */
		/* 2 : DMA start by embedded timing generator */
		/* 3 : Reserved */
		volatile u32 dma_start_mode0	: 2;
		volatile u32 reserved			: 30;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_SMODE0;

typedef union {
	struct {
		/* DMA read start command0 (Auto-cleared) */
		/* It is validated only in DMA_START_MODE0 = 0. */
		volatile u32 dma_start0 		: 1;
		volatile u32 reserved			: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_START0;

typedef union {
	struct {
		volatile u32 d_fmt0 		: 2;	/* Data format 0 */
		volatile u32 reserved		: 30;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_D_FMT0;

typedef union {
	struct {
		volatile u32 d_order0		: 2;	/* OTF / DMA data order 0 */
		volatile u32 reserved		: 30;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_D_ORDER0;

typedef union {
	struct {
		volatile u32 dma_busy0		: 1;	/* DMA read operation busy0 */
		volatile u32 reserved		: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_RD_BUSY0;

typedef union {
	struct {
		volatile u32 cam1_if_sel	: 1;	/* Camera1 interface selection */
		volatile u32 reserved		: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_IF_SEL1;

typedef union {
	struct {
		/* Y component base address1 [15:0] */
		volatile u32 y_base_addr1_l		: 16;
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_RADDR1_Y_BASE_ADDR1_L;

typedef union {
	struct {
		/* Y component base address1 [31:16] */
		volatile u32 y_base_addr1_h		: 16;
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_RADDR1_Y_BASE_ADDR1_H;

typedef union {
	struct {
		/* C0 component base address1 [15:0] */
		volatile u32 c0_base_addr1_l	: 16;
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_RADDR1_C0_BASE_ADDR1_L;

typedef union {
	struct {
		/* C0 component base address1 [31:16] */
		volatile u32 c0_base_addr1_h	: 16;
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_RADDR1_C0_BASE_ADDR1_H;

typedef union {
	struct {
		/* Vertical Sync Width (only for DMA interface mode only) */
		volatile u32 vs_blank1		: 16;
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INT_TIMING1_VSBLANK1;

typedef union {
	struct {
		/* Horizontal Sync Width (for DMA interface mode only) */
		volatile u32 hs_blank1		: 16;
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INT_TIMING1_HSBLANK1;

typedef union {
	struct {
		/* Interval between Vertical Sync and Horizontal Sync
			(for DMA interface mode only) */
		volatile u32 vs2hs_del1 	: 16;
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INT_TIMING1_VS2HS_DEL1;

typedef union {
	struct {
		/* Interval between Horizontal Sync and Vertical Sync
			(for DMA interface mode only) */
		volatile u32 hs2vs_del1 	: 16;
		volatile u32 reserved		: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INT_TIMING1_HS2VS_DEL1;

typedef union {
	struct {
		/* Source image total width 1 */
		volatile u32 dma_t_width1	: 14;
		volatile u32 reserved		: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INPUT_SIZE1_DMA_T_WIDTH1;

typedef union {
	struct {
		/* Source image total height 1 */
		volatile u32 dma_t_height1		: 14;
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INPUT_SIZE1_DMA_T_HEIGHT1;

typedef union {
	struct {
		/* Source image crop x offset 1 from origin */
		volatile u32 dma_x_offset1		: 14;
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INPUT_SIZE1_DMA_X_OFFSET1;

typedef union {
	struct {
		/* Source image crop y offset 1 from origin */
		volatile u32 dma_y_offset1		: 14;
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INPUT_SIZE1_DMA_Y_OFFSET1;

typedef union {
	struct {
		volatile u32 dma_p_width1		: 14;	/* Source image crop width 1 */
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INPUT_SIZE1_DMA_P_WIDTH1;

typedef union {
	struct {
		volatile u32 dma_p_height1		: 14;	/* Source image crop height 1 */
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_INPUT_SIZE1_DMA_P_HEIGHT1;

typedef union {
	struct {
		/* DMA start mode1 */
		/* 0 : DMA start by host command */
		/* 1 : DMA start by CAM0 (direct interface) VSYNC */
		/* 2 : DMA start by embedded timing generator */
		/* 3 : Reserved */
		volatile u32 dma_start_mode1	: 2;
		volatile u32 reserved			: 30;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_SMODE1;

typedef union {
	struct {
		/* DMA read start command1 (Auto-cleared)
			: It is validated only in DMA_START_MODE1 = 0. */
		volatile u32 dma_start1 		: 1;
		volatile u32 reserved			: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_START1;

typedef union {
	struct {
		volatile u32 d_fmt1 		: 2;	/* Data format 1 */
		volatile u32 reserved		: 30;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_D_FMT1;

typedef union {
	struct {
		volatile u32 d_order1		: 2;	/* OTF / DMA data order 1 */
		volatile u32 reserved		: 30;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CF_D_ORDER1;

typedef union {
	struct {
		volatile u32 dma_busy1		: 1;	/* DMA read operation busy1 */
		volatile u32 reserved		: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_DMA_IF_RD_BUSY1;

typedef union {
	struct {
		/* CPU trigger mode enable / disable */
		volatile u32 cpu_trig_en	: 1;
		volatile u32 reserved		: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CPU_TRIG_EN;

typedef union {
	struct {
		volatile u32 cpu_trig_st	: 1;	/* CPU trigger (auto cleared) */
		volatile u32 reserved		: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_CPU_TRIG_ST;

typedef union {
	struct {
		/* One shot mode enable (0 : disable, 1 : enable) */
		volatile u32 one_shot_en	: 1;
		volatile u32 reserved		: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_ONE_SHOT_EN;

typedef union {
	struct {
		/* White Balance Enable */
		volatile u32 wb_en				: 1;
		/* WB compensation value application control */
		volatile u32 hwb_en 			: 1;
		/* WB Hold */
		volatile u32 wb_hold			: 1;
		/* WB Auto Stop Enable */
		volatile u32 wb_auto_stop_en	: 1;
		volatile u32 reserved			: 28;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_WB_COMMON_SET_CTRL;

typedef union {
	struct {
		/* WB compensation value reflection ratio */
		volatile u32 hwb_value				: 2;
		/* pre- WB compesation average info. Update counter */
		volatile u32 current_wb_avg_up_cnt	: 5;
		volatile u32 reserved				: 25;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_WB_COMMON_SET_PARAM;

typedef union {
	struct {
		/* WB Frame Stable Counter */
		volatile u32 wb_fstable_cnt 		: 8;
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_WB_COMMON_SET_STABLECNT;

typedef union {
	struct {
		/* WB cam0 compensated luminance averge */
		volatile u32 wb_cam0_avg			: 8;
		/* WB cam1 compensated luminance averge */
		volatile u32 wb_cam1_avg			: 8;
		volatile u32 reserved				: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_WB_AVG_INFO_WB_CAM;

typedef union {
	struct {
		/* WB cam0 input current luminance averge */
		volatile u32 wb_cam0_cur_avg		: 8;
		/* WB cam1 input current luminance averge */
		volatile u32 wb_cam1_cur_avg		: 8;
		volatile u32 reserved				: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_WB_AVG_INFO_WB_CUR_CAM;

typedef union {
	struct {
		/* WB cam0 compensated value */
		volatile u32 wb_cam0_value			: 8;
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_WB_AVG_INFO_WB_CAM0_VAL;

typedef union {
	struct {
		/* WB cam1 compensated value */
		volatile u32 wb_cam1_value			: 8;
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_WB_AVG_INFO_WB_CAM1_VAL;

typedef union {
	struct {
		/* Cam0 Color Contol Enable */
		volatile u32 cam0_color_con_en		: 1;
		/* Cam1 Color Contol Enable */
		volatile u32 cam1_color_con_en		: 1;
		/* Pattern Enable */
		volatile u32 pattern_en 			: 1;
		/* Reserved */
		volatile u32 reserved1				: 1;
		/* y2r no color input chrominance correction enable */
		volatile u32 no_color_en			: 1;
		volatile u32 reserved2				: 27;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON_EN;

typedef union {
	struct {
		volatile u32 cam0_hue_degree		: 8;	/* cam0 Hue Degree */
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON0_CAM0_HUE_DEGREE;

typedef union {
	struct {
		volatile u32 cam0_cgain 		: 8;	/* cam0 Saturation Gain */
		volatile u32 cam0_ygain 		: 8;	/* cam0 Luminance Gain */
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON0_CAM0_GAIN;

typedef union {
	struct {
		/* cam0 Low Saturation Surpression Region Threshold */
		volatile u32 cam0_lcolor_th 		: 8;
		/* cam0 High Saturation Surpression Region Threshold */
		volatile u32 cam0_hcolor_th 		: 8;
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON0_CAM0_COLOR_TH;

typedef union {
	struct {
		/* cam0 Low Saturation Region Gain */
		volatile u32 cam0_lcgain			: 8;
		/* cam0 High Saturation Region Gain */
		volatile u32 cam0_hcgain			: 8;
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON0_CAM0_CGAIN;

typedef union {
	struct {
		/* cam0 Low Luminance Surpression Region Threshold */
		volatile u32 cam0_lbright_th		: 8;
		/* cam0 High Luminance Surpression Region Threshold */
		volatile u32 cam0_hbright_th		: 8;
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON0_CAM0_BRIGHT_TH;

typedef union {
	struct {
		/* cam0 Low Luminance Region Gain */
		volatile u32 cam0_lygain			: 8;
		/* cam0 High Luminance Region Gain */
		volatile u32 cam0_hygain			: 8;
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON0_CAM0_YGAIN;

typedef union {
	struct {
		volatile u32 cam0_rgb_bright		: 7;	/* cam0 RGB Boostup Value */
		volatile u32 reserved1				: 1;
		volatile u32 cam0_rgb_ogain 		: 8;	/* cam0 RGB Contrast Gain */
		volatile u32 reserved2				: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON0_CAM0_RGB_BRIGHT_OGAIN;

typedef union {
	struct {
		volatile u32 cam1_hue_degree		: 8;	/* cam1 Hue Degree */
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON1_CAM1_HUE_DEGREE;

typedef union {
	struct {
		volatile u32 cam1_cgain 		: 8;	/* cam1 Saturation Gain */
		volatile u32 cam1_ygain 		: 8;	/* cam1 Luminance Gain */
		volatile u32 reserved			: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON1_CAM1_GAIN;

typedef union {
	struct {
		/* cam1 Low Saturation Surpression Region Threshold */
		volatile u32 cam1_lcolor_th 		: 8;
		/* cam1 High Saturation Surpression Region Threshold */
		volatile u32 cam1_hcolor_th 		: 8;
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON1_CAM1_COLOR_TH;

typedef union {
	struct {
		/* cam1 Low Saturation Region Gain */
		volatile u32 cam1_lcgain			: 8;
		/* cam1 High Saturation Region Gain */
		volatile u32 cam1_hcgain			: 8;
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON1_CAM1_CGAIN;

typedef union {
	struct {
		/* cam1 Low Luminance Surpression Region Threshold */
		volatile u32 cam1_lbright_th		: 8;
		/* cam1 High Luminance Surpression	Region Threshold */
		volatile u32 cam1_hbright_th		: 8;
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON1_CAM1_BRIGHT_TH;

typedef union {
	struct {
		/* cam1 Low Luminance Region Gain */
		volatile u32 cam1_lygain			: 8;
		/* cam1 High Luminance Region Gain */
		volatile u32 cam1_hygain			: 8;
		volatile u32 reserved				: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON1_CAM1_YGAIN;

typedef union {
	struct {
		volatile u32 cam1_rgb_bright		: 7;	/* cam1 RGB Boostup Value */
		volatile u32 reserved1				: 1;
		volatile u32 cam1_rgb_ogain 		: 8;	/* cam1 RGB Contrast Gain */
		volatile u32 reserved2				: 16;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_COLOR_CON1_CAM1_RGB_BRIGHT_OGAIN;

typedef union {
	struct {
		/* r11 offset (sign1 int 8 fraction 4) */
		volatile u32 r11_offset 			: 13;
		volatile u32 reserved				: 19;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_Y2R_COEFF_SET_R11_OFFSET;

typedef union {
	struct {
		/* r11coef (sign1 int 2 fraction 10) */
		volatile u32 r11_coeff				: 13;
		volatile u32 reserved				: 19;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_Y2R_COEFF_SET_R11_COEFF;

typedef union {
	struct {
		volatile u32 y_st				: 13;	/* auto-convergence의 y start */
		volatile u32 reserved			: 19;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_REGION_Y_ST;

typedef union {
	struct {
		volatile u32 x_st				: 13;	/* auto-convergence의 x start */
		volatile u32 reserved			: 19;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_REGION_X_ST;

typedef union {
	struct {
		volatile u32 ac_height			: 13;	/* auto-convergence의 height */
		volatile u32 reserved			: 19;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_REGION_AC_HEIGHT;

typedef union {
	struct {
		volatile u32 ac_width			: 13;	/* auto-convergence의 width */
		volatile u32 reserved			: 19;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_REGION_AC_WIDTH;

typedef union {
	struct {
		volatile u32 edgetol			: 8;	/* edge threshold, 초기값 */
		volatile u32 reserved			: 24;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_EDGE_THRESHOLD;

typedef union {
	struct {
		volatile u32 p_start0_d 		: 14;	/* P_START0_d */
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_P_RESULT_P_START0_D;

typedef union {
	struct {
		volatile u32 p_start1_d 		: 14;	/* P_START1_d */
		volatile u32 reserved			: 18;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_P_RESULT_P_START1_D;

typedef union {
	struct {
		volatile u32 ac_theight 		: 13;	/* AC Total HEIGHT */
		volatile u32 reserved			: 19;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_TOTAL_SIZE_AC_THEIGHT;

typedef union {
	struct {
		volatile u32 ac_twidth			: 13;	/* AC Total WIDTH */
		volatile u32 reserved			: 19;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_TOTAL_SIZE_AC_TWIDTH;

typedef union {
	struct {
		/* if ([5] == 0) dp = dp + [4:0] else if ([5] == 1) dp = dp - [4:0] */
		volatile u32 ac_dp_offset		: 6;
		volatile u32 reserved			: 26;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_OFFSET;

typedef union {
	struct {
		/* auto ac enable, ac start autometicaly */
		volatile u32 ac_auto_enable		: 1;
		volatile u32 reserved			: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_AUTO_ENABLE;

typedef union {
	struct {
		volatile u32 ac_enable_one		: 1;	/* ac enable one time */
		volatile u32 reserved			: 31;
	} asbits;
	volatile u32 as32bits;
} CSS_S3DC_AC_ONE_ENABLE;

struct css_s3dc_reg_data {
	CSS_S3DC_TOP_EN_RESET			  top_en_reset;					/* 0x1000 */
	CSS_S3DC_BLT_TEST_MODE0			  blt_test_mode0;				/* 0x3000 */
	CSS_S3DC_BLT_FR_COLOR_L			  blt_fillrect_color_l;			/* 0x3008 */
	CSS_S3DC_BLT_FR_COLOR_H			  blt_fillrect_color_h;			/* 0x300C */
	CSS_S3DC_BLT_RGNST0_WIDTH_L 	  blt_cmd_width_l;				/* 0x3024 */
	CSS_S3DC_BLT_RGNST0_WIDTH_H 	  blt_cmd_width_h;				/* 0x3028 */
	CSS_S3DC_BLT_RGNST0_HEIGHT		  blt_cmd_height;				/* 0x302C */
	CSS_S3DC_BLT_RGNST0_SRCSM_WIDTH_L blt_cmd_src_sm_width_l;		/* 0x3040 */
	CSS_S3DC_BLT_RGNST0_SRCSM_WIDTH_H blt_cmd_src_sm_width_h;		/* 0x3044 */
	CSS_S3DC_BLT_RGNST0_DST_XST 	  blt_cmd_dst_x_start;			/* 0x3048 */
	CSS_S3DC_BLT_RGNST0_DST_YST 	  blt_cmd_dst_y_start;			/* 0x304C */
	CSS_S3DC_BLT_RGNST0_DSTSM_ADDR_L  blt_cmd_dst_sm_addr_l;		/* 0x3050 */
	CSS_S3DC_BLT_RGNST0_DSTSM_ADDR_H  blt_cmd_dst_sm_addr_h;		/* 0x3054 */
	CSS_S3DC_BLT_RGNST0_DSTSM_WIDTH_L blt_cmd_dst_sm_width_l;		/* 0x3058 */
	CSS_S3DC_BLT_RGNST0_DSTSM_WIDTH_H blt_cmd_dst_sm_width_h;		/* 0x305C */
	CSS_S3DC_BLT_RGNST0_DSTSM_HEIGHT  blt_cmd_dst_sm_height;		/* 0x3060 */
	CSS_S3DC_BLT_RGNST0_SRC2SM_WIDTH_L blt_cmd_src2_sm_width_l;		/* 0x3074 */
	CSS_S3DC_BLT_RGNST0_SRC2SM_WIDTH_H blt_cmd_src2_sm_width_h;		/* 0x3078 */
	CSS_S3DC_BLT_TEST_MODE1 		   blt_cmd_host_data;			/* 0x3080 */
	CSS_S3DC_BLT_FMT				   blt_cmd_d_fmt;				/* 0x3084 */
	CSS_S3DC_BLT_MODE_R 			   blt_mode;					/* 0x30A0 */
	CSS_S3DC_BLT_TEST_MODE2 		   blt_sbl;						/* 0x30BC */
	CSS_S3DC_BLT_FULL_EN			   blt_full_en;					/* 0x30C0 */
	CSS_S3DC_BLT_SCALE_SET0 		   blt_filter_scaler_en;		/* 0x30E4 */
	CSS_S3DC_BLT_RGNST1_DSTSM_ADDR_L   blt_cmd_dst_sm_addr2_l;		/* 0x30E8 */
	CSS_S3DC_BLT_RGNST1_DSTSM_ADDR_H   blt_cmd_dst_sm_addr2_h;		/* 0x30EC */
	CSS_S3DC_BLT_IN_FMT 			   blt_input_type;				/* 0x3140 */
	CSS_S3DC_BLT_OUT_FMT			   blt_out_type;				/* 0x3144 */
	CSS_S3DC_BLT_YUV_SET			   blt_out_yuv_type;			/* 0x3148 */
	CSS_S3DC_CF_WR_CONDITION		   cf_wr_condition;				/* 0x5000 */
	CSS_S3DC_CF_S3D_FMT 			   cf_s3d_fmt;					/* 0x5004 */
	CSS_S3DC_CF_ENABLE				   cf_enable;					/* 0x5008 */
	CSS_S3DC_CF_CROP_REGION0_L_START0  cf_crop_line_start_0;		/* 0x500C */
	CSS_S3DC_CF_CROP_REGION0_L_WIDTH0  cf_crop_line_count_0;		/* 0x5010 */
	CSS_S3DC_CF_CROP_REGION0_P_START0  cf_crop_pixel_start_0;		/* 0x5014 */
	CSS_S3DC_CF_CROP_REGION0_P_WIDTH0  cf_crop_pixel_count_0;		/* 0x5018 */
	CSS_S3DC_CF_WADDR0_OFF0_ADDR0_L    cf_off0_address0_l;			/* 0x501C */
	CSS_S3DC_CF_WADDR0_OFF0_ADDR0_H    cf_off0_address0_h;			/* 0x5020 */
	CSS_S3DC_CF_WADDR0_OFF1_ADDR0_L    cf_off1_address0_l;			/* 0x5024 */
	CSS_S3DC_CF_WADDR0_OFF1_ADDR0_H    cf_off1_address0_h;			/* 0x5028 */
	CSS_S3DC_CF_SYNC				   cf_sync_ctrl;				/* 0x502C */
	CSS_S3DC_CF_CROP_REGION1_L_START1  cf_crop_line_start_1;		/* 0x5030 */
	CSS_S3DC_CF_CROP_REGION1_L_WIDTH1  cf_crop_line_count_1;		/* 0x5034 */
	CSS_S3DC_CF_CROP_REGION1_P_START1  cf_crop_pixel_start_1;		/* 0x5038 */
	CSS_S3DC_CF_CROP_REGION1_P_WIDTH1  cf_crop_pixel_count_1;		/* 0x503C */
	CSS_S3DC_CF_WADDR1_OFF0_ADDR1_L    cf_off0_address1_l;			/* 0x5040 */
	CSS_S3DC_CF_WADDR1_OFF0_ADDR1_H    cf_off0_address1_h;			/* 0x5044 */
	CSS_S3DC_CF_WADDR1_OFF1_ADDR1_L    cf_off1_address1_l;			/* 0x5048 */
	CSS_S3DC_CF_WADDR1_OFF1_ADDR1_H    cf_off1_address1_h;			/* 0x504C */
	CSS_S3DC_CF_CAM_SELECT			   cf_cam_lr_change;			/* 0x5054 */
	CSS_S3DC_CF_SCALE_FACTOR0_WS_WD0   cf_scale_ws_wd0;				/* 0x5058 */
	CSS_S3DC_CF_SCALE_FACTOR0_HS_HD0   cf_scale_hs_hd0;				/* 0x505C */
	CSS_S3DC_CF_CAM_INFO0			   cf_cam0_total_pixel_count;	/* 0x5060 */
	CSS_S3DC_CF_DP_STEP 			   cf_dp_step;					/* 0x5064 */
	CSS_S3DC_CF_TILT_BYPASS 		   cf_tilt_bypass;				/* 0x5068 */
	CSS_S3DC_CF_TILT_TANGENT		   cf_tilt_tangent;				/* 0x506C */
	CSS_S3DC_CF_TILT_RECI_PWID		   cf_tilt_reci_pwidth;			/* 0x5070 */
	CSS_S3DC_CF_SCALE_FACTOR1_WS_WD1   cf_scale_ws_wd1;				/* 0x5074 */
	CSS_S3DC_CF_SCALE_FACTOR1_HS_HD1   cf_scale_hs_hd1;				/* 0x5078 */
	CSS_S3DC_CF_P_OFFSET0			   cf_p_start_offset0;			/* 0x5080 */
	CSS_S3DC_CF_P_OFFSET1			   cf_p_start_offset1;			/* 0x5084 */
	CSS_S3DC_CF_CAM_INFO1			   cf_cam1_total_pixel_count;	/* 0x5088 */
	CSS_S3DC_CF_CAM_INFO1_CTH0		   cf_cam0_total_line_count;	/* 0x508C */
	CSS_S3DC_AC_DP_TH_STEP			   ac_dp_th_step;				/* 0x5090 */
	CSS_S3DC_CF_CAM_INFO1_CTH1		   cf_cam1_total_line_count;	/* 0x5094 */
	CSS_S3DC_CF_WARNNING			   cf_warn;						/* 0x50C0 */
	CSS_S3DC_CF_COMPENSATION_CTW0	   cf_ctw0_comp;				/* 0x50C4 */
	CSS_S3DC_CF_COMPENSATION_CTW1	   cf_ctw1_comp;				/* 0x50C8 */
	CSS_S3DC_CF_ROT_BR_BUFSIZE		   cf_br_rot_bufsize;			/* 0x50CC */
	CSS_S3DC_CF_ROT_BR_ROT_ENABLE	   cf_br_rot_enable;			/* 0x50D0 */
	CSS_S3DC_CF_ROT_BR_DEGREE_SIGN	   cf_br_rot_degree_sign;		/* 0x50D4 */
	CSS_S3DC_CF_ROT_BR_SIN_L		   cf_br_rot_sin_l;				/* 0x50D8 */
	CSS_S3DC_CF_ROT_BR_SIN_H		   cf_br_rot_sin_h;				/* 0x50DC */
	CSS_S3DC_CF_ROT_BR_COS_L		   cf_br_rot_cos_l;				/* 0x50E0 */
	CSS_S3DC_CF_ROT_BR_COS_H		   cf_br_rot_cos_h;				/* 0x50E4 */
	CSS_S3DC_CF_ROT_BR_CSC_L		   cf_br_rot_csc_l;				/* 0x50E8 */
	CSS_S3DC_CF_ROT_BR_CSC_M		   cf_br_rot_csc_m;				/* 0x50EC */
	CSS_S3DC_CF_ROT_BR_CSC_H		   cf_br_rot_csc_h;				/* 0x50F0 */
	CSS_S3DC_CF_ROT_BR_COT_L		   cf_br_rot_cot_l;				/* 0x50F4 */
	CSS_S3DC_CF_ROT_BR_COT_M		   cf_br_rot_cot_m;				/* 0x50F8 */
	CSS_S3DC_CF_ROT_BR_COT_H		   cf_br_rot_cot_h;				/* 0x50FC */
	CSS_S3DC_CF_ROT_BR_MIN_LIMIT	   cf_br_rot_min_limit;			/* 0x5100 */
	CSS_S3DC_CF_ROT 				   cf_rot_deci_block;			/* 0x5118 */
	CSS_S3DC_CF_IF_SEL0 			   cf_cam0_if_sel;				/* 0x5140 */
	CSS_S3DC_DMA_IF_RADDR0_Y_BASE_ADDR0_L	 dma_y_base_addr0_l;	/* 0x5144 */
	CSS_S3DC_DMA_IF_RADDR0_Y_BASE_ADDR0_H	 dma_y_base_addr0_h;	/* 0x5148 */
	CSS_S3DC_DMA_IF_RADDR0_C0_BASE_ADDR0_L	 dma_c0_base_addr0_l;	/* 0x514C */
	CSS_S3DC_DMA_IF_RADDR0_C0_BASE_ADDR0_H	 dma_c0_base_addr0_h;	/* 0x5150 */
	CSS_S3DC_DMA_IF_INT_TIMING0_VSBLANK0	 dma_vs_blank0;			/* 0x515C */
	CSS_S3DC_DMA_IF_INT_TIMING0_HSBLANK0	 dma_hs_blank0;			/* 0x5160 */
	CSS_S3DC_DMA_IF_INT_TIMING0_VS2HS_DEL0	 dma_vs2hs_del0;		/* 0x5164 */
	CSS_S3DC_DMA_IF_INT_TIMING0_HS2VS_DEL0	 dma_hs2vs_del0;		/* 0x5168 */
	CSS_S3DC_DMA_IF_INPUT_SIZE0_DMA_T_WIDTH0  dma_t_width0;			/* 0x516C */
	CSS_S3DC_DMA_IF_INPUT_SIZE0_DMA_T_HEIGHT0 dma_t_height0;		/* 0x5170 */
	CSS_S3DC_DMA_IF_INPUT_SIZE0_DMA_X_OFFSET0 dma_x_offset0;		/* 0x5174 */
	CSS_S3DC_DMA_IF_INPUT_SIZE0_DMA_Y_OFFSET0 dma_y_offset0;		/* 0x5178 */
	CSS_S3DC_DMA_IF_INPUT_SIZE0_DMA_P_WIDTH0  dma_p_width0;			/* 0x517C */
	CSS_S3DC_DMA_IF_INPUT_SIZE0_DMA_P_HEIGHT0 dma_p_height0;		/* 0x5180 */
	CSS_S3DC_DMA_IF_SMODE0					  dma_start_mode0;		/* 0x5184 */
	CSS_S3DC_DMA_IF_START0					  dma_start0;			/* 0x5188 */
	CSS_S3DC_CF_D_FMT0						  cf_d_fmt0;			/* 0x518C */
	CSS_S3DC_CF_D_ORDER0					  cf_d_order0;			/* 0x5190 */
	CSS_S3DC_DMA_IF_RD_BUSY0				  dma_busy0;			/* 0x519C */
	CSS_S3DC_CF_IF_SEL1 					  cf_cam1_if_sel;		/* 0x51C0 */
	CSS_S3DC_DMA_IF_RADDR1_Y_BASE_ADDR1_L	  dma_y_base_addr1_l;	/* 0x51C4 */
	CSS_S3DC_DMA_IF_RADDR1_Y_BASE_ADDR1_H	  dma_y_base_addr1_h;	/* 0x51C8 */
	CSS_S3DC_DMA_IF_RADDR1_C0_BASE_ADDR1_L	  dma_c0_base_addr1_l;	/* 0x51CC */
	CSS_S3DC_DMA_IF_RADDR1_C0_BASE_ADDR1_H	  dma_c0_base_addr1_h;	/* 0x51D0 */
	CSS_S3DC_DMA_IF_INT_TIMING1_VSBLANK1	  dma_vs_blank1;		/* 0x51DC */
	CSS_S3DC_DMA_IF_INT_TIMING1_HSBLANK1	  dma_hs_blank1;		/* 0x51E0 */
	CSS_S3DC_DMA_IF_INT_TIMING1_VS2HS_DEL1	  dma_vs2hs_del1;		/* 0x51E4 */
	CSS_S3DC_DMA_IF_INT_TIMING1_HS2VS_DEL1	  dma_hs2vs_del1;		/* 0x51E8 */
	CSS_S3DC_DMA_IF_INPUT_SIZE1_DMA_T_WIDTH1  dma_t_width1;			/* 0x51EC */
	CSS_S3DC_DMA_IF_INPUT_SIZE1_DMA_T_HEIGHT1 dma_t_height1;		/* 0x51F0 */
	CSS_S3DC_DMA_IF_INPUT_SIZE1_DMA_X_OFFSET1 dma_x_offset1;		/* 0x51F4 */
	CSS_S3DC_DMA_IF_INPUT_SIZE1_DMA_Y_OFFSET1 dma_y_offset1;		/* 0x51F8 */
	CSS_S3DC_DMA_IF_INPUT_SIZE1_DMA_P_WIDTH1  dma_p_width1;			/* 0x51FC */
	CSS_S3DC_DMA_IF_INPUT_SIZE1_DMA_P_HEIGHT1 dma_p_height1;		/* 0x5200 */
	CSS_S3DC_DMA_IF_SMODE1					  dma_start_mode1;		/* 0x5204 */
	CSS_S3DC_DMA_IF_START1					  dma_start1;			/* 0x5208 */
	CSS_S3DC_CF_D_FMT1						  cf_d_fmt1;			/* 0x520C */
	CSS_S3DC_CF_D_ORDER1					  cf_d_order1;			/* 0x5210 */
	CSS_S3DC_DMA_IF_RD_BUSY1				  dma_busy1;			/* 0x521C */
	CSS_S3DC_CPU_TRIG_EN					  cpu_trig_en;			/* 0x5240 */
	CSS_S3DC_CPU_TRIG_ST					  cpu_trig_st;			/* 0x5244 */
	CSS_S3DC_ONE_SHOT_EN					  one_shot_enable;		/* 0x5248 */
	CSS_S3DC_WB_COMMON_SET_CTRL 			  wb_set_ctrl;			/* 0x5400 */
	CSS_S3DC_WB_COMMON_SET_PARAM			  wb_set_param;			/* 0x5404 */
	CSS_S3DC_WB_COMMON_SET_STABLECNT		  wb_set_stablecnt;		/* 0x5410 */
	CSS_S3DC_WB_AVG_INFO_WB_CAM 			  wb_cam_avg;			/* 0x5420 */
	CSS_S3DC_WB_AVG_INFO_WB_CUR_CAM 		  wb_cur_cam_avg;		/* 0x5424 */
	CSS_S3DC_WB_AVG_INFO_WB_CAM0_VAL		  wb_cam0_val;			/* 0x5428 */
	CSS_S3DC_WB_AVG_INFO_WB_CAM1_VAL		  wb_cam1_val;			/* 0x542C */
	CSS_S3DC_COLOR_CON_EN					  color_con_en;			/* 0x5800 */
	CSS_S3DC_COLOR_CON0_CAM0_HUE_DEGREE 	  cam0_hue_degree;		/* 0x5804 */
	CSS_S3DC_COLOR_CON0_CAM0_GAIN			  cam0_gain;			/* 0x5808 */
	CSS_S3DC_COLOR_CON0_CAM0_COLOR_TH		  cam0_color_th;		/* 0x580C */
	CSS_S3DC_COLOR_CON0_CAM0_CGAIN			  cam0_cgain;			/* 0x5810 */
	CSS_S3DC_COLOR_CON0_CAM0_BRIGHT_TH		  cam0_bright_th;		/* 0x5814 */
	CSS_S3DC_COLOR_CON0_CAM0_YGAIN			  cam0_ygain;			/* 0x5818 */
	CSS_S3DC_COLOR_CON0_CAM0_RGB_BRIGHT_OGAIN cam0_rgb_bright_ogain;/* 0x581C */
	CSS_S3DC_COLOR_CON1_CAM1_HUE_DEGREE 	  cam1_hue_degree;		/* 0x5840 */
	CSS_S3DC_COLOR_CON1_CAM1_GAIN			  cam1_gain;			/* 0x5844 */
	CSS_S3DC_COLOR_CON1_CAM1_COLOR_TH		  cam1_color_th;		/* 0x5848 */
	CSS_S3DC_COLOR_CON1_CAM1_CGAIN			  cam1_cgain;			/* 0x584C */
	CSS_S3DC_COLOR_CON1_CAM1_BRIGHT_TH		  cam1_bright_th;		/* 0x5850 */
	CSS_S3DC_COLOR_CON1_CAM1_YGAIN			  cam1_ygain;			/* 0x5854 */
	CSS_S3DC_COLOR_CON1_CAM1_RGB_BRIGHT_OGAIN cam1_rgb_bright_ogain;/* 0x5858 */
	CSS_S3DC_Y2R_COEFF_SET_R11_OFFSET		  coeff_set_r11_offset;	/* 0x59C0 */
	CSS_S3DC_Y2R_COEFF_SET_R11_COEFF		  coeff_set_r11_coeff;	/* 0x59F0 */
	CSS_S3DC_AC_REGION_Y_ST 				  ac_region_y;			/* 0x5C04 */
	CSS_S3DC_AC_REGION_X_ST 				  ac_region_x;			/* 0x5C08 */
	CSS_S3DC_AC_REGION_AC_HEIGHT			  ac_region_height;		/* 0x5C0C */
	CSS_S3DC_AC_REGION_AC_WIDTH 			  ac_region_width;		/* 0x5C10 */
	CSS_S3DC_AC_EDGE_THRESHOLD				  ac_edge_th;			/* 0x5C14 */
	CSS_S3DC_AC_P_RESULT_P_START0_D 		  ac_p_start0_d;		/* 0x5C54 */
	CSS_S3DC_AC_P_RESULT_P_START1_D 		  ac_p_start1_d;		/* 0x5C58 */
	CSS_S3DC_AC_TOTAL_SIZE_AC_THEIGHT		  ac_total_height;		/* 0x5C64 */
	CSS_S3DC_AC_TOTAL_SIZE_AC_TWIDTH		  ac_total_width;		/* 0x5C68 */
	CSS_S3DC_AC_OFFSET						  ac_dp_offset;			/* 0x5C90 */
	CSS_S3DC_AC_AUTO_ENABLE 				  ac_auto_enable;		/* 0x5C94 */
	CSS_S3DC_AC_ONE_ENABLE					  ac_once_enable;		/* 0x5CAC */
};

struct css_s3dc_set {
	struct css_rect rect;
	u32				fmt;
	u32				align;
};

struct css_s3dc_buf_set {
#ifdef CONFIG_ODIN_ION_SMMU
	struct ion_handle_data ion_hdl;
#endif
	unsigned int	index;
	unsigned int	buf_size;
	unsigned int	dma_addr;
	void			*cpu_addr;
};

struct css_s3dc_config {
	u8 in_path0;
	u8 in_path1;
	u8 out_path;
	u8 outmode;
	dma_addr_t inaddr0;
	dma_addr_t inaddr1;
	struct css_s3dc_set input0;
	struct css_s3dc_set input1;
	struct css_s3dc_set output;
};

typedef struct NXSCContext {
	int 					s3dc_enable;
	struct css_s3dc_config	config;
	int 					config_done;
	unsigned int			mem_size[6];
	int 					v4l2_behavior;
	void					*v4l2_preview_buf_set;
	void					*v4l2_capture_buf_set;
	void					*v4l2_preview_cam_fh;
	void					*v4l2_capture_cam_fh;

	/* compensation */
	unsigned int compensation_tilt_bypass;
	unsigned int compensation_tilt_tangent;
	unsigned int compensation_rot_degree;

	/* Direct IF, DMA input */
	int						cfi_cam_if_sel[2];
	int						cfi_y_addr[2];
	int						cfi_c0_addr[2];

	int						cfi_dma_total_w[2];
	int						cfi_dma_total_h[2];
	int						cfi_dma_crop_x[2];
	int						cfi_dma_crop_y[2];
	int						cfi_dma_crop_w[2];
	int						cfi_dma_crop_h[2];
	int						cfi_dma_start_mode[2];
	int						cfi_dma_format[2];
	int						cfi_dma_data_order[2];

	/* Drawing surface */
	unsigned int			frame_buffer_sm_addr[6];
	int						fb_change_flag;
	int						fb_format;
	int						fb_order;
	/* BLT 출력 이미지의 byte단위의 총 데이터량 */
	int						fb_data_size;
	int						page_flip_on;

	/* BLT */
	int						blt_sbl_val;
	int						blto_yuv_enable;/* RGB or YUV output */
	int						blto_yuv_format;/* 444, 422packed, 422planar, 420 */
	int						blto_yuv422_mode;/* 422 mode detail set */
	int						blto_yuv420_mode;/* 420 mode detail set */
	int						blto_data_order;/* RGB or u,v order */
	int						blto_z_order;/* YUV444 dummy position */
	int						blto_24bit_en;/* BLT출력이 24bit인 경우 enable */
	int						blt_dst_format;

	int						blti_format;	/* cf write 888/8888 */
	int						blti_24bit_en;/* BLT input 이 RGB888인경우 enable */
	int						blti_z_order;
	int						blti_data_order;
	int						blt_src_format;

	int						blt_fullsize_formatting_on;

	/* CF엔진에서 출력되는 이미지 사이즈(실제 LCD사이즈와 다를 수 있음) */
	int						cf_out_width;
	int						cf_out_height;

	int						curr_mode;	/* current S3D mode(include 2D mode) */
	int						pre_mode;	/* previous S3D mode */

	/* Display Size */
	int						display_width;
	int						display_height;

	/* CF engine */
	int						cf_enable;

	int						deci_filter_en;
	int						s3d_type;
	int						cam_select;

	int						deci_mode;
	int						change_lr;
	int						manual_en;
	int						s3d_en;

	int						tilt_bypass_en;
	int						rotate_en;
	/* camera 출력 사이즈 */
	int						cam0_width;
	int						cam0_height;
	int						cam1_width;
	int						cam1_height;

	/* camera로 부터 받을 실제 이미지 사이즈 */
	int						cam0_crop_width;
	int						cam0_crop_height;
	int						cam1_crop_width;
	int						cam1_crop_height;

	int						cam0_crop_w_correction;
	int						cam1_crop_w_correction;

	int						cam0_pixel_start;
	int						cam1_pixel_start;
	int						cam0_line_start;
	int						cam1_line_start;

	/* hw내부적으로 16bit로 줄여 처리할 것인지의 여부 */
	int						cam0_wr_16bit;
	int						cam1_wr_16bit;

	/* auto convergence 관련 */
	int						ac_edge_tol;
	int						ac_enable;
	int						ac_quick_enable;
	int						ac_area_x_start;
	int						ac_area_y_start;
	int						ac_area_width;
	int						ac_area_height;

	/* auto white balance 관련 */
	int						wb_enable;
	int						hwb_enable;
	int						wb_hold;
	int						wb_auto_stop_en;
	int						hwb_value;
	int						wb_avg_up_cnt;
	int						wb_fstable_cnt;

	/* color control 관련 */
	int						color_ctrl_en[2];
	int						color_pattern_en;
	int						color_no_color_en;
} NXSCContext;


/*----------------------------------------------------------------
 *	Defines
 *----------------------------------------------------------------*/
/* BLT out data format */
enum {
	BLT_OUT_YUV444_1PLANE_UNPACKED	= 0,
	BLT_OUT_YUV444_1PLANE_PACKED	= 1,
	BLT_OUT_YUV422_1PLANE_PACKED	= 2,
	BLT_OUT_YUV422_PLANAR			= 3,
	BLT_OUT_Y_V_12					= 5, /* YVU 4:2:0	*/
	BLT_OUT_Y_V_21					= 6,
	BLT_OUT_N_V_12					= 7, /* Y/CbCr 4:2:0 */
	BLT_OUT_N_V_21					= 8, /* Y/CrCb 4:2:0 */
	BLT_OUT_IMC_1					= 9,
	BLT_OUT_IMC_2					= 10,
	BLT_OUT_IMC_3					= 11,
	BLT_OUT_IMC_4					= 12,
	BLT_OUT_RGB565					= 13,
	BLT_OUT_RGB888					= 14,
	BLT_OUT_RGB8888 				= 15,
	BLT_OUT_NONE					= 16
};

/* CF input data format */
typedef struct CFInputFormatInfo {
	int		c0_input_mode;
	int		c1_input_mode;
	int		c0_format;
	int		c1_format;
	int		c0_order;
	int		c1_order;
} CFInputFormatInfo, *PCFInputFormatInfo;

/* CF input (DMA) size info */
typedef struct CFInputDMASizeInfo {
	int		c0_total_w;
	int		c0_total_h;
	int		c0_crop_x;
	int		c0_crop_y;
	int		c0_crop_w;
	int		c0_crop_h;
	int		c1_total_w;
	int		c1_total_h;
	int		c1_crop_x;
	int		c1_crop_y;
	int		c1_crop_w;
	int		c1_crop_h;
} CFInputDMASizeInfo, *PCFInputDMASizeInfo;

/* CF-eng size info */
typedef struct CFSizeInfo {
	int			cam0_width;
	int			cam0_height;
	int			cam0_crop_w;
	int			cam0_crop_h;
	int			cam1_width;
	int			cam1_height;
	int			cam1_crop_w;
	int			cam1_crop_h;
	int			cf_out_w;
	int			cf_out_h;
} CFSizeInfo, *PCFSizeInfo;

/* BLT out format info */
typedef struct BLTInfo {
	/* various YUV/RGB format */
	int			blto_format;
	int			blto_order;
	/* CF write format	: RGB565/888/8888 (=BLT intput format) */
	int			blti_format;

} BLTInfo, *PBLTInfo;

/* CF Input DMA Start mode, Address info */
typedef struct CFInputDMAInfo {
	int		cfi_dma0_start_mode;
	int		cfi_dma0_addr_y;
	int		cfi_dma0_addr_c0;
	int		cfi_dma1_start_mode;
	int		cfi_dma1_addr_y;
	int		cfi_dma1_addr_c0;
} CFInputDMAInfo, *PCFInputDMAInfo;


typedef struct {
	int 			rot_en;
	int 			degree_sign;
	unsigned short	rot_sin_l;
	unsigned short	rot_sin_h;
	unsigned short	rot_cos_l;
	unsigned short	rot_cos_h;
	unsigned short	rot_csc_l;
	unsigned short	rot_csc_m;
	unsigned short	rot_csc_h;
	unsigned short	rot_cot_l;
	unsigned short	rot_cot_m;
	unsigned short	rot_cot_h;
} BrRotationInfo;

/* CF input mode(Do not change) */
enum {
	CF_INPUT_DIRECT = 0,
	CF_INPUT_DMA	= 1,
	CF_INPUT_NONE	= 2,
};

/* DMA Start mode(Do not change) */
enum {
	DMA_START_HOST_CMD		= 0,
	DMA_START_DIR_IF_VSYNC	= 1,
	DMA_START_EMBEDDED_TIME = 2,
	DMA_START_NONE			= 15
};

/* CF Input data format (Do not change) */
enum {
	YUV444_1PLANE_UNPACKED	= 0,
	YUV444_1PLANE_PACKED	= 1,
	YUV422_1PLANE_PACKED	= 2,
	YUV422_2PLANE_PACKED	= 3,
	NONE					= 16
};

/* Color Control */
enum {
	COLOR_Y_GAIN		= 0,
	COLOR_C_GAIN		= 1,
	COLOR_LC_GAIN		= 2,
	COLOR_HC_GAIN		= 3,
	COLOR_LY_GAIN		= 4,
	COLOR_HY_GAIN		= 5,
	COLOR_O_GAIN		= 6,
	L_COLOR_THRESHOLD	= 7,
	H_COLOR_THRESHOLD	= 8,
	L_BRIGHT_THRESHOLD	= 9,
	H_BRIGHT_THRESHOLD	= 10,
	COLOR_HUE			= 11,
	COLOR_BRIGHT		= 12,
};

typedef enum {
	INT_MODE_EDGE	= 0,
	INT_MODE_LEVEL	= 2
} top_int_mode;

NXSCContext *s3dc_get_context(void);
int	s3dc_hw_init(void);
int	s3dc_hw_deinit(void);
int	s3dc_config_context(struct css_s3dc_config *config, unsigned int mem_base);
int	s3dc_create_context(void);
void s3dc_destroy_context(void);
int s3dc_int_enable(unsigned int mask);
int s3dc_int_disable(unsigned int mask);
int s3dc_int_clear(unsigned int int_source);
int	s3dc_set_int_mode(top_int_mode mode);
void s3dc_stop_trans(void);
int s3dc_wait_capture_done(unsigned int msec);
void s3dc_set_framebuffer_address(unsigned int addr);
unsigned int s3dc_get_framebuffer_address(void);
int s3dc_schedule_zsl_load_work(void);
int s3dc_set_v4l2_stereo_buf_set(int buf_type, struct css_bufq *bufq);
int s3dc_set_v4l2_current_behavior(int behavior);
int s3dc_set_v4l2_preview_cam_fh(void *capfh);
int s3dc_set_v4l2_capture_cam_fh(void *capfh);
void s3dc_set_frame_count(unsigned int frcnt);
unsigned int s3dc_get_frame_count(void);

#endif /* __ODIN_S3DC_H__ */
