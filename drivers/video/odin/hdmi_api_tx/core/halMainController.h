/*
 * @file:halMainController.h
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALMAINCONTROLLER_H_
#define HALMAINCONTROLLER_H_

#include "../util/types.h"

void halmaincontroller_sfrclockdivision(u16 baseAddr, u8 value);

void halmaincontroller_hdcpclockgate(u16 baseAddr, u8 bit);

void halmaincontroller_cecclockgate(u16 baseAddr, u8 bit);

void halmaincontroller_colorspaceconverterclockgate(u16 baseAddr, u8 bit);

void halmaincontroller_audiosamplerclockgate(u16 baseAddr, u8 bit);

void halmaincontroller_pixelrepetitionclockgate(u16 baseAddr, u8 bit);

void halmaincontroller_tmdsclockgate(u16 baseAddr, u8 bit);

void halmaincontroller_pixelclockgate(u16 baseAddr, u8 bit);

void halmaincontroller_cecclockreset(u16 baseAddr, u8 bit);

void halmaincontroller_audiogpareset(u16 baseAddr, u8 bit);

void halmaincontroller_audiohbrreset(u16 baseAddr, u8 bit);

void halmaincontroller_audiospdifreset(u16 baseAddr, u8 bit);

void halmaincontroller_audioi2creset(u16 baseAddr, u8 bit);

void halmaincontroller_pixelrepetitionclockreset(u16 baseAddr, u8 bit);

void halmaincontroller_tmdsclockreset(u16 baseAddr, u8 bit);

void halmaincontroller_pixelclockreset(u16 baseAddr, u8 bit);

void halmaincontroller_videofeedthroughoff(u16 baseAddr, u8 bit);

void halmaincontroller_phyreset(u16 baseAddr, u8 bit);

void halmaincontroller_phyreset(u16 baseAddr, u8 bit);

void halmaincontroller_heacphyreset(u16 baseAddr, u8 bit);

u8 halmaincontroller_lockonclockstatus(u16 baseAddr, u8 clockDomain);

void halmaincontroller_lockonclockclear(u16 baseAddr, u8 clockDomain);

#endif /* HALMAINCONTROLLER_H_ */
