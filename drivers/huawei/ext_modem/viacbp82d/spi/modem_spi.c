/*
 * drivers/mmc/card/modem_spi.c
 *
 * VIA CBP SPI driver for Linux
 *
 * Copyright (C) 2009 VIA TELECOM Corporation, Inc.
 * Author: VIA TELECOM Corporation, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/circ_buf.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/kfifo.h>
#include <linux/slab.h>

#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/hw_log.h>
#include "modem_spi.h"
#include "../viatel.h"

static int spi_tx_cnt = 0;
static int spi_rx_cnt = 0;

#define SPI_WAKEUP_CHARS		8*256

#define TRANSMIT_SHIFT	(10)
#define TRANSMIT_BUFFER_SIZE	(1UL << TRANSMIT_SHIFT)
#define TRANSMIT_MAX_SIZE     ((1UL << TRANSMIT_SHIFT)  - 4)

#define VIA_SPI_SPEED      (12000000)
#define VIA_SPI_PACKET_HEADER_SIZE         (4)
#define VIA_SPI_START_BUF_SIZE         (60)
#define VIA_SPI_FIFO_SIZE         (512)
//for SPI FIFO size
//#define FIFO_SIZE	(VIA_SPI_FIFO_SIZE - VIA_SPI_PACKET_HEADER_SIZE)
#define FIFO_SIZE	8*PAGE_SIZE   //PAGE_SIZE is 4096

static char *via_spi_start_buf;
static char *via_spi_follow_buf;
static int remain_packet_len = 0;
static int msg_buf_pos = 0;
//static int transmit_fifo_pos = 0;


static struct tty_driver *modem_spi_tty_driver;
static struct cbp_platform_data *cbp_pdata;
static struct spi_modem_port *spi_modem_table[SPI_TTY_NR];
static DEFINE_SPINLOCK(spi_modem_table_lock);

#define SPI_AT_CHANNEL_NUM			4
extern unsigned char cbp_power_state;
static struct spi_modem *via_modem = NULL;

static unsigned int dtr_value = 0;
static unsigned int dcd_state = 0;

static struct work_struct dtr_work;
static struct work_struct dcd_query_work;

static unsigned int modem_remove = 1;
static spinlock_t modem_remove_lock;

static void spi_read_port_work(struct work_struct *work);

/**
 * spi_tx_rx_printk - print spi tx and rx data, when log level is LOG_NOTICE or larger
 * @buf: the point of data buffer
 * @type: print type, 0:rx  1:tx
 *
 * no return
 */
static void spi_tx_rx_printk(const void *buf, unsigned char type)
{
	unsigned int count;
	const unsigned char *print_buf = (const unsigned char *)buf;
	int i;

	count =(((*(print_buf+2) & 0x0F) << 8) | (*(print_buf+3) & 0xFF));
	if(type == 1){
		hwlog_info("[MODEM SPI] write %d to channel%d/[%d]>>",
				count, *(print_buf+1), spi_tx_cnt);
	}
	else{
		hwlog_info("[MODEM SPI] read %d from channel%d/[%d]<<",
				count, *(print_buf+1), spi_rx_cnt);
	}

	if(count > 20)
		count = 20;
	for(i = 0; i < count + 4; i++)
	{
		hwlog_info("%x-", *(print_buf+i));
	}
	hwlog_info("\n");
}

void modem_dtr_send(struct work_struct *work);

static struct spi_modem_port *spi_modem_port_get(unsigned index)
{
	struct spi_modem_port *port;
	unsigned long flags = 0;

	if (index >= SPI_TTY_NR)
		return NULL;

	spin_lock_irqsave(&spi_modem_table_lock, flags);
	port = spi_modem_table[index];
	spin_unlock_irqrestore(&spi_modem_table_lock, flags);

	return port;
}
static void spi_modem_port_destroy(struct kref *kref)
{
	struct spi_modem_port *port = container_of(kref, struct spi_modem_port, kref);
	int index;

	if(port){
		index = port->index;
		hwlog_info("%s %d: index = %d .\n", __func__, __LINE__, index);
		kfifo_free(&port->transmit_fifo);
		kfree(port);
	}else{
		hwlog_err("%s %d: invalid port.\n", __func__, __LINE__);
	}
}

static void spi_modem_port_put(struct spi_modem_port *port)
{
	if(port){
		hwlog_info("%s %d: port %d.\n", __func__, __LINE__, port->index);
		kref_put(&port->kref, spi_modem_port_destroy);
	}else{
		hwlog_err("%s %d: invalid port.\n", __func__, __LINE__);
	}
}

static int check_port(struct spi_modem_port *port) {
	if (!port || !port->spi || (cbp_power_state ==0)){
		hwlog_err("%s %d: cbp_power_state=%d.\n", __func__, __LINE__,cbp_power_state);
		return -ENODEV;
	}
	//WARN_ON(!port->port.count);
	return 0;
}

static void modem_spi_write(struct spi_modem *modem, int addr,
		void *buf, size_t len);

/* CBP control message type */
enum cbp_contrl_message_type {
	CHAN_ONOFF_MSG_ID = 0,
	MDM_STATUS_IND_MSG_ID,
	MDM_STATUS_QUERY_MSG_ID,
	CHAN_SWITCH_REQ_MSG_ID,
	CHAN_STATUS_QUERY_MSG_ID,
	FLOW_CONTROL_MSG_ID,
	CHAN_LOOPBACK_TST_MSG_ID,
	MESSAGE_COUNT,
};

typedef enum {
	OPT_LOOPBACK_NON  = 0,		//no operation, default 0
	OPT_LOOPBACK_OPEN  = 1,		//open loopback test
	OPT_LOOPBACK_CLOSE  = 2,		//close loopback test
	OPT_LOOPBACK_QUERY = 3,		//query loopback test
	OPT_LOOPBACK_NUM
}IOP_OPT_LOOPBACK;

typedef enum {
	RSLT_LOOPBACK_SUCCESS  = 0,	//operation succed
	RSLT_LOOPBACK_WORK = 1,		//loopback testing
	RSLT_LOOPBACK_CLOSED = 2,		//no loopback test
	RSLT_LOOPBACK_INVALID = 3,	//invalid operation
	RSLT_LOOPBACK_FAIL = 4,		//loopback test failed
	RSLT_LOOPBACK_NUM
}IOP_RSLT_LOOPBACK;

static void respond_cflag80_packet_work(struct work_struct *work);
static void spi_write_port_work(struct work_struct *work);


int respond_cflag80_packet_to_via_request_to_send(void)
{
	struct spi_modem *modem;
	struct spi_modem_port *port;
	int ret = 0;

	hwlog_debug("%s: enter\n", __func__);

	port = spi_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d check_port failed\n", __func__,__LINE__);
		goto down_out;
	}
	modem = port->modem;

	queue_work(port->respond_cflag80_q, &port->respond_cflag80_work);

down_out:
	return 0;
}
EXPORT_SYMBOL(respond_cflag80_packet_to_via_request_to_send);

int modem_data_ack_handle(void)
{
	struct spi_modem *modem;
	struct spi_modem_port *port;
	int ret = 0;

	hwlog_info("%s: enter\n", __func__);
	port = spi_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d check_port failed\n", __func__,__LINE__);
		goto down_out;
	}
	modem = port->modem;

	if (SPI_STATE_RX == modem->spi_state) {
		queue_work(port->read_q, &port->read_work);
	} else if (SPI_STATE_IDLE== modem->spi_state) {
	  //IDLE can not has ack interrupt
	  hwlog_err("%s %d ack interrupt unexcepted\n", __func__,__LINE__);
	}
down_out:
	return 0;
}
EXPORT_SYMBOL(modem_data_ack_handle);


static int contruct_ctrl_chan_msg(struct spi_modem_ctrl_port *ctrl_port , int msg,
		unsigned char chan_num, unsigned char opt)
{
	if (unlikely(ctrl_port == NULL)){
		hwlog_err("%s %d: control channel is null.\n", __func__, __LINE__);
		return -EINVAL;
	}

	ctrl_port->chan_ctrl_msg.head.start_flag	= 0xFE;
	ctrl_port->chan_ctrl_msg.head.chanInfo	= 0;
	ctrl_port->chan_ctrl_msg.head.tranHi		= 0;			/*High byte of the following payload length*/
	ctrl_port->chan_ctrl_msg.head.tranLow	= 4;			/*Low byte of the following payload length*/
	ctrl_port->chan_ctrl_msg.id_hi			= msg >> 8;/*High byte of control message ID,for onoff request ID=CHAN_ONOFF_MSG_ID*/
	ctrl_port->chan_ctrl_msg.id_low			= msg;/*Low byte of control message ID,for onoff request ID=CHAN_ONOFF_MSG_ID*/
	ctrl_port->chan_ctrl_msg.chan_num		= chan_num;			/*ChanNum ,same as ChanInfo*/
	ctrl_port->chan_ctrl_msg.option			= opt;

	return 0;
}
static unsigned char loop_back[12];

int modem_on_off_ctrl_chan(unsigned char on)
{
	struct spi_modem *modem;
	struct spi_modem_port *port;
	struct spi_modem_ctrl_port *ctrl_port;
	unsigned char msg_len = 0;
	int ret = 0;

	hwlog_info("%s: enter, on = %d \n", __func__, on);
	port = spi_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d check_port failed\n", __func__,__LINE__);
		goto down_out;
	}
	modem = port->modem;
	ctrl_port = modem->ctrl_port;
	wait_event(ctrl_port->sflow_ctrl_wait_q, (SFLOW_CTRL_DISABLE == atomic_read(&ctrl_port->sflow_ctrl_state)||(cbp_power_state==0)));
	if(down_interruptible(&modem->sem)){
		hwlog_err("%s %d down_interruptible failed.\n", __func__,__LINE__);
		ret =  -ERESTARTSYS;
		goto down_out;
	}

	ret = contruct_ctrl_chan_msg(ctrl_port, CHAN_ONOFF_MSG_ID, 0, on);
	if (ret < 0){
		hwlog_err("%s contruct_ctrl_chan_msg failed\n", __func__);
		goto up_sem;
	}
	msg_len = sizeof(struct ctrl_port_msg);
	msg_len = (msg_len + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
	modem_spi_write(modem, 0x00, &(ctrl_port->chan_ctrl_msg), msg_len);

up_sem:
	up(&modem->sem);
down_out:
	return ret;
}
EXPORT_SYMBOL(modem_on_off_ctrl_chan);

void modem_dtr_set(int on, int low_latency)
{
	unsigned long flags;

	spin_lock_irqsave(&modem_remove_lock, flags);
	if(!modem_remove) {
		spin_unlock_irqrestore(&modem_remove_lock, flags);
		dtr_value = on;
		if(low_latency)
			modem_dtr_send(&dtr_work);
		else
			schedule_work(&dtr_work);
	}
	else{
		spin_unlock_irqrestore(&modem_remove_lock, flags);
	}
}
EXPORT_SYMBOL(modem_dtr_set);

void modem_dtr_send(struct work_struct *work)
{
	struct spi_modem *modem;
	struct spi_modem_port *port;
	struct spi_modem_ctrl_port *ctrl_port;
	unsigned char control_signal=0;
	unsigned char msg_len = 0;
	int ret = 0;
	unsigned int on = 0;

	on = dtr_value;
	hwlog_info("%s: enter, on =%d\n", __func__, on);
	port = spi_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d check_port failed\n", __func__,__LINE__);
		goto down_out;
	}
	modem = port->modem;
	ctrl_port = modem->ctrl_port;
	wait_event(ctrl_port->sflow_ctrl_wait_q, (SFLOW_CTRL_DISABLE == atomic_read(&ctrl_port->sflow_ctrl_state)||(cbp_power_state==0)));
	if(down_interruptible(&modem->sem)){
		hwlog_err("%s %d down_interruptible failed.\n", __func__,__LINE__);
		ret =  -ERESTARTSYS;
		goto down_out;
	}

	if(ctrl_port->chan_state == 1){
		if(on){
			control_signal |= 0x04;
		}
		else{
			control_signal &= 0xFB;
		}

		ret = contruct_ctrl_chan_msg(ctrl_port, MDM_STATUS_IND_MSG_ID, 2, control_signal);
		if (ret < 0){
			hwlog_err("%s contruct_ctrl_chan_msg failed\n", __func__);
			goto up_sem;
		}
		msg_len = sizeof(struct ctrl_port_msg);
		msg_len = (msg_len + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
		modem_spi_write(modem, 0x00, &(ctrl_port->chan_ctrl_msg), msg_len);
	}
	else{
		ret = -1;
		hwlog_err("%s: ctrl channel is off, please turn on first\n", __func__);
	}

up_sem:
	up(&modem->sem);
down_out:
	return;
}

int modem_dcd_state(void)
{
	unsigned long flags;

	spin_lock_irqsave(&modem_remove_lock, flags);
	if(!modem_remove) {
		spin_unlock_irqrestore(&modem_remove_lock, flags);
		schedule_work(&dcd_query_work);
	} else {
		spin_unlock_irqrestore(&modem_remove_lock, flags);
		dcd_state = 0;
	}
	return dcd_state;
}
EXPORT_SYMBOL(modem_dcd_state);

void modem_dcd_query(struct work_struct *work)
{
	struct spi_modem *modem;
	struct spi_modem_port *port;
	struct spi_modem_ctrl_port *ctrl_port;
	unsigned char msg_len = 0;
	int ret = 0;

	hwlog_info("%s: enter\n", __func__);
	port = spi_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d check_port failed\n", __func__,__LINE__);
		goto down_out;
	}
	modem = port->modem;
	ctrl_port = modem->ctrl_port;

	if(ctrl_port->chan_state == 1){
		wait_event(ctrl_port->sflow_ctrl_wait_q, (SFLOW_CTRL_DISABLE == atomic_read(&ctrl_port->sflow_ctrl_state)||(cbp_power_state==0)));
		if(down_interruptible(&modem->sem)){
			hwlog_err("%s %d down_interruptible failed.\n", __func__,__LINE__);
			ret =  -ERESTARTSYS;
			goto down_out;
		}

		ret = contruct_ctrl_chan_msg(ctrl_port, MDM_STATUS_QUERY_MSG_ID, 2, 0);
		if (ret < 0){
			hwlog_err("%s contruct_ctrl_chan_msg failed\n", __func__);
			goto up_sem;
		}
		msg_len = sizeof(struct ctrl_port_msg);
		msg_len = (msg_len + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
		modem_spi_write(modem, 0x00, &(ctrl_port->chan_ctrl_msg), msg_len);
up_sem:
		up(&modem->sem);
		msleep(10);
		dcd_state = port->dtr_state;
	}
	else{
		dcd_state = 0;
		hwlog_err("%s: ctrl channel is off, please turn on first\n", __func__);
	}
down_out:
	return;
}

int modem_loop_back_chan(unsigned char chan_num, unsigned char opt)
{
	struct spi_modem *modem;
	struct spi_modem_port *port;
	struct spi_modem_ctrl_port *ctrl_port;
	unsigned char msg_len = 0;
	int ret = 0;

	hwlog_info("%s: enter\n", __func__);
	port = spi_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d check_port failed\n", __func__,__LINE__);
		goto down_out;
	}
	modem = port->modem;
	ctrl_port = modem->ctrl_port;
	wait_event(ctrl_port->sflow_ctrl_wait_q, (SFLOW_CTRL_DISABLE == atomic_read(&ctrl_port->sflow_ctrl_state)||(cbp_power_state==0)));
	if(down_interruptible(&modem->sem)){
		hwlog_err("%s %d down_interruptible failed.\n", __func__,__LINE__);
		ret =  -ERESTARTSYS;
		goto down_out;
	}

	if(ctrl_port->chan_state == 1){
		loop_back[0]		= 0xFE;
		loop_back[1]		= 0;
		loop_back[2]		= 0;			/*High byte of the following payload length*/
		loop_back[3]		= 6;			/*Low byte of the following payload length*/
		loop_back[4]		= 0x00;		/*High byte of control message ID,for onoff request ID=CHAN_ONOFF_MSG_ID*/
		loop_back[5]		= 0x06;		/*Low byte of control message ID,for onoff request ID=CHAN_ONOFF_MSG_ID*/
		loop_back[6]		= 3;			/*device id spi = 3*/
		loop_back[7]		= opt;
		loop_back[8]		= chan_num;/*ChanNum ,same as ChanInfo*/
		loop_back[9]		= 0;

		msg_len = 12;
		msg_len = (msg_len + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
		modem_spi_write(modem, 0x00, &(loop_back[0]), msg_len);
	}
	else{
		ret = -1;
		hwlog_err("%s: ctrl channel is off, please turn on first\n", __func__);
	}
	up(&modem->sem);
down_out:
	return ret;
}
EXPORT_SYMBOL(modem_loop_back_chan);

static int ctrl_msg_analyze(struct spi_modem *modem)
{
	struct spi_modem_ctrl_port *ctrl_port;
	const unsigned char msg_id_high 	= *modem->msg->buffer;
	const unsigned char msg_id_low 	= *(modem->msg->buffer+1);
	const unsigned int msg_id 			= (msg_id_high << 8) + msg_id_low;
	unsigned char option=*(modem->msg->buffer+3);
	struct spi_modem_port *port;
	unsigned char chan_num;
	unsigned char res;

	ctrl_port = modem->ctrl_port;

	switch(msg_id)
	{
		case CHAN_ONOFF_MSG_ID:
			if(option == 1){
				ctrl_port->chan_state = 1;
				hwlog_info("%s: ctrl channel is open\n", __func__);
			}
			else if(option == 0){
				ctrl_port->chan_state = 0;
				hwlog_info("%s: ctrl channel is close\n", __func__);
			}
			else{
				hwlog_err("%s: err option value = %d\n",
						__func__, option);
			}
		case MDM_STATUS_IND_MSG_ID:
			port = modem->port[0];
			if(option & 0x80)/*connetc*/
			{
				port->dtr_state = 1;
			}
			else/*disconnetc*/
			{
				port->dtr_state = 0;
			}
			break;
		case MDM_STATUS_QUERY_MSG_ID:
			port = modem->port[0];
			if(option & 0x80)/*connetc*/
			{
				port->dtr_state = 1;
			}
			else/*disconnetc*/
			{
				port->dtr_state = 0;
			}
			/*to be contionue*/
			break;
		case CHAN_LOOPBACK_TST_MSG_ID:
			{
				chan_num = *(modem->msg->buffer+4);
				res = *(modem->msg->buffer+5);
				if(option == OPT_LOOPBACK_OPEN)/*open*/
				{
					hwlog_info("%s %d: open chan %d, result = %d\n",
							__func__, __LINE__,chan_num, res);
				}
				else if(option == OPT_LOOPBACK_CLOSE)/*close*/
				{
					hwlog_info("%s %d: close chan %d, result = %d\n",
							__func__, __LINE__,chan_num, res);
				}
				else if(option == OPT_LOOPBACK_QUERY)/*close*/
				{
					hwlog_info("%s %d: query chan %d, result = %d\n",
							__func__, __LINE__,chan_num, res);
				}
				else
				{
					hwlog_err("%s %d: unknow option %d\n", __func__, __LINE__, option);
				}
			}
			break;
		case FLOW_CONTROL_MSG_ID:
			chan_num = *(modem->msg->buffer+2);
			if(chan_num > 0 && chan_num < (SPI_TTY_NR + 1)){
				chan_num = chan_num - 1;
				port = modem->port[chan_num];
				if(option == 1){
					hwlog_debug("%s %d: channel%d soft flow ctrl enable!\n", __func__, __LINE__, (port->index + 1));
					atomic_set(&port->sflow_ctrl_state, SFLOW_CTRL_ENABLE);
				}
				else if(option == 0){
					hwlog_debug("%s %d: channel%d soft flow ctrl disable!\n", __func__, __LINE__, (port->index + 1));
					atomic_set(&port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
					wake_up(&port->sflow_ctrl_wait_q);
				}
			}
			else if(chan_num == 0){
				if(option == 1){
					hwlog_debug("%s %d: ctrl channel soft flow ctrl enable!\n", __func__, __LINE__);
					atomic_set(&ctrl_port->sflow_ctrl_state, SFLOW_CTRL_ENABLE);
				}
				else if(option == 0){
					hwlog_debug("%s %d: ctrl channel soft flow ctrl disable!\n", __func__, __LINE__);
					atomic_set(&ctrl_port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
					wake_up(&ctrl_port->sflow_ctrl_wait_q);
				}
			}else{
				hwlog_err("%s %d: unkown channel num%d!\n", __func__, __LINE__, chan_num);
			}
			break;
		case CHAN_SWITCH_REQ_MSG_ID:
			/*to be contionue*/
			break;
		case CHAN_STATUS_QUERY_MSG_ID:
			/*to be contionue*/
			break;
		default:
			hwlog_err("%s %d: unknow control message received\n", __func__, __LINE__);
			goto err_wrong_format;
	}
	return 0;

err_wrong_format:
	return -1;
}

static void spi_buffer_in_print(struct spi_modem_port *port, struct spi_buf_in_packet *packet)
{
	unsigned int count;
	int i;

	hwlog_info("[MODEM SPI] spi channel%d buffer in %d bytes data<<", (port->index+1), packet->size);
	count = packet->size;
	if( count > 20){
		count = 20;
	}
	for(i = 0; i < count; i++)
	{
		hwlog_info("%x-", *(packet->buffer+i));
	}
	hwlog_info("\n");
}

static void spi_buf_in_tty_work(struct spi_modem_port *port)
{
	struct spi_buf_in_packet *packet = NULL;
	struct tty_struct *tty;
	int room;

	tty = tty_port_tty_get(&port->port);
	if(tty){
		while(!list_empty(&port->spi_buf_in_list)){
			packet = list_first_entry(&port->spi_buf_in_list, struct spi_buf_in_packet, node);
			room = tty_buffer_request_room(&port->port, packet->size);
			if(room < packet->size){
				hwlog_err("%s %d: no room in tty rx buffer!\n", __func__,__LINE__);
			}
			else{
				room = tty_insert_flip_string(&port->port, packet->buffer, packet->size);
				if(room < packet->size){
					hwlog_err("%s %d: couldn't insert all characters (TTY is full?)!\n", __func__,__LINE__);
				}
				else{
					tty_flip_buffer_push(&port->port);
				}
			}

			spi_buffer_in_print(port, packet);

			list_del(&packet->node);
			if(packet){
				port->spi_buf_in_size -= packet->size;
				kfree(packet->buffer);
				kfree(packet);
			}
			port->spi_buf_in_num--;
		}
	}
	tty_kref_put(tty);
}

/*****************************************************************************
 * tty driver interface functions
 *****************************************************************************/
/**
 *	spi_uart_install	-	install method
 *	@driver: the driver in use (spi_uart in our case)
 *	@tty: the tty being bound
 *
 *	Look up and bind the tty and the driver together. Initialize
 *	any needed private data (in our case the termios)
 */

static int modem_tty_install(struct tty_driver *driver, struct tty_struct *tty)
{
	struct spi_modem_port *port ;
	int idx = tty->index;
	int ret;

	port = spi_modem_port_get(idx);
	hwlog_info("%s %d: port %d.\n", __func__, __LINE__, port->index);
	if (!port) {
		tty->driver_data = NULL;
		hwlog_err("%s %d can't find spi modem port.\n", __func__, __LINE__);
		return -ENODEV;
	}

	kref_get(&port->kref);

	ret = tty_init_termios(tty);
	if (ret)
		return ret;

	tty_driver_kref_get(driver);
	tty->count++;
	driver->ttys[tty->index] = tty;

	if (ret == 0)
		/* This is the ref spi_uart_port get provided */
		tty->driver_data = port;
	else
		spi_modem_port_put(port);
	return ret;
}

/**
 *	spi_uart_cleanup	-	called on the last tty kref drop
 *	@tty: the tty being destroyed
 *
 *	Called asynchronously when the last reference to the tty is dropped.
 *	We cannot destroy the tty->driver_data port kref until this point
 */

static void modem_tty_cleanup(struct tty_struct *tty)
{
	struct spi_modem_port *port = tty->driver_data;
	tty->driver_data = NULL;	/* Bug trap */
	if(port){
		hwlog_info("%s %d: port %d.\n", __func__, __LINE__, port->index);
		spi_modem_port_put(port);
	}else{
		hwlog_info("%s %d: invalid port.\n", __func__, __LINE__);
	}
}

static int modem_tty_open(struct tty_struct *tty, struct file *filp)
{
	struct spi_modem_port *port = tty->driver_data;
	hwlog_info("%s %d: port %d.\n", __func__, __LINE__, port->index);
	return tty_port_open(&port->port, tty, filp);
}

static void modem_tty_close(struct tty_struct *tty, struct file * filp)
{
	struct spi_modem_port *port = tty->driver_data;

	hwlog_info("%s %d: port %d.\n", __func__, __LINE__, port->index);
	tty_port_close(&port->port, tty, filp);
}

static void modem_tty_hangup(struct tty_struct *tty)
{
	struct spi_modem_port *port = tty->driver_data;
	hwlog_info("%s %d: port %d.\n", __func__, __LINE__, port->index);
	tty_port_hangup(&port->port);
}

static int modem_tty_write(struct tty_struct *tty, const unsigned char *buf,
		int count)
{
	struct spi_modem_port *port = tty->driver_data;
	int ret=0;

	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d check_port failed\n", __func__,__LINE__);
		return ret;
	}

	if(port->inception){
		hwlog_err("%s %d port is inception\n", __func__,__LINE__);
		return -EBUSY;
	}

	if(count > FIFO_SIZE){
		hwlog_err("%s %d FIFO size is not enough!\n", __func__,__LINE__);
		return -1;
	}

	if(down_interruptible(&port->modem->spi_sem)){
		hwlog_err("%s %d down_interruptible failed.\n", __func__,__LINE__);
		ret =  -ERESTARTSYS;
		goto down_out;
	}
	hwlog_debug("modem_tty_write got semaphore.\n");

	ret = kfifo_in_locked(&port->transmit_fifo, buf, count, &port->write_lock);

	spi_write_port_work(&port->write_work);

	up(&port->modem->spi_sem);
	hwlog_debug("modem_tty_write release semaphore.\n");

down_out:

	hwlog_debug("%s %d: port%d\n", __func__,__LINE__, port->index);

	return ret;
}

static int modem_tty_write_room(struct tty_struct *tty)
{
	struct spi_modem_port *port = tty->driver_data;
	unsigned long flags = 0;
	unsigned int data_len = 0;
	unsigned int ret;

	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d check_port failed\n", __func__,__LINE__);
		return ret;
	}

	spin_lock_irqsave(&port->write_lock, flags);
	data_len = FIFO_SIZE - kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

	hwlog_debug("%s %d: port %d free size %d.\n", __func__, __LINE__, port->index, data_len);
	return data_len;
}

static int modem_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct spi_modem_port *port = tty->driver_data;
	unsigned long flags = 0;
	unsigned int data_len = 0;
	unsigned int ret;

	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d ret=%d\n", __func__,__LINE__,ret);
		return ret;
	}

	spin_lock_irqsave(&port->write_lock, flags);
	data_len = kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

	hwlog_debug("%s %d: port %d chars in buffer %d.\n", __func__, __LINE__, port->index, data_len);
	return data_len;
}

static void modem_tty_set_termios(struct tty_struct *tty,
		struct ktermios *old_termios)
{
	struct spi_modem_port *port = tty->driver_data;
	int ret;

	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d ret=%d\n", __func__,__LINE__,ret);
		return ;
	}

	tty_termios_copy_hw(&(tty->termios), old_termios);
}

static int modem_tty_tiocmget(struct tty_struct *tty)
{
	struct spi_modem_port *port = tty->driver_data;
	int ret;

	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d ret=%d\n", __func__,__LINE__,ret);
		return ret;
	}
	return 0;
}

static int modem_tty_tiocmset(struct tty_struct *tty,
		unsigned int set, unsigned int clear)
{
	struct spi_modem_port *port = tty->driver_data;
	int ret;

	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d ret=%d\n", __func__,__LINE__,ret);
		return ret;
	}
	return 0;
}

static int spi_modem_activate(struct tty_port *tport, struct tty_struct *tty)
{
	struct spi_modem_port *port = NULL;

	hwlog_info("%s %d: enter.\n", __func__, __LINE__);
	port = container_of(tport, struct spi_modem_port, port);

	kfifo_reset(&port->transmit_fifo);
	mutex_lock(&port->spi_buf_in_mutex);
	if (port->spi_buf_in == 1){
		spi_buf_in_tty_work(port);
		port->spi_buf_in = 0;
	}
	mutex_unlock(&port->spi_buf_in_mutex);
	hwlog_info("%s %d: Leave.\n", __func__, __LINE__);
	return 0;
}

static void spi_modem_shutdown(struct tty_port *tport)
{
	struct spi_modem_port *port = NULL;
	struct spi_buf_in_packet *packet = NULL;

	hwlog_info("%s %d: enter.\n", __func__, __LINE__);
	port = container_of(tport, struct spi_modem_port, port);
	mutex_lock(&port->spi_buf_in_mutex);
	while(!list_empty(&port->spi_buf_in_list)){
		packet = list_first_entry(&port->spi_buf_in_list, struct spi_buf_in_packet, node);
		list_del(&packet->node);
		if(packet){
			kfree(packet->buffer);
			kfree(packet);
		}
	}
	mutex_unlock(&port->spi_buf_in_mutex);
	hwlog_info("%s %d: Leave.\n", __func__, __LINE__);
}


static const struct tty_port_operations spi_modem_port_ops = {
	.shutdown = spi_modem_shutdown,
	.activate = spi_modem_activate,
};

static const struct tty_operations modem_tty_ops = {
	.open			= modem_tty_open,
	.close			= modem_tty_close,
	.write			= modem_tty_write,
	.write_room		= modem_tty_write_room,
	.chars_in_buffer	= modem_tty_chars_in_buffer,
	.set_termios		= modem_tty_set_termios,
	.tiocmget		= modem_tty_tiocmget,
	.tiocmset		= modem_tty_tiocmset,
	.hangup			= modem_tty_hangup,
	.install		= modem_tty_install,
	.cleanup		= modem_tty_cleanup,
};

static void spi_port_work_name(const char	*name, int index, char *p)
{
	sprintf(p, "%s%d", name, index);
}

static void respond_cflag80_packet_work(struct work_struct *work)
{
	struct spi_modem_port *port;
	struct spi_modem *modem;
	int ret = 0;

		struct spi_transfer t = {
				.tx_buf 	= via_spi_start_buf,
				.rx_buf 	= NULL,
				.len		= VIA_SPI_START_BUF_SIZE,
			};

		struct spi_message	m;

	port = container_of(work, struct spi_modem_port, respond_cflag80_work);
	modem = port->modem;

	if(down_interruptible(&modem->spi_sem)){
		hwlog_err("%s %d down_interruptible failed.\n", __func__,__LINE__);
		ret =  -ERESTARTSYS;
		goto down_sem_fail;
	}
	hwlog_debug("respond_cflag80_packet_work got semaphore.\n");

	atomic_set(&modem->cbp_data->cbp_data_ack->state, MODEM_ST_TX_RX);

	/*
	 * -------------------------------------------------------------------------
	 * |0xFE|ChanInfo(1Byte)|ThanHi(1Byte)|TranLow(1Byte)|Dummy|Paylaod|Padding|
	 * -------------------------------------------------------------------------
	 */
		via_spi_start_buf[0] = 0xFE;
		via_spi_start_buf[1] = 0x80;
		via_spi_start_buf[2] = 0x0;
		via_spi_start_buf[3] = 0x0;

		modem->spi_tx_state = TRANS_STATUS_WAITING_INIT_SEGMENT;
		//for ACK always quick than spi sync, so first change spi_state before spi_sync.
		//After send C=1 packet, change SPI state machine to RX state.
		modem->spi_state = SPI_STATE_RX;
		//Change RX substate to INIT.
		modem->spi_rx_state = TRANS_STATUS_WAITING_INIT_SEGMENT;

		spi_message_init(&m);
		spi_message_add_tail(&t, &m);
		spi_sync(port->modem->spi, &m);

		modem->spi_tx_state = TRANS_STATUS_FINISHED;


		hwlog_debug("respond_cflag80_packet_work wait_event ack begin left packet length is %d, current ack gpio level is %d\n", \
				0, oem_gpio_get_value(modem->cbp_data->cbp_data_ack->wait_gpio));
		modem->cbp_data->data_ack_wait_event(modem->cbp_data->cbp_data_ack);
		hwlog_debug("respond_cflag80_packet_work wait_event ack end left packet length is %d, current ack gpio level is %d\n", \
				0, oem_gpio_get_value(modem->cbp_data->cbp_data_ack->wait_gpio));

		spi_read_port_work(work);

down_sem_fail:
	up(&modem->spi_sem);
	hwlog_debug("respond_cflag80_packet_work release semaphore.\n");
}

static int modem_fc_flag = 0;
static void spi_write_port_work(struct work_struct *work)
{
	struct spi_modem_port *port;
	struct spi_modem *modem;
	struct tty_struct *tty;
	unsigned int count;
	unsigned int left, todo;
	unsigned int write_len;
	unsigned int fifo_size;
	unsigned long flags = 0;
	int ret = 0;

	port = container_of(work, struct spi_modem_port, write_work);
	modem = port->modem;

	atomic_set(&modem->cbp_data->cbp_data_ack->state, MODEM_ST_TX_RX);

	spin_lock_irqsave(&port->write_lock, flags);
	count = kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

	//for AT command problem of /r;
	if(count == 0){
		up(&modem->spi_sem);
		goto down_out;
	}

	if(modem->cbp_data->flow_ctrl_enable){
		if(oem_gpio_get_value(modem->cbp_data->cbp_flow_ctrl->wait_gpio)!=modem->cbp_data->gpio_flow_ctrl_polar){
			if(FLOW_CTRL_ENABLE == atomic_read(&modem->cbp_data->cbp_flow_ctrl->state))
				atomic_set(&modem->cbp_data->cbp_flow_ctrl->state, FLOW_CTRL_DISABLE);
		}else{
			while(1){/*print added for testing,to be removed*/
				if(cbp_power_state==0){
					hwlog_err("%s %d: card is removed when channel%d flow is enable,data is dropped\n", __func__, __LINE__, (port->index + 1));
					goto terminate;
				}

				if(modem_fc_flag < MODEM_FC_PRINT_MAX)
					hwlog_err("%s %d: channel%d flow ctrl before!\n", __func__, __LINE__, (port->index + 1));
				atomic_set(&modem->cbp_data->cbp_flow_ctrl->state, FLOW_CTRL_ENABLE);

				hwlog_debug("spi_write_port_work wait flow_ctrl begin, current flow_ctrl gpio level is %d\n", \
						oem_gpio_get_value(modem->cbp_data->cbp_flow_ctrl->wait_gpio));
				modem->cbp_data->flow_ctrl_wait_event(modem->cbp_data->cbp_flow_ctrl);
				hwlog_debug("spi_write_port_work wait flow_ctrl end, current flow_ctrl gpio level is %d\n", \
						oem_gpio_get_value(modem->cbp_data->cbp_flow_ctrl->wait_gpio));

				if(modem_fc_flag < MODEM_FC_PRINT_MAX){
					hwlog_err("%s %d: channel%d flow ctrl after!\n", __func__, __LINE__, (port->index + 1));
					modem_fc_flag++;
				}
				if(oem_gpio_get_value(modem->cbp_data->cbp_flow_ctrl->wait_gpio)!=modem->cbp_data->gpio_flow_ctrl_polar){
					hwlog_err("%s %d: channel%d flow ctrl ok!\n", __func__, __LINE__, (port->index + 1));
					atomic_set(&modem->cbp_data->cbp_flow_ctrl->state, FLOW_CTRL_DISABLE);
					modem_fc_flag = 0;
					break;
				}
			}
		}
	}

	if(modem->cbp_data->ipc_enable){
		asc_tx_auto_ready(modem->cbp_data->tx_handle->name, 1);
	}

	left = count;
	modem->spi_state = SPI_STATE_TX;
	modem->spi_tx_state = TRANS_STATUS_WAITING_INIT_SEGMENT;

	do{

		atomic_set(&modem->cbp_data->cbp_data_ack->state, MODEM_ST_TX_RX);

		if(down_interruptible(&modem->sem)){
			hwlog_err("%s %d down_interruptible failed.\n", __func__,__LINE__);
			ret =  -ERESTARTSYS;
			goto down_sem_fail;
		}
		todo = left;
		if (TRANS_STATUS_WAITING_INIT_SEGMENT == modem->spi_tx_state){
			if(todo > (VIA_SPI_FIFO_SIZE - 4)) {
				todo = VIA_SPI_FIFO_SIZE - 4;
			}
		} else if (TRANS_STATUS_WAITING_MORE_SEGMENT == modem->spi_tx_state) {
			if(todo > VIA_SPI_FIFO_SIZE) {
				todo = VIA_SPI_FIFO_SIZE;
			}
		}

/*
 * -------------------------------------------------------------------------
 * |0xFE|ChanInfo(1Byte)|ThanHi(1Byte)|TranLow(1Byte)|Dummy|Paylaod|Padding|
 * -------------------------------------------------------------------------
 */
	 	if (TRANS_STATUS_WAITING_INIT_SEGMENT == modem->spi_tx_state){
			*modem->trans_buffer = 0xFE;
			*(modem->trans_buffer + 1) = 0x0F & (port->index + 1);
			//this packet length is left, not todo as todo has changed.
			*(modem->trans_buffer + 2) = 0x0F & (left >> 8);
			*(modem->trans_buffer + 3) = 0xFF & left;

			//spin_lock_irqsave(&port->write_lock, flags);
			fifo_size = kfifo_out_locked(&port->transmit_fifo, modem->trans_buffer + 4, todo, &port->write_lock);
			//spin_unlock_irqrestore(&port->write_lock, flags);
			if(todo != fifo_size){
				hwlog_err("%s %d: port%d todo[%d] !=  kfifo lock out size[%d].\n", __func__, __LINE__,port->index, todo, fifo_size);
				todo = fifo_size;
			}
			write_len = (todo + 4 + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
		} else {
			//spin_lock_irqsave(&port->write_lock, flags);
			fifo_size = kfifo_out_locked(&port->transmit_fifo, modem->trans_buffer, todo, &port->write_lock);
			//spin_unlock_irqrestore(&port->write_lock, flags);
			if(todo != fifo_size){
				hwlog_err("%s %d: port%d todo[%d] !=  kfifo lock out size[%d].\n", __func__, __LINE__,port->index, todo, fifo_size);
				todo = fifo_size;
			}
			write_len = (todo + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
		}

		if(write_len > 0) {
			modem_spi_write(modem, 0x00, modem->trans_buffer, write_len);
		}
		left -= todo;

		if (left > VIA_SPI_FIFO_SIZE){
			modem->spi_tx_state = TRANS_STATUS_WAITING_MORE_SEGMENT;
			modem->spi_state = SPI_STATE_TX;
		} else if(left > 0) {
			modem->spi_tx_state = TRANS_STATUS_WAITING_LAST_SEGMENT;
			modem->spi_state = SPI_STATE_TX;
		} else if(0 == left) {
			modem->spi_tx_state = TRANS_STATUS_FINISHED;
			modem->spi_state = SPI_STATE_IDLE;
		}

		up(&modem->sem);

				hwlog_debug("spi_write_port_work wait ack begin left packet length is %d, current ack gpio level is %d\n", \
						left, oem_gpio_get_value(modem->cbp_data->cbp_data_ack->wait_gpio));
		modem->cbp_data->data_ack_wait_event(modem->cbp_data->cbp_data_ack);
				hwlog_debug("spi_write_port_work wait ack end left packet length is %d, current ack gpio level is %d\n", \
						left, oem_gpio_get_value(modem->cbp_data->cbp_data_ack->wait_gpio));
	}while(left);

	spin_lock_irqsave(&port->write_lock, flags);
	count = kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

	if (count < SPI_WAKEUP_CHARS) {
		tty = tty_port_tty_get(&port->port);
		if(tty){
			tty_wakeup(tty);
			tty_kref_put(tty);
		}
	}

down_sem_fail:
down_out:
terminate:
	return;
}


/*
 * This SPI interrupt handler.
 */
static void spi_read_port_work(struct work_struct *work)
{
	struct spi_modem *modem;
	struct spi_modem_port *port;
	int ret = 0;
	struct tty_struct *tty;
	unsigned char index = 0;
	unsigned char payload_offset = 0;
	struct spi_buf_in_packet *packet = NULL;
       u32 total_len = 0;
	int status = 0;

    struct spi_transfer xfer = {
            .tx_buf = NULL,
            .rx_buf = via_spi_follow_buf,
            .len = VIA_SPI_FIFO_SIZE,
    };
    struct spi_message  m;
	port = container_of(work, struct spi_modem_port, respond_cflag80_work);
	modem = port->modem;

	spi_rx_cnt++;
	modem = spi_get_drvdata(modem->spi);

	//only clear head and buffer for first packet.
	if (TRANS_STATUS_WAITING_INIT_SEGMENT == modem->spi_rx_state) {
		modem->msg->head.start_flag = 0;
		modem->msg->head.chanInfo = 0;
		modem->msg->head.tranHi = 0;
		modem->msg->head.tranLow = 0;
		memset(modem->msg->buffer, 0, sizeof(modem->msg->buffer));
	}

	do{

		atomic_set(&modem->cbp_data->cbp_data_ack->state, MODEM_ST_TX_RX);

		xfer.len = VIA_SPI_FIFO_SIZE;

		if(TRANS_STATUS_WAITING_INIT_SEGMENT == modem->spi_rx_state) {
			xfer.len = VIA_SPI_PACKET_HEADER_SIZE;
		} else {
			if(remain_packet_len > VIA_SPI_FIFO_SIZE) {
				xfer.len = VIA_SPI_FIFO_SIZE;
			} else {
				xfer.len = (remain_packet_len + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
			}
		}

		//for ACK always quick than spi sync, so first change spi_state before spi_sync.
		//After send C=1 packet, change SPI state machine to RX state.
		modem->spi_state = SPI_STATE_RX;

	    spi_message_init(&m);
	    spi_message_add_tail(&xfer, &m);
		//READ data from SPI
	    status = spi_sync(modem->spi, &m);
	    if (status) {
	        hwlog_err("%s - sync error: status=%d", __func__, status);
	        return;
	    }

		if (TRANS_STATUS_WAITING_INIT_SEGMENT == modem->spi_rx_state) {
			memcpy(modem->msg, via_spi_follow_buf, VIA_SPI_PACKET_HEADER_SIZE);
			msg_buf_pos = 0;
			total_len= (((modem->msg->head.tranHi & 0x0F) << 8) |
					(modem->msg->head.tranLow & 0xFF));
			if(total_len <= (VIA_SPI_FIFO_SIZE - 4)) {
				xfer.len = (total_len + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
			} else {
				xfer.len = (VIA_SPI_FIFO_SIZE - 4);
			}

			//for ACK always quick than spi sync, so first change spi_state before spi_sync.
			//After send C=1 packet, change SPI state machine to RX state.
			modem->spi_state = SPI_STATE_RX;

			spi_message_init(&m);
			spi_message_add_tail(&xfer, &m);
			status = spi_sync(modem->spi, &m);
			if (status) {
				hwlog_err("%s - spi read first segment remain data error=%d.\n", __func__, status);
				return;
			}

			memcpy(modem->msg->buffer, via_spi_follow_buf, xfer.len);

			if(total_len < (VIA_SPI_FIFO_SIZE - 4)) {
				msg_buf_pos = total_len;
			} else {
				msg_buf_pos = VIA_SPI_FIFO_SIZE - 4;
			}
			remain_packet_len = (total_len > (VIA_SPI_FIFO_SIZE - 4)) ? (total_len - (VIA_SPI_FIFO_SIZE - 4)) :0;

			if (remain_packet_len > VIA_SPI_FIFO_SIZE) {
				modem->spi_state = SPI_STATE_RX;
				modem->spi_rx_state = TRANS_STATUS_WAITING_MORE_SEGMENT;
			} else if (remain_packet_len > 0) {
				modem->spi_state = SPI_STATE_RX;
				modem->spi_rx_state = TRANS_STATUS_WAITING_LAST_SEGMENT;
			} else if (0 == remain_packet_len) {
				modem->spi_state = SPI_STATE_IDLE;
				modem->spi_rx_state = TRANS_STATUS_FINISHED;
			}
		} else if (TRANS_STATUS_WAITING_MORE_SEGMENT == modem->spi_rx_state) {
			memcpy(modem->msg->buffer + msg_buf_pos, via_spi_follow_buf, VIA_SPI_FIFO_SIZE);
			//data already readed, calculate remain data length first.
			msg_buf_pos += VIA_SPI_FIFO_SIZE;
			remain_packet_len -= VIA_SPI_FIFO_SIZE;

			if (remain_packet_len > VIA_SPI_FIFO_SIZE) {
				modem->spi_rx_state = TRANS_STATUS_WAITING_MORE_SEGMENT;
			} else if (remain_packet_len > 0) {
				modem->spi_rx_state = TRANS_STATUS_WAITING_LAST_SEGMENT;
			}
			modem->spi_state = SPI_STATE_RX;
		} else if (TRANS_STATUS_WAITING_LAST_SEGMENT == modem->spi_rx_state) {
			memcpy(modem->msg->buffer + msg_buf_pos, via_spi_follow_buf, remain_packet_len);
			//the whole packet have received.
			msg_buf_pos = 0;
			remain_packet_len = 0;
			modem->spi_state = SPI_STATE_IDLE;
			modem->spi_rx_state = TRANS_STATUS_FINISHED;
		}
		hwlog_debug("spi_read_port_work wait ack begin left packet length is %d, current ack gpio level is %d\n", \
				remain_packet_len, oem_gpio_get_value(modem->cbp_data->cbp_data_ack->wait_gpio));
		modem->cbp_data->data_ack_wait_event(modem->cbp_data->cbp_data_ack);
		hwlog_debug("spi_read_port_work wait ack end left packet length is %d, current ack gpio level is %d\n", \
				remain_packet_len, oem_gpio_get_value(modem->cbp_data->cbp_data_ack->wait_gpio));
	} while(remain_packet_len > 0);

	if (modem->msg->head.start_flag != 0xFE){
		hwlog_err("%s %d: start_flag != 0xFE and value is 0x%x, go out.\n",
				__func__, __LINE__, modem->msg->head.start_flag);
		goto out;
	}

	if (modem->msg->head.chanInfo > 0 && modem->msg->head.chanInfo < (SPI_TTY_NR + 1))
	{
	  if(TRANS_STATUS_FINISHED == modem->spi_rx_state) {
		index = modem->msg->head.chanInfo -1;
		payload_offset = ((modem->msg->head.tranHi & 0xC0) >> 6);
		if (payload_offset)
		{
			hwlog_debug("%s %d: payload_offset = %d.\n",__func__, __LINE__, payload_offset);
		}
		modem->data_length =(((modem->msg->head.tranHi & 0x0F) << 8) |
				(modem->msg->head.tranLow & 0xFF));
		if(modem->data_length == 0){
			hwlog_err("%s %d: data_length is 0\n",__func__,__LINE__);
			goto out;
		}
		port = modem->port[index];
		ret = check_port(port);
		if (ret < 0)
		{
			hwlog_err("%s %d: check port error\n",__func__,__LINE__);
			goto out;
		}

		if(port->inception){
			//  rawbulk_push_upstream_buffer(index,(modem->msg->buffer + payload_offset), (modem->data_length - payload_offset));
		} else {
			tty = tty_port_tty_get(&port->port);
			if(!tty)
			{
				mutex_lock(&port->spi_buf_in_mutex);
				port->spi_buf_in_size += (modem->data_length - payload_offset);
				if(port->spi_buf_in_size > SPI_BUF_IN_MAX_SIZE){
					port->spi_buf_in_size -= (modem->data_length - payload_offset);
					mutex_unlock(&port->spi_buf_in_mutex);
					hwlog_err("%s %d: ttySPI%d data buffer overrun!\n", __func__,__LINE__, index);
				}
				else{
					packet = kzalloc(sizeof(struct spi_buf_in_packet), GFP_KERNEL);
					if(!packet){
						hwlog_err("%s %d: kzalloc packet error\n",__func__,__LINE__);
						ret = -ENOMEM;
						mutex_unlock(&port->spi_buf_in_mutex);
						tty_kref_put(tty);
						goto out;
					}
					packet->size = modem->data_length - payload_offset;
					packet->buffer = kzalloc(packet->size, GFP_KERNEL);
					if(!packet->buffer){
						hwlog_err("%s %d: kzalloc packet buffer error\n",__func__,__LINE__);
						ret = -ENOMEM;
						kfree(packet);
						mutex_unlock(&port->spi_buf_in_mutex);
						tty_kref_put(tty);
						goto out;
					}
					memcpy(packet->buffer, (modem->msg->buffer + payload_offset), packet->size);

					if(port->spi_buf_in_num < port->spi_buf_in_max_num){
						list_add_tail(&packet->node, &port->spi_buf_in_list);
						port->spi_buf_in_num++;
					}
					else{
						struct spi_buf_in_packet *old_packet = NULL;
						old_packet = list_first_entry(&port->spi_buf_in_list, struct spi_buf_in_packet, node);
						list_del(&old_packet->node);
						if(old_packet){
							port->spi_buf_in_size -= old_packet->size;
							kfree(old_packet->buffer);
							kfree(old_packet);
						}
						list_add_tail(&packet->node, &port->spi_buf_in_list);
					}
					port->spi_buf_in = 1;
					mutex_unlock(&port->spi_buf_in_mutex);
					hwlog_err("%s %d: ttySPI%d data buffered! \n", __func__,__LINE__, index);
				}
			}

			if (tty && modem->data_length) {
				if(port->spi_buf_in ==1){
					mutex_lock(&port->spi_buf_in_mutex);/*make sure data in list bufeer had been pushed to tty buffer*/
					mutex_unlock(&port->spi_buf_in_mutex);
				}
				ret = tty_buffer_request_room(&port->port, (modem->data_length - payload_offset));
				if(ret < (modem->data_length - payload_offset)){
					hwlog_err("%s %d: ttySPI%d no room in tty rx buffer!\n", __func__,__LINE__, index);
				}
				else{
					ret = tty_insert_flip_string(&port->port, (modem->msg->buffer + payload_offset), (modem->data_length - payload_offset));
					if(ret < (modem->data_length - payload_offset)){
						hwlog_err("%s %d: ttySPI%d couldn't insert all characters (TTY is full?)!\n", __func__,__LINE__, index);
					}
					else{
						tty_flip_buffer_push(&port->port);
					}
				}
			}
			tty_kref_put(tty);
		}
	  }
	}
	else if(modem->msg->head.chanInfo ==0)/*control message analyze*/
	{
		ctrl_msg_analyze(modem);
	}
	else{
		hwlog_err("%s %d: error chanInfo is %d, go out.\n",
				__func__, __LINE__, modem->msg->head.chanInfo);
		goto out;
	}

out:
	return;
}


static void modem_spi_write(struct spi_modem *modem, int addr,
		void *buf, size_t len)
{
	struct spi_device *spi = modem->spi;
	unsigned char *print_buf = (unsigned char *)buf;
	unsigned char index = *(print_buf + 1);
	int status;

    struct spi_transfer xfer = {
            .tx_buf = via_spi_follow_buf,
            .rx_buf = NULL,
            .len = VIA_SPI_START_BUF_SIZE,
    };
    struct spi_message  m;

	spi_tx_cnt++;

	if(cbp_power_state==0){
		hwlog_err("%s %d: card is removed when channel%d flow is enable,data is dropped\n", __func__, __LINE__, index);
		spi_tx_rx_printk(buf, 1);
		goto terminate;
	}

	atomic_set(&modem->cbp_data->cbp_data_ack->state, MODEM_ST_TX_RX);

	//for ACK always quick than spi sync, so first change spi_state before spi_sync.
	//After send C=1 packet, change SPI state machine to RX state.
	modem->spi_state = SPI_STATE_TX;

	//use actual length
	memcpy(via_spi_follow_buf, buf, len);

	xfer.len = len;

	if (TRANS_STATUS_WAITING_INIT_SEGMENT == modem->spi_tx_state){
		if(len > VIA_SPI_START_BUF_SIZE) {
			xfer.len = len;
		} else {
			//for VIA RX FIFO interrupt watermark set to 60.
			xfer.len = VIA_SPI_START_BUF_SIZE;
		}
	} else {
		xfer.len = len;
	}

    spi_message_init(&m);
    spi_message_add_tail(&xfer, &m);
    status = spi_sync(spi, &m);
    if (status) {
        hwlog_err("%s - sync error: status=%d", __func__, status);
        return;
    }

terminate:
	return;
}

static void modem_port_remove(struct spi_modem *modem)
{
	struct spi_modem_port *port;
	struct tty_struct *tty;
	unsigned long flags = 0;
	int index;
	hwlog_info("%s %d: Enter.\n", __func__, __LINE__);

	for (index = 0; index < SPI_TTY_NR; index++) {
		port = modem->port[index];
		atomic_set(&port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
		wake_up(&port->sflow_ctrl_wait_q);
		atomic_set(&modem->ctrl_port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
		wake_up(&modem->ctrl_port->sflow_ctrl_wait_q);
		atomic_set(&modem->cbp_data->cbp_via_requst_to_send->state, FLOW_CTRL_DISABLE);
		wake_up(&modem->cbp_data->cbp_via_requst_to_send->wait_q);
		atomic_set(&modem->cbp_data->cbp_data_ack->state, MODEM_ST_READY);
		wake_up(&modem->cbp_data->cbp_data_ack->wait_q);
		if(port->write_q){
			hwlog_info(	"%s %d: port%d destroy queue before.\n", __func__, __LINE__, index);
			cancel_work_sync(&port->write_work);
			destroy_workqueue(port->write_q);
			hwlog_info(	"%s %d: port%d destroy queue after.\n", __func__, __LINE__, index);
		}
		if(port->respond_cflag80_q){
			hwlog_info(	"%s %d: port%d destroy queue before.\n", __func__, __LINE__, index);
			cancel_work_sync(&port->respond_cflag80_work);
			destroy_workqueue(port->respond_cflag80_q);
			hwlog_info(	"%s %d: port%d destroy queue after.\n", __func__, __LINE__, index);
		}
		if(port->read_q){
			hwlog_info(	"%s %d: port%d destroy queue before.\n", __func__, __LINE__, index);
			cancel_work_sync(&port->read_work);
			destroy_workqueue(port->read_q);
			hwlog_info(	"%s %d: port%d destroy queue after.\n", __func__, __LINE__, index);
		}
		BUG_ON(spi_modem_table[port->index] != port);

		spin_lock_irqsave(&spi_modem_table_lock, flags);
		spi_modem_table[index] = NULL;
		spin_unlock_irqrestore(&spi_modem_table_lock, flags);
		hwlog_info(	"%s %d: spi_modem_table cleared.\n", __func__, __LINE__);
		mutex_lock(&port->port.mutex);
		port->spi = NULL;
		tty = tty_port_tty_get(&port->port);
		/* tty_hangup is async so is this safe as is ?? */
		if (tty) {
			hwlog_info("%s %d destory tty,index=%d port->index=%d\n",__func__,__LINE__,index,port->index);
			tty_hangup(tty);
			tty_kref_put(tty);
		}
		mutex_unlock(&port->port.mutex);

		spi_modem_port_put(port);
	}
	hwlog_info("%s %d: Leave.\n", __func__, __LINE__);
}

static void spi_buffer_in_set_max_len(struct spi_modem_port *port)
{
	unsigned int	index = port->index;
	switch(index)
	{
		case 0:
			port->spi_buf_in_max_num = SPI_PPP_BUF_IN_MAX_NUM;
			break;
		case 1:
			port->spi_buf_in_max_num = SPI_ETS_BUF_IN_MAX_NUM;
			break;
		case 2:
			port->spi_buf_in_max_num = SPI_IFS_BUF_IN_MAX_NUM;
			break;
		case 3:
			port->spi_buf_in_max_num = SPI_AT_BUF_IN_MAX_NUM;
			break;
		case 4:
			port->spi_buf_in_max_num = SPI_PCV_BUF_IN_MAX_NUM;
			break;
		default:
			port->spi_buf_in_max_num = SPI_DEF_BUF_IN_MAX_NUM;
			break;
	}
}

static int spi_modem_port_init(struct spi_modem_port *port, int index)
{
	int ret = 0;
	unsigned long flags = 0;

	kref_init(&port->kref);
	spin_lock_init(&port->write_lock);

	if (kfifo_alloc(&port->transmit_fifo, FIFO_SIZE, GFP_KERNEL)){
		hwlog_err("%s %d : Couldn't allocate transmit_fifo\n",__func__,__LINE__);
		return -ENOMEM;
	}

	/* create port's write work queue */
	port->name = "modem_spi_write_wq";
	spi_port_work_name(port->name, index, port->work_name);
	port->write_q = create_singlethread_workqueue(port->work_name);
	if (port->write_q == NULL) {
		hwlog_err("%s %d error creat write workqueue \n",__func__, __LINE__);
		return -ENOMEM;
	}
	port->respond_cflag80_q = create_singlethread_workqueue(port->work_name);
	if (port->respond_cflag80_q == NULL) {
		hwlog_err("%s %d error creat start_read workqueue \n",__func__, __LINE__);
		return -ENOMEM;
	}
	port->read_q = create_singlethread_workqueue(port->work_name);
	if (port->read_q == NULL) {
		hwlog_err("%s %d error creat read workqueue \n",__func__, __LINE__);
		return -ENOMEM;
	}
	INIT_WORK(&port->write_work, spi_write_port_work);
	INIT_WORK(&port->respond_cflag80_work, respond_cflag80_packet_work);
	INIT_WORK(&port->read_work, spi_read_port_work);

	spin_lock_irqsave(&spi_modem_table_lock, flags);
	if (!spi_modem_table[index]) {
		port->index = index;
		spi_modem_table[index] = port;
	}
	spin_unlock_irqrestore(&spi_modem_table_lock, flags);

	mutex_init(&port->spi_buf_in_mutex);
	INIT_LIST_HEAD(&port->spi_buf_in_list);
	port->spi_buf_in = 0;
	port->spi_buf_in_num = 0;
	port->spi_buf_in_size = 0;
	spi_buffer_in_set_max_len(port);

	init_waitqueue_head(&port->sflow_ctrl_wait_q);
	atomic_set(&port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);

	return ret;
}

static ssize_t modem_refer_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	char *s = buf;
	s += sprintf(s, "Tx:  times %d\n", spi_tx_cnt);
	s += sprintf(s, "\n");
	s += sprintf(s, "Rx:  times %d\n", spi_rx_cnt);

	return (s - buf);
}

static ssize_t modem_refer_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	return n;
}

static ssize_t modem_ctrl_on_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	struct spi_modem_ctrl_port *ctrl_port;
	struct spi_modem *modem;
	struct spi_modem_port *port;
	char *s = buf;
	int ret=-1;

	hwlog_info("%s: enter\n", __func__);

	port = spi_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d check_port failed\n", __func__,__LINE__);
		goto out;
	}
	modem = port->modem;
	ctrl_port = modem->ctrl_port;

	s += sprintf(s, "ctrl state: %s\n", ctrl_port->chan_state ?"enable":"disable");

	return (s - buf);

out:
	return ret;
}

static ssize_t modem_ctrl_on_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	if (val){
		modem_on_off_ctrl_chan(1);
	}
	else{
		modem_on_off_ctrl_chan(0);
	}
	return n;
}
static ssize_t modem_dtr_send_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	int status=-1,ret=-1;
	char *s = buf;

	hwlog_info("%s: enter\n", __func__);

	status=modem_dcd_state();
	if(ret <0){
		hwlog_info(	"query cp ctrl channel state failed ret=%d\n",ret);
	}
	s += sprintf(s, "ctrl state: %d\n", status);

	return (s - buf);
}

static ssize_t modem_dtr_send_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	dtr_value = val;
	modem_dtr_set(val, 1);

	return n;
}

static ssize_t modem_dtr_query_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	int status=-1,ret=-1;
	char *s = buf;

	hwlog_info("%s: enter\n", __func__);

	status = modem_dcd_state();
	if(status <0){
		hwlog_info(	"query cp ctrl channel state failed ret=%d\n",ret);
		s += sprintf(s, "ctrl state: %s\n","N/A" );
	}else{
		s += sprintf(s, "ctrl state: %d\n", status);
	}
	return (s - buf);
}

static ssize_t modem_dtr_query_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	modem_dcd_state();
	return n;
}

static unsigned char loop_back_chan = 0;
static ssize_t modem_loop_back_chan_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	return 0;
}

static ssize_t modem_loop_back_chan_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;
	if (val < 6){
		loop_back_chan = val;
	}
	else{
		hwlog_err("%s %d error channel select, please < 6!\n", __func__,__LINE__);
	}

	return n;
}

static ssize_t modem_loop_back_mod_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	return 0;
}

static ssize_t modem_loop_back_mod_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t n)
{
	unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	if (val < 4){
		modem_loop_back_chan(loop_back_chan, val);
	}
	else{
		hwlog_err("%s %d error channel select, please check the option!\n", __func__,__LINE__);
	}
	return n;
}

static ssize_t modem_ack_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	char *s = buf;
	struct spi_modem *modem;
	struct spi_modem_port *port;
	int ret =-1;

	if((cbp_pdata!=NULL) && (cbp_pdata->cbp_data_ack!=NULL )){
		s += sprintf(s, "gpio[%d]\t state:[%d]\t polar[%d]\t ",
				cbp_pdata->cbp_data_ack->wait_gpio ,atomic_read(&cbp_pdata->cbp_data_ack->state),cbp_pdata->cbp_data_ack->wait_polar);
		port = spi_modem_port_get(0);
		ret = check_port(port);
		if (ret < 0){
			hwlog_err("%s %d check_port failed\n", __func__,__LINE__);
			goto out;
		}
		modem = port->modem;
		s += sprintf(s, "stored:[%d]\n", oem_gpio_get_value(modem->cbp_data->cbp_data_ack->wait_gpio));
	}
out:
	return (s - buf);
}

static ssize_t modem_ack_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t n)
{

	return n;
}

static ssize_t modem_flw_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	char *s = buf;
	struct spi_modem *modem;
	struct spi_modem_port *port;
	int ret =-1;

	if((cbp_pdata!=NULL) && (cbp_pdata->cbp_via_requst_to_send!=NULL )){
		s += sprintf(s, "gpio[%d] \tstate:[%d]\t polar[%d]\t ",
				cbp_pdata->cbp_via_requst_to_send->wait_gpio,atomic_read(&cbp_pdata->cbp_via_requst_to_send->state),cbp_pdata->cbp_via_requst_to_send->wait_polar);
		port = spi_modem_port_get(0);
		ret = check_port(port);
		if (ret < 0){
			hwlog_err("%s %d check_port failed\n", __func__,__LINE__);
			goto out;
		}
		modem = port->modem;
		s += sprintf(s, "stored:[%d]\n", oem_gpio_get_value(modem->cbp_data->cbp_via_requst_to_send->wait_gpio));
	}
out:
	return (s - buf);
}

static ssize_t modem_flw_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t n)
{

	return n;
}


#define modem_attr(_name) \
	static struct kobj_attribute _name##_attr = {	\
		.attr	= {				\
			.name = __stringify(_name),	\
			.mode = 0660,			\
		},					\
		.show	= modem_##_name##_show,			\
		.store	= modem_##_name##_store,		\
	}
modem_attr(refer);
modem_attr(ctrl_on);
modem_attr(dtr_send);
modem_attr(dtr_query);
modem_attr(loop_back_chan);
modem_attr(loop_back_mod);
modem_attr(ack);
modem_attr(flw);


static struct attribute * modem_spi_attr[] = {
	&refer_attr.attr,
	&ctrl_on_attr.attr,
	&dtr_send_attr.attr,
	&dtr_query_attr.attr,
	&loop_back_chan_attr.attr,
	&loop_back_mod_attr.attr,
	&ack_attr.attr,
	&flw_attr.attr,
	NULL,
};
static struct kobject *modem_spi_kobj;
static struct attribute_group g_modem_attr_group = {
	.attrs =modem_spi_attr,
};

int modem_buffer_push(int port_num, void *buf, int count)
{
	int ret, data_len;
	unsigned long flags;
	struct spi_modem_port *port = spi_modem_port_get(port_num);

	if(port_num >= 2)
		port_num++;

	ret = check_port(port);
	if (ret < 0){
		hwlog_err("%s %d invalid port\n", __FUNCTION__,__LINE__);
		return ret;
	}

	if (count == 0)
		return 0;

	spin_lock_irqsave(&port->write_lock, flags);
	data_len = FIFO_SIZE - kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);
	if(data_len < count) {
		hwlog_debug("%s %d: SPI driver buffer is full!\n", __FUNCTION__,__LINE__);
		return -ENOMEM;
	}

	spin_lock_irqsave(&modem_remove_lock, flags);
	if(modem_remove == 0){
		spin_unlock_irqrestore(&modem_remove_lock, flags);
		ret = kfifo_in_locked(&port->transmit_fifo, buf, count, &port->write_lock);
		queue_work(port->write_q, &port->write_work);
	}
	else{
		spin_unlock_irqrestore(&modem_remove_lock, flags);
		hwlog_info("%s %d: port%d is removed!\n", __func__, __LINE__, port->index);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(modem_buffer_push);
extern void VIA_trigger_signal(int i_on);

static int modem_spi_probe(struct spi_device *spi)
{
	struct spi_modem *modem = NULL;
	struct spi_modem_port *port = NULL;
	int ret = 0;
	int index = 0;
	unsigned long flags;

	hwlog_info("%s %d: enter.\n", __func__, __LINE__);
	modem = kzalloc(sizeof(struct spi_modem), GFP_KERNEL);
	if (!modem){
		hwlog_err("%s %d kzalloc spi_modem failed.\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto err_kzalloc_spi_modem;
	}

	modem->ctrl_port = kzalloc(sizeof(struct spi_modem_ctrl_port), GFP_KERNEL);
	if (!modem->ctrl_port) {
		hwlog_err("%s %d kzalloc ctrl_port failed \n",__func__, __LINE__);
		ret =  -ENOMEM;
		goto err_kzalloc_ctrl_port;
	}

	modem->msg = kzalloc(sizeof(struct spi_msg), GFP_KERNEL);
	if (!modem->msg){
		hwlog_err("%s %d kzalloc spi_msg failed \n",__func__, __LINE__);
		ret = -ENOMEM;
		goto err_kzalloc_spi_msg;
	}

	modem->trans_buffer = kzalloc(TRANSMIT_BUFFER_SIZE, GFP_KERNEL);
	if (!modem->trans_buffer) {
		hwlog_err("%s %d kzalloc trans_buffer failed \n",__func__, __LINE__);
		ret =  -ENOMEM;
		goto err_kzalloc_trans_buffer;
	}

	modem->spi = spi;
	sema_init(&modem->sem, 1);
	spi_set_drvdata(spi, modem);
	modem->cbp_data = cbp_pdata;
	modem->ctrl_port->chan_state = 0;
	init_waitqueue_head(&modem->ctrl_port->sflow_ctrl_wait_q);
	atomic_set(&modem->ctrl_port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
	sema_init(&modem->spi_sem, 1);

	/* step 3: setup spi */
    spi->mode = SPI_MODE_1;
    spi->max_speed_hz = VIA_SPI_SPEED;
    spi->bits_per_word = 8;
    ret = spi_setup(spi);
    if (ret<0) {
        hwlog_err("probe - setup spi error");
        goto err_enable_func;
    }

    via_spi_start_buf = (char *)__get_free_pages(GFP_KERNEL, 0);
    if(!via_spi_start_buf) {
        hwlog_err("probe - spi request via_spi_start_buf memory error");
        goto err_enable_func;
    }

    via_spi_follow_buf = (char *)__get_free_pages(GFP_KERNEL, 0);
    if(!via_spi_follow_buf) {
        hwlog_err("probe - via_spi_follow_buf request memory error");
        goto err_free_spi_start_buf;
    }

	modem->spi_state = SPI_STATE_IDLE;
	atomic_set(&modem->ack_tx_state, 0);

	for (index = 0; index < SPI_TTY_NR; index++) {
		port = kzalloc(sizeof(struct spi_modem_port), GFP_KERNEL);
		if (!port)
		{
			hwlog_err("%s %d kzalloc spi_modem_port %d failed.\n",
					__func__, __LINE__, index);
			ret = -ENOMEM;
			goto err_kazlloc_spi_modem_port;
		}

		tty_port_init(&port->port);
		port->spi = spi;
		port->port.ops = &spi_modem_port_ops;
		port->modem = modem;
		modem->port[index] = port;
		spin_lock_init(&port->inception_lock);
		port->inception = false;
	}

	for (index = 0; index < SPI_TTY_NR; index++)
	{
		port = modem->port[index];
		ret = spi_modem_port_init(port, index);
		if (ret){
			hwlog_err("%s %d spi add port failed.\n",__func__, __LINE__);
			goto err_spi_modem_port_init;
		}
		else {
			struct device *dev;

			//dev = tty_register_device(modem_spi_tty_driver,
			//		port->index, &spi->dev);
            dev = tty_port_register_device(&port->port, modem_spi_tty_driver,
                    port->index, &spi->dev);
			if (IS_ERR(dev)) {
				ret = PTR_ERR(dev);
				hwlog_err("%s %d tty register failed \n",__func__,__LINE__);
				goto err_spi_modem_port_init;
			}
		}
	}

	modem_spi_kobj = viatel_kobject_add("modem_spi");
	if (!modem_spi_kobj){
		ret = -ENOMEM;
		goto err_create_kobj;
	}
	hwlog_info("%s %d: exit.\n", __func__, __LINE__);

	cbp_power_state = 1;

	spin_lock_init(&modem_remove_lock);
	INIT_WORK(&dtr_work, modem_dtr_send);
	INIT_WORK(&dcd_query_work, modem_dcd_query);
	spin_lock_irqsave(&modem_remove_lock, flags);
	modem_remove = 0;
	spin_unlock_irqrestore(&modem_remove_lock, flags);
	via_modem = modem;

	//VIA_trigger_signal(2);
	return sysfs_create_group(modem_spi_kobj, &g_modem_attr_group);;
err_create_kobj:
err_spi_modem_port_init:
	modem_port_remove(modem);
err_kazlloc_spi_modem_port:
	for (index = 0; index < SPI_TTY_NR; index++) {
		port = modem->port[index];
		if(port){
			kfree(port);
		}
	}

    free_pages((unsigned long)via_spi_follow_buf, 0);
err_free_spi_start_buf:
    free_pages((unsigned long) via_spi_start_buf, 0);
err_enable_func:
	kfree(modem->trans_buffer);
err_kzalloc_trans_buffer:
	kfree(modem->msg);
err_kzalloc_spi_msg:
	kfree(modem->ctrl_port);
err_kzalloc_ctrl_port:
	kfree(modem);
err_kzalloc_spi_modem:
	return ret;
}

void modem_reset_handler(void)
{
	struct spi_modem *modem = via_modem;
	struct spi_modem_port *port;
	int index;
	unsigned long flags;

	hwlog_info("%s %d: Enter.\n", __func__, __LINE__);
	if(modem == NULL){
		hwlog_info("%s %d: modem is NULL.\n", __func__, __LINE__);
		goto out;
	}

	for (index= 0; index< SPI_TTY_NR; index++) {
		port = modem->port[index];
		tty_unregister_device(modem_spi_tty_driver, port->index);
		hwlog_info("%s %d: port%d rawbulk_unbind_spi_channel before.\n", __func__, __LINE__, index);
		//rawbulk_unbind_spi_channel(index);
		hwlog_info("%s %d: port%d rawbulk_unbind_spi_channel after.\n", __func__, __LINE__, index);
	}

	spin_lock_irqsave(&modem_remove_lock, flags);
	modem_remove = 1;
	spin_unlock_irqrestore(&modem_remove_lock, flags);
	hwlog_info("%s %d: cancel_work_sync(&dtr_work) before.\n", __func__, __LINE__);
	cancel_work_sync(&dtr_work);
	hwlog_info("%s %d: cancel_work_sync(&dtr_work) after.\n", __func__, __LINE__);
	cancel_work_sync(&dcd_query_work);
	hwlog_info("%s %d: cancel_work_sync(&dcd_query_work) after.\n", __func__, __LINE__);
	dcd_state = 0;

	modem_port_remove(modem);

out:
	hwlog_info("%s %d: Leave.\n", __func__, __LINE__);
}
EXPORT_SYMBOL_GPL(modem_reset_handler);

static int modem_spi_remove(struct spi_device *spi)
{
	struct spi_modem *modem = spi_get_drvdata(spi);

	hwlog_info("%s %d: Enter.\n", __func__, __LINE__);
	modem_reset_handler();
	sysfs_remove_group(modem_spi_kobj, &g_modem_attr_group);
	kobject_put(modem_spi_kobj);

    if(via_spi_start_buf) {
        free_pages((unsigned long) via_spi_start_buf, 0);
    }
    if(via_spi_follow_buf) {
        free_pages((unsigned long)via_spi_follow_buf, 0);
    }

	via_modem = NULL;
	kfree(modem->trans_buffer);
	kfree(modem->ctrl_port);
	kfree(modem->msg);
	kfree(modem);
	hwlog_info("%s %d: Leave.\n", __func__, __LINE__);

	return 0;
}

#define SPI_VENDOR_ID_CBP		0x0296
#define SPI_DEVICE_ID_CBP		0x5347

static const struct of_device_id modem_spi_ids[] = {
	{ .compatible = "spi_dev4" },
	{},
};

MODULE_DEVICE_TABLE(of, modem_spi_ids);

static struct spi_driver modem_spi_driver = {
	.driver = {
		.name = "spi_dev4",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(modem_spi_ids),
	},
	.probe		= modem_spi_probe,
	.remove		= modem_spi_remove,
};

int  modem_spi_init(struct cbp_platform_data *pdata)
{
	int ret;
	struct tty_driver *tty_drv;

	modem_spi_tty_driver = tty_drv = alloc_tty_driver(SPI_TTY_NR);
	cbp_pdata = pdata;
	if (!tty_drv)
		return -ENOMEM;

	tty_drv->owner = THIS_MODULE;
	tty_drv->driver_name = "modem_spi";
	tty_drv->name =   "ttySPI";
	tty_drv->major = 0;  // dynamically allocated
	tty_drv->minor_start = 0;
	tty_drv->type = TTY_DRIVER_TYPE_SERIAL;
	tty_drv->subtype = SERIAL_TYPE_NORMAL;
	tty_drv->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_drv->init_termios = tty_std_termios;
	tty_drv->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	tty_drv->init_termios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD);
	tty_drv->init_termios.c_cflag |= CREAD | CLOCAL | CS8 ;
	tty_drv->init_termios.c_cflag &= ~(CRTSCTS);
	tty_drv->init_termios.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG);
	tty_drv->init_termios.c_iflag &= ~(INPCK | IGNPAR | PARMRK | ISTRIP | IXANY | ICRNL);
	tty_drv->init_termios.c_iflag &= ~(IXON | IXOFF);
	tty_drv->init_termios.c_oflag &= ~(OPOST | OCRNL);
	tty_drv->init_termios.c_ispeed = 9600;
	tty_drv->init_termios.c_ospeed = 9600;
	tty_set_operations(tty_drv, &modem_tty_ops);

	ret = tty_register_driver(tty_drv);
	if (ret) {
		hwlog_err("%s: tty_register_driver failed.\n", __func__);
		goto exit_reg_driver;
	}

	ret = spi_register_driver(&modem_spi_driver);
	if (ret) {
		hwlog_err("%s: spi_register_driver failed.\n",__func__);
		goto exit_tty;
	}

	hwlog_debug(" %s: spi driver is initialized!\n",__func__);
	return ret;
exit_tty:
	tty_unregister_driver(tty_drv);
exit_reg_driver:
	hwlog_err("%s: returning with error %d\n",__func__, ret);
	put_tty_driver(tty_drv);
	return ret;
}

void  modem_spi_exit(void)
{
	spi_unregister_driver(&modem_spi_driver);
	tty_unregister_driver(modem_spi_tty_driver);
	put_tty_driver(modem_spi_tty_driver);
}

