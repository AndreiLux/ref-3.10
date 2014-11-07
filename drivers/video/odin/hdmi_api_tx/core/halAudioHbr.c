/*
 * hal_audio_hbr.c
 *
 *  Created on: Jun 29, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "halAudioHbr.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 aud_conf0_hbr = 0x00;
static const u8 aud_hbr_status = 0x01;
static const u8 aud_hbrint = 0x02;
static const u8 aud_hbrpol = 0x03;
static const u8 aud_hbrmask = 0x04;

void halaudiohbr_resetfifo(u16 baseAddress)
{
	LOG_TRACE();
	access_corewrite(1, baseAddress + aud_conf0_hbr, 7, 1);
}

void halaudiohbr_select(u16 baseAddress, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, baseAddress + aud_conf0_hbr, 2, 1);
}

void halaudiohbr_interruptmask(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + aud_hbrmask, 0, 3);
}

void halaudiohbr_interruptpolarity(u16 baseAddress, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddress + aud_hbrpol, 0, 3);
}
