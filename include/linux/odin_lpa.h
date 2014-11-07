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


#ifndef __LINUX_ODIN_LPA_H__
#define __LINUX_ODIN_LPA_H__

#include <linux/odin_mailbox.h>

#define CONFIG_LPA_ENABLE		1
#define LPA					8
#define LPA_OFFSET			28
#define SUB_BLOCK_OFFSET	24
#define SUB_BLOCK_MASK		0xF
#define LPA_START			0x0
#define LPA_STOP			0x1
#define LPA_WRITE_DONE		0x2
#define SMS_LPA_WRITE		0x5
#define SMS_LPA_START		0x6
#define SMS_LPA_STOP		0x7
#define SMS_LPA_RESUME		0x8
#define SMS_LPA_PAUSE		0x9
#define SMS_LPA_DMA_START	0xA
#define SMS_LPA_FILL_BUFFER	0xB
#define SMS_LPA_PCM_DECODING_START	0xC
#define SMS_LPA_PCM_DECODING_END	0xD

#define PM_SUB_BLOCK_SDIO		1
#define PM_SUB_BLOCK_DSS		2
#define PM_SUB_BLOCK_LPA		3


extern int odin_lpa_start(unsigned int sub_block, unsigned int id);
extern int odin_lpa_stop(unsigned int sub_block, unsigned int id);
extern int odin_lpa_write_done(unsigned int sub_block, unsigned int id);


extern int lpa_register_client(struct notifier_block *nb);
extern int lpa_unregister_client(struct notifier_block *nb);

extern int odin_sms_lpa_write(unsigned int sub_block, unsigned int address, unsigned int size);
extern int odin_sms_lpa_start(unsigned int sub_block);
extern int odin_sms_lpa_stop(unsigned int sub_block);
extern int odin_sms_lpa_resume(unsigned int sub_block);
extern int odin_sms_lpa_pause(unsigned int sub_block);
extern int odin_sms_lpa_dma_start(unsigned int sub_block);
extern int odin_sms_lpa_pcm_decoding_start(unsigned int sub_block);
extern int odin_sms_lpa_pcm_decoding_end(unsigned int sub_block);

extern int lpa_enable(int lpa_ipc);
extern int lpa_disable();
extern int get_lpa_state();


#endif /* __LINUX_ODIN_LPA_H__ */
