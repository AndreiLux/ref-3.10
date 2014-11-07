/*
 * @file:halMainController.c
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halMainController.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 mc_sfrdiv = 0x00;
static const u8 mc_clkdis = 0x01;
static const u8 mc_swrstzreq = 0x02;
static const u8 mc_flowctrl = 0x04;
static const u8 mc_phyrstz = 0x05;
static const u8 mc_lockonclock = 0x06;
static const u8 mc_heacphy_rst = 0x07;

void halmaincontroller_sfrclockdivision(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + mc_sfrdiv), 0, 4);
}

void halmaincontroller_hdcpclockgate(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_clkdis), 6, 1);
}

void halmaincontroller_cecclockgate(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_clkdis), 5, 1);
}

void halmaincontroller_colorspaceconverterclockgate(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_clkdis), 4, 1);
}

void halmaincontroller_audiosamplerclockgate(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_clkdis), 3, 1);
}

void halmaincontroller_pixelrepetitionclockgate(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_clkdis), 2, 1);
}

void halmaincontroller_tmdsclockgate(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_clkdis), 1, 1);
}

void halmaincontroller_pixelclockgate(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_clkdis), 0, 1);
}

void halmaincontroller_cecclockreset(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_swrstzreq), 6, 1);
}

void halmaincontroller_audiogpareset(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_swrstzreq), 7, 1);
}

void halmaincontroller_audiohbrreset(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_swrstzreq), 5, 1);
}

void halmaincontroller_audiospdifreset(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_swrstzreq), 4, 1);
}

void halmaincontroller_audioi2creset(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_swrstzreq), 3, 1);
}

void halmaincontroller_pixelrepetitionclockreset(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_swrstzreq), 2, 1);
}

void halmaincontroller_tmdsclockreset(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_swrstzreq), 1, 1);
}

void halmaincontroller_pixelclockreset(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_swrstzreq), 0, 1);
}

void halmaincontroller_videofeedthroughoff(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + mc_flowctrl), 0, 1);
}

void halmaincontroller_phyreset(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	/* active low */
	access_corewrite(bit ? 0 : 1, (baseAddr + mc_phyrstz), 0, 1);
}

void halmaincontroller_heacphyreset(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	/* active high */
	access_corewrite(bit ? 1 : 0, (baseAddr + mc_heacphy_rst), 0, 1);
}

u8 halmaincontroller_lockonclockstatus(u16 baseAddr, u8 clockDomain)
{
	LOG_TRACE1(clockDomain);
	return access_coreread((baseAddr + mc_lockonclock), clockDomain, 1);
}

void halmaincontroller_lockonclockclear(u16 baseAddr, u8 clockDomain)
{
	LOG_TRACE1(clockDomain);
	access_corewrite(1, (baseAddr + mc_lockonclock), clockDomain, 1);
}
