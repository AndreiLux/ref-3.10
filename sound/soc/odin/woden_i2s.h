/*
 * woden_i2s.h - Definitions for WODEN I2S driver
 *
 * Copyright (c) 2012, LGE Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __WODEN_I2S_H__
#define __WODEN_I2S_H__


/* common register for all channel */
#define WODEN_I2S_GLOBAL_EN			0x000
#define WODEN_I2S_RECV_BLOCK_EN		0x004
#define WODEN_I2S_TRANS_BLOCK_EN	0x008
#define WODEN_I2S_CLOCK_EN			0x00C
#define WODEN_I2S_CLOCK_CONFIG		0x010
#define WODEN_I2S_RX_FIFO_RESET		0x014
#define WODEN_I2S_TX_FIFO_RESET		0x018

/* I2STxRxRegisters for all channels */
#define WODEN_I2S_LEFT_BUFF(x)			(0x40 * x + 0x020)
#define WODEN_I2S_RIGHT_BUFF(x)			(0x40 * x + 0x024)
#define WODEN_I2S_RX_EN(x)				(0x40 * x + 0x028)
#define WODEN_I2S_TX_EN(x)				(0x40 * x + 0x02C)
#define WODEN_I2S_RX_CONFIG(x)			(0x40 * x + 0x030)
#define WODEN_I2S_TX_CONFIG(x)			(0x40 * x + 0x034)
#define WODEN_I2S_INT_STATUS(x)			(0x40 * x + 0x038)
#define WODEN_I2S_INT_MASK(x)			(0x40 * x + 0x03C)
#define WODEN_I2S_RX_OVERRUN_STATUS(x)	(0x40 * x + 0x040)
#define WODEN_I2S_TX_OVERRUN_STATUS(x)	(0x40 * x + 0x044)
#define WODEN_I2S_RX_FIFO_CONFIG(x)		(0x40 * x + 0x048)
#define WODEN_I2S_TX_FIFO_CONFIG(x)		(0x40 * x + 0x04C)
#define WODEN_I2S_RX_FLUSH(x)			(0x40 * x + 0x050)
#define WODEN_I2S_TX_FLUSH(x)			(0x40 * x + 0x054)

/* common register for all channel */
#define IER			0x000
#define IRER		0x004
#define ITER		0x008
#define CER			0x00C
#define CCR			0x010
#define RXFFR		0x014
#define TXFFR		0x018

/* I2STxRxRegisters for all channels */
#define LRBR_LTHR(x)	(0x40 * x + 0x020)
#define RRBR_RTHR(x)	(0x40 * x + 0x024)
#define RER(x)			(0x40 * x + 0x028)
#define TER(x)			(0x40 * x + 0x02C)
#define RCR(x)			(0x40 * x + 0x030)
#define TCR(x)			(0x40 * x + 0x034)
#define ISR(x)			(0x40 * x + 0x038)
#define IMR(x)			(0x40 * x + 0x03C)
#define ROR(x)			(0x40 * x + 0x040)
#define TOR(x)			(0x40 * x + 0x044)
#define RFCR(x)			(0x40 * x + 0x048)
#define TFCR(x)			(0x40 * x + 0x04C)
#define RFF(x)			(0x40 * x + 0x050)
#define TFF(x)			(0x40 * x + 0x054)

#define WODEN_I2S_SELECT_MODE			0x140

#define WODEN_I2S_NCO_SELECT			0x180
#define WODEN_I2S_NCO_PARAM				0x184

#define WODEN_I2S_RESET_RECV_BLOCK_DMA	0x1C4
#define WODEN_I2S_RESET_TRANS_BLOCK_DMA	0x1CC

#define WODEN_I2S_DMA_EN				0x1D0
#define WODEN_I2S_DMA_CLEAR				0x1D4
#define WODEN_I2S_DMA_TX_REQ			0x1D8
#define WODEN_I2S_DMA_RX_REQ			0x1DC

/* Format and channnel NUM */
#define WODEN_I2S_FORMAT	SNDRV_PCM_FMTBIT_S16_LE
#define MAX_CHANNEL_NUM			8
#define MIN_CHANNEL_NUM			2

#if 0
/* Register offsets from WODEN_I2S_BASE */
#define WODEN_I2S_GLOBAL_EN				0x0
#define WODEN_I2S_RECV_BLOCK_EN			0x4
#define WODEN_I2S_TRANS_BLOCK_EN		0x8
#define WODEN_I2S_CLOCK_EN				0xC
#define WODEN_I2S_CLOCK_CONFIG			0x10
#define WODEN_I2S_RX_FIFO_RESET			0x14
#define WODEN_I2S_TX_FIFO_RESET			0x18
#define WODEN_I2S_LEFT_BUFF_0			0x20
#define WODEN_I2S_RIGHT_BUFF_0			0x24
#define WODEN_I2S_RX_EN_0				0x28
#define WODEN_I2S_TX_EN_0				0x2C
#define WODEN_I2S_RX_CONFIG_0			0x30
#define WODEN_I2S_TX_CONFIG_0			0x34
#define WODEN_I2S_INT_STATUS_0			0x38
#define WODEN_I2S_INT_MASK_0			0x3C
#define WODEN_I2S_RX_OVERRUN_STATUS_0	0x40
#define WODEN_I2S_TX_OVERRUN_STATUS_0	0x44
#define WODEN_I2S_RX_FIFO_CONFIG_0		0x48
#define WODEN_I2S_TX_FIFO_CONFIG_0		0x4C
#define WODEN_I2S_RX_FLUSH_0			0x50
#define WODEN_I2S_TX_FLUSH_0			0x54

#define WODEN_I2S_LEFT_BUFF_1			0x60
#define WODEN_I2S_RIGHT_BUFF_1			0x64
#define WODEN_I2S_RX_EN_1				0x68
#define WODEN_I2S_TX_EN_1				0x6C
#define WODEN_I2S_RX_CONFIG_1			0x70
#define WODEN_I2S_TX_CONFIG_1			0x74
#define WODEN_I2S_INT_STATUS_1			0x78
#define WODEN_I2S_INT_MASK_1			0x7C
#define WODEN_I2S_RX_OVERRUN_STATUS_1	0x80
#define WODEN_I2S_TX_OVERRUN_STATUS_1	0x84
#define WODEN_I2S_RX_FIFO_CONFIG_1		0x88
#define WODEN_I2S_TX_FIFO_CONFIG_1		0x8C
#define WODEN_I2S_RX_FLUSH_1			0x90
#define WODEN_I2S_TX_FLUSH_1			0x94

#define WODEN_I2S_LEFT_BUFF_2			0xA0
#define WODEN_I2S_RIGHT_BUFF_2			0xA4
#define WODEN_I2S_RX_EN_2				0xA8
#define WODEN_I2S_TX_EN_2				0xAC
#define WODEN_I2S_RX_CONFIG_2			0xB0
#define WODEN_I2S_TX_CONFIG_2			0xB4
#define WODEN_I2S_INT_STATUS_2			0xB8
#define WODEN_I2S_INT_MASK_2			0xBC
#define WODEN_I2S_RX_OVERRUN_STATUS_2	0xC0
#define WODEN_I2S_TX_OVERRUN_STATUS_2	0xC4
#define WODEN_I2S_RX_FIFO_CONFIG_2		0xC8
#define WODEN_I2S_TX_FIFO_CONFIG_2		0xCC
#define WODEN_I2S_RX_FLUSH_2			0xD0
#define WODEN_I2S_TX_FLUSH_2			0xD4

#define WODEN_I2S_LEFT_BUFF_3			0xE0
#define WODEN_I2S_RIGHT_BUFF_3			0xE4
#define WODEN_I2S_RX_EN_3				0xE8
#define WODEN_I2S_TX_EN_3				0xEC
#define WODEN_I2S_RX_CONFIG_3			0xF0
#define WODEN_I2S_TX_CONFIG_3			0xF4
#define WODEN_I2S_INT_STATUS_3			0xF8
#define WODEN_I2S_INT_MASK_3			0xFC
#define WODEN_I2S_RX_OVERRUN_STATUS_3	0x100
#define WODEN_I2S_TX_OVERRUN_STATUS_3	0x104
#define WODEN_I2S_RX_FIFO_CONFIG_3		0x108
#define WODEN_I2S_TX_FIFO_CONFIG_3		0x10C
#define WODEN_I2S_RX_FLUSH_3			0x110
#define WODEN_I2S_TX_FLUSH_3			0x114

#define WODEN_I2S_SELECT_MODE			0x140

#define WODEN_I2S_NCO_SELECT			0x180
#define WODEN_I2S_NCO_PARAM				0x184

#define WODEN_I2S_RESET_RECV_BLOCK_DMA	0x1C4
#define WODEN_I2S_RESET_TRANS_BLOCK_DMA	0x1CC

#define WODEN_I2S_DMA_EN				0x1D0
#define WODEN_I2S_DMA_CLEAR				0x1D4
#define WODEN_I2S_DMA_TX_REQ			0x1D8
#define WODEN_I2S_DMA_RX_REQ			0x1DC
#endif
typedef struct
{
	unsigned int enable : 1; // 0
	unsigned int reserved : 31;
}GLOBAL_ENABLE;

typedef struct
{
	unsigned int rxEnable : 1;
	unsigned int reserved : 31;
}RECEIVER_BLOCK_ENABLE;

typedef struct
{
	unsigned int txEnable : 1;
	unsigned int reserved : 31;
}TRANSMITTER_BLOCK_ENABLE;

typedef struct
{
	unsigned int clockEnable : 1;
	unsigned int reserved : 31;
}CLOCK_ENABLE;

typedef struct
{
	unsigned int reserved0 : 3;		// 2:0 gatingOfSclk : No Function
	unsigned int wordSelect : 2;		// 3:4
	unsigned int reserved : 27;
}CLOCK_CONFIGURATION;

typedef struct
{
	unsigned int receiverFifoReset : 1;
	unsigned int reserved : 31;
}RECEIVER_FIFO_RESET;

typedef struct
{
	unsigned int transmitterFifoReset : 1;
	unsigned int reserved : 31;
}TRANSMITTER_FIFO_RESET;

typedef struct
{
	unsigned int receiveEnable : 1;
	unsigned int reserved : 31;
}RECEIVE_ENABLE;

typedef struct
{
	unsigned int transmitEnable : 1;
	unsigned int reserved : 31;
}TRANSMIT_ENABLE;

typedef struct
{
	unsigned int rxWordLength : 3;
	unsigned int reserved : 29;
}RECEIVE_CONFIGURATION;

typedef struct
{
	unsigned int txWordLength : 3;
	unsigned int reserved : 29;
}TRANSMIT_CONFIGURATION;

typedef struct
{
	unsigned int rxDataAvailable : 1;
	unsigned int rxFifoWriteOverrun : 1;
	unsigned int reserved : 2;
	unsigned int txEmptyTrigger : 1;
	unsigned int txFifoWriteOverrun : 1;
	unsigned int reserved1 : 26;
}INTERRUPT_STATUS;

typedef struct
{
	unsigned int rxFifowriteStatus : 1;
	unsigned int reserved : 31;
}RECEIVE_OVERRUN;

typedef struct
{
	unsigned int txFifowriteStatus : 1;
	unsigned int reserved : 31;
}TRANSMIT_OVERRUN;

typedef struct
{
	unsigned int rxchdt : 4;
	unsigned int reserved : 28;
}RECEIVE_FIFO_CONFIGURATION;

typedef struct
{
	unsigned int txchet : 4;
	unsigned int reserved : 28;
}TRANSMIT_FIFO_CONFIGURATION;

typedef struct
{
	unsigned int rxFifoFlush : 1;
	unsigned int reserved : 31;
}RECEIVE_FIFO_FLUSH;

typedef struct
{
	unsigned int txFifoFlush : 1;
	unsigned int reserved : 31;
}TRANSMIT_FIFO_FLUSH;

typedef struct
{
	unsigned int masterSlaveSel : 1;
	unsigned int reserved : 31;
}MASTER_SLAVE_SELECT;

typedef struct
{
	unsigned int ncoEnable : 1;
	unsigned int reserved : 31;
}NCO_ENABLE;

typedef struct
{
	unsigned int ncoValue : 20;
	unsigned int reserved : 12;
}NCO_VALUE;

typedef struct
{
	unsigned int dmaEnable : 1;
	unsigned int reserved : 31;
}DMA_ENABLE;

typedef struct
{
	unsigned int dmaClear : 1;
	unsigned int reserved : 31;
}DMA_CLEAR;

typedef struct
{
	unsigned int dmaTxLevel : 4;
	unsigned int reserved : 28;
}DMA_TX_LEVEL;

typedef struct
{
	unsigned int dmaRxLevel : 4;
	unsigned int reserved : 28;
}DMA_RX_LEVEL;

typedef struct
{
	GLOBAL_ENABLE globalEnable;	// 000 h
	RECEIVER_BLOCK_ENABLE receiverBlockEnable;	// 004h
	TRANSMITTER_BLOCK_ENABLE transmitterBlockEnable;	// 008h
	CLOCK_ENABLE clockEnable;	// 00Ch
	CLOCK_CONFIGURATION clockConfiguration;	// 010h
	RECEIVER_FIFO_RESET rxFiforeset;	// 014h
	TRANSMITTER_FIFO_RESET txFiforeset;	// 018h
	unsigned int reserved;	// 01Ch

	unsigned int leftBuffer_0;	// 020h
	unsigned int rightBuffer_0;	// 024h
	RECEIVE_ENABLE rxEnable_0;	// 028h
	TRANSMIT_ENABLE txEnable_0;	// 02Ch
	RECEIVE_CONFIGURATION rxConfig_0;		// 030h
	TRANSMIT_CONFIGURATION txConfig_0;	// 034h
	INTERRUPT_STATUS intStatus_0;		// 038h
	INTERRUPT_STATUS intMask_0;		// 03Ch
	RECEIVE_OVERRUN rxOverrunStatus_0;	// 040h
	TRANSMIT_OVERRUN txOverrunStatus_0;	// 044h
	RECEIVE_FIFO_CONFIGURATION rxFifoConfig_0;	// 048h
	TRANSMIT_FIFO_CONFIGURATION txFifoConfig_0;	// 04Ch
	RECEIVE_FIFO_FLUSH rxFlush_0;	// 050h
	TRANSMIT_FIFO_FLUSH txFlush_0; 	// 054h
	unsigned int reserved1[2];		// 058h, 05Ch

	unsigned int leftBuffer_1;	// 060h
	unsigned int rightBuffer_1;	// 064h
	RECEIVE_ENABLE rxEnable_1;	// 068h
	TRANSMIT_ENABLE txEnable_1;	// 06Ch
	RECEIVE_CONFIGURATION rxConfig_1;		// 070h
	TRANSMIT_CONFIGURATION txConfig_1;	// 074h
	INTERRUPT_STATUS intStatus_1;		// 078h
	INTERRUPT_STATUS intMask_1;		// 07Ch
	RECEIVE_OVERRUN rxOverrunStatus_1;	// 080h
	TRANSMIT_OVERRUN txOverrunStatus_1;	// 084h
	RECEIVE_FIFO_CONFIGURATION rxFifoConfig_1;	// 088h
	TRANSMIT_FIFO_CONFIGURATION txFifoConfig_1;	// 08Ch
	RECEIVE_FIFO_FLUSH rxFlush_1;	// 090h
	TRANSMIT_FIFO_FLUSH txFlush_1; 	// 094h
	unsigned int reserved2[2];		// 098h, 09Ch

	unsigned int leftBuffer_2;	// 0A0h
	unsigned int rightBuffer_2;	// 0A4h
	RECEIVE_ENABLE rxEnable_2;	// 0A8h
	TRANSMIT_ENABLE txEnable_2;	// 0ACh
	RECEIVE_CONFIGURATION rxConfig_2;		// 0B0h
	TRANSMIT_CONFIGURATION txConfig_2;	// 0B4h
	INTERRUPT_STATUS intStatus_2;		// 0B8h
	INTERRUPT_STATUS intMask_2;		// 0BCh
	RECEIVE_OVERRUN rxOverrunStatus_2;	// 0C0h
	TRANSMIT_OVERRUN txOverrunStatus_2;	// 0C4h
	RECEIVE_FIFO_CONFIGURATION rxFifoConfig_2;	// 0C8h
	TRANSMIT_FIFO_CONFIGURATION txFifoConfig_2;	// 0CCh
	RECEIVE_FIFO_FLUSH rxFlush_2;	// 0D0h
	TRANSMIT_FIFO_FLUSH txFlush_2; 	// 0D4h
	unsigned int reserved3[2];		// 0D8h, 0DCh

	unsigned int leftBuffer_3;	// 0E0h
	unsigned int rightBuffer_3;	// 0E4h
	RECEIVE_ENABLE rxEnable_3;	// 0E8h
	TRANSMIT_ENABLE txEnable_3;	// 0ECh
	RECEIVE_CONFIGURATION rxConfig_3;		// 0F0h
	TRANSMIT_CONFIGURATION txConfig_3;	// 0F4h
	INTERRUPT_STATUS intStatus_3;		// 0F8h
	INTERRUPT_STATUS intMask_3;		// 0FCh
	RECEIVE_OVERRUN rxOverrunStatus_3;	// 100h
	TRANSMIT_OVERRUN txOverrunStatus_3;	// 104h
	RECEIVE_FIFO_CONFIGURATION rxFifoConfig_3;	// 108h
	TRANSMIT_FIFO_CONFIGURATION txFifoConfig_3;	// 10Ch
	RECEIVE_FIFO_FLUSH rxFlush_3;	// 110h
	TRANSMIT_FIFO_FLUSH txFlush_3; 	// 114h
	unsigned int reserved4[10];	// 118h - 13Ch

	MASTER_SLAVE_SELECT selectMode;	// 140h
	unsigned int reserved5[15] ; // 144h - 17Ch

	NCO_ENABLE ncoSelect;	// 180h
	NCO_VALUE ncoParam;	// 184h
	unsigned int reserved6[15];	// 188h -1CCh

	unsigned int resetReceiverBlockDma;
	unsigned int reserved7;
	unsigned int resetTransmmiterBlockDma;

	DMA_ENABLE dmaEn;	// 1D0h
	DMA_CLEAR dmaClear;	// 1D4h
	DMA_TX_LEVEL dmaTxRequest;		// 1D8h
	DMA_RX_LEVEL dmaRxRequest;	// 1DCh
}I2S_REGISTERS;

/* Number of i2s controllers*/
#define WODEN_NR_I2S_IFC				5
#define WODEN_NR_REG_CACHE				30	// This should be modified

#include "woden_pcm.h"

/*
 * struct i2s_clk_config_data - represent i2s clk configuration data
 * @chan_nr: number of channel
 * @data_width: number of bits per sample (8/16/24/32 bit)
 * @sample_rate: sampling frequency (8Khz, 16Khz, 32Khz, 44Khz, 48Khz)
 */
struct i2s_clk_config_data {
    int chan_nr;
    u32 data_width;
    u32 sample_rate;
};

struct woden_i2s {
	int id;
	struct clk *clk_i2s;
	struct clk *clk_i2s_sync;
	struct clk *clk_audio_2x;
	struct clk *clk_pll_a_out0;

	void __iomem *base;
	void __iomem *regs;

	struct dentry *debug;
	int  playback_ref_count;
#ifdef CONFIG_PM
	u32  reg_cache[WODEN_NR_REG_CACHE];
#endif
	int active;

    /* data related to DMA transfers b/w i2s and DMAC */
    u8 swidth;
    u8 ccr;
    u8 xfer_resolution;

	int max_channel;

    struct i2s_clk_config_data config;


    struct woden_pcm_dma_params play_dma_data;
    struct woden_pcm_dma_params capture_dma_data;
};

#define TWO_CHANNEL_SUPPORT    2    /* up to 2.0 */
#define FOUR_CHANNEL_SUPPORT    4    /* up to 3.1 */
#define SIX_CHANNEL_SUPPORT    6    /* up to 5.1 */
#define EIGHT_CHANNEL_SUPPORT    8    /* up to 7.1 */





/* Below code was used verification code */
typedef long I2S_RESULT;

#define I2S_SUCCESS		(0) //No error
#define I2S_ERROR		(-1)

typedef enum
{
	MULTICHANNEL = 0x0,
	SINGLECHANNEL0 = 0x1,
	SINGLECHANNEL1 = 0x2,
	SINGLECHANNEL2 = 0x3,
	SINGLECHANNEL3 = 0x4,
	SINGLECHANNEL4 = 0x5,
	NONECHANNEL = 0xf
}DRV_I2S_CHANNEL_MODE;

typedef enum
{
	IGNORE_WORD = 0x0,
	RESOLUTION_12BIT = 0x1,
	RESOLUTION_16BIT = 0x2,
	RESOLUTION_20BIT = 0x3,
	RESOLUTION_24BIT = 0x4,
	RESOLUTION_32BIT = 0x5
}DRV_I2S_WORD_LENGTH;

typedef enum
{
	CYCLES_16CLOCK = 0x0,
	CYCLES_24CLOCK = 0x1,
	CYCLES_32CLOCK = 0x2
}DRV_I2S_WORD_CYCLES;

typedef enum
{
	FIFOLEVELCHECK0 = 0,
	FIFOLEVELCHECK1 = 1,
	FIFOLEVELCHECK2 = 2,
	FIFOLEVELCHECK3 = 3,
	FIFOLEVELCHECK4 = 4,
	FIFOLEVELCHECK5 = 5,
	FIFOLEVELCHECK6 = 6,
	FIFOLEVELCHECK7 = 7,
	FIFOLEVELCHECK8 = 8,
	FIFOLEVELCHECK9 = 9,
	FIFOLEVELCHECK10 = 0xA,
	FIFOLEVELCHECK11 = 0xB,
	FIFOLEVELCHECK12 = 0xC,
	FIFOLEVELCHECK13 = 0xD,
	FIFOLEVELCHECK14 = 0xE,
	FIFOLEVELCHECK15 = 0xF
}DRV_I2S_FIFO_CHECK_LEVEL;

typedef enum
{
	HOST_SLAVE = 0x0,
	HOST_MASTER = 0x1
}DRV_I2S_HOST_MODE;
typedef enum
{
    S8000HZ  	= 0,
   S11025HZ  	= 1,
   S12000HZ  	= 2,
   S16000HZ  	= 3,
   S22050HZ  	= 4,
   S24000HZ  	= 5,
   S32000HZ  	= 6,
   S44100HZ  	= 7,
   S48000HZ  	= 8
}AUD_SAMPLE_RATE;

typedef enum
{
	AUD_MONO = 0, // single
	AUD_STEREO = 1, // L/R 1Ch
	AUD_2STEREO = 2, // L/R 2Ch
	AUD_3STEREO = 3, // L/R 3Ch
	AUD_4STEREO = 4 // L/R 4Ch
}AUD_PLAYBACK_MODE;

typedef enum
{
	VIA_APB = 0,
	VIA_DMA = 1
}DEBUG_DATAPUSH;

typedef enum
{
	AUD_OUTPUT = 0,
	AUD_INPUT = 1
}AUD_TRANSACTION_MODE;

typedef struct
{
	DRV_I2S_CHANNEL_MODE baseChSelect;
	DRV_I2S_WORD_LENGTH wordLength;
	DRV_I2S_WORD_CYCLES wordCycles;
	DRV_I2S_FIFO_CHECK_LEVEL fifoCheckLevel;
	DRV_I2S_HOST_MODE hostMode;
	AUD_SAMPLE_RATE sampleRate;
	AUD_PLAYBACK_MODE playbackMode;
	DEBUG_DATAPUSH datapushMode;
	AUD_TRANSACTION_MODE transactionMode;
}DRV_I2S_PARAM;

#endif
