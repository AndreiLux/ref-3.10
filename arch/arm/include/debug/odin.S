/* arch/arm/mach-realview/include/mach/debug-macro.S
 *
 * Debugging macro include header
 *
 *  Copyright (C) 1994-1999 Russell King
 *  Moved from linux/arch/arm/kernel/debug.S by Ben Dooks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define ODIN_LL_UART_PHYS		0x20014000
#define ODIN_LL_UART_VIRT		0xfe014000


		.macro	addruart,rp,rv,tmp
		movw	\rp, #(ODIN_LL_UART_PHYS & 0xffff)
		movt	\rp, #(ODIN_LL_UART_PHYS >> 16)
		movw	\rv, #(ODIN_LL_UART_VIRT & 0xffff)
		movt	\rv, #(ODIN_LL_UART_VIRT >> 16)
		.endm

#if defined(CONFIG_DEBUG_ODIN_UART)
#define ODIN_LL_UART_FIFO		0x80

		.macro	senduart,rd,rx
		strb	\rd, [\rx, #0x100]
		.endm

		.macro	waituart,rd,rx
1001:		ldr	\rd, [\rx, #0x014]
		tst	\rd, #0xff
		beq	1001b
		.endm

		.macro	busyuart,rd,rx
1001:		ldr	\rd, [\rx, #0x014]
		and	\rd, #0xff
		teq	\rd, #ODIN_LL_UART_FIFO
		bne	1001b
		.endm
#elif defined(CONFIG_DEBUG_ODIN_PL011_UART)
#include <asm/hardware/debug-pl01x.S>
#endif
