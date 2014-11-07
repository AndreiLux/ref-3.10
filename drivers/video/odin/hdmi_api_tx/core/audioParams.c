/*
 * @file:audioParams.c
 *
 *  Created on: Jul 2, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "audioParams.h"

void audioparams_reset(audioParams_t *params)
{
	params->minterfacetype = I2S;
	params->mcodingtype = PCM;
/* 0x0=2ch, 0x13=7.1ch */
	params->mchannelallocation = 0x00;
	params->msamplesize = 16;
	params->msamplingfrequency = 48000;
	params->mlevelshiftvalue = 0;
/* 1:multi-ch to 2ch(2h shoud not be 1 set) */
	params->mdownmixinhibitflag = 0;
	params->moriginalsamplingfrequency = 0;
	params->mieccopyright = 1;
	params->mieccgmsa = 3;
	params->miiecpcmmode = 0;
	params->mieccategorycode = 0xaa;
/* 1 -> 4 */
	params->miecsourcenumber = 4;
	params->miecclockaccuracy = 1;
	params->mpackettype = AUDIO_SAMPLE;
/* 512 -> 64 */
	params->mclockfsfactor = 64;
	params->mdmabeatincrement = DMA_UNSPECIFIED_INCREMENT;
	params->mdmathreshold = 0;
	params->mdmahlock = 0;
	params->mgpainsertpucv = 0;
}

 u8 audioparams_getchannelallocation(audioParams_t *params)
{
	return params->mchannelallocation;
}

void audioparams_setchannelallocation(audioParams_t *params, u8 value)
{
	params->mchannelallocation = value;
}

 u8 audioparams_getcodingtype(audioParams_t *params)
{
	return params->mcodingtype;
}

void audioparams_setcodingtype(audioParams_t *params, codingType_t value)
{
	params->mcodingtype = value;
}

 u8 audioparams_getdownmixinhibitflag(audioParams_t *params)
{
	return params->mdownmixinhibitflag;
}

void audioparams_setdownmixinhibitflag(audioParams_t *params, u8 value)
{
	params->mdownmixinhibitflag = value;
}

 u8 audioparams_getieccategorycode(audioParams_t *params)
{
	return params->mieccategorycode;
}

void audioparams_setieccategorycode(audioParams_t *params, u8 value)
{
	params->mieccategorycode = value;
}

 u8 audioparams_getieccgmsa(audioParams_t *params)
{
	return params->mieccgmsa;
}

void audioparams_setieccgmsa(audioParams_t *params, u8 value)
{
	params->mieccgmsa = value;
}

 u8 audioparams_getiecclockaccuracy(audioParams_t *params)
{
	return params->miecclockaccuracy;
}

void audioparams_setiecclockaccuracy(audioParams_t *params, u8 value)
{
	params->miecclockaccuracy = value;
}

 u8 audioparams_getieccopyright(audioParams_t *params)
{
	return params->mieccopyright;
}

void audioparams_setieccopyright(audioParams_t *params, u8 value)
{
	params->mieccopyright = value;
}

 u8 audioparams_getiecpcmmode(audioParams_t *params)
{
	return params->miiecpcmmode;
}

void audioparams_setiecpcmmode(audioParams_t *params, u8 value)
{
	params->miiecpcmmode = value;
}

 u8 audioparams_getiecsourcenumber(audioParams_t *params)
{
	return params->miecsourcenumber;
}

void audioparams_setiecsourcenumber(audioParams_t *params, u8 value)
{
	params->miecsourcenumber = value;
}

 interfaceType_t audioparams_getinterfacetype(audioParams_t *params)
{
	return params->minterfacetype;
}

void audioparams_setinterfacetype(audioParams_t *params, interfaceType_t value)
{
	params->minterfacetype = value;
}

 u8 audioparams_getlevelshiftvalue(audioParams_t *params)
{
	return params->mlevelshiftvalue;
}

void audioparams_setlevelshiftvalue(audioParams_t *params, u8 value)
{
	params->mlevelshiftvalue = value;
}

 u32 audioparams_getoriginalsamplingfrequency(audioParams_t *params)
{
	return params->moriginalsamplingfrequency;
}

void audioparams_setoriginalsamplingfrequency(audioParams_t *params, u32 value)
{
	params->moriginalsamplingfrequency = value;
}

 u8 audioparams_getsamplesize(audioParams_t *params)
{
	return params->msamplesize;
}

void audioparams_setsamplesize(audioParams_t *params, u8 value)
{
	params->msamplesize = value;
}

 u32 audioparams_getsamplingfrequency(audioParams_t *params)
{
	return params->msamplingfrequency;
}

void audioparams_setsamplingfrequency(audioParams_t *params, u32 value)
{
	params->msamplingfrequency = value;
}

 u32 audioparams_audioclock(audioParams_t *params)
{
	return (params->mclockfsfactor * params->msamplingfrequency);
}

 u8 audioparams_channelcount(audioParams_t *params)
{
	switch (params->mchannelallocation)
	{
		case 0x00:
			return 1;
		case 0x01:
		case 0x02:
		case 0x04:
			return 2;
		case 0x03:
		case 0x05:
		case 0x06:
		case 0x08:
		case 0x14:
			return 3;
		case 0x07:
		case 0x09:
		case 0x0A:
		case 0x0C:
		case 0x15:
		case 0x16:
		case 0x18:
			return 4;
		case 0x0B:
		case 0x0D:
		case 0x0E:
		case 0x10:
		case 0x17:
		case 0x19:
		case 0x1A:
		case 0x1C:
			return 5;
		case 0x0F:
		case 0x11:
		case 0x12:
		case 0x1B:
		case 0x1D:
		case 0x1E:
			return 6;
		case 0x13:
		case 0x1F:
			return 7;
		default:
			return 0;
	}
}

 u8 audioparams_iecoriginalsamplingfrequency(audioParams_t *params)
{
	/* SPEC values are LSB to MSB */
	switch (params->moriginalsamplingfrequency)
	{
		case 8000:
			return 0x6;
		case 11025:
			return 0xA;
		case 12000:
			return 0x2;
		case 16000:
			return 0x8;
		case 22050:
			return 0xB;
		case 24000:
			return 0x9;
		case 32000:
			return 0xC;
		case 44100:
			return 0xF;
		case 48000:
			return 0xD;
		case 88200:
			return 0x7;
		case 96000:
			return 0x5;
		case 176400:
			return 0x3;
		case 192000:
			return 0x1;
		default:
			return 0x0; /* not indicated */
	}
}

 u8 audioparams_iecsamplingfrequency(audioParams_t *params)
{
	/* SPEC values are LSB to MSB */
	switch (params->msamplingfrequency)
	{
		case 22050:
			return 0x4;
		case 44100:
			return 0x0;
		case 88200:
			return 0x8;
		case 176400:
			return 0xC;
		case 24000:
			return 0x6;
		case 48000:
			return 0x2;
		case 96000:
			return 0xA;
		case 192000:
			return 0xE;
		case 32000:
			return 0x3;
		case 768000:
			return 0x9;
		default:
			return 0x1; /* not indicated */
	}
}

 u8 audioparams_iecwordlength(audioParams_t *params)
{
	/* SPEC values are LSB to MSB */
	switch (params->msamplesize)
	{
		case 20:
			return 0x3; /* or 0xA; */
		case 22:
			return 0x5;
		case 23:
			return 0x9;
		case 24:
			return 0xB;
		case 21:
			return 0xD;
		case 16:
			return 0x2;
		case 18:
			return 0x4;
		case 19:
			return 0x8;
		case 17:
			return 0xC;
		default:
			return 0x0; /* not indicated */
	}
}

 u8 audioparams_islinearpcm(audioParams_t *params)
{
	return params->mcodingtype == PCM;
}

packet_t audioparams_getpackettype(audioParams_t *params)
{
	return params->mpackettype;
}

void audioparams_setpackettype(audioParams_t *params, packet_t packetType)
{
	params->mpackettype = packetType;
}

u8 audioparams_ischannelen(audioParams_t *params, u8 channel)
{
	switch (channel)
	{
		case 0:
		case 1:
			return 1;
		case 2:
			return params->mchannelallocation & BIT(0);
		case 3:
			return (params->mchannelallocation & BIT(1)) >> 1;
		case 4:
			if (((params->mchannelallocation > 0x03) &&
				(params->mchannelallocation < 0x14)) ||
				((params->mchannelallocation > 0x17) &&
				(params->mchannelallocation < 0x20)))
				return 1;
			else
				return 0;
		case 5:
			if (((params->mchannelallocation > 0x07) &&
				(params->mchannelallocation < 0x14)) ||
				((params->mchannelallocation > 0x1C) &&
				(params->mchannelallocation < 0x20)))
				return 1;
			else
				return 0;
		case 6:
			if ((params->mchannelallocation > 0x0B) &&
				(params->mchannelallocation < 0x20))
				return 1;
			else
				return 0;
		case 7:
			return (params->mchannelallocation & BIT(4)) >> 4;
		default:
			return 0;
	}
}

u16 audioparams_getclockfsfactor(audioParams_t *params)
{
	return params->mclockfsfactor;
}

void audioparams_setclockfsfactor(audioParams_t *params, u16 clockFsFactor)
{
	params->mclockfsfactor = clockFsFactor;
}

dmaIncrement_t audioparams_getdmabeatincrement(audioParams_t *params)
{
	return params->mdmabeatincrement;
}

void audioparams_setdmabeatincrement(
			audioParams_t *params, dmaIncrement_t value)
{
	params->mdmabeatincrement = value;
}

void audioparams_setdmathreshold(audioParams_t *params, u8 threshold)
{
	params->mdmathreshold = threshold;
}

u8 audioparams_getdmathreshold(audioParams_t *params)
{
	return params->mdmathreshold;
}

u8 audioparams_getdmahlock(audioParams_t *params)
{
	return params->mdmahlock;
}

void audioparams_setdmahlock(audioParams_t *params, int enable)
{
	params->mdmahlock = enable;
}

void audioparams_setgpasamplepacketinfosource(
				audioParams_t *params, int internal)
{
	params->mgpainsertpucv = internal;
}

int audioparams_getgpasamplepacketinfosource(audioParams_t *params)
{
	return params->mgpainsertpucv;
}
