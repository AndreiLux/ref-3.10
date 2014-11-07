/*
 * @file:halIdentification.h
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HALIDENTIFICATION_H_
#define HALIDENTIFICATION_H_

#include "../util/types.h"

u8 halidentification_design(u16 baseAddr);

u8 halidentification_revision(u16 baseAddr);

u8 halidentification_productline(u16 baseAddr);

u8 halidentification_producttype(u16 baseAddr);

#endif /* HALIDENTIFICATION_H_ */
