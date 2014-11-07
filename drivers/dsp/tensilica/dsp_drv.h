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

#include <asm/atomic.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>    /**< For isr */
#include <linux/irq.h>			/**< For isr */
#include <linux/wakelock.h>
#include "dsp_cfg.h"

#define RET_CHANGED_INFORMATION		1
#define RET_NO_CHANGED_INFORMATION	0
#define RET_OK						0
#define RET_ERROR					-1
#define RET_OUT_OF_MEMORY			-ENOMEM


typedef struct
{
	int						dev_open_count;
	dev_t					devno;
	struct cdev				cdev;
	bool					is_initialized;
}
DSP_DEVICE_T;

typedef struct
BUFFER_INFO {
	unsigned int buffer;
	unsigned int size;
	int eos_flag;
} BUFFER_INFO;


typedef struct
AUDIO_INFO {
	unsigned int sampling_freq;
	unsigned int bit_per_sample;
	unsigned int channel_number;
	unsigned int bit_rate;
	unsigned int codec_type;
} AUDIO_INFO;

#if !defined(WITH_BINARY)
extern int (*dsp_download_dram)(void *firmwareBuffer);
extern int (*dsp_download_iram)(void *firmwareBuffer);
#else
extern int (*dsp_download_dram)(void);
extern int (*dsp_download_iram)(void);
#endif

extern int (*dsp_power_on)(void);
extern void (*dsp_power_off)(void);

extern atomic_t status;

extern DSP_MEM_CFG *pst_dsp_mem_cfg;

#define	RECEIVE_SEND2		0
#define SEND_REPLY			0


#ifdef CONFIG_PM_WAKELOCKS
	extern struct wake_lock odin_wake_lock;
	extern char odin_wake_lock_name[32];
#endif
