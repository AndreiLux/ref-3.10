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

extern struct file_operations gst_dsp_lpa_fops;
extern int buffer_number;

typedef struct
{
	unsigned int	sys_table_addr;
	unsigned int	sys_table_size;
	unsigned int	stream_buffer_addr;
	unsigned int	stream_buffer_size;
	unsigned int	pcm_buffer_addr;
	unsigned int	pcm_buffer_size;
}
DSP_LPA_MEM_CFG;


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
			unsigned int timestamp_upper	:32; /* 32:63   */
			unsigned int timestamp_lower	:32; /* 0:31	*/
		} field;
		long long value;
	} timestamp;
	unsigned int eos_flag;
	LPA_AUDIO_INFO audio_info;
	wait_queue_head_t wait;
	unsigned int lpa_readqueue_count;
} EVENT_LPA;


extern DSP_LPA_MEM_CFG dsp_lpa_mem_cfg;

extern struct workqueue_struct *lpa_workqueue;
extern struct work_struct lpa_work;

extern atomic_t lpa_status;

#define	LPA_OFF		0
#define	LPA_ON		1
#define	LPA_READY	2
#define	LPA_CREATE	3
#define	LPA_OPEN	4
#define	LPA_INIT	5
#define	LPA_PLAY	6
#define	LPA_PAUSE	7
#define	LPA_STOP	8


#define	PLAY_PP			1
#define	PLAY_ENTIRE		0

#if PLAY_PP
#define LPA_DOUBLE_BUFFER	1
#define LPA_NEAR_STOP		0
#elif PLAY_ENTIRE
#define LPA_DOUBLE_BUFFER	0
#define LPA_NEAR_STOP		1
#endif
