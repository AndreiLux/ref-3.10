/*
 * ISP interface driver
 *
 * Copyright (C) 2010 - 2013 Mtekvision
 * Author: DongHyung Ko <kodh@mtekvision.com>
 *         Jinyoung Park <parkjyb@mtekvision.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/******************************************************************************
	Internal header files
******************************************************************************/

#if CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

#include "odin-css.h"
#include "odin-css-utils.h"
#include "odin-css-system.h"
#include "odin-css-clk.h"
#include "odin-capture.h"
#include "odin-bayercontrol.h"
#include "odin-isp.h"

/******************************************************************************
	Macro definitions
******************************************************************************/
#define STAT_INT_TIME_CHECK 1
#define STAT_ZONE_UNIT_SIZE 80

#define ctrl_to_hw(ctrl)	(ctrl->isp_hw)

/* #define INTR_CHECK_THROUGH_GPIO */

/******************************************************************************
	Locally defined global variables
******************************************************************************/
atomic_t   frame_cnt		   = ATOMIC_INIT(0);
atomic_t   mdis_cnt 		   = ATOMIC_INIT(0);
u32 	   mdis_fill_idx	   = 0;
u32 	   *sensor_reg		   = NULL;
u32 	   intr_check_gpio_num = 0;

/******************************************************************************
	Static variables
******************************************************************************/
static struct odin_isp_ctrl *isp_ctrl = NULL;
struct odin_isp_ctrl *isp_ctrl_info   = NULL;
struct		  i2c_client	*sensor   = NULL;
struct		  i2c_adapter	*adapter  = NULL;

static DEFINE_MUTEX(devinit);

#ifdef CONFIG_VIDEO_ISP_MISC_DRIVER
struct odin_isp_dev_info
{
	s32  index;
	char dev_name[256];
};

struct odin_isp_fh
{
	s32    idx;
	struct odin_isp_ctrl *isp_ctrl;
};

static const struct odin_isp_dev_info isp_dev_name[] = {
	{0, "isp0"},
	{1, "isp1"},
};
#endif

static const u32 lut_hsl_table_add[4] =
{
	HSL_OFFSET + REG_HUE_CURVE_BASE_ADDR,
	HSL_OFFSET + REG_HS_BASE_ADDR,
	HSL_OFFSET + REG_S_CURVE_BASE_ADDR,
	HSL_OFFSET + REG_L_CURVE_BASE_ADDR
};
static const u32 lut_gamma_curve_ctrl_add[5] = {
	BAYER_OFFSET + REG_BAYER_GAMMA_EN,
	RGB_OFFSET	 + REG_RGB_GAMMA_EN,
	YUV_OFFSET	 + REG_Y_GAMMA_EN,
	YUV_OFFSET	 + REG_CB_GAMMA_EN,
	YUV_OFFSET	 + REG_CR_GAMMA_EN
};

static const u32 lut_gamma_table_add[11] = {
	BAYER_OFFSET + REG_GAMMA_R_BASE_ADDR,
	BAYER_OFFSET + REG_GAMMA_G_BASE_ADDR,
	BAYER_OFFSET + REG_GAMMA_G_BASE_ADDR,
	0x0,
	RGB_OFFSET	 + REG_RGB_GAMMA_R_BASE_ADDR,
	RGB_OFFSET	 + REG_RGB_GAMMA_G_BASE_ADDR,
	RGB_OFFSET	 + REG_RGB_GAMMA_B_BASE_ADDR,
	0x0,
	YUV_OFFSET	 + REG_Y_GAMMA_BASE_ADDR,
	YUV_OFFSET	 + REG_CB_GAMMA_BASE_ADDR,
	YUV_OFFSET	 + REG_CR_GAMMA_BASE_ADDR
};

static const u32 lut_imm_reg_update[4] = {
	BAYER_OFFSET + REG_BAYER_IMMEDIATE_UPDATE,
	RGB_OFFSET	 + REG_RGB_IMMEDIATE_UPDATE,
	HSL_OFFSET	 + REG_HSL_IMMEDIATE_UPDATE,
	YUV_OFFSET	 + REG_YUV_IMMEDIATE_UPDATE,
};

static const u32 lut_hsl_table_enable[4] =
{
	ENABLE_H_CURVE,
	ENABLE_H_S_CURVE,
	ENABLE_S_CURVE,
	ENABLE_L_CURVE,
};

static const u32 lut_hsl_curv_step_num[2] = {192, 256};
static const u32 lut_gamma_step_number[2] = {114, 128};

/******************************************************************************
	Static function prototypes
******************************************************************************/
/******************************************************************************
	Function definitions
******************************************************************************/
static inline struct isp_hardware *get_isp_hw(void)
{
	return (struct isp_hardware *)&get_css_plat()->isp_hw;
}

static inline struct odin_isp_ctrl *
odin_isp_get_handler(void)
{
	return isp_ctrl;
}

static inline
void odin_isp_vsync_intr_enable(u32 val, u32 offset)
{
	writel(val, (void __iomem *)(offset + REG_VSYNC_INT_EN));
}

static inline
void odin_isp_stat_intr_enable(u32 val, u32 offset)
{
	writel(val, (void __iomem *)(offset + REG_STATISTIC_LAYER_FUNC_ENABLE));
}

static inline
void odin_isp_vsync_intr_clear(u32 val, u32 offset)
{
	writel(val, (void __iomem *)(offset + REG_VSYNC_INT_CLEAR));
}

static inline
void odin_isp_stat_intr_clear(u32 val, u32 offset)
{
	writel(val, (void __iomem *)(offset + REG_STAT_INT_CLEAR));
}

static inline
u32 odin_isp_vsync_intr_status(u32 offset)
{
	return readl((void __iomem *)offset + REG_VSYNC_INT_STATUS);
}

void
odin_isp_interrupt_enable(u32 isp_idx, u32 irq)
{
	struct odin_isp_ctrl *isp		  = odin_isp_get_handler();
	u32 				 base		  = isp->isp_base[isp_idx];
	u32 				 offset_vsync = base + TEST_OFFSET;
	u32 				 offset_stat  = base + STATISTIC_OFFSET;

	if (irq == IRQ_STATIC) {
		odin_isp_stat_intr_enable(0x7, offset_stat);
	} else {
		odin_isp_vsync_intr_clear((VSYNC_INTERRUPT_POS_BIT |
									VSYNC_INTERRUPT_NEG_BIT), offset_vsync);
		odin_isp_vsync_intr_enable((VSYNC_INTERRUPT_POS_BIT |
									VSYNC_INTERRUPT_NEG_BIT), offset_vsync);
	}
}

void
odin_isp_interrupt_disable(u32 isp_idx, u32 irq)
{
	struct odin_isp_ctrl *isp		  = odin_isp_get_handler();
	u32 				 base		  = isp->isp_base[isp_idx];
	u32 				 offset_vsync = base + TEST_OFFSET;
	u32 				 offset_stat  = base + STATISTIC_OFFSET;

	if (irq == IRQ_STATIC) {
		odin_isp_stat_intr_clear(STAT_INTERRUPT_ENABLE_BIT, offset_stat);
		odin_isp_stat_intr_enable(0x0, offset_stat);
	} else {
		odin_isp_vsync_intr_enable(~(VSYNC_INTERRUPT_POS_BIT |
									 VSYNC_INTERRUPT_NEG_BIT), offset_vsync);
	}
}

static inline void
odin_isp_set_reg_imm_update(u32 status, u32 type, u32 base)
{
	writel(status, (void __iomem *)(base + lut_imm_reg_update[type]));
}

static inline void
odin_isp_set_gamma_ctrl(u32 ctrl, u32 type, u32 base)
{
	writel(ctrl, (void __iomem *)(base + lut_gamma_curve_ctrl_add[type]));
}

static inline void
odin_isp_update_awb_gain(struct odin_isp_ctrl *pIsp, u32 base, u32 isp_idx)
{
	writel(pIsp->awb_gain[isp_idx].r_gain,
		   (void __iomem *)(base + BAYER_OFFSET + REG_AWB_POST_R_GAIN));
	writel(pIsp->awb_gain[isp_idx].g_gain,
		   (void __iomem *)(base + BAYER_OFFSET + REG_AWB_POST_G_GAIN));
	writel(pIsp->awb_gain[isp_idx].b_gain,
		   (void __iomem *)(base + BAYER_OFFSET + REG_AWB_POST_B_GAIN));
}

static inline void
odin_isp_update_cc_matrix(struct odin_isp_ctrl *pIsp, u32 base, u32 isp_idx)
{
	u32 mode = 0;
	u32 i    = 0;
	u32 offset = 0x0;

	mode = pIsp->cc_matrix[isp_idx].mode;

	if (mode == CC_COLOR) {
		for (i = 0; i < ODIN_ISP_CC_MATRIX_CNT; i++) {
			offset = (u32)(RGB_OFFSET + REG_CC_11 + (i << 2));

			writel(pIsp->cc_matrix[isp_idx].value[i],
				   (void __iomem *)(base + offset));
		}
	}
	else if (mode == CC_COLOR_OFFSET) {
		for (i = 0; i < ODIN_ISP_CC_4X3_MATRIX_CNT; i++) {
			if (i < 9) {
				offset = (u32)(RGB_OFFSET + REG_CC_11 + (i << 2));
			}
			else {
				offset = (u32)(RGB_OFFSET + REG_CC_R_OFFSET + ((i - 9) << 2));
			}

			writel(pIsp->cc_matrix[isp_idx].value[i],
				   (void __iomem *)(base + offset));
		}
	}
}

static inline void
odin_isp_update_ldc_global_gain(struct odin_isp_ctrl *pIsp, u32 base,
															u32 isp_idx)
{
	writel(pIsp->global_gain[isp_idx].gain,
		   (void __iomem *)(base + BAYER_OFFSET + REG_LDC_GLOBAL_GAIN));
	writel(pIsp->global_gain[isp_idx].drc_level,
		   (void __iomem *)(base + BAYER_OFFSET + REG_LDC_REMAP_RATIO));
	writel(pIsp->global_gain[isp_idx].on,
		   (void __iomem *)(base + BAYER_OFFSET + REG_LDC_REMAP_ON));
}

static inline void
odin_isp_update_ldc_second_gain(struct odin_isp_ctrl *pIsp, u32 base,
															u32 isp_idx)
{
	writel(pIsp->second_gain[isp_idx].r_gain,
		   (void __iomem *)(base + BAYER_OFFSET + REG_LDC2_R_GAIN));
	writel(pIsp->second_gain[isp_idx].g_gain,
		   (void __iomem *)(base + BAYER_OFFSET + REG_LDC2_G_GAIN));
	writel(pIsp->second_gain[isp_idx].b_gain,
		   (void __iomem *)(base + BAYER_OFFSET + REG_LDC2_B_GAIN));
}

void
odin_isp_set_hsl_curve_ctrl(u32 ctrl, u32 base)
{
	u32 control, upper_ctrl;

	control = readl((void __iomem *)(base + HSL_OFFSET + REG_CURVE_EN));

	upper_ctrl = ctrl & 0xFFFF0000;
	ctrl	   &= 0x0000FFFF;
	ctrl	   = ((ctrl << 16) & 0xFFF00000) | (ctrl & 0x0000000F);
	upper_ctrl = ((upper_ctrl & 0xFFF00000)  |
				 ((upper_ctrl >> 16) & 0x0000000F));
	control    = (control | ctrl) & (~upper_ctrl);

	writel(control, (void __iomem *)(base + HSL_OFFSET + REG_CURVE_EN));

	return;
}

void
odin_isp_get_afd_data(u32 base, void *buf)
{
	struct odin_isp_data_bunch *acc_buff = buf;
	u32 					   addr 	= base + STATISTIC_OFFSET;
	register u32			   i;

	writel(1, (void __iomem *)(addr + RED_STOP_UPDATE_FK_FOR_CPU_READ));

	for (i = 0; i < 16; i++) {
		acc_buff[1].afd.y_sum[i] =
			readl((void __iomem *)(addr + REG_AFD_WINDOW00_Y_SUM + (i * 4)));
	}

	writel(0, (void __iomem *)(addr + RED_STOP_UPDATE_FK_FOR_CPU_READ));

	return;
}

s32
odin_isp_write_gamma_table(u32 base, void *buf, u32 *flag)
{
	u32 				*gamma_table			 = buf;
	u32 				*gamma_curve_update_flag = flag;
	u32 				addr, ctrl, type, flag_type, i;
	u32 				ret 					 = CSS_SUCCESS;

	for (i = 0; i < ISP_GAMMA_CNT; i++) {
		if ((*gamma_curve_update_flag >> i) & ON) {
			type = i;

			*gamma_curve_update_flag &= ~(0x1 << i);

			addr = base + lut_gamma_table_add[type];

			flag_type = type >> 2;

			if (type > 8) {
				flag_type += (type & 0x3);
			}

			ctrl = readl((void __iomem *)(base +
				lut_gamma_curve_ctrl_add[flag_type]));

			odin_isp_set_gamma_ctrl(DISABLE_GAMMA_CURVE, flag_type, base);

			/* css_isp("gamma_table[%d](0x%08x), size(%d),
				gamma_curve_update_flag(%d)\n",
				type, gamma_table[type],
				lut_gamma_step_number[type >> 3],
				*gamma_curve_update_flag); */
			if (copy_from_user((void *)addr, (const void *)gamma_table[type],
				(sizeof(u16) * lut_gamma_step_number[type >> 3]))) {
					/* Always '0' */
				css_err("failed to copy memory from user to kernel(type:%d)",
					type);
				ret = -EFAULT;
			}

			odin_isp_set_gamma_ctrl(ctrl, flag_type, base);
		}
	}

	return ret;
}

s32
odin_isp_write_hsl_curve_table(u32 base, void *buf, u32 *flag)
{
	u32 				*hsl_table			   = buf;
	u32 				*hsl_curve_update_flag = flag;
	u32 				addr, type, ctrl, control;
	register u32		i;
	u32 				ret 				   = CSS_SUCCESS;

	for (i = 0; i < ISP_HSLCURVE_CNT; i++) {
		if ((*hsl_curve_update_flag >> i) & ON) {
			type = i;

			*hsl_curve_update_flag &= ~(0x1 << i);

			addr = base + lut_hsl_table_add[type];

			control = lut_hsl_table_enable[type];

			ctrl = readl((void __iomem *)(base + HSL_OFFSET + REG_CURVE_EN));

			odin_isp_set_hsl_curve_ctrl(control << 16, base);

			/*
			css_isp(KERN_ALERT "hsl_table[%d](0x%08x), size(%d),
				hsl_curve_update_flag(%d)\n",
				type, hsl_table[type],
				sizeof(u16) * lut_hsl_curv_step_num[type >> 1],
				*hsl_curve_update_flag);
			*/
			if (copy_from_user((void *)addr, (const void *)hsl_table[type],
				(sizeof(u16) * lut_hsl_curv_step_num[type >> 1]))) {
					/* 0 -> 0 -> 1 -> 1 */
				css_err("failed to copy memory from user to kernel(type:%d)",
					type);
				ret = -EFAULT;
			}

			writel(ctrl, (void __iomem *)(base + HSL_OFFSET + REG_CURVE_EN));
		}
	}

	return ret;
}

s32
odin_isp_rake_histogram_data(u32 isp_idx, u32 fill_idx)
{
	struct odin_isp_ctrl		 *isp		  = odin_isp_get_handler();
	struct odin_stat_l_histogram *his_buff	  = NULL;
	u32 						 *buff		  = NULL;
	u32 						 offset 	  = 0;
	u32 						 lh_step_size = 0;
	register u32				 i;

	offset	 = isp->isp_base[isp_idx] + HSL_OFFSET;
	his_buff = &isp->his_buff[isp_idx][0];

	/* Backup Current Control */
	lh_step_size = readl((void __iomem *)offset + REG_LH_STEP_SIZE);
	lh_step_size = 256 >> lh_step_size;

	/* HSL Block Update immediatly */
	writel(ON, (void __iomem *)(offset + REG_HSL_IMMEDIATE_UPDATE));

	/* Histo Update Off */
	writel(ON, (void __iomem *)(offset + REG_LH_UPDATE_CTRL));

	his_buff[fill_idx].hist_pxl_cnt = readl((void __iomem *)(offset + \
			REG_LH_TOTAL_PIXEL_COUNT));
	his_buff[fill_idx].hist_l_sum = readl((void __iomem *)(offset + \
			REG_LH_TOTAL_L_SUM));

	for (i = 0; i < lh_step_size; i++) {
		his_buff[fill_idx].level[i] = readl((void __iomem *)(offset + \
			REG_LH_CNT_PIX_BASE_ADDR + (i << 2)));
	}

	/* Restore Previous Control */
	writel(OFF, (void __iomem *)(offset + REG_LH_UPDATE_CTRL));
	writel(OFF, (void __iomem *)(offset + REG_HSL_IMMEDIATE_UPDATE));

	return SUCCESS;
}

u32
odin_isp_rake_statistic(u32 isp_idx)
{
	struct odin_isp_ctrl		*isp 	  = odin_isp_get_handler();
	struct odin_isp_data_bunch	*acc_buff  = NULL;
	register u32				*buff	  = NULL;
	register u32				*ysum_buf   = NULL;
	register u32				*hpfsum_buf = NULL;
	register u32				*lpfsum_buf = NULL;
	register u32				*rsum_buf   = NULL;
	register u32				*gsum_buf   = NULL;
	register u32				*bsum_buf   = NULL;
	register u32				i;
	register u32				offset	  = 0;
	u32 						intr_cnt   = 0;
	u32 						fill_idx   = 0;
	u8							stat_irq_mode = 0;
	u32							stat_irq_max_cnt = 0;
	s32 					   ret = CSS_SUCCESS;
#ifdef INTR_CHECK_THROUGH_GPIO
	static u32				   toggle = 0;
#endif

	offset	 = isp->isp_base[isp_idx] + STATISTIC_OFFSET;
	acc_buff = &isp->acc_buff[isp_idx][0];
	fill_idx = isp->fill_idx[isp_idx];
	intr_cnt = isp->intr_cnt[isp_idx];

    stat_irq_mode    = isp->stat_irq_mode[isp_idx];
    stat_irq_max_cnt = isp->stat_irq_max_cnt[isp_idx];

    if (intr_cnt > stat_irq_max_cnt) {
        css_isp("%d %d\n", intr_cnt, stat_irq_max_cnt);
        return CSS_FAILURE;
    }

	isp->fill_time[isp_idx][intr_cnt] = ktime_get();

#ifdef INTR_CHECK_THROUGH_GPIO
	if (toggle) {
		gpio_set_value(intr_check_gpio_num, 0);
		toggle = 0;
	} else {
		gpio_set_value(intr_check_gpio_num, 1);
		toggle = 1;
	}
#endif

	writel(1, (void __iomem *)(offset + REG_STATISTIC_ZONE_UPDATE_OFF));

	/* 0 ->16 -> 32 ->... -> 256 */
	ysum_buf   = &acc_buff[fill_idx].ae.y_sum    [(intr_cnt << 4)];
	hpfsum_buf = &acc_buff[fill_idx].edge.hpf_sum[(intr_cnt << 4)];
	lpfsum_buf = &acc_buff[fill_idx].edge.lpf_sum[(intr_cnt << 4)];
	rsum_buf   = &acc_buff[fill_idx].awb.r_sum   [(intr_cnt << 4)];
	gsum_buf   = &acc_buff[fill_idx].awb.g_sum   [(intr_cnt << 4)];
	bsum_buf   = &acc_buff[fill_idx].awb.b_sum   [(intr_cnt << 4)];

	for (i = 0; i < 16; i++) {
		ysum_buf[i]	 = readl((void __iomem *)(offset + REG_STAT_ZONE00_Y_SUM
			+ (i * STAT_ZONE_UNIT_SIZE)));
		hpfsum_buf[i] = readl((void __iomem *)(offset + REG_STAT_ZONE00_HPF_EDGE
			+ (i * STAT_ZONE_UNIT_SIZE)));
		lpfsum_buf[i] = readl((void __iomem *)(offset + REG_STAT_ZONE00_LPF_EDGE
			+ (i * STAT_ZONE_UNIT_SIZE)));
		rsum_buf[i]	 = readl((void __iomem *)(offset + REG_STAT_ZONE00_R_SUM
			+ (i * STAT_ZONE_UNIT_SIZE)));
		gsum_buf[i]	 = readl((void __iomem *)(offset + REG_STAT_ZONE00_G_SUM
			+ (i * STAT_ZONE_UNIT_SIZE)));
		bsum_buf[i]	 = readl((void __iomem *)(offset + REG_STAT_ZONE00_B_SUM
			+ (i * STAT_ZONE_UNIT_SIZE)));
	}

	/* Last STATISTIC Handling */
	if (intr_cnt == stat_irq_max_cnt) {
		buff = &acc_buff[fill_idx].wga.r_sum;

		*buff++ = readl((void __iomem *)(offset + REG_TOTAL_RED_SUM));
		*buff++ = readl((void __iomem *)(offset + REG_TOTAL_GREEN_SUM));
		*buff	= readl((void __iomem *)(offset + REG_TOTAL_BLUE_SUM));

		writel(1, (void __iomem *)(offset + REG_CTD_UPDATE_OFF));

		/* CTD GROUP */
		for (i = 0; i < 10; i++) {
			buff = &acc_buff[fill_idx].ctd[i].count;

			*buff++ = readl((void __iomem *)(offset +
				REG_CTD_PIXEL_COUNT_GROUP0 + (i << 4)));
			*buff++ = readl((void __iomem *)(offset +
				REG_CTD_PIXEL_RED_SUM_GROUP0 + (i << 4)));
			*buff++ = readl((void __iomem *)(offset +
				REG_CTD_PIXEL_GREEN_SUM_GROUP0 + (i << 4)));
			*buff++ = readl((void __iomem *)(offset +
				REG_CTD_PIXEL_BLUE_SUM_GROUP0 + (i << 4)));
		}

		/* CTD TOTAL */
		buff = &acc_buff[fill_idx].ctdtotal.count;

		*buff++ = readl((void __iomem *)(offset + REG_CTD_ENTIRE_PIXEL_SUM));
		*buff++ = readl((void __iomem *)(offset + REG_CTD_ENTIRE_PIXEL_R_SUM));
		*buff++ = readl((void __iomem *)(offset + REG_CTD_ENTIRE_PIXEL_G_SUM));
		*buff++ = readl((void __iomem *)(offset + REG_CTD_ENTIRE_PIXEL_B_SUM));

		writel(0, (void __iomem *)(offset + REG_CTD_UPDATE_OFF));
	}

	writel(0, (void __iomem *)(offset + REG_STATISTIC_ZONE_UPDATE_OFF));

	if (intr_cnt == stat_irq_max_cnt - 1) {
		/* Histogram Data */
		odin_isp_rake_histogram_data(isp_idx, fill_idx);
	}

	if (intr_cnt == stat_irq_max_cnt){
		odin_isp_enlarge_statistics(isp_idx);
	}

	return ret;
}

void
odin_isp_enlarge_statistics(u32 isp_idx)
{
	struct odin_isp_ctrl	   *isp = odin_isp_get_handler();
	struct odin_isp_data_bunch *acc_buff = NULL;
	u32 					   fill_idx = 0;
	u8						   destidx = 0, srcidx = 0, i = 0;
	u8						   stat_irq_mode = 0;
	u32 					   stat_irq_max_cnt = 0;

	acc_buff = &isp->acc_buff[isp_idx][0];
	fill_idx = isp->fill_idx[isp_idx];

	stat_irq_mode	 = isp->stat_irq_mode[isp_idx];
	stat_irq_max_cnt = isp->stat_irq_max_cnt[isp_idx];

	if (stat_irq_mode == ODIN_ISP_OPMODE_HALF) {
		for (i = 0; i <= STAT_INT_MAX_COUNT_HALF; i++) {

			css_isp("ODIN_ISP_OPMODE_HALF\n");

			srcidx = stat_irq_max_cnt - i;
			destidx = (stat_irq_max_cnt - i) * 2;

			/* Y Sum */
			memcpy(&acc_buff[fill_idx].ae.y_sum[((destidx + 0) << 4)], \
				   &acc_buff[fill_idx].ae.y_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].ae.y_sum[((destidx + 1) << 4)], \
				   &acc_buff[fill_idx].ae.y_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			/* HPF Sum */
			memcpy(&acc_buff[fill_idx].edge.hpf_sum[((destidx + 0) << 4)], \
				   &acc_buff[fill_idx].edge.hpf_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].edge.hpf_sum[((destidx+1)<<4)], \
				   &acc_buff[fill_idx].edge.hpf_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);

			/* LPF Sum */
			memcpy(&acc_buff[fill_idx].edge.lpf_sum[((destidx + 0) << 4)], \
				   &acc_buff[fill_idx].edge.lpf_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].edge.lpf_sum[((destidx+1)<<4)], \
				   &acc_buff[fill_idx].edge.lpf_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);

			/* R Sum */
			memcpy(&acc_buff[fill_idx].awb.r_sum[((destidx + 0) << 4)], \
				   &acc_buff[fill_idx].awb.r_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].awb.r_sum[((destidx+1)<<4)], \
				   &acc_buff[fill_idx].awb.r_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);

			/* G Sum */
			memcpy(&acc_buff[fill_idx].awb.g_sum[((destidx + 0) << 4)], \
				   &acc_buff[fill_idx].awb.g_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].awb.g_sum[((destidx+1)<<4)], \
				   &acc_buff[fill_idx].awb.g_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);

			/* B Sum */
			memcpy(&acc_buff[fill_idx].awb.b_sum[((destidx + 0) << 4)], \
				   &acc_buff[fill_idx].awb.b_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].awb.b_sum[((destidx+1)<<4)], \
				   &acc_buff[fill_idx].awb.b_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
		}
	}
	else if (stat_irq_mode == ODIN_ISP_OPMODE_QUATER) {
		for (i = 0; i <= STAT_INT_MAX_COUNT_QUATER; i++) {

			css_isp("STAT_INT_MAX_COUNT_QUATER\n");

			srcidx = stat_irq_max_cnt - i;
			destidx = (stat_irq_max_cnt - i) * 4;

			/* Y Sum */
			memcpy(&acc_buff[fill_idx].ae.y_sum[((destidx + 0) << 4)], \
				   &acc_buff[fill_idx].ae.y_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].ae.y_sum[((destidx + 1)<<4)], \
				   &acc_buff[fill_idx].ae.y_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].ae.y_sum[((destidx + 2)<<4)], \
				   &acc_buff[fill_idx].ae.y_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].ae.y_sum[((destidx + 3)<<4)], \
				   &acc_buff[fill_idx].ae.y_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);

			/* HPF Sum */
			memcpy(&acc_buff[fill_idx].edge.hpf_sum[((destidx + 0) << 4)], \
				   &acc_buff[fill_idx].edge.hpf_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].edge.hpf_sum[((destidx + 1)<<4)], \
				   &acc_buff[fill_idx].edge.hpf_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].edge.hpf_sum[((destidx + 2)<<4)], \
				   &acc_buff[fill_idx].edge.hpf_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].edge.hpf_sum[((destidx + 3)<<4)], \
				   &acc_buff[fill_idx].edge.hpf_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);

			/* LPF Sum */
			memcpy(&acc_buff[fill_idx].edge.lpf_sum[((destidx + 0) << 4)], \
				   &acc_buff[fill_idx].edge.lpf_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].edge.lpf_sum[((destidx + 1)<<4)], \
				   &acc_buff[fill_idx].edge.lpf_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].edge.lpf_sum[((destidx + 2)<<4)], \
				   &acc_buff[fill_idx].edge.lpf_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].edge.lpf_sum[((destidx + 3)<<4)], \
				   &acc_buff[fill_idx].edge.lpf_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);

			/* R Sum */
			memcpy(&acc_buff[fill_idx].awb.r_sum[((destidx + 0) << 4)], \
				   &acc_buff[fill_idx].awb.r_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].awb.r_sum[((destidx + 1)<<4)], \
				   &acc_buff[fill_idx].awb.r_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].awb.r_sum[((destidx + 2)<<4)], \
				   &acc_buff[fill_idx].awb.r_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].awb.r_sum[((destidx + 3)<<4)], \
				   &acc_buff[fill_idx].awb.r_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);

			/* G Sum */
			memcpy(&acc_buff[fill_idx].awb.g_sum[((destidx + 0) << 4)], \
				   &acc_buff[fill_idx].awb.g_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].awb.g_sum[((destidx + 1)<<4)], \
				   &acc_buff[fill_idx].awb.g_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].awb.g_sum[((destidx + 2)<<4)], \
				   &acc_buff[fill_idx].awb.g_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].awb.g_sum[((destidx + 3)<<4)], \
				   &acc_buff[fill_idx].awb.g_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);

			/* B Sum */
			memcpy(&acc_buff[fill_idx].awb.b_sum[((destidx + 0) << 4)], \
				   &acc_buff[fill_idx].awb.b_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].awb.b_sum[((destidx + 1)<<4)], \
				   &acc_buff[fill_idx].awb.b_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].awb.b_sum[((destidx + 2)<<4)], \
				   &acc_buff[fill_idx].awb.b_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
			memcpy(&acc_buff[fill_idx].awb.b_sum[((destidx + 3)<<4)], \
				   &acc_buff[fill_idx].awb.b_sum[(srcidx << 4)], \
				   sizeof(u32) * 16);
		}
	}

	return;
}

s32
odin_isp_tp_control(int id, int enable)
{
	struct odin_isp_ctrl *isp	 = odin_isp_get_handler();
	struct isp_hardware  *isp_hw = ctrl_to_hw(isp);
	u32 				 base	 = isp->isp_base[id];
	unsigned long		 flags	 = 0;

	spin_lock_irqsave(&isp_hw->slock[id], flags);

	if (enable) {
			writel(0x1, (void __iomem *)(base
				+ TEST_OFFSET + REG_TESTPATTERN_ON));
	} else {
			writel(0x0, (void __iomem *)(base
				+ TEST_OFFSET + REG_TESTPATTERN_ON));
	}

	spin_unlock_irqrestore(&isp_hw->slock[id], flags);

	return CSS_SUCCESS;
}

u32
odin_isp_get_vsync_count(int id)
{
	struct odin_isp_ctrl *isp = odin_isp_get_handler();
	u32    count			  = 0;

	if (id >= ISP0 && id < ISP_MAX) {
		count = isp->vsync_cnt[id];
	}

	return count;
}

u32
odin_isp_get_dropped_3a(int id)
{
	struct odin_isp_ctrl *isp	 = odin_isp_get_handler();
	struct isp_hardware  *isp_hw = NULL;
	unsigned long		 flags	 = 0;
	int count	= 0;


	if (isp) {
		isp_hw = ctrl_to_hw(isp);
		if (isp_hw) {
			if (id >= ISP0 && id < ISP_MAX) {
				spin_lock_irqsave(&isp_hw->slock[id], flags);
				count = isp->drop_3a[id];
				isp->drop_3a[id] = 0;
				spin_unlock_irqrestore(&isp_hw->slock[id], flags);
			}
		}
	}

	return count;
}

s32
odin_isp_get_bad_statistic(int id)
{
	struct odin_isp_ctrl *isp	 = odin_isp_get_handler();
	struct isp_hardware  *isp_hw = NULL;
	unsigned long		 flags	 = 0;
	int count = 0;

	if (isp) {
		isp_hw = ctrl_to_hw(isp);
		if (isp_hw) {
			if (id >= ISP0 && id < ISP_MAX) {
				spin_lock_irqsave(&isp_hw->slock[id], flags);
#ifdef STAT_INT_TIME_CHECK
				count = isp->bad_statistic[id];
#else
				count = -1;
#endif
				isp->bad_statistic[id] = 0;
				spin_unlock_irqrestore(&isp_hw->slock[id], flags);
			}
		}
	}

	return count;
}

s32
odin_isp_get_bayer_statistic(int id, struct odin_isp_bayer_statistic *data)
{
	u32 ret = CSS_FAILURE;
	struct odin_isp_ctrl *isp = odin_isp_get_handler();

	/* dead_error_condition: The condition "id >= 2" cannot be true */
	/* if (id < ISP0 && id >= ISP_MAX)
		return ret; */

	if (id < ISP0)
		return ret;

	if (data == NULL)
		return ret;

	if (isp && isp->cssdev) {
		struct capture_fh *capfh  = NULL;
		struct css_zsl_device *zsl_dev = NULL;
		struct css_bufq *bayer_bufq = NULL;
		struct css_buffer	  *cssbuf = NULL;

		unsigned int queued_cnt = 0;
		unsigned int stat_data = 0;
		int fileid = 0;

		if (capture_get_path_mode(isp->cssdev) == \
					CSS_LIVE_PATH_MODE) {
			id = 0;
		}

		capfh = (struct capture_fh *)isp->cssdev->capfh[id];

		if (capfh == NULL) {
	        css_err("capfh is NULL!!\n");
			return CSS_ERROR_CAPTURE_HANDLE;
		}

	    if (capfh->main_scaler > CSS_SCALER_1) {
	        css_err("stat invalid scaler number!!\n");
	        return CSS_ERROR_INVALID_SCALER_NUM;
	    }

	    zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);

	    if (zsl_dev == NULL) {
	        css_err("stat zsl[%d] device null!!\n", zsl_dev->path);
	        return CSS_ERROR_ZSL_DEVICE;
	    }

		if (!zsl_dev->config.enable) {
			css_warn("stat zsl[%d] not enabled\n", zsl_dev->path);
			return CSS_ERROR_ZSL_NOT_ENABLED;
		}

		bayer_bufq = &zsl_dev->raw_buf;

		cssbuf = bayer_buffer_get_frame(bayer_bufq, CSS_BUF_POP_TOP);

		if (cssbuf == NULL)
			return ret;

		queued_cnt = bayer_buffer_get_queued_count(bayer_bufq);

		if (queued_cnt > 0) {
			struct css_buffer *tmp_buf = NULL;
			do
			{
				tmp_buf = bayer_buffer_get_frame(bayer_bufq, CSS_BUF_POP_TOP);
				if (tmp_buf != NULL)
					bayer_buffer_release_frame(bayer_bufq, tmp_buf);
			} while (tmp_buf != NULL);
		}

		data->frmcnt = cssbuf->frame_cnt;

		if (cssbuf->cpu_addr) {
			stat_data = (u32)cssbuf->cpu_addr;
			memcpy(&data->data[0], (char*)stat_data,
					CSS_SENSOR_STATISTIC_SIZE);
		} else {
			bayer_buffer_release_frame(bayer_bufq, cssbuf);
			return CSS_ERROR_ZSL_STAT_NOT_READY;
		}

		bayer_buffer_release_frame(bayer_bufq, cssbuf);

		css_zsl_low("sens stat:(%d) %d %02x%02x%02x%02x\n", queued_cnt,
					data->frmcnt, data->data[0], data->data[1], data->data[2],
					data->data[3]);

		ret = CSS_SUCCESS;
	}

	return ret;
}

static int g_torch_on  = 0;
static int g_torch_off = 0;
static int g_led_high  = 0;
static int g_led_low   = 0;
static int g_strobe_on = 0;
static struct css_flash_control_args g_flash = {0,};

s32
odin_isp_control(struct file *p_file, u32 cmds, u32 arg)
{
	struct odin_isp_ctrl *isp	 = odin_isp_get_handler();
	struct odin_isp_arg  *camarg = (struct odin_isp_arg *)arg;
	struct css_flash_control_args *flash = &camarg->flash;
	u32 				 cmd	 = (cmds & ODIN_CMD_MASK);
	u32 				 ctrl	 = (cmds & ODIN_CTRL_MASK);
	s32 				 ret	 = CSS_SUCCESS;

	switch (cmd) {
	case ODIN_CMD_SET_STAT_INTERRUPT_COUNT:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			if (camarg->data == ODIN_ISP_OPMODE_QUATER) {
				isp->stat_irq_mode[ISP0] = ODIN_ISP_OPMODE_QUATER;
				isp->stat_irq_max_cnt[ISP0] = STAT_INT_MAX_COUNT_QUATER;
			} else if (camarg->data == ODIN_ISP_OPMODE_HALF) {
				isp->stat_irq_mode[ISP0] = ODIN_ISP_OPMODE_HALF;
				isp->stat_irq_max_cnt[ISP0] = STAT_INT_MAX_COUNT_HALF;
			} else if (camarg->data == ODIN_ISP_OPMODE_DEFAULT) {
				isp->stat_irq_mode[ISP0] = ODIN_ISP_OPMODE_DEFAULT;
				isp->stat_irq_max_cnt[ISP0] = STAT_INT_MAX_COUNT_DEFAULT;
			} else {
				isp->stat_irq_mode[ISP0] = ODIN_ISP_OPMODE_DEFAULT;
				isp->stat_irq_max_cnt[ISP0] = STAT_INT_MAX_COUNT_DEFAULT;
			}
			css_isp("ISP0 op mode : %d", isp->stat_irq_mode[ISP0]);
			break;

		case ODIN_ISP1_CTRL:
			if (camarg->data == ODIN_ISP_OPMODE_QUATER) {
				isp->stat_irq_mode[ISP1] = ODIN_ISP_OPMODE_QUATER;
				isp->stat_irq_max_cnt[ISP1] = STAT_INT_MAX_COUNT_QUATER;
			} else if (camarg->data == ODIN_ISP_OPMODE_HALF) {
				isp->stat_irq_mode[ISP1] = ODIN_ISP_OPMODE_HALF;
				isp->stat_irq_max_cnt[ISP1] = STAT_INT_MAX_COUNT_HALF;
			} else if (camarg->data == ODIN_ISP_OPMODE_DEFAULT) {
				isp->stat_irq_mode[ISP1] = ODIN_ISP_OPMODE_DEFAULT;
				isp->stat_irq_max_cnt[ISP1] = STAT_INT_MAX_COUNT_DEFAULT;
			} else {
				isp->stat_irq_mode[ISP1] = ODIN_ISP_OPMODE_DEFAULT;
				isp->stat_irq_max_cnt[ISP1] = STAT_INT_MAX_COUNT_DEFAULT;
			}
			css_isp("ISP1 op mode: %d", isp->stat_irq_mode[ISP1]);
			break;
		}
		break;

	case ODIN_CMD_SET_INTERRUPT_ENABLE:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			odin_isp_interrupt_enable(ISP0, IRQ_VSYNC);
			odin_isp_interrupt_enable(ISP0, IRQ_STATIC);
			break;
		case ODIN_ISP1_CTRL:
			odin_isp_interrupt_enable(ISP1, IRQ_VSYNC);
			odin_isp_interrupt_enable(ISP1, IRQ_STATIC);
			break;
		}
		break;

	case ODIN_CMD_SET_INTERRUPT_DISABLE:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			odin_isp_interrupt_disable(ISP0, IRQ_STATIC);
			odin_isp_interrupt_disable(ISP0, IRQ_VSYNC);
			break;
		case ODIN_ISP1_CTRL:
			odin_isp_interrupt_disable(ISP1, IRQ_STATIC);
			odin_isp_interrupt_disable(ISP1, IRQ_VSYNC);
			break;
		break;
		}
		break;

	case ODIN_CMD_GET_ACC_BUFFER:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			if (copy_to_user((void __user *)camarg->offset,
							&isp->acc_buff[ISP0][isp->update_idx[ISP0]],
							sizeof(struct odin_isp_data_bunch))) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;

		case ODIN_ISP1_CTRL:
			if (copy_to_user((void __user *)camarg->offset,
							&isp->acc_buff[ISP1][isp->update_idx[ISP1]],
							sizeof(struct odin_isp_data_bunch))) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;

		default:
			ret = -EFAULT;
			break;
		}
		break;

	case ODIN_CMD_SENSOR_GET_STATISTIC:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			odin_isp_get_bayer_statistic(ISP0, &isp->bayer_stat[ISP0]);
			if (copy_to_user((void __user *)camarg->offset,
								&isp->bayer_stat[ISP0],
								sizeof(struct odin_isp_bayer_statistic))) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;
		case ODIN_ISP1_CTRL:
			odin_isp_get_bayer_statistic(ISP1, &isp->bayer_stat[ISP1]);
			if (copy_to_user((void __user *)camarg->offset,
								&isp->bayer_stat[ISP1],
								sizeof(struct odin_isp_bayer_statistic))) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;
		default:
			ret = -EFAULT;
			break;
		}
		break;

	case ODIN_CMD_GET_HISTOGRAM_BUFFER:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			if (copy_to_user((void __user *)camarg->offset,
							&isp->his_buff[ISP0][isp->update_idx[ISP0]],
							sizeof(struct odin_stat_l_histogram))) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;

		case ODIN_ISP1_CTRL:
			if (copy_to_user((void __user *)camarg->offset,
							&isp->his_buff[ISP1][isp->update_idx[ISP1]],
							sizeof(struct odin_stat_l_histogram))) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;

		default:
			ret = -EFAULT;
			break;
		}
		break;

	case ODIN_CMD_SET_UPDATE_TRIGGER:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			if (get_user(isp->update_trigger[ISP0],
						(u32 *)camarg->offset) < 0) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;

		case ODIN_ISP1_CTRL:
			if (get_user(isp->update_trigger[ISP1],
						(u32 *)camarg->offset) < 0) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;

		default:
			ret = -EFAULT;
			break;
		}
		break;

	case ODIN_CMD_SET_HSLCURVE:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			isp->hsl_curve_update_flag[ISP0] |= (0x00000001 << camarg->data);
			if (get_user(isp->hsl_table[ISP0][camarg->data],
						(u32 *)camarg->offset) < 0) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;

		case ODIN_ISP1_CTRL:
			isp->hsl_curve_update_flag[ISP1] |= (0x00000001 << camarg->data);
			if (get_user(isp->hsl_table[ISP1][camarg->data],
						(u32 *)camarg->offset) < 0) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;

		default:
			ret = CSS_FAILURE;
			break;
		}
		break;

	case ODIN_CMD_SET_GAMMA:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			isp->gamma_curve_update_flag[ISP0] |= (0x00000001 << camarg->data);
			if (get_user(isp->gamma_table[ISP0][camarg->data],
						(u32 *)camarg->offset) < 0) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;

		case ODIN_ISP1_CTRL:
			isp->gamma_curve_update_flag[ISP1] |= (0x00000001 << camarg->data);
			if (get_user(isp->gamma_table[ISP1][camarg->data],
						(u32 *)camarg->offset) < 0) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;

		default:
			ret = -EFAULT;
			break;
		}

		break;
	case ODIN_CMD_GET_VSYNC_COUNT:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			if ((put_user(isp->vsync_cnt[ISP0], (u32 *)camarg->offset)) < 0) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;

		case ODIN_ISP1_CTRL:
			if ((put_user(isp->vsync_cnt[ISP1], (u32 *)camarg->offset)) < 0) {
				css_err("failed to copy memory from kernel to user");
				ret = -EFAULT;
			}
			break;

		default:
			ret = -EFAULT;
			break;
		}

		break;
	case ODIN_CMD_ISP_PAUSE:
		{
			switch (ctrl) {
			case ODIN_ISP0_CTRL:
				css_clock_disable(CSS_CLK_ISP0);
				break;
			case ODIN_ISP1_CTRL:
				css_clock_disable(CSS_CLK_ISP1);
				break;
			}
		}
		break;
	case ODIN_CMD_ISP_RESUME:
		{
			switch (ctrl) {
			case ODIN_ISP0_CTRL:
				css_clock_enable(CSS_CLK_ISP0);
				break;
			case ODIN_ISP1_CTRL:
				css_clock_enable(CSS_CLK_ISP1);
				break;
			}
		}
		break;
	case TORCH_ON_VIA_ISP:
		if (flash->flash_current[0] > 0x7F
			|| flash->flash_current[1] > 0x7F)
		{
			flash->flash_current[0] = 0x7F;
			flash->flash_current[1] = 0x7F;
		}
		else
		{
			g_flash.flash_current[0] = flash->flash_current[0];
			g_flash.flash_current[1] = flash->flash_current[1];
		}

#ifdef CONFIG_VIDEO_ODIN_FLASH
		g_flash.mode = CSS_TORCH_ON;
		flash_control(&g_flash);
#endif
		break;
	case TORCH_OFF_VIA_ISP:
#ifdef CONFIG_VIDEO_ODIN_FLASH
		g_flash.mode = CSS_TORCH_OFF;
		flash_control(&g_flash);
#endif
		break;
	case LED_HIGH_VIA_ISP:
#ifdef CONFIG_VIDEO_ODIN_FLASH
		flash_hw_init();
#endif
		break;
	case LED_LOW_VIA_ISP:
#ifdef CONFIG_VIDEO_ODIN_FLASH
		flash_hw_deinit();
#endif
		break;
	case PRE_FLASH_ON_VIA_ISP:
		g_torch_on = 1;

		if (flash->flash_current[0] > 0x7F
			|| flash->flash_current[1] > 0x7F)
		{
			flash->flash_current[0] = 0x7F;
			flash->flash_current[1] = 0x7F;
		}
		else
		{
			g_flash.flash_current[0] = flash->flash_current[0];
			g_flash.flash_current[1] = flash->flash_current[1];
		}

		break;
	case PRE_FLASH_OFF_VIA_ISP:
		g_torch_off = 1;
		break;
	case STROBE_ON_VIA_ISP:
		g_strobe_on = 1;

		if (flash->flash_current[0] > 0x7F
			|| flash->flash_current[1] > 0x7F)
		{
			flash->flash_current[0] = 0x7F;
			flash->flash_current[1] = 0x7F;
		}
		else
		{
			g_flash.flash_current[0] = flash->flash_current[0];
			g_flash.flash_current[1] = flash->flash_current[1];
		}

		break;
	case ODIN_CMD_SET_AWB_GAIN:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			if (copy_from_user((void *)&isp->awb_gain[ISP0],
							   (const void *)camarg->offset,
							    sizeof(struct odin_isp_awb_gain))) {
				css_err("failed to copy memory from user to kernel");
				ret = -EFAULT;
			}
			break;

		case ODIN_ISP1_CTRL:
			if (copy_from_user((void *)&isp->awb_gain[ISP1],
							   (const void *)camarg->offset,
							   sizeof(struct odin_isp_awb_gain))) {
				css_err("failed to copy memory from user to kernel");
				ret = -EFAULT;
			}
			break;

		default:
			ret = -EFAULT;
			break;
		}

		break;
	case ODIN_CMD_SET_CC_MATRIX:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			if (copy_from_user((void *)&isp->cc_matrix[ISP0],
							   (const void *)camarg->offset,
							   sizeof(struct odin_isp_cc_matrix))) {
				css_err("failed to copy mefrom user to kernel");
				ret = -EFAULT;
			}
			break;

		case ODIN_ISP1_CTRL:
			if (copy_from_user((void *)&isp->cc_matrix[ISP1],
							   (const void *)camarg->offset,
							   sizeof(struct odin_isp_cc_matrix))) {
				css_err("failed to copy memory from user to kernel");
				ret = -EFAULT;
			}
			break;

		default:
			ret = -EFAULT;
			break;
		}

		break;
	case ODIN_CMD_SET_LDC_GLOBAL_GAIN:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			if (copy_from_user((void *)&isp->global_gain[ISP0],
							   (const void *)camarg->offset,
							    sizeof(struct odin_isp_ldc_global_gain))) {
				css_err("failed to copy memory from user to kernel");
				ret = -EFAULT;
			}
			break;

		case ODIN_ISP1_CTRL:
			if (copy_from_user((void *)&isp->global_gain[ISP1],
							   (const void *)camarg->offset,
							   sizeof(struct odin_isp_ldc_global_gain))) {
				css_err("failed to copy memory from user to kernel");
				ret = -EFAULT;
			}
			break;

		default:
			ret = -EFAULT;
			break;
		}

		break;
	case ODIN_CMD_SET_LDC_SECOND_GAIN:
		switch (ctrl) {
		case ODIN_ISP0_CTRL:
			if (copy_from_user((void *)&isp->second_gain[ISP0],
							   (const void *)camarg->offset,
							    sizeof(struct odin_isp_ldc_second_gain))) {
				css_err("failed to copy memory from user to kernel");
				ret = -EFAULT;
			}
			break;

		case ODIN_ISP1_CTRL:
			if (copy_from_user((void *)&isp->second_gain[ISP1],
							   (const void *)camarg->offset,
							   sizeof(struct odin_isp_ldc_second_gain))) {
				css_err("failed to copy memory from user to kernel");
				ret = -EFAULT;
			}
			break;

		default:
			ret = -EFAULT;
			break;
		}

		break;
	default:
		ret = -EFAULT;
		break;
	}

	if (ret < 0) {
		css_err("Invaild ISP Control Command %x\n", cmd);
	}

	return ret;
}

irqreturn_t
odin_isp_irq_vsync0(s32 irq, void *dev_id)
{
	struct odin_isp_ctrl *isp	 = odin_isp_get_handler();
	struct isp_hardware  *isp_hw = ctrl_to_hw(isp);
	u32 				 base	 = isp->isp_base[ISP0];
	u32 				 offset  = base + TEST_OFFSET;
	unsigned long		 status  = 0;
	unsigned long		 flags	 = 0;
	unsigned long		 isvalid = 1;

	spin_lock_irqsave(&isp_hw->slock[ISP0], flags);

	status = odin_isp_vsync_intr_status(offset);
	if (status & 0x02) {
		if (g_torch_on == 1)
		{
			g_flash.mode = CSS_TORCH_ON;
#ifdef CONFIG_VIDEO_ODIN_FLASH
			flash_control(&g_flash);
#endif
			g_torch_on = 0;
		}
		else if (g_torch_off == 1)
		{
			g_flash.mode = CSS_TORCH_OFF;
#ifdef CONFIG_VIDEO_ODIN_FLASH
			flash_control(&g_flash);
#endif
			g_torch_off = 0;
		}
		else if (g_strobe_on == 1)
		{
			g_flash.mode = CSS_LED_LOW;
#ifdef CONFIG_VIDEO_ODIN_FLASH
			flash_control(&g_flash);
#endif
			g_strobe_on = 0;
		}

		atomic_inc(&frame_cnt);
		isp->vsync_cnt[ISP0]++;

		odin_isp_update_awb_gain(isp, base, ISP0);
		odin_isp_update_cc_matrix(isp, base, ISP0);
		odin_isp_update_ldc_global_gain(isp, base, ISP0);
		odin_isp_update_ldc_second_gain(isp, base, ISP0);

		if (isp->intr_cnt[ISP0] > isp->stat_irq_max_cnt[ISP0]) {
#ifdef STAT_INT_TIME_CHECK
			{
				int i = 0;
				ktime_t delta, stime, etime;
				unsigned long long duration;
				unsigned long long max_duration = 0;
				unsigned int tot_duration = 0;

				for (i = 0; i < isp->intr_cnt[ISP0]; i++) {
					if (i == isp->stat_irq_max_cnt[ISP0]) {
						break;
					}
					etime = isp->fill_time[ISP0][i+1];
					stime = isp->fill_time[ISP0][i];
					delta = ktime_sub(etime, stime);
					duration = (unsigned long long) ktime_to_ns(delta) >> 10;
					max_duration = CSS_MAX(max_duration, duration);
					tot_duration += (unsigned int)duration;
				}

				if (isp->avg_time[ISP0] == 0) {
					isp->avg_time[ISP0] = (tot_duration / isp->intr_cnt[ISP0]);
				}

				if (max_duration > ((isp->avg_time[ISP0] * 1600) >> 10)) {
					css_sys("watch! %lld\n", max_duration);
					isp->bad_statistic[ISP0]++;
					isvalid = 0;
				} else {
					isvalid = 1;
				}
				memset(&isp->fill_time[ISP0][0], 0x0, 16 * sizeof(ktime_t));
			}
#endif
			isp->intr_cnt[ISP0] 	 = 0;
			isp->remain_statistic[ISP0] = 0;

			if (isvalid) {
				isp->update_idx[ISP0] = isp->fill_idx[ISP0];
				isp->fill_idx[ISP0] = (isp->fill_idx[ISP0] + 1) & 1;
#ifdef CONFIG_VIDEO_ISP_MISC_DRIVER
				complete(&isp->isp_int_done[ISP0]);
#endif
			} else {
				isp->drop_3a[ISP0]++;
			}
		} else {
			int remain_int = (isp->stat_irq_max_cnt[ISP0] + 1)
							- isp->intr_cnt[ISP0];

			memset(&isp->acc_buff[ISP0][isp->fill_idx[ISP0]], 0x0,
					sizeof(struct odin_isp_data_bunch));
			memset(&isp->his_buff[ISP0][isp->fill_idx[ISP0]], 0x0,
					sizeof(struct odin_stat_l_histogram));

			isp->intr_cnt[ISP0] 	 = 0;
			isp->remain_statistic[ISP0] = remain_int;
			css_sys("r:%d\n", remain_int);
			isp->drop_3a[ISP0]++;

		}
		complete(&isp->isp_wait_done[ISP0]);
		odin_isp_vsync_intr_clear(0x02, offset);
	}

	if (status & 0x01) {
		mdis_fill_idx = (mdis_fill_idx + 1) & 1;
		atomic_set(&mdis_cnt, 0);

#if 0
		if (isp->update_trigger[ISP0]) {
			odin_isp_set_reg_imm_update(ON, ISP_BAYER_BLOCK, base);
			odin_isp_set_reg_imm_update(ON, ISP_RGB_BLOCK  , base);
			odin_isp_set_reg_imm_update(ON, ISP_HSL_BLOCK  , base);
			odin_isp_set_reg_imm_update(ON, ISP_YUV_BLOCK  , base);

			if (isp->hsl_curve_update_flag[ISP0]) {
				odin_isp_write_hsl_curve_table(isp->isp_base[ISP0],
					&isp->hsl_table[ISP0],
					&isp->hsl_curve_update_flag[ISP0]);
			}

			/* The flag is always 0  - odin_set_gamma_table() in odin_isp.c */
			if (isp->gamma_curve_update_flag[ISP0]) {
				odin_isp_write_gamma_table(isp->isp_base[ISP0],
					&isp->gamma_table[ISP0],
					&isp->gamma_curve_update_flag[ISP0]);
			}

			odin_isp_set_reg_imm_update(OFF, ISP_BAYER_BLOCK, base);
			odin_isp_set_reg_imm_update(OFF, ISP_RGB_BLOCK	, base);
			odin_isp_set_reg_imm_update(OFF, ISP_HSL_BLOCK	, base);
			odin_isp_set_reg_imm_update(OFF, ISP_YUV_BLOCK	, base);

			isp->update_trigger[ISP0] = 0;
			css_isp("INTR: update_trigger(%d)\n", isp->update_trigger[ISP0]);

		}
#endif

		odin_isp_vsync_intr_clear(0x01, offset);
		odin_isp_get_afd_data(base, isp->acc_buff[ISP0]);
	}

	spin_unlock_irqrestore(&isp_hw->slock[ISP0], flags);

	return IRQ_HANDLED;
}

irqreturn_t
odin_isp_irq_statistic0(s32 irq, void *dev_id)
{
	struct odin_isp_ctrl *isp = odin_isp_get_handler();
	struct isp_hardware  *isp_hw = ctrl_to_hw(isp);
	unsigned long flags = 0;

	spin_lock_irqsave(&isp_hw->slock[ISP0], flags);
	odin_isp_rake_statistic(ISP0);
	isp->intr_cnt[ISP0]++;
	odin_isp_stat_intr_clear(0x01, isp->isp_base[ISP0] + STATISTIC_OFFSET);
	spin_unlock_irqrestore(&isp_hw->slock[ISP0], flags);

	return IRQ_HANDLED;
}

irqreturn_t
odin_isp_irq_vsync1(s32 irq, void *dev_id)
{
	struct odin_isp_ctrl *isp	 = odin_isp_get_handler();
	struct isp_hardware  *isp_hw = ctrl_to_hw(isp);
	u32 				 base	 = isp->isp_base[ISP1];
	u32 				 offset  = base + TEST_OFFSET;
	unsigned long		 status  = 0;
	unsigned long		 flags	 = 0;
	unsigned long		 isvalid = 1;

	spin_lock_irqsave(&isp_hw->slock[ISP1], flags);

	status = odin_isp_vsync_intr_status(offset);
	if (status & 0x02) {
		if (g_torch_on == 1)
		{
			g_flash.mode = CSS_TORCH_ON;
#ifdef CONFIG_VIDEO_ODIN_FLASH
			flash_control(&g_flash);
#endif
			g_torch_on = 0;
		}
		else if (g_torch_off == 1)
		{
			g_flash.mode = CSS_TORCH_OFF;
#ifdef CONFIG_VIDEO_ODIN_FLASH
			flash_control(&g_flash);
#endif
			g_torch_off = 0;
		}
		else if (g_strobe_on == 1)
		{
			g_flash.mode = CSS_LED_LOW;
#ifdef CONFIG_VIDEO_ODIN_FLASH
			flash_control(&g_flash);
#endif
			g_strobe_on = 0;
		}

		isp->vsync_cnt[ISP1]++;

		odin_isp_update_awb_gain(isp, base, ISP1);
		odin_isp_update_cc_matrix(isp, base, ISP1);
		odin_isp_update_ldc_global_gain(isp, base, ISP1);
		odin_isp_update_ldc_second_gain(isp, base, ISP1);

		if (isp->intr_cnt[ISP1] > isp->stat_irq_max_cnt[ISP1]) {
#ifdef STAT_INT_TIME_CHECK
			{
				int i = 0;
				ktime_t delta, stime, etime;
				unsigned long long duration;
				unsigned long long max_duration = 0;
				unsigned int tot_duration = 0;

				for (i = 0; i < isp->intr_cnt[ISP1]; i++) {
					if (i == isp->stat_irq_max_cnt[ISP1]) {
						break;
					}
					etime = isp->fill_time[ISP1][i+1];
					stime = isp->fill_time[ISP1][i];
					delta = ktime_sub(etime, stime);
					duration = (unsigned long long) ktime_to_ns(delta) >> 10;
					max_duration = CSS_MAX(max_duration, duration);
					tot_duration += (unsigned int)duration;
				}

				if (isp->avg_time[ISP1] == 0) {
					isp->avg_time[ISP1] = (tot_duration / isp->intr_cnt[ISP1]);
				}

				if (max_duration > ((isp->avg_time[ISP1] * 1600) >> 10)) {
					css_sys("watch! %lld\n", max_duration);
					isp->bad_statistic[ISP1]++;
					isvalid = 0;
				} else {
					isvalid = 1;
				}
				memset(&isp->fill_time[ISP1][0], 0x0, sizeof(ktime_t) * 16);
			}
#endif
			isp->intr_cnt[ISP1] 	 = 0;
			isp->remain_statistic[ISP1] = 0;

			if (isvalid) {
				isp->update_idx[ISP1] = isp->fill_idx[ISP1];
				isp->fill_idx[ISP1]   = (isp->fill_idx[ISP1] + 1) & 1;
#ifdef CONFIG_VIDEO_ISP_MISC_DRIVER
				// complete(&isp->isp_int_done[ISP1]);
#endif
			} else {
				isp->drop_3a[ISP1]++;
				// complete(&isp->isp_int_done[ISP1]);
			}
		} else {
			int remain_int = (isp->stat_irq_max_cnt[ISP1] + 1)
                             - isp->intr_cnt[ISP1];
/*
			memset(&isp->acc_buff[ISP1][isp->fill_idx[ISP1]], 0x0,
					sizeof(struct odin_isp_data_bunch));
			memset(&isp->his_buff[ISP1][isp->fill_idx[ISP1]], 0x0,
					sizeof(struct odin_stat_l_histogram));
*/
			isp->intr_cnt[ISP1] = 0;
			isp->remain_statistic[ISP1] = remain_int;
			css_sys("r:%d\n", remain_int);
			isp->drop_3a[ISP1]++;
		}
		complete(&isp->isp_int_done[ISP1]);
		odin_isp_vsync_intr_clear(0x02, offset);
	}

	if (status & 0x01) {
#if 0
		if (isp->update_trigger[ISP1]) {
			odin_isp_set_reg_imm_update(ON, ISP_BAYER_BLOCK, base);
			odin_isp_set_reg_imm_update(ON, ISP_RGB_BLOCK  , base);
			odin_isp_set_reg_imm_update(ON, ISP_HSL_BLOCK  , base);
			odin_isp_set_reg_imm_update(ON, ISP_YUV_BLOCK  , base);

			if (isp->hsl_curve_update_flag[ISP1]) {
				odin_isp_write_hsl_curve_table(isp->isp_base[ISP1],
					&isp->hsl_table[ISP1],
					&isp->hsl_curve_update_flag[ISP1]);
			}

			/* The flag is always 0  - odin_set_gamma_table() in odin_isp.c */
			if (isp->gamma_curve_update_flag[ISP1]) {
				odin_isp_write_gamma_table(isp->isp_base[ISP1],
					&isp->gamma_table[ISP1],
					&isp->gamma_curve_update_flag[ISP1]);
			}

			odin_isp_set_reg_imm_update(OFF, ISP_BAYER_BLOCK, base);
			odin_isp_set_reg_imm_update(OFF, ISP_RGB_BLOCK	, base);
			odin_isp_set_reg_imm_update(OFF, ISP_HSL_BLOCK	, base);
			odin_isp_set_reg_imm_update(OFF, ISP_YUV_BLOCK	, base);

			isp->update_trigger[ISP1] = 0;
			css_isp("INTR: update_trigger(%d)\n", isp->update_trigger[ISP1]);
		}
#endif

		odin_isp_vsync_intr_clear(0x01, offset);
		odin_isp_get_afd_data(base, isp->acc_buff[ISP1]);
	}

	spin_unlock_irqrestore(&isp_hw->slock[ISP1], flags);

	return IRQ_HANDLED;
}

irqreturn_t
odin_isp_irq_statistic1(s32 irq, void *dev_id)
{
	struct odin_isp_ctrl *isp = odin_isp_get_handler();
	struct isp_hardware  *isp_hw = ctrl_to_hw(isp);
	unsigned long flags = 0;

	spin_lock_irqsave(&isp_hw->slock[ISP1], flags);
	odin_isp_rake_statistic(ISP1);
	isp->intr_cnt[ISP1]++;
	odin_isp_stat_intr_clear(0x01, isp->isp_base[ISP1] + STATISTIC_OFFSET);
	spin_unlock_irqrestore(&isp_hw->slock[ISP1], flags);

	return IRQ_HANDLED;
}

s32
odin_isp_wait_vsync(u32 isp_idx, u32 msec)
{
	struct odin_isp_ctrl *isp = NULL;
	struct completion *wait = NULL;
	int timeout = 0;

	isp = odin_isp_get_handler();

	wait = &isp->isp_wait_done[isp_idx];

	INIT_COMPLETION(isp->isp_wait_done[isp_idx]);

	timeout = wait_for_completion_timeout(
			wait, msecs_to_jiffies(msec));

	if (timeout == 0) {
		css_sys("isp%02d vsync timeout!!\n", isp_idx);
	} else {
		css_sys("isp%02d vsync waitdone!!\n", isp_idx);
	}

	return timeout;
}

void
odin_isp_wait_vsync_poll(u32 isp_idx, u32 msec)
{
	unsigned int cur_cnt, frame_cnt, count;
	struct odin_isp_ctrl *isp = odin_isp_get_handler();

	if (!isp)
		return;
	count = msec * 100;
	cur_cnt = isp->intr_cnt[isp_idx];
	while (1) {
		frame_cnt = isp->intr_cnt[isp_idx];
		if (frame_cnt != cur_cnt)
			break;

		if (!count--) {
			css_info("-----> timeout");
			break;
		}
		usleep_range(10,10);
	}
}

void
odin_isp_init_device(int id)
{
	struct odin_isp_ctrl *isp	 = odin_isp_get_handler();
	struct isp_hardware  *isp_hw = ctrl_to_hw(isp);

	spin_lock_init(&isp_hw->slock[id]);

    return;
}

s32
odin_isp_deinit_device(int id)
{
	return CSS_SUCCESS;
}

static s32
odin_isp_init_resources(struct platform_device *pdev)
{
	struct isp_hardware *isp_hw  = NULL;
	struct resource 	*res_isp = NULL;
	unsigned long		size_isp = 0;
	s32 				ret 	 = CSS_SUCCESS;
	irq_handler_t		irq_vsync;
	irq_handler_t		irq_statistic;
	int idx = 0;

	const char *isp_vsync_int[2] = {"isp-vsync0", "isp-vsync1"};
	const char *isp_int[2]		 = {"isp0", "isp1"};

	idx = pdev->id;

	isp_hw = (struct isp_hardware *)&get_css_plat()->isp_hw;

	platform_set_drvdata(pdev, isp_hw);

	isp_hw->pdev[idx] = pdev;

	if (idx == ISP0) {
		irq_vsync	  = odin_isp_irq_vsync0;
		irq_statistic = odin_isp_irq_statistic0;
	} else {
		irq_vsync	  = odin_isp_irq_vsync1;
		irq_statistic = odin_isp_irq_statistic1;
	}

	res_isp = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == res_isp) {
		css_err("failed to get resource(isp%02d)!\n", idx);
		return CSS_ERROR_GET_RESOURCE;
	}
	size_isp = res_isp->end - res_isp->start + 1;

	isp_hw->iores[idx] = request_mem_region(res_isp->start,
											size_isp, pdev->name);
	if (NULL == isp_hw->iores[idx]) {
		css_err("failed to request mem region(isp%02d) : %s!\n",
			idx, pdev->name);
		ret = CSS_ERROR_GET_RESOURCE;
		goto error_get_resource;
	}

	isp_hw->iobase[idx] = ioremap_nocache(res_isp->start, size_isp);
	if (NULL == isp_hw->iobase[idx]) {
		css_err("failed to ioremap(isp%02d)!\n", idx);
		ret = CSS_ERROR_IOREMAP;
		goto error_ioremap;
	}

	isp_hw->irq_isp[idx] = platform_get_irq(pdev, 0);
	if (isp_hw->irq_isp[idx] < 0) {
		css_err("failed to get isp0 irq number(isp%02d)\n", idx);
		return CSS_ERROR_GET_RESOURCE;
	}

	ret = request_irq(isp_hw->irq_isp[idx], irq_statistic, 0,
						isp_int[idx], NULL);
	if (ret) {
		css_err("Failed to request isp0 irq %d(isp0)\n", ret);
		ret = CSS_ERROR_REQUEST_IRQ;
		goto error_irq;
	}

	disable_irq(isp_hw->irq_isp[idx]);

	isp_hw->irq_vsync[idx] = platform_get_irq(pdev, 1);
	if (isp_hw->irq_vsync[idx] < 0) {
		css_err("failed to get vsync%02 irq number(isp%02)\n", idx, idx);
		return CSS_ERROR_GET_RESOURCE;
	}

	ret = request_irq(isp_hw->irq_vsync[idx], irq_vsync, 0,
						isp_vsync_int[idx], NULL);
	if (ret) {
		css_err("Failed to request vsync0 irq %d(isp0)\n", ret);
		ret = CSS_ERROR_REQUEST_IRQ;
		goto error_irq;
	}

	disable_irq(isp_hw->irq_vsync[idx]);

	if (isp_ctrl == NULL) {
		isp_ctrl = kzalloc(sizeof(struct odin_isp_ctrl), GFP_KERNEL);
		if (NULL == isp_ctrl) {
			printk(KERN_ERR "Failed to alloc buffer of isp handler\n");
			ret = -ENOMEM;
			goto err_alloc;
		}
		isp_ctrl_info = isp_ctrl;
	}

	isp_ctrl->isp_fh[idx] = NULL;
	isp_ctrl->isp_base[idx] = (u32)isp_hw->iobase[idx];
	isp_ctrl->isp_hw		= isp_hw;

    if (isp_ctrl->cssdev == NULL)
		isp_ctrl->cssdev = get_css_plat();

	init_completion(&isp_ctrl->isp_int_done[idx]);
	init_completion(&isp_ctrl->isp_wait_done[idx]);
	mutex_init(&isp_ctrl->isp_mutex[idx]);

	isp_ctrl->probe_status |= (0x1 << idx);

	return ret;

err_alloc:
	if (NULL != isp_ctrl) {
			kfree(isp_ctrl);
			isp_ctrl = NULL;
	}
error_irq:
	if (isp_hw->irq_vsync[idx] > 0)
		free_irq(isp_hw->irq_vsync[idx], NULL);
	if (isp_hw->irq_isp[idx] > 0)
		free_irq(isp_hw->irq_isp[idx], NULL);

error_ioremap:
	if (NULL != isp_hw->iobase[idx])
		iounmap(isp_hw->iobase[idx]);

error_get_resource:
	if (NULL != isp_hw->iores[idx])
		release_mem_region(isp_hw->iores[idx]->start,
						   isp_hw->iores[idx]->end -
						   isp_hw->iores[idx]->start + 1);
	return ret;
}

static s32
odin_isp_deinit_resources(struct platform_device *pdev)
{
	struct isp_hardware *isp_hw = NULL;
	s32 ret = CSS_SUCCESS;
	int idx = 0;

	isp_hw = platform_get_drvdata(pdev);

	if (isp_hw->irq_vsync[idx] > 0) {
		free_irq(isp_hw->irq_vsync[idx], NULL);
		isp_hw->irq_vsync[idx] = 0;
	}

	if (isp_hw->irq_isp[idx] > 0) {
		free_irq(isp_hw->irq_isp[idx], NULL);
		isp_hw->irq_isp[idx] = 0;
	}

	if (NULL != isp_hw->iobase[idx]) {
		iounmap(isp_hw->iobase[idx]);
		isp_hw->iobase[idx] = NULL;
	}

	if (NULL != isp_hw->iores[idx]) {
		release_mem_region(isp_hw->iores[idx]->start,
						   isp_hw->iores[idx]->end -
						   isp_hw->iores[idx]->start + 1);
		isp_hw->iores[idx] = NULL;
	}

	isp_ctrl->probe_status &= ~(0x1 << idx);

	if (isp_ctrl != NULL) {
		if (isp_ctrl->probe_status == 0) {
			kfree(isp_ctrl);
			isp_ctrl = NULL;
		}
	}

	return ret;
}

static void
take_affinity(u32 irq_num)
{
	cpumask_var_t new_value;

	/* SW workaround for irq affinity */
	cpumask_clear(new_value);
	cpumask_set_cpu(CSS_ISP_INT_CPU, new_value);
	irq_set_affinity(irq_num, new_value);
	/* SW workaround for irq affinity */
}

#ifdef CONFIG_VIDEO_ISP_MISC_DRIVER

static s32
isp_data_init(s32 idx, struct odin_isp_ctrl *isp_ctrl)
{
	s32 ret = CSS_SUCCESS;
	if (isp_ctrl) {
        isp_ctrl->stat_irq_mode[idx]           = ODIN_ISP_OPMODE_DEFAULT;
        isp_ctrl->stat_irq_max_cnt[idx]        = STAT_INT_MAX_COUNT_DEFAULT;
		isp_ctrl->vsync_cnt[idx]               = 0;
		isp_ctrl->intr_cnt[idx]                = 0;
		isp_ctrl->irq_cnt[idx]                 = 0;
		isp_ctrl->remain_statistic[idx]        = 0;
		isp_ctrl->bad_statistic[idx]	       = 0;
		isp_ctrl->drop_3a[idx]			       = 0;
		isp_ctrl->fill_idx[idx] 		       = 0;
		isp_ctrl->avg_time[idx] 		       = 0;
		isp_ctrl->update_idx[idx]		       = 0;
		isp_ctrl->update_trigger[idx]          = 0;
		isp_ctrl->hsl_curve_update_flag[idx]   = 0;
		isp_ctrl->gamma_curve_update_flag[idx] = 0;

		memset(&isp_ctrl->fill_time[idx][0], 0x0, sizeof(ktime_t) * 16);
		memset(&isp_ctrl->acc_buff[idx][0], 0x0,
				sizeof(struct odin_isp_data_bunch));
		memset(&isp_ctrl->acc_buff[idx][1], 0x0,
				sizeof(struct odin_isp_data_bunch));
		memset(&isp_ctrl->his_buff[idx][0], 0x0,
				sizeof(struct odin_stat_l_histogram));
		memset(&isp_ctrl->his_buff[idx][1], 0x0,
				sizeof(struct odin_stat_l_histogram));
	}

	return ret;
}

static s32
isp_open(struct inode *inode, struct file *file)
{
	struct odin_isp_fh *isp_fh = NULL;
	s32 			   ret	   = 0;
	s32 			   idx	   = -1;
	s32 			   i;


	if (isp_ctrl == NULL) {
		return -EPERM;
    }

	for (i = 0; i < ARRAY_SIZE(isp_dev_name); i++) {
		if (strstr(file->f_path.dentry->d_iname, isp_dev_name[i].dev_name)) {
			idx = isp_dev_name[i].index;
		}
	}

	if (idx < ISP0 || idx >= ISP_MAX)
		return -EINVAL;

	mutex_lock(&isp_ctrl->isp_mutex[idx]);

	if (isp_ctrl->isp_fh[idx]) {
		css_warn("already opened isp%2d file\n", idx);
		mutex_unlock(&isp_ctrl->isp_mutex[idx]);
		return -EBUSY;
	}

	css_info("odin isp %d dev open\n", idx);

	isp_fh = (struct odin_isp_fh*)kzalloc(sizeof(struct odin_isp_fh), \
										GFP_KERNEL);
	if (isp_fh) {
		isp_ctrl->isp_fh[idx] = isp_fh;
		isp_fh->idx 	 = idx;
		isp_fh->isp_ctrl = isp_ctrl;
		isp_data_init(idx, isp_ctrl);
		init_completion(&isp_ctrl->isp_int_done[idx]);
		file->private_data = isp_fh;
	} else {
		ret = -ENOMEM;
	}

	if (ret == CSS_SUCCESS) {
#ifdef INTR_CHECK_THROUGH_GPIO
			if (idx == ISP0) {
				intr_check_gpio_num = 181;
			} else if (idx == ISP1) {
				intr_check_gpio_num = 179;
			} else {
				mutex_unlock(&isp_ctrl->isp_mutex[idx]);
				return ret;
			}
			if (gpio_request(intr_check_gpio_num, "odin_isp") < 0) {
				css_err("gpio_request fail\n");
				isp_ctrl->isp_fh[idx] = NULL;
				kzfree(isp_fh);
				mutex_unlock(&isp_ctrl->isp_mutex[idx]);
				return ret;
			}
			gpio_direction_output(intr_check_gpio_num, 1);
#endif
      mutex_lock(&devinit);
		if (idx == 0) {
			css_power_domain_on(POWER_ISP0);
		} else {
			css_power_domain_on(POWER_ISP1);
		}

		odin_isp_init_device(idx);

		if (idx == 0) {
			css_clock_enable(CSS_CLK_ISP0);
			css_block_reset(BLK_RST_ISP0);
		} else {
			css_clock_enable(CSS_CLK_ISP1);
			css_block_reset(BLK_RST_ISP1);
		}

		odin_isp_interrupt_enable(idx, IRQ_VSYNC);
		odin_isp_interrupt_enable(idx, IRQ_STATIC);

		take_affinity(isp_ctrl->isp_hw->irq_isp[isp_fh->idx]);
		take_affinity(isp_ctrl->isp_hw->irq_vsync[isp_fh->idx]);

		enable_irq(isp_ctrl->isp_hw->irq_isp[isp_fh->idx]);
		enable_irq(isp_ctrl->isp_hw->irq_vsync[isp_fh->idx]);
		mutex_unlock(&devinit);
	}

	mutex_unlock(&isp_ctrl->isp_mutex[idx]);
	return ret;
}

static int isp_release(struct inode *ignored, struct file *file)
{
	int ret = 0;
	struct odin_isp_fh	 *isp_fh   = NULL;
	int idx = 0;

	if (file->private_data) {
		isp_fh	 = file->private_data;
		idx = isp_fh->idx;

		mutex_lock(&isp_ctrl->isp_mutex[idx]);
      	mutex_lock(&devinit);
		css_info("odin isp %d dev release\n", idx);

		odin_isp_interrupt_disable(isp_fh->idx, IRQ_STATIC);
		odin_isp_interrupt_disable(isp_fh->idx, IRQ_VSYNC);

		disable_irq(isp_ctrl->isp_hw->irq_isp[idx]);
		disable_irq(isp_ctrl->isp_hw->irq_vsync[idx]);

		if (idx == ISP0) {
			css_clock_disable(CSS_CLK_ISP0);
		} else {
			css_clock_disable(CSS_CLK_ISP1);
		}

		odin_isp_deinit_device(idx);
		if (idx == ISP0) {
			css_power_domain_off(POWER_ISP0);
		} else {
			css_power_domain_off(POWER_ISP1);
		}

		mutex_unlock(&devinit);
		if (isp_fh) {
			isp_ctrl->isp_fh[idx] = NULL;
			kzfree(isp_fh);
			isp_fh = NULL;
			file->private_data = NULL;
		}

#ifdef INTR_CHECK_THROUGH_GPIO
		gpio_free(intr_check_gpio_num);
#endif

		mutex_unlock(&isp_ctrl->isp_mutex[idx]);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static s64
isp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct css_isp_control_args *args = (struct css_isp_control_args *)arg;
	u32 						isp_cmd   = 0;
	u32 						isp_arg   = 0;

	switch (cmd) {
	case CSS_IOC_ISP_CONTROL:
		isp_cmd = args->c.left;
		isp_arg = (unsigned int)&args->c.top;

		if (CSS_FAILURE == odin_isp_control(file, isp_cmd, isp_arg))
		   goto invalid_ioctl;
		break;

	default:
		css_err("Invaild ioctl command\n");
		goto invalid_ioctl;
	}

	return 0;

invalid_ioctl:
	return -EINVAL;
}

static s32
isp_mmap(struct file *file, struct vm_area_struct *vma)
{
	u64 start  = vma->vm_start;
	u64 size   = PAGE_ALIGN(vma->vm_end - vma->vm_start);
	u64 offset = vma->vm_pgoff << PAGE_SHIFT;
	u64 pfn    = __phys_to_pfn(offset);
	u64 prot   = vma->vm_page_prot;
	s32 ret    = 0;

	switch (offset) {
	case MMAP_OFFSET_ISP1:
	case MMAP_OFFSET_ISP2:
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,6,0))
		vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
#else
		vma->vm_flags |= (VM_IO | VM_RESERVED);
#endif
		prot = pgprot_noncached(prot);

		if (remap_pfn_range(vma, start, pfn, size, prot)) {
			css_err("failed to remap kernel memory for ISP to user space \n");
			return -EAGAIN;
		}
		break;

	default:
		if (remap_pfn_range(vma, start, pfn, size, prot)) {
			css_err("failed to remap kernel memory for" \
				"frame buffer to user space \n");
			return -EAGAIN;
		}
		break;
	}
	return ret;
}

u32
isp_poll(struct file *file, poll_table *wait)
{
	struct odin_isp_fh	 *isp_fh   = file->private_data;
	struct odin_isp_ctrl *isp_ctrl = NULL;
	u32 				 poll_mask = 0;

	if (isp_fh) {
		isp_ctrl = isp_fh->isp_ctrl;
		if (isp_ctrl->isp_int_done[isp_fh->idx].done) {
			poll_mask |= POLLIN | POLLRDNORM;
			INIT_COMPLETION(isp_ctrl->isp_int_done[isp_fh->idx]);
		} else {
			poll_wait(file, &isp_ctrl->isp_int_done[isp_fh->idx].wait, wait);
		}
	}

	return poll_mask;
}

static struct file_operations isp_fops = {
	.owner			= THIS_MODULE,
	.open			= isp_open,
	.release		= isp_release,
	.unlocked_ioctl = isp_ioctl,
	.mmap			= isp_mmap,
	.poll			= isp_poll,
};

static struct miscdevice isp_miscdevice[ISP_MAX] = {
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name  = "isp0",
		.fops  = &isp_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name  = "isp1",
		.fops  = &isp_fops,
	}
};

#endif

s32 odin_isp_probe(struct platform_device *pdev)
{
	s32 ret = CSS_SUCCESS;
	s32 val, idx;

#if CONFIG_OF
	of_property_read_u32(pdev->dev.of_node, "id",&val);
	pdev->id = val;
#endif

	idx = pdev->id;

	ret = odin_isp_init_resources(pdev);

#ifdef CONFIG_VIDEO_ISP_MISC_DRIVER
	if (ret == CSS_SUCCESS) {
		ret = misc_register(&isp_miscdevice[idx]);
		if (ret < 0) {
			css_err("isp%02d failed to register misc %d\n", idx, ret);
		}
	}
#endif

	if (ret == CSS_SUCCESS) {
		css_info("isp%02d probe ok!!\n", idx);
	} else {
		css_err("isp%02d probe err!!\n", idx);
	}

	return ret;
}

s32
odin_isp_remove(struct platform_device *pdev)
{
	s32 ret = CSS_SUCCESS;
	s32 idx = 0;

	idx = pdev->id;

	ret = odin_isp_deinit_resources(pdev);

	if (ret == CSS_SUCCESS) {
		css_info("isp%02d remove ok!!\n", idx);
	} else {
		css_err("isp%02d remove err!!\n", idx);
	}

	return ret;
}

s32
odin_isp_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

s32
odin_isp_resume(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id odin_isp_match[] = {
	{ .compatible = CSS_OF_ISP_NAME},
	{},
};
#endif /* CONFIG_OF */

struct platform_driver odin_isp_driver =
{
	.probe		= odin_isp_probe,
	.remove 	= odin_isp_remove,
	.suspend	= odin_isp_suspend,
	.resume 	= odin_isp_resume,
	.driver 	= {
		.name  = CSS_PLATDRV_ISP,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_isp_match),
#endif
	},
};

static int __init odin_isp_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&odin_isp_driver);

	return ret;
}

static void __exit odin_isp_exit(void)
{
	platform_driver_unregister(&odin_isp_driver);
	return;
}

MODULE_DESCRIPTION("odin isp driver");
MODULE_AUTHOR("MTEKVISION<http://www.mtekvision.com>");

late_initcall(odin_isp_init);
module_exit(odin_isp_exit);

MODULE_LICENSE("GPL");

