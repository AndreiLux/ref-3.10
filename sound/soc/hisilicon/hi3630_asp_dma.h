/*
 * hi3630_asp_dma.h -- ALSA SoC HI3630 ASP DMA driver
 *
 * Copyright (c) 2013 Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HI3630_DMA_H__
#define __HI3630_DMA_H__

#ifndef __DRV_AUDIO_MAILBOX_WORK__
#define __DRV_AUDIO_MAILBOX_WORK__
#endif

/* hifi_pcm_play start*/
#define HI_CHN_COMMON \
	unsigned short msg_type; \
	unsigned short reserved; \
	unsigned short pcm_mode; \
	unsigned short reserved1;

enum HI3630_STATUS {
	STATUS_STOP	= 0,
	STATUS_RUNNING	= 1,
	STATUS_STOPPING	= 2
};

/* message type of audio data between ap & hifi */
enum AUDIO_MSG_ENUM {
	ID_AP_AUDIO_PLAY_START_REQ	= 0xDD31,
	ID_AP_AUDIO_PLAY_STOP_REQ	= 0xDD32,
	ID_AP_AUDIO_PLAY_DECODE_REQ	= 0xDD33,
	ID_AUDIO_AP_PLAY_DECODE_RSP	= 0xDD34,

	ID_AP_AUDIO_RECORD_START_REQ	= 0xDD40,
	ID_AP_AUDIO_RECORD_STOP_REQ	= 0xDD41,

	ID_AP_AUDIO_PCM_OPEN_REQ	= 0xDD25,
	ID_AP_AUDIO_PCM_CLOSE_REQ	= 0xDD26,
	ID_AP_AUDIO_PCM_HW_PARA_REQ	= 0xDD27,
	ID_AP_AUDIO_PCM_HW_FREE_REQ	= 0xDD28, /* reserved */
	ID_AP_AUDIO_PCM_PREPARE_REQ	= 0xDD29, /* reserved */
	ID_AP_AUDIO_PCM_TRIGGER_REQ	= 0xDD2A,
	ID_AP_AUDIO_PCM_POINTER_REQ	= 0xDD2B, /* reserved */
	ID_AP_AUDIO_PCM_SET_BUF_CMD	= 0xDD2C,
	ID_AUDIO_AP_PCM_PERIOD_ELAPSED_CMD = 0xDD2D,

	ID_AUDIO_UPDATE_PLAY_BUFF_CMD	= 0xDD2E,
	ID_AUDIO_UPDATE_CAPTURE_BUFF_CMD = 0xDD2F,
	ID_AUDIO_AP_PCM_TRIGGER_CNF	= 0xDDA0
};
typedef unsigned short AUDIO_MSG_ENUM_UINT16;

enum HIFI_CHN_MSG_TYPE {
	HI_CHN_MSG_PCM_OPEN		= ID_AP_AUDIO_PCM_OPEN_REQ,
	HI_CHN_MSG_PCM_CLOSE		= ID_AP_AUDIO_PCM_CLOSE_REQ,
	HI_CHN_MSG_PCM_HW_PARAMS	= ID_AP_AUDIO_PCM_HW_PARA_REQ,
	HI_CHN_MSG_PCM_HW_FREE		= ID_AP_AUDIO_PCM_HW_FREE_REQ,
	HI_CHN_MSG_PCM_PREPARE		= ID_AP_AUDIO_PCM_PREPARE_REQ,
	HI_CHN_MSG_PCM_TRIGGER		= ID_AP_AUDIO_PCM_TRIGGER_REQ,
	HI_CHN_MSG_PCM_POINTER		= ID_AP_AUDIO_PCM_POINTER_REQ,
	HI_CHN_MSG_PCM_SET_BUF		= ID_AP_AUDIO_PCM_SET_BUF_CMD,
	HI_CHN_MSG_PCM_PERIOD_ELAPSED	= ID_AUDIO_AP_PCM_PERIOD_ELAPSED_CMD,
	HI_CHN_MSG_PCM_PERIOD_STOP	= ID_AUDIO_AP_PCM_TRIGGER_CNF,
};

enum IRQ_RT {
	/* IRQ Not Handled as Other problem */
	IRQ_NH_OTHERS	= -5,
	/* IRQ Not Handled as Mailbox problem */
	IRQ_NH_MB	= -4,
	/* IRQ Not Handled as pcm MODE problem */
	IRQ_NH_MODE	= -3,
	/* IRQ Not Handled as TYPE problem */
	IRQ_NH_TYPE	= -2,
	/* IRQ Not Handled */
	IRQ_NH		= -1,
	/* IRQ HanDleD */
	IRQ_HDD		= 0,
	/* IRQ HanDleD related to PoinTeR */
	IRQ_HDD_PTR	= 1,
	/* IRQ HanDleD related to STATUS */
	IRQ_HDD_STATUS,
	/* IRQ HanDleD related to SIZE */
	IRQ_HDD_SIZE,
	/* IRQ HanDleD related to PoinTeR of Substream */
	IRQ_HDD_PTRS,
	/* IRQ HanDleD Error */
	IRQ_HDD_ERROR,
};
typedef enum IRQ_RT irq_rt_t;

typedef irq_rt_t (*irq_hdl_t)(void *, unsigned int);

#ifdef CONFIG_SND_TEST_AUDIO_PCM_LOOP
struct hi3630_simu_pcm_data {
	struct workqueue_struct *simu_pcm_delay_wq;
	struct delayed_work  simu_pcm_delay_work;

	unsigned short	msg_type;	/* HIFI_CHN_MSG_TYPE */
	unsigned short	reserved;	/* reserved for aligned */
	unsigned short	pcm_mode;	/* playback or capture */
	unsigned short	tg_cmd;		/* trigger command */
	void *		substream;	/* addr of substream */
	unsigned int	data_addr;	/* dma addr(physical address) */
	unsigned int	data_len;	/* dma length(byte) */
};
#endif

#ifdef __DRV_AUDIO_MAILBOX_WORK__
struct hi3630_pcm_mailbox_wq {
	struct workqueue_struct *pcm_mailbox_delay_wq;
};

struct hi3630_pcm_mailbox_data {
	struct workqueue_struct *pcm_mailbox_delay_wq;
	struct delayed_work pcm_mailbox_delay_work;

	unsigned short	msg_type;	/* HIFI_CHN_MSG_TYPE */
	unsigned short	pcm_mode;	/* playback or capture */
	void *		substream;	/* addr of substream */
};
#endif

struct hi3630_runtime_data {
	spinlock_t	lock;		/* protect hi3630_runtime_data */
	unsigned int	period_next;	/* record which period to fix dma next time */
	unsigned int	period_cur;	/* record which period using now */
	unsigned int	period_size;	/* DMA SIZE */
	enum HI3630_STATUS status;	/* pcm status running or stop */
#ifdef __DRV_AUDIO_MAILBOX_WORK__
	struct hi3630_pcm_mailbox_data hi3630_pcm_mailbox;
#endif
#ifdef CONFIG_SND_TEST_AUDIO_PCM_LOOP
	struct hi3630_simu_pcm_data HI3630_simu_pcm;
#endif
};

struct hi3630_asp_dmac_data {
	struct hwspinlock *hwlock;
	struct resource	*res;
	void __iomem	*reg_base_addr;
	int		irq;
	spinlock_t 	lock;
	struct device *dev;
};

struct hifi_chn_pcm_open {
	HI_CHN_COMMON
};

struct hifi_chn_pcm_close {
	HI_CHN_COMMON
};

struct hifi_chn_pcm_hw_params {
	HI_CHN_COMMON
	unsigned int channel_num;
	unsigned int sample_rate;
	unsigned int format;
};

struct hifi_chn_pcm_trigger {
	unsigned short	msg_type;	/* HIFI_CHN_MSG_TYPE */
	unsigned short	reserved;	/* reserved for aligned */
	unsigned short	pcm_mode;	/* playback or capture */
	unsigned short	tg_cmd;		/* trigger command */
	unsigned short	enPcmObj;	/* pcm object, ap or hifi */
	unsigned short	reserved1;	/* reserved for aligned */
	void *		substream;	/* addr of substream */
	unsigned int	data_addr;	/* dma addr(physical address) */
	unsigned int	data_len;	/* dma length(byte) */
};

struct hifi_channel_set_buffer {
	HI_CHN_COMMON
	unsigned int	data_addr;	/* dma addr(physical address) */
	unsigned int	data_len;	/* dma length(byte) */
};

struct hifi_chn_pcm_period_elapsed {
	HI_CHN_COMMON
	void *		substream;	/* addr of substream */
};


#endif


