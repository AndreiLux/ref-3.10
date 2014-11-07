/*
 * @file:halInterrupt.c
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halInterrupt.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 ih_fc_stat0 = 0x00;
static const u8 ih_fc_stat1 = 0x01;
static const u8 ih_fc_stat2 = 0x02;
static const u8 ih_as_stat0 = 0x03;
static const u8 ih_phy_stat0 = 0x04;
static const u8 ih_i2cm_stat0 = 0x05;
static const u8 ih_cec_stat0 = 0x06;
static const u8 ih_vp_stat0 = 0x07;
static const u8 ih_i2cmphy_stat0 = 0x08;
static const u8 ih_ahbdmaaud_stat0 = 0x09;

static const u8 ih_mute_fc_stat0 = 0x80;
static const u8 ih_mute_fc_stat1 = 0x81;
static const u8 ih_mute_fc_stat2 = 0x82;
static const u8 ih_mute_ac_stat0 = 0x83;
static const u8 ih_mute_phy_stat0 = 0x84;
static const u8 ih_mute_i2cm_stat0 = 0x85;
static const u8 ih_mute_cec_stat0 = 0x86;
static const u8 ih_mute_vp_stat0 = 0x87;
static const u8 ih_mute_i2cmphy_stat0 = 0x88;
static const u8 ih_mute_ahbdmaaud_stat0 = 0x89;
static const u8 ih_mute = 0xFF;

u8 halinterrupt_audiopacketsstate(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + ih_fc_stat0);
}

void halinterrupt_audiopacketsclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_fc_stat0));
}

void halinterrupt_audiopacketsmute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_mute_fc_stat0));
}

u8 halinterrupt_otherpacketsstate(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + ih_fc_stat1);
}

void halinterrupt_otherpacketsclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_fc_stat1));
}

void halinterrupt_otherpacketsmute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_mute_fc_stat1));
}

u8 halinterrupt_packetsoverflowstate(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + ih_fc_stat2);
}

void halinterrupt_packetsoverflowclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_fc_stat2));
}

void halinterrupt_packetsoverflowmute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_mute_fc_stat2));
}
u8 halinterrupt_audiosamplestate(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + ih_as_stat0);
}

void halinterrupt_audiosampleclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_as_stat0));
}

void halinterrupt_audiosamplemute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_mute_ac_stat0));
}

u8 halinterrupt_phystate(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + ih_phy_stat0);
}

void halinterrupt_phyclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_phy_stat0));
}

void halinterrupt_phymute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_mute_phy_stat0));
}

u8 halinterrupt_i2cddcstate(u16 baseAddr)

{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + ih_i2cm_stat0);
}
void halinterrupt_i2cddcclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_i2cm_stat0));
}

void halinterrupt_i2cddcmute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_mute_i2cm_stat0));
}

u8 halinterrupt_cecstate(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + ih_cec_stat0);
}

void halinterrupt_cecclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_cec_stat0));
}

void halinterrupt_cecmute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_mute_cec_stat0));
}

u8 halinterrupt_videopacketizerstate(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + ih_vp_stat0);
}

void halinterrupt_videopacketizerclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_vp_stat0));
}

void halinterrupt_videopacketizermute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_mute_vp_stat0));
}

u8 halinterrupt_i2cphystate(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + ih_i2cmphy_stat0);
}

void halinterrupt_i2cphyclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_i2cmphy_stat0));
}

void halinterrupt_i2cphymute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_mute_i2cmphy_stat0));
}

u8 halinterrupt_audiodmastate(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + ih_ahbdmaaud_stat0);
}

void halinterrupt_audiodmaclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_ahbdmaaud_stat0));
}

void halinterrupt_audiodmamute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_mute_ahbdmaaud_stat0));
}


u8 halinterrupt_mutei2cstate(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + ih_mute_i2cm_stat0);
}

void halinterrupt_mutei2cclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_mute_i2cm_stat0));
}
void halinterrupt_mute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + ih_mute));
}
