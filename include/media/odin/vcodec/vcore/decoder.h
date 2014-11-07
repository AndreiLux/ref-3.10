/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
 * Seokhoon Kang <m4seokhoon.kang@lgepartner.com>
 * Inpyo Cho <inpyo.cho@lge.com>
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

#ifndef _VCORE_DECODER_H_
#define _VCORE_DECODER_H_

#define	VDEC_MAX_NUM_OF_DPB			31

#define	VCORE_TRUE			0
#define	VCORE_FALSE			(-1)

typedef int vcore_bool_t;

enum vcore_dec_ret
{
	VCORE_DEC_FAIL = -1,
	VCORE_DEC_SUCCESS = 0,
	VCORE_DEC_RETRY,
};

enum vcore_dec_codec
{
	VCORE_DEC_AVC,
	VCORE_DEC_MPEG4,
	VCORE_DEC_H263,
	VCORE_DEC_VP8,
	VCORE_DEC_MPEG2,
	VCORE_DEC_DIVX3,
	VCORE_DEC_DIVX,	/* DIVX4,5,6 */
	VCORE_DEC_SORENSON,
	VCORE_DEC_VC1,
	VCORE_DEC_AVS,
	VCORE_DEC_MVC,
	VCORE_DEC_THO,
	VCORE_DEC_JPEG,
	VCORE_DEC_PNG,
};

enum vcore_dec_pixel_format
{
	VCORE_DEC_FORMAT_420,
	VCORE_DEC_FORMAT_422,
	VCORE_DEC_FORMAT_224,
	VCORE_DEC_FORMAT_444,
	VCORE_DEC_FORMAT_400
};

enum vcore_dec_rotate
{
	VCORE_DEC_ROTATE_0,
	VCORE_DEC_ROTATE_90,
	VCORE_DEC_ROTATE_180,
	VCORE_DEC_ROTATE_270
};

struct vcore_dec_init_dpb
{
	struct
	{
		unsigned int paddr;
		unsigned int size;
	} internal_once_buf;
};
struct vcore_dec_dpb
{
	unsigned int sid;
	unsigned int buf_width;	/* stride */
	unsigned int buf_height;

	unsigned int external_dbp_addr;
	struct
	{
		unsigned int paddr;
		unsigned char *vaddr;
		unsigned int size;
	} internal_each_buf;
};

enum vcore_dec_au_type{
	VCORE_DEC_AU_SEQUENCE = 0x0,
	VCORE_DEC_AU_PICTURE,
};

struct vcore_dec_au_buf
{
	enum vcore_dec_au_type au_type;
	unsigned int start_phy_addr;
	unsigned char *start_vir_ptr;
	unsigned int size;				/* = buf_size + (padding) */
	unsigned int end_phy_addr;
};

struct vcore_dec_au_meta
{
	unsigned long long timestamp;
	unsigned int uid;
	unsigned long input_time;
	unsigned long feed_time;
	unsigned int flush_age;
	unsigned int chunk_id;
	enum vcore_dec_rotate rotation_angle;
};

struct vcore_dec_au
{
	vcore_bool_t eos;
	struct vcore_dec_au_buf buf;
	struct vcore_dec_au_meta meta;
};

enum vocre_dec_pic_type
{
	VCORE_DEC_PIC_I,
	VCORE_DEC_PIC_P,
	VCORE_DEC_PIC_B,
};

struct vcore_dec_report
{
	enum {
		VCORE_DEC_RESET,
		VCORE_DEC_DONE,
		VCORE_DEC_FEED,
	} hdr;

	union {
		enum {
			VCORE_DEC_REPORT_RESET_START,
			VCORE_DEC_REPORT_RESET_END,
		} reset;

		struct {
			vcore_bool_t		vcore_complete;		/* idle/busy */
			vcore_bool_t		need_more_au;
			vcore_bool_t		remaining_data_in_es;

			enum {
				VCORE_DEC_REPORT_NONE,
				VCORE_DEC_REPORT_SEQ,
				VCORE_DEC_REPORT_SEQ_CHANGE,
				VCORE_DEC_REPORT_PIC,
				VCORE_DEC_REPORT_EOS,
				VCORE_DEC_REPORT_FLUSH_DONE,
			} hdr;

			union
			{
				struct {
					vcore_bool_t success;
					vcore_bool_t over_spec;
					enum vcore_dec_pixel_format format;
					unsigned int buf_width;	/* stride */
					unsigned int buf_height;
					unsigned int pic_width;
					unsigned int pic_height;

					struct
					{
						unsigned int once_buf_size;
						unsigned int each_buf_size;
					} internal_buf_size;
					unsigned int sid;
					unsigned int ref_frame_cnt;
					unsigned int profile;
					unsigned int level;
					struct
					{
						unsigned int residual;
						unsigned int divider;
					} frame_rate;
					struct
					{
						unsigned int left;
						unsigned int right;
						unsigned int top;
						unsigned int bottom;
					} crop;

					struct vcore_dec_au_meta meta;
				} seq;

				struct {
					vcore_bool_t success;
					unsigned int pic_width;
					unsigned int pic_height;
					unsigned int sid;
					int rotation_angle;
					unsigned int aspect_ratio;
					unsigned int err_mbs;		/* percentage */
					enum vocre_dec_pic_type pic_type;
					unsigned int dpb_addr;
					struct
					{
						unsigned int left;
						unsigned int right;
						unsigned int top;
						unsigned int bottom;
					} crop;
					struct
					{
						unsigned int residual;
						unsigned int divider;
					} frame_rate;

					unsigned long long timestamp_dts;
					struct vcore_dec_au_meta meta;
				} pic;
			} info;
		} done;

		struct {
			/* none */
		} feed;
	} info;
};

enum vcore_dec_skipmode
{
	VCORE_DEC_SKIP_NO,
	VCORE_DEC_SKIP_B,
	VCORE_DEC_SKIP_P,
	VCORE_DEC_SKIP_B_P,
};

struct vcore_dec_ops
{
	enum vcore_dec_ret (*open)(void **vcore_id,
			enum vcore_dec_codec codec_type,
			unsigned int cpb_phy_addr, unsigned char *cpb_vir_ptr,
			unsigned int cpb_size,
			vcore_bool_t reordering,
			vcore_bool_t rendering_dpb,
			vcore_bool_t secure_buf,
			void *vdc_id,
			void (*vcore_dec_report)(void *vdc_id,
						struct vcore_dec_report *vcore_report));

	enum vcore_dec_ret (*close)(void *vcore_id);

	enum vcore_dec_ret (*init_dpb)(void *vcore_id,
								struct vcore_dec_init_dpb *arg);
	enum vcore_dec_ret (*register_dpb)(void *vcore_id,
								struct vcore_dec_dpb *arg);
	enum vcore_dec_ret (*clear_dpb)(void *vcore_id, unsigned int dpb_addr, unsigned int sid);
	enum vcore_dec_ret (*update_buffer)(void *vcore_id,
							struct vcore_dec_au *au, vcore_bool_t *running);

	enum vcore_dec_ret (*flush)(void *vcore_id, unsigned int rd_addr);

	void (*reset)(void *vcore_id);
};
#endif /* #ifndef _VCORE_DECODER_H_ */

