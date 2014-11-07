/*
 *  arch/arm/mach-odin/include/mach/regs-uart.h
 *
 *  Copyright (C) 2010 MtekVision Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ASM_ARCH_REGS_UART_H
#define __ASM_ARCH_REGS_UART_H

#define UART_CFG_FIFO_SIZE_8BITS	0
#define UART_CFG_FIFO_SIZE_16BITS	1
#define UART_CFG_FIFO_SIZE_32BITS	2
#define UART_CFG_FIFO_SIZE_64BITS	3

#define UART_CFG_EVEN_PARITY		0
#define UART_CFG_ODD_PARITY		1
#define UART_CFG_NO_PARITY		2
#define UART_CFG_RESERVED_PARITY	3

#define UART_MODE_WORD_LEN_8BITS	0
#define UART_MODE_WORD_LEN_7BITS	1
#define UART_MODE_WORD_LEN_6BITS	2
#define UART_MODE_WORD_LEN_5BITS	3

struct odin_serial_platform_data {
	unsigned long membase;	/* ioremap cookie or NULL */
	unsigned long mapbase;	/* resource base */
	unsigned int irq;	/* interrupt number */
	unsigned int uartclk;	/* UART clock rate */
	int debug;
};

enum uart_config_fifo_size {
	UART_CONFIG_FIFO_SIZE_8BIT	= 0,
	UART_CONFIG_FIFO_SIZE_16BIT	= 1,
	UART_CONFIG_FIFO_SIZE_32BIT	= 2,
	UART_CONFIG_FIFO_SIZE_64BIT	= 3
};

enum uart_config_parity_type {
	UART_CONFIG_PARITY_EVEN		= 0,
	UART_CONFIG_PARITY_ODD 		= 1,
	UART_CONFIG_PARITY_NO	 	= 2
};

enum uart_mode_word_length {
	UART_MODE_WORD_LENGTH_8BIT	= 0,
	UART_MODE_WORD_LENGTH_7BIT	= 1,
	UART_MODE_WORD_LENGTH_6BIT	= 2,
	UART_MODE_WORD_LENGTH_5BIT	= 3
};

typedef union
{
	struct
	{
		volatile unsigned int tx_fifo_emtpy     : 1;		// 0
		volatile unsigned int tx_fifo_half_full : 1;		// 1
		volatile unsigned int tx_fifo_full      : 1;		// 2
		volatile unsigned int rx_fifo_emtpy     : 1;		// 3
		volatile unsigned int rx_fifo_half_full : 1;		// 4
		volatile unsigned int rx_fifo_full      : 1;		// 5
		volatile unsigned int bus_push_error    : 1;		// 6
		volatile unsigned int bus_pop_error     : 1;		// 7
		volatile unsigned int frame_error       : 1;		// 8
		volatile unsigned int parity_error      : 1;		// 9
		volatile unsigned int rx_push_error     : 1;		// 10
		volatile unsigned int cts_rising_edge   : 1;		// 11
		volatile unsigned int cts_falling_edge  : 1;		// 12
		volatile unsigned int rts_rising_edge   : 1;		// 13
		volatile unsigned int rts_falling_edge  : 1;		// 14
		volatile unsigned int reserved15        : 17;		// 15-31
	} asField;
	volatile unsigned int as32Bits;
} UART_INTERRUPT;

typedef union
{
	struct
	{
		volatile unsigned int tx_divider 	: 10;			// 0-9
		volatile unsigned int tx_bit_cycle 	: 6;			// 10-15
		volatile unsigned int rx_divider 	: 10;			// 16-25
		volatile unsigned int rx_bit_cycle	: 6;			// 26-31
	} asField;
	volatile unsigned int as32Bits;
} UART_BAUD_RATE;

typedef union
{
	struct
	{
		volatile unsigned int stop_bits 		: 1;			// 0
		volatile unsigned int parity_type 		: 2;			// 1-2 :: uart_config_parity_type
		volatile unsigned int tx_endian			: 1;			// 3
		volatile unsigned int rx_endian 		: 1;			// 4
		volatile unsigned int local_loop 		: 1;			// 5
		volatile unsigned int remote_loop 		: 1;			// 6
		volatile unsigned int echo_enable 		: 1;			// 7
		volatile unsigned int ir_mode 			: 1;			// 8
		volatile unsigned int rx_polarity_low 	: 1;			// 9
		volatile unsigned int tx_polarity_low 	: 1;			// 10
		volatile unsigned int rts_invert 		: 1;			// 11
		volatile unsigned int uart_enable		: 1;			// 12
		volatile unsigned int reseved30		 	: 1;			// 13
        volatile unsigned int tx_dma_en         : 1;            // 14
        volatile unsigned int rx_dma_en         : 1;            // 15
		volatile unsigned int reserved31		: 1;			// 16
		volatile unsigned int tx_fifo_size 		: 2;			// 17-18 :: uart_config_fifo_size
		volatile unsigned int rx_fifo_size 		: 2;			// 19-20 :: uart_config_fifo_size
		volatile unsigned int rts_enable 		: 1;			// 21
		volatile unsigned int cts_enable 		: 1;			// 22
		volatile unsigned int cts_invert 		: 1;			// 23
		volatile unsigned int rts_fifo_level 	: 8;			// 24-31
	} asField;
	volatile unsigned int as32Bits;
} UART_CONFIG;

typedef union
{
	struct
	{
		volatile unsigned int tx_byte_count : 8;		// 0-7
		volatile unsigned int rx_byte_count : 8;		// 8-15
		volatile unsigned int reserved16 	: 14;		// 16-29
		volatile unsigned int tx_flush 		: 1;		// 30
		volatile unsigned int rx_flush 		: 1;		// 31
	} asField;
	volatile unsigned int as32Bits;
} UART_FIFO_STATUS;

typedef union
{
	struct
	{
		volatile unsigned int tx_half_full  : 8;		// 0-7
		volatile unsigned int rx_half_empty : 8;		// 8-15
		volatile unsigned int reserved16 	: 16;		// 16-31
	} asField;
	volatile unsigned int as32Bits;
} UART_FIFO_CONFIG;

typedef union
{
	struct
	{
		volatile unsigned int tx_word_length : 2;		// 0-1	:: uart_mode_word_length
		volatile unsigned int rx_word_length : 2;		// 2-3	:: uart_mode_word_length
		volatile unsigned int rx_sampling_pos: 6;		// 4-9
		volatile unsigned int tx_ir_start	 : 6;		// 10-15
		volatile unsigned int tx_ir_end		 : 6; 		// 16-21
		volatile unsigned int rts_status	 : 1;		// 22
		volatile unsigned int cts_status	 : 1;		// 23
		volatile unsigned int reserved05 	 : 8;		// 24-31
	} asField;
	volatile unsigned int as32Bits;
} UART_MODE;

typedef struct
{
	UART_INTERRUPT		interrupt_status;			// 0x00
	UART_INTERRUPT		interrupt_mask;				// 0x04
	UART_INTERRUPT		interrupt_clear;			// 0x08
	UART_BAUD_RATE		baud_rate;					// 0x0C
	UART_CONFIG			config;						// 0x10
	UART_FIFO_STATUS	fifo_status;				// 0x14
	UART_FIFO_CONFIG	fifo_config;				// 0x18
	UART_MODE			mode;						// 0x1C
	volatile unsigned int 	reserved020[56];		// 0x20-0xFC
	volatile unsigned int 	tx_fifo;				// 0x100
	volatile unsigned int 	reserved104[63];		// 0x104-0x1FC
	volatile unsigned int 	rx_fifo;				// 0x200
} UART_REGISTERS;


#endif /* __ASM_ARCH_REGS_UART_H */
