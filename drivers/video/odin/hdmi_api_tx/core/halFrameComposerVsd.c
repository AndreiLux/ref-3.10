/*
 * @file:halFrameComposerVsd.c
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halFrameComposerVsd.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 fc_vsdieeed0 = 0x29;
static const u8 fc_vsdieeed1 = 0x30;
static const u8 fc_vsdieeed2 = 0x31;
static const u8 fc_vsdpayload0 = 0x32;

void halframecomposervsd_vendoroui(u16 baseAddr, u32 id)
{
	LOG_TRACE1(id);
	access_corewritebyte(id, (baseAddr + fc_vsdieeed0));
	access_corewritebyte(id >> 8, (baseAddr + fc_vsdieeed1));
	access_corewritebyte(id >> 16, (baseAddr + fc_vsdieeed2));
}

u8 halframecomposervsd_vendorpayload(u16 baseAddr, const u8 * data,
		unsigned short length)
{
	const unsigned short size = 24;
	unsigned i = 0;
	LOG_TRACE();
	if (data == 0)
	{
		LOG_WARNING("invalid parameter");
		return 1;
	}
	if (length > size)
	{
		length = size;
		LOG_WARNING("vendor payload truncated");
	}
	for (i = 0; i < length; i++)
	{
		access_corewritebyte(data[i], (baseAddr + fc_vsdpayload0 + i));
	}
	return 0;
}
