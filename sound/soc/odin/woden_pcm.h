/*
 * woden_pcm.h - Definitions for Woden PCM driver
 *
 * Author: Hyunhee Jeon <hhjeon@lge.com>
 * Copyright (c) 2012, LGE Corporation.
 *
 * Based on code copyright/by:
 *
 * Copyright (C) 2010 Google, Inc.
 * Iliyan Malchev <malchev@google.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __WODEN_PCM_H__
#define __WODEN_PCM_H__

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/wakelock.h>
#endif

struct woden_runtime_data {
	struct snd_pcm_substream *substream;
	spinlock_t lock;
	int running;
	int dma_pos;
	int dma_pos_end;
	int period_index;
	int period_bytes;
#ifdef CONFIG_PM_WAKELOCKS
	struct wake_lock woden_wake_lock;
	char woden_wake_lock_name[32];
#endif
	struct dma_async_tx_descriptor *desc;
	struct dma_chan *dma_chan;
	struct woden_pcm_dma_params *dma_params;
	struct scatterlist *sg;
};

/* Below code will be used when using DMA Mode */

struct woden_pcm_dma_params {
	unsigned long addr;
	unsigned long max_burst;
	unsigned long addr_width;
	int chan_num;
};

struct woden_pcm_runtime_data {
	int period_bytes;
	int periods;
	int dma;
	int dma_pos;
	int dma_pos_end;
	unsigned long offset;
	unsigned long size;
	void *buf;
	int period_time;
	struct dma_async_tx_descriptor *desc;
	struct dma_chan *dma_chan;
	//struct woden_pcm_dma_params *dma_params;
};

int woden_pcm_platform_register(struct device *dev);
void woden_pcm_platform_unregister(struct device *dev);

#endif

