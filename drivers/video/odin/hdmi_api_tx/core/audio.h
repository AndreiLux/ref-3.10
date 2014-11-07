/*
 * audio.h
 *
 * Synopsys Inc.
 * SG DWC PT02
 */
/** @file
 * audio module, initialisation and configuration.
 */

#ifndef AUDIO_H_
#define AUDIO_H_

#include "../util/types.h"
#include "audioParams.h"
/**
 * Initial set up of package and prepare it to be configured.
 * Set audio mute to on.
 * @param baseAddr base Address of controller
 * @return TRUE if successful
 */
int audio_initialize(u16 baseAddr);
/**
 * Configure hardware modules corresponding to user
 * requirements to start transmitting audio packets.
 * @param baseAddr base Address of controller
 * @param params: audio parameters
 * @param pixelClk: pixel clock [0.01 MHz]
 * @param ratioClk: ratio clock (TMDS / Pixel) [0.01]
 * @return TRUE if successful
 */
int audio_configure(
	u16 baseAddr, audioParams_t *params, u16 pixelClk, unsigned int ratioClk);
/**
 * Mute audio.
 * Stop sending audio packets
 * @param baseAddr base Address of controller
 * @param state:  1 to enable/0 to disable the mute
 * @return TRUE if successful
 */
int audio_mute(u16 baseAddr, u8 state);
/**
 * The module is implemented for testing reasons. The method has no effect in
 *  the real application.
 * @param baseAddr base Address of audio generator module
 * @param params: audio parameters
 * @return TRUE if successful
 */
int audio_audiogenerator(u16 baseAddr, audioParams_t *params);
/**
 * Compute the value of N to be used in a clock divider to generate an
 * intermediate clock that is slower than the 128�fS clock by the factor N.
 * The N value relates the TMDS to the sampling frequency fS.
 * @param baseAddr base Address of controller
 * @param freq: audio sampling frequency [Hz]
 * @param pixelClk: pixel clock [0.01 MHz]
 * @param ratioClk: ratio clock (TMDS / Pixel) [0.01]
 * @return N
 */
u16 audio_computen(u16 baseAddr, u32 freq, u16 pixelClk, u16 ratioClk);
/**
 * Calculate the CTS value, the denominator of the fractional relationship
 * between the TMDS clock and the audio reference clock used in ACR
 * @param baseAddr base Address of controller
 * @param freq: audio sampling frequency [Hz]
 * @param pixelClk: pixel clock [0.01 MHz]
 * @param ratioClk: ratio clock (TMDS / Pixel) [0.01]
 * @return CTS value
 */
u32 audio_computects(u16 baseAddr, u32 freq, u16 pixelClk, u16 ratioClk);
/**
 * Configure the memory range on which the DMA engine perform autonomous
 * burst read operations.
 * @param baseAddr base Address of controller
 * @param startAddress 32 bit address for the DMA to start its read requests at
 * @param stopAddress 32 bit address for the DMA to end its read requests at
 */
void audio_dmarequestaddress(u16 baseAddr, u32 startAddress, u32 stopAddress);
/**
 * Get the length of the current burst read request operation
 * @param baseAddr base Address of controller
 */
u16 audio_dmagetcurrentburstlength(u16 baseAddr);
/**
 * Get the start address of the current burst read request operation
 * @param baseAddr base Address of controller
 */
u32 audio_dmagetcurrentoperationaddress(u16 baseAddr);
/**
 * Start the DMA read operation
 * @param baseAddr base Address of controller
 * @note After completion of the DMA operation, the DMA engine issues the done
 *  interrupt signaling end of operation
 */
void audio_dmastartread(u16 baseAddr);
/**
 * Stop the DMA read operation
 * @param baseAddr base Address of controller
 * @note After stopping of the DMA operation, the DMA engine issues the done
 *  interrupt signaling end of operation
 */
void audio_dmastopread(u16 baseAddr);
/**
 * Enable Audio DMA interrupts
 * @param baseAddr base Address of controller
 * @param mask of the interrupt register
 */
void audio_dmainterruptenable(u16 baseAddr, u8 mask);
/**
 * Status of Audio DMA interrupt mask
 * @param baseAddr base Address of controller
 * @return contents of interrupt mask register
 */
u8 audio_dmainterruptenablestatus(u16 baseAddr);

#endif /* AUDIO_H_ */
