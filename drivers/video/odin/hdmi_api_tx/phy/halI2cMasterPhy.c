/*
 * @file:halI2cMasterPhy.c
 *
 *  Created on: Jul 1, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halI2cMasterPhy.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 phy_i2cm_slave = 0x00;
static const u8 phy_i2cm_address = 0x01;
static const u8 phy_i2cm_datao = 0x02;
static const u8 phy_i2cm_datai = 0x04;
static const u8 phy_i2cm_operation = 0x06;
static const u8 phy_i2cm_div = 0x09;
static const u8 phy_i2cm_softrstz = 0x0A;

void hali2cmasterphy_slaveaddress(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + phy_i2cm_slave), 0, 7);
}

void hali2cmasterphy_registeraddress(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewritebyte(value, (baseAddr + phy_i2cm_address));
}

void hali2cmasterphy_writedata(u16 baseAddr, u16 value)
{
	LOG_TRACE1(value);
	access_corewritebyte((u8) (value >> 8), (baseAddr + phy_i2cm_datao + 0));
	access_corewritebyte((u8) (value >> 0), (baseAddr + phy_i2cm_datao + 1));
}

/* 121129 attached I2C read function by hojung.han*/
u16 hali2cmasterphy_readdata(u32 baseAddr)
{
	u16 data;
	u8 data_0, data_1;
	data_0 = access_corereadbyte(baseAddr+phy_i2cm_datai+0);
	data_1 = access_corereadbyte(baseAddr+phy_i2cm_datai+1);
	data = data_0<<8 | data_1;
	return data;
}
/* 121129 attached I2C read function by hojung.han*/
void hali2cmasterphy_readrequest(u32 baseAddr)
{
	LOG_TRACE();
	access_corewrite(1, (baseAddr + phy_i2cm_operation), 0, 1);
}
void hali2cmasterphy_writerequest(u16 baseAddr)
{
	LOG_TRACE();
	access_corewrite(1, (baseAddr + phy_i2cm_operation), 4, 1);
}

void hali2cmasterphy_fastmode(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + phy_i2cm_div), 0, 1);
}

void hali2cmasterphy_reset(u16 baseAddr)
{
	LOG_TRACE();
	access_corewrite(0, (baseAddr + phy_i2cm_softrstz), 0, 1);
}
