/*
 * @file:dtd.c
 *
 *
 * Synopsys Inc.
 * SG DWC PT02
 */
#include <linux/kernel.h>

#include "dtd.h"
#include "../util/log.h"
#include "../util/bitOperation.h"
#include "../util/error.h"

#define SLIMPORT_USE_CASE

int dtd_parse(dtd_t *dtd, u8 data[18])
{
	LOG_TRACE();

	dtd->mcode = -1;
	dtd->mpixelrepetitioninput = 0;

	dtd->mpixelclock = bitoperation_bytes2word(data[1], data[0]); /*  [10000Hz] */
	if (dtd->mpixelclock < 0x01)
	{ /* 0x0000 is defined as reserved */
		return FALSE;
	}

	dtd->mhactive = bitoperation_concatbits(data[4], 4, 4, data[2], 0, 8);
	dtd->mhblanking = bitoperation_concatbits(data[4], 0, 4, data[3], 0, 8);
	dtd->mhsyncoffset = bitoperation_concatbits(data[11], 6, 2, data[8], 0, 8);
	dtd->mhsyncpulsewidth = bitoperation_concatbits(data[11], 4, 2, data[9], 0,
			8);
	dtd->mhimagesize = bitoperation_concatbits(data[14], 4, 4, data[12], 0, 8);
	dtd->mhborder = data[15];

	dtd->mvactive = bitoperation_concatbits(data[7], 4, 4, data[5], 0, 8);
	dtd->mvblanking = bitoperation_concatbits(data[7], 0, 4, data[6], 0, 8);
	dtd->mvsyncoffset = bitoperation_concatbits(data[11], 2, 2, data[10], 4, 4);
	dtd->mvsyncpulsewidth = bitoperation_concatbits(data[11], 0, 2, data[10],
			0, 4);
	dtd->mvimagesize = bitoperation_concatbits(data[14], 0, 4, data[13], 0, 8);
	dtd->mvborder = data[16];

	if (bitoperation_bitfield(data[17], 4, 1) != 1)
	{ /* if not DIGITAL SYNC SIGNAL DEF */
		LOG_WARNING("Invalid DTD Parameters");
		return FALSE;
	}
	if (bitoperation_bitfield(data[17], 3, 1) != 1)
	{ /* if not DIGITAL SEPATATE SYNC */
		LOG_WARNING("Invalid DTD Parameters");
		return FALSE;
	}
	/* no stereo viewing support in HDMI */
	dtd->minterlaced = bitoperation_bitfield(data[17], 7, 1) == 1;
	dtd->mvsyncpolarity = bitoperation_bitfield(data[17], 2, 1) == 1;
	dtd->mhsyncpolarity = bitoperation_bitfield(data[17], 1, 1) == 1;
	return TRUE;
}
int dtd_fill(dtd_t *dtd, u8 code, u32 refreshRate)
{
	LOG_TRACE();
	dtd->mcode = code;
	dtd->mhborder = 0;
	dtd->mvborder = 0;
	dtd->mpixelrepetitioninput = 0;
	dtd->mhimagesize = 16;
	dtd->mvimagesize = 9;


	switch (code)
	{
		case 1: /* 640x480p @ 59.94/60Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
			dtd->mhactive = 640;
			dtd->mvactive = 480;
			dtd->mhblanking = 160;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 16;
			dtd->mvsyncoffset = 10;
			dtd->mhsyncpulsewidth = 96;
			dtd->mvsyncpulsewidth = 2;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0; /* not(progressive_nI) */
			dtd->mpixelclock = (refreshRate == 59940) ? 2517 : 2520;
			break;
		case 2: /* 720x480p @ 59.94/60Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 3: /* 720x480p @ 59.94/60Hz 16:9 */
			dtd->mhactive = 720;
			dtd->mvactive = 480;
			dtd->mhblanking = 138;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 16;
			dtd->mvsyncoffset = 9;
			dtd->mhsyncpulsewidth = 62;
			dtd->mvsyncpulsewidth = 6;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 59940) ? 2700 : 2702;
			break;
		case 4: /* 1280x720p @ 59.94/60Hz 16:9 */
			dtd->mhactive = 1280;
			dtd->mvactive = 720;
			dtd->mhblanking = 370;
			dtd->mvblanking = 30;
			dtd->mhsyncoffset = 110;
			dtd->mvsyncoffset = 5;
			dtd->mhsyncpulsewidth = 41;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 59940) ? 7417 : 7425;
			break;
		case 5: /* 1920x1080i @ 59.94/60Hz 16:9 */
			dtd->mhactive = 1920;
			dtd->mvactive = 540;
			dtd->mhblanking = 280;
			dtd->mvblanking = 22;
			dtd->mhsyncoffset = 88;
			dtd->mvsyncoffset = 2;
			dtd->mhsyncpulsewidth = 44;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 1;
			dtd->mpixelclock = (refreshRate == 59940) ? 7417 : 7425;
			break;
		case 6: /* 720(1440)x480i @ 59.94/60Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 7: /* 720(1440)x480i @ 59.94/60Hz 16:9 */
			dtd->mhactive = 1440;
			dtd->mvactive = 240;
			dtd->mhblanking = 276;
			dtd->mvblanking = 22;
			dtd->mhsyncoffset = 38;
			dtd->mvsyncoffset = 4;
			dtd->mhsyncpulsewidth = 124;
			dtd->mvsyncpulsewidth = 3;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 1;
			dtd->mpixelclock = (refreshRate == 59940) ? 2700 : 2702;
			dtd->mpixelrepetitioninput = 1;
			break;
		case 8: /* 720(1440)x240p @ 59.826/60.054/59.886/60.115Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 9: /* 720(1440)x240p @ 59.826/60.054/59.886/60.115Hz 16:9 */
			dtd->mhactive = 1440;
			dtd->mvactive = 240;
			dtd->mhblanking = 276;
			dtd->mvblanking = (refreshRate > 60000) ? 22 : 23;
			dtd->mhsyncoffset = 38;
			dtd->mvsyncoffset = (refreshRate > 60000) ? 4 : 5;
			dtd->mhsyncpulsewidth = 124;
			dtd->mvsyncpulsewidth = 3;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock
					= ((refreshRate == 60054) || refreshRate == 59826) ? 2700
							: 2702; /*  else 60.115/59.886 Hz */
			dtd->mpixelrepetitioninput = 1;
			break;
		case 10: /* 2880x480i @ 59.94/60Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 11: /* 2880x480i @ 59.94/60Hz 16:9 */
			dtd->mhactive = 2880;
			dtd->mvactive = 240;
			dtd->mhblanking = 552;
			dtd->mvblanking = 22;
			dtd->mhsyncoffset = 76;
			dtd->mvsyncoffset = 4;
			dtd->mhsyncpulsewidth = 248;
			dtd->mvsyncpulsewidth = 3;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 1;
			dtd->mpixelclock = (refreshRate == 59940) ? 5400 : 5405;
			break;
		case 12: /* 2880x240p @ 59.826/60.054/59.886/60.115Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 13: /* 2880x240p @ 59.826/60.054/59.886/60.115Hz 16:9 */
			dtd->mhactive = 2880;
			dtd->mvactive = 240;
			dtd->mhblanking = 552;
			dtd->mvblanking = (refreshRate > 60000) ? 22 : 23;
			dtd->mhsyncoffset = 76;
			dtd->mvsyncoffset = (refreshRate > 60000) ? 4 : 5;
			dtd->mhsyncpulsewidth = 248;
			dtd->mvsyncpulsewidth = 3;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock
					= ((refreshRate == 60054) || refreshRate == 59826) ? 5400
							: 5405; /*  else 60.115/59.886 Hz */
			break;
		case 14: /* 1440x480p @ 59.94/60Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 15: /* 1440x480p @ 59.94/60Hz 16:9 */
			dtd->mhactive = 1440;
			dtd->mvactive = 480;
			dtd->mhblanking = 276;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 32;
			dtd->mvsyncoffset = 9;
			dtd->mhsyncpulsewidth = 124;
			dtd->mvsyncpulsewidth = 6;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 59940) ? 5400 : 5405;
			break;
		case 16: /* 1920x1080p @ 59.94/60Hz 16:9 */
			dtd->mhactive = 1920;
			dtd->mvactive = 1080;
			dtd->mhblanking = 280;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 88;
			dtd->mvsyncoffset = 4;
			dtd->mhsyncpulsewidth = 44;
			dtd->mvsyncpulsewidth = 5;
/* need syncpolarity change test */
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 59940) ? 14835 : 14850;
			break;
		case 17: /* 720x576p @ 50Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 18: /* 720x576p @ 50Hz 16:9 */
			dtd->mhactive = 720;
			dtd->mvactive = 576;
			dtd->mhblanking = 144;
			dtd->mvblanking = 49;
			dtd->mhsyncoffset = 12;
			dtd->mvsyncoffset = 5;
			dtd->mhsyncpulsewidth = 64;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 2700;
			break;
		case 19: /* 1280x720p @ 50Hz 16:9 */
			dtd->mhactive = 1280;
			dtd->mvactive = 720;
			dtd->mhblanking = 700;
			dtd->mvblanking = 30;
			dtd->mhsyncoffset = 440;
			dtd->mvsyncoffset = 5;
			dtd->mhsyncpulsewidth = 40;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 7425;
			break;
		case 20: /* 1920x1080i @ 50Hz 16:9 */
			dtd->mhactive = 1920;
			dtd->mvactive = 540;
			dtd->mhblanking = 720;
			dtd->mvblanking = 22;
			dtd->mhsyncoffset = 528;
			dtd->mvsyncoffset = 2;
			dtd->mhsyncpulsewidth = 44;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 1;
			dtd->mpixelclock = 7425;
			break;
		case 21: /* 720(1440)x576i @ 50Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 22: /* 720(1440)x576i @ 50Hz 16:9 */
			dtd->mhactive = 1440;
			dtd->mvactive = 288;
			dtd->mhblanking = 288;
			dtd->mvblanking = 24;
			dtd->mhsyncoffset = 24;
			dtd->mvsyncoffset = 2;
			dtd->mhsyncpulsewidth = 126;
			dtd->mvsyncpulsewidth = 3;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 1;
			dtd->mpixelclock = 2700;
			dtd->mpixelrepetitioninput = 1;
			break;
		case 23: /* 720(1440)x288p @ 50Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 24: /* 720(1440)x288p @ 50Hz 16:9 */
			dtd->mhactive = 1440;
			dtd->mvactive = 288;
			dtd->mhblanking = 288;
			dtd->mvblanking = (refreshRate == 50080) ? 24
					: ((refreshRate == 49920) ? 25 : 26);
			dtd->mhsyncoffset = 24;
			dtd->mvsyncoffset = (refreshRate == 50080) ? 2
					: ((refreshRate == 49920) ? 3 : 4);
			dtd->mhsyncpulsewidth = 126;
			dtd->mvsyncpulsewidth = 3;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 2700;
			dtd->mpixelrepetitioninput = 1;
			break;
		case 25: /* 2880x576i @ 50Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 26: /* 2880x576i @ 50Hz 16:9 */
			dtd->mhactive = 2880;
			dtd->mvactive = 288;
			dtd->mhblanking = 576;
			dtd->mvblanking = 24;
			dtd->mhsyncoffset = 48;
			dtd->mvsyncoffset = 2;
			dtd->mhsyncpulsewidth = 252;
			dtd->mvsyncpulsewidth = 3;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 1;
			dtd->mpixelclock = 5400;
			break;
		case 27: /* 2880x288p @ 50Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 28: /* 2880x288p @ 50Hz 16:9 */
			dtd->mhactive = 2880;
			dtd->mvactive = 288;
			dtd->mhblanking = 576;
			dtd->mvblanking = (refreshRate == 50080) ? 24
					: ((refreshRate == 49920) ? 25 : 26);
			dtd->mhsyncoffset = 48;
			dtd->mvsyncoffset = (refreshRate == 50080) ? 2
					: ((refreshRate == 49920) ? 3 : 4);
			dtd->mhsyncpulsewidth = 252;
			dtd->mvsyncpulsewidth = 3;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 5400;
			break;
		case 29: /* 1440x576p @ 50Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 30: /* 1440x576p @ 50Hz 16:9 */
			dtd->mhactive = 1440;
			dtd->mvactive = 576;
			dtd->mhblanking = 288;
			dtd->mvblanking = 49;
			dtd->mhsyncoffset = 24;
			dtd->mvsyncoffset = 5;
			dtd->mhsyncpulsewidth = 128;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 5400;
			break;
		case 31: /* 1920x1080p @ 50Hz 16:9 */
			dtd->mhactive = 1920;
			dtd->mvactive = 1080;
			dtd->mhblanking = 720;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 528;
			dtd->mvsyncoffset = 4;
			dtd->mhsyncpulsewidth = 44;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 14850;
			break;
		case 32: /* 1920x1080p @ 23.976/24Hz 16:9 */
			dtd->mhactive = 1920;
			dtd->mvactive = 1080;
			dtd->mhblanking = 830;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 638;
			dtd->mvsyncoffset = 4;
			dtd->mhsyncpulsewidth = 44;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 23976) ? 7417 : 7425;
			break;
		case 33: /* 1920x1080p @ 25Hz 16:9 */
			dtd->mhactive = 1920;
			dtd->mvactive = 1080;
			dtd->mhblanking = 720;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 528;
			dtd->mvsyncoffset = 4;
			dtd->mhsyncpulsewidth = 44;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 7425;
			break;
		case 34: /* 1920x1080p @ 29.97/30Hz 16:9 */
			dtd->mhactive = 1920;
			dtd->mvactive = 1080;
			dtd->mhblanking = 280;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 88;
			dtd->mvsyncoffset = 4;
			dtd->mhsyncpulsewidth = 44;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 29970) ? 7417 : 7425;
			break;
		case 35: /* 2880x480p @ 60Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 36: /* 2880x480p @ 60Hz 16:9 */
			dtd->mhactive = 2880;
			dtd->mvactive = 480;
			dtd->mhblanking = 552;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 64;
			dtd->mvsyncoffset = 9;
			dtd->mhsyncpulsewidth = 248;
			dtd->mvsyncpulsewidth = 6;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 59940) ? 10800 : 10810;
			break;
		case 37: /* 2880x576p @ 50Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 38: /* 2880x576p @ 50Hz 16:9 */
			dtd->mhactive = 2880;
			dtd->mvactive = 576;
			dtd->mhblanking = 576;
			dtd->mvblanking = 49;
			dtd->mhsyncoffset = 48;
			dtd->mvsyncoffset = 5;
			dtd->mhsyncpulsewidth = 256;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 10800;
			break;
		case 39: /* 1920x1080i (1250 total) @ 50Hz 16:9 */
			dtd->mhactive = 1920;
			dtd->mvactive = 540;
			dtd->mhblanking = 384;
			dtd->mvblanking = 85;
			dtd->mhsyncoffset = 32;
			dtd->mvsyncoffset = 23;
			dtd->mhsyncpulsewidth = 168;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 1;
			dtd->mpixelclock = 7200;
			break;
		case 40: /* 1920x1080i @ 100Hz 16:9 */
			dtd->mhactive = 1920;
			dtd->mvactive = 540;
			dtd->mhblanking = 720;
			dtd->mvblanking = 22;
			dtd->mhsyncoffset = 528;
			dtd->mvsyncoffset = 2;
			dtd->mhsyncpulsewidth = 44;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 1;
			dtd->mpixelclock = 14850;
			break;
		case 41: /* 1280x720p @ 100Hz 16:9 */
			dtd->mhactive = 1280;
			dtd->mvactive = 720;
			dtd->mhblanking = 700;
			dtd->mvblanking = 30;
			dtd->mhsyncoffset = 440;
			dtd->mvsyncoffset = 5;
			dtd->mhsyncpulsewidth = 40;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 14850;
			break;
		case 42: /* 720x576p @ 100Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 43: /* 720x576p @ 100Hz 16:9 */
			dtd->mhactive = 720;
			dtd->mvactive = 576;
			dtd->mhblanking = 144;
			dtd->mvblanking = 49;
			dtd->mhsyncoffset = 12;
			dtd->mvsyncoffset = 5;
			dtd->mhsyncpulsewidth = 64;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 5400;
			break;
		case 44: /* 720(1440)x576i @ 100Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 45: /* 720(1440)x576i @ 100Hz 16:9 */
			dtd->mhactive = 1440;
			dtd->mvactive = 288;
			dtd->mhblanking = 288;
			dtd->mvblanking = 24;
			dtd->mhsyncoffset = 24;
			dtd->mvsyncoffset = 2;
			dtd->mhsyncpulsewidth = 126;
			dtd->mvsyncpulsewidth = 3;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 1;
			dtd->mpixelclock = 5400;
			dtd->mpixelrepetitioninput = 1;
			break;
		case 46: /* 1920x1080i @ 119.88/120Hz 16:9 */
			dtd->mhactive = 1920;
			dtd->mvactive = 540;
			dtd->mhblanking = 288;
			dtd->mvblanking = 22;
			dtd->mhsyncoffset = 88;
			dtd->mvsyncoffset = 2;
			dtd->mhsyncpulsewidth = 44;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 1;
			dtd->mpixelclock = (refreshRate == 119880) ? 14835 : 14850;
			break;
		case 47: /* 1280x720p @ 119.88/120Hz 16:9 */
			dtd->mhactive = 1280;
			dtd->mvactive = 720;
			dtd->mhblanking = 370;
			dtd->mvblanking = 30;
			dtd->mhsyncoffset = 110;
			dtd->mvsyncoffset = 5;
			dtd->mhsyncpulsewidth = 40;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 119880) ? 14835 : 14850;
			break;
		case 48: /* 720x480p @ 119.88/120Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 49: /* 720x480p @ 119.88/120Hz 16:9 */
			dtd->mhactive = 720;
			dtd->mvactive = 480;
			dtd->mhblanking = 138;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 16;
			dtd->mvsyncoffset = 9;
			dtd->mhsyncpulsewidth = 62;
			dtd->mvsyncpulsewidth = 6;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 119880) ? 5400 : 5405;
			break;
		case 50: /* 720(1440)x480i @ 119.88/120Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 51: /* 720(1440)x480i @ 119.88/120Hz 16:9 */
			dtd->mhactive = 1440;
			dtd->mvactive = 240;
			dtd->mhblanking = 276;
			dtd->mvblanking = 22;
			dtd->mhsyncoffset = 38;
			dtd->mvsyncoffset = 4;
			dtd->mhsyncpulsewidth = 124;
			dtd->mvsyncpulsewidth = 3;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 1;
			dtd->mpixelclock = (refreshRate == 119880) ? 5400 : 5405;
			dtd->mpixelrepetitioninput = 1;
			break;
		case 52: /* 720X576p @ 200Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 53: /* 720X576p @ 200Hz 16:9 */
			dtd->mhactive = 720;
			dtd->mvactive = 576;
			dtd->mhblanking = 144;
			dtd->mvblanking = 49;
			dtd->mhsyncoffset = 12;
			dtd->mvsyncoffset = 5;
			dtd->mhsyncpulsewidth = 64;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 10800;
			break;
		case 54: /* 720(1440)x576i @ 200Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 55: /* 720(1440)x576i @ 200Hz 16:9 */
			dtd->mhactive = 1440;
			dtd->mvactive = 288;
			dtd->mhblanking = 288;
			dtd->mvblanking = 24;
			dtd->mhsyncoffset = 24;
			dtd->mvsyncoffset = 2;
			dtd->mhsyncpulsewidth = 126;
			dtd->mvsyncpulsewidth = 3;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 1;
			dtd->mpixelclock = 10800;
			dtd->mpixelrepetitioninput = 1;
			break;
		case 56: /* 720x480p @ 239.76/240Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 57: /* 720x480p @ 239.76/240Hz 16:9 */
			dtd->mhactive = 720;
			dtd->mvactive = 480;
			dtd->mhblanking = 138;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 16;
			dtd->mvsyncoffset = 9;
			dtd->mhsyncpulsewidth = 62;
			dtd->mvsyncpulsewidth = 6;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 239760) ? 10800 : 10810;
			break;
		case 58: /* 720(1440)x480i @ 239.76/240Hz 4:3 */
			dtd->mhimagesize = 4;
			dtd->mvimagesize = 3;
		case 59: /* 720(1440)x480i @ 239.76/240Hz 16:9 */
			dtd->mhactive = 1440;
			dtd->mvactive = 240;
			dtd->mhblanking = 276;
			dtd->mvblanking = 22;
			dtd->mhsyncoffset = 38;
			dtd->mvsyncoffset = 4;
			dtd->mhsyncpulsewidth = 124;
			dtd->mvsyncpulsewidth = 3;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 0;
			dtd->minterlaced = 1;
			dtd->mpixelclock = (refreshRate == 239760) ? 10800 : 10810;
			dtd->mpixelrepetitioninput = 1;
			break;
		case 60: /* 1280x720p @ 23.97/24Hz 16:9 */
			dtd->mhactive = 1280;
			dtd->mvactive = 720;
			dtd->mhblanking = 2020;
			dtd->mvblanking = 30;
			dtd->mhsyncoffset = 1760;
			dtd->mvsyncoffset = 5;
			dtd->mhsyncpulsewidth = 40;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 23970) ? 5934 : 5940;
			break;
		case 61: /* 1280x720p @ 25Hz 16:9 */
			dtd->mhactive = 1280;
			dtd->mvactive = 720;
			dtd->mhblanking = 2680;
			dtd->mvblanking = 30;
			dtd->mhsyncoffset = 2420;
			dtd->mvsyncoffset = 5;
			dtd->mhsyncpulsewidth = 40;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 7425;
			break;
		case 62: /* 1280x720p @ 29.97/30Hz  16:9 */
			dtd->mhactive = 1280;
			dtd->mvactive = 720;
			dtd->mhblanking = 2020;
			dtd->mvblanking = 30;
			dtd->mhsyncoffset = 1760;
			dtd->mvsyncoffset = 5;
			dtd->mhsyncpulsewidth = 40;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 29970) ? 7417: 7425;
			break;
		case 63: /* 1920x1080p @ 119.88/120Hz 16:9 */
			dtd->mhactive = 1920;
			dtd->mvactive = 1080;
			dtd->mhblanking = 280;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 88;
			dtd->mvsyncoffset = 4;
			dtd->mhsyncpulsewidth = 44;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = (refreshRate == 119880) ?  29670: 29700;
			break;
		case 64: /* 1920x1080p @ 100Hz 16:9 */
			dtd->mhactive = 1920;
			dtd->mvactive = 1080;
			dtd->mhblanking = 720;
			dtd->mvblanking = 45;
			dtd->mhsyncoffset = 528;
			dtd->mvsyncoffset = 4;
			dtd->mhsyncpulsewidth = 44;
			dtd->mvsyncpulsewidth = 5;
			dtd->mhsyncpolarity = dtd->mvsyncpolarity = 1;
			dtd->minterlaced = 0;
			dtd->mpixelclock = 29700;
			break;
	default:
		dtd->mcode = -1;
		error_set(ERR_DTD_INVALID_CODE);
		LOG_ERROR("invalid code");
		return FALSE;
	}

	return TRUE;
}

u8 dtd_getcode(const dtd_t *dtd)
{
	return dtd->mcode;
}

u16 dtd_getpixelrepetitioninput(const dtd_t *dtd)
{
	return dtd->mpixelrepetitioninput;
}

u16 dtd_getpixelclock(const dtd_t *dtd)
{
	return dtd->mpixelclock;
}

u8 dtd_getinterlaced(const dtd_t *dtd)
{
	return dtd->minterlaced;
}

u16 dtd_gethactive(const dtd_t *dtd)
{
	return dtd->mhactive;
}

u16 dtd_gethblanking(const dtd_t *dtd)
{
	return dtd->mhblanking;
}

u16 dtd_gethborder(const dtd_t *dtd)
{
	return dtd->mhborder;
}

u16 dtd_gethimagesize(const dtd_t *dtd)
{
	return dtd->mhimagesize;
}

u16 dtd_gethsyncoffset(const dtd_t *dtd)
{
	return dtd->mhsyncoffset;
}

u8 dtd_gethsyncpolarity(const dtd_t *dtd)
{
	return dtd->mhsyncpolarity;
}

u16 dtd_gethsyncpulsewidth(const dtd_t *dtd)
{
	return dtd->mhsyncpulsewidth;
}

u16 dtd_getvactive(const dtd_t *dtd)
{
	return dtd->mvactive;
}

u16 dtd_getvblanking(const dtd_t *dtd)
{
	return dtd->mvblanking;
}

u16 dtd_getvborder(const dtd_t *dtd)
{
	return dtd->mvborder;
}

u16 dtd_getvimagesize(const dtd_t *dtd)
{
	return dtd->mvimagesize;
}

u16 dtd_getvsyncoffset(const dtd_t *dtd)
{
	return dtd->mvsyncoffset;
}

u8 dtd_getvsyncpolarity(const dtd_t *dtd)
{
	return dtd->mvsyncpolarity;
}

u16 dtd_getvsyncpulsewidth(const dtd_t *dtd)
{
	return dtd->mvsyncpulsewidth;
}

int dtd_isequal(const dtd_t *dtd1, const dtd_t *dtd2)
{
	return (dtd1->minterlaced == dtd2->minterlaced && dtd1->mhactive
			== dtd2->mhactive && dtd1->mhblanking == dtd2->mhblanking
			&& dtd1->mhborder == dtd2->mhborder && dtd1->mhsyncoffset
			== dtd2->mhsyncoffset && dtd1->mhsyncpolarity
			== dtd2->mhsyncpolarity && dtd1->mhsyncpulsewidth
			== dtd2->mhsyncpulsewidth && dtd1->mvactive == dtd2->mvactive
			&& dtd1->mvblanking == dtd2->mvblanking && dtd1->mvborder
			== dtd2->mvborder && dtd1->mvsyncoffset == dtd2->mvsyncoffset
			&& dtd1->mvsyncpolarity == dtd2->mvsyncpolarity
			&& dtd1->mvsyncpulsewidth == dtd2->mvsyncpulsewidth
			&& ((dtd1->mhimagesize * 10) / dtd1->mvimagesize)
					== ((dtd2->mhimagesize * 10) / dtd2->mvimagesize));
}

int dtd_setpixelrepetitioninput(dtd_t *dtd, u16 value)
{
	LOG_TRACE();

	switch (dtd->mcode)
	{
	case (u8)-1:
	case 10:
	case 11:
	case 12:
	case 13:
	case 25:
	case 26:
	case 27:
	case 28:
		if (value < 10)
		{
			dtd->mpixelrepetitioninput = value;
			return TRUE;
		}
		break;
	case 14:
	case 15:
	case 29:
	case 30:
		if (value < 2)
		{
			dtd->mpixelrepetitioninput = value;
			return TRUE;
		}
		break;
	case 35:
	case 36:
	case 37:
	case 38:
		if (value < 2 || value == 3)
		{
			dtd->mpixelrepetitioninput = value;
			return TRUE;
		}
		break;
	default:
		if (value == dtd->mpixelrepetitioninput)
		{
			return TRUE;
		}
		break;
	}
	error_set(ERR_PIXEL_REPETITION_FOR_VIDEO_MODE);
	LOG_ERROR3("Invalid pixel repetition input for video mode", value, dtd->mcode);
	return FALSE;
}
