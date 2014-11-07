/*
 * @file:halFrameComposerPackets.c
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halFrameComposerPackets.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 fc_ctrlqhigh = 0x73;
static const u8 fc_ctrlqlow = 0x74;
static const u8 fc_dataut01 = 0xB4;
static const u8 fc_dataut02 = 0xB5;
static const u8 fc_dataut03 = 0xB7;
static const u8 fc_datman = 0xB6;
static const u8 fc_dataut00 = 0xB3;
static const u8 fc_rdrb0 = 0xB8;
/* bit shifts */
static const u8 frame_interpolation = 0;
static const u8 frame_per_packets = 4;
static const u8 line_spacing = 0;

void halframecomposerpackets_queuepriorityhigh(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + fc_ctrlqhigh), 0, 5);
}

void halframecomposerpackets_queueprioritylow(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + fc_ctrlqlow), 0, 5);
}

void halframecomposerpackets_metadataframeinterpolation(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + fc_dataut01), frame_interpolation, 4);
}

void halframecomposerpackets_metadataframesperpacket(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + fc_dataut02), frame_per_packets, 4);
}

void halframecomposerpackets_metadatalinespacing(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + fc_dataut02), line_spacing, 4);
}

void halframecomposerpackets_setrdrb(u16 baseAddr, u8 offset, u8 value)
{
	LOG_TRACE1(value);
#if 1 /* hanhojung request */
	access_corewrite(value, (baseAddr + fc_rdrb0+offset), frame_per_packets, 8);
#else
	access_corewrite(value, (baseAddr + fc_rdrb0+offset), frame_per_packets, 4);
#endif
}
void halframecomposerpackets_autosend(u16 baseAddr, u8 enable, u8 mask)
{
	LOG_TRACE2(enable, mask);
	access_corewrite((enable ? 1 : 0), (baseAddr + fc_dataut00), mask, 1);
}

void halframecomposerpackets_auto3send(u16 baseAddr, u8 enable, u8 mask)
{
	LOG_TRACE2(enable, mask);
	access_corewrite((enable ? 1 : 0), (baseAddr + fc_dataut03), mask, 1);
}
void halframecomposerpackets_manualsend(u16 baseAddr, u8 mask)
{
	LOG_TRACE1(mask);
	access_corewrite(1, (baseAddr + fc_datman), mask, 1);
}

void halframecomposerpackets_disableall(u16 baseAddr)
{
	LOG_TRACE();
	access_corewritebyte((~(BIT(ACP_TX) | BIT(ISRC1_TX) | BIT(ISRC2_TX)
			| BIT(SPD_TX) | BIT(VSD_TX))) & access_corereadbyte
			(baseAddr + fc_dataut00), (baseAddr + fc_dataut00));
}
