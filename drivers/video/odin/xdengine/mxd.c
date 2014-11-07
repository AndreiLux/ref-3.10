/*
 * linux/drivers/video/odin/mxd/mxd.c
 *
 * Copyright (C) 2012 LG Electronics, Inc.
 * Author: suhwa.kim
 *
 * Some code and ideas taken from drivers/video/omap/ driver
 * by Imre Deak.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#include <linux/export.h>
#endif

#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/odin_pm_domain.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include <linux/ion.h>
#include <linux/odin_iommu.h>
#include <video/odindss.h>
#include <asm/uaccess.h>
#include <video/odindss.h>

#include <linux/fb.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/odin_mailbox.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/odin_pd.h>

#include <video/odindss.h>


#include "../dss/dss.h"
#include "../display/panel_device.h"
#include <linux/miscdevice.h>

#include "mxd.h"
#include "ce_reg_def.h"
#include "nr_reg_def.h"
#include "re_reg_def.h"
#include "xd_top_reg_def.h"

#include "default_algorithm.h"


#define DRV_NAME 	"odindss-mxdhw"


static struct {
	mXdFormat  	  fmt;
	int	  	  width;
	int 	  	  height;
	mXd_SCENARIO_Case scenario;
	bool		  uvswap;
}mxd_desc;

u32 filmlikeinfo, nrinfo, contstinfo, brtnsinfo = 0;
u32 sharpinfo1, sharpinfo2, sharpinfo3, sharpinfo4 =0;
bool mxd_pd_onoff = false;
bool mxd_clk_onoff = false;
bool mxd_m2m_pd_onoff = false;

extern int odin_crg_mxd_swreset(bool enable);

int odin_mxd_init(void);

void mxd_info_set(void)
{

    filmlikeinfo    = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_FILM_CTRL_00);
    nrinfo            = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
    contstinfo      = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_VSPRGB_CTRL_00);
    brtnsinfo       = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_VSPRGB_CTRL_00);
    sharpinfo1     = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_CORING_CTRL);
    sharpinfo2     = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_02);
    sharpinfo3	     = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_04);
    sharpinfo4     = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_DP_CTRL);

}

void mxd_setting(void)
{
    mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_FILM_CTRL_00, filmlikeinfo);
    mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, nrinfo);
    mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CE_VSPRGB_CTRL_00, contstinfo);
    mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CE_VSPRGB_CTRL_00, brtnsinfo);
    mxd_write_reg(ODIN_MXD_NR_RES, ADDR_RE_CORING_CTRL, sharpinfo1);
    mxd_write_reg(ODIN_MXD_NR_RES, ADDR_RE_SP_CTRL_02, sharpinfo2);
    mxd_write_reg(ODIN_MXD_NR_RES, ADDR_RE_SP_CTRL_04, sharpinfo3);
    mxd_write_reg(ODIN_MXD_NR_RES, ADDR_RE_DP_CTRL, sharpinfo4);
}

bool get_mxd_pd_status(void)
{
	return mxd_pd_onoff;
}

bool get_mxd_clk_status(void)
{
	return mxd_clk_onoff;
}

void mxd_pw_reset(bool pw_flag)
{
    if (pw_flag == true) { /* power on */
	    if (mxd_pd_onoff == false)
		    pm_runtime_get_sync(&mxd_dev.pdev->dev);
	    else
		    printk(KERN_WARNING "mxd_pd is already ON\n");
    }
    else {			/* power off */
	    if (mxd_pd_onoff == true) {
		    mxd_info_set();
		    pm_runtime_put_sync(&mxd_dev.pdev->dev);
	    }
	    else
		    printk(KERN_WARNING "mxd_pd is already OFF\n");
    }
}

int mxd_on_for_m2m_on(__u8 enable)
{
	struct odin_writeback *wb;
	wb = odin_dss_get_writeback(1); /*VID1*/
	if (enable == 1)
	{
		mxd_m2m_pd_onoff = true;
		mxd_pw_reset(true);

		wb->register_framedone(wb);
		printk("mxd enable \n");
	}
	else
	{
		mxd_m2m_pd_onoff = false;
		mxd_pw_reset(false);

		wb->unregister_framedone(wb);
		printk("mxd disable \n");
	}

	return 0;
}

void _mxd_set_ce_dce_lut(ceHISTLUTMODE mode, bool ReadEn);
void _mxd_set_ce_dse_lut(ceHISTLUTMODE mode, bool ReadEn);
void _odin_mxd_set_histogram(bool ReadEn, int numofBin);

void odin_dma_default_set()
{
	u32 val;

	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
	val = MXD_REG_MOD(val, TOP_LUT_DMASTART_USERMODE,
	DCE_DSE_GAIN_LUT_OPERATION_MODE);
	mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val);

	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
	val = MXD_REG_MOD(val, DMA_AUTO, DCE_DSE_GAIN_LUT_DMA_START);
	mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val);

	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
	val = MXD_REG_MOD(val, TOP_HIST_LUT_APB, HIST_LUT_OPERATION_MODE);
	mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val);

	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
	val = MXD_REG_MOD(val, DMA_USER_DEFINE, HIST_LUT_DMA_START);
	mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val);
}

void odin_interrupt_mask(void)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00);
	val = MXD_REG_MOD(val, ENABLE, NR_INT_MASK);
	val = MXD_REG_MOD(val, ENABLE, RE_INT_MASK);
	val = MXD_REG_MOD(val, DISABLE, CE_INT_MASK);
	mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00, val);
}


void odin_interrupt_unmask(void)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00);
	val = MXD_REG_MOD(val, ENABLE, NR_INT_MASK);
	val = MXD_REG_MOD(val, ENABLE, RE_INT_MASK);
	val = MXD_REG_MOD(val, ENABLE, CE_INT_MASK);
	mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00, val);
}

void odin_histogram_test(void)
{
	_odin_mxd_set_histogram(DISABLE, 64);
	_odin_mxd_set_histogram(ENABLE, 64);
}

static void mxd_irq_handler_lut(void *tossed_data,u32 irqmask,u32 sub_irqmask)
{
	odin_crg_unregister_isr(mxd_irq_handler_lut, NULL, irqmask, sub_irqmask);
}

int odin_interrupt_test(u32 irqmask, u32 sub_irqmask)
{
	int r;
	r= odin_crg_register_isr(mxd_irq_handler_lut, NULL, irqmask, (u32)NULL);
	return 0;
}


void _odin_mxd_set_histogram(bool ReadEn, int numofBin)
{
	u32 val;
	volatile unsigned int addr = 0;
	unsigned int lutsize = 0;
	int index;
	int hist_min, top_1, top_2, top_3, top_4, top_5;

	if (numofBin == 32) 	lutsize = 0;
	else if (numofBin == 64) 	lutsize = 1;

	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_CTRL_00);
	val = MXD_REG_MOD(val, lutsize, HIST_LUT_STEP_MODE);
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_HIST_CTRL_00, val);

	if (ReadEn)
	{
		for (index = 0; index < numofBin; index++)
		{

			val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
			val = MXD_REG_MOD(val, DMA_AUTO, HIST_LUT_DMA_START);
			mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val);
			val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
			val = MXD_REG_MOD(val, TOP_HIST_LUT_APB, HIST_LUT_OPERATION_MODE);
			mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val);
			val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_LUT_CTRL);
			val = MXD_REG_MOD(val, index, HIST_LUT_ADRS);
			mxd_write_reg(ODIN_MXD_CE_RES, ADDR_HIST_LUT_CTRL, val);
		}

		val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_MIN_MAX);
		hist_min = MXD_REG_GET(val, HIST_Y_MIN_VALUE);
		hist_min = MXD_REG_GET(val, HIST_Y_MAX_VALUE);

		val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_TOP5);
		top_1 = MXD_REG_GET(val, HIST_BIN_HIGH_LEVEL_TOP1);
		top_2 = MXD_REG_GET(val, HIST_BIN_HIGH_LEVEL_TOP2);
		top_3 = MXD_REG_GET(val, HIST_BIN_HIGH_LEVEL_TOP3);
		top_4 = MXD_REG_GET(val, HIST_BIN_HIGH_LEVEL_TOP4);
		top_5 = MXD_REG_GET(val, HIST_BIN_HIGH_LEVEL_TOP5);


	}
	else
	{
		val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
		val = MXD_REG_MOD(val, TOP_LUT_DMASTART_USERMODE,
										DCE_DSE_GAIN_LUT_OPERATION_MODE);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val);

		val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
		val = MXD_REG_MOD(val, DMA_AUTO, DCE_DSE_GAIN_LUT_DMA_START);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val);

		val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_HIST_LUT_BADDR);
		val = MXD_REG_MOD(val, addr , HIST_LUT_DRAM_BASE_ADDRESS);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_GAIN_LUT_BADDR, val);

		val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
		val = MXD_REG_MOD(val, DMA_USER_DEFINE, HIST_LUT_DMA_START);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val);

		val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
		val = MXD_REG_MOD(val, DMA_AUTO, HIST_LUT_OPERATION_MODE);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val);
	}
}


void _odin_mxd_set_path(mXd_SCENARIO_Case scenario)
{
	u32 val;

	switch (scenario)
	{
	case MXD_SCENARIO_CASE1:
		val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00);
		val = MXD_REG_MOD(val, 0, NR_INPUT_SEL);
		val = MXD_REG_MOD(val, 1, RE_INPUT_SEL);
		val = MXD_REG_MOD(val, 0, SCL_INPUT_SEL);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00, val);
		break;
	case MXD_SCENARIO_CASE2:
		val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00);
		val = MXD_REG_MOD(val, 0, NR_INPUT_SEL);
		val = MXD_REG_MOD(val, 0, RE_INPUT_SEL);
		val = MXD_REG_MOD(val, 0, SCL_INPUT_SEL);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00, val);
		break;
	case MXD_SCENARIO_CASE3:
		val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00);
		val = MXD_REG_MOD(val, 1, NR_INPUT_SEL);
		val = MXD_REG_MOD(val, 0, RE_INPUT_SEL);
		val = MXD_REG_MOD(val, 0, SCL_INPUT_SEL);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00, val);
		break;
	case MXD_SCENARIO_CASE4:
		val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00);
		val = MXD_REG_MOD(val, 0, NR_INPUT_SEL);
		val = MXD_REG_MOD(val, 1, RE_INPUT_SEL);
		val = MXD_REG_MOD(val, 0, SCL_INPUT_SEL);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00, val);
		break;
	case MXD_SCENARIO_MAX:
		break;
		}
}

u32 odin_mxd_get_nr_image_size_width(void)
{
	u32 val, mxd_nr_width;
	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE0);
	mxd_nr_width = MXD_REG_GET(val, NR_PIC_WIDTH);
	return mxd_nr_width;
}

u32 odin_mxd_get_nr_image_size_height(void)
{
	u32 val, mxd_nr_height;
	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE0);
	mxd_nr_height= MXD_REG_GET(val, NR_PIC_HEIGHT);
	return mxd_nr_height;
}

u32 odin_mxd_get_re_image_size_width(void)
{
	u32 val, mxd_re_width;
	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE1);
	mxd_re_width = MXD_REG_GET(val, RE_PIC_WIDTH);
	return mxd_re_width;
}

u32 odin_mxd_get_re_image_size_height(void)
{
	u32 val, mxd_re_height;
	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE1);
	mxd_re_height= MXD_REG_GET(val, RE_PIC_HEIGHT);
	return mxd_re_height;
}

void odin_mxd_set_image_size( u16 nr_size_width, u16 nr_size_height,
			      u16 re_size_width, u16 re_size_height)
{
	u32 val;
/*
	if (nr_size_width > ODIN_MXD_MAX_WIDTH
					|| nr_size_height > ODIN_MXD_MAX_HEIGHT)
		BUG();*/

    if (nr_size_width == 0
                || nr_size_height == 0)
                BUG();

	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE0);
	val = MXD_REG_MOD(val, nr_size_width, NR_PIC_WIDTH);
	val = MXD_REG_MOD(val, nr_size_height, NR_PIC_HEIGHT);
	mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE0, val);
/*
	if (re_size_width > ODIN_MXD_MAX_WIDTH
					|| re_size_height > ODIN_MXD_MAX_HEIGHT)
		BUG();*/
	if (re_size_width == 0
					|| re_size_height == 0)
					BUG();

	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE1);
	val = MXD_REG_MOD(val, re_size_width, RE_PIC_WIDTH);
	val = MXD_REG_MOD(val, re_size_height, RE_PIC_HEIGHT);
	mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE1, val);
}

void _odin_mxd_set_color_format(mXdFormat format, int uvSwap)
{
	u32 val;

	if (format < MXD_FORMAT_YUV420_MPEG_1_2_CVI ||
	    format > MXD_FORMAT_YUV444)
		BUG();

	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_02);
	val = MXD_REG_MOD(val, format, COLOR_FORMAT);
	mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_02, val);
}

void _mxd_set_re_peaking_balancing_gain(MXD_Level re_peaking_level)
{
	u32 val, ctrl07, ctrl08, ctrl09, ctrl0a, ctrl0b, ctrl0c;

	val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_07);
	val = MXD_REG_MOD(val, GBMODE_EN, GB_EN);
	val = MXD_REG_MOD(val, GB_MODE_VAL, GB_MODE);
	mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_07, val);

	ctrl07 = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_07);
	ctrl08 = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_08);
	ctrl09 = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_09);
	ctrl0a = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0A);
	ctrl0b = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0B);
	ctrl0c = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0C);

	ctrl0a = MXD_REG_MOD(ctrl0a, LUM1_Y0_VAL, LUM1_Y0);
	ctrl0a = MXD_REG_MOD(ctrl0a, LUM1_Y1_VAL, LUM1_Y1);
	ctrl0a = MXD_REG_MOD(ctrl0a, LUM1_Y2_VAL, LUM1_Y2);
	ctrl0b = MXD_REG_MOD(ctrl0b, LUM2_Y0_VAL, LUM2_Y0);

	ctrl0c = MXD_REG_MOD(ctrl0c, LUM2_Y1_VAL, LUM2_Y1);
	ctrl0c = MXD_REG_MOD(ctrl0c, LUM2_Y2_VAL, LUM2_Y2);

	switch (re_peaking_level)
	{
		case LEVEL_STRONG:
		{
			ctrl07 = MXD_REG_MOD(ctrl07, GB_X1_STRONG, GB_X1);
			ctrl07 = MXD_REG_MOD(ctrl07, GB_Y1_STRONG, GB_Y1);
			ctrl08 = MXD_REG_MOD(ctrl08, GB_X2_STRONG, GB_X2);
			ctrl08 = MXD_REG_MOD(ctrl08, GB_Y2_STRONG, GB_Y2);
			ctrl08 = MXD_REG_MOD(ctrl08, GB_Y3_STRONG, GB_Y3);
			ctrl09  = MXD_REG_MOD(ctrl09, LUM1_X_L0_STRONG, LUM1_X_L0);
			ctrl09  = MXD_REG_MOD(ctrl09, LUM1_X_L1_STRONG, LUM1_X_L1);
			ctrl09  = MXD_REG_MOD(ctrl09, LUM1_X_H0_STRONG, LUM1_X_H0);
			ctrl09  = MXD_REG_MOD(ctrl09, LUM1_X_H1_STRONG, LUM1_X_H1);
			ctrl0a = MXD_REG_MOD(ctrl0a, LUM2_X_L0_STRONG, LUM2_X_L0);
			ctrl0b = MXD_REG_MOD(ctrl0b, LUM2_X_L1_STRONG, LUM2_X_L1);
			ctrl0b = MXD_REG_MOD(ctrl0b, LUM2_X_H0_STRONG, LUM2_X_H0);
			ctrl0b = MXD_REG_MOD(ctrl0b, LUM2_X_H1_STRONG, LUM2_X_H1);
			break;
		}
		case LEVEL_MEDIUM:
		{
			ctrl07 = MXD_REG_MOD(ctrl07, GB_X1_MEDIUM, GB_X1);
			ctrl07 = MXD_REG_MOD(ctrl07, GB_Y1_MEDIUM, GB_Y1);
			ctrl08 = MXD_REG_MOD(ctrl08, GB_X2_MEDIUM, GB_X2);
			ctrl08 = MXD_REG_MOD(ctrl08, GB_Y2_MEDIUM, GB_Y2);
			ctrl08 = MXD_REG_MOD(ctrl08, GB_Y3_MEDIUM, GB_Y3);
			ctrl09  = MXD_REG_MOD(ctrl09, LUM1_X_L0_MEDIUM, LUM1_X_L0);
			ctrl09  = MXD_REG_MOD(ctrl09, LUM1_X_L1_MEDIUM, LUM1_X_L1);
			ctrl09  = MXD_REG_MOD(ctrl09, LUM1_X_H0_MEDIUM, LUM1_X_H0);
			ctrl09  = MXD_REG_MOD(ctrl09, LUM1_X_H1_MEDIUM, LUM1_X_H1);
			ctrl0a = MXD_REG_MOD(ctrl0a, LUM2_X_L0_MEDIUM, LUM2_X_L0);
			ctrl0b = MXD_REG_MOD(ctrl0b, LUM2_X_L1_MEDIUM, LUM2_X_L1);
			ctrl0b = MXD_REG_MOD(ctrl0b, LUM2_X_H0_MEDIUM, LUM2_X_H0);
			ctrl0b = MXD_REG_MOD(ctrl0b, LUM2_X_H1_MEDIUM, LUM2_X_H1);
			break;
		}
		case LEVEL_WEEK:
		{
			ctrl07 = MXD_REG_MOD(ctrl07, GB_X1_WEEK, GB_X1);
			ctrl07 = MXD_REG_MOD(ctrl07, GB_Y1_WEEK, GB_Y1);
			ctrl08 = MXD_REG_MOD(ctrl08, GB_X2_WEEK, GB_X2);
			ctrl08 = MXD_REG_MOD(ctrl08, GB_Y2_WEEK, GB_Y2);
			ctrl08 = MXD_REG_MOD(ctrl08, GB_Y3_WEEK, GB_Y3);
			ctrl09  = MXD_REG_MOD(ctrl09, LUM1_X_L0_WEEK, LUM1_X_L0);
			ctrl09  = MXD_REG_MOD(ctrl09, LUM1_X_L1_WEEK, LUM1_X_L1);
			ctrl09  = MXD_REG_MOD(ctrl09, LUM1_X_H0_WEEK, LUM1_X_H0);
			ctrl09  = MXD_REG_MOD(ctrl09, LUM1_X_H1_WEEK, LUM1_X_H1);
			ctrl0a = MXD_REG_MOD(ctrl0a, LUM2_X_L0_WEEK, LUM2_X_L0);
			ctrl0b = MXD_REG_MOD(ctrl0b, LUM2_X_L1_WEEK, LUM2_X_L1);
			ctrl0b = MXD_REG_MOD(ctrl0b, LUM2_X_H0_WEEK, LUM2_X_H0);
			ctrl0b = MXD_REG_MOD(ctrl0b, LUM2_X_H1_WEEK, LUM2_X_H1);
			break;
		}
		default:
			BUG();

	}

	mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_07, ctrl07);
	mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_08, ctrl08);
	mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_09, ctrl09);
	mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0A, ctrl0a);
	mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0B, ctrl0b);
	mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0C, ctrl0c);

}

void _mxd_set_re_peaking(MXD_Level re_peaking_level, bool overstate)
{
	u32 val, val02, val04;
		val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_01);
		val02 = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_02);
		val04 = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_04);


		if (re_peaking_level == LEVEL_STRONG)
			val = MXD_REG_MOD(val, WHITE_GAIN_STRONG, WHITE_GAIN);
		else if (re_peaking_level == LEVEL_WEEK)
			val = MXD_REG_MOD(val, WHITE_GAIN_WEEK, WHITE_GAIN);
		else
			val = MXD_REG_MOD(val, WHITE_GAIN_MEDIUM, WHITE_GAIN);

		if (re_peaking_level == LEVEL_STRONG)
			val = MXD_REG_MOD(val, BLACK_GAIN_STRONG, BLACK_GAIN);
		else if (re_peaking_level == LEVEL_WEEK)
			val = MXD_REG_MOD(val, BLACK_GAIN_WEEK, BLACK_GAIN);
		else
			val = MXD_REG_MOD(val, BLACK_GAIN_MEDIUM, BLACK_GAIN);

		if (re_peaking_level == LEVEL_STRONG)
			val = MXD_REG_MOD(val, HORIZONTAL_GAIN_STRONG, HORIZONTAL_GAIN);
		else if (re_peaking_level == LEVEL_WEEK)
			val = MXD_REG_MOD(val, HORIZONTAL_GAIN_WEEK, HORIZONTAL_GAIN);
		else
			val = MXD_REG_MOD(val, HORIZONTAL_GAIN_MEDIUM, HORIZONTAL_GAIN);

		if (re_peaking_level == LEVEL_STRONG)
			val = MXD_REG_MOD(val, VERTICAL_GAIN_STRONG, VERTICAL_GAIN);
		else if (re_peaking_level == LEVEL_WEEK)
			val = MXD_REG_MOD(val, VERTICAL_GAIN_WEEK, VERTICAL_GAIN);
		else
			val = MXD_REG_MOD(val, VERTICAL_GAIN_MEDIUM, VERTICAL_GAIN);

		mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_01, val);


		if (re_peaking_level == LEVEL_STRONG)
			val02 = MXD_REG_MOD(val02, SOBEL_WEIGHT_STRONG, SOBEL_WEIGHT);
		else if (re_peaking_level == LEVEL_WEEK)
			val02 = MXD_REG_MOD(val02, SOBEL_WEIGHT_WEEK, SOBEL_WEIGHT);
		else
			val02 = MXD_REG_MOD(val02, SOBEL_WEIGHT_MEDIUM, SOBEL_WEIGHT);

		if (re_peaking_level == LEVEL_STRONG)
			val02 = MXD_REG_MOD(val02, LAPLACIAN_WEIGHT_STRONG,
															LAPLACIAN_WEIGHT);
		else if (re_peaking_level == LEVEL_WEEK)
			val02 = MXD_REG_MOD(val02, LAPLACIAN_WEIGHT_WEEK,
															LAPLACIAN_WEIGHT);
		else
			val02 = MXD_REG_MOD(val02, LAPLACIAN_WEIGHT_MEDIUM,
															LAPLACIAN_WEIGHT);

	if (overstate == OVERSTATE_EN_VAL)
	{
		val02 = MXD_REG_MOD(val02, DISABLE, SOBEL_MANUAL_MODE_EN);
		val04 = MXD_REG_MOD(val04, DISABLE, GX_WEIGHT_MANUAL_EN);

		mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_02, val02);
		mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_04, val04);

		_mxd_set_re_peaking_balancing_gain(re_peaking_level);
	}

	else if (overstate == DISABLE)
	{
		val02 = MXD_REG_MOD(val02, SOBEL_MANUAL_MODE_EN_VAL,
													SOBEL_MANUAL_MODE_EN);

		if (re_peaking_level == LEVEL_STRONG)
			val02 = MXD_REG_MOD(val02, SOBEL_MANUAL_GAIN_STRONG,
													SOBEL_MANUAL_GAIN);
		else if (re_peaking_level == LEVEL_WEEK)
			val02 = MXD_REG_MOD(val02, SOBEL_MANUAL_GAIN_WEEK,
													SOBEL_MANUAL_GAIN);
		else
			val02 = MXD_REG_MOD(val02, SOBEL_MANUAL_GAIN_MEDIUM,
													SOBEL_MANUAL_GAIN);

		val04 = MXD_REG_MOD(val04, GX_WEIGHT_MANUAL_MODE_EN_VAL,
													GX_WEIGHT_MANUAL_EN);

		if (re_peaking_level == LEVEL_STRONG)
			val04 = MXD_REG_MOD(val04, GX_WEIGHT_MANUAL_GAIN_STRONG,
													GX_WEIGHT_MANUAL_GAIN);
		else if (re_peaking_level == LEVEL_WEEK)
			val04 = MXD_REG_MOD(val04, GX_WEIGHT_MANUAL_GAIN_WEEK,
													GX_WEIGHT_MANUAL_GAIN);
		else
			val04 = MXD_REG_MOD(val04, GX_WEIGHT_MANUAL_GAIN_MEDIUM,
													GX_WEIGHT_MANUAL_GAIN);

		val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_07);
		val = MXD_REG_MOD(val, DISABLE, GB_EN);
		mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_07, val);


	mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_02, val02);
	mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_04, val04);

	}

	val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_06);
	if (re_peaking_level == LEVEL_STRONG)
		val = MXD_REG_MOD(val, LAP_H_MODE_STRONG, LAP_H_MODE);
	else
		val = MXD_REG_MOD(val, LAP_H_MODE_VAL, LAP_H_MODE);

	val = MXD_REG_MOD(val, LAP_V_MODE_VAL, LAP_V_MODE);

	mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_06, val);

}

void _mxd_set_ce_gmc_lut(bool ReadEn, ceGMCLUTMode mode)
	{
		u32 val, val2, val3;
		int index;
		val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02);
		val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL);
		val3 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_DATA);


		switch (mode)
		{
			case CE_GMC_LUT_READ:
				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02);
				val = MXD_REG_MOD(val, mode, GMC_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02, val);

				for (index = 0; index < 256; index++)
				{
					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL);
					val2 = MXD_REG_MOD(val2, ReadEn, GMC_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL, val2);

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL);
					val2 = MXD_REG_MOD(val2, index, GMC_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL, val2);

				}
				break;

			case CE_GMC_LUT_WRITE:
				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02);
				val = MXD_REG_MOD(val, mode, GMC_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02, val);

				for (index = 0; index < 256; index++)
				{

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL);
					val2 = MXD_REG_MOD(val2, ReadEn, GMC_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL, val2);

					val3 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_DATA);
					val3 = MXD_REG_MOD(val3, gmc_lut_r_y_gamma2_2[index],
																LUT_DATA_R);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_DATA, val3);

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL);
					val2 = MXD_REG_MOD(val2, index, GMC_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL, val2);

					val3 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_DATA);
					val3 = MXD_REG_MOD(val3, gmc_lut_r_y_gamma2_2[index],
																LUT_DATA_G);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_DATA, val3);

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL);
					val2 = MXD_REG_MOD(val2, index, GMC_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL, val2);

					val3 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_DATA);
					val3 = MXD_REG_MOD(val3, gmc_lut_r_y_gamma2_2[index],
																LUT_DATA_B);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_DATA, val3);

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL);
					val2 = MXD_REG_MOD(val2, index, GMC_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL, val2);
				}
				break;

			case CE_GMC_LUT_NORMALOP:
				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02);
				val = MXD_REG_MOD(val, mode, GMC_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02, val);
				break;

			case CE_GMC_LUT_USED:
				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02);
				val = MXD_REG_MOD(val, mode, GMC_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02, val);
				break;
		}

		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02, val);
		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL, val2);
		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_DATA, val3);

	}



void _mxd_set_ce_gmc_dither(bool dithEn)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_03);
	val = MXD_REG_MOD(val, dithEn, DITHER_EN);
	if (dithEn)
	{

		val = MXD_REG_MOD(val, DITHER_BIT_MODE_8BIT, BIT_MODE);
	}
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_03, val);

}


void _mxd_set_ce_gmc_decontour(bool decEn, MXD_Level dect_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_03);
	val = MXD_REG_MOD(val, decEn, DECONTOUR_EN);
	if (decEn)
	{
		val
                = MXD_REG_MOD(val, DITHER_USER_RANDOM_NUMBER,
		    DITHER_RANDOM_FREEZE_EN);
		if (dect_level == LEVEL_WEEK)
		{
			val = MXD_REG_MOD(val, DECONTOR_GAIN_R_WEEK, DECONTOUR_GAIN_R);
			val = MXD_REG_MOD(val, DECONTOR_GAIN_G_WEEK, DECONTOUR_GAIN_G);
			val = MXD_REG_MOD(val, DECONTOR_GAIN_B_WEEK, DECONTOUR_GAIN_B);
		}
		else if (dect_level == LEVEL_MEDIUM)
		{
			val = MXD_REG_MOD(val, DECONTOR_GAIN_R_MEDIUM, DECONTOUR_GAIN_R);
			val = MXD_REG_MOD(val, DECONTOR_GAIN_G_MEDIUM, DECONTOUR_GAIN_G);
			val = MXD_REG_MOD(val, DECONTOR_GAIN_B_MEDIUM, DECONTOUR_GAIN_B);
		}
		else if (dect_level == LEVEL_STRONG)
		{
			val = MXD_REG_MOD(val, DECONTOR_GAIN_R_STRONG, DECONTOUR_GAIN_R);
			val = MXD_REG_MOD(val, DECONTOR_GAIN_G_STRONG, DECONTOUR_GAIN_G);
			val = MXD_REG_MOD(val, DECONTOR_GAIN_B_STRONG, DECONTOUR_GAIN_B);
		}

	}
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_03, val);

}


void _mxd_set_ce_gmc_pixelreplacement(bool repEn, bool repR, bool repG,
												bool repB, MXD_Level rep_level)
{
	u32 val, val2, val3;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_00);
	val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_01);
	val3 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02);
	val = MXD_REG_MOD(val, repEn, PXL_REP_ENABLE);
	if (repEn == ENABLE)
	{
		val = MXD_REG_MOD(val, 0x0, PXL_REP_XPOS);
		val = MXD_REG_MOD(val, 0x0, PXL_REP_YPOS);
		val = MXD_REG_MOD(val, 0x0, PXL_REP_AREA);
		val2 = MXD_REG_MOD(val2, 0x0, PXL_REP_WIDTH);
		val2 = MXD_REG_MOD(val2, 0x0, PXL_REP_HEIGHT);

		if (repR)
		{
			val = MXD_REG_MOD(val, 0x0, PXL_REP_DISABLE_R);
			val3 = MXD_REG_MOD(val3, 0x10, PXL_REP_VALUE_R);
		}

		if (repG)
		{
			val = MXD_REG_MOD(val, 0x0, PXL_REP_DISABLE_G);
			val3 = MXD_REG_MOD(val3, 0x10, PXL_REP_VALUE_G);
		}

		if (repB)
		{
			val = MXD_REG_MOD(val, 0x0, PXL_REP_DISABLE_B);
			val3 = MXD_REG_MOD(val3, 0x10, PXL_REP_VALUE_B);
		}
	}


	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_00, val);
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_01, val2);
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02, val3);
}


void odin_mxd_set_ce_gmc(bool pix_En, bool dit_En,
								bool dec_En, MXD_Level gmc_level)
{
	_mxd_set_ce_gmc_pixelreplacement(pix_En, ENABLE, ENABLE,
												ENABLE, gmc_level);
	_mxd_set_ce_gmc_decontour(dec_En, gmc_level);
	_mxd_set_ce_gmc_dither(dit_En);
}

void _mxd_set_ce_dse_lut(ceHISTLUTMODE mode, bool ReadEn)
{
	u32 val, val2, val3;
	volatile unsigned int addr_reg = 0;
	int index, resultd;

	switch (mode)
	{
		case CE_HISTLUT_AUTOMODE:
			if (ReadEn)
			{

				for (index = 0; index < 32; index++)
				{
					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
										ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_USERMODE,
								    DCE_DSE_GAIN_LUT_OPERATION_MODE);
					mxd_write_reg(ODIN_MXD_TOP_RES,
										ADDR_XD_DMA_REG_CTRL, val3);

					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
									        ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, DMA_AUTO,
									DCE_DSE_GAIN_LUT_DMA_START);
					mxd_write_reg(ODIN_MXD_TOP_RES,
											ADDR_XD_DMA_REG_CTRL, val3);

					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
										ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_APB,
									DCE_DSE_GAIN_LUT_OPERATION_MODE);
					mxd_write_reg(ODIN_MXD_TOP_RES,
										ADDR_XD_DMA_REG_CTRL, val3);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL);
					val = MXD_REG_MOD(val, DSE_LUT_WEN_READ, DSE_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL);
					val = MXD_REG_MOD(val, DISABLE, DSE_LUT_EN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);

					val = MXD_REG_MOD(val, index, DSE_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_TOP_RES,
										            ADDR_XD_GAIN_LUT_BADDR);
					val = MXD_REG_MOD(val, addr_reg ,
										        HIST_LUT_DRAM_BASE_ADDRESS);
					mxd_write_reg(ODIN_MXD_TOP_RES,
											ADDR_XD_GAIN_LUT_BADDR, val);
					resultd = mxd_read_reg(ODIN_MXD_CE_RES,
													ADDR_CE_DSE_IA_DATA);
					odin_dma_default_set();
				}
				odin_dma_default_set();
			}


			/*write*/
			else
			{
				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_GAIN_LUT_BADDR);
				val3 = MXD_REG_MOD(val3, addr_reg,
										DCE_DSE_GAIN_LUT_DRAM_BASE_ADDRESS);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_GAIN_LUT_BADDR, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, DMA_AUTO,
											DCE_DSE_GAIN_LUT_DMA_START);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, DMA_AUTO, HIST_LUT_DMA_START);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, TOP_HIST_LUT_AUTO,
												HIST_LUT_OPERATION_MODE);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

				val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_HIST_LUT_BADDR);
				val = MXD_REG_MOD(val, addr_reg , HIST_LUT_DRAM_BASE_ADDRESS);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_GAIN_LUT_BADDR, val);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_USERMODE,
											DCE_DSE_GAIN_LUT_OPERATION_MODE);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);
			}
			break;
		case CE_HISTLUT_USERDEFINEMODE:
        		if (ReadEn)
        		{
        			for (index = 0; index < 32; index++)
        			{
        				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
        				val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_USERMODE,
        										DCE_DSE_GAIN_LUT_OPERATION_MODE);
        				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

        				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
        				val3 = MXD_REG_MOD(val3, DMA_USER_DEFINE,
        										DCE_DSE_GAIN_LUT_DMA_START);
        				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

        				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
        				val3 = MXD_REG_MOD(val3, DMA_AUTO,
        										DCE_DSE_GAIN_LUT_DMA_START);
        				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

        				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
        				val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_APB,
        										DCE_DSE_GAIN_LUT_OPERATION_MODE);
        				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

        				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL);
        				val = MXD_REG_MOD(val, DSE_LUT_WEN_READ, DSE_LUT_WEN);
        				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);

        				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL);
        				val = MXD_REG_MOD(val, DISABLE, DSE_LUT_EN);
        				mxd_write_reg(ODIN_MXD_CE_RES,
                                    ADDR_CE_DSE_IA_CTRL, val);

                                 val = mxd_read_reg(ODIN_MXD_CE_RES,
                                    ADDR_CE_DSE_IA_CTRL);
        				val = MXD_REG_MOD(val, index, DSE_LUT_ADRS);
        				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);

        				resultd = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_DATA);
        			}
			}

			else
			{
				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_GAIN_LUT_BADDR);
				val3 = MXD_REG_MOD(val3, addr_reg,
				DCE_DSE_GAIN_LUT_DRAM_BASE_ADDRESS);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_GAIN_LUT_BADDR, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, DMA_AUTO, DCE_DSE_GAIN_LUT_DMA_START);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, DMA_USER_DEFINE, HIST_LUT_DMA_START);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, TOP_HIST_LUT_USERMODE,
					HIST_LUT_OPERATION_MODE);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, DMA_AUTO,
					DCE_DSE_GAIN_LUT_OPERATION_MODE);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);
			}
			break;
		case CE_HISTLUT_APBMODE:

			if (ReadEn)
			{
				for (index = 0; index < 32; index++)
				{

					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_APB,
						DCE_DSE_GAIN_LUT_OPERATION_MODE);
					mxd_write_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL, val3);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL);
					val = MXD_REG_MOD(val, DSE_LUT_WEN_READ, DSE_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL);
					val = MXD_REG_MOD(val, DISABLE, DSE_LUT_EN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);

					val = MXD_REG_MOD(val, index, DSE_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);
					resultd = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_DATA);

				}
			}
			else
			{
				for (index = 0; index < 32; index++)
				{

					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_APB,
						DCE_DSE_GAIN_LUT_OPERATION_MODE);
					mxd_write_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL, val3);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL);
					val = MXD_REG_MOD(val, DISABLE, DSE_LUT_EN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL);
					val = MXD_REG_MOD(val, DSE_LUT_WEN_WRITE, DSE_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_DATA);
					val2 = MXD_REG_MOD(val2,
							((dse_lut_x_1[index] & (0xffffffff)) << 16)
						+ (dse_lut_y_1[index]& (0xffffffff)), DSE_LUT_DATA);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_DATA, val2);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL);
					val = MXD_REG_MOD(val, index, DSE_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL);
					val = MXD_REG_MOD(val, DSE_LUT_WEN_WRITE, DSE_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, val);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_DATA, val2);
				}
			}
					break;
	}
}



void _mxd_set_ce_dse_mapping_curve(void)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_02);
	val = MXD_REG_MOD(val, HIF_DSE_WDATA_Y32ND, HIF_DSE_WDATA_Y_32ND);
	val = MXD_REG_MOD(val, HIF_DSE_WDATA_X32ND, HIF_DSE_WDATA_X_32ND);
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_02, val);
}

void _mxd_set_ce_dse_gain(MXD_Level dse_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_01);

	if (dse_level == LEVEL_WEEK)
	{
		val = MXD_REG_MOD(val, SATURATION_REGION_GAIN_WEEK,
						SATURATION_REGION_GAIN);
	}
	else if (dse_level == LEVEL_MEDIUM)
	{
		val = MXD_REG_MOD(val, SATURATION_REGION_GAIN_MEDIUM,
						SATURATION_REGION_GAIN);
	}
	else if (dse_level == LEVEL_STRONG)
	{
		val = MXD_REG_MOD(val, SATURATION_REGION_GAIN_STRONG,
						SATURATION_REGION_GAIN);
	}
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_01, val);

}

void _mxd_set_ce_dse_color_region(void)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_00);
	val = MXD_REG_MOD(val, SATURATION_REGION0_SEL_VAL, SATURATION_REGION0_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION1_SEL_VAL, SATURATION_REGION1_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION2_SEL_VAL, SATURATION_REGION2_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION3_SEL_VAL, SATURATION_REGION3_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION4_SEL_VAL, SATURATION_REGION4_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION5_SEL_VAL, SATURATION_REGION5_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION6_SEL_VAL, SATURATION_REGION6_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION7_SEL_VAL, SATURATION_REGION7_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION8_SEL_VAL, SATURATION_REGION8_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION9_SEL_VAL, SATURATION_REGION9_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION10_SEL_VAL,
					SATURATION_REGION10_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION11_SEL_VAL,
					SATURATION_REGION12_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION13_SEL_VAL,
					SATURATION_REGION13_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION14_SEL_VAL,
					SATURATION_REGION14_SEL);
	val = MXD_REG_MOD(val, SATURATION_REGION15_SEL_VAL,
					SATURATION_REGION15_SEL);
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_00, val);

}


void odin_mxd_set_ce_dse(bool DSEEn, MXD_Level dse_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_00);
	val = MXD_REG_MOD(val, DSEEn, DYNAMIC_SATURATION_EN);
	if (DSEEn)
	{
		_mxd_set_ce_dse_color_region();
		_mxd_set_ce_dse_gain(dse_level);
		_mxd_set_ce_dse_mapping_curve();
		odin_dma_default_set();
	}
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_00, val);
}


void _mxd_set_ce_dce_lut(ceHISTLUTMODE mode, bool ReadEn)
{
	u32 val = 0, val2 = 0, val3 = 0, resultd = 0;
	int index = 0;
	volatile unsigned long addr_reg = 0;

	switch (mode)
	{
		case CE_HISTLUT_AUTOMODE:
			if (ReadEn)
			{

				for (index = 0; index < 32; index++)
				{
					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_USERMODE,
							DCE_DSE_GAIN_LUT_OPERATION_MODE);
					mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, DMA_AUTO,
							DCE_DSE_GAIN_LUT_DMA_START);
					mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_APB,
							DCE_DSE_GAIN_LUT_OPERATION_MODE);
					mxd_write_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL, val3);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL);
					val = MXD_REG_MOD(val, DCE_LUT_WEN_READ, DCE_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL);
					val = MXD_REG_MOD(val, DISABLE, DCE_LUT_EN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);

					val = MXD_REG_MOD(val, index, DCE_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);
					resultd = mxd_read_reg(ODIN_MXD_CE_RES,
							ADDR_CE_DCE_IA_DATA);
					odin_dma_default_set();
				}
				odin_dma_default_set();
			}

			else
			{
				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_GAIN_LUT_BADDR);
				val3 = MXD_REG_MOD(val3, addr_reg,DCE_DSE_GAIN_LUT_DRAM_BASE_ADDRESS);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_GAIN_LUT_BADDR, val3);

				val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_HIST_LUT_BADDR);
				val = MXD_REG_MOD(val, addr_reg , HIST_LUT_DRAM_BASE_ADDRESS);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_GAIN_LUT_BADDR, val);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, DMA_AUTO,DCE_DSE_GAIN_LUT_DMA_START);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, DMA_AUTO, HIST_LUT_DMA_START);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3,TOP_HIST_LUT_AUTO,HIST_LUT_OPERATION_MODE);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_USERMODE,
				DCE_DSE_GAIN_LUT_OPERATION_MODE);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);
			}
			break;
		case CE_HISTLUT_USERDEFINEMODE:

			if (ReadEn)
			{

				for (index = 0; index < 32; index++)
				{

					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_USERMODE,
					DCE_DSE_GAIN_LUT_OPERATION_MODE);
					mxd_write_reg(ODIN_MXD_TOP_RES,
						ADDR_XD_DMA_REG_CTRL, val3);

					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, DMA_USER_DEFINE,
						DCE_DSE_GAIN_LUT_DMA_START);
					mxd_write_reg(ODIN_MXD_TOP_RES,
						ADDR_XD_DMA_REG_CTRL, val3);

					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, DMA_AUTO,
							DCE_DSE_GAIN_LUT_DMA_START);
					mxd_write_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL, val3);

					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_APB,
							DCE_DSE_GAIN_LUT_OPERATION_MODE);
					mxd_write_reg(ODIN_MXD_TOP_RES,
							ADDR_XD_DMA_REG_CTRL, val3);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL);
					val = MXD_REG_MOD(val, DCE_LUT_WEN_READ, DCE_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL);
					val = MXD_REG_MOD(val, DISABLE, DCE_LUT_EN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL);
					val = MXD_REG_MOD(val, index, DCE_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);
				}
			}

			else
			{
				val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
                                    ADDR_XD_GAIN_LUT_BADDR);
				val3 = MXD_REG_MOD(val3, addr_reg,
                                    DCE_DSE_GAIN_LUT_DRAM_BASE_ADDRESS);
				mxd_write_reg(ODIN_MXD_TOP_RES,
                                    ADDR_XD_GAIN_LUT_BADDR, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, DMA_AUTO,
                                                DCE_DSE_GAIN_LUT_DMA_START);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, DMA_USER_DEFINE, HIST_LUT_DMA_START);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, TOP_HIST_LUT_USERMODE,
                                    HIST_LUT_OPERATION_MODE);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);

				val3 = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
				val3 = MXD_REG_MOD(val3, DMA_AUTO,
                                    DCE_DSE_GAIN_LUT_OPERATION_MODE);
				mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, val3);
			}

			break;
		case CE_HISTLUT_APBMODE:

			if (ReadEn)
			{
				for (index = 0; index < 32; index++)
				{
					val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
                                      ADDR_XD_DMA_REG_CTRL);
					val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_APB,
                                     DCE_DSE_GAIN_LUT_OPERATION_MODE);
					mxd_write_reg(ODIN_MXD_TOP_RES,
                                    ADDR_XD_DMA_REG_CTRL, val3);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL);
					val = MXD_REG_MOD(val, DCE_LUT_WEN_READ, DCE_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL);
					val = MXD_REG_MOD(val, DISABLE, DCE_LUT_EN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);

					val = MXD_REG_MOD(val, index, DCE_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);
				}
			}

			else
			{

				for (index = 0; index < 32; index++)
				{
                                 val3 = mxd_read_reg(ODIN_MXD_TOP_RES,
                                      ADDR_XD_DMA_REG_CTRL);
        				val3 = MXD_REG_MOD(val3, TOP_LUT_DMASTART_APB,
                                         DCE_DSE_GAIN_LUT_OPERATION_MODE);
        				mxd_write_reg(ODIN_MXD_TOP_RES,
                                          ADDR_XD_DMA_REG_CTRL, val3);

        				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL);
        				val = MXD_REG_MOD(val, DISABLE, DCE_LUT_EN);
        				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);

        				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL);
        				val = MXD_REG_MOD(val, DCE_LUT_WEN_WRITE, DCE_LUT_WEN);
        				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);

        				val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_DATA);
        				val2 = MXD_REG_MOD(val2, ((dse_lut_x_1[index] & (0xffffffff)) << 16)
        					+ (dse_lut_y_1[index]& (0xffffffff)), DCE_LUT_DATA);
        				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_DATA, val2);


        				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL);
        				val = MXD_REG_MOD(val, index, DCE_LUT_ADRS);
        				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);
                        	       val = mxd_read_reg(ODIN_MXD_CE_RES,
                                    ADDR_CE_DCE_IA_CTRL);
        				val = MXD_REG_MOD(val, DCE_LUT_WEN_WRITE, DCE_LUT_WEN);
        				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);
        				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_CTRL, val);
        				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_IA_DATA, val2);

				}
			}
			break;
		}


}


void _mxd_set_ce_dce_set_domain(bool HSVdomain, MXD_Level dce_level)
{
	u32 val, val2, val3, val4, val5,val6;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_00);

	if (HSVdomain == SELECT_HSV_VAL_EN)
	{
		val = MXD_REG_MOD(val, DCE_HSV_SELECTION_VAL, DCE_Y_SELECTION);
		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_00, val);
		val6 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_01);
		if (dce_level == LEVEL_WEEK)
			val6 = MXD_REG_MOD(val6, COLOR_REGION_GAIN_WEEK,
			                          COLOR_REGION_GAIN);
		else if (dce_level == LEVEL_MEDIUM)
			val6 = MXD_REG_MOD(val6, COLOR_REGION_GAIN_MEDIUM,
			                     	COLOR_REGION_GAIN);
		else if (dce_level == LEVEL_STRONG)
			val6 = MXD_REG_MOD(val6, COLOR_REGION_GAIN_STRONG,
			                       COLOR_REGION_GAIN);

		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_01, val6);
	}
	else
	{
		val = MXD_REG_MOD(val, DCE_YUV_SELECTION_VAL,
                                               DCE_Y_SELECTION);
		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_00, val);
		val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_02);
		val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_03);
		val3 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_04);
		val4 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_05);
		val5 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_08);
		if (dce_level == LEVEL_WEEK)
		{
			val = MXD_REG_MOD(val, DISABLE, COLOR_REGION_EN);
			val = MXD_REG_MOD(val, Y_GRAD_GAIN_WEEK, Y_GRAD_GAIN);
			val = MXD_REG_MOD(val, CB_GRAD_GAIN_WEEK, CB_GRAD_GAIN);
			val = MXD_REG_MOD(val, CR_GRAD_GAIN_WEEK, CR_GRAD_GAIN);
			val2 = MXD_REG_MOD(val2, Y_RANGE_MIN_WEEK, Y_RANGE_MIN);
			val2 = MXD_REG_MOD(val2, Y_RANGE_MAX_WEEK, Y_RANGE_MAX);
			val3 = MXD_REG_MOD(val3, CB_RANGE_MIN_VALUE, CB_RANGE_MIN);
			val3 = MXD_REG_MOD(val3, CB_RANGE_MAX_VALUE, CB_RANGE_MAX);
			val4 = MXD_REG_MOD(val4, CR_RANGE_MIN_VALUE, CR_RANGE_MIN);
			val4 = MXD_REG_MOD(val4, CR_RANGE_MAX_VALUE, CR_RANGE_MAX);
			val5 = MXD_REG_MOD(val5, HIF_DYC_WDATA_Y32ND_WEEK,
                        HIF_DYC_WDATA_Y_32ND);
			val5 = MXD_REG_MOD(val5, HIF_DYC_WDATA_X32ND_WEEK,
                        HIF_DYC_WDATA_X_32ND);
		}
		else if (dce_level == LEVEL_MEDIUM)
		{
			val = MXD_REG_MOD(val, DISABLE, COLOR_REGION_EN);
			val = MXD_REG_MOD(val, Y_GRAD_GAIN_MEDIUM, Y_GRAD_GAIN);
			val = MXD_REG_MOD(val, CB_GRAD_GAIN_MEDIUM, CB_GRAD_GAIN);
			val = MXD_REG_MOD(val, CR_GRAD_GAIN_MEDIUM, CR_GRAD_GAIN);
			val2 = MXD_REG_MOD(val2, Y_RANGE_MIN_MEDIUM, Y_RANGE_MIN);
			val2 = MXD_REG_MOD(val2, Y_RANGE_MAX_MEDIUM, Y_RANGE_MAX);
			val3 = MXD_REG_MOD(val3, CB_RANGE_MIN_VALUE, CB_RANGE_MIN);
			val3 = MXD_REG_MOD(val3, CB_RANGE_MAX_VALUE, CB_RANGE_MAX);
			val4 = MXD_REG_MOD(val4, CR_RANGE_MIN_VALUE, CR_RANGE_MIN);
			val4 = MXD_REG_MOD(val4, CR_RANGE_MAX_VALUE, CR_RANGE_MAX);
			val5 = MXD_REG_MOD(val5, HIF_DYC_WDATA_Y32ND_MEDIUM,
                        HIF_DYC_WDATA_Y_32ND);
			val5 = MXD_REG_MOD(val5, HIF_DYC_WDATA_X32ND_MEDIUM,
                        HIF_DYC_WDATA_X_32ND);
		}
		else if (dce_level == LEVEL_STRONG)
		{
			val = MXD_REG_MOD(val, ENABLE, COLOR_REGION_EN);
			val = MXD_REG_MOD(val, Y_GRAD_GAIN_STRONG, Y_GRAD_GAIN);
			val = MXD_REG_MOD(val, CB_GRAD_GAIN_STRONG, CB_GRAD_GAIN);
			val = MXD_REG_MOD(val, CR_GRAD_GAIN_STRONG, CR_GRAD_GAIN);
			val2 = MXD_REG_MOD(val2, Y_RANGE_MIN_STRONG, Y_RANGE_MIN);
			val2 = MXD_REG_MOD(val2, Y_RANGE_MAX_STRONG, Y_RANGE_MAX);
			val3 = MXD_REG_MOD(val3, CB_RANGE_MIN_VALUE, CB_RANGE_MIN);
			val3 = MXD_REG_MOD(val3, CB_RANGE_MAX_VALUE, CB_RANGE_MAX);
			val4 = MXD_REG_MOD(val4, CR_RANGE_MIN_VALUE, CR_RANGE_MIN);
			val4 = MXD_REG_MOD(val4, CR_RANGE_MAX_VALUE, CR_RANGE_MAX);
			val5 = MXD_REG_MOD(val5, HIF_DYC_WDATA_Y32ND_STRONG,
                         HIF_DYC_WDATA_Y_32ND);
			val5 = MXD_REG_MOD(val5, HIF_DYC_WDATA_X32ND_STRONG,
                         HIF_DYC_WDATA_X_32ND);
		}


		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_02, val);
		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_03, val2);
		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_04, val3);
		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_05, val4);
		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_08, val5);

	}



}

void _mxd_set_ce_dce_color_region(void)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_00);
	val = MXD_REG_MOD(val, COLOR_REGION0_SEL_VALUE, COLOR_REGION0_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION1_SEL_VALUE, COLOR_REGION1_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION2_SEL_VALUE, COLOR_REGION2_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION3_SEL_VALUE, COLOR_REGION3_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION4_SEL_VALUE, COLOR_REGION4_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION5_SEL_VALUE, COLOR_REGION5_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION6_SEL_VALUE, COLOR_REGION6_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION7_SEL_VALUE, COLOR_REGION7_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION8_SEL_VALUE, COLOR_REGION8_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION9_SEL_VALUE, COLOR_REGION9_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION10_SEL_VALUE, COLOR_REGION10_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION11_SEL_VALUE, COLOR_REGION12_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION13_SEL_VALUE, COLOR_REGION13_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION14_SEL_VALUE, COLOR_REGION14_SEL);
	val = MXD_REG_MOD(val, COLOR_REGION15_SEL_VALUE, COLOR_REGION15_SEL);
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_00, val);

	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_02);
	val = MXD_REG_MOD(val, ENABLE, COLOR_REGION_EN);
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_02, val);
}

void odin_mxd_set_ce_dce(bool DCEEn, MXD_Level dce_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_00);
	val = MXD_REG_MOD(val, DCEEn, DYNAMIC_CONTRAST_EN);
	if (DCEEn)
	{
		_mxd_set_ce_dce_color_region();
		_mxd_set_ce_dce_set_domain(ENABLE, LEVEL_WEEK);
		odin_dma_default_set();
	}
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DCE_CTRL_00, val);

}

void _mxd_set_ce_cen_lut(bool ReadEn, ceCenLUTMode mode)
	{
		u32 val, val2;
		int index;
		if (!ReadEn)
		{
			val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
			val = MXD_REG_MOD(val, ReadEn, CEN_LUT_EN);
			val = MXD_REG_MOD(val, CEN_LUT_WEN_WRITE, CEN_LUT_WEN);
			mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

			switch (mode)
			{
			case CE_CEN_LUT_HUE:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 8; index++)
				{
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_WRITE, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA);
					val2 = MXD_REG_MOD(val2, (cen_lut_h_8point[index]*(0x10000))
                                   +cen_lut_h_gain[index], CEN_LUT_DATA);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA, val2);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				}
				break;

			case CE_CEN_LUT_SATURATION:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);


				for (index = 0; index < 8; index++)
				{
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_WRITE, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA);
					val2 = MXD_REG_MOD(val2, (cen_lut_s_8point[index]*(0x10000))
                                  +cen_lut_s_gain[index], CEN_LUT_DATA);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA, val2);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);
				}
				break;

			case CE_CEN_LUT_VALUE:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 8; index++)
				{
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_WRITE, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA);
					val2 = MXD_REG_MOD(val2, cen_lut_v_8point[index]*(0x10000)
                                  +cen_lut_v_gain[index], CEN_LUT_DATA);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA, val2);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);
				}
				break;

			case CE_CEN_LUT_DEBUGCOLOR:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 16; index++)
				{

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_WRITE, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA);
					val2 = MXD_REG_MOD(val2, cen_lut_debug_color[index],
                                        CEN_LUT_DATA);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA, val2);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);
				}
				break;

			case CE_CEN_LUT_DELTAGAIN:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 16; index++)
				{
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_WRITE, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA);
					val2 = MXD_REG_MOD(val2,
                                     cen_lut_delta_gain_v[index]*(0x10000)
					    +cen_lut_delta_gain_s[index]*(0x100)
					    +cen_lut_delta_gain_h[index], CEN_LUT_DATA);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA, val2);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);
				}
				break;

			case CE_CEN_LUT_GLOBALMASTERGAIN:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 16; index++)
				{
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_WRITE, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA);
					val2 = MXD_REG_MOD(val2, cen_lut_global_master_gain[index],
                                     CEN_LUT_DATA);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA, val2);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);
				}
				break;

			case CE_CEN_LUT_GLOBALDELTAGAIN:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 6; index++)
				{
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_WRITE, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val2 = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA);
					val2 = MXD_REG_MOD(val2, cen_lut_global_delta_gain[index],
                                     CEN_LUT_DATA);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA, val2);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);
				}
				break;
			}
		}
		else
		{
			switch (mode)
			{
				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, ReadEn, CEN_LUT_EN);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, CEN_LUT_WEN_READ, CEN_LUT_WEN);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				case CE_CEN_LUT_HUE:
				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 8; index++)
				{
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_READ, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				}
				break;

				case CE_CEN_LUT_SATURATION:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 8; index++)
				{
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_READ, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);
				}
				break;

				case CE_CEN_LUT_VALUE:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 8; index++)
				{
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_READ, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);
				}
				break;

			case CE_CEN_LUT_DEBUGCOLOR:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 16; index++)
				{

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_READ, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);
				}
				break;

			case CE_CEN_LUT_DELTAGAIN:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 16; index++)
				{
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_READ, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);
				}
				break;

			case CE_CEN_LUT_GLOBALMASTERGAIN:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 16; index++)
				{
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_READ, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);
				}
				break;

			case CE_CEN_LUT_GLOBALDELTAGAIN:

				val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
				val = MXD_REG_MOD(val, mode, CEN_LUT_CTRL_MODE);
				mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

				for (index = 0; index < 6; index++)
				{
					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, CEN_LUT_WEN_READ, CEN_LUT_WEN);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);

					val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
					val = MXD_REG_MOD(val, index, CEN_LUT_ADRS);
					mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, val);
				}
				break;
			}
		}


	}
void _mxd_set_ce_dark_enh(bool DarkEn, MXD_Level dark_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_04);
	val = MXD_REG_MOD(val, DarkEn, DEN_CTRL0);

	if (DarkEn)
	{
		if (dark_level == LEVEL_STRONG)
		{
			val = MXD_REG_MOD(val, DEN_APL_LIMIT_HIGH_STRONG,
                                                      DEN_APL_LIMIT_HIGH);
			val = MXD_REG_MOD(val, DEN_GAIN_STRONG, DEN_GAIN);
			val = MXD_REG_MOD(val, DEN_CORING_STRONG, DEN_CORING);
		}
		else if (dark_level == LEVEL_MEDIUM)
		{
			val = MXD_REG_MOD(val, DEN_APL_LIMIT_HIGH_MEDIUM,
                                                       DEN_APL_LIMIT_HIGH);
			val = MXD_REG_MOD(val, DEN_GAIN_MEDIUM, DEN_GAIN);
			val = MXD_REG_MOD(val, DEN_CORING_MEDIUM, DEN_CORING);
		}
		else if (dark_level == LEVEL_WEEK)
		{
			val = MXD_REG_MOD(val, DEN_APL_LIMIT_HIGH_WEEK,
                                                    DEN_APL_LIMIT_HIGH);
			val = MXD_REG_MOD(val, DEN_GAIN_WEEK, DEN_GAIN);
			val = MXD_REG_MOD(val, DEN_CORING_WEEK, DEN_CORING);
		}
	}

	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_04, val);
}

void _mxd_set_ce_hsvoffsetctrl(MXD_Level h_level,
         MXD_Level s_level, MXD_Level v_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_03);
	if (h_level == LEVEL_STRONG)
	{
		val = MXD_REG_MOD(val, IHSV_HOFFSET_STRONG, IHSV_HOFFSET);
	}
	else if (h_level == LEVEL_MEDIUM)
	{
		val = MXD_REG_MOD(val, IHSV_HOFFSET_MEDIUM, IHSV_HOFFSET);
	}
	else if (h_level == LEVEL_WEEK)
	{
		val = MXD_REG_MOD(val, IHSV_HOFFSET_WEEK, IHSV_HOFFSET);
	}

	if (s_level == LEVEL_STRONG)
	{
		val = MXD_REG_MOD(val, IHSV_SOFFSET_STRONG, IHSV_SOFFSET);
	}
	else if (s_level == LEVEL_MEDIUM)
	{
		val = MXD_REG_MOD(val, IHSV_SOFFSET_MEDIUM, IHSV_SOFFSET);
	}
	else if (s_level == LEVEL_WEEK)
	{
		val = MXD_REG_MOD(val, IHSV_SOFFSET_WEEK, IHSV_SOFFSET);
	}

	if (v_level == LEVEL_STRONG)
	{
		val = MXD_REG_MOD(val, IHSV_VOFFSET_STRONG, IHSV_VOFFSET);
	}
	else if (v_level == LEVEL_MEDIUM)
	{
		val = MXD_REG_MOD(val, IHSV_VOFFSET_MEDIUM, IHSV_VOFFSET);
	}
	else if (v_level == LEVEL_WEEK)
	{
		val = MXD_REG_MOD(val, IHSV_VOFFSET_WEEK, IHSV_VOFFSET);
	}
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_03, val);

}

void _mxd_set_ce_svgainctrl(MXD_Level s_level, MXD_Level v_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_02);
	if (s_level == LEVEL_STRONG)
	{
		val = MXD_REG_MOD(val, IHSV_SGAIN_STRONG, IHSV_SGAIN);
	}
	else if (s_level == LEVEL_MEDIUM)
	{
		val = MXD_REG_MOD(val, IHSV_SGAIN_MEDIUM, IHSV_SGAIN);
	}
	else if (s_level == LEVEL_WEEK)
	{
		val = MXD_REG_MOD(val, IHSV_SGAIN_WEEK, IHSV_SGAIN);
	}

	if (v_level == LEVEL_STRONG)
	{
		val = MXD_REG_MOD(val, IHSV_VGAIN_STRONG, IHSV_VGAIN);
	}
	else if (v_level == LEVEL_MEDIUM)
	{
		val = MXD_REG_MOD(val, IHSV_VGAIN_MEDIUM, IHSV_VGAIN);
	}
	else if (v_level == LEVEL_WEEK)
	{
		val = MXD_REG_MOD(val, IHSV_VGAIN_WEEK, IHSV_VGAIN);
	}
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_02, val);
}

void _mxd_set_ce_colorregion(void)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_01);
	val = MXD_REG_MOD(val, SHOW_COLOR_REGION0_VAL, SHOW_COLOR_REGION0);
	val = MXD_REG_MOD(val, SHOW_COLOR_REGION1_VAL, SHOW_COLOR_REGION0);
	val = MXD_REG_MOD(val, COLOR_REGION_EN0_VAL, COLOR_REGION_EN0);
	val = MXD_REG_MOD(val, COLOR_REGION_EN1_VAL, COLOR_REGION_EN1);
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_01, val);
}


void _mxd_set_ce_vspselect(bool gain_En)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_00);

	val = MXD_REG_MOD(val, gain_En, SELECT_HSV);
	val = MXD_REG_MOD(val, gain_En, SELECT_RGB);
	val = MXD_REG_MOD(val, gain_En, VSP_SEL);
	if (gain_En)
	{
		val = MXD_REG_MOD(val, gain_En, V_SCALER_EN);
	}
	val = MXD_REG_MOD(val, gain_En, GLOBAL_APL_SEL);
	val = MXD_REG_MOD(val, gain_En, 1ST_CORE_GAIN_DISABLE);
	val = MXD_REG_MOD(val, gain_En, 2ND_CORE_GAIN_DISABLE);

	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_00, val);
}

void _mxd_set_ce_globaltonesetting(MXD_Level tone_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_FILM_CTRL_02);
	if (tone_level == LEVEL_STRONG)
	{
		val = MXD_REG_MOD(val, TONE_GAIN_STRONG, TONE_GAIN);
		val = MXD_REG_MOD(val, TONE_OFFSET_STRONG, TONE_OFFSET);
	}
	else if (tone_level == LEVEL_MEDIUM)
	{
		val = MXD_REG_MOD(val, TONE_GAIN_MEDIUM, TONE_GAIN);
		val = MXD_REG_MOD(val, TONE_OFFSET_BYPASS, TONE_OFFSET);
	}
	else if (tone_level == LEVEL_WEEK)
	{
		val = MXD_REG_MOD(val, TONE_GAIN_WEEK, TONE_GAIN);
		val = MXD_REG_MOD(val, TONE_OFFSET_WEEK, TONE_OFFSET);
	}
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_FILM_CTRL_02, val);
}

void odin_mxd_set_ce_colorenhance(bool CEN_En, MXD_Level CEN_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_00);
	val = MXD_REG_MOD(val, CEN_En, REG_CEN_BYPASS);
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_00, val);

	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_00);

	if (CEN_En)
	{
	_mxd_set_ce_vspselect(CEN_level);
	_mxd_set_ce_globaltonesetting(CEN_level);
	_mxd_set_ce_colorregion();
	_mxd_set_ce_svgainctrl(LEVEL_STRONG, LEVEL_STRONG);
	_mxd_set_ce_hsvoffsetctrl(LEVEL_STRONG, LEVEL_STRONG, LEVEL_WEEK);
	_mxd_set_ce_dark_enh(CEN_En, CEN_level);

	_mxd_set_ce_cen_lut(DISABLE, CE_CEN_LUT_HUE);
	_mxd_set_ce_cen_lut(ENABLE, CE_CEN_LUT_HUE);

	_mxd_set_ce_cen_lut(DISABLE, CE_CEN_LUT_SATURATION);
	_mxd_set_ce_cen_lut(ENABLE, CE_CEN_LUT_SATURATION);

	_mxd_set_ce_cen_lut(DISABLE, CE_CEN_LUT_VALUE);
	_mxd_set_ce_cen_lut(ENABLE, CE_CEN_LUT_VALUE);

	_mxd_set_ce_cen_lut(DISABLE, CE_CEN_LUT_DEBUGCOLOR);
	_mxd_set_ce_cen_lut(ENABLE, CE_CEN_LUT_DEBUGCOLOR);

	_mxd_set_ce_cen_lut(DISABLE, CE_CEN_LUT_DELTAGAIN);
	_mxd_set_ce_cen_lut(ENABLE, CE_CEN_LUT_DELTAGAIN);

	_mxd_set_ce_cen_lut(DISABLE, CE_CEN_LUT_GLOBALMASTERGAIN);
	_mxd_set_ce_cen_lut(ENABLE, CE_CEN_LUT_GLOBALMASTERGAIN);

	_mxd_set_ce_cen_lut(DISABLE, CE_CEN_LUT_GLOBALDELTAGAIN);
	_mxd_set_ce_cen_lut(ENABLE, CE_CEN_LUT_GLOBALDELTAGAIN);

	}
}

void odin_mxd_set_ce_globalctrstnbrtnsctrl(bool Contrast_En,
                         MXD_Level cr_level, MXD_Level br_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_VSPRGB_CTRL_00);
	if (Contrast_En)
	{
		val = MXD_REG_MOD(val, ENABLE, CONTRAST_ENABLE);

		if (cr_level == LEVEL_STRONG)
		{
			val = MXD_REG_MOD(val, CONTRAST_CTRL_STRONG, CONTRAST_CTRL);
			val = MXD_REG_MOD(val, CENTER_POSITION_STRONG, CENTER_POSITION);
		}

		else if (cr_level == LEVEL_MEDIUM)
		{
			val = MXD_REG_MOD(val, CONTRAST_CTRL_MEDIUM, CONTRAST_CTRL);
			val = MXD_REG_MOD(val, CENTER_POSITION_MEDIUM, CENTER_POSITION);
		}
		else if (cr_level == LEVEL_WEEK)
		{
			val = MXD_REG_MOD(val, CONTRAST_CTRL_WEEK, CONTRAST_CTRL);
			val = MXD_REG_MOD(val, CENTER_POSITION_WEEK, CENTER_POSITION);
		}


		if (br_level == LEVEL_STRONG)
		{
			val = MXD_REG_MOD(val, BRIGHTNESS_CONTROL_STRONG, BRIGHTNESS);
		}

		else if (br_level == LEVEL_MEDIUM)
		{
			val = MXD_REG_MOD(val, BRIGHTNESS_CONTROL_MEDIUM, BRIGHTNESS);
		}
		else if (br_level == LEVEL_WEEK)
		{
			val = MXD_REG_MOD(val, BRIGHTNESS_CONTROL_WEEK, BRIGHTNESS);
		}

	}
	else
		val = MXD_REG_MOD(val, DISABLE, CONTRAST_ENABLE);

	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_VSPRGB_CTRL_00, val);

}

void odin_mxd_set_ce_whitebalance(bool WB_En, MXD_Level WB_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_00);
	val = MXD_REG_MOD(val, WB_En, WB_EN);
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_00, val);

	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_01);

	if (WB_En)
	{
		if (WB_level == LEVEL_STRONG)
		{
			val = MXD_REG_MOD(val, USER_CTRL_G_GAIN_STRONG, USER_CTRL_G_GAIN);
			val = MXD_REG_MOD(val, USER_CTRL_B_GAIN_STRONG, USER_CTRL_B_GAIN);
			val = MXD_REG_MOD(val, USER_CTRL_R_GAIN_STRONG, USER_CTRL_R_GAIN);
		}
		else if (WB_level == LEVEL_MEDIUM)
		{
			val = MXD_REG_MOD(val, USER_CTRL_G_GAIN_MEDIUM, USER_CTRL_G_GAIN);
			val = MXD_REG_MOD(val, USER_CTRL_B_GAIN_MEDIUM, USER_CTRL_B_GAIN);
			val = MXD_REG_MOD(val, USER_CTRL_R_GAIN_MEDIUM, USER_CTRL_R_GAIN);
		}
		else if (WB_level == LEVEL_WEEK)
		{
			val = MXD_REG_MOD(val, USER_CTRL_G_GAIN_WEEK, USER_CTRL_G_GAIN);
			val = MXD_REG_MOD(val, USER_CTRL_B_GAIN_WEEK, USER_CTRL_B_GAIN);
			val = MXD_REG_MOD(val, USER_CTRL_R_GAIN_WEEK, USER_CTRL_R_GAIN);
		}
		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_01, val);

		val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_02);
		val = MXD_REG_MOD(val, USER_CTRL_G_OFFSET_VALUE, USER_CTRL_G_OFFSET);
		val = MXD_REG_MOD(val, USER_CTRL_B_OFFSET_VALUE, USER_CTRL_B_OFFSET);
		val = MXD_REG_MOD(val, USER_CTRL_R_OFFSET_VALUE, USER_CTRL_R_OFFSET);
		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_02, val);
	}
}


void odin_mxd_set_ce_filmlike(bool vignettingEn, bool filmlikeEn,
	MXD_Level vignetting_level, MXD_Level filmlike_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_FILM_CTRL_00);
	val = MXD_REG_MOD(val, vignettingEn, VIGNETTING_ENABLE);
	val = MXD_REG_MOD(val, filmlikeEn, FILM_CONTRAST_ENABLE);

	if (vignettingEn)
	{
		if (vignetting_level == LEVEL_STRONG)
			val = MXD_REG_MOD(val, VIGNETTING_GAIN_STRONG, VIGNETTING_GAIN);
		else if (vignetting_level == LEVEL_MEDIUM)
			val = MXD_REG_MOD(val, VIGNETTING_GAIN_MEDIUM, VIGNETTING_GAIN);
		else if (vignetting_level == LEVEL_WEEK)
			val = MXD_REG_MOD(val, VIGNETTING_GAIN_WEEK, VIGNETTING_GAIN);
	}

	if (filmlikeEn)
	{
		if (filmlike_level == LEVEL_STRONG)
		{
			val = MXD_REG_MOD(val, CONTRAST_ALPHA_STRONG, CONTRAST_ALPHA);
			val = MXD_REG_MOD(val, CONTRAST_DELTA_MAX_STRONG,
                                                      CONTRAST_DELTA_MAX);
		}
		else if (filmlike_level == LEVEL_MEDIUM)
		{
			val = MXD_REG_MOD(val, CONTRAST_ALPHA_MEDIUM, CONTRAST_ALPHA);
			val = MXD_REG_MOD(val, CONTRAST_DELTA_MAX_MEDIUM,
                                                      CONTRAST_DELTA_MAX);
		}
		else if (filmlike_level == LEVEL_WEEK)
		{
			val = MXD_REG_MOD(val, CONTRAST_ALPHA_WEEK, CONTRAST_ALPHA);
			val = MXD_REG_MOD(val, CONTRAST_DELTA_MAX_WEEK,
                                                 CONTRAST_DELTA_MAX);
		}
	}

	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_FILM_CTRL_00, val);

}

void odin_mxd_set_ce_apl(void)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_APL_CTRL_00);
	val = MXD_REG_MOD(val, APL_WIN_X0, APL_WIN_CTRL_X0);
	val = MXD_REG_MOD(val, APL_WIN_Y0, APL_WIN_CTRL_Y0);
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_APL_CTRL_00, val);

	val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_APL_CTRL_01);
	val = MXD_REG_MOD(val, APL_WIN_X0, APL_WIN_CTRL_X1);
	val = MXD_REG_MOD(val, APL_WIN_Y0, APL_WIN_CTRL_Y1);
	mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_APL_CTRL_01, val);
}

void odin_mxd_set_re_peaking(bool peakingEn, bool natural,
                                    MXD_Level peaking_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_00);
	val = MXD_REG_MOD(val, peakingEn, EDGE_ENHANCE_ENABLE);
	if (peakingEn)
	{
		if (peaking_level == LEVEL_STRONG)
			val = MXD_REG_MOD(val, EDGE_OPERATOR_CON_SOBEL,
			                      EDGE_OPERATOR_SELECTION);
		else if (peaking_level == LEVEL_MEDIUM)
			val = MXD_REG_MOD(val, EDGE_OPERATOR_CON_SOBEL,
			                  EDGE_OPERATOR_SELECTION);
		else if (peaking_level == LEVEL_WEEK)
			val = MXD_REG_MOD(val, EDGE_OPERATOR_MOD_SOBEL,
			                       EDGE_OPERATOR_SELECTION);

		mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_00, val);

		_mxd_set_re_peaking(peaking_level, natural);

	}
}

void _mxd_set_re_coring(MXD_Level coring_level)
{
	u32 val;

	val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_DP_CTRL);

	if (coring_level == LEVEL_STRONG)
		val = MXD_REG_MOD(val, RE_SP_GAIN_E_B_STRONG, SP_GAIN_E_B);
	else if (coring_level == LEVEL_WEEK)
		val = MXD_REG_MOD(val, RE_SP_GAIN_E_B_WEEK, SP_GAIN_E_B);
	else if (coring_level == LEVEL_MEDIUM)
		val = MXD_REG_MOD(val, RE_SP_GAIN_E_B, SP_GAIN_E_B);

	if (coring_level == LEVEL_STRONG)
		val = MXD_REG_MOD(val, RE_SP_GAIN_E_W_STRONG, SP_GAIN_E_W);
	else if (coring_level == LEVEL_WEEK)
		val = MXD_REG_MOD(val, RE_SP_GAIN_E_W_WEEK, SP_GAIN_E_W);
	else
		val = MXD_REG_MOD(val, RE_SP_GAIN_E_W, SP_GAIN_E_W);

	if (coring_level == LEVEL_STRONG)
		val = MXD_REG_MOD(val, RE_SP_GAIN_T_B_STRONG, SP_GAIN_T_B);
	else if (coring_level == LEVEL_WEEK)
		val = MXD_REG_MOD(val, RE_SP_GAIN_T_B_WEEK, SP_GAIN_T_B);
	else
		val = MXD_REG_MOD(val, RE_SP_GAIN_T_B, SP_GAIN_T_B);

	if (coring_level == LEVEL_STRONG)
		val = MXD_REG_MOD(val, RE_SP_GAIN_T_W_STRONG, SP_GAIN_T_W);
	else if (coring_level == LEVEL_WEEK)
		val = MXD_REG_MOD(val, RE_SP_GAIN_T_W_WEEK, SP_GAIN_T_W);
	else
		val = MXD_REG_MOD(val, RE_SP_GAIN_T_W, SP_GAIN_T_W);

	mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_DP_CTRL, val);

}

void odin_mxd_set_re_coring(bool coring, MXD_Level coring_level)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_CORING_CTRL);
	val = MXD_REG_MOD(val, CORING_MODE_BOTH, CORING_MODE);
	val = MXD_REG_MOD(val, coring, REG_CORING_EN);

	if (coring)
	{
		if (coring_level == LEVEL_STRONG)
		{
			val = MXD_REG_MOD(val, WEIGHT_A_STRONG, WEIGHT_A);
			val = MXD_REG_MOD(val, WEIGHT_T_STRONG, WEIGHT_T);
		}
		else if (coring_level == LEVEL_WEEK)
		{
			val = MXD_REG_MOD(val, WEIGHT_A_WEEK, WEIGHT_A);
			val = MXD_REG_MOD(val, WEIGHT_T_WEEK, WEIGHT_T);
		}
		else
		{
			val = MXD_REG_MOD(val, WEIGHT_A_MEDIUM, WEIGHT_A);
			val = MXD_REG_MOD(val, WEIGHT_T_MEDIUM, WEIGHT_T);
		}

		mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_CORING_CTRL, val);
		_mxd_set_re_coring(coring_level);
	}
}

void re_disable(void)
{
	odin_mxd_set_re_peaking(DISABLE, DISABLE, LEVEL_WEEK);
	odin_mxd_set_re_coring(DISABLE, LEVEL_WEEK);
}

void odin_mxd_set_nr_win0_x_y_tune(int winx, int winy)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_00);
	val = MXD_REG_MOD(val, winx, WIN0_X0);
       val = MXD_REG_MOD(val, winy, WIN0_Y0);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_00, val);
}

void odin_mxd_set_nr_win1_x_y_tune(int winx, int winy)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_01);
	val = MXD_REG_MOD(val, winx, WIN1_X1);
       val = MXD_REG_MOD(val, winy, WIN1_Y1);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_01, val);
}

void odin_mxd_set_nr_win2_x_y_tune(int winx, int winy)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_02);
	val = MXD_REG_MOD(val, winx, WIN2_X0);
       val = MXD_REG_MOD(val, winy, WIN2_Y0);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_02, val);
}

void odin_mxd_set_nr_win3_x_y_tune(int winx, int winy)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_03);
	val = MXD_REG_MOD(val, winx, WIN3_X1);
       val = MXD_REG_MOD(val, winy, WIN3_Y1);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_03, val);
}


void _mxd_set_nr_vfilter(bool vFilterEn)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_VFLT_CTRL_00);
	val = MXD_REG_MOD(val, vFilterEn, VFILTERENABLE);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_VFLT_CTRL_00, val);
    printk("mxd: vFilter val  = %d\n", vFilterEn);
}

void odin_set_nr_vfilter(MXD_Level nr_level)
{
	if (nr_level == LEVEL_STRONG)
	{
		_mxd_set_nr_vfilter(ENABLE);
	}
}

void odin_set_nr_vfilter_tune(int vFilterEn)
{
    _mxd_set_nr_vfilter(vFilterEn);
    printk("mxd: vFilter val  = %d\n", vFilterEn);
}


void odin_mxd_set_nr_der_en_tune(int der_en)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00);
	val = MXD_REG_MOD(val, der_en, DER_EN);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00, val);
}

void odin_mxd_set_nr_der_gain_mapping_tune(int der_gain_mapping)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00);
	val = MXD_REG_MOD(val, der_gain_mapping, DER_GAIN_MAPPING);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00, val);
}

void odin_mxd_set_nr_der_bif_en_tune(int der_bif_en)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00);
	val = MXD_REG_MOD(val, der_bif_en, DER_BIF_EN);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00, val);
}

void odin_mxd_set_nr_der_bif_th_tune(int bif_th)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00);
	val = MXD_REG_MOD(val, bif_th, BIF_TH);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00, val);
}

void odin_mxd_set_nr_gain_th0_tune(int gain_th0)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_01);
	val = MXD_REG_MOD(val, gain_th0, GAIN_TH0);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_01, val);
}

void odin_mxd_set_nr_gain_th1_tune(int gain_th1)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_01);
	val = MXD_REG_MOD(val, gain_th1, GAIN_TH1);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_01, val);
}


void _mxd_set_nr_der(MXD_Level der_level)
{
	u32 val;

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00);
	val = MXD_REG_MOD(val, ENABLE, DER_EN);
	val = MXD_REG_MOD(val, ENABLE, DER_GAIN_MAPPING);
	val = MXD_REG_MOD(val, ENABLE, DER_BIF_EN);

	if (der_level == LEVEL_STRONG)
		val = MXD_REG_MOD(val, DER_BIF_TH_STRONG, BIF_TH);
	else if (der_level == LEVEL_WEEK)
		val = MXD_REG_MOD(val, DER_BIF_TH_WEEK, BIF_TH);
	else
		val = MXD_REG_MOD(val, DER_BIF_TH_MEDIUM, BIF_TH);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00, val);

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_01);
	val = MXD_REG_MOD(val, DER_GAIN_TH0, GAIN_TH0);
	val = MXD_REG_MOD(val, DER_GAIN_TH1, GAIN_TH1);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_01, val);
}

void odin_mxd_set_nr_der(MXD_Level  der_level)
{
	_mxd_set_nr_der(der_level);

}

void odin_mxd_set_nr_mnr_ctrl00_mnr_enable_tune(int mnrEn)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_00);
	val = MXD_REG_MOD(val, mnrEn, MNR_ENABLE);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_00, val);
}

void odin_mxd_set_nr_mnr_ctrl00_edgegain_map_enable_tune(int edgeEn)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_00);
	val = MXD_REG_MOD(val, edgeEn, EDGE_GAIN_MAPPING_ENABLE);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_00, val);
}

void odin_mxd_set_nr_mnr_ctrl01_hcoef00_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01);
	val = MXD_REG_MOD(val, hcoef, HCOEF_00);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01, val);
}

void odin_mxd_set_nr_mnr_ctrl01_hcoef01_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01);
	val = MXD_REG_MOD(val, hcoef, HCOEF_01);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01, val);
}

void odin_mxd_set_nr_mnr_ctrl01_hcoef02_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01);
	val = MXD_REG_MOD(val, hcoef, HCOEF_02);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01, val);
}

void odin_mxd_set_nr_mnr_ctrl01_hcoef03_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01);
	val = MXD_REG_MOD(val, hcoef, HCOEF_03);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01, val);
}

void odin_mxd_set_nr_mnr_ctrl02_hcoef04_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02);
	val = MXD_REG_MOD(val, hcoef, HCOEF_04);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02, val);
}

void odin_mxd_set_nr_mnr_ctrl02_hcoef05_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02);
	val = MXD_REG_MOD(val, hcoef, HCOEF_05);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02, val);
}

void odin_mxd_set_nr_mnr_ctrl02_hcoef06_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02);
	val = MXD_REG_MOD(val, hcoef, HCOEF_06);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02, val);
}

void odin_mxd_set_nr_mnr_ctrl02_hcoef07_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02);
	val = MXD_REG_MOD(val, hcoef, HCOEF_07);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02, val);
}

void odin_mxd_set_nr_mnr_ctrl02_hcoef08_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02);
	val = MXD_REG_MOD(val, hcoef, HCOEF_08);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02, val);
}

void odin_mxd_set_nr_mnr_ctrl02_hcoef09_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02);
	val = MXD_REG_MOD(val, hcoef, HCOEF_09);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02, val);
}

void odin_mxd_set_nr_mnr_ctrl02_hcoef10_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02);
	val = MXD_REG_MOD(val, hcoef, HCOEF_10);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02, val);
}

void odin_mxd_set_nr_mnr_ctrl02_hcoef11_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02);
	val = MXD_REG_MOD(val, hcoef, HCOEF_11);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02, val);
}

void odin_mxd_set_nr_mnr_ctrl03_hcoef12_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03);
	val = MXD_REG_MOD(val, hcoef, HCOEF_12);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03, val);
}

void odin_mxd_set_nr_mnr_ctrl03_hcoef13_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03);
	val = MXD_REG_MOD(val, hcoef, HCOEF_13);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03, val);
}

void odin_mxd_set_nr_mnr_ctrl03_hcoef14_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03);
	val = MXD_REG_MOD(val, hcoef, HCOEF_14);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03, val);
}

void odin_mxd_set_nr_mnr_ctrl03_hcoef15_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03);
	val = MXD_REG_MOD(val, hcoef, HCOEF_15);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03, val);
}

void odin_mxd_set_nr_mnr_ctrl03_hcoef16_tune(int hcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03);
	val = MXD_REG_MOD(val, hcoef, HCOEF_16);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03, val);
}

void odin_mxd_set_nr_mnr_ctrl03_x1_position_tune(int x1_position)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03);
	val = MXD_REG_MOD(val, x1_position, X1_POSITION);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03, val);
}

void odin_mxd_set_nr_mnr_ctrl04_x2_position_tune(int x2_position)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04);
	val = MXD_REG_MOD(val, x2_position, X2_POSITION);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04, val);
}

void odin_mxd_set_nr_mnr_ctrl04_x3_position_tune(int x3_position)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04);
	val = MXD_REG_MOD(val, x3_position, X3_POSITION);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04, val);
}

void odin_mxd_set_nr_mnr_ctrl04_x4_position_tune(int x4_position)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04);
	val = MXD_REG_MOD(val, x4_position, X4_POSITION);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04, val);
}

void odin_mxd_set_nr_mnr_ctrl04_y1_position_tune(int y1_position)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04);
	val = MXD_REG_MOD(val, y1_position, Y1_POSITION);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04, val);
}

void odin_mxd_set_nr_mnr_ctrl05_y2_position_tune(int y2_position)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05);
	val = MXD_REG_MOD(val, y2_position, Y2_POSITION);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05, val);
}

void odin_mxd_set_nr_mnr_ctrl05_y3_position_tune(int y3_position)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05);
	val = MXD_REG_MOD(val, y3_position, Y3_POSITION);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05, val);
}

void odin_mxd_set_nr_mnr_ctrl05_y4_position_tune(int y4_position)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05);
	val = MXD_REG_MOD(val, y4_position, Y4_POSITION);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05, val);
}

void odin_mxd_set_nr_mnr_ctrl05_filter_threshold_tune(int filter_threshold)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05);
	val = MXD_REG_MOD(val, filter_threshold, FILTER_THRESHOLD);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05, val);
}

void odin_mxd_set_nr_mnr_ctrl06_vcoef0_tune(int vcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06);
	val = MXD_REG_MOD(val, vcoef, VCOEF0);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06, val);
}

void odin_mxd_set_nr_mnr_ctrl06_vcoef1_tune(int vcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06);
	val = MXD_REG_MOD(val, vcoef, VCOEF1);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06, val);
}

void odin_mxd_set_nr_mnr_ctrl06_vcoef2_tune(int vcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06);
	val = MXD_REG_MOD(val, vcoef, VCOEF2);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06, val);
}

void odin_mxd_set_nr_mnr_ctrl06_vcoef3_tune(int vcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06);
	val = MXD_REG_MOD(val, vcoef, VCOEF3);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06, val);
}

void odin_mxd_set_nr_mnr_ctrl06_vcoef4_tune(int vcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06);
	val = MXD_REG_MOD(val, vcoef, VCOEF4);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06, val);
}

void odin_mxd_set_nr_mnr_ctrl06_vcoef5_tune(int vcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06);
	val = MXD_REG_MOD(val, vcoef, VCOEF5);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06, val);
}

void odin_mxd_set_nr_mnr_ctrl06_vcoef6_tune(int vcoef)
{
       u32 val;
       val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06);
	val = MXD_REG_MOD(val, vcoef, VCOEF6);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06, val);
}

void _mxd_set_nr_mnr(MXD_Level  mnr_level)
{

	u32 val;

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_00);
	val = MXD_REG_MOD(val, ENABLE, MNR_ENABLE);
	val = MXD_REG_MOD(val, ENABLE, EDGE_GAIN_MAPPING_ENABLE);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_00, val);

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01);
	val = MXD_REG_MOD(val, mnr_h_coef[0], HCOEF_00);
	val = MXD_REG_MOD(val, mnr_h_coef[1], HCOEF_01);
	val = MXD_REG_MOD(val, mnr_h_coef[2], HCOEF_02);
	val = MXD_REG_MOD(val, mnr_h_coef[3], HCOEF_03);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01, val);

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02);
	val = MXD_REG_MOD(val, mnr_h_coef[4], HCOEF_04);
	val = MXD_REG_MOD(val, mnr_h_coef[5], HCOEF_05);
	val = MXD_REG_MOD(val, mnr_h_coef[6], HCOEF_06);
	val = MXD_REG_MOD(val, mnr_h_coef[7], HCOEF_07);
	val = MXD_REG_MOD(val, mnr_h_coef[8], HCOEF_08);
	val = MXD_REG_MOD(val, mnr_h_coef[9], HCOEF_09);
	val = MXD_REG_MOD(val, mnr_h_coef[10], HCOEF_10);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02, val);

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03);
	val = MXD_REG_MOD(val, mnr_h_coef[11], HCOEF_11);
	val = MXD_REG_MOD(val, mnr_h_coef[12], HCOEF_12);
	val = MXD_REG_MOD(val, mnr_h_coef[13], HCOEF_13);
	val = MXD_REG_MOD(val, mnr_h_coef[14], HCOEF_14);
	val = MXD_REG_MOD(val, mnr_h_coef[15], HCOEF_15);
	val = MXD_REG_MOD(val, mnr_h_coef[16], HCOEF_16);
	val = MXD_REG_MOD(val, mnr_gain_coef_xy[0].sx, X1_POSITION);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03, val);

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04);
	val = MXD_REG_MOD(val, mnr_gain_coef_xy[1].sx, X2_POSITION);
	val = MXD_REG_MOD(val, mnr_gain_coef_xy[2].sx, X3_POSITION);
	val = MXD_REG_MOD(val, mnr_gain_coef_xy[3].sx, X4_POSITION);
	val = MXD_REG_MOD(val, mnr_gain_coef_xy[0].sy, Y1_POSITION);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04, val);

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05);
	val = MXD_REG_MOD(val, mnr_gain_coef_xy[1].sy, Y2_POSITION);
	val = MXD_REG_MOD(val, mnr_gain_coef_xy[2].sy, Y3_POSITION);
	val = MXD_REG_MOD(val, mnr_gain_coef_xy[3].sy, Y4_POSITION);
	if (mnr_level == LEVEL_STRONG)
		val = MXD_REG_MOD(val, MNR_FILTER_THRESHOLD_STRONG, FILTER_THRESHOLD);
	else if (mnr_level == LEVEL_WEEK)
		val = MXD_REG_MOD(val, MNR_FILTER_THRESHOLD_WEEK, FILTER_THRESHOLD);
	else
		val = MXD_REG_MOD(val, MNR_FILTER_THRESHOLD_MEDIUM, FILTER_THRESHOLD);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05, val);

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06);
	val = MXD_REG_MOD(val, mnr_v_coef[0], VCOEF0);
	val = MXD_REG_MOD(val, mnr_v_coef[1], VCOEF1);
	val = MXD_REG_MOD(val, mnr_v_coef[2], VCOEF2);
	val = MXD_REG_MOD(val, mnr_v_coef[3], VCOEF3);
	val = MXD_REG_MOD(val, mnr_v_coef[4], VCOEF4);
	val = MXD_REG_MOD(val, mnr_v_coef[5], VCOEF5);
	val = MXD_REG_MOD(val, mnr_v_coef[6], VCOEF6);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06, val);

}

void odin_mxd_set_nr_mnr(MXD_Level mnr_level)
{
	_mxd_set_nr_mnr(mnr_level);

}

void odin_mxd_set_nr_bnr_ac_h_enable_tune(int bnrhEn)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
	 val = MXD_REG_MOD(val, bnrhEn, BNR_H_EN);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);
}

void odin_mxd_set_nr_bnr_ac_v_enable_tune(int bnrvEn)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
	 val = MXD_REG_MOD(val, bnrvEn, BNR_V_EN);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);
}

void odin_mxd_set_nr_bnr_ac_status_read_type_tune(int type)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
	 val = MXD_REG_MOD(val, type, STATUS_READ_TYPE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);
}

void odin_mxd_set_nr_bnr_ac_status_read_mode_tune(int mode)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
	 val = MXD_REG_MOD(val, mode, STATUS_READ_MODE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);
}

void odin_mxd_set_nr_bnr_ac_hbmax_gain_tune(int HBmax_gain)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
	 val = MXD_REG_MOD(val, HBmax_gain, HBMAX_GAIN);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);
}

void odin_mxd_set_nr_bnr_ac_vbmax_gain_tune(int VBmax_gain)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
	 val = MXD_REG_MOD(val, VBmax_gain, VBMAX_GAIN);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);
}

void odin_mxd_set_nr_bnr_ac_strength_resolution_tune(int strength_resolution)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
	 val = MXD_REG_MOD(val, strength_resolution, STRENGTH_RESOLUTION);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);
}

void odin_mxd_set_nr_bnr_ac_filter_type_tune(int filter_type)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
	 val = MXD_REG_MOD(val, filter_type, FITER_TYPE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);
}

void odin_mxd_set_nr_bnr_ac_output_mux_tune(int ac_output_mux)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
	 val = MXD_REG_MOD(val, ac_output_mux, BNR_OUTPUT_MUX);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);
}

void odin_mxd_set_nr_bnr_ac_strength_h_x0_tune(int strength_h_x0)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01);
	 val = MXD_REG_MOD(val, strength_h_x0, STRENGTH_H_X0);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01, val);
}

void odin_mxd_set_nr_bnr_ac_strength_h_x1_tune(int strength_h_x1)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01);
	 val = MXD_REG_MOD(val, strength_h_x1, STRENGTH_H_X1);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01, val);
}

void odin_mxd_set_nr_bnr_ac_strength_h_max_tune(int strength_h_max)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01);
	 val = MXD_REG_MOD(val, strength_h_max, STRENGTH_H_MAX);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01, val);
}

void odin_mxd_set_nr_bnr_ac_min_th_tune(int ac_min_th)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01);
	 val = MXD_REG_MOD(val, ac_min_th, AC_MIN_TH);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01, val);
}

void odin_mxd_set_nr_bnr_ac_strength_v_x0_tune(int strength_v_x0)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02);
	 val = MXD_REG_MOD(val, strength_v_x0, STRENGTH_V_X0);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02, val);
}

void odin_mxd_set_nr_bnr_ac_strength_v_x1_tune(int strength_v_x1)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02);
	 val = MXD_REG_MOD(val, strength_v_x1, STRENGTH_V_X1);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02, val);
}

void odin_mxd_set_nr_bnr_ac_strength_v_max_tune(int strength_v_max)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02);
	 val = MXD_REG_MOD(val, strength_v_max, STRENGTH_V_MAX);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02, val);
}

void odin_mxd_set_nr_bnr_h_offset_mode_tune(int h_offset_mode)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03);
	 val = MXD_REG_MOD(val, h_offset_mode, H_OFFSET_MODE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03, val);
}

void odin_mxd_set_nr_bnr_v_offset_mode_tune(int v_offset_mode)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03);
	 val = MXD_REG_MOD(val, v_offset_mode, V_OFFSET_MODE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03, val);
}

void odin_mxd_set_nr_bnr_manual_offset_h_value_tune(int manual_offset_h_value)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03);
	 val = MXD_REG_MOD(val, manual_offset_h_value, MANUAL_OFFSET_H_VALUE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03, val);
}

void odin_mxd_set_nr_bnr_manual_v_offset_mode_tune(int v_offset_mode)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03);
	 val = MXD_REG_MOD(val, v_offset_mode, V_OFFSET_MODE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03, val);
}


void odin_mxd_set_nr_bnr_manual_offset_v_value_tune(int manual_offset_v_value)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03);
	 val = MXD_REG_MOD(val, manual_offset_v_value, MANUAL_OFFSET_V_VALUE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03, val);
}

void odin_mxd_set_nr_bnr_offset_use_of_hysterisis_tune
    (int offset_use_of_hysterisis)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03);
	 val = MXD_REG_MOD(val, offset_use_of_hysterisis, OFFSET_USE_OF_HYSTERISIS);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03, val);
}


void odin_mxd_set_nr_bnr_t_filter_weight_tune(int t_filter_weight)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03);
	 val = MXD_REG_MOD(val, t_filter_weight, T_FILTER_WEIGHT);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03, val);
}


void odin_mxd_set_nr_bnr_max_delta_th0_tune(int max_delta_th0)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04);
	 val = MXD_REG_MOD(val, max_delta_th0, MAX_DELTA_TH0);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04, val);
}

void odin_mxd_set_nr_bnr_max_delta_th1_tune(int max_delta_th1)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04);
	 val = MXD_REG_MOD(val, max_delta_th1, MAX_DELTA_TH1);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04, val);
}

void odin_mxd_set_nr_bnr_h_blockness_th_tune(int h_blockness_th)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04);
	 val = MXD_REG_MOD(val, h_blockness_th, H_BLOCKNESS_TH);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04, val);
}

void odin_mxd_set_nr_bnr_h_weight_max_tune(int h_weight_max)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04);
	 val = MXD_REG_MOD(val, h_weight_max, H_WEIGHT_MAX);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04, val);
}

void odin_mxd_set_nr_bnr_scaled_use_of_hysterisis_tune
    (int scaled_use_of_hysterisis)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_05);
	 val = MXD_REG_MOD(val, scaled_use_of_hysterisis, SCALED_USE_OF_HYSTERISIS);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_05, val);
}

void odin_mxd_set_nr_enable_mnr_tune(int enable_mnr)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, enable_mnr, ENABLE_MNR);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}

void odin_mxd_set_nr_enable_der_tune(int enable_der)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, enable_der, ENABLE_DER);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}

void odin_mxd_set_nr_enable_dc_bnr_tune(int enable_dc_bnr)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, enable_dc_bnr, ENABLE_DC_BNR);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}

void odin_mxd_set_nr_enable_ac_bnr_tune(int enable_ac_bnr)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, enable_ac_bnr, ENABLE_AC_BNR);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}

void odin_mxd_set_nr_debug_enable_tune(int debug_enable)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, debug_enable, DEBUG_ENABLE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}


void odin_mxd_set_nr_debug_mode_tune(int debug_mode)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, debug_mode, DEBUG_MODE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}

void odin_mxd_set_nr_debug_mnr_enable_tune(int debug_mnr_enable)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, debug_mnr_enable, DEBUG_MNR_ENABLE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}

void odin_mxd_set_nr_debug_der_enable_tune(int debug_der_enable)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, debug_der_enable, DEBUG_DER_ENABLE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}

void odin_mxd_set_nr_debug_dc_bnr_enable_tune(int debug_dc_bnr_enable)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, debug_dc_bnr_enable, DEBUG_DC_BNR_ENABLE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}

void odin_mxd_set_nr_debug_ac_bnr_enable_tune(int debug_ac_bnr_enable)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, debug_ac_bnr_enable, DEBUG_AC_BNR_ENABLE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}

void odin_mxd_set_nr_win_control_enable_tune(int win_control_enable)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, win_control_enable, WIN_CONTROL_ENABLE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}

void odin_mxd_set_nr_border_enable_tune(int border_enable)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, border_enable, BORDER_ENABLE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}

void odin_mxd_set_nr_win_inout_tune(int win_inout)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	 val = MXD_REG_MOD(val, win_inout, WIN_INOUT);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}


void set_blockness(MXD_Level bnr_ac_level)
{
	u32 val;

	if (bnr_ac_level == LEVEL_STRONG)
	{
		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
		val = MXD_REG_MOD(val, BNR_AC_HBMAX_GAIN_STRONG, HBMAX_GAIN);
		val = MXD_REG_MOD(val, BNR_AC_VBMAX_GAIN_STRONG, VBMAX_GAIN);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);

		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01);
		val = MXD_REG_MOD(val, BNR_AC_STREMGTH_H_MAX_STRONG, STRENGTH_H_MAX);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01, val);

		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02);
		val = MXD_REG_MOD(val, BNR_AC_STREMGTH_V_MAX_STRONG, STRENGTH_V_MAX);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02, val);
	}
	else if (bnr_ac_level == LEVEL_WEEK)
	{
		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
		val = MXD_REG_MOD(val, BNR_AC_HBMAX_GAIN_WEEK, HBMAX_GAIN);
		val = MXD_REG_MOD(val, BNR_AC_VBMAX_GAIN_WEEK, VBMAX_GAIN);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);

		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01);
		val = MXD_REG_MOD(val, BNR_AC_STREMGTH_H_MAX_WEEK, STRENGTH_H_MAX);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01, val);

		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02);
		val = MXD_REG_MOD(val, BNR_AC_STREMGTH_V_MAX_WEEK, STRENGTH_V_MAX);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02, val);
	}
	else
	{
		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
		val = MXD_REG_MOD(val, BNR_AC_HBMAX_GAIN_MEDIUM, HBMAX_GAIN);
		val = MXD_REG_MOD(val, BNR_AC_VBMAX_GAIN_MEDIUM, VBMAX_GAIN);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);

		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01);
		val = MXD_REG_MOD(val, BNR_AC_STREMGTH_H_MAX_MEDIUM, STRENGTH_H_MAX);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01, val);

		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02);
		val = MXD_REG_MOD(val, BNR_AC_STREMGTH_V_MAX_MEDIUM, STRENGTH_V_MAX);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02, val);
	}

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01);
	val = MXD_REG_MOD(val, BNR_AC_STRENGTH_H_X0, STRENGTH_H_X0);
	val = MXD_REG_MOD(val, BNR_AC_STRENGTH_H_X1, STRENGTH_H_X1);
	val = MXD_REG_MOD(val, BNR_AC_MIN_THRESHOLD, AC_MIN_TH);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01, val);

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02);
	val = MXD_REG_MOD(val, BNR_AC_STRENGTH_V_X0, STRENGTH_V_X0);
	val = MXD_REG_MOD(val, BNR_AC_STRENGTH_V_X1, STRENGTH_V_X1);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02, val);

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03);
	val = MXD_REG_MOD(val, BNR_AC_H_OFFSET_MODE, H_OFFSET_MODE);
	val = MXD_REG_MOD(val, BNR_AC_MANUAL_OFFSET_H_VALUE, MANUAL_OFFSET_H_VALUE);
	val = MXD_REG_MOD(val, BNR_AC_V_OFFSET_MODE, V_OFFSET_MODE);
	val = MXD_REG_MOD(val, BNR_AC_MANUAL_OFFSET_V_VALUE, MANUAL_OFFSET_V_VALUE);
	val = MXD_REG_MOD(val, BNR_AC_OFFSET_USE_OF_HYSTERISIS,
                                   OFFSET_USE_OF_HYSTERISIS);
	val = MXD_REG_MOD(val, BNR_AC_T_FILTER_WEIGHT, T_FILTER_WEIGHT);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03, val);

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04);
	val = MXD_REG_MOD(val, BNR_AC_MAX_DELTA_TH0, MAX_DELTA_TH0);
	val = MXD_REG_MOD(val, BNR_AC_MAX_DELTA_TH1, MAX_DELTA_TH1);
	val = MXD_REG_MOD(val, BNR_AC_H_BLOCKNESS_TH, H_BLOCKNESS_TH);
	val = MXD_REG_MOD(val, BNR_AC_H_WEIGHT_MAX, H_WEIGHT_MAX);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04, val);

	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_05);
	val =
        MXD_REG_MOD(val, BNR_AC_SCALED_USE_OF_HYSTERISIS,
        SCALED_USE_OF_HYSTERISIS);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_05, val);
}

void _mxd_set_nr_bnr_ac(MXD_Level bnr_ac_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
	val = MXD_REG_MOD(val, ENABLE, BNR_H_EN);
	val = MXD_REG_MOD(val, ENABLE, BNR_V_EN);
	val = MXD_REG_MOD(val, BNR_AC_STATUS_READ_TYPE, STATUS_READ_TYPE);
	val = MXD_REG_MOD(val, BNR_AC_STATUS_READ_MODE, STATUS_READ_MODE);
	val = MXD_REG_MOD(val, BNR_AC_STRENGTH_RESOLUTION, STRENGTH_RESOLUTION);
	val = MXD_REG_MOD(val, BNR_AC_FILTER_TYPE, FITER_TYPE);
	val = MXD_REG_MOD(val, BNR_AC_OUTPUT_MUX, OUTPUT_MUX);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, val);

	set_blockness(bnr_ac_level);

}

void odin_mxd_set_nr_bnr_ac(MXD_Level bnr_ac_level)
{
	_mxd_set_nr_bnr_ac(bnr_ac_level);

}

void odin_mxd_set_nr_bnr_dc_enable_tune(int dcEn)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_00);
	 val = MXD_REG_MOD(val, dcEn, DC_BNR_ENABLE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_00, val);
}

void odin_mxd_set_nr_bnr_dc_adaptive_mode_tune(int mode)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_00);
	 val = MXD_REG_MOD(val, mode, ADAPTIVE_MODE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_00, val);
}


void odin_mxd_set_nr_bnr_dc_output_mux_mode_tune(int output_mux)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_00);
	 val = MXD_REG_MOD(val, output_mux, MUL2BLK);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_00, val);
}

void odin_mxd_set_nr_bnr_dc_blur_enable_tune(int blurEn)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_01);
	 val = MXD_REG_MOD(val, blurEn, BLUR_ENABLE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_01, val);
}

void odin_mxd_set_nr_bnr_dc_mul2blk_tune(int mul2blk)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_01);
	 val = MXD_REG_MOD(val, mul2blk, MUL2BLK);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_01, val);
}

void odin_mxd_set_nr_bnr_dc_bnr_gain_ctrl_y2_tune(int gain_ctrl_y2)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02);
	 val = MXD_REG_MOD(val, gain_ctrl_y2, DC_BNR_GAIN_CTRL_Y2);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02, val);
}

void odin_mxd_set_nr_bnr_dc_bnr_gain_ctrl_x2_tune(int gain_ctrl_x2)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02);
	 val = MXD_REG_MOD(val, gain_ctrl_x2, DC_BNR_GAIN_CTRL_X2);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02, val);
}

void odin_mxd_set_nr_bnr_dc_bnr_gain_ctrl_y3_tune(int gain_ctrl_y3)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02);
	 val = MXD_REG_MOD(val, gain_ctrl_y3, DC_BNR_GAIN_CTRL_Y3);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02, val);
}

void odin_mxd_set_nr_bnr_dc_bnr_gain_ctrl_x3_tune(int gain_ctrl_x3)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02);
	 val = MXD_REG_MOD(val, gain_ctrl_x3, DC_BNR_GAIN_CTRL_X3);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02, val);
}

void odin_mxd_set_nr_bnr_dc_bnr_gain_ctrl_y0_tune(int gain_ctrl_y0)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03);
	 val = MXD_REG_MOD(val, gain_ctrl_y0, DC_BNR_GAIN_CTRL_Y0);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03, val);
}

void odin_mxd_set_nr_bnr_dc_bnr_gain_ctrl_x0_tune(int gain_ctrl_x0)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03);
	 val = MXD_REG_MOD(val, gain_ctrl_x0, DC_BNR_GAIN_CTRL_X0);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03, val);
}

void odin_mxd_set_nr_bnr_dc_bnr_gain_ctrl_y1_tune(int gain_ctrl_y1)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03);
	 val = MXD_REG_MOD(val, gain_ctrl_y1, DC_BNR_GAIN_CTRL_Y1);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03, val);
}

void odin_mxd_set_nr_bnr_dc_bnr_gain_ctrl_x1_tune(int gain_ctrl_x1)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03);
	 val = MXD_REG_MOD(val, gain_ctrl_x1, DC_BNR_GAIN_CTRL_X1);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03, val);
}


void _mxd_set_nr_bnr_dc(MXD_Level bnr_dc_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_00);
	val = MXD_REG_MOD(val, ENABLE, DC_BNR_ENABLE);
	val = MXD_REG_MOD(val, ENABLE, ADAPTIVE_MODE);
	val = MXD_REG_MOD(val, BNR_DC_OUTPUT_MUX, OUTPUT_MUX);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_00, val);
       _mxd_set_nr_bnr_ac(bnr_dc_level);


	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_01);
	val = MXD_REG_MOD(val, BNR_DC_BLUR_ENABLE, BLUR_ENABLE);

	if (bnr_dc_level == LEVEL_WEEK)
	{
		val = MXD_REG_MOD(val, BNR_DC_MUL2BLK_WEEK, MUL2BLK);
	}
	else
	{
		val = MXD_REG_MOD(val, BNR_DC_MUL2BLK, MUL2BLK);
	}
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_01, val);

	if (bnr_dc_level == LEVEL_STRONG)
	{
		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_Y2, DC_BNR_GAIN_CTRL_Y2);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_X2, DC_BNR_GAIN_CTRL_X2);
		val = MXD_REG_MOD(val,BNR_DC_GAIN_CTRL_Y3, DC_BNR_GAIN_CTRL_Y3);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_X3, DC_BNR_GAIN_CTRL_X3);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02, val);

		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_Y0, DC_BNR_GAIN_CTRL_Y0);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_X0, DC_BNR_GAIN_CTRL_X0);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_Y1, DC_BNR_GAIN_CTRL_Y1);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_X1, DC_BNR_GAIN_CTRL_X1);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03, val);
	}
	else if (bnr_dc_level == LEVEL_WEEK)
	{
        	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02);
        	val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_Y2_WEEK,
                                           DC_BNR_GAIN_CTRL_Y2);
        	val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_X2_WEEK,
                                          DC_BNR_GAIN_CTRL_X2);
        	val = MXD_REG_MOD(val,BNR_DC_GAIN_CTRL_Y3_WEEK,
                                           DC_BNR_GAIN_CTRL_Y3);
        	val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_X3_WEEK,
                                           DC_BNR_GAIN_CTRL_X3);
        	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02, val);

        	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03);
        	val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_Y0_WEEK,
                                            DC_BNR_GAIN_CTRL_Y0);
        	val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_X0_WEEK,
                                           DC_BNR_GAIN_CTRL_X0);
        	val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_Y1_WEEK,
                                          DC_BNR_GAIN_CTRL_Y1);
        	val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_X1_WEEK,
                                     	DC_BNR_GAIN_CTRL_X1);
        	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03, val);
	}
	else
	{
		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_Y2_MEDIUM,
                                             DC_BNR_GAIN_CTRL_Y2);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_X2_MEDIUM,
                                             DC_BNR_GAIN_CTRL_X2);
		val = MXD_REG_MOD(val,BNR_DC_GAIN_CTRL_Y3_MEDIUM,
                                             DC_BNR_GAIN_CTRL_Y3);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_X3_MEDIUM,
                                             DC_BNR_GAIN_CTRL_X3);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02, val);

		val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_Y0_MEDIUM,
                                             DC_BNR_GAIN_CTRL_Y0);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_X0_MEDIUM,
                                             DC_BNR_GAIN_CTRL_X0);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_Y1_MEDIUM,
                                             DC_BNR_GAIN_CTRL_Y1);
		val = MXD_REG_MOD(val, BNR_DC_GAIN_CTRL_X1_MEDIUM,
                                             DC_BNR_GAIN_CTRL_X1);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03, val);
	}

}

void odin_mxd_set_cnr_cb_en_tune(int cnr_cb_en)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL);
	 val = MXD_REG_MOD(val, cnr_cb_en, CNR_CB_EN);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL, val);
}

void odin_mxd_set_cnr_cr_en_tune(int cnr_cr_en)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL);
	 val = MXD_REG_MOD(val, cnr_cr_en, CNR_CR_EN);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL, val);
}

void odin_mxd_set_cnr_low_luminance_en_tune(int low_lum_en)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL);
	 val = MXD_REG_MOD(val, low_lum_en, LOW_LUMINACE_EN);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL, val);
}

void odin_mxd_set_cnr_cb_diff_bottom_tune(int cb_diff_botm)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF);
	 val = MXD_REG_MOD(val, cb_diff_botm, CB_DIFF_BOTTOM);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF, val);
}

void odin_mxd_set_cnr_cb_diff_top_tune(int cb_diff_top)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF);
	 val = MXD_REG_MOD(val, cb_diff_top, CB_DIFF_TOP);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF, val);
}

void odin_mxd_set_cnr_cb_slope_tune(int cb_slope)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL);
	 val = MXD_REG_MOD(val, cb_slope, CB_SLOPE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL, val);
}

void odin_mxd_set_cnr_cb_dist_tune(int cb_dist)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL);
	 val = MXD_REG_MOD(val, cb_dist, CB_DIST);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL, val);
}

void odin_mxd_set_cnr_delta_cb_tune(int delta_cb)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL);
	 val = MXD_REG_MOD(val, delta_cb, DELTA_CB);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL, val);
}

void odin_mxd_set_cnr_cb_low_diff_bottom_tune(int cb_low_diff_botm)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW);
	 val = MXD_REG_MOD(val, cb_low_diff_botm, CB_LOW_DIFF_BOTTOM);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW, val);
}

void odin_mxd_set_cnr_cb_low_diff_top_tune(int cb_low_diff_top)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW);
	 val = MXD_REG_MOD(val, cb_low_diff_top, CB_LOW_DIFF_TOP);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW, val);
}


void odin_mxd_set_cnr_cb_low_slope_tune(int cb_low_slop)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW);
	 val = MXD_REG_MOD(val, cb_low_slop, CB_LOW_SLOPE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW, val);
}

void odin_mxd_set_cnr_cb_low_dist_tune(int cb_low_dist)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW);
	 val = MXD_REG_MOD(val, cb_low_dist, CB_LOW_DIST);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW, val);
}




void odin_mxd_set_cnr_cr_diff_bottom_tune(int cb_diff_botm)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF);
	 val = MXD_REG_MOD(val, cb_diff_botm, CR_DIFF_BOTTOM);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF, val);
}

void odin_mxd_set_cnr_cr_diff_top_tune(int cr_diff_top)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF);
	 val = MXD_REG_MOD(val, cr_diff_top, CR_DIFF_TOP);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF, val);
}

void odin_mxd_set_cnr_cr_slope_tune(int cr_slope)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL);
	 val = MXD_REG_MOD(val, cr_slope, CR_SLOPE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL, val);
}

void odin_mxd_set_cnr_cr_dist_tune(int cr_dist)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL);
	 val = MXD_REG_MOD(val, cr_dist, CR_DIST);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL, val);
}

void odin_mxd_set_cnr_delta_cr_tune(int delta_cr)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL);
	 val = MXD_REG_MOD(val, delta_cr, DELTA_CR);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL, val);
}

void odin_mxd_set_cnr_cr_low_diff_bottom_tune(int cr_low_diff_botm)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW);
	 val = MXD_REG_MOD(val, cr_low_diff_botm, CR_LOW_DIFF_BOTTOM);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW, val);
}

void odin_mxd_set_cnr_cr_low_diff_top_tune(int cr_low_diff_top)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW);
	 val = MXD_REG_MOD(val, cr_low_diff_top, CR_LOW_DIFF_TOP);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW, val);
}


void odin_mxd_set_cnr_cr_low_slope_tune(int cr_low_slope)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW);
	 val = MXD_REG_MOD(val, cr_low_slope, CR_LOW_SLOPE);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW, val);
}

void odin_mxd_set_cnr_cr_low_dist_tune(int cr_low_dist)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW);
	 val = MXD_REG_MOD(val, cr_low_dist, CR_LOW_DIST);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW, val);
}


void odin_mxd_set_cnr_delta_cr_low_tune(int delta_cr_low)
{
        u32 val;
        val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW);
	 val = MXD_REG_MOD(val, delta_cr_low, DELTA_CR_LOW);
	 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW, val);
}



void _mxd_set_cnr(MXD_Level bnr_dc_level)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL);
	val = MXD_REG_MOD(val, ENABLE, CNR_CB_EN);
	val = MXD_REG_MOD(val, ENABLE, CNR_CR_EN);
	val = MXD_REG_MOD(val, ENABLE, LOW_LUMINACE_EN);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL, val);

	if (bnr_dc_level == LEVEL_STRONG)
	{
		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF);
		val = MXD_REG_MOD(val, CNR_CB_DIFF_BOTTOM_STRONG, CB_DIFF_BOTTOM);
		val = MXD_REG_MOD(val, CNR_CB_DIFF_TOP_STRONG, CB_DIFF_TOP);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL);
		val = MXD_REG_MOD(val, CNR_CB_SLOPE_STRONG, CB_SLOPE);
		val = MXD_REG_MOD(val, CNR_CB_DIST, CB_DIST);
		val = MXD_REG_MOD(val, CNR_CB_DELTA_STRONG, DELTA_CB);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW);
		val = MXD_REG_MOD(val, CNR_CB_LOW_DIFF_BOTTOM_STRONG,
                                                  CB_LOW_DIFF_BOTTOM);
		val = MXD_REG_MOD(val, CNR_CB_LOW_DIFF_TOP_STRONG, CB_LOW_DIFF_TOP);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW);
		val = MXD_REG_MOD(val, CNR_CB_LOW_SLOPE_STRONG, CB_LOW_SLOPE);
		val = MXD_REG_MOD(val, CNR_CB_LOW_DIST, CB_LOW_DIST);
		val = MXD_REG_MOD(val, CNR_CB_DELTA_LOW_STRONG, DELTA_CB_LOW);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF);
		val = MXD_REG_MOD(val, CNR_CR_DIFF_BOTTOM_STRONG, CR_DIFF_BOTTOM);
		val = MXD_REG_MOD(val, CNR_CR_DIFF_TOP_STRONG, CR_DIFF_TOP);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL);
		val = MXD_REG_MOD(val, CNR_CR_SLOPE_STRONG, CR_SLOPE);
		val = MXD_REG_MOD(val, CNR_CR_DIST, CR_DIST);
		val = MXD_REG_MOD(val, CNR_CR_DELTA_STRONG, DELTA_CR);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW);
		val = MXD_REG_MOD(val, CNR_CR_LOW_DIFF_BOTTOM_STRONG,
                                                   CR_LOW_DIFF_BOTTOM);
 		val = MXD_REG_MOD(val, CR_LOW_DIFF_TOP_STRONG, CR_LOW_DIFF_TOP);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW);
		val = MXD_REG_MOD(val, CNR_CR_LOW_SLOPE_STRONG, CR_LOW_SLOPE);
		val = MXD_REG_MOD(val, CNR_CR_LOW_DIST, CR_LOW_DIST);
		val = MXD_REG_MOD(val, CNR_CR_DELTA_LOW_STRONG, DELTA_CR_LOW);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW, val);
	}
	else if (bnr_dc_level == LEVEL_MEDIUM)
	{
		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF);
		val = MXD_REG_MOD(val, CNR_CB_DIFF_BOTTOM_MEDIUM, CB_DIFF_BOTTOM);
		val = MXD_REG_MOD(val, CNR_CB_DIFF_TOP_MEDIUM, CB_DIFF_TOP);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL);
		val = MXD_REG_MOD(val, CNR_CB_SLOPE_MEDIUM, CB_SLOPE);
		val = MXD_REG_MOD(val, CNR_CB_DIST, CB_DIST);
		val = MXD_REG_MOD(val, CNR_CB_DELTA_MEDIUM, DELTA_CB);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW);
		val = MXD_REG_MOD(val, CNR_CB_LOW_DIFF_BOTTOM_MEDIUM,
                                                   CB_LOW_DIFF_BOTTOM);
		val = MXD_REG_MOD(val, CNR_CB_LOW_DIFF_TOP_MEDIUM, CB_LOW_DIFF_TOP);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW);
		val = MXD_REG_MOD(val, CNR_CB_LOW_SLOPE_MEDIUM, CB_LOW_SLOPE);
		val = MXD_REG_MOD(val, CNR_CB_LOW_DIST, CB_LOW_DIST);
		val = MXD_REG_MOD(val, CNR_CB_DELTA_LOW_MEDIUM, DELTA_CB_LOW);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF);
		val = MXD_REG_MOD(val, CNR_CR_DIFF_BOTTOM_MEDIUM, CR_DIFF_BOTTOM);
		val = MXD_REG_MOD(val, CNR_CR_DIFF_TOP_MEDIUM, CR_DIFF_TOP);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL);
		val = MXD_REG_MOD(val, CNR_CR_SLOPE_MEDIUM, CR_SLOPE);
		val = MXD_REG_MOD(val, CNR_CR_DIST, CR_DIST);
		val = MXD_REG_MOD(val, CNR_CR_DELTA_MEDIUM, DELTA_CR);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW);
		val = MXD_REG_MOD(val, CNR_CR_LOW_DIFF_BOTTOM_MEDIUM,
                                                  CR_LOW_DIFF_BOTTOM);
 		val = MXD_REG_MOD(val, CR_LOW_DIFF_TOP_MEDIUM, CR_LOW_DIFF_TOP);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW);
		val = MXD_REG_MOD(val, CNR_CR_LOW_SLOPE_MEDIUM, CR_LOW_SLOPE);
		val = MXD_REG_MOD(val, CNR_CR_LOW_DIST, CR_LOW_DIST);
		val = MXD_REG_MOD(val, CNR_CR_DELTA_LOW_MEDIUM, DELTA_CR_LOW);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW, val);
	}

	else if (bnr_dc_level == LEVEL_WEEK)
	{
		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF);
		val = MXD_REG_MOD(val, CNR_CB_DIFF_BOTTOM_WEEK, CB_DIFF_BOTTOM);
		val = MXD_REG_MOD(val, CNR_CB_DIFF_TOP_WEEK, CB_DIFF_TOP);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL);
		val = MXD_REG_MOD(val, CNR_CB_SLOPE_WEEK, CB_SLOPE);
		val = MXD_REG_MOD(val, CNR_CB_DIST, CB_DIST);
		val = MXD_REG_MOD(val, CNR_CB_DELTA_WEEK, DELTA_CB);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW);
		val = MXD_REG_MOD(val, CNR_CB_LOW_DIFF_BOTTOM_WEEK,
                                              CB_LOW_DIFF_BOTTOM);
		val = MXD_REG_MOD(val, CNR_CB_LOW_DIFF_TOP_WEEK, CB_LOW_DIFF_TOP);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW);
		val = MXD_REG_MOD(val, CNR_CB_LOW_SLOPE_WEEK, CB_LOW_SLOPE);
		val = MXD_REG_MOD(val, CNR_CB_LOW_DIST, CB_LOW_DIST);
		val = MXD_REG_MOD(val, CNR_CB_DELTA_LOW_WEEK, DELTA_CB_LOW);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF);
		val = MXD_REG_MOD(val, CNR_CR_DIFF_BOTTOM_WEEK, CR_DIFF_BOTTOM);
		val = MXD_REG_MOD(val, CNR_CR_DIFF_TOP_WEEK, CR_DIFF_TOP);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL);
		val = MXD_REG_MOD(val, CNR_CR_SLOPE_WEEK, CR_SLOPE);
		val = MXD_REG_MOD(val, CNR_CR_DIST, CR_DIST);
		val = MXD_REG_MOD(val, CNR_CR_DELTA_WEEK, DELTA_CR);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW);
		val = MXD_REG_MOD(val, CNR_CR_LOW_DIFF_BOTTOM_WEEK,
                                                 CR_LOW_DIFF_BOTTOM);
 		val = MXD_REG_MOD(val, CR_LOW_DIFF_TOP_WEEK, CR_LOW_DIFF_TOP);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW, val);

		val= mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW);
		val = MXD_REG_MOD(val, CNR_CR_LOW_SLOPE_WEEK, CR_LOW_SLOPE);
		val = MXD_REG_MOD(val, CNR_CR_LOW_DIST, CR_LOW_DIST);
		val = MXD_REG_MOD(val, CNR_CR_DELTA_LOW_WEEK, DELTA_CR_LOW);
		mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW, val);

		}



}

void odin_mxd_set_cnr(MXD_Level bnr_ac_level)
{
	_mxd_set_cnr(bnr_ac_level);

}


void set_max_ctrl(void)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	val = MXD_REG_MOD(val, ENABLE, ENABLE_MNR);
	val = MXD_REG_MOD(val, ENABLE, ENABLE_DER);
	val = MXD_REG_MOD(val, ENABLE, ENABLE_DC_BNR);
	val = MXD_REG_MOD(val, ENABLE, ENABLE_AC_BNR);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);
}
void odin_mxd_set_nr_bnr_dc(MXD_Level bnr_dc_level)
{
	_mxd_set_nr_bnr_dc(bnr_dc_level);

}

int check_format(enum odin_color_mode color_mode)
{
	switch (CHECK_COLOR_VALUE(color_mode, ODIN_COLOR_MODE))
	{
		case ODIN_DSS_COLOR_YUV420P_2:
		case ODIN_DSS_COLOR_YUV420P_3:
			mxd_desc.fmt = MXD_FORMAT_YUV420_MPEG_1_2_CVI;
			break;

		case ODIN_DSS_COLOR_YUV422P_2:
		case ODIN_DSS_COLOR_YUV422P_3:
			mxd_desc.fmt = MXD_FORMAT_YUV422_JPEG_H;
			break;

		case ODIN_DSS_COLOR_YUV224P_2:
		case ODIN_DSS_COLOR_YUV224P_3:
			mxd_desc.fmt = MXD_FORMAT_YUV422_JPEG_V;
			break;

		case ODIN_DSS_COLOR_YUV444P_3:
			mxd_desc.fmt = MXD_FORMAT_YUV444;
			break;

		default:
			return -1;
	}

	if (CHECK_COLOR_VALUE(color_mode, ODIN_COLOR_YUV_SWAP))
		mxd_desc.uvswap = true;
	else
		mxd_desc.uvswap = false;

	return 0;
}

int odin_mxd_adaptedness(enum odin_resource_index resource_index,
                                 enum odin_color_mode color_mode,
		int width, int height, bool scl_en, int scenario)
{
	int ret = 0;

	odin_dma_default_set();
	if (resource_index == ODIN_DSS_VID0_RES
               || resource_index == ODIN_DSS_VID1_RES)
	{
		if (check_format(color_mode))
		{
			ret = -1;
			goto exit;
		}
/*
		if (width > ODIN_MXD_MAX_WIDTH || height > ODIN_MXD_MAX_HEIGHT)
		{
			mxd_desc.scenario = MXD_SCENARIO_CASE2;
			ret = 1;
		}
		*/
		else
		{
			switch (scenario)
			{
			case MXD_SCENARIO_CASE1:
				mxd_desc.scenario = MXD_SCENARIO_CASE1;
				break;
			case MXD_SCENARIO_CASE2:
				mxd_desc.scenario = MXD_SCENARIO_CASE2;
				break;
			case MXD_SCENARIO_CASE3:
				mxd_desc.scenario = MXD_SCENARIO_CASE3;
				break;
			case MXD_SCENARIO_CASE4:
				mxd_desc.scenario = MXD_SCENARIO_CASE4;
				break;
			}

		}

		mxd_desc.width = width;
		mxd_desc.height = height;

	}
	else
	{
		mxd_desc.scenario = MXD_SCENARIO_CASE2;
		ret = -1;
	}
exit:
	return ret;
}

int odin_mxd_init(void)
{

	_odin_mxd_set_color_format(mxd_desc.fmt, mxd_desc.uvswap);
	odin_mxd_set_image_size(mxd_desc.width, mxd_desc.height,
				mxd_desc.width, mxd_desc.height);

	return 0;
}

int odin_mxd_enable(bool mxd_en)
{
	odin_dma_default_set();
	if (mxd_en)
	{
		_odin_mxd_set_path(mxd_desc.scenario);
		_odin_mxd_set_color_format(mxd_desc.fmt, mxd_desc.uvswap);
	}
	else
	{
		_odin_mxd_set_path(MXD_SCENARIO_CASE3);
	}
	return 0;
}

void _odin_mxd_tp_sync_gen(mxdtpsyncselect tp_sync)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00);
	switch (tp_sync)
	{
		case MXD_TP_SYNC_DATA_INPUT:
			val = MXD_REG_MOD(val, 0, TP_SYNC_GENERATION_ENABLE);
			break;
		case MXD_TP_SYNC_DATA_ITSELF:
			val = MXD_REG_MOD(val, 1, TP_SYNC_GENERATION_ENABLE);
			break;
	}
	mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00, val);
}

void _odin_mxd_tp_slct(mxdtestpatternsel sel_tp)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00);
	switch (sel_tp)
	{

		case MXD_TEST_PATTERN_DATA_H_GRADIENT_COLOR_BAR:
			val = MXD_REG_MOD(val, 0, TEST_PATTERN_SELECT);
			break;
		case MXD_TEST_PATTERN_DATA_H_STATIC_COLOR_BAR:
			val = MXD_REG_MOD(val, 1, TEST_PATTERN_SELECT);
			break;
		case MXD_TEST_PATTERN_DATA_V_GRADIENT_COLOR_BAR:
			val = MXD_REG_MOD(val, 2, TEST_PATTERN_SELECT);
			break;
		case MXD_TEST_PATTERN_DATA_V_STATIC_COLOR_BAR:
			val = MXD_REG_MOD(val, 3, TEST_PATTERN_SELECT);
			break;
		case MXD_TEST_PATTERN_DATA_SOLID_DATA:
			val = MXD_REG_MOD(val, 4, TEST_PATTERN_SELECT);
			break;
		case MXD_TEST_PATTERN_DATA_MAX:
		default:
			break;
	}
	mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00, val);
}

void _odin_mxd_set_tp(bool tp_en)
{

	u32 val;
	val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00);

	if (tp_en)
	{
		val = MXD_REG_MOD(val, tp_en, TEST_PATTERN_ENABLE);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00, val);

		_odin_mxd_tp_sync_gen(0);
             printk("test pattern =  %d\n", tp_en);

		_odin_mxd_tp_slct(MXD_TEST_PATTERN_DATA_V_STATIC_COLOR_BAR);

		val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00);
		val = MXD_REG_MOD(val, TP_SOLID_Y_8BIT, TEST_PATTERN_SOLID_Y_DATA);
		val = MXD_REG_MOD(val, TP_SOLID_CB_8BIT, TEST_PATTERN_SOLID_CB_DATA);
		val = MXD_REG_MOD(val, TP_SOLID_CR_8BIT, TEST_PATTERN_SOLID_CR_DATA);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00, val);

		val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_01);
		val = MXD_REG_MOD(val, mxd_desc.width, TEST_PATTERN_IMAGE_WIDTH_SIZE);
		val = MXD_REG_MOD(val, mxd_desc.height, TEST_PATTERN_IMAGE_HEIGHT_SIZE);
            printk("mxd_desc.width =  %d\n", mxd_desc.width);
            printk("mxd_desc.height =  %d\n", mxd_desc.height);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_01, val);

		val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_02);
		val = MXD_REG_MOD(val, TP_Y_DATA_MASK, TEST_PATTERN_Y_DATA_MASK);
		val = MXD_REG_MOD(val, TP_CB_DATA_MASK, TEST_PATTERN_CB_DATA_MASK);
		val = MXD_REG_MOD(val, TP_CR_DATA_MASK, TEST_PATTERN_CR_DATA_MASK);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_02, val);

	}

	else
	{
		val = MXD_REG_MOD(val, tp_en, TEST_PATTERN_ENABLE);
		mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00, val);

	}
}

void odin_mxd_set_tp(bool tp_en)
{

	_odin_mxd_set_tp(tp_en);
}

int odin_mxd_get_tuning(int resource_index, char* regist, char* field)
{
    u32 rg_val = 0;
    u32 get_value = 0;

    switch (resource_index)
    {
       case ODIN_MXD_TOP_RES:

           if (strcmp("ADDR_XD_TOP_CTRL_00", regist) == 0)
           {
               rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00);
               if (strcmp("NR_INPUT_SEL", field) == 0)
                   get_value = MXD_REG_GET(rg_val, NR_INPUT_SEL);
               else if (strcmp("RE_INPUT_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, RE_INPUT_SEL);
                else if (strcmp("SCL_INPUT_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SCL_INPUT_SEL);
                else if (strcmp("NR_INT_MASK", field) == 0)
                    get_value = MXD_REG_GET(rg_val, NR_INT_MASK);
                else if (strcmp("RE_INT_MASK", field) == 0)
                    get_value = MXD_REG_GET(rg_val, RE_INT_MASK);
                else if (strcmp("CE_INT_MASK", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CE_INT_MASK);
                else if (strcmp("RE_INPUT_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, RE_INPUT_SEL);
                else if (strcmp("REG_UPDATE_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, REG_UPDATE_MODE);
            }
            else if (strcmp("ADDR_XD_TOP_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_01);
                if (strcmp("MEMORY_ALWAYS_ON", field) == 0)
                    get_value = MXD_REG_GET(rg_val, MEMORY_ALWAYS_ON);
            }
             else if (strcmp("ADDR_XD_TOP_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_02);
                if (strcmp("COLOR_FORMAT", field) == 0)
                    get_value = MXD_REG_GET(rg_val, COLOR_FORMAT);
                else if (strcmp("CBCR_ORDER", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CBCR_ORDER);
            }

             else if (strcmp("ADDR_XD_IMAGE_SIZE0", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE0);
                if (strcmp("NR_PIC_WIDTH", field) == 0)
                    {
                    get_value = MXD_REG_GET(rg_val, NR_PIC_WIDTH);
                    printk("nr size for getting value = %d", get_value);
                    }
                else if (strcmp("NR_PIC_HEIGHT", field) == 0)
                    get_value = MXD_REG_GET(rg_val, NR_PIC_HEIGHT);
            }
             else if (strcmp("ADDR_XD_IMAGE_SIZE1", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE1);
                if (strcmp("RE_PIC_WIDTH", field) == 0)
                    {
                    get_value = MXD_REG_GET(rg_val, RE_PIC_WIDTH);
                    printk("re width size for getting value = %x", get_value);
                    }
                else if (strcmp("RE_PIC_HEIGHT", field) == 0)
                    get_value = MXD_REG_GET(rg_val, RE_PIC_HEIGHT);
            }
              else if (strcmp("ADDR_XD_DMA_REG_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
                if (strcmp("HIST_LUT_DMA_START", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HIST_LUT_DMA_START);
                else if (strcmp("HIST_LUT_OPERATION_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HIST_LUT_OPERATION_MODE);
                else if (strcmp("DCE_DSE_GAIN_LUT_DMA_START", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DCE_DSE_GAIN_LUT_DMA_START);
                else if (strcmp("DCE_DSE_GAIN_LUT_OPERATION_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val,
                    DCE_DSE_GAIN_LUT_OPERATION_MODE);
            }
              else if (strcmp("ADDR_XD_HIST_LUT_BADDR", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_HIST_LUT_BADDR);
                if (strcmp("HIST_LUT_DRAM_BASE_ADDRESS", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HIST_LUT_DRAM_BASE_ADDRESS);
            }
              else if (strcmp("ADDR_XD_GAIN_LUT_BADDR", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_GAIN_LUT_BADDR);
                if (strcmp("DCE_DSE_GAIN_LUT_DRAM_BASE_ADDRESS", field) == 0)
                    get_value = MXD_REG_GET(rg_val,
                    DCE_DSE_GAIN_LUT_DRAM_BASE_ADDRESS);
            }

              else  if (strcmp("ADDR_XD_TP_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00);
                if (strcmp("TEST_PATTERN_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, TEST_PATTERN_ENABLE);
                else if (strcmp("TP_SYNC_GENERATION_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, TP_SYNC_GENERATION_ENABLE);
                else if (strcmp("TEST_PATTERN_SELECT", field) == 0)
                    get_value = MXD_REG_GET(rg_val, TEST_PATTERN_SELECT);
                else if (strcmp("TEST_PATTERN_SOLID_Y_DATA", field) == 0)
                    get_value = MXD_REG_GET(rg_val, TEST_PATTERN_SOLID_Y_DATA);
                else if (strcmp("TEST_PATTERN_SOLID_CB_DATA", field) == 0)
                    get_value = MXD_REG_GET(rg_val, TEST_PATTERN_SOLID_CB_DATA);
                else if (strcmp("TEST_PATTERN_SOLID_CR_DATA", field) == 0)
                    get_value = MXD_REG_GET(rg_val, TEST_PATTERN_SOLID_CR_DATA);
            }

               else if (strcmp("ADDR_XD_TP_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_01);
                if (strcmp("TEST_PATTERN_IMAGE_WIDTH_SIZE", field) == 0)
                    get_value = MXD_REG_GET(rg_val,
                        TEST_PATTERN_IMAGE_WIDTH_SIZE);
                else if (strcmp("TEST_PATTERN_IMAGE_HEIGHT_SIZE", field) == 0)
                    get_value = MXD_REG_GET(rg_val,
                    TEST_PATTERN_IMAGE_HEIGHT_SIZE);
            }

              else if (strcmp("ADDR_XD_TP_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_02);
                if (strcmp("TEST_PATTERN_Y_DATA_MASK", field) == 0)
                    get_value = MXD_REG_GET(rg_val, TEST_PATTERN_Y_DATA_MASK);
                else if (strcmp("TEST_PATTERN_CB_DATA_MASK", field) == 0)
                    get_value = MXD_REG_GET(rg_val, TEST_PATTERN_CB_DATA_MASK);
                else if (strcmp("TEST_PATTERN_CR_DATA_MASK", field) == 0)
                    get_value = MXD_REG_GET(rg_val,
                    TEST_PATTERN_CR_DATA_MASK);
            }

           break;


       case ODIN_MXD_NR_RES:

         if (strcmp("ADDR_WIN_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_00);
                if (strcmp("WIN0_X0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WIN0_X0);
                else if (strcmp("WIN0_Y0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WIN0_Y0);
            }
            else  if (strcmp("ADDR_WIN_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_01);
                if (strcmp("WIN1_X1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WIN1_X1);
                else if (strcmp("WIN1_Y1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WIN1_Y1);
            }
             else  if (strcmp("ADDR_WIN_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_02);
                if (strcmp("WIN2_X0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WIN2_X0);
                else if (strcmp("WIN2_Y0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WIN2_Y0);
                else if (strcmp("AC_BNR_FEATURE_CAL_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, AC_BNR_FEATURE_CAL_MODE);
            }
              else  if (strcmp("ADDR_WIN_CTRL_03", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_03);
                if (strcmp("WIN3_X1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WIN3_X1);
                else if (strcmp("WIN3_Y1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WIN3_Y1);
            }
              else  if (strcmp("ADDR_VFLT_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_VFLT_CTRL_00);
            }
              else  if (strcmp("ADDR_MNR_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_00);
                if (strcmp("MNR_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, MNR_ENABLE);
                else if (strcmp("EDGE_GAIN_MAPPING_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, EDGE_GAIN_MAPPING_ENABLE);
            }
             else  if (strcmp("ADDR_MNR_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01);
                if (strcmp("HCOEF_00", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_00);
                else if (strcmp("HCOEF_01", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_01);
                else if (strcmp("HCOEF_02", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_02);
                else if (strcmp("HCOEF_03", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_03);
            }
             else  if (strcmp("ADDR_MNR_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02);
                if (strcmp("HCOEF_04", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_04);
                else if (strcmp("HCOEF_05", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_05);
                else if (strcmp("HCOEF_06", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_06);
                else if (strcmp("HCOEF_07", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_07);
                else if (strcmp("HCOEF_08", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_08);
                else if (strcmp("HCOEF_09", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_09);
                else if (strcmp("HCOEF_10", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_10);
                else if (strcmp("HCOEF_11", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_11);
            }

             else  if (strcmp("ADDR_MNR_CTRL_03", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03);
                if (strcmp("HCOEF_12", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_12);
                else if (strcmp("HCOEF_13", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_13);
                else if (strcmp("HCOEF_14", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_14);
                else if (strcmp("HCOEF_15", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_15);
                else if (strcmp("HCOEF_16", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HCOEF_16);
                else if (strcmp("X1_POSITION", field) == 0)
                    get_value = MXD_REG_GET(rg_val, X1_POSITION);
            }

              else  if (strcmp("ADDR_MNR_CTRL_04", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04);
                if (strcmp("X2_POSITION", field) == 0)
                    get_value = MXD_REG_GET(rg_val, X2_POSITION);
                else if (strcmp("X3_POSITION", field) == 0)
                    get_value = MXD_REG_GET(rg_val, X3_POSITION);
                else if (strcmp("X4_POSITION", field) == 0)
                    get_value = MXD_REG_GET(rg_val, X4_POSITION);
                else if (strcmp("Y1_POSITION", field) == 0)
                    get_value = MXD_REG_GET(rg_val, Y1_POSITION);
            }
              else  if (strcmp("ADDR_MNR_CTRL_05", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05);
                if (strcmp("Y2_POSITION", field) == 0)
                    get_value = MXD_REG_GET(rg_val, Y2_POSITION);
                else if (strcmp("Y3_POSITION", field) == 0)
                    get_value = MXD_REG_GET(rg_val, Y3_POSITION);
                else if (strcmp("Y4_POSITION", field) == 0)
                    get_value = MXD_REG_GET(rg_val, Y4_POSITION);
                else if (strcmp("FILTER_THRESHOLD", field) == 0)
                    get_value = MXD_REG_GET(rg_val, FILTER_THRESHOLD);
            }
              else  if (strcmp("ADDR_MNR_CTRL_06", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06);
                if (strcmp("VCOEF0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, VCOEF0);
                else if (strcmp("VCOEF1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, VCOEF1);
                else if (strcmp("VCOEF2", field) == 0)
                    get_value = MXD_REG_GET(rg_val, VCOEF2);
                else if (strcmp("VCOEF3", field) == 0)
                    get_value = MXD_REG_GET(rg_val, VCOEF3);
                else if (strcmp("VCOEF4", field) == 0)
                    get_value = MXD_REG_GET(rg_val, VCOEF4);
                else if (strcmp("VCOEF5", field) == 0)
                    get_value = MXD_REG_GET(rg_val, VCOEF5);
                else if (strcmp("VCOEF6", field) == 0)
                    get_value = MXD_REG_GET(rg_val, VCOEF6);
            }
              else  if (strcmp("ADDR_BNR_DC_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_00);
                if (strcmp("DC_BNR_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DC_BNR_ENABLE);
                else if (strcmp("ADAPTIVE_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, ADAPTIVE_MODE);
                else if (strcmp("OUTPUT_MUX", field) == 0)
                    get_value = MXD_REG_GET(rg_val, OUTPUT_MUX);
                else if (strcmp("DC_OFFSET", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DC_OFFSET);
                else if (strcmp("DC_GAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DC_GAIN);
            }
             else  if (strcmp("ADDR_BNR_DC_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_01);
                if (strcmp("BLUR_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, BLUR_ENABLE);
                else if (strcmp("MUL2BLK", field) == 0)
                    get_value = MXD_REG_GET(rg_val, MUL2BLK);
            }
              else  if (strcmp("ADDR_BNR_DC_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02);
                if (strcmp("DC_BNR_GAIN_CTRL_Y2", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DC_BNR_GAIN_CTRL_Y2);
                else if (strcmp("DC_BNR_GAIN_CTRL_X2", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DC_BNR_GAIN_CTRL_X2);
                else if (strcmp("DC_BNR_GAIN_CTRL_Y3", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DC_BNR_GAIN_CTRL_Y3);
                else if (strcmp("DC_BNR_GAIN_CTRL_X3", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DC_BNR_GAIN_CTRL_X3);
            }
              else  if (strcmp("ADDR_BNR_DC_CTRL_03", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03);
                if (strcmp("DC_BNR_GAIN_CTRL_Y0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DC_BNR_GAIN_CTRL_Y0);
                else if (strcmp("DC_BNR_GAIN_CTRL_X0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DC_BNR_GAIN_CTRL_X0);
                else if (strcmp("DC_BNR_GAIN_CTRL_Y1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DC_BNR_GAIN_CTRL_Y1);
                else if (strcmp("DC_BNR_GAIN_CTRL_X1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DC_BNR_GAIN_CTRL_X1);
            }
             else  if (strcmp("ADDR_BNR_AC_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
                if (strcmp("BNR_H_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, BNR_H_EN);
                else if (strcmp("BNR_V_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, BNR_V_EN);
                else if (strcmp("STATUS_READ_TYPE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, STATUS_READ_TYPE);
                else if (strcmp("STATUS_READ_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, STATUS_READ_MODE);
                else if (strcmp("HBMAX_GAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HBMAX_GAIN);
                else if (strcmp("VBMAX_GAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, VBMAX_GAIN);
                else if (strcmp("STRENGTH_RESOLUTION", field) == 0)
                    get_value = MXD_REG_GET(rg_val, STRENGTH_RESOLUTION);
                else if (strcmp("FITER_TYPE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, FITER_TYPE);
                else if (strcmp("BNR_OUTPUT_MUX", field) == 0)
                    get_value = MXD_REG_GET(rg_val, BNR_OUTPUT_MUX);
            }
             else  if (strcmp("ADDR_BNR_AC_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01);
                if (strcmp("STRENGTH_H_X0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, STRENGTH_H_X0);
                else if (strcmp("STRENGTH_H_X1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, STRENGTH_H_X1);
                else if (strcmp("STRENGTH_H_MAX", field) == 0)
                    get_value = MXD_REG_GET(rg_val, STRENGTH_H_MAX);
                else if (strcmp("AC_MIN_TH", field) == 0)
                    get_value = MXD_REG_GET(rg_val, AC_MIN_TH);
            }
             else  if (strcmp("ADDR_BNR_AC_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02);
                if (strcmp("STRENGTH_V_X0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, STRENGTH_V_X0);
                else if (strcmp("STRENGTH_V_X1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, STRENGTH_V_X1);
                else if (strcmp("STRENGTH_V_MAX", field) == 0)
                    get_value = MXD_REG_GET(rg_val, STRENGTH_V_MAX);
            }
             else  if (strcmp("ADDR_BNR_AC_CTRL_03", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03);
                if (strcmp("H_OFFSET_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, H_OFFSET_MODE);
                else if (strcmp("MANUAL_OFFSET_H_VALUE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, MANUAL_OFFSET_H_VALUE);
                else if (strcmp("V_OFFSET_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, V_OFFSET_MODE);
                else if (strcmp("MANUAL_OFFSET_V_VALUE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, MANUAL_OFFSET_V_VALUE);
                else if (strcmp("OFFSET_USE_OF_HYSTERISIS", field) == 0)
                    get_value = MXD_REG_GET(rg_val, OFFSET_USE_OF_HYSTERISIS);
                else if (strcmp("T_FILTER_WEIGHT", field) == 0)
                    get_value = MXD_REG_GET(rg_val, T_FILTER_WEIGHT);
            }
              else  if (strcmp("ADDR_BNR_AC_CTRL_04", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04);
                if (strcmp("MAX_DELTA_TH0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, MAX_DELTA_TH0);
                else if (strcmp("MAX_DELTA_TH1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, MAX_DELTA_TH1);
                else if (strcmp("H_BLOCKNESS_TH", field) == 0)
                    get_value = MXD_REG_GET(rg_val, H_BLOCKNESS_TH);
                else if (strcmp("H_WEIGHT_MAX", field) == 0)
                    get_value = MXD_REG_GET(rg_val, H_WEIGHT_MAX);
            }
              else  if (strcmp("ADDR_BNR_AC_CTRL_05", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_05);
                get_value = MXD_REG_GET(rg_val, SCALED_USE_OF_HYSTERISIS);
            }
              else  if (strcmp("ADDR_BNR_AC_CTRL_06", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_06);
                if (strcmp("AC_BNR_PROTECT_LVL_2", field) == 0)
                    get_value = MXD_REG_GET(rg_val, AC_BNR_PROTECT_LVL_2);
                else if (strcmp("AC_BNR_PROTECT_LVL_1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, AC_BNR_PROTECT_LVL_1);
            }

              else  if (strcmp("ADDR_DNR_MAX_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
                if (strcmp("ENABLE_MNR", field) == 0)
                    get_value = MXD_REG_GET(rg_val, ENABLE_MNR);
                else if (strcmp("ENABLE_DER", field) == 0)
                    get_value = MXD_REG_GET(rg_val, ENABLE_DER);
                else if (strcmp("ENABLE_DC_BNR", field) == 0)
                    get_value = MXD_REG_GET(rg_val, ENABLE_DC_BNR);
                else if (strcmp("ENABLE_AC_BNR", field) == 0)
                    get_value = MXD_REG_GET(rg_val, ENABLE_AC_BNR);
                else if (strcmp("DEBUG_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DEBUG_ENABLE);
                else if (strcmp("DEBUG_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DEBUG_MODE);
                else if (strcmp("DEBUG_MNR_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DEBUG_MNR_ENABLE);
                else if (strcmp("DEBUG_DER_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DEBUG_DER_ENABLE);
                else if (strcmp("DEBUG_DC_BNR_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DEBUG_DC_BNR_ENABLE);
                else if (strcmp("DEBUG_AC_BNR_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DEBUG_AC_BNR_ENABLE);
                else if (strcmp("WIN_CONTROL_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WIN_CONTROL_ENABLE);
                else if (strcmp("BORDER_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, BORDER_ENABLE);
                else if (strcmp("WIN_INOUT", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WIN_INOUT);
            }
             else  if (strcmp("ADDR_DER_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00);
                if (strcmp("DER_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DER_EN);
                else if (strcmp("DER_GAIN_MAPPING", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DER_GAIN_MAPPING);
                else if (strcmp("DER_BIF_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DER_BIF_EN);
                else if (strcmp("BIF_TH", field) == 0)
                    get_value = MXD_REG_GET(rg_val, BIF_TH);
            }
              else  if (strcmp("ADDR_DER_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_01);
                if (strcmp("GAIN_TH0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, GAIN_TH0);
                else if (strcmp("GAIN_TH1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, GAIN_TH1);
            }

              /* CNR */
              else  if (strcmp("ADDR_CNR_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL);
                if (strcmp("CNR_CB_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CNR_CB_EN);
                else if (strcmp("CNR_CR_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CNR_CR_EN);
                else if (strcmp("LOW_LUMINACE_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LOW_LUMINACE_EN);
            }
              else  if (strcmp("ADDR_CNR_CB_DIFF", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF);
                if (strcmp("CB_DIFF_BOTTOM", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CB_DIFF_BOTTOM);
                else if (strcmp("CB_DIFF_TOP", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CB_DIFF_TOP);
            }
             else  if (strcmp("ADDR_CNR_CB_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL);
                if (strcmp("CB_SLOPE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CB_SLOPE);
                else if (strcmp("CB_DIST", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CB_DIST);
                else if (strcmp("DELTA_CB", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DELTA_CB);
            }
             else  if (strcmp("ADDR_CNR_CB_DIFF_LOW", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW);
                if (strcmp("CB_LOW_DIFF_BOTTOM", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CB_LOW_DIFF_BOTTOM);
                else if (strcmp("CB_LOW_DIFF_TOP", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CB_LOW_DIFF_TOP);
            }
             else  if (strcmp("ADDR_CNR_CB_CTRL_LOW", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW);
                if (strcmp("CB_LOW_SLOPE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CB_LOW_SLOPE);
                else if (strcmp("CB_LOW_DIST", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CB_LOW_DIST);
                else if (strcmp("DELTA_CB_LOW", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DELTA_CB_LOW);
            }
              else  if (strcmp("ADDR_CNR_CR_DIFF", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF);
                if (strcmp("CR_DIFF_BOTTOM", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CR_DIFF_BOTTOM);
                else if (strcmp("CR_DIFF_TOP", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CR_DIFF_TOP);
            }
              else  if (strcmp("ADDR_CNR_CR_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL);
                if (strcmp("CR_SLOPE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CR_SLOPE);
                else if (strcmp("CR_DIST", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CR_DIST);
                else if (strcmp("DELTA_CR", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DELTA_CR);
            }
              else  if (strcmp("ADDR_CNR_CR_DIFF_LOW", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW);
                if (strcmp("CR_LOW_DIFF_BOTTOM", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CR_LOW_DIFF_BOTTOM);
                else if (strcmp("CR_LOW_DIFF_TOP", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CR_LOW_DIFF_TOP);
            }
              else  if (strcmp("ADDR_CNR_CR_CTRL_LOW", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW);
                if (strcmp("CR_LOW_SLOPE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CR_LOW_SLOPE);
                else if (strcmp("CR_LOW_DIST", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CR_LOW_DIST);
                else if (strcmp("DELTA_CR_LOW", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DELTA_CR_LOW);
            }

           break;

       case ODIN_MXD_RE_RES:
         if (strcmp("ADDR_RE_DP_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_DP_CTRL);
                if (strcmp("SP_GAIN_E_B", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SP_GAIN_E_B);
                else if (strcmp("SP_GAIN_E_W", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SP_GAIN_E_W);
                else if (strcmp("SP_GAIN_T_B", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SP_GAIN_T_B);
                else if (strcmp("SP_GAIN_T_W", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SP_GAIN_T_W);
            }
            else  if (strcmp("ADDR_RE_CORING_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_CORING_CTRL);
                if (strcmp("CORING_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CORING_MODE);
                else if (strcmp("REG_CORING_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, REG_CORING_EN);
                else if (strcmp("WEIGHT_A", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WEIGHT_A);
                else if (strcmp("WEIGHT_T", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WEIGHT_T);
            }
             else  if (strcmp("ADDR_RE_SP_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_00);
                if (strcmp("EDGE_ENHANCE_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, EDGE_ENHANCE_ENABLE);
                else if (strcmp("EDGE_OPERATOR_SELECTION", field) == 0)
                    get_value = MXD_REG_GET(rg_val, EDGE_OPERATOR_SELECTION);
            }
             else  if (strcmp("ADDR_RE_SP_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_01);
                if (strcmp("WHITE_GAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WHITE_GAIN);
                else if (strcmp("BLACK_GAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, BLACK_GAIN);
                else if (strcmp("HORIZONTAL_GAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, HORIZONTAL_GAIN);
                else if (strcmp("VERTICAL_GAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, VERTICAL_GAIN);
            }
              else  if (strcmp("ADDR_RE_SP_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_02);
                if (strcmp("SOBEL_WEIGHT", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SOBEL_WEIGHT);
                else if (strcmp("LAPLACIAN_WEIGHT", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LAPLACIAN_WEIGHT);
                else if (strcmp("SOBEL_MANUAL_MODE_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SOBEL_MANUAL_MODE_EN);
                else if (strcmp("SOBEL_MANUAL_GAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SOBEL_MANUAL_GAIN);
            }
                else  if (strcmp("ADDR_RE_SP_CTRL_04", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_04);
                if (strcmp("DISPLAY_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DISPLAY_MODE);
                else if (strcmp("GX_WEIGHT_MANUAL_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, GX_WEIGHT_MANUAL_EN);
                else if (strcmp("GX_WEIGHT_MANUAL_GAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, GX_WEIGHT_MANUAL_GAIN);
            }
               else  if (strcmp("ADDR_RE_SP_CTRL_06", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_06);
                if (strcmp("LAP_H_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LAP_H_MODE);
                else if (strcmp("LAP_V_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LAP_V_MODE);
            }
               else  if (strcmp("ADDR_RE_SP_CTRL_07", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_07);
                if (strcmp("GB_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, GB_EN);
                else if (strcmp("GB_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, GB_MODE);
                else if (strcmp("GB_X1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, GB_X1);
                 else if (strcmp("GB_Y1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, GB_Y1);
            }
               else  if (strcmp("ADDR_RE_SP_CTRL_08", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_08);
                if (strcmp("GB_X2", field) == 0)
                    get_value = MXD_REG_GET(rg_val, GB_X2);
                else if (strcmp("GB_Y2", field) == 0)
                    get_value = MXD_REG_GET(rg_val, GB_Y2);
                else if (strcmp("GB_Y3", field) == 0)
                    get_value = MXD_REG_GET(rg_val, GB_Y3);
            }
              else  if (strcmp("ADDR_RE_SP_CTRL_09", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_09);
                if (strcmp("LUM1_X_L0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM1_X_L0);
                else if (strcmp("LUM1_X_L1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM1_X_L1);
                else if (strcmp("LUM1_X_H0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM1_X_H0);
                 else if (strcmp("LUM1_X_H1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM1_X_H1);
            }
              else  if (strcmp("ADDR_RE_SP_CTRL_0A", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0A);
                if (strcmp("LUM1_Y0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM1_Y0);
                else if (strcmp("LUM1_Y1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM1_Y1);
                else if (strcmp("LUM1_Y2", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM1_Y2);
                 else if (strcmp("LUM2_X_L0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM2_X_L0);
            }
              else  if (strcmp("ADDR_RE_SP_CTRL_0B", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0B);
                if (strcmp("LUM2_X_L1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM2_X_L1);
                else if (strcmp("LUM2_X_H0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM2_X_H0);
                else if (strcmp("LUM2_X_H1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM2_X_H1);
                 else if (strcmp("LUM2_Y0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM2_Y0);
            }
              else  if (strcmp("ADDR_RE_SP_CTRL_0C", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0C);
                if (strcmp("LUM2_Y1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM2_Y1);
                else if (strcmp("LUM2_Y2", field) == 0)
                    get_value = MXD_REG_GET(rg_val, LUM2_Y2);
            }
           break;

       case ODIN_MXD_CE_RES:
         if (strcmp("ADDR_CE_APL_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_APL_CTRL_00);
                if (strcmp("APL_WIN_CTRL_X0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, APL_WIN_CTRL_X0);
                else if (strcmp("APL_WIN_CTRL_Y0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, APL_WIN_CTRL_Y0);
            }
         else if (strcmp("ADDR_CE_APL_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_APL_CTRL_01);
                if (strcmp("APL_WIN_CTRL_X1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, APL_WIN_CTRL_X1);
                else if (strcmp("APL_WIN_CTRL_Y1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, APL_WIN_CTRL_Y1);
            }
         else if (strcmp("ADDR_CE_FILM_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_FILM_CTRL_00);
                if (strcmp("VIGNETTING_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, VIGNETTING_ENABLE);
                else if (strcmp("FILM_CONTRAST_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, FILM_CONTRAST_ENABLE);
                else if (strcmp("CONTRAST_ALPHA", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CONTRAST_ALPHA);
                else if (strcmp("CONTRAST_DELTA_MAX", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CONTRAST_DELTA_MAX);
                else if (strcmp("VIGNETTING_GAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, VIGNETTING_GAIN);
            }
         else if (strcmp("ADDR_CE_FILM_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_FILM_CTRL_02);
                if (strcmp("TONE_GAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, TONE_GAIN);
                else if (strcmp("TONE_OFFSET", field) == 0)
                    get_value = MXD_REG_GET(rg_val, TONE_OFFSET);
            }
          else if (strcmp("ADDR_CE_VSPRGB_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_VSPRGB_CTRL_00);
                if (strcmp("CONTRAST_CTRL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CONTRAST_CTRL);
                else if (strcmp("CENTER_POSITION", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CENTER_POSITION);
                else if (strcmp("BRIGHTNESS", field) == 0)
                    get_value = MXD_REG_GET(rg_val, BRIGHTNESS);
                else if (strcmp("CONTRAST_ENABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CONTRAST_ENABLE);
            }
          else if (strcmp("ADDR_CE_CEN_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_00);
                if (strcmp("SELECT_HSV", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SELECT_HSV);
                else if (strcmp("SELECT_RGB", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SELECT_RGB);
                else if (strcmp("VSP_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, VSP_SEL);
                else if (strcmp("GLOBAL_APL_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, GLOBAL_APL_SEL);
                else if (strcmp("REG_CEN_BYPASS", field) == 0)
                    get_value = MXD_REG_GET(rg_val, REG_CEN_BYPASS);
                else if (strcmp("REG_CEN_DEBUG_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, REG_CEN_DEBUG_MODE);
                else if (strcmp("1ST_CORE_GAIN_DISABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, 1ST_CORE_GAIN_DISABLE);
                else if (strcmp("2ND_CORE_GAIN_DISABLE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, 2ND_CORE_GAIN_DISABLE);
                else if (strcmp("V_SCALER_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, V_SCALER_EN);
                else if (strcmp("DEBUG_MODE_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DEBUG_MODE_SEL);
                else if (strcmp("DEMO_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DEMO_MODE);
            }
          else if (strcmp("ADDR_CE_CEN_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_01);
                if (strcmp("SHOW_COLOR_REGION0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SHOW_COLOR_REGION0);
                else if (strcmp("SHOW_COLOR_REGION1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SHOW_COLOR_REGION1);
                else if (strcmp("COLOR_REGION_EN0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, COLOR_REGION_EN0);
                else if (strcmp("COLOR_REGION_EN1", field) == 0)
                    get_value = MXD_REG_GET(rg_val, COLOR_REGION_EN1);
            }
          else if (strcmp("ADDR_CE_CEN_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_02);
                if (strcmp("IHSV_SGAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, IHSV_SGAIN);
                else if (strcmp("IHSV_VGAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, IHSV_VGAIN);
            }
          else if (strcmp("ADDR_CE_CEN_CTRL_03", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_03);
                if (strcmp("IHSV_HOFFSET", field) == 0)
                    get_value = MXD_REG_GET(rg_val, IHSV_HOFFSET);
                else if (strcmp("IHSV_SOFFSET", field) == 0)
                    get_value = MXD_REG_GET(rg_val, IHSV_SOFFSET);
                else if (strcmp("IHSV_VOFFSET", field) == 0)
                    get_value = MXD_REG_GET(rg_val, IHSV_VOFFSET);
            }
           else if (strcmp("ADDR_CE_CEN_CTRL_04", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_04);
                if (strcmp("DEN_CTRL0", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DEN_CTRL0);
                else if (strcmp("DEN_APL_LIMIT_HIGH", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DEN_APL_LIMIT_HIGH);
                else if (strcmp("DEN_GAIN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DEN_GAIN);
                else if (strcmp("DEN_CORING", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DEN_CORING);
            }
             else if (strcmp("ADDR_CE_CEN_IA_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
                if (strcmp("CEN_LUT_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CEN_LUT_EN);
                else if (strcmp("CEN_LUT_CTRL_MODE", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CEN_LUT_CTRL_MODE);
                else if (strcmp("CEN_LUT_ADRS", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CEN_LUT_ADRS);
                else if (strcmp("CEN_LUT_WEN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, CEN_LUT_WEN);
            }

             else if (strcmp("ADDR_CE_CEN_IA_DATA", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA);
                 if (strcmp("CEN_LUT_DATA", field) == 0)
                     get_value = MXD_REG_GET(rg_val, CEN_LUT_DATA);
             }

              else if (strcmp("ADDR_CE_DSE_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_00);
                if (strcmp("DYNAMIC_SATURATION_EN", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DYNAMIC_SATURATION_EN);
                else if (strcmp("WIN_SELECTION", field) == 0)
                    get_value = MXD_REG_GET(rg_val, WIN_SELECTION);
                else if (strcmp("SATURATION_REGION0_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION0_SEL);
                else if (strcmp("SATURATION_REGION1_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION1_SEL);
                else if (strcmp("SATURATION_REGION2_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION2_SEL);
                else if (strcmp("SATURATION_REGION3_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION3_SEL);
                else if (strcmp("SATURATION_REGION4_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION4_SEL);
                else if (strcmp("SATURATION_REGION5_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION5_SEL);
                else if (strcmp("SATURATION_REGION6_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION6_SEL);
                else if (strcmp("SATURATION_REGION7_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION7_SEL);
                else if (strcmp("SATURATION_REGION8_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION8_SEL);
                else if (strcmp("SATURATION_REGION9_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION9_SEL);
                else if (strcmp("SATURATION_REGION10_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION10_SEL);
                else if (strcmp("SATURATION_REGION11_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION11_SEL);
                else if (strcmp("SATURATION_REGION12_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION12_SEL);
                else if (strcmp("SATURATION_REGION13_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION13_SEL);
                else if (strcmp("SATURATION_REGION14_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION14_SEL);
                else if (strcmp("SATURATION_REGION15_SEL", field) == 0)
                    get_value = MXD_REG_GET(rg_val, SATURATION_REGION15_SEL);
            }

             else if (strcmp("ADDR_CE_DSE_CTRL_01", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_01);
                 if (strcmp("SATURATION_REGION_GAIN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, SATURATION_REGION_GAIN);
             }
              else if (strcmp("ADDR_CE_DSE_CTRL_02", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_02);
                 if (strcmp("HIF_DSE_WDATA_Y_32ND", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIF_DSE_WDATA_Y_32ND);
                 else if (strcmp("HIF_DSE_WDATA_X_32ND", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIF_DSE_WDATA_X_32ND);
             }
              else if (strcmp("ADDR_CE_DSE_IA_CTRL", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL);
                 if (strcmp("DSE_LUT_EN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, DSE_LUT_EN);
                 else if (strcmp("DSE_LUT_ADRS", field) == 0)
                     get_value = MXD_REG_GET(rg_val, DSE_LUT_ADRS);
                 else if (strcmp("DSE_LUT_WEN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, DSE_LUT_WEN);
             }
              else if (strcmp("ADDR_CE_DSE_IA_DATA", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_DATA);
                 if (strcmp("DSE_LUT_DATA", field) == 0)
                     get_value = MXD_REG_GET(rg_val, DSE_LUT_DATA);
             }
              else if (strcmp("ADDR_CE_WB_CTRL_00", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_00);
                 if (strcmp("WB_EN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, WB_EN);
                 else if (strcmp("GAMMA_EN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, GAMMA_EN);
                 else if (strcmp("DEGAMMA_EN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, DEGAMMA_EN);
             }
              else if (strcmp("ADDR_CE_WB_CTRL_01", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_01);
                 if (strcmp("USER_CTRL_G_GAIN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, USER_CTRL_G_GAIN);
                 else if (strcmp("USER_CTRL_B_GAIN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, USER_CTRL_B_GAIN);
                 else if (strcmp("USER_CTRL_R_GAIN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, USER_CTRL_R_GAIN);
             }
              else if (strcmp("ADDR_CE_WB_CTRL_02", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_02);
                 if (strcmp("USER_CTRL_G_OFFSET", field) == 0)
                     get_value = MXD_REG_GET(rg_val, USER_CTRL_G_OFFSET);
                 else if (strcmp("USER_CTRL_B_OFFSET", field) == 0)
                     get_value = MXD_REG_GET(rg_val, USER_CTRL_B_OFFSET);
                 else if (strcmp("USER_CTRL_R_OFFSET", field) == 0)
                     get_value = MXD_REG_GET(rg_val, USER_CTRL_R_OFFSET);
             }
               else if (strcmp("ADDR_CE_GMC_CTRL_00", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_00);
                 if (strcmp("PXL_REP_XPOS", field) == 0)
                     get_value = MXD_REG_GET(rg_val, PXL_REP_XPOS);
                 else if (strcmp("LUT_WMASK_G", field) == 0)
                     get_value = MXD_REG_GET(rg_val, LUT_WMASK_G);
                 else if (strcmp("LUT_WMASK_B", field) == 0)
                     get_value = MXD_REG_GET(rg_val, LUT_WMASK_B);
                 else if (strcmp("LUT_WMASK_R", field) == 0)
                     get_value = MXD_REG_GET(rg_val, LUT_WMASK_R);
                 else if (strcmp("PXL_REP_AREA", field) == 0)
                     get_value = MXD_REG_GET(rg_val, PXL_REP_AREA);
                 else if (strcmp("PXL_REP_YPOS", field) == 0)
                     get_value = MXD_REG_GET(rg_val, PXL_REP_YPOS);
                 else if (strcmp("PXL_REP_DISABLE_G", field) == 0)
                     get_value = MXD_REG_GET(rg_val, PXL_REP_DISABLE_G);
                 else if (strcmp("PXL_REP_DISABLE_B", field) == 0)
                     get_value = MXD_REG_GET(rg_val, PXL_REP_DISABLE_B);
                 else if (strcmp("PXL_REP_DISABLE_R", field) == 0)
                     get_value = MXD_REG_GET(rg_val, PXL_REP_DISABLE_R);
                 else if (strcmp("PXL_REP_ENABLE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, PXL_REP_ENABLE);
             }
               else if (strcmp("ADDR_CE_GMC_CTRL_01", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_01);
                 if (strcmp("PXL_REP_WIDTH", field) == 0)
                     get_value = MXD_REG_GET(rg_val, PXL_REP_WIDTH);
                 else if (strcmp("PXL_REP_HEIGHT", field) == 0)
                     get_value = MXD_REG_GET(rg_val, PXL_REP_HEIGHT);
             }
                else if (strcmp("ADDR_CE_GMC_CTRL_02", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02);
                 if (strcmp("PXL_REP_VALUE_G", field) == 0)
                     get_value = MXD_REG_GET(rg_val, PXL_REP_VALUE_G);
                 else if (strcmp("PXL_REP_VALUE_B", field) == 0)
                     get_value = MXD_REG_GET(rg_val, PXL_REP_VALUE_B);
                 else if (strcmp("PXL_REP_VALUE_R", field) == 0)
                     get_value = MXD_REG_GET(rg_val, PXL_REP_VALUE_R);
                 else if (strcmp("GMC_MODE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, GMC_MODE);
             }
               else if (strcmp("ADDR_CE_GMC_CTRL_03", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_03);
                 if (strcmp("DITHER_EN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, DITHER_EN);
                 else if (strcmp("DECONTOUR_EN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, DECONTOUR_EN);
                 else if (strcmp("DITHER_RANDOM_FREEZE_EN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, DITHER_RANDOM_FREEZE_EN);
                 else if (strcmp("DEMO_PATTERN_ENABLE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, DEMO_PATTERN_ENABLE);
                 else if (strcmp("BIT_MODE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, BIT_MODE);
                 else if (strcmp("DECONTOUR_GAIN_R", field) == 0)
                     get_value = MXD_REG_GET(rg_val, DECONTOUR_GAIN_R);
                 else if (strcmp("DECONTOUR_GAIN_G", field) == 0)
                     get_value = MXD_REG_GET(rg_val, DECONTOUR_GAIN_G);
                 else if (strcmp("DECONTOUR_GAIN_B", field) == 0)
                     get_value = MXD_REG_GET(rg_val, DECONTOUR_GAIN_B);
             }
                else if (strcmp("ADDR_CE_GMC_IA_CTRL", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL);
                 if (strcmp("GMC_LUT_MAX_VALUE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, GMC_LUT_MAX_VALUE);
                 else if (strcmp("GMC_LUT_ADRS", field) == 0)
                     get_value = MXD_REG_GET(rg_val, GMC_LUT_ADRS);
                 else if (strcmp("GMC_LUT_WEN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, GMC_LUT_WEN);
             }
                else if (strcmp("ADDR_CE_GMC_IA_DATA", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_DATA);
                 if (strcmp("LUT_DATA_G", field) == 0)
                     get_value = MXD_REG_GET(rg_val, LUT_DATA_G);
                 else if (strcmp("LUT_DATA_B", field) == 0)
                     get_value = MXD_REG_GET(rg_val, LUT_DATA_B);
                 else if (strcmp("LUT_DATA_R", field) == 0)
                     get_value = MXD_REG_GET(rg_val, LUT_DATA_R);
             }
               else if (strcmp("ADDR_YUV2RGB_CLIP", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_CLIP);
                 if (strcmp("RGB_MAX", field) == 0)
                     get_value = MXD_REG_GET(rg_val, RGB_MAX);
                 else if (strcmp("RGB_MIN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, RGB_MIN);
             }
                else if (strcmp("ADDR_YUV2RGB_OFFSET", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_OFFSET);
                 if (strcmp("Y_OFFSET", field) == 0)
                     get_value = MXD_REG_GET(rg_val, Y_OFFSET);
                 else if (strcmp("C_OFFSET", field) == 0)
                     get_value = MXD_REG_GET(rg_val, C_OFFSET);
             }
               else if (strcmp("ADDR_YUV2RGB_COEF_00", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_COEF_00);
                 if (strcmp("R_CR_COEF", field) == 0)
                     get_value = MXD_REG_GET(rg_val, R_CR_COEF);
                 else if (strcmp("B_CB_COEF", field) == 0)
                     get_value = MXD_REG_GET(rg_val, B_CB_COEF);
             }
               else if (strcmp("ADDR_YUV2RGB_COEF_01", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_COEF_01);
                 if (strcmp("G_CR_COEF", field) == 0)
                     get_value = MXD_REG_GET(rg_val, G_CR_COEF);
                 else if (strcmp("G_CB_COEF", field) == 0)
                     get_value = MXD_REG_GET(rg_val, G_CB_COEF);
             }
                else if (strcmp("ADDR_YUV2RGB_GAIN", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_GAIN);
                 if (strcmp("Y_GAIN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, Y_GAIN);
             }
                else if (strcmp("ADDR_CTI_CTRL_00", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CTI_CTRL_00);
                 if (strcmp("CTI_EN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, CTI_EN);
                 else if (strcmp("TAP_SIZE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, TAP_SIZE);
                 else if (strcmp("COLOR_CTI_GAIN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, COLOR_CTI_GAIN);
             }
                else if (strcmp("ADDR_CTI_CTRL_01", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CTI_CTRL_01);
                 if (strcmp("YCM_EN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, YCM_EN);
                 else if (strcmp("YCM_BAND_SEL", field) == 0)
                     get_value = MXD_REG_GET(rg_val, YCM_BAND_SEL);
                 else if (strcmp("YCM_DIFF_TH", field) == 0)
                     get_value = MXD_REG_GET(rg_val, YCM_DIFF_TH);
                 else if (strcmp("YCM_Y_GAIN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, YCM_Y_GAIN);
                 else if (strcmp("YCM_C_GAIN", field) == 0)
                     get_value = MXD_REG_GET(rg_val, YCM_C_GAIN);
             }
               else if (strcmp("ADDR_HIST_CTRL_00", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_CTRL_00);
                 if (strcmp("HIST_ZONE_DISPLAY", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_ZONE_DISPLAY);
                 else if (strcmp("HIST_LUT_STEP_MODE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_LUT_STEP_MODE);
                 else if (strcmp("HIST_LUT_X_POS_RATE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_LUT_X_POS_RATE);
                 else if (strcmp("HIST_LUT_Y_POS_RATE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_LUT_Y_POS_RATE);
             }
               else if (strcmp("ADDR_HIST_CTRL_01", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_CTRL_01);
                 if (strcmp("HIST_WINDOW_START_X", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_WINDOW_START_X);
                 else if (strcmp("HIST_WINDOW_START_Y", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_WINDOW_START_Y);
             }
               else if (strcmp("ADDR_HIST_CTRL_02", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_CTRL_02);
                 if (strcmp("HIST_WINDOW_X_SIZE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_WINDOW_X_SIZE);
                 else if (strcmp("HIST_WINDOW_Y_SIZE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_WINDOW_Y_SIZE);
             }
                else if (strcmp("ADDR_HIST_MIN_MAX", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_MIN_MAX);
                 if (strcmp("HIST_Y_MIN_VALUE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_Y_MIN_VALUE);
                 else if (strcmp("HIST_Y_MAX_VALUE", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_Y_MAX_VALUE);
             }

              else if (strcmp("ADDR_HIST_TOP5", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_TOP5);
                 if (strcmp("HIST_BIN_HIGH_LEVEL_TOP1", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_BIN_HIGH_LEVEL_TOP1);
                 else if (strcmp("HIST_BIN_HIGH_LEVEL_TOP2", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_BIN_HIGH_LEVEL_TOP2);
                 else if (strcmp("HIST_BIN_HIGH_LEVEL_TOP3", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_BIN_HIGH_LEVEL_TOP3);
                 else if (strcmp("HIST_BIN_HIGH_LEVEL_TOP4", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_BIN_HIGH_LEVEL_TOP4);
             }
              else if (strcmp("ADDR_HIST_TOTAL_CNT", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_TOTAL_CNT);
                 if (strcmp("HIST_BIN_HIGH_LEVEL_TOP5", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_BIN_HIGH_LEVEL_TOP5);
                 else if (strcmp("HIST_WINDOW_PIXEL_TOTAL_CNT", field) == 0)
                     get_value = MXD_REG_GET(rg_val,
                     HIST_WINDOW_PIXEL_TOTAL_CNT);
             }
               else if (strcmp("ADDR_HIST_LUT_CTRL", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_LUT_CTRL);
                 if (strcmp("HIST_LUT_ADRS", field) == 0)
                     get_value = MXD_REG_GET(rg_val, HIST_LUT_ADRS);
             }

            else if (strcmp("ADDR_HIST_LUT_RDATA", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_LUT_RDATA);
                if (strcmp("DSE_LUT_RDATA", field) == 0)
                    get_value = MXD_REG_GET(rg_val, DSE_LUT_RDATA);
            }
           break;
    }
    return get_value;

}


int odin_mxd_set_tuning(int resource_index,
    char* regist, char* field, int p_val)
{
     	u32 rg_val;

     switch (resource_index)
     {
        case ODIN_MXD_TOP_RES:

            if (strcmp("ADDR_XD_TOP_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00);
                if (strcmp("NR_INPUT_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, NR_INPUT_SEL);
                else if (strcmp("RE_INPUT_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, RE_INPUT_SEL);
                else if (strcmp("SCL_INPUT_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SCL_INPUT_SEL);
                else if (strcmp("NR_INT_MASK", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, NR_INT_MASK);
                else if (strcmp("RE_INT_MASK", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, RE_INT_MASK);
                else if (strcmp("CE_INT_MASK", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CE_INT_MASK);
                else if (strcmp("RE_INPUT_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, RE_INPUT_SEL);
                else if (strcmp("REG_UPDATE_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, REG_UPDATE_MODE);
                mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_00, rg_val);
            }
             else if (strcmp("ADDR_XD_TOP_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_01);
                if (strcmp("MEMORY_ALWAYS_ON", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, MEMORY_ALWAYS_ON);
                mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_01, rg_val);
            }
             else if (strcmp("ADDR_XD_TOP_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_02);
                if (strcmp("COLOR_FORMAT", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, COLOR_FORMAT);
                else if (strcmp("CBCR_ORDER", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CBCR_ORDER);
                mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TOP_CTRL_02, rg_val);
            }

             else if (strcmp("ADDR_XD_IMAGE_SIZE0", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE0);
                if (strcmp("NR_PIC_WIDTH", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, NR_PIC_WIDTH);
                else if (strcmp("NR_PIC_HEIGHT", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, NR_PIC_HEIGHT);
                mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE0, rg_val);
            }
             else if (strcmp("ADDR_XD_IMAGE_SIZE1", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE1);
                if (strcmp("RE_PIC_WIDTH", field) == 0)
                    {

                    rg_val = MXD_REG_MOD(rg_val, p_val, RE_PIC_WIDTH);
                    printk("re width size for setting value = %x", p_val);

                    }
                else if (strcmp("RE_PIC_HEIGHT", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, RE_PIC_HEIGHT);
                mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_IMAGE_SIZE1, rg_val);
            }
              else if (strcmp("ADDR_XD_DMA_REG_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL);
                if (strcmp("HIST_LUT_DMA_START", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, HIST_LUT_DMA_START);
                else if (strcmp("HIST_LUT_OPERATION_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val,
                        HIST_LUT_OPERATION_MODE);
                else if (strcmp("DCE_DSE_GAIN_LUT_DMA_START", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, DCE_DSE_GAIN_LUT_DMA_START);
                else if (strcmp("DCE_DSE_GAIN_LUT_OPERATION_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, DCE_DSE_GAIN_LUT_OPERATION_MODE);
                mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_DMA_REG_CTRL, rg_val);
            }
              else if (strcmp("ADDR_XD_HIST_LUT_BADDR", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_HIST_LUT_BADDR);
                if (strcmp("HIST_LUT_DRAM_BASE_ADDRESS", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val,
                        HIST_LUT_DRAM_BASE_ADDRESS);
                 mxd_write_reg(ODIN_MXD_TOP_RES,
                    ADDR_XD_HIST_LUT_BADDR, rg_val);
            }
              else if (strcmp("ADDR_XD_GAIN_LUT_BADDR", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_GAIN_LUT_BADDR);
                if (strcmp("DCE_DSE_GAIN_LUT_DRAM_BASE_ADDRESS", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val,
                    DCE_DSE_GAIN_LUT_DRAM_BASE_ADDRESS);
                 mxd_write_reg(ODIN_MXD_TOP_RES,
                    ADDR_XD_GAIN_LUT_BADDR, rg_val);
            }

              else  if (strcmp("ADDR_XD_TP_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00);
                if (strcmp("TEST_PATTERN_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, TEST_PATTERN_ENABLE);
                else if (strcmp("TP_SYNC_GENERATION_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, TP_SYNC_GENERATION_ENABLE);
                else if (strcmp("TEST_PATTERN_SELECT", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, TEST_PATTERN_SELECT);
                else if (strcmp("TEST_PATTERN_SOLID_Y_DATA", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, TEST_PATTERN_SOLID_Y_DATA);
                else if (strcmp("TEST_PATTERN_SOLID_CB_DATA", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, TEST_PATTERN_SOLID_CB_DATA);
                else if (strcmp("TEST_PATTERN_SOLID_CR_DATA", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, TEST_PATTERN_SOLID_CR_DATA);
                mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00, rg_val);
            }

               else if (strcmp("ADDR_XD_TP_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_01);
                if (strcmp("TEST_PATTERN_IMAGE_WIDTH_SIZE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, TEST_PATTERN_IMAGE_WIDTH_SIZE);
                else if (strcmp("TEST_PATTERN_IMAGE_HEIGHT_SIZE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, TEST_PATTERN_IMAGE_HEIGHT_SIZE);
                 mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_01, rg_val);
            }

              else if (strcmp("ADDR_XD_TP_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_02);
                if (strcmp("TEST_PATTERN_Y_DATA_MASK", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, TEST_PATTERN_Y_DATA_MASK);
                else if (strcmp("TEST_PATTERN_CB_DATA_MASK", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, TEST_PATTERN_CB_DATA_MASK);
                else if (strcmp("TEST_PATTERN_CR_DATA_MASK", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, TEST_PATTERN_CR_DATA_MASK);
                 mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_02, rg_val);
            }

            break;

        case ODIN_MXD_NR_RES:

             if (strcmp("ADDR_WIN_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_00);
                if (strcmp("WIN0_X0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WIN0_X0);
                else if (strcmp("WIN0_Y0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WIN0_Y0);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_00, rg_val);
            }
            else  if (strcmp("ADDR_WIN_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_01);
                if (strcmp("WIN1_X1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WIN1_X1);
                else if (strcmp("WIN1_Y1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WIN1_Y1);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_01, rg_val);
            }
             else  if (strcmp("ADDR_WIN_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_02);
                if (strcmp("WIN2_X0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WIN2_X0);
                else if (strcmp("WIN2_Y0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WIN2_Y0);
                else if (strcmp("AC_BNR_FEATURE_CAL_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, AC_BNR_FEATURE_CAL_MODE);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_02, rg_val);
            }
              else  if (strcmp("ADDR_WIN_CTRL_03", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_03);
                if (strcmp("WIN3_X1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WIN3_X1);
                else if (strcmp("WIN3_Y1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WIN3_Y1);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_WIN_CTRL_03, rg_val);
            }
              else  if (strcmp("ADDR_VFLT_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_VFLT_CTRL_00);
                rg_val = MXD_REG_MOD(rg_val, p_val, VFILTERENABLE);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_VFLT_CTRL_00, rg_val);
                printk("mxd: vFilter input p val  = %d\n", p_val);
                printk("mxd: vFilter val  = %d\n", rg_val);
            }
              else  if (strcmp("ADDR_MNR_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_00);
                if (strcmp("MNR_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, MNR_ENABLE);
                else if (strcmp("EDGE_GAIN_MAPPING_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, EDGE_GAIN_MAPPING_ENABLE);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_00, rg_val);
            }
             else  if (strcmp("ADDR_MNR_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01);
                if (strcmp("HCOEF_00", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_00);
                else if (strcmp("HCOEF_01", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_01);
                else if (strcmp("HCOEF_02", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_02);
                else if (strcmp("HCOEF_03", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_03);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_01, rg_val);
            }
             else  if (strcmp("ADDR_MNR_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02);
                if (strcmp("HCOEF_04", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_04);
                else if (strcmp("HCOEF_05", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_05);
                else if (strcmp("HCOEF_06", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_06);
                else if (strcmp("HCOEF_07", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_07);
                else if (strcmp("HCOEF_08", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_08);
                else if (strcmp("HCOEF_09", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_09);
                else if (strcmp("HCOEF_10", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_10);
                else if (strcmp("HCOEF_11", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_11);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_02, rg_val);
            }

             else  if (strcmp("ADDR_MNR_CTRL_03", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03);
                if (strcmp("HCOEF_12", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_12);
                else if (strcmp("HCOEF_13", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_13);
                else if (strcmp("HCOEF_14", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_14);
                else if (strcmp("HCOEF_15", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_15);
                else if (strcmp("HCOEF_16", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HCOEF_16);
                else if (strcmp("X1_POSITION", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, X1_POSITION);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_03, rg_val);
            }

              else  if (strcmp("ADDR_MNR_CTRL_04", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04);
                if (strcmp("X2_POSITION", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, X2_POSITION);
                else if (strcmp("X3_POSITION", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, X3_POSITION);
                else if (strcmp("X4_POSITION", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, X4_POSITION);
                else if (strcmp("Y1_POSITION", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, Y1_POSITION);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_04, rg_val);
            }
              else  if (strcmp("ADDR_MNR_CTRL_05", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05);
                if (strcmp("Y2_POSITION", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, Y2_POSITION);
                else if (strcmp("Y3_POSITION", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, Y3_POSITION);
                else if (strcmp("Y4_POSITION", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, Y4_POSITION);
                else if (strcmp("FILTER_THRESHOLD", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, FILTER_THRESHOLD);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_05, rg_val);
            }
              else  if (strcmp("ADDR_MNR_CTRL_06", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06);
                if (strcmp("VCOEF0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, VCOEF0);
                else if (strcmp("VCOEF1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, VCOEF1);
                else if (strcmp("VCOEF2", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, VCOEF2);
                else if (strcmp("VCOEF3", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, VCOEF3);
                else if (strcmp("VCOEF4", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, VCOEF4);
                else if (strcmp("VCOEF5", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, VCOEF5);
                else if (strcmp("VCOEF6", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, VCOEF6);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_MNR_CTRL_06, rg_val);
            }
              else  if (strcmp("ADDR_BNR_DC_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_00);
                if (strcmp("DC_BNR_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DC_BNR_ENABLE);
                else if (strcmp("ADAPTIVE_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, ADAPTIVE_MODE);
                else if (strcmp("OUTPUT_MUX", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, OUTPUT_MUX);
                else if (strcmp("DC_OFFSET", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DC_OFFSET);
                else if (strcmp("DC_GAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DC_GAIN);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_00, rg_val);
            }
             else  if (strcmp("ADDR_BNR_DC_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_01);
                if (strcmp("BLUR_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, BLUR_ENABLE);
                else if (strcmp("MUL2BLK", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, MUL2BLK);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_01, rg_val);
            }
              else  if (strcmp("ADDR_BNR_DC_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02);
                if (strcmp("DC_BNR_GAIN_CTRL_Y2", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DC_BNR_GAIN_CTRL_Y2);
                else if (strcmp("DC_BNR_GAIN_CTRL_X2", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DC_BNR_GAIN_CTRL_X2);
                else if (strcmp("DC_BNR_GAIN_CTRL_Y3", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DC_BNR_GAIN_CTRL_Y3);
                else if (strcmp("DC_BNR_GAIN_CTRL_X3", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DC_BNR_GAIN_CTRL_X3);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_02, rg_val);
            }
              else  if (strcmp("ADDR_BNR_DC_CTRL_03", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03);
                if (strcmp("DC_BNR_GAIN_CTRL_Y0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DC_BNR_GAIN_CTRL_Y0);
                else if (strcmp("DC_BNR_GAIN_CTRL_X0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DC_BNR_GAIN_CTRL_X0);
                else if (strcmp("DC_BNR_GAIN_CTRL_Y1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DC_BNR_GAIN_CTRL_Y1);
                else if (strcmp("DC_BNR_GAIN_CTRL_X1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DC_BNR_GAIN_CTRL_X1);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_DC_CTRL_03, rg_val);
            }
             else  if (strcmp("ADDR_BNR_AC_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00);
                if (strcmp("BNR_H_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, BNR_H_EN);
                else if (strcmp("BNR_V_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, BNR_V_EN);
                else if (strcmp("STATUS_READ_TYPE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, STATUS_READ_TYPE);
                else if (strcmp("STATUS_READ_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, STATUS_READ_MODE);
                else if (strcmp("HBMAX_GAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HBMAX_GAIN);
                else if (strcmp("VBMAX_GAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, VBMAX_GAIN);
                else if (strcmp("STRENGTH_RESOLUTION", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, STRENGTH_RESOLUTION);
                else if (strcmp("FITER_TYPE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, FITER_TYPE);
                else if (strcmp("BNR_OUTPUT_MUX", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, BNR_OUTPUT_MUX);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_00, rg_val);
            }
             else  if (strcmp("ADDR_BNR_AC_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01);
                if (strcmp("STRENGTH_H_X0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, STRENGTH_H_X0);
                else if (strcmp("STRENGTH_H_X1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, STRENGTH_H_X1);
                else if (strcmp("STRENGTH_H_MAX", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, STRENGTH_H_MAX);
                else if (strcmp("AC_MIN_TH", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, AC_MIN_TH);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_01, rg_val);
            }
             else  if (strcmp("ADDR_BNR_AC_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02);
                if (strcmp("STRENGTH_V_X0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, STRENGTH_V_X0);
                else if (strcmp("STRENGTH_V_X1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, STRENGTH_V_X1);
                else if (strcmp("STRENGTH_V_MAX", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, STRENGTH_V_MAX);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_02, rg_val);
            }
             else  if (strcmp("ADDR_BNR_AC_CTRL_03", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03);
                if (strcmp("H_OFFSET_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, H_OFFSET_MODE);
                else if (strcmp("MANUAL_OFFSET_H_VALUE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, MANUAL_OFFSET_H_VALUE);
                else if (strcmp("V_OFFSET_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, V_OFFSET_MODE);
                else if (strcmp("MANUAL_OFFSET_V_VALUE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, MANUAL_OFFSET_V_VALUE);
                else if (strcmp("OFFSET_USE_OF_HYSTERISIS", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, OFFSET_USE_OF_HYSTERISIS);
                else if (strcmp("T_FILTER_WEIGHT", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, T_FILTER_WEIGHT);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_03, rg_val);
            }
              else  if (strcmp("ADDR_BNR_AC_CTRL_04", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04);
                if (strcmp("MAX_DELTA_TH0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, MAX_DELTA_TH0);
                else if (strcmp("MAX_DELTA_TH1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, MAX_DELTA_TH1);
                else if (strcmp("H_BLOCKNESS_TH", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, H_BLOCKNESS_TH);
                else if (strcmp("H_WEIGHT_MAX", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, H_WEIGHT_MAX);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_04, rg_val);
            }
              else  if (strcmp("ADDR_BNR_AC_CTRL_05", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_05);
                MXD_REG_MOD(rg_val, p_val, SCALED_USE_OF_HYSTERISIS);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_05, rg_val);
            }
              else  if (strcmp("ADDR_BNR_AC_CTRL_06", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_06);
                if (strcmp("AC_BNR_PROTECT_LVL_2", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, AC_BNR_PROTECT_LVL_2);
                else if (strcmp("AC_BNR_PROTECT_LVL_1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, AC_BNR_PROTECT_LVL_1);
                 mxd_write_reg(ODIN_MXD_NR_RES, ADDR_BNR_AC_CTRL_06, rg_val);
            }

              else  if (strcmp("ADDR_DNR_MAX_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
                if (strcmp("ENABLE_MNR", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, ENABLE_MNR);
                else if (strcmp("ENABLE_DER", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, ENABLE_DER);
                else if (strcmp("ENABLE_DC_BNR", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, ENABLE_DC_BNR);
                else if (strcmp("ENABLE_AC_BNR", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, ENABLE_AC_BNR);
                else if (strcmp("DEBUG_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DEBUG_ENABLE);
                else if (strcmp("DEBUG_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DEBUG_MODE);
                else if (strcmp("DEBUG_MNR_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DEBUG_MNR_ENABLE);
                else if (strcmp("DEBUG_DER_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DEBUG_DER_ENABLE);
                else if (strcmp("DEBUG_DC_BNR_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DEBUG_DC_BNR_ENABLE);
                else if (strcmp("DEBUG_AC_BNR_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DEBUG_AC_BNR_ENABLE);
                else if (strcmp("WIN_CONTROL_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WIN_CONTROL_ENABLE);
                else if (strcmp("BORDER_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, BORDER_ENABLE);
                else if (strcmp("WIN_INOUT", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WIN_INOUT);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, rg_val);
            }
             else  if (strcmp("ADDR_DER_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00);
                if (strcmp("DER_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DER_EN);
                else if (strcmp("DER_GAIN_MAPPING", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DER_GAIN_MAPPING);
                else if (strcmp("DER_BIF_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DER_BIF_EN);
                else if (strcmp("BIF_TH", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, BIF_TH);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_00, rg_val);
            }
              else  if (strcmp("ADDR_DER_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_01);
                if (strcmp("GAIN_TH0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, GAIN_TH0);
                else if (strcmp("GAIN_TH1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, GAIN_TH1);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DER_CTRL_01, rg_val);
            }

              /* CNR */
              else  if (strcmp("ADDR_CNR_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL);
                if (strcmp("CNR_CB_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CNR_CB_EN);
                else if (strcmp("CNR_CR_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CNR_CR_EN);
                else if (strcmp("LOW_LUMINACE_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LOW_LUMINACE_EN);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL, rg_val);
            }
              else  if (strcmp("ADDR_CNR_CB_DIFF", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF);
                if (strcmp("CB_DIFF_BOTTOM", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CB_DIFF_BOTTOM);
                else if (strcmp("CB_DIFF_TOP", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CB_DIFF_TOP);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF, rg_val);
            }
             else  if (strcmp("ADDR_CNR_CB_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL);
                if (strcmp("CB_SLOPE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CB_SLOPE);
                else if (strcmp("CB_DIST", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CB_DIST);
                else if (strcmp("DELTA_CB", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DELTA_CB);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL, rg_val);
            }
             else  if (strcmp("ADDR_CNR_CB_DIFF_LOW", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW);
                if (strcmp("CB_LOW_DIFF_BOTTOM", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CB_LOW_DIFF_BOTTOM);
                else if (strcmp("CB_LOW_DIFF_TOP", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CB_LOW_DIFF_TOP);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_DIFF_LOW, rg_val);
            }
             else  if (strcmp("ADDR_CNR_CB_CTRL_LOW", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW);
                if (strcmp("CB_LOW_SLOPE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CB_LOW_SLOPE);
                else if (strcmp("CB_LOW_DIST", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CB_LOW_DIST);
                else if (strcmp("DELTA_CB_LOW", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DELTA_CB_LOW);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CB_CTRL_LOW, rg_val);
            }
              else  if (strcmp("ADDR_CNR_CR_DIFF", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF);
                if (strcmp("CR_DIFF_BOTTOM", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CR_DIFF_BOTTOM);
                else if (strcmp("CR_DIFF_TOP", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CR_DIFF_TOP);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF, rg_val);
            }
              else  if (strcmp("ADDR_CNR_CR_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL);
                if (strcmp("CR_SLOPE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CR_SLOPE);
                else if (strcmp("CR_DIST", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CR_DIST);
                else if (strcmp("DELTA_CR", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DELTA_CR);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL, rg_val);
            }
              else  if (strcmp("ADDR_CNR_CR_DIFF_LOW", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW);
                if (strcmp("CR_LOW_DIFF_BOTTOM", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CR_LOW_DIFF_BOTTOM);
                else if (strcmp("CR_LOW_DIFF_TOP", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CR_LOW_DIFF_TOP);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_DIFF_LOW, rg_val);
            }
              else  if (strcmp("ADDR_CNR_CR_CTRL_LOW", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW);
                if (strcmp("CR_LOW_SLOPE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CR_LOW_SLOPE);
                else if (strcmp("CR_LOW_DIST", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CR_LOW_DIST);
                else if (strcmp("DELTA_CR_LOW", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DELTA_CR_LOW);
                mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CR_CTRL_LOW, rg_val);
            }
        break;

    case ODIN_MXD_RE_RES:
             if (strcmp("ADDR_RE_DP_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_DP_CTRL);
                if (strcmp("SP_GAIN_E_B", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SP_GAIN_E_B);
                else if (strcmp("SP_GAIN_E_W", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SP_GAIN_E_W);
                else if (strcmp("SP_GAIN_T_B", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SP_GAIN_T_B);
                else if (strcmp("SP_GAIN_T_W", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SP_GAIN_T_W);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_DP_CTRL, rg_val);
            }
            else  if (strcmp("ADDR_RE_CORING_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_CORING_CTRL);
                if (strcmp("CORING_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CORING_MODE);
                else if (strcmp("REG_CORING_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, REG_CORING_EN);
                else if (strcmp("WEIGHT_A", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WEIGHT_A);
                else if (strcmp("WEIGHT_T", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WEIGHT_T);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_CORING_CTRL, rg_val);
            }
             else  if (strcmp("ADDR_RE_SP_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_00);
                if (strcmp("EDGE_ENHANCE_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, EDGE_ENHANCE_ENABLE);
                else if (strcmp("EDGE_OPERATOR_SELECTION", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, EDGE_OPERATOR_SELECTION);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_00, rg_val);
            }
             else  if (strcmp("ADDR_RE_SP_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_01);
                if (strcmp("WHITE_GAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WHITE_GAIN);
                else if (strcmp("BLACK_GAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, BLACK_GAIN);
                else if (strcmp("HORIZONTAL_GAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, HORIZONTAL_GAIN);
                else if (strcmp("VERTICAL_GAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, VERTICAL_GAIN);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_01, rg_val);
            }
              else  if (strcmp("ADDR_RE_SP_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_02);
                if (strcmp("SOBEL_WEIGHT", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SOBEL_WEIGHT);
                else if (strcmp("LAPLACIAN_WEIGHT", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LAPLACIAN_WEIGHT);
                else if (strcmp("SOBEL_MANUAL_MODE_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SOBEL_MANUAL_MODE_EN);
                else if (strcmp("SOBEL_MANUAL_GAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SOBEL_MANUAL_GAIN);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_02, rg_val);
            }
                else  if (strcmp("ADDR_RE_SP_CTRL_04", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_04);
                if (strcmp("DISPLAY_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DISPLAY_MODE);
                else if (strcmp("GX_WEIGHT_MANUAL_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, GX_WEIGHT_MANUAL_EN);
                else if (strcmp("GX_WEIGHT_MANUAL_GAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, GX_WEIGHT_MANUAL_GAIN);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_04, rg_val);
            }
               else  if (strcmp("ADDR_RE_SP_CTRL_06", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_06);
                if (strcmp("LAP_H_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LAP_H_MODE);
                else if (strcmp("LAP_V_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LAP_V_MODE);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_06, rg_val);
            }
               else  if (strcmp("ADDR_RE_SP_CTRL_07", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_07);
                if (strcmp("GB_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, GB_EN);
                else if (strcmp("GB_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, GB_MODE);
                else if (strcmp("GB_X1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, GB_X1);
                 else if (strcmp("GB_Y1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, GB_Y1);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_07, rg_val);
            }
               else  if (strcmp("ADDR_RE_SP_CTRL_08", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_08);
                if (strcmp("GB_X2", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, GB_X2);
                else if (strcmp("GB_Y2", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, GB_Y2);
                else if (strcmp("GB_Y3", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, GB_Y3);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_08, rg_val);
            }
              else  if (strcmp("ADDR_RE_SP_CTRL_09", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_09);
                if (strcmp("LUM1_X_L0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM1_X_L0);
                else if (strcmp("LUM1_X_L1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM1_X_L1);
                else if (strcmp("LUM1_X_H0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM1_X_H0);
                 else if (strcmp("LUM1_X_H1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM1_X_H1);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_09, rg_val);
            }
              else  if (strcmp("ADDR_RE_SP_CTRL_0A", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0A);
                if (strcmp("LUM1_Y0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM1_Y0);
                else if (strcmp("LUM1_Y1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM1_Y1);
                else if (strcmp("LUM1_Y2", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM1_Y2);
                 else if (strcmp("LUM2_X_L0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM2_X_L0);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0A, rg_val);
            }
              else  if (strcmp("ADDR_RE_SP_CTRL_0B", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0B);
                if (strcmp("LUM2_X_L1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM2_X_L1);
                else if (strcmp("LUM2_X_H0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM2_X_H0);
                else if (strcmp("LUM2_X_H1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM2_X_H1);
                 else if (strcmp("LUM2_Y0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM2_Y0);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0B, rg_val);
            }
              else  if (strcmp("ADDR_RE_SP_CTRL_0C", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0C);
                if (strcmp("LUM2_Y1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM2_Y1);
                else if (strcmp("LUM2_Y2", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, LUM2_Y2);
                mxd_write_reg(ODIN_MXD_RE_RES, ADDR_RE_SP_CTRL_0C, rg_val);
            }
        break;

    case ODIN_MXD_CE_RES:
         if (strcmp("ADDR_CE_APL_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_APL_CTRL_00);
                if (strcmp("APL_WIN_CTRL_X0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, APL_WIN_CTRL_X0);
                else if (strcmp("APL_WIN_CTRL_Y0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, APL_WIN_CTRL_Y0);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_APL_CTRL_00, rg_val);
            }
         else if (strcmp("ADDR_CE_APL_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_APL_CTRL_01);
                if (strcmp("APL_WIN_CTRL_X1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, APL_WIN_CTRL_X1);
                else if (strcmp("APL_WIN_CTRL_Y1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, APL_WIN_CTRL_Y1);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_APL_CTRL_01, rg_val);
            }
         else if (strcmp("ADDR_CE_FILM_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_FILM_CTRL_00);
                if (strcmp("VIGNETTING_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, VIGNETTING_ENABLE);
                else if (strcmp("FILM_CONTRAST_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, FILM_CONTRAST_ENABLE);
                else if (strcmp("CONTRAST_ALPHA", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CONTRAST_ALPHA);
                else if (strcmp("CONTRAST_DELTA_MAX", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CONTRAST_DELTA_MAX);
                else if (strcmp("VIGNETTING_GAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, VIGNETTING_GAIN);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_FILM_CTRL_00, rg_val);
                printk("mxd: vignetting input p val  = %d\n", p_val);
                printk("mxd: vignetting enable val  = %d\n", rg_val);
            }
         else if (strcmp("ADDR_CE_FILM_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_FILM_CTRL_02);
                if (strcmp("TONE_GAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, TONE_GAIN);
                else if (strcmp("TONE_OFFSET", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, TONE_OFFSET);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_FILM_CTRL_02, rg_val);
            }
          else if (strcmp("ADDR_CE_VSPRGB_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_VSPRGB_CTRL_00);
                if (strcmp("CONTRAST_CTRL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CONTRAST_CTRL);
                else if (strcmp("CENTER_POSITION", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CENTER_POSITION);
                else if (strcmp("BRIGHTNESS", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, BRIGHTNESS);
                else if (strcmp("CONTRAST_ENABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CONTRAST_ENABLE);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_VSPRGB_CTRL_00, rg_val);
            }
          else if (strcmp("ADDR_CE_CEN_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_00);
                if (strcmp("SELECT_HSV", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SELECT_HSV);
                else if (strcmp("SELECT_RGB", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SELECT_RGB);
                else if (strcmp("VSP_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, VSP_SEL);
                else if (strcmp("GLOBAL_APL_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, GLOBAL_APL_SEL);
                else if (strcmp("REG_CEN_BYPASS", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, REG_CEN_BYPASS);
                else if (strcmp("REG_CEN_DEBUG_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, REG_CEN_DEBUG_MODE);
                else if (strcmp("1ST_CORE_GAIN_DISABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, 1ST_CORE_GAIN_DISABLE);
                else if (strcmp("2ND_CORE_GAIN_DISABLE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, 2ND_CORE_GAIN_DISABLE);
                else if (strcmp("V_SCALER_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, V_SCALER_EN);
                else if (strcmp("DEBUG_MODE_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DEBUG_MODE_SEL);
                else if (strcmp("DEMO_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DEMO_MODE);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_00, rg_val);
            }
          else if (strcmp("ADDR_CE_CEN_CTRL_01", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_01);
                if (strcmp("SHOW_COLOR_REGION0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SHOW_COLOR_REGION0);
                else if (strcmp("SHOW_COLOR_REGION1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SHOW_COLOR_REGION1);
                else if (strcmp("COLOR_REGION_EN0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, COLOR_REGION_EN0);
                else if (strcmp("COLOR_REGION_EN1", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, COLOR_REGION_EN1);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_01, rg_val);
            }
          else if (strcmp("ADDR_CE_CEN_CTRL_02", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_02);
                if (strcmp("IHSV_SGAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, IHSV_SGAIN);
                else if (strcmp("IHSV_VGAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, IHSV_VGAIN);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_02, rg_val);
            }
          else if (strcmp("ADDR_CE_CEN_CTRL_03", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_03);
                if (strcmp("IHSV_HOFFSET", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, IHSV_HOFFSET);
                else if (strcmp("IHSV_SOFFSET", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, IHSV_SOFFSET);
                else if (strcmp("IHSV_VOFFSET", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, IHSV_VOFFSET);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_03, rg_val);
            }
           else if (strcmp("ADDR_CE_CEN_CTRL_04", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_04);
                if (strcmp("DEN_CTRL0", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DEN_CTRL0);
                else if (strcmp("DEN_APL_LIMIT_HIGH", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DEN_APL_LIMIT_HIGH);
                else if (strcmp("DEN_GAIN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DEN_GAIN);
                else if (strcmp("DEN_CORING", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DEN_CORING);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_CTRL_04, rg_val);
            }
             else if (strcmp("ADDR_CE_CEN_IA_CTRL", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL);
                if (strcmp("CEN_LUT_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CEN_LUT_EN);
                else if (strcmp("CEN_LUT_CTRL_MODE", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CEN_LUT_CTRL_MODE);
                else if (strcmp("CEN_LUT_ADRS", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CEN_LUT_ADRS);
                else if (strcmp("CEN_LUT_WEN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, CEN_LUT_WEN);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_CTRL, rg_val);
            }

             else if (strcmp("ADDR_CE_CEN_IA_DATA", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA);
                 if (strcmp("CEN_LUT_DATA", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, CEN_LUT_DATA);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_CEN_IA_DATA, rg_val);
             }

              else if (strcmp("ADDR_CE_DSE_CTRL_00", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_00);
                if (strcmp("DYNAMIC_SATURATION_EN", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DYNAMIC_SATURATION_EN);
                else if (strcmp("WIN_SELECTION", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, WIN_SELECTION);
                else if (strcmp("SATURATION_REGION0_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SATURATION_REGION0_SEL);
                else if (strcmp("SATURATION_REGION1_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SATURATION_REGION1_SEL);
                else if (strcmp("SATURATION_REGION2_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SATURATION_REGION2_SEL);
                else if (strcmp("SATURATION_REGION3_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SATURATION_REGION3_SEL);
                else if (strcmp("SATURATION_REGION4_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SATURATION_REGION4_SEL);
                else if (strcmp("SATURATION_REGION5_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SATURATION_REGION5_SEL);
                else if (strcmp("SATURATION_REGION6_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SATURATION_REGION6_SEL);
                else if (strcmp("SATURATION_REGION7_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SATURATION_REGION7_SEL);
                else if (strcmp("SATURATION_REGION8_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SATURATION_REGION8_SEL);
                else if (strcmp("SATURATION_REGION9_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, SATURATION_REGION9_SEL);
                else if (strcmp("SATURATION_REGION10_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, SATURATION_REGION10_SEL);
                else if (strcmp("SATURATION_REGION11_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, SATURATION_REGION11_SEL);
                else if (strcmp("SATURATION_REGION12_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, SATURATION_REGION12_SEL);
                else if (strcmp("SATURATION_REGION13_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, SATURATION_REGION13_SEL);
                else if (strcmp("SATURATION_REGION14_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, SATURATION_REGION14_SEL);
                else if (strcmp("SATURATION_REGION15_SEL", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val,
                    p_val, SATURATION_REGION15_SEL);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_00, rg_val);
            }

             else if (strcmp("ADDR_CE_DSE_CTRL_01", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_01);
                 if (strcmp("SATURATION_REGION_GAIN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val,
                     p_val, SATURATION_REGION_GAIN);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_01, rg_val);
             }
              else if (strcmp("ADDR_CE_DSE_CTRL_02", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_02);
                 if (strcmp("HIF_DSE_WDATA_Y_32ND", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIF_DSE_WDATA_Y_32ND);
                 else if (strcmp("HIF_DSE_WDATA_X_32ND", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIF_DSE_WDATA_X_32ND);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_CTRL_02, rg_val);
             }
              else if (strcmp("ADDR_CE_DSE_IA_CTRL", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL);
                 if (strcmp("DSE_LUT_EN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, DSE_LUT_EN);
                 else if (strcmp("DSE_LUT_ADRS", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, DSE_LUT_ADRS);
                 else if (strcmp("DSE_LUT_WEN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, DSE_LUT_WEN);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_CTRL, rg_val);
             }
              else if (strcmp("ADDR_CE_DSE_IA_DATA", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_DATA);
                 if (strcmp("DSE_LUT_DATA", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, DSE_LUT_DATA);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_DSE_IA_DATA, rg_val);
             }
              else if (strcmp("ADDR_CE_WB_CTRL_00", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_00);
                 if (strcmp("WB_EN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, WB_EN);
                 else if (strcmp("GAMMA_EN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, GAMMA_EN);
                 else if (strcmp("DEGAMMA_EN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, DEGAMMA_EN);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_00, rg_val);
             }
              else if (strcmp("ADDR_CE_WB_CTRL_01", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_01);
                 if (strcmp("USER_CTRL_G_GAIN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, USER_CTRL_G_GAIN);
                 else if (strcmp("USER_CTRL_B_GAIN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, USER_CTRL_B_GAIN);
                 else if (strcmp("USER_CTRL_R_GAIN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, USER_CTRL_R_GAIN);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_01, rg_val);
             }
              else if (strcmp("ADDR_CE_WB_CTRL_02", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_02);
                 if (strcmp("USER_CTRL_G_OFFSET", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, USER_CTRL_G_OFFSET);
                 else if (strcmp("USER_CTRL_B_OFFSET", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, USER_CTRL_B_OFFSET);
                 else if (strcmp("USER_CTRL_R_OFFSET", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, USER_CTRL_R_OFFSET);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_WB_CTRL_02, rg_val);
             }
               else if (strcmp("ADDR_CE_GMC_CTRL_00", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_00);
                 if (strcmp("PXL_REP_XPOS", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, PXL_REP_XPOS);
                 else if (strcmp("LUT_WMASK_G", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, LUT_WMASK_G);
                 else if (strcmp("LUT_WMASK_B", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, LUT_WMASK_B);
                 else if (strcmp("LUT_WMASK_R", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, LUT_WMASK_R);
                 else if (strcmp("PXL_REP_AREA", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, PXL_REP_AREA);
                 else if (strcmp("PXL_REP_YPOS", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, PXL_REP_YPOS);
                 else if (strcmp("PXL_REP_DISABLE_G", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, PXL_REP_DISABLE_G);
                 else if (strcmp("PXL_REP_DISABLE_B", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, PXL_REP_DISABLE_B);
                 else if (strcmp("PXL_REP_DISABLE_R", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, PXL_REP_DISABLE_R);
                 else if (strcmp("PXL_REP_ENABLE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, PXL_REP_ENABLE);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_00, rg_val);
             }
               else if (strcmp("ADDR_CE_GMC_CTRL_01", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_01);
                 if (strcmp("PXL_REP_WIDTH", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, PXL_REP_WIDTH);
                 else if (strcmp("PXL_REP_HEIGHT", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, PXL_REP_HEIGHT);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_01, rg_val);
             }
                else if (strcmp("ADDR_CE_GMC_CTRL_02", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02);
                 if (strcmp("PXL_REP_VALUE_G", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, PXL_REP_VALUE_G);
                 else if (strcmp("PXL_REP_VALUE_B", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, PXL_REP_VALUE_B);
                 else if (strcmp("PXL_REP_VALUE_R", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, PXL_REP_VALUE_R);
                 else if (strcmp("GMC_MODE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, GMC_MODE);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_02, rg_val);
             }
               else if (strcmp("ADDR_CE_GMC_CTRL_03", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_03);
                 if (strcmp("DITHER_EN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, DITHER_EN);
                 else if (strcmp("DECONTOUR_EN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, DECONTOUR_EN);
                 else if (strcmp("DITHER_RANDOM_FREEZE_EN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val,
                     p_val, DITHER_RANDOM_FREEZE_EN);
                 else if (strcmp("DEMO_PATTERN_ENABLE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, DEMO_PATTERN_ENABLE);
                 else if (strcmp("BIT_MODE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, BIT_MODE);
                 else if (strcmp("DECONTOUR_GAIN_R", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, DECONTOUR_GAIN_R);
                 else if (strcmp("DECONTOUR_GAIN_G", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, DECONTOUR_GAIN_G);
                 else if (strcmp("DECONTOUR_GAIN_B", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, DECONTOUR_GAIN_B);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_CTRL_03, rg_val);
             }
                else if (strcmp("ADDR_CE_GMC_IA_CTRL", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL);
                 if (strcmp("GMC_LUT_MAX_VALUE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, GMC_LUT_MAX_VALUE);
                 else if (strcmp("GMC_LUT_ADRS", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, GMC_LUT_ADRS);
                 else if (strcmp("GMC_LUT_WEN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, GMC_LUT_WEN);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_CTRL, rg_val);
             }
                else if (strcmp("ADDR_CE_GMC_IA_DATA", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_DATA);
                 if (strcmp("LUT_DATA_G", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, LUT_DATA_G);
                 else if (strcmp("LUT_DATA_B", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, LUT_DATA_B);
                 else if (strcmp("LUT_DATA_R", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, LUT_DATA_R);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CE_GMC_IA_DATA, rg_val);
             }
               else if (strcmp("ADDR_YUV2RGB_CLIP", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_CLIP);
                 if (strcmp("RGB_MAX", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, RGB_MAX);
                 else if (strcmp("RGB_MIN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, RGB_MIN);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_CLIP, rg_val);
             }
                else if (strcmp("ADDR_YUV2RGB_OFFSET", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_OFFSET);
                 if (strcmp("Y_OFFSET", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, Y_OFFSET);
                 else if (strcmp("C_OFFSET", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, C_OFFSET);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_OFFSET, rg_val);
             }
               else if (strcmp("ADDR_YUV2RGB_COEF_00", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_COEF_00);
                 if (strcmp("R_CR_COEF", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, R_CR_COEF);
                 else if (strcmp("B_CB_COEF", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, B_CB_COEF);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_COEF_00, rg_val);
             }
               else if (strcmp("ADDR_YUV2RGB_COEF_01", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_COEF_01);
                 if (strcmp("G_CR_COEF", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, G_CR_COEF);
                 else if (strcmp("G_CB_COEF", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, G_CB_COEF);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_COEF_01, rg_val);
             }
                else if (strcmp("ADDR_YUV2RGB_GAIN", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_GAIN);
                 if (strcmp("Y_GAIN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, Y_GAIN);
                  mxd_write_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_GAIN, rg_val);
             }
                else if (strcmp("ADDR_CTI_CTRL_00", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CTI_CTRL_00);
                 if (strcmp("CTI_EN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, CTI_EN);
                 else if (strcmp("TAP_SIZE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, TAP_SIZE);
                 else if (strcmp("COLOR_CTI_GAIN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, COLOR_CTI_GAIN);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CTI_CTRL_00, rg_val);
             }
                else if (strcmp("ADDR_CTI_CTRL_01", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_CTI_CTRL_01);
                 if (strcmp("YCM_EN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, YCM_EN);
                 else if (strcmp("YCM_BAND_SEL", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, YCM_BAND_SEL);
                 else if (strcmp("YCM_DIFF_TH", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, YCM_DIFF_TH);
                 else if (strcmp("YCM_Y_GAIN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, YCM_Y_GAIN);
                 else if (strcmp("YCM_C_GAIN", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, YCM_C_GAIN);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_CTI_CTRL_01, rg_val);
             }
               else if (strcmp("ADDR_HIST_CTRL_00", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_CTRL_00);
                 if (strcmp("HIST_ZONE_DISPLAY", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIST_ZONE_DISPLAY);
                 else if (strcmp("HIST_LUT_STEP_MODE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIST_LUT_STEP_MODE);
                 else if (strcmp("HIST_LUT_X_POS_RATE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIST_LUT_X_POS_RATE);
                 else if (strcmp("HIST_LUT_Y_POS_RATE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIST_LUT_Y_POS_RATE);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_HIST_CTRL_00, rg_val);
             }
               else if (strcmp("ADDR_HIST_CTRL_01", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_CTRL_01);
                 if (strcmp("HIST_WINDOW_START_X", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIST_WINDOW_START_X);
                 else if (strcmp("HIST_WINDOW_START_Y", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIST_WINDOW_START_Y);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_HIST_CTRL_01, rg_val);
             }
               else if (strcmp("ADDR_HIST_CTRL_02", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_CTRL_02);
                 if (strcmp("HIST_WINDOW_X_SIZE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIST_WINDOW_X_SIZE);
                 else if (strcmp("HIST_WINDOW_Y_SIZE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIST_WINDOW_Y_SIZE);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_HIST_CTRL_02, rg_val);
             }
                else if (strcmp("ADDR_HIST_MIN_MAX", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_MIN_MAX);
                 if (strcmp("HIST_Y_MIN_VALUE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIST_Y_MIN_VALUE);
                 else if (strcmp("HIST_Y_MAX_VALUE", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIST_Y_MAX_VALUE);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_HIST_MIN_MAX, rg_val);
             }

              else if (strcmp("ADDR_HIST_TOP5", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_TOP5);
                 if (strcmp("HIST_BIN_HIGH_LEVEL_TOP1", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val,
                     p_val, HIST_BIN_HIGH_LEVEL_TOP1);
                 else if (strcmp("HIST_BIN_HIGH_LEVEL_TOP2", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val,
                     p_val, HIST_BIN_HIGH_LEVEL_TOP2);
                 else if (strcmp("HIST_BIN_HIGH_LEVEL_TOP3", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val,
                     p_val, HIST_BIN_HIGH_LEVEL_TOP3);
                 else if (strcmp("HIST_BIN_HIGH_LEVEL_TOP4", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val,
                     p_val, HIST_BIN_HIGH_LEVEL_TOP4);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_HIST_TOP5, rg_val);
             }
              else if (strcmp("ADDR_HIST_TOTAL_CNT", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_TOTAL_CNT);
                 if (strcmp("HIST_BIN_HIGH_LEVEL_TOP5", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val,
                     p_val, HIST_BIN_HIGH_LEVEL_TOP5);
                 else if (strcmp("HIST_WINDOW_PIXEL_TOTAL_CNT", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val,
                     p_val, HIST_WINDOW_PIXEL_TOTAL_CNT);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_HIST_TOTAL_CNT, rg_val);
             }
               else if (strcmp("ADDR_HIST_LUT_CTRL", regist) == 0)
             {
                 rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_LUT_CTRL);
                 if (strcmp("HIST_LUT_ADRS", field) == 0)
                     rg_val = MXD_REG_MOD(rg_val, p_val, HIST_LUT_ADRS);
                 mxd_write_reg(ODIN_MXD_CE_RES, ADDR_HIST_LUT_CTRL, rg_val);
             }

            else if (strcmp("ADDR_HIST_LUT_RDATA", regist) == 0)
            {
                rg_val = mxd_read_reg(ODIN_MXD_CE_RES, ADDR_HIST_LUT_RDATA);
                if (strcmp("DSE_LUT_RDATA", field) == 0)
                    rg_val = MXD_REG_MOD(rg_val, p_val, DSE_LUT_RDATA);
                mxd_write_reg(ODIN_MXD_CE_RES, ADDR_HIST_LUT_RDATA, rg_val);

            }

        break;

        }

    return 0;
}
int setup_nr(enum odin_dss_mXDalgorithm_level nr_level )
{

	odin_set_nr_vfilter(nr_level);
	odin_mxd_set_nr_mnr(nr_level);
	odin_mxd_set_nr_der(nr_level);
	odin_mxd_set_nr_bnr_dc(nr_level);
	odin_mxd_set_nr_bnr_ac(nr_level);

	set_max_ctrl();
	odin_mxd_set_cnr(nr_level);

	return 0;
}



int _odin_mxd_cnr_disable(void)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL);
	val = MXD_REG_MOD(val, DISABLE, CNR_CB_EN);
	val = MXD_REG_MOD(val, DISABLE, CNR_CR_EN);
	val = MXD_REG_MOD(val, DISABLE, LOW_LUMINACE_EN);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_CNR_CTRL, val);

	return 0;
}


int _odin_mxd_ynr_disable(void)
{
	u32 val;
	val = mxd_read_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL);
	val = MXD_REG_MOD(val, DISABLE, ENABLE_MNR);
	val = MXD_REG_MOD(val, DISABLE, ENABLE_DER);
	val = MXD_REG_MOD(val, DISABLE, ENABLE_DC_BNR);
	val = MXD_REG_MOD(val, DISABLE, ENABLE_AC_BNR);
	mxd_write_reg(ODIN_MXD_NR_RES, ADDR_DNR_MAX_CTRL, val);

	return 0;
}

void nr_disable(void)
{
	_odin_mxd_ynr_disable();
	_odin_mxd_cnr_disable();
}


int setup_re(enum odin_dss_mXDalgorithm_level re_level)
{
	odin_mxd_set_re_peaking(ENABLE, DISABLE, re_level);
	odin_mxd_set_re_coring(ENABLE, re_level);
	return 0;
}

int setup_ce(enum odin_dss_mXDalgorithm_level ce_level)
{
	odin_mxd_set_ce_apl();
	odin_mxd_set_ce_filmlike(ENABLE, ENABLE, ce_level, ce_level);
	odin_mxd_set_ce_whitebalance(DISABLE, ce_level);
	odin_mxd_set_ce_colorenhance(DISABLE, ce_level);
	odin_mxd_set_ce_dce(DISABLE, ce_level);
	odin_mxd_set_ce_dse(DISABLE, ce_level);
	odin_mxd_set_ce_globalctrstnbrtnsctrl(DISABLE, DISABLE, ce_level);
	odin_mxd_set_ce_gmc(DISABLE, DISABLE, DISABLE, ce_level);
	return 0;
}

int odin_mxd_filmlike(bool ce_en, enum odin_dss_mXDalgorithm_level ce_level)
{
    odin_mxd_set_ce_filmlike(ce_en, ce_en, ce_level, ce_level);

    return 0;
}


int odin_mxd_contrast(bool ce_en, enum odin_dss_mXDalgorithm_level cr_level, enum odin_dss_mXDalgorithm_level br_level)
{
    odin_mxd_set_ce_globalctrstnbrtnsctrl(ce_en, cr_level, br_level);

    return 0;
}

int odin_mxd_brightness(bool ce_en, enum odin_dss_mXDalgorithm_level ce_level)
{
    //odin_mxd_set_ce_globalctrstnbrtnsctrl(DISABLE, ce_en, ce_level);

    return 0;
}

int odin_mxd_sharpness(bool re_en, enum odin_dss_mXDalgorithm_level re_level)
{

       odin_mxd_set_re_peaking(re_en, DISABLE, re_level);
	odin_mxd_set_re_coring(re_en, re_level);

	return 0;
}

int odin_mxd_noise_reduction(bool nr_en,
    enum odin_dss_mXDalgorithm_level nr_level )
{
       if (nr_en)
       {
            setup_nr(nr_level);
       }
       else
            nr_disable();

       return 0;
}

int odin_mxd_whitebalance(bool ce_en, enum odin_dss_mXDalgorithm_level ce_level)
{
    odin_mxd_set_ce_whitebalance(ce_en, ce_level);

    return 0;
}

int odin_mxd_colorenhance(bool ce_en, enum odin_dss_mXDalgorithm_level ce_level)
{
    odin_mxd_set_ce_colorenhance(ce_en, ce_level);

    return 0;
}

int odin_mxd_algorithm_setup(mxd_algorithm_data *algorithm_data)
{
	if (algorithm_data->nr_en)
	{
		setup_nr(algorithm_data->nr_level);
	}
	if (algorithm_data->nr_en == DISABLE)
	{
		nr_disable();
	}

	if (algorithm_data->re_en)
	{
		setup_re(algorithm_data->re_level);
	}
	if (algorithm_data->ce_en)
	{
		setup_ce(algorithm_data->ce_level);
	}

	return 0;
}



int mxd_mmap(struct file *flip, struct vm_area_struct *vma)
{
	return 0;
}

long mxd_ioctl(struct file*file, unsigned int cmd, unsigned long arg)
{
	int r = 0;

	void __user *ptr = (void __user*)arg;

	int	mxd_en;
	mxd_verify_data verify_data;
	mxd_algorithm_data  *algorithm_data;
       mxd_noise_reduction_data nr;
       mxd_sharpness_data sh;
       mxd_brightness_data brt;
       mxd_contrast_data ctr;
       mxd_filmlike_data flmk;
       mxd_set_tunning_data set_tune;
       mxd_get_tunning_data get_tune;
	mxd_ce_data ce_data;

	switch (cmd)
	{
		case ODIN_MXDIOC_MXD_VERIFY:
			r = copy_from_user(&verify_data, ptr, sizeof(mxd_verify_data));
			if (!r)
			mxd_en = odin_mxd_adaptedness(verify_data.resource_index,
			               verify_data.color_mode, verify_data.width,
			                  verify_data.height, verify_data.scl_en,
			                                  verify_data.scenario);
			if (copy_to_user((void __user *)arg, &mxd_en, sizeof(int)))
				r = -EFAULT;
			break;

		case ODIN_MXDIOC_MXD_DETAIL_SET:
			algorithm_data = kzalloc(sizeof(mxd_algorithm_data), GFP_KERNEL);
			r = copy_from_user(algorithm_data, ptr,
                                 sizeof(mxd_algorithm_data)) ? :
			odin_mxd_algorithm_setup(algorithm_data);
			kfree(algorithm_data);
			break;

		case ODIN_MXDIOC_MXD_ENA:
			mxd_pw_reset(true);
			break;

		case ODIN_MXDIOC_MXD_DISABLE:
			mxd_pw_reset(false);
			break;

		case ODIN_MXDIOC_MXD_NOISE_REDUCTION:
			printk("ODIN_MXDIOC_MXD_NOISE_REDUCTION\n");
                    r = copy_from_user(&nr, ptr, sizeof(nr)) ?:
                    odin_mxd_noise_reduction(nr.nr_en, nr.nr_level);

                    break;

             case ODIN_MXDIOC_MXD_SHARPNESS:
                    printk("ODIN_MXDIOC_MXD_SHARPNESS\n");
                    r = copy_from_user(&sh, ptr, sizeof(sh)) ?:
                    odin_mxd_sharpness(sh.re_en, sh.re_level);

                    break;

             case ODIN_MXDIOC_MXD_BRIGHTNESS:
                    printk("ODIN_MXDIOC_MXD_BRIGHTNESS\n");
                    r = copy_from_user(&brt, ptr, sizeof(brt)) ?:
                    odin_mxd_brightness(brt.ce_en, brt.ce_level);

                    break;

            case ODIN_MXDIOC_MXD_CONTRAST:
                    printk("ODIN_MXDIOC_MXD_CONTRAST\n");
                    r = copy_from_user(&ctr, ptr, sizeof(ctr)) ?:
                    odin_mxd_contrast(ctr.ce_en, ctr.cr_level, ctr.br_level);

                    break;

            case ODIN_MXDIOC_MXD_FILMLIKE:
                    printk("ODIN_MXDIOC_MXD_FILMLIKE\n");
                    r = copy_from_user(&flmk, ptr, sizeof(flmk)) ?:
                    odin_mxd_filmlike(flmk.ce_en, flmk.ce_level);
                    break;

	    	case ODIN_MXDIOC_MXD_WHITE_BALANCE:
                    printk("ODIN_MXDIOC_MXD_WHITE_BALANCE\n");
                    r = copy_from_user(&ce_data, ptr, sizeof(ce_data)) ?:
                    odin_mxd_whitebalance(ce_data.ce_en, ce_data.ce_level);
                    break;

		case ODIN_MXDIOC_MXD_COLOR_ENHANCE:
                    printk("ODIN_MXDIOC_MXD_COLOR_ENHANCE\n");
                    r = copy_from_user(&ce_data, ptr, sizeof(ce_data)) ?:
                    odin_mxd_colorenhance(ce_data.ce_en, ce_data.ce_level);
                    break;

             case ODIN_MXDIOC_MXD_SET_TUNNING:
                    printk("ODIN_MXDIOC_MXD_TUNNING\n");
                    r = copy_from_user(&set_tune, ptr, sizeof(set_tune)) ?:
                    odin_mxd_set_tuning(set_tune.resource_index,
                        set_tune.regist, set_tune.field, set_tune.p_val);
                    break;

		case ODIN_MXDIOC_MXD_DMB_SET_TUNNING:
                    r = copy_from_user(&get_tune, ptr, sizeof(get_tune)) ;

			if (strcmp("ADDR_YUV2RGB_COEF_00", get_tune.regist) == 0)
               		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_COEF_00, get_tune.p_val);
            		else if (strcmp("ADDR_YUV2RGB_COEF_01", get_tune.regist) == 0)
               		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_COEF_01, get_tune.p_val);
			else if (strcmp("ADDR_YUV2RGB_OFFSET", get_tune.regist) == 0)
               		mxd_write_reg(ODIN_MXD_CE_RES, ADDR_YUV2RGB_OFFSET, get_tune.p_val);
                    break;

             case ODIN_MXDIOC_MXD_GET_TUNNING:
                    r = copy_from_user(&get_tune, ptr, sizeof(get_tune));
                    get_tune.p_val = -1;
                    if (!r)
		      {
                        get_tune.p_val=
                            odin_mxd_get_tuning(get_tune.resource_index,
                            get_tune.regist, get_tune.field);
                        printk("ODIN_MXDIOC_MXD_GET_TUNNING get value is %x\n", get_tune.p_val);
                     }

                    if (get_tune.p_val == -1)
                        r = -EFAULT;
                    else if (copy_to_user((void __user *)arg,
                        &get_tune, sizeof(get_tune)))
                    {
                        printk("get value is %x\n", get_tune.p_val);
                        printk("get value is %s\n", get_tune.regist);
                        printk("get value is %s\n", get_tune.field);
                        printk("get value is %x\n", get_tune.resource_index);
                     }
                   break;

	      default:
			r = -EINVAL;
	}

	return r;
}

int mxd_open(struct inode *inode, struct file *file)
{
	odin_crg_dss_clk_ena(CRG_CORE_CLK, MXD_CLK, true);
	mxd_clk_onoff = true;
	printk("%s: MXD_CLK On \n",__func__);

	mxd_pw_reset(true);
	return 0;
}

int mxd_release(struct inode *inode, struct file *file)
{
    	odin_crg_dss_clk_ena(CRG_CORE_CLK, MXD_CLK, false);
	mxd_clk_onoff = false;
	printk("%s: MXD_CLK Off\n",__func__);

	mxd_pw_reset(false);
	return 0;
}

void free_memory_region(void)
{
	int size;
	int i;

	for (i = 0; i < ODIN_MXD_MAX_RES; i++)
	{
		if (mxd_dev.base[i])
		{
			iounmap(mxd_dev.base[i]);
			mxd_dev.base[i] = NULL;
		}
		if (mxd_dev.iomem[i])
		{
			size = mxd_dev.iomem[i]->end -  mxd_dev.iomem[i]->start + 1;
			release_mem_region( mxd_dev.iomem[i]->start, size);
			mxd_dev.iomem[i] = NULL;
		}


	}
}

const struct file_operations mxd_fops = {
	.owner = THIS_MODULE,
	.open = mxd_open,
	.release = mxd_release,
	.unlocked_ioctl = mxd_ioctl,
	.mmap = mxd_mmap,
};

struct miscdevice mxd_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = DRV_NAME,
	.fops  = &mxd_fops,
};


#ifdef CONFIG_OF
extern struct device odin_device_parent;

static struct of_device_id odindss_mxdhw_match[] = {
	{
		.compatible = "odindss-mxdhw",
	},
	{},
};
#endif

#if 0
static struct platform_device odin_mxdhw_device = {
	.name		= DRV_NAME,
	.id		= -1,
};
#endif

/* for sysfs */
/* set sharpness ========================================== */

static bool odin_mxd_sharpness_onoff = false;

bool get_odin_mxd_sharpness( void )
{
    return odin_mxd_sharpness_onoff;
}
EXPORT_SYMBOL(get_odin_mxd_sharpness);

void set_odin_mxd_sharpness( bool val )
{
    odin_mxd_sharpness_onoff = val;

}
EXPORT_SYMBOL(set_odin_mxd_sharpness);

static ssize_t show_mxd_sharpness_onoff( struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	if ( get_odin_mxd_sharpness() == false )
		printk( "odin mXD sharpness message : off\n"   );
	else if ( get_odin_mxd_sharpness() == true )
		printk( "odin mXD sharpness message : on\n"   );

	return 0;

}

static ssize_t store_mxd_sharpness_onoff( struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{

	if ( (*(buf+0)=='o') && (*(buf+1)=='n') ){
		set_odin_mxd_sharpness( true );
             odin_mxd_set_re_peaking(true, false, LEVEL_MEDIUM);
             odin_mxd_set_re_coring(true, LEVEL_MEDIUM);
		printk( "odin mXD sharpness is ON\n" );
	}else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') ){
		set_odin_mxd_sharpness( false );
             odin_mxd_set_re_peaking(false, false, LEVEL_MEDIUM);
             odin_mxd_set_re_coring(false, LEVEL_MEDIUM);
		printk( "odin mXD sharpness is OFF\n" );
	}else{
		printk( "what?" );
	}

	return count;

}

static DEVICE_ATTR( mxd_sharpness_onoff, 0664,
							show_mxd_sharpness_onoff,
							store_mxd_sharpness_onoff );


/* set contrast ========================================== */

static bool odin_mxd_contrast_onoff = false;

bool get_odin_mxd_contrast( void )
{
    return odin_mxd_contrast_onoff;
}
EXPORT_SYMBOL(get_odin_mxd_contrast);

void set_odin_mxd_contrast( bool val )
{
    odin_mxd_contrast_onoff = val;

}
EXPORT_SYMBOL(set_odin_mxd_contrast);

static ssize_t show_mxd_contrast_onoff( struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	if ( get_odin_mxd_contrast() == false )
		printk( "odin mXD contrast message : off\n"   );
	else if ( get_odin_mxd_contrast() == true )
		printk( "odin mXD contrast message : on\n"   );

	return 0;

}

static ssize_t store_mxd_contrast_onoff( struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{

	if ( (*(buf+0)=='o') && (*(buf+1)=='n') ){
		set_odin_mxd_contrast( true );
             odin_mxd_set_ce_globalctrstnbrtnsctrl(true, false, LEVEL_STRONG);
		printk( "odin mXD contrast is ON\n" );
	}else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') ){
		set_odin_mxd_contrast( false );
		printk( "odin mXD contrast is OFF\n" );
             odin_mxd_set_ce_globalctrstnbrtnsctrl(false, false, LEVEL_STRONG);
	}else{
		printk( "what?" );
	}

	return count;

}

static DEVICE_ATTR( mxd_contrast_onoff, 0664,
							show_mxd_contrast_onoff,
							store_mxd_contrast_onoff );


/* test pattern ========================================== */

static bool odin_mxd_tp_onoff = false;

bool get_odin_mxd_tp( void )
{
    return odin_mxd_tp_onoff;
}
EXPORT_SYMBOL(get_odin_mxd_tp);

void set_odin_mxd_tp( bool val )
{
    odin_mxd_tp_onoff = val;

}
EXPORT_SYMBOL(set_odin_mxd_tp);

static ssize_t show_mxd_tp_onoff( struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	if ( get_odin_mxd_tp() == false )
		printk( "odin mXD tp message : off\n"   );
	else if ( get_odin_mxd_tp() == true )
		printk( "odin mXD tp message : on\n"   );

	return 0;

}

static ssize_t store_mxd_tp_onoff( struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{

	if ( (*(buf+0)=='o') && (*(buf+1)=='n') ){
		odin_mxd_set_tp( true );
		printk( "odin mXD tp is ON\n" );
	}else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') ){
		odin_mxd_set_tp( false );
		printk( "odin mXD tp is OFF\n" );
	}else{
		printk( "what?" );
	}

	return count;

}

static DEVICE_ATTR( mxd_tp_onoff, 0664,
							show_mxd_tp_onoff,
							store_mxd_tp_onoff );


/* set brightness ========================================== */

static bool odin_mxd_brightness_onoff = false;

bool get_odin_mxd_brightness( void )
{
    return odin_mxd_brightness_onoff;
}
EXPORT_SYMBOL(get_odin_mxd_brightness);

void set_odin_mxd_brightness( bool val )
{
    odin_mxd_brightness_onoff = val;

}
EXPORT_SYMBOL(set_odin_mxd_brightness);

static ssize_t show_mxd_brightness_onoff( struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	if ( get_odin_mxd_brightness() == false )
		printk( "odin mXD brightness message : off\n"   );
	else if ( get_odin_mxd_brightness() == true )
		printk( "odin mXD brightness message : on\n"   );

	return 0;

}

static ssize_t store_mxd_brightness_onoff( struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{

	if ( (*(buf+0)=='o') && (*(buf+1)=='n') ){
		set_odin_mxd_brightness( true );
             odin_mxd_set_ce_globalctrstnbrtnsctrl(false, true, LEVEL_STRONG);
		printk( "odin mXD brightness is ON\n" );
	}else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') ){
		set_odin_mxd_brightness( false );
             odin_mxd_set_ce_globalctrstnbrtnsctrl(false, false, LEVEL_STRONG);
		printk( "odin mXD brightness is OFF\n" );
	}else{
		printk( "what?" );
	}

	return count;

}

static DEVICE_ATTR( mxd_brightness_onoff, 0664,
							show_mxd_brightness_onoff,
							store_mxd_brightness_onoff );



/* set filmlike ========================================== */

static bool odin_mxd_filmlike_onoff = false;

bool get_odin_mxd_filmlike( void )
{
    return odin_mxd_filmlike_onoff;
}
EXPORT_SYMBOL(get_odin_mxd_filmlike);

void set_odin_mxd_filmlike( bool val )
{
    odin_mxd_filmlike_onoff = val;

}
EXPORT_SYMBOL(set_odin_mxd_filmlike);

static ssize_t show_mxd_filmlike_onoff( struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	if ( get_odin_mxd_filmlike() == false )
		printk( "odin mXD film-like message : off\n"   );
	else if ( get_odin_mxd_filmlike() == true )
		printk( "odin mXD film-like message : on\n"   );

	return 0;

}

static ssize_t store_mxd_filmlike_onoff( struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{

	if ( (*(buf+0)=='o') && (*(buf+1)=='n') ){
		set_odin_mxd_filmlike( true );
             odin_mxd_set_ce_filmlike(true, true, LEVEL_STRONG, LEVEL_STRONG);
		printk( "odin mXD film-like is ON\n" );
	}else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') ){
		set_odin_mxd_filmlike( false );
             odin_mxd_set_ce_filmlike(false, false, LEVEL_STRONG, LEVEL_STRONG);
             printk( "odin mXD film-like is OFF\n" );
	}else{
		printk( "what?" );
	}

	return count;

}

static DEVICE_ATTR( mxd_filmlike_onoff, 0664,
							show_mxd_filmlike_onoff,
							store_mxd_filmlike_onoff );



/* View message ========================================== */

static bool odin_mem_mxd_message_onoff = false;

bool get_odin_mxd_message( void )
{

	return odin_mem_mxd_message_onoff;

}
EXPORT_SYMBOL(get_odin_mxd_message);

void set_odin_mxd_message( bool val )
{

	odin_mem_mxd_message_onoff = val;

}
EXPORT_SYMBOL(set_odin_mxd_message);

static ssize_t show_mxd_message_onoff( struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	if ( get_odin_mxd_message() == false )
		printk( "odin mxd sysfs test message : off\n"   );
	else if ( get_odin_mxd_message() == true )
		printk( "odin mxd sysfs test message : on\n"   );

	return 0;

}

static ssize_t store_mxd_message_onoff( struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{

	if ( (*(buf+0)=='o') && (*(buf+1)=='n') ){
		set_odin_mxd_message( true );
		printk( "odin xmd sysfs log is ON\n" );
	}else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') ){
		set_odin_mxd_message( false );
		printk( "odin xmd sysfs log is OFF\n" );
	}else{
		printk( "what?" );
	}

	return count;

}

static DEVICE_ATTR( mxd_message_onoff, 0664,
							show_mxd_message_onoff,
							store_mxd_message_onoff );


/* set mxd enable ========================================== */

static ssize_t show_mxd_onoff( struct device *dev,
										struct device_attribute *attr,
										char *buf )
{

	if ( get_odin_mxd_message() == false )
		printk( "odin mxd disable : turn off\n"   );
	else if ( get_odin_mxd_message() == true )
		printk( "odin mxd enable : turn on\n"   );

	return 0;

}

void set_odin_mxd_enable( bool val )
{
	odin_mxd_enable(val);
}
EXPORT_SYMBOL(set_odin_mxd_enable);


static ssize_t store_mxd_onoff( struct device *dev,
										struct device_attribute *attr,
										const char *buf, size_t count )
{
	if ( (*(buf+0)=='o') && (*(buf+1)=='n') ){
		set_odin_mxd_enable( true );
		printk( "odin mxd driver enabled\n" );
	}else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') ){
		set_odin_mxd_enable( false );
		printk( "odin mxd driver disabled\n" );
	}else{
		printk( "what?" );
	}

	return count;

}

static DEVICE_ATTR( mxd_onoff, 0664,
							show_mxd_onoff,
							store_mxd_onoff );
/* DISPC HW IP initialisation */
int odin_mxdhw_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
#ifndef CONFIG_OF
	int size;
#endif
       /* sysfs */
	struct class *cls;
	struct device *dev;
#ifndef CONFIG_OF
	struct resource *mxd_mem;
#endif

	DSSINFO("odin_mxdhw_probe.\n");

#ifdef CONFIG_OF
	pdev->dev.parent = &odin_device_parent;

#endif

	for (i = 0; i < ODIN_MXD_MAX_RES; i++)
	{
#ifdef CONFIG_OF
		mxd_dev.base[i] = of_iomap(pdev->dev.of_node, i);
		if (mxd_dev.base[i]  == 0)
		{
			DSSERR("can't ioremap MXD\n");
			ret = -ENOMEM;
			goto free_region;
		}

#else
		mxd_mem = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!mxd_mem)
		{
			DSSERR("can't get IORESOURCE_MEM MXD\n");
			ret = -EINVAL;
			goto free_region;
		}

		size = mxd_mem->end - mxd_mem->start;
		mxd_dev.iomem[i] = request_mem_region(mxd_mem->start,
                                                   size, pdev->name);
		if (mxd_dev.iomem[i] == NULL)
		{
			DSSERR("failed to request memory region\n");
			ret = -ENOENT;
			goto free_region;
		}

		mxd_dev.base[i] = ioremap(mxd_mem->start, size);
		if (mxd_dev.base[i]  == 0)
		{
			DSSERR("can't ioremap MXD\n");
			ret = -ENOMEM;
			goto free_region;
		}
#endif
	}

	platform_set_drvdata(pdev, &mxd_dev);
	mxd_dev.pdev = pdev;
	ret = misc_register(&mxd_device);

	odin_dma_default_set();
	/*mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00, 0);*/
#if 0
	/* mxd power gating */
	ret	= odin_pd_off(PD_DSS, 2);
	if (ret > 0)
		mxd_pd_onoff = false;
#endif
        /* sysfs */

       cls = class_create(THIS_MODULE, "odin_mxd");
       if ( IS_ERR( cls ) ){
    		return PTR_ERR( cls );
    	}

	dev = device_create( cls, NULL, 0, NULL, "dbg" );
	if ( IS_ERR( dev ) ){
		class_destroy( cls );
		return PTR_ERR( dev );
	}

	ret = device_create_file( dev, &dev_attr_mxd_message_onoff );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
     		class_destroy( cls );
		return -ENODEV;
	}

       ret = device_create_file( dev, &dev_attr_mxd_filmlike_onoff );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
     		class_destroy( cls );
		return -ENODEV;
	}

      ret = device_create_file( dev, &dev_attr_mxd_brightness_onoff );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
     		class_destroy( cls );
		return -ENODEV;
	}

      ret = device_create_file( dev, &dev_attr_mxd_contrast_onoff );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
     		class_destroy( cls );
		return -ENODEV;
	}

      ret = device_create_file( dev, &dev_attr_mxd_sharpness_onoff );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
     		class_destroy( cls );
		return -ENODEV;
	}

      ret = device_create_file( dev, &dev_attr_mxd_tp_onoff );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
     		class_destroy( cls );
		return -ENODEV;
	}

       ret = device_create_file( dev, &dev_attr_mxd_onoff );
	if ( ret != 0 ){
		/*device_destroy( cls, dev ); change with MKDEV() */
		class_destroy( cls );
		return -ENODEV;
	}

       DSSINFO("mxd init for sysfs\n");

       ret = odin_pd_register_dev (&pdev->dev, &odin_pd_dss2);
       if (ret < 0) {
		dev_err(&pdev->dev, "failed to register power domain\n");
		return ret;
       }
       pm_runtime_enable (&pdev->dev);

       return ret;

free_region:
	free_memory_region();

	return ret;
}

static int odin_mxdhw_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int odin_mxdhw_resume(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "resume\n");
#if 0
	odin_mxd_set_tp(0);
	odin_dma_default_set();
	odin_mxd_init();
#endif
	return 0;
}

int odin_mxdhw_remove(struct platform_device *pdev)
{
	struct mxd_mdev *cdev = platform_get_drvdata(pdev);
	misc_deregister(&mxd_device);
	kfree(cdev);

	return 0;
}

static int mxd_runtime_resume(struct device *dev)
{
	mxd_write_reg(ODIN_MXD_TOP_RES, ADDR_XD_TP_CTRL_00, 0);
	mxd_pd_onoff = true;

	dev_info(dev, "mxd_runtime_resume\n");
	return 0;
}

static int mxd_runtime_suspend(struct device *dev)
{
	mxd_pd_onoff = false;
	dev_info(dev, "mxd_runtime_suspend\n");
	return 0;
}

static const struct dev_pm_ops mxd_pm_ops = {
	.runtime_suspend = mxd_runtime_suspend,
	.runtime_resume = mxd_runtime_resume,
};

static struct platform_driver odin_mxdhw_driver = {
	.probe          = odin_mxdhw_probe,
	.remove         = odin_mxdhw_remove,
	.suspend	= odin_mxdhw_suspend,
	.resume		= odin_mxdhw_resume,
	.driver         = {
		.name   = DRV_NAME,
		.owner  = THIS_MODULE,
		.pm	= &mxd_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odindss_mxdhw_match),
#endif
	},
};

int odin_mxd_init_platform_driver(void)
{
	return platform_driver_register(&odin_mxdhw_driver);
}

void odin_mxd_uninit_platform_driver(void)
{
	return platform_driver_unregister(&odin_mxdhw_driver);
}



