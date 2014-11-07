/*
 * @file:halSourcePhy.h
 *
 *  Created on: Jul 1, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02

 */

#ifndef HALSOURCEPHY_H_
#define HALSOURCEPHY_H_

#include "../util/types.h"

void halsourcephy_powerdown(u16 baseAddr, u8 bit);

void halsourcephy_enabletmds(u16 baseAddr, u8 bit);

void halsourcephy_gen2pddq(u16 baseAddr, u8 bit);

void halsourcephy_gen2txpoweron(u16 baseAddr, u8 bit);

void halsourcephy_gen2enhpdrxsense(u16 baseAddr, u8 bit);

void halsourcephy_dataenablepolarity(u16 baseAddr, u8 bit);

void halsourcephy_interfacecontrol(u16 baseAddr, u8 bit);

void halsourcephy_testclear(u16 baseAddr, u8 bit);

void halsourcephy_testenable(u16 baseAddr, u8 bit);

void halsourcephy_testclock(u16 baseAddr, u8 bit);

void halsourcephy_testdatain(u16 baseAddr, u8 data);

u8 halsourcephy_testdataout(u16 baseAddr);

u8 halsourcephy_phaselockloopstate(u16 baseAddr);

u8 halsourcephy_hotplugstate(u16 baseAddr);

void halsourcephy_interruptmask(u16 baseAddr, u8 value);

u8 halsourcephy_interruptmaskstatus(u16 baseAddr, u8 mask);

void halsourcephy_interruptpolarity(u16 baseAddr, u8 bitShift, u8 value);

u8 halsourcephy_interruptpolaritystatus(u16 baseAddr, u8 mask);


#endif /* HALSOURCEPHY_H_ */
