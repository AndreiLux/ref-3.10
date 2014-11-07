/*
 * linux/drivers/video/odin/dss/dispc-regs.h
 *
 * Copyright (C) 2012 LG Electronics, Inc.
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
#ifndef __ODIN_DIPC_REG_H__
#define __ODIN_DIPC_REG_H__

/* RGB LCD Interface*/

#define	ODIN_DIP_INTSOURCE			0x00
#define ODIN_DIP_INTMASK			0x04
#define ODIN_DIP_INTCLEAR			0x08
#define ODIN_DIP_CTRL				0x0C
#define ODIN_DIP_FIFO_STATUS			0x10
#define ODIN_DIP_FLUSH_RESYNC_SHADOW		0x14
#define ODIN_DIP_LCD_FIFO0_WMARK		0x18
#define ODIN_DIP_LCD_FIFO1_WMARK		0x1C
#define ODIN_DIP_PCOMP_WMARK			0x20
#define ODIN_DIP_LCD_DSP_CONFIGURATION		0x24
#define ODIN_DIP_CCR_LCD_CLAMP0_MAX		0x28
#define ODIN_DIP_CCR_LCD_CLAMP0_MIN		0x2C
#define ODIN_DIP_CCR_LCD_CLAMP1_MAX		0x30
#define ODIN_DIP_CCR_LCD_CLAMP1_MIN		0x34
#define ODIN_DIP_CSC0_MATRIXA_LCD		0x38
#define ODIN_DIP_CSC0_MATRIXB_LCD		0x3C
#define ODIN_DIP_CSC0_MATRIXC_LCD		0x40
#define ODIN_DIP_CSC0_MATRIXD_LCD		0x44
#define ODIN_DIP_CSC0_MATRIXE_LCD		0x48
#define ODIN_DIP_CSC0_MATRIXF_LCD		0x4C
#define ODIN_DIP_CSC0_MATRIXG_LCD		0x50
#define ODIN_DIP_CSC0_MATRIXH_LCD		0x54
#define ODIN_DIP_CSC1_MATRIXA_LCD		0x58
#define ODIN_DIP_CSC1_MATRIXB_LCD		0x5C
#define ODIN_DIP_CSC1_MATRIXC_LCD		0x60
#define ODIN_DIP_CSC1_MATRIXD_LCD		0x64
#define ODIN_DIP_CSC1_MATRIXE_LCD		0x68
#define ODIN_DIP_CSC1_MATRIXF_LCD		0x6C
#define ODIN_DIP_CSC1_MATRIXG_LCD		0x70
#define ODIN_DIP_CSC1_MATRIXH_LCD		0x74
#define ODIN_DIP_CCR_LCD_GAMMA_ADDR		0x78
#define ODIN_DIP_CCR_LCD_GAMMA_RDATA		0x7C
#define ODIN_DIP_TEST_FIFO_RDATA		0x80
#define ODIN_DIP_LCD_BYTE_COUNT			0x84
#define ODIN_DIP_DITHER				0x88
#define ODIN_DIP_BLANK_DATA			0x8C
#define ODIN_DIP_CFG0				0x90
#define ODIN_DIP_CFG1				0x94
#define ODIN_DIP_CFG2				0x98
#define ODIN_DIP_CFG3				0x9C
#define ODIN_DIP_CFG4				0xA0
#define ODIN_DIP_CFG5				0xA4
#define ODIN_DIP_CFG6				0xA8
#define ODIN_DIP_CFG7				0xAC
#define ODIN_DIP_CFG8				0xB0
#define ODIN_DIP_CFG9				0xB4
#define ODIN_DIP_CFG10				0xB8
#define ODIN_DIP_CFG11				0xBC
#define ODIN_DIP_CFG12				0xC0
#define ODIN_DIP_CFG13				0xC4
#define ODIN_DIP_CNT_STATE			0xC8
#define ODIN_DIP_ACT_CNT_STATE			0xCC
#define ODIN_DIP_CONTROL_STATE			0xD0


/* IRQ bits definition*/
#define DIP_PEXPD_FIFO_ERR      0 /* RGB/TVout/MIPI*/
#define DIP_FIFO0_ACCESS_ERR    1 /* RGB/TVout/MIPI*/
#define DIP_FIFO0_BE            2 /* RGB/TVout/MIPI*/
#define DIP_FIFO0_AE            3 /* RGB/TVout/MIPI*/
#define DIP_PCOMPC_FIFO_ERR     4 /* RGB/TVout/MIPI*/
#define DIP_FIFO1_ACCESS_ERR    5 /* RGB/TVout*/
#define DIP_FIFO1_BE            6 /* RGB/TVout*/
#define DIP_FIFO1_AE            7 /* RGB/TVout*/
#define DIP_COMBINATION_ERR     8 /* RGB/TVout*/
#define DIP_SOVF                9 /* RGB/TVout*/
#define DIP_EOVF               10 /* RGB/TVout*/
#define DIP_SOVL               11 /* RGB/TVout*/
#define DIP_EOVL               12 /* RGB/TVout*/
#define DIP_SOF                13 /* RGB/TVout*/
#define DIP_EOF                14 /* RGB/TVout*/
#define DIP_SOL                15 /* RGB/TVout*/
#define DIP_EOL                16 /* RGB/TVout*/


union dip_interrupt {
	struct {
		u32 pexpd_fifo_error 	:	1;
		u32 fifo0_access_error	:	1;
		u32 fifo0_becomes_empty :	1;
		u32 fifo0_almost_empty	:	1;
		u32 pcompc_fifo_error	:	1;
		u32 fifo1_access_error	:	1;
		u32 fifo1_becomes_empty :	1;
		u32 fifo1_almost_empty	:	1;
		u32 combinaion_error	:	1;
		u32 start_of_valid_frame :	1;
		u32 end_of_valid_frame	:	1;
		u32 start_of_valid_line	:	1;
		u32 end_of_valid_line	:	1;
		u32 start_of_frame	:	1;
		u32 end_of_frame	:	1;
		u32 start_of_line	:	1;
		u32 end_of_line		:	1;
		u32 reserved		:	15;
	} af;
	u32 a32;
};

union dip_ctrl {
	struct {
		u32 enable		:	1;
		u32 test_port_enable	:	1;
		u32 fifo1_pushsize	:	2;
		u32 timing_width	:	3;
		u32 fifo_data_swap	:	1;
		u32 swap		:	1;
		u32 imgfmtout		:	3;
		u32 imgfmtin		:	2;
		u32 invert_blank	:	1;
		u32 invert_hsync	:	1;
		u32 invert_vsync	:	1;
		u32 reserved0		:	1;
		u32 rgb_rotate_blank_data :	1;
		u32 rgb_play_blank_data	:	1;
		u32 rgb_blank_select	:	1;
		u32 sof_eof_mode	:	1;
		u32 lcd_if_type		:	1;
		u32 lcd_invert_pclk	:	1;
		u32 lcd_drv_pclk	:	1;
		u32 reserved		:	7;
	} af;
	u32 a32;
};

union dip_fifo_status {
	struct {
		u32 pexpd_in_underflow	:	1;
		u32 pexpd_in_overflow	:	1;
		u32 pcompc_in_underflow	:	1;
		u32 pcompc_in_overflow	:	1;
		u32 fifo0_almost_empty	:	1;
		u32 fifo0_almost_full	:	1;
		u32 fifo0_full		:	1;
		u32 fifo1_almost_empty	:	1;
		u32 fifo1_almost_full	:	1;
		u32 fifo1_full		:	1;
		u32 reserved 		:	20;
	} af;
	u32 a32;
};

union dip_flush_resync_shadow {
	struct {
		u32 shadow_update	:	1;
		u32 fifo0_flush		:	1;
		u32 fifo1_flush		:	1;
		u32 resync_on		:	1;
		u32 reserved 		:	28;
	} af;
	u32 a32;
};

union dip_lcd_fifo0_wmark {
	struct {
		u32 fifo0_almost_empty_wmark	: 13;
		u32 reserved0			:  3;
		u32 fifo0_almost_full_wmark	: 13;
		u32 reserved1			:  3;
	} af;
	u32 a32;
};

union dip_lcd_fifo1_wmark {
	struct {
		u32 fifo1_almost_empty_wmark	: 10;
		u32 reserved0			:  6;
		u32 fifo1_almost_full_wmark	: 10;
		u32 reserved1			:  6;
	} af;
	u32 a32;
};

union dip_pcomp_wmark {
	struct {
		u32 pcompc_af_watermark	:	9;
		u32 reserved 		:	23;
	} af;
	u32 a32;
};

union dip_lcd_dsp_configuration {
	struct {
		u32 pexpd_latency	:	4;
		u32 pexpd_in_fmt	:	1;
		u32 pexpd_swap_color	:	1;
		u32 pcompc_in_fmt	:	2;
		u32 pcompc_swap		:	1;
		u32 gamma_enb		:	1;
		u32 reserved		:	22;
	} af;
	u32 a32;
};

union dip_ccr_lcd_clamp {
	struct {
		u32 b			:	8;
		u32 g			:	8;
		u32 r 			:	8;
		u32 reserved 		:	8;
	} af;
	u32 a32;
};

union dip_csc_matrix1 {
	struct {
		u32 a11			:	13;
		u32 reserved0		:	 3;
		u32 a12			:	13;
		u32 reserved1		:	 3;
	} af;
	u32 a32;
};

union dip_csc_matrix2 {
	struct {
		u32 b_offset		:	9;
		u32 g_offset		:	9;
		u32 r_offset		:	9;
	} af;
	u32 a32;
};


union dip_ccr_lcd_gamma_addr {
	struct {
		u32 address		:	8;
		u32 reserved 		: 	24;
	} af;
	u32 a32;
};

union dip_ccr_lcd_gamma_rdata {
	struct {
		u32 b			:	8;
		u32 g			:	8;
		u32 r 			:	8;
		u32 reserved 		:	8;
	} af;
	u32 a32;
};

union dip_lcd_byte_count {
	struct {
		u32 fifo0_rd_byte_count	:	14;
		u32 reserved		:	18;
	} af;
	u32 a32;
};

union rgb_dither {
	struct {
		u32 dith_height		:	10;
		u32 dith_width		:	10;
		u32 dith_en		:	1;
		u32 reserved 		:	10;
		u32 dith_clear		:	1;
	} af;
	u32 a32;
};

union rgb_cfg0 {
	struct {
		u32 hsync_total		:	12;
		u32 reserved0 		:	4;
		u32 hsync_width		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union rgb_cfg1 {
	struct {
		u32 vsync_total		:	12;
		u32 reserved0 		:	4;
		u32 vsync_width		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};


union rgb_cfg2 {
	struct {
		u32 hvalid_start	:	12;
		u32 reserved0 		:	4;
		u32 hvalid_stop		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};


union rgb_cfg3 {
	struct {
		u32 vvalid_start	:	12;
		u32 reserved0 		:	4;
		u32 vvalid_stop		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union rgb_cfg4 {
	struct {
		u32 hblank_start	:	12;
		u32 reserved0 		:	4;
		u32 hblank_stop		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union rgb_cfg5 {
	struct {
		u32 vblank_start	:	12;
		u32 reserved0 		:	4;
		u32 vblank_stop		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};


union dip_cnt_state {
	struct {
		u32 rgb_hcnt_state	:	12;
		u32 reserved0		:	4;
		u32 rgb_vcnt_state	:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};


union dip_act_cnt_state {
	struct {
		u32 rgb_act_hcnt_state	:	12;
		u32 reserved0		:	4;
		u32 rgb_act_vcnt_state	:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union dip_control_state {
	struct {
		u32 tc_state		:	2;
		u32 fifo1_underflow	:	1;
		u32 tc_data_valid	:	1;
		u32 reserved 		:	28;
	} af;
	u32 a32;
};

union tv_ctrl {
	struct {
		u32 enable		:	1;
		u32 test_port_enable	:	1;
		u32 fifo1_pushsize	:	2;
		u32 timing_width	:	3;
		u32 fifo_data_swap	:	1;
		u32 swap		:	1;
		u32 imgfmtout	:	3;
		u32 imgfmtin	:	2;
		u32 invert_blank	:	1;
		u32 invert_hsync	:	1;
		u32 invert_vsync	:	1;
		u32 invert_fsync	:	1;
		u32 rotate_blank_data	:	1;
		u32 play_blank_data	:	1;
		u32 blank_select	:	1;
		u32 sof_eof_mode	:	1;
		u32 reserved		:	10;
	} af;
	u32 a32;
};


union tv_cfg0 {
	struct {
		u32 hsync_total		:	12;
		u32 reserved0 		:	4;
		u32 vsync_total		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union tv_cfg1 {
	struct {
		u32 hsync_f		:	12;
		u32 reserved0 		:	4;
		u32 hsync_r		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};


union tv_cfg2 {
	struct {
		u32 vsync_vr0		:	12;
		u32 reserved0 		:	4;
		u32 vsync_hr0		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};


union tv_cfg3 {
	struct {
		u32 vsync_vf0		:	12;
		u32 reserved0 		:	4;
		u32 vsync_hf0		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union tv_cfg4 {
	struct {
		u32 vsync_vr1		:	12;
		u32 reserved0 		:	4;
		u32 vsync_hr1		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union tv_cfg5 {
	struct {
		u32 vsync_vf1		:	12;
		u32 reserved0 		:	4;
		u32 vsync_hf1		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union tv_cfg6 {
	struct {
		u32 fsync_vr		:	12;
		u32 reserved0 		:	4;
		u32 fsync_hr		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union tv_cfg7 {
	struct {
		u32 fsync_vf		:	12;
		u32 reserved0 		:	4;
		u32 fsync_hf		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union tv_cfg8 {
	struct {
		u32 hvalid_start	:	12;
		u32 reserved0 		:	4;
		u32 hvalid_stop		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union tv_cfg9 {
	struct {
		u32 vvalid_start_odd	:	12;
		u32 reserved0 		:	4;
		u32 vvalid_stop_odd	:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union tv_cfg10 {
	struct {
		u32 vvalid_start_even	:	12;
		u32 reserved0 		:	4;
		u32 vvalid_stop_even	:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union tv_cfg11 {
	struct {
		u32 hblank_start	:	12;
		u32 reserved0 		:	4;
		u32 hblank_stop		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union tv_cfg12 {
	struct {
		u32 vblank_start_odd	:	12;
		u32 reserved0 		:	4;
		u32 vblank_stop_odd		:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

union tv_cfg13 {
	struct {
		u32 vblank_start_even	:	12;
		u32 reserved0 		:	4;
		u32 vblank_stop_even	:	12;
		u32 reserved1		:	4;
	} af;
	u32 a32;
};

struct dip_register {
	union dip_interrupt 		int_source;
	union dip_interrupt 		int_mask;
	union dip_interrupt 		int_clear;
	union dip_ctrl			ctrl;
	union dip_fifo_status		fifo_status;
	union dip_flush_resync_shadow 	flush_resync_shadow;
	union dip_lcd_fifo0_wmark 	fifo0_watermark;
	union dip_lcd_fifo1_wmark 	fifo1_watermark;
	union dip_pcomp_wmark		pcomp_watermark;
	union dip_lcd_dsp_configuration dsp_configuration;
	union dip_ccr_lcd_clamp		clamp0_max;
	union dip_ccr_lcd_clamp		clamp0_min;
	union dip_ccr_lcd_clamp		clamp1_max;
	union dip_ccr_lcd_clamp		clamp1_min;

	union dip_csc_matrix1 		csc0_matrixa_lcd;
	union dip_csc_matrix1		csc0_matrixb_lcd;
	union dip_csc_matrix1 		csc0_matrixc_lcd;
	union dip_csc_matrix1		csc0_matrixd_lcd;
	union dip_csc_matrix1 		csc0_matrixe_lcd;
	union dip_csc_matrix1		csc0_matrixf_lcd;
	union dip_csc_matrix2		csc0_matrixg_lcd;
	union dip_csc_matrix2		csc0_matrixh_lcd;
	union dip_csc_matrix1 		csc1_matrixa_lcd;
	union dip_csc_matrix1		csc1_matrixb_lcd;
	union dip_csc_matrix1 		csc1_matrixc_lcd;
	union dip_csc_matrix1		csc1_matrixd_lcd;
	union dip_csc_matrix1 		csc1_matrixe_lcd;
	union dip_csc_matrix1		csc1_matrixf_lcd;
	union dip_csc_matrix2		csc1_matrixg_lcd;
	union dip_csc_matrix2		csc1_matrixh_lcd;

	union dip_ccr_lcd_gamma_addr	gamma_addr;
	union dip_ccr_lcd_gamma_rdata	gamma_rdata;
	u32 				test_fifo_rdata;
	union dip_lcd_byte_count	byte_count;
	u32				reserved1;
	u32				blank_data;

	union cfg {
		struct {
			union rgb_cfg0			cfg0;
			union rgb_cfg1			cfg1;
			union rgb_cfg2			cfg2;
			union rgb_cfg3			cfg3;
			union rgb_cfg4			cfg4;
			union rgb_cfg5			cfg5;

			u32 				reserved1[8];
		} lcd;
		struct {
			union tv_cfg0			cfg0;
			union tv_cfg1			cfg1;
			union tv_cfg2			cfg2;
			union tv_cfg3			cfg3;
			union tv_cfg4			cfg4;
			union tv_cfg5			cfg5;
			union tv_cfg6			cfg6;
			union tv_cfg7			cfg7;
			union tv_cfg8			cfg8;
			union tv_cfg9			cfg9;
			union tv_cfg10			cfg10;
			union tv_cfg11			cfg11;
			union tv_cfg12			cfg12;
			union tv_cfg13			cfg13;
		} tv;
	};
	union dip_cnt_state 		cnt_state;
	union dip_act_cnt_state 	act_cnt_state;
	union dip_control_state		control_state;
};

#endif	/* __ODIN_DIPC_REG_H__*/
