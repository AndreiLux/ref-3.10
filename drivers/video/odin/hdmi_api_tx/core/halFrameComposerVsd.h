/*
 * @file:halFrameComposerVsd.h
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALFRAMECOMPOSERVSD_H_
#define HALFRAMECOMPOSERVSD_H_

#include "../util/types.h"
/*
 * Configure the 24 bit IEEE Registration Identifier
 * @param baseAddr Block base address
 * @param id vendor unique identifier
 */
void halframecomposervsd_vendoroui(u16 baseAddr, u32 id);
/*
 * Configure the Vendor Payload to be carried by the InfoFrame
 * @param info array
 * @param length of the array
 * @return 0 when successful and 1 on error
 */
u8 halframecomposervsd_vendorpayload(u16 baseAddr, const u8 * data,
		unsigned short length);

#endif /* HALFRAMECOMPOSERVSD_H_ */
