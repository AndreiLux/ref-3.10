/*
 * @file:halVideoSampler.c
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halVideoSampler.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 tx_invid0 = 0x00;
static const u8 tx_instuffing = 0x01;
static const u8 tx_gydata0 = 0x02;
static const u8 tx_gydata1 = 0x03;
static const u8 tx_rcrdata0 = 0x04;
static const u8 tx_rcrdata1 = 0x05;
static const u8 tx_bcbdata0 = 0x06;
static const u8 tx_bcbdata1 = 0x07;

void halvideosampler_internaldataenablegenerator(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + tx_invid0), 7, 1);
}

void halvideosampler_videomapping(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + tx_invid0), 0, 5);
}

void halvideosampler_stuffinggy(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	access_corewritebyte((u8) (value >> 0), (baseAddr + tx_gydata0));
	access_corewritebyte((u8) (value >> 8), (baseAddr + tx_gydata1));
	access_corewrite(1, (baseAddr + tx_instuffing), 0, 1);
}

void halvideosampler_stuffingrcr(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	access_corewritebyte((u8) (value >> 0), (baseAddr + tx_rcrdata0));
	access_corewritebyte((u8) (value >> 8), (baseAddr + tx_rcrdata1));
	access_corewrite(1, (baseAddr + tx_instuffing), 1, 1);
}

void halvideosampler_stuffingbcb(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	access_corewritebyte((u8) (value >> 0), (baseAddr + tx_bcbdata0));
	access_corewritebyte((u8) (value >> 8), (baseAddr + tx_bcbdata1));
	access_corewrite(1, (baseAddr + tx_instuffing), 2, 1);
}
