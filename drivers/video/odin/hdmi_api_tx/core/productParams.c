/*
 * @file:productParams.c
 *
 *  Created on: Jul 6, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "productParams.h"
#include "../util/log.h"

void productparams_reset(productParams_t *params)
{
	unsigned i = 0;
	params->mvendornamelength = 0;
	params->mproductnamelength = 0;
	params->msourcetype = (u8) (-1);
	params->moui = (u8) (-1);
	params->mvendorpayloadlength = 0;
	for (i = 0; i < sizeof(params->mvendorname); i++)
	{
		params->mvendorname[i] = 0;
	}
	for (i = 0; i < sizeof(params->mproductname); i++)
	{
		params->mvendorname[i] = 0;
	}
	for (i = 0; i < sizeof(params->mvendorpayload); i++)
	{
		params->mvendorname[i] = 0;
	}
}

u8 productparams_setproductname(productParams_t *params, const u8 * data,
		u8 length)
{
	u16 i = 0;
	if ((data == 0) || length > sizeof(params->mproductname))
	{
		LOG_ERROR("invalid parameter");
		return 1;
	}
	params->mproductnamelength = 0;
	for (i = 0; i < sizeof(params->mproductname); i++)
	{
		params->mproductname[i] = (i < length) ? data[i] : 0;
	}
	params->mproductnamelength = length;
	return 0;
}

u8 * productparams_getproductname(productParams_t *params)
{
	return params->mproductname;
}

u8 productparams_getproductnamelength(productParams_t *params)
{
	return params->mproductnamelength;
}

u8 productparams_setvendorname(productParams_t *params, const u8 * data,
		u8 length)
{
	u16 i = 0;
	if (data == 0 || length > sizeof(params->mvendorname))
	{
		LOG_ERROR("invalid parameter");
		return 1;
	}
	params->mvendornamelength = 0;
	for (i = 0; i < sizeof(params->mvendorname); i++)
	{
		params->mvendorname[i] = (i < length) ? data[i] : 0;
	}
	params->mvendornamelength = length;
	return 0;
}

u8 * productparams_getvendorname(productParams_t *params)
{
	return params->mvendorname;
}

u8 productparams_getvendornamelength(productParams_t *params)
{
	return params->mvendornamelength;
}

u8 productparams_setsourcetype(productParams_t *params, u8 value)
{
	params->msourcetype = value;
	return 0;
}

u8 productparams_getsourcetype(productParams_t *params)
{
	return params->msourcetype;
}

u8 productparams_setoui(productParams_t *params, u32 value)
{
	params->moui = value;
	return 0;
}

u32 productparams_getoui(productParams_t *params)
{
	return params->moui;
}

u8 productparams_setvendorpayload(productParams_t *params, const u8 * data,
		u8 length)
{
	u16 i = 0;
	if (data == 0 || length > sizeof(params->mvendorpayload))
	{
		LOG_ERROR("invalid parameter");
		return 1;
	}
	params->mvendorpayloadlength = 0;
	for (i = 0; i < sizeof(params->mvendorpayload); i++)
	{
		params->mvendorpayload[i] = (i < length) ? data[i] : 0;
	}
	params->mvendorpayloadlength = length;
	return 0;
}

u8 * productparams_getvendorpayload(productParams_t *params)
{
	return params->mvendorpayload;
}

u8 productparams_getvendorpayloadlength(productParams_t *params)
{
	return params->mvendorpayloadlength;
}

u8 productparams_issourceproductvalid(productParams_t *params)
{
	return productparams_getsourcetype(params) != (u8) (-1)
			&& productparams_getvendornamelength(params) != 0
			&& productparams_getproductnamelength(params) != 0;
}

u8 productparams_isvendorspecificvalid(productParams_t *params)
{
	return productparams_getoui(params) != (u32) (-1)
			&& productparams_getvendorpayloadlength(params) != 0;
}
