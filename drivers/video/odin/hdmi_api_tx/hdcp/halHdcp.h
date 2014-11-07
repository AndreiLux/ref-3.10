/*
 * halHdcp.h
 *
 *  Created on: Jul 19, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02 
 */
 /** @file
 *   HAL (hardware abstraction layer) of HDCP engine register block
 */

#ifndef HALHDCP_H_
#define HALHDCP_H_

#include "../util/types.h"

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_devicemode(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_enablefeature11(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_rxdetected(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_enableavmute(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_richeck(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_bypassencryption(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_enablei2cfastmode(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_enhancedlinkverification(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 */
void halhdcp_swreset(u16 baseAddr);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_disableencryption(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_encodingpacketheader(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_disableksvlistcheck(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 */
u8 halhdcp_hdcpengaged(u16 baseAddr);

/**
 * @param baseAddr Base address of the HDCP module registers
 */
u8 halhdcp_authenticationstate(u16 baseAddr);

/**
 * @param baseAddr Base address of the HDCP module registers
 */
u8 halhdcp_cipherstate(u16 baseAddr);

/**
 * @param baseAddr Base address of the HDCP module registers
 */
u8 halhdcp_revocationstate(u16 baseAddr);

/**
 * @param baseAddr Base address of the HDCP module registers
 */
u8 halhdcp_oessstate(u16 baseAddr);

/**
 * @param baseAddr Base address of the HDCP module registers
 */
u8 halhdcp_eessstate(u16 baseAddr);

/**
 * @param baseAddr Base address of the HDCP module registers
 */
u8 halhdcp_debuginfo(u16 baseAddr);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param value
 */
void halhdcp_interruptclear(u16 baseAddr, u8 value);

/**
 * @param baseAddr Base address of the HDCP module registers
 */
u8 halhdcp_interruptstatus(u16 baseAddr);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param value
 */
void halhdcp_interruptmask(u16 baseAddr, u8 value);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_hsyncpolarity(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_vsyncpolarity(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_dataenablepolarity(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param value
 */
void halhdcp_unencryptedvideocolor(u16 baseAddr, u8 value);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param value
 */
void halhdcp_oesswindowsize(u16 baseAddr, u8 value);

/**
 * @param baseAddr Base address of the HDCP module registers
 */
u16 halhdcp_coreversion(u16 baseAddr);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_memoryaccessrequest(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 */
u8 halhdcp_memoryaccessgranted(u16 baseAddr);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param bit
 */
void halhdcp_updateksvliststate(u16 baseAddr, u8 bit);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param addr in list
 */
u8 halhdcp_ksvlistread(u16 baseAddr, u16 addr);

/**
 * @param baseAddr Base address of the HDCP module registers
 * @param addr in list
 * @param data
 */
void halhdcp_revoclistwrite(u16 baseAddr, u16 addr, u8 data);
void halhdcp_key(u16 baseAddr, u8 data);

#endif /* HALHDCP_H_ */
