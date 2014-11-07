/*
 * @file:halSourcePhy.c
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halSourcePhy.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 phy_conf0 = 0x00;
static const u8 phy_tst0 = 0x01;
static const u8 phy_tst1 = 0x02;
static const u8 phy_tst2 = 0x03;
static const u8 phy_stat0 = 0x04;
static const u8 phy_mask0 = 0x06;
static const u8 phy_pol0 = 0x07;

void halsourcephy_powerdown(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + phy_conf0), 7, 1);
}

void halsourcephy_enabletmds(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + phy_conf0), 6, 1);
}

void halsourcephy_gen2pddq(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + phy_conf0), 4, 1);
}

void halsourcephy_gen2txpoweron(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + phy_conf0), 3, 1);
}

void halsourcephy_gen2enhpdrxsense(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + phy_conf0), 2, 1);
}

void halsourcephy_dataenablepolarity(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + phy_conf0), 1, 1);
}

void halsourcephy_interfacecontrol(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + phy_conf0), 0, 1);
}

void halsourcephy_testclear(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + phy_tst0), 5, 1);
}

void halsourcephy_testenable(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + phy_tst0), 4, 1);
}

void halsourcephy_testclock(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + phy_tst0), 0, 1);
}

void halsourcephy_testdatain(u16 baseAddr, u8 data)
{
	LOG_TRACE1(data);
	access_corewritebyte(data, (baseAddr + phy_tst1));
}

u8 halsourcephy_testdataout(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddr + phy_tst2);
}

u8 halsourcephy_phaselockloopstate(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread((baseAddr + phy_stat0), 0, 1);
}

u8 halsourcephy_hotplugstate(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread((baseAddr + phy_stat0), 1, 1);
}

void halsourcephy_interruptmask(u16 baseAddr, u8 mask)
{
	LOG_TRACE1(mask);
	access_corewritebyte(mask, (baseAddr + phy_mask0));
}

u8 halsourcephy_interruptmaskstatus(u16 baseAddr, u8 mask)
{
	LOG_TRACE1(mask);
	return access_corereadbyte(baseAddr + phy_mask0) & mask;
}

void halsourcephy_interruptpolarity(u16 baseAddr, u8 bitShift, u8 value)
{
	LOG_TRACE2(bitShift, value);
	access_corewrite(value, (baseAddr + phy_pol0), bitShift, 1);
}

u8 halsourcephy_interruptpolaritystatus(u16 baseAddr, u8 mask)
{
	LOG_TRACE1(mask);
	return access_corereadbyte(baseAddr + phy_pol0) & mask;
}
