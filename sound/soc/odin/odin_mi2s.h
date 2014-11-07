/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This file is based on code in sound/soc/dwc/designware_i2s.c
 * and include/sound/designware_i2s.h
 *
 * Copyright (C) 2010 ST Microelectronics
 * Rajeev Kumar <rajeev-dlh.kumar@st.com>
 *
 * designware_i2s.c and designware_i2s.h is distributed under GPL 2.0
 * See designware_i2s.c and designware_i2s.h for further copyright information.
 *
 */

#ifndef __SOUND_ODIN_MULTI_I2S_H
#define __SOUND_ODIN_MULTI_I2S_H

/* common register for all channel */
#define IER		0x000
#define IRER		0x004
#define ITER		0x008
#define CER		0x00C
#define CCR		0x010
#define RXFFR		0x014
#define TXFFR		0x018

#define MSSETR		0x140

#define NCOER		0x180
#define NCOPR		0x184

#define RXDMA		0x1C0
#define RRXDMA		0x1C4
#define TXDMA		0x1C8
#define RTXDMA		0x1CC

#define DMAER		0x1D0
#define DMACR		0x1D4
#define DMATXLR		0x1D8
#define DMARXLR		0x1DC

/* I2STxRxRegisters for all channels */
#define LRBR_LTHR(x)	(0x40 * x + 0x020)
#define RRBR_RTHR(x)	(0x40 * x + 0x024)
#define RER(x)		(0x40 * x + 0x028)
#define TER(x)		(0x40 * x + 0x02C)
#define RCR(x)		(0x40 * x + 0x030)
#define TCR(x)		(0x40 * x + 0x034)
#define ISR(x)		(0x40 * x + 0x038)
#define IMR(x)		(0x40 * x + 0x03C)
#define ROR(x)		(0x40 * x + 0x040)
#define TOR(x)		(0x40 * x + 0x044)
#define RFCR(x)		(0x40 * x + 0x048)
#define TFCR(x)		(0x40 * x + 0x04C)
#define RFF(x)		(0x40 * x + 0x050)
#define TFF(x)		(0x40 * x + 0x054)

/* I2SCOMPRegisters */
#define I2S_COMP_PARAM_2	0x01F0
#define I2S_COMP_PARAM_1	0x01F4
#define I2S_COMP_VERSION	0x01F8
#define I2S_COMP_TYPE		0x01FC

#define MAX_CHANNEL_NUM		8
#define MIN_CHANNEL_NUM		2

#define TWO_CHANNEL_SUPPORT	2	/* up to 2.0 */
#define FOUR_CHANNEL_SUPPORT	4	/* up to 3.1 */
#define SIX_CHANNEL_SUPPORT	6	/* up to 5.1 */
#define EIGHT_CHANNEL_SUPPORT	8	/* up to 7.1 */

#endif /*  __SOUND_ODIN_MULTI_I2S_H */
