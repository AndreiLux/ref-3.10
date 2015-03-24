/*----------------------------------------------------------------------------*/
#define TMFL_TDA19989
#define _tx_c_
#include <generated/autoconf.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/earlysuspend.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/vmalloc.h>
#include <linux/disp_assert_layer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/switch.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <mach/dma.h>
#include <mach/irqs.h>
#include <asm/tlbflush.h>
#include <asm/page.h>


#include "hdmi_drv.h"
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

unsigned int u2MhlConut = 0;

bool fgMhlConnectState = FALSE;
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
unsigned int mhl_log_on = 0xFFFF;

bool fgMhlPowerOn = FALSE;

static struct timer_list r_mhl_timer;
static struct timer_list r_cbus_timer;

static struct task_struct *mhl_int_task;
wait_queue_head_t mhl_int_wq;
atomic_t mhl_int_event = ATOMIC_INIT(0);
static int mhl_int_kthread(void *data);

static struct task_struct *mhl_main_task;
static int mhl_main_kthread(void *data);


void vMhlTriggerIntTask(void)
{
	/* MHL_DRV_FUNC(); */
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
	_stAvdAVInfo.e_video_color_space = HDMI_YCBCR_444;
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
}

static int mt8193_enter(void)
{
	MHL_DRV_FUNC();
	return 0;

}

static int mt8193_exit(void)
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

	printk("0:HDMI_VIDEO_720x480p_60Hz=0\n");
	printk("1:HDMI_VIDEO_720x576p_50Hz\n");
	printk("2:HDMI_VIDEO_1280x720p_60Hz\n");
	printk("3:HDMI_VIDEO_1280x720p_50Hz\n");
	printk("4:HDMI_VIDEO_1920x1080i_60Hz\n");
	printk("5:HDMI_VIDEO_1920x1080i_50Hz\n");
	printk("6:HDMI_VIDEO_1920x1080p_30Hz\n");
	printk("7:HDMI_VIDEO_1920x1080p_25Hz\n");
	printk("8:HDMI_VIDEO_1920x1080p_24Hz\n");
	printk("9:HDMI_VIDEO_1920x1080p_23Hz\n");
	printk("a:HDMI_VIDEO_1920x1080p_29Hz\n");
	printk("b:HDMI_VIDEO_1920x1080p_60Hz\n");
	printk("c:HDMI_VIDEO_1920x1080p_50Hz\n");
	printk("d:HDMI_VIDEO_1280x720p_60Hz_3D\n");
	printk("e:HDMI_VIDEO_1280x720p_50Hz_3D\n");
	printk("f:HDMI_VIDEO_1920x1080p_24Hz_3D\n");
	printk("10:HDMI_VIDEO_1920x1080p_23Hz_3D\n");

	printk("vformat=%x,vin=%x,vout=%x\n", vformat, vin, vout);

	_stAvdAVInfo.e_resolution = vformat;
	_stAvdAVInfo.fgHdmiTmdsEnable = 1;

	if (vout == HDMI_VOUT_FORMAT_YUV444)
		_stAvdAVInfo.e_video_color_space = HDMI_YCBCR_444;
	else if (vout == HDMI_VOUT_FORMAT_YUV422)
		_stAvdAVInfo.e_video_color_space = HDMI_YCBCR_422;
	else
		_stAvdAVInfo.e_video_color_space = HDMI_RGB;

	_stAvdAVInfo.e_video_color_space = HDMI_RGB;

	switch (vformat) {
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1920x1080p_50Hz:
	case HDMI_VIDEO_1280x720p_60Hz_3D:
	case HDMI_VIDEO_1280x720p_50Hz_3D:
	case HDMI_VIDEO_1920x1080p_24Hz_3D:
	case HDMI_VIDEO_1920x1080p_23Hz_3D:
		_stAvdAVInfo.e_video_color_space = HDMI_YCBCR_422;
		printk("YCbCr422\n");
		break;
	default:
		printk("RGB\n");
		break;
	}


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
	case HDMI_VIDEO_1280x720p_60Hz_3D:
	case HDMI_VIDEO_1280x720p_50Hz_3D:
	case HDMI_VIDEO_1920x1080p_24Hz_3D:
	case HDMI_VIDEO_1920x1080p_23Hz_3D:
		vSetMhlPPmode(TRUE);
		break;
	default:
		printk("abist can not support resolution\n");
		break;
	}

	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);


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
		av_hdmiset(HDMI_SET_HDCP_INITIAL_AUTH, &_stAvdAVInfo, 1);
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1920x1080p_50Hz:
	case HDMI_VIDEO_1280x720p_60Hz_3D:
	case HDMI_VIDEO_1280x720p_50Hz_3D:
	case HDMI_VIDEO_1920x1080p_24Hz_3D:
	case HDMI_VIDEO_1920x1080p_23Hz_3D:
		vDisableHDCP(TRUE);
		break;
	default:
		break;
	}
	return 0;
}

/*----------------------------------------------------------------------------*/

static int mhl_audio_config(HDMI_AUDIO_FORMAT aformat)
{
	MHL_DRV_FUNC();

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

	/* vCbusIntEnable(); */
	vCbusCtrlInit();
	vInitAvInfoVar();
	vMhlInit();
	vCbusReset();
	vCbusInit();
	vCbusStart();

	return 0;
}

/*----------------------------------------------------------------------------*/

void mhl_power_off(void)
{
	MHL_DRV_FUNC();
	vCbusReset();
	vCbusCtrlInit();
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
	MHL_DRV_FUNC();

	if (fgMhlConnectState) {
		return HDMI_STATE_ACTIVE;
	} else {
		return HDMI_STATE_NO_DEVICE;
	}
}

/*----------------------------------------------------------------------------*/


void mhl_log_enable(u16 enable)
{
	MHL_DRV_FUNC();

	if (enable == 0) {
		printk("hdmi_pll_log =   0x1\n");
		printk("hdmi_dgi_log =   0x2\n");
		printk("hdmi_plug_log =  0x4\n");
		printk("hdmi_video_log = 0x8\n");
		printk("hdmi_audio_log = 0x10\n");
		printk("hdmi_hdcp_log =  0x20\n");
		printk("hdmi_cec_log =   0x40\n");
		printk("hdmi_ddc_log =   0x80\n");
		printk("hdmi_edid_log =  0x100\n");
		printk("hdmi_drv_log =   0x200\n");

		printk("hdmi_all_log =   0xffff\n");

	}

	mhl_log_on = enable;

}

/*----------------------------------------------------------------------------*/

void mhl_enablehdcp(u8 u1hdcponoff)
{
	MHL_DRV_FUNC();
	_stAvdAVInfo.u1hdcponoff = u1hdcponoff;
	av_hdmiset(HDMI_SET_HDCP_OFF, &_stAvdAVInfo, 1);
}

void mhl_write(unsigned int u4Reg, unsigned int u4Data)
{
	(*((volatile unsigned int *)u4Reg)) = u4Data;
	printk("%08x:%08x\n", u4Reg, u4Data);
}

void mhl_read(unsigned int u4Reg, unsigned int u4Len)
{
	unsigned int addr, u4data;

	for (addr = u4Reg; addr < u4Reg + 4 * u4Len; addr = addr + 4) {
		if ((addr & 0x0F) == 0) {
			printk("\n");
			printk("%08x : ", addr);
		}
		u4data = (*((volatile unsigned int *)addr));
		printk("%08x ", u4data);
	}
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
	printk("%08x:%08x\n", u4Addr, tmp);
}

void vCBusStatus(void)
{
	vCbusLinkStatus();
	vCbusCmdStatus();
}

void vMhlConnectCallback(bool fgConnected)
{
	fgMhlConnectState = fgConnected;
/*
	if(fgConnected)
		hdmi_util.state_callback(HDMI_STATE_ACTIVE);
	else
		hdmi_util.state_callback(HDMI_STATE_NO_DEVICE);
*/
	MHL_DRV_LOG("MHL Connected : %d\n", fgConnected);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void mhl_set_mode(unsigned char ucMode)
{
	MHL_DRV_FUNC();
	/* vSetClk(); */
}

static void mhl_irq_handler(void)
{
	/* MHL_DRV_FUNC(); */
	atomic_set(&mhl_int_event, 1);
	wake_up_interruptible(&mhl_int_wq);

/* mt65xx_eint_mask(16); */
}

static int mhl_int_kthread(void *data)
{
	for (;;) {
		vMhlIntWaitEvent();

		if (fgMhlPowerOn) {
			vMhlIntProcess();
		}

		u2MhlConut = u2MhlConut + 0x10000;
		if (kthread_should_stop())
			break;
	}
	return 0;
}

static int mhl_main_kthread(void *data)
{
	unsigned char u1Data;

	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(MHL_LINK_TIME / (1000 / HZ));	/* delay 5ms */

		if (fgMhlPowerOn) {
			vCbusConnectState();
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

		}

		u2MhlConut = u2MhlConut + 1;

		if (kthread_should_stop())
			break;
	}
	return 0;
}

/* 10ms timer */
unsigned int u4MhlTest = 0;
unsigned int u4MhlRsenBak0 = 0;
unsigned int u4MhlRsenBak1 = 0;
unsigned int u4MhlRsenBak2 = 0;

void mhl_poll_isr(unsigned long n)
{
	unsigned int i;

	vCbusTimer();

	/* check Rsen */
	u4MhlRsenBak2 = u4MhlRsenBak1;
	u4MhlRsenBak1 = u4MhlRsenBak0;
	if (bCheckPordHotPlug())
		u4MhlRsenBak0 = 1;
	else
		u4MhlRsenBak0 = 0;
	if ((u4MhlRsenBak0 == 0) && (u4MhlRsenBak1 == 0) && (u4MhlRsenBak2 == 0)) {
		if (fgGetMhlRsen() == TRUE) {
			vHDCPReset();
			vMhlAnalogPD();
			printk("Rsen : 0\n");
		}
		vSetMhlRsen(FALSE);
	}
	if ((u4MhlRsenBak0 == 1) && (u4MhlRsenBak1 == 1) && (u4MhlRsenBak2 == 1)) {
		if (fgGetMhlRsen() == FALSE) {
			printk("Rsen : 1\n");
		}
		vSetMhlRsen(TRUE);
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
	u4MhlTest++;

	mod_timer(&r_mhl_timer, jiffies + MHL_LINK_TIME / (1000 / HZ));
}

void cbus_poll_isr(unsigned long n)
{
	/* for test */
	vMhlTriggerIntTask();

	mod_timer(&r_cbus_timer, jiffies + 2 / (1000 / HZ));
}

void mhl_cmd_proc(unsigned int u4Cmd, unsigned int u4Para, unsigned int u4Para1,
		  unsigned int u4Para2)
{
	switch (u4Cmd) {
	case 0:
		printk("[MHL]count : %x,%x\n", u2MhlConut, u4MhlTest);
		break;
	case 1:
		printk("[MHL]get mhl state\n");
		vCbusLinkStatus();
		vCbusCmdStatus();
		break;
	case 2:
		printk("[MHL]trigger int\n");
		atomic_set(&mhl_int_event, 1);
		wake_up_interruptible(&mhl_int_wq);
		break;
	case 3:
		break;
	case 4:
		break;
	case 5:
		mhl_InfoframeSetting(u4Para, 0);
		break;
	case 6:
		mhl_InfoframeSetting(u4Para, 1);
		break;
	case 7:
		av_hdmiset(HDMI_SET_HDCP_INITIAL_AUTH, &_stAvdAVInfo, 1);
		break;
	default:
		printk("[MHL]can not support cmd\n");
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

	memset((void *)&r_cbus_timer, 0, sizeof(r_cbus_timer));
	r_cbus_timer.expires = jiffies + 2 / (1000 / HZ);	/* 10ms */
	r_cbus_timer.function = cbus_poll_isr;
	r_cbus_timer.data = 0;
	init_timer(&r_cbus_timer);
	add_timer(&r_cbus_timer);

	mhl_int_task = kthread_create(mhl_int_kthread, NULL, "mhl_int_kthread");
	if (!IS_ERR(mhl_int_task))
		wake_up_process(mhl_int_task);

	mhl_main_task = kthread_create(mhl_main_kthread, NULL, "mhl_main_kthread");
	if (!IS_ERR(mhl_main_task))
		wake_up_process(mhl_main_task);

	return 0;
}

const HDMI_DRIVER *HDMI_GetDriver(void)
{
	static const HDMI_DRIVER HDMI_DRV = {
		.set_util_funcs = mhl_set_util_funcs,
		.get_params = mhl_get_params,
		.init = mhl_init,
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

		.mhl_cmd = mhl_cmd_proc,

		.dump6397 = vDump6397,
		.write6397 = vWrite6397,
		.cbusstatus = vCBusStatus,
	};

	return &HDMI_DRV;
}
EXPORT_SYMBOL(HDMI_GetDriver);
