/*
 * halAudioGpa.c
 *
 * Created on Oct. 30th 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halAudioGpa.h"
#include "../bsp/access.h"
#include "../util/log.h"

static const u16 gp_conf0 = 0x00;
static const u16 gp_conf1 = 0x01;
static const u16 gp_conf2 = 0x02;
static const u16 gp_stat  = 0x03;

static const u16 gp_mask  = 0x06;
static const u16 gp_pol   = 0x05;

void halaudiogpa_resetfifo(u16 baseAddress)
{
 	LOG_TRACE();
	access_corewrite(1, baseAddress + gp_conf0, 0, 1);
}

void halaudiogpa_channelenable(u16 baseAddress, u8 enable, u8 channel)
{
	LOG_TRACE();
	access_corewrite(enable, baseAddress + gp_conf1, channel, 1);
}

void halaudiogpa_hbrenable(u16 baseAddress, u8 enable)
{
 	LOG_TRACE();
 	/* 8 channels must be enabled */
	access_corewrite(enable, baseAddress + gp_conf2, 0, 1);
}

void halaudiogpa_insertpucv(u16 baseAddress, u8 enable)
{
 	LOG_TRACE();
	access_corewrite(enable, baseAddress + gp_conf2, 1, 1);
}

u8 halaudiogpa_interruptstatus(u16 baseAddress)
{
	LOG_TRACE();
	return access_coreread(baseAddress + gp_stat, 0, 2);
}

void halaudiogpa_interruptmask(u16 baseAddress, u8 value)
{
	LOG_TRACE();
	access_corewrite(value, baseAddress + gp_mask, 0, 2);
}

void halaudiogpa_interruptpolority(u16 baseAddress, u8 value)
{
	LOG_TRACE();
	access_corewrite(value, baseAddress + gp_pol, 0, 2);
}
