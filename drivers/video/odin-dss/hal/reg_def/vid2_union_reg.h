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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 */

#include "reg_types.h"

#ifndef __VID2_UNION_REG_H
#define __VID2_UNION_REG_H


/*----------------------------------------------------------------------------
	0x31020000 vid2_rd_cfg0
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_en                                           : 1; /* 0 */
		u32 src_dst_sel                                      : 1; /* 1 */
		u32 reserved2                                        : 2; /* 3:2 */
		u32 src_buf_sel                                      : 2; /* 5:4 */
		u32 src_trans_num                                    : 2; /* 7:6 */
		u32 src_sync_sel                                     : 2; /* 9:8 */
		u32 reserved6                                        : 2; /* 11:10 */
		u32 src_reg_sw_update                                : 1; /* 12 */
		u32 reserved8                                        :19; /* 31:13 */
	} af;
	u32 a32;
} vid2_rd_cfg0;

/*----------------------------------------------------------------------------
	0x31020004 vid2_rd_cfg1
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_format                                       : 4; /* 3:0 */
		u32 src_byte_swap                                    : 2; /* 5:4 */
		u32 src_word_swap                                    : 1; /* 6 */
		u32 src_alpha_swap                                   : 1; /* 7 */
		u32 src_rgb_swap                                     : 3; /* 10:8 */
		u32 src_yuv_swap                                     : 1; /* 11 */
		u32 reserved6                                        : 2; /* 13:12 */
		u32 src_lsb_sel                                      : 1; /* 14 */
		u32 reserved8                                        : 5; /* 19:15 */
		u32 src_flip_mode                                    : 2; /* 21:20 */
		u32 reserved10                                       :10; /* 31:22 */
	} af;
	u32 a32;
} vid2_rd_cfg1;

/*----------------------------------------------------------------------------
	0x31020008 vid2_rd_cfg2
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 reserved0                                        :32; /* 31:0 */
	} af;
	u32 a32;
} vid2_rd_cfg2;

/*----------------------------------------------------------------------------
	0x31020028 vid2_rd_width_height
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_width                                        :13; /* 12:0 */
		u32 reserved1                                        : 3; /* 15:13 */
		u32 src_height                                       :13; /* 28:16 */
		u32 reserved3                                        : 3; /* 31:29 */
	} af;
	u32 a32;
} vid2_rd_width_height;

/*----------------------------------------------------------------------------
	0x3102002c vid2_rd_startxy
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_sx                                           :13; /* 12:0 */
		u32 reserved1                                        : 3; /* 15:13 */
		u32 src_sy                                           :13; /* 28:16 */
		u32 reserved3                                        : 3; /* 31:29 */
	} af;
	u32 a32;
} vid2_rd_startxy;

/*----------------------------------------------------------------------------
	0x31020030 vid2_rd_sizexy
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_sizex                                        :13; /* 12:0 */
		u32 reserved1                                        : 3; /* 15:13 */
		u32 src_sizey                                        :13; /* 28:16 */
		u32 reserved3                                        : 3; /* 31:29 */
	} af;
	u32 a32;
} vid2_rd_sizexy;

/*----------------------------------------------------------------------------
	0x31020040 vid2_rd_dss_base_y_addr0
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_y0                                   :32; /* 31:0 */
	} af;
	u32 a32;
} vid2_rd_dss_base_y_addr0;

/*----------------------------------------------------------------------------
	0x31020044 vid2_rd_dss_base_u_addr0
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_u0                                   :32; /* 31:0 */
	} af;
	u32 a32;
} vid2_rd_dss_base_u_addr0;

/*----------------------------------------------------------------------------
	0x31020048 vid2_rd_dss_base_v_addr0
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_v0                                   :32; /* 31:0 */
	} af;
	u32 a32;
} vid2_rd_dss_base_v_addr0;

/*----------------------------------------------------------------------------
	0x3102004c vid2_rd_dss_base_y_addr1
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_y1                                   :32; /* 31:0 */
	} af;
	u32 a32;
} vid2_rd_dss_base_y_addr1;

/*----------------------------------------------------------------------------
	0x31020050 vid2_rd_dss_base_u_addr1
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_u1                                   :32; /* 31:0 */
	} af;
	u32 a32;
} vid2_rd_dss_base_u_addr1;

/*----------------------------------------------------------------------------
	0x31020054 vid2_rd_dss_base_v_addr1
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_v1                                   :32; /* 31:0 */
	} af;
	u32 a32;
} vid2_rd_dss_base_v_addr1;

/*----------------------------------------------------------------------------
	0x31020058 vid2_rd_dss_base_y_addr2
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_y2                                   :32; /* 31:0 */
	} af;
	u32 a32;
} vid2_rd_dss_base_y_addr2;

/*----------------------------------------------------------------------------
	0x3102005c vid2_rd_dss_base_u_addr2
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_u2                                   :32; /* 31:0 */
	} af;
	u32 a32;
} vid2_rd_dss_base_u_addr2;

/*----------------------------------------------------------------------------
	0x31020060 vid2_rd_dss_base_v_addr2
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_v2                                   :32; /* 31:0 */
	} af;
	u32 a32;
} vid2_rd_dss_base_v_addr2;

/*----------------------------------------------------------------------------
	0x31020064 vid2_rd_status
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 rdmif_full_cmdfifo                               : 1; /* 0 */
		u32 rdmif_full_datfifo                               : 1; /* 1 */
		u32 reserved2                                        : 2; /* 3:2 */
		u32 rdmif_empty_cmdfifo                              : 1; /* 4 */
		u32 rdmif_empty_datfifo                              : 1; /* 5 */
		u32 reserved5                                        : 2; /* 7:6 */
		u32 rdbif_full_datfifo_y                             : 1; /* 8 */
		u32 rdbif_full_datfifo_u                             : 1; /* 9 */
		u32 rdbif_full_datfifo_v                             : 1; /* 10 */
		u32 reserved9                                        : 1; /* 11 */
		u32 rdbif_empty_datfifo_y                            : 1; /* 12 */
		u32 rdbif_empty_datfifo_u                            : 1; /* 13 */
		u32 rdbif_empty_datfifo_v                            : 1; /* 14 */
		u32 reserved13                                       : 1; /* 15 */
		u32 rdbif_con_state_y                                : 1; /* 16 */
		u32 rdbif_con_state_u                                : 1; /* 17 */
		u32 rdbif_con_state_v                                : 1; /* 18 */
		u32 rdbif_con_sync_state                             : 1; /* 19 */
		u32 rdbif_fmc_y_state                                : 1; /* 20 */
		u32 rdbif_fmc_uv_state                               : 1; /* 21 */
		u32 rdbif_fmc_sync_state                             : 1; /* 22 */
		u32 reserved21                                       : 9; /* 31:23 */
	} af;
	u32 a32;
} vid2_rd_status;

/*----------------------------------------------------------------------------
	0x31020078 vid2_fmt_cfg
----------------------------------------------------------------------------*/
typedef union {
	struct {
		u32 fmt_sync_en                                      : 1; /* 0 */
		u32 reserved1                                        : 3; /* 3:1 */
		u32 fmt_sync_sel                                     : 2; /* 5:4 */
		u32 reserved3                                        : 9; /* 31:23 */
	} af;
	u32 a32;
} vid2_fmt_cfg;

typedef struct {
	vid2_rd_cfg0                  	i2_rd_cfg0;
	vid2_rd_cfg1                  	i2_rd_cfg1;
	vid2_rd_cfg2                  	i2_rd_cfg2;
	vid2_rd_width_height          	i2_rd_width_height;
	vid2_rd_startxy               	i2_rd_startxy;
	vid2_rd_sizexy                	i2_rd_sizexy;
	vid2_rd_dss_base_y_addr0      	i2_rd_dss_base_y_addr0;
	vid2_rd_dss_base_u_addr0      	i2_rd_dss_base_u_addr0;
	vid2_rd_dss_base_v_addr0      	i2_rd_dss_base_v_addr0;
	vid2_rd_dss_base_y_addr1      	i2_rd_dss_base_y_addr1;
	vid2_rd_dss_base_u_addr1      	i2_rd_dss_base_u_addr1;
	vid2_rd_dss_base_v_addr1      	i2_rd_dss_base_v_addr1;
	vid2_rd_dss_base_y_addr2      	i2_rd_dss_base_y_addr2;
	vid2_rd_dss_base_u_addr2      	i2_rd_dss_base_u_addr2;
	vid2_rd_dss_base_v_addr2      	i2_rd_dss_base_v_addr2;
	vid2_rd_status                	i2_rd_status;
	vid2_fmt_cfg                  	i2_fmt_cfg;
} DSS_DU_VID2_REG_T;
#endif

