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

extern struct file_operations gst_dsp_alsa_fops;
extern struct workqueue_struct *alsa_workqueue;
extern struct work_struct alsa_work;

extern void dsp_alsa_pcm_write(unsigned int buffer,
		unsigned int size,
		unsigned int count,
		void (*callback_func)(unsigned long callback_param),
		unsigned long *run_info);

extern void dsp_alsa_pcm_read(unsigned int buffer,
		unsigned int size,
		unsigned int count,
		void (*callback_func)(unsigned long callback_param),
		unsigned long *run_info);

extern void dsp_alsa_pcm_stop(void);

typedef struct
{
	unsigned int	sys_table_addr;
	unsigned int	sys_table_size;
	unsigned int	stream_buffer_addr;
	unsigned int	stream_buffer_size;
	unsigned int	pcm_buffer_addr;
	unsigned int	pcm_buffer_size;
}
DSP_ALSA_MEM_CFG;

typedef struct
EVENT_ALSA
{
	unsigned int command;
	unsigned int type;
	unsigned int address;
	unsigned int operation;
	wait_queue_head_t wait;
} EVENT_ALSA;

typedef struct ALSA_RUN_INFO
{
	unsigned int ops;
	unsigned int buffer;
	unsigned int size;
	unsigned int count;
	unsigned long *callback_param;
	void (*callback_func)(unsigned long *callback_param);
} ALSA_RUN_INFO;

extern ALSA_RUN_INFO alsa_run_info;
extern EVENT_ALSA *event_alsa;
