/*
 * halColorSpaceConverter.c
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "halColorSpaceConverter.h"
#include "../bsp/access.h"
#include "../util/log.h"
/* Color Space Converter register offsets */
static const u8 csc_cfg = 0x00;
static const u8 csc_scale = 0x01;
static const u8 csc_coef_a1_msb = 0x02;
static const u8 csc_coef_a1_lsb = 0x03;
static const u8 csc_coef_a2_msb = 0x04;
static const u8 csc_coef_a2_lsb = 0x05;
static const u8 csc_coef_a3_msb = 0x06;
static const u8 csc_coef_a3_lsb = 0x07;
static const u8 csc_coef_a4_msb = 0x08;
static const u8 csc_coef_a4_lsb = 0x09;
static const u8 csc_coef_b1_msb = 0x0A;
static const u8 csc_coef_b1_lsb = 0x0B;
static const u8 csc_coef_b2_msb = 0x0C;
static const u8 csc_coef_b2_lsb = 0x0D;
static const u8 csc_coef_b3_msb = 0x0E;
static const u8 csc_coef_b3_lsb = 0x0F;
static const u8 csc_coef_b4_msb = 0x10;
static const u8 csc_coef_b4_lsb = 0x11;
static const u8 csc_coef_c1_msb = 0x12;
static const u8 csc_coef_c1_lsb = 0x13;
static const u8 csc_coef_c2_msb = 0x14;
static const u8 csc_coef_c2_lsb = 0x15;
static const u8 csc_coef_c3_msb = 0x16;
static const u8 csc_coef_c3_lsb = 0x17;
static const u8 csc_coef_c4_msb = 0x18;
static const u8 csc_coef_c4_lsb = 0x19;

void halcolorspaceconverter_interpolation(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	/* 2-bit width */
	access_corewrite(value, baseAddr + csc_cfg, 4, 2);
}

void halcolorspaceconverter_decimation(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	/* 2-bit width */
	access_corewrite(value, baseAddr + csc_cfg, 0, 2);
}

void halcolorspaceconverter_colordepth(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	/* 4-bit width */
	access_corewrite(value, baseAddr + csc_scale, 4, 4);
}

void halcolorspaceconverter_scalefactor(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	/* 2-bit width */
	access_corewrite(value, baseAddr + csc_scale, 0, 2);
}

void halcolorspaceconverter_coefficienta1(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 15-bit width */
	access_corewritebyte((u8) (value), baseAddr + csc_coef_a1_lsb);
	access_corewrite((u8) (value >> 8), baseAddr + csc_coef_a1_msb, 0, 7);
}

void halcolorspaceconverter_coefficienta2(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 15-bit width */
	access_corewritebyte((u8) (value), baseAddr + csc_coef_a2_lsb);
	access_corewrite((u8) (value >> 8), baseAddr + csc_coef_a2_msb, 0, 7);
}

void halcolorspaceconverter_coefficienta3(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 15-bit width */
	access_corewritebyte((u8) (value), baseAddr + csc_coef_a3_lsb);
	access_corewrite((u8) (value >> 8), baseAddr + csc_coef_a3_msb, 0, 7);
}

void halcolorspaceconverter_coefficienta4(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 15-bit width */
	access_corewritebyte((u8) (value), baseAddr + csc_coef_a4_lsb);
	access_corewrite((u8) (value >> 8), baseAddr + csc_coef_a4_msb, 0, 7);
}

void halcolorspaceconverter_coefficientb1(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 15-bit width */
	access_corewritebyte((u8) (value), baseAddr + csc_coef_b1_lsb);
	access_corewrite((u8) (value >> 8), baseAddr + csc_coef_b1_msb, 0, 7);
}

void halcolorspaceconverter_coefficientb2(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 15-bit width */
	access_corewritebyte((u8) (value), baseAddr + csc_coef_b2_lsb);
	access_corewrite((u8) (value >> 8), baseAddr + csc_coef_b2_msb, 0, 7);
}

void halcolorspaceconverter_coefficientb3(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 15-bit width */
	access_corewritebyte((u8) (value), baseAddr + csc_coef_b3_lsb);
	access_corewrite((u8) (value >> 8), baseAddr + csc_coef_b3_msb, 0, 7);
}

void halcolorspaceconverter_coefficientb4(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 15-bit width */
	access_corewritebyte((u8) (value), baseAddr + csc_coef_b4_lsb);
	access_corewrite((u8) (value >> 8), baseAddr + csc_coef_b4_msb, 0, 7);
}

void halcolorspaceconverter_coefficientc1(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 15-bit width */
	access_corewritebyte((u8) (value), baseAddr + csc_coef_c1_lsb);
	access_corewrite((u8) (value >> 8), baseAddr + csc_coef_c1_msb, 0, 7);
}

void halcolorspaceconverter_coefficientc2(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 15-bit width */
	access_corewritebyte((u8) (value), baseAddr + csc_coef_c2_lsb);
	access_corewrite((u8) (value >> 8), baseAddr + csc_coef_c2_msb, 0, 7);
}

void halcolorspaceconverter_coefficientc3(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	/* 15-bit width */
	access_corewritebyte((u8) (value), baseAddr + csc_coef_c3_lsb);
	access_corewrite((u8) (value >> 8), baseAddr + csc_coef_c3_msb, 0, 7);
}

void halcolorspaceconverter_coefficientc4(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	access_corewritebyte((u8) (value), baseAddr + csc_coef_c4_lsb);
	access_corewrite((u8) (value >> 8), baseAddr + csc_coef_c4_msb, 0, 7);
}
