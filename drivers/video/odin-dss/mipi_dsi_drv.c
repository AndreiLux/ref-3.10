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
#define DEBUG
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>

#include "hal/mipi_dsi_common.h"
#include "hal/mipi_dsi_hal.h"
#include "mipi_dsi_drv.h"
#include "dss_irq.h"

// for debug
//#define ROP_TEST
#ifdef ROP_TEST
#include "hal/reg_def/ovl_union_reg.h"
#include "hal/dss_hal_ch_id.h"
#include "hal/du_hal.h"
#include "hal/ovl_hal.h"
#endif

/*------------------------------------------------------------------------------
  Constant Definitions
  ------------------------------------------------------------------------------*/
char *irq0_message[] =
{
	"hs_cfg_complete",
	"esc_complete",
	"hs_data_complete",
	"frame_complete",
	"rx_complete",
	"rxdata_complete",
	"rxtrigger0",
	"rxtrigger1",
	"rxtrigger2",
	"rxtrigger3",
	"txcfg_fifo_ur_err",
	"txdata_fifo_ur_err",
	"rxdata_fifo_ur_err",
	"rx_ecc_err1_l0",
	"rx_ecc_err2_l0",
	"rx_chksum_err_l0",
	"PHY_err_esc_l0",
	"PHY_syncerr_esc_l0",
	"PHY_err_control_l0",
	"PHY_err_cont_lp0_l0",
	"PHY_err_cont_lp1_l0",
	"clk_lane_enter_HS",
	"clk_lane_exit_HS",
	"rx_ecc_err1_l1",
	"rx_ecc_err2_l1",
	"rx_chksum_err_l1",
	"PHY_err_esc_l1",
	"PHY_syncerr_esc_l1",
	"PHY_err_control_l1",
	"PHY_err_cont_lp0_l1",
	"PHY_err_cont_lp1_l1",
	"reserved"
};

char *irq1_message[] =
{
	"Rx_complete_L1",
	"Rxdata_complete_L1",
	"Rxtrigger0_L1",
	"Rxtrigger1_L1",
	"Rxtrigger2_L1",
	"Rxtrigger3_L1",
	"rxdata_fifo_err_L1",
	"Rx_complete_L2",
	"Rxdata_complete_L2",
	"Rxtrigger0_L2",
	"Rxtrigger1_L2",
	"Rxtrigger2_L2",
	"Rxtrigger3_L2",
	"rxdata_fifo_err_L2",
	"Rx_complete_L3",
	"Rxdata_complete_L3",
	"Rxtrigger0_L3",
	"Rxtrigger1_L3",
	"Rxtrigger2_L3",
	"Rxtrigger3_L3",
	"rxdata_fifo_err_L3",
	"xcfg_fifo_or_err",
	"txdata_fifo_or_err",
	"reserved"
};

static char *api_state_message[] =
{
	"DSI_STATE_NOT_READY",
	"DSI_STATE_READY",
	"DSI_STATE_SENDING_PACKET",
	"DSI_STATE_VIDEO_MODE",
};

unsigned int irq0_err_bit = 0x0;
unsigned int irq0_err_cnt = 0x0;

enum dsi_state dsi_api_state;

/*------------------------------------------------------------------------------
  Macro Definitions
  ------------------------------------------------------------------------------*/
#define IRQ_DEBUG		1

#define MAX_INT0_SOURCE		31
#define MAX_INT1_SOURCE		23

/*------------------------------------------------------------------------------
  Type Definitions
  ------------------------------------------------------------------------------*/

struct vm_timing_info
{
	unsigned short  hsa_count;
	unsigned short  hfp_count;
	unsigned short  hbp_count;
	unsigned short  bllp_count;

	unsigned short  num_vsa_lines;
	unsigned short  num_vfp_lines;
	unsigned short  num_vbp_lines;

	unsigned short  num_bytes_line;
	unsigned short  num_vact_lines;

	unsigned short  full_sync;
};


enum dsi_state
{
	DSI_STATE_NOT_READY,
	DSI_STATE_READY,
	DSI_STATE_SENDING_PACKET,
	DSI_STATE_VIDEO_MODE,
};

/*------------------------------------------------------------------------------
  HIGH-LEVEL  Function Prototypes Definitions
  ------------------------------------------------------------------------------*/

#ifdef ROP_TEST
void rop_test(void)
{
	struct dss_image_size img_size;
	struct dss_image_pos  img_pos;

	img_size.h= 1920;
	img_size.w= 1080;
	img_pos.x = 0;
	img_pos.y = 0;

	ovl_hal_enable(OVL_CH_ID0, img_size, false, true, 0xc, 0x00, 0x00, 0x00);
}
#endif

void _dip0_isr(enum dss_irq irq, void *arg)
{
	unsigned int irq_pending;

	irq_pending = dip_hal_read_interrupt_source(MIPI_DSI_DIP0);
	
	pr_info("%s : 0x%x\n" ,__func__,irq_pending);

	dip_hal_write_interrupt_clear(MIPI_DSI_DIP0, 0xFFFFFFFF); 
}

void _mipi0_isr(enum dss_irq irq, void *arg)
{
	unsigned int irq_pending, index;

	irq_pending = mipi_hal_read_interrupt_source(MIPI_DSI_DIP0, DSI_INT0);

    /* print log for only err case */
	for (index = 10; index < MAX_INT0_SOURCE; index++){
		if (irq_pending & (1 << index)) {
			if (irq0_err_bit & (1 << index)) {
				irq0_err_cnt++;
			}
			else {
				irq0_err_bit = (1 << index);
				irq0_err_cnt = 0;
			}

			if ((irq0_err_cnt % 0x1000) == 0x0)
				pr_err("%s : %s, 0x%X, 0x%X\n", __func__, irq0_message[index], irq0_err_bit, irq0_err_cnt);

			continue;
		}
	}
	
	mipi_hal_write_interrupt_clear(MIPI_DSI_DIP0, DSI_INT0, 0xFFFFFFFF);

	irq_pending = mipi_hal_read_interrupt_source(MIPI_DSI_DIP0, DSI_INT1);

	for (index = 0; index < MAX_INT1_SOURCE; index++){
		if (irq_pending & (1 << index))
			pr_info("%s : %s\n", __func__, irq1_message[index]);
	}

	mipi_hal_write_interrupt_clear(MIPI_DSI_DIP0, DSI_INT1, 0xFFFFFFFF);
	
}

/*------------------------------------------------------------------------------
  LOW-LEVEL  Function Prototypes Definitions (internal)
  ------------------------------------------------------------------------------*/

/**

  convert panel video timing info  to dsi video mode timing info.

  The _mipi_dsi_calc_video_timing  should follow these comments.

  @param[out]  dsi_time  video mode timing info using MIPI-DSI.

  @param[in]    panel_time  panel video timing info using panel device.

  @return      the result of operation (true: sucess, false: failure)

 */
void _mipi_dsi_calc_video_timing(struct vm_timing_info *dsi_time, struct video_timing *panel_time)
{
	unsigned int  psize;
	unsigned int  cnthsa, cnthfp, cnthbp, cntbllp;

	switch (panel_time->data_width) {
	case 24:
	case 18:
		psize = 3;
		break;
	case 16:
		psize = 2;
		break;
	default:
		psize = 3;
		break;
	}

	cnthsa = panel_time->hsw * psize;
	cnthfp = panel_time->hfp * psize;
	cnthbp = panel_time->hbp * psize;
	cntbllp = (cnthbp + 6) + (panel_time->x_res * psize + 6) + (cnthfp + 6);

	dsi_time->hsa_count = cnthsa - 6;
	dsi_time->hfp_count = cnthfp - 6;
	dsi_time->hbp_count = cnthbp - 6;
	dsi_time->bllp_count = cntbllp - 6;

	dsi_time->num_vsa_lines = panel_time->vsw;
	dsi_time->num_vfp_lines = panel_time->vfp;
	dsi_time->num_vbp_lines = panel_time->vbp;

	dsi_time->num_bytes_line = panel_time->x_res * psize;
	dsi_time->num_vact_lines = panel_time->y_res;
	dsi_time->full_sync = 1;
}


/**

  set D-PHY clock using pixel clock of the panel video timing.

  The _mipi_dsi_dphy_set_byteclock should follow these comments.

  @param[in]   pixel_clock  pixel clock the panel video timing.

  @return      the result of operation (true: sucess, false: failure)

 */
void _mipi_dsi_dphy_set_byteclock(enum dsi_module idx, unsigned int pixel_clock)
{
	mipi_hal_write_tif_testclr(idx,true); /* 0x70*/
	mipi_hal_write_tif_testclr(idx,false); /* 0x70*/

	mipi_hal_write_tif(idx, 0x10, 0x00);
	mipi_hal_write_tif(idx, 0xF0, 0x00);
	mipi_hal_write_tif(idx, 0xF1, 0x10);

	if ((pixel_clock > 147000) && (pixel_clock < 149000))
	{
		pr_info("set to MIPI Phy Clk 888 MHz \n");
		mipi_hal_write_tif(idx, 0xF2, 0x00);
		mipi_hal_write_tif(idx, 0xF3, 0x08);
		mipi_hal_write_tif(idx, 0xF4, 0x05);
		mipi_hal_write_tif(idx, 0xF5, 0x00);
		mipi_hal_write_tif(idx, 0xF6, 0x00);
	}
	else
	{
		pr_info("set to MIPI Phy Clk 864 MHz \n");
		mipi_hal_write_tif(idx, 0xF2, 0x00);
		mipi_hal_write_tif(idx, 0xF3, 0x08);
		mipi_hal_write_tif(idx, 0xF4, 0x04);
		mipi_hal_write_tif(idx, 0xF5, 0x00);
		mipi_hal_write_tif(idx, 0xF6, 0x00);
	}

	mipi_hal_write_tif(idx, 0x34, 0x03);
	mipi_hal_write_tif(idx, 0x01, 0x02);
	mipi_hal_write_tif(idx, 0x34, 0x03);
	mipi_hal_write_tif(idx, 0x01, 0x03);
	mipi_hal_write_tif(idx, 0xF7, 0x01); /* pll cal */
	mipi_hal_write_tif(idx, 0x14, 0x0f); /* THS-ZERO */
	mipi_hal_write_tif(idx, 0x16, 0x10); /* THS-PREPARE */
	mipi_hal_write_tif(idx, 0x20, 0x01);
	mipi_hal_write_tif(idx, 0x35, 0x2a); /* clk */
	mipi_hal_write_tif(idx, 0x45, 0x2a); /* ch0 */
	mipi_hal_write_tif(idx, 0x55, 0x2a); /* ch1 */
	mipi_hal_write_tif(idx, 0x65, 0x2a); /* ch2 */
	mipi_hal_write_tif(idx, 0x75, 0x2a); /* ch3 */

#if 1
	while(mipi_hal_read_status_field(idx, _DSI_STATUS_MIPI_PLL_LOCK) == 0) {
		pr_info("waiting for mipi_pll_lock !!! \n");
		mdelay(1);
	}
#else
	mdelay(100);
#endif
	/* Now it's D-PHY LP11 state.*/
}

/**
  The _mipi_dsi_get_packet_struct should follow these comments.

  @param[in]

  @return      the result of operation (true: sucess, false: failure)

 */
static enum packet_struct _mipi_dsi_get_packet_struct(enum packet_data_type di)
{
	enum packet_struct pkt_struct;

	switch (di) {
	case SYNC_EVENT_VSYNC_START:
	case SYNC_EVENT_VSYNC_END:
	case SYNC_EVENT_HSYNC_START:
	case SYNC_ENVET_HSYNC_END:
	case END_OF_TRANSMISSION_PACKET:
	case COLOR_MODE_OFF_COMMAND:
	case COLOR_MODE_ON_COMMAND:
	case SHUT_DOWN_PERIPHERAL_COMMAND:
	case TURN_ON_PERIPHERAL_COMMAND:
	case GENERIC_SHORT_WRITE_NO_PARAMETERS:
	case GENERIC_SHORT_WRITE_1_PARAMETER:
	case GENERIC_SHORT_WRITE_2_PARAMETERS:
	case GENERIC_READ_NO_PARAMETERS:
	case GENERIC_READ_1_PARAMETER:
	case GENERIC_READ_2_PARAMETERS:
	case DCS_SHORT_WRITE_NO_PARAMETERS:
	case DCS_SHORT_WRITE_1_PARAMETER:
	case DCS_READ_NO_PARAMETERS:
		pkt_struct = SHORT_PACKET;
		break;
	case DCS_LONG_WRITE_WRITE_LUT_COMMAND_PACKET:
	case SET_MAXIMUM_RETURN_PACKET_SIZE:
	case NULL_PACKET_NO_DATA:
	case BLANKING_PACKET_NO_DATA:
	case GENERIC_LONG_WRITE:
	case PACKED_PIXEL_STREAM_16BIT_RGB565_FORMAT:
	case PACKED_PIXEL_STREAM_18BIT_RGB666_FORMAT:
	case LOOSELY_PACKED_PIXEL_STREAM_18BIT_RGB666_FORMAT:
	case PACKED_PIXEL_STEAM_24BIT_RGB888_FORMAT:
		pkt_struct = LONG_PACKET;
		break;
	default:
		pr_err("unknown packet data type !!!");
		pkt_struct = INVALID_PACKET;
		break;
	}

	return pkt_struct;
}

/**

  put dphy operating modes(HS(High Speed), LP(Low-Power), ULPS)

  The _mipi_dsi_enter_dphy_opr_mode should follow these comments.

  @param[in]   op_mode dphy operating mode

  @return      the result of operation (true: sucess, false: failure)

 */
static bool _mipi_dsi_enter_dphy_opr_mode(enum dsi_module dsi_idx, enum dsi_dphy_op_mode  op_mode)
{
	switch (op_mode) {
		// HS mode
		case DPHY_HS_MODE:
			mipi_hal_write_action(dsi_idx, DSI_ACTION_CLK_LANE_ENTER_HS);
		// pr_info("dphy op mode: DPHY_HS_MODE");
		break;
		// LP mode
		case DPHY_ESCAPE:
			mipi_hal_write_action(dsi_idx, DSI_ACTION_CLK_LANE_EXIT_HS);
		// nothing to do..
		// pr_info("dphy op mode: DPHY_ESCAPE");
		break;
	}
	return true;
}

/**
  write to Escape Mode packet Register.

  The _mipi_dsi_load_esc_mode_pkt should follow these comments.

  @param[in]   dataID   Data Identifier Byte
  @param[in]   data0    Packet byte 0
  @param[in]   data1    Packet byte 1
  @param[in]   data      Ignored for packet header(ECC)
  @return      none

 */
void _mipi_dsi_load_esc_mode_pkt(enum dsi_module idx, u8 dataID, u8 data0, u8 data1, u8 data)
{
	int try_cnt=0;
	
	mipi_hal_write_escape(idx, dataID, data0, data1, data);

	// Status - escape_ready - Indicates that the escape mode command register
	// can be written again.
	while (mipi_hal_read_status_field(idx, _DSI_STATUS_ESCAPE_READY) == 0) {
		if (try_cnt >= 10) {
			pr_info("%s: DSI_STATUS_ESCAPE_READY not yet cnt over!! \n",__func__);
			break;
		}
	
		mdelay(1);
 		try_cnt++;
  	}
}

static bool _mipi_dsi_check_dsi_state(enum dsi_state state)
{
	if (dsi_api_state == state)
		return true;
	else
		return false;
}

static void _mipi_dsi_change_dsi_state(enum dsi_state state)
{
	dsi_api_state = state;
}

static char* _mipi_dsi_get_dsi_state_mesg(void)
{
	return api_state_message[dsi_api_state];
}
bool mipi_dsi_hal_start_video_mode(enum dsi_module dsi_idx)
{
	bool ret= true;
	
	pr_info("%s: B \n",__func__);

	if (!_mipi_dsi_check_dsi_state(DSI_STATE_READY)) {
		pr_err("%s: hal api state : Invalid(Current: %s, Needed: DSI_STATE_READY)\n", __func__,_mipi_dsi_get_dsi_state_mesg());
		ret = false;
		goto err_api_state;
	}

	_mipi_dsi_enter_dphy_opr_mode(dsi_idx, DPHY_HS_MODE);

	mipi_hal_write_action(dsi_idx, DSI_ACTION_FLUSH_DATA_FIFO);

	mipi_hal_write_config0_opr_mode(dsi_idx, VIDEO_MODE_DATAPATH);

	mipi_hal_write_command_cfg(dsi_idx, 512, true);


	mipi_hal_write_hs_packet_mode(dsi_idx, MODE1_DATA_PATH);

	pr_info("%s: E \n",__func__);

	_mipi_dsi_change_dsi_state(DSI_STATE_VIDEO_MODE);

err_api_state:
	
	return true;
}

/**

  For video mode, continuous sync and blanking packets stop to be sent
  along with the data in a timed fashion required by the display
  (much like parallel RGB displays).

  The mipi_dsi_hal_pkt_send  should follow these comments.

  @param[in]   dsi_idx  MIPI-DSI module index(ex. MIPI_DSI_DIP0)

  @return      the result of operation (true: sucess, false: failure)

 */
bool mipi_dsi_hal_stop_video_mode(enum dsi_module dsi_idx)
{
	bool ret= true;
	unsigned int try_cnt= 0;

	pr_info("%s: B \n",__func__);

	if (!_mipi_dsi_check_dsi_state(DSI_STATE_VIDEO_MODE)) {
		pr_err("%s: hal api state : Invalid(Current: %s, Needed: DSI_STATE_VIDEO_MODE)\n", __func__,_mipi_dsi_get_dsi_state_mesg());
		ret = false;
		goto err_api_state;
	}

	mipi_hal_write_action(dsi_idx, DSI_ACTION_STOP_VIDEO_DATA);

	try_cnt=0;

	while(mipi_hal_read_status_field(dsi_idx, _DSI_STATUS_PHY_DATA_STATUS) == 0) {
		
		if (try_cnt >= 20) {
			pr_info("Time out wait unitl DPHY CLK STATUS is not active. \n");
			break;
		}

		try_cnt++;
		mdelay(1);
	}
	
	mipi_hal_write_config0_opr_mode(dsi_idx, CMD_MODE_DATAPATH);

	mipi_hal_write_action(dsi_idx, DSI_ACTION_FLUSH_DATA_FIFO);

    _mipi_dsi_enter_dphy_opr_mode(dsi_idx, DPHY_ESCAPE);

	pr_info("%s: E \n",__func__);

	_mipi_dsi_change_dsi_state(DSI_STATE_READY);

err_api_state:
	
	return ret;
}

/**

  composite DSI packet and send it to Peripheral.

  The mipi_dsi_hal_pkt_send  should follow these comments.

  @param[in]   dsi_idx  MIPI-DSI module index(ex. MIPI_DSI_DIP0)

  @param[out]  pPkt  datas for compositing DSI packet.

  @return      the result of operation (true: sucess, false: failure)

 */
bool mipi_dsi_hal_send(enum dsi_module dsi_idx, struct dsi_packet *pPkt)
{
	int i, j;
	bool ret= true;
	unsigned short wc;
	unsigned char data[4];
	enum packet_struct pkt_struct;

	//    pr_info("%s: B \n",__func__);
	if (!_mipi_dsi_check_dsi_state(DSI_STATE_READY)) {
		pr_err("%s: hal api state : Invalid(Current: %s, Needed: DSI_STATE_READY)\n", __func__,_mipi_dsi_get_dsi_state_mesg());
		ret = false;
		goto err_api_state;
	}

	_mipi_dsi_change_dsi_state(DSI_STATE_SENDING_PACKET);

	pkt_struct = _mipi_dsi_get_packet_struct(pPkt->ph.di);

	if (pkt_struct == INVALID_PACKET) {
		pr_info("%s:packet data identifier is not valid!! \n",__func__);
		ret = false;
		goto err;
	}

	//  MIPI D-PHY operating mode : Control, High-Speed, and Escape.
	switch (pPkt->escape_or_hs) {
		case DPHY_HS_MODE:
			pr_err("%s: not support \n",__func__);
		break;
	case DPHY_ESCAPE:

  		switch (pkt_struct) {
		case SHORT_PACKET:
			mipi_hal_write_config0_lp_escape(dsi_idx, false);

			data[0] =  pPkt->ph.di;
			data[1] =  pPkt->ph.packet_data.sp.data0; // data[0]
			data[2] =  pPkt->ph.packet_data.sp.data1; // data[1]
			data[3] =  0;
			_mipi_dsi_load_esc_mode_pkt(dsi_idx, data[0], data[1], data[2], data[3]);
			break;
		case LONG_PACKET:
			mipi_hal_write_config0_lp_escape(dsi_idx, true);

			// Escape Mode packet header - DataID Short packet(Header):Data Identifier Byte.
			data[0] =  pPkt->ph.di;
			wc = pPkt->ph.packet_data.lp.wc;

			// Escape Mode packet header - Data0  byte count LSB (WORD COUNT)
			data[1]= wc & 0xff;
			// Escape Mode packet header - Data1  byte count MSB (WORD COUNT)
			data[2]= wc >> 8 & 0xff;
			data[3] = 0;

//              pr_info("%s: S long data0:%x, data1: %x, data2: %x, data3: %x \n",__func__, data[0],data[1],data[2],data[3]);
			_mipi_dsi_load_esc_mode_pkt(dsi_idx, data[0], data[1], data[2], data[3]);

			data[0]= data[1]= data[2] = data[3]= 0;

			for (i = 1, j=0 ; i <= wc ; i++) {
				// Escape Mode packet - Packet Data
				data[j++] = *pPkt->pPayload++;
				if ((j % 4 == 0) || (i == wc )) {
					j = 0;
//                    pr_info("%s: C long data0:%x, data1: %x, data2: %x, data3: %x \n",__func__, data[0],data[1],data[2],data[3]);
					_mipi_dsi_load_esc_mode_pkt(dsi_idx, data[0], data[1], data[2], data[3]);
					data[0]= data[1]= data[2] = data[3]= 0;
				}
			}
			break;
		default:
			pr_err("%s: DPHY_ESCAPE  INVALID_PACKET \n",__func__);
			break;
		}

 	}

 	//   pr_info("%s: E \n",__func__);
err:
	_mipi_dsi_change_dsi_state(DSI_STATE_READY);
err_api_state:
	return ret;
}


/**

  read  DSI devices's capabilities and parameters from Peripheral
  and initialize MIPI-DSI module.

  The mipi_dsi_hal_open should follow these comments.

  @param[in]   dsi_ctrl_num  MIPI-DSI module index(ex. MIPI_DSI_DIP0)

  @param[out]  pPeri  MIPI-DSI module info for setting MIPI-DSI AP side.

  @return      the result of operation (true: sucess, false: failure)

 */
bool mipi_dsi_hal_open(enum dsi_module dsi_idx, struct mipi_dsi_desc* dsi_desc)
{
	bool ret= true;
	struct vm_timing_info vm_timing;

	if (!_mipi_dsi_check_dsi_state(DSI_STATE_NOT_READY)) {
		pr_err("%s: hal api state : Invalid(Current: %s, Needed: DSI_STATE_NOT_READY)\n", __func__,_mipi_dsi_get_dsi_state_mesg());
		ret = false;
		goto err_api_state;
	}
#ifdef ROP_TEST
	rop_test();
#endif
	dip_hal_write_mipi_path_params(dsi_idx, true, true);

	mipi_hal_write_interrupt_mask(dsi_idx, DSI_INT0, 0x0);
	mipi_hal_write_interrupt_mask(dsi_idx, DSI_INT1, 0x0);
	mipi_hal_write_interrupt_clear(dsi_idx,DSI_INT0, 0xFFFFFFFF);
	mipi_hal_write_interrupt_clear(dsi_idx,DSI_INT1, 0xFFFFFFFF);
	
	if (IRQ_DEBUG) {
		dss_request_irq(DSS_IRQ_DIP0, _dip0_isr, NULL);
		dss_request_irq(DSS_IRQ_MIPI0, _mipi0_isr, NULL);
	}

	mipi_hal_write_config0_pixel_fmt(dsi_idx, dsi_desc->in_pixel_fmt, dsi_desc->out_pixel_fmt);
	mipi_hal_write_config0_opr_mode(dsi_idx, CMD_MODE_DATAPATH);

	if (dsi_desc->bidirectinal)
		mipi_hal_write_config0_turndisable(dsi_idx, true);
	else
		mipi_hal_write_config0_turndisable(dsi_idx, false);

	mipi_hal_write_config0_phy_lane_module(dsi_idx, dsi_desc->num_of_lanes);

	mipi_hal_write_config1_sel_data_lanes(dsi_idx, 0x1, 0xf);
	
	mipi_hal_write_config0_dphy_reset(dsi_idx, false);
	mipi_hal_write_config2_dphy_shutdown(dsi_idx, false);

	if (dsi_desc->dsi_op_mode == DSI_VIDEO_MODE) {
		_mipi_dsi_calc_video_timing(&vm_timing ,&dsi_desc->vm_timing);
		mipi_hal_write_video_mode_desc(dsi_idx, vm_timing.hfp_count, vm_timing.hsa_count, vm_timing.bllp_count, vm_timing.hbp_count,
									   vm_timing.num_vsa_lines, vm_timing.num_vbp_lines, vm_timing.num_vfp_lines, vm_timing.num_vact_lines,
									   vm_timing.num_bytes_line, true);
	}

	_mipi_dsi_dphy_set_byteclock(dsi_idx, dsi_desc->vm_timing.pixel_clock);

	_mipi_dsi_change_dsi_state(DSI_STATE_READY);

err_api_state:
	
	return ret;
}

/**

  deinitialize resources related to MIPI-DSI module.

  The mipi_dsi_hal_close  should follow these comments.

  @param[in]   dsi_ctrl_num  MIPI-DSI module index(ex. MIPI_DSI_DIP0)

  @return      the result of operation (true: sucess, false: failure)

 */
bool mipi_dsi_hal_close(enum dsi_module dsi_idx)
{
	bool ret = true;
	if (!_mipi_dsi_check_dsi_state(DSI_STATE_READY)) {
		pr_err("%s: hal api state : Invalid(Current: %s, Needed: DSI_STATE_READY)\n", __func__,_mipi_dsi_get_dsi_state_mesg());
		ret = false;
		goto err_api_state;
	}

	mipi_hal_write_config2_dphy_shutdown(dsi_idx, true);

	dip_hal_write_mipi_path_params(dsi_idx,false,false);

	dss_free_irq(DSS_IRQ_DIP0);
	dss_free_irq(DSS_IRQ_MIPI0);

	_mipi_dsi_change_dsi_state(DSI_STATE_NOT_READY);

err_api_state:

	return true;
}

/**

  write lcd_mipi_base , mipi_mipi_base variable value.

  @param[in]   dsi_ctrl_num  MIPI-DSI module index(ex. MIPI_DSI_DIP0)

  @param[in]  lcd_base  dip_base address

  @param[in]  mipi_base  mipi_base address

  @return      the result of operation (true: sucess, false: failure)

 */
void mipi_dsi_hal_init(enum dsi_module dsi_num, unsigned int lcd_base, unsigned int mipi_base, bool is_active)
{
	pr_info("%s:  dsi_num: %d ,lcd_base: %x, mipi_base: %x\n", __func__, dsi_num, lcd_base, mipi_base);

	if (dsi_num >= MIPI_DSI_MAX) {
		pr_err("%s: DIP%d is not supported \n", __func__, dsi_num);
		return;
	}
	mipi_hal_set_base_addr(dsi_num, lcd_base, mipi_base);

	if(is_active)
		_mipi_dsi_change_dsi_state(DSI_STATE_VIDEO_MODE);
	else
		_mipi_dsi_change_dsi_state(DSI_STATE_NOT_READY);
		
}

void mipi_dsi_hal_cleanup(enum dsi_module dsi_num)
{

}
