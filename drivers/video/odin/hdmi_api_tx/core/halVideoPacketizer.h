/*
 * @file:halVideoPacketizer.h
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALVIDEOPACKETIZER_H_
#define HALVIDEOPACKETIZER_H_

#include "../util/types.h"

u8 halvideopacketizer_pixelpackingphase(u16 baseAddr);

void halvideopacketizer_colordepth(u16 baseAddr, u8 value);

void halvideopacketizer_pixelpackingdefaultphase(u16 baseAddr, u8 bit);

void halvideopacketizer_pixelrepetitionfactor(u16 baseAddr, u8 value);

void halvideopacketizer_ycc422remapsize(u16 baseAddr, u8 value);

void halvideopacketizer_outputselector(u16 baseAddr, u8 value);

#endif /* HALVIDEOPACKETIZER_H_ */
