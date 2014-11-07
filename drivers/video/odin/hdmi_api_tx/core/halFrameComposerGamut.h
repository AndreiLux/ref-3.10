/*
 * @file:halFrameComposerGamut.h
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALFRAMECOMPOSERGAMUT_H_
#define HALFRAMECOMPOSERGAMUT_H_

#include "../util/types.h"

void halframecomposergamut_profile(u16 baseAddr, u8 profile);

void halframecomposergamut_affectedseqno(u16 baseAddr, u8 no);

void halframecomposergamut_packetsperframe(u16 baseAddr, u8 packets);

void halframecomposergamut_packetlinespacing(u16 baseAddr, u8 lineSpacing);

void halframecomposergamut_content(u16 baseAddr, const u8 * content, u8 length);

void halframecomposergamut_enabletx(u16 baseAddr, u8 enable);

/* Only when this function is called is the packet updated with
 * the new information
 * @param baseAddr of the frame composer-gamut block
 * @return nothing
 */
void halframecomposergamut_updatepacket(u16 baseAddr);

u8 halframecomposergamut_currentseqno(u16 baseAddr);

u8 halframecomposergamut_packetseq(u16 baseAddr);

u8 halframecomposergamut_nocurrentgbd(u16 baseAddr);

#endif /* HALFRAMECOMPOSERGAMUT_H_ */
