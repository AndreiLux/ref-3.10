/*
 * @file:bitOperation.h
 *
 *  Created on: Jul 5, 2010
  *
 * Synopsys Inc.
 * SG DWC PT02
 */

#ifndef BITOPERATION_G_H_
#define BITOPERATION_G_H_

#include "types.h"
/**
 * Concatenate two parts of two 8-bit bytes into a new 16-bit word
 * @param bHi first byte
 * @param oHi shift part of first byte (to be place as most significant
 * bits)
 * @param nHi width part of first byte (to be place as most significant
 * bits)
 * @param bLo second byte
 * @param oLo shift part of second byte (to be place as least
 * significant bits)
 * @param nLo width part of second byte (to be place as least
 * significant bits)
 * @returns 16-bit concatinated word as part of the first byte and part of
 * the second byte
 */
u16 bitoperation_concatbits(u8 bHi, u8 oHi, u8 nHi, u8 bLo, u8 oLo, u8 nLo);

/** Concatinate two full bytes into a new 16-bit word
 * @param hi
 * @param lo
 * @returns hi as most segnificant bytes concatenated with lo as least
 * significant bits.
 */
u16 bitoperation_bytes2word(const u8 hi, const u8 lo);

/** Extract the content of a certain part of a byte
 * @param data 8bit byte
 * @param shift shift from the start of the bit (0)
 * @param width width of the desired part starting from shift
 * @returns an 8bit byte holding only the desired part of the passed on
 * data byte
 */
u8 bitoperation_bitfield(const u16 data, u8 shift, u8 width);

/** Concatenate four 8-bit bytes into a new 32-bit word
 * @param b3 assumed as most significant 8-bit byte
 * @param b2
 * @param b1
 * @param b0 assumed as least significant 8bit byte
 * @returns a 2D word, 32bits, composed of the 4 passed on parameters
 */
u32 bitoperation_bytes2dword(u8 b3, u8 b2, u8 b1, u8 b0);

#endif /* BITOPERATION_G_H_ */
