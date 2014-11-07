/*
 * hal_audio_i2s.h
 *
 *  Created on: Jun 29, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALAUDIOI2S_H_
#define HALAUDIOI2S_H_

#include "../util/types.h"

void halaudioi2s_resetfifo(u16 baseAddress);

void halaudioi2s_select(u16 baseAddress, u8 bit);

void halaudioi2s_dataenable(u16 baseAddress, u8 value);

void halaudioi2s_datamode(u16 baseAddress, u8 value);

void halaudioi2s_datawidth(u16 baseAddress, u8 value);

void halaudioi2s_interruptmask(u16 baseAddress, u8 value);

void halaudioi2s_interruptpolarity(u16 baseAddress, u8 value);

#endif /* HALAUDIOI2S_H_ */
