#ifndef __OVL_UNION_REG_H
#define __OVL_UNION_REG_H


/*----------------------------------------------------------------
	0x31230000 ovl0_cfg0
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_en                              : 1; /* 0 */
		u32 ovl0_layer_op                        : 1; /* 1 */
		u32 ovl0_dst_sel                         : 1; /* 2 */
		u32 ovl0_dual_op_en                      : 1; /* 3 */
		u32 ovl0_layer0_rop                      : 4; /* 7:4 */
		u32 ovl0_layer0_cop                      : 4; /* 11:8 */
		u32 ovl0_layer1_cop                      : 4; /* 15:12 */
		u32 ovl0_layer2_cop                      : 4; /* 19:16 */
		u32 ovl0_layer3_cop                      : 4; /* 23:20 */
		u32 ovl0_layer4_cop                      : 4; /* 27:24 */
		u32 ovl0_layer5_cop                      : 4; /* 31:28 */
	} af;
	u32 a32;
} ovl0_cfg0;

/*----------------------------------------------------------------
	0x31230004 ovl0_sizexy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_sizex                           :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl0_sizey                           :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl0_sizexy;

/*----------------------------------------------------------------
	0x31230008 ovl0_pattern_data
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_bg_pat_bdata                    : 8; /* 7:0 */
		u32 ovl0_bg_pat_gdata                    : 8; /* 15:8 */
		u32 ovl0_bg_pat_rdata                    : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_pattern_data;

/*----------------------------------------------------------------
	0x3123000c ovl0_cfg1
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer0_chromakey_en             : 1; /* 0 */
		u32 ovl0_layer1_chromakey_en             : 1; /* 1 */
		u32 ovl0_layer2_chromakey_en             : 1; /* 2 */
		u32 ovl0_layer3_chromakey_en             : 1; /* 3 */
		u32 ovl0_layer4_chromakey_en             : 1; /* 4 */
		u32 ovl0_layer5_chromakey_en             : 1; /* 5 */
		u32 reserved6                            : 2; /* 7:6 */
		u32 ovl0_layer0_dst_pre_mul_en           : 1; /* 8 */
		u32 ovl0_layer0_src_pre_mul_en           : 1; /* 9 */
		u32 ovl0_layer1_dst_pre_mul_en           : 1; /* 10 */
		u32 ovl0_layer1_src_pre_mul_en           : 1; /* 11 */
		u32 ovl0_layer2_dst_pre_mul_en           : 1; /* 12 */
		u32 ovl0_layer2_src_pre_mul_en           : 1; /* 13 */
		u32 ovl0_layer3_dst_pre_mul_en           : 1; /* 14 */
		u32 ovl0_layer3_src_pre_mul_en           : 1; /* 15 */
		u32 ovl0_layer4_dst_pre_mul_en           : 1; /* 16 */
		u32 ovl0_layer4_src_pre_mul_en           : 1; /* 17 */
		u32 ovl0_layer5_dst_pre_mul_en           : 1; /* 18 */
		u32 ovl0_layer5_src_pre_mul_en           : 1; /* 19 */
		u32 reserved19                           :12; /* 31:20 */
	} af;
	u32 a32;
} ovl0_cfg1;

/*----------------------------------------------------------------
	0x31230010 ovl0_l0_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer0_chroma_bdata             : 8; /* 7:0 */
		u32 ovl0_layer0_chroma_gdata             : 8; /* 15:8 */
		u32 ovl0_layer0_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_l0_ckey_value;

/*----------------------------------------------------------------
	0x31230014 ovl0_l0_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer0_chroma_bmask             : 8; /* 7:0 */
		u32 ovl0_layer0_chroma_gmask             : 8; /* 15:8 */
		u32 ovl0_layer0_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_l0_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230018 ovl0_l1_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer1_chroma_bdata             : 8; /* 7:0 */
		u32 ovl0_layer1_chroma_gdata             : 8; /* 15:8 */
		u32 ovl0_layer1_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_l1_ckey_value;

/*----------------------------------------------------------------
	0x3123001c ovl0_l1_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer1_chroma_bmask             : 8; /* 7:0 */
		u32 ovl0_layer1_chroma_gmask             : 8; /* 15:8 */
		u32 ovl0_layer1_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_l1_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230020 ovl0_l2_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer2_chroma_bdata             : 8; /* 7:0 */
		u32 ovl0_layer2_chroma_gdata             : 8; /* 15:8 */
		u32 ovl0_layer2_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_l2_ckey_value;

/*----------------------------------------------------------------
	0x31230024 ovl0_l2_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer2_chroma_bmask             : 8; /* 7:0 */
		u32 ovl0_layer2_chroma_gmask             : 8; /* 15:8 */
		u32 ovl0_layer2_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_l2_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230028 ovl0_l3_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer3_chroma_bdata             : 8; /* 7:0 */
		u32 ovl0_layer3_chroma_gdata             : 8; /* 15:8 */
		u32 ovl0_layer3_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_l3_ckey_value;

/*----------------------------------------------------------------
	0x3123002c ovl0_l3_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer3_chroma_bmask             : 8; /* 7:0 */
		u32 ovl0_layer3_chroma_gmask             : 8; /* 15:8 */
		u32 ovl0_layer3_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_l3_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230030 ovl0_l4_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer4_chroma_bdata             : 8; /* 7:0 */
		u32 ovl0_layer4_chroma_gdata             : 8; /* 15:8 */
		u32 ovl0_layer4_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_l4_ckey_value;

/*----------------------------------------------------------------
	0x31230034 ovl0_l4_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer4_chroma_bmask             : 8; /* 7:0 */
		u32 ovl0_layer4_chroma_gmask             : 8; /* 15:8 */
		u32 ovl0_layer4_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_l4_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230038 ovl0_l5_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer5_chroma_bdata             : 8; /* 7:0 */
		u32 ovl0_layer5_chroma_gdata             : 8; /* 15:8 */
		u32 ovl0_layer5_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_l5_ckey_value;

/*----------------------------------------------------------------
	0x3123003c ovl0_l5_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_layer5_chroma_bmask             : 8; /* 7:0 */
		u32 ovl0_layer5_chroma_gmask             : 8; /* 15:8 */
		u32 ovl0_layer5_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_l5_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230040 ovl0_cfg2
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_src0_sel                        : 3; /* 2:0 */
		u32 ovl0_src1_sel                        : 3; /* 5:3 */
		u32 ovl0_src2_sel                        : 3; /* 8:6 */
		u32 ovl0_src3_sel                        : 3; /* 11:9 */
		u32 ovl0_src4_sel                        : 3; /* 14:12 */
		u32 ovl0_src5_sel                        : 3; /* 17:15 */
		u32 ovl0_src6_sel                        : 3; /* 20:18 */
		u32 reserved7                            : 3; /* 23:21 */
		u32 ovl0_src0_alpha_reg_en               : 1; /* 24 */
		u32 ovl0_src1_alpha_reg_en               : 1; /* 25 */
		u32 ovl0_src2_alpha_reg_en               : 1; /* 26 */
		u32 ovl0_src3_alpha_reg_en               : 1; /* 27 */
		u32 ovl0_src4_alpha_reg_en               : 1; /* 28 */
		u32 ovl0_src5_alpha_reg_en               : 1; /* 29 */
		u32 ovl0_src6_alpha_reg_en               : 1; /* 30 */
		u32 reserved15                           : 1; /* 31 */
	} af;
	u32 a32;
} ovl0_cfg2;

/*----------------------------------------------------------------
	0x31230044 ovl0_alpha_reg0
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_src0_alpha                      : 8; /* 7:0 */
		u32 ovl0_src1_alpha                      : 8; /* 15:8 */
		u32 ovl0_src2_alpha                      : 8; /* 23:16 */
		u32 ovl0_src3_alpha                      : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_alpha_reg0;

/*----------------------------------------------------------------
	0x31230048 ovl0_alpha_reg1
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_src4_alpha                      : 8; /* 7:0 */
		u32 ovl0_src5_alpha                      : 8; /* 15:8 */
		u32 ovl0_src6_alpha                      : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl0_alpha_reg1;

/*----------------------------------------------------------------
	0x3123004c ovl0_cfg3
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_src2_mux_sel                    : 1; /* 0 */
		u32 reserved1                            :31; /* 31:1 */
	} af;
	u32 a32;
} ovl0_cfg3;

/*----------------------------------------------------------------
	0x31230100 ovl1_cfg0
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_en                              : 1; /* 0 */
		u32 ovl1_layer_op                        : 1; /* 1 */
		u32 ovl1_dst_sel                         : 1; /* 2 */
		u32 ovl1_dual_op_en                      : 1; /* 3 */
		u32 ovl1_layer0_rop                      : 4; /* 7:4 */
		u32 ovl1_layer0_cop                      : 4; /* 11:8 */
		u32 ovl1_layer1_cop                      : 4; /* 15:12 */
		u32 ovl1_layer2_cop                      : 4; /* 19:16 */
		u32 ovl1_layer3_cop                      : 4; /* 23:20 */
		u32 ovl1_layer4_cop                      : 4; /* 27:24 */
		u32 ovl1_layer5_cop                      : 4; /* 31:28 */
	} af;
	u32 a32;
} ovl1_cfg0;

/*----------------------------------------------------------------
	0x31230104 ovl1_sizexy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_sizex                           :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl1_sizey                           :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl1_sizexy;

/*----------------------------------------------------------------
	0x31230108 ovl1_pattern_data
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_bg_pat_bdata                    : 8; /* 7:0 */
		u32 ovl1_bg_pat_gdata                    : 8; /* 15:8 */
		u32 ovl1_bg_pat_rdata                    : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_pattern_data;

/*----------------------------------------------------------------
	0x3123010c ovl1_cfg1
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer0_chromakey_en             : 1; /* 0 */
		u32 ovl1_layer1_chromakey_en             : 1; /* 1 */
		u32 ovl1_layer2_chromakey_en             : 1; /* 2 */
		u32 ovl1_layer3_chromakey_en             : 1; /* 3 */
		u32 ovl1_layer4_chromakey_en             : 1; /* 4 */
		u32 ovl1_layer5_chromakey_en             : 1; /* 5 */
		u32 reserved6                            : 2; /* 7:6 */
		u32 ovl1_layer0_dst_pre_mul_en           : 1; /* 8 */
		u32 ovl1_layer0_src_pre_mul_en           : 1; /* 9 */
		u32 ovl1_layer1_dst_pre_mul_en           : 1; /* 10 */
		u32 ovl1_layer1_src_pre_mul_en           : 1; /* 11 */
		u32 ovl1_layer2_dst_pre_mul_en           : 1; /* 12 */
		u32 ovl1_layer2_src_pre_mul_en           : 1; /* 13 */
		u32 ovl1_layer3_dst_pre_mul_en           : 1; /* 14 */
		u32 ovl1_layer3_src_pre_mul_en           : 1; /* 15 */
		u32 ovl1_layer4_dst_pre_mul_en           : 1; /* 16 */
		u32 ovl1_layer4_src_pre_mul_en           : 1; /* 17 */
		u32 ovl1_layer5_dst_pre_mul_en           : 1; /* 18 */
		u32 ovl1_layer5_src_pre_mul_en           : 1; /* 19 */
		u32 reserved19                           :12; /* 31:20 */
	} af;
	u32 a32;
} ovl1_cfg1;

/*----------------------------------------------------------------
	0x31230110 ovl1_l0_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer0_chroma_bdata             : 8; /* 7:0 */
		u32 ovl1_layer0_chroma_gdata             : 8; /* 15:8 */
		u32 ovl1_layer0_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_l0_ckey_value;

/*----------------------------------------------------------------
	0x31230114 ovl1_l0_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer0_chroma_bmask             : 8; /* 7:0 */
		u32 ovl1_layer0_chroma_gmask             : 8; /* 15:8 */
		u32 ovl1_layer0_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_l0_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230118 ovl1_l1_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer1_chroma_bdata             : 8; /* 7:0 */
		u32 ovl1_layer1_chroma_gdata             : 8; /* 15:8 */
		u32 ovl1_layer1_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_l1_ckey_value;

/*----------------------------------------------------------------
	0x3123011c ovl1_l1_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer1_chroma_bmask             : 8; /* 7:0 */
		u32 ovl1_layer1_chroma_gmask             : 8; /* 15:8 */
		u32 ovl1_layer1_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_l1_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230120 ovl1_l2_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer2_chroma_bdata             : 8; /* 7:0 */
		u32 ovl1_layer2_chroma_gdata             : 8; /* 15:8 */
		u32 ovl1_layer2_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_l2_ckey_value;

/*----------------------------------------------------------------
	0x31230124 ovl1_l2_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer2_chroma_bmask             : 8; /* 7:0 */
		u32 ovl1_layer2_chroma_gmask             : 8; /* 15:8 */
		u32 ovl1_layer2_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_l2_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230128 ovl1_l3_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer3_chroma_bdata             : 8; /* 7:0 */
		u32 ovl1_layer3_chroma_gdata             : 8; /* 15:8 */
		u32 ovl1_layer3_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_l3_ckey_value;

/*----------------------------------------------------------------
	0x3123012c ovl1_l3_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer3_chroma_bmask             : 8; /* 7:0 */
		u32 ovl1_layer3_chroma_gmask             : 8; /* 15:8 */
		u32 ovl1_layer3_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_l3_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230130 ovl1_l4_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer4_chroma_bdata             : 8; /* 7:0 */
		u32 ovl1_layer4_chroma_gdata             : 8; /* 15:8 */
		u32 ovl1_layer4_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_l4_ckey_value;

/*----------------------------------------------------------------
	0x31230134 ovl1_l4_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer4_chroma_bmask             : 8; /* 7:0 */
		u32 ovl1_layer4_chroma_gmask             : 8; /* 15:8 */
		u32 ovl1_layer4_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_l4_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230138 ovl1_l5_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer5_chroma_bdata             : 8; /* 7:0 */
		u32 ovl1_layer5_chroma_gdata             : 8; /* 15:8 */
		u32 ovl1_layer5_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_l5_ckey_value;

/*----------------------------------------------------------------
	0x3123013c ovl1_l5_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_layer5_chroma_bmask             : 8; /* 7:0 */
		u32 ovl1_layer5_chroma_gmask             : 8; /* 15:8 */
		u32 ovl1_layer5_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_l5_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230140 ovl1_cfg2
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_src0_sel                        : 3; /* 2:0 */
		u32 ovl1_src1_sel                        : 3; /* 5:3 */
		u32 ovl1_src2_sel                        : 3; /* 8:6 */
		u32 ovl1_src3_sel                        : 3; /* 11:9 */
		u32 ovl1_src4_sel                        : 3; /* 14:12 */
		u32 ovl1_src5_sel                        : 3; /* 17:15 */
		u32 ovl1_src6_sel                        : 3; /* 20:18 */
		u32 reserved7                            : 3; /* 23:21 */
		u32 ovl1_src0_alpha_reg_en               : 1; /* 24 */
		u32 ovl1_src1_alpha_reg_en               : 1; /* 25 */
		u32 ovl1_src2_alpha_reg_en               : 1; /* 26 */
		u32 ovl1_src3_alpha_reg_en               : 1; /* 27 */
		u32 ovl1_src4_alpha_reg_en               : 1; /* 28 */
		u32 ovl1_src5_alpha_reg_en               : 1; /* 29 */
		u32 ovl1_src6_alpha_reg_en               : 1; /* 30 */
		u32 reserved15                           : 1; /* 31 */
	} af;
	u32 a32;
} ovl1_cfg2;

/*----------------------------------------------------------------
	0x31230144 ovl1_alpha_reg0
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_src0_alpha                      : 8; /* 7:0 */
		u32 ovl1_src1_alpha                      : 8; /* 15:8 */
		u32 ovl1_src2_alpha                      : 8; /* 23:16 */
		u32 ovl1_src3_alpha                      : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_alpha_reg0;

/*----------------------------------------------------------------
	0x31230148 ovl1_alpha_reg1
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_src4_alpha                      : 8; /* 7:0 */
		u32 ovl1_src5_alpha                      : 8; /* 15:8 */
		u32 ovl1_src6_alpha                      : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl1_alpha_reg1;

/*----------------------------------------------------------------
	0x3123014c ovl1_cfg3
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_src2_mux_sel                    : 1; /* 0 */
		u32 reserved1                            :31; /* 31:1 */
	} af;
	u32 a32;
} ovl1_cfg3;

/*----------------------------------------------------------------
	0x31230200 ovl2_cfg0
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_en                              : 1; /* 0 */
		u32 ovl2_layer_op                        : 1; /* 1 */
		u32 ovl2_dst_sel                         : 1; /* 2 */
		u32 ovl2_dual_op_en                      : 1; /* 3 */
		u32 ovl2_layer0_rop                      : 4; /* 7:4 */
		u32 ovl2_layer0_cop                      : 4; /* 11:8 */
		u32 ovl2_layer1_cop                      : 4; /* 15:12 */
		u32 ovl2_layer2_cop                      : 4; /* 19:16 */
		u32 ovl2_layer3_cop                      : 4; /* 23:20 */
		u32 ovl2_layer4_cop                      : 4; /* 27:24 */
		u32 ovl2_layer5_cop                      : 4; /* 31:28 */
	} af;
	u32 a32;
} ovl2_cfg0;

/*----------------------------------------------------------------
	0x31230204 ovl2_sizexy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_sizex                           :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl2_sizey                           :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl2_sizexy;

/*----------------------------------------------------------------
	0x31230208 ovl2_pattern_data
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_bg_pat_bdata                    : 8; /* 7:0 */
		u32 ovl2_bg_pat_gdata                    : 8; /* 15:8 */
		u32 ovl2_bg_pat_rdata                    : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_pattern_data;

/*----------------------------------------------------------------
	0x3123020c ovl2_cfg1
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer0_chromakey_en             : 1; /* 0 */
		u32 ovl2_layer1_chromakey_en             : 1; /* 1 */
		u32 ovl2_layer2_chromakey_en             : 1; /* 2 */
		u32 ovl2_layer3_chromakey_en             : 1; /* 3 */
		u32 ovl2_layer4_chromakey_en             : 1; /* 4 */
		u32 ovl2_layer5_chromakey_en             : 1; /* 5 */
		u32 reserved6                            : 2; /* 7:6 */
		u32 ovl2_layer0_dst_pre_mul_en           : 1; /* 8 */
		u32 ovl2_layer0_src_pre_mul_en           : 1; /* 9 */
		u32 ovl2_layer1_dst_pre_mul_en           : 1; /* 10 */
		u32 ovl2_layer1_src_pre_mul_en           : 1; /* 11 */
		u32 ovl2_layer2_dst_pre_mul_en           : 1; /* 12 */
		u32 ovl2_layer2_src_pre_mul_en           : 1; /* 13 */
		u32 ovl2_layer3_dst_pre_mul_en           : 1; /* 14 */
		u32 ovl2_layer3_src_pre_mul_en           : 1; /* 15 */
		u32 ovl2_layer4_dst_pre_mul_en           : 1; /* 16 */
		u32 ovl2_layer4_src_pre_mul_en           : 1; /* 17 */
		u32 ovl2_layer5_dst_pre_mul_en           : 1; /* 18 */
		u32 ovl2_layer5_src_pre_mul_en           : 1; /* 19 */
		u32 reserved19                           :12; /* 31:20 */
	} af;
	u32 a32;
} ovl2_cfg1;

/*----------------------------------------------------------------
	0x31230210 ovl2_l0_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer0_chroma_bdata             : 8; /* 7:0 */
		u32 ovl2_layer0_chroma_gdata             : 8; /* 15:8 */
		u32 ovl2_layer0_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_l0_ckey_value;

/*----------------------------------------------------------------
	0x31230214 ovl2_l0_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer0_chroma_bmask             : 8; /* 7:0 */
		u32 ovl2_layer0_chroma_gmask             : 8; /* 15:8 */
		u32 ovl2_layer0_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_l0_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230218 ovl2_l1_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer1_chroma_bdata             : 8; /* 7:0 */
		u32 ovl2_layer1_chroma_gdata             : 8; /* 15:8 */
		u32 ovl2_layer1_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_l1_ckey_value;

/*----------------------------------------------------------------
	0x3123021c ovl2_l1_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer1_chroma_bmask             : 8; /* 7:0 */
		u32 ovl2_layer1_chroma_gmask             : 8; /* 15:8 */
		u32 ovl2_layer1_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_l1_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230220 ovl2_l2_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer2_chroma_bdata             : 8; /* 7:0 */
		u32 ovl2_layer2_chroma_gdata             : 8; /* 15:8 */
		u32 ovl2_layer2_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_l2_ckey_value;

/*----------------------------------------------------------------
	0x31230224 ovl2_l2_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer2_chroma_bmask             : 8; /* 7:0 */
		u32 ovl2_layer2_chroma_gmask             : 8; /* 15:8 */
		u32 ovl2_layer2_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_l2_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230228 ovl2_l3_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer3_chroma_bdata             : 8; /* 7:0 */
		u32 ovl2_layer3_chroma_gdata             : 8; /* 15:8 */
		u32 ovl2_layer3_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_l3_ckey_value;

/*----------------------------------------------------------------
	0x3123022c ovl2_l3_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer3_chroma_bmask             : 8; /* 7:0 */
		u32 ovl2_layer3_chroma_gmask             : 8; /* 15:8 */
		u32 ovl2_layer3_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_l3_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230230 ovl2_l4_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer4_chroma_bdata             : 8; /* 7:0 */
		u32 ovl2_layer4_chroma_gdata             : 8; /* 15:8 */
		u32 ovl2_layer4_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_l4_ckey_value;

/*----------------------------------------------------------------
	0x31230234 ovl2_l4_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer4_chroma_bmask             : 8; /* 7:0 */
		u32 ovl2_layer4_chroma_gmask             : 8; /* 15:8 */
		u32 ovl2_layer4_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_l4_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230238 ovl2_l5_ckey_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer5_chroma_bdata             : 8; /* 7:0 */
		u32 ovl2_layer5_chroma_gdata             : 8; /* 15:8 */
		u32 ovl2_layer5_chroma_rdata             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_l5_ckey_value;

/*----------------------------------------------------------------
	0x3123023c ovl2_l5_ckey_mask_value
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_layer5_chroma_bmask             : 8; /* 7:0 */
		u32 ovl2_layer5_chroma_gmask             : 8; /* 15:8 */
		u32 ovl2_layer5_chroma_rmask             : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_l5_ckey_mask_value;

/*----------------------------------------------------------------
	0x31230240 ovl2_cfg2
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_src0_sel                        : 3; /* 2:0 */
		u32 ovl2_src1_sel                        : 3; /* 5:3 */
		u32 ovl2_src2_sel                        : 3; /* 8:6 */
		u32 ovl2_src3_sel                        : 3; /* 11:9 */
		u32 ovl2_src4_sel                        : 3; /* 14:12 */
		u32 ovl2_src5_sel                        : 3; /* 17:15 */
		u32 ovl2_src6_sel                        : 3; /* 20:18 */
		u32 reserved7                            : 3; /* 23:21 */
		u32 ovl2_src0_alpha_reg_en               : 1; /* 24 */
		u32 ovl2_src1_alpha_reg_en               : 1; /* 25 */
		u32 ovl2_src2_alpha_reg_en               : 1; /* 26 */
		u32 ovl2_src3_alpha_reg_en               : 1; /* 27 */
		u32 ovl2_src4_alpha_reg_en               : 1; /* 28 */
		u32 ovl2_src5_alpha_reg_en               : 1; /* 29 */
		u32 ovl2_src6_alpha_reg_en               : 1; /* 30 */
		u32 reserved15                           : 1; /* 31 */
	} af;
	u32 a32;
} ovl2_cfg2;

/*----------------------------------------------------------------
	0x31230244 ovl2_alpha_reg0
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_src0_alpha                      : 8; /* 7:0 */
		u32 ovl2_src1_alpha                      : 8; /* 15:8 */
		u32 ovl2_src2_alpha                      : 8; /* 23:16 */
		u32 ovl2_src3_alpha                      : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_alpha_reg0;

/*----------------------------------------------------------------
	0x31230248 ovl2_alpha_reg1
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_src4_alpha                      : 8; /* 7:0 */
		u32 ovl2_src5_alpha                      : 8; /* 15:8 */
		u32 ovl2_src6_alpha                      : 8; /* 23:16 */
		u32 reserved3                            : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl2_alpha_reg1;

/*----------------------------------------------------------------
	0x3123024c ovl2_cfg3
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_src2_mux_sel                    : 1; /* 0 */
		u32 reserved1                            :31; /* 31:1 */
	} af;
	u32 a32;
} ovl2_cfg3;

/*----------------------------------------------------------------
	0x31230310 ovl_cfg_g0
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_src0_en                         : 1; /* 0 */
		u32 ovl0_src1_en                         : 1; /* 1 */
		u32 ovl0_src2_en                         : 1; /* 2 */
		u32 ovl0_src3_en                         : 1; /* 3 */
		u32 ovl0_src4_en                         : 1; /* 4 */
		u32 ovl0_src5_en                         : 1; /* 5 */
		u32 ovl0_src6_en                         : 1; /* 6 */
		u32 reserved7                            : 1; /* 7 */
		u32 ovl1_src0_en                         : 1; /* 8 */
		u32 ovl1_src1_en                         : 1; /* 9 */
		u32 ovl1_src2_en                         : 1; /* 10 */
		u32 ovl1_src3_en                         : 1; /* 11 */
		u32 ovl1_src4_en                         : 1; /* 12 */
		u32 ovl1_src5_en                         : 1; /* 13 */
		u32 ovl1_src6_en                         : 1; /* 14 */
		u32 reserved15                           : 1; /* 15 */
		u32 ovl2_src0_en                         : 1; /* 16 */
		u32 ovl2_src1_en                         : 1; /* 17 */
		u32 ovl2_src2_en                         : 1; /* 18 */
		u32 ovl2_src3_en                         : 1; /* 19 */
		u32 ovl2_src4_en                         : 1; /* 20 */
		u32 ovl2_src5_en                         : 1; /* 21 */
		u32 ovl2_src6_en                         : 1; /* 22 */
		u32 reserved23                           : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl_cfg_g0;

/*----------------------------------------------------------------
	0x31230318 ovl_s0_pip_startxy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src0_pip_sx                      :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src0_pip_sy                      :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s0_pip_startxy;

/*----------------------------------------------------------------
	0x3123031c ovl_s0_pip_sizexy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src0_pip_sizex                   :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src0_pip_sizey                   :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s0_pip_sizexy;

/*----------------------------------------------------------------
	0x31230320 ovl_s1_pip_startxy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src1_pip_sx                      :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src1_pip_sy                      :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s1_pip_startxy;

/*----------------------------------------------------------------
	0x31230324 ovl_s1_pip_sizexy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src1_pip_sizex                   :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src1_pip_sizey                   :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s1_pip_sizexy;

/*----------------------------------------------------------------
	0x31230328 ovl_s2_pip_startxy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src2_pip_sx                      :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src2_pip_sy                      :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s2_pip_startxy;

/*----------------------------------------------------------------
	0x3123032c ovl_s2_pip_sizexy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src2_pip_sizex                   :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src2_pip_sizey                   :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s2_pip_sizexy;

/*----------------------------------------------------------------
	0x31230330 ovl_s3_pip_startxy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src3_pip_sx                      :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src3_pip_sy                      :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s3_pip_startxy;

/*----------------------------------------------------------------
	0x31230334 ovl_s3_pip_sizexy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src3_pip_sizex                   :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src3_pip_sizey                   :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s3_pip_sizexy;

/*----------------------------------------------------------------
	0x31230338 ovl_s4_pip_startxy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src4_pip_sx                      :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src4_pip_sy                      :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s4_pip_startxy;

/*----------------------------------------------------------------
	0x3123033c ovl_s4_pip_sizexy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src4_pip_sizex                   :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src4_pip_sizey                   :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s4_pip_sizexy;

/*----------------------------------------------------------------
	0x31230340 ovl_s5_pip_startxy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src5_pip_sx                      :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src5_pip_sy                      :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s5_pip_startxy;

/*----------------------------------------------------------------
	0x31230344 ovl_s5_pip_sizexy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src5_pip_sizex                   :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src5_pip_sizey                   :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s5_pip_sizexy;

/*----------------------------------------------------------------
	0x31230348 ovl_s6_pip_startxy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src6_pip_sx                      :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src6_pip_sy                      :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s6_pip_startxy;

/*----------------------------------------------------------------
	0x3123034c ovl_s6_pip_sizexy
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl_src6_pip_sizex                   :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl_src6_pip_sizey                   :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl_s6_pip_sizexy;

/*----------------------------------------------------------------
	0x31230350 ovl_sw_update
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_reg_sw_update                   : 1; /* 0 */
		u32 ovl1_reg_sw_update                   : 1; /* 1 */
		u32 ovl2_reg_sw_update                   : 1; /* 2 */
		u32 reserved3                            :13; /* 15:3 */
		u32 ovl0_save_pip_srr                    : 1; /* 16 */
		u32 ovl1_save_pip_srr                    : 1; /* 17 */
		u32 ovl2_save_pip_srr                    : 1; /* 18 */
		u32 reserved7                            :13; /* 31:19 */
	} af;
	u32 a32;
} ovl_sw_update;

/*----------------------------------------------------------------
	0x31230354 ovl0_status
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_s0_ififo_empty                  : 1; /* 0 */
		u32 ovl0_s1_ififo_empty                  : 1; /* 1 */
		u32 ovl0_s2_ififo_empty                  : 1; /* 2 */
		u32 ovl0_s3_ififo_empty                  : 1; /* 3 */
		u32 ovl0_s4_ififo_empty                  : 1; /* 4 */
		u32 ovl0_s5_ififo_empty                  : 1; /* 5 */
		u32 ovl0_s6_ififo_empty                  : 1; /* 6 */
		u32 reserved7                            : 1; /* 7 */
		u32 ovl0_s0_ififo_full                   : 1; /* 8 */
		u32 ovl0_s1_ififo_full                   : 1; /* 9 */
		u32 ovl0_s2_ififo_full                   : 1; /* 10 */
		u32 ovl0_s3_ififo_full                   : 1; /* 11 */
		u32 ovl0_s4_ififo_full                   : 1; /* 12 */
		u32 ovl0_s5_ififo_full                   : 1; /* 13 */
		u32 ovl0_s6_ififo_full                   : 1; /* 14 */
		u32 reserved15                           : 1; /* 15 */
		u32 ovl0_con_st                          : 1; /* 16 */
		u32 reserved17                           :15; /* 31:17 */
	} af;
	u32 a32;
} ovl0_status;

/*----------------------------------------------------------------
	0x31230358 ovl1_status
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl1_s0_ififo_empty                  : 1; /* 0 */
		u32 ovl1_s1_ififo_empty                  : 1; /* 1 */
		u32 ovl1_s2_ififo_empty                  : 1; /* 2 */
		u32 ovl1_s3_ififo_empty                  : 1; /* 3 */
		u32 ovl1_s4_ififo_empty                  : 1; /* 4 */
		u32 ovl1_s5_ififo_empty                  : 1; /* 5 */
		u32 ovl1_s6_ififo_empty                  : 1; /* 6 */
		u32 reserved7                            : 1; /* 7 */
		u32 ovl1_s0_ififo_full                   : 1; /* 8 */
		u32 ovl1_s1_ififo_full                   : 1; /* 9 */
		u32 ovl1_s2_ififo_full                   : 1; /* 10 */
		u32 ovl1_s3_ififo_full                   : 1; /* 11 */
		u32 ovl1_s4_ififo_full                   : 1; /* 12 */
		u32 ovl1_s5_ififo_full                   : 1; /* 13 */
		u32 ovl1_s6_ififo_full                   : 1; /* 14 */
		u32 reserved15                           : 1; /* 15 */
		u32 ovl1_con_st                          : 1; /* 16 */
		u32 reserved17                           :29; /* 31:3 */
	} af;
	u32 a32;
} ovl1_status;

/*----------------------------------------------------------------
	0x3123035c ovl2_status
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl2_s0_ififo_empty                  : 1; /* 0 */
		u32 ovl2_s1_ififo_empty                  : 1; /* 1 */
		u32 ovl2_s2_ififo_empty                  : 1; /* 2 */
		u32 ovl2_s3_ififo_empty                  : 1; /* 3 */
		u32 ovl2_s4_ififo_empty                  : 1; /* 4 */
		u32 ovl2_s5_ififo_empty                  : 1; /* 5 */
		u32 ovl2_s6_ififo_empty                  : 1; /* 6 */
		u32 reserved7                            : 1; /* 7 */
		u32 ovl2_s0_ififo_full                   : 1; /* 8 */
		u32 ovl2_s1_ififo_full                   : 1; /* 9 */
		u32 ovl2_s2_ififo_full                   : 1; /* 10 */
		u32 ovl2_s3_ififo_full                   : 1; /* 11 */
		u32 ovl2_s4_ififo_full                   : 1; /* 12 */
		u32 ovl2_s5_ififo_full                   : 1; /* 13 */
		u32 ovl2_s6_ififo_full                   : 1; /* 14 */
		u32 reserved15                           : 1; /* 15 */
		u32 ovl2_con_st                          : 1; /* 16 */
		u32 reserved17                           :29; /* 31:3 */
	} af;
	u32 a32;
} ovl2_status;

/*----------------------------------------------------------------
	0x31230364 ovl0_save_pip_pos
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_save_pip_posx                   :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl0_save_pip_posy                   :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl0_save_pip_pos;

/*----------------------------------------------------------------
	0x31230368 ovl1_save_pip_pos
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_save_pip_posx                   :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl0_save_pip_posy                   :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl1_save_pip_pos;

/*----------------------------------------------------------------
	0x3123036c ovl2_save_pip_pos
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_save_pip_posx                   :13; /* 12:0 */
		u32 reserved1                            : 3; /* 15:13 */
		u32 ovl0_save_pip_posy                   :13; /* 28:16 */
		u32 reserved3                            : 3; /* 31:29 */
	} af;
	u32 a32;
} ovl2_save_pip_pos;

/*----------------------------------------------------------------
	0x31230398 ovl_se_src_secure_enable
----------------------------------------------------------------*/
typedef union {
	struct {
		u32 ovl0_se_src0_en                      : 1; /* 0 */
		u32 ovl0_se_src1_en                      : 1; /* 1 */
		u32 ovl0_se_src2_en                      : 1; /* 2 */
		u32 ovl0_se_src3_en                      : 1; /* 3 */
		u32 ovl0_se_src4_en                      : 1; /* 4 */
		u32 ovl0_se_src5_en                      : 1; /* 5 */
		u32 ovl0_se_src6_en                      : 1; /* 6 */
		u32 reserved7                            : 1; /* 7 */
		u32 ovl1_se_src0_en                      : 1; /* 8 */
		u32 ovl1_se_src1_en                      : 1; /* 9 */
		u32 ovl1_se_src2_en                      : 1; /* 10 */
		u32 ovl1_se_src3_en                      : 1; /* 11 */
		u32 ovl1_se_src4_en                      : 1; /* 12 */
		u32 ovl1_se_src5_en                      : 1; /* 13 */
		u32 ovl1_se_src6_en                      : 1; /* 14 */
		u32 reserved15                           : 1; /* 15 */
		u32 ovl2_se_src0_en                      : 1; /* 16 */
		u32 ovl2_se_src1_en                      : 1; /* 17 */
		u32 ovl2_se_src2_en                      : 1; /* 18 */
		u32 ovl2_se_src3_en                      : 1; /* 19 */
		u32 ovl2_se_src4_en                      : 1; /* 20 */
		u32 ovl2_se_src5_en                      : 1; /* 21 */
		u32 ovl2_se_src6_en                      : 1; /* 22 */
		u32 reserved23                           : 8; /* 31:24 */
		u32 ovl0_se_src0_sec_en                  : 1; /* 0 */
		u32 ovl0_se_src1_sec_en                  : 1; /* 1 */
		u32 ovl0_se_src2_sec_en                  : 1; /* 2 */
		u32 ovl0_se_src3_sec_en                  : 1; /* 3 */
		u32 ovl0_se_src4_sec_en                  : 1; /* 4 */
		u32 ovl0_se_src5_sec_en                  : 1; /* 5 */
		u32 ovl0_se_src6_sec_en                  : 1; /* 6 */
		u32 reserved31                           : 1; /* 7 */
		u32 ovl1_se_src0_sec_en                  : 1; /* 8 */
		u32 ovl1_se_src1_sec_en                  : 1; /* 9 */
		u32 ovl1_se_src2_sec_en                  : 1; /* 10 */
		u32 ovl1_se_src3_sec_en                  : 1; /* 11 */
		u32 ovl1_se_src4_sec_en                  : 1; /* 12 */
		u32 ovl1_se_src5_sec_en                  : 1; /* 13 */
		u32 ovl1_se_src6_sec_en                  : 1; /* 14 */
		u32 reserved39                           : 1; /* 15 */
		u32 ovl2_se_src0_sec_en                  : 1; /* 16 */
		u32 ovl2_se_src1_sec_en                  : 1; /* 17 */
		u32 ovl2_se_src2_sec_en                  : 1; /* 18 */
		u32 ovl2_se_src3_sec_en                  : 1; /* 19 */
		u32 ovl2_se_src4_sec_en                  : 1; /* 20 */
		u32 ovl2_se_src5_sec_en                  : 1; /* 21 */
		u32 ovl2_se_src6_sec_en                  : 1; /* 22 */
		u32 reserved47                           : 8; /* 31:24 */
		u32 ovl0_se_layer0_sec_en                : 1; /* 0 */
		u32 ovl0_se_layer1_sec_en                : 1; /* 1 */
		u32 ovl0_se_layer2_sec_en                : 1; /* 2 */
		u32 ovl0_se_layer3_sec_en                : 1; /* 3 */
		u32 ovl0_se_layer4_sec_en                : 1; /* 4 */
		u32 ovl0_se_layer5_sec_en                : 1; /* 5 */
		u32 ovl0_se_layer6_sec_en                : 1; /* 6 */
		u32 reserved55                           : 1; /* 7 */
		u32 ovl1_se_layer0_sec_en                : 1; /* 8 */
		u32 ovl1_se_layer1_sec_en                : 1; /* 9 */
		u32 ovl1_se_layer2_sec_en                : 1; /* 10 */
		u32 ovl1_se_layer3_sec_en                : 1; /* 11 */
		u32 ovl1_se_layer4_sec_en                : 1; /* 12 */
		u32 ovl1_se_layer5_sec_en                : 1; /* 13 */
		u32 ovl1_se_layer6_sec_en                : 1; /* 14 */
		u32 reserved63                           : 1; /* 15 */
		u32 ovl2_se_layer0_sec_en                : 1; /* 16 */
		u32 ovl2_se_layer1_sec_en                : 1; /* 17 */
		u32 ovl2_se_layer2_sec_en                : 1; /* 18 */
		u32 ovl2_se_layer3_sec_en                : 1; /* 19 */
		u32 ovl2_se_layer4_sec_en                : 1; /* 20 */
		u32 ovl2_se_layer5_sec_en                : 1; /* 21 */
		u32 ovl2_se_layer6_sec_en                : 1; /* 22 */
		u32 reserved71                           : 8; /* 31:24 */
	} af;
	u32 a32;
} ovl_se_src_secure_enable;

typedef struct {
	ovl0_cfg0                     	ovl0_cfg0;
	ovl0_sizexy                   	ovl0_sizexy;
	ovl0_pattern_data             	ovl0_pattern_data;
	ovl0_cfg1                     	ovl0_cfg1;
	ovl0_l0_ckey_value            	ovl0_l0_ckey_value;
	ovl0_l0_ckey_mask_value       	ovl0_l0_ckey_mask_value;
	ovl0_l1_ckey_value            	ovl0_l1_ckey_value;
	ovl0_l1_ckey_mask_value       	ovl0_l1_ckey_mask_value;
	ovl0_l2_ckey_value            	ovl0_l2_ckey_value;
	ovl0_l2_ckey_mask_value       	ovl0_l2_ckey_mask_value;
	ovl0_l3_ckey_value            	ovl0_l3_ckey_value;
	ovl0_l3_ckey_mask_value       	ovl0_l3_ckey_mask_value;
	ovl0_l4_ckey_value            	ovl0_l4_ckey_value;
	ovl0_l4_ckey_mask_value       	ovl0_l4_ckey_mask_value;
	ovl0_l5_ckey_value            	ovl0_l5_ckey_value;
	ovl0_l5_ckey_mask_value       	ovl0_l5_ckey_mask_value;
	ovl0_cfg2                     	ovl0_cfg2;
	ovl0_alpha_reg0               	ovl0_alpha_reg0;
	ovl0_alpha_reg1               	ovl0_alpha_reg1;
	ovl0_cfg3                     	ovl0_cfg3;
	ovl1_cfg0                     	ovl1_cfg0;
	ovl1_sizexy                   	ovl1_sizexy;
	ovl1_pattern_data             	ovl1_pattern_data;
	ovl1_cfg1                     	ovl1_cfg1;
	ovl1_l0_ckey_value            	ovl1_l0_ckey_value;
	ovl1_l0_ckey_mask_value       	ovl1_l0_ckey_mask_value;
	ovl1_l1_ckey_value            	ovl1_l1_ckey_value;
	ovl1_l1_ckey_mask_value       	ovl1_l1_ckey_mask_value;
	ovl1_l2_ckey_value            	ovl1_l2_ckey_value;
	ovl1_l2_ckey_mask_value       	ovl1_l2_ckey_mask_value;
	ovl1_l3_ckey_value            	ovl1_l3_ckey_value;
	ovl1_l3_ckey_mask_value       	ovl1_l3_ckey_mask_value;
	ovl1_l4_ckey_value            	ovl1_l4_ckey_value;
	ovl1_l4_ckey_mask_value       	ovl1_l4_ckey_mask_value;
	ovl1_l5_ckey_value            	ovl1_l5_ckey_value;
	ovl1_l5_ckey_mask_value       	ovl1_l5_ckey_mask_value;
	ovl1_cfg2                     	ovl1_cfg2;
	ovl1_alpha_reg0               	ovl1_alpha_reg0;
	ovl1_alpha_reg1               	ovl1_alpha_reg1;
	ovl1_cfg3                     	ovl1_cfg3;
	ovl2_cfg0                     	ovl2_cfg0;
	ovl2_sizexy                   	ovl2_sizexy;
	ovl2_pattern_data             	ovl2_pattern_data;
	ovl2_cfg1                     	ovl2_cfg1;
	ovl2_l0_ckey_value            	ovl2_l0_ckey_value;
	ovl2_l0_ckey_mask_value       	ovl2_l0_ckey_mask_value;
	ovl2_l1_ckey_value            	ovl2_l1_ckey_value;
	ovl2_l1_ckey_mask_value       	ovl2_l1_ckey_mask_value;
	ovl2_l2_ckey_value            	ovl2_l2_ckey_value;
	ovl2_l2_ckey_mask_value       	ovl2_l2_ckey_mask_value;
	ovl2_l3_ckey_value            	ovl2_l3_ckey_value;
	ovl2_l3_ckey_mask_value       	ovl2_l3_ckey_mask_value;
	ovl2_l4_ckey_value            	ovl2_l4_ckey_value;
	ovl2_l4_ckey_mask_value       	ovl2_l4_ckey_mask_value;
	ovl2_l5_ckey_value            	ovl2_l5_ckey_value;
	ovl2_l5_ckey_mask_value       	ovl2_l5_ckey_mask_value;
	ovl2_cfg2                     	ovl2_cfg2;
	ovl2_alpha_reg0               	ovl2_alpha_reg0;
	ovl2_alpha_reg1               	ovl2_alpha_reg1;
	ovl2_cfg3                     	ovl2_cfg3;
	ovl_cfg_g0                    	cfg_g0;
	ovl_s0_pip_startxy            	i_s0_pip_startxy;
	ovl_s0_pip_sizexy             	i_s0_pip_sizexy;
	ovl_s1_pip_startxy            	i_s1_pip_startxy;
	ovl_s1_pip_sizexy             	i_s1_pip_sizexy;
	ovl_s2_pip_startxy            	i_s2_pip_startxy;
	ovl_s2_pip_sizexy             	i_s2_pip_sizexy;
	ovl_s3_pip_startxy            	i_s3_pip_startxy;
	ovl_s3_pip_sizexy             	i_s3_pip_sizexy;
	ovl_s4_pip_startxy            	i_s4_pip_startxy;
	ovl_s4_pip_sizexy             	i_s4_pip_sizexy;
	ovl_s5_pip_startxy            	i_s5_pip_startxy;
	ovl_s5_pip_sizexy             	i_s5_pip_sizexy;
	ovl_s6_pip_startxy            	i_s6_pip_startxy;
	ovl_s6_pip_sizexy             	i_s6_pip_sizexy;
	ovl_sw_update                 	sw_update;
	ovl0_status                   	ovl0_status;
	ovl1_status                   	ovl1_status;
	ovl2_status                   	ovl2_status;
	ovl0_save_pip_pos             	ovl0_save_pip_pos;
	ovl1_save_pip_pos             	ovl1_save_pip_pos;
	ovl2_save_pip_pos             	ovl2_save_pip_pos;
	ovl_se_src_secure_enable      	se_src_secure_enable;
} DSS_DU_OVL_REG_T;
#endif

