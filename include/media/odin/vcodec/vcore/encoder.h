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

#ifndef _VCORE_ENCODER_H_
#define _VCORE_ENCODER_H_

#define	VCORE_ENCODER_DEFAULT_RUNNING_WEIGHT		(1)

#define	VENC_MAX_NUM_OF_DEVICE		4
#define	VENC_MAX_NUM_OF_DPB			32

#define	VCORE_TRUE			0
#define	VCORE_FALSE			(-1)

typedef int vcore_bool_t;

enum vcore_enc_ret
{
	VCORE_ENC_FAIL = -1,
	VCORE_ENC_SUCCESS = 0,
	VCORE_ENC_RETRY,
};

enum vcore_enc_codec
{
	VCORE_ENC_AVC,
	VCORE_ENC_MPEG4,
	VCORE_ENC_H263,
	VCORE_ENC_JPEG,
};

struct vcore_enc_fb
{
	unsigned int fb_phy_addr;
	unsigned int fb_size;
};

struct vcore_enc_dpb
{
	unsigned int buf_width;	/* stride */
	unsigned int buf_height;
	unsigned int num;
	unsigned int dpb_addr[VENC_MAX_NUM_OF_DPB];
	struct
	{
		unsigned int addr;
		unsigned int size;
	} scratch_buf;
};

enum vcore_enc_pic_type
{
	VCORE_ENC_SEQ_HDR,
	VCORE_ENC_PIC_I,
	VCORE_ENC_PIC_P,
	VCORE_ENC_PIC_B,
};

struct vcore_enc_report
{
	enum {
		VCORE_ENC_RESET,
		VCORE_ENC_DONE,
		VCORE_ENC_FEED,
	} hdr;

	union {
		enum {
			VCORE_ENC_REPORT_RESET_START,
			VCORE_ENC_REPORT_RESET_END,
		} reset;

		struct {
			enum {
				VCORE_ENC_REPORT_SEQ,
				VCORE_ENC_REPORT_PIC,
			} hdr;

			union {
				struct {
					unsigned int buf_width;
					unsigned int buf_height;
					unsigned int ref_frame_size;
					unsigned int ref_frame_cnt;
					unsigned int scratch_buf_size;
				} seq;

				struct {
					vcore_bool_t success;
					enum vcore_enc_pic_type pic_type;
					unsigned int start_phy_addr;
					unsigned int size;
					unsigned int end_phy_addr;
				} pic;
			} info;
		} done;

		struct {
			// none
		} feed;

	} info;
};

enum vcore_enc_pixel_format
{
	VCORE_ENC_FORMAT_420,
	VCORE_ENC_FORMAT_422,
	VCORE_ENC_FORMAT_224,
	VCORE_ENC_FORMAT_444,
	VCORE_ENC_FORMAT_400
};

struct vcore_enc_running_config
{
	enum {
		VCORE_ENC_RUNNING_PARAM_BITRATE = 0,
		VCORE_ENC_RUNNING_PARAM_INTRA_REFRESH,
	}index;

	union {
		unsigned int bit_rate_kbps;
		unsigned int intra_refresh;
	}data;
};

struct vcore_enc_config
{
	enum vcore_enc_codec codec_type;
	unsigned int pic_width;
	unsigned int pic_height;
	unsigned int buf_width;
	unsigned int buf_height;

	union {

		struct {
			unsigned int gop_size;
			unsigned int mb_num_per_slice;
			unsigned int cabac_init_idx;
			enum {
				VCORE_ENC_LOOP_FILTER_ENABLE = 0,
				VCORE_ENC_LOOP_FILTER_DISABLE,
				VCORE_ENC_LOOP_FILTER_DISABLE_SLICE_BOUNDARY,
			}loop_filter_mode;
			vcore_bool_t const_intra_pred_enable;

			enum {
				VCORE_ENC_H264_PROFILE_BASE = 0,
				VCORE_ENC_H264_PROFILE_MAIN,
				VCORE_ENC_H264_PROFILE_HIGH,
			} profile;
			unsigned int level_idc;

			vcore_bool_t field_enable;
			vcore_bool_t cabac_enable;
		}h264;

		struct {
			unsigned int gop_size;
			enum {
				VCORE_ENC_H263_PROFILE_BASE = 0,
				VCORE_ENC_H263_PROFILE_SWV2,
			}profile;
		}h263;

		struct {
			unsigned int gop_size;
			vcore_bool_t short_video_header_enable;
			unsigned int intra_dc_vlc_threshold;
		}mpeg4;
	}codec_param;

	struct {
		enum {
			VCORE_ENC_RATE_CONTROL_DISABLE = 0,
			VCORE_ENC_RATE_CONTROL_CBR,
			VCORE_ENC_RATE_CONTROL_ABR,
		}control_mode;
		vcore_bool_t auto_skip_enable;
		unsigned int target_kbps;
	}bit_rate;

	struct {
		vcore_bool_t hec_enable;
		vcore_bool_t data_partitioning_enable;
		vcore_bool_t reversible_vlc_enble;
	}error_correction;

	struct {
		vcore_bool_t enable;
		unsigned int mb_num;
	}cyclic_intra_block_refresh;

	struct {
		unsigned int residual;
		unsigned int divider;
	} frame_rate;

	enum {
		VCORE_ENC_CBCR_INTERLEAVE_NONE = 0,
		VCORE_ENC_CBCR_INTERLEAVE_NV12,
		VCORE_ENC_CBCR_INTERLEAVE_NV21,
	} cbcr_interleave;

	enum vcore_enc_pixel_format format;

	unsigned int epb_phy_addr;
	unsigned char *epb_vir_ptr;
	unsigned int epb_size;
};

struct vcore_enc_ops
{
	enum vcore_enc_ret (*open)(void **vcore_id,
					struct vcore_enc_config *enc_config,
					unsigned int workbuf_paddr, unsigned long *workbuf_vaddr,
					unsigned int workbuf_size, void *vec_id,
					void (*vcore_enc_report)(void *vec_id,
							struct vcore_enc_report *vcore_report));
	enum vcore_enc_ret (*close)(void *vcore_id);
	enum vcore_enc_ret (*register_dpb)(void *vcore_id,
									struct vcore_enc_dpb *dpb);
	enum vcore_enc_ret (*update_buffer)(void *vcore_id,
									struct vcore_enc_fb *fb);
	enum vcore_enc_ret (*update_epb_rdaddr)(void *vcore_id, unsigned int rd_addr);
	enum vcore_enc_ret (*set_config)(void *vcore_id,
					struct vcore_enc_running_config *config);

	void (*reset)(void *vcore_id);
};
#endif /* #ifndef _VCORE_ENCODER_H_ */

