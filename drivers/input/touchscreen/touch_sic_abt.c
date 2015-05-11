/* touch_sic_abt.c
 *
 * Copyright (C) 2014 LGE.
 *
 * Author: esook.kang@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/signal.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/input/lge_touch_core.h>
#include <linux/input/touch_sic.h>
#include <linux/input/touch_sic_abt.h>

#if USE_ABT_MONITOR_APP
struct mutex abt_i2c_comm_lock;
struct mutex abt_socket_lock;
struct sock_comm_t abt_comm;
int abt_socket_mutex_flag;
int abt_socket_report_mode;

int abt_report_mode;
u8 abt_report_point;
u8 abt_report_ocd;
u32 CMD_GET_ABT_DEBUG_REPORT;
int abt_report_mode_onoff;
enum ABT_CONNECT_TOOL abt_conn_tool = NOTHING;

/*sysfs*/
u16 abt_ocd[2][ACTIVE_SCREEN_CNT_X*ACTIVE_SCREEN_CNT_Y];
u8 abt_ocd_read;
u8 abt_reportP[256];
char abt_head[128];
int abt_show_mode;
int abt_ocd_on;
u32 abt_compress_flag;
int abt_head_flag;

static int32_t abt_ksocket_init_send_socket(struct sock_comm_t *abt_socket)
{
	int ret;
	struct socket *sock;
	struct sockaddr_in *addr = &abt_socket->addr_send;
	int *connected = &abt_socket->send_connected;
	char *ip = (char *)abt_socket->send_ip;

	ret = sock_create(AF_INET,
			SOCK_DGRAM,
			IPPROTO_UDP,
			&(abt_socket->sock_send));
	sock = abt_socket->sock_send;

	if (ret >= 0) {
		memset(addr, 0, sizeof(struct sockaddr));
		addr->sin_family = AF_INET;
		addr->sin_addr.s_addr = in_aton(ip);
		addr->sin_port = htons(SEND_PORT);
	}	else {
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME": can not create socket %d\n",
			-ret);
		goto error;
	}

	ret = sock->ops->connect(sock,
				(struct sockaddr *)addr,
				sizeof(struct sockaddr),
				!O_NONBLOCK);

	if (ret < 0) {
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME
			": Could not connect to send socket, error = %d\n",
			-ret);
		goto error;
	}	else {
		*connected = 1;
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME
			": connect send socket (%s,%d)(\n",
			ip, SEND_PORT);
	}

	return ret;
error:
	sock = NULL;
	*connected = 0;
	return ret;
}

static int abt_ksocket_receive(unsigned char *buf, int len)
{
	struct msghdr msg;
	struct iovec iov;
	struct socket *sock = abt_comm.sock;
	struct sockaddr_in *addr = &abt_comm.addr;
	mm_segment_t oldfs;
	unsigned int flag = 0;
	int size = 0;

	if (abt_conn_tool == ABT_PCTOOL) {
		sock = abt_comm.client_sock;
		addr = &abt_comm.client_addr;
	}

	if (sock == NULL)
		return 0;

	if (abt_conn_tool == ABT_PCTOOL) {
		abt_comm.client_sock->ops = abt_comm.sock->ops;
		flag = MSG_WAITALL;
	}

	iov.iov_base = buf;
	iov.iov_len = len;

	msg.msg_flags = flag;
	msg.msg_name = addr;
	msg.msg_namelen  = sizeof(struct sockaddr_in);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	size = sock_recvmsg(sock, &msg, len, msg.msg_flags);
	set_fs(oldfs);

	abt_comm.sock_listener(buf, size);

	return size;
}

static void abt_ksocket_start_for_pctool(struct i2c_client *client)
{
	static int client_connected;
	int size, err;
	unsigned char *buf;

	/* kernel thread initialization */
	abt_comm.running = 1;
	abt_comm.client = client;

	/* create a socket */
	err = sock_create_kern(AF_INET,
				SOCK_STREAM,
				IPPROTO_TCP,
				&abt_comm.sock);
	if (err < 0) {
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME
			": could not create a datagram socket, error = %d\n\n",
			-ENXIO);
		goto out;
	}


	err = sock_create_lite(AF_INET,
				SOCK_STREAM,
				IPPROTO_TCP,
				&abt_comm.client_sock);
	if (err < 0) {
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME
			": could not create a Client socket, error = %d\n\n",
			-ENXIO);
		goto out;
	}

	memset(&abt_comm.addr, 0, sizeof(struct sockaddr));
	abt_comm.addr.sin_family	= AF_INET;
	abt_comm.addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	abt_comm.addr.sin_port		= htons(DEFAULT_PORT);
	err = abt_comm.sock->ops->bind(abt_comm.sock,
					(struct sockaddr *)&abt_comm.addr,
					sizeof(struct sockaddr_in));

	if (err < 0)
		goto out;

	err = abt_comm.sock->ops->listen(abt_comm.sock, 10);

	if (err < 0)
		goto out;

	/* init  data send socket */
	abt_ksocket_init_send_socket(&abt_comm);

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (abt_comm.running != 1)
			break;

		if (client_connected != 1) {
			msleep(200);
			err = abt_comm.sock->ops->accept(abt_comm.sock,
						abt_comm.client_sock,
						O_NONBLOCK);

			if (err < 0)
				continue;
			else if (err >= 0) {
				TOUCH_D(DEBUG_BASE_INFO,
					MODULE_NAME": accept ok(%d)\n", err);
				client_connected = 1;
			}
		}

		size = 0;

		buf = kzalloc(sizeof(struct s_comm_packet), GFP_KERNEL);
		size = abt_ksocket_receive(&buf[0],
						sizeof(struct s_comm_packet));

		if (size <= 0)
			client_connected = 0;

		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME ": RECEIVE OK, size(%d)\n\n", size);

		if (kthread_should_stop()) {
			TOUCH_D(DEBUG_BASE_INFO, ": kthread_should_stop\n");
			break;
		}

		if (size < 0)
			TOUCH_D(DEBUG_BASE_INFO,
				": error getting datagram, sock_recvmsg error = %d\n",
				 size);
		if (buf != NULL)
			kfree(buf);
	}

out:
	__set_current_state(TASK_RUNNING);
	abt_comm.running = 0;
}

static void abt_ksocket_start_for_abtstudio(struct i2c_client *client)
{
	int size, err;
	int bufsize = 10;
	unsigned char buf[bufsize+1];

	/* kernel thread initialization */
	abt_comm.running = 1;
	abt_comm.client = client;

	/* create a socket */
	err = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP,	&abt_comm.sock);
	if (err < 0) {
		TOUCH_D(DEBUG_BASE_INFO,
			": could not create a datagram socket, error = %d\n",
			-ENXIO);
		goto out;
	}

	memset(&abt_comm.addr, 0, sizeof(struct sockaddr));
	abt_comm.addr.sin_family	= AF_INET;
	abt_comm.addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	abt_comm.addr.sin_port		= htons(DEFAULT_PORT);
	err = abt_comm.sock->ops->bind(abt_comm.sock,
					(struct sockaddr *)&abt_comm.addr,
					sizeof(struct sockaddr));
	if (err < 0) {
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME
			": Could not bind to receive socket, error = %d\n",
			-err);
		goto out;
	}

	TOUCH_D(DEBUG_BASE_INFO, ": listening on port %d\n", DEFAULT_PORT);

	/* init raw data send socket */
	abt_ksocket_init_send_socket(&abt_comm);

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		memset(&buf, 0, bufsize+1);

		size = 0;
		if (abt_comm.running == 1) {
			size = abt_ksocket_receive(buf, bufsize);
			TOUCH_D(DEBUG_BASE_INFO, ": receive packet\n");
		} else {
			TOUCH_D(DEBUG_BASE_INFO, ": running off\n");
			break;
		}

		if (kthread_should_stop())
			break;

		if (size < 0)
			TOUCH_D(DEBUG_BASE_INFO,
			": error getting datagram, sock_recvmsg error = %d\n",
			size);

		msleep(50);
	}

out:
	__set_current_state(TASK_RUNNING);
	abt_comm.running = 0;

}

static void abt_recv_packet_parsing(struct i2c_client *client,
				struct s_comm_packet *p_packet_from_pc,
				struct s_comm_packet *p_packet_to_pc)
{
	int idx = 0;
	int ret = -1;
	u32 addr_ptr = p_packet_from_pc->addr;
	u8 *data_ptr_from_pc = (u8 *)&(p_packet_from_pc->data[0]);
	u8 *data_ptr_to_pc = (u8 *)&(p_packet_to_pc->data[0]);
	int packet_cnt = p_packet_from_pc->data_size / I2C_PACKET_SIZE;

	if (p_packet_from_pc->flag.rw == READ_TYPE) {

		for (idx = 0; idx < packet_cnt; idx++) {
			ret = sic_i2c_read(client,
						addr_ptr,
						data_ptr_to_pc,
						I2C_PACKET_SIZE);
			if (ret < 0)
				break;

			addr_ptr += (I2C_PACKET_SIZE/4);
			data_ptr_to_pc += I2C_PACKET_SIZE;
		}

		if (p_packet_from_pc->data_size % I2C_PACKET_SIZE != 0) {
			ret = sic_i2c_read(client,
						addr_ptr,
						data_ptr_to_pc,
						p_packet_from_pc->data_size %
						I2C_PACKET_SIZE);
		}
	}	else if (p_packet_from_pc->flag.rw == WRITE_TYPE) {
		for (idx = 0; idx < packet_cnt; idx++) {
			ret = sic_i2c_write(client,
						addr_ptr,
						data_ptr_from_pc,
						I2C_PACKET_SIZE);
			addr_ptr += (I2C_PACKET_SIZE/4);
			data_ptr_from_pc += I2C_PACKET_SIZE;
		}

		if (p_packet_from_pc->data_size % I2C_PACKET_SIZE != 0) {
			ret = sic_i2c_write(client, addr_ptr,
						data_ptr_from_pc,
						p_packet_from_pc->data_size %
						I2C_PACKET_SIZE);
		}
	}

	p_packet_to_pc->cmd = DEBUG_DATA_RW_MODE;
	p_packet_to_pc->flag.rw = p_packet_from_pc->flag.rw;
	p_packet_to_pc->status = ret;
	p_packet_to_pc->addr = p_packet_from_pc->addr;

	if (p_packet_from_pc->flag.rw == READ_TYPE)
		p_packet_to_pc->data_size = p_packet_from_pc->data_size;
	else if (p_packet_from_pc->flag.rw == WRITE_TYPE)
		p_packet_to_pc->data_size = 0;
}

static uint32_t abt_ksocket_send_exit(void)
{
	uint32_t ret = 0;
	struct msghdr msg;
	struct iovec iov;
	mm_segment_t oldfs;
	struct socket *sock;
	struct sockaddr_in addr;
	uint8_t buf = 1;

	ret = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock);
	if (ret < 0) {
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME
			": could not create a datagram socket, error = %d\n",
			-ENXIO);
		goto error;
	}

	memset(&addr, 0, sizeof(struct sockaddr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = in_aton("127.0.0.1");
	addr.sin_port = htons(DEFAULT_PORT);

	ret = sock->ops->connect(
			sock,
			(struct sockaddr *)&addr,
			sizeof(struct sockaddr),
			!O_NONBLOCK);

	if (ret < 0) {
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME
			": Could not connect to send socket, error = %d\n",
			-ret);
		goto error;
	} else {
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME": connect send socket (%s,%d)\n",
			"127.0.0.1",
			DEFAULT_PORT);
	}

	iov.iov_base = &buf;
	iov.iov_len = 1;

	msg.msg_flags = 0;
	msg.msg_name = &addr;
	msg.msg_namelen  = sizeof(struct sockaddr_in);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;

	oldfs = get_fs();

	set_fs(KERNEL_DS);
	ret = sock_sendmsg(sock, &msg, 1);
	TOUCH_D(DEBUG_BASE_INFO, ": exit send message return : %d\n", ret);
	set_fs(oldfs);
	sock_release(sock);

error:
	return ret;
}

static int abt_ksocket_send(struct socket *sock,
			struct sockaddr_in *addr,
			unsigned char *buf, int len)
{
	struct msghdr msg;
	struct iovec iov;
	mm_segment_t oldfs;
	int size = 0;

	if (sock == NULL)
		return 0;

	iov.iov_base = buf;
	iov.iov_len = len;

	msg.msg_flags = 0;
	msg.msg_name = addr;
	msg.msg_namelen  = sizeof(struct sockaddr_in);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;

	oldfs = get_fs();

	set_fs(KERNEL_DS);
	size = sock_sendmsg(sock, &msg, len);
	set_fs(oldfs);

	return size;
}

static int abt_set_report_mode(struct i2c_client *client, u32 mode)
{
	int ret;
	TOUCH_D(DEBUG_BASE_INFO,
		"[ABT](%s)mode:%d\n", __func__, mode);
	if (abt_report_mode == mode) {
		TOUCH_D(DEBUG_BASE_INFO,
			"[ABT](%s) mode(%d) is already set\n",
			__func__, mode);
		return NO_ERROR;
	}

	if (mode < 0) {
		TOUCH_D(DEBUG_BASE_INFO,
			"[ABT](%s) mode(%d) is invalid\n",
			__func__, mode);
		return ERROR;
	}

	ret = abt_force_set_report_mode(client, mode);

	return ret;

}

static uint32_t abt_ksocket_rcv_from_pctool(uint8_t *buf, uint32_t len)
{
	abt_comm.recv_packet = (struct s_comm_packet *)buf;

	TOUCH_D(DEBUG_BASE_INFO,
		MODULE_NAME
		": rev_cmd(%d), rev_addr(%d), rev_rw(%d), recv_dataS(%d)\n\n",
		abt_comm.recv_packet->cmd,
		abt_comm.recv_packet->addr,
		abt_comm.recv_packet->flag.rw,
		abt_comm.recv_packet->data_size);

	if (abt_comm.recv_packet->cmd == DEBUG_DATA_CAPTURE_MODE) {
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME ": DEBUG_DATA_CAPTURE_MODE\n\n");
		abt_set_report_mode(abt_comm.client,
					abt_comm.recv_packet->flag.data_type);
		sic_set_get_data_func(1);

		abt_report_point = 1;
		abt_report_ocd = 1;
	}	else if (abt_comm.recv_packet->cmd == DEBUG_DATA_RW_MODE) {
		TOUCH_D(DEBUG_BASE_INFO,
					MODULE_NAME ": DEBUG_DATA_RW_MODE\n\n");
		abt_recv_packet_parsing(abt_comm.client,
					abt_comm.recv_packet,
					&abt_comm.send_packet);
		/*abt_ksocket_send(abt_comm.sock,
				&abt_comm.addr,
				(u8 *)&abt_comm.send_packet,
				sizeof(s_comm_packet));*/
		abt_ksocket_send(abt_comm.client_sock,
				&abt_comm.client_addr,
				(u8 *)&abt_comm.send_packet,
				sizeof(struct s_comm_packet));
	}	else if (abt_comm.recv_packet->cmd == DEBUG_DATA_CMD_MODE) {
		TOUCH_D(DEBUG_BASE_INFO, ": DEBUG_DATA_CMD_MODE\n");
		mutex_lock(&abt_i2c_comm_lock);

		switch (abt_comm.recv_packet->addr) {
		case CMD_RAW_DATA_REPORT_MODE:
			TOUCH_D(DEBUG_BASE_INFO,
				MODULE_NAME": mode setting - %d\n",
				abt_comm.recv_packet->report_type);
			abt_set_report_mode(abt_comm.client,
					abt_comm.recv_packet->report_type);
			break;

		case CMD_RAW_DATA_COMPRESS:
			TOUCH_D(DEBUG_BASE_INFO,
				MODULE_NAME": data_compress val(%d)\n",
				abt_comm.recv_packet->report_type);
			sic_i2c_write(abt_comm.client,
			CMD_RAW_DATA_COMPRESS,
			(u8 *)&(abt_comm.recv_packet->report_type),
			sizeof(abt_comm.recv_packet->report_type));

			abt_compress_flag =
					abt_comm.recv_packet->report_type;
			break;

		case DEBUG_REPORT_POINT:
			abt_report_point =
					abt_comm.recv_packet->report_type;
			TOUCH_D(DEBUG_BASE_INFO,
					": report_point = %d\n",
					abt_report_point);
			break;

		case DEBUG_REPORT_OCD:
			abt_report_ocd =
					abt_comm.recv_packet->report_type;
			TOUCH_D(DEBUG_BASE_INFO, ": report_ocd = %d\n",
				abt_report_ocd);
			break;

		default:
			TOUCH_D(DEBUG_BASE_INFO, ": unknown command\n");
			break;
	}

	mutex_unlock(&abt_i2c_comm_lock);
	}

	return 0;
}

static uint32_t abt_ksocket_rcv_from_abtstudio(uint8_t *buf, uint32_t len)
{
	uint32_t cmd = (uint32_t)*((uint32_t *)buf);
	uint32_t val = (uint32_t)*((uint32_t *)(buf+4));
	TOUCH_D(DEBUG_BASE_INFO, ": CMD=%d VAL=%d\n", cmd, val);
	mutex_lock(&abt_i2c_comm_lock);
	switch (cmd) {
	case CMD_RAW_DATA_REPORT_MODE:
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME": mode setting - %d\n",
			val);
		abt_set_report_mode(abt_comm.client, val);
		break;

	case CMD_RAW_DATA_COMPRESS:
		sic_i2c_write(abt_comm.client,
			CMD_RAW_DATA_COMPRESS,
			(u8 *)&val, sizeof(val));
		abt_compress_flag = val;
		break;

	case DEBUG_REPORT_POINT:
		abt_report_point = val;
		break;

	case DEBUG_REPORT_OCD:
		abt_report_ocd = val;
		break;

	default:
		TOUCH_D(DEBUG_BASE_INFO, ": unknown command\n");
		break;
	}
	mutex_unlock(&abt_i2c_comm_lock);
	return 0;
}


static void abt_ksocket_exit(void)
{
	int err;

	if (abt_comm.thread == NULL) {
		if (abt_socket_mutex_flag == 1)
			mutex_destroy(&abt_socket_lock);
		abt_socket_mutex_flag = 0;
		abt_socket_report_mode = 0;
		abt_comm.running = 0;

		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME
			": no kernel thread to kill\n");

		return;
	}

	TOUCH_D(DEBUG_BASE_INFO,
		MODULE_NAME
		": start killing thread\n");

	abt_comm.running = 2;
	if (abt_ksocket_send_exit() < 0)
		TOUCH_D(DEBUG_BASE_INFO, ": send_exit error\n");

	err = kthread_stop(abt_comm.thread);

	if (err < 0) {
		TOUCH_D(DEBUG_BASE_INFO,
		MODULE_NAME
		": unknown error %d while trying to terminate kernel thread\n",
		-err);
	} else {
		while (abt_comm.running != 0)
			usleep(10000);
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME
			": succesfully killed kernel thread!\n");
	}

	abt_comm.thread = NULL;

	if (abt_comm.sock != NULL) {
		sock_release(abt_comm.sock);
		abt_comm.sock = NULL;
	}

	if (abt_comm.sock_send != NULL) {
		sock_release(abt_comm.sock_send);
		abt_comm.sock_send = NULL;
	}

	if (abt_comm.client_sock != NULL) {
		sock_release(abt_comm.client_sock);
		abt_comm.client_sock = NULL;
	}

	mutex_destroy(&abt_socket_lock);
	abt_socket_mutex_flag = 0;
	abt_socket_report_mode = 0;

	TOUCH_D(DEBUG_BASE_INFO, ": module unloaded\n");
}

int32_t abt_ksocket_raw_data_send(uint8_t *buf, uint32_t len)
{
	int ret = 0;
	static uint32_t connect_error_count;

	if (abt_comm.send_connected == 0)
		abt_ksocket_init_send_socket(&abt_comm);

	if (abt_comm.send_connected == 1) {
		ret = abt_ksocket_send(abt_comm.sock_send,
				&abt_comm.addr_send,
				buf, len);
	}	else {
		connect_error_count++;
		if (connect_error_count > 10) {
			TOUCH_D(DEBUG_BASE_INFO,
					": connection error - socket release\n");
			abt_force_set_report_mode(abt_comm.client, 0);
			abt_ksocket_exit();
		}
	}

	return ret;
}

static int32_t abt_ksocket_init(struct i2c_client *client,
			char *ip,
			uint32_t (*listener)(uint8_t *buf, uint32_t len))
{
	mutex_init(&abt_socket_lock);
	abt_socket_mutex_flag = 1;
	abt_socket_report_mode = 1;
	memcpy(abt_comm.send_ip, ip, 16);

	if (abt_conn_tool == ABT_STUDIO) {
		abt_comm.thread =
			kthread_run((void *)abt_ksocket_start_for_abtstudio,
						client, MODULE_NAME);
	}	else if (abt_conn_tool == ABT_PCTOOL) {
		abt_comm.thread =
			kthread_run((void *)abt_ksocket_start_for_pctool,
						client, MODULE_NAME);
	}

	/* abt_comm.thread =
				kthread_run((void *)tcp_init_module,
				client, MODULE_NAME);*/
	if (IS_ERR(abt_comm.thread)) {
		TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME
			": unable to start kernel thread\n");
		abt_comm.thread = NULL;
		return -ENOMEM;
	}

	abt_comm.sock_listener = listener;
	/* TOUCH_D(DEBUG_BASE_INFO,
			MODULE_NAME": init OK\n",
			__func__);*/

	return 0;
}

ssize_t show_abtApp(struct i2c_client *client, char *buf)
{
	int i;
	ssize_t retVal = 0;

	if (abt_head_flag) {
		if (abt_show_mode == REPORT_SEG1)
			abt_head[14] = DATA_TYPE_SEG1;
		else if (abt_show_mode == REPORT_SEG2)
			abt_head[14] = DATA_TYPE_SEG2;
		else if (abt_show_mode == REPORT_RAW)
			abt_head[14] = DATA_TYPE_RAW;
		else if (abt_show_mode == REPORT_BASELINE)
			abt_head[14] = DATA_TYPE_BASELINE;
		else if (abt_show_mode == REPORT_RNORG)
			abt_head[14] = DATA_TYPE_RN_ORG;

		retVal = sizeof(abt_head);
		memcpy(&buf[0], (u8 *)&abt_head[0], retVal);
		abt_head_flag = 0;
		return retVal;
	}

	switch (abt_show_mode) {
	case REPORT_RNORG:
	case REPORT_RAW:
	case REPORT_BASELINE:
	case REPORT_SEG1:
	case REPORT_SEG2:
		i = ACTIVE_SCREEN_CNT_X*
			ACTIVE_SCREEN_CNT_Y*2;
		memcpy(&buf[0], (u8 *)&abt_ocd[abt_ocd_read^1][0], i);
		memcpy(&buf[i], (u8 *)&abt_reportP, sizeof(abt_reportP));
		i += sizeof(abt_reportP);
		retVal = i;
		break;
	case REPORT_DEBUG_ONLY:
		memcpy(&buf[0], (u8 *)&abt_reportP, sizeof(abt_reportP));
		i = sizeof(abt_reportP);
		retVal = i;
		break;
	default:
		abt_show_mode = 0;
		retVal = 0;
		break;
	}
	return retVal;
}

ssize_t store_abtApp(struct i2c_client *client, const char *buf, size_t count)
{
	u32 mode;

	/* sscanf(buf, "%d", &mode); */
	mode = count;

	if (mode == HEAD_LOAD) {
		abt_head_flag	= 1;
		abt_ocd_on	= 1;
		TOUCH_D(DEBUG_BASE_INFO, "[ABT] abt_head load\n");
		return abt_head_flag;
	} else {
		abt_show_mode = mode;
		abt_head_flag = 0;
	}

	switch (abt_show_mode) {
	case REPORT_RNORG:
		TOUCH_D(DEBUG_BASE_INFO, "[ABT] show mode : RNORG\n");
		break;
	case REPORT_RAW:
		TOUCH_D(DEBUG_BASE_INFO, "[ABT] show mode : RAW\n");
		break;
	case REPORT_BASELINE:
		TOUCH_D(DEBUG_BASE_INFO, "[ABT] show mode : BASELINE\n");
		break;
	case REPORT_SEG1:
		TOUCH_D(DEBUG_BASE_INFO, "[ABT] show mode : SEG1\n");
		break;
	case REPORT_SEG2:
		TOUCH_D(DEBUG_BASE_INFO, "[ABT] show mode : SEG2\n");
		break;
	case REPORT_DEBUG_ONLY:
		TOUCH_D(DEBUG_BASE_INFO, "[ABT] show mode : DEBUG ONLY\n");
		break;
	case REPORT_OFF:
		TOUCH_D(DEBUG_BASE_INFO, "[ABT] show mode : OFF\n");
		break;
	default:
		TOUCH_D(DEBUG_BASE_INFO,
					"[ABT] show mode unknown : %d\n", mode);
		break;
	}

	return abt_show_mode;
}

ssize_t show_abtTool(struct i2c_client *client, char *buf)
{
	buf[0] = abt_report_mode_onoff;
	memcpy((u8 *)&buf[1], (u8 *)&(abt_comm.send_ip[0]), 16);
	TOUCH_D(DEBUG_BASE_INFO,
		MODULE_NAME
		":read raw report mode - mode:%d ip:%s\n",
		buf[0], (char *)&buf[1]);

	return 20;
}

ssize_t store_abtTool(struct i2c_client *client,
				const char *buf,
				size_t count)
{
	int mode = buf[0];
	char *ip = (char *)&buf[1];
	bool setFlag = false;
	TOUCH_D(DEBUG_BASE_INFO,
			":set raw report mode - mode:%d IP:%s\n", mode, ip);

	if (mode > 47)
		mode -= 48;

	switch (mode) {
	case 1:
		abt_conn_tool = ABT_STUDIO;
		if (abt_comm.thread == NULL) {
			TOUCH_D(DEBUG_BASE_INFO,
				":  mode ABT STUDIO Start\n\n");
			abt_ksocket_init(client,
			    (u8 *)ip,
			    abt_ksocket_rcv_from_abtstudio);
			sic_set_get_data_func(1);
			setFlag = true;
		}	else {
			if (memcmp((u8 *)abt_comm.send_ip,
					(u8 *)ip, 16) != 0) {
				TOUCH_D(DEBUG_BASE_INFO,
					":IP change->ksocket exit n init\n\n");
				abt_ksocket_exit();
				abt_ksocket_init(client, (u8 *)ip,
				abt_ksocket_rcv_from_abtstudio);
				setFlag = true;
			}
		}
		break;
	case 2:
		abt_conn_tool = ABT_PCTOOL;

		if (abt_comm.thread == NULL) {
				TOUCH_D(DEBUG_BASE_INFO,
				    ": mode PC TOOL Start\n\n");
				abt_ksocket_init(client, (u8 *)ip,
					abt_ksocket_rcv_from_pctool);
				sic_set_get_data_func(1);
				setFlag = true;

				abt_report_point = 1;
				abt_report_ocd = 1;
		}	else {
			TOUCH_D(DEBUG_BASE_INFO,
			      ": abt_comm.thread Not NULL\n\n");
			if (memcmp((u8 *)abt_comm.send_ip,
				     (u8 *)ip, 16) != 0) {
				abt_ksocket_exit();
				abt_ksocket_init(client, (u8 *)ip,
				   abt_ksocket_rcv_from_pctool);
				setFlag = true;

			} else {
				TOUCH_D(DEBUG_BASE_INFO,
				    ": same IP\n\n");
			}
		}
		break;
	default:
		abt_conn_tool = NOTHING;
		abt_ksocket_exit();
		sic_set_get_data_func(0);

		mutex_lock(&abt_i2c_comm_lock);
		abt_set_report_mode(client, 0);
		mutex_unlock(&abt_i2c_comm_lock);
	}

	if (setFlag) {
		mutex_lock(&abt_i2c_comm_lock);
		abt_set_report_mode(client, mode);
		mutex_unlock(&abt_i2c_comm_lock);
	}

	return mode;
}

int abt_force_set_report_mode(struct i2c_client *client, u32 mode)
{
	int ret = 0;
	u32 rdata = 0;
	u32 wdata = mode;
	u32 ram_region_access = 0;

	DO_SAFE(sic_i2c_read(client,
				tc_sp_ctl,
				(u8 *)&ram_region_access,
				sizeof(u32)),
				error);
	if (mode > 0) {
		ram_region_access |= 0x6;
		ram_region_access |= 0x1;
	} else if (mode == 0) {
		ram_region_access &= 0x79;
		ram_region_access |= 0x1;
	} else {
		TOUCH_D(DEBUG_BASE_INFO,
			"[ABT](%s) mode(%d) is invalid\n",
			__func__, mode);
		goto error;
	}
	DO_SAFE(sic_i2c_write(client,
				tc_sp_ctl,
				(u8 *)&ram_region_access,
				sizeof(u32)),
				error);

	/* send debug mode*/
	DO_SAFE(sic_i2c_write(client,
				CMD_RAW_DATA_REPORT_MODE,
				(u8 *)&wdata,
				sizeof(wdata)),
				error);
	abt_report_mode = mode;

	/* receive debug report buffer*/
	if (mode >= 0) {
		ret = sic_i2c_read(client,
					CMD_RAW_DATA_REPORT_MODE,
					(u8 *)&rdata,
					sizeof(rdata));
		if (ret < 0 || rdata <= 0) {
			TOUCH_D(DEBUG_BASE_INFO,
				"(%s)debug report buffer pointer error\n",
				__func__);
			goto error;
		}

		TOUCH_D(DEBUG_BASE_INFO,
			"(%s)debug report buffer pointer : 0x%x\n",
			__func__, rdata);
		CMD_GET_ABT_DEBUG_REPORT = (rdata - 0x20000000)/4;
	}

	return NO_ERROR;

error:
	sic_i2c_read(client, tc_sp_ctl, (u8 *)&ram_region_access, sizeof(u32));
	ram_region_access &= 0x79;
	ram_region_access |= 0x1;
	sic_i2c_write(client, tc_sp_ctl, (u8 *)&ram_region_access, sizeof(u32));

	wdata = 0;
	sic_i2c_write(client,
			CMD_RAW_DATA_REPORT_MODE,
			(u8 *)&wdata,
			sizeof(wdata));
	abt_report_mode = 0;
	abt_report_mode_onoff = 0;
	return ERROR;
}

#endif

