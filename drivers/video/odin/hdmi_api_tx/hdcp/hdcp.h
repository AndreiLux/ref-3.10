/*
 * hdcp.h
 *
 *  Created on: Jul 21, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HDCP_H_
#define HDCP_H_

#include "../util/types.h"
#include "hdcpParams.h"

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param dataEnablePolarity
 * @return TRUE if successful
 */
int hdcp_initialize(u16 baseAddr, int dataEnablePolarity);
/**
 * @param baseAddr Base address of the HDCP module registers
 * @param params HDCP parameters
 * @param hdmi TRUE if HDMI, FALSE if DVI
 * @param hsPol HSYNC polarity
 * @param vsPol VSYNC polarity
 * @return TRUE if successful
 */
int hdcp_configure(u16 baseAddr, hdcpParams_t *params, int hdmi, int hsPol,
		int vsPol);
/**
 * The method handles DONE and ERROR events.
 * A DONE event will trigger the retrieving the read byte, and sending
 * a request to read the following byte. The EDID is read until the block
 * is done and then the reading moves to the next block.
 *  When the block is successfully read, it is sent to be parsed.
 * @param baseAddr Base address of the HDCP module registers
 * @param hpd on or off
 * @param state of the HDCP engine interrupts
 * @param ksvHandler Handler to call when KSV list is ready
 * @return the state of which the event was handled (FALSE for fail)
 */
u8 hdcp_eventhandler(u16 baseAddr, int hpd, u8 state, handler_t ksvHandler);
/**
 * @param baseAddr Base address of the HDCP module registers
 * @param detected if RX is detected (TRUE or FALSE)
 */
void hdcp_rxdetected(u16 baseAddr, int detected);
/**
 * Enter or exit AV mute mode
 * @param baseAddr Base address of the HDCP module registers
 * @param enable the HDCP AV mute
 */
void hdcp_avmute(u16 baseAddr, int enable);
/**
 * Bypass data encryption stage
 * @param baseAddr Base address of the HDCP module registers
 * @param bypass the HDCP AV mute
 */
void hdcp_bypassencryption(u16 baseAddr, int bypass);
/**
 * @param baseAddr Base address of the HDCP module registers
 * @param disable the HDCP encrption
 */
void hdcp_disableencryption(u16 baseAddr, int disable);
/**
 * @param baseAddr Base address of the HDCP module registers
 * @return TRUE if engaged
 */
int hdcp_engaged(u16 baseAddr);
/**
 * The engine goes through the authentication
 * statesAs defined in the HDCP spec
 * @param baseAddr Base address of the HDCP module registers
 * @return a the state of the authentication machine
 * @note Used for debug purposes
 */
u8 hdcp_authenticationstate(u16 baseAddr);
/**
 * The engine goes through several cipher states
 * @param baseAddr Base address of the HDCP module registers
 * @return a the state of the cipher machine
 * @note Used for debug purposes
 */
u8 hdcp_cipherstate(u16 baseAddr);
/**
 * The engine goes through revocation states
 * @param baseAddr Base address of the HDCP module registers
 * @return a the state of the revocation the engine is going through
 * @note Used for debug purposes
 */
u8 hdcp_revocationstate(u16 baseAddr);
/**
 * @param baseAddr Base address of the HDCP module registers
 * @param data pointer to memory where SRM info is located
 * @return TRUE if successful
 */
int hdcp_srmupdate(u16 baseAddr, const u8 * data);
/**
 * @param baseAddr base address of HDCP module registers
 * @return HDCP interrupts state
 */
u8 hdcp_interruptstatus(u16 baseAddr);
/**
 * Clear HDCP interrupts
 * @param baseAddr base address of controller
 * @param value mask of interrupts to clear
 * @return TRUE if successful
 */
int hdcp_interruptclear(u16 baseAddr, u8 value);

void hdmi_rxkeywrite(u16 baseAddr);
#endif /* HDCP_H_ */
