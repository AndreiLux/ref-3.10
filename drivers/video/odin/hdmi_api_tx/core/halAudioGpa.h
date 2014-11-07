/*
 * halAudioGpa.h
 *
 * Created on Oct. 30th 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALAUDIOGPA_H_
#define HALAUDIOGPA_H_

#include "../util/types.h"

void halaudiogpa_resetfifo(u16 baseAddress);
void halaudiogpa_channelenable(u16 baseAddress, u8 enable, u8 channel);
void halaudiogpa_hbrenable(u16 baseAddress, u8 enable);
void halaudiogpa_insertpucv(u16 baseAddress, u8 enable);

u8 halaudiogpa_interruptstatus(u16 baseAddress);
void halaudiogpa_interruptmask(u16 baseAddress, u8 value);
void halaudiogpa_interruptpolority(u16 baseAddress, u8 value);

#endif /* HALAUDIOGPA_H_ */
