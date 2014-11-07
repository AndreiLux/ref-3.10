/*
 * halAudioHbr.h
 *
 *  Created on: Jun 29, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALAUDIOHBR_H_
#define HALAUDIOHBR_H_

#include "../util/types.h"

void halaudiohbr_resetfifo(u16 baseAddress);

void halaudiohbr_select(u16 baseAddress, u8 bit);

void halaudiohbr_interruptmask(u16 baseAddress, u8 value);

void halaudiohbr_interruptpolarity(u16 baseAddress, u8 value);

#endif /* HALAUDIOHBR_H_ */
