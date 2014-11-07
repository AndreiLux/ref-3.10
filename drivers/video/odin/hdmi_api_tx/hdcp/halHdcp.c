/*
 * halHdcp.c
 *
 *  Created on: Jul 19, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include <linux/delay.h>
#include "halHdcp.h"
#include "../bsp/access.h"
#include "../util/log.h"

const u8 a_hdcpcfg0 = 0x00;
const u8 a_hdcpcfg1 = 0x01;
const u8 a_hdcpobs0 = 0x02;
const u8 a_hdcpobs1 = 0x03;
const u8 a_hdcpobs2 = 0x04;
const u8 a_hdcpobs3 = 0x05;
const u8 a_apiintclr = 0x06;
const u8 a_apiintsta = 0x07;
const u8 a_apiintmsk = 0x08;
const u8 a_vidpolcfg = 0x09;
const u8 a_oesswcfg = 0x0A;
const u8 a_coreverlsb = 0x14;
const u8 a_corevermsb = 0x15;
const u8 a_ksvmemctrl = 0x16;
const u8 a_memory = 0x20;
const u16 revocation_offset = 0x299;

void halhdcp_devicemode(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_hdcpcfg0), 0, 1);
}

void halhdcp_enablefeature11(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_hdcpcfg0), 1, 1);
}

void halhdcp_rxdetected(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_hdcpcfg0), 2, 1);
	mdelay(10);
}

void halhdcp_enableavmute(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_hdcpcfg0), 3, 1);
}

void halhdcp_richeck(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_hdcpcfg0), 4, 1);
}

void halhdcp_bypassencryption(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_hdcpcfg0), 5, 1);
}

void halhdcp_enablei2cfastmode(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_hdcpcfg0), 6, 1);
}

void halhdcp_enhancedlinkverification(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_hdcpcfg0), 7, 1);
}

void halhdcp_swreset(u16 baseAddr)
{
	LOG_TRACE();
	/* active low */
	access_corewrite(0, (baseAddr + a_hdcpcfg1), 0, 1);
}

void halhdcp_disableencryption(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_hdcpcfg1), 1, 1);
}

void halhdcp_encodingpacketheader(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_hdcpcfg1), 2, 1);
}

void halhdcp_disableksvlistcheck(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_hdcpcfg1), 3, 1);
}

u8 halhdcp_hdcpengaged(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread((baseAddr + a_hdcpobs0), 0, 1);
}

u8 halhdcp_authenticationstate(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread((baseAddr + a_hdcpobs0), 1, 7);
}

u8 halhdcp_cipherstate(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread((baseAddr + a_hdcpobs2), 3, 3);
}

u8 halhdcp_revocationstate(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread((baseAddr + a_hdcpobs1), 0, 3);
}

u8 halhdcp_oessstate(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread((baseAddr + a_hdcpobs1), 3, 3);
}

u8 halhdcp_eessstate(u16 baseAddr)
{
	LOG_TRACE();
	/* TODO width = 3 or 4? */
	return access_coreread((baseAddr + a_hdcpobs2), 0, 3);
}

u8 halhdcp_debuginfo(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + a_hdcpobs3);
}

void halhdcp_interruptclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + a_apiintclr));
}

u8 halhdcp_interruptstatus(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + a_apiintsta);
}

void halhdcp_interruptmask(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + a_apiintmsk));
}

void halhdcp_hsyncpolarity(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_vidpolcfg), 1, 1);
}

void halhdcp_vsyncpolarity(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_vidpolcfg), 3, 1);
}

void halhdcp_dataenablepolarity(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_vidpolcfg), 4, 1);
}

void halhdcp_unencryptedvideocolor(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + a_vidpolcfg), 5, 2);
}

void halhdcp_oesswindowsize(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + a_oesswcfg));
}

u16 halhdcp_coreversion(u16 baseAddr)
{
	u16 version = 0;
	LOG_TRACE();
	version = access_corereadbyte(baseAddr + a_coreverlsb);
	version |= access_corereadbyte(baseAddr + a_corevermsb) << 8;
	return version;
}

void halhdcp_memoryaccessrequest(u16 baseAddr, u8 bit)
{
	LOG_TRACE();
	access_corewrite(bit, (baseAddr + a_ksvmemctrl), 0, 1);
}

u8 halhdcp_memoryaccessgranted(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread((baseAddr + a_ksvmemctrl), 1, 1);
}

void halhdcp_updateksvliststate(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + a_ksvmemctrl), 3, 1);
	access_corewrite(1, (baseAddr + a_ksvmemctrl), 2, 1);
	access_corewrite(0, (baseAddr + a_ksvmemctrl), 2, 1);
}

u8 halhdcp_ksvlistread(u16 baseAddr, u16 addr)
{
	LOG_TRACE1(addr);
	if (addr >= revocation_offset)
	{
		LOG_WARNING("Invalid address");
	}
	return access_corereadbyte((baseAddr + a_memory) + addr);
}

void halhdcp_revoclistwrite(u16 baseAddr, u16 addr, u8 data)
{
	LOG_TRACE2(addr, data);
	access_corewritebyte(data, (baseAddr + a_memory) + revocation_offset + addr);
}

void halhdcp_key(u16 baseAddr, u8 data)
{
	access_write(data, baseAddr);

}
