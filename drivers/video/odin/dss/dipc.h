/*
 * linux/drivers/video/odin/dss/dispc.h
 *
 * Copyright (C) 2012 LG Electronics
 * Author: Seungwon Shin <m4seungwon.shin@lge.com>
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

#ifndef __ODIN_DIPC_H__
#define __ODIN_DIPC_H__

#include "dipc-regs.h"

#define DIP_IMPRB_GAMMA_TABLE_MXA_LENGTH	256

enum odin_dip_type {
	ODIN_DIP_TYPE_RGB_OR_MIPI	= 0,
	ODIN_DIP_TYPE_MIPI_ONLY		= 1,
	ODIN_DIP_TYPE_HDMI		= 2,
};

enum odin_dip_ctrl_fifo_width {
	ODIN_DIP_CTRL_FIFO_SIZE_8BITS	= 0,
	ODIN_DIP_CTRL_FIFO_SIZE_16BITS	= 1,
	ODIN_DIP_CTRL_FIFO_SIZE_32BITS	= 2
};

enum odin_dip_data_width {
	ODIN_DATA_WIDTH_8BIT		= 0,
	ODIN_DATA_WIDTH_12BIT		= 1,
	ODIN_DATA_WIDTH_16BIT		= 2,
	ODIN_DATA_WIDTH_18BIT		= 3,
	ODIN_DATA_WIDTH_24BIT		= 4
};

enum odin_dip_input_format {
	ODIN_DIP_INPUT_FORMAT_RGB24	= 0,
	ODIN_DIP_INPUT_FORMAT_RGB565	= 1,
	ODIN_DIP_INPUT_FORMAT_YUV444	= 2,
	ODIN_DIP_INPUT_FORMAT_YUV422S	= 3,
	ODIN_DIP_INPUT_FORMAT_YUV420P	= 4,
};

enum mv_dip_output_format {
	ODIN_DIP_OUTPUT_FORMAT_RGB24	= 0,
	ODIN_DIP_OUTPUT_FORMAT_RGB666	= 1,
	ODIN_DIP_OUTPUT_FORMAT_RGB565	= 2,
	ODIN_DIP_OUTPUT_FORMAT_YUV444	= 3,
	ODIN_DIP_OUTPUT_FORMAT_YUV422S	= 4
};

enum odin_dip_gamma_operation {
	ODIN_DIP_GAMMA_TABLE_VAL    = 0,
	ODIN_DIP_GAMMA_DEFAULT_VAL  = 1,
	ODIN_DIP_GAMMA_R_SATURATION	= 2,
	ODIN_DIP_GAMMA_R_REJECION	= 3,
};

#endif	/* __ODIN_DIPC_H__*/
