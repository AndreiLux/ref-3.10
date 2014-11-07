/*
 * video.c
 *
 *  Created on: Jul 5, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "video.h"
#include "../util/log.h"
#include "halColorSpaceConverter.h"
#include "halFrameComposerVideo.h"
#include "halIdentification.h"
#include "halMainController.h"
#include "halVideoPacketizer.h"
#include "halVideoSampler.h"
#include "halVideoGenerator.h"
#include "halFrameComposerDebug.h"
#include "../edid/dtd.h"
#include "../util/error.h"

const u16 tx_base_addr = 0x0200;
const u16 vp_base_addr = 0x0800;
static const u16 fc_base_addr = 0x1000;
static const u16 mc_base_addr = 0x4000;
const u16 csc_base_addr = 0x4100;
const u16 vg_base_addr = 0x7000;

int video_initialize(u16 baseAddr, videoParams_t *params, u8 dataEnablePolarity)
{
	LOG_TRACE1(dataEnablePolarity);
	return TRUE;
}
int video_configure(
	u16 baseAddr, videoParams_t *params,
	u8 dataEnablePolarity, u8 hdcp, u8 fixed_color_screen)
{
	LOG_TRACE();

	/* DVI mode does not support pixel repetition */

 	if (!videoparams_gethdmi(params) && videoparams_ispixelrepetition(params))
	{
		error_set(ERR_DVI_MODE_WITH_PIXEL_REPETITION);
		LOG_ERROR("DVI mode with pixel repetition: video not transmitted");
		return FALSE;
	}
	if (video_forceoutput(baseAddr, fixed_color_screen) != TRUE)
		return FALSE;
	if (video_framecomposer(baseAddr, params, dataEnablePolarity, hdcp) != TRUE)
		return FALSE;
	if (video_videopacketizer(baseAddr, params) != TRUE)
		return FALSE;
	if (video_colorspaceconverter(baseAddr, params) != TRUE)
		return FALSE;
	if (video_videosampler(baseAddr, params) != TRUE)
		return FALSE;
	return video_videogenerator(baseAddr, params, dataEnablePolarity);
}

int video_forceoutput(u16 baseAddr, u8 fixed_color_screen)
{
	halframecomposerdebug_forceaudio(baseAddr + fc_base_addr + 0x0200,
			fixed_color_screen);
	halframecomposerdebug_forcevideo(baseAddr + fc_base_addr + 0x0200,
			fixed_color_screen);
	return TRUE;
}

int video_colorspaceconverter(u16 baseAddr, videoParams_t *params)
{
	unsigned interpolation = 0;
	unsigned decimation = 0;
	unsigned color_depth = 0;

	LOG_TRACE();

	if (videoparams_iscolorspaceinterpolation(params))
	{
		if (videoparams_getcscfilter(params) > 1)
		{
			error_set(ERR_CHROMA_INTERPOLATION_FILTER_INVALID);
			LOG_ERROR2("invalid chroma interpolation filter: ",\
				videoparams_getcscfilter(params));
			return FALSE;
		}
		interpolation = 1 + videoparams_getcscfilter(params);
	}
	else if (videoparams_iscolorspacedecimation(params))
	{
		if (videoparams_getcscfilter(params) > 2)
		{
			error_set(ERR_CHROMA_DECIMATION_FILTER_INVALID);
			LOG_ERROR2("invalid chroma decimation filter: ",\
				videoparams_getcscfilter(params));
			return FALSE;
		}
		decimation = 1 + videoparams_getcscfilter(params);
	}

	if ((videoparams_getcolorresolution(params) == 8)
			|| (videoparams_getcolorresolution(params) == 0))
		color_depth = 4;
	else if (videoparams_getcolorresolution(params) == 10)
		color_depth = 5;
	else if (videoparams_getcolorresolution(params) == 12)
		color_depth = 6;
	else if (videoparams_getcolorresolution(params) == 16)
		color_depth = 7;
	else
	{
		error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
		LOG_ERROR2("invalid color depth: ", videoparams_getcolorresolution(params));
		return FALSE;
	}

	halcolorspaceconverter_interpolation(baseAddr + csc_base_addr,
			interpolation);
	halcolorspaceconverter_decimation(baseAddr + csc_base_addr,
			decimation);
	halcolorspaceconverter_coefficienta1(baseAddr + csc_base_addr,
			videoparams_getcsca(params)[0]);
	halcolorspaceconverter_coefficienta2(baseAddr + csc_base_addr,
			videoparams_getcsca(params)[1]);
	halcolorspaceconverter_coefficienta3(baseAddr + csc_base_addr,
			videoparams_getcsca(params)[2]);
	halcolorspaceconverter_coefficienta4(baseAddr + csc_base_addr,
			videoparams_getcsca(params)[3]);
	halcolorspaceconverter_coefficientb1(baseAddr + csc_base_addr,
			videoparams_getcscb(params)[0]);
	halcolorspaceconverter_coefficientb2(baseAddr + csc_base_addr,
			videoparams_getcscb(params)[1]);
	halcolorspaceconverter_coefficientb3(baseAddr + csc_base_addr,
			videoparams_getcscb(params)[2]);
	halcolorspaceconverter_coefficientb4(baseAddr + csc_base_addr,
			videoparams_getcscb(params)[3]);
	halcolorspaceconverter_coefficientc1(baseAddr + csc_base_addr,
			videoparams_getcscc(params)[0]);
	halcolorspaceconverter_coefficientc2(baseAddr + csc_base_addr,
			videoparams_getcscc(params)[1]);
	halcolorspaceconverter_coefficientc3(baseAddr + csc_base_addr,
			videoparams_getcscc(params)[2]);
	halcolorspaceconverter_coefficientc4(baseAddr + csc_base_addr,
			videoparams_getcscc(params)[3]);
	halcolorspaceconverter_scalefactor(baseAddr + csc_base_addr,
			videoparams_getcscscale(params));
	halcolorspaceconverter_colordepth(baseAddr + csc_base_addr,
			color_depth);
	return TRUE;
}

int video_framecomposer(
	u16 baseAddr, videoParams_t *params, u8 dataEnablePolarity, u8 hdcp)
{
	const dtd_t *dtd = videoparams_getdtd(params);
	u16 i = 0;

	LOG_TRACE();
	/* HDCP support was checked previously */
	halframecomposervideo_hdcpkeepout(
	baseAddr + fc_base_addr, hdcp);
	halframecomposervideo_vsyncpolarity(
		baseAddr + fc_base_addr, dtd_getvsyncpolarity(dtd));
	halframecomposervideo_hsyncpolarity(
		baseAddr + fc_base_addr, dtd_gethsyncpolarity(dtd));
	halframecomposervideo_dataenablepolarity(
		baseAddr + fc_base_addr,dataEnablePolarity);
	halframecomposervideo_dviorhdmi(
		baseAddr + fc_base_addr,videoparams_gethdmi(params));
	halframecomposervideo_vblankosc(
		baseAddr + fc_base_addr,(dtd_getcode(dtd) == 39) \
			? 0 : dtd_getinterlaced(dtd));
	halframecomposervideo_interlaced(
		baseAddr + fc_base_addr,dtd_getinterlaced(dtd));
	halframecomposervideo_hactive(
		baseAddr + fc_base_addr,dtd_gethactive(dtd));
	halframecomposervideo_hblank(
		baseAddr + fc_base_addr,dtd_gethblanking(dtd));
	halframecomposervideo_vactive(
		baseAddr + fc_base_addr,dtd_getvactive(dtd));
	halframecomposervideo_vblank(
		baseAddr + fc_base_addr,dtd_getvblanking(dtd));
	halframecomposervideo_hsyncedgedelay(
		baseAddr + fc_base_addr,dtd_gethsyncoffset(dtd));
	halframecomposervideo_hsyncpulsewidth(
		baseAddr + fc_base_addr,dtd_gethsyncpulsewidth(dtd));
	halframecomposervideo_vsyncedgedelay(
		baseAddr + fc_base_addr,dtd_getvsyncoffset(dtd));
	halframecomposervideo_vsyncpulsewidth(
		baseAddr + fc_base_addr,dtd_getvsyncpulsewidth(dtd));
	halframecomposervideo_controlperiodminduration(
		baseAddr+ fc_base_addr, 12);
	halframecomposervideo_extendedcontrolperiodminduration(
		baseAddr+ fc_base_addr, 32);

	halframecomposervideo_refreshrate(baseAddr+ fc_base_addr, 60000);

	/* spacing < 256^2 * config / tmdsClock, spacing <= 50ms
	 * worst case: tmdsClock == 25MHz => config <= 19
	 */
	halframecomposervideo_extendedcontrolperiodmaxspacing(
		baseAddr + fc_base_addr, 0x21);	/*1);*/
	for (i = 0; i < 3; i++)
	{
		halframecomposervideo_preamblefilter(
			baseAddr + fc_base_addr, (i + 1) * 11, i);
	}
	halframecomposervideo_pixelrepetitioninput(
		baseAddr + fc_base_addr, dtd_getpixelrepetitioninput(dtd)+ 1);
	return TRUE;
}

int video_videopacketizer(u16 baseAddr, videoParams_t *params)
{
	unsigned color_depth = 0;
	unsigned remap_size = 0;
	unsigned output_select = 0;

	LOG_TRACE();
	if (videoparams_getencodingout(params) == RGB
			|| videoparams_getencodingout(params) == YCC444)
	{
		if (videoparams_getcolorresolution(params) == 0)
			output_select = 3;
		else if (videoparams_getcolorresolution(params) == 8)
		{
			color_depth = 4;
			output_select = 3;
		}
		else if (videoparams_getcolorresolution(params) == 10)
			color_depth = 5;
		else if (videoparams_getcolorresolution(params) == 12)
			color_depth = 6;
		else if (videoparams_getcolorresolution(params) == 16)
			color_depth = 7;
		else
		{
			error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
			LOG_ERROR2("invalid color depth: ",\
				videoparams_getcolorresolution(params));
			return FALSE;
		}
	}
	else if (videoparams_getencodingout(params) == YCC422)
	{
		if ((videoparams_getcolorresolution(params) == 8)
				|| (videoparams_getcolorresolution(params) == 0))
			remap_size = 0;
		else if (videoparams_getcolorresolution(params) == 10)
			remap_size = 1;
		else if (videoparams_getcolorresolution(params) == 12)
			remap_size = 2;
		else
		{
			error_set(ERR_COLOR_REMAP_SIZE_INVALID);
			LOG_ERROR2("invalid color remap size: ",\
				videoparams_getcolorresolution(params));
			return FALSE;
		}
		output_select = 1;
	}
	else
	{
		error_set(ERR_OUTPUT_ENCODING_TYPE_INVALID);
		LOG_ERROR2("invalid output encoding type: ",\
			videoparams_getencodingout(params));
		return FALSE;
	}

	halvideopacketizer_pixelrepetitionfactor(baseAddr + vp_base_addr,
			videoparams_getpixelrepetitionfactor(params));
/* odin doesn't support deep color mode color_depth -> 0*/
	halvideopacketizer_colordepth(baseAddr + vp_base_addr, 0);
	halvideopacketizer_pixelpackingdefaultphase(baseAddr
			+ vp_base_addr, videoparams_getpixelpackingdefaultphase(params));
	halvideopacketizer_ycc422remapsize(baseAddr + vp_base_addr,
			remap_size);
	halvideopacketizer_outputselector(baseAddr + vp_base_addr,
			output_select);
	return TRUE;
}

int video_videosampler(u16 baseAddr, videoParams_t *params)
{
	unsigned map_code = 0;

	LOG_TRACE();
	if (videoparams_getencodingin(params) == RGB || videoparams_getencodingin(
			params) == YCC444)
	{
		if ((videoparams_getcolorresolution(params) == 8)
				|| (videoparams_getcolorresolution(params) == 0))
			map_code = 1;
		else if (videoparams_getcolorresolution(params) == 10)
			map_code = 3;
		else if (videoparams_getcolorresolution(params) == 12)
			map_code = 5;
		else if (videoparams_getcolorresolution(params) == 16)
			map_code = 7;
		else
		{
			error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
			LOG_ERROR2("invalid color depth: ",\
				videoparams_getcolorresolution(params));
			return FALSE;
		}
		map_code += (videoparams_getencodingin(params) == YCC444) ? 8 : 0;
	}
	else if (videoparams_getencodingin(params) == YCC422)
	{
		/* YCC422 mapping is discontinued - only map 1 is supported */
		if (videoparams_getcolorresolution(params) == 12)
			map_code = 18;
		else if (videoparams_getcolorresolution(params) == 10)
			map_code = 20;
		else if ((videoparams_getcolorresolution(params) == 8)
				|| (videoparams_getcolorresolution(params) == 0))
			map_code = 22;
		else
		{
			error_set(ERR_COLOR_REMAP_SIZE_INVALID);
			LOG_ERROR2("invalid color remap size: ",\
				videoparams_getcolorresolution(params));
			return FALSE;
		}
	}
	else
	{
		error_set(ERR_INPUT_ENCODING_TYPE_INVALID);
		LOG_ERROR2("invalid input encoding type: ",\
			videoparams_getencodingin(params));
		return FALSE;
	}
/* 0x312d0200 0x1 ---> 0x81 setting for hdmi-timing-info use
* 0x312d0200 0x0 --> dss hdmi-timing-info use
*/
	halvideosampler_internaldataenablegenerator(baseAddr
			+ tx_base_addr, 1);
	halvideosampler_videomapping(baseAddr + tx_base_addr, map_code);
	halvideosampler_stuffinggy(baseAddr + tx_base_addr, 0x5555);
	halvideosampler_stuffingrcr(baseAddr + tx_base_addr, 0xaaaa);
	halvideosampler_stuffingbcb(baseAddr + tx_base_addr, 0x5555);
	return TRUE;
}

int video_videogenerator(
	u16 baseAddr, videoParams_t *params, u8 dataEnablePolarity)
{
	/*
	 * video generator block is not included in real application hardware,
	 * then the external video sources are used and this code has no effect
	 */
	unsigned resolution = 0;
	LOG_TRACE();

	if ((videoparams_getcolorresolution(params) == 8)
			|| (videoparams_getcolorresolution(params) == 0))
		resolution = 0;
	else if (videoparams_getcolorresolution(params) == 10)
		resolution = 1;
	else if (videoparams_getcolorresolution(params) == 12)
		resolution = 2;
	else if (videoparams_getcolorresolution(params) == 16
			&& videoparams_getencodingin(params) != YCC422)
		resolution = 3;
	else
	{
		error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
		LOG_ERROR2("invalid color depth: ",\
			videoparams_getcolorresolution(params));
		return FALSE;
	}
/*	DipHDMI_ColorBar(dtd->mcode, 400);*/
/*
	halvideogenerator_ycc(baseAddr + vg_base_addr,
			videoparams_getencodingin(params) != RGB);
	halvideogenerator_ycc422(baseAddr + vg_base_addr,
			videoparams_getencodingin(params) == YCC422);
	halvideogenerator_vblankosc(baseAddr + vg_base_addr, (dtd_getcode(
			dtd) == 39) ? 0 : (dtd_getinterlaced(dtd) ? 1 : 0));
	halvideogenerator_colorincrement(baseAddr + vg_base_addr, 0);
	halvideogenerator_interlaced(baseAddr + vg_base_addr,
			dtd_getinterlaced(dtd));
	halvideogenerator_vsyncpolarity(baseAddr + vg_base_addr,
			dtd_getvsyncpolarity(dtd));
	halvideogenerator_hsyncpolarity(baseAddr + vg_base_addr,
			dtd_gethsyncpolarity(dtd));
	halvideogenerator_dataenablepolarity(baseAddr + vg_base_addr,
			dataEnablePolarity);
	halvideogenerator_colorresolution(baseAddr + vg_base_addr,
			resolution);
	halvideogenerator_pixelrepetitioninput(baseAddr + vg_base_addr,
			dtd_getpixelrepetitioninput(dtd));
	halvideogenerator_hactive(baseAddr + vg_base_addr, dtd_gethactive(
			dtd));
	halvideogenerator_hblank(baseAddr + vg_base_addr,
			dtd_gethblanking(dtd));
	halvideogenerator_hsyncedgedelay(baseAddr + vg_base_addr,
			dtd_gethsyncoffset(dtd));
	halvideogenerator_hsyncpulsewidth(baseAddr + vg_base_addr,
			dtd_gethsyncpulsewidth(dtd));
	halvideogenerator_vactive(baseAddr + vg_base_addr, dtd_getvactive(
			dtd));
	halvideogenerator_vblank(baseAddr + vg_base_addr,
			dtd_getvblanking(dtd));
	halvideogenerator_vsyncedgedelay(baseAddr + vg_base_addr,
			dtd_getvsyncoffset(dtd));
	halvideogenerator_vsyncpulsewidth(baseAddr + vg_base_addr,
			dtd_getvsyncpulsewidth(dtd));
	halvideogenerator_enable3d(baseAddr + vg_base_addr,
			(videoparams_gethdmivideoformat(params) == 2) ? 1 : 0);
	halvideogenerator_structure3d(baseAddr + vg_base_addr,
			videoparams_get3dstructure(params));
	halvideogenerator_swreset(baseAddr + vg_base_addr, 1);
*/
	return TRUE;
}
