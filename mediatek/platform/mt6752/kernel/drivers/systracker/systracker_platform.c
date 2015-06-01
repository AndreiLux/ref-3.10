#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/init.h>
#include "mach/mt_reg_base.h"
#include "mach/systracker.h"
#include <mach/sync_write.h>
#include <mach/systracker_drv.h>
#include <linux/interrupt.h>
#include <asm/signal.h>
#ifdef CONFIG_OF
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#include <linux/mtk_ram_console.h>
#include <linux/sched.h>
#include <mach/mt_clkmgr.h>
#include <ddp_reg.h>
#ifdef CONFIG_OF
void __iomem *BUS_DBG_BASE;
static int irq;
#endif
extern struct systracker_config_t track_config;
extern struct systracker_entry_t track_entry;

char systracker_buf[1024*1024];
struct systracker_config_t track_config;
struct systracker_entry_t track_entry;
unsigned int is_systracker_device_registered = 0;
unsigned int is_systracker_irq_registered = 0;
/* Some chip do not have reg dump, define a weak to avoid build error */
static int mt_reg_dump(char *buf) { return 1; }
static void save_entry(void);
extern void __iomem *p;
extern void __iomem *mm_area;
extern void __iomem *sleep;
extern void __iomem *idle;
extern asmlinkage void c_backtrace_ramconsole_print(unsigned long fp, int pmode);
irqreturn_t systracker_isr(void)
{
    unsigned int con;
    static char reg_buf[512];
    save_entry();
#ifdef SYSTRACKER_TEST_SUIT
    writel(readl(p) & ~(0x1 << 6), p);
#endif
    printk("Sys Tracker ISR\n");
    con = readl(IOMEM(BUS_DBG_CON));
    printk("[TRAKER] BUS_DBG_CON 0x%x\n",readl(IOMEM(BUS_DBG_CON)));
    writel(con | BUS_DBG_CON_IRQ_CLR, IOMEM(BUS_DBG_CON));
    dsb();
    if (con & BUS_DBG_CON_IRQ_WP_STA) {
        printk("[TRACKER] Watch address: 0x%x was touched\n", track_config.wp_phy_address);
        if (mt_reg_dump(reg_buf) == 0) {
            printk("%s\n", reg_buf);
        }
    }
    if(con & BUS_DBG_CON_IRQ_AR_STA)
    { 
       printk("[TRAKER] Read time out trigger\n");
       tracker_dump((char *)&systracker_buf);

    }
    if (con & BUS_DBG_CON_IRQ_AW_STA)
    {
       printk("[TRAKER] Write time out trigger\n");
       tracker_dump((char *)&systracker_buf);
    } 
    return IRQ_HANDLED;
}
static int watch_point_platform_enable(void)
{
    /* systracker interrupt registration */
    if (!is_systracker_irq_registered) {
#ifndef CONFIG_OF
        if (request_irq(BUS_DBG_TRACKER_IRQ_BIT_ID, systracker_isr, IRQF_TRIGGER_LOW, "SYSTRACKER", NULL)) {
#else
        if (request_irq(irq, (irq_handler_t)systracker_isr, IRQF_TRIGGER_NONE, "SYSTRACKER", NULL)) {
#endif
            printk(KERN_ERR "SYSTRACKER IRQ LINE NOT AVAILABLE!!\n");
        }
        else {
            is_systracker_irq_registered = 1;
        }
    }
    writel(track_config.wp_phy_address, IOMEM(BUS_DBG_WP));
    writel(0x0000000F, IOMEM(BUS_DBG_WP_MASK));
    track_config.enable_wp = 1;
    writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_WP_EN, IOMEM(BUS_DBG_CON));
    dsb();

    return 0;
}
static int watch_point_platform_disable(void)
{
    track_config.enable_wp = 0;
    writel(readl(IOMEM(BUS_DBG_CON)) & ~BUS_DBG_CON_WP_EN, IOMEM(BUS_DBG_CON));
    dsb();

    return 0;
}

static int watch_point_platform_set_address(unsigned int wp_phy_address)
{
   track_config.wp_phy_address = wp_phy_address;

   return 0;
}

static void systracker_platform_reset(void)
{
    writel(BUS_DBG_CON_DEFAULT_VAL, IOMEM(BUS_DBG_CON));
    writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_SW_RST, IOMEM(BUS_DBG_CON));
    writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_IRQ_CLR, IOMEM(BUS_DBG_CON));
    dsb();
}

static void systracker_platform_enable(void)
{
    unsigned int con;
    unsigned int timer_control_value;

    /* prescale = (133 * (10 ^ 6)) / 16 = 8312500/s */
    timer_control_value = (BUS_DBG_BUS_MHZ * 1000 / 16) * track_config.timeout_ms;
    // check time out value?
    printk("check for BUS_DBG_TIMER_CON value: 0x%x\n", readl(IOMEM(BUS_DBG_TIMER_CON)));
    writel(timer_control_value, IOMEM(BUS_DBG_TIMER_CON));

    track_config.state = 1;
    con = BUS_DBG_CON_BUS_DBG_EN | BUS_DBG_CON_BUS_OT_EN;
    if (track_config.enable_timeout) {
        con |= BUS_DBG_CON_TIMEOUT_EN;
    }
    if (track_config.enable_slave_err) {
        con |= BUS_DBG_CON_SLV_ERR_EN;
    }
    if (track_config.enable_irq) {
        con |= BUS_DBG_CON_IRQ_EN;
    }
    con |= BUS_DBG_CON_HALT_ON_EN;
    writel(con, IOMEM(BUS_DBG_CON));

    dsb();

    return;
}
static void systracker_platform_disable(void)
{
    track_config.state = 0;
    writel(readl(IOMEM(BUS_DBG_CON)) & ~BUS_DBG_CON_BUS_DBG_EN, IOMEM(BUS_DBG_CON));
    dsb();

    return;
}

/*
 * save entry info early
 */
static void save_entry(void)
{
    int i;
    track_entry.dbg_con =  readl(IOMEM(BUS_DBG_CON));

    for(i = 0; i < BUS_DBG_NUM_TRACKER; i++){
        track_entry.ar_track_l[i]   = readl(IOMEM(BUS_DBG_AR_TRACK_L(i)));
        track_entry.ar_track_h[i]   = readl(IOMEM(BUS_DBG_AR_TRACK_H(i)));
        track_entry.ar_trans_tid[i] = readl(IOMEM(BUS_DBG_AR_TRANS_TID(i)));
        track_entry.aw_track_l[i]   = readl(IOMEM(BUS_DBG_AW_TRACK_L(i)));
        track_entry.aw_track_h[i]   = readl(IOMEM(BUS_DBG_AW_TRACK_H(i)));
        track_entry.aw_trans_tid[i] = readl(IOMEM(BUS_DBG_AW_TRANS_TID(i)));
    }
}
int systracker_platform_probe(struct platform_device *pdev)
{


    printk("systracker probe\n");

    /* iomap register */
    BUS_DBG_BASE = of_iomap(pdev->dev.of_node, 0);
    if(!BUS_DBG_BASE) {
        printk("can't of_iomap for systracker!!\n");
        return -ENOMEM;
    }
    else {
        printk("of_iomap for systracker @ 0x%p\n", BUS_DBG_BASE);
    }

    /* get irq #  */
    irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
    printk("get irq # %d\n", irq);

#ifdef SYSTRACKER_TEST_SUIT
    if (!is_systracker_irq_registered) {
#ifndef CONFIG_OF
        if (request_irq(BUS_DBG_TRACKER_IRQ_BIT_ID, systracker_isr, IRQF_TRIGGER_LOW, "SYSTRACKER", NULL)) {
#else
        if (request_irq(irq, (irq_handler_t)systracker_isr, IRQF_TRIGGER_NONE, "SYSTRACKER", NULL)) {
#endif
            printk(KERN_ERR "SYSTRACKER IRQ LINE NOT AVAILABLE!!\n");
        }
        else
        {
            printk("[SYSTRACKER] IRQ request ok!\n");
          is_systracker_irq_registered=1;
        }
    }
#endif
    is_systracker_device_registered = 1;

    /* save entry info */
    save_entry();

    memset(&track_config, sizeof(struct systracker_config_t), 0);
    /* To latch last PC when tracker timeout, we need to enable interrupt mode */
    track_config.enable_timeout = 1;
    track_config.enable_slave_err = 1;
    track_config.enable_irq = 1;
    track_config.timeout_ms = 500;

    systracker_platform_reset();
    //systracker_platform_enable();

    return 0;
}

int systracker_platform_remove(struct platform_device *pdev)
{
    return 0;
}

int systracker_platform_suspend(struct platform_device *pdev, pm_message_t state)
{
    return 0;
}

int systracker_platform_resume(struct platform_device *pdev)
{
    if (track_config.state) {
        systracker_platform_enable();
    }

    if (track_config.enable_wp) {
        systracker_platform_enable();
    }

    return 0;
}
static int test_systracker(void)
{
#if 0
    *(volatile unsigned int *)0xF0007004 = 0x2008;
    *(volatile unsigned int *)0xF0007000 = 0x22000067;
    *(volatile unsigned int *)0xF0007008 = 0x1971;
    dsb();
    *(volatile unsigned int*)0xF0001220 |= 0x40;
    *(volatile unsigned int*)0xF3050000;
#endif
#if 0
    *(volatile unsigned int*)0xF011A070 |= 0x0;
    dsb();
    *(volatile unsigned int*)0x10113000;
    while (1);
#endif
    
    return 1;
}

void dump_backtrace_entry_ramconsole_print(unsigned long where, unsigned long from, unsigned long frame)
{
    char str_buf[256];

#ifdef CONFIG_KALLSYMS
    snprintf(str_buf, sizeof(str_buf), "[<%08lx>] (%pS) from [<%08lx>] (%pS)\n", where, (void *)where, from, (void *)from);
#else
    snprintf(str_buf, sizeof(str_buf), "Function entered at [<%08lx>] from [<%08lx>]\n", where, from);
#endif
    aee_sram_fiq_log(str_buf);
}

void dump_regs(const char *fmt, const char v1, const unsigned int reg, const unsigned int reg_val)
{
    char str_buf[256];
    snprintf(str_buf, sizeof(str_buf), fmt, v1, reg, reg_val);
    aee_sram_fiq_log(str_buf);
}

static int verify_stack(unsigned long sp)
{
    if (sp < PAGE_OFFSET ||(sp > (unsigned long)high_memory && high_memory != NULL))
        return -EFAULT;

    return 0;
}

static void dump_backtrace(struct pt_regs *regs, struct task_struct *tsk)
{
    char str_buf[256];
    unsigned int fp, mode;
    int ok = 1;

    snprintf(str_buf, sizeof(str_buf), "PC is 0x%lx, LR is 0x%lx\n", regs->ARM_pc, regs->ARM_lr);
    aee_sram_fiq_log(str_buf);

    if (!tsk)
        tsk = current;

    if (regs) {
        fp = regs->ARM_fp;
        mode = processor_mode(regs);
    } else if (tsk != current) {
        fp = thread_saved_fp(tsk);
        mode = 0x10;
    } else {
        asm("mov %0, fp" : "=r" (fp) : : "cc");
        mode = 0x10;
    }

    if (!fp) {
        aee_sram_fiq_log("no frame pointer");
        ok = 0;
    } else if (verify_stack(fp)) {
        aee_sram_fiq_log("invalid frame pointer");
        ok = 0;
    } else if (fp < (unsigned long)end_of_stack(tsk))
        aee_sram_fiq_log("frame pointer underflow");
    aee_sram_fiq_log("\n");

    if (ok)
        c_backtrace_ramconsole_print(fp, mode);

}

int systracker_handler(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
{
    int i;
    char str_buf[256], *ptr;
    ptr = (char *)str_buf;

    dump_backtrace(regs, NULL); 
#ifdef SYSTRACKER_TEST_SUIT
    writel(readl(p) & ~(0x1 << 6), p);
#endif
    //ptr += snprintf(ptr, sizeof(str_buf) - (ptr - str_buf), "mmsys clock=0x%x, CG_CON0=0x%x, CG_CON1=0x%x \n", 
    //        __raw_readl(IOMEM(DISP_REG_CLK_CFG_0_MM_CLK)),
    //        __raw_readl(IOMEM(DISP_REG_CONFIG_MMSYS_CG_CON0)),
    //        __raw_readl(IOMEM(DISP_REG_CONFIG_MMSYS_CG_CON1)));
    //ptr += snprintf(ptr, sizeof(str_buf) - (ptr - str_buf), "mmsys clock=0x%x\n", __raw_readl(IOMEM(DISP_REG_CLK_CFG_0_MM_CLK)));
    ptr += snprintf(ptr, sizeof(str_buf) - (ptr - str_buf), "clk_cfg_0=0x%x\n", __raw_readl(IOMEM(CLK_CFG_0)));
    ptr += snprintf(ptr, sizeof(str_buf) - (ptr - str_buf), "larb0_force_on=%d, smi_common_force_on=%d\n", 
            clk_is_force_on(MT_CG_DISP0_SMI_LARB0), clk_is_force_on(MT_CG_DISP0_SMI_COMMON));

    ptr += snprintf(ptr, sizeof(str_buf) - (ptr - str_buf), "==Sleep protect==\n");
    for(i = 0; i <= 0xc; i+=4) {
        ptr += snprintf(ptr, sizeof(str_buf) - (ptr - str_buf), "0x%x = 0x%x\n", 0x10001220 + i, __raw_readl(IOMEM(sleep + i)));
    }

    ptr += snprintf(ptr, sizeof(str_buf) - (ptr - str_buf), "==Bus idle==\n");
    for(i = 0; i <= 0x14; i+=4) {
        ptr += snprintf(ptr, sizeof(str_buf) - (ptr - str_buf), "0x%x = 0x%x\n", 0x10201180 + i, __raw_readl(IOMEM(idle + i)));
    }
    
    aee_sram_fiq_log(str_buf);

    //printk("CLK_CFG_0: 0x%x\n", __raw_readl(IOMEM(CLK_CFG_0)));
    //printk("==Sleep protect==\n");
    //for(i = 0; i <= 0xc; i+=4) {
    //    printk("0x%x = 0x%x\n", 0x10001220 + i, __raw_readl(IOMEM(sleep + i)));
    //}

    //printk("==Bus idle==\n");
    //for(i = 0; i <= 0x14; i+=4) {
    //    printk("0x%x = 0x%x\n", 0x10201180 + i, __raw_readl(IOMEM(idle + i)));
    //}

    //printk("let's go infinite loop :)\n");
    //while(1);
    //printk("mmsys clock=0x%x, CG_CON0=0x%x, CG_CON1=0x%x \n", 
    //        __raw_readl(DISP_REG_CLK_CFG_0_MM_CLK),
    //        __raw_readl(DISP_REG_CONFIG_MMSYS_CG_CON0),
    //        __raw_readl(DISP_REG_CONFIG_MMSYS_CG_CON1));
    //printk("larb0_force_on=%d, smi_common_force_on=%d\n", 
    //        clk_is_force_on(MT_CG_DISP0_SMI_LARB0), clk_is_force_on(MT_CG_DISP0_SMI_COMMON));

    #if 0
    if(readl(IOMEM(BUS_DBG_CON)) & BUS_DBG_CON_IRQ_AR_STA){
        for(i = 0; i < BUS_DBG_NUM_TRACKER; i++){
            printk(KERN_ALERT "AR_TRACKER Timeout Entry[%d]: ReadAddr:0x%x, Length:0x%x, TransactionID:0x%x!\n", i, readl(IOMEM(BUS_DBG_AR_TRACK_L(i))), readl(IOMEM(BUS_DBG_AR_TRACK_H(i))), readl(IOMEM(BUS_DBG_AR_TRANS_TID(i))));
        }
    } 
    if(readl(IOMEM(BUS_DBG_CON)) & BUS_DBG_CON_IRQ_AW_STA){
        for(i = 0; i < BUS_DBG_NUM_TRACKER; i++){
            printk(KERN_ALERT "AW_TRACKER Timeout Entry[%d]: WriteAddr:0x%x, Length:0x%x, TransactionID:0x%x!\n", i, readl(IOMEM(BUS_DBG_AW_TRACK_L(i))), readl(IOMEM(BUS_DBG_AW_TRACK_H(i))), readl(IOMEM(BUS_DBG_AW_TRANS_TID(i))));
        }
    }
    #endif

    return -1;
}

/*
 * mt_systracker_init: initialize driver.
 * Always return 0.
 */
static int __init mt_systracker_init(void)
{
    struct mt_systracker_driver *systracker_drv;
    systracker_drv = get_mt_systracker_drv();
#ifndef CONFIG_OF
    err = platform_device_register(&systracker_drv->device);
    if (err) {
        pr_err("Fail to register_systracker_device");
        return err;
    }else{
        printk("systracker init done\n");
    }
#endif
    systracker_drv->reset_systracker = systracker_platform_reset;
    systracker_drv->enable_watch_point = watch_point_platform_enable;
    systracker_drv->disable_watch_point = watch_point_platform_disable;
    systracker_drv->set_watch_point_address = watch_point_platform_set_address;
    systracker_drv->enable_systracker = systracker_platform_enable;
    systracker_drv->disable_systracker = systracker_platform_disable;
    systracker_drv->test_systracker = test_systracker; 
    systracker_drv->systracker_probe = systracker_platform_probe;
    systracker_drv->systracker_remove = systracker_platform_remove;
    systracker_drv->systracker_suspend = systracker_platform_suspend;
    systracker_drv->systracker_resume = systracker_platform_resume;

#ifdef CONFIG_ARM_LPAE
    hook_fault_code(0x10, systracker_handler, SIGTRAP, 0, "Systracker debug exception");
    hook_fault_code(0x11, systracker_handler, SIGTRAP, 0, "Systracker debug exception");
#else
    hook_fault_code(0x8, systracker_handler, SIGTRAP, 0, "Systracker debug exception");
    hook_fault_code(0x16, systracker_handler, SIGTRAP, 0, "Systracker debug exception"); 
#endif

    return 0;
}

arch_initcall(mt_systracker_init);
MODULE_AUTHOR("Chieh-Jay Liu");
MODULE_DESCRIPTION("system tracker driver v2.0");
