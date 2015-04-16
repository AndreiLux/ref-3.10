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


/* default pwm clk */
#define DEFAULT_PWM_CLK_RATE	(120 * 1000000L)

static char __iomem *k3fd_pwm_base;
static struct clk *g_dss_pwm_clk;

static struct pinctrl_data pwmpctrl;

static struct pinctrl_cmd_desc pwm_pinctrl_init_cmds[] = {
	{DTYPE_PINCTRL_GET, &pwmpctrl, 0},
	{DTYPE_PINCTRL_STATE_GET, &pwmpctrl, DTYPE_PINCTRL_STATE_DEFAULT},
	{DTYPE_PINCTRL_STATE_GET, &pwmpctrl, DTYPE_PINCTRL_STATE_IDLE},
};

static struct pinctrl_cmd_desc pwm_pinctrl_normal_cmds[] = {
	{DTYPE_PINCTRL_SET, &pwmpctrl, DTYPE_PINCTRL_STATE_DEFAULT},
};

static struct pinctrl_cmd_desc pwm_pinctrl_lowpower_cmds[] = {
	{DTYPE_PINCTRL_SET, &pwmpctrl, DTYPE_PINCTRL_STATE_IDLE},
};

static struct pinctrl_cmd_desc pwm_pinctrl_finit_cmds[] = {
	{DTYPE_PINCTRL_PUT, &pwmpctrl, 0},
};


#define PWM_LOCK_OFFSET	(0x0000)
#define PWM_CTL_OFFSET	(0X0004)
#define PWM_CFG_OFFSET	(0x0008)
#define PWM_PR0_OFFSET	(0x0100)
#define PWM_PR1_OFFSET	(0x0104)
#define PWM_C0_MR_OFFSET	(0x0300)
#define PWM_C0_MR0_OFFSET	(0x0304)

#define PWM_OUT_PRECISION	(800)


int k3_pwm_set_backlight(struct k3_fb_data_type *k3fd)
{
	char __iomem *pwm_base = NULL;
	u32 bl_max = 0;
	u32 bl_level = 0;

	BUG_ON(k3fd == NULL);

	pwm_base = k3fd_pwm_base;
	BUG_ON(pwm_base == NULL);

	bl_level = k3fd->bl_level;
	bl_max = k3fd->panel_info.bl_max;
	if (bl_level > k3fd->panel_info.bl_max) {
		bl_level = k3fd->panel_info.bl_max;
	}

	if (bl_max < 1) {
		K3_FB_ERR("bl_max=%d is invalid!\n", bl_max);
		return -EINVAL;
	}

	bl_level = (bl_level * PWM_OUT_PRECISION) / bl_max;

	outp32(pwm_base + PWM_LOCK_OFFSET, 0x1acce551);
	outp32(pwm_base + PWM_CTL_OFFSET, 0x0);
	outp32(pwm_base + PWM_CFG_OFFSET, 0x2);
	outp32(pwm_base + PWM_PR0_OFFSET, 0x1);
	outp32(pwm_base + PWM_PR1_OFFSET, 0x2);
	outp32(pwm_base + PWM_CTL_OFFSET, 0x1);
	outp32(pwm_base + PWM_C0_MR_OFFSET, (PWM_OUT_PRECISION - 1));
	outp32(pwm_base + PWM_C0_MR0_OFFSET, bl_level);

	return 0;
}

int k3_pwm_on(struct platform_device *pdev)
{
	int ret = 0;
	struct clk *clk_tmp = NULL;

	BUG_ON(pdev == NULL);

	clk_tmp = g_dss_pwm_clk;
	if (clk_tmp) {
		ret = clk_prepare(clk_tmp);
		if (ret) {
			K3_FB_ERR("dss_pwm_clk clk_prepare failed, error=%d!\n", ret);
			return -EINVAL;
		}

		ret = clk_enable(clk_tmp);
		if (ret) {
			K3_FB_ERR("dss_pwm_clk clk_enable failed, error=%d!\n", ret);
			return -EINVAL;
		}
	}

	ret = pinctrl_cmds_tx(pdev, pwm_pinctrl_normal_cmds,
		ARRAY_SIZE(pwm_pinctrl_normal_cmds));

	return ret;
}

int k3_pwm_off(struct platform_device *pdev)
{
	int ret = 0;
	struct clk *clk_tmp = NULL;

	BUG_ON(pdev == NULL);

	ret = pinctrl_cmds_tx(pdev, pwm_pinctrl_lowpower_cmds,
		ARRAY_SIZE(pwm_pinctrl_lowpower_cmds));

	clk_tmp = g_dss_pwm_clk;
	if (clk_tmp) {
		clk_disable(clk_tmp);
		clk_unprepare(clk_tmp);
	}

	return ret;
}

static int k3_pwm_probe(struct platform_device *pdev)
{
	struct device_node *np = NULL;
	int ret = 0;

	K3_FB_DEBUG("+.\n");

	BUG_ON(pdev == NULL);

	np = of_find_compatible_node(NULL, NULL, DTS_COMP_PWM_NAME);
	if (!np) {
		K3_FB_ERR("NOT FOUND device node %s!\n", DTS_COMP_PWM_NAME);
		return -ENXIO;
	}

	/* get pwm reg base */
	k3fd_pwm_base = of_iomap(np, 0);

	/* pwm pinctrl init */
	ret = pinctrl_cmds_tx(pdev, pwm_pinctrl_init_cmds,
		ARRAY_SIZE(pwm_pinctrl_init_cmds));
	if (ret != 0) {
		K3_FB_ERR("Init pwm pinctrl failed! ret=%d.\n", ret);
		goto err_return;
	}

	/* get blpwm clk resource */
	g_dss_pwm_clk = of_clk_get(np, 0);
	if (IS_ERR(g_dss_pwm_clk)) {
		K3_FB_ERR("%s clock not found: %d!\n",
			np->name, (int)PTR_ERR(g_dss_pwm_clk));
		return -ENXIO;
	}

	ret = clk_set_rate(g_dss_pwm_clk, DEFAULT_PWM_CLK_RATE);
	if (ret < 0) {
		K3_FB_ERR("dss_pwm_clk clk_set_rate(%lu) failed, error=%d!\n",
			DEFAULT_PWM_CLK_RATE, ret);
		return -EINVAL;
	}

	K3_FB_INFO("dss_pwm_clk:[%lu]->[%lu].\n",
		DEFAULT_PWM_CLK_RATE, clk_get_rate(g_dss_pwm_clk));

	k3_fb_device_set_status0(DTS_PWM_READY);

	K3_FB_DEBUG("-.\n");

	return 0;

err_return:
	return ret;
}

static int k3_pwm_remove(struct platform_device *pdev)
{
	struct clk *clk_tmp = NULL;
	int ret = 0;

	ret = pinctrl_cmds_tx(pdev, pwm_pinctrl_finit_cmds,
		ARRAY_SIZE(pwm_pinctrl_finit_cmds));

	clk_tmp = g_dss_pwm_clk;
	if (clk_tmp) {
		clk_put(clk_tmp);
		clk_tmp = NULL;
	}

	return ret;
}

static const struct of_device_id k3_pwm_match_table[] = {
	{
		.compatible = DTS_COMP_PWM_NAME,
		.data = NULL,
	}
};
MODULE_DEVICE_TABLE(of, k3_pwm_match_table);

static struct platform_driver this_driver = {
	.probe = k3_pwm_probe,
	.remove = k3_pwm_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		.name = DEV_NAME_PWM,
		.of_match_table = of_match_ptr(k3_pwm_match_table),
	},
};

static int __init k3_pwm_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&this_driver);
	if (ret) {
		K3_FB_ERR("platform_driver_register failed, error=%d!\n", ret);
		return ret;
	}

	return ret;
}

module_init(k3_pwm_init);
