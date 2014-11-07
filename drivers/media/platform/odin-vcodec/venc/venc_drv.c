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

#include <asm-generic/uaccess.h>
#include <asm-generic/ioctl.h>

#include <linux/err.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/pm_qos.h>

#include <media/odin/vcodec/vcore/encoder.h>
#include <media/odin/vcodec/vlog.h>

#include "../misc/vevent/vevent.h"
#include "../common/vcodec_dpb.h"

#include "venc_drv.h"
#include "vec/vec_drv.h"

#define	VENC_MOVIE_EPB_SIZE		(8*1024*1024)
#define	VENC_STILL_EPB_SIZE		(6*1024*1024)

struct venc_buffer
{
	unsigned int paddr;
	unsigned long *vaddr;
	unsigned int size;
};

struct venc_channel
{
	void *vec_id;
	void *dpb_id;

	int vcore_fb;
	unsigned int received_fb;

	struct vcore_enc_dpb dpb;

	struct venc_buffer epb;
	struct venc_buffer dpb_buf[VENC_MAX_NUM_OF_DPB];
	struct venc_buffer scratch_buf;
	struct venc_buffer work_buf;

	struct vcore_enc_config config;

	void *cb_arg;
	void (*callback)(void *cb_arg, struct venc_cb_data *cb_data);
	void *vcore_select_id;

	struct mutex lock;

	struct pm_qos_request mem_qos;
};

static void* (*_venc_get_vcore)(enum vcore_enc_codec codec_type,
								struct vcore_enc_ops *ops,
								unsigned int width,
								unsigned int height,
								unsigned int fr_residual,
								unsigned int fr_divider) = NULL;
static void (*_venc_put_vcore)(void *vcodec_vcore_id) = NULL;

static void _venc_cb_encoded(void *_ch, struct vec_report_enc *report_enc)
{
	struct venc_channel *ch = (struct venc_channel*)_ch;
	struct venc_cb_data cb_encoded;
	unsigned int dpb_i;
	unsigned int dpb_i_err;

	memset(&cb_encoded, 0, sizeof(cb_encoded));

	switch (report_enc->hdr)
	{
	case VEC_REPORT_SEQ :
		ch->dpb.buf_width = report_enc->u.seq.buf_width;
		ch->dpb.buf_height = report_enc->u.seq.buf_height;
		ch->dpb_buf[0].size = report_enc->u.seq.ref_frame_size;
		for (dpb_i=0; dpb_i<report_enc->u.seq.ref_frame_cnt; dpb_i++) {
			if (vcodec_dpb_add(ch->dpb_id,
								-1,
								&ch->dpb_buf[dpb_i].paddr,
								NULL,
								ch->dpb_buf[0].size) == false) {
				vlog_error("dpb buffer alloc failed\n");
				goto err_dpb_alloc;
			}

			ch->dpb.dpb_addr[dpb_i] = ch->dpb_buf[dpb_i].paddr;
		}
		ch->dpb.num = report_enc->u.seq.ref_frame_cnt;

		if (report_enc->u.seq.scratch_buf_size) {
			ch->scratch_buf.size = report_enc->u.seq.scratch_buf_size;
			if (vcodec_dpb_add(ch->dpb_id,
								-1,
								&ch->scratch_buf.paddr,
								NULL,
								ch->scratch_buf.size) == false) {
				vlog_error("scratch buffer alloc failed\n");
				goto err_dpb_alloc;
			}

			ch->dpb.scratch_buf.addr = ch->scratch_buf.paddr;
			ch->dpb.scratch_buf.size = ch->scratch_buf.size;
		}

		vec_register_decompressed_buffer(ch->vec_id, &ch->dpb);
		break;
	case VEC_REPORT_USED_FB_EP :
	case VEC_REPORT_FLUSH_DONE :
		if (report_enc->hdr == VEC_REPORT_USED_FB_EP) {
			cb_encoded.type = VENC_CB_TYPE_USED_FB_EP;
		}
		else if (report_enc->hdr == VEC_REPORT_FLUSH_DONE) {
			cb_encoded.type = VENC_CB_TYPE_FLUSH_DONE;
		}
		else {
			vlog_error("invalid report_enc->hdr: %d\n", report_enc->hdr);
			cb_encoded.type = VENC_CB_TYPE_INVALID;
		}

		if (report_enc->u.used_fb_ep.pic_phy_addr) {
			cb_encoded.fb.ion_fd = vcodec_dpb_fd_get(ch->dpb_id,
					report_enc->u.used_fb_ep.pic_phy_addr);
			if (cb_encoded.fb.ion_fd <= 0) {
				vlog_error("can`t get fb ion fd. paddr:0x%x\n",
						report_enc->u.used_fb_ep.pic_phy_addr);
				return;
			}
			cb_encoded.fb.size = report_enc->u.used_fb_ep.pic_size;
			cb_encoded.fb.timestamp = report_enc->u.used_fb_ep.timestamp;
			cb_encoded.fb.uid = report_enc->u.used_fb_ep.uid;
			cb_encoded.fb.input_time = report_enc->u.used_fb_ep.input_time;
			cb_encoded.fb.feed_time = report_enc->u.used_fb_ep.feed_time;

			ch->vcore_fb--;
		}
		else {
			cb_encoded.fb.ion_fd = -1;
			cb_encoded.fb.size = 0;
			cb_encoded.fb.timestamp = 0xFFFFFFFFFFFFFFFFLL;
			cb_encoded.fb.uid = 0xFFFFFFFF;
			cb_encoded.fb.input_time = jiffies;
			cb_encoded.fb.feed_time = cb_encoded.fb.input_time;
		}

		cb_encoded.fb.eos = report_enc->u.used_fb_ep.eos;
		cb_encoded.fb.private_in_meta = report_enc->u.used_fb_ep.private_in_meta;
		cb_encoded.fb.num_of_pic = report_enc->u.used_fb_ep.num_of_pic;
		cb_encoded.es.last_es_size = report_enc->u.used_fb_ep.last_es_size;
		cb_encoded.es.num_of_es = report_enc->u.used_fb_ep.num_of_es;
		cb_encoded.es.epb_occupancy = report_enc->u.used_fb_ep.epb_occupancy;
		cb_encoded.es.epb_size = report_enc->u.used_fb_ep.epb_size;

		if (ch->callback)
			ch->callback(ch->cb_arg, &cb_encoded);
		else
			vlog_error( "no callback path for encoder\n" );
		break;
	case VEC_REQUEST_RESET :
		vlog_warning("reset request\n");
		break;
	default :
		vlog_error("unknown encoding hdr(%d)\n", report_enc->hdr);
		break;
	}
	return;

err_dpb_alloc:
	for(dpb_i_err=0; dpb_i_err < dpb_i; dpb_i_err++)
		vcodec_dpb_del(ch->dpb_id, -1, ch->dpb_buf[dpb_i].paddr);
	memset(ch->dpb_buf, 0 , sizeof(struct venc_buffer)*VENC_MAX_NUM_OF_DPB);

	ch->dpb.num = 0;
}

void* venc_open(struct vcore_enc_config *enc_config,
				void *cb_arg,
				void (*callback)(void *cb_arg, struct venc_cb_data *cb_data))
{
	int errno = 0;
	struct vcore_enc_ops enc_ops;
	struct venc_channel *ch = NULL;

	vlog_trace("codec: %d, resolution: %u * %u, fps: %u / %u\n",
				enc_config->codec_type,
				enc_config->buf_width, enc_config->buf_height,
				enc_config->frame_rate.residual, enc_config->frame_rate.divider);

	if (_venc_get_vcore == NULL) {
		vlog_error("_venc_get_vcore is NULL\n");
		return ERR_PTR(-EINVAL);
	}

	ch = (struct venc_channel*)vzalloc(sizeof(struct venc_channel));
	if (ch == NULL) {
		vlog_error("vzalloc failed\n");
		return ERR_PTR(-ENOMEM);
	}

	pm_qos_add_request(&ch->mem_qos, PM_QOS_ODIN_MEM_MIN_FREQ,
				PM_QOS_ODIN_MEM_MIN_FREQ_DEFAULT_VALUE);

	mutex_init(&ch->lock);

	mutex_lock(&ch->lock);
	ch->vcore_select_id = _venc_get_vcore(enc_config->codec_type,
										&enc_ops,
										enc_config->buf_width,
										enc_config->buf_height,
										enc_config->frame_rate.residual,
										enc_config->frame_rate.divider);
	if (ch->vcore_select_id == NULL) {
		vlog_error( "no available vcore\n" );
		errno = -EINVAL;
		goto err_venc_get_vcore;
	}

	ch->dpb_id = vcodec_dpb_open();
	if (IS_ERR_OR_NULL(ch->dpb_id)) {
		vlog_error("vcodec_dpb_open failed\n");
		errno = -ENOMEM;
		goto err_dpb_list_open;
	}

	if (enc_config->codec_type == VCORE_ENC_JPEG)
		ch->epb.size = VENC_STILL_EPB_SIZE;
	else
		ch->epb.size = VENC_MOVIE_EPB_SIZE;

	if (vcodec_dpb_add(ch->dpb_id, -1, &ch->epb.paddr, &ch->epb.vaddr,
					ch->epb.size) == false) {
		vlog_error("epb alloc failed\n");
		errno = -ENOBUFS;
		goto err_epb_buffer_alloc;
	}

	ch->work_buf.size = 80*1024;
	if (vcodec_dpb_add(ch->dpb_id, -1, &ch->work_buf.paddr, NULL,
					ch->work_buf.size) == false) {
		vlog_error("work buffer alloc failed\n");
		errno = -ENOBUFS;
		goto err_work_buffer_alloc;
	}

	enc_config->epb_phy_addr = ch->epb.paddr;
	enc_config->epb_vir_ptr = (unsigned char*)ch->epb.vaddr;
	enc_config->epb_size = ch->epb.size;

	ch->vec_id = vec_open(&enc_ops, enc_config, ch->work_buf.paddr,
					ch->work_buf.vaddr, ch->work_buf.size,
					(void*)ch, _venc_cb_encoded);
	if (IS_ERR_OR_NULL(ch->vec_id)) {
		vlog_error("vec_open faild\n");
		errno = -EINVAL;
		goto err_vec_open;
	}

	ch->cb_arg = cb_arg;
	ch->callback = callback;

	ch->vcore_fb = 0;
	ch->received_fb = 0;
	memcpy(&ch->config, enc_config, sizeof(struct vcore_enc_config));

	vlog_trace("success - venc:0x%X, vec:0x%X, dpb:0x%X\n",
						(unsigned int)ch,
						(unsigned int)ch->vec_id, (unsigned int)ch->dpb_id);
	mutex_unlock(&ch->lock);

	return (void*)ch;

err_vec_open:
	vcodec_dpb_del(ch->dpb_id, -1, ch->work_buf.paddr);
	memset(&ch->work_buf, 0 , sizeof(struct venc_buffer));

err_work_buffer_alloc:
	vcodec_dpb_del(ch->dpb_id, -1, ch->epb.paddr);
	memset(&ch->epb, 0 , sizeof(struct venc_buffer));

err_epb_buffer_alloc:
	vcodec_dpb_close(ch->dpb_id);
	ch->dpb_id = NULL;

err_dpb_list_open:
	if (_venc_put_vcore && ch->vcore_select_id) {
		_venc_put_vcore(ch->vcore_select_id);
		ch->vcore_select_id = NULL;
	}

err_venc_get_vcore:
	mutex_unlock(&ch->lock);
	mutex_destroy(&ch->lock);

	pm_qos_remove_request(&ch->mem_qos);
	vfree(ch);

	return ERR_PTR(errno);
}

void venc_close(void *_ch)
{
	struct venc_channel *ch = (struct venc_channel*)_ch;
	unsigned int dpb_i;

	if (ch == NULL) {
		vlog_error("invalid channel\n");
		return;
	}

	mutex_lock(&ch->lock);

	if (ch->vec_id) {
		vec_close(ch->vec_id);
		ch->vec_id = NULL;
	}

	vcodec_dpb_del(ch->dpb_id, -1, ch->epb.paddr);
	memset(&ch->epb, 0 , sizeof(struct venc_buffer));

	for (dpb_i = 0; dpb_i < ch->dpb.num; dpb_i++)
		vcodec_dpb_del(ch->dpb_id, -1, ch->dpb_buf[dpb_i].paddr);
	memset(ch->dpb_buf, 0 , sizeof(struct venc_buffer)*VENC_MAX_NUM_OF_DPB);

	if (ch->scratch_buf.paddr) {
		vcodec_dpb_del(ch->dpb_id, -1, ch->scratch_buf.paddr);
		memset(&ch->scratch_buf, 0 , sizeof(struct venc_buffer));
	}

	vcodec_dpb_del(ch->dpb_id, -1, ch->work_buf.paddr);
	memset(&ch->work_buf, 0 , sizeof(struct venc_buffer));

	if (ch->dpb_id) {
		vcodec_dpb_close(ch->dpb_id);
		ch->dpb_id = NULL;
	}

	mutex_unlock(&ch->lock);
	mutex_destroy(&ch->lock);

	if (_venc_put_vcore && ch->vcore_select_id) {
		_venc_put_vcore(ch->vcore_select_id);
		ch->vcore_select_id = NULL;
	} else {
		vlog_error("_venc_put_vcore  : 0x%08X, ch->vcore_select_id : 0x%08X\n",
			(unsigned int)_venc_put_vcore, (unsigned int)ch->vcore_select_id);
	}

	pm_qos_remove_request(&ch->mem_qos);

	vfree(ch);

	vlog_trace("success\n");
}

void venc_resume(void *_ch)
{
	struct venc_channel *ch = (struct venc_channel*)_ch;

	if (ch == NULL) {
		vlog_error("invalid channel\n");
		return;
	}

	mutex_lock(&ch->lock);

	vec_resume(ch->vec_id);

	mutex_unlock(&ch->lock);
}

void venc_pause(void *_ch)
{
	struct venc_channel *ch = (struct venc_channel*)_ch;

	if (ch == NULL) {
		vlog_error("invalid channel\n");
		return;
	}

	mutex_lock(&ch->lock);

	vec_pause(ch->vec_id);

	ch->received_fb = 0;

	mutex_unlock(&ch->lock);
}

void venc_flush(void *_ch)
{
	struct venc_channel *ch = (struct venc_channel*)_ch;

	if (ch == NULL) {
		vlog_error("invalid channel\n");
		return;
	}

	mutex_lock(&ch->lock);

	vec_flush(ch->vec_id);

	ch->received_fb = 0;

	mutex_unlock(&ch->lock);
}

int venc_update_fb(void *_ch,
					int ion_fd,
					unsigned int size,
					void *private_in_meta,
					bool eos,
					unsigned long long timestamp,
					unsigned int uid)
{
	struct vec_pic vec_pic;
	struct venc_channel *ch = (struct venc_channel*)_ch;

	if (ch == NULL) {
		vlog_error("invalid channel\n");
		return -EINVAL;
	}

	mutex_lock(&ch->lock);

	if (ion_fd < 0) {
		vec_pic.pic_phy_addr = 0x0;
		vec_pic.pic_vir_ptr = NULL;
		if (size != 0)
			vlog_error("size: %u\n", size);
		vec_pic.pic_size = size;
	}
	else {
		if (vcodec_dpb_addr_get(ch->dpb_id,
						ion_fd,
						&vec_pic.pic_phy_addr,
						(unsigned long **)&vec_pic.pic_vir_ptr) == false) {
			if (vcodec_dpb_add(ch->dpb_id,
							ion_fd,
							&vec_pic.pic_phy_addr,
							(unsigned long **)&vec_pic.pic_vir_ptr, 0) == false) {
				vlog_error("vcodec_dpb_add failed. ion_fd:%d\n", ion_fd);
				goto err_venc_update_fb;
			}
		}
		vec_pic.pic_size = size;
		vec_pic.private_in_meta = private_in_meta;
	}

	vec_pic.eos = eos;
	vec_pic.private_in_meta = private_in_meta;

	vec_pic.timestamp = timestamp;
	vec_pic.uid = uid;
	if (vec_update_pic_buffer(ch->vec_id, &vec_pic) == false) {
		vlog_error("vec_update_pic_buffer failed.pic_phy_addr:0x%X++%d\n",
					vec_pic.pic_phy_addr, vec_pic.pic_size);
		goto err_venc_update_fb;
	}

	if (ch->received_fb == 0) {
		/* request DRAM frequency to be at least 800 MHz for next 100 ms,
	  	  because of DFS measurement period 50ms */
		pm_qos_update_request_timeout(&ch->mem_qos, 800000, 100000);
	}

	ch->vcore_fb++;
	ch->received_fb++;

	mutex_unlock(&ch->lock);
	return 0;

err_venc_update_fb:
	mutex_unlock(&ch->lock);
	return -1;
}

void venc_update_epb(void *_ch, unsigned int num_of_free_esq_node)
{
	struct venc_channel *ch = (struct venc_channel*)_ch;

	if (ch == NULL) {
		vlog_error("invalid channel\n");
		return;
	}

	mutex_lock(&ch->lock);

	vec_request_epb_cb(ch->vec_id, num_of_free_esq_node);

	mutex_unlock(&ch->lock);
}

int venc_get_epb(void *_ch,
				char *buf,
				unsigned int buf_size,
				unsigned int *es_size,
				enum venc_es_type *type,
				unsigned long long *timestamp,
				unsigned int *uid,
				unsigned long *input_time,
				unsigned long *feed_time,
				unsigned long *encode_time)
{
	struct venc_channel *ch = (struct venc_channel*)_ch;
	struct vec_epb epb;

	if (ch == NULL) {
		vlog_error("invalid channel\n");
		return -EINVAL;
	}

	mutex_lock(&ch->lock);

	epb.buf_ptr = buf;
	epb.buf_size = buf_size;

	if (vec_get_encoded_buffer(ch->vec_id, &epb) == false)
	{
		vlog_error("vec_get_encoded_buffer() failed\n");
		mutex_unlock(&ch->lock);
		return -EIO;
	}

	switch (epb.pic_type)
	{
	case VEC_ENC_SEQ_HDR :
		*type = VENC_ES_SEQ_HDR;
		break;
	case VEC_ENC_PIC_I :
	case VEC_ENC_PIC_P :
	case VEC_ENC_PIC_B :
		*type = VENC_ES_PIC_I + (epb.pic_type - VEC_ENC_PIC_I);
		break;
	case VEC_ENC_EOS :
		vlog_trace( "end of stream\n" );
		*type = VENC_ES_EOS;
		break;
	default :
		vlog_error( "pic type:%d\n", epb.pic_type );
		break;
	}

	*es_size = epb.es_size;
	*timestamp = epb.timestamp;
	*uid = epb.uid;
	*input_time = epb.input_time;
	*feed_time = epb.feed_time;
	*encode_time = epb.encode_time;

	mutex_unlock(&ch->lock);

	return 0;
}

int venc_get_buffer_info(void *_ch,
						unsigned int *num_of_pic,
						unsigned int *last_es_size,
						unsigned int *num_of_es,
						unsigned int *epb_occupancy,
						unsigned int *epb_size)
{
	struct venc_channel *ch = (struct venc_channel*)_ch;
	struct vec_esq_info esq_info;

	if (ch == NULL) {
		vlog_error("invalid channel\n");
		return -EINVAL;
	}

	mutex_lock(&ch->lock);

	vec_get_esq_info(ch->vec_id, &esq_info);

	*num_of_pic = vec_get_picq_occupancy(ch->vec_id);
	*last_es_size = esq_info.last_es_size;
	*num_of_es = esq_info.num_of_es;
	*epb_occupancy = esq_info.epb_occupancy;
	*epb_size = esq_info.epb_size;

	mutex_unlock(&ch->lock);

	return 0;
}

int venc_set_config(void *_ch, struct vcore_enc_running_config *config)
{
	struct venc_channel *ch = (struct venc_channel*)_ch;

	if (ch == NULL) {
		vlog_error("invalid channel\n");
		return -EINVAL;
	}

	mutex_lock(&ch->lock);

	vec_set_config(ch->vec_id, config);

	mutex_unlock(&ch->lock);

	return 0;
}

int venc_init(void* (*get_vcore)(enum vcore_enc_codec codec,
								struct vcore_enc_ops *ops,
								unsigned int width,
								unsigned int height,
								unsigned int fr_residual,
								unsigned int fr_divider),
			void (*put_vcore)(void *select_id))
{
	_venc_get_vcore = get_vcore;
	_venc_put_vcore = put_vcore;

	vec_init();

	return 0;
}

int venc_cleanup(void)
{
	vec_cleanup();
	return 0;
}

