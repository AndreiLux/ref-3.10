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

#ifndef _H_MIPI_DSI_DRV
#define _H_MIPI_DSI_DRV

/// DSI-compliant peripherals support either of two basic modes of operation.
enum dsi_operation_mode {
	DSI_VIDEO_MODE,
	DSI_COMMAND_MODE,
};

/// MIPI D-PHY operating mode : Control, High-Sppend, and Escape
enum dsi_dphy_op_mode {
	DPHY_HS_MODE,
	DPHY_ESCAPE,
};

enum payload_type {
	PAYLOAD_IMAGE_DATA,
	PAYLOAD_COMMAND,
};

/// - Non-Burst Mode with Sync Pulses – enables the peripheral to accurately
/// reconstruct original video timing, including sync pulse widths.
/// - Non-Burst Mode with Sync Events – similar to above, but accurate
/// reconstruction of sync pulse widths is not required,
///  so a single Sync Event is substituted.
/// - Burst mode – RGB pixel packets are time-compressed, leaving more time
/// during a scan line for LP mode (saving power) or for multiplexing
/// other transmissions onto the DSI link.
enum video_mode_fmt {
	NON_BURST_MODE_WITH_SYNC_PLUSES,
	NON_BURST_MODE_WITH_SYNC_EVENTS,
	BURST_MODE,
};

/// Processor-to-Peripheral Transcations  data type
enum packet_data_type {
	SYNC_EVENT_VSYNC_START = 0x01,
	SYNC_EVENT_VSYNC_END = 0x11,
	SYNC_EVENT_HSYNC_START = 0x21,
	SYNC_ENVET_HSYNC_END = 0x31,
	END_OF_TRANSMISSION_PACKET = 0x8,
	COLOR_MODE_OFF_COMMAND = 0x02,
	COLOR_MODE_ON_COMMAND = 0x12,
	SHUT_DOWN_PERIPHERAL_COMMAND = 0x22,
	TURN_ON_PERIPHERAL_COMMAND = 0x32,
	GENERIC_SHORT_WRITE_NO_PARAMETERS = 0x03,
	GENERIC_SHORT_WRITE_1_PARAMETER = 0x13,
	GENERIC_SHORT_WRITE_2_PARAMETERS = 0x23,
	GENERIC_READ_NO_PARAMETERS = 0x04,
	GENERIC_READ_1_PARAMETER = 0x14,
	GENERIC_READ_2_PARAMETERS = 0x24,
	DCS_SHORT_WRITE_NO_PARAMETERS = 0x05,
	DCS_SHORT_WRITE_1_PARAMETER = 0x15,
	DCS_READ_NO_PARAMETERS = 0x06,
	SET_MAXIMUM_RETURN_PACKET_SIZE = 0x37,
	NULL_PACKET_NO_DATA = 0x09,
	BLANKING_PACKET_NO_DATA = 0x19,
	GENERIC_LONG_WRITE = 0x29,
	DCS_LONG_WRITE_WRITE_LUT_COMMAND_PACKET = 0x39,
	PACKED_PIXEL_STREAM_16BIT_RGB565_FORMAT = 0x0E,
	PACKED_PIXEL_STREAM_18BIT_RGB666_FORMAT = 0x1E,
	LOOSELY_PACKED_PIXEL_STREAM_18BIT_RGB666_FORMAT = 0x2E,
	PACKED_PIXEL_STEAM_24BIT_RGB888_FORMAT = 0x3E,
};

/// needed for calculating transmission packets
///(HSA,BLLP,HBP,HFP,VSA,VBP,VFP,VAT,... ) and D-PHY byte clock for video mode.
struct video_timing {
	unsigned int x_res;
	unsigned int y_res;
	unsigned int height;
	unsigned int width;
	unsigned int pixel_clock;
	unsigned int data_width;
	unsigned char bpp;
	unsigned int hsw;
	unsigned int hfp;
	unsigned int hbp;
	unsigned int vsw;
	unsigned int vfp;
	unsigned int vbp;
	bool blank_pol;
	bool pclk_pol;
	bool vsync_pol;
	bool hsync_pol;
};


struct mipi_dsi_desc {
	enum dsi_input_pixel_fmt in_pixel_fmt;
	/// Since a host processor may implement both Command and Video Modes of
	/// operations, it should support bidirectional operation and
	/// Escape Mode transmission and reception.
	bool bidirectinal;

	/// DSI device's capabilities and parameters

	enum dsi_operation_mode dsi_op_mode;
	enum dsi_output_pixel_fmt out_pixel_fmt;
	enum video_mode_fmt vm_fmt;
	struct video_timing vm_timing;

	/// ECC capability for  dsi packet header.
	bool ecc;
	/// CHECKSUM capability for dsi packet payload data.
	bool checksum;
	/// EoTp(End of Transmission Packet) capability
	bool eotp;
	unsigned char num_of_lanes;
	/// maximum lane frequency documented by DSI deivce manufacturer.
	unsigned int max_lane_freq;	//ex. 1G bps/lane - value :1000
	/// Multiple Peripheral support
	unsigned char max_num_of_vc;	//vc(virtual channel)
};

struct packet_header {
	enum packet_data_type di;
	union {
		struct {
			unsigned char data0;
			unsigned char data1;
		} sp;		// short packet
		struct {
			unsigned short wc;	// 16-bit word count: convey how many words(bytes)
		} lp;		// long packet
	} packet_data;
};

struct dsi_packet {
	enum dsi_dphy_op_mode escape_or_hs;
	struct packet_header ph;
	unsigned char *pPayload;
};

bool mipi_dsi_hal_send(enum dsi_module dsi_num, struct dsi_packet *pPkt);
bool mipi_dsi_hal_start_video_mode(enum dsi_module dsi_num);
bool mipi_dsi_hal_stop_video_mode(enum dsi_module dsi_num);

bool mipi_dsi_hal_open(enum dsi_module dsi_num,
		struct mipi_dsi_desc *dsi_desc);
bool mipi_dsi_hal_close(enum dsi_module dsi_num);

void mipi_dsi_hal_init(enum dsi_module dsi_num, unsigned int lcd_base,
		unsigned int mipi_base, bool is_active);
void mipi_dsi_hal_cleanup(enum dsi_module dsi_num);
#endif /* _H_MIPI_DSI_DRV */
