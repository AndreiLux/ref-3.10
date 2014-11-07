/*
 * @file:halFrameComposerAcp.c
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "halFrameComposerAcp.h"
#include "../bsp/access.h"
#include "../util/log.h"
/* Frame composer ACP register offsets*/
static const u8 fc_acp0 = 0x75;
static const u8 fc_acp1 = 0x91;
static const u8 fc_acp16 = 0x82;

void halframecomposeracp_type(u16 baseAddr, u8 type)
{
	LOG_TRACE1(type);
	access_corewritebyte(type, baseAddr + fc_acp0);
}

void halframecomposeracp_typedependentfields(u16 baseAddr, u8 * fields,
		u8 fieldsLength)
{
	u8 c = 0;
	LOG_TRACE1(fields[0]);
	if (fieldsLength > (fc_acp1 - fc_acp16 + 1))
	{
		fieldsLength = (fc_acp1 - fc_acp16 + 1);
		LOG_WARNING("ACP Fields Truncated");
	}

	for (c = 0; c < fieldsLength; c++)
		access_corewritebyte(fields[c], baseAddr + fc_acp1 - c);
}
