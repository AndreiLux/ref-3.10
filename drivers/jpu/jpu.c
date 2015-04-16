
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regulator/driver.h>

#include <linux/of_address.h>
#include <linux/module.h>
#include "jpu.h"


// definitions to be changed as customer  configuration
#define JPU_SUPPORT_CLOCK_CONTROL             /if you want to have clock gating scheme frame by frame
#define JPU_SUPPORT_ISR
#define CONFIG_PM_RS
//#define JPU_SUPPORT_PLATFORM_DRIVER_REGISTER  //if the platform driver knows the name of this driver. VPU_PLATFORM_DEVICE_NAME
//#define JPU_SUPPORT_RESERVED_VIDEO_MEMORY       //if this driver knows the dedicated video memory address

#define JPU_PLATFORM_DEVICE_NAME "hisilicon,k3jpeg"
#define JPU_DEV_NAME "jpu"
#define JPU_LDO_NAME "ldo_jpu"

//hisi l00206966 @add begin
#ifdef JPU_MMU
static u32 mmu_phy_base     = JPU_MMU_PAGE_BASE;
static u32 mmu_reg_phy_base = JPU_MMU_ADDR;
#endif
//hisi l00206966 @add end
#ifdef JPU_SUPPORT_ISR
//hisi l00206966 @modify begin
#define JPU_IRQ_NUM 172
//hisi l00206966 @modify end
#endif
typedef struct jpu_drv_context_t {
    struct fasync_struct *async_queue;
} jpu_drv_context_t;


/* To track the allocated memory buffer */
typedef struct jpudrv_buffer_pool_t {
    struct list_head list;
    struct jpudrv_buffer_t jb;
} jpudrv_buffer_pool_t;

static struct regulator_bulk_data jpu_regulator = {0};


#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
#define JPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE JPU_PHYSICAL_SIZE
#define JPU_DRAM_PHYSICAL_BASE JPU_MMU_PHYSICAL_BASE
#include "jmm.h"
static jpu_mm_t s_jmem;
static jpudrv_buffer_t s_video_memory = {0};
#endif

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

static int jpu_hw_reset(void);

static void jpu_clk_put(struct clk *clk);
// end customer definition

static jpudrv_buffer_t s_instance_pool = {0};
static jpu_drv_context_t s_jpu_drv_context = {0};
static int s_jpu_major = 0;
//hisi l00206966 @add begin
struct class *jpu_class  =NULL;
struct device *s_jpu_dev = NULL;
//hisi l00206966 @add end
static u32 s_jpu_open_count = 0;
static struct clk *s_jpu_clk = NULL;
#ifdef JPU_SUPPORT_ISR
static int s_jpu_irq          = 0;
#endif
static void __iomem *s_jpu_reg_virt_base = NULL;

static int s_interrupt_flag = 0;
static wait_queue_head_t s_interrupt_wait_q;

static spinlock_t s_jpu_lock       = __SPIN_LOCK_UNLOCKED(s_jpu_lock);
static struct list_head s_jbp_head = LIST_HEAD_INIT(s_jbp_head);

/* implement to power management functions */

#define ReadJpuRegister(addr)       __raw_readl((__force u32)(s_jpu_reg_virt_base + addr))
#define WriteJpuRegister(addr, val) __raw_writel(val, (__force u32)(s_jpu_reg_virt_base + addr))


static int jpu_alloc_dma_buffer(jpudrv_buffer_t *jb)
{
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    jb->phys_addr = (unsigned long long)jmem_alloc(&s_jmem, jb->size, 0);
    if (jb->phys_addr  == (unsigned long)-1)
    {
        printk(KERN_ERR "[JPUDRV] Physical memory allocation error size=%d\n", jb->size);
        return -1;
    }

    jb->base = (unsigned long)(s_video_memory.base + (jb->phys_addr - s_video_memory.phys_addr));
#else
    jb->base = (unsigned long)dma_alloc_coherent(NULL, PAGE_ALIGN(jb->size), (dma_addr_t *) (&jb->phys_addr), GFP_DMA | GFP_KERNEL);
    if ((void *)(jb->base) == NULL)
    {
        printk(KERN_ERR "[JPUDRV] Physical memory allocation error size=%d\n", jb->size);
        return -1;
    }
#endif
    return 0;
}

static void jpu_free_dma_buffer(jpudrv_buffer_t *jb)
{
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (jb->base)
        jmem_free(&s_jmem, jb->phys_addr, 0);
#else
    if (jb->base)
    {
        dma_free_coherent(0, PAGE_ALIGN(jb->size), (void *)jb->base, jb->phys_addr);
    }
#endif
}

static int jpu_free_buffers(void)
{
    jpudrv_buffer_pool_t *pool, *n;
    jpudrv_buffer_t jb;

    list_for_each_entry_safe(pool, n, &s_jbp_head, list)
    {
        jb = pool->jb;
        if (jb.base)
        {
            jpu_free_dma_buffer(&jb);
            list_del(&pool->list);
            kfree(pool);
        }
    }

    return 0;
}


#ifdef JPU_SUPPORT_ISR
static irqreturn_t jpu_irq_handler(int irq, void *dev_id)
{
    jpu_drv_context_t *dev = (jpu_drv_context_t *)dev_id;
    disable_irq_nosync(s_jpu_irq);

    if (dev->async_queue)
        kill_fasync(&dev->async_queue, SIGIO, POLL_IN); // notify the interrupt to userspace

    s_interrupt_flag = 1;
    wake_up_interruptible(&s_interrupt_wait_q);
    return IRQ_HANDLED;
}
#endif

static int jpu_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "[JPUDRV] jpu_open\n");
    spin_lock(&s_jpu_lock);
    s_jpu_open_count++;

    filp->private_data = (void *)(&s_jpu_drv_context);
    spin_unlock(&s_jpu_lock);
    return 0;
}

static long jpu_ioctl(struct file *filp, u_int cmd, u_long arg)
{
    int ret = 0;
    u32 timeout;

    switch (cmd)
    {
    case JDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY:
        {
            jpudrv_buffer_pool_t *jbp;

            jbp = kzalloc(sizeof(*jbp), GFP_KERNEL);
            if (!jbp)
                return -ENOMEM;

            ret = copy_from_user(&(jbp->jb), (jpudrv_buffer_t *)arg, sizeof(jpudrv_buffer_t));
            if (ret)
            {
                kfree(jbp);
                return -EFAULT;
            }

            ret = jpu_alloc_dma_buffer(&(jbp->jb));
            if (ret == -1)
            {
                ret = -ENOMEM;
                kfree(jbp);
                break;
            }
            ret = copy_to_user((void __user *)arg, &(jbp->jb), sizeof(jpudrv_buffer_t));
            if (ret)
            {
                kfree(jbp);
                ret = -EFAULT;
                break;
            }

            spin_lock(&s_jpu_lock);
            list_add(&jbp->list, &s_jbp_head);
            spin_unlock(&s_jpu_lock);

        }
        break;
    case JDI_IOCTL_FREE_PHYSICALMEMORY:
        {

            jpudrv_buffer_pool_t *jbp, *n;
            jpudrv_buffer_t jb;

            ret = copy_from_user(&jb, (jpudrv_buffer_t *)arg, sizeof(jpudrv_buffer_t));
            if (ret)
                return -EACCES;

            if (jb.base)
                jpu_free_dma_buffer(&jb);

            spin_lock(&s_jpu_lock);
            list_for_each_entry_safe(jbp, n, &s_jbp_head, list)
            {
                if (jbp->jb.base == jb.base)
                {
                    list_del(&jbp->list);
                    kfree(jbp);
                    break;
                }
            }
            spin_unlock(&s_jpu_lock);

        }
        break;
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    case JDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO:
        {
            spin_lock(&s_jpu_lock);
            if (s_video_memory.base != 0)
            {
                ret = copy_to_user((void __user *)arg, &s_video_memory, sizeof(jpudrv_buffer_t));
                if (ret != 0)
                    ret = -EFAULT;
            }
            else
            {
                ret = -EFAULT;
            }
            spin_unlock(&s_jpu_lock);
        }
        break;
#endif

    case JDI_IOCTL_WAIT_INTERRUPT:
        {
            timeout = (u32) arg;
            if (!wait_event_interruptible_timeout(s_interrupt_wait_q, s_interrupt_flag != 0, msecs_to_jiffies(timeout)))
            {
                ret = -ETIME;
                break;
            }

            if (signal_pending(current))
            {
                ret = -ERESTARTSYS;
                break;
            }

            s_interrupt_flag = 0;
            enable_irq(s_jpu_irq);
        }
        break;
    case JDI_IOCTL_SET_CLOCK_GATE:
        {
            u32 clkgate;
            if (get_user(clkgate, (u32 __user *) arg))
                return -EFAULT;

            printk(KERN_INFO "[JPUDRV] to operate jpu regulator  = %d\n", clkgate);

#ifdef JPU_SUPPORT_CLOCK_CONTROL
            if (clkgate)
            {
                if (regulator_bulk_enable(1, &jpu_regulator))
                {
                    printk(KERN_ERR "[JPUDRV] failed to enable jpu regulators\n");
                    return -EFAULT;
                }
            } else
            {
                regulator_bulk_disable(1, &jpu_regulator);
            }
#endif

        }
        break;
    case JDI_IOCTL_GET_INSTANCE_POOL:
        {

            spin_lock(&s_jpu_lock);

            if (s_instance_pool.base != 0)
            {
                ret = copy_to_user((void __user *)arg, &s_instance_pool, sizeof(jpudrv_buffer_t));
                if (ret != 0)
                    ret = -EFAULT;

                spin_unlock(&s_jpu_lock);
            }
            else
            {
                ret = copy_from_user(&s_instance_pool, (jpudrv_buffer_t *)arg, sizeof(jpudrv_buffer_t));
                spin_unlock(&s_jpu_lock);

                if (ret == 0)
                {
                    if (jpu_alloc_dma_buffer(&s_instance_pool) != -1)
                    {
                        memset((void *)s_instance_pool.base, 0x0, s_instance_pool.size);
                        ret = copy_to_user((void __user *)arg, &s_instance_pool, sizeof(jpudrv_buffer_t));
                        if (ret == 0)
                        {
                            // success to get memory for instance pool
                            //spin_unlock(&s_jpu_lock);
                            break;
                        }
                    }
                }
                ret = -EFAULT;
            }

        }
        break;
    case JDI_IOCTL_RESET:
        {
            jpu_hw_reset();
        }
        break;
    default:
        {
            printk(KERN_ERR "No such IOCTL, cmd is %d\n", cmd);
        }
        break;
    }
    return ret;
}

static ssize_t jpu_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    return -1;
}

static ssize_t jpu_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{

    return -1;
}

static int jpu_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "[JPUDRV] jpu_release\n");
    spin_lock(&s_jpu_lock);
    s_jpu_open_count--;

    if (s_jpu_open_count <= 0)
    {
        /* found and free the not handled buffer by user applications */
        jpu_free_buffers();
    }

    spin_unlock(&s_jpu_lock);
    return 0;
}


static int jpu_fasync(int fd, struct file *filp, int mode)
{
    struct jpu_drv_context_t *dev = (struct jpu_drv_context_t *)filp->private_data;
    return fasync_helper(fd, filp, mode, &dev->async_queue);
}

//hisi l00206966 @modify begin
static int jpu_map_to_physical_memory(struct file *fp, struct vm_area_struct *vm)
{
    vm->vm_flags |= VM_IO | VM_RESERVED;
    vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);

    return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

/*!
 * @brief memory map interface for jpu file operation
 * @return  0 on success or negative error code on error
 */
static int jpu_mmap(struct file *fp, struct vm_area_struct *vm)
{
    printk(KERN_INFO "[JPUDRV]  jpu_map_to_physical_memory\n");
    return jpu_map_to_physical_memory(fp, vm);
}
//hisi l00206966 @modify end
struct file_operations jpu_fops = {
    .owner          = THIS_MODULE,
    .open           = jpu_open,
    .read           = jpu_read,
    .write          = jpu_write,
    .unlocked_ioctl = jpu_ioctl,
    .release        = jpu_release,
    .fasync         = jpu_fasync,
    .mmap           = jpu_mmap,
};

static int jpu_setup_cdev(void)
{
    unsigned int minor = 0;
    struct device *dev = NULL;

    printk(KERN_INFO "[JPUDRV] :in jpu_setup_cdev\n");
    dev = device_create(jpu_class, 0, MKDEV(s_jpu_major, minor), NULL, JPU_DEV_NAME);
    if (NULL == dev)
    {
        printk(KERN_ERR "[JPUDRV] : Can't create device\n");
        return -ENOMEM;
    }

    s_jpu_dev = dev;
    return 0;
}

static int jpu_probe(struct platform_device *pdev)
{
    int err      = 0;
    struct device *dev       = &pdev->dev;
    struct device_node *node = dev->of_node;

    printk(KERN_ERR "[JPUDRV] jpu_probe\n");

    if (!node) {
        printk(KERN_INFO "[JPUDRV] NOT FOUND device node %s!\n", JPU_PLATFORM_DEVICE_NAME);
        goto ERROR_PROVE_DEVICE;
    }
    printk(KERN_INFO "[JPUDRV] jpu_probe node name = %s \n",node->name);

    jpu_regulator.supply = JPU_LDO_NAME;
    err = regulator_bulk_get(dev, 1, &jpu_regulator);
    if (err){
        printk(KERN_ERR "[JPUDRV] couldn't get regulators jpu %d\n", err);
        goto ERROR_PROVE_DEVICE;
    }

    s_jpu_clk = of_clk_get(node,0);
    if (IS_ERR(s_jpu_clk)) {
        printk(KERN_ERR "[JPUDRV] : Err :fail to get clock controller! \n");
        goto ERROR_PROVE_DEVICE;
    }

    s_jpu_reg_virt_base = of_iomap(node, 0);
    if (!s_jpu_reg_virt_base)
    {
        printk(KERN_ERR "[JPUDRV] : jpu base address get failed!\n");
        goto ERROR_PROVE_DEVICE;
    }
    printk(KERN_INFO "[JPUDRV] : jpu base address get from defined value base virtualbase=0x%x\n", (int)s_jpu_reg_virt_base);

    /* get the major number of the character device */
    s_jpu_major = register_chrdev(s_jpu_major, JPU_DEV_NAME, &jpu_fops);
    if (s_jpu_major < 0)
    {
        printk(KERN_ERR "[JPUDRV] : fail to register driver\n");
        err = -EBUSY;
        goto ERROR_PROVE_DEVICE;
    }

    jpu_class=class_create(THIS_MODULE,JPU_DEV_NAME);
    if (NULL == jpu_class)
    {
        printk(KERN_ERR "[JPUDRV] : fail to create  driver\n");
        err =  -ENOMEM;
        goto ERROR_PROVE_DEVICE;
    }

    jpu_setup_cdev();
    if (NULL == s_jpu_dev)
    {
        printk(KERN_ERR "[JPUDRV] : Can't register char driver\n");
        goto ERROR_PROVE_DEVICE;
    }


#ifdef JPU_SUPPORT_ISR
    s_jpu_irq = irq_of_parse_and_map(node, 0);
    if (!s_jpu_irq)
    {
        printk(KERN_ERR "[JPUDRV] :  get jpu interrupt handler s_jpu_irq failed \n");
        goto ERROR_PROVE_DEVICE;
    }

    printk(KERN_INFO "[JPUDRV] :  interrupt handler s_jpu_irq = %d \n",s_jpu_irq);

    err = request_irq(s_jpu_irq, jpu_irq_handler, 0, "JPU_CODEC_IRQ", (void *)(&s_jpu_drv_context));
    if (err)
    {
        printk(KERN_ERR "[JPUDRV] :  fail to register interrupt handler\n");
        goto ERROR_PROVE_DEVICE;
    }
#endif

#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    s_video_memory.size      = JPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE;
    s_video_memory.phys_addr = JPU_DRAM_PHYSICAL_BASE;
    s_video_memory.base      = (unsigned long)ioremap(s_video_memory.phys_addr, PAGE_ALIGN(s_video_memory.size));

    if (!s_video_memory.base)
    {
        printk(KERN_ERR "[JPUDRV] :  fail to remap video memory physical phys_addr=0x%x, base=0x%x, size=%d\n", (int)s_video_memory.phys_addr, (int)s_video_memory.base, (int)s_video_memory.size);
        goto ERROR_PROVE_DEVICE;
    }

    if (jmem_init(&s_jmem, s_video_memory.phys_addr, s_video_memory.size) < 0)
    {
        printk(KERN_ERR "[JPUDRV] :  fail to init vmem system\n");
        goto ERROR_PROVE_DEVICE;
    }
    printk(KERN_INFO "[JPUDRV] success to probe jpu device with reserved video memory phys_addr=0x%x, base = 0x%x\n",(int) s_video_memory.phys_addr, (int)s_video_memory.base);
#else
    printk(KERN_INFO "[JPUDRV] success to probe jpu device with non reserved video memory\n");
#endif

    return 0;
ERROR_PROVE_DEVICE:

    if (s_jpu_major){
        unregister_chrdev(s_jpu_major, JPU_DEV_NAME);
        s_jpu_major = 0;
    }

    if (s_jpu_reg_virt_base){
        iounmap(s_jpu_reg_virt_base);
        s_jpu_reg_virt_base = NULL;
    }

    if (s_jpu_irq){
        free_irq(s_jpu_irq, (void *)0);
        s_jpu_irq = 0;
    }

    if (!IS_ERR(s_jpu_clk)) {
        jpu_clk_put(s_jpu_clk);
        s_jpu_clk = NULL;
    }

    regulator_put(jpu_regulator.consumer);
    memset(&jpu_regulator, 0, sizeof(jpu_regulator));

    return err;
}

static int jpu_remove(struct platform_device *pdev)
{
#ifdef JPU_SUPPORT_PLATFORM_DRIVER_REGISTER

    printk(KERN_INFO "jpu_remove\n");

    if (s_instance_pool.base)
    {
        jpu_free_dma_buffer(&s_instance_pool);
        s_instance_pool.base = 0;
    }
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (s_video_memory.base)
    {
        iounmap((void *)s_video_memory.base);
        s_video_memory.base = 0;
        jmem_exit(&s_jmem);
    }
#endif


//hisi l00206966 @modify begin
    if (s_jpu_major > 0)
    {
        unregister_chrdev(s_jpu_major, JPU_DEV_NAME);
        s_jpu_major = 0;
    }
//hisi l00206966 @modify end

#ifdef JPU_SUPPORT_ISR
    if (s_jpu_irq){
        free_irq(s_jpu_irq, &s_jpu_drv_context);
        s_jpu_irq = 0;
    }
#endif
    if (s_jpu_reg_virt_base){
        iounmap(s_jpu_reg_virt_base);
        s_jpu_reg_virt_base = NULL;
    }

    if (!IS_ERR(s_jpu_clk)) {
        jpu_clk_put(s_jpu_clk);
        s_jpu_clk = NULL;
    }

    regulator_put(jpu_regulator.consumer);
    memset(&jpu_regulator, 0, sizeof(jpu_regulator));
#endif //JPU_SUPPORT_PLATFORM_DRIVER_REGISTER
    return 0;
}

#ifdef CONFIG_PM_RS
static int jpu_suspend(struct platform_device *pdev, pm_message_t state)
{
     printk("%s+.\n",__func__);
    //jpu_clk_disable(s_jpu_clk);
     printk("%s-.\n",__func__);
    return 0;

}

static int jpu_resume(struct platform_device *pdev)
{
     printk("%s+.\n",__func__);
    //jpu_clk_enable(s_jpu_clk);
     printk("%s-.\n",__func__);
    return 0;
}
#else
#define jpu_suspend NULL
#define jpu_resume  NULL
#endif              /* !CONFIG_PM */

static const struct of_device_id jpu_project_match_table[] = {
    {
        .compatible = JPU_PLATFORM_DEVICE_NAME,
        .data = NULL,
    },
    {
    },
};

static struct platform_driver jpu_driver = {
    .driver  = {
    .name    = JPU_PLATFORM_DEVICE_NAME,
    .owner   = THIS_MODULE,
    .of_match_table = of_match_ptr(jpu_project_match_table),
           },
    .probe   = jpu_probe,
    .remove  = jpu_remove,
    .suspend = jpu_suspend,
    .resume  = jpu_resume,
};

static int __init jpu_init(void)
{
    int res = 0;
    printk(KERN_INFO "jpu_init\n");

    init_waitqueue_head(&s_interrupt_wait_q);
    s_instance_pool.base = 0;
#ifdef JPU_SUPPORT_PLATFORM_DRIVER_REGISTER
    res = platform_driver_register(&jpu_driver);
#else
    res = platform_driver_register(&jpu_driver);
#endif

    printk(KERN_INFO "end jpu_init result=0x%x\n", res);
    return res;
}

static void __exit jpu_exit(void)
{
#ifdef JPU_SUPPORT_PLATFORM_DRIVER_REGISTER
    printk(KERN_INFO "jpu_exit\n");
#else
//hisi l00206966 @modify begin
    printk(KERN_INFO "jpu_exit s_jpu_open_count=%d\n", s_jpu_open_count);
    if (s_jpu_major > 0)
    {
        unregister_chrdev(s_jpu_major, JPU_DEV_NAME);
        s_jpu_major = 0;
    }

//hisi l00206966 @modify begin
    if (s_instance_pool.base)
    {
        printk(KERN_INFO "jpu_exit jpu_free_dma_buffer\n");
        jpu_free_dma_buffer(&s_instance_pool);
        s_instance_pool.base = 0;
    }
#ifdef JPU_SUPPORT_RESERVED_VIDEO_MEMORY
    if (s_video_memory.base)
    {
        iounmap((void *)s_video_memory.base);
        s_video_memory.base = 0;
    }
#endif

//hisi l00206966 @modify begin
    if(s_jpu_dev)
    {
        device_destroy(jpu_class, MKDEV(s_jpu_major, 0));
        class_destroy(jpu_class);
    }
//hisi l00206966 @modify end
#ifdef JPU_SUPPORT_ISR
    if (s_jpu_irq){
        free_irq(s_jpu_irq, &s_jpu_drv_context);
        s_jpu_irq = 0;
    }
#endif
    if (s_jpu_reg_virt_base){
        iounmap(s_jpu_reg_virt_base);
        s_jpu_reg_virt_base = NULL;
    }

    if (!IS_ERR(s_jpu_clk)) {
        jpu_clk_put(s_jpu_clk);
        s_jpu_clk = NULL;
    }

    regulator_put(jpu_regulator.consumer);
    memset(&jpu_regulator, 0, sizeof(jpu_regulator));
#endif //JPU_SUPPORT_PLATFORM_DRIVER_REGISTER

    platform_driver_unregister(&jpu_driver);
    return;
}

MODULE_AUTHOR("A customer using C&M JPU, Inc.");
MODULE_DESCRIPTION("JPU linux driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, sample_project_match_table);

module_init(jpu_init);
module_exit(jpu_exit);

int jpu_hw_reset(void)
{
    printk(KERN_INFO "request jpu reset from application. \n");
    return 0;
}

void jpu_clk_put(struct clk *clk)
{
    if (clk)
        clk_put(clk);
}
