/*
 * @file:halVideoGenerator.c
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halVideoGenerator.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 vg_swrstz = 0x00;
static const u8 vg_conf = 0x01;
static const u8 vg_prep = 0x02;
static const u8 vg_hactive0 = 0x03;
static const u8 vg_hactive1 = 0x04;
static const u8 vg_hblank0 = 0x05;
static const u8 vg_hblank1 = 0x06;
static const u8 vg_hdelay0 = 0x07;
static const u8 vg_hdelay1 = 0x08;
static const u8 vg_hwidth0 = 0x09;
static const u8 vg_hwidth1 = 0x0A;
static const u8 vg_vactive0 = 0x0B;
static const u8 vg_vactive1 = 0x0C;
static const u8 vg_vblank0 = 0x0D;
static const u8 vg_vdelay0 = 0x0E;
static const u8 vg_vwidth0 = 0x0F;
static const u8 vg_vidsource = 0x10;
static const u8 vg_3dstruct = 0x11;
static const u8 vg_ram_addr0 = 0x20;
static const u8 vg_ram_addr1 = 0x21;
static const u8 vg_wrt_ram_ctrl = 0x22;
static const u8 vg_wrt_ram_data = 0x23;
static const u8 vg_wrt_ram_stop_addr0 = 0x24;
static const u8 vg_wrt_ram_stop_addr1 = 0x25;
static const u8 vg_wrt_ram_byte_stop = 0x26;

void halvideogenerator_swreset(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	/* active low */
	access_corewrite(bit ? 0 : 1, (baseAddr + vg_swrstz), 0, 1);
}

void halvideogenerator_ycc(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + vg_conf), 7, 1);
}

void halvideogenerator_ycc422(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + vg_conf), 6, 1);
}

void halvideogenerator_vblankosc(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + vg_conf), 5, 1);
}

void halvideogenerator_colorincrement(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + vg_conf), 4, 1);
}

void halvideogenerator_interlaced(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + vg_conf), 3, 1);
}

void halvideogenerator_vsyncpolarity(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + vg_conf), 2, 1);
}

void halvideogenerator_hsyncpolarity(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + vg_conf), 1, 1);
}

void halvideogenerator_dataenablepolarity(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + vg_conf), 0, 1);
}

void halvideogenerator_colorresolution(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + vg_prep), 4, 2);
}

void halvideogenerator_pixelrepetitioninput(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + vg_prep), 0, 4);
}

void halvideogenerator_hactive(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 12-bit width */
	access_corewritebyte((u8) (value), (baseAddr + vg_hactive0));
	access_corewrite((u8) (value >> 8), (baseAddr + vg_hactive1), 0, 5);
}

void halvideogenerator_hblank(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 13-bit width - 1.31a */
	access_corewritebyte((u8) (value), (baseAddr + vg_hblank0));
	access_corewrite((u8) (value >> 8), (baseAddr + vg_hblank1), 0, 5);
}

void halvideogenerator_hsyncedgedelay(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 11-bit width */
	access_corewritebyte((u8) (value), (baseAddr + vg_hdelay0));
	access_corewrite(value >> 8, (baseAddr + vg_hdelay1), 0, 4);
}

void halvideogenerator_hsyncpulsewidth(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 9-bit width */
	access_corewritebyte((u8) (value), (baseAddr + vg_hwidth0));
	access_corewrite((u8) (value >> 8), (baseAddr + vg_hwidth1), 0, 1);
}

void halvideogenerator_vactive(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 11-bit width */
	access_corewritebyte((u8) (value), (baseAddr + vg_vactive0));
	access_corewrite((u8) (value >> 8), (baseAddr + vg_vactive1), 0, 4);
}

void halvideogenerator_vblank(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 8-bit width */
	access_corewritebyte((u8) (value), (baseAddr + vg_vblank0));
}

void halvideogenerator_vsyncedgedelay(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 8-bit width */
	access_corewritebyte((u8) (value), (baseAddr + vg_vdelay0));
}

void halvideogenerator_vsyncpulsewidth(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 6-bit width */
	access_corewrite((u8) (value), (baseAddr + vg_vwidth0), 0, 6);
}

void halvideogenerator_enable3d(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + vg_3dstruct), 4, 1);
}

void halvideogenerator_structure3d(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + vg_3dstruct), 0, 4);
}

void halvideogenerator_writestart(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 13-bit width */
	access_corewritebyte((u8) (value), (baseAddr + vg_ram_addr0));
	access_corewrite((u8) (value >> 8), (baseAddr + vg_ram_addr1), 0, 5);
	/* start RAM write */
	access_corewrite(1, (baseAddr + vg_wrt_ram_ctrl), 0, 1);
}

void halvideogenerator_writedata(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte((u8) (value), (baseAddr + vg_wrt_ram_data));
}

u16 halvideogenerator_writecurrentaddr(u16 baseAddr)
{
	u16 value = 0;
	LOG_TRACE();
	value = access_coreread((baseAddr + vg_wrt_ram_stop_addr1), 0, 5) << 8;
	return (value | access_corereadbyte(baseAddr + vg_wrt_ram_stop_addr0));
}

u8 halvideogenerator_writecurrentbyte(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread((baseAddr + vg_wrt_ram_byte_stop), 0, 3);
}
