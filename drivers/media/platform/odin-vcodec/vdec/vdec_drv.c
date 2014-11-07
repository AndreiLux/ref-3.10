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

#include <asm/ioctl.h>
#include <asm/uaccess.h>

#include <linux/slab.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/odin_iommu.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/pm_qos.h>

#include <media/odin/vcodec/vcore/decoder.h>
#include <media/odin/vcodec/vlog.h>

#include "../common/vcodec_dpb.h"
#include "../misc/vbuf/vbuf.h"
#include "../misc/vevent/vevent.h"

#include "vdec_drv.h"
#include "vdc/vdc_drv.h"
#include "ves/ves_drv.h"

#define	VDEC_MOVIE_CPB_SIZE		(8*1024*1024)
#define	VDEC_STILL_CPB_SIZE		(6*1024*1024)

static void* (*_vdec_get_vcore)(enum vcore_dec_codec codec_type,
							struct vcore_dec_ops *ops,
							bool rendering_dpb) = NULL;
static void (*_vdec_put_vcore)(void *vcodec_vcore_id) = NULL;

struct vdec_buffer
{
	unsigned int paddr;
	unsigned long *vaddr;
	unsigned int size;

	struct list_head list;
};

struct vdec_channel
{
	void *ves_id;
	void *vdc_id;

	bool secure_buf;
	unsigned int end_phy_addr_update;

	void *dpb_id;

	struct vdec_buffer cpb;
	struct list_head dpb_list;
	struct
	{
		unsigned int once_buf_size;
		unsigned int each_buf_size;
	}internal_buf_size;
	struct list_head internal_buf_list;
	unsigned int dpb_num;

	void *cb_arg;
	void (*callback)(void *cb_arg, struct vdec_cb_data *cb_data);
	void *vcore_select_id;
	struct mutex lock;

	unsigned int free_dpb_num;

	struct pm_qos_request mem_qos;
};

struct au_type_table
{
	enum vdec_au_type vdec;
	enum ves_au_type ves;
	enum vdc_au_type vdc;
};
static struct au_type_table au_type_table[] =
{
	{VDEC_AU_SEQ_HDR,	VES_AU_SEQ_HDR, VDC_AU_SEQ},
	{VDEC_AU_SEQ_END,	VES_AU_SEQ_END, VDC_AU_SEQ},
	{VDEC_AU_PIC, 		VES_AU_PIC, 	VDC_AU_PIC},
	{VDEC_AU_PIC_I, 	VES_AU_PIC_I, 	VDC_AU_PIC},
	{VDEC_AU_PIC_P, 	VES_AU_PIC_P, 	VDC_AU_PIC},
	{VDEC_AU_PIC_B, 	VES_AU_PIC_B, 	VDC_AU_PIC},
	{VDEC_AU_UNKNOWN,	VES_AU_UNKNOWN, VDC_AU_UNKNOWN},
};

static void _vdec_cb_vdc_used_au(void* _ch, struct vdc_report_au *report_au)
{
	struct vdec_cb_data cb_data;
	struct vdec_channel* ch = (struct vdec_channel*)_ch;

	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return;
	}

	if (ch->secure_buf == true)
		vcodec_dpb_del(ch->dpb_id, -1, report_au->start_phy_addr);
	else if (report_au->start_phy_addr)
		ch->end_phy_addr_update = report_au->end_phy_addr;

	vlog_print( VLOG_VDEC_BUF, "used au, user:0x%08X, addr:0x%08X, uid:%u\n",
				(unsigned int)report_au->p_buf, report_au->start_phy_addr,
				report_au->uid);

	cb_data.type = VDEC_CB_TYPE_FEEDED_AU;
	cb_data.feeded_au.type = au_type_table[report_au->type].vdec;
	cb_data.feeded_au.p_buf = report_au->p_buf;
	cb_data.feeded_au.start_phy_addr = report_au->start_phy_addr;
	cb_data.feeded_au.size = report_au->size;
	cb_data.feeded_au.end_phy_addr = report_au->end_phy_addr;
	cb_data.feeded_au.private_in_meta = report_au->private_in_meta;
	cb_data.feeded_au.timestamp = report_au->timestamp;
	cb_data.feeded_au.uid = report_au->uid;
	cb_data.feeded_au.input_time = report_au->input_time;
	cb_data.feeded_au.feed_time = report_au->used_time;
	cb_data.feeded_au.num_of_au = report_au->num_of_au;

	if (ch->callback)
		ch->callback(ch->cb_arg, &cb_data);
	else
		vlog_error("no callback\n");

	return;
}

static void _vdec_cb_vdc_decoded(void *_ch, struct vdc_report_dec *report_dec)
{
	struct vdec_cb_data cb_data;
	struct vdec_channel* ch = (struct vdec_channel*)_ch;
	struct vdec_buffer *node, *tmp;

	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return;
	}

	cb_data.type = VDEC_CB_TYPE_INVALID;
	cb_data.decoded_dpb.dpb.dpb_fd = -1;
	cb_data.decoded_dpb.dpb.private_out_meta = NULL;
	cb_data.decoded_dpb.dpb.rotation_angle = VCORE_DEC_ROTATE_0;

	switch (report_dec->hdr)
	{
	case VDC_REPORT_DEC_SEQ :
		cb_data.type = VDEC_CB_TYPE_DECODED_DPB;
		cb_data.decoded_dpb.hdr = VDEC_DEDCODED_SEQ;
		cb_data.decoded_dpb.u.sequence.success = report_dec->u.seq.success;
		cb_data.decoded_dpb.u.sequence.format = report_dec->u.seq.format;
		cb_data.decoded_dpb.u.sequence.profile = report_dec->u.seq.profile;
		cb_data.decoded_dpb.u.sequence.level = report_dec->u.seq.level;
		cb_data.decoded_dpb.u.sequence.ref_frame_cnt =
					report_dec->u.seq.ref_frame_cnt;
		cb_data.decoded_dpb.u.sequence.buf_width = report_dec->u.seq.buf_width;
		cb_data.decoded_dpb.u.sequence.buf_height = report_dec->u.seq.buf_height;
		cb_data.decoded_dpb.u.sequence.width = report_dec->u.seq.pic_width;
		cb_data.decoded_dpb.u.sequence.height = report_dec->u.seq.pic_height;
		cb_data.decoded_dpb.sid = report_dec->u.seq.sid;
		cb_data.decoded_dpb.uid = report_dec->u.seq.uid;
		cb_data.decoded_dpb.input_time = report_dec->u.seq.input_time;
		cb_data.decoded_dpb.feed_time = report_dec->u.seq.feed_time;
		cb_data.decoded_dpb.decode_time = report_dec->u.seq.decode_time;
		cb_data.decoded_dpb.frame_rate.residual =
					report_dec->u.seq.frame_rate.residual;
		cb_data.decoded_dpb.frame_rate.divider =
					report_dec->u.seq.frame_rate.divider;
		cb_data.decoded_dpb.crop.left = report_dec->u.seq.crop.left;
		cb_data.decoded_dpb.crop.right = report_dec->u.seq.crop.right;
		cb_data.decoded_dpb.crop.top = report_dec->u.seq.crop.top;
		cb_data.decoded_dpb.crop.bottom = report_dec->u.seq.crop.bottom;

		if (report_dec->u.seq.success)
		{
			ch->internal_buf_size.once_buf_size = report_dec->u.seq.internal_buf_size.once_buf_size;
			ch->internal_buf_size.each_buf_size = report_dec->u.seq.internal_buf_size.each_buf_size;
		}

		vlog_trace("sequence done (sid:%d)\n", cb_data.decoded_dpb.sid);
#if 0
		{
			vlog_error("input time:%d\n", report_dec->u.seq.input_time);
			vlog_error("feed time:%d\n", report_dec->u.seq.feed_time);
			vlog_error("decode time:%d\n", report_dec->u.seq.decode_time);
			vlog_error("uid:%d\n", report_dec->u.seq.uid);
		}
#endif
		break;
	case VDC_REPORT_DEC_SEQ_CHANGE :
		mutex_lock(&ch->lock);

		vlog_trace("sequence change\n");

		/* free dpb buf */
		list_for_each_entry_safe(node, tmp, &ch->dpb_list, list) {
			vcodec_dpb_del(ch->dpb_id, -1, node->paddr);
			list_del(&node->list);
			vfree(node);
		}

		list_for_each_entry_safe(node, tmp, &ch->internal_buf_list, list) {
			vcodec_dpb_del(ch->dpb_id, -1, node->paddr);
			list_del(&node->list);
			vfree(node);
		}

		ch->free_dpb_num = 0;
		ch->dpb_num = 0;

		mutex_unlock(&ch->lock);
		break;
	case VDC_REPORT_DEC_PIC :
		cb_data.type = VDEC_CB_TYPE_DECODED_DPB;
		cb_data.decoded_dpb.hdr = VDEC_DEDCODED_PIC;
		cb_data.decoded_dpb.u.picture.width = report_dec->u.pic.pic_width;
		cb_data.decoded_dpb.u.picture.height = report_dec->u.pic.pic_height;
		cb_data.decoded_dpb.u.picture.aspect_ratio =
					report_dec->u.pic.aspect_ratio;
		cb_data.decoded_dpb.u.picture.err_mbs = report_dec->u.pic.err_mbs;
		cb_data.decoded_dpb.u.picture.timestamp = report_dec->u.pic.timestamp;
		cb_data.decoded_dpb.crop.left = report_dec->u.pic.crop.left;
		cb_data.decoded_dpb.crop.right = report_dec->u.pic.crop.right;
		cb_data.decoded_dpb.crop.top = report_dec->u.pic.crop.top;
		cb_data.decoded_dpb.crop.bottom = report_dec->u.pic.crop.bottom;
		cb_data.decoded_dpb.sid = report_dec->u.pic.sid;
		cb_data.decoded_dpb.uid = report_dec->u.pic.uid;
		cb_data.decoded_dpb.input_time = report_dec->u.pic.input_time;
		cb_data.decoded_dpb.feed_time = report_dec->u.pic.feed_time;
		cb_data.decoded_dpb.decode_time = report_dec->u.pic.decode_time;
		cb_data.decoded_dpb.frame_rate.residual =
					report_dec->u.pic.frame_rate.residual;
		cb_data.decoded_dpb.frame_rate.divider =
					report_dec->u.pic.frame_rate.divider;

		cb_data.decoded_dpb.dpb.dpb_fd =
					vcodec_dpb_fd_get(ch->dpb_id, report_dec->u.pic.dpb_addr);
		cb_data.decoded_dpb.dpb.private_out_meta = report_dec->u.pic.private_out_meta;
		cb_data.decoded_dpb.dpb.rotation_angle = report_dec->u.pic.rotation_angle;

		ch->free_dpb_num--;

		vlog_trace("picture done (sid:%d)\n", cb_data.decoded_dpb.sid);
		vlog_print( VLOG_VDEC_BUF, "dpb ion:%d, addr:0x%08X, sid:%u, uid:%u\n",
					cb_data.decoded_dpb.dpb.dpb_fd, report_dec->u.pic.dpb_addr,
					report_dec->u.pic.sid, report_dec->u.pic.uid );
		break;
	case VDC_REPORT_DEC_EOS :
		cb_data.type = VDEC_CB_TYPE_DECODED_DPB;
		cb_data.decoded_dpb.hdr = VDEC_DEDCODED_EOS;
		break;
	case VDC_RETURN_DEC_BUF :
		cb_data.type = VDEC_CB_TYPE_DECODED_DPB;
		cb_data.decoded_dpb.hdr = VDEC_DEDCODED_NONE;
		cb_data.decoded_dpb.dpb.dpb_fd =
					vcodec_dpb_fd_get(ch->dpb_id, report_dec->u.buf.dpb_addr);
		cb_data.decoded_dpb.dpb.private_out_meta = report_dec->u.buf.private_out_meta;
		cb_data.decoded_dpb.dpb.rotation_angle = report_dec->u.buf.rotation_angle;
		break;
	case VDC_REPORT_FLUSH_DONE :
		cb_data.type = VDEC_CB_TYPE_FLUSH_DONE;
		break;
	case VDC_UPDATE_SEQHDR :
		vlog_error("update_seqhdr \n");
		break;
	default :
		vlog_error("unsupport hdr type:%d \n", report_dec->hdr);
		return;
	}

	if ((cb_data.type != VDEC_CB_TYPE_INVALID) && (ch->callback)) {
		vlog_print(VLOG_VDEC_CB, "type %d hdr %d\n",
					cb_data.type, cb_data.decoded_dpb.hdr);
		ch->callback(ch->cb_arg, &cb_data);
	}
}

static bool _vdec_cb_ves_written_au(void* _ch, struct ves_report_au *report_au)
{
	struct vdec_channel* ch = (struct vdec_channel*)_ch;
	struct vdc_update_au vdc_au;

	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return false;
	}

	if (report_au == NULL) {
		vlog_error("report_au is NULL\n");
		return false;
	}

	vdc_au.type = au_type_table[report_au->type].vdc;
	vdc_au.p_buf = report_au->p_buf;
	vdc_au.start_phy_addr = report_au->start_phy_addr;
	vdc_au.start_vir_ptr = report_au->start_vir_ptr;
	vdc_au.size = report_au->size;
	vdc_au.end_phy_addr = report_au->end_phy_addr;
	vdc_au.private_in_meta = report_au->private_in_meta;
	vdc_au.eos = report_au->eos;
	vdc_au.timestamp = report_au->timestamp;
	vdc_au.uid = report_au->uid;
	vdc_au.input_time = report_au->input_time;

	if (ch->vdc_id)
		vdc_update_compressed_buffer(ch->vdc_id, &vdc_au);
	else
		vlog_error("no vdc id\n");

	return true;
}

void* vdec_open(enum vcore_dec_codec codec,
				bool reordering,
				bool rendering_dpb,
				bool secure_buf,
				void *cb_arg,
				void (*callback)(void *cb_arg, struct vdec_cb_data *cb_data))
{
	int errno = 0;
	struct vdec_channel *ch;
	struct vcore_dec_ops dec_ops;

	vlog_trace("codec: %d, reordering: %d, rendering_dpb: %d, secure_buf: %d\n",
				codec, reordering, rendering_dpb, secure_buf);

	if (_vdec_get_vcore == NULL) {
		vlog_error("_vdec_select_vcore is NULL\n");
		return ERR_PTR(-EPERM);
	}

	ch = vzalloc(sizeof(struct vdec_channel));
	if (ch == NULL) {
		vlog_error("vzalloc failed\n");
		errno = -ENOMEM;
		goto err_vzalloc;
	}

	pm_qos_add_request(&ch->mem_qos, PM_QOS_ODIN_MEM_MIN_FREQ,
			PM_QOS_ODIN_MEM_MIN_FREQ_DEFAULT_VALUE);

	mutex_init(&ch->lock);

	mutex_lock(&ch->lock);
	ch->vcore_select_id = _vdec_get_vcore(codec, &dec_ops, rendering_dpb);
	if (ch->vcore_select_id == NULL) {
		vlog_error( "no available vcore\n" );
		errno =  -EINVAL;
		goto err_vdec_get_vcore;
	}

	ch->dpb_id = vcodec_dpb_open();
	if (IS_ERR_OR_NULL(ch->dpb_id)) {
		vlog_error("vcodec_dpb_open failed\n");
		errno = -EPERM;
		goto err_vcodec_dpb_open;
	}

	if (secure_buf) {
		ch->cpb.paddr = 0x0;
		ch->cpb.vaddr = (unsigned long *)NULL;
		ch->cpb.size = 0xFFFFFFFF;
	}
	else {
		if ((codec == VCORE_DEC_JPEG) || (codec == VCORE_DEC_PNG))
			ch->cpb.size = VDEC_STILL_CPB_SIZE;
		else
			ch->cpb.size = VDEC_MOVIE_CPB_SIZE;

		if (vcodec_dpb_add(ch->dpb_id, -1, &ch->cpb.paddr, &ch->cpb.vaddr,
						ch->cpb.size) == false) {
			vlog_error("cpb alloc failed\n");
			errno =  -ENOBUFS;
			goto err_cpb_alloc;
		}
	}
	ch->end_phy_addr_update = 0x0;

	ch->vdc_id = vdc_open(&dec_ops, codec,
			(unsigned int)(ch->cpb.paddr), (unsigned char*)ch->cpb.vaddr,
			ch->cpb.size,
			reordering,
			rendering_dpb,
			secure_buf,
			(void*)ch, _vdec_cb_vdc_used_au, _vdec_cb_vdc_decoded);
	if (ch->vdc_id == NULL) {
		vlog_error("vdc_open failed\n");
		errno = -EPERM;
		goto err_vdc_open;
	}

	if (secure_buf) {
		ch->ves_id = NULL;
	}
	else {
		ch->ves_id = ves_open(ch->cpb.paddr, (char*)ch->cpb.vaddr, ch->cpb.size,
				(void*)ch, _vdec_cb_ves_written_au);
		if (ch->ves_id == NULL) {
			vlog_error("ves_open failed\n");
			errno = -EPERM;
			goto err_ves_open;
		}
	}
	ch->cb_arg = cb_arg;
	ch->secure_buf = secure_buf;
	ch->callback = callback;
	ch->free_dpb_num = 0;
	ch->internal_buf_size.once_buf_size = 0;
	ch->internal_buf_size.each_buf_size = 0;

	INIT_LIST_HEAD(&ch->dpb_list);
	INIT_LIST_HEAD(&ch->internal_buf_list);

	vlog_trace("success - vdec:0x%X, ves:0x%X, vdc:0x%X, dpb:0x%X\n",
						(unsigned int)ch, (unsigned int)ch->ves_id,
						(unsigned int)ch->vdc_id, (unsigned int)ch->dpb_id);
	mutex_unlock(&ch->lock);

	return (void*)ch;

err_ves_open:
	vdc_close(ch->vdc_id);
	ch->vdc_id = NULL;

err_vdc_open:
	if (!secure_buf)
		vcodec_dpb_del(ch->dpb_id, -1, ch->cpb.paddr);
	memset(&ch->cpb, 0 , sizeof(struct vdec_buffer));

err_cpb_alloc:
	if (ch->dpb_id) {
		vcodec_dpb_close(ch->dpb_id);
		ch->dpb_id = NULL;
	}

err_vcodec_dpb_open:
	if (_vdec_put_vcore && ch->vcore_select_id) {
		_vdec_put_vcore(ch->vcore_select_id);
		ch->vcore_select_id = NULL;
	}

err_vdec_get_vcore:
	mutex_unlock(&ch->lock);
	mutex_destroy(&ch->lock);
	pm_qos_remove_request(&ch->mem_qos);
	vfree(ch);

err_vzalloc:
	return ERR_PTR(errno);
}

void vdec_close(void *id)
{
	struct vdec_channel *ch = (struct vdec_channel *)id;
	struct vdec_buffer *node, *tmp;

	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return;
	}

	mutex_lock(&ch->lock);

	ch->end_phy_addr_update = 0x0;

	if (ch->ves_id)
		ves_close(ch->ves_id);

	if (ch->vdc_id)
		vdc_close(ch->vdc_id);

	list_for_each_entry_safe(node, tmp, &ch->dpb_list, list) {
		vcodec_dpb_del(ch->dpb_id, -1, node->paddr);
		list_del(&node->list);
		vfree(node);
	}

	list_for_each_entry_safe(node, tmp, &ch->internal_buf_list, list) {
		vcodec_dpb_del(ch->dpb_id, -1, node->paddr);
		list_del(&node->list);
		vfree(node);
	}

	if (!ch->secure_buf)
		vcodec_dpb_del(ch->dpb_id, -1, ch->cpb.paddr);

	if (ch->dpb_id)
		vcodec_dpb_close(ch->dpb_id);

	/*
	printk("nr_au:%d nr_decoded:%d\n", ch->debug.nr_au, ch->debug.nr_decoded);
	printk("first dpb:%d [mesc]\n",
				jiffies_to_msecs(ch->debug.first_dpb - ch->debug.first_cpb));
	*/

	mutex_unlock(&ch->lock);
	mutex_destroy(&ch->lock);

	if (_vdec_put_vcore && ch->vcore_select_id)
		_vdec_put_vcore(ch->vcore_select_id);

	pm_qos_remove_request(&ch->mem_qos);

	vfree(ch);
}

void vdec_resume(void *id)
{
	struct vdec_channel *ch = (struct vdec_channel *)id;
	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return;
	}

	mutex_lock(&ch->lock);

	if (ch->vdc_id)
		vdc_resume(ch->vdc_id);

	mutex_unlock(&ch->lock);
}

void vdec_pause(void *id)
{
	struct vdec_channel *ch = (struct vdec_channel *)id;
	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return;
	}

	mutex_lock(&ch->lock);

	if (ch->vdc_id)
		vdc_pause(ch->vdc_id);

	mutex_unlock(&ch->lock);
}

void vdec_flush(void *id)
{
	struct vdec_channel *ch = (struct vdec_channel *)id;
	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return;
	}

	mutex_lock(&ch->lock);

	if (ch->vdc_id)
		vdc_flush(ch->vdc_id);

	if (ch->ves_id)
		ves_flush(ch->ves_id);

	vlog_trace("vcore dpb: %d/%d\n", ch->free_dpb_num, ch->dpb_num);

	ch->free_dpb_num = 0;

	mutex_unlock(&ch->lock);
}

int vdec_update_cpb(void *id,
					unsigned char *es_addr,
					int ion_fd,
					unsigned int es_size,
					void *private_in_meta,
					enum vdec_au_type au_type,
					bool eos,
					unsigned long long timestamp,
					unsigned int uid,
					bool ring_buffer,
					unsigned int align_bytes)
{
	struct ves_update_au au;
	struct vdec_channel *ch = (struct vdec_channel *)id;

	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return -EINVAL;
	}

	mutex_lock(&ch->lock);

	if (ch->secure_buf) {
		struct vdc_update_au vdc_au;
		unsigned int paddr;
		unsigned long *vaddr = 0;

		if (ion_fd < 0) {
			vlog_error("invalid fd\n");
			mutex_unlock(&ch->lock);
			return -EINVAL;
		}

		if (vcodec_dpb_add(ch->dpb_id,
							ion_fd,
							&paddr,
							NULL,
							es_size) == false) {
			vlog_error("vcodec_dpb_add failed\n");
			mutex_unlock(&ch->lock);
			return -ENOMEM;
		}

		vlog_print( VLOG_VDEC_BUF, "update au, fd:%d, user:0x%08x, addr:0x%08x, uid:%u\n",
									ion_fd, es_addr, paddr, uid);

		vdc_au.type = au_type_table[au_type].vdc;
		vdc_au.p_buf = es_addr;
		vdc_au.start_phy_addr = paddr;
		vdc_au.start_vir_ptr = (unsigned char *)vaddr;
		vdc_au.size = es_size;
		vdc_au.end_phy_addr = paddr+es_size;
		vdc_au.private_in_meta = private_in_meta;
		vdc_au.eos = eos;
		vdc_au.timestamp = timestamp;
		vdc_au.uid = uid;
		vdc_au.input_time = jiffies;

		if (ch->vdc_id)
			vdc_update_compressed_buffer(ch->vdc_id, &vdc_au);
		else
			vlog_error("no vdc id\n");
	}
	else {
		if (ch->ves_id == NULL) {
			vlog_error("ves is NULL\n");
			mutex_unlock(&ch->lock);
			return -EINVAL;
		}

		vlog_print( VLOG_VDEC_BUF, "update au, addr:0x%08X, uid:%u\n",
									(unsigned int)es_addr, uid);

		if (eos == true || es_size > 0) {
			au.type = au_type_table[au_type].ves;
			au.es_addr = es_addr;
			au.es_size = es_size;
			au.private_in_meta = private_in_meta;
			au.eos = eos;
			au.timestamp = timestamp;
			au.uid = uid;

			if (ch->end_phy_addr_update) {
				ves_update_rdptr(ch->ves_id, ch->end_phy_addr_update);
				ch->end_phy_addr_update = 0x0;
			}

			if (ves_update_buffer(ch->ves_id,
									&au, true,
									ring_buffer, align_bytes) == false) {
				vlog_error("ves_update_buffer() error\n");
				mutex_unlock(&ch->lock);
				return -1;
			}
		}
		else {
			struct vdec_cb_data cb_data;
			cb_data.type = VDEC_CB_TYPE_FEEDED_AU;
			cb_data.feeded_au.type = au_type_table[au_type].ves;
			cb_data.feeded_au.size = es_size;
			cb_data.feeded_au.private_in_meta = private_in_meta;
			cb_data.feeded_au.timestamp = timestamp;
			cb_data.feeded_au.uid = uid;

			if (ch->callback)
				ch->callback(ch->cb_arg, &cb_data);
			else
				vlog_error("no callback\n");
		}
	}

	mutex_unlock(&ch->lock);

	return 0;
}

int vdec_free_dpb(void *id,
					struct vdec_update_dpb *dpb)
{
	unsigned int paddr;
	struct vdec_channel *ch = (struct vdec_channel *)id;
	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return -EINVAL;
	}

	mutex_lock(&ch->lock);

	if (ch->end_phy_addr_update) {
		ves_update_rdptr(ch->ves_id, ch->end_phy_addr_update);
		ch->end_phy_addr_update = 0x0;
	}

	if (vcodec_dpb_addr_get(ch->dpb_id, dpb->dpb_fd, &paddr, NULL) == false) {
		vlog_error("invalid fd(%d)\n", dpb->dpb_fd);
		mutex_unlock(&ch->lock);
		/* skip this case for dynamic playback */
		return 0;
	}

	vlog_print( VLOG_VDEC_BUF, "clear dpb fd:%d, addr:0x%08x\n",
								dpb->dpb_fd, paddr );
	if (vdc_clear_decompressed_buffer(ch->vdc_id, dpb->sid, paddr,
		dpb->private_out_meta, dpb->rotation_angle) == false) {
		vlog_error("clear dpb failed - fd:%d, addr:0x%08x\n");
		mutex_unlock(&ch->lock);
		/* skip this case for dynamic playback */
		return 0;
	}

	if (ch->free_dpb_num == 0) {
		/* request DRAM frequency to be at least 800 MHz for next 100 ms,
	  	  because of DFS measurement period 50ms */
		pm_qos_update_request_timeout(&ch->mem_qos, 800000, 100000);
	}

	ch->free_dpb_num++;

	mutex_unlock(&ch->lock);

	return 0;
}

int vdec_register_dpb(void *id,
						int width,
						int height,
						int dpb_fd,
						int sid)
{
	struct vdec_buffer *internal_once_buf = NULL;
	struct vdec_buffer *internal_each_buf = NULL;
	struct vdec_buffer *external_each_dpb;

	struct vcore_dec_init_dpb init_dpb_arg;
	struct vcore_dec_dpb dec_dpb_arg;

	struct vdec_channel *ch = (struct vdec_channel *)id;
	int ret = -1;

	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return -1;
	}

	mutex_lock(&ch->lock);

	vlog_trace("sid %u, dpb_n %d, w %d, h %d, fd %d, int_size %d %d\n",
				sid, ch->dpb_num, width, height, dpb_fd,
				ch->internal_buf_size.once_buf_size,
				ch->internal_buf_size.each_buf_size);

	/* internal once dpb */
	if (ch->dpb_num == 0) {
		memset(&init_dpb_arg, 0, sizeof(struct vcore_dec_init_dpb));

		if (ch->internal_buf_size.once_buf_size > 0) {
			internal_once_buf = vzalloc(sizeof(struct vdec_buffer));
			if (IS_ERR_OR_NULL(internal_once_buf)) {
				vlog_error("vzalloc failed\n");
				mutex_unlock(&ch->lock);
				return -1;
			}

			internal_once_buf->size = ch->internal_buf_size.once_buf_size;
			if (vcodec_dpb_add(ch->dpb_id,
							-1,
							&internal_once_buf->paddr,
							NULL,
							internal_once_buf->size) == false) {
				vlog_error("internal ion alloc failed\n");
				vfree(internal_once_buf);
				mutex_unlock(&ch->lock);
				return -1;
			}

			list_add_tail(&internal_once_buf->list, &ch->internal_buf_list);

			init_dpb_arg.internal_once_buf.paddr = internal_once_buf->paddr;
			init_dpb_arg.internal_once_buf.size = internal_once_buf->size;
		}

		if (vdc_init_decompressed_buffer(ch->vdc_id, &init_dpb_arg) == false) {
			vlog_error("init dpb failed\n");
			if (internal_once_buf) {
				vcodec_dpb_del(ch->dpb_id, -1, internal_once_buf->paddr);
				list_del(&internal_once_buf->list);
				vfree(internal_once_buf);
			}
			mutex_unlock(&ch->lock);
			/* skip this case for dynamic playback */
			return 0;
		}
	}

	/* internal each dpb */
	memset(&dec_dpb_arg, 0, sizeof(struct vcore_dec_dpb));

	if (ch->internal_buf_size.each_buf_size > 0) {
		internal_each_buf = vzalloc(sizeof(struct vdec_buffer));
		if (IS_ERR_OR_NULL(internal_each_buf)) {
			vlog_error("vzalloc failed\n");
			goto err_each_alloc;
		}

		internal_each_buf->size = ch->internal_buf_size.each_buf_size;
		if (vcodec_dpb_add(ch->dpb_id,
					-1,
					&internal_each_buf->paddr,
					NULL,
					internal_each_buf->size) == false) {
			vlog_error("internal ion alloc failed\n");
			vfree(internal_each_buf);
			goto err_each_alloc;
		}
		list_add_tail(&internal_each_buf->list, &ch->internal_buf_list);

		dec_dpb_arg.internal_each_buf.paddr = internal_each_buf->paddr;
		dec_dpb_arg.internal_each_buf.vaddr = internal_each_buf->vaddr;
		dec_dpb_arg.internal_each_buf.size = internal_each_buf->size;
	}

	/* external each dpb */
	external_each_dpb = vzalloc(sizeof(struct vdec_buffer));
	if (IS_ERR_OR_NULL(external_each_dpb)) {
		vlog_error("vzalloc failed\n");
		if (internal_each_buf) {
			vcodec_dpb_del(ch->dpb_id, -1, internal_each_buf->paddr);
			list_del(&internal_each_buf->list);
			vfree(internal_each_buf);
		}
		goto err_each_alloc;
	}

	if (vcodec_dpb_add(ch->dpb_id,
						dpb_fd,
						&external_each_dpb->paddr,
						NULL,
						0) == false) {
		vlog_error("vcodec_dpb_add is NULL\n");
		if (internal_each_buf) {
			vcodec_dpb_del(ch->dpb_id, -1, internal_each_buf->paddr);
			list_del(&internal_each_buf->list);
			vfree(internal_each_buf);
		}
		vfree(external_each_dpb);
		goto err_each_alloc;
	}

	dec_dpb_arg.sid = sid;
	dec_dpb_arg.buf_width = width;
	dec_dpb_arg.buf_height = height;
	dec_dpb_arg.external_dbp_addr = external_each_dpb->paddr;

	vlog_print( VLOG_VDEC_BUF, "register dpb fd:%d, addr:0x%08X\n",
								dpb_fd, external_each_dpb->paddr );

	if (vdc_register_decompressed_buffer(ch->vdc_id, &dec_dpb_arg) == false) {
		vlog_error("register dpb failed\n");
		if (internal_each_buf) {
			vcodec_dpb_del(ch->dpb_id, -1, internal_each_buf->paddr);
			list_del(&internal_each_buf->list);
			vfree(internal_each_buf);
		}
		vcodec_dpb_del(ch->dpb_id, dpb_fd, external_each_dpb->paddr);
		vfree(external_each_dpb);
		/* skip this case for dynamic playback */
		ret = 0;
		goto err_each_alloc;
	}

	list_add_tail(&external_each_dpb->list, &ch->dpb_list);
	ch->dpb_num++;

	mutex_unlock(&ch->lock);

	return 0;

err_each_alloc :
	if (ch->dpb_num == 0) {
			if (internal_once_buf) {
				vcodec_dpb_del(ch->dpb_id, -1, internal_once_buf->paddr);
				list_del(&internal_once_buf->list);
				vfree(internal_once_buf);
			}
	}

	mutex_unlock(&ch->lock);

	return ret;
}

int vdec_get_buffer_info(void* id,
						unsigned int *cpb_size,
						unsigned int *cpb_occupancy,
						unsigned int *num_of_au)
{
	struct vdec_channel *ch = (struct vdec_channel *)id;
	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return -EINVAL;
	}

	mutex_lock(&ch->lock);

	if (ch->end_phy_addr_update) {
		ves_update_rdptr(ch->ves_id, ch->end_phy_addr_update);
		ch->end_phy_addr_update = 0x0;
	}

	if (ch->secure_buf) {
		*cpb_occupancy = 0;
		*cpb_size = 0xFFFFFFFF;
	}
	else {
		ves_get_cbp_status(ch->ves_id, cpb_occupancy, cpb_size);
	}
	*num_of_au = vdc_get_auq_occupancy(ch->vdc_id);

	mutex_unlock(&ch->lock);

	return 0;
}

int vdec_init(void* (*get_vcore)(enum vcore_dec_codec codec,
								struct vcore_dec_ops *ops,
								bool rendering_dpb),
			void (*put_vcore)(void *vcodec_vcore_id))
{
	_vdec_get_vcore = get_vcore;
	_vdec_put_vcore = put_vcore;

	vdc_init();
	ves_init();

	return 0;
}

int vdec_cleanup(void)
{
	ves_cleanup();
	vdc_cleanup();
	return 0;
}


