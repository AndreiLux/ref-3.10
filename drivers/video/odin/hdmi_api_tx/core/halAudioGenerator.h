/*
 * hal_audio_generator.h
 *
 *  Created on: Jun 29, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HAL_AUDIO_GENERATOR_H_
#define HAL_AUDIO_GENERATOR_H_

#include "../util/types.h"

void halaudiogenerator_swreset(u16 baseAddress, u8 bit);

void halaudiogenerator_i2smode(u16 baseAddress, u8 value);

void halaudiogenerator_freqincrementleft(u16 baseAddress, u16 value);

void halaudiogenerator_freqincrementright(u16 baseAddress, u16 value);

void halaudiogenerator_ieccgmsa(u16 baseAddress, u8 value);

void halaudiogenerator_ieccopyright(u16 baseAddress, u8 bit);

void halaudiogenerator_ieccategorycode(u16 baseAddress, u8 value);

void halaudiogenerator_iecpcmmodee(u16 baseAddress, u8 value);

void halaudiogenerator_iecsource(u16 baseAddress, u8 value);

void halaudiogenerator_iecchannelright(u16 baseAddress, u8 value, u8 channelNo);

void halaudiogenerator_iecchannelleft(u16 baseAddress, u8 value, u8 channelNo);

void halaudiogenerator_iecclockaccuracy(u16 baseAddress, u8 value);

void halaudiogenerator_iecsamplingfreq(u16 baseAddress, u8 value);

void halaudiogenerator_iecoriginalsamplingfreq(u16 baseAddress, u8 value);

void halaudiogenerator_iecwordlength(u16 baseAddress, u8 value);

void halaudiogenerator_userright(u16 baseAddress, u8 bit, u8 channelNo);

void halaudiogenerator_userleft(u16 baseAddress, u8 bit, u8 channelNo);

void halaudiogenerator_spdiftxdata(u16 baseAddress, u8 bit);

void halaudiogenerator_hbrenable(u16 baseAddress, u8 bit);

void halaudiogenerator_hbrddrenable(u16 baseAddress, u8 bit);

/*
 * @param bit: right(1) or left(0)
 */
void halaudiogenerator_hbrddrchannel(u16 baseAddress, u8 bit);

void halaudiogenerator_hbrburstlength(u16 baseAddress, u8 value);

void halaudiogenerator_hbrclockdivider(u16 baseAddress, u16 value);
/*
 * (HW simulation only)
 */
void halaudiogenerator_gpareplylatency(u16 baseAddress, u8 value);
/*
 * (HW simulation only)
 */
void halaudiogenerator_uselookuptable(u16 baseAddress, u8 value);

void halaudiogenerator_gpasamplevalid(u16 baseAddress, u8 value, u8 channel);

void halaudiogenerator_channelselect(u16 baseAddress, u8 enable, u8 channel);

#endif /* HAL_AUDIO_GENERATOR_H_ */
