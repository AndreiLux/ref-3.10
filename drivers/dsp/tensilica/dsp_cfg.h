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

#define	DSP_MODULE			"tenilica_dsp"

#define	KDRV_MAJOR_BASE		140
#define DSP_MAJOR			(KDRV_MAJOR_BASE+1)
#define DSP_OMX_MINOR		0
#define DSP_LPA_MINOR		1
#define DSP_PCM_MINOR		2
#define DSP_ALSA_MINOR		3

typedef struct
{
	unsigned int	sys_table_addr;
	unsigned int	sys_table_size;
	unsigned int	stream_buffer_addr;
	unsigned int	stream_buffer_size;
	unsigned int	pcm_buffer_addr;
	unsigned int	pcm_buffer_size;
	unsigned int	cpb1_address;
	unsigned int	cpb1_size;
	unsigned int	cpb2_address;
	unsigned int	cpb2_size;
	unsigned int	dpb1_address;
	unsigned int	dpb1_size;
	unsigned int	dpb2_address;
	unsigned int	dpb2_size;
} DSP_MEM_CFG;

extern DSP_MEM_CFG dsp_mem_cfg[];
