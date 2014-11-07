/*
 * @file:halFrameComposerVideo.c
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halFrameComposerVideo.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 fc_invidconf = 0x00;
static const u8 fc_inhactv0 = 0x01;
static const u8 fc_inhactv1 = 0x02;
static const u8 fc_inhblank0 = 0x03;
static const u8 fc_inhblank1 = 0x04;
static const u8 fc_invactv0 = 0x05;
static const u8 fc_invactv1 = 0x06;
static const u8 fc_invblank = 0x07;
static const u8 fc_hsyncindelay0 = 0x08;
static const u8 fc_hsyncindelay1 = 0x09;
static const u8 fc_hsyncinwidth0 = 0x0A;
static const u8 fc_hsyncinwidth1 = 0x0B;
static const u8 fc_vsyncindelay = 0x0C;
static const u8 fc_vsyncinwidth = 0x0D;
static const u8 fc_infreq0 = 0x0E;
static const u8 fc_infreq1 = 0x0F;
static const u8 fc_infreq2 = 0x10;
static const u8 fc_ctrldur = 0x11;
static const u8 fc_exctrldur = 0x12;
static const u8 fc_exctrlspac = 0x13;
static const u8 fc_ch0pream = 0x14;
static const u8 fc_ch1pream = 0x15;
static const u8 fc_ch2pream = 0x16;
static const u8 fc_prconf = 0xE0;

void halframecomposervideo_hdcpkeepout(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + fc_invidconf), 7, 1);
}

void halframecomposervideo_vsyncpolarity(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + fc_invidconf), 6, 1);
}

void halframecomposervideo_hsyncpolarity(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + fc_invidconf), 5, 1);
}

void halframecomposervideo_dataenablepolarity(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + fc_invidconf), 4, 1);
}

void halframecomposervideo_dviorhdmi(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	/* 1: HDMI; 0: DVI */
	access_corewrite(bit, (baseAddr + fc_invidconf), 3, 1);
}

void halframecomposervideo_vblankosc(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + fc_invidconf), 1, 1);
}

void halframecomposervideo_interlaced(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + fc_invidconf), 0, 1);
}

void halframecomposervideo_hactive(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 12-bit width */
	access_corewritebyte((u8) (value), (baseAddr + fc_inhactv0));
	access_corewrite((u8) (value >> 8), (baseAddr + fc_inhactv1), 0, 5);
}

void halframecomposervideo_hblank(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 10-bit width */
	access_corewritebyte((u8) (value), (baseAddr + fc_inhblank0));
	access_corewrite((u8) (value >> 8), (baseAddr + fc_inhblank1), 0, 5);
}

void halframecomposervideo_vactive(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 11-bit width */
	access_corewritebyte((u8) (value), (baseAddr + fc_invactv0));
	access_corewrite((u8) (value >> 8), (baseAddr + fc_invactv1), 0, 4);
}

void halframecomposervideo_vblank(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 8-bit width */
	access_corewritebyte((u8) (value), (baseAddr + fc_invblank));
}

void halframecomposervideo_hsyncedgedelay(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 11-bit width */
	access_corewritebyte((u8) (value), (baseAddr + fc_hsyncindelay0));
	access_corewrite((u8) (value >> 8), (baseAddr + fc_hsyncindelay1), 0, 4);
}

void halframecomposervideo_hsyncpulsewidth(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 9-bit width */
	access_corewritebyte((u8) (value), (baseAddr + fc_hsyncinwidth0));
	access_corewrite((u8) (value >> 8), (baseAddr + fc_hsyncinwidth1), 0, 1);
}

void halframecomposervideo_vsyncedgedelay(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 8-bit width */
	access_corewritebyte((u8) (value), (baseAddr + fc_vsyncindelay));
}

void halframecomposervideo_vsyncpulsewidth(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	access_corewrite((u8) (value), (baseAddr + fc_vsyncinwidth), 0, 6);
}

void halframecomposervideo_refreshrate(u16 baseAddr, u32 value)
{
	LOG_TRACE1(value);
	/* 20-bit width */
	access_corewritebyte((u8) (value >> 0), (baseAddr + fc_infreq0));
	access_corewritebyte((u8) (value >> 8), (baseAddr + fc_infreq1));
	access_corewrite((u8) (value >> 16), (baseAddr + fc_infreq2), 0, 4);
}

void halframecomposervideo_controlperiodminduration(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + fc_ctrldur));
}

void halframecomposervideo_extendedcontrolperiodminduration(u16 baseAddr,
		u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + fc_exctrldur));
}

void halframecomposervideo_extendedcontrolperiodmaxspacing(u16 baseAddr,
		u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + fc_exctrlspac));
}

void halframecomposervideo_preamblefilter(u16 baseAddr, u8 value,
		unsigned channel)
{
	LOG_TRACE1(value);
	if (channel == 0)
		access_corewritebyte(0xff, (baseAddr + fc_ch0pream));
		/*access_corewritebyte(value, (baseAddr + fc_ch0pream));*/
	else if (channel == 1)
		access_corewrite(0x15, (baseAddr + fc_ch1pream), 0, 6);
		/*access_corewrite(value, (baseAddr + fc_ch1pream), 0, 6);*/
	else if (channel == 2)
		access_corewrite(0x0, (baseAddr + fc_ch2pream), 0, 6);
		/*access_corewrite(value, (baseAddr + fc_ch2pream), 0, 6);*/
	else
		LOG_ERROR2("invalid channel number: ", channel);
}

void halframecomposervideo_pixelrepetitioninput(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + fc_prconf), 4, 4);
}
