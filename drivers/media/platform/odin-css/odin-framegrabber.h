/*
 * Frame Grabber driver
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

#ifndef __ODIN_FRAME_GRABBER_H__
#define __ODIN_FRAME_GRABBER_H__


#define RESET					0x0000
#define CLK_CTRL				0x0004
#define PARA_SET				0x0008
#define DONE_ST 				0x000C
#define ERR_ST					0x0010
#define INT_MASK				0x0014
#define INT_CLR 				0x0018
#define INT_FLG 				0x001C
#define PATH0_VSYNC_DLY 		0x0020
#define STROB					0x0024
#define PATH0_CROP_OFFSET		0x0028
#define PATH0_CROP_VALID		0x002C
#define PATH0_EFFECT			0x0030
#define PATH0_SCL_SET			0x0034
#define PATH0_SCL_SIZE			0x0038
#define PATH0_SCL_FACTOR		0x003C
#define PATH0_DITHERING_SET		0x0040
#define PATH0_SCL_MONITOR		0x0044
#define PATH1_VSYNC_DLY 		0x0048
#define PATH1_CROP_OFFSET		0x004C
#define PATH1_CROP_VALID		0x0050
#define PATH1_EFFECT			0x0054
#define PATH1_SCL_SET			0x0058
#define PATH1_SCL_SIZE			0x005C
#define PATH1_SCL_FACTOR		0x0060
#define PATH1_DITHERING_SET 	0x0064
#define PATH1_SCL_MONITOR		0x0068
#define PATH01_DRAM_ADDR_SET	0x006C
#define PATH01_DRAM_ADDR_0_Y	0x0070
#define PATH01_DRAM_ADDR_0_U	0x0074
#define PATH01_DRAM_ADDR_0_V	0x0078
#define PATH01_DRAM_ADDR_1_Y	0x007C
#define PATH01_DRAM_ADDR_1_U	0x0080
#define PATH01_DRAM_ADDR_1_V	0x0084
#define PATH01_DRAM_ADDR_2_Y	0x0088
#define PATH01_DRAM_ADDR_2_U	0x008C
#define PATH01_DRAM_ADDR_2_V	0x0090
#define PATH01_DRAM_ADDR_3_Y	0x0094
#define PATH01_DRAM_ADDR_3_U	0x0098
#define PATH01_DRAM_ADDR_3_V	0x009C
#define PATH01_DRAM_ADDR_4_Y	0x00A0
#define PATH01_DRAM_ADDR_4_U	0x00A4
#define PATH01_DRAM_ADDR_4_V	0x00A8
#define PATH01_DRAM_ADDR_5_Y	0x00AC
#define PATH01_DRAM_ADDR_5_U	0x00B0
#define PATH01_DRAM_ADDR_5_V	0x00B4
#define PATH01_DRAM_ADDR_6_Y	0x00B8
#define PATH01_DRAM_ADDR_6_U	0x00BC
#define PATH01_DRAM_ADDR_6_V	0x00C0
#define PATH01_DRAM_ADDR_7_Y	0x00C4
#define PATH01_DRAM_ADDR_7_U	0x00C8
#define PATH01_DRAM_ADDR_7_V	0x00CC
#define FD_VSYNC_DLY			0x00D0
#define FD_CROP_OFFSET			0x00D4
#define FD_CROP_VALID			0x00D8
#define FD_SCALER_SIZE			0x00DC
#define FD_SCALER_FACTOR		0x00E0
#define FD_DRAM_ADDR			0x00E4
#define RAW01_ST_SET			0x00E8
#define RAW0_ST_SIZE			0x00EC
#define RAW0_ST_DRAM_ADDR		0x00F0
#define JPEGST_DRAM_ADDR		0x00F4
#define JPEGST_TOTAL_SIZE		0x00F8
#define RAW1_ST_SIZE			0x00FC
#define RAW1_ST_DRAM_ADDR		0x0100
#define RAW0_LD_SET 			0x0104
#define RAW0_LD_SIZE			0x0108
#define RAW0_LD_DRAM_ADDR_Y 	0x010C
#define RAW0_LD_DRAM_ADDR_U 	0x0110
#define RAW0_LD_DRAM_ADDR_V 	0x0114
#define RAW1_LD_SET 			0x0118
#define RAW1_LD_SIZE			0x011C
#define RAW1_LD_DRAM_ADDR_Y		0x0120
#define RAW1_LD_DRAM_ADDR_U		0x0124
#define RAW1_LD_DRAM_ADDR_V		0x0128
#define CRC_CLEAR_SET			0x012C
#define PATH0_Y_H_32			0x0130
#define PATH0_Y_L_32			0x0134
#define PATH0_U_H_32			0x0138
#define PATH0_U_L_32			0x013C
#define PATH0_V_H_32			0x0140
#define PATH0_V_L_32			0x0144
#define PATH1_Y_H_32			0x0148
#define PATH1_Y_L_32			0x014C
#define PATH1_U_H_32			0x0150
#define PATH1_U_L_32			0x0154
#define PATH1_V_H_32			0x0158
#define PATH1_V_L_32			0x015C
#define FD_Y_H_32				0x0160
#define FD_Y_L_32				0x0164
#define DEBUG_0 				0x0168
#define DEBUG_1 				0x016C
#define DEBUG_2 				0x0170
#define DEBUG_3 				0x0174
#define AXI_CACHE_CTRL			0x0178

typedef union {
	struct {
		volatile unsigned int path_0_rst		: 1;
		volatile unsigned int path_1_rst		: 1;
		volatile unsigned int path_fd_rst		: 1;
		volatile unsigned int path_raw0_st_rst	: 1;
		volatile unsigned int path_raw1_st_rst	: 1;
		volatile unsigned int path_raw0_ld_rst	: 1;
		volatile unsigned int path_raw1_ld_rst	: 1;
		volatile unsigned int path_3d_rst		: 1;
		volatile unsigned int raw0_st_ctrl_rst	: 1;
		volatile unsigned int raw1_st_ctrl_rst	: 1;
		volatile unsigned int jpeg_st_ctrl_rst	: 1;
		volatile unsigned int jpeg_enc_ctrl_rst	: 1;
		volatile unsigned int reserved0			: 20;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RESET_CTRL;

typedef union {
	struct {
		volatile unsigned int path_0_clk		: 1;
		volatile unsigned int path_1_clk		: 1;
		volatile unsigned int path_fd_clk		: 1;
		volatile unsigned int path_raw0_st_clk	: 1;
		volatile unsigned int path_raw1_st_clk	: 1;
		volatile unsigned int path_raw0_ld_clk	: 1;
		volatile unsigned int path_raw1_ld_clk	: 1;
		volatile unsigned int path_3d_clk		: 1;
		volatile unsigned int raw0_st_ctrl_clk	: 1;
		volatile unsigned int raw1_st_ctrl_clk	: 1;
		volatile unsigned int jpeg_st_ctrl_clk	: 1;
		volatile unsigned int jpeg_enc_ctrl_clk	: 1;
		volatile unsigned int reserved0			: 20;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_CLK_CTRL;

typedef union {
	struct {
		volatile unsigned int path_0_para_set			: 1;
		volatile unsigned int path_1_para_set			: 1;
		volatile unsigned int path_fd_para_set			: 1;
		volatile unsigned int path_raw0_st_para_set		: 1;
		volatile unsigned int path_raw1_st_para_set		: 1;
		volatile unsigned int path_raw0_ld_para_set		: 1;
		volatile unsigned int path_raw1_ld_para_set		: 1;
		volatile unsigned int jpeg_st_ctrl_para_set		: 1;
		volatile unsigned int path_0_mem_con			: 2;
		volatile unsigned int path_1_mem_con			: 2;
		volatile unsigned int path_fd_mem_con			: 2;
		volatile unsigned int path_raw0_st_mem_con		: 2;
		volatile unsigned int path_raw1_st_mem_con		: 2;
		volatile unsigned int reserved0					: 14;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PARA_SET;

typedef union {
	struct {
		volatile unsigned int path_0_busi_cap_done		: 1;
		volatile unsigned int path_0_busi_pre_done		: 1;
		volatile unsigned int path_0_jpgi_done			: 1;
		volatile unsigned int path_1_busi_cap_done		: 1;
		volatile unsigned int path_1_busi_pre_done		: 1;
		volatile unsigned int path_1_jpgi_done			: 1;
		volatile unsigned int path_fd_done				: 1;
		volatile unsigned int path_raw0_st_busi_done	: 1;
		volatile unsigned int path_raw1_st_busi_done	: 1;
		volatile unsigned int path_raw0_ld_done			: 1;
		volatile unsigned int path_raw1_ld_done			: 1;
		volatile unsigned int path_3d_0_done			: 1;
		volatile unsigned int path_3d_1_done			: 1;
		volatile unsigned int raw0_st_ctr1_done			: 1;
		volatile unsigned int jpeg_st_ctrl_done			: 1;
		volatile unsigned int reserved0					: 17;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_DONE_STATE;

typedef union {
	struct {
		volatile unsigned int path_0_bof_err		: 1;
		volatile unsigned int path_0_roop_err		: 1;
		volatile unsigned int path_1_bof_err		: 1;
		volatile unsigned int path_1_roop_err		: 1;
		volatile unsigned int path_fd_bof_err		: 1;
		volatile unsigned int path_raw0_st_bof_err	: 1;
		volatile unsigned int path_raw1_st_bof_err	: 1;
		volatile unsigned int jpegst_bof_err		: 1;
		volatile unsigned int jpif_err				: 1;
		volatile unsigned int reserved0				: 23;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_ERROR_STATE;

typedef union {
	struct { /* 0 : mask 1: unmask */
		volatile unsigned int path_0_busi_intr_mask		: 1;
		volatile unsigned int path_0_jpgi_intr_mask		: 1;
		volatile unsigned int path_1_busi_intr_mask		: 1;
		volatile unsigned int path_1_jpgi_intr_mask		: 1;
		volatile unsigned int path_fd_intr_mask			: 1;
		volatile unsigned int path_raw0_st_intr_mask	: 1;
		volatile unsigned int path_raw1_st_intr_mask	: 1;
		volatile unsigned int path_raw0_ld_intr_mask	: 1;
		volatile unsigned int path_raw1_ld_intr_mask	: 1;

		volatile unsigned int path_3d_0_intr_mask		: 1;
		volatile unsigned int path_3d_1_intr_mask		: 1;
		volatile unsigned int jpeg_st_ctrl_intr_mask	: 1;

		volatile unsigned int path_0_vsync_intr_mask	: 1;
		volatile unsigned int path_1_vsync_intr_mask	: 1;
		volatile unsigned int fd_vsync_intr_mask		: 1;
		volatile unsigned int err_int_mask				: 1;
		volatile unsigned int reserved0					: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_INT_MASK;

typedef union {
	struct {
		volatile unsigned int path_0_busi_intr_clear	: 1;
		volatile unsigned int path_0_jpgi_intr_clear	: 1;
		volatile unsigned int path_1_busi_intr_clear	: 1;
		volatile unsigned int path_1_jpgi_intr_clear	: 1;
		volatile unsigned int path_fd_intr_clear		: 1;
		volatile unsigned int path_raw0_st_intr_clear	: 1;
		volatile unsigned int path_raw1_st_intr_clear	: 1;
		volatile unsigned int path_raw0_ld_intr_clear	: 1;
		volatile unsigned int path_raw1_ld_intr_clear	: 1;
		volatile unsigned int path_3d_0_intr_clear		: 1;
		volatile unsigned int path_3d_1_intr_clear		: 1;
		volatile unsigned int jpeg_st_ctrl_intr_clear	: 1;
		volatile unsigned int path_0_vsync_intr_clear	: 1;
		volatile unsigned int path_1_vsync_intr_clear	: 1;
		volatile unsigned int fd_vsync_intr_clear		: 1;
		volatile unsigned int err_int_clear				: 1;
		volatile unsigned int reserved0					: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_INT_CLR;

typedef union {
	struct {
		volatile unsigned int path_0_busi_intr_flg	: 1;
		volatile unsigned int path_0_jpgi_intr_flg	: 1;
		volatile unsigned int path_1_busi_intr_flg	: 1;
		volatile unsigned int path_1_jpgi_intr_flg	: 1;
		volatile unsigned int path_fd_intr_flg		: 1;
		volatile unsigned int path_raw0_st_intr_flg	: 1;
		volatile unsigned int path_raw1_st_intr_flg	: 1;
		volatile unsigned int path_raw0_ld_intr_flg	: 1;
		volatile unsigned int path_raw1_ld_intr_flg	: 1;
		volatile unsigned int path_3d_0_intr_flg	: 1;
		volatile unsigned int path_3d_1_intr_flg	: 1;
		volatile unsigned int jpeg_st_ctrl_intr_flg	: 1;
		volatile unsigned int path_0_vsync_intr_flg	: 1;
		volatile unsigned int path_1_vsync_intr_flg	: 1;
		volatile unsigned int fd_vsync_intr_flg		: 1;
		volatile unsigned int err_int_flg			: 1;
		volatile unsigned int reserved0				: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_INT_FLG;

typedef union {
	struct {
		volatile unsigned int vsync_delay_0		: 16;
		volatile unsigned int strobe_on			: 1;
		volatile unsigned int reserved0			: 15;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_VSYNC_DLY;

typedef union {
	struct {
		volatile unsigned int high				: 16;
		volatile unsigned int time				: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_STROB;

typedef union {
	struct {
		volatile unsigned int path_0_offset_v	: 16;
		volatile unsigned int path_0_offset_h	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_CROP_OFFSET;

typedef union {
	struct {
		volatile unsigned int path_0_valid_v	: 16;
		volatile unsigned int path_0_valid_h	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_CROP_VALID;

typedef union {
	struct {
		volatile unsigned int path_0_sepia_v	: 8;
		volatile unsigned int path_0_sepia_u	: 8;
		volatile unsigned int path_0_efft_inst	: 3;
		volatile unsigned int reserved0			: 5;
		volatile unsigned int path_0_efft_stng	: 3;
		volatile unsigned int reserved1			: 5;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_EFFECT;

typedef union {
	struct {
		volatile unsigned int scl_0_inst		: 6;
		volatile unsigned int reserved0			: 2;
		volatile unsigned int scl_0_dst_format	: 2;
		volatile unsigned int reserved1			: 6;
		volatile unsigned int scl_0_av_mode		: 2;
		volatile unsigned int reserved2			: 6;
		volatile unsigned int scl_0_skip_mask	: 1;
		volatile unsigned int reserved3			: 7;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_SCALER_SET;

typedef union {
	struct {
		volatile unsigned int scl_0_dst_v		: 16;
		volatile unsigned int scl_0_dst_h		: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_SCALER_SIZE;

typedef union {
	struct {
		volatile unsigned int scl_0_factor_v	: 16;
		volatile unsigned int scl_0_factor_h	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_SCALER_FACTOR;

typedef union {
	struct {
		volatile unsigned int scl_0_yuv_cgain	: 8;
		volatile unsigned int scl_0_yuv_ygain	: 8;
		volatile unsigned int scl_0_yuv_pedi	: 8;
		volatile unsigned int scl_0_yuv_sel		: 1;
		volatile unsigned int scl_0_dither_on	: 1;
		volatile unsigned int reserved0			: 6;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_DITHERING_SET;

typedef union {
	struct {
		volatile unsigned int href_monitor_0	: 16;
		volatile unsigned int vsync_monitor_0	: 15;
		volatile unsigned int monitor_rst_0		: 1;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_SCALER_MONITOR;

typedef union {
	struct {
		volatile unsigned int vsync_delay_1		: 16;
		volatile unsigned int reserved0			: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_VSYNC_DLY;

typedef union {
	struct {
		volatile unsigned int path_1_offset_v	: 16;
		volatile unsigned int path_1_offset_h	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_CROP_OFFSET;

typedef union {
	struct {
		volatile unsigned int path_1_valid_v	: 16;
		volatile unsigned int path_1_valid_h	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_CROP_VALID;

typedef union {
	struct {
		volatile unsigned int path_1_sepia_v	: 8;
		volatile unsigned int path_1_sepia_u	: 8;
		volatile unsigned int path_1_efft_inst	: 3;
		volatile unsigned int reserved0			: 5;
		volatile unsigned int path_1_efft_stng	: 3;
		volatile unsigned int reserved1			: 5;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_EFFECT;

typedef union {
	struct {
		volatile unsigned int scl_1_inst		: 6;
		volatile unsigned int reserved0			: 2;
		volatile unsigned int scl_1_dst_format	: 2;
		volatile unsigned int reserved1			: 6;
		volatile unsigned int scl_1_av_mode		: 2;
		volatile unsigned int reserved2			: 6;
		volatile unsigned int scl_1_skip_mask	: 1;
		volatile unsigned int reserved3			: 7;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_SCALER_SET;

typedef union {
	struct {
		volatile unsigned int scl_1_dst_v		: 16;
		volatile unsigned int scl_1_dst_h		: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_SCALER_SIZE;

typedef union {
	struct {
		volatile unsigned int scl_1_factor_v	: 16;
		volatile unsigned int scl_1_factor_h	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_SCALER_FACTOR;

typedef union {
	struct {
		volatile unsigned int scl_1_yuv_cgain	: 8;
		volatile unsigned int scl_1_yuv_ygain	: 8;
		volatile unsigned int scl_1_yuv_pedi	: 8;
		volatile unsigned int scl_1_yuv_sel		: 1;
		volatile unsigned int scl_1_dither_on	: 1;
		volatile unsigned int reserved0			: 6;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_DITHERING_SET;

typedef union {
	struct {
		volatile unsigned int href_monitor_1	: 16;
		volatile unsigned int vsync_monitor_1	: 15;
		volatile unsigned int monitor_rst_1		: 1;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_SCALER_MONITOR;

typedef union {
	struct {
		volatile unsigned int path0_sw_addr_set		: 8;
		volatile unsigned int path0_sw_addr_reset	: 1;
		volatile unsigned int reserved0				: 7;
		volatile unsigned int path1_sw_addr_set		: 8;
		volatile unsigned int path1_sw_addr_reset	: 1;
		volatile unsigned int reserved1				: 7;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_SET;

typedef union {
	struct {
		volatile unsigned int s0_y_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_0_Y;

typedef union {
	struct {
		volatile unsigned int s0_u_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_0_U;

typedef union {
	struct {
		volatile unsigned int s0_v_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_0_V;

typedef union {
	struct {
		volatile unsigned int s1_y_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_1_Y;

typedef union {
	struct {
		volatile unsigned int s1_u_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_1_U;

typedef union {
	struct {
		volatile unsigned int s1_v_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_1_V;

typedef union {
	struct {
		volatile unsigned int s2_y_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_2_Y;

typedef union {
	struct {
		volatile unsigned int s2_u_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_2_U;

typedef union {
	struct {
		volatile unsigned int s2_v_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_2_V;

typedef union {
	struct {
		volatile unsigned int s3_y_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_3_Y;

typedef union {
	struct {
		volatile unsigned int s3_u_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_3_U;

typedef union {
	struct {
		volatile unsigned int s3_v_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_3_V;

typedef union {
	struct {
		volatile unsigned int s4_y_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_4_Y;

typedef union {
	struct {
		volatile unsigned int s4_u_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_4_U;

typedef union {
	struct {
		volatile unsigned int s4_v_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_4_V;

typedef union {
	struct {
		volatile unsigned int s5_y_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_5_Y;

typedef union
{
	struct
	{
		volatile unsigned int s5_u_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_5_U;

typedef union {
	struct {
		volatile unsigned int s5_v_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_5_V;

typedef union {
	struct {
		volatile unsigned int s6_y_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_6_Y;

typedef union {
	struct {
		volatile unsigned int s6_u_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_6_U;

typedef union {
	struct {
		volatile unsigned int s6_v_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_6_V;

typedef union {
	struct {
		volatile unsigned int s7_y_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_7_Y;

typedef union {
	struct {
		volatile unsigned int s7_u_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_7_U;

typedef union {
	struct {
		volatile unsigned int s7_v_straddr		: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH01_DRAM_ADDR_7_V;

typedef union {
	struct {
		volatile unsigned int vsync_delay_fd	: 16;
		volatile unsigned int inst_fd			: 1;
		volatile unsigned int fd_source			: 1;
		volatile unsigned int reserved0			: 14;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_FD_VSYNC_DLY;

typedef union {
	struct {
		volatile unsigned int vertical		: 16;
		volatile unsigned int horizontal	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_FD_SCALER_SIZE;

typedef union {
	struct {
		volatile unsigned int start_addr	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_FD_SCALER_DRAM_ADDR;

typedef union {
	struct {
		volatile unsigned int raw0_con_bit_mode		: 2;
		volatile unsigned int inst_raw0				: 1;
		volatile unsigned int reserved0				: 5;
		volatile unsigned int raw1_con_bit_mode		: 2;
		volatile unsigned int inst_raw1				: 1;
		volatile unsigned int reserved1				: 5;
		volatile unsigned int jpegst_con_bit_mode	: 2;
		volatile unsigned int inst_jpegst			: 1;
		volatile unsigned int sel_jpegst			: 1;
		volatile unsigned int reserved2				: 12;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW01_ST_SET;

typedef union {
	struct {
		volatile unsigned int raw0_con_src_v	: 16;
		volatile unsigned int raw0_con_src_h	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW0_ST_SIZE;

typedef union {
	struct {
		volatile unsigned int raw0_st_straddr	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW0_ST_DRAM_ADDR;

typedef union {
	struct {
		volatile unsigned int jpegst_straddr	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_JPEGST_DRAM_ADDR;

typedef union {
	struct {
		volatile unsigned int jpegst_total_size	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_JPEGST_TOTAL_SIZE;

typedef union {
	struct {
		volatile unsigned int raw1_con_src_v	: 16;
		volatile unsigned int raw1_con_src_h	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW1_ST_SIZE;

typedef union {
	struct {
		volatile unsigned int raw1_st_straddr	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW1_ST_DRAM_ADDR;

typedef union {
	struct {
		volatile unsigned int raw0_ld_img_format	: 2;
		volatile unsigned int reserved0				: 6;
		volatile unsigned int raw0_ld_raw_mode		: 2;
		volatile unsigned int reserved1				: 6;
		volatile unsigned int raw0_ld_h_bank_size	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW0_LD_SET;

typedef union {
	struct {
		volatile unsigned int raw0_ld_src_v	: 16;
		volatile unsigned int raw0_ld_src_h	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW0_LD_SIZE;

typedef union {
	struct {
		volatile unsigned int raw0_ld_y_straddr	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW0_LD_DRAM_ADDR_Y;

typedef union {
	struct {
		volatile unsigned int raw0_ld_u_straddr	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW0_LD_DRAM_ADDR_U;

typedef union {
	struct {
		volatile unsigned int raw0_ld_v_straddr	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW0_LD_DRAM_ADDR_V;

typedef union {
	struct {
		volatile unsigned int raw1_ld_img_format	: 2;
		volatile unsigned int reserved0				: 6;
		volatile unsigned int raw1_ld_raw_mode		: 2;
		volatile unsigned int reserved1				: 6;
		volatile unsigned int raw1_ld_h_bank_size	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW1_LD_SET;

typedef union {
	struct {
		volatile unsigned int raw1_ld_src_v	: 16;
		volatile unsigned int raw1_ld_src_h	: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW1_LD_SIZE;

typedef union {
	struct {
		volatile unsigned int raw1_ld_y_straddr	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW1_LD_DRAM_ADDR_Y;

typedef union {
	struct {
		volatile unsigned int raw1_ld_u_straddr	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW1_LD_DRAM_ADDR_U;

typedef union {
	struct {
		volatile unsigned int raw1_ld_v_straddr	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_RAW1_LD_DRAM_ADDR_V;

typedef union {
	struct {
		volatile unsigned int path_0_crc_clr	: 1;
		volatile unsigned int reserved0			: 3;
		volatile unsigned int path_1_crc_clr	: 1;
		volatile unsigned int reserved1			: 3;
		volatile unsigned int fd_crc_clr		: 1;
		volatile unsigned int reserved2			: 23;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_CRC_CLEAR_SET;

typedef union {
	struct {
		volatile unsigned int path0_y_h_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_Y_H_32;

typedef union {
	struct {
		volatile unsigned int path0_y_l_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_Y_L_32;

typedef union {
	struct {
		volatile unsigned int path0_u_h_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_U_H_32;

typedef union {
	struct {
		volatile unsigned int path0_u_l_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_U_L_32;

typedef union {
	struct {
		volatile unsigned int path0_v_h_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_V_H_32;

typedef union {
	struct {
		volatile unsigned int path0_v_l_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH0_V_L_32;

typedef union {
	struct {
		volatile unsigned int path1_y_h_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_Y_H_32;

typedef union {
	struct {
		volatile unsigned int path1_y_l_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_Y_L_32;

typedef union {
	struct {
		volatile unsigned int path1_u_h_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_U_H_32;

typedef union {
	struct {
		volatile unsigned int path1_u_l_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_U_L_32;

typedef union {
	struct {
		volatile unsigned int path1_v_h_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_V_H_32;

typedef union {
	struct {
		volatile unsigned int path1_v_l_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_PATH1_V_L_32;

typedef union {
	struct {
		volatile unsigned int fd_y_h_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_FD_Y_H_32;

typedef union {
	struct {
		volatile unsigned int fd_y_l_32	: 32;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_FD_Y_L_32;

typedef union {
	struct {
		volatile unsigned int awcache_path0 	: 4;
		volatile unsigned int awcache_path1 	: 4;
		volatile unsigned int awcache_fd		: 4;
		volatile unsigned int awcache_st_raw0	: 4;
		volatile unsigned int awcache_st_raw1	: 4;
		volatile unsigned int awcache_ld_raw0	: 4;
		volatile unsigned int awcache_ld_raw1	: 4;
		volatile unsigned int reserved0			: 4;
	} asbits;
	volatile unsigned int as32bits;
} CSS_SCALER_AXI_CACHE_CTRL;

typedef struct {
	CSS_SCALER_RESET_CTRL			scl_rst_ctrl;				/* 0x00 */
	CSS_SCALER_CLK_CTRL				scl_clk_ctrl;				/* 0x04 */
	CSS_SCALER_PARA_SET				scl_para_set;				/* 0x08 */
	CSS_SCALER_DONE_STATE			scl_done_state;				/* 0x0C */
	CSS_SCALER_ERROR_STATE			scl_err_state;				/* 0x10 */
	CSS_SCALER_INT_MASK				scl_int_mask;				/* 0x14 */
	CSS_SCALER_INT_CLR				scl_int_clr;				/* 0x18 */
	CSS_SCALER_INT_FLG				scl_int_flag;				/* 0x1C */
	CSS_SCALER_PATH0_VSYNC_DLY		scl_path0_vsync_dly;		/* 0x20 */
	CSS_SCALER_STROB				scl_strob;					/* 0x24 */
	CSS_SCALER_PATH0_CROP_OFFSET	scl_path0_crop_offset;		/* 0x28 */
	CSS_SCALER_PATH0_CROP_VALID		scl_path0_crop_valid;		/* 0x2C */
	CSS_SCALER_PATH0_EFFECT			scl_path0_effect;			/* 0x30 */
	CSS_SCALER_PATH0_SCALER_SET		scl_path0_scl_set;			/* 0x34 */
	CSS_SCALER_PATH0_SCALER_SIZE	scl_path0_scl_size;			/* 0x38 */
	CSS_SCALER_PATH0_SCALER_FACTOR	scl_path0_scl_factor;		/* 0x3C */
	CSS_SCALER_PATH0_DITHERING_SET	scl_path0_dither_set;		/* 0x40 */
	CSS_SCALER_PATH0_SCALER_MONITOR	scl_path0_scl_monitor;		/* 0x44 */
	CSS_SCALER_PATH1_VSYNC_DLY		scl_path1_vsync_dly;		/* 0x48 */
	CSS_SCALER_PATH1_CROP_OFFSET	scl_path1_crop_offset;		/* 0x4C */
	CSS_SCALER_PATH1_CROP_VALID		scl_path1_crop_valid;		/* 0x50 */
	CSS_SCALER_PATH1_EFFECT			scl_path1_effect;			/* 0x54 */
	CSS_SCALER_PATH1_SCALER_SET		scl_path1_scl_set;			/* 0x58 */
	CSS_SCALER_PATH1_SCALER_SIZE	scl_path1_scl_size;			/* 0x5C */
	CSS_SCALER_PATH1_SCALER_FACTOR	scl_path1_scl_factor;		/* 0x60 */
	CSS_SCALER_PATH1_DITHERING_SET	scl_path1_dither_set;		/* 0x64 */
	CSS_SCALER_PATH1_SCALER_MONITOR scl_path1_scl_monitor;		/* 0x68 */
	CSS_SCALER_PATH01_DRAM_ADDR_SET scl_path01_dram_addr_set;	/* 0x6C */
	CSS_SCALER_PATH01_DRAM_ADDR_0_Y scl_path01_dram_addr_0_y;	/* 0x70 */
	CSS_SCALER_PATH01_DRAM_ADDR_0_U	scl_path01_dram_addr_0_u;	/* 0x74 */
	CSS_SCALER_PATH01_DRAM_ADDR_0_V	scl_path01_dram_addr_0_v;	/* 0x78 */
	CSS_SCALER_PATH01_DRAM_ADDR_1_Y	scl_path01_dram_addr_1_y;	/* 0x7C */
	CSS_SCALER_PATH01_DRAM_ADDR_1_U	scl_path01_dram_addr_1_u;	/* 0x80 */
	CSS_SCALER_PATH01_DRAM_ADDR_1_V	scl_path01_dram_addr_1_v;	/* 0x84 */
	CSS_SCALER_PATH01_DRAM_ADDR_2_Y	scl_path01_dram_addr_2_y;	/* 0x88 */
	CSS_SCALER_PATH01_DRAM_ADDR_2_U	scl_path01_dram_addr_2_u;	/* 0x8C */
	CSS_SCALER_PATH01_DRAM_ADDR_2_V	scl_path01_dram_addr_2_v;	/* 0x90 */
	CSS_SCALER_PATH01_DRAM_ADDR_3_Y	scl_path01_dram_addr_3_y;	/* 0x94 */
	CSS_SCALER_PATH01_DRAM_ADDR_3_U	scl_path01_dram_addr_3_u;	/* 0x98 */
	CSS_SCALER_PATH01_DRAM_ADDR_3_V	scl_path01_dram_addr_3_v;	/* 0x9C */
	CSS_SCALER_PATH01_DRAM_ADDR_4_Y	scl_path01_dram_addr_4_y;	/* 0xA0 */
	CSS_SCALER_PATH01_DRAM_ADDR_4_U	scl_path01_dram_addr_4_u;	/* 0xA4 */
	CSS_SCALER_PATH01_DRAM_ADDR_4_V	scl_path01_dram_addr_4_v;	/* 0xA8 */
	CSS_SCALER_PATH01_DRAM_ADDR_5_Y	scl_path01_dram_addr_5_y;	/* 0xAC */
	CSS_SCALER_PATH01_DRAM_ADDR_5_U	scl_path01_dram_addr_5_u;	/* 0xB0 */
	CSS_SCALER_PATH01_DRAM_ADDR_5_V	scl_path01_dram_addr_5_v;	/* 0xB4 */
	CSS_SCALER_PATH01_DRAM_ADDR_6_Y	scl_path01_dram_addr_6_y;	/* 0xB8 */
	CSS_SCALER_PATH01_DRAM_ADDR_6_U	scl_path01_dram_addr_6_u;	/* 0xBC */
	CSS_SCALER_PATH01_DRAM_ADDR_6_V	scl_path01_dram_addr_6_v;	/* 0xC0 */
	CSS_SCALER_PATH01_DRAM_ADDR_7_Y	scl_path01_dram_addr_7_y;	/* 0xC4 */
	CSS_SCALER_PATH01_DRAM_ADDR_7_U	scl_path01_dram_addr_7_u;	/* 0xC8 */
	CSS_SCALER_PATH01_DRAM_ADDR_7_V	scl_path01_dram_addr_7_v;	/* 0xCC */
	CSS_SCALER_FD_VSYNC_DLY			scl_fd_vsync_dly;			/* 0xD0 */
	CSS_FD_SCALER_SIZE				fd_crop_offset;				/* 0xD4 */
	CSS_FD_SCALER_SIZE				fd_crop_valid;				/* 0xD8 */
	CSS_FD_SCALER_SIZE				fd_dst_size;				/* 0xDC */
	CSS_FD_SCALER_SIZE				fd_factor;					/* 0xE0 */
	volatile unsigned int			fd_dram_addr;				/* 0xE4 */
	CSS_SCALER_RAW01_ST_SET			scl_raw01_st_set;			/* 0xE8 */
	CSS_SCALER_RAW0_ST_SIZE			scl_raw0_st_size;			/* 0xEC */
	CSS_SCALER_RAW0_ST_DRAM_ADDR	scl_raw0_st_dram_addr;		/* 0xF0 */
	CSS_SCALER_JPEGST_DRAM_ADDR		scl_jpegst_dram_addr;		/* 0xF4 */
	CSS_SCALER_JPEGST_TOTAL_SIZE	scl_jpegst_total_size;		/* 0xF8 */
	CSS_SCALER_RAW1_ST_SIZE			scl_raw1_st_size;			/* 0xFC */
	CSS_SCALER_RAW1_ST_DRAM_ADDR	scl_raw1_st_dram_addr;		/* 0x100 */
	CSS_SCALER_RAW0_LD_SET			scl_raw0_ld_set;			/* 0x104 */
	CSS_SCALER_RAW0_LD_SIZE			scl_raw0_ld_size;			/* 0x108 */
	CSS_SCALER_RAW0_LD_DRAM_ADDR_Y	scl_raw0_ld_dram_addr_y;	/* 0x10C */
	CSS_SCALER_RAW0_LD_DRAM_ADDR_U	scl_raw0_ld_dram_addr_u;	/* 0x110 */
	CSS_SCALER_RAW0_LD_DRAM_ADDR_V	scl_raw0_ld_dram_addr_v;	/* 0x114 */
	CSS_SCALER_RAW1_LD_SET			scl_raw1_ld_set;			/* 0x118 */
	CSS_SCALER_RAW1_LD_SIZE			scl_raw1_ld_size;			/* 0x11C */
	CSS_SCALER_RAW1_LD_DRAM_ADDR_Y	scl_raw1_ld_dram_addr_y;	/* 0x120 */
	CSS_SCALER_RAW1_LD_DRAM_ADDR_U	scl_raw1_ld_dram_addr_u;	/* 0x124 */
	CSS_SCALER_RAW1_LD_DRAM_ADDR_V	scl_raw1_ld_dram_addr_v;	/* 0x128 */
	CSS_SCALER_CRC_CLEAR_SET		scl_crc_clear_set;			/* 0x12C */
	CSS_SCALER_PATH0_Y_H_32			scl_path0_y_high_crc32bit;	/* 0x130 */
	CSS_SCALER_PATH0_Y_L_32			scl_path0_y_low_crc32bit;	/* 0x134 */
	CSS_SCALER_PATH0_U_H_32			scl_path0_u_high_crc32bit;	/* 0x138 */
	CSS_SCALER_PATH0_U_L_32			scl_path0_u_low_crc32bit;	/* 0x13C */
	CSS_SCALER_PATH0_V_H_32			scl_path0_v_high_crc32bit;	/* 0x140 */
	CSS_SCALER_PATH0_V_L_32			scl_path0_v_low_crc32bit;	/* 0x144 */
	CSS_SCALER_PATH1_Y_H_32			scl_path1_y_high_crc32bit;	/* 0x148 */
	CSS_SCALER_PATH1_Y_L_32			scl_path1_y_low_crc32bit;	/* 0x14C */
	CSS_SCALER_PATH1_U_H_32			scl_path1_u_high_crc32bit;	/* 0x150 */
	CSS_SCALER_PATH1_U_L_32			scl_path1_u_low_crc32bit;	/* 0x154 */
	CSS_SCALER_PATH1_V_H_32			scl_path1_v_high_crc32bit;	/* 0x158 */
	CSS_SCALER_PATH1_V_L_32			scl_path1_v_low_crc32bit;	/* 0x15C */
	CSS_SCALER_FD_Y_H_32			scl_fd_y_high_crc32bit;		/* 0x160 */
	CSS_SCALER_FD_Y_L_32			scl_fd_y_low_crc32bit;		/* 0x164 */
	CSS_SCALER_AXI_CACHE_CTRL		scl_axi_cache_ctrl;			/* 0x178 */
} CSS_SCALER_REG_DATA;

typedef enum {
	CSS_INT_PATH0_BUSI_INT			= 0,
	CSS_INT_PATH0_JPEGI_INT			= 1,
	CSS_INT_PATH1_BUSI_INT			= 2,
	CSS_INT_PATH1_JPGI_INT			= 3,
	CSS_INT_PATH_FD_INT				= 4,
	CSS_INT_PATH_RAW0_STORE_INT		= 5,
	CSS_INT_PATH_RAW1_STORE_INT		= 6,
	CSS_INT_PATH_RAW0_LOAD_INT		= 7,
	CSS_INT_PATH_RAW1_LOAD_INT		= 8,
	CSS_INT_PATH_3D_0_INT			= 9,
	CSS_INT_PATH_3D_1_INT			= 10,
	CSS_INT_JPEG_STREAM_CONTROL_INT = 11,
	CSS_INT_PATH0_VSYNC_INT			= 12,
	CSS_INT_PATH1_VSYNC_INT			= 13,
	CSS_INT_FD_VSYNC_INT			= 14,
	CSS_INT_ERROR_INT				= 15
} CSS_SCALER_INT;

typedef enum {
	/*path0 reset*/
	PATH_0_RESET					= (0x01 << 0),
	/*path1 reset*/
	PATH_1_RESET					= (0x01 << 1),
	/*Face-detection path reset*/
	PATH_FD_RESET					= (0x01 << 2),
	/*Raw0 store, JPEG Stream path reset*/
	PATH_RAW0_ST_RESET				= (0x01 << 3),
	/*Raw1 store path reset*/
	PATH_RAW1_ST_RESET				= (0x01 << 4),
	/*Raw0 load path reset*/
	PATH_RAW0_LD_RESET				= (0x01 << 5),
	/*Raw1 load path reset*/
	PATH_RAW1_LD_RESET				= (0x01 << 6),
	/*3D creator path reset*/
	PATH_3D_RESET					= (0x01 << 7),
	/*Raw0 store control module reset*/
	RAW0_ST_CTRL_RESET				= (0x01 << 8),
	/*Raw1 store control module reset*/
	RAW1_ST_CTRL_RESET				= (0x01 << 9),
	/*JPEG Stream control module reset*/
	JPEG_STRM_CTRL_RESET			= (0x01 << 10),
	/*JPEG Encoder path module reset*/
	JPEGENC_PATH_CTRL_RESET			= (0x01 << 11),
	/*All path reset*/
	PATH_RESET_ALL					= 0xFFF,
} CSS_SCALER_RESET;

typedef enum {
	PATH_0_CLK						= (0x01 << 0),
	PATH_1_CLK						= (0x01 << 1),
	PATH_FD_CLK						= (0x01 << 2),
	PATH_RAW0_ST_CLK				= (0x01 << 3),
	PATH_RAW1_ST_CLK				= (0x01 << 4),
	PATH_RAW0_LD_CLK				= (0x01 << 5),
	PATH_RAW1_LD_CLK				= (0x01 << 6),
	PATH_3D_CLK						= (0x01 << 7),
	RAW0_ST_CTRL_CLK				= (0x01 << 8),
	RAW1_ST_CTRL_CLK				= (0x01 << 9),
	JPEG_ST_CTRL_CLK				= (0x01 << 10),
	JPEG_EN_CTRL_CLK				= (0x01 << 11),
	PATH_ALL_CLK					= 0xFFF,
} CSS_SCALER_PATH_CLK;

enum {
	CLOCK_OFF,
	CLOCK_ON
};

typedef enum {
	PATH_0_BUSI_CAPTURE_DONE	= (1<<0),
	PATH_0_BUSI_PREVIEW_DONE	= (1<<1),
	PATH_0_JPGI_DONE			= (1<<2),
	PATH_1_BUSI_CAPTURE_DONE	= (1<<3),
	PATH_1_BUSI_PREVIEW_DONE	= (1<<4),
	PATH_1_JPGI_DONE			= (1<<5),
	PATH_FD_DONE				= (1<<6),
	PATH_RAW0_ST_BUSI_DONE		= (1<<7),
	PATH_RAW1_ST_BUSI_DONE		= (1<<8),
	PATH_RAW0_LD_DONE			= (1<<9),
	PATH_RAW1_LD_DONE			= (1<<10),
	PATH_3D_0_DONE				= (1<<11),
	PATH_3D_1_DONE				= (1<<12),
	RAW0_ST_CTRL_DONE			= (1<<13),
	JPEG_ST_CTRL_DONE			= (1<<14)
} CSS_SCALER_DONE_SATATUS;

typedef enum {
	/* H/W ERROR */
	ERR_PATH0_BUF_OVERFLOW			= (1<<0),
	ERR_PATH0_ROOP					= (1<<1),
	ERR_PATH1_BUF_OVERFLOW			= (1<<2),
	ERR_PATH1_ROOP					= (1<<3),
	ERR_FD_BUF_OVERFLOW				= (1<<4),
	ERR_PATH0_STORE_BUF_OVERFLOW	= (1<<5),
	ERR_PATH1_STORE_BUF_OVERFLOW	= (1<<6),
	ERR_JPEG_STREAM_BUF_OVERFLOW	= (1<<7),
	ERR_JPEG_INTERFACE_ERR			= (1<<8),
	/* S/W ERROR */
	ERR_ZSL_PATH0_ROOP				= (1<<9),
	ERR_ZSL_PATH1_ROOP				= (1<<10),
	ERR_ZSL_PATH0_BOF				= (1<<11),
	ERR_ZSL_PATH1_BOF				= (1<<12),
	ERR_ZSL_PATH0_ROOTOUT			= (1<<13),
	ERR_ZSL_PATH1_ROOTOUT			= (1<<14)
} CSS_SCALER_ERR_SATATUS;

typedef enum {
	PATH_0_PARAM_SET				= (0x1<<0),
	PATH_1_PARAM_SET				= (0x1<<1),
	PATH_FD_PARAM_SET				= (0x1<<2),
	PATH_RAW0_ST_PARAM_SET			= (0x1<<3),
	PATH_RAW1_ST_PARAM_SET			= (0x1<<4),
	PATH_RAW0_LD_PARAM_SET			= (0x1<<5),
	PATH_RAW1_LD_PARAM_SET			= (0x1<<6),
	JPEG_ST_CTRL_PARAM_SET			= (0x1<<7),
	PATH_0_MEM_SET_SHUT_DOWN		= (0x1<<8),
	PATH_0_MEM_SET_DEEP_SLEEP		= (0x1<<9),
	PATH_1_MEM_SET_SHUT_DOWN		= (0x1<<10),
	PATH_1_MEM_SET_DEEP_SLEEP		= (0x1<<11),
	PATH_FD_MEM_SET_SHUT_DOWN		= (0x1<<12),
	PATH_FD_MEM_SET_DEEP_SLEEP		= (0x1<<13),
	PATH_RAW0_ST_MEM_SET_SHUT_DOWN	= (0x1<<14),
	PATH_RAW0_ST_MEM_SET_DEEP_SLEEP = (0x1<<15),
	PATH_RAW1_ST_MEM_SET_SHUT_DOWN	= (0x1<<16),
	PATH_RAW1_ST_MEM_SET_DEEP_SLEEP = (0x1<<17)
} CSS_SCALER_PATH_PARAM_SET;

#define INT_PATH0_BUSI			(1 << CSS_INT_PATH0_BUSI_INT)
#define INT_PATH0_JPEGI 		(1 << CSS_INT_PATH0_JPEGI_INT)
#define INT_PATH1_BUSI			(1 << CSS_INT_PATH1_BUSI_INT)
#define INT_PATH1_JPEGI 		(1 << CSS_INT_PATH1_JPGI_INT)
#define INT_PATH_FD 			(1 << CSS_INT_PATH_FD_INT)
#define INT_PATH_RAW0_STORE 	(1 << CSS_INT_PATH_RAW0_STORE_INT)
#define INT_PATH_RAW1_STORE 	(1 << CSS_INT_PATH_RAW1_STORE_INT)
#define INT_PATH_RAW0_LOAD		(1 << CSS_INT_PATH_RAW0_LOAD_INT)
#define INT_PATH_RAW1_LOAD		(1 << CSS_INT_PATH_RAW1_LOAD_INT)
#define INT_PATH_3D_0			(1 << CSS_INT_PATH_3D_0_INT)
#define INT_PATH_3D_1			(1 << CSS_INT_PATH_3D_1_INT)
#define INT_JPEG_STREAM_CONTROL	(1 << CSS_INT_JPEG_STREAM_CONTROL_INT)
#define INT_PATH0_VSYNC 		(1 << CSS_INT_PATH0_VSYNC_INT)
#define INT_PATH1_VSYNC 		(1 << CSS_INT_PATH1_VSYNC_INT)
#define INT_FD_VSYNC			(1 << CSS_INT_FD_VSYNC_INT)
#define INT_ERROR				(1 << CSS_INT_ERROR_INT)
#define INT_ALL 				0xFFFF


#define BAYER_STORE_OFFSET	2
#define BAYER_LOAD_OFFSET	4

void scaler_reg_dump(void);
unsigned int scaler_register_isr(struct scaler_hardware *scl_hw,
			irq_handler_t irq_handler);
unsigned int scaler_get_int_status(void);
unsigned int scaler_get_int_mask(void);
unsigned int scaler_get_done_status(void);
unsigned int scaler_get_param_status(void);
unsigned int scaler_get_href_cnt(css_scaler_select scaler);
unsigned int scaler_get_err_status(void);
int  scaler_get_ready_state(void);
void scaler_int_clear(unsigned int int_flag);
void scaler_int_enable(unsigned int int_flag);
void scaler_int_disable(unsigned int int_flag);
unsigned int scaler_get_frame_num(css_scaler_select scaler);
unsigned int scaler_get_frame_width(css_scaler_select scaler);
unsigned int scaler_get_frame_height(css_scaler_select scaler);
unsigned int scaler_get_frame_size(css_scaler_select scaler);
unsigned int scaler_reset_frame_num(css_scaler_select scaler);
int scaler_path_reset(unsigned int path);
int scaler_path_clk_control(unsigned int path, unsigned int onoff);
int scaler_init_device(css_scaler_select scaler);
int scaler_deinit_device(css_scaler_select scaler);
void scaler_device_close(void);
int fd_scaler_configure(css_scaler_select scaler,
			struct face_detect_config *config_data,
			struct css_scaler_data *fd_scaler_config);
int fd_scaler_set_source(css_scaler_select scaler,
			struct css_scaler_data *fd_scaler_config, css_fd_source fd_source);
int fd_set_buffer(css_scaler_select scaler, unsigned int buf_addr);
int scaler_configure(css_scaler_select scaler,
			struct css_scaler_config *scl_config,
			struct css_scaler_data *scl_data);
int scaler_default_configure(css_scaler_select scaler);
int scaler_action_configure(css_scaler_select scaler,
			struct css_scaler_data *scl_data, css_scaler_action_type action);
int scaler_set_capture_frame(css_scaler_select scaler);
int scaler_set_crop_n_masksize(css_scaler_select scaler,
				struct css_crop_data crop_data,
				struct css_scaler_data *scl_data);
int scaler_set_capture_frame_with_postview(css_scaler_select capture_scl,
			css_scaler_select postview_scl);
int	scaler_set_buffers(css_scaler_select scaler,
			 struct css_scaler_config *scl_config, unsigned int buf_addr);
int scaler_effects_configure(css_scaler_select scaler,
			struct css_scaler_data *scl_data);
int scaler_colorformat_configure(css_scaler_select scaler,
			struct css_scaler_data *scl_data, css_pixel_format pix_fmt);
int scaler_set_sensorstrobe(struct css_strobe_set *StrobeSet);
int scaler_get_sensorstrobe(struct css_strobe_set *StrobeSet);
int scaler_set_vsync_delay(css_scaler_select scaler,
			struct css_scaler_data *scl_data, unsigned short delay);
int scaler_bayer_store_enable(css_zsl_path s_path,
			struct css_zsl_config *zsl_config);
int scaler_bayer_store_disable(css_zsl_path s_path,
			struct css_zsl_config *zsl_config);
int scaler_bayer_store_configure(css_zsl_path s_path,
			struct css_zsl_config *zsl_config);
int scaler_bayer_store_capture(css_zsl_path s_path,
			struct css_zsl_config *zsl_config);
int scaler_bayer_load_configure (css_zsl_path r_path,
			struct css_zsl_config *zsl_config);
int scaler_bayer_load_frame_for_1ch(int enable, css_zsl_path r_path,
			css_scaler_select scaler);
int scaler_bayer_load_frame_for_2ch(int enable, css_zsl_path r_path,
			css_scaler_select capture_scl,
			 css_scaler_select postview_scl);
int scaler_bayer_load_frame_for_stereo(int enable);
int scaler_set_capture_stereo_frame(void);
int scaler_bayer_store_reset(css_zsl_path s_path);

#endif /* __ODIN_FRAME_GRABBER_H__ */

