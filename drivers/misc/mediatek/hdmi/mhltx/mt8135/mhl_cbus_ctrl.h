#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT

#ifndef _MHL_CBUS_CTRL_H_
#define _MHL_CBUS_CTRL_H_

typedef enum {
	CBUS_LINK_STATE_USB_MODE = 0,
	CBUS_LINK_STATE_CHECK_1K,
	CBUS_LINK_STATE_CHECK_RENSE,
	CBUS_LINK_STATE_CONNECTED,
	CBUS_LINK_STATE_CONTENT_ON,
	CBUS_LINK_FLOAT_CBUS,

} CBUS_LINK_STATE;

#define MHL_NO_CONNECT		0
#define MHL_CONNECT			1
#define MHL_CONNECT_DEFULT	0xFF

extern unsigned int u2MhlTimerOutCount;
extern unsigned int u2MhlDcapReadDelay;
extern unsigned int u2MhlEdidReadDelay;
extern unsigned int u2MhlReqDelay;
extern unsigned int u2MhlRqdDelayResp;
extern unsigned char u1CbusConnectState;

extern void vMhlConnectCallback(unsigned char u1Connected);
extern void vCbusConnectState(void);
void vCbusCmdState(void);
extern void vCbusCtrlInit(void);
extern void vCbusLinkStatus(void);
extern bool fgIsCbusContentOn(void);
extern void vSetMhlRsen(bool fgRsen);
extern bool fgGetMhlRsen(void);
extern bool fgIsCbusConnected(void);
extern bool fgIsCbusContentOn(void);
extern void vCbusCtrlInit(void);
extern void vCbusStart(void);
extern void vSetMhlPPmode(bool isPP);

void mhl_AppGetDcapData(unsigned char *pv_get_info);
void mhl_AppGet3DInfo(MHL_3D_INFO_T *pv_get_info);

#endif
#endif
