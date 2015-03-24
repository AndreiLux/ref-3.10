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



#include "mhl_cbus.h"
#include "mhl_cbus_ctrl.h"
#include "mhl_dbg.h"
#include "mt8135_mhl_reg.h"
#include "mt6397_cbus_reg.h"
#include "mhl_ctrl.h"


unsigned char u1CbusConnectState = CBUS_LINK_IDLE;
unsigned char u1CbusConnectStateBak = CBUS_LINK_IDLE;

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

unsigned int u4CbusConnectStateLoopCount = 0;
void vCbusConnectState(void)
{
	u4CbusConnectStateLoopCount++;
	switch (u1CbusConnectState) {
	case CBUS_LINK_IDLE:
		break;
	case CBUS_LINK_STATE_USB_MODE:
		if (u2MhlTimerOutCount > (50 / MHL_LINK_TIME)) {
			vSetMHLUSBMode(TRUE);
			u1CbusHwStatus = 0;
			vCbusReset();
			vCbusTriggerCheck1K();
			vCbusWakeupFinish();
			/* check 1K */
			if (fgCbusCheck1K()) {
				printk("[cbus]wakeup\n");
				vSetMHLUSBMode(FALSE);
				/* wait discovry */
				vCbusWakeupFinish();
				if (fgCbusDiscovery())	/* connected */
				{
					printk("[cbus]discovery\n");
					vCbusInitDevice();
					vCbusRxEnable();
					u1CbusConnectState = CBUS_LINK_STATE_CHECK_RENSE;
					u2MhlReqDelay = 0;
				} else
					u1CbusConnectState = CBUS_LINK_STATE_USB_MODE;

				vSetCbusBit(CBUS_LINK_07, LINK_07_DETECT_AGAIN_MASK);
			}
			u2MhlTimerOutCount = 0;
		}
		break;
	case CBUS_LINK_STATE_CHECK_RENSE:
		if (fgGetMhlRsen() == TRUE) {
			printk("[cbus]Rsen 1\n");
			u1CbusConnectState = CBUS_LINK_STATE_CONNECTED;
		}
		if (u2MhlTimerOutCount > (600 / MHL_LINK_TIME))	/* check rense time out */
		{
			MHL_CBUS_LOG("To detect Rsen 1,but time out\n");
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
			u1CbusConnectState = CBUS_LINK_FLOAT_CBUS;
			vCbusReset();
		}
		u2MhlTimerOutCount = 0;
		break;
	case CBUS_LINK_FLOAT_CBUS:
		/* > 50ms, return to check 1K R */
		if (u2MhlTimerOutCount > (50 / MHL_LINK_TIME)) {
			vSetMhlVbus(TRUE);
			u1CbusConnectState = CBUS_LINK_STATE_USB_MODE;
			vCbusReset();
		}
		break;
	default:
		MHL_CBUS_LOG("Link State Err\n");
		break;
	}

	if (u1CbusConnectState != u1CbusConnectStateBak) {
		if (u1CbusConnectStateBak == CBUS_LINK_STATE_CONTENT_ON)
			vMhlConnectCallback(FALSE);
		u1CbusConnectStateBak = u1CbusConnectState;
		switch (u1CbusConnectState) {
		case CBUS_LINK_STATE_USB_MODE:
			MHL_CBUS_LOG("CBUS_LINK_STATE_USB_MODE\n");
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

/**************************************************
	process cbus msc
**************************************************/
	if (fgIsCbusConnected()) {
		if ((stMhlDev.fgMyIsPathEnChg)
		    && (u2MhlReqDelay > (100 / MHL_LINK_TIME))
		    && fgCbusMscIdle()
		    && fgCbusDdcIdle()) {
			if (fgCbusReqWriteStatCmd
			    (CBUS_MSC_STATUS_LINK_MODE,
			     u1MyDeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE]) == FALSE)
				MHL_CBUS_LOG("set path en fail\n");
			stMhlDev.fgMyIsPathEnChg = FALSE;
		}
	}
	if (fgIsCbusContentOn()) {
		if ((stMhlDev.fgIsDCapChg)
		    && (u2MhlDcapReadDelay > (300 / MHL_LINK_TIME))
		    && fgCbusMscIdle()
		    && fgCbusDdcIdle())	/* get device cap */
		{
			MHL_CBUS_LOG("get dcap\n");
			if (fgMhlGetDevCap() == FALSE)
				MHL_CBUS_LOG("get dcap fail\n");
			stMhlDev.fgIsDCapChg = FALSE;
		} else if ((stMhlDev.fgMyIsPPModeChg)
			   && fgCbusMscIdle()
			   && fgCbusDdcIdle())	/* set mhl mode */
		{
			if (fgCbusReqWriteStatCmd
			    (CBUS_MSC_STATUS_LINK_MODE,
			     u1MyDeviceRegSpace[CBUS_MSC_STATUS_LINK_MODE]) == FALSE)
				MHL_CBUS_LOG("set PP fail\n");
			stMhlDev.fgMyIsPPModeChg = FALSE;
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
		} else if (stMhlDev.fgIsEdidChg && (u2MhlEdidReadDelay > 600 / MHL_LINK_TIME)
			   && fgCbusDdcIdle()
			   && fgCbusMscIdle())	/* get edid */
		{
			stMhlDev.fgIsEdidChg = FALSE;
			MHL_CBUS_LOG("get edid\n");
			mhl_checedid(0);
			vShowEdidRawData();
			vMhlConnectCallback(TRUE);
		}

		if (stMhlDev.fgIsHPD != stMhlDev.fgIsHPDBak) {
			stMhlDev.fgIsHPDBak = stMhlDev.fgIsHPD;
			if (stMhlDev.fgIsHPD == FALSE)
				vMhlConnectCallback(FALSE);
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
	unsigned char i;

	memset(u1MyDeviceRegSpace, 0, MHL_CBUS_DEVICE_LENGTH);
	memset(u1DeviceRegSpace, 0, MHL_CBUS_DEVICE_LENGTH);
	/* init my device */
	u1MyDeviceRegSpace[0] = 0x00;	/* 0: DEV_STATE           x */
	u1MyDeviceRegSpace[1] = 0x20;	/* 1: MHL_VERSION         - */
	u1MyDeviceRegSpace[2] = 0x32;	/* 2: DEV_CAT                     - POW[4], DEV_TYPE[3:0],900mA */
	u1MyDeviceRegSpace[3] = 0x00;	/* 3: ADOPTER_ID_H */
	u1MyDeviceRegSpace[4] = 0x00;	/* 4: ADOPTER_ID_L */
	u1MyDeviceRegSpace[5] = 0x0F;	/* 5: VID_LINK_MODE       - SUPP_VGA[5], SUPP_ISLANDS[4], SUPP_PPIXEL[3], SUPP_YCBCR422[2], SUPP_YCBCR444[1], SUPP_RGB444[0] */
	u1MyDeviceRegSpace[6] = 0x03;	/* 6: AUD_LINK_MODE       - AUD_8CH[1], AUD_2CH[0] */
	u1MyDeviceRegSpace[7] = 0x00;	/* 7: VIDEO_TYPE           x SUPP_VT[7], VT_GAME[3], VT_CINEMA[2], VT_PHOTO[1], VT_GRAPHICS[0] */
	u1MyDeviceRegSpace[8] = 0x00;	/* 8: LOG_DEV_MAP         - LD_GUI[7], LD_SPEAKER[6], LD_RECORD[5], LD_TUNER[4], LD_MEDIA[3], LD_AUDIO[2], LD_VIDEO[1], LD_DISPLAY[0] */
	u1MyDeviceRegSpace[9] = 0x0F;	/* 9: BANDWIDTH            x */
	u1MyDeviceRegSpace[10] = 0x04;	/* A: FEATURE_FLAG       - UCP_RECV_SUPPORT[4], UCP_SEND_SUPPORT[3], SP_SUPPORT[2], RAP_SUPPORT[1], RCP_SUPPORT[0] */
	u1MyDeviceRegSpace[11] = 0x00;	/* B: DEVICE_ID_H         - */
	u1MyDeviceRegSpace[12] = 0x00;	/* C: DEVICE_ID_L                - */
	u1MyDeviceRegSpace[13] = 0x0F;	/* D: SCRATCHPAD_SIZE    - */
	u1MyDeviceRegSpace[14] = 0x33;	/* E: INT_STAT_SIZE      - STAT_SIZE[7:4], INT_SIZE[3:0] */
	u1MyDeviceRegSpace[15] = 0x00;	/* F: Reserved */
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

	stMhlDev.fgIs3DReq = FALSE;
	stMhlDev.fgIsDCapChg = FALSE;
	stMhlDev.fgIsDCapRdy = FALSE;
	stMhlDev.fgIsDScrChg = FALSE;
	stMhlDev.fgIsEdidChg = FALSE;
	stMhlDev.fgIsGrtWrt = FALSE;
	stMhlDev.fgIsHPD = FALSE;
	stMhlDev.fgIsHPDBak = FALSE;
	stMhlDev.fgIsMuted = FALSE;
	stMhlDev.fgIsPathEn = FALSE;
	stMhlDev.fgIsPPMode = FALSE;
	stMhlDev.fgIsRense = FALSE;
	stMhlDev.fgIsReqWrt = FALSE;
	stMhlDev.fgMyIsMutedChg = FALSE;
	stMhlDev.fgMyIsPathEnChg = FALSE;
	stMhlDev.fgMyIsPPModeChg = FALSE;
}

void vCbusCtrlInit(void)
{
	vCbusInitDevice();
	u1CbusConnectState = CBUS_LINK_IDLE;
	u1CbusConnectStateBak = CBUS_LINK_IDLE;
}

void vCbusStart(void)
{
	printk("cbus start\n");
	u1CbusConnectState = CBUS_LINK_STATE_USB_MODE;
}

void vCbusLinkStatus(void)
{
	unsigned int i;
	printk("=========================================\n");
	printk("vCbusConnectState live:%x\n", u4CbusConnectStateLoopCount);
	printk("cbus link status\n");
	switch (u1CbusConnectState) {
	case CBUS_LINK_IDLE:
		printk("CBUS_LINK_IDLE\n");
		break;
	case CBUS_LINK_STATE_USB_MODE:
		printk("CBUS_LINK_STATE_USB_MODE\n");
		break;
	case CBUS_LINK_STATE_CHECK_RENSE:
		printk("CBUS_LINK_STATE_CHECK_RENSE\n");
		break;
	case CBUS_LINK_STATE_CONNECTED:
		printk("CBUS_LINK_STATE_CONNECTED\n");
		break;
	case CBUS_LINK_STATE_CONTENT_ON:
		printk("CBUS_LINK_STATE_CONTENT_ON\n");
		break;
	case CBUS_LINK_FLOAT_CBUS:
		printk("CBUS_LINK_FLOAT_CBUS\n");
		break;
	default:
		break;
	}

	printk("fgIsHPD=%d\n", stMhlDev.fgIsHPD);
	printk("fgIsRense=%d\n", stMhlDev.fgIsRense);
	printk("fgIsDCapChg=%d\n", stMhlDev.fgIsDCapChg);
	printk("fgIsDCapRdy=%d\n", stMhlDev.fgIsDCapRdy);
	printk("fgIsEdidChg=%d\n", stMhlDev.fgIsEdidChg);
	printk("fgIsPPMode=%d\n", stMhlDev.fgIsPPMode);
	printk("fgIsPathEn=%d\n", stMhlDev.fgIsPathEn);
	printk("fgIsMuted=%d\n", stMhlDev.fgIsMuted);
	printk("fgIsMyPPModeChg=%d\n", stMhlDev.fgMyIsPPModeChg);
	printk("fgIsMyPathEnChg=%d\n", stMhlDev.fgMyIsPathEnChg);
	printk("fgIsMyMutedChg=%d\n", stMhlDev.fgMyIsMutedChg);

	printk("cbus my device cap\n");
	for (i = 0; i < 16; i++) {
		if (u1MyDeviceRegSpace[i] > 0x0F)
			printk(" %2x", u1MyDeviceRegSpace[i]);
		else
			printk(" 0%x", u1MyDeviceRegSpace[i]);
	}
	printk("\n");
	printk("cbus device cap\n");
	for (i = 0; i < 16; i++) {
		if (u1DeviceRegSpace[i] > 0x0F)
			printk(" %2x", u1DeviceRegSpace[i]);
		else
			printk(" 0%x", u1DeviceRegSpace[i]);
	}
	printk("\n");
}
