#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT
#ifndef _MHL_CBUS_H_
#define _MHL_CBUS_H_
#include "inter_mhl_drv.h"
/* ////////////////////////////////////////////// */
/* / */
/* /     device register space define */
/* / */
/* ////////////////////////////////////////////// */
#define CBUS_MHL_VERSION							0x01
#define CBUS_MSC_DEV_CAT							0x02
#define	CBUS_MSC_DEV_CAT_POW					0x10
#define	CBUS_MSC_DEV_CAT_DEV_TYPE				0x0F

#define CBUS_ADOPTER_ID_H				0x03
#define CBUS_ADOPTER_ID_L				0x04
#define CBUS_VID_LINK_MODE				0x05
#define CBUS_SUPP_RGB444			0x01
#define CBUS_SUPP_YCBCR444		0x02
#define CBUS_SUPP_YCBCR422		0x04
#define CBUS_SUPP_PP				0x08
#define CBUS_SUPP_ISLANDS			0x10
#define CBUS_SUPP_VGA				0x20
#define CBUS_AUD_LINK_MODE			0x06
#define CBUS_SUPP_AUD_2CH			0x01
#define CBUS_SUPP_AUD_8CH			0x02
#define CBUS_VIDEO_TYPE				0x07
#define VT_GRAPHICS					0x01
#define VT_PHOTO					0x02
#define VT_CINEMA					0x04
#define VT_GAME						0x08
#define VT_SUPP						0x80
#define CBUS_LOG_DEV_MAP									0x08
#define	CBUS_LOG_DEV_MAP_LD_DISPLAY					0x01
#define	CBUS_LOG_DEV_MAP_LD_VIDEO						0x02
#define	CBUS_LOG_DEV_MAP_LD_AUDIO						0x04
#define	CBUS_LOG_DEV_MAP_LD_MEDIA						0x08
#define	CBUS_LOG_DEV_MAP_LD_TUNER						0x10
#define	CBUS_LOG_DEV_MAP_LD_RECORD					0x20
#define	CBUS_LOG_DEV_MAP_LD_SPEAKER					0x40
#define	CBUS_LOG_DEV_MAP_LD_GUI							0x80
#define CBUS_BANDWIDTH		0x09
#define CBUS_FEATURE_FLAG										0x0A
#define	CBUS_FEATURE_FLAG_RCP_SUPPORT						0x01
#define	CBUS_FEATURE_FLAG_RAP_SUPPORT						0x02
#define	CBUS_FEATURE_FLAG_SP_SUPPORT						0x04
#define	CBUS_FEATURE_UCP_SEND_SUPPORT						0x08
#define	CBUS_FEATURE_UCP_RECV_SUPPORT						0x10

#define CBUS_DEVICE_ID_H					0x0B
#define CBUS_DEVICE_ID_L					0x0C

#define CBUS_SCRATCHPAD_SIZE				0x0D

#define CBUS_INT_STAT_SIZE					0x0E

#define CBUS_MSC_RCHANGE_INT											0x20
#define	CBUS_MSC_RCHANGE_INT_DCAP_CHG								0x01
#define	CBUS_MSC_RCHANGE_INT_DSCR_CHG								0x02
#define	CBUS_MSC_RCHANGE_INT_REQ_WRT								0x04
#define	CBUS_MSC_RCHANGE_INT_GRT_WRT								0x08
#define	CBUS_MSC_RCHANGE_INT_3D_REQ								0x10

#define CBUS_MSC_DCHANGE_INT											0x21
#define	CBUS_MSC_DCHANGE_INT_EDID_CHG								0x02

#define CBUS_MSC_STATUS_CONNECTED_RDY								0x30
#define	CBUS_MSC_STATUS_CONNECTED_RDY_DCAP_RDY					0x01

#define CBUS_MSC_STATUS_LINK_MODE									0x31
#define	CBUS_MSC_STATUS_LINK_MODE_CLK_MODE						0x07
#define	CBUS_MSC_STATUS_LINK_MODE_CLK_MODE__Normal				0x03
#define	CBUS_MSC_STATUS_LINK_MODE_CLK_MODE__PacketPixel			0x02
#define	CBUS_MSC_STATUS_LINK_MODE_PATH_EN							0x08
#define	CBUS_MSC_STATUS_IS_MUTED									0x10

#define CBUS_MSC_SCRATCHPAD				0x40

/* /////////////////////////////////////////////////// */
#define	MHL_CBUS_DEVICE_LENGTH 0x50

#define CBUS_MSC_CTRL_ACK 0x533
#define CBUS_MSC_CTRL_NACK 0x534
#define CBUS_MSC_CTRL_ABORT 0x535
#define CBUS_MSC_CTRL_WRITE_STATE 0x560	/* //type 3, source should send 3 packet,need ack */
#define CBUS_MSC_CTRL_SET_INT 0x560	/* //type 3, source should send 3 packet,need ack */
#define CBUS_MSC_CTRL_READ_DEVCAP 0x561	/* //type 1, source should send 2 packet,need ack, and 1 value */
#define CBUS_MSC_CTRL_GET_STATE 0x562	/* //////type 0, no ack , only value */
#define CBUS_MSC_CTRL_GET_VENDER_ID 0x563	/* //////type 0, no ack , only value */
#define CBUS_MSC_CTRL_SET_HPD 0x564	/* //////type 0, no ack , only value */
#define CBUS_MSC_CTRL_CLR_HPD 0x565	/* //////type 0, no ack , only value */
#define CBUS_MSC_CTRL_MSC_MSG 0x568	/* ////////////type 3, source should send 3 packet,need ack, */
										   /* ////////////type 2, source should send 3 packet,need ack,and 3 value */
#define CBUS_MSC_CTRL_GET_SC1_EC 0x569	/* //////type 0, no ack , only value */
#define CBUS_MSC_CTRL_GET_DDC_EC 0x56A	/* //////type 0, no ack , only value */
#define CBUS_MSC_CTRL_GET_MSC_EC 0x56B	/* //////type 0, no ack , only value */
#define CBUS_MSC_CTRL_WRITE_BURST 0x56C
#define CBUS_MSC_CTRL_GET_SC3_EC 0x56D	/* //////type 0, no ack , only value */
#define CBUS_MSC_CTRL_EOF 0x532

enum {
	MHL_MSC_MSG_MSGE = 0x402,	/* RCP sub-command */
	MHL_MSC_MSG_RCP = 0x410,	/* RCP sub-command */
	MHL_MSC_MSG_RCPK = 0x411,	/* RCP Acknowledge sub-command */
	MHL_MSC_MSG_RCPE = 0x412,	/* RCP Error sub-command */
	MHL_MSC_MSG_RAP = 0x420,	/* RAP sub-command */
	MHL_MSC_MSG_RAPK = 0x421,	/* RAP Acknowledge sub-command */
	MHL_MSC_MSG_UCP = 0x430,
	MHL_MSC_MSG_UCPK = 0x431,
	MHL_MSC_MSG_UCPE = 0x432,
};
/************************************************

************************************************/
#define MHL_MSCDDC_TMR_OUT_200MS (200/MHL_LINK_TIME)	/* 90ms timer out */
#define MHL_MSCDDC_ERR_2S (2100/MHL_LINK_TIME)
#define MHL_TXOK_TMR_OUT_20MS (20/MHL_LINK_TIME)	/* 10ms */
#define MHL_TXOK_TMR_OUT_40MS (40/MHL_LINK_TIME)	/* 10ms */
#define MHL_TXOK_TMR_OUT_60MS (60/MHL_LINK_TIME)	/* 10ms */

/************************************************
	DDC cmd
************************************************/


#define CBUS_DDC_READ 0x01
#define CBUS_DDC_SEG_READ 0x02
#define CBUS_DDC_SHORT_READ 0x03
#define CBUS_DDC_CURRENT_READ 0x04
#define CBUS_DDC_WRITR 0x05

#define NONE_PACKET 0xffff
#define NULL_DATA 0xffff

enum				/* msc command reactions */
{
	MHL_MSC_BAD_OFFSET = 0x10,	/* bad offset, send abort */
	MHL_MSC_BAD_OPCODE = 0x08,	/* bad opcode, send abort */
	MHL_MSC_MSG_TIMEOUT = 0x04,	/* MHL_MSC_TOO_FEW_PKT  = 0x04,//time out */
	/* MHL_MSC_INCOMPLETE_PKT = 0x04,//time out */
	MHL_MSC_DEV_BUSY = 0x20,	/* nack */
	MHL_MSC_PROTOCOL_ERR = 0x02,	/* abort */
	MHL_MSC_RETRY_EXCEED = 0x01,	/* time out */
};

#define MHL_CBUS_STATE_IDLE			0
#define MHL_CBUS_STATE_RESPONDER	1
#define MHL_CBUS_STATE_REQUESTER		2
#define MHL_CBUS_STATE_DISCONNET		3

#define MHL_CBUS_STATE_S0	0
#define MHL_CBUS_STATE_S1	1
#define MHL_CBUS_STATE_S2	2
#define MHL_CBUS_STATE_S3	3
#define MHL_CBUS_STATE_S4	4
#define MHL_CBUS_STATE_S5	6
#define MHL_CBUS_STATE_S6	6
#define MHL_CBUS_STATE_S7	7
#define MHL_CBUS_STATE_S8	8
#define MHL_CBUS_STATE_ABORT	100

#define	EDID_BLOCK_LEN      128
#define	EDID_ADDR_EXT_BLOCK_FLAG              0x7E

#define MHL_TX_HW_BUF_MAX 24
#define MHL_RX_HW_BUF_MAX 32
#define MHL_TX_RETRY_NUM 24

/* ////////////////////////////////////////////////////// */
/* /     DDC */
/* ////////////////////////////////////////////////////// */
#define CBUS_DDC_CTRL_SOF 0x130
#define CBUS_DDC_CTRL_EOF 0x132
#define CBUS_DDC_CTRL_ACK 0x133
#define CBUS_DDC_CTRL_NACK 0x134
#define CBUS_DDC_CTRL_ABORT 0x135
#define CBUS_DDC_CTRL_CONT 0x150
#define CBUS_DDC_CTRL_STOP 0x151

#define CBUS_DDC_DATA_SEGW 0x060
#define CBUS_DDC_DATA_ADRW 0x0A0
#define CBUS_DDC_DATA_ADRR 0x0A1

#define CBUS_DDC_DATA_HDCP_ADRW 0x074
#define CBUS_DDC_DATA_HDCP_ADRR 0x075

#define CBUS_DDC_DATA_HDCP_BKSV_OFFSET 0x00
#define CBUS_DDC_DATA_HDCP_RI1_OFFSET 0x08
#define CBUS_DDC_DATA_HDCP_AKSV_OFFSET 0x10
#define CBUS_DDC_DATA_HDCP_AINFO_OFFSET 0x15
#define CBUS_DDC_DATA_HDCP_AN_OFFSET 0x18
#define CBUS_DDC_DATA_HDCP_BCAPS_OFFSET 0x40
#define CBUS_DDC_DATA_HDCP_BSTATUS_OFFSET 0x41

#define CBUS_DDC_DATA_HDCP_KSVFIFO 0X43	/* //////1 */
#define CBUS_DDC_DATA_HDCP_DBG 0XC0	/* ///////64 */

#define RX_REG_HDCP_AN        0x18
/* Aksv register (total 5 bytes, address from 0x10 ~ 0x14) */
#define RX_REG_HDCP_AKSV      0x10
/* Bksv register (total 5 bytes, address from 0x00 ~ 0x04) */
#define RX_REG_HDCP_BKSV      0x00
/* BCAPS register */
#define RX_REG_BCAPS          0x40
#define RX_REG_BSTATUS1       0x41
#define RX_REG_KSV_FIFO       0x43
#define RX_REG_REPEATER_V     0x20
/* Ri register (total 2 bytes, address from 0x08 ~ 0x09) */
#define RX_REG_RI             0x08

#define HDCP_AN_COUNT                 8
#define HDCP_AKSV_COUNT               5
#define HDCP_BKSV_COUNT               5
#define HDCP_RI_COUNT                 2

#define EDID_BLOCK_LEN      128
#define EDID_SIZE 512

#define MHL_CBUS_STATE_DDC_S0	0
#define MHL_CBUS_STATE_DDC_S1	1
#define MHL_CBUS_STATE_DDC_S2	2
#define MHL_CBUS_STATE_DDC_S3	3
#define MHL_CBUS_STATE_DDC_S4	4
#define MHL_CBUS_STATE_DDC_S5	6
#define MHL_CBUS_STATE_DDC_S6	7
#define MHL_CBUS_STATE_DDC_S7	8
#define MHL_CBUS_STATE_DDC_S8	9
#define MHL_CBUS_STATE_DDC_S9	10
#define MHL_CBUS_STATE_DDC_S10	11
#define MHL_CBUS_STATE_DDC_CONT	100
#define MHL_CBUS_STATE_DDC_DATA	101
#define MHL_CBUS_STATE_DDC_STOP	102
#define MHL_CBUS_STATE_DDC_EOF	103
#define MHL_CBUS_STATE_DDC_ABORT	104

#define CBUS_DDC_TOO_FEW_ERR			0x04
#define CBUS_DDC_PROTOCOL_ERR		0x02
#define CBUS_DDC_INCOMPLETE_ERR		0x04
#define CBUS_DDC_RETRY_ERR			0x01

#define CBUS_3D_VIC_ID_H	0x00
#define CBUS_3D_VIC_ID_L	0x10
#define CBUS_3D_DTD_ID_H	0x00
#define CBUS_3D_DTD_ID_L	0x11


typedef struct {
	bool fgIsHPD;
	bool fgIsHPDBak;
	bool fgIsRense;
	bool fgIsRAP;
	/* 020 */
	bool fgIsDCapChg;
	bool fgIsDScrChg;
	bool fgIsReqWrt;
	bool fgIsGrtWrt;
	bool fgIs3DReq;
	/* 0x21 */
	bool fgIsEdidChg;
	/* 0x30 */
	bool fgIsDCapRdy;
	/* 0x31 */
	bool fgIsPPMode;
	bool fgIsPathEn;
	bool fgIsMuted;
	/* for me */
	bool fgMyIsPPModeChg;
	bool fgMyIsPathEnChg;
	bool fgMyIsDcapChg;
	bool fgMyIsMutedChg;
	bool fgMyIsPOWChg;
} stMhlDev_st;
typedef struct {
	unsigned char u1MhlVersion;
	unsigned char u1DevType;
	bool fgIsPower;
	unsigned char u1PowerCap;
	unsigned int u4AdopterID;
	unsigned char u1VidLinkMode;
	unsigned char u1AudLinkMode;
	unsigned char u1VideoType;
	unsigned char u1LogDevMap;
	unsigned int u4BandWidth;
	unsigned char u1FeatureFlag;
	unsigned int u4DeviceID;
	unsigned char u1ScratchpadSize;
	unsigned char u1intStatSize;
} stMhlDcap_st;

typedef struct {
	unsigned short u2Cmd;	/* cmd name */
	unsigned short u2State;	/* cmd flow state */
	bool fgOk;
	bool fgFinish;
} stReq_st;

typedef struct {
	unsigned short u2Cmd;	/* cmd name */
	unsigned short u2State;	/* cmd flow state */
	bool fgOk;
} stDdc_st;

typedef struct {
	unsigned short u2Cmd;	/* cmd name */
	unsigned short u2State;	/* msc flow */
} stResp_st;

typedef enum {
	CBUS_TX_IDLE = 0,
	CBUS_TX_DDC_BUSY,
	CBUS_TX_MSC_BUSY
} CBUS_TX_STATE;

typedef struct {
	unsigned short u2CbusState;
	unsigned short u2CbusDdcState;
	stReq_st stReq;
	stResp_st stResp;
	stDdc_st stDdc;
	unsigned short u2RXBuf[MHL_RX_HW_BUF_MAX];
	unsigned short u2RXBufInd;
} stCbus_st;
typedef struct {
	bool fgIsRequesterWait;
	bool fgIsDdc;
	unsigned short u2ReqBuf[MHL_RX_HW_BUF_MAX];
	unsigned short u4Len;
} stCbusRequester_st;
typedef struct {
	bool fgTxValid;		/* buf have data */
	bool fgTxBusy;
	unsigned short u2TxBuf[MHL_TX_HW_BUF_MAX];
	unsigned short u2Len;
	unsigned short u2Retry;
	unsigned short u2TxOkRetry;
} stCbusTxBuf_st;

typedef struct {
	bool fgIsValid;
	unsigned short u2Code;
	unsigned char u1Val;
} stCbusMscMsg_st;

typedef struct {
	unsigned char u1Seg;	/* cmd name */
	unsigned char u1DevAddrW;	/* cmd name */
	unsigned char u1DevAddrR;	/* cmd name */
	unsigned char u1Offset;	/* cmd flow state */
	unsigned char u1Len;
	unsigned char u1Inx;
	unsigned char u1Buf[128];
} stCbusDdc_st;

/* ///////////////////////////////////////// */
typedef struct {
	unsigned int u4MscDdcTmr;
	bool fgTmrOut;
} st_MscDdcTmrOut_st;

/* ////////////////////////////////////////// */
extern unsigned char _bEdidData[EDID_SIZE];
extern stMhlDcap_st stMhlDcap;
/* //////////////////////////////////////////// */
extern void vCbusInit(void);
extern void vCbusReset(void);
extern void vCbusIntEnable(void);
extern void vCbusIntDisable(void);
extern void vCbusRxEnable(void);
extern void vCbusTxTrigger(void);

extern void vCbusTriggerCheck1K(void);
extern bool fgCbusCheck1K(void);
extern bool fgCbusCheck1KDone(void);
extern bool fgCbusDiscovery(void);
extern bool fgCbusDiscoveryDone(void);

extern unsigned short u2CbusReadRxLen(void);
extern void vCbusSetTxLen(unsigned short i);

extern void CbusTxRxReset(void);
extern unsigned char u1DeviceRegSpace[MHL_CBUS_DEVICE_LENGTH];
extern unsigned char u1MyDeviceRegSpace[MHL_CBUS_DEVICE_LENGTH];
extern stMhlDev_st stMhlDev;

extern st_MscDdcTmrOut_st st_MscTmrOut;
extern st_MscDdcTmrOut_st st_DdcTmrOut;
extern st_MscDdcTmrOut_st st_TxokTmrOut;

extern unsigned int u1CbusHw1KStatus;
extern unsigned int u1CbusHwWakeupStatus;
extern unsigned int u1CbusMscAbortDelay2S;
extern unsigned int u1CbusDdcAbortDelay2S;
extern stCbusMscMsg_st stCbusMscMsg;

extern void vSetMHLUSBMode(unsigned char fgUse);

extern bool fgCbusReqGetStateCmd(void);
extern bool fgCbusReqGetVendorIDCmd(void);
extern bool fgCbusReqReadDevCapCmd(unsigned char u1Data);
extern bool fgCbusReqWriteStatCmd(unsigned char u1Offset, unsigned char u1Data);
extern bool fgCbusReqSetIntCmd(unsigned char u1Offset, unsigned char u1Data);
extern bool fgCbusReqWriteBrustCmd(unsigned char *ptData, unsigned char u1Len);
extern bool fgCbusReqMscMsgCmd(unsigned short u1Cmd, unsigned char u1Val);
extern bool fgCbusReqMscERRCodeCmd(void);
extern bool fgIsMscMsg(unsigned short u2data);

extern bool fgMhlTxWriteAn(unsigned char *ptData);
extern bool fgMhlTxWriteAKsv(unsigned char *ptData);
extern bool fgMhlTxReadRi(unsigned char *ptData);
extern bool fgMhlTxReadBKsv(unsigned char *ptData);
extern bool fgMhlTxReadBStatus1(unsigned char *ptData);
extern bool fgMhlTxReadBStatus0(unsigned char *ptData);
extern bool fgMhlTxReadBStatus(unsigned char *ptData);
extern bool fgMhlTxReadBCAPS(unsigned char *ptData);
extern bool fgMhlTxReadKsvFIFO(unsigned char *ptData, unsigned char u1Len);
extern bool fgMhlTxReadV(unsigned char *ptData);

extern bool fgMhlGetDevCap(void);
extern void vMhlSetDevCapReady(void);
extern bool fgMhlGetEdid(void);

extern void vMhlIntProcess(void);
extern void vCbusCmdStatus(void);
extern void vMhlUnlockCall(void);
extern bool fgCbusReqDdcReadCmd(bool fgEdid, unsigned char u1Seg, unsigned char u1Offset,
				unsigned char u1Len, unsigned char *ptData);
extern bool fgCbusMscIdle(void);
extern bool fgCbusDdcIdle(void);
extern bool fgCbusHwTxIdle(void);
extern bool fgCbusHwRxIdle(void);
extern bool fgCbusHwTxRxIdle(void);
extern void v6397DumpReg(void);
extern void vResetCbusMSCState(void);
extern void vResetCbusDDCState(void);
extern void vCbusTimer(void);
extern void vCbusRxDisable(void);
extern void vCbusWakeupEvent(void);
extern void vCbusWakeupFinish(void);
extern void vCbusCheck1KEvent(void);
extern void vCbusCheck1KFinish(void);
extern void vDcapParser(void);
unsigned char fgCbusValue(void);

#endif
#endif
