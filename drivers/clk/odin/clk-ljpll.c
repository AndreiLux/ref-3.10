#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/odin_clk.h>

#define to_lj_pll(_hw) container_of(_hw, struct lj_pll, hw)
#define pll_mask	((1 << (1)) - 1)

unsigned long s_req_rate;

struct clk_cal_reg{
	int		pll_type;
	u8		b_shift[5];
	u32		bit_mask[5];
};

static struct clk_cal_reg pll_calc_bit[] = {
	{CPU_PLL_CLK, {12, 17, 6, 8, 0}, {0x1F, 0x3, 0x3, 0xF, 0x3F}},
	{COMM_PLL_CLK, {4, 12, 16, 20, 24}, {0x1F, 0x3, 0x3, 0xF, 0x3F}},
	{MEM_PLL_CLK, {4, 12, 16, 20, 24}, {0x1F, 0x3, 0x3, 0xF, 0x3F}},
	{GPU_PLL_CLK, {4, 12, 16, 20, 24}, {0x1F, 0x3, 0x3, 0xF, 0x3F}},
	{ISP_PLL_CLK, {4, 12, 16, 20, 24}, {0x1F, 0x3, 0x3, 0xF, 0x3F}},
	{DP2_PLL_CLK, {4, 12, 16, 20, 24}, {0x1F, 0x3, 0x3, 0xF, 0x3F}},
	{ICS_PLL_CLK, {11, 4, 8, 1, 5}, {0x1F, 0x3, 0x3, 0xF, 0x3F}},
};

struct clk_pll_table woden_cpu_clk_t[] = {
	{ .clk_rate = 1100000000, .reg_val = 0x03380345},
	{ .clk_rate = 1000000000, .reg_val = 0x03380145},
	{ .clk_rate = 900000000, .reg_val = 0x033A0312},
	{ .clk_rate = 800000000, .reg_val = 0x0158264B},
	{ .clk_rate = 700000000, .reg_val = 0x033A030E},
	{ .clk_rate = 600000000, .reg_val = 0x033A020C},
	{ .clk_rate = 550000000, .reg_val = 0x03380345},
	{ .clk_rate = 500000000, .reg_val = 0x03380145},
	{ .clk_rate = 450000000, .reg_val = 0x033A0312},
	{ .clk_rate = 400000000, .reg_val = 0x0158264B},
	{ .clk_rate = 350000000, .reg_val = 0x033A030E},
	{ .clk_rate = 300000000, .reg_val = 0x033A020C},
	{ .clk_rate = 260000000, .reg_val = 0x0158264B},
	{ .clk_rate = 200000000, .reg_val = 0x0158264B},
	{ .clk_rate = 150000000, .reg_val = 0x033A020C},
	{ .clk_rate = 100000000, .reg_val = 0x0158264B},
	{ .clk_rate = 0},
};

struct clk_pll_table woden_comm_clk_t[] = {
	{ .clk_rate = 790000000, .reg_val = 0x10201001},
	{ .clk_rate = 700000000, .reg_val = 0x0E301001},
	{ .clk_rate = 600000000, .reg_val = 0x0C201001},
	{ .clk_rate = 550000000, .reg_val = 0x05300001},
	{ .clk_rate = 500000000, .reg_val = 0x0A201001},
	{ .clk_rate = 390000000, .reg_val = 0x10201001},
	{ .clk_rate = 350000000, .reg_val = 0x0E301001},
	{ .clk_rate = 300000000, .reg_val = 0x0C201001},
	{ .clk_rate = 250000000, .reg_val = 0x0A201001},
	{ .clk_rate = 190000000, .reg_val = 0x0C201001},
	{ .clk_rate = 150000000, .reg_val = 0x0C201001},
	{ .clk_rate = 100000000, .reg_val = 0x10201001},
	{ .clk_rate = 50000000, .reg_val = 0x10201001},
	{ .clk_rate = 30000000, .reg_val = 0x10201001},
	{ .clk_rate = 0},
};

struct clk_pll_table *odin_ca15_clk_t;
struct clk_pll_table *odin_ca7_clk_t;
struct clk_pll_table *odin_gpu_clk_t;
struct clk_pll_table *odin_mem_clk_t;
struct clk_pll_table *odin_isp_clk_t;
struct clk_pll_table *odin_dp2_clk_t;
struct clk_pll_table *odin_comm_clk_t;

int get_odin_pll_table(u32 pll_type){

	struct clk_pll_table *cpt;
	void __iomem *sram_base;
	int table_size, i;
	u32 cpu_type, next_reg = 0;

	sram_base = ioremap(ODIN_PLL_TABLE_BASE, SZ_256);

	table_size = mb_get_pll_table(pll_type);

	switch (pll_type & 0xF){
		case CPU_PLL_CLK:
			cpu_type = pll_type >> 4;

			if(cpu_type == CA15_PLL_CLK){
				odin_ca15_clk_t = kzalloc((size_t)(sizeof(struct clk_pll_table)*table_size),
					GFP_KERNEL);
				if (!odin_ca15_clk_t)
					return -ENOMEM;

				for(i=0; i<table_size; i++){
					odin_ca15_clk_t[i].clk_rate = readl(sram_base + next_reg)*1000;
					odin_ca15_clk_t[i].reg_val = readl((sram_base+0x4) + next_reg);
					next_reg = next_reg + 0x8;
				}
			}else{
				odin_ca7_clk_t = kzalloc((size_t)(sizeof(struct clk_pll_table)*table_size),
					GFP_KERNEL);

				if (!odin_ca7_clk_t)
					return -ENOMEM;

				for(i=0; i<table_size; i++){
					odin_ca7_clk_t[i].clk_rate = readl(sram_base + next_reg)*1000;
					odin_ca7_clk_t[i].reg_val = readl((sram_base+0x4) + next_reg);
					next_reg = next_reg + 0x8;
				}
			}

			break;

		case MEM_PLL_CLK:
			odin_mem_clk_t = kzalloc((size_t)(sizeof(struct clk_pll_table)*table_size),
					GFP_KERNEL);

			if (!odin_mem_clk_t)
				return -ENOMEM;

			for(i=0; i<table_size; i++){
				odin_mem_clk_t[i].clk_rate = readl(sram_base + next_reg)*1000;
				odin_mem_clk_t[i].reg_val = readl((sram_base+0x4) + next_reg);
				next_reg = next_reg + 0x8;
			}

			break;

		case GPU_PLL_CLK:
			odin_gpu_clk_t = kzalloc((size_t)(sizeof(struct clk_pll_table)*table_size),
					GFP_KERNEL);

			if (!odin_gpu_clk_t)
				return -ENOMEM;

			for(i=0; i<table_size; i++){
				odin_gpu_clk_t[i].clk_rate = readl(sram_base + next_reg)*1000;
				odin_gpu_clk_t[i].reg_val = readl((sram_base+0x4) + next_reg);
				next_reg = next_reg + 0x8;
			}

			break;

		case ISP_PLL_CLK:
			odin_isp_clk_t = kzalloc((size_t)(sizeof(struct clk_pll_table)*table_size),
					GFP_KERNEL);

			if (!odin_isp_clk_t)
				return -ENOMEM;

			for(i=0; i<table_size; i++){
				odin_isp_clk_t[i].clk_rate = readl(sram_base + next_reg)*1000;
				odin_isp_clk_t[i].reg_val = readl((sram_base+0x4) + next_reg);
				next_reg = next_reg + 0x8;
			}

			break;

		case DP2_PLL_CLK:
			odin_dp2_clk_t = kzalloc((size_t)(sizeof(struct clk_pll_table)*table_size),
					GFP_KERNEL);

			if (!odin_dp2_clk_t)
				return -ENOMEM;

			for(i=0; i<table_size; i++){
				odin_dp2_clk_t[i].clk_rate = readl(sram_base + next_reg)*1000;
				odin_dp2_clk_t[i].reg_val = readl((sram_base+0x4) + next_reg);
				next_reg = next_reg + 0x8;
			}

			break;

		case COMM_PLL_CLK:
			odin_comm_clk_t = kzalloc((size_t)(sizeof(struct clk_pll_table)*table_size),
					GFP_KERNEL);

			if (!odin_dp2_clk_t)
				return -ENOMEM;

			for(i=0; i<table_size; i++){
				odin_comm_clk_t[i].clk_rate = readl(sram_base + next_reg)*1000;
				odin_comm_clk_t[i].reg_val = readl((sram_base+0x4) + next_reg);
				next_reg = next_reg + 0x8;
			}

			break;

		default:
			break;
	}

	iounmap(sram_base);

	return 0;
}

void odin_get_all_pll_table(void){
	get_odin_pll_table(CPU_PLL_CLK | CA15_PLL_CLK<<4);
	get_odin_pll_table(CPU_PLL_CLK | CA7_PLL_CLK<<4);
	get_odin_pll_table(MEM_PLL_CLK);
	get_odin_pll_table(GPU_PLL_CLK);
	get_odin_pll_table(ISP_PLL_CLK);
	get_odin_pll_table(DP2_PLL_CLK);
	get_odin_pll_table(COMM_PLL_CLK);
}

static void woden_cpu_clk_guard(void){

	void __iomem *cpu_base_reg;

	cpu_base_reg = ioremap(WODEN_CPU_CFG_BASE, SZ_4K);

	/*ca7 core can not handle upto 800Mhz. So we divid the clock*/
	writel(0x03FF0101, cpu_base_reg+0x104);
	writel(0xFFFF1001, cpu_base_reg+0x10C);

	/*update cpu divider*/
	writel(0x00000F71, cpu_base_reg+0x100);
	writel(0x06710100, cpu_base_reg+0x110);
	writel(0x0000FF71, cpu_base_reg+0x100);
	writel(0x3E710100, cpu_base_reg+0x110);

	iounmap((void *)cpu_base_reg);

}

int get_table_size(struct clk_pll_table *table){

	int i = 0;

	while(table[i].clk_rate != 0)
		i++;

	return i;
}

static void cpu_pdb_endisable(struct clk_hw *hw, int enable)
{
	struct lj_pll *pll = to_lj_pll(hw);
	int set = 0;
	u32 val;

	unsigned long flags = 0;

	set ^= enable;

	if(pll->lock && !((hw->clk)->secure_flag))
		spin_lock_irqsave(pll->lock, flags);

	if(!((hw->clk)->secure_flag)){
		val = readl(pll->reg);
	}
	else{
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

	if(!((hw->clk)->secure_flag)){
		writel(val, pll->reg);
	}else{
#ifdef CONFIG_ODIN_TEE
		tz_write(val, pll->p_reg);
#else
		writel(val, pll->reg);
#endif
	}

	if(pll->lock && !((hw->clk)->secure_flag))
		spin_unlock_irqrestore(pll->lock, flags);
}

static int ljpll_enable(struct clk_hw *hw)
{
	cpu_pdb_endisable(hw, 1);

	return 0;
}

static void ljpll_disable(struct clk_hw *hw)
{
	cpu_pdb_endisable(hw, 0);
}

static unsigned long ljpll_calc_rate(struct clk_hw *hw, u32 reg_val)
{
	struct lj_pll *pll = to_lj_pll(hw);
	unsigned long ll;
	int i;
	u32 ics_reg;
	u64 m = 0, od_fout = 0, pre_fd = 0, nsc = 0, npc = 0, od_fout_p = 0, pre_fd_p = 0;

	for(i=0; i<ARRAY_SIZE(pll_calc_bit); i++){
		if(pll->pll_type == pll_calc_bit[i].pll_type){
			m = (reg_val >> pll_calc_bit[i].b_shift[0]) & pll_calc_bit[i].bit_mask[0];
			if(pll->pll_type == ICS_PLL_CLK){
				ics_reg = readl(pll->reg+0x4);
				od_fout = (ics_reg >> pll_calc_bit[i].b_shift[1]) & pll_calc_bit[i].bit_mask[1];
				pre_fd = (ics_reg >> pll_calc_bit[i].b_shift[2]) & pll_calc_bit[i].bit_mask[2];
			}else{
				od_fout = (reg_val >> pll_calc_bit[i].b_shift[1]) & pll_calc_bit[i].bit_mask[1];
				pre_fd = (reg_val >> pll_calc_bit[i].b_shift[2]) & pll_calc_bit[i].bit_mask[2];
			}
			nsc = (reg_val >> pll_calc_bit[i].b_shift[3]) & pll_calc_bit[i].bit_mask[3];
			npc = (reg_val >> pll_calc_bit[i].b_shift[4]) & pll_calc_bit[i].bit_mask[4];
		}
	}

	od_fout_p = 1 << od_fout;
	pre_fd_p = 1 << pre_fd;

	ll = (unsigned long)(divisor_64(((((4*npc)+nsc)*pre_fd_p)*24000000), ((m+1) * od_fout_p)));

	return ll;
}

unsigned long ljpll_recalc(struct clk_hw *hw, unsigned long parent_rate)
{
	struct lj_pll *pll = to_lj_pll(hw);
	unsigned long ll;
	int i;
	u32 reg, ics_reg;
	u64 m = 0, od_fout = 0, pre_fd = 0, nsc = 0, npc = 0, od_fout_p = 0, pre_fd_p = 0;

	if(!((hw->clk)->secure_flag)){
		reg = readl(pll->reg);
	}
	else{
#ifdef CONFIG_ODIN_TEE
		reg = tz_read(pll->p_reg);
#else
		reg = readl(pll->reg);
#endif
	}

	for(i=0; i<ARRAY_SIZE(pll_calc_bit); i++){
		if(pll->pll_type == pll_calc_bit[i].pll_type){
			m = (reg >> pll_calc_bit[i].b_shift[0]) & pll_calc_bit[i].bit_mask[0];
			if(pll->pll_type == ICS_PLL_CLK){
				ics_reg = readl(pll->reg+0x4);
				od_fout = (ics_reg >> pll_calc_bit[i].b_shift[1]) & pll_calc_bit[i].bit_mask[1];
				pre_fd = (ics_reg >> pll_calc_bit[i].b_shift[2]) & pll_calc_bit[i].bit_mask[2];
			}else{
				od_fout = (reg >> pll_calc_bit[i].b_shift[1]) & pll_calc_bit[i].bit_mask[1];
				pre_fd = (reg >> pll_calc_bit[i].b_shift[2]) & pll_calc_bit[i].bit_mask[2];
			}
			nsc = (reg >> pll_calc_bit[i].b_shift[3]) & pll_calc_bit[i].bit_mask[3];
			npc = (reg >> pll_calc_bit[i].b_shift[4]) & pll_calc_bit[i].bit_mask[4];
		}
	}

	od_fout_p = 1 << od_fout;
	pre_fd_p = 1 << pre_fd;

	ll = (unsigned long)(divisor_64(((((4*npc)+nsc)*pre_fd_p)*parent_rate), ((m+1) * od_fout_p)));

	return ll;
}

long ljpll_round_rate(struct clk_hw *hw, unsigned long target_rate,
		unsigned long *parent_rate)
{
	struct lj_pll *pll = to_lj_pll(hw);
	struct clk_pll_table *cpt;

	switch (pll->mach_type){
		case MACH_WODEN:
			if(pll->pll_type == CPU_PLL_CLK){
				cpt = ljpll_search_table(woden_cpu_clk_t, ARRAY_SIZE(woden_cpu_clk_t),
				target_rate);
				s_req_rate = cpt->clk_rate;
			}else{
				cpt = ljpll_search_table(woden_comm_clk_t, ARRAY_SIZE(woden_comm_clk_t),
				target_rate);
				s_req_rate = cpt->clk_rate;
			}

			break;
		case MACH_ODIN:
			if(pll->pll_type == CPU_PLL_CLK){
				if(0==strcmp(hw->clk->name, "ca15_pll_clk"))
					cpt = ljpll_search_table(odin_ca15_clk_t, get_table_size(odin_ca15_clk_t),
					target_rate);
				else
					cpt = ljpll_search_table(odin_ca7_clk_t, get_table_size(odin_ca7_clk_t),
					target_rate);
				s_req_rate = cpt->clk_rate;
			}else if(pll->pll_type == MEM_PLL_CLK){
				cpt = ljpll_search_table(odin_mem_clk_t, get_table_size(odin_mem_clk_t),
					target_rate);
			}else if(pll->pll_type == ISP_PLL_CLK){
				cpt = ljpll_search_table(odin_isp_clk_t, get_table_size(odin_isp_clk_t),
					target_rate);
				s_req_rate = cpt->clk_rate;
			}else{
				cpt = ljpll_search_table(odin_comm_clk_t, get_table_size(odin_comm_clk_t),
				target_rate);
				s_req_rate = cpt->clk_rate;
			}

			break;
		default:
			return -1;
	}

	return target_rate;
}

static void update_nsecure_divreg(struct lj_pll *pll)
{
	u32 val;

	val = readl(pll->div_ud_reg);
	val &= ~BIT(pll->div_ud_bit);
	writel(val, pll->div_ud_reg);
	val |= BIT(pll->div_ud_bit);
	writel(val, pll->div_ud_reg);
}

static int chg_nsecure_pllreg(struct clk_hw *hw, u32 reg_val, u32 div_val){

	struct lj_pll *pll = to_lj_pll(hw);
	u32 val, lock_time = 0x0;

	/*Bypass the src clock*/
	val = readl(pll->sel_reg);

	val &= ~(pll_mask << pll->byp_shift);
	val |= 1 << pll->byp_shift;

	writel(val, pll->sel_reg);

	/*set pll reg*/
	writel(reg_val, pll->reg);

	if(pll->lock_time_reg != NULL){
		while(!(lock_time >> 31))
		{
			lock_time = readl(pll->lock_time_reg);
		}
	}else
		udelay(200);

	if((pll->mach_type==MACH_WODEN)&&(pll->pll_type==CPU_PLL_CLK))
		woden_cpu_clk_guard();

	if((pll->div_reg!=NULL)&&(pll->p_div_reg!=0x0))
	{
		val = readl(pll->div_reg);
		val &= ~((((1 << pll->div_width) - 1)) << pll->div_shift);
		val |= div_val << pll->div_shift;

		writel(val, pll->div_reg);

		if(pll->div_ud_reg!=NULL)
			update_nsecure_divreg(pll);

		pll->dvfs_flag = DVFS_PLL_DIV;
	}

	/*change to src clock*/
	val = readl(pll->sel_reg);
	val &= ~(pll_mask << pll->byp_shift);
	val |= 0 << pll->byp_shift;

	writel(val, pll->sel_reg);

	return 0;
}

static int chg_secure_pllreg(struct clk_hw *hw, unsigned long clk_rate, u32 reg_val){

	struct lj_pll *pll = to_lj_pll(hw);
	u32 lock_byp_reg = 0x0, scp_index = 0x0, divud_req_rate=0x0;
	u32 val, lock_time = 0x0;

	if(clk_rate == 0)
		return 0;

	if(0==strcmp(hw->clk->name, "ca15_pll_clk") || 0==strcmp(hw->clk->name, "ca7_pll_clk") ||
		0==strcmp(hw->clk->name, "mem_pll_clk")	|| 0==strcmp(hw->clk->name, "gpu_pll_clk")){

		lock_byp_reg = ((pll->p_lock_time_reg&0xFFFF)<<16) | (pll->p_sel_reg&0xFFFF);
		divud_req_rate = (pll->p_div_ud_reg<<16) | (clk_rate/1000000);
		scp_index = pll->byp_shift | (pll->div_shift<<8) | (pll->div_width<<16) |
		(pll->div_ud_bit<<20) | (pll->pll_type << 28);

		if(0==strcmp(hw->clk->name, "ca15_pll_clk"))
			scp_index = ((scp_index) & 0x1FFFFFFF)|(CA15_PLL_CLK<<31);
		else if(0==strcmp(hw->clk->name, "ca7_pll_clk"))
			scp_index = ((scp_index) & 0x1FFFFFFF)|(CA7_PLL_CLK<<31);
		else
			scp_index = scp_index;

		mb_clk_set_pll(pll->p_reg, lock_byp_reg, pll->p_div_reg, divud_req_rate,
			scp_index);

		pll->dvfs_flag = DVFS_PLL_DIV;

	}else{
		/*Bypass the src clock*/
#ifdef CONFIG_ODIN_TEE
		val = tz_read(pll->p_sel_reg);
#else
		val = readl(pll->sel_reg);
#endif

		val &= ~(pll_mask << pll->byp_shift);
		val |= 1 << pll->byp_shift;

#ifdef CONFIG_ODIN_TEE
		tz_write(val, pll->p_sel_reg);
#else
		writel(val, pll->sel_reg);
#endif

		/*set pll reg*/
#ifdef CONFIG_ODIN_TEE
		tz_write(reg_val, pll->p_reg);
#else
		writel(reg_val, pll->reg);
#endif

		if(pll->lock_time_reg != NULL){
			while(!(lock_time >> 31))
			{
#ifdef CONFIG_ODIN_TEE
				lock_time = tz_read(pll->p_lock_time_reg);
#else
				lock_time = readl(pll->lock_time_reg);
#endif
			}
		}else{
			udelay(200);
		}

		/*change to src clock*/
#ifdef CONFIG_ODIN_TEE
		val = tz_read(pll->p_sel_reg);
		val &= ~(pll_mask << pll->byp_shift);
		val |= 0 << pll->byp_shift;
		tz_write(val, pll->p_sel_reg);
#else
		val = readl(pll->sel_reg);
		val &= ~(pll_mask << pll->byp_shift);
		val |= 0 << pll->byp_shift;
		writel(val, pll->sel_reg);
#endif
		pll->dvfs_flag = DVFS_PLL_ONLY;
	}

	return 0;

}

int ljpll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long best_rate)
{
	struct lj_pll *pll = to_lj_pll(hw);
	struct clk_pll_table *cpt;
	u32 reg_comp;
	unsigned long flags = 0;
	int div = 0;

	switch (pll->mach_type){
		case MACH_WODEN:
			if(pll->pll_type == CPU_PLL_CLK)
				cpt = ljpll_search_table(woden_cpu_clk_t, ARRAY_SIZE(woden_cpu_clk_t), rate);
			else
				cpt = ljpll_search_table(woden_comm_clk_t, ARRAY_SIZE(woden_comm_clk_t), rate);

			break;

		case MACH_ODIN:
			if(!((hw->clk)->secure_flag)){
				reg_comp = readl(pll->reg);
			}else{
#ifdef CONFIG_ODIN_TEE
				reg_comp = tz_read(pll->p_reg);
#else
				reg_comp = readl(pll->reg);
#endif
			}

			if(pll->pll_type == CPU_PLL_CLK){
				if(0==strcmp(hw->clk->name, "ca15_pll_clk")){
					cpt = ljpll_search_table(odin_ca15_clk_t, get_table_size(odin_ca15_clk_t), rate);
				}else{
					cpt = ljpll_search_table(odin_ca7_clk_t, get_table_size(odin_ca7_clk_t), rate);
				}
				if(reg_comp == cpt->reg_val){
					pll->dvfs_flag = DVFS_PLL_DIV;
					goto out;
				}
			}else if(pll->pll_type == MEM_PLL_CLK){
				cpt = ljpll_search_table(odin_mem_clk_t, get_table_size(odin_mem_clk_t), rate);
			}else if(pll->pll_type == GPU_PLL_CLK){
				cpt = ljpll_search_table(odin_gpu_clk_t, get_table_size(odin_gpu_clk_t), rate);
			}else if(pll->pll_type == ISP_PLL_CLK){
				cpt = ljpll_search_table(odin_isp_clk_t, get_table_size(odin_isp_clk_t), rate);
			}else if(pll->pll_type == DP2_PLL_CLK){
				cpt = ljpll_search_table(odin_dp2_clk_t, get_table_size(odin_dp2_clk_t), rate);
			}else{
				cpt = ljpll_search_table(odin_comm_clk_t, get_table_size(odin_comm_clk_t), rate);
			}

			//if(reg_comp == cpt->reg_val)
			//	goto out;

			break;

		default:
			return -1;
	}


	if(pll->lock && !((hw->clk)->secure_flag))
		spin_lock_irqsave(pll->lock, flags);

	if(!((hw->clk)->secure_flag)){
		div = ljpll_calc_rate(hw, cpt->reg_val)/cpt->clk_rate;
		chg_nsecure_pllreg(hw, cpt->reg_val, div-1);
	}else{
		chg_secure_pllreg(hw, cpt->clk_rate, cpt->reg_val);
	}

	if(pll->lock && !((hw->clk)->secure_flag))
		spin_unlock_irqrestore(pll->lock, flags);

	if(pll->pll_type == MEM_PLL_CLK){
		//dss_underrun_handler();
	}

out:
	return 0;
}

struct clk_ops lj_pll_ops = {
	.enable = ljpll_enable,
	.disable = ljpll_disable,
	.recalc_rate = ljpll_recalc,
	.set_rate = ljpll_set_rate,
	.round_rate = ljpll_round_rate,
};

static struct clk *_register_ljpll(struct device *dev, const char *name,
		const char *parent_name, int flags, void __iomem *reg, void __iomem *sel_reg,
		void __iomem *lock_time_reg, void __iomem *div_reg, void __iomem *div_ud_reg, u32 p_reg,
		u32 p_sel_reg, u32 p_lock_time_reg, u32 p_div_reg, u32 p_div_ud_reg,  u8 byp_shift,
		u8 pdb_shift, u8 div_shift, u8 div_width, u8 div_ud_bit, u8 mach_type, u8 pll_type,
		u8 secure_flag, spinlock_t *lock)
{

	struct lj_pll *pll;
	struct clk *clk;
	struct clk_init_data init;

	pll = kzalloc(sizeof(struct lj_pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &lj_pll_ops;
	init.flags = flags;
	init.parent_names = (parent_name ? &parent_name: NULL);
	init.num_parents = (parent_name ? 1 : 0);

	pll->reg = reg;
	pll->sel_reg = sel_reg;
	pll->lock_time_reg = lock_time_reg;
	pll->div_reg = div_reg;
	pll->div_ud_reg = div_ud_reg;
	pll->p_reg = p_reg;
	pll->p_sel_reg = p_sel_reg;
	pll->p_lock_time_reg = p_lock_time_reg;
	pll->p_div_reg = p_div_reg;
	pll->p_div_ud_reg = p_div_ud_reg;
	pll->byp_shift = byp_shift;
	pll->pdb_shift = pdb_shift;
	pll->div_shift = div_shift;
	pll->div_width = div_width;
	pll->div_ud_bit = div_ud_bit;
	pll->mach_type = mach_type;
	pll->pll_type = pll_type;
	pll->dvfs_flag = DVFS_DIV_ONLY;
	pll->lock = lock;
	pll->hw.init = &init;

	clk = __odin_clk_register(dev, &pll->hw, reg, p_reg, secure_flag);

	if (IS_ERR(clk))
		kfree(pll);

	return clk;
}

struct clk *odin_clk_register_ljpll(struct device *dev, const char *name,
		const char *parent_name, int flags, void __iomem *reg, void __iomem *sel_reg,
		void __iomem *lock_time_reg, u32 p_reg, u32 p_sel_reg, u32 p_lock_time_reg, u8 byp_shift,
		u8 pdb_shift, u8 mach_type, u8 pll_type, u8 secure_flag, spinlock_t *lock)
{
	return _register_ljpll(dev, name, parent_name, flags, reg, sel_reg, lock_time_reg, NULL, NULL,
			p_reg, p_sel_reg, p_lock_time_reg, 0x0, 0x0, byp_shift, pdb_shift, 0, 0, 0, mach_type,
			pll_type, secure_flag, lock);
}

struct clk *odin_clk_register_ljpll_dvfs(struct device *dev, const char *name,
		const char *parent_name, int flags, void __iomem *reg, void __iomem *sel_reg,
		void __iomem *lock_time_reg, void __iomem *div_reg, void __iomem *div_ud_reg, u32 p_reg,
		u32 p_sel_reg, u32 p_lock_time_reg, u32 p_div_reg, u32 p_div_ud_reg,  u8 byp_shift,
		u8 pdb_shift, u8 div_shift, u8 div_width, u8 div_ud_bit, u8 mach_type, u8 pll_type,
		u8 secure_flag, spinlock_t *lock)
{
	return _register_ljpll(dev, name, parent_name, flags, reg, sel_reg, lock_time_reg, div_reg,
			div_ud_reg, p_reg, p_sel_reg, p_lock_time_reg, p_div_reg, p_div_ud_reg, byp_shift,
			pdb_shift, div_shift, div_width, div_ud_bit, mach_type, pll_type, secure_flag, lock);
}
