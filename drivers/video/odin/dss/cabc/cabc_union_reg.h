#ifndef _REG_CABC_H
#define _REG_CABC_H


/*---------------------------------------------------------------------------
	0x31240000 cabc1_size_ctrl
---------------------------------------------------------------------------*/
typedef union{
    struct {
        u32 image_width_size                           :12;    /* 11:0 */
        u32 reserved1                                  : 4;    /* 15:12*/
        u32 image_height_size                          :11;    /* 26:16*/
        u32 reserved3                                  : 4;    /* 30:27*/
        u32 reg_update_mode                            : 1;    /* 31   */
    } af;
    u32 a32;
} cabc1_size_ctrl;

/*---------------------------------------------------------------------------
	0x31240004 cabc1_csc_ctrl
---------------------------------------------------------------------------*/
typedef union{
    struct {
        u32 r_coef                                     :10;    /* 9:0 */
        u32 g_coef                                     :10;    /* 19:10*/
        u32 b_coef                                     :10;    /* 29:20*/
        u32 reserved3                                  : 2;    /* 31:30*/
    } af;
    u32 a32;
} cabc1_csc_ctrl;

/*---------------------------------------------------------------------------
	0x31240008 cabc1_top_ctrl
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 cabc_en                                    : 1;    /* 0*/
        u32 hist_step_mode                             : 1;    /* 1*/
        u32 cabc_gain_range                            : 2;    /* 3:2*/
        u32 reserved3                                  :12;    /* 15:4*/
        u32 cabc_global_gain                           :12;    /* 27:16*/
        u32 reserved5                                  : 3;    /* 30:28*/
        u32 interrupt_mask                             : 1;    /* 31*/
    } af;
    u32 a32;
} cabc1_top_ctrl;

/*---------------------------------------------------------------------------
	0x3124000c cabc1_gain_lut_ctrl
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 cabc_lut_wen                               : 1;    /* 0*/
        u32 cabc_lut_adrs                              : 8;    /* 8:1*/
        u32 reserved2                                  : 7;    /* 15:9*/
        u32 cabc_lut_wdata                             :12;    /* 27:16*/
        u32 reserved4                                  : 4;    /* 31:28*/
    } af;
    u32 a32;
} cabc1_gain_lut_ctrl;

/*---------------------------------------------------------------------------
	0x31240010 cabc1_gain_lut_data
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 cabc_lut_rdata                             :12;    /* 11:0*/
        u32 reserved1                                  :20;    /* 31:12*/
    } af;
    u32 a32;
} cabc1_gain_lut_data;

/*---------------------------------------------------------------------------
	0x31240014 cabc1_hist_lut_adrs
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 hist_lut_adrs                              : 5;    /* 4:0*/
        u32 reserved1                                  :27;    /* 31:5*/
    } af;
    u32 a32;
} cabc1_hist_lut_adrs;

/*---------------------------------------------------------------------------
	0x31240018 cabc1_hist_lut_rdata
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 hist_lut_rdata                             :23;    /* 22:0*/
        u32 reserved1                                  : 9;    /* 31:23*/
    } af;
    u32 a32;
} cabc1_hist_lut_rdata;

/*---------------------------------------------------------------------------
	0x3124001c cabc1_apl
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 apl_y                                      : 8;    /* 7:0 */
        u32 apl_r                                      : 8;    /* 15:8 */
        u32 apl_g                                      : 8;    /* 23:16 */
        u32 apl_b                                      : 8;    /* 31:24 */
    } af;
    u32 a32;
} cabc1_apl;

/*---------------------------------------------------------------------------
	0x31240020 cabc1_max_min
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 max_y                                      : 8;    /* 7:0 */
        u32 min_y                                      : 9;    /* 16:8 */
        u32 reserved2                                  :15;    /* 31:17 */
    } af;
    u32 a32;
} cabc1_max_min;

/*---------------------------------------------------------------------------
	0x31240024 cabc1_total_pixel_cnt
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 total_pixel_cnt                            :23;    /* 22:0 */
        u32 reserved1                                  : 9;    /* 31:23 */
    } af;
    u32 a32;
} cabc1_total_pixel_cnt;

/*---------------------------------------------------------------------------
	0x31240028 cabc1_hist_top5
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 hist_top1                                  : 6;    /* 5:0 */
        u32 hist_top2                                  : 6;    /* 11:6 */
        u32 hist_top3                                  : 6;    /* 17:12 */
        u32 hist_top4                                  : 6;    /* 23:18 */
        u32 hist_top5                                  : 6;    /* 29:24 */
        u32 reserved5                                  : 2;    /* 31:30 */
    } af;
    u32 a32;
} cabc1_hist_top5;

/*---------------------------------------------------------------------------
	0x3124002c cabc1_dma_ctrl
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 hist_lut_dma_start                         : 1;    /* 0 */
        u32 hist_lut_operation_mode                    : 2;    /* 2:1 */
        u32 reserved2                                  : 1;    /* 3 */
        u32 gain_lut_dam_start                         : 1;    /* 4 */
        u32 gain_lut_operation_mode                    : 1;    /* 5 */
        u32 reserved5                                  :26;    /* 31:6 */
    } af;
    u32 a32;
} cabc1_dma_ctrl;

/*---------------------------------------------------------------------------
	0x31240030 cabc1_hist_lut_baddr
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 hist_lut_dram_base_address                 :32;    /* 31:0 */
    } af;
    u32 a32;
} cabc1_hist_lut_baddr;

/*---------------------------------------------------------------------------
	0x31240034 cabc1_gain_lut_baddr
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 gain_lut_dram_base_address                 :32;    /* 31:0 */
    } af;
    u32 a32;
} cabc1_gain_lut_baddr;

/*---------------------------------------------------------------------------
	0x31240038 cabc2_size_ctrl
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 image_width_size                           :12;    /* 11:0 */
        u32 reserved1                                  : 4;    /* 15:12 */
        u32 image_height_size                          :11;    /* 26:16 */
        u32 reserved3                                  : 5;    /* 31:27 */
    } af;
    u32 a32;
} cabc2_size_ctrl;

/*---------------------------------------------------------------------------
	0x3124003c cabc2_csc_ctrl
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 r_coef                                     :10;    /* 9:0 */
        u32 g_coef                                     :10;    /* 19:10 */
        u32 b_coef                                     :10;    /* 29:20 */
        u32 reserved3                                  : 2;    /* 31:30 */
    } af;
    u32 a32;
} cabc2_csc_ctrl;

/*---------------------------------------------------------------------------
	0x31240040 cabc2_top_ctrl
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 cabc_en                                    : 1;    /* 0 */
        u32 hist_step_mode                             : 1;    /* 1 */
        u32 cabc_gain_range                            : 2;    /* 3:2 */
        u32 reserved3                                  :12;    /* 15:4 */
        u32 cabc_global_gain                           :12;    /* 27:16 */
        u32 reserved5                                  : 3;    /* 30:28 */
        u32 interrupt_mask                             : 1;    /* 31 */
    } af;
    u32 a32;
} cabc2_top_ctrl;

/*---------------------------------------------------------------------------
	0x31240044 cabc2_gain_lut_ctrl
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 cabc_lut_wen                               : 1;    /* 0 */
        u32 cabc_lut_adrs                              : 8;    /* 8:1 */
        u32 reserved2                                  : 7;    /* 15:9 */
        u32 cabc_lut_wdata                             :12;    /* 27:16 */
        u32 reserved4                                  : 4;    /* 31:28 */
    } af;
    u32 a32;
} cabc2_gain_lut_ctrl;

/*---------------------------------------------------------------------------
	0x31240048 cabc2_gain_lut_rdata
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 cabc_lut_rdata                             :12;    /* 11:0 */
        u32 reserved1                                  :20;    /* 31:12 */
    } af;
    u32 a32;
} cabc2_gain_lut_rdata;

/*---------------------------------------------------------------------------
	0x3124004c cabc2_hist_lut_adrs
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 hist_lut_adrs                              : 5;    /* 4:0 */
        u32 reserved1                                  :27;    /* 31:5 */
    } af;
    u32 a32;
} cabc2_hist_lut_adrs;

/*---------------------------------------------------------------------------
	0x31240050 cabc2_hist_lut_data
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 hist_lut_rdata                             :23;    /* 22:0 */
        u32 reserved1                                  : 9;    /* 31:23 */
    } af;
    u32 a32;
} cabc2_hist_lut_data;

/*---------------------------------------------------------------------------
	0x31240054 cabc2_apl
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 apl_y                                      : 8;    /* 7:0 */
        u32 apl_r                                      : 8;    /* 15:8 */
        u32 apl_g                                      : 8;    /* 23:16 */
        u32 apl_b                                      : 8;    /* 31:24 */
    } af;
    u32 a32;
} cabc2_apl;

/*---------------------------------------------------------------------------
	0x31240058 cabc2_max_min
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 max_y                                      : 8;    /* 7:0 */
        u32 min_y                                      : 9;    /* 16:8 */
        u32 reserved2                                  :15;    /* 31:17 */
    } af;
    u32 a32;
} cabc2_max_min;

/*---------------------------------------------------------------------------
	0x3124005c cabc2_total_pixel_cnt
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 total_pixel_cnt                            :23;    /* 22:0 */
        u32 reserved1                                  : 9;    /* 31:23 */
    } af;
    u32 a32;
} cabc2_total_pixel_cnt;

/*---------------------------------------------------------------------------
	0x31240060 cabc2_hist_top5
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 hist_top1                                  : 6;    /* 5:0 */
        u32 hist_top2                                  : 6;    /* 11:6 */
        u32 hist_top3                                  : 6;    /* 17:12 */
        u32 hist_top4                                  : 6;    /* 23:18 */
        u32 hist_top5                                  : 6;    /* 29:24 */
        u32 reserved5                                  : 2;    /* 31:30 */
    } af;
    u32 a32;
} cabc2_hist_top5;

/*---------------------------------------------------------------------------
	0x31240064 cabc2_dma_ctrl
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 hist_lut_dma_start                         : 1;    /* 0 */
        u32 hist_lut_operation_mode                    : 2;    /* 2:1 */
        u32 reserved2                                  : 1;    /* 3 */
        u32 gain_lut_dam_start                         : 1;    /* 4 */
        u32 gain_lut_operation_mode                    : 1;    /* 5 */
        u32 reserved5                                  :26;    /* 31:6 */
    } af;
    u32 a32;
} cabc2_dma_ctrl;

/*---------------------------------------------------------------------------
	0x31240068 cabc2_hist_lut_baddr
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 hist_lut_dram_base_address                 :32;    /* 31:0 */
    } af;
    u32 a32;
} cabc2_hist_lut_baddr;

/*---------------------------------------------------------------------------
	0x3124006c cabc2_gain_lut_baddr
---------------------------------------------------------------------------*/
typedef union {
    struct {
        u32 gain_lut_dram_base_address                 :32;    /* 31:0 */
    } af;
    u32 a32;
} cabc2_gain_lut_baddr;

typedef struct {
	cabc1_size_ctrl               	cabc1_size_ctrl;
	cabc1_csc_ctrl                	cabc1_csc_ctrl;
	cabc1_top_ctrl                	cabc1_top_ctrl;
	cabc1_gain_lut_ctrl           	cabc1_gain_lut_ctrl;
	cabc1_gain_lut_data           	cabc1_gain_lut_data;
	cabc1_hist_lut_adrs           	cabc1_hist_lut_adrs;
	cabc1_hist_lut_rdata          	cabc1_hist_lut_rdata;
	cabc1_apl                     	cabc1_apl;
	cabc1_max_min                 	cabc1_max_min;
	cabc1_total_pixel_cnt         	cabc1_total_pixel_cnt;
	cabc1_hist_top5               	cabc1_hist_top5;
	cabc1_dma_ctrl                	cabc1_dma_ctrl;
	cabc1_hist_lut_baddr          	cabc1_hist_lut_baddr;
	cabc1_gain_lut_baddr          	cabc1_gain_lut_baddr;
	cabc2_size_ctrl               	cabc2_size_ctrl;
	cabc2_csc_ctrl                	cabc2_csc_ctrl;
	cabc2_top_ctrl                	cabc2_top_ctrl;
	cabc2_gain_lut_ctrl           	cabc2_gain_lut_ctrl;
	cabc2_gain_lut_rdata          	cabc2_gain_lut_rdata;
	cabc2_hist_lut_adrs           	cabc2_hist_lut_adrs;
	cabc2_hist_lut_data           	cabc2_hist_lut_data;
	cabc2_apl                     	cabc2_apl;
	cabc2_max_min                 	cabc2_max_min;
	cabc2_total_pixel_cnt         	cabc2_total_pixel_cnt;
	cabc2_hist_top5               	cabc2_hist_top5;
	cabc2_dma_ctrl                	cabc2_dma_ctrl;
	cabc2_hist_lut_baddr          	cabc2_hist_lut_baddr;
	cabc2_gain_lut_baddr          	cabc2_gain_lut_baddr;
} DSS_DU_CABC_REG_T;
#endif

