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

#ifndef AUDIO_GLOBAL_H
#define AUDIO_GLOBAL_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <mach/irqs.h>
#include <mach/sync_write.h>
#include <linux/xlog.h>
#include <mach/mt_typedefs.h>
#include <AudDrv_Def.h>

struct AFE_BLOCK_T {
	kal_uint32 pucPhysBufAddr;
	kal_uint8 *pucVirtBufAddr;
	kal_int32 u4BufferSize;
	kal_int32 u4DataRemained;
	kal_uint32 u4SampleNumMask;	/* sample number mask */
	kal_uint32 u4SamplesPerInt;	/* number of samples to play before interrupting */
	kal_int32 u4WriteIdx;	/* Previous Write Index. */
	kal_int32 u4DMAReadIdx;	/* Previous DMA Read Index. */
	kal_uint32 u4fsyncflag;
	kal_uint32 uResetFlag;
};

struct AFE_MEM_CONTROL_T {
	struct file *flip;
	struct AFE_BLOCK_T rBlock;
	kal_bool bRunning;
	kal_uint32 MemIfNum;
};

struct pcm_afe_info {
	unsigned long dma_addr;
	struct snd_pcm_substream *substream;
	unsigned int pcm_irq_pos;	/* IRQ position */
	uint32_t samp_rate;
	uint32_t channel_mode;
	uint8_t start;
	uint32_t dsp_cnt;
	uint32_t buf_phys;
	int32_t mmap_flag;
	bool prepared;
	struct AFE_MEM_CONTROL_T *pAfe_MEM_Control;
};
#endif
