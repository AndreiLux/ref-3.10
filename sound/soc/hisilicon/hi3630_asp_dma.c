/*
 * hi3630_asp_dmac.c -- ALSA SoC HI3630 ASP DMA driver
 *
 * Copyright (c) 2013 Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define LOG_TAG "hi3630_asp_dmac"

#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/compiler.h>
#include <linux/hwspinlock.h>
#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include "drv_mailbox_cfg.h"

#include "hi3630_log.h"
#include "hi3630_asp_dma.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif
#define __DRV_AUDIO_MAILBOX_WORK__

#ifndef OK
#define OK	0
#endif

#ifndef ERROR
#define ERROR	-1
#endif

#undef NULL
#define NULL ((void *)0)

#define HI3630_MAX_BUFFER_SIZE	( 128 * 1024 )	/* 0x20000 */

static const unsigned int freq[] = {
	8000,	11025,	12000,	16000,
	22050,	24000,	32000,	44100,
	48000,	88200,	96000,	176400,
	192000,
};

static const struct of_device_id hi3630_asp_dmac_of_match[] = {
	{
		.compatible = "hisilicon,hi3630-pcm-asp-dma",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, hi3630_asp_dmac_of_match);

static const struct snd_pcm_hardware hi3630_asp_dmac_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S16_BE |
				  SNDRV_PCM_FMTBIT_S24_LE |
				  SNDRV_PCM_FMTBIT_S24_BE,
	.period_bytes_min	= 32,
	.period_bytes_max	= 16 * 1024,
	.periods_min		= 2,
	.periods_max		= 32,
	.buffer_bytes_max	= 128 * 1024,
};

#ifdef CONFIG_SND_TEST_AUDIO_PCM_LOOP
struct hi3630_simu_pcm_data hi3630_simu_pcm;
#endif

#ifdef __DRV_AUDIO_MAILBOX_WORK__
/* workqueue for playback and capture */
struct hi3630_pcm_mailbox_wq hi3630_pcm_mailbox_workqueue;
#endif

static u32 pcm_cp_status_open = 0;
static u32 pcm_pb_status_open = 0;

DEFINE_SEMAPHORE(g_pcm_cp_open_sem);
DEFINE_SEMAPHORE(g_pcm_pb_open_sem);

extern unsigned long mailbox_send_msg(unsigned long mailcode,
					void *data, unsigned long length);

extern unsigned long mailbox_reg_msg_cb(unsigned long mailcode,
					mb_msg_cb func, void *data);

extern unsigned long mailbox_read_msg_data(void *mail_handle,
					char *buff, unsigned long *size);

#ifdef CONFIG_SND_TEST_AUDIO_PCM_LOOP
static irq_rt_t hi3630_notify_recv_isr(void *usr_para, void *mail_handle,
					unsigned int mail_len);
#endif

#ifdef __DRV_AUDIO_MAILBOX_WORK__
static irq_rt_t hi3630_mb_intr_handle(struct snd_pcm_substream *substream,
					unsigned short pcm_mode);
#endif

#ifdef CONFIG_SND_TEST_AUDIO_PCM_LOOP
void simu_pcm_work_func(struct work_struct *work)
{
	struct hi3630_simu_pcm_data *priv =
		container_of(work, struct hi3630_simu_pcm_data, simu_pcm_delay_work.work);
	struct hifi_chn_pcm_period_elapsed pk_data;

	BUG_ON(NULL == priv);

	logd("simu_pcm_work_func:\r\n");
	logd("\tmsg_type = %x\r\n", priv->msg_type);
	logd("\tpcm_mode = %d\r\n", priv->pcm_mode);
	logd("\tdata_addr = 0x%x\r\n", priv->data_addr);
	logd("\tdata_len = %d\r\n", priv->data_len);
	logd("\tsubstream = 0x%x\r\n", priv->substream);

	/* simu data move and make response */
	memset(&pk_data, 0, sizeof(struct hifi_chn_pcm_period_elapsed));
	pk_data.msg_type = HI_CHN_MSG_PCM_PERIOD_ELAPSED;
	pk_data.pcm_mode = priv->pcm_mode;
	pk_data.substream = priv->substream;

	hi3630_notify_recv_isr(0, &pk_data, sizeof(struct hifi_chn_pcm_period_elapsed));
}
#endif

#ifdef __DRV_AUDIO_MAILBOX_WORK__
void pcm_mailbox_work_func(struct work_struct *work)
{
	struct hi3630_pcm_mailbox_data *priv =
		container_of(work, struct hi3630_pcm_mailbox_data, pcm_mailbox_delay_work.work);
	int ret = 0;
	unsigned short msg_type= 0;
	unsigned short pcm_mode = 0;
	struct snd_pcm_substream * substream = NULL;
	struct hi3630_runtime_data *prtd = NULL;

	BUG_ON(NULL == priv);

	msg_type = priv->msg_type;
	pcm_mode = priv->pcm_mode;
	substream = priv->substream;
	logd("substream : 0x%x\n", substream);

	if (NULL == substream) {
		loge("End, substream == NULL\n");
		return;
	}

	if (NULL == substream->runtime) {
		loge("End, substream->runtime == NULL\n");
		return;
	}

	prtd = (struct hi3630_runtime_data *)substream->runtime->private_data;

	if (NULL == prtd) {
		loge("End, prtd is null \n");
		return;
	}

	switch(msg_type) {
	case HI_CHN_MSG_PCM_PERIOD_ELAPSED:
		ret = hi3630_mb_intr_handle(substream, pcm_mode);
		if (IRQ_NH == ret) {
			loge("ret : %d\n", ret);
			return;
		}
		break;
	case HI_CHN_MSG_PCM_PERIOD_STOP:
		if (STATUS_STOPPING == prtd->status){
			prtd->status = STATUS_STOP;
			logd("stop now !\n");
		}
		break;
	default:
		loge("msg_type 0x%x\n", msg_type);
		break;
	}

	return;
}
#endif

int hi3630_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = hi3630_asp_dmac_hardware.buffer_bytes_max;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);

	if (!buf->area) {
		loge("dma_alloc_writecombine error\n");
		return -ENOMEM;
	}

	buf->bytes = size;

	return 0;
}
EXPORT_SYMBOL(hi3630_pcm_preallocate_dma_buffer);

void hi3630_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_writecombine(pcm->card->dev, buf->bytes,
				      buf->area, buf->addr);
		buf->area = NULL;
	}
}
EXPORT_SYMBOL(hi3630_pcm_free_dma_buffers);

static int hi3630_mailbox_send_data(void *pmsg_body, unsigned int msg_len,
					unsigned int msg_priority)
{
	int ret = 0;

#ifdef CONFIG_SND_TEST_AUDIO_PCM_LOOP
	struct hifi_chn_pcm_trigger *pcm_trigger = (struct hifi_chn_pcm_trigger *)pmsg_body;

	/* simu */
	switch (pcm_trigger->msg_type) {
	case HI_CHN_MSG_PCM_TRIGGER :
		if((SNDRV_PCM_TRIGGER_START == pcm_trigger->tg_cmd) ||
				(SNDRV_PCM_TRIGGER_RESUME == pcm_trigger->tg_cmd) ||
				(SNDRV_PCM_TRIGGER_PAUSE_RELEASE == pcm_trigger->tg_cmd)) {
			hi3630_simu_pcm.msg_type = (pcm_trigger)->msg_type;
			hi3630_simu_pcm.pcm_mode = (pcm_trigger)->pcm_mode;
			hi3630_simu_pcm.substream = (pcm_trigger)->substream;
			hi3630_simu_pcm.data_addr = (pcm_trigger)->data_addr;
			hi3630_simu_pcm.data_len = (pcm_trigger)->data_len;
			queue_delayed_work(hi3630_simu_pcm.simu_pcm_delay_wq,
					&hi3630_simu_pcm.simu_pcm_delay_work,
					msecs_to_jiffies(20));
		}
		break;
	case HI_CHN_MSG_PCM_SET_BUF :
		hi3630_simu_pcm.msg_type = ((struct hifi_channel_set_buffer *)pmsg_body)->msg_type;
		hi3630_simu_pcm.pcm_mode = ((struct hifi_channel_set_buffer *)pmsg_body)->pcm_mode;
		hi3630_simu_pcm.data_addr = ((struct hifi_channel_set_buffer *)pmsg_body)->data_addr;
		hi3630_simu_pcm.data_len = ((struct hifi_channel_set_buffer *)pmsg_body)->data_len;
		queue_delayed_work(hi3630_simu_pcm.simu_pcm_delay_wq,
				&hi3630_simu_pcm.simu_pcm_delay_work,
				msecs_to_jiffies(0));
		break;
	default:
		logd("hi3630_mailbox_send_data MSG_TYPE(0x%x)\r\n", (pcm_trigger)->msg_type);
		break;
	}

#else

	ret = mailbox_send_msg(MAILBOX_MAILCODE_ACPU_TO_HIFI_AUDIO, pmsg_body, msg_len);
	if (0 != ret) {
		extern void hifi_dump_panic_log(void);
		hifi_dump_panic_log();
		loge("ret=%d\n", ret);
	}
#endif

	return ret;
}

static bool check_pcm_mode(unsigned short pcm_mode)
{
	if ((SNDRV_PCM_STREAM_PLAYBACK != pcm_mode)
			&& (SNDRV_PCM_STREAM_CAPTURE != pcm_mode)) {
		loge("pcm_mode=%d\n", pcm_mode);
		return false;
	}

	return true;
}

static int hi3630_notify_pcm_set_buf(struct snd_pcm_substream *substream)
{
	struct hi3630_runtime_data *prtd =
			(struct hi3630_runtime_data *)substream->runtime->private_data;
	unsigned int period_size = 0;
	unsigned short pcm_mode = (unsigned short)substream->stream;
	struct hifi_channel_set_buffer msg_body = {0};
	int ret = 0;

	if (!check_pcm_mode(pcm_mode))
		return -EINVAL;

	BUG_ON(NULL == prtd);

	period_size = prtd->period_size;

	msg_body.msg_type	= (unsigned short)HI_CHN_MSG_PCM_SET_BUF;
	msg_body.pcm_mode	= pcm_mode;
	msg_body.data_addr	= substream->runtime->dma_addr + (prtd->period_next * period_size);
	msg_body.data_len	= period_size;

	logd("d_addr=0x%x(%d)\r\n", msg_body.data_addr, msg_body.data_len);

	if (STATUS_RUNNING != prtd->status){
		loge("pcm is closed \n");
		return -EINVAL;
	}

	/* mail-box send */
	ret = hi3630_mailbox_send_data(&msg_body, sizeof(struct hifi_channel_set_buffer), 0);
	if (OK != ret)
		ret = -EBUSY;

	logd("End(%d)\r\n", ret);
	return ret;
}

static irq_rt_t hi3630_intr_handle(struct snd_pcm_substream *substream)
{
	struct hi3630_runtime_data *prtd = NULL;
	unsigned int rt_period_size = 0;
	unsigned int num_period = 0;
	snd_pcm_uframes_t avail = 0;
	int ret = OK;

	if (NULL == substream) {
		loge("End, substream == NULL\n");
		return IRQ_HDD_PTR;
	}

	if (NULL == substream->runtime) {
		loge("End, substream->runtime == NULL\n");
		return IRQ_HDD_PTR;
	}

	prtd = (struct hi3630_runtime_data *)substream->runtime->private_data;
	rt_period_size = substream->runtime->period_size;
	num_period = substream->runtime->periods;

	if (NULL == prtd) {
		loge("End, prtd == NULL\n");
		return IRQ_HDD_PTR;
	}

	spin_lock(&prtd->lock);
	++prtd->period_cur;
	prtd->period_cur = (prtd->period_cur) % num_period;
	spin_unlock(&prtd->lock);

	snd_pcm_period_elapsed(substream);

	if (STATUS_RUNNING != prtd->status) {
		logd("End, dma stopped\n");
		return IRQ_HDD_STATUS;
	}

	if (SNDRV_PCM_STREAM_PLAYBACK == substream->stream)
		avail = (snd_pcm_uframes_t)snd_pcm_playback_hw_avail(substream->runtime);
	else
		avail = (snd_pcm_uframes_t)snd_pcm_capture_hw_avail(substream->runtime);
	if (avail < rt_period_size) {
		logd("End, avail(%d)< rt_period_size(%d)\n", (int)avail, rt_period_size);
		return IRQ_HDD_SIZE;
	} else {
		ret = hi3630_notify_pcm_set_buf(substream);
		if(0 >  ret) {
			loge("End, hi3630_notify_pcm_set_buf(ret=%d)\n", ret);
			return IRQ_HDD_ERROR;
		}

		spin_lock(&prtd->lock);
		prtd->period_next = (prtd->period_next + 1) % num_period;
		spin_unlock(&prtd->lock);
	}

	return IRQ_HDD;
}

static irq_rt_t hi3630_mb_intr_handle(struct snd_pcm_substream *substream,
					unsigned short pcm_mode)
{
	irq_rt_t ret = IRQ_NH;
	int sema = 0;

	switch(pcm_mode) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		if (NULL !=substream) {
			/* SEM used to protect close while doing _intr_handle_pb */
			sema = down_interruptible(&g_pcm_pb_open_sem);
			if (0 == pcm_pb_status_open) {
				logd("pcm playback closed\n");
				up(&g_pcm_pb_open_sem);
				return IRQ_HDD;
			}

			ret = hi3630_intr_handle(substream);
			up(&g_pcm_pb_open_sem);
		} else {
			loge("PB,substream is NULL\n");
			ret = IRQ_HDD_PTRS;
		}
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
		if (NULL != substream) {
			/* SEM used to protect close while doing _intr_handle_cp */
			sema = down_interruptible(&g_pcm_cp_open_sem);
			if (0 == pcm_cp_status_open) {
				logd("pcm capture closed\n");
				up(&g_pcm_cp_open_sem);
				return IRQ_HDD;
			}
			ret = hi3630_intr_handle(substream);
			up(&g_pcm_cp_open_sem);
		} else {
			loge("CP,substream is NULL\n");
			ret = IRQ_HDD_PTRS;
		}
		break;
	default:
		ret = IRQ_NH_MODE;
		loge("PCM Mode error(%d)\n", pcm_mode);
		break;
	}

	return ret;
}

static int hi3630_notify_pcm_open(unsigned short pcm_mode)
{
	struct hifi_chn_pcm_open msg_body = {0};
	int ret = OK;

	if (!check_pcm_mode(pcm_mode))
		return -EINVAL;

	msg_body.msg_type = (unsigned short)HI_CHN_MSG_PCM_OPEN;
	msg_body.pcm_mode = pcm_mode;

	/* mail-box send */
	ret = hi3630_mailbox_send_data(&msg_body, sizeof(struct hifi_chn_pcm_open), 0);
	if (OK != ret) {
		ret = -EBUSY; 
		loge("mailbox ret=%d\r\n", ret);
	}

	return ret;
}

static int hi3630_notify_pcm_close(unsigned short pcm_mode)
{
	struct hifi_chn_pcm_close msg_body = {0};
	int ret = OK;

	if (!check_pcm_mode(pcm_mode))
		return -EINVAL;

	msg_body.msg_type = (unsigned short)HI_CHN_MSG_PCM_CLOSE;
	msg_body.pcm_mode = pcm_mode;

	/* mail-box send */
	ret = hi3630_mailbox_send_data(&msg_body, sizeof(struct hifi_chn_pcm_close), 0);
	if (OK != ret)
		ret = -EBUSY;

	logd("mailbox ret=%d\r\n", ret);

	return ret;
}

static int hi3630_notify_pcm_hw_params(unsigned short pcm_mode,
					struct snd_pcm_hw_params *params)
{
	struct hifi_chn_pcm_hw_params msg_body = {0};
	unsigned int params_value = 0;
	unsigned int infreq_index = 0;
	int ret	= OK;

	if (!check_pcm_mode(pcm_mode))
		return -EINVAL;

	msg_body.msg_type = (unsigned short)HI_CHN_MSG_PCM_HW_PARAMS;
	msg_body.pcm_mode = pcm_mode;

	/* CHECK SUPPORT CHANNELS : mono or stereo */
	params_value = params_channels(params);
	if ((2 == params_value) || (1 == params_value)) {
		msg_body.channel_num = params_value;
	} else {
		loge("DAC not support %d channels\n", params_value);
		return -EINVAL;
	}

	/* CHECK SUPPORT RATE */
	params_value = params_rate(params);
	logd("set rate = %d \n", params_value);
	for (infreq_index = 0; infreq_index < ARRAY_SIZE(freq); infreq_index++) {
		if (params_value == freq[infreq_index])
			break;
	}
	if (ARRAY_SIZE(freq) <= infreq_index) {
		loge("set rate = %d \n", params_value);
		return -EINVAL;
	}
	msg_body.sample_rate = params_value;

	if (SNDRV_PCM_STREAM_PLAYBACK == pcm_mode) {
		/* PLAYBACK */
		params_value = (unsigned int)params_format(params);
		/* check formats */
		if ((SNDRV_PCM_FORMAT_S16_BE != params_value)
				&& (SNDRV_PCM_FORMAT_S16_LE != params_value)) {
			loge("format err : %d, not support\n", params_value);
			return -EINVAL;
		}
		msg_body.format = params_value;
	} else {
		/* CAPTURE */
		params_value = (unsigned int)params_format(params);
		/* check formats */
		if (SNDRV_PCM_FORMAT_LAST < params_value) {
			loge("format err : %d, not support\n", params_value);
			return -EINVAL;
		}
		msg_body.format = params_value;
	}

	logd("%s:\r\n", __FUNCTION__);
	logd("\tmsg_type = 0x%x\r\n", msg_body.msg_type);
	logd("\tpcm_mode = %d\r\n", msg_body.pcm_mode);
	logd("\tchannel_num = %d\r\n", msg_body.channel_num);
	logd("\tsample_rate = %d\r\n", msg_body.sample_rate);
	logd("\tformat = %d\r\n", msg_body.format);

	/* mail-box send */
	ret = hi3630_mailbox_send_data(&msg_body, sizeof(struct hifi_chn_pcm_hw_params), 0);
	if(OK != ret)
		ret = -EBUSY;

	return ret;
}

static int hi3630_notify_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct hi3630_runtime_data *prtd =
			(struct hi3630_runtime_data *)substream->runtime->private_data;
	unsigned int period_size = 0;
	struct hifi_chn_pcm_trigger msg_body = {0};
	int ret = OK;

	BUG_ON(NULL == prtd);

	period_size = prtd->period_size;

	msg_body.msg_type	= (unsigned short)HI_CHN_MSG_PCM_TRIGGER;
	msg_body.pcm_mode	= (unsigned short)substream->stream;
	msg_body.tg_cmd		= (unsigned short)cmd;
	msg_body.enPcmObj	= (unsigned short)0/*AP*/;
	msg_body.substream	= substream;

	if ((SNDRV_PCM_TRIGGER_START == cmd) || (SNDRV_PCM_TRIGGER_RESUME == cmd)
			|| (SNDRV_PCM_TRIGGER_PAUSE_RELEASE == cmd)) {
		msg_body.data_addr = substream->runtime->dma_addr + prtd->period_next * period_size;
		msg_body.data_len = period_size;
	}

	logd("%s:\r\n", __FUNCTION__);
	logd("\tmsg_type = 0x%x\r\n", msg_body.msg_type);
	logd("\tpcm_mode = %d\r\n", msg_body.pcm_mode);
	logd("\ttg_cmd = %d\r\n", msg_body.tg_cmd);
	logd("\tenPcmObj = %d\r\n", msg_body.enPcmObj);
	logd("\tdata_addr = 0x%x\r\n", msg_body.data_addr);
	logd("\tdata_len = %d\r\n", msg_body.data_len);
	logd("\tsubstream = 0x%p\r\n", msg_body.substream);

	/* mail-box send */
	ret = hi3630_mailbox_send_data(&msg_body, sizeof(struct hifi_chn_pcm_trigger), 0);
	if(OK != ret)
		ret = -EBUSY;

	return ret;
}

static irq_rt_t hi3630_notify_recv_isr(void *usr_para, void *mail_handle, unsigned int mail_len)
{
	struct hifi_chn_pcm_period_elapsed mail_buf;
	unsigned long mail_size = mail_len;
	unsigned long ret_mail = 0;

#ifdef __DRV_AUDIO_MAILBOX_WORK__
	struct snd_pcm_substream * substream = NULL;
	struct hi3630_runtime_data *prtd = NULL;
	struct hi3630_pcm_mailbox_data *hi3630_pcm_mailbox = NULL;
#else
	irq_rt_t ret = IRQ_NH;
#endif

	memset(&mail_buf, 0, sizeof(struct hifi_chn_pcm_period_elapsed));
	/* get data */
#ifdef CONFIG_SND_TEST_AUDIO_PCM_LOOP
	memcpy(&mail_buf, mail_handle, sizeof(struct hifi_chn_pcm_period_elapsed));
#else
	ret_mail = mailbox_read_msg_data(mail_handle, (unsigned char*)&mail_buf, &mail_size);

	if ((ret_mail != MAILBOX_OK) || (mail_size <= 0)
			|| (mail_size > sizeof(struct hifi_chn_pcm_period_elapsed))) {
		loge("Empty point or data length error! size: %ld\n", mail_size);
		return IRQ_NH_MB;
	}
#endif

#ifdef __DRV_AUDIO_MAILBOX_WORK__
	substream = mail_buf.substream;
	if (NULL == substream) {
		loge("substream from hifi is NULL\n");
		return IRQ_NH_OTHERS;
	}

	if (NULL == substream->runtime) {
		loge("substream->runtime is NULL\n");
		return IRQ_NH_OTHERS;
	}

	prtd = (struct hi3630_runtime_data *)substream->runtime->private_data;
	if (NULL == prtd) {
		loge("prtd is NULL\n");
		return IRQ_NH_OTHERS;
	}

	if (STATUS_STOP == prtd->status) {
		logd("process has stopped there is still info coming from hifi\n");
		return IRQ_NH_OTHERS;
	}

	logd("Begin msg_type=0x%x, substream=0x%x\n", mail_buf.msg_type, mail_buf.substream);
	hi3630_pcm_mailbox = &prtd->hi3630_pcm_mailbox;
	hi3630_pcm_mailbox->msg_type = mail_buf.msg_type;
	hi3630_pcm_mailbox->pcm_mode = mail_buf.pcm_mode;
	hi3630_pcm_mailbox->substream = mail_buf.substream;

	queue_delayed_work(hi3630_pcm_mailbox->pcm_mailbox_delay_wq,
			&hi3630_pcm_mailbox->pcm_mailbox_delay_work,
			msecs_to_jiffies(0));

	logd("End\n");

	return IRQ_HDD;
#else
	switch(mail_buf.msg_type) {
	case HI_CHN_MSG_PCM_PERIOD_ELAPSED:
		ret = hi3630_mb_intr_handle(mail_buf.substream, mail_buf.pcm_mode);
		if (IRQ_NH == ret)
			loge("ret : %d\n", ret);
		break;
	default:
		ret = IRQ_NH_TYPE;
		loge("msg_type 0x%x\n", mail_buf.msg_type);
		break;
	}

	return ret;
#endif
}

static int hi3630_notify_isr_register(irq_hdl_t pisr)
{
#ifdef CONFIG_SND_TEST_AUDIO_PCM_LOOP
	return OK;
#else
	int ret = OK;

	if(NULL == pisr) {
		loge("pisr==NULL!\n");
		ret = ERROR;
	} else {
		ret = mailbox_reg_msg_cb(MAILBOX_MAILCODE_HIFI_TO_ACPU_AUDIO, (void *)pisr, NULL);
		if (OK != ret)
			loge("ret : %d\n", ret);
	}

	return ret;
#endif
}

static int hi3630_pcm_hifi_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	unsigned long bytes = params_buffer_bytes(params);
	struct hi3630_runtime_data *prtd =
			(struct hi3630_runtime_data *)substream->runtime->private_data;
	int ret = 0;

	IN_FUNCTION;

	logd("entry : %s\n", substream->stream == SNDRV_PCM_STREAM_PLAYBACK
			? "PLAYBACK" : "CAPTURE");

	ret = snd_pcm_lib_malloc_pages(substream, bytes);
	if (0 > ret) {
		loge("snd_pcm_lib_malloc_pages ret : %d\n", ret);

		OUT_FUNCTION;
		return ret;
	}

	prtd->period_size = params_period_bytes(params);
	prtd->period_next = 0;

	ret = hi3630_notify_pcm_hw_params((unsigned short)substream->stream, params);
	if (0 > ret) {
		loge("hi3630_notify_pcm_hw_params ret : %d\n", ret);
		snd_pcm_lib_free_pages(substream);
	}

	OUT_FUNCTION;

	return ret;
}

static int hi3630_pcm_hifi_hw_free(struct snd_pcm_substream *substream)
{
	logd("entry : %s\n", substream->stream == SNDRV_PCM_STREAM_PLAYBACK
		? "PLAYBACK" : "CAPTURE");

	return snd_pcm_lib_free_pages(substream);
}

static int hi3630_pcm_hifi_prepare(struct snd_pcm_substream *substream)
{
	struct hi3630_runtime_data *prtd =
			(struct hi3630_runtime_data *)substream->runtime->private_data;

	logd("entry : %s\n", substream->stream == SNDRV_PCM_STREAM_PLAYBACK
			? "PLAYBACK" : "CAPTURE");

	BUG_ON(NULL == prtd);

	/* init prtd */
	prtd->status = STATUS_STOP;
	prtd->period_next = 0;
	prtd->period_cur = 0;

	return OK;
}

static int hi3630_pcm_hifi_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hi3630_runtime_data *prtd =
			(struct hi3630_runtime_data *)substream->runtime->private_data;
	unsigned int num_periods = runtime->periods;
	int ret = OK;

	IN_FUNCTION;

	logd("entry : %s, cmd : %d\n", substream->stream == SNDRV_PCM_STREAM_PLAYBACK
			? "PLAYBACK" : "CAPTURE", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ret = hi3630_notify_pcm_trigger(substream, cmd);
		if (0 > ret) {
			loge("hi3630_notify_pcm_trigger ret : %d\n", ret);
		} else {
			spin_lock(&prtd->lock);
			prtd->status = STATUS_RUNNING;
			prtd->period_next = (prtd->period_next + 1) % num_periods;
			spin_unlock(&prtd->lock);
		}
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		spin_lock(&prtd->lock);
		prtd->status = STATUS_STOPPING;
		spin_unlock(&prtd->lock);

		ret = hi3630_notify_pcm_trigger(substream, cmd);
		if (0 > ret)
			loge("hi3630_notify_pcm_trigger ret : %d\n", ret);
		break;

	default:
		loge("cmd error : %d", cmd);
		ret = -EINVAL;
		break;
	}

	OUT_FUNCTION;

	return ret;
}

static snd_pcm_uframes_t hi3630_pcm_hifi_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hi3630_runtime_data *prtd =
			(struct hi3630_runtime_data *)substream->runtime->private_data;
	snd_pcm_uframes_t frame = 0L;

	frame = bytes_to_frames(runtime, prtd->period_cur * prtd->period_size);
	if (frame >= runtime->buffer_size)
		frame = 0;

	return frame;
}

static int hi3630_pcm_hifi_open(struct snd_pcm_substream *substream)
{
	struct hi3630_runtime_data *prtd = NULL;
	int ret = OK;

	IN_FUNCTION;

	logd("entry : %s\n", substream->stream == SNDRV_PCM_STREAM_PLAYBACK
		? "PLAYBACK" : "CAPTURE");

	prtd = kzalloc(sizeof(struct hi3630_runtime_data), GFP_KERNEL);
	if (NULL == prtd) {
		loge("kzalloc faile,prtd==NULL\n");
		OUT_FUNCTION;
		return -ENOMEM;
	}

	spin_lock_init(&prtd->lock);

	substream->runtime->private_data = prtd;

	snd_soc_set_runtime_hwparams(substream, &hi3630_asp_dmac_hardware);

#ifdef __DRV_AUDIO_MAILBOX_WORK__
	prtd->hi3630_pcm_mailbox.pcm_mailbox_delay_wq = hi3630_pcm_mailbox_workqueue.pcm_mailbox_delay_wq;
	INIT_DELAYED_WORK(&prtd->hi3630_pcm_mailbox.pcm_mailbox_delay_work, pcm_mailbox_work_func);
#endif

	ret = hi3630_notify_pcm_open( (unsigned short)substream->stream );
	if (0 > ret) {
		loge("hi3630_notify_pcm_open ret : %d\n", ret);
		OUT_FUNCTION;
		return ret;
	}

	OUT_FUNCTION;

	return ret;
}

static int hi3630_pcm_hifi_close(struct snd_pcm_substream *substream)
{
	struct hi3630_runtime_data *prtd =
			(struct hi3630_runtime_data *)substream->runtime->private_data;

	IN_FUNCTION;

	logd("entry : %s\n", substream->stream == SNDRV_PCM_STREAM_PLAYBACK
			? "PLAYBACK" : "CAPTURE");

	if(NULL == prtd)
		loge("prtd==NULL\n");

	hi3630_notify_pcm_close((unsigned short)substream->stream);

#ifdef CONFIG_SND_TEST_AUDIO_PCM_LOOP
	if(hi3630_simu_pcm.simu_pcm_delay_wq) {
		cancel_delayed_work(&hi3630_simu_pcm.simu_pcm_delay_work);
		flush_workqueue(hi3630_simu_pcm.simu_pcm_delay_wq);
	}
#endif

	if (prtd) {
		kfree(prtd);
		substream->runtime->private_data = NULL;
	}

	OUT_FUNCTION;

	return OK;
}

static int hi3630_asp_dmac_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	return hi3630_pcm_hifi_hw_params(substream, params);
}

static int hi3630_asp_dmac_hw_free(struct snd_pcm_substream *substream)
{
	struct hi3630_runtime_data *prtd = NULL;
	int i = 0;
	int ret = 0;

	prtd = (struct hi3630_runtime_data *)substream->runtime->private_data;

	for(i = 0; i < 30 ; i++) {  /* wait for dma ok */
		if (STATUS_STOP == prtd->status) {
			break;
		} else {
			msleep(10);
		}
	}
	if (30 == i){
		loge("timeout for waiting for stop info from hifi \n");
	}
#ifdef __DRV_AUDIO_MAILBOX_WORK__
	flush_workqueue(hi3630_pcm_mailbox_workqueue.pcm_mailbox_delay_wq);
#endif
	ret = hi3630_pcm_hifi_hw_free(substream);

	return ret;
}

static int hi3630_asp_dmac_prepare(struct snd_pcm_substream *substream)
{
	return hi3630_pcm_hifi_prepare(substream);
}

static int hi3630_asp_dmac_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return hi3630_pcm_hifi_trigger(substream, cmd);
}

static snd_pcm_uframes_t hi3630_asp_dmac_pointer(struct snd_pcm_substream *substream)
{
	return hi3630_pcm_hifi_pointer(substream);
}

static int hi3630_asp_dmac_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (NULL != runtime)
		ret = dma_mmap_writecombine(substream->pcm->card->dev,
				vma,
				runtime->dma_area,
				runtime->dma_addr,
				runtime->dma_bytes);

	return ret;
}


static int hi3630_asp_dmac_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	int sema = 0;

	if (SNDRV_PCM_STREAM_PLAYBACK == substream->stream) {
		sema = down_interruptible(&g_pcm_pb_open_sem);
		pcm_pb_status_open = (u32)1;
		up(&g_pcm_pb_open_sem);
	} else {
		sema = down_interruptible(&g_pcm_cp_open_sem);
		pcm_cp_status_open = (u32)1;
		up(&g_pcm_cp_open_sem);
	}
	ret = hi3630_pcm_hifi_open(substream);

	return ret;
}

static int hi3630_asp_dmac_close(struct snd_pcm_substream *substream)
{
	int ret = 0;
	int sema = 0;

	if (SNDRV_PCM_STREAM_PLAYBACK == substream->stream) {
		sema = down_interruptible(&g_pcm_pb_open_sem);
		pcm_pb_status_open = (u32)0;
		ret = hi3630_pcm_hifi_close(substream);
		up(&g_pcm_pb_open_sem);
	} else {
		sema = down_interruptible(&g_pcm_cp_open_sem);
		pcm_cp_status_open = (u32)0;
		ret = hi3630_pcm_hifi_close(substream);
		up(&g_pcm_cp_open_sem);
	}

	return ret;
}

static struct snd_pcm_ops hi3630_asp_dmac_ops = {
	.open	    = hi3630_asp_dmac_open,
	.close	    = hi3630_asp_dmac_close,
	.ioctl      = snd_pcm_lib_ioctl,
	.hw_params  = hi3630_asp_dmac_hw_params,
	.hw_free    = hi3630_asp_dmac_hw_free,
	.prepare    = hi3630_asp_dmac_prepare,
	.trigger    = hi3630_asp_dmac_trigger,
	.pointer    = hi3630_asp_dmac_pointer,
	.mmap       = hi3630_asp_dmac_mmap,
};

static unsigned long long hi3630_pcm_dmamask = DMA_BIT_MASK(32);

static int hi3630_asp_dmac_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm  *pcm  = rtd->pcm;
	struct hi3630_asp_dmac_data *pdata =
		(struct hi3630_asp_dmac_data *)snd_soc_platform_get_drvdata(rtd->platform);
	int ret = 0;

	BUG_ON(NULL == card);
	BUG_ON(NULL == pcm);
	BUG_ON(NULL == pdata);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &hi3630_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = hi3630_pcm_preallocate_dma_buffer(pcm,
				SNDRV_PCM_STREAM_CAPTURE);
		if (ret) {
			loge("preallocate dma for dmac error(%d)\n", ret);
			hi3630_pcm_free_dma_buffers(pcm);
			return ret;
		}
	}

	ret = snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
			pcm->card->dev, HI3630_MAX_BUFFER_SIZE, HI3630_MAX_BUFFER_SIZE);
	if (ret) {
		loge("snd_pcm_lib_preallocate_pages_for_all error : %d\n", ret);
		return ret;
	}

	/* register ipc recv function */
	ret = hi3630_notify_isr_register((void *)hi3630_notify_recv_isr);
	if (ret) {
		loge("notify Isr register error : %d\n", ret);
		snd_pcm_lib_preallocate_free_for_all(pcm);
	}

	return ret;
}

static void hi3630_asp_dmac_free(struct snd_pcm *pcm)
{
	BUG_ON(NULL == pcm);

	hi3630_pcm_free_dma_buffers(pcm);
}

static struct snd_soc_platform_driver hi3630_pcm_asp_dmac_platform = {
	.ops		= &hi3630_asp_dmac_ops,
	.pcm_new	= hi3630_asp_dmac_new,
	.pcm_free	= hi3630_asp_dmac_free,
};

static int hi3630_asp_dmac_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct device *dev = &pdev->dev;
	struct hi3630_asp_dmac_data *pdata = NULL;

	if (!dev) {
		loge("platform_device has no device\n");
		return -ENOENT;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "cannot allocate hi3630 asp dma platform data\n");
		return -ENOMEM;
	}

	/* get resources */
	pdata->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!pdata->res) {
		dev_err(dev, "platform_get_resource err\n");
		return -ENOENT;
	}

	if (!devm_request_mem_region(dev, pdata->res->start,
				     resource_size(pdata->res),
				     pdev->name)) {
		dev_err(dev, "cannot claim register memory\n");
		return -ENOMEM;
	}

	pdata->reg_base_addr = devm_ioremap(dev, pdata->res->start,
					    resource_size(pdata->res));
	if (!pdata->reg_base_addr) {
		dev_err(dev, "cannot map register memory\n");
		return -ENOMEM;
	}

	pdata->irq = platform_get_irq_byname(pdev, "asp_dma_irq");
	if (0 > pdata->irq) {
		dev_err(dev, "cannot get irq\n");
		return -ENOENT;
	}

	spin_lock_init(&pdata->lock);

	pdata->dev = dev;

	platform_set_drvdata(pdev, pdata);

	dev_set_name(dev, "hi3630-pcm-asp-dma");

#ifdef CONFIG_SND_TEST_AUDIO_PCM_LOOP
	hi3630_simu_pcm.simu_pcm_delay_wq = create_singlethread_workqueue("simu_pcm_delay_wq");
	if (!(hi3630_simu_pcm.simu_pcm_delay_wq)) {
		pr_err("%s(%u) : workqueue create failed", __FUNCTION__,__LINE__);
		return -ENOMEM;
	}
	INIT_DELAYED_WORK(&hi3630_simu_pcm.simu_pcm_delay_work, simu_pcm_work_func);
#endif
#ifdef __DRV_AUDIO_MAILBOX_WORK__
	hi3630_pcm_mailbox_workqueue.pcm_mailbox_delay_wq = create_singlethread_workqueue("pcm_mailbox_delay_wq");
	if (!(hi3630_pcm_mailbox_workqueue.pcm_mailbox_delay_wq)) {
		pr_err("%s(%u) : workqueue create failed", __FUNCTION__,__LINE__);
		return -ENOMEM;
	}
	/*  put INIT_DELAYED_WORK to open() function */
#endif

	ret = snd_soc_register_platform(dev, &hi3630_pcm_asp_dmac_platform);
	if (ret) {
		loge("snd_soc_register_platform return %d\n" ,ret);
		return -ENODEV;
	}

	return 0;
}

static int hi3630_asp_dmac_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

static struct platform_driver hi3630_asp_dmac_driver = {
	.driver = {
		.name	= "hi3630-pcm-asp-dma",
		.owner	= THIS_MODULE,
		.of_match_table = hi3630_asp_dmac_of_match,
	},
	.probe	= hi3630_asp_dmac_probe,
	.remove	= hi3630_asp_dmac_remove,
};
module_platform_driver(hi3630_asp_dmac_driver);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

MODULE_AUTHOR("chengong <apollo.chengong@huawei.com>");
MODULE_DESCRIPTION("Hi3630 ASP DMA Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:hi3630_asp_dmac");
