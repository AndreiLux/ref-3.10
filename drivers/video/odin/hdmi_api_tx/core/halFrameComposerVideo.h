/*
 * @file:halFrameComposerVideo.h
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALFRAMECOMPOSERVIDEO_H_
#define HALFRAMECOMPOSERVIDEO_H_

#include "../util/types.h"

void halframecomposervideo_hdcpkeepout(u16 baseAddr, u8 bit);

void halframecomposervideo_vsyncpolarity(u16 baseAddr, u8 bit);

void halframecomposervideo_hsyncpolarity(u16 baseAddr, u8 bit);

void halframecomposervideo_dataenablepolarity(u16 baseAddr, u8 bit);

void halframecomposervideo_dviorhdmi(u16 baseAddr, u8 bit);

void halframecomposervideo_vblankosc(u16 baseAddr, u8 bit);

void halframecomposervideo_interlaced(u16 baseAddr, u8 bit);

void halframecomposervideo_hactive(u16 baseAddr, u16 value);

void halframecomposervideo_hblank(u16 baseAddr, u16 value);

void halframecomposervideo_vactive(u16 baseAddr, u16 value);

void halframecomposervideo_vblank(u16 baseAddr, u16 value);

void halframecomposervideo_hsyncedgedelay(u16 baseAddr, u16 value);

void halframecomposervideo_hsyncpulsewidth(u16 baseAddr, u16 value);

void halframecomposervideo_vsyncedgedelay(u16 baseAddr, u16 value);

void halframecomposervideo_vsyncpulsewidth(u16 baseAddr, u16 value);

void halframecomposervideo_refreshrate(u16 baseAddr, u32 value);

void halframecomposervideo_controlperiodminduration(u16 baseAddr, u8 value);

void halframecomposervideo_extendedcontrolperiodminduration(u16 baseAddr,
		u8 value);

void halframecomposervideo_extendedcontrolperiodmaxspacing(u16 baseAddr,
		u8 value);

void halframecomposervideo_preamblefilter(u16 baseAddr, u8 value,
		unsigned channel);

void halframecomposervideo_pixelrepetitioninput(u16 baseAddr, u8 value);

#endif /* HALFRAMECOMPOSERVIDEO_H_ */
