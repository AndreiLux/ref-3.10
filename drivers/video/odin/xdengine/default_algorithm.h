/*
 * linux/drivers/video/odin/xdengine/default_algorithm.h
 *
 * Copyright (C) 2012 LG Electronics, Inc.
 * Author: suhwa.kim
 *
 * Some code and ideas taken from drivers/video/omap/ driver
 * by Imre Deak.
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




#ifndef ENABLE
#define ENABLE (1)
#endif

#ifndef DISABLE
#define DISABLE (0)
#endif


/* 	TOP default algorithm	*/
#define TOP_REG_UPDATE_MODE		(XD_TOP_REG_UPDATE_ANYTIME)

#define TOP_LUT_DMASTART_USERMODE		(0x0)
#define TOP_LUT_DMASTART_APB				(0x1)
#define TOP_LUT_DMASTART_WRITE			(0x0)

#define TOP_HIST_LUT_AUTO					(0x0)
#define TOP_HIST_LUT_USERMODE			(0x1)
#define TOP_HIST_LUT_APB					(0x2)


#define TP_TEST_IMAGE		XD_TEST_PATTERN_DATA_H_GRADIENT_COLOR_BAR
#define TP_SYNC_SEL			MXD_MADE_BY_ITSELF
#define TP_SOLID_Y_8BIT		0xFF
#define TP_SOLID_CB_8BIT		0x00
#define TP_SOLID_CR_8BIT		0x95
#define TP_Y_DATA_MASK		MXD_TP_NOTMASK
#define TP_CB_DATA_MASK		MXD_TP_NOTMASK
#define TP_CR_DATA_MASK		MXD_TP_NOTMASK
/*  	level		*/
#define LEVEL_STRONG		(0)
#define LEVEL_MEDIUM		(1)
#define LEVEL_WEEK			(2)


/************/
/*  NR		*/
/************/

/* 	NR default algorithm	*/
#define NR_VFILTER			(DISABLE)

mXD_POSITION nr_active_window_xy[2] = {{0x0, 0x0}, {0x90, 0xB0}};

mXD_POSITION nr_active_window_for_ac_bnr_xy[2] = {{0x0, 0x90}, {0x90, 0xB0}};


u8 mnr_h_coef[17] = {0x0, 0x0, 0x4, 0x6, 0x6, 0x6, 0x6, 0x7,
					0x8, 0x7, 0x6, 0x6, 0x6, 0x6, 0x4, 0x0, 0x0};

u8 mnr_v_coef[7] = {0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8};

mXD_POSITION 	mnr_gain_coef_xy[4]
						= {{0x8, 0x0}, {0xC, 0x2}, {0x12, 0x5}, {0x20, 0xFF}};



#define MNR_FILTER_THRESHOLD			(0x18)
#define MNR_FILTER_THRESHOLD_WEEK	(0x10)
#define MNR_FILTER_THRESHOLD_MEDIUM	(0x18)
#define MNR_FILTER_THRESHOLD_STRONG	(0x20)



#define MNR_EDGE_GAIN_MAPPING		(ENABLE)
#define BNR_DC_ADAPTIVE_MODE		(bnr_dc_adapt_on)
#define BNR_DC_ADAPTIVE_MODE_EN	(ENABLE)

#define BNR_DC_OUTPUT_MUX		(BNR_DC_OUTPUT_NORMAL_DISPLAY)

#define BNR_DC_BLUR_ENABLE		(0x2)
#define BNR_DC_BLUR_ENABLE_DEBUG	(0x3)
#define BNR_DC_BLUR_BYPASS		(0x0)
#define BNR_DC_MUL2BLK				(0x04)
#define BNR_DC_MUL2BLK_WEEK		(0x02)
#define BNR_DC_GAIN_CTRL_EN		(ENABLE)
#define BNR_DC_GAIN_CTRL_Y2		(0x40)
#define BNR_DC_GAIN_CTRL_X2		(0x80)
#define BNR_DC_GAIN_CTRL_Y3		(0xFF)
#define BNR_DC_GAIN_CTRL_X3		(0x80)
#define BNR_DC_GAIN_CTRL_Y0		(0x00)
#define BNR_DC_GAIN_CTRL_X0		(0x20)
#define BNR_DC_GAIN_CTRL_Y1		(0x20)
#define BNR_DC_GAIN_CTRL_X1		(0x50)
#define BNR_DC_GAIN_CTRL_Y2_WEEK		(0x30)
#define BNR_DC_GAIN_CTRL_X2_WEEK		(0x80)
#define BNR_DC_GAIN_CTRL_Y3_WEEK		(0x40)
#define BNR_DC_GAIN_CTRL_X3_WEEK		(0xFF)

#define BNR_DC_GAIN_CTRL_Y0_WEEK		(0x00)
#define BNR_DC_GAIN_CTRL_X0_WEEK		(0x10)
#define BNR_DC_GAIN_CTRL_Y1_WEEK		(0x20)
#define BNR_DC_GAIN_CTRL_X1_WEEK		(0x20)
#define BNR_DC_GAIN_CTRL_Y2_MEDIUM		(0x40)
#define BNR_DC_GAIN_CTRL_X2_MEDIUM		(0x80)
#define BNR_DC_GAIN_CTRL_Y3_MEDIUM		(0x80)
#define BNR_DC_GAIN_CTRL_X3_MEDIUM		(0xA0)

#define BNR_DC_GAIN_CTRL_Y0_MEDIUM		(0x00)
#define BNR_DC_GAIN_CTRL_X0_MEDIUM		(0x18)
#define BNR_DC_GAIN_CTRL_Y1_MEDIUM		(0x20)
#define BNR_DC_GAIN_CTRL_X1_MEDIUM		(0x30)



#define AC_BNR_FEATURE_CAL_MODE_VAL		(0xB0)

#define BNR_AC_H_ENABLE			(ENABLE)
#define BNR_AC_V_ENABLE			(ENABLE)
#define BNR_AC_STATUS_READ_TYPE		(BNR_AC_READ_TYPE_STRENGTH_HV)
#define BNR_AC_STATUS_READ_MODE		(BNR_AC_READ_MODE_X1000)
#define BNR_AC_HBMAX_GAIN				(0x5)
#define BNR_AC_VBMAX_GAIN				(0x5)
#define BNR_AC_HBMAX_GAIN_WEEK			(0x4)
#define BNR_AC_VBMAX_GAIN_WEEK			(0x4)
#define BNR_AC_HBMAX_GAIN_MEDIUM		(0x5)
#define BNR_AC_VBMAX_GAIN_MEDIUM		(0x5)
#define BNR_AC_HBMAX_GAIN_STRONG		(0x6)
#define BNR_AC_VBMAX_GAIN_STRONG		(0x6)


#define BNR_AC_STRENGTH_RESOLUTION	(BNR_AC_STR_RES_10BIT)
#define BNR_AC_FILTER_TYPE				(BNR_AC_FILTER_5TAP_AVG)
#define BNR_AC_OUTPUT_MUX			(BNR_AC_OUTPUT_NORMAL_DISPLAY)

#define BNR_AC_STRENGTH_H_X0					(0x19)
#define BNR_AC_STRENGTH_H_X1					(0x4B)
#define BNR_AC_STREMGTH_H_MAX				(0x80)
#define BNR_AC_STREMGTH_H_MAX_WEEK			(0x60)
#define BNR_AC_STREMGTH_H_MAX_MEDIUM		(0x80)
#define BNR_AC_STREMGTH_H_MAX_STRONG		(0x80)

#define BNR_AC_MIN_THRESHOLD				(0x04)
#define BNR_AC_STRENGTH_V_X0				(0x19)
#define BNR_AC_STRENGTH_V_X1				(0x4B)

#define BNR_AC_STREMGTH_V_MAX			(0x80)
#define BNR_AC_STREMGTH_V_MAX_WEEK		(0x60)
#define BNR_AC_STREMGTH_V_MAX_MEDIUM	(0x80)
#define BNR_AC_STREMGTH_V_MAX_STRONG	(0x80)

#define BNR_AC_H_OFFSET_MODE				(BNR_AC_H_OFFSET_AUTO)
#define BNR_AC_MANUAL_OFFSET_H_VALUE	(0x00)
#define BNR_AC_V_OFFSET_MODE				(BNR_AC_V_OFFSET_AUTO)
#define BNR_AC_MANUAL_OFFSET_V_VALUE	(0x00)
#define BNR_AC_OFFSET_USE_OF_HYSTERISIS	(BNR_AC_PREV_7FRAME)
#define BNR_AC_T_FILTER_WEIGHT			(0x70)

#define BNR_AC_MAX_DELTA_TH0				(0x0F)
#define BNR_AC_MAX_DELTA_TH1				(0x14)
#define BNR_AC_H_BLOCKNESS_TH			(0x00)
#define BNR_AC_H_WEIGHT_MAX				(0x00)

#define BNR_AC_SCALED_USE_OF_HYSTERISIS	(BNR_AC_PREV_6FRAME)

#define DER_ENABLE				(ENABLE)
#define DER_GAIN_MAPPING_EN	(ENABLE)

#define DER_BIF_TH_ENABLE			(0x20)
#define DER_BIF_TH_WEEK			(0x18)
#define DER_BIF_TH_MEDIUM			(0x20)
#define DER_BIF_TH_STRONG			(0x28)

#define DER_GAIN_TH0				(0x00)
#define DER_GAIN_TH1				(0x40)

/************/
/*  CNR		*/
/************/

#define CNR_CB_ENABLE				(ENABLE)
#define CNR_CR_ENABLE				(ENABLE)
#define CNR_LOW_LUMINANCE_EN		(ENABLE)

#define CNR_CB_DIFF_BOTTOM				(0x700)
#define CNR_CB_DIFF_BOTTOM_WEEK			(0x400)
#define CNR_CB_DIFF_BOTTOM_MEDIUM		(0x700)
#define CNR_CB_DIFF_BOTTOM_STRONG		(0x780)

#define CNR_CB_DIFF_TOP				(0x100)
#define CNR_CB_DIFF_TOP_WEEK			(0x3ff)
#define CNR_CB_DIFF_TOP_MEDIUM		(0x100)
#define CNR_CB_DIFF_TOP_STRONG		(0x80)

#define CNR_CB_SLOPE					(CNR_CBCR_CTRL_SLOPE1)
#define CNR_CB_SLOPE_WEEK				(0x3)
#define CNR_CB_SLOPE_MEDIUM			(0x1)
#define CNR_CB_SLOPE_STRONG			(0x0)

#define CNR_CB_DIST						(CNR_CBCR_CTRL_DIST0)
#define CNR_CB_DELTA					(0x64)
#define CNR_CB_DELTA_WEEK				(0x3ff)
#define CNR_CB_DELTA_MEDIUM			(0x64)
#define CNR_CB_DELTA_STRONG			(0x01)

#define CNR_CB_LOW_DIFF_BOTTOM				(0x700)
#define CNR_CB_LOW_DIFF_BOTTOM_WEEK		(0x400)
#define CNR_CB_LOW_DIFF_BOTTOM_MEDIUM		(0x700)
#define CNR_CB_LOW_DIFF_BOTTOM_STRONG		(0x780)

#define CNR_CB_LOW_DIFF_TOP				(0x100)
#define CNR_CB_LOW_DIFF_TOP_WEEK		(0x3ff)
#define CNR_CB_LOW_DIFF_TOP_MEDIUM		(0x100)
#define CNR_CB_LOW_DIFF_TOP_STRONG		(0x080)

#define CNR_CB_LOW_SLOPE					(CNR_CBCR_CTRL_SLOPE1)
#define CNR_CB_LOW_SLOPE_WEEK			(0x3)
#define CNR_CB_LOW_SLOPE_MEDIUM			(0x1)
#define CNR_CB_LOW_SLOPE_STRONG			(0x0)


#define CNR_CB_LOW_DIST			(CNR_CBCR_CTRL_DIST0)

#define CNR_CB_DELTA_LOW				(0x64)
#define CNR_CB_DELTA_LOW_WEEK		(0x3ff)
#define CNR_CB_DELTA_LOW_MEDIUM		(0x64)
#define CNR_CB_DELTA_LOW_STRONG		(0x01)

#define CNR_CR_DIFF_BOTTOM			(0x700)
#define CNR_CR_DIFF_BOTTOM_WEEK		(0x400)
#define CNR_CR_DIFF_BOTTOM_MEDIUM	(0x700)
#define CNR_CR_DIFF_BOTTOM_STRONG	(0x780)

#define CNR_CR_DIFF_TOP				(0x100)
#define CNR_CR_DIFF_TOP_WEEK			(0x3ff)
#define CNR_CR_DIFF_TOP_MEDIUM		(0x100)
#define CNR_CR_DIFF_TOP_STRONG		(0x080)

#define CNR_CR_SLOPE					(CNR_CBCR_CTRL_SLOPE1)
#define CNR_CR_SLOPE_WEEK				(0x3)
#define CNR_CR_SLOPE_MEDIUM			(0x1)
#define CNR_CR_SLOPE_STRONG			(0x0)


#define CNR_CR_DIST						(CNR_CBCR_CTRL_DIST0)
#define CNR_CR_DELTA					(0x64)
#define CNR_CR_DELTA_WEEK				(0x3ff)
#define CNR_CR_DELTA_MEDIUM			(0x64)
#define CNR_CR_DELTA_STRONG			(0x01)

#define CNR_CR_LOW_DIFF_BOTTOM 					(0x700)
#define CNR_CR_LOW_DIFF_BOTTOM_WEEK			(0x400)
#define CNR_CR_LOW_DIFF_BOTTOM_MEDIUM			(0x700)
#define CNR_CR_LOW_DIFF_BOTTOM_STRONG			(0x780)

#define CR_LOW_DIFF_TOP_BYPASS			(0x100)
#define CR_LOW_DIFF_TOP_WEEK				(0x3ff)
#define CR_LOW_DIFF_TOP_MEDIUM			(0x100)
#define CR_LOW_DIFF_TOP_STRONG			(0x080)

#define CNR_CR_LOW_SLOPE					(CNR_CBCR_CTRL_SLOPE1)
#define CNR_CR_LOW_SLOPE_WEEK			(0x3)
#define CNR_CR_LOW_SLOPE_MEDIUM			(0x1)
#define CNR_CR_LOW_SLOPE_STRONG			(0x0)

#define CNR_CR_LOW_DIST					(CNR_CBCR_CTRL_DIST0)
#define CNR_CR_DELTA_LOW					(0x64)
#define CNR_CR_DELTA_LOW_WEEK			(0x3ff)
#define CNR_CR_DELTA_LOW_MEDIUM			(0x64)
#define CNR_CR_DELTA_LOW_STRONG			(0x01)

#define MNR_FILTER_THRESHOLD		0x18

/************/
/*  RE		*/
/************/

#define OVERSTATE_EN_VAL		(ENABLE)


/* 	RE default algorithm	*/
#define RE_SP_GAIN_E_B				(0x14)
#define RE_SP_GAIN_E_B_WEEK		(0x5)
#define RE_SP_GAIN_E_B_STRONG		(0x3F)


#define RE_SP_GAIN_E_W				(0x14)
#define RE_SP_GAIN_E_W_WEEK		(0x5)
#define RE_SP_GAIN_E_W_STRONG	(0x3F)

#define RE_SP_GAIN_T_B				(0x14)
#define RE_SP_GAIN_T_B_WEEK		(0x5)
#define RE_SP_GAIN_T_B_STRONG		(0x3F)

#define RE_SP_GAIN_T_W				(0x14)
#define RE_SP_GAIN_T_W_WEEK		(0x5)
#define RE_SP_GAIN_T_W_STRONG	(0x3F)


#define CORING_MODE_BOTH			(0x3)
#define CORING_MODE_EDGE			(0x1)
#define CORING_MODE_TEXTURE		(0x2)

#define REG_CORING_EN_VAL			(ENABLE)

#define WEIGHT_A_MEDIUM			(0x20)
#define WEIGHT_A_WEEK				(0x5)
#define WEIGHT_A_STRONG			(0x7F)


#define WEIGHT_T_MEDIUM			(0x20)
#define WEIGHT_T_WEEK				(0x5)
#define WEIGHT_T_STRONG			(0x7F)

#define EDGE_OPERATOR_MOD_SOBEL	(0x0)
#define EDGE_OPERATOR_CON_SOBEL	(0x1)

#define WHITE_GAIN_MEDIUM			(0x32)
#define WHITE_GAIN_WEEK			(0x5)
#define WHITE_GAIN_STRONG			(0x7F)

#define BLACK_GAIN_MEDIUM			(0x3C)
#define BLACK_GAIN_WEEK			(0x5)
#define BLACK_GAIN_STRONG			(0x7F)

#define HORIZONTAL_GAIN_WEEK		(0x8)
#define HORIZONTAL_GAIN_MEDIUM	(0x20)
#define HORIZONTAL_GAIN_STRONG	(0xFF)

#define VERTICAL_GAIN_WEEK		(0x8)
#define VERTICAL_GAIN_MEDIUM		(0x20)
#define VERTICAL_GAIN_STRONG		(0xFF)

#define SOBEL_WEIGHT_WEEK			(0x19)
#define SOBEL_WEIGHT_MEDIUM		(0x64)
#define SOBEL_WEIGHT_STRONG		(0xFF)

#define LAPLACIAN_WEIGHT_DEFAULT		(0x24)
#define LAPLACIAN_WEIGHT_WEEK		(0x24)
#define LAPLACIAN_WEIGHT_MEDIUM		(0x64)
#define LAPLACIAN_WEIGHT_STRONG		(0xFF)

#define SOBEL_MANUAL_MODE_EN_VAL		(ENABLE)

#define SOBEL_MANUAL_GAIN_DEFAULT	(0x20)
#define SOBEL_MANUAL_GAIN_WEEK 		(0x19)
#define SOBEL_MANUAL_GAIN_MEDIUM		(0x32)
#define SOBEL_MANUAL_GAIN_STRONG		(0xFF)




#define DISPLAY_MODE_VAL					(re_display_normal)
#define GX_WEIGHT_MANUAL_MODE_EN_VAL	(ENABLE)
#define GX_WEIGHT_MANUAL_GAIN_WEEK		(0x20)
#define GX_WEIGHT_MANUAL_GAIN_MEDIUM	(0x80)
#define GX_WEIGHT_MANUAL_GAIN_STRONG	(0xFF)

#define LAP_H_MODE_VAL			(0x00)
#define LAP_H_MODE_STRONG		(0x02)
#define LAP_V_MODE_VAL			(0x00)
#define GBMODE_EN				(ENABLE)
#define GB_MODE_VAL			(0x00)

#define GB_X1_WEEK				(0x06)
#define GB_X1_MEDIUM			(0x0C)
#define GB_X1_STRONG			(0x14)

#define GB_Y1_WEEK				(0x06)
#define GB_Y1_MEDIUM			(0x0C)
#define GB_Y1_STRONG			(0x14)

#define GB_X2_WEEK				(0xa)
#define GB_X2_MEDIUM			(0x19)
#define GB_X2_STRONG			(0x32)


#define GB_Y2_WEEK				(0xa)
#define GB_Y2_MEDIUM			(0x19)
#define GB_Y2_STRONG			(0x32)


#define GB_Y3_WEEK				(0x96)
#define GB_Y3_MEDIUM			(0xc8)
#define GB_Y3_STRONG			(0xe6)



#define LUM1_X_L0_WEEK		(0x28)
#define LUM1_X_L0_MEDIUM	(0x14)
#define LUM1_X_L0_STRONG	(0xa)


#define LUM1_X_L1_WEEK		(0x64)
#define LUM1_X_L1_MEDIUM	(0x3c)
#define LUM1_X_L1_STRONG	(0x1e)

#define LUM1_X_H0_WEEK	(0x82)
#define LUM1_X_H0_MEDIUM	(0x96)
#define LUM1_X_H0_STRONG	(0xb4)

#define LUM1_X_H1_WEEK	(0xb4)
#define LUM1_X_H1_MEDIUM	(0xc8)
#define LUM1_X_H1_STRONG	(0xe6)

#define LUM1_Y0_VAL			(0x00)
#define LUM1_Y1_VAL			(0xFF)
#define LUM1_Y2_VAL			(0x00)

#define LUM2_X_L0_WEEK		(0x28)
#define LUM2_X_L0_MEDIUM	(0x14)
#define LUM2_X_L0_STRONG	(0xa)

#define LUM2_X_L1_WEEK		(0x64)
#define LUM2_X_L1_MEDIUM	(0x3c)
#define LUM2_X_L1_STRONG	(0x1e)

#define LUM2_X_H0_WEEK	(0x82)
#define LUM2_X_H0_MEDIUM	(0x96)
#define LUM2_X_H0_STRONG	(0xb4)

#define LUM2_X_H1_WEEK	(0xb4)
#define LUM2_X_H1_MEDIUM	(0xc8)
#define LUM2_X_H1_STRONG	(0xe6)

#define LUM2_Y0_VAL			(0x00)
#define LUM2_Y1_VAL			(0xFF)
#define LUM2_Y2_VAL			(0x00)

/************/
/*  CE		*/
/************/
/* 	CE default algorithm	*/
#define DMA_AUTO				(0x0)
#define DMA_USER_DEFINE		(0x1)

#define APL_WIN_X0				(0x0)
#define APL_WIN_Y0				(0x0)
#define APL_WIN_X1				(0x77F)
#define APL_WIN_Y1				(0x437)
#define APL_WIN_X1_BYPASS		(0xAF)
#define APL_WIN_Y1_BYPASS		(0x8f)

#define CONTRAST_ALPHA_MEDIUM		(0x80)
#define CONTRAST_ALPHA_WEEK			(0x10)
#define CONTRAST_ALPHA_STRONG		(0xFF)

#define CONTRAST_DELTA_MAX_MEDIUM	(0x40)
#define CONTRAST_DELTA_MAX_WEEK		(0x5)
#define CONTRAST_DELTA_MAX_STRONG	(0xFF)

#define VIGNETTING_GAIN_BYPASS		(0x0)
#define VIGNETTING_GAIN_WEEK			(0x5)
#define VIGNETTING_GAIN_MEDIUM		(0x40)
#define VIGNETTING_GAIN_STRONG		(0xFF)

#define TONE_GAIN_BYPASS				(0x0)
#define TONE_GAIN_WEEK				(0x1)
#define TONE_GAIN_MEDIUM				(0x3F)
#define TONE_GAIN_STRONG				(0x7F)

#define TONE_OFFSET_BYPASS			(0x10)
#define TONE_OFFSET_WEEK				(0x4)
#define TONE_OFFSET_STRONG			(0x40)

#define CONTRAST_CTRL_MEDIUM			(0x260)
#define CONTRAST_CTRL_WEEK			(0x300)
#define CONTRAST_CTRL_STRONG			(0x3FF)

#define CENTER_POSITION_MEDIUM		(0x10)
#define CENTER_POSITION_WEEK			(0x10)
#define CENTER_POSITION_STRONG		(0x10)

#define BRIGHTNESS_CONTROL_MEDIUM	(0x80)
#define BRIGHTNESS_CONTROL_WEEK		(0x0)
#define BRIGHTNESS_CONTROL_STRONG	(0xFF)

#define SELECT_HSV_VAL_EN				(ENABLE)
#define SELECT_RGB_VAL_EN				(ENABLE)
#define VSP_SEL_VAL_DIS					(DISABLE)
#define GLOBAL_APL_SEL_VAL_DIS			(DISABLE)
#define REG_CEN_BYPASS_ENABLE			(ENABLE)
#define REG_CEN_DEBUG_MODE_VAL			(DISABLE)
#define FIRST_CORE_GAIN_DISABLE		(DISABLE)
#define SECOND_CORE_GAIN_DISABLE		(DISABLE)
#define V_SCALER_EN_VALUE				(DISABLE)

#define DEBUG_MODE_SEL_VAL					(DISABLE)
#define DEMO_MODE_VAL						(0x78)

#define SHOW_COLOR_REGION0_VAL		(0x00)
#define SHOW_COLOR_REGION1_VAL		(0x00)
#define COLOR_REGION_EN0_VAL		(0xFF)
#define COLOR_REGION_EN1_VAL		(0xFF)

#define IHSV_SGAIN_MEDIUM		(0x80)
#define IHSV_SGAIN_WEEK			(0x01)
#define IHSV_SGAIN_STRONG		(0xFF)

#define IHSV_VGAIN_MEDIUM		(0x80)
#define IHSV_VGAIN_WEEK			(0x01)
#define IHSV_VGAIN_STRONG		(0xFF)

#define IHSV_HOFFSET_MEDIUM		(0x80)
#define IHSV_HOFFSET_WEEK		(0x01)
#define IHSV_HOFFSET_STRONG		(0xFF)

#define IHSV_SOFFSET_MEDIUM		(0x80)
#define IHSV_SOFFSET_WEEK		(0x01)
#define IHSV_SOFFSET_STRONG		(0xFF)

#define IHSV_VOFFSET_MEDIUM		(0x80)
#define IHSV_VOFFSET_WEEK		(0x01)
#define IHSV_VOFFSET_STRONG		(0xFF)

#define DEN_CTRL0_DEFAULT		(0x01)
#define DEN_CTRL0_WEEK			(0x80)
#define DEN_CTRL0_MEDIUM		(0x82)
#define DEN_CTRL0_STRONG		(0x83)

#define DEN_APL_LIMIT_HIGH_MEDIUM	(0x80)
#define DEN_APL_LIMIT_HIGH_WEEK		(0x01)
#define DEN_APL_LIMIT_HIGH_STRONG	(0xFF)

#define DEN_GAIN_MEDIUM			(0x80)
#define DEN_GAIN_WEEK			(0x00)
#define DEN_GAIN_STRONG			(0xFF)

#define DEN_CORING_MEDIUM		(0x80)
#define DEN_CORING_WEEK			(0x01)
#define DEN_CORING_STRONG		(0xFF)

#define CEN_LUT_EN_VALUE			(DISABLE)
#define CEN_LUT_MODE			(0x0)
#define CEN_LUT_ADRS_VALUE			(0x0)
#define CEN_LUT_WEN_READ			(0x1)
#define CEN_LUT_WEN_WRITE			(0x0)

#define COLOR_REGION0_SEL_VALUE			(0x0)
#define COLOR_REGION1_SEL_VALUE			(0x0)
#define COLOR_REGION2_SEL_VALUE			(0x0)
#define COLOR_REGION3_SEL_VALUE			(0x0)
#define COLOR_REGION4_SEL_VALUE			(0x0)
#define COLOR_REGION5_SEL_VALUE			(0x0)
#define COLOR_REGION6_SEL_VALUE			(0x0)
#define COLOR_REGION7_SEL_VALUE			(0x0)
#define COLOR_REGION8_SEL_VALUE			(0x0)
#define COLOR_REGION9_SEL_VALUE			(0x0)
#define COLOR_REGION10_SEL_VALUE			(0x0)
#define COLOR_REGION11_SEL_VALUE			(0x0)
#define COLOR_REGION12_SEL_VALUE			(0x0)
#define COLOR_REGION13_SEL_VALUE			(0x0)
#define COLOR_REGION14_SEL_VALUE			(0x0)
#define COLOR_REGION15_SEL_VALUE			(0x0)

#define DCE_HSV_SELECTION_VAL				(0x0)
#define DCE_YUV_SELECTION_VAL				(0x1)

#define COLOR_REGION_GAIN_MEDIUM		(0x80)
#define COLOR_REGION_GAIN_WEEK			(0x01)
#define COLOR_REGION_GAIN_STRONG		(0xFF)

#define Y_GRAD_GAIN_MEDIUM				(0x1)
#define Y_GRAD_GAIN_WEEK				(0x3)
#define Y_GRAD_GAIN_STRONG				(0x0)

#define CB_GRAD_GAIN_MEDIUM				(0x1)
#define CB_GRAD_GAIN_WEEK				(0x3)
#define CB_GRAD_GAIN_STRONG				(0x0)

#define CR_GRAD_GAIN_MEDIUM				(0x1)
#define CR_GRAD_GAIN_WEEK				(0x3)
#define CR_GRAD_GAIN_STRONG				(0x0)

#define Y_RANGE_MIN_MEDIUM				(0x01E0)
#define Y_RANGE_MIN_WEEK				(0x001)
#define Y_RANGE_MIN_STRONG				(0x1FF)

#define Y_RANGE_MAX_MEDIUM				(0x02D0)
#define Y_RANGE_MAX_WEEK				(0x200)
#define Y_RANGE_MAX_STRONG				(0x3FF)

#define CB_RANGE_MIN_VALUE					(0x264)
#define CB_RANGE_MAX_VALUE					(0x02B4)

#define CR_RANGE_MIN_VALUE					(0x134)
#define CR_RANGE_MAX_VALUE					(0x01FC)

#define HIF_DYC_WDATA_Y32ND				(0x0FF)
#define HIF_DYC_WDATA_Y32ND_WEEK		(0x0)
#define HIF_DYC_WDATA_Y32ND_MEDIUM			(0x0FF)
#define HIF_DYC_WDATA_Y32ND_STRONG		(0x1FF)

#define HIF_DYC_WDATA_X32ND				(0x1FF)
#define HIF_DYC_WDATA_X32ND_WEEK		(0x0)
#define HIF_DYC_WDATA_X32ND_MEDIUM		(0x1FF)
#define HIF_DYC_WDATA_X32ND_STRONG		(0x3FF)

#define DCE_LUT_WEN_READ						(0x1)
#define DCE_LUT_WEN_WRITE						(0x0)


#define SATURATION_REGION0_SEL_VAL			(0x0)
#define SATURATION_REGION1_SEL_VAL			(0x0)
#define SATURATION_REGION2_SEL_VAL			(0x0)
#define SATURATION_REGION3_SEL_VAL			(0x0)
#define SATURATION_REGION4_SEL_VAL			(0x0)
#define SATURATION_REGION5_SEL_VAL			(0x0)
#define SATURATION_REGION6_SEL_VAL			(0x0)
#define SATURATION_REGION7_SEL_VAL			(0x0)
#define SATURATION_REGION8_SEL_VAL			(0x0)
#define SATURATION_REGION9_SEL_VAL			(0x0)
#define SATURATION_REGION10_SEL_VAL			(0x0)
#define SATURATION_REGION11_SEL_VAL			(0x0)
#define SATURATION_REGION12_SEL_VAL			(0x0)
#define SATURATION_REGION13_SEL_VAL			(0x0)
#define SATURATION_REGION14_SEL_VAL			(0x0)
#define SATURATION_REGION15_SEL_VAL			(0x0)

#define SATURATION_REGION_GAIN_MEDIUM					(0x80)
#define SATURATION_REGION_GAIN_WEEK			(0x01)
#define SATURATION_REGION_GAIN_STRONG			(0xFF)

#define HIF_DSE_WDATA_Y32ND				(0x3FF)
#define HIF_DSE_WDATA_Y32ND_WEEK		(0x0)
#define HIF_DSE_WDATA_Y32ND_MEDIUM			(0x0FF)
#define HIF_DSE_WDATA_Y32ND_STRONG		(0x1FF)

#define HIF_DSE_WDATA_X32ND				(0x3FF)
#define HIF_DSE_WDATA_X32ND_WEEK		(0x0)
#define HIF_DSE_WDATA_X32ND_MEDIUM			(0x1FF)
#define HIF_DSE_WDATA_X32ND_STRONG		(0x3FF)

#define DSE_LUT_WEN_READ						(0x1)
#define DSE_LUT_WEN_WRITE						(0x0)

#define USER_CTRL_G_GAIN_MEDIUM					(0xC0)
#define USER_CTRL_G_GAIN_WEEK			(0x1)
#define USER_CTRL_G_GAIN_STRONG			(0xFF)

#define USER_CTRL_B_GAIN_MEDIUM					(0xC0)
#define USER_CTRL_B_GAIN_WEEK			(0x1)
#define USER_CTRL_B_GAIN_STRONG			(0xFF)

#define USER_CTRL_R_GAIN_MEDIUM					(0xC0)
#define USER_CTRL_R_GAIN_WEEK			(0x1)
#define USER_CTRL_R_GAIN_STRONG			(0xFF)

#define USER_CTRL_G_OFFSET_VALUE					(0x80)
#define USER_CTRL_B_OFFSET_VALUE				(0x80)
#define USER_CTRL_R_OFFSET_VALUE					(0x80)

#define DITHER_USER_RANDOM_NUMBER				(0x0)
#define DITHER_RANDOM_NUMBER_FREEZE			(0x1)

#define DITHER_BIT_MODE_10BIT					(0x0)
#define DITHER_BIT_MODE_8BIT						(0x1)

#define DECONTOR_GAIN_R_WEEK					(0x0)
#define DECONTOR_GAIN_R_MEDIUM					(0x4)
#define DECONTOR_GAIN_R_STRONG				(0x10)

#define DECONTOR_GAIN_G_WEEK					(0x0)
#define DECONTOR_GAIN_G_MEDIUM					(0x4)
#define DECONTOR_GAIN_G_STRONG				(0x10)

#define DECONTOR_GAIN_B_WEEK					(0x0)
#define DECONTOR_GAIN_B_MEDIUM					(0x4)
#define DECONTOR_GAIN_B_STRONG				(0x10)

#define GMC_LUT_MAX_VALUE_VAL			(0x00)
#define GMC_LUT_MAX_VALUE_STRONG		(0xFF)


#define RGB_MAX_VAL						(0xFF)
#define RGB_MIN_VAL					(0x00)

#define Y_OFFSET_VAL							(0x0)
#define Y_OFFSET_STRONG					(0x10)

#define C_OFFSET_VAL							(0x80)

#define R_CR_COEF_VAL						(0x167)
#define B_CB_COEF_VAL						(0x1c6)
#define G_CR_COEF_VAL						(0x0b7)
#define G_CB_COEF_VAL						(0x058)

#define Y_GAIN_VAL							(0x10)
#define Y_GAIN_WEEK						(0x04)
#define Y_GAIN_STRONG					(0xFF)

#define TAP_SIZE_VAL							(0x3)
#define TAP_SIZE_WEEK					(0x6)
#define TAP_SIZE_STRONG					(0x0)

#define COLOR_CTI_REGION					(0x20)
#define COLOR_CTI_REGION_WEEK			(0x01)
#define COLOR_CTI_REGION_STRONG			(0xFF)

#define HIST_ZONE_DISPLAY_VAL                       (0x0)

#define HIST_LUT_STEP_MODE_32				(0x0)
#define HIST_LUT_STEP_MODE_64				(0x1)

#define HIST_LUT_X_POS_RATE_ALL					(0x0)
#define HIST_LUT_X_POS_RATE_HALF				(0x1)
#define HIST_LUT_X_POS_RATE_QUARTER			(0x2)

#define HIST_WINDOW_START_X_VAL					(0x0)
#define HIST_WINDOW_START_Y_VAL					(0x0)

#define HIST_WINDOW_START_X_SIZE			(0x77F)
#define HIST_WINDOW_START_Y_SIZE			(0x437)

u16 gmc_lut_r_y_gamma2_2[256] = {
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,
	0x1,
	0x1,
	0x1,
	0x1,
	0x1,
	0x2,
	0x2,
	0x2,
	0x3,
	0x3,
	0x3,
	0x4,
	0x4,
	0x5,
	0x5,
	0x6,
	0x6,
	0x7,
	0x7,
	0x8,
	0x9,
	0x9,
	0xA,
	0xB,
	0xB,
	0xC,
	0xD,
	0xE,
	0xF,
	0xF,
	0x10,
	0x11,
	0x12,
	0x13,
	0x14,
	0x15,
	0x16,
	0x17,
	0x19,
	0x1A,
	0x1B,
	0x1C,
	0x1D,
	0x1F,
	0x20,
	0x21,
	0x23,
	0x24,
	0x26,
	0x27,
	0x29,
	0x2A,
	0x2C,
	0x2D,
	0x2F,
	0x31,
	0x32,
	0x34,
	0x36,
	0x37,
	0x39,
	0x3B,
	0x3D,
	0x3F,
	0x41,
	0x43,
	0x45,
	0x47,
	0x49,
	0x4B,
	0x4D,
	0x4F,
	0x51,
	0x54,
	0x56,
	0x58,
	0x5B,
	0x5D,
	0x5F,
	0x62,
	0x64,
	0x67,
	0x69,
	0x6C,
	0x6E,
	0x71,
	0x74,
	0x76,
	0x79,
	0x7C,
	0x7F,
	0x81,
	0x84,
	0x87,
	0x8A,
	0x8D,
	0x90,
	0x93,
	0x96,
	0x99,
	0x9C,
	0xA0,
	0xA3,
	0xA6,
	0xA9,
	0xAD,
	0xB0,
	0xB3,
	0xB7,
	0xBA,
	0xBE,
	0xC1,
	0xC5,
	0xC9,
	0xCC,
	0xD0,
	0xD4,
	0xD7,
	0xDB,
	0xDF,
	0xE3,
	0xE7,
	0xEB,
	0xEE,
	0xF2,
	0xF6,
	0xFB,
	0xFF,
	0x103,
	0x107,
	0x10B,
	0x10F,
	0x114,
	0x118,
	0x11C,
	0x121,
	0x125,
	0x12A,
	0x12E,
	0x133,
	0x137,
	0x13C,
	0x141,
	0x145,
	0x14A,
	0x14F,
	0x154,
	0x158,
	0x15D,
	0x162,
	0x167,
	0x16C,
	0x171,
	0x176,
	0x17B,
	0x180,
	0x186,
	0x18B,
	0x190,
	0x195,
	0x19B,
	0x1A0,
	0x1A5,
	0x1AB,
	0x1B0,
	0x1B6,
	0x1BB,
	0x1C1,
	0x1C7,
	0x1CC,
	0x1D2,
	0x1D8,
	0x1DE,
	0x1E3,
	0x1E9,
	0x1EF,
	0x1F5,
	0x1FB,
	0x201,
	0x207,
	0x20D,
	0x213,
	0x21A,
	0x220,
	0x226,
	0x22C,
	0x233,
	0x239,
	0x23F,
	0x246,
	0x24C,
	0x253,
	0x259,
	0x260,
	0x267,
	0x26D,
	0x274,
	0x27B,
	0x282,
	0x289,
	0x28F,
	0x296,
	0x29D,
	0x2A4,
	0x2AB,
	0x2B2,
	0x2B9,
	0x2C1,
	0x2C8,
	0x2CF,
	0x2D6,
	0x2DE,
	0x2E5,
	0x2EC,
	0x2F4,
	0x2FB,
	0x303,
	0x30A,
	0x312,
	0x31A,
	0x321,
	0x329,
	0x331,
	0x339,
	0x340,
	0x348,
	0x350,
	0x358,
	0x360,
	0x368,
	0x370,
	0x378,
	0x381,
	0x389,
	0x391,
	0x399,
	0x3A2,
	0x3AA,
	0x3B2,
	0x3BB,
	0x3C3,
	0x3CC,
	0x3D5,
	0x3DD,
	0x3E6,
	0x3EE,
	0x3F7

};


u16 dse_lut_x_1[32] = {
	0x1,
	0x21,
	0x41,
	0x61,
	0x81,
	0xA1,
	0xC1,
	0xE1,
	0x101,
	0x121,
	0x141,
	0x161,
	0x181,
	0x1A1,
	0x1C1,
	0x1E1,
	0x201,
	0x221,
	0x241,
	0x261,
	0x281,
	0x2A1,
	0x2C1,
	0x2E1,
	0x301,
	0x321,
	0x341,
	0x361,
	0x381,
	0x3A1,
	0x3C1,
	0x3E1
};

u16 dse_lut_y_1[32] = {

	0x1,
	0x11,
	0x21,
	0x31,
	0x41,
	0x51,
	0x61,
	0x71,
	0x81,
	0x91,
	0xA1,
	0xB1,
	0xC1,
	0xD1,
	0xE1,
	0xF1,
	0x111,
	0x141,
	0x171,
	0x1A1,
	0x1D1,
	0x201,
	0x231,
	0x261,
	0x291,
	0x2C1,
	0x2F1,
	0x321,
	0x351,
	0x381,
	0x3B1,
	0x3E1

};



u16 cen_lut_v_8point[8] = {

	0x0,
	0x24,
	0x48,
	0x6C,
	0x90,
	0xB4,
	0xD8,
	0xFF


};

u16 cen_lut_s_8point[8] = {

	0xA,
	0x15,
	0x20,
	0x2B,
	0x36,
	0x41,
	0x4C,
	0x5A
};

u16 cen_lut_h_8point[8] = {


	0x3AC,
	0x3CA,
	0x3E8,
	0x0,
	0x14,
	0x28,
	0x3C,
	0x46
};

u16 cen_lut_h_green_8point[8] = {

	0xF5,
	0x12C,
	0x13B,
	0x154,
	0x163,
	0x172,
	0x181,
	0x190

};



u16 cen_lut_v_gain[8] = {
	0x0,
	0x2D,
	0x5A,
	0x7F,
	0x7F,
	0x5A,
	0x2D,
	0x0

};

u16 cen_lut_s_gain[8] = {

	0x0,
	0x28,
	0x46,
	0x5A,
	0x64,
	0x73,
	0x7F,
	0x0
};

u16 cen_lut_h_gain[8] = {

	0x0,
	0x2D,
	0x5A,
	0x7F,
	0x7F,
	0x5A,
	0x2D,
	0x0
};

u16 cen_lut_h_green_gain[8] = {

		0x0,
		0x2D,
		0x5A,
		0x7F,
		0x7F,
		0x5A,
		0x2D,
		0x00

};


u16 cen_lut_debug_color[16] = {

	0x0,
	0x3F,
	0xC,
	0x30,
	0x27,
	0x3,
	0x33,
	0x1B,
	0x0,
	0x3F,
	0xC,
	0x30,
	0x27,
	0x3,
	0x33,
	0x1B
};

u16 cen_lut_delta_gain_h[16] = {

	0x80,
	0x80,
	0x80,
	0xFF,
	0xFF,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80
};

u16 cen_lut_delta_gain_s[16] = {

	0x80,
	0xFF,
	0x80,
	0x80,
	0xFF,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80
};

u16 cen_lut_delta_gain_v[16] = {

	0x80,
	0x80,
	0xFF,
	0x80,
	0xFF,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80
};

u16 cen_lut_global_master_gain[16] = {

	0x0FF,
	0x40,
	0xC0,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80,
	0x80

};


u16 cen_lut_global_delta_gain[6] = {

	0x200,
	0x200,
	0x200,
	0x200,
	0x200,
	0x200
};

