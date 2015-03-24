#ifndef _MHL_CBUS_CTRL_H_
#define _MHL_CBUS_CTRL_H_

typedef enum {
	CBUS_LINK_IDLE = 0,
	CBUS_LINK_STATE_USB_MODE,
	CBUS_LINK_STATE_DISCOVERY,
	CBUS_LINK_STATE_CHECK_RENSE,
	CBUS_LINK_STATE_CONNECTED,
	CBUS_LINK_STATE_CONTENT_ON,
	CBUS_LINK_FLOAT_CBUS,

} CBUS_LINK_STATE;


extern unsigned int u2MhlTimerOutCount;
extern unsigned int u2MhlDcapReadDelay;
extern unsigned int u2MhlEdidReadDelay;
extern unsigned int u2MhlReqDelay;
extern unsigned int u2MhlRqdDelayResp;

extern void vMhlConnectCallback(bool fgConnected);
extern void vCbusConnectState(void);
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
#endif
