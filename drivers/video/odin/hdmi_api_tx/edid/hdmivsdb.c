/*
 * hdmivsdb.c
 *
 *  Created on: Jul 21, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */
 #include <linux/kernel.h>

#include "hdmivsdb.h"
#include "../util/bitOperation.h"
#include "../util/log.h"

void hdmivsdb_reset(hdmivsdb_t *vsdb)
{
	vsdb->mphysicaladdress = 0;
	vsdb->msupportsai = FALSE;
	vsdb->mdeepcolor30 = FALSE;
	vsdb->mdeepcolor36 = FALSE;
	vsdb->mdeepcolor48 = FALSE;
	vsdb->mdeepcolory444 = FALSE;
	vsdb->mdvidual = FALSE;
	vsdb->mmaxtmdsclk = 0;
	vsdb->mvideolatency = 0;
	vsdb->maudiolatency = 0;
	vsdb->minterlacedvideolatency = 0;
	vsdb->minterlacedaudiolatency = 0;
	vsdb->mid = 0;
	vsdb->mlength = 0;
	vsdb->mvalid = FALSE;
}
int hdmivsdb_parse(hdmivsdb_t *vsdb, u8 * data)
{
	u8 blocklength = 0;
	LOG_TRACE();
	hdmivsdb_reset(vsdb);
	if (data == 0)
	{
		return FALSE;
	}
	if (bitoperation_bitfield(data[0], 5, 3) != 0x3)
	{
		LOG_WARNING("Invalid datablock tag");
		return FALSE;
	}
	blocklength = bitoperation_bitfield(data[0], 0, 5);
	hdmivsdb_setlength(vsdb, blocklength);
	if (blocklength < 5)
	{
		LOG_WARNING("Invalid minimum length");
		return FALSE;
	}
	if (bitoperation_bytes2dword(0x00, data[3], data[2], data[1]) != 0x000C03)
	{
		LOG_WARNING("HDMI IEEE registration identifier not valid");
		return FALSE;
	}
	hdmivsdb_reset(vsdb);
	hdmivsdb_setid(vsdb, 0x000C03);
	vsdb->mphysicaladdress = bitoperation_bytes2word(data[4], data[5]);
	/* parse extension fields if they exist */
	if (blocklength > 5)
	{
		vsdb->msupportsai = bitoperation_bitfield(data[6], 7, 1) == 1;
		vsdb->mdeepcolor48 = bitoperation_bitfield(data[6], 6, 1) == 1;
		vsdb->mdeepcolor36 = bitoperation_bitfield(data[6], 5, 1) == 1;
		vsdb->mdeepcolor30 = bitoperation_bitfield(data[6], 4, 1) == 1;
		vsdb->mdeepcolory444 = bitoperation_bitfield(data[6], 3, 1) == 1;
		vsdb->mdvidual = bitoperation_bitfield(data[6], 0, 1) == 1;
}
	else
	{
		vsdb->msupportsai = FALSE;
		vsdb->mdeepcolor48 = FALSE;
		vsdb->mdeepcolor36 = FALSE;
		vsdb->mdeepcolor30 = FALSE;
		vsdb->mdeepcolory444 = FALSE;
		vsdb->mdvidual = FALSE;
	}
	vsdb->mmaxtmdsclk = (blocklength > 6) ? data[7] : 0;
	vsdb->mvideolatency = 0;
	vsdb->maudiolatency = 0;
	vsdb->minterlacedvideolatency = 0;
	vsdb->minterlacedaudiolatency = 0;
	if (blocklength > 7)
	{
		if (bitoperation_bitfield(data[8], 7, 1) == 1)
		{
			if (blocklength < 10)
			{
				LOG_WARNING("Invalid length - latencies are not valid");
				return FALSE;
			}
			if (bitoperation_bitfield(data[8], 6, 1) == 1)
			{
				if (blocklength < 12)
				{
					LOG_WARNING("Invalid length - Interlaced latencies are not valid");
					return FALSE;
				}
				else
				{
					vsdb->mvideolatency = data[9];
					vsdb->maudiolatency = data[10];
					vsdb->minterlacedvideolatency = data[11];
					vsdb->minterlacedaudiolatency = data[12];
				}
			}
			else
			{
				vsdb->mvideolatency = data[9];
				vsdb->maudiolatency = data[10];
				vsdb->minterlacedvideolatency = data[9];
				vsdb->minterlacedaudiolatency = data[10];
			}
		}
	}
	vsdb->mvalid = TRUE;
//		printk("supportsAi = %d, deepcolor48 = %d, color36 = %d, color30 = %d, colory444 = %d,\
//				dvidual = %d\n", vsdb->msupportsai, vsdb->mdeepcolor48, vsdb->mdeepcolor36,\
//				vsdb->mdeepcolor30, vsdb->mdeepcolory444, vsdb->mdvidual);
//	printk("vidoe latency = %d, audio latency = %d, maxtdmslck = %d\n", vsdb->mvideolatency, vsdb->maudiolatency,
//			vsdb->mmaxtmdsclk);

	return TRUE;
}
int hdmivsdb_getdeepcolor30(hdmivsdb_t *vsdb)
{
	return vsdb->mdeepcolor30;
}

int hdmivsdb_getdeepcolor36(hdmivsdb_t *vsdb)
{
	return vsdb->mdeepcolor36;
}

int hdmivsdb_getdeepcolor48(hdmivsdb_t *vsdb)
{
	return vsdb->mdeepcolor48;
}

int hdmivsdb_getdeepcolory444(hdmivsdb_t *vsdb)
{
	return vsdb->mdeepcolory444;
}

int hdmivsdb_getsupportsai(hdmivsdb_t *vsdb)
{
	return vsdb->msupportsai;
}

int hdmivsdb_getdvidual(hdmivsdb_t *vsdb)
{
	return vsdb->mdvidual;
}

u8 hdmivsdb_getmaxtmdsclk(hdmivsdb_t *vsdb)
{
	return vsdb->mmaxtmdsclk;
}

u16 hdmivsdb_getpvideolatency(hdmivsdb_t *vsdb)
{
	return vsdb->mvideolatency;
}

u16 hdmivsdb_getpaudiolatency(hdmivsdb_t *vsdb)
{
	return vsdb->maudiolatency;
}

u16 hdmivsdb_getiaudiolatency(hdmivsdb_t *vsdb)
{
	return vsdb->minterlacedaudiolatency;
}

u16 hdmivsdb_getivideolatency(hdmivsdb_t *vsdb)
{
	return vsdb->minterlacedvideolatency;
}

u16 hdmivsdb_getphysicaladdress(hdmivsdb_t *vsdb)
{
	return vsdb->mphysicaladdress;
}

u32 hdmivsdb_getid(hdmivsdb_t *vsdb)
{
	return vsdb->mid;
}

u8 hdmivsdb_getlength(hdmivsdb_t *vsdb)
{
	return vsdb->mlength;
}

void hdmivsdb_setid(hdmivsdb_t *vsdb, u32 id)
{
	vsdb->mid = id;
}

void hdmivsdb_setlength(hdmivsdb_t *vsdb, u8 length)
{
	vsdb->mlength = length;
}
