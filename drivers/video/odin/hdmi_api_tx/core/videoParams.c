/*
 * @file:videoParams.c
 *
 *  Created on: Jul 2, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "videoParams.h"

void videoparams_reset(videoParams_t *params)
{
	params->mhdmi = 1;
	params->mencodingout = RGB;
	params->mencodingin = RGB;
	params->mcolorresolution = 8;
	params->mpixelrepetitionfactor = 0;
	/* 0=limited Range(16~235), 1=Full Range(0~255)*/
	params->mrgbquantizationrange = 0;
/* odin doesn't support deep color mode.it should be 0. */
	params->mpixelpackingdefaultphase = 0;
	params->mcolorimetry = 3;
	params->mscaninfo = 0;
	params->mactiveformataspectratio = 10;
	params->mnonuniformscaling = 1;
	params->mextcolorimetry = 4;
	params->mitcontent = 2;	/* 0=graphic, 1=photo, 2=cinema, 3=game*/
	params->mendtopbar = 0xaaaa;
	params->mstartbottombar = ~0;
	params->mendleftbar = ~0;
	params->mstartrightbar = ~0;
	params->mcscfilter = 0;
	params->mhdmivideoformat = 0;
	params->m3dstructure = 0;
	params->m3deextdata = 0;
	params->mhdmivic = 0;
}

u8 videoparams_getactiveformataspectratio(videoParams_t *params)
{
	return (u8)params->mactiveformataspectratio;
}

u8 videoparams_getcolorimetry(videoParams_t *params)
{
	return (u8)params->mcolorimetry;
}

u8 videoparams_getcolorresolution(videoParams_t *params)
{
	return (u8)params->mcolorresolution;
}

u8 videoparams_getcscfilter(videoParams_t *params)
{
	return (u8)params->mcscfilter;
}

dtd_t* videoparams_getdtd(videoParams_t *params)
{
	return (dtd_t*)&(params->mdtd);
}

encoding_t videoparams_getencodingin(videoParams_t *params)
{
	return (encoding_t)params->mencodingin;
}

encoding_t videoparams_getencodingout(videoParams_t *params)
{
	return (encoding_t)params->mencodingout;
}

u16 videoparams_getendleftbar(videoParams_t *params)
{
	return (u16)params->mendleftbar;
}

u16 videoparams_getendtopbar(videoParams_t *params)
{
	return (u16)params->mendtopbar;
}

u8 videoparams_getextcolorimetry(videoParams_t *params)
{
	return (u8)params->mextcolorimetry;
}

u8 videoparams_gethdmi(videoParams_t *params)
{
	return (u8)params->mhdmi;
}

u8 videoparams_getitcontent(videoParams_t *params)
{
	return (u8)params->mitcontent;
}

u8 videoparams_getnonuniformscaling(videoParams_t *params)
{
	return (u8)params->mnonuniformscaling;
}

u8 videoparams_getpixelpackingdefaultphase(videoParams_t *params)
{
	return (u8)params->mpixelpackingdefaultphase;
}

u8 videoparams_getpixelrepetitionfactor(videoParams_t *params)
{
	return (u8)params->mpixelrepetitionfactor;
}

u8 videoparams_getrgbquantizationrange(videoParams_t *params)
{
	return (u8)params->mrgbquantizationrange;
}

u8 videoparams_getscaninfo(videoParams_t *params)
{
	return (u8)params->mscaninfo;
}

u16 videoparams_getstartbottombar(videoParams_t *params)
{
	return (u16)params->mstartbottombar;
}

u16 videoparams_getstartrightbar(videoParams_t *params)
{
	return (u16)params->mstartrightbar;
}

u16 videoparams_getcscscale(videoParams_t *params)
{
	return (u16)params->mcscscale;
}

void videoparams_setactiveformataspectratio(videoParams_t *params, u8 value)
{
	params->mactiveformataspectratio = value;
}

void videoparams_setcolorimetry(videoParams_t *params, u8 value)
{
	params->mcolorimetry = value;
}

void videoparams_setcolorresolution(videoParams_t *params, u8 value)
{
	params->mcolorresolution = value;
}

void videoparams_setcscfilter(videoParams_t *params, u8 value)
{
	params->mcscfilter = value;
}

void videoparams_setdtd(videoParams_t *params, dtd_t *dtd)
{
	params->mdtd = *dtd;
}

void videoparams_setencodingin(videoParams_t *params, encoding_t value)
{
	params->mencodingin = value;
}

void videoparams_setencodingout(videoParams_t *params, encoding_t value)
{
	params->mencodingout = value;
}

void videoparams_setendleftbar(videoParams_t *params, u16 value)
{
	params->mendleftbar = value;
}

void videoparams_setendtopbar(videoParams_t *params, u16 value)
{
	params->mendtopbar = value;
}

void videoparams_setextcolorimetry(videoParams_t *params, u8 value)
{
	params->mextcolorimetry = value;
}

void videoparams_sethdmi(videoParams_t *params, u8 value)
{
	params->mhdmi = value;
}

void videoparams_setitcontent(videoParams_t *params, u8 value)
{
	params->mitcontent = value;
}

void videoparams_setnonuniformscaling(videoParams_t *params, u8 value)
{
	params->mnonuniformscaling = value;
}

void videoparams_setpixelpackingdefaultphase(videoParams_t *params, u8 value)
{
	params->mpixelpackingdefaultphase = value;
}

void videoparams_setpixelrepetitionfactor(videoParams_t *params, u8 value)
{
	params->mpixelrepetitionfactor = value;
}

void videoparams_setrgbquantizationrange(videoParams_t *params, u8 value)
{
	params->mrgbquantizationrange = value;
}

void videoparams_setscaninfo(videoParams_t *params, u8 value)
{
	params->mscaninfo = value;
}

void videoparams_setstartbottombar(videoParams_t *params, u16 value)
{
	params->mstartbottombar = value;
}

void videoparams_setstartrightbar(videoParams_t *params, u16 value)
{
	params->mstartrightbar = value;
}

u16 * videoparams_getcsca(videoParams_t *params)
{
	videoparams_updatecsccoefficients(params);
	return params->mcsca;
}

void videoparams_setcsca(videoParams_t *params, u16 value[4])
{
	u16 i = 0;
	for (i = 0; i < sizeof(params->mcsca) / sizeof(params->mcsca[0]); i++)
	{
		params->mcsca[i] = value[i];
	}
}

u16 * videoparams_getcscb(videoParams_t *params)
{
	videoparams_updatecsccoefficients(params);
	return params->mcscb;
}

void videoparams_setcscb(videoParams_t *params, u16 value[4])
{
	u16 i = 0;
	for (i = 0; i < sizeof(params->mcscb) / sizeof(params->mcscb[0]); i++)
	{
		params->mcscb[i] = value[i];
	}
}

u16 * videoparams_getcscc(videoParams_t *params)
{
	videoparams_updatecsccoefficients(params);
	return params->mcscc;
}

void videoparams_setcscc(videoParams_t *params, u16 value[4])
{
	u16 i = 0;
	for (i = 0; i < sizeof(params->mcscc) / sizeof(params->mcscc[0]); i++)
	{
		params->mcscc[i] = value[i];
	}
}

void videoparams_setcscscale(videoParams_t *params, u16 value)
{
	params->mcscscale = value;
}

/* [0.01 MHz] */
u16 videoparams_getpixelclock(videoParams_t *params)
{
	return dtd_getpixelclock(&(params->mdtd));
}

/* [0.01 MHz] */
u16 videoparams_gettmdsclock(videoParams_t *params)
{
	return (u16) ((u32) (dtd_getpixelclock(&(params->mdtd))
			* (u32) (videoparams_getratioclock(params))) / 100);
}

/* 0.01 */
unsigned videoparams_getratioclock(videoParams_t *params)
{
	unsigned ratio = 100;

	if (params->mencodingout != YCC422)
	{
		if (params->mcolorresolution == 8)
		{
			ratio = 100;
		}
		else if (params->mcolorresolution == 10)
		{
			ratio = 125;
		}
		else if (params->mcolorresolution == 12)
		{
			ratio = 150;
		}
		else if (params->mcolorresolution == 16)
		{
			ratio = 200;
		}
	}
	return ratio * (params->mpixelrepetitionfactor + 1);
}

int videoparams_iscolorspaceconversion(videoParams_t *params)
{
	return params->mencodingin != params->mencodingout;
}

int videoparams_iscolorspacedecimation(videoParams_t *params)
{
	return params->mencodingout == YCC422 && (params->mencodingin == RGB
			|| params->mencodingin == YCC444);
}

int videoparams_iscolorspaceinterpolation(videoParams_t *params)
{
	return params->mencodingin == YCC422 && (params->mencodingout == RGB
			|| params->mencodingout == YCC444);
}

int videoparams_ispixelrepetition(videoParams_t *params)
{
	return params->mpixelrepetitionfactor > 0 || dtd_getpixelrepetitioninput(
			&(params->mdtd)) > 0;
}
u8 videoparams_gethdmivideoformat(videoParams_t *params)
{
	return params->mhdmivideoformat;
}

void videoparams_sethdmivideoformat(videoParams_t *params, u8 value)
{
	params->mhdmivideoformat = value;
}

u8 videoparams_get3dstructure(videoParams_t *params)
{
	return params->m3dstructure;
}

void videoparams_set3dstructure(videoParams_t *params, u8 value)
{
	params->m3dstructure = value;
}

u8 videoparams_get3dextdata(videoParams_t *params)
{
	return params->m3deextdata;
}

void videoparams_set3dextdata(videoParams_t *params, u8 value)
{
	params->m3deextdata = value;
}

u8 videoparams_gethdmivic(videoParams_t *params)
{
	return params->mhdmivic;
}

void videoparams_sethdmivic(videoParams_t *params, u8 value)
{
	params->mhdmivic = value;
}

void videoparams_updatecsccoefficients(videoParams_t *params)
{
	u16 i = 0;
	if (!videoparams_iscolorspaceconversion(params))
	{
		for (i = 0; i < 4; i++)
		{
			params->mcsca[i] = 0;
			params->mcscb[i] = 0;
			params->mcscc[i] = 0;
		}
		params->mcsca[0] = 0x2000;
		params->mcscb[1] = 0x2000;
		params->mcscc[2] = 0x2000;
		params->mcscscale = 1;
	}
	else if (videoparams_iscolorspaceconversion(params) && params->mencodingout
			== RGB)
	{
		if (params->mcolorimetry == ITU601)
		{
			params->mcsca[0] = 0x2000;
			params->mcsca[1] = 0x6926;
			params->mcsca[2] = 0x74fd;
			params->mcsca[3] = 0x010e;

			params->mcscb[0] = 0x2000;
			params->mcscb[1] = 0x2cdd;
			params->mcscb[2] = 0x0000;
			params->mcscb[3] = 0x7e9a;

			params->mcscc[0] = 0x2000;
			params->mcscc[1] = 0x0000;
			params->mcscc[2] = 0x38b4;
			params->mcscc[3] = 0x7e3b;

			params->mcscscale = 1;
		}
		else if (params->mcolorimetry == ITU709)
		{
			params->mcsca[0] = 0x2000;
			params->mcsca[1] = 0x7106;
			params->mcsca[2] = 0x7a02;
			params->mcsca[3] = 0x00a7;

			params->mcscb[0] = 0x2000;
			params->mcscb[1] = 0x3264;
			params->mcscb[2] = 0x0000;
			params->mcscb[3] = 0x7e6d;

			params->mcscc[0] = 0x2000;
			params->mcscc[1] = 0x0000;
			params->mcscc[2] = 0x3b61;
			params->mcscc[3] = 0x7e25;

			params->mcscscale = 1;
		}
	}
	else if (videoparams_iscolorspaceconversion(params) && params->mencodingin
			== RGB)
	{
		if (params->mcolorimetry == ITU601)
		{
			params->mcsca[0] = 0x2591;
			params->mcsca[1] = 0x1322;
			params->mcsca[2] = 0x074b;
			params->mcsca[3] = 0x0000;

			params->mcscb[0] = 0x6535;
			params->mcscb[1] = 0x2000;
			params->mcscb[2] = 0x7acc;
			params->mcscb[3] = 0x0200;

			params->mcscc[0] = 0x6acd;
			params->mcscc[1] = 0x7534;
			params->mcscc[2] = 0x2000;
			params->mcscc[3] = 0x0200;

			params->mcscscale = 0;
		}
		else if (params->mcolorimetry == ITU709)
		{
			params->mcsca[0] = 0x2dc5;
			params->mcsca[1] = 0x0d9b;
			params->mcsca[2] = 0x049e;
			params->mcsca[3] = 0x0000;

			params->mcscb[0] = 0x62f0;
			params->mcscb[1] = 0x2000;
			params->mcscb[2] = 0x7d11;
			params->mcscb[3] = 0x0200;

			params->mcscc[0] = 0x6756;
			params->mcscc[1] = 0x78ab;
			params->mcscc[2] = 0x2000;
			params->mcscc[3] = 0x0200;

			params->mcscscale = 0;
		}
	}
	/* else use user coefficients */
}

