/*
 * colorimetryDataBlock.c
 *
 *  Created on: Jul 22, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "colorimetryDataBlock.h"
#include "../util/bitOperation.h"
#include "../util/log.h"

void colorimetrydatablock_reset(colorimetryDataBlock_t * cdb)
{
	cdb->mbyte3 = 0;
	cdb->mbyte4 = 0;
	cdb->mvalid = FALSE;
}

int colorimetrydatablock_parse(colorimetryDataBlock_t * cdb, u8 * data)
{
	LOG_TRACE();
	colorimetrydatablock_reset(cdb);
	if (data != 0 && bitoperation_bitfield(data[0], 0, 5) == 0x03
			&& bitoperation_bitfield(data[0], 5, 3) == 0x07
			&& bitoperation_bitfield(data[1], 0, 7) == 0x05)
	{
		cdb->mbyte3 = data[2];
		cdb->mbyte4 = data[3];
		cdb->mvalid = TRUE;
		return TRUE;
	}
	return FALSE;
}

int colorimetrydatablock_supportsxvycc709(colorimetryDataBlock_t * cdb)
{
	return (bitoperation_bitfield(cdb->mbyte3, 1, 1) == 1) ? TRUE : FALSE;
}

int colorimetrydatablock_supportsxvycc601(colorimetryDataBlock_t * cdb)
{
	return (bitoperation_bitfield(cdb->mbyte3, 0, 1) == 1) ? TRUE : FALSE;
}

int colorimetrydatablock_supportsmetadata0(colorimetryDataBlock_t * cdb)
{
	return (bitoperation_bitfield(cdb->mbyte4, 0, 1) == 1) ? TRUE : FALSE;
}

int colorimetrydatablock_supportsmetadata1(colorimetryDataBlock_t * cdb)
{
	return (bitoperation_bitfield(cdb->mbyte4, 1, 1) == 1) ? TRUE : FALSE;
}

int colorimetrydatablock_supportsmetadata2(colorimetryDataBlock_t * cdb)
{
	return (bitoperation_bitfield(cdb->mbyte4, 2, 1) == 1) ? TRUE : FALSE;
}
