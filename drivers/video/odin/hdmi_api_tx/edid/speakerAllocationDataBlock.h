/*
 * speakerAllocationDataBlock.h
 *
 *  Created on: Jul 22, 2010
  *
 * Synopsys Inc.
 * SG DWC PT02
 */

#ifndef SPEAKERALLOCATIONDATABLOCK_H_
#define SPEAKERALLOCATIONDATABLOCK_H_

#include "../util/types.h"

/**
 * @file
 * SpeakerAllocation Data Block.
 * Holds and parse the Speaker Allocation datablock information.
 * For detailed handling of this structure,
 * refer to documentation of the functions
 */

typedef struct
{
	u8 mbyte1;

	int mvalid;
} speakerAllocationDataBlock_t;

void speakerallocationdatablock_reset(speakerAllocationDataBlock_t * sadb);

int speakerallocationdatablock_parse(speakerAllocationDataBlock_t * sadb,
		u8 * data);

int speakerallocationdatablock_supportsflfr(
		speakerAllocationDataBlock_t * sadb);

int speakerallocationdatablock_supportslfe(
		speakerAllocationDataBlock_t * sadb);

int speakerallocationdatablock_supportsfc(
		speakerAllocationDataBlock_t * sadb);

int speakerallocationdatablock_supportsrlrr(
		speakerAllocationDataBlock_t * sadb);

int speakerallocationdatablock_supportsrc(
		speakerAllocationDataBlock_t * sadb);

int speakerallocationdatablock_supportsflcfrc(
		speakerAllocationDataBlock_t * sadb);

int speakerallocationdatablock_supportsrlcrrc(
		speakerAllocationDataBlock_t * sadb);
/**
 * @return the Channel Allocation code used in
 * the Audio Infoframe to ease the translation process
 */
u8 speakerallocationdatablock_getchannelallocationcode
	(speakerAllocationDataBlock_t * sadb);
/**
 * @return the whole byte of Speaker Allocation DataBlock
 * where speaker allocation is indicated. User wishing to access
 * and interpret it must know specifically how to parse it.
 */
u8 speakerallocationdatablock_getspeakerallocationbyte
	(speakerAllocationDataBlock_t * sadb);

#endif /* SPEAKERALLOCATIONDATABLOCK_H_ */
