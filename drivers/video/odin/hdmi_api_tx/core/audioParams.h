/*
 * audioParams.h
 *
 *  Created on: Jul 2, 2010
 *
 * Synopsys Inc.
 * SG DWC PT02
 */
/**
 * @file
 * Define audio parameters structure and functions to manipulate the
 * information held by the structure.
 * Audio interfaces, packet types and coding types are also defined here
 */
#ifndef AUDIOPARAMS_H_
#define AUDIOPARAMS_H_
#include "../util/types.h"

typedef enum
{
	I2S = 0,
	SPDIF,
	HBR,
	GPA,
	DMA
} interfaceType_t;

typedef enum
{
	AUDIO_SAMPLE = 1, HBR_STREAM
} packet_t;

typedef enum
{
	PCM = 1,
	AC3,
	MPEG1,
	MP3,
	MPEG2,
	AAC,
	DTS,
	ATRAC,
	ONE_BIT_AUDIO,
	DOLBY_DIGITAL_PLUS,
	DTS_HD,
	MAT,
	DST,
	WMAPRO
} codingType_t;

typedef enum
{
	DMA_4_BEAT_INCREMENT = 0,
	DMA_8_BEAT_INCREMENT,
	DMA_16_BEAT_INCREMENT,
	DMA_UNUSED_BEAT_INCREMENT,
	DMA_UNSPECIFIED_INCREMENT
} dmaIncrement_t;

/** For detailed handling of this structure, refer to documentation of the
functions */
typedef struct
{
	interfaceType_t minterfacetype;

	codingType_t mcodingtype; /** (audioParams_t *params, see InfoFrame) */

	u8 mchannelallocation; /** channel allocation (audioParams_t *params,
						   see InfoFrame) */

	u8 msamplesize; /**  sample size (audioParams_t *params, 16 to 24) */

	u32 msamplingfrequency; /** sampling frequency (audioParams_t *params, Hz) */

	u8 mlevelshiftvalue; /** level shift value (audioParams_t *params,
						 see InfoFrame) */

	u8 mdownmixinhibitflag; /** down-mix inhibit flag (audioParams_t *params,
							see InfoFrame) */

	u32 moriginalsamplingfrequency; /** Original sampling frequency */

	u8 mieccopyright; /** IEC copyright */

	u8 mieccgmsa; /** IEC CGMS-A */

	u8 miiecpcmmode; /** IEC PCM mode */

	u8 mieccategorycode; /** IEC category code */

	u8 miecsourcenumber; /** IEC source number */

	u8 miecclockaccuracy; /** IEC clock accuracy */

	packet_t mpackettype; /** packet type. currently only Audio Sample (AUDS)
						  and High Bit Rate (HBR) are supported */

	u16 mclockfsfactor; /** Input audio clock Fs factor used at the audop
						packetizer to calculate the CTS value and ACR packet
						insertion rate */

	dmaIncrement_t mdmabeatincrement; /** Incremental burst modes: unspecified
									lengths (upper limit is 1kB boundary) and
									INCR4, INCR8, and INCR16 fixed-beat burst */

	u8 mdmathreshold; /** When the number of samples in the Audio FIFO is lower
						than the threshold, the DMA engine requests a new burst
						request to the AHB master interface */

	u8 mdmahlock; /** Master burst lock mechanism */

	u8 mgpainsertpucv; /* discard incoming (Parity, Channel status, User bit,
					      Valid and B bit) data and insert data configured in
					      controller instead */
} audioParams_t;

/**
 * This method resets the parameters strucutre to a known state
 * SPDIF 16bits 32Khz Linear PCM
 * It is recommended to call this method before setting any parameters
 * to start from a stable and known condition avoid overflows.
 * @param params pointer to the audio parameters structure
 */
void audioparams_reset(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @return the Channel Allocation code from Table 20 in CEA-861-D
 */
u8 audioparams_getchannelallocation(audioParams_t *params);
/**
 * Set the audio channels' set up
 * @param params pointer to the audio parameters structure
 * @param value code of the channel allocation from Table 20 in CEA-861-D
 */
void audioparams_setchannelallocation(audioParams_t *params, u8 value);
/**
 * @param params pointer to the audio parameters structure
 * @return the audio coding type of audio stream
 */
u8 audioparams_getcodingtype(audioParams_t *params);
/** Audio coding type. Allowed values are
 *  PCM = 1, AC3 = 2, MPEG1 = 3, MP3 = 4, MPEG2 = 5, AAC = 6, DTS = 7,
 *  ATRAC = 8, ONE_BIT_AUDIO = 9, DOLBY_DIGITAL_PLUS = 10, DTS_HD = 11,
 *  MAT = 12, DST = 13, WMAPRO = 14. Check codingType_t structure
 * @param params pointer to the audio parameters structure
 * @param value the audio coding type of audio stream
 */
void audioparams_setcodingtype(audioParams_t *params, codingType_t value);
/**
 * @param params pointer to the audio parameters structure
 * @return 1 if the Down Mix Inhibit is allowed
 */
u8 audioparams_getdownmixinhibitflag(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @param value 1 if the Down Mix Inhibit is allowed, 0 otherwise
 */
void audioparams_setdownmixinhibitflag(audioParams_t *params, u8 value);
/**
 * Category code indicates the kind of equipment that generates the digital
 * audio interface signal.
 * @param params pointer to the audio parameters structure
 * @return the IEC audio category code
 * @note when trasnmitting L-PCM on I2S
 */
u8 audioparams_getieccategorycode(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @param value the IEC audio category code
 * @note when trasnmitting L-PCM on I2S
 */
void audioparams_setieccategorycode(audioParams_t *params, u8 value);
/**
 * IEC CGMS-A indicated the copying permission
 * @param params pointer to the audio parameters structure
 * @return the IEC CGMS-A code
 * @note when trasnmitting L-PCM on I2S
 */
u8 audioparams_getieccgmsa(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @param value the IEC CGMS-A code
 * @note when trasnmitting L-PCM on I2S
 */
void audioparams_setieccgmsa(audioParams_t *params, u8 value);
/**
 * @param params pointer to the audio parameters structure
 * @return the audio clock accuracy
 * @note when trasnmitting L-PCM on I2S
 */
u8 audioparams_getiecclockaccuracy(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @param value the audio clock accuracy
 * @note when trasnmitting L-PCM on I2S
 */
void audioparams_setiecclockaccuracy(audioParams_t *params, u8 value);
/**
 * @param params pointer to the audio parameters structure
 * @return 1 if IEC Copyright is enable
 * @note when trasnmitting L-PCM on I2S
 */
u8 audioparams_getieccopyright(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @param value 1 if IEC Copyright enable, 0 otherwise
 * @note when trasnmitting L-PCM on I2S
 */
void audioparams_setieccopyright(audioParams_t *params, u8 value);
/**
 * @param params pointer to the audio parameters structure
 * @return the IEC PCM audio mode (audioParams_t *params, pre-emphasis)
 * @note when trasnmitting L-PCM on I2S
 */
u8 audioparams_getiecpcmmode(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @param value the IEC PCM audio mode (audioParams_t *params, pre-emphasis)
 * @note when trasnmitting L-PCM on I2S
 */
void audioparams_setiecpcmmode(audioParams_t *params, u8 value);
/**
 * @param params pointer to the audio parameters structure
 * @return the IEC Source Number
 * @note when trasnmitting L-PCM on I2S
 */
u8 audioparams_getiecsourcenumber(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @param value the IEC Source Number
 * @note when trasnmitting L-PCM on I2S
 */
void audioparams_setiecsourcenumber(audioParams_t *params, u8 value);
/**
 * InterfaceType is an enumerate of the physical interfaces
 * of the HDMICTRL (audioParams_t *params, of which I2S is one)
 * @param params pointer to the audio parameters structure
 * @return the interface type to which the audio stream is inputted
 */
interfaceType_t audioparams_getinterfacetype(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @param value the interface type to which the audio stream is inputted
 */
void audioparams_setinterfacetype(audioParams_t *params, interfaceType_t value);
/**
 * Level shift value is the value to which the sound level
 * is attenuated after down-mixing channels and speaker audio
 * @param params pointer to the audio parameters structure
 * @return the value in dB
 */
u8 audioparams_getlevelshiftvalue(audioParams_t *params);

void audioparams_setlevelshiftvalue(audioParams_t *params, u8 value);
/**
 * Original sampling frequency field may be used to indicate the sampling
 * frequency of a signal prior to sampling frequency conversion in a consumer
 * playback system.
 * @param params pointer to the audio parameters structure
 * @return a 32-bit word original sampling frequency
 * @note when trasnmitting L-PCM on I2S
 */
u32 audioparams_getoriginalsamplingfrequency(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @param value a 32-bit word original sampling frequency
 * @note when trasnmitting L-PCM on I2S
 */
void audioparams_setoriginalsamplingfrequency(audioParams_t *params, u32 value);
/**
 * @param params pointer to the audio parameters structure
 * @return 8-bit sample size in bits
 */
u8 audioparams_getsamplesize(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @param value 8-bit sample size in bits
 */
void audioparams_setsamplesize(audioParams_t *params, u8 value);
/**
 * 192000 Hz| 176400 Hz| 96000 Hz| 88200 Hz| 48000 Hz| 44100 Hz| 32000 Hz
 * @param params pointer to the audio parameters structure
 * @return 32-bit word sampling frequency
 * @note when trasnmitting L-PCM on I2S
 */
u32 audioparams_getsamplingfrequency(audioParams_t *params);
/**
 * 192000 Hz| 176400 Hz| 96000 Hz| 88200 Hz| 48000 Hz| 44100 Hz| 32000 Hz
 * @param params pointer to the audio parameters structure
 * @param value 32-bit word sampling frequency
 * @note when trasnmitting L-PCM on I2S
 */
void audioparams_setsamplingfrequency(audioParams_t *params, u32 value);
/**
 * [Hz]
 * @param params pointer to the audio parameters structure
 * @return audio clock in Hz
 */
u32 audioparams_audioclock(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @return number of audio channels transmitted -1
 */
u8 audioparams_channelcount(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 */
u8 audioparams_iecoriginalsamplingfrequency(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 */
u8 audioparams_iecsamplingfrequency(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 */
u8 audioparams_iecwordlength(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @return 1 if it is linear PCM
 */
u8 audioparams_islinearpcm(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @return the type of audio packets received by controller
 */
packet_t audioparams_getpackettype(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @param packetType type of audio packets received by the controller
 * obselete for SPDIF
 */
void audioparams_setpackettype(audioParams_t *params, packet_t packetType);
/**
 * return if channel is enabled or not using the user's channel allocation
 * code
 * @param params pointer to the audio parameters structure
 * @param channel in question -1
 * @return 1 if channel is to be enabled, 0 otherwise
 */
u8 audioparams_ischannelen(audioParams_t *params, u8 channel);
/**
 * @param params pointer to the audio parameters structure
 * @return the input audio clock Fs factor
 */
u16 audioparams_getclockfsfactor(audioParams_t *params);
/**
 * @param params pointer to the audio parameters structure
 * @param clockFsFactor the input aidio clock Fs factor
 * this should be 512 for HBR
 * 128 or 512 for SPDIF
 * 64/128/256/512/1024 for I2S
 * obselete for GPA
 */
void audioparams_setclockfsfactor(audioParams_t *params, u16 clockFsFactor);

/**
 * @param params pointer to the audio parameters structure
 * @return the Dma Beat Increment
 */
dmaIncrement_t audioparams_getdmabeatincrement(audioParams_t *params);
/**
 * Incremental burst modes: unspecified lengths (upper limit is 1kB boundary)
 * and INCR4, INCR8, and INCR16 fixed-beat bursts
 * @param params pointer to the audio parameters structure
 * @param value of the DMA beat increment
 */
void audioparams_setdmabeatincrement(
		audioParams_t *params, dmaIncrement_t value);
/**
 * If a fixed DMA beat increment is selected, FIFO?™s threshold must be
 * correctly configured such that the last of the consecutive INCRx
 * (with x = 4, 8, or 16) bursts correctly fill the FIFO at last burst
 * received.
 * @note there is a re-alignment at the 1k boundary
 * @param params pointer to the audio parameters structure
 * @param threshold 8-bit value
 */
void audioparams_setdmathreshold(audioParams_t *params, u8 threshold);
/**
 * @param params pointer to the audio parameters structure
 * @return DMA threshold 8bit value
 */
u8 audioparams_getdmathreshold(audioParams_t *params);
/**
 * Get status of burst AHB request lock mechanism.
 * @param params pointer to the audio parameters structure
 * @return TRUE if enabled
 */
u8 audioparams_getdmahlock(audioParams_t *params);
/**
 * Enable request of locked burst AHB mechanism.
 * @param params pointer to the audio parameters structure
 * @param enable status
 * @note it is recommended to keep this to 0 as it may jeopardise the
 * performance of a real-time system
 */
void audioparams_setdmahlock(audioParams_t *params, int enable);
/**
 * Configure the DWC_hdmi_tx controller to insert the PCUV
 * (Parity, Channel status, User bit, Valid and B bit) data onto the outgoing
 *  audio packets.
 * @param params pointer to the audio parameters structure
 * @param internal When active (TRUE), any input data is ignored and the parity
 *  and B pulse bits are generated in run time, while the Channel status,
 *  User bit and Valid bit are retrieved from the information given
 *  in the Channel Allocation. See audioparams_setchannelallocation
 */
void audioparams_setgpasamplepacketinfosource(
			audioParams_t *params, int internal);
/**
 * Get current status of DWC_hdmi_tx controller configuration whether to insert
 *  the PCUV (Parity, Channel status, User bit, Valid and B bit) data onto the
 *  outgoing audio packets or get them from the external source.
 * @param params pointer to the audio parameters structure
 * @return TRUE When input data is ignored and the parity and B pulse bits are
 *  generated in run time, while the Channel status, User bit and Valid bit are
 *  retrieved from the information given in the Channel Allocation parameters,
 *  FALSE otherwise
 */
int audioparams_getgpasamplepacketinfosource(audioParams_t *params);

#endif /* AUDIOPARAMS_H_ */
