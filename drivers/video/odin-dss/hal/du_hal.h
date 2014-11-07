/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Sungwon Son <sungwon.son@lge.com>
 * Youngki Lyu <youngki.lyu@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DU_HAL_H_
#define _DU_HAL_H_

#include <linux/types.h>
#include <video/odin-dss/dss_types.h>
#include "dss_hal_ch_id.h"

enum du_color_format
{
	DU_COLOR_RGB565,
	DU_COLOR_RGB555,
	DU_COLOR_RGB444,
	DU_COLOR_RGB888,
	DU_COLOR_YUV422S,
	DU_COLOR_YUV422_2P,
	DU_COLOR_YUV224_2P,
	DU_COLOR_YUV420_2P,
	DU_COLOR_YUV444_2P,
	DU_COLOR_YUV422_3P,
	DU_COLOR_YUV224_3P,
	DU_COLOR_YUV420_3P,
	DU_COLOR_YUV444_3P
};

enum du_byte_swap
{
	DU_BYTE_ABCD,
	DU_BYTE_BADC,
	DU_BYTE_CDAB,
	DU_BYTE_DCBA,
};

enum du_word_swap
{
	DU_WORD_AB,
	DU_WORD_BA
};

enum du_alpha_swap
{
	DU_ALPHA_ARGB,
	DU_ALPHA_RGBA
};

enum du_rgb_swap
{
	DU_RGB_RGB,
	DU_RGB_RBG,
	DU_RGB_BGR,
	DU_RGB_BRG,
	DU_RGB_GRB,
	DU_RGB_RBR
};

enum du_yuv_swap
{
	DU_YUV_UV,
	DU_YUV_VU
};

enum du_lsb_padding_sel
{
	DU_PAD_ZERO,
	DU_PAD_MSB
};

enum du_bnd_fill_mode
{
	DU_FILL_BOUNDARY_COLOR,
	DU_FILL_BLACK_COLOR
};

enum du_flip_mode
{
	DU_FLIP_NONE,
	DU_FLIP_V,
	DU_FLIP_H,
	DU_FLIP_HV
};

unsigned int du_hal_read_mem_full_command_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_mem_full_data_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_mem_empty_command_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_mem_empty_data_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_full_data_y_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_full_data_u_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_full_data_v_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_empty_data_y_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_empty_data_u_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_empty_data_v_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_con_y_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_con_u_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_con_v_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_con_state(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_con_sync_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_y_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_uv_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_read_buffer_sync_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_scaler_v_write_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_scaler_v_read_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_scaler_h_write_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_scaler_h_read_status(enum du_src_ch_id du_src_idx);

/* write back status function */
unsigned int du_hal_write_control_full_buffer_y_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_write_control_full_buffer_u_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_write_control_full_buffer_v_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_write_control_empty_buffer_y_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_write_control_empty_buffer_u_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_write_control_empty_buffer_v_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_write_control_y_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_write_control_u_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_write_control_v_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_write_control_sync_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_write_mem_full_command_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_write_mem_full_data_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_wirte_mem_empty_command_fifo_status(enum du_src_ch_id du_src_idx);
unsigned int du_hal_wirte_mem_empty_data_fifo_status(enum du_src_ch_id du_src_idx);

/* mxd status function */
/* mxd top status function */
unsigned int du_hal_mxd_top_pre_nr_in_fifo1_pindexel_cnt(void);
unsigned int du_hal_mxd_top_pre_nr_out_fifo1_pindexel_cnt(void);
unsigned int du_hal_mxd_top_post_nr_in_fifo1_pindexel_cnt(void);
unsigned int du_hal_mxd_top_post_nr_out_fifo1_pindexel_cnt(void);
unsigned int du_hal_mxd_top_pre_re_in_fifo2_pindexel_cnt(void);
unsigned int du_hal_mxd_top_pre_re_out_fifo2_pindexel_cnt(void);
unsigned int du_hal_mxd_top_post_re_in_fifo2_pindexel_cnt(void);
unsigned int du_hal_mxd_top_post_re_out_fifo2_pindexel_cnt(void);
unsigned int du_hal_mxd_top_pre_nr_write_fifo1_cnt(void);
unsigned int du_hal_mxd_top_post_nr_write_fifo1_cnt(void);
unsigned int du_hal_mxd_top_pre_re_write_fifo2_cnt(void);
unsigned int du_hal_mxd_top_post_re_write_fifo2_cnt(void);
unsigned int du_hal_mxd_top_xdversion(void);

/* mxd ce status function */
unsigned int du_hal_mxd_ce_apl_y_status(void);
unsigned int du_hal_mxd_ce_apl_rgb_status(void);
unsigned int du_hal_mxd_ce_apl_r_status(void);
unsigned int du_hal_mxd_ce_apl_g_status(void);
unsigned int du_hal_mxd_ce_apl_b_status(void);
unsigned int du_hal_mxd_ce_outside_apl_y(void);
unsigned int du_hal_mxd_ce_outside_apl_r(void);
unsigned int du_hal_mxd_ce_outside_apl_g(void);
unsigned int du_hal_mxd_ce_outside_apl_b(void);
unsigned int du_hal_mxd_ce_hist_win_y_max(void);
unsigned int du_hal_mxd_ce_hist_win_y_min(void);
unsigned int du_hal_mxd_ce_hist_rate_top1_step(void);
unsigned int du_hal_mxd_ce_hist_rate_top2_step(void);
unsigned int du_hal_mxd_ce_hist_rate_top3_step(void);
unsigned int du_hal_mxd_ce_hist_rate_top4_step(void);
unsigned int du_hal_mxd_ce_hist_rate_top5_step(void);
unsigned int du_hal_mxd_ce_hist_wind_pindexel_total_cnt(void);

bool du_hal_enable_mxd(bool vid_xd_path_en,bool fr_xd_re_path_sel, bool to_xd_re_path_sel, bool fr_xd_nr_path_sel, bool to_xd_nr_path_set);
bool du_hal_disable_mxd(void);

unsigned int du_hal_get_image_size(enum du_src_ch_id du_src_idx);
bool du_hal_gra_enable(enum du_src_ch_id du_src_idx,
		enum sync_ch_id sync_sel,
		enum du_color_format color_format, enum du_byte_swap byte_swap,
		enum du_word_swap word_swap, enum du_alpha_swap alpha_swap,
		enum du_rgb_swap rgb_swap, enum du_lsb_padding_sel lsb_sel,
		enum du_flip_mode flip_mode,
		struct dss_image_size src_image, struct dss_image_rect crop_image,
		unsigned int addr_y);
bool du_hal_gscl_wb_enable(
		bool alpha_reg_enable, enum sync_ch_id sync_sel,
		enum du_color_format color_format, enum du_byte_swap byte_swap,
		enum du_word_swap word_swap, enum du_alpha_swap alpha_swap,
		enum du_rgb_swap rgb_swap, enum du_yuv_swap yuv_swap,
		enum ovl_ch_id input_src_sel,
		unsigned char alpha_value,
		struct dss_image_size src_image, struct dss_image_rect crop_image,
		unsigned int addr_y, unsigned int addr_u);
bool du_hal_gra_disable(enum du_src_ch_id du_src_idx);

bool du_hal_vid_enable(enum du_src_ch_id du_src_idx,
		bool dual_op_enable, enum sync_ch_id sync_sel,
		enum du_color_format color_format, enum du_byte_swap byte_swap,
		enum du_word_swap word_swap, enum du_alpha_swap alpha_swap,
		enum du_rgb_swap rgb_swap, enum du_yuv_swap yuv_swap,
		enum du_lsb_padding_sel lsb_sel, enum du_bnd_fill_mode bnd_fill_mode,
		enum du_flip_mode flip_mode,
		struct dss_image_size src_image, struct dss_image_rect crop_image,
		struct dss_image_size pip_image,
		unsigned int addr_y, unsigned int addr_u, unsigned int addr_v);
bool du_hal_vid_wb_enable(enum du_src_ch_id du_src_idx,
		bool alpha_reg_enable, enum sync_ch_id sync_sel,
		enum du_color_format color_format, enum du_byte_swap byte_swap,
		enum du_word_swap word_swap, enum du_alpha_swap alpha_swap,
		enum du_rgb_swap rgb_swap, enum du_yuv_swap yuv_swap,
		enum du_src_ch_id  input_src_sel,
		unsigned char alpha_value,
		struct dss_image_size src_image, struct dss_image_rect crop_image,
		unsigned int addr_y, unsigned int addr_u);
bool du_hal_vid_disable(enum du_src_ch_id du_src_idx);
bool du_hal_vid_polypahse_set(enum du_src_ch_id du_src_idx,
		unsigned char polypahse_m_v[8], unsigned char polypahse_m_h[8][5],
		unsigned char polypahse_p_v[8][3], unsigned char polypahse_p_h[8][7]);

void du_hal_init(unsigned int vid0, unsigned int vid1, unsigned int vid2,
		unsigned int gra0, unsigned int gra1, unsigned int gra2,
		unsigned int gscl);
void du_hal_cleanup(void);

#endif /* _DU_HAL_H_ */
