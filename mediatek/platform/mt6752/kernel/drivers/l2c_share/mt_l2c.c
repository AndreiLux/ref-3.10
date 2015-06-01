#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include "mach/mt_reg_base.h"
#include "mach/sync_write.h"
#include "asm/cacheflush.h"
#include "mt_l2c.h"

enum options{
    BORROW_FROM_MP0_256k,
    RETURN_TO_MP0_256k,
    BORROW_FROM_MP1_256k,
    RETURN_TO_MP1_256k,
    BORROW_FROM_MP0_MP1_512k,
    RETURN_TO_MP0_MP1_512k,
    BORROW_FROM_MP1_512k,
    RETURN_TO_MP1_512k,
    BORROW_NONE
};

/* config L2 to its size */
extern unsigned int get_cluster_core_count(void);
extern void __disable_cache(void);
extern void __enable_cache(void);
void inner_dcache_flush_all(void);
void inner_dcache_flush_L1(void);
void inner_dcache_flush_L2(void);
#define L2C_SIZE_CFG_OFF    8
#define L2C_SHARE_ENABLE    12

static DEFINE_SPINLOCK(cache_cfg_lock);
static DEFINE_SPINLOCK(cache_cfg1_lock);

static char *log[] = {
    "borrow from MP0 for 256k",
    "return to MP0 for 256k",
    "borrow from MP1 for 256k",
    "return to MP1 for 256k",
    "borrow from MP0 & MP1 for 512k",
    "return to MP0 & MP1 for 512k",
    "borrow from MP1 for 512k",
    "return to MP1 for 512k",
    "wrong option"
};

#define CONFIGED_256K   0x1
#define CONFIGED_512K   0x3

unsigned int mp0_cache_config;
unsigned int mp1_cache_config;

/* config L2 cache and sram to its size */
int config_L2_size(enum options option)
{
	volatile unsigned int cache_cfg0, cache_cfg1;
	int ret = 0;
    cache_cfg0 = readl(IOMEM(mp0_cache_config));
    printk("cache_cfg0 = 0x%x\n", cache_cfg0);
    cache_cfg0 &= (~0xf) << L2C_SIZE_CFG_OFF;
    cache_cfg1 = readl(IOMEM(mp1_cache_config));
    printk("cache_cfg1 = 0x%x\n", cache_cfg1);
    cache_cfg1 &= (~0xf) << L2C_SIZE_CFG_OFF;

	spin_lock(&cache_cfg_lock);
    switch(option) {
    case BORROW_FROM_MP0_256k:
		cache_cfg0 |= CONFIGED_256K << L2C_SIZE_CFG_OFF;
        printk("writing 0x%x to address 0x%x\n", cache_cfg0, mp0_cache_config);
		mt_reg_sync_writel(cache_cfg0, mp0_cache_config);
        cache_cfg0 = 0x1 << L2C_SHARE_ENABLE;       
		mt_reg_sync_writel(cache_cfg0, mp0_cache_config);
        printk("writing 0x%x to address 0x%x\n", cache_cfg0, mp0_cache_config);
        break;
    case RETURN_TO_MP0_256k:
		cache_cfg0 |= CONFIGED_512K << L2C_SIZE_CFG_OFF;
		mt_reg_sync_writel(cache_cfg0, mp0_cache_config);
        cache_cfg0 &= ~0x1 << L2C_SHARE_ENABLE;       
		mt_reg_sync_writel(cache_cfg0, mp0_cache_config);
        break;
    case BORROW_FROM_MP1_256k:
		cache_cfg1 |= CONFIGED_256K << L2C_SIZE_CFG_OFF;
		mt_reg_sync_writel(cache_cfg1, mp1_cache_config);
        cache_cfg1 |= 0x1 << L2C_SHARE_ENABLE;       
		mt_reg_sync_writel(cache_cfg1, mp1_cache_config);
        break;
    case RETURN_TO_MP1_256k:
    case RETURN_TO_MP1_512k:
		cache_cfg1 |= CONFIGED_512K << L2C_SIZE_CFG_OFF;
		mt_reg_sync_writel(cache_cfg0, mp0_cache_config);
        cache_cfg1 &= ~0x1 << L2C_SHARE_ENABLE;       
		mt_reg_sync_writel(cache_cfg0, mp0_cache_config);
        break;
    case BORROW_FROM_MP0_MP1_512k:
		cache_cfg0 |= CONFIGED_256K << L2C_SIZE_CFG_OFF;
        cache_cfg1 = cache_cfg0;
		mt_reg_sync_writel(cache_cfg0, mp0_cache_config);
		mt_reg_sync_writel(cache_cfg1, mp1_cache_config);
        cache_cfg0 |= 0x1 << L2C_SHARE_ENABLE;       
        cache_cfg1 = cache_cfg0;
		mt_reg_sync_writel(cache_cfg0, mp0_cache_config);
		mt_reg_sync_writel(cache_cfg1, mp1_cache_config);
    case RETURN_TO_MP0_MP1_512k:
		cache_cfg0 |= CONFIGED_512K << L2C_SIZE_CFG_OFF;
        cache_cfg1 = cache_cfg0;
		mt_reg_sync_writel(cache_cfg0, mp0_cache_config);
		mt_reg_sync_writel(cache_cfg1, mp1_cache_config);
        cache_cfg0 &= ~0x1 << L2C_SHARE_ENABLE;       
        cache_cfg1 = cache_cfg0;
		mt_reg_sync_writel(cache_cfg0, mp0_cache_config);
		mt_reg_sync_writel(cache_cfg1, mp1_cache_config);
        break;
    case BORROW_FROM_MP1_512k:
		cache_cfg1 |= CONFIGED_512K << L2C_SIZE_CFG_OFF;
		mt_reg_sync_writel(cache_cfg1, mp1_cache_config);
        cache_cfg1 |= 0x1 << L2C_SHARE_ENABLE;       
		mt_reg_sync_writel(cache_cfg1, mp1_cache_config);
        break;
    default:
        ret = -1;
        break;
    }
	spin_unlock(&cache_cfg_lock);
	return ret;
}

int is_l2c_configable(enum options option)
{
	volatile unsigned int cache_cfg0, cache_cfg1;
	int result = 0;
    cache_cfg0 = readl(IOMEM(mp0_cache_config));
    cache_cfg0 = cache_cfg0 >> L2C_SIZE_CFG_OFF;
    cache_cfg0 = cache_cfg0 & 0xf;
    cache_cfg1 = readl(IOMEM(mp1_cache_config));
    cache_cfg1 = cache_cfg1 >> L2C_SIZE_CFG_OFF;
    cache_cfg1 = cache_cfg1 & 0xf;

    switch(option) {
    case BORROW_FROM_MP0_256k:
        if(cache_cfg0 == CONFIGED_256K)
            result = -1;
        break;
    case RETURN_TO_MP0_256k:
        if(cache_cfg0 == CONFIGED_512K)
            result = -1;
        break;
    case BORROW_FROM_MP1_256k:
    case BORROW_FROM_MP1_512k:
        if(cache_cfg1 == CONFIGED_256K)
            result = -1;
        break;
    case RETURN_TO_MP1_256k:
        if(cache_cfg1 == CONFIGED_512K)
            result = -1;
        break;
    case BORROW_FROM_MP0_MP1_512k:
        if(cache_cfg0 == CONFIGED_256K || cache_cfg1 == CONFIGED_256K)
            result = -1;
        break;
    case RETURN_TO_MP0_MP1_512k:
        if(cache_cfg0 == CONFIGED_512K || cache_cfg1 == CONFIGED_512K)
            result = -1;
        break;
    default:
        result = -1;
        break;
    }

	return result;
}

atomic_t L1_flush_done = ATOMIC_INIT(0);
extern int __is_dcache_enable(void);
extern int cr_alignment;
extern void v7_disable_clean_invalidate_dcache(int, int);
#define L1_AND_L2   0
#define L1_ONLY     1
#define L2_ONLY     2

#define ENABLE_CACHE    0
#define DISABLE_CACHE   1

void atomic_flush(void)
{
    inner_dcache_flush_L1();

    //update cr_alignment for other kernel function usage 
    cr_alignment = cr_alignment & ~(0x4); //C1_CBIT
    printk("cpu %d is done\n", smp_processor_id());
}

//int config_L2(int size)
int config_L2(enum options option)
{
    int i, cpu;
    struct cpumask mask0, mask1;
    cpumask_clear(&mask0);
    cpumask_clear(&mask1);

	if (in_interrupt()) {
		printk(KERN_ERR "Cannot use %s in interrupt/softirq context\n",
		       __func__);
		return -1;
	}

	if (is_l2c_configable(option) == -1) {
		printk("config L2 size by action %s is not feasible\n",
		       log[option]);
		return 0;
	}

    //atomic_set(&L1_flush_done, 0);
	get_online_cpus();
    printk("%s: 1\n", __func__);

    // set for MP0
    for(i = 0; i < get_cluster_core_count(); i++)
        cpumask_set_cpu(i, &mask0);
    // set for MP1
    i = get_cluster_core_count();
    for(;i < get_cluster_core_count() * 2; i++) // MP1
        cpumask_set_cpu(i, &mask1);

    //printk("mask0 = %d, mask1 = %d\n", mask0.bits[0], mask1.bits[0]);

    printk("%s: 2\n", __func__);
    switch(option) {
    case BORROW_FROM_MP0_256k:
    case RETURN_TO_MP0_256k:
        /* disable cache and flush L1, then L2 */
        //on_each_cpu_mask(&mask0, (smp_call_func_t)atomic_flush, NULL, true);
        for_each_cpu(cpu, &mask0) {
            mtk_smp_call_function_single(cpu, (smp_call_func_t)atomic_flush, NULL, true); 
        }
        printk("%s: 3\n", __func__);
        //smp_call_function_any(&mask0, (smp_call_func_t)inner_dcache_flush_L2, NULL, true);
        printk("%s: 4\n", __func__);
        break;
    case BORROW_FROM_MP1_256k:
    case BORROW_FROM_MP1_512k:
    case RETURN_TO_MP1_256k:
    case RETURN_TO_MP1_512k:
        /* disable cache and flush L1, then L2 */
        on_each_cpu_mask(&mask1, (smp_call_func_t)atomic_flush, NULL, true);
        smp_call_function_any(&mask1, (smp_call_func_t)inner_dcache_flush_L2, NULL, true);
        break;
    case BORROW_FROM_MP0_MP1_512k:
    case RETURN_TO_MP0_MP1_512k:
        on_each_cpu_mask(&mask0, (smp_call_func_t)atomic_flush, NULL, true);
        smp_call_function_any(&mask0, (smp_call_func_t)inner_dcache_flush_L2, NULL, true);
        on_each_cpu_mask(&mask1, (smp_call_func_t)atomic_flush, NULL, true);
        smp_call_function_any(&mask1, (smp_call_func_t)inner_dcache_flush_L2, NULL, true);
        break;
    default:
        printk("bad option for %s: %d\n", __func__, option);
        return -1;
    }
	//printk("[Config L2] Config L2 start, on line cpu = %d\n",num_online_cpus());	
	
	//while(atomic_read(&L1_flush_done) != num_online_cpus());	
	
	/* change L2 size */	
    printk("%s: 5\n", __func__);
	//config_L2_size(option);
    printk("%s: 6\n", __func__);
		
	/* enable cache */
    switch(option) {
    case BORROW_FROM_MP0_256k:
    case RETURN_TO_MP0_256k:
        on_each_cpu_mask(&mask0, (smp_call_func_t)__enable_cache, NULL, true);
        printk("%s: 7\n", __func__);
        break;
    case BORROW_FROM_MP1_256k:
    case BORROW_FROM_MP1_512k:
    case RETURN_TO_MP1_256k:
    case RETURN_TO_MP1_512k:
        on_each_cpu_mask(&mask1, (smp_call_func_t)__enable_cache, NULL, true);
        printk("%s: 7\n", __func__);
        break;
    case BORROW_FROM_MP0_MP1_512k:
        on_each_cpu((smp_call_func_t)__enable_cache, NULL, true);
        printk("%s: 7\n", __func__);
        break;
    default:
        printk("bad option for %s: %d\n", __func__, option);
        return -1;
    }
	
	//update cr_alignment for other kernel function usage 
	cr_alignment = cr_alignment | (0x4); //C1_CBIT
    printk("%s: 8\n", __func__);
	put_online_cpus();
	printk("config L2 size: %s done\n", log[option]);
	return 0;
}

#include <linux/device.h>
#include <linux/platform_device.h>
static struct device_driver mt_l2c_drv = {
	.name = "l2c",
	.bus = &platform_bus_type,
	.owner = THIS_MODULE,
};

int mt_l2c_get_status(char *buf)
{
	unsigned int size = 0, cache0_cfg, cache1_cfg;
	cache0_cfg = readl(IOMEM(mp0_cache_config));
	cache0_cfg = cache0_cfg >> L2C_SIZE_CFG_OFF;
	cache1_cfg = readl(IOMEM(mp1_cache_config));
	cache1_cfg = cache1_cfg >> L2C_SIZE_CFG_OFF;
    cache0_cfg &= 0xf;
	cache1_cfg &= 0xf;
    switch(cache0_cfg) {
    case CONFIGED_256K:
        size += snprintf(buf, PAGE_SIZE, "MP0 cache = 256K ");
        buf += size;
        break;
    case CONFIGED_512K:
        size += snprintf(buf, PAGE_SIZE, "MP0 cache = 512K ");
        buf += size;
        break;
    default:
        size += snprintf(buf, PAGE_SIZE, "Weird cache config for MP0: 0x%x ", cache0_cfg);
        buf += size;
        break;
    }

    switch(cache1_cfg) {
    case CONFIGED_256K:
        size += snprintf(buf, PAGE_SIZE, "MP1 cache = 256K\n");
        break;
    case CONFIGED_512K:
        size += snprintf(buf, PAGE_SIZE, "MP1 cache = 512K\n");
        break;
    default:
        size += snprintf(buf, PAGE_SIZE, "Weird cache config for MP1: 0x%x\n", cache0_cfg);
        break;
    }

	return size;
}

/*
 * cur_l2c_show: To show cur_l2c size.
 */
static ssize_t cur_l2c_show(struct device_driver *driver, char *buf)
{
	return mt_l2c_get_status(buf);
	//return snprintf(buf, PAGE_SIZE, "%d\n", size);
}

/*
 * cur_l2c_store: To set cur_l2c size.
 */
typedef int (*FUNC)(void *);
static ssize_t cur_l2c_store(struct device_driver *driver, const char *buf,
			     size_t count)
{
	char *p = (char *)buf;
	int option, ret;

	option = simple_strtoul(p, &p, 10);

    if(option >= BORROW_NONE) {
        printk("wrong option %d\n", option);
        return count;
    }

    printk("config L2 option: %s\n", log[option]);

    ret = config_L2(option);

	if (ret < 0)
		printk("Config L2 error ret:%d by %s\n", ret, log[option]);
	return count;
}

DRIVER_ATTR(current_l2c, 0644, cur_l2c_show, cur_l2c_store);
/*
 * mt_l2c_init: initialize l2c driver.
 * Always return 0.
 */
int mt_l2c_init(void)
{
	int ret;

	ret = driver_register(&mt_l2c_drv);
	if (ret) {
		printk("fail to register mt_l2c_drv\n");
	}

    mp0_cache_config = (unsigned int)ioremap(MP0_CACHE_CONFIG, 0x4); 
    mp1_cache_config = (unsigned int)ioremap(MP1_CACHE_CONFIG, 0x4);

	ret = driver_create_file(&mt_l2c_drv, &driver_attr_current_l2c);
	if (ret) {
		printk("fail to create mt_l2c sysfs files\n");
	}

	return 0;
}

arch_initcall(mt_l2c_init);

/* To disable/enable auto invalidate cache API 
    @ disable == 1 to disable auto invalidate cache
    @ disable == 0 to enable auto invalidate cache 
   return 0 -> success, -1 -> fail
*/
int auto_inv_cache(unsigned int disable)
{
	volatile unsigned int cache_cfg, cache_cfg_new;
	spin_lock(&cache_cfg1_lock);
	if(disable) {
		/* set cache auto disable */
		cache_cfg = readl(IOMEM(MP0_CACHE_CONFIG));
		cache_cfg |= 0x1F;
		writel(cache_cfg, IOMEM(MP0_CACHE_CONFIG));
	} else if (disable == 0){
		/* set cache auto enable */
		cache_cfg = readl(IOMEM(MP0_CACHE_CONFIG));
		cache_cfg &= ~0x1F;
		writel(cache_cfg, IOMEM(MP0_CACHE_CONFIG));
	} else {
		printk("Caller give a wrong arg:%d\n", disable);
		spin_unlock(&cache_cfg1_lock);
		return -1;
	}
	cache_cfg_new = readl(IOMEM(MP0_CACHE_CONFIG));
	spin_unlock(&cache_cfg1_lock);
	if((cache_cfg_new & 0x1F) != (cache_cfg & 0x1F))
		return 0;
	else
		return -1;
}

EXPORT_SYMBOL(config_L2);
