/*
 * @file:halAudioDma.h
 *
 *  Created on: May 3, 2011
 *
 * Synopsys Inc.
 * SG DWC PT02
 */

#ifndef HALAUDIODMA_H_
#define HALAUDIODMA_H_

#include "../util/types.h"

void halaudiodma_resetfifo(u16 baseAddress);
void halaudiodma_hbrenable(u16 baseAddress, u8 enable);
void halaudiodma_hlockenable(u16 baseAddress, u8 enable);
void halaudiodma_fixburstmode(u16 baseAddress, u8 fixedBeatIncrement);
void halaudiodma_start(u16 baseAddress);
void halaudiodma_stop(u16 baseAddress);
void halaudiodma_threshold(u16 baseAddress, u8 threshold);
void halaudiodma_startaddress(u16 baseAddress, u32 startAddress);
void halaudiodma_stopaddress(u16 baseAddress, u32 stopAddress);
u32 halaudiodma_currentoperationaddress(u16 baseAddress);
u16 halaudiodma_currentburstlength(u16 baseAddress);
void halaudiodma_dmainterruptmask(u16 baseAddress, u8 mask);
void halaudiodma_bufferinterruptmask(u16 baseAddress, u8 mask);
u8 halaudiodma_dmainterruptmaskstatus(u16 baseAddress);
u8 halaudiodma_bufferinterruptmaskstatus(u16 baseAddress);
void halaudiodma_channelenable(u16 baseAddress, u8 enable, u8 channel);


#endif /* HALAUDIODMA_H_ */
