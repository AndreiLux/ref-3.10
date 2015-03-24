#ifndef __MT6575_UNCOMPRESS_H__
#define __MT6575_UNCOMPRESS_H__

#define MT_DEBUG_UART_PHY_BASE 0x11009000

#define MT_DEBUG_UART_LSR (*(volatile unsigned char *)( MT_DEBUG_UART_PHY_BASE+0x14))
#define MT_DEBUG_UART_THR (*(volatile unsigned char *)( MT_DEBUG_UART_PHY_BASE+0x0))
#define MT_DEBUG_UART_LCR (*(volatile unsigned char *)( MT_DEBUG_UART_PHY_BASE+0xc))
#define MT_DEBUG_UART_DLL (*(volatile unsigned char *)( MT_DEBUG_UART_PHY_BASE+0x0))
#define MT_DEBUG_UART_DLH (*(volatile unsigned char *)( MT_DEBUG_UART_PHY_BASE+0x4))
#define MT_DEBUG_UART_FCR (*(volatile unsigned char *)( MT_DEBUG_UART_PHY_BASE+0x8))
#define MT_DEBUG_UART_MCR (*(volatile unsigned char *)( MT_DEBUG_UART_PHY_BASE+0x10))
#define MT_DEBUG_UART_SPEED (*(volatile unsigned char *)( MT_DEBUG_UART_PHY_BASE+0x24))


static void arch_decomp_setup(void)
{
#if defined(CONFIG_MT6575_FPGA)
	MT_DEBUG_UART_LCR = 0x3;
	tmp =  MT_DEBUG_UART_LCR;
	MT_DEBUG_UART_LCR = (tmp | 0x80);
	MT_DEBUG_UART_SPEED = 0x0;
	MT_DEBUG_UART_DLL = 0x0E;
	MT_DEBUG_UART_DLH = 0;
	MT_DEBUG_UART_LCR = tmp;
	MT_DEBUG_UART_FCR = 0x0047;
	MT_DEBUG_UART_MCR = (0x1 | 0x2);
#endif
}

/*
 * This does not append a newline
 */
static inline void putc(int c)
{
	while (!(MT_DEBUG_UART_LSR & 0x20))
		;
	MT_DEBUG_UART_THR = c;
}

static inline void flush(void)
{
}

/*
 * nothing to do
 */
#define arch_decomp_wdog()

#endif				/* !__MT6575_UNCOMPRESS_H__ */
