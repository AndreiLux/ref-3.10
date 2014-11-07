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

/* The comment below is the front page for code-generated doxygen documentation */
/*!
 ******************************************************************************
 @mainpage
 This document details the APIs and implementation of the DSS DIP MIPI-DSI.
 It is intended to be used in conjunction with the DSS-DU-DIP
 and the MIPI DSI Device Driver.
 *****************************************************************************/

#ifndef _H_MIPI_DSI_HAL
#define _H_MIPI_DSI_HAL

enum mode_data_path
{
	CMD_MODE_DATAPATH, //   0 = command mode datapath (cpu_mode)
	VIDEO_MODE_DATAPATH, //   1 = video mode datapath (rgb_mode)
};

enum hs_data_path
{
	MODE0_NON_DATA_PATH,
	MODE1_DATA_PATH,
};

enum dsi_intr {
	DSI_INT0,
	DSI_INT1,
	DSI_INT2,
};

enum packet_struct
{
	SHORT_PACKET,
	LONG_PACKET,
	INVALID_PACKET,
};

enum dsi_action {
	DSI_ACTION_CLK_LANE_ENTER_ULP,
	DSI_ACTION_CLK_LANE_EXIT_ULP,
	DSI_ACTION_CLK_LANE_ENTER_HS,
	DSI_ACTION_CLK_LANE_EXIT_HS,
	DSI_ACTION_DATA_LANE_ENTER_ULP,
	DSI_ACTION_DATA_LANE_EXIT_ULP,
	DSI_ACTION_FORCE_DATA_LANE_STOP,
	DSI_ACTION_TURN_REQUEST,
	DSI_ACTION_FLUSH_RX_FIFO0,
	DSI_ACTION_FLUSH_CFG_FIFO,
	DSI_ACTION_FLUSH_DATA_FIFO,
	DSI_ACTION_STOP_VIDEO_DATA,
};

/* status*/
enum dsi_status {
    _DSI_STATUS_MIPI_PLL_LOCK        = (1 << 31),
    _DSI_STATUS_TX_STOP_STATUS_DATA3 = (1 << 30),
    _DSI_STATUS_LOCAL_RX_LONG_ENA3   = (1 << 28),
    _DSI_STATUS_RX_ULP_ESC3          = (1 << 27),
    _DSI_STATUS_PHY_DIRECTION3       = (1 << 26),
    _DSI_STATUS_LOCAL_RX_LONG_ENA2   = (1 << 25),
    _DSI_STATUS_RX_ULP_ESC2          = (1 << 24),
    _DSI_STATUS_PHY_DIRECTION2       = (1 << 23),
    _DSI_STATUS_TX_STOP_STATUS_DATA2 = (1 << 22),
    _DSI_STATUS_LOCAL_RX_LONG_ENA1   = (1 << 19),
    _DSI_STATUS_RX_ULP_ESC1          = (1 << 18),
    _DSI_STATUS_PHY_DIRECTION1       = (1 << 17),
    _DSI_STATUS_TX_STOP_STATUS_DATA1 = (1 << 16),
    _DSI_STATUS_LOCAL_RX_LONG_ENA0   = (1 << 15),
    _DSI_STATUS_TX_STOP_STATUS_DATA0 = (1 << 14),
    _DSI_STATUS_HS_DATA_SM           = (0x1F << 9),
    _DSI_STATUS_PHY_DATA_STATUS      = (0x03 << 7),
    _DSI_STATUS_PHY_CLK_STATUS       = (0x03 << 5),
    _DSI_STATUS_HS_CFG_READY         = (1 << 4),
    _DSI_STATUS_HS_DATA_READY        = (1 << 3),
    _DSI_STATUS_ESCAPE_READY         = (1 << 2),
    _DSI_STATUS_RX_ULP_ESC0          = (1 << 1),
	_DSI_STATUS_PHY_DIRECTION0       = (1 << 0),
};

void dip_hal_write_mipi_path_params(enum dsi_module idx, bool ctrl_en, bool is_mipi_path);
unsigned int dip_hal_read_interrupt_source(enum dsi_module idx);
unsigned int dip_hal_read_interrupt_clear(enum dsi_module idx);
void dip_hal_write_interrupt_clear(enum dsi_module idx, unsigned int val);

unsigned int mipi_hal_read_interrupt_source(enum dsi_module idx, enum dsi_intr int_idx);
unsigned int mipi_hal_read_interrupt_mask(enum dsi_module idx, enum dsi_intr int_idx);
void mipi_hal_write_interrupt_mask(enum dsi_module idx, enum dsi_intr int_idx, unsigned int val);
unsigned int mipi_hal_read_interrupt_clear(enum dsi_module idx, enum dsi_intr int_idx);
void mipi_hal_write_interrupt_clear(enum dsi_module idx, enum dsi_intr int_idx, unsigned int val);

void mipi_hal_write_config0_pixel_fmt(enum dsi_module idx, enum dsi_input_pixel_fmt in_pixel, enum dsi_output_pixel_fmt out_pixel);
void mipi_hal_write_config0_opr_mode(enum dsi_module idx, enum mode_data_path mode);
void mipi_hal_write_config0_turndisable(enum dsi_module idx, bool turndisable);
void mipi_hal_write_config0_phy_lane_module(enum dsi_module idx, u8 lanes_num);
void mipi_hal_write_config0_dphy_reset(enum dsi_module idx, bool is_reset);
void mipi_hal_write_config0_lp_escape(enum dsi_module idx, bool lp_esc );

void mipi_hal_write_config1_sel_data_lanes(enum dsi_module idx, u8 lp_num, u8 hs_num);
void mipi_hal_write_config2_dphy_shutdown(enum dsi_module idx, bool is_pd);
void mipi_hal_write_video_mode_desc(enum dsi_module idx, unsigned short hfp_cnt, unsigned short hsa_cnt, unsigned short bllp_cnt, unsigned short hbp_cnt,
									unsigned short vsa_lines, unsigned short vbp_lines, unsigned short vfp_lines, unsigned short vact_lines, unsigned short bytes_line,
									bool full_sync);
void mipi_hal_write_action(enum dsi_module idx, enum dsi_action dsi_act);
u8 mipi_hal_read_status_field(enum dsi_module idx, enum dsi_status field );
void mipi_hal_write_hs_packet(enum dsi_module idx, unsigned char dataID, unsigned short data, bool hs_enter_BTA_skip,
	                              bool enable_Eotp, enum hs_data_path mode, enum packet_struct packet_type);
void mipi_hal_write_hs_packet_mode(enum dsi_module idx, enum hs_data_path mode);
void mipi_hal_write_escape(enum dsi_module idx, u8 dataID, u8 data0, u8 data1, u8 data);
void mipi_hal_write_command_cfg(enum dsi_module idx, unsigned short num_bytes, bool wait_fifo);
unsigned int mipi_hal_write_tif (enum dsi_module dsi_idx, u32 addr, u8 data);
void mipi_hal_write_tif_testclr(enum dsi_module idx, bool is_hi);

void mipi_hal_set_base_addr(enum dsi_module dsi_num, unsigned int lcd_base, unsigned int mipi_base);
#endif /* _H_MIPI_DSI_HAL */
