/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Junglark Park <junglark.park@lgepartner.com>
 * Youngki Lyu <youngki.lyu@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _H_MIPI_DSI_COMMON
#define _H_MIPI_DSI_COMMON

/// The DIP architecture have 2 MIPI Controller.
enum dsi_module {
	MIPI_DSI_DIP0 = 0,	/// LCD_MIPI
	MIPI_DSI_DIP1,		/// MIPI_MIPI
	MIPI_DSI_MAX
};

/// Indicates the format of the frame data entering the MIPI DSI block.
enum dsi_input_pixel_fmt {
	INPUT_RGB24,
	INPUT_RGB565,
};

/// Indicates the format of the frame data sent to the LCD. Formats are
///described in the MIPI DSI spec.
enum dsi_output_pixel_fmt {
	OUTPUT_RGB565_CMD_MODE,
	OUTPUT_RGB565_VM_MODE,
	OUTPUT_RGB666_VM_MODE_PACKED,
	OUTPUT_RGB666,
	OUTPUT_RGB24,
	OUTPUT_RGB111_CMD_MODE,
	OUTPUT_RGB332_CMD_MODE,
	OUTPUT_RGB444_CMD_MODE,
};

#endif // _H_MIPI_DSI_COMMON
