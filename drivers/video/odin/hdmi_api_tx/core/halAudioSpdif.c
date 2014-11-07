/*
 * halAudioSpdif.c
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "halAudioSpdif.h"
#include "../bsp/access.h"
#include "../util/log.h"
/* register offsets */
static const u8 aud_spdif0 = 0x00;
static const u8 aud_spdif1 = 0x01;
static const u8 aud_spdifint = 0x02;

void halaudiospdif_resetfifo(u16 baseAddr)
{
	LOG_TRACE();
	access_corewrite(1, baseAddr + aud_spdif0, 7, 1);
}

void halaudiospdif_nonlinearpcm(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, baseAddr + aud_spdif1, 7, 1);
}

void halaudiospdif_datawidth(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddr + aud_spdif1, 0, 5);
}

void halaudiospdif_interruptmask(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddr + aud_spdifint, 2, 2);
}

void halaudiospdif_interruptpolarity(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, baseAddr + aud_spdifint, 0, 2);
}
