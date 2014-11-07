#ifndef __GSCL_UNION_REG_H
#define __GSCL_UNION_REG_H

/*-----------------------------------------------------------------
	0x31250000 gscl_rd_cfg0
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_en                                : 1; /* 0 */
		u32 reserved1                             : 3; /* 3:1 */
		u32 src_buf_sel                           : 2; /* 5:4 */
		u32 src_trans_num                         : 2; /* 7:6 */
		u32 src_sync_sel                          : 2; /* 9:8 */
		u32 reserved5                             : 2; /* 11:10 */
		u32 src_reg_sw_update                     : 1; /* 12 */
		u32 reserved7                             :19; /* 31:13 */
	} af;
	u32 a32;
} gscl_rd_cfg0;

/*-----------------------------------------------------------------
	0x31250004 gscl_rd_cfg1
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_format                            : 3; /* 2:0 */
		u32 reserved1                             : 1; /* 3 */
		u32 src_byte_swap                         : 2; /* 5:4 */
		u32 src_word_swap                         : 1; /* 6 */
		u32 src_alpha_swap                        : 1; /* 7 */
		u32 src_rgb_swap                          : 3; /* 10:8 */
		u32 reserved6                             : 2; /* 12:11 */
		u32 reserved7                             : 1; /* 13 */
		u32 src_lsb_sel                           : 1; /* 14 */
		u32 reserved9                             : 2; /* 16:15 */
		u32 reserved10                            : 3; /* 19:17 */
		u32 src_flip_op                           : 2; /* 21:20 */
		u32 reserved12                            :10; /* 31:22 */
	} af;
	u32 a32;
} gscl_rd_cfg1;

/*-----------------------------------------------------------------
	0x31250028 gscl_rd_width_height
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_width                             :13; /* 12:0 */
		u32 reserved1                             : 3; /* 15:13 */
		u32 src_height                            :13; /* 28:16 */
		u32 reserved3                             : 3; /* 31:29 */
	} af;
	u32 a32;
} gscl_rd_width_height;

/*-----------------------------------------------------------------
	0x3125002c gscl_rd_startxy
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_sx                                :13; /* 12:0 */
		u32 reserved1                             : 3; /* 15:13 */
		u32 src_sy                                :13; /* 28:16 */
		u32 reserved3                             : 3; /* 31:29 */
	} af;
	u32 a32;
} gscl_rd_startxy;

/*-----------------------------------------------------------------
	0x31250030 gscl_rd_sizexy
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_sizex                             :13; /* 12:0 */
		u32 reserved1                             : 3; /* 15:13 */
		u32 src_sizey                             :13; /* 28:16 */
		u32 reserved3                             : 3; /* 31:29 */
	} af;
	u32 a32;
} gscl_rd_sizexy;

/*-----------------------------------------------------------------
	0x31250040 gscl_rd_dss_base_y_addr0
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_y0                        :32; /* 31:0 */
	} af;
	u32 a32;
} gscl_rd_dss_base_y_addr0;

/*-----------------------------------------------------------------
	0x3125004c gscl_rd_dss_base_y_addr1
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_y1                        :32; /* 31:0 */
	} af;
	u32 a32;
} gscl_rd_dss_base_y_addr1;

/*-----------------------------------------------------------------
	0x31250058 gscl_rd_dss_base_y_addr2
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_y2                        :32; /* 31:0 */
	} af;
	u32 a32;
} gscl_rd_dss_base_y_addr2;

/*-----------------------------------------------------------------
	0x31250064 gscl_rd_status
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 rdmif_full_cmdfifo                    : 1; /* 0 */
		u32 rdmif_full_datfifo                    : 1; /* 1 */
		u32 reserved2                             : 2; /* 3:2 */
		u32 rdmif_empty_cmdfifo                   : 1; /* 4 */
		u32 rdmif_empty_datfifo                   : 1; /* 5 */
		u32 reserved5                             : 2; /* 7:6 */
		u32 rdbif_full_datfifo_y                  : 1; /* 8 */
		u32 reserved7                             : 2; /* 10:9 */
		u32 reserved8                             : 1; /* 11 */
		u32 rdbif_empty_datfifo_y                 : 1; /* 12 */
		u32 reserved10                            : 2; /* 14:13 */
		u32 reserved11                            : 1; /* 15 */
		u32 rdbif_con_state                       : 1; /* 16 */
		u32 rdbif_con_sync_state                  : 1; /* 17 */
		u32 reserved14                            : 2; /* 19:18 */
		u32 rdbif_fmc_y_state                     : 1; /* 20 */
		u32 reserved16                            : 1; /* 21 */
		u32 rdbif_fmc_sync_state                  : 1; /* 22 */
		u32 reserved18                            : 9; /* 31:23 */
	} af;
	u32 a32;
} gscl_rd_status;

/*-----------------------------------------------------------------
	0x31250190 gscl_wb_cfg0
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 wb_en                                 : 1; /* 0 */
		u32 wb_alpha_reg_en                       : 1; /* 1 */
		u32 reserved2                             : 2; /* 3:2 */
		u32 wb_buf_sel                            : 1; /* 4 */
		u32 reserved4                             : 3; /* 7:5 */
		u32 wb_sync_sel                           : 2; /* 9:8 */
		u32 reserved6                             : 2; /* 11:10 */
		u32 wb_reg_sw_update                      : 1; /* 12 */
		u32 reserved8                             :19; /* 31:13 */
	} af;
	u32 a32;
} gscl_wb_cfg0;

/*-----------------------------------------------------------------
	0x31250194 gscl_wb_cfg1
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 wb_format                             : 4; /* 3:0 */
		u32 wb_byte_swap                          : 2; /* 5:4 */
		u32 wb_word_swap                          : 1; /* 6 */
		u32 wb_alpha_swap                         : 1; /* 7 */
		u32 wb_rgb_swap                           : 3; /* 10:8 */
		u32 wb_yuv_swap                           : 1; /* 11 */
		u32 reserved6                             : 2; /* 13:12 */
		u32 wb_yuv_range                          : 1; /* 14 */
		u32 reserved8                             : 2; /* 16:15 */
		u32 ovl_inf_src_sel                       : 2; /* 18:17 */
		u32 reserved10                            :13; /* 31:19 */
	} af;
	u32 a32;
} gscl_wb_cfg1;

/*-----------------------------------------------------------------
	0x312501b8 gscl_wb_alpha_value
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 wb_alpha                              : 8; /* 7:0 */
		u32 reserved1                             :24; /* 31:8 */
	} af;
	u32 a32;
} gscl_wb_alpha_value;

/*-----------------------------------------------------------------
	0x312501bc gscl_wb_width_height
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 wb_width                              :13; /* 12:0 */
		u32 reserved1                             : 3; /* 15:13 */
		u32 wb_height                             :13; /* 28:16 */
		u32 reserved3                             : 3; /* 31:29 */
	} af;
	u32 a32;
} gscl_wb_width_height;

/*-----------------------------------------------------------------
	0x312501c0 gscl_wb_startxy
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 wb_sx                                 :13; /* 12:0 */
		u32 reserved1                             : 3; /* 15:13 */
		u32 wb_sy                                 :13; /* 28:16 */
		u32 reserved3                             : 3; /* 31:29 */
	} af;
	u32 a32;
} gscl_wb_startxy;

/*-----------------------------------------------------------------
	0x312501c4 gscl_wb_sizexy
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 wb_sizex                              :13; /* 12:0 */
		u32 reserved1                             : 3; /* 15:13 */
		u32 wb_sizey                              :13; /* 28:16 */
		u32 reserved3                             : 3; /* 31:29 */
	} af;
	u32 a32;
} gscl_wb_sizexy;

/*-----------------------------------------------------------------
	0x312501c8 gscl_wb_cfg2
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 wb_line_intval                        : 8; /* 7:0 */
		u32 wb_trans_intval                       : 8; /* 15:8 */
		u32 reserved2                             :16; /* 31:16 */
	} af;
	u32 a32;
} gscl_wb_cfg2;

/*-----------------------------------------------------------------
	0x312501cc gscl_wb_dss_base_y_addr0
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 wb_st_addr_y0                         :32; /* 31:0 */
	} af;
	u32 a32;
} gscl_wb_dss_base_y_addr0;

/*-----------------------------------------------------------------
	0x312501d0 gscl_wb_dss_base_u_addr0
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 wb_st_addr_u0                         :32; /* 31:0 */
	} af;
	u32 a32;
} gscl_wb_dss_base_u_addr0;

/*-----------------------------------------------------------------
	0x312501d8 gscl_wb_dss_base_y_addr1
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 wb_st_addr_y1                         :32; /* 31:0 */
	} af;
	u32 a32;
} gscl_wb_dss_base_y_addr1;

/*-----------------------------------------------------------------
	0x312501dc gscl_wb_dss_base_u_addr1
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 wb_st_addr_u1                         :32; /* 31:0 */
	} af;
	u32 a32;
} gscl_wb_dss_base_u_addr1;

/*-----------------------------------------------------------------
	0x312501e4 gscl_wb_status
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 wrctrl_full_inf_fifoy                 : 1; /* 0 */
		u32 wrctrl_full_inf_fifou                 : 1; /* 1 */
		u32 wrctrl_full_inf_fifov                 : 1; /* 2 */
		u32 reserved3                             : 1; /* 3 */
		u32 wrctrl_empty_inf_fifoy                : 1; /* 4 */
		u32 wrctrl_empty_inf_fifou                : 1; /* 5 */
		u32 wrctrl_empty_inf_fifov                : 1; /* 6 */
		u32 reserved7                             : 1; /* 7 */
		u32 wrctrl_con_y_state                    : 1; /* 8 */
		u32 wrctrl_con_u_state                    : 1; /* 9 */
		u32 wrctrl_con_v_state                    : 1; /* 10 */
		u32 wrctrl_con_sync_state                 : 1; /* 11 */
		u32 wrmif_full_cmdfifo                    : 1; /* 12 */
		u32 wrmif_full_datfifo                    : 1; /* 13 */
		u32 reserved14                            : 2; /* 15:14 */
		u32 wrmif_empty_cmdfifo                   : 1; /* 16 */
		u32 wrmif_empty_datfifo                   : 1; /* 17 */
		u32 reserved17                            :14; /* 31:18 */
	} af;
	u32 a32;
} gscl_wb_status;

typedef struct {
	gscl_rd_cfg0                  	rd_cfg0;
	gscl_rd_cfg1                  	rd_cfg1;
	gscl_rd_width_height          	rd_width_height;
	gscl_rd_startxy               	rd_startxy;
	gscl_rd_sizexy                	rd_sizexy;
	gscl_rd_dss_base_y_addr0      	rd_dss_base_y_addr0;
	gscl_rd_dss_base_y_addr1      	rd_dss_base_y_addr1;
	gscl_rd_dss_base_y_addr2      	rd_dss_base_y_addr2;
	gscl_rd_status                	rd_status;
	gscl_wb_cfg0                  	wb_cfg0;
	gscl_wb_cfg1                  	wb_cfg1;
	gscl_wb_alpha_value           	wb_alpha_value;
	gscl_wb_width_height          	wb_width_height;
	gscl_wb_startxy               	wb_startxy;
	gscl_wb_sizexy                	wb_sizexy;
	gscl_wb_cfg2                  	wb_cfg2;
	gscl_wb_dss_base_y_addr0      	wb_dss_base_y_addr0;
	gscl_wb_dss_base_u_addr0      	wb_dss_base_u_addr0;
	gscl_wb_dss_base_y_addr1      	wb_dss_base_y_addr1;
	gscl_wb_dss_base_u_addr1      	wb_dss_base_u_addr1;
	gscl_wb_status                	wb_status;
} DSS_DU_GSCL_REG_T;
#endif

