/*
 * @file:halAudioDma.c
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "halAudioDma.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 ahb_dma_conf0     = 0x00;
static const u8 ahb_dma_start     = 0x01;
static const u8 ahb_dma_stop      = 0x02;
static const u8 ahb_dma_thrsld    = 0x03;
static const u8 ahb_dma_straddr0  = 0x04;
static const u8 ahb_dma_straddr1  = 0x05;
static const u8 ahb_dma_straddr2  = 0x06;
static const u8 ahb_dma_straddr3  = 0x07;
static const u8 ahb_dma_stpaddr0  = 0x08;
static const u8 ahb_dma_stpaddr1  = 0x09;
static const u8 ahb_dma_stpaddr2  = 0x0A;
static const u8 ahb_dma_stpaddr3  = 0x0B;
static const u8 ahb_dma_bstaddr0  = 0x0C;
static const u8 ahb_dma_bstaddr1  = 0x0D;
static const u8 ahb_dma_bstaddr2  = 0x0E;
static const u8 ahb_dma_bstaddr3  = 0x0F;
static const u8 ahb_dma_mblength0 = 0x10;
static const u8 ahb_dma_mblength1 = 0x11;
static const u8 ahb_dma_mask      = 0x14;
static const u8 ahb_dma_conf1     = 0x16;
static const u8 ahb_dma_buffmask  = 0x19;

void halaudiodma_resetfifo(u16 baseAddress)
{
 	LOG_TRACE();
	access_corewrite(1, baseAddress + ahb_dma_conf0, 7, 1);
}

void halaudiodma_hbrenable(u16 baseAddress, u8 enable)
{
 	LOG_TRACE();
 	/* 8 channels must be enabled */
	access_corewrite(enable, baseAddress + ahb_dma_conf0, 4, 1);
}

void halaudiodma_hlockenable(u16 baseAddress, u8 enable)
{
 	LOG_TRACE();
	access_corewrite(enable, baseAddress + ahb_dma_conf0, 3, 1);
}

void halaudiodma_fixburstmode(u16 baseAddress, u8 fixedBeatIncrement)
{
	LOG_TRACE();
	access_corewrite((fixedBeatIncrement > 3)?\
		fixedBeatIncrement: 0, baseAddress + ahb_dma_conf0, 1, 2);
	access_corewrite((fixedBeatIncrement > 3)?\
		0: 1, baseAddress + ahb_dma_conf0, 0, 1);
}

void halaudiodma_start(u16 baseAddress)
{
	LOG_TRACE();
	access_corewrite(1, baseAddress + ahb_dma_start, 0, 1);
}

void halaudiodma_stop(u16 baseAddress)
{
	access_corewrite(1, baseAddress + ahb_dma_stop, 0, 1);
}

void halaudiodma_threshold(u16 baseAddress, u8 threshold)
{
	LOG_TRACE();
	access_corewritebyte(threshold, baseAddress + ahb_dma_thrsld);
}

void halaudiodma_startaddress(u16 baseAddress, u32 startAddress)
{
	LOG_TRACE();
	access_corewritebyte((u8)(startAddress), baseAddress + ahb_dma_straddr0);
	access_corewritebyte((u8)(startAddress >> 8), baseAddress + ahb_dma_straddr1);
	access_corewritebyte((u8)(startAddress >> 16), baseAddress + ahb_dma_straddr2);
	access_corewritebyte((u8)(startAddress >> 24), baseAddress + ahb_dma_straddr3);
}

void halaudiodma_stopaddress(u16 baseAddress, u32 stopAddress)
{
	LOG_TRACE();
	access_corewritebyte((u8)(stopAddress), baseAddress + ahb_dma_stpaddr0);
	access_corewritebyte((u8)(stopAddress >> 8), baseAddress + ahb_dma_stpaddr1);
	access_corewritebyte((u8)(stopAddress >> 16), baseAddress + ahb_dma_stpaddr2);
	access_corewritebyte((u8)(stopAddress >> 24), baseAddress + ahb_dma_stpaddr3);
}

u32 halaudiodma_currentoperationaddress(u16 baseAddress)
{
	u32 temp = 0;
	LOG_TRACE();
	temp = access_corereadbyte(baseAddress + ahb_dma_bstaddr0);
	temp |= access_corereadbyte(baseAddress + ahb_dma_bstaddr0) << 8;
	temp |= access_corereadbyte(baseAddress + ahb_dma_bstaddr0) << 16;
	temp |= access_corereadbyte(baseAddress + ahb_dma_bstaddr0) << 24;
	return temp;
}

u16 halaudiodma_currentburstlength(u16 baseAddress)
{
	u16 temp = 0;
	LOG_TRACE();
	temp = access_corereadbyte(baseAddress + ahb_dma_mblength0);
	temp |= access_corereadbyte(baseAddress + ahb_dma_mblength1) << 8;
	return temp;
}

void halaudiodma_dmainterruptmask(u16 baseAddress, u8 mask)
{
	LOG_TRACE1(mask);
	access_corewritebyte(mask, baseAddress + ahb_dma_mask);
}

void halaudiodma_bufferinterruptmask(u16 baseAddress, u8 mask)
{
	LOG_TRACE1(mask);
	access_corewritebyte(mask, baseAddress + ahb_dma_buffmask);
}

u8 halaudiodma_dmainterruptmaskstatus(u16 baseAddress)
{
	LOG_TRACE();
	return access_corereadbyte(baseAddress + ahb_dma_mask);
}
u8 halaudiodma_bufferinterruptmaskstatus(u16 baseAddress)
{
	return access_corereadbyte(baseAddress + ahb_dma_buffmask);
}

void halaudiodma_channelenable(u16 baseAddress, u8 enable, u8 channel)
{
	LOG_TRACE1(channel);
	access_corewrite(enable, baseAddress + ahb_dma_conf1, channel, 1);
}
