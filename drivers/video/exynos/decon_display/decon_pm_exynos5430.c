/* linux/drivers/video/decon_display/decon_pm_exynos5430.c
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/clk-private.h>
#include <linux/sec_batt.h>

#include <linux/platform_device.h>
#include <mach/map.h>

#if defined(CONFIG_SOC_EXYNOS5433)
#include <mach/regs-clock-exynos5433.h>
#else
#include <mach/regs-clock-exynos5430.h>
#endif

#include <mach/regs-pmu.h>

#include "regs-decon.h"
#include "decon_display_driver.h"
#include "decon_fb.h"
#include "decon_mipi_dsi.h"
#include "decon_dt.h"

#if defined(CONFIG_DISP_CLK_DEBUG)
struct cmu_list_t {
	unsigned int	pa;
	void __iomem	*va;
};

#define EXYNOS543X_CMU_LIST(name)	\
	{EXYNOS5430_PA_CMU_##name,	EXYNOS5430_VA_CMU_##name}

struct cmu_list_t cmu_list[] = {
	EXYNOS543X_CMU_LIST(TOP),
	EXYNOS543X_CMU_LIST(EGL),
	EXYNOS543X_CMU_LIST(KFC),
	EXYNOS543X_CMU_LIST(AUD),
	EXYNOS543X_CMU_LIST(BUS1),
	EXYNOS543X_CMU_LIST(BUS2),
	EXYNOS543X_CMU_LIST(CAM0),
	EXYNOS543X_CMU_LIST(CAM0_LOCAL),
	EXYNOS543X_CMU_LIST(CAM1),
	EXYNOS543X_CMU_LIST(CAM1_LOCAL),
	EXYNOS543X_CMU_LIST(CPIF),
	EXYNOS543X_CMU_LIST(DISP),
	EXYNOS543X_CMU_LIST(FSYS),
	EXYNOS543X_CMU_LIST(G2D),
	EXYNOS543X_CMU_LIST(G3D),
	EXYNOS543X_CMU_LIST(GSCL),
	EXYNOS543X_CMU_LIST(HEVC),
	EXYNOS543X_CMU_LIST(IMEM),
	EXYNOS543X_CMU_LIST(ISP),
	EXYNOS543X_CMU_LIST(ISP_LOCAL),
	EXYNOS543X_CMU_LIST(MFC0),
	EXYNOS543X_CMU_LIST(MFC1),
	EXYNOS543X_CMU_LIST(MIF),
	EXYNOS543X_CMU_LIST(MSCL),
	EXYNOS543X_CMU_LIST(PERIC),
	EXYNOS543X_CMU_LIST(PERIS),
};

unsigned int get_pa(void __iomem *va)
{
	unsigned int i, reg;

	reg = (unsigned int)va;

	for (i = 0; i < ARRAY_SIZE(cmu_list); i++) {
		if ((reg & 0xfffff000) == (unsigned int)cmu_list[i].va)
			break;
	}

	i = (i >= ARRAY_SIZE(cmu_list)) ? 0 : cmu_list[i].pa;

	i += (reg & 0xfff);

	return i;
}
#endif

struct clk_list_t {
	struct clk *c;
	struct clk *p;
	const char *c_name;
	const char *p_name;
};

/* this clk eum value is DIFFERENT with clk-exynos543x.c */
enum disp_clks {
	disp_pll,
	mout_aclk_disp_333_a,
	mout_aclk_disp_333_b,
	dout_pclk_disp,
	mout_aclk_disp_333_user,
	dout_sclk_dsd,
	mout_sclk_dsd_c,
	mout_sclk_dsd_b,
	mout_sclk_dsd_a,
	mout_sclk_dsd_user,
	dout_sclk_decon_eclk,
	mout_sclk_decon_eclk_c,
	mout_sclk_decon_eclk_b,
	mout_sclk_decon_eclk_a,
	dout_sclk_decon_eclk_disp,
	mout_sclk_decon_eclk,
	mout_sclk_decon_eclk_user,
	dout_sclk_decon_vclk,
	mout_sclk_decon_vclk_c,
	mout_sclk_decon_vclk_b,
	mout_sclk_decon_vclk_a,
	dout_sclk_decon_vclk_disp,
	mout_sclk_decon_vclk,
	mout_disp_pll,
	dout_sclk_dsim0,
	mout_sclk_dsim0_c,
	mout_sclk_dsim0_b,
	mout_sclk_dsim0_a,
	dout_sclk_dsim0_disp,
	mout_sclk_dsim0,
	mout_sclk_dsim0_user,
	mout_phyclk_mipidphy_rxclkesc0_user,
	mout_phyclk_mipidphy_bitclkdiv8_user,
	disp_clks_max,
};

static struct clk_list_t clk_list[disp_clks_max] = {
	{ .c_name = "disp_pll", },
	{ .c_name = "mout_aclk_disp_333_a", },
	{ .c_name = "mout_aclk_disp_333_b", },
	{ .c_name = "dout_pclk_disp", },
	{ .c_name = "mout_aclk_disp_333_user", },
	{ .c_name = "dout_sclk_dsd", },
	{ .c_name = "mout_sclk_dsd_c", },
	{ .c_name = "mout_sclk_dsd_b", },
	{ .c_name = "mout_sclk_dsd_a", },
	{ .c_name = "mout_sclk_dsd_user", },
	{ .c_name = "dout_sclk_decon_eclk", },
	{ .c_name = "mout_sclk_decon_eclk_c", },
	{ .c_name = "mout_sclk_decon_eclk_b", },
	{ .c_name = "mout_sclk_decon_eclk_a", },
	{ .c_name = "dout_sclk_decon_eclk_disp", },
	{ .c_name = "mout_sclk_decon_eclk", },
	{ .c_name = "mout_sclk_decon_eclk_user", },
	{ .c_name = "dout_sclk_decon_vclk", },
	{ .c_name = "mout_sclk_decon_vclk_c", },
	{ .c_name = "mout_sclk_decon_vclk_b", },
	{ .c_name = "mout_sclk_decon_vclk_a", },
	{ .c_name = "dout_sclk_decon_vclk_disp", },
	{ .c_name = "mout_sclk_decon_vclk", },
	{ .c_name = "mout_disp_pll", },
	{ .c_name = "dout_sclk_dsim0", },
	{ .c_name = "mout_sclk_dsim0_c", },
	{ .c_name = "mout_sclk_dsim0_b", },
	{ .c_name = "mout_sclk_dsim0_a", },
	{ .c_name = "dout_sclk_dsim0_disp", },
	{ .c_name = "mout_sclk_dsim0", },
	{ .c_name = "mout_sclk_dsim0_user", },
	{ .c_name = "mout_phyclk_mipidphy_rxclkesc0_user", },
	{ .c_name = "mout_phyclk_mipidphy_bitclkdiv8_user", },
};

static int exynos_display_clk_get(struct device *dev, enum disp_clks idx, const char *parent)
{
	struct clk *p;
	struct clk *c;
	int ret = 0;
	const char *conid = clk_list[idx].c_name;

	if (IS_ERR_OR_NULL(clk_list[idx].c)) {
		c = clk_get(dev, conid);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("%s: can't get clock: %s\n", __func__, conid);
			return -EINVAL;
		} else
			clk_list[idx].c = c;
	}

	if (IS_ERR_OR_NULL(parent)) {
		if (IS_ERR_OR_NULL(clk_list[idx].p)) {
			p = clk_get_parent(clk_list[idx].c);
			if (IS_ERR_OR_NULL(p)) {
				pr_err("%s: can't get clock parent: %s\n", __func__, conid);
				return -EINVAL;
			} else
				clk_list[idx].p = p;
		}
	} else {
		if (IS_ERR_OR_NULL(clk_list[idx].p)) {
			p = clk_get(dev, parent);
			if (IS_ERR_OR_NULL(p)) {
				pr_err("%s: can't get clock: %s\n", __func__, parent);
				return -EINVAL;
			} else
				clk_list[idx].p = p;
		}
	}

	return ret;
}

static int exynos_display_set_parent(struct device *dev, enum disp_clks idx, const char *parent)
{
	struct clk *p;
	struct clk *c;
	int ret = 0;
	const char *conid = clk_list[idx].c_name;

	if (unlikely(IS_ERR_OR_NULL(clk_list[idx].c))) {
		ret = exynos_display_clk_get(dev, idx, parent);
		if (ret < 0) {
			pr_err("%s: can't get clock: %s\n", __func__, conid);
			return ret;
		}
	}

	p = clk_list[idx].p;
	c = clk_list[idx].c;

	ret = clk_set_parent(c, p);
	if (ret < 0)
		pr_info("failed %s: %s, %s, %d\n", __func__, conid, parent, ret);
#if defined(CONFIG_DISP_CLK_DEBUG)
	else {
		struct clk_mux *mux = container_of(c->hw, struct clk_mux, hw);
		unsigned int val = readl(mux->reg) >> mux->shift;
		val &= mux->mask;
		pr_info("%08X[%2d], %8d, 0x%08x, %30s, %30s\n",
			get_pa(mux->reg), mux->shift, val, readl(mux->reg), conid, parent);
	}
#endif

	return ret;
}

static int exynos_display_set_divide(struct device *dev, enum disp_clks idx, unsigned int divider)
{
	struct clk *p;
	struct clk *c;
	unsigned long rate;
	int ret = 0;
	const char *conid = clk_list[idx].c_name;

	if (unlikely(IS_ERR_OR_NULL(clk_list[idx].c))) {
		ret = exynos_display_clk_get(dev, idx, NULL);
		if (ret < 0) {
			pr_err("%s: can't get clock: %s\n", __func__, conid);
			return ret;
		}
	}

	p = clk_list[idx].p;
	c = clk_list[idx].c;

	rate = DIV_ROUND_UP(clk_get_rate(p), (divider + 1));

	ret = clk_set_rate(c, rate);
	if (ret < 0)
		pr_info("failed to %s, %s, %d\n", __func__, conid, ret);
#if defined(CONFIG_DISP_CLK_DEBUG)
	else {
		struct clk_divider *div = container_of(c->hw, struct clk_divider, hw);
		unsigned int val = readl(div->reg) >> div->shift;
		val &= (BIT(div->width) - 1);
		WARN_ON(divider != val);
		pr_info("%08X[%2d], %8d, 0x%08x, %30s, %30s\n",
			get_pa(div->reg), div->shift, val, readl(div->reg), conid, __clk_get_name(p));
	}
#endif

	return ret;
}

static int exynos_display_set_rate(struct device *dev, enum disp_clks idx, unsigned long rate)
{
	struct clk *p;
	struct clk *c;
	int ret = 0;
	const char *conid = clk_list[idx].c_name;

	if (unlikely(IS_ERR_OR_NULL(clk_list[idx].c))) {
		ret = exynos_display_clk_get(dev, idx, NULL);
		if (ret < 0) {
			pr_err("%s: can't get clock: %s\n", __func__, conid);
			return ret;
		}
	}

	p = clk_list[idx].p;
	c = clk_list[idx].c;

	ret = clk_set_rate(c, rate);
	if (ret < 0)
		pr_info("failed to %s, %s, %d\n", __func__, conid, ret);
#if defined(CONFIG_DISP_CLK_DEBUG)
	else {
		struct clk_divider *div = container_of(c->hw, struct clk_divider, hw);
		pr_info("%08X[%2s], %8s, 0x%08x, %30s, %30s\n",
			get_pa(div->reg), "", "", readl(div->reg), conid, __clk_get_name(p));
	}
#endif

	return ret;
}

int init_display_decon_clocks(struct device *dev)
{
	int ret = 0;
	struct decon_lcd *lcd = decon_get_lcd_info();
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	struct display_driver *dispdrv = get_display_driver();
#endif

	if (lcd->xres * lcd->yres == 720 * 1280)
		exynos_display_set_rate(dev, disp_pll, 67 * MHZ);
	else if (lcd->xres * lcd->yres == 1080 * 1920)
		exynos_display_set_rate(dev, disp_pll, 142 * MHZ);
	else if (lcd->xres * lcd->yres == 1536 * 2048)
		exynos_display_set_rate(dev, disp_pll, 214 * MHZ);
	else if (lcd->xres * lcd->yres == 1440 * 2560)
		exynos_display_set_rate(dev, disp_pll, 250 * MHZ);
	else if (lcd->xres * lcd->yres == 2560 * 1600)
		exynos_display_set_rate(dev, disp_pll, 278 * MHZ);
	else
		dev_err(dev, "%s: resolution %d:%d is missing\n", __func__, lcd->xres, lcd->yres);

	exynos_display_set_parent(dev, mout_aclk_disp_333_a, "mout_mfc_pll_div2");
	exynos_display_set_parent(dev, mout_aclk_disp_333_b, "mout_aclk_disp_333_a");
	exynos_display_set_divide(dev, dout_pclk_disp, 2);
	exynos_display_set_parent(dev, mout_aclk_disp_333_user, "aclk_disp_333");

	exynos_display_set_divide(dev, dout_sclk_dsd, 1);
	exynos_display_set_parent(dev, mout_sclk_dsd_c, "mout_sclk_dsd_b");
	exynos_display_set_parent(dev, mout_sclk_dsd_b, "mout_sclk_dsd_a");
	exynos_display_set_parent(dev, mout_sclk_dsd_a, "mout_mfc_pll_div2");
	exynos_display_set_parent(dev, mout_sclk_dsd_user, "sclk_dsd_disp");

	if (lcd->xres * lcd->yres == 720 * 1280) {
		exynos_display_set_divide(dev, dout_sclk_decon_eclk, 10);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_c, "mout_sclk_decon_eclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_b, "mout_sclk_decon_eclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_a, "mout_bus_pll_div2");
		exynos_display_set_divide(dev, dout_sclk_decon_eclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk, "mout_sclk_decon_eclk_user");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_user, "sclk_decon_eclk_disp");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_c, "mout_sclk_decon_vclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_b, "mout_sclk_decon_vclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_a, "oscclk");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk, "mout_disp_pll");
		exynos_display_set_parent(dev, mout_disp_pll, "disp_pll");
		exynos_display_set_divide(dev, dout_sclk_dsim0, 10);
		exynos_display_set_parent(dev, mout_sclk_dsim0_c, "mout_sclk_dsim0_b");
		exynos_display_set_parent(dev, mout_sclk_dsim0_b, "mout_sclk_dsim0_a");
		exynos_display_set_parent(dev, mout_sclk_dsim0_a, "mout_bus_pll_div2");
		exynos_display_set_divide(dev, dout_sclk_dsim0_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_dsim0, "mout_sclk_dsim0_user");
		exynos_display_set_parent(dev, mout_sclk_dsim0_user, "sclk_dsim0_disp");
	} else if (lcd->xres * lcd->yres == 1080 * 1920) {
		exynos_display_set_divide(dev, dout_sclk_decon_eclk, 4);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_c, "mout_sclk_decon_eclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_b, "mout_sclk_decon_eclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_a, "mout_bus_pll_div2");
		exynos_display_set_divide(dev, dout_sclk_decon_eclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk, "mout_sclk_decon_eclk_user");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_user, "sclk_decon_eclk_disp");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_c, "mout_sclk_decon_vclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_b, "mout_sclk_decon_vclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_a, "oscclk");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk, "mout_disp_pll");
		exynos_display_set_parent(dev, mout_disp_pll, "disp_pll");
		exynos_display_set_divide(dev, dout_sclk_dsim0, 4);
		exynos_display_set_parent(dev, mout_sclk_dsim0_c, "mout_sclk_dsim0_b");
		exynos_display_set_parent(dev, mout_sclk_dsim0_b, "mout_sclk_dsim0_a");
		exynos_display_set_parent(dev, mout_sclk_dsim0_a, "mout_bus_pll_div2");
		exynos_display_set_divide(dev, dout_sclk_dsim0_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_dsim0, "mout_sclk_dsim0_user");
		exynos_display_set_parent(dev, mout_sclk_dsim0_user, "sclk_dsim0_disp");
	} else if (lcd->xres * lcd->yres == 1440 * 2560 || lcd->xres * lcd->yres == 1536 * 2048) {
		exynos_display_set_divide(dev, dout_sclk_decon_eclk, 2);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_c, "mout_sclk_decon_eclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_b, "mout_sclk_decon_eclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_a, "mout_bus_pll_div2");
		exynos_display_set_divide(dev, dout_sclk_decon_eclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk, "mout_sclk_decon_eclk_user");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_user, "sclk_decon_eclk_disp");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_c, "mout_sclk_decon_vclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_b, "mout_sclk_decon_vclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_a, "oscclk");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk, "mout_disp_pll");
		exynos_display_set_parent(dev, mout_disp_pll, "disp_pll");
		exynos_display_set_divide(dev, dout_sclk_dsim0, 2);
		exynos_display_set_parent(dev, mout_sclk_dsim0_c, "mout_sclk_dsim0_b");
		exynos_display_set_parent(dev, mout_sclk_dsim0_b, "mout_sclk_dsim0_a");
		exynos_display_set_parent(dev, mout_sclk_dsim0_a, "mout_bus_pll_div2");
		exynos_display_set_divide(dev, dout_sclk_dsim0_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_dsim0, "mout_sclk_dsim0_user");
		exynos_display_set_parent(dev, mout_sclk_dsim0_user, "sclk_dsim0_disp");
	} else if (lcd->xres * lcd->yres == 2560 * 1600) {
		exynos_display_set_divide(dev, dout_sclk_decon_eclk, 1);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_c, "mout_sclk_decon_eclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_b, "mout_mfc_pll_div2");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_a, "oscclk");
		exynos_display_set_divide(dev, dout_sclk_decon_eclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk, "mout_sclk_decon_eclk_user");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_user, "sclk_decon_eclk_disp");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_c, "mout_sclk_decon_vclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_b, "mout_sclk_decon_vclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_a, "oscclk");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk, "mout_disp_pll");
		exynos_display_set_parent(dev, mout_disp_pll, "disp_pll");
		exynos_display_set_divide(dev, dout_sclk_dsim0, 1);
		exynos_display_set_parent(dev, mout_sclk_dsim0_c, "mout_sclk_dsim0_b");
		exynos_display_set_parent(dev, mout_sclk_dsim0_b, "mout_mfc_pll_div2");
		exynos_display_set_parent(dev, mout_sclk_dsim0_a, "oscclk");
		exynos_display_set_divide(dev, dout_sclk_dsim0_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_dsim0, "mout_sclk_dsim0_user");
		exynos_display_set_parent(dev, mout_sclk_dsim0_user, "sclk_dsim0_disp");
	} else
		dev_err(dev, "%s: resolution %d:%d is missing\n", __func__, lcd->xres, lcd->yres);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	dispdrv->pm_status.ops->clk_on(dispdrv);
#endif

	return ret;
}

int init_display_dsi_clocks(struct device *dev)
{
	int ret = 0;

	exynos_display_set_parent(dev, mout_phyclk_mipidphy_rxclkesc0_user, "phyclk_mipidphy_rxclkesc0_phy");
	exynos_display_set_parent(dev, mout_phyclk_mipidphy_bitclkdiv8_user, "phyclk_mipidphy_bitclkdiv8_phy");

	return ret;
}

int enable_display_decon_clocks(struct device *dev)
{
	return 0;
}

int disable_display_decon_clocks(struct device *dev)
{
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	struct display_driver *dispdrv;
	dispdrv = get_display_driver();

	dispdrv->pm_status.ops->clk_off(dispdrv);
#endif

	return 0;
}

/* should be moved to dt file */
static struct regulator_bulk_data *supplies;
static int num_supplies;

static void get_supplies(struct device *dev)
{
	int ret = 0;
	unsigned int i;

	num_supplies = of_property_count_strings(dev->of_node, "display,regulator-names");

	if (num_supplies <= 0)
		return;

	supplies = kzalloc(num_supplies * sizeof(struct regulator_bulk_data), GFP_KERNEL);

	for (i = 0; i < num_supplies; i++) {
		of_property_read_string_index(dev->of_node, "display,regulator-names", i, &supplies[i].supply);
		pr_info("%s: %s\n", __func__, supplies[i].supply);
	}

	if (num_supplies) {
		ret = regulator_bulk_get(NULL, num_supplies, supplies);
		if (ret < 0)
			pr_err("%s: failed to get regulators: %d\n", __func__, ret);
	}
}

static struct pinctrl		*pins;
static struct pinctrl_state	*state_default;
#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
static struct pinctrl_state	*state_turnon_tes;
static struct pinctrl_state	*state_turnoff_tes;
#endif

static void get_pintctrl(struct device *dev)
{
	if (!pins) {
		pins = devm_pinctrl_get(dev);
		if (!pins)
			pr_err("%s: failed to get pinctrl\n", __func__);
	}

	if (!state_default) {
		state_default = pinctrl_lookup_state(pins, "default");
		if (!state_default)
			pr_err("%s: failed to get default pinctrl\n", __func__);
	}

#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
	if (!state_turnon_tes) {
		state_turnon_tes = pinctrl_lookup_state(pins, "turnon_tes");
		if (!state_turnon_tes)
			pr_err("%s: failed to get default pinctrl\n", __func__);
	}

	if (!state_turnoff_tes) {
		state_turnoff_tes = pinctrl_lookup_state(pins, "turnoff_tes");
		if (!state_turnoff_tes)
			pr_err("%s: failed to get default pinctrl\n", __func__);
	}
#endif
}

static int set_pinctrl(struct device *dev, int enable)
{
	int ret = 0;

	if (enable) {
		if (!pins)
			get_pintctrl(dev);

		if (!IS_ERR(pins) && !IS_ERR(state_default)) {
			ret = pinctrl_select_state(pins, state_default);
			if (ret) {
				pr_err("%s: failed to select state for default", __func__);
				return ret;
			}
		}

#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
		if (!IS_ERR(pins) && !IS_ERR(state_turnon_tes)) {
			ret = pinctrl_select_state(pins, state_turnon_tes);
			if (ret) {
				pr_err("%s: failed to select state for turn on tes", __func__);
				return ret;
			}
		}
#endif
	} else {
#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
		if (!IS_ERR(pins) && !IS_ERR(state_turnoff_tes)) {
			ret = pinctrl_select_state(pins, state_turnoff_tes);
			if (ret) {
				pr_err("%s: failed to select state for turn off tes", __func__);
				return ret;
			}
		}
#endif
	}

	return ret;
}

#if defined(CONFIG_DECON_LCD_S6TNMR7)
int enable_display_driver_power(struct device *dev)
{
	struct display_driver *dispdrv = get_display_driver();
	struct display_gpio *gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio();
	int ret = 0;

	if (!num_supplies)
		get_supplies(dev);

	if (!num_supplies)
		goto pin_config;

	/* Turn on VDD_3.3V*/
	ret = gpio_request_one(gpio->id[0], GPIOF_OUT_INIT_HIGH, "lcd_power");
	if (ret < 0) {
		pr_err("%s: gpio lcd_power request fail\n", __func__);
		goto pin_config;
	}
	gpio_free(gpio->id[0]);

	usleep_range(10000, 12000);

	/* Turn on VDD_1.8V(TCON_1.8v) */
	ret = regulator_enable(supplies[0].consumer);
	if (ret < 0) {
		pr_err("%s: regulator %s enable fail\n", __func__, supplies[0].supply);
		goto pin_config;
	}

pin_config:
	set_pinctrl(dev, 1);

	return 0;
}

int disable_display_driver_power(struct device *dev)
{
	struct display_driver *dispdrv;
	struct display_gpio *gpio;
	int ret;

	dispdrv = get_display_driver();
	gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio();

	if (!num_supplies)
		get_supplies(dev);

	if (!num_supplies)
		goto pin_config;

	/* Turn on VDD_1.8V(TCON_1.8v) */
	ret = regulator_disable(supplies[0].consumer);
	if (ret < 0) {
		pr_err("%s: regulator %s enable fail\n", __func__, supplies[0].supply);
		goto pin_config;
	}
	usleep_range(5000, 10000);
	/* Turn off VDD_3.3V*/
	ret = gpio_request_one(gpio->id[0], GPIOF_OUT_INIT_LOW, "lcd_power");
	if (ret < 0) {
		pr_err("%s: gpio lcd_power request fail\n", __func__);
		goto pin_config;
	}
	gpio_free(gpio->id[0]);

pin_config:
	set_pinctrl(dev, 0);

	return 0;
}

int reset_display_driver_panel(struct device *dev)
{
	int timeout = 10;
	int gpio;

	pr_debug(" Chagall %s\n", __func__);

	gpio = of_get_named_gpio(dev->of_node, "oled-pcd-gpio", 0);
	if (gpio < 0) {
		dev_err(dev, "failed to get proper gpio number\n");
		return -EINVAL;
	}
	msleep_interruptible(150);
	do {
		if (gpio_get_value(gpio))
			break;
		msleep(30);
	} while (timeout--);
	if (timeout < 0)
		pr_err(" %s timeout...\n", __func__);
	else
		pr_info("%s duration: %d\n", __func__, 150+(10-timeout)*30);
	return 0;
}
#elif defined(CONFIG_DECON_LCD_S6E3HA1)
int enable_display_driver_power(struct device *dev)
{
	int ret = 0;

	if (!num_supplies)
		get_supplies(dev);

	if (!num_supplies)
		goto pin_config;

	/* Turn on VDDI(VDD3): 1.8 */
	ret = regulator_enable(supplies[0].consumer);
	if (ret < 0) {
		pr_err("%s: regulator %s enable fail\n", __func__, supplies[0].supply);
		goto pin_config;
	}

	/* Wait 5ms */
	usleep_range(5000, 6000);

	/* Turn on VCI: 3.0 */
	ret = regulator_enable(supplies[1].consumer);
	if (ret < 0) {
		pr_err("%s: regulator %s enable fail\n", __func__, supplies[1].supply);
		goto pin_config;
	}

	/* Turn on VDDR(VDDI_REG): 1.5 or 1.6 */
	ret = regulator_enable(supplies[2].consumer);
	if (ret < 0) {
		pr_err("%s: regulator %s enable fail\n", __func__, supplies[2].supply);
		goto pin_config;
	}

	/* Wait 10ms */
	usleep_range(10000, 11000);

pin_config:
	set_pinctrl(dev, 1);

	return 0;
}

int disable_display_driver_power(struct device *dev)
{
	struct display_driver *dispdrv;
	struct display_gpio *gpio;
	int ret;

	dispdrv = get_display_driver();
	gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio();

	ret = gpio_request_one(gpio->id[0], GPIOF_OUT_INIT_LOW, "lcd_reset");
	if (ret < 0) {
		pr_err("Failed to get gpio number for the lcd_reset\n");
		return -EINVAL;
	}
	gpio_free(gpio->id[0]);

	if (!num_supplies)
		get_supplies(dev);

	if (!num_supplies)
		goto pin_config;

	/* Turn off VDDR(VDDI_REG): 1.5 or 1.6 */
	ret = regulator_disable(supplies[2].consumer);
	if (ret < 0) {
		pr_err("%s: regulator %s enable fail\n", __func__, supplies[2].supply);
		goto pin_config;
	}

	/* Wait 5ms */
	usleep_range(5000, 6000);

	/* Turn off VCI: 3.0 */
	ret = regulator_disable(supplies[1].consumer);
	if (ret < 0) {
		pr_err("%s: regulator %s enable fail\n", __func__, supplies[1].supply);
		goto pin_config;
	}

	/* Turn off VDDI(VDD3): 1.8 */
	ret = regulator_disable(supplies[0].consumer);
	if (ret < 0) {
		pr_err("%s: regulator %s enable fail\n", __func__, supplies[0].supply);
		goto pin_config;
	}

pin_config:
	set_pinctrl(dev, 0);

	return 0;
}

int reset_display_driver_panel(struct device *dev)
{
	struct display_driver *dispdrv;
	struct display_gpio *gpio;

	dispdrv = get_display_driver();

	gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio();
	gpio_request_one(gpio->id[0], GPIOF_OUT_INIT_HIGH, "lcd_reset");
	usleep_range(5000, 6000);
	gpio_set_value(gpio->id[0], 0);
	usleep_range(5000, 6000);
	gpio_set_value(gpio->id[0], 1);
	gpio_free(gpio->id[0]);

	/* Wait 10ms */
	usleep_range(10000, 11000);

	return 0;
}
#else
int enable_display_driver_power(struct device *dev)
{
#if !defined(CONFIG_MACH_XYREF5430) && !defined(CONFIG_MACH_ESPRESSO5433)
	struct display_driver *dispdrv = get_display_driver();
	struct display_gpio *gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio();
	int ret = 0, i;

#ifdef CONFIG_LCD_ALPM
	if (dispdrv->dsi_driver.dsim->lcd_alpm)
		return 0;
#endif

	if (!num_supplies)
		get_supplies(dev);

	if (!num_supplies)
		goto pin_config;

	/* Turn on VDDI(VDD3): 1.8 */
	ret = regulator_enable(supplies[0].consumer);
	if (ret < 0) {
		pr_err("%s: regulator %s enable fail\n", __func__, supplies[0].supply);
		goto pin_config;
	}

	if (gpio->num > 2) {
		/* 1. Turn on VDDR(VDDI_REG): 1.5 or 1.6 */
		/* 2. Turn on VCI: 3.0 */
		for (i = 1; i < gpio->num; i++) {
			ret = gpio_request_one(gpio->id[i], GPIOF_OUT_INIT_HIGH, "lcd_power");
			if (ret < 0) {
				pr_err("Failed to get gpio number for the lcd power(%d)\n", i);
				return ret;
			}
			gpio_free(gpio->id[i]);
		}
	} else {
		/* Turn on VDDR(VDDI_REG): 1.5 or 1.6 */
		ret = gpio_request_one(gpio->id[1], GPIOF_OUT_INIT_HIGH, "lcd_power");
		if (ret < 0) {
			pr_err("%s: gpio lcd_power request fail\n", __func__);
			goto pin_config;
		}
		gpio_free(gpio->id[1]);

		/* Turn on VCI: 3.0 */
		ret = regulator_enable(supplies[1].consumer);
		if (ret < 0) {
			pr_err("%s: regulator %s enable fail\n", __func__, supplies[0].supply);
			goto pin_config;
		}
	}
	/* Wait 10ms */
	usleep_range(10000, 11000);
#endif

pin_config:
	set_pinctrl(dev, 1);

	return 0;
}

int disable_display_driver_power(struct device *dev)
{
	struct display_driver *dispdrv;
	struct display_gpio *gpio;
	int ret, i;

	dispdrv = get_display_driver();
	gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio();

#ifdef CONFIG_LCD_ALPM
	if (dispdrv->dsi_driver.dsim->lcd_alpm)
		return 0;
#endif

	ret = gpio_request_one(gpio->id[0], GPIOF_OUT_INIT_LOW, "lcd_reset");
	if (ret < 0) {
		pr_err("Failed to get gpio number for the lcd_reset\n");
		return -EINVAL;
	}
	gpio_free(gpio->id[0]);

	if (!num_supplies)
		get_supplies(dev);

	if (!num_supplies)
		goto pin_config;

	if (gpio->num > 2) {
		/* 1. Turn off VCI: 3.0 */
		/* 2. Turn on VDDR(VDDI_REG): 1.5 or 1.6 */
		for (i = gpio->num - 1; i > 0; i--) {
			ret = gpio_request_one(gpio->id[i], GPIOF_OUT_INIT_LOW, "lcd_power");
			if (ret < 0) {
				pr_err("Failed to get gpio number for the lcd power(%d)\n", i);
				return -EINVAL;
			}
			gpio_free(gpio->id[i]);
		}
	} else {
		/* Turn off VCI: 3.0 */
		ret = regulator_disable(supplies[1].consumer);
		if (ret < 0) {
			pr_err("%s: regulator %s enable fail\n", __func__, supplies[0].supply);
			goto pin_config;
		}

#if !defined(CONFIG_MACH_XYREF5430) && !defined(CONFIG_MACH_ESPRESSO5433)
		/* Turn on VDDR(VDDI_REG): 1.5 or 1.6 */
		ret = gpio_request_one(gpio->id[1], GPIOF_OUT_INIT_LOW, "lcd_power");
		if (ret < 0) {
			pr_err("%s: gpio lcd_power request fail\n", __func__);
			goto pin_config;
		}
		gpio_free(gpio->id[1]);
		usleep_range(3000, 5000);
#endif
	}
	/* Turn off VDDI(VDD3): 1.8 */
	ret = regulator_disable(supplies[0].consumer);
	if (ret < 0) {
		pr_err("%s: regulator %s enable fail\n", __func__, supplies[0].supply);
		goto pin_config;
	}

pin_config:
	set_pinctrl(dev, 0);

	return 0;
}

int reset_display_driver_panel(struct device *dev)
{
	struct display_driver *dispdrv;
	struct display_gpio *gpio;

	dispdrv = get_display_driver();

#ifdef CONFIG_LCD_ALPM
	if (dispdrv->dsi_driver.dsim->lcd_alpm)
		return 0;
#endif

	gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio();
	gpio_request_one(gpio->id[0], GPIOF_OUT_INIT_HIGH, "lcd_reset");
	usleep_range(5000, 6000);
	gpio_set_value(gpio->id[0], 0);
	usleep_range(5000, 6000);
	gpio_set_value(gpio->id[0], 1);
	gpio_free(gpio->id[0]);

	/* Wait 10ms */
	usleep_range(10000, 11000);

	return 0;
}
#endif

int enable_display_decon_runtimepm(struct device *dev)
{
	return 0;
}

int disable_display_decon_runtimepm(struct device *dev)
{
	return 0;
}

int enable_display_dsd_clocks(struct device *dev, bool enable)
{
	struct display_driver *dispdrv;
	dispdrv = get_display_driver();

	if (!dispdrv->decon_driver.dsd_clk) {
		dispdrv->decon_driver.dsd_clk = clk_get(dev, "gate_dsd");
		if (IS_ERR(dispdrv->decon_driver.dsd_clk)) {
			pr_err("Failed to clk_get - gate_dsd\n");
			return -EBUSY;
		}
	}
#if defined(CONFIG_SOC_EXYNOS5433) && !defined(CONFIG_DECON_LCD_S6TNMR7)
	if (!dispdrv->decon_driver.gate_dsd_clk) {
		dispdrv->decon_driver.gate_dsd_clk = clk_get(dev, "gate_dsd_clk");
		if (IS_ERR(dispdrv->decon_driver.gate_dsd_clk)) {
			pr_err("Failed to clk_get - gate_dsd_clk\n");
			return -EBUSY;
		}
	}
	clk_prepare_enable(dispdrv->decon_driver.gate_dsd_clk);
#endif
	clk_prepare_enable(dispdrv->decon_driver.dsd_clk);
	return 0;
}

int disable_display_dsd_clocks(struct device *dev, bool enable)
{
	struct display_driver *dispdrv;
	dispdrv = get_display_driver();

	if (!dispdrv->decon_driver.dsd_clk)
		return -EBUSY;

	clk_disable_unprepare(dispdrv->decon_driver.dsd_clk);
#if defined(CONFIG_SOC_EXYNOS5433) && !defined(CONFIG_DECON_LCD_S6TNMR7)
	if (dispdrv->decon_driver.gate_dsd_clk)
		clk_disable_unprepare(dispdrv->decon_driver.gate_dsd_clk);
#endif
	return 0;
}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
bool check_camera_is_running(void)
{
#if defined(CONFIG_SOC_EXYNOS5433)
	if (readl(EXYNOS5433_CAM0_STATUS) & 0x1)
#else
	if (readl(EXYNOS5430_CAM0_STATUS) & 0x1)
#endif
		return true;
	else
		return false;
}

bool get_display_power_status(void)
{
#if defined(CONFIG_SOC_EXYNOS5433)
	if (readl(EXYNOS5433_DISP_STATUS) & 0x1)
#else
	if (readl(EXYNOS5430_DISP_STATUS) & 0x1)
#endif
		return true;
	else
		return false;
}

void set_hw_trigger_mask(struct s3c_fb *sfb, bool mask)
{
	unsigned int val;

	val = readl(sfb->regs + TRIGCON);
	if (mask)
		val &= ~(TRIGCON_HWTRIGMASK_I80_RGB);
	else
		val |= (TRIGCON_HWTRIGMASK_I80_RGB);

	writel(val, sfb->regs + TRIGCON);
}

int get_display_line_count(struct display_driver *dispdrv)
{
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	return readl(sfb->regs + VIDCON1) >> VIDCON1_LINECNT_SHIFT;
}
#endif

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
void set_default_hibernation_mode(struct display_driver *dispdrv)
{
	bool clock_gating = false;
	bool power_gating = false;
	bool hotplug_gating = false;

	if (lpcharge)
		pr_info("%s: off, Low power charging mode: %d\n", __func__, lpcharge);
	else {
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
		clock_gating = true;
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING
		power_gating = true;
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING_DEEPSTOP
		hotplug_gating = true;
#endif
	}
	dispdrv->pm_status.clock_gating_on = clock_gating;
	dispdrv->pm_status.power_gating_on = power_gating;
	dispdrv->pm_status.hotplug_gating_on = hotplug_gating;
}

void decon_clock_on(struct display_driver *dispdrv)
{
#if !defined(CONFIG_SOC_EXYNOS5430)
	int ret = 0;

	if (!dispdrv->decon_driver.clk) {
		dispdrv->decon_driver.clk = clk_get(dispdrv->display_driver, "gate_decon");
		if (IS_ERR(dispdrv->decon_driver.clk)) {
			pr_err("Failed to clk_get - gate_decon\n");
			return;
		}
	}
	clk_prepare(dispdrv->decon_driver.clk);
	ret = clk_enable(dispdrv->decon_driver.clk);
#endif
}

void mic_clock_on(struct display_driver *dispdrv)
{
#ifdef CONFIG_DECON_MIC
	struct decon_lcd *lcd = decon_get_lcd_info();
	int ret = 0;

	if (!lcd->mic)
		return;

	if (!dispdrv->mic_driver.clk) {
		dispdrv->mic_driver.clk = clk_get(dispdrv->display_driver, "gate_mic");
		if (IS_ERR(dispdrv->mic_driver.clk)) {
			pr_err("Failed to clk_get - gate_mic\n");
			return;
		}
	}
	clk_prepare(dispdrv->mic_driver.clk);
	ret = clk_enable(dispdrv->mic_driver.clk);
#endif
}

void dsi_clock_on(struct display_driver *dispdrv)
{
	int ret = 0;

	if (!dispdrv->dsi_driver.clk) {
		dispdrv->dsi_driver.clk = clk_get(dispdrv->display_driver, "gate_dsim0");
		if (IS_ERR(dispdrv->dsi_driver.clk)) {
			pr_err("Failed to clk_get - gate_dsi\n");
			return;
		}
	}
	clk_prepare(dispdrv->dsi_driver.clk);
	ret = clk_enable(dispdrv->dsi_driver.clk);
}

void decon_clock_off(struct display_driver *dispdrv)
{
#if !defined(CONFIG_SOC_EXYNOS5430)
	clk_disable(dispdrv->decon_driver.clk);
	clk_unprepare(dispdrv->decon_driver.clk);
#endif
}

void dsi_clock_off(struct display_driver *dispdrv)
{
	clk_disable(dispdrv->dsi_driver.clk);
	clk_unprepare(dispdrv->dsi_driver.clk);
}

void mic_clock_off(struct display_driver *dispdrv)
{
#ifdef CONFIG_DECON_MIC
	struct decon_lcd *lcd = decon_get_lcd_info();

	if (!lcd->mic)
		return;

	clk_disable(dispdrv->mic_driver.clk);
	clk_unprepare(dispdrv->mic_driver.clk);
#endif
}

struct pm_ops decon_pm_ops = {
	.clk_on		= decon_clock_on,
	.clk_off	= decon_clock_off,
};
#ifdef CONFIG_DECON_MIC
struct pm_ops mic_pm_ops = {
	.clk_on		= mic_clock_on,
	.clk_off	= mic_clock_off,
};
#endif
struct pm_ops dsi_pm_ops = {
	.clk_on		= dsi_clock_on,
	.clk_off	= dsi_clock_off,
};
#else
int disp_pm_runtime_get_sync(struct display_driver *dispdrv)
{
	pm_runtime_get_sync(dispdrv->display_driver);
	return 0;
}

int disp_pm_runtime_put_sync(struct display_driver *dispdrv)
{
	pm_runtime_put_sync(dispdrv->display_driver);
	return 0;
}
#endif

