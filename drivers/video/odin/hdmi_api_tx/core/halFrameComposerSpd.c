/*
 * @file:halFrameComposerSpd.c
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halFrameComposerSpd.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 fc_spdvendorname0 = 0x4A;
static const u8 fc_spdproductname0 = 0x52;
static const u8 fc_spddeviceinf = 0x62;

void halframecomposerspd_vendorname(u16 baseAddr, const u8 * data,
		unsigned short length)
{
	unsigned short i = 0;
	LOG_TRACE();
	for (i = 0; i < length; i++)
	{
		access_corewritebyte(data[i], (baseAddr + fc_spdvendorname0 + i));
	}
}
void halframecomposerspd_productname(u16 baseAddr, const u8 * data,
		unsigned short length)
{
	unsigned short i = 0;
	LOG_TRACE();
	for (i = 0; i < length; i++)
	{
		access_corewritebyte(data[i], (baseAddr + fc_spdproductname0 + i));
	}
}

void halframecomposerspd_sourcedeviceinfo(u16 baseAddr, u8 code)
{
	LOG_TRACE1(code);
	access_corewritebyte(code, (baseAddr + fc_spddeviceinf));
}

