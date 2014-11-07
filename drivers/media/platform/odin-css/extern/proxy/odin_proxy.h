/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef MSM_PROXY_H
#define MSM_PROXY_H

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <media/v4l2-subdev.h>

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

#define PROXY_SUCCESS 0
#define PROXY_FAIL    -1
#define PROXY_INIT_OLD_MODULE		1
#define PROXY_INIT_NOT_SUPPORTED  -2
#define PROXY_INIT_CHECKSUM_ERROR -3
#define PROXY_INIT_EEPROM_ERROR   -4
#define PROXY_INIT_I2C_ERROR      -5
#define PROXY_INIT_TIMEOUT		-6
#define PROXY_INIT_LOAD_BIN_ERROR -7
#define PROXY_INIT_NOMEM			-8
#define PROXY_INIT_GYRO_ADJ_FAIL	 2

#define LASER_SLAVE_ID		0x52
#define CAMERA_SLAVE_ID     0xA0

#define IOCTL_PROXY_MAGIC 'L'

#define LASER_IOC_SET_PROXY_CFG		_IOWR(IOCTL_PROXY_MAGIC, 0,\
					uint32_t)

#define LASER_IOC_GET_PROXY_DATA	_IOWR(IOCTL_PROXY_MAGIC, 1,\
					uint32_t)

enum vl6180_ldaf_i2c_data_type {
	MSM_CAMERA_I2C_BYTE_DATA = 1,
	MSM_CAMERA_I2C_WORD_DATA = 2,
	MSM_CAMERA_I2C_DWORD_DATA= 4,
};

enum vl6180_ldaf_cmd_mode {
	PROXY_ON = 0,
	PROXY_THREAD_ON,
	PROXY_THREAD_PAUSE,
	PROXY_THREAD_RESTART,
	PROXY_THREAD_OFF,
	PROXY_CAL,
};

struct  vl6180_proxy_info {
	uint16_t proxy_val[3];
	uint32_t proxy_conv;
	uint32_t proxy_sig;
	uint32_t proxy_amb;
	uint32_t proxy_raw;
	uint32_t cal_count;
	uint32_t cal_done;
};

struct vl6180_power {
	uint16_t gpio_enable;
	uint16_t gpio_interrupt;
};

struct vl6180_proxy_ctrl {
	struct i2c_client *client;
	struct mutex proxy_lock;
	struct work_struct proxy_work;
	struct workqueue_struct *work_thread;
	struct vl6180_proxy_info *proxy_info;

	struct vl6180_power proxy_power;

	uint16_t last_proxy;
	uint8_t exit_workqueue;
	uint8_t pause_workqueue;
	uint8_t wq_init_success;
	uint32_t max_i2c_fail_thres;
	uint32_t i2c_fail_cnt;
	uint8_t proxy_cal;
};

int32_t vl6180_i2c_read(const char *buf, int count);
int32_t vl6180_i2c_write(const char *buf, int count);
int32_t vl6180_camera_i2c_read(const char *buf, int count);
int32_t proxy_i2c_write_seq(uint32_t addr, uint8_t *data, uint16_t num_byte);

uint16_t vl6180_proxy_cal(void);
int vl6180_init_proxy();

uint16_t vl6180_proxy_thread_start(void);
uint16_t vl6180_proxy_thread_end(void);
uint16_t vl6180_proxy_thread_pause(void);
uint16_t vl6180_proxy_thread_restart(void);

void vl6180_proxy_stop_by_pd_off();

#endif
