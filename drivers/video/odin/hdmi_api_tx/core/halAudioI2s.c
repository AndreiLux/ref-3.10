/*
 * hal_audio_i2s.c
 *
 *  Created on: Jun 29, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "halAudioI2s.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 aud_conf0 = 0x00;
static const u8 aud_conf1 = 0x01;
static const u8 aud_int = 0x02;

void halaudioi2s_resetfifo(u16 baseAddress)
{
	LOG_TRACE();
	access_corewrite(1, baseAddress + aud_conf0, 7, 1);
}

void halaudioi2s_select(u16 baseAddress, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, baseAddress + aud_conf0, 5, 1);
}

void halaudioi2s_dataenable(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + aud_conf0, 0, 4);
}

void halaudioi2s_datamode(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + aud_conf1, 5, 3);
}

void halaudioi2s_datawidth(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + aud_conf1, 0, 5);
}

void halaudioi2s_interruptmask(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + aud_int, 2, 2);
}

void halaudioi2s_interruptpolarity(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + aud_int, 0, 2);
}

