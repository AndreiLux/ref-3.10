/*
 * @file:halFrameComposerAudio.c
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halFrameComposerAudio.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* frame composer audio register offsets */
static const u8 fc_audsconf = 0x63;
static const u8 fc_audsstat = 0x64;
static const u8 fc_audsv = 0x65;
static const u8 fc_audsu = 0x66;
static const u8 fc_audschnls0 = 0x67;
static const u8 fc_audschnls1 = 0x68;
static const u8 fc_audschnls2 = 0x69;
static const u8 fc_audschnls3 = 0x6A;
static const u8 fc_audschnls4 = 0x6B;
static const u8 fc_audschnls5 = 0x6C;
static const u8 fc_audschnls6 = 0x6D;
static const u8 fc_audschnls7 = 0x6E;
static const u8 fc_audschnls8 = 0x6F;

void halframecomposeraudio_packetsampleflat(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddr + fc_audsconf, 4, 4);
}

void halframecomposeraudio_packetlayout(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, baseAddr + fc_audsconf, 0, 1);
}

void halframecomposeraudio_validityright(u16 baseAddr, u8 bit, unsigned channel)
{
	LOG_TRACE2(bit, channel);
	if (channel < 4)
		access_corewrite(bit, baseAddr + fc_audsv, 4 + channel, 1);
	else
		LOG_ERROR("invalid channel number");
}

void halframecomposeraudio_validityleft(u16 baseAddr, u8 bit, unsigned channel)
{
	LOG_TRACE2(bit, channel);
	if (channel < 4)
		access_corewrite(bit, baseAddr + fc_audsv, channel, 1);
	else
		LOG_ERROR2("invalid channel number: ", channel);
}

void halframecomposeraudio_userright(u16 baseAddr, u8 bit, unsigned channel)
{
	LOG_TRACE2(bit, channel);
	if (channel < 4)
		access_corewrite(bit, baseAddr + fc_audsu, 4 + channel, 1);
	else
		LOG_ERROR2("invalid channel number: ", channel);
}

void halframecomposeraudio_userleft(u16 baseAddr, u8 bit, unsigned channel)
{
	LOG_TRACE2(bit, channel);
	if (channel < 4)
		access_corewrite(bit, baseAddr + fc_audsu, channel, 1);
	else
		LOG_ERROR2("invalid channel number: ", channel);
}

void halframecomposeraudio_ieccgmsa(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddr + fc_audschnls0, 4, 2);
}

void halframecomposeraudio_ieccopyright(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, baseAddr + fc_audschnls0, 0, 1);
}

void halframecomposeraudio_ieccategorycode(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, baseAddr + fc_audschnls1);
}

void halframecomposeraudio_iecpcmmode(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddr + fc_audschnls2, 4, 3);
}

void halframecomposeraudio_iecsource(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddr + fc_audschnls2, 0, 4);
}

void halframecomposeraudio_iecchannelright(u16 baseAddr, u8 value,
		unsigned channel)
{
	LOG_TRACE2(value, channel);
	if (channel == 0)
		access_corewrite(value, baseAddr + fc_audschnls3, 0, 4);
	else if (channel == 1)
		access_corewrite(value, baseAddr + fc_audschnls3, 4, 4);
	else if (channel == 2)
		access_corewrite(value, baseAddr + fc_audschnls4, 0, 4);
	else if (channel == 3)
		access_corewrite(value, baseAddr + fc_audschnls4, 4, 4);
	else
		LOG_ERROR2("invalid channel number: ", channel);
}

void halframecomposeraudio_iecchannelleft(u16 baseAddr, u8 value,
		unsigned channel)
{
	LOG_TRACE2(value, channel);
	if (channel == 0)
		access_corewrite(value, baseAddr + fc_audschnls5, 0, 4);
	else if (channel == 1)
		access_corewrite(value, baseAddr + fc_audschnls5, 4, 4);
	else if (channel == 2)
		access_corewrite(value, baseAddr + fc_audschnls6, 0, 4);
	else if (channel == 3)
		access_corewrite(value, baseAddr + fc_audschnls6, 4, 4);
	else
		LOG_ERROR2("invalid channel number: ", channel);
}

void halframecomposeraudio_iecclockaccuracy(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddr + fc_audschnls7, 4, 2);
}

void halframecomposeraudio_iecsamplingfreq(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddr + fc_audschnls7, 0, 4);
}

void halframecomposeraudio_iecoriginalsamplingfreq(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddr + fc_audschnls8, 4, 4);
}

void halframecomposeraudio_iecwordlength(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddr + fc_audschnls8, 0, 4);
}

