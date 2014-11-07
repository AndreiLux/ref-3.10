/*
 * ISP interface driver
 *
 * Copyright (C) 2010 - 2013 Mtekvision
 * Author: DongHyung Ko <kodh@mtekvision.com>
 *         Jinyoung Park <parkjyb@mtekvision.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ISPCORE_H__
#define __ISPCORE_H__

extern unsigned int g_awb_path;

struct isp_core_handler {
	int (*probe)(void*);
	void (*update_acc)(int, int, int, int, int);
	void (*get_accsize)(void *);
	void (*acc_sync)(void);

	void (*blank_update)(void);
	void (*intr_ctrl)(int);
	void (*block_imm_update)(unsigned int, unsigned int);
	void (*line_mem_ctrl)(unsigned int);
	int  (*intr_vsync)(void);
	int  (*intr_fd)(void);
	int  (*intr_isp)(void);
	int  (*intr_mdis)(void);
	unsigned int (*getframecnt)(void);
	void (*set_pixel_order)(unsigned int, unsigned int);
	struct {
		void (*set_level)(void *,unsigned int);
		void (*set_level2)(unsigned int, unsigned int, unsigned int);
	} blc;
	struct {
		void (*set_table)(u8 *);
		void (*set_gain)(float, float, float);
		void (*set_gain2)(float, float, float);
		void (*set_ratio)(float, float);
		void (*set_order)(unsigned int);
		void (*set_control)(int);
		void (*set_masksel)(unsigned int, unsigned int);
	} ldc;
	struct {
		void (*set_control)(unsigned int);
		void (*set_threshold)(unsigned int, unsigned int);
	} grgb;
	struct {
		void (*set_table)(void *, unsigned int);

		void (*set_control)(unsigned int, unsigned int);
	} gamma;
	struct {
		int  (*set_matrix)(float *, unsigned int);
		void (*set_control)(int);
	} cc;
	struct {
		void (*set_window_rect)(int,int,int,int,int);
		unsigned int (*get_window_sum)(int);
		unsigned int (*get_window_gsum)(int);
		unsigned int (*get_window_pixelcnt)(int);
		unsigned int (*get_window_gpixelcnt)(int);
		unsigned int (*get_numof_window)(void);
		unsigned int (*get_numof_gwindow)(void);
	} ae;
	struct {
		void (*set_awb_gain)(float, float, float);
		void (*set_awb_path)(unsigned int);
		unsigned int (*get_awb_path)(void);
		void (*set_awb_ctdzone)(void *,int); /**< (CTD ZONE PTR, CTD ZONE No) */
		void (*set_window_rect)(int,int,int,int,int);
		struct autowhitebal_wga_value (*get_wga_sum)(int); /**< (winIdx, RGBch) */
		unsigned int (*get_wga_pixelcnt)(int);
		unsigned int (*get_numof_wgawindow)(void);
		unsigned int (*get_numof_ctdwindow)(void);
		unsigned int (*get_ctdgroup_sum)(int, int); /**< (GroupIdx, RGBch) */
		unsigned int (*get_ctdgroup_pixelcnt)(int); /**< (GroupIdx)*/
		unsigned int (*get_ctd_sum)(int);
		unsigned int (*get_ctd_pixelcnt)(void);
		void (*set_ctd_inc_zone_enable)(void *);
		void (*set_ctd_exc_zone_enable)(void *);
	} awb;
	struct {
		void (*set_dpa_table)(void *);
		void (*set_bnr_table)(void *);
		void (*set_ynr_table)(void *);
		void (*set_edge_table)(void *);
		void (*set_cnr_table)(void *);
		void (*set_ycnr2_table)(void *);
		void (*set_dpa_strength)(void *);
		void (*set_bnr_strength)(void *);
		void (*set_ynr_strength)(void *);
		void (*set_edge_strength)(void *, float);
		void (*set_cnr_strength)(void *);
		void (*set_ycnr2_strength)(void *);
		void (*set_filteroff)(void);
	} nr;
	struct {
		struct statistic_l_cdf (*get_l_cdf)(void);
		struct statistic_l_histogram (*get_l_histogram)(void);
		void (*set_l_cdf)(void);
		int (*analysis_cdf)(void);
		unsigned short *(*cal_l_curve)(void);
	} al;
	struct {
		void (*sel_effect_zone)(unsigned int, unsigned int);
		void (*acquisition)(void);
		void (*update_ctrl)(unsigned int);
		void (*set_step_number)(unsigned int);
		unsigned int (*get_step_number)(void);
	}hist;
	struct {
		void (*ymap_ctrl)(unsigned int);
		void (*y_avg_filter_ctrl)(unsigned int);
		void (*color_suppression_ctrl)(unsigned int);
		void (*set_color_suppression)(unsigned int, float, unsigned int, float);
		void (*init_ymap_table)(void *);
		void (*set_al_ymap_table)(unsigned int);
	} yc;
	struct {
		void (*set_bypass_ctrl)(unsigned int);
		void (*set_s_comp_weight)(float);
		void (*set_curve_table)(void *,unsigned int);
		void (*set_curve_table2)(unsigned int, unsigned int);
		void (*set_block_ctrl)(unsigned int);
		void (*set_curve_ctrl)(unsigned int);
		void (*set_curve_step)(unsigned int, unsigned int);
		void (*set_s_gain)(float);
		void (*tune_hsl_offset)(unsigned int, int);
		void (*set_h_extract_ctrl)(unsigned int);
		void (*set_h_extract_target)(void *);
		unsigned int (*get_curve_ctrl)(void);
	}hsl;
	struct {
		void (*onoff)(unsigned int, unsigned int);
		void (*debug_onoff)(unsigned int, unsigned int);
		void (*sel_effect_zone)(unsigned int, unsigned int);
		void (*debug_draw_ctrl)(unsigned int, unsigned int);
		void (*set_area)(void *);
	}pce;
	struct {
		void (*set_control)(unsigned int);
		void (*set_effect_color)(unsigned int, unsigned int);
		void (*set_rgbswap)(unsigned int);
	} image_effect;
	struct {
		void (*set_window_rect)(int,int,int,int,int);
		unsigned int (*get_window_sum)(int);
		unsigned int (*get_window_pixelcnt)(int);
		unsigned int (*get_numof_window)(void);
		unsigned int (*get_window_hpfsum)(int);
		unsigned int (*get_edge_data)(void*);
	} af;
	struct {
		void (*set_control)(unsigned int);
		void (*set_size)(unsigned int);
		void (*set_trunc_thr)(void *);
		void (*set_interval)(void *);
		void (*set_compare_cnt)(void *);
		void (*set_offset)(void *, unsigned int);
		void (*set_mask)(void *);
		struct mdis_vector_data *(*get_vector)(void);
		void (*write_vector)(int, int);
	} mdis;
	struct {
		unsigned int (*get_window_sum)(unsigned int);
		unsigned int (*get_numof_window)(void);
		unsigned int (*get_afd_sum)(unsigned int);
		void (*set_window_rect)(int,int,int,int,int);
	} afd;

#if 0
	struct {
		void (*set_fd)(unsigned int);
	} fd;
#endif
};

/* AWB PATH SELECT */
#define STAT_AWB_PATH_1 		0	/* awb pre gain 적용된 data */
#define STAT_AWB_PATH_2 		1	/* ldc2 gain 적용된 data */

/** @brief frame rate control configuration */
struct isp_framerate_ctrl {
	unsigned int step_fps;
	float step_down_gain;
};

struct isp_pixel_order_table {
	unsigned int bayerorder;
	unsigned int statisticorder;
};

struct isp_blc_table {
	unsigned int	red	, gr, gb, blue;
};

struct isp_ldc_maskctrl {
	float r_gain, g_gain, b_gain;
	float h_ratio, v_ratio;
	unsigned int masksel, maskedsel;
};

struct isp_ldc_table {
	u8 r, g, b;
};

struct isp_dpc_table {
	unsigned int	*config;
	unsigned int	size;
};


/*******************************************************************************
@ingroup hslBlock
@brief Structure of lightness histogram

Lightness 에 대한 Histogram 구조체
*******************************************************************************/

struct statistic_l_histogram{
	unsigned int hist_pxl_cnt;	/**< Histogram 의 Total Pixel Count */
	unsigned int hist_l_sum;		/**< Histogram 의 Total Lightness Sum */
	unsigned int level[512];	/**< Histogram Bin*/
};

enum {
	BYPASS_ENABLE_ENTIRE_HSL_BLOCK		= 0x00000001,
	BYPASS_ENABLE_HSL_TEST_PATTERN		= 0x00000002,
	BYPASS_ENABLE_IMAGE_EFFECT			= 0x00000004,
	BYPASS_ENABLE_H_S_CURVE 			= 0x00000008,
	BYPASS_ENABLE_PCE					= 0x00000010,
	BYPASS_ENABLE_L_HISTOGRAM			= 0x00000020,
	BYPASS_DISABLE_ENTIRE_HSL_BLOCK 	= 0x00010000,
	BYPASS_DISABLE_HSL_TEST_PATTERN 	= 0x00020000,
	BYPASS_DISABLE_IMAGE_EFFECT 		= 0x00040000,
	BYPASS_DISABLE_S_L_CURVE			= 0x00080000,
	BYPASS_DISABLE_PCE					= 0x00100000,
	BYPASS_DISABLE_L_HISTOGRAM			= 0x00200000
};

enum {
	ENABLE_PCE_BEBUG_DRAW_WHITE_POINT	= 0x00000001,
	ENABLE_PCE_SLOW_GAIN				= 0x00000002,
	DISABLE_PCE_BEBUG_DRAW_WHITE_POINT	= 0x00010000,
	DISABLE_PCE_SLOW_GAIN				= 0x00020000
};

enum {
	ENABLE_H_CURVE						= 0x00000001,
	ENABLE_H_S_CURVE					= 0x00000002,
	ENABLE_S_CURVE						= 0x00000004,
	ENABLE_L_CURVE						= 0x00000008,
	ENABLE_S_COMP						= 0x00000010,
	ENABLE_DRAW_S_COMP_AREA 			= 0x00000020,
	DISABLE_H_CURVE 					= 0x00010000,
	DISABLE_H_S_CURVE					= 0x00020000,
	DISABLE_S_CURVE 					= 0x00040000,
	DISABLE_L_CURVE 					= 0x00080000,
	DISABLE_S_COMP						= 0x00100000,
	DISABLE_DRAW_S_COMP_AREA			= 0x00200000
};

enum {
	DISABLE_GAMMA_CURVE 				= 0x00000000,  /*9351*/
	ENABLE_GAMMA_CURVE					= 0x00000001,
/*	  ENABLE_G_GAMMA					= 0x00000002, */
/*	  DISABLE_GAMMA_CURVE				= 0x00010000, */ /*zv*/
/*	  DISABLE_G_GAMMA					= 0x00020000  */
};

enum {
	BAYER_GAMMA_CURVE					= 0,
	RGB_GAMMA_CURVE 					= 1,
	Y_GAMMA_CURVE						= 2,
	CB_GAMMA_CURVE						= 3,
	CR_GAMMA_CURVE						= 4
};

enum {
	DISABLE_GRGB_FILTER					= 0,
	ENABLE_GRGB_FILTER					= 1,
	GRGB_FILTER_3X3_MASK				= 2,
	GRGB_FILTER_5X5_MASK				= 4,
};

enum {
	GRGB_FILTER_THRESHOLD_LOW			= 0x10,
	GRGB_FILTER_THRESHOLD_MID			= 0x20,
	GRGB_FILTER_THRESHOLD_HIGH			= 0x80,
};

enum {
	BAYER_R_GAMMA_CURVE_TABLE			= 0,
	BAYER_G_GAMMA_CURVE_TABLE			= 1,
	BATER_B_GAMMA_CURVE_TABLE			= 2,
	RGB_R_GAMMA_CURVE_TABLE 			= 4,
	RGB_G_GAMMA_CURVE_TABLE 			= 5,
	RGB_B_GAMMA_CURVE_TABLE 			= 6,
	YUV_Y_GAMMA_CURVE_TABLE 			= 8,
	YUV_CB_GAMMA_CURVE_TABLE			= 9,
	YUV_CR_GAMMA_CURVE_TABLE			= 10
};

enum {
	IMAGE_EFFECT_OFF					= 0,
	IMAGE_EFFECT_SKETCH_MODE			= 1,
	IMAGE_EFFECT_COLOR_SKETCH_MODE		= 2,
	IMAGE_EFFECT_EMBOSS_MODDE			= 3,
	IMAGE_EFFECT_COLOR_EMBOSS_MODE		= 4,
	IMAGE_EFFECT_SEPIA_MODE 			= 5,
	IMAGE_EFFECT_NEGATIVE_MODE			= 6,
	IMAGE_EFFECT_GRAY_MODE				= 7,
	IMAGE_EFFECT_SOLAR_MODE 			= 8
};

enum {
	HSL_CURVE_TYPE_H					= 0,
	HSL_CURVE_TYPE_H_S					= 1,
	HSL_CURVE_TYPE_S					= 2,
	HSL_CURVE_TYPE_L					= 3
};

enum {
	ENABLE_H_EXTRACTOR					= 0x00000001,
	ENABLE_H_INCLUDE_MODE				= 0x00000002,
	DISABLE_H_EXTRACTOR 				= 0x00010000,
	DISABLE_H_INCLUDE_MODE				= 0x00020000
};

enum {
	ISP_BAYER_BLOCK 					= 0,
	ISP_RGB_BLOCK						= 1,
	ISP_HSL_BLOCK						= 2,
	ISP_YUV_BLOCK						= 3
};

enum {
	LDC_MASK_SIZE_SEL_X2				= 0x00,
	LDC_MASK_SIZE_SEL_X1				= 0x01,
	LDC_MASK_SIZE_SEL_X05				= 0x02,
	LDC_MASK_SIZE_SEL_X025				= 0x03
};

enum {
	LDC_MASKED_IMAGES_SIZE_SEL_X4		= 0x00,
	LDC_MASKED_IMAGES_SIZE_SEL_X2		= 0x01,
	LDC_MASKED_IMAGES_SIZE_SEL_X1		= 0x02,
	LDC_MASKED_IMAGES_SIZE_SEL_X05		= 0x03
};

#if 1
#define G_GAMMA_ENABLE					0x02
#define GAMMA_ENABLE					0x01
#define GAMMA_DISABLE					0x00
#endif
#define LDC_ENABLE						0x01
#define LDC_DISABLE 					0x00

#define ISPCORE_INTR_POS_EN				0x0001	/* End Of Frame */
#define ISPCORE_INTR_NEG_EN				0x0002	/* Start Of Frame */
#define ISPCORE_INTR_STAT_EN			0x0010	/* Statistic Interrupt Enable */
#define ISPCORE_INTR_MDIS_EN			0x0020	/* Mdis Interrupt Enable */

#define ISPCORE_INTR_ALL_OFF			0x0000	/* All ISP Interrupt Disable */
#define ISPCORE_INTR_ALL_EN				(ISPCORE_INTR_POS_EN |\
										ISPCORE_INTR_NEG_EN |\
										ISPCORE_INTR_STAT_EN |\
										ISPCORE_INTR_MDIS_EN)

#define STAT_MDIS_WIN3_MODE 			0x80
#define STAT_MDIS_COMPARE_MODE			0x40
#define STAT_MDIS_VF_REVERSE_ON 		0x20
#define STAT_MDIS_VF_MODE				0x10
#define STAT_MDIS_UPDATE_TRIGGER		0x02
#define STAT_MDIS_ENABLE				0x01

#define MDIS_ENABLE 					0x01
#define MDIS_DISABLE					0x00

#define ISP_GAMMA_IDX_BAYER_R			0x001
#define ISP_GAMMA_IDX_BAYER_G			0x002
#define ISP_GAMMA_IDX_BAYER_B			0x003
#define ISP_GAMMA_IDX_RGB_R				0x011
#define ISP_GAMMA_IDX_RGB_G				0x012
#define ISP_GAMMA_IDX_RGB_B				0x013
#define ISP_GAMMA_IDX_Y					0x020
#define ISP_HSLCURVE_CNT				4
#define ISP_GAMMA_CNT					11


#endif    /* __ISPCORE_H__ */
