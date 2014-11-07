/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Sungwon Son <sungwon.son@lge.com>
 * Youngki Lyu <youngki.lyu@lge.com>
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

/*------------------------------------------------------------------------------
  File Incluseions
  ------------------------------------------------------------------------------*/

#include <linux/types.h>
#include <linux/io.h>
#include <linux/printk.h>

#include "du_hal.h"
#include "sync_hal.h"
#include "ovl_hal.h"
#include "reg_def/gra0_union_reg.h"
#include "reg_def/vid0_union_reg.h"
#include "reg_def/gscl_union_reg.h"
#include "reg_def/ovl_union_reg.h"

#define _ovl_hal_verify_ovl_id(ovl_idx)	((ovl_idx) < OVL_CH_NUM ? true : false)
#define _ovl_hal_verify_ovl_ch(ch)	((ch) < DU_SRC_NUM ? true : false)
#define UCHAR_MAX_SIZE	0xFF

volatile DSS_DU_OVL_REG_T *ovlbase;

/* ovl status function */
unsigned int ovl_hal_empty_data_fifo_status(enum ovl_ch_id ovl_idx, enum ovl_zorder src_index)
{
	unsigned int ret = 0xffffffff;

	if (_ovl_hal_verify_ovl_id(ovl_idx) == false) {
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return ret;
	}

	switch (ovl_idx) {
	case OVL_CH_ID0:
		switch (src_index) {
		case OVL_ZORDER_0:
			ret = ovlbase->ovl0_status.af.ovl0_s0_ififo_empty;
			break;
		case OVL_ZORDER_1:
			ret = ovlbase->ovl0_status.af.ovl0_s1_ififo_empty;
			break;
		case OVL_ZORDER_2:
			ret = ovlbase->ovl0_status.af.ovl0_s2_ififo_empty;
			break;
		case OVL_ZORDER_3:
			ret = ovlbase->ovl0_status.af.ovl0_s3_ififo_empty;
			break;
		case OVL_ZORDER_4:
			ret = ovlbase->ovl0_status.af.ovl0_s4_ififo_empty;
			break;
		case OVL_ZORDER_5:
			ret = ovlbase->ovl0_status.af.ovl0_s5_ififo_empty;
			break;
		case OVL_ZORDER_6:
			ret = ovlbase->ovl0_status.af.ovl0_s6_ififo_empty;
			break;
		default :
			pr_err("invalid src_index(%d)\n", src_index);
			return ret;
		}
		break;
	case OVL_CH_ID1:
		switch (src_index) {
		case OVL_ZORDER_0:
			ret = ovlbase->ovl1_status.af.ovl1_s0_ififo_empty;
			break;
		case OVL_ZORDER_1:
			ret = ovlbase->ovl1_status.af.ovl1_s1_ififo_empty;
			break;
		case OVL_ZORDER_2:
			ret = ovlbase->ovl1_status.af.ovl1_s2_ififo_empty;
			break;
		case OVL_ZORDER_3:
			ret = ovlbase->ovl1_status.af.ovl1_s3_ififo_empty;
			break;
		case OVL_ZORDER_4:
			ret = ovlbase->ovl1_status.af.ovl1_s4_ififo_empty;
			break;
		case OVL_ZORDER_5:
			ret = ovlbase->ovl1_status.af.ovl1_s5_ififo_empty;
			break;
		case OVL_ZORDER_6:
			ret = ovlbase->ovl1_status.af.ovl1_s6_ififo_empty;
			break;
		default :
			pr_err("invalid src_index(%d)\n", src_index);
			return ret;
		}
		break;
	case OVL_CH_ID2:
		switch (src_index) {
		case OVL_ZORDER_0:
			ret = ovlbase->ovl2_status.af.ovl2_s0_ififo_empty;
			break;
		case OVL_ZORDER_1:
			ret = ovlbase->ovl2_status.af.ovl2_s1_ififo_empty;
			break;
		case OVL_ZORDER_2:
			ret = ovlbase->ovl2_status.af.ovl2_s2_ififo_empty;
			break;
		case OVL_ZORDER_3:
			ret = ovlbase->ovl2_status.af.ovl2_s3_ififo_empty;
			break;
		case OVL_ZORDER_4:
			ret = ovlbase->ovl2_status.af.ovl2_s4_ififo_empty;
			break;
		case OVL_ZORDER_5:
			ret = ovlbase->ovl2_status.af.ovl2_s5_ififo_empty;
			break;
		case OVL_ZORDER_6:
			ret = ovlbase->ovl2_status.af.ovl2_s6_ififo_empty;
			break;
		default :
			pr_err("invalid src_index(%d)\n", src_index);
			return ret;
		}
		break;
	default :
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return ret;
	}

	return ret;
}

unsigned int ovl_hal_full_data_fifo_status(enum ovl_ch_id ovl_idx, enum ovl_zorder src_index)
{
	unsigned int ret = 0xffffffff;

	if (_ovl_hal_verify_ovl_id(ovl_idx) == false) {
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return ret;
	}

	switch (ovl_idx) {
	case OVL_CH_ID0:
		switch (src_index) {
		case OVL_ZORDER_0:
			ret = ovlbase->ovl0_status.af.ovl0_s0_ififo_full;
			break;
		case OVL_ZORDER_1:
			ret = ovlbase->ovl0_status.af.ovl0_s1_ififo_full;
			break;
		case OVL_ZORDER_2:
			ret = ovlbase->ovl0_status.af.ovl0_s2_ififo_full;
			break;
		case OVL_ZORDER_3:
			ret = ovlbase->ovl0_status.af.ovl0_s3_ififo_full;
			break;
		case OVL_ZORDER_4:
			ret = ovlbase->ovl0_status.af.ovl0_s4_ififo_full;
			break;
		case OVL_ZORDER_5:
			ret = ovlbase->ovl0_status.af.ovl0_s5_ififo_full;
			break;
		case OVL_ZORDER_6:
			ret = ovlbase->ovl0_status.af.ovl0_s6_ififo_full;
			break;
		default :
			pr_err("invalid src_index(%d)\n", src_index);
			return ret;
		}
		break;
	case OVL_CH_ID1:
		switch (src_index) {
		case OVL_ZORDER_0:
			ret = ovlbase->ovl1_status.af.ovl1_s0_ififo_full;
			break;
		case OVL_ZORDER_1:
			ret = ovlbase->ovl1_status.af.ovl1_s1_ififo_full;
			break;
		case OVL_ZORDER_2:
			ret = ovlbase->ovl1_status.af.ovl1_s2_ififo_full;
			break;
		case OVL_ZORDER_3:
			ret = ovlbase->ovl1_status.af.ovl1_s3_ififo_full;
			break;
		case OVL_ZORDER_4:
			ret = ovlbase->ovl1_status.af.ovl1_s4_ififo_full;
			break;
		case OVL_ZORDER_5:
			ret = ovlbase->ovl1_status.af.ovl1_s5_ififo_full;
			break;
		case OVL_ZORDER_6:
			ret = ovlbase->ovl1_status.af.ovl1_s6_ififo_full;
			break;
		default :
			pr_err("invalid src_index(%d)\n", src_index);
			return ret;
		}
		break;
	case OVL_CH_ID2:
		switch (src_index) {
		case OVL_ZORDER_0:
			ret = ovlbase->ovl2_status.af.ovl2_s0_ififo_full;
			break;
		case OVL_ZORDER_1:
			ret = ovlbase->ovl2_status.af.ovl2_s1_ififo_full;
			break;
		case OVL_ZORDER_2:
			ret = ovlbase->ovl2_status.af.ovl2_s2_ififo_full;
			break;
		case OVL_ZORDER_3:
			ret = ovlbase->ovl2_status.af.ovl2_s3_ififo_full;
			break;
		case OVL_ZORDER_4:
			ret = ovlbase->ovl2_status.af.ovl2_s4_ififo_full;
			break;
		case OVL_ZORDER_5:
			ret = ovlbase->ovl2_status.af.ovl2_s5_ififo_full;
			break;
		case OVL_ZORDER_6:
			ret = ovlbase->ovl2_status.af.ovl2_s6_ififo_full;
			break;
		default :
			pr_err("invalid src_index(%d)\n", src_index);
			return ret;
		}
		break;
	default :
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return ret;
	}
	return ret;
}

unsigned int ovl_hal_con_st_status(enum ovl_ch_id ovl_idx)
{
	unsigned int ret = 0xffffffff;

	if (_ovl_hal_verify_ovl_id(ovl_idx) == false) {
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return ret;
	}

	switch (ovl_idx) {
	case OVL_CH_ID0:
		ret = ovlbase->ovl0_status.af.ovl0_con_st;
		break;
	case OVL_CH_ID1:
		ret = ovlbase->ovl1_status.af.ovl1_con_st;
		break;
	case OVL_CH_ID2:
		ret = ovlbase->ovl2_status.af.ovl2_con_st;
		break;
	default :
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return ret;
	}

	return ret;
}

bool ovl_hal_channel_set (enum ovl_ch_id ovl_idx, enum du_src_ch_id ch,
		struct dss_image_pos ovl_image_pos, struct dss_image_size ovl_pip_size,
		enum ovl_zorder zorder, enum ovl_operation ops,
		bool ovl_alpha_enable, unsigned char alpha_value, bool pre_mul_enable,
		bool chromakey_enable,
		unsigned char chroma_data_r, unsigned char chroma_data_g, unsigned char chroma_data_b,
		unsigned char chroma_mask_r, unsigned char chroma_mask_g, unsigned char chroma_mask_b,
		bool se_src_secure, bool se_src_sel_secure, bool se_src_layer_secure)
{

	if (_ovl_hal_verify_ovl_id(ovl_idx) == false) {
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return false;
	}

	if (ovl_image_pos.x > 1080 || ovl_image_pos.y > 1920) {
		pr_err("invalid ch position to ovl_hal_channel_set, size x(%d) y(%d)\n", ovl_image_pos.x, ovl_image_pos.y);
		return false;
	}

	if (ovl_pip_size.w > 1080 || ovl_pip_size.h > 1920) {
		pr_err("invalid ch pip_size to ovl_hal_channel_set, size w(%d) h(%d)\n", ovl_pip_size.w, ovl_pip_size.h);
		return false;
	}

	if (ch >= DU_SRC_NUM) {
		pr_err("invalid ch(%d) of ovl_hal_channel_set()\n", ch);
		return false;
	}

	if (zorder >= OVL_ZORDER_NR) {
		pr_err("invalid zorder(%d) ovl_hal_channel_set\n", zorder);
		return false;
	}

	if (ops > OVL_OPS_XOR) {
		pr_err("invalid ops(%d) ovl_hal_channel_set\n", ops);
		return false;
	}

	if (alpha_value > UCHAR_MAX_SIZE) {
		pr_err("invalid alpha_value(%d) ovl_hal_channel_set\n", alpha_value);
		return false;
	}

	if (chroma_data_r > UCHAR_MAX_SIZE || chroma_data_g > UCHAR_MAX_SIZE || chroma_data_b > UCHAR_MAX_SIZE) {
		pr_err("invalid chroma_data_r(%d) chroma_data_r(%d) chroma_data_r(%d) ovl_hal_channel_set\n",
			chroma_data_r, chroma_data_g, chroma_data_b);
		return false;
	}

	if (chroma_mask_r > UCHAR_MAX_SIZE || chroma_mask_g > UCHAR_MAX_SIZE || chroma_mask_b > UCHAR_MAX_SIZE) {
		pr_err("invalid chroma_mask_r(%d) chroma_mask_g(%d) chroma_mask_b(%d) ovl_hal_channel_set\n",
			chroma_mask_r, chroma_mask_g, chroma_mask_b);
		return false;
	}

	vid0_rd_sizexy vid_sizexy;
	vid0_rd_pip_sizexy vid_pip_sizexy;
	gra0_rd_sizexy gra_sizexy;
	gscl_rd_sizexy gscl_sizexy;

	switch (ch) {
	case DU_SRC_VID0:
	case DU_SRC_VID1:
		vid_pip_sizexy.a32 = du_hal_get_image_size(ch);
		if (vid_sizexy.a32 < 0) {
			pr_err("invalid vid_pip_sizexy of ovl_hal_channel_set()\n");
			return false;
		} else if (vid_pip_sizexy.af.src_pip_sizex != ovl_pip_size.w || vid_pip_sizexy.af.src_pip_sizey != ovl_pip_size.h) {
			pr_err("not equal vid_pip_sizexy image size ch(%d) : x(%d), y(%d) ovl_pip : w(%d), h(%d)",
				ch, vid_pip_sizexy.af.src_pip_sizex, vid_pip_sizexy.af.src_pip_sizey, ovl_pip_size.w, ovl_pip_size.h);
			return false;
		}
		break;
	case DU_SRC_VID2:
		vid_sizexy.a32 = du_hal_get_image_size(ch);
		if (vid_sizexy.a32 < 0) {
			pr_err("invalid vid_sizexy of ovl_hal_channel_set()\n");
			return false;
		} else if (vid_sizexy.af.src_sizex != ovl_pip_size.w || vid_sizexy.af.src_sizey != ovl_pip_size.h) {
			pr_err("not equal vid_sizexy image size ch(%d) : x(%d), y(%d) ovl_pip : w(%d), h(%d)",
				ch, vid_sizexy.af.src_sizex, vid_sizexy.af.src_sizey, ovl_pip_size.w, ovl_pip_size.h);
			return false;
		}
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
		gra_sizexy.a32 = du_hal_get_image_size(ch);
		if (gra_sizexy.a32 < 0) {
			pr_err("invalid gra_sizexy of ovl_hal_channel_set()\n");
			return false;
		} else if (gra_sizexy.af.src_sizex != ovl_pip_size.w || gra_sizexy.af.src_sizey != ovl_pip_size.h) {
			pr_err("not equal gra_sizexy image size ch(%d) : x(%d), y(%d) ovl_pip : w(%d), h(%d)",
				ch, gra_sizexy.af.src_sizex, gra_sizexy.af.src_sizey, ovl_pip_size.w, ovl_pip_size.h);
			return false;
		}
	case DU_SRC_GSCL:
		gscl_sizexy.a32 = du_hal_get_image_size(ch);
		if (gscl_sizexy.a32 < 0) {
			pr_err("invalid gscl_sizexy of ovl_hal_channel_set()\n");
			return false;
		} else if (gscl_sizexy.af.src_sizex != ovl_pip_size.w || gra_sizexy.af.src_sizey != ovl_pip_size.h) {
			pr_err("not equal gscl_sizexy image size ch(%d) : x(%d), y(%d) ovl_pip : w(%d), h(%d)",
				ch, gra_sizexy.af.src_sizex, gra_sizexy.af.src_sizey, ovl_pip_size.w, ovl_pip_size.h);
			return false;
		}
		break;
	default:
		pr_err("invalid ch(%d) of ovl_hal_channel_set()\n",ch);
		return false;
	}

	if (ovl_idx == OVL_CH_ID0) {
		volatile ovl0_cfg0 ovl0_cfg0 = ovlbase->ovl0_cfg0;
		volatile ovl0_cfg1 ovl0_cfg1 = ovlbase->ovl0_cfg1;
		volatile ovl0_cfg2 ovl0_cfg2 = ovlbase->ovl0_cfg2;

		switch (zorder) {
		case OVL_ZORDER_6:
			ovl0_cfg0.af.ovl0_layer5_cop = ops;
			ovl0_cfg1.af.ovl0_layer5_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl0_cfg1.af.ovl0_layer5_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl0_cfg2.af.ovl0_src6_sel = ch;
			ovl0_cfg2.af.ovl0_src6_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl0_cfg0 = ovl0_cfg0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_l5_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl0_l5_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl0_alpha_reg1.af.ovl0_src6_alpha = alpha_value;
			break;

		case OVL_ZORDER_5:
			ovl0_cfg0.af.ovl0_layer4_cop = ops;
			ovl0_cfg1.af.ovl0_layer4_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl0_cfg1.af.ovl0_layer5_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl0_cfg1.af.ovl0_layer4_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl0_cfg2.af.ovl0_src5_sel = ch;
			ovl0_cfg2.af.ovl0_src5_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl0_cfg0 = ovl0_cfg0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_l4_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl0_l4_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl0_alpha_reg1.af.ovl0_src5_alpha = alpha_value;
			break;
		case OVL_ZORDER_4:
			ovl0_cfg0.af.ovl0_layer3_cop = ops;
			ovl0_cfg1.af.ovl0_layer3_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl0_cfg1.af.ovl0_layer4_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl0_cfg1.af.ovl0_layer3_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl0_cfg2.af.ovl0_src4_sel = ch;
			ovl0_cfg2.af.ovl0_src4_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl0_cfg0 = ovl0_cfg0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_l3_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl0_l3_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl0_alpha_reg1.af.ovl0_src4_alpha = alpha_value;
			break;
		case OVL_ZORDER_3:
			ovl0_cfg0.af.ovl0_layer2_cop = ops;
			ovl0_cfg1.af.ovl0_layer2_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl0_cfg1.af.ovl0_layer3_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl0_cfg1.af.ovl0_layer2_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
 			ovl0_cfg2.af.ovl0_src3_sel = ch;
			ovl0_cfg2.af.ovl0_src3_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl0_cfg0 = ovl0_cfg0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_l2_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl0_l2_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl0_alpha_reg0.af.ovl0_src3_alpha = alpha_value;
			break;
		case OVL_ZORDER_2:
			ovl0_cfg0.af.ovl0_layer1_cop = ops;
			ovl0_cfg1.af.ovl0_layer1_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl0_cfg1.af.ovl0_layer2_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl0_cfg1.af.ovl0_layer1_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
 			ovl0_cfg2.af.ovl0_src2_sel = ch;
			ovl0_cfg2.af.ovl0_src2_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl0_cfg0 = ovl0_cfg0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_l1_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl0_l1_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl0_alpha_reg0.af.ovl0_src2_alpha = alpha_value;
			break;
		case OVL_ZORDER_1:
			ovl0_cfg0.af.ovl0_layer0_cop = ops;
			ovl0_cfg1.af.ovl0_layer0_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl0_cfg1.af.ovl0_layer1_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl0_cfg1.af.ovl0_layer0_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
 			ovl0_cfg2.af.ovl0_src1_sel = ch;
			ovl0_cfg2.af.ovl0_src1_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl0_cfg0 = ovl0_cfg0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_l0_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl0_l0_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl0_alpha_reg0.af.ovl0_src1_alpha = alpha_value;
			break;
		case OVL_ZORDER_0:
			ovl0_cfg1.af.ovl0_layer0_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl0_cfg2.af.ovl0_src0_sel = ch;
			ovl0_cfg2.af.ovl0_src0_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2.a32 = ovl0_cfg2.a32;
			ovlbase->ovl0_alpha_reg0.af.ovl0_src0_alpha = alpha_value;
			break;
		default :
			pr_err("invalid zorder(%d)\n", zorder);
			return false;
		}
	}
	else if (ovl_idx == OVL_CH_ID1) {
		volatile ovl1_cfg0 ovl1_cfg0 = ovlbase->ovl1_cfg0;
		volatile ovl1_cfg1 ovl1_cfg1 = ovlbase->ovl1_cfg1;
		volatile ovl1_cfg2 ovl1_cfg2 = ovlbase->ovl1_cfg2;

		switch (zorder) {
		case OVL_ZORDER_6:
			ovl1_cfg0.af.ovl1_layer5_cop = ops;
			ovl1_cfg1.af.ovl1_layer5_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl1_cfg1.af.ovl1_layer5_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
 			ovl1_cfg2.af.ovl1_src6_sel = ch;
			ovl1_cfg2.af.ovl1_src6_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl1_cfg0 = ovl1_cfg0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_l5_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl1_l5_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl1_alpha_reg1.af.ovl1_src6_alpha = alpha_value;
			break;
		case OVL_ZORDER_5:
			ovl1_cfg0.af.ovl1_layer4_cop = ops;
			ovl1_cfg1.af.ovl1_layer4_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl1_cfg1.af.ovl1_layer5_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl1_cfg1.af.ovl1_layer4_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
 			ovl1_cfg2.af.ovl1_src5_sel = ch;
			ovl1_cfg2.af.ovl1_src5_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl1_cfg0 = ovl1_cfg0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_l4_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl1_l4_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl1_alpha_reg1.af.ovl1_src5_alpha = alpha_value;
			break;
		case OVL_ZORDER_4:
			ovl1_cfg0.af.ovl1_layer3_cop = ops;
			ovl1_cfg1.af.ovl1_layer3_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl1_cfg1.af.ovl1_layer4_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl1_cfg1.af.ovl1_layer3_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0; 
			ovl1_cfg2.af.ovl1_src4_sel = ch;
			ovl1_cfg2.af.ovl1_src4_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl1_cfg0 = ovl1_cfg0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_l3_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl1_l3_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl1_alpha_reg1.af.ovl1_src4_alpha = alpha_value;
			break;
		case OVL_ZORDER_3:
			ovl1_cfg0.af.ovl1_layer2_cop = ops;
			ovl1_cfg1.af.ovl1_layer2_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl1_cfg1.af.ovl1_layer3_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl1_cfg1.af.ovl1_layer2_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0; 
			ovl1_cfg2.af.ovl1_src3_sel = ch;
			ovl1_cfg2.af.ovl1_src3_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl1_cfg0 = ovl1_cfg0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_l2_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl1_l2_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl1_alpha_reg0.af.ovl1_src3_alpha = alpha_value;
			break;
		case OVL_ZORDER_2:
			ovl1_cfg0.af.ovl1_layer1_cop = ops;
			ovl1_cfg1.af.ovl1_layer1_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl1_cfg1.af.ovl1_layer2_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl1_cfg1.af.ovl1_layer1_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0; 
			ovl1_cfg2.af.ovl1_src2_sel = ch;
			ovl1_cfg2.af.ovl1_src2_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl1_cfg0 = ovl1_cfg0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_l1_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl1_l1_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl1_alpha_reg0.af.ovl1_src2_alpha = alpha_value;
			break;
		case OVL_ZORDER_1:
			ovl1_cfg0.af.ovl1_layer0_cop = ops;
			ovl1_cfg1.af.ovl1_layer0_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl1_cfg1.af.ovl1_layer1_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl1_cfg1.af.ovl1_layer0_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0; 
			ovl1_cfg2.af.ovl1_src1_sel = ch;
			ovl1_cfg2.af.ovl1_src1_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl1_cfg0 = ovl1_cfg0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_l0_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl1_l0_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl1_alpha_reg0.af.ovl1_src1_alpha = alpha_value;
			break;
		case OVL_ZORDER_0:
			ovl1_cfg1.af.ovl1_layer0_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;			
			ovl1_cfg2.af.ovl1_src0_sel = ch;
			ovl1_cfg2.af.ovl1_src0_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_alpha_reg0.af.ovl1_src0_alpha = alpha_value;
			break;
		default :
			pr_err("invalid zorder(%d)\n", zorder);
			return false;
		}
	}	
	else if (ovl_idx == OVL_CH_ID2) {
		volatile ovl2_cfg0 ovl2_cfg0 = ovlbase->ovl2_cfg0;
		volatile ovl2_cfg1 ovl2_cfg1 = ovlbase->ovl2_cfg1;
		volatile ovl2_cfg2 ovl2_cfg2 = ovlbase->ovl2_cfg2;

		switch (zorder) {
		case OVL_ZORDER_6:
			ovl2_cfg0.af.ovl2_layer5_cop = ops;
			ovl2_cfg1.af.ovl2_layer5_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl2_cfg1.af.ovl2_layer5_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;			
 			ovl2_cfg2.af.ovl2_src6_sel = ch;
			ovl2_cfg2.af.ovl2_src6_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl2_cfg0 = ovl2_cfg0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_l5_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl2_l5_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl2_alpha_reg1.af.ovl2_src6_alpha = alpha_value;
			break;
		case OVL_ZORDER_5:
			ovl2_cfg0.af.ovl2_layer4_cop = ops;
			ovl2_cfg1.af.ovl2_layer4_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl2_cfg1.af.ovl2_layer5_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl2_cfg1.af.ovl2_layer4_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
 			ovl2_cfg2.af.ovl2_src5_sel = ch;
			ovl2_cfg2.af.ovl2_src5_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl2_cfg0 = ovl2_cfg0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_l4_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl2_l4_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl2_alpha_reg1.af.ovl2_src5_alpha = alpha_value;
			break;
		case OVL_ZORDER_4:
			ovl2_cfg0.af.ovl2_layer3_cop = ops;
			ovl2_cfg1.af.ovl2_layer3_chromakey_en =( chromakey_enable == true) ? 1 : 0;
			ovl2_cfg1.af.ovl2_layer4_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl2_cfg1.af.ovl2_layer3_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
 			ovl2_cfg2.af.ovl2_src4_sel = ch;
			ovl2_cfg2.af.ovl2_src4_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl2_cfg0 = ovl2_cfg0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_l3_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl2_l3_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl2_alpha_reg1.af.ovl2_src4_alpha = alpha_value;
			break;
		case OVL_ZORDER_3:
			ovl2_cfg0.af.ovl2_layer2_cop = ops;
			ovl2_cfg1.af.ovl2_layer2_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl2_cfg1.af.ovl2_layer3_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl2_cfg1.af.ovl2_layer2_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
 			ovl2_cfg2.af.ovl2_src3_sel = ch;
			ovl2_cfg2.af.ovl2_src3_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl2_cfg0 = ovl2_cfg0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_l2_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl2_l2_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl2_alpha_reg0.af.ovl2_src3_alpha = alpha_value;
			break;
		case OVL_ZORDER_2:
			ovl2_cfg0.af.ovl2_layer1_cop = ops;
			ovl2_cfg1.af.ovl2_layer1_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl2_cfg1.af.ovl2_layer2_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl2_cfg1.af.ovl2_layer1_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
 			ovl2_cfg2.af.ovl2_src2_sel = ch;
			ovl2_cfg2.af.ovl2_src2_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl2_cfg0 = ovl2_cfg0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_l1_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl2_l1_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl2_alpha_reg0.af.ovl2_src2_alpha = alpha_value;
			break;
		case OVL_ZORDER_1:
			ovl2_cfg0.af.ovl2_layer0_cop = ops;
			ovl2_cfg1.af.ovl2_layer0_chromakey_en = (chromakey_enable == true) ? 1 : 0;
			ovl2_cfg1.af.ovl2_layer1_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl2_cfg1.af.ovl2_layer0_dst_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
 			ovl2_cfg2.af.ovl2_src1_sel = ch;
			ovl2_cfg2.af.ovl2_src1_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl2_cfg0 = ovl2_cfg0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_l0_ckey_value.a32 = (chroma_data_r << 16 | chroma_data_g << 8 | chroma_data_b);
			ovlbase->ovl2_l0_ckey_mask_value.a32 = (chroma_mask_r << 16 | chroma_mask_g << 8 | chroma_mask_b);
			ovlbase->ovl2_alpha_reg0.af.ovl2_src1_alpha = alpha_value;
			break;
		case OVL_ZORDER_0:
			ovl2_cfg1.af.ovl2_layer0_src_pre_mul_en = (pre_mul_enable == true) ? 1 : 0;
			ovl2_cfg2.af.ovl2_src0_sel = ch;
			ovl2_cfg2.af.ovl2_src0_alpha_reg_en = (ovl_alpha_enable == true) ? 1 : 0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;			
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_alpha_reg0.af.ovl2_src0_alpha = alpha_value;
			break;
		default :
			pr_err("invalid zorder(%d)\n", zorder);
			return false;
		}
	}
	else{
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return false;
	}
 
	switch (zorder) {
	case OVL_ZORDER_6:
 		ovlbase->i_s6_pip_startxy.a32 = (ovl_image_pos.y << 16 | ovl_image_pos.x);
		ovlbase->i_s6_pip_sizexy.a32 = (ovl_pip_size.h << 16 | ovl_pip_size.w);
		break;
	case OVL_ZORDER_5:
		ovlbase->i_s5_pip_startxy.a32 = (ovl_image_pos.y << 16 | ovl_image_pos.x);
		ovlbase->i_s5_pip_sizexy.a32 = (ovl_pip_size.h << 16 | ovl_pip_size.w);
		break;
	case OVL_ZORDER_4:
		ovlbase->i_s4_pip_startxy.a32 = (ovl_image_pos.y << 16 | ovl_image_pos.x);
		ovlbase->i_s4_pip_sizexy.a32 = (ovl_pip_size.h << 16 | ovl_pip_size.w);
		break;
	case OVL_ZORDER_3:
		ovlbase->i_s3_pip_startxy.a32 = (ovl_image_pos.y << 16 | ovl_image_pos.x);
		ovlbase->i_s3_pip_sizexy.a32 = (ovl_pip_size.h << 16 | ovl_pip_size.w);
		break;
	case OVL_ZORDER_2:
		ovlbase->i_s2_pip_startxy.a32 = (ovl_image_pos.y << 16 | ovl_image_pos.x);
		ovlbase->i_s2_pip_sizexy.a32 = (ovl_pip_size.h << 16 | ovl_pip_size.w);
		break;
	case OVL_ZORDER_1:
		ovlbase->i_s1_pip_startxy.a32 = (ovl_image_pos.y << 16 | ovl_image_pos.x);
		ovlbase->i_s1_pip_sizexy.a32 = (ovl_pip_size.h << 16 | ovl_pip_size.w);
		break;
	case OVL_ZORDER_0:
		ovlbase->i_s0_pip_startxy.a32 = (ovl_image_pos.y << 16 | ovl_image_pos.x);
		ovlbase->i_s0_pip_sizexy.a32 = (ovl_pip_size.h << 16 | ovl_pip_size.w);
		break;
	default :
		pr_err("invalid zorder(%d)\n", zorder);
		return false;
	}

	volatile ovl_cfg_g0 cfg_g0 = ovlbase->cfg_g0;
	volatile ovl_se_src_secure_enable se_src_secure_enable = ovlbase->se_src_secure_enable;	

	switch (ovl_idx) {
	case OVL_CH_ID0:
		switch (ch) { /* OVL_CFG_G0 */
		case DU_SRC_VID0:
			cfg_g0.af.ovl0_src0_en = 1;
			se_src_secure_enable.af.ovl0_se_src0_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl0_se_src0_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl0_se_layer0_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_VID1:
			cfg_g0.af.ovl0_src1_en = 1;
			se_src_secure_enable.af.ovl0_se_src1_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl0_se_src1_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl0_se_layer1_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_VID2:
			cfg_g0.af.ovl0_src2_en = 1;
			se_src_secure_enable.af.ovl0_se_src2_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl0_se_src2_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl0_se_layer2_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_GRA0:
			cfg_g0.af.ovl0_src3_en = 1;
			se_src_secure_enable.af.ovl0_se_src3_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl0_se_src3_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl0_se_layer3_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_GRA1:
			cfg_g0.af.ovl0_src4_en = 1;
			se_src_secure_enable.af.ovl0_se_src4_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl0_se_src4_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl0_se_layer4_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_GRA2:
			cfg_g0.af.ovl0_src5_en = 1;
			se_src_secure_enable.af.ovl0_se_src5_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl0_se_src5_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl0_se_layer5_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_GSCL:
			cfg_g0.af.ovl0_src6_en = 1;
			se_src_secure_enable.af.ovl0_se_src6_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl0_se_src6_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl0_se_layer6_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_NUM:
		default :
			pr_err("invalid ovl_idx(%d)\n", ovl_idx);
			return false;
		}
		break;	
	case OVL_CH_ID1:
		switch (ch) { /* OVL_CFG_G0 */
		case DU_SRC_VID0:
			cfg_g0.af.ovl1_src0_en = 1;
			se_src_secure_enable.af.ovl1_se_src0_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl1_se_src0_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl1_se_layer0_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_VID1:
			cfg_g0.af.ovl1_src1_en = 1;
			se_src_secure_enable.af.ovl1_se_src1_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl1_se_src1_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl1_se_layer1_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_VID2:
			cfg_g0.af.ovl1_src2_en = 1;
			se_src_secure_enable.af.ovl1_se_src2_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl1_se_src2_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl1_se_layer2_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_GRA0:
			cfg_g0.af.ovl1_src3_en = 1;
			se_src_secure_enable.af.ovl1_se_src3_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl1_se_src3_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl1_se_layer3_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_GRA1:
			cfg_g0.af.ovl1_src4_en = 1;
			se_src_secure_enable.af.ovl1_se_src4_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl1_se_src4_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl1_se_layer4_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_GRA2:
			cfg_g0.af.ovl1_src5_en = 1;
			se_src_secure_enable.af.ovl1_se_src5_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl1_se_src5_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl1_se_layer5_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_GSCL:
			cfg_g0.af.ovl1_src6_en = 1;
			se_src_secure_enable.af.ovl1_se_src6_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl1_se_src6_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl1_se_layer6_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_NUM:
		default :
			pr_err("invalid ovl_idx(%d)\n", ovl_idx);
			return false;
		}
		break;
	case OVL_CH_ID2:
		switch (ch) { /* OVL_CFG_G0 */
		case DU_SRC_VID0:
			cfg_g0.af.ovl2_src0_en = 1;
			se_src_secure_enable.af.ovl2_se_src0_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl2_se_src0_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl2_se_layer0_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_VID1:
			cfg_g0.af.ovl2_src1_en = 1;
			se_src_secure_enable.af.ovl2_se_src1_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl2_se_src1_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl2_se_layer1_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_VID2:
			cfg_g0.af.ovl2_src2_en = 1;
			se_src_secure_enable.af.ovl2_se_src2_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl2_se_src2_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl2_se_layer2_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_GRA0:
			cfg_g0.af.ovl2_src3_en = 1;
			se_src_secure_enable.af.ovl2_se_src3_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl2_se_src3_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl2_se_layer3_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_GRA1:
			cfg_g0.af.ovl2_src4_en = 1;
			se_src_secure_enable.af.ovl2_se_src4_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl2_se_src4_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl2_se_layer4_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_GRA2:
			cfg_g0.af.ovl2_src5_en = 1;
			se_src_secure_enable.af.ovl2_se_src5_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl2_se_src5_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl2_se_layer5_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_GSCL:
			cfg_g0.af.ovl2_src6_en = 1;
			se_src_secure_enable.af.ovl2_se_src6_en = (se_src_secure  == true) ? 0 : 1;
			se_src_secure_enable.af.ovl2_se_src6_sec_en = (se_src_secure  == true) ? 1 : 0;
			se_src_secure_enable.af.ovl2_se_layer6_sec_en = (se_src_layer_secure == true)  ? 1 : 0;
			break;
		case DU_SRC_NUM:
		default :
			pr_err("invalid ovl_idx(%d)\n", ovl_idx);
			return false;
		}
		break;
	default :
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return false;
	}

	ovlbase->cfg_g0 = cfg_g0;
	ovlbase->se_src_secure_enable = se_src_secure_enable;

	return true;
}

bool ovl_hal_set(enum ovl_ch_id ovl_idx, struct ovl_hal_set_param param[OVL_ZORDER_NR])
{
	unsigned int i, j, tempreg;

	for (i = 0; i < OVL_ZORDER_NR; i++) {
		if (param[i].ch == DU_SRC_NUM)
			continue;
		
		for (j = 0; j < OVL_ZORDER_NR; j++) {
			if (param[j].ch == DU_SRC_NUM || i == j)
				continue;

			if(param[i].ch == param[j].ch) {
				pr_err("invalid zorder(%d)=ch(%d) zorder(%d)=ch(%d) of ovl_hal_set()\n",
					i, param[i].ch, j, param[j].ch);
			}
		}
	}

	ovlbase->i_s0_pip_sizexy.a32 = 0;
	ovlbase->i_s1_pip_sizexy.a32 = 0;
	ovlbase->i_s2_pip_sizexy.a32 = 0;
	ovlbase->i_s3_pip_sizexy.a32 = 0;
	ovlbase->i_s4_pip_sizexy.a32 = 0;
	ovlbase->i_s5_pip_sizexy.a32 = 0;
	ovlbase->i_s6_pip_sizexy.a32 = 0;
	ovlbase->i_s0_pip_startxy.a32 = 0;
	ovlbase->i_s1_pip_startxy.a32 = 0;
	ovlbase->i_s2_pip_startxy.a32 = 0;
	ovlbase->i_s3_pip_startxy.a32 = 0;
	ovlbase->i_s4_pip_startxy.a32 = 0;
	ovlbase->i_s5_pip_startxy.a32 = 0;
	ovlbase->i_s6_pip_startxy.a32 = 0;

	switch (ovl_idx) {
	case OVL_CH_ID0:
		ovlbase->ovl0_cfg1.a32 = 0;
		ovlbase->ovl0_l0_ckey_value.a32 = 0;
		ovlbase->ovl0_l1_ckey_value.a32 = 0;
		ovlbase->ovl0_l2_ckey_value.a32 = 0;
		ovlbase->ovl0_l3_ckey_value.a32 = 0;
		ovlbase->ovl0_l4_ckey_value.a32 = 0;
		ovlbase->ovl0_l5_ckey_value.a32 = 0;
		ovlbase->ovl0_l0_ckey_mask_value.a32 = 0;
		ovlbase->ovl0_l1_ckey_mask_value.a32 = 0;
		ovlbase->ovl0_l2_ckey_mask_value.a32 = 0;
		ovlbase->ovl0_l3_ckey_mask_value.a32 = 0;
		ovlbase->ovl0_l4_ckey_mask_value.a32 = 0;
		ovlbase->ovl0_l5_ckey_mask_value.a32 = 0;
		ovlbase->ovl0_cfg2.a32 = 0x1FFFFF;
		ovlbase->ovl0_alpha_reg0.a32 = 0;
		ovlbase->ovl0_alpha_reg1.a32 = 0;
		tempreg = ovlbase->cfg_g0.a32;
		tempreg = tempreg & 0xFFFFFF00;
		ovlbase->cfg_g0.a32 = tempreg;
		break;
	case OVL_CH_ID1:
		ovlbase->ovl1_cfg1.a32 = 0;
		ovlbase->ovl1_l0_ckey_value.a32 = 0;
		ovlbase->ovl1_l1_ckey_value.a32 = 0;
		ovlbase->ovl1_l2_ckey_value.a32 = 0;
		ovlbase->ovl1_l3_ckey_value.a32 = 0;
		ovlbase->ovl1_l4_ckey_value.a32 = 0;
		ovlbase->ovl1_l5_ckey_value.a32 = 0;
		ovlbase->ovl1_l0_ckey_mask_value.a32 = 0;
		ovlbase->ovl1_l1_ckey_mask_value.a32 = 0;
		ovlbase->ovl1_l2_ckey_mask_value.a32 = 0;
		ovlbase->ovl1_l3_ckey_mask_value.a32 = 0;
		ovlbase->ovl1_l4_ckey_mask_value.a32 = 0;
		ovlbase->ovl1_l5_ckey_mask_value.a32 = 0;
		ovlbase->ovl1_cfg2.a32 = 0x1FFFFF;
		ovlbase->ovl1_alpha_reg0.a32 = 0;
		ovlbase->ovl1_alpha_reg0.a32 = 0;
		tempreg = ovlbase->cfg_g0.a32;
		tempreg = tempreg & 0xFFFF00FF;
		ovlbase->cfg_g0.a32 = tempreg;
		break;
	case OVL_CH_ID2:
		ovlbase->ovl2_cfg1.a32 = 0;
		ovlbase->ovl2_l0_ckey_value.a32 = 0;
		ovlbase->ovl2_l1_ckey_value.a32 = 0;
		ovlbase->ovl2_l2_ckey_value.a32 = 0;
		ovlbase->ovl2_l3_ckey_value.a32 = 0;
		ovlbase->ovl2_l4_ckey_value.a32 = 0;
		ovlbase->ovl2_l5_ckey_value.a32 = 0;
		ovlbase->ovl2_l0_ckey_mask_value.a32 = 0;
		ovlbase->ovl2_l1_ckey_mask_value.a32 = 0;
		ovlbase->ovl2_l2_ckey_mask_value.a32 = 0;
		ovlbase->ovl2_l3_ckey_mask_value.a32 = 0;
		ovlbase->ovl2_l4_ckey_mask_value.a32 = 0;
		ovlbase->ovl2_l5_ckey_mask_value.a32 = 0;
		ovlbase->ovl2_cfg2.a32 = 0x1FFFFF;
		ovlbase->ovl2_alpha_reg1.a32 = 0;
		ovlbase->ovl2_alpha_reg1.a32 = 0;
		tempreg = ovlbase->cfg_g0.a32;
		tempreg = tempreg & 0xFF00FFFF;
		ovlbase->cfg_g0.a32 = tempreg;
		break;
	default:
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return false;
	}

	unsigned int g0_temp = 0;
	unsigned int temp = 0;

	for (i = 0; i < OVL_ZORDER_NR; i++) {
		if (param[i].ch == DU_SRC_NUM)
			continue;

		switch (param[i].ch) {
		case DU_SRC_VID0:
			g0_temp = g0_temp | 0x1;
			break;
		case DU_SRC_VID1:
			g0_temp = g0_temp | 0x2;
			break;
		case DU_SRC_VID2:
			g0_temp = g0_temp | 0x4;
			break;
		case DU_SRC_GRA0:
			g0_temp = g0_temp | 0x8;
			break;
		case DU_SRC_GRA1:
			g0_temp = g0_temp | 0x10;
			break;
		case DU_SRC_GRA2:
			g0_temp = g0_temp | 0x20;
			break;
		case DU_SRC_GSCL:
			g0_temp = g0_temp | 0x40;
			break;
		default:
			pr_err("invalid ch(%d) ovl_hal_set\n", param[i].ch);
			return false;
		}

		if (!ovl_hal_channel_set (ovl_idx, param[i].ch, param[i].ovl_image_pos, param[i].ovl_pip_size,
		i, param[i].ops, param[i].ovl_alpha_enable, param[i].alpha_value,
		param[i].pre_mul_enable, param[i].chromakey_enable,
		param[i].chroma_data_r, param[i].chroma_data_g, param[i].chroma_data_b,
		param[i].chroma_mask_r, param[i].chroma_mask_g, param[i].chroma_mask_b,
		param[i].se_src_secure, param[i].se_src_sel_secure, param[i].se_src_layer_secure)) {
			pr_err("ovl_hal_cahnnel_set error\n");
			return false;
		}
	}

	temp = ovlbase->cfg_g0.a32 & 0xFF;

	if (g0_temp != temp) {
		pr_err("not equal cfg_g0(0x%x)g0_reg(0x%x)\n",temp, g0_temp);
		return false;
	}

	return true;
}

bool ovl_hal_channel_clear(enum ovl_ch_id ovl_idx, enum du_src_ch_id ch, enum ovl_zorder zorder)
{
	if (_ovl_hal_verify_ovl_id(ovl_idx) == false) {
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return false;
	}

	if (_ovl_hal_verify_ovl_ch(ch) == false) {
		pr_err("invalid ovl_ch(%d)\n", ch);
	}

	if (zorder > OVL_ZORDER_6) {
		pr_err("invalid ovl_zorder(%d)\n", zorder);
		return false;
	}

	if (ovl_idx == OVL_CH_ID0) {
		volatile ovl0_cfg0 ovl0_cfg0 = ovlbase->ovl0_cfg0;
		volatile ovl0_cfg1 ovl0_cfg1 = ovlbase->ovl0_cfg1;
		volatile ovl0_cfg2 ovl0_cfg2 = ovlbase->ovl0_cfg2;

		switch (zorder) {
		case OVL_ZORDER_6:
			ovl0_cfg0.af.ovl0_layer5_cop = 0xC;
			ovl0_cfg1.af.ovl0_layer5_chromakey_en = 0;
			ovl0_cfg1.af.ovl0_layer5_dst_pre_mul_en = 0;
			ovl0_cfg2.af.ovl0_src6_sel = 0x7;
			ovl0_cfg2.af.ovl0_src6_alpha_reg_en = 0;
			ovlbase->ovl0_cfg0 = ovl0_cfg0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_l5_ckey_value.a32 = 0;
			ovlbase->ovl0_l5_ckey_mask_value.a32 = 0;
			ovlbase->ovl0_alpha_reg1.af.ovl0_src6_alpha = 0;
			break;
		case OVL_ZORDER_5:
			ovl0_cfg0.af.ovl0_layer4_cop = 0xC;
			ovl0_cfg1.af.ovl0_layer4_chromakey_en = 0;
			ovl0_cfg1.af.ovl0_layer5_src_pre_mul_en = 0;
			ovl0_cfg1.af.ovl0_layer4_dst_pre_mul_en = 0;
			ovl0_cfg2.af.ovl0_src5_sel = 0x7;
			ovl0_cfg2.af.ovl0_src5_alpha_reg_en = 0;
			ovlbase->ovl0_cfg0 = ovl0_cfg0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_l4_ckey_value.a32 = 0;
			ovlbase->ovl0_l4_ckey_mask_value.a32 = 0;
			ovlbase->ovl0_alpha_reg1.af.ovl0_src5_alpha = 0;
			break;
		case OVL_ZORDER_4:
			ovl0_cfg0.af.ovl0_layer3_cop = 0xC;
			ovl0_cfg1.af.ovl0_layer3_chromakey_en = 0;
			ovl0_cfg1.af.ovl0_layer4_src_pre_mul_en = 0;
			ovl0_cfg1.af.ovl0_layer3_dst_pre_mul_en = 0;
			ovl0_cfg2.af.ovl0_src4_sel = 0x7;
			ovl0_cfg2.af.ovl0_src4_alpha_reg_en = 0;
			ovlbase->ovl0_cfg0 = ovl0_cfg0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_l3_ckey_value.a32 = 0;
			ovlbase->ovl0_l3_ckey_mask_value.a32 = 0;
			ovlbase->ovl0_alpha_reg1.af.ovl0_src4_alpha = 0;
			break;
		case OVL_ZORDER_3:
			ovl0_cfg0.af.ovl0_layer2_cop = 0xC;
			ovl0_cfg1.af.ovl0_layer2_chromakey_en = 0;
			ovl0_cfg1.af.ovl0_layer3_src_pre_mul_en = 0;
			ovl0_cfg1.af.ovl0_layer2_dst_pre_mul_en = 0;
			ovl0_cfg2.af.ovl0_src3_sel = 0x7;
			ovl0_cfg2.af.ovl0_src3_alpha_reg_en = 0;
			ovlbase->ovl0_cfg0 = ovl0_cfg0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_l2_ckey_value.a32 = 0;
			ovlbase->ovl0_l2_ckey_mask_value.a32 = 0;
			ovlbase->ovl0_alpha_reg0.af.ovl0_src3_alpha = 0;
			break;
		case OVL_ZORDER_2:
			ovl0_cfg0.af.ovl0_layer1_cop = 0xC;
			ovl0_cfg1.af.ovl0_layer1_chromakey_en = 0;
			ovl0_cfg1.af.ovl0_layer2_src_pre_mul_en = 0;
			ovl0_cfg1.af.ovl0_layer1_dst_pre_mul_en = 0;
			ovl0_cfg2.af.ovl0_src2_sel = 0x7;
			ovl0_cfg2.af.ovl0_src2_alpha_reg_en = 0;
			ovlbase->ovl0_cfg0 = ovl0_cfg0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_l1_ckey_value.a32 = 0;
			ovlbase->ovl0_l1_ckey_mask_value.a32 = 0;
			ovlbase->ovl0_alpha_reg0.af.ovl0_src2_alpha = 0;
			break;
		case OVL_ZORDER_1:
			ovl0_cfg0.af.ovl0_layer0_cop = 0xC;
			ovl0_cfg1.af.ovl0_layer0_chromakey_en = 0;
			ovl0_cfg1.af.ovl0_layer1_src_pre_mul_en = 0;
			ovl0_cfg1.af.ovl0_layer0_dst_pre_mul_en = 0;
			ovl0_cfg2.af.ovl0_src1_sel = 0x7;
			ovl0_cfg2.af.ovl0_src1_alpha_reg_en = 0;
			ovlbase->ovl0_cfg0 = ovl0_cfg0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_l0_ckey_value.a32 = 0;
			ovlbase->ovl0_l0_ckey_mask_value.a32 = 0;
			ovlbase->ovl0_alpha_reg0.af.ovl0_src1_alpha = 0;
			break;
		case OVL_ZORDER_0:
			ovl0_cfg1.af.ovl0_layer1_src_pre_mul_en = 0;			
			ovl0_cfg2.af.ovl0_src0_sel = 0x7;
			ovl0_cfg2.af.ovl0_src0_alpha_reg_en = 0;
			ovlbase->ovl0_cfg1 = ovl0_cfg1;
			ovlbase->ovl0_cfg2 = ovl0_cfg2;
			ovlbase->ovl0_alpha_reg0.af.ovl0_src0_alpha = 0;
			break;
		default :
			pr_err("invalid zorder(%d)\n", zorder);
			return false;
		}
	}
	else if (ovl_idx == OVL_CH_ID1) {
		volatile ovl1_cfg0 ovl1_cfg0 = ovlbase->ovl1_cfg0;
		volatile ovl1_cfg1 ovl1_cfg1 = ovlbase->ovl1_cfg1;
		volatile ovl1_cfg2 ovl1_cfg2 = ovlbase->ovl1_cfg2;

		switch (zorder) {
		case OVL_ZORDER_6:
			ovl1_cfg0.af.ovl1_layer5_cop = 0xC;
			ovl1_cfg1.af.ovl1_layer5_chromakey_en = 0;
			ovl1_cfg1.af.ovl1_layer5_dst_pre_mul_en = 0;			
			ovl1_cfg2.af.ovl1_src6_sel = 0x7;
			ovl1_cfg2.af.ovl1_src6_alpha_reg_en = 0;
			ovlbase->ovl1_cfg0 = ovl1_cfg0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_l5_ckey_value.a32 = 0;
			ovlbase->ovl1_l5_ckey_mask_value.a32 = 0;
			ovlbase->ovl1_alpha_reg1.af.ovl1_src6_alpha = 0;
			break;
		case OVL_ZORDER_5:
			ovl1_cfg0.af.ovl1_layer4_cop = 0xC;
			ovl1_cfg1.af.ovl1_layer4_chromakey_en = 0;
			ovl1_cfg1.af.ovl1_layer5_src_pre_mul_en = 0;
			ovl1_cfg1.af.ovl1_layer4_dst_pre_mul_en = 0;
			ovl1_cfg2.af.ovl1_src5_sel = 0x7;
			ovl1_cfg2.af.ovl1_src5_alpha_reg_en = 0;
			ovlbase->ovl1_cfg0 = ovl1_cfg0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_l4_ckey_value.a32 = 0;
			ovlbase->ovl1_l4_ckey_mask_value.a32 = 0;
			ovlbase->ovl1_alpha_reg1.af.ovl1_src5_alpha = 0;
			break;
		case OVL_ZORDER_4:
			ovl1_cfg0.af.ovl1_layer3_cop = 0xC;
			ovl1_cfg1.af.ovl1_layer3_chromakey_en = 0;
			ovl1_cfg1.af.ovl1_layer4_src_pre_mul_en = 0;
			ovl1_cfg1.af.ovl1_layer3_dst_pre_mul_en = 0;
			ovl1_cfg2.af.ovl1_src4_sel = 0x7;
			ovl1_cfg2.af.ovl1_src4_alpha_reg_en = 0;
			ovlbase->ovl1_cfg0 = ovl1_cfg0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_l3_ckey_value.a32 = 0;
			ovlbase->ovl1_l3_ckey_mask_value.a32 = 0;
			ovlbase->ovl1_alpha_reg1.af.ovl1_src4_alpha = 0;
			break;
		case OVL_ZORDER_3:
			ovl1_cfg0.af.ovl1_layer2_cop = 0xC;
			ovl1_cfg1.af.ovl1_layer2_chromakey_en = 0;
			ovl1_cfg1.af.ovl1_layer3_src_pre_mul_en = 0;
			ovl1_cfg1.af.ovl1_layer2_dst_pre_mul_en = 0;
			ovl1_cfg2.af.ovl1_src3_sel = 0x7;
			ovl1_cfg2.af.ovl1_src3_alpha_reg_en = 0;
			ovlbase->ovl1_cfg0 = ovl1_cfg0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_l2_ckey_value.a32 = 0;
			ovlbase->ovl1_l2_ckey_mask_value.a32 = 0;
			ovlbase->ovl1_alpha_reg0.af.ovl1_src3_alpha = 0;
			break;
		case OVL_ZORDER_2:
			ovl1_cfg0.af.ovl1_layer1_cop = 0xC;
			ovl1_cfg1.af.ovl1_layer1_chromakey_en = 0;
			ovl1_cfg1.af.ovl1_layer2_src_pre_mul_en = 0;
			ovl1_cfg1.af.ovl1_layer1_dst_pre_mul_en = 0;
			ovl1_cfg2.af.ovl1_src2_sel = 0x7;
			ovl1_cfg2.af.ovl1_src2_alpha_reg_en = 0;
			ovlbase->ovl1_cfg0 = ovl1_cfg0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_l1_ckey_value.a32 = 0;
			ovlbase->ovl1_l1_ckey_mask_value.a32 = 0;
			ovlbase->ovl1_alpha_reg0.af.ovl1_src2_alpha = 0;
			break;
		case OVL_ZORDER_1:
			ovl1_cfg0.af.ovl1_layer0_cop = 0xC;
			ovl1_cfg1.af.ovl1_layer0_chromakey_en = 0;
			ovl1_cfg1.af.ovl1_layer1_src_pre_mul_en = 0;
			ovl1_cfg1.af.ovl1_layer0_dst_pre_mul_en = 0;
			ovl1_cfg2.af.ovl1_src1_sel = 0x7;
			ovl1_cfg2.af.ovl1_src1_alpha_reg_en = 0;
			ovlbase->ovl1_cfg0 = ovl1_cfg0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_l0_ckey_value.a32 = 0;
			ovlbase->ovl1_l0_ckey_mask_value.a32 = 0;
			ovlbase->ovl1_alpha_reg0.af.ovl1_src1_alpha = 0;
			break;
		case OVL_ZORDER_0:
			ovl1_cfg1.af.ovl1_layer0_src_pre_mul_en = 0;			
			ovl1_cfg2.af.ovl1_src0_sel = 0x7;
			ovl1_cfg2.af.ovl1_src0_alpha_reg_en = 0;
			ovlbase->ovl1_cfg1 = ovl1_cfg1;			
			ovlbase->ovl1_cfg2 = ovl1_cfg2;
			ovlbase->ovl1_alpha_reg0.af.ovl1_src0_alpha = 0;
			break;
		default :
			pr_err("invalid zorder(%d)\n", zorder);
			return false;
		}
	}
	else if (ovl_idx == OVL_CH_ID2) {
		volatile ovl2_cfg0 ovl2_cfg0 = ovlbase->ovl2_cfg0;
		volatile ovl2_cfg1 ovl2_cfg1 = ovlbase->ovl2_cfg1;
		volatile ovl2_cfg2 ovl2_cfg2 = ovlbase->ovl2_cfg2;

		switch (zorder) {
		case OVL_ZORDER_6:
			ovl2_cfg0.af.ovl2_layer5_cop = 0xC;
			ovl2_cfg1.af.ovl2_layer5_chromakey_en = 0;
			ovl2_cfg1.af.ovl2_layer5_src_pre_mul_en = 0;
			ovl2_cfg1.af.ovl2_layer5_dst_pre_mul_en = 0;
			ovl2_cfg2.af.ovl2_src6_sel = 0x7;
			ovl2_cfg2.af.ovl2_src6_alpha_reg_en = 0;
			ovlbase->ovl2_cfg0 = ovl2_cfg0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_l5_ckey_value.a32 = 0;
			ovlbase->ovl2_l5_ckey_mask_value.a32 = 0;
			ovlbase->ovl2_alpha_reg1.af.ovl2_src6_alpha = 0;
			break;
		case OVL_ZORDER_5:
			ovl2_cfg0.af.ovl2_layer4_cop = 0xC;
			ovl2_cfg1.af.ovl2_layer4_chromakey_en = 0;
			ovl2_cfg1.af.ovl2_layer4_src_pre_mul_en = 0;
			ovl2_cfg1.af.ovl2_layer4_dst_pre_mul_en = 0;
			ovl2_cfg2.af.ovl2_src5_sel = 0x7;
			ovl2_cfg2.af.ovl2_src5_alpha_reg_en = 0;
			ovlbase->ovl2_cfg0 = ovl2_cfg0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_l4_ckey_value.a32 = 0;
			ovlbase->ovl2_l4_ckey_mask_value.a32 = 0;
			ovlbase->ovl2_alpha_reg1.af.ovl2_src5_alpha = 0;
			break;
		case OVL_ZORDER_4:
			ovl2_cfg0.af.ovl2_layer3_cop = 0xC;
			ovl2_cfg1.af.ovl2_layer3_chromakey_en = 0;
			ovl2_cfg1.af.ovl2_layer3_src_pre_mul_en = 0;
			ovl2_cfg1.af.ovl2_layer3_dst_pre_mul_en = 0;
			ovl2_cfg2.af.ovl2_src4_sel = 0x7;
			ovl2_cfg2.af.ovl2_src4_alpha_reg_en = 0;
			ovlbase->ovl2_cfg0 = ovl2_cfg0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_l3_ckey_value.a32 = 0;
			ovlbase->ovl2_l3_ckey_mask_value.a32 = 0;
			ovlbase->ovl2_alpha_reg1.af.ovl2_src4_alpha = 0;
			break;
		case OVL_ZORDER_3:
			ovl2_cfg0.af.ovl2_layer2_cop = 0xC;
			ovl2_cfg1.af.ovl2_layer2_chromakey_en = 0;
			ovl2_cfg1.af.ovl2_layer2_src_pre_mul_en = 0;
			ovl2_cfg1.af.ovl2_layer2_dst_pre_mul_en = 0;
			ovl2_cfg2.af.ovl2_src3_sel = 0x7;
			ovl2_cfg2.af.ovl2_src3_alpha_reg_en = 0;
			ovlbase->ovl2_cfg0 = ovl2_cfg0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_l2_ckey_value.a32 = 0;
			ovlbase->ovl2_l2_ckey_mask_value.a32 = 0;
			ovlbase->ovl2_alpha_reg0.af.ovl2_src3_alpha = 0;
			break;
		case OVL_ZORDER_2:
			ovl2_cfg0.af.ovl2_layer1_cop = 0xC;
			ovl2_cfg1.af.ovl2_layer1_chromakey_en = 0;
			ovl2_cfg1.af.ovl2_layer1_src_pre_mul_en = 0;
			ovl2_cfg1.af.ovl2_layer1_dst_pre_mul_en = 0;
			ovl2_cfg2.af.ovl2_src2_sel = 0x7;
			ovl2_cfg2.af.ovl2_src2_alpha_reg_en = 0;
			ovlbase->ovl2_cfg0 = ovl2_cfg0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_l1_ckey_value.a32 = 0;
			ovlbase->ovl2_l1_ckey_mask_value.a32 = 0;
			ovlbase->ovl2_alpha_reg0.af.ovl2_src2_alpha = 0;
			break;
		case OVL_ZORDER_1:
			ovl2_cfg0.af.ovl2_layer0_cop = 0xC;
			ovl2_cfg1.af.ovl2_layer0_chromakey_en = 0;
			ovl2_cfg1.af.ovl2_layer0_src_pre_mul_en = 0;
			ovl2_cfg1.af.ovl2_layer0_dst_pre_mul_en = 0;
			ovl2_cfg2.af.ovl2_src1_sel = 0x7;
			ovl2_cfg2.af.ovl2_src1_alpha_reg_en = 0;
			ovlbase->ovl2_cfg0 = ovl2_cfg0;
			ovlbase->ovl2_cfg1 = ovl2_cfg1;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_l0_ckey_value.a32 = 0;
			ovlbase->ovl2_l0_ckey_mask_value.a32 = 0;
			ovlbase->ovl2_alpha_reg0.af.ovl2_src1_alpha = 0;
			break;
		case OVL_ZORDER_0:
			ovl2_cfg2.af.ovl2_src0_sel = 0x7;
			ovl2_cfg2.af.ovl2_src0_alpha_reg_en = 0;
			ovlbase->ovl2_cfg2 = ovl2_cfg2;
			ovlbase->ovl2_alpha_reg0.af.ovl2_src0_alpha = 0;
			break;
		default :
			pr_err("invalid zorder(%d)\n", zorder);
			return false;
		}
	}
	else{
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return false;
	}

	switch (zorder) {
	case OVL_ZORDER_6:
		ovlbase->i_s6_pip_startxy.a32 = 0;
		ovlbase->i_s6_pip_sizexy.a32 = 0;
		break;
	case OVL_ZORDER_5:
		ovlbase->i_s5_pip_startxy.a32 = 0;
		ovlbase->i_s5_pip_sizexy.a32 = 0;
		break;
	case OVL_ZORDER_4:
		ovlbase->i_s4_pip_startxy.a32 = 0;
		ovlbase->i_s4_pip_sizexy.a32 = 0;
		break;
	case OVL_ZORDER_3:
		ovlbase->i_s3_pip_startxy.a32 = 0;
		ovlbase->i_s3_pip_sizexy.a32 = 0;
		break;
	case OVL_ZORDER_2:
		ovlbase->i_s2_pip_startxy.a32 = 0;
		ovlbase->i_s2_pip_sizexy.a32 = 0;
		break;
	case OVL_ZORDER_1:
		ovlbase->i_s1_pip_startxy.a32 = 0;
		ovlbase->i_s1_pip_sizexy.a32 = 0;
		break;
	case OVL_ZORDER_0:
		ovlbase->i_s0_pip_startxy.a32 = 0;
		ovlbase->i_s0_pip_sizexy.a32 = 0;
		break;
	default :
		pr_err("invalid zorder(%d)\n", zorder);
		return false;
	}


	volatile ovl_cfg_g0 cfg_g0 = ovlbase->cfg_g0;
	volatile ovl_se_src_secure_enable se_src_secure_enable = ovlbase->se_src_secure_enable;

	switch (ovl_idx) {
	case OVL_CH_ID0:
		switch (ch) { /* OVL_CFG_G0 */
		case DU_SRC_VID0:
			cfg_g0.af.ovl0_src0_en = 0;
			se_src_secure_enable.af.ovl0_se_src0_en = 1;
			se_src_secure_enable.af.ovl0_se_src0_sec_en = 0;
			se_src_secure_enable.af.ovl0_se_layer0_sec_en = 0;
			break;
		case DU_SRC_VID1:
			cfg_g0.af.ovl0_src1_en = 0;
			se_src_secure_enable.af.ovl0_se_src1_en = 1;
			se_src_secure_enable.af.ovl0_se_src1_sec_en = 0;
			se_src_secure_enable.af.ovl0_se_layer1_sec_en = 0;
			break;
		case DU_SRC_VID2:
			cfg_g0.af.ovl0_src2_en = 0;
			se_src_secure_enable.af.ovl0_se_src2_en = 1;
			se_src_secure_enable.af.ovl0_se_src2_sec_en = 0;
			se_src_secure_enable.af.ovl0_se_layer2_sec_en = 0;
			break;
		case DU_SRC_GRA0:
			cfg_g0.af.ovl0_src3_en = 0;
			se_src_secure_enable.af.ovl0_se_src3_en = 1;
			se_src_secure_enable.af.ovl0_se_src3_sec_en = 0;
			se_src_secure_enable.af.ovl0_se_layer3_sec_en = 0;
			break;
		case DU_SRC_GRA1:
			cfg_g0.af.ovl0_src4_en = 0;
			se_src_secure_enable.af.ovl0_se_src4_en = 1;
			se_src_secure_enable.af.ovl0_se_src4_sec_en = 0;
			se_src_secure_enable.af.ovl0_se_layer4_sec_en = 0;
			break;
		case DU_SRC_GRA2:
			cfg_g0.af.ovl0_src5_en = 0;
			se_src_secure_enable.af.ovl0_se_src5_en = 1;
			se_src_secure_enable.af.ovl0_se_src5_sec_en = 0;
			se_src_secure_enable.af.ovl0_se_layer5_sec_en = 0;
			break;
		case DU_SRC_GSCL:
			cfg_g0.af.ovl0_src6_en = 0;
			se_src_secure_enable.af.ovl0_se_src6_en = 1;
			se_src_secure_enable.af.ovl0_se_src6_sec_en = 0;
			se_src_secure_enable.af.ovl0_se_layer6_sec_en = 0;
			break;
		case DU_SRC_NUM:
		default :
			pr_err("invalid ovl_idx(%d)\n", ovl_idx);
			return false;
		}
		break;	
	case OVL_CH_ID1:
		switch (ch) { /* OVL_CFG_G0 */
		case DU_SRC_VID0:
			cfg_g0.af.ovl1_src0_en = 0;
			se_src_secure_enable.af.ovl1_se_src0_en = 1;
			se_src_secure_enable.af.ovl1_se_src0_sec_en = 0;
			se_src_secure_enable.af.ovl1_se_layer0_sec_en = 0;
			break;
		case DU_SRC_VID1:
			cfg_g0.af.ovl1_src1_en = 0;
			se_src_secure_enable.af.ovl1_se_src1_en = 1;
			se_src_secure_enable.af.ovl1_se_src1_sec_en = 0;
			se_src_secure_enable.af.ovl1_se_layer1_sec_en = 0;
			break;
		case DU_SRC_VID2:
			cfg_g0.af.ovl1_src2_en = 0;
			se_src_secure_enable.af.ovl1_se_src2_en = 1;
			se_src_secure_enable.af.ovl1_se_src2_sec_en = 0;
			se_src_secure_enable.af.ovl1_se_layer2_sec_en = 0;
			break;
		case DU_SRC_GRA0:
			cfg_g0.af.ovl1_src3_en = 0;
			se_src_secure_enable.af.ovl1_se_src3_en = 1;
			se_src_secure_enable.af.ovl1_se_src3_sec_en = 0;
			se_src_secure_enable.af.ovl1_se_layer3_sec_en = 0;
			break;
		case DU_SRC_GRA1:
			cfg_g0.af.ovl1_src4_en = 0;
			se_src_secure_enable.af.ovl1_se_src4_en = 1;
			se_src_secure_enable.af.ovl1_se_src4_sec_en = 0;
			se_src_secure_enable.af.ovl1_se_layer4_sec_en = 0;
			break;
		case DU_SRC_GRA2:
			cfg_g0.af.ovl1_src5_en = 0;
			se_src_secure_enable.af.ovl1_se_src5_en = 1;
			se_src_secure_enable.af.ovl1_se_src5_sec_en = 0;
			se_src_secure_enable.af.ovl1_se_layer5_sec_en = 0;
			break;
		case DU_SRC_GSCL:
			cfg_g0.af.ovl1_src6_en = 0;
			se_src_secure_enable.af.ovl1_se_src6_en = 1;
			se_src_secure_enable.af.ovl1_se_src6_sec_en = 0;
			se_src_secure_enable.af.ovl1_se_layer6_sec_en = 0;
			break;
		case DU_SRC_NUM:
		default :
			pr_err("invalid ovl_idx(%d)\n", ovl_idx);
			return false;
		}
		break;
	case OVL_CH_ID2:
		switch (ch) { /* OVL_CFG_G0 */
		case DU_SRC_VID0:
			cfg_g0.af.ovl2_src0_en = 0;
			se_src_secure_enable.af.ovl2_se_src0_en = 1;
			se_src_secure_enable.af.ovl2_se_src0_sec_en = 0;
			se_src_secure_enable.af.ovl2_se_layer0_sec_en = 0;
			break;
		case DU_SRC_VID1:
			cfg_g0.af.ovl2_src1_en = 0;
			se_src_secure_enable.af.ovl2_se_src1_en = 1;
			se_src_secure_enable.af.ovl2_se_src1_sec_en = 0;
			se_src_secure_enable.af.ovl2_se_layer1_sec_en = 0;
			break;
		case DU_SRC_VID2:
			cfg_g0.af.ovl2_src2_en = 0;
			se_src_secure_enable.af.ovl2_se_src2_en = 1;
			se_src_secure_enable.af.ovl2_se_src2_sec_en = 0;
			se_src_secure_enable.af.ovl2_se_layer2_sec_en = 0;
			break;
		case DU_SRC_GRA0:
			cfg_g0.af.ovl2_src3_en = 0;
			se_src_secure_enable.af.ovl2_se_src3_en = 1;
			se_src_secure_enable.af.ovl2_se_src3_sec_en = 0;
			se_src_secure_enable.af.ovl2_se_layer3_sec_en = 0;
			break;
		case DU_SRC_GRA1:
			cfg_g0.af.ovl2_src4_en = 0;
			se_src_secure_enable.af.ovl2_se_src4_en = 1;
			se_src_secure_enable.af.ovl2_se_src4_sec_en = 0;
			se_src_secure_enable.af.ovl2_se_layer4_sec_en = 0;
			break;
		case DU_SRC_GRA2:
			cfg_g0.af.ovl2_src5_en = 0;
			se_src_secure_enable.af.ovl2_se_src5_en = 1;
			se_src_secure_enable.af.ovl2_se_src5_sec_en = 0;
			se_src_secure_enable.af.ovl2_se_layer5_sec_en = 0;
			break;
		case DU_SRC_GSCL:
			cfg_g0.af.ovl2_src6_en = 0;
			se_src_secure_enable.af.ovl2_se_src6_en = 1;
			se_src_secure_enable.af.ovl2_se_src6_sec_en = 0;
			se_src_secure_enable.af.ovl2_se_layer6_sec_en = 0;
			break;
		case DU_SRC_NUM:
		default :
			pr_err("invalid ovl_idx(%d)\n", ovl_idx);
			return false;
		}
		break;
	default :
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return false;
	}

	ovlbase->cfg_g0 = cfg_g0;
	ovlbase->se_src_secure_enable = se_src_secure_enable;

	return true;
}

bool ovl_hal_enable(enum ovl_ch_id ovl_idx, struct dss_image_size ovl_size, bool dual_op_enable, bool raster, enum ovl_raster_ops raster_op, 
		unsigned char pattern_data_r, unsigned char pattern_data_g, unsigned char pattern_data_b)
{
	if (_ovl_hal_verify_ovl_id(ovl_idx) == false) {
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return false;
	}

	if (raster == true && raster_op >= OVL_RASTER_OPS_NUM) {
		pr_err("invalid raster(%d)\n", raster_op);
		return false;
	}

	if (ovl_idx == OVL_CH_ID0) {
		volatile ovl0_cfg0 ovl0_cfg0 = ovlbase->ovl0_cfg0;
		volatile ovl0_sizexy ovl0_sizexy = ovlbase->ovl0_sizexy;
		volatile ovl0_pattern_data ovl0_pattern_data = ovlbase->ovl0_pattern_data;

		ovl0_cfg0.af.ovl0_en = 1;
		ovl0_cfg0.af.ovl0_layer_op = (raster == true) ? 1 : 0;
		ovl0_cfg0.af.ovl0_dst_sel = 0;
		ovl0_cfg0.af.ovl0_dual_op_en = (dual_op_enable == true) ? 1 : 0;
		ovl0_cfg0.af.ovl0_layer0_rop = raster_op;
		ovlbase->ovl0_cfg0 = ovl0_cfg0;
		ovlbase->ovl0_sizexy.a32 = (ovl_size.h << 16 | ovl_size.w);
		ovlbase->ovl0_pattern_data.a32 = (pattern_data_r << 16 | pattern_data_g << 8 | pattern_data_b);
	}
	else if (ovl_idx == OVL_CH_ID1) {
		volatile ovl1_cfg0 ovl1_cfg0 = ovlbase->ovl1_cfg0;
		volatile ovl1_sizexy ovl1_sizexy = ovlbase->ovl1_sizexy;
		volatile ovl1_pattern_data ovl1_pattern_data = ovlbase->ovl1_pattern_data;

		ovl1_cfg0.af.ovl1_en = 1;
		ovl1_cfg0.af.ovl1_layer_op = (raster == true) ? 1 : 0;;
		ovl1_cfg0.af.ovl1_dst_sel = 0;
		ovl1_cfg0.af.ovl1_dual_op_en = (dual_op_enable == true) ? 1 : 0;
		ovl1_cfg0.af.ovl1_layer0_rop = raster_op;
		ovlbase->ovl1_cfg0 = ovl1_cfg0;
		ovlbase->ovl1_sizexy.a32 = (ovl_size.h << 16 | ovl_size.w);
		ovlbase->ovl1_pattern_data.a32 = (pattern_data_r << 16 | pattern_data_g << 8 | pattern_data_b);
	}
	else if (ovl_idx == OVL_CH_ID2) {
		volatile ovl2_cfg0 ovl2_cfg0 = ovlbase->ovl2_cfg0;
		volatile ovl2_sizexy ovl2_sizexy = ovlbase->ovl2_sizexy;
		volatile ovl2_pattern_data ovl2_pattern_data = ovlbase->ovl2_pattern_data;

		ovl2_cfg0.af.ovl2_en = 1;
		ovl2_cfg0.af.ovl2_layer_op = (raster == true) ? 1 : 0;
		ovl2_cfg0.af.ovl2_dst_sel = 0;
		ovl2_cfg0.af.ovl2_dual_op_en = (dual_op_enable == true) ? 1 : 0;
		ovl2_cfg0.af.ovl2_layer0_rop = raster_op;
		ovlbase->ovl2_cfg0 = ovl2_cfg0;
		ovlbase->ovl2_sizexy.a32 = (ovl_size.h << 16 | ovl_size.w);
		ovlbase->ovl2_pattern_data.a32 = (pattern_data_r << 16 | pattern_data_g << 8 | pattern_data_b);
	}
	else{
		pr_err("invalid ovl_idx(%d)\n", ovl_idx);
		return false;
	}

	return true;
}

bool ovl_hal_disable(enum ovl_ch_id ovl_idx)
{
	volatile ovl0_cfg0 ovl0_cfg0_set = {.a32 = 0};
	volatile ovl1_cfg0 ovl1_cfg0_set = {.a32 = 0};
	volatile ovl2_cfg0 ovl2_cfg0_set = {.a32 = 0};

	switch (ovl_idx) {
	case OVL_CH_ID0:
		ovl0_cfg0_set.af.ovl0_en = 0;
		ovl0_cfg0_set.af.ovl0_layer0_rop = 0xf;
		ovl0_cfg0_set.af.ovl0_layer0_cop = 0xc;
		ovl0_cfg0_set.af.ovl0_layer1_cop = 0xc;
		ovl0_cfg0_set.af.ovl0_layer2_cop = 0xc;
		ovl0_cfg0_set.af.ovl0_layer3_cop = 0xc;
		ovl0_cfg0_set.af.ovl0_layer4_cop = 0xc;
		ovl0_cfg0_set.af.ovl0_layer5_cop = 0xc;
		ovlbase->ovl0_cfg0 = ovl0_cfg0_set;
		ovlbase->ovl0_sizexy.a32 = 0;
		ovlbase->ovl0_pattern_data.a32 = 0;
		break;
	case OVL_CH_ID1:
		ovl1_cfg0_set.af.ovl1_en = 0;
		ovl1_cfg0_set.af.ovl1_layer0_rop = 0xf;
		ovl1_cfg0_set.af.ovl1_layer0_cop = 0xc;
		ovl1_cfg0_set.af.ovl1_layer1_cop = 0xc;
		ovl1_cfg0_set.af.ovl1_layer2_cop = 0xc;
		ovl1_cfg0_set.af.ovl1_layer3_cop = 0xc;
		ovl1_cfg0_set.af.ovl1_layer4_cop = 0xc;
		ovl1_cfg0_set.af.ovl1_layer5_cop = 0xc;
		ovlbase->ovl1_cfg0 = ovl1_cfg0_set;
		ovlbase->ovl1_sizexy.a32 = 0;
		ovlbase->ovl1_pattern_data.a32 = 0;
		break;
	case OVL_CH_ID2:
		ovl2_cfg0_set.af.ovl2_en = 0;
		ovl2_cfg0_set.af.ovl2_layer0_rop = 0xf;
		ovl2_cfg0_set.af.ovl2_layer0_cop = 0xc;
		ovl2_cfg0_set.af.ovl2_layer1_cop = 0xc;
		ovl2_cfg0_set.af.ovl2_layer2_cop = 0xc;
		ovl2_cfg0_set.af.ovl2_layer3_cop = 0xc;
		ovl2_cfg0_set.af.ovl2_layer4_cop = 0xc;
		ovl2_cfg0_set.af.ovl2_layer5_cop = 0xc;
		ovlbase->ovl2_cfg0 = ovl2_cfg0_set;
		ovlbase->ovl2_sizexy.a32 = 0;
		ovlbase->ovl2_pattern_data.a32 = 0;
		break;
	default :
		pr_err("invalid ovl_hal_disable ovl_idx(%d)\n", ovl_idx);
		return false;
	}

	return true;
}

void ovl_hal_init(unsigned int ovl_base)
{
	ovlbase = (DSS_DU_OVL_REG_T *)ovl_base;

	ovl_hal_disable(OVL_CH_ID0);
	ovl_hal_disable(OVL_CH_ID1);
	ovl_hal_disable(OVL_CH_ID2);

	ovlbase->ovl0_cfg1.a32 = 0;  /* invalide src_sel value */
	ovlbase->ovl1_cfg1.a32 = 0;
	ovlbase->ovl2_cfg1.a32 = 0;
	ovlbase->ovl0_cfg2.a32 = 0x1FFFFF;
	ovlbase->ovl1_cfg2.a32 = 0x1FFFFF;
	ovlbase->ovl2_cfg2.a32 = 0x1FFFFF;
	ovlbase->ovl0_alpha_reg0.a32 = 0;
	ovlbase->ovl0_alpha_reg1.a32 = 0;
	ovlbase->ovl1_alpha_reg0.a32 = 0;
	ovlbase->ovl1_alpha_reg1.a32 = 0;
	ovlbase->ovl2_alpha_reg0.a32 = 0;
	ovlbase->ovl2_alpha_reg1.a32 = 0;
}

void ovl_hal_cleanup(void)
{
}
