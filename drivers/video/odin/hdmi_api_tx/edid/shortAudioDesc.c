/*
 * shortAudioDesc.c
 *
 *  Created on: Jul 22, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include <linux/kernel.h>
#include "shortAudioDesc.h"
#include "../util/bitOperation.h"
#include "../util/log.h"

void shortaudiodesc_reset(shortAudioDesc_t *sad)
{
	sad->mformat = 0;
	sad->mmaxchannels = 0;
	sad->msamplerates = 0;
	sad->mbyte3 = 0;
}

int shortaudiodesc_parse(shortAudioDesc_t *sad, u8 * data)
{
	LOG_TRACE();
	shortaudiodesc_reset(sad);
	if (data != 0)
	{
		sad->mformat = bitoperation_bitfield(data[0], 3, 4);
		sad->mmaxchannels = bitoperation_bitfield(data[0], 0, 3) + 1;
		sad->msamplerates = bitoperation_bitfield(data[1], 0, 7);
		sad->mbyte3 = data[2];
//		printk("audio desc : fromat = %x, maxchannels = %x, samplerate = %x, byte3 = %x\n",
//				sad->mformat, sad->mmaxchannels, sad->msamplerates, sad->mbyte3);
		return TRUE;
	}
	return FALSE;
}

u8 shortaudiodesc_getbyte3(shortAudioDesc_t *sad)
{
	return sad->mbyte3;
}

u8 shortaudiodesc_getformatcode(shortAudioDesc_t *sad)
{
	return sad->mformat;
}

u8 shortaudiodesc_getmaxchannels(shortAudioDesc_t *sad)
{
	return sad->mmaxchannels;
}

u8 shortaudiodesc_getsamplerates(shortAudioDesc_t *sad)
{
	return sad->msamplerates;
}

int shortaudiodesc_support32k(shortAudioDesc_t *sad)
{
	return (bitoperation_bitfield(sad->msamplerates, 0, 1) == 1) ? TRUE : FALSE;
}

int shortaudiodesc_support44k1(shortAudioDesc_t *sad)
{
	return (bitoperation_bitfield(sad->msamplerates, 1, 1) == 1) ? TRUE : FALSE;
}

int shortaudiodesc_support48k(shortAudioDesc_t *sad)
{
	return (bitoperation_bitfield(sad->msamplerates, 2, 1) == 1) ? TRUE : FALSE;
}

int shortaudiodesc_support88k2(shortAudioDesc_t *sad)
{
	return (bitoperation_bitfield(sad->msamplerates, 3, 1) == 1) ? TRUE : FALSE;
}

int shortaudiodesc_support96k(shortAudioDesc_t *sad)
{
	return (bitoperation_bitfield(sad->msamplerates, 4, 1) == 1) ? TRUE : FALSE;
}

int shortaudiodesc_support176k4(shortAudioDesc_t *sad)
{
	return (bitoperation_bitfield(sad->msamplerates, 5, 1) == 1) ? TRUE : FALSE;
}

int shortaudiodesc_support192k(shortAudioDesc_t *sad)
{
	return (bitoperation_bitfield(sad->msamplerates, 6, 1) == 1) ? TRUE : FALSE;
}

u8 shortaudiodesc_getmaxbitrate(shortAudioDesc_t *sad)
{
	if (sad->mformat > 1 && sad->mformat < 9)
	{
		return sad->mbyte3;
	}
	LOG_WARNING("Information is not valid for this format");
	return 0;
}

int shortaudiodesc_support16bit(shortAudioDesc_t *sad)
{
	if (sad->mformat == 1)
	{
		return (bitoperation_bitfield(sad->mbyte3, 0, 1) == 1) ? TRUE : FALSE;
	}
	LOG_WARNING("Information is not valid for this format");
	return FALSE;
}

int shortaudiodesc_support20bit(shortAudioDesc_t *sad)
{
	if (sad->mformat == 1)
	{
		return (bitoperation_bitfield(sad->mbyte3, 1, 1) == 1) ? TRUE : FALSE;
	}
	LOG_WARNING("Information is not valid for this format");
	return FALSE;
}

int shortaudiodesc_support24bit(shortAudioDesc_t *sad)
{
	if (sad->mformat == 1)
	{
		return (bitoperation_bitfield(sad->mbyte3, 2, 1) == 1) ? TRUE : FALSE;
	}
	LOG_WARNING("Information is not valid for this format");
	return FALSE;
}
