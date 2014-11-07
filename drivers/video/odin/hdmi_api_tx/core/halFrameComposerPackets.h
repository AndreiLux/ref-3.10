/*
 * @file:halFrameComposerPackets.h
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALFRAMECOMPOSERPACKETS_H_
#define HALFRAMECOMPOSERPACKETS_H_

#include "../util/types.h"

#define ACP_TX  	0
#define ISRC1_TX 	1
#define ISRC2_TX 	2
#define SPD_TX 		4
#define VSD_TX 		3

void halframecomposerpackets_queuepriorityhigh(u16 baseAddr, u8 value);

void halframecomposerpackets_queueprioritylow(u16 baseAddr, u8 value);

void halframecomposerpackets_metadataframeinterpolation(u16 baseAddr, u8 value);

void halframecomposerpackets_metadataframesperpacket(u16 baseAddr, u8 value);

void halframecomposerpackets_metadatalinespacing(u16 baseAddr, u8 value);

void halframecomposerpackets_setrdrb(u16 baseAddr, u8 offset, u8 value);
void halframecomposerpackets_autosend(u16 baseAddr, u8 enable, u8 mask);

void halframecomposerpackets_auto3send(u16 baseAddr, u8 enable, u8 mask);
void halframecomposerpackets_manualsend(u16 baseAddr, u8 mask);

void halframecomposerpackets_disableall(u16 baseAddr);

#endif /* HALFRAMECOMPOSERPACKETS_H_ */
