/*
 * api.h
 *
 *  Created on: Jul 7, 2010
 *
 * Synopsys Inc.
 * SG DWC PT02
 */
/**
 * @file
 * This is the upper layer of the HDMI TX API.
 * It is the entry point and interface of the software package
 * It crosses information from the sink and information passed on from
 * the user to check if it violates the HDMI protocol or not.
 * It coordinates between the various modules of the API to program the
 * controller successfully in the correct steps and in a compliant
 * configuration.
 * Errors are written to the error buffer. use error_Get in error.h to read
 * last error.
 */

#ifndef API_H_
#define API_H_
#include "../util/types.h"
#include "../util/error.h"
#include "../core/productParams.h"
#include "../core/videoParams.h"
#include "../core/audioParams.h"
#include "../hdcp/hdcpParams.h"
#include "../edid/dtd.h"
#include "../edid/shortVideoDesc.h"
#include "../edid/shortAudioDesc.h"
#include "../edid/hdmivsdb.h"
#include "../edid/monitorRangeLimits.h"
#include "../edid/videoCapabilityDataBlock.h"
#include "../edid/colorimetryDataBlock.h"
#include "../edid/speakerAllocationDataBlock.h"
#include "../bsp/board.h"
#include "../bsp/access.h"
#include "../bsp/system.h"
#include "../phy/phy.h"


/** event_t events to register a callback for in the API
 */
typedef enum
{
	EDID_READ_EVENT = 0, /** calls registered callback when through reading
							E-EDID information */
	HPD_EVENT, /** calls registered callback for any rising edge
					or falling edge events */
	DMA_DONE,	/** calls registered callback when a DMA is done reading */
	DMA_ERROR,	/** calls registered callback when a DMA error occurs */
	KSV_LIST_READY, /** calls registered callback when ksv list is updated
						with buffer_t struct, having:
						- KSV list (5 bytes each entry) first
						- Bstatus and M0 (10 bytes)
						- V' 20 bytes */
	DUMMY		/* out of bound */
}
event_t;
/**
 * state_t Internal state of HDMI API Tx
 */
typedef enum
{
	API_NOT_INIT = 1,
	API_INIT,
	API_HPD,
	API_EDID_READ,
	API_CONFIGURED
}
state_t;
/**
 * Sets initial state of hardware and software to be able to be
 * configured
 * prepares to listen for HPD and to read the E-EDID structure at the sink.
 * @param address offset of the core instance from the bus base address
 * @param dataEnablePolarity data enable polarity
 * @param sfrClock external clock supplied to the HDMI core
 * @param force configuration without sensing/waiting for HPD
 * @return TRUE when successful
 * @note must be called either upon HW bring up or after a standby
 */
int api_initialize(u16 address, u8 dataEnablePolarity,
	u16 sfrClock, u8 fixed_color_screen);
/**
 * Configure API.
 * Configure the modules of the API according to the parameters given by
 * the user. If EDID at sink is read, it does parameter checking using the
 * Check methods against the sink's E-EDID. Violations are outputted to the
 * buffer.
 * Shall only be called after an Init call or configure.
 * @param video parameters pointer
 * @param audio parameters pointer
 * @param product parameters pointer
 * @param hdcp parameters pointer
 * @return TRUE when successful
 * @note during this function, all controller's interrupts are disabled
 * @note this function needs to have the HW initialised before the first call
 */
int api_configure(videoParams_t * video, audioParams_t * audio,
		productParams_t * product, hdcpParams_t * hdcp, u8 fixed_color_screen);
/**
 * Prepare API modules and local variables to standby mode (and not respond
 * to interrupts) and frees all resources
 * @return TRUE when successful
 * @note must be called to free up resources and before another Init.
 */
int api_standby(void);
/**
 * Read the contents of a certain byte in the hardware.
 * @param addr address of the byte to be read.
 * @return the value held in the byte.
 */
u8 api_coreread(u16 addr);
/**
 * Write a byte to a certain register in the hardware.
 * @note please use care when writing to the core as it
 * may alter the behaviour of the API
 * @param data byte to be wrtten to HW.
 * @param addr address of register to be written to
 */
void api_corewrite(u8 data, u16 addr);
/**
 * Event Handler determines the active HDMI event, and calls the associated
 * function if event is enabled, assigned by the user.
 * If the system package is fully implemented (ie. interrupt handling), this
 * function will be registered directly in the IRQ request to the OS, otherwise
 * Event Handler shall be called explicitly by the user when the HDMICTRL
 * interrupt line is active
 * (or to be called periodically if polling detection is used instead).
 * @param param dummy argument pointer to void
 */
void api_eventhandler(void * param);
/**
 * Event Enable enables a certain event notification giving it a function
 * handle, in which this function is called whenever this event occurs.
 * @param idEvent the assigned ID of type EventType
 * @param handler of the function to be called when a certain even is
 * raised. (handlers receive a void * param, which the HPD state will be passed
 * as an argument)
 * @param oneshot to indicate if it is to be enabled for only one time
 * @return TRUE when successful
 * @note This function must be called only before API initialization
 * that is either before any initialize calls are done or after
 * a standby. Otherwise race conditions might occur as it uses structures
 * used within interrupts
 */
int api_eventenable(event_t idEvent, handler_t handler, u8 oneshot);
/**
 * Event Clear clears a certain event when the event is handled and responded
 *  to. To be called explicitly by the user when the event is handled and
 *  closed, to prevent same events of the same type, or line fluctuations to
 *  happen while user is evaluating and responding to the event.
 * @param idEvent the id given to the even.
 * @return TRUE when successful
 * @note this function must be called only before API initialization
 * that is either before any initialize calls are done or after
 * a standby. otherwise race conditions might occur as it uses structures
 * used within interrupts
 */
int api_eventclear(event_t idEvent);
/**
 * Disables a certain event  already assigned and enabled.  This will prevent
 *  SW and HW from generating such event.
 * @param idEvent the assigned ID of type EventType
 * @return TRUE when successful
 * @note this function must be called only before api initialization
 * that is either before any initialize calls are done or after
 * a standby. otherwise race conditions might occur as it uses structures
 * used within interrupts
 */
int api_eventdisable(event_t idEvent);
/**
 * Trigger reading E-EDID sequence. It will enable edid interrupts.
 * @note make sure PHY is outputting TMDS clock (video) when this function
 * is invoked. some sinks disable E-EDID memory if no video is detected
 * @param handler of the function to be called when E-EDID reading is done
 * @return TRUE when successful
 */
int api_edidread(handler_t handler);
/**
 * Get number of  DTDs read from the sink's EDID structure
 * @return the number of Detailed Timing Descriptors read and parsed from EDID
 *  structure
 */
int api_ediddtdcount(void);
/**
 * Get the indicated Detailed Timing Descriptor read from
 * the sink's EDID structure
 * @param n the index of DTD as parsed from EDID structure
 * @param dtd pointer to DTD structure memory already allocated for the DTD
 * data to be copied to
 * @return FALSE when no valid DTD at that index is found
 */
 int api_ediddtd(unsigned  int n, dtd_t * dtd);
/**
 * @param vsdb pointer to HDMI VSDB structure memory already allocated for the
 *  HDMI VSDB data to be copied to
 * @return FALSE when no valid HDMI VSDB has been parsed
 */
 int api_edidhdmivsdb(hdmivsdb_t * vsdb);

/**
 * @param name pointer to char array memory already allocated for the monitor
 *  name to be copied to
 * @param length of the char array allocated
 * @return FALSE when no valid Monitor name is parsed
 */
 int api_edidmonitorname(char * name , unsigned  int length);

/**
 * @param limits pointer to monitor range limits structure memory already
 * allocated for the data to be copied to
 * @return FALSE when no valid Monitor Range Limits has been parsed
 */
 int api_edidmonitorrangelimits(monitorRangeLimits_t * limits);
 /**
  * Get the number of Short Video Descriptors, read from Video Data Block, at
  * the sink's EDID data structure
  * @return the number of Short Video Descriptors read and parsed from EDID
  *  structure
 */
 int api_edidsvdcount(void);
/**
 * Get an indicated Short Video Descriptors, read from Video Data Block, at the
 *  sink's EDID data structure
 * @param n the index of SVD as parsed from EDID structure
 * @param svd pointer to short video descriptor structure memory already
 *  allocated for the SVD
 * data to be copied to
 * @return TRUE when no valid SVD at that index is found
 */
 int api_edidsvd(unsigned  int n, shortVideoDesc_t * svd);
 /**
  * Get the number of Short Audio Descriptors, read from Audio Data Block, at
  * the sink's EDID data structure
  * @return the number of Short Audio Descriptors read and
  * parsed from EDID structure
 */
 int api_edidsadcount(void);
/**
 * Get an indicated Short Audio Descriptor, read from Audio Data Block, at the
 * sink's EDID data structure
 * @param n the index of Short Audio Descriptor as parsed from EDID structure
 * @param sad pointer to Short Audio Descriptor structure memory already
 * allocated for the SAD data to be copied to
 * @return FALSE when no valid Short Audio Descriptor at that index is found
 */
 int api_edidsad(unsigned  int n, shortAudioDesc_t * sad);
 /**
 * @param capability pointer to Video Capability Data Block
 * structure memory already allocated for the data to be copied to
 * @return FALSE when no valid Video Capability Data Block has been parsed
 */
 int api_edidvideocapabilitydatablock(
 	videoCapabilityDataBlock_t * capability);
 /**
 * @param allocation pointer to Speaker Allocation Data Block
 * structure memory already allocated for the data to be copied to
 * @return FALSE when no valid Speaker Allocation Data Block has been parsed
 */
 int api_edidspeakerallocationdatablock(
 	speakerAllocationDataBlock_t * allocation);
 /**
 * @param colorimetry pointer to Colorimetry Data Block
 * structure memory already allocated for the data to be copied to
 * @return FALSE when no valid Colorimetry Data Block has been parsed
 */
 int api_edidcolorimetrydatablock(colorimetryDataBlock_t * colorimetry);
/**
* @return TRUE if Basic Audio is supported by sink
*/
 int api_edidsupportsbasicaudio(void);
 /**
* @return TRUE if underscan is supported by sink
*/
 int api_edidsupportsunderscan(void);
/**
* @return TRUE if YCC:4:2:2 is supported by sink
*/
 int api_edidsupportsycc422(void);
/**
* @return TRUE if YCC:4:4:4 is supported by sink
*/
int api_edidsupportsycc444(void);
/**
 * Configure Audio Content Protection packets.
 * The function will configure ACP packets to be sent automatically with
 * the passed content and type until it is stopped using the respective
 * stop method.
 * @param type Content protection type (see HDMI1.3a Section 9.3)
 * @param fields ACP Type Dependent Fields
 * @param length of the ACP fields
 * @param autoSend Send Packets Automatically
 */
void api_packetaudiocontentprotection(u8 type, u8 * fields, u8 length,
		u8 autoSend);
/**
 * Configure ISRC 1 & 2 Packets
 * The function will configure ISRC packets to be sent
 * (automatically, when autoSend is set) with the passed content and type until
 *  it is stopped using the respective stop method.
 * The method will split the codes to ISRC1 and ISRC2 packets
 * automatically when needed. Only the valid packets will be sent (ie. if
 * ISRC2 has no valid data it will not be sent)
 * @param initStatus Initial status which the packets are sent with
 * (usually starting position)
 * @param codes ISRC codes as an array
 * @param length of the ISRC codes array
 * @param autoSend Send ISRC Automatically
 * @note Automatic sending does not change status automatically, it does
 * the insertion of the packets in the data islands.
 */
void api_packetsisrc(u8 initStatus, u8 * codes, u8 length, u8 autoSend);
/**
 * Set ISRC status that is changing during play back depending on position
 * (see HDMI 1.3a Section 8.8)
 * @param status the ISRC status code according to position of track
 */
void api_packetsisrcstatus(u8 status);
/**
 * Configure Gamut Metadata packets to be sent with the passed content. It
 * is essential for the colorimetry to be set as Extended and the encoding
 * as YCbCr of the output video for the sink to interpret the Gamut Packet
 * content properly (and for the Gamut packets to be legal in the HDMI
 * protocol)
 * @param gbdContent Gamut Boundary Description Content as an array
 * @param length of the Gamut Boundary Description Content array
 */
void api_packetsgamutmetadata(u8 * gbdContent, u8 length);
/**
 * Stop sending ACP packets when in auto send mode
 */
void api_packetsstopsendacp(void);
/**
 * Stop sending ISRC 1 & 2 packets when in auto send mode
 * (ISRC 2 packets cannot be send without ISRC 1)
 */
void api_packetsstopsendisrc(void);
/**
 * Stop sending Source Product Description InfoFrame
 * packets when in auto send mode
 */
void api_packetsstopsendspd(void);
/**
 * Stop sending Vendor Specific InfoFrame packets when in auto send mode
 */
void api_packetsstopsendvsd(void);
/**
 * Send/stop sending AV Mute in the General Control Packet
 * @param enable TRUE set the AVMute in the general control packet
 */
void api_avmute(int enable);
/**
 * Put audio samples to flat.
 * @param enable TRUE to mute audio
 */
void api_audiomute(int enable);
/**
 * Configure the memory range on which the DMA engine perform autonomous
 * burst read operations.
 * @param startAddress 32 bit address for the DMA to start its read requests at
 * @param stopAddress 32 bit address for the DMA to end its read requests at
 */
void api_audiodmarequestaddress(u32 startAddress, u32 stopAddress);
/**
 * Get the length of the current burst read request operation
 */
u16 api_audiodmagetcurrentburstlength(void);
/**
 * Get the start address of the current burst read request operation
 */
u32 api_audiodmagetcurrentoperationaddress(void);
/**
 * Start the DMA read operation
 * @note After completion of the DMA operation, the DMA engine issues the done
 *  interrupt signaling end of operation
 */
void api_audiodmastartread(void);
/**
 * Stop the DMA read operation
 * @note After stopping of the DMA operation, the DMA engine issues the done
 *  interrupt signaling end of operation
 */
void api_audiodmastopread(void);
/**
 * Verify HDCP status
 * @return TRUE if HDCP is engaged
 */
int api_hdcpengaged(void);
/**
 * The engine goes through the authentication
 * statesAs defined in the HDCP specification.
 * @return a the state of the authentication machine
 * @note Used for debug purposes
 */
u8 api_hdcpauthenticationstate(void);
/**
 * The engine goes through several cipher states
 * @return a the state of the cipher machine
 * @note Used for debug purposes
 */
u8 api_hdcpcipherstate(void);
/**
 * The engine goes through revocation states
 * @return a the state of the revocation the engine is going through
 * @note Used for debug purposes
 */
u8 api_hdcprevocationstate(void);
/**
 * Bypass the HDCP encryption and transmit an non-encrypted content
 * (without ending authentication)
 * @param bypass Whether or not to bypass
 * @note Used for debug purposes
 */
int api_hdcpbypassencryption(int bypass);
/**
 * To disable the HDCP encryption and transmit an
 * non-encrypted content and lose authentication
 * @param disable Whether or not HDCP is to be disabled
 * @note Used for debug purposes
 */
int api_hdcpdisableencryption(int disable);

/**
 * Update HDCP SRM, list of maximum 5200 entries (version 1)
 * @param data array of the SRM file
 */
int api_hdcpsrmupdate(const u8 * data);

/**
 * Hot plug detect
 * @return TRUE when hot plug detected
 */
int api_hotplugdetected(void);
/**
 * Dump core configuration to output stream
 * return error code
 */
u8 api_readconfig(void);

int only_audio_configure(videoParams_t * video, audioParams_t * audio);

int api_deinitialize(void);

u8 hdmi_get_irq_status(void);
 unsigned int hdmi_clear_irq(u8 reg_val);

  int api_read_edid(void);

#endif /* API_H_ */
