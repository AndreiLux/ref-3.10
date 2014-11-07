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

#include "dsp_cfg.h"

#define WODEN_DSP_BASE_ADDRESS 0x9E000000

#define ODIN_DSP_BASE_ADDRESS 0xDE000000

DSP_MEM_CFG dsp_mem_cfg[] =
{
	/* For Woden */
	{
		/* For LPA */
		.sys_table_addr = WODEN_DSP_BASE_ADDRESS,
		.sys_table_size = 0x1,
		.stream_buffer_addr = WODEN_DSP_BASE_ADDRESS+0x100000,
		.stream_buffer_size = 516096, /* 504*1024 */
		.pcm_buffer_addr = WODEN_DSP_BASE_ADDRESS+0xA00000,
		.pcm_buffer_size = 0x5000,
		/* For OMX */
		.cpb1_address = WODEN_DSP_BASE_ADDRESS+0x100000,
		.cpb1_size = 0x1400,
		.cpb2_address = WODEN_DSP_BASE_ADDRESS+0x101400,
		.cpb2_size = 0x1400,
		.dpb1_address = WODEN_DSP_BASE_ADDRESS+0x200000,
		.dpb1_size = 0x5000,
		.dpb2_address = WODEN_DSP_BASE_ADDRESS+0x205000,
		.dpb2_size = 0x5000,
	},
	/* For Odin */
	{
		/* For LPA */
		.sys_table_addr = ODIN_DSP_BASE_ADDRESS,
		.sys_table_size = 0x1,
		.stream_buffer_addr = ODIN_DSP_BASE_ADDRESS+0x100000,
		//.stream_buffer_size = 2580480,
		.stream_buffer_size = 774144,
		/* .stream_buffer_size = 524288, //512*1024 */
		.pcm_buffer_addr = ODIN_DSP_BASE_ADDRESS+0xA00000,
		.pcm_buffer_size = 0x5000,
		/* For OMX */
		.cpb1_address = ODIN_DSP_BASE_ADDRESS+0x100000,
		.cpb1_size = 0x1400,
		.cpb2_address = ODIN_DSP_BASE_ADDRESS+0x101400,
		.cpb2_size = 0x1400,
		.dpb1_address = ODIN_DSP_BASE_ADDRESS+0x200000,
		.dpb1_size = 0x5000,
		.dpb2_address = ODIN_DSP_BASE_ADDRESS+0x205000,
		.dpb2_size = 0x5000,
	},
};
