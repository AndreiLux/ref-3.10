/*
 * videoCapabilityDataBlock.h
 *
 *  Created on: Jul 23, 2010
 *
 * Synopsys Inc.
 * SG DWC PT02
 */

#ifndef VIDEOCAPABILITYDATABLOCK_H_
#define VIDEOCAPABILITYDATABLOCK_H_


#include "../util/types.h"

/**
 * @file
 * Video Capability Data Block.
 * (videoCapabilityDataBlock_t * vcdbCEA Data Block Tag Code 0).
 * Parse and hold information from EDID data structure.
 * For detailed handling of this structure,
 * refer to documentation of the functions
 */

typedef struct
{
	int mquantizationrangeselectable;

	u8 mpreferredtimingscaninfo;

	u8 mitscaninfo;

	u8 mcescaninfo;

	int mvalid;
} videoCapabilityDataBlock_t;

void videocapabilitydatablock_reset(videoCapabilityDataBlock_t * vcdb);

int videocapabilitydatablock_parse(videoCapabilityDataBlock_t * vcdb,
				u8 * data);
/**
 * @return CE Overscan/Underscan behaviour
 * 0 0  CE video formats not supported
 * 0 1  Always Overscanned
 * 1 0  Always Underscanned
 * 1 1  Supports both over- and underscan
 */
u8 videocapabilitydatablock_getcescaninfo(
		videoCapabilityDataBlock_t * vcdb);
/**
 * @return IT Overscan/Underscan behaviour
 * 0 0  IT video formats not supported
 * 0 1  Always Overscanned
 * 1 0  Always Underscanned
 * 1 1  Supports both over- and underscan
 */
u8 videocapabilitydatablock_getitscaninfo(
		videoCapabilityDataBlock_t * vcdb);
/**
 * @return Preferred Time Overscan/Underscan behaviour
 * 0 0 No Data (videoCapabilityDataBlock_t * vcdbrefer to CE or IT fields)
 * 0 1  Always Overscanned
 * 1 0  Always Underscanned
 * 1 1  Supports both over- and underscan
 */
u8 videocapabilitydatablock_getpreferredtimingscaninfo(
		videoCapabilityDataBlock_t * vcdb);

/**
 * @return TRUE if Quantization Range is Selectable
 */
int videocapabilitydatablock_getquantizationrangeselectable
	(videoCapabilityDataBlock_t * vcdb);

#endif /* VIDEOCAPABILITYDATABLOCK_H_ */
