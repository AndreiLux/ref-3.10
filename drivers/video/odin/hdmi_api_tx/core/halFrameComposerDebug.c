/*
 * @file:halFrameComposerDebug.c
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halFrameComposerDebug.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 fc_dbgforce = 0x00;
static const u8 fc_dbgtmds0 = 0x19;
static const u8 fc_dbgtmds1 = 0x1A;
static const u8 fc_dbgtmds2 = 0x1B;

void halframecomposerdebug_forceaudio(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, baseAddr + fc_dbgforce, 4, 1);
}

void halframecomposerdebug_forcevideo(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	/* avoid glitches */
	if (bit != 0)
	{
		access_corewritebyte(0x00, baseAddr + fc_dbgtmds2); /* R */
		access_corewritebyte(0x00, baseAddr + fc_dbgtmds1); /* G */
		access_corewritebyte(0xFF, baseAddr + fc_dbgtmds0); /* B */
		access_corewrite(bit, baseAddr + fc_dbgforce, 0, 1);
	}
	else
	{
		access_corewrite(bit, baseAddr + fc_dbgforce, 0, 1);
		access_corewritebyte(0x00, baseAddr + fc_dbgtmds2); /* R */
		access_corewritebyte(0x00, baseAddr + fc_dbgtmds1); /* G */
		access_corewritebyte(0x00, baseAddr + fc_dbgtmds0); /* B */
	}
}
