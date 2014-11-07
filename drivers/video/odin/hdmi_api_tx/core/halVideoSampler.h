/*
 * @file:halVideoSampler.h
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALVIDEOSAMPLER_H_
#define HALVIDEOSAMPLER_H_

#include "../util/types.h"

void halvideosampler_internaldataenablegenerator(u16 baseAddr, u8 bit);

void halvideosampler_videomapping(u16 baseAddr, u8 value);

void halvideosampler_stuffinggy(u16 baseAddr, u16 value);

void halvideosampler_stuffingrcr(u16 baseAddr, u16 value);

void halvideosampler_stuffingbcb(u16 baseAddr, u16 value);

#endif /* HALVIDEOSAMPLER_H_ */
