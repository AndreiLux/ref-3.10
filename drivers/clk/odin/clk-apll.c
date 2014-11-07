#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/odin_clk.h>

#define to_aud_pll(_hw) container_of(_hw, struct aud_pll, hw)

#define TO_OSC 0
#define TO_PLL 1

struct clksrc_chg_reg{
	int		mach_type;
	u32		off_set[6];
};

static struct clksrc_chg_reg aud_clksrc_calbit[] = {
	{MACH_WODEN, {0x3C, 0x48, 0x54, 0x60, 0x6C, 0x78},},
	{MACH_ODIN, {0x50, 0x60, 0x70, 0x80, 0x90, 0xa0}},
};

struct clk_pll_table woden_aud_clk_t[] = {
	{ .clk_rate = AUD_CLK_RATE_98, .reg_val = 0x03E80002, .aud_val = 1024000},
	{ .clk_rate = AUD_CLK_RATE_90, .reg_val = 0x04406F02, .aud_val = 1114556},
	{ .clk_rate = AUD_CLK_RATE_49, .reg_val = 0x03E80022, .aud_val = 512000},
	{ .clk_rate = AUD_CLK_RATE_45, .reg_val = 0x04406F22, .aud_val = 557278},
	{ .clk_rate = AUD_CLK_RATE_32, .reg_val = 0x05DC0022, .aud_val = 768000},
	{ .clk_rate = AUD_CLK_RATE_24, .reg_val = 0x03E80042, .aud_val = 256000},
	{ .clk_rate = AUD_CLK_RATE_22, .reg_val = 0x04406F42, .aud_val = 278639},
	{ .clk_rate = AUD_CLK_RATE_16, .reg_val = 0x05DC0042, .aud_val = 384000},
	{ .clk_rate = AUD_CLK_RATE_12, .reg_val = 0x03E80062, .aud_val = 170666},
	{ .clk_rate = AUD_CLK_RATE_11, .reg_val = 0x04406F62, .aud_val = 185759},
	{ .clk_rate = AUD_CLK_RATE_8, .reg_val = 0x05DC0062, .aud_val = 557279},
	{ .clk_rate = 0},
};

static void chg_clk_src(struct clk_hw *hw, int osc_pll_flag)
{
	struct aud_pll *pll = to_aud_pll(hw);
	int i, j;
	u32 reg_val;

	if (osc_pll_flag == TO_OSC){
		reg_val = 0x0;

		for (i=0; i<ARRAY_SIZE(aud_clksrc_calbit); i++){
			if (pll->mach_type == aud_clksrc_calbit[i].mach_type){
				if (!((hw->clk)->secure_flag)){
					for (j=0; j < ARRAY_SIZE(aud_clksrc_calbit[i].off_set); j++)
						writel(reg_val, pll->reg + (aud_clksrc_calbit[i].off_set[j]));
				}else{
					for (j=0; j < ARRAY_SIZE(aud_clksrc_calbit[i].off_set); j++){
#ifdef CONFIG_ODIN_TEE
						tz_write(reg_val, pll->p_reg+(aud_clksrc_calbit[i].off_set[j]));
#else
						writel(reg_val, pll->reg + (aud_clksrc_calbit[i].off_set[j]));
#endif
					}
				}
			}
		}
	}
}

static void apll_pdb_endisable(struct clk_hw *hw, int enable)
{
	struct aud_pll *pll = to_aud_pll(hw);
	int set = 0;
	u32 val;

	unsigned long flags = 0;

	set ^= enable;

	if (pll->lock && !((hw->clk)->secure_flag))
		spin_lock_irqsave(pll->lock, flags);

	if (!((hw->clk)->secure_flag)){
		val = readl(pll->reg);
	}else{
#ifdef CONFIG_ODIN_TEE
		val = tz_read(pll->p_reg);
#else
		val = readl(pll->reg);
#endif

	}

	if (set){
		val |= BIT(pll->pdb_shift);
	}else{
		val &= ~BIT(pll->pdb_shift);
	}

	if (!((hw->clk)->secure_flag)){
		writel(val, pll->reg);
	}else{
#ifdef CONFIG_ODIN_TEE
		tz_write(val, pll->p_reg);
#else
		writel(val, pll->reg);
#endif
	}

	if (pll->lock && !((hw->clk)->secure_flag))
		spin_unlock_irqrestore(pll->lock, flags);
}

static int apll_enable(struct clk_hw *hw)
{
	apll_pdb_endisable(hw, 1);

	return 0;
}

static void apll_disable(struct clk_hw *hw)
{
	apll_pdb_endisable(hw, 0);
}

unsigned long apll_recalc(struct clk_hw *hw,
	unsigned long parent_rate)
{
	struct aud_pll *pll = to_aud_pll(hw);
	unsigned long ll, m, fcw, od;
	u32 reg;
	int i;

	if (!((hw->clk)->secure_flag)){
		reg = readl(pll->reg+0x4);
	}else{
#ifdef CONFIG_ODIN_TEE
		reg = tz_read(pll->p_reg+0x4);
#else
		reg = readl(pll->reg+0x4);
#endif
	}

	m = reg & 0x1f;
	fcw = (reg >> 8) & 0xfffff;
	od = (reg >> 5) & 0x3;

	if (od == 0)
		ll = (fcw * m) * 2;
	else
		ll = (fcw * m) / od;

	if (fcw * m == 0)
	{
		printk("pll rate is 0!\n");
		return 0;
	}

	for (i=0; i<ARRAY_SIZE(woden_aud_clk_t); i++)
	{
		if (ll == woden_aud_clk_t[i].aud_val)
		{
			ll = woden_aud_clk_t[i].clk_rate;
			break;
		}
	}

	return ll;
}

long apll_round_rate(struct clk_hw *hw,
	unsigned long target_rate, unsigned long *parent_rate)
{
	return target_rate;
}

int apll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long best_rate)
{
	struct aud_pll *pll = to_aud_pll(hw);
	u32 lock_time = 0x0;
	unsigned long flags = 0;
	u32 sel_reg_val[6] = {0x0, };
	int i, j;

	if (rate == hw->clk->rate)
		return 0;

	apll_enable(hw);

	for (i=0; i<ARRAY_SIZE(aud_clksrc_calbit); i++){
		if (pll->mach_type == aud_clksrc_calbit[i].mach_type){
			if (!((hw->clk)->secure_flag)){
				for (j=0; j < ARRAY_SIZE(aud_clksrc_calbit[i].off_set); j++){
					sel_reg_val[j] = readl(pll->reg+(aud_clksrc_calbit[i].off_set[j]));
				}
			}else{
				for (j=0; j < ARRAY_SIZE(aud_clksrc_calbit[i].off_set); j++){
#ifdef CONFIG_ODIN_TEE
					sel_reg_val[j] = tz_read(pll->p_reg+(aud_clksrc_calbit[i].off_set[j]));
#else
					sel_reg_val[j] = readl(pll->reg+(aud_clksrc_calbit[i].off_set[j]));
#endif
				}
			}
		}
	}

	if (pll->lock && !((hw->clk)->secure_flag))
		spin_lock_irqsave(pll->lock, flags);

	/*Bypass the src clock and set clock*/
	chg_clk_src(hw, TO_OSC);

	for (i=0; i<ARRAY_SIZE(woden_aud_clk_t); i++)
	{
		if (rate >= (woden_aud_clk_t[i].clk_rate - 1000000))
		{
			writel(woden_aud_clk_t[i].reg_val, pll->reg+0x4);
			break;
		}
	}

	if (i == ARRAY_SIZE(woden_aud_clk_t))
	{
		printk("Can not set aud-pll!\n");
		goto not_support;
	}

	/*lock time*/
	while (!(lock_time >> 31))
	{
		if (!((hw->clk)->secure_flag)){
			lock_time = readl(pll->lock_time_reg);
		}else{
#ifdef CONFIG_ODIN_TEE
			lock_time = tz_read(pll->p_lock_time_reg);
#else
			lock_time = readl(pll->lock_time_reg);
#endif
		}
	}

not_support:

	/*Back to the src clock*/
	for (i=0; i<ARRAY_SIZE(aud_clksrc_calbit); i++){
		if (pll->mach_type == aud_clksrc_calbit[i].mach_type){
			if (!((hw->clk)->secure_flag)){
				for (j=0; j < ARRAY_SIZE(aud_clksrc_calbit[i].off_set); j++){
					writel(sel_reg_val[j], pll->reg+(aud_clksrc_calbit[i].off_set[j]));
				}
			}else{
				for (j=0; j < ARRAY_SIZE(aud_clksrc_calbit[i].off_set); j++){
#ifdef CONFIG_ODIN_TEE
					tz_write(sel_reg_val[j], pll->p_reg+(aud_clksrc_calbit[i].off_set[j]));
#else
					writel(sel_reg_val[j], pll->reg+(aud_clksrc_calbit[i].off_set[j]));
#endif
				}
			}
		}
	}

	if (pll->lock && !((hw->clk)->secure_flag))
		spin_unlock_irqrestore(pll->lock, flags);

	return 0;
}

struct clk_ops aud_pll_ops = {
	.enable = apll_enable,
	.disable = apll_disable,
	.recalc_rate = apll_recalc,
	.set_rate = apll_set_rate,
	.round_rate = apll_round_rate,
};

struct clk *odin_clk_register_apll(struct device *dev, const char *name,
			const char *parent_name, int flags, void __iomem *reg,
			void __iomem *lock_time_reg, u32 p_reg, u32 p_lock_time_reg,
			u8 pdb_shift, u8 mach_type, u8 pll_type, u8 secure_flag,
			spinlock_t *lock)
{
	struct aud_pll *pll;
	struct clk *clk;
	struct clk_init_data init;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &aud_pll_ops;
	init.flags = flags;
	init.parent_names = (parent_name ? &parent_name: NULL);
	init.num_parents = (parent_name ? 1 : 0);

	pll->reg = reg;
	pll->lock_time_reg = lock_time_reg;
	pll->p_reg = p_reg;
	pll->p_lock_time_reg = p_lock_time_reg;
	pll->pdb_shift = pdb_shift;
	pll->mach_type = mach_type;
	pll->pll_type = pll_type;
	pll->lock = lock;
	pll->hw.init = &init;

	clk = clk_register(dev, &pll->hw);

	if (IS_ERR(clk))
		kfree(pll);

	return clk;
}
