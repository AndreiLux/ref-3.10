/*
 * @file:halFrameComposerIsrc.h
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALFRAMECOMPOSERISRC_H_
#define HALFRAMECOMPOSERISRC_H_

#include "../util/types.h"
/*
 * Configure the ISRC packet status
 * @param code
 * 001 - Starting Position
 * 010 - Intermediate Position
 * 100 - Ending Position
 * @param baseAddr block base address
 */
void halframecomposerisrc_status(u16 baseAddr, u8 code);
/*
 * Configure the validity bit in the ISRC packets
 * @param validity: 1 if valid
 * @param baseAddr block base address
 */
void halframecomposerisrc_valid(u16 baseAddr, u8 validity);
/*
 * Configure the cont bit in the ISRC1 packets
 * When a subsequent ISRC2 Packet is transmitted,
 * the ISRC_Cont field shall be set and shall be clear otherwise.
 * @param isContinued 1 when set
 * @param baseAddr block base address
 */
void halframecomposerisrc_cont(u16 baseAddr, u8 isContinued);
/*
 * Configure the ISRC 1 Codes
 * @param codes
 * @param length
 * @param baseAddr block base address
 */
void halframecomposerisrc_isrc1codes(u16 baseAddr, u8 * codes, u8 length);
/*
 * Configure the ISRC 2 Codes
 * @param codes
 * @param length
 * @param baseAddr block base address
 */
void halframecomposerisrc_isrc2codes(u16 baseAddr, u8 * codes, u8 length);

#endif /* HALFRAMECOMPOSERISRC_H_ */
