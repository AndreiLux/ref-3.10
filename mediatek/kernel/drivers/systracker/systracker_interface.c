#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/interrupt.h>
#include <mach/systracker.h>
#include <mach/sync_write.h>
#include <asm/signal.h>
#include <linux/mtk_ram_console.h>
#include <linux/sched.h>
#include <asm/system_misc.h>
#ifdef CONFIG_OF
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#include <mach/systracker_drv.h>
#include <asm/memory.h>
#define TRACKER_DEBUG 1
#include <linux/cpu.h>
int systracker_probe(struct platform_device *pdev);
int systracker_remove(struct platform_device *pdev);
int systracker_suspend(struct platform_device *pdev, pm_message_t state);
int systracker_resume(struct platform_device *pdev);
void __iomem *sleep;
void __iomem *idle;
#ifdef CONFIG_OF
extern void __iomem *BUS_DBG_BASE;
static const struct of_device_id systracker_of_ids[] = {
    {   .compatible = "mediatek,BUS_DBG", },
    {}
};
#endif

static struct mt_systracker_driver mt_systracker_drv =
{
    .driver = {
        .driver = {
            .name = "systracker",
            .bus = &platform_bus_type,
            .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = systracker_of_ids,
#endif
        },
        .probe=systracker_probe,
        .remove =systracker_remove,
        .suspend =systracker_suspend,
        .resume =systracker_resume,
    },
    .device = {
       .name = "systracker",
       .id = 0,
       .dev =
        {
        },
    },
    .reset_systracker		= NULL,
    .enable_watch_point 	= NULL,
    .disable_watch_point 	= NULL,
    .set_watch_point_address	= NULL,
    .enable_systracker         	= NULL,
    .disable_systracker         = NULL,
    .test_systracker 		= NULL,
    .systracker_probe		= NULL,
    .systracker_remove          = NULL,
    .systracker_suspend         = NULL,
    .systracker_resume          = NULL,

};



extern struct systracker_config_t track_config;
extern struct systracker_entry_t track_entry;


int systracker_probe(struct platform_device *pdev)
{
    if (mt_systracker_drv.systracker_probe)
    {
        mt_systracker_drv.systracker_probe(pdev);
    	return 1;
    }
    else
    {
    	printk("mt_systracker_drv.systracker_probe is NULL");
    	return 0;
    }
}

int systracker_remove(struct platform_device *pdev)
{
    if (mt_systracker_drv.systracker_remove)
    {
        mt_systracker_drv.systracker_remove(pdev);
        return 1;
    }
    else
    {
        printk("mt_systracker_drv.systracker_remove is NULL");
        return 0;
    }
}
int systracker_suspend(struct platform_device *pdev, pm_message_t state)
{

    if(mt_systracker_drv.systracker_suspend)
    {
    	mt_systracker_drv.systracker_suspend(pdev,state);
        return 1;
    }
    else
    {
    	printk("mt_systracker_drv.systracker_suspend is NULL");
        return 0;
    }
}

int systracker_resume(struct platform_device *pdev)
{
    if(mt_systracker_drv.systracker_resume)
    {
    	mt_systracker_drv.systracker_resume(pdev);
        return 1;
    }
    else
    {
        printk("mt_systracker_drv.systracker_resume is NULL");
        return 0;
    }
}

struct mt_systracker_driver *get_mt_systracker_drv(void)
{
    return &mt_systracker_drv;
}


int tracker_dump(char *buf)
{

    char *ptr = buf;
    unsigned int reg_value;
    int i;
    unsigned int entry_valid;
    unsigned int entry_tid;
    unsigned int entry_id;
    unsigned int entry_address;
    unsigned int entry_data_size;
    unsigned int entry_burst_length;


    //if(is_systracker_device_registered)
    {
        /* Get tracker info and save to buf */

        /* BUS_DBG_AR_TRACK_L(__n)
         * [31:0] ARADDR: DBG read tracker entry read address
         */

        /* BUS_DBG_AR_TRACK_H(__n)
         * [14] Valid:DBG read tracker entry valid
         * [13:7] ARID:DBG read tracker entry read ID
         * [6:4] ARSIZE:DBG read tracker entry read data size
         * [3:0] ARLEN: DBG read tracker entry read burst length
         */

        /* BUS_DBG_AR_TRACK_TID(__n)
         * [2:0] BUS_DBG_AR_TRANS0_ENTRY_ID: DBG read tracker entry ID of 1st transaction
         */

#ifdef TRACKER_DEBUG
        printk("Sys Tracker Dump\n");
#endif

        for (i = 0; i < BUS_DBG_NUM_TRACKER; i++) {
            entry_address       = track_entry.ar_track_l[i];
            reg_value           = track_entry.ar_track_h[i];
            entry_valid         = extract_n2mbits(reg_value,19,19);
            entry_id            = extract_n2mbits(reg_value,7,18);
            entry_data_size     = extract_n2mbits(reg_value,4,6);
            entry_burst_length  = extract_n2mbits(reg_value,0,3);
            entry_tid           = track_entry.ar_trans_tid[i];

            ptr += sprintf(ptr, " \
read entry = %d, \
valid = 0x%x, \
tid = 0x%x, \
read id = 0x%x, \
address = 0x%x, \
data_size = 0x%x, \
burst_length = 0x%x\n",
                        i,
                        entry_valid,
                        entry_tid,
                        entry_id,
                        entry_address,
                        entry_data_size,
                        entry_burst_length);

#ifdef TRACKER_DEBUG
            printk("\
read entry = %d, \
valid = 0x%x, \
tid = 0x%x, \
read id = 0x%x, \
address = 0x%x, \
data_size = 0x%x, \
burst_length = 0x%x\n",
                        i,
                        entry_valid,
                        entry_tid,
                        entry_id,
                        entry_address,
                        entry_data_size,
                        entry_burst_length);
#endif
        }

        /* BUS_DBG_AW_TRACK_L(__n)
         * [31:0] AWADDR: DBG write tracker entry write address
         */

        /* BUS_DBG_AW_TRACK_H(__n)
         * [14] Valid:DBG   write tracker entry valid
         * [13:7] ARID:DBG  write tracker entry write ID
         * [6:4] ARSIZE:DBG write tracker entry write data size
         * [3:0] ARLEN: DBG write tracker entry write burst length
         */

        /* BUS_DBG_AW_TRACK_TID(__n)
         * [2:0] BUS_DBG_AW_TRANS0_ENTRY_ID: DBG write tracker entry ID of 1st transaction
         */
        
      for (i = 0; i < BUS_DBG_NUM_TRACKER; i++) {
            entry_address       = track_entry.aw_track_l[i];
            reg_value           = track_entry.aw_track_h[i];
            entry_valid         = extract_n2mbits(reg_value,19,19);
            entry_id            = extract_n2mbits(reg_value,7,18);
            entry_data_size     = extract_n2mbits(reg_value,4,6);
            entry_burst_length  = extract_n2mbits(reg_value,0,3);
            entry_tid           = track_entry.aw_trans_tid[i];

            ptr += sprintf(ptr, " \
write entry = %d, \
valid = 0x%x, \
tid = 0x%x, \
write id = 0x%x, \
address = 0x%x, \
data_size = 0x%x, \
burst_length = 0x%x\n",
                        i,
                        entry_valid,
                        entry_tid,
                        entry_id,
                        entry_address,
                        entry_data_size,
                        entry_burst_length);

#ifdef TRACKER_DEBUG
            printk("\
write entry = %d, \
valid = 0x%x, \
tid = 0x%x, \
write id = 0x%x, \
address = 0x%x, \
data_size = 0x%x, \
burst_length = 0x%x\n",
                        i,
                        entry_valid,
                        entry_tid,
                        entry_id,
                        entry_address,
                        entry_data_size,
                        entry_burst_length);
#endif
      }
      
      return strlen(buf);
    }

    return -1;
}

static ssize_t tracker_run_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%x\n", readl(IOMEM(BUS_DBG_CON)));
}

static ssize_t tracker_run_store(struct device_driver * driver, const char *buf, size_t count)
{
    unsigned int value;

    if (unlikely(sscanf(buf, "%u", &value) != 1))
        return -EINVAL;

    if (value == 1) {
        mt_systracker_drv.enable_systracker();
    } else if(value == 0) {
        mt_systracker_drv.disable_systracker();
    } else {
        return -EINVAL;
    }

    return count;
}

DRIVER_ATTR(tracker_run, 0644, tracker_run_show, tracker_run_store);

static ssize_t enable_wp_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%x\n", track_config.enable_wp);
}

static ssize_t enable_wp_store(struct device_driver * driver, const char *buf, size_t count)
{
    unsigned int value;

    if (unlikely(sscanf(buf, "%u", &value) != 1))
        return -EINVAL;

    if (value == 1) {
        mt_systracker_drv.enable_watch_point();
    } else if(value == 0) {
        mt_systracker_drv.disable_watch_point();
    } else {
        return -EINVAL;
    }

    return count;
}

DRIVER_ATTR(enable_wp, 0644, enable_wp_show, enable_wp_store);

static ssize_t set_wp_address_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%x\n", track_config.wp_phy_address);
}

static ssize_t set_wp_address_store(struct device_driver * driver, const char *buf, size_t count)
{
    unsigned int value;

    sscanf(buf, "0x%x", &value);
    printk("watch address:0x%x\n",value);
    mt_systracker_drv.set_watch_point_address(value);

    return count;
}

DRIVER_ATTR(set_wp_address, 0644, set_wp_address_show, set_wp_address_store);

static ssize_t tracker_entry_dump_show(struct device_driver *driver, char *buf)
{
    int ret = tracker_dump(buf);
    if (ret == -1)
        printk(KERN_CRIT "Dump error in %s, %d\n", __func__, __LINE__);

    ///*FOR test*/
    //test_systracker();

    return strlen(buf);;
}


extern void wdt_arch_reset(char mode);
static ssize_t tracker_swtrst_show(struct device_driver *driver, char *buf)
{
    return 0;
}

static ssize_t tracker_swtrst_store(struct device_driver * driver, const char *buf, size_t count)
{
    writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_SW_RST, IOMEM(BUS_DBG_CON));
    return count;
}

DRIVER_ATTR(tracker_swtrst, 0664, tracker_swtrst_show, tracker_swtrst_store);
#ifdef SYSTRACKER_TEST_SUIT
void systracker_wp_test(void)
{
    void __iomem *p;
    //use watch dog reg base as our watchpoint
    p = ioremap(0x10007000, 0x4); 
    printk("in wp_test, we got p = 0x%p\n", p);
    mt_systracker_drv.set_watch_point_address(0x10007000);
    mt_systracker_drv.enable_watch_point();
    //touch it
    writel(0, p);
    printk("after we touched watchpoint\n");
    iounmap(p);
    return;
}


void __iomem *p;
void __iomem *mm_area;
void systracker_timeout_test(void)
{
    printk("systracker_timeout_test v1\n");
    //skip slverr
    track_config.enable_slave_err = 0;
    //enable systracker
    mt_systracker_drv.enable_systracker();
    printk("in wp_test, we got mm_area = 0x%p, mm_protect=0x%p, pa = 0x%llx\n", 
		mm_area,p, (unsigned long long)(vmalloc_to_pfn(mm_area) << 12));
    
    //read it, should be deadbeef
      writel(readl(p) | (0x1 << 6), p);
     // readl(mm_area);
      writel(0x0,mm_area);
      writel(readl(p) & ~(0x1 << 6), p);
    return;
}

void systracker_timeout_withrecord_test(void)
{
    //enable timeout halt 
    printk("in wp_test, we got p = 0x%p\n", p);
    writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_HALT_ON_EN, IOMEM(BUS_DBG_CON));
    writel(readl(p) | (0x1 << 6), p);
    //use mmsys reg base for our test 
    //read it, should be deadbeef
 //   readl(mm_area);
    writel(0x0,mm_area);
    wdt_arch_reset(1);
    return;
}

void systracker_notimeout_test(void)
{

    writel(readl(p) | (0x1 << 6), p);

    printk("in wp_test, we got p = 0x%p\n", p);
    
    //disable timeout
    writel(readl(IOMEM(BUS_DBG_CON)) & ~ (BUS_DBG_CON_TIMEOUT_EN), IOMEM(BUS_DBG_CON));
    //read it, should cause bus hang 
    readl(mm_area);
    //never come back
    printk("failed??\n");
    return;
}

void systracker_slverr_test(void)
{

    writel(readl(p) | (0x1 << 6), p);

    printk("in wp_test, we got p = 0x%p\n", p);

    //enable slverr
    writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_SLV_ERR_EN, IOMEM(BUS_DBG_CON));
    
    //read it, should cause bus hang 
    readl(mm_area);
    //writel(0x0,mm_area);
    //never come back
    printk("failed??\n");
  //  iounmap(p);
    return;
}

static ssize_t test_suit_show(struct device_driver *driver, char *buf)
{
   return snprintf(buf, PAGE_SIZE, "==Systracker test==\n"
                                   "1.Systracker show dump test\n"
                                   "2.Systracker watchpoint test\n"
                                   "3.Systracker timeout test\n"
                                   "4.Systracker timeout with record test\n"
                                   "5.Systracker no timeout test\n"
                                   "6.Systracker SLVERR test\n"
   );
}

static ssize_t test_suit_store(struct device_driver *driver, const char *buf, size_t count)
{
    char *p = (char *)buf;
    unsigned int num;

    num = simple_strtoul(p, &p, 10);
        switch(num){
            /* Test Systracker Function */
            case 1:
                return tracker_entry_dump_show(driver, p);
            case 2:
                systracker_wp_test();
                break;
            case 3:
                systracker_timeout_test();
                break;
            case 4:
                systracker_timeout_withrecord_test();
                break;
            case 5:
                systracker_notimeout_test();
                break;
            case 6:
                systracker_slverr_test();
                break;
            default:
                break;
        }

    return count;
}

DRIVER_ATTR(test_suit, 0664, test_suit_show, test_suit_store);
#endif

static ssize_t tracker_entry_dump_store(struct device_driver * driver, const char *buf, size_t count)
{
#ifdef TRACKER_DEBUG
    mt_systracker_drv.test_systracker();
#endif
    return count;
}

DRIVER_ATTR(tracker_entry_dump, 0664, tracker_entry_dump_show, tracker_entry_dump_store);

static ssize_t tracker_last_status_show(struct device_driver *driver, char *buf)
{
    
    if(track_entry.dbg_con & (BUS_DBG_CON_IRQ_AR_STA | BUS_DBG_CON_IRQ_AW_STA)){
        return snprintf(buf, PAGE_SIZE, "1\n");
    } else {
        return snprintf(buf, PAGE_SIZE, "0\n");
    }
}

static ssize_t tracker_last_status_store(struct device_driver * driver, const char *buf, size_t count)
{
    return count;
}

DRIVER_ATTR(tracker_last_status, 0664, tracker_last_status_show, tracker_last_status_store);


/*
 * driver initialization entry point
 */
static int __init systracker_init(void)
{
    int err;
    int ret=0; 
    sleep = ioremap(0x10001220, 0xc);
    idle = ioremap(0x10201180, 0x14);

#ifdef SYSTRACKER_TEST_SUIT
    p = ioremap(0x10001220, 0x4);
    //use mmsys reg base for our test
    mm_area = ioremap(0x14000000, 0x4);
#endif


    err = platform_driver_register(&mt_systracker_drv.driver);
    if (err) {
        return err;
    }

    /* Create sysfs entry */
    ret  = driver_create_file(&mt_systracker_drv.driver.driver , &driver_attr_tracker_entry_dump);
    ret |= driver_create_file(&mt_systracker_drv.driver.driver , &driver_attr_tracker_run);
    ret |= driver_create_file(&mt_systracker_drv.driver.driver , &driver_attr_enable_wp);
    ret |= driver_create_file(&mt_systracker_drv.driver.driver , &driver_attr_set_wp_address);
    ret |= driver_create_file(&mt_systracker_drv.driver.driver , &driver_attr_tracker_swtrst);
    ret |= driver_create_file(&mt_systracker_drv.driver.driver , &driver_attr_tracker_last_status);
#ifdef SYSTRACKER_TEST_SUIT
    ret |= driver_create_file(&mt_systracker_drv.driver.driver , &driver_attr_test_suit);
#endif
    if (ret) {
        pr_err("Fail to create systracker_drv sysfs files");
    }
    printk(KERN_ALERT "systracker init done\n");
    return 0;
}

/*
 * driver exit point
 */
static void __exit systracker_exit(void)
{
}

module_init(systracker_init);
module_exit(systracker_exit);
