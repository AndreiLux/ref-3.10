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
#include "mhl_hdcp.h"

#include "mhl_cbus_ctrl.h"
#include "mhl_dbg.h"
#include "mt8135_mhl_reg.h"
#include "mt6397_cbus_reg.h"
#include "mhl_ctrl.h"
#include "../../pmic_wrap/reg_pmic_wrap.h"
#include <mach/mt_pmic_wrap.h>


/* //////////////////////////////////////////////////////// */
stMhlDev_st stMhlDev;

unsigned int u1CbusTxHwStatus = 0;
unsigned int u1CbusRxHwStatus = 0;
unsigned int u1CbusHwStatus = 0;

st_MscDdcTmrOut_st st_MscTmrOut;
st_MscDdcTmrOut_st st_DdcTmrOut;

unsigned char u1CbusDdcErrCode = 0;
unsigned char u1CbusMscErrCode = 0;
unsigned char u1CbusMscErrCodeDelay2S = 0;
unsigned char u1CbusDdcErrCodeDelay2S = 0;
unsigned char u1CbusMscAbortDelay2S = 0;
unsigned char u1CbusDdcAbortDelay2S = 0;

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

unsigned char u1CbusDdcErrCode;

unsigned int u4CbusIntEnable = 0;
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
	while ((fgReqWaitFinish == FALSE) && fgGetMhlRsen()) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1 / (1000 / HZ));	/* delay 1ms */
		if (u4CbusWaitTimeOut++ > 2000) {
			MHL_CBUS_LOG("req wait time out\n");
			break;
		}
		if (stCbusRequester.fgIsRequesterWait) {
			vMhlTriggerIntTask();
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
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(5 / (1000 / HZ));	/* delay 1ms */
		if (u4CbusWaitTimeOut++ > (5000 / 5)) {
			/* MHL_CBUS_LOG("wakeup time out\n"); */
			break;
		}
	}
}

/* //////////////////////////////////////////////////////// */
void vWriteCbus(unsigned short dAddr, unsigned int dVal)
{
	unsigned int tmp;
	pwrap_wacs2(1, dAddr, (dVal & 0xFFFF), &tmp);
	pwrap_wacs2(1, (dAddr + 2), ((dVal >> 16) & 0xFFFF), &tmp);
}

void vWrite2BCbus(unsigned short dAddr, unsigned int dVal)
{
	unsigned int tmp;
	pwrap_wacs2(1, dAddr, (dVal & 0xFFFF), &tmp);
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
void vSetMHLUSBMode(bool fgUse)
{
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
		/* tdms -> USB, ID -> CBUS */
		vWrite2BCbus(0xC088,
			     (u4Read2BCbus(0xC088) & (~((1 << 9) | (1 << 10)))) | ((1 << 10)));
	} else {
		/* tdms -> MHL, ID -> CBUS */
		vWrite2BCbus(0xC088,
			     (u4Read2BCbus(0xC088) & (~((1 << 9) | (1 << 10)))) | ((1 << 9) |
										   (1 << 10)));
	}
}

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
	if ((u2data == MHL_MSC_MSG_RCP)
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

/* /////////////////////////////////////// */

void vResetCbusMSCState(void)
{
	vClrCbusMSCWaitTmr();
	/* u1CbusMscErrCodeDelay2S = 0; */
	/* u1CbusMscAbortDelay2S = 0; */
	stCbus.stMSCTX.fgTXValid = FALSE;
	stCbus.u2RXBufInd = 0;
	if (stCbus.u1TXBusy != CBUS_TX_DDC_BUSY)
		stCbus.u1TXBusy = CBUS_TX_IDLE;
	stCbus.u2CbusState = MHL_CBUS_STATE_IDLE;
	stCbus.stResp.u2State = MHL_CBUS_STATE_S0;
	stCbus.stReq.u2State = MHL_CBUS_STATE_S0;
}

void vResetCbusDDCState(void)
{				/* xubo */
	vClrCbusDDCWaitTmr();
	/* u1CbusDdcErrCodeDelay2S = 0; */
	/* u1CbusDdcAbortDelay2S = 0; */
	stCbus.stDDCTX.fgTXValid = FALSE;
	if (stCbus.u1TXBusy != CBUS_TX_MSC_BUSY)
		stCbus.u1TXBusy = CBUS_TX_IDLE;
	stCbus.u2CbusDdcState = MHL_CBUS_STATE_IDLE;
	stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S0;
}

static unsigned char u1CbusSendMsg(unsigned short *pMsgData, unsigned char dataSize)
{
	unsigned char i;

	for (i = 0; i < dataSize; i++) {
		vWrite2BCbus(CBUS_WBUF0 + (i << 1), *(pMsgData + i));
		MHL_CBUS_LOG("T:%X,%x\n", *(pMsgData + i), dataSize);
	}
	vWriteCbusMsk(CBUS_LINK_00, (i << LINK_00_TX_NUM), LINK_00_TX_NUM_MASK);
	vCbusTxTrigger();

	return 0;
}

void vCbusSendMscMsg(unsigned short *pMsgData, unsigned char dataSize)
{
	unsigned char i;

	/* stCbus.u1TXBusy = CBUS_TX_MSC_BUSY; */
	stCbus.stMSCTX.fgTXValid = TRUE;
	stCbus.stMSCTX.u2TXRetry = MHL_TX_RETRY_NUM;
	stCbus.stMSCTX.u2TXLen = dataSize;
	for (i = 0; i < dataSize; i++) {
		stCbus.stMSCTX.u2TXBuf[i] = pMsgData[i];
	}
	vMhlTriggerIntTask();
}

void vCbusSendDdcMsg(unsigned short *pMsgData, unsigned char dataSize)
{
	unsigned char i;

	/* stCbus.u1TXBusy = CBUS_TX_DDC_BUSY; */
	stCbus.stDDCTX.fgTXValid = TRUE;
	stCbus.stDDCTX.u2TXRetry = MHL_TX_RETRY_NUM;
	stCbus.stDDCTX.u2TXLen = dataSize;
	for (i = 0; i < dataSize; i++) {
		stCbus.stDDCTX.u2TXBuf[i] = pMsgData[i];
	}
	vMhlTriggerIntTask();
}

static void vCbusMscErrHandling(unsigned char u1ErrorCode)
{
	unsigned short arTxMscMsgs[2];
	vClrCbusMSCWaitTmr();
	/* need to clear erro code after 2S???? */
	u1CbusMscErrCode = u1ErrorCode;
	u1CbusMscErrCodeDelay2S = MHL_MSCDDC_ERR_2S;
	MHL_CBUS_LOG("MSC EC = %x\n", u1CbusMscErrCode);
	arTxMscMsgs[0] = CBUS_MSC_CTRL_ABORT;
	vCbusSendMscMsg(arTxMscMsgs, 1);
}

static void vCbusDdcErrHandling(unsigned char u1ErrorCode)
{
	vClrCbusMSCWaitTmr();
	/* need to clear erro code after 2S???? */
	u1CbusDdcErrCode = u1ErrorCode;
	MHL_CBUS_LOG("DDC EC = %x\n", u1CbusMscErrCode);
	u1CbusDdcErrCodeDelay2S = MHL_MSCDDC_ERR_2S;
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
			MHL_CBUS_LOG("ddc timer out\n");
		}
	}
	if (st_MscTmrOut.u4MscDdcTmr > 0) {
		st_MscTmrOut.u4MscDdcTmr--;
		if (st_MscTmrOut.u4MscDdcTmr == 0) {
			st_MscTmrOut.fgTmrOut = TRUE;
			vMhlTriggerIntTask();
			MHL_CBUS_LOG("msc timer out\n");
		}
	}

	if (u1CbusMscErrCodeDelay2S > 0) {
		u1CbusMscErrCodeDelay2S--;
		if (u1CbusMscErrCodeDelay2S == 0)
			u1CbusMscErrCode = 0;
	}
	if (u1CbusDdcErrCodeDelay2S > 0) {
		u1CbusDdcErrCodeDelay2S--;
		if (u1CbusDdcErrCodeDelay2S == 0)
			u1CbusDdcErrCode = 0;
	}
	/* 6.3.6.5 */
	if (u1CbusMscAbortDelay2S > 0) {
		u1CbusMscAbortDelay2S--;
	}
	if (u1CbusDdcAbortDelay2S > 0) {
		u1CbusDdcAbortDelay2S--;
	}
	/* cbus link timer */
	u2MhlTimerOutCount++;
	u2MhlDcapReadDelay++;
	u2MhlEdidReadDelay++;
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
	case MHL_CBUS_STATE_S0:	/* wait read_devcap/regoffset ok */
		if (u2RxMsg == NONE_PACKET) {
			vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			stCbus.stReq.u2State = MHL_CBUS_STATE_S1;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_S1:	/* wait value */
		if (fgIsMscData(u2RxMsg)) {
			u1DeviceRegSpace[0] = u1GetMscData(u2RxMsg);
			vResetCbusMSCState();
			stCbus.stReq.fgOk = TRUE;
			vMhlUnlockCall();
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg) || fgIsMscACK(u2RxMsg)) {
			vResetCbusMSCState();
			stCbus.stReq.fgOk = FALSE;
			vMhlUnlockCall();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_ABORT:
		vResetCbusMSCState();
		stCbus.stReq.fgOk = FALSE;
		vMhlUnlockCall();
		break;
	default:
		break;
	}

}

bool fgCbusReqGetStateCmd(void)
{
	unsigned short u2TXBuf[5];

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
		if (u2RxMsg == NONE_PACKET) {
			vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			stCbus.stReq.u2State = MHL_CBUS_STATE_S1;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_S2:	/* wait value */
		if (fgIsMscData(u2RxMsg)) {
			u1MhlSinkVendorID = u1GetMscData(u2RxMsg);
			vResetCbusMSCState();
			stCbus.stReq.fgOk = TRUE;
			vMhlUnlockCall();
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg) || fgIsMscACK(u2RxMsg)) {
			vResetCbusMSCState();
			stCbus.stReq.fgOk = FALSE;
			vMhlUnlockCall();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_ABORT:
		vResetCbusMSCState();
		stCbus.stReq.fgOk = FALSE;
		vMhlUnlockCall();
		break;
	default:
		break;
	}

}

bool fgCbusReqGetVendorIDCmd(void)
{
	unsigned short u2TXBuf[5];

	MHL_CBUS_FUNC();

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
	case MHL_CBUS_STATE_S0:	/* wait read_devcap/regoffset ok */
		if (u2RxMsg == NONE_PACKET) {
			vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			stCbus.stReq.u2State = MHL_CBUS_STATE_S1;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
			MHL_CBUS_FUNC();
		}
		break;
	case MHL_CBUS_STATE_S1:	/* wait ack */
		if (fgIsMscACK(u2RxMsg)) {
			vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			stCbus.stReq.u2State = MHL_CBUS_STATE_S2;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
			stCbus.stReq.fgOk = FALSE;
			vMhlUnlockCall();
			MHL_CBUS_FUNC();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			vClrCbusMSCWaitTmr();
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
			MHL_CBUS_FUNC();
		}
		break;
	case MHL_CBUS_STATE_S2:	/* wait value */
		if (fgIsMscData(u2RxMsg)) {
			MHL_CBUS_LOG("id=%X,data=%X\n", u1GetMscData(stCbus.stMSCTX.u2TXBuf[1]),
				     u1GetMscData(u2RxMsg));
			u1DeviceRegSpace[u1GetMscData(stCbus.stMSCTX.u2TXBuf[1])] =
			    u1GetMscData(u2RxMsg);
			vResetCbusMSCState();
			stCbus.stReq.fgOk = TRUE;
			vMhlUnlockCall();
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg) || fgIsMscACK(u2RxMsg)) {
			vResetCbusMSCState();
			stCbus.stReq.fgOk = FALSE;
			vMhlUnlockCall();
			MHL_CBUS_FUNC();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			vClrCbusMSCWaitTmr();
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
			MHL_CBUS_FUNC();
		}
		break;
	case MHL_CBUS_STATE_ABORT:
		vResetCbusMSCState();
		stCbus.stReq.fgOk = FALSE;
		vMhlUnlockCall();
		MHL_CBUS_FUNC();
		break;
	default:
		break;
	}

}

bool fgCbusReqReadDevCapCmd(unsigned char u1Data)
{
	unsigned short u2TXBuf[5];

	MHL_CBUS_FUNC();

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
	case MHL_CBUS_STATE_S0:	/* wait read_devcap/regoffset ok */
		if (u2RxMsg == NONE_PACKET) {
			vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			stCbus.stReq.u2State = MHL_CBUS_STATE_S1;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_S1:	/* wait ack */
		if (fgIsMscACK(u2RxMsg)) {
			vResetCbusMSCState();
			stCbus.stReq.fgOk = TRUE;
			vMhlUnlockCall();
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
			stCbus.stReq.fgOk = FALSE;
			vMhlUnlockCall();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_ABORT:
		vResetCbusMSCState();
		stCbus.stReq.fgOk = FALSE;
		vMhlUnlockCall();
		break;
	default:
		break;
	}

}

bool fgCbusReqWriteStatCmd(unsigned char u1Offset, unsigned char u1Data)
{
	unsigned short u2TXBuf[5];

	MHL_CBUS_FUNC();

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
		if (u2RxMsg == NONE_PACKET) {
			vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			stCbus.stReq.u2State = MHL_CBUS_STATE_S1;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_S1:	/* wait ack */
		if (fgIsMscACK(u2RxMsg)) {
			vResetCbusMSCState();
			stCbus.stReq.fgOk = TRUE;
			vMhlUnlockCall();
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
			stCbus.stReq.fgOk = FALSE;
			vMhlUnlockCall();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_ABORT:
		vResetCbusMSCState();
		stCbus.stReq.fgOk = FALSE;
		vMhlUnlockCall();
		break;
	default:
		break;
	}
}

bool fgCbusReqWriteBrustCmd(unsigned char *ptData, unsigned char u1Len)
{
	unsigned char i;
	unsigned short u2TXBuf[20];

	MHL_CBUS_FUNC();

	if (u1Len > 0x10) {
		printk("ReqWriteBrust Len Err\n");
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
	case MHL_CBUS_STATE_S0:	/* wait read_devcap/regoffset ok */
		if (u2RxMsg == NONE_PACKET) {
			vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			stCbus.stReq.u2State = MHL_CBUS_STATE_S1;
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_S1:	/* wait ack */
		if (fgIsMscACK(u2RxMsg)) {
			vResetCbusMSCState();
			stCbus.stReq.fgOk = TRUE;
			vMhlUnlockCall();
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
			stCbus.stReq.fgOk = FALSE;
			vMhlUnlockCall();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stReq.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_ABORT:
		vResetCbusMSCState();
		stCbus.stReq.fgOk = FALSE;
		vMhlUnlockCall();
		break;
	default:
		break;
	}
}

bool fgCbusReqMscMsgCmd(unsigned char u1Cmd, unsigned char u1Val)
{
	unsigned short u2TXBuf[5];

	MHL_CBUS_FUNC();

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
	/* MHL_CBUS_LOG("DdcRead:%d,%X\n",(stCbus.stDdc.u2State),u2RxMsg); */

	switch (stCbus.stDdc.u2State) {
	case MHL_CBUS_STATE_DDC_S0:	/* sof/dev_w */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S1;
		break;
	case MHL_CBUS_STATE_DDC_S1:	/* wait ack */
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = u2SetDdcData(stCbusDdc.u1Offset);	/* set offset */
			vCbusSendDdcData(u2Data[0]);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S2;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_S2:	/* offset */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S3;
		break;
	case MHL_CBUS_STATE_DDC_S3:	/* wait ack */
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = CBUS_DDC_CTRL_SOF;
			u2Data[1] = stCbusDdc.u1DevAddrR;
			vCbusSendDdcMsg(u2Data, 2);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S4;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_S4:	/* sof / dev_r */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S5;
		break;
	case MHL_CBUS_STATE_DDC_S5:	/* wait ack */
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			vCbusSendDdcCont();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_CONT;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_CONT:	/* cont */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_DATA;
		break;
	case MHL_CBUS_STATE_DDC_DATA:
		if (fgCbusDDCWaitTmrOut()) {
			vClrCbusDDCWaitTmr();
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (fgIdDdcData(u2RxMsg)) {
			stCbusDdc.u1Buf[stCbusDdc.u1Inx] = u1GetDdcData(u2RxMsg);
			vCbusSendDdcCont();
			/* printk("%x:%d\n",stCbusDdc.u1Buf[stCbusDdc.u1Inx],stCbusDdc.u1Inx); */
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_CONT;
			stCbusDdc.u1Len--;
			stCbusDdc.u1Inx++;

			if (stCbusDdc.u1Len == 0) {
				vCbusSendDdcStop();
				stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_STOP;
			}
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_STOP:
		vResetCbusDDCState();
		stCbus.stDdc.fgOk = TRUE;
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_ABORT:
	case MHL_CBUS_STATE_DDC_EOF:
		vResetCbusDDCState();
		stCbus.stDdc.fgOk = FALSE;
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
	case MHL_CBUS_STATE_DDC_S0:	/* sof/0x60_w */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S1;
		break;
	case MHL_CBUS_STATE_DDC_S1:	/* wait ack */
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = u2SetDdcData(stCbusDdc.u1Seg);	/* set seg */
			vCbusSendDdcData(u2Data[0]);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S2;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_S2:	/* seg */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S3;
		break;
	case MHL_CBUS_STATE_DDC_S3:	/* wait ack */
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = CBUS_DDC_CTRL_SOF;	/* sof */
			u2Data[1] = stCbusDdc.u1DevAddrW;	/* dev_w */
			vCbusSendDdcMsg(&u2Data[0], 2);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S4;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_S4:	/* sof / dev_w */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S5;
		break;
	case MHL_CBUS_STATE_DDC_S5:	/* wait ack */
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = u2SetDdcData(stCbusDdc.u1Offset);	/* set offset */
			vCbusSendDdcData(u2Data[0]);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S6;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_S6:	/* offset */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S7;
		break;
	case MHL_CBUS_STATE_DDC_S7:	/* wait ack */
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = CBUS_DDC_CTRL_SOF;
			u2Data[1] = stCbusDdc.u1DevAddrR;	/* sof / dev_r */
			vCbusSendDdcMsg(u2Data, 2);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S8;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_S8:	/* sof / dev_r */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S9;
		break;
	case MHL_CBUS_STATE_DDC_S9:	/* wait ack */
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			vCbusSendDdcCont();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_CONT;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_CONT:	/* cont */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_DATA;
		break;
	case MHL_CBUS_STATE_DDC_DATA:
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (fgIdDdcData(u2RxMsg)) {
			stCbusDdc.u1Buf[stCbusDdc.u1Inx] = u1GetDdcData(u2RxMsg);
			/* printk("ddc:%x:%d\n",stCbusDdc.u1Buf[stCbusDdc.u1Inx],stCbusDdc.u1Inx); */
			vCbusSendDdcCont();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_CONT;
			stCbusDdc.u1Len--;
			stCbusDdc.u1Inx++;

			if (stCbusDdc.u1Len == 0) {
				vCbusSendDdcStop();
				stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_STOP;
			}
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_STOP:
		vResetCbusDDCState();
		stCbus.stDdc.fgOk = TRUE;
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_ABORT:
	case MHL_CBUS_STATE_DDC_EOF:
		vResetCbusDDCState();
		stCbus.stDdc.fgOk = FALSE;
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
	case MHL_CBUS_STATE_DDC_S0:	/* sof/dev_r */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S1;
		break;
	case MHL_CBUS_STATE_DDC_S1:	/* wait ack */
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			vCbusSendDdcCont();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_CONT;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_CONT:	/* cont */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_DATA;
		break;
	case MHL_CBUS_STATE_DDC_DATA:
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (fgIdDdcData(u2RxMsg)) {
			stCbusDdc.u1Buf[stCbusDdc.u1Inx] = u1GetDdcData(u2RxMsg);
			vCbusSendDdcCont();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_CONT;
			stCbusDdc.u1Len--;
			stCbusDdc.u1Inx++;

			if (stCbusDdc.u1Len == 0) {
				vCbusSendDdcStop();
				stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_STOP;
			}
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_STOP:
		vResetCbusDDCState();
		stCbus.stDdc.fgOk = TRUE;
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_ABORT:
	case MHL_CBUS_STATE_DDC_EOF:
		vResetCbusDDCState();
		stCbus.stDdc.fgOk = FALSE;
		vMhlUnlockCall();
		break;
	default:
		break;
	}
}

void vCbusReqDdcWriteState(unsigned short u2RxMsg)
{
	unsigned short u2Data[16];
	/* MHL_CBUS_LOG("Write:%d,%X\n",(stCbus.stDdc.u2State),u2RxMsg); */

	switch (stCbus.stDdc.u2State) {
	case MHL_CBUS_STATE_DDC_S0:	/* sof/dev_w */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S1;
		break;
	case MHL_CBUS_STATE_DDC_S1:	/* wait ack */
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			u2Data[0] = u2SetDdcData(stCbusDdc.u1Offset);	/* set offset */
			vCbusSendDdcData(u2Data[0]);
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S2;
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_S2:	/* offset */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S3;
		break;
		break;
	case MHL_CBUS_STATE_DDC_S3:	/* wait ack */
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		} else if (u2RxMsg == CBUS_DDC_CTRL_NACK) {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcEof();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_EOF;
		} else if (u2RxMsg == CBUS_DDC_CTRL_ACK) {
			if (stCbusDdc.u1Len == 0) {
				vCbusSendDdcStop();
				stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_STOP;
			} else {
				u2Data[0] = u2SetDdcData(stCbusDdc.u1Buf[stCbusDdc.u1Inx]);
				/* MHL_CBUS_LOG("DDCW:%X/%X/%X\n",stCbusDdc.u1Buf[stCbusDdc.u1Inx] ,stCbusDdc.u1Inx,stCbusDdc.u1Len); */
				vCbusSendDdcMsg(u2Data, 1);
				stCbusDdc.u1Inx++;
				stCbusDdc.u1Len--;
				stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S4;
			}
		} else {
			vCbusDdcErrHandling(CBUS_DDC_INCOMPLETE_ERR);
			vCbusSendDdcAbort();
			stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_ABORT;
		}
		vClrCbusDDCWaitTmr();
		break;
	case MHL_CBUS_STATE_DDC_S4:	/* data */
		vSetCbusDDCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S3;
		break;
	case MHL_CBUS_STATE_DDC_STOP:
		vResetCbusDDCState();
		stCbus.stDdc.fgOk = TRUE;
		vMhlUnlockCall();
		break;
	case MHL_CBUS_STATE_DDC_ABORT:
	case MHL_CBUS_STATE_DDC_EOF:
		vResetCbusDDCState();
		stCbus.stDdc.fgOk = FALSE;
		vMhlUnlockCall();
		break;
	default:
		break;
	}
}

bool fgCbusReqDdcReadCmd(bool fgEdid, unsigned char u1Seg, unsigned char u1Offset,
			 unsigned char u1Len, unsigned char *ptData)
{
	unsigned short u2Data[10];
	unsigned char i;

	MHL_CBUS_FUNC();

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

	MHL_CBUS_LOG("result:%d\n", stCbus.stDdc.fgOk);
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
	unsigned short u2Data[10];

	MHL_CBUS_FUNC();

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
		stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		break;
	case MHL_CBUS_STATE_ABORT:
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
		stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		break;
	case MHL_CBUS_STATE_ABORT:
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
		stMhlDev.fgIsEdidChg = TRUE;
		u2MhlEdidReadDelay = 0;
		vCbusRespAck();
		stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		break;
	case MHL_CBUS_STATE_ABORT:
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
		stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		break;
	case MHL_CBUS_STATE_ABORT:
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
		stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		break;
	case MHL_CBUS_STATE_ABORT:
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
		stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		break;
	case MHL_CBUS_STATE_ABORT:
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
				stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
			} else {
				stCbus.u2RXBuf[1] = (u2RxMsg & 0xff);
				u2Data[0] = CBUS_MSC_CTRL_ACK;
				u2Data[1] =
				    u2SetMscData(u1MyDeviceRegSpace
						 [u1GetMscData(stCbus.u2RXBuf[1])]);
				vCbusSendMscMsg(u2Data, 2);
				stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
			}
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_ABORT:
		vResetCbusMSCState();
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
		stCbus.stResp.u2State = MHL_CBUS_STATE_S1;
		vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
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
				stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
			}
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
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
					stMhlDev.fgIsDCapChg = TRUE;
					u2MhlDcapReadDelay = 0;
					MHL_CBUS_LOG("fgIsDCapChg\n");
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
					stMhlDev.fgIsEdidChg = TRUE;
					u2MhlEdidReadDelay = 0;
					MHL_CBUS_LOG("fgIsEdidChg\n");
				}
				u1DeviceRegSpace[u1Data1] = 0;
			} else if (u1Data1 == CBUS_MSC_STATUS_CONNECTED_RDY) {
				if (u1Data & CBUS_MSC_STATUS_CONNECTED_RDY_DCAP_RDY) {
					stMhlDev.fgIsDCapRdy = TRUE;
					stMhlDev.fgIsDCapChg = TRUE;
					u2MhlDcapReadDelay = 0;
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
			stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_ABORT:
		vResetCbusMSCState();
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
		stCbus.stResp.u2State = MHL_CBUS_STATE_S1;
		vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		break;
	case MHL_CBUS_STATE_S1:	/* offset */
		if ((fgIsMscData(u2RxMsg))
		    || ((u2RxMsg & 0xff) >= (u1MyDeviceRegSpace[CBUS_SCRATCHPAD_SIZE] + 0x40))) {
			vCbusMscErrHandling(MHL_MSC_BAD_OFFSET);
			stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		} else if (fgIsMscData(u2RxMsg)) {
			stCbus.u2RXBuf[1] = (u2RxMsg & 0xff);
			stCbus.u2RXBufInd = 0;
			vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			stCbus.stResp.u2State = MHL_CBUS_STATE_S2;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_S2:	/* data */
		if (u2RxMsg == CBUS_MSC_CTRL_EOF)	/* end */
		{
			if ((stCbus.u2RXBufInd <= 2)
			    || (stCbus.u2RXBufInd >= ((u1GetMscData(stCbus.u2RXBuf[1]) - 0x40) + u1MyDeviceRegSpace[CBUS_SCRATCHPAD_SIZE])))	/* least 2 data,max 16byte */
			{
				vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
				stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
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
				stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
			}
		} else if (fgIsMscData(u2RxMsg)) {
			if ((stCbus.u2RXBufInd + stCbus.u2RXBuf[1]) >=
			    (u1MyDeviceRegSpace[CBUS_SCRATCHPAD_SIZE] + 0x40)) {
				stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
				vCbusMscErrHandling(MHL_MSC_BAD_OFFSET);
			} else {
				stCbus.u2RXBufInd++;
				stCbus.u2RXBuf[stCbus.u2RXBufInd] = (u2RxMsg & 0xff);
				vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
			}
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_ABORT:
		vResetCbusMSCState();
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
		stCbus.stResp.u2State = MHL_CBUS_STATE_S1;
		vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
		break;
	case MHL_CBUS_STATE_S1:	/* get sub cmd */
		if (fgIsMscMsg(u2RxMsg)) {
			if (fgIsMscMsg(u2RxMsg)) {
				stCbusMscMsg.u2Code = u2RxMsg;
				vSetCbusMSCWaitTmr(MHL_MSCDDC_TMR_OUT_200MS);
				stCbus.stResp.u2State = MHL_CBUS_STATE_S2;
			} else {
				vCbusMscErrHandling(MHL_MSC_BAD_OPCODE);
				stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
			}
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_S2:	/* get data */
		if (fgIsMscData(u2RxMsg)) {
			stCbusMscMsg.u1Val = u1GetMscData(u2RxMsg);
			stCbusMscMsg.fgIsValid = TRUE;
			vCbusRespAck();
			stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		} else if (fgIsMscNACK(u2RxMsg) || fgIsMscAbort(u2RxMsg)) {
			vResetCbusMSCState();
		} else {
			vCbusMscErrHandling(MHL_MSC_PROTOCOL_ERR);
			stCbus.stResp.u2State = MHL_CBUS_STATE_ABORT;
		}
		break;
	case MHL_CBUS_STATE_ABORT:
		vResetCbusMSCState();
		break;
	default:
		break;
	}
}

/* /////////////////////////////////////////////////////////// */
void vCbusInit(void)
{
	/* for cbus pads */
	/* vWrite2BCbus(0x014C, 0xF001); */
	/* vWrite2BCbus(0xC0E8, 0x00C0); */
	/* vWrite2BCbus(0xC100, 0x0001); */
	/* vWrite2BCbus(0xC0F8, 0x1249); */

	/* config gpio27 to CBUS mode */
	/* gpio27 mode : 0xC0E8[8,6], 3: cbus */
	vWrite2BCbus(0xC0E8, (u4Read2BCbus(0xC0E8) & (~(0x07 << 6))) | (3 << 6));

	vSetMHLUSBMode(TRUE);

	/* clk to HPD for debug clock */
	/* vWrite2BCbus(0x013A, 0x0B0A); */
	/* vWrite2BCbus(0xC0F8, 0x7009); */

	/* Cal 12mHZ clock */
	vWrite2BCbus(0x039E, 0x0041);
	vWrite2BCbus(0x039E, 0x0040);
	vWrite2BCbus(0x039E, 0x0050);

	vWriteCbus(CBUS_LINK_00, 0x17813200);
	vWriteCbus(CBUS_LINK_01, 0x03028794);
	vWriteCbus(CBUS_LINK_02, 0x5FC58A0A);
	vWriteCbus(CBUS_LINK_03, 0x04A4888A);
	vWriteCbus(CBUS_LINK_04, 0x40030964);
	vWriteCbus(CBUS_LINK_05, 0xC85A5A1E);
	vWriteCbus(CBUS_LINK_06, 0x1496965A);
	vWriteCbus(CBUS_LINK_07, 0xC0580130);
	vWriteCbus(CBUS_LINK_08, 0x00000000);
	vWriteCbus(CBUS_LINK_09, 0x0180080C);
	vWriteCbus(CBUS_LINK_0A, 0xFFFF000F);
	vWriteCbus(CBUS_LINK_0B, 0x00411600);
	vWriteCbus(CBUS_LINK_0C, 0xC0060A21);
	vWriteCbus(CBUS_LINK_0D, 0x00000000);

	/* for rx read val error */
	vWriteCbusMsk(CBUS_LINK_02, (8 << LINK_02_LINK_HALFTRAN_MIN),
		      (0x7F << LINK_02_LINK_HALFTRAN_MIN));
	vWriteCbusMsk(CBUS_LINK_02, (18 << LINK_02_LINK_HALFTRAN_MAX),
		      (0x7F << LINK_02_LINK_HALFTRAN_MAX));
	vWriteCbusMsk(CBUS_LINK_04, (18 << LINK_04_LINK_RXDECISION),
		      (0x7F << LINK_04_LINK_RXDECISION));

	/* for no ack reply */
	vWriteCbusMsk(CBUS_LINK_02, LINK_02_LINK_ACK_MANU_EN_MASK, LINK_02_LINK_ACK_MANU_EN_MASK);
	vWriteCbusMsk(CBUS_LINK_04, (1 << LINK_04_LINK_ACK_WIDTH),
		      (0x7F << LINK_04_LINK_ACK_WIDTH));
	vWriteCbusMsk(CBUS_LINK_0C, (13 << LINK_0C_LINK_ACK_WIDTH_UPPER),
		      (0x7F << LINK_0C_LINK_ACK_WIDTH_UPPER));

	/* for tx read ack error */
	vWriteCbusMsk(CBUS_LINK_01, (5 << LINK_01_CBUS_ACK_0_MIN),
		      (0x7F << LINK_01_CBUS_ACK_0_MIN));
	vWriteCbusMsk(CBUS_LINK_01, (22 << LINK_01_CBUS_ACK_0_MAX),
		      (0x7F << LINK_01_CBUS_ACK_0_MAX));
	vWriteCbusMsk(CBUS_LINK_01, (40 << LINK_01_CBUS_ACK_FALL_MAX),
		      (0x7F << LINK_01_CBUS_ACK_FALL_MAX));

	/* vCbusRxEnable(); */

	vResetCbusDDCState();
	vResetCbusMSCState();

	MHL_CBUS_LOG("0x03A0:%X\n", u4Read2BCbus(0x03A0));

}

void vCbusReset(void)
{
	/* clear tx */
	vResetCbusDDCState();
	vResetCbusMSCState();
	stCbusRequester.fgIsRequesterWait = FALSE;
	vWriteCbusMsk(CBUS_LINK_00, 0, LINK_00_TX_NUM_MASK);
	vClrCbusBit(CBUS_LINK_00, LINK_00_TX_TRIG_MASK);
	vCbusRxDisable();

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
	u4CbusIntEnable = (LINK_08_LINKRX_TIMEOUT_INT_CLR_MASK
			   | LINK_08_RBUF_TRIG_INT_CLR_MASK
			   | LINK_08_ZCBUS_DET_DONE_INT_CLR_MASK
			   | LINK_08_TX_ARB_FAIL_INT_CLR_MASK
			   | LINK_08_TX_OK_INT_CLR_MASK
			   | LINK_08_TX_RETRY_TO_INT_CLR_MASK
			   | LINK_08_DISC_DET_FAIL_INT_CLR_MASK | LINK_08_DISC_DET_INT_CLR_MASK);
	u4CbusIntEnable = u4CbusIntEnable << 16;
	vWriteCbus(CBUS_LINK_08, u4CbusIntEnable);
}

void vCbusRxEnable(void)
{
	vSetCbusBit(CBUS_LINK_02, LINK_02_LINKRX_EN_MASK);
}

void vCbusRxDisable(void)
{
	vClrCbusBit(CBUS_LINK_02, LINK_02_LINKRX_EN_MASK);
}

void vCbusTxTrigger(void)
{
	vClrCbusBit(CBUS_LINK_00, LINK_00_TX_TRIG_MASK);
	vSetCbusBit(CBUS_LINK_00, LINK_00_TX_TRIG_MASK);
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
	return (u1CbusHwStatus & LINK_STA_00_ZCBUS_DET_1K_INT_MASK);
}

bool fgCbusCheck1KDone(void)
{
	return (u1CbusHwStatus &
		(LINK_STA_00_ZCBUS_DET_1K_INT_MASK | LINK_STA_00_ZCBUS_DET_DONE_INT_MASK));
}

bool fgCbusDiscovery(void)
{
	return (u1CbusHwStatus & LINK_STA_00_DISC_DET_INT_MASK);
}

bool fgCbusDiscoveryDone(void)
{
	return (u1CbusHwStatus &
		(LINK_STA_00_DISC_DET_INT_MASK | LINK_STA_00_DISC_DET_FAIL_INT_MASK));
}

/* /////////////////////////////////////////////////////////// */
/* / */
/* /             cbus cmd process */
/* / */
/* /////////////////////////////////////////////////////////// */
void vCbusMSCState(unsigned short u2RxMsg)
{
	/* MHL_CBUS_LOG("RxMsg=%x\n",u2RxMsg); */

	if (fgIsMscAbort(u2RxMsg))
		u1CbusMscAbortDelay2S = MHL_MSCDDC_ERR_2S;

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

	if (fgIsDdcAbort(u2RxMsg))
		u1CbusDdcAbortDelay2S = MHL_MSCDDC_ERR_2S;

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

unsigned int u4loopcounter = 0;
unsigned int u4IntStatBak = 0;
unsigned int u4CbusRxCount = 0;
/* /////////////////////////////////////////////// */
void vMhlIntProcess(void)
{
	unsigned int u4IntStat;
	unsigned short u2RxMsg = 0;
	unsigned char i = 0;
	unsigned char j = 0;
	unsigned short aRxMsg[16];
	unsigned short u2HwTxBuf[16];

	u4loopcounter++;

	u4CbusRxCount = 0;
/****************************************************
	clear hw interrupt
****************************************************/
	u4IntStat = u4ReadCbus(CBUS_LINK_STA_00);
	vWriteCbusMsk(CBUS_LINK_08, u4IntStat, 0xFFFF);
	vWriteCbusMsk(CBUS_LINK_08, 0, 0xFFFF);
	u4IntStat = u4IntStat & 0xFFFFF;

/****************************************************
	check wake up and discovery
****************************************************/
	if (u4IntStat & (LINK_STA_00_ZCBUS_DET_DONE_INT_MASK
			 | LINK_STA_00_DISC_DET_INT_MASK | LINK_STA_00_DISC_DET_FAIL_INT_MASK)) {
		u1CbusHwStatus = 0;
	}
/*
	if(u4IntStat  & (1 << 8))
	{
		MHL_CBUS_LOG("nack\n");
	}
*/

	if (u4IntStat & (LINK_STA_00_ZCBUS_DET_DONE_INT_MASK | LINK_STA_00_ZCBUS_DET_1K_INT_MASK)) {
		u1CbusHwStatus =
		    u4IntStat & (LINK_STA_00_ZCBUS_DET_DONE_INT_MASK |
				 LINK_STA_00_ZCBUS_DET_1K_INT_MASK);
		vCbusWakeupEvent();
	}
	if (u4IntStat & (LINK_STA_00_DISC_DET_INT_MASK | LINK_STA_00_DISC_DET_FAIL_INT_MASK)) {
		u1CbusHwStatus =
		    u4IntStat & (LINK_STA_00_DISC_DET_INT_MASK |
				 LINK_STA_00_DISC_DET_FAIL_INT_MASK);
		vCbusWakeupEvent();
	}
/****************************************************
	check hw Rx/Tx status
****************************************************/
	u1CbusTxHwStatus = 0;
	u1CbusRxHwStatus = 0;

	if (u4IntStat & LINK_08_LINKRX_TIMEOUT_INT_CLR_MASK) {
		u1CbusRxHwStatus |= LINK_08_LINKRX_TIMEOUT_INT_CLR_MASK;
		MHL_CBUS_LOG(">RTO\n");
	}
	if ((u4IntStat & LINK_08_RBUF_TRIG_INT_CLR_MASK))	/* bit 12 */
	{
		u1CbusRxHwStatus |= LINK_08_RBUF_TRIG_INT_CLR_MASK;

		u4CbusRxCount =
		    u4ReadCbusFld(CBUS_LINK_STA_00, LINK_STA_00_RBUF_LVL_LAT,
				  LINK_STA_00_RBUF_LVL_LAT_MASK);
		j = u4CbusRxCount;
		while (j > 0) {
			j--;
			u2RxMsg = u4Read2BCbus(CBUS_RBUF) & 0x7FF;
			aRxMsg[i] = u2RxMsg;
			i++;
			MHL_CBUS_LOG("R:%X,%d\n", u2RxMsg, i);
		}
	}
	if (u4IntStat & LINK_08_TX_RETRY_TO_INT_CLR_MASK)	/* 6 */
	{
		u1CbusTxHwStatus |= LINK_08_TX_RETRY_TO_INT_CLR_MASK;
		MHL_CBUS_LOG(">TRTO\n");
	}
	if (u4IntStat & LINK_08_TX_OK_INT_CLR_MASK)	/* bit */
	{
		u1CbusTxHwStatus |= LINK_08_TX_OK_INT_CLR_MASK;
	}
	if (u4IntStat & LINK_08_TX_ARB_FAIL_INT_CLR_MASK) {
		u1CbusTxHwStatus |= LINK_08_TX_ARB_FAIL_INT_CLR_MASK;
		MHL_CBUS_LOG(">TAL\n");
	}

	if ((fgIsCbusConnected() == FALSE) || (fgGetMhlRsen() == FALSE))
		return;
/****************************************************
	read rx duffer data
****************************************************/
	/* hw error process */
	if (fgCbusTxErr()) {
		u4CbusRxCount = 0;
		/* retry */
		if ((stCbus.u1TXBusy == CBUS_TX_MSC_BUSY) && (stCbus.stMSCTX.u2TXRetry > 0)) {
			stCbus.stMSCTX.u2TXRetry--;
			vCbusTxTrigger();	/* fail,trigger again */
		} else if ((stCbus.u1TXBusy == CBUS_TX_DDC_BUSY) && (stCbus.stDDCTX.u2TXRetry > 0)) {
			stCbus.stDDCTX.u2TXRetry--;
			vCbusTxTrigger();	/* fail,trigger again */
		} else {
			vResetCbusMSCState();
			vResetCbusDDCState();
			CbusTxRxReset();
			stCbus.stDdc.fgOk = FALSE;
			stCbus.stReq.fgOk = FALSE;
			vMhlUnlockCall();
		}
	}
	if (fgCbusRxErr()) {
		u4CbusRxCount = 0;
		vResetCbusMSCState();
		vResetCbusDDCState();
		CbusTxRxReset();
		stCbus.stDdc.fgOk = FALSE;
		stCbus.stReq.fgOk = FALSE;
		vMhlUnlockCall();
	}

	if (fgCbusMSCWaitTmrOut() || fgCbusDDCWaitTmrOut()) {
		MHL_CBUS_LOG("timer out\n");
		if (fgCbusMSCWaitTmrOut()) {
			u1CbusMscErrCode = MHL_MSC_MSG_TIMEOUT;
			u1CbusMscErrCodeDelay2S = MHL_MSCDDC_ERR_2S;
			vResetCbusMSCState();
			stCbus.stReq.fgOk = FALSE;
			vMhlUnlockCall();
		}
		/* ddc cmd must process timer out */
		if (fgCbusDDCWaitTmrOut()) {
			vCbusDDCState(NONE_PACKET);
		}
	} else {
		if (fgCbusTxOk()) {
			if (stCbus.u1TXBusy == CBUS_TX_MSC_BUSY)
				vCbusMSCState(NONE_PACKET);
			else if (stCbus.u1TXBusy == CBUS_TX_DDC_BUSY)
				vCbusDDCState(NONE_PACKET);
			stCbus.u1TXBusy = CBUS_TX_IDLE;
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
					MHL_CBUS_LOG("not msc/ddc\n");
			}
		}
	}
/* ////////////////////////////////////////////////////////////////// */
	if (fgCbusMscIdle()
	    && fgCbusDdcIdle()
	    && (stCbus.u1TXBusy == CBUS_TX_IDLE)
	    && (stCbus.stMSCTX.fgTXValid == FALSE)
	    && (stCbus.stDDCTX.fgTXValid == FALSE)
	    && (u4CbusRxCount == 0)) {
		/* at the same time, only ddc or msc requester */
		/* req new msc/ddc cmd */
		if (stCbusRequester.fgIsRequesterWait)	/* get cbus idle */
		{
			stCbusRequester.fgIsRequesterWait = FALSE;
			if (stCbusRequester.fgIsDdc == FALSE) {
				MHL_CBUS_LOG("ReqMSCNewCmd:%x\n", stCbusRequester.u2ReqBuf[0]);
				stCbus.u2CbusState = MHL_CBUS_STATE_REQUESTER;
				stCbus.stReq.u2Cmd = stCbusRequester.u2ReqBuf[0];
				stCbus.stReq.u2State = MHL_CBUS_STATE_S0;
				stCbus.stReq.fgOk = FALSE;
				stCbus.stMSCTX.fgTXValid = FALSE;
				stCbus.u1TXBusy = CBUS_TX_MSC_BUSY;
				stCbus.stMSCTX.u2TXRetry = MHL_TX_RETRY_NUM;
				for (i = 0; i < stCbusRequester.u4Len; i++) {
					stCbus.stMSCTX.u2TXBuf[i] = stCbusRequester.u2ReqBuf[i];
				}
				vClrCbusMSCWaitTmr();
				u1CbusSendMsg(stCbusRequester.u2ReqBuf, stCbusRequester.u4Len);
			} else {
				MHL_CBUS_LOG("ReqDDCNewCmd:%x\n", stCbus.stDdc.u2Cmd);
				stCbus.u2CbusDdcState = MHL_CBUS_STATE_REQUESTER;
				stCbus.stDdc.u2State = MHL_CBUS_STATE_DDC_S0;
				stCbus.stDdc.fgOk = FALSE;
				stCbus.stDDCTX.fgTXValid = FALSE;
				stCbus.u1TXBusy = CBUS_TX_DDC_BUSY;
				stCbus.stDDCTX.u2TXRetry = MHL_TX_RETRY_NUM;
				for (i = 0; i < stCbusRequester.u4Len; i++) {
					stCbus.stDDCTX.u2TXBuf[i] = stCbusRequester.u2ReqBuf[i];
				}
				vClrCbusDDCWaitTmr();
				u1CbusSendMsg(stCbusRequester.u2ReqBuf, stCbusRequester.u4Len);
			}
		}
	}
/* ////////////////////////////////////////////////////////////////// */
/* TX process */
	if (stCbus.u1TXBusy == CBUS_TX_IDLE) {
		if (stCbus.stMSCTX.fgTXValid) {
			stCbus.stMSCTX.fgTXValid = FALSE;
			stCbus.u1TXBusy = CBUS_TX_MSC_BUSY;
			stCbus.stMSCTX.u2TXRetry = MHL_TX_RETRY_NUM;
			vClrCbusMSCWaitTmr();
			u1CbusSendMsg(stCbus.stMSCTX.u2TXBuf, stCbus.stMSCTX.u2TXLen);
			/* MHL_CBUS_LOG("msc hw tx send\n"); */
		} else if (stCbus.stDDCTX.fgTXValid) {
			stCbus.stDDCTX.fgTXValid = FALSE;
			stCbus.u1TXBusy = CBUS_TX_DDC_BUSY;
			stCbus.stDDCTX.u2TXRetry = MHL_TX_RETRY_NUM;
			vClrCbusDDCWaitTmr();
			u1CbusSendMsg(stCbus.stDDCTX.u2TXBuf, stCbus.stDDCTX.u2TXLen);
			/* MHL_CBUS_LOG("ddc hw tx send\n"); */
		}
	}
/* //////////////////////////////////////////////////////// */
}

void vDcapParser(void)
{
	unsigned char i;
	printk("Sink device cap:\n");
	for (i = 0; i < 0x10; i++)
		if (u1DeviceRegSpace[i] > 0x0F)
			printk(" %2X", u1DeviceRegSpace[i]);
		else
			printk(" 0%X", u1DeviceRegSpace[i]);
	printk("\n");
	/* Check POW bit from Sink */
	if (u1DeviceRegSpace[CBUS_MSC_DEV_CAT] & CBUS_MSC_DEV_CAT_POW) {
		MHL_CBUS_LOG("sink support pow\n");
	} else {
		MHL_CBUS_LOG("sink not support pow\n");
	}
	switch (u1DeviceRegSpace[CBUS_MSC_DEV_CAT] & CBUS_MSC_DEV_CAT_DEV_TYPE) {
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
	if (u1DeviceRegSpace[CBUS_VID_LINK_MODE] & CBUS_SUPP_RGB444)
		MHL_CBUS_LOG("CBUS_SUPP_RGB444\n");
	if (u1DeviceRegSpace[CBUS_VID_LINK_MODE] & CBUS_SUPP_YCBCR444)
		MHL_CBUS_LOG("CBUS_SUPP_YCBCR444\n");
	if (u1DeviceRegSpace[CBUS_VID_LINK_MODE] & CBUS_SUPP_YCBCR422)
		MHL_CBUS_LOG("CBUS_SUPP_YCBCR422\n");
	if (u1DeviceRegSpace[CBUS_VID_LINK_MODE] & CBUS_SUPP_PP)
		MHL_CBUS_LOG("CBUS_SUPP_PP\n");
	if (u1DeviceRegSpace[CBUS_VID_LINK_MODE] & CBUS_SUPP_ISLANDS)
		MHL_CBUS_LOG("CBUS_SUPP_ISLANDS\n");
	if (u1DeviceRegSpace[CBUS_VID_LINK_MODE] & CBUS_SUPP_VGA)
		MHL_CBUS_LOG("CBUS_SUPP_VGA\n");
	/* aud support */
	if (u1DeviceRegSpace[CBUS_AUD_LINK_MODE] & CBUS_SUPP_AUD_2CH)
		MHL_CBUS_LOG("CBUS_SUPP_AUD_2CH\n");
	if (u1DeviceRegSpace[CBUS_AUD_LINK_MODE] & CBUS_SUPP_AUD_8CH)
		MHL_CBUS_LOG("CBUS_SUPP_AUD_8CH\n");
	/* CBUS_BANDWIDTH */
	MHL_CBUS_LOG("MHL bandwidth = %d\n", u1DeviceRegSpace[CBUS_BANDWIDTH] * 5);
	/* Check Scratchpad/RAP/RCP support */
	if (u1DeviceRegSpace[CBUS_FEATURE_FLAG] & CBUS_FEATURE_FLAG_RCP_SUPPORT) {
		MHL_CBUS_LOG("sink support RCP_SUPPORT\n");
	}
	if (u1DeviceRegSpace[CBUS_FEATURE_FLAG] & CBUS_FEATURE_FLAG_RAP_SUPPORT) {
		MHL_CBUS_LOG("sink support RAP_SUPPORT\n");
	}
	if (u1DeviceRegSpace[CBUS_FEATURE_FLAG] & CBUS_FEATURE_FLAG_SP_SUPPORT) {
		MHL_CBUS_LOG("sink support SP_SUPPORT\n");
	}
	MHL_CBUS_LOG("CBUS_SCRATCHPAD_SIZE  = %d\n", u1DeviceRegSpace[CBUS_SCRATCHPAD_SIZE]);
	MHL_CBUS_LOG("CBUS_INT_SIZE  = %d\n", u1DeviceRegSpace[CBUS_INT_STAT_SIZE] & 0x0F);
	MHL_CBUS_LOG("CBUS_STAT_SIZE  = %d\n", u1DeviceRegSpace[CBUS_INT_STAT_SIZE] >> 4);
	MHL_CBUS_LOG("===== end dcap parser ========\n");

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
	if ((stCbus.u2CbusState == MHL_CBUS_STATE_IDLE) && (u1CbusMscAbortDelay2S == 0))
		return TRUE;
	else
		return FALSE;
}

bool fgCbusDdcIdle(void)
{
	if ((stCbus.u2CbusDdcState == MHL_CBUS_STATE_IDLE) && (u1CbusDdcAbortDelay2S == 0))
		return TRUE;
	else
		return FALSE;
}

void vCbusCmdStatus(void)
{
	unsigned int i, j;

	printk("=========================================\n");
	printk("Int loop live :%x\n", u4loopcounter);
	printk(">>> cbus cmd status\n");
	printk("u2CbusState=%d\n", stCbus.u2CbusState);
	printk("stReq.u1Cmd=%d\n", stCbus.stReq.u2Cmd);
	printk("stReq.u2State=%d\n", stCbus.stReq.u2State);
	printk("stReq.fgOk=%d\n", stCbus.stReq.fgOk);
	printk("stResp.u2State=%d\n", stCbus.stResp.u2State);
	printk("fgCbusMscIdle()=%d\n", fgCbusMscIdle());
	printk("fgCbusDdcIdle()=%d\n", fgCbusDdcIdle());
	printk("stCbus.u1TXBusy=%d\n", stCbus.u1TXBusy);
	printk("u4CbusRxCount=%d\n", u4CbusRxCount);

	printk(">>> cbus sink device reg space\n");
	for (i = 0; i < MHL_CBUS_DEVICE_LENGTH; i = i + 0x10) {
		if (i > 0x0F)
			printk("%2X : ", i);
		else
			printk("0%X : ", i);
		for (j = 0; j < 0x10; j++)
			if (u1DeviceRegSpace[i + j] > 0x0F)
				printk("%2X ", u1DeviceRegSpace[i + j]);
			else
				printk("0%X ", u1DeviceRegSpace[i + j]);
		printk("\n");

	}
	printk(">>> cbus my device reg space\n");
	for (i = 0; i < MHL_CBUS_DEVICE_LENGTH; i = i + 0x10) {
		if (i > 0x0F)
			printk("%2X : ", i);
		else
			printk("0%X : ", i);
		for (j = 0; j < 0x10; j++)
			if (u1MyDeviceRegSpace[i + j] > 0x0F)
				printk("%2X ", u1MyDeviceRegSpace[i + j]);
			else
				printk("0%X ", u1MyDeviceRegSpace[i + j]);
		printk("\n");
	}
	printk(">>> 1 block edid data\n");
	for (i = 0; i < 128; i = i + 0x10) {
		if (i > 0x0F)
			printk("%2X : ", i);
		else
			printk("%X : ", i);
		for (j = 0; j < 0x10; j++)
			if (stCbusDdc.u1Buf[i + j] > 0x0F)
				printk("%2X ", stCbusDdc.u1Buf[i + j]);
			else
				printk("0%X ", stCbusDdc.u1Buf[i + j]);
		printk("\n");
	}
	for (i = 0; i < 128; i++) {
		stCbusDdc.u1Buf[i] = 0;
	}
	vDcapParser();
}

void v6397DumpReg(void)
{
	unsigned int addr, u4data;

	printk("\n");
	printk("0x013A:%08X\n", u4Read2BCbus(0x013A));
	printk("0x014C:%08X\n", u4Read2BCbus(0x014C));
	printk("0xC008:%08X\n", u4Read2BCbus(0xC008));
	printk("0xC088:%08X\n", u4Read2BCbus(0xC088));
	printk("0xC0E8:%08X\n", u4Read2BCbus(0xC0E8));
	printk("0xC0F8:%08X\n", u4Read2BCbus(0xC0F8));
	printk("0xC100:%08X\n", u4Read2BCbus(0xC100));

	for (addr = 0xA00; addr < 0xA80; addr = addr + 4) {
		if ((addr & 0x0F) == 0) {
			printk("\n");
			printk("%08X : ", addr);
		}
		u4data = u4ReadCbus(addr);
		printk("%08X ", u4data);
	}

	printk("\n");
}
