#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/string.h>
#include <linux/odin_clk.h>

#define to_lj_pll(_hw) container_of(_hw, struct lj_pll, hw)

static DEFINE_SPINLOCK(lock_test);

int cur_clk = -1;

struct clk_attr {
    struct attribute attr;
    char *clk_name;
	int value;
};

static struct clk_attr get_clk = {
    .attr.name="get_clk",
    .attr.mode = 0555,
    .value = 0,
};

static struct clk_attr set_clk = {
    .attr.name="set_clk",
    .attr.mode = 0644,
    .value = 2,
};

static struct clk_attr set_dvfs_item = {
    .attr.name="set_dvfs_item",
    .attr.mode = 0644,
    .value = 3,
};

static struct clk_attr set_enable = {
    .attr.name="set_enable",
    .attr.mode = 0644,
    .value = 4,
};

static struct clk_attr lockup = {
    .attr.name="lockup",
    .attr.mode = 0644,
    .value = 5,
};

static struct attribute * clkattr[] = {
    &get_clk.attr,
    &set_clk.attr,
    &set_dvfs_item.attr,
    &set_enable.attr,
    &lockup.attr,
    NULL
};

static u32 string_to_hex(char *input)
{
	unsigned int result = 0;
	int temp ;

	if ('0' == *input && 'x' == *(input+1))
	{
	input+=2;
	while (*input) {
		result = result << 4;
		if (temp=(*input-'0'),(temp>=0 && temp <=9)) result|=temp;
		else if (temp=(*input-'A'),(temp>=0 && temp <=5)) result|=(temp+10);
		else if (temp=(*input-'a'),(temp>=0 && temp <=5)) result|=(temp+10);
		else break;
		++input;
		}
	}
	return result;
}

static ssize_t show_get_clock_rate(struct kobject *kobj, struct attribute *attr,
        char *buf)
{
    struct clk_attr *a = container_of(attr, struct clk_attr, attr);
	struct clk *clk;
	unsigned long dsp_clk_rate = 0;
	unsigned long gpu_clk_rate = 0;

	if (a->value==0) {
		print_clk_info();
	} else if (a->value == 1) {
		clk = clk_get(NULL, "cclk_gpu");
		if (clk==NULL)
			return 0;
		gpu_clk_rate = clk_get_rate(clk);
		clk = clk_get(NULL, "cclk_dsp");
		if (clk==NULL)
			return 0;
		dsp_clk_rate = clk_get_rate(clk);
		return scnprintf(buf, PAGE_SIZE, "%ld\n%ld\n", gpu_clk_rate, dsp_clk_rate);
	} else {
	}

	return scnprintf(buf, PAGE_SIZE, "\n",cur_clk);
}

static ssize_t store_get_clock_rate(struct kobject *kobj,
	struct attribute *attr, const char *buf, size_t len)
{
    struct clk_attr *a = container_of(attr, struct clk_attr, attr);
    struct clk *req_clk;
    unsigned long old_rate = 0;
	char str_clkname[30];
	char *delmit = ":";
	char *first_attr="", *second_attr="";
	int first_size;
	int i = 0, temp = 0, rate = 0;

	sscanf(buf, "%29s", str_clkname);
	first_size = strlen(str_clkname)-1;

	for (i=temp;i<first_size;i++)
	{
		if (str_clkname[i] == *delmit)
		{
			str_clkname[i] = NULL;
			first_attr = &str_clkname[temp];
			temp = i+1;
		}
		if (i==first_size-1){
			second_attr = &str_clkname[temp];
		}
	}

	if (a->value==2)
	{
		while (*second_attr!=NULL&&(*second_attr>='0'&&*second_attr<='9'))
		{
			rate = (rate*10)+((int)*second_attr-(int)'0');
			second_attr++;
		}

		if (rate<7 || rate>3000)
		{
			printk("you have to enter right clock rate\n");
			return len;
		}

		req_clk = clk_get(NULL, first_attr);

		if (IS_ERR_OR_NULL(req_clk))
		{
			printk("can not find clock!\nusage -> echo clock_name:clock_rate >\
			/sys/clkmodule/set_clk\n");
			return len;
		}
		old_rate = clk_get_rate(req_clk);

		clk_set_rate(req_clk, rate*1000000);
		printk("change %s 's clock rate from %ld to %ld\n", \
		req_clk->name, old_rate, clk_get_rate(req_clk));
	}
	else if (a->value == 3)		/* dvfs item set */
	{
		u32 table_index, freq = 0, volt = 0, pll = 0;
		sscanf(buf, "%d %d %d %d", &table_index, &freq, &volt, &pll);
		mb_dvfs_item_set(table_index, freq, volt, pll);
	}
	else if (a->value == 4)
	{
		req_clk = clk_get(NULL, first_attr);

		if (IS_ERR_OR_NULL(req_clk))
		{
			printk("can not find clock!\n");
			return len;
		}

		if (0 == strcmp("on", second_attr)){
			clk_prepare_enable(req_clk);
		}
		else if (0 == strcmp("off", second_attr)){
			clk_disable_unprepare(req_clk);
		}
		else{
			printk("their is no option called %s\n", second_attr);
		}

		printk("%s is %s", req_clk->name, second_attr);
  	}
	else
	{
		unsigned long flags = 0;

		spin_lock_irqsave(&lock_test, flags);

		spin_lock_irqsave(&lock_test, flags);

		spin_unlock_irqrestore(&lock_test, flags);

		spin_unlock_irqrestore(&lock_test, flags);
	}

	return len;
}

static struct sysfs_ops clk_ops = {
    .show = show_get_clock_rate,
    .store = store_get_clock_rate,
};

static struct kobj_type clk_type = {
	.sysfs_ops = &clk_ops,
	.default_attrs = clkattr,
};

struct kobject *clk_kobj;
static int __init clk_module_init(void)
{
    int err = -1;
    clk_kobj = kzalloc(sizeof(*clk_kobj), GFP_KERNEL);
    if (clk_kobj) {
        kobject_init(clk_kobj, &clk_type);
        if (kobject_add(clk_kobj, NULL, "%s", "clk_module")) {
             err = -1;
             printk("Sysfs creation failed\n");
             kobject_put(clk_kobj);
             clk_kobj = NULL;
        }
        err = 0;
    }
    return err;
}

static void __exit clk_module_exit(void)
{
    if (clk_kobj) {
        kobject_put(clk_kobj);
        kfree(clk_kobj);
    }
}

module_init(clk_module_init);
module_exit(clk_module_exit);

