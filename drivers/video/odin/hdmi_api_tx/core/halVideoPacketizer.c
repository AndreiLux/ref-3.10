/*
 * @file:halVideoPacketizer.c
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halVideoPacketizer.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 vp_status = 0x00;
static const u8 vp_pr_cd = 0x01;
static const u8 vp_stuff = 0x02;
static const u8 vp_remap = 0x03;
static const u8 vp_conf = 0x04;

u8 halvideopacketizer_pixelpackingphase(u16 baseAddr)
{
	LOG_TRACE();
	return access_coreread((baseAddr + vp_status), 0, 4);
}

void halvideopacketizer_colordepth(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	/* color depth */
	access_corewrite(value, (baseAddr + vp_pr_cd), 4, 4);
}

void halvideopacketizer_pixelpackingdefaultphase(u16 baseAddr, u8 bit)
{
	LOG_TRACE1(bit);
	access_corewrite(bit, (baseAddr + vp_stuff), 5, 1);
}

void halvideopacketizer_pixelrepetitionfactor(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	/* desired factor */
	access_corewrite(value, (baseAddr + vp_pr_cd), 0, 4);
	/* enable stuffing */
	access_corewrite(0, (baseAddr + vp_stuff), 0, 1);
	/* enable block */
	access_corewrite((value > 1) ? 1 : 0, (baseAddr + vp_conf), 4, 1);
	/* bypass block */
	access_corewrite((value > 1) ? 0 : 1, (baseAddr + vp_conf), 2, 1);
}

void halvideopacketizer_ycc422remapsize(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	access_corewrite(value, (baseAddr + vp_remap), 0, 2);
}

void halvideopacketizer_outputselector(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	if (value == 0)
	{ /* pixel packing */
		access_corewrite(0, (baseAddr + vp_conf), 6, 1);
		/* enable pixel packing */
		access_corewrite(1, (baseAddr + vp_conf), 5, 1);
		access_corewrite(0, (baseAddr + vp_conf), 3, 1);
	}
	else if (value == 1)
	{ /* YCC422 */
		access_corewrite(0, (baseAddr + vp_conf), 6, 1);
		access_corewrite(0, (baseAddr + vp_conf), 5, 1);
		/* enable YCC422 */
		access_corewrite(1, (baseAddr + vp_conf), 3, 1);
	}
	else if (value == 2 || value == 3)
	{ /* bypass */
		/* enable bypass */
		access_corewrite(1, (baseAddr + vp_conf), 6, 1);
		access_corewrite(0, (baseAddr + vp_conf), 5, 1);
		access_corewrite(0, (baseAddr + vp_conf), 3, 1);
	}
	else
	{
		LOG_ERROR2("wrong output option: ", value);
		return;
	}

	/* YCC422 stuffing */
	access_corewrite(0, (baseAddr + vp_stuff), 2, 1);
	/* pixel packing stuffing */
	access_corewrite(0, (baseAddr + vp_stuff), 1, 1);

	/* ouput selector */
	access_corewrite(value, (baseAddr + vp_conf), 0, 2);
}
