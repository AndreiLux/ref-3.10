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
#include <mach/mt_pmic_wrap.h>
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>

#include "mhl_cbus.h"
#include "mhl_hdcp.h"

#include "mhl_cbus_ctrl.h"
#include "mhl_dbg.h"
#include "mt8135_mhl_reg.h"
#include "mt6397_cbus_reg.h"
#include "mhl_ctrl.h"



/* //////////////////////////////////////////////////////// */
extern void vMhlTriggerIntTask(void);
extern void vMhlIntWaitEvent(void);
/* //////////////////////////////////////////////////////// */
stMhlDev_st stMhlDev;
stMhlDcap_st stMhlDcap;

unsigned int u1CbusTxHwStatus = 0;
unsigned int u1CbusRxHwStatus = 0;
unsigned int u1CbusHw1KStatus = 0;
unsigned int u1CbusHwWakeupStatus = 0;

st_MscDdcTmrOut_st st_MscTmrOut;
st_MscDdcTmrOut_st st_DdcTmrOut;
st_MscDdcTmrOut_st st_TxokTmrOut;

unsigned char u1CbusDdcErrCode = 0;
unsigned char u1CbusMscErrCode = 0;
unsigned int u1CbusMscErrCodeDelay2S = 0;
unsigned int u1CbusDdcErrCodeDelay2S = 0;
unsigned int u1CbusMscAbortDelay2S = 0;
unsigned int u1CbusDdcAbortDelay2S = 0;
/* /////////////////////////////////////// */
/* /////////////////////////////////////// */
/* for Sink device reg */
unsigned char u1DeviceRegSpace[MHL_CBUS_DEVICE_LENGTH];
/* for me(Source) device reg */
unsigned char u1MyDeviceRegSpace[MHL_CBUS_DEVICE_LENGTH];

unsigned char u1MhlVendorID = 0;
unsigned char u1MhlSinkVendorID = 0;

stCbus_st stCbus;
stCbusRequester_st stCbusRequester;
stCbusMscMsg_st stCbusMscMsg;
stCbusDdc_st stCbusDdc;
stCbusTxBuf_st stCbusTxBuf;
/* //////////////////////////////////////////////////////// */
static unsigned int u4MhlMt6397Version;

void vMhlGet6397Version(void)
{
	u4MhlMt6397Version = upmu_get_cid();
	MHL_CBUS_LOG("6397 Ver : %X\n", u4MhlMt6397Version);
}

bool fgMhlMt6397IsEco2(void)
{

	if (u4MhlMt6397Version == CBUS_MT6397_VER_ECO1)
		return FALSE;
	return TRUE;

}

/* //////////////////////////////////////////////////////// */
unsigned int u4CbusWaitTimeOut = 0;
bool fgReqWaitFinish = FALSE;

void vMhlUnlockCall(void)
{
	fgReqWaitFinish = TRUE;
}

void vCbusReqWaitFinish(void)
{
	u4CbusWaitTimeOut = 0;
	fgReqWaitFinish = FALSE;
	while ((fgReqWaitFinish == FALSE) && fgIsCbusConnected()) {
		if (stCbusRequester.fgIsRequesterWait) {
			vMhlTriggerIntTask();
		}
		msleep(5);	/* delay 5ms */
		if (u4CbusWaitTimeOut++ > (2000 / 5)) {
			MHL_CBUS_LOG("req wait time out\n");
			break;
		}
	}
}

unsigned int u4CbusWakupTimeOut = 0;
bool fgWakupFinish = FALSE;
void vCbusWakeupEvent(void)
{
	fgWakupFinish = TRUE;
}

void vCbusWakeupFinish(void)
{
	u4CbusWakupTimeOut = 0;
	fgWakupFinish = FALSE;
	while (fgWakupFinish == FALSE) {
		msleep(1);	/* delay10ms */
		if (u4CbusWakupTimeOut++ > (2000 / 1)) {
			MHL_CBUS_LOG("wakeup time out\n");
			break;
		}
	}
}

unsigned int u4CbusCheck1KTimeOut = 0;
bool fgCheck1KFinish = FALSE;
void vCbusCheck1KEvent(void)
{
	fgCheck1KFinish = TRUE;
}

void vCbusCheck1KFinish(void)
{
	u4CbusCheck1KTimeOut = 0;
	fgCheck1KFinish = FALSE;
	msleep(10);
}

/* //////////////////////////////////////////////////////// */
void vWriteCbus(unsigned short dAddr, unsigned int dVal)
{
	unsigned int tmp;
	pwrap_wacs2(1, dAddr, (dVal & 0xFFFF), &tmp);
	pwrap_wacs2(1, (dAddr + 2), ((dVal >> 16) & 0xFFFF), &tmp);
	/* MHL_CBUS_LOG("W4B:%X=%X\n ",dAddr,dVal); */
}

void vWrite2BCbus(unsigned short dAddr, unsigned int dVal)
{
	unsigned int tmp;
	pwrap_wacs2(1, dAddr, (dVal & 0xFFFF), &tmp);
	/* MHL_CBUS_LOG("W2B:%X=%X\n ",dAddr,dVal); */
}

unsigned int u4ReadCbus(unsigned int dAddr)
{
	unsigned int tmp, u4data;
	tmp = 0;
	pwrap_wacs2(0, dAddr, 0, &tmp);
	u4data = tmp;
	pwrap_wacs2(0, (dAddr + 2), 0, &tmp);
	u4data |= (tmp << 16);
	return u4data;
}

unsigned int u4Read2BCbus(unsigned int dAddr)
{
	unsigned int tmp, u4data;
	tmp = 0;
	u4data = 0;
	pwrap_wacs2(0, dAddr, 0, &tmp);
	u4data = tmp;
	return u4data;
}

unsigned int u4ReadCbusFld(unsigned int dAddr, unsigned char u1Shift, unsigned int u1Mask)
{
	return ((u4ReadCbus(dAddr) & u1Mask) >> u1Shift);
}

void vWriteCbusMsk(unsigned int dAddr, unsigned int dVal, unsigned int dMsk)
{
	vWriteCbus((dAddr), (u4ReadCbus(dAddr) & (~(dMsk))) | ((dVal) & (dMsk)));
}

void vSetCbusBit(unsigned int dAddr, unsigned int dMsk)
{
	vWriteCbus((dAddr), u4ReadCbus(dAddr) | (dMsk));
}

void vClrCbusBit(unsigned int dAddr, unsigned int dMsk)
{
	vWriteCbus((dAddr), u4ReadCbus(dAddr) & (~(dMsk)));
}

/* //////////////////////////////////////////////////////// */
unsigned char u1UsbMhlMode = 0xff;
#if 1
/* p2v2 */
void vSetMHLUSBMode(unsigned char fgUse)
{
	/* stop cbus flow and share GPIO16/17 to signal trigger for debug */
	if (u1ForceCbusStop == 0xA5) {
		fgUse = 0;
		if (fgCbusStop)
			return;
	}
	/* force tmds output for tmds test */
	if (u1ForceTmdsOutput == 0xa5) {
		/* pr_info("force tmds output, always mhl mode\n"); */
		fgUse = 0;
	}

	if (u1UsbMhlMode == fgUse)
		return;
	u1UsbMhlMode = fgUse;

	/* p2v2 */
	/* bit9 : gpio16, tdms sel */
	/* bit10:gpio17, cbus sel */

	/* config to gpio */
	/* C0D8[5,3] : GPIO16 */
	/* C0D8[8,6] : GPIO17 */
	vWrite2BCbus(0xC0D8,
		     (u4Read2BCbus(0xC0D8) & (~((0x07 << 6) | (0x07 << 3)))) | ((0 << 6) |
										(0 << 3)));
	/* config to output */
	/* c008 :bit0 :gpio16 */
	/* c008 :bit1 :gpio17 */
	vWrite2BCbus(0xC008,
		     (u4Read2BCbus(0xC008) & (~((1 << 1) | (1 << 0)))) | ((1 << 1) | (1 << 0)));
	if (fgUse) {
		/* USB mode */
		/* 0,0,tdms -> USB, ID -> USB_ID */
		vWrite2BCbus(0xC088, (u4Read2BCbus(0xC088) & (~((1 << 1) | (1 << 0)))));
	} else {
		/* MHL mode */
		/* 1,1,tdms -> MHL, ID -> CBUS */
		vWrite2BCbus(0xC088,
			     (u4Read2BCbus(0xC088) & (~((1 << 1) | (1 << 0)))) | ((1 << 1) |
										  (1 << 0)));
	}
}

void vMhlUseSel2PinTriggerForDebug(void)
{
	/* sw_sel2 to trigger stop for waveform debug, 1->0 */
	vWrite2BCbus(0xC088, (u4Read2BCbus(0xC088) & (~(1 << 1))));
}
#else
/* evb1 */
void vSetMHLUSBMode(unsigned char fgUse)
{
	/* stop cbus flow and share GPIO25/26 to signal trigger for debug */
	if (u1ForceCbusStop == 0xA5) {
		fgUse = 0;
		if (fgCbusStop)
			return;
	}
	/* force tmds output for tmds test */
	if (u1ForceTmdsOutput == 0xa5) {
		fgUse = 0;
	}

	if (u1ForceCbusStop != 0xA5) {
		if (u1UsbMhlMode == fgUse)
			return;
		u1UsbMhlMode = fgUse;
	}
	/* bit9 : gpio25, tdms sel */
	/* bit10:gpio26, cbus sel */

	/* config row5/6 to gpio */
	/* C0E8[2,0] : GPIO25 */
	/* C0E8[5,3] : GPIO26 */
	vWrite2BCbus(0xC0E8,
		     (u4Read2BCbus(0xC0E8) & (~((0x07 << 3) | (0x07 << 0)))) | ((0 << 3) |
										(0 << 0)));
	/* config row5/6 to output */
	vWrite2BCbus(0xC008,
		     (u4Read2BCbus(0xC008) & (~((1 << 9) | (1 << 10)))) | ((1 << 9) | (1 << 10)));
	if (fgUse) {
		/* USB mode */
		/* 0,0,tdms -> USB, ID -> USB_ID */
		vWrite2BCbus(0xC088, (u4Read2BCbus(0xC088) & (~((1 << 9) | (1 << 10)))));
	} else {
		/* MHL mode */
		/* 1,1,tdms -> MHL, ID -> CBUS */
		vWrite2BCbus(0xC088,
			     (u4Read2BCbus(0xC088) & (~((1 << 9) | (1 << 10)))) | ((1 << 9) |
										   (1 << 10)));
	}
}

void vMhlUseSel2PinTriggerForDebug(void)
{
	/* sw_sel2 to trigger stop for waveform debug, 1->0 */
	vWrite2BCbus(0xC088, (u4Read2BCbus(0xC088) & (~((1 << 10)))));
}
#endif
/* //////////////////////////////////////////////////////// */
bool fgCbusTxEvent(void)
{
	if ((u1CbusTxHwStatus &
	     (LINK_08_TX_RETRY_TO_INT_CLR_MASK | LINK_08_TX_OK_INT_CLR_MASK |
	      LINK_08_TX_ARB_FAIL_INT_CLR_MASK))
	    == 0)
		return FALSE;
	return TRUE;
}

bool fgCbusTxOk(void)
{
	if ((u1CbusTxHwStatus &
	     (LINK_08_TX_RETRY_TO_INT_CLR_MASK | LINK_08_TX_OK_INT_CLR_MASK |
	      LINK_08_TX_ARB_FAIL_INT_CLR_MASK))
	    == LINK_08_TX_OK_INT_CLR_MASK)
		return TRUE;
	return FALSE;
}

bool fgCbusTxErr(void)
{
	if ((u1CbusTxHwStatus &
	     (LINK_08_TX_RETRY_TO_INT_CLR_MASK | LINK_08_TX_ARB_FAIL_INT_CLR_MASK)) == 0)
		return FALSE;
	return TRUE;
}

bool fgCbusTriFail(void)
{
	if ((u1CbusTxHwStatus & LINK_08_MONITOR_CMP_INT_CLR_MASK) ==
	    LINK_08_MONITOR_CMP_INT_CLR_MASK)
		return TRUE;
	return FALSE;
}

bool fgCbusRxEvent(void)
{
	if ((u1CbusRxHwStatus
	     & (LINK_08_LINKRX_TIMEOUT_INT_CLR_MASK | LINK_08_RBUF_TRIG_INT_CLR_MASK))
	    == 0)
		return FALSE;
	return TRUE;
}

bool fgCbusRxOk(void)
{
	if ((u1CbusRxHwStatus
	     & (LINK_08_LINKRX_TIMEOUT_INT_CLR_MASK | LINK_08_RBUF_TRIG_INT_CLR_MASK))
	    == LINK_08_RBUF_TRIG_INT_CLR_MASK)
		return TRUE;
	return FALSE;
}

bool fgCbusRxErr(void)
{
	if ((u1CbusRxHwStatus & LINK_08_LINKRX_TIMEOUT_INT_CLR_MASK) == 0)
		return FALSE;
	return TRUE;
}

/************************************************
	MSC control packets
************************************************/
bool fgIsMscData(unsigned short u2data)
{
	if ((u2data & 0x700) == 0x400)
		return TRUE;
	else
		return FALSE;
}

unsigned char u1GetMscData(unsigned short u2data)
{
	return (unsigned char)(u2data & 0xff);
}

unsigned short u2SetMscData(unsigned short u2Data)
{
	return ((u2Data & 0xff) | 0x400);
}

bool fgIsMscACK(unsigned short u2data)
{
	if (u2data == CBUS_MSC_CTRL_ACK)
		return TRUE;
	else
		return FALSE;
}

bool fgIsMscNACK(unsigned short u2data)
{
	if (u2data == CBUS_MSC_CTRL_NACK)
		return TRUE;
	else
		return FALSE;
}

bool fgIsMscAbort(unsigned short u2data)
{
	if (u2data == CBUS_MSC_CTRL_ABORT)
		return TRUE;
	else
		return FALSE;
}

bool fgIsMscOpInvalid(unsigned short u2data)
{
	if ((u2data & 0x100) == 0)
		return FALSE;

	if ((u2data == CBUS_MSC_CTRL_ACK)
	    || (u2data == CBUS_MSC_CTRL_ABORT)
	    || (u2data == CBUS_MSC_CTRL_NACK)
	    || (u2data == CBUS_MSC_CTRL_WRITE_STATE)
	    || (u2data == CBUS_MSC_CTRL_READ_DEVCAP)
	    || (u2data == CBUS_MSC_CTRL_GET_STATE)
	    || (u2data == CBUS_MSC_CTRL_GET_VENDER_ID)
	    || (u2data == CBUS_MSC_CTRL_SET_HPD)
	    || (u2data == CBUS_MSC_CTRL_CLR_HPD)
	    || (u2data == CBUS_MSC_CTRL_MSC_MSG)
	    || (u2data == CBUS_MSC_CTRL_GET_SC1_EC)
	    || (u2data == CBUS_MSC_CTRL_GET_DDC_EC)
	    || (u2data == CBUS_MSC_CTRL_GET_MSC_EC)
	    || (u2data == CBUS_MSC_CTRL_WRITE_BURST)
	    || (u2data == CBUS_MSC_CTRL_GET_SC3_EC)
	    || (u2data == CBUS_MSC_CTRL_EOF)
	    )
		return FALSE;
	else
		return TRUE;
}

bool fgIsRespMscCmdValid(unsigned short u2data)
{
	if ((u2data == CBUS_MSC_CTRL_WRITE_STATE)
	    || (u2data == CBUS_MSC_CTRL_READ_DEVCAP)
	    || (u2data == CBUS_MSC_CTRL_GET_STATE)
	    || (u2data == CBUS_MSC_CTRL_GET_VENDER_ID)
	    || (u2data == CBUS_MSC_CTRL_SET_HPD)
	    || (u2data == CBUS_MSC_CTRL_CLR_HPD)
	    || (u2data == CBUS_MSC_CTRL_MSC_MSG)
	    || (u2data == CBUS_MSC_CTRL_GET_SC1_EC)
	    || (u2data == CBUS_MSC_CTRL_GET_DDC_EC)
	    || (u2data == CBUS_MSC_CTRL_GET_MSC_EC)
	    || (u2data == CBUS_MSC_CTRL_WRITE_BURST)
	    || (u2data == CBUS_MSC_CTRL_GET_SC3_EC)
	    )
		return TRUE;
	else
		return FALSE;
}

bool fgIsRespMscACKNACKABORTCmd(unsigned short u2data)
{
	if ((u2data == CBUS_MSC_CTRL_ACK)
	    || (u2data == CBUS_MSC_CTRL_NACK)
	    || (u2data == CBUS_MSC_CTRL_ABORT)
	    )
		return TRUE;
	else
		return FALSE;
}

bool fgIsMscMsg(unsigned short u2data)
{
	if ((u2data == MHL_MSC_MSG_MSGE)
	    || (u2data == MHL_MSC_MSG_RCP)
	    || (u2data == MHL_MSC_MSG_RCPK)
	    || (u2data == MHL_MSC_MSG_RCPE)
	    || (u2data == MHL_MSC_MSG_RAP)
	    || (u2data == MHL_MSC_MSG_RAPK)
	    || (u2data == MHL_MSC_MSG_UCP)
	    || (u2data == MHL_MSC_MSG_UCPK)
	    || (u2data == MHL_MSC_MSG_UCPE)
	    )
		return TRUE;
	else
		return FALSE;
}


void vSetCbusMSCWaitTmr(unsigned int u2Tmr)
{
	st_MscTmrOut.u4MscDdcTmr = u2Tmr;
	st_MscTmrOut.fgTmrOut = FALSE;
}

void vClrCbusMSCWaitTmr(void)
{
	st_MscTmrOut.u4MscDdcTmr = 0;
	st_MscTmrOut.fgTmrOut = FALSE;
}

bool fgCbusMSCWaitTmrOut(void)
{
	if (st_MscTmrOut.fgTmrOut == TRUE)
		return TRUE;
	else
		return FALSE;
}

void vSetCbusDDCWaitTmr(unsigned int u2Tmr)
{
	st_DdcTmrOut.u4MscDdcTmr = u2Tmr;
	st_DdcTmrOut.fgTmrOut = FALSE;
}

void vClrCbusDDCWaitTmr(void)
{
	st_DdcTmrOut.u4MscDdcTmr = 0;
	st_DdcTmrOut.fgTmrOut = FALSE;
}

bool fgCbusDDCWaitTmrOut(void)
{
	if (st_DdcTmrOut.fgTmrOut == TRUE)
		return TRUE;
	else
		return FALSE;
}

void vSetCbusTxOkWaitTmr(unsigned int u2Tmr)
{
	st_TxokTmrOut.u4MscDdcTmr = u2Tmr;
	st_TxokTmrOut.fgTmrOut = FALSE;
}

void vClrCbusTxOkWaitTmr(void)
{
	st_TxokTmrOut.u4MscDdcTmr = 0;
	st_TxokTmrOut.fgTmrOut = FALSE;
}

bool fgCbusTxOkWaitTmrOut(void)
{
	if (st_TxokTmrOut.fgTmrOut == TRUE)
		return TRUE;
	else
		return FALSE;
}

/* /////////////////////////////////////// */

void vResetCbusMSCState(void)
{
	vClrCbusMSCWaitTmr();
	stCbus.u2RXBufInd = 0;
	stCbus.u2CbusState = MHL_CBUS_STATE_IDLE;
	stCbus.stResp.u2State = MHL_CBUS_STATE_S0;
	stCbus.stReq.u2State = MHL_CBUS_STATE_S0;
}

void vResetCbusDDCState(void)
{				/* xubo */
	vClrCbusDDCWaitTmr();
	stCbus.u2CbusDdcState = MHL_CBUS_STATE_IDLE;
	stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S0;
}

void vCbusTxTriggerECO(bool fgen)
{
	if (fgen)
		vWrite2BCbus(CBUS_LINK_0D, 1);
	else
		vWrite2BCbus(CBUS_LINK_0D, 0);
}

void vCbusTxTrigger(void)
{
	unsigned short tmp;
	tmp = u4Read2BCbus(CBUS_LINK_00);
	tmp = tmp | LINK_00_TX_TRIG_MASK;
	vWrite2BCbus(CBUS_LINK_00, tmp);
	tmp = tmp & (~LINK_00_TX_TRIG_MASK);	/* clean trigger */
	vWrite2BCbus(CBUS_LINK_00, tmp);
}

void vCbusTxQuickTrigger(unsigned int u4data)
{
	unsigned short tmp;
	tmp = u4data | 1;
	vWrite2BCbus(CBUS_LINK_00, tmp);
	tmp = tmp & (~(1));
	vWrite2BCbus(CBUS_LINK_00, tmp);
}

static unsigned char u1CbusSendMsg(unsigned short *pMsgData, unsigned char dataSize)
{
	unsigned char i;
	unsigned int tmp;

	for (i = 0; i < dataSize; i++) {
		vWrite2BCbus(CBUS_WBUF0 + (i << 1), *(pMsgData + i));
	}
	/* set tx len , and clear trigger bit */
	tmp = CBUS_LINK_00_SETTING_L | (i << LINK_00_TX_NUM);
	vWrite2BCbus(CBUS_LINK_00, tmp);
	if (fgMhlMt6397IsEco2() == TRUE) {
		vCbusTxTriggerECO(TRUE);
		vCbusTxTriggerECO(FALSE);
	} else {
		vCbusTxQuickTrigger(tmp);
	}

	for (i = 0; i < dataSize; i++) {
		MHL_CBUS_TXRX("T:%X,%d\n", *(pMsgData + i), i);
	}
	return 0;
}

static unsigned int u4CbusSendBufMsg(unsigned short *pMsgData, unsigned char dataSize)
{
	unsigned char i;
	unsigned int tmp;

	for (i = 0; i < dataSize; i++) {
		MHL_CBUS_TXRX("NT:%X\n", *(pMsgData + i));
	}
	for (i = 0; i < dataSize; i++) {
		vWrite2BCbus(CBUS_WBUF0 + (i << 1), *(pMsgData + i));
	}
	/* set tx len , and clear trigger bit */
	tmp = CBUS_LINK_00_SETTING_L | (i << LINK_00_TX_NUM);
	vWrite2BCbus(CBUS_LINK_00, tmp);

	return tmp;
}

void vCbusSendMscMsg(unsigned short *pMsgData, unsigned char dataSize)
{
	unsigned char i;

	if (stCbusTxBuf.fgTxValid == FALSE)
		stCbusTxBuf.u2Len = 0;
	for (i = 0; i < dataSize; i++)
		stCbusTxBuf.u2TxBuf[stCbusTxBuf.u2Len + i] = pMsgData[i];
	stCbusTxBuf.u2Len = stCbusTxBuf.u2Len + dataSize;
	stCbusTxBuf.fgTxValid = TRUE;
	if (stCbusTxBuf.u2Len > MHL_TX_HW_BUF_MAX)
		MHL_CBUS_ERR("tx f\n");
}

void vCbusSendDdcMsg(unsigned short *pMsgData, unsigned char dataSize)
{
	unsigned char i;

	if (stCbusTxBuf.fgTxValid == FALSE)
		stCbusTxBuf.u2Len = 0;
	for (i = 0; i < dataSize; i++)
		stCbusTxBuf.u2TxBuf[stCbusTxBuf.u2Len + i] = pMsgData[i];
	stCbusTxBuf.u2Len = stCbusTxBuf.u2Len + dataSize;
	stCbusTxBuf.fgTxValid = TRUE;
	if (stCbusTxBuf.u2Len > MHL_TX_HW_BUF_MAX)
		MHL_CBUS_ERR("tx f\n");
}

static void vCbusMscErrHandling(unsigned char u1ErrorCode)
{
	unsigned short arTxMscMsgs[2];
	vClrCbusMSCWaitTmr();
	u1CbusMscErrCode = u1ErrorCode;
	u1CbusMscAbortDelay2S = MHL_MSCDDC_ERR_2S;
	MHL_CBUS_ERR("MSC EC = %x\n", u1CbusMscErrCode);
	arTxMscMsgs[0] = CBUS_MSC_CTRL_ABORT;
	vCbusSendMscMsg(arTxMscMsgs, 1);
	if (u1ForceCbusStop == 0xA5) {
		vMhlUseSel2PinTriggerForDebug();
		fgCbusStop = TRUE;
		while (fgCbusStop) {
			msleep(500);
		}
	}
}

static void vCbusDdcErrHandling(unsigned char u1ErrorCode)
{
	unsigned short arTxMscMsgs[2];
	vClrCbusDDCWaitTmr();
	u1CbusDdcErrCode = u1ErrorCode;
	u1CbusDdcAbortDelay2S = MHL_MSCDDC_ERR_2S;
	MHL_CBUS_ERR("DDC EC = %x\n", u1CbusDdcErrCode);
	arTxMscMsgs[0] = CBUS_DDC_CTRL_ABORT;
	vCbusSendDdcMsg(arTxMscMsgs, 1);
	if (u1ForceCbusStop == 0xA5) {
		vMhlUseSel2PinTriggerForDebug();
		fgCbusStop = TRUE;
		while (fgCbusStop) {
			msleep(500);
		}
	}
}

/* 10ms */
void vCbusTimer(void)
{
	/* cbus timer out */
	if (st_DdcTmrOut.u4MscDdcTmr > 0) {
		st_DdcTmrOut.u4MscDdcTmr--;
		if (st_DdcTmrOut.u4MscDdcTmr == 0) {
			st_DdcTmrOut.fgTmrOut = TRUE;
			vMhlTriggerIntTask();
			MHL_CBUS_ERR("timer_tri,ddc timer out\n");
		}
	}
	if (st_MscTmrOut.u4MscDdcTmr > 0) {
		st_MscTmrOut.u4MscDdcTmr--;
		if (st_MscTmrOut.u4MscDdcTmr == 0) {
			st_MscTmrOut.fgTmrOut = TRUE;
			vMhlTriggerIntTask();
			MHL_CBUS_ERR("timer_tri,msc timer out\n");
		}
	}

	if (st_TxokTmrOut.u4MscDdcTmr > 0) {
		st_TxokTmrOut.u4MscDdcTmr--;
		if (st_TxokTmrOut.u4MscDdcTmr == 0) {
			st_TxokTmrOut.fgTmrOut = TRUE;
			vMhlTriggerIntTask();
			MHL_CBUS_ERR("timer_tri,txok time out\n");
		}
	}
	/* 6.3.6.5 */
	if (u1CbusMscAbortDelay2S > 0) {
		u1CbusMscAbortDelay2S--;
	}
	if (u1CbusDdcAbortDelay2S > 0) {
		u1CbusDdcAbortDelay2S--;
	}
	if (u2MhlEdidReadDelay > 0)
		u2MhlEdidReadDelay--;
	if (u2MhlDcapReadDelay > 0)
		u2MhlDcapReadDelay--;
	/* cbus link timer */
	u2MhlTimerOutCount++;
	u2MhlReqDelay++;
}

void vCbusRespAbort(void)
{
	unsigned short u2msg;

	vResetCbusMSCState();
	u2msg = CBUS_MSC_CTRL_ABORT;
	vCbusSendMscMsg(&u2msg, 1);
}

void vCbusRespAck(void)
{
	unsigned short u2msg;
	u2msg = CBUS_MSC_CTRL_ACK;
	vCbusSendMscMsg(&u2msg, 1);
}

void vCbusRespData(unsigned char u1Data)
{
	unsigned short u2msg;
	u2msg = (unsigned short)u1Data | 0x400;
	vCbusSendMscMsg(&u2msg, 1);
}

/*************************************************

	requseter state process

*************************************************/
static void vCbusReqGetStateState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("ReqGetState:%d\n", (stCbus.stReq.u2State));
	switch (stCbus.stReq.u2State) {
	case MHL_CBUS_STATE_S0:	/* wait read_devcap/regoffset ok,wait value */
		if (fgIsMscData(u2RxMsg)) {
			u1DeviceRegSpace[0] = u1GetMscData(u2RxMsg);
			stCbus.stReq.fgOk = TRUE;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			stCbus.stReq.fgOk = FALSE;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.fgOk = FALSE;
		}
		vResetCbusMSCState();
		vMhlUnlockCall();
		break;
	default:
		break;
	}

}

bool fgCbusReqGetStateCmd(void)
{

	if (u1CbusMscAbortDelay2S)
		return FALSE;

	MHL_CBUS_FUNC();
	stCbusRequester.fgIsRequesterWait = TRUE;
	stCbusRequester.fgIsDdc = FALSE;
	stCbusRequester.u2ReqBuf[0] = CBUS_MSC_CTRL_GET_STATE;
	stCbusRequester.u4Len = 1;
	vCbusReqWaitFinish();
	if (stCbus.stReq.fgOk)
		return TRUE;
	return FALSE;
}

static void vCbusReqGetVendorIDState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("ReqGetVendorID:%d\n", (stCbus.stReq.u2State));
	switch (stCbus.stReq.u2State) {
	case MHL_CBUS_STATE_S0:	/* wait read_devcap/regoffset ok */
		if (fgIsMscData(u2RxMsg)) {
			u1MhlSinkVendorID = u1GetMscData(u2RxMsg);
			stCbus.stReq.fgOk = TRUE;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			stCbus.stReq.fgOk = FALSE;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.fgOk = FALSE;
		}
		vResetCbusMSCState();
		vMhlUnlockCall();
		break;
	default:
		break;
	}

}

bool fgCbusReqGetVendorIDCmd(void)
{

	MHL_CBUS_FUNC();
	if (u1CbusMscAbortDelay2S)
		return FALSE;

	stCbusRequester.fgIsRequesterWait = TRUE;
	stCbusRequester.fgIsDdc = FALSE;
	stCbusRequester.u2ReqBuf[0] = CBUS_MSC_CTRL_GET_VENDER_ID;
	stCbusRequester.u4Len = 1;
	vCbusReqWaitFinish();
	if (stCbus.stReq.fgOk)
		return TRUE;
	return FALSE;
}

void vCbusReqReadDevCapState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("ReqReadDevCap:%d\n", (stCbus.stReq.u2State));
	switch (stCbus.stReq.u2State) {
	case MHL_CBUS_STATE_S0:	/* wait read_devcap/regoffset ok,wait ack */
		if (fgIsMscACK(u2RxMsg)) {
			vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			stCbus.stReq.u2State = MHL_CBUS_STATE_S1;
			break;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			stCbus.stReq.fgOk = FALSE;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.fgOk = FALSE;
		}
		vResetCbusMSCState();
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_S1:	/* wait value */
		if (fgIsMscData(u2RxMsg)) {
			MHL_CBUS_LOG("ID=%X,Data=%X\n", u1GetMscData(stCbusRequester.u2ReqBuf[1]),
				     u1GetMscData(u2RxMsg));
			u1DeviceRegSpace[u1GetMscData(stCbusRequester.u2ReqBuf[1])] =
			    u1GetMscData(u2RxMsg);
			stCbus.stReq.fgOk = TRUE;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			stCbus.stReq.fgOk = FALSE;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.fgOk = FALSE;
		}
		vResetCbusMSCState();
		vMhlUnlockCall();
		break;
	default:
		break;
	}

}

bool fgCbusReqReadDevCapCmd(unsigned char u1Data)
{
	MHL_CBUS_FUNC();
	if (u1CbusMscAbortDelay2S)
		return FALSE;
	stCbusRequester.fgIsRequesterWait = TRUE;
	stCbusRequester.fgIsDdc = FALSE;
	stCbusRequester.u2ReqBuf[0] = CBUS_MSC_CTRL_READ_DEVCAP;
	stCbusRequester.u2ReqBuf[1] = u2SetMscData(u1Data);
	stCbusRequester.u4Len = 2;
	vCbusReqWaitFinish();
	if (stCbus.stReq.fgOk)
		return TRUE;
	return FALSE;
}

static void vCbusReqWriteStatState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("ReqWriteStat:%d\n", (stCbus.stReq.u2State));

	switch (stCbus.stReq.u2State) {
	case MHL_CBUS_STATE_S0:	/* wait read_devcap/regoffset ok,wait ack */
		if (fgIsMscACK(u2RxMsg)) {
			stCbus.stReq.fgOk = TRUE;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			stCbus.stReq.fgOk = FALSE;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.fgOk = FALSE;
		}
		vResetCbusMSCState();
		vMhlUnlockCall();
		break;
	default:
		break;
	}

}

bool fgCbusReqWriteStatCmd(unsigned char u1Offset, unsigned char u1Data)
{
	MHL_CBUS_FUNC();

	if (u1CbusMscAbortDelay2S)
		return FALSE;

	stCbusRequester.fgIsRequesterWait = TRUE;
	stCbusRequester.fgIsDdc = FALSE;
	stCbusRequester.u2ReqBuf[0] = CBUS_MSC_CTRL_WRITE_STATE;
	stCbusRequester.u2ReqBuf[1] = u2SetMscData(u1Offset);
	stCbusRequester.u2ReqBuf[2] = u2SetMscData(u1Data);
	stCbusRequester.u4Len = 3;
	vCbusReqWaitFinish();
	if (stCbus.stReq.fgOk)
		return TRUE;
	return FALSE;
}

void vCbusReqWriteBrustState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("ReqWriteBrust:%d\n", (stCbus.stReq.u2State));

	switch (stCbus.stReq.u2State) {
	case MHL_CBUS_STATE_S0:	/* wait read_devcap/regoffset ok */
		if (fgIsMscACK(u2RxMsg)) {
			stCbus.stReq.fgOk = TRUE;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			stCbus.stReq.fgOk = FALSE;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.fgOk = FALSE;
		}
		vResetCbusMSCState();
		vMhlUnlockCall();
		break;
	default:
		break;
	}
}

bool fgCbusReqWriteBrustCmd(unsigned char *ptData, unsigned char u1Len)
{
	unsigned char i;

	MHL_CBUS_FUNC();
	if (u1CbusMscAbortDelay2S)
		return FALSE;

	if (u1Len > 0x10) {
		pr_info("ReqWriteBrust Len Err\n");
		return FALSE;
	}
	stCbusRequester.fgIsRequesterWait = TRUE;
	stCbusRequester.fgIsDdc = FALSE;
	stCbusRequester.u2ReqBuf[0] = CBUS_MSC_CTRL_WRITE_BURST;
	for (i = 0; i < u1Len; i++) {
		stCbusRequester.u2ReqBuf[i + 1] = u2SetMscData(ptData[i]);	/* offset/adopter/data */
	}
	stCbusRequester.u2ReqBuf[u1Len + 1] = CBUS_MSC_CTRL_EOF;
	stCbusRequester.u4Len = u1Len + 2;
	vCbusReqWaitFinish();
	if (stCbus.stReq.fgOk)
		return TRUE;
	return FALSE;
}

void vCbusReqMscMsgState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("ReqMscMsg:%d\n", (stCbus.stReq.u2State));

	switch (stCbus.stReq.u2State) {
	case MHL_CBUS_STATE_S0:	/* wait read_devcap/regoffset ok,wait ack */
		if (fgIsMscACK(u2RxMsg)) {
			stCbus.stReq.fgOk = TRUE;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			stCbus.stReq.fgOk = FALSE;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.fgOk = FALSE;
		}
		vResetCbusMSCState();
		vMhlUnlockCall();
		break;
	default:
		break;
	}
}

bool fgCbusReqMscMsgCmd(unsigned short u1Cmd, unsigned char u1Val)
{
	MHL_CBUS_FUNC();
	if (u1CbusMscAbortDelay2S)
		return FALSE;
	stCbusRequester.fgIsRequesterWait = TRUE;
	stCbusRequester.fgIsDdc = FALSE;
	stCbusRequester.u2ReqBuf[0] = CBUS_MSC_CTRL_MSC_MSG;
	stCbusRequester.u2ReqBuf[1] = u2SetMscData(u1Cmd);
	stCbusRequester.u2ReqBuf[2] = u2SetMscData(u1Val);
	stCbusRequester.u4Len = 3;
	vCbusReqWaitFinish();
	if (stCbus.stReq.fgOk)
		return TRUE;
	return FALSE;
}

static void vCbusReqMSCERRCodeDState(unsigned short u2RxMsg)
{
	unsigned char tmp;
	MHL_CBUS_LOG("ReqMSCERRCode:%d\n", (stCbus.stReq.u2State));
	switch (stCbus.stReq.u2State) {
	case MHL_CBUS_STATE_S0:	/* wait read_devcap/regoffset ok */
		if (fgIsMscData(u2RxMsg)) {
			tmp = u1GetMscData(u2RxMsg);
			MHL_CBUS_ERR("errc:%X\n", tmp);
			stCbus.stReq.fgOk = TRUE;
		} else {
			stCbus.stReq.fgOk = FALSE;
		}
		vResetCbusMSCState();
		vMhlUnlockCall();
		break;
	default:
		break;
	}

}

bool fgCbusReqMscERRCodeCmd(void)
{
	MHL_CBUS_FUNC();
	if (u1CbusMscAbortDelay2S)
		return FALSE;
	stCbusRequester.fgIsRequesterWait = TRUE;
	stCbusRequester.fgIsDdc = FALSE;
	stCbusRequester.u2ReqBuf[0] = CBUS_MSC_CTRL_GET_MSC_EC;
	stCbusRequester.u4Len = 1;
	vCbusReqWaitFinish();
	if (stCbus.stReq.fgOk)
		return TRUE;
	return FALSE;
}

/* ////////////////////////////////////////////////////// */
/* / */
/* /             DDC */
/* / */
/* ////////////////////////////////////////////////////// */
bool fgIsDdcCmdValid(unsigned short u2data)
{
	if ((u2data == CBUS_DDC_CTRL_SOF)
	    || (u2data == CBUS_DDC_CTRL_EOF)
	    || (u2data == CBUS_DDC_CTRL_ACK)
	    || (u2data == CBUS_DDC_CTRL_NACK)
	    || (u2data == CBUS_DDC_CTRL_ABORT)
	    || (u2data == CBUS_DDC_CTRL_CONT)
	    || (u2data == CBUS_DDC_CTRL_STOP)
	    )
		return TRUE;
	return FALSE;
}

bool fgIsDdcChannel(unsigned short u2data)
{
	if ((u2data & 0x600) == 0)
		return TRUE;
	return FALSE;
}

bool fgIsMscChannel(unsigned short u2data)
{
	if ((u2data & 0x600) == 0x400)
		return TRUE;
	return FALSE;
}

unsigned char u1GetDdcData(unsigned short u2Data)
{
	return (u2Data & 0xff);
}

unsigned short u2SetDdcData(unsigned char u1Data)
{
	return (unsigned short)(u1Data & 0xff);
}

bool fgIdDdcData(unsigned short u2Data)
{
	if ((u2Data & 0x0700) == 0)
		return TRUE;
	return FALSE;
}

bool fgIsDdcAbort(unsigned short u2data)
{
	if (u2data == CBUS_DDC_CTRL_ABORT)
		return TRUE;
	else
		return FALSE;
}

void vCbusSendDdcData(unsigned char u1Data)
{
	unsigned short u2Data[1];
	u2Data[0] = (u1Data & 0xff);
	vCbusSendDdcMsg(u2Data, 1);
}

void vCbusSendDdcCont(void)
{
	unsigned short u2Data[1];
	u2Data[0] = CBUS_DDC_CTRL_CONT;
	vCbusSendDdcMsg(u2Data, 1);
}

void vCbusSendDdcEof(void)
{
	unsigned short u2Data[1];
	u2Data[0] = CBUS_DDC_CTRL_EOF;
	vCbusSendDdcMsg(u2Data, 1);
}

void vCbusSendDdcStop(void)
{
	unsigned short u2Data[2];
	u2Data[0] = CBUS_DDC_CTRL_STOP;
	u2Data[1] = CBUS_DDC_CTRL_EOF;
	vCbusSendDdcMsg(u2Data, 2);
}

void vCbusSendDdcAbort(void)
{
	unsigned short u2Data[1];
	u2Data[0] = CBUS_DDC_CTRL_ABORT;
	vCbusSendDdcMsg(u2Data, 1);
}

void vCbusReqDdcReadState(unsigned short u2RxMsg)
{
	unsigned short u2Data[16];
	MHL_CBUS_LOG("DdcRead:%d,%X\n", (stCbus.stDdc.u2State), u2RxMsg);

	switch (stCbus.stDdc.u2State) {
	case MHL_CBUS_STATE_DDC_S0:	/* sof/dev_w,wait ack */
		if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = u2SetDdcData(stCbusDdc.u1Offset);	/* set offset */
			vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			vCbusSendDdcData(u2Data[0]);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S1;
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_S1:	/* offsetok,wait ack */
		if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = CBUS_DDC_CTRL_SOF;
			u2Data[1] = stCbusDdc.u1DevAddrR;
			vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			vCbusSendDdcMsg(u2Data, 2);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S2;
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_S2:	/* sof / dev_r,wait ack */
		if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			vCbusSendDdcCont();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_CONT;
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_CONT:	/* cont */
		if (fgIdDdcData(u2RxMsg)) {
			stCbusDdc.u1Buf[stCbusDdc.u1Inx] = u1GetDdcData(u2RxMsg);
			/* pr_info("%x:%d\n",stCbusDdc.u1Buf[stCbusDdc.u1Inx],stCbusDdc.u1Inx); */
			stCbusDdc.u1Len--;
			stCbusDdc.u1Inx++;
			if (stCbusDdc.u1Len == 0) {
				vCbusSendDdcStop();
				vResetCbusDDCState();
				stCbus.stDdc.fgOk = TRUE;
				vMhlUnlockCall();
			} else {
				vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
				vCbusSendDdcCont();
			}
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	default:
		break;
	}
}

void vCbusReqDdcSegReadState(unsigned short u2RxMsg)
{
	unsigned short u2Data[16];
	MHL_CBUS_LOG("SegRead:%d,%X\n", (stCbus.stDdc.u2State), u2RxMsg);

	switch (stCbus.stDdc.u2State) {
	case MHL_CBUS_STATE_DDC_S0:	/* sof/0x60_w,wait ack */
		if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = u2SetDdcData(stCbusDdc.u1Seg);	/* set seg */
			vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			vCbusSendDdcData(u2Data[0]);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S1;
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_S1:	/* seg,wait ack */
		if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = CBUS_DDC_CTRL_SOF;	/* sof */
			u2Data[1] = stCbusDdc.u1DevAddrW;	/* dev_w */
			vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			vCbusSendDdcMsg(&u2Data[0], 2);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S2;
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_S2:	/* sof /dev_w,wait ack */
		if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = u2SetDdcData(stCbusDdc.u1Offset);	/* set offset */
			vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			vCbusSendDdcData(u2Data[0]);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S3;
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_S3:	/* offset,wait ack */
		if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = CBUS_DDC_CTRL_SOF;
			u2Data[1] = stCbusDdc.u1DevAddrR;	/* sof / dev_r */
			vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			vCbusSendDdcMsg(u2Data, 2);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S4;
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_S4:	/* sof / dev_r,wait ack */
		if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			vCbusSendDdcCont();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_CONT;
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_CONT:	/* cont */
		if (fgIdDdcData(u2RxMsg)) {
			stCbusDdc.u1Buf[stCbusDdc.u1Inx] = u1GetDdcData(u2RxMsg);
			/* pr_info("ddc:%x:%d\n",stCbusDdc.u1Buf[stCbusDdc.u1Inx],stCbusDdc.u1Inx); */
			stCbusDdc.u1Len--;
			stCbusDdc.u1Inx++;
			if (stCbusDdc.u1Len == 0) {
				vCbusSendDdcStop();
				vResetCbusDDCState();
				stCbus.stDdc.fgOk = TRUE;
				vMhlUnlockCall();
			} else {
				vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
				vCbusSendDdcCont();
			}

			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	default:
		break;
	}
}

void vCbusReqDdcShortReadState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("ShortRead:%d,%X\n", (stCbus.stDdc.u2State), u2RxMsg);

	switch (stCbus.stDdc.u2State) {
	case MHL_CBUS_STATE_DDC_S0:	/* sof/dev_r,wait ack */
		if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			vCbusSendDdcCont();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_CONT;
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_CONT:	/* cont */
		if (fgIdDdcData(u2RxMsg)) {
			stCbusDdc.u1Buf[stCbusDdc.u1Inx] = u1GetDdcData(u2RxMsg);
			stCbusDdc.u1Len--;
			stCbusDdc.u1Inx++;
			if (stCbusDdc.u1Len == 0) {
				vCbusSendDdcStop();
				vResetCbusDDCState();
				stCbus.stDdc.fgOk = TRUE;
				vMhlUnlockCall();
			} else {
				vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
				vCbusSendDdcCont();
			}
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	default:
		break;
	}
}

void vCbusReqDdcWriteState(unsigned short u2RxMsg)
{
	unsigned short u2Data[16];
	MHL_CBUS_LOG("Write:%d,%X\n", (stCbus.stDdc.u2State), u2RxMsg);

	switch (stCbus.stDdc.u2State) {
	case MHL_CBUS_STATE_DDC_S0:	/* sof/dev_w,wait ack */
		if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = u2SetDdcData(stCbusDdc.u1Offset);	/* set offset */
			vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			vCbusSendDdcData(u2Data[0]);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S1;
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_S1:	/* offset,wait ack */
		if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			if (stCbusDdc.u1Len == 0) {
				vCbusSendDdcStop();
				vResetCbusDDCState();
				stCbus.stDdc.fgOk = TRUE;
				vMhlUnlockCall();
			} else {
				u2Data[0] = u2SetDdcData(stCbusDdc.u1Buf[stCbusDdc.u1Inx]);
				/* MHL_CBUS_LOG("DDCW:%X/%X/%X\n",stCbusDdc.u1Buf[stCbusDdc.u1Inx] ,stCbusDdc.u1Inx,stCbusDdc.u1Len); */
				vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
				vCbusSendDdcMsg(u2Data, 1);
				stCbusDdc.u1Inx++;
				stCbusDdc.u1Len--;
			}
			break;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ABORT) {
			stCbus.stDdc.fgOk = FALSE;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusSendDdcEof();
			stCbus.stDdc.fgOk = FALSE;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
		}
		vResetCbusDDCState();
		vMhlUnlockCall();
		break;
	default:
		break;
	}
}

bool fgCbusReqDdcReadCmd(bool fgEdid, unsigned char u1Seg, unsigned char u1Offset,
			 unsigned char u1Len, unsigned char *ptData)
{
	unsigned char i;

	MHL_CBUS_FUNC();

	if (u1CbusDdcAbortDelay2S != 0) {
		MHL_CBUS_LOG("DdcAbortDelay2S\n");
		stCbus.stDdc.fgOk = FALSE;
		return FALSE;
	}

	if ((u1Seg > 2) || (u1Len > 128))
		return FALSE;

	stCbusDdc.u1Seg = u1Seg;
	stCbusDdc.u1Offset = u1Offset;
	stCbusDdc.u1Len = u1Len;
	stCbusDdc.u1Inx = 0;

	if (fgEdid) {
		stCbusDdc.u1DevAddrW = CBUS_DDC_DATA_ADRW;
		stCbusDdc.u1DevAddrR = CBUS_DDC_DATA_ADRR;
	} else {
		stCbusDdc.u1DevAddrW = CBUS_DDC_DATA_HDCP_ADRW;
		stCbusDdc.u1DevAddrR = CBUS_DDC_DATA_HDCP_ADRR;
	}

	stCbus.u2CbusDdcState = MHL_CBUS_STATE_IDLE;
	stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S0;
	stCbus.stDdc.fgOk = FALSE;

	if (u1Seg == 0)		/* block 0/1 */
	{
		stCbusRequester.fgIsRequesterWait = TRUE;
		stCbusRequester.fgIsDdc = TRUE;
		stCbusRequester.u2ReqBuf[0] = CBUS_DDC_CTRL_SOF;
		stCbusRequester.u2ReqBuf[1] = stCbusDdc.u1DevAddrW;
		stCbusRequester.u4Len = 2;
		stCbus.stDdc.u2Cmd = CBUS_DDC_READ;
	} else {
		stCbusRequester.fgIsRequesterWait = TRUE;
		stCbusRequester.fgIsDdc = TRUE;
		stCbusRequester.u2ReqBuf[0] = CBUS_DDC_CTRL_SOF;
		stCbusRequester.u2ReqBuf[1] = CBUS_DDC_DATA_SEGW;
		stCbusRequester.u4Len = 2;
		stCbus.stDdc.u2Cmd = CBUS_DDC_SEG_READ;
	}
	vCbusReqWaitFinish();

	MHL_CBUS_LOG("fgCbusReqDdcReadCmd result:%d\n", stCbus.stDdc.fgOk);
	if (stCbus.stDdc.fgOk) {
		for (i = 0; i < u1Len; i++) {
			ptData[i] = stCbusDdc.u1Buf[i];
		}
		return TRUE;
	}
	return FALSE;
}

bool fgCbusReqDdcWriteCmd(unsigned char u1Offset, unsigned char u1Len, unsigned char *ptData)
{
	unsigned char i;

	MHL_CBUS_FUNC();

	if (u1CbusDdcAbortDelay2S != 0) {
		MHL_CBUS_LOG("DdcAbortDelay2S\n");
		stCbus.stDdc.fgOk = FALSE;
		return FALSE;
	}

	stCbusDdc.u1Seg = 0;
	stCbusDdc.u1Offset = u1Offset;
	stCbusDdc.u1Len = u1Len;
	stCbusDdc.u1Inx = 0;
	stCbusDdc.u1DevAddrW = CBUS_DDC_DATA_HDCP_ADRW;
	stCbusDdc.u1DevAddrR = CBUS_DDC_DATA_HDCP_ADRR;

	for (i = 0; i < u1Len; i++)
		stCbusDdc.u1Buf[i] = ptData[i];

	stCbus.u2CbusDdcState = MHL_CBUS_STATE_IDLE;
	stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S0;
	stCbus.stDdc.fgOk = FALSE;

	/* only for hdcp write */
	stCbusRequester.fgIsRequesterWait = TRUE;
	stCbusRequester.fgIsDdc = TRUE;
	stCbusRequester.u2ReqBuf[0] = CBUS_DDC_CTRL_SOF;
	stCbusRequester.u2ReqBuf[1] = CBUS_DDC_DATA_HDCP_ADRW;
	stCbusRequester.u4Len = 2;
	stCbus.stDdc.u2Cmd = CBUS_DDC_WRITR;
	vCbusReqWaitFinish();

	if (stCbus.stDdc.fgOk) {
		return TRUE;
	}
	return FALSE;
}

bool fgMhlTxWriteAn(unsigned char *ptData)
{
	return fgCbusReqDdcWriteCmd(RX_REG_HDCP_AN, HDCP_AN_COUNT, ptData);
}

bool fgMhlTxWriteAKsv(unsigned char *ptData)
{
	return fgCbusReqDdcWriteCmd(RX_REG_HDCP_AKSV, HDCP_AKSV_COUNT, ptData);
}

bool fgMhlTxReadRi(unsigned char *ptData)
{
	return fgCbusReqDdcReadCmd(FALSE, 0, RX_REG_RI, HDCP_RI_COUNT, ptData);
}

bool fgMhlTxReadBKsv(unsigned char *ptData)
{
	return fgCbusReqDdcReadCmd(FALSE, 0, RX_REG_HDCP_BKSV, HDCP_AKSV_COUNT, ptData);
}

bool fgMhlTxReadBStatus1(unsigned char *ptData)
{
	return fgCbusReqDdcReadCmd(FALSE, 0, RX_REG_BSTATUS1 + 1, 1, ptData);
}

bool fgMhlTxReadBStatus0(unsigned char *ptData)
{
	return fgCbusReqDdcReadCmd(FALSE, 0, RX_REG_BSTATUS1, 1, ptData);
}

bool fgMhlTxReadBStatus(unsigned char *ptData)
{
	return fgCbusReqDdcReadCmd(FALSE, 0, RX_REG_BSTATUS1, 2, ptData);
}

bool fgMhlTxReadBCAPS(unsigned char *ptData)
{
	return fgCbusReqDdcReadCmd(FALSE, 0, RX_REG_BCAPS, 1, ptData);
}

bool fgMhlTxReadKsvFIFO(unsigned char *ptData, unsigned char u1Len)
{
	return fgCbusReqDdcReadCmd(FALSE, 0, RX_REG_KSV_FIFO, u1Len, ptData);
}

bool fgMhlTxReadV(unsigned char *ptData)
{
	return fgCbusReqDdcReadCmd(FALSE, 0, RX_REG_REPEATER_V, 20, ptData);
}

/****************************************************
	cbus responder cmd process
****************************************************/
void vCbusRespGetStateState(unsigned short u2RxMsg)
{
	unsigned short u2Data[2];
	MHL_CBUS_LOG("RespGetState:%d\n", stCbus.stResp.u2State);
	switch (stCbus.stResp.u2State) {
	case MHL_CBUS_STATE_S0:	/* get msc cmd */
		u2Data[0] = u1DeviceRegSpace[0];
		u2Data[0] |= 0x400;
		vCbusSendMscMsg(u2Data, 1);
		vResetCbusMSCState();
		break;
	default:
		break;
	}
}

void vCbusRespGetVendorIDState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("RespGetVendorID:%d\n", stCbus.stResp.u2State);

	switch (stCbus.stResp.u2State) {
	case MHL_CBUS_STATE_S0:	/* get msc cmd */
		vCbusRespData(u1MhlVendorID);
		vResetCbusMSCState();
		break;
	default:
		break;
	}
}

void vCbusRespSetHPDState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("RespSetHPD:%d\n", stCbus.stResp.u2State);

	switch (stCbus.stResp.u2State) {
	case MHL_CBUS_STATE_S0:	/* get msc cmd */
		stMhlDev.fgIsHPD = TRUE;
		if (u2MhlEdidReadDelay == 0)
			stMhlDev.fgIsEdidChg = TRUE;
		else
			MHL_CBUS_LOG("too many HPD\n");
		stMhlDev.fgIsEdidChg = TRUE;
		u2MhlEdidReadDelay = 3000 / MHL_LINK_TIME;
		vCbusRespAck();
		vResetCbusMSCState();
		break;
	default:
		break;
	}
}

void vCbusRespClrHPDState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("RespClrHPD:%d\n", stCbus.stResp.u2State);

	switch (stCbus.stResp.u2State) {
	case MHL_CBUS_STATE_S0:	/* get msc cmd */
		stMhlDev.fgIsHPD = FALSE;
		vHDCPReset();
		vMhlAnalogPD();
		vCbusRespAck();
		vResetCbusMSCState();
		break;
	default:
		break;
	}
}

void vCbusRespGetDdcECState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("RespGetDdcEC:%d\n", stCbus.stResp.u2State);
	switch (stCbus.stResp.u2State) {
	case MHL_CBUS_STATE_S0:	/* get msc cmd */
		vCbusRespData(u1CbusDdcErrCode);
		vResetCbusMSCState();
		break;
	default:
		break;
	}
}

void vCbusRespGetMscECState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("RespGetMscEC:%d\n", stCbus.stResp.u2State);
	switch (stCbus.stResp.u2State) {
	case MHL_CBUS_STATE_S0:	/* get msc cmd */
		vCbusRespData(u1CbusMscErrCode);
		vResetCbusMSCState();
		break;
	default:
		break;
	}
}

void vCbusRespReadDevCapState(unsigned short u2RxMsg)
{
	unsigned short u2Data[2];
	MHL_CBUS_LOG("RespReadDevCap:%d\n", stCbus.stResp.u2State);
	switch (stCbus.stResp.u2State) {
	case MHL_CBUS_STATE_S0:	/* get msc cmd */
		vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stResp.u2State = MHL_CBUS_STATE_S1;
		break;
	case MHL_CBUS_STATE_S1:	/* offset */
		if (fgIsMscData(u2RxMsg)) {
			if (u1GetMscData(u2RxMsg) > 0x0f) {
				vCbusMscErrHandling(MHL_MSC_BAD_OFFSET);
			} else {
				stCbus.u2RXBuf[1] = (u2RxMsg & 0xff);
				u2Data[0] = CBUS_MSC_CTRL_ACK;
				u2Data[1] =
				    u2SetMscData(u1MyDeviceRegSpace
						 [u1GetMscData(stCbus.u2RXBuf[1])]);
				vCbusSendMscMsg(u2Data, 2);
			}
			vResetCbusMSCState();
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			vResetCbusMSCState();
		}
		break;
	default:
		break;
	}
}

void vCbusRespWriteStatState(unsigned short u2RxMsg)
{
	unsigned char u1Data, u1Data1;
	MHL_CBUS_LOG("RespWriteStat:%d\n", stCbus.stResp.u2State);
	switch (stCbus.stResp.u2State) {
	case MHL_CBUS_STATE_S0:	/* get msc cmd */
		vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stResp.u2State = MHL_CBUS_STATE_S1;
		break;
	case MHL_CBUS_STATE_S1:	/* get offset */
		if (fgIsMscData(u2RxMsg)) {
			/* set_int : 0x20,write_stat :0x30 */

			if (((u1GetMscData(u2RxMsg) >= 0x20)
			     && (u1GetMscData(u2RxMsg) <=
				 (0x20 + (u1DeviceRegSpace[CBUS_INT_STAT_SIZE] & 0x0F))))
			    || ((u1GetMscData(u2RxMsg) >= 0x30)
				&& (u1GetMscData(u2RxMsg) <=
				    (0x30 + (u1DeviceRegSpace[CBUS_INT_STAT_SIZE] >> 4))))
			    ) {
				stCbus.u2RXBuf[1] = u1GetMscData(u2RxMsg);
				vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
				stCbus.stResp.u2State = MHL_CBUS_STATE_S2;
			} else {
				vCbusMscErrHandling(MHL_MSC_BAD_OFFSET);
				vResetCbusMSCState();
			}
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			vResetCbusMSCState();
		}
		break;
	case MHL_CBUS_STATE_S2:	/* get data */
		if (fgIsMscData(u2RxMsg)) {
			u1Data1 = u1GetMscData(stCbus.u2RXBuf[1]);
			if (u1Data1 < 0x30)	/* set_int */
				u1DeviceRegSpace[u1Data1] |= u1GetMscData(u2RxMsg);
			else	/* write stat */
				u1DeviceRegSpace[u1Data1] = u1GetMscData(u2RxMsg);
			u1Data = u1GetMscData(u2RxMsg);
			if (u1Data1 == CBUS_MSC_RCHANGE_INT) {
				if (u1Data & CBUS_MSC_RCHANGE_INT_DCAP_CHG) {
					MHL_CBUS_LOG("fgIsDCapChg\n");
					if (u2MhlDcapReadDelay == 0)
						stMhlDev.fgIsDCapChg = TRUE;
					else
						MHL_CBUS_LOG("too many dcap chg\n");
					stMhlDev.fgIsDCapChg = TRUE;
					u2MhlDcapReadDelay = 3000 / MHL_LINK_TIME;
				}
				if (u1Data & CBUS_MSC_RCHANGE_INT_DSCR_CHG) {
					stMhlDev.fgIsDScrChg = TRUE;
					MHL_CBUS_LOG("fgIsDScrChg\n");
				}
				if (u1Data & CBUS_MSC_RCHANGE_INT_REQ_WRT) {
					stMhlDev.fgIsReqWrt = TRUE;
					MHL_CBUS_LOG("fgIsReqWrt\n");
				}
				if (u1Data & CBUS_MSC_RCHANGE_INT_GRT_WRT) {
					stMhlDev.fgIsGrtWrt = TRUE;
					MHL_CBUS_LOG("fgIsGrtWrt\n");
				}
				u1DeviceRegSpace[u1Data1] = 0;
			} else if (u1Data1 == CBUS_MSC_DCHANGE_INT) {
				if (u1Data & CBUS_MSC_DCHANGE_INT_EDID_CHG) {
					if (u2MhlEdidReadDelay == 0)
						stMhlDev.fgIsEdidChg = TRUE;
					else
						MHL_CBUS_LOG("too many HPD\n");
					stMhlDev.fgIsEdidChg = TRUE;
					u2MhlEdidReadDelay = 3000 / MHL_LINK_TIME;
					MHL_CBUS_LOG("fgIsEdidChg\n");
				}
				u1DeviceRegSpace[u1Data1] = 0;
			} else if (u1Data1 == CBUS_MSC_STATUS_CONNECTED_RDY) {
				if (u1Data & CBUS_MSC_STATUS_CONNECTED_RDY_DCAP_RDY) {
					stMhlDev.fgIsDCapRdy = TRUE;
#ifdef MHL_SUPPORT_SPEC_20
					stMhlDev.fgIs3DReq = TRUE;
#endif
					if (u2MhlDcapReadDelay == 0)
						stMhlDev.fgIsDCapChg = TRUE;
					else
						MHL_CBUS_LOG("too many dcap chg\n");
					stMhlDev.fgIsDCapChg = TRUE;
					u2MhlDcapReadDelay = 3000 / MHL_LINK_TIME;
				} else
					stMhlDev.fgIsDCapRdy = FALSE;
				MHL_CBUS_LOG("fgIsDCapRdy:%d\n", stMhlDev.fgIsDCapRdy);
			} else if (u1Data1 == CBUS_MSC_STATUS_LINK_MODE) {
				if ((u1Data & CBUS_MSC_STATUS_LINK_MODE_CLK_MODE) ==
				    CBUS_MSC_STATUS_LINK_MODE_CLK_MODE__PacketPixel)
					stMhlDev.fgIsPPMode = TRUE;
				else
					stMhlDev.fgIsPPMode = FALSE;
				MHL_CBUS_LOG("fgIsPPMode:%d\n", stMhlDev.fgIsPPMode);
				if (u1Data & CBUS_MSC_STATUS_LINK_MODE_PATH_EN)
					stMhlDev.fgIsPathEn = TRUE;
				else
					stMhlDev.fgIsPathEn = FALSE;
				MHL_CBUS_LOG("fgIsPathEn:%d\n", stMhlDev.fgIsPathEn);
				if (u1Data & CBUS_MSC_STATUS_IS_MUTED)
					stMhlDev.fgIsMuted = TRUE;
				else
					stMhlDev.fgIsMuted = FALSE;
				MHL_CBUS_LOG("fgIsMuted:%d\n", stMhlDev.fgIsMuted);
			}
			vCbusRespAck();
			vResetCbusMSCState();
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			vResetCbusMSCState();
		}
		break;
	default:
		break;
	}
}

void vCbusRespWriteBrustState(unsigned short u2RxMsg)
{
	unsigned char i;
	MHL_CBUS_LOG("RespWriteBrust:%d\n", stCbus.stResp.u2State);

	switch (stCbus.stResp.u2State) {
	case MHL_CBUS_STATE_S0:	/* get msc cmd */
		vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stResp.u2State = MHL_CBUS_STATE_S1;
		break;
	case MHL_CBUS_STATE_S1:	/* offset */
		if ((fgIsMscData(u2RxMsg))
		    && ((u2RxMsg & 0xff) > (u1MyDeviceRegSpace[CBUS_SCRATCHPAD_SIZE] + 0x40))) {
			vCbusMscErrHandling(MHL_MSC_BAD_OFFSET);
			vResetCbusMSCState();
		} else if (fgIsMscData(u2RxMsg)) {
			stCbus.u2RXBuf[1] = (u2RxMsg & 0xff);
			stCbus.u2RXBufInd = 0;
			vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			stCbus.stResp.u2State = MHL_CBUS_STATE_S2;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			vResetCbusMSCState();
		}
		break;
	case MHL_CBUS_STATE_S2:	/* data */
		/* pr_info("rb:%d,%x\n",stCbus.u2RXBufInd,u2RxMsg); */
		if (u2RxMsg == CBUS_MSC_CTRL_EOF)	/* end */
		{
			if ((stCbus.u2RXBufInd <= 2)
			    || ((stCbus.u2RXBufInd + 0x40) > (u1GetMscData(stCbus.u2RXBuf[1]) + u1MyDeviceRegSpace[CBUS_SCRATCHPAD_SIZE])))	/* least 2 data,max 16byte */
			{
				vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			} else {
				if (stCbus.u2RXBufInd > 2) {
					/* scratchpad */
					if ((stCbus.u2RXBuf[2] ==
					     u1MyDeviceRegSpace[CBUS_ADOPTER_ID_H])
					    && (stCbus.u2RXBuf[3] ==
						u1MyDeviceRegSpace[CBUS_ADOPTER_ID_L])) {
						for (i = 0; i < stCbus.u2RXBufInd; i++) {
							u1DeviceRegSpace[stCbus.u2RXBuf[1] + i] =
							    stCbus.u2RXBuf[i + 4];
						}
					}
					/* 3D VIC info */
					else if ((stCbus.u2RXBuf[2] == CBUS_3D_VIC_ID_H)
						 && (stCbus.u2RXBuf[3] == CBUS_3D_VIC_ID_L)) {
					}
					/* 3D DTD info */
					else if ((stCbus.u2RXBuf[2] == CBUS_3D_DTD_ID_H)
						 && (stCbus.u2RXBuf[3] == CBUS_3D_DTD_ID_L)) {
					}
				}
				vCbusRespAck();
			}
			vResetCbusMSCState();
		} else if (fgIsMscData(u2RxMsg)) {
			if ((stCbus.u2RXBufInd + stCbus.u2RXBuf[1]) >
			    (u1MyDeviceRegSpace[CBUS_SCRATCHPAD_SIZE] + 0x40)) {
				vCbusMscErrHandling(MHL_MSC_BAD_OFFSET);
				vResetCbusMSCState();
			} else {
				stCbus.u2RXBuf[stCbus.u2RXBufInd + 2] = (u2RxMsg & 0xff);
				vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
				stCbus.u2RXBufInd++;
			}
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			vResetCbusMSCState();
		}
		break;
	default:
		break;
	}
}

void vCbusRespMsgState(unsigned short u2RxMsg)
{
	MHL_CBUS_LOG("RespMsg:%d\n", stCbus.stResp.u2State);
	switch (stCbus.stResp.u2State) {
	case MHL_CBUS_STATE_S0:	/* get msc cmd */
		vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stResp.u2State = MHL_CBUS_STATE_S1;
		break;
	case MHL_CBUS_STATE_S1:	/* get sub cmd */
		if (fgIsMscData(u2RxMsg)) {
			stCbusMscMsg.u2Code = u2RxMsg;
			vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			stCbus.stResp.u2State = MHL_CBUS_STATE_S2;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			vResetCbusMSCState();
		}
		break;
	case MHL_CBUS_STATE_S2:	/* get data */
		if (fgIsMscData(u2RxMsg)) {
			stCbusMscMsg.u1Val = u1GetMscData(u2RxMsg);
			stCbusMscMsg.fgIsValid = TRUE;
			vCbusRespAck();
			vResetCbusMSCState();
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			vResetCbusMSCState();
		}
		break;
	default:
		break;
	}
}

/* /////////////////////////////////////////////////////////// */
void vCbusInit(void)
{
	unsigned short tmp;

	if (u1ForceCbusStop == 0xA5)
		if (fgCbusStop)
			return;

	vMhlGet6397Version();

	/* for cbus pads */
	/* vWrite2BCbus(0x014C, 0xF001); */
	/* vWrite2BCbus(0xC0E8, 0x00C0); */
	/* vWrite2BCbus(0xC100, 0x0001); */
	/* vWrite2BCbus(0xC0F8, 0x1249); */

	/* disable gpio27 pull */
	vWrite2BCbus(0xC028, u4Read2BCbus(0xC028) & (~(0x01 << 11)));
	tmp = u4Read2BCbus(0xc028);
	pr_info("c028:%x\n", tmp);
	/* config gpio27 to CBUS mode */
	/* gpio27 mode : 0xC0E8[8,6], 3: cbus */
	vWrite2BCbus(0xC0E8, (u4Read2BCbus(0xC0E8) & (~(0x07 << 6))) | (3 << 6));
	u1UsbMhlMode = 0xff;
	vSetMHLUSBMode(1);

	/* clk to HPD for debug clock */
	/* vWrite2BCbus(0x013A, 0x0B0A); */
	/* vWrite2BCbus(0xC0F8, 0x7009); */

	/* Cal 12mHZ clock */
	/*
	   vWrite2BCbus(0x039E, 0x0041);
	   vWrite2BCbus(0x039E, 0x0040);
	   vWrite2BCbus(0x039E, 0x0050);
	 */

	vWriteCbus(CBUS_LINK_00, CBUS_LINK_00_SETTING);
	vWriteCbus(CBUS_LINK_01, 0x03028794);
	vWriteCbus(CBUS_LINK_02, 0x5FC80A0A);
	vWriteCbus(CBUS_LINK_03, 0x04A4088A);
	vWriteCbus(CBUS_LINK_04, 0x40030964);
	/* vWriteCbus(CBUS_LINK_04,0x44030964); */
	vWriteCbus(CBUS_LINK_05, 0xC85A5A1E);
	vWriteCbus(CBUS_LINK_06, 0x1496965A);
	vWriteCbus(CBUS_LINK_07, 0xC0580130);
	vWriteCbus(CBUS_LINK_08, 0x00000000);
	vWriteCbus(CBUS_LINK_09, 0x0180080C);
	vWriteCbus(CBUS_LINK_0A, 0xFFFF000F);
	vWriteCbus(CBUS_LINK_0B, 0x00641000);
	vWriteCbus(CBUS_LINK_0C, 0xC0060821);
	vWriteCbus(CBUS_LINK_0D, 0x00000000);

	/* for rx read val error */
	vWriteCbusMsk(CBUS_LINK_02, (7 << LINK_02_LINK_HALFTRAN_MIN),
		      (0x7F << LINK_02_LINK_HALFTRAN_MIN));
	vWriteCbusMsk(CBUS_LINK_02, (18 << LINK_02_LINK_HALFTRAN_MAX),
		      (0x7F << LINK_02_LINK_HALFTRAN_MAX));
	vWriteCbusMsk(CBUS_LINK_04, (18 << LINK_04_LINK_RXDECISION),
		      (0x7F << LINK_04_LINK_RXDECISION));

	/* for no ack reply */
	vWriteCbusMsk(CBUS_LINK_02, LINK_02_LINK_ACK_MANU_EN_MASK, LINK_02_LINK_ACK_MANU_EN_MASK);
	vWriteCbusMsk(CBUS_LINK_04, (1 << LINK_04_LINK_ACK_WIDTH),
		      (0x7F << LINK_04_LINK_ACK_WIDTH));
	vWriteCbusMsk(CBUS_LINK_0C, (12 << LINK_0C_LINK_ACK_WIDTH_UPPER),
		      (0x7F << LINK_0C_LINK_ACK_WIDTH_UPPER));

	/* for tx read ack error */
	vWriteCbusMsk(CBUS_LINK_01, (5 << LINK_01_CBUS_ACK_0_MIN),
		      (0x7F << LINK_01_CBUS_ACK_0_MIN));
	vWriteCbusMsk(CBUS_LINK_01, (22 << LINK_01_CBUS_ACK_0_MAX),
		      (0x7F << LINK_01_CBUS_ACK_0_MAX));
	vWriteCbusMsk(CBUS_LINK_01, (40 << LINK_01_CBUS_ACK_FALL_MAX),
		      (0x7F << LINK_01_CBUS_ACK_FALL_MAX));

	/* vCbusRxEnable(); */

	/* for mt6397 eco,select tx trigger fail interrput function */
	if (fgMhlMt6397IsEco2() == TRUE) {
		vWriteCbusMsk(CBUS_LINK_BAK, CBUS_LINK_BAK_INT_SEL_ECO_MASK,
			      CBUS_LINK_BAK_INT_SEL_ECO_MASK);
	}

	vResetCbusDDCState();
	vResetCbusMSCState();

	MHL_CBUS_LOG("0x03A0:%X\n", u4Read2BCbus(0x03A0));

}

void vCbusReset(void)
{
	/* clear tx */
	vResetCbusDDCState();
	vResetCbusMSCState();
	vClrCbusTxOkWaitTmr();

	u1CbusDdcErrCode = 0;
	u1CbusMscErrCode = 0;
	u1CbusMscErrCodeDelay2S = 0;
	u1CbusDdcErrCodeDelay2S = 0;
	u1CbusMscAbortDelay2S = 0;
	u1CbusDdcAbortDelay2S = 0;
	u2MhlEdidReadDelay = 0;
	u2MhlDcapReadDelay = 0;

	u1CbusTxHwStatus = 0;
	u1CbusRxHwStatus = 0;
	u1CbusHw1KStatus = 0;
	u1CbusHwWakeupStatus = 0;

	stCbusTxBuf.fgTxValid = FALSE;
	stCbusTxBuf.fgTxBusy = FALSE;
	stCbusRequester.fgIsRequesterWait = FALSE;

	vWriteCbusMsk(CBUS_LINK_00, 0, LINK_00_TX_NUM_MASK);
	vClrCbusBit(CBUS_LINK_00, LINK_00_TX_TRIG_MASK);
	vCbusRxDisable();
	vCbusTxTriggerECO(FALSE);

	vWriteCbusMsk(CBUS_LINK_09,
		      (LINK_09_SW_RESET_MISC_MASK
		       | LINK_09_SW_RESET_WAKE_MASK
		       | LINK_09_SW_RESET_RX_MASK
		       | LINK_09_SW_RESET_TX_MASK),
		      (LINK_09_SW_RESET_MISC_MASK
		       | LINK_09_SW_RESET_WAKE_MASK
		       | LINK_09_SW_RESET_RX_MASK | LINK_09_SW_RESET_TX_MASK)
	    );
	vWriteCbusMsk(CBUS_LINK_09,
		      0,
		      (LINK_09_SW_RESET_MISC_MASK
		       | LINK_09_SW_RESET_WAKE_MASK
		       | LINK_09_SW_RESET_RX_MASK | LINK_09_SW_RESET_TX_MASK)
	    );

}

void CbusTxRxReset(void)
{
	vWriteCbusMsk(CBUS_LINK_09,
		      (LINK_09_SW_RESET_RX_MASK
		       | LINK_09_SW_RESET_TX_MASK),
		      (LINK_09_SW_RESET_RX_MASK | LINK_09_SW_RESET_TX_MASK)
	    );
	vWriteCbusMsk(CBUS_LINK_09, 0, (LINK_09_SW_RESET_RX_MASK | LINK_09_SW_RESET_TX_MASK)
	    );
}


void vCbusIntEnable(void)
{
	unsigned int u4CbusIntEnable = 0;
	u4CbusIntEnable = (LINK_08_LINKRX_TIMEOUT_INT_CLR_MASK
			   | LINK_08_RBUF_TRIG_INT_CLR_MASK
			   | LINK_08_TX_ARB_FAIL_INT_CLR_MASK
			   | LINK_08_TX_OK_INT_CLR_MASK
			   | LINK_08_TX_RETRY_TO_INT_CLR_MASK
			   | LINK_08_ZCBUS_DET_DONE_INT_CLR_MASK
			   | LINK_08_ZCBUS_DET_1K_INT_CLR_MASK
			   | LINK_08_DISC_DET_FAIL_INT_CLR_MASK | LINK_08_DISC_DET_INT_CLR_MASK);
	if (fgMhlMt6397IsEco2() == TRUE) {
		/* eco2, enable tx trigger fail */
		u4CbusIntEnable |= LINK_08_MONITOR_CMP_INT_CLR_MASK;
	}

	u4CbusIntEnable = u4CbusIntEnable << 16;
	/* enable interrput */
	vWriteCbusMsk(CBUS_LINK_08, u4CbusIntEnable, (0xFFFF << 16));
	/* mark status */
	vWriteCbusMsk(CBUS_LINK_0A,
		      (u4CbusIntEnable | (LINK_08_CBUS_POS_INT_CLR_MASK << 16) |
		       (LINK_08_CBUS_NEG_INT_CLR_MASK << 16)), (0xFFFF << 16));
}

void vCbusIntDisable(void)
{
	vWriteCbus(CBUS_LINK_08, 0);
}

void vCbusRxEnable(void)
{
	vSetCbusBit(CBUS_LINK_02, LINK_02_LINKRX_EN_MASK);
}

void vCbusRxDisable(void)
{
	vClrCbusBit(CBUS_LINK_02, LINK_02_LINKRX_EN_MASK);
}

/* //////////////////////////////////////////////////////////// */
/* / */
/* /             wake up  and discovery */
/* / */
/* //////////////////////////////////////////////////////////// */
void vCbusTriggerCheck1K(void)
{
	vWriteCbusMsk(CBUS_LINK_09, LINK_09_SW_RESET_WAKE_MASK, LINK_09_SW_RESET_WAKE_MASK);
	vWriteCbusMsk(CBUS_LINK_09, 0, LINK_09_SW_RESET_WAKE_MASK);

	vClrCbusBit(CBUS_LINK_07, LINK_07_DETECT_AGAIN_MASK);

	vClrCbusBit(CBUS_LINK_07, LINK_07_ZCBUS_DET_EN_SW_MASK);
	vSetCbusBit(CBUS_LINK_07, LINK_07_ZCBUS_DET_EN_SW_MASK);
	vClrCbusBit(CBUS_LINK_07, LINK_07_ZCBUS_DET_EN_SW_MASK);

/* vSetCbusBit(CBUS_LINK_07,LINK_07_DETECT_AGAIN_MASK); */
}

bool fgCbusCheck1K(void)
{
	return (u1CbusHw1KStatus & LINK_STA_00_ZCBUS_DET_1K_INT_MASK);
}

bool fgCbusCheck1KDone(void)
{
	return (u1CbusHw1KStatus &
		(LINK_STA_00_ZCBUS_DET_1K_INT_MASK | LINK_STA_00_ZCBUS_DET_DONE_INT_MASK));
}

bool fgCbusDiscovery(void)
{
	return (u1CbusHwWakeupStatus & LINK_STA_00_DISC_DET_INT_MASK);
}

bool fgCbusDiscoveryDone(void)
{
	return (u1CbusHwWakeupStatus &
		(LINK_STA_00_DISC_DET_INT_MASK | LINK_STA_00_DISC_DET_FAIL_INT_MASK));
}

unsigned char fgCbusValue(void)
{
	unsigned short tmp;
	tmp = u4Read2BCbus(CBUS_LINK_STA_01 + 2);
	if (mhl_log_on & 0x20000) {
		pr_info("cbus:%d\n", tmp & 0x8000);
	}
	if (tmp & 0x8000)
		return 1;
	return 0;
}

/* /////////////////////////////////////////////////////////// */
/* / */
/* /             cbus cmd process */
/* / */
/* /////////////////////////////////////////////////////////// */
bool fgCbusMscAbort = FALSE;
void vCbusMSCState(unsigned short u2RxMsg)
{
	/* MHL_CBUS_LOG("RxMsg=%x\n",u2RxMsg); */

	if (fgIsMscAbort(u2RxMsg)) {
		fgCbusMscAbort = TRUE;
		MHL_CBUS_ERR("msc abort fail\n");
		u1CbusMscAbortDelay2S = MHL_MSCDDC_ERR_2S;
		if (u1ForceCbusStop == 0xA5) {
			fgCbusStop = TRUE;
			vMhlUseSel2PinTriggerForDebug();
			while (fgCbusStop) {
				msleep(500);
			}
		}
	}

	if (stCbus.u2CbusState == MHL_CBUS_STATE_REQUESTER) {
		switch (stCbus.stReq.u2Cmd) {
		case CBUS_MSC_CTRL_GET_STATE:
			vCbusReqGetStateState(u2RxMsg);
			break;
		case CBUS_MSC_CTRL_GET_VENDER_ID:
			vCbusReqGetVendorIDState(u2RxMsg);
			break;
		case CBUS_MSC_CTRL_READ_DEVCAP:
			vCbusReqReadDevCapState(u2RxMsg);
			break;
		case CBUS_MSC_CTRL_WRITE_STATE:
			vCbusReqWriteStatState(u2RxMsg);
			break;
		case CBUS_MSC_CTRL_WRITE_BURST:
			vCbusReqWriteBrustState(u2RxMsg);
			break;
		case CBUS_MSC_CTRL_MSC_MSG:
			vCbusReqMscMsgState(u2RxMsg);
			break;
		case CBUS_MSC_CTRL_GET_MSC_EC:
			vCbusReqMSCERRCodeDState(u2RxMsg);
			break;
		default:
			MHL_CBUS_LOG("req error\n");
			break;
		}
	} else {
		if (stCbus.u2CbusState == MHL_CBUS_STATE_IDLE) {
			/* if(fgIsRespMscACKNACKABORTCmd(u2RxMsg) == FALSE) */
			if (fgIsMscAbort(u2RxMsg) == FALSE)	/* 6.3.5.6 */
			{
				if (fgIsRespMscCmdValid(u2RxMsg)) {
					stCbus.u2CbusState = MHL_CBUS_STATE_RESPONDER;
					stCbus.stResp.u2State = MHL_CBUS_STATE_S0;
					stCbus.stResp.u2Cmd = u2RxMsg;
					stCbus.u2RXBuf[0] = u2RxMsg;
					MHL_CBUS_LOG("RespNewCmd : %x\n", stCbus.u2RXBuf[0]);
				} else {
					/* 6.3.5.2/6.3.5.3/6.3.5.4/6.3.5.5/6.3.5.7/ */
					if (u2RxMsg != NONE_PACKET) {
						if (fgIsMscOpInvalid(u2RxMsg)) {
							vCbusMscErrHandling(MHL_MSC_BAD_OPCODE);
						} else {
							vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
						}
					}
				}
			}
		}
		if (stCbus.u2CbusState == MHL_CBUS_STATE_RESPONDER) {
			switch (stCbus.stResp.u2Cmd) {
			case CBUS_MSC_CTRL_GET_STATE:
				vCbusRespGetStateState(u2RxMsg);
				break;
			case CBUS_MSC_CTRL_GET_VENDER_ID:
				vCbusRespGetVendorIDState(u2RxMsg);
				break;
			case CBUS_MSC_CTRL_SET_HPD:
				vCbusRespSetHPDState(u2RxMsg);
				break;
			case CBUS_MSC_CTRL_CLR_HPD:
				vCbusRespClrHPDState(u2RxMsg);
				break;
			case CBUS_MSC_CTRL_GET_DDC_EC:
				vCbusRespGetDdcECState(u2RxMsg);
				break;
			case CBUS_MSC_CTRL_GET_MSC_EC:
				vCbusRespGetMscECState(u2RxMsg);
				break;
			case CBUS_MSC_CTRL_READ_DEVCAP:
				vCbusRespReadDevCapState(u2RxMsg);
				break;
			case CBUS_MSC_CTRL_WRITE_STATE:
				vCbusRespWriteStatState(u2RxMsg);
				break;
			case CBUS_MSC_CTRL_WRITE_BURST:
				vCbusRespWriteBrustState(u2RxMsg);
				break;
			case CBUS_MSC_CTRL_MSC_MSG:
				vCbusRespMsgState(u2RxMsg);
				break;
			default:
				vResetCbusMSCState();
				MHL_CBUS_LOG("resp reserved cmd\n");
				break;
			}
		}
	}
}

/* /////////////////////////////////////////////////////////// */
/* / */
/* /             cbus cmd process */
/* / */
/* /////////////////////////////////////////////////////////// */
void vCbusDDCState(unsigned short u2RxMsg)
{
	/* MHL_CBUS_LOG("RxMsg=%x\n",u2RxMsg); */

	if (fgIsDdcAbort(u2RxMsg)) {
		MHL_CBUS_ERR("ddc abort fail\n");
		u1CbusDdcAbortDelay2S = MHL_MSCDDC_ERR_2S;
		if (u1ForceCbusStop == 0xA5) {
			vMhlUseSel2PinTriggerForDebug();
			fgCbusStop = TRUE;
			while (fgCbusStop) {
				msleep(500);
			}
		}
	}

	if (stCbus.u2CbusDdcState == MHL_CBUS_STATE_REQUESTER) {
		switch (stCbus.stDdc.u2Cmd) {
		case CBUS_DDC_READ:
			vCbusReqDdcReadState(u2RxMsg);
			break;
		case CBUS_DDC_SEG_READ:
			vCbusReqDdcSegReadState(u2RxMsg);
			break;
		case CBUS_DDC_SHORT_READ:
			vCbusReqDdcShortReadState(u2RxMsg);
			break;
		case CBUS_DDC_WRITR:
			vCbusReqDdcWriteState(u2RxMsg);
			break;
		default:
			MHL_CBUS_LOG("ddc error\n");
			break;
		}
	}

}

/* /////////////////////////////////////////////// */
unsigned short u2CbusReadRxLen(void)
{
	unsigned int tmp;
	tmp = u4Read2BCbus(CBUS_LINK_STA_00 + 2);
	tmp = (tmp >> 4) & 0x1F;
	return (unsigned short)tmp;
}

void vCbusSetTxLen(unsigned short i)
{
	unsigned int tmp;
	tmp = u4Read2BCbus(CBUS_LINK_00);
	tmp = tmp & (~(LINK_00_TX_NUM_MASK));
	tmp = tmp | ((i << LINK_00_TX_NUM) & LINK_00_TX_NUM_MASK);
	vWrite2BCbus(CBUS_LINK_00, tmp);
}

/* /////////////////////////////////////////////// */
unsigned int u4loopcounter = 0;
unsigned int u4CbusRxCount = 0;
unsigned int u4CbusTest = 0;
unsigned int u4IntStatusBak = 0;
/* /////////////////////////////////////////////// */
void vMhlIntProcess(void)
{
	unsigned int u4IntStat;
	unsigned short u2RxMsg = 0;
	unsigned char i = 0;
	unsigned char j = 0;
	unsigned int tmp;
	unsigned int tmp_1;
	unsigned short aRxMsg[MHL_RX_HW_BUF_MAX];

	u4loopcounter++;

	u4CbusRxCount = 0;
/****************************************************
	clear hw interrupt
****************************************************/
	u4IntStat = u4Read2BCbus(CBUS_LINK_STA_00);
	vWrite2BCbus(CBUS_LINK_08, u4IntStat);
	vWrite2BCbus(CBUS_LINK_08, 0);

	u4IntStat = u4IntStat & 0xFFFF;

	MHL_CBUS_INT("%X\n", u4IntStat);

	if (u1CbusConnectState == CBUS_LINK_STATE_USB_MODE)
		return;
/****************************************************
	check wake up and discovery
****************************************************/
	if (u4IntStat & (LINK_STA_00_ZCBUS_DET_DONE_INT_MASK | LINK_STA_00_ZCBUS_DET_1K_INT_MASK)) {
		u1CbusHw1KStatus =
		    u4IntStat & (LINK_STA_00_ZCBUS_DET_DONE_INT_MASK |
				 LINK_STA_00_ZCBUS_DET_1K_INT_MASK);
		vCbusCheck1KEvent();
	}
	if (u4IntStat & (LINK_STA_00_DISC_DET_INT_MASK | LINK_STA_00_DISC_DET_FAIL_INT_MASK)) {
		u1CbusHwWakeupStatus =
		    u4IntStat & (LINK_STA_00_DISC_DET_INT_MASK |
				 LINK_STA_00_DISC_DET_FAIL_INT_MASK);
		if (fgCbusDiscovery() && (u1CbusConnectState == CBUS_LINK_STATE_CHECK_1K))	/* connected */
		{
			u1CbusConnectState = CBUS_LINK_STATE_CHECK_RENSE;
			/* vCbusRxEnable(); */
		}

		vCbusWakeupEvent();
	}
/****************************************************
	check hw Rx/Tx status
****************************************************/
	u1CbusTxHwStatus = 0;
	u1CbusRxHwStatus = 0;

	if (u4IntStat & LINK_08_LINKRX_TIMEOUT_INT_CLR_MASK) {
		u1CbusRxHwStatus |= LINK_08_LINKRX_TIMEOUT_INT_CLR_MASK;
		MHL_CBUS_ERR(">RTO\n");
	}

	if (u4IntStat & LINK_08_TX_RETRY_TO_INT_CLR_MASK)	/* 6 */
	{
		u1CbusTxHwStatus |= LINK_08_TX_RETRY_TO_INT_CLR_MASK;
		MHL_CBUS_ERR(">TRTO\n");
	}
	if (u4IntStat & LINK_08_TX_OK_INT_CLR_MASK)	/* bit */
	{
		u1CbusTxHwStatus |= LINK_08_TX_OK_INT_CLR_MASK;
	}
	if (u4IntStat & LINK_08_TX_ARB_FAIL_INT_CLR_MASK) {
		u1CbusTxHwStatus |= LINK_08_TX_ARB_FAIL_INT_CLR_MASK;
		MHL_CBUS_ERR(">TAL\n");
	}
	if (u4IntStat & LINK_08_MONITOR_CMP_INT_CLR_MASK) {
		if (fgMhlMt6397IsEco2() == TRUE) {
			u1CbusTxHwStatus |= LINK_08_MONITOR_CMP_INT_CLR_MASK;
			MHL_CBUS_ERR(">TTF\n");
		}
	}
	/* if((fgIsCbusConnected() == FALSE)/*||(fgGetMhlRsen() == FALSE)*/) */
/* return ; */
/****************************************************
	read rx duffer data
****************************************************/
	/* hw error process */
	if (fgCbusTxErr()) {
		/* retry */
		/*
		   if(stCbusTxBuf.u2Retry > 0)
		   {
		   stCbusTxBuf.u2Retry--;
		   vCbusTxTrigger(); //fail,trigger again
		   }
		   else
		 */
		{
			stCbusTxBuf.fgTxBusy = FALSE;
			vClrCbusTxOkWaitTmr();
			vResetCbusMSCState();
			vResetCbusDDCState();
			CbusTxRxReset();
			stCbus.stDdc.fgOk = FALSE;
			stCbus.stReq.fgOk = FALSE;
			vMhlUnlockCall();
		}
		MHL_CBUS_ERR("tx err\n");
	}
	if (fgCbusTxOk()) {
		stCbusTxBuf.fgTxBusy = FALSE;
		vClrCbusTxOkWaitTmr();
	}

	if (fgCbusRxErr()) {
		stCbusTxBuf.fgTxBusy = FALSE;
		vClrCbusTxOkWaitTmr();
		vResetCbusMSCState();
		vResetCbusDDCState();
		CbusTxRxReset();
		stCbus.stDdc.fgOk = FALSE;
		stCbus.stReq.fgOk = FALSE;
		vMhlUnlockCall();
		MHL_CBUS_ERR("rx err\n");
	}

	if (fgCbusMSCWaitTmrOut() || fgCbusDDCWaitTmrOut()) {
		if (fgCbusMSCWaitTmrOut()) {
			MHL_CBUS_ERR("mto\n");
			u1CbusMscErrCode = MHL_MSC_MSG_TIMEOUT;
			vResetCbusMSCState();
			stCbus.stReq.fgOk = FALSE;
			vMhlUnlockCall();
		}
		/* ddc cmd must process timer out */
		if (fgCbusDDCWaitTmrOut()) {
			MHL_CBUS_ERR("dto\n");
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			stCbus.stDdc.fgOk = FALSE;
			vResetCbusDDCState();
			vMhlUnlockCall();
		}
		MHL_CBUS_ERR("time out\n");
	} else {
		/*
		   if(fgCbusTxOk())
		   {
		   stCbusTxBuf.fgTxBusy = FALSE;
		   vClrCbusTxOkWaitTmr();
		   }
		 */
		if ((u4IntStat & LINK_08_RBUF_TRIG_INT_CLR_MASK))	/* bit 12 */
		{
			u1CbusRxHwStatus |= LINK_08_RBUF_TRIG_INT_CLR_MASK;

			u4CbusRxCount = u2CbusReadRxLen();
			j = u4CbusRxCount;
			i = 0;
			while (j > 0) {
				j--;
				u2RxMsg = u4Read2BCbus(CBUS_RBUF) & 0x7FF;
				aRxMsg[i] = u2RxMsg;
				i++;
			}
			if (u4CbusRxCount > 0)	/* fgCbusRxOk()) */
			{
				i = 0;
				j = u4CbusRxCount;
				while (j) {
					j--;
					u2RxMsg = aRxMsg[i];
					i++;
					if (fgIsDdcChannel(u2RxMsg))
						vCbusDDCState(u2RxMsg);
					else if (fgIsMscChannel(u2RxMsg))
						vCbusMSCState(u2RxMsg);
					else
						MHL_CBUS_ERR("not cmd\n");
				}
			}
			j = u4CbusRxCount;
			i = 0;
			while (j > 0) {
				j--;
				MHL_CBUS_TXRX("R:%X:%d\n", aRxMsg[i], i);
				i++;
			}
		}

	}
/* ////////////////////////////////////////////////////////////////// */
/* TX process */
	if (stCbusTxBuf.fgTxValid && (stCbusTxBuf.u2Len > 0) && (stCbusTxBuf.fgTxBusy == FALSE)) {
		stCbusTxBuf.u2Retry = MHL_TX_RETRY_NUM;
		stCbusTxBuf.u2TxOkRetry = 2;
		stCbusTxBuf.fgTxBusy = TRUE;
		u1CbusSendMsg(stCbusTxBuf.u2TxBuf, stCbusTxBuf.u2Len);
		if (fgMhlMt6397IsEco2() == TRUE)
			vClrCbusTxOkWaitTmr();
		else
			vSetCbusTxOkWaitTmr(MHL_TXOK_TMR_OUT_60MS);
	} else if (stCbusTxBuf.fgTxBusy) {
		if (fgMhlMt6397IsEco2() == TRUE) {
			if (fgCbusTriFail()) {
				if (stCbusTxBuf.u2TxOkRetry > 0) {
					stCbusTxBuf.u2TxOkRetry--;
					vCbusTxTriggerECO(TRUE);
					vCbusTxTriggerECO(FALSE);
					MHL_CBUS_ERR("tri a\n");
				} else {
					stCbusTxBuf.fgTxBusy = FALSE;
					MHL_CBUS_ERR("tx fail\n");
				}
			}
		} else {
			if (fgCbusTxOkWaitTmrOut()) {
				if (stCbusTxBuf.u2TxOkRetry > 0) {
					stCbusTxBuf.u2TxOkRetry--;
					vCbusTxTrigger();
					vSetCbusTxOkWaitTmr(MHL_TXOK_TMR_OUT_60MS);
					MHL_CBUS_ERR("tri a\n");
				} else {
					vClrCbusTxOkWaitTmr();
					stCbusTxBuf.fgTxBusy = FALSE;
					MHL_CBUS_ERR("tx fail\n");
				}
			}
		}
	}
/* //////////////////////////////////////////////////////// */
/* new cmd process */
	if (stCbusRequester.fgIsRequesterWait)	/* get cbus idle */
	{
		if (fgCbusMscIdle()
		    && fgCbusDdcIdle()
		    && (stCbusTxBuf.fgTxValid == FALSE)
		    && (stCbusTxBuf.fgTxBusy == FALSE)) {
			stCbusTxBuf.u2Retry = MHL_TX_RETRY_NUM;
			for (i = 0; i < stCbusRequester.u4Len; i++)
				stCbusTxBuf.u2TxBuf[i] = stCbusRequester.u2ReqBuf[i];
			stCbusTxBuf.u2Len = stCbusRequester.u4Len;
			if (stCbusTxBuf.u2Len >= MHL_TX_HW_BUF_MAX)
				MHL_CBUS_ERR("tx data len err\n");

			tmp = u4CbusSendBufMsg(stCbusTxBuf.u2TxBuf, stCbusTxBuf.u2Len);

			vWrite2BCbus(CBUS_LINK_08,
				     (LINK_08_CBUS_POS_INT_CLR_MASK |
				      LINK_08_CBUS_NEG_INT_CLR_MASK));
			vWrite2BCbus(CBUS_LINK_08, 0);
			tmp_1 = u4Read2BCbus(CBUS_LINK_STA_01);
			u4CbusRxCount = u2CbusReadRxLen();
			u4IntStat = u4Read2BCbus(CBUS_LINK_STA_00);
			if ((u4CbusRxCount == 0)
			    && (tmp_1 == 0)
			    &&
			    ((u4IntStat &
			      (LINK_08_CBUS_POS_INT_CLR_MASK | LINK_08_CBUS_NEG_INT_CLR_MASK)) ==
			     0)) {
				if (fgMhlMt6397IsEco2() == TRUE) {
					vCbusTxTriggerECO(TRUE);
					vCbusTxTriggerECO(FALSE);
				} else {
					vCbusTxQuickTrigger(tmp);
				}
				stCbusRequester.fgIsRequesterWait = FALSE;
				if (stCbusRequester.fgIsDdc == FALSE) {
					stCbus.u2CbusState = MHL_CBUS_STATE_REQUESTER;
					stCbus.stReq.u2Cmd = stCbusRequester.u2ReqBuf[0];
					stCbus.stReq.u2State = MHL_CBUS_STATE_S0;
					stCbus.stReq.fgOk = FALSE;
					vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
				} else {
					stCbus.u2CbusDdcState = MHL_CBUS_STATE_REQUESTER;
					stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S0;
					stCbus.stDdc.fgOk = FALSE;
					vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
				}
				MHL_CBUS_TXRX("tri\n");
			} else {
				MHL_CBUS_ERR("u4CbusRxCount=%X\n", u4CbusRxCount);
				MHL_CBUS_ERR("tmp_1=%X\n", tmp_1);
				MHL_CBUS_ERR("u4IntStat=%X\n", u4IntStat);
			}
		}
	}
/* ///////////////////////////////////////////////////////////////// */
	stCbusTxBuf.fgTxValid = FALSE;
}

void vDcapParser(void)
{
	memset(&stMhlDcap, 0, sizeof(stMhlDcap_st));

	stMhlDcap.u1MhlVersion = u1DeviceRegSpace[CBUS_MHL_VERSION];
	stMhlDcap.u4AdopterID =
	    (u1DeviceRegSpace[CBUS_ADOPTER_ID_H] << 8) | u1DeviceRegSpace[CBUS_ADOPTER_ID_L];
	stMhlDcap.u4DeviceID =
	    (u1DeviceRegSpace[CBUS_DEVICE_ID_H] << 8) | u1DeviceRegSpace[CBUS_DEVICE_ID_L];

	/* Check POW bit from Sink */
	if (u1DeviceRegSpace[CBUS_MSC_DEV_CAT] & CBUS_MSC_DEV_CAT_POW) {
		stMhlDcap.fgIsPower = TRUE;
	} else {
		stMhlDcap.fgIsPower = FALSE;
	}

	stMhlDcap.u1DevType = u1DeviceRegSpace[CBUS_MSC_DEV_CAT] & CBUS_MSC_DEV_CAT_DEV_TYPE;

	/* video support */
	if (u1DeviceRegSpace[CBUS_VID_LINK_MODE] & CBUS_SUPP_RGB444) {
		stMhlDcap.u1VidLinkMode |= CBUS_SUPP_RGB444;
	}
	if (u1DeviceRegSpace[CBUS_VID_LINK_MODE] & CBUS_SUPP_YCBCR444) {
		stMhlDcap.u1VidLinkMode |= CBUS_SUPP_YCBCR444;
	}
	if (u1DeviceRegSpace[CBUS_VID_LINK_MODE] & CBUS_SUPP_YCBCR422) {
		stMhlDcap.u1VidLinkMode |= CBUS_SUPP_YCBCR422;
	}
	if (u1DeviceRegSpace[CBUS_VID_LINK_MODE] & CBUS_SUPP_PP) {
		stMhlDcap.u1VidLinkMode |= CBUS_SUPP_PP;
	}
	if (u1DeviceRegSpace[CBUS_VID_LINK_MODE] & CBUS_SUPP_ISLANDS) {
		stMhlDcap.u1VidLinkMode |= CBUS_SUPP_ISLANDS;
	}
	if (u1DeviceRegSpace[CBUS_VID_LINK_MODE] & CBUS_SUPP_VGA) {
		stMhlDcap.u1VidLinkMode |= CBUS_SUPP_VGA;
	}
	/* video support */
	if (u1DeviceRegSpace[CBUS_LOG_DEV_MAP] & CBUS_LOG_DEV_MAP_LD_DISPLAY) {
		stMhlDcap.u1LogDevMap |= CBUS_LOG_DEV_MAP_LD_DISPLAY;
	}
	if (u1DeviceRegSpace[CBUS_LOG_DEV_MAP] & CBUS_LOG_DEV_MAP_LD_VIDEO) {
		stMhlDcap.u1LogDevMap |= CBUS_LOG_DEV_MAP_LD_VIDEO;
	}
	if (u1DeviceRegSpace[CBUS_LOG_DEV_MAP] & CBUS_LOG_DEV_MAP_LD_AUDIO) {
		stMhlDcap.u1LogDevMap |= CBUS_LOG_DEV_MAP_LD_AUDIO;
	}
	if (u1DeviceRegSpace[CBUS_LOG_DEV_MAP] & CBUS_LOG_DEV_MAP_LD_MEDIA) {
		stMhlDcap.u1VidLinkMode |= CBUS_LOG_DEV_MAP_LD_MEDIA;
	}
	if (u1DeviceRegSpace[CBUS_LOG_DEV_MAP] & CBUS_LOG_DEV_MAP_LD_TUNER) {
		stMhlDcap.u1LogDevMap |= CBUS_LOG_DEV_MAP_LD_TUNER;
	}
	if (u1DeviceRegSpace[CBUS_LOG_DEV_MAP] & CBUS_LOG_DEV_MAP_LD_RECORD) {
		stMhlDcap.u1LogDevMap |= CBUS_LOG_DEV_MAP_LD_RECORD;
	}
	if (u1DeviceRegSpace[CBUS_LOG_DEV_MAP] & CBUS_LOG_DEV_MAP_LD_SPEAKER) {
		stMhlDcap.u1LogDevMap |= CBUS_LOG_DEV_MAP_LD_SPEAKER;
	}
	if (u1DeviceRegSpace[CBUS_LOG_DEV_MAP] & CBUS_LOG_DEV_MAP_LD_GUI) {
		stMhlDcap.u1LogDevMap |= CBUS_LOG_DEV_MAP_LD_GUI;
	}
	/* video type */
	if (u1DeviceRegSpace[CBUS_VIDEO_TYPE] & VT_GRAPHICS) {
		stMhlDcap.u1VideoType |= VT_GRAPHICS;
	}
	if (u1DeviceRegSpace[CBUS_VIDEO_TYPE] & VT_PHOTO) {
		stMhlDcap.u1VideoType |= VT_PHOTO;
	}
	if (u1DeviceRegSpace[CBUS_VIDEO_TYPE] & VT_CINEMA) {
		stMhlDcap.u1VideoType |= VT_CINEMA;
	}
	if (u1DeviceRegSpace[CBUS_VIDEO_TYPE] & VT_GAME) {
		stMhlDcap.u1VideoType |= VT_GAME;
	}
	if (u1DeviceRegSpace[CBUS_VIDEO_TYPE] & VT_SUPP) {
		stMhlDcap.u1VideoType |= VT_SUPP;
	}
	/* aud support */
	if (u1DeviceRegSpace[CBUS_AUD_LINK_MODE] & CBUS_SUPP_AUD_2CH) {
		stMhlDcap.u1AudLinkMode |= CBUS_SUPP_AUD_2CH;
	}
	if (u1DeviceRegSpace[CBUS_AUD_LINK_MODE] & CBUS_SUPP_AUD_8CH) {
		stMhlDcap.u1AudLinkMode |= CBUS_SUPP_AUD_8CH;
	}
	/* CBUS_BANDWIDTH */
	stMhlDcap.u4BandWidth = u1DeviceRegSpace[CBUS_BANDWIDTH] * 5;

	/* Check Scratchpad/RAP/RCP support */
	if (u1DeviceRegSpace[CBUS_FEATURE_FLAG] & CBUS_FEATURE_FLAG_RCP_SUPPORT) {
		stMhlDcap.u1FeatureFlag |= CBUS_FEATURE_FLAG_RCP_SUPPORT;
	}
	if (u1DeviceRegSpace[CBUS_FEATURE_FLAG] & CBUS_FEATURE_FLAG_RAP_SUPPORT) {
		stMhlDcap.u1FeatureFlag |= CBUS_FEATURE_FLAG_RAP_SUPPORT;
	}
	if (u1DeviceRegSpace[CBUS_FEATURE_FLAG] & CBUS_FEATURE_FLAG_SP_SUPPORT) {
		stMhlDcap.u1FeatureFlag |= CBUS_FEATURE_FLAG_SP_SUPPORT;
	}
	if (u1DeviceRegSpace[CBUS_FEATURE_FLAG] & CBUS_FEATURE_UCP_SEND_SUPPORT) {
		stMhlDcap.u1FeatureFlag |= CBUS_FEATURE_UCP_SEND_SUPPORT;
	}
	if (u1DeviceRegSpace[CBUS_FEATURE_FLAG] & CBUS_FEATURE_UCP_RECV_SUPPORT) {
		stMhlDcap.u1FeatureFlag |= CBUS_FEATURE_UCP_RECV_SUPPORT;
	}

	stMhlDcap.u1ScratchpadSize = u1DeviceRegSpace[CBUS_SCRATCHPAD_SIZE];
	stMhlDcap.u1intStatSize = u1DeviceRegSpace[CBUS_INT_STAT_SIZE];

}

bool fgMhlGetDevCap(void)
{
	unsigned char i;
	for (i = 0; i < 16; i++) {
		if (fgCbusReqReadDevCapCmd(i) == FALSE)
			return FALSE;
	}

	vDcapParser();

	return TRUE;
}

void vMhlSetDevCapReady(void)
{
	/* set 0x30[0] */
	fgCbusReqWriteStatCmd(CBUS_MSC_STATUS_CONNECTED_RDY,
			      CBUS_MSC_STATUS_CONNECTED_RDY_DCAP_RDY);
	/* set 0x20[0] */
	fgCbusReqWriteStatCmd(CBUS_MSC_RCHANGE_INT, CBUS_MSC_RCHANGE_INT_DCAP_CHG);
}

bool fgCbusMscIdle(void)
{
	if (stCbus.u2CbusState == MHL_CBUS_STATE_IDLE)
		return TRUE;
	else
		return FALSE;
}

bool fgCbusDdcIdle(void)
{
	if (stCbus.u2CbusDdcState == MHL_CBUS_STATE_IDLE)
		return TRUE;
	else
		return FALSE;
}

bool fgCbusHwTxIdle(void)
{
	unsigned int tmp;
	tmp = u4ReadCbus(CBUS_LINK_STA_01);
	if ((tmp & LINK_STA_01_LINKTX_FSM_MASK) == 0)
		return TRUE;
	return FALSE;
}

bool fgCbusHwRxIdle(void)
{
	unsigned int tmp;
	tmp = u4ReadCbus(CBUS_LINK_STA_01);
	if ((tmp & LINK_STA_01_LINKRX_FSM_MASK) == 0)
		return TRUE;
	return FALSE;
}

bool fgCbusHwTxRxIdle(void)
{
	unsigned int tmp;
	tmp = u4ReadCbus(CBUS_LINK_STA_01);
	if ((tmp & (LINK_STA_01_LINKTX_FSM_MASK | LINK_STA_01_LINKRX_FSM_MASK)) == 0)
		return TRUE;
	return FALSE;
}

void vCbusCmdStatus(void)
{

	pr_info("=========================================\n");
	pr_info("log setting:%08X\n", mhl_log_on);
	pr_info("cbus err debug mode by waveform : en=%d,stop=%d\n", u1ForceCbusStop, fgCbusStop);
	pr_info("cbus link stop to floatcbus : en=%d\n", fgCbusFlowStop);
	pr_info("force tmds output : en=%d\n", u1ForceTmdsOutput);
	pr_info("u2CbusState=%d\n", stCbus.u2CbusState);
	pr_info("stReq.u1Cmd=%d\n", stCbus.stReq.u2Cmd);
	pr_info("stReq.u2State=%d\n", stCbus.stReq.u2State);
	pr_info("stReq.fgOk=%d\n", stCbus.stReq.fgOk);
	pr_info("stResp.u2State=%d\n", stCbus.stResp.u2State);
	pr_info("stCbusTxBuf.fgTxValid=%d\n", stCbusTxBuf.fgTxValid);
	pr_info("stCbusTxBuf.fgTxBusy=%d\n", stCbusTxBuf.fgTxBusy);
	pr_info("stCbusTxBuf.u2Len=%d\n", stCbusTxBuf.u2Len);
	pr_info("stCbusTxBuf.u2Retry=%d\n", stCbusTxBuf.u2Retry);
	pr_info("stCbusRequester.fgIsRequesterWait=%d\n", stCbusRequester.fgIsRequesterWait);
	pr_info("stCbusRequester.fgIsDdc=%d\n", stCbusRequester.fgIsDdc);
	pr_info("stCbusRequester.u4Len=%d\n", stCbusRequester.u4Len);
	pr_info("fgCbusMscIdle()=%d\n", fgCbusMscIdle());
	pr_info("fgCbusDdcIdle()=%d\n", fgCbusDdcIdle());
	pr_info("fgCbusHwTxIdle()=%d\n", fgCbusHwTxIdle());
	pr_info("u1CbusDdcAbortDelay2S=%d\n", u1CbusDdcAbortDelay2S);
	pr_info("u1CbusMscAbortDelay2S=%d\n", u1CbusMscAbortDelay2S);
	pr_info("u4CbusRxCount=%d\n", u4CbusRxCount);

}

void v6397DumpReg(void)
{
	unsigned int addr, u4data;

	pr_info("\n");
	pr_info("0x013A:%08X\n", u4Read2BCbus(0x013A));
	pr_info("0x014C:%08X\n", u4Read2BCbus(0x014C));
	pr_info("0xC008:%08X\n", u4Read2BCbus(0xC008));
	pr_info("0xC028:%08X\n", u4Read2BCbus(0xC028));
	pr_info("0xC088:%08X\n", u4Read2BCbus(0xC088));
	pr_info("0xC0D8:%08X\n", u4Read2BCbus(0xC0D8));
	pr_info("0xC0E8:%08X\n", u4Read2BCbus(0xC0E8));
	pr_info("0xC0F8:%08X\n", u4Read2BCbus(0xC0F8));
	pr_info("0xC100:%08X\n", u4Read2BCbus(0xC100));

	for (addr = 0x100; addr < 0x200; addr = addr + 4) {
		if ((addr & 0x0F) == 0) {
			pr_info("\n");
			pr_info("%08X : ", addr);
		}
		u4data = u4ReadCbus(addr);
		pr_info("%08X ", u4data);
	}


	for (addr = 0xA00; addr < 0xA80; addr = addr + 4) {
		if ((addr & 0x0F) == 0) {
			pr_info("\n");
			pr_info("%08X : ", addr);
		}
		u4data = u4ReadCbus(addr);
		pr_info("%08X ", u4data);
	}

	pr_info("\n");
}

#endif
