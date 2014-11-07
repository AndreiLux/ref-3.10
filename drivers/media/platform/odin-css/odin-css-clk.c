/*
 * Copyright (C) 2013 Mtekvision
 * Author: Daeheon Kim <kimdh@mtekvision.com>
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

#include <linux/clk-provider.h>

#include "odin-css.h"
#include "odin-css-system.h"
#include "odin-css-clk.h"


struct isp_clk_rates {
	unsigned long clk_rate;
};

static struct isp_clk_rates ispclks[] = {
	{ .clk_rate = 102000000 },
	{ .clk_rate = 204000000 },
	{ .clk_rate = 384000000 },
	{ .clk_rate = 408000000 },
	{ .clk_rate = 432000000 },
};

static inline int hw_clk_checker(css_clks clk)
{
	int ret = 0;
	unsigned int crg_clk = 0;
	crg_clk = css_crg_clk_info();
	switch (clk) {
	case CSS_CLK_MIPI0:
		if ((crg_clk & CRG_CLK_MIPI0) == 0) {
			css_err("check mipi0 clk: 0x%08x\n", clk);
			ret = CSS_FAILURE;
		}
		break;
	case CSS_CLK_MIPI1:
		if ((crg_clk & CRG_CLK_MIPI1) == 0) {
			css_err("check mipi1 clk: 0x%08x\n", clk);
			ret = CSS_FAILURE;
		}
		break;
	case CSS_CLK_MIPI2:
		if ((crg_clk & CRG_CLK_MIPI2) == 0) {
			css_err("check mipi2 clk: 0x%08x\n", clk);
			ret = CSS_FAILURE;
		}
		break;
	case CSS_CLK_FD:
		if ((crg_clk & CRG_CLK_FD) == 0) {
			css_err("check fd clk: 0x%08x\n", clk);
			ret = CSS_FAILURE;
		}
		break;
	case CSS_CLK_MDIS:
		if ((crg_clk & CRG_CLK_MDIS) == 0) {
			css_err("check mdis clk: 0x%08x\n", clk);
			ret = CSS_FAILURE;
		}
		break;
	case CSS_CLK_ISP:
		if (((crg_clk & CRG_CLK_ISP0) == 0) &&
			((crg_clk & CRG_CLK_ISP1) == 0)) {
			css_err("check isp clk: 0x%08x\n", clk);
			ret = CSS_FAILURE;
		}
		break;
	case CSS_CLK_ISP0:
		if ((crg_clk & CRG_CLK_ISP0) == 0) {
			css_err("check isp0 clk: 0x%08x\n", clk);
			ret = CSS_FAILURE;
		}
		break;
	case CSS_CLK_ISP1:
		if ((crg_clk & CRG_CLK_ISP1) == 0) {
			css_err("check isp1 clk: 0x%08x\n", clk);
			ret = CSS_FAILURE;
		}
		break;
	case CSS_CLK_FRGB:
		if ((crg_clk & CRG_CLK_FRGB) == 0) {
			css_err("check frgb clk: 0x%08x\n", clk);
			ret = CSS_FAILURE;
		}
		break;
	case CSS_CLK_VNR:
		if ((crg_clk & CRG_CLK_VNR) == 0) {
			css_err("check frgb clk: 0x%08x\n", clk);
			ret = CSS_FAILURE;
		}
		break;
	default:
		break;
	}

	return ret;
}

static inline void hw_clk_enable(char* clk_name)
{
	struct clk *clk;

	clk = clk_get(NULL, clk_name);

	if (!IS_ERR_OR_NULL(clk))
		clk_prepare_enable(clk);
}

static inline void hw_clk_disable(char* clk_name)
{
	struct clk *clk;

	clk = clk_get(NULL, clk_name);
	if (!IS_ERR_OR_NULL(clk) && __clk_get_enable_count(clk)) {
		clk_disable_unprepare(clk);
	}
}

static inline int hw_clock_available(char* clk_name)
{
	struct clk *clk;

	clk = clk_get(NULL, clk_name);
	if (!IS_ERR_OR_NULL(clk) && __clk_get_enable_count(clk)) {
		return 1;
	}
	return 0;
}

int css_clock_available(css_clks clk_idx)
{
	int ret = 0;

	switch (clk_idx) {
	case CSS_CLK_MIPI0:
		ret = hw_clock_available(MIPI0_CLK);
		break;
	case CSS_CLK_MIPI1:
		ret = hw_clock_available(MIPI1_CKL);
		break;
	case CSS_CLK_MIPI2:
		ret = hw_clock_available(MIPI2_CKL);
		break;
	case CSS_CLK_BUS:
		ret = hw_clock_available(AXI_CKL);
		break;
	case CSS_CLK_BUS_R:
		ret = hw_clock_available(AXI_CKL_R);
		break;
	case CSS_CLK_TUNER:
		ret = hw_clock_available(TUNE_CKL);
		break;
	case CSS_CLK_FD:
		ret = hw_clock_available(FD_CKL);
		break;
	case CSS_CLK_FPD:
		ret = 0;
		break;
	case CSS_CLK_S3DC:
		ret = hw_clock_available(S3C_CKL);
		break;
	case CSS_CLK_MDIS:
		ret = hw_clock_available(MDIS_CKL);
		break;
	case CSS_CLK_ISP:
		if (hw_clock_available(ISP0_CKL) && hw_clock_available(ISP1_CKL))
			ret = 1;
		break;
	case CSS_CLK_ISP0:
		ret = hw_clock_available(ISP0_CKL);
		break;
	case CSS_CLK_ISP1:
		ret = hw_clock_available(ISP1_CKL);
		break;
	case CSS_CLK_FRGB:
		ret = hw_clock_available(FRGB_CKL);
		break;
	case CSS_CLK_VNR:
		ret = hw_clock_available(VNR_CLK);
		break;
	default:
		break;
	}
	return ret;
}

int css_clock_enable(css_clks clk_idx)
{
	int ret = 0;

	switch (clk_idx) {
	case CSS_CLK_MIPI0:
		hw_clk_enable(MIPI0_CLK);
		hw_clk_enable(MIPI0_CFG_CLK);
		hw_clk_checker(CSS_CLK_MIPI0);
		css_sys("CSS clk:mipi0 enable\n");
		break;
	case CSS_CLK_MIPI1:
		hw_clk_enable(MIPI1_CKL);
		hw_clk_enable(MIPI1_CFG_CKL);
		hw_clk_checker(CSS_CLK_MIPI1);
		css_sys("CSS clk:mipi1 enable\n");
		break;
	case CSS_CLK_MIPI2:
		hw_clk_enable(MIPI2_CKL);
		hw_clk_enable(MIPI2_CFG_CKL);
		hw_clk_checker(CSS_CLK_MIPI2);
		css_sys("CSS clk:mipi2 enable\n");
		break;
	case CSS_CLK_BUS:
		hw_clk_enable(AXI_CKL);
		hw_clk_enable(AHB_CKL);
		css_sys("CSS clk:bus enable\n");
		break;
	case CSS_CLK_BUS_R:
		hw_clk_enable(AXI_CKL_R);
		hw_clk_enable(AHB_CKL_R);
		css_sys("CSS clk:bus_r enable\n");
		break;
	case CSS_CLK_TUNER:
		hw_clk_enable(TUNE_CKL);
		hw_clk_enable(TUNE_CKL_2X);
		css_sys("CSS clk:tuner enable\n");
		break;
	case CSS_CLK_FD:
		hw_clk_enable(FD_CKL);
		hw_clk_checker(CSS_CLK_FD);
		css_sys("CSS clk:facedetect enable\n");
		break;
	case CSS_CLK_FPD:
		/* hw_clk_enable(FPD_CKL); */
		css_sys("CSS clk:fpd enable\n");
		break;
	case CSS_CLK_S3DC:
		hw_clk_enable(S3C_CKL);
		hw_clk_enable(S3C0_CKL);
		hw_clk_enable(S3C1_CKL);
		css_sys("CSS clk:s3dc enable\n");
		break;
	case CSS_CLK_MDIS:
		hw_clk_enable(MDIS_CKL);
		hw_clk_checker(CSS_CLK_MDIS);
		css_sys("CSS clk:mdis enable\n");
		break;
	case CSS_CLK_ISP:
		hw_clk_enable(ISP0_CKL);
		hw_clk_enable(PIX0_CKL);
		hw_clk_enable(ISP1_CKL);
		hw_clk_enable(PIX1_CKL);
		hw_clk_checker(CSS_CLK_ISP);
		css_sys("CSS clk:ISP all enable\n");
		break;
	case CSS_CLK_ISP0:
		hw_clk_enable(ISP0_CKL);
		hw_clk_enable(PIX0_CKL);
		hw_clk_checker(CSS_CLK_ISP0);
		css_sys("CSS clk:ISP0 enable\n");
		break;
	case CSS_CLK_ISP1:
		hw_clk_enable(ISP1_CKL);
		hw_clk_enable(PIX1_CKL);
		hw_clk_checker(CSS_CLK_ISP1);
		css_sys("CSS clk:ISP1 enable\n");
		break;
	case CSS_CLK_FRGB:
		hw_clk_enable(FRGB_CKL);
		hw_clk_checker(CSS_CLK_FRGB);
		css_sys("CSS clk:frgb enable\n");
		break;
	case CSS_CLK_VNR:
		hw_clk_enable(VNR_CLK);
		hw_clk_checker(CSS_CLK_VNR);
		css_sys("CSS clk:vnr enable\n");
		break;
	default:
		break;
	}

	return ret;
}

int css_clock_disable(css_clks clk_idx)
{
	int ret = 0;

	switch (clk_idx) {
	case CSS_CLK_MIPI0:
		hw_clk_disable(MIPI0_CLK);
		hw_clk_disable(MIPI0_CFG_CLK);
		css_sys("CSS clk:mipi0 disable\n");
		break;
	case CSS_CLK_MIPI1:
		hw_clk_disable(MIPI1_CKL);
		hw_clk_disable(MIPI1_CFG_CKL);
		css_sys("CSS clk:mipi1 disable\n");
		break;
	case CSS_CLK_MIPI2:
		hw_clk_disable(MIPI2_CKL);
		hw_clk_disable(MIPI2_CFG_CKL);
		css_sys("CSS clk:mipi2 disable\n");
		break;
	case CSS_CLK_BUS:
		hw_clk_disable(AXI_CKL);
		hw_clk_disable(AHB_CKL);
		css_sys("CSS clk:bus disable\n");
		break;
	case CSS_CLK_BUS_R:
		hw_clk_disable(AXI_CKL_R);
		hw_clk_disable(AHB_CKL_R);
		css_sys("CSS clk:bus_r disable\n");
		break;
	case CSS_CLK_TUNER:
		hw_clk_disable(TUNE_CKL);
		hw_clk_disable(TUNE_CKL_2X);
		css_sys("CSS clk:tuner disable\n");
		break;
	case CSS_CLK_FD:
		hw_clk_disable(FD_CKL);
		css_sys("CSS clk:facedetect disable\n");
		break;
	case CSS_CLK_FPD:
		/* hw_clk_disable(FPD_CKL); */
		css_sys("CSS clk:fpd disable\n");
		break;
	case CSS_CLK_S3DC:
		hw_clk_disable(S3C_CKL);
		hw_clk_disable(S3C0_CKL);
		hw_clk_disable(S3C1_CKL);
		css_sys("CSS clk:s3dc disable\n");
		break;
	case CSS_CLK_MDIS:
		hw_clk_disable(MDIS_CKL);
		css_sys("CSS clk:mdis disable\n");
		break;
	case CSS_CLK_ISP:
		hw_clk_disable(ISP0_CKL);
		hw_clk_disable(PIX0_CKL);
		hw_clk_disable(ISP1_CKL);
		hw_clk_disable(PIX1_CKL);
		css_sys("CSS clk:ISP all disable\n");
		break;
	case CSS_CLK_ISP0:
		hw_clk_disable(ISP0_CKL);
		hw_clk_disable(PIX0_CKL);
		css_sys("CSS clk:ISP0 disable\n");
		break;
	case CSS_CLK_ISP1:
		hw_clk_disable(ISP1_CKL);
		hw_clk_disable(PIX1_CKL);
		css_sys("CSS clk:ISP1 disable\n");
		break;
	case CSS_CLK_FRGB:
		hw_clk_disable(FRGB_CKL);
		css_sys("CSS clk:frgb disable\n");
		break;
	case CSS_CLK_VNR:
		hw_clk_disable(VNR_CLK);
		css_sys("CSS clk:vnr disable\n");
		break;
	default:
		break;
	}

	return ret;
}

unsigned long css_isp_clk_round_khz(unsigned int khz)
{
	unsigned long round_clk = 0;
	unsigned long in_hz = khz * 1000;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(ispclks); i++) {
		if (ispclks[i].clk_rate >= in_hz) {
			round_clk = ispclks[i].clk_rate;
			break;
		}
	}

	return round_clk/1000;
}

int css_set_isp_clock_khz(unsigned int khz)
{
	struct clk *clk;
	int ret = 0;
	unsigned long round_clk_khz = 0;
	ktime_t delta, stime, etime;
	unsigned long long duration;

	stime = ktime_get();

	clk = clk_get(NULL, "v_isp0_clk");
	if (!IS_ERR(clk)) {
		unsigned long isp_clk_rate = 0;
		round_clk_khz = css_isp_clk_round_khz(khz);
		if (clk_set_rate(clk, round_clk_khz * 1000)) {
			css_err("clk_set_rate fail\n");
			ret = -1;
		} else {
			etime = ktime_get();
			delta = ktime_sub(etime, stime);
			duration = (unsigned long long) ktime_to_ns(delta) >> 10;
			isp_clk_rate = (unsigned long)(clk_get_rate(clk));
			css_info("[ISP] %ld:%ld(MHz): %llu\n",
					round_clk_khz/1000, isp_clk_rate/1000000, duration);
		}
	} else {
		css_err("clk_get error\n");
		ret = PTR_ERR(clk);
	}

	return ret;
}


void css_init_isp_clock(void)
{
	struct css_device *cssdev = NULL;
	cssdev = get_css_plat();
	if (cssdev) {
		cssdev->ispclk.clk_hz[0]	= MINIMUM_ISP_PERFORMANCE;
		cssdev->ispclk.clk_hz[1]	= MINIMUM_ISP_PERFORMANCE;
		cssdev->ispclk.set_clk_hz	= 0;
		cssdev->ispclk.cur_clk_hz	= 0;
	}
}

int css_get_isp_clock_khz(void)
{
	struct clk *clk;
	int ret = 0;

	clk = clk_get(NULL, "v_isp0_clk");
	if (!IS_ERR(clk)) {
		unsigned int isp_clk_rate = 0;
		isp_clk_rate = (unsigned int)(clk_get_rate(clk));
		css_sys("[ISP] %d(KHz)\n", isp_clk_rate / 1000);
		ret = isp_clk_rate / 1000;
	} else {
		css_err("clk_get error\n");
		ret = PTR_ERR(clk);
	}

	return ret;
}

int css_get_isp_clock_khz_by_div(void)
{
	unsigned int div = 0;
	int clk = 0;

	clk = css_get_isp_clock_khz();
	div = css_get_isp_divider();
	switch (div) {
		case 0x1:
		clk = clk/2;
		break;
		case 0x2:
		clk = clk/4;
		break;
		case 0x4:
		clk = clk/8;
		break;
		case 0x8:
		clk = clk/16;
		break;
	}
	return clk;
}
