/*
 * halAudioSpdif.h
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALAUDIOSPDIF_H_
#define HALAUDIOSPDIF_H_

#include "../util/types.h"

void halaudiospdif_resetfifo(u16 baseAddr);

void halaudiospdif_nonlinearpcm(u16 baseAddr, u8 bit);

void halaudiospdif_datawidth(u16 baseAddr, u8 value);

void halaudiospdif_interruptmask(u16 baseAddr, u8 value);

void halaudiospdif_interruptpolarity(u16 baseAddr, u8 value);

#endif /* HALAUDIOSPDIF_H_ */
