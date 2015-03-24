/*
 * mt_clk.c
 * Simple API that supports clock management
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <mach/mt_reg_base.h> /* ISP_ADDR */
#include <mach/mt_gpio_def.h> /* basic pin manipulation */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>

struct mt_clk_init_info {
	struct clk_init_data data;
	struct clk_hw *hw;
	int (*init)(struct mt_clk_init_info *);
};

struct mt_clk_pin {
	struct clk_hw hw;
	u16 pin;
	u16 mode;
};

static int mt_clk_pin_init(struct mt_clk_init_info *info)
{
	struct mt_clk_pin *clk_pin = container_of(info->hw, struct mt_clk_pin, hw);
	int pin = mt_pin_find_by_pin_name(0, info->data.name, NULL);
	if (pin < 0 || gpio_request(pin, __func__) || mt_pin_set_mode_clk(pin))
		return -ENOENT;
	clk_pin->pin = pin;
	clk_pin->mode = mt_pin_find_mode(clk_pin->pin, MT_MODE_CLK);
	pr_debug("%s: pin=%d, mode=%d\n", __func__, clk_pin->pin, clk_pin->mode);
	return 0;
}

static int mt_clk_pin_enable(struct clk_hw *hw)
{
	struct mt_clk_pin *clk_pin = container_of(hw, struct mt_clk_pin, hw);
	mt_pin_set_mode(clk_pin->pin, clk_pin->mode);
	mt_pin_set_load(clk_pin->pin, 4, true);
	pr_debug("%s: pin=%d\n", __func__, clk_pin->pin);
	return 0;
}

static void mt_clk_pin_disable(struct clk_hw *hw)
{
	struct mt_clk_pin *clk_pin = container_of(hw, struct mt_clk_pin, hw);
	mt_pin_set_mode_gpio(clk_pin->pin);
	gpio_direction_output(clk_pin->pin, 0);
	mt_pin_set_load(clk_pin->pin, 0, true);
	pr_debug("%s: pin=%d\n", __func__, clk_pin->pin);
}

static int mt_clk_pin_is_enabled(struct clk_hw *hw)
{
	struct mt_clk_pin *clk_pin = container_of(hw, struct mt_clk_pin, hw);
	int mode = mt_pin_get_mode(clk_pin->pin);
	pr_debug("%s: pin=%d; mode=%d\n", __func__, clk_pin->pin, mode);
	return mode == clk_pin->mode;
}

static struct clk_ops mt_clk_pin_ops = {
	.enable = mt_clk_pin_enable,
	.disable = mt_clk_pin_disable,
	.is_enabled = mt_clk_pin_is_enabled,
};

static struct mt_clk_pin cmmclk;

static DEFINE_SPINLOCK(isp_mclk_lock);

/* data borrowed from camera_isp.c */
static struct clk_gate isp_mclk1 = {
	.reg = (void __iomem *)(ISP_ADDR + 0x4300),
	.bit_idx = 29,
	.lock = &isp_mclk_lock,
};
static struct clk_gate isp_mclk2 = {
	.reg = (void __iomem *)(ISP_ADDR + 0x43A0),
	.bit_idx = 29,
	.lock = &isp_mclk_lock,
};

static struct mt_clk_init_info mt_clk_info[] __initdata = {
	{
		.data = {
			.name = "CMMCLK",
			.flags = CLK_IS_ROOT,
			.ops  = &mt_clk_pin_ops,
		},
		.init = mt_clk_pin_init,
		.hw = &cmmclk.hw,
	},
	{
		.data = {
			.name = "ISP_MCLK1",
			.flags = CLK_IS_ROOT | CLK_IS_BASIC,
			.ops  = &clk_gate_ops,
		},
		.hw = &isp_mclk1.hw,
	},
	{
		.data = {
			.name = "ISP_MCLK2",
			.flags = CLK_IS_ROOT | CLK_IS_BASIC,
			.ops  = &clk_gate_ops,
		},
		.hw = &isp_mclk2.hw,
	},
};

static struct clk __init *mt_clk_register(struct mt_clk_init_info *info)
{
	struct clk *clk;
	int err = 0;

	info->hw->init = &info->data;
	clk = clk_register(NULL, info->hw);
	if (!IS_ERR(clk)) {
		if (info->init)
			err = info->init(info);
		/* clock must be initially disabled */
		if (info->data.ops->disable)
			info->data.ops->disable(info->hw);
		clk_register_clkdev(clk, info->data.name, NULL);
		clk_prepare(clk);
	}
	return err < 0 ? ERR_PTR(err) : clk;
}

int __init mt_clk_init(void)
{
	struct clk *clk;
	struct mt_clk_init_info *info = mt_clk_info;
	int i;
	for (i = 0; i < ARRAY_SIZE(mt_clk_info); ++i, ++info) {
		clk = mt_clk_register(info);
		if (IS_ERR(clk))
			pr_err("%s: failed to register clock: %s\n",
				__func__, info->data.name);
	}
	return 0;
}
