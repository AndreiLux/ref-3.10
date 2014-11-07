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

#ifndef __ODIN_CSS_CLK_H__
#define __ODIN_CSS_CLK_H__


/* MIPI CSI CLOCKS */
#define MIPI0_CLK		"css_mipi0_clk"
#define MIPI1_CKL		"css_mipi1_clk"
#define MIPI2_CKL		"css_mipi2_clk"
#define MIPI0_CFG_CLK	"mipi_cfg0_clk"
#define MIPI1_CFG_CKL	"mipi_cfg1_clk"
#define MIPI2_CFG_CKL	"mipi_cfg2_clk"

/* BUS CLOCKS */
#define AXI_CKL			"axi_clk"
#define AXI_CKL_R		"axi_clk_r"
#define AHB_CKL			"ahb_clk"
#define AHB_CKL_R		"ahb_clk_r"

/* TUNER CLOCKS */
#define TUNE_CKL		"tune_clk"
#define TUNE_CKL_2X		"tune_clkx2"

/* FaceDetect CLOCK */
#define FD_CKL			"fd_clk"
/* Facial Part Detect CLOCK
 * (Not avalible Now!!)
 */
#define FPD_CKL			"fpd_clk"

/* 3D CLOCKS */
#define S3C_CKL			"s3c_clk"
#define S3C0_CKL		"s3c0_clk"
#define S3C1_CKL		"s3c1_clk"

/* MDIS(VDIS) CLOCK */
#define MDIS_CKL		"mdis_clk"

/* ISP related CLOCKS */
#define ISP0_CKL		"v_isp0_clk"
#define ISP1_CKL		"v_isp1_clk"
#define PIX0_CKL		"pix0_clk"
#define PIX1_CKL		"pix1_clk"
#define ISP0_CKL_2X		"v_isp0_clkx2"

/* FrameGrabber CLOCK */
#define FRGB_CKL		"v_fg_clk"

/* VNR(3DNR) CLOCK */
#define VNR_CLK			"vnr_clk"

typedef enum
{
	CSS_CLK_MIPI0 = 0,
	CSS_CLK_MIPI1,
	CSS_CLK_MIPI2,
	CSS_CLK_BUS,
	CSS_CLK_BUS_R,
	CSS_CLK_TUNER,
	CSS_CLK_FD,
	CSS_CLK_FPD,
	CSS_CLK_S3DC,
	CSS_CLK_MDIS,
	CSS_CLK_ISP,
	CSS_CLK_ISP0,
	CSS_CLK_ISP1,
	CSS_CLK_FRGB,
	CSS_CLK_VNR,
	CSS_CLK_MAX,
} css_clks;


int css_clock_available(css_clks clk_idx);
int css_clock_enable(css_clks clk_idx);
int css_clock_disable(css_clks clk_idx);
unsigned long css_isp_clk_round_khz(unsigned int khz);
int css_set_isp_clock_khz(unsigned int khz);
void css_init_isp_clock(void);
int css_get_isp_clock_khz(void);
int css_get_isp_clock_khz_by_div(void);
#endif /* __ODIN_CSS_CLK_H__ */
