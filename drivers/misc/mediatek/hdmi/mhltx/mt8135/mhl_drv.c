#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT
/*----------------------------------------------------------------------------*/
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
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#endif

#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>

#include "inter_mhl_drv.h"
#include "mhl_ctrl.h"
#include "mhl_edid.h"
#include "mhl_hdcp.h"
#include "mhl_table.h"
#include "mhl_cbus.h"
#include "mhl_cbus_ctrl.h"
#include "mhl_avd.h"
#include "mhl_dbg.h"
#include "mt8135_mhl_reg.h"
#include "mt6397_cbus_reg.h"
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
#include "hdmi_ca.h"
#endif

unsigned int u2MhlConut = 0;

unsigned int u4MhlIntLive = 0;
unsigned int u4MhlTimerLive = 0;
unsigned int u4MhlMainLive = 0;
unsigned int u4MhlCmdLive = 0;

unsigned char u1MhlConnectState = MHL_CONNECT_DEFULT;
/*----------------------------------------------------------------------------*/
/* Debug message defination */
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* HDMI Timer */
/*----------------------------------------------------------------------------*/
size_t mhl_TmrValue[MAX_HDMI_TMR_NUMBER] = { 0 };

size_t mhl_cmd = 0xff;
HDMI_CTRL_STATE_T e_hdmi_ctrl_state = HDMI_STATE_IDLE;
HDCP_CTRL_STATE_T e_hdcp_ctrl_state = HDCP_RECEIVER_NOT_READY;
unsigned int mhl_log_on = (hdmicbuserr);
bool fgCbusStop = FALSE;
unsigned char u1ForceCbusStop = 0;
bool fgCbusFlowStop = FALSE;
unsigned char u1MhlDpi1Pattern = 0;
unsigned char u1ForceTmdsOutput = 0;
unsigned char u1MhlIgnoreRsen = 0;

bool fgMhlPowerOn = FALSE;

static struct timer_list r_mhl_timer;

static struct task_struct *mhl_int_task;
wait_queue_head_t mhl_int_wq;
atomic_t mhl_int_event = ATOMIC_INIT(0);
static int mhl_int_kthread(void *data);

static struct task_struct *mhl_main_task;
static int mhl_main_kthread(void *data);

static struct task_struct *mhl_cmd_task;
static int mhl_cmd_kthread(void *data);

void vMhlTriggerIntTask(void)
{
	atomic_set(&mhl_int_event, 1);
	wake_up_interruptible(&mhl_int_wq);
}

void vMhlIntWaitEvent(void)
{
	wait_event_interruptible(mhl_int_wq, atomic_read(&mhl_int_event));
	atomic_set(&mhl_int_event, 0);
}

/* ////////////////////////////////////////// */
static HDMI_UTIL_FUNCS hdmi_util = { 0 };

void mhl_poll_isr(unsigned long n);
void cbus_poll_isr(unsigned long n);

static void vInitAvInfoVar(void)
{
	_stAvdAVInfo.e_resolution = HDMI_VIDEO_1280x720p_50Hz;
	_stAvdAVInfo.fgHdmiOutEnable = TRUE;
	_stAvdAVInfo.fgHdmiTmdsEnable = TRUE;

	_stAvdAVInfo.bMuteHdmiAudio = FALSE;
	_stAvdAVInfo.e_video_color_space = HDMI_RGB;
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

}

/*----------------------------------------------------------------------------*/

static void mhl_set_util_funcs(const HDMI_UTIL_FUNCS *util)
{
	memcpy(&hdmi_util, util, sizeof(HDMI_UTIL_FUNCS));
}

/*----------------------------------------------------------------------------*/

static void mhl_get_params(HDMI_PARAMS *params)
{
	memset(params, 0, sizeof(HDMI_PARAMS));

	MHL_DRV_LOG("720p\n");
	params->init_config.vformat = HDMI_VIDEO_1280x720p_50Hz;
	params->init_config.aformat = HDMI_AUDIO_PCM_16bit_48000;

	params->clk_pol = HDMI_POLARITY_FALLING;
	params->de_pol = HDMI_POLARITY_RISING;
	params->vsync_pol = HDMI_POLARITY_FALLING;
	params->hsync_pol = HDMI_POLARITY_FALLING;

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
#ifndef CONFIG_MTK_HDMI_HDCP_SUPPORT
	params->NeedSwHDCP = 1;
#endif
}

static int mhl_enter(void)
{
	MHL_DRV_FUNC();
	return 0;

}

static int mhl_exit(void)
{
	MHL_DRV_FUNC();
	return 0;
}

/*----------------------------------------------------------------------------*/

static void mhl_suspend(void)
{
	MHL_DRV_FUNC();

	_stAvdAVInfo.fgHdmiTmdsEnable = 0;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);
	vHDCPReset();
	vMhlAnalogPD();
	vMhlDigitalPD();
}

/*----------------------------------------------------------------------------*/

static void mhl_resume(void)
{
	MHL_DRV_FUNC();
	_stAvdAVInfo.fgHdmiTmdsEnable = 1;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);
}

/*----------------------------------------------------------------------------*/

static int mhl_video_config(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin,
			    HDMI_VIDEO_OUTPUT_FORMAT vout)
{
	MHL_DRV_FUNC();


	pr_info("[mhl]vic=%x,cs=%x\n", vformat, _stAvdAVInfo.e_video_color_space);

	_stAvdAVInfo.fgHdmiTmdsEnable = 0;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);

	_stAvdAVInfo.e_resolution = vformat;

/*
    if(vout == HDMI_VOUT_FORMAT_YUV444)
		_stAvdAVInfo.e_video_color_space = HDMI_YCBCR_444;
    else if(vout == HDMI_VOUT_FORMAT_YUV422)
		_stAvdAVInfo.e_video_color_space = HDMI_YCBCR_422;
    else
		_stAvdAVInfo.e_video_color_space = HDMI_RGB;



	switch(vformat)
	{
		case HDMI_VIDEO_1920x1080p_60Hz:
		case HDMI_VIDEO_1920x1080p_50Hz:
		case HDMI_VIDEO_1280x720p3d_60Hz:
		case HDMI_VIDEO_1280x720p3d_50Hz:
		case HDMI_VIDEO_1920x1080p3d_24Hz:
		case HDMI_VIDEO_1920x1080p3d_23Hz:
		case HDMI_VIDEO_1920x1080i3d_60Hz:
		case HDMI_VIDEO_1920x1080i3d_50Hz:
			_stAvdAVInfo.e_video_color_space = HDMI_YCBCR_422;
			pr_info("YCbCr422\n");
			break;
		default:
			pr_info("RGB\n");
			break;
	}
	*/


	av_hdmiset(HDMI_SET_VPLL, &_stAvdAVInfo, 1);
	av_hdmiset(HDMI_SET_SOFT_NCTS, &_stAvdAVInfo, 1);
	av_hdmiset(HDMI_SET_VIDEO_RES_CHG, &_stAvdAVInfo, 1);

	switch (vformat) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
	case HDMI_VIDEO_1280x720p_60Hz:
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1280x720p_50Hz:
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
	case HDMI_VIDEO_1920x1080p_25Hz:
	case HDMI_VIDEO_1920x1080p_29Hz:
	case HDMI_VIDEO_1920x1080p_30Hz:
		vSetMhlPPmode(FALSE);
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1920x1080p_50Hz:
	case HDMI_VIDEO_1280x720p3d_60Hz:
	case HDMI_VIDEO_1280x720p3d_50Hz:
	case HDMI_VIDEO_1920x1080p3d_24Hz:
	case HDMI_VIDEO_1920x1080p3d_23Hz:
	case HDMI_VIDEO_1920x1080i3d_60Hz:
	case HDMI_VIDEO_1920x1080i3d_50Hz:
		vSetMhlPPmode(TRUE);
		break;
	default:
		pr_info("abist can not support resolution\n");
		break;
	}

	_stAvdAVInfo.fgHdmiTmdsEnable = 1;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);

	/*
	   switch(vformat)
	   {
	   case HDMI_VIDEO_720x480p_60Hz:
	   case HDMI_VIDEO_720x576p_50Hz:
	   case HDMI_VIDEO_1280x720p_60Hz:
	   case HDMI_VIDEO_1920x1080i_60Hz:
	   case HDMI_VIDEO_1280x720p_50Hz:
	   case HDMI_VIDEO_1920x1080i_50Hz:
	   case HDMI_VIDEO_1920x1080p_24Hz:
	   case HDMI_VIDEO_1920x1080p_23Hz:
	   case HDMI_VIDEO_1920x1080p_25Hz:
	   case HDMI_VIDEO_1920x1080p_29Hz:
	   case HDMI_VIDEO_1920x1080p_30Hz:
	   av_hdmiset(HDMI_SET_HDCP_INITIAL_AUTH, &_stAvdAVInfo, 1);
	   break;
	   case HDMI_VIDEO_1920x1080p_60Hz:
	   case HDMI_VIDEO_1920x1080p_50Hz:
	   case HDMI_VIDEO_1280x720p3d_60Hz:
	   case HDMI_VIDEO_1280x720p3d_50Hz:
	   case HDMI_VIDEO_1920x1080p3d_24Hz:
	   case HDMI_VIDEO_1920x1080p3d_23Hz:
	   case HDMI_VIDEO_1920x1080i3d_60Hz:
	   case HDMI_VIDEO_1920x1080i3d_50Hz:
	   vDisableHDCP(TRUE);
	   break;
	   default:
	   break;
	   }
	 */
	av_hdmiset(HDMI_SET_HDCP_INITIAL_AUTH, &_stAvdAVInfo, 1);


	return 0;
}

/*----------------------------------------------------------------------------*/

static int mhl_audio_config(HDMI_AUDIO_FORMAT aformat)
{
	MHL_DRV_FUNC();

	return 0;
}

static int mhl_audiosetting(HDMIDRV_AUDIO_PARA *audio_para)
{
	MHL_DRV_FUNC();

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

	MHL_DRV_LOG("e_hdmi_aud_in=%d,e_iec_frame=%d,e_hdmi_fs=%d\n", _stAvdAVInfo.e_hdmi_aud_in,
		    _stAvdAVInfo.e_iec_frame, _stAvdAVInfo.e_hdmi_fs);
	MHL_DRV_LOG("e_aud_code=%d,u1Aud_Input_Chan_Cnt=%d,e_I2sFmt=%d\n", _stAvdAVInfo.e_aud_code,
		    _stAvdAVInfo.u1Aud_Input_Chan_Cnt, _stAvdAVInfo.e_I2sFmt);
	MHL_DRV_LOG("u1HdmiI2sMclk=%d\n", _stAvdAVInfo.u1HdmiI2sMclk);

	MHL_DRV_LOG("bhdmiLChstatus0=%d\n", _stAvdAVInfo.bhdmiLChstatus[0]);
	MHL_DRV_LOG("bhdmiLChstatus1=%d\n", _stAvdAVInfo.bhdmiLChstatus[1]);
	MHL_DRV_LOG("bhdmiLChstatus2=%d\n", _stAvdAVInfo.bhdmiLChstatus[2]);
	MHL_DRV_LOG("bhdmiLChstatus3=%d\n", _stAvdAVInfo.bhdmiLChstatus[3]);
	MHL_DRV_LOG("bhdmiLChstatus4=%d\n", _stAvdAVInfo.bhdmiLChstatus[4]);
	MHL_DRV_LOG("bhdmiRChstatus0=%d\n", _stAvdAVInfo.bhdmiRChstatus[0]);
	MHL_DRV_LOG("bhdmiRChstatus1=%d\n", _stAvdAVInfo.bhdmiRChstatus[1]);
	MHL_DRV_LOG("bhdmiRChstatus2=%d\n", _stAvdAVInfo.bhdmiRChstatus[2]);
	MHL_DRV_LOG("bhdmiRChstatus3=%d\n", _stAvdAVInfo.bhdmiRChstatus[3]);
	MHL_DRV_LOG("bhdmiRChstatus4=%d\n", _stAvdAVInfo.bhdmiRChstatus[4]);

	return 0;
}

/*----------------------------------------------------------------------------*/

static int mhl_video_enable(bool enable)
{
	MHL_DRV_FUNC();

	return 0;
}

/*----------------------------------------------------------------------------*/

static int mhl_audio_enable(bool enable)
{
	MHL_DRV_FUNC();

	return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

void mt8193_set_mode(unsigned char ucMode)
{
	MHL_DRV_FUNC();
	/* vSetClk(); */

}

/*----------------------------------------------------------------------------*/

int mhl_power_on(void)
{
	MHL_DRV_FUNC();
	/* init cbus */
	u1MhlConnectState = MHL_CONNECT_DEFULT;
	vCbusCtrlInit();
	vInitAvInfoVar();
	vMhlInit();
	vCbusReset();
	vCbusInit();
	vCbusIntEnable();
	/* vCbusStart(); */
	fgMhlPowerOn = TRUE;

	return 0;
}

/*----------------------------------------------------------------------------*/

void mhl_power_off(void)
{
	MHL_DRV_FUNC();
	fgMhlPowerOn = FALSE;
	vCbusIntDisable();
	vCbusReset();
	vCbusCtrlInit();
	vHDCPReset();
	vMhlAnalogPD();
}

/*----------------------------------------------------------------------------*/

void mhl_dump(void)
{
	MHL_DRV_FUNC();
	/* mt8193_dump_reg(); */
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

HDMI_STATE mhl_get_state(void)
{
	if (u1MhlConnectState == MHL_CONNECT) {
		return HDMI_STATE_ACTIVE;
	} else {
		return HDMI_STATE_NO_DEVICE;
	}
}

/*----------------------------------------------------------------------------*/


void mhl_log_enable(unsigned short enable)
{
	MHL_DRV_FUNC();

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

		pr_info("hdmi_all_log =   0xffff\n");

	}

	mhl_log_on = enable;

}

/*----------------------------------------------------------------------------*/

void mhl_enablehdcp(unsigned char u1hdcponoff)
{
	MHL_DRV_FUNC();
	_stAvdAVInfo.u1hdcponoff = u1hdcponoff;
	av_hdmiset(HDMI_SET_HDCP_OFF, &_stAvdAVInfo, 1);
}

int mhl_tmds_onoff(unsigned char u1data)
{
	MHL_DRV_LOG("tmds onoff=%d\n", u1data);
	_stAvdAVInfo.fgHdmiTmdsEnable = u1data;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);
	return 0;
}

void mhl_write(unsigned int u4Reg, unsigned int u4Data)
{
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	if ((u4Reg & 0xFFFFF000) == 0) {
		if (u4Reg < 0x100)
			vCaDPI1WriteReg(u4Reg, u4Data);
		else
			vCaHDMIWriteReg(u4Reg, u4Data);
	} else
		(*((volatile unsigned int *)u4Reg)) = u4Data;
#else
	(*((volatile unsigned int *)u4Reg)) = u4Data;
#endif
	pr_info("%08X:%08X\n", u4Reg, u4Data);
}

void mhl_read(unsigned int u4Reg, unsigned int *pdata)
{
#if 0
	unsigned int addr, u4data;

	for (addr = u4Reg; addr < u4Reg + 4 * u4Len; addr = addr + 4) {
		if ((addr & 0x0F) == 0) {
			pr_info("\n");
			pr_info("%08X : ", addr);
		}
		u4data = (*((volatile unsigned int *)addr));
		pr_info("%08X ", u4data);
	}
#else
	*pdata = (*((volatile unsigned int *)u4Reg));
	pr_info("[MHL]%08X : %08X\n", u4Reg, *pdata);
#endif
}

void vDump6397(void)
{
	v6397DumpReg();
}

void vWrite6397(unsigned int u4Addr, unsigned int u4Data)
{
	unsigned int tmp;
	vWriteCbus(u4Addr, u4Data);
	tmp = u4ReadCbus(u4Addr);
	pr_info("%08X:%08X\n", u4Addr, tmp);
}

void vRead6397(unsigned int u4Addr, unsigned int *pdata)
{
#if 0
	unsigned int tmp, addr;

	pr_info("\n");

	for (addr = 0; addr < u4Data; addr = addr + 4) {
		if ((addr & 0x0F) == 0) {
			pr_info("\n");
			pr_info("%08X : ", u4Addr + addr);
		}
		tmp = u4ReadCbus(u4Addr + addr);
		pr_info("%08X ", tmp);
	}
	pr_info("\n");
#else
	*pdata = u4ReadCbus(u4Addr);
	pr_info("[MHL]%08X : %08X\n", u4Addr, *pdata);
#endif

}

void mhl_colordeep(unsigned char u1colorspace)
{
	MHL_DRV_FUNC();
	if (u1colorspace == 0xff) {
		pr_info("color_space:HDMI_YCBCR_444 = 2\n");
		pr_info("color_space:HDMI_YCBCR_422 = 3\n");

		return;
	}

	_stAvdAVInfo.e_video_color_space = u1colorspace;
}

void vCBusStatus(void)
{
	pr_info("fgMhlPowerOn=%d\n", fgMhlPowerOn);
	vCbusLinkStatus();
	vCbusCmdStatus();
}

unsigned char vMhlConnectStatus(void)
{
	return u1MhlConnectState;
}

void vMhlConnectCallback(unsigned char u1Connected)
{
	u1MhlConnectState = u1Connected;

	if (fgMhlPowerOn) {
		if (u1Connected)
			hdmi_util.state_callback(HDMI_STATE_ACTIVE);
		else
			hdmi_util.state_callback(HDMI_STATE_NO_DEVICE);

		MHL_DRV_LOG("MHL Connected : %d\n", u1Connected);
	} else {
		MHL_DRV_LOG("MHL power off, ignore callback\n");
	}
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void mhl_set_mode(unsigned char ucMode)
{
	MHL_DRV_FUNC();
	/* vSetClk(); */
}

static int mhl_int_kthread(void *data)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_SCRN_UPDATE };
	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		vMhlIntWaitEvent();

		if (fgMhlPowerOn) {
			vMhlIntProcess();
			upmu_set_rg_int_en_hdmi_sifm(1);
		}

		u4MhlIntLive++;

		if (kthread_should_stop())
			break;
	}
	return 0;
}

static int mhl_main_kthread(void *data)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_KSDIOIRQ };
	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		if (fgMhlPowerOn) {
			if (u1ForceCbusStop == 0xA5) {
				while (fgCbusStop) {
					msleep(MHL_LINK_TIME);
				}
			}

			msleep(MHL_LINK_TIME);

			vCbusConnectState();
		} else {
			msleep(MHL_POWER_OFF_TIME);
		}

		u4MhlMainLive++;

		if (kthread_should_stop())
			break;
	}
	return 0;
}

static int mhl_cmd_kthread(void *data)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_KSDIOIRQ };
	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		if (fgMhlPowerOn) {
			if (u1ForceCbusStop == 0xA5) {
				while (fgCbusStop) {
					msleep(MHL_LINK_TIME);
				}
			}

			msleep(MHL_LINK_TIME);

			vCbusCmdState();
			if (fgIsCbusContentOn()) {
				if (((e_hdcp_ctrl_state == HDCP_WAIT_RI)
				     || (e_hdcp_ctrl_state == HDCP_CHECK_LINK_INTEGRITY))) {
					if (bCheckHDCPStatus(HDCP_STA_RI_RDY)) {
						vSetHDCPState(HDCP_CHECK_LINK_INTEGRITY);
						vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
					}
				}

				if (mhl_cmd == HDMI_HDCP_PROTOCAL_CMD) {
					vClearHdmiCmd();
					HdcpService(e_hdcp_ctrl_state);
				}

			}


		} else {
			msleep(MHL_POWER_OFF_TIME);
		}

		u4MhlCmdLive++;

		if (kthread_should_stop())
			break;
	}
	return 0;
}

/* 10ms timer */
unsigned int u4MhlRsenBak0 = 0;
unsigned int u4MhlRsenBak1 = 0xff;
unsigned int u4MhlRsenCount0 = 0;
unsigned int u4MhlRsenCount1 = 0;

void mhl_poll_isr(unsigned long n)
{
	unsigned int i;

	if (fgMhlPowerOn) {
		vCbusTimer();

		if (bCheckPordHotPlug())
			u4MhlRsenBak0 = 1;
		else
			u4MhlRsenBak0 = 0;

		/* check Rsen */
		if (u4MhlRsenBak1 != u4MhlRsenBak0) {
			u4MhlRsenCount0 = 0;
			u4MhlRsenCount1 = 0;
		} else {
			if (u4MhlRsenBak0 == 0) {
				if (u4MhlRsenCount0 < (300 / MHL_LINK_TIME)) {
					u4MhlRsenCount0++;
				}
				u4MhlRsenCount1 = 0;
			} else {
				if (u4MhlRsenCount1 < (300 / MHL_LINK_TIME)) {
					u4MhlRsenCount1++;
				}
				u4MhlRsenCount0 = 0;
			}
		}
		u4MhlRsenBak1 = u4MhlRsenBak0;

		if (u1MhlIgnoreRsen != 0xa5) {
			if (u4MhlRsenCount0 > (110 / MHL_LINK_TIME)) {
				if (fgGetMhlRsen() == TRUE) {
					vHDCPReset();
					vMhlAnalogPD();
					pr_info("[mhl]rsen off\n");
				}
				vSetMhlRsen(FALSE);
			}
			if (u4MhlRsenCount1 > (110 / MHL_LINK_TIME)) {
				if (fgGetMhlRsen() == FALSE) {
					pr_info("[mhl]rsen on\n");
				}
				vSetMhlRsen(TRUE);
			}
		}

		if (mhl_log_on & 0x10000) {
			pr_info("[mhl]rs:%d\n", u4MhlRsenBak0);
		}

		for (i = 0; i < MAX_HDMI_TMR_NUMBER; i++) {
			if (mhl_TmrValue[i] >= MHL_LINK_TIME) {
				mhl_TmrValue[i] -= MHL_LINK_TIME;
				if ((i == HDMI_HDCP_PROTOCAL_CMD)
				    && (mhl_TmrValue[HDMI_HDCP_PROTOCAL_CMD] == 0))
					vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			} else if (mhl_TmrValue[i] > 0) {
				mhl_TmrValue[i] = 0;
				if ((i == HDMI_HDCP_PROTOCAL_CMD)
				    && (mhl_TmrValue[HDMI_HDCP_PROTOCAL_CMD] == 0))
					vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			}
		}

		/* for test */
		u4MhlTimerLive++;
		mod_timer(&r_mhl_timer, jiffies + MHL_LINK_TIME / (1000 / HZ));
	} else {
		/* for test */
		u4MhlTimerLive++;
		mod_timer(&r_mhl_timer, jiffies + MHL_POWER_OFF_TIME / (1000 / HZ));
	}
}

void mhl_cmd_proc(unsigned int u4Cmd, unsigned int u4Para, unsigned int u4Para1,
		  unsigned int u4Para2)
{
	switch (u4Cmd) {
	case 0:
		pr_info("[mhl]log setting:%08X\n", u4Para);
		mhl_log_on = u4Para;
		break;
	case 1:
		pr_info("[mhl]cbus cmd debug mode by waveform\n");
		fgCbusStop = FALSE;
		u1ForceCbusStop = u4Para;
		break;
	case 2:
		pr_info("[mhl]cbus float stop\n");
		fgCbusFlowStop = u4Para;
		break;
	case 3:
		pr_info("[mhl]pattern\n");
		u1MhlDpi1Pattern = u4Para;
		break;
	case 4:
		pr_info("[mhl]force tmds output(0xa5)\n");
		u1ForceTmdsOutput = u4Para;
		break;
	case 5:
		pr_info("[mhl]rsen force on\n");
		if (u4Para == 0xa5)
			u1MhlIgnoreRsen = 0xa5;
		else
			u1MhlIgnoreRsen = 0;
		break;
	case 6:
		mhl_InfoframeSetting(u4Para, 0);
		break;
	case 7:
		mhl_InfoframeSetting(u4Para, 1);
		break;
	case 8:
		av_hdmiset(HDMI_SET_HDCP_INITIAL_AUTH, &_stAvdAVInfo, 1);
		break;
	case 0x100:
		if (u4Para == 0)
			_stAvdAVInfo.ui1_aud_out_ch_number = 2;
		else if (u4Para == 6)
			_stAvdAVInfo.ui1_aud_out_ch_number = 6;
		else if (u4Para == 8)
			_stAvdAVInfo.ui1_aud_out_ch_number = 8;

		if (u4Para1 == 0)
			_stAvdAVInfo.e_hdmi_fs = HDMI_FS_32K;
		else if (u4Para1 == 1)
			_stAvdAVInfo.e_hdmi_fs = HDMI_FS_44K;
		else if (u4Para1 == 2)
			_stAvdAVInfo.e_hdmi_fs = HDMI_FS_48K;
		else if (u4Para1 == 3)
			_stAvdAVInfo.e_hdmi_fs = HDMI_FS_88K;
		else if (u4Para1 == 4)
			_stAvdAVInfo.e_hdmi_fs = HDMI_FS_96K;
		else if (u4Para1 == 5)
			_stAvdAVInfo.e_hdmi_fs = HDMI_FS_176K;
		else if (u4Para1 == 6)
			_stAvdAVInfo.e_hdmi_fs = HDMI_FS_192K;

		break;
	default:
		pr_info("[MHL]can not support cmd\n");
		break;
	}
}

static int mhl_init(void)
{
	init_waitqueue_head(&mhl_int_wq);
	atomic_set(&mhl_int_event, 0);

	memset((void *)&r_mhl_timer, 0, sizeof(r_mhl_timer));
	r_mhl_timer.expires = jiffies + MHL_LINK_TIME / (1000 / HZ);	/* 10ms */
	r_mhl_timer.function = mhl_poll_isr;
	r_mhl_timer.data = 0;
	init_timer(&r_mhl_timer);
	add_timer(&r_mhl_timer);

	mhl_int_task = kthread_create(mhl_int_kthread, NULL, "mhl_int_kthread");
	if (!IS_ERR(mhl_int_task))
		wake_up_process(mhl_int_task);

	mhl_main_task = kthread_create(mhl_main_kthread, NULL, "mhl_main_kthread");
	if (!IS_ERR(mhl_main_task))
		wake_up_process(mhl_main_task);

	mhl_cmd_task = kthread_create(mhl_cmd_kthread, NULL, "mhl_cmd_kthread");
	if (!IS_ERR(mhl_cmd_task))
		wake_up_process(mhl_cmd_task);

#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	pr_info("[HDMI]fgCaHDMICreate\n");
	fgCaHDMICreate();
#endif


	return 0;
}

const HDMI_DRIVER *HDMI_GetDriver(void)
{
	static const HDMI_DRIVER HDMI_DRV = {
		.set_util_funcs = mhl_set_util_funcs,
		.get_params = mhl_get_params,
		.init = mhl_init,
		.enter = mhl_enter,
		.exit = mhl_exit,
		.suspend = mhl_suspend,
		.resume = mhl_resume,
		.video_config = mhl_video_config,
		.audio_config = mhl_audio_config,
		.video_enable = mhl_video_enable,
		.audio_enable = mhl_audio_enable,
		.power_on = mhl_power_on,
		.power_off = mhl_power_off,
		.set_mode = mhl_set_mode,
		.dump = mhl_dump,
		.read = mhl_read,
		.write = mhl_write,
		.get_state = mhl_get_state,
		.log_enable = mhl_log_enable,

		.InfoframeSetting = mhl_InfoframeSetting,
		.checkedid = mhl_checedid,
		.enablehdcp = mhl_enablehdcp,
		.hdmistatus = mhl_status,
		.hdcpkey = mhl_hdcpkey,
		.getedid = mhl_AppGetEdidInfo,
		.getdcapdata = mhl_AppGetDcapData,
		.get3dinfo = mhl_AppGet3DInfo,

		.mhl_cmd = mhl_cmd_proc,

		.dump6397 = vDump6397,
		.write6397 = vWrite6397,
		.cbusstatus = vCBusStatus,
		.read6397 = vRead6397,
		.audiosetting = mhl_audiosetting,
		.colordeep = mhl_colordeep,
		.mutehdmi = vDrm_mutehdmi,
		.tmdsonoff = mhl_tmds_onoff,
		.svpmutehdmi = vSvp_mutehdmi,
	};

	return &HDMI_DRV;
}
EXPORT_SYMBOL(HDMI_GetDriver);
#endif
