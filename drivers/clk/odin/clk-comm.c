#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/clk-private.h>
#include <linux/clkdev.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/odin_mailbox.h>
#include <linux/debugfs.h>
#include <linux/odin_clk.h>

#include <video/odindss.h>


static DEFINE_MUTEX(prepare_lock);
static struct task_struct *prepare_owner;
static int prepare_refcnt;

static HLIST_HEAD(clk_list);

int s_r_flag;

static DEFINE_SPINLOCK(mem_dfs_lock);
static DECLARE_COMPLETION(mem_dfs_completion);
static int mem_dfs_ref;
static void *last_get;
static void *last_put;
#ifdef CONFIG_PM_SLEEP_DEBUG
u32 debug_suspend;
#endif


static void start_mem_dfs(unsigned long *flags)
{
	do {
		spin_lock_irqsave(&mem_dfs_lock, *flags);
		if (!mem_dfs_ref)
			break;
		init_completion(&mem_dfs_completion);
		spin_unlock_irqrestore(&mem_dfs_lock, *flags);

		if (wait_for_completion_timeout(&mem_dfs_completion,
						msecs_to_jiffies(1000)) == 0) {
			WARN(1, "wait timeout last_get %pS last_put %pS\n",
				last_get, last_put);
			spin_lock_irqsave(&mem_dfs_lock, *flags);
			mem_dfs_ref = 0;
			break;
		}
	} while (true);
}

static void end_mem_dfs(unsigned long *flags)
{
	spin_unlock_irqrestore(&mem_dfs_lock, *flags);
}

void get_mem_dfs(void)
{
	unsigned long flags;

	spin_lock_irqsave(&mem_dfs_lock, flags);
	mem_dfs_ref++;
	spin_unlock_irqrestore(&mem_dfs_lock, flags);

	last_get = __builtin_return_address(0);
}
EXPORT_SYMBOL(get_mem_dfs);

void put_mem_dfs(void)
{
	unsigned long flags;

	spin_lock_irqsave(&mem_dfs_lock, flags);
	if (!mem_dfs_ref) {
		pr_info("%pS called %s with ref count %d !!!\n",
			__builtin_return_address(0), __func__, mem_dfs_ref);
		goto out;
	}
	mem_dfs_ref--;
	if (!mem_dfs_ref)
		complete_all(&mem_dfs_completion);
out:
	spin_unlock_irqrestore(&mem_dfs_lock, flags);

	last_put = __builtin_return_address(0);
}
EXPORT_SYMBOL(put_mem_dfs);

int get_mem_dfs_ref_cnt(void)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&mem_dfs_lock, flags);
	ret = mem_dfs_ref;
	spin_unlock_irqrestore(&mem_dfs_lock, flags);

	return ret;
}

u64 divisor_64(u64 dividend, u64 divisor)
{

	u64 i = 0, sign = 0, div = 0;

	if (dividend < 0){
		dividend = ~dividend + 1;
		sign++;
	}

	if (divisor < 0){
		divisor = ~divisor + 1;
		sign++;
	}

	if (dividend < divisor){
		div = 0;
	}
	else{
		for (i=0; i<64; i++){

			if (dividend < (divisor<<i)){
				if( i > 0 ) i--;
				break;
			}else if (dividend == (divisor<<i)){
				break;
			}
		}

		div = 0;

		for (; i>=0; i--){
			if (dividend < divisor)
				break;

			if (dividend >= (divisor<<i)){
				dividend -= (divisor<<i);
				div += (1<<i);
			}
		}
	}

	if (sign & 0x01){
		div = ~div + 1;
	}

	return div;
}

int clk_reg_store(u32 base_addr, struct reg_save *rs, int count, size_t size,
	u8 secure_flag)
{
	void __iomem *va_addr;

	if (!secure_flag){
		va_addr = ioremap(base_addr, size);

		for (; count > 0; count--, rs++){
			rs->reg_val = __raw_readl(va_addr+rs->off_set);
			/* this will be erased after suspend&resume function is merged!!*/
			/*writel(0x0, va_addr+rs->off_set);*/
		}
		iounmap(va_addr);
	}else{
		for (; count > 0; count--, rs++){
#ifdef CONFIG_ODIN_TEE
			rs->reg_val = tz_read(base_addr+rs->off_set);
#else
			va_addr = ioremap(base_addr, size);
			rs->reg_val = __raw_readl(va_addr+rs->off_set);
			iounmap(va_addr);
#endif
			/* this will be erased after suspend&resume function is merged!! */
			/*tz_write(0x0, base_addr+rs->off_set);*/
		}
	}
	return 0;
}

int clk_reg_restore(u32 base_addr, struct reg_save *rs, int count, size_t size,
	u8 secure_flag)
{
	void __iomem *va_addr;

	if(!secure_flag){
		va_addr = ioremap(base_addr, size);
		for (; count > 0; count--, rs++){
			/*printk("restore %p (restore %08x, was %08x)\n",
		       (va_addr+rs->off_set), rs->reg_val,
			   __raw_readl(va_addr+rs->off_set));*/
			__raw_writel(rs->reg_val, va_addr+rs->off_set);
		}
		iounmap(va_addr);
	}else{
		for (; count > 0; count--, rs++){
#ifdef CONFIG_ODIN_TEE
			/*printk("restore %x (restore %08x, was %08x)\n",
		       (base_addr+rs->off_set), rs->reg_val,
			   tz_read(base_addr+rs->off_set));*/
			tz_write(rs->reg_val, base_addr+rs->off_set);
#else
			va_addr = ioremap(base_addr, size);
			__raw_writel(rs->reg_val, va_addr+rs->off_set);
			iounmap(va_addr);
#endif
		}
	}
	return 0;
}

int clk_change_parent(struct clk *clk, char *parent_name)
{
	struct clk *temp_clk;
	struct device_node *np, *from;

	from = of_find_node_by_name(NULL, "clocks");

	for (np = of_get_next_child(from, NULL); np; np=of_get_next_child(from, np)){
		temp_clk = clk_get(NULL, np->name);

		do{
			temp_clk = temp_clk->parent;
			if (temp_clk == NULL){
				break;
			}
			if (0 == strcmp(temp_clk->name, parent_name)){
				clk_set_parent(clk, temp_clk);
				return 0;
			}
		}while (1);
	}

	return -EINVAL;
}

int clk_set_par_rate(struct clk *clk, char *parent_name, unsigned long req_rate)
{
	int ret;

	ret = clk_change_parent(clk, parent_name);

	if (ret != 0){
		pr_warn("%s: failed to change %s parent to %s\n", __func__,
				clk->name, parent_name);
	}

	ret = clk_set_rate(clk, req_rate);

	if (ret != 0){
		pr_warn("%s: failed to set %s rate\n", __func__,
				clk->name);
	}

	return ret;
}

void print_clk_info(void)
{
	struct clk *clk, *clk_temp, *clk_parent;
	struct device_node *np, *from;
	int cnt =0;

	from = of_find_node_by_name(NULL, "clocks");

	printk("     clock name     clock rate    state   div      p_clock name     p_clock rate\n");
	printk("====================================================================================\n");

	for (np = of_get_next_child(from, NULL); np; np=of_get_next_child(from, np)){
		clk_temp = clk = clk_get(NULL, np->name);
		do{
			clk_parent = clk_get_parent(clk_temp);
			if (clk_parent == NULL)
				break;

			if (0==strcmp(__clk_get_name(clk_parent), "osc_clk")){
#ifdef CONFIG_PM_SLEEP_DEBUG
				if(clk->enable_count<=0 && (likely(debug_suspend)))
					break;
#endif
				printk("%14s %14luHZ%7s     x%d %16s       %luHz\n", __clk_get_name(clk),
				clk_get_rate(clk), clk->enable_count>0?"ON":"OFF",
				(int)(clk_get_rate(clk_temp)/clk_get_rate(clk)), __clk_get_name(clk_temp),
				clk_get_rate(clk_temp));
				cnt++;
				break;
			}
			clk_temp = clk_get_parent(clk_temp);

		}while (1);
	}
#ifdef CONFIG_PM_SLEEP_DEBUG
	if(likely(debug_suspend))
		printk("Enable clock count : %d\n " ,cnt );
#endif
}
struct clk_pll_table *ljpll_search_table(struct clk_pll_table *table,
		int array_size, unsigned long rate)
{

	int i;

	struct clk_pll_table *cur = table;
	struct clk_pll_table *prev;

	for (i = 0; i < array_size; i++, cur++)
		if (cur->clk_rate <= rate)
			break;

	if (i) {
		prev = cur - 1;

		if (!cur->clk_rate || ((prev->clk_rate - rate) < (rate - cur->clk_rate)))
			cur = prev;
	}

	return cur;
}

int mb_clk_probe(void)
{
	s_r_flag = 1;
	return 0;
}

int mb_get_pll_table(u32 pll_type)
{
	u32 data[7] = {0, };
	int ret;

	data[0] = 0x0;
	data[1] = 0x60000007;
	data[2] = pll_type;

	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_CLK, data, 1000);

	if (ret < 0) {
		ret = -ETIMEDOUT;
		pr_err("%s: Failed to recieve clk reg_write data: errno(%d)\n",
				__func__, ret);

		return -1;
	}

	return data[2];
}

int mb_volt_set(u32 addr, u32 val)
{
	u32 data[7] = {0, };
	int ret;

	data[0] = 0x0;
	data[1] = 0x60000005;
	data[2] = addr;
	data[3] = val;

	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_CLK, data, 1000);

	if (ret < 0) {
		ret = -ETIMEDOUT;
		pr_err("%s: Failed to recieve clk reg_write data: errno(%d)\n",
				__func__, ret);

		return -1;
	}

	return 0;
}

int mb_dvfs_item_set(u32 table_index, u32 freq, u32 voltage, u32 pll)
{
	u32 data[7] = {0, };
	int ret;

	data[0] = 0x0;
	data[1] = 0x60000006;
	data[2] = table_index;
	data[3] = freq;
	data[4] = voltage;
	data[5] = pll;

	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_CLK, data, 1000);

	if (ret < 0) {
		ret = -ETIMEDOUT;
		pr_err("%s: Failed to recieve clk reg_write data: errno(%d)\n",
				__func__, ret);

		return -1;
	}

	return 0;
}

u32 mb_clk_set_div(u32 div_reg, u32 update_reg, u32 sft_wid_ud_bit, u32 div_val,
	unsigned long req_rate)
{
	u32 data[7] = {0, };
	int ret;

	data[0] = 0x0;
	data[1] = 0x60000004;
	data[2] = div_reg;
	data[3] = update_reg;
	data[4] = sft_wid_ud_bit;
	data[5] = div_val;
	data[6] = req_rate;

	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_CLK, data, 1000);

	if (ret < 0) {
		ret = -ETIMEDOUT;
		pr_err("%s: Failed to recieve clk reg_write data: errno(%d)\n",
				__func__, ret);

		return -1;
	}

	return data[2];
}

u32 mb_clk_set_pll(u32 pll_reg, u32 lock_byp_reg, u32 div_reg,
	u32 divud_req_rate, u32 scp_index)
{
	u32 data[7] = {0, };
	int ret;
	u8 pll_type = (scp_index >> 28) & 0xff;

	data[0] = 0x0;
	data[1] = 0x60000003;
	data[2] = pll_reg;
	data[3] = lock_byp_reg;
	data[4] = div_reg;
	data[5] = divud_req_rate;
	data[6] = scp_index;

	if (s_r_flag == 0)
		return 0;

	if (pll_type == MEM_PLL_CLK) {
#ifdef CONFIG_PANEL_LGLH600WF2
		/* command mode */
		unsigned long flags;
		start_mem_dfs(&flags);
#else
		/* video   mode */
#ifdef CONFIG_ODIN_DSS
		odin_dram_clk_change_before();
#endif
#endif
		/* handle mem freq change directly in ISR */
		data[0] |= (1 << 28);

#ifdef CONFIG_ODIN_PL320_SW_WORKAROUND_MEM_DFS_BUS_LOCK
		ret = ipc_call_fast_mem(data);
#else
		ret = ipc_call_fast(data);
#endif

#ifdef CONFIG_PANEL_LGLH600WF2
		end_mem_dfs(&flags);
#else
#ifdef CONFIG_ODIN_DSS
		odin_dram_clk_change_after();
#endif
#endif
	} else
		ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_CLK, data, 1000);

	if (ret < 0) {
		ret = -ETIMEDOUT;
		pr_err("%s: Failed to recieve clk reg_write data: errno(%d)\n",
				__func__, ret);

		return -1;
	}

	return data[2];
}

static struct clk *__odin_clk_init_parent(struct clk *clk)
{
	struct clk *ret = NULL;
	u8 index;

	if (!clk->num_parents)
		goto out;

	if (clk->num_parents == 1) {
		if (IS_ERR_OR_NULL(clk->parent))
			ret = clk->parent = __clk_lookup(clk->parent_names[0]);
		ret = clk->parent;
		goto out;
	}

	if (!clk->ops->get_parent) {
		WARN(!clk->ops->get_parent,
			"%s: multi-parent clocks must implement .get_parent\n",
			__func__);
		goto out;
	};

	index = clk->ops->get_parent(clk->hw);
	//clk->ops->set_parent(clk->hw, index);

	if (!clk->parents)
		ret = __clk_lookup(clk->parent_names[index]);
	else if (!clk->parents[index])
		ret = clk->parents[index] =
			__clk_lookup(clk->parent_names[index]);
	else
		ret = clk->parents[index];

	__clk_reparent(clk, ret);

out:
	return ret;
}

int __clk_reg_update(struct device *dev, struct clk *clk)
{
	int ret = 0;

	if (!clk)
		return -EINVAL;

	mutex_lock(&prepare_lock);

	__clk_lookup(clk->name);

	clk->parent = __odin_clk_init_parent(clk);

	if (clk->ops->recalc_rate)
		clk->rate = clk->ops->recalc_rate(clk->hw,
				__clk_get_rate(clk->parent));
	else if (clk->parent)
		clk->rate = clk->parent->rate;
	else
		clk->rate = 0;

	mutex_unlock(&prepare_lock);

	return ret;
}

static int _clk_register(struct device *dev, struct clk_hw *hw,
	struct clk *clk, void __iomem *reg, u32 p_reg, u8 secure_flag)
{
	int i, ret;

	clk->name = kstrdup(hw->init->name, GFP_KERNEL);
	if (!clk->name) {
		pr_err("%s: could not allocate clk->name\n", __func__);
		ret = -ENOMEM;
		goto fail_name;
	}
	clk->ops = hw->init->ops;
	clk->hw = hw;
	clk->flags = hw->init->flags;
	clk->num_parents = hw->init->num_parents;
	clk->secure_flag = secure_flag;

	hw->clk = clk;

	/* allocate local copy in case parent_names is __initdata */
	clk->parent_names = kzalloc((sizeof(char*) * clk->num_parents),
			GFP_KERNEL);

	if (!clk->parent_names) {
		pr_err("%s: could not allocate clk->parent_names\n", __func__);
		ret = -ENOMEM;
		goto fail_parent_names;
	}


	/* copy each string name in case parent_names is __initdata */
	for (i = 0; i < clk->num_parents; i++) {
		clk->parent_names[i] = kstrdup(hw->init->parent_names[i],
						GFP_KERNEL);
		if (!clk->parent_names[i]) {
			pr_err("%s: could not copy parent_names\n", __func__);
			ret = -ENOMEM;
			goto fail_parent_names_copy;
		}
	}

	ret = __clk_init(dev, clk);
	if (!ret)
		return 0;

fail_parent_names_copy:
	while (--i >= 0)
		kfree(clk->parent_names[i]);
	kfree(clk->parent_names);
fail_parent_names:
	kfree(clk->name);
fail_name:
	return ret;
}

struct clk *__odin_clk_register(struct device *dev, struct clk_hw *hw,
	void __iomem *reg, u32 p_reg, u8 secure_flag)
{
	int ret;
	struct clk *clk;

	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	if (!clk) {
		pr_err("%s: could not allocate clk\n", __func__);
		ret = -ENOMEM;
		goto fail_out;
	}

	ret = _clk_register(dev, hw, clk, reg, p_reg, secure_flag);
	if (!ret)
		return clk;

	kfree(clk);
fail_out:
	return ERR_PTR(ret);
}


/***           locking             ***/
static void clk_prepare_lock(void)
{
	if (!mutex_trylock(&prepare_lock)) {
		if (prepare_owner == current) {
			prepare_refcnt++;
			return;
		}
		mutex_lock(&prepare_lock);
	}
	WARN_ON_ONCE(prepare_owner != NULL);
	WARN_ON_ONCE(prepare_refcnt != 0);
	prepare_owner = current;
	prepare_refcnt = 1;
}

static void clk_prepare_unlock(void)
{
	WARN_ON_ONCE(prepare_owner != current);
	WARN_ON_ONCE(prepare_refcnt == 0);

	if (--prepare_refcnt)
		return;
	prepare_owner = NULL;
	mutex_unlock(&prepare_lock);
}

/***        debugfs support        ***/
static struct dentry *rootdir;
static int inited = 0;

static void clk_summary_show_one(struct seq_file *s, struct clk *c, int level)
{
	struct clk *parent_clk;
	char str[16];
	int div_val;
	if (!c)
		return;

	if (NULL != strstr(c->name, "div")){
		parent_clk = clk_get_parent(c);
		div_val = (int)(clk_get_rate(parent_clk)/clk_get_rate(c));
		sprintf(str, "x%d", div_val);
	}

	seq_printf(s, "%*s%-*s \t %d \t\t %s \t %luhz",
		   level * 3 + 1, "",
		   30 - level * 3, c->name,
		   c->enable_count,
		   strstr(c->name, "div")!=NULL?str:"" ,
		   c->rate);
	seq_printf(s, "\n");
}

static void clk_summary_show_subtree(struct seq_file *s, struct clk *c,
				     int level)
{
	struct clk *child;

	if (!c)
		return;

	clk_summary_show_one(s, c, level);

	hlist_for_each_entry(child, &c->children, child_node)
		clk_summary_show_subtree(s, child, level + 1);
}

static int clk_summary_show(struct seq_file *s, void *data)
{
	struct clk *c;

	seq_printf(s, "   clock_name\t\t  clk_status\t div\trate\n");
	seq_printf(s, "-------------------------------------");
	seq_printf(s, "----------------------------------------\n");

	clk_prepare_lock();

	hlist_for_each_entry(c, &clk_list, child_node)
		clk_summary_show_subtree(s, c, 0);

	clk_prepare_unlock();

	return 0;
}


static int clk_summary_open(struct inode *inode, struct file *file)
{
	return single_open(file, clk_summary_show, inode->i_private);
}

static const struct file_operations clk_summary_fops = {
	.open		= clk_summary_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void clk_dump_one(struct seq_file *s, struct clk *c, int level)
{
	if (!c)
		return;

	seq_printf(s, "\"%s\": { ", c->name);
	seq_printf(s, "\"enable_count\": %d,", c->enable_count);
	seq_printf(s, "\"prepare_count\": %d,", c->prepare_count);
	seq_printf(s, "\"rate\": %lu", c->rate);
}

static void clk_dump_subtree(struct seq_file *s, struct clk *c, int level)
{
	struct clk *child;

	if (!c)
		return;

	clk_dump_one(s, c, level);

	hlist_for_each_entry(child, &c->children, child_node) {
		seq_printf(s, ",");
		clk_dump_subtree(s, child, level + 1);
	}

	seq_printf(s, "}");
}

static int clk_dump(struct seq_file *s, void *data)
{
	struct clk *c;
	bool first_node = true;

	seq_printf(s, "{");

	clk_prepare_lock();

	hlist_for_each_entry(c, &clk_list, child_node) {
		if (!first_node)
			seq_printf(s, ",");
		first_node = false;
		clk_dump_subtree(s, c, 0);
	}

	clk_prepare_unlock();

	seq_printf(s, "}");
	return 0;
}


static int clk_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, clk_dump, inode->i_private);
}

static const struct file_operations clk_dump_fops = {
	.open		= clk_dump_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/* caller must hold prepare_lock */
static int clk_debug_create_one(struct clk *clk, struct dentry *pdentry)
{
	struct dentry *d;
	int ret = -ENOMEM;

	if (!clk || !pdentry) {
		ret = -EINVAL;
		goto out;
	}

	d = debugfs_create_dir(clk->name, pdentry);
	if (!d)
		goto out;

	clk->dentry = d;

	d = debugfs_create_u32("clk_rate", S_IRUGO, clk->dentry,
			(u32 *)&clk->rate);
	if (!d)
		goto err_out;

	d = debugfs_create_x32("clk_flags", S_IRUGO, clk->dentry,
			(u32 *)&clk->flags);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("clk_prepare_count", S_IRUGO, clk->dentry,
			(u32 *)&clk->prepare_count);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("clk_enable_count", S_IRUGO, clk->dentry,
			(u32 *)&clk->enable_count);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("clk_notifier_count", S_IRUGO, clk->dentry,
			(u32 *)&clk->notifier_count);
	if (!d)
		goto err_out;

	ret = 0;
	goto out;

err_out:
	debugfs_remove(clk->dentry);
out:
	return ret;
}

/* caller must hold prepare_lock */
static int clk_debug_create_subtree(struct clk *clk, struct dentry *pdentry)
{
	struct clk *child;
	int ret = -EINVAL;

	if (!clk || !pdentry)
		goto out;

	ret = clk_debug_create_one(clk, pdentry);

	if (ret)
		goto out;

	hlist_for_each_entry(child, &clk->children, child_node)
		clk_debug_create_subtree(child, clk->dentry);

	ret = 0;
out:
	return ret;
}

static int __init clk_debug_init(void)
{
	struct clk *clk;
	struct dentry *d;

	clk_list = get_clk_list();

	rootdir = debugfs_create_dir("clk", NULL);

	if (!rootdir)
		return -ENOMEM;

	d = debugfs_create_file("clk_summary", S_IRUGO, rootdir, NULL,
				&clk_summary_fops);
	if (!d)
		return -ENOMEM;

	clk_prepare_lock();

	hlist_for_each_entry(clk, &clk_list, child_node)
		clk_debug_create_subtree(clk, rootdir);

	inited = 1;

	clk_prepare_unlock();

	return 0;
}
#ifndef CONFIG_ODIN_TEE
late_initcall(clk_debug_init);
#endif



#ifdef CONFIG_PM_SLEEP_DEBUG
void clock_debug_print_enabled(void)
{
	if (likely(!debug_suspend))
		return;
	print_clk_info();
}
#endif


