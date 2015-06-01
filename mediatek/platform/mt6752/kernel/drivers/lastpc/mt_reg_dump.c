#include "mach/mt_reg_base.h"
#include "mach/mt_reg_dump.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <mach/sync_write.h>
#include <mach/dbg_dump.h>
#include <linux/kallsyms.h>
#include <linux/init.h>
#ifdef CONFIG_OF
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

int lastpc_platform_probe(struct platform_device *pdev);
int lastpc_platform_reg_dump(char *buf);

#ifndef CONFIG_OF
#define MCUSYS_CFGREG_BASE 0x10200000
struct reg_dump_driver_data reg_dump_driver_data =
{
    .mcu_regs = (MCUSYS_CFGREG_BASE + 0x410),
};
#else
void __iomem *MCUSYS_CFGREG_BASE;

#ifdef LASTPC_TEST_SUIT
void __iomem *RGU_BASE;
#endif
#endif

#if 0
static struct platform_device reg_dump_device = 
{    
    .name = "dbg_reg_dump",
    .dev = 
    {
        .platform_data = &(reg_dump_driver_data),
    },
};
#endif

#ifdef LASTPC_TEST_SUIT

void mt_last_pc_reboot_test_store(void)
{



    int i;

    for (i = 0x410; i <= 0x48C; i +=4)
    {
        printk("0x%x:0x%x\n",(MCUSYS_CFGREG_BASE+i),*(volatile unsigned int *)(MCUSYS_CFGREG_BASE+i));
    }
    //ddr_preserve_mode
    *(volatile unsigned int *)(RGU_BASE+0x40) |= 0x59000002; //bit1: rg_mcu_lath_en

#define MTK_WDT_MODE_DDR_RESERVE  (0x0080)
    /* enable WDT */
    *(volatile unsigned int *)(RGU_BASE+0x4) = 0x2008;//2 second
    *(volatile unsigned int *)(RGU_BASE+0x0) = (0x22000067 | MTK_WDT_MODE_DDR_RESERVE); //To enable MCU-latch, we need to enable DDR-reseve
    *(volatile unsigned int *)(RGU_BASE+0x8) = 0x1971;
    printk("0x10007040:0x%x\n",*(volatile unsigned int *)(RGU_BASE+0x40));
    printk("0x10007004:0x%x\n",*(volatile unsigned int *)(RGU_BASE+0x4));
    printk("0x10007000:0x%x\n",*(volatile unsigned int *)(RGU_BASE+0x0));
    printk("0x10007008:0x%x\n",*(volatile unsigned int *)(RGU_BASE+0x8));

    printk("wait 2 seconds, then the last PC will be dumped in the preloader\n");

    return;
}

#endif


int lastpc_platform_probe(struct platform_device *pdev)
{
  printk("lastpc probe\n");
  MCUSYS_CFGREG_BASE = of_iomap(pdev->dev.of_node, 0);
  if(!MCUSYS_CFGREG_BASE) {
      printk("can't of_iomap for lastpc!!\n");
      return -ENOMEM;
  }
  else {
      printk("of_iomap for lastpc @ 0x%p\n", MCUSYS_CFGREG_BASE);
  }
    
  return 0;
}


int lastpc_platform_reg_dump(char *buf)
{
  /* Get core numbers */
       
        int cnt = num_possible_cpus();
        char *ptr = buf;
        unsigned long pc_value;
        unsigned long fp_value;
        unsigned long sp_value;
        unsigned long size = 0;
        unsigned long offset = 0;
        char str[KSYM_SYMBOL_LEN];
        int i;
        int cluster;
        int cpu_in_cluster;
        int mcu_base;

#ifdef CONFIG_OF
        mcu_base=(unsigned int )MCUSYS_CFGREG_BASE;
        mcu_base = mcu_base + 0x410;
#else
        mcu_base=reg_dump_driver_data.mcu_regs;
#endif        
#if 0   
        if(cnt < 0)
            return ret;
#endif
#ifdef CONFIG_64BIT

        /* Get PC, FP, SP and save to buf */
        for (i = 0; i < cnt; i++) {
            cluster = i / 4;
            cpu_in_cluster = i % 4;
            pc_value = readl(IOMEM((mcu_base+0x0) + (cpu_in_cluster << 5) + (0x100 * cluster)));
            fp_value = readl(IOMEM((mcu_base+0x10) + (cpu_in_cluster << 5) + (0x100 * cluster)));
            sp_value = readl(IOMEM((mcu_base+0x18) + (cpu_in_cluster << 5) + (0x100 * cluster)));
            kallsyms_lookup((unsigned long)pc_value, &size, &offset, NULL, str);
            ptr += sprintf(ptr, "[LAST PC] CORE_%d PC = 0x%lx(%s + 0x%lx), FP = 0x%lx, SP = 0x%lx\n", i, pc_value, str, offset, fp_value, sp_value);
            printk("[LAST PC] CORE_%d PC = 0x%lx(%s), FP = 0x%lx, SP = 0x%lx\n", i, pc_value, str, fp_value, sp_value);
        }


#else
        printk("[LAST PC] mcu_base 0x%x,cnt 0x%x\n",mcu_base,cnt);
        /* Get PC, FP, SP and save to buf */
        for (i = 0; i < cnt; i++) {
            cluster = i / 4;
            cpu_in_cluster = i % 4;
            pc_value = readl(IOMEM((mcu_base+0x0) + (cpu_in_cluster << 5) + (0x100 * cluster)));
            fp_value = readl(IOMEM((mcu_base+0x8) + (cpu_in_cluster << 5) + (0x100 * cluster)));
            sp_value = readl(IOMEM((mcu_base+0xc) + (cpu_in_cluster << 5) + (0x100 * cluster)));
            kallsyms_lookup((unsigned long)pc_value, &size, &offset, NULL, str);
            ptr += sprintf(ptr, "[LAST PC] CORE_%d PC = 0x%lx(%s + 0x%lx), FP = 0x%lx, SP = 0x%lx\n", i, pc_value, str, offset, fp_value, sp_value);
            printk("[LAST PC] CORE_%d PC = 0x%lx(%s), FP = 0x%lx, SP = 0x%lx\n", i, pc_value, str, fp_value, sp_value);
        }
#endif
        return 0;
}

/*
 * mt_reg_dump_init: initialize driver.
 * Always return 0.
 */

static int __init mt_reg_dump_init(void)
{
   
    struct mt_lastpc_driver *lastpc_drv;
    lastpc_drv = get_mt_lastpc_drv();

#ifdef LASTPC_TEST_SUIT
    RGU_BASE = ioremap(0x10007000, 0x1000);
#endif

#ifndef CONFIG_OF
    err = platform_device_register(&lastpc_drv->device);
    if (err) {
        pr_err("Fail to register reg_dump_device");
        return err;
    }
    else
    {
        printk("lastpc init done\n");
    }
#endif
    lastpc_drv->lastpc_reg_dump = lastpc_platform_reg_dump;
    lastpc_drv->lastpc_probe    = lastpc_platform_probe;

    return 0;
}


arch_initcall(mt_reg_dump_init);
