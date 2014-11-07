/*
 * hal_audio_generator.c
 *
 *  Created on: Jun 29, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "halAudioGenerator.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 ag_swrstz = 0x00;
static const u8 ag_mode = 0x01;
static const u8 ag_incleft0 = 0x02;
static const u8 ag_incleft1 = 0x03;
static const u8 ag_incright0 = 0x04;
static const u8 ag_spdif_audschnls2 = 0x12;
static const u8 ag_incright1 = 0x05;
static const u8 ag_spdif_conf = 0x16;
static const u8 ag_spdif_audschnls1 = 0x11;
static const u8 ag_spdif_audschnls0 = 0x10;
static const u8 ag_spdif_audschnls3 = 0x13;
static const u8 ag_spdif_audschnls5 = 0x15;
static const u8 ag_spdif_audschnls4 = 0x14;
static const u8 ag_hbr_conf = 0x17;
static const u8 ag_hbr_clkdiv0 = 0x18;
static const u8 ag_hbr_clkdiv1 = 0x19;
static const u8 ag_gpa_conf0 = 0x1A;
static const u8 ag_gpa_conf1 = 0x1B;
static const u8 ag_gpa_samplevalid = 0x1C;
static const u8 ag_gpa_chnum1 = 0x1D;
static const u8 ag_gpa_chnum2 = 0x1E;
static const u8 ag_gpa_chnum3 = 0x1F;
static const u8 ag_gpa_userbit = 0x20;
static const u8 ag_gpa_sample_lsb = 0x21;
static const u8 ag_gpa_sample_msb = 0x22;
static const u8 ag_gpa_sample_diff = 0x23;
static const u8 ag_gpa_int = 0x24;

void halaudiogenerator_swreset(u16 baseAddress, u8 bit)
{
	LOG_TRACE1(bit);
	/* active low */
	access_corewrite(bit ? 0 : 1, baseAddress + ag_swrstz, 0, 1);
}

void halaudiogenerator_i2smode(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + ag_mode, 0, 3);
}

void halaudiogenerator_freqincrementleft(u16 baseAddress, u16 value)
{
	LOG_TRACE1(value);
	access_corewritebyte((u8) (value), baseAddress + ag_incleft0);
	access_corewritebyte(value >> 8, baseAddress + ag_incleft1);
}

void halaudiogenerator_freqincrementright(u16 baseAddress, u16 value)
{
	LOG_TRACE1(value);
	access_corewritebyte((u8) (value), baseAddress + ag_incright0);
	access_corewritebyte(value >> 8, baseAddress + ag_incright1);
}

void halaudiogenerator_ieccgmsa(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + ag_spdif_audschnls0, 1, 2);
}

void halaudiogenerator_ieccopyright(u16 baseAddress, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, baseAddress + ag_spdif_audschnls0, 0, 1);
}

void halaudiogenerator_ieccategorycode(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, baseAddress + ag_spdif_audschnls1);
}

void halaudiogenerator_iecpcmmodee(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + ag_spdif_audschnls2, 4, 3);
}

void halaudiogenerator_iecsource(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + ag_spdif_audschnls2, 0, 4);
}

void halaudiogenerator_iecchannelright(u16 baseAddress, u8 value, u8 channelNo)
{
	LOG_TRACE1(value);
	switch (channelNo)
	{
		case 0:
			access_corewrite(value, baseAddress + ag_spdif_audschnls3, 4, 4);
			break;
		case 1:
			access_corewrite(value, baseAddress + ag_gpa_chnum1, 4, 4);
			break;
		case 2:
			access_corewrite(value, baseAddress + ag_gpa_chnum2, 4, 4);
			break;
		case 3:
			access_corewrite(value, baseAddress + ag_gpa_chnum3, 4, 4);
			break;
		default:
			LOG_ERROR("wrong channel number");
			break;
	}
	
}

void halaudiogenerator_iecchannelleft(u16 baseAddress, u8 value, u8 channelNo)
{
	LOG_TRACE1(value);
	switch (channelNo)
	{
		case 0:
			access_corewrite(value, baseAddress + ag_spdif_audschnls3, 0, 4);
			break;
		case 1:
			access_corewrite(value, baseAddress + ag_gpa_chnum1, 0, 4);
			break;
		case 2:
			access_corewrite(value, baseAddress + ag_gpa_chnum2, 0, 4);
			break;
		case 3:
			access_corewrite(value, baseAddress + ag_gpa_chnum3, 0, 4);
			break;
		default:
			LOG_ERROR("wrong channel number");
			break;
	}	
}

void halaudiogenerator_iecclockaccuracy(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + ag_spdif_audschnls4, 4, 2);
}

void halaudiogenerator_iecsamplingfreq(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + ag_spdif_audschnls4, 0, 4);
}

void halaudiogenerator_iecoriginalsamplingfreq(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + ag_spdif_audschnls5, 4, 4);
}

void halaudiogenerator_iecwordlength(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + ag_spdif_audschnls5, 0, 4);
}

void halaudiogenerator_userright(u16 baseAddress, u8 bit, u8 channelNo)
{
	LOG_TRACE1(bit);
	switch (channelNo)
	{
		case 0:
			access_corewrite(bit, baseAddress + ag_spdif_conf, 3, 1);
			break;
		case 1:
			access_corewrite(bit, baseAddress + ag_gpa_userbit, 1, 1);
			break;
		case 2:
			access_corewrite(bit, baseAddress + ag_gpa_userbit, 2, 1);
			break;
		case 3:
			access_corewrite(bit, baseAddress + ag_gpa_userbit, 5, 1);
			break;
		default:
			LOG_ERROR("wrong channel number");
			break;
	}
}

void halaudiogenerator_userleft(u16 baseAddress, u8 bit, u8 channelNo)
{
	LOG_TRACE1(bit);
	switch (channelNo)
	{
		case 0:
			access_corewrite(bit, baseAddress + ag_spdif_conf, 2, 1);
			break;
		case 1:
			access_corewrite(bit, baseAddress + ag_gpa_userbit, 0, 1);
			break;
		case 2:
			access_corewrite(bit, baseAddress + ag_gpa_userbit, 2, 1);
			break;
		case 3:
			access_corewrite(bit, baseAddress + ag_gpa_userbit, 4, 1);
			break;
		default:
			LOG_ERROR("wrong channel number");
			break;
	}
}

void halaudiogenerator_spdiftxdata(u16 baseAddress, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, baseAddress + ag_spdif_conf, 1, 1);
}

void halaudiogenerator_hbrenable(u16 baseAddress, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, baseAddress + ag_hbr_conf, 0, 1);
}

void halaudiogenerator_hbrddrenable(u16 baseAddress, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, baseAddress + ag_hbr_conf, 1, 1);
}

void halaudiogenerator_hbrddrchannel(u16 baseAddress, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, baseAddress + ag_hbr_conf, 2, 1);
}

void halaudiogenerator_hbrburstlength(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + ag_hbr_conf, 3, 4);
}

void halaudiogenerator_hbrclockdivider(u16 baseAddress, u16 value)
{
	LOG_TRACE1(value);
	/* 9-bit width */
	access_corewritebyte((u8) (value), baseAddress + ag_hbr_clkdiv0);
	access_corewrite(value >> 8, baseAddress + ag_hbr_clkdiv1, 0, 1);
}

void halaudiogenerator_uselookuptable(u16 baseAddress, u8 value)
{
	access_corewrite(value, baseAddress + ag_gpa_conf0, 5, 1);
}

void halaudiogenerator_gpareplylatency(u16 baseAddress, u8 value)
{
	access_corewrite(value, baseAddress + ag_gpa_conf0, 0, 2);
}

void halaudiogenerator_channelselect(u16 baseAddress, u8 enable, u8 channel)
{
	LOG_TRACE2(channel, enable);
	access_corewrite(enable, baseAddress + ag_gpa_conf1, channel, 1);
}

void halaudiogenerator_gpasamplevalid(u16 baseAddress, u8 value, u8 channelNo)
{
		access_corewrite(value, baseAddress + ag_gpa_samplevalid, channelNo, 1);	
}
