#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <mach/sync_write.h>
#include <mach/dbg_dump.h>
#include <linux/kallsyms.h>
int lastpc_probe(struct platform_device *pdev);
int lastpc_remove(struct platform_device *pdev);
int lastpc_suspend(struct platform_device *pdev, pm_message_t state);
int lastpc_resume(struct platform_device *pdev);

/* Some chip do not have reg dump, define a weak to avoid build error */
int __weak reg_dump_platform(char *buf) { return 1; }


int dbg_reg_dump_probe(struct platform_device *pdev);


#ifdef CONFIG_OF
extern void __iomem *MCUSYS_CFGREG_BASE;
static const struct of_device_id mcusys_of_ids[] = {
    {   .compatible = "mediatek,MCUCFG", },
    {}
};
#endif

static struct mt_lastpc_driver mt_lastpc_drv =
{
    .driver = {
        .driver = {
            .name = "dbg_reg_dump",
            .bus = &platform_bus_type,
            .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = mcusys_of_ids,
#endif
        },
        .probe=lastpc_probe,
        .remove =lastpc_remove,
        .suspend =lastpc_suspend,
        .resume =lastpc_resume,
    },
#ifndef CONFIG_OF
    .device = {
       .name = "dbg_reg_dump",
       .id = 0,
       .dev =
        {
            .platform_data = &(reg_dump_driver_data),
        },
    },
#endif
    .lastpc_probe           = NULL,
    .lastpc_remove          = NULL,
    .lastpc_suspend         = NULL,
    .lastpc_resume          = NULL,
    .lastpc_reg_dump          = NULL,
};


int lastpc_probe(struct platform_device *pdev)
{
    if (mt_lastpc_drv.lastpc_probe)
    {
        mt_lastpc_drv.lastpc_probe(pdev);
        return 1;
    }
    else
    {
        printk("mt_lastpc_drv.lastpc_probe is NULL");
        return 0;
    }
}

int lastpc_remove(struct platform_device *pdev)
{
    if (mt_lastpc_drv.lastpc_remove)
    {
        mt_lastpc_drv.lastpc_remove(pdev);
        return 1;
    }
    else
    {
        printk("mt_lastpc_drv.lastpc_remove is NULL");
        return 0;
    }
}
int lastpc_suspend(struct platform_device *pdev, pm_message_t state)
{

    if(mt_lastpc_drv.lastpc_suspend)
    {
        mt_lastpc_drv.lastpc_suspend(pdev,state);
        return 1;
    }
    else
    {
        printk("mt_lastpc_drv.lastpc_suspend is NULL");
        return 0;
    }
}

int lastpc_resume(struct platform_device *pdev)
{
    if(mt_lastpc_drv.lastpc_resume)
    {
        mt_lastpc_drv.lastpc_resume(pdev);
        return 1;
    }
    else
    {
        printk("mt_lastpc_drv.lastpc_resume is NULL");
        return 0;
    }
}

struct mt_lastpc_driver *get_mt_lastpc_drv(void)
{
    return &mt_lastpc_drv;
}


#if 0
static struct platform_driver dbg_reg_dump_driver =
{
	.probe = dbg_reg_dump_probe,
	.driver = {
		.name = "dbg_reg_dump",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
	},
};
#endif
static ssize_t last_pc_dump_show(struct device_driver *driver, char *buf)
{
    int ret;
    if(mt_lastpc_drv.lastpc_reg_dump)
    {
        ret=mt_lastpc_drv.lastpc_reg_dump(buf);
        if(ret == -1)
        {
            printk(KERN_CRIT "Dump error in %s, %d\n", __func__, __LINE__);
        }            
    }
    else
    {
        printk("mt_lastpc_drv.last_pc_dump_show is NULL"); 
    }	
    return strlen(buf);;
}

static ssize_t last_pc_dump_store(struct device_driver * driver, const char *buf,
			   size_t count)
{
	return count;
}

DRIVER_ATTR(last_pc_dump, 0664, last_pc_dump_show, last_pc_dump_store);


#ifdef LASTPC_TEST_SUIT

extern void mt_last_pc_reboot_test_store(void);
static ssize_t last_pc_reboot_test_show(struct device_driver *driver, char *buf)
{

   return snprintf(buf, PAGE_SIZE, "==LAST PC test==\n"
                                   "1.LAST PC Reboot test\n"
   );

}

static ssize_t last_pc_reboot_test_store(struct device_driver * driver, const char *buf,
                           size_t count)
{
    char *p = (char *)buf;
    unsigned int num;

    num = simple_strtoul(p, &p, 10);
    switch(num){
    /* Test Systracker Function */
        case 1:
             mt_last_pc_reboot_test_store();
             break;
        default:
             break;

    }
        return count;

}

DRIVER_ATTR(last_pc_reboot_test, 0664, last_pc_reboot_test_show, last_pc_reboot_test_store);
#endif



int mt_reg_dump(char *buf)
{
    if(mt_lastpc_drv.lastpc_reg_dump(buf) == 0)
       return 0;
    else
        return -1;
}



/**
 * driver initialization entry point
 */
static int __init dbg_reg_dump_init(void)
{
    int err;
    int ret;
    err = platform_driver_register(&mt_lastpc_drv.driver);
    if (err) {
    return err;
    }
    ret = driver_create_file(&mt_lastpc_drv.driver.driver,&driver_attr_last_pc_dump);
#ifdef LASTPC_TEST_SUIT
    ret = driver_create_file(&mt_lastpc_drv.driver.driver,&driver_attr_last_pc_reboot_test);
#endif 
    if (ret) {
      pr_err("Fail to create mt_reg_dump_drv sysfs files");
    }
    return 0;
}

/**
 * driver exit point
 */
static void __exit dbg_reg_dump_exit(void)
{	
}

module_init(dbg_reg_dump_init);
module_exit(dbg_reg_dump_exit);
