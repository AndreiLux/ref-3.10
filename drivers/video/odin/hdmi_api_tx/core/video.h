/*
 * video.h
 *
 *  Created on: Jul 5, 2010
 *
 * Synopsys Inc.
 * SG DWC PT02
 */

#ifndef VIDEO_H_
#define VIDEO_H_
/**
 * @file
 * Video controller layer.
 * Configure and set up the video transmitted to sink.
 * This includes colour-space conversion as well as pixel packetising.
 */

#include "../util/types.h"
#include "videoParams.h"

/**
 * Initializes and configures the video blocks to transmit a blue screen
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @param dataEnablePolarity data enable polarity (1 = enable, 0 not)
 * @return TRUE if successful
 */
int video_initialize(u16 baseAddr, videoParams_t *params,u8 dataEnablePolarity);

/**
 * Configures the video blocks to do any video processing and to
 * transmit the video set up required by the user, allowing to
 * force video pixels (from the DEBUG pixels) to be transmitted
 * rather than the video stream being received.
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @param dataEnablePolarity
 * @param hdcp Whether HDCP is required (1 = required, 0 not)
 * @return TRUE if successful
 */
int video_configure(u16 baseAddr, videoParams_t *params, u8 dataEnablePolarity,
		u8 hdcp, u8 fixed_color_screen);

/**
 * Force output video (blue screen)
 * @param baseAddr Base Address of module
 * @param force when 1 forces output video
 * @return TRUE if successful
 */
int video_forceoutput(u16 baseAddr, u8 fixed_color_screen);

/**
 * Set up color space converter to video requirements
 * (if there is any encoding type conversion or csc coefficients)
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @return TRUE if successful
 */
int video_colorspaceconverter(u16 baseAddr, videoParams_t *params);

/**
 * Set up frame composer which transmits the video stream
 * as indicated by the DTD (all physical signals about the video)
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @param dataEnablePolarity
 * @param hdcp Whether HDCP is required (1 = required, 0 not)
 * @return TRUE if successful
 */
int video_framecomposer(u16 baseAddr, videoParams_t *params,
		u8 dataEnablePolarity, u8 hdcp);

/**
 * Set up video packetizer which "packetizes" pixel transmission
 * (in deep colour mode, YCC422 mapping and pixel repetition)
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @return TRUE if successful
 */
int video_videopacketizer(u16 baseAddr, videoParams_t *params);

/**
 * Set up video mapping and stuffing
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @return TRUE if successful
 */
int video_videosampler(u16 baseAddr, videoParams_t *params);

/**
 * A test only method that is used for a test module
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @param dataEnablePolarity
 * @return TRUE if successful
 */
int video_videogenerator(u16 baseAddr, videoParams_t *params,
		u8 dataEnablePolarity);

#endif /* VIDEO_H_ */
