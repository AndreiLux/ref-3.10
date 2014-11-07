/*
 * linux/drivers/video/odin/hdmi/compliance.c
 *
 * Copyright (C) 2012 LG Electronics
 * Author:
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define DSS_SUBSYS_NAME "HDMI_COMPLIANCE"

#include <linux/slab.h>
#include "compliance.h"
#include "../hdmi_api_tx/api/api.h"
#include "../hdmi_api_tx/util/log.h"
#include "../dss/dss.h"
#include "../display/hdmi-panel.h"
#include <linux/slimport.h>

#include "hdmi_debug.h"






static videoParams_t *pvideo = NULL;
static audioParams_t *paudio = NULL;
static hdcpParams_t *phdcp = NULL;
static productParams_t *pproduct = NULL;
static u8 currentmode = 0;

shortVideoDesc_t current_svd[HDMI_VFRMT_MAX];

struct resolution_key {
	u32 w;
	u32 h;
	u32 fs;
	u32 num;
};



int compliance_standby(void)
{
	if (paudio != 0)
	{
		kfree(paudio);
		paudio = 0;
	}
	if (pproduct != 0)
	{
		kfree(pproduct);
		pproduct = 0;
	}
	if (pvideo != 0)
	{
		kfree(pvideo);
		pvideo = 0;
	}

	compliance_hdcp(false);
	currentmode = 0;
	return TRUE;
}

int compliance_hdcp(int on)
{
	if (on)
	{
		if (phdcp == NULL)
		{
			phdcp = kzalloc (sizeof(hdcpParams_t), GFP_KERNEL);
		}
		if (phdcp == NULL)
		{
			hdmi_err("cannot allocate memory");
			return FALSE;
		}
		else
		{
			hdcpparams_reset(phdcp);
		}
		hdmi_debug(LOG_DEBUG,"HDCP ON");
	}
	else
	{
		if (phdcp != NULL)
		{
			kfree(phdcp);
			phdcp = NULL;
		}
		hdmi_debug(LOG_DEBUG,"HDCP OFF");
	}
	return TRUE;
}
int compliance_init(void)
{
	int ret;
	const u8 vname[] = "LGE";
	const u8 pname[] = "Complnce";
	if (pproduct == NULL)
	{
		pproduct = kzalloc(sizeof(productParams_t), GFP_KERNEL);
	}
	if (pproduct == NULL)
	{
		hdmi_err("cannot allocate memory");
		return FALSE;
	}
	else
	{
		productparams_reset(pproduct);
		productparams_setvendorname(pproduct, vname,sizeof(vname) - 1);
		productparams_setproductname(pproduct, pname,sizeof(pname) - 1);
		productparams_setsourcetype(pproduct, 0x0A);
	}
	if (paudio == NULL)
	{
		paudio = kzalloc(sizeof(audioParams_t), GFP_KERNEL);
	}
	if (paudio == NULL)
	{
		hdmi_err("cannot allocate memory");
		return FALSE;
	}
	else
	{
		audioparams_reset(paudio);
	}
	if (pvideo == NULL)
	{
		pvideo = kzalloc(sizeof(videoParams_t), GFP_KERNEL);
	}
	if (pvideo == NULL)
	{
		hdmi_err("cannot allocate memory");
		return FALSE;
	}
	else
	{
		videoparams_reset(pvideo);
		videoparams_setencodingin(pvideo, RGB);
		videoparams_setencodingout(pvideo, RGB);
	}

	ret = compliance_hdcp(true);
	if(ret != true)
		return FALSE;
	return TRUE;
}

void compliance_displayformat(void)
{
}

u8 cliedid_getdtdcode(const dtd_t *dtd)
{
	u8 cea_mode = dtd_getcode(dtd);
	int ret=0;
	dtd_t tmp_dtd;
	if (cea_mode == (u8)(-1))
	{
		for (cea_mode = 1; cea_mode < 60; cea_mode++)
		{
			ret = dtd_fill(&tmp_dtd, cea_mode, 0);
	//		hdmi_debug(LOG_DEBUG, "get dtd code = %d, ret = %d\n", cea_mode, ret);
			if(ret == FALSE)
			{
				ret =  -1;
				break;
			}
			if (dtd_isequal(dtd, &tmp_dtd))
			{
	//			hdmi_debug(LOG_DEBUG, "equal code = %d\n", cea_mode);
				ret = cea_mode;
				break;
			}
		}
		if (cea_mode >= 60)
		{
			ret =  -1;
		}
	}
	return ret;
}




int compliance_configure(u8 fixed_color_screen)
{
	hdmivsdb_t vsdb;
	shortVideoDesc_t tmp_svd;
	dtd_t tmp_dtd;
	u8 ceacode = 0;
	u8 errorcode = NO_ERROR;
	int ret;
	/* needed to make sure it doesnt change while in this function*/
	u8 svdno = api_edidsvdcount();
	u8 dtdno = api_ediddtdcount();


	hdmi_debug(LOG_DEBUG,"svdno = %d, dtdno = %d\n", svdno, dtdno);

	if (api_edidhdmivsdb(&vsdb) == TRUE || is_slimport_dp())
	{
		hdmi_debug(LOG_DEBUG,"HDMI MODE\n");
		if(pvideo == NULL || paudio == NULL)
		{
			hdmi_err("pvideo, paudio is NULL\n");
			return FALSE;
		}
		videoparams_sethdmi(pvideo, TRUE);
		if(hdmivsdb_getdeepcolory444(&vsdb))
		{
			videoparams_setcolorresolution(pvideo, 8);
		}
		else if (hdmivsdb_getdeepcolor30(&vsdb))
		{
			videoparams_setcolorresolution(pvideo, 10);
		}
		else if (hdmivsdb_getdeepcolor36(&vsdb))
		{
			videoparams_setcolorresolution(pvideo, 12);
		}
		else if (hdmivsdb_getdeepcolor48(&vsdb))
		{
			videoparams_setcolorresolution(pvideo, 16);
		}
		else
		{
			videoparams_setcolorresolution(pvideo, 8);
		}

		if (currentmode >= (dtdno + svdno))
		{
			currentmode = 0;
		}

		if (currentmode < dtdno)
		{
			api_ediddtd(currentmode, &tmp_dtd);
			videoparams_setdtd(pvideo, &tmp_dtd);
		}
		else if (currentmode < svdno+dtdno)
		{
			tmp_svd = current_svd[currentmode-dtdno];
#if 0
			if (api_edidsvd(currentmode-dtdno, &tmp_svd))
#endif
			{
				ceacode = shortvideodesc_getcode(&tmp_svd);

				if (board_supportedrefreshrate(ceacode) != -1)
				{
					dtd_fill(&tmp_dtd, ceacode, board_supportedrefreshrate(ceacode));
					videoparams_setdtd(pvideo, &tmp_dtd);
				}
				else
				{
					hdmi_err("CEA mode not supported %d", ceacode);
				}
			}
		}
		else
		{
			hdmi_err("wrong currentmode:%d", currentmode);
		}

		ret = api_configure(pvideo, paudio, pproduct, phdcp, fixed_color_screen);
		if(ret != TRUE)
			return FALSE;

	}
	else
	{

		hdmi_debug(LOG_DEBUG,"DTD MODE\n");
		if(pvideo == NULL)
		{
			hdmi_err("DTD MODE\n");
			return FALSE;
		}

		videoparams_sethdmi(pvideo, FALSE);
		videoparams_setcolorresolution(pvideo, 8);
		if (currentmode >= api_ediddtdcount())
		{
			currentmode = 0;
		}
		for (; currentmode < api_ediddtdcount(); currentmode++)
		{
			if (api_ediddtd(currentmode, &tmp_dtd))
			{
				ceacode = cliedid_getdtdcode(&tmp_dtd);
				dtd_fill(&tmp_dtd, ceacode, board_supportedrefreshrate(ceacode));
				videoparams_setdtd(pvideo, &tmp_dtd);
			}
			if (api_configure(pvideo, paudio, pproduct, phdcp, fixed_color_screen))
			{
				break;
			}
			errorcode = error_get();
			hdmi_err("API configure %d", errorcode);
			if (errorcode == ERR_HDCP_FAIL)
			{
				system_sleepms(1000);
				currentmode--;
			}
		}
		if (currentmode >= api_ediddtdcount())
		{
			hdmi_err("----spanned all dtds and non works, sending VGA\n");
			dtd_fill(&tmp_dtd, 1, 60000);
//			dtd_fill(&tmp_dtd, 0, board_supportedrefreshrate(1));
			videoparams_setdtd(pvideo, &tmp_dtd);
			api_configure(pvideo, paudio, pproduct, phdcp, fixed_color_screen);
		}
	}
	return TRUE;
}

int compliance_modex(u8 x)
{
	if ((videoparams_gethdmi(pvideo)) && (x < api_edidsvdcount()))
	{
		currentmode = x;
	}
	else if ((!videoparams_gethdmi(pvideo)) && (x < api_ediddtdcount()))
	{
		currentmode = x;
	}
	else
	{
		LOG_WARNING("invalid index");
		return FALSE;
	}
	return compliance_configure(0);
}

int compliance_next(void)
{
	currentmode++;

	return compliance_configure(0);
}


void compliance_audio_mute(int enable)
{
	api_audiomute(enable);
}

int compliance_audio_params_set(void *audparam)
{
	audioParams_t *audio_params = NULL;
	int audioon = 0;

	if (!paudio || !audparam) {
		printk(KERN_ERR "paudio or audparam is null");
		return -1;
	}

	audioon = videoparams_gethdmi(pvideo);

	if (audioon == 0) {
		printk(KERN_ERR "is not stage audio");
		return -1;
	}

	audio_params = (audioParams_t *)audparam;
	memcpy(paudio, audio_params, sizeof(audioParams_t));
	return only_audio_configure(pvideo, paudio);
}

int compliance_get_currentmode(void)
{
	return currentmode;
}

void compliance_set_currentmode(int i)
{
	currentmode = i;
}

int compliance_find_resolution(int i)
{
	return 0;
}

int get_dtd(dtd_t *tmp_dtd)
{
	int ceacode;
	shortVideoDesc_t tmp_svd;
	int svdno = api_edidsvdcount();
	int dtdno = api_ediddtdcount();

	if (dtdno == 0 || currentmode >= (dtdno + svdno))
	{
		hdmi_debug(LOG_DEBUG,"dtdno = %d, svdno = %d, currentmode = %d\n",
				dtdno, svdno, currentmode);
		return -1;
	}

	if (currentmode < dtdno)
	{
		api_ediddtd(currentmode, tmp_dtd);
	}
	else if (currentmode < svdno+dtdno)
	{

		tmp_svd = current_svd[currentmode-dtdno];
#if 0
		if (api_edidsvd(currentmode-dtdno, &tmp_svd))
#endif
		{
			ceacode = shortvideodesc_getcode(&tmp_svd);

			if (board_supportedrefreshrate(ceacode) != -1)
			{
				dtd_fill(tmp_dtd, ceacode, board_supportedrefreshrate(ceacode));
			}
			else
			{
				hdmi_err("CEA mode not supported %d", ceacode);
				return -1;
			}
		}
	}

	return 0;
}

bool is_dtd(void)
{
	int dtdno = api_ediddtdcount();

	if (currentmode < dtdno)
	{
		return true;
	}
	return false;
}

int get_pclock(void)
{
	dtd_t 	dtd_desc;
	api_ediddtd(currentmode, &dtd_desc);

	return dtd_desc.mpixelclock;
}

int get_ceacode(void)
{
	shortVideoDesc_t tmp_svd;
	/* int svdno = api_edidsvdcount(); */
	int dtdno = api_ediddtdcount();
#if 0
	api_edidsvd(currentmode-dtdno, &tmp_svd);
#else
	tmp_svd = current_svd[currentmode-dtdno];
#endif

	return shortvideodesc_getcode(&tmp_svd);
}

int get_sad_count(void)
{
	return api_edidsadcount() * 3;
}

int get_edid_sad(u8 *sadbuf, int cnt)
{
	int sadno, i;
	int copy_cnt = 0;
	shortAudioDesc_t sad_desc;

	if (sadbuf == NULL)
		return -1;

	sadno = api_edidsadcount();

	for (i = 0; i < sadno; i++)
	{
		api_edidsad(i, &sad_desc);
		sadbuf[copy_cnt++] = (sad_desc.mformat << 3) |
						(sad_desc.mmaxchannels - 1);
		sadbuf[copy_cnt++] = sad_desc.msamplerates;
		sadbuf[copy_cnt++] = sad_desc.mbyte3;
	}

	return !(copy_cnt == cnt);
}

int get_edid_spk_alloc_data(u8 *spk_alloc)
{
	speakerAllocationDataBlock_t allocation;

	if (spk_alloc == NULL)
		return -1;

	if (TRUE == api_edidspeakerallocationdatablock(&allocation))
		*spk_alloc = allocation.mbyte1;
	else
		*spk_alloc = 0;

	return 0;
}

int compliance_total_count(void)
{
	int sadno;
	int dtdno;
	sadno = api_edidsadcount();
	dtdno = api_ediddtdcount();

	return (sadno + dtdno);

}

void compliance_show_sad(void)
{
	int sadno, i;
	shortAudioDesc_t sad_desc;
	sadno = api_edidsadcount();

	hdmi_debug(LOG_DEBUG,"---------------------sadNo--------------------: %d\n", sadno);
	for (i = 0; i < sadno; i++)
	{
		api_edidsad(i, &sad_desc);
		hdmi_debug(LOG_DEBUG,"(%d)**********************************************\n", i);
		hdmi_debug(LOG_DEBUG,"mformat:%d mmaxchannels:%d msamplerates:%d mbyte3:%d\n",
			sad_desc.mformat, sad_desc.mmaxchannels,
			sad_desc.msamplerates, sad_desc.mbyte3);
	}
}

int compliance_show_dtd_list(void)
{
	int dtdno, i;
	dtd_t 	dtd_desc;

	dtdno = api_ediddtdcount();

	printk("---------------------dtdNo--------------------: %d\n", dtdno);

	for (i = 0; i < dtdno; i++)
	{
		api_ediddtd(i, &dtd_desc);

		hdmi_debug(LOG_DEBUG,"(%d)**********************************************\n", i);
		hdmi_debug(LOG_DEBUG,"mcode:%d mpixelrepetitioninput:%d mpixelclock:%d minterlaced:%d\n",
	dtd_desc.mcode, dtd_desc.mpixelrepetitioninput,
	dtd_desc.mpixelclock, dtd_desc.minterlaced);

		hdmi_debug(LOG_DEBUG,"mhactive:%d mhblanking:%d mhborder:%d mhimagesize:%d\n",
	dtd_desc.mhactive, dtd_desc.mhblanking,
	dtd_desc.mhborder, dtd_desc.mhimagesize);

		hdmi_debug(LOG_DEBUG,"mhsyncoffset:%d mhsyncpulsewidth:%d mhsyncpolarity:%d \n",
	dtd_desc.mhsyncoffset, dtd_desc.mhsyncpulsewidth,
	dtd_desc.mhsyncpolarity);


		hdmi_debug(LOG_DEBUG,"mvactive:%d mvblanking:%d mvborder:%d mvimagesize:%d\n",
	dtd_desc.mvactive, dtd_desc.mvblanking,
	dtd_desc.mvborder, dtd_desc.mvimagesize);

		hdmi_debug(LOG_DEBUG,"mvsyncoffset:%d mvsyncpulsewidth:%d mvsyncpolarity:%d \n",
	dtd_desc.mvsyncoffset, dtd_desc.mvsyncpulsewidth,
	dtd_desc.mvsyncpolarity);
		hdmi_debug(LOG_DEBUG,"************************************************\n");
	}

	return i;
}

int compliance_show_svd_list(int i)
{
	u8 svdno, ceacode, j;
	shortVideoDesc_t tmp_svd;
	dtd_t 	dtd_desc;

	svdno = api_edidsvdcount();

	hdmi_debug(LOG_DEBUG,"---------------------svdNo--------------------: %d\n", svdno);

	for (j = 0; j < svdno; j++)
	{
		api_edidsvd(j, &tmp_svd);
		ceacode = shortvideodesc_getcode(&tmp_svd);
		dtd_fill(&dtd_desc, ceacode, board_supportedrefreshrate(ceacode));

		hdmi_debug(LOG_DEBUG,"(%d)*******************ceacode-%d************************\n",
			i++, ceacode);
		hdmi_debug(LOG_DEBUG,"mcode:%d mpixelrepetitioninput:%d mpixelclock:%d minterlaced:%d\n",
	dtd_desc.mcode, dtd_desc.mpixelrepetitioninput,
	dtd_desc.mpixelclock, dtd_desc.minterlaced);

		hdmi_debug(LOG_DEBUG,"mhactive:%d mhblanking:%d mhborder:%d mhimagesize:%d\n",
	dtd_desc.mhactive, dtd_desc.mhblanking,
	dtd_desc.mhborder, dtd_desc.mhimagesize);

		hdmi_debug(LOG_DEBUG,"mhsyncoffset:%d mhsyncpulsewidth:%d mhsyncpolarity:%d \n",
	dtd_desc.mhsyncoffset, dtd_desc.mhsyncpulsewidth,
	dtd_desc.mhsyncpolarity);


		hdmi_debug(LOG_DEBUG,"mvactive:%d mvblanking:%d mvborder:%d mvimagesize:%d\n",
	dtd_desc.mvactive, dtd_desc.mvblanking,
	dtd_desc.mvborder, dtd_desc.mvimagesize);

		hdmi_debug(LOG_DEBUG,"mvsyncoffset:%d mvsyncpulsewidth:%d mvsyncpolarity:%d \n",
	dtd_desc.mvsyncoffset, dtd_desc.mvsyncpulsewidth,
	dtd_desc.mvsyncpolarity);
		hdmi_debug(LOG_DEBUG,"************************************************\n");
	}

	return i;
}

int getModeOrder(int mode)
{

	switch (mode) {
		default:
		case HDMI_VFRMT_1440x480i60_4_3:
			return 1; // 480i 4:3
		case HDMI_VFRMT_1440x480i60_16_9:
			return 2; // 480i 16:9
		case HDMI_VFRMT_1440x576i50_4_3:
			return 3; // i576i 4:3
		case HDMI_VFRMT_1440x576i50_16_9:
			return 4; // 576i 16:9
		case HDMI_VFRMT_1920x1080i60_16_9:
			return 5; // 1080i 16:9
		case HDMI_VFRMT_640x480p60_4_3:
			return 6; // 640x480 4:3
		case HDMI_VFRMT_720x480p60_4_3:
			return 7; // 480p 4:3
		case HDMI_VFRMT_720x480p60_16_9:
			return 8; // 480p 16:9
		case HDMI_VFRMT_720x576p50_4_3:
			return 9; // 576p 4:3
		case HDMI_VFRMT_720x576p50_16_9:
			return 10; // 576p 16:9
		case HDMI_VFRMT_1024x768p60_4_3:
			return 11; // 768p 4:3 Vesa format
		case HDMI_VFRMT_1280x1024p60_5_4:
			return 12; // 1024p Vesa format
		case HDMI_VFRMT_1280x720p50_16_9:
			return 13; // 720p@50Hz
		case HDMI_VFRMT_1280x720p60_16_9:
			return 14; // 720p@60Hz
		case HDMI_VFRMT_1920x1080p24_16_9:
			return 15; //1080p@24Hz
		case HDMI_VFRMT_1920x1080p25_16_9:
			return 16; //108-p@25Hz
		case HDMI_VFRMT_1920x1080p30_16_9:
			return 17; //1080p@30Hz
		case HDMI_VFRMT_1920x1080p50_16_9:
			return 18; //1080p@50Hz
		case HDMI_VFRMT_1920x1080p60_16_9:
			return 19; //1080p@60Hz
		case HDMI_VFRMT_2560x1600p60_16_9:
			return 20; //WQXGA@60Hz541
		case HDMI_VFRMT_3840x2160p24_16_9:
			return 21;//2160@24Hz
		case HDMI_VFRMT_3840x2160p25_16_9:
			return 22;//2160@25Hz
		case HDMI_VFRMT_3840x2160p30_16_9:
			return 23; //2160@30Hz
		case HDMI_VFRMT_4096x2160p24_16_9:
			return 24; //4kx2k@24Hz
	}
}

static void limit_supported_video_format(unsigned int *video_format)
{
	unchar video_format_type = sp_get_link_bw();

	switch(video_format_type)
	{
		case 0x0a:
			hdmi_debug(LOG_DEBUG,"get link bw type= 0x0a, format = %d\n", *video_format);
			if((*video_format == HDMI_VFRMT_1920x1080p60_16_9) ||
					(*video_format == HDMI_VFRMT_2880x480p60_4_3)||
					(*video_format == HDMI_VFRMT_2880x480p60_16_9) ||
					(*video_format == HDMI_VFRMT_1280x720p120_16_9))
			{
				hdmi_debug(LOG_DEBUG,"video format change %d to %d \n", *video_format, HDMI_VFRMT_1280x720p60_16_9);
				*video_format = HDMI_VFRMT_1280x720p60_16_9;
			}
			else if((*video_format == HDMI_VFRMT_1920x1080p50_16_9) ||
					(*video_format == HDMI_VFRMT_2880x576p50_4_3)||
					(*video_format == HDMI_VFRMT_2880x576p50_16_9) ||
					(*video_format == HDMI_VFRMT_1280x720p100_16_9))
			{
				hdmi_debug(LOG_DEBUG,"video format change %d to %d \n", *video_format, HDMI_VFRMT_1280x720p50_16_9);
				*video_format = HDMI_VFRMT_1280x720p50_16_9;
			}

			else if (*video_format == HDMI_VFRMT_1920x1080i100_16_9)
			{
				hdmi_debug(LOG_DEBUG,"video format change %d to %d \n", *video_format, HDMI_VFRMT_1920x1080i50_16_9);
				*video_format = HDMI_VFRMT_1920x1080i50_16_9;
			}
			else if (*video_format == HDMI_VFRMT_1920x1080i120_16_9)
			{
				hdmi_debug(LOG_DEBUG,"video format change %d to %d \n", *video_format, HDMI_VFRMT_1920x1080i60_16_9);
				*video_format = HDMI_VFRMT_1920x1080i60_16_9;
			}
			break;

		case 0x06:

			hdmi_debug(LOG_DEBUG,"get link bw = 0x06\n");
			if(*video_format != HDMI_VFRMT_640x480p60_4_3)
				*video_format = HDMI_VFRMT_640x480p60_4_3;
			break;

		case 0x14:
		default:
			break;
	}

}

int compliance_svd_select(void)
{
	u8 svdno, dtdno, ceacode;
	unsigned int limit_ceacode;
	int bestmode;
	int order, bestorder=0;
	int i, best_idx=0;
	shortVideoDesc_t tmp_svd;
	int disp_mode_count=0;

	svdno = api_edidsvdcount();
	dtdno = api_ediddtdcount();
	if(svdno <= 0)
		return false;

	for(i=0; i<svdno; i++)
	{
		api_edidsvd(i, &tmp_svd);
		ceacode = shortvideodesc_getcode(&tmp_svd);

		limit_ceacode = 0;
		limit_ceacode = 0xff & ceacode;
		limit_supported_video_format(&limit_ceacode);
		current_svd[i].mcode = (u8)limit_ceacode;
		disp_mode_count++;
	}

/* Changing to preferred resolution through sysfs*/
	if(aftmode_resolution != 0) {
		for (i=0; i<disp_mode_count; i++) {
			if (aftmode_resolution == current_svd[i].mcode){
				best_idx = i;
				break;
			}
		}

	}

	if (disp_mode_count == i) {
		for(i=0; i<disp_mode_count; i++)
		{
#if 0
			api_edidsvd(i, &tmp_svd);
			ceacode = shortvideodesc_getcode(&tmp_svd);
#endif
			order = getModeOrder(current_svd[i].mcode);
			if(order > bestorder)
			{
				bestorder = order;
				bestmode = ceacode;
				best_idx = i;
			}

		}
	}

	best_idx = best_idx+dtdno;
	compliance_set_currentmode(best_idx);

	return true;
}


int compliance_video_desc_choice(void)
{
	int i, j, r = 0;
	i = compliance_show_dtd_list();
	j = compliance_show_svd_list(i);
	/* HDMI_COMP_DEBUG_INFO("****** total: %d*********\n"); */

	/* todo later....*/
	return r;
}

