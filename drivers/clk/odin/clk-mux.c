#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/odin_clk.h>

#define to_clk_mux(_hw) container_of(_hw, struct odin_clk_mux, hw)

static u8 odin_clk_mux_get_parent(struct clk_hw *hw)
{
	struct odin_clk_mux *mux = to_clk_mux(hw);
	u32 val;

	if (!((hw->clk)->secure_flag)){
		val = readl(mux->reg) >> mux->shift;
	}else{
#ifdef CONFIG_ODIN_TEE
		val = tz_read(mux->p_reg) >> mux->shift;
#else
		val = readl(mux->reg) >> mux->shift;
#endif
	}

	val &= (1 << mux->width) - 1;

	if (val && (mux->flags & CLK_MUX_INDEX_BIT))
		val = ffs(val) - 1;

	if (val && (mux->flags & CLK_MUX_INDEX_ONE))
		val--;

	if (val >= __clk_get_num_parents(hw->clk))
		return -EINVAL;

	return val;
}

static int odin_clk_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct odin_clk_mux *mux = to_clk_mux(hw);
	u32 val;
	unsigned long flags = 0;

	if (mux->flags & CLK_MUX_INDEX_BIT)
		index = (1 << ffs(index));

	if (mux->flags & CLK_MUX_INDEX_ONE)
		index++;

	if (mux->lock && !((hw->clk)->secure_flag))
		spin_lock_irqsave(mux->lock, flags);

	if (!((hw->clk)->secure_flag)){
		val = readl(mux->reg);
	}else{
#ifdef CONFIG_ODIN_TEE
		val = tz_read(mux->p_reg);
#else
		val = readl(mux->reg);
#endif
	}

	val &= ~(((1 << mux->width) - 1) << mux->shift);
	val |= index << mux->shift;

	if (!((hw->clk)->secure_flag)){
		writel(val, mux->reg);
	}else{
#ifdef CONFIG_ODIN_TEE
		tz_write(val, mux->p_reg);
#else
		writel(val, mux->reg);
#endif
	}

	if (mux->lock && !((hw->clk)->secure_flag))
		spin_unlock_irqrestore(mux->lock, flags);

	return 0;
}

struct clk_ops odin_clk_mux_ops = {
	.get_parent = odin_clk_mux_get_parent,
	.set_parent = odin_clk_mux_set_parent,
};

struct clk *odin_clk_register_mux(struct device *dev, const char *name,
		const char **parent_names, u8 num_parents, unsigned long flags,
		void __iomem *reg, u32 p_reg, u8 shift, u8 width,
		u8 clk_mux_flags, u8 secure_flag,  spinlock_t *lock)
{
	struct odin_clk_mux *mux;
	struct clk *clk;
	struct clk_init_data init;

	/* allocate the mux */
	mux = kzalloc(sizeof(struct clk_mux), GFP_KERNEL);
	if (!mux) {
		pr_err("%s: could not allocate mux clk\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	init.name = name;
	init.ops = &odin_clk_mux_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = parent_names;
	init.num_parents = num_parents;

	/* struct clk_mux assignments */
	mux->reg = reg;
	mux->p_reg = p_reg;
	mux->shift = shift;
	mux->width = width;
	mux->flags = clk_mux_flags;
	mux->lock = lock;
	mux->hw.init = &init;

	clk = __odin_clk_register(dev, &mux->hw, reg, p_reg, secure_flag);

	if (IS_ERR(clk))
		kfree(mux);

	return clk;
}
