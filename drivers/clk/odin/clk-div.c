#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/odin_clk.h>

#define to_clk_divider(_hw) container_of(_hw, struct odin_clk_divider, hw)
#define to_lj_pll(_hw) container_of(_hw, struct lj_pll, hw)

#define div_mask(d)	((1 << (d->width)) - 1)
#define is_power_of_two(i)	!(i & ~i)

const static char *req_clk_name;

static void woden_cpu_div_update(struct clk_hw *hw)
{
	void __iomem *cpu_base_reg;

	if (!((hw->clk)->secure_flag)){
		cpu_base_reg = ioremap(WODEN_CPU_CFG_BASE, SZ_4K);

		writel(0x00000F71, cpu_base_reg+0x100);
		writel(0x06710100, cpu_base_reg+0x110);
		writel(0x0000FF71, cpu_base_reg+0x100);
		writel(0x3E710100, cpu_base_reg+0x110);

		iounmap((void *)cpu_base_reg);
	}else{
#ifdef CONFIG_ODIN_TEE
		tz_write(0x00000F71, WODEN_CPU_CFG_BASE+0x100);
		tz_write(0x06710100, WODEN_CPU_CFG_BASE+0x110);
		tz_write(0x0000FF71, WODEN_CPU_CFG_BASE+0x100);
		tz_write(0x3E710100, WODEN_CPU_CFG_BASE+0x110);
#else
		cpu_base_reg = ioremap(WODEN_CPU_CFG_BASE, SZ_4K);

		writel(0x00000F71, cpu_base_reg+0x100);
		writel(0x06710100, cpu_base_reg+0x110);
		writel(0x0000FF71, cpu_base_reg+0x100);
		writel(0x3E710100, cpu_base_reg+0x110);

		iounmap((void *)cpu_base_reg);
#endif
	}
}

static unsigned int _get_table_maxdiv(const struct clk_div_table *table)
{
	unsigned int maxdiv = 0;
	const struct clk_div_table *clkt;

	for (clkt = table; clkt->div; clkt++)
		if (clkt->div > maxdiv)
			maxdiv = clkt->div;
	return maxdiv;
}

static unsigned int _get_maxdiv(struct odin_clk_divider *divider)
{
	if (divider->flags & CLK_DIVIDER_ONE_BASED)
		return div_mask(divider);
	if (divider->flags & CLK_DIVIDER_POWER_OF_TWO)
		return 1 << div_mask(divider);
	if (divider->table)
		return _get_table_maxdiv(divider->table);
	return div_mask(divider) + 1;
}

static unsigned int _get_table_div(const struct clk_div_table *table,
							unsigned int val)
{
	const struct clk_div_table *clkt;

	for (clkt = table; clkt->div; clkt++)
		if (clkt->val == val)
			return clkt->div;
	return 0;
}

static unsigned int _get_div(struct odin_clk_divider *divider, unsigned int val)
{
	if (divider->flags & CLK_DIVIDER_ONE_BASED)
		return val;
	if (divider->flags & CLK_DIVIDER_POWER_OF_TWO)
		return 1 << val;
	if (divider->table)
		return _get_table_div(divider->table, val);
	return val + 1;
}

static unsigned int _get_table_val(const struct clk_div_table *table,
							unsigned int div)
{
	const struct clk_div_table *clkt;

	for (clkt = table; clkt->div; clkt++)
		if (clkt->div == div)
			return clkt->val;
	return 0;
}

static unsigned int _get_val(struct odin_clk_divider *divider, u8 div)
{
	if (divider->flags & CLK_DIVIDER_ONE_BASED)
		return div;
	if (divider->flags & CLK_DIVIDER_POWER_OF_TWO)
		return __ffs(div);
	if (divider->table)
		return  _get_table_val(divider->table, div);
	return div - 1;
}

static unsigned long clk_divider_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct odin_clk_divider *divider = to_clk_divider(hw);
	unsigned int div, val;

	if (!((hw->clk)->secure_flag)){
		val = readl(divider->reg) >> divider->shift;
	}else{
#ifdef CONFIG_ODIN_TEE
		val = tz_read(divider->p_reg) >> divider->shift;
#else
		val = readl(divider->reg) >> divider->shift;
#endif
	}

	val &= div_mask(divider);

	div = _get_div(divider, val);

	if (!div) {
		WARN(1, "%s: Invalid divisor for clock %s\n", __func__,
						__clk_get_name(hw->clk));
		return parent_rate;
	}

	return parent_rate / div;
}

/*
 * The reverse of DIV_ROUND_UP: The maximum number which
 * divided by m is r
 */
#define MULT_ROUND_UP(r, m) ((r) * (m) + (m) - 1)

static bool _is_valid_table_div(const struct clk_div_table *table,
							 unsigned int div)
{
	const struct clk_div_table *clkt;

	for (clkt = table; clkt->div; clkt++)
		if (clkt->div == div)
			return true;
	return false;
}

static bool _is_valid_div(struct odin_clk_divider *divider, unsigned int div)
{
	if (divider->flags & CLK_DIVIDER_POWER_OF_TWO)
		return is_power_of_two(div);
	if (divider->table)
		return _is_valid_table_div(divider->table, div);
	return true;
}

static int clk_divider_bestdiv(struct clk_hw *hw, unsigned long rate,
		unsigned long *best_parent_rate)
{
	struct odin_clk_divider *divider = to_clk_divider(hw);
	int i, bestdiv = 0;
	unsigned long parent_rate, best = 0, now, maxdiv;

	if (!rate)
		rate = 1;

	maxdiv = _get_maxdiv(divider);

	if (!(__clk_get_flags(hw->clk) & CLK_SET_RATE_PARENT)) {
		parent_rate = *best_parent_rate;
		bestdiv = DIV_ROUND_UP(parent_rate, rate);
		bestdiv = bestdiv == 0 ? 1 : bestdiv;
		bestdiv = bestdiv > maxdiv ? maxdiv : bestdiv;
		return bestdiv;
	}

	/*
	 * The maximum divider we can use without overflowing
	 * unsigned long in rate * i below
	 */
	maxdiv = min(ULONG_MAX / rate, maxdiv);

	for (i = 1; i <= maxdiv; i++) {
		if (!_is_valid_div(divider, i))
			continue;
		parent_rate = __clk_round_rate(__clk_get_parent(hw->clk),
				MULT_ROUND_UP(rate, i));
		now = parent_rate / i;
		if (now <= rate && now > best) {
			bestdiv = i;
			best = now;
			*best_parent_rate = parent_rate;
		}
	}

	if (!bestdiv) {
		bestdiv = _get_maxdiv(divider);
		*best_parent_rate = __clk_round_rate(__clk_get_parent(hw->clk), 1);
	}

	return bestdiv;
}

static long clk_divider_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	struct odin_clk_divider *divider = to_clk_divider(hw);
	struct clk_pll_table *cpt;
	unsigned long ret = 0;
	int div;

	req_clk_name = __clk_get_name(hw->clk);

	switch (divider->mach_type){
		case MACH_WODEN:
			if (divider->pll_type == CPU_PLL_CLK){
				cpt = ljpll_search_table(woden_cpu_clk_t,
					get_table_size(woden_cpu_clk_t), rate);
				div = clk_divider_bestdiv(hw, rate, prate);
				s_req_rate = cpt->clk_rate;
				ret = s_req_rate;
			}else if (divider->pll_type == GPU_PLL_CLK){
				cpt = ljpll_search_table(woden_comm_clk_t,
					get_table_size(woden_comm_clk_t), rate);
				div = clk_divider_bestdiv(hw, rate, prate);
				s_req_rate = cpt->clk_rate;
				ret = s_req_rate;
			}else{
				div = clk_divider_bestdiv(hw, rate, prate);
				ret = *prate / div;
			}

			break;
		case MACH_ODIN:
			if (divider->pll_type == CPU_PLL_CLK){
				if (0==strcmp(hw->clk->name, "ca15_pll_clk"))
					cpt = ljpll_search_table(odin_ca15_clk_t,
						get_table_size(odin_ca15_clk_t), rate);
				else
					cpt = ljpll_search_table(odin_ca7_clk_t,
						get_table_size(odin_ca7_clk_t), rate);

				div = clk_divider_bestdiv(hw, rate, prate);
				s_req_rate = cpt->clk_rate;
				ret = s_req_rate;
			}else if (divider->pll_type == GPU_PLL_CLK){
				cpt = ljpll_search_table(odin_gpu_clk_t,
					get_table_size(odin_gpu_clk_t), rate);
				div = clk_divider_bestdiv(hw, rate, prate);
				s_req_rate = cpt->clk_rate;
				ret = s_req_rate;
			}else if (divider->pll_type == ISP_PLL_CLK){
				cpt = ljpll_search_table(odin_isp_clk_t,
					get_table_size(odin_isp_clk_t), rate);
				div = clk_divider_bestdiv(hw, rate, prate);
				s_req_rate = cpt->clk_rate;
				ret = s_req_rate;
			}else{
				div = clk_divider_bestdiv(hw, rate, prate);
				ret = *prate / div;
			}

			break;
		default:
			return -1;
	}

	return ret;
}

static void update_divreg(struct odin_clk_divider *divider, int secure_flag)
{
	u32 val;

	if (secure_flag == NON_SECURE){
		val = readl(divider->update_reg);
		val &= ~BIT(divider->update_bit);
		writel(val, divider->update_reg);
		val |= BIT(divider->update_bit);
		writel(val, divider->update_reg);
	}else{
#ifdef CONFIG_ODIN_TEE
		val = tz_read((u32)divider->p_update_reg);
		val &= ~BIT(divider->update_bit);
		tz_write(val, (u32)divider->p_update_reg);
		val |= BIT(divider->update_bit);
		tz_write(val, (u32)divider->p_update_reg);
#else
		val = readl(divider->update_reg);
		val &= ~BIT(divider->update_bit);
		writel(val, divider->update_reg);
		val |= BIT(divider->update_bit);
		writel(val, divider->update_reg);
#endif
	}
}

static int chg_nsecure_divreg(struct clk_hw *hw, unsigned long req_rate,
	unsigned int div)
{
	struct odin_clk_divider *divider = to_clk_divider(hw);
	struct lj_pll *pll = NULL;
	struct clk *clk = hw->clk;
	unsigned int value;
	unsigned long flags = 0;
	u32 val;

	while (strcmp(__clk_get_name(clk->parent), "osc_clk"))
		clk = clk->parent;

	pll = to_lj_pll(clk->hw);

	if (div == 0) {
		value = 0;
	} else {
		value = _get_val(divider, div);

		if (value > div_mask(divider))
			value = div_mask(divider);
	}

	if (divider->lock && !((hw->clk)->secure_flag))
		spin_lock_irqsave(divider->lock, flags);

	if (pll->dvfs_flag==DVFS_PLL_DIV) {
		pll->dvfs_flag = 0;

		if (divider->lock && !((hw->clk)->secure_flag))
		spin_unlock_irqrestore(divider->lock, flags);

		return 0;
	}

	val = readl(divider->reg);

	val &= ~(div_mask(divider) << divider->shift);
	val |= value << divider->shift;

	writel(val, divider->reg);

	if ((divider->update_reg != NULL) && (divider->p_update_reg != 0x0)){
		switch (divider->mach_type){
			case MACH_WODEN:
				if (divider->pll_type == CPU_PLL_CLK)
					woden_cpu_div_update(hw);
				else
					update_divreg(divider, NON_SECURE);
				break;
			case MACH_ODIN:
				update_divreg(divider, NON_SECURE);
				break;
		}
	}

	if (divider->lock && !((hw->clk)->secure_flag))
		spin_unlock_irqrestore(divider->lock, flags);

	pll->dvfs_flag = 0;

	return 0;
}

static int chg_secure_divreg(struct clk_hw *hw, unsigned long req_rate,
	unsigned int div)
{
	struct odin_clk_divider *divider = to_clk_divider(hw);
	struct lj_pll *pll = NULL;
	struct clk *clk = hw->clk;
	u32 val;
	u32 sft_wid_ud_bit = divider->shift | divider->width << 8 |
		divider->update_bit << 12 | divider->pll_type<<20;

	while (strcmp(__clk_get_name(clk->parent), "osc_clk"))
		clk = clk->parent;

	pll = to_lj_pll(clk->hw);

	if (0==strcmp(clk->name, "ca15_pll_clk") ||
		0==strcmp(clk->name, "ca7_pll_clk") ||	0==strcmp(clk->name, "mem_pll_clk")
		|| 0==strcmp(clk->name, "gpu_pll_clk")){

		if (0==strcmp(clk->name, "ca15_pll_clk"))
			sft_wid_ud_bit = ((sft_wid_ud_bit) & 0xFFFFF)|(CA15_PLL_CLK<<20);
		else if (0==strcmp(clk->name, "ca7_pll_clk"))
			sft_wid_ud_bit = ((sft_wid_ud_bit) & 0xFFFFF)|(CA7_PLL_CLK<<20);
		else
			sft_wid_ud_bit = sft_wid_ud_bit;

		if (pll->pll_type == CPU_PLL_CLK || pll->pll_type == GPU_PLL_CLK)
			goto out;

		if (pll->dvfs_flag!=DVFS_PLL_DIV){
			mb_clk_set_div(divider->p_reg, divider->p_update_reg,	sft_wid_ud_bit, div,
			(((req_rate/1000000)<<16)|(hw->clk->rate/1000000)));
		}
	} else {
		if (div != 0)
			div = div - 1;

#ifdef CONFIG_ODIN_TEE
		val = tz_read(divider->p_reg);
#else
		val = readl(divider->reg);
#endif

		val &= ~(div_mask(divider) << divider->shift);
		val |= div << divider->shift;

#ifdef CONFIG_ODIN_TEE
		tz_write(val, divider->p_reg);
#else
		writel(val, divider->reg);
#endif

		if ((divider->update_reg != NULL) || (divider->p_update_reg != 0x0)){
			update_divreg(divider, SECURE);
		}
	}

out:
	pll->dvfs_flag = 0;

	return 0;
}

static int clk_divider_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct odin_clk_divider *divider = to_clk_divider(hw);
	struct lj_pll *pll = NULL;
	struct clk *clk = hw->clk;
	unsigned long req_rate = 0;
	unsigned int div = 0;
	unsigned int val = 0;

	while (strcmp(__clk_get_name(clk->parent), "osc_clk"))
		clk = clk->parent;

	pll = to_lj_pll(clk->hw);

	if (pll->dvfs_flag == DVFS_PLL_DIV)
	{
		pll->dvfs_flag = 0;
		return 0;
	}

	switch (divider->mach_type){
		case MACH_WODEN:
			if (divider->pll_type == CPU_PLL_CLK)
			{
				if (0 == strcmp(__clk_get_name(hw->clk), "ca15_armclk_div") || \
					0 == strcmp(__clk_get_name(hw->clk), "ca7_armclk_div") || \
					0 == strcmp(__clk_get_name(hw->clk), "ca7_cci_400_div") || \
					0 == strcmp(__clk_get_name(hw->clk), "ca7_gic_400_div")){

					if (0 == strcmp(__clk_get_name(hw->clk), req_clk_name))
						div = parent_rate / s_req_rate;
					else
						div = parent_rate / __clk_get_rate(hw->clk);
				}
			}else
				div = parent_rate / rate;

			break;
		case MACH_ODIN:
			if (divider->pll_type == CPU_PLL_CLK || divider->pll_type == GPU_PLL_CLK)
			{
				div = parent_rate / s_req_rate;
				req_rate = s_req_rate;
			}else if (pll->pll_type == ISP_PLL_CLK){
				if (pll->dvfs_flag == DVFS_PLL_ONLY){
					div = parent_rate / s_req_rate;

					if (s_req_rate == 0)
						return 0;

					req_rate = s_req_rate;
				}else{
					div = parent_rate / rate;
					req_rate = rate;
				}

			}else{
				div = parent_rate / rate;
				req_rate = rate;
			}

			break;

		default:
			return -1;
	}

	if (!((hw->clk)->secure_flag)){
		val = readl(divider->reg) >> divider->shift;
	}else{
#ifdef CONFIG_ODIN_TEE
		val = tz_read(divider->p_reg) >> divider->shift;
#else
		val = readl(divider->reg) >> divider->shift;
#endif
	}

	val &= div_mask(divider);

	if ((div-1) == val) {
		return 0;
	}

	if (!(hw->clk)->secure_flag)
		chg_nsecure_divreg(hw, req_rate, div);
	else
		chg_secure_divreg(hw, req_rate, div);

	return 0;
}

struct clk_ops odin_clk_divider_ops = {
	.recalc_rate = clk_divider_recalc_rate,
	.round_rate = clk_divider_round_rate,
	.set_rate = clk_divider_set_rate,
};

static struct clk *_register_divider(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags, void __iomem *reg,
		void __iomem *update_reg, u32 p_reg, u32 p_update_reg, u8 shift,
		u8 width, u8 update_bit, u8 mach_type, u8 pll_type, u8 clk_divider_flags,
		const struct clk_div_table *table, u8 secure_flag, spinlock_t *lock)
{
	struct odin_clk_divider *div;
	struct clk *clk;
	struct clk_init_data init;

	/* allocate the divider */
	div = kzalloc(sizeof(struct odin_clk_divider), GFP_KERNEL);
	if (!div) {
		pr_err("%s: could not allocate divider clk\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	init.name = name;
	init.ops = &odin_clk_divider_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = (parent_name ? &parent_name: NULL);
	init.num_parents = (parent_name ? 1 : 0);

	/* struct clk_divider assignments */
	div->reg = reg;
	div->update_reg = update_reg;
	div->p_reg = p_reg;
	div->p_update_reg = p_update_reg;
	div->shift = shift;
	div->width = width;
	div->update_bit = update_bit;
	div->mach_type = mach_type;
	div->pll_type = pll_type;
	div->flags = clk_divider_flags;
	div->lock = lock;
	div->hw.init = &init;
	div->table = table;

	/* register the clock */
	clk = __odin_clk_register(dev, &div->hw, reg, p_reg, secure_flag);

	if (IS_ERR(clk))
		kfree(div);

	return clk;
}

struct clk *odin_clk_register_div(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags, void __iomem *reg,
		void __iomem *update_reg, u32 p_reg, u32 p_update_reg, u8 shift,
		u8 width, u8 update_bit, u8 mach_type, u8 pll_type,
		u8 clk_divider_flags, u8 secure_flag, spinlock_t *lock)
{
	return _register_divider(dev, name, parent_name, flags, reg, update_reg,
	p_reg, p_update_reg, shift, width, update_bit, mach_type, pll_type,
	clk_divider_flags, NULL, secure_flag, lock);
}
