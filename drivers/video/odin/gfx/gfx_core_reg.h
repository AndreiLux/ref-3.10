/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See th
 * GNU General Public License for more details.
 */

/*       
  
                                                        
  
                                            
                  
                         
  
 */

#ifndef _GFX_CORE_REG_H_
#define _GFX_CORE_REG_H_

/*-------------------------------------------------------------------
    Control Constants
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
    File Inclusions
--------------------------------------------------------------------*/


#ifdef __cplusplus
extern "C" {
#endif

/*-------------------------------------------------------------------
	0x0000 gfx_version ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x0004 gfx_status0 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	exec_status                     : 1,	/*      0 */
	                                :15,	/*   1:15 reserved*/
	exec_line                       :13;	/*  16:28*/
} GFX_STATUS0;

/*-------------------------------------------------------------------
	0x0008 gfx_status1 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	cmd_que_full                    : 1,	/*      0*/
	                                :15,	/*   1:15 reserved*/
	que_remain                      :10,	/*  16:25*/
	                                : 4,	/*  26:29 reserved*/
	cmd_que_status                  : 2;	/*  30:31*/
} GFX_STATUS1;

/*-------------------------------------------------------------------
	0x000c gfx_status2 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	batch_status                    : 1,	/*      0*/
	                                :15,	/*   1:15 reserved*/
	batch_remain                    :10,	/*  16:25*/
	                                : 4,	/*  26:29 reserved*/
	mflust_st                       : 1,	/*     30*/
	intr_st                         : 1;	/*     31*/
} GFX_STATUS2;

/*-------------------------------------------------------------------
	0x0010 gfx_intr_ctrl ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	intr_en                         : 1,	/*      0*/
	                                : 3,	/*   1: 3 reserved*/
	cmd_queue_ful                   : 1,	/*      4*/
	reset_intr_en                   : 1,	/*      5*/
	                                : 2,	/*   6: 7 reserved*/
	intr_gen_mode                   : 2,	/*   8: 9*/
	                                : 6,	/*  10:15 reserved*/
	cmd_num_intr                    : 8;	/*  16:23 */
} GFX_INTR_CTRL;

/*-------------------------------------------------------------------
	0x0014 gfx_intr_clear ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	gfx_intr_clear                  : 1;	/*      0 */
} GFX_INTR_CLEAR;

/*-------------------------------------------------------------------
	0x0018 gfx_cmd_pri ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	r2_pri2                         : 3,	/*   0: 2 */
	r2_pri1                         : 3,	/*   3: 5 */
	r1_pri2                         : 3,	/*   6: 8 */
	r1_pri1                         : 3,	/*   9:11 */
	r0_pri2                         : 3,	/*  12:14 */
	r0_pri1                         : 3,	/*  15:17 */
	w1_pri2                         : 3,	/*  18:20 */
	w1_pri1                         : 3,	/*  21:23 */
	w0_pri2                         : 3,	/*  24:26 */
	w0_pri1                         : 3;	/*  27:29 */
} GFX_CMD_PRI;

/*-------------------------------------------------------------------
	0x001c gfx_mem_port_id ''
--------------------------------------------------------------------*/
typedef struct {
#if 1	/* raxis.lim -- register conversion error */
	u32							do_not_use_this;
#else
	u32
	gfx_mem_port_id                 : 3,	/*   0: 2 */
	                                : 5,	/*   3: 7 reserved */
	gfx_mem_port_id                 : 3,	/*   8:10 */
	                                : 1,	/*     11 reserved */
	gfx_mem_port_id                 : 3,	/*  12:14 */
	                                : 1,	/*     15 reserved */
	gfx_mem_port_id                 : 3,	/*  16:18 */
	                                : 1,	/*     19 reserved */
	gfx_mem_port_id                 : 3,	/*  20:22 */
	                                : 1,	/*     23 reserved */
	gfx_mem_port_id                 : 3,	/*  24:26 */
	                                : 1,	/*     27 reserved */
	gfx_mem_port_id                 : 3;	/*  28:30 */
#endif
} GFX_MEM_PORT_ID;

/*-------------------------------------------------------------------
	0x0020 gfx_op_mode ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	op_mode                         : 3,	/*   0: 2 */
	                                : 1,	/*      3 reserved */
	burst_mode                      : 1,	/*      4 */
	clut_up_en                      : 1,	/*      5 */
	                                : 2,	/*   6: 7 reserved */
	cflt_mode                       : 2;	/*   8: 9 */
} GFX_OP_MODE;

/*-------------------------------------------------------------------
	0x0024 cflt_coef0 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	cflt_coef_1                     :12,	/*   0:11 */
	                                : 4,	/*  12:15 reserved */
	cflt_coef_0                     :12;	/*  16:27 */
} CFLT_COEF0;

/*-------------------------------------------------------------------
	0x0028 cflt_coef1 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	cflt_coef_3                     :12,	/*   0:11 */
	                                : 4,	/*  12:15 reserved */
	cflt_coef_2                     :12;	/*  16:27 */
} CFLT_COEF1;

/*-------------------------------------------------------------------
	0x002c gfx_cmd_dly ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	gfx_cmd_dly                     :16;	/*   0:15 */
} GFX_CMD_DLY;

/*-------------------------------------------------------------------
	0x0030 gfx_batch_run ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	gfx_batch_run                   : 1;	/*      0 */
} GFX_BATCH_RUN;

/*-------------------------------------------------------------------
	0x0034 gfx_start ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	gfx_start                       : 1;	/*      0 */
} GFX_START;

/*-------------------------------------------------------------------
	0x0038 gfx_pause ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	gfx_pause                       : 2;	/*   0: 1 */
} GFX_PAUSE;

/*-------------------------------------------------------------------
	0x003c gfx_reset ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	gfx_reset                       : 1;	/*      0 */
} GFX_RESET;

/*-------------------------------------------------------------------
	0x0040 debug_stat_00 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	r1_debug_status                 :16,	/*   0:15 */
	r0_debug_status                 :16;	/*  16:31 */
} DEBUG_STAT_00;

/*-------------------------------------------------------------------
	0x0044 debug_stat_01 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	wr_debug_status                 :16,	/*   0:15 */
	r2_debug_status                 :16;	/*  16:31 */
} DEBUG_STAT_01;

/*-------------------------------------------------------------------
	0x0050 r0_base_addr ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x0054 r0_stride ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	r0_stride                       :15;	/*   0:14 */
} R0_STRIDE;

/*-------------------------------------------------------------------
	0x0058 r0_pformat ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	pixel_format                    : 5,	/*   0: 4 */
	                                : 3,	/*   5: 7 reserved */
	endian_mode                     : 1;	/*      8 */
} R0_PFORMAT;

/*-------------------------------------------------------------------
	0x005c r0_galpha ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	r0_alpha0                       : 8,	/*   0: 7 */
	r0_alpha1                       : 8,	/*   8:15 */
	r0_alpha2                       : 8,	/*  16:23 */
	r0_alpha3                       : 8;	/*  24:31 */
} R0_GALPHA;

/*-------------------------------------------------------------------
	0x0060 r0_clut_ctrl ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	clut0_addr                      : 8,	/*   0: 7 */
	clut0_rw                        : 1,	/*      8 */
	clut0_wai                       : 1;	/*      9 */
} R0_CLUT_CTRL;

/*-------------------------------------------------------------------
	0x0064 r0_clut_data ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x0068 r0_ctrl ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	color_key_en                    : 1,	/*      0 */
	bitmask_en                      : 1,	/*      1 */
	coc_en                          : 1,	/*      2 */
	csc_en                          : 1,	/*      3 */
	                                : 4,	/*   4: 7 reserved */
	color_key_mode                  : 1,	/*      8 */
	                                : 3,	/*   9:11 reserved */
	csc_coef_sel                    : 2;	/*  12:13 */
} R0_CTRL;

/*-------------------------------------------------------------------
	0x006c r0_ckey_key0 ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x0070 r0_ckey_key1 ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x0074 r0_ckey_replace_color ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x0078 r0_bitmask ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x007c r0_coc_ctrl ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	r0_coc_ctrl_0_after            : 2,	/*   0: 1 */
	r0_coc_ctrl_1_after            : 2,	/*   2: 3 */
	r0_coc_ctrl_2_after            : 2,	/*   4: 5 */
	r0_coc_ctrl_3_after            : 2,	/*   6: 7 */
	                               : 8,	/*   8:15 reserved */
	r0_coc_ctrl_0_before 		   : 2,	/*  16:17 */
	r0_coc_ctrl_1_before           : 2,	/*  18:19 */
	r0_coc_ctrl_2_before           : 2,	/*  20:21 */
	r0_coc_ctrl_3_before  		   : 2;	/*  22:23 */
} R0_COC_CTRL;

/*-------------------------------------------------------------------
	0x0080 r1_base_addr ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x0084 r1_stride ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	r1_stride                       :15;	/*   0:14 */
} R1_STRIDE;

/*-------------------------------------------------------------------
	0x0088 r1_pformat ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	pixel_format                    : 5,	/*   0: 4 */
	                                : 3,	/*   5: 7 reserved */
	endian_mode                     : 1;	/*      8 */
} R1_PFORMAT;

/*-------------------------------------------------------------------
	0x008c r1_galpha ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	r1_alpha0                       : 8,	/*   0: 7 */
	r1_alpha1                       : 8,	/*   8:15 */
	r1_alpha2                       : 8,	/*  16:23 */
	r1_alpha3                       : 8;	/*  24:31 */
} R1_GALPHA;

/*-------------------------------------------------------------------
	0x0090 r1_clut_ctrl ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	clut1_addr                      : 8,	/*   0: 7 */
	clut1_rw                        : 1,	/*      8 */
	clut1_wai                       : 1;	/*      9 */
} R1_CLUT_CTRL;

/*-------------------------------------------------------------------
	0x0094 r1_clut_data ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x0098 r1_ctrl ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	color_key_en                    : 1,	/*      0 */
	bitmask_en                      : 1,	/*      1 */
	coc_en                          : 1,	/*      2 */
	csc_en                          : 1,	/*      3 */
	                                : 4,	/*   4: 7 reserved */
	color_key_mode                  : 1,	/*      8 */
	                                : 3,	/*   9:11 reserved */
	csc_coef_sel                    : 2;	/*  12:13 */
} R1_CTRL;

/*-------------------------------------------------------------------
	0x009c r1_ckey_key0 ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x00a0 r1_ckey_key1 ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x00a4 r1_ckey_replace_color ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x00a8 r1_bitmask ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x00ac r1_coc_ctrl ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	r1_coc_ctrl_0_after            : 2,	/*   0: 1 */
	r1_coc_ctrl_1_after            : 2,	/*   2: 3 */
	r1_coc_ctrl_2_after            : 2,	/*   4: 5 */
	r1_coc_ctrl_3_after            : 2,	/*   6: 7 */
	                               : 8,	/*   8:15 reserved */
	r1_coc_ctrl_0_before 		   : 2,	/*  16:17 */
	r1_coc_ctrl_1_before           : 2,	/*  18:19 */
	r1_coc_ctrl_2_before           : 2,	/*  20:21 */
	r1_coc_ctrl_3_before  		   : 2;	/*  22:23 */
} R1_COC_CTRL;

/*-------------------------------------------------------------------
	0x00b0 r2_base_addr ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x00b4 r2_stride ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	r2_stride                       :15;	/*   0:14 */
} R2_STRIDE;

/*-------------------------------------------------------------------
	0x00b8 r2_pformat ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	pixel_format                    : 5,	/*   0: 4 */
	                                : 3,	/*   5: 7 reserved */
	endian_mode                     : 1;	/*      8 */
} R2_PFORMAT;

/*-------------------------------------------------------------------
	0x00bc r2_galpha ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	r2_alpha0                       : 8,	/*   0: 7 */
	r2_alpha1                       : 8,	/*   8:15 */
	r2_alpha2                       : 8,	/*  16:23 */
	r2_alpha3                       : 8;	/*  24:31 */
} R2_GALPHA;

/*-------------------------------------------------------------------
	0x0100 out_sel ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	out_sel                         : 3;	/*   0: 2 */
} OUT_SEL;

/*-------------------------------------------------------------------
	0x0104 blend_ctrl0 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	pma0                            : 1,	/*      0 */
	pma1                            : 1,	/*      1 */
	xor0                            : 1,	/*      2 */
	xor1                            : 1,	/*      3 */
	div_en                          : 1,	/*      4 */
	                                : 3,	/*   5: 7 reserved */
	c_m0                            : 2,	/*   8: 9 */
	                                : 2,	/*  10:11 reserved */
	alpha_m0                        : 2,	/*  12:13 */
	                                : 2,	/*  14:15 reserved */
	comp_sel_b                      : 2,	/*  16:17 */
	                                : 2,	/*  18:19 reserved */
	comp_sel_g                      : 2,	/*  20:21 */
	                                : 2,	/*  22:23 reserved */
	comp_sel_r                      : 2,	/*  24:25 */
	                                : 2,	/*  26:27 reserved */
	comp_sel_a                      : 2;	/*  28:29 */
} BLEND_CTRL0;

/*-------------------------------------------------------------------
	0x0108 blend_ctrl1 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	b3_sel                          : 4,	/*   0: 3 */
	a3_sel                          : 4,	/*   4: 7 */
	b2_sel                          : 4,	/*   8:11 */
	a2_sel                          : 4,	/*  12:15 */
	b1_sel                          : 4,	/*  16:19 */
	a1_sel                          : 4,	/*  20:23 */
	b0_sel                          : 4,	/*  24:27 */
	a0_sel                          : 4;	/*  28:31 */
} BLEND_CTRL1;

/*-------------------------------------------------------------------
	0x010c blend_ctrl_const ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	c_c                             :24,	/*   0:23 */
	alpha_c                         : 8;	/*  24:31 */
} BLEND_CTRL_CONST;

/*-------------------------------------------------------------------
	0x0110 rop_ctrl ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	rop_ctrl                        : 4;	/*   0: 3 */
} ROP_CTRL;

/*-------------------------------------------------------------------
	0x0120 wr_base_addr ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x0124 wr_stride ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	wr_stride                       :15;	/*   0:14 */
} WR_STRIDE;

/*-------------------------------------------------------------------
	0x0128 wr_pformat ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	pixel_format                    : 5,	/*   0: 4 */
	                                : 3,	/*   5: 7 reserved */
	endian_mode                     : 1;	/*      8 */
} WR_PFORMAT;

/*-------------------------------------------------------------------
	0x012c wr_size ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	hsize                           :13,	/*   0:12 */
	                                : 3,	/*  13:15 reserved */
	vsize                           :13;	/*  16:28 */
} WR_SIZE;

/*-------------------------------------------------------------------
	0x0130 wr_galpha ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	r2_alpha0                       : 8,	/*   0: 7 */
	r2_alpha1                       : 8,	/*   8:15 */
	r2_alpha2                       : 8,	/*  16:23 */
	r2_alpha3                       : 8;	/*  24:31 */
} WR_GALPHA;

/*-------------------------------------------------------------------
	0x0134 wr_ctrl ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	                                : 2,	/*   0: 1 reserved */
	coc_en                          : 1,	/*      2 */
	csc_en                          : 1;	/*      3 */
} WR_CTRL;

/*-------------------------------------------------------------------
	0x0138 wr_coc_ctrl ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	wr_coc_ctrl_0_after            : 2,	/*   0: 1 */
	wr_coc_ctrl_1_after            : 2,	/*   2: 3 */
	wr_coc_ctrl_2_after            : 2,	/*   4: 5 */
	wr_coc_ctrl_3_after            : 2,	/*   6: 7 */
	                               : 8,	/*   8:15 reserved */
	wr_coc_ctrl_0_before 		   : 2,	/*  16:17 */
	wr_coc_ctrl_1_before           : 2,	/*  18:19 */
	wr_coc_ctrl_2_before           : 2,	/*  20:21 */
	wr_coc_ctrl_3_before  		   : 2;	/*  22:23 */
} WR_COC_CTRL;

/*-------------------------------------------------------------------
	0x0140 wr_csc_coef_00 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	csc_coef_1                      :15,	/*   0:14 */
	                                : 1,	/*     15 reserved */
	csc_coef_0                      :15;	/*  16:30 */
} WR_CSC_COEF_00;

/*-------------------------------------------------------------------
	0x0144 wr_csc_coef_01 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	csc_coef_3                      :15,	/*   0:14 */
	                                : 1,	/*     15 reserved */
	csc_coef_2                      :15;	/*  16:30 */
} WR_CSC_COEF_01;

/*-------------------------------------------------------------------
	0x0148 wr_csc_coef_02 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	csc_coef_5                      :15,	/*   0:14 */
	                                : 1,	/*     15 reserved */
	csc_coef_4                      :15;	/*  16:30 */
} WR_CSC_COEF_02;

/*-------------------------------------------------------------------
	0x014c wr_csc_coef_03 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	csc_coef_7                      :15,	/*   0:14 */
	                                : 1,	/*     15 reserved */
	csc_coef_6                      :15;	/*  16:30 */
} WR_CSC_COEF_03;

/*-------------------------------------------------------------------
	0x0150 wr_csc_coef_04 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	csc_coef_9                      :15,	/*   0:14 */
	                                : 1,	/*     15 reserved */
	csc_coef_8                      :15;	/*  16:30 */
} WR_CSC_COEF_04;

/*-------------------------------------------------------------------
	0x0154 wr_csc_coef_05 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	csc_coef_11                     :15,	/*   0:14 */
	                                : 1,	/*     15 reserved */
	csc_coef_10                     :15;	/*  16:30 */
} WR_CSC_COEF_05;

/*-------------------------------------------------------------------
	0x0158 wr_csc_coef_06 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	csc_coef_13                     :15,	/*   0:14 */
	                                : 1,	/*     15 reserved */
	csc_coef_12                     :15;	/*  16:30 */
} WR_CSC_COEF_06;

/*-------------------------------------------------------------------
	0x015c wr_csc_coef_07 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	csc_coef_15                     :15,	/*   0:14 */
	                                : 1,	/*     15 reserved */
	csc_coef_14                     :15;	/*  16:30 */
} WR_CSC_COEF_07;

/*-------------------------------------------------------------------
	0x0180 scaler_bypass ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_bypass                   : 1;	/*      0 */
} SCALER_BYPASS;


/*-------------------------------------------------------------------
	0x0184 scaler_soft_reset ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_soft_reset                 : 1;	/*      0 */
} SCALER_SOFT_RESET;

/*-------------------------------------------------------------------
	0x0188 scaler_frame_start ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_frame_start              : 1;	/*      0 */
} SCALER_FRAME_START;

/*-------------------------------------------------------------------
	0x018c scaler_in_pic_width ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_in_pic_width             :11;	/*   0:10 */
} SCALER_IN_PIC_WIDTH;

/*-------------------------------------------------------------------
	0x0190 scaler_out_pic_width ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_out_pic_width            :11;	/*   0:10 */
} SCALER_OUT_PIC_WIDTH;

/*-------------------------------------------------------------------
	0x0194 scaler_in_pic_height ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_in_pic_height            :11;	/*   0:10 */
} SCALER_IN_PIC_HEIGHT;

/*-------------------------------------------------------------------
	0x0198 scaler_out_pic_height ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_out_pic_height           :11;	/*   0:10 */
} SCALER_OUT_PIC_HEIGHT;

/*-------------------------------------------------------------------
	0x019c scaler_line_init ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_line_init                : 1;	/*      0 */
} SCALER_LINE_INIT;

/*-------------------------------------------------------------------
	0x01a0 scaler_phase_offset ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_phase_offset             : 6;	/*   0: 5 */
} SCALER_PHASE_OFFSET;

/*-------------------------------------------------------------------
	0x01a4 scaler_boundary_mode ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_boundary_mode            : 1;	/*      0 */
} SCALER_BOUNDARY_MODE;

/*-------------------------------------------------------------------
	0x01a8 scaler_sampling_mode ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_sampling_mode            : 1;	/*      0 */
} SCALER_SAMPLING_MODE;

/*-------------------------------------------------------------------
	0x01ac scaler_numerator ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_numerator                :11;	/*   0:10 */
} SCALER_NUMERATOR;

/*-------------------------------------------------------------------
	0x01b0 scaler_denominator ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_denominator              :11;	/*   0:10 */
} SCALER_DENOMINATOR;

/*-------------------------------------------------------------------
	0x01b4 scaler_bilinear ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_bilinear                 : 1;	/*      0 */
} SCALER_BILINEAR;

/*-------------------------------------------------------------------
	0x01b8 scaler_hcoef_index ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_hcoef_index              : 2;	/*   0: 1 */
} SCALER_HCOEF_INDEX;

/*-------------------------------------------------------------------
	0x01bc scaler_vcoef_index ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_vcoef_index              : 2;	/*   0: 1 */
} SCALER_VCOEF_INDEX;

/*-------------------------------------------------------------------
	0x01c0 scaler_hphase_addr ''
--------------------------------------------------------------------*/
typedef struct {
	u32
/* raxis.lim -- fix register conversion bug fix */
	custom_filter_address           : 4,	/*   0: 3 */
	custom_filter_set_number        : 2,	/*   4: 5 */
	                                : 2,	/*   6: 7 reserved */
	custom_filter_update            : 1;	/*      8 */
} SCALER_HPHASE_ADDR;

/*-------------------------------------------------------------------
	0x01c4 scaler_hphase_data2 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_hphase_data2             :16;	/*   0:15 */
} SCALER_HPHASE_DATA2;

/*-------------------------------------------------------------------
	0x01c8 scaler_hphase_data1 ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x01cc scaler_hphase_data0 ''
--------------------------------------------------------------------*/
/*	no field */

/*-------------------------------------------------------------------
	0x01d0 scaler_vphase_addr ''
--------------------------------------------------------------------*/
typedef struct {
	u32
/* raxis.lim -- fix register conversion bug fix */
	custom_filter_address           : 4,	/*   0: 3 */
	custom_filter_set_number        : 2,	/*   4: 5 */
	                                : 2,	/*   6: 7 reserved */
	custom_filter_update            : 1;	/*      8 */
} SCALER_VPHASE_ADDR;

/*-------------------------------------------------------------------
	0x01d4 scaler_vphase_data1 ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_vphase_data1             : 8;	/*   0: 7 */
} SCALER_VPHASE_DATA1;

/*-------------------------------------------------------------------
	0x01d8 scaler_vphase_data0 ''
--------------------------------------------------------------------*/
/*	no field */


/*-------------------------------------------------------------------
	0x01f4 scaler_vh_order ''
--------------------------------------------------------------------*/
typedef struct {
	u32
	scaler_vh_order					: 1;	/*   0: 0 */
} SCALER_VH_ORDER;

typedef struct {
	u32					gfx_version            ;	/*  0x0000 : ''*/
	GFX_STATUS0			gfx_status0            ;	/*  0x0004 : '' */
	GFX_STATUS1			gfx_status1                ;	/*  0x0008 : '' */
	GFX_STATUS2			gfx_status2                    ;	/*  0x000c : ''*/
	GFX_INTR_CTRL		gfx_intr_ctrl                  ;	/*  0x0010 : ''*/
	GFX_INTR_CLEAR		gfx_intr_clear                 ;	/*  0x0014 : ''*/
	GFX_CMD_PRI			gfx_cmd_pri                    ;	/*  0x0018 : ''*/
	GFX_MEM_PORT_ID		gfx_mem_port_id                ;	/*  0x001c : ''*/
	GFX_OP_MODE			gfx_op_mode                    ;	/*  0x0020 : ''*/
	CFLT_COEF0			cflt_coef0                     ;	/*  0x0024 : ''*/
	CFLT_COEF1			cflt_coef1                     ;	/*  0x0028 : ''*/
	GFX_CMD_DLY			gfx_cmd_dly                    ;	/*  0x002c : ''*/
	GFX_BATCH_RUN		gfx_batch_run                  ;	/*  0x0030 : ''*/
	GFX_START			gfx_start                      ;	/*  0x0034 : ''*/
	GFX_PAUSE			gfx_pause               	   ;	/*  0x0038 : ''*/
	GFX_RESET			gfx_reset					;	/*  0x003c : ''*/
	DEBUG_STAT_00		debug_stat_00                  ;	/*  0x0040 : ''*/
	DEBUG_STAT_01		debug_stat_01                  ;	/*  0x0044 : ''*/
	u32					__rsvd_00[   2]				;	/*  0x0048 ~ 0x004c*/
	u32					r0_base_addr                   ;	/*  0x0050 : ''*/
	R0_STRIDE			r0_stride                      ;	/*  0x0054 : ''*/
	R0_PFORMAT			r0_pformat                     ;	/*  0x0058 : ''*/
	u32					r0_galpha                      ;	/*  0x005c : ''*/
	R0_CLUT_CTRL		r0_clut_ctrl                   ;	/*  0x0060 : ''*/
	u32					r0_clut_data                   ;	/*  0x0064 : ''*/
	R0_CTRL				r0_ctrl                        ;	/*  0x0068 : ''*/
	u32					r0_ckey_key0                   ;	/*  0x006c : ''*/
	u32					r0_ckey_key1                   ;	/*  0x0070 : ''*/
	u32					r0_ckey_replace_color          ;	/*  0x0074 : ''*/
	u32					r0_bitmask                     ;	/*  0x0078 : ''*/
	u32					r0_coc_ctrl                    ;	/*  0x007c : ''*/
	u32					r1_base_addr                   ;	/*  0x0080 : ''*/
	R1_STRIDE			r1_stride                      ;	/*  0x0084 : ''*/
	R1_PFORMAT			r1_pformat                     ;	/*  0x0088 : ''*/
	u32					r1_galpha                      ;	/*  0x008c : ''*/
	R1_CLUT_CTRL		r1_clut_ctrl                   ;	/*  0x0090 : ''*/
	u32					r1_clut_data                   ;	/*  0x0094 : ''*/
	R1_CTRL				r1_ctrl                        ;	/*  0x0098 : ''*/
	u32					r1_ckey_key0                   ;	/*  0x009c : ''*/
	u32					r1_ckey_key1                   ;	/*  0x00a0 : ''*/
	u32					r1_ckey_replace_color          ;	/*  0x00a4 : ''*/
	u32					r1_bitmask                     ;	/*  0x00a8 : ''*/
	u32					r1_coc_ctrl                    ;	/*  0x00ac : ''*/
	u32					r2_base_addr                   ;	/*  0x00b0 : ''*/
	R2_STRIDE			r2_stride                      ;	/*  0x00b4 : ''*/
	R2_PFORMAT			r2_pformat                     ;	/*  0x00b8 : ''*/
	u32					r2_galpha                      ;	/*  0x00bc : ''*/
	u32					__rsvd_01[  16]				;	/*  0x00c0 ~ 0x00fc*/
	OUT_SEL				out_sel                        ;	/*  0x0100 : ''*/
	BLEND_CTRL0			blend_ctrl0                    ;	/*  0x0104 : ''*/
	BLEND_CTRL1			blend_ctrl1                    ;	/*  0x0108 : ''*/
	BLEND_CTRL_CONST	blend_ctrl_const               ;	/*  0x010c : ''*/
	ROP_CTRL			rop_ctrl                       ;	/*  0x0110 : ''*/
	u32					__rsvd_02[   3]				;	/*  0x0114 ~ 0x011c*/
	u32					wr_base_addr                   ;	/*  0x0120 : ''*/
	WR_STRIDE			wr_stride                      ;	/*  0x0124 : ''*/
	WR_PFORMAT			wr_pformat                     ;	/*  0x0128 : ''*/
	WR_SIZE				wr_size                        ;	/*  0x012c : ''*/
	u32					wr_galpha                      ;	/*  0x0130 : ''*/
	WR_CTRL				wr_ctrl                        ;	/*  0x0134 : ''*/
	u32					wr_coc_ctrl                    ;	/*  0x0138 : ''*/
	u32					__rsvd_03[   1]				;	/*  0x013c*/
	u32					wr_csc_coef_00                 ;	/*  0x0140 : ''*/
	u32					wr_csc_coef_01                 ;	/*  0x0144 : ''*/
	u32					wr_csc_coef_02                 ;	/*  0x0148 : ''*/
	u32					wr_csc_coef_03                 ;	/*  0x014c : ''*/
	u32					wr_csc_coef_04                 ;	/*  0x0150 : ''*/
	u32					wr_csc_coef_05                 ;	/*  0x0154 : ''*/
	u32					wr_csc_coef_06                 ;	/*  0x0158 : ''*/
	u32					wr_csc_coef_07                 ;	/*  0x015c : ''*/
	u32					__rsvd_04[   8]				;	/*  0x0160 ~ 0x017c*/
	SCALER_BYPASS			scaler_bypass                 ;	/*  0x0180 : ''*/
	SCALER_SOFT_RESET		scaler_soft_reset		;	/*  0x0184 : "*/
	SCALER_FRAME_START		scaler_frame_start            ;/*  0x0188 : ''*/
	SCALER_IN_PIC_WIDTH		scaler_in_pic_width           ;/*  0x018c : ''*/
	SCALER_OUT_PIC_WIDTH	scaler_out_pic_width          ;/*  0x0190 : ''*/
	SCALER_IN_PIC_HEIGHT	scaler_in_pic_height          ;/*  0x0194 : ''*/
	SCALER_OUT_PIC_HEIGHT	scaler_out_pic_height         ;/*  0x0198 : ''*/
	SCALER_LINE_INIT		scaler_line_init              ;/*  0x019c : ''*/
	SCALER_PHASE_OFFSET		scaler_phase_offset           ;/*  0x01a0 : ''*/
	SCALER_BOUNDARY_MODE	scaler_boundary_mode          ;/*  0x01a4 : ''*/
	SCALER_SAMPLING_MODE	scaler_sampling_mode          ;/*  0x01a8 : ''*/
	SCALER_NUMERATOR		scaler_numerator              ;/*  0x01ac : ''*/
	SCALER_DENOMINATOR		scaler_denominator            ;/*  0x01b0 : ''*/
	SCALER_BILINEAR			scaler_bilinear               ;/*  0x01b4 : ''*/
	SCALER_HCOEF_INDEX		scaler_hcoef_index            ;/*  0x01b8 : ''*/
	SCALER_VCOEF_INDEX		scaler_vcoef_index            ;/*  0x01bc : ''*/
	u32						scaler_hphase_addr            ;/*  0x01c0 : ''*/
	u32						scaler_hphase_data2           ;/*  0x01c4 : ''*/
	u32						scaler_hphase_data1           ;/*  0x01c8 : ''*/
	u32						scaler_hphase_data0           ;/*  0x01cc : ''*/
	u32						scaler_vphase_addr            ;/*  0x01d0 : ''*/
	u32						scaler_vphase_data1           ;/*  0x01d4 : ''*/
	u32						scaler_vphase_data0           ;/*  0x01d8 : ''*/
	u32						__rsvd_06[   1]				;	/*  0x01dc*/
#if 1	/* raxis.lim -- UINT32 is suitable for reg access */
	u32						sw_de_sav                  ;/*  0x01e0 : ''*/
	u32						sw_cpu_gpu                 ;/*  0x01e4 : ''*/
	u32						sw_cpu_shadow              ;/*  0x01e8 : ''*/
#endif
	u32						disable_ls;				;	/*  0x01f0 */
	SCALER_VH_ORDER			scaler_vh_order;		;	/*  0x01f4*/
} GFX_REG_T;
/* 90 regs, 90 types */
/* 91 regs, 91 types in Total*/

/*
 * @{
 * Naming for register pointer.
 * g_gfx_reg : real register of GFX.
 * g_gfx_reg_cache     : shadow register.
 *
 * @def GFX_RdFL: Read  FLushing : Shadow <- Real.
 * @def GFX_WrFL: Write FLushing : Shadow -> Real.
 * @def GFX_Rd  : Read  whole register(u32) from Shadow register.
 * @def GFX_Wr  : Write whole register(u32) from Shadow register.
 * @def GFX_Rd01 ~ GFX_Rdnn: Read  given '01~nn' fields from Shadow register.
 * @def GFX_Wr01 ~ GFX_Wrnn: Write given '01~nn' fields to   Shadow register.
 * */
#define GFX_RdFL(_r)			((g_gfx_reg_cache->_r)=(g_gfx_reg->_r))
#define GFX_WrFL(_r)			((g_gfx_reg->_r)=(g_gfx_reg_cache->_r))

#define GFX_Rd(_r)				*((u32*)(&(g_gfx_reg_cache->_r)))
#define GFX_Wr(_r,_v)			((GFX_Rd(_r))=((u32)(_v)))

#define GFX_Rd00(_r,_v01)								\
								do { 						\
									(_v01) = (GFX_Rd(_r));	\
								} while (0)
#define GFX_Wr00(_r,_v01)		GFX_Wr(_r,_v01)

#define GFX_Rd01(_r,_f01,_v01)								\
								do { 						\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
								} while (0)

#define GFX_Rd02(_r,_f01,_v01,_f02,_v02)									\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
								} while (0)

#define GFX_Rd03(_r,_f01,_v01,_f02,_v02,_f03,_v03)							\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
								} while (0)

#define GFX_Rd04(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04)				\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
								} while (0)

#define GFX_Rd05(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05)												\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
									(_v05) = (g_gfx_reg_cache->_r._f05);	\
								} while (0)

#define GFX_Rd06(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06)									\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
									(_v05) = (g_gfx_reg_cache->_r._f05);	\
									(_v06) = (g_gfx_reg_cache->_r._f06);	\
								} while (0)

#define GFX_Rd07(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07)							\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
									(_v05) = (g_gfx_reg_cache->_r._f05);	\
									(_v06) = (g_gfx_reg_cache->_r._f06);	\
									(_v07) = (g_gfx_reg_cache->_r._f07);	\
								} while (0)

#define GFX_Rd08(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08)				\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
									(_v05) = (g_gfx_reg_cache->_r._f05);	\
									(_v06) = (g_gfx_reg_cache->_r._f06);	\
									(_v07) = (g_gfx_reg_cache->_r._f07);	\
									(_v08) = (g_gfx_reg_cache->_r._f08);	\
								} while (0)

#define GFX_Rd09(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09)												\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
									(_v05) = (g_gfx_reg_cache->_r._f05);	\
									(_v06) = (g_gfx_reg_cache->_r._f06);	\
									(_v07) = (g_gfx_reg_cache->_r._f07);	\
									(_v08) = (g_gfx_reg_cache->_r._f08);	\
									(_v09) = (g_gfx_reg_cache->_r._f09);	\
								} while (0)

#define GFX_Rd10(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10)									\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
									(_v05) = (g_gfx_reg_cache->_r._f05);	\
									(_v06) = (g_gfx_reg_cache->_r._f06);	\
									(_v07) = (g_gfx_reg_cache->_r._f07);	\
									(_v08) = (g_gfx_reg_cache->_r._f08);	\
									(_v09) = (g_gfx_reg_cache->_r._f09);	\
									(_v10) = (g_gfx_reg_cache->_r._f10);	\
								} while (0)

#define GFX_Rd11(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10,_f11,_v11)							\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
									(_v05) = (g_gfx_reg_cache->_r._f05);	\
									(_v06) = (g_gfx_reg_cache->_r._f06);	\
									(_v07) = (g_gfx_reg_cache->_r._f07);	\
									(_v08) = (g_gfx_reg_cache->_r._f08);	\
									(_v09) = (g_gfx_reg_cache->_r._f09);	\
									(_v10) = (g_gfx_reg_cache->_r._f10);	\
									(_v11) = (g_gfx_reg_cache->_r._f11);	\
								} while (0)

#define GFX_Rd12(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12)				\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
									(_v05) = (g_gfx_reg_cache->_r._f05);	\
									(_v06) = (g_gfx_reg_cache->_r._f06);	\
									(_v07) = (g_gfx_reg_cache->_r._f07);	\
									(_v08) = (g_gfx_reg_cache->_r._f08);	\
									(_v09) = (g_gfx_reg_cache->_r._f09);	\
									(_v10) = (g_gfx_reg_cache->_r._f10);	\
									(_v11) = (g_gfx_reg_cache->_r._f11);	\
									(_v12) = (g_gfx_reg_cache->_r._f12);	\
								} while (0)

#define GFX_Rd13(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,				\
					_f13,_v13)												\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
									(_v05) = (g_gfx_reg_cache->_r._f05);	\
									(_v06) = (g_gfx_reg_cache->_r._f06);	\
									(_v07) = (g_gfx_reg_cache->_r._f07);	\
									(_v08) = (g_gfx_reg_cache->_r._f08);	\
									(_v09) = (g_gfx_reg_cache->_r._f09);	\
									(_v10) = (g_gfx_reg_cache->_r._f10);	\
									(_v11) = (g_gfx_reg_cache->_r._f11);	\
									(_v12) = (g_gfx_reg_cache->_r._f12);	\
									(_v13) = (g_gfx_reg_cache->_r._f13);	\
								} while (0)

#define GFX_Rd14(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,				\
					_f13,_v13,_f14,_v14)									\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
									(_v05) = (g_gfx_reg_cache->_r._f05);	\
									(_v06) = (g_gfx_reg_cache->_r._f06);	\
									(_v07) = (g_gfx_reg_cache->_r._f07);	\
									(_v08) = (g_gfx_reg_cache->_r._f08);	\
									(_v09) = (g_gfx_reg_cache->_r._f09);	\
									(_v10) = (g_gfx_reg_cache->_r._f10);	\
									(_v11) = (g_gfx_reg_cache->_r._f11);	\
									(_v12) = (g_gfx_reg_cache->_r._f12);	\
									(_v13) = (g_gfx_reg_cache->_r._f13);	\
									(_v14) = (g_gfx_reg_cache->_r._f14);	\
								}while (0)

#define GFX_Rd15(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,				\
					_f13,_v13,_f14,_v14,_f15,_v15)							\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
									(_v05) = (g_gfx_reg_cache->_r._f05);	\
									(_v06) = (g_gfx_reg_cache->_r._f06);	\
									(_v07) = (g_gfx_reg_cache->_r._f07);	\
									(_v08) = (g_gfx_reg_cache->_r._f08);	\
									(_v09) = (g_gfx_reg_cache->_r._f09);	\
									(_v10) = (g_gfx_reg_cache->_r._f10);	\
									(_v11) = (g_gfx_reg_cache->_r._f11);	\
									(_v12) = (g_gfx_reg_cache->_r._f12);	\
									(_v13) = (g_gfx_reg_cache->_r._f13);	\
									(_v14) = (g_gfx_reg_cache->_r._f14);	\
									(_v15) = (g_gfx_reg_cache->_r._f15);	\
								}while (0)

#define GFX_Rd16(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,				\
					_f13,_v13,_f14,_v14,_f15,_v15,_f16,_v16)				\
								do { 										\
									(_v01) = (g_gfx_reg_cache->_r._f01);	\
									(_v02) = (g_gfx_reg_cache->_r._f02);	\
									(_v03) = (g_gfx_reg_cache->_r._f03);	\
									(_v04) = (g_gfx_reg_cache->_r._f04);	\
									(_v05) = (g_gfx_reg_cache->_r._f05);	\
									(_v06) = (g_gfx_reg_cache->_r._f06);	\
									(_v07) = (g_gfx_reg_cache->_r._f07);	\
									(_v08) = (g_gfx_reg_cache->_r._f08);	\
									(_v09) = (g_gfx_reg_cache->_r._f09);	\
									(_v10) = (g_gfx_reg_cache->_r._f10);	\
									(_v11) = (g_gfx_reg_cache->_r._f11);	\
									(_v12) = (g_gfx_reg_cache->_r._f12);	\
									(_v13) = (g_gfx_reg_cache->_r._f13);	\
									(_v14) = (g_gfx_reg_cache->_r._f14);	\
									(_v15) = (g_gfx_reg_cache->_r._f15);	\
									(_v16) = (g_gfx_reg_cache->_r._f16);	\
								}while (0)



#define GFX_Wr01(_r,_f01,_v01)												\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
								} while (0)

#define GFX_Wr02(_r,_f01,_v01,_f02,_v02)									\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
								} while (0)

#define GFX_Wr03(_r,_f01,_v01,_f02,_v02,_f03,_v03)							\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
								} while (0)

#define GFX_Wr04(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04)				\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
								} while (0)

#define GFX_Wr05(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05)												\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
									(g_gfx_reg_cache->_r._f05) = (_v05);	\
								} while (0)

#define GFX_Wr06(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06)									\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
									(g_gfx_reg_cache->_r._f05) = (_v05);	\
									(g_gfx_reg_cache->_r._f06) = (_v06);	\
								} while (0)

#define GFX_Wr07(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07)							\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
									(g_gfx_reg_cache->_r._f05) = (_v05);	\
									(g_gfx_reg_cache->_r._f06) = (_v06);	\
									(g_gfx_reg_cache->_r._f07) = (_v07);	\
								} while (0)

#define GFX_Wr08(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08)				\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
									(g_gfx_reg_cache->_r._f05) = (_v05);	\
									(g_gfx_reg_cache->_r._f06) = (_v06);	\
									(g_gfx_reg_cache->_r._f07) = (_v07);	\
									(g_gfx_reg_cache->_r._f08) = (_v08);	\
								} while (0)

#define GFX_Wr09(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09)												\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
									(g_gfx_reg_cache->_r._f05) = (_v05);	\
									(g_gfx_reg_cache->_r._f06) = (_v06);	\
									(g_gfx_reg_cache->_r._f07) = (_v07);	\
									(g_gfx_reg_cache->_r._f08) = (_v08);	\
									(g_gfx_reg_cache->_r._f09) = (_v09);	\
								} while (0)

#define GFX_Wr10(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10)									\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
									(g_gfx_reg_cache->_r._f05) = (_v05);	\
									(g_gfx_reg_cache->_r._f06) = (_v06);	\
									(g_gfx_reg_cache->_r._f07) = (_v07);	\
									(g_gfx_reg_cache->_r._f08) = (_v08);	\
									(g_gfx_reg_cache->_r._f09) = (_v09);	\
									(g_gfx_reg_cache->_r._f10) = (_v10);	\
								} while (0)

#define GFX_Wr11(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10,_f11,_v11)							\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
									(g_gfx_reg_cache->_r._f05) = (_v05);	\
									(g_gfx_reg_cache->_r._f06) = (_v06);	\
									(g_gfx_reg_cache->_r._f07) = (_v07);	\
									(g_gfx_reg_cache->_r._f08) = (_v08);	\
									(g_gfx_reg_cache->_r._f09) = (_v09);	\
									(g_gfx_reg_cache->_r._f10) = (_v10);	\
									(g_gfx_reg_cache->_r._f11) = (_v11);	\
								} while (0)

#define GFX_Wr12(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12)				\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
									(g_gfx_reg_cache->_r._f05) = (_v05);	\
									(g_gfx_reg_cache->_r._f06) = (_v06);	\
									(g_gfx_reg_cache->_r._f07) = (_v07);	\
									(g_gfx_reg_cache->_r._f08) = (_v08);	\
									(g_gfx_reg_cache->_r._f09) = (_v09);	\
									(g_gfx_reg_cache->_r._f10) = (_v10);	\
									(g_gfx_reg_cache->_r._f11) = (_v11);	\
									(g_gfx_reg_cache->_r._f12) = (_v12);	\
								} while (0)

#define GFX_Wr13(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,				\
					_f13,_v13)												\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
									(g_gfx_reg_cache->_r._f05) = (_v05);	\
									(g_gfx_reg_cache->_r._f06) = (_v06);	\
									(g_gfx_reg_cache->_r._f07) = (_v07);	\
									(g_gfx_reg_cache->_r._f08) = (_v08);	\
									(g_gfx_reg_cache->_r._f09) = (_v09);	\
									(g_gfx_reg_cache->_r._f10) = (_v10);	\
									(g_gfx_reg_cache->_r._f11) = (_v11);	\
									(g_gfx_reg_cache->_r._f12) = (_v12);	\
									(g_gfx_reg_cache->_r._f13) = (_v13);	\
								} while (0)

#define GFX_Wr14(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,				\
					_f13,_v13,_f14,_v14)									\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
									(g_gfx_reg_cache->_r._f05) = (_v05);	\
									(g_gfx_reg_cache->_r._f06) = (_v06);	\
									(g_gfx_reg_cache->_r._f07) = (_v07);	\
									(g_gfx_reg_cache->_r._f08) = (_v08);	\
									(g_gfx_reg_cache->_r._f09) = (_v09);	\
									(g_gfx_reg_cache->_r._f10) = (_v10);	\
									(g_gfx_reg_cache->_r._f11) = (_v11);	\
									(g_gfx_reg_cache->_r._f12) = (_v12);	\
									(g_gfx_reg_cache->_r._f13) = (_v13);	\
									(g_gfx_reg_cache->_r._f14) = (_v14);	\
								}while (0)

#define GFX_Wr15(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,				\
					_f13,_v13,_f14,_v14,_f15,_v15)							\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
									(g_gfx_reg_cache->_r._f05) = (_v05);	\
									(g_gfx_reg_cache->_r._f06) = (_v06);	\
									(g_gfx_reg_cache->_r._f07) = (_v07);	\
									(g_gfx_reg_cache->_r._f08) = (_v08);	\
									(g_gfx_reg_cache->_r._f09) = (_v09);	\
									(g_gfx_reg_cache->_r._f10) = (_v10);	\
									(g_gfx_reg_cache->_r._f11) = (_v11);	\
									(g_gfx_reg_cache->_r._f12) = (_v12);	\
									(g_gfx_reg_cache->_r._f13) = (_v13);	\
									(g_gfx_reg_cache->_r._f14) = (_v14);	\
									(g_gfx_reg_cache->_r._f15) = (_v15);	\
								}while (0)

#define GFX_Wr16(_r,_f01,_v01,_f02,_v02,_f03,_v03,_f04,_v04,				\
					_f05,_v05,_f06,_v06,_f07,_v07,_f08,_v08,				\
					_f09,_v09,_f10,_v10,_f11,_v11,_f12,_v12,				\
					_f13,_v13,_f14,_v14,_f15,_v15,_f16,_v16)				\
								do { 										\
									(g_gfx_reg_cache->_r._f01) = (_v01);	\
									(g_gfx_reg_cache->_r._f02) = (_v02);	\
									(g_gfx_reg_cache->_r._f03) = (_v03);	\
									(g_gfx_reg_cache->_r._f04) = (_v04);	\
									(g_gfx_reg_cache->_r._f05) = (_v05);	\
									(g_gfx_reg_cache->_r._f06) = (_v06);	\
									(g_gfx_reg_cache->_r._f07) = (_v07);	\
									(g_gfx_reg_cache->_r._f08) = (_v08);	\
									(g_gfx_reg_cache->_r._f09) = (_v09);	\
									(g_gfx_reg_cache->_r._f10) = (_v10);	\
									(g_gfx_reg_cache->_r._f11) = (_v11);	\
									(g_gfx_reg_cache->_r._f12) = (_v12);	\
									(g_gfx_reg_cache->_r._f13) = (_v13);	\
									(g_gfx_reg_cache->_r._f14) = (_v14);	\
									(g_gfx_reg_cache->_r._f15) = (_v15);	\
									(g_gfx_reg_cache->_r._f16) = (_v16);	\
								}while (0)


#ifdef __cplusplus
}
#endif

#endif	/* _GFX_CORE_REG_H_ */
