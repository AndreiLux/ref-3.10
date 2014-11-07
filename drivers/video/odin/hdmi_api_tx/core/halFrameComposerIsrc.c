/*
 * @file:halFrameComposerIsrc.c
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halFrameComposerIsrc.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offset */
static const u8 fc_isrc1_0 = 0x92;
static const u8 fc_isrc1_1 = 0xA2;
static const u8 fc_isrc1_16 = 0x93;
static const u8 fc_isrc2_0 = 0xB2;
static const u8 fc_isrc2_15 = 0xA3;
/* bit shifts */
static const u8 isrc_cont = 0;
static const u8 isrc_valid = 1;
static const u8 isrc_status = 2;

void halframecomposerisrc_status(u16 baseAddr, u8 code)
{
	LOG_TRACE1(code);
	access_corewrite(code, (baseAddr + fc_isrc1_0), isrc_status, 3);
}

void halframecomposerisrc_valid(u16 baseAddr, u8 validity)
{
	LOG_TRACE1(validity);
	access_corewrite((validity ? 1 : 0), (baseAddr + fc_isrc1_0), isrc_valid, 1);
}

void halframecomposerisrc_cont(u16 baseAddr, u8 isContinued)
{
	LOG_TRACE1(isContinued);
	access_corewrite((isContinued ? 1 : 0), (baseAddr + fc_isrc1_0), isrc_cont,
			1);
}

void halframecomposerisrc_isrc1codes(u16 baseAddr, u8 * codes, u8 length)
{
	u8 c = 0;
	LOG_TRACE1(codes[0]);
	if (length > (fc_isrc1_1 - fc_isrc1_16 + 1))
	{
		length = (fc_isrc1_1 - fc_isrc1_16 + 1);
		LOG_WARNING("ISRC1 Codes Truncated");
	}

	for (c = 0; c < length; c++)
		access_corewritebyte(codes[c], (baseAddr + fc_isrc1_1 - c));
}

void halframecomposerisrc_isrc2codes(u16 baseAddr, u8 * codes, u8 length)
{
	u8 c = 0;
	LOG_TRACE1(codes[0]);
	if (length > (fc_isrc2_0 - fc_isrc2_15 + 1))
	{
		length = (fc_isrc2_0 - fc_isrc2_15 + 1);
		LOG_WARNING("ISRC2 Codes Truncated");
	}

	for (c = 0; c < length; c++)
		access_corewritebyte(codes[c], (baseAddr + fc_isrc2_0 - c));
}
