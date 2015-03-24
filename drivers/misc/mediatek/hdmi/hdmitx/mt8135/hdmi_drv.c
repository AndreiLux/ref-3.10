/*----------------------------------------------------------------------------*/
#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>

#include "hdmi_ctrl.h"

#include "hdmiddc.h"
#include "hdmihdcp.h"

#include "internal_hdmi_drv.h"

#include "mach/eint.h"
#include "mach/irqs.h"

#ifdef MT6575
#include <mach/mt6575_devs.h>
#include <mach/mt6575_typedefs.h>
#include <mach/mt6575_gpio.h>
#include <mach/mt6575_pm_ldo.h>
#else
#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_pm_ldo.h>
#endif

/* #include "hdmi_iic.h" */
#include "hdmiavd.h"
#include "hdmicmd.h"
#include <mach/mt_pmic_wrap.h>

#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
#include "hdmi_ca.h"
#endif
/*----------------------------------------------------------------------------*/
/* Debug message defination */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* HDMI Timer */
/*----------------------------------------------------------------------------*/

static struct timer_list r_hdmi_timer;

static uint32_t gHDMI_CHK_INTERVAL = 10;

size_t hdmidrv_log_on = hdmidrvlog | hdmihdcplog | hdmitxaudiolog | hdmitxvideolog | hdmitxhotpluglog | hdmidgilog;
size_t hdmi_cec_on = 0;
size_t hdmi_cec_interrupt = 0;
size_t hdmi_cecinit = 0;
size_t hdmi_hdmiinit = 0;
size_t hdmi_printaudio = 0;
size_t hdmi_hotinit = 0;
size_t hdmi_vga_mode = 1;
size_t hdmi_hdmipoweroninit = 0;
size_t hdmi_TmrValue[MAX_HDMI_TMR_NUMBER] = { 0 };
static unsigned long delay_detect_plug = 0;
size_t hdmi_hdmiCmd = 0xff;
size_t hdmi_rxcecmode = CEC_NORMAL_MODE;
HDMI_CTRL_STATE_T e_hdmi_ctrl_state = HDMI_STATE_IDLE;
HDCP_CTRL_STATE_T e_hdcp_ctrl_state = HDCP_RECEIVER_NOT_READY;
size_t hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;

#if defined(CONFIG_HAS_EARLYSUSPEND)
size_t hdmi_hdmiearlysuspend = 1;
#endif

static struct task_struct *hdmi_timer_task;
wait_queue_head_t hdmi_timer_wq;
atomic_t hdmi_timer_event = ATOMIC_INIT(0);

static HDMI_UTIL_FUNCS hdmi_util = { 0 };

void hdmi_poll_isr(unsigned long n);
static int hdmi_timer_kthread(void *data);

const char *szHdmiPordStatusStr[] = {
	"HDMI_PLUG_OUT=0",
	"HDMI_PLUG_IN_AND_SINK_POWER_ON",
	"HDMI_PLUG_IN_ONLY",
	"HDMI_PLUG_IN_EDID",
	"HDMI_PLUG_IN_CEC",
	"HDMI_PLUG_IN_POWER_EDID",
};

static void vInitAvInfoVar(void)
{
	_stAvdAVInfo.e_resolution = HDMI_VIDEO_1280x720p_50Hz;
	_stAvdAVInfo.fgHdmiOutEnable = TRUE;
	_stAvdAVInfo.fgHdmiTmdsEnable = TRUE;

	_stAvdAVInfo.bMuteHdmiAudio = FALSE;
	_stAvdAVInfo.e_video_color_space = HDMI_RGB;
	_stAvdAVInfo.e_deep_color_bit = HDMI_NO_DEEP_COLOR;
	_stAvdAVInfo.ui1_aud_out_ch_number = 2;
	_stAvdAVInfo.e_hdmi_fs = HDMI_FS_44K;

	_stAvdAVInfo.bhdmiRChstatus[0] = 0x00;
	_stAvdAVInfo.bhdmiRChstatus[1] = 0x00;
	_stAvdAVInfo.bhdmiRChstatus[2] = 0x02;
	_stAvdAVInfo.bhdmiRChstatus[3] = 0x00;
	_stAvdAVInfo.bhdmiRChstatus[4] = 0x00;
	_stAvdAVInfo.bhdmiRChstatus[5] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[0] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[1] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[2] = 0x02;
	_stAvdAVInfo.bhdmiLChstatus[3] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[4] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[5] = 0x00;

	vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);



}

void vSetHDMIMdiTimeOut(unsigned int i4_count)
{
	HDMI_DRV_FUNC();
	hdmi_TmrValue[HDMI_PLUG_DETECT_CMD] = i4_count;

}

/*----------------------------------------------------------------------------*/

static void hdmi_set_util_funcs(const HDMI_UTIL_FUNCS *util)
{
	memcpy(&hdmi_util, util, sizeof(HDMI_UTIL_FUNCS));
}

/*----------------------------------------------------------------------------*/

static void hdmi_get_params(HDMI_PARAMS *params)
{
	memset(params, 0, sizeof(HDMI_PARAMS));

	HDMI_DRV_LOG("720p\n");
	params->init_config.vformat = HDMI_VIDEO_1280x720p_50Hz;
	params->init_config.aformat = HDMI_AUDIO_PCM_16bit_48000;

	params->clk_pol = HDMI_POLARITY_FALLING;
	params->de_pol = HDMI_POLARITY_RISING;
	params->vsync_pol = HDMI_POLARITY_RISING;
	params->hsync_pol = HDMI_POLARITY_RISING;

	params->hsync_pulse_width = 40;
	params->hsync_back_porch = 220;
	params->hsync_front_porch = 440;
	params->vsync_pulse_width = 5;
	params->vsync_back_porch = 20;
	params->vsync_front_porch = 5;

	params->rgb_order = HDMI_COLOR_ORDER_RGB;

	params->io_driving_current = IO_DRIVING_CURRENT_2MA;
	params->intermediat_buffer_num = 4;
	params->output_mode = HDMI_OUTPUT_MODE_LCD_MIRROR;
	params->is_force_awake = 1;
	params->is_force_landscape = 1;

	params->scaling_factor = 0;

/*#ifndef CONFIG_MTK_HDMI_HDCP_SUPPORT*/
/*params->NeedSwHDCP = 1;*/
/*#endif*/

}

static int hdmi_internal_enter(void)
{
	HDMI_DRV_FUNC();
	return 0;

}

static int hdmi_internal_exit(void)
{
	HDMI_DRV_FUNC();
	return 0;
}

/*----------------------------------------------------------------------------*/

static void hdmi_internal_suspend(void)
{
	HDMI_DRV_FUNC();

	_stAvdAVInfo.fgHdmiTmdsEnable = 0;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);
}

/*----------------------------------------------------------------------------*/

static void hdmi_internal_resume(void)
{
	HDMI_DRV_FUNC();


}

/*----------------------------------------------------------------------------*/

static int hdmi_internal_video_config(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin,
				      HDMI_VIDEO_OUTPUT_FORMAT vout)
{
	HDMI_DRV_FUNC();
	if (r_hdmi_timer.function) {
		del_timer_sync(&r_hdmi_timer);
	}
	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));

	_stAvdAVInfo.e_resolution = vformat;

	/* _stAvdAVInfo.fgHdmiTmdsEnable = 0; */
	/* av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1); */
	av_hdmiset(HDMI_SET_VPLL, &_stAvdAVInfo, 1);
	av_hdmiset(HDMI_SET_SOFT_NCTS, &_stAvdAVInfo, 1);
	av_hdmiset(HDMI_SET_VIDEO_RES_CHG, &_stAvdAVInfo, 1);
	av_hdmiset(HDMI_SET_HDCP_INITIAL_AUTH, &_stAvdAVInfo, 1);

	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));
	r_hdmi_timer.expires = jiffies + 1 / (1000 / HZ);	/* wait 1s to stable */
	r_hdmi_timer.function = hdmi_poll_isr;
	r_hdmi_timer.data = 0;
	init_timer(&r_hdmi_timer);
	add_timer(&r_hdmi_timer);
	hdmi_hdmiinit = 1;
	hdmi_hdmistatus();
	return 0;
}

int hdmi_audiosetting(HDMIDRV_AUDIO_PARA *audio_para)
{
	HDMI_DRV_FUNC();

	_stAvdAVInfo.e_hdmi_aud_in = audio_para->e_hdmi_aud_in;	/* SV_I2S; */
	_stAvdAVInfo.e_iec_frame = audio_para->e_iec_frame;	/* IEC_48K; */
	_stAvdAVInfo.e_hdmi_fs = audio_para->e_hdmi_fs;	/* HDMI_FS_48K; */
	_stAvdAVInfo.e_aud_code = audio_para->e_aud_code;	/* AVD_LPCM; */
	_stAvdAVInfo.u1Aud_Input_Chan_Cnt = audio_para->u1Aud_Input_Chan_Cnt;	/* AUD_INPUT_2_0; */
	_stAvdAVInfo.e_I2sFmt = audio_para->e_I2sFmt;	/* HDMI_I2S_24BIT; */
	_stAvdAVInfo.u1HdmiI2sMclk = audio_para->u1HdmiI2sMclk;	/* MCLK_128FS; */
	_stAvdAVInfo.bhdmiLChstatus[0] = audio_para->bhdmi_LCh_status[0];
	_stAvdAVInfo.bhdmiLChstatus[1] = audio_para->bhdmi_LCh_status[1];
	_stAvdAVInfo.bhdmiLChstatus[2] = audio_para->bhdmi_LCh_status[2];
	_stAvdAVInfo.bhdmiLChstatus[3] = audio_para->bhdmi_LCh_status[3];
	_stAvdAVInfo.bhdmiLChstatus[4] = audio_para->bhdmi_LCh_status[4];
	_stAvdAVInfo.bhdmiRChstatus[0] = audio_para->bhdmi_RCh_status[0];
	_stAvdAVInfo.bhdmiRChstatus[1] = audio_para->bhdmi_RCh_status[1];
	_stAvdAVInfo.bhdmiRChstatus[2] = audio_para->bhdmi_RCh_status[2];
	_stAvdAVInfo.bhdmiRChstatus[3] = audio_para->bhdmi_RCh_status[3];
	_stAvdAVInfo.bhdmiRChstatus[4] = audio_para->bhdmi_RCh_status[4];

	av_hdmiset(HDMI_SET_AUDIO_CHG_SETTING, &_stAvdAVInfo, 1);
	HDMI_DRV_LOG("e_hdmi_aud_in=%d,e_iec_frame=%d,e_hdmi_fs=%d\n", _stAvdAVInfo.e_hdmi_aud_in,
		     _stAvdAVInfo.e_iec_frame, _stAvdAVInfo.e_hdmi_fs);
	HDMI_DRV_LOG("e_aud_code=%d,u1Aud_Input_Chan_Cnt=%d,e_I2sFmt=%d\n", _stAvdAVInfo.e_aud_code,
		     _stAvdAVInfo.u1Aud_Input_Chan_Cnt, _stAvdAVInfo.e_I2sFmt);
	HDMI_DRV_LOG("u1HdmiI2sMclk=%d\n", _stAvdAVInfo.u1HdmiI2sMclk);

	HDMI_DRV_LOG("bhdmiLChstatus0=%d\n", _stAvdAVInfo.bhdmiLChstatus[0]);
	HDMI_DRV_LOG("bhdmiLChstatus1=%d\n", _stAvdAVInfo.bhdmiLChstatus[1]);
	HDMI_DRV_LOG("bhdmiLChstatus2=%d\n", _stAvdAVInfo.bhdmiLChstatus[2]);
	HDMI_DRV_LOG("bhdmiLChstatus3=%d\n", _stAvdAVInfo.bhdmiLChstatus[3]);
	HDMI_DRV_LOG("bhdmiLChstatus4=%d\n", _stAvdAVInfo.bhdmiLChstatus[4]);
	HDMI_DRV_LOG("bhdmiRChstatus0=%d\n", _stAvdAVInfo.bhdmiRChstatus[0]);
	HDMI_DRV_LOG("bhdmiRChstatus1=%d\n", _stAvdAVInfo.bhdmiRChstatus[1]);
	HDMI_DRV_LOG("bhdmiRChstatus2=%d\n", _stAvdAVInfo.bhdmiRChstatus[2]);
	HDMI_DRV_LOG("bhdmiRChstatus3=%d\n", _stAvdAVInfo.bhdmiRChstatus[3]);
	HDMI_DRV_LOG("bhdmiRChstatus4=%d\n", _stAvdAVInfo.bhdmiRChstatus[4]);


	return 0;
}

int hdmi_tmdsonoff(unsigned char u1ionoff)
{
	HDMI_DRV_FUNC();

	_stAvdAVInfo.fgHdmiTmdsEnable = u1ionoff;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);

	return 0;
}

/*----------------------------------------------------------------------------*/

static int hdmi_internal_audio_config(HDMI_AUDIO_FORMAT aformat)
{
	HDMI_DRV_FUNC();

	return 0;
}

/*----------------------------------------------------------------------------*/

static int hdmi_internal_video_enable(unsigned char enable)
{
	HDMI_DRV_FUNC();

	return 0;
}

/*----------------------------------------------------------------------------*/

static int hdmi_internal_audio_enable(unsigned char enable)
{
	HDMI_DRV_FUNC();

	return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

void hdmi_internal_set_mode(unsigned char ucMode)
{
	HDMI_DRV_FUNC();
	vSetClk();

}

/*----------------------------------------------------------------------------*/

int hdmi_internal_power_on(void)
{
	unsigned int ui4Temp = 0;

	HDMI_DRV_FUNC();

#if defined(CONFIG_HAS_EARLYSUSPEND)
	if (hdmi_hdmiearlysuspend == 0)
		return 0;
#endif
	hdmi_hotinit = 0;
	hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;

	_stAvdAVInfo.fgHdmiTmdsEnable = 0;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);

	mt_hdmi_power_ctrl(TRUE);

	vWriteHdmiSYSMsk(HDMI_SYS_CFG20, HDMI_PCLK_FREE_RUN, HDMI_PCLK_FREE_RUN);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, ANLG_ON | HDMI_ON, ANLG_ON | HDMI_ON);

	vInitHdcpKeyGetMethod(NON_HOST_ACCESS_FROM_EEPROM);

	vWriteHdmiIntMask(0xFF);

	pwrap_read(0x14c, &ui4Temp);	/* hdmi pin enable */
	pwrap_write(0x14c, ui4Temp | 0x1);

	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));
	r_hdmi_timer.expires = jiffies + 100 / (1000 / HZ);	/* wait 1s to stable */
	r_hdmi_timer.function = hdmi_poll_isr;
	r_hdmi_timer.data = 0;
	init_timer(&r_hdmi_timer);
	add_timer(&r_hdmi_timer);

	return 0;
}

/*----------------------------------------------------------------------------*/

void hdmi_internal_power_off(void)
{
	unsigned int ui4Temp = 0;

	HDMI_DRV_FUNC();

	_stAvdAVInfo.fgHdmiTmdsEnable = 0;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);

	vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
	vWriteHdmiIntMask(0xFF);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, 0, ANLG_ON | HDMI_ON);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG20, 0, HDMI_PCLK_FREE_RUN);

	mt_hdmi_power_ctrl(FALSE);

	vWriteHdmiSYSMsk(HDMI_SYS_CFG110, DISABLE_DPICLK | DISABLE_IDCLK | DISABLE_PLLCLK,
			 DISABLE_DPICLK | DISABLE_IDCLK | DISABLE_PLLCLK);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG110, DISABLE_SPDIFCLK | DISABLE_BCLK,
			 DISABLE_SPDIFCLK | DISABLE_BCLK);

	pwrap_read(0x14c, &ui4Temp);	/* hdmi pin enable */
	pwrap_write(0x14c, ui4Temp & 0xfffffffe);

	if (r_hdmi_timer.function) {
		del_timer_sync(&r_hdmi_timer);
	}
	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));

	delay_detect_plug = jiffies;
	hdmi_hotinit = 1;
	hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
}

/*----------------------------------------------------------------------------*/

void hdmi_internal_dump(void)
{
	HDMI_DRV_FUNC();
	/* hdmi_dump_reg(); */
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

HDMI_STATE hdmi_get_state(void)
{
	HDMI_DRV_FUNC();

	if (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE) {
		return HDMI_STATE_ACTIVE;
	} else {
		return HDMI_STATE_NO_DEVICE;
	}
}

void hdmi_enablehdcp(unsigned char u1hdcponoff)
{
	HDMI_DRV_FUNC();
	_stAvdAVInfo.u1hdcponoff = u1hdcponoff;
	av_hdmiset(HDMI_SET_HDCP_OFF, &_stAvdAVInfo, 1);
}

void hdmi_setcecrxmode(unsigned char u1cecrxmode)
{
	HDMI_DRV_FUNC();
	hdmi_rxcecmode = u1cecrxmode;
}

void hdmi_colordeep(unsigned char u1colorspace, unsigned char u1deepcolor)
{
	HDMI_DRV_FUNC();
	if ((u1colorspace == 0xff) && (u1deepcolor == 0xff)) {
		pr_info("color_space:HDMI_YCBCR_444 = 2\n");
		pr_info("color_space:HDMI_YCBCR_422 = 3\n");

		pr_info("deep_color:HDMI_NO_DEEP_COLOR = 1\n");
		pr_info("deep_color:HDMI_DEEP_COLOR_10_BIT = 2\n");
		pr_info("deep_color:HDMI_DEEP_COLOR_12_BIT = 3\n");
		pr_info("deep_color:HDMI_DEEP_COLOR_16_BIT = 4\n");

		return;
	}

	_stAvdAVInfo.e_video_color_space = HDMI_RGB;

	_stAvdAVInfo.e_deep_color_bit = (HDMI_DEEP_COLOR_T) u1deepcolor;
}

void hdmi_read(unsigned int u2Reg, unsigned int *p4Data)
{

}

void hdmi_write(unsigned int u2Reg, unsigned int u4Data)
{
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	if ((u2Reg & 0xFFFFF000) == 0) {
		if (u2Reg < 0x100)
			vCaDPI1WriteReg(u2Reg, u4Data);
		else
			vCaHDMIWriteReg(u2Reg, u4Data);
	}
#endif
	pr_info("Reg write= 0x%08x, data = 0x%08x\n", u2Reg, u4Data);
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void hdmi_hdmi_early_suspend(struct early_suspend *h)
{
	HDMI_PLUG_FUNC();
	hdmi_hdmiearlysuspend = 0;
}

static void hdmi_hdmi_late_resume(struct early_suspend *h)
{
	HDMI_PLUG_FUNC();
	hdmi_hdmiearlysuspend = 1;
}

static struct early_suspend hdmi_hdmi_early_suspend_desc = {
	.level = 0xFE,
	.suspend = hdmi_hdmi_early_suspend,
	.resume = hdmi_hdmi_late_resume,
};
#endif

static int hdmi_internal_init(void)
{
	HDMI_DRV_FUNC();

	init_waitqueue_head(&hdmi_timer_wq);
	hdmi_timer_task = kthread_create(hdmi_timer_kthread, NULL, "hdmi_timer_kthread");
	wake_up_process(hdmi_timer_task);

#if defined(CONFIG_HAS_EARLYSUSPEND)
	register_early_suspend(&hdmi_hdmi_early_suspend_desc);
#endif

#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	pr_info("[HDMI]fgCaHDMICreate\n");
	fgCaHDMICreate();
#endif

	return 0;
}

int check_sink_mode(void)
{
	return hdmi_vga_mode;
}

static void vNotifyAppHdmiState(unsigned char u1hdmistate)
{
	HDMI_EDID_INFO_T get_info;

	HDMI_PLUG_LOG("u1hdmistate = %d\n", u1hdmistate);
	pr_info("[hdmi_plug] u1hdmistate = %d, %s\n", u1hdmistate,
		szHdmiPordStatusStr[u1hdmistate]);

	hdmi_AppGetEdidInfo(&get_info);
	vShowEdidInformation();
	if (1 == get_info.ui1_Video_Input_Definitation) {
		hdmi_vga_mode = 1;
	} else {
		hdmi_vga_mode = 0;
	}

	switch (u1hdmistate) {
	case HDMI_PLUG_OUT:
		hdmi_util.state_callback(HDMI_STATE_NO_DEVICE);
		hdmi_SetPhysicCECAddress(0xffff, 0x0);
		break;

	case HDMI_PLUG_IN_AND_SINK_POWER_ON:
		hdmi_util.state_callback(HDMI_STATE_ACTIVE);
		hdmi_SetPhysicCECAddress(get_info.ui2_sink_cec_address, 0x4);
		break;

	case HDMI_PLUG_IN_ONLY:
		hdmi_util.state_callback(HDMI_STATE_PLUGIN_ONLY);
		hdmi_SetPhysicCECAddress(get_info.ui2_sink_cec_address, 0xf);
		break;

	case HDMI_PLUG_IN_CEC:
		hdmi_util.state_callback(HDMI_STATE_CEC_UPDATE);
		break;

	default:
		break;

	}
}

/*
*	For DDC I2C verification, we add the function to turn on/off
*	workaround solution.
*	The default mode is set to DDC I2C mode.
*	By Kevin Hwang
*/
#ifdef CONFIG_SLIMPORT_ANX3618
static unsigned char g_set_edid_mode = 1;
void change_edid_read_mode(unsigned char use_mem)
{
	pr_info("mydp: edid read mode is %s mode.\n", use_mem == 1 ? "MEM":"DDC I2C");
	g_set_edid_mode = use_mem;
}
#else
static unsigned char g_set_edid_mode;
#endif

static void vPlugDetectService(HDMI_CTRL_STATE_T e_state)
{
	unsigned char bData = 0xff;

	HDMI_PLUG_FUNC();

	e_hdmi_ctrl_state = HDMI_STATE_IDLE;

	switch (e_state) {
	case HDMI_STATE_HOT_PLUG_OUT:
		vClearEdidInfo();
		vHDCPReset();
		bData = HDMI_PLUG_OUT;

		break;

	case HDMI_STATE_HOT_PLUGIN_AND_POWER_ON:
		hdmi_checkedid(g_set_edid_mode);
		bData = HDMI_PLUG_IN_AND_SINK_POWER_ON;

		break;

	case HDMI_STATE_HOT_PLUG_IN_ONLY:
		vClearEdidInfo();
		vHDCPReset();
		hdmi_checkedid(g_set_edid_mode);
		bData = HDMI_PLUG_IN_ONLY;

		break;

	case HDMI_STATE_IDLE:

		break;
	default:
		break;

	}

	if (bData != 0xff)
		vNotifyAppHdmiState(bData);
}

void hdmi_drvlog_enable(unsigned short enable)
{
	HDMI_DRV_FUNC();

	if (enable == 0) {
		pr_info("hdmi_pll_log =   0x1\n");
		pr_info("hdmi_dgi_log =   0x2\n");
		pr_info("hdmi_plug_log =  0x4\n");
		pr_info("hdmi_video_log = 0x8\n");
		pr_info("hdmi_audio_log = 0x10\n");
		pr_info("hdmi_hdcp_log =  0x20\n");
		pr_info("hdmi_cec_log =   0x40\n");
		pr_info("hdmi_ddc_log =   0x80\n");
		pr_info("hdmi_edid_log =  0x100\n");
		pr_info("hdmi_drv_log =   0x200\n");

		pr_info("hdmi_all_log =   0x3ff\n");

	}

	hdmidrv_log_on = enable;

	if ((enable & 0xc000) == 0xc000) {
		hdmi_hotinit = 2;
	} else if ((enable & 0x8000) == 0x8000) {
		vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
		vPlugDetectService(HDMI_STATE_HOT_PLUG_OUT);
		hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
		hdmi_hotinit = 1;
	} else if ((enable & 0x4000) == 0x4000) {
		vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_IN_AND_SINK_POWER_ON);
		vPlugDetectService(HDMI_STATE_HOT_PLUGIN_AND_POWER_ON);
		hdmi_hotplugstate = HDMI_STATE_HOT_PLUGIN_AND_POWER_ON;
		hdmi_hotinit = 1;
	}

}

void print_audio_register(void)
{
	pr_info("0xf400f818 = 0x%x\n",bReadByteHdmiGRL(0x18));
	pr_info("0xf400f81c = 0x%x\n",bReadByteHdmiGRL(0x1c));
	pr_info("0xf400f820 = 0x%x\n",bReadByteHdmiGRL(0x20));
	pr_info("0xf400f824 = 0x%x\n",bReadByteHdmiGRL(0x24));
	pr_info("0xf400f828 = 0x%x\n",bReadByteHdmiGRL(0x28));
	pr_info("0xf400f82c = 0x%x\n",bReadByteHdmiGRL(0x2c));
	pr_info("0xf400f830 = 0x%x\n",bReadByteHdmiGRL(0x30));
	pr_info("0xf400f834 = 0x%x\n",bReadByteHdmiGRL(0x34));
	pr_info("0xf400f838 = 0x%x\n",bReadByteHdmiGRL(0x38));
	pr_info("0xf400f8bc = 0x%x\n",bReadByteHdmiGRL(0xbc));

	pr_info("0xf400f954 = 0x%x\n",bReadByteHdmiGRL(0x154));
	pr_info("0xf400f958 = 0x%x\n",bReadByteHdmiGRL(0x158));
	pr_info("0xf400f95c = 0x%x\n",bReadByteHdmiGRL(0x15c));

	pr_info("0xf400f960 = 0x%x\n",bReadByteHdmiGRL(0x160));
	pr_info("0xf400f964 = 0x%x\n",bReadByteHdmiGRL(0x164));
	pr_info("0xf400f968 = 0x%x\n",bReadByteHdmiGRL(0x168));
	pr_info("0xf400f96c = 0x%x\n",bReadByteHdmiGRL(0x16c));

	pr_info("0xf400f978 = 0x%x\n",bReadByteHdmiGRL(0x178));
	pr_info("0xf400f97c = 0x%x\n",bReadByteHdmiGRL(0x17c));

	pr_info("0xf400f9b0 = 0x%x\n",bReadByteHdmiGRL(0x1b0));
	pr_info("0xf400fa00 = 0x%x\n",bReadByteHdmiGRL(0x200));
	pr_info("0xf400fa60 = 0x%x\n",bReadByteHdmiGRL(0x260));
	pr_info("0xf400f08c = 0x%x\n",(*(volatile unsigned int *)(0xf400f08c)));
	pr_info("0xf400f090 = 0x%x\n",(*(volatile unsigned int *)(0xf400f090)));
}

void hdmi_timer_impl(void)
{
	if (hdmi_hdmiinit == 0) {
		hdmi_hdmiinit = 1;
		/* hdmi_internal_power_off(); */
		/* vMoveHDCPInternalKey(INTERNAL_ENCRYPT_KEY); */
		vInitAvInfoVar();
		return;
	}
	if ((hdmi_hotplugstate == HDMI_STATE_HOT_PLUG_OUT)
		&& (jiffies_to_msecs(jiffies - delay_detect_plug) < MIN_PERIOD_NOTIFICATION)) {
		pr_err("error: wait 2s from unplug to plug\n");
		return;
	}
	if (hdmi_hotinit != 1)
		hdmi_hdmiinit++;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	if (hdmi_hdmiearlysuspend == 1)
#endif
	{
		if (((hdmi_hdmiinit > 5) || (hdmi_hotinit == 0)) && (hdmi_hotinit != 1)) {
			if (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == FALSE) {
				if ((hdmi_hotplugstate == HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
				    && (hdmi_hotinit == 2)) {
					hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
					vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
					vPlugDetectService(HDMI_STATE_HOT_PLUG_OUT);
					HDMI_PLUG_LOG
					    ("[hotplug1] hdmi_hotinit = %d,hdmi_hdmiinit=%d\n",
					     hdmi_hotinit, hdmi_hdmiinit);
				}

				if ((hdmi_hotinit == 0)
				    && (bCheckPordHotPlug(HOTPLUG_MODE) == TRUE)) {
					hdmi_hotinit = 2;
					hdmi_hotplugstate = HDMI_STATE_HOT_PLUGIN_AND_POWER_ON;
					vSetSharedInfo(SI_HDMI_RECEIVER_STATUS,
						       HDMI_PLUG_IN_AND_SINK_POWER_ON);
					vPlugDetectService(HDMI_STATE_HOT_PLUGIN_AND_POWER_ON);
					vWriteHdmiIntMask(0xff);	/* INT mask MDI */
					HDMI_PLUG_LOG
					    ("[hotplug2] hdmi_hotinit = %d,hdmi_hdmiinit=%d\n",
					     hdmi_hotinit, hdmi_hdmiinit);
				}

			} else if ((hdmi_hotplugstate == HDMI_STATE_HOT_PLUG_OUT) && (hdmi_hotinit != 1)
				   && (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE)) {
				hdmi_hotplugstate = HDMI_STATE_HOT_PLUGIN_AND_POWER_ON;
				hdmi_hotinit = 2;
				hdmi_printaudio = 0;
				delay_detect_plug = jiffies;
				vSetSharedInfo(SI_HDMI_RECEIVER_STATUS,
					       HDMI_PLUG_IN_AND_SINK_POWER_ON);
				vPlugDetectService(HDMI_STATE_HOT_PLUGIN_AND_POWER_ON);
				vWriteHdmiIntMask(0xff);	/* INT mask MDI */
				HDMI_PLUG_LOG("[hotplug3] hdmi_hotinit = %d,hdmi_hdmiinit=%d\n",
					      hdmi_hotinit, hdmi_hdmiinit);
			} else if ((hdmi_hotplugstate == HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
				   && ((e_hdcp_ctrl_state == HDCP_WAIT_RI)
				       || (e_hdcp_ctrl_state == HDCP_CHECK_LINK_INTEGRITY))) {
				if (bCheckHDCPStatus(HDCP_STA_RI_RDY)) {
					vSetHDCPState(HDCP_CHECK_LINK_INTEGRITY);
					vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				}
				if ((e_hdcp_ctrl_state == HDCP_CHECK_LINK_INTEGRITY) && (!hdmi_printaudio)) {
					hdmi_printaudio = 1;
					print_audio_register();
				}
			}
			hdmi_hdmiinit = 1;
		}
	}

	if (hdmi_hdmiCmd == HDMI_PLUG_DETECT_CMD) {
		vClearHdmiCmd();
		/* vcheckhdmiplugstate(); */
		/* vPlugDetectService(e_hdmi_ctrl_state); */
	} else if (hdmi_hdmiCmd == HDMI_HDCP_PROTOCAL_CMD) {
		vClearHdmiCmd();
		HdcpService(e_hdcp_ctrl_state);
	}
}

void cec_timer_impl(void)
{
	if ((hdmi_cecinit == 0) && (hdmi_cec_on == 1)) {
		hdmi_cecinit = 1;
		hdmi_cec_init();
		return;
	}

	if (hdmi_cec_on == 1) {
		hdmi_cec_mainloop(hdmi_rxcecmode);
	}
}

static int hdmi_timer_kthread(void *data)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_CAMERA_PREVIEW };
	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(hdmi_timer_wq, atomic_read(&hdmi_timer_event));
		atomic_set(&hdmi_timer_event, 0);
		hdmi_timer_impl();
		if (kthread_should_stop())
			break;
	}

	return 0;
}

void hdmi_poll_isr(unsigned long n)
{
	unsigned int i;

	for (i = 0; i < MAX_HDMI_TMR_NUMBER; i++) {
		if (hdmi_TmrValue[i] >= AVD_TMR_ISR_TICKS) {
			hdmi_TmrValue[i] -= AVD_TMR_ISR_TICKS;

			if ((i == HDMI_PLUG_DETECT_CMD)
			    && (hdmi_TmrValue[HDMI_PLUG_DETECT_CMD] == 0))
				vSendHdmiCmd(HDMI_PLUG_DETECT_CMD);
			else if ((i == HDMI_HDCP_PROTOCAL_CMD)
				 && (hdmi_TmrValue[HDMI_HDCP_PROTOCAL_CMD] == 0))
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		} else if (hdmi_TmrValue[i] > 0) {
			hdmi_TmrValue[i] = 0;

			if ((i == HDMI_PLUG_DETECT_CMD)
			    && (hdmi_TmrValue[HDMI_PLUG_DETECT_CMD] == 0))
				vSendHdmiCmd(HDMI_PLUG_DETECT_CMD);
			else if ((i == HDMI_HDCP_PROTOCAL_CMD)
				 && (hdmi_TmrValue[HDMI_HDCP_PROTOCAL_CMD] == 0))
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		}
	}

	atomic_set(&hdmi_timer_event, 1);
	wake_up_interruptible(&hdmi_timer_wq);
	mod_timer(&r_hdmi_timer, jiffies + gHDMI_CHK_INTERVAL / (1000 / HZ));

}


const HDMI_DRIVER *HDMI_GetDriver(void)
{
	static const HDMI_DRIVER HDMI_DRV = {
		.set_util_funcs = hdmi_set_util_funcs,
		.get_params = hdmi_get_params,
		.init = hdmi_internal_init,
		.enter = hdmi_internal_enter,
		.exit = hdmi_internal_exit,
		.suspend = hdmi_internal_suspend,
		.resume = hdmi_internal_resume,
		.video_config = hdmi_internal_video_config,
		.audio_config = hdmi_internal_audio_config,
		.video_enable = hdmi_internal_video_enable,
		.audio_enable = hdmi_internal_audio_enable,
		.power_on = hdmi_internal_power_on,
		.power_off = hdmi_internal_power_off,
		.set_mode = hdmi_internal_set_mode,
		.dump = hdmi_internal_dump,
		.read = hdmi_read,
		.write = hdmi_write,
		.get_state = hdmi_get_state,
		.log_enable = hdmi_drvlog_enable,
		.InfoframeSetting = hdmi_InfoframeSetting,
		.checkedid = hdmi_checkedid,
		.colordeep = hdmi_colordeep,
		.enablehdcp = hdmi_enablehdcp,
		.setcecrxmode = hdmi_setcecrxmode,
		.hdmistatus = hdmi_hdmistatus,
		.hdcpkey = hdmi_hdcpkey,
		.getedid = hdmi_AppGetEdidInfo,
		.setcecla = hdmi_CECMWSetLA,
		.sendsltdata = hdmi_u4CecSendSLTData,
		.getceccmd = hdmi_CECMWGet,
		.getsltdata = hdmi_GetSLTData,
		.setceccmd = hdmi_CECMWSend,
		.cecenable = hdmi_CECMWSetEnableCEC,
		.getcecaddr = hdmi_NotifyApiCECAddress,
		.audiosetting = hdmi_audiosetting,
		.tmdsonoff = hdmi_tmdsonoff,
		.mutehdmi = vDrm_mutehdmi,
		.svpmutehdmi = vSvp_mutehdmi,
	};

	return &HDMI_DRV;
}
EXPORT_SYMBOL(HDMI_GetDriver);
#endif
