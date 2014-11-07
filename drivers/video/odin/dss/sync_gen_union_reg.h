#ifndef __SYNC_GEN_UNION_REG_H
#define __SYNC_GEN_UNION_REG_H


/*------------------------------------------------------------------
	0x31270000 sync_cfg0
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 sync0_enable                           : 1; /* 0 */
		u32 sync1_enable                           : 1; /* 1 */
		u32 sync2_enable                           : 1; /* 2 */
		u32 reserved3                              : 1; /* 3 */
		u32 sync0_mode                             : 1; /* 4 */
		u32 sync1_mode                             : 1; /* 5 */
		u32 sync2_mode                             : 1; /* 6 */
		u32 reserved7                              : 1; /* 7 */
		u32 sync0_int_en                           : 1; /* 8 */
		u32 sync1_int_en                           : 1; /* 9 */
		u32 sync2_int_en                           : 1; /* 10 */
		u32 reserved11                             : 1; /* 11 */
		u32 sync0_int_src_sel                      : 1; /* 12 */
		u32 sync1_int_src_sel                      : 1; /* 13 */
		u32 sync2_int_src_sel                      : 1; /* 14 */
		u32 reserved15                             : 1; /* 15 */
		u32 sync0_disp_sync_src_sel                : 1; /* 16 */
		u32 sync1_disp_sync_src_sel                : 1; /* 17 */
		u32 sync2_disp_sync_src_sel                : 1; /* 18 */
		u32 reserved19                             :13; /* 31:19 */
	} af;
	u32 a32;
} sync_cfg0;

/*------------------------------------------------------------------
	0x31270004 sync_cfg1
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_sync_en                           : 1; /* 0 */
		u32 ovl1_sync_en                           : 1; /* 1 */
		u32 ovl2_sync_en                           : 1; /* 2 */
		u32 vid0_sync_en0                          : 1; /* 3 */
		u32 vid1_sync_en0                          : 1; /* 4 */
		u32 vid2_sync_en                           : 1; /* 5 */
		u32 gra0_sync_en                           : 1; /* 6 */
		u32 gra1_sync_en                           : 1; /* 7 */
		u32 gra2_sync_en                           : 1; /* 8 */
		u32 gscl_sync_en0                          : 1; /* 9 */
		u32 reserved10                             :22; /* 31:10 */
	} af;
	u32 a32;
} sync_cfg1;

/*------------------------------------------------------------------
	0x31270008 sync_cfg2
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 reserved0                              : 3; /* 2:0 */
		u32 vid0_sync_en1                          : 1; /* 3 */
		u32 vid1_sync_en1                          : 1; /* 4 */
		u32 reserved3                              : 4; /* 8:5 */
		u32 gscl_sync_en1                          : 1; /* 9 */
		u32 reserved5                              :22; /* 31:10 */
	} af;
	u32 a32;
} sync_cfg2;

/*------------------------------------------------------------------
	0x31270028 sync_raster_size0
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 sync0_tot_hsize                        :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 sync0_tot_vsize                        :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_raster_size0;

/*------------------------------------------------------------------
	0x3127002c sync_raster_size1
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 sync1_tot_hsize                        :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 sync1_tot_vsize                        :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_raster_size1;

/*------------------------------------------------------------------
	0x31270030 sync_raster_size2
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 sync2_tot_hsize                        :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 sync2_tot_vsize                        :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_raster_size2;

/*------------------------------------------------------------------
	0x31270038 sync_ovl0_st_dly
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_h_st_val                          :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 ovl0_v_st_val                          :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_ovl0_st_dly;

/*------------------------------------------------------------------
	0x3127003c sync_ovl1_st_dly
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_h_st_val                          :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 ovl1_v_st_val                          :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_ovl1_st_dly;

/*------------------------------------------------------------------
	0x31270040 sync_ovl2_st_dly
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_h_st_val                          :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 ovl2_v_st_val                          :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_ovl2_st_dly;

/*------------------------------------------------------------------
	0x31270044 sync_vid0_st_dly0
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vid0_h_st_val0                         :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 vid0_v_st_val0                         :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_vid0_st_dly0;

/*------------------------------------------------------------------
	0x31270048 sync_vid1_st_dly0
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vid1_h_st_val0                         :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 vid1_v_st_val0                         :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_vid1_st_dly0;

/*------------------------------------------------------------------
	0x3127004c sync_vid2_st_dly
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vid2_h_st_val                          :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 vid2_v_st_val                          :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_vid2_st_dly;

/*------------------------------------------------------------------
	0x31270050 sync_gra0_st_dly
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 gra0_h_st_val                          :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 gra0_v_st_val                          :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_gra0_st_dly;

/*------------------------------------------------------------------
	0x31270054 sync_gra1_st_dly
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 gra1_h_st_val                          :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 gra1_v_st_val                          :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_gra1_st_dly;

/*------------------------------------------------------------------
	0x31270058 sync_gra2_st_dly
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 gra2_h_st_val                          :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 gra2_v_st_val                          :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_gra2_st_dly;

/*------------------------------------------------------------------
	0x3127005c sync_gscl_st_dly0
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 gscl_h_st_val0                         :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 gscl_v_st_val0                         :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_gscl_st_dly0;

/*------------------------------------------------------------------
	0x31270060 sync_vid0_st_dly1
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vid0_h_st_val1                         :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 vid0_v_st_val1                         :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_vid0_st_dly1;

/*------------------------------------------------------------------
	0x31270064 sync_vid1_st_dly1
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vid1_h_st_val1                         :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 vid1_v_st_val1                         :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_vid1_st_dly1;

/*------------------------------------------------------------------
	0x31270068 sync_gscl_st_dly1
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 gscl_h_st_val1                         :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 gscl_v_st_val1                         :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync_gscl_st_dly1;

/*------------------------------------------------------------------
	0x3127006c sync0_act_size
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 sync0_act_hsize                        :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 sync0_act_vsize                        :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync0_act_size;

/*------------------------------------------------------------------
	0x31270070 sync1_act_size
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 sync1_act_hsize                        :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 sync1_act_vsize                        :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync1_act_size;

/*------------------------------------------------------------------
	0x31270074 sync2_act_size
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 sync2_act_hsize                        :13; /* 12:0 */
		u32 reserved1                              : 3; /* 15:13 */
		u32 sync2_act_vsize                        :13; /* 28:16 */
		u32 reserved3                              : 3; /* 31:29 */
	} af;
	u32 a32;
} sync2_act_size;

typedef struct {
	sync_cfg0                     	sync_cfg0;
	sync_cfg1                     	sync_cfg1;
	sync_cfg2                     	sync_cfg2;
	sync_raster_size0             	sync_raster_size0;
	sync_raster_size1             	sync_raster_size1;
	sync_raster_size2             	sync_raster_size2;
	sync_ovl0_st_dly              	sync_ovl0_st_dly;
	sync_ovl1_st_dly              	sync_ovl1_st_dly;
	sync_ovl2_st_dly              	sync_ovl2_st_dly;
	sync_vid0_st_dly0             	sync_vid0_st_dly0;
	sync_vid1_st_dly0             	sync_vid1_st_dly0;
	sync_vid2_st_dly              	sync_vid2_st_dly;
	sync_gra0_st_dly              	sync_gra0_st_dly;
	sync_gra1_st_dly              	sync_gra1_st_dly;
	sync_gra2_st_dly              	sync_gra2_st_dly;
	sync_gscl_st_dly0             	sync_gscl_st_dly0;
	sync_vid0_st_dly1             	sync_vid0_st_dly1;
	sync_vid1_st_dly1             	sync_vid1_st_dly1;
	sync_gscl_st_dly1             	sync_gscl_st_dly1;
	sync0_act_size                	sync0_act_size;
	sync1_act_size                	sync1_act_size;
	sync2_act_size                	sync2_act_size;
} DSS_DU_SYNC_GEN_REG_T;
#endif

