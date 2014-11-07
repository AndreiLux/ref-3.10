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

#include "dsp_reg_odin.h"

#define ODIN_REMAP_VALUE	0x6D		/* 0xDB00_0000 */

struct dsp_buffer {
       unsigned char *buffer_virtual_address;
       unsigned int buffer_size;
};

enum dsp_odin_mem_resources {
	ODIN_DSP_IOMEM_BASE,
	ODIN_DSP_IOMEM_AUD_SRAM,
	ODIN_DSP_IOMEM_AUD_DRAM0,
	ODIN_DSP_IOMEM_AUD_DRAM1,
	ODIN_DSP_IOMEM_AUD_IRAM0,
	ODIN_DSP_IOMEM_AUD_IRAM1,
	ODIN_DSP_IOMEM_AUD_MAILBOX,
	ODIN_DSP_IOMEM_AUD_INTERRUPT,
	ODIN_DSP_IOMEM_AUD_CTR,
};

struct odin_dsp_mem_ctl {
	unsigned int *dsp_control_register;
	unsigned int dsp_control_register_size;
	unsigned int *dsp_dram0_address;
	unsigned int dsp_dram0_size;
	unsigned int *dsp_dram1_address;
	unsigned int dsp_dram1_size;
	unsigned int *dsp_iram0_address;
	unsigned int dsp_iram0_size;
	unsigned int *dsp_iram1_address;
	unsigned int dsp_iram1_size;
};

struct odin_dsp_device {
	struct odin_dsp_mem_ctl *aud_mem_ctl;
	struct aud_control_odin *aud_reg_ctl;
	unsigned int dsp_irq;
};

typedef struct
DSP_WAIT {
	wait_queue_head_t wait;
	spinlock_t dsp_lock;
}DSP_WAIT;

typedef struct
LPA_AUDIO_INFO {
	unsigned int sampling_freq;
	unsigned int bit_per_sample;
	unsigned int channel_number;
	unsigned int bit_rate;
	unsigned int codec_type;
} LPA_AUDIO_INFO;

typedef struct
EVENT_LPA
{
	unsigned int command;
	unsigned int type;
	unsigned int address;
	union timestamp {
		struct {
			unsigned int timestamp_lower	:32; /* 0:31	*/
			unsigned int timestamp_upper	:32; /* 32:63   */
		} field;
		long long value;
	} timestamp;
	union decoded_frame {
		struct {
			unsigned int decoded_frame_upper	:32; /* 32:63   */
			unsigned int decoded_frame_lower	:32; /* 0:31	*/
		} field;
		long long value;
	} decoded_frame;
	unsigned int eos_flag;
	LPA_AUDIO_INFO audio_info;
	wait_queue_head_t wait;
	unsigned int lpa_readqueue_count;
} EVENT_LPA;

typedef struct LPA_BUFFER_LIST
{
	struct list_head list;
	spinlock_t lock;
	unsigned int buffer_num;
	unsigned int eos_flag;
} LPA_BUFFER_LIST;

irqreturn_t odin_dsp_interrupt(int irq, void *data);

extern struct workqueue_struct *workqueue_odin_dsp;
extern struct work_struct odin_dsp_work;

extern EVENT_LPA *event_lpa;

extern DSP_WAIT *dsp_wait;

extern LPA_BUFFER_LIST *lpa_buffer_list;
