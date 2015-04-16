/* Copyright (c) 2013-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*
*/

#include "k3_fb.h"
#include "k3_overlay_utils.h"


void set_reg(char __iomem *addr, u32 val, u8 bw, u8 bs)
{
	u32 mask = (1 << bw) - 1;
	u32 tmp = 0;

	tmp = inp32(addr);
	tmp &= ~(mask << bs);
	outp32(addr, tmp | ((val & mask) << bs));
}

u32 set_bits32(u32 old_val, u32 val, u8 bw, u8 bs)
{
	u32 mask = (1 << bw) - 1;
	u32 tmp = 0;

	tmp = old_val;
	tmp &= ~(mask << bs);

	return (tmp | ((val & mask) << bs));
}

void k3fb_set_reg(struct k3_fb_data_type *k3fd,
	char __iomem *addr, u32 val, u8 bw, u8 bs)
{
	set_reg(addr, val, bw, bs);
}

u32 k3fb_line_length(int index, u32 xres, int bpp)
{
	return ALIGN_UP(xres * bpp, DMA_STRIDE_ALIGN);
}

unsigned int k3fb_ms2jiffies(unsigned long ms)
{
	return ((ms * HZ + 512) >> 10);
}

 void k3fb_get_timestamp(struct timeval *tv)
{
	struct timespec ts;

	ktime_get_ts(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;

	//struct timeval timestamp;
	//do_gettimeofday(&timestamp);
	//timestamp = ktime_to_timeval(ktime_get());
}

int k3fb_sbl_pow_i(int base,int exp)
{
	int result = 1;
	int i = 0;

	for (i = 1; i <= exp; ++i) {
		result *= base;
	}

	return result;
}

 u32 k3fb_timestamp_diff(struct timeval *lasttime, struct timeval *curtime)
 {
	 return (curtime->tv_usec >= lasttime->tv_usec) ?
		 curtime->tv_usec - lasttime->tv_usec :
		 1000000 - (lasttime->tv_usec - curtime->tv_usec);

	//return (curtime->tv_sec - lasttime->tv_sec) * 1000 +
	//	(curtime->tv_usec - lasttime->tv_usec) /1000;
 }

int k3fb_ctrl_fastboot(struct k3_fb_data_type *k3fd)
{
	struct k3_fb_panel_data *pdata = NULL;
	int ret = 0;

	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (pdata->set_fastboot) {
		ret = pdata->set_fastboot(k3fd->pdev);
	}

	return ret;
}

int k3fb_ctrl_on(struct k3_fb_data_type *k3fd)
{
	struct k3_fb_panel_data *pdata = NULL;
	int ret = 0;

	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (pdata->on) {
		ret = pdata->on(k3fd->pdev);
	}

	k3_overlay_on(k3fd);

	k3fb_vsync_resume(k3fd);

	return ret;
}

int k3fb_ctrl_off(struct k3_fb_data_type *k3fd)
{
	struct k3_fb_panel_data *pdata = NULL;
	int ret = 0;

	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	k3fb_vsync_suspend(k3fd);

	k3_overlay_off(k3fd);

	if (pdata->off) {
		ret = pdata->off(k3fd->pdev);
	}

	return ret;
}

int k3fb_ctrl_frc(struct k3_fb_data_type *k3fd, int fps)
{
	struct k3_fb_panel_data *pdata = NULL;
	int ret = 0;

	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (pdata->frc_handle) {
		ret = pdata->frc_handle(k3fd->pdev, fps);
	}

	return ret;
}

int k3fb_ctrl_esd(struct k3_fb_data_type *k3fd)
{
	struct k3_fb_panel_data *pdata = NULL;
	int ret = 0;

	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (pdata->esd_handle) {
		ret = pdata->esd_handle(k3fd->pdev);
	}

	return ret;
}

int k3fb_ctrl_sbl(struct fb_info *info, void __user *argp)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_fb_panel_data *pdata = NULL;
	int val = 0;
	int sbl_lsensor_value = 0;
	int sbl_level = 0;
	int sbl_enable = 0;
	dss_sbl_t sbl;

	int assertiveness = 0;
	int assertiveness_base = 0;
	int assertiveness_exp = 0;
	int calib_a = 60;
	int calib_b = 95;
	int calib_c = 5;
	int calib_d = 1;
	int tmp = 0;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (!k3fd->panel_info.sbl_support) {
		K3_FB_ERR("fb%d, SBL not supported!", k3fd->index);
		return 0;
	}

	ret = copy_from_user(&val, argp, sizeof(val));
	if (ret) {
		K3_FB_ERR("k3fb_ctrl_sbl ioctl failed!ret=%d.", ret);
		return ret;
	}

	sbl_lsensor_value = val & 0xffff;
	sbl_level = (val >> 16) & 0xff;
	sbl_enable = (val >> 24) & 0x1;

	assertiveness_base = 2;
	assertiveness_exp = 8 * (sbl_level - 128) / 255;

	if (assertiveness_exp < 0) {
		assertiveness_exp *= (-1);
		assertiveness = k3fb_sbl_pow_i(assertiveness_base, assertiveness_exp);
		calib_a = calib_a / assertiveness;
		calib_b = calib_b;
		calib_c = calib_c / assertiveness;
		calib_d = calib_d / assertiveness;
	} else {
		assertiveness = k3fb_sbl_pow_i(assertiveness_base, assertiveness_exp);
		calib_a = calib_a * assertiveness;
		calib_b = calib_b;
		calib_c = calib_c * assertiveness;
		calib_d = calib_d * assertiveness;
	}

	tmp = k3fd->bl_level & 0xff;
	sbl.sbl_backlight_l = set_bits32(sbl.sbl_backlight_l, tmp, 8, 0);
	tmp = (k3fd->bl_level >> 8) & 0xff;
	sbl.sbl_backlight_h= set_bits32(sbl.sbl_backlight_h, tmp, 8, 0);
	tmp = sbl_lsensor_value & 0xff;
	sbl.sbl_ambient_light_l = set_bits32(sbl.sbl_ambient_light_l, tmp, 8, 0);
	tmp= (sbl_lsensor_value >> 8) & 0xff;
	sbl.sbl_ambient_light_h= set_bits32(sbl.sbl_ambient_light_h, tmp, 8, 0);
	tmp = calib_a & 0xff;
	sbl.sbl_calibration_a_l = set_bits32(sbl.sbl_calibration_a_l, tmp, 8, 0);
	tmp= (calib_a >> 8) & 0xff;
	sbl.sbl_calibration_a_h= set_bits32(sbl.sbl_calibration_a_h, tmp, 8, 0);
	tmp = calib_b & 0xff;
	sbl.sbl_calibration_b_l = set_bits32(sbl.sbl_calibration_b_l, tmp, 8, 0);
	tmp= (calib_b >> 8) & 0xff;
	sbl.sbl_calibration_b_h= set_bits32(sbl.sbl_calibration_b_h, tmp, 8, 0);
	tmp = calib_c & 0xff;
	sbl.sbl_calibration_c_l = set_bits32(sbl.sbl_calibration_c_l, tmp, 8, 0);
	tmp= (calib_c >> 8) & 0xff;
	sbl.sbl_calibration_c_h= set_bits32(sbl.sbl_calibration_c_h, tmp, 8, 0);
	tmp = calib_d & 0xff;
	sbl.sbl_calibration_d_l = set_bits32(sbl.sbl_calibration_d_l, tmp, 8, 0);
	tmp= (calib_d >> 8) & 0xff;
	sbl.sbl_calibration_d_h = set_bits32(sbl.sbl_calibration_d_h, tmp, 8, 0);
	tmp = sbl_enable & 0x1;
	sbl.sbl_enable = set_bits32(sbl.sbl_enable, tmp, 1, 0);

	memcpy(&(k3fd->sbl), &sbl, sizeof(dss_sbl_t));

	if (!is_mipi_cmd_panel(k3fd) && pdata->sbl_ctrl) {
		k3fb_activate_vsync(k3fd);
		k3fd->sbl_enable = (sbl_enable > 0) ? 1 : 0;
		pdata->sbl_ctrl(k3fd->pdev, k3fd->sbl_enable);
		k3fb_deactivate_vsync(k3fd);
	}

	return 0;
}

int k3fb_ctrl_dss_clk_rate(struct fb_info *info, void __user *argp)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	dss_clk_rate_t dss_clk_rate;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	ret = copy_from_user(&dss_clk_rate, argp, sizeof(dss_clk_rate_t));
	if (ret) {
		K3_FB_ERR("k3fb_ctrl_sbl ioctl failed!ret=%d.", ret);
		return ret;
	}

	if (dss_clk_rate.dss_pri_clk_rate > 0) {
		k3fd->dss_clk_rate.dss_pri_clk_rate = dss_clk_rate.dss_pri_clk_rate;
	}

	if (dss_clk_rate.dss_aux_clk_rate > 0) {
		k3fd->dss_clk_rate.dss_aux_clk_rate = dss_clk_rate.dss_aux_clk_rate;
	}

	if (dss_clk_rate.dss_pclk_clk_rate > 0) {
		k3fd->dss_clk_rate.dss_pclk_clk_rate = dss_clk_rate.dss_pclk_clk_rate;
	}

	ret = set_dss_clk_rate(k3fd);

	return ret;
}
