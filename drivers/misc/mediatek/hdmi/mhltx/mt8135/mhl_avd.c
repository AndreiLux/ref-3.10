#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT
#include "mhl_avd.h"
#include "mhl_ctrl.h"
#include "mhl_hdcp.h"

void av_hdmiset(AV_D_HDMI_DRV_SET_TYPE_T e_set_type, const void *pv_set_info,
		unsigned char z_set_info_len)
{
	HDMI_AV_INFO_T *prAvInf;

	prAvInf = (HDMI_AV_INFO_T *) pv_set_info;

	switch (e_set_type) {

	case HDMI_SET_TURN_OFF_TMDS:
		vTmdsOnOffAndResetHdcp(prAvInf->fgHdmiTmdsEnable);
		break;

	case HDMI_SET_VPLL:
		vChangeVpll(prAvInf->e_resolution);
		break;

	case HDMI_SET_VIDEO_RES_CHG:
		vChgHDMIVideoResolution(prAvInf->e_resolution, prAvInf->e_video_color_space,
					prAvInf->e_hdmi_fs);
		break;

	case HDMI_SET_AUDIO_CHG_SETTING:
		vChgHDMIAudioOutput(prAvInf->e_hdmi_fs, prAvInf->e_resolution);
		break;

	case HDMI_SET_HDCP_INITIAL_AUTH:
		vMoveHDCPInternalKey(INTERNAL_ENCRYPT_KEY);
		vHDCPInitAuth();
		break;

	case HDMI_SET_VIDEO_COLOR_SPACE:

		break;

	case HDMI_SET_SOFT_NCTS:
		vChgtoSoftNCTS(prAvInf->e_resolution, prAvInf->u1audiosoft, prAvInf->e_hdmi_fs);
		break;

	case HDMI_SET_HDCP_OFF:
		vDisableHDCP(prAvInf->u1hdcponoff);
		break;

	default:
		break;
	}

}
#endif
