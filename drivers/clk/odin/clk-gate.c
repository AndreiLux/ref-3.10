#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/odin_clk.h>

#define to_clk_gate(_hw) container_of(_hw, struct odin_clk_gate, hw)

static void clk_gate_endisable(struct clk_hw *hw, int enable)
{
	struct odin_clk_gate *gate = to_clk_gate(hw);
	int set = gate->flags & CLK_GATE_SET_TO_DISABLE ? 1 : 0;
	u32 reg;

	unsigned long flags = 0;

	set ^= enable;

	if (gate->lock && !((hw->clk)->secure_flag))
		spin_lock_irqsave(gate->lock, flags);

	if (!((hw->clk)->secure_flag)){
		reg = readl(gate->reg);
	}else{
#ifdef CONFIG_ODIN_TEE
		reg = tz_read(gate->p_reg);
#else
		reg = readl(gate->reg);
#endif
	}

	if (set)
		reg |= BIT(gate->bit_idx);
	else
		reg &= ~BIT(gate->bit_idx);

	if (!((hw->clk)->secure_flag)){
		writel(reg, gate->reg);
	}else{
#ifdef CONFIG_ODIN_TEE
		tz_write(reg, gate->p_reg);
#else
		writel(reg, gate->reg);
#endif
	}

	if (gate->lock && !((hw->clk)->secure_flag))
		spin_unlock_irqrestore(gate->lock, flags);
}

static int odin_clk_gate_enable(struct clk_hw *hw)
{
	clk_gate_endisable(hw, 1);

	return 0;
}

static void odin_clk_gate_disable(struct clk_hw *hw)
{
	clk_gate_endisable(hw, 0);
}

static int odin_clk_gate_is_enabled(struct clk_hw *hw)
{
	u32 reg;
	struct odin_clk_gate *gate = to_clk_gate(hw);

	if (!((hw->clk)->secure_flag)){
		reg = readl(gate->reg);
	}else{
#ifdef CONFIG_ODIN_TEE
		reg = tz_read(gate->p_reg);
#else
		reg = readl(gate->reg);
#endif
	}

	/* if a set bit disables this clk, flip it before masking */
	if (gate->flags & CLK_GATE_SET_TO_DISABLE)
		reg ^= BIT(gate->bit_idx);

	reg &= BIT(gate->bit_idx);

	return reg ? 1 : 0;
}

struct clk_ops odin_clk_gate_ops = {
	.enable = odin_clk_gate_enable,
	.disable = odin_clk_gate_disable,
	.is_enabled = odin_clk_gate_is_enabled,
};

struct clk *odin_clk_register_gate(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		void __iomem *reg, u32 p_reg, u8 bit_idx,
		u8 clk_gate_flags, u8 secure_flag, spinlock_t *lock)
{
	struct odin_clk_gate *gate;
	struct clk *clk;
	struct clk_init_data init;

	/* allocate the gate */
	gate = kzalloc(sizeof(struct odin_clk_gate), GFP_KERNEL);
	if (!gate) {
		pr_err("%s: could not allocate gated clk\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	init.name = name;
	init.ops = &odin_clk_gate_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = (parent_name ? &parent_name: NULL);
	init.num_parents = (parent_name ? 1 : 0);

	/* struct clk_gate assignments */
	gate->reg = reg;
	gate->p_reg = p_reg;
	gate->bit_idx = bit_idx;
	gate->flags = clk_gate_flags;
	gate->lock = lock;
	gate->hw.init = &init;

	clk = __odin_clk_register(dev, &gate->hw, reg, p_reg, secure_flag);

	if (IS_ERR(clk))
		kfree(gate);

	return clk;
}
