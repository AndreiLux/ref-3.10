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

/*------------------------------------------------------------------------------
  File Inclusions
  ------------------------------------------------------------------------------*/

#include <linux/module.h>

#include "reg_def/mipi_mipi_union_reg.h"
#include "reg_def/mipi-dsi-regs.h"
#include "reg_def/dipc-regs.h"

#include "mipi_dsi_common.h"
#include "mipi_dsi_hal.h"

static struct
{
	volatile struct dip_register __iomem *dip_base[MIPI_DSI_MAX];
	volatile DSS_DIP_MIPI_MIPI_REG_T __iomem *mipi_base[MIPI_DSI_MAX];
} dsi_dev;

void dip_hal_write_mipi_path_params(enum dsi_module idx, bool ctrl_en, bool is_mipi_path)
{
	// 1 : DIP_LCD block enable, 0 : DIP_LCD block disable
	dsi_dev.dip_base[idx]->ctrl.af.enable = ctrl_en;

	// LCD Interface type 0 : RGB Paralle LCD path select 1 : MIPI LCD path select
	dsi_dev.dip_base[idx]->ctrl.af.lcd_if_type= is_mipi_path;

	dsi_dev.dip_base[idx]->dsp_configuration.af.pcompc_in_fmt = 3;

// if not use, flush fifos.
	if(ctrl_en == false) {	
		dsi_dev.dip_base[idx]->flush_resync_shadow.af.fifo0_flush=1;
		dsi_dev.dip_base[idx]->flush_resync_shadow.af.fifo1_flush=1;
	}
	
	dsi_dev.dip_base[idx]->flush_resync_shadow.af.shadow_update=1;
	
}

unsigned int dip_hal_read_interrupt_source(enum dsi_module idx)
{
	unsigned int ret_val;

	ret_val = dsi_dev.dip_base[idx]->int_source.a32;
	
	return ret_val;
}

unsigned int dip_hal_read_interrupt_clear(enum dsi_module idx)
{
	unsigned int ret_val;

	ret_val = dsi_dev.dip_base[idx]->int_clear.a32;
	
	return ret_val;
}

void dip_hal_write_interrupt_clear(enum dsi_module idx, unsigned int val)
{
	dsi_dev.dip_base[idx]->int_clear.a32 = val;
}

unsigned int mipi_hal_read_interrupt_source(enum dsi_module idx, enum dsi_intr int_idx)
{
	unsigned int ret_val=0;

	switch(int_idx) {
	case DSI_INT0:
		ret_val = dsi_dev.mipi_base[idx]->interrupt_source0.a32;
		break;

	case DSI_INT1:
		ret_val = dsi_dev.mipi_base[idx]->interrupt_source1.a32;
		break;

	case DSI_INT2:
		ret_val = dsi_dev.mipi_base[idx]->interrupt_source2.a32;
		break;
	}
	
	return ret_val;
}

unsigned int mipi_hal_read_interrupt_mask(enum dsi_module idx, enum dsi_intr int_idx)
{
	unsigned int ret_val=0;

	switch(int_idx) {
	case DSI_INT0:
		ret_val = dsi_dev.mipi_base[idx]->interrupt_mask0.af.mask0;
		break;

	case DSI_INT1:
		ret_val = dsi_dev.mipi_base[idx]->interrupt_mask1.af.mask1;
		break;

	case DSI_INT2:
		ret_val = dsi_dev.mipi_base[idx]->interrupt_mask2.af.mask2;
		break;
	}
	
	return ret_val;
}

void mipi_hal_write_interrupt_mask(enum dsi_module idx, enum dsi_intr int_idx, unsigned int val)
{
	switch(int_idx) {
	case DSI_INT0:
		dsi_dev.mipi_base[idx]->interrupt_mask0.af.mask0 = val;
		break;

	case DSI_INT1:
		dsi_dev.mipi_base[idx]->interrupt_mask1.af.mask1 = val;
		break;

	case DSI_INT2:
		dsi_dev.mipi_base[idx]->interrupt_mask2.af.mask2 = val;
		break;
	}
}

unsigned int mipi_hal_read_interrupt_clear(enum dsi_module idx, enum dsi_intr int_idx)
{
	unsigned int ret_val=0;

	switch(int_idx) {
	case DSI_INT0:
		ret_val = dsi_dev.mipi_base[idx]->interrupt_clear0.af.clr0;
		break;

	case DSI_INT1:
		ret_val = dsi_dev.mipi_base[idx]->interrupt_clear1.af.clr1;
		break;

	case DSI_INT2:
		ret_val = dsi_dev.mipi_base[idx]->interrupt_clear2.af.clr2;
		break;
	}
	
	return ret_val;
}

void mipi_hal_write_interrupt_clear(enum dsi_module idx, enum dsi_intr int_idx, unsigned int val)
{
	switch(int_idx) {
	case DSI_INT0:
		dsi_dev.mipi_base[idx]->interrupt_clear0.af.clr0 = val;
		break;

	case DSI_INT1:
		dsi_dev.mipi_base[idx]->interrupt_clear1.af.clr1 = val;
		break;

	case DSI_INT2:
		dsi_dev.mipi_base[idx]->interrupt_clear2.af.clr2 = val;
		break;
	}
}

void mipi_hal_write_config0_pixel_fmt(enum dsi_module idx, enum dsi_input_pixel_fmt in_pixel, enum dsi_output_pixel_fmt out_pixel)
{
	// Configuration0 - Inputformat - 1:RGB565
	dsi_dev.mipi_base[idx]->configuration0.af.input_format = in_pixel;
	// Configuration0 - Outputformat - 4:RGB24
	dsi_dev.mipi_base[idx]->configuration0.af.output_format = out_pixel;
}

void mipi_hal_write_config0_opr_mode(enum dsi_module idx, enum mode_data_path mode)
{
	// Configuration0 - datapath_mode - 0 = command mode datapath (cpu_mode) or 1 = video mode datapath(rgb_mode)
	dsi_dev.mipi_base[idx]->configuration0.af.datapath_mode = mode;
}

void mipi_hal_write_config0_turndisable(enum dsi_module idx, bool turndisable)
{
	//Configuration0 - TURNDISABLE0 - Disable Turn around for data lane 0.
	dsi_dev.mipi_base[idx]->configuration0.af.turndisable0= turndisable;
	//Configuration0 - TURNDISABLE1 - Disable Turn around for data lane 1.
 	dsi_dev.mipi_base[idx]->configuration0.af.turndisable1= 1;
	//Configuration0 - TURNDISABLE2 - Disable Turn around for data lane 2.
	dsi_dev.mipi_base[idx]->configuration0.af.turndisable2= 1;
	//Configuration0 - TURNDISABLE3 - Disable Turn around for data lane 3.
	dsi_dev.mipi_base[idx]->configuration0.af.turndisable3= 1;
}

void mipi_hal_write_config0_phy_lane_module(enum dsi_module idx, u8 lanes_num)
{
	dsi_dev.mipi_base[idx]->configuration0.af.phy_clk_lane_ena = 1;

	switch (lanes_num) {
	case 1:
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena0=1;
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena1=0;
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena2=0;
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena3=0;
		break;
	case 2:
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena0=1;
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena1=1;
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena2=0;
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena3=0;
		break;
	case 3:
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena0=1;
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena1=1;
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena2=1;
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena3=0;
		break;
	case 4:
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena0=1;
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena1=1;
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena2=1;
		dsi_dev.mipi_base[idx]->configuration0.af.phy_data_lane_ena3=1;
		break;
	}

}

void mipi_hal_write_config1_sel_data_lanes(enum dsi_module idx, u8 lp_num, u8 hs_num)
{
	dsi_dev.mipi_base[idx]->configuration1.af.lp_lane_sel= lp_num;
	dsi_dev.mipi_base[idx]->configuration1.af.hs_lane_sel= hs_num;
}

void mipi_hal_write_config2_dphy_shutdown(enum dsi_module idx, bool is_pd)
{
	// Configuration2 - shutdownz - DPHY Shutdown.
	// This line is used to place the complete macro in power down.
	// All analog blocks are in power down mode and digital logic is reset. Active Low
	if(is_pd)
		dsi_dev.mipi_base[idx]->configuration2.af.shutdownz = 0; // active low
	else
		dsi_dev.mipi_base[idx]->configuration2.af.shutdownz = 1;
}

void mipi_hal_write_config0_dphy_reset(enum dsi_module idx, bool is_reset)
{
	// Configuration0 - PHY_resetN - 0:PHY  is held in reset -> 1: PHY is unreset
	if(is_reset)
		dsi_dev.mipi_base[idx]->configuration0.af.phy_resetn= 0; // active low.
	else
		dsi_dev.mipi_base[idx]->configuration0.af.phy_resetn= 1;		
}

void mipi_hal_write_video_mode_desc(enum dsi_module idx, unsigned short hfp_cnt, unsigned short hsa_cnt, unsigned short bllp_cnt, unsigned short hbp_cnt,
									unsigned short vsa_lines, unsigned short vbp_lines, unsigned short vfp_lines, unsigned short vact_lines, unsigned short bytes_line,
									bool full_sync)
{
	//Video Timing2 - HFP_count - Count for timing the horizontal back porch. This the number of bytes in the blanking packet.
	dsi_dev.mipi_base[idx]->video_timing2.af.hfp_count=	hfp_cnt;
	//Video Timing1 - HSA_count - Count for timing the horizontal sync active. This the number of bytes in the blanking packet.
	dsi_dev.mipi_base[idx]->video_timing1.af.hsa_count=	hsa_cnt;
	//Video Timing1 - BLLP_count - Count for timing the blanking interval. This the number of bytes in the blanking packet.
	dsi_dev.mipi_base[idx]->video_timing1.af.bllp_count= bllp_cnt;
	//Video Timing2 - HBP_count - Count for timing the horizontal back porch. This the number of bytes in the blanking packet.
	dsi_dev.mipi_base[idx]->video_timing2.af.hbp_count=	hbp_cnt;
	//Video Config1 - Num_VSA_lines  - Number of vertical sync active lines.
	dsi_dev.mipi_base[idx]->video_config1.af.num_vsa_lines= vsa_lines;
	//Video Config1 - Num_VBP_lines - Number vertical back porch lines.
	dsi_dev.mipi_base[idx]->video_config1.af.num_vbp_lines= vbp_lines;
	//Video Config1 - Num_VFP_lines - Number vertical front porch lines.
	dsi_dev.mipi_base[idx]->video_config1.af.num_vfp_lines= vfp_lines;
	//Video Config1 - full_sync - Sets whether full sync or partial sync events are sent for each frame  0 = partial sync 1 = full sync.
	dsi_dev.mipi_base[idx]->video_config1.af.full_sync = full_sync;
	//Video Config2 - Num_VACT_lines - Number of vertical sync active lines.
	dsi_dev.mipi_base[idx]->video_config2.af.num_vact_lines= vact_lines;
	//Video Config2 - Num_bytes_line - Number of bytes in an active line. Based on the output_format
	//and how many pixels are in a line, software must calculate how many bytes there are in a line.
	dsi_dev.mipi_base[idx]->video_config2.af.num_bytes_line= bytes_line;
}

void mipi_hal_write_action(enum dsi_module idx, enum dsi_action dsi_act)
{
	switch(dsi_act) {
	case DSI_ACTION_CLK_LANE_ENTER_ULP:
		dsi_dev.mipi_base[idx]->action.af.clk_lane_enter_ulp = 1;
		break;
	case DSI_ACTION_CLK_LANE_EXIT_ULP:
		dsi_dev.mipi_base[idx]->action.af.clk_lane_exit_ulp = 1;
		break;
	case DSI_ACTION_CLK_LANE_ENTER_HS:
		dsi_dev.mipi_base[idx]->action.af.clk_lane_enter_hs = 1;
		break;
	case DSI_ACTION_CLK_LANE_EXIT_HS:
// This action will take the clk lane out of  the high speed state.
// The event has occurred when the interrupt source has been asserted.
// Ensure any high speed data transfers are complete before exiting the clk high speed state.
		dsi_dev.mipi_base[idx]->action.af.clk_lane_exit_hs = 1;
		break;
	case DSI_ACTION_DATA_LANE_ENTER_ULP:
		dsi_dev.mipi_base[idx]->action.af.data_lane_enter_ulp = 1;
		break;
	case DSI_ACTION_DATA_LANE_EXIT_ULP:
		dsi_dev.mipi_base[idx]->action.af.data_lane_exit_ulp = 1;
		break;
	case DSI_ACTION_FORCE_DATA_LANE_STOP:
		dsi_dev.mipi_base[idx]->action.af.force_data_lane_stop = 1;
		break;
	case DSI_ACTION_TURN_REQUEST:
		dsi_dev.mipi_base[idx]->action.af.turn_request = 1;
		break;
	case DSI_ACTION_FLUSH_RX_FIFO0:
		dsi_dev.mipi_base[idx]->action.af.flush_rx_fifo0 = 1;
		break;
	case DSI_ACTION_FLUSH_CFG_FIFO:
//  Action - flush_cfg_fifo
//  Configuration fifo is used during mode 0. operation when configuration information
//  is being sent to the display.  only long packets make use of this fifo.
//  When a long packet is being sent, the hardware waits until the fifo contains
//  the entire payload before it starts sending the command.
		dsi_dev.mipi_base[idx]->action.af.flush_cfg_fifo = 1;
		break;
	case DSI_ACTION_FLUSH_DATA_FIFO:
		dsi_dev.mipi_base[idx]->action.af.flush_data_fifo = 1;
		break;
	case DSI_ACTION_STOP_VIDEO_DATA:
		dsi_dev.mipi_base[idx]->action.af.stop_video_data = 1;
		break;
	}
}

u8 mipi_hal_read_status_field(enum dsi_module idx, enum dsi_status field )
{
	u8 ret_val;
	unsigned int reg_val;

	reg_val = dsi_dev.mipi_base[idx]->status.a32;

	ret_val = (reg_val & field )? 1:0;

	return ret_val;
}

void mipi_hal_write_hs_packet(enum dsi_module idx, unsigned char dataID, unsigned short data, bool hs_enter_BTA_skip,
	                              bool enable_Eotp, enum hs_data_path mode, enum packet_struct packet_type)
{
	hs_packet  hs_pkt;

	hs_pkt = dsi_dev.mipi_base[idx]->hs_packet;
	
	hs_pkt.af.mode = mode;
	hs_pkt.af.data = data;
	hs_pkt.af.hs_enter_bta_skip = hs_enter_BTA_skip;
	hs_pkt.af.enable_eotp = enable_Eotp;
	hs_pkt.af.mode = mode;
	hs_pkt.af.packet_type = packet_type;

	dsi_dev.mipi_base[idx]->hs_packet = hs_pkt;
}
void mipi_hal_write_hs_packet_mode(enum dsi_module idx, enum hs_data_path mode)
{
	hs_packet  reg_val;

	reg_val = dsi_dev.mipi_base[idx]->hs_packet;

	reg_val.af.mode = mode;
	reg_val.af.hs_enter_bta_skip = true;

	dsi_dev.mipi_base[idx]->hs_packet = reg_val;
}

void mipi_hal_write_escape(enum dsi_module idx, u8 dataID, u8 data0, u8 data1, u8 data)
{
	escape_mode_packet es_pkt;

	es_pkt.af.dataid = dataID;
	es_pkt.af.data0  = data0;
	es_pkt.af.data1  = data1;
	es_pkt.af.data	 = data;

	dsi_dev.mipi_base[idx]->escape_mode_packet = es_pkt;
}

void mipi_hal_write_config0_lp_escape(enum dsi_module idx, bool lp_esc )
{
	// Configuration0 - long packet_escape
	// - This field is only pertinent when doing escape data transmissions (see escape mode packet register).
	configuration0 reg_val = dsi_dev.mipi_base[idx]->configuration0;
	
	reg_val.af.long_packet_escape = lp_esc;

	dsi_dev.mipi_base[idx]->configuration0 = reg_val;
}

void mipi_hal_write_command_cfg(enum dsi_module idx, unsigned short num_bytes, bool wait_fifo)
{
	//    Command Config - num_bytes - Number of bytes that the data fifo has to have before the HS packet is initiated.
	//    This field is only applicable if wait_fifo = 1. The size of the fifo is 512x32.
	dsi_dev.mipi_base[idx]->command_config.af.num_bytes = num_bytes;

	//    Command Config - wait_fifo - If latency is an issue, hardware can be configured to wait
	//    until the fifo contains num_bytes amount of bytes before initiating the packet transfer.
	//    Otherwise, as soon as the fifo contains a pixel, the packet will start to be transmitted.
	//    0 = don't wait 1 = wait until fifo has num_bytes worth of data.
	dsi_dev.mipi_base[idx]->command_config.af.wait_fifo= wait_fifo;
}

unsigned int mipi_hal_write_tif (enum dsi_module dsi_idx, u32 addr, u8 data)
{
	u32 read_val;

	/* set Test code*/
	dsi_dev.mipi_base[dsi_idx]->test_clock.af.testclk = 1;      /* 0x74*/
	dsi_dev.mipi_base[dsi_idx]->test_data_in.af.testdin = addr; /* 0x78*/
	dsi_dev.mipi_base[dsi_idx]->test_data_enable.af.testen = 1; /* 0x7C*/
	dsi_dev.mipi_base[dsi_idx]->test_clock.af.testclk = 0;      /* 0x74*/
	dsi_dev.mipi_base[dsi_idx]->test_clock.af.testclk = 1;      /* 0x74*/
	dsi_dev.mipi_base[dsi_idx]->test_data_enable.af.testen = 0; /* 0x7C*/

	/* set Test data*/
	dsi_dev.mipi_base[dsi_idx]->test_clock.af.testclk = 0;      /* 0x74*/
	dsi_dev.mipi_base[dsi_idx]->test_data_in.af.testdin = data; /* 0x78*/
	dsi_dev.mipi_base[dsi_idx]->test_clock.af.testclk = 0;      /* 0x74*/
	dsi_dev.mipi_base[dsi_idx]->test_clock.af.testclk = 1;      /* 0x74*/

	/* set Test code(read data)*/
	dsi_dev.mipi_base[dsi_idx]->test_clock.af.testclk = 1;      /* 0x74*/
	dsi_dev.mipi_base[dsi_idx]->test_data_in.af.testdin = addr; /* 0x78*/
	dsi_dev.mipi_base[dsi_idx]->test_data_enable.af.testen = 1; /* 0x7C*/
	dsi_dev.mipi_base[dsi_idx]->test_clock.af.testclk = 0;      /* 0x74*/
	dsi_dev.mipi_base[dsi_idx]->test_clock.af.testclk = 1;      /* 0x74*/
	dsi_dev.mipi_base[dsi_idx]->test_data_enable.af.testen = 0; /* 0x7C*/

	read_val = dsi_dev.mipi_base[dsi_idx]->test_data_out.af.testdout;

	if (data == dsi_dev.mipi_base[dsi_idx]->test_data_out.a32 /* 0x80*/) {
		return  0;
	}
	else {
		pr_err("\t-->> MIPI DSI D-PHY test I/F fail\n");
		return -1;
	}
}

void mipi_hal_write_tif_testclr(enum dsi_module idx, bool is_hi)
{
	dsi_dev.mipi_base[idx]->test_clear.af.testclr = is_hi;  /* 0x70*/
}

void mipi_hal_set_base_addr(enum dsi_module dsi_num, unsigned int lcd_base, unsigned int mipi_base)
{
	dsi_dev.dip_base[dsi_num]= (volatile struct dip_register *)lcd_base;
	dsi_dev.mipi_base[dsi_num]= (volatile DSS_DIP_MIPI_MIPI_REG_T *)mipi_base;
}

