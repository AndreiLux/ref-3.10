/*
 * @file:packets.c
 *
 *  Created on: Jul 7, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "packets.h"
#include "halFrameComposerGcp.h"
#include "halFrameComposerPackets.h"
#include "halFrameComposerGamut.h"
#include "halFrameComposerAcp.h"
#include "halFrameComposerAudioInfo.h"
#include "halFrameComposerAvi.h"
#include "halFrameComposerIsrc.h"
#include "halFrameComposerSpd.h"
#include "halFrameComposerVsd.h"
#include "../util/error.h"
#include "../util/log.h"

#define ACP_PACKET_SIZE 16
#define ISRC_PACKET_SIZE 16
static const u16 fc_base_addr = 0x1000;

int packets_initialize(u16 baseAddr)
{
	LOG_TRACE();
	packets_disableallpackets(baseAddr + fc_base_addr);
	return TRUE;
}

int packets_configure(u16 baseAddr, videoParams_t * video,
		productParams_t * prod)
{
	u8 send3d = FALSE;
	u8 rdrb_num = 0;
	u8 rdrb[8] = { 0x0, 0x10, 0x1, 0x61, 0x0, 0x10, 0x1, 0x73};
	u8 content[28] = {0xFF, 0xAA, 0x0, 0xFF, 0x0, 0xFF, 0x0, 0xFF,
		0x55, 0x0, 0xFF, 0x0, 0xAA, 0xFF, 0xAA, 0x0, 0xAA, 0xAA,
		0xAA, 0xAA, 0xAA, 0xFF, 0xAA, 0x0, 0x55, 0x0, 0x55, 0xFF};
	LOG_TRACE();
	if (videoparams_gethdmivideoformat(video) == 2)
	{
		if (videoparams_get3dstructure(video) == 6
				|| videoparams_get3dstructure(video) == 8)
		{
			u8 data[3];
			data[0] = videoparams_gethdmivideoformat(video) << 5;
			data[1] = videoparams_get3dstructure(video) << 4;
			data[2] = videoparams_get3dextdata(video) << 4;
			packets_vendorspecificinfoframe(baseAddr, 0x000C03,
					data, sizeof(data), 1); /* HDMI Licensing, LLC */
			send3d = TRUE;
		}
		else
		{
			LOG_ERROR2("3D structure not supported",
					videoparams_get3dstructure(video));
			error_set(ERR_3D_STRUCT_NOT_SUPPORTED);
			return FALSE;
		}
	}
	if (prod != 0)
	{
		if (productparams_issourceproductvalid(prod))
		{
			packets_sourceproductinfoframe(baseAddr,
					productparams_getvendorname(prod),
					productparams_getvendornamelength(prod),
					productparams_getproductname(prod),
					productparams_getproductnamelength(prod),
					productparams_getsourcetype(prod), 1);
		}
		if (productparams_isvendorspecificvalid(prod))
		{
			if (send3d)
			{
				LOG_WARNING("forcing Vendor Specific InfoFrame,\
					3D configuration will be ignored");
				error_set(ERR_FORCING_VSD_3D_IGNORED);
			}
			packets_vendorspecificinfoframe(baseAddr,
					productparams_getoui(prod), productparams_getvendorpayload(
							prod), productparams_getvendorpayloadlength(prod),
					1);
		}
	}
	else
	{
		LOG_WARNING("No product info provided: not configured");
	}
	/* set values that shall not change */
	halframecomposerpackets_metadataframeinterpolation(baseAddr + fc_base_addr,	1);
	halframecomposerpackets_metadataframesperpacket(baseAddr + fc_base_addr, 4);
	halframecomposerpackets_metadatalinespacing(baseAddr + fc_base_addr, 5);
	for (rdrb_num =0; rdrb_num<8; rdrb_num++)
	{
		halframecomposerpackets_setrdrb(baseAddr + fc_base_addr,\
			rdrb_num, rdrb[rdrb_num]);
	}
	/* default phase 1 = true */
	halframecomposergcp_defaultphase(baseAddr + fc_base_addr,\
		videoparams_getpixelpackingdefaultphase(video) == 1);
	halframecomposergamut_profile(baseAddr + fc_base_addr + 0x100, 0x0);
	halframecomposergamut_enabletx(baseAddr + fc_base_addr + 0x100, 0x1);
	halframecomposergamut_packetsperframe(baseAddr + fc_base_addr + 0x100, 0x1);
	halframecomposergamut_packetlinespacing(baseAddr + fc_base_addr + 0x100,0x1);
	halframecomposergamut_content(baseAddr + fc_base_addr + 0x100, content, 0x1c);

	packets_auxiliaryvideoinfoframe(baseAddr, video);
	return TRUE;
}

void packets_audiocontentprotection(u16 baseAddr, u8 type, const u8 * fields,
		u8 length, u8 autoSend)
{
	u8 newfields[ACP_PACKET_SIZE];
	u16 i = 0;
	LOG_TRACE();
	halframecomposerpackets_autosend(baseAddr + fc_base_addr, 0, ACP_TX);
	halframecomposeracp_type(baseAddr + fc_base_addr, type);

	for (i = 0; i < length; i++)
	{
		newfields[i] = fields[i];
	}
	if (length < ACP_PACKET_SIZE)
	{
		for (i = length; i < ACP_PACKET_SIZE; i++)
		{
			newfields[i] = 0; /* Padding */
		}
		length = ACP_PACKET_SIZE;
	}
	halframecomposeracp_typedependentfields(baseAddr + fc_base_addr, newfields,
			length);
	if (!autoSend)
	{
		halframecomposerpackets_manualsend(baseAddr + fc_base_addr, ACP_TX);
	}
	else
	{
		halframecomposerpackets_autosend(baseAddr + fc_base_addr, autoSend,
				ACP_TX);
	}

}

void packets_isrcpackets(u16 baseAddr, u8 initStatus, const u8 * codes,
		u8 length, u8 autoSend)
{
	u16 i = 0;
	u8 newcodes[ISRC_PACKET_SIZE * 2];
	LOG_TRACE();
	halframecomposerpackets_autosend(baseAddr + fc_base_addr, 0, ISRC1_TX);
	halframecomposerpackets_autosend(baseAddr + fc_base_addr, 0, ISRC2_TX);
	halframecomposerisrc_status(baseAddr + fc_base_addr, initStatus);

	for (i = 0; i < length; i++)
	{
		newcodes[i] = codes[i];
	}

	if (length > ISRC_PACKET_SIZE)
	{
		for (i = length; i < (ISRC_PACKET_SIZE * 2); i++)
		{
			newcodes[i] = 0; /* Padding */
		}
		length = (ISRC_PACKET_SIZE * 2);
		halframecomposerisrc_isrc2codes(baseAddr + fc_base_addr, newcodes
				+ (ISRC_PACKET_SIZE * sizeof(u8)), length - ISRC_PACKET_SIZE);
		halframecomposerisrc_cont(baseAddr + fc_base_addr, 1);
		halframecomposerpackets_autosend(baseAddr + fc_base_addr, autoSend,
				ISRC2_TX);
		if (!autoSend)
		{
			halframecomposerpackets_manualsend(baseAddr + fc_base_addr,
					ISRC2_TX);
		}
	}
	if (length < ISRC_PACKET_SIZE)
	{
		for (i = length; i < ISRC_PACKET_SIZE; i++)
		{
			newcodes[i] = 0; /* Padding */
		}
		length = ISRC_PACKET_SIZE;
		halframecomposerisrc_cont(baseAddr + fc_base_addr, 0);
	}
	/* first part only */
	halframecomposerisrc_isrc1codes(baseAddr + fc_base_addr, newcodes, length);
	halframecomposerisrc_valid(baseAddr + fc_base_addr, 1);
	halframecomposerpackets_autosend(baseAddr + fc_base_addr, autoSend,
			ISRC1_TX);
	if (!autoSend)
	{
		halframecomposerpackets_manualsend(baseAddr + fc_base_addr, ISRC1_TX);
	}
}

void packets_audioinfoframe(u16 baseAddr, audioParams_t *params)
{
	LOG_TRACE();
	halframecomposeraudioinfo_channelcount(baseAddr + fc_base_addr,\
			audioparams_channelcount(params));
	halframecomposeraudioinfo_allocatechannels(baseAddr + fc_base_addr,
			audioparams_getchannelallocation(params));
	halframecomposeraudioinfo_levelshiftvalue(baseAddr + fc_base_addr,
			audioparams_getlevelshiftvalue(params));
	halframecomposeraudioinfo_downmixinhibit(baseAddr + fc_base_addr,
			audioparams_getdownmixinhibitflag(params));
	if ((audioparams_getcodingtype(params) == ONE_BIT_AUDIO)
			|| (audioparams_getcodingtype(params) == DST))
	{
		/* Audio InfoFrame sample frequency when OBA or DST */
		if (audioparams_getsamplingfrequency(params) == 32000)
		{
			halframecomposeraudioinfo_samplefreq(baseAddr + fc_base_addr, 1);
		}
		else if (audioparams_getsamplingfrequency(params) == 44100)
		{
			halframecomposeraudioinfo_samplefreq(baseAddr + fc_base_addr, 2);
		}
		else if (audioparams_getsamplingfrequency(params) == 48000)
		{
			halframecomposeraudioinfo_samplefreq(baseAddr + fc_base_addr, 3);
		}
		else if (audioparams_getsamplingfrequency(params) == 88200)
		{
			halframecomposeraudioinfo_samplefreq(baseAddr + fc_base_addr, 4);
		}
		else if (audioparams_getsamplingfrequency(params) == 96000)
		{
			halframecomposeraudioinfo_samplefreq(baseAddr + fc_base_addr, 5);
		}
		else if (audioparams_getsamplingfrequency(params) == 176400)
		{
			halframecomposeraudioinfo_samplefreq(baseAddr + fc_base_addr, 6);
		}
		else if (audioparams_getsamplingfrequency(params) == 192000)
		{
			halframecomposeraudioinfo_samplefreq(baseAddr + fc_base_addr, 7);
		}
		else
		{
			halframecomposeraudioinfo_samplefreq(baseAddr + fc_base_addr, 0);
		}
	}
	else
	{
		/* otherwise refer to stream header (0) */
		halframecomposeraudioinfo_samplefreq(baseAddr + fc_base_addr, 0);
	}
	/* for HDMI refer to stream header  (0) */
	halframecomposeraudioinfo_codingtype(baseAddr + fc_base_addr, 0);
	/* for HDMI refer to stream header  (0) */
	halframecomposeraudioinfo_samplingsize(baseAddr + fc_base_addr, 0);
}

void packets_avmute(u16 baseAddr, u8 enable)
{
	LOG_TRACE();
	halframecomposergcp_avmute(baseAddr + fc_base_addr, enable);
}
void packets_isrcstatus(u16 baseAddr, u8 status)
{
	LOG_TRACE();
	halframecomposerisrc_status(baseAddr + fc_base_addr, status);
}
void packets_stopsendacp(u16 baseAddr)
{
	LOG_TRACE();
	halframecomposerpackets_autosend(baseAddr + fc_base_addr, 0, ACP_TX);
}

void packets_stopsendisrc1(u16 baseAddr)
{
	LOG_TRACE();
	halframecomposerpackets_autosend(baseAddr + fc_base_addr, 0, ISRC1_TX);
	halframecomposerpackets_autosend(baseAddr + fc_base_addr, 0, ISRC2_TX);
}

void packets_stopsendisrc2(u16 baseAddr)
{
	LOG_TRACE();
	halframecomposerisrc_cont(baseAddr + fc_base_addr, 0);
	halframecomposerpackets_autosend(baseAddr + fc_base_addr, 0, ISRC2_TX);
}

void packets_stopsendspd(u16 baseAddr)
{
	LOG_TRACE();
	halframecomposerpackets_autosend(baseAddr + fc_base_addr, 0, SPD_TX);
}

void packets_stopsendvsd(u16 baseAddr)
{
	LOG_TRACE();
	halframecomposerpackets_autosend(baseAddr + fc_base_addr, 0, VSD_TX);
}

void packets_gamutmetadatapackets(u16 baseAddr, const u8 * gbdContent,
		u8 length)
{
	LOG_TRACE();
	halframecomposergamut_enabletx(baseAddr + fc_base_addr + 0x100, 1);
	halframecomposergamut_affectedseqno(
			baseAddr + fc_base_addr + 0x100,
			(halframecomposergamut_currentseqno(baseAddr + fc_base_addr + 0x100)
					+ 1) % 16); /* sequential */
	halframecomposergamut_content(
		baseAddr + fc_base_addr + 0x100, gbdContent, length);
	/* set next_field to 1 */
	halframecomposergamut_updatepacket(baseAddr + fc_base_addr + 0x100);
}

void packets_disableallpackets(u16 baseAddr)
{
	LOG_TRACE();
	halframecomposerpackets_disableall(baseAddr + fc_base_addr);
}

int packets_sourceproductinfoframe(u16 baseAddr, const u8 * vName, u8 vLength,
		const u8 * pName, u8 pLength, u8 code, u8 autoSend)
{
	const unsigned short psize = 8;
	const unsigned short vsize = 16;

	LOG_TRACE();
	/* prevent sending half the info. */
	halframecomposerpackets_autosend(baseAddr + fc_base_addr, 0, SPD_TX);
	if (vName == 0)
	{
		error_set(ERR_INVALID_PARAM_VENDOR_NAME);
		LOG_WARNING("invalid parameter");
		return FALSE;
	}
	if (vLength > vsize)
	{
		vLength = vsize;
		LOG_WARNING("vendor name truncated");
	}
	if (pName == 0)
	{
		error_set(ERR_INVALID_PARAM_PRODUCT_NAME);
		LOG_WARNING("invalid parameter");
		return FALSE;
	}
	if (pLength > psize)
	{
		pLength = psize;
		LOG_WARNING("product name truncated");
	}
	halframecomposerspd_vendorname(baseAddr + fc_base_addr, vName, vLength);
	halframecomposerspd_productname(baseAddr + fc_base_addr, pName, pLength);

	halframecomposerspd_sourcedeviceinfo(baseAddr + fc_base_addr, code);
	if (autoSend)
	{
		halframecomposerpackets_autosend(baseAddr + fc_base_addr, autoSend, SPD_TX);
	}
	else
	{
		halframecomposerpackets_manualsend(baseAddr + fc_base_addr, SPD_TX);
	}
	return TRUE;
}

int packets_vendorspecificinfoframe(u16 baseAddr, u32 oui, const u8 * payload,
		u8 length, u8 autoSend)
{
	LOG_TRACE();
	/* prevent sending half the info. */
	halframecomposerpackets_autosend(baseAddr + fc_base_addr, 0, VSD_TX);
	halframecomposervsd_vendoroui(baseAddr + fc_base_addr, oui);
	if (halframecomposervsd_vendorpayload(
		baseAddr + fc_base_addr, payload, length))
	{
		return FALSE; /* DEFINE ERROR */
	}
	if (autoSend)
	{
		halframecomposerpackets_autosend(baseAddr + fc_base_addr, autoSend, VSD_TX);
	}
	else
	{
		halframecomposerpackets_manualsend(baseAddr + fc_base_addr, VSD_TX);
	}
	return TRUE;
}

void packets_auxiliaryvideoinfoframe(u16 baseAddr, videoParams_t *videoParams)
{
	u16 endtop = 0;
	u16 startbottom = 0;
	u16 endleft = 0;
	u16 startright = 0;
	LOG_TRACE();
	if (videoparams_getencodingout(videoParams) == RGB)
	{
		halframecomposeravi_rgbycc(baseAddr + fc_base_addr, 0);
	}
	else if (videoparams_getencodingout(videoParams) == YCC422)
	{
		halframecomposeravi_rgbycc(baseAddr + fc_base_addr, 1);
	}
	else if (videoparams_getencodingout(videoParams) == YCC444)
	{
		halframecomposeravi_rgbycc(baseAddr + fc_base_addr, 2);
	}
	halframecomposeravi_scaninfo(
		baseAddr + fc_base_addr, videoparams_getscaninfo(videoParams));
	if (dtd_gethimagesize(videoparams_getdtd(videoParams)) != 0
			|| dtd_getvimagesize(videoparams_getdtd(videoParams)) != 0)
	{
		u8 pic = (dtd_gethimagesize(videoparams_getdtd(videoParams)) * 10)
				% dtd_getvimagesize(videoparams_getdtd(videoParams));
		/* 16:9 or 4:3 */
		halframecomposeravi_picaspectratio(
			baseAddr + fc_base_addr, (pic > 5) ? 2 : 1);
	}
	else
	{
		halframecomposeravi_picaspectratio(baseAddr + fc_base_addr, 0); /* No Data */
	}
	halframecomposeravi_isltcontent(
		baseAddr + fc_base_addr, videoparams_getitcontent(videoParams));
	halframecomposeravi_quantizationrange(
		baseAddr + fc_base_addr, videoparams_getrgbquantizationrange(videoParams));
	halframecomposeravi_nonuniformpicscaling(
		baseAddr + fc_base_addr, videoparams_getnonuniformscaling(videoParams));

	halframecomposeravi_quantizationrangecea(
		baseAddr + fc_base_addr, videoparams_getrgbquantizationrange(videoParams));
	halframecomposeravi_itcontentcea(
		baseAddr + fc_base_addr, videoparams_getitcontent(videoParams));

	if (dtd_getcode(videoparams_getdtd(videoParams)) != (u8) (-1))
	{
		halframecomposeravi_videocode(baseAddr + fc_base_addr, dtd_getcode(
		videoparams_getdtd(videoParams)));
	}
	else
	{
		halframecomposeravi_videocode(baseAddr + fc_base_addr, 0);
	}
	if (videoparams_getcolorimetry(videoParams) == EXTENDED_COLORIMETRY)
	{ /* ext colorimetry valid */
		if (videoparams_getextcolorimetry(videoParams) != (u8) (-1))
		{
			halframecomposeravi_extendedcolorimetry(baseAddr + fc_base_addr,
					videoparams_getextcolorimetry(videoParams));
			halframecomposeravi_colorimety(baseAddr + fc_base_addr,
					videoparams_getcolorimetry(videoParams)); /* EXT-3 */
		}
		else
		{
			halframecomposeravi_colorimety(baseAddr + fc_base_addr, 0); /* No Data */
		}
	}
	else
	{
		/* NODATA-0/ 601-1/ 709-2/ EXT-3 */
		halframecomposeravi_colorimety(
			baseAddr + fc_base_addr, videoparams_getcolorimetry(videoParams));
	}
	if (videoparams_getactiveformataspectratio(videoParams) != 0)
	{
		halframecomposeravi_activeformataspectratio(baseAddr + fc_base_addr,
				videoparams_getactiveformataspectratio(videoParams));
		halframecomposeravi_activeaspectratiovalid(baseAddr + fc_base_addr, 1);
	}
	else
	{
		halframecomposeravi_activeaspectratiovalid(baseAddr + fc_base_addr, 0);
	}
	if (videoparams_getendtopbar(videoParams) != (u16) (-1)
			|| videoparams_getstartbottombar(videoParams) != (u16) (-1))
	{
		if (videoparams_getendtopbar(videoParams) != (u16) (-1))
		{
			endtop = videoparams_getendtopbar(videoParams);
		}
		if (videoparams_getstartbottombar(videoParams) != (u16) (-1))
		{
			startbottom = videoparams_getstartbottombar(videoParams);
		}
		halframecomposeravi_horizontalbars(
			baseAddr + fc_base_addr, endtop, startbottom);
		halframecomposeravi_horizontalbarsvalid(
			baseAddr + fc_base_addr, 0);
	}
	else
	{
		halframecomposeravi_horizontalbarsvalid(baseAddr + fc_base_addr, 0);
	}
	if (videoparams_getendleftbar(videoParams) != (u16) (-1)
			|| videoparams_getstartrightbar(videoParams) != (u16) (-1))
	{
		if (videoparams_getendleftbar(videoParams) != (u16) (-1))
		{
			endleft = videoparams_getendleftbar(videoParams);
		}
		if (videoparams_getstartrightbar(videoParams) != (u16) (-1))
		{
			startright = videoparams_getstartrightbar(videoParams);
		}
		halframecomposeravi_verticalbars(
			baseAddr + fc_base_addr, endleft, startright);
		halframecomposeravi_verticalbarsvalid(baseAddr + fc_base_addr, 1);
	}
	else
	{
		halframecomposeravi_verticalbarsvalid(baseAddr + fc_base_addr, 0);
	}
	halframecomposeravi_outpixelrepetition(baseAddr + fc_base_addr,
			(dtd_getpixelrepetitioninput(videoparams_getdtd(videoParams)) + 1)
					* (videoparams_getpixelrepetitionfactor(videoParams) + 1)
					- 1);
}
