/*
 *  drivers/serial/odin-uart.c
 *
 * Copyright (C) 2011 MtekVision Ltd.
 *  	Tein Oh <ohti@mtekvision.com>
 *	Jinyoung Park <parkjy@mtekvision.com>
 * Copyright (C) 2012 LG Electronics Co. Ltd.
 *	Wonchul Choi  <wonchul.choi@lge.com>
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

#if defined(CONFIG_SERIAL_ODIN_UART_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/serial_reg.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/io.h>
#include <linux/slab.h>
#ifdef CONFIG_DMA_ENGINE
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#endif
#ifdef CONFIG_ODIN_SMMU_DMA
#include <linux/odin_iommu.h>
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <asm/irq.h>

#include <linux/pm_runtime.h>

#include "odin-uart.h"
#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>
#include <linux/device.h>

#if defined(CONFIG_ARCH_WODEN)
#define UART0_BASE	WODEN_FPGA_UART0
#define UART1_BASE	WODEN_FPGA_UART1
#define UART2_BASE	WODEN_FPGA_UART2
#else
#define UART0_BASE	ODIN_FPGA_UART0
#define UART1_BASE	ODIN_FPGA_UART1
#define UART2_BASE	ODIN_FPGA_UART2
#endif

#define PLCK_PERI_UART	"uart"

#define DRV_NAME		"odin-uart"
#define ODIN_MAX_UART	5
#define ODIN_UART_DEVICENAME		"ttyS"

#define UART_INT_CTS_RISING_EDGE_MASK	(1 << 11)
#define UART_INT_CTS_FALLING_EDGE_MASK	(1 << 12)
#if 1
#define ODIN_UART_CTS_ENABLE_STATUS		UART_INT_CTS_RISING_EDGE_MASK
#else
/* active negative */
#define  ODIN_UART_CTS_ENABLE_STATUS	UART_INT_CTS_FALLING_EDGE_MASK
#endif
#define UART_RX_PUSH		0x0400
#define UART_PARITY			0x0200
#define UART_FRAME			0x0100

#define UART_INT_ANY_ERROR	(UART_RX_PUSH|UART_PARITY|UART_FRAME)

#if defined(CONFIG_WODEN_HDK) || defined(CONFIG_WODEN_HDK_DT)
/* osc_clk */
#define UART_NM_FREQ 90000000
/* sub_clk */
#define UART_HS_FREQ 90000000
#else	/* !CONFIG_WODEN_HDK */
#if defined(CONFIG_MACH_ODIN_FPGA)
/* osc_clk */
#define UART_NM_FREQ 30000000
/* sub_clk */
#define UART_HS_FREQ 30000000
#else
/* osc_clk */
#define UART_NM_FREQ 40000000
/* sub_clk */
#define UART_HS_FREQ 40000000
#endif
#endif

#define DEBUG_RX 0
#define DEBUG_TX 0
#define INT_TX 1
#define WQ 0
#define RUNTIME 0

#if defined (CONFIG_ODIN_UART_CPU_FREQ)
extern int odin_register_peri_set_clk(int clk,int (*begin)(void *),
int (*setup)(void *,int), const char *name, void *dev);
#endif
static UART_REGISTERS *uartregisters[ODIN_MAX_UART];

static char *odin_drv_name[ODIN_MAX_UART] =
{
	"odin-uart0",
	"odin-uart1",
	"odin-uart2",
	"odin-uart3",
	"odin-uart4",
};
#if WQ
static struct workqueue_struct *odin_uart_wq;
#endif
#ifdef CONFIG_DMA_ENGINE
struct odin_dmarx_data {
	struct dma_chan		*chan;
	struct scatterlist	sg;
	char			*buf;
	dma_cookie_t		cookie;
	bool			running;
};

struct odin_dmatx_data {
	struct dma_chan		*chan;
	struct scatterlist	sg;
	char			*buf;
	bool			queued;
};
#endif

struct odin_uart_port {
	struct uart_port port;
	char *name;
	u32 int_mask;
	u32 cts_status;
	u32 stop_tx;
	int debug;
	bool b_startup;
#ifdef CONFIG_DMA_ENGINE
	/* DMA stuff */
	bool			shutdown;
	bool			using_tx_dma;
	bool			using_rx_dma;
	struct odin_dmarx_data	dma_rx;
	struct odin_dmatx_data	dma_tx;
#endif
	struct work_struct irq_work;
	struct clk *clk;
	unsigned long port_lock_flag;
	struct kobject kobj;
};

struct uart_attr {
	struct attribute attr;
	char *uart_name;
	int value;
};
static struct uart_attr get_register= {
	.attr.name="get_register",
	.attr.mode=0644,
	.value = 0,
};
static struct uart_attr set_register= {
	.attr.name="set_register",
	.attr.mode=0644,
	.value = 1,
};
static struct attribute * uartattr[] = {
	&get_register.attr,
	&set_register.attr,
	NULL
};

static void odin_uart_stop_tx(struct uart_port *port);

static u8 odin_uart_get_cts_edge_status(struct odin_uart_port *up,
u32 current_cts)
{
	u32 old_cts, new_cts;
#if 0
	const u8 cts_normal[4][4] =
	{
		/********************************************************
		  New CTS         0x00        0x01        0x10        0x11
		  Old CTS 0x00    N/A         RISING      FALLING     NOTING
		  0x01    NOTING      N/A         FALLING     RISING
		  0x10    NOTING      RISING      N/A         FALLING
		  0x11    NOTING      RISING      FALLING     N/A
		 ********************************************************/
		{0xFF,   0,      1,      0xFF},
		{0xFF,   0xFF,   1,      0},
		{0xFF,   0,      0xFF,   1},
		{0xFF,   0,      1,      0xFF},
	};
#endif

	const u8 cts_invert[4][4] =
	{
		/********************************************************
		  New CTS         0x00        0x01        0x10        0x11
		  Old CTS 0x00    N/A         FALLING     RISING      NOTING
		  0x01    NOTING      N/A         RISING      FALLING
		  0x10    NOTING      FALLING     N/A         RISING
		  0x11    NOTING      FALLING     RISING      N/A
		 ********************************************************/
		{0xFF,   1,      0,      0xFF},
		{0xFF,   0xFF,   0,      1},
		{0xFF,   1,      0xFF,   0},
		{0xFF,   1,      0,      0xFF},
	};

	new_cts = (current_cts & (UART_INT_CTS_RISING_EDGE_MASK)) ? 1 : 0;
	new_cts |= (current_cts & (UART_INT_CTS_FALLING_EDGE_MASK)) ? 2 : 0;

	old_cts = (up->cts_status & (UART_INT_CTS_RISING_EDGE_MASK)) ? 1 : 0;
	old_cts |= (up->cts_status & (UART_INT_CTS_FALLING_EDGE_MASK)) ? 2 : 0;


	return cts_invert[old_cts][new_cts];
	/* return cts_normal[old_cts][new_cts]; */
}

static int odin_uart_clk_enable(struct odin_uart_port *up)
{
	int ret = 0;
	char name[11];
	struct clk *clk;

	sprintf(name,"uart_pclk%d",up->port.line);
	clk = clk_get(NULL,name);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		return ret;
	}
	if (up->clk->enable_count == 0 && up->port.line != 0) {
		clk_prepare_enable(up->clk);
		clk_prepare_enable(clk); /* pclk */
	}
	return ret;
}

static int odin_uart_clk_disable(struct odin_uart_port *up)
{
	struct clk *clk;
	char name[11];
	int ret = 0;

	sprintf(name,"uart_pclk%d",up->port.line);
	clk = clk_get(NULL,name);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		return ret;
	}
	if (up->clk->enable_count != 0 && up->port.line != 0) {
		clk_disable_unprepare(up->clk);
		clk_put(up->clk);

		clk_disable_unprepare(clk);
		clk_put(clk);
	}
	return 0;
}

static inline void odin_uart_flush_fifo(struct odin_uart_port *up, u8 tx, u8 rx)
{
	if (tx)
		uartregisters[up->port.line]->fifo_status.asField.tx_flush = 1;
	if (rx)
		uartregisters[up->port.line]->fifo_status.asField.rx_flush = 1;
}

static inline int odin_uart_get_fifo_cnt(struct odin_uart_port *up, u8 type)
{
	if (type) /* rx */
		return uartregisters[up->port.line]->fifo_status.asField.rx_byte_count;
	else /* tx */
		return uartregisters[up->port.line]->fifo_status.asField.tx_byte_count;
}

static inline unsigned int odin_uart_get_channel(struct odin_uart_port *up)
{
	return up->port.line;
}
static inline unsigned int odin_uart_get_cts(struct odin_uart_port *up)
{
	return (uartregisters[up->port.line]->interrupt_status.as32Bits &
		(UART_INT_CTS_RISING_EDGE_MASK | UART_INT_CTS_FALLING_EDGE_MASK));
}

void odin_uart_set_baudrate(struct odin_uart_port *up, u32 baud_rate,
unsigned int uart_clk)
{
    unsigned int div, bitcycle, bit_cycles = 0, baud_rate_div = 0;
	int real_baud_rate, error;

#ifndef CONFIG_COMMON_CLK
    unsigned int clk_src_sel = 0;
	uart_clk = (clk_src_sel?UART_HS_FREQ:UART_NM_FREQ);
#endif
    for (bitcycle=63;bitcycle>5;bitcycle--)
	{
        div = uart_clk/(baud_rate*bitcycle) + (((uart_clk%(baud_rate*bitcycle))
		> ((baud_rate*bitcycle)/2))?1:0);
		if (div != 0)
		{
			real_baud_rate=uart_clk/(div*bitcycle);
			if (baud_rate < real_baud_rate)
				error = real_baud_rate - baud_rate;
			else
				error = baud_rate - real_baud_rate;

			if (error < (baud_rate*15/1000))
			{
				bit_cycles   = bitcycle - 1;
				baud_rate_div = div;
				break;
			}
		}
	}

	uartregisters[up->port.line]->baud_rate.asField.tx_bit_cycle = bit_cycles;
	uartregisters[up->port.line]->baud_rate.asField.rx_bit_cycle = bit_cycles;
	uartregisters[up->port.line]->mode.asField.rx_sampling_pos = bit_cycles/2;
	uartregisters[up->port.line]->baud_rate.asField.tx_divider = baud_rate_div;
	uartregisters[up->port.line]->baud_rate.asField.rx_divider = baud_rate_div;
}

#if defined (CONFIG_ODIN_UART_CPU_FREQ)
/*
 * DVFS problem :
 * when apb clock is changed, noise data is occured from serial.
 * In order to solve this problem,
 * I tried to on/off serial main clock, but it makes another problem.
 * So, temporary disconnect communication line with this method.
 */
int odin_uart_begin_clk(void *odin_up)
{
	struct odin_uart_port *up = (struct odin_uart_port *) odin_up;
	while (odin_uart_get_fifo_cnt(up, 0) < up->port.fifosize);

	if (up->port.mapbase == UART0_BASE) {
		odin_gpio_enable(GPIO25);
		odin_gpio_enable(GPIO26);
		odin_gpio_enable(GPIO27);
		odin_gpio_enable(GPIO28);
	} else if (up->port.mapbase == UART1_BASE) {
		odin_gpio_enable(GPIO29);
		odin_gpio_enable(GPIO30);
		odin_gpio_enable(GPIO31);
		odin_gpio_enable(GPIO32);
	} else if (up->port.mapbase == UART2_BASE) {
		odin_gpio_enable(GPIO33);
		odin_gpio_enable(GPIO34);
		odin_gpio_enable(GPIO35);
		odin_gpio_enable(GPIO36);
	}

	return 0;
}

int odin_uart_setup_clk(void *odin_up, int clk)
{
	struct odin_uart_port *up = (struct odin_uart_port *) odin_up;

	odin_uart_set_baudrate(up, (u32) clk);

	if (up->port.mapbase == UART0_BASE) {
		odin_gpio_disable(GPIO25);
		odin_gpio_disable(GPIO26);
		odin_gpio_disable(GPIO27);
		odin_gpio_disable(GPIO28);
	} else if (up->port.mapbase == UART1_BASE) {
		odin_gpio_disable(GPIO29);
		odin_gpio_disable(GPIO30);
		odin_gpio_disable(GPIO31);
		odin_gpio_disable(GPIO32);
	} else if (up->port.mapbase == UART2_BASE) {
		odin_gpio_disable(GPIO33);
		odin_gpio_disable(GPIO34);
		odin_gpio_disable(GPIO35);
		odin_gpio_disable(GPIO36);
	}

	return 0;
}
#endif

static inline void odin_uart_tx_chars(struct odin_uart_port *up);
static inline void odin_uart_rx_chars(struct odin_uart_port *up);

static void odin_uart_start_tx(struct uart_port *port);
static void odin_uart_start_rx(struct uart_port *port);
static void odin_uart_stop_tx(struct uart_port *port);
static void odin_uart_stop_rx(struct uart_port *port);
#if WQ
static irqreturn_t  odin_uart_irq_work(int irq, void *arg)
{
	struct odin_uart_port *up = (struct odin_uart_port *)arg;
	odin_uart_rx_chars(up);

	return IRQ_HANDLED;
}
#endif
#ifdef CONFIG_DMA_ENGINE
/*
 * All the DMA operation mode stuff goes inside this ifdef.
 * This assumes that you have a generic DMA device interface,
 * no custom DMA interfaces are supported.
 */
#define ODIN_UART_DMA_BUFFER_SIZE UART_XMIT_SIZE

static void odin_uart_dma_rx_callback(void *data);
static int odin_uart_dma_tx_refill(struct odin_uart_port *uap);
static int odin_uart_dma_rx_trigger_dma(struct odin_uart_port *uap, int fifo);
#if 0
static void odin_uart_sgbuf_free(struct dma_chan *chan,
struct odin_dmarx_data *sg,	enum dma_data_direction dir)
{
	if (sg->buf) {
		dma_unmap_sg(chan->device->dev, &sg->sg, 1, dir);
		kfree(sg->buf);
	}
}
#endif
bool odin_uart_dma_filter(struct dma_chan *chan, void *filter_param)
{
	struct odin_uart_port *uap = (struct odin_uart_port *)filter_param;

#if defined(CONFIG_ARCH_WODEN)
	/* dma-pl230.0 => PDMA dev id */
	if (strcmp(dev_name(chan->device->dev), "20020000.pdma")==0
	&& (chan->chan_id == uap->port.line*2
	|| chan->chan_id == uap->port.line*2 + 1))
		return 1;
#else	/* CONFIG_ARCH_ODIN */
	static char rx_chan_check = 1;

	/* pl08xdmac => PDMA dev id */
	if (strcmp(dev_name(chan->device->dev), "20000000.pdma")==0
	&& (chan->chan_id == uap->port.line*2+2+rx_chan_check))
	{
		if (rx_chan_check)
			rx_chan_check = 0;
		else
			rx_chan_check = 1;

		return 1;
	}
#endif
	else
		return 0;
}

static void odin_uart_dma_probe(struct odin_uart_port *uap)
{
	/* DMA is the sole user of the platform data right now */
	struct dma_slave_config tx_conf = {
		.dst_addr = uap->port.mapbase + 0x100,
		.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE,
		.direction = DMA_TO_DEVICE,
		.dst_maxburst = uap->port.fifosize >> 1,
	};
	struct dma_slave_config rx_conf = {
		.src_addr = uap->port.mapbase + 0x200,
		.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE,
		.direction = DMA_FROM_DEVICE,
		.src_maxburst = uap->port.fifosize >> 1,
	};

	struct dma_chan *chan;
	dma_cap_mask_t mask;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	chan = dma_request_channel(mask, &odin_uart_dma_filter, (void *)uap);
	if (!chan) {
		dev_err(uap->port.dev,"no RX DMA channel!\n");
		return;
	}
	dmaengine_slave_config(chan, &rx_conf);
	uap->dma_rx.chan = chan;
	dev_info(uap->port.dev, "DMA channel RX %s client count %d\n",
		 dma_chan_name(uap->dma_rx.chan),uap->dma_rx.chan->client_count);

        chan = dma_request_channel(mask, &odin_uart_dma_filter, (void *)uap);
	if (!chan) {
		dev_err(uap->port.dev,"no TX DMA channel!\n");
		return;
	}
	dmaengine_slave_config(chan, &tx_conf);
	uap->dma_tx.chan = chan;
	dev_info(uap->port.dev, "DMA channel TX %s client count %d\n",
		 dma_chan_name(uap->dma_tx.chan),uap->dma_tx.chan->client_count);
}


static void odin_uart_dma_rx_chars(struct odin_uart_port *uap,
			       u32 pending, bool readfifo)
{
	struct tty_port *tty = &uap->port.state->port;
	struct device *dev = uap->dma_rx.chan->device->dev;
	struct odin_dmarx_data *dmarx = &uap->dma_rx;
	int dma_count = 0;

	/* Pick everything from the DMA first */
#if 1
	if (pending) {
		/* Sync in buffer */
		dma_sync_sg_for_cpu(dev, &dmarx->sg, 1, DMA_FROM_DEVICE);
		dma_count = tty_insert_flip_string(tty, dmarx->buf, pending);
#if DEBUG_RX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)start pending : %d dma_count : %d\n"
			,__func__, pending, dma_count);
#endif

		/* Return buffer to device */
		dma_sync_sg_for_device(dev, &dmarx->sg, 1, DMA_FROM_DEVICE);

		uap->port.icount.rx += dma_count;
		if (dma_count < pending)
			dev_warn(uap->port.dev,
				 "couldn't insert all characters (TTY is full?)\n");
		tty_flip_buffer_push(tty);

	}
#endif
	if (dma_count == pending && readfifo) {
		/* Clear any error flags */
		uartregisters[uap->port.line]->interrupt_clear.as32Bits = 0;
#if DEBUG_RX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)befor rx_chars dma_count = 0\n",__func__);
#endif
		odin_uart_rx_chars(uap);
	}
#if DEBUG_RX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)end\n",__func__);
#endif
}

static bool odin_uart_dma_tx_irq(struct odin_uart_port *uap)
{

	if (!uap->using_tx_dma)
		return false;

	/*
	 * If we already have a TX buffer queued, but received a
	 * TX interrupt, it will be because we've just sent an X-char.
	 * Ensure the TX DMA is enabled and the TX IRQ is disabled.
	 */

#if DEBUG_TX
	if (uap->port.line == 1)
		printk(KERN_DEBUG"%s)start\n",__func__);
#endif
	if (uap->dma_tx.queued) {
		uartregisters[uap->port.line]->config.asField.tx_dma_en = 1;
		odin_uart_stop_tx(&uap->port);
#if DEBUG_TX
	if (uap->port.line == 1)
		printk(KERN_DEBUG"%s)end tx.queued\n",__func__);
#endif
		return true;
	}

	if (odin_uart_dma_tx_refill(uap) > 0) {
#if DEBUG_TX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)end after tx_refill\n",__func__);
#endif
		return true;
	}
#if DEBUG_TX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)end return false\n",__func__);
#endif
	return false;

}

/*
 * Stop the DMA transmit (eg, due to received XOFF).
 * Locking: called with port lock held and IRQs disabled.
 */
static inline void odin_uart_dma_tx_stop(struct odin_uart_port *uap)
{
	if (uap->dma_tx.queued) {
		uartregisters[uap->port.line]->config.asField.tx_dma_en = 0;
	}
}

static int odin_uart_dma_rx_trigger_dma(struct odin_uart_port *uap, int fifo)
{
	struct dma_chan *rxchan = uap->dma_rx.chan;
	struct odin_dmarx_data *dmarx = &uap->dma_rx;
	struct dma_async_tx_descriptor *desc;

#if 0 /* def CONFIG_ODIN_SMMU_PERI */
	uap->dma_rx.sg.dma_address = dma_map_single(uap->dma_rx.chan->device->dev,
													uap->dma_rx.buf,
													ODIN_UART_DMA_BUFFER_SIZE,
													DMA_FROM_DEVICE);
#endif
	if (!rxchan)
		return -EIO;
	/* Start the RX DMA job */

#ifdef CONFIG_ODIN_SMMU_DMA
	uap->dma_rx.sg.dma_length = fifo;
	uap->dma_rx.sg.length = fifo;
#else
	uap->dma_rx.sg.length = fifo;
#endif

#if DEBUG_RX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)start fifo : %d\n",__func__, fifo);
#endif
	if (dma_map_sg(uap->dma_rx.chan->device->dev, &uap->dma_rx.sg, 1,
	DMA_FROM_DEVICE) != 1) {
		dmarx->running= false;
		dev_dbg(uap->port.dev, "unable to map RX DMA\n");
		return -EBUSY;
	}
	desc = rxchan->device->device_prep_slave_sg(rxchan, &uap->dma_rx.sg, 1,
					DMA_FROM_DEVICE,
					DMA_PREP_INTERRUPT | DMA_CTRL_ACK, NULL);
	/*
	 * If the DMA engine is busy and cannot prepare a
	 * channel, no big deal, the driver will fall back
	 * to interrupt mode as a result of this error code.
	 */
	if (!desc) {
		dmarx->running = false;
		printk(KERN_ERR"%s)!desc\n",__func__);
		dmaengine_terminate_all(rxchan);
		return -EBUSY;
	}

	/* Some data to go along to the callback */
	desc->callback = odin_uart_dma_rx_callback;
	desc->callback_param = uap;
	dmarx->cookie = dmaengine_submit(desc);
	dma_async_issue_pending(rxchan);
	uartregisters[uap->port.line]->config.asField.rx_dma_en = 1;

	dmarx->running = true;

#if DEBUG_RX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)end\n",__func__);
#endif

	return 0;
}

static void odin_uart_dma_rx_callback(void *data)
{
	struct odin_uart_port *uap = data;
	struct odin_dmarx_data *dmarx = &uap->dma_rx;
	int ret;
	int len;
	unsigned long flag;

	/*
	 * This completion interrupt occurs typically when the
	 * RX buffer is totally stuffed but no timeout has yet
	 * occurred. When that happens, we just want the RX
	 * routine to flush out the secondary DMA buffer while
	 * we immediately trigger the next DMA job.
	 */
	spin_lock_irq(&uap->port.lock);

	if (uap->shutdown == true) {
		dma_unmap_sg(uap->dma_rx.chan->device->dev, &uap->dma_rx.sg, 1,
		DMA_FROM_DEVICE);
		spin_unlock_irq(&uap->port.lock);
		return;
	}

#ifdef CONFIG_ODIN_SMMU_DMA
	len = uap->dma_rx.sg.dma_length;
#else
	len = uap->dma_rx.sg.length;
#endif

#if DEBUG_RX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)start len : %d\n",__func__, len);
#endif

	odin_uart_dma_rx_chars(uap, len, false);

	/*
	 * Do this check after we picked the DMA chars so we don't
	 * get some IRQ immediately from RX.
	 */
	len = odin_uart_get_fifo_cnt(uap,1);
	if (len >= 1){
#if DEBUG_RX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)before rx_trigger_dma len >= 1\n",__func__);
#endif
		dma_unmap_sg(uap->dma_rx.chan->device->dev, &uap->dma_rx.sg, 1,
		DMA_FROM_DEVICE);
		ret = odin_uart_dma_rx_trigger_dma(uap, len);
		if (ret) {
			dev_dbg(uap->port.dev, "could not retrigger RX DMA job "
				"fall back to interrupt mode\n");
			odin_uart_start_rx(&uap->port);
		}
	}
	else{
#if DEBUG_RX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)before start_rx, rx_chars len = 0\n",__func__);
#endif
		odin_uart_start_rx(&uap->port);
		dma_unmap_sg(uap->dma_rx.chan->device->dev, &uap->dma_rx.sg, 1,
		DMA_FROM_DEVICE);
#if 0
		odin_uart_rx_chars(uap);
#endif
#if DEBUG_RX
		if (uap->port.line == 1) {
			printk(KERN_DEBUG"%s)int_status %x mask : %x fifo_status : %x \n"
			,__func__
			,uartregisters[1]->interrupt_status.as32Bits
			,uartregisters[1]->interrupt_mask.as32Bits
			,uartregisters[1]->fifo_status.as32Bits);
		}
#endif
	}
	spin_unlock_irq(&uap->port.lock);
}

static void odin_uart_dma_tx_callback(void *data)
{
	struct odin_uart_port *uap = data;
	struct odin_dmatx_data *dmatx = &uap->dma_tx;
	struct circ_buf *xmit = &uap->port.state->xmit;
	unsigned int pending;

	spin_lock_irqsave(&uap->port.lock, uap->port_lock_flag);
	pending = uart_circ_chars_pending(xmit);
#if DEBUG_TX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)start pending : %d\n",__func__, pending);
#endif

	if ( pending > 0 )
	{
		odin_uart_dma_tx_refill(uap);
#if DEBUG_TX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)end after tx_refill\n",__func__);
#endif
		spin_unlock_irqrestore(&uap->port.lock, uap->port_lock_flag);
		return;
	}
	if ( pending == 0 || uart_tx_stopped(&uap->port) ||
	uart_circ_empty(&uap->port.state->xmit))
	{
		odin_uart_dma_tx_stop(uap);
		dmatx->queued = false;
#if DEBUG_TX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)before start_tx pending = 0\n",__func__);
#endif
		odin_uart_stop_tx(&uap->port);

#if DEBUG_TX
		if (uap->port.line == 1) {
			printk(KERN_DEBUG"%s)int_status %x mask : %x fifo_status : %x \n"
				,__func__
				,uartregisters[1]->interrupt_status.as32Bits
				,uartregisters[1]->interrupt_mask.as32Bits
			,uartregisters[1]->fifo_status.as32Bits);
	}
#endif
		dma_unmap_sg(dmatx->chan->device->dev, &dmatx->sg, 1, DMA_TO_DEVICE);
	}
	spin_unlock_irqrestore(&uap->port.lock, uap->port_lock_flag);
}

/*
 * Try to refill the TX DMA buffer.
 * Locking: called with port lock held and IRQs disabled.
 * Returns:
 *   1 if we queued up a TX DMA buffer.
 *   0 if we didn't want to handle this by DMA
 *  <0 on error
 */
static int odin_uart_dma_tx_refill(struct odin_uart_port *uap)
{
	struct odin_dmatx_data *dmatx = &uap->dma_tx;
	struct dma_chan *chan = dmatx->chan;
	struct dma_device *dma_dev = chan->device;
	struct dma_async_tx_descriptor *desc;
	struct circ_buf *xmit = &uap->port.state->xmit;
	unsigned int count;

	/*
	 * Try to avoid the overhead involved in using DMA if the
	 * transaction fits in the first half of the FIFO, by using
	 * the standard interrupt handling.  This ensures that we
	 * issue a uart_write_wakeup() at the appropriate time.
	 */
	count = uart_circ_chars_pending(xmit);
/*
	if (count < (uap->port.fifosize >> 1)) {
		dmatx->queued = false;
		return 0;
	}
*/
	/*
	 * Bodge: don't send the last character by DMA, as this
	 * will prevent XON from notifying us to restart DMA.
	 */
/*	count -= 1; */
/* debug */
#if DEBUG_TX
		if (uap->port.line == 1)
			printk(KERN_DEBUG"%s)start count : %d\n",__func__, count);
#endif

	/* Else proceed to copy the TX chars to the DMA buffer and fire DMA */
	if (count > ODIN_UART_DMA_BUFFER_SIZE)
		count = ODIN_UART_DMA_BUFFER_SIZE;

	if (xmit->tail < xmit->head)
		memcpy(&dmatx->buf[0], &xmit->buf[xmit->tail], count);
	else {
		size_t first = UART_XMIT_SIZE - xmit->tail;
		size_t second = xmit->head;

		memcpy(&dmatx->buf[0], &xmit->buf[xmit->tail], first);
		if (second)
			memcpy(&dmatx->buf[first], &xmit->buf[0], second);
	}

#if 0 /* def CONFIG_ODIN_SMMU_PERI */
	uap->dma_tx.sg.dma_address = dma_map_single(uap->dma_tx.chan->device->dev,
													uap->dma_tx.buf,
													ODIN_UART_DMA_BUFFER_SIZE,
													DMA_TO_DEVICE);
#endif

#ifdef CONFIG_ODIN_SMMU_DMA
	dmatx->sg.length = count;
	dmatx->sg.dma_length = count;
#else
	dmatx->sg.length = count;
#endif

	if (dma_map_sg(uap->dma_tx.chan->device->dev, &uap->dma_tx.sg, 1,
	DMA_TO_DEVICE) != 1) {
		dmatx->queued = false;
		dev_dbg(uap->port.dev, "unable to map TX DMA\n");
		return -EBUSY;
	}

	desc = dma_dev->device_prep_slave_sg(chan, &uap->dma_tx.sg, 1,DMA_TO_DEVICE,
	DMA_PREP_INTERRUPT | DMA_CTRL_ACK, NULL);
	if (!desc) {
		dma_unmap_sg(dma_dev->dev, &dmatx->sg, 1, DMA_TO_DEVICE);
		dmatx->queued = false;
		/*
		 * If DMA cannot be used right now, we complete this
		 * transaction via IRQ and let the TTY layer retry.
		 */
		dev_dbg(uap->port.dev, "TX DMA busy\n");
		return -EBUSY;
	}

	/* Some data to go along to the callback */
	desc->callback = odin_uart_dma_tx_callback;
	desc->callback_param = uap;

	/* All errors should happen at prepare time */
	dmaengine_submit(desc);

	/* Fire the DMA transaction */
	dma_dev->device_issue_pending(chan);
	uartregisters[uap->port.line]->config.asField.tx_dma_en = 1;

	dmatx->queued = true;

	/*
	 * Now we know that DMA will fire, so advance the ring buffer
	 * with the stuff we just dispatched.
	 */
	xmit->tail = (xmit->tail + count) & (UART_XMIT_SIZE - 1);
	uap->port.icount.tx += count;

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS){
#if INT_TX
		spin_unlock_irqrestore(&uap->port.lock, uap->port_lock_flag);
#endif
		uart_write_wakeup(&uap->port);
#if INT_TX
		spin_lock_irqsave(&uap->port.lock, uap->port_lock_flag);
#endif
	}
#if DEBUG_TX
	if (uap->port.line == 1)
		printk(KERN_DEBUG"%s)end\n",__func__);
#endif
	return 1;
}


void odin_uart_dma_startup(struct odin_uart_port *uap)
{
	if (!uap->dma_tx.chan)
		return;

	uap->dma_tx.buf = kmalloc(ODIN_UART_DMA_BUFFER_SIZE, GFP_DMA);

#if 0 /* def CONFIG_ODIN_SMMU_PERI */
	uap->dma_tx.sg.dma_address = dma_map_single(uap->dma_tx.chan->device->dev,
													uap->dma_tx.buf,
													ODIN_UART_DMA_BUFFER_SIZE,
													DMA_TO_DEVICE);
#endif

	if (!uap->dma_tx.buf) {
		dev_err(uap->port.dev, "no memory for DMA TX buffer\n");
		return;
	}
	memset(uap->dma_tx.buf, 0, ODIN_UART_DMA_BUFFER_SIZE);

	sg_init_one(&uap->dma_tx.sg, uap->dma_tx.buf, ODIN_UART_DMA_BUFFER_SIZE);

	printk(KERN_INFO"dma_txbuf = %x, dma_address = %x\n",
	(u32)uap->dma_tx.buf, (u32)uap->dma_tx.sg.dma_address);
	uap->using_tx_dma = true;

	/* Allocate and map DMA RX buffers */

	uap->dma_rx.buf = kmalloc(ODIN_UART_DMA_BUFFER_SIZE, GFP_DMA);

#if 0 /* def CONFIG_ODIN_SMMU_PERI */
	uap->dma_rx.sg.dma_address = dma_map_single(uap->dma_rx.chan->device->dev,
													uap->dma_rx.buf,
													ODIN_UART_DMA_BUFFER_SIZE,
													DMA_FROM_DEVICE);
#endif

	if (!uap->dma_rx.buf) {
		dev_err(uap->port.dev, "no memory for DMA RX buffer\n");
		return;
	}
	memset(uap->dma_rx.buf, 0, ODIN_UART_DMA_BUFFER_SIZE);
	sg_init_one(&uap->dma_rx.sg, uap->dma_rx.buf, ODIN_UART_DMA_BUFFER_SIZE);

	printk(KERN_INFO"dma_rxbuf = %x, dma_address = %x\n",
	(u32)uap->dma_rx.buf, (u32)uap->dma_rx.sg.dma_address);
	uap->using_rx_dma = true;

	return;
}

static void odin_uart_dma_shutdown(struct odin_uart_port *uap)
{
	if (!(uap->using_tx_dma || uap->using_rx_dma))
		return;

	/* Disable RX and TX DMA */
	spin_lock_irq(&uap->port.lock);
	uartregisters[uap->port.line]->config.asField.tx_dma_en = 0;
	uartregisters[uap->port.line]->config.asField.rx_dma_en = 0;
	spin_unlock_irq(&uap->port.lock);
	barrier();

	if (uap->using_tx_dma) {
		/* Clean up the TX DMA */
		dmaengine_terminate_all(uap->dma_tx.chan);
		dma_release_channel(uap->dma_tx.chan);
		kfree(uap->dma_tx.buf);
		uap->dma_tx.buf = NULL;
		uap->using_tx_dma = false;
	}

	if (uap->using_rx_dma) {
		/* Clean up the RX DMA */
		dmaengine_terminate_all(uap->dma_rx.chan);
		dma_release_channel(uap->dma_rx.chan);
		kfree(uap->dma_rx.buf);
		uap->dma_rx.buf = NULL;
		uap->using_rx_dma = false;
	}
}
#endif

static inline void odin_uart_tx_chars(struct odin_uart_port *up)
{
	/* jtjt struct circ_buf *xmit = &up->port.info->xmit; */
	struct circ_buf *xmit = &up->port.state->xmit;
	int len;
#if DEBUG_TX
	int count;
	count = uart_circ_chars_pending(xmit);
	if (up->port.line == 1)
		printk(KERN_DEBUG"%s)count : %d\n",__func__, count);
#endif

	if (up->port.x_char) {
		uartregisters[up->port.line]->tx_fifo = up->port.x_char;
		up->port.icount.tx++;
		up->port.x_char = 0;
#if DEBUG_TX
	if (up->port.line == 1)
		printk(KERN_DEBUG"%s)x_char return\n",__func__);
#endif
		return;
	}

	if (uart_tx_stopped(&up->port) || uart_circ_empty(xmit)) {
#if DEBUG_TX
	if (up->port.line == 1)
		printk(KERN_DEBUG"%s)tx_stop & empty return\n",__func__);
#endif
		odin_uart_stop_tx(&up->port);
		return;
	}

#ifdef CONFIG_DMA_ENGINE
	if (up->using_tx_dma)
	{
		odin_uart_stop_tx(&up->port);
#if DEBUG_TX
	if (up->port.line == 1)
		printk(KERN_DEBUG"%s)start before dma_tx_irq\n",__func__);
#endif
		odin_uart_dma_tx_irq(up);
#if DEBUG_TX
	if (up->port.line == 1)
		printk(KERN_DEBUG"%s)end after dma_tx_irq\n",__func__);
#endif
		return ;
	}
#endif

	len = odin_uart_get_fifo_cnt(up,0);
	while (!uart_circ_empty(xmit) && (odin_uart_get_fifo_cnt(up, 0) > 0)) {
		uartregisters[up->port.line]->tx_fifo = xmit->buf[xmit->tail];
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		up->port.icount.tx++;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) {
		spin_unlock_irqrestore(&up->port.lock, up->port_lock_flag);
		uart_write_wakeup(&up->port);
		spin_lock_irqsave(&up->port.lock, up->port_lock_flag);
	}

	if (uart_circ_empty(xmit))
		odin_uart_stop_tx(&up->port);
}

static inline void odin_uart_rx_chars(struct odin_uart_port *up)
{
	/* jtjt struct tty_struct *tty = up->port.info->port.tty; */
	struct tty_port *tty = &up->port.state->port;
	unsigned int c;
	char flag;
	UART_INTERRUPT  uart_clear;
	UART_INTERRUPT  uart_int;
	UART_INTERRUPT  over_run;
	int max_count = up->port.fifosize - 16;
	int rxfifo;

	while (max_count-- > 0) {

		rxfifo = odin_uart_get_fifo_cnt(up, 1);
#if DEBUG_RX
		if (up->port.line == 1)
			printk(KERN_DEBUG"%s)start rxfifo : %d\n"
			,__func__, rxfifo);
#endif

		if (rxfifo == 0)
			break;
#ifdef CONFIG_DMA_ENGINE
        else if (up->using_rx_dma==false)
#else
		else

#endif
		{
			do{
				c = uartregisters[up->port.line]->rx_fifo & 0xFF;
				flag = TTY_NORMAL;
				up->port.icount.rx++;

				uart_int.as32Bits=uartregisters[up->port.line]->interrupt_status.as32Bits;

				if (unlikely(uart_int.as32Bits & UART_INT_ANY_ERROR)) {
					if (uart_int.asField.parity_error)
						up->port.icount.parity++;
					else if (uart_int.asField.frame_error)
						up->port.icount.frame++;

					uart_clear.as32Bits = 0;
					uart_clear.asField.frame_error = 1;
					uart_clear.asField.parity_error = 1;
					uartregisters[up->port.line]->interrupt_clear.as32Bits
					= uart_clear.as32Bits;

					if (uart_int.asField.rx_push_error) {
						up->port.icount.overrun++;
						uartregisters[up->port.line]->interrupt_clear.asField.rx_push_error = 1;
					}

					uart_int.as32Bits &= up->port.read_status_mask;
					if (uart_int.asField.parity_error)
						flag = TTY_PARITY;
					else if (uart_int.asField.frame_error)
						flag = TTY_FRAME;
				}

				if (uart_handle_sysrq_char(&up->port, c))
					continue;

				over_run.as32Bits = 0; /* check,plz... by.tiny */
				over_run.asField.rx_push_error = 1;
				uart_insert_char(&up->port, uart_int.as32Bits, over_run.as32Bits, c, flag);
			} while (--rxfifo);
			spin_unlock(&up->port.lock);
			tty_flip_buffer_push(tty);
			spin_lock(&up->port.lock);
		}
#ifdef CONFIG_DMA_ENGINE
		else{
			odin_uart_stop_rx(&up->port);
			odin_uart_dma_rx_trigger_dma(up, rxfifo);
			break;
		}
#endif
	}
#if DEBUG_RX
		if (up->port.line == 1)
			printk(KERN_DEBUG"%s)end\n",__func__);
#endif
}

static inline void odin_uart_check_modem_status(struct odin_uart_port *up)
{
	u32 status;
	u8 cts_status;

	status = odin_uart_get_cts(up);

	if (status != 0) {
		cts_status = odin_uart_get_cts_edge_status(up, status);
		if (cts_status == 1 ) {
			uart_handle_cts_change(&up->port, cts_status);
			up->stop_tx = 0;
		}
		else if (cts_status == 0) {
			uart_handle_cts_change(&up->port, cts_status);
			up->stop_tx = 1;
			return;
		}
		else if (up->stop_tx == 1)
			return;

		up->cts_status = status;
		/* jtjt wake_up_interruptible(&up->port.info->delta_msr_wait); */
		wake_up_interruptible(&up->port.state->port.delta_msr_wait);
	}
}

static irqreturn_t odin_uart_irq(int irq, void *dev_id)
{
	struct odin_uart_port *up = (struct odin_uart_port *)dev_id;
	UART_INTERRUPT int_status;
	UART_INTERRUPT int_clear;
	irqreturn_t ret = IRQ_HANDLED;

	spin_lock_irqsave(&up->port.lock, up->port_lock_flag);
	int_clear.as32Bits = 0xFFFFFFFF;
#if RUNTIME
	pm_runtime_get_sync(up->port.dev);
#endif
	int_status.as32Bits = uartregisters[up->port.line]->interrupt_status.as32Bits;
	uartregisters[up->port.line]->interrupt_mask.as32Bits = int_clear.as32Bits;

	/* Interrupt must be cleared before reading rx fifo. */
	int_clear.as32Bits &= (~UART_INT_ANY_ERROR);
	if (!int_status.asField.cts_rising_edge)
		int_clear.asField.cts_rising_edge = 0;

	if (!int_status.asField.cts_falling_edge)
		int_clear.asField.cts_falling_edge = 0;

	uartregisters[up->port.line]->interrupt_clear.as32Bits = int_clear.as32Bits;

	if (int_status.asField.rx_fifo_emtpy ||
	    int_status.asField.rx_fifo_half_full ||
	    int_status.asField.rx_fifo_full)
	{
		uartregisters[up->port.line]->config.asField.rx_dma_en = 0;
#if WQ
		ret = IRQ_WAKE_THREAD;
#else
		odin_uart_rx_chars(up);
#endif
	}
	/* FIXME : workaround of avoiding unlimited isr call
			   by clearing any int errors. */
	uartregisters[up->port.line]->interrupt_clear.as32Bits |= UART_INT_ANY_ERROR;

#if INT_TX
	if (int_status.asField.tx_fifo_full)
		odin_uart_tx_chars(up);
#endif
	uartregisters[up->port.line]->interrupt_mask.as32Bits = up->int_mask;
	spin_unlock_irqrestore(&up->port.lock, up->port_lock_flag);

#if RUNTIME
	pm_runtime_mark_last_busy(up->port.dev);
	pm_runtime_put_autosuspend(up->port.dev);
#endif
	return ret;
}

static const char *odin_uart_type(struct uart_port *port)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);

	return up->name;
}

static unsigned int odin_uart_tx_empty(struct uart_port *port)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);
	unsigned int ret = 0;

#if RUNTIME
	pm_runtime_get_sync(up->port.dev);
#endif
	spin_lock_irqsave(&up->port.lock, up->port_lock_flag);

	if (odin_uart_get_fifo_cnt(up, 0) == up->port.fifosize)
		ret = TIOCSER_TEMT;

	spin_unlock_irqrestore(&up->port.lock, up->port_lock_flag);

#if RUNTIME
	pm_runtime_mark_last_busy(up->port.dev);
	pm_runtime_put_autosuspend(up->port.dev);
#endif

	return ret;
}

static unsigned int odin_uart_get_mctrl(struct uart_port *port)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);
	UART_INTERRUPT int_status;
	u32 cts_status;
	unsigned int ret = 0;

#if RUNTIME
	pm_runtime_get_sync(up->port.dev);
#endif
	int_status.as32Bits = uartregisters[up->port.line]->interrupt_status.as32Bits;

#if RUNTIME
	pm_runtime_mark_last_busy(up->port.dev);
	pm_runtime_put_autosuspend(up->port.dev);
#endif

	if (!int_status.asField.cts_rising_edge &&
	!int_status.asField.cts_falling_edge) {
		if (up->cts_status & ODIN_UART_CTS_ENABLE_STATUS || up->cts_status == 0)
			ret |= TIOCM_CTS;
	}
	else {
		cts_status = odin_uart_get_cts_edge_status(up, int_status.as32Bits);
		if (cts_status == 1 || ((cts_status == 0xFF) &&
		    !(int_status.as32Bits & ODIN_UART_CTS_ENABLE_STATUS)))
			ret |= TIOCM_CTS;
	}

	return ret;
}

static void odin_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);

	if (mctrl & TIOCM_RTS){
		uartregisters[up->port.line]->config.asField.rts_enable = 1;
		uartregisters[up->port.line]->config.asField.rts_invert = 1;
		uartregisters[up->port.line]->config.asField.cts_enable = 1;
		uartregisters[up->port.line]->config.asField.cts_invert = 1;
	}
	else
		uartregisters[up->port.line]->config.asField.rts_enable = 0;
}

static void odin_uart_stop_tx(struct uart_port *port)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);
	UART_INTERRUPT int_mask;

#if RUNTIME
	pm_runtime_get_sync(up->port.dev);
#endif

	int_mask.as32Bits = 0;
#if INT_TX
	int_mask.asField.tx_fifo_emtpy = 1;
	int_mask.asField.tx_fifo_full = 1;
#endif
	up->int_mask |= int_mask.as32Bits;

	uartregisters[up->port.line]->interrupt_mask.as32Bits = up->int_mask;

#if RUNTIME
	pm_runtime_mark_last_busy(up->port.dev);
	pm_runtime_put_autosuspend(up->port.dev);
#endif
}

static void odin_uart_start_tx(struct uart_port *port)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);
	UART_INTERRUPT int_mask;
#if DEBUG_TX
	if (up->port.line == 1)
		printk(KERN_DEBUG"%s)start\n",__func__);
#endif
#if RUNTIME
	pm_runtime_get_sync(up->port.dev);
#endif
	int_mask.as32Bits = 0;
#if INT_TX
	int_mask.asField.tx_fifo_full = 1;
#endif
	up->int_mask &= ~int_mask.as32Bits;

	uartregisters[up->port.line]->interrupt_mask.as32Bits = up->int_mask;
	odin_uart_tx_chars(up);
#if RUNTIME
	pm_runtime_mark_last_busy(up->port.dev);
	pm_runtime_put_autosuspend(up->port.dev);
#endif
#if DEBUG_TX
	if (up->port.line == 1)
		printk(KERN_DEBUG"%s)end int_status %x fifo_status : %x\n"
		,__func__
		,uartregisters[1]->interrupt_status.as32Bits
		,uartregisters[1]->fifo_status.as32Bits);
#endif
}

static void odin_uart_start_rx(struct uart_port *port)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);
	UART_INTERRUPT int_mask;

#if DEBUG_RX
		if (up->port.line == 1)
			printk(KERN_DEBUG"%s)start\n",__func__);
#endif
#if RUNTIME
	pm_runtime_get_sync(up->port.dev);
#endif
	int_mask.as32Bits = 0;
	int_mask.asField.rx_fifo_half_full = 1;
	int_mask.asField.frame_error = 1;
	int_mask.asField.parity_error = 1;
	int_mask.asField.rx_push_error = 1;

	up->int_mask &= ~int_mask.as32Bits;

	uartregisters[up->port.line]->interrupt_mask.as32Bits = up->int_mask;
#if RUNTIME
	pm_runtime_mark_last_busy(up->port.dev);
	pm_runtime_put_autosuspend(up->port.dev);
#endif
#if DEBUG_RX
		if (up->port.line == 1)
			printk(KERN_DEBUG"%s)end\n",__func__);
#endif
}

static void odin_uart_stop_rx(struct uart_port *port)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);
	UART_INTERRUPT int_mask;
	UART_INTERRUPT int_clear;

#if RUNTIME
	pm_runtime_get_sync(up->port.dev);
#endif
	int_mask.as32Bits = 0;
	int_mask.asField.rx_fifo_emtpy = 1;
	int_mask.asField.rx_fifo_half_full = 1;
	int_mask.asField.rx_fifo_full = 1;
	up->int_mask |= int_mask.as32Bits;

	int_clear.as32Bits = 0xFFFFFFFF;

	/* Interrupt must be cleared before reading rx fifo. */
	int_clear.as32Bits &= (~UART_INT_ANY_ERROR);
	if (!uartregisters[up->port.line]->interrupt_status.asField.cts_rising_edge)
		int_clear.asField.cts_rising_edge = 0;

	if (!uartregisters[up->port.line]->interrupt_status.asField.cts_falling_edge)
		int_clear.asField.cts_falling_edge = 0;

	uartregisters[up->port.line]->interrupt_clear.as32Bits = int_clear.as32Bits;
	uartregisters[up->port.line]->interrupt_mask.as32Bits = up->int_mask;

#if RUNTIME
	pm_runtime_mark_last_busy(up->port.dev);
	pm_runtime_put_autosuspend(up->port.dev);
#endif
}

static void odin_uart_enable_ms(struct uart_port *port)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);
#if 0
	UART_INTERRUPT int_mask;
	/* CTS interrupt triggering is falling edge,
	 * but Zenith's interrupt triggering is level.
	 * So CTS interrupt is disabled. */
	int_mask.as32Bits = 0;
	int_mask.asField.cts_risign_edge = 1;
	up->int_mask &= ~int_mask.as32Bits;
#endif
#if RUNTIME
	pm_runtime_get_sync(up->port.dev);
#endif
	uartregisters[up->port.line]->interrupt_mask.as32Bits = up->int_mask;

#if RUNTIME
	pm_runtime_mark_last_busy(up->port.dev);
	pm_runtime_put_autosuspend(up->port.dev);
#endif
}

static void odin_uart_break_ctl(struct uart_port *port, int break_state)
{
}

static int odin_uart_startup(struct uart_port *port)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);
	UART_FIFO_CONFIG fifo_config;
	UART_CONFIG config;
	UART_INTERRUPT int_mask;
	int ret = 0;

	if (up->b_startup) {
		pr_err(DRV_NAME ": %s: Already started up!!\n", up->name);
		goto out;
	}

	ret = odin_uart_clk_enable(up);
	if (ret){
		pr_err(DRV_NAME ": %s: Can't enable uart clock enable!!\n", up->name);
		goto out;
	}

	uartregisters[up->port.line]->interrupt_mask.as32Bits = 0xFFFFFFFF;
	uartregisters[up->port.line]->interrupt_clear.as32Bits = 0xFFFFFFFF;
#if WQ
	ret = request_threaded_irq(up->port.irq, odin_uart_irq, odin_uart_irq_work,0
											,up->name, port);
#else
	ret = request_irq(up->port.irq, odin_uart_irq, 0, up->name, up);
#endif
	if (ret) {
		pr_err(DRV_NAME ": %s: Can't get irq %d\n", up->name,
			up->port.irq);
		goto out;
	}
	fifo_config.as32Bits = 0;
	fifo_config.asField.rx_half_empty = 1;
	fifo_config.asField.tx_half_full = (up->port.fifosize / 2);
	uartregisters[up->port.line]->fifo_config.as32Bits = fifo_config.as32Bits;

	config.as32Bits = uartregisters[up->port.line]->config.as32Bits;
	config.asField.rx_fifo_size = UART_CONFIG_FIFO_SIZE_8BIT;
	config.asField.tx_fifo_size = UART_CONFIG_FIFO_SIZE_8BIT;
	config.asField.rx_dma_en = 0;
	config.asField.tx_dma_en = 0;

	config.asField.rts_fifo_level = 16;
	uartregisters[up->port.line]->config.as32Bits = config.as32Bits;

	up->cts_status = odin_uart_get_cts(up);
	up->stop_tx = 0;

	int_mask.as32Bits = 0;
	int_mask.asField.rx_fifo_half_full = 1;
	int_mask.asField.frame_error = 1;
	int_mask.asField.parity_error = 1;
	int_mask.asField.rx_push_error = 1;

	up->int_mask &= ~int_mask.as32Bits;

#ifdef CONFIG_DMA_ENGINE
	odin_uart_dma_probe(up);
	odin_uart_dma_startup(up);
#endif
	up->shutdown = false;
	up->b_startup = true;

	odin_uart_flush_fifo(up, 1, 1);
	uartregisters[up->port.line]->config.asField.uart_enable = 1;
	uartregisters[up->port.line]->interrupt_mask.as32Bits = up->int_mask;
	uartregisters[up->port.line]->interrupt_clear.as32Bits = 0xFFFFFFFF;
out:
	return ret;
}

static void odin_uart_shutdown(struct uart_port *port)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);
	unsigned long flag;

	spin_lock_irqsave(&up->port.lock, flag);
	up->b_startup = false;
	up->int_mask = 0xFFFFFFFF;
	uartregisters[up->port.line]->interrupt_mask.as32Bits = 0xFFFFFFFF;
	uartregisters[up->port.line]->interrupt_clear.as32Bits = 0xFFFFFFFF;
	up->shutdown = true;
	spin_unlock_irqrestore(&up->port.lock, flag);

#ifdef CONFIG_DMA_ENGINE
	odin_uart_dma_shutdown(up);
#endif
	free_irq(up->port.irq, up);
	printk(KERN_DEBUG"%s[%d]\n",__func__, up->port.line);

	odin_uart_flush_fifo(up, 1, 1);
	odin_uart_clk_disable(up);
	udelay(1);
}

static void odin_uart_set_termios(struct uart_port *port, struct ktermios *new,
			struct ktermios *old)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);
	UART_CONFIG uart_config;
	UART_MODE uart_mode;
	UART_INTERRUPT read_status_mask;
	UART_INTERRUPT ignore_status_mask;

	u32 baud;
	/* 20120130-1,syblue.lee,temporary apb clock, need to be fixed */
/*	u32 apb_clk_hz = UART_HS_FREQ; */

#ifdef CONFIG_COMMON_CLK
	struct clk *uart_peri_clk;
	unsigned int uart_clk;
	uart_peri_clk = up->clk;
	uart_clk = (unsigned int)(clk_get_rate(uart_peri_clk));
#endif
	
	uart_config.as32Bits = uartregisters[up->port.line]->config.as32Bits;
	uart_mode.as32Bits = uartregisters[up->port.line]->mode.as32Bits;

	/* set bits per char */
	switch (new->c_cflag & CSIZE) {
	case CS8:
		uart_mode.asField.rx_word_length = UART_MODE_WORD_LENGTH_8BIT;
		uart_mode.asField.tx_word_length = UART_MODE_WORD_LENGTH_8BIT;
		break;
	case CS7:
		uart_mode.asField.rx_word_length = UART_MODE_WORD_LENGTH_7BIT;
		uart_mode.asField.tx_word_length = UART_MODE_WORD_LENGTH_7BIT;
		break;
	case CS6:
		uart_mode.asField.rx_word_length = UART_MODE_WORD_LENGTH_6BIT;
		uart_mode.asField.tx_word_length = UART_MODE_WORD_LENGTH_6BIT;
		break;
	case CS5:
		uart_mode.asField.rx_word_length = UART_MODE_WORD_LENGTH_5BIT;
		uart_mode.asField.tx_word_length = UART_MODE_WORD_LENGTH_5BIT;
		break;
	default:
		uart_mode.asField.rx_word_length = UART_MODE_WORD_LENGTH_8BIT;
		uart_mode.asField.tx_word_length = UART_MODE_WORD_LENGTH_8BIT;
		pr_err(DRV_NAME ": %s: Invalied CSIZE: 0x%x\n",
			up->name, new->c_cflag & CSIZE);
		BUG();
	}

	/* set stop bit */
	if (new->c_cflag & CSTOPB)
		uart_config.asField.stop_bits = 1;
	else
		uart_config.asField.stop_bits = 0;

	/* set parity */
	if (new->c_cflag & PARENB) {
		if (new->c_cflag & PARODD)
			uart_config.asField.parity_type = UART_CONFIG_PARITY_ODD;
		else
			uart_config.asField.parity_type = UART_CONFIG_PARITY_EVEN;
	} else {
		uart_config.asField.parity_type = UART_CONFIG_PARITY_NO;
	}

	/* set baudrate */
	baud = uart_get_baud_rate(port, new, old,
			uart_clk / 0xFFFF,
			uart_clk / 8);

	/* set CTS and RTS */
	if (new->c_cflag & CRTSCTS) {
		uart_config.asField.rts_enable = 1;
		uart_config.asField.cts_enable = 1;
		uart_config.asField.rts_invert = 1;
		uart_config.asField.cts_invert = 1;
	} else {
		uart_config.asField.rts_enable = 0;
		uart_config.asField.cts_enable = 0;
		uart_config.asField.rts_invert = 1;
		uart_config.asField.cts_invert = 1;
	}


#if RUNTIME
	pm_runtime_get_sync(up->port.dev);
#endif
	/* CTS flow control flag and modem status interrupts */
	spin_lock_irqsave(&up->port.lock, up->port_lock_flag);

	/* update the per-port timout */
	uart_update_timeout(port, new->c_cflag, baud);

	/* configure read status bits */
	read_status_mask.as32Bits = 0;
	read_status_mask.asField.rx_push_error = 1;
	read_status_mask.asField.rx_fifo_half_full = 1;

	if (new->c_iflag & INPCK) {
		read_status_mask.asField.frame_error = 1;
		read_status_mask.asField.parity_error = 1;
	}
	up->port.read_status_mask = read_status_mask.as32Bits;

	/* configure ignore status bits */
	ignore_status_mask.as32Bits = 0;
	if (new->c_iflag & IGNPAR) {
		ignore_status_mask.asField.frame_error = 1;
		ignore_status_mask.asField.parity_error = 1;
	}

	/* ignore all characters */
	if ((new->c_cflag & CREAD) == 0)
		ignore_status_mask.asField.rx_fifo_half_full = 1;

	up->port.ignore_status_mask = ignore_status_mask.as32Bits;

	uartregisters[up->port.line]->config.as32Bits = uart_config.as32Bits;
	uartregisters[up->port.line]->mode.as32Bits = uart_mode.as32Bits;
	uartregisters[up->port.line]->interrupt_mask.as32Bits = up->int_mask;

	odin_uart_set_baudrate(up, baud, uart_clk);

	odin_uart_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, up->port_lock_flag);

#if RUNTIME
	pm_runtime_mark_last_busy(up->port.dev);
	pm_runtime_put_autosuspend(up->port.dev);
#endif

	dev_dbg(up->port.dev, "CSIZE=%s, CSTOPB=%d, PARENB=%s, CRTSCTS=%d, "
		"INPCK=%d, IGNPAR=%d, CREAD=%d, CLOCAL=%d, CBAUD=%u\n",
		new->c_cflag & CS8 ? "8" : (new->c_cflag & CS7 ? "7" :
			(new->c_cflag & CS6 ? "6" : (new->c_cflag & CS5 ? "5" :
			"UNKNOWN"))),
		new->c_cflag & CSTOPB ? 1 : 0,
		new->c_cflag & PARENB ?
			(new->c_cflag & PARODD ? "ODD" : "EVEN") : "NONE",
		new->c_cflag & CRTSCTS ? 1 : 0,
		new->c_iflag & INPCK ? 1 : 0,
		new->c_iflag & IGNPAR ? 1 : 0,
		new->c_cflag & CREAD ? 1 : 0,
		new->c_cflag & CLOCAL ? 1 : 0,
		baud);
}

static void odin_uart_pm(struct uart_port *port, unsigned int state,
			unsigned int oldstate)
{
}

static void odin_uart_release_port(struct uart_port *port)
{
}

static int odin_uart_request_port(struct uart_port *port)
{
	return 0;
}

static void odin_uart_config_port(struct uart_port *port, int flags)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);

	up->port.type = PORT_ODIN;
}

static int odin_uart_verify_port(struct uart_port *port,
				struct serial_struct *ser)
{
	int ret = 0;

	if ((ser->type != PORT_UNKNOWN) && (ser->type != PORT_ODIN)) {
		ret = -EINVAL;
		goto out;
	}

	if (port->irq != ser->irq) {
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

struct uart_ops odin_uart_ops = {
	.tx_empty	= odin_uart_tx_empty,
	.set_mctrl	= odin_uart_set_mctrl,
	.get_mctrl	= odin_uart_get_mctrl,
	.stop_tx	= odin_uart_stop_tx,
	.start_tx	= odin_uart_start_tx,
	.stop_rx	= odin_uart_stop_rx,
	.enable_ms	= odin_uart_enable_ms,
	.break_ctl	= odin_uart_break_ctl,
	.startup	= odin_uart_startup,
	.shutdown	= odin_uart_shutdown,
	.set_termios	= odin_uart_set_termios,
	.pm		= odin_uart_pm,
	.type		= odin_uart_type,
	.release_port	= odin_uart_release_port,
	.request_port	= odin_uart_request_port,
	.config_port	= odin_uart_config_port,
	.verify_port	= odin_uart_verify_port,
};

static int odin_uart_init_port(struct platform_device *pdev,
				struct odin_uart_port *up)
{
	struct odin_serial_platform_data *board_data;
	int ret = 0;
	int id = pdev->id;
	struct clk *clk;

#if CONFIG_OF
	struct resource res;
	int val = 0;
	board_data = (void *)kmalloc(sizeof(struct odin_serial_platform_data),
	GFP_KERNEL);

	if (!board_data) {
		ret = -ENXIO;
		goto out;
	}

	board_data->membase = (unsigned long)of_iomap(pdev->dev.of_node, 0);
	board_data->irq = irq_of_parse_and_map(pdev->dev.of_node,0);

	if (!of_property_read_u32(pdev->dev.of_node, "uartclk",&val))
		board_data->uartclk = val;

	if (of_address_to_resource(pdev->dev.of_node, 0, &res))
	{
		ret = -EINVAL;
		goto out;
	}

	board_data->mapbase = res.start;
#else
	board_data = pdev->dev.platform_data;
	if (!board_data) {
		ret = -ENXIO;
		goto out;
	}
#endif

	up->name = odin_drv_name[id];
	up->port.line = id;

	up->port.type = PORT_ODIN;
	up->port.iotype = UPIO_MEM;
	up->port.mapbase = board_data->mapbase;
	up->port.membase = (unsigned char __iomem *)board_data->membase;
	up->port.irq = board_data->irq;
	up->port.uartclk = board_data->uartclk;
	up->port.fifosize = 128;
	up->port.ops = &odin_uart_ops;
	up->port.dev = &pdev->dev;
	up->debug = board_data->debug;
	up->b_startup = false;

	clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		printk("clk_get error\n");
		ret = PTR_ERR(clk);
		goto out;
	}
	up->clk = clk;
	up->int_mask = 0xFFFFFFFF;
#if WQ
	odin_uart_wq = create_workqueue(up->name);

	if (odin_uart_wq == NULL) {
		return ENOMEM;
	}
#endif
	uartregisters[up->port.line] = (UART_REGISTERS*)up->port.membase;

	uartregisters[up->port.line]->interrupt_mask.as32Bits = 0xFFFFFFFF;
	uartregisters[up->port.line]->interrupt_clear.as32Bits = 0xFFFFFFFF;

	if (up->port.line != 0)
		odin_uart_clk_disable(up);

out:
	kfree(board_data);
	return ret;
}

static ssize_t uart_register_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	int ret = 0;
	struct odin_uart_port *up = container_of(kobj, struct odin_uart_port, kobj);

	printk(KERN_INFO"status : %x mask : %x fifo : %x mode : %x\n"
	,uartregisters[up->port.line]->interrupt_status.as32Bits
	,uartregisters[up->port.line]->interrupt_mask.as32Bits
	,uartregisters[up->port.line]->fifo_status.as32Bits
	,uartregisters[up->port.line]->mode.as32Bits);
	printk(KERN_INFO"baud: %x config: %x fifo_config: %x\n\n"
	,uartregisters[up->port.line]->baud_rate.as32Bits
	,uartregisters[up->port.line]->config.as32Bits
	,uartregisters[up->port.line]->fifo_config.as32Bits);

	return ret;
}
static ssize_t uart_register_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	struct odin_uart_port *up = container_of(kobj, struct odin_uart_port, kobj);
	struct uart_attr *u_attr = container_of(attr, struct uart_attr, attr);
	int offset = -1;
	int data = -1;
	char str[13];

	sscanf(buf, "%x %x", &offset, &data);

	switch (offset) {
		case 0x4:
			uartregisters[up->port.line]->interrupt_mask.as32Bits = data;
			strcpy(str,"INT mask");
			break;
		case 0x8:
			uartregisters[up->port.line]->interrupt_clear.as32Bits = data;
			strcpy(str,"INT clear");
			break;
		case 0xC:
			uartregisters[up->port.line]->baud_rate.as32Bits = data;
			strcpy(str,"baudrate");
			break;
		case 0x10:
			uartregisters[up->port.line]->config.as32Bits = data;
			strcpy(str,"config");
			break;
		case 0x14:
			uartregisters[up->port.line]->fifo_status.as32Bits = data;
			strcpy(str,"fifo status");
			break;
		case 0x18:
			uartregisters[up->port.line]->fifo_config.as32Bits = data;
			strcpy(str,"fifo config");
			break;
		case 0x1C:
			uartregisters[up->port.line]->mode.as32Bits = data;
			strcpy(str,"mode");
			break;
		default:
			printk(KERN_INFO"Input register offset(0x4) and data\n");
			goto out;
	}
	printk(KERN_INFO"%s) %s set 0x%x\n", __func__, str, data);
	out:
	return count;
}

static struct odin_uart_port odin_uart_ports[ODIN_MAX_UART];
static struct uart_driver odin_uart_driver;
DEVICE_ATTR(uart_register, 0664, uart_register_show, uart_register_store);

#ifdef CONFIG_SERIAL_ODIN_UART_CONSOLE
static void odin_uart_console_putchar(struct uart_port *port, int c)
{
	struct odin_uart_port *up = container_of(port, struct odin_uart_port, port);

	while (odin_uart_get_fifo_cnt(up, 0) < 1)
		continue;

	uartregisters[up->port.line]->tx_fifo = c;
}

/*
 * Print a string to the serial port trying not to disturb
 * any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void odin_uart_console_write(struct console *co, const char *s,
				unsigned int count)
{
	struct odin_uart_port *up = &odin_uart_ports[co->index];
	u32 int_mask;
#if 0 /* seong hoon */
	pm_runtime_get_sync(up->port.dev);
#endif

	uart_console_write(&up->port, s, count, odin_uart_console_putchar);

#if 0 /* seong hoon */
	pm_runtime_mark_last_busy(up->port.dev);
	pm_runtime_put_autosuspend(up->port.dev);
#endif
}

static int __init odin_uart_console_setup(struct console *co, char *options)
{
	struct odin_uart_port *up;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index == -1 || co->index >= odin_uart_driver.nr)
		co->index = 0;

	up = &odin_uart_ports[co->index];

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(&up->port, co, baud, parity, bits, flow);
}

static struct console odin_uart_console = {
	.name	= ODIN_UART_DEVICENAME,
	.write	= odin_uart_console_write,
	.device	= uart_console_device,
	.setup	= odin_uart_console_setup,
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
	.data	= &odin_uart_driver,
};

static int __init odin_uart_console_init(void)
{
/*
	if (odin_default_console_device) {
		add_preferred_console(ODIN_UART_DEVICENAME,
				odin_default_console_device->id, "115200n8");
		odin_uart_init_port(odin_default_console_device,
				&odin_uart_ports[odin_default_console_device->id]);
		register_console(&odin_uart_console);
	}
*/
	return 0;
}

console_initcall(odin_uart_console_init);

#define ODIN_UART_CONSOLE	(&odin_uart_console)
#else
#define ODIN_UART_CONSOLE	NULL
#endif /* CONFIG_SERIAL_ODIN_UART_CONSOLE */

static struct uart_driver odin_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= DRV_NAME,
	.dev_name	= ODIN_UART_DEVICENAME,
	.major		= TTY_MAJOR,
	.minor		= 64,
	.nr		= ODIN_MAX_UART,
	.cons		= ODIN_UART_CONSOLE,
};
static struct sysfs_ops uart_module_ops = {
	.show = uart_register_show,
	.store = uart_register_store,
};

static struct kobj_type uart_type = {
	.sysfs_ops = &uart_module_ops,
	.default_attrs = uartattr,
};

static int odin_uart_probe(struct platform_device *pdev)
{
	int ret;
	char name[11];

#if CONFIG_OF
	if (pdev->dev.of_node)
		pdev->id = of_alias_get_id(pdev->dev.of_node, "serial");

	if (pdev->id < 0 || pdev->id >= ODIN_MAX_UART)
	{
		ret = -EINVAL;
		goto out;
	}
#endif
	ret = odin_uart_init_port(pdev, &odin_uart_ports[pdev->id]);
	if (ret)
		goto out;

	ret = uart_add_one_port(&odin_uart_driver, &odin_uart_ports[pdev->id].port);
	if (ret)
		goto out;

	platform_set_drvdata(pdev, &odin_uart_ports[pdev->id]);

	ret = device_create_file(&pdev->dev, &dev_attr_uart_register);
	if (ret < 0) {
		pr_err("%s: failed to create sysfs uart register\n",__func__);
		goto err_create_sysfs;
	}
	sprintf(name,"uart%d",pdev->id);
	kobject_init(&odin_uart_ports[pdev->id].kobj, &uart_type);
	if (kobject_add(&odin_uart_ports[pdev->id].kobj, NULL,name)) {
		ret = -1;
		printk(KERN_WARNING"UART sysfs creation failed\n");
		kobject_put(&odin_uart_ports[pdev->id].kobj);
	}


#if RUNTIME
	/* RuntimePM is not used in console and bluethooth */
	if (pdev->id !=0 && pdev->id != 1) {
		pm_runtime_enable(&pdev->dev);
		pm_runtime_use_autosuspend(&pdev->dev);
		pm_runtime_set_autosuspend_delay(&pdev->dev, 1000);
		pm_runtime_irq_safe(&pdev->dev);
		pm_runtime_get_sync(&pdev->dev);

		pm_runtime_mark_last_busy(&pdev->dev);
		pm_runtime_put_autosuspend(&pdev->dev);
	}
#endif

out:
	return ret;
err_create_sysfs:
	return ret;
}

static int odin_uart_remove(struct platform_device *dev)
{
	struct odin_uart_port *sport = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);

	if (sport) {
#if RUNTIME
		pm_runtime_put_sync(sport->port.dev);
		pm_runtime_disable(sport->port.dev);
#endif
		device_remove_file(&dev->dev, &dev_attr_uart_register);
		uart_remove_one_port(&odin_uart_driver, &sport->port);
	}
	return 0;
}

static int odin_uart_suspend(struct platform_device *dev, pm_message_t state)
{
	struct odin_uart_port *sport = platform_get_drvdata(dev);

	if (sport)
		uart_suspend_port(&odin_uart_driver, &sport->port);

	return 0;
}

static int odin_uart_resume(struct platform_device *dev)
{
	struct odin_uart_port *sport = platform_get_drvdata(dev);

	if (sport)
		uart_resume_port(&odin_uart_driver, &sport->port);

	return 0;
}
#if RUNTIME
#if defined(CONFIG_PM_RUNTIME)
static int odin_uart_runtime_suspend(struct device *dev)
{
	struct odin_uart_port *up = dev_get_drvdata(dev);
	int ret = 0;

	ret = odin_uart_clk_disable(up);
	return ret;
}
static int odin_uart_runtime_resume(struct device *dev)
{
	struct odin_uart_port *up = dev_get_drvdata(dev);
	int ret = 0;

	ret = odin_uart_clk_enable(up);
	return ret;
}
#else
#define odin_uart_runtime_suspend	NULL
#define odin_uart_runtime_resume	NULL
#endif

static struct dev_pm_ops odin_uart_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(odin_uart_runtime_suspend, odin_uart_runtime_resume, NULL)
};
#endif
#ifdef CONFIG_OF
static struct of_device_id odin_uart_match[] = {
	{ .compatible = "mtekvision,mvs508"},
	{},
};
#endif /* CONFIG_OF */
static struct platform_driver odin_uart_platform_driver = {
	.probe          = odin_uart_probe,
	.remove         = odin_uart_remove,
	.suspend	= odin_uart_suspend,
	.resume		= odin_uart_resume,
	.driver		= {
		.name	= DRV_NAME,
		#if 0
		.pm = &odin_uart_dev_pm_ops,
		#endif
		.of_match_table	= of_match_ptr(odin_uart_match),
	},
};

int __init odin_uart_init(void)
{
	int ret;

	ret = uart_register_driver(&odin_uart_driver);
	if (ret != 0)
		goto out;

	ret = platform_driver_register(&odin_uart_platform_driver);
	if (ret != 0)
		goto out_uart_unregister;

	goto out;

out_uart_unregister:
	uart_unregister_driver(&odin_uart_driver);
out:
	return ret;
}

void __exit odin_uart_exit(void)
{
	platform_driver_unregister(&odin_uart_platform_driver);
	uart_unregister_driver(&odin_uart_driver);
}

module_init(odin_uart_init);
module_exit(odin_uart_exit);

MODULE_LICENSE("GPL");
