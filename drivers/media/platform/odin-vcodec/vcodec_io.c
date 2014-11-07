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

#include<asm/uaccess.h>

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ion.h>
#include <linux/odin_iommu.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/wakelock.h>

#include <media/odin/vcodec/vdec_io.h>
#include <media/odin/vcodec/venc_io.h>
#include <media/odin/vcodec/vppu_io.h>
#include <media/odin/vcodec/vlog.h>
#include <media/odin/vcodec/vppu_drv.h>

#include "misc/vevent/vevent.h"

#include "vdec/vdec_drv.h"
#include "venc/venc_drv.h"



struct vcodec_channel
{
	enum
	{
		VCODEC_FOR_NONE,
		VCODEC_FOR_DEC,
		VCODEC_FOR_ENC,
		VCODEC_FOR_PPU,
	} use;
	union
	{
		void *vdec_ch;
		void *venc_ch;
		void *vppu_ch;
	};

	void *vevent_id;

	struct ion_client *vppu_ion_client;
};

#define VCODEC_WAKE_LOCK_NAME "odin-vcodec"
struct wake_lock vcodec_wake_lock;
unsigned int vcodec_open_cnt = 0;


static void _vdec_io_cb(void *callee_arg, struct vdec_cb_data *cb_data)
{
	struct vcodec_channel* ch = (struct vcodec_channel*)callee_arg;
	struct vdec_io_cb_data io_cb_data;

	switch (cb_data->type)
	{
	case VDEC_CB_TYPE_FEEDED_AU :
		io_cb_data.type = VDEC_IO_CB_TYPE_FEEDED_AU;
		io_cb_data.feeded_au.p_buf = cb_data->feeded_au.p_buf;

		switch (cb_data->feeded_au.type)
		{
		case VDEC_AU_SEQ_HDR:
			io_cb_data.feeded_au.type = VDEC_IO_AU_SEQ_HDR;
			break;
		case VDEC_IO_AU_SEQ_END:
			io_cb_data.feeded_au.type = VDEC_AU_SEQ_END;
			break;
		case VDEC_IO_AU_PIC:
			io_cb_data.feeded_au.type = VDEC_IO_AU_PIC;
			break;
		case VDEC_IO_AU_PIC_I:
			io_cb_data.feeded_au.type = VDEC_IO_AU_PIC_I;
			break;
		case VDEC_IO_AU_PIC_P:
			io_cb_data.feeded_au.type = VDEC_IO_AU_PIC_P;
			break;
		case VDEC_IO_AU_PIC_B:
			io_cb_data.feeded_au.type = VDEC_IO_AU_PIC_B;
			break;
		case VDEC_IO_AU_UNKNOWN:
		default:
			io_cb_data.feeded_au.type = VDEC_IO_AU_UNKNOWN;
			break;
		}

		io_cb_data.feeded_au.start_phy_addr = cb_data->feeded_au.start_phy_addr;
		io_cb_data.feeded_au.size = cb_data->feeded_au.size;
		io_cb_data.feeded_au.end_phy_addr = cb_data->feeded_au.end_phy_addr;
		io_cb_data.feeded_au.private_in_meta = cb_data->feeded_au.private_in_meta;
		io_cb_data.feeded_au.eos = cb_data->feeded_au.eos;
		io_cb_data.feeded_au.timestamp = cb_data->feeded_au.timestamp;
		io_cb_data.feeded_au.uid = cb_data->feeded_au.uid;
		io_cb_data.feeded_au.input_time = cb_data->feeded_au.input_time;
		io_cb_data.feeded_au.feed_time = cb_data->feeded_au.feed_time;
		io_cb_data.feeded_au.num_of_au = cb_data->feeded_au.num_of_au;
		break;

	case VDEC_CB_TYPE_DECODED_DPB :
		io_cb_data.type = VDEC_IO_CB_TYPE_DECODED_DPB;

		switch (cb_data->decoded_dpb.hdr)
		{
		case VDEC_DEDCODED_SEQ:
			io_cb_data.decoded_dpb.hdr = VDEC_IO_DEDCODED_SEQ;
			io_cb_data.decoded_dpb.u.sequence.success = cb_data->decoded_dpb.u.sequence.success;
			io_cb_data.decoded_dpb.u.sequence.format = cb_data->decoded_dpb.u.sequence.format;
			io_cb_data.decoded_dpb.u.sequence.profile = cb_data->decoded_dpb.u.sequence.profile;
			io_cb_data.decoded_dpb.u.sequence.level = cb_data->decoded_dpb.u.sequence.level;
			io_cb_data.decoded_dpb.u.sequence.ref_frame_cnt = cb_data->decoded_dpb.u.sequence.ref_frame_cnt;
			io_cb_data.decoded_dpb.u.sequence.buf_width = cb_data->decoded_dpb.u.sequence.buf_width;
			io_cb_data.decoded_dpb.u.sequence.buf_height = cb_data->decoded_dpb.u.sequence.buf_height;
			io_cb_data.decoded_dpb.u.sequence.width = cb_data->decoded_dpb.u.sequence.width;
			io_cb_data.decoded_dpb.u.sequence.height = cb_data->decoded_dpb.u.sequence.height;
			io_cb_data.decoded_dpb.sid = cb_data->decoded_dpb.sid;
			io_cb_data.decoded_dpb.uid = cb_data->decoded_dpb.uid;
			io_cb_data.decoded_dpb.input_time = cb_data->decoded_dpb.input_time;
			io_cb_data.decoded_dpb.feed_time = cb_data->decoded_dpb.feed_time;
			io_cb_data.decoded_dpb.decode_time = cb_data->decoded_dpb.decode_time;
			io_cb_data.decoded_dpb.frame_rate.residual = cb_data->decoded_dpb.frame_rate.residual;
			io_cb_data.decoded_dpb.frame_rate.divider = cb_data->decoded_dpb.frame_rate.divider;
			io_cb_data.decoded_dpb.crop.left = cb_data->decoded_dpb.crop.left;
			io_cb_data.decoded_dpb.crop.right = cb_data->decoded_dpb.crop.right;
			io_cb_data.decoded_dpb.crop.top = cb_data->decoded_dpb.crop.top;
			io_cb_data.decoded_dpb.crop.bottom = cb_data->decoded_dpb.crop.bottom;
			break;
		case VDEC_DEDCODED_PIC :
			io_cb_data.decoded_dpb.hdr = VDEC_IO_DEDCODED_PIC;
			io_cb_data.decoded_dpb.u.picture.width = cb_data->decoded_dpb.u.picture.width;
			io_cb_data.decoded_dpb.u.picture.height = cb_data->decoded_dpb.u.picture.height;
			io_cb_data.decoded_dpb.u.picture.aspect_ratio = cb_data->decoded_dpb.u.picture.aspect_ratio;
			io_cb_data.decoded_dpb.u.picture.err_mbs = cb_data->decoded_dpb.u.picture.err_mbs;
			io_cb_data.decoded_dpb.u.picture.timestamp = cb_data->decoded_dpb.u.picture.timestamp;
			io_cb_data.decoded_dpb.crop.left = cb_data->decoded_dpb.crop.left;
			io_cb_data.decoded_dpb.crop.right = cb_data->decoded_dpb.crop.right;
			io_cb_data.decoded_dpb.crop.top = cb_data->decoded_dpb.crop.top;
			io_cb_data.decoded_dpb.crop.bottom = cb_data->decoded_dpb.crop.bottom;
			io_cb_data.decoded_dpb.sid = cb_data->decoded_dpb.sid;
			io_cb_data.decoded_dpb.uid = cb_data->decoded_dpb.uid;
			io_cb_data.decoded_dpb.input_time = cb_data->decoded_dpb.input_time;
			io_cb_data.decoded_dpb.feed_time = cb_data->decoded_dpb.feed_time;
			io_cb_data.decoded_dpb.decode_time = cb_data->decoded_dpb.decode_time;
			io_cb_data.decoded_dpb.frame_rate.residual = cb_data->decoded_dpb.frame_rate.residual;
			io_cb_data.decoded_dpb.frame_rate.divider = cb_data->decoded_dpb.frame_rate.divider;
			break;
		case VDEC_DEDCODED_EOS :
			io_cb_data.decoded_dpb.hdr = VDEC_IO_DEDCODED_EOS;
			break;
		case VDEC_DEDCODED_NONE :
			io_cb_data.decoded_dpb.hdr = VDEC_IO_DEDCODED_NONE;
			break;
		default:
			vlog_error("unsupport decoded hdr: %d \n", cb_data->decoded_dpb.hdr);
			return;
		}

		io_cb_data.decoded_dpb.dpb.dpb_fd = cb_data->decoded_dpb.dpb.dpb_fd;
		io_cb_data.decoded_dpb.dpb.private_out_meta = cb_data->decoded_dpb.dpb.private_out_meta;
		switch (cb_data->decoded_dpb.dpb.rotation_angle) {
			case VCORE_DEC_ROTATE_0 :
				io_cb_data.decoded_dpb.dpb.rotation_angle = VCORE_IO_ROTATE_0;
				break;
			case VCORE_DEC_ROTATE_90 :
				io_cb_data.decoded_dpb.dpb.rotation_angle = VCORE_IO_ROTATE_90;
				break;
			case VCORE_DEC_ROTATE_180 :
				io_cb_data.decoded_dpb.dpb.rotation_angle = VCORE_IO_ROTATE_180;
				break;
			case VCORE_DEC_ROTATE_270 :
				io_cb_data.decoded_dpb.dpb.rotation_angle = VCORE_IO_ROTATE_270;
				break;
			default :
				vlog_error("rotation angle: %d\n", cb_data->decoded_dpb.dpb.rotation_angle);
				io_cb_data.decoded_dpb.dpb.rotation_angle = VCORE_IO_ROTATE_0;
				break;
		}
		break;

	case VDEC_CB_TYPE_FLUSH_DONE :
		vlog_trace( "flushed\n");
		io_cb_data.type = VDEC_IO_CB_TYPE_FLUSH_DONE;
		break;

	default :
		vlog_error("unsupport cb type: %d \n", cb_data->type);
		return;
	}

	vevent_send(ch->vevent_id, (void*)&io_cb_data, sizeof(struct vdec_io_cb_data));
}

static int _vdec_io_open(struct vcodec_channel *ch, unsigned long arg)
{
	int ret;
	struct vdec_io_open_arg a;

	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return -EINVAL;
	}

	ret = copy_from_user((void*)&a, (const void __user*)arg, sizeof(struct vdec_io_open_arg));
	if (ret != 0) {
		vlog_error("__copy_from_user failed. ret:%d \n", ret);
		return -EINVAL;
	}

	ch->vevent_id = a.vevent_id;
	ch->vdec_ch = vdec_open(a.codec,
							a.reordering ? true : false,
							a.rendering_dpb ? true : false,
							a.secure_buf ? true : false,
							(void*)ch, _vdec_io_cb);
	if (IS_ERR_OR_NULL(ch->vdec_ch)) {
		ch->vdec_ch = NULL;
		vlog_error("vdec_open failed\n");
		return -1;
	}

	return 0;
}

static int _vdec_io_close(struct vcodec_channel *ch, unsigned long arg)
{
	if (ch == NULL)
		return -1;

	if (ch->vdec_ch) {
		vdec_close(ch->vdec_ch);
		ch->vdec_ch = NULL;
	}

	return 0;
}

static int _vdec_io_resume(struct vcodec_channel *ch, unsigned long arg)
{
	if (ch == NULL)
		return -1;

	vdec_resume(ch->vdec_ch);
	return 0;
}

static int _vdec_io_pause(struct vcodec_channel *ch, unsigned long arg)
{
	if (ch == NULL)
		return -1;

	vdec_pause(ch->vdec_ch);
	return 0;
}

static int _vdec_io_flush(struct vcodec_channel *ch, unsigned long arg)
{
	if (ch == NULL)
		return -1;

	vdec_flush(ch->vdec_ch);
	return 0;
}

static int _vdec_io_update_cpb(struct vcodec_channel *ch, unsigned long arg)
{
	int ret = -1;
	struct vdec_io_update_cpb a;
	enum vdec_au_type au_type;

	if (ch == NULL)
		return -1;

	ret = copy_from_user((void*)&a, (const void __user*)arg,
			sizeof(struct vdec_io_update_cpb));
	if (ret != 0) {
		vlog_error("__copy_from_user failed. ret:%d \n", ret);
		return -EINVAL;
	}

	switch (a.type)
	{
	case VDEC_IO_AU_SEQ_HDR:
		au_type = VDEC_AU_SEQ_HDR;
		break;
	case VDEC_IO_AU_SEQ_END:
		au_type = VDEC_AU_SEQ_END;
		break;
	case VDEC_IO_AU_PIC:
		au_type = VDEC_AU_PIC;
		break;
	case VDEC_IO_AU_PIC_I:
		au_type = VDEC_AU_PIC_I;
		break;
	case VDEC_IO_AU_PIC_P:
		au_type = VDEC_AU_PIC_P;
		break;
	case VDEC_IO_AU_PIC_B:
		au_type = VDEC_AU_PIC_B;
		break;
	case VDEC_IO_AU_UNKNOWN:
	default:
		au_type = VDEC_AU_UNKNOWN;
		break;
	}

	return vdec_update_cpb(ch->vdec_ch,
							a.es.user_addr,
							a.es.ion_fd,
							a.es_size,
							a.private_in_meta,
							au_type,
							a.eos ? true : false,
							a.timestamp,
							a.uid,
							a.ring_buffer,
							a.align_bytes);
}

static int _vdec_io_free_dpb(struct vcodec_channel *ch, unsigned long arg)
{
	int ret;
	struct vdec_update_dpb dpb;
	struct vdec_io_update_dpb a;

	if (ch == NULL)
		return -1;

	ret = copy_from_user((void*)&a, (const void __user*)arg, sizeof(struct vdec_io_update_dpb));
	if (ret != 0) {
		vlog_error("__copy_from_user failed. ret:%d \n", ret);
		return -EINVAL;
	}

	dpb.sid = a.sid;
	dpb.dpb_fd = a.dpb_fd;
	dpb.private_out_meta = a.private_out_meta;
	switch (a.rotation_angle) {
		case VCORE_IO_ROTATE_0 :
			dpb.rotation_angle = VCORE_DEC_ROTATE_0;
			break;
		case VCORE_IO_ROTATE_90 :
			dpb.rotation_angle = VCORE_DEC_ROTATE_90;
			break;
		case VCORE_IO_ROTATE_180 :
			dpb.rotation_angle = VCORE_DEC_ROTATE_180;
			break;
		case VCORE_IO_ROTATE_270 :
			dpb.rotation_angle = VCORE_DEC_ROTATE_270;
			break;
		default :
			vlog_error("rotation angle: %d\n", a.rotation_angle);
			dpb.rotation_angle = VCORE_DEC_ROTATE_0;
			break;
	}

	ret = vdec_free_dpb(ch->vdec_ch, &dpb);
	if (ret != 0) {
		vlog_error("vdec_free_dpb failed. ret:%d \n", ret);
		return -EINVAL;
	}

	return ret;
}

static int _vdec_io_get_buffer_info(struct vcodec_channel *ch, unsigned long arg)
{
	int ret;
	struct vdec_io_get_buffer_status a;

	if (ch == NULL)
		return -1;

	if (vdec_get_buffer_info(ch->vdec_ch, &a.cpb_size, &a.cpb_occupancy,
				&a.num_of_au) < 0) {
		vlog_error("vdec_get_buffer_info failed\n");
		return -1;
	}

	ret = copy_to_user((void __user*)arg, &a,
			sizeof(struct vdec_io_get_buffer_status));
	if (ret != 0) {
		vlog_error("__copy_to_user failed. ret:%d \n", ret);
		return -1;
	}

	return 0;
}

static int _vdec_io_register_dpb(struct vcodec_channel *ch, unsigned long arg)
{
	int ret;
	struct vdec_io_register_dpb a = {0,};

	if (ch == NULL)
		return -1;

	ret = copy_from_user((void*)&a, (const void __user*)arg, sizeof(struct vdec_io_register_dpb));
	if (ret != 0) {
		vlog_error("__copy_from_user failed. ret:%d \n", ret);
		return -EINVAL;
	}

	ret = vdec_register_dpb(ch->vdec_ch, a.width, a.height, a.dpb_fd, a.sid);
	if (ret != 0) {
		vlog_error("vdec_register_dpb failed. ret:%d \n", ret);
		return -EINVAL;
	}

	return 0;
}

static int (*vdec_io_fp[])(struct vcodec_channel *ch, unsigned long arg) =
{
	_vdec_io_open,
	_vdec_io_close,
	_vdec_io_resume,
	_vdec_io_pause,
	_vdec_io_flush,
	_vdec_io_register_dpb,
	_vdec_io_update_cpb,
	_vdec_io_free_dpb,
	_vdec_io_get_buffer_info,
};

static int _vdec_io_do_ioctl(struct vcodec_channel* ch, unsigned long cmd, unsigned long arg)
{
	int errno = 0;
	unsigned long nr = _IOC_NR(cmd);

	if (nr < sizeof(vdec_io_fp)/sizeof(int*) && vdec_io_fp[nr] != NULL)
		errno = vdec_io_fp[nr](ch, arg);
	else {
		vlog_error("unknwon IOCTL cmd(cmd : %d)\n", (int)cmd);
		errno = -EPERM;
	}

	return errno;
}

static void _venc_io_cb(void *cb_arg, struct venc_cb_data *cb_data)
{
	struct vcodec_channel* ch = (struct vcodec_channel*)cb_arg;
	struct venc_io_cb_encoded io_cb_encoded;

	memset(&io_cb_encoded, 0, sizeof(struct venc_io_cb_encoded));

	switch (cb_data->type)
	{
	case VENC_CB_TYPE_USED_FB_EP :
		io_cb_encoded.type = VENC_IO_CB_TYPE_USED_FB_EP;
		break;
	case VENC_CB_TYPE_FLUSH_DONE :
		io_cb_encoded.type = VENC_IO_CB_TYPE_FLUSH_DONE;
		break;
	default :
		vlog_error("unsupport cb type: %d \n", cb_data->type);
		return;
	}

	io_cb_encoded.fb.ion_fd = cb_data->fb.ion_fd;
	io_cb_encoded.fb.size = cb_data->fb.size;
	io_cb_encoded.fb.private_in_meta = cb_data->fb.private_in_meta;
	io_cb_encoded.fb.eos = cb_data->fb.eos;
	io_cb_encoded.fb.timestamp = cb_data->fb.timestamp;
	io_cb_encoded.fb.uid = cb_data->fb.uid;
	io_cb_encoded.fb.input_time = cb_data->fb.input_time;
	io_cb_encoded.fb.feed_time = cb_data->fb.feed_time;
	io_cb_encoded.fb.num_of_pic = cb_data->fb.num_of_pic;

	io_cb_encoded.es.last_es_size = cb_data->es.last_es_size;
	io_cb_encoded.es.num_of_es = cb_data->es.num_of_es;
	io_cb_encoded.es.epb_occupancy = cb_data->es.epb_occupancy;
	io_cb_encoded.es.epb_size = cb_data->es.epb_size;

	vevent_send(ch->vevent_id, &io_cb_encoded, sizeof(struct venc_io_cb_encoded));
}

static int _venc_io_open(struct vcodec_channel *ch, unsigned long arg)
{
	int ret;
	struct venc_io_open_arg a;
	struct vcore_enc_config enc_config;

	ret = copy_from_user((void*)&a, (const void __user*)arg, sizeof(a));
	if (ret != 0) {
		vlog_error("__copy_from_user failed. ret:%d\n", ret);
		return -EINVAL;
	}

	switch (a.codec_type) {
	case VENC_IO_CODEC_AVC:
		enc_config.codec_type = VCORE_ENC_AVC;
		enc_config.codec_param.h264.gop_size = a.codec_param.h264.gop_size;
		enc_config.codec_param.h264.mb_num_per_slice = a.codec_param.h264.mb_num_per_slice;
		enc_config.codec_param.h264.cabac_init_idx = a.codec_param.h264.cabac_init_idx;
		switch (a.codec_param.h264.loop_filter_mode) {
		case VENC_IO_LOOP_FILTER_ENABLE :
			enc_config.codec_param.h264.loop_filter_mode = VCORE_ENC_LOOP_FILTER_ENABLE;
			break;
		case VENC_IO_LOOP_FILTER_DISABLE :
			enc_config.codec_param.h264.loop_filter_mode = VCORE_ENC_LOOP_FILTER_DISABLE;
			break;
		case VENC_IO_LOOP_FILTER_DISABLE_SLICE_BOUNDARY :
			enc_config.codec_param.h264.loop_filter_mode = VCORE_ENC_LOOP_FILTER_DISABLE_SLICE_BOUNDARY;
			break;
		default :
			vlog_error("invalid h264.loop_filter_mode %d\n", a.codec_param.h264.loop_filter_mode);
			break;
		}
		enc_config.codec_param.h264.const_intra_pred_enable = (a.codec_param.h264.const_intra_pred_enable == 1) ? VCORE_TRUE : VCORE_FALSE;
		enc_config.codec_param.h264.profile = a.codec_param.h264.profile;

		switch (a.codec_param.h264.profile) {
		case VENC_IO_H264_PROFILE_BASE :
			enc_config.codec_param.h264.profile = VCORE_ENC_H264_PROFILE_BASE;
			break;
		case VENC_IO_H264_PROFILE_MAIN :
			enc_config.codec_param.h264.profile = VCORE_ENC_H264_PROFILE_MAIN;
			break;
		case VENC_IO_H264_PROFILE_HIGH :
			enc_config.codec_param.h264.profile = VCORE_ENC_H264_PROFILE_HIGH;
			break;
		default :
			vlog_error("invalid h264.profile %d\n", a.codec_param.h264.profile);
			break;
		}
    enc_config.codec_param.h264.level_idc = a.codec_param.h264.level_idc;
		enc_config.codec_param.h264.field_enable = (a.codec_param.h264.field_enable == 1) ? VCORE_TRUE : VCORE_FALSE;
		enc_config.codec_param.h264.cabac_enable = (a.codec_param.h264.cabac_enable == 1) ? VCORE_TRUE : VCORE_FALSE;
		break;
	case VCORE_ENC_MPEG4:
		enc_config.codec_type = VCORE_ENC_MPEG4;
		enc_config.codec_param.mpeg4.gop_size = a.codec_param.mpeg4.gop_size;
		enc_config.codec_param.mpeg4.short_video_header_enable = (a.codec_param.mpeg4.short_video_header_enable == 1) ? VCORE_TRUE : VCORE_FALSE;
		enc_config.codec_param.mpeg4.intra_dc_vlc_threshold = a.codec_param.mpeg4.intra_dc_vlc_threshold;
		break;
	case VCORE_ENC_H263:
		enc_config.codec_type = VCORE_ENC_H263;
		enc_config.codec_param.h263.gop_size = a.codec_param.h263.gop_size;
		switch (a.codec_param.h263.profile) {
		case VENC_IO_H263_PROFILE_BASE :
			enc_config.codec_param.h263.profile = VCORE_ENC_H263_PROFILE_BASE;
			break;
		case VENC_IO_H263_PROFILE_SWV2 :
			enc_config.codec_param.h263.profile = VCORE_ENC_H263_PROFILE_SWV2;
			break;
		default :
			vlog_error("invalid h263.profile %d\n", a.codec_param.h263.profile);
			break;
		}
		break;
	case VCORE_ENC_JPEG:
		enc_config.codec_type = VCORE_ENC_JPEG;
		break;
	default:
		vlog_error("invalid codec_type %d\n", a.codec_type);
		ch->venc_ch = NULL;
        	return -1;
	}

	enc_config.pic_width = a.pic_width;
	enc_config.pic_height = a.pic_height;
	enc_config.buf_width = a.buf_width;
	enc_config.buf_height = a.buf_height;

	enc_config.frame_rate.residual = a.frame_rate.residual;
	enc_config.frame_rate.divider = a.frame_rate.divider;
	switch (a.bit_rate.control_mode) {
	case VENC_IO_RATE_CONTROL_DISABLE :
		enc_config.bit_rate.control_mode = VCORE_ENC_RATE_CONTROL_DISABLE;
		break;
	case VENC_IO_RATE_CONTROL_CBR :
		enc_config.bit_rate.control_mode = VCORE_ENC_RATE_CONTROL_CBR;
		break;
	case VENC_IO_RATE_CONTROL_ABR :
		enc_config.bit_rate.control_mode = VCORE_ENC_RATE_CONTROL_ABR;
		break;
	default :
		vlog_error("invalid bit_rate control mode %d\n", a.bit_rate.control_mode);
		ch->venc_ch = NULL;
        	return -1;
	}

	enc_config.bit_rate.auto_skip_enable = (a.bit_rate.auto_skip_enable == 1) ? VCORE_TRUE : VCORE_FALSE;
	enc_config.bit_rate.target_kbps = a.bit_rate.target_kbps;
	enc_config.error_correction.hec_enable = (a.error_correction.hec_enable == 1) ? VCORE_TRUE : VCORE_FALSE;
	enc_config.error_correction.data_partitioning_enable = (a.error_correction.data_partitioning_enable == 1) ? VCORE_TRUE : VCORE_FALSE;
	enc_config.error_correction.reversible_vlc_enble = (a.error_correction.reversible_vlc_enble == 1) ? VCORE_TRUE : VCORE_FALSE;
	enc_config.cyclic_intra_block_refresh.enable = (a.cyclic_intra_block_refresh.enable == 1) ? VCORE_TRUE : VCORE_FALSE;
	enc_config.cyclic_intra_block_refresh.mb_num = a.cyclic_intra_block_refresh.mb_num;
	switch (a.cbcr_interleave) {
	case  VENC_IO_CBCR_INTERLEAVE_NONE :
		enc_config.cbcr_interleave = VCORE_ENC_CBCR_INTERLEAVE_NONE;
		break;
	case VENC_IO_CBCR_INTERLEAVE_NV12 :
		enc_config.cbcr_interleave = VCORE_ENC_CBCR_INTERLEAVE_NV12;
		break;
        case VENC_IO_CBCR_INTERLEAVE_NV21 :
        	enc_config.cbcr_interleave = VCORE_ENC_CBCR_INTERLEAVE_NV21;
        	break;
        default :
        	vlog_error("invalid cbcr_interleave %d\n", a.cbcr_interleave);
        	ch->venc_ch = NULL;
        	return -1;
	}

	switch (a.format) {
	case VENC_IO_FORMAT_420 :
		enc_config.format = VCORE_ENC_FORMAT_420;
		break;
	case VENC_IO_FORMAT_422 :
		enc_config.format = VCORE_ENC_FORMAT_422;
		break;
	case VENC_IO_FORMAT_224 :
		enc_config.format = VCORE_ENC_FORMAT_224;
		break;
	case VENC_IO_FORMAT_444 :
		enc_config.format = VCORE_ENC_FORMAT_444;
		break;
	case VENC_IO_FORMAT_400 :
		enc_config.format = VCORE_ENC_FORMAT_400;
		break;
	default :
		vlog_error("invalid format %d\n", a.format);
		break;
	}

	ch->vevent_id = a.vevent_id;
	ch->venc_ch = venc_open(&enc_config, ch, _venc_io_cb);
	if (IS_ERR_OR_NULL(ch->venc_ch)) {
		ch->venc_ch = NULL;
		vlog_error("venc_open failed\n");
		return -1;
	}

	return 0;
}

static int _venc_io_close(struct vcodec_channel *ch, unsigned long arg)
{
	if (ch == NULL)
		return -1;

	if (ch->venc_ch) {
		venc_close(ch->venc_ch);
		ch->venc_ch = NULL;
	}
	return 0;
}

static int _venc_io_resume(struct vcodec_channel *ch, unsigned long arg)
{
	venc_resume(ch->venc_ch);

	return 0;
}

static int _venc_io_pause(struct vcodec_channel *ch, unsigned long arg)
{
	venc_pause(ch->venc_ch);

	return 0;
}

static int _venc_io_flush(struct vcodec_channel *ch, unsigned long arg)
{
	venc_flush(ch->venc_ch);

	return 0;
}

static int _venc_io_update_fb(struct vcodec_channel *ch, unsigned long arg)
{
	int ret;
	struct venc_io_update_fb a;

	ret = copy_from_user((void*)&a, (const void __user*)arg, sizeof(a));
	if (ret != 0) {
		vlog_error("__copy_from_user failed. ret:%d\n", ret);
		return -EINVAL;
	}

	return venc_update_fb(ch->venc_ch, a.ion_fd, a.size, a.private_in_meta, (bool)a.eos, a.timestamp, a.uid);
}

static int _venc_io_update_epb(struct vcodec_channel *ch, unsigned long arg)
{
	int ret;
	struct venc_io_free_esq a;

	ret = copy_from_user((void*)&a, (const void __user*)arg, sizeof(a));
	if (ret != 0) {
		vlog_error("copy_from_user failed. ret:%d\n", ret);
		return -EINVAL;
	}

	venc_update_epb(ch->venc_ch, a.num_of_free_esq_node);

	return 0;
}

static int _venc_io_get_epb(struct vcodec_channel *ch, unsigned long arg)
{
	int ret;
	struct venc_io_epb a;
	enum venc_es_type es_type;

	ret = copy_from_user((void*)&a, (const void __user*)arg, sizeof(a));
	if (ret != 0) {
		vlog_error("copy_from_user failed. ret:%d\n", ret);
		return -EINVAL;
	}

	if (venc_get_epb(ch->venc_ch, a.buf_ptr, a.buf_size, &a.es_size, &es_type, &a.timestamp, &a.uid, &a.input_time, &a.feed_time, &a.encode_time) < 0) {
		vlog_error("venc_get_epb failed\n");
		return -1;
	}

	switch (es_type)
	{
		case VENC_ES_SEQ_HDR:
			a.type = VENC_IO_ES_SEQ_HDR;
			break;
		case VENC_ES_PIC_I:
			a.type = VENC_IO_ES_PIC_I;
			break;
		case VENC_ES_PIC_P:
			a.type = VENC_IO_ES_PIC_P;
			break;
		case VENC_ES_PIC_B:
			a.type = VENC_IO_ES_PIC_B;
			break;
		case VENC_ES_EOS:
			a.type = VENC_IO_ES_EOS;
			break;
		default:
			a.type = VENC_IO_ES_SEQ_HDR;
			break;
	}

	ret = copy_to_user((void __user*)arg, &a, sizeof(a));
	if (ret != 0) {
		vlog_error("__copy_to_user failed. ret:%d\n", ret);
		return -EIO;
	}

	return 0;
}

static int _venc_io_get_buffer_info(struct vcodec_channel *ch, unsigned long arg)
{
	int ret;
	struct venc_io_get_buffer_status a;

	 if (venc_get_buffer_info(ch->venc_ch, &a.num_of_pic, &a.last_es_size, &a.num_of_es, &a.epb_occupancy, &a.epb_size) < 0) {
		 vlog_error("venc_get_buffer_info failed\n");
		 return -1;
	 }

	 ret = copy_to_user((void __user*)arg, &a, sizeof(a));
	 if (ret != 0) {
		 vlog_error("copy_to_user failed. ret:%d\n", ret);
		 return -EIO;
	 }

	return 0;
}

static int _venc_io_set_config(struct vcodec_channel *ch, unsigned long arg)
{
	int ret;
	struct venc_io_running_arg a;
	struct vcore_enc_running_config enc_running_config;

	ret = copy_from_user((void*)&a, (const void __user*)arg, sizeof(a));
	if (ret != 0) {
		vlog_error("__copy_from_user failed. ret:%d\n", ret);
		return -EINVAL;
	}

	switch (a.index) {
	case VENC_IO_RUNNING_PARAM_BITRATE :
		enc_running_config.index = VCORE_ENC_RUNNING_PARAM_BITRATE;
		enc_running_config.data.bit_rate_kbps = a.data.bit_rate_kbps;
		break;
	case VENC_IO_RUNNING_PARAM_INTRA_REFRESH :
		enc_running_config.index = VCORE_ENC_RUNNING_PARAM_INTRA_REFRESH;
		enc_running_config.data.intra_refresh = a.data.intra_refresh;
		break;
	default :
		vlog_error("invalid index(%s) %d\n", __func__,  a.index);
		break;
	}

	ret = venc_set_config(ch->venc_ch, &enc_running_config);
	if (ret != 0) {
		vlog_error("venc_set_config fail\n");
		return -1;
	}

	return 0;
}

static int (*_venc_io_fp[])(struct vcodec_channel *ch, unsigned long arg) =
{
	_venc_io_open,
	_venc_io_close,
	_venc_io_resume,
	_venc_io_pause,
	_venc_io_flush,
	_venc_io_update_fb,
	_venc_io_update_epb,
	_venc_io_get_epb,
	_venc_io_get_buffer_info,
	_venc_io_set_config,
};

int _venc_io_do_ioctl(struct vcodec_channel *ch, unsigned long cmd, unsigned long arg)
{
	int errno = 0;
	unsigned long nr = _IOC_NR(cmd);

	if (nr < sizeof(_venc_io_fp)/sizeof(int*) && _venc_io_fp[nr] != NULL)
		errno = _venc_io_fp[nr](ch, arg);
	else {
		vlog_error("unknwon IOCTL cmd(cmd : %d)\n", (int)cmd);
		errno = -EPERM;
	}

	return errno;
}

static void *__vppu_io_fd_map(struct vcodec_channel *ch, int fd, unsigned int *iova)
{
	unsigned int _iova;
	struct ion_handle *ion_handle;

	ion_handle = ion_import_dma_buf(ch->vppu_ion_client, fd);
	if (IS_ERR_OR_NULL(ion_handle)) {
		vlog_error("fd(%d) ion_import_dma_buf failed\n", fd);
		goto err_ion_import_dma_buf;
	}

	if (odin_ion_map_iommu(ion_handle, ODIN_SUBSYS_VSP) < 0) {
		vlog_error("fd(%d) odin_ion_map_iommu failed\n", fd);
		goto err_odin_ion_map_iommu;
	}

	_iova = (unsigned int)odin_ion_get_iova_of_buffer(ion_handle, ODIN_SUBSYS_VSP);
	if (_iova == 0) {
		vlog_error("fd(%d) odin_ion_get_iova_of_buffer failed\n", fd);
		goto err_odin_ion_get_iova_of_buffer;
	}

	*iova = _iova;

	return (void*)ion_handle;

err_odin_ion_get_iova_of_buffer:
err_odin_ion_map_iommu:
	ion_free(ch->vppu_ion_client, ion_handle);

err_ion_import_dma_buf:
	*iova = 0;
	return ERR_PTR(-1);
}

static void __vppu_io_fd_unmap(struct vcodec_channel *ch, void *map)
{
	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return;
	}

	if (map == NULL) {
		vlog_error("map is NULL\n");
		return;
	}

	ion_free(ch->vppu_ion_client, (struct ion_handle*)map);
}

int _vppu_io_open(struct vcodec_channel *ch, unsigned long arg)
{
	int errno = -1;

	ch->vppu_ion_client = odin_ion_client_create("vppu");
	if (IS_ERR_OR_NULL(ch->vppu_ion_client)) {
		vlog_error("odin_ion_client_create failed\n");
		errno = -EIO;
		goto err_odin_ion_client_create;
	}

	ch->vppu_ch = vppu_open(true, 0, 0, 0, 0, NULL, NULL);
	if (IS_ERR_OR_NULL(ch->vppu_ch)) {
		vlog_error("vppu_open failed\n");
		errno = -EPERM;
		goto err_vppu_open;
	}

	return 0;

err_vppu_open:
	odin_ion_client_destroy(ch->vppu_ion_client);
	ch->vppu_ion_client = NULL;

err_odin_ion_client_create:
	return errno;
}

int _vppu_io_close(struct vcodec_channel *ch, unsigned long arg)
{
	if (ch == NULL) {
		vlog_error("ch is NULL\n");
		return -1;
	}

	if (ch->vppu_ch) {
		vppu_close(ch->vppu_ch);
		ch->vppu_ch = NULL;
	}

	if (ch->vppu_ion_client) {
		odin_ion_client_destroy(ch->vppu_ion_client);
		ch->vppu_ion_client = NULL;
	}

	return 0;
}

int _vppu_io_rotate(struct vcodec_channel *ch, unsigned long arg)
{
	int ret;
	struct vppu_io_rotate a;
	unsigned int src_iova, dst_iova;
	enum vppu_image_format vppu_image_format;
	void *src_map = NULL;
	void *dst_map = NULL;

	if (ch->vppu_ch == NULL) {
		vlog_error("ch->vppu_ch is NULL\n");
		return -EINVAL;
	}

	ret = copy_from_user(&a, (void*)arg, sizeof(struct vppu_io_rotate));
	if (ret != 0) {
		vlog_error("copy_from_user failed\n");
		return -EINVAL;
	}

	switch (a.format) {
	case VPPU_IO_IMAGE_FORMAT_YUV420_2P:
		vppu_image_format = VPPU_IMAGE_FORMAT_YUV420_2P;
		break;
	case VPPU_IO_IMAGE_FORMAT_YUV420_3P:
		vppu_image_format = VPPU_IMAGE_FORMAT_YUV420_3P;
		break;
	default:
		vlog_error("invald format(%d)\n", a.format);
		return -EINVAL;
	}

	src_map = __vppu_io_fd_map(ch, a.src_fd, &src_iova);
	if (IS_ERR_OR_NULL(src_map)) {
		vlog_error("src_fd(%d) __vppu_io_fd_map failed\n", a.src_fd);
		return -1;
	}

	dst_map = __vppu_io_fd_map(ch, a.dst_fd, &dst_iova);
	if (IS_ERR_OR_NULL(dst_map)) {
		vlog_error("dst_fd(%d) __vppu_io_fd_map failed\n", a.dst_fd);
		__vppu_io_fd_unmap(ch, src_map);
		return -1;
	}

	if (vppu_rotate(ch->vppu_ch, src_iova, dst_iova, a.width, a.height, a.angle, vppu_image_format) !=  VPPU_OK) {
		vlog_error("vppu_rotate failed\n");
		ret = -1;
	}
	else
		ret = 0;

	__vppu_io_fd_unmap(ch, src_map);
	__vppu_io_fd_unmap(ch, dst_map);

	return ret;
}

int _vppu_io_mirror(struct vcodec_channel *ch, unsigned long arg)
{
	return 0;
}

static int (*vppu_io_fp[])(struct vcodec_channel *ch, unsigned long arg) =
{
	_vppu_io_open,
	_vppu_io_close,
	_vppu_io_rotate,
	_vppu_io_mirror,
};

int vppu_do_ioctl(struct vcodec_channel *ch, unsigned long cmd, unsigned long arg)
{
	int errno = 0;
	unsigned long nr = _IOC_NR(cmd);

//	vlog_info("IOCTL cmd received. cmd : %lu(%lu)\n", cmd, nr);

	if (nr < sizeof(vppu_io_fp)/sizeof(int*) && vppu_io_fp[nr] != NULL)
		errno = vppu_io_fp[nr](ch, arg);
	else {
		vlog_error("unknwon IOCTL cmd(cmd : %d)\n", (int)cmd);
		errno = -EPERM;
	}

	return errno;
}

int vcodec_io_open(struct inode *inode, struct file *file)
{
	struct vcodec_channel *vcodec_ch;

	vlog_trace("start\n");

	vcodec_ch = (struct vcodec_channel *)vzalloc(sizeof(struct vcodec_channel));
	if (vcodec_ch == NULL) {
		pr_err("[vdec] vzalloc failed(%s)", __func__);
		return -ENOMEM;
	}

	vcodec_ch->use = VCODEC_FOR_NONE;

	vlog_info("success; file:0x%X, ch:0x%X, vcodec_open_cnt = %d\n", (unsigned int)file, (unsigned int)vcodec_ch, vcodec_open_cnt);

	if (vcodec_open_cnt == 0) {
		wake_lock_init(&vcodec_wake_lock, WAKE_LOCK_SUSPEND, VCODEC_WAKE_LOCK_NAME);
		wake_lock(&vcodec_wake_lock);
	}
	vcodec_open_cnt++;

#if 0
	spin_lock(&vcodec_desc->lock_channel_head);
	list_add(&vcodec_ch->list, &vcodec_desc->vcodec_channel_head);
	spin_unlock(&vcodec_desc->lock_channel_head);
#endif

	file->private_data = vcodec_ch;

	return 0;
}

int vcodec_io_close(struct inode *inode, struct file *file)
{
	int errno = 0;
	struct vcodec_channel *vcodec_ch = (struct vcodec_channel *)file->private_data;;

	vlog_trace("start\n");

	switch (vcodec_ch->use) {
	case VCODEC_FOR_DEC :
 		errno = _vdec_io_do_ioctl(vcodec_ch, VDEC_IO_CLOSE, 0);
		break;
	case VCODEC_FOR_ENC :
 		errno = _venc_io_do_ioctl(vcodec_ch, VENC_IO_CLOSE, 0);
		break;
	case VCODEC_FOR_PPU :
 		errno = vppu_do_ioctl(vcodec_ch, VPPU_IO_CLOSE, 0);
		break;
	case VCODEC_FOR_NONE :
		errno = 0;
		vlog_error("vcodec not used\n");
		break;
	default :
		errno = -EINVAL;
		vlog_error("vcodec use:0x%x\n", vcodec_ch->use);
		break;
	}

#if 0
	spin_lock(&vcodec_desc->lock_channel_head);
	list_del(&vcodec_ch->list);
	spin_unlock(&vcodec_desc->lock_channel_head);
#endif

	if (errno < 0) {
		vlog_error("vxxx_io_close() failed\n");
		return errno;
	}

	vcodec_open_cnt--;
	if (vcodec_open_cnt == 0) {
		wake_unlock(&vcodec_wake_lock);
		wake_lock_destroy(&vcodec_wake_lock);
	}

	vlog_info("success; file:0x%X, ch:0x%X, vcodec_open_cnt = %d\n", (unsigned int)file, (unsigned int)vcodec_ch, vcodec_open_cnt);

	file->private_data = NULL;
	vfree( vcodec_ch );

	return 0;
}

long vcodec_io_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct vcodec_channel *vcodec_ch = (struct vcodec_channel *)file->private_data;
	long errno = -EINVAL;

	if (vcodec_ch->use == VCODEC_FOR_NONE) {
		if (_IOC_TYPE(cmd) == VDEC_IOCTL_MAGIC)
			vcodec_ch->use = VCODEC_FOR_DEC;
		else if (_IOC_TYPE(cmd) == VENC_IOCTL_MAGIC)
			vcodec_ch->use = VCODEC_FOR_ENC;
		else if (_IOC_TYPE(cmd) == VPPU_IOCTL_MAGIC)
			vcodec_ch->use = VCODEC_FOR_PPU;
		else
			vlog_error("invalid ioctl type:0x%x\n", cmd);
	}

	switch (vcodec_ch->use) {
	case VCODEC_FOR_DEC :
		if (_IOC_TYPE(cmd) == VDEC_IOCTL_MAGIC)
	 		errno = _vdec_io_do_ioctl(vcodec_ch, cmd, arg);
		else
			vlog_error("invalid ioctl dec type:0x%x\n", cmd);
		break;

	case VCODEC_FOR_ENC :
		if (_IOC_TYPE(cmd) == VENC_IOCTL_MAGIC)
			errno = _venc_io_do_ioctl(vcodec_ch, cmd, arg);
		else
			vlog_error("invalid ioctl enc type:0x%x\n", cmd);
		break;

	case VCODEC_FOR_PPU :
		if (_IOC_TYPE(cmd) == VPPU_IOCTL_MAGIC)
	 		errno = vppu_do_ioctl(vcodec_ch, cmd, arg);
		else
			vlog_error("invalid ioctl ppu type:0x%x\n", cmd);
		break;

	default :
		vlog_error("vcodec use:0x%x\n", vcodec_ch->use);
		break;
	}

	if (errno < 0) {
		vlog_error("vxxx_do_ioctl() failed, cmd;0x%X\n", cmd);
	}

	return errno;
}

