/*
 * @file:audio.c
 *
 *  Created on: Jul 2, 2010
 *  Modified: Oct 2010: GPA interface added
 *  Modified: May 2011: DMA added
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include <linux/kernel.h>
#include "audio.h"
#include "halFrameComposerAudio.h"
#include "halAudioI2s.h"
#include "halAudioSpdif.h"
#include "halAudioHbr.h"
#include "halAudioGpa.h"
#include "halAudioClock.h"
#include "halAudioGenerator.h"
#include "halAudioDma.h"
#include "../util/log.h"

/* block offsets */
static const u16 fc_base_addr = 0x1000;
static const u16 aud_base_addr = 0x3100;
static const u16 ag_base_addr = 0x7100;
/* interface offset*/
static const u16 audio_i2s   = 0x0000;
static const u16 audio_clock = 0x0100;
static const u16 audio_spdif = 0x0200;
static const u16 audio_hbr   = 0x0300;
static const u16 audio_gpa   = 0x0400;
static const u16 audio_dma   = 0x0500;
/* constants */
static const u8 user_bit = 0;

int audio_initialize(u16 baseAddr)
{
	LOG_TRACE();
	halaudiodma_dmainterruptmask(baseAddr + aud_base_addr + audio_dma, ~0);
	halaudiodma_bufferinterruptmask(baseAddr + aud_base_addr + audio_dma, ~0);
	return audio_mute(baseAddr, 1);
}

int audio_configure(
	u16 baseAddr, audioParams_t *params, u16 pixelClk, unsigned int ratioClk)
{
	u16 i = 0;
	LOG_TRACE();
	/* more than 2 channels => layout 1 else layout 0 */
	halframecomposeraudio_packetlayout(
		baseAddr + fc_base_addr,
		((audioparams_channelcount(params) + 1) > 2) ? 1 : 0);
	/* iec validity and user bits (IEC 60958-1 */
	for (i = 0; i < 4; i++)
	{
		/* audioParams_IsChannelEn considers left as 1 channel and
		 * right as another (+1), hence the x2 factor in the following */
		/* validity bit is 0 when reliable, which is !IsChannelEn */
		halframecomposeraudio_validityright(
			baseAddr + fc_base_addr, !(audioparams_ischannelen(params, (2 * i))), i);
		halframecomposeraudio_validityleft(
			baseAddr + fc_base_addr, !(audioparams_ischannelen(params, (2 * i) + 1)), i);
		halframecomposeraudio_userright(
			baseAddr + fc_base_addr, 	user_bit, i);
		halframecomposeraudio_userleft(
			baseAddr + fc_base_addr, user_bit, i);
	}
	/* IEC - not needed if non-linear PCM */
	halframecomposeraudio_ieccgmsa(
		baseAddr + fc_base_addr, audioparams_getieccgmsa(params));
	halframecomposeraudio_ieccopyright(
		baseAddr + fc_base_addr, audioparams_getieccopyright(params) ? 0 : 1);
	halframecomposeraudio_ieccategorycode(
		baseAddr + fc_base_addr, audioparams_getieccategorycode(params));
	halframecomposeraudio_iecpcmmode(
		baseAddr + fc_base_addr, audioparams_getiecpcmmode(params));
	halframecomposeraudio_iecsource(
		baseAddr + fc_base_addr, audioparams_getiecsourcenumber(params));
	for (i = 0; i < 4; i++)
	{ 	/* 0, 1, 2, 3 */
		halframecomposeraudio_iecchannelleft(
			baseAddr + fc_base_addr, 2* i + 1, i); /* 1, 3, 5, 7 */
		halframecomposeraudio_iecchannelright(
			baseAddr + fc_base_addr, 2* (i + 1), i); /* 2, 4, 6, 8 */
	}
	halframecomposeraudio_iecclockaccuracy(
		baseAddr + fc_base_addr, audioparams_getiecclockaccuracy(params));
    halframecomposeraudio_iecsamplingfreq(
    	baseAddr + fc_base_addr, audioparams_iecsamplingfrequency(params));
    halframecomposeraudio_iecoriginalsamplingfreq(
    	baseAddr + fc_base_addr, audioparams_iecoriginalsamplingfrequency(params));
    halframecomposeraudio_iecwordlength(
    	baseAddr + fc_base_addr, audioparams_iecwordlength(params));

	halaudioi2s_select(baseAddr + aud_base_addr + audio_i2s,
		(audioparams_getinterfacetype(params) == I2S)? 1 : 0);
 	/*
 	 * ATTENTION: fixed I2S data enable configuration
 	 * is equivalent to 0x1 for 1 or 2 channels
 	 * is equivalent to 0x3 for 3 or 4 channels
 	 * is equivalent to 0x7 for 5 or 6 channels
 	 * is equivalent to 0xF for 7 or 8 channels
 	 */
 	halaudioi2s_dataenable(baseAddr + aud_base_addr + audio_i2s, 0xF);
 	/* ATTENTION: fixed I2S data mode (standard) */
 	halaudioi2s_datamode(baseAddr + aud_base_addr + audio_i2s, 0);
 	halaudioi2s_datawidth(baseAddr + aud_base_addr + audio_i2s,
			audioparams_getsamplesize(params));
 	halaudioi2s_interruptmask(baseAddr + aud_base_addr + audio_i2s, 3);
 	halaudioi2s_interruptpolarity(baseAddr + aud_base_addr + audio_i2s, 3);
 	halaudioi2s_resetfifo(baseAddr + aud_base_addr + audio_i2s);

 	halaudiospdif_nonlinearpcm(baseAddr + aud_base_addr + audio_spdif,
			audioparams_islinearpcm(params)? 0 : 1);
 	halaudiospdif_datawidth(baseAddr + aud_base_addr + audio_spdif,
			audioparams_getsamplesize(params));
 	halaudiospdif_interruptmask(baseAddr + aud_base_addr + audio_spdif, 3);
 	halaudiospdif_interruptpolarity(baseAddr + aud_base_addr + audio_spdif, 3);
 	halaudiospdif_resetfifo(baseAddr + aud_base_addr + audio_spdif);

 	halaudiohbr_select(baseAddr + aud_base_addr + audio_hbr, (
			audioparams_getinterfacetype(params) == HBR)? 1 : 0);
 	halaudiohbr_interruptmask(baseAddr + aud_base_addr + audio_hbr, 7);
 	halaudiohbr_interruptpolarity(baseAddr + aud_base_addr + audio_hbr, 7);
 	halaudiohbr_resetfifo(baseAddr + aud_base_addr + audio_hbr);

 	halaudioclock_n(baseAddr + aud_base_addr + audio_clock,
			audio_computen(baseAddr,
				audioparams_getsamplingfrequency(params), pixelClk, ratioClk));

 	if (audioparams_getinterfacetype(params) == GPA)
	{
		if (audioparams_getpackettype(params) == HBR_STREAM)
		{
			halaudiogpa_hbrenable(baseAddr + aud_base_addr + audio_gpa, 1);
			for (i = 0; i < 8; i++)
			{	/* 8 channels must be enabled */
				halaudiogpa_channelenable(baseAddr + aud_base_addr + audio_gpa, 1, i);
			}
		}
		else
		{	/* When insert_pucv is active (1) any input data is ignored  and the
			 parity and B pulse bits are generated in run time, while the
			 Channel status, User bit and Valid bit are retrieved from registers
			  fc_audschnls0 to fc_audschnls8, fc_audsu and fc_audsv. */
			halaudiogpa_insertpucv(baseAddr + aud_base_addr + audio_gpa,
					audioparams_getgpasamplepacketinfosource(params));
			halaudiogpa_hbrenable(baseAddr + aud_base_addr + audio_gpa, 0);
			for (i = 0; i < 8; i++)
			{
				halaudiogpa_channelenable(baseAddr + aud_base_addr + audio_gpa,
					audioparams_ischannelen(params, i), i);
			}
		}
		halaudiogpa_interruptmask(baseAddr + aud_base_addr + audio_gpa, 3);
		halaudiogpa_interruptpolority(baseAddr + aud_base_addr + audio_gpa, 3);
		halaudiogpa_resetfifo(baseAddr + aud_base_addr + audio_gpa);
		/* compute CTS */
		halaudioclock_cts(baseAddr + aud_base_addr + audio_clock,
					audio_computects(baseAddr,
						audioparams_getsamplingfrequency(params), pixelClk, ratioClk));
	}
	else if (audioparams_getinterfacetype(params) == DMA)
	{
		if (audioparams_getpackettype(params) == HBR_STREAM)
		{
			halaudiodma_hbrenable(baseAddr + aud_base_addr + audio_dma, 1);
			for (i = 2; i < 8; i++)
			{	/* 8 channels must be enabled */
				halaudiodma_channelenable(baseAddr + aud_base_addr + audio_dma, 1, i);
			}
		}
		else
		{
			halaudiodma_hbrenable(baseAddr + aud_base_addr + audio_dma, 0);
			for (i = 2; i < 8; i++)
			{
				halaudiodma_channelenable(baseAddr + aud_base_addr + audio_dma,
					audioparams_ischannelen(params, i), i);
			}
		}
		halaudiodma_resetfifo(baseAddr + aud_base_addr + audio_dma);
		halaudiodma_fixburstmode(baseAddr + aud_base_addr + audio_dma,
				audioparams_getdmabeatincrement(params));
		halaudiodma_threshold(baseAddr + aud_base_addr + audio_dma,
				audioparams_getdmathreshold(params));
		/* compute CTS */
		halaudioclock_cts(baseAddr + aud_base_addr + audio_clock,
				audio_computects(baseAddr,
					audioparams_getsamplingfrequency(params), pixelClk, ratioClk));
	}
	else
	{
		/* if NOT GPA interface, use automatic mode CTS */
		halaudioclock_cts(baseAddr + aud_base_addr + audio_clock, 0x1220a);
		switch (audioparams_getclockfsfactor(params))
		{
			/* This version does not support DRU Bypass - found in controller 1v30a on
			 *	0 128Fs		I2S- (SPDIF when DRU in bypass)
			 *	1 256Fs		I2S-
			 *	2 512Fs		I2S-HBR-(SPDIF - when NOT DRU bypass)
			 *	3 1024Fs	I2S-(SPDIF - when NOT DRU bypass)
			 *	4 64Fs		I2S
			 */

			case 64:
				if (audioparams_getinterfacetype(params) != I2S)
				{
					printk("audio param 64\n");
					return FALSE;
				}
				halaudioclock_f(baseAddr + aud_base_addr + audio_clock, 4);
				break;
			case 128:
				if (audioparams_getinterfacetype(params) != I2S)
				{
					printk("audio param 128\n");
					return FALSE;
				}
				halaudioclock_f(baseAddr + aud_base_addr + audio_clock, 0);
				break;
			case 256:
				if (audioparams_getinterfacetype(params) != I2S)
				{
					printk("audio param 256\n");
					return FALSE;
				}
				halaudioclock_f(baseAddr + aud_base_addr + audio_clock, 1);
				break;
			case 512:
				halaudioclock_f(baseAddr + aud_base_addr + audio_clock, 2);
				break;
			case 1024:
				if (audioparams_getinterfacetype(params) == HBR)
				{
					printk("audio param 512\n");
					return FALSE;
				}
				halaudioclock_f(baseAddr + aud_base_addr + audio_clock, 3);
				break;
			default:
				/* Fs clock factor not supported */
				printk("audio param default\n");
				return FALSE;
		}
	}
 	if (audio_audiogenerator(baseAddr, params) != TRUE)
 	{
		printk("auio gen error\n");
 		return FALSE;
 	}

 	return audio_mute(baseAddr, 0);
 }

int audio_mute(u16 baseAddr, u8 state)
{
	/* audio mute priority: AVMUTE, sample flat, validity */
	/* AVMUTE also mutes video */
	halframecomposeraudio_packetsampleflat(baseAddr + fc_base_addr,
			state ? 0xF : 0);
	return TRUE;
}

int audio_audiogenerator(u16 baseAddr, audioParams_t *params)
{
	/*
	 * audio generator block is not included in real application hardware,
	 * Then the external audio sources are used and this code has no effect
	 */
	u32 tmp;
	LOG_TRACE();
	/* should be coherent with I2S config? */
	halaudiogenerator_i2smode(baseAddr + ag_base_addr, 1);
	tmp = (1500 * 65535) / audioparams_getsamplingfrequency(params); /* 1500Hz */
	halaudiogenerator_freqincrementleft(baseAddr + ag_base_addr, (u16) (tmp));
	tmp = (3000 * 65535) / audioparams_getsamplingfrequency(params); /* 3000Hz */
	halaudiogenerator_freqincrementright(baseAddr + ag_base_addr,
			(u16) (tmp));

	halaudiogenerator_ieccgmsa(baseAddr + ag_base_addr,
			audioparams_getieccgmsa(params));
	halaudiogenerator_ieccopyright(baseAddr + ag_base_addr,
			audioparams_getieccopyright(params) ? 0 : 1);
	halaudiogenerator_ieccategorycode(baseAddr + ag_base_addr,
			audioparams_getieccategorycode(params));
	halaudiogenerator_iecpcmmodee(baseAddr + ag_base_addr,
			audioparams_getiecpcmmode(params));
	halaudiogenerator_iecsource(baseAddr + ag_base_addr,
			audioparams_getiecsourcenumber(params));
	for (tmp = 0; tmp < 4; tmp++)
	{
		/* 0 -> iec spec 60958-3 means "do not take into account" */
		halaudiogenerator_iecchannelleft(baseAddr + ag_base_addr, 0, tmp);
		halaudiogenerator_iecchannelright(baseAddr + ag_base_addr, 0, tmp);
		/* user_bit 0 default by spec */
		halaudiogenerator_userleft(baseAddr + ag_base_addr, user_bit, tmp);
		halaudiogenerator_userright(baseAddr + ag_base_addr, user_bit, tmp);
	}
	halaudiogenerator_iecclockaccuracy(baseAddr + ag_base_addr,
			audioparams_getiecclockaccuracy(params));
	halaudiogenerator_iecsamplingfreq(baseAddr + ag_base_addr,
			audioparams_iecsamplingfrequency(params));
	halaudiogenerator_iecoriginalsamplingfreq(baseAddr + ag_base_addr,
			audioparams_iecoriginalsamplingfrequency(params));
	halaudiogenerator_iecwordlength(baseAddr + ag_base_addr,
			audioparams_iecwordlength(params));

	halaudiogenerator_spdiftxdata(baseAddr + ag_base_addr, 1);
	/* HBR is synthesizable but not audible */
	halaudiogenerator_hbrddrenable(baseAddr + ag_base_addr, 0);
	halaudiogenerator_hbrddrchannel(baseAddr + ag_base_addr, 0);
	halaudiogenerator_hbrburstlength(baseAddr + ag_base_addr, 0);
	halaudiogenerator_hbrclockdivider(baseAddr + ag_base_addr, 128); /* 128 * fs */
	halaudiogenerator_hbrenable(baseAddr + ag_base_addr, 1);
	/* GPA */
	for (tmp = 0; tmp < 8; tmp++)
	{
		halaudiogenerator_gpasamplevalid(baseAddr + ag_base_addr,
			!(audioparams_ischannelen(params, tmp)), tmp);
		halaudiogenerator_channelselect(baseAddr + ag_base_addr,
			audioparams_ischannelen(params, tmp), tmp);
	}
	halaudiogenerator_gpareplylatency(baseAddr + ag_base_addr, 0x03);
	halaudiogenerator_swreset(baseAddr + ag_base_addr, 1);
	return TRUE;
}

u16 audio_computen(u16 baseAddr, u32 freq, u16 pixelClk, u16 ratioClk)
{
	u32 n = (128 * freq) / 1000;

	switch (freq)
	{
	case 32000:
		if (pixelClk == 2517)
			n = (ratioClk == 150) ? 9152 : 4576;
		else if (pixelClk == 2702)
			n = (ratioClk == 150) ? 8192 : 4096;
		else if (pixelClk == 7417 || pixelClk == 14835)
			n = 11648;
		else
			n = 4096;
		break;
	case 44100:
		if (pixelClk == 2517)
			n = 7007;
		else if (pixelClk == 7417)
			n = 17836;
		else if (pixelClk == 14835)
			n = (ratioClk == 150) ? 17836 : 8918;
		else
			n = 6272;
		break;
	case 48000:
		if (pixelClk == 2517)
			n = (ratioClk == 150) ? 9152 : 6864;
		else if (pixelClk == 2702)
			n = (ratioClk == 150) ? 8192 : 6144;
		else if (pixelClk == 7417)
			n = 11648;
		else if (pixelClk == 14835)
			n = (ratioClk == 150) ? 11648 : 5824;
		else
			n = 6144;
		break;
	case 88200:
		n = audio_computen(baseAddr, 44100, pixelClk, ratioClk) * 2;
		break;
	case 96000:
		n = audio_computen(baseAddr, 48000, pixelClk, ratioClk) * 2;
		break;
	case 176400:
		n = audio_computen(baseAddr, 44100, pixelClk, ratioClk) * 4;
		break;
	case 192000:
		n = audio_computen(baseAddr, 48000, pixelClk, ratioClk) * 4;
		break;
	default:
		break;
	}
	return n;
}

u32 audio_computects(u16 baseAddr, u32 freq, u16 pixelClk, u16 ratioClk)
{
	u32 cts = 0;
	switch (freq)
	{
		case 32000:
			if (pixelClk == 29700)
			{
				cts = 222750;
				break;
			}
		case 48000:
		case 96000:
		case 192000:
			switch (pixelClk)
			{
				case 2520:
				case 2700:
				case 5400:
				case 7425:
				case 14850:
					cts = pixelClk * 10;
					break;
				case 29700:
					cts = 247500;
					break;
				default:
					/* All other TMDS clocks are not supported by DWC_hdmi_tx
					 * the TMDS clocks divided or multiplied by 1,001
					 * coefficients are not supported.
					 */
					LOG_WARNING("TMDS is not guaranteed to give proper CTS,\
									refer to DWC_HDMI_TX databook");
					cts = (pixelClk * ratioClk *\
						audio_computen(baseAddr, freq, pixelClk, ratioClk)) / (128 * freq);
					break;
			}
			break;
		case 44100:
		case 88200:
		case 176400:
			switch (pixelClk)
			{
				case 2520:
					cts = 28000;
					break;
				case 2700:
					cts = 30000;
					break;
				case 5400:
					cts = 60000;
					break;
				case 7425:
					cts = 82500;
					break;
				case 10800:
					cts = 120000;
					break;
				case 14850:
					cts = 165000;
					break;
				case 29700:
					cts = 247500;
					break;
				default:
					/* All other TMDS clocks are not supported by DWC_hdmi_tx
					 * the TMDS clocks divided or multiplied by 1,001
					 * coefficients are not supported.
					 */
					LOG_WARNING("TMDS is not guaranteed to give proper CTS,\
								refer to DWC_HDMI_TX databook");
					cts = (pixelClk * ratioClk *\
						audio_computen(baseAddr, freq, pixelClk, ratioClk)) / (128 * freq);
					break;
			}
			break;
		default:
			break;
	}
	return (cts * ratioClk) / 100;
}

void audio_dmarequestaddress(u16 baseAddr, u32 startAddress, u32 stopAddress)
{
	LOG_TRACE();
	halaudiodma_startaddress(baseAddr + aud_base_addr + audio_dma, startAddress);
	halaudiodma_stopaddress(baseAddr + aud_base_addr + audio_dma, stopAddress);
}

u16 audio_dmagetcurrentburstlength(u16 baseAddr)
{
	LOG_TRACE();
	return halaudiodma_currentburstlength(
		baseAddr + aud_base_addr + audio_dma);
}

u32 audio_dmagetcurrentoperationaddress(u16 baseAddr)
{
	LOG_TRACE();
	return halaudiodma_currentoperationaddress(
		baseAddr + aud_base_addr + audio_dma);
}


void audio_dmastartread(u16 baseAddr)
{
	LOG_TRACE();
	halaudiodma_start(baseAddr + aud_base_addr + audio_dma);
}


void audio_dmastopread(u16 baseAddr)
{
	LOG_TRACE();
	halaudiodma_stop(baseAddr + aud_base_addr + audio_dma);
}

void audio_dmainterruptenable(u16 baseAddr, u8 mask)
{
	LOG_TRACE();
	halaudiodma_dmainterruptmask(baseAddr + aud_base_addr + audio_dma, mask);
}

u8 audio_dmainterruptenablestatus(u16 baseAddr)
{
	LOG_TRACE();
	return halaudiodma_dmainterruptmaskstatus(
		baseAddr + aud_base_addr + audio_dma);
}
