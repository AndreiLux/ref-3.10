#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT
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

#include "mhl_cbus.h"
#include "mhl_cbus_ctrl.h"
#include "mhl_dbg.h"
#include "mt8135_mhl_reg.h"
#include "mt6397_cbus_reg.h"
#include "mhl_ctrl.h"
#include "mhl_keycode.h"

unsigned char u1CbusConnectState = CBUS_LINK_STATE_USB_MODE;
unsigned char u1CbusConnectStateBak = 0xFF;

unsigned int u2MhlTimerOutCount = 0;
unsigned int u2MhlDcapReadDelay = 0;
unsigned int u2MhlEdidReadDelay = 0;
unsigned int u2MhlReqDelay = 0;
unsigned int u2MhlRqdDelayResp = 0;


void vCbusInitDevice(void);


void vSetMhlVbus(bool fgEn)
{

}

void vSetMhlPPmode(bool isPP)
{
	if (isPP)
		u1MyDeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE] =
		    (u1MyDeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE] &
		     (~CBUS_MSC_STATUS_LINK_MODE_CLK_MODE)) |
		    CBUS_MSC_STATUS_LINK_MODE_CLK_MODE__PacketPixel;
	else
		u1MyDeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE] =
		    (u1MyDeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE] &
		     (~CBUS_MSC_STATUS_LINK_MODE_CLK_MODE)) |
		    CBUS_MSC_STATUS_LINK_MODE_CLK_MODE__Normal;
	stMhlDev.fgMyIsPPModeChg = TRUE;
}

bool fgIsCbusConnected(void)
{
	if ((u1CbusConnectState == CBUS_LINK_STATE_CHECK_RENSE)
	    || (u1CbusConnectState == CBUS_LINK_STATE_CONNECTED)
	    || (u1CbusConnectState == CBUS_LINK_STATE_CONTENT_ON)
	    )
		return TRUE;
	return FALSE;
}

bool fgIsCbusContentOn(void)
{
	if (u1CbusConnectState == CBUS_LINK_STATE_CONTENT_ON)
		return TRUE;
	return FALSE;
}

bool fgGetMhlRsen(void)
{
	return stMhlDev.fgIsRense;
}

void vSetMhlRsen(bool fgRsen)
{
	stMhlDev.fgIsRense = fgRsen;
}

extern bool fgCbusMscAbort;
unsigned char u1CbusValueBak = 0xff;
void vCbusConnectState(void)
{
	unsigned char tmp;

	switch (u1CbusConnectState) {
	case CBUS_LINK_STATE_USB_MODE:
		vSetMHLUSBMode(1);
		tmp = fgCbusValue();
		if (u1CbusValueBak != tmp) {
			if (tmp == 0) {
				vSetMHLUSBMode(0);
				u1CbusConnectState = CBUS_LINK_STATE_CHECK_1K;
			}
			pr_info("[mhl]id:%d\n", tmp);
		}
		u1CbusValueBak = tmp;
		u2MhlTimerOutCount = 0;
		break;
	case CBUS_LINK_STATE_CHECK_1K:
		if (u2MhlTimerOutCount > (50 / MHL_LINK_TIME)) {
			vWriteCbusMsk(CBUS_LINK_07, 0,
				      (LINK_07_CBUS_PULL_DOWN_MASK |
				       LINK_07_CBUS_PULL_DOWN_SW_MASK));
			vCbusReset();
			pr_info("[mhl]trigger 1K\n");
			vCbusTriggerCheck1K();
			u1CbusHw1KStatus = 0;
			vCbusCheck1KFinish();
			/* check 1K */
			if (fgCbusCheck1K()) {
				MHL_CBUS_LOG("wakeup\n");
				/* wait discovry */
				u1CbusHwWakeupStatus = 0;
				msleep(500);
				vCbusInitDevice();
				vCbusRxEnable();
				vCbusWakeupFinish();
				if (fgCbusDiscovery())	/* connected */
				{
					u1CbusConnectState = CBUS_LINK_STATE_CHECK_RENSE;
					u2MhlReqDelay = 0;
					/* vCbusRxEnable(); */
					MHL_CBUS_LOG("discovery\n");
				} else
					u1CbusConnectState = CBUS_LINK_STATE_CHECK_1K;

				vSetCbusBit(CBUS_LINK_07, LINK_07_DETECT_AGAIN_MASK);
			} else
				u1CbusConnectState = CBUS_LINK_STATE_USB_MODE;
			u2MhlTimerOutCount = 0;
		}
		break;
	case CBUS_LINK_STATE_CHECK_RENSE:
		if (fgGetMhlRsen() == TRUE) {
			pr_info("[mhl]Rsen 1\n");
			stMhlDev.fgMyIsDcapChg = TRUE;
			u1CbusConnectState = CBUS_LINK_STATE_CONNECTED;
		}
		if (u2MhlTimerOutCount > (350 / MHL_LINK_TIME))	/* check rense time out */
		{
			MHL_CBUS_ERR("To detect Rsen 1,but time out\n");
			vWriteCbusMsk(CBUS_LINK_07,
				      (LINK_07_CBUS_PULL_DOWN_MASK |
				       LINK_07_CBUS_PULL_DOWN_SW_MASK),
				      (LINK_07_CBUS_PULL_DOWN_MASK |
				       LINK_07_CBUS_PULL_DOWN_SW_MASK));
			u2MhlTimerOutCount = 0;
			u1CbusConnectState = CBUS_LINK_FLOAT_CBUS;
			vCbusReset();
		}
		break;
	case CBUS_LINK_STATE_CONNECTED:
		if (u1DeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE] & CBUS_MSC_STATUS_LINK_MODE_PATH_EN) {
			stMhlDev.fgMyIsPathEnChg = TRUE;
			u1MyDeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE] |=
			    CBUS_MSC_STATUS_LINK_MODE_PATH_EN;
			u1CbusConnectState = CBUS_LINK_STATE_CONTENT_ON;
		}

		if (fgGetMhlRsen() == FALSE) {
			MHL_CBUS_LOG("Detect Rsen 0,disconnect(connected)\n");

			vWriteCbusMsk(CBUS_LINK_07,
				      (LINK_07_CBUS_PULL_DOWN_MASK |
				       LINK_07_CBUS_PULL_DOWN_SW_MASK),
				      (LINK_07_CBUS_PULL_DOWN_MASK |
				       LINK_07_CBUS_PULL_DOWN_SW_MASK));
			u2MhlTimerOutCount = 0;
			u1CbusConnectState = CBUS_LINK_FLOAT_CBUS;
			vCbusReset();
		}
		u2MhlTimerOutCount = 0;
		break;
	case CBUS_LINK_STATE_CONTENT_ON:
		/* check pow,if pow = 1, then close VBUS */
		if (u1DeviceRegSpace[CBUS_MSC_DEV_CAT] & CBUS_MSC_DEV_CAT_POW)
			vSetMhlVbus(FALSE);

		if ((u1DeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE] &
		     CBUS_MSC_STATUS_LINK_MODE_PATH_EN) == FALSE) {
			stMhlDev.fgMyIsPathEnChg = TRUE;
			u1MyDeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE] &=
			    ~CBUS_MSC_STATUS_LINK_MODE_PATH_EN;
			u1CbusConnectState = CBUS_LINK_STATE_CONNECTED;
		}
		if (fgGetMhlRsen() == FALSE) {
			MHL_CBUS_LOG("Detect Rsen 0,disconnect(content on)\n");

			vWriteCbusMsk(CBUS_LINK_07,
				      (LINK_07_CBUS_PULL_DOWN_MASK |
				       LINK_07_CBUS_PULL_DOWN_SW_MASK),
				      (LINK_07_CBUS_PULL_DOWN_MASK |
				       LINK_07_CBUS_PULL_DOWN_SW_MASK));
			u2MhlTimerOutCount = 0;
			u1CbusConnectState = CBUS_LINK_FLOAT_CBUS;
			vCbusReset();
		}
		u2MhlTimerOutCount = 0;
		break;
	case CBUS_LINK_FLOAT_CBUS:
		/* > 50ms, return to check 1K R */
		if (fgCbusFlowStop == TRUE)
			break;

		if (u2MhlTimerOutCount > (50 / MHL_LINK_TIME)) {
			vSetMhlVbus(TRUE);
			u1CbusConnectState = CBUS_LINK_STATE_CHECK_1K;
			vCbusReset();
		}
		break;
	default:
		MHL_CBUS_LOG("Link State Err\n");
		break;
	}

	if (u1CbusConnectState != u1CbusConnectStateBak) {
		u1CbusConnectStateBak = u1CbusConnectState;
		switch (u1CbusConnectState) {
		case CBUS_LINK_STATE_USB_MODE:
			MHL_CBUS_LOG("CBUS_LINK_STATE_USB_MODE\n");
			break;
		case CBUS_LINK_STATE_CHECK_1K:
			MHL_CBUS_LOG("CBUS_LINK_STATE_CHECK_1K\n");
			break;
		case CBUS_LINK_STATE_CHECK_RENSE:
			MHL_CBUS_LOG("CBUS_LINK_STATE_CHECK_RENSE\n");
			break;
		case CBUS_LINK_STATE_CONNECTED:
			MHL_CBUS_LOG("CBUS_LINK_STATE_CONNECTED\n");
			break;
		case CBUS_LINK_STATE_CONTENT_ON:
			MHL_CBUS_LOG("CBUS_LINK_STATE_CONTENT_ON\n");
			break;
		case CBUS_LINK_FLOAT_CBUS:
			MHL_CBUS_LOG("CBUS_LINK_FLOAT_CBUS\n");
			break;
		default:
			break;
		}
	}

	if (((stMhlDev.fgIsHPD == FALSE)
	     || (fgGetMhlRsen() == FALSE)
	     || (stMhlDev.fgIsRAP == FALSE)
	     || ((u1DeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE] & CBUS_MSC_STATUS_LINK_MODE_PATH_EN)
		 == FALSE)
	    )
	    && (vMhlConnectStatus() != MHL_NO_CONNECT)) {
		if (u1ForceTmdsOutput == 0xa5)
			vMhlSignalOff(0);	/* tmds off */
		else
			vMhlSignalOff(1);	/* tmds off */
		vMhlConnectCallback(MHL_NO_CONNECT);
		pr_info("[mhl]no connect\n");
	}
	if ((stMhlDev.fgIsHPD == TRUE)
	    && (fgGetMhlRsen() == TRUE)
	    && (stMhlDev.fgIsEdidChg == FALSE)
	    && (stMhlDev.fgIsDCapChg == FALSE)
	    && (stMhlDev.fgIsRAP == TRUE)
	    && (u1DeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE] & CBUS_MSC_STATUS_LINK_MODE_PATH_EN)
	    && (vMhlConnectStatus() != MHL_CONNECT)) {
		vMhlConnectCallback(MHL_CONNECT);
		pr_info("[mhl]connect\n");
	}
}

void vCbusCmdState(void)
{

	if (fgIsCbusConnected() && (u1CbusMscAbortDelay2S == 0)) {
		if ((stMhlDev.fgMyIsPathEnChg)
		    && fgCbusMscIdle()
		    && fgCbusDdcIdle()) {
			stMhlDev.fgMyIsPathEnChg = FALSE;
			if (fgCbusReqWriteStatCmd
			    (CBUS_MSC_STATUS_LINK_MODE,
			     u1MyDeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE]) == FALSE)
				MHL_CBUS_ERR("set path en fail\n");
		} else if ((stMhlDev.fgIsDCapChg)
			   && fgCbusMscIdle()
			   && fgCbusDdcIdle())	/* get device cap */
		{
			MHL_CBUS_LOG("get dcap\n");
			if (fgMhlGetDevCap() == FALSE)
				MHL_CBUS_ERR("get dcap fail\n");
			stMhlDev.fgIsDCapChg = FALSE;
		} else if ((stMhlDev.fgMyIsDcapChg)
			   && fgCbusMscIdle()
			   && fgCbusDdcIdle()) {
			MHL_CBUS_LOG("set dcap int\n");
			if (fgCbusReqWriteStatCmd
			    (CBUS_MSC_STATUS_CONNECTED_RDY,
			     u1MyDeviceRegSpace[CBUS_MSC_STATUS_CONNECTED_RDY]) == FALSE)
				MHL_CBUS_ERR("set dcap rdy fail\n");
			if (fgCbusReqWriteStatCmd
			    (CBUS_MSC_RCHANGE_INT, CBUS_MSC_RCHANGE_INT_DCAP_CHG) == FALSE)
				MHL_CBUS_ERR("set dcap int fail\n");
			stMhlDev.fgMyIsDcapChg = FALSE;
		} else if ((stMhlDev.fgMyIsPPModeChg)
			   && fgCbusMscIdle()
			   && fgCbusDdcIdle())	/* set mhl mode */
		{
			stMhlDev.fgMyIsPPModeChg = FALSE;
			if (fgCbusReqWriteStatCmd
			    (CBUS_MSC_STATUS_LINK_MODE,
			     u1MyDeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE]) == FALSE)
				MHL_CBUS_ERR("set PP fail\n");
		} else if (fgCbusMscAbort && fgCbusMscIdle()
			   && fgCbusDdcIdle())	/* get error code */
		{
			fgCbusMscAbort = FALSE;
			/* fgCbusReqMscERRCodeCmd(); */
		} else if ((stMhlDev.fgIsDScrChg)
			   && fgCbusMscIdle()
			   && fgCbusDdcIdle()) {
			stMhlDev.fgIsDScrChg = FALSE;
			MHL_CBUS_LOG("IsDScrChg\n");
		} else if ((stMhlDev.fgIsReqWrt)
			   && fgCbusMscIdle()
			   && fgCbusDdcIdle()) {
			/* please check spec2.0 figure 7-61 */
			stMhlDev.fgIsReqWrt = FALSE;
			fgCbusReqWriteStatCmd(CBUS_MSC_RCHANGE_INT, CBUS_MSC_RCHANGE_INT_GRT_WRT);
		}
#ifdef MHL_SUPPORT_SPEC_20
		else if ((stMhlDev.fgIs3DReq)
			 && fgCbusMscIdle()
			 && fgCbusDdcIdle()) {
			if (u1DeviceRegSpace[CBUS_MHL_VERSION] == 0x20) {
				fgCbusReqWriteStatCmd(CBUS_MSC_RCHANGE_INT,
						      CBUS_MSC_RCHANGE_INT_3D_REQ);
			}
			stMhlDev.fgIs3DReq = FALSE;
		}
#endif
		else if ((stCbusMscMsg.fgIsValid)
			 && fgCbusMscIdle()
			 && fgCbusDdcIdle()) {
			if (fgIsMscMsg(stCbusMscMsg.u2Code) == FALSE) {
				fgCbusReqMscMsgCmd(MHL_MSC_MSG_MSGE, 1);
			} else if ((stCbusMscMsg.u2Code & 0xff) == (MHL_MSC_MSG_RCP & 0xff)) {
				fgCbusReqMscMsgCmd(MHL_MSC_MSG_RCPK, stCbusMscMsg.u1Val);
				input_report_mhl_rcp_key(stCbusMscMsg.u1Val);
			} else if ((stCbusMscMsg.u2Code & 0xff) == (MHL_MSC_MSG_RAP & 0xff)) {
				fgCbusReqMscMsgCmd(MHL_MSC_MSG_RAPK, 0);
				if (stCbusMscMsg.u1Val == 0x10)	/* content on */
				{
					stMhlDev.fgIsRAP = TRUE;
				} else if (stCbusMscMsg.u1Val == 0x11)	/* content off */
				{
					stMhlDev.fgIsRAP = FALSE;
				}
			}
			stCbusMscMsg.fgIsValid = FALSE;
		}
	}

	if (fgIsCbusConnected() && (u1CbusDdcAbortDelay2S == 0)) {
		if (stMhlDev.fgIsEdidChg && fgCbusDdcIdle()
		    && fgCbusMscIdle())	/* get edid */
		{
			/* stMhlDev.fgIsEdidChg = FALSE; */
			MHL_CBUS_LOG("get edid\n");
			mhl_checedid(0);
			if (mhl_log_on & hdmicbuslog)
				vShowEdidRawData();
			stMhlDev.fgIsEdidChg = FALSE;
		}
	}

}

/* /////////////////////////////////////////////////////// */
/* / */
/* /     device */
/* / */
/* /////////////////////////////////////////////////////// */
void vCbusInitDevice(void)
{
	memset(u1MyDeviceRegSpace, 0, MHL_CBUS_DEVICE_LENGTH);
	memset(u1DeviceRegSpace, 0, MHL_CBUS_DEVICE_LENGTH);
	/* init my device */
#ifdef MHL_SUPPORT_SPEC_20
	u1MyDeviceRegSpace[0] = 0x00;	/* 0: DEV_STATE           x */
	u1MyDeviceRegSpace[1] = 0x20;	/* 1: MHL_VERSION         - */
	u1MyDeviceRegSpace[2] = 0x02;	/* 2: DEV_CAT                     - POW[4], DEV_TYPE[3:0],900mA */
	u1MyDeviceRegSpace[3] = 0x00;	/* 3: ADOPTER_ID_H */
	u1MyDeviceRegSpace[4] = 0x00;	/* 4: ADOPTER_ID_L */
	u1MyDeviceRegSpace[5] = 0x0F;	/* 5: VID_LINK_MODE       - SUPP_VGA[5], SUPP_ISLANDS[4], SUPP_PPIXEL[3], SUPP_YCBCR422[2], SUPP_YCBCR444[1], SUPP_RGB444[0] */
	u1MyDeviceRegSpace[6] = 0x03;	/* 6: AUD_LINK_MODE       - AUD_8CH[1], AUD_2CH[0] */
	u1MyDeviceRegSpace[7] = 0x00;	/* 7: VIDEO_TYPE           x SUPP_VT[7], VT_GAME[3], VT_CINEMA[2], VT_PHOTO[1], VT_GRAPHICS[0] */
	u1MyDeviceRegSpace[8] = 0x02;	/* 8: LOG_DEV_MAP         - LD_GUI[7], LD_SPEAKER[6], LD_RECORD[5], LD_TUNER[4], LD_MEDIA[3], LD_AUDIO[2], LD_VIDEO[1], LD_DISPLAY[0] */
	u1MyDeviceRegSpace[9] = 0x0F;	/* 9: BANDWIDTH            x */
	u1MyDeviceRegSpace[10] = 0x07;	/* A: FEATURE_FLAG       - UCP_RECV_SUPPORT[4], UCP_SEND_SUPPORT[3], SP_SUPPORT[2], RAP_SUPPORT[1], RCP_SUPPORT[0] */
	u1MyDeviceRegSpace[11] = 0x00;	/* B: DEVICE_ID_H         - */
	u1MyDeviceRegSpace[12] = 0x00;	/* C: DEVICE_ID_L                - */
	u1MyDeviceRegSpace[13] = 0x10;	/* D: SCRATCHPAD_SIZE    - */
	u1MyDeviceRegSpace[14] = 0x33;	/* E: INT_STAT_SIZE      - STAT_SIZE[7:4], INT_SIZE[3:0] */
	u1MyDeviceRegSpace[15] = 0x00;	/* F: Reserved */
	u1MyDeviceRegSpace[CBUS_MSC_STATUS_CONNECTED_RDY] = CBUS_MSC_STATUS_CONNECTED_RDY_DCAP_RDY;
	u1MyDeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE] = CBUS_MSC_STATUS_LINK_MODE_CLK_MODE__Normal;
#else
	u1MyDeviceRegSpace[0] = 0x00;	/* 0: DEV_STATE           x */
	u1MyDeviceRegSpace[1] = 0x12;	/* 1: MHL_VERSION         - */
	u1MyDeviceRegSpace[2] = 0x02;	/* 2: DEV_CAT             - POW[4], DEV_TYPE[3:0],900mA */
	u1MyDeviceRegSpace[3] = 0x00;	/* 3: ADOPTER_ID_H */
	u1MyDeviceRegSpace[4] = 0x00;	/* 4: ADOPTER_ID_L */
	u1MyDeviceRegSpace[5] = 0x01;	/* 5: VID_LINK_MODE       - SUPP_VGA[5], SUPP_ISLANDS[4], SUPP_PPIXEL[3], SUPP_YCBCR422[2], SUPP_YCBCR444[1], SUPP_RGB444[0] */
	u1MyDeviceRegSpace[6] = 0x03;	/* 6: AUD_LINK_MODE       - AUD_8CH[1], AUD_2CH[0] */
	u1MyDeviceRegSpace[7] = 0x00;	/* 7: VIDEO_TYPE           x SUPP_VT[7], VT_GAME[3], VT_CINEMA[2], VT_PHOTO[1], VT_GRAPHICS[0] */
	u1MyDeviceRegSpace[8] = 0x02;	/* 8: LOG_DEV_MAP         - LD_GUI[7], LD_SPEAKER[6], LD_RECORD[5], LD_TUNER[4], LD_MEDIA[3], LD_AUDIO[2], LD_VIDEO[1], LD_DISPLAY[0] */
	u1MyDeviceRegSpace[9] = 0x0F;	/* 9: BANDWIDTH            x */
	u1MyDeviceRegSpace[10] = 0x07;	/* A: FEATURE_FLAG       - UCP_RECV_SUPPORT[4], UCP_SEND_SUPPORT[3], SP_SUPPORT[2], RAP_SUPPORT[1], RCP_SUPPORT[0] */
	u1MyDeviceRegSpace[11] = 0x00;	/* B: DEVICE_ID_H         - */
	u1MyDeviceRegSpace[12] = 0x00;	/* C: DEVICE_ID_L                - */
	u1MyDeviceRegSpace[13] = 0x10;	/* D: SCRATCHPAD_SIZE    - */
	u1MyDeviceRegSpace[14] = 0x33;	/* E: INT_STAT_SIZE      - STAT_SIZE[7:4], INT_SIZE[3:0] */
	u1MyDeviceRegSpace[15] = 0x00;	/* F: Reserved */
	u1MyDeviceRegSpace[CBUS_MSC_STATUS_CONNECTED_RDY] = CBUS_MSC_STATUS_CONNECTED_RDY_DCAP_RDY;
	u1MyDeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE] = CBUS_MSC_STATUS_LINK_MODE_CLK_MODE__Normal;
#endif

	/* init sink device */
	u1DeviceRegSpace[0] = 0x00;	/* 0: DEV_STATE             x */
	u1DeviceRegSpace[1] = 0x10;	/* 1: MHL_VERSION           - */
	u1DeviceRegSpace[2] = 0x31;	/* 2: DEV_CAT                       - POW[4], DEV_TYPE[3:0],900mA */
	u1DeviceRegSpace[3] = 0x00;	/* 3: ADOPTER_ID_H */
	u1DeviceRegSpace[4] = 0x00;	/* 4: ADOPTER_ID_L */
	u1DeviceRegSpace[5] = 0x00;	/* 5: VID_LINK_MODE - SUPP_VGA[5], SUPP_ISLANDS[4], SUPP_PPIXEL[3], SUPP_YCBCR422[2], SUPP_YCBCR444[1], SUPP_RGB444[0] */
	u1DeviceRegSpace[6] = 0x01;	/* 6: AUD_LINK_MODE - AUD_8CH[1], AUD_2CH[0] */
	u1DeviceRegSpace[7] = 0x00;	/* 7: VIDEO_TYPE             x SUPP_VT[7], VT_GAME[3], VT_CINEMA[2], VT_PHOTO[1], VT_GRAPHICS[0] */
	u1DeviceRegSpace[8] = 0x06;	/* 8: LOG_DEV_MAP           - LD_GUI[7], LD_SPEAKER[6], LD_RECORD[5], LD_TUNER[4], LD_MEDIA[3], LD_AUDIO[2], LD_VIDEO[1], LD_DISPLAY[0] */
	u1DeviceRegSpace[9] = 0x0F;	/* 9: BANDWIDTH              x */
	u1DeviceRegSpace[10] = 0x04;	/* A: FEATURE_FLAG - UCP_RECV_SUPPORT[4], UCP_SEND_SUPPORT[3], SP_SUPPORT[2], RAP_SUPPORT[1], RCP_SUPPORT[0] */
	u1DeviceRegSpace[11] = 0x00;	/* B: DEVICE_ID_H           - */
	u1DeviceRegSpace[12] = 0x00;	/* C: DEVICE_ID_L          - */
	u1DeviceRegSpace[13] = 0x00;	/* D: SCRATCHPAD_SIZE      - */
	u1DeviceRegSpace[14] = 0x33;	/* E: INT_STAT_SIZE        - STAT_SIZE[7:4], INT_SIZE[3:0] */
	u1DeviceRegSpace[15] = 0x00;	/* F: Reserved */

	memset(&stMhlDev, 0, sizeof(stMhlDev_st));
	stMhlDev.fgIsRAP = TRUE;

}

void mhl_AppGetDcapInfo(stMhlDcap_st *pv_get_info)
{
	pv_get_info->u1MhlVersion = stMhlDcap.u1MhlVersion;
	pv_get_info->u1DevType = stMhlDcap.u1DevType;
	pv_get_info->fgIsPower = stMhlDcap.fgIsPower;
	pv_get_info->u1PowerCap = stMhlDcap.u1PowerCap;
	pv_get_info->u4AdopterID = stMhlDcap.u4AdopterID;
	pv_get_info->u1VidLinkMode = stMhlDcap.u1VidLinkMode;
	pv_get_info->u1AudLinkMode = stMhlDcap.u1AudLinkMode;
	pv_get_info->u1VideoType = stMhlDcap.u1VideoType;
	pv_get_info->u1LogDevMap = stMhlDcap.u1LogDevMap;
	pv_get_info->u4BandWidth = stMhlDcap.u4BandWidth;
	pv_get_info->u1FeatureFlag = stMhlDcap.u1FeatureFlag;
	pv_get_info->u4DeviceID = stMhlDcap.u4DeviceID;
	pv_get_info->u1ScratchpadSize = stMhlDcap.u1ScratchpadSize;
	pv_get_info->u1intStatSize = stMhlDcap.u1intStatSize;
}

void mhl_AppGetDcapData(unsigned char *pv_get_info)
{
	unsigned char i;
	for (i = 0; i < 16; i++)
		pv_get_info[i] = u1DeviceRegSpace[i];
}

void mhl_AppGet3DInfo(MHL_3D_INFO_T *pv_get_info)
{
	pv_get_info->ui4_sink_FP_SUP_3D_resolution =
	    SINK_720P50 | SINK_720P60 | SINK_1080P23976 | SINK_1080P24;
	pv_get_info->ui4_sink_SBS_SUP_3D_resolution = 0;
	pv_get_info->ui4_sink_TOB_SUP_3D_resolution = 0;
}

/* cbus ctrl init when power on */
void vCbusCtrlInit(void)
{
	vCbusInitDevice();
	mhl_init_rmt_input_dev();
	u1CbusConnectState = CBUS_LINK_STATE_USB_MODE;
	u1CbusConnectStateBak = 0xff;
	u1CbusValueBak = 0xff;
	vSetMHLUSBMode(1);
}

void vCbusStart(void)
{
	/* pr_info("cbus start\n"); */
	/* u1CbusConnectStateBak = 0xff; */
	/* u1CbusConnectState = CBUS_LINK_STATE_USB_MODE; */
}

void vDcapShow(void)
{
	MHL_CBUS_LOG("===== sink dcap info ========\n");
	MHL_CBUS_LOG("u1MhlVersion=%x\n", stMhlDcap.u1MhlVersion);
	MHL_CBUS_LOG("u4AdopterID=%x\n", stMhlDcap.u4AdopterID);
	MHL_CBUS_LOG("u4DeviceID=%x\n", stMhlDcap.u4DeviceID);
	MHL_CBUS_LOG("fgIsPower=%x\n", stMhlDcap.fgIsPower);

	switch (stMhlDcap.u1DevType) {
	case 1:
		MHL_CBUS_LOG("dev type is sink\n");
		break;
	case 2:
		MHL_CBUS_LOG("dev type is source\n");
		break;
	case 3:
		MHL_CBUS_LOG("dev type is dongle\n");
		break;
	default:
		MHL_CBUS_LOG("dev type is error\n");
		break;
	}

	/* video support */
	if (stMhlDcap.u1VidLinkMode & CBUS_SUPP_RGB444) {
		MHL_CBUS_LOG("CBUS_SUPP_RGB444\n");
	}
	if (stMhlDcap.u1VidLinkMode & CBUS_SUPP_YCBCR444) {
		MHL_CBUS_LOG("CBUS_SUPP_YCBCR444\n");
	}
	if (stMhlDcap.u1VidLinkMode & CBUS_SUPP_YCBCR422) {
		MHL_CBUS_LOG("CBUS_SUPP_YCBCR422\n");
	}
	if (stMhlDcap.u1VidLinkMode & CBUS_SUPP_PP) {
		MHL_CBUS_LOG("CBUS_SUPP_PP\n");
	}
	if (stMhlDcap.u1VidLinkMode & CBUS_SUPP_ISLANDS) {
		MHL_CBUS_LOG("CBUS_SUPP_ISLANDS\n");
	}
	if (stMhlDcap.u1VidLinkMode & CBUS_SUPP_VGA) {
		MHL_CBUS_LOG("CBUS_SUPP_VGA\n");
	}
	/* video support */
	if (stMhlDcap.u1LogDevMap & CBUS_LOG_DEV_MAP_LD_DISPLAY) {
		MHL_CBUS_LOG("CBUS_LOG_DEV_MAP_LD_DISPLAY\n");
	}
	if (stMhlDcap.u1LogDevMap & CBUS_LOG_DEV_MAP_LD_VIDEO) {
		MHL_CBUS_LOG("CBUS_LOG_DEV_MAP_LD_VIDEO\n");
	}
	if (stMhlDcap.u1LogDevMap & CBUS_LOG_DEV_MAP_LD_AUDIO) {
		MHL_CBUS_LOG("CBUS_LOG_DEV_MAP_LD_AUDIO\n");
	}
	if (stMhlDcap.u1LogDevMap & CBUS_LOG_DEV_MAP_LD_MEDIA) {
		MHL_CBUS_LOG("CBUS_LOG_DEV_MAP_LD_MEDIA\n");
	}
	if (stMhlDcap.u1LogDevMap & CBUS_LOG_DEV_MAP_LD_TUNER) {
		MHL_CBUS_LOG("CBUS_LOG_DEV_MAP_LD_TUNER\n");
	}
	if (stMhlDcap.u1LogDevMap & CBUS_LOG_DEV_MAP_LD_RECORD) {
		MHL_CBUS_LOG("CBUS_LOG_DEV_MAP_LD_RECORD\n");
	}
	if (stMhlDcap.u1LogDevMap & CBUS_LOG_DEV_MAP_LD_SPEAKER) {
		MHL_CBUS_LOG("CBUS_LOG_DEV_MAP_LD_SPEAKER\n");
	}
	if (stMhlDcap.u1LogDevMap & CBUS_LOG_DEV_MAP_LD_GUI) {
		MHL_CBUS_LOG("CBUS_LOG_DEV_MAP_LD_GUI\n");
	}
	/* video type */
	if (stMhlDcap.u1VideoType & VT_GRAPHICS) {
		MHL_CBUS_LOG("VT_GRAPHICS\n");
	}
	if (stMhlDcap.u1VideoType & VT_PHOTO) {
		MHL_CBUS_LOG("VT_PHOTO\n");
	}
	if (stMhlDcap.u1VideoType & VT_CINEMA) {
		MHL_CBUS_LOG("VT_CINEMA\n");
	}
	if (stMhlDcap.u1VideoType & VT_GAME) {
		MHL_CBUS_LOG("VT_GAME\n");
	}
	if (stMhlDcap.u1VideoType & VT_SUPP) {
		MHL_CBUS_LOG("VT_SUPP\n");
	}
	/* aud support */
	if (stMhlDcap.u1AudLinkMode & CBUS_SUPP_AUD_2CH) {
		MHL_CBUS_LOG("CBUS_SUPP_AUD_2CH\n");
	}
	if (stMhlDcap.u1AudLinkMode & CBUS_SUPP_AUD_8CH) {
		MHL_CBUS_LOG("CBUS_SUPP_AUD_8CH\n");
	}
	/* CBUS_BANDWIDTH */
	stMhlDcap.u4BandWidth = u1DeviceRegSpace[CBUS_BANDWIDTH] * 5;
	MHL_CBUS_LOG("MHL bandwidth = %d\n", stMhlDcap.u4BandWidth);

	/* Check Scratchpad/RAP/RCP support */
	if (stMhlDcap.u1FeatureFlag & CBUS_FEATURE_FLAG_RCP_SUPPORT) {
		MHL_CBUS_LOG("sink support RCP_SUPPORT\n");
	}
	if (stMhlDcap.u1FeatureFlag & CBUS_FEATURE_FLAG_RAP_SUPPORT) {
		MHL_CBUS_LOG("sink support RAP_SUPPORT\n");
	}
	if (stMhlDcap.u1FeatureFlag & CBUS_FEATURE_FLAG_SP_SUPPORT) {
		MHL_CBUS_LOG("sink support SP_SUPPORT\n");
	}
	if (stMhlDcap.u1FeatureFlag & CBUS_FEATURE_UCP_SEND_SUPPORT) {
		MHL_CBUS_LOG("sink support SP_SUPPORT\n");
	}
	if (stMhlDcap.u1FeatureFlag & CBUS_FEATURE_UCP_RECV_SUPPORT) {
		MHL_CBUS_LOG("sink support SP_SUPPORT\n");
	}

	MHL_CBUS_LOG("CBUS_SCRATCHPAD_SIZE  = %d\n", stMhlDcap.u1ScratchpadSize);
	MHL_CBUS_LOG("CBUS_INT_SIZE  = %d\n", stMhlDcap.u1intStatSize & 0x0F);
	MHL_CBUS_LOG("CBUS_STAT_SIZE  = %d\n", stMhlDcap.u1intStatSize >> 4);
	MHL_CBUS_LOG("===== end dcap info ========\n");
}

extern unsigned int u4MhlIntLive;
extern unsigned int u4MhlTimerLive;
extern unsigned int u4MhlMainLive;
extern unsigned int u4MhlCmdLive;

void vCbusLinkStatus(void)
{
	unsigned int i, j;
	pr_info("=========================================\n");
	pr_info("u4MhlIntLive:%x\n", u4MhlIntLive);
	pr_info("u4MhlTimerLive:%x\n", u4MhlTimerLive);
	pr_info("u4MhlMainLive:%x\n", u4MhlMainLive);
	pr_info("u4MhlCmdLive:%x\n", u4MhlCmdLive);
	pr_info("cbus link status : ");
	switch (u1CbusConnectState) {
	case CBUS_LINK_STATE_USB_MODE:
		pr_info("CBUS_LINK_STATE_USB_MODE\n");
		break;
	case CBUS_LINK_STATE_CHECK_1K:
		pr_info("CBUS_LINK_STATE_CHECK_1K\n");
		break;
	case CBUS_LINK_STATE_CHECK_RENSE:
		pr_info("CBUS_LINK_STATE_CHECK_RENSE\n");
		break;
	case CBUS_LINK_STATE_CONNECTED:
		pr_info("CBUS_LINK_STATE_CONNECTED\n");
		break;
	case CBUS_LINK_STATE_CONTENT_ON:
		pr_info("CBUS_LINK_STATE_CONTENT_ON\n");
		break;
	case CBUS_LINK_FLOAT_CBUS:
		pr_info("CBUS_LINK_FLOAT_CBUS\n");
		break;
	default:
		pr_info("error state\n");
		break;
	}

	pr_info("fgIsHPD=%d\n", stMhlDev.fgIsHPD);
	pr_info("fgIsRAP=%d\n", stMhlDev.fgIsRAP);
	pr_info("fgIsRense=%d\n", stMhlDev.fgIsRense);
	pr_info("fgIsDCapChg=%d\n", stMhlDev.fgIsDCapChg);
	pr_info("fgIsDCapRdy=%d\n", stMhlDev.fgIsDCapRdy);
	pr_info("fgIsEdidChg=%d\n", stMhlDev.fgIsEdidChg);
	pr_info("fgIsPPMode=%d\n", stMhlDev.fgIsPPMode);
	pr_info("fgIsPathEn=%d\n", stMhlDev.fgIsPathEn);
	pr_info("fgIsMuted=%d\n", stMhlDev.fgIsMuted);
	pr_info("fgIsMyPPModeChg=%d\n", stMhlDev.fgMyIsPPModeChg);
	pr_info("fgIsMyPathEnChg=%d\n", stMhlDev.fgMyIsPathEnChg);
	pr_info("fgIsMyMutedChg=%d\n", stMhlDev.fgMyIsMutedChg);

	pr_info(">>> cbus sink device reg space\n");
	for (i = 0; i < MHL_CBUS_DEVICE_LENGTH; i = i + 0x10) {
		if (i > 0x0F)
			pr_info("%2X : ", i);
		else
			pr_info("0%X : ", i);
		for (j = 0; j < 0x10; j++)
			if (u1DeviceRegSpace[i + j] > 0x0F)
				pr_info("%2X ", u1DeviceRegSpace[i + j]);
			else
				pr_info("0%X ", u1DeviceRegSpace[i + j]);
		pr_info("\n");

	}
	pr_info(">>> cbus my device reg space\n");
	for (i = 0; i < MHL_CBUS_DEVICE_LENGTH; i = i + 0x10) {
		if (i > 0x0F)
			pr_info("%2X : ", i);
		else
			pr_info("0%X : ", i);
		for (j = 0; j < 0x10; j++)
			if (u1MyDeviceRegSpace[i + j] > 0x0F)
				pr_info("%2X ", u1MyDeviceRegSpace[i + j]);
			else
				pr_info("0%X ", u1MyDeviceRegSpace[i + j]);
		pr_info("\n");
	}
	vDcapShow();

	vShowEdidRawData();
}
#endif
