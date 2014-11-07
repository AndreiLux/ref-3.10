/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*Data Register 0 */

/* Command ID Type( 3 bits ) */
#define REQUEST			0x1
#define RESPONSE		0x2
#define INDICATION		0x3
#define NOTIFICATION	0x4

/* COMMAND (Task Action) for REQ/RES */
#define READY		0x1
#define CREATE		0x2
#define OPEN		0x3
#define CLOSE		0x4
#define DELETE		0x5
#define WAITI		0x8
#define CONTROL_INIT		0x10
#define CONTROL_PLAY_ENTIRE		0x11
#define CONTROL_PLAY_PARTIAL		0x12
#define CONTROL_PLAY_PARTIAL_END	0x13
#define CONTROL_STOP		0x14
#define CONTROL_PAUSE		0x15
#define CONTROL_RESUME		0x16
#define CONTROL_VOLUME		0x1A
#define CONTROL_MUTE		0x1B
#define CONTROL_UNMUTE		0x1C
#define CONTROL_ALARM		0x20
#define POST		0x21
#define VR		0x22
#define CONTROL_GET_TIMESTAMP	0x50

#define CAPTURE				0x60
#define PLAYBACK			0x61



/* COMMAND (Task Action) for NTF */
#define NEAR_CONSUMED		0x1
#define CAPTURE_NTF			0x2
#define PLAYBACK_NTF		0x3

#if 0
#define CONTROL_SEEK	0x16
#define CONTROL_GET_TIMESTAMP	0x17
#define CONTROL_NEARSTOP		0xE
#define CONTROL_SELFSTOP		0xF
#define CONTROL_GET_AUDIO_INFO	0x23
#endif


/* Data Register 1 */

/* Flow Type (4 bits ) */
#define DECODE	0x1
#define ENCODE	0x2
#define PLAYBACK_CAPTURE 0xC


/* Operation mode ( 8 bits ) */
#if 0
#define LPA	0x20			/* 0010 0000 */
#define OMX	0x10			/* 0001 0000 */
#define VR	0x8				/* 0000 1000 */
#define PRE_PROCESSING	0x4	/* 0000 0100 */
#define POST_PROCESSING	0x2	/* 0000 0010 */
#endif
#define LPA	0x1		
#define OMX	0x2		
#define ALSA 0x8000000


/* Algorithm Type (4 bits) */
#define WAV				0x1
#define MP3				0x2
#define AAC				0x3
#define AAC_PLUS		0x4
#define E_AAC_PLUS		0x5
#define WMA10_PRO		0x6
#define DM3_PULS		0x7
#define DTS				0x8

/* Data Register 2 */

/* Sampling Frequency (20 bits) */
#define KHZ_4		4000
#define KHZ_8		8000
#define KHZ_11025	11025
#define KHZ_12		12000
#define KHZ_16		16000
#define KHZ_2205	22050
#define KHZ_24		24000
#define KHZ_32		32000
#define KHZ_441		44100
#define KHZ_48		48000
#define KHZ_64		64000
#define KHZ_882		88200
#define KHZ_96		96000
#define KHZ_128		128000
#define KHZ_176		176000
#define KHZ_192		192000
#define KHZ_768		768000

/* Bit rate (3 bits) */
#define KBPS_48		48
#define KBPS_64		64
#define KBPS_96		96
#define KBPS_128	128
#define KBPS_196	196
#define KBPS_320	320
#define KBPS_384	384
#define KBPS_1024	1024
#define KBPS_1411	1411


/* Data Register 3 */

/* Channel Number ( 4 bits) */

#define CHANNEL_1	0x1
#define CHANNEL_2	0x2
#define CHANNEL_3	0x3
#define CHANNEL_4	0x4
#define CHANNEL_5	0x5
#define CHANNEL_51	0x6
#define CHANNEL_61	0x7

/* bit per sample ( 4 bits) */

#define BIT_8	8
#define BIT_10	10
#define BIT_12	12
#define BIT_16	16
#define BIT_24	24
#define BIT_32	32
#define BIT_64	64


/* Data Register 3 */

#define START 0x1
#define STOP 0x2


/* Sender&Receiver Device ID (4 bits + 4 bits) */

#define ODIN_ARM 0x0
#define ODIN_DSP 0x1
#define ODIN_SMS 0x2




/* Resourece ID (8 bits) */

#define AUDIO			0x0
#define VOICE			0x1
#define SPEECH_RECOGNITION			0x2
#define IMAGE			0x3
#define VIDEO			0x4

#if 0
/* Action for Audio Manager (4 bits ) */
#define OPEN	0x1
#define INIT	0x2
#define EXECUTE	0x3
#define STOP	0x4
#define PAUSE	0x5
#define RESUME	0x6
#endif


/* Action for CDAIF */
#define CONFIG		0x1
#define CPB1_ACTIVE	0x2
#define CPB2_ACTIVE	0x3
#define DPB1_ACTIVE	0x4
#define DPB2_ACTIVE	0x5

/*Data Register 4 */

/* Paramter MAILBOX 필요 유무 */
#define PARAM_UNSET		0x0
#define PARAM_SET		0x1


/* Define Mailbox channel */
#define CH0_REQ_RES	0
#define CH1_IND		1
#define CH2_NTF		2
#define CH7_NTF		7
#define CH25_LOG	25
#define CH26_DUMP	26
#define CH27_ERR	27
#define CH28_TRC	28
#define CH29_DBG	29



/* For OMX */

/* Task group ( 4 bits ) */
#define OMX_ENCODE	0x1
#define OMX_DECODE	0x2

/* Action for Audio Manager (4 bits ) */
#define OMX_OPEN	0x1
#define OMX_INIT	0x2
#define OMX_EXECUTE	0x3
#define OMX_STOP	0x4
#define OMX_PAUSE	0x5
#define OMX_RESUME	0x6

/* Sender&Receiver Device ID (4 bits + 4 bits) */
#define OMX_ARM 0x1
#define OMX_DSP 0x2
#define OMX_SMS 0x3


/* Sender&Receiver Object ID (8 bits + 8 bits) */
#define DSP_DRIVER		0x0 /* For ARM */
#define AUDIO_MANAGER	0x0	/* For DSP */
#define CDAIF			0x1 /* DSP     */


#define DOLBY_DIGITAL	0x2
#define WMA		0x3
#define PCM		0x5

#define DEFAULT			0x0
