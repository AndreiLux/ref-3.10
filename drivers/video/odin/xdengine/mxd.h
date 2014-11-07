/*
 * linux/drivers/video/odin/mxd/mxd.h
 *
 * Copyright (C) 2012 LGE
 * Author: suhwa.kim
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

#ifndef __ODIN_MXD_H__
#define __ODIN_MXD_H__

#include <video/odindss.h>

#define MXD_REG_MASK(idx)    \
    (((1 << ((XD_##idx##_START) - (XD_##idx##_END) + 1)) - 1) \
     << (XD_##idx##_END))
#define MXD_REG_VAL(val, idx) ((val) << (XD_##idx##_END))
#define MXD_REG_GET(val, idx) (((val) & MXD_REG_MASK(idx)) >> (XD_##idx##_END))
#define MXD_REG_MOD(orig, val, idx) \
    (((orig) & ~MXD_REG_MASK(idx)) | MXD_REG_VAL(val, idx))

#define ODIN_MXD_MAX_WIDTH	1920
#define ODIN_MXD_MAX_HEIGHT	1080

typedef enum
{
	LEVEL_STRONG = 0,
	LEVEL_MEDIUM,
	LEVEL_WEEK
} MXD_Level;

typedef struct
{
	u32 sx;
	u32 sy;
} mXD_POSITION;

typedef struct
{
	u32 width;
	u32 height;
} mXD_SIZE;

typedef struct
{
	mXD_POSITION pos;
	mXD_SIZE size;
} mXD_RECT;

typedef enum
{
	MXD_TP_DISABLE = 0,
	MXD_TP_ENABLE
} mXd_TP_Select;

typedef enum
{
	MXD_TP_NOTMASK = 0,
	MXD_TP_MASK
} mXd_TP_MASK;



typedef enum
{
    GAIN_1024 = 0,
    GAIN_512,
    GAIN_256,
    GAIN_RANGE_MAX,
}mXD_GAINRANGE;

typedef enum
{
    HISTSTEP_32 = 0,
    HISTSTEP_64,
}mXD_HISTSTEP;
typedef enum
{
	MXD_TEST_PATTERN_DATA_H_GRADIENT_COLOR_BAR = 0,
	MXD_TEST_PATTERN_DATA_H_STATIC_COLOR_BAR,
	MXD_TEST_PATTERN_DATA_V_GRADIENT_COLOR_BAR,
	MXD_TEST_PATTERN_DATA_V_STATIC_COLOR_BAR,
	MXD_TEST_PATTERN_DATA_SOLID_DATA,
	MXD_TEST_PATTERN_DATA_MAX
} mxdtestpatternsel;

typedef enum
{
	MXD_TP_SYNC_DATA_INPUT = 0,
	MXD_TP_SYNC_DATA_ITSELF
} mxdtpsyncselect;

typedef struct
{
	mxdtestpatternsel        testpatternsel;
	mxdtpsyncselect		syncsel;
	u8			solid_y_8bit;
	u8			solid_cb_8bit;
	u8			solid_cr_8bit;
	u8			y_data_mask;
	u8			cb_data_mask;
	u8			cr_data_mask;
} mxdtestpatterndesc;


/*  NR descriptor */

typedef struct
{
	bool edge_gain_mapping_en;
}MNR_desc;

typedef struct
{
	bool 					bnrdcen;
	bool	 				       adaptivemode_en;
	bool 					bluren;


}bnrDc_desc;

typedef struct
{
	u32					bnracfiltertype;
}FilterSel;

typedef struct
{
	bool					bnr_h_en;
	bool					bnr_v_en;
	FilterSel				bnracfiltertype;

}bnrAc_desc;

typedef struct
{
	bool					deren;
	bool					dergainmappingen;
	bool 				bifen;
}der_desc;


/*  CNR descriptor  */

typedef struct
{
	bool					cben;
	bool					cren;
	bool 				lowluminance_en;
}cnr_desc;



enum odin_mxd_resource_index {
	ODIN_MXD_TOP_RES		        =0,
	ODIN_MXD_NR_RES			 =1,
	ODIN_MXD_RE_RES			 =2,
	ODIN_MXD_CE_RES			 =3,
	ODIN_MXD_MAX_RES		        =4,
};

typedef enum{
	MXD_CE_LUT_DSE = 0,
	MXD_CE_LUT_DCE,
	MXD_HIST
} mXd_ce_LUT;

typedef enum{
    MXD_INT_SECUREMODE = 0,
    MXD_INT_NONSECUREMODE,
}eMXD_INT_CPUMODE;


typedef enum
{
	MXD_SCENARIO_CASE1 = 0,
	MXD_SCENARIO_CASE2,
	MXD_SCENARIO_CASE3,
	MXD_SCENARIO_CASE4,
	MXD_SCENARIO_MAX
} mXd_SCENARIO_Case;


typedef enum
{
	MXD_TARGET_NR = 0,
	MXD_TARGET_RE,
	MXD_TARGET_CE,
	MXD_TARGET_CTI,
	MXD_TARGET_NONE,
	MXD_TARGET_MAX
} mXd_Target;

typedef enum
{
	MXD_MEMORY_OP_AUTOMATIC = 0,
	MXD_MEMORY_OP_MANUAL
} mXd_memoryOp;

typedef enum
{
	MXD_FORMAT_YUV420_MPEG_1_2_CVI = 0,
	MXD_FORMAT_YUV422_JPEG_H,
	MXD_FORMAT_YUV422_JPEG_V,
	MXD_FORMAT_YUV444,
	MXD_FORMAT_MAX
} mXdFormat;

typedef enum
{
	MXD_CB_FIRST = 0,
	MXD_CR_FIRST = 1,
} mXdCBCR_Order;


typedef enum{
    MXD_WBPF_RGB565S = 0,
    MXD_WBPF_RGB555S = 1,
    MXD_WBPF_RGB444S = 2,
    MXD_WBPF_RGB888S = 3,
    MXD_WBPF_YUV422S = 4,
    #if 0   /* remove 12.05.23*/
    eDU_WBPF_YUV422_2P = 5,
    eDU_WBPF_YUV420_2P = 6,
    eDU_WBPF_YUV422_3P = 7,
    eDU_WBPF_YUV420_3P = 8,
    #endif
    XD_WBPF_MAX,
/*0x9-0xF reserved*/
}mXd_WBPF;

typedef enum
{
	BNR_DC_OUTPUT_NORMAL_DISPLAY = 0,
       BNR_DC_OUTPUT_BLOCK_AVERAGE,
       BNR_DC_OUTPUT_RESERVED0,
       BNR_DC_OUTPUT_RESERVED1,
       BNR_DC_OUTPUT_DC_BLUR_MAP,
       BNR_DC_OUTPUT_MAX

} bnrDcOutputMux;

typedef enum
{

    BNR_AC_READ_TYPE_STRENGTH_HV = 0,
    BNR_AC_READ_TYPE_BLOCKINESS,
    BNR_AC_READ_TYPE_MAX

} bnrAcReadType;

typedef enum
{

       BNR_AC_READ_MODE_X1000 = 0,
       BNR_AC_READ_MODE_X0500,
       BNR_AC_READ_MODE_X0250,
       BNR_AC_READ_MODE_X0125,
       BNR_AC_READ_MODE_MAX

} bnrAcReadMode;

typedef enum
{
        BNR_AC_STR_RES_10BIT = 0,
        BNR_AC_STR_RES_12BIT,
        BNR_AC_STR_RES_MAX

} bnrAcStrengthResolution;

typedef enum
{
        BNR_AC_FILTER_3TAP_AVG = 0,
        BNR_AC_FILTER_5TAP_AVG,
        BNR_AC_FILTER_MAX

} bnrAcFilterType;

typedef enum
{
        BNR_AC_OUTPUT_NORMAL_DISPLAY = 0,
        BNR_AC_OUTPUT_AVG_FILTER,
        BNR_AC_OUTPUT_INPUT_N_BLOCK_POSITION,
        BNR_AC_OUTPUT_INPUT_N_BLOCKINESS,
        BNR_AC_OUTPUT_MAX

} bnrAcOutputMux;


typedef enum
{
        BNR_AC_H_OFFSET_AUTO = 0,
        BNR_AC_H_OFFSET_NANUAL,
        BNR_AC_H_OFFSET_MAX

} bnrAcHOffsetMode;

typedef enum
{
        BNR_AC_V_OFFSET_AUTO = 0,
        BNR_AC_V_OFFSET_NORMAL,
        BNR_AC_V_OFFSET_MAX

} bnrAcVOffsetMode;

typedef enum
{
      BNR_AC_PREV_NO_HYS = 0,
      BNR_AC_PREV_1FRAME,
      BNR_AC_PREV_2FRAME,
      BNR_AC_PREV_3FRAME,
      BNR_AC_PREV_4FRAME,
      BNR_AC_PREV_5FRAME,
      BNR_AC_PREV_6FRAME,
      BNR_AC_PREV_7FRAME,
      BNR_AC_PREV_8FRAME,
      BNR_AC_PREV_9FRAME,
      BNR_AC_PREV_10FRAME,
      BNR_AC_PREV_11FRAME,
      BNR_AC_PREV_12FRAME,
      BNR_AC_PREV_13FRAME,
      BNR_AC_PREV_14FRAME,
      BNR_AC_PREV_15FRAME,
      BNR_AC_PREV_MAX

} bnrAcUseOfHysterisis;

typedef enum
{
        DER_GAIN_MAPPING_DISABLE = 0,
        DER_GAIN_MAPPING_ENABLE,
        DER_GAIN_MAPPING_MAX

} derGainMapping;

typedef enum
{
        CNR_CBCR_CTRL_SLOPE1 = 0,
        CNR_CBCR_CTRL_SLOPE2,
        CNR_CBCR_CTRL_SLOPE4,
        CNR_CBCR_CTRL_SLOPE16,
        CNR_CBCR_CTRL_SLOPE_MAX

} cnrCbcrCtrlSlope;

typedef enum
{
        CNR_CBCR_CTRL_DIST0 = 0,
        CNR_CBCR_CTRL_DIST1,
        CNR_CBCR_CTRL_DIST2,
        CNR_CBCR_CTRL_DIST3,
        CNR_CBCR_CTRL_DIST_MAX

} cnrCbcrCtrlDist;

typedef enum
{
        CNR_CBCR_CTRL_LOW_SLOPE1 = 0,
        CNR_CBCR_CTRL_LOW_SLOPE4,
        CNR_CBCR_CTRL_LOW_SLOPE16,
        CNR_CBCR_CTRL_LOW_SLOPE64,
        CNR_CBCR_CTRL_LOW_SLOPE_MAX

} cnrCbcrCtrlLowSlope;

typedef enum
{
        CE_CEN_LUT_HUE = 0,
        CE_CEN_LUT_SATURATION,
        CE_CEN_LUT_VALUE,
        CE_CEN_LUT_DEBUGCOLOR,
        CE_CEN_LUT_DELTAGAIN,
        CE_CEN_LUT_GLOBALMASTERGAIN,
        CE_CEN_LUT_GLOBALDELTAGAIN

} ceCenLUTMode;

typedef enum
{
        CE_GMC_LUT_READ = 0,
        CE_GMC_LUT_WRITE,
        CE_GMC_LUT_NORMALOP,
        CE_GMC_LUT_USED

} ceGMCLUTMode;
typedef enum
{
	CE_HISTLUT_AUTOMODE = 0,
	CE_HISTLUT_USERDEFINEMODE,
	CE_HISTLUT_APBMODE,
}ceHISTLUTMODE;

typedef struct
{
	mXd_ce_LUT lut_mode;
	bool readen;
}mXD_LUT_DATA;
static struct {

	struct platform_device *pdev;
	struct resource *iomem[ODIN_MXD_MAX_RES];
	void __iomem    *base[ODIN_MXD_MAX_RES];

	int irq;

}mxd_dev;


typedef struct {
	int nr_en;
	enum odin_dss_mXDalgorithm_level nr_level;

}mxd_noise_reduction_data;

typedef struct {
	int re_en;
	enum odin_dss_mXDalgorithm_level re_level;

}mxd_sharpness_data;


typedef struct {
	int ce_en;
	enum odin_dss_mXDalgorithm_level ce_level;

}mxd_brightness_data;

/*
typedef struct {
	int ce_en;
	enum odin_dss_mXDalgorithm_level ce_level;

}mxd_contrast_data;
*/

typedef struct {
	int ce_en;
	enum odin_dss_mXDalgorithm_level cr_level;
	enum odin_dss_mXDalgorithm_level br_level;

}mxd_contrast_data;

typedef struct {
	int ce_en;
	enum odin_dss_mXDalgorithm_level ce_level;

}mxd_filmlike_data;

typedef struct {
	int ce_en;
	enum odin_dss_mXDalgorithm_level ce_level;

}mxd_ce_data;

typedef struct {
        int resource_index;
        char *regist;
        char *field;
        int p_val;

}mxd_set_tunning_data;

typedef struct {
        int resource_index;
        char *regist;
        char *field;
        u32 p_val;

}mxd_get_tunning_data;

static inline void mxd_write_reg
    (enum odin_mxd_resource_index resource_index, u32 addr, u32 val)
{
    __raw_writel(val, mxd_dev.base[resource_index] + addr);
}

static inline u32 mxd_read_reg
    (enum odin_mxd_resource_index resource_index, u32 addr)
{
    return __raw_readl(mxd_dev.base[resource_index] + addr);
}


int 	odin_mxd_init_platform_driver(void);
void 	odin_mxd_uninit_platform_driver(void);
int     odin_mxd_adaptedness
    (enum odin_resource_index resource_index, enum odin_color_mode color_mode,
							int width, int height, bool scl_en,  int scenario);
extern int odin_mxd_enable(bool mxd_en);
extern int odin_mxd_algorithm_setup(mxd_algorithm_data *algorithm_data);
extern void odin_mxd_set_tp(bool tp_en);
extern int setup_nr(enum odin_dss_mXDalgorithm_level nr_level );
extern void nr_disable(void);
extern void re_disable(void);
extern int setup_re(enum odin_dss_mXDalgorithm_level re_level);
extern int setup_ce(enum odin_dss_mXDalgorithm_level re_level);
extern void odin_dma_default_set(void);
extern u32 odin_mxd_get_nr_image_size_width(void);
extern u32 odin_mxd_get_nr_image_size_height(void);
extern u32 odin_mxd_get_re_image_size_width(void);
extern u32 odin_mxd_get_re_image_size_height(void);


extern void odin_mxd_set_image_size( u16 nr_size_width, u16 nr_size_height,
                            u16 re_size_width, u16 re_size_height);
extern void odin_interrupt_unmask(void);
extern void odin_interrupt_mask(void);
extern void mxd_pw_reset(bool pw_flag);
extern bool get_mxd_pd_status(void);
extern bool get_mxd_clk_status(void);

extern int mxd_on_for_m2m_on(__u8 enable);




#define ODIN_XDIO_MAGIC		'M'

/* IOCTLS */
#define ODIN_MXDIOC_MXD_VERIFY	\
                              _IOWR(ODIN_XDIO_MAGIC, 100,  mxd_verify_data)
#define ODIN_MXDIOC_MXD_DETAIL_SET \
                             _IOW(ODIN_XDIO_MAGIC, 101,  mxd_algorithm_data)
#define ODIN_MXDIOC_MXD_ENA 	\
                             _IOW(ODIN_XDIO_MAGIC, 102,  mxd_verify_data)
#define ODIN_MXDIOC_MXD_NOISE_REDUCTION \
                             _IOW(ODIN_XDIO_MAGIC, 103,\
                             mxd_noise_reduction_data)
#define ODIN_MXDIOC_MXD_SHARPNESS	\
                             _IOW(ODIN_XDIO_MAGIC, 104,  mxd_sharpness_data)
#define ODIN_MXDIOC_MXD_BRIGHTNESS 	\
                             _IOW(ODIN_XDIO_MAGIC, 105,  mxd_brightness_data)

#define ODIN_MXDIOC_MXD_CONTRAST 	\
                             _IOW(ODIN_XDIO_MAGIC, 106,  mxd_contrast_data)
#define ODIN_MXDIOC_MXD_FILMLIKE 	\
                             _IOW(ODIN_XDIO_MAGIC, 107,  mxd_filmlike_data)
#define ODIN_MXDIOC_MXD_SET_TUNNING 	\
                             _IOW(ODIN_XDIO_MAGIC, 108,  mxd_set_tunning_data)
#define ODIN_MXDIOC_MXD_GET_TUNNING 	\
                             _IOW(ODIN_XDIO_MAGIC, 109,  mxd_get_tunning_data)

#define ODIN_MXDIOC_MXD_WHITE_BALANCE	\
                             _IOW(ODIN_XDIO_MAGIC, 110,  mxd_ce_data)
#define ODIN_MXDIOC_MXD_COLOR_ENHANCE	\
                             _IOW(ODIN_XDIO_MAGIC, 111,  mxd_ce_data)

#define ODIN_MXDIOC_MXD_DISABLE	\
                             _IOW(ODIN_XDIO_MAGIC, 112,  mxd_verify_data)

#define ODIN_MXDIOC_MXD_DMB_SET_TUNNING	\
                             _IOW(ODIN_XDIO_MAGIC, 113,  mxd_get_tunning_data)



#endif	/* __ODIN_MXD_H__*/
