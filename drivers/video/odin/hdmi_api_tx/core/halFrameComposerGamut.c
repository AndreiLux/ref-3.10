/*
 * @file:halFrameComposerGamut.c
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halFrameComposerGamut.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 fc_gmd_stat = 0x00;
static const u8 fc_gmd_en = 0x01;
static const u8 fc_gmd_up = 0x02;
static const u8 fc_gmd_conf = 0x03;
static const u8 fc_gmd_hb = 0x04;
static const u8 fc_gmd_pb0 = 0x05;
static const u8 fc_gmd_pb27 = 0x20;
/* bit shifts */
static const u8 gmd_enable_tx = 0;
static const u8 gmd_enable_tx_width = 1;
static const u8 gmd_update = 0;
static const u8 gmd_update_width = 1;
static const u8 packet_line_spacing = 0;
static const u8 packet_line_spacing_width = 4;
static const u8 packet_per_frame = 4;
static const u8 packet_per_frame_width = 4;
static const u8 affected_seq_no = 0;
static const u8 affected_seq_no_width = 4;
static const u8 gmd_profile = 4;
static const u8 gmd_profile_width = 3;
static const u8 gmd_curr_seq = 0;
static const u8 gmd_pack_seq = 4;
static const u8 gmd_no_curr_gbd = 7;

void halframecomposergamut_profile(u16 baseAddr, u8 profile)
{
	LOG_TRACE1(profile);
	access_corewrite(profile, baseAddr + fc_gmd_hb, gmd_profile,
			gmd_profile_width);
}

void halframecomposergamut_affectedseqno(u16 baseAddr, u8 no)
{
	LOG_TRACE1(no);
	access_corewrite(no, baseAddr + fc_gmd_hb, affected_seq_no,
			affected_seq_no_width);
}

void halframecomposergamut_packetsperframe(u16 baseAddr, u8 packets)
{
	LOG_TRACE1(packets);
	access_corewrite(packets, baseAddr + fc_gmd_conf, packet_per_frame,
			packet_per_frame_width);
}

void halframecomposergamut_packetlinespacing(u16 baseAddr, u8 lineSpacing)
{
	LOG_TRACE1(lineSpacing);
	access_corewrite(lineSpacing, baseAddr + fc_gmd_conf, packet_line_spacing,
			packet_line_spacing_width);
}

void halframecomposergamut_content(u16 baseAddr, const u8 * content, u8 length)
{
	u8 c = 0;
	LOG_TRACE1(content[0]);
	if (length > (fc_gmd_pb27 - fc_gmd_pb0 + 1))
	{
		length = (fc_gmd_pb27 - fc_gmd_pb0 + 1);
		LOG_WARNING("Gamut Content Truncated");
	}

	for (c = 0; c < length; c++)
		access_corewritebyte(content[c], baseAddr + c + fc_gmd_pb0);
}

void halframecomposergamut_enabletx(u16 baseAddr, u8 enable)
{
	LOG_TRACE1(enable);
	access_corewrite(enable, baseAddr + fc_gmd_en, gmd_enable_tx, 1);
}

void halframecomposergamut_updatepacket(u16 baseAddr)
{
	LOG_TRACE();
	access_corewrite(1, baseAddr + fc_gmd_up, gmd_update, 1);
}

u8 halframecomposergamut_currentseqno(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread(baseAddr + fc_gmd_stat, gmd_curr_seq, 4);
}

u8 halframecomposergamut_packetseq(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread(baseAddr + fc_gmd_stat, gmd_pack_seq, 2);
}

u8 halframecomposergamut_nocurrentgbd(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread(baseAddr + fc_gmd_stat, gmd_no_curr_gbd, 1);
}
