/*
 * @file:halI2cMasterPhy.h
 *
 *  Created on: Jul 1, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 *
 *      NOTE: only write operations are implemented in this module
 *   	This module is used only from PHY GEN2 on
 */

#ifndef HALI2CMASTERPHY_H_
#define HALI2CMASTERPHY_H_

#include "../util/types.h"

void hali2cmasterphy_slaveaddress(u16 baseAddr, u8 value);

void hali2cmasterphy_registeraddress(u16 baseAddr, u8 value);

void hali2cmasterphy_writedata(u16 baseAddr, u16 value);

/* 121129 attached I2C read function by hojung.han*/
u16 hali2cmasterphy_readdata(u32 baseAddr);

/* 121129 attached I2C read function by hojung.han*/
void hali2cmasterphy_readrequest(u32 baseAddr);
void hali2cmasterphy_writerequest(u16 baseAddr);

void hali2cmasterphy_fastmode(u16 baseAddr, u8 bit);

void hali2cmasterphy_reset(u16 baseAddr);

#endif /* HALI2CMASTERPHY_H_ */
