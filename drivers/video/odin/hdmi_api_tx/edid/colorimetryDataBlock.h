/*
 * colorimetryDataBlock.h
 *
 *  Created on: Jul 22, 2010
 *
 * Synopsys Inc.
 * SG DWC PT02
 */

#ifndef COLORIMETRYDATABLOCK_H_
#define COLORIMETRYDATABLOCK_H_

#include "../util/types.h"

/**
 * @file
 * Colorimetry Data Block class.
 * Holds and parses the Colorimetry datablock information.
 */

typedef struct
{
	u8 mbyte3;

	u8 mbyte4;

	int mvalid;

} colorimetryDataBlock_t;

void colorimetrydatablock_reset(colorimetryDataBlock_t * cdb);

int colorimetrydatablock_parse(colorimetryDataBlock_t * cdb, u8 * data);

int colorimetrydatablock_supportsxvycc709(colorimetryDataBlock_t * cdb);

int colorimetrydatablock_supportsxvycc601(colorimetryDataBlock_t * cdb);

int colorimetrydatablock_supportsmetadata0(colorimetryDataBlock_t * cdb);

int colorimetrydatablock_supportsmetadata1(colorimetryDataBlock_t * cdb);

int colorimetrydatablock_supportsmetadata2(colorimetryDataBlock_t * cdb);

#endif /* COLORIMETRYDATABLOCK_H_ */
