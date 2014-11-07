/*
 * linux/drivers/video/odin/dss/dispc.c
 *
 * Copyright (C) 2012 LG Electronics
 * Author:
 *
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
#define DEBUG
#define DSS_SUBSYS_NAME "DIPC"

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#include <linux/export.h>
#endif
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/hardirq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include <video/odindss.h>

#include "dss.h"
#include "dipc.h"
#include "du.h"
#include "../../../../arch/arm/mach-odin/core.h"
#include "../display/panel_device.h"

#ifdef CONFIG_LCD_KCAL
#include "kcalctrl.h"

static struct {
	struct platform_device *pdev;
	struct resource *iomem[NUM_DIPC_BLOCK];
	void __iomem *base_dip[NUM_DIPC_BLOCK];
	int enable_dip[NUM_DIPC_BLOCK];
	int clk[NUM_DIPC_BLOCK];
	int irq;
	spinlock_t spinlock;
	struct completion completion;
	u32 csc_gamma_data[256];
	bool csc_gamma_setting[NUM_DIPC_BLOCK];
	/* for debug....*/
	/* 0: MIPI LCD, 1: RGB or MIPI LCD, 2: HDMI*/
	struct dip_register *dip[NUM_DIPC_BLOCK];
}dip_dev;

/* gamma 2.2 */
static u8 min_LUT[256] = {
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   1,   1,   1,   1,   1,
	1,   1,   1,   1,   1,   2,   2,   2,   2,   2,
	2,   2,   3,   3,   3,   3,   3,   4,   4,   4,
	4,   5,   5,   5,   5,   6,   6,   6,   6,   7,
	7,   7,   8,   8,   8,   9,   9,   9,   10,   10,
	11,  11,  11,  12,  12,  13,  13,  13,  14,  14,
	15,  15,  16,  16,  17,  17,  18,  18,  19,  19,
	20,  20,  21,  22,  22,  23,  23,  24,  25,  25,
	26,  26,  27,  28,  28,  29,  30,  30,  31,  32,
	33,  33,  34,  35,  35,  36,  37,  38,  39,  39,
	40,  41,  42,  43,  43,  44,  45,  46,  47,  48,
	49,  49,  50,  51,  52,  53,  54,  55,  56,  57,
	58,  59,  60,  61,  62,  63,  64,  65,  66,  67,
	68,  69,  70,  71,  73,  74,  75,  76,  77,  78,
	79,  81,  82,  83,  84,  85,  87,  88,  89,  90,
	91,  93,  94,  95,  97,  98,  99,  100, 102, 103,
	105, 106, 107, 109, 110, 111, 113, 114, 116, 117,
	119, 120, 121, 123, 124, 126, 127, 129, 130, 132,
	133, 135, 137, 138, 140, 141, 143, 145, 146, 148,
	149, 151, 153, 154, 156, 158, 159, 161, 163, 165,
	166, 168, 170, 172, 173, 175, 177, 179, 181, 182,
	184, 186, 188, 190, 192, 194, 196, 197, 199, 201,
	203, 205, 207, 209, 211, 213, 215, 217, 219, 221,
	223, 225, 227, 229, 231, 234, 236, 238, 240, 242,
	244, 246, 248, 251, 253, 255
};

static int get_kcal_data_from_misc(void)
{
	/* HANA_TBD read MISC setting value */
	return (g_kcal_ctrl_mode);
}

static int convert_kcal_dipc_lut(int dip_ch)
{
	union dip_ccr_lcd_gamma_addr	gamma_addr;
	union dip_ccr_lcd_gamma_rdata	gamma_rdata;
	union dip_lcd_dsp_configuration dsp_configuration;
	union dip_ctrl ctrl;

	int i;

	/* enable dip*/
	ctrl.a32 = readl(dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);
	ctrl.af.enable = 1;
	writel(ctrl.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);

	/* gamma disable*/
	dsp_configuration.a32 = readl(dip_dev.base_dip[dip_ch] +
					ODIN_DIP_LCD_DSP_CONFIGURATION);
	dsp_configuration.af.gamma_enb = 0;
	writel(dsp_configuration.a32, dip_dev.base_dip[dip_ch] +
					ODIN_DIP_LCD_DSP_CONFIGURATION);

	for (i = 0; i < DIP_IMPRB_GAMMA_TABLE_MXA_LENGTH; i++)
	{
		gamma_addr.af.address = i;
		/* There is no leaner rgb LUT, so, index i is the same linear r, g or b LUT */
		gamma_rdata.af.r = min_LUT[i] + ( ( i * g_kcal_r )
				   - ( min_LUT[i] * g_kcal_r ) ) / 256;
		/* There is no leaner rgb LUT, so, index i is the same linear r, g or b LUT */
		gamma_rdata.af.g = min_LUT[i] + ( ( i * g_kcal_g )
				   - ( min_LUT[i] * g_kcal_g ) ) / 256;
		/* There is no leaner rgb LUT, so, index i is the same linear r, g or b LUT */
		gamma_rdata.af.b = min_LUT[i] + ( ( i * g_kcal_b )
				   - ( min_LUT[i] * g_kcal_b ) ) / 256;

		writel(gamma_addr.a32, dip_dev.base_dip[0] +
					ODIN_DIP_CCR_LCD_GAMMA_ADDR);
		writel(gamma_rdata.a32, dip_dev.base_dip[0] +
					ODIN_DIP_CCR_LCD_GAMMA_RDATA);
	}

	/* gamma enable*/
	dsp_configuration.af.gamma_enb = 1;
	writel(dsp_configuration.a32, dip_dev.base_dip[dip_ch] +
					ODIN_DIP_LCD_DSP_CONFIGURATION);

	return 0;
}

int update_screen_dipc_lut(void)
{
	int dip_ch = 0;

	odin_crg_dss_clk_ena(CRG_CORE_CLK, DIP0_CLK | MIPI0_CLK, true);
	dip_dev.csc_gamma_setting[dip_ch] = true;
	return convert_kcal_dipc_lut(dip_ch);
}
EXPORT_SYMBOL(update_screen_dipc_lut);


#endif // CONFIG_LCD_KCAL
int dipc_test_fifo_read(int dip_ch)
{
	u32 fifo_data = 0xffffffff;
	u32 fifo_status;
	u64 cnt = 10000000000;

	fifo_status = readl(dip_dev.base_dip[dip_ch] + ODIN_DIP_FIFO_STATUS);
	while (1)
	{
		if (cnt-- <10)
		{
			printk("fifo read timeout");
			break;
		}
		if (fifo_status & (1 << 5)) /*empty*/
			continue;

			fifo_data = readl(dip_dev.base_dip[dip_ch] +
					  ODIN_DIP_TEST_FIFO_RDATA);
			break;
	}
	return fifo_data;
}

int dipc_fifo_empty_check(int dip_ch)
{
	u32 fifo_status;

	fifo_status = readl(dip_dev.base_dip[dip_ch] + ODIN_DIP_FIFO_STATUS);

	if (fifo_status & (1 << 5)) /*empty*/
		return 1;
	else
		return 0;
}

/* configuration for RGB LCD*/
static int dipc_rgb_timing_config(struct odin_video_timings *ti)
{
	union rgb_cfg0 cfg0;
	union rgb_cfg1 cfg1;
	union rgb_cfg2 cfg2;
	union rgb_cfg3 cfg3;
	union rgb_cfg4 cfg4;
	union rgb_cfg5 cfg5;

	/* horizontal config*/
	cfg0.a32 = 0x00;
	cfg0.af.hsync_width = ti->hsw;
	cfg0.af.hsync_total = ti->hbp + ti->x_res + ti->hfp;
	writel(cfg0.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_LCD0] +
			ODIN_DIP_CFG0);


	cfg2.a32 = 0x00;
	cfg2.af.hvalid_start = ti->hbp;
	cfg2.af.hvalid_stop = ti->hbp + ti->x_res - 1;
	writel(cfg2.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_LCD0] +
			 ODIN_DIP_CFG2);

	/* check....*/
	cfg4.a32 = 0x00;
	cfg4.af.hblank_start = ti->hbp;	/* 확인 필요.*/
	cfg4.af.hblank_stop = cfg4.af.hblank_start + ti->x_res -1;
	writel(cfg4.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_LCD0] +
			 ODIN_DIP_CFG4);

	/* vertical config*/
	cfg1.a32 = 0x00;
	cfg1.af.vsync_width = ti->vsw;
	cfg1.af.vsync_total = ti->vbp + ti->y_res + ti->vfp;
	writel(cfg1.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_LCD0] +
			 ODIN_DIP_CFG1);

	cfg3.a32 = 0x00;
	cfg3.af.vvalid_start = ti->vbp;
	cfg3.af.vvalid_stop = ti->vbp + ti->y_res - 1;
	writel(cfg3.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_LCD0] +
			 ODIN_DIP_CFG3);

	cfg5.a32 = 0x00;
	cfg5.af.vblank_start = ti->vbp;
	cfg5.af.vblank_stop = cfg5.af.vblank_start + ti->y_res - 1;
	writel(cfg5.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_LCD0] +
	       		 ODIN_DIP_CFG5);

	return 0;
}

#if 0
/* configuration for MIPI LCD*/
static int dipc_mipi_timing_config(struct odin_video_timings *ti)
{
	return 0;
}
#endif

#if 1
static int dipc_tv_timing_config(struct odin_video_timings *ti)
{
	union tv_cfg0 cfg0;
	union tv_cfg1 cfg1;
	union tv_cfg2 cfg2;
	union tv_cfg3 cfg3;
	union tv_cfg4 cfg4;
	union tv_cfg5 cfg5;
	union tv_cfg6 cfg6;
	union tv_cfg7 cfg7;
	union tv_cfg8 cfg8;
	union tv_cfg9 cfg9;
	union tv_cfg10 cfg10;
	union tv_cfg11 cfg11;
	union tv_cfg12 cfg12;
	union tv_cfg13 cfg13;

	u32 sx, sy;

	ti->mPixelRepetitionInput = 1;

	printk("************************************************\n");
		printk("mPixelRepetitionInput:%d mPixelClock:%d mInterlaced:%d\n",
	ti->mPixelRepetitionInput, ti->mPixelClock, ti->mInterlaced);

		printk("mHActive:%d mHBlanking:%d mHBorder:%d mHImageSize:%d\n",
	ti->mHActive, ti->mHBlanking, ti->mHBorder, ti->mHImageSize);

		printk("mHSyncOffset:%d mHSyncPulseWidth:%d mHSyncPolarity:%d \n",
	ti->mHSyncOffset, ti->mHSyncPulseWidth, ti->mHSyncPolarity);


		printk("mVActive:%d mVBlanking:%d mVBorder:%d mVImageSize:%d\n",
	ti->mVActive, ti->mVBlanking, ti->mVBorder, ti->mVImageSize);

		printk("mVSyncOffset:%d mVSyncPulseWidth:%d mVSyncPolarity:%d \n",
	ti->mVSyncOffset, ti->mVSyncPulseWidth, ti->mVSyncPolarity);
		printk("************************************************\n");

	sx = sy = 0;
	/* cfg0*/
	cfg0.a32 = 0x00;

	cfg0.af.vsync_total = (ti->mVActive + ti->mVBlanking) -1;
	cfg0.af.hsync_total = ti->mHActive /
		ti->mPixelRepetitionInput + ti->mHBlanking /
		ti->mPixelRepetitionInput -1;
	writel(cfg0.a32,
	       dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG0);

	/* cfg1*/
	cfg1.a32 = 0x00;
	cfg1.af.hsync_r = (ti->mHSyncPulseWidth /
			   ti->mPixelRepetitionInput) -1;
	cfg1.af.hsync_f = 0;
	writel(cfg1.a32,
	       dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG1);

	/* cfg2*/
	cfg2.a32 = 0x00;
	cfg2.af.vsync_vr0 = ti->mVSyncPulseWidth;
	cfg2.af.vsync_hr0 = 0;
	writel(cfg2.a32,
	       dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG2);

	/* cfg3*/
	cfg3.a32 = 0x00;
	cfg3.af.vsync_vf0 = 0;	/* vsync vertical falling*/
	cfg3.af.vsync_hf0 = 0;	/* vsync horizontal falling*/
	writel(cfg3.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
			 ODIN_DIP_CFG3);

	cfg4.a32 = 0x00;
	cfg5.a32 = 0x00;

	if (ti->mInterlaced) {
		cfg4.af.vsync_vr1 = ti->mVBlanking + ti->mVActive +
				    ti->mVSyncPulseWidth;
		cfg4.af.vsync_hr1 = cfg0.af.hsync_total /2;
		cfg5.af.vsync_vf1 = ti->mVBlanking + ti->mVActive;
		cfg5.af.vsync_hf1 = cfg0.af.hsync_total /2;
	}
	else {
		cfg4.af.vsync_vr1 = 0xfff;
		cfg4.af.vsync_hr1 = 0xfff;
		cfg5.af.vsync_vf1 = 0xfff;
		cfg5.af.vsync_hf1 = 0xfff;
	}
	writel(cfg4.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
			 ODIN_DIP_CFG4);
	writel(cfg5.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
			 ODIN_DIP_CFG5);

	/* cfg8*/
	cfg6.a32 = 0x0FFF0FFF;
	cfg7.a32 = 0x0FFF0FFF;
	cfg8.a32 = 0x00;
	cfg9.a32 = 0x00;

	cfg8.af.hvalid_start = (ti->mHBlanking / ti->mPixelRepetitionInput) -
		(ti->mHSyncOffset / ti->mPixelRepetitionInput) + sx;
	cfg8.af.hvalid_stop = cfg8.af.hvalid_start + ti->mHActive - 1;
	cfg9.af.vvalid_start_odd = ti->mVBlanking - ti->mVSyncOffset + sy;
	cfg9.af.vvalid_stop_odd = cfg9.af.vvalid_start_odd + ti->mVActive - 1;

	writel(cfg6.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
			 ODIN_DIP_CFG6);
	writel(cfg7.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
			 ODIN_DIP_CFG7);
	writel(cfg8.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
			 ODIN_DIP_CFG8);
	writel(cfg9.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
			 ODIN_DIP_CFG9);


	cfg10.a32 = 0x00;
	cfg13.a32 = 0x00;

	if (ti->mInterlaced) {
		cfg10.af.vvalid_start_even = ti->mVBlanking +
			ti->mVActive + ti->mVBlanking -
			ti->mVSyncOffset + sx + 1;
		cfg10.af.vvalid_stop_even = cfg10.af.vvalid_start_even +
					    ti->mVActive - 1;
		cfg13.af.vblank_start_even = ti->mVBlanking +
					     ti->mVActive + ti->mVBlanking -
					     ti->mVSyncOffset + 1;
		cfg13.af.vblank_stop_even = cfg13.af.vblank_start_even +
					    ti->mVActive - 1;
	}
	else {
		cfg10.af.vvalid_start_even = 0;
		cfg10.af.vvalid_stop_even = 0;
		cfg13.af.vblank_start_even = 0;
		cfg13.af.vblank_stop_even = 0;
	}

	/* blank*/
	cfg11.af.hblank_start = (ti->mHBlanking /
		ti->mPixelRepetitionInput) - (ti->mHSyncOffset /
		ti->mPixelRepetitionInput) - 1;
	cfg11.af.hblank_stop = cfg11.af.hblank_start +
		(ti->mHActive / ti->mPixelRepetitionInput);
	cfg12.af.vblank_start_odd = ti->mVBlanking + ti->mVSyncOffset;
	cfg12.af.vblank_stop_odd = cfg12.af.vblank_start_odd + ti->mVActive;

	writel(cfg10.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
			  ODIN_DIP_CFG10);
	writel(cfg11.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
			  ODIN_DIP_CFG11);
	writel(cfg12.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
			  ODIN_DIP_CFG12);
	writel(cfg13.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
			  ODIN_DIP_CFG13);

	writel(0x004000C0, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
					ODIN_DIP_LCD_DSP_CONFIGURATION);
	writel(0x00FFFFFF, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
					ODIN_DIP_CCR_LCD_CLAMP0_MAX);
	writel(0x00FFFFFF, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
					ODIN_DIP_CCR_LCD_CLAMP1_MAX);
	writel(0x00000400, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
					ODIN_DIP_CSC0_MATRIXA_LCD);
	writel(0x04000000, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
					ODIN_DIP_CSC0_MATRIXC_LCD);
#if 0
	writel(0x04640897, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG0);
	writel(0x02B00000, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG1);
	writel(0x00000005, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG2);
	/*writel(0x00000005, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
		ODIN_DIP_CFG3);*/
	writel(0x0FFF0FFF, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG4);
	writel(0x0FFF0FFF, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG5);
	writel(0x0FFF0FFF, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG6);
	writel(0x0FFF0FFF, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG7);
	writel(0x083F00C0, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG8);
	writel(0x04600029, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG9);
	writel(0x00000000, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG10);
	writel(0x083F00BF, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG11);
	writel(0x04610029, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG12);
	writel(0x00000000, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG13);
#endif


	return 0;
}

#else
/* configuration for HDMI*/
static int dipc_tv_timing_config(struct odin_video_timings *ti)
{
	int r, prepeat;

	union tv_cfg0 cfg0;
	union tv_cfg1 cfg1;
	union tv_cfg2 cfg2;
	union tv_cfg3 cfg3;
	union tv_cfg4 cfg4;
	union tv_cfg5 cfg5;
	union tv_cfg6 cfg6;
	union tv_cfg7 cfg7;
	union tv_cfg8 cfg8;
	union tv_cfg9 cfg9;
	union tv_cfg10 cfg10;
	union tv_cfg11 cfg11;
	union tv_cfg12 cfg12;
	union tv_cfg13 cfg13;

	u32 sx, sy;

	prepeat = ti->mPixelRepetitionInput + 1;

	printk("************************************************\n");
		printk("mPixelRepetitionInput:%d mPixelClock:%d mInterlaced:%d\n",
	ti->mPixelRepetitionInput, ti->mPixelClock, ti->mInterlaced);

		printk("mHActive:%d mHBlanking:%d mHBorder:%d mHImageSize:%d\n",
	ti->mHActive, ti->mHBlanking, ti->mHBorder, ti->mHImageSize);

		printk("mHSyncOffset:%d mHSyncPulseWidth:%d mHSyncPolarity:%d \n",
	ti->mHSyncOffset, ti->mHSyncPulseWidth, ti->mHSyncPolarity);


		printk("mVActive:%d mVBlanking:%d mVBorder:%d mVImageSize:%d\n",
	ti->mVActive, ti->mVBlanking, ti->mVBorder, ti->mVImageSize);

		printk("mVSyncOffset:%d mVSyncPulseWidth:%d mVSyncPolarity:%d \n",
	ti->mVSyncOffset, ti->mVSyncPulseWidth, ti->mVSyncPolarity);
		printk("************************************************\n");

	sx = sy = 0;
	/* cfg0*/
	cfg0.a32 = 0x00;

	cfg0.af.vsync_total = (ti->mVActive + ti->mVBlanking) -1;
	cfg0.af.hsync_total = ti->mHActive /
		prepeat + ti->mHBlanking / prepeat -1;
	writel(cfg0.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG0);

	/* cfg1*/
	cfg1.a32 = 0x00;
	cfg1.af.hsync_r = (ti->mHSyncPulseWidth / prepeat) -1;
	cfg1.af.hsync_f = 0;
	writel(cfg1.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG1);

	/* cfg2*/
	cfg2.a32 = 0x00;
	cfg2.af.vsync_vr0 = ti->mVSyncPulseWidth;
	cfg2.af.vsync_hr0 = 0;
	writel(cfg2.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG2);

	/* cfg3*/
	cfg3.a32 = 0x00;
	cfg3.af.vsync_vf0 = 0;	/* vsync vertical falling*/
	cfg3.af.vsync_hf0 = 0;	/* vsync horizontal falling*/
	writel(cfg3.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG3);

	cfg4.a32 = 0x00;
	cfg5.a32 = 0x00;

	if (ti->mInterlaced) {
		cfg4.af.vsync_vr1 = ti->mVBlanking + ti->mVActive + ti->mVSyncPulseWidth;
		cfg4.af.vsync_hr1 = cfg0.af.hsync_total /2;
		cfg5.af.vsync_vf1 = ti->mVBlanking + ti->mVActive;
		cfg5.af.vsync_hf1 = cfg0.af.hsync_total /2;
	}
	else {
		cfg4.af.vsync_vr1 = 0xfff;
		cfg4.af.vsync_hr1 = 0xfff;
		cfg5.af.vsync_vf1 = 0xfff;
		cfg5.af.vsync_hf1 = 0xfff;
	}
	writel(cfg4.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG4);
	writel(cfg5.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG5);

	/* cfg8*/
	cfg6.a32 = 0x0FFF0FFF;
	cfg7.a32 = 0x0FFF0FFF;
	cfg8.a32 = 0x00;
	cfg9.a32 = 0x00;

	cfg8.af.hvalid_start = (ti->mHBlanking / prepeat) -
		(ti->mHSyncOffset / prepeat) + sx;
	cfg8.af.hvalid_stop = cfg8.af.hvalid_start + ti->mHActive - 1;
	cfg9.af.vvalid_start_odd = ti->mVBlanking - ti->mVSyncOffset + sy;
	cfg9.af.vvalid_stop_odd = cfg9.af.vvalid_start_odd + ti->mVActive - 1;

	writel(cfg6.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG6);
	writel(cfg7.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG7);
	writel(cfg8.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG8);
	writel(cfg9.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG9);


	cfg10.a32 = 0x00;
	cfg13.a32 = 0x00;

	if (ti->mInterlaced) {
		cfg10.af.vvalid_start_even = ti->mVBlanking +
			ti->mVActive + ti->mVBlanking - ti->mVSyncOffset + sx + 1;
		cfg10.af.vvalid_stop_even = cfg10.af.vvalid_start_even + ti->mVActive - 1;
		cfg13.af.vblank_start_even = ti->mVBlanking +
			ti->mVActive + ti->mVBlanking - ti->mVSyncOffset + 1;
		cfg13.af.vblank_stop_even = cfg13.af.vblank_start_even + ti->mVActive - 1;
	}
	else {
		cfg10.af.vvalid_start_even = 0;
		cfg10.af.vvalid_stop_even = 0;
		cfg13.af.vblank_start_even = 0;
		cfg13.af.vblank_stop_even = 0;
	}

	/* blank*/
	cfg11.af.hblank_start = (ti->mHBlanking /
		prepeat) - (ti->mHSyncOffset /
		prepeat) - 1;
	cfg11.af.hblank_stop = cfg11.af.hblank_start +
		(ti->mHActive / prepeat);
	cfg12.af.vblank_start_odd = ti->mVBlanking + ti->mVSyncOffset;
	cfg12.af.vblank_stop_odd = cfg12.af.vblank_start_odd + ti->mVActive;

	writel(cfg10.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG10);
	writel(cfg11.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG11);
	writel(cfg12.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG12);
	writel(cfg13.a32, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] + ODIN_DIP_CFG13);

	writel(0x004000C0, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
					ODIN_DIP_LCD_DSP_CONFIGURATION);
	writel(0x00FFFFFF, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
					ODIN_DIP_CCR_LCD_CLAMP0_MAX);
	writel(0x00FFFFFF, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
					ODIN_DIP_CCR_LCD_CLAMP1_MAX);
	writel(0x00000400, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
					ODIN_DIP_CSC0_MATRIXA_LCD);
	writel(0x04000000, dip_dev.base_dip[ODIN_DSS_CHANNEL_HDMI] +
					ODIN_DIP_CSC0_MATRIXC_LCD);

	return 0;
}
#endif

/* RGB --> RGB... No conversion*/
static int dipc_csc_config(int dip_ch, int matrix_id)
{
	/* Configure matrix for YUV->YUV or RGB->RGB */
	/* No conversion */

	/* Matrix A */
	union dip_csc_matrix1 matrix1;
	/*union dip_csc_matrix2 matrix2;*/
	int offset;

	offset = matrix_id == 0 ? 0 : 0x20;

	matrix1.a32 = 0x00;

	matrix1.af.a11 = 0x400;
	matrix1.af.a12 = 0x0;
	writel(matrix1.a32, dip_dev.base_dip[dip_ch] +
					ODIN_DIP_CSC0_MATRIXA_LCD + offset);

	/* Matrix B */
	writel(0, dip_dev.base_dip[dip_ch] +
					ODIN_DIP_CSC0_MATRIXB_LCD + offset);

	/* Matrix C */
	matrix1.af.a11 = 0x00;
	matrix1.af.a12 = 0x400;
	writel(matrix1.a32, dip_dev.base_dip[dip_ch] +
					ODIN_DIP_CSC0_MATRIXC_LCD + offset);

	/* Matrix D */
	writel(0, dip_dev.base_dip[dip_ch] +
					ODIN_DIP_CSC0_MATRIXD_LCD + offset);

	/* Matrix E */
	writel(0, dip_dev.base_dip[dip_ch] +
					ODIN_DIP_CSC0_MATRIXE_LCD + offset);

	/* Matrix F */
	matrix1.a32 = 0x00;
	matrix1.af.a11 = 0x400;
	writel(matrix1.a32, dip_dev.base_dip[dip_ch] +
					ODIN_DIP_CSC0_MATRIXF_LCD + offset);

	/* Matrix G */
	writel(0, dip_dev.base_dip[dip_ch] +
					ODIN_DIP_CSC0_MATRIXG_LCD + offset);

	/* Matrix H */
	writel(0, dip_dev.base_dip[dip_ch] +
					ODIN_DIP_CSC0_MATRIXH_LCD + offset);

	return 0;
}

static int dipc_ccr_clamp_config(int dip_ch, int clamp_id)
{
	union dip_ccr_lcd_clamp clamp_max, clamp_min;
	int offset;
	clamp_max.a32 = 0x00;
	clamp_min.a32 = 0x00;

	offset = clamp_id == 0 ? 0 : 0x08;

	switch (dip_ch) {
	case ODIN_DSS_CHANNEL_LCD0:
	case ODIN_DSS_CHANNEL_LCD1:
		/* default setting */
		clamp_max.af.r = 0xff;
		clamp_max.af.g = 0xff;
		clamp_max.af.b = 0xff;
		clamp_min.af.r = 0x0;
		clamp_min.af.g = 0x0;
		clamp_min.af.b = 0x0;
		break;
	case ODIN_DSS_CHANNEL_HDMI:
		/* default setting */
		clamp_max.af.r = 0xf0;
		clamp_max.af.g = 0xee;
		clamp_max.af.b = 0xee;
		clamp_min.af.r = 0x10;
		clamp_min.af.g = 0x10;
		clamp_min.af.b = 0x10;
		break;
	}

	writel(clamp_max.a32, dip_dev.base_dip[dip_ch]
		+ ODIN_DIP_CCR_LCD_CLAMP0_MAX + offset);
	writel(clamp_min.a32, dip_dev.base_dip[dip_ch]
		+ ODIN_DIP_CCR_LCD_CLAMP0_MIN + offset);

	return 0;
}

#if 0
static int dipc_dsp_config(void)
{
	return 0;
}
#endif

int dipc_ccr_gamma_config(int dip_ch, bool enable,
	enum odin_dip_gamma_operation ops)
{
	union dip_ccr_lcd_gamma_addr	gamma_addr;
	union dip_ccr_lcd_gamma_rdata	gamma_rdata;
	union dip_lcd_dsp_configuration dsp_configuration;
	union dip_ctrl ctrl;

	int i;


#ifdef CONFIG_LCD_KCAL
	/* HANA_TBD get kcal r, g, b from misc */
	if(get_kcal_data_from_misc()){
		convert_kcal_dipc_lut(dip_ch);
		return 0;
	}
#endif

	/* enable dip*/
	ctrl.a32 = readl(dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);
	ctrl.af.enable = 1;
	writel(ctrl.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);

	if (enable) {
		/* gamma disable*/
		dsp_configuration.a32 = readl(dip_dev.base_dip[dip_ch] +
						ODIN_DIP_LCD_DSP_CONFIGURATION);
		dsp_configuration.af.gamma_enb = 0;
		writel(dsp_configuration.a32, dip_dev.base_dip[dip_ch] +
						ODIN_DIP_LCD_DSP_CONFIGURATION);

		for (i = 0; i < DIP_IMPRB_GAMMA_TABLE_MXA_LENGTH; i++)
		{
			gamma_addr.af.address = i;
			if (ops == ODIN_DIP_GAMMA_TABLE_VAL)
				gamma_rdata.a32 = dip_dev.csc_gamma_data[i];
			else if (ops == ODIN_DIP_GAMMA_R_SATURATION)
				gamma_rdata.a32 = (i | (i << 8) | (0xff << 16));
			else if (ops == ODIN_DIP_GAMMA_R_REJECION)
				gamma_rdata.a32 = (i | (i << 8));
			else
				gamma_rdata.a32 = (i | (i << 8) | (i << 16));

			writel(gamma_addr.a32, dip_dev.base_dip[0] +
						ODIN_DIP_CCR_LCD_GAMMA_ADDR);
			writel(gamma_rdata.a32, dip_dev.base_dip[0] +
						ODIN_DIP_CCR_LCD_GAMMA_RDATA);
		}

		/* gamma enable*/
		dsp_configuration.af.gamma_enb = 1;
		writel(dsp_configuration.a32, dip_dev.base_dip[dip_ch] +
						ODIN_DIP_LCD_DSP_CONFIGURATION);


	} else {

		dsp_configuration.a32 = readl(dip_dev.base_dip[dip_ch] +
						ODIN_DIP_LCD_DSP_CONFIGURATION);
		dsp_configuration.af.gamma_enb = 0;
		writel(dsp_configuration.a32, dip_dev.base_dip[dip_ch] +
						ODIN_DIP_LCD_DSP_CONFIGURATION);

		writel(0x00, dip_dev.base_dip[dip_ch] +
						ODIN_DIP_CCR_LCD_GAMMA_ADDR);
		writel(0x00, dip_dev.base_dip[dip_ch] +
						ODIN_DIP_CCR_LCD_GAMMA_RDATA);
	}

	return 0;
}

static int dipc_pcmpc_config(int dip_ch)
{
	union dip_lcd_dsp_configuration dsp_configuration;

	dsp_configuration.a32 = readl(dip_dev.base_dip[dip_ch] +
						ODIN_DIP_LCD_DSP_CONFIGURATION);
	dsp_configuration.af.pcompc_in_fmt = 3; /*bypass*/
	dsp_configuration.af.pcompc_swap = 0;
	writel(dsp_configuration.a32, dip_dev.base_dip[dip_ch] +
						ODIN_DIP_LCD_DSP_CONFIGURATION);

	return 0;
}

static int dipc_bus_interace_type_config(
	int dip_ch, enum odin_display_type type)
{
	union dip_ctrl dip_ctrl;

	if (dip_ch != 0)
	{
		DSSERR("we can config bus_type in LCD0\n");
		return -EINVAL;
	}

	dip_ctrl.a32 = readl(dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);
	if (type == ODIN_DISPLAY_TYPE_DSI)
		dip_ctrl.af.lcd_if_type = 1; /* 0: RGBLCD path, 1: MIPILCD path*/
	else
		dip_ctrl.af.lcd_if_type = 0; /* 0: RGBLCD path, 1: MIPILCD path*/
	writel(dip_ctrl.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);

	return 0;
}

/* dip channel 0 or 1 config*/
static int dipc_mipi_rgb_config (struct odin_dss_device *dssdev, int dip_ch)
{
	u32 data_width, input_fmt, output_fmt;
	u32 fifo_push_size;

	union dip_ctrl ctrl;
	union dip_flush_resync_shadow flush_resync_shadow;

	/* Don't use debug because of spinlock DSSDBG("dipc_mipi_rgb_config\n"); */

	/* disable dip*/
	ctrl.a32 = readl(dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);
	/*ctrl.af.enable = 0;*/
	writel(ctrl.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);

	/* config dsp*/

	/* config csc id - 0*/
	dipc_csc_config(dip_ch, 0);
	/* config ccr clamp id - 0*/
	dipc_ccr_clamp_config(dip_ch, 0);
	/* config csc id - 1*/
	dipc_csc_config(dip_ch, 1);
	/* config ccr clamp id - 1*/
	dipc_ccr_clamp_config(dip_ch, 1);
	/* enable or disable of gamma table*/
	dipc_ccr_gamma_config(dip_ch, 0, ODIN_DIP_GAMMA_DEFAULT_VAL);

	data_width = dssdev->panel.timings.data_width;
	input_fmt = 0;
	output_fmt = 0;

	if (dssdev->channel == ODIN_DSS_CHANNEL_LCD0) {

		switch (data_width) {
		case ODIN_DATA_WIDTH_24BIT:
		case ODIN_DATA_WIDTH_18BIT:
			fifo_push_size = ODIN_DIP_CTRL_FIFO_SIZE_32BITS;
			break;
		case ODIN_DATA_WIDTH_16BIT:
		case ODIN_DATA_WIDTH_12BIT:
			fifo_push_size = ODIN_DIP_CTRL_FIFO_SIZE_16BITS;
			break;
		default:
			fifo_push_size = ODIN_DIP_CTRL_FIFO_SIZE_8BITS;
		}

		/* config cfg*/
		dipc_rgb_timing_config(&dssdev->panel.timings);

		/* flush fifo*/
		flush_resync_shadow.a32 = readl(dip_dev.base_dip[dip_ch] +
			ODIN_DIP_FLUSH_RESYNC_SHADOW);
		flush_resync_shadow.af.fifo0_flush = 1;
		flush_resync_shadow.af.fifo1_flush = 1;
		writel(flush_resync_shadow.a32, dip_dev.base_dip[dip_ch] +
			ODIN_DIP_FLUSH_RESYNC_SHADOW);

		/* dip ctrl*/
		ctrl.a32 = readl(dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);
		ctrl.af.fifo1_pushsize = fifo_push_size;
		ctrl.af.timing_width = data_width;
		ctrl.af.fifo_data_swap = 1;
		ctrl.af.swap = dssdev->panel.timings.rgb_swap;
		ctrl.af.imgfmtin = input_fmt;
		ctrl.af.imgfmtout = output_fmt;
		ctrl.af.invert_blank = dssdev->panel.timings.blank_pol;
		ctrl.af.invert_vsync = dssdev->panel.timings.vsync_pol;
		ctrl.af.invert_hsync = dssdev->panel.timings.hsync_pol;
		ctrl.af.lcd_invert_pclk = dssdev->panel.timings.pclk_pol;
		ctrl.af.rgb_rotate_blank_data = 0;
		ctrl.af.rgb_play_blank_data = 1;
		ctrl.af.rgb_blank_select = 0;  /*Full or Partial*/
		/* turn on Pclk*/
		ctrl.af.lcd_drv_pclk = 1;
		writel(ctrl.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);
	 }else if (dssdev->channel == ODIN_DSS_CHANNEL_LCD1) {

		/* MIPI DSI Initialize ???*/
		/*fo_push_size = ODIN_DIP_CTRL_FIFO_SIZE_32BITS;*/

	}
	return 0;
}


static int dipc_tv_config(struct odin_dss_device *dssdev, int dip_ch)
{
	u32 data_width, input_fmt, output_fmt;
	u32 fifo_push_size;
	u32 blankdata;

	union tv_ctrl ctrl;
	union dip_flush_resync_shadow flush_resync_shadow;

	/* disable dip*/
	ctrl.a32 = readl(dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);
	ctrl.af.enable = 0;
	writel(ctrl.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);

	/* config dsp*/

	/* config csc id - 0*/
	dipc_csc_config(dip_ch, 0);
	/* config ccr clamp id - 0*/
	dipc_ccr_clamp_config(dip_ch, 0);
	/* config csc id - 1*/
	dipc_csc_config(dip_ch, 1);
	/* config ccr clamp id - 1*/
	dipc_ccr_clamp_config(dip_ch, 1);
	/* enable or disable of gamma table*/
	dipc_ccr_gamma_config(dip_ch, 0, ODIN_DIP_GAMMA_DEFAULT_VAL);

	data_width = dssdev->panel.timings.data_width;
	input_fmt = 0;
	output_fmt = 0;

	switch (data_width) {
	case ODIN_DATA_WIDTH_24BIT:
	case ODIN_DATA_WIDTH_18BIT:
		fifo_push_size = ODIN_DIP_CTRL_FIFO_SIZE_32BITS;
		break;
	case ODIN_DATA_WIDTH_16BIT:
	case ODIN_DATA_WIDTH_12BIT:
		fifo_push_size = ODIN_DIP_CTRL_FIFO_SIZE_16BITS;
		break;
	default:
		fifo_push_size = ODIN_DIP_CTRL_FIFO_SIZE_8BITS;
	}

	/* config cfg*/
	dipc_tv_timing_config(&dssdev->panel.timings);

	blankdata = 0x000000;
	writel(blankdata, dip_dev.base_dip[dip_ch] + ODIN_DIP_BLANK_DATA);


	/* force shadow update in DSP*/

	flush_resync_shadow.a32 = readl(dip_dev.base_dip[dip_ch] +
		ODIN_DIP_FLUSH_RESYNC_SHADOW);
	flush_resync_shadow.af.shadow_update = 1;
	writel(ctrl.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_FLUSH_RESYNC_SHADOW);

	/* flush fifo*/
	ctrl.a32 = readl(dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);
	ctrl.af.fifo1_pushsize = fifo_push_size;
	ctrl.af.timing_width = data_width;
	ctrl.af.imgfmtin = input_fmt;
	ctrl.af.imgfmtout = output_fmt;

	ctrl.af.invert_hsync = dssdev->panel.timings.mHSyncPolarity;
	ctrl.af.invert_vsync = dssdev->panel.timings.mVSyncPolarity;
#if 0
	ctrl.af.invert_blank = 1;
	ctrl.af.play_blank_data = 1;
	ctrl.af.blank_select = 1;
	ctrl.af.sof_eof_mode = 1;
#endif
	ctrl.af.enable = 1;

	writel(ctrl.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);

	return 0;
}

int dipc_mipi_rgb_init(struct odin_dss_device *dssdev, int dip_ch)
{
	enum odin_display_type channel_type;		/* RGB, MIPI  or HDMI*/
	bool gamma_enable = false;

	union dip_ctrl ctrl;
	union dip_interrupt 		irq_mask;
	union dip_interrupt 		irq_clear;

	/* disable dip*/
	ctrl.a32 = readl(dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);
	ctrl.af.enable = 0;
	writel(ctrl.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);

	channel_type = dssdev->type;

 	/* mask all DIP IRQs*/
	if (channel_type & ODIN_DISPLAY_TYPE_DSI)
		irq_mask.a32 = 0x1F;
	else /*LCD0, HDMI*/
		irq_mask.a32 = 0x1FFFF;
	writel(irq_mask.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_INTMASK);

	/* clear all DIP IRQs*/
	if (channel_type & ODIN_DISPLAY_TYPE_DSI)
		irq_clear.a32 = 0x1F;
	else /*LCD0, HDMI*/
		irq_clear.a32 = 0x1FFFF;
	writel(irq_clear.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_INTCLEAR);

	/* config csc id - 0*/
	dipc_csc_config(dip_ch, 0);
	/* config ccr clamp id - 0*/
	dipc_ccr_clamp_config(dip_ch, 0);
	/* config csc id - 1*/
	dipc_csc_config(dip_ch, 1);
	/* config ccr clamp id - 1*/
	dipc_ccr_clamp_config(dip_ch, 1);

	/*config PCmp C*/
	dipc_pcmpc_config(dip_ch);

	/*config bus interace type*/
	if (dip_ch == 0)
		dipc_bus_interace_type_config(dip_ch, dssdev->type);

	dipc_datapath_en(dip_ch, ODIN_DISPLAY_TYPE_DSI);

	/* enable or disable of gamma table*/
	dipc_ccr_gamma_config(dip_ch, gamma_enable, ODIN_DIP_GAMMA_DEFAULT_VAL);

	return 0;
}

static int dipc_tv_init(struct odin_dss_device *dssdev, int dip_ch)
{
	union dip_ctrl ctrl;
	union dip_interrupt 		irq_mask;
	union dip_interrupt 		irq_clear;

	/* disable dip*/
	ctrl.a32 = readl(dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);
	ctrl.af.enable = 0;
	writel(ctrl.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_CTRL);

 	/* mask all DIP IRQs*/
	if (dip_ch == 1) /*LCD 1 Channel*/
		irq_mask.a32 = 0x1F;
	else /*LCD0, HDMI*/
		irq_mask.a32 = 0x1FFFF;
	writel(irq_mask.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_INTMASK);

    	/* clear all DIP IRQs*/
	if (dip_ch == 1) /*LCD 1 Channel*/
		irq_clear.a32 = 0x1F;
	else /*LCD0, HDMI*/
		irq_clear.a32 = 0x1FFFF;
	writel(irq_clear.a32, dip_dev.base_dip[dip_ch] + ODIN_DIP_INTCLEAR);

	/* config dsp*/

	/* config csc id - 0*/
	dipc_csc_config(dip_ch, 0);
	/* config ccr clamp id - 0*/
	dipc_ccr_clamp_config(dip_ch, 0);
	/* config csc id - 1*/
	dipc_csc_config(dip_ch, 1);
	/* config ccr clamp id - 1*/
	dipc_ccr_clamp_config(dip_ch, 1);
	/* enable or disable of gamma table*/
	dipc_ccr_gamma_config(dip_ch, 0, ODIN_DIP_GAMMA_DEFAULT_VAL);

	/*config PCmp C*/
	dipc_pcmpc_config(dip_ch);

	/*config bus interace type*/
	dipc_bus_interace_type_config(dip_ch, dssdev->type);

	return 0;
}

#if 0
static unsigned int dipc_clear_all_irq(void)
{
	int reg = 0;

	reg |= 0xffffffff;
	writel(reg, dip_dev.base_dip[0] + ODIN_DIP_INTCLEAR);
#if 0 /*현재 Bitstrame에 DIP1, DIP2 존재안함*/
	writel(reg, dip_dev.base_dip[1] + ODIN_DIP_INTCLEAR);
	writel(reg, dip_dev.base_dip[2] + ODIN_DIP_INTCLEAR);
#endif

	return 0;
}
#endif

unsigned int dipc_get_irq_mask(enum odin_channel channel)
{
	int reg;
	reg = readl(dip_dev.base_dip[channel] + ODIN_DIP_INTMASK);
	return reg;
}

unsigned int dipc_get_irq_status(enum odin_channel channel)
{
	int reg;
	reg = readl(dip_dev.base_dip[channel] + ODIN_DIP_INTSOURCE);
	return reg;
}

unsigned int dipc_clear_irq(enum odin_channel channel, u32 reg_val)
{
	writel(reg_val, dip_dev.base_dip[channel] + ODIN_DIP_INTCLEAR);
	return 0;
}

unsigned int dipc_mask_irq(enum odin_channel channel, u32 reg_val)
{
	writel(reg_val, dip_dev.base_dip[channel] + ODIN_DIP_INTMASK);
	return 0;
}


unsigned int dipc_check_irq(int dip_irq, enum odin_channel channel)
{
	union dip_interrupt dip_interrupt_src;

	dip_interrupt_src.a32 = readl(dip_dev.base_dip[channel] + ODIN_DIP_INTSOURCE);

    if (channel == ODIN_DSS_CHANNEL_LCD0)
        return ((dip_interrupt_src.a32 >> dip_irq) & 0x1);
    else if (channel == ODIN_DSS_CHANNEL_LCD1)
        return ((dip_interrupt_src.a32 >> dip_irq) & 0x1);
    else /* ODIN_DSS_CHANNEL_HDMI*/
        return ((dip_interrupt_src.a32 >> dip_irq) & 0x1);
}

static int dipc_config(struct odin_dss_device *dss_dev)
{
	int ret = 0;
	switch (dss_dev->channel) {
	case ODIN_DSS_CHANNEL_LCD0:
		ret = dipc_mipi_rgb_config(dss_dev, 0);
		break;
	case ODIN_DSS_CHANNEL_LCD1:
		ret = dipc_mipi_rgb_config(dss_dev, 1);
		break;
	case ODIN_DSS_CHANNEL_HDMI:
		ret = dipc_tv_config(dss_dev, 2);
		break;
	case ODIN_DSS_CHANNEL_MAX:
		break;
	}

	return ret;
}

static int dipc_enable(enum odin_channel ch)
{
	u32 reg;
	union dip_flush_resync_shadow flush_resync_shadow;

	/* Enable*/
	reg = readl(dip_dev.base_dip[ch] + ODIN_DIP_CTRL);
	reg |= 0x01;
	writel(reg, dip_dev.base_dip[ch] + ODIN_DIP_CTRL);

	/* Fifo Flush*/
	reg = readl(dip_dev.base_dip[ch] + ODIN_DIP_FLUSH_RESYNC_SHADOW);
	reg |= (0x07);
	writel(reg, dip_dev.base_dip[ch] + ODIN_DIP_FLUSH_RESYNC_SHADOW);

	/* force shadow update in DSP*/
	flush_resync_shadow.a32 = readl(dip_dev.base_dip[ch] +
					ODIN_DIP_FLUSH_RESYNC_SHADOW);
	flush_resync_shadow.af.shadow_update = 1;
	writel(flush_resync_shadow.a32, dip_dev.base_dip[ch] +
					ODIN_DIP_FLUSH_RESYNC_SHADOW);

	return 0;
}

void dipc_datapath_en(enum odin_mipi_dsi_index ch, enum odin_display_type type)
{
	u32 reg;

	if (ch == ODIN_DSI_CH0 && type == ODIN_DISPLAY_TYPE_DSI)
	{
		reg = readl(dip_dev.base_dip[ch] + ODIN_DIP_CTRL);
		reg |= (1 << 22) | 1;
		writel(reg, dip_dev.base_dip[ch] + ODIN_DIP_CTRL);
	}
	else if (ch == ODIN_DSI_CH1 && type == ODIN_DISPLAY_TYPE_DSI)
	{
		reg = readl(dip_dev.base_dip[ch] + ODIN_DIP_CTRL);
		reg |= 1;
		writel(reg, dip_dev.base_dip[ch] + ODIN_DIP_CTRL);
	}
	else /*RGB*/
	{
		reg = readl(dip_dev.base_dip[ch] + ODIN_DIP_CTRL);
		reg &= ~(1 << 22);
		writel(reg, dip_dev.base_dip[ch] + ODIN_DIP_CTRL);
	}
}

static int dipc_disable(enum odin_display_type ch)
{
	u32 reg;
	reg = readl(dip_dev.base_dip[ch] + ODIN_DIP_CTRL);
	reg &= ~(0x00000001);
	writel(reg, dip_dev.base_dip[ch] + ODIN_DIP_CTRL);

	return 0;
}

int odin_dipc_resync_hdmi(struct odin_dss_device *dss_dev)
{
	int ret;
	enum odin_channel channel = dss_dev->channel;

	ret = dipc_disable(dss_dev->channel);

	ret = dipc_enable(dss_dev->channel);

	return ret;
}
EXPORT_SYMBOL(odin_dipc_resync_hdmi);

int odin_dipc_power_on(struct odin_dss_device *dss_dev)
{
	int ret;
	enum odin_channel channel = dss_dev->channel;

	ret = dipc_config(dss_dev);

	if(channel == ODIN_DSS_CHANNEL_HDMI)
	{
		odin_du_set_ovl_sync_enable(dss_dev->channel, true);
	}

	ret = dipc_enable(dss_dev->channel);

	return ret;
}
EXPORT_SYMBOL(odin_dipc_power_on);

int odin_dipc_power_off(struct odin_dss_device *dss_dev)
{
	int ret;

	ret = dipc_disable(dss_dev->channel);

	return ret;
}
EXPORT_SYMBOL(odin_dipc_power_off);


int odin_dipc_fifo_flush(int dip_ch)
{
	union dip_flush_resync_shadow flush_resync_shadow;

	/* flush fifo*/
	flush_resync_shadow.a32 = readl(dip_dev.base_dip[dip_ch] +
					ODIN_DIP_FLUSH_RESYNC_SHADOW);
	flush_resync_shadow.af.fifo0_flush = 1;
	flush_resync_shadow.af.fifo1_flush = 1;
	flush_resync_shadow.af.shadow_update = 1;
	writel(flush_resync_shadow.a32, dip_dev.base_dip[dip_ch] +
					ODIN_DIP_FLUSH_RESYNC_SHADOW);

	return 0;
}


int odin_dipc_get_lcd_cnt(int dip_ch)
{
	union dip_lcd_byte_count lcd_byte_count;
	int ret = 0;

	/* flush fifo*/
	lcd_byte_count.a32 = readl(dip_dev.base_dip[dip_ch] +
				   ODIN_DIP_LCD_BYTE_COUNT);

	ret = lcd_byte_count.af.fifo0_rd_byte_count;

	/*printk("odiin_dipc_get_lcd_cnt ret : %d, lcd_byte_cnt : %d \n", ret, lcd_byte_count.a32);*/
	return ret;
}

int odin_dipc_resync(int dip_ch)
{
	union dip_flush_resync_shadow flush_resync_shadow;

	/* resync on*/
	flush_resync_shadow.a32 = readl(dip_dev.base_dip[dip_ch] +
					ODIN_DIP_FLUSH_RESYNC_SHADOW);
	flush_resync_shadow.af.resync_on = 1;
	writel(flush_resync_shadow.a32, dip_dev.base_dip[dip_ch] +
					ODIN_DIP_FLUSH_RESYNC_SHADOW);

	return 0;
}

unsigned int *pclk_gpio;

int odin_dipc_display_init(struct odin_dss_device *dss_dev)
{
	/* #define PCLK_GPIO_ADDR 0x31433008*/

	int ret = 0;

	/*pclk_gpio = MMIO_P2V(PCLK_GPIO_ADDR);*/
	/*(pclk_gpio) = 0x1; //From Firmware Code?*/

	switch (dss_dev->channel) {
	case ODIN_DSS_CHANNEL_LCD0:
		ret = dipc_mipi_rgb_init(dss_dev, 0);
		break;
	case ODIN_DSS_CHANNEL_LCD1:
#ifdef CONFIG_ODIN_DSS_FPGA
#else
		ret = dipc_mipi_rgb_init(dss_dev, 1);
#endif
		break;
	case ODIN_DSS_CHANNEL_HDMI:
#ifdef CONFIG_ODIN_DSS_FPGA
#else
		ret = dipc_tv_init(dss_dev, 2);
#endif
		break;
	case ODIN_DSS_CHANNEL_MAX:
		break;
	}

	return ret;
}
EXPORT_SYMBOL(odin_dipc_display_init);

int odin_dipc_ccr_gamma_config(int dip_ch, u32 src_gamma_data)
{
	memcpy(dip_dev.csc_gamma_data, &src_gamma_data,
		sizeof(dip_dev.csc_gamma_data));
	odin_crg_dss_clk_ena(CRG_CORE_CLK, DIP0_CLK, true);
	dip_dev.csc_gamma_setting[dip_ch] = true;
	dipc_ccr_gamma_config(dip_ch, true, ODIN_DIP_GAMMA_TABLE_VAL);
	dip_dev.csc_gamma_setting[dip_ch] = false;
	return 0;
}
EXPORT_SYMBOL(odin_dipc_ccr_gamma_config);

bool odin_dipc_ccr_gamma_config_status(int dip_ch)
{
	return dip_dev.csc_gamma_setting[dip_ch];
}
EXPORT_SYMBOL(odin_dipc_ccr_gamma_config_status);

static void free_memory_region(void)
{
#ifndef CONFIG_OF
	int size;
#endif
	int i;

	DSSERR("err : free_memory_region");
	for (i=0; i<NUM_DIPC_BLOCK; i++) {
		if (dip_dev.base_dip[i]) {
			iounmap(dip_dev.base_dip[i]);
			dip_dev.base_dip[i] = NULL;
		}

#ifndef CONFIG_OF
		if (dip_dev.iomem[i]) {
			size = dip_dev.iomem[i]->end - dip_dev.iomem[i]->start + 1;
			release_mem_region(dip_dev.iomem[i]->start, size);
			dip_dev.iomem[i] = NULL;
		}
#endif
	}

}

#ifdef CONFIG_OF
extern struct device odin_device_parent;

static struct of_device_id odindss_dip_match[] = {
	{
		.compatible = "odindss-dip",
	},
	{},
};
#endif
/* DIPC HW IP initialisation */
static int dip_probe(struct platform_device *pdev)
{
	int ret = 0;
#ifndef CONFIG_OF
	struct resource *res;
	int size = 0;
#endif
	int i = 0;

	DSSINFO("dip_probe\n");

#ifdef CONFIG_OF
	pdev->dev.parent = &odin_device_parent;
#endif
	memset(&dip_dev, 0x00, sizeof(dip_dev));

#if 0
	dip_dev.irq = platform_get_irq(pdev, 0);
	if (dip_dev.irq < 0) {
		DSSERR("no irq for device\n");
		ret = -ENOENT;
		goto err;
	}

	ret = request_irq(dip_dev.irq, dipc_irq_handler, 0, pdev->name, &dip_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't request IRQ for woden_dip_irq(%d)\n",
			dip_dev.irq);
		goto err;
	}
#endif

	spin_lock_init(&dip_dev.spinlock);
	init_completion(&dip_dev.completion);

#ifdef CONFIG_OF
	for (i=0 ; i<NUM_DIPC_BLOCK; i++) {
		dip_dev.base_dip[i] = of_iomap(pdev->dev.of_node, i);

		DSSDBG("remap dip base to 0x%08x\n",(unsigned int) dip_dev.base_dip[i]);
		if (dip_dev.base_dip[i] == NULL) {
			DSSERR("failed to remap io memory\n");
			ret = -ENXIO;
			goto free_region;
		}
		/* for debugging purpose*/
		dip_dev.dip[i] = (struct dip_register*)dip_dev.base_dip[i];
	}

#else
	for (i=0 ;i<NUM_DIPC_BLOCK; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res == NULL) {
			DSSERR("no resource for device\n");
			ret = -ENXIO;
			goto free_region;
		}

		size = res->end - res->start + 1;
		dip_dev.iomem[i] = request_mem_region(res->start, size, pdev->name);
		if (dip_dev.iomem[i] == NULL) {
			DSSERR("failed to request memory region\n");
			ret = -ENOENT;
			goto free_region;
		}

		dip_dev.base_dip[i] = ioremap(res->start, size);
		DSSDBG("remap dip base to 0x%08x\n",
				(unsigned int) dip_dev.base_dip[i]);
		if (dip_dev.base_dip[i] == NULL) {
			DSSERR("failed to remap io memory\n");
			ret = -ENXIO;
			goto free_region;
		}

		/* for debugging purpose*/
		dip_dev.dip[i] = (struct dip_register*)dip_dev.base_dip[i];
	}
#endif
	platform_set_drvdata(pdev, &dip_dev);
	dip_dev.pdev = pdev;
#if 0
err:
#endif
	return ret;
free_region:
	free_memory_region();
	return ret;
}

static int dip_remove(struct platform_device *pdev)
{
	free_memory_region();
	return 0;
}

static int dip_runtime_suspend(struct device *dev)
{
	return 0;
}

static int dip_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops dispc_pm_ops = {
	.runtime_suspend = dip_runtime_suspend,
	.runtime_resume = dip_runtime_resume,
};

static struct platform_driver odin_dip_driver = {
	.probe          = dip_probe,
	.remove         = dip_remove,
	.driver         = {
		.name   = "odindss-dip",
		.owner  = THIS_MODULE,
		.pm	= &dispc_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odindss_dip_match),
#endif
	},
};

int odin_dipc_init_platform_driver(void)
{
	DSSDBG("odin_dipc_init_platform_driver\n");

	return platform_driver_register(&odin_dip_driver);
}

void odin_dipc_uninit_platform_driver(void)
{
	return platform_driver_unregister(&odin_dip_driver);
}

