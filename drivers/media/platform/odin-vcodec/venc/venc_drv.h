/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _VENC_DRV_H_
#define _VENC_DRV_H_

#include <media/odin/vcodec/vcore/encoder.h>

enum venc_es_type
{
	VENC_ES_SEQ_HDR,
	VENC_ES_PIC_I,
	VENC_ES_PIC_P,
	VENC_ES_PIC_B,
	VENC_ES_EOS,
};

enum venc_cb_type
{
	VENC_CB_TYPE_USED_FB_EP,
	VENC_CB_TYPE_FLUSH_DONE,
	VENC_CB_TYPE_INVALID,
};

struct venc_cb_data
{
	enum venc_cb_type type;

	struct {
		int ion_fd;
		unsigned int size;
		void *private_in_meta;
		bool eos;
		unsigned long long timestamp;
		unsigned int uid;
		unsigned long input_time;	// jiffies
		unsigned long feed_time;	// jiffies
		unsigned int num_of_pic;
	} fb;

	struct {
		unsigned int last_es_size;
		unsigned int num_of_es;
		unsigned int epb_occupancy;
		unsigned int epb_size;
	} es;
};

void* venc_open(struct vcore_enc_config *enc_config, void *cb_arg, void (*callback)(void *cb_arg, struct venc_cb_data *cb_data));
void venc_close(void *ch);

void venc_resume(void *ch);
void venc_pause(void *ch);
void venc_flush(void *ch);

int venc_update_fb(void *_ch,
					int ion_fd,
					unsigned int size,
					void *private_in_meta,
					bool eos,
					unsigned long long timestamp,
					unsigned int uid);
void venc_update_epb(void *_ch, unsigned int num_of_free_esq_node);
int venc_get_epb(void *_ch, char *buf, unsigned int buf_size, unsigned int *es_size, enum venc_es_type *type, unsigned long long *timestamp, unsigned int *uid, unsigned long *intput_time, unsigned long *feed_time, unsigned long *encode_time);
int venc_get_buffer_info(void *_ch, unsigned int *num_of_pic, unsigned int *last_es_size, unsigned int *num_of_es, unsigned int *epb_occupancy, unsigned int *epb_size);
int venc_set_config(void *_ch, struct vcore_enc_running_config *config);

int venc_init(void* (*get_vcore)(enum vcore_enc_codec codec, struct vcore_enc_ops *ops, unsigned int width, unsigned int height, unsigned int fr_residual, unsigned int fr_divider),
			void (*put_vcore)(void *select_id));
int venc_cleanup(void);

#endif /* #ifndef _VEND_DRV_H_ */
