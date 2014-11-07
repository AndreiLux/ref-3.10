/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2014 by LG Electronics Inc.
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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/odin_clk.h>
#include <linux/odin_pm_domain.h>

#include "odin_aud_powclk.h"

struct aud_power_item {
	int is_enable;
};

struct aud_clock_item {
	int id;
	struct clk *clk;
	int enable_cnt;
};

struct odin_aud_power_clk {
	struct aud_power_item aud_pws[ODIN_AUD_PW_DOMAIN_MAX];
	struct aud_clock_item aud_clks[ODIN_AUD_CLK_MAX];
};

static char *aud_parent_string[ODIN_AUD_PARENT_MAX] = {
	"osc_clk",      /* ODIN_AUD_PARENT_OSC */
	"aud_pll_clk",  /* ODIN_AUD_PARENT_PLL */
};

struct odin_aud_power_clk *power_clk;

int odin_aud_powclk_get_clk_id(unsigned int phy_address)
{
	int id;
	switch (phy_address) {
	case 0x34610000:
		id = ODIN_AUD_CLK_MI2S0;
		break;
	case 0x34611000:
		id = ODIN_AUD_CLK_SI2S0;
		break;
	case 0x34612000:
		id = ODIN_AUD_CLK_SI2S1;
		break;
	case 0x34613000:
		id = ODIN_AUD_CLK_SI2S2;
		break;
	case 0x34614000:
		id = ODIN_AUD_CLK_SI2S3;
		break;
	case 0x34615000:
		id = ODIN_AUD_CLK_SI2S4;
		break;
	default:
		id = -EINVAL;
		break;
	}

	pr_debug("%s: phy_addr 0x%x has %d id\n", __func__, phy_address, id);
	return id;
}

int odin_aud_powclk_clk_unprepare_disable(int id)
{
	if (!power_clk)
		return -ENODEV;

	if (id < ODIN_AUD_CLK_SI2S0 || id >= ODIN_AUD_CLK_MAX)
		return -EINVAL;

	if (!power_clk->aud_clks[id].clk)
		return -ENODEV;

	pr_debug("%s: id %d, cnt %d\n", __func__, id,
		 power_clk->aud_clks[id].enable_cnt);

	if (power_clk->aud_clks[id].enable_cnt)
		power_clk->aud_clks[id].enable_cnt--;

	if (power_clk->aud_clks[id].enable_cnt == 0)
		clk_disable_unprepare(power_clk->aud_clks[id].clk);

	return 0;
}

int odin_aud_powclk_clk_prepare_enable(int id)
{
	int ret;
	if (!power_clk)
		return -ENODEV;

	if (id < ODIN_AUD_CLK_SI2S0 || id >= ODIN_AUD_CLK_MAX)
		return -EINVAL;

	if (!power_clk->aud_clks[id].clk)
		return -ENODEV;

	if (power_clk->aud_clks[id].enable_cnt == 0) {
		ret = clk_prepare_enable(power_clk->aud_clks[id].clk);
		if (ret) {
			pr_err("clk_enable failed: %d\n", ret);
			return ret;
		}
	}

	power_clk->aud_clks[id].enable_cnt++;

	pr_debug("%s: id %d, cnt %d\n", __func__, id,
		 power_clk->aud_clks[id].enable_cnt);

	return 0;
}

int odin_aud_powclk_set_clk_parent(int id, int parent_id, int freqeuncy)
{
	if (!power_clk)
		return -ENODEV;

	if (id < ODIN_AUD_CLK_SI2S0 || id >= ODIN_AUD_CLK_MAX)
		return -EINVAL;

	if (parent_id < ODIN_AUD_PARENT_OSC || parent_id >= ODIN_AUD_PARENT_MAX)
		return -EINVAL;

	if (!power_clk->aud_clks[id].clk)
		return -ENODEV;

	pr_debug("%s: id %d, set parent %s, freq %d\n", __func__, id,
		 aud_parent_string[parent_id], freqeuncy);

	return clk_set_par_rate(power_clk->aud_clks[id].clk,
				aud_parent_string[parent_id],
				freqeuncy);

}

static int odin_aud_powclk_platform_probe(struct platform_device *pdev)
{
	char strbuf[16] = {0, };
	int ret = 0;
	int i;

	power_clk = devm_kzalloc(&pdev->dev, sizeof(*power_clk), GFP_KERNEL);
	if (!power_clk) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto probe_exit;
	}

	for (i = 0; i < ODIN_AUD_CLK_MAX; i++) {
		sprintf(strbuf, "mclk_i2s%d", i);
		power_clk->aud_clks[i].clk = clk_get(&pdev->dev, strbuf);
		if (IS_ERR(power_clk->aud_clks[i].clk)) {
			dev_err(&pdev->dev, "Failed to get %s clock\n", strbuf);
			ret = PTR_ERR(power_clk->aud_clks[i].clk);
			goto probe_err;
		}
		power_clk->aud_clks[i].enable_cnt = 0;
		power_clk->aud_clks[i].id = i;

		dev_dbg(&pdev->dev, "%s clk geted, id %d", strbuf, i);
	}

	/*
	 * TO DO power sequence
	 */
	dev_info(&pdev->dev, "odin audio power and clock driver probed");
	return ret;
probe_err:
	do { clk_put(power_clk->aud_clks[i-1].clk); } while (i--);
	devm_kfree(&pdev->dev, power_clk);
probe_exit:
	return ret;
}

static int odin_aud_powclk_platform_remove(struct platform_device *pdev)
{
	int i;

	if (!power_clk)
		return -ENODEV;

	for (i = 0; i < ODIN_AUD_CLK_MAX; i++) {
		if (power_clk->aud_clks[i].enable_cnt)
			dev_warn(&pdev->dev, "%d clk is alive\n",
				 power_clk->aud_clks[i].id);
		clk_put(power_clk->aud_clks[i].clk);
	}

	devm_kfree(&pdev->dev, power_clk);
	power_clk = NULL;

	return 0;
}

static const struct of_device_id odin_aud_powclk_match[] = {
	{ .compatible = "lge,odin-aud-powclk", },
	{},
};

static struct platform_driver odin_aud_powclk_driver = {
	.driver = {
		.name = "odin-aud-powclk",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(odin_aud_powclk_match),
	},
	.probe = odin_aud_powclk_platform_probe,
	.remove = odin_aud_powclk_platform_remove,
};
module_platform_driver(odin_aud_powclk_driver);

MODULE_DESCRIPTION("ODIN AUDIO POWER AND CLOCK Driver");
MODULE_LICENSE("GPL");
