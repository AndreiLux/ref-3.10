/*
 * videoParams.h
 *
 * Synopsys Inc.
 * SG DWC PT02
 */
 /**
 * @file
 * Define video parameters structure and functions to manipulate the
 * information held by the structure.
 * Video encoding and colorimetry are also defined here
 */

#ifndef VIDEOPARAMS_H_
#define VIDEOPARAMS_H_

#include "../util/types.h"
#include "../edid/dtd.h"

typedef enum
{
	RGB = 0,
	YCC444,
	YCC422

} encoding_t;

typedef enum
{

	ITU601 = 1,
	ITU709,
	EXTENDED_COLORIMETRY

} colorimetry_t;

typedef struct
{
	u8 mhdmi;
	encoding_t mencodingout;
	encoding_t mencodingin;
	u8 mcolorresolution;
	u8 mpixelrepetitionfactor;
	dtd_t mdtd;
	u8 mrgbquantizationrange;
	u8 mpixelpackingdefaultphase;
	u8 mcolorimetry;
	u8 mscaninfo;
	u8 mactiveformataspectratio;
	u8 mnonuniformscaling;
	u8 mextcolorimetry;
	u8 mitcontent;
	u16 mendtopbar;
	u16 mstartbottombar;
	u16 mendleftbar;
	u16 mstartrightbar;
	u16 mcscfilter;
	u16 mcsca[4];
	u16 mcscc[4];
	u16 mcscb[4];
	u16 mcscscale;
	u8 mhdmivideoformat;
	u8 m3dstructure;
	u8 m3deextdata;
	u8 mhdmivic;

} videoParams_t;

/**
 * This method should be called before setting any parameters
 * to put the state of the strucutre to default
 * @param params pointer to the video parameters structure
 */
void videoparams_reset(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return active format aspect ratio code in Table 9 in CEA-D/E
 */
u8 videoparams_getactiveformataspectratio(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return colorimetry code in Table 9 in CEA-D/E
 */
u8 videoparams_getcolorimetry(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return color resolution/depth (8/10/12/16-bits per pixel)
 */
u8 videoparams_getcolorresolution(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return the colour space convertion filter
 */
u8 videoparams_getcscfilter(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return pointer to the DTD structure that describes the video output stream
 */
dtd_t* videoparams_getdtd(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return the input video encoding type
 */
encoding_t videoparams_getencodingin(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return the output video encoding type
 */
encoding_t videoparams_getencodingout(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 *  @return the pixel number of end of left bar
 */
u16 videoparams_getendleftbar(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 *  @return line number of end of top bar
 */
u16 videoparams_getendtopbar(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 *  @return the extended colorimetry code in Table 11 in CEA-D/E
 */
u8 videoparams_getextcolorimetry(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return TRUE if output stream is HDMI
 */
u8 videoparams_gethdmi(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return TRUE if stream is IT content, refer to Table 11 in CEA-D/E
 */
u8 videoparams_getitcontent(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 *  @return non uniform scaling code, refer to Table 11 in CEA-D/E
 */
u8 videoparams_getnonuniformscaling(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 *  @return default phase (0 or 1)
 */
u8 videoparams_getpixelpackingdefaultphase(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return desired pixel repetition factor
 */
u8 videoparams_getpixelrepetitionfactor(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return the quantisation range of the video stream,
 * refer to Table 11 in CEA-D/E
 */
u8 videoparams_getrgbquantizationrange(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return the scan info code, refer to Table 8 in CEA-D/E
 */
u8 videoparams_getscaninfo(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return the line number of start of bottom bar
 */
u16 videoparams_getstartbottombar(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return the pixel number of start of right bar
 */
u16 videoparams_getstartrightbar(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return custom csc scale
 */
u16 videoparams_getcscscale(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @param value active format aspect ratio code in Table 9 in CEA-D/E
 */
void videoparams_setactiveformataspectratio(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @param value colorimetry code in Table 9 in CEA-D/E
 */
void videoparams_setcolorimetry(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @param value color resolution/depth (8/10/12/16-bits per pixel)
 */
void videoparams_setcolorresolution(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @param value colour space convertion chroma filter
 * check Color Space Converter configuration in the
 * HDMI Transmitter Controller Databook
 */
void videoparams_setcscfilter(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @param dtd pointer to dtd_t structure describing the video output stream
 */
void videoparams_setdtd(videoParams_t *params, dtd_t *dtd);
/**
 * @param params pointer to the video parameters structure
 * @param value of thie input video one of enumurators:
 * RGB/YCC444/YCC422 of type encoding_t
 */
void videoparams_setencodingin(videoParams_t *params, encoding_t value);
/**
 * @param params pointer to the video parameters structure
 * @param value one of enumurators: RGB/YCC444/YCC422 of type encoding_t
 */
void videoparams_setencodingout(videoParams_t *params, encoding_t value);
/**
 * @param params pointer to the video parameters structure
 * @param value
 */
void videoparams_setendleftbar(videoParams_t *params, u16 value);
/**
 * @param params pointer to the video parameters structure
 * @param value
 */
void videoparams_setendtopbar(videoParams_t *params, u16 value);
/**
 * @param params pointer to the video parameters structure
 * @param value extended colorimetry code in Table 11 in CEA-D/E
 */
void videoparams_setextcolorimetry(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @param value HDMI TRUE, DVI FALSE
 */
void videoparams_sethdmi(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @param value
 */
void videoparams_setitcontent(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @param value
 */
void videoparams_setnonuniformscaling(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @param value
 */
void videoparams_setpixelpackingdefaultphase(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @param value
 */
void videoparams_setpixelrepetitionfactor(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @param value
 */
void videoparams_setrgbquantizationrange(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @param value
 */
void videoparams_setscaninfo(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @param value
 */
void videoparams_setstartbottombar(videoParams_t *params, u16 value);
/**
 * @param params pointer to the video parameters structure
 * @param value
 */
void videoparams_setstartrightbar(videoParams_t *params, u16 value);
/**
 * @param params pointer to the video parameters structure
 * @return the custom csc coefficients A
 */
u16 * videoparams_getcsca(videoParams_t *params);

void videoparams_setcsca(videoParams_t *params, u16 value[4]);
/**
 * @param params pointer to the video parameters structure
 * @return the custom csc coefficients B
 */
u16 * videoparams_getcscb(videoParams_t *params);

void videoparams_setcscb(videoParams_t *params, u16 value[4]);
/**
 * @param params pointer to the video parameters structure
 * @return the custom csc coefficients C
 */
u16 * videoparams_getcscc(videoParams_t *params);

void videoparams_setcscc(videoParams_t *params, u16 value[4]);

void videoparams_setcscscale(videoParams_t *params, u16 value);
/**
 * @param params pointer to the video parameters structure
 * @return Video PixelClock in [0.01 MHz]
 */
u16 videoparams_getpixelclock(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return TMDS Clock in [0.01 MHz]
 */
u16 videoparams_gettmdsclock(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return Ration clock x 100 (should be multiplied by x 0.01 afterwards)
 */
unsigned videoparams_getratioclock(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return TRUE if csc is needed
 */
int videoparams_iscolorspaceconversion(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return TRUE if color space decimation is needed
 */
int videoparams_iscolorspacedecimation(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return TRUE if if video is interpolated
 */
int videoparams_iscolorspaceinterpolation(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 * @return TRUE if if video has pixel repetition
 */
int videoparams_ispixelrepetition(videoParams_t *params);
/**
 * @param params pointer to the video parameters structure
 *  @return non uniform scaling code, refer to Table 11 in CEA-D/E
 */
u8 videoparams_gethdmivideoformat(videoParams_t *params);

void videoparams_sethdmivideoformat(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @return non uniform scaling code, refer to Table 11 in CEA-D/E
 */
u8 videoparams_get3dstructure(videoParams_t *params);

void videoparams_set3dstructure(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @return non uniform scaling code, refer to Table 11 in CEA-D/E
 */
u8 videoparams_get3dextdata(videoParams_t *params);

void videoparams_set3dextdata(videoParams_t *params, u8 value);
/**
 * @param params pointer to the video parameters structure
 * @return HDMI Video Format Identification Code
 * (CEA-D/E and HDMI 1.4 Table 8-14)
 */
u8 videoparams_gethdmivic(videoParams_t *params);

void videoparams_sethdmivic(videoParams_t *params, u8 value);

void videoparams_updatecsccoefficients(videoParams_t *params);

#endif /* VIDEOPARAMS_H_ */
