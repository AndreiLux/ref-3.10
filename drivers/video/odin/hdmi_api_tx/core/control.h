/*
 * control.h
 *
 *  Created on: Jul 2, 2010
 *
 * Synopsys Inc.
 * SG DWC PT02
 */
/**
 * @file
 * Core control module:
 * Product information
 * Power management
 * Interrupt handling
 *
 */
#ifndef CONTROL_H_
#define CONTROL_H_

#include "../util/types.h"
/**
 * Initializes PHY and core clocks
 * @param baseAddr base address of controller
 * @param dataEnablePolarity data enable polarity
 * @param pixelClock pixel clock [10KHz]
 * @return TRUE if successful
 */
int control_initialize(u16 baseAddr, u8 dataEnablePolarity, u16 pixelClock);
/**
 * Configures PHY and core clocks
 * @param baseAddr base address of controller
 * @param pClk pixel clock [10KHz]
 * @param pRep pixel repetition factor
 * @param cRes color resolution
 * @param cscOn 1 if colour space converter active
 * @param audioOn 1 if Audio active
 * @param cecOn 1 if Cec module active
 * @param hdcpOn 1 if Hdcp active
 * @return TRUE if successful
 */
int control_configure(u16 baseAddr, u16 pClk, u8 pRep, u8 cRes, int cscOn,
		int audioOn, int cecOn, int hdcpOn);
/**
 * Go into standby mode: stop all clocks from
 * all modules except for the CEC (refer to CEC for more detail)
 * @param baseAddr base address of controller
 * @return TRUE if successful
 */
int control_standby(u16 baseAddr);
/**
 * Read product design information
 * @param baseAddr base address of controller
 * @return the design number stored in the hardware
 */
u8 control_design(u16 baseAddr);
/**
 * Read product revision information
 * @param baseAddr base address of controller
 * @return the revision number stored in the hardware
 */
u8 control_revision(u16 baseAddr);
/**
 * Read product line information
 * @param baseAddr base address of controller
 * @return the product line stored in the hardware
 */
u8 control_productline(u16 baseAddr);
/**
 * Read product type information
 * @param baseAddr base address of controller
 * @return the product type stored in the hardware
 */
u8 control_producttype(u16 baseAddr);
/**
 * Check if hardware is compatible with the API
 * @param baseAddr base address of controller
 * @return TRUE if the HDMICTRL API supports the hardware (HDMI TX)
 */
int control_supportscore(u16 baseAddr);
/**
 * Check if HDCP is instantiated in hardware
 * @param baseAddr base address of controller
 * @return TRUE if hardware supports HDCP encryption
 */
int control_supportshdcp(u16 baseAddr);

/**
 * Mute controller interrupts
 * @param baseAddr base address of controller
 * @param value mask of the register
 * @return TRUE when successful
 */
int control_interruptmute(u16 baseAddr, u8 value);
/**
 * Clear CEC interrupts
 * @param baseAddr base address of controller
 * @param value
 * @return TRUE if successful
 */
int control_interruptcecclear(u16 baseAddr, u8 value);
/**
 * @param baseAddr base address of controller
 * @return CEC interrupts state
 */
u8 control_interruptcecstate(u16 baseAddr);
/**
 * Clear EDID reader (I2C master) interrupts
 * @param baseAddr base address of controller
 * @param value
 * @return TRUE if successful
 */
int control_interruptedidclear(u16 baseAddr, u8 value);

/**
 * @param baseAddr base address of controller
 * @return EDID reader interrupts state (I2C master controller)
 */
u8 control_interruptedidstate(u16 baseAddr);
/**
 * Clear phy interrupts
 * @param baseAddr base address of controller
 * @param value
 * @return TRUE if successful
 */
int control_interruptphyclear(u16 baseAddr, u8 value);

/**
 * @param baseAddr base address of controller
 * @return PHY interrupts state
 */
u8 control_interruptphystate(u16 baseAddr);
/**
 * @param baseAddr base address of controller
 * @return Audio DMA interrupts state
 */
u8 control_interruptaudiodmastate(u16 baseAddr);
/**
 * Clear Audio DMA interrupts
 * @param baseAddr base address of controller
 * @param value
 * @return TRUE if successful
 */
int control_interruptaudiodmaclear(u16 baseAddr, u8 value);

/**
 * Clear all controller interrputs (except for hdcp)
 * @param baseAddr base address of controller
 * @return TRUE if successful
 */
int control_interruptclearall(u16 baseAddr);

int control_interruptaudiosamplermute(u16 baseAddr, u8 value);

int control_interruptvideopacketizermute(u16 baseAddr, u8 value);

int control_interrupti2cphymute(u16 baseAddr, u8 value);

int control_interruptaudiopacketsmute(u16 baseAddr, u8 value);

int control_interruptotherpacketsmute(u16 baseAddr, u8 value);

int control_interruptpacketsoverflowmute(u16 baseAddr, u8 value);

#endif /* CONTROL_H_ */
