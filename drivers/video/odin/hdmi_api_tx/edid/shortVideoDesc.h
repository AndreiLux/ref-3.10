/*
 * shortVideoDesc.h
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef SHORTVIDEODESC_H_
#define SHORTVIDEODESC_H_

#include "../util/types.h"

/**
 * @file
 * Short Video Descriptor.
 * Parse and hold Short Video Descriptors found in Video Data Block in EDID.
 */
/** For detailed handling of this structure,
   refer to documentation of the functions */
typedef struct
{
	int mnative;

	unsigned mcode;

} shortVideoDesc_t;

void shortvideodesc_reset(shortVideoDesc_t *svd);

int shortvideodesc_parse(shortVideoDesc_t *svd, u8 data);
/**
 * @return the video code (VIC) defined in CEA-861-(D or later versions)
 */
unsigned shortvideodesc_getcode(shortVideoDesc_t *svd);

/**
 * @return TRUE if video mode is native to sink
 */
int shortvideodesc_getnative(shortVideoDesc_t *svd);

#endif /* SHORTVIDEODESC_H_ */
