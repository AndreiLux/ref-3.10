/*
 * @file:halEdid.c
 *
 *  Created on: Jul 5, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "halEdid.h"
#include "../util/log.h"
#include "../bsp/access.h"

static const u8 slave_addr = 0x00;
static const u8 request_addr = 0x01;
static const u8 data_out = 0x02;
static const u8 data_in = 0x03;
static const u8 operation_g = 0x04;
static const u8 done_int = 0x05;
static const u8 error_int = 0x06;
static const u8 clock_div = 0x07;
static const u8 seg_addr = 0x08;
static const u8 seg_pointer = 0x0A;
static const u8 ss_scl_hcnt = 0x0B;
static const u8 ss_scl_lcnt = 0x0D;
static const u8 fs_scl_hcnt = 0x0F;
static const u8 fs_scl_lcnt = 0x11;

void haledid_slaveaddress(u16 baseAddr, u8 addr)
{
	LOG_TRACE1(addr);
	access_corewrite(addr, (baseAddr + slave_addr), 0, 7);
}

void haledid_requestaddr(u16 baseAddr, u8 addr)
{
	LOG_TRACE1(addr);
	access_corewritebyte(addr, (baseAddr + request_addr));
}

void haledid_writedata(u16 baseAddr, u8 data)
{
	LOG_TRACE1(data);
	access_corewritebyte(data, (baseAddr + data_out));
}

u8 haledid_readdata(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte((baseAddr + data_in));
}

void haledid_requestread(u16 baseAddr)
{
	LOG_TRACE();
	access_corewrite(1, (baseAddr + operation_g), 0, 1);
}

void haledid_requestextread(u16 baseAddr)
{
	LOG_TRACE();
	access_corewrite(1, (baseAddr + operation_g), 1, 1);
}

void haledid_requestwrite(u16 baseAddr)
{
	LOG_TRACE();
	access_corewrite(1, (baseAddr + operation_g), 4, 1);
}

void haledid_masterclockdivision(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	/* bit 4 selects between high and standard speed operation */
	access_corewrite(value, (baseAddr + clock_div), 0, 4);
}

void haledid_segmentaddr(u16 baseAddr, u8 addr)
{
	LOG_TRACE1(addr);
	access_corewritebyte(addr, (baseAddr + seg_addr));
}

void haledid_segmentpointer(u16 baseAddr, u8 pointer)
{
	LOG_TRACE1(pointer);
	access_corewritebyte(pointer, (baseAddr + seg_pointer));
}

void haledid_maskinterrupts(u16 baseAddr, u8 mask)
{
	LOG_TRACE1(mask);
	access_corewrite(mask ? 1 : 0, (baseAddr + done_int), 2, 1);
	access_corewrite(mask ? 1 : 0, (baseAddr + error_int), 2, 1);
	access_corewrite(mask ? 1 : 0, (baseAddr + error_int), 6, 1);
}

void haledid_fastspeedhighcounter(u16 baseAddr, u16 value)
{
	LOG_TRACE2((baseAddr + fs_scl_hcnt), value);
	access_corewritebyte((u8) (value >> 8), (baseAddr + fs_scl_hcnt + 0));
	access_corewritebyte((u8) (value >> 0), (baseAddr + fs_scl_hcnt + 1));
}

void haledid_fastspeedlowcounter(u16 baseAddr, u16 value)
{
	LOG_TRACE2((baseAddr + fs_scl_lcnt), value);
	access_corewritebyte((u8) (value >> 8), (baseAddr + fs_scl_lcnt + 0));
	access_corewritebyte((u8) (value >> 0), (baseAddr + fs_scl_lcnt + 1));
}

void haledid_standardspeedhighcounter(u16 baseAddr, u16 value)
{
	LOG_TRACE2((baseAddr + ss_scl_hcnt), value);
	access_corewritebyte((u8) (value >> 8), (baseAddr + ss_scl_hcnt + 0));
	access_corewritebyte((u8) (value >> 0), (baseAddr + ss_scl_hcnt + 1));
}

void haledid_standardspeedlowcounter(u16 baseAddr, u16 value)
{
	LOG_TRACE2((baseAddr + ss_scl_lcnt), value);
	access_corewritebyte((u8) (value >> 8), (baseAddr + ss_scl_lcnt + 0));
	access_corewritebyte((u8) (value >> 0), (baseAddr + ss_scl_lcnt + 1));
}
