/* drivers/gpu/t6xx/kbase/src/platform/gpu_exynos5433.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_exynos5433.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/regulator/driver.h>
#include <linux/pm_qos.h>

#include <mach/asv-exynos.h>
#include <mach/pm_domains.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_control.h"

#if 0
#define L2CONFIG_MO_1BY8			(0b0101)
#define L2CONFIG_MO_1BY4			(0b1010)
#define L2CONFIG_MO_1BY2			(0b1111)
#define L2CONFIG_MO_NO_RESTRICT		(0)

#define G3D_NOC_DCG_EN 0x14a90200
#endif

extern struct kbase_device *pkbdev;

#define CPU_MAX PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE

/*  clk,vol,abb,min,max,down stay,time_in_state,pm_qos mem,pm_qos int,pm_qos cpu_kfc_min,pm_qos cpu_egl_max */
static gpu_dvfs_info gpu_dvfs_table_default[] = {
#if 0
	{600, 1150000, 0, 78, 100, 1, 0, 825000, 413000, 1500000, 1800000},
	{550, 1125000, 0, 78, 100, 1, 0, 825000, 413000, 1500000, 1800000},
	{500, 1075000, 0, 98, 100, 1, 0, 633000, 317000, 1500000, 1800000},
	{420, 1025000, 0, 80,  99, 1, 0, 543000, 267000,  900000, 1800000},
	{350, 1025000, 0, 80,  90, 1, 0, 413000, 200000,  500000, CPU_MAX},
	{266, 1000000, 0, 80,  90, 1, 0, 211000, 160000,  500000, CPU_MAX},
	{160, 1000000, 0,  0,  90, 1, 0, 136000, 133000,  500000, CPU_MAX},
#endif
};

static gpu_attribute gpu_config_attributes[] = {
#if 0
	{GPU_MAX_CLOCK, 600},
	{GPU_MIN_CLOCK, 160},
	{GPU_DVFS_START_CLOCK, 266},
	{GPU_DVFS_BL_CONFIG_CLOCK, 266},
	{GPU_GOVERNOR_START_CLOCK_DEFAULT, 266},
	{GPU_GOVERNOR_START_CLOCK_STATIC, 266},
	{GPU_GOVERNOR_START_CLOCK_BOOSTER, 266},
	{GPU_GOVERNOR_TABLE_DEFAULT, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_STATIC, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_BOOSTER, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_SIZE_DEFAULT, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_STATIC, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_BOOSTER, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_DEFAULT_VOLTAGE, 937500},
	{GPU_COLD_MINIMUM_VOL, 0},
	{GPU_VOLTAGE_OFFSET_MARGIN, 37500},
	{GPU_TMU_CONTROL, 1},
	{GPU_THROTTLING_90_95, 500},
	{GPU_THROTTLING_95_100,	350},
	{GPU_THROTTLING_100_105, 266},
	{GPU_THROTTLING_105_110, 160},
	{GPU_TRIPPING_110, 160},
	{GPU_POWER_COEFF, 46}, /* all core on param */
	{GPU_DVFS_TIME_INTERVAL, 5},
	{GPU_DEFAULT_WAKEUP_LOCK, 1},
	{GPU_BUS_DEVFREQ, 1},
	{GPU_DYNAMIC_CLK_GATING, 0},
	{GPU_DYNAMIC_ABB, 0},
	{GPU_EARLY_CLK_GATING, 0},
	{GPU_RUNTIME_PM_DELAY_TIME, 50},
	{GPU_DVFS_POLLING_TIME, 100},
	{GPU_DEBUG_LEVEL, DVFS_WARNING},
	{GPU_TRACE_LEVEL, TRACE_ALL},
#endif
};

void *gpu_get_config_attributes(void)
{
	return &gpu_config_attributes;
}

uintptr_t gpu_get_max_freq(void)
{
	return gpu_get_attrib_data(gpu_config_attributes, GPU_MAX_CLOCK) * 1000;
}

uintptr_t gpu_get_min_freq(void)
{
	return gpu_get_attrib_data(gpu_config_attributes, GPU_MIN_CLOCK) * 1000;
}

#if 0
struct clk *fin_pll;
struct clk *fout_g3d_pll;
struct clk *aclk_g3d;
struct clk *mout_g3d_pll;
struct clk *dout_aclk_g3d;
struct clk *mout_aclk_g3d;
#endif

int gpu_is_power_on(void)
{
#if 0
	return ((__raw_readl(EXYNOS5430_G3D_STATUS) & EXYNOS_INT_LOCAL_PWR_EN) == EXYNOS_INT_LOCAL_PWR_EN) ? 1 : 0;
#endif
}

int gpu_power_init(kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power initialized\n");

	return 0;
}

int gpu_get_cur_clock(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

#if 0
	if (!aclk_g3d) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: clock is not initialized\n", __func__);
		return -1;
	}

	return clk_get_rate(aclk_g3d)/MHZ;
#endif
}

int gpu_is_clock_on(void)
{
#if 0
	return __clk_is_enabled(aclk_g3d);
#endif
}

#if 0
#ifdef GPU_DYNAMIC_CLK_GATING
int gpu_dcg_enable(struct exynos_context *platform)
{
	int *p_dcg;

	if (!platform)
		return -ENODEV;

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: can't set clock on in power off status\n", __func__);
		return -1;
	}

	p_dcg = (int *)ioremap_nocache(G3D_NOC_DCG_EN, 4);
	/* ACLK_G3DND_600 is the same as the ACLK_G3D, so the current clock is read */
	if (platform->cur_clock < 275)
		*p_dcg = 0x3;
	else
		*p_dcg = 0x1;
	iounmap(p_dcg);

	return 0;
}

int gpu_dcg_disable(struct exynos_context *platform)
{
	int *p_dcg;

	if (!platform)
		return -ENODEV;

	p_dcg = (int *)ioremap_nocache(G3D_NOC_DCG_EN, 4);
	*p_dcg = 0x0;
	iounmap(p_dcg);

	return 0;
}
#endif /* GPU_DYNAMIC_CLK_GATING */
#endif

static int gpu_clock_on(struct exynos_context *platform)
{
	int ret = 0;
	if (!platform)
		return -ENODEV;

#ifdef CONFIG_PM_RUNTIME
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_PM_RUNTIME */

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: can't set clock on in power off status\n", __func__);
		ret = -1;
		goto err_return;
	}

	if (platform->clk_g3d_status == 1) {
		ret = 0;
		goto err_return;
	}

#if 0
	if (!gpu_is_clock_on()) {
		if (aclk_g3d) {
			(void) clk_prepare_enable(aclk_g3d);
			GPU_LOG(DVFS_DEBUG, LSI_CLOCK_ON, 0u, 0u, "clock is enabled\n");
		}
	}

#ifdef GPU_DYNAMIC_CLK_GATING
	gpu_dcg_enable(platform);
#endif /* GPU_DYNAMIC_CLK_GATING */
#endif
	platform->clk_g3d_status = 1;

err_return:
#ifdef CONFIG_PM_RUNTIME
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_PM_RUNTIME */
	return ret;
}

static int gpu_clock_off(struct exynos_context *platform)
{
	int ret = 0;

	if (!platform)
		return -ENODEV;

#ifdef CONFIG_PM_RUNTIME
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_PM_RUNTIME */

#if 0
#ifdef GPU_DYNAMIC_CLK_GATING
	gpu_dcg_disable(platform);
#endif /* GPU_DYNAMIC_CLK_GATING */
#endif
	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock off in power off status\n", __func__);
		ret = -1;
		goto err_return;
	}

	if (platform->clk_g3d_status == 0) {
		ret = 0;
		goto err_return;
	}

#if 0
	if (aclk_g3d) {
		(void)clk_disable_unprepare(aclk_g3d);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_OFF, 0u, 0u, "clock is disabled\n");
	}
#endif
	platform->clk_g3d_status = 0;

err_return:
#ifdef CONFIG_PM_RUNTIME
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_PM_RUNTIME */
	return ret;
}

static int gpu_set_maximum_outstanding_req(int val)
{
	volatile unsigned int reg;

	if (val > 0b1111)
		return -1;

	if (!pkbdev)
		return -2;

	if (!gpu_is_power_on())
		return -3;

	reg = kbase_os_reg_read(pkbdev, GPU_CONTROL_REG(L2_MMU_CONFIG));
	reg &= ~(0b1111 << 24);
	reg |= ((val & 0b1111) << 24);
	kbase_os_reg_write(pkbdev, GPU_CONTROL_REG(L2_MMU_CONFIG), reg);

	return 0;
}

int gpu_register_dump(void)
{
	if (gpu_is_power_on()) {
#if 0
		/* G3D PMU */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x105C4064, __raw_readl(EXYNOS5430_G3D_STATUS),
							"REG_DUMP: EXYNOS5430_G3D_STATUS %x\n", __raw_readl(EXYNOS5430_G3D_STATUS));
		/* G3D PLL */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0000, __raw_readl(EXYNOS5430_G3D_PLL_LOCK),
							"REG_DUMP: EXYNOS5430_G3D_PLL_LOCK %x\n", __raw_readl(EXYNOS5430_G3D_PLL_LOCK));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0100, __raw_readl(EXYNOS5430_G3D_PLL_CON0),
							"REG_DUMP: EXYNOS5430_G3D_PLL_CON0 %x\n", __raw_readl(EXYNOS5430_G3D_PLL_CON0));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0104, __raw_readl(EXYNOS5430_G3D_PLL_CON1),
							"REG_DUMP: EXYNOS5430_G3D_PLL_CON1 %x\n", __raw_readl(EXYNOS5430_G3D_PLL_CON1));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA010c, __raw_readl(EXYNOS5430_G3D_PLL_FREQ_DET),
							"REG_DUMP: EXYNOS5430_G3D_PLL_FREQ_DET %x\n", __raw_readl(EXYNOS5430_G3D_PLL_FREQ_DET));

		/* G3D SRC */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0200, __raw_readl(EXYNOS5430_SRC_SEL_G3D),
							"REG_DUMP: EXYNOS5430_SRC_SEL_G3D %x\n", __raw_readl(EXYNOS5430_SRC_SEL_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0300, __raw_readl(EXYNOS5430_SRC_ENABLE_G3D),
							"REG_DUMP: EXYNOS5430_SRC_ENABLE_G3D %x\n", __raw_readl(EXYNOS5430_SRC_ENABLE_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0400, __raw_readl(EXYNOS5430_SRC_STAT_G3D),
							"REG_DUMP: EXYNOS5430_SRC_STAT_G3D %x\n", __raw_readl(EXYNOS5430_SRC_STAT_G3D));

		/* G3D DIV */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0600, __raw_readl(EXYNOS5430_DIV_G3D),
							"REG_DUMP: EXYNOS5430_DIV_G3D %x\n", __raw_readl(EXYNOS5430_DIV_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0604, __raw_readl(EXYNOS5430_DIV_G3D_PLL_FREQ_DET),
							"REG_DUMP: EXYNOS5430_DIV_G3D_PLL_FREQ_DET %x\n", __raw_readl(EXYNOS5430_DIV_G3D_PLL_FREQ_DET));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0700, __raw_readl(EXYNOS5430_DIV_STAT_G3D),
							"REG_DUMP: EXYNOS5430_DIV_STAT_G3D %x\n", __raw_readl(EXYNOS5430_DIV_STAT_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0704, __raw_readl(EXYNOS5430_DIV_STAT_G3D_PLL_FREQ_DET),
							"REG_DUMP: EXYNOS5430_DIV_STAT_G3D_PLL_FREQ_DET %x\n", __raw_readl(EXYNOS5430_DIV_STAT_G3D_PLL_FREQ_DET));

		/* G3D ENABLE */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0800, __raw_readl(EXYNOS5430_ENABLE_ACLK_G3D),
							"REG_DUMP: EXYNOS5430_ENABLE_ACLK_G3D %x\n", __raw_readl(EXYNOS5430_ENABLE_ACLK_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0900, __raw_readl(EXYNOS5430_ENABLE_PCLK_G3D),
							"REG_DUMP: EXYNOS5430_ENABLE_PCLK_G3D %x\n", __raw_readl(EXYNOS5430_ENABLE_PCLK_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0A00, __raw_readl(EXYNOS5430_ENABLE_SCLK_G3D),
							"REG_DUMP: EXYNOS5430_ENABLE_SCLK_G3D %x\n", __raw_readl(EXYNOS5430_ENABLE_SCLK_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0B00, __raw_readl(EXYNOS5430_ENABLE_IP_G3D0),
							"REG_DUMP: EXYNOS5430_ENABLE_IP_G3D0 %x\n", __raw_readl(EXYNOS5430_ENABLE_IP_G3D0));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0B0A, __raw_readl(EXYNOS5430_ENABLE_IP_G3D1),
							"REG_DUMP: EXYNOS5430_ENABLE_IP_G3D1 %x\n", __raw_readl(EXYNOS5430_ENABLE_IP_G3D1));
#endif
	}

	return 0;
}

static int gpu_set_clock(struct exynos_context *platform, int clk)
{
	long g3d_rate_prev = -1;
	unsigned long g3d_rate = clk * MHZ;
	int ret = 0;

#if 0
	if (aclk_g3d == 0)
		return -1;
#endif

#ifdef CONFIG_PM_RUNTIME
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_PM_RUNTIME */

	if (!gpu_is_power_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock in the power-off state!\n", __func__);
		goto err;
	}

	if (!gpu_is_clock_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock in the clock-off state!\n", __func__);
		goto err;
	}

#if 0
	g3d_rate_prev = clk_get_rate(fout_g3d_pll);

	/* if changed the VPLL rate, set rate for VPLL and wait for lock time */
	if (g3d_rate != g3d_rate_prev) {
		ret = gpu_set_maximum_outstanding_req(L2CONFIG_MO_1BY8);
		if (ret < 0)
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to set MO (%d)\n", __func__, ret);

		/*change here for future stable clock changing*/
		ret = clk_set_parent(mout_g3d_pll, fin_pll);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_parent [mout_g3d_pll]\n", __func__);
			goto err;
		}

		/*change g3d pll*/
		ret = clk_set_rate(fout_g3d_pll, g3d_rate);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_rate [fout_g3d_pll]\n", __func__);
			goto err;
		}

		/*restore parent*/
		ret = clk_set_parent(mout_g3d_pll, fout_g3d_pll);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_parent [mout_g3d_pll]\n", __func__);
			goto err;
		}

#ifdef CONFIG_SOC_EXYNOS5433_REV_0
		/*restore parent*/
		ret = clk_set_parent(mout_aclk_g3d, aclk_g3d);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_parent [mout_ack_g3d]\n", __func__);
			goto err;
		}
#endif /* CONFIG_SOC_EXYNOS5433_REV_0 */

		ret = gpu_set_maximum_outstanding_req(L2CONFIG_MO_NO_RESTRICT);
		if (ret < 0)
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to restore MO (%d)\n", __func__, ret);

		g3d_rate_prev = g3d_rate;
	}
#endif

	platform->cur_clock = gpu_get_cur_clock(platform);

	GPU_LOG(DVFS_DEBUG, LSI_CLOCK_VALUE, 0u, g3d_rate/MHZ, "clock set: %ld\n", g3d_rate/MHZ);
	GPU_LOG(DVFS_DEBUG, LSI_CLOCK_VALUE, 0u, platform->cur_clock, "clock get: %d\n", platform->cur_clock);
err:
#ifdef CONFIG_PM_RUNTIME
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_PM_RUNTIME */
	return ret;
}

static int gpu_get_clock(kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

#if 0
	fin_pll = clk_get(kbdev->dev, "fin_pll");
	if (IS_ERR(fin_pll) || (fin_pll == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [fin_pll]\n", __func__);
		return -1;
	}

	fout_g3d_pll = clk_get(NULL, "fout_g3d_pll");
	if (IS_ERR(fout_g3d_pll)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [fout_g3d_pll]\n", __func__);
		return -1;
	}

	aclk_g3d = clk_get(kbdev->dev, "aclk_g3d");
	if (IS_ERR(aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [aclk_g3d]\n", __func__);
		return -1;
	}

	dout_aclk_g3d = clk_get(kbdev->dev, "dout_aclk_g3d");
	if (IS_ERR(dout_aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [dout_aclk_g3d]\n", __func__);
		return -1;
	}

	mout_g3d_pll = clk_get(kbdev->dev, "mout_g3d_pll");
	if (IS_ERR(mout_g3d_pll)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [mout_g3d_pll]\n", __func__);
		return -1;
	}

#ifdef CONFIG_SOC_EXYNOS5433_REV_0
	mout_aclk_g3d = clk_get(kbdev->dev, "mout_aclk_g3d");
	if (IS_ERR(mout_aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [mout_aclk_g3d]\n", __func__);
		return -1;
	}
#endif /* CONFIG_SOC_EXYNOS5433_REV_0 */
#endif

	return 0;
}

int gpu_clock_init(kbase_device *kbdev)
{
	int ret;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	ret = gpu_get_clock(kbdev);
	if (ret < 0)
		return -1;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "clock initialized\n");

	return 0;
}

int gpu_get_cur_voltage(struct exynos_context *platform)
{
	int ret = 0;
#ifdef CONFIG_REGULATOR
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: regulator is not initialized\n", __func__);
		return -1;
	}

	ret = regulator_get_voltage(platform->g3d_regulator);
#endif /* CONFIG_REGULATOR */
	return ret;
}

static int gpu_set_voltage(struct exynos_context *platform, int vol)
{
	static int _vol = -1;

	if (_vol == vol)
		return 0;

#ifdef CONFIG_REGULATOR
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: regulator is not initialized\n", __func__);
		return -1;
	}

	if (regulator_set_voltage(platform->g3d_regulator, vol, vol) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to set voltage, voltage: %d\n", __func__, vol);
		return -1;
	}
#endif /* CONFIG_REGULATOR */

	_vol = vol;

	GPU_LOG(DVFS_DEBUG, LSI_VOL_VALUE, 0u, vol, "voltage set:%d\n", vol);
	GPU_LOG(DVFS_DEBUG, LSI_VOL_VALUE, 0u, gpu_get_cur_voltage(platform), "voltage get:%d\n", gpu_get_cur_voltage(platform));

	return 0;
}

#ifdef CONFIG_REGULATOR
int gpu_regulator_enable(struct exynos_context *platform)
{
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: regulator is not initialized\n", __func__);
		return -1;
	}

	if (regulator_enable(platform->g3d_regulator) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to enable regulator\n", __func__);
		return -1;
	}
	return 0;
}

int gpu_regulator_disable(struct exynos_context *platform)
{
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: regulator is not initialized\n", __func__);
		return -1;
	}

	if (regulator_disable(platform->g3d_regulator) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to disable regulator\n", __func__);
		return -1;
	}
	return 0;
}

int gpu_regulator_init(struct exynos_context *platform)
{
	int gpu_voltage = 0;

#if 0
	platform->g3d_regulator = regulator_get(NULL, "vdd_g3d");
#endif
	if (IS_ERR(platform->g3d_regulator)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to get regulator, 0x%p\n", __func__, platform->g3d_regulator);
		platform->g3d_regulator = NULL;
		return -1;
	}

	if (gpu_regulator_enable(platform) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to enable regulator\n", __func__);
		platform->g3d_regulator = NULL;
		return -1;
	}

#if 0
	gpu_voltage = get_match_volt(ID_G3D, platform->gpu_dvfs_config_clock*1000);
#else
	gpu_voltage = 0;
#endif

	if (gpu_voltage == 0)
		gpu_voltage = platform->gpu_default_vol;

	if (gpu_set_voltage(platform, gpu_voltage) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to set voltage [%d]\n", __func__, gpu_voltage);
		return -1;
	}

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "regulator initialized\n");

	return 0;
}
#endif /* CONFIG_REGULATOR */

#if 0
static int gpu_set_voltage_pre(struct exynos_context *platform, bool is_up)
{
	if (!platform)
		return -ENODEV;

	if (!is_up && platform->dynamic_abb_status)
		set_match_abb(ID_G3D, gpu_dvfs_get_cur_asv_abb());

	return 0;
}

static int gpu_set_voltage_post(struct exynos_context *platform, bool is_up)
{
	if (!platform)
		return -ENODEV;

	if (is_up && platform->dynamic_abb_status)
		set_match_abb(ID_G3D, gpu_dvfs_get_cur_asv_abb());

	return 0;
}
#endif

static struct gpu_control_ops ctr_ops = {
	.is_power_on = gpu_is_power_on,
	.set_voltage = gpu_set_voltage,
#if 0
	.set_voltage_pre = gpu_set_voltage_pre,
	.set_voltage_post = gpu_set_voltage_post,
#endif
	.set_clock = gpu_set_clock,
#if 0
	.set_clock_pre = NULL,
	.set_clock_post = NULL,
#endif
	.enable_clock = gpu_clock_on,
	.disable_clock = gpu_clock_off,
};

struct gpu_control_ops *gpu_get_control_ops(void)
{
	return &ctr_ops;
}
