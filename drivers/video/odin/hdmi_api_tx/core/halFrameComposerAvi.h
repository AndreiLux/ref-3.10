/*
 * @file:halFrameComposerAvi.h
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALFRAMECOMPOSERAVI_H_
#define HALFRAMECOMPOSERAVI_H_

#include "../util/types.h"

void halframecomposeravi_rgbycc(u16 baseAddr, u8 type);

void halframecomposeravi_scaninfo(u16 baseAddr, u8 left);

void halframecomposeravi_colorimety(u16 baseAddr, unsigned cscITU);

void halframecomposeravi_picaspectratio(u16 baseAddr, u8 ar);

void halframecomposeravi_activeaspectratiovalid(u16 baseAddr, u8 valid);

void halframecomposeravi_activeformataspectratio(u16 baseAddr, u8 left);

void halframecomposeravi_isltcontent(u16 baseAddr, u8 it);

void halframecomposeravi_extendedcolorimetry(u16 baseAddr, u8 extColor);

void halframecomposeravi_quantizationrange(u16 baseAddr, u8 range);

void halframecomposeravi_quantizationrangecea(u32 baseAddr, u8 range);

void halframecomposeravi_itcontentcea(u32 baseAddr, u8 it_content);
void halframecomposeravi_nonuniformpicscaling(u16 baseAddr, u8 scale);

void halframecomposeravi_videocode(u16 baseAddr, u8 code);

void halframecomposeravi_horizontalbarsvalid(u16 baseAddr, u8 validity);

void halframecomposeravi_horizontalbars(
		u16 baseAddr, u16 endTop, u16 startBottom);

void halframecomposeravi_verticalbarsvalid(u16 baseAddr, u8 validity);

void halframecomposeravi_verticalbars(
		u16 baseAddr, u16 endLeft, u16 startRight);

void halframecomposeravi_outpixelrepetition(u16 baseAddr, u8 pr);

#endif /* HALFRAMECOMPOSERAVI_H_ */
