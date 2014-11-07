/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Jooyeon Lee <jylee256.lee@lge.com>
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

#ifndef _CABC_HAL_H
#define _CABC_HAL_H

#include <linux/types.h>

enum cabc_ch_id
{
	CABC_SRC_1,
	CABC_SRC_2
};

enum cabc_hist_lut_mode
{
	CABC_HISTLUT_DMA_AUTO_MODE,
	CABC_HISTLUT_DMA_MANUAL_MODE,
	CABC_HISTLUT_APB_MODE
};

enum cabc_hist_step
{
	CABC_HIST_STEP_32,
	CABC_HIST_STEP_64
};

enum cabc_gain_lut_mode
{
	CABC_GAINLUT_DMA_MANUAL_MODE,
	CABC_GAINLUT_APB_MODE
};

enum cabc_gain_range
{
	CABC_GAIN_RANGE_1024,
	CABC_GAIN_RANGE_512,
	CABC_GAIN_RANGE_256
};

void cabc_hal_write_interrupt_mask(enum cabc_ch_id cabc_idx, bool enable);
bool cabc_hal_write_image_size(enum cabc_ch_id cabc_idx, uint32_t w, uint32_t h);

void cabc_hal_write_hist_lut_dma_start(enum cabc_ch_id cabc_idx);
void cabc_hal_write_hist_lut_baseaddr(enum cabc_ch_id cabc_idx, unsigned int baseaddr);
bool cabc_hal_write_hist_lut_dma_mode(enum cabc_ch_id cabc_idx, enum cabc_hist_lut_mode cabc_op_mode);
bool cabc_hal_write_hist_lut_step(enum cabc_ch_id cabc_idx, enum cabc_hist_step step);

void cabc_hal_write_gain_lut_dma_start(enum cabc_ch_id cabc_idx);
void cabc_hal_write_gain_lut_baseaddr(enum cabc_ch_id cabc_idx, unsigned int baseaddr);
bool cabc_hal_write_gain_lut_dma_mode(enum cabc_ch_id cabc_idx, enum cabc_gain_lut_mode cabc_op_mode);
bool cabc_hal_write_gain_lut_range(enum cabc_ch_id cabc_idx, enum cabc_gain_range range);

void cabc_hal_enable(enum cabc_ch_id cabc_idx, bool enable);
void cabc_hal_write_gain_lut_wen(enum cabc_ch_id cabc_idx);

bool cabc_hal_set_global_gain(enum cabc_ch_id cabc_idx, unsigned int global_gain);

bool cabc_hal_open(enum cabc_ch_id cabc_idx, uint32_t w, uint32_t h, enum cabc_hist_step step,
		enum cabc_hist_lut_mode cabc_op_mode, enum cabc_gain_range range, unsigned int global_gain);
bool cabc_hal_close(enum cabc_ch_id cabc_idx);
void cabc_hal_init(enum cabc_ch_id cabc_idx);
void cabc_hal_cleanup(enum cabc_ch_id cabc_idx);
#endif
