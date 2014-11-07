/*
 * @file:halInterrupt.h
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALINTERRUPT_H_
#define HALINTERRUPT_H_

#include "../util/types.h"

u8 halinterrupt_audiopacketsstate(u16 baseAddr);

void halinterrupt_audiopacketsclear(u16 baseAddr, u8 value);

void halinterrupt_audiopacketsmute(u16 baseAddr, u8 value);

u8 halinterrupt_audiopacketsstate(u16 baseAddr);

void halinterrupt_otherpacketsclear(u16 baseAddr, u8 value);

void halinterrupt_otherpacketsmute(u16 baseAddr, u8 value);

u8 halinterrupt_packetsoverflowstate(u16 baseAddr);

void halinterrupt_packetsoverflowclear(u16 baseAddr, u8 value);

void halinterrupt_packetsoverflowmute(u16 baseAddr, u8 value);

u8 halinterrupt_audiosamplestate(u16 baseAddr);

void halinterrupt_audiosampleclear(u16 baseAddr, u8 value);

void halinterrupt_audiosamplemute(u16 baseAddr, u8 value);

u8 halinterrupt_phystate(u16 baseAddr);

void halinterrupt_phyclear(u16 baseAddr, u8 value);

void halinterrupt_phymute(u16 baseAddr, u8 value);

u8 halinterrupt_i2cddcstate(u16 baseAddr);

void halinterrupt_i2cddcclear(u16 baseAddr, u8 value);

void halinterrupt_i2cddcmute(u16 baseAddr, u8 value);

u8 halinterrupt_cecstate(u16 baseAddr);

void halinterrupt_cecclear(u16 baseAddr, u8 value);

void halinterrupt_cecmute(u16 baseAddr, u8 value);

u8 halinterrupt_videopacketizerstate(u16 baseAddr);

void halinterrupt_videopacketizerclear(u16 baseAddr, u8 value);

void halinterrupt_videopacketizermute(u16 baseAddr, u8 value);

u8 halinterrupt_i2cphystate(u16 baseAddr);

void halinterrupt_i2cphyclear(u16 baseAddr, u8 value);

void halinterrupt_i2cphymute(u16 baseAddr, u8 value);

u8 halinterrupt_audiodmastate(u16 baseAddr);

void halinterrupt_audiodmaclear(u16 baseAddr, u8 value);

void halinterrupt_audiodmamute(u16 baseAddr, u8 value);

u8 halinterrupt_mutei2cstate(u16 baseAddr);

void halinterrupt_mutei2cclear(u16 baseAddr, u8 value);

void halinterrupt_mute(u16 baseAddr, u8 value);

#endif /* HALINTERRUPT_H_ */
