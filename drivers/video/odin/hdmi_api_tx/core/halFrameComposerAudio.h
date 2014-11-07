/*
 * @file:halFrameComposerAudio.h
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALFRAMECOMPOSERAUDIO_H_
#define HALFRAMECOMPOSERAUDIO_H_

#include "../util/types.h"

void halframecomposeraudio_packetsampleflat(u16 baseAddr, u8 value);

void halframecomposeraudio_packetlayout(u16 baseAddr, u8 bit);

void halframecomposeraudio_validityright(u16 baseAddr, u8 bit,
				unsigned channel);

void halframecomposeraudio_validityleft(u16 baseAddr, u8 bit, unsigned channel);

void halframecomposeraudio_userright(u16 baseAddr, u8 bit, unsigned channel);

void halframecomposeraudio_userleft(u16 baseAddr, u8 bit, unsigned channel);

void halframecomposeraudio_ieccgmsa(u16 baseAddr, u8 value);

void halframecomposeraudio_ieccopyright(u16 baseAddr, u8 bit);

void halframecomposeraudio_ieccategorycode(u16 baseAddr, u8 value);

void halframecomposeraudio_iecpcmmode(u16 baseAddr, u8 value);

void halframecomposeraudio_iecsource(u16 baseAddr, u8 value);

void halframecomposeraudio_iecchannelright(u16 baseAddr, u8 value,
		unsigned channel);

void halframecomposeraudio_iecchannelleft(u16 baseAddr, u8 value,
		unsigned channel);

void halframecomposeraudio_iecclockaccuracy(u16 baseAddr, u8 value);

void halframecomposeraudio_iecsamplingfreq(u16 baseAddr, u8 value);

void halframecomposeraudio_iecoriginalsamplingfreq(u16 baseAddr, u8 value);

void halframecomposeraudio_iecwordlength(u16 baseAddr, u8 value);

#endif /* HALFRAMECOMPOSERAUDIO_H_ */
