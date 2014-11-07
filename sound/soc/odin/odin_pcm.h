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

#ifndef __ODIN_PCM_H__
#define __ODIN_PCM_H__

/* Below code will be used when using DMA Mode */

#define ODIN_PCM_FOR_NONE		0
#define ODIN_PCM_FOR_SINGLE		1
#define ODIN_PCM_FOR_MULTI		2

struct odin_pcm_dma_params {
	unsigned long addr;
	unsigned long max_burst;
	unsigned long addr_width;
	int chan_num;
	int transfer_devtype;
	void *pcm_device;
};

struct odin_pcm_platform_data {
	char *dma_dev_name;
	void *dma_dev;
};

#endif

