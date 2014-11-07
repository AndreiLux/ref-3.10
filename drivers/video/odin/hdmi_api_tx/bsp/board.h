/*
 * board.h
 *
 *  Created on: Jun 28, 2010
 * 
 * Synopsys Inc.
 * SG DWC PT02
 */
/**
 * @file
 * board specific functions
 * configure onboard PLLs and multiplexers
 * 	@note: this file should be re-written to match the environment the 
 * 	API is running on
 */

#ifndef BOARD_H_
#define BOARD_H_

#include "../util/types.h"

/** Initialize board
 * @param baseAddr base address of controller
 * @param pixelClock pixel clock [10KHz]
 * @param cd color depth (8, 10, 12 or 16)
 * @return TRUE if successful
 */
int board_initialize(u16 baseAddr, u16 pixelClock, u8 cd);

/** Set up oscillator for audio
 * @param baseAddr base address of controller
 * @param value: audio clock [Hz]
 * @return TRUE if successful
 */
int board_audioclock(u16 baseAddr, u32 value);

/** Set up oscillator for video
 * @param baseAddr base address of controller
 * @param value: pixel clock [10KHz]
 * @param cd color depth
 * @return TRUE if successful
 */
int board_pixelclock(u16 baseAddr, u16 value, u8 cd);

/**  Write to DRP controller which allows to change the PLL parameters
 *  dynamically - controlling the input signals of the controller
 *  @note: this is not included in package but rather board specific
 *  @param pllBaseAddress 16-bit base address of a DRP PLL controller
 *  @param regAddr 8-bit register address in the DRP PLL controller
 *  @param data 8-bit data to be written to the refereced register
 */
void board_pllwrite(u16 pllBaseAddress, u8 regAddr, u16 data);

/** Bypass internal static frame video generator
 * @param baseAddr base address of controller
 * @param enable 0 off, 1 on (ie external video through)
 */
void board_videogeneratorbypass(u16 baseAddr, u8 enable);

/** Read value in a specific register in a DRP PLL controller
 *  @param pllBaseAddress 16-bit base address of a DRP PLL controller
 *  @param regAddr 8-bit register address in the DRP PLL controller
 */
u16 board_pllread(u16 pllBaseAddress, u8 regAddr);

/** Reset the DRP PLL controller
 * Usually called before and after setting of controller
 * @param pllBaseAddress 16-bit base address of a DRP PLL controller
 * @param value
 */
void board_pllreset(u16 pllBaseAddress, u8 value);

/** Bypass internal audio generator
 * @param baseAddr base address of controller
 * @param enable 0 off, 1 on (ie external audio through)
 */
void board_audiogeneratorbypass(u16 baseAddr, u8 enable);

/** Return the refresh rates supported by the board for the CEA codes
 * @return u32 refresh rate  in ([Hz] x 100) for supported modes, -1 if not
 * @note: refresh rates affect pixel clocks, which not all are
 * supported by the demo board 
 * (eg. for 25.18MHz and 25.2Mhz, only  25.2MHz is supported)
 */
u32 board_supportedrefreshrate(u8 code);

#endif /** BOARD_H_ */
