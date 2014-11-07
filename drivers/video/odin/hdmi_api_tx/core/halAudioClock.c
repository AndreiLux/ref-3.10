/*
 * hal_audio_clock.c
 *
 *  Created on: Jun 28, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "halAudioClock.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 aud_n1 = 0x00;
static const u8 aud_n2 = 0x01;
static const u8 aud_n3 = 0x02;
static const u8 aud_cts1 = 0x03;
static const u8 aud_cts2 = 0x04;
static const u8 aud_cts3 = 0x05;
static const u8 aud_inputclkfs = 0x06;

void halaudioclock_n(u16 baseAddress, u32 value)
{
	LOG_TRACE();
	/* 19-bit width */
	access_corewritebyte((u8) (value >> 0), baseAddress + aud_n1);
	access_corewritebyte((u8) (value >> 8), baseAddress + aud_n2);
	access_corewrite((u8) (value >> 16), baseAddress + aud_n3, 0, 3);
	/* no shift */
	access_corewrite(0, baseAddress + aud_cts3, 5, 3);
}

/*
 * @param value: new CTS value or set to zero for automatic mode
 */
void halaudioclock_cts(u16 baseAddress, u32 value)
{
	LOG_TRACE1(value);
	/* manual or automatic? must be programmed first */
	access_corewrite((value != 0) ? 1 : 0, baseAddress + aud_cts3, 4, 1);
	/* 19-bit width */
	access_corewritebyte((u8) (value >> 0), baseAddress + aud_cts1);
	access_corewritebyte((u8) (value >> 8), baseAddress + aud_cts2);
	access_corewrite((u8) (value >> 16), baseAddress + aud_cts3, 0, 3);
}

void halaudioclock_f(u16 baseAddress, u8 value)
{
	LOG_TRACE();
	access_corewrite(value, baseAddress + aud_inputclkfs, 0, 3);
}
