#ifndef __GRA1_UNION_REG_H
#define __GRA1_UNION_REG_H


/*----------------------------------------------------------------
	0x31210000 gra1_rd_cfg0
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_en                               : 1; /* 0 */
		u32 src_dst_sel                          : 1; /* 1 */
		u32 reserved2                            : 2; /* 3:2 */
		u32 src_buf_sel                          : 2; /* 5:4 */
		u32 src_trans_num                        : 2; /* 7:6 */
		u32 src_sync_sel                         : 2; /* 9:8 */
		u32 dma_index_rd_start                   : 1; /* 10 */
		u32 reserved7                            : 1; /* 11 */
		u32 src_reg_sw_update                    : 1; /* 12 */
		u32 reserved9                            :19; /* 31:13 */
	} af;
	u32 a32;
} gra1_rd_cfg0;

/*----------------------------------------------------------------
	0x31210004 gra1_rd_cfg1
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_format                           : 3; /* 2:0 */
		u32 reserved1                            : 1; /* 3 */
		u32 src_byte_swap                        : 2; /* 5:4 */
		u32 src_word_swap                        : 1; /* 6 */
		u32 src_alpha_swap                       : 1; /* 7 */
		u32 src_rgb_swap                         : 3; /* 10:8 */
		u32 reserved6                            : 3; /* 13:11 */
		u32 src_lsb_sel                          : 1; /* 14 */
		u32 reserved8                            : 5; /* 19:15 */
		u32 src_flip_mode                        : 2; /* 21:20 */
		u32 reserved10                           :10; /* 31:22 */
	} af;
	u32 a32;
} gra1_rd_cfg1;

/*----------------------------------------------------------------
	0x31210028 gra1_rd_width_height
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_width                            :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 src_height                           :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} gra1_rd_width_height;

/*----------------------------------------------------------------
	0x3121002c gra1_rd_startxy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_sx                               :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 src_sy                               :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} gra1_rd_startxy;

/*----------------------------------------------------------------
	0x31210030 gra1_rd_sizexy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_sizex                            :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 src_sizey                            :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} gra1_rd_sizexy;

/*----------------------------------------------------------------
	0x31210040 gra1_rd_dss_base_y_addr0
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_y0                       :32; /* 31:0 */
	} af;
	u32 a32;
} gra1_rd_dss_base_y_addr0;

/*----------------------------------------------------------------
	0x3121004c gra1_rd_dss_base_y_addr1
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_y1                       :32; /* 31:0 */
	} af;
	u32 a32;
} gra1_rd_dss_base_y_addr1;

/*----------------------------------------------------------------
	0x31210058 gra1_rd_dss_base_y_addr2
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 src_st_addr_y2                       :32; /* 31:0 */
	} af;
	u32 a32;
} gra1_rd_dss_base_y_addr2;

/*----------------------------------------------------------------
	0x31210064 gra1_rd_status
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 rdmif_full_cmdfifo                   : 1; /* 0 */
		u32 rdmif_full_datfifo                   : 1; /* 1 */
		u32 reserved2                            : 2; /* 3:2 */
		u32 rdmif_empty_cmdfifo                  : 1; /* 4 */
		u32 rdmif_empty_datfifo                  : 1; /* 5 */
		u32 reserved5                            : 2; /* 7:6 */
		u32 rdbif_full_datfifo_y                 : 1; /* 8 */
		u32 reserved7                            : 3; /* 11:9 */
		u32 rdbif_empty_datfifo_y                : 1; /* 12 */
		u32 reserved9                            : 3; /* 15:13 */
		u32 rdbif_con_state                      : 1; /* 16 */
		u32 rdbif_con_sync_state                 : 1; /* 17 */
		u32 reserved12                           : 2; /* 19:18 */
		u32 rdbif_fmc_y_state                    : 1; /* 20 */
		u32 reserved14                           : 1; /* 21 */
		u32 rdbif_fmc_sync_state                 : 1; /* 22 */
		u32 reserved16                           : 9; /* 31:23 */
	} af;
	u32 a32;
} gra1_rd_status;

typedef struct {
	gra1_rd_cfg0                  	i1_rd_cfg0;
	gra1_rd_cfg1                  	i1_rd_cfg1;
	gra1_rd_width_height          	i1_rd_width_height;
	gra1_rd_startxy               	i1_rd_startxy;
	gra1_rd_sizexy                	i1_rd_sizexy;
	gra1_rd_dss_base_y_addr0      	i1_rd_dss_base_y_addr0;
	gra1_rd_dss_base_y_addr1      	i1_rd_dss_base_y_addr1;
	gra1_rd_dss_base_y_addr2      	i1_rd_dss_base_y_addr2;
	gra1_rd_status                	i1_rd_status;
} DSS_DU_GRA1_REG_T;

#endif

