/*
 * @file:halFrameComposerGcp.c
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halFrameComposerGcp.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offset */
static const u8 fc_gcp = 0x18;
/* bit shifts */
static const u8 default_phase = 2;
static const u8 clear_avmute = 0;
static const u8 set_avmute = 1;

void halframecomposergcp_defaultphase(u16 baseAddr, u8 uniform)
{
	LOG_TRACE1(uniform);
	access_corewrite((uniform ? 1 : 0), (baseAddr + fc_gcp), default_phase, 1);
}

void halframecomposergcp_avmute(u16 baseAddr, u8 enable)
{
	LOG_TRACE1(enable);
	access_corewrite((enable ? 1 : 0), (baseAddr + fc_gcp), set_avmute, 1);
	access_corewrite((enable ? 0 : 1), (baseAddr + fc_gcp), clear_avmute, 1);
}
