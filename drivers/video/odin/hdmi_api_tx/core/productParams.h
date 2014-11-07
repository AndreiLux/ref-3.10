/*
 * productParams.h
 *
 *  Created on: Jul 6, 2010
 *
 * Synopsys Inc.
 * SG DWC PT02
 */

#ifndef PRODUCTPARAMS_H_
#define PRODUCTPARAMS_H_

#include "../util/types.h"

/** For detailed handling of this structure,
refer to documentation of the functions */
typedef struct
{
	/* Vendor Name of eight 7-bit ASCII characters */
	u8 mvendorname[8];

	u8 mvendornamelength;

	/* Product name or description, consists of sixteen 7-bit ASCII characters */
	u8 mproductname[16];

	u8 mproductnamelength;

	/* Code that classifies the source device (CEA Table 15) */
	u8 msourcetype;

	/* oui 24 bit IEEE Registration Identifier */
	u32 moui;

	u8 mvendorpayload[24];

	u8 mvendorpayloadlength;

} productParams_t;

void productparams_reset(productParams_t *params);

u8 productparams_setproductname(productParams_t *params, const u8 * data,
		u8 length);

u8 * productparams_getproductname(productParams_t *params);

u8 productparams_getproductnamelength(productParams_t *params);

u8 productparams_setvendorname(productParams_t *params, const u8 * data,
		u8 length);

u8 * productparams_getvendorname(productParams_t *params);

u8 productparams_getvendornamelength(productParams_t *params);

u8 productparams_setsourcetype(productParams_t *params, u8 value);

u8 productparams_getsourcetype(productParams_t *params);

u8 productparams_setoui(productParams_t *params, u32 value);

u32 productparams_getoui(productParams_t *params);

u8 productparams_setvendorpayload(productParams_t *params, const u8 * data,
		u8 length);

u8 * productparams_getvendorpayload(productParams_t *params);

u8 productparams_getvendorpayloadlength(productParams_t *params);

u8 productparams_issourceproductvalid(productParams_t *params);

u8 productparams_isvendorspecificvalid(productParams_t *params);

#endif /* PRODUCTPARAMS_H_ */
