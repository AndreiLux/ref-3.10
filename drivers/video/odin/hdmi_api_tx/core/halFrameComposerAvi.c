/*
 * @file:halFrameComposerAvi.c
 *
 *  Created on: Jun 30, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halFrameComposerAvi.h"
#include "../bsp/access.h"
#include "../util/log.h"

/* register offsets */
static const u8 fc_aviconf0 = 0x19;
static const u8 fc_aviconf1 = 0x1A;
static const u8 fc_aviconf2 = 0x1B;
static const u8 fc_aviconf3 = 0x17;
static const u8 fc_avivid = 0x1C;
static const u8 fc_avietb0 = 0x1D;
static const u8 fc_avietb1 = 0x1E;
static const u8 fc_avisbb0 = 0x1F;
static const u8 fc_avisbb1 = 0x20;
static const u8 fc_avielb0 = 0x21;
static const u8 fc_avielb1 = 0x22;
static const u8 fc_avisrb0 = 0x23;
static const u8 fc_avisrb1 = 0x24;
static const u8 fc_prconf = 0xE0;
/* bit shifts */
static const u8 rgbycc = 0;
static const u8 scan_info = 4;
static const u8 colorimetry = 6;
static const u8 pic_aspect_ratio = 4;
static const u8 active_format_aspect_ratio = 0;
static const u8 active_format_ar_valid = 6;
static const u8 it_content = 7;
static const u8 ext_colorimetry = 4;
static const u8 quantization_range = 2;
static const u8 non_uniform_pic_scaling = 0;
static const u8 h_bar_info = 3;
static const u8 v_bar_info = 2;
static const u8 pixel_rep_out = 0;

void halframecomposeravi_rgbycc(u16 baseAddr, u8 type)
{
	LOG_TRACE1(type);
	access_corewrite(type, baseAddr + fc_aviconf0, rgbycc, 2);
}

void halframecomposeravi_scaninfo(u16 baseAddr, u8 left)
{
	LOG_TRACE1(left);
	access_corewrite(left, baseAddr + fc_aviconf0, scan_info, 2);
}

void halframecomposeravi_colorimety(u16 baseAddr, unsigned cscITU)
{
	LOG_TRACE1(cscITU);
	access_corewrite(cscITU, baseAddr + fc_aviconf1, colorimetry, 2);
}

void halframecomposeravi_picaspectratio(u16 baseAddr, u8 ar)
{
	LOG_TRACE1(ar);
	access_corewrite(ar, baseAddr + fc_aviconf1, pic_aspect_ratio, 2);
}

void halframecomposeravi_activeaspectratiovalid(u16 baseAddr, u8 valid)
{
	LOG_TRACE1(valid);
	/* data valid flag */
	access_corewrite(valid, baseAddr + fc_aviconf0, active_format_ar_valid, 1);
}

void halframecomposeravi_activeformataspectratio(u16 baseAddr, u8 left)
{
	LOG_TRACE1(left);
	access_corewrite(left, baseAddr + fc_aviconf1, active_format_aspect_ratio, 4);
}

void halframecomposeravi_isltcontent(u16 baseAddr, u8 it)
{
	LOG_TRACE1(it);
	access_corewrite((it ? 1 : 0), baseAddr + fc_aviconf2, it_content, 1);
}

void halframecomposeravi_extendedcolorimetry(u16 baseAddr, u8 extColor)
{
	LOG_TRACE1(extColor);
	access_corewrite(extColor, baseAddr + fc_aviconf2, ext_colorimetry, 3);
	/*data valid flag */
	access_corewrite(0x3, baseAddr + fc_aviconf1, colorimetry, 2);
}

void halframecomposeravi_quantizationrange(u16 baseAddr, u8 range)
{
	LOG_TRACE1(range);
	access_corewrite(range, baseAddr + fc_aviconf2, quantization_range, 2);
}

void halframecomposeravi_quantizationrangecea(u32 baseAddr, u8 range)
{
	LOG_TRACE1(range);
	access_corewrite(range, baseAddr + fc_aviconf3, 2, 2);
}

void halframecomposeravi_itcontentcea(u32 baseAddr, u8 it_content)
{
	LOG_TRACE1(it_content);
	access_corewrite(it_content, baseAddr + fc_aviconf3, 0, 2);
}
void halframecomposeravi_nonuniformpicscaling(u16 baseAddr, u8 scale)
{
	LOG_TRACE1(scale);
	access_corewrite(scale, baseAddr + fc_aviconf2, non_uniform_pic_scaling, 2);
}

void halframecomposeravi_videocode(u16 baseAddr, u8 code)
{
	LOG_TRACE1(code);
	access_corewritebyte(code, baseAddr + fc_avivid);
}

void halframecomposeravi_horizontalbarsvalid(u16 baseAddr, u8 validity)
{
	/*data valid flag */
	access_corewrite((validity ? 1 : 0), baseAddr + fc_aviconf0, h_bar_info, 1);
}

void halframecomposeravi_horizontalbars(u16 baseAddr, u16 endTop,
		u16 startBottom)
{
	LOG_TRACE2(endTop, startBottom);
	access_corewritebyte((u8) (endTop), baseAddr + fc_avietb0);
	access_corewritebyte((u8) (endTop >> 8), baseAddr + fc_avietb1);
	access_corewritebyte((u8) (startBottom), baseAddr + fc_avisbb0);
	access_corewritebyte((u8) (startBottom >> 8), baseAddr + fc_avisbb1);
}

void halframecomposeravi_verticalbarsvalid(u16 baseAddr, u8 validity)
{
	/*data valid flag */
	access_corewrite((validity ? 1 : 0), baseAddr + fc_aviconf0, v_bar_info, 1);
}

void halframecomposeravi_verticalbars(u16 baseAddr, u16 endLeft, u16 startRight)
{
	LOG_TRACE2(endLeft, startRight);
	access_corewritebyte((u8) (endLeft), baseAddr + fc_avielb0);
	access_corewritebyte((u8) (endLeft >> 8), baseAddr + fc_avielb1);
	access_corewritebyte((u8) (startRight), baseAddr + fc_avisrb0);
	access_corewritebyte((u8) (startRight >> 8), baseAddr + fc_avisrb1);
}

void halframecomposeravi_outpixelrepetition(u16 baseAddr, u8 pr)
{
	LOG_TRACE1(pr);
	access_corewrite(pr, baseAddr + fc_prconf, pixel_rep_out, 4);
}

