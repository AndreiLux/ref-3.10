/*
 * @file:halFrameComposerAudioInfo.c
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halFrameComposerAudioInfo.h"
#include "../bsp/access.h"
#include "../util/log.h"

static const u8 fc_audiconf0 = 0X25;
static const u8 fc_audiconf1 = 0X26;
static const u8 fc_audiconf2 = 0X27;
static const u8 fc_audiconf3 = 0X28;
static const u8 channel_count = 4;
static const u8 coding_type = 0;
static const u8 sampling_size = 4;
static const u8 sampling_freq = 0;
static const u8 channel_allocation = 0;
static const u8 level_shift_value = 0;
static const u8 down_mix_inhibit = 4;

void halframecomposeraudioinfo_channelcount(u16 baseAddr, u8 noOfChannels)
{
	LOG_TRACE1(noOfChannels);
	access_corewrite(noOfChannels, baseAddr + fc_audiconf0, channel_count, 3);
}

void halframecomposeraudioinfo_samplefreq(u16 baseAddr, u8 sf)
{
	LOG_TRACE1(sf);
	access_corewrite(sf, baseAddr + fc_audiconf1, sampling_freq, 3);
}

void halframecomposeraudioinfo_allocatechannels(u16 baseAddr, u8 ca)
{
	LOG_TRACE1(ca);
	access_corewritebyte(ca, baseAddr + fc_audiconf2);
}

void halframecomposeraudioinfo_levelshiftvalue(u16 baseAddr, u8 lsv)
{
	LOG_TRACE1(lsv);
	access_corewrite(lsv, baseAddr + fc_audiconf3, level_shift_value, 4);
}

void halframecomposeraudioinfo_downmixinhibit(u16 baseAddr, u8 prohibited)
{
	LOG_TRACE1(prohibited);
	access_corewrite((prohibited ? 1 : 0), baseAddr + fc_audiconf3,
			down_mix_inhibit, 1);
}

void halframecomposeraudioinfo_codingtype(u16 baseAddr, u8 codingType)
{
	LOG_TRACE1(codingType);
	access_corewrite(codingType, baseAddr + fc_audiconf0, coding_type, 4);
}

void halframecomposeraudioinfo_samplingsize(u16 baseAddr, u8 ss)
{
	LOG_TRACE1(ss);
	access_corewrite(ss, baseAddr + fc_audiconf1, sampling_size, 2);
}
